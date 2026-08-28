/**
 * @file lldrawpool.cpp
 * @brief LLDrawPool class implementation
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

#include "lldrawpool.h"
#include "llrender.h"
#include "llfasttimer.h"
#include "llviewercontrol.h"
#include "llagentdata.h"
#include "llframetimer.h"

#include "lldrawable.h"
#include "lldrawpoolalpha.h"
#include "lldrawpoolavatar.h"
#include "lldrawpoolbump.h"
#include "lldrawpoolmaterials.h"
#include "lldrawpoolpbropaque.h"
#include "lldrawpoolsimple.h"
#include "lldrawpoolsky.h"
#include "lldrawpooltree.h"
#include "lldrawpoolterrain.h"
#include "lldrawpoolwater.h"
#include "lldrawpoolwaterexclusion.h"
#include "llface.h"
#include "llviewerobjectlist.h" // For debug listing.
#include "pipeline.h"
#include "llspatialpartition.h"
#include "llviewercamera.h"
#include "llviewerwindow.h"
#include "lldrawpoolwlsky.h"
#include "llglslshader.h"
#include "llglcommonfunc.h"
#include "llvoavatar.h"
#include "llviewershadermgr.h"
#include "llfetchedgltfmaterial.h"
#include "llviewertexture.h"
#include "lldirectorcast.h"
#include "llactormover.h"

S32 LLDrawPool::sNumDrawPools = 0;
extern bool gSnapshot;
extern bool gCubeSnapshot;
static const LLClientOuterTransform* sLastOuterTransform = nullptr;
static U32 sLastOuterTransformRevision = 0;

void setup_texture_matrix(LLDrawInfo& params);
void teardown_texture_matrix(LLDrawInfo& params);

//=============================
// Draw Pool Implementation
//=============================
LLDrawPool *LLDrawPool::createPool(const U32 type, LLViewerTexture *tex0)
{
    LLDrawPool *poolp = NULL;
    switch (type)
    {
    case POOL_SIMPLE:
        poolp = new LLDrawPoolSimple();
        break;
    case POOL_GRASS:
        poolp = new LLDrawPoolGrass();
        break;
    case POOL_ALPHA_MASK:
        poolp = new LLDrawPoolAlphaMask();
        break;
    case POOL_FULLBRIGHT_ALPHA_MASK:
        poolp = new LLDrawPoolFullbrightAlphaMask();
        break;
    case POOL_FULLBRIGHT:
        poolp = new LLDrawPoolFullbright();
        break;
    case POOL_GLOW:
        poolp = new LLDrawPoolGlow();
        break;
    case POOL_ALPHA_PRE_WATER:
        poolp = new LLDrawPoolAlpha(LLDrawPool::POOL_ALPHA_PRE_WATER);
        break;
    case POOL_ALPHA_POST_WATER:
        poolp = new LLDrawPoolAlpha(LLDrawPool::POOL_ALPHA_POST_WATER);
        break;
    case POOL_AVATAR:
    case POOL_CONTROL_AV:
        poolp = new LLDrawPoolAvatar(type);
        break;
    case POOL_TREE:
        poolp = new LLDrawPoolTree(tex0);
        break;
    case POOL_TERRAIN:
        poolp = new LLDrawPoolTerrain(tex0);
        break;
    case POOL_SKY:
        poolp = new LLDrawPoolSky();
        break;
    case POOL_VOIDWATER:
    case POOL_WATER:
        poolp = new LLDrawPoolWater();
        break;
    case POOL_BUMP:
        poolp = new LLDrawPoolBump();
        break;
    case POOL_MATERIALS:
        poolp = new LLDrawPoolMaterials();
        break;
    case POOL_WL_SKY:
        poolp = new LLDrawPoolWLSky();
        break;
    case POOL_GLTF_PBR:
        poolp = new LLDrawPoolGLTFPBR();
        break;
    case POOL_GLTF_PBR_ALPHA_MASK:
        poolp = new LLDrawPoolGLTFPBR(LLDrawPool::POOL_GLTF_PBR_ALPHA_MASK);
        break;
    case POOL_WATEREXCLUSION:
        poolp = new LLDrawPoolWaterExclusion();
        break;
    default:
        LL_ERRS() << "Unknown draw pool type!" << LL_ENDL;
        return NULL;
    }

    llassert(poolp->mType == type);
    return poolp;
}

LLDrawPool::LLDrawPool(const U32 type)
{
    mType = type;
    sNumDrawPools++;
    mId = sNumDrawPools;
    mShaderLevel = 0;
    mSkipRender = false;
}

LLDrawPool::~LLDrawPool()
{

}

LLViewerTexture *LLDrawPool::getDebugTexture()
{
    return NULL;
}

//virtual
void LLDrawPool::beginRenderPass( S32 pass )
{
}

//virtual
S32  LLDrawPool::getNumPasses()
{
    return 1;
}

//virtual
void LLDrawPool::beginDeferredPass(S32 pass)
{

}

//virtual
void LLDrawPool::endDeferredPass(S32 pass)
{

}

//virtual
S32 LLDrawPool::getNumDeferredPasses()
{
    return 0;
}

//virtual
void LLDrawPool::renderDeferred(S32 pass)
{

}

//virtual
void LLDrawPool::beginPostDeferredPass(S32 pass)
{

}

//virtual
void LLDrawPool::endPostDeferredPass(S32 pass)
{

}

//virtual
S32 LLDrawPool::getNumPostDeferredPasses()
{
    return 0;
}

//virtual
void LLDrawPool::renderPostDeferred(S32 pass)
{

}

//virtual
void LLDrawPool::endRenderPass( S32 pass )
{
    //make sure channel 0 is active channel
    gGL.getTexUnit(0)->activate();
}

//virtual
void LLDrawPool::beginShadowPass(S32 pass)
{

}

//virtual
void LLDrawPool::endShadowPass(S32 pass)
{

}

//virtual
S32 LLDrawPool::getNumShadowPasses()
{
    return 0;
}

//virtual
void LLDrawPool::renderShadow(S32 pass)
{

}

// [BDMerge A5.4-1a] Velocity-pass base hooks. Default: this pool contributes no
// velocity (0 passes). Rigid draw pools override these; skinned support is 1b.
//virtual
void LLDrawPool::beginVelocityPass(S32 pass)
{

}

//virtual
void LLDrawPool::endVelocityPass(S32 pass)
{

}

//virtual
S32 LLDrawPool::getNumVelocityPasses()
{
    return 0;
}

//virtual
void LLDrawPool::renderVelocity(S32 pass)
{

}

//=============================
// Face Pool Implementation
//=============================
LLFacePool::LLFacePool(const U32 type)
: LLDrawPool(type)
{
    resetDrawOrders();
}

LLFacePool::~LLFacePool()
{
    destroy();
}

void LLFacePool::destroy()
{
    if (!mReferences.empty())
    {
        LL_INFOS() << mReferences.size() << " references left on deletion of draw pool!" << LL_ENDL;
    }
}

void LLFacePool::dirtyTextures(const std::set<LLViewerFetchedTexture*>& textures)
{
}

void LLFacePool::enqueue(LLFace* facep)
{
    mDrawFace.push_back(facep);
}

// virtual
bool LLFacePool::addFace(LLFace *facep)
{
    addFaceReference(facep);
    return true;
}

// virtual
bool LLFacePool::removeFace(LLFace *facep)
{
    removeFaceReference(facep);

    vector_replace_with_last(mDrawFace, facep);

    return true;
}

// Not absolutely sure if we should be resetting all of the chained pools as well - djs
void LLFacePool::resetDrawOrders()
{
    mDrawFace.resize(0);
}

LLViewerTexture *LLFacePool::getTexture()
{
    return NULL;
}

void LLFacePool::removeFaceReference(LLFace *facep)
{
    if (facep->getReferenceIndex() != -1)
    {
        if (facep->getReferenceIndex() != (S32)mReferences.size())
        {
            LLFace *back = mReferences.back();
            mReferences[facep->getReferenceIndex()] = back;
            back->setReferenceIndex(facep->getReferenceIndex());
        }
        mReferences.pop_back();
    }
    facep->setReferenceIndex(-1);
}

void LLFacePool::addFaceReference(LLFace *facep)
{
    if (-1 == facep->getReferenceIndex())
    {
        facep->setReferenceIndex(static_cast<S32>(mReferences.size()));
        mReferences.push_back(facep);
    }
}

void LLFacePool::pushFaceGeometry()
{
    for (LLFace* const& face : mDrawFace)
    {
        face->renderIndexed();
    }
}

bool LLFacePool::verify() const
{
    bool ok = true;

    for (std::vector<LLFace*>::const_iterator iter = mDrawFace.begin();
         iter != mDrawFace.end(); iter++)
    {
        const LLFace* facep = *iter;
        if (facep->getPool() != this)
        {
            LL_INFOS() << "Face in wrong pool!" << LL_ENDL;
            facep->printDebugInfo();
            ok = false;
        }
        else if (!facep->verify())
        {
            ok = false;
        }
    }

    return ok;
}

void LLFacePool::printDebugInfo() const
{
    LL_INFOS() << "Pool " << this << " Type: " << getType() << LL_ENDL;
}

bool LLFacePool::LLOverrideFaceColor::sOverrideFaceColor = false;

void LLFacePool::LLOverrideFaceColor::setColor(const LLColor4& color)
{
    gGL.diffuseColor4fv(color.mV);
}

void LLFacePool::LLOverrideFaceColor::setColor(const LLColor4U& color)
{
    gGL.diffuseColor4ubv(color.mV);
}

void LLFacePool::LLOverrideFaceColor::setColor(F32 r, F32 g, F32 b, F32 a)
{
    gGL.diffuseColor4f(r,g,b,a);
}


//=============================
// Render Pass Implementation
//=============================
static bool skip_rain_occlusion_attachment(const LLDrawInfo* params)
{
    return LLPipeline::sRainOcclusionRender &&
           params && params->mAttachedToAvatar;
}

namespace
{
const LLStaticHashedString sActorFxEnabled("actorFxEnabled");
const LLStaticHashedString sActorFxLook("actorFxLook");
const LLStaticHashedString sActorFxTime("actorFxTime");
const LLStaticHashedString sActorFxTint("actorFxTint");
const LLStaticHashedString sActorFxParams0("actorFxParams0");
const LLStaticHashedString sActorFxParams1("actorFxParams1");
const LLStaticHashedString sActorFxParams2("actorFxParams2");
const LLStaticHashedString sActorFxScreenSize("actorFxScreenSize");
const LLStaticHashedString sActorFxDissolveProgress("actorFxDissolveProgress");
const LLStaticHashedString sActorFxUseCoverageAlpha("actorFxUseCoverageAlpha");

LLGLSLShader* get_actor_fx_shader()
{
    LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
    return shader &&
           (shader->mFeatures.hasActorFx || shader->mFeatures.hasActorFxShadow)
        ? shader : nullptr;
}
}

// static
void LLRenderPass::uploadActorFxDisabled()
{
    if (LLGLSLShader* shader = get_actor_fx_shader())
    {
        // This assignment is deliberately made for every unowned native draw.
        // Actor-FX uniforms are program state and otherwise leak from the last
        // styled attachment into unrelated world geometry using that program.
        shader->uniform1i(sActorFxEnabled, 0);
    }
}

// static
bool LLRenderPass::actorFxLookNeedsSyntheticBloom(S32 look)
{
    // actorFxEmissive() also provides restrained self-light for many looks so
    // they remain legible after scene lighting; self-light does not imply a
    // costly second bloom draw. Queue zero-authored-emissive alpha faces only
    // for deliberate bloom-signature looks. Opaque and authored-emissive faces
    // publish their glow in their normal paths.
    switch (look)
    {
        case 2:  // Hologram
        case 6:  // Neon outline
        case 10: // Dissolve incandescent boundary
        case 14: // Blueprint
        case 15: // Ectoplasm
        case 17: // Prism
        case 19: // Wallhack / ESP
        case 26: // Sonar reveal
        case 27: // Hologram interference
        case LLDirectorCast::ACTOR_LOOK_BASS_SWEEP:
            return true;
        default:
            return false;
    }
}

// static
F32 LLRenderPass::actorFxFrameTime(F32 requested_fps)
{
    static bool sClockInitialized = false;
    static F64 sClockEpoch = 0.0;
    static U32 sClockFrame = U32_MAX;
    static F64 sFrameClock = 0.0;
    static F64 sSnapshotTime = 0.0;
    static bool sWasSnapshot = false;

    if (sClockFrame != gFrameCount)
    {
        sClockFrame = gFrameCount;
        sFrameClock = LLFrameTimer::getElapsedSeconds();
    }
    if (!sClockInitialized)
    {
        sClockEpoch = sFrameClock;
        sClockInitialized = true;
    }

    const F64 frame_time = llmax(sFrameClock - sClockEpoch, 0.0);
    if (gSnapshot && !sWasSnapshot)
    {
        sSnapshotTime = frame_time;
    }
    const F64 real_time = gSnapshot ? sSnapshotTime : frame_time;
    sWasSnapshot = gSnapshot;
    const F64 effect_fps = static_cast<F64>(
        llclamp(requested_fps, 0.f, 30.f));
    return static_cast<F32>(effect_fps > 0.0
        ? floor(real_time * effect_fps) / effect_fps : real_time);
}

// static
LLVector2 LLRenderPass::actorFxFragOffset()
{
    LLVector2 offset;
    const F32 zoom = LLViewerCamera::getInstance()->getZoomFactor();
    if (gSnapshot && gViewerWindow && zoom > 1.f)
    {
        const S32 tiles = llceil(zoom);
        const S32 sub = LLViewerCamera::getInstance()->getZoomSubRegion();
        const S32 tile_y = sub / tiles;
        const S32 tile_x = sub - tile_y * tiles;
        offset.mV[VX] = static_cast<F32>(
            tile_x * gViewerWindow->getWorldViewWidthRaw());
        offset.mV[VY] = static_cast<F32>(
            tile_y * gViewerWindow->getWorldViewHeightRaw());
    }
    return offset;
}

// static
LLVector2 LLRenderPass::actorFxScreenSize()
{
    LLVector2 size(1.f, 1.f);
    if (gViewerWindow)
    {
        const F32 zoom = LLViewerCamera::getInstance()->getZoomFactor();
        const F32 snapshot_tiles = gSnapshot && zoom > 1.f
            ? static_cast<F32>(llceil(zoom)) : 1.f;
        size.mV[VX] = llmax(
            static_cast<F32>(gViewerWindow->getWorldViewWidthRaw())
                * snapshot_tiles,
            1.f);
        size.mV[VY] = llmax(
            static_cast<F32>(gViewerWindow->getWorldViewHeightRaw())
                * snapshot_tiles,
            1.f);
    }
    return size;
}

bool upload_actor_fx_style(const LLUUID& style_id,
                           const LLDirectorCast::ActorStyle& style,
                           bool allow_native_wire)
{
    LLGLSLShader* shader = get_actor_fx_shader();
    if (!shader)
    {
        return false;
    }

    // A zero-strength Layer has no visible contribution. Treat it as disabled
    // so it takes the uniform fast path and agrees with the live-LOD/impostor
    // policy used by the avatar pools. Replace deliberately remains effective
    // regardless of its stored Layer alpha.
    const bool effective = style.mEnabled &&
        (style.mMode == LLDirectorCast::ACTOR_STYLE_REPLACE ||
         style.mAlpha > 0.001f);
    if (!effective)
    {
        shader->uniform1i(sActorFxEnabled, 0);
        return false;
    }

    // Wire always keeps a native hidden-line backing under its dedicated live
    // topology pass. Layer mixes authored material toward that backing by its
    // strength; Cover owns it completely. Disabling native Wire for Layer left
    // every legacy/PBR face fully authored, which read as material-shaped holes
    // and mismatched patches between otherwise continuous topology lines.

    // Null is Director's explicit identity for You. Use the runtime agent id
    // only for stable tint/phase generation; the style lookup above must retain
    // Director's null-is-self semantics.
    const LLUUID& stable_id = style_id.isNull() && gAgentID.notNull()
        ? gAgentID : style_id;

    LLColor4 tint;
    if (style.mUseActorHue)
    {
        tint = LLActorMover::actorPathColor(stable_id);
    }
    else
    {
        tint.setHSL(fmodf(llmax(style.mHue, 0.f), 360.f) / 360.f,
                    0.9f, 0.6f);
        tint.mV[VW] = 1.f;
    }

    const F32 effect_time = LLRenderPass::actorFxFrameTime(style.mEffectFps);
    const F32 strength = style.mMode == LLDirectorCast::ACTOR_STYLE_REPLACE
        ? 1.f : llclamp(style.mAlpha, 0.f, 1.f);
    const F32 stable_phase =
        static_cast<F32>(stable_id.mData[0] | (stable_id.mData[1] << 8))
        * (F_TWO_PI / 65536.f);

    shader->uniform1i(sActorFxLook, llclamp(
        style.mStyle,
        static_cast<S32>(LLDirectorCast::ACTOR_LOOK_DEFAULT),
        static_cast<S32>(LLDirectorCast::ACTOR_LOOK_MAX)));
    shader->uniform1f(sActorFxTime, effect_time);
    shader->uniform3f(sActorFxTint,
                      tint.mV[VX], tint.mV[VY], tint.mV[VZ]);
    shader->uniform4f(sActorFxParams0,
                      strength,
                      llmax(style.mPixelSize, 0.f),
                      llclamp(style.mShimmerAmount, 0.f, 1.f),
                      llclamp(style.mGlitch, 0.f, 1.f));
    shader->uniform4f(sActorFxParams1,
                      static_cast<F32>(llclamp(style.mDistortion, 0, 8)),
                      llclamp(style.mDistortionAmount, 0.f, 1.f),
                      llclamp(style.mBrightness, 0.05f, 1.5f),
                      stable_phase);
    // gl_FragCoord is local to each high-resolution snapshot tile. Supply the
    // tile's whole-image pixel origin so procedural grain/scanlines/pixelation
    // meet exactly at tile edges. Y is expressed in bottom-up GL coordinates.
    const LLVector2 frag_offset = LLRenderPass::actorFxFragOffset();
    const LLVector2 screen_size = LLRenderPass::actorFxScreenSize();
    F32 render_semantics =
        style.mMode == LLDirectorCast::ACTOR_STYLE_REPLACE ? 1.f : 0.f;
    if (style.mStyle == 3 && allow_native_wire)
    {
        render_semantics = 2.f; // exact topology line pass
    }
    shader->uniform4f(sActorFxParams2,
                      llclamp(style.mShimmerSpeed, 0.f, 20.f),
                      frag_offset.mV[VX], frag_offset.mV[VY],
                      render_semantics);
    shader->uniform2fv(sActorFxScreenSize, 1, screen_size.mV);
    shader->uniform1f(sActorFxDissolveProgress,
                      llclamp(style.mDissolveProgress, 0.f, 1.f));
    shader->uniform1i(sActorFxEnabled, 1);
    return LLRenderPass::actorFxLookNeedsSyntheticBloom(style.mStyle);
}

// Defined below the public upload entry points with the suppression policy.
// Anonymous-namespace declarations keep this shared-replay decision local to
// native draw dispatch.
namespace
{
bool shared_actor_fx_beauty_context();
bool shared_actor_fx_replay_active(const LLUUID& actor_id);
}

// static
bool LLRenderPass::uploadActorFx(const LLUUID& actor_id,
                                  bool allow_native_wire,
                                  bool force_native)
{
    if (!force_native && shared_actor_fx_replay_active(actor_id))
    {
        // Layer keeps the authored material as its base, but the treatment is
        // supplied exactly once by the shared forward-alpha replay. Cover is
        // normally skipped before reaching here; disabling is its safe fallback.
        uploadActorFxDisabled();
        return false;
    }

    // UUID callers and LLDrawInfo owners are canonicalized at their mutation /
    // geometry-build boundaries; keep this per-draw path free of object lookup.
    const LLDirectorCast::ActorStyle& style =
        LLDirectorCast::instance().getActorStyle(actor_id);
    return upload_actor_fx_style(actor_id, style, allow_native_wire);
}

// static
bool LLRenderPass::uploadActorFx(const LLDrawInfo& params, bool force_native)
{
    // Draw-info null is intentionally *not* Director's null-is-You identity.
    // It denotes world geometry with no stable actor owner.
    if (params.mActorFxOwner.isNull())
    {
        uploadActorFxDisabled();
        return false;
    }

    LLUUID owner;
    const LLDirectorCast::ActorStyle& style =
        LLDirectorCast::instance().resolveStoredActorStyle(
            params.mActorFxOwner, params.mActorFxFallbackOwner, owner);
    if (!force_native && shared_actor_fx_replay_active(owner))
    {
        uploadActorFxDisabled();
        return false;
    }
    return upload_actor_fx_style(owner, style, false);
}

namespace
{
bool shared_actor_fx_beauty_context()
{
    // Suppression belongs solely to the main world beauty render.  Shadow and
    // velocity retain native geometry/coverage, while selection, impostors,
    // reflection probes, mirrors, HUD previews and Prism auxiliary cameras must
    // continue to see the authoritative source actor.  Replaying the shared
    // proxy must never recursively suppress itself.
    LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
    return shader &&
           (shader->mFeatures.hasActorFx || shader->mFeatures.hasActorFxShadow) &&
           LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD &&
           !LLPipeline::sShadowRender &&
           !LLPipeline::sVelocityRender &&
           !LLPipeline::sReflectionRender &&
           !LLPipeline::sImpostorRender &&
           !LLPipeline::sRenderingHUDs &&
           !LLPipeline::sPrismLensRender &&
           !gCubeSnapshot &&
           !gPipeline.mHeroProbeManager.isMirrorPass() &&
           !LLActorMover::instance().isRenderingSharedActorStyle();
}

bool shared_actor_fx_zero_cover_context()
{
    // A zero-opacity Cover means "the actor is absent", so its native
    // geometry must not leak into image-producing auxiliary passes either.
    // Keep editor/UI-only paths (HUD previews and impostors) native so the
    // actor remains selectable and usable. Prism is a retained camera feed,
    // so an intentionally absent actor must be suppressed there too. Nonzero
    // Cover deliberately stays fail-open in every unsupported auxiliary view.
    if (LLPipeline::sRenderingHUDs ||
        LLPipeline::sImpostorRender ||
        LLActorMover::instance().isRenderingSharedActorStyle())
    {
        return false;
    }

    return shared_actor_fx_beauty_context() ||
           LLPipeline::sShadowRender ||
           LLPipeline::sVelocityRender ||
           LLPipeline::sReflectionRender ||
           LLPipeline::sPrismLensRender ||
           gCubeSnapshot ||
           gPipeline.mHeroProbeManager.isMirrorPass();
}

bool shared_actor_fx_zero_cover(
    const LLDirectorCast::ActorStyle& style)
{
    return style.mEnabled &&
           style.mMode == LLDirectorCast::ACTOR_STYLE_REPLACE &&
           style.mAlpha <= 0.f;
}

bool shared_actor_fx_replay_active(const LLUUID& actor_id)
{
    return shared_actor_fx_beauty_context() &&
           LLActorMover::instance().isSharedActorStyleActive(actor_id);
}

bool shared_actor_fx_style_suppresses(
    const LLUUID& actor_id,
    const LLDirectorCast::ActorStyle& style)
{
    // Ordinary Layer preserves the authored actor below the treatment. Dissolve
    // is coverage-changing, so Layer must own native beauty/glow for that one
    // look or native colour remains visible through its holes. In either mode,
    // suppress only after the actor-wide proxy preflight succeeds this frame.
    const bool owns_native_beauty =
        style.mMode == LLDirectorCast::ACTOR_STYLE_REPLACE
        || (style.mMode == LLDirectorCast::ACTOR_STYLE_LAYER
            && style.mStyle == 10); // GHOST_STYLE_DISSOLVE
    return style.mEnabled &&
           owns_native_beauty &&
           LLActorMover::instance().isSharedActorStyleReady(actor_id);
}
}

// static
bool LLRenderPass::shouldSuppressSharedActorFx(const LLUUID& actor_id)
{
    const LLDirectorCast::ActorStyle& style =
        LLDirectorCast::instance().getActorStyle(actor_id);
    if (shared_actor_fx_zero_cover_context() &&
        shared_actor_fx_zero_cover(style))
    {
        // No replacement draw is needed at this exact endpoint, so this path
        // is intentionally independent of shared-proxy shader readiness.
        return true;
    }
    if (!shared_actor_fx_beauty_context())
    {
        return false;
    }

    return shared_actor_fx_style_suppresses(actor_id, style);
}

// static
bool LLRenderPass::shouldSuppressSharedActorFx(const LLDrawInfo& params,
                                                bool depth_only)
{
    if (params.mActorFxOwner.isNull())
    {
        return false;
    }

    LLUUID owner;
    const LLDirectorCast::ActorStyle& style =
        LLDirectorCast::instance().resolveStoredActorStyle(
            params.mActorFxOwner, params.mActorFxFallbackOwner, owner);
    if (shared_actor_fx_zero_cover_context() &&
        shared_actor_fx_zero_cover(style))
    {
        return true;
    }
    if (!shared_actor_fx_beauty_context())
    {
        return false;
    }
    // A nonzero Cover keeps native depth for DoF/coverage fidelity. At the
    // fully invisible endpoint, however, retaining native alpha depth would
    // still occlude or blur the background even though neither native colour
    // nor the shared proxy emits a pixel.
    if (depth_only && style.mAlpha > 0.f)
    {
        return false;
    }
    return shared_actor_fx_style_suppresses(owner, style);
}

// static
bool LLRenderPass::shouldSuppressSharedActorFxPBR(const LLDrawInfo& params,
                                                   bool depth_only)
{
    if (params.mActorFxOwner.isNull())
    {
        return false;
    }

    LLUUID owner;
    const LLDirectorCast::ActorStyle& style =
        LLDirectorCast::instance().resolveStoredActorStyle(
            params.mActorFxOwner, params.mActorFxFallbackOwner, owner);
    if (shared_actor_fx_zero_cover_context() &&
        shared_actor_fx_zero_cover(style))
    {
        return true;
    }
    if (!shared_actor_fx_beauty_context())
    {
        return false;
    }
    if (!style.mEnabled ||
        !LLActorMover::instance().isSharedActorStylePBRReady(owner))
    {
        return false;
    }

    // Transparent DoF/depth-only coverage remains native for every visible
    // endpoint.  Opaque/masked coverage is supplied by the exact shared PBR
    // prime before alpha draining.  Cover zero is intentionally absent from
    // both paths and must never leave an invisible depth silhouette.
    if (depth_only && style.mAlpha > 0.f)
    {
        return false;
    }
    return true;
}

LLRenderPass::LLRenderPass(const U32 type)
: LLDrawPool(type)
{

}

LLRenderPass::~LLRenderPass()
{

}

void LLRenderPass::renderGroup(LLSpatialGroup* group, U32 type, bool texture)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    LLSpatialGroup::drawmap_elem_t& draw_info = group->mDrawMap[type];

    for (LLSpatialGroup::drawmap_elem_t::iterator k = draw_info.begin(); k != draw_info.end(); ++k)
    {
        LLDrawInfo *pparams = *k;
        if (pparams && !skip_rain_occlusion_attachment(pparams))
        {
            pushBatch(*pparams, texture);
        }
    }
}

void LLRenderPass::renderRiggedGroup(LLSpatialGroup* group, U32 type, bool texture)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    LLSpatialGroup::drawmap_elem_t& draw_info = group->mDrawMap[type];
    const LLVOAvatar* lastAvatar = nullptr;
    U64 lastMeshId = 0;
    bool skipLastSkin = false;

    for (LLSpatialGroup::drawmap_elem_t::iterator k = draw_info.begin(); k != draw_info.end(); ++k)
    {
        LLDrawInfo* pparams = *k;
        if (pparams)
        {
            if (uploadMatrixPalette(pparams->mAvatar, pparams->mSkinInfo, lastAvatar, lastMeshId, skipLastSkin))
            {
                pushBatch(*pparams, texture);
            }
        }
    }
}

void LLRenderPass::pushBatches(U32 type, bool texture, bool batch_textures)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    if (texture)
    {
        auto* begin = gPipeline.beginRenderMap(type);
        auto* end = gPipeline.endRenderMap(type);
        for (LLCullResult::drawinfo_iterator i = begin; i != end; )
        {
            LLDrawInfo* pparams = *i;
            LLCullResult::increment_iterator(i, end);

            if (skip_rain_occlusion_attachment(pparams))
            {
                continue;
            }
            pushBatch(*pparams, texture, batch_textures);
        }
    }
    else
    {
        pushUntexturedBatches(type);
    }
}

void LLRenderPass::pushUntexturedBatches(U32 type)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    auto* begin = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);
    for (LLCullResult::drawinfo_iterator i = begin; i != end; )
    {
        LLDrawInfo* pparams = *i;
        LLCullResult::increment_iterator(i, end);

        if (skip_rain_occlusion_attachment(pparams))
        {
            continue;
        }
        pushUntexturedBatch(*pparams);
    }
}

void LLRenderPass::pushRiggedBatches(U32 type, bool texture, bool batch_textures)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;

    if (texture)
    {
        const LLVOAvatar* lastAvatar = nullptr;
        U64 lastMeshId = 0;
        bool skipLastSkin = false;
        auto* begin = gPipeline.beginRenderMap(type);
        auto* end = gPipeline.endRenderMap(type);
        for (LLCullResult::drawinfo_iterator i = begin; i != end; )
        {
            LLDrawInfo* pparams = *i;
            LLCullResult::increment_iterator(i, end);

            if (uploadMatrixPalette(pparams->mAvatar, pparams->mSkinInfo, lastAvatar, lastMeshId, skipLastSkin))
            {
                pushBatch(*pparams, texture, batch_textures);
            }
        }
    }
    else
    {
        pushUntexturedRiggedBatches(type);
    }
}

void LLRenderPass::pushUntexturedRiggedBatches(U32 type)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    const LLVOAvatar* lastAvatar = nullptr;
    U64 lastMeshId = 0;
    bool skipLastSkin = false;
    auto* begin = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);
    for (LLCullResult::drawinfo_iterator i = begin; i != end; )
    {
        LLDrawInfo* pparams = *i;
        LLCullResult::increment_iterator(i, end);

        if (uploadMatrixPalette(pparams->mAvatar, pparams->mSkinInfo, lastAvatar, lastMeshId, skipLastSkin))
        {
            pushUntexturedBatch(*pparams);
        }
    }
}

void LLRenderPass::pushMaskBatches(U32 type, bool texture, bool batch_textures)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    auto* begin = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);
    for (LLCullResult::drawinfo_iterator i = begin; i != end; )
    {
        LLDrawInfo* pparams = *i;
        LLCullResult::increment_iterator(i, end);
        if (skip_rain_occlusion_attachment(pparams))
        {
            continue;
        }
        if (pparams->mMaterialSlotList.size() > 1)
        { // multi-material legacy batch -- drawn by pushMaskBatchesIndexed
            continue;
        }
        LLGLSLShader::sCurBoundShaderPtr->setMinimumAlpha(pparams->mAlphaMaskCutoff);
        pushBatch(*pparams, texture, batch_textures);
    }
}

void LLRenderPass::pushRiggedMaskBatches(U32 type, bool texture, bool batch_textures)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    const LLVOAvatar* lastAvatar = nullptr;
    U64 lastMeshId = 0;
    bool skipLastSkin = false;
    auto* begin = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);
    for (LLCullResult::drawinfo_iterator i = begin; i != end; )
    {
        LLDrawInfo* pparams = *i;

        LLCullResult::increment_iterator(i, end);

        llassert(pparams);

        if (pparams->mMaterialSlotList.size() > 1)
        { // multi-material legacy batch -- drawn by pushMaskBatchesIndexed
            continue;
        }

        LLGLSLShader::sCurBoundShaderPtr->setMinimumAlpha(pparams->mAlphaMaskCutoff);

        if (uploadMatrixPalette(pparams->mAvatar, pparams->mSkinInfo, lastAvatar, lastMeshId, skipLastSkin))
        {
            pushBatch(*pparams, texture, batch_textures);
        }
    }
}

void LLRenderPass::pushMaskBatchesIndexed(U32 type, bool rigged)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;

    const LLVOAvatar* lastAvatar = nullptr;
    U64 lastMeshId = 0;
    bool skipLastSkin = false;

    auto* begin = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);
    for (LLCullResult::drawinfo_iterator i = begin; i != end; )
    {
        LLDrawInfo& params = **i;
        LLCullResult::increment_iterator(i, end);

        if (skip_rain_occlusion_attachment(&params))
        {
            continue;
        }
        if (params.mMaterialSlotList.size() < 2)
        {
            continue;
        }
        if (shouldSuppressSharedActorFx(params))
        {
            continue;
        }

        if (rigged)
        {
            if (!uploadMatrixPalette(params.mAvatar, params.mSkinInfo, lastAvatar, lastMeshId, skipLastSkin))
            {
                continue;
            }
        }

        // Slot count is capped at N (<= 8) by genDrawInfo; clamp defensively so a
        // stale/over-long list can never overrun the 8-slot array or N sampler units.
        const S32 N = LLGLSLShader::sIndexedGLTFChannels;
        llassert((S32)params.mMaterialSlotList.size() <= N);
        const S32 n = llmin((S32)params.mMaterialSlotList.size(), N);
        LL_PROFILE_ZONE_NUM(n);

        F32 min_alpha[8] = { 0.f };
        for (S32 s = 0; s < n; ++s)
        {
            const LLDrawInfo::MaterialSlot& slot = params.mMaterialSlotList[s];
            LLViewerTexture* diffuse = slot.mDiffuse.notNull() ? slot.mDiffuse.get() : LLViewerFetchedTexture::sWhiteImagep.get();
            gGL.getTexUnit(s)->bindFast(diffuse);
            min_alpha[s] = slot.mAlphaMaskCutoff;
        }

        static const LLStaticHashedString sMinAlpha("mat_minimum_alpha");
        shader->uniform1fv(sMinAlpha, n, min_alpha);

        applyModelMatrix(params);

        uploadActorFx(params);
        params.mVertexBuffer->setBuffer();
        params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart, params.mEnd, params.mCount, params.mOffset);
    }
}

void LLRenderPass::pushEmissiveBatchesScalar(U32 type, bool rigged)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    const LLVOAvatar* lastAvatar = nullptr;
    U64 lastMeshId = 0;
    bool skipLastSkin = false;

    // PASS_GLOW is an opaque authored sidecar. Synthetic Actor FX on this
    // path covers the surface rather than an alpha card; explicitly reset the
    // program uniform because the same shader family is also used by the alpha
    // pool with coverage enabled.
    if (LLGLSLShader::sCurBoundShaderPtr)
    {
        LLGLSLShader::sCurBoundShaderPtr->uniform1i(
            sActorFxUseCoverageAlpha, 0);
    }

    auto* begin = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);
    for (LLCullResult::drawinfo_iterator i = begin; i != end; )
    {
        LLDrawInfo* pparams = *i;
        LLCullResult::increment_iterator(i, end);

        if (pparams->mMaterialSlotList.size() > 1)
        { // multi-material glow batch -- drawn by pushEmissiveBatchesIndexed
            continue;
        }

        if (rigged)
        {
            if (!uploadMatrixPalette(pparams->mAvatar, pparams->mSkinInfo, lastAvatar, lastMeshId, skipLastSkin))
            {
                continue;
            }
        }

        pushBatch(*pparams, true, true);
    }
}

void LLRenderPass::pushEmissiveBatchesIndexed(U32 type, bool rigged)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    const LLVOAvatar* lastAvatar = nullptr;
    U64 lastMeshId = 0;
    bool skipLastSkin = false;

    if (LLGLSLShader::sCurBoundShaderPtr)
    {
        LLGLSLShader::sCurBoundShaderPtr->uniform1i(
            sActorFxUseCoverageAlpha, 0);
    }

    auto* begin = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);
    for (LLCullResult::drawinfo_iterator i = begin; i != end; )
    {
        LLDrawInfo& params = **i;
        LLCullResult::increment_iterator(i, end);

        if (params.mMaterialSlotList.size() < 2)
        {
            continue;
        }
        if (shouldSuppressSharedActorFx(params))
        {
            continue;
        }

        if (rigged)
        {
            if (!uploadMatrixPalette(params.mAvatar, params.mSkinInfo, lastAvatar, lastMeshId, skipLastSkin))
            {
                continue;
            }
        }

        // Slot count is capped at N (<= 8) by genDrawInfo; clamp defensively so a
        // stale/over-long list can never bind past the N diffuse sampler units.
        const S32 N = LLGLSLShader::sIndexedGLTFChannels;
        llassert((S32)params.mMaterialSlotList.size() <= N);
        const S32 n = llmin((S32)params.mMaterialSlotList.size(), N);
        LL_PROFILE_ZONE_NUM(n);

        for (S32 s = 0; s < n; ++s)
        {
            const LLDrawInfo::MaterialSlot& slot = params.mMaterialSlotList[s];
            LLViewerTexture* diffuse = slot.mDiffuse.notNull() ? slot.mDiffuse.get() : LLViewerFetchedTexture::sWhiteImagep.get();
            gGL.getTexUnit(s)->bindFast(diffuse);
        }

        applyModelMatrix(params);

        uploadActorFx(params);
        params.mVertexBuffer->setBuffer();
        params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart, params.mEnd, params.mCount, params.mOffset);
    }
}

void LLRenderPass::applyModelMatrix(const LLDrawInfo& params)
{
    LLClientOuterTransform* outer = params.mOuterTransform.get();
    const U32 revision = outer ? outer->mRevision : 0;
    if (params.mModelMatrix != gGLLastMatrix ||
        outer != sLastOuterTransform ||
        revision != sLastOuterTransformRevision ||
        (outer && !params.mModelMatrix))
    {
        gGLLastMatrix = params.mModelMatrix;
        sLastOuterTransform = outer;
        sLastOuterTransformRevision = revision;
        gGL.matrixMode(LLRender::MM_MODELVIEW);
        gGL.loadMatrix(gGLModelView);
        if (outer && outer->mEnabled && !is_approx_equal(outer->mScale, 1.f))
        {
            gGL.multMatrix((GLfloat*)outer->mCurrent.mMatrix);
        }
        if (params.mModelMatrix)
        {
            gGL.multMatrix((const GLfloat*)params.mModelMatrix->mMatrix);
        }
        gPipeline.mMatrixOpCount++;
    }
}

void LLRenderPass::applyModelMatrix(const LLMatrix4* model_matrix)
{
    if (model_matrix != gGLLastMatrix || sLastOuterTransform)
    {
        gGLLastMatrix = model_matrix;
        sLastOuterTransform = nullptr;
        sLastOuterTransformRevision = 0;
        gGL.matrixMode(LLRender::MM_MODELVIEW);
        gGL.loadMatrix(gGLModelView);
        if (model_matrix)
        {
            gGL.multMatrix((GLfloat*) model_matrix->mMatrix);
        }
        gPipeline.mMatrixOpCount++;
    }
}

void LLRenderPass::invalidateModelMatrixCache()
{
    gGLLastMatrix = nullptr;
    sLastOuterTransform = nullptr;
    sLastOuterTransformRevision = 0;
}

// [BDMerge A5.4-1a] Bind a velocity program + upload the shared per-pass
// uniforms. The current jittered modelview/projection/MVP are auto-synced on
// draw; here we upload only the manual uniforms: the previous camera modelview
// (for last-frame reprojection) and the un-jittered current projection (so T2x
// jitter does not leak into the motion vectors -- brief pitfall 1).
//static
void LLRenderPass::bindVelocityUniforms(LLGLSLShader& shader)
{
    shader.uniformMatrix4fv(LLShaderMgr::LAST_MODELVIEW_MATRIX, 1, GL_FALSE, gGLLastModelView);
    shader.uniformMatrix4fv(LLShaderMgr::CURRENT_MODELVIEW_MATRIX, 1, GL_FALSE, gGLModelView);
    shader.uniformMatrix4fv(LLShaderMgr::PROJECTION_MATRIX_UNJITTERED, 1, GL_FALSE, gPipeline.mVelocityProjMat);
}

// [BDMerge A5.4-1a] Rigid velocity batch pushers. Donor: Black Dragon
// LLRenderPass::pushVelocityBatches / pushVelocityBatchesTextured
// (lldrawpool.cpp:802-910), adapted to Alchemy's iterator idiom.
//
// Per draw info: honour double-sided GLTF culling, apply the current model
// matrix (so modelview_matrix syncs to camera*object), upload the PREVIOUS
// object matrix (LAST_OBJECT_MATRIX) from mLastModelMatrix (identity fallback ->
// zero object velocity), draw, then WRITE BACK the current matrix into
// *mLastModelMatrix for next frame. The write-back must happen exactly once per
// drawable per frame; blended-alpha is intentionally excluded from the velocity
// pass to avoid double-stamping (see renderGeomVelocity exclusions).
void LLRenderPass::pushVelocityBatches(U32 type)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    static const LLMatrix4 identity;

    auto* begin = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);

    for (LLCullResult::drawinfo_iterator i = begin; i != end; )
    {
        LLDrawInfo& params = **i;
        LLCullResult::increment_iterator(i, end);

        if (params.mVertexBuffer.isNull())
        {
            continue;
        }

        if (shouldSuppressSharedActorFx(params))
        {
            // Keep rigid previous-transform history current while the actor is
            // intentionally absent, avoiding a one-frame velocity spike when
            // Cover opacity is raised again.
            if (params.mLastModelMatrix)
            {
                const LLMatrix4* current_mat = params.mModelMatrix
                    ? params.mModelMatrix : &identity;
                *params.mLastModelMatrix = *current_mat;
            }
            continue;
        }

        LLGLDisable cull_face(params.mGLTFMaterial && params.mGLTFMaterial->mDoubleSided ? GL_CULL_FACE : 0);

        applyModelMatrix(params);

        uploadActorFx(params);

        const LLMatrix4* last_mat = params.mLastModelMatrix ? params.mLastModelMatrix : &identity;
        LLGLSLShader::sCurBoundShaderPtr->uniformMatrix4fv(LLShaderMgr::LAST_OBJECT_MATRIX, 1, GL_FALSE, (GLfloat*)last_mat->mMatrix);

        params.mVertexBuffer->setBuffer();
        params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart, params.mEnd, params.mCount, params.mOffset);

        // store current matrix on the drawable for next frame's "previous"
        if (params.mLastModelMatrix)
        {
            const LLMatrix4* current_mat = params.mModelMatrix ? params.mModelMatrix : &identity;
            *params.mLastModelMatrix = *current_mat;
        }
    }
}

void LLRenderPass::pushVelocityBatchesTextured(U32 type, bool legacy_material)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    static const LLMatrix4 identity;

    auto* begin = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);

    for (LLCullResult::drawinfo_iterator i = begin; i != end; )
    {
        LLDrawInfo& params = **i;
        LLCullResult::increment_iterator(i, end);

        if (params.mVertexBuffer.isNull())
        {
            continue;
        }

        if (params.mMaterialSlotList.size() > 1 || params.mGLTFMaterialList.size() > 1)
        { // indexed multi-material geometry is emitted by the indexed helper
            continue;
        }

        if (shouldSuppressSharedActorFx(params))
        {
            if (params.mLastModelMatrix)
            {
                const LLMatrix4* current_mat = params.mModelMatrix
                    ? params.mModelMatrix : &identity;
                *params.mLastModelMatrix = *current_mat;
            }
            continue;
        }

        LLGLDisable cull_face(params.mGLTFMaterial && params.mGLTFMaterial->mDoubleSided ? GL_CULL_FACE : 0);

        applyModelMatrix(params);

        uploadActorFx(params);

        LLFetchedGLTFMaterial* gltf = params.mGLTFMaterial.get();
        if (gltf)
        {
            LLViewerTexture* base = params.mTexture.notNull()
                ? params.mTexture.get()
                : gltf->mBaseColorTexture.get();
            gGL.getTexUnit(0)->bindFast(base ? base : LLViewerFetchedTexture::sWhiteImagep.get());
            LLGLSLShader::sCurBoundShaderPtr->setMinimumAlpha(
                params.mAlphaMaskCutoff);
            LLGLTFMaterial::TextureTransform::Pack packed;
            gltf->mTextureTransform[LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR].getPacked(packed);
            LLGLSLShader::sCurBoundShaderPtr->uniform4fv(
                LLShaderMgr::TEXTURE_BASE_COLOR_TRANSFORM, 2, packed);
        }
        else
        {
            gGL.getTexUnit(0)->bindFast(params.mTexture.notNull()
                ? params.mTexture.get()
                : LLViewerFetchedTexture::sWhiteImagep.get());
            const F32 bias = legacy_material ? 0.001953125f : 0.f;
            LLGLSLShader::sCurBoundShaderPtr->setMinimumAlpha(
                params.mAlphaMaskCutoff - bias);
        }

        setup_texture_matrix(params);

        const LLMatrix4* last_mat = params.mLastModelMatrix ? params.mLastModelMatrix : &identity;
        LLGLSLShader::sCurBoundShaderPtr->uniformMatrix4fv(LLShaderMgr::LAST_OBJECT_MATRIX, 1, GL_FALSE, (GLfloat*)last_mat->mMatrix);

        params.mVertexBuffer->setBuffer();
        params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart, params.mEnd, params.mCount, params.mOffset);

        teardown_texture_matrix(params);

        if (params.mLastModelMatrix)
        {
            const LLMatrix4* current_mat = params.mModelMatrix ? params.mModelMatrix : &identity;
            *params.mLastModelMatrix = *current_mat;
        }
    }
}

// [BDMerge A5.4-1b] Upload current + previous skinning palettes for a rigged
// velocity draw. Donor: BD uploadMatrixPalette/uploadLastMatrixPalette pair,
// merged so AVATAR_LAST_MATRIX is ALWAYS left valid for the draw that follows:
// the donor skipped the last-palette upload when it had no previous data,
// silently reusing whatever palette the uniform held from the previous avatar
// (a one-frame velocity explosion on that mesh). Here the previous palette
// falls back to the CURRENT one -- zero limb velocity, camera velocity still
// correct -- whenever it is missing, non-contiguous (mLastFrame !=
// gFrameCount-1: avatar was culled or just appeared, brief pitfall 2/3), or
// sized differently (joint count changed under a mesh swap).
//static
bool LLRenderPass::uploadVelocityMatrixPalettes(LLVOAvatar* avatar, LLMeshSkinInfo* skinInfo,
                                                const LLVOAvatar*& lastAvatar, U64& lastMeshId,
                                                bool& skipLastSkin)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;

    llassert(skinInfo);
    llassert(LLGLSLShader::sCurBoundShaderPtr);

    if (!avatar)
    {
        return false;
    }

    if (avatar == lastAvatar && skinInfo->mHash == lastMeshId)
    {
        return !skipLastSkin;
    }

    const LLVOAvatar::MatrixPaletteCache& mpc = avatar->updateSkinInfoMatrixPalette(skinInfo);
    U32 count = static_cast<U32>(mpc.mMatrixPalette.size());
    skipLastSkin = !bool(count);
    lastAvatar = avatar;
    lastMeshId = skinInfo->mHash;

    if (!skipLastSkin)
    {
        LLGLSLShader::sCurBoundShaderPtr->uniformMatrix3x4fv(LLShaderMgr::AVATAR_MATRIX,
            count,
            false,
            (GLfloat*)&(mpc.mGLMp[0]));

        const bool prev_valid = (mpc.mLastFrame == gFrameCount - 1)
            && (mpc.mLastGLMp.size() == mpc.mGLMp.size());
        const GLfloat* last_mp = prev_valid ? (GLfloat*)&(mpc.mLastGLMp[0])
                                            : (GLfloat*)&(mpc.mGLMp[0]);
        LLGLSLShader::sCurBoundShaderPtr->uniformMatrix3x4fv(LLShaderMgr::AVATAR_LAST_MATRIX,
            count,
            false,
            last_mp);
    }

    return !skipLastSkin;
}

// [BDMerge A5.4-1b] Rigged velocity batch pushers. Same iteration shape as
// pushRiggedBatches; per (avatar, mesh) key both palettes are uploaded once,
// then every batch of that mesh draws. No mLastModelMatrix bookkeeping here --
// rigged motion lives entirely in the palettes.
void LLRenderPass::pushRiggedVelocityBatches(U32 type)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;

    const LLVOAvatar* lastAvatar = nullptr;
    U64 lastMeshId = 0;
    bool skipLastSkin = false;

    auto* begin = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);

    for (LLCullResult::drawinfo_iterator i = begin; i != end; )
    {
        LLDrawInfo& params = **i;
        LLCullResult::increment_iterator(i, end);

        if (params.mVertexBuffer.isNull() || !params.mAvatar || !params.mSkinInfo)
        {
            continue;
        }

        if (shouldSuppressSharedActorFx(params))
        {
            continue;
        }

        if (!uploadVelocityMatrixPalettes(params.mAvatar, params.mSkinInfo, lastAvatar, lastMeshId, skipLastSkin))
        {
            continue;
        }

        LLGLDisable cull_face(params.mGLTFMaterial && params.mGLTFMaterial->mDoubleSided ? GL_CULL_FACE : 0);

        applyModelMatrix(params);

        uploadActorFx(params);

        params.mVertexBuffer->setBuffer();
        params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart, params.mEnd, params.mCount, params.mOffset);
    }
}

void LLRenderPass::pushRiggedVelocityBatchesTextured(U32 type, bool legacy_material)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;

    const LLVOAvatar* lastAvatar = nullptr;
    U64 lastMeshId = 0;
    bool skipLastSkin = false;

    auto* begin = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);

    for (LLCullResult::drawinfo_iterator i = begin; i != end; )
    {
        LLDrawInfo& params = **i;
        LLCullResult::increment_iterator(i, end);

        if (params.mVertexBuffer.isNull() || !params.mAvatar || !params.mSkinInfo)
        {
            continue;
        }

        if (params.mMaterialSlotList.size() > 1 || params.mGLTFMaterialList.size() > 1)
        {
            continue;
        }

        if (shouldSuppressSharedActorFx(params))
        {
            continue;
        }

        if (!uploadVelocityMatrixPalettes(params.mAvatar, params.mSkinInfo, lastAvatar, lastMeshId, skipLastSkin))
        {
            continue;
        }

        LLGLDisable cull_face(params.mGLTFMaterial && params.mGLTFMaterial->mDoubleSided ? GL_CULL_FACE : 0);

        applyModelMatrix(params);

        uploadActorFx(params);

        LLFetchedGLTFMaterial* gltf = params.mGLTFMaterial.get();
        if (gltf)
        {
            LLViewerTexture* base = params.mTexture.notNull()
                ? params.mTexture.get()
                : gltf->mBaseColorTexture.get();
            gGL.getTexUnit(0)->bindFast(base ? base : LLViewerFetchedTexture::sWhiteImagep.get());
            LLGLSLShader::sCurBoundShaderPtr->setMinimumAlpha(
                params.mAlphaMaskCutoff);
            LLGLTFMaterial::TextureTransform::Pack packed;
            gltf->mTextureTransform[LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR].getPacked(packed);
            LLGLSLShader::sCurBoundShaderPtr->uniform4fv(
                LLShaderMgr::TEXTURE_BASE_COLOR_TRANSFORM, 2, packed);
        }
        else
        {
            gGL.getTexUnit(0)->bindFast(params.mTexture.notNull()
                ? params.mTexture.get()
                : LLViewerFetchedTexture::sWhiteImagep.get());
            const F32 bias = legacy_material ? 0.001953125f : 0.f;
            LLGLSLShader::sCurBoundShaderPtr->setMinimumAlpha(
                params.mAlphaMaskCutoff - bias);
        }

        setup_texture_matrix(params);

        params.mVertexBuffer->setBuffer();
        params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart, params.mEnd, params.mCount, params.mOffset);
        teardown_texture_matrix(params);
    }
}

void LLRenderPass::pushVelocityAlphaBatchesIndexed(U32 type, bool gltf, bool rigged)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    static const LLMatrix4 identity;
    const LLVOAvatar* last_avatar = nullptr;
    U64 last_mesh_id = 0;
    bool skip_last_skin = false;

    auto* begin = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);
    for (LLCullResult::drawinfo_iterator i = begin; i != end; )
    {
        LLDrawInfo& params = **i;
        LLCullResult::increment_iterator(i, end);

        const S32 list_size = gltf ? (S32)params.mGLTFMaterialList.size()
                                   : (S32)params.mMaterialSlotList.size();
        if (params.mVertexBuffer.isNull() || list_size < 2)
        {
            continue;
        }
        if (shouldSuppressSharedActorFx(params))
        {
            if (!rigged && params.mLastModelMatrix)
            {
                const LLMatrix4* current_mat = params.mModelMatrix
                    ? params.mModelMatrix : &identity;
                *params.mLastModelMatrix = *current_mat;
            }
            continue;
        }
        if (rigged && (!params.mAvatar || !params.mSkinInfo ||
            !uploadVelocityMatrixPalettes(params.mAvatar, params.mSkinInfo,
                                          last_avatar, last_mesh_id, skip_last_skin)))
        {
            continue;
        }

        const S32 n = llmin(list_size, LLGLSLShader::sIndexedGLTFChannels);
        F32 min_alpha[8] = { 0.f };
        F32 bc_xform[8 * 8] = { 0.f };
        bool double_sided = false;
        for (S32 s = 0; s < n; ++s)
        {
            if (gltf)
            {
                LLFetchedGLTFMaterial* mat = params.mGLTFMaterialList[s].get();
                if (!mat)
                {
                    min_alpha[s] = -1.f;
                    continue;
                }
                double_sided = double_sided || mat->mDoubleSided;
                LLViewerTexture* base = mat->mBaseColorTexture.notNull()
                    ? mat->mBaseColorTexture.get()
                    : LLViewerFetchedTexture::sWhiteImagep.get();
                gGL.getTexUnit(s)->bindFast(base);
                min_alpha[s] =
                    s < (S32)params.mGLTFAlphaMaskCutoffList.size()
                        ? params.mGLTFAlphaMaskCutoffList[s]
                        : mat->mAlphaCutoff;
                LLGLTFMaterial::TextureTransform::Pack packed;
                mat->mTextureTransform[LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR].getPacked(packed);
                memcpy(&bc_xform[8 * s], packed, sizeof(packed));
            }
            else
            {
                const LLDrawInfo::MaterialSlot& slot = params.mMaterialSlotList[s];
                LLViewerTexture* diffuse = slot.mDiffuse.notNull()
                    ? slot.mDiffuse.get()
                    : LLViewerFetchedTexture::sWhiteImagep.get();
                gGL.getTexUnit(s)->bindFast(diffuse);
                min_alpha[s] = slot.mAlphaMaskCutoff - 0.001953125f;
            }
        }

        LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
        static const LLStaticHashedString sVelocityMinAlpha("velocity_minimum_alpha");
        shader->uniform1fv(sVelocityMinAlpha, n, min_alpha);
        if (gltf)
        {
            static const LLStaticHashedString sBaseXform("gltf_basecolor_transform");
            shader->uniform4fv(sBaseXform, 2 * n, bc_xform);
        }

        LLGLDisable cull_face(double_sided ? GL_CULL_FACE : 0);
        applyModelMatrix(params);
        uploadActorFx(params);
        if (!rigged)
        {
            const LLMatrix4* last_mat = params.mLastModelMatrix
                ? params.mLastModelMatrix : &identity;
            shader->uniformMatrix4fv(LLShaderMgr::LAST_OBJECT_MATRIX, 1, GL_FALSE,
                                     (GLfloat*)last_mat->mMatrix);
        }

        params.mVertexBuffer->setBuffer();
        params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart,
                                        params.mEnd, params.mCount, params.mOffset);
        if (!rigged && params.mLastModelMatrix)
        {
            const LLMatrix4* current_mat = params.mModelMatrix
                ? params.mModelMatrix : &identity;
            *params.mLastModelMatrix = *current_mat;
        }
    }
}

void LLRenderPass::pushBatch(LLDrawInfo& params, bool texture, bool batch_textures)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    llassert(texture);

    if (!params.mCount || shouldSuppressSharedActorFx(params))
    {
        return;
    }
    applyModelMatrix(params);

    bool tex_setup = false;

    {
        if (batch_textures && params.mTextureList.size() > 1)
        {
            for (U32 i = 0; i < params.mTextureList.size(); ++i)
            {
                if (params.mTextureList[i].notNull())
                {
                    gGL.getTexUnit(i)->bindFast(params.mTextureList[i]);
                }
            }
        }
        else
        { //not batching textures or batch has only 1 texture -- might need a texture matrix
            if (params.mTexture.notNull())
            {
                gGL.getTexUnit(0)->bindFast(params.mTexture);
                if (params.mTextureMatrix)
                {
                    tex_setup = true;
                    gGL.getTexUnit(0)->activate();
                    gGL.matrixMode(LLRender::MM_TEXTURE);
                    gGL.loadMatrix((GLfloat*) params.mTextureMatrix->mMatrix);
                    gPipeline.mTextureMatrixOps++;
                }
            }
            else
            {
                gGL.getTexUnit(0)->unbindFast(LLTexUnit::TT_TEXTURE);
            }
        }
    }

    uploadActorFx(params);
    params.mVertexBuffer->setBuffer();
    params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart, params.mEnd, params.mCount, params.mOffset);

    if (tex_setup)
    {
        gGL.matrixMode(LLRender::MM_TEXTURE0);
        gGL.loadIdentity();
        gGL.matrixMode(LLRender::MM_MODELVIEW);
    }
}

void LLRenderPass::pushUntexturedBatch(LLDrawInfo& params)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;

    if (!params.mCount || shouldSuppressSharedActorFx(params))
    {
        return;
    }
    applyModelMatrix(params);

    uploadActorFx(params);
    params.mVertexBuffer->setBuffer();
    params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart, params.mEnd, params.mCount, params.mOffset);
}

// static
bool LLRenderPass::uploadMatrixPalette(LLDrawInfo& params)
{
    // upload matrix palette to shader
    return uploadMatrixPalette(params.mAvatar, params.mSkinInfo);
}

//static
bool LLRenderPass::uploadMatrixPalette(LLVOAvatar* avatar, LLMeshSkinInfo* skinInfo)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;

    if (!avatar)
    {
        return false;
    }
    const LLVOAvatar::MatrixPaletteCache& mpc = avatar->updateSkinInfoMatrixPalette(skinInfo);
    U32 count = static_cast<U32>(mpc.mMatrixPalette.size());
    if (count == 0)
    {
        //skin info not loaded yet, don't render
        return false;
    }

    LLGLSLShader::sCurBoundShaderPtr->uniformMatrix3x4fv(LLViewerShaderMgr::AVATAR_MATRIX,
        count,
        false,
        (GLfloat*)&(mpc.mGLMp[0]));

    return true;
}

// Returns true if rendering should proceed
//static
bool LLRenderPass::uploadMatrixPalette(LLVOAvatar* avatar, LLMeshSkinInfo* skinInfo, const LLVOAvatar*& lastAvatar, U64& lastMeshId, bool& skipLastSkin)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;

    llassert(skinInfo);
    llassert(LLGLSLShader::sCurBoundShaderPtr);

    if (!avatar)
    {
        return false;
    }

    if (avatar == lastAvatar && skinInfo->mHash == lastMeshId)
    {
        return !skipLastSkin;
    }

    const LLVOAvatar::MatrixPaletteCache& mpc = avatar->updateSkinInfoMatrixPalette(skinInfo);
    U32 count = static_cast<U32>(mpc.mMatrixPalette.size());
    // skipLastSkin -> skin info not loaded yet, don't render
    skipLastSkin = !bool(count);
    lastAvatar = avatar;
    lastMeshId = skinInfo->mHash;

    if (!skipLastSkin)
    {
        LLGLSLShader::sCurBoundShaderPtr->uniformMatrix3x4fv(LLViewerShaderMgr::AVATAR_MATRIX,
            count,
            false,
            (GLfloat*)&(mpc.mGLMp[0]));
    }

    return !skipLastSkin;
}

// Returns true if rendering should proceed
//static
bool LLRenderPass::uploadMatrixPalette(LLVOAvatar* avatar, LLMeshSkinInfo* skinInfo, const LLVOAvatar*& lastAvatar, U64& lastMeshId, const LLGLSLShader*& lastAvatarShader, bool& skipLastSkin)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;

    llassert(skinInfo);
    llassert(LLGLSLShader::sCurBoundShaderPtr);

    if (!avatar)
    {
        return false;
    }

    if (avatar == lastAvatar && skinInfo->mHash == lastMeshId && lastAvatarShader == LLGLSLShader::sCurBoundShaderPtr)
    {
        return !skipLastSkin;
    }

    const LLVOAvatar::MatrixPaletteCache& mpc = avatar->updateSkinInfoMatrixPalette(skinInfo);
    U32 count = static_cast<U32>(mpc.mMatrixPalette.size());
    // skipLastSkin -> skin info not loaded yet, don't render
    skipLastSkin = !bool(count);
    lastAvatar = avatar;
    lastMeshId = skinInfo->mHash;
    lastAvatarShader = LLGLSLShader::sCurBoundShaderPtr;

    if (!skipLastSkin)
    {
        LLGLSLShader::sCurBoundShaderPtr->uniformMatrix3x4fv(LLViewerShaderMgr::AVATAR_MATRIX,
            count,
            false,
            (GLfloat*)&(mpc.mGLMp[0]));
    }

    return !skipLastSkin;
}

// static
void LLRenderPass::setupGLTFTextureMatrix(const LLMatrix4* texture_matrix)
{
    if (texture_matrix)
    { //special case implementation of texture animation here because of special handling of textures for PBR batches
        gGL.getTexUnit(0)->activate();
        gGL.matrixMode(LLRender::MM_TEXTURE);
        gGL.loadMatrix((GLfloat*)texture_matrix->mMatrix);
        gPipeline.mTextureMatrixOps++;
    }
}

// static
void LLRenderPass::teardownGLTFTextureMatrix(const LLMatrix4* texture_matrix)
{
    if (texture_matrix)
    {
        gGL.matrixMode(LLRender::MM_TEXTURE0);
        gGL.loadIdentity();
        gGL.matrixMode(LLRender::MM_MODELVIEW);
    }
}

void setup_texture_matrix(LLDrawInfo& params)
{
    LLRenderPass::setupGLTFTextureMatrix(params.mTextureMatrix);
}

void teardown_texture_matrix(LLDrawInfo& params)
{
    LLRenderPass::teardownGLTFTextureMatrix(params.mTextureMatrix);
}

void LLRenderPass::pushGLTFBatches(U32 type, bool textured)
{
    if (textured)
    {
        pushGLTFBatches(type);
    }
    else
    {
        pushUntexturedGLTFBatches(type);
    }
}

void LLRenderPass::pushGLTFBatches(U32 type)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    LLFetchedGLTFMaterial* lastMat = nullptr;
    LLViewerTexture* lastTex = nullptr;
    auto* begin = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);
    for (LLCullResult::drawinfo_iterator i = begin; i != end; )
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DRAWPOOL("pushGLTFBatch");
        LLDrawInfo& params = **i;
        LLCullResult::increment_iterator(i, end);

        if (skip_rain_occlusion_attachment(&params))
        {
            continue;
        }
        pushGLTFBatch(params, lastMat, lastTex);
    }
}

void LLRenderPass::pushUntexturedGLTFBatches(U32 type)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    auto* begin = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);
    for (LLCullResult::drawinfo_iterator i = begin; i != end; )
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DRAWPOOL("pushGLTFBatch");
        LLDrawInfo& params = **i;
        LLCullResult::increment_iterator(i, end);

        if (skip_rain_occlusion_attachment(&params))
        {
            continue;
        }
        pushUntexturedGLTFBatch(params);
    }
}

// Like pushGLTFBatches, but skips multi-material (indexed) draw infos -- those are
// rendered separately by pushGLTFBatchesIndexed under the indexed shader. Used by
// the main opaque GBuffer pass only.
void LLRenderPass::pushGLTFBatchesScalar(U32 type)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    LLFetchedGLTFMaterial* lastMat = nullptr;
    LLViewerTexture* lastTex = nullptr;
    auto* begin = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);
    for (LLCullResult::drawinfo_iterator i = begin; i != end; )
    {
        LLDrawInfo& params = **i;
        LLCullResult::increment_iterator(i, end);

        if (skip_rain_occlusion_attachment(&params))
        {
            continue;
        }
        if (params.mGLTFMaterialList.size() > 1)
        { // multi-material batch -- handled by the indexed sweep
            continue;
        }

        pushGLTFBatch(params, lastMat, lastTex);
    }
}

// Renders only the multi-material (indexed) draw infos. Assumes the indexed PBR
// shader is bound. maps selects which material maps to bind (see eGLTFIndexedMaps).
void LLRenderPass::pushGLTFBatchesIndexed(U32 type, eGLTFIndexedMaps maps)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    auto* begin = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);
    for (LLCullResult::drawinfo_iterator i = begin; i != end; )
    {
        LLDrawInfo& params = **i;
        LLCullResult::increment_iterator(i, end);

        if (skip_rain_occlusion_attachment(&params))
        {
            continue;
        }
        if (params.mGLTFMaterialList.size() < 2)
        { // single-material batch -- handled by the scalar sweep
            continue;
        }

        pushGLTFBatchIndexed(params, maps);
    }
}

// static
// Bind one draw call's worth of indexed materials and emit it. Each material slot
// s binds its maps to texture units [s, N+s, 2N+s, 3N+s] (N == shader's
// active shader material stride) and contributes one element to the per-slot scalar/transform
// uniform arrays. Mirrors LLFetchedGLTFMaterial::bind for the default-texture and
// factor handling. maps trims the bound/uploaded set: GLTF_MAPS_BASE_COLOR (shadow
// alpha-mask) binds only base color; GLTF_MAPS_GLOW (glow pass) adds emissive but
// skips normal/ORM; GLTF_MAPS_FULL (GBuffer write) binds everything.
void LLRenderPass::pushGLTFBatchIndexed(LLDrawInfo& params,
                                        eGLTFIndexedMaps maps,
                                        bool shared_replay)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;

    if (!shared_replay && shouldSuppressSharedActorFxPBR(params))
    {
        return;
    }

    const bool want_base     = (maps != GLTF_MAPS_NONE);
    const bool want_emissive = (maps == GLTF_MAPS_FULL || maps == GLTF_MAPS_GLOW);
    const bool want_full     = (maps == GLTF_MAPS_FULL); // normal + ORM

    const S32 N = shared_replay
        ? LLGLSLShader::sSharedPBRIndexedGLTFChannels
        : LLGLSLShader::sIndexedGLTFChannels; // shader sampler-array stride
    // Slot count is capped at N (<= 8) by genDrawInfo; clamp defensively so a stale
    // or over-long list can never overrun the fixed 8-slot arrays / N sampler units.
    llassert((S32)params.mGLTFMaterialList.size() <= N);
    const S32 n = llmin((S32)params.mGLTFMaterialList.size(), N); // materials in this batch
    LL_PROFILE_ZONE_NUM(n);

    LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;

    // gathered per-slot uniform data (max 8 slots; see sIndexedGLTFChannels clamp)
    F32 roughness[8] = { 0.f };
    F32 metallic[8]  = { 0.f };
    F32 min_alpha[8] = { 0.f };
    F32 emissive[3 * 8] = { 0.f };
    F32 bc_xform[8 * 8] = { 0.f }; // 2 vec4 (8 floats) per slot
    F32 nm_xform[8 * 8] = { 0.f };
    F32 mr_xform[8 * 8] = { 0.f };
    F32 em_xform[8 * 8] = { 0.f };

    bool double_sided = false;

    for (S32 s = 0; s < n; ++s)
    {
        LLFetchedGLTFMaterial* mat = params.mGLTFMaterialList[s].get();
        if (mat == nullptr)
        { // gap left by a fragmented batch -- this slot is never sampled
            min_alpha[s] = -1.f;
            continue;
        }

        double_sided = double_sided || mat->mDoubleSided;

        if (!want_base)
        {
            continue;
        }

        LLViewerTexture* base = mat->mBaseColorTexture.notNull() ? mat->mBaseColorTexture.get() : LLViewerFetchedTexture::sWhiteImagep.get();
        gGL.getTexUnit(s)->bindFast(base);

        min_alpha[s] =
            s < (S32)params.mGLTFAlphaMaskCutoffList.size()
                ? params.mGLTFAlphaMaskCutoffList[s]
                : ((mat->mAlphaMode == LLGLTFMaterial::ALPHA_MODE_MASK)
                       ? mat->mAlphaCutoff
                       : -1.f);

        // getPacked() takes F32(&)[8]; copy each transform into its slot stride.
        LLGLTFMaterial::TextureTransform::Pack packed;
        mat->mTextureTransform[LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR].getPacked(packed);
        memcpy(&bc_xform[8 * s], packed, sizeof(packed));

        if (!want_emissive)
        { // shadow alpha-mask samples only base color
            continue;
        }

        // emissive map/color/transform -- needed by both the glow and GBuffer passes
        LLViewerTexture* em = mat->mEmissiveTexture.notNull() ? mat->mEmissiveTexture.get() : LLViewerFetchedTexture::sWhiteImagep.get();
        gGL.getTexUnit(3 * N + s)->bindFast(em);

        emissive[3 * s + 0] = mat->mEmissiveColor.mV[0];
        emissive[3 * s + 1] = mat->mEmissiveColor.mV[1];
        emissive[3 * s + 2] = mat->mEmissiveColor.mV[2];

        mat->mTextureTransform[LLGLTFMaterial::GLTF_TEXTURE_INFO_EMISSIVE].getPacked(packed);
        memcpy(&em_xform[8 * s], packed, sizeof(packed));

        if (!want_full)
        { // glow needs base color + emissive only
            continue;
        }

        LLViewerTexture* norm = (mat->mNormalTexture.notNull() && mat->mNormalTexture->getDiscardLevel() <= 4) ? mat->mNormalTexture.get() : LLViewerFetchedTexture::sFlatNormalImagep.get();
        LLViewerTexture* orm  = mat->mMetallicRoughnessTexture.notNull() ? mat->mMetallicRoughnessTexture.get() : LLViewerFetchedTexture::sWhiteImagep.get();

        gGL.getTexUnit(N + s)->bindFast(norm);
        gGL.getTexUnit(2 * N + s)->bindFast(orm);

        roughness[s] = mat->mRoughnessFactor;
        metallic[s]  = mat->mMetallicFactor;

        mat->mTextureTransform[LLGLTFMaterial::GLTF_TEXTURE_INFO_NORMAL].getPacked(packed);
        memcpy(&nm_xform[8 * s], packed, sizeof(packed));
        mat->mTextureTransform[LLGLTFMaterial::GLTF_TEXTURE_INFO_METALLIC_ROUGHNESS].getPacked(packed);
        memcpy(&mr_xform[8 * s], packed, sizeof(packed));
    }

    static const LLStaticHashedString sMinAlpha("gltf_minimum_alpha");
    static const LLStaticHashedString sBcXform("gltf_basecolor_transform");

    if (want_base)
    {
        shader->uniform1fv(sMinAlpha, n, min_alpha);
        shader->uniform4fv(sBcXform, 2 * n, bc_xform);
    }

    if (want_emissive)
    {
        static const LLStaticHashedString sEmissive("gltf_emissive_color");
        static const LLStaticHashedString sEmXform("gltf_emissive_transform");

        shader->uniform3fv(sEmissive, n, emissive);
        shader->uniform4fv(sEmXform, 2 * n, em_xform);
    }

    if (want_full)
    {
        static const LLStaticHashedString sRoughness("gltf_roughness_factor");
        static const LLStaticHashedString sMetallic("gltf_metallic_factor");
        static const LLStaticHashedString sNmXform("gltf_normal_transform");
        static const LLStaticHashedString sMrXform("gltf_mr_transform");

        shader->uniform1fv(sRoughness, n, roughness);
        shader->uniform1fv(sMetallic, n, metallic);
        shader->uniform4fv(sNmXform, 2 * n, nm_xform);
        shader->uniform4fv(sMrXform, 2 * n, mr_xform);
    }

    LLGLDisable cull_face(double_sided ? GL_CULL_FACE : 0);

    applyModelMatrix(params);

    // Shared replay uploads the proxy's authoritative style immediately before
    // entering this helper (including the topology-wire bit).  Do not resolve
    // and overwrite it from the source DrawInfo a second time.
    if (!shared_replay)
    {
        uploadActorFx(params);
    }
    params.mVertexBuffer->setBuffer();
    params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart, params.mEnd, params.mCount, params.mOffset);
}

// static
void LLRenderPass::pushGLTFBatch(LLDrawInfo& params, LLFetchedGLTFMaterial*& lastMat, LLViewerTexture*& lastTex)
{
    if (shouldSuppressSharedActorFxPBR(params))
    {
        return;
    }

    LLFetchedGLTFMaterial* mat = params.mGLTFMaterial.get();

    if (mat)
    {
        // params.mTexture is the media override (bind() applies it to base color
        // and emissive), so it is part of the cache key -- otherwise media faces
        // sharing a material would render with a stale base texture.
        LLViewerTexture* tex = params.mTexture.get();
        if (mat != lastMat || tex != lastTex)
        {
            mat->bind(params.mTexture, params.mAlphaMaskCutoff);
            lastMat = mat;
            lastTex = tex;
        }
    }

    // Material/texture caching must not cache the per-draw cutoff. Two objects
    // may share one PBR asset while carrying different client overrides.
    if (!LLPipeline::sShadowRender || params.mAlphaMaskCutoff >= 0.f)
    {
        LLGLSLShader::sCurBoundShaderPtr->setMinimumAlpha(
            params.mAlphaMaskCutoff);
    }

    LLGLDisable cull_face(mat && mat->mDoubleSided ? GL_CULL_FACE : 0);

    setup_texture_matrix(params);

    applyModelMatrix(params);

    uploadActorFx(params);
    params.mVertexBuffer->setBuffer();
    params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart, params.mEnd, params.mCount, params.mOffset);

    teardown_texture_matrix(params);
}

// static
void LLRenderPass::pushUntexturedGLTFBatch(LLDrawInfo& params)
{
    if (shouldSuppressSharedActorFxPBR(params))
    {
        return;
    }

    auto& mat = params.mGLTFMaterial;

    LLGLDisable cull_face(mat->mDoubleSided ? GL_CULL_FACE : 0);

    if (!LLPipeline::sShadowRender || params.mAlphaMaskCutoff >= 0.f)
    {
        LLGLSLShader::sCurBoundShaderPtr->setMinimumAlpha(
            params.mAlphaMaskCutoff);
    }

    applyModelMatrix(params);

    uploadActorFx(params);
    params.mVertexBuffer->setBuffer();
    params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart, params.mEnd, params.mCount, params.mOffset);
}

void LLRenderPass::pushRiggedGLTFBatches(U32 type, bool textured)
{
    if (textured)
    {
        pushRiggedGLTFBatches(type);
    }
    else
    {
        pushUntexturedRiggedGLTFBatches(type);
    }
}

void LLRenderPass::pushRiggedGLTFBatches(U32 type)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    const LLVOAvatar* lastAvatar = nullptr;
    U64 lastMeshId = 0;
    bool skipLastSkin = false;
    LLFetchedGLTFMaterial* lastMat = nullptr;
    LLViewerTexture* lastTex = nullptr;

    auto* begin = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);
    for (LLCullResult::drawinfo_iterator i = begin; i != end; )
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DRAWPOOL("pushRiggedGLTFBatch");
        LLDrawInfo& params = **i;
        LLCullResult::increment_iterator(i, end);

        pushRiggedGLTFBatch(params, lastAvatar, lastMeshId, skipLastSkin, lastMat, lastTex);
    }
}

void LLRenderPass::pushUntexturedRiggedGLTFBatches(U32 type)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    const LLVOAvatar* lastAvatar = nullptr;
    U64 lastMeshId = 0;
    bool skipLastSkin = false;

    auto* begin = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);
    for (LLCullResult::drawinfo_iterator i = begin; i != end; )
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DRAWPOOL("pushRiggedGLTFBatch");
        LLDrawInfo& params = **i;
        LLCullResult::increment_iterator(i, end);

        pushUntexturedRiggedGLTFBatch(params, lastAvatar, lastMeshId, skipLastSkin);
    }
}


// static
void LLRenderPass::pushRiggedGLTFBatch(LLDrawInfo& params, const LLVOAvatar*& lastAvatar, U64& lastMeshId, bool& skipLastSkin, LLFetchedGLTFMaterial*& lastMat, LLViewerTexture*& lastTex)
{
    if (uploadMatrixPalette(params.mAvatar, params.mSkinInfo, lastAvatar, lastMeshId, skipLastSkin))
    {
        pushGLTFBatch(params, lastMat, lastTex);
    }
}

// static
void LLRenderPass::pushUntexturedRiggedGLTFBatch(LLDrawInfo& params, const LLVOAvatar*& lastAvatar, U64& lastMeshId, bool& skipLastSkin)
{
    if (uploadMatrixPalette(params.mAvatar, params.mSkinInfo, lastAvatar, lastMeshId, skipLastSkin))
    {
        pushUntexturedGLTFBatch(params);
    }
}

// rigged counterpart of pushGLTFBatchesScalar -- skips multi-material infos
void LLRenderPass::pushRiggedGLTFBatchesScalar(U32 type)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    const LLVOAvatar* lastAvatar = nullptr;
    U64 lastMeshId = 0;
    bool skipLastSkin = false;
    LLFetchedGLTFMaterial* lastMat = nullptr;
    LLViewerTexture* lastTex = nullptr;

    auto* begin = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);
    for (LLCullResult::drawinfo_iterator i = begin; i != end; )
    {
        LLDrawInfo& params = **i;
        LLCullResult::increment_iterator(i, end);

        if (params.mGLTFMaterialList.size() > 1)
        { // multi-material batch -- handled by the indexed sweep
            continue;
        }

        pushRiggedGLTFBatch(params, lastAvatar, lastMeshId, skipLastSkin, lastMat, lastTex);
    }
}

// rigged counterpart of pushGLTFBatchesIndexed -- only multi-material infos.
// Assumes the rigged indexed program is bound.
void LLRenderPass::pushRiggedGLTFBatchesIndexed(U32 type, eGLTFIndexedMaps maps)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    const LLVOAvatar* lastAvatar = nullptr;
    U64 lastMeshId = 0;
    bool skipLastSkin = false;

    auto* begin = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);
    for (LLCullResult::drawinfo_iterator i = begin; i != end; )
    {
        LLDrawInfo& params = **i;
        LLCullResult::increment_iterator(i, end);

        if (params.mGLTFMaterialList.size() < 2)
        { // single-material batch -- handled by the scalar sweep
            continue;
        }

        pushRiggedGLTFBatchIndexed(params, lastAvatar, lastMeshId, skipLastSkin, maps);
    }
}

// static
void LLRenderPass::pushRiggedGLTFBatchIndexed(LLDrawInfo& params, const LLVOAvatar*& lastAvatar, U64& lastMeshId, bool& skipLastSkin, eGLTFIndexedMaps maps)
{
    if (uploadMatrixPalette(params.mAvatar, params.mSkinInfo, lastAvatar, lastMeshId, skipLastSkin))
    {
        pushGLTFBatchIndexed(params, maps);
    }
}
