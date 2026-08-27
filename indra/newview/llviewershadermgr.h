/**
 * @file llviewershadermgr.h
 * @brief Viewer Shader Manager
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

#ifndef LL_VIEWER_SHADER_MGR_H
#define LL_VIEWER_SHADER_MGR_H

#include "llshadermgr.h"
#include "llmaterial.h"

#define LL_DEFERRED_MULTI_LIGHT_COUNT 16

// Cine-rig lens flare: max rig projector sources uploaded per frame to
// uCineFlareA/uCineFlareColor (postEffectUtilsF.glsl AL_CINE_FLARE_MAX must
// match — LIGHT_COUNT(4) rig lights per rig, cap sized for ~2 subjects).
#define AL_CINE_FLARE_MAX 8

class LLViewerShaderMgr: public LLShaderMgr
{
public:
    static bool sInitialized;
    static bool sSkipReload;

    LLViewerShaderMgr();
    /* virtual */ ~LLViewerShaderMgr();

    // Add shaders to mShaderList for later uniform propagation
    // Will assert on redundant shader entries in debug builds
    void finalizeShaderList();

    // singleton pattern implementation
    static LLViewerShaderMgr * instance();
    static void releaseInstance();

    void initAttribsAndUniforms(void);
    void setShaders();
    void unloadShaders();
    S32  getShaderLevel(S32 type);

    // loadBasicShaders in case of a failure returns
    // name of a file error happened at, otherwise
    // returns an empty string
    std::string loadBasicShaders();
    bool loadShadersEffects();
    bool loadShadersDeferred();
    bool loadShadersObject();
    bool loadShadersAvatar();
    bool loadShadersWater();
    bool loadShadersInterface();

    // True only while both authored Actor FX fragment modules are available.
    // Shared replay must not publish programs linked against identity fallbacks,
    // because it suppresses the matching native material pass after preflight.
    static bool hasPrimaryActorFxModules();

    // True only when every shared Actor FX forward-PBR beauty/glow program,
    // including its rigged variant, is linked and safe for replay dispatch.
    static bool hasSharedActorFxPBRShaders();

    std::vector<S32> mShaderLevel;
    S32 mMaxAvatarShaderLevel;

    enum EShaderClass
    {
        SHADER_LIGHTING,
        SHADER_OBJECT,
        SHADER_AVATAR,
        SHADER_ENVIRONMENT,
        SHADER_INTERFACE,
        SHADER_EFFECT,
        SHADER_WINDLIGHT,
        SHADER_WATER,
        SHADER_DEFERRED,
        SHADER_COUNT
    };

    // simple model of forward iterator
    // http://www.sgi.com/tech/stl/ForwardIterator.html
    class shader_iter
    {
    private:
        friend bool operator == (shader_iter const & a, shader_iter const & b);
        friend bool operator != (shader_iter const & a, shader_iter const & b);

        typedef std::vector<LLGLSLShader *>::const_iterator base_iter_t;
    public:
        shader_iter()
        {
        }

        shader_iter(base_iter_t iter) : mIter(iter)
        {
        }

        LLGLSLShader & operator * () const
        {
            return **mIter;
        }

        LLGLSLShader * operator -> () const
        {
            return *mIter;
        }

        shader_iter & operator++ ()
        {
            ++mIter;
            return *this;
        }

        shader_iter operator++ (int)
        {
            return mIter++;
        }

    private:
        base_iter_t mIter;
    };

    shader_iter beginShaders() const;
    shader_iter endShaders() const;

    /* virtual */ std::string getShaderDirPrefix(void);

    /* virtual */ void updateShaderUniforms(LLGLSLShader * shader);

private:
    // the list of shaders we need to propagate parameters to.
    std::vector<LLGLSLShader *> mShaderList;

}; //LLViewerShaderMgr

inline bool operator == (LLViewerShaderMgr::shader_iter const & a, LLViewerShaderMgr::shader_iter const & b)
{
    return a.mIter == b.mIter;
}

inline bool operator != (LLViewerShaderMgr::shader_iter const & a, LLViewerShaderMgr::shader_iter const & b)
{
    return a.mIter != b.mIter;
}

extern LLVector4            gShinyOrigin;

//utility shaders
extern LLGLSLShader         gOcclusionProgram;
extern LLGLSLShader         gOcclusionCubeProgram;
extern LLGLSLShader         gGlowCombineProgram;
extern LLGLSLShader         gReflectionMipProgram;
extern LLGLSLShader         gGaussianProgram;
extern LLGLSLShader         gRadianceGenProgram;
extern LLGLSLShader         gHeroRadianceGenProgram;
extern LLGLSLShader         gIrradianceGenProgram;
extern LLGLSLShader         gGlowCombineFXAAProgram;
extern LLGLSLShader         gDebugProgram;
enum NormalDebugShaderVariant : S32
{
    NORMAL_DEBUG_SHADER_DEFAULT,
    NORMAL_DEBUG_SHADER_WITH_TANGENTS,
    NORMAL_DEBUG_SHADER_COUNT
};
extern LLGLSLShader         gNormalDebugProgram[NORMAL_DEBUG_SHADER_COUNT];
extern LLGLSLShader         gSkinnedNormalDebugProgram[NORMAL_DEBUG_SHADER_COUNT];
extern LLGLSLShader         gClipProgram;
extern LLGLSLShader         gBenchmarkProgram;
extern LLGLSLShader         gReflectionProbeDisplayProgram;
extern LLGLSLShader         gCopyProgram;
extern LLGLSLShader         gPBRTerrainBakeProgram;
extern LLGLSLShader         gDrawColorProgram;

//output tex0[tc0] - tex1[tc1]
extern LLGLSLShader         gTwoTextureCompareProgram;
//discard some fragments based on user-set color tolerance
extern LLGLSLShader         gOneTextureFilterProgram;


//object shaders
extern LLGLSLShader     gObjectPreviewProgram;
extern LLGLSLShader        gPhysicsPreviewProgram;
extern LLGLSLShader     gObjectBumpProgram;
extern LLGLSLShader        gSkinnedObjectBumpProgram;
extern LLGLSLShader     gObjectAlphaMaskNoColorProgram;

//environment shaders
extern LLGLSLShader         gWaterProgram;
extern LLGLSLShader         gUnderWaterProgram;
extern LLGLSLShader         gGlowProgram;
extern LLGLSLShader         gGlowExtractProgram;
extern LLGLSLShader         gBloomExtractProgram;
extern LLGLSLShader         gBloomDownsampleProgram;
extern LLGLSLShader         gBloomDownsampleFirstProgram;
extern LLGLSLShader         gBloomUpsampleProgram;
extern LLGLSLShader         gBloomCompositeProgram;

//interface shaders
extern LLGLSLShader         gHighlightProgram;
extern LLGLSLShader         gHighlightNormalProgram;
extern LLGLSLShader         gHighlightSpecularProgram;
// [ActorMover] pose-ghost FX (hologram / x-ray styles); the rigged variant is
// what drawGeometryGhost binds. Optional: draw code falls back to the classic
// ghost when this failed to compile.
extern LLGLSLShader         gActorGhostProgram;
// Main-world/HDR variants used only by shared live Actor FX. Ghost Studio keeps
// gActorGhostProgram's historical display-space path.
extern LLGLSLShader         gWorldActorGhostProgram;
extern LLGLSLShader         gWorldSkinnedActorGhostProgram;
// One-draw indexed legacy variants for shared live Actor FX. These use the
// full native tex0..texN width for simple and material batches; Ghost Studio
// deliberately remains on its historical scalar per-slot program.
extern LLGLSLShader         gWorldActorGhostIndexedProgram;
extern LLGLSLShader         gWorldSkinnedActorGhostIndexedProgram;
// World/HDR classic-avatar replays of actorghostF.  The skinned program consumes
// the system avatar's classic palette; the rigid program covers eyeballs.
extern LLGLSLShader         gAvatarActorGhostProgram;
extern LLGLSLShader         gAvatarEyeballActorGhostProgram;

extern LLGLSLShader         gDeferredHighlightProgram;

extern LLGLSLShader         gPathfindingProgram;
extern LLGLSLShader         gPathfindingNoNormalsProgram;

// avatar shader handles
extern LLGLSLShader         gAvatarProgram;
extern LLGLSLShader         gAvatarEyeballProgram;
extern LLGLSLShader         gImpostorProgram;

// Post Process Shaders
extern LLGLSLShader         gPostScreenSpaceReflectionProgram;

// Deferred rendering shaders
extern LLGLSLShader         gDeferredImpostorProgram;
extern LLGLSLShader         gPrismLensProgram;
extern LLGLSLShader         gDeferredDiffuseProgram;
extern LLGLSLShader         gDeferredDiffuseAlphaMaskProgram;
extern LLGLSLShader         gDeferredNonIndexedDiffuseAlphaMaskProgram;
extern LLGLSLShader         gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram;
extern LLGLSLShader         gDeferredNonIndexedDiffuseProgram;
extern LLGLSLShader         gDeferredBumpProgram;
extern LLGLSLShader         gDeferredTerrainProgram;
extern LLGLSLShader         gDeferredTreeProgram;
extern LLGLSLShader         gDeferredTreeShadowProgram;
extern LLGLSLShader         gDeferredLightProgram;
extern LLGLSLShader         gDeferredMultiLightProgram[LL_DEFERRED_MULTI_LIGHT_COUNT];
extern LLGLSLShader         gDeferredSpotLightProgram;
extern LLGLSLShader         gDeferredMultiSpotLightProgram;
extern LLGLSLShader         gDeferredSunProgram;
extern LLGLSLShader         gDeferredSunProbeProgram;
extern LLGLSLShader         gHazeProgram;
extern LLGLSLShader         gHazeWaterProgram;
extern LLGLSLShader         gDeferredBlurLightProgram;
extern LLGLSLShader         gDeferredAvatarProgram;
extern LLGLSLShader         gDeferredSoftenProgram;
extern LLGLSLShader         gVisibleDiffuseSeedProgram;
extern LLGLSLShader         gDeferredShadowProgram;
extern LLGLSLShader         gDeferredShadowCubeProgram;
extern LLGLSLShader         gDeferredShadowAlphaMaskProgram;
extern LLGLSLShader         gDeferredShadowGLTFAlphaMaskProgram;
extern LLGLSLShader         gDeferredShadowGLTFAlphaMaskIndexedProgram; // multi-material indexed
extern LLGLSLShader         gDeferredShadowMaterialIndexedProgram; // multi-material indexed legacy mask shadow
extern LLGLSLShader         gDeferredShadowGLTFAlphaBlendProgram;
extern LLGLSLShader         gDeferredShadowFullbrightAlphaMaskProgram;
extern LLGLSLShader         gDeferredPostProgram;
extern LLGLSLShader         gDeferredPostProgramNoNear;
extern LLGLSLShader         gDeferredCoFProgram;
extern LLGLSLShader         gDeferredDoFCombineProgram;
extern LLGLSLShader         gFXAAProgram[4];
extern LLGLSLShader         gSMAAEdgeDetectProgram[4];
extern LLGLSLShader         gSMAABlendWeightsProgram[4];
extern LLGLSLShader         gSMAANeighborhoodBlendProgram[4];
extern LLGLSLShader         gCASProgram;
extern LLGLSLShader         gCineFisheyeProgram;
// [Ultimate Diopter] pass 1 gather at four tap-count quality tiers + pass 2 composite
extern LLGLSLShader         gUltimateDiopterGatherProgram[4];
extern LLGLSLShader         gUltimateDiopterProgram;
// [BDMerge G3.2] volumetric lighting (donor: Black Dragon)
extern LLGLSLShader         gVolumetricLightProgram;
// [Cine Outline Phase 1] deferred normal/depth outline post pass
extern LLGLSLShader         gCineOutlineProgram;
// [BDMerge G3.3] per-projector volumetric light cones (visible spotlight shafts)
extern LLGLSLShader         gDeferredProjectorVolumetricProgram;
extern LLGLSLShader         gDeferredProjectorVolumetricUpsampleProgram; // [BDMerge G3.3 P1 item 3]
extern LLGLSLShader         gDeferredProjectorVolumetricTemporalProgram; // [BDMerge G3.3 Batch 1 A]
extern LLGLSLShader         gDeferredProjectorVolumetricBloomFeedProgram; // [BDMerge G3.3 P3 item 4]
// Viewer-native cinematic weather: volumetric rain, surface response, lightning.
extern LLGLSLShader         gDeferredWeatherRainProgram;
extern LLGLSLShader         gDeferredWeatherRainOcclusionProgram;
extern LLGLSLShader         gDeferredWeatherRainUpsampleProgram;
extern LLGLSLShader         gDeferredWeatherSurfaceProgram;
extern LLGLSLShader         gDeferredWeatherSurfaceOcclusionProgram;
extern LLGLSLShader         gDeferredWeatherLightningProgram;
extern LLGLSLShader         gDeferredWeatherLightningQualityProgram;
extern LLGLSLShader         gDeferredPostNoDoFProgram;
extern LLGLSLShader         gExposureProgram;
extern LLGLSLShader         gExposureProgramNoFade;
extern LLGLSLShader         gOnLensFiltersProgram; // ND + polarizer pre-pass (pre-bloom)
extern LLGLSLShader         gLuminanceProgram;
extern LLGLSLShader         gDeferredAvatarShadowProgram;
extern LLGLSLShader         gDeferredAvatarAlphaShadowProgram;
extern LLGLSLShader         gDeferredAvatarAlphaMaskShadowProgram;
extern LLGLSLShader         gDeferredAlphaProgram;
extern LLGLSLShader         gHUDAlphaProgram;
extern LLGLSLShader         gDeferredAlphaImpostorProgram;
extern LLGLSLShader         gDeferredFullbrightProgram;
extern LLGLSLShader         gHUDFullbrightProgram;
extern LLGLSLShader         gDeferredFullbrightAlphaMaskProgram;
extern LLGLSLShader         gHUDFullbrightAlphaMaskProgram;
extern LLGLSLShader         gDeferredFullbrightAlphaMaskAlphaProgram;
extern LLGLSLShader         gHUDFullbrightAlphaMaskAlphaProgram;
extern LLGLSLShader         gDeferredEmissiveProgram;
extern LLGLSLShader         gDeferredEmissiveIndexedProgram; // multi-material indexed legacy glow
extern LLGLSLShader         gActorFxGlowProgram;              // zero-authored-glow legacy alpha
extern LLGLSLShader         gActorFxPBRGlowProgram;           // zero-authored-glow PBR alpha
extern LLGLSLShader         gDeferredAvatarEyesProgram;
extern LLGLSLShader         gDeferredAvatarAlphaProgram;
extern LLGLSLShader         gEnvironmentMapProgram;
extern LLGLSLShader         gDeferredWLSkyProgram;
extern LLGLSLShader         gDeferredWLCloudProgram;
extern LLGLSLShader         gDeferredWLSunProgram;
extern LLGLSLShader         gDeferredWLMoonProgram;
extern LLGLSLShader         gDeferredStarProgram;
extern LLGLSLShader         gDeferredMeteorProgram;
extern LLGLSLShader         gDeferredAuroraProgram;
extern LLGLSLShader         gDeferredFullbrightShinyProgram;
extern LLGLSLShader         gHUDFullbrightShinyProgram;
extern LLGLSLShader         gNormalMapGenProgram;
extern LLGLSLShader         gDeferredGenBrdfLutProgram;
extern LLGLSLShader         gDeferredBufferVisualProgram;
// [BDMerge A5.4-1a] velocity / motion-vector pass programs (rigid + camera).
// Skinned/rigged + avatar variants are Phase 1b.
extern LLGLSLShader         gVelocityProgram;
extern LLGLSLShader         gVelocityAlphaProgram;
extern LLGLSLShader         gVelocityPBRAlphaProgram;
extern LLGLSLShader         gVelocityAlphaIndexedProgram;
extern LLGLSLShader         gVelocityPBRAlphaIndexedProgram;
extern LLGLSLShader         gVelocityDebugProgram;
extern LLGLSLShader         gVelocitySkinnedProgram;        // [BDMerge A5.4-1b]
extern LLGLSLShader         gVelocityAlphaSkinnedProgram;   // [BDMerge A5.4-1b]
extern LLGLSLShader         gVelocityPBRAlphaSkinnedProgram;
extern LLGLSLShader         gVelocityAlphaIndexedSkinnedProgram;
extern LLGLSLShader         gVelocityPBRAlphaIndexedSkinnedProgram;
extern LLGLSLShader         gAvatarVelocityProgram;         // [BDMerge A5.4-1b] classic avatar
extern LLGLSLShader         gDeferredMotionBlurProgram;     // [BDMerge A5.4-3]
// [BDMerge A5.4-1c] fullscreen camera-motion fallback (fills avatar/sky/uncovered
// pixels with camera-induced motion before the geometry velocity stamps).
extern LLGLSLShader         gVelocityCameraProgram;
// [BDMerge Froxel F0] hybrid froxel volumetrics: P1 media pass + debug visualizer.
extern LLGLSLShader         gFroxelMediaProgram;
extern LLGLSLShader         gFroxelDebugProgram;
// [BDMerge Froxel F1] P4 integrate + P5 apply passes.
extern LLGLSLShader         gFroxelIntegrateProgram;
extern LLGLSLShader         gFroxelApplyProgram;
// [BDMerge Froxel F2] P2 per-light injection pass.
extern LLGLSLShader         gFroxelInjectProgram;
// [BDMerge Froxel F3] P3 froxel-space temporal resolve pass.
extern LLGLSLShader         gFroxelTemporalProgram;
extern LLGLSLShader         gBlitWithEffectsProgram;
extern LLGLSLShader         gCGGammaProgram;
extern LLGLSLShader         gCGLegacyGammaProgram;
extern LLGLSLShader         gCGTonemapProgram;
extern LLGLSLShader         gCGTonemapLegacyGammaProgram;
extern LLGLSLShader         gCGColorgradeGammaProgram;
extern LLGLSLShader         gCGColorgradeLegacyGammaProgram;
extern LLGLSLShader         gCGTonemapColorgradeProgram;
extern LLGLSLShader         gCGTonemapColorgradeLegacyGammaProgram;
// [RLVa:KB] - @setsphere
extern LLGLSLShader         gRlvSphereProgram;
// [/RLVa:KB]

// Deferred materials shaders
extern LLGLSLShader         gDeferredMaterialProgram[LLMaterial::SHADER_COUNT*2];
extern LLGLSLShader         gDeferredMaterialIndexedProgram[LLMaterial::SHADER_COUNT*2]; // multi-material indexed (GBuffer masks only)

extern LLGLSLShader         gHUDPBROpaqueProgram;
extern LLGLSLShader         gPBRGlowProgram;
extern LLGLSLShader         gPBRGlowIndexedProgram; // multi-material indexed PBR glow
extern LLGLSLShader         gDeferredPBROpaqueProgram;
extern LLGLSLShader         gDeferredPBROpaqueIndexedProgram; // multi-material indexed PBR opaque
extern LLGLSLShader         gDeferredPBRAlphaProgram;
extern LLGLSLShader         gHUDPBRAlphaProgram;

// Optional, fail-open forward PBR family for the shared live Actor FX replay.
// The alpha-mode index is a replay API contract rather than LLMaterial's enum
// ordering (LLMaterial orders BLEND before MASK).
enum ESharedActorFxPBRAlphaMode : U32
{
    SHARED_ACTOR_FX_PBR_ALPHA_OPAQUE = 0,
    SHARED_ACTOR_FX_PBR_ALPHA_MASK,
    SHARED_ACTOR_FX_PBR_ALPHA_BLEND,
    SHARED_ACTOR_FX_PBR_ALPHA_COUNT
};

extern LLGLSLShader gSharedActorFxPBRProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
extern LLGLSLShader gSharedActorFxSkinnedPBRProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
// Slot-named variants advertise TYPE_TEXTURE_INDEX and evaluate the complete
// indexed material array in one geometry draw. Scalar variants deliberately do
// not expose that input.
extern LLGLSLShader gSharedActorFxPBRSlotProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
extern LLGLSLShader gSharedActorFxSkinnedPBRSlotProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
// Authored glow consumes TYPE_EMISSIVE; synthetic glow intentionally consumes
// only the colour stream so zero-glow PBR VBOs remain replayable.
extern LLGLSLShader gSharedActorFxPBRGlowProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
extern LLGLSLShader gSharedActorFxSkinnedPBRGlowProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
extern LLGLSLShader gSharedActorFxPBRGlowSlotProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
extern LLGLSLShader gSharedActorFxSkinnedPBRGlowSlotProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
extern LLGLSLShader gSharedActorFxPBRSyntheticGlowProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
extern LLGLSLShader gSharedActorFxSkinnedPBRSyntheticGlowProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
extern LLGLSLShader gSharedActorFxPBRSyntheticGlowSlotProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
extern LLGLSLShader gSharedActorFxSkinnedPBRSyntheticGlowSlotProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
// Minimal solid depth prime. Only OPAQUE and MASK elements are linked/used;
// BLEND never writes the shared solid depth surface.
extern LLGLSLShader gSharedActorFxPBRDepthProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
extern LLGLSLShader gSharedActorFxSkinnedPBRDepthProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
extern LLGLSLShader gSharedActorFxPBRDepthSlotProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];
extern LLGLSLShader gSharedActorFxSkinnedPBRDepthSlotProgram[SHARED_ACTOR_FX_PBR_ALPHA_COUNT];

// Encodes detail level for dropping textures, in accordance with the GLTF spec where possible
// 0 is highest detail, -1 drops emissive, etc
// Dropping metallic roughness is off-spec - Reserve for potato machines as needed
// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#additional-textures
enum TerrainPBRDetail : S32
{
    TERRAIN_PBR_DETAIL_MAX                = 0,
    TERRAIN_PBR_DETAIL_EMISSIVE           = 0,
    TERRAIN_PBR_DETAIL_OCCLUSION          = -1,
    TERRAIN_PBR_DETAIL_NORMAL             = -2,
    TERRAIN_PBR_DETAIL_METALLIC_ROUGHNESS = -3,
    TERRAIN_PBR_DETAIL_BASE_COLOR         = -4,
    TERRAIN_PBR_DETAIL_MIN                = -4,
};
enum TerrainPaintType : U32
{
    // Use LLVLComposition::mDatap (heightmap) generated by generateHeights, plus noise from TERRAIN_ALPHARAMP
    TERRAIN_PAINT_TYPE_HEIGHTMAP_WITH_NOISE = 0,
    // Use paint map if PBR terrain, otherwise fall back to TERRAIN_PAINT_TYPE_HEIGHTMAP_WITH_NOISE
    TERRAIN_PAINT_TYPE_PBR_PAINTMAP         = 1,
    TERRAIN_PAINT_TYPE_COUNT                = 2,
};
extern LLGLSLShader         gDeferredPBRTerrainProgram[TERRAIN_PAINT_TYPE_COUNT];
#endif
