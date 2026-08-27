/**
 * @file llviewershadermgr.cpp
 * @brief Viewer shader manager implementation.
 *
 * $LicenseInfo:firstyear=2005&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * Alchemy Viewer Source Code
 * Copyright © 2026, Rye <rye@alchemyviewer.org>
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

#include <boost/lexical_cast.hpp>
#include <filesystem>
#include <fstream>

#include "hbxxh.h"
#include "fsyspath.h"
#include "llfeaturemanager.h"
#include "llviewershadermgr.h"
#include "llviewercontrol.h"
#include "llversioninfo.h"

#include "llrender.h"
#include "llenvironment.h"
#include "llerrorcontrol.h"
#include "llworld.h"
#include "llsky.h"

#include "pipeline.h"

#include "llfile.h"
#include "llviewerwindow.h"
#include "llwindow.h"

#include "lljoint.h"
#include "llskinningutil.h"

static LLStaticHashedString sTexture0("texture0");
static LLStaticHashedString sTexture1("texture1");
static LLStaticHashedString sTex0("tex0");
static LLStaticHashedString sTex1("tex1");
static LLStaticHashedString sDitherTex("dither_tex");

// Lots of STL stuff in here, using namespace std to keep things more readable
using std::vector;
using std::pair;
using std::make_pair;
using std::string;

bool                LLViewerShaderMgr::sInitialized = false;
bool                LLViewerShaderMgr::sSkipReload = false;

LLVector4           gShinyOrigin;

S32 clamp_terrain_mapping(S32 mapping)
{
    // 1 = "flat", 2 not implemented, 3 = triplanar mapping
    mapping = llclamp(mapping, 1, 3);
    if (mapping == 2) { mapping = 1; }
    return mapping;
}

//utility shaders
LLGLSLShader    gOcclusionProgram;
LLGLSLShader    gSkinnedOcclusionProgram;
LLGLSLShader    gOcclusionCubeProgram;
LLGLSLShader    gGlowCombineProgram;
LLGLSLShader    gReflectionMipProgram;
LLGLSLShader    gGaussianProgram;
LLGLSLShader    gRadianceGenProgram;
LLGLSLShader    gHeroRadianceGenProgram;
LLGLSLShader    gIrradianceGenProgram;
LLGLSLShader    gGlowCombineFXAAProgram;
LLGLSLShader    gTwoTextureCompareProgram;
LLGLSLShader    gOneTextureFilterProgram;
LLGLSLShader    gDebugProgram;
LLGLSLShader    gSkinnedDebugProgram;
LLGLSLShader    gNormalDebugProgram[NORMAL_DEBUG_SHADER_COUNT];
LLGLSLShader    gSkinnedNormalDebugProgram[NORMAL_DEBUG_SHADER_COUNT];
LLGLSLShader    gClipProgram;
LLGLSLShader    gAlphaMaskProgram;
LLGLSLShader    gBenchmarkProgram;
LLGLSLShader    gReflectionProbeDisplayProgram;
LLGLSLShader    gCopyProgram;
LLGLSLShader    gCopyDepthProgram;
LLGLSLShader    gPBRTerrainBakeProgram;
LLGLSLShader    gDrawColorProgram;

//object shaders
LLGLSLShader        gObjectPreviewProgram;
LLGLSLShader        gSkinnedObjectPreviewProgram;
LLGLSLShader        gPhysicsPreviewProgram;
LLGLSLShader        gObjectFullbrightAlphaMaskProgram;
LLGLSLShader        gObjectBumpProgram;
LLGLSLShader        gSkinnedObjectBumpProgram;
LLGLSLShader        gObjectAlphaMaskNoColorProgram;

//environment shaders
LLGLSLShader        gWaterProgram;
LLGLSLShader        gUnderWaterProgram;

//interface shaders
LLGLSLShader        gHighlightProgram;
LLGLSLShader        gSkinnedHighlightProgram;
LLGLSLShader        gHighlightNormalProgram;
LLGLSLShader        gHighlightSpecularProgram;
// [ActorMover] pose-ghost FX (hologram / x-ray ghost styles)
LLGLSLShader        gActorGhostProgram;
LLGLSLShader        gSkinnedActorGhostProgram;
LLGLSLShader        gWorldActorGhostProgram;
LLGLSLShader        gWorldSkinnedActorGhostProgram;
LLGLSLShader        gWorldActorGhostIndexedProgram;
LLGLSLShader        gWorldSkinnedActorGhostIndexedProgram;
LLGLSLShader        gAvatarActorGhostProgram;
LLGLSLShader        gAvatarEyeballActorGhostProgram;

LLGLSLShader        gDeferredHighlightProgram;

LLGLSLShader        gPathfindingProgram;
LLGLSLShader        gPathfindingNoNormalsProgram;

//avatar shader handles
LLGLSLShader        gAvatarProgram;
LLGLSLShader        gAvatarEyeballProgram;
LLGLSLShader        gImpostorProgram;

// Effects Shaders
LLGLSLShader            gGlowProgram;
LLGLSLShader            gGlowExtractProgram;
LLGLSLShader            gBloomExtractProgram;
LLGLSLShader            gBloomDownsampleProgram;
LLGLSLShader            gBloomDownsampleFirstProgram;
LLGLSLShader            gBloomUpsampleProgram;
LLGLSLShader            gBloomCompositeProgram;
LLGLSLShader            gPostScreenSpaceReflectionProgram;

// Deferred rendering shaders
LLGLSLShader            gDeferredImpostorProgram;
LLGLSLShader            gPrismLensProgram;
LLGLSLShader            gDeferredDiffuseProgram;
LLGLSLShader            gDeferredDiffuseAlphaMaskProgram;
LLGLSLShader            gDeferredSkinnedDiffuseAlphaMaskProgram;
LLGLSLShader            gDeferredNonIndexedDiffuseAlphaMaskProgram;
LLGLSLShader            gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram;
LLGLSLShader            gDeferredSkinnedDiffuseProgram;
LLGLSLShader            gDeferredSkinnedBumpProgram;
LLGLSLShader            gDeferredBumpProgram;
LLGLSLShader            gDeferredTerrainProgram;
LLGLSLShader            gDeferredTreeProgram;
LLGLSLShader            gDeferredTreeShadowProgram;
LLGLSLShader            gDeferredSkinnedTreeShadowProgram;
LLGLSLShader            gDeferredAvatarProgram;
LLGLSLShader            gDeferredAvatarAlphaProgram;
LLGLSLShader            gDeferredLightProgram;
LLGLSLShader            gDeferredMultiLightProgram[16];
LLGLSLShader            gDeferredSpotLightProgram;
LLGLSLShader            gDeferredMultiSpotLightProgram;
LLGLSLShader            gDeferredSunProgram;
LLGLSLShader            gDeferredSunProbeProgram;
LLGLSLShader            gHazeProgram;
LLGLSLShader            gHazeWaterProgram;
LLGLSLShader            gDeferredBlurLightProgram;
LLGLSLShader            gDeferredSoftenProgram;
LLGLSLShader            gVisibleDiffuseSeedProgram;
LLGLSLShader            gDeferredShadowProgram;
LLGLSLShader            gDeferredSkinnedShadowProgram;
LLGLSLShader            gDeferredShadowCubeProgram;
LLGLSLShader            gDeferredShadowAlphaMaskProgram;
LLGLSLShader            gDeferredSkinnedShadowAlphaMaskProgram;
LLGLSLShader            gDeferredShadowGLTFAlphaMaskProgram;
LLGLSLShader            gDeferredShadowGLTFAlphaMaskIndexedProgram; // multi-material indexed
LLGLSLShader            gDeferredSkinnedShadowGLTFAlphaMaskIndexedProgram;
LLGLSLShader            gDeferredSkinnedShadowGLTFAlphaMaskProgram;
LLGLSLShader            gDeferredShadowMaterialIndexedProgram; // multi-material indexed legacy mask shadow
LLGLSLShader            gDeferredSkinnedShadowMaterialIndexedProgram;
LLGLSLShader            gDeferredShadowGLTFAlphaBlendProgram;
LLGLSLShader            gDeferredSkinnedShadowGLTFAlphaBlendProgram;
LLGLSLShader            gDeferredShadowFullbrightAlphaMaskProgram;
LLGLSLShader            gDeferredSkinnedShadowFullbrightAlphaMaskProgram;
LLGLSLShader            gDeferredAvatarShadowProgram;
LLGLSLShader            gDeferredAvatarAlphaShadowProgram;
LLGLSLShader            gDeferredAvatarAlphaMaskShadowProgram;
LLGLSLShader            gDeferredAlphaProgram;
LLGLSLShader            gHUDAlphaProgram;
LLGLSLShader            gDeferredSkinnedAlphaProgram;
LLGLSLShader            gDeferredAlphaImpostorProgram;
LLGLSLShader            gDeferredSkinnedAlphaImpostorProgram;
LLGLSLShader            gDeferredAvatarEyesProgram;
LLGLSLShader            gDeferredFullbrightProgram;
LLGLSLShader            gHUDFullbrightProgram;
LLGLSLShader            gDeferredFullbrightAlphaMaskProgram;
LLGLSLShader            gHUDFullbrightAlphaMaskProgram;
LLGLSLShader            gDeferredFullbrightAlphaMaskAlphaProgram;
LLGLSLShader            gHUDFullbrightAlphaMaskAlphaProgram;
LLGLSLShader            gDeferredEmissiveProgram;
LLGLSLShader            gDeferredSkinnedEmissiveProgram;
LLGLSLShader            gDeferredEmissiveIndexedProgram; // multi-material indexed legacy glow
LLGLSLShader            gDeferredSkinnedEmissiveIndexedProgram;
LLGLSLShader            gActorFxGlowProgram;
LLGLSLShader            gActorFxSkinnedGlowProgram;
LLGLSLShader            gActorFxPBRGlowProgram;
LLGLSLShader            gActorFxPBRSkinnedGlowProgram;
LLGLSLShader            gDeferredPostProgram;
LLGLSLShader            gDeferredPostProgramNoNear;
LLGLSLShader            gDeferredCoFProgram;
LLGLSLShader            gDeferredDoFCombineProgram;
LLGLSLShader            gExposureProgram;
LLGLSLShader            gExposureProgramNoFade;
LLGLSLShader            gLuminanceProgram;
LLGLSLShader            gFXAAProgram[4];
LLGLSLShader            gSMAAEdgeDetectProgram[4];
LLGLSLShader            gSMAABlendWeightsProgram[4];
LLGLSLShader            gSMAANeighborhoodBlendProgram[4];
LLGLSLShader            gCASProgram;
LLGLSLShader            gCineFisheyeProgram;
// [Ultimate Diopter] pass 1 gather (quality tap tiers) + pass 2 composite
LLGLSLShader            gUltimateDiopterGatherProgram[4];
LLGLSLShader            gUltimateDiopterProgram;
// [Ultimate Kaleidoscope] tool mode 1 of the Ultimate Diopter post pass
LLGLSLShader            gUltimateKaleidoProgram;
// [BDMerge G3.2] volumetric lighting (donor: Black Dragon)
LLGLSLShader            gVolumetricLightProgram;
// [Cine Outline Phase 1] deferred normal/depth outline post pass
LLGLSLShader            gCineOutlineProgram;
// [BDMerge G3.3] per-projector volumetric light cones (visible spotlight shafts)
LLGLSLShader            gDeferredProjectorVolumetricProgram;
LLGLSLShader            gDeferredProjectorVolumetricUpsampleProgram; // [BDMerge G3.3 P1 item 3]
LLGLSLShader            gDeferredProjectorVolumetricTemporalProgram; // [BDMerge G3.3 Batch 1 A]
LLGLSLShader            gDeferredProjectorVolumetricBloomFeedProgram; // [BDMerge G3.3 P3 item 4]
LLGLSLShader            gDeferredWeatherRainProgram;
LLGLSLShader            gDeferredWeatherRainOcclusionProgram;
LLGLSLShader            gDeferredWeatherRainUpsampleProgram;
LLGLSLShader            gDeferredWeatherSurfaceProgram;
LLGLSLShader            gDeferredWeatherSurfaceOcclusionProgram;
LLGLSLShader            gDeferredWeatherLightningProgram;
LLGLSLShader            gDeferredWeatherLightningQualityProgram;
LLGLSLShader            gDeferredPostNoDoFProgram;
LLGLSLShader            gDeferredWLSkyProgram;
LLGLSLShader            gEnvironmentMapProgram;
LLGLSLShader            gDeferredWLCloudProgram;
LLGLSLShader            gDeferredWLSunProgram;
LLGLSLShader            gDeferredWLMoonProgram;
LLGLSLShader            gDeferredStarProgram;
LLGLSLShader            gDeferredMeteorProgram;
LLGLSLShader            gDeferredAuroraProgram;
LLGLSLShader            gDeferredFullbrightShinyProgram;
LLGLSLShader            gHUDFullbrightShinyProgram;
LLGLSLShader            gDeferredSkinnedFullbrightShinyProgram;
LLGLSLShader            gDeferredSkinnedFullbrightProgram;
LLGLSLShader            gDeferredSkinnedFullbrightAlphaMaskProgram;
LLGLSLShader            gDeferredSkinnedFullbrightAlphaMaskAlphaProgram;
LLGLSLShader            gNormalMapGenProgram;
LLGLSLShader            gDeferredGenBrdfLutProgram;
LLGLSLShader            gDeferredBufferVisualProgram;
// [BDMerge A5.4-1a] velocity / motion-vector pass programs (rigid + camera). The
// skinned/rigged and avatar variants are Phase 1b.
LLGLSLShader            gVelocityProgram;
LLGLSLShader            gVelocityAlphaProgram;
LLGLSLShader            gVelocityPBRAlphaProgram;
LLGLSLShader            gVelocityAlphaIndexedProgram;
LLGLSLShader            gVelocityPBRAlphaIndexedProgram;
LLGLSLShader            gVelocityDebugProgram;
LLGLSLShader            gVelocityCameraProgram; // [BDMerge A5.4-1c] camera fallback
LLGLSLShader            gVelocitySkinnedProgram;        // [BDMerge A5.4-1b]
LLGLSLShader            gVelocityAlphaSkinnedProgram;   // [BDMerge A5.4-1b]
LLGLSLShader            gVelocityPBRAlphaSkinnedProgram;
LLGLSLShader            gVelocityAlphaIndexedSkinnedProgram;
LLGLSLShader            gVelocityPBRAlphaIndexedSkinnedProgram;
LLGLSLShader            gAvatarVelocityProgram;         // [BDMerge A5.4-1b] classic avatar
LLGLSLShader            gDeferredMotionBlurProgram;     // [BDMerge A5.4-3]
// [BDMerge Froxel F0] hybrid froxel volumetrics: P1 media pass + debug visualizer.
LLGLSLShader            gFroxelMediaProgram;
LLGLSLShader            gFroxelDebugProgram;
// [BDMerge Froxel F1] P4 integrate + P5 apply passes.
LLGLSLShader            gFroxelIntegrateProgram;
LLGLSLShader            gFroxelApplyProgram;
// [BDMerge Froxel F2] P2 per-light injection pass.
LLGLSLShader            gFroxelInjectProgram;
// [BDMerge Froxel F3] P3 froxel-space temporal resolve pass.
LLGLSLShader            gFroxelTemporalProgram;
LLGLSLShader            gBlitWithEffectsProgram;
LLGLSLShader            gCGGammaProgram;
LLGLSLShader            gCGLegacyGammaProgram;
LLGLSLShader            gCGTonemapProgram;
LLGLSLShader            gCGTonemapLegacyGammaProgram;
LLGLSLShader            gCGColorgradeGammaProgram;
LLGLSLShader            gCGColorgradeLegacyGammaProgram;
LLGLSLShader            gCGTonemapColorgradeProgram;
LLGLSLShader            gCGTonemapColorgradeLegacyGammaProgram;
LLGLSLShader            gOnLensFiltersProgram; // ND + polarizer pre-pass (pre-bloom)
// [RLVa:KB] - @setsphere
LLGLSLShader            gRlvSphereProgram;
// [/RLVa:KB]

// Deferred materials shaders
LLGLSLShader            gDeferredMaterialProgram[LLMaterial::SHADER_COUNT*2];
LLGLSLShader            gDeferredMaterialIndexedProgram[LLMaterial::SHADER_COUNT*2]; // multi-material indexed (GBuffer masks only)
LLGLSLShader            gHUDPBROpaqueProgram;
LLGLSLShader            gPBRGlowProgram;
LLGLSLShader            gPBRGlowSkinnedProgram;
LLGLSLShader            gPBRGlowIndexedProgram; // multi-material indexed PBR glow
LLGLSLShader            gPBRGlowSkinnedIndexedProgram;
LLGLSLShader            gDeferredPBROpaqueProgram;
LLGLSLShader            gDeferredPBROpaqueIndexedProgram;
LLGLSLShader            gDeferredSkinnedPBROpaqueIndexedProgram;
LLGLSLShader            gDeferredSkinnedPBROpaqueProgram;
LLGLSLShader            gHUDPBRAlphaProgram;
LLGLSLShader            gDeferredPBRAlphaProgram;
LLGLSLShader            gDeferredSkinnedPBRAlphaProgram;
LLGLSLShader            gSharedActorFxPBRProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
LLGLSLShader            gSharedActorFxSkinnedPBRProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
LLGLSLShader            gSharedActorFxPBRSlotProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
LLGLSLShader            gSharedActorFxSkinnedPBRSlotProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
LLGLSLShader            gSharedActorFxPBRGlowProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
LLGLSLShader            gSharedActorFxSkinnedPBRGlowProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
LLGLSLShader            gSharedActorFxPBRGlowSlotProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
LLGLSLShader            gSharedActorFxSkinnedPBRGlowSlotProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
LLGLSLShader            gSharedActorFxPBRSyntheticGlowProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
LLGLSLShader            gSharedActorFxSkinnedPBRSyntheticGlowProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
LLGLSLShader            gSharedActorFxPBRSyntheticGlowSlotProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
LLGLSLShader            gSharedActorFxSkinnedPBRSyntheticGlowSlotProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
LLGLSLShader            gSharedActorFxPBRDepthProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
LLGLSLShader            gSharedActorFxSkinnedPBRDepthProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
LLGLSLShader            gSharedActorFxPBRDepthSlotProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
LLGLSLShader            gSharedActorFxSkinnedPBRDepthSlotProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
LLGLSLShader            gDeferredPBRTerrainProgram[TERRAIN_PAINT_TYPE_COUNT];

//helper for making a rigged variant of a given shader
static bool make_rigged_variant(LLGLSLShader& shader, LLGLSLShader& riggedShader)
{
    riggedShader.mName = llformat("Skinned %s", shader.mName.c_str());
    riggedShader.mFeatures = shader.mFeatures;
    riggedShader.mFeatures.hasObjectSkinning = true;
    riggedShader.mDefines = shader.mDefines;    // NOTE: Must come before addPermutation

    riggedShader.addPermutation("HAS_SKIN", "1");
    riggedShader.mShaderFiles = shader.mShaderFiles;
    riggedShader.mShaderLevel = shader.mShaderLevel;
    riggedShader.mShaderGroup = shader.mShaderGroup;

    shader.mRiggedVariant = &riggedShader;
    return riggedShader.createShader();
}

// The shared replay is an optional cinematic accelerator.  Publish it only as
// one complete family: partial availability could suppress a native component
// before its matching alpha-mode or glow command exists.
static bool shared_actor_fx_pbr_family_loaded()
{
    if (!LLViewerShaderMgr::hasPrimaryActorFxModules())
    {
        return false;
    }

    for (U32 mode = 0; mode < SHARED_ACTOR_FX_PBR_ALPHA_COUNT; ++mode)
    {
        if (!gSharedActorFxPBRProgram[mode].mProgramObject ||
            !gSharedActorFxSkinnedPBRProgram[mode].mProgramObject ||
            !gSharedActorFxPBRSlotProgram[mode].mProgramObject ||
            !gSharedActorFxSkinnedPBRSlotProgram[mode].mProgramObject ||
            !gSharedActorFxPBRGlowProgram[mode].mProgramObject ||
            !gSharedActorFxSkinnedPBRGlowProgram[mode].mProgramObject ||
            !gSharedActorFxPBRGlowSlotProgram[mode].mProgramObject ||
            !gSharedActorFxSkinnedPBRGlowSlotProgram[mode].mProgramObject ||
            !gSharedActorFxPBRSyntheticGlowProgram[mode].mProgramObject ||
            !gSharedActorFxSkinnedPBRSyntheticGlowProgram[mode].mProgramObject ||
            !gSharedActorFxPBRSyntheticGlowSlotProgram[mode].mProgramObject ||
            !gSharedActorFxSkinnedPBRSyntheticGlowSlotProgram[mode].mProgramObject)
        {
            return false;
        }
        if (mode != SHARED_ACTOR_FX_PBR_ALPHA_BLEND
            && (!gSharedActorFxPBRDepthProgram[mode].mProgramObject
                || !gSharedActorFxSkinnedPBRDepthProgram[mode].mProgramObject
                || !gSharedActorFxPBRDepthSlotProgram[mode].mProgramObject
                || !gSharedActorFxSkinnedPBRDepthSlotProgram[mode].mProgramObject))
        {
            return false;
        }
    }
    return true;
}

static void unload_shared_actor_fx_pbr_family()
{
    for (U32 mode = 0; mode < SHARED_ACTOR_FX_PBR_ALPHA_COUNT; ++mode)
    {
        gSharedActorFxPBRProgram[mode].unload();
        gSharedActorFxSkinnedPBRProgram[mode].unload();
        gSharedActorFxPBRSlotProgram[mode].unload();
        gSharedActorFxSkinnedPBRSlotProgram[mode].unload();
        gSharedActorFxPBRGlowProgram[mode].unload();
        gSharedActorFxSkinnedPBRGlowProgram[mode].unload();
        gSharedActorFxPBRGlowSlotProgram[mode].unload();
        gSharedActorFxSkinnedPBRGlowSlotProgram[mode].unload();
        gSharedActorFxPBRSyntheticGlowProgram[mode].unload();
        gSharedActorFxSkinnedPBRSyntheticGlowProgram[mode].unload();
        gSharedActorFxPBRSyntheticGlowSlotProgram[mode].unload();
        gSharedActorFxSkinnedPBRSyntheticGlowSlotProgram[mode].unload();
        gSharedActorFxPBRDepthProgram[mode].unload();
        gSharedActorFxSkinnedPBRDepthProgram[mode].unload();
        gSharedActorFxPBRDepthSlotProgram[mode].unload();
        gSharedActorFxSkinnedPBRDepthSlotProgram[mode].unload();
    }
}

static void init_shared_actor_fx_pbr_replay_uniforms(LLGLSLShader& shader)
{
    static const LLStaticHashedString opacity("sharedActorFxOpacity");
    static const LLStaticHashedString synthetic_enabled(
        "sharedActorFxSyntheticEnabled");

    // Safe program defaults. The authored-glow program can add synthetic bloom
    // in the same indexed draw, but only when dispatch explicitly enables it.
    shader.bind();
    shader.uniform1f(opacity, 1.f);
    shader.uniform1i(synthetic_enabled, 0);
    shader.unbind();
}

// static
bool LLViewerShaderMgr::hasPrimaryActorFxModules()
{
    if (!sInstance)
    {
        return false;
    }

    const LLViewerShaderMgr* shader_mgr =
        static_cast<const LLViewerShaderMgr*>(sInstance);
    return shader_mgr->mFragmentShaderObjects.count(
               "alchemy/actorFxDissolveF.glsl") != 0
        && shader_mgr->mFragmentShaderObjects.count(
               "alchemy/actorFxF.glsl") != 0;
}

// static
bool LLViewerShaderMgr::hasSharedActorFxPBRShaders()
{
    return shared_actor_fx_pbr_family_loaded();
}

// One reader for the sidecar gate. Programs that are permuted OUTSIDE
// add_common_permutations() (water, underwater) use this so the setting is read
// identically everywhere.
static bool sl_visible_diffuse_sidecar()
{
    static LLCachedControl<bool> sl_sidecar(gSavedSettings, "RenderVisibleDiffuseSidecar", false);
    return sl_sidecar;
}

static void add_common_permutations(LLGLSLShader* shader)
{
    static LLCachedControl<bool> emissive(gSavedSettings, "RenderEnableEmissiveBuffer", false);
    static LLCachedControl<bool> visible_diffuse(gSavedSettings, "RenderVisibleDiffuseSidecar", false);

    if (emissive)
    {
        shader->addPermutation("HAS_EMISSIVE", "1");
    }
    if (visible_diffuse)
    {
        shader->addPermutation("HAS_VISIBLE_DIFFUSE", "1");
    }
}

// Map an indexed GLTF PBR program's per-slot samplers to texture units. Slot s
// uses base color unit s; when full (the GBuffer-write shaders, not the shadow
// alpha-mask shader) it also uses normal N+s, ORM 2N+s and emissive 3N+s. Inactive
// samplers resolve to -1 and are skipped by uniform1i. Safe to call on a program's
// rigged variant too.
static void setup_gltf_indexed_samplers(LLGLSLShader& shader, S32 n, bool full)
{
    shader.bind();
    for (S32 s = 0; s < n; ++s)
    {
        shader.uniform1i(LLStaticHashedString(llformat("basecolor%d", s)), s);
        if (full)
        {
            shader.uniform1i(LLStaticHashedString(llformat("normalmap%d", s)), n + s);
            shader.uniform1i(LLStaticHashedString(llformat("ormmap%d", s)), 2 * n + s);
            shader.uniform1i(LLStaticHashedString(llformat("emissivemap%d", s)), 3 * n + s);
        }
    }
    shader.unbind();
}

// Map an indexed legacy material program's per-slot samplers to texture units:
// diffuse slot s -> unit s; normal s -> N+s (HAS_NORMAL_MAP); spec s -> 2N+s
// (HAS_SPECULAR_MAP). Inactive samplers resolve to -1 and are skipped.
static void setup_material_indexed_samplers(LLGLSLShader& shader, S32 n, bool has_normal, bool has_spec)
{
    shader.bind();
    for (S32 s = 0; s < n; ++s)
    {
        shader.uniform1i(LLStaticHashedString(llformat("diffuse%d", s)), s);
        if (has_normal)
        {
            shader.uniform1i(LLStaticHashedString(llformat("bump%d", s)), n + s);
        }
        if (has_spec)
        {
            shader.uniform1i(LLStaticHashedString(llformat("spec%d", s)), 2 * n + s);
        }
    }
    shader.unbind();
}

// Indexed GLTF geometry is formed before rendering and cannot be safely drawn
// through scalar programs one material at a time. If any required indexed PBR
// beauty/glow/shadow program fails, disable indexed material formation before
// the pipeline rebuilds geometry and consistently use the scalar path.
static void disable_indexed_gltf_batching()
{
    gDeferredPBROpaqueIndexedProgram.unload();
    gDeferredSkinnedPBROpaqueIndexedProgram.unload();
    gPBRGlowIndexedProgram.unload();
    gPBRGlowSkinnedIndexedProgram.unload();
    gDeferredShadowGLTFAlphaMaskIndexedProgram.unload();
    gDeferredSkinnedShadowGLTFAlphaMaskIndexedProgram.unload();

    // Legacy indexed materials share the GLTF channel count and must obey the
    // same invariant once that count drops to zero.
    for (U32 i = 0; i < LLMaterial::SHADER_COUNT * 2; ++i)
    {
        gDeferredMaterialIndexedProgram[i].unload();
    }
    gDeferredEmissiveIndexedProgram.unload();
    gDeferredSkinnedEmissiveIndexedProgram.unload();
    gDeferredShadowMaterialIndexedProgram.unload();
    gDeferredSkinnedShadowMaterialIndexedProgram.unload();

    LLGLSLShader::sIndexedLegacyMaterials = false;
    LLGLSLShader::sIndexedGLTFChannels = 0;
    LLGLSLShader::sSharedPBRIndexedGLTFChannels = 0;
}

#ifdef SHOW_ASSERT
// return true if there are no redundant shaders in the given vector
// also checks for redundant variants
static bool no_redundant_shaders(const std::vector<LLGLSLShader*>& shaders)
{
    std::set<std::string> names;
    for (LLGLSLShader* shader : shaders)
    {
        if (names.find(shader->mName) != names.end())
        {
            LL_WARNS("Shader") << "Redundant shader: " << shader->mName << LL_ENDL;
            return false;
        }
        names.insert(shader->mName);

        if (shader->mRiggedVariant)
        {
            if (names.find(shader->mRiggedVariant->mName) != names.end())
            {
                LL_WARNS("Shader") << "Redundant shader: " << shader->mRiggedVariant->mName << LL_ENDL;
                return false;
            }
            names.insert(shader->mRiggedVariant->mName);
        }
    }
    return true;
}
#endif


LLViewerShaderMgr::LLViewerShaderMgr() :
    mShaderLevel(SHADER_COUNT, 0),
    mMaxAvatarShaderLevel(0)
{
}

LLViewerShaderMgr::~LLViewerShaderMgr()
{
    mShaderLevel.clear();
    mShaderList.clear();
}

void LLViewerShaderMgr::finalizeShaderList()
{
    //ONLY shaders that need WL Param management should be added here
    mShaderList.push_back(&gAvatarProgram);
    mShaderList.push_back(&gWaterProgram);
    mShaderList.push_back(&gAvatarEyeballProgram);
    // Actorghost world programs are optional/fail-open. Never publish an
    // unloaded optional program to the environment-uniform list (debug builds
    // assert that every entry is linked).
    if (gAvatarActorGhostProgram.mProgramObject
        && gAvatarEyeballActorGhostProgram.mProgramObject)
    {
        mShaderList.push_back(&gAvatarActorGhostProgram);
        mShaderList.push_back(&gAvatarEyeballActorGhostProgram);
    }
    if (gWorldActorGhostProgram.mProgramObject)
    {
        mShaderList.push_back(&gWorldActorGhostProgram);
    }
    if (gWorldActorGhostIndexedProgram.mProgramObject)
    {
        mShaderList.push_back(&gWorldActorGhostIndexedProgram);
    }
    mShaderList.push_back(&gImpostorProgram);
    mShaderList.push_back(&gObjectBumpProgram);
    mShaderList.push_back(&gObjectFullbrightAlphaMaskProgram);
    mShaderList.push_back(&gObjectAlphaMaskNoColorProgram);
    mShaderList.push_back(&gUnderWaterProgram);
    mShaderList.push_back(&gDeferredSunProgram);
    mShaderList.push_back(&gDeferredSunProbeProgram);
    mShaderList.push_back(&gHazeProgram);
    mShaderList.push_back(&gHazeWaterProgram);
    mShaderList.push_back(&gDeferredSoftenProgram);
    mShaderList.push_back(&gPrismLensProgram);
    // [BDMerge G3.2] atmospherics uniforms (blue_density etc.) are pushed to
    // registered shaders only - without this the volumetric shader computes
    // haze_density/(blue_density+haze_density) = 0/0 = NaN and blacks the frame
    mShaderList.push_back(&gVolumetricLightProgram);
    mShaderList.push_back(&gCineOutlineProgram);
    mShaderList.push_back(&gCineFisheyeProgram);
    // [Ultimate Diopter]
    for (U32 i = 0; i < 4; ++i)
    {
        mShaderList.push_back(&gUltimateDiopterGatherProgram[i]);
    }
    mShaderList.push_back(&gUltimateDiopterProgram);
    // [Ultimate Kaleidoscope]
    mShaderList.push_back(&gUltimateKaleidoProgram);
    // [BDMerge G3.3] projector volumetrics links the same atmospherics/deferred
    // util set as G3.2, so register it too (keeps linked util externs satisfied).
    mShaderList.push_back(&gDeferredProjectorVolumetricProgram);
    mShaderList.push_back(&gDeferredProjectorVolumetricUpsampleProgram); // [BDMerge G3.3 P1 item 3]
    mShaderList.push_back(&gDeferredProjectorVolumetricTemporalProgram); // [BDMerge G3.3 Batch 1 A]
    mShaderList.push_back(&gDeferredProjectorVolumetricBloomFeedProgram); // [BDMerge G3.3 P3 item 4]
    mShaderList.push_back(&gDeferredWeatherRainProgram);
    if (gSavedSettings.getBOOL("AlchemyWeatherRainOcclusion"))
    {
        mShaderList.push_back(&gDeferredWeatherRainOcclusionProgram);
        mShaderList.push_back(&gDeferredWeatherSurfaceOcclusionProgram);
    }
    mShaderList.push_back(&gDeferredWeatherRainUpsampleProgram);
    mShaderList.push_back(&gDeferredWeatherSurfaceProgram);
    mShaderList.push_back(&gDeferredWeatherLightningProgram);
    if (gSavedSettings.getBOOL(
            "AlchemyWeatherLightningQualityEnabled") &&
        gSavedSettings.getBOOL("RenderHDREnabled") &&
        gGLManager.mGLVersion > 4.05f)
    {
        // Quality EEP parameters are uploaded explicitly by renderWeather().
        // Keep it out of ongoing environment propagation while it is disabled.
        mShaderList.push_back(&gDeferredWeatherLightningQualityProgram);
    }
    mShaderList.push_back(&gFroxelMediaProgram); // [BDMerge Froxel F0]
    mShaderList.push_back(&gFroxelDebugProgram); // [BDMerge Froxel F0]
    mShaderList.push_back(&gFroxelIntegrateProgram); // [BDMerge Froxel F1]
    mShaderList.push_back(&gFroxelApplyProgram); // [BDMerge Froxel F1]
    mShaderList.push_back(&gFroxelInjectProgram); // [BDMerge Froxel F2]
    mShaderList.push_back(&gFroxelTemporalProgram); // [BDMerge Froxel F3]
    mShaderList.push_back(&gDeferredAlphaProgram);
    mShaderList.push_back(&gHUDAlphaProgram);
    mShaderList.push_back(&gDeferredAlphaImpostorProgram);
    mShaderList.push_back(&gDeferredFullbrightProgram);
    mShaderList.push_back(&gHUDFullbrightProgram);
    mShaderList.push_back(&gDeferredFullbrightAlphaMaskProgram);
    mShaderList.push_back(&gHUDFullbrightAlphaMaskProgram);
    mShaderList.push_back(&gDeferredFullbrightAlphaMaskAlphaProgram);
    mShaderList.push_back(&gHUDFullbrightAlphaMaskAlphaProgram);
    mShaderList.push_back(&gDeferredFullbrightShinyProgram);
    mShaderList.push_back(&gHUDFullbrightShinyProgram);
    mShaderList.push_back(&gDeferredEmissiveProgram);
    mShaderList.push_back(&gActorFxGlowProgram);
    mShaderList.push_back(&gActorFxPBRGlowProgram);
    mShaderList.push_back(&gDeferredAvatarEyesProgram);
    mShaderList.push_back(&gDeferredAvatarAlphaProgram);
    mShaderList.push_back(&gEnvironmentMapProgram);
    mShaderList.push_back(&gDeferredWLSkyProgram);
    mShaderList.push_back(&gDeferredWLCloudProgram);
    mShaderList.push_back(&gDeferredWLMoonProgram);
    mShaderList.push_back(&gDeferredWLSunProgram);
// [RLVa:KB] - @setsphere
    mShaderList.push_back(&gRlvSphereProgram);
// [/RLVa:KB]
    mShaderList.push_back(&gDeferredPBRAlphaProgram);
    mShaderList.push_back(&gHUDPBRAlphaProgram);
    // Optional shared Actor FX PBR programs must never enter environment
    // propagation partially loaded: debug builds assert every registered base
    // and rigged variant is linked.  The renderer therefore also gets a simple
    // all-or-nothing readiness signal from this same family invariant.
    if (hasSharedActorFxPBRShaders())
    {
        for (U32 mode = 0; mode < SHARED_ACTOR_FX_PBR_ALPHA_COUNT; ++mode)
        {
            mShaderList.push_back(&gSharedActorFxPBRProgram[mode]);
            mShaderList.push_back(&gSharedActorFxPBRSlotProgram[mode]);
            mShaderList.push_back(&gSharedActorFxPBRGlowProgram[mode]);
            mShaderList.push_back(&gSharedActorFxPBRGlowSlotProgram[mode]);
            mShaderList.push_back(
                &gSharedActorFxPBRSyntheticGlowProgram[mode]);
            mShaderList.push_back(
                &gSharedActorFxPBRSyntheticGlowSlotProgram[mode]);
            if (mode != SHARED_ACTOR_FX_PBR_ALPHA_BLEND)
            {
                mShaderList.push_back(&gSharedActorFxPBRDepthProgram[mode]);
                mShaderList.push_back(&gSharedActorFxPBRDepthSlotProgram[mode]);
            }
        }
    }
    mShaderList.push_back(&gDeferredDiffuseProgram);
    mShaderList.push_back(&gDeferredBumpProgram);
    mShaderList.push_back(&gDeferredPBROpaqueProgram);

    mShaderList.push_back(&gDeferredAvatarProgram);
    mShaderList.push_back(&gDeferredTerrainProgram);

    for (U32 paint_type = 0; paint_type < TERRAIN_PAINT_TYPE_COUNT; ++paint_type)
    {
        mShaderList.push_back(&gDeferredPBRTerrainProgram[paint_type]);
    }

    mShaderList.push_back(&gDeferredDiffuseAlphaMaskProgram);
    mShaderList.push_back(&gDeferredNonIndexedDiffuseAlphaMaskProgram);
    mShaderList.push_back(&gDeferredTreeProgram);

    mShaderList.push_back(&gCGGammaProgram);
    mShaderList.push_back(&gCGLegacyGammaProgram);
    mShaderList.push_back(&gCGColorgradeGammaProgram);
    mShaderList.push_back(&gCGColorgradeLegacyGammaProgram);
    mShaderList.push_back(&gCGTonemapProgram);
    mShaderList.push_back(&gCGTonemapLegacyGammaProgram);
    mShaderList.push_back(&gCGTonemapColorgradeProgram);
    mShaderList.push_back(&gCGTonemapColorgradeLegacyGammaProgram);
    mShaderList.push_back(&gOnLensFiltersProgram);

    // make sure there are no redundancies
    llassert(no_redundant_shaders(mShaderList));
}

// static
LLViewerShaderMgr * LLViewerShaderMgr::instance()
{
    if(NULL == sInstance)
    {
        sInstance = new LLViewerShaderMgr();
    }

    return static_cast<LLViewerShaderMgr*>(sInstance);
}

// static
void LLViewerShaderMgr::releaseInstance()
{
    if (sInstance != NULL)
    {
        delete sInstance;
        sInstance = NULL;
    }
}

void LLViewerShaderMgr::initAttribsAndUniforms(void)
{
    if (mReservedAttribs.empty())
    {
        LLShaderMgr::initAttribsAndUniforms();
    }
}


//============================================================================
// Set Levels

S32 LLViewerShaderMgr::getShaderLevel(S32 type)
{
    return mShaderLevel[type];
}

//============================================================================
// Shader Management

void LLViewerShaderMgr::setShaders()
{
    LL_PROFILE_ZONE_SCOPED;
    //setShaders might be called redundantly by gSavedSettings, so return on reentrance
    static bool reentrance = false;

    if (!gPipeline.mInitialized || !sInitialized || reentrance || sSkipReload)
    {
        return;
    }

    mShaderList.clear();

    if (!gGLManager.mHasRequirements)
    {
        // Viewer will show 'hardware requirements' warning later
        LL_INFOS("ShaderLoading") << "Not supported hardware/software" << LL_ENDL;
        return;
    }

    {
        static LLCachedControl<bool> shader_cache_enabled(gSavedSettings, "RenderShaderCacheEnabled", true);
        static LLUUID old_cache_version;
        static LLUUID current_cache_version;
        if (current_cache_version.isNull())
        {
            HBXXH128 hash_obj;
            hash_obj.update(LLVersionInfo::instance().getVersion());

            // Fold a fingerprint of the on-disk shader SOURCE into the cache key
            // so ANY shader edit invalidates the GL program-binary cache on next
            // launch — automatically, independent of the viewer version/build
            // number. Without this, a shader-only change (or two builds that share
            // a version) leaves stale cached binaries that fail to link and crash
            // the deferred pipeline. We hash each shader file's relative path plus
            // its full CONTENTS (the tree is ~1.6 MB, hashed once per process) in
            // SORTED path order: content hashing also catches edits that preserve
            // size+mtime (reproducible builds, coarse-resolution filesystems), and
            // sorting makes the key independent of the unspecified iterator order.
            // On ANY scan error we fold in NOTHING and fall back to the exact
            // version-only key (never a partial/unstable fingerprint).
            try
            {
                namespace fs = std::filesystem;
                // fsyspath: correct UTF-8 -> native path conversion on Windows.
                fs::path shader_root = fsyspath(getShaderDirPrefix()).parent_path(); // .../shaders
                std::error_code root_ec;
                if (!shader_root.empty() && fs::exists(shader_root, root_ec) && !root_ec)
                {
                    bool scan_ok = true;
                    std::vector<fs::path> files;
                    std::error_code it_ec;
                    // NOTE: default directory_options (NOT skip_permission_denied) so an
                    // inaccessible subtree surfaces as it_ec and forces the version-only
                    // fallback rather than silently folding a PARTIAL fingerprint (which
                    // would let edits in the omitted files reuse stale binaries).
                    for (fs::recursive_directory_iterator it(shader_root, it_ec), end;
                         !it_ec && it != end;
                         it.increment(it_ec))
                    {
                        if (it_ec) { scan_ok = false; break; }
                        std::error_code f_ec;
                        bool is_file = it->is_regular_file(f_ec);
                        if (f_ec) { scan_ok = false; break; }
                        if (is_file) files.push_back(it->path());
                    }
                    // An increment that both errors AND reaches end exits the loop
                    // without the in-body check, and a failed constructor sets it_ec
                    // too — catch both here so a partial scan never folds.
                    if (it_ec) scan_ok = false;

                    if (scan_ok)
                    {
                        std::sort(files.begin(), files.end()); // deterministic order
                        // Separate accumulator: a mid-scan failure folds in nothing.
                        HBXXH128 fp;
                        for (const fs::path& p : files)
                        {
                            std::error_code r_ec;
                            const std::string rel = fs::relative(p, shader_root, r_ec).generic_string();
                            if (r_ec) { scan_ok = false; break; }
                            std::ifstream fin(p, std::ios::binary);
                            if (!fin.good()) { scan_ok = false; break; }
                            fp.update(rel);
                            fp.update(fin); // hash full file contents
                            if (fin.bad()) { scan_ok = false; break; }
                        }
                        if (scan_ok)
                        {
                            hash_obj.update(fp.digest().asString());
                        }
                    }

                    if (!scan_ok)
                    {
                        LL_WARNS("ShaderLoading") << "Shader source fingerprint incomplete; using version-only cache key" << LL_ENDL;
                    }
                }
            }
            catch (...)
            {
                LL_WARNS("ShaderLoading") << "Shader source fingerprint failed; using version-only cache key" << LL_ENDL;
            }

            current_cache_version = hash_obj.digest();

            old_cache_version = LLUUID(gSavedSettings.getString("RenderShaderCacheVersion"));
            gSavedSettings.setString("RenderShaderCacheVersion", current_cache_version.asString());
        }

        initShaderCache(
            shader_cache_enabled,
            old_cache_version,
            current_cache_version,
            LLAppViewer::instance()->isSecondInstance());
    }

    static LLCachedControl<U32> max_texture_index(gSavedSettings, "RenderMaxTextureIndex", 16);

    // when using indexed texture rendering, leave some texture units available for shadow and reflection maps
    // We assume we always have atleast 16 texunits available, but we clamp the reserved units to ensure we don't end up with a negative
    // number of texture channels
    static LLCachedControl<S32> reserved_texture_units(gSavedSettings, "RenderReservedTextureIndices", 12);

    LLGLSLShader::sIndexedTextureChannels = llmax(4, gGLManager.mNumTextureImageUnits - reserved_texture_units);

    // Native indexed GLTF PBR uses only four material maps per slot in its
    // GBuffer pass, so retain its full batch width. Shared Actor FX replays
    // rigged indexed PBR through a forward-lighting shader and therefore needs
    // a separate, narrower stride. At the highest feature level that forward
    // contract has ten simultaneously active auxiliary samplers: BRDF LUT (1),
    // directional shadows (4), reflection + irradiance probes (2), SSR colour
    // + depth (2), and the hero probe (1). Rigged PBR geometry is split at this
    // second limit by genDrawInfo; static/native PBR and indexed legacy material
    // batching keep the native limit. On a 32-unit GPU the shared limit is five
    // materials; on a 16-unit GPU it is one and shared indexed replay naturally
    // falls back to exact scalar commands.
    LLGLSLShader::sIndexedGLTFChannels = llclamp(
        gGLManager.mNumTextureImageUnits / 4, 1, 8);
    constexpr S32 SHARED_FORWARD_PBR_AUX_TEXTURE_UNITS = 10;
    const S32 indexed_gltf_texture_units = llmax(
        gGLManager.mNumTextureImageUnits
            - SHARED_FORWARD_PBR_AUX_TEXTURE_UNITS,
        4);
    LLGLSLShader::sSharedPBRIndexedGLTFChannels = llclamp(
        indexed_gltf_texture_units / 4, 1, 8);

    reentrance = true;

    // Make sure the compiled shader map is cleared before we recompile shaders.
    mVertexShaderObjects.clear();
    mFragmentShaderObjects.clear();

    initAttribsAndUniforms();
    gPipeline.releaseGLBuffers();

    unloadShaders();

    LLPipeline::sRenderGlow = gSavedSettings.getBOOL("RenderGlow");
    LLPipeline::sRenderTransparentWater = gSavedSettings.getBOOL("RenderTransparentWater");

    if (gViewerWindow)
    {
        gViewerWindow->setCursor(UI_CURSOR_WAIT);
    }

    // Shaders
    LL_INFOS("ShaderLoading") << "\n~~~~~~~~~~~~~~~~~~\n Loading Shaders:\n~~~~~~~~~~~~~~~~~~" << LL_ENDL;
    LL_INFOS("ShaderLoading") << llformat("Using GLSL %d.%d", gGLManager.mGLSLVersionMajor, gGLManager.mGLSLVersionMinor) << LL_ENDL;

    for (S32 i = 0; i < SHADER_COUNT; i++)
    {
        mShaderLevel[i] = 0;
    }
    mMaxAvatarShaderLevel = 0;

    LLVertexBuffer::unbind();

    llassert((gGLManager.mGLSLVersionMajor > 1 || gGLManager.mGLSLVersionMinor >= 10));


    S32 light_class = 3;
    S32 interface_class = 2;
    S32 env_class = 2;
    S32 obj_class = 2;
    S32 effect_class = 2;
    S32 wl_class = 2;
    S32 water_class = 3;
    S32 deferred_class = 3;

    // Trigger a full rebuild of the fallback skybox / cubemap if we've toggled windlight shaders
    if (!wl_class || (mShaderLevel[SHADER_WINDLIGHT] != wl_class && gSky.mVOSkyp.notNull()))
    {
        gSky.mVOSkyp->forceSkyUpdate();
    }

    // Load lighting shaders
    mShaderLevel[SHADER_LIGHTING] = light_class;
    mShaderLevel[SHADER_INTERFACE] = interface_class;
    mShaderLevel[SHADER_ENVIRONMENT] = env_class;
    mShaderLevel[SHADER_WATER] = water_class;
    mShaderLevel[SHADER_OBJECT] = obj_class;
    mShaderLevel[SHADER_EFFECT] = effect_class;
    mShaderLevel[SHADER_WINDLIGHT] = wl_class;
    mShaderLevel[SHADER_DEFERRED] = deferred_class;

    std::string shader_name = loadBasicShaders();
    if (shader_name.empty())
    {
        LL_INFOS("Shader") << "Loaded basic shaders." << LL_ENDL;
    }
    else
    {
        // "ShaderLoading" and "Shader" need to be logged
        LL_WARNS("Shader") << "Failed loading basic shaders.  Retrying with increased log level..." << LL_ENDL;

        LLError::ELevel lvl = LLError::getDefaultLevel();
        LLError::setDefaultLevel(LLError::LEVEL_DEBUG);
        loadBasicShaders();
        LLError::setDefaultLevel(lvl);
        gGLManager.printGLInfoString();
        LL_ERRS() << "Unable to load basic shader " << shader_name << ", verify graphics driver installed and current." << LL_ENDL;
        reentrance = false; // For hygiene only, re-try probably helps nothing
        return;
    }

    gPipeline.mShadersLoaded = true;

    bool loaded = loadShadersWater();

    if (loaded)
    {
        LL_INFOS() << "Loaded water shaders." << LL_ENDL;
    }
    else
    {
        LL_WARNS() << "Failed to load water shaders." << LL_ENDL;
        llassert(loaded);
    }

    if (loaded)
    {
        loaded = loadShadersEffects();
        if (loaded)
        {
            LL_INFOS() << "Loaded effects shaders." << LL_ENDL;
        }
        else
        {
            LL_WARNS() << "Failed to load effects shaders." << LL_ENDL;
            llassert(loaded);
        }
    }

    if (loaded)
    {
        loaded = loadShadersInterface();
        if (loaded)
        {
            LL_INFOS() << "Loaded interface shaders." << LL_ENDL;
        }
        else
        {
            LL_WARNS() << "Failed to load interface shaders." << LL_ENDL;
            llassert(loaded);
        }
    }

    if (loaded)
    {
        // Load max avatar shaders to set the max level
        mShaderLevel[SHADER_AVATAR] = 3;
        mMaxAvatarShaderLevel = 3;

        if (loadShadersObject())
        { //hardware skinning is enabled and rigged attachment shaders loaded correctly
            // cloth is a class3 shader
            S32 avatar_class = 1;

            // Set the actual level
            mShaderLevel[SHADER_AVATAR] = avatar_class;

            loaded = loadShadersAvatar();
            llassert(loaded);
        }
        else
        { //hardware skinning not possible, neither is deferred rendering
            llassert(false); // SHOULD NOT BE POSSIBLE
        }
    }

    llassert(loaded);
    loaded = loaded && loadShadersDeferred();
    if (loaded)
    {
        LL_INFOS() << "Loaded deferred shaders." << LL_ENDL;
    }
    else
    {
        LL_WARNS() << "Failed to load deferred shaders." << LL_ENDL;
        llassert(loaded);
    }

    // We only want to persist shader cache metadata if we successfully loaded shaders, otherwise we might be caching failure states
    if (loaded && !LLAppViewer::instance()->isSecondInstance())
    {
        persistShaderCacheMetadata();
    }

    if (gViewerWindow)
    {
        gViewerWindow->setCursor(UI_CURSOR_ARROW);
    }
    gPipeline.createGLBuffers();

    finalizeShaderList();

    reentrance = false;
}

void LLViewerShaderMgr::unloadShaders()
{
    // A completed auxiliary Prism pass can retain shader addresses solely to
    // restore main-view screen uniforms on their next bind. Program unload
    // makes that tracking obsolete; clear it before variant storage changes.
    gPipeline.clearPrismLensDirtyScreenShaderTracking();

    while (!LLGLSLShader::sInstances.empty())
    {
        LLGLSLShader* shader = *(LLGLSLShader::sInstances.begin());
        shader->unload();
    }

    mShaderLevel[SHADER_LIGHTING] = 0;
    mShaderLevel[SHADER_OBJECT] = 0;
    mShaderLevel[SHADER_AVATAR] = 0;
    mShaderLevel[SHADER_ENVIRONMENT] = 0;
    mShaderLevel[SHADER_WATER] = 0;
    mShaderLevel[SHADER_INTERFACE] = 0;
    mShaderLevel[SHADER_EFFECT] = 0;
    mShaderLevel[SHADER_WINDLIGHT] = 0;

    gPipeline.mShadersLoaded = false;
}

std::string LLViewerShaderMgr::loadBasicShaders()
{
    // Load basic dependency shaders first
    // All of these have to load for any shaders to function

    S32 sum_lights_class = 3;

    // Use the feature table to mask out the max light level to use.  Also make sure it's at least 1.
    S32 max_light_class = gSavedSettings.getS32("RenderShaderLightingMaxLevel");
    sum_lights_class = llclamp(sum_lights_class, 1, max_light_class);

    // Load the Basic Vertex Shaders at the appropriate level.
    // (in order of shader function call depth for reference purposes, deepest level first)

    vector< pair<string, S32> > shaders;
    shaders.push_back( make_pair( "windlight/atmosphericsVarsV.glsl",       mShaderLevel[SHADER_WINDLIGHT] ) );
    shaders.push_back( make_pair( "windlight/atmosphericsHelpersV.glsl",    mShaderLevel[SHADER_WINDLIGHT] ) );
    shaders.push_back( make_pair( "lighting/lightFuncV.glsl",               mShaderLevel[SHADER_LIGHTING] ) );
    shaders.push_back( make_pair( "lighting/sumLightsV.glsl",               sum_lights_class ) );
    shaders.push_back( make_pair( "lighting/lightV.glsl",                   mShaderLevel[SHADER_LIGHTING] ) );
    shaders.push_back( make_pair( "lighting/lightFuncSpecularV.glsl",       mShaderLevel[SHADER_LIGHTING] ) );
    shaders.push_back( make_pair( "lighting/sumLightsSpecularV.glsl",       sum_lights_class ) );
    shaders.push_back( make_pair( "lighting/lightSpecularV.glsl",           mShaderLevel[SHADER_LIGHTING] ) );
    shaders.push_back( make_pair( "windlight/atmosphericsFuncs.glsl",       mShaderLevel[SHADER_WINDLIGHT] ) );
    shaders.push_back( make_pair( "windlight/atmosphericsV.glsl",           mShaderLevel[SHADER_WINDLIGHT] ) );
    shaders.push_back( make_pair( "environment/srgbF.glsl",                 1 ) );
    shaders.push_back( make_pair( "avatar/avatarSkinV.glsl",                1 ) );
    shaders.push_back( make_pair( "avatar/objectSkinV.glsl",                1 ) );
    shaders.push_back( make_pair( "deferred/textureUtilV.glsl",             1 ) );
    if (gGLManager.mGLSLVersionMajor >= 2 || gGLManager.mGLSLVersionMinor >= 30)
    {
        shaders.push_back( make_pair( "objects/indexedTextureV.glsl",           1 ) );
    }
    shaders.push_back( make_pair( "objects/nonindexedTextureV.glsl",        1 ) );

    std::map<std::string, std::string> attribs;
    attribs["MAX_JOINTS_PER_MESH_OBJECT"] =
        std::to_string(LLSkinningUtil::getMaxJointCount());

    static LLCachedControl<bool> emissive(gSavedSettings, "RenderEnableEmissiveBuffer", false);

    if (emissive)
    {
        attribs["HAS_EMISSIVE"] = "1";
    }

    bool ssr = gSavedSettings.getBOOL("RenderScreenSpaceReflections");

    bool mirrors = gSavedSettings.getBOOL("RenderMirrors");

    bool has_reflection_probes = gSavedSettings.getBOOL("RenderReflectionsEnabled") && gGLManager.mGLVersion > 3.99f;

    S32 probe_level = llclamp(gSavedSettings.getS32("RenderReflectionProbeLevel"), 0, 3);

    S32 shadow_detail            = gSavedSettings.getS32("RenderShadowDetail");

    if (shadow_detail >= 1)
    {
        attribs["SUN_SHADOW"] = "1";

        if (shadow_detail >= 2)
        {
            attribs["SPOT_SHADOW"] = "1";
        }
    }

    if (ssr)
    {
        attribs["SSR"] = "1";
    }

    if (has_reflection_probes)
    {
        attribs["REFMAP_LEVEL"] = std::to_string(probe_level);
        attribs["REF_SAMPLE_COUNT"] = "32";
    }

    if (mirrors)
    {
        attribs["HERO_PROBES"] = "1";
    }

    { // PBR terrain
        const S32 mapping = clamp_terrain_mapping(gSavedSettings.getS32("RenderTerrainPBRPlanarSampleCount"));
        attribs["TERRAIN_PLANAR_TEXTURE_SAMPLE_COUNT"] = llformat("%d", mapping);
        const F32 triplanar_factor = gSavedSettings.getF32("RenderTerrainPBRTriplanarBlendFactor");
        attribs["TERRAIN_TRIPLANAR_BLEND_FACTOR"] = llformat("%.2f", triplanar_factor);
        S32 detail = gSavedSettings.getS32("RenderTerrainPBRDetail");
        detail = llclamp(detail, TERRAIN_PBR_DETAIL_MIN, TERRAIN_PBR_DETAIL_MAX);
        attribs["TERRAIN_PBR_DETAIL"] = llformat("%d", detail);
    }

    LLGLSLShader::sGlobalDefines = attribs;

    // We no longer have to bind the shaders to global glhandles, they are automatically added to a map now.
    for (U32 i = 0; i < shaders.size(); i++)
    {
        // Note usage of GL_VERTEX_SHADER
        if (loadShaderFile(shaders[i].first, shaders[i].second, GL_VERTEX_SHADER, &attribs) == 0)
        {
            LL_WARNS("Shader") << "Failed to load basic vertex shader " << i << ": " << shaders[i].first << LL_ENDL;
            return shaders[i].first;
        }
    }

    // Load the Basic Fragment Shaders at the appropriate level.
    // (in order of shader function call depth for reference purposes, deepest level first)

    shaders.clear();
    S32 ch = 1;

    if (gGLManager.mGLSLVersionMajor > 1 || gGLManager.mGLSLVersionMinor >= 30)
    { //use indexed texture rendering for GLSL >= 1.30
        ch = llmax(LLGLSLShader::sIndexedTextureChannels, 1);
    }


    std::vector<S32> index_channels;
    index_channels.push_back(-1);    shaders.push_back( make_pair( "windlight/atmosphericsVarsF.glsl",      mShaderLevel[SHADER_WINDLIGHT] ) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "windlight/atmosphericsHelpersF.glsl",       mShaderLevel[SHADER_WINDLIGHT] ) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "windlight/gammaF.glsl",                 mShaderLevel[SHADER_WINDLIGHT]) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "windlight/atmosphericsFuncs.glsl",       mShaderLevel[SHADER_WINDLIGHT] ) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "windlight/atmosphericsF.glsl",          mShaderLevel[SHADER_WINDLIGHT] ) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "environment/waterFogF.glsl",                mShaderLevel[SHADER_WATER] ) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "environment/srgbF.glsl",                    mShaderLevel[SHADER_ENVIRONMENT] ) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "deferred/deferredUtil.glsl",                    1) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "deferred/gbufferUtil.glsl",                    1) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "deferred/globalF.glsl",                          1));
    index_channels.push_back(-1);    shaders.push_back( make_pair( "deferred/shadowUtil.glsl",                      1) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "deferred/aoUtil.glsl",                          1) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "deferred/pbrterrainUtilF.glsl",                 1) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "deferred/tonemapUtilF.glsl",                    1) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "deferred/LPMUtil.glsl",                         1) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "alchemy/colorGradeUtilF.glsl",                 1) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "alchemy/postEffectUtilsF.glsl",                 1) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "alchemy/actorFxDissolveFallbackF.glsl",         1) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "alchemy/actorFxFallbackF.glsl",                 1) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "alchemy/actorFxDissolveF.glsl",                 1) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "alchemy/actorFxF.glsl",                         1) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "deferred/reflectionProbeF.glsl",                has_reflection_probes ? 3 : 2) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "deferred/screenSpaceReflUtil.glsl",             ssr ? 3 : 1) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "lighting/lightNonIndexedF.glsl",                    mShaderLevel[SHADER_LIGHTING] ) );
    index_channels.push_back(-1);    shaders.push_back( make_pair( "lighting/lightAlphaMaskNonIndexedF.glsl",                   mShaderLevel[SHADER_LIGHTING] ) );
    index_channels.push_back(ch);    shaders.push_back( make_pair( "lighting/lightF.glsl",                  mShaderLevel[SHADER_LIGHTING] ) );
    index_channels.push_back(ch);    shaders.push_back( make_pair( "lighting/lightAlphaMaskF.glsl",                 mShaderLevel[SHADER_LIGHTING] ) );

    for (U32 i = 0; i < shaders.size(); i++)
    {
        // Note usage of GL_FRAGMENT_SHADER
        if (loadShaderFile(shaders[i].first, shaders[i].second, GL_FRAGMENT_SHADER, &attribs, index_channels[i]) == 0)
        {
            LL_WARNS("Shader") << "Failed to load fragment shader " << shaders[i].first << LL_ENDL;
            if (shaders[i].first == "alchemy/actorFxDissolveF.glsl" ||
                shaders[i].first == "alchemy/actorFxF.glsl")
            {
                LL_WARNS("Shader") << "Actor FX will use its identity fallback; core shader loading continues"
                                    << LL_ENDL;
                continue;
            }
            return shaders[i].first;
        }
    }

    return std::string();
}

bool LLViewerShaderMgr::loadShadersWater()
{
    LL_PROFILE_ZONE_SCOPED;
    bool success = true;
    bool terrainWaterSuccess = true;

    bool use_sun_shadow = mShaderLevel[SHADER_DEFERRED] > 1 &&
        gSavedSettings.getS32("RenderShadowDetail") > 0;

    if (mShaderLevel[SHADER_WATER] == 0)
    {
        gWaterProgram.unload();
        gUnderWaterProgram.unload();
        return true;
    }

    if (success)
    {
        // load water shader
        gWaterProgram.mName = "Water Shader";
        gWaterProgram.mFeatures.calculatesAtmospherics = true;
        gWaterProgram.mFeatures.hasAtmospherics = true;
        gWaterProgram.mFeatures.hasGamma = true;
        gWaterProgram.mFeatures.hasSrgb = true;
        gWaterProgram.mFeatures.hasReflectionProbes = true;
        gWaterProgram.mFeatures.hasTonemap = true;
        gWaterProgram.mFeatures.hasShadows = use_sun_shadow;
        gWaterProgram.mShaderFiles.clear();
        gWaterProgram.mShaderFiles.push_back(make_pair("environment/waterV.glsl", GL_VERTEX_SHADER));
        gWaterProgram.mShaderFiles.push_back(make_pair("environment/waterF.glsl", GL_FRAGMENT_SHADER));
        gWaterProgram.clearPermutations();
        if (LLPipeline::sRenderTransparentWater)
        {
            gWaterProgram.addPermutation("TRANSPARENT_WATER", "1");
        }

        if (use_sun_shadow)
        {
            gWaterProgram.addPermutation("HAS_SUN_SHADOW", "1");
        }

        // S3: without this the water shader declares only frag_color, so the
        // sidecar attachment kept the seed pass's answer for the SEABED behind
        // the water and published it as the water surface's albedo at K=1.
        // Only the visible-diffuse permutation -- water has no emissive output,
        // so add_common_permutations() would define an unused HAS_EMISSIVE.
        // NOTE: gUnderWaterProgram is permuted in its OWN block below, not here.
        // Its clearPermutations() runs later and would erase anything set now.
        if (sl_visible_diffuse_sidecar())
        {
            gWaterProgram.addPermutation("HAS_VISIBLE_DIFFUSE", "1");
        }

        gWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
        gWaterProgram.mShaderLevel = mShaderLevel[SHADER_WATER];
        success = gWaterProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        //load under water vertex shader
        gUnderWaterProgram.mName = "Underwater Shader";
        gUnderWaterProgram.mFeatures.calculatesAtmospherics = true;
        gUnderWaterProgram.mFeatures.hasAtmospherics = true;
        gUnderWaterProgram.mShaderFiles.clear();
        gUnderWaterProgram.mShaderFiles.push_back(make_pair("environment/waterV.glsl", GL_VERTEX_SHADER));
        gUnderWaterProgram.mShaderFiles.push_back(make_pair("environment/underWaterF.glsl", GL_FRAGMENT_SHADER));
        gUnderWaterProgram.mShaderLevel = mShaderLevel[SHADER_WATER];
        gUnderWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
        gUnderWaterProgram.clearPermutations();
        if (LLPipeline::sRenderTransparentWater)
        {
            gUnderWaterProgram.addPermutation("TRANSPARENT_WATER", "1");
        }
        // MUST be after clearPermutations(). The underwater program shares
        // POOL_WATER, which opens the sidecar and coverage attachments; without
        // the extra output declarations this shader would write UNDEFINED values
        // to them (hazard H1) -- the precise failure this exists to prevent.
        if (sl_visible_diffuse_sidecar())
        {
            gUnderWaterProgram.addPermutation("HAS_VISIBLE_DIFFUSE", "1");
        }
        success = gUnderWaterProgram.createShader();
        llassert(success);
    }

    /// Keep track of water shader levels
    if (gWaterProgram.mShaderLevel != mShaderLevel[SHADER_WATER]
        || gUnderWaterProgram.mShaderLevel != mShaderLevel[SHADER_WATER])
    {
        mShaderLevel[SHADER_WATER] = llmin(gWaterProgram.mShaderLevel, gUnderWaterProgram.mShaderLevel);
    }

    if (!success)
    {
        mShaderLevel[SHADER_WATER] = 0;
        return false;
    }

    // if we failed to load the terrain water shaders and we need them (using class2 water),
    // then drop down to class1 water.
    if (mShaderLevel[SHADER_WATER] > 1 && !terrainWaterSuccess)
    {
        mShaderLevel[SHADER_WATER]--;
        return loadShadersWater();
    }

    if (LLWorld::instanceExists())
    {
        LLWorld::getInstance()->updateWaterObjects();
    }

    return true;
}

bool LLViewerShaderMgr::loadShadersEffects()
{
    LL_PROFILE_ZONE_SCOPED;
    bool success = true;

    if (mShaderLevel[SHADER_EFFECT] == 0)
    {
        gGlowProgram.unload();
        gGlowExtractProgram.unload();
        gBloomExtractProgram.unload();
        gBloomDownsampleProgram.unload();
        gBloomDownsampleFirstProgram.unload();
        gBloomUpsampleProgram.unload();
        gBloomCompositeProgram.unload();
        return true;
    }

    if (success)
    {
        gGlowProgram.mName = "Glow Shader (Post)";
        gGlowProgram.mShaderFiles.clear();
        gGlowProgram.mShaderFiles.push_back(make_pair("effects/glowV.glsl", GL_VERTEX_SHADER));
        gGlowProgram.mShaderFiles.push_back(make_pair("effects/glowF.glsl", GL_FRAGMENT_SHADER));
        gGlowProgram.mShaderLevel = mShaderLevel[SHADER_EFFECT];
        success = gGlowProgram.createShader();
        if (!success)
        {
            LLPipeline::sRenderGlow = false;
        }
    }

    if (success)
    {
        const bool use_glow_noise = gSavedSettings.getBOOL("RenderGlowNoise");
        const std::string glow_noise_label = use_glow_noise ? " (+Noise)" : "";

        gGlowExtractProgram.mName = llformat("Glow Extract Shader (Post)%s", glow_noise_label.c_str());
        gGlowExtractProgram.mShaderFiles.clear();
        gGlowExtractProgram.mShaderFiles.push_back(make_pair("effects/glowExtractV.glsl", GL_VERTEX_SHADER));
        gGlowExtractProgram.mShaderFiles.push_back(make_pair("effects/glowExtractF.glsl", GL_FRAGMENT_SHADER));
        gGlowExtractProgram.mShaderLevel = mShaderLevel[SHADER_EFFECT];

        if (use_glow_noise)
        {
            gGlowExtractProgram.addPermutation("HAS_NOISE", "1");
        }

        success = gGlowExtractProgram.createShader();
        if (!success)
        {
            LLPipeline::sRenderGlow = false;
        }
    }

    const bool bloom_halation = gSavedSettings.getBOOL("RenderBloomHalation");

    if (success)
    {
        gBloomExtractProgram.mName = "HDR Bloom Extract";
        gBloomExtractProgram.mShaderFiles.clear();
        gBloomExtractProgram.mShaderFiles.push_back(make_pair("effects/glowExtractV.glsl", GL_VERTEX_SHADER));
        gBloomExtractProgram.mShaderFiles.push_back(make_pair("effects/bloomExtractF.glsl", GL_FRAGMENT_SHADER));
        gBloomExtractProgram.mShaderLevel = mShaderLevel[SHADER_EFFECT];
        if (bloom_halation)
        {
            gBloomExtractProgram.addPermutation("BLOOM_HALATION", "1");
        }
        success = gBloomExtractProgram.createShader();
    }

    if (success)
    {
        gBloomDownsampleFirstProgram.mName = "HDR Bloom Downsample (First)";
        gBloomDownsampleFirstProgram.mShaderFiles.clear();
        gBloomDownsampleFirstProgram.mShaderFiles.push_back(make_pair("effects/glowExtractV.glsl", GL_VERTEX_SHADER));
        gBloomDownsampleFirstProgram.mShaderFiles.push_back(make_pair("effects/bloomDownsampleF.glsl", GL_FRAGMENT_SHADER));
        gBloomDownsampleFirstProgram.mShaderLevel = mShaderLevel[SHADER_EFFECT];
        gBloomDownsampleFirstProgram.addPermutation("FIRST_DOWNSAMPLE", "1");
        success = gBloomDownsampleFirstProgram.createShader();
    }

    if (success)
    {
        gBloomDownsampleProgram.mName = "HDR Bloom Downsample";
        gBloomDownsampleProgram.mShaderFiles.clear();
        gBloomDownsampleProgram.mShaderFiles.push_back(make_pair("effects/glowExtractV.glsl", GL_VERTEX_SHADER));
        gBloomDownsampleProgram.mShaderFiles.push_back(make_pair("effects/bloomDownsampleF.glsl", GL_FRAGMENT_SHADER));
        gBloomDownsampleProgram.mShaderLevel = mShaderLevel[SHADER_EFFECT];
        success = gBloomDownsampleProgram.createShader();
    }

    if (success)
    {
        gBloomUpsampleProgram.mName = "HDR Bloom Upsample";
        gBloomUpsampleProgram.mShaderFiles.clear();
        gBloomUpsampleProgram.mShaderFiles.push_back(make_pair("effects/glowExtractV.glsl", GL_VERTEX_SHADER));
        gBloomUpsampleProgram.mShaderFiles.push_back(make_pair("effects/bloomUpsampleF.glsl", GL_FRAGMENT_SHADER));
        gBloomUpsampleProgram.mShaderLevel = mShaderLevel[SHADER_EFFECT];
        success = gBloomUpsampleProgram.createShader();
    }

    if (success)
    {
        gBloomCompositeProgram.mName = "HDR Bloom Composite";
        gBloomCompositeProgram.mShaderFiles.clear();
        gBloomCompositeProgram.mShaderFiles.push_back(make_pair("effects/glowExtractV.glsl", GL_VERTEX_SHADER));
        gBloomCompositeProgram.mShaderFiles.push_back(make_pair("effects/bloomCompositeF.glsl", GL_FRAGMENT_SHADER));
        gBloomCompositeProgram.mShaderLevel = mShaderLevel[SHADER_EFFECT];
        if (bloom_halation)
        {
            gBloomCompositeProgram.addPermutation("BLOOM_HALATION", "1");
        }
        success = gBloomCompositeProgram.createShader();
    }

    return success;

}

bool LLViewerShaderMgr::loadShadersDeferred()
{
    LL_PROFILE_ZONE_SCOPED;
    bool use_sun_shadow = mShaderLevel[SHADER_DEFERRED] > 1 &&
        gSavedSettings.getS32("RenderShadowDetail") > 0;

    if (mShaderLevel[SHADER_DEFERRED] == 0)
    {
        gDeferredTreeProgram.unload();
        gDeferredTreeShadowProgram.unload();
        gDeferredSkinnedTreeShadowProgram.unload();
        gDeferredDiffuseProgram.unload();
        gDeferredSkinnedDiffuseProgram.unload();
        gDeferredDiffuseAlphaMaskProgram.unload();
        gDeferredSkinnedDiffuseAlphaMaskProgram.unload();
        gDeferredNonIndexedDiffuseAlphaMaskProgram.unload();
        gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.unload();
        gDeferredBumpProgram.unload();
        gDeferredSkinnedBumpProgram.unload();
        gDeferredImpostorProgram.unload();
        gDeferredTerrainProgram.unload();
        gDeferredLightProgram.unload();
        for (U32 i = 0; i < LL_DEFERRED_MULTI_LIGHT_COUNT; ++i)
        {
            gDeferredMultiLightProgram[i].unload();
        }
        gDeferredSpotLightProgram.unload();
        gDeferredMultiSpotLightProgram.unload();
        gDeferredSunProgram.unload();
        gDeferredBlurLightProgram.unload();
        gDeferredSoftenProgram.unload();
        gVisibleDiffuseSeedProgram.unload();
        gDeferredShadowProgram.unload();
        gDeferredSkinnedShadowProgram.unload();
        gDeferredShadowCubeProgram.unload();
        gDeferredShadowAlphaMaskProgram.unload();
        gDeferredSkinnedShadowAlphaMaskProgram.unload();
        gDeferredShadowGLTFAlphaMaskProgram.unload();
        gDeferredShadowGLTFAlphaMaskIndexedProgram.unload();
        gDeferredSkinnedShadowGLTFAlphaMaskIndexedProgram.unload();
        gDeferredShadowMaterialIndexedProgram.unload();
        gDeferredSkinnedShadowMaterialIndexedProgram.unload();
        gDeferredSkinnedShadowGLTFAlphaMaskProgram.unload();
        gDeferredShadowFullbrightAlphaMaskProgram.unload();
        gDeferredSkinnedShadowFullbrightAlphaMaskProgram.unload();
        gDeferredAvatarShadowProgram.unload();
        gDeferredAvatarAlphaShadowProgram.unload();
        gDeferredAvatarAlphaMaskShadowProgram.unload();
        gDeferredAvatarProgram.unload();
        gDeferredAvatarAlphaProgram.unload();
        gDeferredAlphaProgram.unload();
        gHUDAlphaProgram.unload();
        gDeferredSkinnedAlphaProgram.unload();
        gDeferredFullbrightProgram.unload();
        gHUDFullbrightProgram.unload();
        gDeferredFullbrightAlphaMaskProgram.unload();
        gHUDFullbrightAlphaMaskProgram.unload();
        gDeferredFullbrightAlphaMaskAlphaProgram.unload();
        gHUDFullbrightAlphaMaskAlphaProgram.unload();
        gDeferredEmissiveProgram.unload();
        gDeferredSkinnedEmissiveProgram.unload();
        gDeferredEmissiveIndexedProgram.unload();
        gDeferredSkinnedEmissiveIndexedProgram.unload();
        gActorFxGlowProgram.unload();
        gActorFxSkinnedGlowProgram.unload();
        gActorFxPBRGlowProgram.unload();
        gActorFxPBRSkinnedGlowProgram.unload();
        gDeferredAvatarEyesProgram.unload();
        gDeferredPostProgram.unload();
        gDeferredCoFProgram.unload();
        gDeferredDoFCombineProgram.unload();
        gExposureProgram.unload();
        gExposureProgramNoFade.unload();
        gLuminanceProgram.unload();

        for (auto i = 0; i < 4; ++i)
        {
            gFXAAProgram[i].unload();
            gSMAAEdgeDetectProgram[i].unload();
            gSMAABlendWeightsProgram[i].unload();
            gSMAANeighborhoodBlendProgram[i].unload();
        }
        gCASProgram.unload();
        gCineFisheyeProgram.unload();
        // [Ultimate Diopter]
        for (U32 i = 0; i < 4; ++i)
        {
            gUltimateDiopterGatherProgram[i].unload();
        }
        gUltimateDiopterProgram.unload();
        gUltimateKaleidoProgram.unload();
        gVolumetricLightProgram.unload();
        gCineOutlineProgram.unload();
        gDeferredProjectorVolumetricProgram.unload(); // [BDMerge G3.3]
        gDeferredProjectorVolumetricUpsampleProgram.unload(); // [BDMerge G3.3 P1 item 3]
        gDeferredProjectorVolumetricTemporalProgram.unload(); // [BDMerge G3.3 Batch 1 A]
        gDeferredProjectorVolumetricBloomFeedProgram.unload(); // [BDMerge G3.3 P3 item 4]
        gDeferredWeatherRainProgram.unload();
        gDeferredWeatherRainOcclusionProgram.unload();
        gDeferredWeatherRainUpsampleProgram.unload();
        gDeferredWeatherSurfaceProgram.unload();
        gDeferredWeatherSurfaceOcclusionProgram.unload();
        gDeferredWeatherLightningProgram.unload();
        gDeferredWeatherLightningQualityProgram.unload();
        gEnvironmentMapProgram.unload();
        gDeferredWLSkyProgram.unload();
        gDeferredWLCloudProgram.unload();
        gDeferredWLSunProgram.unload();
        gDeferredWLMoonProgram.unload();
        gDeferredStarProgram.unload();
        gDeferredMeteorProgram.unload();
        gDeferredAuroraProgram.unload();
        gDeferredFullbrightShinyProgram.unload();
        gHUDFullbrightShinyProgram.unload();
        gDeferredSkinnedFullbrightShinyProgram.unload();
        gDeferredSkinnedFullbrightProgram.unload();
        gDeferredSkinnedFullbrightAlphaMaskProgram.unload();
        gDeferredSkinnedFullbrightAlphaMaskAlphaProgram.unload();

        gDeferredHighlightProgram.unload();
        gPrismLensProgram.unload();

        gNormalMapGenProgram.unload();
        gDeferredGenBrdfLutProgram.unload();
        gDeferredBufferVisualProgram.unload();
        gVelocityProgram.unload();          // [BDMerge A5.4-1a]
        gVelocityAlphaProgram.unload();     // [BDMerge A5.4-1a]
        gVelocityPBRAlphaProgram.unload();
        gVelocityAlphaIndexedProgram.unload();
        gVelocityPBRAlphaIndexedProgram.unload();
        gVelocityDebugProgram.unload();     // [BDMerge A5.4-1a]
        gVelocityCameraProgram.unload();    // [BDMerge A5.4-1c]
        gVelocitySkinnedProgram.unload();       // [BDMerge A5.4-1b]
        gVelocityAlphaSkinnedProgram.unload();  // [BDMerge A5.4-1b]
        gVelocityPBRAlphaSkinnedProgram.unload();
        gVelocityAlphaIndexedSkinnedProgram.unload();
        gVelocityPBRAlphaIndexedSkinnedProgram.unload();
        gAvatarVelocityProgram.unload();        // [BDMerge A5.4-1b]
        gDeferredMotionBlurProgram.unload();    // [BDMerge A5.4-3]
        gFroxelMediaProgram.unload();       // [BDMerge Froxel F0]
        gFroxelDebugProgram.unload();       // [BDMerge Froxel F0]
        gFroxelIntegrateProgram.unload();   // [BDMerge Froxel F1]
        gFroxelApplyProgram.unload();       // [BDMerge Froxel F1]
        gFroxelInjectProgram.unload();      // [BDMerge Froxel F2]
        gFroxelTemporalProgram.unload();    // [BDMerge Froxel F3]

        for (U32 i = 0; i < LLMaterial::SHADER_COUNT*2; ++i)
        {
            gDeferredMaterialProgram[i].unload();
            gDeferredMaterialIndexedProgram[i].unload();
        }
        LLGLSLShader::sIndexedLegacyMaterials = false;

        gHUDPBROpaqueProgram.unload();
        gPBRGlowProgram.unload();
        gPBRGlowIndexedProgram.unload();
        gPBRGlowSkinnedIndexedProgram.unload();
        gDeferredPBROpaqueProgram.unload();
        gDeferredPBROpaqueIndexedProgram.unload();
        gDeferredSkinnedPBROpaqueIndexedProgram.unload();
        gDeferredSkinnedPBROpaqueProgram.unload();
        gDeferredPBRAlphaProgram.unload();
        gDeferredSkinnedPBRAlphaProgram.unload();
        unload_shared_actor_fx_pbr_family();
        for (U32 paint_type = 0; paint_type < TERRAIN_PAINT_TYPE_COUNT; ++paint_type)
        {
            gDeferredPBRTerrainProgram[paint_type].unload();
        }

// [RLVa:KB] - @setsphere
        gRlvSphereProgram.unload();
// [/RLVa:KB]

        return true;
    }

    bool success = true;

    gPrismLensProgram.mName = "Prism Lens Composite Shader";
    gPrismLensProgram.mShaderFiles.clear();
    gPrismLensProgram.clearPermutations();
    gPrismLensProgram.mShaderFiles.push_back(make_pair("deferred/prismLensV.glsl", GL_VERTEX_SHADER));
    gPrismLensProgram.mShaderFiles.push_back(make_pair("deferred/prismLensF.glsl", GL_FRAGMENT_SHADER));
    gPrismLensProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
    if (!gPrismLensProgram.createShader())
    {
        LL_WARNS("Shader") << "Prism Lens composite shader failed to load; Prism Lens disabled. Deferred rendering unaffected." << LL_ENDL;
    }

    if (success)
    {
        gDeferredHighlightProgram.mName = "Deferred Highlight Shader";
        gDeferredHighlightProgram.mShaderFiles.clear();
        gDeferredHighlightProgram.mShaderFiles.push_back(make_pair("interface/highlightV.glsl", GL_VERTEX_SHADER));
        gDeferredHighlightProgram.mShaderFiles.push_back(make_pair("deferred/highlightF.glsl", GL_FRAGMENT_SHADER));
        gDeferredHighlightProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        add_common_permutations(&gDeferredHighlightProgram);
        success = gDeferredHighlightProgram.createShader();
    }

    if (success)
    {
        gDeferredDiffuseProgram.mName = "Deferred Diffuse Shader";
        gDeferredDiffuseProgram.mFeatures.hasSrgb = true;
        gDeferredDiffuseProgram.mFeatures.hasActorFx = true;
        gDeferredDiffuseProgram.mShaderFiles.clear();
        gDeferredDiffuseProgram.mShaderFiles.push_back(make_pair("deferred/diffuseV.glsl", GL_VERTEX_SHADER));
        gDeferredDiffuseProgram.mShaderFiles.push_back(make_pair("deferred/diffuseIndexedF.glsl", GL_FRAGMENT_SHADER));
        gDeferredDiffuseProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
        gDeferredDiffuseProgram.addPermutation("HAS_ACTOR_FX", "1");
        gDeferredDiffuseProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        add_common_permutations(&gDeferredDiffuseProgram);
        success = make_rigged_variant(gDeferredDiffuseProgram, gDeferredSkinnedDiffuseProgram);
        success = success && gDeferredDiffuseProgram.createShader();
    }

    if (success)
    {
        gDeferredDiffuseAlphaMaskProgram.mName = "Deferred Diffuse Alpha Mask Shader";
        gDeferredDiffuseAlphaMaskProgram.mFeatures.hasSrgb = true;
        gDeferredDiffuseAlphaMaskProgram.mFeatures.hasActorFx = true;
        gDeferredDiffuseAlphaMaskProgram.mShaderFiles.clear();
        gDeferredDiffuseAlphaMaskProgram.mShaderFiles.push_back(make_pair("deferred/diffuseV.glsl", GL_VERTEX_SHADER));
        gDeferredDiffuseAlphaMaskProgram.mShaderFiles.push_back(make_pair("deferred/diffuseAlphaMaskIndexedF.glsl", GL_FRAGMENT_SHADER));
        gDeferredDiffuseAlphaMaskProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
        gDeferredDiffuseAlphaMaskProgram.addPermutation("HAS_ACTOR_FX", "1");
        gDeferredDiffuseAlphaMaskProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        add_common_permutations(&gDeferredDiffuseAlphaMaskProgram);
        success = make_rigged_variant(gDeferredDiffuseAlphaMaskProgram, gDeferredSkinnedDiffuseAlphaMaskProgram);
        success = success && gDeferredDiffuseAlphaMaskProgram.createShader();
    }

    if (success)
    {
        gDeferredNonIndexedDiffuseAlphaMaskProgram.mName = "Deferred Diffuse Non-Indexed Alpha Mask Shader";
        gDeferredNonIndexedDiffuseAlphaMaskProgram.mShaderFiles.clear();
        gDeferredNonIndexedDiffuseAlphaMaskProgram.mShaderFiles.push_back(make_pair("deferred/diffuseV.glsl", GL_VERTEX_SHADER));
        gDeferredNonIndexedDiffuseAlphaMaskProgram.mShaderFiles.push_back(make_pair("deferred/diffuseAlphaMaskF.glsl", GL_FRAGMENT_SHADER));
        gDeferredNonIndexedDiffuseAlphaMaskProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        add_common_permutations(&gDeferredNonIndexedDiffuseAlphaMaskProgram);
        success = gDeferredNonIndexedDiffuseAlphaMaskProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.mName = "Deferred Diffuse Non-Indexed Alpha Mask No Color Shader";
        gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.mFeatures.hasSrgb = true;
        gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.mFeatures.hasActorFx = true;
        gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.mShaderFiles.clear();
        gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.mShaderFiles.push_back(make_pair("deferred/diffuseNoColorV.glsl", GL_VERTEX_SHADER));
        gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.mShaderFiles.push_back(make_pair("deferred/diffuseAlphaMaskNoColorF.glsl", GL_FRAGMENT_SHADER));
        gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.addPermutation("HAS_ACTOR_FX", "1");
        add_common_permutations(&gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram);
        success = gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredBumpProgram.mName = "Deferred Bump Shader";
        gDeferredBumpProgram.mFeatures.hasSrgb = true;
        gDeferredBumpProgram.mFeatures.hasActorFx = true;
        gDeferredBumpProgram.mShaderFiles.clear();
        gDeferredBumpProgram.mShaderFiles.push_back(make_pair("deferred/bumpV.glsl", GL_VERTEX_SHADER));
        gDeferredBumpProgram.mShaderFiles.push_back(make_pair("deferred/bumpF.glsl", GL_FRAGMENT_SHADER));
        gDeferredBumpProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gDeferredBumpProgram.addPermutation("HAS_ACTOR_FX", "1");
        add_common_permutations(&gDeferredBumpProgram);
        success = make_rigged_variant(gDeferredBumpProgram, gDeferredSkinnedBumpProgram);
        success = success && gDeferredBumpProgram.createShader();
        llassert(success);
    }

    gDeferredMaterialProgram[1].mFeatures.hasLighting = false;
    gDeferredMaterialProgram[5].mFeatures.hasLighting = false;
    gDeferredMaterialProgram[9].mFeatures.hasLighting = false;
    gDeferredMaterialProgram[13].mFeatures.hasLighting = false;
    gDeferredMaterialProgram[1+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = false;
    gDeferredMaterialProgram[5+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = false;
    gDeferredMaterialProgram[9+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = false;
    gDeferredMaterialProgram[13+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = false;

    for (U32 i = 0; i < LLMaterial::SHADER_COUNT*2; ++i)
    {
        if (success)
        {
            bool has_skin = i & 0x10;

            if (!has_skin)
            {
                mShaderList.push_back(&gDeferredMaterialProgram[i]);
                gDeferredMaterialProgram[i].mName = llformat("Material Shader %d", i);
            }
            else
            {
                gDeferredMaterialProgram[i].mName = llformat("Skinned Material Shader %d", i);
            }

            U32 alpha_mode = i & 0x3;

            gDeferredMaterialProgram[i].mShaderFiles.clear();
            gDeferredMaterialProgram[i].mShaderFiles.push_back(make_pair("deferred/materialV.glsl", GL_VERTEX_SHADER));
            gDeferredMaterialProgram[i].mShaderFiles.push_back(make_pair("deferred/materialF.glsl", GL_FRAGMENT_SHADER));
            gDeferredMaterialProgram[i].mShaderLevel = mShaderLevel[SHADER_DEFERRED];
            gDeferredMaterialProgram[i].mFeatures.hasActorFx = true;

            gDeferredMaterialProgram[i].clearPermutations();
            gDeferredMaterialProgram[i].addPermutation("HAS_ACTOR_FX", "1");

            bool has_normal_map   = (i & 0x8) > 0;
            bool has_specular_map = (i & 0x4) > 0;

            if (has_normal_map)
            {
                gDeferredMaterialProgram[i].addPermutation("HAS_NORMAL_MAP", "1");
            }

            if (has_specular_map)
            {
                gDeferredMaterialProgram[i].addPermutation("HAS_SPECULAR_MAP", "1");
            }

            gDeferredMaterialProgram[i].addPermutation("DIFFUSE_ALPHA_MODE", llformat("%d", alpha_mode));

            if (alpha_mode != 0)
            {
                gDeferredMaterialProgram[i].mFeatures.hasAlphaMask = true;
                gDeferredMaterialProgram[i].addPermutation("HAS_ALPHA_MASK", "1");
            }

            if (use_sun_shadow)
            {
                gDeferredMaterialProgram[i].addPermutation("HAS_SUN_SHADOW", "1");
            }

            add_common_permutations(&gDeferredMaterialProgram[i]);

            gDeferredMaterialProgram[i].mFeatures.hasSrgb = true;
            gDeferredMaterialProgram[i].mFeatures.calculatesAtmospherics = true;
            gDeferredMaterialProgram[i].mFeatures.hasAtmospherics = true;
            gDeferredMaterialProgram[i].mFeatures.hasGamma = true;
            gDeferredMaterialProgram[i].mFeatures.hasShadows = use_sun_shadow;
            gDeferredMaterialProgram[i].mFeatures.hasReflectionProbes = true;

            if (has_skin)
            {
                gDeferredMaterialProgram[i].addPermutation("HAS_SKIN", "1");
                gDeferredMaterialProgram[i].mFeatures.hasObjectSkinning = true;
            }
            else
            {
                gDeferredMaterialProgram[i].mRiggedVariant = &gDeferredMaterialProgram[i + 0x10];
            }

            success = gDeferredMaterialProgram[i].createShader();
            llassert(success);
        }
    }

    gDeferredMaterialProgram[1].mFeatures.hasLighting = true;
    gDeferredMaterialProgram[5].mFeatures.hasLighting = true;
    gDeferredMaterialProgram[9].mFeatures.hasLighting = true;
    gDeferredMaterialProgram[13].mFeatures.hasLighting = true;
    gDeferredMaterialProgram[1+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = true;
    gDeferredMaterialProgram[5+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = true;
    gDeferredMaterialProgram[9+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = true;
    gDeferredMaterialProgram[13+LLMaterial::SHADER_COUNT].mFeatures.hasLighting = true;

    // Clear any stale value from a previous load before (re)deciding legacy indexed
    // eligibility -- if the block below is skipped or fails partway, the flag must
    // not carry a prior 'true' while the indexed programs are unloaded/incomplete.
    LLGLSLShader::sIndexedLegacyMaterials = false;

    if (success && LLGLSLShader::sIndexedGLTFChannels >= 2)
    {
        // Indexed (multi-material) legacy material GBuffer-write programs, parallel to
        // gDeferredMaterialProgram but covering only the non-blend (GBuffer) masks and
        // sampling the GBuffer-relevant maps only. Optional: failure leaves
        // sIndexedLegacyMaterials false so legacy batching is skipped (the pool falls
        // back to scalar). Kept out of the `success` chain.
        bool material_indexed_ok = true;
        for (U32 i = 0; i < LLMaterial::SHADER_COUNT*2 && material_indexed_ok; ++i)
        {
            U32 alpha_mode = i & 0x3;
            if (alpha_mode == 1) // DIFFUSE_ALPHA_MODE_BLEND -- forward/alpha pool, not indexed
            {
                continue;
            }

            bool has_skin   = (i & 0x10) != 0;
            bool has_spec   = (i & 0x4) != 0;
            bool has_normal = (i & 0x8) != 0;

            LLGLSLShader& prog = gDeferredMaterialIndexedProgram[i];
            prog.mName = llformat("Material Indexed Shader %d", i);
            prog.mShaderFiles.clear();
            prog.mShaderFiles.push_back(make_pair("deferred/materialIndexedV.glsl", GL_VERTEX_SHADER));
            prog.mShaderFiles.push_back(make_pair("deferred/materialIndexedF.glsl", GL_FRAGMENT_SHADER));
            prog.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
            prog.mFeatures.mIndexedMaterialChannels = LLGLSLShader::sIndexedGLTFChannels;
            prog.mFeatures.hasSrgb = true;
            prog.mFeatures.hasActorFx = true;
            prog.clearPermutations();
            prog.addPermutation("HAS_ACTOR_FX", "1");
            if (has_normal) prog.addPermutation("HAS_NORMAL_MAP", "1");
            if (has_spec)   prog.addPermutation("HAS_SPECULAR_MAP", "1");
            prog.addPermutation("DIFFUSE_ALPHA_MODE", llformat("%d", alpha_mode));
            prog.addPermutation("GLTF_INDEXED_CHANNELS", llformat("%d", LLGLSLShader::sIndexedGLTFChannels));
            add_common_permutations(&prog);

            if (has_skin)
            {
                prog.addPermutation("HAS_SKIN", "1");
                prog.mFeatures.hasObjectSkinning = true;
            }
            else
            {
                prog.mRiggedVariant = &gDeferredMaterialIndexedProgram[i + 0x10];
            }

            material_indexed_ok = prog.createShader();
            if (material_indexed_ok)
            {
                setup_material_indexed_samplers(prog, LLGLSLShader::sIndexedGLTFChannels, has_normal, has_spec);
            }
        }

        if (material_indexed_ok)
        {
            LLGLSLShader::sIndexedLegacyMaterials = true;
        }
        else
        {
            LL_WARNS("ShaderLoading") << "Indexed legacy material shaders failed to load; legacy batching disabled." << LL_ENDL;
            for (U32 i = 0; i < LLMaterial::SHADER_COUNT*2; ++i)
            {
                gDeferredMaterialIndexedProgram[i].unload();
            }
        }
    }

    if (success)
    {
        gDeferredPBROpaqueProgram.mName = "Deferred PBR Opaque Shader";
        gDeferredPBROpaqueProgram.mFeatures.hasSrgb = true;
        gDeferredPBROpaqueProgram.mFeatures.hasActorFx = true;

        gDeferredPBROpaqueProgram.mShaderFiles.clear();
        gDeferredPBROpaqueProgram.mShaderFiles.push_back(make_pair("deferred/pbropaqueV.glsl", GL_VERTEX_SHADER));
        gDeferredPBROpaqueProgram.mShaderFiles.push_back(make_pair("deferred/pbropaqueF.glsl", GL_FRAGMENT_SHADER));
        gDeferredPBROpaqueProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gDeferredPBROpaqueProgram.clearPermutations();
        gDeferredPBROpaqueProgram.addPermutation("HAS_ACTOR_FX", "1");

        add_common_permutations(&gDeferredPBROpaqueProgram);

        success = make_rigged_variant(gDeferredPBROpaqueProgram, gDeferredSkinnedPBROpaqueProgram);
        if (success)
        {
            success = gDeferredPBROpaqueProgram.createShader();
        }
        llassert(success);
    }

    if (success && LLGLSLShader::sIndexedGLTFChannels >= 2)
    {
        // Indexed (multi-material) PBR opaque. Optional acceleration: failure here
        // disables GLTF batching but must NOT fail overall shader loading, so the
        // result is kept out of the `success` chain.
        gDeferredPBROpaqueIndexedProgram.mName = "Deferred PBR Opaque Indexed Shader";
        gDeferredPBROpaqueIndexedProgram.mFeatures.mIndexedMaterialChannels = LLGLSLShader::sIndexedGLTFChannels;
        gDeferredPBROpaqueIndexedProgram.mFeatures.hasSrgb = true;
        gDeferredPBROpaqueIndexedProgram.mFeatures.hasActorFx = true;
        gDeferredPBROpaqueIndexedProgram.mShaderFiles.clear();
        gDeferredPBROpaqueIndexedProgram.mShaderFiles.push_back(make_pair("deferred/pbropaqueIndexedV.glsl", GL_VERTEX_SHADER));
        gDeferredPBROpaqueIndexedProgram.mShaderFiles.push_back(make_pair("deferred/pbropaqueIndexedF.glsl", GL_FRAGMENT_SHADER));
        gDeferredPBROpaqueIndexedProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gDeferredPBROpaqueIndexedProgram.clearPermutations();
        gDeferredPBROpaqueIndexedProgram.addPermutation("HAS_ACTOR_FX", "1");
        gDeferredPBROpaqueIndexedProgram.addPermutation("GLTF_INDEXED_CHANNELS", llformat("%d", LLGLSLShader::sIndexedGLTFChannels));
        add_common_permutations(&gDeferredPBROpaqueIndexedProgram);

        // rigged (skinned) variant for animesh / avatar attachments
        bool indexed_ok = make_rigged_variant(gDeferredPBROpaqueIndexedProgram, gDeferredSkinnedPBROpaqueIndexedProgram);
        if (indexed_ok)
        {
            indexed_ok = gDeferredPBROpaqueIndexedProgram.createShader();
        }

        if (indexed_ok)
        {
            // Map each slot's four material samplers to texture units, on both the
            // static and rigged variants.
            const S32 n = LLGLSLShader::sIndexedGLTFChannels;
            setup_gltf_indexed_samplers(gDeferredPBROpaqueIndexedProgram, n, true);
            setup_gltf_indexed_samplers(gDeferredSkinnedPBROpaqueIndexedProgram, n, true);
        }
        else
        {
            // Degrade gracefully: route all PBR faces back to the scalar path.
            LL_WARNS("ShaderLoading") << "Indexed PBR shader failed to load; GLTF batching disabled." << LL_ENDL;
            disable_indexed_gltf_batching();
        }
    }
    else
    {
        LLGLSLShader::sIndexedGLTFChannels = 0;
    }

    if (success)
    {
        gPBRGlowProgram.mName = " PBR Glow Shader";
        gPBRGlowProgram.mFeatures.hasSrgb = true;
        gPBRGlowProgram.mFeatures.hasActorFx = true;
        gPBRGlowProgram.mShaderFiles.clear();
        gPBRGlowProgram.mShaderFiles.push_back(make_pair("deferred/pbrglowV.glsl", GL_VERTEX_SHADER));
        gPBRGlowProgram.mShaderFiles.push_back(make_pair("deferred/pbrglowF.glsl", GL_FRAGMENT_SHADER));
        gPBRGlowProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gPBRGlowProgram.addPermutation("HAS_ACTOR_FX", "1");

        add_common_permutations(&gPBRGlowProgram);

        success = make_rigged_variant(gPBRGlowProgram, gPBRGlowSkinnedProgram);
        if (success)
        {
            success = gPBRGlowProgram.createShader();
        }
        llassert(success);
    }

    if (success && LLGLSLShader::sIndexedGLTFChannels >= 2)
    {
        // Indexed (multi-material) PBR glow, parallel to gPBRGlowProgram. Shares the
        // GBuffer indexed sampler-unit layout (base color s, emissive 3N+s) so
        // pushGLTFBatchIndexed drives it directly. A failure disables indexed
        // material formation so rebuilt geometry uses the scalar glow path.
        gPBRGlowIndexedProgram.mName = "PBR Glow Indexed Shader";
        gPBRGlowIndexedProgram.mFeatures.mIndexedMaterialChannels = LLGLSLShader::sIndexedGLTFChannels;
        gPBRGlowIndexedProgram.mFeatures.hasSrgb = true;
        gPBRGlowIndexedProgram.mFeatures.hasActorFx = true;
        gPBRGlowIndexedProgram.mShaderFiles.clear();
        gPBRGlowIndexedProgram.mShaderFiles.push_back(make_pair("deferred/pbrglowIndexedV.glsl", GL_VERTEX_SHADER));
        gPBRGlowIndexedProgram.mShaderFiles.push_back(make_pair("deferred/pbrglowIndexedF.glsl", GL_FRAGMENT_SHADER));
        gPBRGlowIndexedProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gPBRGlowIndexedProgram.clearPermutations();
        gPBRGlowIndexedProgram.addPermutation("HAS_ACTOR_FX", "1");
        gPBRGlowIndexedProgram.addPermutation("GLTF_INDEXED_CHANNELS", llformat("%d", LLGLSLShader::sIndexedGLTFChannels));
        add_common_permutations(&gPBRGlowIndexedProgram);

        bool glow_indexed_ok = make_rigged_variant(gPBRGlowIndexedProgram, gPBRGlowSkinnedIndexedProgram);
        if (glow_indexed_ok)
        {
            glow_indexed_ok = gPBRGlowIndexedProgram.createShader();
        }
        if (glow_indexed_ok)
        {
            S32 n = LLGLSLShader::sIndexedGLTFChannels;
            setup_gltf_indexed_samplers(gPBRGlowIndexedProgram, n, true);
            setup_gltf_indexed_samplers(gPBRGlowSkinnedIndexedProgram, n, true);
        }
        else
        {
            LL_WARNS("ShaderLoading") << "Indexed PBR glow shader failed to load; GLTF batching disabled." << LL_ENDL;
            disable_indexed_gltf_batching();
        }
    }

    if (success)
    {
        gHUDPBROpaqueProgram.mName = "HUD PBR Opaque Shader";
        gHUDPBROpaqueProgram.mFeatures.hasSrgb = true;
        gHUDPBROpaqueProgram.mShaderFiles.clear();
        gHUDPBROpaqueProgram.mShaderFiles.push_back(make_pair("deferred/pbropaqueV.glsl", GL_VERTEX_SHADER));
        gHUDPBROpaqueProgram.mShaderFiles.push_back(make_pair("deferred/pbropaqueF.glsl", GL_FRAGMENT_SHADER));
        gHUDPBROpaqueProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gHUDPBROpaqueProgram.clearPermutations();
        gHUDPBROpaqueProgram.addPermutation("IS_HUD", "1");

        add_common_permutations(&gHUDPBROpaqueProgram);

        success = gHUDPBROpaqueProgram.createShader();

        llassert(success);
    }

    if (success)
    {
        LLGLSLShader* shader = &gDeferredPBRAlphaProgram;
        shader->mName = "Deferred PBR Alpha Shader";

        shader->mFeatures.calculatesLighting = false;
        shader->mFeatures.hasLighting = false;
        shader->mFeatures.isAlphaLighting = true;
        shader->mFeatures.hasSrgb = true;
        shader->mFeatures.calculatesAtmospherics = true;
        shader->mFeatures.hasAtmospherics = true;
        shader->mFeatures.hasGamma = true;
        shader->mFeatures.hasShadows = use_sun_shadow;
        shader->mFeatures.isDeferred = true; // include deferredUtils
        shader->mFeatures.hasReflectionProbes = mShaderLevel[SHADER_DEFERRED];
        shader->mFeatures.hasActorFx = true;

        shader->mShaderFiles.clear();
        shader->mShaderFiles.push_back(make_pair("deferred/pbralphaV.glsl", GL_VERTEX_SHADER));
        shader->mShaderFiles.push_back(make_pair("deferred/pbralphaF.glsl", GL_FRAGMENT_SHADER));

        shader->clearPermutations();
        shader->addPermutation("HAS_ACTOR_FX", "1");

        U32 alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_BLEND;
        shader->addPermutation("DIFFUSE_ALPHA_MODE", llformat("%d", alpha_mode));
        shader->addPermutation("HAS_NORMAL_MAP", "1");
        shader->addPermutation("HAS_SPECULAR_MAP", "1"); // PBR: Packed: Occlusion, Metal, Roughness
        shader->addPermutation("HAS_EMISSIVE_MAP", "1");
        shader->addPermutation("USE_VERTEX_COLOR", "1");

        add_common_permutations(shader);

        if (use_sun_shadow)
        {
            shader->addPermutation("HAS_SUN_SHADOW", "1");
        }

        shader->mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = make_rigged_variant(*shader, gDeferredSkinnedPBRAlphaProgram);
        if (success)
        {
            success = shader->createShader();
        }
        llassert(success);

        // Alpha Shader Hack
        // See: LLRender::syncMatrices()
        shader->mFeatures.calculatesLighting = true;
        shader->mFeatures.hasLighting = true;

        shader->mRiggedVariant->mFeatures.calculatesLighting = true;
        shader->mRiggedVariant->mFeatures.hasLighting = true;
    }

    if (success)
    {
        // [ActorStyle/Shared PBR Phase 1] Optional, isolated forward-PBR replay
        // family.  It intentionally does not participate in the main `success`
        // chain: a failed cinematic replay program must leave native GLTF
        // rendering available.  The family is unloaded as a unit below.
        unload_shared_actor_fx_pbr_family();
        static const char* const mode_names[SHARED_ACTOR_FX_PBR_ALPHA_COUNT] =
        {
            "Opaque", "Mask", "Blend"
        };
        static const U32 material_alpha_modes[SHARED_ACTOR_FX_PBR_ALPHA_COUNT] =
        {
            LLMaterial::DIFFUSE_ALPHA_MODE_NONE,
            LLMaterial::DIFFUSE_ALPHA_MODE_MASK,
            LLMaterial::DIFFUSE_ALPHA_MODE_BLEND
        };

        auto load_shared_beauty =
            [&](LLGLSLShader& beauty, LLGLSLShader& skinned_beauty,
                U32 mode, bool slot_filter) -> bool
        {
            beauty.mName = llformat("Shared Actor FX PBR %s %s Shader",
                                    mode_names[mode],
                                    slot_filter ? "Slot" : "Scalar");
            beauty.mFeatures.calculatesLighting = false;
            beauty.mFeatures.hasLighting = false;
            beauty.mFeatures.isAlphaLighting = true;
            beauty.mFeatures.hasSrgb = true;
            beauty.mFeatures.calculatesAtmospherics = true;
            beauty.mFeatures.hasAtmospherics = true;
            beauty.mFeatures.hasGamma = true;
            beauty.mFeatures.hasShadows = use_sun_shadow;
            beauty.mFeatures.isDeferred = true;
            beauty.mFeatures.hasReflectionProbes =
                mShaderLevel[SHADER_DEFERRED];
            beauty.mFeatures.hasActorFx = true;
            const S32 indexed_channels = llmax(
                LLGLSLShader::sSharedPBRIndexedGLTFChannels, 1);
            if (slot_filter)
            {
                beauty.mFeatures.mIndexedMaterialChannels = indexed_channels;
            }
            beauty.mShaderFiles.clear();
            beauty.mShaderFiles.push_back(make_pair(
                "deferred/sharedActorFxPbrV.glsl", GL_VERTEX_SHADER));
            beauty.mShaderFiles.push_back(make_pair(
                "deferred/sharedActorFxPbrF.glsl", GL_FRAGMENT_SHADER));
            beauty.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
            beauty.clearPermutations();
            beauty.addPermutation("HAS_ACTOR_FX", "1");
            beauty.addPermutation("SHARED_ACTOR_FX_REPLAY", "1");
            beauty.addPermutation("SHARED_ACTOR_FX_ALPHA_MODE",
                                  llformat("%u", mode));
            beauty.addPermutation("DIFFUSE_ALPHA_MODE",
                                  llformat("%u", material_alpha_modes[mode]));
            beauty.addPermutation("HAS_NORMAL_MAP", "1");
            beauty.addPermutation("HAS_SPECULAR_MAP", "1");
            beauty.addPermutation("HAS_EMISSIVE_MAP", "1");
            beauty.addPermutation("USE_VERTEX_COLOR", "1");
            if (slot_filter)
            {
                beauty.addPermutation("SHARED_ACTOR_FX_SLOT_FILTER", "1");
                beauty.addPermutation("GLTF_INDEXED_CHANNELS",
                                      llformat("%d", indexed_channels));
            }
            if (mode == SHARED_ACTOR_FX_PBR_ALPHA_MASK)
            {
                beauty.addPermutation("HAS_ALPHA_MASK", "1");
            }
            if (use_sun_shadow)
            {
                beauty.addPermutation("HAS_SUN_SHADOW", "1");
            }

            // Deliberately omit add_common_permutations(): shared replay owns
            // exactly one HDR colour output and no optional MRT sidecars.
            bool ok = make_rigged_variant(beauty, skinned_beauty);
            if (ok)
            {
                ok = beauty.createShader();
            }
            if (ok)
            {
                // Same post-link matrix-sync contract as native PBR alpha.
                beauty.mFeatures.calculatesLighting = true;
                beauty.mFeatures.hasLighting = true;
                skinned_beauty.mFeatures.calculatesLighting = true;
                skinned_beauty.mFeatures.hasLighting = true;
                init_shared_actor_fx_pbr_replay_uniforms(beauty);
                init_shared_actor_fx_pbr_replay_uniforms(skinned_beauty);
                if (slot_filter)
                {
                    setup_gltf_indexed_samplers(beauty, indexed_channels,
                                                true);
                    setup_gltf_indexed_samplers(skinned_beauty,
                                                indexed_channels, true);
                }
            }
            return ok;
        };

        auto load_shared_glow =
            [&](LLGLSLShader& glow, LLGLSLShader& skinned_glow,
                U32 mode, bool slot_filter, bool synthetic) -> bool
        {
            glow.mName = llformat("Shared Actor FX PBR %s %s %sGlow Shader",
                                  mode_names[mode],
                                  slot_filter ? "Slot" : "Scalar",
                                  synthetic ? "Synthetic " : "");
            glow.mFeatures.hasSrgb = true;
            glow.mFeatures.hasActorFx = true;
            // Synthetic treatment bloom is emitted from the same pre-resolve
            // world stream as shared beauty. Attach the identical sky/water
            // atmosphere law; authored native bloom remains unmodified.
            glow.mFeatures.calculatesAtmospherics = true;
            glow.mFeatures.hasAtmospherics = true;
            const S32 indexed_channels = llmax(
                LLGLSLShader::sSharedPBRIndexedGLTFChannels, 1);
            if (slot_filter)
            {
                glow.mFeatures.mIndexedMaterialChannels = indexed_channels;
            }
            glow.mShaderFiles.clear();
            glow.mShaderFiles.push_back(make_pair(
                synthetic
                    ? "deferred/sharedActorFxPbrSyntheticGlowV.glsl"
                    : "deferred/sharedActorFxPbrGlowV.glsl",
                GL_VERTEX_SHADER));
            glow.mShaderFiles.push_back(make_pair(
                synthetic
                    ? "deferred/sharedActorFxPbrSyntheticGlowF.glsl"
                    : "deferred/sharedActorFxPbrGlowF.glsl",
                GL_FRAGMENT_SHADER));
            glow.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
            glow.clearPermutations();
            glow.addPermutation("HAS_ACTOR_FX", "1");
            glow.addPermutation("SHARED_ACTOR_FX_REPLAY", "1");
            glow.addPermutation("SHARED_ACTOR_FX_ALPHA_MODE",
                                llformat("%u", mode));
            glow.addPermutation("DIFFUSE_ALPHA_MODE",
                                llformat("%u", material_alpha_modes[mode]));
            if (slot_filter)
            {
                glow.addPermutation("SHARED_ACTOR_FX_SLOT_FILTER", "1");
                glow.addPermutation("GLTF_INDEXED_CHANNELS",
                                    llformat("%d", indexed_channels));
            }
            if (mode == SHARED_ACTOR_FX_PBR_ALPHA_MASK)
            {
                glow.addPermutation("HAS_ALPHA_MASK", "1");
            }

            bool ok = make_rigged_variant(glow, skinned_glow);
            if (ok)
            {
                ok = glow.createShader();
            }
            if (ok)
            {
                init_shared_actor_fx_pbr_replay_uniforms(glow);
                init_shared_actor_fx_pbr_replay_uniforms(skinned_glow);
                if (slot_filter)
                {
                    // Authored glow consumes base+emissive while synthetic glow
                    // consumes base only. The common mapping helper safely
                    // skips sampler uniforms optimized out of either program.
                    setup_gltf_indexed_samplers(glow, indexed_channels, true);
                    setup_gltf_indexed_samplers(skinned_glow,
                                                indexed_channels, true);
                }
            }
            return ok;
        };

        auto load_shared_depth =
            [&](LLGLSLShader& depth, LLGLSLShader& skinned_depth,
                U32 mode, bool indexed) -> bool
        {
            depth.mName = llformat("Shared Actor FX PBR %s %s Depth Shader",
                                  mode_names[mode],
                                  indexed ? "Indexed" : "Scalar");
            // Depth needs only the dissolve module. Linking the beauty module
            // would pull the full material treatment into this hot prepass.
            depth.mFeatures.hasActorFxShadow = true;
            const S32 indexed_channels = llmax(
                LLGLSLShader::sSharedPBRIndexedGLTFChannels, 1);
            if (indexed)
            {
                depth.mFeatures.mIndexedMaterialChannels = indexed_channels;
            }
            depth.mShaderFiles.clear();
            depth.mShaderFiles.push_back(make_pair(
                "deferred/sharedActorFxPbrDepthV.glsl", GL_VERTEX_SHADER));
            depth.mShaderFiles.push_back(make_pair(
                "deferred/sharedActorFxPbrDepthF.glsl", GL_FRAGMENT_SHADER));
            depth.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
            depth.clearPermutations();
            depth.addPermutation("SHARED_ACTOR_FX_REPLAY", "1");
            depth.addPermutation("SHARED_ACTOR_FX_ALPHA_MODE",
                                 llformat("%u", mode));
            if (indexed)
            {
                depth.addPermutation("SHARED_ACTOR_FX_SLOT_FILTER", "1");
                depth.addPermutation("GLTF_INDEXED_CHANNELS",
                                     llformat("%d", indexed_channels));
            }

            bool ok = make_rigged_variant(depth, skinned_depth);
            if (ok)
            {
                ok = depth.createShader();
            }
            if (ok && indexed)
            {
                // OPAQUE optimizes every sampler out; MASK retains base color.
                setup_gltf_indexed_samplers(depth, indexed_channels, false);
                setup_gltf_indexed_samplers(skinned_depth,
                                            indexed_channels, false);
            }
            return ok;
        };

        bool shared_pbr_ok = true;
        for (U32 mode = 0; mode < SHARED_ACTOR_FX_PBR_ALPHA_COUNT; ++mode)
        {
            const bool scalar_beauty_ok = load_shared_beauty(
                gSharedActorFxPBRProgram[mode],
                gSharedActorFxSkinnedPBRProgram[mode], mode, false);
            const bool slot_beauty_ok = load_shared_beauty(
                gSharedActorFxPBRSlotProgram[mode],
                gSharedActorFxSkinnedPBRSlotProgram[mode], mode, true);
            const bool scalar_glow_ok = load_shared_glow(
                gSharedActorFxPBRGlowProgram[mode],
                gSharedActorFxSkinnedPBRGlowProgram[mode],
                mode, false, false);
            const bool slot_glow_ok = load_shared_glow(
                gSharedActorFxPBRGlowSlotProgram[mode],
                gSharedActorFxSkinnedPBRGlowSlotProgram[mode],
                mode, true, false);
            const bool scalar_synthetic_ok = load_shared_glow(
                gSharedActorFxPBRSyntheticGlowProgram[mode],
                gSharedActorFxSkinnedPBRSyntheticGlowProgram[mode],
                mode, false, true);
            const bool slot_synthetic_ok = load_shared_glow(
                gSharedActorFxPBRSyntheticGlowSlotProgram[mode],
                gSharedActorFxSkinnedPBRSyntheticGlowSlotProgram[mode],
                mode, true, true);

            bool scalar_depth_ok = true;
            bool indexed_depth_ok = true;
            if (mode != SHARED_ACTOR_FX_PBR_ALPHA_BLEND)
            {
                scalar_depth_ok = load_shared_depth(
                    gSharedActorFxPBRDepthProgram[mode],
                    gSharedActorFxSkinnedPBRDepthProgram[mode], mode, false);
                indexed_depth_ok = load_shared_depth(
                    gSharedActorFxPBRDepthSlotProgram[mode],
                    gSharedActorFxSkinnedPBRDepthSlotProgram[mode], mode, true);
            }

            shared_pbr_ok = shared_pbr_ok && scalar_beauty_ok &&
                            slot_beauty_ok && scalar_glow_ok &&
                            slot_glow_ok && scalar_synthetic_ok &&
                            slot_synthetic_ok && scalar_depth_ok &&
                            indexed_depth_ok;
        }

        if (!shared_pbr_ok || !shared_actor_fx_pbr_family_loaded())
        {
            unload_shared_actor_fx_pbr_family();
            // No shared forward replay will consume rigged geometry until the
            // next shader reload, so restore the native indexed width rather
            // than imposing the forward sampler reserve on native rendering.
            LLGLSLShader::sSharedPBRIndexedGLTFChannels =
                LLGLSLShader::sIndexedGLTFChannels;
            LL_WARNS("ShaderLoading")
                << "Shared Actor FX forward-PBR shader family failed to load; "
                   "cinematic replay remains fail-open to native PBR rendering"
                << LL_ENDL;
        }
    }

    if (success)
    {
        LLGLSLShader* shader = &gHUDPBRAlphaProgram;
        shader->mName = "HUD PBR Alpha Shader";

        shader->mFeatures.hasSrgb = true;

        shader->mShaderFiles.clear();
        shader->mShaderFiles.push_back(make_pair("deferred/pbralphaV.glsl", GL_VERTEX_SHADER));
        shader->mShaderFiles.push_back(make_pair("deferred/pbralphaF.glsl", GL_FRAGMENT_SHADER));

        shader->clearPermutations();

        shader->addPermutation("IS_HUD", "1");

        add_common_permutations(shader);

        shader->mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = shader->createShader();
        llassert(success);
    }

    if (success)
    {
        S32 detail = gSavedSettings.getS32("RenderTerrainPBRDetail");
        detail = llclamp(detail, TERRAIN_PBR_DETAIL_MIN, TERRAIN_PBR_DETAIL_MAX);
        const S32 mapping = clamp_terrain_mapping(gSavedSettings.getS32("RenderTerrainPBRPlanarSampleCount"));
        for (U32 paint_type = 0; paint_type < TERRAIN_PAINT_TYPE_COUNT; ++paint_type)
        {
            LLGLSLShader* shader = &gDeferredPBRTerrainProgram[paint_type];
            shader->mName = llformat("Deferred PBR Terrain Shader %d %s %s",
                    detail,
                    (paint_type == TERRAIN_PAINT_TYPE_PBR_PAINTMAP ? "paintmap" : "heightmap-with-noise"),
                    (mapping == 1 ? "flat" : "triplanar"));
            shader->mFeatures.hasSrgb = true;
            shader->mFeatures.isAlphaLighting = true;
            shader->mFeatures.calculatesAtmospherics = true;
            shader->mFeatures.hasAtmospherics = true;
            shader->mFeatures.hasGamma = true;
            shader->mFeatures.hasTransport = true;
            shader->mFeatures.isPBRTerrain = true;

            shader->mShaderFiles.clear();
            shader->mShaderFiles.push_back(make_pair("deferred/pbrterrainV.glsl", GL_VERTEX_SHADER));
            shader->mShaderFiles.push_back(make_pair("deferred/pbrterrainF.glsl", GL_FRAGMENT_SHADER));
            shader->mShaderLevel = mShaderLevel[SHADER_DEFERRED];
            shader->addPermutation("TERRAIN_PBR_DETAIL", llformat("%d", detail));
            shader->addPermutation("TERRAIN_PAINT_TYPE", llformat("%d", paint_type));
            shader->addPermutation("TERRAIN_PLANAR_TEXTURE_SAMPLE_COUNT", llformat("%d", mapping));

            add_common_permutations(shader);

            success = success && shader->createShader();
            llassert(success);
        }
    }

    if (success)
    {
        gDeferredTreeProgram.mName = "Deferred Tree Shader";
        gDeferredTreeProgram.mShaderFiles.clear();
        gDeferredTreeProgram.mShaderFiles.push_back(make_pair("deferred/treeV.glsl", GL_VERTEX_SHADER));
        gDeferredTreeProgram.mShaderFiles.push_back(make_pair("deferred/treeF.glsl", GL_FRAGMENT_SHADER));
        gDeferredTreeProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];

        add_common_permutations(&gDeferredTreeProgram);

        success = gDeferredTreeProgram.createShader();
    }

    if (success)
    {
        gDeferredTreeShadowProgram.mName = "Deferred Tree Shadow Shader";
        gDeferredTreeShadowProgram.mFeatures.hasActorFxShadow = true;
        gDeferredTreeShadowProgram.mShaderFiles.clear();
        gDeferredTreeShadowProgram.mShaderFiles.push_back(make_pair("deferred/treeShadowV.glsl", GL_VERTEX_SHADER));
        gDeferredTreeShadowProgram.mShaderFiles.push_back(make_pair("deferred/treeShadowF.glsl", GL_FRAGMENT_SHADER));
        gDeferredTreeShadowProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gDeferredTreeShadowProgram.mRiggedVariant = &gDeferredSkinnedTreeShadowProgram;
        success = gDeferredTreeShadowProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredSkinnedTreeShadowProgram.mName = "Deferred Skinned Tree Shadow Shader";
        gDeferredSkinnedTreeShadowProgram.mFeatures.hasActorFxShadow = true;
        gDeferredSkinnedTreeShadowProgram.mShaderFiles.clear();
        gDeferredSkinnedTreeShadowProgram.mFeatures.hasObjectSkinning = true;
        gDeferredSkinnedTreeShadowProgram.mShaderFiles.push_back(make_pair("deferred/treeShadowSkinnedV.glsl", GL_VERTEX_SHADER));
        gDeferredSkinnedTreeShadowProgram.mShaderFiles.push_back(make_pair("deferred/treeShadowF.glsl", GL_FRAGMENT_SHADER));
        gDeferredSkinnedTreeShadowProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gDeferredSkinnedTreeShadowProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredImpostorProgram.mName = "Deferred Impostor Shader";
        gDeferredImpostorProgram.mFeatures.hasSrgb = true;
        gDeferredImpostorProgram.mShaderFiles.clear();
        gDeferredImpostorProgram.mShaderFiles.push_back(make_pair("deferred/impostorV.glsl", GL_VERTEX_SHADER));
        gDeferredImpostorProgram.mShaderFiles.push_back(make_pair("deferred/impostorF.glsl", GL_FRAGMENT_SHADER));
        gDeferredImpostorProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];

        add_common_permutations(&gDeferredImpostorProgram);

        success = gDeferredImpostorProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredLightProgram.mName = "Deferred Light Shader";
        gDeferredLightProgram.mFeatures.isDeferred = true;
        gDeferredLightProgram.mFeatures.hasFullGBuffer = true;
        gDeferredLightProgram.mFeatures.hasShadows = true;
        gDeferredLightProgram.mFeatures.hasSrgb = true;

        gDeferredLightProgram.mShaderFiles.clear();
        gDeferredLightProgram.mShaderFiles.push_back(make_pair("deferred/pointLightV.glsl", GL_VERTEX_SHADER));
        gDeferredLightProgram.mShaderFiles.push_back(make_pair("deferred/pointLightF.glsl", GL_FRAGMENT_SHADER));
        gDeferredLightProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];

        gDeferredLightProgram.clearPermutations();

        add_common_permutations(&gDeferredLightProgram);

        success = gDeferredLightProgram.createShader();
        llassert(success);
    }

    for (U32 i = 0; i < LL_DEFERRED_MULTI_LIGHT_COUNT; i++)
    {
        if (success)
        {
            gDeferredMultiLightProgram[i].mName = llformat("Deferred MultiLight Shader %d", i);
            gDeferredMultiLightProgram[i].mFeatures.isDeferred = true;
            gDeferredMultiLightProgram[i].mFeatures.hasFullGBuffer = true;
            gDeferredMultiLightProgram[i].mFeatures.hasShadows = true;
            gDeferredMultiLightProgram[i].mFeatures.hasSrgb = true;

            gDeferredMultiLightProgram[i].clearPermutations();
            gDeferredMultiLightProgram[i].mShaderFiles.clear();
            gDeferredMultiLightProgram[i].mShaderFiles.push_back(make_pair("deferred/multiPointLightV.glsl", GL_VERTEX_SHADER));
            gDeferredMultiLightProgram[i].mShaderFiles.push_back(make_pair("deferred/multiPointLightF.glsl", GL_FRAGMENT_SHADER));
            gDeferredMultiLightProgram[i].mShaderLevel = mShaderLevel[SHADER_DEFERRED];
            gDeferredMultiLightProgram[i].addPermutation("LIGHT_COUNT", llformat("%d", i+1));

            add_common_permutations(&gDeferredMultiLightProgram[i]);

            success = gDeferredMultiLightProgram[i].createShader();
            llassert(success);
        }
    }

    if (success)
    {
        gDeferredSpotLightProgram.mName = "Deferred SpotLight Shader";
        gDeferredSpotLightProgram.mShaderFiles.clear();
        gDeferredSpotLightProgram.mFeatures.hasSrgb = true;
        gDeferredSpotLightProgram.mFeatures.isDeferred = true;
        gDeferredSpotLightProgram.mFeatures.hasFullGBuffer = true;
        gDeferredSpotLightProgram.mFeatures.hasShadows = true;

        gDeferredSpotLightProgram.clearPermutations();
        gDeferredSpotLightProgram.mShaderFiles.push_back(make_pair("deferred/pointLightV.glsl", GL_VERTEX_SHADER));
        gDeferredSpotLightProgram.mShaderFiles.push_back(make_pair("deferred/spotLightF.glsl", GL_FRAGMENT_SHADER));
        gDeferredSpotLightProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];

        add_common_permutations(&gDeferredSpotLightProgram);

        success = gDeferredSpotLightProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredMultiSpotLightProgram.mName = "Deferred MultiSpotLight Shader";
        gDeferredMultiSpotLightProgram.mFeatures.hasSrgb = true;
        gDeferredMultiSpotLightProgram.mFeatures.isDeferred = true;
        gDeferredMultiSpotLightProgram.mFeatures.hasFullGBuffer = true;
        gDeferredMultiSpotLightProgram.mFeatures.hasShadows = true;

        gDeferredMultiSpotLightProgram.clearPermutations();
        gDeferredMultiSpotLightProgram.addPermutation("MULTI_SPOTLIGHT", "1");
        gDeferredMultiSpotLightProgram.mShaderFiles.clear();
        gDeferredMultiSpotLightProgram.mShaderFiles.push_back(make_pair("deferred/multiPointLightV.glsl", GL_VERTEX_SHADER));
        gDeferredMultiSpotLightProgram.mShaderFiles.push_back(make_pair("deferred/spotLightF.glsl", GL_FRAGMENT_SHADER));
        gDeferredMultiSpotLightProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];

        add_common_permutations(&gDeferredMultiSpotLightProgram);

        success = gDeferredMultiSpotLightProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        std::string fragment;
        bool use_ao = gSavedSettings.getBOOL("RenderDeferredSSAO");
        if (use_ao)
        {
            fragment = "deferred/sunLightSSAOF.glsl";
        }
        else
        {
            fragment = "deferred/sunLightF.glsl";
        }

        gDeferredSunProgram.mName = "Deferred Sun Shader";
        gDeferredSunProgram.mFeatures.isDeferred    = true;
        gDeferredSunProgram.mFeatures.hasShadows    = true;
        gDeferredSunProgram.mFeatures.hasAmbientOcclusion = use_ao;

        gDeferredSunProgram.mShaderFiles.clear();
        gDeferredSunProgram.mShaderFiles.push_back(make_pair("deferred/sunLightV.glsl", GL_VERTEX_SHADER));
        gDeferredSunProgram.mShaderFiles.push_back(make_pair(fragment, GL_FRAGMENT_SHADER));
        gDeferredSunProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];

        add_common_permutations(&gDeferredSunProgram);

        success = gDeferredSunProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredSunProbeProgram.mName = "Deferred Sun Probe Shader";
        gDeferredSunProbeProgram.mFeatures.isDeferred = true;
        gDeferredSunProbeProgram.mFeatures.hasShadows = true;

        gDeferredSunProbeProgram.mShaderFiles.clear();
        gDeferredSunProbeProgram.mShaderFiles.push_back(make_pair("deferred/sunLightV.glsl", GL_VERTEX_SHADER));
        gDeferredSunProbeProgram.mShaderFiles.push_back(make_pair("deferred/sunLightF.glsl", GL_FRAGMENT_SHADER));
        gDeferredSunProbeProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];

        add_common_permutations(&gDeferredSunProbeProgram);

        success = gDeferredSunProbeProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredBlurLightProgram.mName = "Deferred Blur Light Shader";
        gDeferredBlurLightProgram.mFeatures.isDeferred = true;

        gDeferredBlurLightProgram.mShaderFiles.clear();
        gDeferredBlurLightProgram.mShaderFiles.push_back(make_pair("deferred/blurLightV.glsl", GL_VERTEX_SHADER));
        gDeferredBlurLightProgram.mShaderFiles.push_back(make_pair("deferred/blurLightF.glsl", GL_FRAGMENT_SHADER));
        gDeferredBlurLightProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];

        add_common_permutations(&gDeferredBlurLightProgram);

        success = gDeferredBlurLightProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        for (int i = 0; i < 3 && success; ++i)
        {
            LLGLSLShader* shader = nullptr;
            bool rigged = (i == 1);
            bool hud = (i == 2);

            if (hud)
            {
                shader = &gHUDAlphaProgram;
                shader->mName = "HUD Alpha Shader";
            }
            else if (!rigged)
            {
                shader = &gDeferredAlphaProgram;
                shader->mName = "Deferred Alpha Shader";
                shader->mRiggedVariant = &gDeferredSkinnedAlphaProgram;
            }
            else
            {
                shader = &gDeferredSkinnedAlphaProgram;
                shader->mName = "Skinned Deferred Alpha Shader";
                shader->mFeatures.hasObjectSkinning = true;
            }

            shader->mFeatures.calculatesLighting = false;
            shader->mFeatures.hasLighting = false;
            shader->mFeatures.isAlphaLighting = true;
            shader->mFeatures.hasSrgb = true;
            shader->mFeatures.calculatesAtmospherics = true;
            shader->mFeatures.hasAtmospherics = true;
            shader->mFeatures.hasGamma = true;
            shader->mFeatures.hasShadows = use_sun_shadow;
            shader->mFeatures.hasReflectionProbes = true;
            shader->mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
            shader->mFeatures.hasActorFx = !hud;

            shader->mShaderFiles.clear();
            shader->mShaderFiles.push_back(make_pair("deferred/alphaV.glsl", GL_VERTEX_SHADER));
            shader->mShaderFiles.push_back(make_pair("deferred/alphaF.glsl", GL_FRAGMENT_SHADER));

            shader->clearPermutations();
            if (!hud)
            {
                shader->addPermutation("HAS_ACTOR_FX", "1");
            }
            shader->addPermutation("USE_VERTEX_COLOR", "1");
            shader->addPermutation("HAS_ALPHA_MASK", "1");
            shader->addPermutation("USE_INDEXED_TEX", "1");
            if (use_sun_shadow)
            {
                shader->addPermutation("HAS_SUN_SHADOW", "1");
            }

            add_common_permutations(shader);

            if (rigged)
            {
                shader->addPermutation("HAS_SKIN", "1");
            }

            if (hud)
            {
                shader->addPermutation("IS_HUD", "1");
            }

            shader->mShaderLevel = mShaderLevel[SHADER_DEFERRED];

            success = shader->createShader();
            llassert(success);

            // Hack
            shader->mFeatures.calculatesLighting = true;
            shader->mFeatures.hasLighting = true;
        }
    }

    if (success)
    {
        LLGLSLShader* shaders[] = {
            &gDeferredAlphaImpostorProgram,
            &gDeferredSkinnedAlphaImpostorProgram
        };

        for (int i = 0; i < 2 && success; ++i)
        {
            bool rigged = i == 1;
            LLGLSLShader* shader = shaders[i];

            shader->mName = rigged ? "Skinned Deferred Alpha Impostor Shader" : "Deferred Alpha Impostor Shader";

            // Begin Hack
            shader->mFeatures.calculatesLighting = false;
            shader->mFeatures.hasLighting = false;

            shader->mFeatures.hasSrgb = true;
            shader->mFeatures.isAlphaLighting = true;
            shader->mFeatures.hasShadows = use_sun_shadow;
            shader->mFeatures.hasReflectionProbes = true;
            shader->mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;

            shader->mShaderFiles.clear();
            shader->mShaderFiles.push_back(make_pair("deferred/alphaV.glsl", GL_VERTEX_SHADER));
            shader->mShaderFiles.push_back(make_pair("deferred/alphaF.glsl", GL_FRAGMENT_SHADER));

            shader->clearPermutations();
            shader->addPermutation("USE_INDEXED_TEX", "1");
            shader->addPermutation("FOR_IMPOSTOR", "1");
            shader->addPermutation("HAS_ALPHA_MASK", "1");
            shader->addPermutation("USE_VERTEX_COLOR", "1");
            if (rigged)
            {
                shader->mFeatures.hasObjectSkinning = true;
                shader->addPermutation("HAS_SKIN", "1");
            }

            if (use_sun_shadow)
            {
                shader->addPermutation("HAS_SUN_SHADOW", "1");
            }

            add_common_permutations(shader);

            shader->mRiggedVariant = &gDeferredSkinnedAlphaImpostorProgram;
            shader->mShaderLevel = mShaderLevel[SHADER_DEFERRED];
            if (!rigged)
            {
                shader->mRiggedVariant = shaders[1];
            }
            success = shader->createShader();
            llassert(success);

            // End Hack
            shader->mFeatures.calculatesLighting = true;
            shader->mFeatures.hasLighting = true;
        }
    }

    if (success)
    {
        gDeferredAvatarEyesProgram.mName = "Deferred Avatar Eyes Shader";
        gDeferredAvatarEyesProgram.mFeatures.calculatesAtmospherics = true;
        gDeferredAvatarEyesProgram.mFeatures.hasGamma = true;
        gDeferredAvatarEyesProgram.mFeatures.hasAtmospherics = true;
        gDeferredAvatarEyesProgram.mFeatures.hasSrgb = true;
        gDeferredAvatarEyesProgram.mFeatures.hasShadows = true;
        gDeferredAvatarEyesProgram.mFeatures.hasActorFx = true;

        gDeferredAvatarEyesProgram.mShaderFiles.clear();
        gDeferredAvatarEyesProgram.mShaderFiles.push_back(make_pair("deferred/avatarEyesV.glsl", GL_VERTEX_SHADER));
        gDeferredAvatarEyesProgram.mShaderFiles.push_back(make_pair("deferred/diffuseF.glsl", GL_FRAGMENT_SHADER));
        gDeferredAvatarEyesProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gDeferredAvatarEyesProgram.addPermutation("HAS_ACTOR_FX", "1");

        add_common_permutations(&gDeferredAvatarEyesProgram);

        success = gDeferredAvatarEyesProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredFullbrightProgram.mName = "Deferred Fullbright Shader";
        gDeferredFullbrightProgram.mFeatures.calculatesAtmospherics = true;
        gDeferredFullbrightProgram.mFeatures.hasGamma = true;
        gDeferredFullbrightProgram.mFeatures.hasAtmospherics = true;
        gDeferredFullbrightProgram.mFeatures.hasSrgb = true;
        gDeferredFullbrightProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
        gDeferredFullbrightProgram.mFeatures.hasActorFx = true;
        gDeferredFullbrightProgram.mShaderFiles.clear();
        gDeferredFullbrightProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightV.glsl", GL_VERTEX_SHADER));
        gDeferredFullbrightProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightF.glsl", GL_FRAGMENT_SHADER));
        gDeferredFullbrightProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gDeferredFullbrightProgram.addPermutation("HAS_ACTOR_FX", "1");

        add_common_permutations(&gDeferredFullbrightProgram);

        success = make_rigged_variant(gDeferredFullbrightProgram, gDeferredSkinnedFullbrightProgram);
        success = success && gDeferredFullbrightProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gHUDFullbrightProgram.mName = "HUD Fullbright Shader";
        gHUDFullbrightProgram.mFeatures.calculatesAtmospherics = true;
        gHUDFullbrightProgram.mFeatures.hasGamma = true;
        gHUDFullbrightProgram.mFeatures.hasAtmospherics = true;
        gHUDFullbrightProgram.mFeatures.hasSrgb = true;
        gHUDFullbrightProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
        gHUDFullbrightProgram.mShaderFiles.clear();
        gHUDFullbrightProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightV.glsl", GL_VERTEX_SHADER));
        gHUDFullbrightProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightF.glsl", GL_FRAGMENT_SHADER));
        gHUDFullbrightProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gHUDFullbrightProgram.clearPermutations();
        gHUDFullbrightProgram.addPermutation("IS_HUD", "1");

        add_common_permutations(&gHUDFullbrightProgram);

        success = gHUDFullbrightProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredFullbrightAlphaMaskProgram.mName = "Deferred Fullbright Alpha Masking Shader";
        gDeferredFullbrightAlphaMaskProgram.mFeatures.calculatesAtmospherics = true;
        gDeferredFullbrightAlphaMaskProgram.mFeatures.hasGamma = true;
        gDeferredFullbrightAlphaMaskProgram.mFeatures.hasAtmospherics = true;
        gDeferredFullbrightAlphaMaskProgram.mFeatures.hasSrgb = true;
        gDeferredFullbrightAlphaMaskProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
        gDeferredFullbrightAlphaMaskProgram.mFeatures.hasActorFx = true;
        gDeferredFullbrightAlphaMaskProgram.mShaderFiles.clear();
        gDeferredFullbrightAlphaMaskProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightV.glsl", GL_VERTEX_SHADER));
        gDeferredFullbrightAlphaMaskProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightF.glsl", GL_FRAGMENT_SHADER));
        gDeferredFullbrightAlphaMaskProgram.clearPermutations();
        gDeferredFullbrightAlphaMaskProgram.addPermutation("HAS_ALPHA_MASK","1");
        gDeferredFullbrightAlphaMaskProgram.addPermutation("HAS_ACTOR_FX", "1");
        gDeferredFullbrightAlphaMaskProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];

        add_common_permutations(&gDeferredFullbrightAlphaMaskProgram);

        success = make_rigged_variant(gDeferredFullbrightAlphaMaskProgram, gDeferredSkinnedFullbrightAlphaMaskProgram);
        success = success && gDeferredFullbrightAlphaMaskProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gHUDFullbrightAlphaMaskProgram.mName = "HUD Fullbright Alpha Masking Shader";
        gHUDFullbrightAlphaMaskProgram.mFeatures.calculatesAtmospherics = true;
        gHUDFullbrightAlphaMaskProgram.mFeatures.hasGamma = true;
        gHUDFullbrightAlphaMaskProgram.mFeatures.hasAtmospherics = true;
        gHUDFullbrightAlphaMaskProgram.mFeatures.hasSrgb = true;
        gHUDFullbrightAlphaMaskProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
        gHUDFullbrightAlphaMaskProgram.mShaderFiles.clear();
        gHUDFullbrightAlphaMaskProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightV.glsl", GL_VERTEX_SHADER));
        gHUDFullbrightAlphaMaskProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightF.glsl", GL_FRAGMENT_SHADER));
        gHUDFullbrightAlphaMaskProgram.clearPermutations();
        gHUDFullbrightAlphaMaskProgram.addPermutation("HAS_ALPHA_MASK", "1");
        gHUDFullbrightAlphaMaskProgram.addPermutation("IS_HUD", "1");

        add_common_permutations(&gHUDFullbrightAlphaMaskProgram);

        gHUDFullbrightAlphaMaskProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gHUDFullbrightAlphaMaskProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredFullbrightAlphaMaskAlphaProgram.mName = "Deferred Fullbright Alpha Masking Alpha Shader";
        gDeferredFullbrightAlphaMaskAlphaProgram.mFeatures.calculatesAtmospherics = true;
        gDeferredFullbrightAlphaMaskAlphaProgram.mFeatures.hasGamma = true;
        gDeferredFullbrightAlphaMaskAlphaProgram.mFeatures.hasAtmospherics = true;
        gDeferredFullbrightAlphaMaskAlphaProgram.mFeatures.hasSrgb = true;
        gDeferredFullbrightAlphaMaskAlphaProgram.mFeatures.isDeferred = true;
        gDeferredFullbrightAlphaMaskAlphaProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
        gDeferredFullbrightAlphaMaskAlphaProgram.mFeatures.hasActorFx = true;
        gDeferredFullbrightAlphaMaskAlphaProgram.mShaderFiles.clear();
        gDeferredFullbrightAlphaMaskAlphaProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightV.glsl", GL_VERTEX_SHADER));
        gDeferredFullbrightAlphaMaskAlphaProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightF.glsl", GL_FRAGMENT_SHADER));
        gDeferredFullbrightAlphaMaskAlphaProgram.clearPermutations();
        gDeferredFullbrightAlphaMaskAlphaProgram.addPermutation("HAS_ALPHA_MASK", "1");
        gDeferredFullbrightAlphaMaskAlphaProgram.addPermutation("IS_ALPHA", "1");
        gDeferredFullbrightAlphaMaskAlphaProgram.addPermutation("HAS_ACTOR_FX", "1");

        add_common_permutations(&gDeferredFullbrightAlphaMaskAlphaProgram);

        gDeferredFullbrightAlphaMaskAlphaProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = make_rigged_variant(gDeferredFullbrightAlphaMaskAlphaProgram, gDeferredSkinnedFullbrightAlphaMaskAlphaProgram);
        success = success && gDeferredFullbrightAlphaMaskAlphaProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gHUDFullbrightAlphaMaskAlphaProgram.mName = "HUD Fullbright Alpha Masking Alpha Shader";
        gHUDFullbrightAlphaMaskAlphaProgram.mFeatures.calculatesAtmospherics = true;
        gHUDFullbrightAlphaMaskAlphaProgram.mFeatures.hasGamma = true;
        gHUDFullbrightAlphaMaskAlphaProgram.mFeatures.hasAtmospherics = true;
        gHUDFullbrightAlphaMaskAlphaProgram.mFeatures.hasSrgb = true;
        gHUDFullbrightAlphaMaskAlphaProgram.mFeatures.isDeferred = true;
        gHUDFullbrightAlphaMaskAlphaProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
        gHUDFullbrightAlphaMaskAlphaProgram.mShaderFiles.clear();
        gHUDFullbrightAlphaMaskAlphaProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightV.glsl", GL_VERTEX_SHADER));
        gHUDFullbrightAlphaMaskAlphaProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightF.glsl", GL_FRAGMENT_SHADER));
        gHUDFullbrightAlphaMaskAlphaProgram.clearPermutations();
        gHUDFullbrightAlphaMaskAlphaProgram.addPermutation("HAS_ALPHA_MASK", "1");
        gHUDFullbrightAlphaMaskAlphaProgram.addPermutation("IS_ALPHA", "1");
        gHUDFullbrightAlphaMaskAlphaProgram.addPermutation("IS_HUD", "1");

        add_common_permutations(&gHUDFullbrightAlphaMaskAlphaProgram);

        gHUDFullbrightAlphaMaskAlphaProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = success && gHUDFullbrightAlphaMaskAlphaProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredFullbrightShinyProgram.mName = "Deferred FullbrightShiny Shader";
        gDeferredFullbrightShinyProgram.mFeatures.calculatesAtmospherics = true;
        gDeferredFullbrightShinyProgram.mFeatures.hasAtmospherics = true;
        gDeferredFullbrightShinyProgram.mFeatures.hasGamma = true;
        gDeferredFullbrightShinyProgram.mFeatures.hasSrgb = true;
        gDeferredFullbrightShinyProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
        gDeferredFullbrightShinyProgram.mFeatures.hasActorFx = true;
        gDeferredFullbrightShinyProgram.mShaderFiles.clear();
        gDeferredFullbrightShinyProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightShinyV.glsl", GL_VERTEX_SHADER));
        gDeferredFullbrightShinyProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightShinyF.glsl", GL_FRAGMENT_SHADER));
        gDeferredFullbrightShinyProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gDeferredFullbrightShinyProgram.addPermutation("HAS_ACTOR_FX", "1");
        gDeferredFullbrightShinyProgram.mFeatures.hasReflectionProbes = true;

        add_common_permutations(&gDeferredFullbrightShinyProgram);

        success = make_rigged_variant(gDeferredFullbrightShinyProgram, gDeferredSkinnedFullbrightShinyProgram);
        success = success && gDeferredFullbrightShinyProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gHUDFullbrightShinyProgram.mName = "HUD FullbrightShiny Shader";
        gHUDFullbrightShinyProgram.mFeatures.calculatesAtmospherics = true;
        gHUDFullbrightShinyProgram.mFeatures.hasAtmospherics = true;
        gHUDFullbrightShinyProgram.mFeatures.hasGamma = true;
        gHUDFullbrightShinyProgram.mFeatures.hasSrgb = true;
        gHUDFullbrightShinyProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
        gHUDFullbrightShinyProgram.mShaderFiles.clear();
        gHUDFullbrightShinyProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightShinyV.glsl", GL_VERTEX_SHADER));
        gHUDFullbrightShinyProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightShinyF.glsl", GL_FRAGMENT_SHADER));
        gHUDFullbrightShinyProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gHUDFullbrightShinyProgram.mFeatures.hasReflectionProbes = true;
        gHUDFullbrightShinyProgram.clearPermutations();
        gHUDFullbrightShinyProgram.addPermutation("IS_HUD", "1");

        add_common_permutations(&gHUDFullbrightShinyProgram);

        success = gHUDFullbrightShinyProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredEmissiveProgram.mName = "Deferred Emissive Shader";
        gDeferredEmissiveProgram.mFeatures.calculatesAtmospherics = true;
        gDeferredEmissiveProgram.mFeatures.hasGamma = true;
        gDeferredEmissiveProgram.mFeatures.hasAtmospherics = true;
        gDeferredEmissiveProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;
        gDeferredEmissiveProgram.mFeatures.hasActorFx = true;
        gDeferredEmissiveProgram.mShaderFiles.clear();
        gDeferredEmissiveProgram.mShaderFiles.push_back(make_pair("deferred/emissiveV.glsl", GL_VERTEX_SHADER));
        gDeferredEmissiveProgram.mShaderFiles.push_back(make_pair("deferred/emissiveF.glsl", GL_FRAGMENT_SHADER));
        gDeferredEmissiveProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gDeferredEmissiveProgram.addPermutation("HAS_ACTOR_FX", "1");

        add_common_permutations(&gDeferredEmissiveProgram);

        success = make_rigged_variant(gDeferredEmissiveProgram, gDeferredSkinnedEmissiveProgram);
        success = success && gDeferredEmissiveProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        // A dedicated zero-authored-glow path uses the ordinary colour stream,
        // so Actor FX does not force every actor VBO to carry duplicate
        // emissive vertex data. It is only submitted for active glowing looks.
        gActorFxGlowProgram.mName = "Actor FX Synthetic Glow Shader";
        gActorFxGlowProgram.mFeatures.hasSrgb = true;
        gActorFxGlowProgram.mFeatures.hasActorFx = true;
        gActorFxGlowProgram.mFeatures.mIndexedTextureChannels =
            LLGLSLShader::sIndexedTextureChannels;
        gActorFxGlowProgram.mShaderFiles.clear();
        gActorFxGlowProgram.mShaderFiles.push_back(
            make_pair("deferred/actorFxGlowV.glsl", GL_VERTEX_SHADER));
        gActorFxGlowProgram.mShaderFiles.push_back(
            make_pair("deferred/actorFxGlowF.glsl", GL_FRAGMENT_SHADER));
        gActorFxGlowProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gActorFxGlowProgram.clearPermutations();
        gActorFxGlowProgram.addPermutation("HAS_ACTOR_FX", "1");
        success = make_rigged_variant(gActorFxGlowProgram,
                                      gActorFxSkinnedGlowProgram);
        success = success && gActorFxGlowProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gActorFxPBRGlowProgram.mName = "Actor FX Synthetic PBR Glow Shader";
        gActorFxPBRGlowProgram.mFeatures.hasSrgb = true;
        gActorFxPBRGlowProgram.mFeatures.hasActorFx = true;
        gActorFxPBRGlowProgram.mShaderFiles.clear();
        gActorFxPBRGlowProgram.mShaderFiles.push_back(
            make_pair("deferred/actorFxPbrGlowV.glsl", GL_VERTEX_SHADER));
        gActorFxPBRGlowProgram.mShaderFiles.push_back(
            make_pair("deferred/actorFxPbrGlowF.glsl", GL_FRAGMENT_SHADER));
        gActorFxPBRGlowProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gActorFxPBRGlowProgram.clearPermutations();
        gActorFxPBRGlowProgram.addPermutation("HAS_ACTOR_FX", "1");
        success = make_rigged_variant(gActorFxPBRGlowProgram,
                                      gActorFxPBRSkinnedGlowProgram);
        success = success && gActorFxPBRGlowProgram.createShader();
        llassert(success);
    }

    if (success && LLGLSLShader::sIndexedLegacyMaterials)
    {
        // Indexed (multi-material) legacy glow, parallel to gDeferredEmissiveProgram.
        // Selects each slot's diffuse map (bound to unit s) for the glow alpha mask.
        // Only enabled when legacy material batching is active; failure leaves the
        // program incomplete and the pool falls back to scalar glow. Kept out of the
        // `success` chain.
        gDeferredEmissiveIndexedProgram.mName = "Deferred Emissive Indexed Shader";
        gDeferredEmissiveIndexedProgram.mFeatures.mIndexedMaterialChannels = LLGLSLShader::sIndexedGLTFChannels;
        gDeferredEmissiveIndexedProgram.mFeatures.hasActorFx = true;
        gDeferredEmissiveIndexedProgram.mShaderFiles.clear();
        gDeferredEmissiveIndexedProgram.mShaderFiles.push_back(make_pair("deferred/emissiveIndexedV.glsl", GL_VERTEX_SHADER));
        gDeferredEmissiveIndexedProgram.mShaderFiles.push_back(make_pair("deferred/emissiveIndexedF.glsl", GL_FRAGMENT_SHADER));
        gDeferredEmissiveIndexedProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gDeferredEmissiveIndexedProgram.clearPermutations();
        gDeferredEmissiveIndexedProgram.addPermutation("HAS_ACTOR_FX", "1");
        gDeferredEmissiveIndexedProgram.addPermutation("GLTF_INDEXED_CHANNELS", llformat("%d", LLGLSLShader::sIndexedGLTFChannels));
        add_common_permutations(&gDeferredEmissiveIndexedProgram);

        bool emissive_indexed_ok = make_rigged_variant(gDeferredEmissiveIndexedProgram, gDeferredSkinnedEmissiveIndexedProgram);
        if (emissive_indexed_ok)
        {
            emissive_indexed_ok = gDeferredEmissiveIndexedProgram.createShader();
        }
        if (emissive_indexed_ok)
        {
            S32 n = LLGLSLShader::sIndexedGLTFChannels;
            setup_material_indexed_samplers(gDeferredEmissiveIndexedProgram, n, false, false);
            setup_material_indexed_samplers(gDeferredSkinnedEmissiveIndexedProgram, n, false, false);
        }
        else
        {
            LL_WARNS("ShaderLoading") << "Indexed legacy glow shader failed to load; multi-material glow falls back to scalar." << LL_ENDL;
            gDeferredEmissiveIndexedProgram.unload();
            gDeferredSkinnedEmissiveIndexedProgram.unload();
        }
    }

    if (success)
    {
        static LLCachedControl<bool> visible_diffuse(gSavedSettings, "RenderVisibleDiffuseSidecar", false);
        if (visible_diffuse)
        {
            gVisibleDiffuseSeedProgram.mName = "Visible Diffuse Seed Shader";
            gVisibleDiffuseSeedProgram.mShaderFiles.clear();
            gVisibleDiffuseSeedProgram.clearPermutations();
            gVisibleDiffuseSeedProgram.mFeatures.isDeferred = true;
            gVisibleDiffuseSeedProgram.mFeatures.hasFullGBuffer = true;
            gVisibleDiffuseSeedProgram.mShaderFiles.push_back(make_pair("deferred/softenLightV.glsl", GL_VERTEX_SHADER));
            gVisibleDiffuseSeedProgram.mShaderFiles.push_back(make_pair("deferred/visibleDiffuseSeedF.glsl", GL_FRAGMENT_SHADER));
            gVisibleDiffuseSeedProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];

            // NOT folded into `success`, and deliberately not asserted. This
            // program is OPTIONAL: it exists only when the visible-diffuse
            // sidecar is enabled, and the whole feature is designed to degrade
            // to feature-off. Gating `success` on it would abort the rest of
            // the load chain -- gDeferredSoftenProgram is created immediately
            // below -- and a missing REQUIRED program crashes at bind time
            // (llglslshader.cpp bind: ASSERT (mProgramObject != 0)). That is
            // exactly the CTD this feature already shipped once.
            //
            // Every dispatch site gates on isComplete(), so a failure here is
            // contained to "no sidecar this session".
            if (!gVisibleDiffuseSeedProgram.createShader())
            {
                LL_WARNS("ShaderLoading") << "Visible-diffuse seed shader failed to load; "
                                             "the sidecar is disabled for this session."
                                          << LL_ENDL;
                gVisibleDiffuseSeedProgram.unload();
            }
        }
        else
        {
            gVisibleDiffuseSeedProgram.unload();
        }
    }

    if (success)
    {
        gDeferredSoftenProgram.mName = "Deferred Soften Shader";
        gDeferredSoftenProgram.mShaderFiles.clear();
        gDeferredSoftenProgram.mFeatures.hasSrgb = true;
        gDeferredSoftenProgram.mFeatures.calculatesAtmospherics = true;
        gDeferredSoftenProgram.mFeatures.hasAtmospherics = true;
        gDeferredSoftenProgram.mFeatures.hasGamma = true;
        gDeferredSoftenProgram.mFeatures.isDeferred = true;
        gDeferredSoftenProgram.mFeatures.hasFullGBuffer = true;
        gDeferredSoftenProgram.mFeatures.hasShadows = use_sun_shadow;
        gDeferredSoftenProgram.mFeatures.hasReflectionProbes = mShaderLevel[SHADER_DEFERRED] > 2;

        gDeferredSoftenProgram.clearPermutations();
        add_common_permutations(&gDeferredSoftenProgram);
        gDeferredSoftenProgram.mShaderFiles.push_back(make_pair("deferred/softenLightV.glsl", GL_VERTEX_SHADER));
        gDeferredSoftenProgram.mShaderFiles.push_back(make_pair("deferred/softenLightF.glsl", GL_FRAGMENT_SHADER));

        gDeferredSoftenProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];

        if (use_sun_shadow)
        {
            gDeferredSoftenProgram.addPermutation("HAS_SUN_SHADOW", "1");
        }

        if (gSavedSettings.getBOOL("RenderDeferredSSAO"))
        { //if using SSAO, take screen space light map into account as if shadows are enabled
            gDeferredSoftenProgram.mShaderLevel = llmax(gDeferredSoftenProgram.mShaderLevel, 2);
            gDeferredSoftenProgram.addPermutation("HAS_SSAO", "1");
        }

        success = gDeferredSoftenProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gHazeProgram.mName = "Haze Shader";
        gHazeProgram.mShaderFiles.clear();
        gHazeProgram.mFeatures.hasSrgb                = true;
        gHazeProgram.mFeatures.calculatesAtmospherics = true;
        gHazeProgram.mFeatures.hasAtmospherics        = true;
        gHazeProgram.mFeatures.hasGamma               = true;
        gHazeProgram.mFeatures.isDeferred             = true;
        gHazeProgram.mFeatures.hasShadows             = use_sun_shadow;
        gHazeProgram.mFeatures.hasReflectionProbes    = mShaderLevel[SHADER_DEFERRED] > 2;

        gHazeProgram.clearPermutations();
        gHazeProgram.mShaderFiles.push_back(make_pair("deferred/softenLightV.glsl", GL_VERTEX_SHADER));
        gHazeProgram.mShaderFiles.push_back(make_pair("deferred/hazeF.glsl", GL_FRAGMENT_SHADER));

        add_common_permutations(&gHazeProgram);

        gHazeProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];

        success = gHazeProgram.createShader();
        llassert(success);
    }


    if (success)
    {
        gHazeWaterProgram.mName = "Water Haze Shader";
        gHazeWaterProgram.mShaderFiles.clear();
        gHazeWaterProgram.mShaderGroup           = LLGLSLShader::SG_WATER;
        gHazeWaterProgram.mFeatures.hasSrgb                = true;
        gHazeWaterProgram.mFeatures.calculatesAtmospherics = true;
        gHazeWaterProgram.mFeatures.hasAtmospherics        = true;
        gHazeWaterProgram.mFeatures.hasGamma               = true;
        gHazeWaterProgram.mFeatures.isDeferred             = true;
        gHazeWaterProgram.mFeatures.hasShadows             = use_sun_shadow;
        gHazeWaterProgram.mFeatures.hasReflectionProbes    = mShaderLevel[SHADER_DEFERRED] > 2;

        gHazeWaterProgram.clearPermutations();
        gHazeWaterProgram.mShaderFiles.push_back(make_pair("deferred/waterHazeV.glsl", GL_VERTEX_SHADER));
        gHazeWaterProgram.mShaderFiles.push_back(make_pair("deferred/waterHazeF.glsl", GL_FRAGMENT_SHADER));

        add_common_permutations(&gHazeWaterProgram);

        gHazeWaterProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];

        success = gHazeWaterProgram.createShader();
        llassert(success);
    }


    if (success)
    {
        gDeferredShadowProgram.mName = "Deferred Shadow Shader";
        gDeferredShadowProgram.mFeatures.hasActorFxShadow = true;
        gDeferredShadowProgram.mShaderFiles.clear();
        gDeferredShadowProgram.mShaderFiles.push_back(make_pair("deferred/shadowV.glsl", GL_VERTEX_SHADER));
        gDeferredShadowProgram.mShaderFiles.push_back(make_pair("deferred/shadowF.glsl", GL_FRAGMENT_SHADER));
        gDeferredShadowProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gDeferredShadowProgram.mRiggedVariant = &gDeferredSkinnedShadowProgram;
        success = gDeferredShadowProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredSkinnedShadowProgram.mName = "Deferred Skinned Shadow Shader";
        gDeferredSkinnedShadowProgram.mFeatures.hasActorFxShadow = true;
        gDeferredSkinnedShadowProgram.mFeatures.isDeferred = true;
        gDeferredSkinnedShadowProgram.mFeatures.hasShadows = true;
        gDeferredSkinnedShadowProgram.mFeatures.hasObjectSkinning = true;
        gDeferredSkinnedShadowProgram.mShaderFiles.clear();
        gDeferredSkinnedShadowProgram.mShaderFiles.push_back(make_pair("deferred/shadowSkinnedV.glsl", GL_VERTEX_SHADER));
        gDeferredSkinnedShadowProgram.mShaderFiles.push_back(make_pair("deferred/shadowF.glsl", GL_FRAGMENT_SHADER));
        gDeferredSkinnedShadowProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];

        add_common_permutations(&gDeferredSkinnedShadowProgram);

        // gDeferredSkinnedShadowProgram.addPermutation("DEPTH_CLAMP", "1"); // disable depth clamp for now
        success = gDeferredSkinnedShadowProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredShadowCubeProgram.mName = "Deferred Shadow Cube Shader";
        gDeferredShadowCubeProgram.mFeatures.hasActorFxShadow = true;
        gDeferredShadowCubeProgram.mFeatures.isDeferred = true;
        gDeferredShadowCubeProgram.mFeatures.hasShadows = true;
        gDeferredShadowCubeProgram.mShaderFiles.clear();
        gDeferredShadowCubeProgram.mShaderFiles.push_back(make_pair("deferred/shadowCubeV.glsl", GL_VERTEX_SHADER));
        gDeferredShadowCubeProgram.mShaderFiles.push_back(make_pair("deferred/shadowF.glsl", GL_FRAGMENT_SHADER));
        // gDeferredShadowCubeProgram.addPermutation("DEPTH_CLAMP", "1");
        gDeferredShadowCubeProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gDeferredShadowCubeProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredShadowFullbrightAlphaMaskProgram.mName = "Deferred Shadow Fullbright Alpha Mask Shader";
        gDeferredShadowFullbrightAlphaMaskProgram.mFeatures.hasActorFxShadow = true;
        gDeferredShadowFullbrightAlphaMaskProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;

        gDeferredShadowFullbrightAlphaMaskProgram.mShaderFiles.clear();
        gDeferredShadowFullbrightAlphaMaskProgram.mShaderFiles.push_back(make_pair("deferred/shadowAlphaMaskV.glsl", GL_VERTEX_SHADER));
        gDeferredShadowFullbrightAlphaMaskProgram.mShaderFiles.push_back(make_pair("deferred/shadowAlphaMaskF.glsl", GL_FRAGMENT_SHADER));

        gDeferredShadowFullbrightAlphaMaskProgram.clearPermutations();
        gDeferredShadowFullbrightAlphaMaskProgram.addPermutation("DEPTH_CLAMP", "1");
        gDeferredShadowFullbrightAlphaMaskProgram.addPermutation("IS_FULLBRIGHT", "1");

        add_common_permutations(&gDeferredShadowFullbrightAlphaMaskProgram);

        gDeferredShadowFullbrightAlphaMaskProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = make_rigged_variant(gDeferredShadowFullbrightAlphaMaskProgram, gDeferredSkinnedShadowFullbrightAlphaMaskProgram);
        success = success && gDeferredShadowFullbrightAlphaMaskProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredShadowAlphaMaskProgram.mName = "Deferred Shadow Alpha Mask Shader";
        gDeferredShadowAlphaMaskProgram.mFeatures.hasActorFxShadow = true;
        gDeferredShadowAlphaMaskProgram.mFeatures.mIndexedTextureChannels = LLGLSLShader::sIndexedTextureChannels;

        gDeferredShadowAlphaMaskProgram.mShaderFiles.clear();
        gDeferredShadowAlphaMaskProgram.mShaderFiles.push_back(make_pair("deferred/shadowAlphaMaskV.glsl", GL_VERTEX_SHADER));
        gDeferredShadowAlphaMaskProgram.mShaderFiles.push_back(make_pair("deferred/shadowAlphaMaskF.glsl", GL_FRAGMENT_SHADER));
        gDeferredShadowAlphaMaskProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = make_rigged_variant(gDeferredShadowAlphaMaskProgram, gDeferredSkinnedShadowAlphaMaskProgram);
        success = success && gDeferredShadowAlphaMaskProgram.createShader();
        llassert(success);
    }


    if (success)
    {
        gDeferredShadowGLTFAlphaMaskProgram.mName = "Deferred GLTF Shadow Alpha Mask Shader";
        gDeferredShadowGLTFAlphaMaskProgram.mFeatures.hasActorFxShadow = true;
        gDeferredShadowGLTFAlphaMaskProgram.mShaderFiles.clear();
        gDeferredShadowGLTFAlphaMaskProgram.mShaderFiles.push_back(make_pair("deferred/pbrShadowAlphaMaskV.glsl", GL_VERTEX_SHADER));
        gDeferredShadowGLTFAlphaMaskProgram.mShaderFiles.push_back(make_pair("deferred/pbrShadowAlphaMaskF.glsl", GL_FRAGMENT_SHADER));
        gDeferredShadowGLTFAlphaMaskProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gDeferredShadowGLTFAlphaMaskProgram.clearPermutations();

        add_common_permutations(&gDeferredShadowGLTFAlphaMaskProgram);

        success = make_rigged_variant(gDeferredShadowGLTFAlphaMaskProgram, gDeferredSkinnedShadowGLTFAlphaMaskProgram);
        success = success && gDeferredShadowGLTFAlphaMaskProgram.createShader();
        llassert(success);
    }

    if (success && LLGLSLShader::sIndexedGLTFChannels >= 2)
    {
        // Indexed (multi-material) shadow alpha mask, so batched mask faces alpha-test
        // per-slot in the shadow map. A failure disables indexed material formation;
        // the subsequent geometry rebuild then routes all faces through scalar shadow.
        gDeferredShadowGLTFAlphaMaskIndexedProgram.mName = "Deferred GLTF Shadow Alpha Mask Indexed Shader";
        gDeferredShadowGLTFAlphaMaskIndexedProgram.mFeatures.mIndexedMaterialChannels = LLGLSLShader::sIndexedGLTFChannels;
        gDeferredShadowGLTFAlphaMaskIndexedProgram.mFeatures.hasActorFxShadow = true;
        gDeferredShadowGLTFAlphaMaskIndexedProgram.mShaderFiles.clear();
        gDeferredShadowGLTFAlphaMaskIndexedProgram.mShaderFiles.push_back(make_pair("deferred/pbrShadowAlphaMaskIndexedV.glsl", GL_VERTEX_SHADER));
        gDeferredShadowGLTFAlphaMaskIndexedProgram.mShaderFiles.push_back(make_pair("deferred/pbrShadowAlphaMaskIndexedF.glsl", GL_FRAGMENT_SHADER));
        gDeferredShadowGLTFAlphaMaskIndexedProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gDeferredShadowGLTFAlphaMaskIndexedProgram.clearPermutations();
        gDeferredShadowGLTFAlphaMaskIndexedProgram.addPermutation("GLTF_INDEXED_CHANNELS", llformat("%d", LLGLSLShader::sIndexedGLTFChannels));
        add_common_permutations(&gDeferredShadowGLTFAlphaMaskIndexedProgram);

        bool shadow_indexed_ok = make_rigged_variant(gDeferredShadowGLTFAlphaMaskIndexedProgram, gDeferredSkinnedShadowGLTFAlphaMaskIndexedProgram);
        if (shadow_indexed_ok)
        {
            shadow_indexed_ok = gDeferredShadowGLTFAlphaMaskIndexedProgram.createShader();
        }

        if (shadow_indexed_ok)
        { // only base color is sampled for the shadow alpha test
            const S32 n = LLGLSLShader::sIndexedGLTFChannels;
            setup_gltf_indexed_samplers(gDeferredShadowGLTFAlphaMaskIndexedProgram, n, false);
            setup_gltf_indexed_samplers(gDeferredSkinnedShadowGLTFAlphaMaskIndexedProgram, n, false);
        }
        else
        {
            LL_WARNS("ShaderLoading") << "Indexed PBR shadow alpha mask shader failed to load; GLTF batching disabled." << LL_ENDL;
            disable_indexed_gltf_batching();
        }
    }

    if (success && LLGLSLShader::sIndexedLegacyMaterials)
    {
        // Indexed (multi-material) legacy material shadow alpha mask, so batched
        // masked legacy faces alpha-test per-slot in the shadow map. Required when
        // legacy batching is on: a failure here would leave indexed mask batches
        // casting no shadow (skipped by the scalar pass, no indexed sweep), so on
        // failure we disable legacy batching entirely rather than degrade silently.
        gDeferredShadowMaterialIndexedProgram.mName = "Deferred Material Shadow Indexed Shader";
        gDeferredShadowMaterialIndexedProgram.mFeatures.mIndexedMaterialChannels = LLGLSLShader::sIndexedGLTFChannels;
        gDeferredShadowMaterialIndexedProgram.mFeatures.hasActorFxShadow = true;
        gDeferredShadowMaterialIndexedProgram.mShaderFiles.clear();
        gDeferredShadowMaterialIndexedProgram.mShaderFiles.push_back(make_pair("deferred/materialShadowIndexedV.glsl", GL_VERTEX_SHADER));
        gDeferredShadowMaterialIndexedProgram.mShaderFiles.push_back(make_pair("deferred/materialShadowIndexedF.glsl", GL_FRAGMENT_SHADER));
        gDeferredShadowMaterialIndexedProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gDeferredShadowMaterialIndexedProgram.clearPermutations();
        gDeferredShadowMaterialIndexedProgram.addPermutation("GLTF_INDEXED_CHANNELS", llformat("%d", LLGLSLShader::sIndexedGLTFChannels));

        bool mat_shadow_ok = make_rigged_variant(gDeferredShadowMaterialIndexedProgram, gDeferredSkinnedShadowMaterialIndexedProgram);
        if (mat_shadow_ok)
        {
            mat_shadow_ok = gDeferredShadowMaterialIndexedProgram.createShader();
        }

        if (mat_shadow_ok)
        { // only diffuse is sampled for the shadow alpha test
            const S32 n = LLGLSLShader::sIndexedGLTFChannels;
            setup_material_indexed_samplers(gDeferredShadowMaterialIndexedProgram, n, false, false);
            setup_material_indexed_samplers(gDeferredSkinnedShadowMaterialIndexedProgram, n, false, false);
        }
        else
        {
            LL_WARNS("ShaderLoading") << "Indexed legacy material shadow shader failed to load; legacy batching disabled." << LL_ENDL;
            gDeferredShadowMaterialIndexedProgram.unload();
            gDeferredSkinnedShadowMaterialIndexedProgram.unload();
            LLGLSLShader::sIndexedLegacyMaterials = false; // can't shadow indexed batches -- don't form them
        }
    }

    if (success)
    {
        gDeferredShadowGLTFAlphaBlendProgram.mName = "Deferred GLTF Shadow Alpha Blend Shader";
        gDeferredShadowGLTFAlphaBlendProgram.mFeatures.hasActorFxShadow = true;
        gDeferredShadowGLTFAlphaBlendProgram.mShaderFiles.clear();
        gDeferredShadowGLTFAlphaBlendProgram.mShaderFiles.push_back(make_pair("deferred/pbrShadowAlphaMaskV.glsl", GL_VERTEX_SHADER));
        gDeferredShadowGLTFAlphaBlendProgram.mShaderFiles.push_back(make_pair("deferred/pbrShadowAlphaBlendF.glsl", GL_FRAGMENT_SHADER));
        gDeferredShadowGLTFAlphaBlendProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gDeferredShadowGLTFAlphaBlendProgram.clearPermutations();

        add_common_permutations(&gDeferredShadowGLTFAlphaBlendProgram);

        success = make_rigged_variant(gDeferredShadowGLTFAlphaBlendProgram, gDeferredSkinnedShadowGLTFAlphaBlendProgram);
        success = success && gDeferredShadowGLTFAlphaBlendProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredAvatarShadowProgram.mName = "Deferred Avatar Shadow Shader";
        gDeferredAvatarShadowProgram.mFeatures.hasSkinning = true;
        gDeferredAvatarShadowProgram.mFeatures.hasActorFxShadow = true;

        gDeferredAvatarShadowProgram.mShaderFiles.clear();
        gDeferredAvatarShadowProgram.mShaderFiles.push_back(make_pair("deferred/avatarShadowV.glsl", GL_VERTEX_SHADER));
        gDeferredAvatarShadowProgram.mShaderFiles.push_back(make_pair("deferred/avatarShadowF.glsl", GL_FRAGMENT_SHADER));
        gDeferredAvatarShadowProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gDeferredAvatarShadowProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredAvatarAlphaShadowProgram.mName = "Deferred Avatar Alpha Shadow Shader";
        gDeferredAvatarAlphaShadowProgram.mFeatures.hasSkinning = true;
        gDeferredAvatarAlphaShadowProgram.mFeatures.hasActorFxShadow = true;
        gDeferredAvatarAlphaShadowProgram.mShaderFiles.clear();
        gDeferredAvatarAlphaShadowProgram.mShaderFiles.push_back(make_pair("deferred/avatarAlphaShadowV.glsl", GL_VERTEX_SHADER));
        gDeferredAvatarAlphaShadowProgram.mShaderFiles.push_back(make_pair("deferred/avatarAlphaShadowF.glsl", GL_FRAGMENT_SHADER));
        gDeferredAvatarAlphaShadowProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gDeferredAvatarAlphaShadowProgram.createShader();
        llassert(success);
    }
    if (success)
    {
        gDeferredAvatarAlphaMaskShadowProgram.mName = "Deferred Avatar Alpha Mask Shadow Shader";
        gDeferredAvatarAlphaMaskShadowProgram.mFeatures.hasSkinning  = true;
        gDeferredAvatarAlphaMaskShadowProgram.mFeatures.hasActorFxShadow = true;
        gDeferredAvatarAlphaMaskShadowProgram.mShaderFiles.clear();
        gDeferredAvatarAlphaMaskShadowProgram.mShaderFiles.push_back(make_pair("deferred/avatarAlphaShadowV.glsl", GL_VERTEX_SHADER));
        gDeferredAvatarAlphaMaskShadowProgram.mShaderFiles.push_back(make_pair("deferred/avatarAlphaMaskShadowF.glsl", GL_FRAGMENT_SHADER));
        gDeferredAvatarAlphaMaskShadowProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gDeferredAvatarAlphaMaskShadowProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredTerrainProgram.mName = "Deferred Terrain Shader";
        gDeferredTerrainProgram.mFeatures.hasSrgb = true;
        gDeferredTerrainProgram.mFeatures.isAlphaLighting = true;
        gDeferredTerrainProgram.mFeatures.calculatesAtmospherics = true;
        gDeferredTerrainProgram.mFeatures.hasAtmospherics = true;
        gDeferredTerrainProgram.mFeatures.hasGamma = true;

        gDeferredTerrainProgram.mShaderFiles.clear();
        gDeferredTerrainProgram.mShaderFiles.push_back(make_pair("deferred/terrainV.glsl", GL_VERTEX_SHADER));
        gDeferredTerrainProgram.mShaderFiles.push_back(make_pair("deferred/terrainF.glsl", GL_FRAGMENT_SHADER));

        add_common_permutations(&gDeferredTerrainProgram);

        gDeferredTerrainProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gDeferredTerrainProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredAvatarProgram.mName = "Deferred Avatar Shader";
        gDeferredAvatarProgram.mFeatures.hasSkinning = true;
        gDeferredAvatarProgram.mFeatures.hasSrgb = true;
        gDeferredAvatarProgram.mFeatures.hasActorFx = true;
        gDeferredAvatarProgram.mShaderFiles.clear();
        gDeferredAvatarProgram.mShaderFiles.push_back(make_pair("deferred/avatarV.glsl", GL_VERTEX_SHADER));
        gDeferredAvatarProgram.mShaderFiles.push_back(make_pair("deferred/avatarF.glsl", GL_FRAGMENT_SHADER));
        gDeferredAvatarProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];

        gDeferredAvatarProgram.clearPermutations();
        gDeferredAvatarProgram.addPermutation("HAS_ACTOR_FX", "1");
        add_common_permutations(&gDeferredAvatarProgram);
        if (gSavedSettings.getBOOL("RenderAvatarCloth"))
        {
            gDeferredAvatarProgram.addPermutation("AVATAR_CLOTH", "1");
        }

        success = gDeferredAvatarProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredAvatarAlphaProgram.mName = "Deferred Avatar Alpha Shader";
        gDeferredAvatarAlphaProgram.mFeatures.hasSkinning = true;
        gDeferredAvatarAlphaProgram.mFeatures.calculatesLighting = false;
        gDeferredAvatarAlphaProgram.mFeatures.hasLighting = false;
        gDeferredAvatarAlphaProgram.mFeatures.isAlphaLighting = true;
        gDeferredAvatarAlphaProgram.mFeatures.hasSrgb = true;
        gDeferredAvatarAlphaProgram.mFeatures.calculatesAtmospherics = true;
        gDeferredAvatarAlphaProgram.mFeatures.hasAtmospherics = true;
        gDeferredAvatarAlphaProgram.mFeatures.hasGamma = true;
        gDeferredAvatarAlphaProgram.mFeatures.isDeferred = true;
        gDeferredAvatarAlphaProgram.mFeatures.hasShadows = true;
        gDeferredAvatarAlphaProgram.mFeatures.hasReflectionProbes = true;
        gDeferredAvatarAlphaProgram.mFeatures.hasActorFx = true;

        gDeferredAvatarAlphaProgram.mShaderFiles.clear();
        gDeferredAvatarAlphaProgram.mShaderFiles.push_back(make_pair("deferred/alphaV.glsl", GL_VERTEX_SHADER));
        gDeferredAvatarAlphaProgram.mShaderFiles.push_back(make_pair("deferred/alphaF.glsl", GL_FRAGMENT_SHADER));

        gDeferredAvatarAlphaProgram.clearPermutations();
        gDeferredAvatarAlphaProgram.addPermutation("HAS_ACTOR_FX", "1");
        gDeferredAvatarAlphaProgram.addPermutation("USE_DIFFUSE_TEX", "1");
        gDeferredAvatarAlphaProgram.addPermutation("IS_AVATAR_SKIN", "1");
        if (use_sun_shadow)
        {
            gDeferredAvatarAlphaProgram.addPermutation("HAS_SUN_SHADOW", "1");
        }

        gDeferredAvatarAlphaProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];

        add_common_permutations(&gDeferredAvatarAlphaProgram);

        success = gDeferredAvatarAlphaProgram.createShader();
        llassert(success);

        gDeferredAvatarAlphaProgram.mFeatures.calculatesLighting = true;
        gDeferredAvatarAlphaProgram.mFeatures.hasLighting = true;
    }

    if (success)
    {
        gExposureProgram.mName = "Exposure";
        gExposureProgram.mFeatures.hasSrgb = true;
        gExposureProgram.mFeatures.isDeferred = true;
        gExposureProgram.mShaderFiles.clear();
        gExposureProgram.clearPermutations();
        gExposureProgram.addPermutation("USE_LAST_EXPOSURE", "1");
        gExposureProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gExposureProgram.mShaderFiles.push_back(make_pair("deferred/exposureF.glsl", GL_FRAGMENT_SHADER));
        gExposureProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gExposureProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gExposureProgramNoFade.mName = "Exposure (no fade)";
        gExposureProgramNoFade.mFeatures.hasSrgb = true;
        gExposureProgramNoFade.mFeatures.isDeferred = true;
        gExposureProgramNoFade.mShaderFiles.clear();
        gExposureProgramNoFade.clearPermutations();
        gExposureProgramNoFade.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gExposureProgramNoFade.mShaderFiles.push_back(make_pair("deferred/exposureF.glsl", GL_FRAGMENT_SHADER));
        gExposureProgramNoFade.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gExposureProgramNoFade.createShader();
        llassert(success);
    }

    if (success)
    {
        gLuminanceProgram.mName = "Luminance";
        gLuminanceProgram.mShaderFiles.clear();
        gLuminanceProgram.clearPermutations();
        gLuminanceProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gLuminanceProgram.mShaderFiles.push_back(make_pair("deferred/luminanceF.glsl", GL_FRAGMENT_SHADER));
        gLuminanceProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gLuminanceProgram.createShader();
        llassert(success);
    }

    if (success && gGLManager.mGLVersion > 3.9f)
    {
        std::vector<std::pair<std::string, std::string>> quality_levels = { {"12", "Low"},
                                                                             {"23", "Medium"},
                                                                             {"28", "High"},
                                                                             {"39", "Ultra"} };
        int i = 0;
        bool failed = false;
        for (const auto& quality_pair : quality_levels)
        {
            if (success)
            {
                gFXAAProgram[i].mName = llformat("FXAA Shader (%s)", quality_pair.second.c_str());
                gFXAAProgram[i].mFeatures.isDeferred = true;
                gFXAAProgram[i].mShaderFiles.clear();
                gFXAAProgram[i].mShaderFiles.push_back(make_pair("deferred/postDeferredV.glsl", GL_VERTEX_SHADER));
                gFXAAProgram[i].mShaderFiles.push_back(make_pair("deferred/fxaaF.glsl", GL_FRAGMENT_SHADER));

                gFXAAProgram[i].clearPermutations();
                gFXAAProgram[i].addPermutation("FXAA_QUALITY__PRESET", quality_pair.first);
                if (gGLManager.mGLVersion > 3.9)
                {
                    gFXAAProgram[i].addPermutation("FXAA_GLSL_400", "1");
                }
                else
                {
                    gFXAAProgram[i].addPermutation("FXAA_GLSL_130", "1");
                }

                gFXAAProgram[i].mShaderLevel = mShaderLevel[SHADER_DEFERRED];
                success = gFXAAProgram[i].createShader();
                // llassert(success);
                if (!success)
                {
                    LL_WARNS() << "Failed to create shader '" << gFXAAProgram[i].mName << "', disabling!" << LL_ENDL;
                    // continue as if this shader never happened
                    failed = true;
                    success = true;
                    break;
                }
            }
            ++i;
        }

        if (failed)
        {
            for (auto i = 0; i < 4; ++i)
            {
                gFXAAProgram[i].unload();
            }
        }
    }

    if (gGLManager.mGLVersion > 3.15f && success)
    {
        std::vector<std::pair<std::string, std::string>> quality_levels = { {"SMAA_PRESET_LOW", "Low"},
                                                                             {"SMAA_PRESET_MEDIUM", "Medium"},
                                                                             {"SMAA_PRESET_HIGH", "High"},
                                                                          {"SMAA_PRESET_ULTRA", "Ultra"} };
        const bool smaa_predication = gSavedSettings.getBOOL("RenderSMAAPredication");
        const F32 smaa_pred_threshold = gSavedSettings.getF32("RenderSMAAPredicationThreshold");
        const F32 smaa_pred_scale = llclamp(gSavedSettings.getF32("RenderSMAAPredicationScale"), 1.f, 5.f);
        const F32 smaa_pred_strength = llclamp(gSavedSettings.getF32("RenderSMAAPredicationStrength"), 0.f, 1.f);
        int i = 0;
        bool failed = false;
        for (const auto& smaa_pair : quality_levels)
        {
            std::map<std::string, std::string> defines;
            if (gGLManager.mGLVersion >= 4.f)
                defines.emplace("SMAA_GLSL_4", "1");
            else if (gGLManager.mGLVersion >= 3.1f)
                defines.emplace("SMAA_GLSL_3", "1");
            else
                defines.emplace("SMAA_GLSL_2", "1");
            defines.emplace("SMAA_PREDICATION", smaa_predication ? "1" : "0");
            if (smaa_predication)
            {
                defines.emplace("SMAA_PREDICATION_THRESHOLD", llformat("%.6f", smaa_pred_threshold));
                defines.emplace("SMAA_PREDICATION_SCALE", llformat("%.3f", smaa_pred_scale));
                defines.emplace("SMAA_PREDICATION_STRENGTH", llformat("%.3f", smaa_pred_strength));
            }
            defines.emplace("SMAA_REPROJECTION", "0");
            defines.emplace(smaa_pair.first, "1");

            if (success)
            {
                gSMAAEdgeDetectProgram[i].mName = llformat("SMAA Edge Detection (%s)", smaa_pair.second.c_str());
                gSMAAEdgeDetectProgram[i].mFeatures.isDeferred = true;

                gSMAAEdgeDetectProgram[i].clearPermutations();
                gSMAAEdgeDetectProgram[i].addPermutations(defines);

                gSMAAEdgeDetectProgram[i].mShaderFiles.clear();
                gSMAAEdgeDetectProgram[i].mShaderFiles.push_back(make_pair("deferred/SMAAEdgeDetectF.glsl", GL_FRAGMENT_SHADER));
                gSMAAEdgeDetectProgram[i].mShaderFiles.push_back(make_pair("deferred/SMAAEdgeDetectV.glsl", GL_VERTEX_SHADER));
                gSMAAEdgeDetectProgram[i].mShaderFiles.push_back(make_pair("deferred/SMAA.glsl", GL_FRAGMENT_SHADER));
                gSMAAEdgeDetectProgram[i].mShaderFiles.push_back(make_pair("deferred/SMAA.glsl", GL_VERTEX_SHADER));
                gSMAAEdgeDetectProgram[i].mShaderLevel = mShaderLevel[SHADER_DEFERRED];
                success = gSMAAEdgeDetectProgram[i].createShader();
                // llassert(success);
                if (!success)
                {
                    LL_WARNS() << "Failed to create shader '" << gSMAAEdgeDetectProgram[i].mName << "', disabling!" << LL_ENDL;
                    // continue as if this shader never happened
                    failed = true;
                    success = true;
                    break;
                }
            }

            if (success)
            {
                gSMAABlendWeightsProgram[i].mName = llformat("SMAA Blending Weights (%s)", smaa_pair.second.c_str());
                gSMAABlendWeightsProgram[i].mFeatures.isDeferred = true;

                gSMAABlendWeightsProgram[i].clearPermutations();
                gSMAABlendWeightsProgram[i].addPermutations(defines);

                gSMAABlendWeightsProgram[i].mShaderFiles.clear();
                gSMAABlendWeightsProgram[i].mShaderFiles.push_back(make_pair("deferred/SMAABlendWeightsF.glsl", GL_FRAGMENT_SHADER));
                gSMAABlendWeightsProgram[i].mShaderFiles.push_back(make_pair("deferred/SMAABlendWeightsV.glsl", GL_VERTEX_SHADER));
                gSMAABlendWeightsProgram[i].mShaderFiles.push_back(make_pair("deferred/SMAA.glsl", GL_FRAGMENT_SHADER));
                gSMAABlendWeightsProgram[i].mShaderFiles.push_back(make_pair("deferred/SMAA.glsl", GL_VERTEX_SHADER));
                gSMAABlendWeightsProgram[i].mShaderLevel = mShaderLevel[SHADER_DEFERRED];
                success = gSMAABlendWeightsProgram[i].createShader();
                // llassert(success);
                if (!success)
                {
                    LL_WARNS() << "Failed to create shader '" << gSMAABlendWeightsProgram[i].mName << "', disabling!" << LL_ENDL;
                    // continue as if this shader never happened
                    failed = true;
                    success = true;
                    break;
                }
            }

            if (success)
            {
                gSMAANeighborhoodBlendProgram[i].mName = llformat("SMAA Neighborhood Blending (%s)", smaa_pair.second.c_str());
                gSMAANeighborhoodBlendProgram[i].mFeatures.isDeferred = true;

                gSMAANeighborhoodBlendProgram[i].clearPermutations();
                gSMAANeighborhoodBlendProgram[i].addPermutations(defines);

                gSMAANeighborhoodBlendProgram[i].mShaderFiles.clear();
                gSMAANeighborhoodBlendProgram[i].mShaderFiles.push_back(make_pair("deferred/SMAANeighborhoodBlendF.glsl", GL_FRAGMENT_SHADER));
                gSMAANeighborhoodBlendProgram[i].mShaderFiles.push_back(make_pair("deferred/SMAANeighborhoodBlendV.glsl", GL_VERTEX_SHADER));
                gSMAANeighborhoodBlendProgram[i].mShaderFiles.push_back(make_pair("deferred/SMAA.glsl", GL_FRAGMENT_SHADER));
                gSMAANeighborhoodBlendProgram[i].mShaderFiles.push_back(make_pair("deferred/SMAA.glsl", GL_VERTEX_SHADER));
                gSMAANeighborhoodBlendProgram[i].mShaderLevel = mShaderLevel[SHADER_DEFERRED];
                success = gSMAANeighborhoodBlendProgram[i].createShader();
                // llassert(success);
                if (!success)
                {
                    LL_WARNS() << "Failed to create shader '" << gSMAANeighborhoodBlendProgram[i].mName << "', disabling!" << LL_ENDL;
                    // continue as if this shader never happened
                    failed = true;
                    success = true;
                    break;
                }
            }
            ++i;
        }

        if (failed)
        {
            for (auto i = 0; i < 4; ++i)
            {
                gSMAAEdgeDetectProgram[i].unload();
                gSMAABlendWeightsProgram[i].unload();
                gSMAANeighborhoodBlendProgram[i].unload();
            }
        }
    }

    if (success)
    {
        gCineOutlineProgram.mName = "Cine Outline Shader";
        gCineOutlineProgram.mFeatures.isDeferred = true;      // attaches deferredUtil.glsl (getPosition/getDepth)
        gCineOutlineProgram.mFeatures.hasFullGBuffer = true;  // attaches gbufferUtil.glsl (getNorm/GET_GBUFFER_FLAG)
        gCineOutlineProgram.mShaderFiles.clear();
        gCineOutlineProgram.clearPermutations();
        gCineOutlineProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gCineOutlineProgram.mShaderFiles.push_back(make_pair("deferred/cineOutlineF.glsl", GL_FRAGMENT_SHADER));
        gCineOutlineProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gCineOutlineProgram.createShader();
        if (!success)
        {
            LL_WARNS() << "Failed to create shader '" << gCineOutlineProgram.mName << "', disabling!" << LL_ENDL;
            success = true;
        }
    }

    if (success)
    {
        gCineFisheyeProgram.mName = "Cine Fisheye Shader";
        gCineFisheyeProgram.mFeatures.isDeferred = true;
        gCineFisheyeProgram.mShaderFiles.clear();
        gCineFisheyeProgram.clearPermutations();
        gCineFisheyeProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gCineFisheyeProgram.mShaderFiles.push_back(make_pair("deferred/cineFisheyeF.glsl", GL_FRAGMENT_SHADER));
        gCineFisheyeProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gCineFisheyeProgram.createShader();
        if (!success)
        {
            LL_WARNS() << "Failed to create shader '" << gCineFisheyeProgram.mName << "', disabling!" << LL_ENDL;
            success = true;
        }
    }

    if (success)
    {
        // [Ultimate Diopter] pass 1: dual-focus MRT gather, four fixed
        // tap-count tiers so drivers see compile-time loop bounds.
        static const char* diopter_taps[4] = { "12", "24", "40", "64" };
        for (U32 i = 0; i < 4; ++i)
        {
            LLGLSLShader& p = gUltimateDiopterGatherProgram[i];
            p.mName = llformat("Ultimate Diopter Gather Shader %s taps", diopter_taps[i]);
            p.mFeatures.isDeferred = true;
            p.mShaderFiles.clear();
            p.clearPermutations();
            p.addPermutation("DIOPTER_TAPS", diopter_taps[i]);
            p.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
            p.mShaderFiles.push_back(make_pair("deferred/ultimateDiopterGatherF.glsl", GL_FRAGMENT_SHADER));
            p.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
            success = p.createShader();
            if (!success)
            {
                LL_WARNS() << "Failed to create shader '" << p.mName << "', disabling!" << LL_ENDL;
                success = true;
            }
        }

        // [Ultimate Diopter] pass 2: composite (seam ghost, halo ghosts,
        // dispersion, rim, vignette, debug views, master blend).
        gUltimateDiopterProgram.mName = "Ultimate Diopter Composite Shader";
        gUltimateDiopterProgram.mFeatures.isDeferred = true;
        gUltimateDiopterProgram.mShaderFiles.clear();
        gUltimateDiopterProgram.clearPermutations();
        gUltimateDiopterProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gUltimateDiopterProgram.mShaderFiles.push_back(make_pair("deferred/ultimateDiopterF.glsl", GL_FRAGMENT_SHADER));
        gUltimateDiopterProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gUltimateDiopterProgram.createShader();
        if (!success)
        {
            LL_WARNS() << "Failed to create shader '" << gUltimateDiopterProgram.mName << "', disabling!" << LL_ENDL;
            success = true;
        }

        // [Ultimate Kaleidoscope] tool mode 1 of the Ultimate Diopter post
        // pass: single full-screen MRT pass, no permutations. Same vertex
        // shader + registration pattern as gUltimateDiopterProgram.
        gUltimateKaleidoProgram.mName = "Ultimate Kaleidoscope Shader";
        gUltimateKaleidoProgram.mFeatures.isDeferred = true;
        gUltimateKaleidoProgram.mShaderFiles.clear();
        gUltimateKaleidoProgram.clearPermutations();
        gUltimateKaleidoProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gUltimateKaleidoProgram.mShaderFiles.push_back(make_pair("deferred/ultimateKaleidoF.glsl", GL_FRAGMENT_SHADER));
        gUltimateKaleidoProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gUltimateKaleidoProgram.createShader();
        if (!success)
        {
            LL_WARNS() << "Failed to create shader '" << gUltimateKaleidoProgram.mName << "', disabling!" << LL_ENDL;
            success = true;
        }
    }

    if (success && gGLManager.mGLVersion > 4.05f)
    {
        // [BDMerge G3.2] volumetric lighting / godrays. Donor: Black Dragon
        // (NiranV Dean, Tofu Buzzard lineage). Real scattering shader is
        // class3; class1 is a passthrough stub for low shader levels.
        // Fix-forward vs donor: GODRAYS_FADE permutation applied to THIS
        // program (BD applies it to the soften program - a wiring bug; the
        // #if lives in volumetricLightF.glsl).
        gVolumetricLightProgram.mName = "Volumetric Light Shader";
        gVolumetricLightProgram.mFeatures.isDeferred = true;
        gVolumetricLightProgram.mFeatures.calculatesAtmospherics = true;
        gVolumetricLightProgram.mFeatures.hasAtmospherics = true;
        gVolumetricLightProgram.mFeatures.hasShadows = true;
        gVolumetricLightProgram.mShaderFiles.clear();
        gVolumetricLightProgram.clearPermutations();
        gVolumetricLightProgram.addPermutation("SUN_SHADOW", "1");
        if (gSavedSettings.getBOOL("RenderVolumetricLightingDirectional"))
        {
            gVolumetricLightProgram.addPermutation("GODRAYS_FADE", "1");
        }
        gVolumetricLightProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gVolumetricLightProgram.mShaderFiles.push_back(make_pair("deferred/volumetricLightF.glsl", GL_FRAGMENT_SHADER));
        gVolumetricLightProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gVolumetricLightProgram.createShader();
        if (!success)
        {
            LL_WARNS() << "Failed to create shader '" << gVolumetricLightProgram.mName << "', disabling!" << LL_ENDL;
            success = true;
        }

        // [BDMerge G3.3] per-projector volumetric light cones (visible spotlight
        // shafts). NET-NEW local-light companion to G3.2. Same feature/util set
        // as the sun godray program so the shared deferredUtil/shadowUtil externs
        // resolve identically; SPOT_SHADOW=1 (instead of the sun's SUN_SHADOW)
        // pulls in shadowUtil's indexed projector-shadow dispatch (sampleSpotShadow,
        // shadowMap4-9). Real march is class3; class1 is an additive-safe no-op
        // (outputs black) for shader levels below 3.
        gDeferredProjectorVolumetricProgram.mName = "Projector Volumetric Light Shader";
        gDeferredProjectorVolumetricProgram.mFeatures.isDeferred = true;
        gDeferredProjectorVolumetricProgram.mFeatures.calculatesAtmospherics = true;
        gDeferredProjectorVolumetricProgram.mFeatures.hasAtmospherics = true;
        gDeferredProjectorVolumetricProgram.mFeatures.hasShadows = true;
        gDeferredProjectorVolumetricProgram.mShaderFiles.clear();
        gDeferredProjectorVolumetricProgram.clearPermutations();
        gDeferredProjectorVolumetricProgram.addPermutation("SPOT_SHADOW", "1");
        // [BDMerge G3.3 Dust / review fix] Dust is a compile-time permutation:
        // with the lever OFF the sampler3D projvol_dust_map (and all dust code)
        // does not exist in the program, so no fragment texture unit is consumed
        // in this already sampler-heavy shader. Toggling the setting rebuilds
        // shaders (handleSetShaderChanged listener in llviewercontrol.cpp) -
        // same settings-driven pattern as GODRAYS_FADE above.
        if (gSavedSettings.getBOOL("BDMergeProjectorVolumetricsDust"))
        {
            gDeferredProjectorVolumetricProgram.addPermutation("PROJVOL_DUST_ENABLE", "1");
        }
        // [BDMerge G3.3 ConservativeShadow / round-2 review fix] The conservative
        // airborne-shadow experiment is likewise a compile-time permutation: with
        // the lever OFF (default) the gate uniform, the on-axis guards, the
        // conservative shadow dispatch and the guarded normalize do not exist in
        // the program at all, so the default-off march is instruction-identical
        // to the legacy path (zero cost) rather than merely equivalent. Toggling
        // the setting rebuilds shaders (handleSetShaderChanged listener in
        // llviewercontrol.cpp), and the define feeds the program-binary cache
        // key through mDefines (LLGLSLShader::hash), so both permutations cache
        // and rebuild correctly.
        if (gSavedSettings.getBOOL("BDMergeProjectorVolumetricsConservativeShadow"))
        {
            gDeferredProjectorVolumetricProgram.addPermutation("PROJVOL_CONSERVATIVE_SHADOW", "1");
        }
        gDeferredProjectorVolumetricProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gDeferredProjectorVolumetricProgram.mShaderFiles.push_back(make_pair("deferred/projectorVolumetricF.glsl", GL_FRAGMENT_SHADER));
        gDeferredProjectorVolumetricProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gDeferredProjectorVolumetricProgram.createShader();
        if (!success)
        {
            LL_WARNS() << "Failed to create shader '" << gDeferredProjectorVolumetricProgram.mName << "', disabling!" << LL_ENDL;
            success = true;
        }

        // [BDMerge G3.3 Phase 1 item 3] depth-aware bilateral upsample that resolves
        // the half-res projector-volumetric march back to full resolution and
        // composites it additively onto the linear HDR scene buffer. isDeferred so
        // getPosition()/depthMap/inv_proj resolve for the depth weighting; no
        // shadow set needed (the half-res pass already baked self-shadowing).
        gDeferredProjectorVolumetricUpsampleProgram.mName = "Projector Volumetric Upsample Shader";
        gDeferredProjectorVolumetricUpsampleProgram.mFeatures.isDeferred = true;
        gDeferredProjectorVolumetricUpsampleProgram.mShaderFiles.clear();
        gDeferredProjectorVolumetricUpsampleProgram.clearPermutations();
        gDeferredProjectorVolumetricUpsampleProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gDeferredProjectorVolumetricUpsampleProgram.mShaderFiles.push_back(make_pair("deferred/projectorVolumetricUpsampleF.glsl", GL_FRAGMENT_SHADER));
        gDeferredProjectorVolumetricUpsampleProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gDeferredProjectorVolumetricUpsampleProgram.createShader();
        if (!success)
        {
            LL_WARNS() << "Failed to create shader '" << gDeferredProjectorVolumetricUpsampleProgram.mName << "', disabling!" << LL_ENDL;
            success = true;
        }

        // [BDMerge G3.3 Batch 1 A] Temporal reprojection resolve shader: blends the
        // freshly-marched half-res shaft with the reprojected previous accumulation
        // (neighborhood-clamped, depth/cut-rejected) into the history slot. Deferred
        // so getPosition()/depthMap/inv_proj resolve for reprojection + disocclusion.
        gDeferredProjectorVolumetricTemporalProgram.mName = "Projector Volumetric Temporal Shader";
        gDeferredProjectorVolumetricTemporalProgram.mFeatures.isDeferred = true;
        gDeferredProjectorVolumetricTemporalProgram.mShaderFiles.clear();
        gDeferredProjectorVolumetricTemporalProgram.clearPermutations();
        gDeferredProjectorVolumetricTemporalProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gDeferredProjectorVolumetricTemporalProgram.mShaderFiles.push_back(make_pair("deferred/projectorVolumetricTemporalF.glsl", GL_FRAGMENT_SHADER));
        gDeferredProjectorVolumetricTemporalProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gDeferredProjectorVolumetricTemporalProgram.createShader();
        if (!success)
        {
            LL_WARNS() << "Failed to create shader '" << gDeferredProjectorVolumetricTemporalProgram.mName << "', disabling!" << LL_ENDL;
            success = true;
        }

        // [BDMerge G3.3 Phase 3 item 4] Bloom-feed shader: a small tent blur of the
        // half-res projector-volumetric shaft, scaled by projvol_bloom_feed, additively
        // composited into the HDR bloom pyramid base so bright shaft cores gain a soft
        // glow halo. Not deferred (plain 2D texture pass, no depth reconstruction).
        gDeferredProjectorVolumetricBloomFeedProgram.mName = "Projector Volumetric Bloom Feed Shader";
        gDeferredProjectorVolumetricBloomFeedProgram.mShaderFiles.clear();
        gDeferredProjectorVolumetricBloomFeedProgram.clearPermutations();
        gDeferredProjectorVolumetricBloomFeedProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gDeferredProjectorVolumetricBloomFeedProgram.mShaderFiles.push_back(make_pair("deferred/projectorVolumetricBloomFeedF.glsl", GL_FRAGMENT_SHADER));
        gDeferredProjectorVolumetricBloomFeedProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gDeferredProjectorVolumetricBloomFeedProgram.createShader();
        if (!success)
        {
            LL_WARNS() << "Failed to create shader '" << gDeferredProjectorVolumetricBloomFeedProgram.mName << "', disabling!" << LL_ENDL;
            success = true;
        }

        // Cinematic weather is an optional deferred subsystem. Each program
        // fails soft so unsupported hardware keeps the rest of deferred rendering.
        gDeferredWeatherRainProgram.mName = "Weather Rain Shader";
        gDeferredWeatherRainProgram.mFeatures.isDeferred = true;
        gDeferredWeatherRainProgram.mShaderFiles.clear();
        gDeferredWeatherRainProgram.clearPermutations();
        gDeferredWeatherRainProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gDeferredWeatherRainProgram.mShaderFiles.push_back(make_pair("deferred/weatherRainF.glsl", GL_FRAGMENT_SHADER));
        gDeferredWeatherRainProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gDeferredWeatherRainProgram.createShader();
        if (!success)
        {
            LL_WARNS() << "Failed to create shader '" << gDeferredWeatherRainProgram.mName << "', disabling weather rain." << LL_ENDL;
            success = true;
        }

        const bool rain_occlusion =
            gSavedSettings.getBOOL("AlchemyWeatherRainOcclusion");
        if (rain_occlusion)
        {
            // Compile the occlusion path as a separate optional permutation.
            // The baseline program stays available if this sampler-heavy
            // variant exceeds a texture-unit/link limit or otherwise fails.
            gDeferredWeatherRainOcclusionProgram.mName = "Weather Rain Occlusion Shader";
            gDeferredWeatherRainOcclusionProgram.mFeatures.isDeferred = true;
            gDeferredWeatherRainOcclusionProgram.mShaderFiles.clear();
            gDeferredWeatherRainOcclusionProgram.clearPermutations();
            gDeferredWeatherRainOcclusionProgram.addPermutation("WEATHER_RAIN_OCCLUSION", "1");
            gDeferredWeatherRainOcclusionProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
            gDeferredWeatherRainOcclusionProgram.mShaderFiles.push_back(make_pair("deferred/weatherRainF.glsl", GL_FRAGMENT_SHADER));
            gDeferredWeatherRainOcclusionProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
            success = gDeferredWeatherRainOcclusionProgram.createShader();
            if (!success)
            {
                LL_WARNS() << "Failed to create shader '" << gDeferredWeatherRainOcclusionProgram.mName << "', using baseline weather rain." << LL_ENDL;
                success = true;
            }
        }
        else
        {
            gDeferredWeatherRainOcclusionProgram.unload();
        }

        gDeferredWeatherRainUpsampleProgram.mName = "Weather Rain Upsample Shader";
        gDeferredWeatherRainUpsampleProgram.mFeatures.isDeferred = true;
        gDeferredWeatherRainUpsampleProgram.mShaderFiles.clear();
        gDeferredWeatherRainUpsampleProgram.clearPermutations();
        gDeferredWeatherRainUpsampleProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gDeferredWeatherRainUpsampleProgram.mShaderFiles.push_back(make_pair("deferred/weatherRainUpsampleF.glsl", GL_FRAGMENT_SHADER));
        gDeferredWeatherRainUpsampleProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gDeferredWeatherRainUpsampleProgram.createShader();
        if (!success)
        {
            LL_WARNS() << "Failed to create shader '" << gDeferredWeatherRainUpsampleProgram.mName << "', disabling weather upsample." << LL_ENDL;
            success = true;
        }

        gDeferredWeatherSurfaceProgram.mName = "Weather Surface Response Shader";
        gDeferredWeatherSurfaceProgram.mFeatures.isDeferred = true;
        gDeferredWeatherSurfaceProgram.mShaderFiles.clear();
        gDeferredWeatherSurfaceProgram.clearPermutations();
        gDeferredWeatherSurfaceProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gDeferredWeatherSurfaceProgram.mShaderFiles.push_back(make_pair("deferred/weatherSurfaceF.glsl", GL_FRAGMENT_SHADER));
        gDeferredWeatherSurfaceProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gDeferredWeatherSurfaceProgram.createShader();
        if (!success)
        {
            LL_WARNS() << "Failed to create shader '" << gDeferredWeatherSurfaceProgram.mName << "', disabling weather surface response." << LL_ENDL;
            success = true;
        }

        if (rain_occlusion)
        {
            gDeferredWeatherSurfaceOcclusionProgram.mName = "Weather Surface Occlusion Shader";
            gDeferredWeatherSurfaceOcclusionProgram.mFeatures.isDeferred = true;
            gDeferredWeatherSurfaceOcclusionProgram.mShaderFiles.clear();
            gDeferredWeatherSurfaceOcclusionProgram.clearPermutations();
            gDeferredWeatherSurfaceOcclusionProgram.addPermutation("WEATHER_RAIN_OCCLUSION", "1");
            gDeferredWeatherSurfaceOcclusionProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
            gDeferredWeatherSurfaceOcclusionProgram.mShaderFiles.push_back(make_pair("deferred/weatherSurfaceF.glsl", GL_FRAGMENT_SHADER));
            gDeferredWeatherSurfaceOcclusionProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
            success = gDeferredWeatherSurfaceOcclusionProgram.createShader();
            if (!success)
            {
                LL_WARNS() << "Failed to create shader '" << gDeferredWeatherSurfaceOcclusionProgram.mName << "', using baseline weather surface response." << LL_ENDL;
                success = true;
            }
        }
        else
        {
            gDeferredWeatherSurfaceOcclusionProgram.unload();
        }

        gDeferredWeatherLightningProgram.mName = "Weather Lightning Shader";
        gDeferredWeatherLightningProgram.mFeatures.isDeferred = true;
        gDeferredWeatherLightningProgram.mShaderFiles.clear();
        gDeferredWeatherLightningProgram.clearPermutations();
        gDeferredWeatherLightningProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gDeferredWeatherLightningProgram.mShaderFiles.push_back(make_pair("deferred/weatherLightningF.glsl", GL_FRAGMENT_SHADER));
        gDeferredWeatherLightningProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gDeferredWeatherLightningProgram.createShader();
        if (!success)
        {
            LL_WARNS() << "Failed to create shader '" << gDeferredWeatherLightningProgram.mName << "', disabling weather lightning." << LL_ENDL;
            success = true;
        }

        if (gSavedSettings.getBOOL(
                "AlchemyWeatherLightningQualityEnabled") &&
            gSavedSettings.getBOOL("RenderHDREnabled") &&
            gGLManager.mGLVersion > 4.05f)
        {
            // Compile the opt-in program as an independent permutation. The
            // setting listener requests a shader reload on toggle, including
            // preset/reset changes, so the default-off state pays no compile
            // or resident-program cost.
            gDeferredWeatherLightningQualityProgram.mName =
                "Weather Lightning Quality Shader";
            gDeferredWeatherLightningQualityProgram.mFeatures.isDeferred =
                true;
            gDeferredWeatherLightningQualityProgram.mShaderFiles.clear();
            gDeferredWeatherLightningQualityProgram.clearPermutations();
            gDeferredWeatherLightningQualityProgram.addPermutation(
                "WEATHER_LIGHTNING_QUALITY", "1");
            gDeferredWeatherLightningQualityProgram.mShaderFiles.push_back(
                make_pair(
                    "deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
            gDeferredWeatherLightningQualityProgram.mShaderFiles.push_back(
                make_pair(
                    "deferred/weatherLightningF.glsl", GL_FRAGMENT_SHADER));
            gDeferredWeatherLightningQualityProgram.mShaderLevel =
                mShaderLevel[SHADER_DEFERRED];
            success =
                gDeferredWeatherLightningQualityProgram.createShader();
            if (!success)
            {
                LL_WARNS() << "Failed to create shader '"
                           << gDeferredWeatherLightningQualityProgram.mName
                           << "', using baseline weather lightning."
                           << LL_ENDL;
                success = true;
            }
        }
        else
        {
            gDeferredWeatherLightningQualityProgram.unload();
        }

        gCASProgram.mName = "Contrast Adaptive Sharpening Shader";
        gCASProgram.mFeatures.hasSrgb = true;
        gCASProgram.mShaderFiles.clear();
        gCASProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gCASProgram.mShaderFiles.push_back(make_pair("deferred/CASF.glsl", GL_FRAGMENT_SHADER));
        gCASProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gCASProgram.createShader();
        // llassert(success);
        if (!success)
        {
            LL_WARNS() << "Failed to create shader '" << gCASProgram.mName << "', disabling!" << LL_ENDL;
            // continue as if this shader never happened
            success = true;
        }
    }

    if (success)
    {
        gDeferredPostProgram.mName = "Deferred Post Shader";
        gDeferredPostProgram.mFeatures.isDeferred = true;
        gDeferredPostProgram.mShaderFiles.clear();
        gDeferredPostProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gDeferredPostProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredF.glsl", GL_FRAGMENT_SHADER));
        gDeferredPostProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gDeferredPostProgram.clearPermutations();
        gDeferredPostProgram.addPermutation("FRONT_BLUR", "1");

        success = gDeferredPostProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredPostProgramNoNear.mName = "Deferred Post Shader No Near Blur";
        gDeferredPostProgramNoNear.mFeatures.isDeferred = true;
        gDeferredPostProgramNoNear.mShaderFiles.clear();
        gDeferredPostProgramNoNear.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gDeferredPostProgramNoNear.mShaderFiles.push_back(make_pair("deferred/postDeferredF.glsl", GL_FRAGMENT_SHADER));
        gDeferredPostProgramNoNear.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gDeferredPostProgramNoNear.clearPermutations();
        gDeferredPostProgramNoNear.addPermutation("FRONT_BLUR", "0");

        success = gDeferredPostProgramNoNear.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredCoFProgram.mName = "Deferred CoF Shader";
        gDeferredCoFProgram.mShaderFiles.clear();
        gDeferredCoFProgram.mFeatures.isDeferred = true;
        gDeferredCoFProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gDeferredCoFProgram.mShaderFiles.push_back(make_pair("deferred/cofF.glsl", GL_FRAGMENT_SHADER));
        gDeferredCoFProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gDeferredCoFProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredDoFCombineProgram.mName = "Deferred DoFCombine Shader";
        gDeferredDoFCombineProgram.mFeatures.isDeferred = true;
        gDeferredDoFCombineProgram.mShaderFiles.clear();
        gDeferredDoFCombineProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gDeferredDoFCombineProgram.mShaderFiles.push_back(make_pair("deferred/dofCombineF.glsl", GL_FRAGMENT_SHADER));
        gDeferredDoFCombineProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gDeferredDoFCombineProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredPostNoDoFProgram.mName = "Deferred Post NoDoF Shader";
        gDeferredPostNoDoFProgram.mFeatures.isDeferred = true;
        gDeferredPostNoDoFProgram.mShaderFiles.clear();
        gDeferredPostNoDoFProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gDeferredPostNoDoFProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoDoFF.glsl", GL_FRAGMENT_SHADER));
        gDeferredPostNoDoFProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gDeferredPostNoDoFProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gEnvironmentMapProgram.mName = "Environment Map Program";
        gEnvironmentMapProgram.mShaderFiles.clear();
        gEnvironmentMapProgram.mFeatures.calculatesAtmospherics = true;
        gEnvironmentMapProgram.mFeatures.hasAtmospherics = true;
        gEnvironmentMapProgram.mFeatures.hasGamma = true;
        gEnvironmentMapProgram.mFeatures.hasSrgb = true;

        gEnvironmentMapProgram.clearPermutations();
        gEnvironmentMapProgram.addPermutation("HAS_HDRI", "1");
        add_common_permutations(&gEnvironmentMapProgram);
        gEnvironmentMapProgram.mShaderFiles.push_back(make_pair("deferred/skyV.glsl", GL_VERTEX_SHADER));
        gEnvironmentMapProgram.mShaderFiles.push_back(make_pair("deferred/skyF.glsl", GL_FRAGMENT_SHADER));
        gEnvironmentMapProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gEnvironmentMapProgram.mShaderGroup = LLGLSLShader::SG_SKY;

        success = gEnvironmentMapProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredWLSkyProgram.mName = "Deferred Windlight Sky Shader";
        gDeferredWLSkyProgram.mShaderFiles.clear();
        gDeferredWLSkyProgram.mFeatures.calculatesAtmospherics = true;
        gDeferredWLSkyProgram.mFeatures.hasAtmospherics = true;
        gDeferredWLSkyProgram.mFeatures.hasGamma = true;
        gDeferredWLSkyProgram.mFeatures.hasSrgb = true;

        gDeferredWLSkyProgram.mShaderFiles.push_back(make_pair("deferred/skyV.glsl", GL_VERTEX_SHADER));
        gDeferredWLSkyProgram.mShaderFiles.push_back(make_pair("deferred/skyF.glsl", GL_FRAGMENT_SHADER));
        gDeferredWLSkyProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gDeferredWLSkyProgram.mShaderGroup = LLGLSLShader::SG_SKY;

        add_common_permutations(&gDeferredWLSkyProgram);

        success = gDeferredWLSkyProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredWLCloudProgram.mName = "Deferred Windlight Cloud Program";
        gDeferredWLCloudProgram.mShaderFiles.clear();
        gDeferredWLCloudProgram.mFeatures.calculatesAtmospherics = true;
        gDeferredWLCloudProgram.mFeatures.hasAtmospherics = true;
        gDeferredWLCloudProgram.mFeatures.hasGamma = true;
        gDeferredWLCloudProgram.mFeatures.hasSrgb = true;

        gDeferredWLCloudProgram.mShaderFiles.push_back(make_pair("deferred/cloudsV.glsl", GL_VERTEX_SHADER));
        gDeferredWLCloudProgram.mShaderFiles.push_back(make_pair("deferred/cloudsF.glsl", GL_FRAGMENT_SHADER));
        gDeferredWLCloudProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gDeferredWLCloudProgram.mShaderGroup = LLGLSLShader::SG_SKY;
        gDeferredWLCloudProgram.addConstant( LLGLSLShader::SHADER_CONST_CLOUD_MOON_DEPTH ); // SL-14113

        add_common_permutations(&gDeferredWLCloudProgram);

        success = gDeferredWLCloudProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredWLSunProgram.mName = "Deferred Windlight Sun Program";
        gDeferredWLSunProgram.mFeatures.calculatesAtmospherics = true;
        gDeferredWLSunProgram.mFeatures.hasAtmospherics = true;
        gDeferredWLSunProgram.mFeatures.hasGamma = true;
        gDeferredWLSunProgram.mFeatures.hasAtmospherics = true;
        gDeferredWLSunProgram.mFeatures.hasSrgb = true;
        gDeferredWLSunProgram.mShaderFiles.clear();
        gDeferredWLSunProgram.mShaderFiles.push_back(make_pair("deferred/sunDiscV.glsl", GL_VERTEX_SHADER));
        gDeferredWLSunProgram.mShaderFiles.push_back(make_pair("deferred/sunDiscF.glsl", GL_FRAGMENT_SHADER));
        gDeferredWLSunProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gDeferredWLSunProgram.mShaderGroup = LLGLSLShader::SG_SKY;

        add_common_permutations(&gDeferredWLSunProgram);

        success = gDeferredWLSunProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredWLMoonProgram.mName = "Deferred Windlight Moon Program";
        gDeferredWLMoonProgram.mFeatures.calculatesAtmospherics = true;
        gDeferredWLMoonProgram.mFeatures.hasAtmospherics = true;
        gDeferredWLMoonProgram.mFeatures.hasGamma = true;
        gDeferredWLMoonProgram.mFeatures.hasAtmospherics = true;
        gDeferredWLMoonProgram.mFeatures.hasSrgb = true;

        gDeferredWLMoonProgram.mShaderFiles.clear();
        gDeferredWLMoonProgram.mShaderFiles.push_back(make_pair("deferred/moonV.glsl", GL_VERTEX_SHADER));
        gDeferredWLMoonProgram.mShaderFiles.push_back(make_pair("deferred/moonF.glsl", GL_FRAGMENT_SHADER));
        gDeferredWLMoonProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gDeferredWLMoonProgram.mShaderGroup = LLGLSLShader::SG_SKY;
        gDeferredWLMoonProgram.addConstant( LLGLSLShader::SHADER_CONST_CLOUD_MOON_DEPTH ); // SL-14113

        add_common_permutations(&gDeferredWLMoonProgram);

        success = gDeferredWLMoonProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredStarProgram.mName = "Deferred Star Program";
        gDeferredStarProgram.mShaderFiles.clear();
        gDeferredStarProgram.mShaderFiles.push_back(make_pair("deferred/starsV.glsl", GL_VERTEX_SHADER));
        gDeferredStarProgram.mShaderFiles.push_back(make_pair("deferred/starsF.glsl", GL_FRAGMENT_SHADER));
        gDeferredStarProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gDeferredStarProgram.mShaderGroup = LLGLSLShader::SG_SKY;
        gDeferredStarProgram.addConstant( LLGLSLShader::SHADER_CONST_STAR_DEPTH ); // SL-14113

        add_common_permutations(&gDeferredStarProgram);

        success = gDeferredStarProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredMeteorProgram.mName = "Deferred Meteor Program";
        gDeferredMeteorProgram.mShaderFiles.clear();
        gDeferredMeteorProgram.mShaderFiles.push_back(make_pair("deferred/meteorsV.glsl", GL_VERTEX_SHADER));
        gDeferredMeteorProgram.mShaderFiles.push_back(make_pair("deferred/meteorsF.glsl", GL_FRAGMENT_SHADER));
        gDeferredMeteorProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gDeferredMeteorProgram.mShaderGroup = LLGLSLShader::SG_SKY;

        add_common_permutations(&gDeferredMeteorProgram);

        success = gDeferredMeteorProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gDeferredAuroraProgram.mName = "Deferred Aurora Program";
        gDeferredAuroraProgram.mShaderFiles.clear();
        gDeferredAuroraProgram.mShaderFiles.push_back(make_pair("deferred/auroraV.glsl", GL_VERTEX_SHADER));
        gDeferredAuroraProgram.mShaderFiles.push_back(make_pair("deferred/auroraF.glsl", GL_FRAGMENT_SHADER));
        gDeferredAuroraProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gDeferredAuroraProgram.mShaderGroup = LLGLSLShader::SG_SKY;

        add_common_permutations(&gDeferredAuroraProgram);

        success = gDeferredAuroraProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gNormalMapGenProgram.mName = "Normal Map Generation Program";
        gNormalMapGenProgram.mShaderFiles.clear();
        gNormalMapGenProgram.mShaderFiles.push_back(make_pair("deferred/normgenV.glsl", GL_VERTEX_SHADER));
        gNormalMapGenProgram.mShaderFiles.push_back(make_pair("deferred/normgenF.glsl", GL_FRAGMENT_SHADER));
        gNormalMapGenProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gNormalMapGenProgram.mShaderGroup = LLGLSLShader::SG_SKY;
        success = gNormalMapGenProgram.createShader();
    }

    if (success)
    {
        gDeferredGenBrdfLutProgram.mName = "Brdf Gen Shader";
        gDeferredGenBrdfLutProgram.mShaderFiles.clear();
        gDeferredGenBrdfLutProgram.mShaderFiles.push_back(make_pair("deferred/genbrdflutV.glsl", GL_VERTEX_SHADER));
        gDeferredGenBrdfLutProgram.mShaderFiles.push_back(make_pair("deferred/genbrdflutF.glsl", GL_FRAGMENT_SHADER));
        gDeferredGenBrdfLutProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gDeferredGenBrdfLutProgram.createShader();
    }

    if (success) {
        gPostScreenSpaceReflectionProgram.mName = "Screen Space Reflection Post";
        gPostScreenSpaceReflectionProgram.mShaderFiles.clear();
        gPostScreenSpaceReflectionProgram.mShaderFiles.push_back(make_pair("deferred/screenSpaceReflPostV.glsl", GL_VERTEX_SHADER));
        gPostScreenSpaceReflectionProgram.mShaderFiles.push_back(make_pair("deferred/screenSpaceReflPostF.glsl", GL_FRAGMENT_SHADER));
        gPostScreenSpaceReflectionProgram.mFeatures.hasScreenSpaceReflections = true;
        gPostScreenSpaceReflectionProgram.mFeatures.isDeferred                = true;
        gPostScreenSpaceReflectionProgram.mShaderLevel = 3;
        success = gPostScreenSpaceReflectionProgram.createShader();
    }

    if (success) {
        gDeferredBufferVisualProgram.mName = "Deferred Buffer Visualization Shader";
        gDeferredBufferVisualProgram.mShaderFiles.clear();
        gDeferredBufferVisualProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gDeferredBufferVisualProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredVisualizeBuffers.glsl", GL_FRAGMENT_SHADER));
        gDeferredBufferVisualProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];

        add_common_permutations(&gDeferredBufferVisualProgram);

        success = gDeferredBufferVisualProgram.createShader();
    }

    // [BDMerge A5.4-1a/1b] Velocity / motion-vector pass programs. Phase 1b adds
    // the make_rigged_variant skinned pair (HAS_SKIN -> objectSkinV.glsl current
    // + last palettes) and the classic-avatar velocity program. These programs
    // are cheap to keep resident and only run when BDMergeVelocityBuffer is on.
    if (success)
    {
        gVelocityProgram.mName = "Velocity Shader";
        gVelocityProgram.mFeatures.hasActorFxShadow = true;
        gVelocityProgram.mShaderFiles.clear();
        gVelocityProgram.mShaderFiles.push_back(make_pair("deferred/velocityV.glsl", GL_VERTEX_SHADER));
        gVelocityProgram.mShaderFiles.push_back(make_pair("deferred/velocityF.glsl", GL_FRAGMENT_SHADER));
        gVelocityProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = make_rigged_variant(gVelocityProgram, gVelocitySkinnedProgram);   // [BDMerge A5.4-1b]
        success = success && gVelocityProgram.createShader();
    }

    if (success)
    {
        gVelocityAlphaProgram.mName = "Velocity Alpha Mask Shader";
        gVelocityAlphaProgram.mFeatures.hasActorFxShadow = true;
        gVelocityAlphaProgram.mShaderFiles.clear();
        gVelocityAlphaProgram.mShaderFiles.push_back(make_pair("deferred/velocityAlphaV.glsl", GL_VERTEX_SHADER));
        gVelocityAlphaProgram.mShaderFiles.push_back(make_pair("deferred/velocityAlphaF.glsl", GL_FRAGMENT_SHADER));
        gVelocityAlphaProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = make_rigged_variant(gVelocityAlphaProgram, gVelocityAlphaSkinnedProgram); // [BDMerge A5.4-1b]
        success = success && gVelocityAlphaProgram.createShader();
    }

    if (success)
    {
        gVelocityPBRAlphaProgram.mName = "Velocity PBR Alpha Mask Shader";
        gVelocityPBRAlphaProgram.mFeatures.hasActorFxShadow = true;
        gVelocityPBRAlphaProgram.mShaderFiles.clear();
        gVelocityPBRAlphaProgram.mShaderFiles.push_back(make_pair("deferred/velocityAlphaV.glsl", GL_VERTEX_SHADER));
        gVelocityPBRAlphaProgram.mShaderFiles.push_back(make_pair("deferred/velocityAlphaF.glsl", GL_FRAGMENT_SHADER));
        gVelocityPBRAlphaProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gVelocityPBRAlphaProgram.addPermutation("PBR_ALPHA_MASK", "1");
        success = make_rigged_variant(gVelocityPBRAlphaProgram, gVelocityPBRAlphaSkinnedProgram);
        success = success && gVelocityPBRAlphaProgram.createShader();
    }

    if (success && LLGLSLShader::sIndexedGLTFChannels >= 2)
    {
        const S32 n = LLGLSLShader::sIndexedGLTFChannels;

        gVelocityAlphaIndexedProgram.mName = "Velocity Legacy Alpha Mask Indexed Shader";
        gVelocityAlphaIndexedProgram.mFeatures.hasActorFxShadow = true;
        gVelocityAlphaIndexedProgram.mFeatures.mIndexedMaterialChannels = n;
        gVelocityAlphaIndexedProgram.mShaderFiles.clear();
        gVelocityAlphaIndexedProgram.mShaderFiles.push_back(make_pair("deferred/velocityAlphaIndexedV.glsl", GL_VERTEX_SHADER));
        gVelocityAlphaIndexedProgram.mShaderFiles.push_back(make_pair("deferred/velocityAlphaIndexedF.glsl", GL_FRAGMENT_SHADER));
        gVelocityAlphaIndexedProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gVelocityAlphaIndexedProgram.addPermutation("GLTF_INDEXED_CHANNELS", llformat("%d", n));
        success = make_rigged_variant(gVelocityAlphaIndexedProgram, gVelocityAlphaIndexedSkinnedProgram);
        success = success && gVelocityAlphaIndexedProgram.createShader();

        if (success)
        {
            setup_material_indexed_samplers(gVelocityAlphaIndexedProgram, n, false, false);
            setup_material_indexed_samplers(gVelocityAlphaIndexedSkinnedProgram, n, false, false);
        }
    }

    if (success && LLGLSLShader::sIndexedGLTFChannels >= 2)
    {
        const S32 n = LLGLSLShader::sIndexedGLTFChannels;

        gVelocityPBRAlphaIndexedProgram.mName = "Velocity PBR Alpha Mask Indexed Shader";
        gVelocityPBRAlphaIndexedProgram.mFeatures.hasActorFxShadow = true;
        gVelocityPBRAlphaIndexedProgram.mFeatures.mIndexedMaterialChannels = n;
        gVelocityPBRAlphaIndexedProgram.mShaderFiles.clear();
        gVelocityPBRAlphaIndexedProgram.mShaderFiles.push_back(make_pair("deferred/velocityAlphaIndexedV.glsl", GL_VERTEX_SHADER));
        gVelocityPBRAlphaIndexedProgram.mShaderFiles.push_back(make_pair("deferred/velocityAlphaIndexedF.glsl", GL_FRAGMENT_SHADER));
        gVelocityPBRAlphaIndexedProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        gVelocityPBRAlphaIndexedProgram.addPermutation("GLTF_INDEXED_CHANNELS", llformat("%d", n));
        gVelocityPBRAlphaIndexedProgram.addPermutation("PBR_ALPHA_MASK", "1");
        success = make_rigged_variant(gVelocityPBRAlphaIndexedProgram, gVelocityPBRAlphaIndexedSkinnedProgram);
        success = success && gVelocityPBRAlphaIndexedProgram.createShader();

        if (success)
        {
            setup_material_indexed_samplers(gVelocityPBRAlphaIndexedProgram, n, false, false);
            setup_material_indexed_samplers(gVelocityPBRAlphaIndexedSkinnedProgram, n, false, false);
        }
    }

    // [BDMerge A5.4-1b] Classic (system) avatar velocity: avatarSkinV supplies
    // getSkinnedTransform (hasSkinning feature); the previous palette is
    // uploaded by llviewerjointmesh uploadJointMatrices when this program is
    // bound. Uses the baked-texture alpha fragment so alpha-hidden body pixels
    // do not stamp motion vectors.
    if (success)
    {
        gAvatarVelocityProgram.mName = "Avatar Velocity Shader";
        gAvatarVelocityProgram.mFeatures.hasSkinning = true;
        gAvatarVelocityProgram.mFeatures.hasActorFxShadow = true;
        gAvatarVelocityProgram.mShaderFiles.clear();
        gAvatarVelocityProgram.mShaderFiles.push_back(make_pair("deferred/avatarVelocityV.glsl", GL_VERTEX_SHADER));
        gAvatarVelocityProgram.mShaderFiles.push_back(make_pair("deferred/velocityAlphaF.glsl", GL_FRAGMENT_SHADER));
        gAvatarVelocityProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gAvatarVelocityProgram.createShader();
    }

    // [BDMerge A5.4-3] Motion blur composite (32-tap gather along the velocity
    // buffer). Donor: Black Dragon llviewershadermgr.cpp:3009-3019.
    if (success)
    {
        gDeferredMotionBlurProgram.mName = "Deferred Motion Blur Shader";
        gDeferredMotionBlurProgram.mFeatures.isDeferred = true;
        gDeferredMotionBlurProgram.mShaderFiles.clear();
        gDeferredMotionBlurProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gDeferredMotionBlurProgram.mShaderFiles.push_back(make_pair("deferred/motionBlurF.glsl", GL_FRAGMENT_SHADER));
        gDeferredMotionBlurProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gDeferredMotionBlurProgram.createShader();
    }

    if (success)
    {
        gVelocityDebugProgram.mName = "Velocity Debug Visualization Shader";
        gVelocityDebugProgram.mShaderFiles.clear();
        gVelocityDebugProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gVelocityDebugProgram.mShaderFiles.push_back(make_pair("deferred/velocityDebugF.glsl", GL_FRAGMENT_SHADER));
        gVelocityDebugProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gVelocityDebugProgram.createShader();
    }

    // [BDMerge A5.4-1c] Fullscreen camera-motion fallback. Fills the velocity
    // buffer from scene depth + the previous camera before the geometry pass
    // stamps true per-object motion, so avatar/sky/uncovered pixels carry
    // camera-induced motion instead of reading "static". inv_proj/inv_modelview
    // are auto-fed by the matrix sync in llrender.cpp; last_modelview_matrix and
    // projection_matrix_unjittered are uploaded at the call site.
    //
    // NON-FATAL: this is an optional feature (gated by BDMergeVelocityBuffer and
    // guarded by isComplete() at the call site). Its load result must NOT feed
    // back into `success` -- otherwise a missing/rejected file would mark the
    // ENTIRE deferred set as failed and crash login on the first null bind()
    // (ASSERT mProgramObject != 0). Degrade to "no camera fallback" instead.
    if (success)
    {
        gVelocityCameraProgram.mName = "Velocity Camera Fallback Shader";
        gVelocityCameraProgram.mShaderFiles.clear();
        gVelocityCameraProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gVelocityCameraProgram.mShaderFiles.push_back(make_pair("deferred/velocityCameraF.glsl", GL_FRAGMENT_SHADER));
        gVelocityCameraProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        if (!gVelocityCameraProgram.createShader())
        {
            LL_WARNS("ShaderLoading") << "Velocity camera fallback shader failed to "
                "load; camera-motion velocity fill disabled (velocity buffer still "
                "works via geometry passes)." << LL_ENDL;
        }
    }

    // [BDMerge Froxel F0] Hybrid froxel volumetrics: P1 media pass + debug
    // visualizer. Both link froxelUtil.glsl (the shared grid<->atlas helper) as a
    // second fragment object - the same multi-file link the loader does for every
    // program (createShader attaches each mShaderFiles entry). froxelUtil carries no
    // main(), only froxel-prefixed functions, so it cannot collide. The media pass
    // is a plain fullscreen pass (like velocityDebug); the debug pass is isDeferred
    // so getPosition()/depthMap/inv_proj resolve for the surface-depth sample.
    if (success)
    {
        gFroxelMediaProgram.mName = "Froxel Media Pass Shader";
        gFroxelMediaProgram.mShaderFiles.clear();
        gFroxelMediaProgram.clearPermutations();
        gFroxelMediaProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gFroxelMediaProgram.mShaderFiles.push_back(make_pair("deferred/froxelMediaF.glsl", GL_FRAGMENT_SHADER));
        gFroxelMediaProgram.mShaderFiles.push_back(make_pair("deferred/froxelUtil.glsl", GL_FRAGMENT_SHADER));
        gFroxelMediaProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gFroxelMediaProgram.createShader();
        if (!success)
        {
            LL_WARNS() << "Failed to create shader '" << gFroxelMediaProgram.mName << "', disabling!" << LL_ENDL;
            success = true;
        }
    }

    if (success)
    {
        gFroxelDebugProgram.mName = "Froxel Debug Visualization Shader";
        gFroxelDebugProgram.mFeatures.isDeferred = true;
        gFroxelDebugProgram.mShaderFiles.clear();
        gFroxelDebugProgram.clearPermutations();
        gFroxelDebugProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gFroxelDebugProgram.mShaderFiles.push_back(make_pair("deferred/froxelDebugF.glsl", GL_FRAGMENT_SHADER));
        gFroxelDebugProgram.mShaderFiles.push_back(make_pair("deferred/froxelUtil.glsl", GL_FRAGMENT_SHADER));
        gFroxelDebugProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gFroxelDebugProgram.createShader();
        if (!success)
        {
            LL_WARNS() << "Failed to create shader '" << gFroxelDebugProgram.mName << "', disabling!" << LL_ENDL;
            success = true;
        }
    }

    // [BDMerge Froxel F1] P4 integrate + P5 apply passes. Both link froxelUtil.glsl
    // as a second fragment object, exactly like the F0 media/debug programs. The
    // integrate pass is a plain fullscreen pass (reads the media atlas only, writes
    // the integrated atlas). The apply pass is isDeferred so getPosition()/depthMap/
    // inv_proj resolve for the surface-depth sample it composites onto the scene.
    if (success)
    {
        gFroxelIntegrateProgram.mName = "Froxel Integrate Pass Shader";
        gFroxelIntegrateProgram.mShaderFiles.clear();
        gFroxelIntegrateProgram.clearPermutations();
        gFroxelIntegrateProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gFroxelIntegrateProgram.mShaderFiles.push_back(make_pair("deferred/froxelIntegrateF.glsl", GL_FRAGMENT_SHADER));
        gFroxelIntegrateProgram.mShaderFiles.push_back(make_pair("deferred/froxelUtil.glsl", GL_FRAGMENT_SHADER));
        gFroxelIntegrateProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gFroxelIntegrateProgram.createShader();
        if (!success)
        {
            LL_WARNS() << "Failed to create shader '" << gFroxelIntegrateProgram.mName << "', disabling!" << LL_ENDL;
            success = true;
        }
    }

    if (success)
    {
        gFroxelApplyProgram.mName = "Froxel Apply Pass Shader";
        gFroxelApplyProgram.mFeatures.isDeferred = true;
        gFroxelApplyProgram.mShaderFiles.clear();
        gFroxelApplyProgram.clearPermutations();
        gFroxelApplyProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gFroxelApplyProgram.mShaderFiles.push_back(make_pair("deferred/froxelApplyF.glsl", GL_FRAGMENT_SHADER));
        gFroxelApplyProgram.mShaderFiles.push_back(make_pair("deferred/froxelUtil.glsl", GL_FRAGMENT_SHADER));
        gFroxelApplyProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gFroxelApplyProgram.createShader();
        if (!success)
        {
            LL_WARNS() << "Failed to create shader '" << gFroxelApplyProgram.mName << "', disabling!" << LL_ENDL;
            success = true;
        }
    }

    // [BDMerge Froxel F2] P2 per-light injection pass. Mirrors the per-cone projector-
    // volumetric program's binding needs so setupSpotLightVolumetric + sampleSpotShadow
    // resolve identically: isDeferred (deferredUtil - clipProjectedLightVars, gobo,
    // atten), hasShadows + SPOT_SHADOW=1 (shadowUtil's indexed projector-shadow
    // dispatch), atmospherics features to match the proven per-cone link. PLUS it
    // attaches froxelUtil.glsl (the grid<->atlas helper) as a second fragment object,
    // exactly like the other froxel programs. Rendered once per injecting projector
    // into the light atlas (additive), so it is a froxel program AND a projector
    // program at the same time.
    if (success)
    {
        gFroxelInjectProgram.mName = "Froxel Light Injection Shader";
        gFroxelInjectProgram.mFeatures.isDeferred = true;
        gFroxelInjectProgram.mFeatures.calculatesAtmospherics = true;
        gFroxelInjectProgram.mFeatures.hasAtmospherics = true;
        gFroxelInjectProgram.mFeatures.hasShadows = true;
        gFroxelInjectProgram.mShaderFiles.clear();
        gFroxelInjectProgram.clearPermutations();
        gFroxelInjectProgram.addPermutation("SPOT_SHADOW", "1");
        gFroxelInjectProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gFroxelInjectProgram.mShaderFiles.push_back(make_pair("deferred/froxelInjectF.glsl", GL_FRAGMENT_SHADER));
        gFroxelInjectProgram.mShaderFiles.push_back(make_pair("deferred/froxelUtil.glsl", GL_FRAGMENT_SHADER));
        gFroxelInjectProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gFroxelInjectProgram.createShader();
        if (!success)
        {
            LL_WARNS() << "Failed to create shader '" << gFroxelInjectProgram.mName << "', disabling!" << LL_ENDL;
            success = true;
        }
    }

    // [BDMerge Froxel F3] P3 temporal resolve pass. Plain froxel program (like media /
    // integrate): a fullscreen-triangle draw over the light atlas that reprojects each
    // froxel's world position into the previous grid and EMA-blends the resolved
    // history. Attaches froxelUtil.glsl (grid<->atlas + trilinear) as a second fragment
    // object, same as every froxel program. NOT isDeferred - it reads only froxel
    // atlases, never the G-buffer.
    if (success)
    {
        gFroxelTemporalProgram.mName = "Froxel Temporal Resolve Shader";
        gFroxelTemporalProgram.mShaderFiles.clear();
        gFroxelTemporalProgram.clearPermutations();
        gFroxelTemporalProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gFroxelTemporalProgram.mShaderFiles.push_back(make_pair("deferred/froxelTemporalF.glsl", GL_FRAGMENT_SHADER));
        gFroxelTemporalProgram.mShaderFiles.push_back(make_pair("deferred/froxelUtil.glsl", GL_FRAGMENT_SHADER));
        gFroxelTemporalProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gFroxelTemporalProgram.createShader();
        if (!success)
        {
            LL_WARNS() << "Failed to create shader '" << gFroxelTemporalProgram.mName << "', disabling!" << LL_ENDL;
            success = true;
        }
    }

    if (success)
    {
        gBlitWithEffectsProgram.mName = "Blit With Post Effects Shader";
        gBlitWithEffectsProgram.mFeatures.isDeferred = true;
        gBlitWithEffectsProgram.mFeatures.hasPostEffects = true;
        gBlitWithEffectsProgram.mShaderFiles.clear();
        gBlitWithEffectsProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gBlitWithEffectsProgram.mShaderFiles.push_back(make_pair("alchemy/blitWithEffectsF.glsl", GL_FRAGMENT_SHADER));
        gBlitWithEffectsProgram.clearPermutations();
        if (gSavedSettings.getBOOL("RenderHDREnabled"))
        {
            gBlitWithEffectsProgram.addPermutation("DITHER", "1");
        }
        gBlitWithEffectsProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gBlitWithEffectsProgram.createShader();
        llassert(success);
    }

    // HDR-only: the bloom pyramid is allocated in the HDR path, so the tonemap
    // shader variants fold the bloom composite inline. Halation rides in the
    // bloom alpha channel when RenderBloomHalation is on — both settings trigger
    // shader rebuilds, so reading them at compile time stays in sync with the
    // bloom pyramid format.
    const bool hdr_enabled         = gSavedSettings.getBOOL("RenderHDREnabled");
    const bool bloom_halation_perm = gSavedSettings.getBOOL("RenderBloomHalation");

    if (success)
    {
        gCGGammaProgram.mName = "CG Gamma Shader";
        gCGGammaProgram.mFeatures.isDeferred = true;
        gCGGammaProgram.mFeatures.hasPostEffects = true;
        gCGGammaProgram.mShaderFiles.clear();
        gCGGammaProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gCGGammaProgram.mShaderFiles.push_back(make_pair("alchemy/colorCorrectF.glsl", GL_FRAGMENT_SHADER));
        gCGGammaProgram.clearPermutations();
        gCGGammaProgram.addPermutation("HAS_POST_EFFECTS", "1");
        if (!hdr_enabled)
        {
            gCGGammaProgram.addPermutation("DITHER", "1");
        }
        else
        {
            gCGGammaProgram.addPermutation("BLOOM_COMPOSITE", "1");
            if (bloom_halation_perm)
            {
                gCGGammaProgram.addPermutation("BLOOM_HALATION", "1");
            }
        }
        gCGGammaProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gCGGammaProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gCGLegacyGammaProgram.mName = "CG Legacy Gamma Shader";
        gCGLegacyGammaProgram.mFeatures.isDeferred = true;
        gCGLegacyGammaProgram.mFeatures.hasPostEffects = true;
        gCGLegacyGammaProgram.mShaderFiles.clear();
        gCGLegacyGammaProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gCGLegacyGammaProgram.mShaderFiles.push_back(make_pair("alchemy/colorCorrectF.glsl", GL_FRAGMENT_SHADER));
        gCGLegacyGammaProgram.clearPermutations();
        gCGLegacyGammaProgram.addPermutation("LEGACY_GAMMA", "1");
        gCGLegacyGammaProgram.addPermutation("HAS_POST_EFFECTS", "1");
        if (!hdr_enabled)
        {
            gCGLegacyGammaProgram.addPermutation("DITHER", "1");
        }
        else
        {
            gCGLegacyGammaProgram.addPermutation("BLOOM_COMPOSITE", "1");
            if (bloom_halation_perm)
            {
                gCGLegacyGammaProgram.addPermutation("BLOOM_HALATION", "1");
            }
        }
        gCGLegacyGammaProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gCGLegacyGammaProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gCGColorgradeGammaProgram.mName = "CG Color Grade Gamma Shader";
        gCGColorgradeGammaProgram.mFeatures.isDeferred = true;
        gCGColorgradeGammaProgram.mFeatures.hasColorGrade = true;
        gCGColorgradeGammaProgram.mFeatures.hasPostEffects = true;
        gCGColorgradeGammaProgram.mShaderFiles.clear();
        gCGColorgradeGammaProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gCGColorgradeGammaProgram.mShaderFiles.push_back(make_pair("alchemy/colorCorrectF.glsl", GL_FRAGMENT_SHADER));
        gCGColorgradeGammaProgram.clearPermutations();
        gCGColorgradeGammaProgram.addPermutation("COLOR_GRADE", "1");
        gCGColorgradeGammaProgram.addPermutation("HAS_POST_EFFECTS", "1");
        if (!hdr_enabled)
        {
            gCGColorgradeGammaProgram.addPermutation("DITHER", "1");
        }
        else
        {
            gCGColorgradeGammaProgram.addPermutation("BLOOM_COMPOSITE", "1");
            if (bloom_halation_perm)
            {
                gCGColorgradeGammaProgram.addPermutation("BLOOM_HALATION", "1");
            }
        }
        gCGColorgradeGammaProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success                                = gCGColorgradeGammaProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gCGColorgradeLegacyGammaProgram.mName = "CG Color Grade Legacy Gamma Shader";
        gCGColorgradeLegacyGammaProgram.mFeatures.isDeferred = true;
        gCGColorgradeLegacyGammaProgram.mFeatures.hasColorGrade = true;
        gCGColorgradeLegacyGammaProgram.mFeatures.hasPostEffects = true;
        gCGColorgradeLegacyGammaProgram.mShaderFiles.clear();
        gCGColorgradeLegacyGammaProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gCGColorgradeLegacyGammaProgram.mShaderFiles.push_back(make_pair("alchemy/colorCorrectF.glsl", GL_FRAGMENT_SHADER));
        gCGColorgradeLegacyGammaProgram.clearPermutations();
        gCGColorgradeLegacyGammaProgram.addPermutation("COLOR_GRADE", "1");
        gCGColorgradeLegacyGammaProgram.addPermutation("LEGACY_GAMMA", "1");
        gCGColorgradeLegacyGammaProgram.addPermutation("HAS_POST_EFFECTS", "1");
        if (!hdr_enabled)
        {
            gCGColorgradeLegacyGammaProgram.addPermutation("DITHER", "1");
        }
        else
        {
            gCGColorgradeLegacyGammaProgram.addPermutation("BLOOM_COMPOSITE", "1");
            if (bloom_halation_perm)
            {
                gCGColorgradeLegacyGammaProgram.addPermutation("BLOOM_HALATION", "1");
            }
        }
        gCGColorgradeLegacyGammaProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gCGColorgradeLegacyGammaProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gCGTonemapProgram.mName = "CG Tonemap Shader";
        gCGTonemapProgram.mFeatures.isDeferred = true;
        gCGTonemapProgram.mFeatures.hasTonemap = true;
        gCGTonemapProgram.mFeatures.hasPostEffects = true;
        gCGTonemapProgram.mShaderFiles.clear();
        gCGTonemapProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gCGTonemapProgram.mShaderFiles.push_back(make_pair("alchemy/colorCorrectF.glsl", GL_FRAGMENT_SHADER));
        gCGTonemapProgram.clearPermutations();
        gCGTonemapProgram.addPermutation("TONEMAP", "1");
        gCGTonemapProgram.addPermutation("HAS_POST_EFFECTS", "1");
        if (!hdr_enabled)
        {
            gCGTonemapProgram.addPermutation("DITHER", "1");
        }
        else
        {
            gCGTonemapProgram.addPermutation("BLOOM_COMPOSITE", "1");
            if (bloom_halation_perm)
            {
                gCGTonemapProgram.addPermutation("BLOOM_HALATION", "1");
            }
        }
        gCGTonemapProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gCGTonemapProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gCGTonemapLegacyGammaProgram.mName = "CG Tonemap Legacy Gamma Shader";
        gCGTonemapLegacyGammaProgram.mFeatures.isDeferred = true;
        gCGTonemapLegacyGammaProgram.mFeatures.hasTonemap = true;
        gCGTonemapLegacyGammaProgram.mFeatures.hasPostEffects = true;
        gCGTonemapLegacyGammaProgram.mShaderFiles.clear();
        gCGTonemapLegacyGammaProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gCGTonemapLegacyGammaProgram.mShaderFiles.push_back(make_pair("alchemy/colorCorrectF.glsl", GL_FRAGMENT_SHADER));
        gCGTonemapLegacyGammaProgram.clearPermutations();
        gCGTonemapLegacyGammaProgram.addPermutation("LEGACY_GAMMA", "1");
        gCGTonemapLegacyGammaProgram.addPermutation("TONEMAP", "1");
        gCGTonemapLegacyGammaProgram.addPermutation("HAS_POST_EFFECTS", "1");
        if (!hdr_enabled)
        {
            gCGTonemapLegacyGammaProgram.addPermutation("DITHER", "1");
        }
        else
        {
            gCGTonemapLegacyGammaProgram.addPermutation("BLOOM_COMPOSITE", "1");
            if (bloom_halation_perm)
            {
                gCGTonemapLegacyGammaProgram.addPermutation("BLOOM_HALATION", "1");
            }
        }
        gCGTonemapLegacyGammaProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gCGTonemapLegacyGammaProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gCGTonemapColorgradeProgram.mName = "CG Tonemap Color Grade Shader";
        gCGTonemapColorgradeProgram.mFeatures.isDeferred = true;
        gCGTonemapColorgradeProgram.mFeatures.hasTonemap = true;
        gCGTonemapColorgradeProgram.mFeatures.hasColorGrade = true;
        gCGTonemapColorgradeProgram.mFeatures.hasPostEffects = true;
        gCGTonemapColorgradeProgram.mShaderFiles.clear();
        gCGTonemapColorgradeProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gCGTonemapColorgradeProgram.mShaderFiles.push_back(make_pair("alchemy/colorCorrectF.glsl", GL_FRAGMENT_SHADER));
        gCGTonemapColorgradeProgram.clearPermutations();
        gCGTonemapColorgradeProgram.addPermutation("COLOR_GRADE", "1");
        gCGTonemapColorgradeProgram.addPermutation("TONEMAP", "1");
        gCGTonemapColorgradeProgram.addPermutation("HAS_POST_EFFECTS", "1");
        if (!hdr_enabled)
        {
            gCGTonemapColorgradeProgram.addPermutation("DITHER", "1");
        }
        else
        {
            gCGTonemapColorgradeProgram.addPermutation("BLOOM_COMPOSITE", "1");
            if (bloom_halation_perm)
            {
                gCGTonemapColorgradeProgram.addPermutation("BLOOM_HALATION", "1");
            }
        }
        gCGTonemapColorgradeProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gCGTonemapColorgradeProgram.createShader();
        llassert(success);
    }

    if (success)
    {
        gCGTonemapColorgradeLegacyGammaProgram.mName = "CG Tonemap Color Grade Legacy Gamma Shader";
        gCGTonemapColorgradeLegacyGammaProgram.mFeatures.isDeferred = true;
        gCGTonemapColorgradeLegacyGammaProgram.mFeatures.hasTonemap = true;
        gCGTonemapColorgradeLegacyGammaProgram.mFeatures.hasColorGrade = true;
        gCGTonemapColorgradeLegacyGammaProgram.mFeatures.hasPostEffects = true;
        gCGTonemapColorgradeLegacyGammaProgram.mShaderFiles.clear();
        gCGTonemapColorgradeLegacyGammaProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gCGTonemapColorgradeLegacyGammaProgram.mShaderFiles.push_back(make_pair("alchemy/colorCorrectF.glsl", GL_FRAGMENT_SHADER));
        gCGTonemapColorgradeLegacyGammaProgram.clearPermutations();
        gCGTonemapColorgradeLegacyGammaProgram.addPermutation("COLOR_GRADE", "1");
        gCGTonemapColorgradeLegacyGammaProgram.addPermutation("LEGACY_GAMMA", "1");
        gCGTonemapColorgradeLegacyGammaProgram.addPermutation("TONEMAP", "1");
        gCGTonemapColorgradeLegacyGammaProgram.addPermutation("HAS_POST_EFFECTS", "1");
        if (!hdr_enabled)
        {
            gCGTonemapColorgradeLegacyGammaProgram.addPermutation("DITHER", "1");
        }
        else
        {
            gCGTonemapColorgradeLegacyGammaProgram.addPermutation("BLOOM_COMPOSITE", "1");
            if (bloom_halation_perm)
            {
                gCGTonemapColorgradeLegacyGammaProgram.addPermutation("BLOOM_HALATION", "1");
            }
        }
        gCGTonemapColorgradeLegacyGammaProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gCGTonemapColorgradeLegacyGammaProgram.createShader();
        llassert(success);
    }

    // On-lens filters (Graduated ND + Polarizer) pre-pass. Runs on the linear
    // HDR scene before bloom/flare. hasPostEffects attaches postEffectUtilsF,
    // which defines applyGradND/applyPolarizer and the gradnd_*/polarizer_*
    // uniforms this shader consumes.
    if (success)
    {
        gOnLensFiltersProgram.mName = "On-Lens Filters Shader";
        gOnLensFiltersProgram.mFeatures.isDeferred = true;
        gOnLensFiltersProgram.mFeatures.hasPostEffects = true;
        gOnLensFiltersProgram.mShaderFiles.clear();
        gOnLensFiltersProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoTCV.glsl", GL_VERTEX_SHADER));
        gOnLensFiltersProgram.mShaderFiles.push_back(make_pair("alchemy/onLensFiltersF.glsl", GL_FRAGMENT_SHADER));
        gOnLensFiltersProgram.clearPermutations();
        gOnLensFiltersProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gOnLensFiltersProgram.createShader();
        llassert(success);
    }

    // [RLVa:KB] - @setsphere
    if(success)
    {
        gRlvSphereProgram.mName = "RLVa Sphere Post Processing Shader";
        gRlvSphereProgram.mFeatures.isDeferred = true;
        gRlvSphereProgram.mShaderFiles.clear();
        gRlvSphereProgram.mShaderFiles.push_back(make_pair("deferred/rlvV.glsl", GL_VERTEX_SHADER));
        gRlvSphereProgram.mShaderFiles.push_back(make_pair("deferred/rlvF.glsl", GL_FRAGMENT_SHADER));
        gRlvSphereProgram.mShaderLevel = mShaderLevel[SHADER_DEFERRED];
        success = gRlvSphereProgram.createShader();
    }
    // [/RLV:KB]
    return success;
}

bool LLViewerShaderMgr::loadShadersObject()
{
    LL_PROFILE_ZONE_SCOPED;
    bool success = true;

    if (success)
    {
        gObjectBumpProgram.mName = "Bump Shader";
        gObjectBumpProgram.mShaderFiles.clear();
        gObjectBumpProgram.mShaderFiles.push_back(make_pair("objects/bumpV.glsl", GL_VERTEX_SHADER));
        gObjectBumpProgram.mShaderFiles.push_back(make_pair("objects/bumpF.glsl", GL_FRAGMENT_SHADER));
        gObjectBumpProgram.mShaderLevel = mShaderLevel[SHADER_OBJECT];
        success = make_rigged_variant(gObjectBumpProgram, gSkinnedObjectBumpProgram);
        success = success && gObjectBumpProgram.createShader();
        if (success)
        { //lldrawpoolbump assumes "texture0" has channel 0 and "texture1" has channel 1
            LLGLSLShader* shader[] = { &gObjectBumpProgram, &gSkinnedObjectBumpProgram };
            for (int i = 0; i < 2; ++i)
            {
                shader[i]->bind();
                shader[i]->uniform1i(sTexture0, 0);
                shader[i]->uniform1i(sTexture1, 1);
                shader[i]->unbind();
            }
        }
    }

    if (success)
    {
        gObjectAlphaMaskNoColorProgram.mName = "No color alpha mask Shader";
        gObjectAlphaMaskNoColorProgram.mFeatures.calculatesLighting = true;
        gObjectAlphaMaskNoColorProgram.mFeatures.calculatesAtmospherics = true;
        gObjectAlphaMaskNoColorProgram.mFeatures.hasGamma = true;
        gObjectAlphaMaskNoColorProgram.mFeatures.hasAtmospherics = true;
        gObjectAlphaMaskNoColorProgram.mFeatures.hasLighting = true;
        gObjectAlphaMaskNoColorProgram.mFeatures.hasAlphaMask = true;
        gObjectAlphaMaskNoColorProgram.mFeatures.hasActorFx = true;
        gObjectAlphaMaskNoColorProgram.mShaderFiles.clear();
        gObjectAlphaMaskNoColorProgram.mShaderFiles.push_back(make_pair("objects/simpleNoColorV.glsl", GL_VERTEX_SHADER));
        gObjectAlphaMaskNoColorProgram.mShaderFiles.push_back(make_pair("objects/simpleF.glsl", GL_FRAGMENT_SHADER));
        gObjectAlphaMaskNoColorProgram.mShaderLevel = mShaderLevel[SHADER_OBJECT];
        gObjectAlphaMaskNoColorProgram.addPermutation("HAS_ACTOR_FX", "1");
        success = gObjectAlphaMaskNoColorProgram.createShader();
    }

    if (success)
    {
        gImpostorProgram.mName = "Impostor Shader";
        gImpostorProgram.mFeatures.hasSrgb = true;
        gImpostorProgram.mShaderFiles.clear();
        gImpostorProgram.mShaderFiles.push_back(make_pair("objects/impostorV.glsl", GL_VERTEX_SHADER));
        gImpostorProgram.mShaderFiles.push_back(make_pair("objects/impostorF.glsl", GL_FRAGMENT_SHADER));
        gImpostorProgram.mShaderLevel = mShaderLevel[SHADER_OBJECT];
        success = gImpostorProgram.createShader();
    }

    if (success)
    {
        gObjectPreviewProgram.mName = "Object Preview Shader";
        gObjectPreviewProgram.mShaderFiles.clear();
        gObjectPreviewProgram.mShaderFiles.push_back(make_pair("objects/previewV.glsl", GL_VERTEX_SHADER));
        gObjectPreviewProgram.mShaderFiles.push_back(make_pair("objects/previewF.glsl", GL_FRAGMENT_SHADER));
        gObjectPreviewProgram.mShaderLevel = mShaderLevel[SHADER_OBJECT];
        success = make_rigged_variant(gObjectPreviewProgram, gSkinnedObjectPreviewProgram);
        success = gObjectPreviewProgram.createShader();
        gObjectPreviewProgram.mFeatures.hasLighting = true;
        gSkinnedObjectPreviewProgram.mFeatures.hasLighting = true;
    }

    if (success)
    {
        gPhysicsPreviewProgram.mName = "Preview Physics Shader";
        gPhysicsPreviewProgram.mFeatures.calculatesLighting = false;
        gPhysicsPreviewProgram.mFeatures.calculatesAtmospherics = false;
        gPhysicsPreviewProgram.mFeatures.hasGamma = false;
        gPhysicsPreviewProgram.mFeatures.hasAtmospherics = false;
        gPhysicsPreviewProgram.mFeatures.hasLighting = false;
        gPhysicsPreviewProgram.mShaderFiles.clear();
        gPhysicsPreviewProgram.mShaderFiles.push_back(make_pair("objects/previewPhysicsV.glsl", GL_VERTEX_SHADER));
        gPhysicsPreviewProgram.mShaderFiles.push_back(make_pair("objects/previewPhysicsF.glsl", GL_FRAGMENT_SHADER));
        gPhysicsPreviewProgram.mShaderLevel = mShaderLevel[SHADER_OBJECT];
        success = gPhysicsPreviewProgram.createShader();
        gPhysicsPreviewProgram.mFeatures.hasLighting = false;
    }

    if (!success)
    {
        mShaderLevel[SHADER_OBJECT] = 0;
        return false;
    }

    return true;
}

bool LLViewerShaderMgr::loadShadersAvatar()
{
    LL_PROFILE_ZONE_SCOPED;
#if 1 // DEPRECATED -- forward rendering is deprecated
    bool success = true;

    if (mShaderLevel[SHADER_AVATAR] == 0)
    {
        gAvatarProgram.unload();
        gAvatarEyeballProgram.unload();
        gAvatarActorGhostProgram.unload();
        gAvatarEyeballActorGhostProgram.unload();
        return true;
    }

    if (success)
    {
        gAvatarProgram.mName = "Avatar Shader";
        gAvatarProgram.mFeatures.hasSkinning = true;
        gAvatarProgram.mFeatures.calculatesAtmospherics = true;
        gAvatarProgram.mFeatures.calculatesLighting = true;
        gAvatarProgram.mFeatures.hasGamma = true;
        gAvatarProgram.mFeatures.hasAtmospherics = true;
        gAvatarProgram.mFeatures.hasLighting = true;
        gAvatarProgram.mFeatures.hasAlphaMask = true;
        gAvatarProgram.mFeatures.hasActorFx = true;
        gAvatarProgram.mShaderFiles.clear();
        gAvatarProgram.mShaderFiles.push_back(make_pair("avatar/avatarV.glsl", GL_VERTEX_SHADER));
        gAvatarProgram.mShaderFiles.push_back(make_pair("avatar/avatarF.glsl", GL_FRAGMENT_SHADER));
        gAvatarProgram.mShaderLevel = mShaderLevel[SHADER_AVATAR];
        gAvatarProgram.addPermutation("HAS_ACTOR_FX", "1");
        success = gAvatarProgram.createShader();

        /// Keep track of avatar levels
        if (gAvatarProgram.mShaderLevel != mShaderLevel[SHADER_AVATAR])
        {
            mMaxAvatarShaderLevel = mShaderLevel[SHADER_AVATAR] = gAvatarProgram.mShaderLevel;
        }
    }

    if (success)
    {
        gAvatarEyeballProgram.mName = "Avatar Eyeball Program";
        gAvatarEyeballProgram.mFeatures.calculatesLighting = true;
        gAvatarEyeballProgram.mFeatures.isSpecular = true;
        gAvatarEyeballProgram.mFeatures.calculatesAtmospherics = true;
        gAvatarEyeballProgram.mFeatures.hasGamma = true;
        gAvatarEyeballProgram.mFeatures.hasAtmospherics = true;
        gAvatarEyeballProgram.mFeatures.hasLighting = true;
        gAvatarEyeballProgram.mFeatures.hasAlphaMask = true;
        gAvatarEyeballProgram.mFeatures.hasActorFx = true;
        gAvatarEyeballProgram.mShaderFiles.clear();
        gAvatarEyeballProgram.mShaderFiles.push_back(make_pair("avatar/eyeballV.glsl", GL_VERTEX_SHADER));
        gAvatarEyeballProgram.mShaderFiles.push_back(make_pair("avatar/eyeballF.glsl", GL_FRAGMENT_SHADER));
        gAvatarEyeballProgram.mShaderLevel = mShaderLevel[SHADER_AVATAR];
        gAvatarEyeballProgram.addPermutation("HAS_ACTOR_FX", "1");
        success = gAvatarEyeballProgram.createShader();
    }

    if (success)
    {
        // [ActorStyle] Dedicated WORLD/HDR actorghostF programs for the classic
        // system avatar.  They deliberately remain separate from the interface
        // Ghost Studio pair: classic body vertices use the avatar palette (not
        // objectSkinV), rigid eyes use their joint model matrix, and both need
        // world atmospherics plus an independent style colour that joint-mesh
        // texture setup cannot overwrite.  Optional/fail-open: native Actor FX
        // stays active if either replay program fails to link.
        gAvatarActorGhostProgram.mName = "Classic Avatar Actor Ghost Shader";
        gAvatarActorGhostProgram.mFeatures.hasSkinning = true;
        gAvatarActorGhostProgram.mFeatures.calculatesAtmospherics = true;
        gAvatarActorGhostProgram.mFeatures.hasAtmospherics = true;
        gAvatarActorGhostProgram.mFeatures.hasSrgb = true;
        gAvatarActorGhostProgram.mShaderFiles.clear();
        gAvatarActorGhostProgram.mShaderFiles.push_back(
            make_pair("avatar/avatarActorGhostV.glsl", GL_VERTEX_SHADER));
        gAvatarActorGhostProgram.mShaderFiles.push_back(
            make_pair("interface/actorghostF.glsl", GL_FRAGMENT_SHADER));
        gAvatarActorGhostProgram.mShaderLevel = mShaderLevel[SHADER_AVATAR];
        gAvatarActorGhostProgram.clearPermutations();
        gAvatarActorGhostProgram.addPermutation("GHOST_WORLD_PASS", "1");
        gAvatarActorGhostProgram.addPermutation("GHOST_SYSTEM_AVATAR", "1");
        gAvatarActorGhostProgram.addPermutation("GHOST_SHARED_DISSOLVE", "1");
        if (gSavedSettings.getBOOL("RenderAvatarCloth"))
        {
            gAvatarActorGhostProgram.addPermutation("AVATAR_CLOTH", "1");
        }
        const bool body_ghost_ok = gAvatarActorGhostProgram.createShader();

        gAvatarEyeballActorGhostProgram.mName = "Classic Avatar Eye Actor Ghost Shader";
        gAvatarEyeballActorGhostProgram.mFeatures.calculatesAtmospherics = true;
        gAvatarEyeballActorGhostProgram.mFeatures.hasAtmospherics = true;
        gAvatarEyeballActorGhostProgram.mFeatures.hasSrgb = true;
        gAvatarEyeballActorGhostProgram.mShaderFiles.clear();
        gAvatarEyeballActorGhostProgram.mShaderFiles.push_back(
            make_pair("avatar/eyeballActorGhostV.glsl", GL_VERTEX_SHADER));
        gAvatarEyeballActorGhostProgram.mShaderFiles.push_back(
            make_pair("interface/actorghostF.glsl", GL_FRAGMENT_SHADER));
        gAvatarEyeballActorGhostProgram.mShaderLevel = mShaderLevel[SHADER_AVATAR];
        gAvatarEyeballActorGhostProgram.clearPermutations();
        gAvatarEyeballActorGhostProgram.addPermutation("GHOST_WORLD_PASS", "1");
        gAvatarEyeballActorGhostProgram.addPermutation("GHOST_SYSTEM_AVATAR", "1");
        gAvatarEyeballActorGhostProgram.addPermutation("GHOST_SHARED_DISSOLVE", "1");
        const bool eye_ghost_ok = gAvatarEyeballActorGhostProgram.createShader();

        if (!body_ghost_ok || !eye_ghost_ok)
        {
            gAvatarActorGhostProgram.unload();
            gAvatarEyeballActorGhostProgram.unload();
            LL_WARNS("Shader") << "Classic avatar Actor FX replay shaders failed; "
                                  "keeping native Actor FX fail-open" << LL_ENDL;
        }
    }

    if( !success )
    {
        mShaderLevel[SHADER_AVATAR] = 0;
        mMaxAvatarShaderLevel = 0;
        return false;
    }
#endif
    return true;
}

bool LLViewerShaderMgr::loadShadersInterface()
{
    LL_PROFILE_ZONE_SCOPED;
    bool success = true;

    if (success)
    {
        gHighlightProgram.mName = "Highlight Shader";
        gHighlightProgram.mShaderFiles.clear();
        gHighlightProgram.mShaderFiles.push_back(make_pair("interface/highlightV.glsl", GL_VERTEX_SHADER));
        gHighlightProgram.mShaderFiles.push_back(make_pair("interface/highlightF.glsl", GL_FRAGMENT_SHADER));
        gHighlightProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        success = make_rigged_variant(gHighlightProgram, gSkinnedHighlightProgram);
        success = success && gHighlightProgram.createShader();
    }

    if (success)
    {
        // [ActorMover] pose-ghost FX shader (ALL model-ghost styles): the
        // highlight pair's skinned constant-colour transform plus animated
        // screen-space scanlines, a fresnel-ish rim boost, a subtle time
        // flicker (ghostTime / ghostParams) and the per-batch alpha stage
        // (ghostAux: mask-cutoff discard + texture-RGB mix) that lets masked
        // and blended clothing layers render faithfully on the ghost/clone.
        // Same registration idiom as gHighlightProgram (the rigged variant is
        // what drawGeometryGhost binds), but NON-FATAL like the indexed-PBR
        // extras: a compile failure only logs; every style then degrades to
        // the classic unmasked look on gHighlightProgram at draw time.
        gActorGhostProgram.mName = "Actor Ghost Shader";
        gActorGhostProgram.mShaderFiles.clear();
        gActorGhostProgram.mShaderFiles.push_back(make_pair("interface/actorghostV.glsl", GL_VERTEX_SHADER));
        gActorGhostProgram.mShaderFiles.push_back(make_pair("interface/actorghostF.glsl", GL_FRAGMENT_SHADER));
        gActorGhostProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        bool ghost_ok = make_rigged_variant(gActorGhostProgram, gSkinnedActorGhostProgram);
        ghost_ok = ghost_ok && gActorGhostProgram.createShader();
        if (!ghost_ok)
        {
            LL_WARNS("Shader") << "Actor ghost FX shader failed to load; the"
                                  " hologram / x-ray ghost styles will fall back"
                                  " to the classic translucent ghost" << LL_ENDL;
        }

        // [ActorStyle/SharedActivation] The live proxy is submitted into the
        // linear HDR world alpha stream, not Ghost Studio's late interface
        // compositor. Compile a separate pair with world atmospherics and the
        // authored rest-space dissolve law; keeping it separate guarantees the
        // existing path/studio ghost result remains byte-identical.
        gWorldActorGhostProgram.mName = "World Actor Ghost Shader";
        gWorldActorGhostProgram.mFeatures.calculatesAtmospherics = true;
        gWorldActorGhostProgram.mFeatures.hasAtmospherics = true;
        gWorldActorGhostProgram.mFeatures.hasSrgb = true;
        gWorldActorGhostProgram.mShaderFiles.clear();
        gWorldActorGhostProgram.mShaderFiles.push_back(
            make_pair("interface/actorghostV.glsl", GL_VERTEX_SHADER));
        gWorldActorGhostProgram.mShaderFiles.push_back(
            make_pair("interface/actorghostF.glsl", GL_FRAGMENT_SHADER));
        gWorldActorGhostProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        gWorldActorGhostProgram.clearPermutations();
        gWorldActorGhostProgram.addPermutation("GHOST_WORLD_PASS", "1");
        gWorldActorGhostProgram.addPermutation("GHOST_SHARED_DISSOLVE", "1");
        bool world_ghost_ok = make_rigged_variant(
            gWorldActorGhostProgram, gWorldSkinnedActorGhostProgram);
        world_ghost_ok = world_ghost_ok &&
            gWorldActorGhostProgram.createShader();
        if (!world_ghost_ok)
        {
            gWorldActorGhostProgram.unload();
            gWorldSkinnedActorGhostProgram.unload();
            LL_WARNS("Shader") << "World Actor FX replay shader failed; "
                                  "native Actor FX remains active fail-open"
                               << LL_ENDL;
        }

        // Native indexed legacy batches carry one material selector per
        // vertex. The shared world replay must consume that selector in one
        // draw as well; scalar slot filtering multiplies every depth/beauty/
        // bloom/wire sweep by the material count. Keep this optional and
        // independent of the scalar family so a link failure remains an
        // actor-wide native fail-open only for geometry that actually needs it.
        if (world_ghost_ok && LLGLSLShader::sIndexedTextureChannels >= 2)
        {
            const S32 indexed_channels = LLGLSLShader::sIndexedTextureChannels;
            gWorldActorGhostIndexedProgram.mName =
                "World Actor Ghost Indexed Shader";
            gWorldActorGhostIndexedProgram.mFeatures.calculatesAtmospherics = true;
            gWorldActorGhostIndexedProgram.mFeatures.hasAtmospherics = true;
            gWorldActorGhostIndexedProgram.mFeatures.hasSrgb = true;
            gWorldActorGhostIndexedProgram.mFeatures.mIndexedTextureChannels =
                indexed_channels;
            gWorldActorGhostIndexedProgram.mShaderFiles.clear();
            gWorldActorGhostIndexedProgram.mShaderFiles.push_back(
                make_pair("interface/actorghostV.glsl", GL_VERTEX_SHADER));
            gWorldActorGhostIndexedProgram.mShaderFiles.push_back(
                make_pair("interface/actorghostF.glsl", GL_FRAGMENT_SHADER));
            gWorldActorGhostIndexedProgram.mShaderLevel =
                mShaderLevel[SHADER_INTERFACE];
            gWorldActorGhostIndexedProgram.clearPermutations();
            gWorldActorGhostIndexedProgram.addPermutation(
                "GHOST_WORLD_PASS", "1");
            gWorldActorGhostIndexedProgram.addPermutation(
                "GHOST_SHARED_DISSOLVE", "1");
            gWorldActorGhostIndexedProgram.addPermutation(
                "GHOST_INDEXED_WORLD", "1");
            gWorldActorGhostIndexedProgram.addPermutation(
                "GHOST_INDEXED_CHANNELS", llformat("%d", indexed_channels));

            bool indexed_ghost_ok = make_rigged_variant(
                gWorldActorGhostIndexedProgram,
                gWorldSkinnedActorGhostIndexedProgram);
            indexed_ghost_ok = indexed_ghost_ok
                && gWorldActorGhostIndexedProgram.createShader();
            if (!indexed_ghost_ok)
            {
                gWorldActorGhostIndexedProgram.unload();
                gWorldSkinnedActorGhostIndexedProgram.unload();
                LL_WARNS("Shader")
                    << "Indexed world Actor FX replay shader failed; "
                       "multi-material actors remain native fail-open"
                    << LL_ENDL;
            }
        }
        else
        {
            gWorldActorGhostIndexedProgram.unload();
            gWorldSkinnedActorGhostIndexedProgram.unload();
        }
    }

    if (success)
    {
        gHighlightNormalProgram.mName = "Highlight Normals Shader";
        gHighlightNormalProgram.mShaderFiles.clear();
        gHighlightNormalProgram.mShaderFiles.push_back(make_pair("interface/highlightNormV.glsl", GL_VERTEX_SHADER));
        gHighlightNormalProgram.mShaderFiles.push_back(make_pair("interface/highlightF.glsl", GL_FRAGMENT_SHADER));
        gHighlightNormalProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        success = gHighlightNormalProgram.createShader();
    }

    if (success)
    {
        gHighlightSpecularProgram.mName = "Highlight Spec Shader";
        gHighlightSpecularProgram.mShaderFiles.clear();
        gHighlightSpecularProgram.mShaderFiles.push_back(make_pair("interface/highlightSpecV.glsl", GL_VERTEX_SHADER));
        gHighlightSpecularProgram.mShaderFiles.push_back(make_pair("interface/highlightF.glsl", GL_FRAGMENT_SHADER));
        gHighlightSpecularProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        success = gHighlightSpecularProgram.createShader();
    }

    if (success)
    {
        gUIProgram.mName = "UI Shader";
        gUIProgram.mShaderFiles.clear();
        gUIProgram.mShaderFiles.push_back(make_pair("interface/uiV.glsl", GL_VERTEX_SHADER));
        gUIProgram.mShaderFiles.push_back(make_pair("interface/uiF.glsl", GL_FRAGMENT_SHADER));
        gUIProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        success = gUIProgram.createShader();
        if (success)
        {
            // Initialize the shadow-path uniform to passthrough so non-text UI
            // and NO_SHADOW text take the early-return branch in uiF.glsl. GLSL
            // already zero-initializes uniforms, but pushing an explicit default
            // documents the contract and protects against driver quirks.
            // shadowMode is the shader's only shadow uniform — atlas texel size
            // and channel layout derive from the bound texture in uiF.glsl.
            static LLStaticHashedString sShadowMode("shadowMode");
            gUIProgram.bind();
            gUIProgram.uniform1i(sShadowMode, 0);
            gUIProgram.unbind();
        }
    }

    if (success)
    {
        gPathfindingProgram.mName = "Pathfinding Shader";
        gPathfindingProgram.mShaderFiles.clear();
        gPathfindingProgram.mShaderFiles.push_back(make_pair("interface/pathfindingV.glsl", GL_VERTEX_SHADER));
        gPathfindingProgram.mShaderFiles.push_back(make_pair("interface/pathfindingF.glsl", GL_FRAGMENT_SHADER));
        gPathfindingProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        success = gPathfindingProgram.createShader();
    }

    if (success)
    {
        gPathfindingNoNormalsProgram.mName = "PathfindingNoNormals Shader";
        gPathfindingNoNormalsProgram.mShaderFiles.clear();
        gPathfindingNoNormalsProgram.mShaderFiles.push_back(make_pair("interface/pathfindingNoNormalV.glsl", GL_VERTEX_SHADER));
        gPathfindingNoNormalsProgram.mShaderFiles.push_back(make_pair("interface/pathfindingF.glsl", GL_FRAGMENT_SHADER));
        gPathfindingNoNormalsProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        success = gPathfindingNoNormalsProgram.createShader();
    }

    if (success)
    {
        gGlowCombineProgram.mName = "Glow Combine Shader";
        gGlowCombineProgram.mShaderFiles.clear();
        gGlowCombineProgram.mShaderFiles.push_back(make_pair("interface/glowcombineV.glsl", GL_VERTEX_SHADER));
        gGlowCombineProgram.mShaderFiles.push_back(make_pair("interface/glowcombineF.glsl", GL_FRAGMENT_SHADER));
        gGlowCombineProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        success = gGlowCombineProgram.createShader();
    }

    if (success)
    {
        gGlowCombineFXAAProgram.mName = "Glow CombineFXAA Shader";
        gGlowCombineFXAAProgram.mShaderFiles.clear();
        gGlowCombineFXAAProgram.mShaderFiles.push_back(make_pair("interface/glowcombineFXAAV.glsl", GL_VERTEX_SHADER));
        gGlowCombineFXAAProgram.mShaderFiles.push_back(make_pair("interface/glowcombineFXAAF.glsl", GL_FRAGMENT_SHADER));
        gGlowCombineFXAAProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        success = gGlowCombineFXAAProgram.createShader();
    }

#ifdef LL_WINDOWS
    if (success)
    {
        gTwoTextureCompareProgram.mName = "Two Texture Compare Shader";
        gTwoTextureCompareProgram.mShaderFiles.clear();
        gTwoTextureCompareProgram.mShaderFiles.push_back(make_pair("interface/twotexturecompareV.glsl", GL_VERTEX_SHADER));
        gTwoTextureCompareProgram.mShaderFiles.push_back(make_pair("interface/twotexturecompareF.glsl", GL_FRAGMENT_SHADER));
        gTwoTextureCompareProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        success = gTwoTextureCompareProgram.createShader();
        if (success)
        {
            gTwoTextureCompareProgram.bind();
            gTwoTextureCompareProgram.uniform1i(sTex0, 0);
            gTwoTextureCompareProgram.uniform1i(sTex1, 1);
            gTwoTextureCompareProgram.uniform1i(sDitherTex, 2);
        }
    }

    if (success)
    {
        gOneTextureFilterProgram.mName = "One Texture Filter Shader";
        gOneTextureFilterProgram.mShaderFiles.clear();
        gOneTextureFilterProgram.mShaderFiles.push_back(make_pair("interface/onetexturefilterV.glsl", GL_VERTEX_SHADER));
        gOneTextureFilterProgram.mShaderFiles.push_back(make_pair("interface/onetexturefilterF.glsl", GL_FRAGMENT_SHADER));
        gOneTextureFilterProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        success = gOneTextureFilterProgram.createShader();
        if (success)
        {
            gOneTextureFilterProgram.bind();
            gOneTextureFilterProgram.uniform1i(sTex0, 0);
        }
    }
#endif

    if (success)
    {
        gSolidColorProgram.mName = "Solid Color Shader";
        gSolidColorProgram.mShaderFiles.clear();
        gSolidColorProgram.mShaderFiles.push_back(make_pair("interface/solidcolorV.glsl", GL_VERTEX_SHADER));
        gSolidColorProgram.mShaderFiles.push_back(make_pair("interface/solidcolorF.glsl", GL_FRAGMENT_SHADER));
        gSolidColorProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        success = gSolidColorProgram.createShader();
        if (success)
        {
            gSolidColorProgram.bind();
            gSolidColorProgram.uniform1i(sTex0, 0);
            gSolidColorProgram.unbind();
        }
    }

    if (success)
    {
        gOcclusionProgram.mName = "Occlusion Shader";
        gOcclusionProgram.mShaderFiles.clear();
        gOcclusionProgram.mShaderFiles.push_back(make_pair("interface/occlusionV.glsl", GL_VERTEX_SHADER));
        gOcclusionProgram.mShaderFiles.push_back(make_pair("interface/occlusionF.glsl", GL_FRAGMENT_SHADER));
        gOcclusionProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        gOcclusionProgram.mRiggedVariant = &gSkinnedOcclusionProgram;
        success = gOcclusionProgram.createShader();
    }

    if (success)
    {
        gSkinnedOcclusionProgram.mName = "Skinned Occlusion Shader";
        gSkinnedOcclusionProgram.mFeatures.hasObjectSkinning = true;
        gSkinnedOcclusionProgram.mShaderFiles.clear();
        gSkinnedOcclusionProgram.mShaderFiles.push_back(make_pair("interface/occlusionSkinnedV.glsl", GL_VERTEX_SHADER));
        gSkinnedOcclusionProgram.mShaderFiles.push_back(make_pair("interface/occlusionF.glsl", GL_FRAGMENT_SHADER));
        gSkinnedOcclusionProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        success = gSkinnedOcclusionProgram.createShader();
    }

    if (success)
    {
        gOcclusionCubeProgram.mName = "Occlusion Cube Shader";
        gOcclusionCubeProgram.mShaderFiles.clear();
        gOcclusionCubeProgram.mShaderFiles.push_back(make_pair("interface/occlusionCubeV.glsl", GL_VERTEX_SHADER));
        gOcclusionCubeProgram.mShaderFiles.push_back(make_pair("interface/occlusionF.glsl", GL_FRAGMENT_SHADER));
        gOcclusionCubeProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        success = gOcclusionCubeProgram.createShader();
    }

    if (success)
    {
        gDebugProgram.mName = "Debug Shader";
        gDebugProgram.mShaderFiles.clear();
        gDebugProgram.mShaderFiles.push_back(make_pair("interface/debugV.glsl", GL_VERTEX_SHADER));
        gDebugProgram.mShaderFiles.push_back(make_pair("interface/debugF.glsl", GL_FRAGMENT_SHADER));
        gDebugProgram.mRiggedVariant = &gSkinnedDebugProgram;
        gDebugProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        success = make_rigged_variant(gDebugProgram, gSkinnedDebugProgram);
        success = success && gDebugProgram.createShader();
    }

    if (success)
    {
        for (S32 variant = 0; variant < NORMAL_DEBUG_SHADER_COUNT; ++variant)
        {
            LLGLSLShader& shader = gNormalDebugProgram[variant];
            LLGLSLShader& skinned_shader = gSkinnedNormalDebugProgram[variant];
            shader.mName = "Normal Debug Shader";
            shader.mShaderFiles.clear();
            shader.mShaderFiles.push_back(make_pair("interface/normaldebugV.glsl", GL_VERTEX_SHADER));
            // *NOTE: Geometry shaders have a reputation for being slow.
            // Consider using compute shaders instead, which have a reputation
            // for being fast. This geometry shader in particular seems to run
            // fine on my machine, but I won't vouch for this in
            // performance-critical areas.  -Cosmic,2023-09-28
            shader.mShaderFiles.push_back(make_pair("interface/normaldebugG.glsl", GL_GEOMETRY_SHADER));
            shader.mShaderFiles.push_back(make_pair("interface/normaldebugF.glsl", GL_FRAGMENT_SHADER));
            shader.mRiggedVariant = &skinned_shader;
            shader.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
            if (variant == NORMAL_DEBUG_SHADER_WITH_TANGENTS)
            {
                shader.addPermutation("HAS_ATTRIBUTE_TANGENT", "1");
            }
            success = make_rigged_variant(shader, skinned_shader);
            success = success && shader.createShader();
        }
    }

    if (success)
    {
        gClipProgram.mName = "Clip Shader";
        gClipProgram.mShaderFiles.clear();
        gClipProgram.mShaderFiles.push_back(make_pair("interface/clipV.glsl", GL_VERTEX_SHADER));
        gClipProgram.mShaderFiles.push_back(make_pair("interface/clipF.glsl", GL_FRAGMENT_SHADER));
        gClipProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        success = gClipProgram.createShader();
    }

    if (success)
    {
        gBenchmarkProgram.mName = "Benchmark Shader";
        gBenchmarkProgram.mShaderFiles.clear();
        gBenchmarkProgram.mShaderFiles.push_back(make_pair("interface/benchmarkV.glsl", GL_VERTEX_SHADER));
        gBenchmarkProgram.mShaderFiles.push_back(make_pair("interface/benchmarkF.glsl", GL_FRAGMENT_SHADER));
        gBenchmarkProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        success = gBenchmarkProgram.createShader();
    }

    if (success)
    {
        gReflectionProbeDisplayProgram.mName = "Reflection Probe Display Shader";
        gReflectionProbeDisplayProgram.mFeatures.hasReflectionProbes = true;
        gReflectionProbeDisplayProgram.mFeatures.hasSrgb = true;
        gReflectionProbeDisplayProgram.mFeatures.calculatesAtmospherics = true;
        gReflectionProbeDisplayProgram.mFeatures.hasAtmospherics = true;
        gReflectionProbeDisplayProgram.mFeatures.hasGamma = true;
        gReflectionProbeDisplayProgram.mFeatures.isDeferred = true;
        gReflectionProbeDisplayProgram.mShaderFiles.clear();
        gReflectionProbeDisplayProgram.mShaderFiles.push_back(make_pair("interface/reflectionprobeV.glsl", GL_VERTEX_SHADER));
        gReflectionProbeDisplayProgram.mShaderFiles.push_back(make_pair("interface/reflectionprobeF.glsl", GL_FRAGMENT_SHADER));
        gReflectionProbeDisplayProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        success = gReflectionProbeDisplayProgram.createShader();
    }

    if (success)
    {
        gCopyProgram.mName = "Copy Shader";
        gCopyProgram.mShaderFiles.clear();
        gCopyProgram.mShaderFiles.push_back(make_pair("interface/copyV.glsl", GL_VERTEX_SHADER));
        gCopyProgram.mShaderFiles.push_back(make_pair("interface/copyF.glsl", GL_FRAGMENT_SHADER));
        gCopyProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        success = gCopyProgram.createShader();
    }

    if (success)
    {
        gDrawColorProgram.mName = "Draw Color Shader";
        gDrawColorProgram.mShaderFiles.clear();
        gDrawColorProgram.mShaderFiles.push_back(make_pair("objects/simpleNoAtmosV.glsl", GL_VERTEX_SHADER));
        gDrawColorProgram.mShaderFiles.push_back(make_pair("objects/simpleColorF.glsl", GL_FRAGMENT_SHADER));
        gDrawColorProgram.clearPermutations();
        gDrawColorProgram.mShaderLevel = mShaderLevel[SHADER_OBJECT];
        success = gDrawColorProgram.createShader();
    }

    if (gSavedSettings.getBOOL("LocalTerrainPaintEnabled"))
    {
        if (success)
        {
            LLGLSLShader* shader = &gPBRTerrainBakeProgram;
            U32 bit_depth = gSavedSettings.getU32("TerrainPaintBitDepth");
            // LLTerrainPaintMap currently uses an RGB8 texture internally
            bit_depth = llclamp(bit_depth, 1, 8);
            shader->mName = llformat("Terrain Bake Shader RGB%o", bit_depth);
            shader->mFeatures.isPBRTerrain = true;

            shader->mShaderFiles.clear();
            shader->mShaderFiles.push_back(make_pair("interface/pbrTerrainBakeV.glsl", GL_VERTEX_SHADER));
            shader->mShaderFiles.push_back(make_pair("interface/pbrTerrainBakeF.glsl", GL_FRAGMENT_SHADER));
            shader->mShaderLevel = mShaderLevel[SHADER_INTERFACE];
            const U32 value_range = (1 << bit_depth) - 1;
            shader->addPermutation("TERRAIN_PAINT_PRECISION", llformat("%d", value_range));
            success = success && shader->createShader();
            //llassert(success);
            if (!success)
            {
                LL_WARNS() << "Failed to create shader '" << shader->mName << "', disabling!" << LL_ENDL;
                gSavedSettings.setBOOL("RenderCanUseTerrainBakeShaders", false);
                // continue as if this shader never happened
                success = true;
            }
        }
    }

    if (success)
    {
        gAlphaMaskProgram.mName = "Alpha Mask Shader";
        gAlphaMaskProgram.mShaderFiles.clear();
        gAlphaMaskProgram.mShaderFiles.push_back(make_pair("interface/alphamaskV.glsl", GL_VERTEX_SHADER));
        gAlphaMaskProgram.mShaderFiles.push_back(make_pair("interface/alphamaskF.glsl", GL_FRAGMENT_SHADER));
        gAlphaMaskProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        success = gAlphaMaskProgram.createShader();
    }

    if (success)
    {
        gReflectionMipProgram.mName = "Reflection Mip Shader";
        gReflectionMipProgram.mFeatures.isDeferred = true;
        gReflectionMipProgram.mFeatures.hasGamma = true;
        gReflectionMipProgram.mFeatures.hasAtmospherics = true;
        gReflectionMipProgram.mFeatures.calculatesAtmospherics = true;
        gReflectionMipProgram.mShaderFiles.clear();
        gReflectionMipProgram.mShaderFiles.push_back(make_pair("interface/splattexturerectV.glsl", GL_VERTEX_SHADER));
        gReflectionMipProgram.mShaderFiles.push_back(make_pair("interface/reflectionmipF.glsl", GL_FRAGMENT_SHADER));
        gReflectionMipProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        success = gReflectionMipProgram.createShader();
    }

    if (success)
    {
        gGaussianProgram.mName = "Reflection Mip Shader";
        gGaussianProgram.mFeatures.isDeferred = true;
        gGaussianProgram.mFeatures.hasGamma = true;
        gGaussianProgram.mFeatures.hasAtmospherics = true;
        gGaussianProgram.mFeatures.calculatesAtmospherics = true;
        gGaussianProgram.mShaderFiles.clear();
        gGaussianProgram.mShaderFiles.push_back(make_pair("interface/splattexturerectV.glsl", GL_VERTEX_SHADER));
        gGaussianProgram.mShaderFiles.push_back(make_pair("interface/gaussianF.glsl", GL_FRAGMENT_SHADER));
        gGaussianProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        success = gGaussianProgram.createShader();
    }

    if (success && gGLManager.mHasCubeMapArray)
    {
        gRadianceGenProgram.mName = "Radiance Gen Shader";
        gRadianceGenProgram.mShaderFiles.clear();
        gRadianceGenProgram.mShaderFiles.push_back(make_pair("interface/radianceGenV.glsl", GL_VERTEX_SHADER));
        gRadianceGenProgram.mShaderFiles.push_back(make_pair("interface/radianceGenF.glsl", GL_FRAGMENT_SHADER));
        gRadianceGenProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        gRadianceGenProgram.addPermutation("PROBE_FILTER_SAMPLES", "32");
        success = gRadianceGenProgram.createShader();
    }

    if (success && gGLManager.mHasCubeMapArray)
    {
        gHeroRadianceGenProgram.mName = "Hero Radiance Gen Shader";
        gHeroRadianceGenProgram.mShaderFiles.clear();
        gHeroRadianceGenProgram.mShaderFiles.push_back(make_pair("interface/radianceGenV.glsl", GL_VERTEX_SHADER));
        gHeroRadianceGenProgram.mShaderFiles.push_back(make_pair("interface/radianceGenF.glsl", GL_FRAGMENT_SHADER));
        gHeroRadianceGenProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        gHeroRadianceGenProgram.addPermutation("HERO_PROBES", "1");
        gHeroRadianceGenProgram.addPermutation("PROBE_FILTER_SAMPLES", "4");
        success                              = gHeroRadianceGenProgram.createShader();
    }

    if (success && gGLManager.mHasCubeMapArray)
    {
        gIrradianceGenProgram.mName = "Irradiance Gen Shader";
        gIrradianceGenProgram.mShaderFiles.clear();
        gIrradianceGenProgram.mShaderFiles.push_back(make_pair("interface/irradianceGenV.glsl", GL_VERTEX_SHADER));
        gIrradianceGenProgram.mShaderFiles.push_back(make_pair("interface/irradianceGenF.glsl", GL_FRAGMENT_SHADER));
        gIrradianceGenProgram.mShaderLevel = mShaderLevel[SHADER_INTERFACE];
        success = gIrradianceGenProgram.createShader();
    }

    if( !success )
    {
        mShaderLevel[SHADER_INTERFACE] = 0;
        return false;
    }

    return true;
}


std::string LLViewerShaderMgr::getShaderDirPrefix(void)
{
    return gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "shaders", "class");
}

void LLViewerShaderMgr::updateShaderUniforms(LLGLSLShader * shader)
{
    LLEnvironment::instance().updateShaderUniforms(shader);
}

LLViewerShaderMgr::shader_iter LLViewerShaderMgr::beginShaders() const
{
    return mShaderList.begin();
}

LLViewerShaderMgr::shader_iter LLViewerShaderMgr::endShaders() const
{
    return mShaderList.end();
}
