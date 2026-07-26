/**
 * @file lldrawpoolsimple.cpp
 * @brief LLDrawPoolSimple class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "lldrawpoolsimple.h"

#include "llviewercamera.h"
#include "lldrawable.h"
#include "llface.h"
#include "llsky.h"
#include "pipeline.h"
#include "llspatialpartition.h"
#include "llviewershadermgr.h"
#include "llrender.h"

void LLDrawPoolGlow::renderPostDeferred(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    LLGLSLShader* shader = &gDeferredEmissiveProgram;

    LLGLEnable blend(GL_BLEND);
    gGL.flush();
    /// Get rid of z-fighting with non-glow pass.
    LLGLEnable polyOffset(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -1.0f);
    gGL.setSceneBlendType(LLRender::BT_ADD);

    LLGLDepthTest depth(GL_TRUE, GL_FALSE);
    gGL.setColorMask(false, true);

    // Multi-material (indexed) legacy glow batches carry a per-slot diffuse list and
    // must be drawn with the indexed program; the scalar sweep skips them. Require
    // both the static and rigged indexed glow programs to be complete; otherwise fall
    // back to scalar for everything (slot-0 diffuse alpha, the pre-batching behavior).
    bool glow_indexed = LLGLSLShader::sIndexedLegacyMaterials &&
                        gDeferredEmissiveIndexedProgram.isComplete() &&
                        gDeferredEmissiveIndexedProgram.mRiggedVariant &&
                        gDeferredEmissiveIndexedProgram.mRiggedVariant->isComplete();

    //first pass -- static objects
    shader->bind();
    if (glow_indexed)
    {
        pushEmissiveBatchesScalar(LLRenderPass::PASS_GLOW, false);
    }
    else
    {
        pushBatches(LLRenderPass::PASS_GLOW, true, true);
    }

    // second pass -- rigged objects
    shader = shader->mRiggedVariant;
    shader->bind();
    if (glow_indexed)
    {
        pushEmissiveBatchesScalar(LLRenderPass::PASS_GLOW_RIGGED, true);
    }
    else
    {
        pushRiggedBatches(LLRenderPass::PASS_GLOW_RIGGED, true, true);
    }

    // indexed (multi-material) passes
    if (glow_indexed)
    {
        gDeferredEmissiveIndexedProgram.bind();
        pushEmissiveBatchesIndexed(LLRenderPass::PASS_GLOW, false);

        gDeferredEmissiveIndexedProgram.bind(true); // rigged variant
        pushEmissiveBatchesIndexed(LLRenderPass::PASS_GLOW_RIGGED, true);
    }

    gGL.setColorMask(true, false);
    gGL.setSceneBlendType(LLRender::BT_ALPHA);
}

LLDrawPoolSimple::LLDrawPoolSimple() :
    LLRenderPass(POOL_SIMPLE)
{
}

LLDrawPoolAlphaMask::LLDrawPoolAlphaMask() :
    LLRenderPass(POOL_ALPHA_MASK)
{
}

LLDrawPoolFullbrightAlphaMask::LLDrawPoolFullbrightAlphaMask() :
    LLRenderPass(POOL_FULLBRIGHT_ALPHA_MASK)
{
}

//===============================
//DEFERRED IMPLEMENTATION
//===============================

S32 LLDrawPoolSimple::getNumDeferredPasses()
{
    return 1;
}

void LLDrawPoolSimple::renderDeferred(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL; //LL_RECORD_BLOCK_TIME(FTM_RENDER_SIMPLE_DEFERRED);
    LLGLDisable blend(GL_BLEND);

    //render static
    gDeferredDiffuseProgram.bind();
    pushBatches(LLRenderPass::PASS_SIMPLE, true, true);

    //render rigged
    gDeferredDiffuseProgram.bind(true);
    pushRiggedBatches(LLRenderPass::PASS_SIMPLE_RIGGED, true, true);
}

void LLDrawPoolAlphaMask::renderDeferred(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL; //LL_RECORD_BLOCK_TIME(FTM_RENDER_ALPHA_MASK_DEFERRED);
    LLGLSLShader* shader = &gDeferredDiffuseAlphaMaskProgram;

    //render static
    shader->bind();
    pushMaskBatches(LLRenderPass::PASS_ALPHA_MASK, true, true);

    //render rigged
    shader->bind(true);
    pushRiggedMaskBatches(LLRenderPass::PASS_ALPHA_MASK_RIGGED, true, true);
}

// grass drawpool
LLDrawPoolGrass::LLDrawPoolGrass() :
 LLRenderPass(POOL_GRASS)
{

}

void LLDrawPoolGrass::renderDeferred(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    {
        gDeferredNonIndexedDiffuseAlphaMaskProgram.bind();
        gDeferredNonIndexedDiffuseAlphaMaskProgram.setMinimumAlpha(0.5f);

        //render grass
        LLRenderPass::pushBatches(LLRenderPass::PASS_GRASS, getVertexDataMask());
    }
}


// Fullbright drawpool
LLDrawPoolFullbright::LLDrawPoolFullbright() :
    LLRenderPass(POOL_FULLBRIGHT)
{
}

void LLDrawPoolFullbright::renderPostDeferred(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL; //LL_RECORD_BLOCK_TIME(FTM_RENDER_FULLBRIGHT);

    LLGLSLShader* shader = nullptr;
    if (LLPipeline::sRenderingHUDs)
    {
        shader = &gHUDFullbrightProgram;
    }
    else
    {
        shader = &gDeferredFullbrightProgram;
    }

    // ⛔ DO NOT add LLGLEnable(GL_BLEND) here. It was tried as a fix for S2 and
    // caused a VISIBLE RENDERING REGRESSION: opaque fullbright surfaces whose
    // textures carry an alpha channel started blending, so geometry that was
    // never transparent became see-through. Reported in-world within minutes.
    //
    // THE INVARIANT: this pool arrives with GL_BLEND *disabled* (inherited --
    // it sets blend FACTORS only, never the enable), so a surviving fragment
    // OVERWRITES attachment 0 regardless of its texture alpha. fullbrightF.glsl
    // depends on that: it publishes sidecar K = 1 for opaque and alpha-mask
    // permutations precisely because the beauty write is an unblended
    // overwrite. Genuinely blended fullbright goes through the ALPHA pool and
    // keeps its fractional coverage.
    //
    // S2 is therefore RESOLVED in the shader, not here. Do not reach for
    // glEnablei / indexed blend enable either -- the guard does not track
    // per-attachment enable bits, and nothing needs it to.
    //
    // ⚠️ Making the invariant EXPLICIT was considered and REJECTED, and the
    // reason is worth keeping: GL_BLEND is NOT provably disabled on every entry
    // path. The MAIN VIEW is safe -- pipeline.cpp wraps renderGeomPostDeferred()
    // in LLGLDisable(GL_BLEND) -- but the HUD (render_hud_attachments), the
    // avatar preview/profile path and the impostor path have no such guard and
    // inherit ambient state. So adding LLGLDisable(GL_BLEND) here would NOT be a
    // no-op: it could change HUD/preview/impostor rendering, which is the exact
    // mirror of the regression above. Routing is confirmed safe -- genuinely
    // blended faces are classified into POOL_ALPHA, never here (llvovolume.cpp
    // batch generation; canRenderAsMask rejects blended/fractional-alpha/glow)
    // -- but that justifies the shader's K = 1, not touching the enable.
    //
    // If this is ever revisited: the missing piece is an explicit blend state at
    // the HUD/preview/impostor CALL SITES, not here.

    gGL.setSceneBlendType(LLRender::BT_ALPHA);

    // render static
    shader->bind();
    pushBatches(LLRenderPass::PASS_FULLBRIGHT, true, true);

    if (!LLPipeline::sRenderingHUDs)
    {
        // render rigged
        shader->bind(true);
        pushRiggedBatches(LLRenderPass::PASS_FULLBRIGHT_RIGGED, true, true);
    }
}

void LLDrawPoolFullbrightAlphaMask::renderPostDeferred(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL; //LL_RECORD_BLOCK_TIME(FTM_RENDER_FULLBRIGHT);

    LLGLSLShader* shader = nullptr;
    if (LLPipeline::sRenderingHUDs)
    {
        shader = &gHUDFullbrightAlphaMaskProgram;
    }
    else
    {
        shader = &gDeferredFullbrightAlphaMaskProgram;
    }

    LLGLDisable blend(GL_BLEND);

    // render static
    shader->bind();
    pushMaskBatches(LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK, true, true);

    if (!LLPipeline::sRenderingHUDs)
    {
        // render rigged
        shader->bind(true);
        pushRiggedMaskBatches(LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK_RIGGED, true, true);
    }
}

// ============================================================================
// [BDMerge A5.4-1a/1b] Velocity / motion-vector passes.
// Donor: Black Dragon lldrawpoolsimple.cpp:160-322. Phase 1a: rigid + camera.
// Phase 1b: each pool also binds the rigged variant (HAS_SKIN) and pushes the
// rigged pass with pushRiggedVelocityBatches* (current + previous palettes).
// bindVelocityUniforms must be re-uploaded after the rigged rebind -- the
// rigged variant is a distinct GL program with its own uniform storage.
// ============================================================================

void LLDrawPoolSimple::beginVelocityPass(S32 pass)
{
    gVelocityProgram.bind();
    bindVelocityUniforms(gVelocityProgram);
}

void LLDrawPoolSimple::endVelocityPass(S32 pass)
{
    gVelocityProgram.unbind();
}

void LLDrawPoolSimple::renderVelocity(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    LLGLEnable cull(GL_CULL_FACE);
    pushVelocityBatches(LLRenderPass::PASS_SIMPLE);

    // [BDMerge A5.4-1b] rigged
    gVelocityProgram.bind(true);
    bindVelocityUniforms(*gVelocityProgram.mRiggedVariant);
    pushRiggedVelocityBatches(LLRenderPass::PASS_SIMPLE_RIGGED);
}

void LLDrawPoolGrass::beginVelocityPass(S32 pass)
{
    gVelocityProgram.bind();
    bindVelocityUniforms(gVelocityProgram);
}

void LLDrawPoolGrass::endVelocityPass(S32 pass)
{
    gVelocityProgram.unbind();
}

void LLDrawPoolGrass::renderVelocity(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    LLGLEnable cull(GL_CULL_FACE);
    pushVelocityBatches(LLRenderPass::PASS_GRASS);
}

void LLDrawPoolFullbright::beginVelocityPass(S32 pass)
{
    gVelocityProgram.bind();
    bindVelocityUniforms(gVelocityProgram);
}

void LLDrawPoolFullbright::endVelocityPass(S32 pass)
{
    gVelocityProgram.unbind();
}

void LLDrawPoolFullbright::renderVelocity(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    LLGLEnable cull(GL_CULL_FACE);
    pushVelocityBatches(LLRenderPass::PASS_FULLBRIGHT);

    // [BDMerge A5.4-1b] rigged
    gVelocityProgram.bind(true);
    bindVelocityUniforms(*gVelocityProgram.mRiggedVariant);
    pushRiggedVelocityBatches(LLRenderPass::PASS_FULLBRIGHT_RIGGED);
}

void LLDrawPoolAlphaMask::beginVelocityPass(S32 pass)
{
    gVelocityAlphaProgram.bind();
    bindVelocityUniforms(gVelocityAlphaProgram);
}

void LLDrawPoolAlphaMask::endVelocityPass(S32 pass)
{
    gVelocityAlphaProgram.unbind();
}

void LLDrawPoolAlphaMask::renderVelocity(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    LLGLEnable cull(GL_CULL_FACE);
    pushVelocityBatchesTextured(LLRenderPass::PASS_ALPHA_MASK);

    // [BDMerge A5.4-1b] rigged (worn masked hair/clothing -- the machinima case)
    gVelocityAlphaProgram.bind(true);
    bindVelocityUniforms(*gVelocityAlphaProgram.mRiggedVariant);
    pushRiggedVelocityBatchesTextured(LLRenderPass::PASS_ALPHA_MASK_RIGGED);
}

void LLDrawPoolFullbrightAlphaMask::beginVelocityPass(S32 pass)
{
    gVelocityAlphaProgram.bind();
    bindVelocityUniforms(gVelocityAlphaProgram);
}

void LLDrawPoolFullbrightAlphaMask::endVelocityPass(S32 pass)
{
    gVelocityAlphaProgram.unbind();
}

void LLDrawPoolFullbrightAlphaMask::renderVelocity(S32 pass)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    LLGLEnable cull(GL_CULL_FACE);
    pushVelocityBatchesTextured(LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK);

    // [BDMerge A5.4-1b] rigged
    gVelocityAlphaProgram.bind(true);
    bindVelocityUniforms(*gVelocityAlphaProgram.mRiggedVariant);
    pushRiggedVelocityBatchesTextured(LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK_RIGGED);
}

