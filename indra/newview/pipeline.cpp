/**
 * @file pipeline.cpp
 * @brief Rendering pipeline.
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

#include "pipeline.h"
#include "alweathermodel.h"

// library includes
#include "llimagebmp.h"
#include "llimagejpeg.h"
#include "llimagepng.h"
#include "llimagetga.h"
#include "llimagewebp.h"
#include "llaudioengine.h" // For debugging.
#include "llerror.h"
#include "llviewercontrol.h"
#include "llfasttimer.h"
#include "llfontgl.h"
#include "llfontvertexbuffer.h"
#include "llnamevalue.h"
#include "llpointer.h"
#include "llprimitive.h"
#include "llvolume.h"
#include "material_codes.h"
#include "v3color.h"
#include "llui.h"
#include "llglheaders.h"
#include "llrender.h"
#include "llrender2dutils.h" // [F8] gl_rect_2d, for renderCompositionGuideOverlay()
#include "llsmoothstep.h"
#include "llstartup.h"
#include "llwindow.h"   // swapBuffers()

#include <array> // [F8] renderCompositionGuideOverlay()
#include <cmath>

// newview includes
#include "llagent.h"
#include "llagentcamera.h"
#include "llappviewer.h"
#include "lltexturecache.h"
#include "lltexturefetch.h"
#include "llimageworker.h"
#include "lldrawable.h"
#include "lldrawpoolalpha.h"
#include "lldrawpoolavatar.h"
#include "lldrawpoolbump.h"
#include "lldrawpooltree.h"
#include "lldrawpoolwater.h"
#include "llface.h"
#include "llfeaturemanager.h"
#include "llfloatertelehub.h"
#include "llfloaterreg.h"
#include "llhudmanager.h"
#include "llhudnametag.h"
#include "llhudtext.h"
#include "lllightconstants.h"
#include "llmeshrepository.h"
#include "llpipelinelistener.h"
#include "llpresentationtime.h"
#include "llprismlens.h"
#include "llreshadebridge.h"
#include "llresmgr.h"
#include "llselectmgr.h"
#include "llsky.h"
#include "lltracker.h"
#include "lltool.h"
#include "lltoolmgr.h"
#include "llviewercamera.h"
#include "llviewermediafocus.h"
#include "llviewertexturelist.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h" // for audio debugging.
#include "llviewerwindow.h" // For getSpinAxis
#include "llvoavatarself.h"
#include "llviewerjointattachment.h"
#include "llvocache.h"
#include "llvosky.h"
#include "llvowlsky.h"
#include "llvotree.h"
#include "llvovolume.h"
#include "llvosurfacepatch.h"
#include "llvowater.h"
#include "llvotree.h"
#include "llvopartgroup.h"
#include "llworld.h"
#include "llcubemap.h"
#include "llviewershadermgr.h"
#include "llactormover.h"               // [GhostDeferred] clone proxy queue
#include "llfetchedgltfmaterial.h"      // [GhostDeferred] GLTF clone batches
#include "llviewertexture.h"            // [GhostDeferred] fallback textures
#include "llghostdeferreddiagnostics.h" // [GhostDeferred] (invariant checker fwd use)
#include <cstring>                      // [GhostDeferred] memcpy (GLTF indexed)
#include "llviewerstats.h"
#include "llviewerjoystick.h"
#include "llviewerdisplay.h"
#include "llspatialpartition.h"
#include "llmutelist.h"
#include "lltoolpie.h"
#include "llnotifications.h"
#include "llpathinglib.h"
#include "llfloaterpathfindingconsole.h"
#include "llfloaterpathfindingcharacters.h"
#include "llfloatertools.h"
#include "llpanelface.h"
#include "llpathfindingpathtool.h"
#include "llscenemonitor.h"
#include "llprogressview.h"
#include "llcleanup.h"
#include "lutcube.h"
#include "allpm.h"
// [RLVa:KB] - Checked: RLVa-2.0.0
#include "llvisualeffect.h"
#include "rlvactions.h"
#include "rlvlocks.h"
// [/RLVa:KB]

#include "llenvironment.h"
#include "llsettingsvo.h"

#include "SMAAAreaTex.h"
#include "SMAASearchTex.h"
#include "llerror.h"

#if LL_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-variable"
#elif LL_GNUC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wrestrict"
#endif
#ifndef LL_WINDOWS
#define A_GCC 1
#endif
#define A_CPU 1
#include "app_settings/shaders/class1/deferred/CASF.glsl" // This is also C++
#if LL_CLANG
#pragma clang diagnostic pop
#elif LL_GNUC
#pragma GCC diagnostic pop
#endif

extern bool gSnapshot;
bool gShiftFrame = false;

namespace
{
    ALWeatherModel::Controller sWeatherController;

    struct WeatherShelterExposure
    {
        void reset()
        {
            mExposure = 1.f;
            mLastPresentationTime = 0.0;
            mInitialized = false;
        }

        F32 update(F64 presentation_time, F32 target)
        {
            target = llclamp(target, 0.f, 1.f);
            if (!std::isfinite(presentation_time))
            {
                reset();
                return target;
            }
            if (!mInitialized || presentation_time < mLastPresentationTime)
            {
                mExposure = target;
                mLastPresentationTime = presentation_time;
                mInitialized = true;
                return mExposure;
            }

            // Exact exponential integration for a constant target makes the
            // ease independent of frame subdivision and presentation-time only.
            constexpr F64 RESPONSE_SECONDS = 0.2;
            const F64 elapsed = llclamp(
                presentation_time - mLastPresentationTime, 0.0, 1.0);
            const F32 blend = static_cast<F32>(
                1.0 - std::exp(-elapsed / RESPONSE_SECONDS));
            mExposure = llclamp(
                mExposure + (target - mExposure) * blend, 0.f, 1.f);
            if (std::abs(target - mExposure) <= 0.001f)
            {
                mExposure = target;
            }
            mLastPresentationTime = presentation_time;
            return mExposure;
        }

        F32 mExposure = 1.f;
        F64 mLastPresentationTime = 0.0;
        bool mInitialized = false;
    };

    WeatherShelterExposure sWeatherShelterExposure;
}

//cached settings
bool LLPipeline::WindLightUseAtmosShaders;
bool LLPipeline::RenderDeferred;
F32 LLPipeline::RenderDeferredSunWash;
U32 LLPipeline::RenderFSAAType;
U32 LLPipeline::RenderResolutionDivisor;
// [SL:KB] - Patch: Settings-RenderResolutionMultiplier | Checked: Catznip-5.4
F32 LLPipeline::RenderResolutionMultiplier;
// [/SL:KB]
bool LLPipeline::RenderUIBuffer;
S32 LLPipeline::RenderShadowDetail;
S32 LLPipeline::RenderShadowSplits;
bool LLPipeline::RenderDeferredSSAO;
F32 LLPipeline::RenderShadowResolutionScale;
bool LLPipeline::RenderDelayCreation;
bool LLPipeline::RenderAnimateRes;
bool LLPipeline::FreezeTime;
S32 LLPipeline::DebugBeaconLineWidth;
F32 LLPipeline::RenderHighlightBrightness;
LLColor4 LLPipeline::RenderHighlightColor;
F32 LLPipeline::RenderHighlightThickness;
bool LLPipeline::RenderSpotLightsInNondeferred;
LLColor4 LLPipeline::PreviewAmbientColor;
LLColor4 LLPipeline::PreviewDiffuse0;
LLColor4 LLPipeline::PreviewSpecular0;
LLColor4 LLPipeline::PreviewDiffuse1;
LLColor4 LLPipeline::PreviewSpecular1;
LLColor4 LLPipeline::PreviewDiffuse2;
LLColor4 LLPipeline::PreviewSpecular2;
LLVector3 LLPipeline::PreviewDirection0;
LLVector3 LLPipeline::PreviewDirection1;
LLVector3 LLPipeline::PreviewDirection2;
F32 LLPipeline::RenderGlowMaxExtractAlpha;
F32 LLPipeline::RenderGlowWarmthAmount;
LLVector3 LLPipeline::RenderGlowLumWeights;
LLVector3 LLPipeline::RenderGlowWarmthWeights;
S32 LLPipeline::RenderGlowResolutionPow;
S32 LLPipeline::RenderGlowIterations;
F32 LLPipeline::RenderGlowWidth;
F32 LLPipeline::RenderGlowStrength;
bool LLPipeline::RenderGlowNoise;
bool LLPipeline::RenderDepthOfField;
F32 LLPipeline::CameraFocusTransitionTime;
F32 LLPipeline::CameraFNumber;
F32 LLPipeline::CameraFocalLength;
F32 LLPipeline::CameraFieldOfView;
F32 LLPipeline::RenderShadowNoise;
F32 LLPipeline::RenderShadowBlurSize;
F32 LLPipeline::RenderSSAOScale;
U32 LLPipeline::RenderSSAOMaxScale;
F32 LLPipeline::RenderSSAOFactor;
LLVector3 LLPipeline::RenderSSAOEffect;
F32 LLPipeline::RenderShadowOffsetError;
F32 LLPipeline::RenderShadowBiasError;
F32 LLPipeline::RenderShadowOffset;
F32 LLPipeline::RenderShadowBias;
F32 LLPipeline::RenderSpotShadowOffset;
F32 LLPipeline::RenderSpotShadowBias;
LLDrawable* LLPipeline::RenderSpotLight = nullptr;
F32 LLPipeline::RenderEdgeDepthCutoff;
F32 LLPipeline::RenderEdgeNormCutoff;
LLVector3 LLPipeline::RenderShadowGaussian;
F32 LLPipeline::RenderShadowBlurDistFactor;
bool LLPipeline::RenderDeferredAtmospheric;
F32 LLPipeline::RenderHighlightFadeTime;
F32 LLPipeline::RenderFarClip;
LLVector3 LLPipeline::RenderShadowSplitExponent;
F32 LLPipeline::RenderShadowErrorCutoff;
F32 LLPipeline::RenderShadowFOVCutoff;
bool LLPipeline::CameraOffset;
F32 LLPipeline::CameraMaxCoF;
F32 LLPipeline::CameraDoFResScale;
F32 LLPipeline::RenderAutoHideSurfaceAreaLimit;
bool LLPipeline::RenderScreenSpaceReflections;
// [BDMerge G3.2] volumetric lighting (donor: Black Dragon)
bool LLPipeline::RenderVolumetricLighting;
U32 LLPipeline::RenderVolumetricLightingResolution;
F32 LLPipeline::RenderVolumetricLightingMultiplier;
F32 LLPipeline::RenderVolumetricLightingFalloffMultiplier;
// [BDMerge G3.3] per-projector volumetric light cones (visible spotlight shafts)
bool LLPipeline::BDMergeProjectorVolumetrics;
U32 LLPipeline::BDMergeProjectorVolumetricsResolution;
F32 LLPipeline::BDMergeProjectorVolumetricsMultiplier;
F32 LLPipeline::BDMergeProjectorVolumetricsAnisotropy;
U32 LLPipeline::BDMergeProjectorVolumetricsDither;
F32 LLPipeline::BDMergeProjectorVolumetricsFeather;
U32 LLPipeline::BDMergeProjectorVolumetricsShadowSamples;
bool LLPipeline::BDMergeProjectorVolumetricsScissor;
bool LLPipeline::BDMergeProjectorVolumetricsAdaptive;
bool LLPipeline::BDMergeProjectorVolumetricsHalfRes;
U32 LLPipeline::BDMergeProjectorVolumetricsMinResolution;
F32 LLPipeline::BDMergeProjectorVolumetricsMaxLuminance;
bool LLPipeline::BDMergeProjectorVolumetricsFrustumClip;
bool LLPipeline::BDMergeProjectorVolumetricsShadowJitterTap;
LLColor3 LLPipeline::BDMergeProjectorVolumetricsTint;
F32 LLPipeline::BDMergeProjectorVolumetricsTintStrength;
F32 LLPipeline::BDMergeProjectorVolumetricsDensity;
F32 LLPipeline::BDMergeProjectorVolumetricsNoiseStrength;
F32 LLPipeline::BDMergeProjectorVolumetricsNoiseScale;
F32 LLPipeline::BDMergeProjectorVolumetricsNoiseSpeed;
F32 LLPipeline::BDMergeProjectorVolumetricsFogStrength;
F32 LLPipeline::BDMergeProjectorVolumetricsFogGroundDensity;
F32 LLPipeline::BDMergeProjectorVolumetricsFogFalloff;
F32 LLPipeline::BDMergeProjectorVolumetricsFogBase;
F32 LLPipeline::BDMergeProjectorVolumetricsBloomFeed;
F32 LLPipeline::BDMergeProjectorVolumetricsBloomFeedAnamorphic;
bool LLPipeline::BDMergeProjectorVolumetricsTemporal;
F32 LLPipeline::BDMergeProjectorVolumetricsTemporalBlend;
F32 LLPipeline::BDMergeProjectorVolumetricsTemporalReject;
bool LLPipeline::BDMergeProjectorVolumetricsTemporalBeamDepth;
F32 LLPipeline::BDMergeProjectorVolumetricsShadowTint;
F32 LLPipeline::BDMergeProjectorVolumetricsRimStrength;
F32 LLPipeline::BDMergeProjectorVolumetricsRimPower;
F32 LLPipeline::BDMergeProjectorVolumetricsRimThreshold;
F32 LLPipeline::BDMergeProjectorVolumetricsRimWrap;
F32 LLPipeline::BDMergeProjectorVolumetricsRimSoftness;
// [BDMerge G3.3 ConservativeShadow] airborne-march shadow sampler A/B gate (default on).
bool LLPipeline::BDMergeProjectorVolumetricsConservativeShadow;
// [BDMerge G3.3 Dust] baked 64^3 dust-volume particulate breakup (default off).
bool LLPipeline::BDMergeProjectorVolumetricsDust;
F32 LLPipeline::BDMergeProjectorVolumetricsDustIntensity;
F32 LLPipeline::BDMergeProjectorVolumetricsDustScale;
F32 LLPipeline::BDMergeProjectorVolumetricsDustDrift;
// [BDMerge Froxel F0] hybrid froxel volumetrics grid (master gate default OFF).
bool LLPipeline::BDMergeFroxelVolumetrics;
U32  LLPipeline::BDMergeFroxelGridX;
U32  LLPipeline::BDMergeFroxelGridY;
U32  LLPipeline::BDMergeFroxelGridZ;
F32  LLPipeline::BDMergeFroxelFar;
F32  LLPipeline::BDMergeFroxelDensity;
F32  LLPipeline::BDMergeFroxelFogStrength;
F32  LLPipeline::BDMergeFroxelFogGroundDensity;
F32  LLPipeline::BDMergeFroxelFogFalloff;
F32  LLPipeline::BDMergeFroxelFogBase;
F32  LLPipeline::BDMergeFroxelNoiseStrength;
F32  LLPipeline::BDMergeFroxelNoiseScale;
F32  LLPipeline::BDMergeFroxelNoiseSpeed;
F32  LLPipeline::BDMergeFroxelAmbient;   // [BDMerge Froxel F1]
bool LLPipeline::BDMergeFroxelLights;    // [BDMerge Froxel F2]
U32  LLPipeline::BDMergeFroxelMaxLights; // [BDMerge Froxel F2]
bool LLPipeline::BDMergeFroxelTemporal;      // [BDMerge Froxel F3]
F32  LLPipeline::BDMergeFroxelTemporalBlend; // [BDMerge Froxel F3]
U32  LLPipeline::BDMergeFroxelDebug;
// [BDMerge Batch 2]
bool LLPipeline::BDMergeSoftProjectorShadows;
F32  LLPipeline::BDMergeSoftShadowSoftness;
F32  LLPipeline::BDMergeSoftShadowMaxPenumbra;
F32  LLPipeline::BDMergeSoftShadowFill;
bool LLPipeline::BDMergeSoftShadowSun;
// [Vogel A/B] soft-shadow kernel selector
bool LLPipeline::BDMergeSoftShadowVogel;
S32  LLPipeline::BDMergeSoftShadowTaps;
bool LLPipeline::BDMergeGoboAnisotropic;
// [BDMerge A5.4-1a] velocity / motion-vector buffer
bool LLPipeline::BDMergeVelocityBuffer;
bool LLPipeline::BDMergeVelocityDebug;
S32  LLPipeline::BDMergeMotionBlurStrength;
bool LLPipeline::BDMergeMotionBlur; // [BDMerge A5.4-3]
bool LLPipeline::sVelocityRender = false;
std::map<LLUUID, LLPipeline::VolumetricShaftOverride> LLPipeline::sVolumetricShaftOverrides;
std::set<LLUUID> LLPipeline::sVolumetricShaftObjects;
std::set<LLUUID> LLPipeline::sNoShadowProjectors; // [BDMerge Batch 3] cast-shadows opt-out
std::set<LLUUID> LLPipeline::sHeroProjectors;     // [BDMerge F4] Hero Beam per-cone opt-in
std::map<LLUUID, S32> LLPipeline::sAlphaModeOverride; // [BDMerge G2.3 per-target] per-object/avatar alpha mode
S32 LLPipeline::RenderScreenSpaceReflectionIterations;
F32 LLPipeline::RenderScreenSpaceReflectionRayStep;
F32 LLPipeline::RenderScreenSpaceReflectionDistanceBias;
F32 LLPipeline::RenderScreenSpaceReflectionDepthRejectBias;
F32 LLPipeline::RenderScreenSpaceReflectionAdaptiveStepMultiplier;
S32 LLPipeline::RenderScreenSpaceReflectionGlossySamples;
S32 LLPipeline::RenderBufferVisualization;
bool LLPipeline::RenderMirrors;
S32 LLPipeline::RenderHeroProbeUpdateRate;
S32 LLPipeline::RenderHeroProbeConservativeUpdateMultiplier;
bool LLPipeline::RenderAvatarCloth;
LLTrace::EventStatHandle<S64> LLPipeline::sStatBatchSize("renderbatchsize");

const U32 LLPipeline::MAX_PREVIEW_WIDTH = 512;

const F32 BACKLIGHT_DAY_MAGNITUDE_OBJECT = 0.1f;
const F32 BACKLIGHT_NIGHT_MAGNITUDE_OBJECT = 0.08f;
const F32 ALPHA_BLEND_CUTOFF = 0.598f;
const F32 DEFERRED_LIGHT_FALLOFF = 0.5f;
const U32 DEFERRED_VB_MASK = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_TEXCOORD1;

extern S32 gBoxFrame;
extern bool gDisplaySwapBuffers;
extern bool gDebugGL;
extern bool gCubeSnapshot;
extern bool gSnapshotNoPost;

bool    gAvatarBacklight = false;

bool    gDebugPipeline = false;
LLPipeline gPipeline;
const LLMatrix4* gGLLastMatrix = NULL;

static LLStaticHashedString sTint("tint");
static LLStaticHashedString sLastProjectionMatrixUnjittered("last_projection_matrix_unjittered");
static F32 sLastVelocityProjMat[16] =
    { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
static bool sHasLastVelocityProjection = false;
static LLStaticHashedString sAmbiance("ambiance");
static LLStaticHashedString sAlphaScale("alpha_scale");
static LLStaticHashedString sNormMat("norm_mat");
static LLStaticHashedString sOffset("offset");
static LLStaticHashedString sScreenRes("screenRes");
static LLStaticHashedString sDelta("delta");
static LLStaticHashedString sDistFactor("dist_factor");
static LLStaticHashedString sKern("kern");
static LLStaticHashedString sKernScale("kern_scale");
static LLStaticHashedString sSmaaRTMetrics("SMAA_RT_METRICS");

//----------------------------------------

void drawBox(const LLVector4a& c, const LLVector4a& r);
void drawBoxOutline(const LLVector3& pos, const LLVector3& size);
U32 nhpo2(U32 v);
LLVertexBuffer* ll_create_cube_vb(U32 type_mask);

void display_update_camera();
//----------------------------------------

S32     LLPipeline::sCompiles = 0;

bool    LLPipeline::sPickAvatar = true;
bool    LLPipeline::sDynamicLOD = true;
bool    LLPipeline::sShowHUDAttachments = true;
bool    LLPipeline::sRenderMOAPBeacons = false;
bool    LLPipeline::sRenderPhysicalBeacons = true;
bool    LLPipeline::sRenderScriptedBeacons = false;
bool    LLPipeline::sRenderScriptedTouchBeacons = true;
bool    LLPipeline::sRenderParticleBeacons = false;
bool    LLPipeline::sRenderSoundBeacons = false;
bool    LLPipeline::sRenderBeacons = false;
bool    LLPipeline::sRenderHighlight = true;
LLRender::eTexIndex LLPipeline::sRenderHighlightTextureChannel = LLRender::DIFFUSE_MAP;
// [F4] see pipeline.h; donor: Firestorm pipeline.cpp LLPipeline::sLastFocusPoint / sDoFEnabled
LLVector3 LLPipeline::sLastFocusPoint = LLVector3::zero;
bool    LLPipeline::sDoFEnabled = false;
bool    LLPipeline::sForceOldBakedUpload = false;
S32     LLPipeline::sUseOcclusion = 0;
bool    LLPipeline::sAutoMaskAlphaDeferred = true;
bool    LLPipeline::sAutoMaskAlphaNonDeferred = false;
bool    LLPipeline::sRenderTransparentWater = true;
bool    LLPipeline::sBakeSunlight = false;
bool    LLPipeline::sNoAlpha = false;
bool    LLPipeline::sUseFarClip = true;
bool    LLPipeline::sShadowRender = false;
bool    LLPipeline::sRainOcclusionRender = false;
bool    LLPipeline::sRenderGlow = false;
bool    LLPipeline::sReflectionRender = false;
bool    LLPipeline::sDistortionRender = false;
bool    LLPipeline::sImpostorRender = false;
bool    LLPipeline::sImpostorRenderAlphaDepthPass = false;
bool    LLPipeline::sPrismLensRender = false;
bool    LLPipeline::sUnderWaterRender = false;
bool    LLPipeline::sTextureBindTest = false;
bool    LLPipeline::sRenderAttachedLights = true;
bool    LLPipeline::sRenderAttachedParticles = true;
bool    LLPipeline::sRenderDeferred = false;
bool    LLPipeline::sReflectionProbesEnabled = false;
S32     LLPipeline::sVisibleLightCount = 0;
bool    LLPipeline::sRenderingHUDs;
F32     LLPipeline::sDistortionWaterClipPlaneMargin = 1.0125f;
// [SL:KB] - Patch: Render-TextureToggle (Catznip-4.0)
bool    LLPipeline::sRenderTextures = true;
// [/SL:KB]
// [RLVa:KB] - @setsphere
bool    LLPipeline::sUseDepthTexture = false;
// [/RLVa:KB]

// EventHost API LLPipeline listener.
static LLPipelineListener sPipelineListener;

static LLCullResult* sCull = NULL;

void validate_framebuffer_object();

// Add color attachments for deferred rendering
// target -- RenderTarget to add attachments to
bool addDeferredAttachments(LLRenderTarget& target, bool for_impostor = false)
{
    U32 orm = GL_RGBA;
    U32 norm = GL_RGBA16;
    U32 emissive = GL_RGB16F;

    static LLCachedControl<bool> has_emissive(gSavedSettings, "RenderEnableEmissiveBuffer", false);
    static LLCachedControl<bool> has_hdr(gSavedSettings, "RenderHDREnabled", true);
    bool hdr = has_hdr() && gGLManager.mGLVersion > 4.05f;

    if (!hdr)
    {
        norm = GL_RGB10_A2;
        emissive = GL_RGB;
    }

    bool valid = true;
    valid      = valid && target.addColorAttachment(orm);    // frag-data[1] specular OR PBR ORM
    valid      = valid && target.addColorAttachment(norm);
    if (has_emissive)
    {
        valid = valid && target.addColorAttachment(emissive); // frag_data[3] PBR emissive OR material env intensity
    }

    return valid;
}

LLPipeline::LLPipeline() :
    mBackfaceCull(false),
    mMatrixOpCount(0),
    mTextureMatrixOps(0),
    mNumVisibleNodes(0),
    mNumVisibleFaces(0),
    mPoissonOffset(0),

    mInitialized(false),
    mShadersLoaded(false),
    mRenderDebugFeatureMask(0),
    mRenderDebugMask(0),
    mOldRenderDebugMask(0),
    mGroupQ1Locked(false),
    mResetVertexBuffers(false),
    mLastRebuildPool(NULL),
    mLightMask(0),
    mLightMovingMask(0)
{
    mNoiseMap = 0;
    mTrueNoiseMap = 0;
    mLightFunc = 0;

    for(U32 i = 0; i < 8; i++)
    {
        mHWLightColors[i] = LLColor4::black;
    }

    // [BDMerge G3.3 Batch 1 A] identity so the first temporal-resolve upload is
    // well-defined (it is ignored that frame anyway - history is invalid).
    for (U32 i = 0; i < 16; ++i)
        mProjVolPrevViewProj[i] = (i % 5 == 0) ? 1.f : 0.f;

    // [BDMerge Froxel F3] same: identity prev-modelview so the first froxel-temporal
    // upload is well-defined (ignored that frame - history is invalid, blend forced 0).
    for (U32 i = 0; i < 16; ++i)
        mFroxelPrevModelview[i] = (i % 5 == 0) ? 1.f : 0.f;
}

void LLPipeline::connectRefreshCachedSettingsSafe(const std::string name)
{
    LLPointer<LLControlVariable> cntrl_ptr = gSavedSettings.getControl(name);
    if ( cntrl_ptr.isNull() )
    {
        LL_WARNS() << "Global setting name not found:" << name << LL_ENDL;
    }
    else
    {
        cntrl_ptr->getCommitSignal()->connect(boost::bind(&LLPipeline::refreshCachedSettings));
    }
}

// [BDMerge NSpot] runtime projector-shadow slot count (2 = stock, up to
// LLPipeline::MAX_SPOT_SHADOWS). Changing reallocates via handleShadowsResized.
static U32 bdmergeMaxSpotShadows()
{
    static LLCachedControl<U32> max_spots(gSavedSettings, "BDMergeMaxSpotShadows", 2);
    return llclamp((U32)max_spots, 2u, LLPipeline::MAX_SPOT_SHADOWS);
}

void LLPipeline::init()
{
    refreshCachedSettings();

    mRT = &mMainRT;

    gOctreeMaxCapacity = gSavedSettings.getU32("OctreeMaxNodeCapacity");
    gOctreeMinSize = gSavedSettings.getF32("OctreeMinimumNodeSize");
    sDynamicLOD = gSavedSettings.getBOOL("RenderDynamicLOD");
    sRenderAttachedLights = gSavedSettings.getBOOL("RenderAttachedLights");
    sRenderAttachedParticles = gSavedSettings.getBOOL("RenderAttachedParticles");

    mReflectionMapManager.refreshSettings();

    mInitialized = true;

    stop_glerror();

    //create render pass pools
    getPool(LLDrawPool::POOL_WATEREXCLUSION);
    getPool(LLDrawPool::POOL_ALPHA_PRE_WATER);
    getPool(LLDrawPool::POOL_ALPHA_POST_WATER);
    getPool(LLDrawPool::POOL_SIMPLE);
    getPool(LLDrawPool::POOL_ALPHA_MASK);
    getPool(LLDrawPool::POOL_FULLBRIGHT_ALPHA_MASK);
    getPool(LLDrawPool::POOL_GRASS);
    getPool(LLDrawPool::POOL_FULLBRIGHT);
    getPool(LLDrawPool::POOL_BUMP);
    getPool(LLDrawPool::POOL_MATERIALS);
    getPool(LLDrawPool::POOL_GLOW);
    getPool(LLDrawPool::POOL_GLTF_PBR);
    getPool(LLDrawPool::POOL_GLTF_PBR_ALPHA_MASK);

    resetFrameStats();

    if (gSavedSettings.getBOOL("DisableAllRenderFeatures"))
    {
        clearAllRenderDebugFeatures();
    }
    else
    {
        setAllRenderDebugFeatures(); // By default, all debugging features on
    }
    clearAllRenderDebugDisplays(); // All debug displays off

    if (gSavedSettings.getBOOL("DisableAllRenderTypes"))
    {
        clearAllRenderTypes();
    }
    else if (gNonInteractive)
    {
        clearAllRenderTypes();
    }
    else
    {
        setAllRenderTypes(); // By default, all rendering types start enabled
    }

    // make sure RenderPerformanceTest persists (hackity hack hack)
    // disables non-object rendering (UI, sky, water, etc)
    if (gSavedSettings.getBOOL("RenderPerformanceTest"))
    {
        gSavedSettings.setBOOL("RenderPerformanceTest", false);
        gSavedSettings.setBOOL("RenderPerformanceTest", true);
    }

    mOldRenderDebugMask = mRenderDebugMask;

    mBackfaceCull = true;

    // Enable features
    LLViewerShaderMgr::instance()->setShaders();

    for (U32 i = 0; i < MAX_SPOT_SHADOWS; ++i)
    {
        mSpotLightFade[i] = 1.f;
    }

    if (mCubeVB.isNull())
    {
        mCubeVB = ll_create_cube_vb(LLVertexBuffer::MAP_VERTEX);
    }

    mDeferredVB = new LLVertexBuffer(DEFERRED_VB_MASK);
    mDeferredVB->allocateBuffer(8, 0);

    {
        mScreenTriangleVB = new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX);
        mScreenTriangleVB->allocateBuffer(3, 0);
        LLStrider<LLVector3> vert;
        mScreenTriangleVB->getVertexStrider(vert);

        vert[0].set(-1, 1, 0);
        vert[1].set(-1, -3, 0);
        vert[2].set(3, 1, 0);

        mScreenTriangleVB->unmapBuffer();
    }

    //
    // Update all settings to trigger a cached settings refresh
    //
    connectRefreshCachedSettingsSafe("RenderAutoMaskAlphaDeferred");
    connectRefreshCachedSettingsSafe("RenderAutoMaskAlphaNonDeferred");
    connectRefreshCachedSettingsSafe("RenderUseFarClip");
    connectRefreshCachedSettingsSafe("RenderAvatarMaxNonImpostors");
    connectRefreshCachedSettingsSafe("UseOcclusion");
    // DEPRECATED -- connectRefreshCachedSettingsSafe("WindLightUseAtmosShaders");
    // DEPRECATED -- connectRefreshCachedSettingsSafe("RenderDeferred");
    connectRefreshCachedSettingsSafe("RenderDeferredSunWash");
    connectRefreshCachedSettingsSafe("RenderFSAAType");
    connectRefreshCachedSettingsSafe("RenderResolutionDivisor");
// [SL:KB] - Patch: Settings-RenderResolutionMultiplier | Checked: Catznip-5.4
    connectRefreshCachedSettingsSafe("RenderResolutionMultiplier");
// [/SL:KB]
    connectRefreshCachedSettingsSafe("RenderUIBuffer");
    connectRefreshCachedSettingsSafe("RenderShadowDetail");
    connectRefreshCachedSettingsSafe("RenderShadowSplits");
    connectRefreshCachedSettingsSafe("RenderDeferredSSAO");
    connectRefreshCachedSettingsSafe("RenderShadowResolutionScale");
    connectRefreshCachedSettingsSafe("RenderDelayCreation");
    connectRefreshCachedSettingsSafe("RenderAnimateRes");
    connectRefreshCachedSettingsSafe("FreezeTime");
    connectRefreshCachedSettingsSafe("DebugBeaconLineWidth");
    connectRefreshCachedSettingsSafe("RenderHighlightBrightness");
    connectRefreshCachedSettingsSafe("RenderHighlightColor");
    connectRefreshCachedSettingsSafe("RenderHighlightThickness");
    connectRefreshCachedSettingsSafe("RenderSpotLightsInNondeferred");
    connectRefreshCachedSettingsSafe("PreviewAmbientColor");
    connectRefreshCachedSettingsSafe("PreviewDiffuse0");
    connectRefreshCachedSettingsSafe("PreviewSpecular0");
    connectRefreshCachedSettingsSafe("PreviewDiffuse1");
    connectRefreshCachedSettingsSafe("PreviewSpecular1");
    connectRefreshCachedSettingsSafe("PreviewDiffuse2");
    connectRefreshCachedSettingsSafe("PreviewSpecular2");
    connectRefreshCachedSettingsSafe("PreviewDirection0");
    connectRefreshCachedSettingsSafe("PreviewDirection1");
    connectRefreshCachedSettingsSafe("PreviewDirection2");
    connectRefreshCachedSettingsSafe("RenderGlowMaxExtractAlpha");
    connectRefreshCachedSettingsSafe("RenderGlowWarmthAmount");
    connectRefreshCachedSettingsSafe("RenderGlowLumWeights");
    connectRefreshCachedSettingsSafe("RenderGlowWarmthWeights");
    connectRefreshCachedSettingsSafe("RenderGlowResolutionPow");
    connectRefreshCachedSettingsSafe("RenderGlowIterations");
    connectRefreshCachedSettingsSafe("RenderGlowWidth");
    connectRefreshCachedSettingsSafe("RenderGlowStrength");
    connectRefreshCachedSettingsSafe("RenderGlowNoise");
    connectRefreshCachedSettingsSafe("RenderDepthOfField");
    connectRefreshCachedSettingsSafe("CameraFocusTransitionTime");
    connectRefreshCachedSettingsSafe("CameraFNumber");
    connectRefreshCachedSettingsSafe("CameraFocalLength");
    connectRefreshCachedSettingsSafe("CameraFieldOfView");
    connectRefreshCachedSettingsSafe("RenderShadowNoise");
    connectRefreshCachedSettingsSafe("RenderShadowBlurSize");
    connectRefreshCachedSettingsSafe("RenderSSAOScale");
    connectRefreshCachedSettingsSafe("RenderSSAOMaxScale");
    connectRefreshCachedSettingsSafe("RenderSSAOFactor");
    connectRefreshCachedSettingsSafe("RenderSSAOEffect");
    connectRefreshCachedSettingsSafe("RenderShadowOffsetError");
    connectRefreshCachedSettingsSafe("RenderShadowBiasError");
    connectRefreshCachedSettingsSafe("RenderShadowOffset");
    connectRefreshCachedSettingsSafe("RenderShadowBias");
    connectRefreshCachedSettingsSafe("RenderSpotShadowOffset");
    connectRefreshCachedSettingsSafe("RenderSpotShadowBias");
    connectRefreshCachedSettingsSafe("RenderEdgeDepthCutoff");
    connectRefreshCachedSettingsSafe("RenderEdgeNormCutoff");
    connectRefreshCachedSettingsSafe("RenderShadowGaussian");
    connectRefreshCachedSettingsSafe("RenderShadowBlurDistFactor");
    connectRefreshCachedSettingsSafe("RenderDeferredAtmospheric");
    connectRefreshCachedSettingsSafe("RenderHighlightFadeTime");
    connectRefreshCachedSettingsSafe("RenderFarClip");
    connectRefreshCachedSettingsSafe("RenderShadowSplitExponent");
    connectRefreshCachedSettingsSafe("RenderShadowErrorCutoff");
    connectRefreshCachedSettingsSafe("RenderShadowFOVCutoff");
    connectRefreshCachedSettingsSafe("CameraOffset");
    connectRefreshCachedSettingsSafe("CameraMaxCoF");
    connectRefreshCachedSettingsSafe("CameraDoFResScale");
    connectRefreshCachedSettingsSafe("RenderAutoHideSurfaceAreaLimit");
    connectRefreshCachedSettingsSafe("RenderScreenSpaceReflections");
    // [BDMerge G3.2]
    connectRefreshCachedSettingsSafe("RenderVolumetricLighting");
    connectRefreshCachedSettingsSafe("RenderVolumetricLightingResolution");
    connectRefreshCachedSettingsSafe("RenderVolumetricLightingMultiplier");
    connectRefreshCachedSettingsSafe("RenderVolumetricLightingFalloffMultiplier");
    // [BDMerge G3.3]
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetrics");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsResolution");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsMultiplier");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsAnisotropy");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsDither");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsFeather");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsShadowSamples");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsScissor");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsAdaptive");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsHalfRes");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsMinResolution");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsMaxLuminance");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsFrustumClip");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsShadowJitterTap");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsTint");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsTintStrength");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsDensity");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsNoiseStrength");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsNoiseScale");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsNoiseSpeed");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsFogStrength");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsFogGroundDensity");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsFogFalloff");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsFogBase");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsBloomFeed");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsBloomFeedAnamorphic");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsTemporal");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsTemporalBlend");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsTemporalReject");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsTemporalBeamDepth");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsShadowTint");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsRimStrength");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsRimPower");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsRimThreshold");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsRimWrap");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsRimSoftness");
    // [BDMerge G3.3 ConservativeShadow + Dust]
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsConservativeShadow");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsDust");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsDustIntensity");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsDustScale");
    connectRefreshCachedSettingsSafe("BDMergeProjectorVolumetricsDustDrift");
    // [BDMerge Froxel F0] hybrid froxel volumetrics grid
    connectRefreshCachedSettingsSafe("BDMergeFroxelVolumetrics");
    connectRefreshCachedSettingsSafe("BDMergeFroxelGridX");
    connectRefreshCachedSettingsSafe("BDMergeFroxelGridY");
    connectRefreshCachedSettingsSafe("BDMergeFroxelGridZ");
    connectRefreshCachedSettingsSafe("BDMergeFroxelFar");
    connectRefreshCachedSettingsSafe("BDMergeFroxelDensity");
    connectRefreshCachedSettingsSafe("BDMergeFroxelFogStrength");
    connectRefreshCachedSettingsSafe("BDMergeFroxelFogGroundDensity");
    connectRefreshCachedSettingsSafe("BDMergeFroxelFogFalloff");
    connectRefreshCachedSettingsSafe("BDMergeFroxelFogBase");
    connectRefreshCachedSettingsSafe("BDMergeFroxelNoiseStrength");
    connectRefreshCachedSettingsSafe("BDMergeFroxelNoiseScale");
    connectRefreshCachedSettingsSafe("BDMergeFroxelNoiseSpeed");
    connectRefreshCachedSettingsSafe("BDMergeFroxelAmbient"); // [BDMerge Froxel F1]
    connectRefreshCachedSettingsSafe("BDMergeFroxelLights");    // [BDMerge Froxel F2]
    connectRefreshCachedSettingsSafe("BDMergeFroxelMaxLights"); // [BDMerge Froxel F2]
    connectRefreshCachedSettingsSafe("BDMergeFroxelTemporal");      // [BDMerge Froxel F3]
    connectRefreshCachedSettingsSafe("BDMergeFroxelTemporalBlend"); // [BDMerge Froxel F3]
    connectRefreshCachedSettingsSafe("BDMergeFroxelDebug");
    connectRefreshCachedSettingsSafe("BDMergeSoftProjectorShadows");
    connectRefreshCachedSettingsSafe("BDMergeSoftShadowSoftness");
    connectRefreshCachedSettingsSafe("BDMergeSoftShadowMaxPenumbra");
    connectRefreshCachedSettingsSafe("BDMergeSoftShadowFill");
    connectRefreshCachedSettingsSafe("BDMergeSoftShadowSun");
    connectRefreshCachedSettingsSafe("RenderShadowSoftVogel");     // [Vogel A/B]
    connectRefreshCachedSettingsSafe("RenderShadowSoftTaps");      // [Vogel A/B]
    connectRefreshCachedSettingsSafe("BDMergeGoboAnisotropic");
    connectRefreshCachedSettingsSafe("BDMergeVelocityBuffer");     // [BDMerge A5.4-1a]
    connectRefreshCachedSettingsSafe("BDMergeMotionBlur");         // [BDMerge A5.4-3]
    connectRefreshCachedSettingsSafe("BDMergeVelocityDebug");      // [BDMerge A5.4-1a]
    connectRefreshCachedSettingsSafe("BDMergeMotionBlurStrength"); // [BDMerge A5.4-1a]
    connectRefreshCachedSettingsSafe("RenderScreenSpaceReflectionIterations");
    connectRefreshCachedSettingsSafe("RenderScreenSpaceReflectionRayStep");
    connectRefreshCachedSettingsSafe("RenderScreenSpaceReflectionDistanceBias");
    connectRefreshCachedSettingsSafe("RenderScreenSpaceReflectionDepthRejectBias");
    connectRefreshCachedSettingsSafe("RenderScreenSpaceReflectionAdaptiveStepMultiplier");
    connectRefreshCachedSettingsSafe("RenderScreenSpaceReflectionGlossySamples");
    connectRefreshCachedSettingsSafe("RenderBufferVisualization");
    connectRefreshCachedSettingsSafe("RenderMirrors");
    connectRefreshCachedSettingsSafe("RenderHeroProbeUpdateRate");
    connectRefreshCachedSettingsSafe("RenderHeroProbeConservativeUpdateMultiplier");
    connectRefreshCachedSettingsSafe("RenderAvatarCloth");

    LLPointer<LLControlVariable> cntrl_ptr = gSavedSettings.getControl("CollectFontVertexBuffers");
    if (cntrl_ptr.notNull())
    {
        cntrl_ptr->getCommitSignal()->connect([](LLControlVariable* control, const LLSD& value, const LLSD& previous)
        {
            bool enable_buffers = control->getValue().asBoolean();
            LLFontVertexBuffer::enableBufferCollection(enable_buffers);
            LLFontWidthBuffer::enableBufferCollection(enable_buffers);
        });
    }

    gSavedSettings.getControl("RenderColorGrade")->getCommitSignal()->connect(boost::bind(&LLPipeline::setupGradingLUT, this));
    gSavedSettings.getControl("RenderColorGradeLUT")->getCommitSignal()->connect(boost::bind(&LLPipeline::setupGradingLUT, this));
}

LLPipeline::~LLPipeline()
{
}

void LLPipeline::cleanup()
{
    assertInitialized();

    mGroupQ1.clear() ;

    for(pool_set_t::iterator iter = mPools.begin();
        iter != mPools.end(); )
    {
        pool_set_t::iterator curiter = iter++;
        LLDrawPool* poolp = *curiter;
        if (poolp->isFacePool())
        {
            LLFacePool* face_pool = (LLFacePool*) poolp;
            if (face_pool->mReferences.empty())
            {
                mPools.erase(curiter);
                removeFromQuickLookup( poolp );
                delete poolp;
            }
        }
        else
        {
            mPools.erase(curiter);
            removeFromQuickLookup( poolp );
            delete poolp;
        }
    }

    if (!mTerrainPools.empty())
    {
        LL_WARNS() << "Terrain Pools not cleaned up" << LL_ENDL;
    }
    if (!mTreePools.empty())
    {
        LL_WARNS() << "Tree Pools not cleaned up" << LL_ENDL;
    }

    delete mAlphaPoolPreWater;
    mAlphaPoolPreWater = nullptr;
    delete mAlphaPoolPostWater;
    mAlphaPoolPostWater = nullptr;
    delete mSkyPool;
    mSkyPool = NULL;
    delete mTerrainPool;
    mTerrainPool = NULL;
    delete mWaterPool;
    mWaterPool = NULL;
    delete mSimplePool;
    mSimplePool = NULL;
    delete mFullbrightPool;
    mFullbrightPool = NULL;
    delete mGlowPool;
    mGlowPool = NULL;
    delete mBumpPool;
    mBumpPool = NULL;
    // don't delete wl sky pool it was handled above in the for loop
    //delete mWLSkyPool;
    mWLSkyPool = NULL;
    delete mWaterExclusionPool;
    mWaterExclusionPool = nullptr;

    releaseGLBuffers();

    mFaceSelectImagep = NULL;

    mMovedList.clear();
    mMovedBridge.clear();
    mShiftList.clear();

    mInitialized = false;

    mDeferredVB = NULL;
    mScreenTriangleVB = nullptr;

    mCubeVB = NULL;

    mReflectionMapManager.cleanup();
    mHeroProbeManager.cleanup();
}

//============================================================================

void LLPipeline::destroyGL()
{
    stop_glerror();
    unloadShaders();
    mHighlightFaces.clear();

    resetDrawOrders();

    releaseGLBuffers();
}

void LLPipeline::requestResizeScreenTexture()
{
    gResizeScreenTexture = true;
}

void LLPipeline::requestResizeShadowTexture()
{
    gResizeShadowTexture = true;
}

void LLPipeline::resizeShadowTexture()
{
    releaseSunShadowTargets();
    releaseSpotShadowTargets();
    allocateShadowBuffer(mRT->width, mRT->height);
    gResizeShadowTexture = false;
}

void LLPipeline::resizeScreenTexture()
{
    if (gPipeline.shadersLoaded())
    {
        GLuint resX = gViewerWindow->getWorldViewWidthRaw();
        GLuint resY = gViewerWindow->getWorldViewHeightRaw();

// [SL:KB] - Patch: Settings-RenderResolutionMultiplier | Checked: Catznip-5.4
        GLuint scaledResX = resX;
        GLuint scaledResY = resY;
        if ( (RenderResolutionDivisor > 1) && (RenderResolutionDivisor < resX) && (RenderResolutionDivisor < resY) )
        {
            scaledResX /= RenderResolutionDivisor;
            scaledResY /= RenderResolutionDivisor;
        }
        else if (RenderResolutionMultiplier > 0.f && RenderResolutionMultiplier != 1.f)
        {
            scaledResX = (GLuint)(scaledResX * RenderResolutionMultiplier);
            scaledResY = (GLuint)(scaledResY * RenderResolutionMultiplier);
        }
// [/SL:KB]

//      if (gResizeScreenTexture || (resX != mRT->screen.getWidth()) || (resY != mRT->screen.getHeight()))
// [SL:KB] - Patch: Settings-RenderResolutionMultiplier | Checked: Catznip-5.4
        if (gResizeScreenTexture || (scaledResX != mRT->screen.getWidth()) || (scaledResY != mRT->screen.getHeight()))
// [/SL:KB]
        {
            releaseScreenBuffers();
            releaseSunShadowTargets();
            releaseSpotShadowTargets();
            mWeatherRainOcclusion.release();
            mWeatherRainOcclusionValid = false;
            mWeatherRainOcclusionFrame = 0;
            mWeatherRainOcclusionFailedResolution = 0;
            mWeatherRainOcclusionDepthRange = 1.f;
            mWeatherRainOcclusionMatrix = glm::mat4(1.f);
            allocateScreenBuffer(resX,resY);
            gResizeScreenTexture = false;
        }
    }
}

bool LLPipeline::allocateScreenBuffer(U32 resX, U32 resY)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    eFBOStatus ret = doAllocateScreenBuffer(resX, resY);

    return ret == FBO_SUCCESS_FULLRES;
}


LLPipeline::eFBOStatus LLPipeline::doAllocateScreenBuffer(U32 resX, U32 resY)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    // try to allocate screen buffers at requested resolution and samples
    // - on failure, shrink number of samples and try again
    // - if not multisampled, shrink resolution and try again (favor X resolution over Y)
    // Make sure to call "releaseScreenBuffers" after each failure to cleanup the partially loaded state

    // refresh cached settings here to protect against inconsistent event handling order
    refreshCachedSettings();

    eFBOStatus ret = FBO_SUCCESS_FULLRES;
    if (!allocateScreenBufferInternal(resX, resY))
    {
        //failed to allocate at requested specification, return false
        ret = FBO_FAILURE;

        releaseScreenBuffers();

        //reduce resolution
        while (resY > 0 && resX > 0)
        {
            resY /= 2;
            if (allocateScreenBufferInternal(resX, resY))
            {
                return FBO_SUCCESS_LOWRES;
            }
            releaseScreenBuffers();

            resX /= 2;
            if (allocateScreenBufferInternal(resX, resY))
            {
                return FBO_SUCCESS_LOWRES;
            }
            releaseScreenBuffers();
        }

        LL_WARNS() << "Unable to allocate screen buffer at any resolution!" << LL_ENDL;
    }

    return ret;
}

bool LLPipeline::allocateScreenBufferInternal(U32 resX, U32 resY)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    bool has_hdr = gSavedSettings.getBOOL("RenderHDREnabled");
    bool hdr = gGLManager.mGLVersion > 4.05f && has_hdr;

    if (mRT == &mMainRT)
    { // hacky -- allocate auxillary buffer

        gCubeSnapshot = true;

        if (sReflectionProbesEnabled)
        {
            mReflectionMapManager.initReflectionMaps();
        }

        mRT = &mAuxillaryRT;
        U32 res = mReflectionMapManager.mProbeResolution * 4;  //multiply by 4 because probes will be 16x super sampled
        allocateScreenBufferInternal(res, res);

        if (RenderMirrors)
        {
            mHeroProbeManager.initReflectionMaps();
            res = mHeroProbeManager.mProbeResolution;  // We also scale the hero probe RT to the probe res since we don't super sample it.
            mRT = &mHeroProbeRT;
            allocateScreenBufferInternal(res, res);
        }

        mRT = &mMainRT;
        gCubeSnapshot = false;
    }

    // remember these dimensions
    mRT->width = resX;
    mRT->height = resY;

    U32 res_mod = RenderResolutionDivisor;

    if (res_mod > 1 && res_mod < resX && res_mod < resY)
    {
        resX /= res_mod;
        resY /= res_mod;
    }
// [SL:KB] - Patch: Settings-RenderResolutionMultiplier | Checked: Catznip-5.4
    else if (RenderResolutionMultiplier > 0.f && RenderResolutionMultiplier != 1.f)
    {
        resX = (GLuint)(resX * RenderResolutionMultiplier);
        resY = (GLuint)(resY * RenderResolutionMultiplier);
    }
// [/SL:KB]

    S32 shadow_detail = RenderShadowDetail;
    bool ssao = RenderDeferredSSAO;

    //allocate deferred rendering color buffers
    if (!mRT->deferredScreen.allocate(resX, resY, GL_SRGB8_ALPHA8, true)) return false;
    if (!addDeferredAttachments(mRT->deferredScreen)) return false;

    GLuint screenFormat = hdr ? GL_RGBA16F : GL_RGBA;

    if (!mRT->screen.allocate(resX, resY, GL_RGBA16F)) return false;

    // Visible-diffuse sidecar: a second colour attachment capturing forward
    // surfaces' diffuse in the same draw calls as the beauty pass.
    //
    // MAIN VIEW ONLY. mRT follows the reflection-probe/hero swap, so without
    // this check the aux and hero packs also get the attachment -- and they
    // then pass the getCurrentBoundTarget() == &mRT->screen identity test in
    // renderGeomPostDeferred, letting probe renders write the published
    // sidecar. (S1)
    //
    // The whole feature rests on indexed draw-buffer state: glColorMaski is
    // GL 3.0, glBlendFuncSeparatei is GL 4.0. Both are resolved as function
    // pointers and are null on older contexts, so calling them would be a
    // null-deref crash rather than a degraded image. Refuse the attachment
    // instead: no attachment => every downstream gate reads false => the
    // feature is simply off. (S5)
    static LLCachedControl<bool> visible_diffuse(gSavedSettings, "RenderVisibleDiffuseSidecar", false);
    if (visible_diffuse && mRT == &mMainRT)
    {
        if (!LLRender::indexedDrawBufferGuardSupported())
        {
            LL_WARNS_ONCE("Pipeline")
                << "RenderVisibleDiffuseSidecar is enabled but this GL context lacks "
                   "glColorMaski (GL 3.0) and/or glBlendFuncSeparatei (GL 4.0); "
                   "the sidecar is disabled." << LL_ENDL;
        }
        // Both attachments or neither. A partially allocated pair must never be
        // published: the coverage mask is what tells the consumer WHICH pixels
        // the sidecar is authoritative for, so a sidecar without coverage would
        // fail closed at best and supersede the whole frame at worst.
        else if (!mRT->screen.addColorAttachment(GL_SRGB8_ALPHA8) ||   // 1: sidecar
                 !mRT->screen.addColorAttachment(GL_RG8))              // 2: coverage
        {
            return false;
        }
    }

    mRT->deferredScreen.shareDepthBuffer(mRT->screen);

    if (shadow_detail > 0 || ssao || RenderDepthOfField)
    { //only need mRT->deferredLight for shadows OR ssao OR dof
        if (!mRT->deferredLight.allocate(resX, resY, screenFormat)) return false;
    }
    else
    {
        mRT->deferredLight.release();
    }

    U32 post_color_fmt = hdr ? GL_RGB10_A2 : GL_RGBA;
    if(mRT != &mHeroProbeRT)
    {
        if (hdr)
        {
            // HDR bloom pyramid. Level 0 starts at scene * (num/den); each subsequent
            // level halves. Lowering the base resolution trades a bit of extract
            // precision for linear savings on the whole pyramid plus wider bloom reach
            // per mip, since the composite bilinearly upsamples mip 0 back to screen.
            // When halation is enabled the alpha channel carries the warmth signal, so
            // we need RGBA16F. With halation off we drop to R11F_G11F_B10F for half the
            // bandwidth on the pyramid hot path.
            const S32 bloom_mip_setting = llclamp(gSavedSettings.getS32("RenderBloomMipCount"), 3, (S32)BLOOM_MAX_MIPS);
            const S32 bloom_scale_idx   = llclamp(gSavedSettings.getS32("RenderBloomResolutionScale"), 0, 4);
            // (numerator, denominator) for each preset: full, 3/4, half, quarter, eighth.
            static const U32 bloom_scale_num[5] = { 1, 3, 1, 1, 1 };
            static const U32 bloom_scale_den[5] = { 1, 4, 2, 4, 8 };
            const U32 base_num = bloom_scale_num[bloom_scale_idx];
            const U32 base_den = bloom_scale_den[bloom_scale_idx];
            const U32 base_w = llmax(1u, (resX * base_num) / base_den);
            const U32 base_h = llmax(1u, (resY * base_num) / base_den);
            const bool bloom_halation = gSavedSettings.getBOOL("RenderBloomHalation");
            const U32 bloom_format = bloom_halation ? GL_RGBA16F : GL_R11F_G11F_B10F;
            mRT->bloomMipCount = 0;
            for (U32 i = 0; i < BLOOM_MAX_MIPS; i++)
            {
                mRT->bloomMip[i].release();
            }
            for (S32 i = 0; i < bloom_mip_setting; i++)
            {
                U32 mw = llmax(1u, base_w >> (U32)i);
                U32 mh = llmax(1u, base_h >> (U32)i);
                if (!mRT->bloomMip[i].allocate(mw, mh, bloom_format))
                {
                    break;
                }
                ++mRT->bloomMipCount;
                if (mw == 1 && mh == 1) break;
            }
        }

        mRT->postPingMap.allocate(resX, resY, post_color_fmt);
        mRT->postPongMap.allocate(resX, resY, post_color_fmt);
    }

    allocateShadowBuffer(resX, resY);

    if (!gCubeSnapshot) // hack to not re-allocate various targets for cube snapshots
    {
        if (RenderUIBuffer)
        {
            if (!mUIScreen.allocate(resX, resY, GL_RGBA))
            {
                return false;
            }
        }

        if (RenderFSAAType > 0)
        {
            // SMAA benefits from a stencil buffer shared across its passes so the
            // blend-weights pass can skip non-edge pixels marked during edge detect.
            bool smaa_stencil = (RenderFSAAType == 2) && gSavedSettings.getBOOL("RenderSMAAUseStencil");
            if (!mFXAAMap.allocate(resX, resY, post_color_fmt, smaa_stencil, smaa_stencil)) return false;
            if (RenderFSAAType == 2)
            {
                if (!mSMAABlendBuffer.allocate(resX, resY, post_color_fmt, false)) return false;
                if (smaa_stencil)
                {
                    mFXAAMap.shareDepthBuffer(mSMAABlendBuffer);
                }
            }
        }
        else
        {
            mFXAAMap.release();
            mSMAABlendBuffer.release();
        }

        // [BDMerge A5.4-1a] Velocity / motion-vector buffer (GL_RG16F). Shares the
        // deferred screen's depth buffer so the velocity geometry pass depth-tests
        // against the already-rendered opaque scene (mirror BD pipeline.cpp:1046).
        // Default OFF -> released -> zero extra cost/behavior. [A5.4-3] Motion
        // blur consumes this buffer, so it also forces allocation + the pass.
        if (gSavedSettings.getBOOL("BDMergeVelocityBuffer") || gSavedSettings.getBOOL("BDMergeMotionBlur"))
        {
            if (!mVelocityMap.allocate(resX, resY, GL_RG16F, false)) return false;
            mRT->deferredScreen.shareDepthBuffer(mVelocityMap);
        }
        else
        {
            mVelocityMap.release();
        }

        //water reflection texture (always needed as scratch space whether or not transparent water is enabled)
        mWaterDis.allocate(resX, resY, screenFormat, true);

        if(RenderScreenSpaceReflections)
        {
            mSceneMap.allocate(resX, resY, screenFormat, true);
        }
        else
        {
            mSceneMap.release();
        }

        // The water exclusion mask needs its own depth buffer so we can take care of the problem of multiple water planes.
        // Should we ever make water not just a plane, it also aids with that as well as the water planes will be rendered into the mask.
        // Why do we do this? Because it saves us some janky logic in the exclusion shader when we generate the mask.
        // Regardless, this should always only be an R8 texture unless we choose to start having multiple kinds of exclusion that 8 bits can't handle.
        // - Geenz 2025-02-06
        bool success = mWaterExclusionMask.allocate(resX, resY, GL_R8, true);

        assert(success);

        // used to scale down textures
        // See LLViwerTextureList::updateImagesCreateTextures and LLImageGL::scaleDown
        mDownResMap.allocate(1024, 1024, GL_RGBA);

        mBakeMap.allocate(LLAvatarAppearanceDefines::SCRATCH_TEX_WIDTH, LLAvatarAppearanceDefines::SCRATCH_TEX_HEIGHT, GL_RGBA);
    }
    //HACK make screenbuffer allocations start failing after 30 seconds
    if (gSavedSettings.getBOOL("SimulateFBOFailure"))
    {
        return false;
    }

    gGL.getTexUnit(0)->disable();

    stop_glerror();

    return true;
}

// Release every GL target owned by one Prism scratch pool entry and zero its
// size key so acquirePrismLensScratch treats it as empty. File-local so no
// caller outside the acquire/release pair can partially tear down an entry.
static void release_prism_scratch_entry(LLPipeline::PrismLensScratch& entry)
{
    entry.rt.screen.release();
    entry.rt.deferredScreen.release();
    entry.rt.deferredLight.release();
    entry.waterDis.release();
    entry.waterExclusionMask.release();
    entry.rt.width = 0;
    entry.rt.height = 0;
    entry.width = 0;
    entry.height = 0;
    entry.lastUsedFrame = 0;
}

bool LLPipeline::acquirePrismLensScratch(U32 width, U32 height)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;

    // Option B (reworked): bounded pool of EXACT-SIZE auxiliary deferred
    // scratch packs keyed by (width,height). The shared deferred fullscreen
    // shaders sample the G-buffer with normalized [0,1] UVs (softenLightV.glsl
    // -> deferredUtil.glsl), so the physical target size MUST equal the render
    // size -- a sub-rect of a larger shared target would be sampled shrunk and
    // polluted by a sibling capture's stale texels. Packs are reused across
    // frames by size (no per-frame realloc for a steady capture set) and the
    // pool is bounded to MAX_CAPTURES entries with LRU eviction.
    constexpr U32 MAX_SCRATCH_EXTENT = 1024;

    // Never leave the active pointer aimed at an entry this call may release
    // or evict below; it is re-set only on success.
    mActivePrismLensScratch = nullptr;

    if (width == 0 || height == 0 ||
        width > MAX_SCRATCH_EXTENT || height > MAX_SCRATCH_EXTENT)
    {
        return false;
    }

    const bool needs_deferred_light = RenderDeferredSSAO || RenderShadowDetail > 0;
    auto pack_complete = [needs_deferred_light](const PrismLensScratch& entry)
    {
        return entry.rt.deferredScreen.isComplete() &&
               entry.rt.screen.isComplete() &&
               (!needs_deferred_light || entry.rt.deferredLight.isComplete()) &&
               entry.waterDis.isComplete() &&
               entry.waterExclusionMask.isComplete();
    };

    // Reuse an existing exact-size pack when its targets are all live.
    for (PrismLensScratch& entry : mPrismLensScratchPool)
    {
        if (entry.width == width && entry.height == height && pack_complete(entry))
        {
            entry.lastUsedFrame = gFrameCount;
            mActivePrismLensScratch = &entry;
            return true;
        }
    }

    // No match: recycle an empty/incomplete entry first; if every entry is
    // live, evict the least-recently-used pack.
    PrismLensScratch* target = nullptr;
    for (PrismLensScratch& entry : mPrismLensScratchPool)
    {
        if (entry.width == 0 || entry.height == 0 || !pack_complete(entry))
        {
            target = &entry;
            break;
        }
    }
    if (!target)
    {
        for (PrismLensScratch& entry : mPrismLensScratchPool)
        {
            if (!target || entry.lastUsedFrame < target->lastUsedFrame)
            {
                target = &entry;
            }
        }
    }

    release_prism_scratch_entry(*target);

    // Allocate at EXACTLY the capture's bucketed size: physical == render, so
    // normalized-UV sampling addresses only texels this capture wrote.
    RenderTargetPack& rt = target->rt;
    if (!rt.deferredScreen.allocate(width, height, GL_SRGB8_ALPHA8, true) ||
        !addDeferredAttachments(rt.deferredScreen) ||
        !rt.screen.allocate(width, height, GL_RGBA16F) ||
        !target->waterDis.allocate(width, height, GL_RGBA16F, true) ||
        !target->waterExclusionMask.allocate(width, height, GL_R8, true))
    {
        release_prism_scratch_entry(*target);
        return false;
    }

    rt.deferredScreen.shareDepthBuffer(rt.screen);

    if (needs_deferred_light &&
        !rt.deferredLight.allocate(width, height, GL_RGBA16F))
    {
        release_prism_scratch_entry(*target);
        return false;
    }

    rt.width = width;
    rt.height = height;
    target->width = width;
    target->height = height;
    target->lastUsedFrame = gFrameCount;
    mActivePrismLensScratch = target;
    return true;
}

bool LLPipeline::allocatePrismLensOutput(U32 slot, U32 width, U32 height)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;

    constexpr U32 PRISM_OUTPUT_CAPACITY = 1024;

    if (slot >= LLPrismLens::MAX_CAPTURES || width == 0 || height == 0 ||
        width > PRISM_OUTPUT_CAPACITY || height > PRISM_OUTPUT_CAPACITY)
    {
        return false;
    }

    LLRenderTarget& output = mPrismLensOutput[slot];
    if (output.isComplete())
    {
        // Outputs are lifetime-fixed at maximum capacity. The producer owns
        // the logical copy extent and publishes its texture-region transform.
        return output.getWidth() == PRISM_OUTPUT_CAPACITY &&
               output.getHeight() == PRISM_OUTPUT_CAPACITY;
    }

    // Allocate once, lazily, before this capture can own a valid publication.
    // There is no release-before-allocate path, so a subsequent logical-size
    // change can never destroy a retained frame shared by sibling displays.
    return output.allocate(PRISM_OUTPUT_CAPACITY, PRISM_OUTPUT_CAPACITY,
                           GL_RGBA16F) &&
           output.isComplete() &&
           output.getWidth() == PRISM_OUTPUT_CAPACITY &&
           output.getHeight() == PRISM_OUTPUT_CAPACITY;
}

LLRenderTarget& LLPipeline::getWaterDisTarget()
{
    if (sPrismLensRender && mActivePrismLensScratch &&
        mRT == &mActivePrismLensScratch->rt)
    {
        return mActivePrismLensScratch->waterDis;
    }
    return mWaterDis;
}

LLRenderTarget& LLPipeline::getWaterExclusionMaskTarget()
{
    if (sPrismLensRender && mActivePrismLensScratch &&
        mRT == &mActivePrismLensScratch->rt)
    {
        return mActivePrismLensScratch->waterExclusionMask;
    }
    return mWaterExclusionMask;
}

void LLPipeline::bindPrismLensTarget(LLRenderTarget& target)
{
    // Prism scratch packs are exact-size, so bindTarget's own full-size
    // viewport is already correct; no logical sub-rect override remains.
    target.bindTarget();
}

void LLPipeline::releasePrismLensBuffers()
{
    for (PrismLensScratch& entry : mPrismLensScratchPool)
    {
        release_prism_scratch_entry(entry);
    }
    // The pool is gone; nothing may keep rendering into a released entry.
    mActivePrismLensScratch = nullptr;
}

void LLPipeline::releasePrismLensOutput(U32 slot)
{
    if (slot < LLPrismLens::MAX_CAPTURES)
    {
        mPrismLensOutput[slot].release();
    }
}

void LLPipeline::releasePrismLensOutputs()
{
    for (U32 slot = 0; slot < LLPrismLens::MAX_CAPTURES; ++slot)
    {
        releasePrismLensOutput(slot);
    }
}

// must be even to avoid a stripe in the horizontal shadow blur
inline U32 BlurHappySize(U32 x, F32 scale) { return U32( x * scale + 16.0f) & ~0xF; }

bool LLPipeline::allocateShadowBuffer(U32 resX, U32 resY)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    S32 shadow_detail = RenderShadowDetail;

    F32 scale = gCubeSnapshot ? 1.0f : llmax(0.f, RenderShadowResolutionScale); // Don't scale probe shadow maps
    U32 sun_shadow_map_width = BlurHappySize(resX, scale);
    U32 sun_shadow_map_height = BlurHappySize(resY, scale);

    if (shadow_detail > 0)
    { //allocate 4 sun shadow maps
        for (U32 i = 0; i < 4; i++)
        {
            U32 cascade_width = sun_shadow_map_width;
            U32 cascade_height = sun_shadow_map_height;
            // [BDMerge G4.1] per-cascade square resolution override (donor
            // behavior: BD's RenderShadowResolution Vector4). 0 = stock
            // screen-derived sizing. Probe shadow maps keep stock sizing.
            if (!gCubeSnapshot)
            {
                static const char* cascade_keys[4] = { "BDMergeShadowResolution0", "BDMergeShadowResolution1", "BDMergeShadowResolution2", "BDMergeShadowResolution3" };
                U32 custom = gSavedSettings.getU32(cascade_keys[i]);
                if (custom >= 256)
                {
                    cascade_width = cascade_height = llclamp(custom, 256u, 8192u);
                }
            }
            if (!mRT->shadow[i].allocate(cascade_width, cascade_height, 0, true))
            {
                return false;
            }
        }
    }
    else
    {
        for (U32 i = 0; i < 4; i++)
        {
            releaseSunShadowTarget(i);
        }
    }

    if (!gCubeSnapshot) // hack to not allocate spot shadow maps during ReflectionMapManager init
    {
        U32 width = (U32)(resX * scale);
        U32 height = width;

        if (shadow_detail > 1)
        { //allocate two spot shadow maps
            U32 spot_shadow_map_width = width;
            U32 spot_shadow_map_height = height;
            // [BDMerge G4.1] independent projector shadow resolution
            U32 custom_spot = gSavedSettings.getU32("BDMergeProjectorShadowResolution");
            if (custom_spot >= 256)
            {
                spot_shadow_map_width = spot_shadow_map_height = llclamp(custom_spot, 256u, 8192u);
            }
            const U32 num_spots = bdmergeMaxSpotShadows(); // [BDMerge NSpot]
            for (U32 i = num_spots; i < MAX_SPOT_SHADOWS; i++)
            { // slots beyond the runtime count: release and unassign
                mSpotShadow[i].release();
                mShadowSpotLight[i] = NULL;
                mTargetShadowSpotLight[i] = NULL;
            }
            for (U32 i = 0; i < num_spots; i++)
            {
                if (!mSpotShadow[i].allocate(spot_shadow_map_width, spot_shadow_map_height, 0, true))
                {
                    return false;
                }
            }
        }
        else
        {
            releaseSpotShadowTargets();
        }
    }


    // set up shadow map filtering and compare modes
    if (shadow_detail > 0)
    {
        for (U32 i = 0; i < 4; i++)
        {
            LLRenderTarget* shadow_target = getSunShadowTarget(i);
            if (shadow_target)
            {
                gGL.getTexUnit(0)->bind(getSunShadowTarget(i), true);
                gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_ANISOTROPIC);
                gGL.getTexUnit(0)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
            }
        }
    }

    if (shadow_detail > 1 && !gCubeSnapshot)
    {
        for (U32 i = 0; i < bdmergeMaxSpotShadows(); i++)
        {
            LLRenderTarget* shadow_target = getSpotShadowTarget(i);
            if (shadow_target)
            {
                gGL.getTexUnit(0)->bind(shadow_target, true);
                gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_ANISOTROPIC);
                gGL.getTexUnit(0)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
            }
        }
    }

    return true;
}

//static
void LLPipeline::updateRenderTransparentWater()
{
    sRenderTransparentWater = gSavedSettings.getBOOL("RenderTransparentWater");
}

// static
void LLPipeline::refreshCachedSettings()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DISPLAY;
    LLPipeline::sAutoMaskAlphaDeferred = gSavedSettings.getBOOL("RenderAutoMaskAlphaDeferred");
    LLPipeline::sAutoMaskAlphaNonDeferred = gSavedSettings.getBOOL("RenderAutoMaskAlphaNonDeferred");
    LLPipeline::sUseFarClip = gSavedSettings.getBOOL("RenderUseFarClip");
    LLVOAvatar::sMaxNonImpostors = gSavedSettings.getU32("RenderAvatarMaxNonImpostors");
    LLVOAvatar::updateImpostorRendering(LLVOAvatar::sMaxNonImpostors);

    LLPipeline::sUseOcclusion =
            (!gUseWireframe
            && LLFeatureManager::getInstance()->isFeatureAvailable("UseOcclusion")
            && gSavedSettings.getBOOL("UseOcclusion")) ? 2 : 0;

    WindLightUseAtmosShaders = true; // DEPRECATED -- gSavedSettings.getBOOL("WindLightUseAtmosShaders");
    RenderDeferred = true; // DEPRECATED -- gSavedSettings.getBOOL("RenderDeferred");
    RenderDeferredSunWash = gSavedSettings.getF32("RenderDeferredSunWash");
    RenderFSAAType = gSavedSettings.getU32("RenderFSAAType");
    // [BDMerge] SMAA T2x (type 3) was reverted (A5.8: 50/50 temporal blend with no
    // velocity reprojection ghosted on any motion). A settings file saved while it
    // existed may still carry 3, which would silently select NO AA - map it back
    // to plain SMAA.
    if (RenderFSAAType > 2)
    {
        RenderFSAAType = 2;
    }
    RenderResolutionDivisor = gSavedSettings.getU32("RenderResolutionDivisor");
// [SL:KB] - Patch: Settings-RenderResolutionMultiplier | Checked: Catznip-5.4
    RenderResolutionMultiplier = gSavedSettings.getF32("RenderResolutionMultiplier");
// [/SL:KB]
    RenderUIBuffer = gSavedSettings.getBOOL("RenderUIBuffer");
    RenderShadowDetail = gSavedSettings.getS32("RenderShadowDetail");
    RenderShadowSplits = gSavedSettings.getS32("RenderShadowSplits");
    RenderDeferredSSAO = gSavedSettings.getBOOL("RenderDeferredSSAO");
    RenderShadowResolutionScale = gSavedSettings.getF32("RenderShadowResolutionScale");
    RenderDelayCreation = gSavedSettings.getBOOL("RenderDelayCreation");
    RenderAnimateRes = gSavedSettings.getBOOL("RenderAnimateRes");
    FreezeTime = gSavedSettings.getBOOL("FreezeTime");
    DebugBeaconLineWidth = gSavedSettings.getS32("DebugBeaconLineWidth");
    RenderHighlightBrightness = gSavedSettings.getF32("RenderHighlightBrightness");
    RenderHighlightColor = gSavedSettings.getColor4("RenderHighlightColor");
    RenderHighlightThickness = gSavedSettings.getF32("RenderHighlightThickness");
    RenderSpotLightsInNondeferred = gSavedSettings.getBOOL("RenderSpotLightsInNondeferred");
    PreviewAmbientColor = gSavedSettings.getColor4("PreviewAmbientColor");
    PreviewDiffuse0 = gSavedSettings.getColor4("PreviewDiffuse0");
    PreviewSpecular0 = gSavedSettings.getColor4("PreviewSpecular0");
    PreviewDiffuse1 = gSavedSettings.getColor4("PreviewDiffuse1");
    PreviewSpecular1 = gSavedSettings.getColor4("PreviewSpecular1");
    PreviewDiffuse2 = gSavedSettings.getColor4("PreviewDiffuse2");
    PreviewSpecular2 = gSavedSettings.getColor4("PreviewSpecular2");
    PreviewDirection0 = gSavedSettings.getVector3("PreviewDirection0");
    PreviewDirection1 = gSavedSettings.getVector3("PreviewDirection1");
    PreviewDirection2 = gSavedSettings.getVector3("PreviewDirection2");
    RenderGlowMaxExtractAlpha = gSavedSettings.getF32("RenderGlowMaxExtractAlpha");
    RenderGlowWarmthAmount = gSavedSettings.getF32("RenderGlowWarmthAmount");
    RenderGlowLumWeights = gSavedSettings.getVector3("RenderGlowLumWeights");
    RenderGlowWarmthWeights = gSavedSettings.getVector3("RenderGlowWarmthWeights");
    RenderGlowResolutionPow = gSavedSettings.getS32("RenderGlowResolutionPow");
    RenderGlowIterations = gSavedSettings.getS32("RenderGlowIterations");
    RenderGlowWidth = gSavedSettings.getF32("RenderGlowWidth");
    RenderGlowStrength = gSavedSettings.getF32("RenderGlowStrength");
    RenderGlowNoise = gSavedSettings.getBOOL("RenderGlowNoise");
    RenderDepthOfField = gSavedSettings.getBOOL("RenderDepthOfField");
    CameraFocusTransitionTime = gSavedSettings.getF32("CameraFocusTransitionTime");
    CameraFNumber = gSavedSettings.getF32("CameraFNumber");
    CameraFocalLength = gSavedSettings.getF32("CameraFocalLength");
    CameraFieldOfView = gSavedSettings.getF32("CameraFieldOfView");
    RenderShadowNoise = gSavedSettings.getF32("RenderShadowNoise");
    RenderShadowBlurSize = gSavedSettings.getF32("RenderShadowBlurSize");
    RenderSSAOScale = gSavedSettings.getF32("RenderSSAOScale");
    RenderSSAOMaxScale = gSavedSettings.getU32("RenderSSAOMaxScale");
    RenderSSAOFactor = gSavedSettings.getF32("RenderSSAOFactor");
    RenderSSAOEffect = gSavedSettings.getVector3("RenderSSAOEffect");
    RenderShadowOffsetError = gSavedSettings.getF32("RenderShadowOffsetError");
    RenderShadowBiasError = gSavedSettings.getF32("RenderShadowBiasError");
    RenderShadowOffset = gSavedSettings.getF32("RenderShadowOffset");
    RenderShadowBias = gSavedSettings.getF32("RenderShadowBias");
    RenderSpotShadowOffset = gSavedSettings.getF32("RenderSpotShadowOffset");
    RenderSpotShadowBias = gSavedSettings.getF32("RenderSpotShadowBias");
    RenderEdgeDepthCutoff = gSavedSettings.getF32("RenderEdgeDepthCutoff");
    RenderEdgeNormCutoff = gSavedSettings.getF32("RenderEdgeNormCutoff");
    RenderShadowGaussian = gSavedSettings.getVector3("RenderShadowGaussian");
    RenderShadowBlurDistFactor = gSavedSettings.getF32("RenderShadowBlurDistFactor");
    RenderDeferredAtmospheric = gSavedSettings.getBOOL("RenderDeferredAtmospheric");
    RenderHighlightFadeTime = gSavedSettings.getF32("RenderHighlightFadeTime");
    RenderFarClip = gSavedSettings.getF32("RenderFarClip");
    RenderShadowSplitExponent = gSavedSettings.getVector3("RenderShadowSplitExponent");
    RenderShadowErrorCutoff = gSavedSettings.getF32("RenderShadowErrorCutoff");
    RenderShadowFOVCutoff = gSavedSettings.getF32("RenderShadowFOVCutoff");
    CameraOffset = gSavedSettings.getBOOL("CameraOffset");
    CameraMaxCoF = gSavedSettings.getF32("CameraMaxCoF");
    CameraDoFResScale = gSavedSettings.getF32("CameraDoFResScale");
    RenderAutoHideSurfaceAreaLimit = gSavedSettings.getF32("RenderAutoHideSurfaceAreaLimit");
    RenderScreenSpaceReflections = gSavedSettings.getBOOL("RenderScreenSpaceReflections");
    // [BDMerge G3.2]
    RenderVolumetricLighting = gSavedSettings.getBOOL("RenderVolumetricLighting");
    RenderVolumetricLightingResolution = gSavedSettings.getU32("RenderVolumetricLightingResolution");
    RenderVolumetricLightingMultiplier = gSavedSettings.getF32("RenderVolumetricLightingMultiplier");
    RenderVolumetricLightingFalloffMultiplier = gSavedSettings.getF32("RenderVolumetricLightingFalloffMultiplier");
    // [BDMerge G3.3]
    BDMergeProjectorVolumetrics = gSavedSettings.getBOOL("BDMergeProjectorVolumetrics");
    BDMergeProjectorVolumetricsResolution = gSavedSettings.getU32("BDMergeProjectorVolumetricsResolution");
    BDMergeProjectorVolumetricsMultiplier = gSavedSettings.getF32("BDMergeProjectorVolumetricsMultiplier");
    BDMergeProjectorVolumetricsAnisotropy = gSavedSettings.getF32("BDMergeProjectorVolumetricsAnisotropy");
    BDMergeProjectorVolumetricsDither = gSavedSettings.getU32("BDMergeProjectorVolumetricsDither");
    BDMergeProjectorVolumetricsFeather = gSavedSettings.getF32("BDMergeProjectorVolumetricsFeather");
    BDMergeProjectorVolumetricsShadowSamples = gSavedSettings.getU32("BDMergeProjectorVolumetricsShadowSamples");
    BDMergeProjectorVolumetricsScissor = gSavedSettings.getBOOL("BDMergeProjectorVolumetricsScissor");
    BDMergeProjectorVolumetricsAdaptive = gSavedSettings.getBOOL("BDMergeProjectorVolumetricsAdaptive");
    BDMergeProjectorVolumetricsHalfRes = gSavedSettings.getBOOL("BDMergeProjectorVolumetricsHalfRes");
    BDMergeProjectorVolumetricsMinResolution = gSavedSettings.getU32("BDMergeProjectorVolumetricsMinResolution");
    BDMergeProjectorVolumetricsMaxLuminance = gSavedSettings.getF32("BDMergeProjectorVolumetricsMaxLuminance");
    BDMergeProjectorVolumetricsFrustumClip = gSavedSettings.getBOOL("BDMergeProjectorVolumetricsFrustumClip");
    BDMergeProjectorVolumetricsShadowJitterTap = gSavedSettings.getBOOL("BDMergeProjectorVolumetricsShadowJitterTap");
    BDMergeProjectorVolumetricsTint = gSavedSettings.getColor3("BDMergeProjectorVolumetricsTint");
    BDMergeProjectorVolumetricsTintStrength = gSavedSettings.getF32("BDMergeProjectorVolumetricsTintStrength");
    BDMergeProjectorVolumetricsDensity = gSavedSettings.getF32("BDMergeProjectorVolumetricsDensity");
    BDMergeProjectorVolumetricsNoiseStrength = gSavedSettings.getF32("BDMergeProjectorVolumetricsNoiseStrength");
    BDMergeProjectorVolumetricsNoiseScale = gSavedSettings.getF32("BDMergeProjectorVolumetricsNoiseScale");
    BDMergeProjectorVolumetricsNoiseSpeed = gSavedSettings.getF32("BDMergeProjectorVolumetricsNoiseSpeed");
    BDMergeProjectorVolumetricsFogStrength = gSavedSettings.getF32("BDMergeProjectorVolumetricsFogStrength");
    BDMergeProjectorVolumetricsFogGroundDensity = gSavedSettings.getF32("BDMergeProjectorVolumetricsFogGroundDensity");
    BDMergeProjectorVolumetricsFogFalloff = gSavedSettings.getF32("BDMergeProjectorVolumetricsFogFalloff");
    BDMergeProjectorVolumetricsFogBase = gSavedSettings.getF32("BDMergeProjectorVolumetricsFogBase");
    BDMergeProjectorVolumetricsBloomFeed = gSavedSettings.getF32("BDMergeProjectorVolumetricsBloomFeed");
    BDMergeProjectorVolumetricsBloomFeedAnamorphic = gSavedSettings.getF32("BDMergeProjectorVolumetricsBloomFeedAnamorphic");
    BDMergeProjectorVolumetricsTemporal = gSavedSettings.getBOOL("BDMergeProjectorVolumetricsTemporal");
    BDMergeProjectorVolumetricsTemporalBlend = gSavedSettings.getF32("BDMergeProjectorVolumetricsTemporalBlend");
    BDMergeProjectorVolumetricsTemporalReject = gSavedSettings.getF32("BDMergeProjectorVolumetricsTemporalReject");
    BDMergeProjectorVolumetricsTemporalBeamDepth = gSavedSettings.getBOOL("BDMergeProjectorVolumetricsTemporalBeamDepth");
    BDMergeProjectorVolumetricsShadowTint = gSavedSettings.getF32("BDMergeProjectorVolumetricsShadowTint");
    BDMergeProjectorVolumetricsRimStrength = gSavedSettings.getF32("BDMergeProjectorVolumetricsRimStrength");
    BDMergeProjectorVolumetricsRimPower = gSavedSettings.getF32("BDMergeProjectorVolumetricsRimPower");
    BDMergeProjectorVolumetricsRimThreshold = gSavedSettings.getF32("BDMergeProjectorVolumetricsRimThreshold");
    BDMergeProjectorVolumetricsRimWrap = gSavedSettings.getF32("BDMergeProjectorVolumetricsRimWrap");
    BDMergeProjectorVolumetricsRimSoftness = gSavedSettings.getF32("BDMergeProjectorVolumetricsRimSoftness");
    // [BDMerge G3.3 ConservativeShadow + Dust]
    BDMergeProjectorVolumetricsConservativeShadow = gSavedSettings.getBOOL("BDMergeProjectorVolumetricsConservativeShadow");
    BDMergeProjectorVolumetricsDust = gSavedSettings.getBOOL("BDMergeProjectorVolumetricsDust");
    BDMergeProjectorVolumetricsDustIntensity = gSavedSettings.getF32("BDMergeProjectorVolumetricsDustIntensity");
    BDMergeProjectorVolumetricsDustScale = gSavedSettings.getF32("BDMergeProjectorVolumetricsDustScale");
    BDMergeProjectorVolumetricsDustDrift = gSavedSettings.getF32("BDMergeProjectorVolumetricsDustDrift");
    // [BDMerge Froxel F0] hybrid froxel volumetrics grid
    BDMergeFroxelVolumetrics = gSavedSettings.getBOOL("BDMergeFroxelVolumetrics");
    BDMergeFroxelGridX = gSavedSettings.getU32("BDMergeFroxelGridX");
    BDMergeFroxelGridY = gSavedSettings.getU32("BDMergeFroxelGridY");
    BDMergeFroxelGridZ = gSavedSettings.getU32("BDMergeFroxelGridZ");
    BDMergeFroxelFar = gSavedSettings.getF32("BDMergeFroxelFar");
    BDMergeFroxelDensity = gSavedSettings.getF32("BDMergeFroxelDensity");
    BDMergeFroxelFogStrength = gSavedSettings.getF32("BDMergeFroxelFogStrength");
    BDMergeFroxelFogGroundDensity = gSavedSettings.getF32("BDMergeFroxelFogGroundDensity");
    BDMergeFroxelFogFalloff = gSavedSettings.getF32("BDMergeFroxelFogFalloff");
    BDMergeFroxelFogBase = gSavedSettings.getF32("BDMergeFroxelFogBase");
    BDMergeFroxelNoiseStrength = gSavedSettings.getF32("BDMergeFroxelNoiseStrength");
    BDMergeFroxelNoiseScale = gSavedSettings.getF32("BDMergeFroxelNoiseScale");
    BDMergeFroxelNoiseSpeed = gSavedSettings.getF32("BDMergeFroxelNoiseSpeed");
    BDMergeFroxelAmbient = gSavedSettings.getF32("BDMergeFroxelAmbient"); // [BDMerge Froxel F1]
    BDMergeFroxelLights = gSavedSettings.getBOOL("BDMergeFroxelLights");       // [BDMerge Froxel F2]
    BDMergeFroxelMaxLights = gSavedSettings.getU32("BDMergeFroxelMaxLights");  // [BDMerge Froxel F2]
    BDMergeFroxelTemporal = gSavedSettings.getBOOL("BDMergeFroxelTemporal");            // [BDMerge Froxel F3]
    BDMergeFroxelTemporalBlend = gSavedSettings.getF32("BDMergeFroxelTemporalBlend");   // [BDMerge Froxel F3]
    BDMergeFroxelDebug = gSavedSettings.getU32("BDMergeFroxelDebug");
    // [BDMerge Batch 2]
    BDMergeSoftProjectorShadows = gSavedSettings.getBOOL("BDMergeSoftProjectorShadows");
    BDMergeSoftShadowSoftness = gSavedSettings.getF32("BDMergeSoftShadowSoftness");
    BDMergeSoftShadowMaxPenumbra = gSavedSettings.getF32("BDMergeSoftShadowMaxPenumbra");
    BDMergeSoftShadowFill = gSavedSettings.getF32("BDMergeSoftShadowFill");
    BDMergeSoftShadowSun = gSavedSettings.getBOOL("BDMergeSoftShadowSun");
    // [Vogel A/B] soft-shadow kernel selector + Vogel tap count
    BDMergeSoftShadowVogel = gSavedSettings.getBOOL("RenderShadowSoftVogel");
    BDMergeSoftShadowTaps = gSavedSettings.getS32("RenderShadowSoftTaps");
    BDMergeGoboAnisotropic = gSavedSettings.getBOOL("BDMergeGoboAnisotropic");
    BDMergeVelocityBuffer = gSavedSettings.getBOOL("BDMergeVelocityBuffer");     // [BDMerge A5.4-1a]
    BDMergeVelocityDebug = gSavedSettings.getBOOL("BDMergeVelocityDebug");       // [BDMerge A5.4-1a]
    BDMergeMotionBlurStrength = gSavedSettings.getS32("BDMergeMotionBlurStrength"); // [BDMerge A5.4-1a]
    BDMergeMotionBlur = gSavedSettings.getBOOL("BDMergeMotionBlur");             // [BDMerge A5.4-3]
    RenderScreenSpaceReflectionIterations = gSavedSettings.getS32("RenderScreenSpaceReflectionIterations");
    RenderScreenSpaceReflectionRayStep = gSavedSettings.getF32("RenderScreenSpaceReflectionRayStep");
    RenderScreenSpaceReflectionDistanceBias = gSavedSettings.getF32("RenderScreenSpaceReflectionDistanceBias");
    RenderScreenSpaceReflectionDepthRejectBias = gSavedSettings.getF32("RenderScreenSpaceReflectionDepthRejectBias");
    RenderScreenSpaceReflectionAdaptiveStepMultiplier = gSavedSettings.getF32("RenderScreenSpaceReflectionAdaptiveStepMultiplier");
    RenderScreenSpaceReflectionGlossySamples = gSavedSettings.getS32("RenderScreenSpaceReflectionGlossySamples");
    RenderBufferVisualization = gSavedSettings.getS32("RenderBufferVisualization");
    RenderMirrors = gSavedSettings.getBOOL("RenderMirrors");
    RenderHeroProbeUpdateRate = gSavedSettings.getS32("RenderHeroProbeUpdateRate");
    RenderHeroProbeConservativeUpdateMultiplier = gSavedSettings.getS32("RenderHeroProbeConservativeUpdateMultiplier");
    RenderAvatarCloth = gSavedSettings.getBOOL("RenderAvatarCloth");

    sReflectionProbesEnabled = LLFeatureManager::getInstance()->isFeatureAvailable("RenderReflectionsEnabled") && gSavedSettings.getBOOL("RenderReflectionsEnabled");
    RenderSpotLight = nullptr;

    if (gNonInteractive)
    {
        LLVOAvatar::sMaxNonImpostors = 1;
        LLVOAvatar::updateImpostorRendering(LLVOAvatar::sMaxNonImpostors);
    }

    bool enable_buffers = gSavedSettings.getBOOL("CollectFontVertexBuffers");
    LLFontVertexBuffer::enableBufferCollection(enable_buffers);
    LLFontWidthBuffer::enableBufferCollection(enable_buffers);
}

void LLPipeline::releaseGLBuffers()
{
    assertInitialized();

    if (mNoiseMap)
    {
        LLImageGL::deleteTextures(1, &mNoiseMap);
        mNoiseMap = 0;
    }

    if (mTrueNoiseMap)
    {
        LLImageGL::deleteTextures(1, &mTrueNoiseMap);
        mTrueNoiseMap = 0;
    }

    if (mSMAAAreaMap)
    {
        LLImageGL::deleteTextures(1, &mSMAAAreaMap);
        mSMAAAreaMap = 0;
    }

    if (mSMAASearchMap)
    {
        LLImageGL::deleteTextures(1, &mSMAASearchMap);
        mSMAASearchMap = 0;
    }

    // [BDMerge G3.3 Dust] drop the dust volume with the other GL textures and
    // clear the attempted flag so a fresh GL context lazily reloads it.
    if (mProjVolDustMap)
    {
        LLImageGL::deleteTextures(1, &mProjVolDustMap);
        mProjVolDustMap = 0;
    }
    mProjVolDustLoadAttempted = false;

    releaseLUTBuffers();

    mWaterDis.release();

    mSceneMap.release();

    mProjVolHalf.release(); // [BDMerge G3.3 Phase 1 item 3]
    mWeatherRainHalf.release();
    mWeatherRainOcclusion.release();
    mWeatherRainOcclusionValid = false;
    mWeatherRainOcclusionFrame = 0;
    mWeatherRainOcclusionFailedResolution = 0;
    mWeatherRainOcclusionDepthRange = 1.f;
    mWeatherRainOcclusionMatrix = glm::mat4(1.f);
    mProjVolHistory[0].release(); // [BDMerge G3.3 Batch 1 A] temporal history
    mProjVolHistory[1].release();
    mProjVolHistoryValid = false;

    mFroxelMedia.release(); // [BDMerge Froxel F0] froxel media atlas
    mFroxelMediaValid = false;
    mFroxelIntegrated.release(); // [BDMerge Froxel F1] integrated froxel atlas
    mFroxelIntegratedValid = false;
    mFroxelLight.release(); // [BDMerge Froxel F2] light-injection froxel atlas
    mFroxelLightValid = false;
    mFroxelLightHistory[0].release(); // [BDMerge Froxel F3] temporal light-atlas history
    mFroxelLightHistory[1].release();
    mFroxelHistoryValid = false;

    mWaterExclusionMask.release();

    mFXAAMap.release();
    mSMAABlendBuffer.release();
    mVelocityMap.release(); // [BDMerge A5.4-1a] velocity / motion-vector buffer

    mUIScreen.release();

    mDownResMap.release();

    mBakeMap.release();

    for (U32 i = 0; i < 3; i++)
    {
        mGlow[i].release();
    }

    mHeroProbeManager.cleanup(); // release hero probes

    releaseScreenBuffers();
    releaseShadowBuffers();

    gBumpImageList.destroyGL();
    LLVOAvatar::resetImpostors();
}

void LLPipeline::releaseLUTBuffers()
{
    if (mLightFunc)
    {
        LLImageGL::deleteTextures(1, &mLightFunc);
        mLightFunc = 0;
    }

    mPbrBrdfLut.release();

    mExposureMap.release();
    mLuminanceMap.release();
    mLastExposure.release();

}

void LLPipeline::releaseShadowBuffers()
{
    // Sun shadows are allocated through mRT->shadow[], and mRT is swapped
    // between packs during cube snapshots, so any of the three packs may
    // hold allocations. releaseSunShadowTargets() only handles the active
    // pack (because resize callers want exactly that); for full teardown
    // we have to walk every pack ourselves.
    auto release_sun_shadows = [](RenderTargetPack& rt)
    {
        for (U32 i = 0; i < 4; i++)
        {
            rt.shadow[i].release();
        }
    };
    release_sun_shadows(mMainRT);
    release_sun_shadows(mAuxillaryRT);
    release_sun_shadows(mHeroProbeRT);

    releaseSpotShadowTargets();
}

void LLPipeline::releaseScreenBuffers()
{
    auto release_pack = [](RenderTargetPack& rt)
    {
        rt.screen.release();
        rt.deferredScreen.release();
        rt.deferredLight.release();
        rt.postPingMap.release();
        rt.postPongMap.release();
        for (U32 i = 0; i < BLOOM_MAX_MIPS; i++)
        {
            rt.bloomMip[i].release();
        }
        rt.bloomMipCount = 0;
    };
    release_pack(mMainRT);
    release_pack(mAuxillaryRT);
    release_pack(mHeroProbeRT);
    releasePrismLensBuffers();
    releasePrismLensOutputs();
    LLPrismLens::onRenderTargetsReleased();
}

void LLPipeline::releaseSunShadowTarget(U32 index)
{
    llassert(index < 4);
    mRT->shadow[index].release();
}

void LLPipeline::releaseSunShadowTargets()
{
    for (U32 i = 0; i < 4; i++)
    {
        releaseSunShadowTarget(i);
    }
}

void LLPipeline::releaseSpotShadowTargets()
{
    if (!gCubeSnapshot) // hack to avoid freeing spot shadows during ReflectionMapManager init
    {
        for (U32 i = 0; i < MAX_SPOT_SHADOWS; i++)
        {
            mSpotShadow[i].release();
            // [Prism spot shadows Stage 2] the dedicated aux maps track
            // mSpotShadow's lifetime (resize + GL teardown both land here);
            // generatePrismSpotShadows lazily re-allocates on next use.
            mPrismSpotShadow[i].release();
        }
    }
}

void LLPipeline::createGLBuffers()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    stop_glerror();
    assertInitialized();

    stop_glerror();

    GLuint resX = gViewerWindow->getWorldViewWidthRaw();
    GLuint resY = gViewerWindow->getWorldViewHeightRaw();

    bool hdr = gGLManager.mGLVersion > 4.05f && gSavedSettings.getBOOL("RenderHDREnabled");
    if (!hdr)
    {
        // allocate screen space glow buffers
        const U32 glow_res = llmax(1, llmin(512, 1 << gSavedSettings.getS32("RenderGlowResolutionPow")));
        const bool glow_hdr = gSavedSettings.getBOOL("RenderGlowHDR");
        const U32 glow_color_fmt = glow_hdr ? GL_RGBA16F : GL_RGBA;
        for (U32 i = 0; i < 3; i++)
        {
            mGlow[i].allocate(512, glow_res, glow_color_fmt);
        }
    }

    allocateScreenBuffer(resX, resY);
    // Do not zero out mRT dimensions here. allocateScreenBuffer() above
    // already sets the correct dimensions. Zeroing them caused resizeShadowTexture()
    // to fail if called immediately after createGLBuffers (e.g., post graphics change).
    // mRT->width = 0;
    // mRT->height = 0;


    if (!mNoiseMap)
    {
        const U32 noiseRes = 128;
        LLVector3 noise[noiseRes*noiseRes];

        F32 scaler = gSavedSettings.getF32("RenderDeferredNoise")/100.f;
        for (U32 i = 0; i < noiseRes*noiseRes; ++i)
        {
            noise[i] = LLVector3(ll_frand()-0.5f, ll_frand()-0.5f, 0.f);
            noise[i].normVec();
            noise[i].mV[2] = ll_frand()*scaler+1.f-scaler/2.f;
        }

        LLImageGL::generateTextures(1, &mNoiseMap);

        gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, mNoiseMap);
        LLImageGL::setManualImage(LLTexUnit::getInternalType(LLTexUnit::TT_TEXTURE), 0, GL_RGB16F, noiseRes, noiseRes, GL_RGB, GL_FLOAT, noise, false);
        gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
    }

    if (!mTrueNoiseMap)
    {
        const U32 noiseRes = 128;
        F32 noise[noiseRes*noiseRes*3];
        for (U32 i = 0; i < noiseRes*noiseRes*3; i++)
        {
            noise[i] = ll_frand()*2.0f-1.0f;
        }

        LLImageGL::generateTextures(1, &mTrueNoiseMap);
        gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, mTrueNoiseMap);
        LLImageGL::setManualImage(LLTexUnit::getInternalType(LLTexUnit::TT_TEXTURE), 0, GL_RGB16F, noiseRes, noiseRes, GL_RGB,GL_FLOAT, noise, false);
        gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
    }

    if (!mSMAAAreaMap)
    {
        std::vector<U8> tempBuffer(AREATEX_SIZE);
        for (U32 y = 0; y < AREATEX_HEIGHT; y++)
        {
            U32 srcY = AREATEX_HEIGHT - 1 - y;
            // unsigned int srcY = y;
            memcpy(&tempBuffer[y * AREATEX_PITCH], areaTexBytes + srcY * AREATEX_PITCH, AREATEX_PITCH);
        }

        LLImageGL::generateTextures(1, &mSMAAAreaMap);
        gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, mSMAAAreaMap);
        LLImageGL::setManualImage(LLTexUnit::getInternalType(LLTexUnit::TT_TEXTURE), 0, GL_RG8, AREATEX_WIDTH, AREATEX_HEIGHT, GL_RG,
            GL_UNSIGNED_BYTE, tempBuffer.data(), false);
        gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
        gGL.getTexUnit(0)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
    }

    if (!mSMAASearchMap)
    {
        std::vector<U8> tempBuffer(SEARCHTEX_SIZE);
        for (U32 y = 0; y < SEARCHTEX_HEIGHT; y++)
        {
            U32 srcY = SEARCHTEX_HEIGHT - 1 - y;
            // unsigned int srcY = y;
            memcpy(&tempBuffer[y * SEARCHTEX_PITCH], searchTexBytes + srcY * SEARCHTEX_PITCH, SEARCHTEX_PITCH);
        }

        LLImageGL::generateTextures(1, &mSMAASearchMap);
        gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, mSMAASearchMap);
        LLImageGL::setManualImage(LLTexUnit::getInternalType(LLTexUnit::TT_TEXTURE), 0, GL_R8, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT,
            GL_RED, GL_UNSIGNED_BYTE, tempBuffer.data(), false);
        gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
        gGL.getTexUnit(0)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
    }

    if (!mSMAASampleMap)
    {
        LLPointer<LLImageRaw>               raw_image = new LLImageRaw;
        LLPointer<LLImagePNG>               png_image = new LLImagePNG;
        static LLCachedControl<std::string> sample_path(gSavedSettings, "SamplePath", "");
        if (gDirUtilp->fileExists(sample_path()) && png_image->load(sample_path()) && png_image->decode(raw_image, 0.0f))
        {
            U32 format = 0;
            switch (raw_image->getComponents())
            {
            case 1:
                format = GL_RED;
                break;
            case 2:
                format = GL_RG;
                break;
            case 3:
                format = GL_RGB;
                break;
            case 4:
                format = GL_RGBA;
                break;
            default:
                return;
            };
            LLImageGL::generateTextures(1, &mSMAASampleMap);
            gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, mSMAASampleMap);
            LLImageGL::setManualImage(LLTexUnit::getInternalType(LLTexUnit::TT_TEXTURE), 0, GL_RGB, raw_image->getWidth(),
                raw_image->getHeight(), format, GL_UNSIGNED_BYTE, raw_image->getData(), false);
            stop_glerror();
            gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
            gGL.getTexUnit(0)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
        }
    }

    createLUTBuffers();

    setupGradingLUT();

    gBumpImageList.restoreGL();
}

F32 lerpf(F32 a, F32 b, F32 w)
{
    return a + w * (b - a);
}

void LLPipeline::createLUTBuffers()
{
    if (!mLightFunc)
    {
        U32 lightResX = gSavedSettings.getU32("RenderSpecularResX");
        U32 lightResY = gSavedSettings.getU32("RenderSpecularResY");
        F32* ls = nullptr;
        try
        {
            ls = new F32[lightResX*lightResY];
        }
        catch (std::bad_alloc&)
        {
            LLError::LLUserWarningMsg::showOutOfMemory();
            // might be better to set the error into mFatalMessage and rethrow
            LL_ERRS() << "Bad memory allocation in createLUTBuffers! lightResX: "
                << lightResX << " lightResY: " << lightResY << LL_ENDL;
        }
        F32 specExp = gSavedSettings.getF32("RenderSpecularExponent");
        // Calculate the (normalized) blinn-phong specular lookup texture. (with a few tweaks)
        for (U32 y = 0; y < lightResY; ++y)
        {
            for (U32 x = 0; x < lightResX; ++x)
            {
                ls[y*lightResX+x] = 0;
                F32 sa = (F32) x/(lightResX-1);
                F32 spec = (F32) y/(lightResY-1);
                F32 n = spec * spec * specExp;

                // Nothing special here.  Just your typical blinn-phong term.
                spec = powf(sa, n);

                // Apply our normalization function.
                // Note: This is the full equation that applies the full normalization curve, not an approximation.
                // This is fine, given we only need to create our LUT once per buffer initialization.
                spec *= (((n + 2) * (n + 4)) / (8 * F_PI * (powf(2, -n/2) + n)));

                // Since we use R16F, we no longer have a dynamic range issue we need to work around here.
                // Though some older drivers may not like this, newer drivers shouldn't have this problem.
                ls[y*lightResX+x] = spec;
            }
        }

        U32 pix_format = GL_R16F;
#if LL_DARWIN
        if(!gGLManager.mIsApple)
        {
            // Need to work around limited precision with 10.6.8 and older drivers
            //
            pix_format = GL_R32F;
        }
#endif
        LLImageGL::generateTextures(1, &mLightFunc);
        gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, mLightFunc);
        LLImageGL::setManualImage(LLTexUnit::getInternalType(LLTexUnit::TT_TEXTURE), 0, pix_format, lightResX, lightResY, GL_RED, GL_FLOAT, ls, false);
        gGL.getTexUnit(0)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
        gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_TRILINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        delete [] ls;
    }

    mPbrBrdfLut.allocate(512, 512, GL_RG16F);
    mPbrBrdfLut.bindTarget();

    if (gDeferredGenBrdfLutProgram.isComplete())
    {
        gDeferredGenBrdfLutProgram.bind();
        llassert_always(LLGLSLShader::sCurBoundShaderPtr != nullptr);

        gGL.begin(LLRender::TRIANGLE_STRIP);
        gGL.vertex2f(-1, -1);
        gGL.vertex2f(-1, 1);
        gGL.vertex2f(1, -1);
        gGL.vertex2f(1, 1);
        gGL.end();
        gGL.flush();
    }
    else
    {
        LL_WARNS("Brad") << gDeferredGenBrdfLutProgram.mName << " failed to load, cannot be used!" << LL_ENDL;
    }

    gDeferredGenBrdfLutProgram.unbind();
    mPbrBrdfLut.flush();

    mExposureMap.allocate(1, 1, GL_R16F);
    mExposureMap.bindTarget();
    glClearColor(1, 1, 1, 0);
    mExposureMap.clear();
    glClearColor(0, 0, 0, 0);
    mExposureMap.flush();

    mLuminanceMap.allocate(256, 256, GL_R16F, false, false, LLTexUnit::TT_TEXTURE, LLTexUnit::TMG_AUTO);

    mLastExposure.allocate(1, 1, GL_R16F);
}

void LLPipeline::setupGradingLUT()
{
    if (mCGLut)
    {
        LLImageGL::deleteTextures(1, &mCGLut);
        mCGLut = 0;
    }

    std::string lut_name = gSavedSettings.getString("RenderColorGradeLUT");
    if (gSavedSettings.getBOOL("RenderColorGrade") && !lut_name.empty())
    {
        std::string lut_path = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "colorlut", lut_name);

        if (!LLFile::isfile(lut_path))
        {
            lut_path = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "colorlut", lut_name);
        }

        if (LLFile::isfile(lut_path))
        {
            std::string temp_exten = gDirUtilp->getExtension(lut_path);
            bool decode_success = false;
            LLPointer<LLImageRaw> raw_image;
            bool flip_green = true;
            bool swap_bluegreen = true;
            if (temp_exten == "cube")
            {
                LutCube lutCube(lut_path);
                if (!lutCube.colorCube.empty())
                {
                    try
                    {
                        raw_image = new LLImageRaw(lutCube.colorCube.data(), lutCube.size * lutCube.size, lutCube.size, 4);
                    }
                    catch (const std::bad_alloc&)
                    {
                        return;
                    }
                    flip_green = false;
                    swap_bluegreen = false;
                    decode_success = true;
                }
            }
            else
            {
                enum class ELutExt
                {
                    EXT_IMG_TGA = 0,
                    EXT_IMG_PNG,
                    EXT_IMG_JPEG,
                    EXT_IMG_BMP,
                    EXT_IMG_WEBP,
                    EXT_NONE
                };

                ELutExt extension = ELutExt::EXT_NONE;
                if (temp_exten == "tga")
                {
                    extension = ELutExt::EXT_IMG_TGA;
                }
                else if (temp_exten == "png")
                {
                    extension = ELutExt::EXT_IMG_PNG;
                }
                else if (temp_exten == "jpg" || temp_exten == "jpeg")
                {
                    extension = ELutExt::EXT_IMG_JPEG;
                }
                else if (temp_exten == "bmp")
                {
                    extension = ELutExt::EXT_IMG_BMP;
                }
                else if (temp_exten == "webp")
                {
                    extension = ELutExt::EXT_IMG_WEBP;
                }

                raw_image = new LLImageRaw;

                switch (extension)
                {
                    default:
                        break;
                    case ELutExt::EXT_IMG_TGA:
                    {
                        LLPointer<LLImageTGA> tga_image = new LLImageTGA;
                        if (tga_image->load(lut_path) && tga_image->decode(raw_image, 0.0f))
                        {
                            decode_success = true;
                        }
                        break;
                    }
                    case ELutExt::EXT_IMG_PNG:
                    {
                        LLPointer<LLImagePNG> png_image = new LLImagePNG;
                        if (png_image->load(lut_path) && png_image->decode(raw_image, 0.0f))
                        {
                            decode_success = true;
                        }
                        break;
                    }
                    case ELutExt::EXT_IMG_JPEG:
                    {
                        LLPointer<LLImageJPEG> jpg_image = new LLImageJPEG;
                        if (jpg_image->load(lut_path) && jpg_image->decode(raw_image, 0.0f))
                        {
                            decode_success = true;
                        }
                        break;
                    }
                    case ELutExt::EXT_IMG_BMP:
                    {
                        LLPointer<LLImageBMP> bmp_image = new LLImageBMP;
                        if (bmp_image->load(lut_path) && bmp_image->decode(raw_image, 0.0f))
                        {
                            decode_success = true;
                        }
                        break;
                    }
                    case ELutExt::EXT_IMG_WEBP:
                    {
                        LLPointer<LLImageWebP> webp_image = new LLImageWebP;
                        if (webp_image->load(lut_path) && webp_image->decode(raw_image, 0.0f))
                        {
                            decode_success = true;
                        }
                        break;
                    }
                }
            }

            if (decode_success && raw_image)
            {
                U32 primary_format = 0;
                U32 int_format = 0;
                switch (raw_image->getComponents())
                {
                    case 3:
                    {
                        primary_format = GL_RGB;
                        int_format = GL_RGB8;
                        break;
                    }
                    case 4:
                    {
                        primary_format = GL_RGBA;
                        int_format = GL_RGBA8;
                        break;
                    }
                    default:
                    {
                        LL_WARNS() << "Color LUT has invalid number of color components: " << raw_image->getComponents() << LL_ENDL;
                        return;
                    }
                };

                S32 image_height = raw_image->getHeight();
                S32 image_width  = raw_image->getWidth();
                if ((image_height > 0 && image_height <= gGLManager.mGLMaxTextureSize) // within dimension limit
                    && ((image_height * image_height) == image_width))                 // width is height * height
                {
                    mCGLutSize = LLVector4((F32)image_height, (F32)flip_green, (F32)swap_bluegreen);

                    LLImageGL::generateTextures(1, &mCGLut);
                    gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE_3D, mCGLut);
                    {
                        stop_glerror();
                        glTexImage3D(LLTexUnit::getInternalType(LLTexUnit::TT_TEXTURE_3D), 0, int_format, image_height, image_height,
                                        image_height, 0, primary_format, GL_UNSIGNED_BYTE, raw_image->getData());
                        stop_glerror();
                    }
                    gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
                    gGL.getTexUnit(0)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
                    gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE_3D);
                }
                else
                {
                    LL_WARNS() << "Color LUT is invalid width or height: " << image_height << " x " << image_width << " at path "
                                << lut_path << LL_ENDL;
                }
            }
            else
            {
                LL_WARNS() << "Failed to decode color grading LUT: " << lut_path << LL_ENDL;
            }
        }
    }
}

// [BDMerge G3.3 Dust] Lazy loader for the baked 64^3 RGBA8 dust volume shipped at
// app_settings/dust/dust_volume_64_rgba8.ktx (seed 0xD057C1A5; linear DATA, no
// sRGB: R = macro haze, G = sparse motes, B = fine turbulence, A = mote phase).
// KTX 1 is parsed inline - the 12-byte identifier, the 13 U32 header words, the
// key/value blob, then one U32 image size + tightly packed payload - and REJECTED
// unless it is exactly the shipped layout (native-endian GL_UNSIGNED_BYTE /
// GL_RGBA / GL_RGBA8 / typeSize 1 / base format GL_RGBA, 64x64x64, single mip).
// Every seek/read is bounds-checked against the real file length (a corruptible
// bytesOfKeyValueData can never seek outside [header_end, file_end]), and the GL
// upload itself is verified with an explicit glGetError() check. On ANY failure
// the texture is deleted and mProjVolDustMap stays 0, so the effect degrades to
// clean OFF: renderProjectorVolumetric only raises projvol_dust while
// mProjVolDustMap is live and verifiably bound, so a missing/corrupt asset (or a
// failed upload) yields the byte-identical shipped look, never partial dust or a
// sample of an incomplete texture.
void LLPipeline::loadProjVolDustMap()
{
    if (mProjVolDustMap != 0 || mProjVolDustLoadAttempted)
    {
        return;
    }
    mProjVolDustLoadAttempted = true; // one disk attempt per GL context

    const std::string path =
        gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "dust", "dust_volume_64_rgba8.ktx");

    LLFILE* file = LLFile::fopen(path, LLFILE_MODE("rb"));
    if (!file)
    {
        LL_WARNS() << "[BDMerge Dust] dust volume not found: " << path << LL_ENDL;
        return;
    }

    const U32 dim = 64;
    const U32 expected_bytes = dim * dim * dim * 4u; // tightly packed RGBA8
    bool ok = false;
    std::vector<U8> texels;
    do
    {
        // [Review fix] File length FIRST, so every seek/read below is bounds-
        // checked against it. The legit asset is ~1 MB; reject anything past a
        // generous cap (also covers files too large for the CRT's long ftell).
        const long max_file_bytes = 64L * 1024L * 1024L;
        long fsize = -1;
        if (fseek(file, 0, SEEK_END) == 0)
        {
            fsize = ftell(file);
        }
        if (fsize < 0 || fsize > max_file_bytes || fseek(file, 0, SEEK_SET) != 0)
        {
            LL_WARNS() << "[BDMerge Dust] implausible KTX file size (" << fsize
                       << " bytes): " << path << LL_ENDL;
            break;
        }

        // KTX 1 identifier + header. Header fields (all U32, written native
        // little-endian by the generator): 0 endianness, 1 glType, 2 glTypeSize,
        // 3 glFormat, 4 glInternalFormat, 5 glBaseInternalFormat, 6 pixelWidth,
        // 7 pixelHeight, 8 pixelDepth, 9 numberOfArrayElements, 10 numberOfFaces,
        // 11 numberOfMipmapLevels, 12 bytesOfKeyValueData.
        const U8 ktx_magic[12] = { 0xAB, 'K', 'T', 'X', ' ', '1', '1', 0xBB, '\r', '\n', 0x1A, '\n' };
        U8 magic[12];
        if (fread(magic, 1, sizeof(magic), file) != sizeof(magic) ||
            memcmp(magic, ktx_magic, sizeof(magic)) != 0)
        {
            LL_WARNS() << "[BDMerge Dust] not a KTX 1 file: " << path << LL_ENDL;
            break;
        }
        U32 hdr[13];
        if (fread(hdr, sizeof(U32), 13, file) != 13)
        {
            LL_WARNS() << "[BDMerge Dust] truncated KTX header: " << path << LL_ENDL;
            break;
        }
        if (hdr[0] != 0x04030201u ||               // native endianness only
            hdr[1] != GL_UNSIGNED_BYTE ||          // glType
            hdr[2] != 1 ||                         // glTypeSize (1 byte per component -
                                                   //  anything else implies endian swap)
            hdr[3] != GL_RGBA ||                   // glFormat
            hdr[4] != GL_RGBA8 ||                  // glInternalFormat
            hdr[5] != GL_RGBA ||                   // glBaseInternalFormat (must match glFormat)
            hdr[6] != dim || hdr[7] != dim || hdr[8] != dim ||
            hdr[9] != 0 ||                         // not an array texture
            hdr[10] != 1 ||                        // not a cube map
            hdr[11] > 1)                           // single mip (0 = unspecified is fine)
        {
            LL_WARNS() << "[BDMerge Dust] unsupported KTX layout (want native-endian RGBA8 "
                       << dim << "^3, 1 mip): " << path << LL_ENDL;
            break;
        }
        // [Review fix] hdr[12] (bytesOfKeyValueData) comes straight from the file.
        // Unchecked, a huge value cast to signed long went NEGATIVE and fseek'd
        // BACKWARD into the header, where a crafted image-size word could pass the
        // size check and upload header bytes as texels. Require the whole layout -
        // key/value blob + image-size word + payload - to fit inside the measured
        // file before seeking (U64 math, no overflow), which also guarantees the
        // (long) cast below is a small positive value.
        const U64 header_end = 12u + 13u * sizeof(U32); // magic + header = 64 bytes
        const U64 layout_end = header_end + (U64)hdr[12] + sizeof(U32) + (U64)expected_bytes;
        if (layout_end > (U64)fsize)
        {
            LL_WARNS() << "[BDMerge Dust] KTX key/value size " << hdr[12]
                       << " overruns the file (" << fsize << " bytes): " << path << LL_ENDL;
            break;
        }
        if (fseek(file, (long)hdr[12], SEEK_CUR) != 0) // skip key/value metadata
        {
            LL_WARNS() << "[BDMerge Dust] truncated KTX key/value data: " << path << LL_ENDL;
            break;
        }
        U32 image_size = 0;
        if (fread(&image_size, sizeof(U32), 1, file) != 1 || image_size != expected_bytes)
        {
            LL_WARNS() << "[BDMerge Dust] unexpected KTX image size " << image_size
                       << " (want " << expected_bytes << "): " << path << LL_ENDL;
            break;
        }
        texels.resize(expected_bytes);
        if (fread(texels.data(), 1, expected_bytes, file) != expected_bytes)
        {
            LL_WARNS() << "[BDMerge Dust] truncated KTX payload: " << path << LL_ENDL;
            break;
        }
        ok = true;
    } while (false);
    fclose(file);

    if (!ok)
    {
        return;
    }

    // GL_TEXTURE_3D per the asset README: RGBA8, REPEAT on all three axes (the
    // volume tiles seamlessly, matching the shader's fract()-wrapped lookups) and
    // plain LINEAR min/mag (single mip level -> bindManual reports no mips, so
    // TFO_BILINEAR resolves to non-mipmapped GL_LINEAR and stays complete).
    //
    // [Review fix] The upload is VERIFIED: stop_glerror() is inert unless
    // gDebugGL, so a failed glTexImage3D (out of memory, driver rejection) used
    // to leave mProjVolDustMap nonzero-but-incomplete - and dust_on keys on the
    // handle alone, so the march would sample an incomplete texture. Any
    // create/bind/upload failure now deletes the texture and zeroes the handle
    // so dust_on stays false (clean OFF; mProjVolDustLoadAttempted remains set,
    // one attempt per GL context).
    //
    // [Round-2 fix] The BIND and the SAMPLER-STATE setup are verified too, not
    // just the upload: bindManual() returns false only for an invalid texture-
    // unit index - after glBindTexture it returns true WITHOUT checking GL
    // errors (llrender.cpp) - and the filter/address calls used to run after
    // the last error check. The stale-error drain now precedes the bind: in its
    // old position (between bind and upload) it silently swallowed a failed
    // bind's error, after which glTexImage3D would have uploaded into whatever
    // 3D texture was ALREADY bound on unit 0. ANY error at any stage unbinds,
    // deletes and zeroes the handle so dust stays cleanly off.
    LLImageGL::generateTextures(1, &mProjVolDustMap);
    // Drain any stale error (bounded - GL_CONTEXT_LOST can repeat forever) so the
    // checks below attribute only OUR bind/upload/state calls, independent of
    // gDebugGL.
    for (S32 drain = 0; drain < 8 && glGetError() != GL_NO_ERROR; ++drain)
    {
    }
    if (mProjVolDustMap == 0 ||
        !gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE_3D, mProjVolDustMap) ||
        glGetError() != GL_NO_ERROR) // explicit post-bind check - bindManual never fails on GL errors
    {
        LL_WARNS() << "[BDMerge Dust] could not create/bind a GL_TEXTURE_3D for the dust volume"
                   << " - dust stays off" << LL_ENDL;
        if (mProjVolDustMap != 0)
        {
            // unbind first: a failed glBindTexture still updated LLTexUnit's
            // currency cache, and the deleted name must not linger there.
            gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE_3D);
            LLImageGL::deleteTextures(1, &mProjVolDustMap);
            mProjVolDustMap = 0;
        }
        return;
    }
    glTexImage3D(LLTexUnit::getInternalType(LLTexUnit::TT_TEXTURE_3D), 0, GL_RGBA8,
                 dim, dim, dim, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels.data());
    const GLenum upload_err = glGetError();
    if (upload_err != GL_NO_ERROR)
    {
        LL_WARNS() << "[BDMerge Dust] glTexImage3D failed (0x" << std::hex << (U32)upload_err
                   << std::dec << ") - dust stays off" << LL_ENDL;
        gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE_3D);
        LLImageGL::deleteTextures(1, &mProjVolDustMap);
        mProjVolDustMap = 0;
        return;
    }
    gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
    gGL.getTexUnit(0)->setTextureAddressMode(LLTexUnit::TAM_WRAP);
    // [Round-2 fix] Verify the filter/address setup as well: an error here would
    // leave the volume with wrong/incomplete sampler state, and these calls used
    // to run after the last error check.
    const GLenum state_err = glGetError();
    gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE_3D);
    if (state_err != GL_NO_ERROR)
    {
        LL_WARNS() << "[BDMerge Dust] dust volume sampler-state setup failed (0x"
                   << std::hex << (U32)state_err << std::dec << ") - dust stays off" << LL_ENDL;
        LLImageGL::deleteTextures(1, &mProjVolDustMap);
        mProjVolDustMap = 0;
        return;
    }

    LL_INFOS() << "[BDMerge Dust] loaded " << dim << "^3 dust volume: " << path << LL_ENDL;
}


void LLPipeline::restoreGL()
{
    assertInitialized();

    LLViewerShaderMgr::instance()->setShaders();

    for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin();
            iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
    {
        LLViewerRegion* region = *iter;
        for (U32 i = 0; i < LLViewerRegion::NUM_PARTITIONS; i++)
        {
            LLSpatialPartition* part = region->getSpatialPartition(i);
            if (part)
            {
                part->restoreGL();
        }
        }
    }
}

bool LLPipeline::shadersLoaded()
{
    return (assertInitialized() && mShadersLoaded);
}

bool LLPipeline::canUseWindLightShaders() const
{
    return true;
}

bool LLPipeline::canUseAntiAliasing() const
{
    return true;
}

void LLPipeline::unloadShaders()
{
    LLViewerShaderMgr::instance()->unloadShaders();
    mShadersLoaded = false;
}

void LLPipeline::assertInitializedDoError()
{
    LL_ERRS() << "LLPipeline used when uninitialized." << LL_ENDL;
}

//============================================================================

void LLPipeline::enableShadows(const bool enable_shadows)
{
    //should probably do something here to wrangle shadows....
}

class LLOctreeDirtyTexture : public OctreeTraveler
{
public:
    const std::set<LLViewerFetchedTexture*>& mTextures;

    LLOctreeDirtyTexture(const std::set<LLViewerFetchedTexture*>& textures) : mTextures(textures) { }

    virtual void visit(const OctreeNode* node)
    {
        LLSpatialGroup* group = (LLSpatialGroup*) node->getListener(0);

        if (!group->hasState(LLSpatialGroup::GEOM_DIRTY) && !group->isEmpty())
        {
            for (LLSpatialGroup::draw_map_t::iterator i = group->mDrawMap.begin(); i != group->mDrawMap.end(); ++i)
            {
                for (LLSpatialGroup::drawmap_elem_t::iterator j = i->second.begin(); j != i->second.end(); ++j)
                {
                    LLDrawInfo* params = *j;
                    LLViewerFetchedTexture* tex = LLViewerTextureManager::staticCastToFetchedTexture(params->mTexture);
                    if (tex && mTextures.find(tex) != mTextures.end())
                    {
                        group->setState(LLSpatialGroup::GEOM_DIRTY);
                    }
                }
            }
        }

        for (LLSpatialGroup::bridge_list_t::iterator i = group->mBridgeList.begin(); i != group->mBridgeList.end(); ++i)
        {
            LLSpatialBridge* bridge = *i;
            traverse(bridge->mOctree);
        }
    }
};

// Called when a texture changes # of channels (causes faces to move to alpha pool)
void LLPipeline::dirtyPoolObjectTextures(const std::set<LLViewerFetchedTexture*>& textures)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    assertInitialized();

    // *TODO: This is inefficient and causes frame spikes; need a better way to do this
    //        Most of the time is spent in dirty.traverse.

    for (pool_set_t::iterator iter = mPools.begin(); iter != mPools.end(); ++iter)
    {
        LLDrawPool *poolp = *iter;
        if (poolp->isFacePool())
        {
            ((LLFacePool*) poolp)->dirtyTextures(textures);
        }
    }

    LLOctreeDirtyTexture dirty(textures);
    for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin();
            iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
    {
        LLViewerRegion* region = *iter;
        for (U32 i = 0; i < LLViewerRegion::NUM_PARTITIONS; i++)
        {
            LLSpatialPartition* part = region->getSpatialPartition(i);
            if (part)
            {
                dirty.traverse(part->mOctree);
            }
        }
    }
}

LLDrawPool *LLPipeline::findPool(const U32 type, LLViewerTexture *tex0)
{
    assertInitialized();

    LLDrawPool *poolp = NULL;
    switch( type )
    {
    case LLDrawPool::POOL_SIMPLE:
        poolp = mSimplePool;
        break;

    case LLDrawPool::POOL_GRASS:
        poolp = mGrassPool;
        break;

    case LLDrawPool::POOL_ALPHA_MASK:
        poolp = mAlphaMaskPool;
        break;

    case LLDrawPool::POOL_FULLBRIGHT_ALPHA_MASK:
        poolp = mFullbrightAlphaMaskPool;
        break;

    case LLDrawPool::POOL_FULLBRIGHT:
        poolp = mFullbrightPool;
        break;

    case LLDrawPool::POOL_GLOW:
        poolp = mGlowPool;
        break;

    case LLDrawPool::POOL_TREE:
        poolp = get_if_there(mTreePools, (uintptr_t)tex0, (LLDrawPool*)0 );
        break;

    case LLDrawPool::POOL_TERRAIN:
        poolp = get_if_there(mTerrainPools, (uintptr_t)tex0, (LLDrawPool*)0 );
        break;

    case LLDrawPool::POOL_BUMP:
        poolp = mBumpPool;
        break;
    case LLDrawPool::POOL_MATERIALS:
        poolp = mMaterialsPool;
        break;
    case LLDrawPool::POOL_ALPHA_PRE_WATER:
        poolp = mAlphaPoolPreWater;
        break;
    case LLDrawPool::POOL_ALPHA_POST_WATER:
        poolp = mAlphaPoolPostWater;
        break;

    case LLDrawPool::POOL_AVATAR:
    case LLDrawPool::POOL_CONTROL_AV:
        break; // Do nothing

    case LLDrawPool::POOL_SKY:
        poolp = mSkyPool;
        break;

    case LLDrawPool::POOL_WATER:
        poolp = mWaterPool;
        break;

    case LLDrawPool::POOL_WL_SKY:
        poolp = mWLSkyPool;
        break;

    case LLDrawPool::POOL_GLTF_PBR:
        poolp = mPBROpaquePool;
        break;
    case LLDrawPool::POOL_GLTF_PBR_ALPHA_MASK:
        poolp = mPBRAlphaMaskPool;
        break;

    case LLDrawPool::POOL_WATEREXCLUSION:
        poolp = mWaterExclusionPool;
        break;

    default:
        llassert(0);
        LL_ERRS() << "Invalid Pool Type in  LLPipeline::findPool() type=" << type << LL_ENDL;
        break;
    }

    return poolp;
}


LLDrawPool *LLPipeline::getPool(const U32 type, LLViewerTexture *tex0)
{
    LLDrawPool *poolp = findPool(type, tex0);
    if (poolp)
    {
        return poolp;
    }

    LLDrawPool *new_poolp = LLDrawPool::createPool(type, tex0);
    addPool( new_poolp );

    return new_poolp;
}


// static
LLDrawPool* LLPipeline::getPoolFromTE(const LLTextureEntry* te, LLViewerTexture* imagep)
{
    U32 type = getPoolTypeFromTE(te, imagep);
    return gPipeline.getPool(type, imagep);
}

//static
U32 LLPipeline::getPoolTypeFromTE(const LLTextureEntry* te, LLViewerTexture* imagep)
{
    if (!te || !imagep)
    {
        return 0;
    }

    LLMaterial* mat = te->getMaterialParams().get();
    LLGLTFMaterial* gltf_mat = te->getGLTFRenderMaterial();

    bool color_alpha = te->getColor().mV[3] < 0.999f;
    bool alpha = color_alpha;
    if (imagep)
    {
        alpha = alpha || (imagep->getComponents() == 4 && imagep->getType() != LLViewerTexture::MEDIA_TEXTURE) || (imagep->getComponents() == 2);
    }

    if (alpha && mat)
    {
        switch (mat->getDiffuseAlphaMode())
        {
            case 1:
                alpha = true; // Material's alpha mode is set to blend.  Toss it into the alpha draw pool.
                break;
            case 0: //alpha mode set to none, never go to alpha pool
            case 3: //alpha mode set to emissive, never go to alpha pool
                alpha = color_alpha;
                break;
            default: //alpha mode set to "mask", go to alpha pool if fullbright
                alpha = color_alpha; // Material's alpha mode is set to none, mask, or emissive.  Toss it into the opaque material draw pool.
                break;
        }
    }

    if (alpha || (gltf_mat && gltf_mat->mAlphaMode == LLGLTFMaterial::ALPHA_MODE_BLEND))
    {
        return LLDrawPool::POOL_ALPHA;
    }
    else if ((te->getBumpmap() || te->getShiny()) && (!mat || mat->getNormalID().isNull()))
    {
        return LLDrawPool::POOL_BUMP;
    }
    else if (gltf_mat)
    {
        return LLDrawPool::POOL_GLTF_PBR;
    }
    else if (mat && !alpha)
    {
        return LLDrawPool::POOL_MATERIALS;
    }
    else
    {
        return LLDrawPool::POOL_SIMPLE;
    }
}


void LLPipeline::addPool(LLDrawPool *new_poolp)
{
    assertInitialized();
    mPools.insert(new_poolp);
    addToQuickLookup( new_poolp );
}

void LLPipeline::allocDrawable(LLViewerObject *vobj)
{
    LLDrawable *drawable = new LLDrawable(vobj);
    vobj->mDrawable = drawable;

    //encompass completely sheared objects by taking
    //the most extreme point possible (<1,1,0.5>)
    drawable->setRadius(LLVector3(1,1,0.5f).scaleVec(vobj->getScale()).length());
    if (vobj->isOrphaned())
    {
        drawable->setState(LLDrawable::FORCE_INVISIBLE);
    }
    drawable->updateXform(true);
}


void LLPipeline::unlinkDrawable(LLDrawable *drawable)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;

    assertInitialized();

    LLPointer<LLDrawable> drawablep = drawable; // make sure this doesn't get deleted before we are done

    // Based on flags, remove the drawable from the queues that it's on.
    if (drawablep->isState(LLDrawable::ON_MOVE_LIST))
    {
        LLDrawable::drawable_vector_t::iterator iter = std::find(mMovedList.begin(), mMovedList.end(), drawablep);
        if (iter != mMovedList.end())
        {
            mMovedList.erase(iter);
        }
    }

    if (drawablep->getSpatialGroup())
    {
        if (!drawablep->getSpatialGroup()->getSpatialPartition()->remove(drawablep, drawablep->getSpatialGroup()))
        {
#ifdef LL_RELEASE_FOR_DOWNLOAD
            LL_WARNS() << "Couldn't remove object from spatial group!" << LL_ENDL;
#else
            LL_ERRS() << "Couldn't remove object from spatial group!" << LL_ENDL;
#endif
        }
    }

    mLights.erase(drawablep);

    for (light_set_t::iterator iter = mNearbyLights.begin();
                iter != mNearbyLights.end(); iter++)
    {
        if (iter->drawable == drawablep)
        {
            mNearbyLights.erase(iter);
            break;
        }
    }

    for (U32 i = 0; i < MAX_SPOT_SHADOWS; ++i)
    {
        if (mShadowSpotLight[i] == drawablep)
        {
            mShadowSpotLight[i] = NULL;
        }

        if (mTargetShadowSpotLight[i] == drawablep)
        {
            mTargetShadowSpotLight[i] = NULL;
        }
    }
}

//static
void LLPipeline::removeMutedAVsLights(LLVOAvatar* muted_avatar)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    light_set_t::iterator iter = gPipeline.mNearbyLights.begin();
    while (iter != gPipeline.mNearbyLights.end())
    {
        const LLViewerObject* vobj = iter->drawable->getVObj();
        if (vobj
            && vobj->getAvatar()
            && vobj->isAttachment()
            && vobj->getAvatar() == muted_avatar)
        {
            gPipeline.mLights.erase(iter->drawable);
            iter = gPipeline.mNearbyLights.erase(iter);
        }
        else
        {
            iter++;
        }
    }
}

U32 LLPipeline::addObject(LLViewerObject *vobj)
{
    if (RenderDelayCreation)
    {
        mCreateQ.push_back(vobj);
    }
    else
    {
        createObject(vobj);
    }

    return 1;
}

void LLPipeline::createObjects(F32 max_dtime)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;

    LLTimer update_timer;

    while (!mCreateQ.empty() && update_timer.getElapsedTimeF32() < max_dtime)
    {
        LLViewerObject* vobj = mCreateQ.front();
        if (!vobj->isDead())
        {
            createObject(vobj);
        }
        mCreateQ.pop_front();
    }

    //for (LLViewerObject::vobj_list_t::iterator iter = mCreateQ.begin(); iter != mCreateQ.end(); ++iter)
    //{
    //  createObject(*iter);
    //}

    //mCreateQ.clear();
}

void LLPipeline::createObject(LLViewerObject* vobj)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    LLDrawable* drawablep = vobj->mDrawable;

    if (!drawablep)
    {
        drawablep = vobj->createDrawable(this);
    }
    else
    {
        LL_ERRS() << "Redundant drawable creation!" << LL_ENDL;
    }

    llassert(drawablep);

    if (vobj->getParent())
    {
        vobj->setDrawableParent(((LLViewerObject*)vobj->getParent())->mDrawable); // LLPipeline::addObject 1
    }
    else
    {
        vobj->setDrawableParent(NULL); // LLPipeline::addObject 2
    }

    markRebuild(drawablep, LLDrawable::REBUILD_ALL);

    if (drawablep->getVOVolume() && RenderAnimateRes)
    {
        // fun animated res
        drawablep->updateXform(true);
        drawablep->clearState(LLDrawable::MOVE_UNDAMPED);
        drawablep->setScale(LLVector3(0,0,0));
        drawablep->makeActive();
    }
}


void LLPipeline::resetFrameStats()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    assertInitialized();

    sCompiles        = 0;
    mNumVisibleFaces = 0;

    if (mOldRenderDebugMask != mRenderDebugMask)
    {
        gObjectList.clearDebugText();
        mOldRenderDebugMask = mRenderDebugMask;
    }
}

//external functions for asynchronous updating
void LLPipeline::updateMoveDampedAsync(LLDrawable* drawablep)
{
    LL_PROFILE_ZONE_SCOPED;
    if (FreezeTime)
    {
        return;
    }
    if (!drawablep)
    {
        LL_ERRS() << "updateMove called with NULL drawablep" << LL_ENDL;
        return;
    }
    if (drawablep->isState(LLDrawable::EARLY_MOVE))
    {
        return;
    }

    assertInitialized();

    // update drawable now
    drawablep->clearState(LLDrawable::MOVE_UNDAMPED); // force to DAMPED
    drawablep->updateMove(); // returns done
    drawablep->setState(LLDrawable::EARLY_MOVE); // flag says we already did an undamped move this frame
    // Put on move list so that EARLY_MOVE gets cleared
    if (!drawablep->isState(LLDrawable::ON_MOVE_LIST))
    {
        mMovedList.push_back(drawablep);
        drawablep->setState(LLDrawable::ON_MOVE_LIST);
    }
}

void LLPipeline::updateMoveNormalAsync(LLDrawable* drawablep)
{
    LL_PROFILE_ZONE_SCOPED;
    if (FreezeTime)
    {
        return;
    }
    if (!drawablep)
    {
        LL_ERRS() << "updateMove called with NULL drawablep" << LL_ENDL;
        return;
    }
    if (drawablep->isState(LLDrawable::EARLY_MOVE))
    {
        return;
    }

    assertInitialized();

    // update drawable now
    drawablep->setState(LLDrawable::MOVE_UNDAMPED); // force to UNDAMPED
    drawablep->updateMove();
    drawablep->setState(LLDrawable::EARLY_MOVE); // flag says we already did an undamped move this frame
    // Put on move list so that EARLY_MOVE gets cleared
    if (!drawablep->isState(LLDrawable::ON_MOVE_LIST))
    {
        mMovedList.push_back(drawablep);
        drawablep->setState(LLDrawable::ON_MOVE_LIST);
    }
}

void LLPipeline::updateMovedList(LLDrawable::drawable_vector_t& moved_list)
{
    LL_PROFILE_ZONE_SCOPED;
    for (LLDrawable::drawable_vector_t::iterator iter = moved_list.begin();
         iter != moved_list.end(); )
    {
        LLDrawable::drawable_vector_t::iterator curiter = iter++;
        LLDrawable *drawablep = *curiter;
        if (!drawablep)
        {
            iter = moved_list.erase(curiter);
            continue;
        }
        bool done = true;
        if (!drawablep->isDead() && (!drawablep->isState(LLDrawable::EARLY_MOVE)))
        {
            done = drawablep->updateMove();
        }
        drawablep->clearState(LLDrawable::EARLY_MOVE | LLDrawable::MOVE_UNDAMPED);
        if (done)
        {
            if (drawablep->isRoot() && !drawablep->isState(LLDrawable::ACTIVE))
            {
                drawablep->makeStatic();
            }
            drawablep->clearState(LLDrawable::ON_MOVE_LIST);
            if (drawablep->isState(LLDrawable::ANIMATED_CHILD))
            { //will likely not receive any future world matrix updates
                // -- this keeps attachments from getting stuck in space and falling off your avatar
                drawablep->clearState(LLDrawable::ANIMATED_CHILD);
                markRebuild(drawablep, LLDrawable::REBUILD_VOLUME);
                if (drawablep->getVObj())
                {
                    drawablep->getVObj()->dirtySpatialGroup();
                }
            }
            iter = moved_list.erase(curiter);
        }
    }
}

void LLPipeline::updateMove()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;

    if (FreezeTime)
    {
        return;
    }

    assertInitialized();

    for (LLDrawable::drawable_set_t::iterator iter = mRetexturedList.begin();
            iter != mRetexturedList.end(); ++iter)
    {
        LLDrawable* drawablep = *iter;
        if (drawablep && !drawablep->isDead())
        {
            drawablep->updateTexture();
        }
    }
    mRetexturedList.clear();

    updateMovedList(mMovedList);

    //balance octrees
    for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin();
        iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
    {
        LLViewerRegion* region = *iter;
        for (U32 i = 0; i < LLViewerRegion::NUM_PARTITIONS; i++)
        {
            LLSpatialPartition* part = region->getSpatialPartition(i);
            if (part)
            {
                part->mOctree->balance();
            }
        }

        //balance the VO Cache tree
        LLVOCachePartition* vo_part = region->getVOCachePartition();
        if(vo_part)
        {
            vo_part->mOctree->balance();
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
// Culling and occlusion testing
/////////////////////////////////////////////////////////////////////////////

//static
F32 LLPipeline::calcPixelArea(LLVector3 center, LLVector3 size, LLCamera &camera)
{
    llassert(!gCubeSnapshot); // shouldn't be doing ANY of this during cube snap shots
    LLVector3 lookAt = center - camera.getOrigin();
    F32 dist = lookAt.length();

    //ramp down distance for nearby objects
    //shrink dist by dist/16.
    if (dist < 16.f)
    {
        dist /= 16.f;
        dist *= dist;
        dist *= 16.f;
    }

    //get area of circle around node
    F32 app_angle = atanf(size.length()/dist);
    F32 radius = app_angle*LLDrawable::sCurPixelAngle;
    return radius*radius * F_PI;
}

//static
F32 LLPipeline::calcPixelArea(const LLVector4a& center, const LLVector4a& size, LLCamera &camera)
{
    LLVector4a origin;
    origin.load3(camera.getOrigin().mV);

    LLVector4a lookAt;
    lookAt.setSub(center, origin);
    F32 dist = lookAt.getLength3().getF32();

    //ramp down distance for nearby objects
    //shrink dist by dist/16.
    if (dist < 16.f)
    {
        dist /= 16.f;
        dist *= dist;
        dist *= 16.f;
    }

    //get area of circle around node
    F32 app_angle = atanf(size.getLength3().getF32() / dist);
    F32 radius = app_angle * LLDrawable::sCurPixelAngle;
    return radius * radius * F_PI;
}

void LLPipeline::grabReferences(LLCullResult& result)
{
    sCull = &result;
}

void LLPipeline::clearReferences()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    sCull = NULL;
    mGroupSaveQ1.clear();
}

void check_references(LLSpatialGroup* group, LLDrawable* drawable)
{
    for (LLSpatialGroup::element_iter i = group->getDataBegin(); i != group->getDataEnd(); ++i)
    {
        LLDrawable* drawablep = (LLDrawable*)(*i)->getDrawable();
        if (drawable == drawablep)
        {
            LL_ERRS() << "LLDrawable deleted while actively reference by LLPipeline." << LL_ENDL;
        }
    }
}

void check_references(LLDrawable* drawable, LLFace* face)
{
    for (S32 i = 0; i < drawable->getNumFaces(); ++i)
    {
        if (drawable->getFace(i) == face)
        {
            LL_ERRS() << "LLFace deleted while actively referenced by LLPipeline." << LL_ENDL;
        }
    }
}

void check_references(LLSpatialGroup* group, LLFace* face)
{
    for (LLSpatialGroup::element_iter i = group->getDataBegin(); i != group->getDataEnd(); ++i)
    {
        LLDrawable* drawable = (LLDrawable*)(*i)->getDrawable();
        if(drawable)
        {
        check_references(drawable, face);
    }
}
}

void LLPipeline::checkReferences(LLFace* face)
{
#if 0
    if (sCull)
    {
        for (LLCullResult::sg_iterator iter = sCull->beginVisibleGroups(); iter != sCull->endVisibleGroups(); ++iter)
        {
            LLSpatialGroup* group = *iter;
            check_references(group, face);
        }

        for (LLCullResult::sg_iterator iter = sCull->beginAlphaGroups(); iter != sCull->endAlphaGroups(); ++iter)
        {
            LLSpatialGroup* group = *iter;
            check_references(group, face);
        }

        for (LLCullResult::sg_iterator iter = sCull->beginDrawableGroups(); iter != sCull->endDrawableGroups(); ++iter)
        {
            LLSpatialGroup* group = *iter;
            check_references(group, face);
        }

        for (LLCullResult::drawable_iterator iter = sCull->beginVisibleList(); iter != sCull->endVisibleList(); ++iter)
        {
            LLDrawable* drawable = *iter;
            check_references(drawable, face);
        }
    }
#endif
}

void LLPipeline::checkReferences(LLDrawable* drawable)
{
#if 0
    if (sCull)
    {
        for (LLCullResult::sg_iterator iter = sCull->beginVisibleGroups(); iter != sCull->endVisibleGroups(); ++iter)
        {
            LLSpatialGroup* group = *iter;
            check_references(group, drawable);
        }

        for (LLCullResult::sg_iterator iter = sCull->beginAlphaGroups(); iter != sCull->endAlphaGroups(); ++iter)
        {
            LLSpatialGroup* group = *iter;
            check_references(group, drawable);
        }

        for (LLCullResult::sg_iterator iter = sCull->beginDrawableGroups(); iter != sCull->endDrawableGroups(); ++iter)
        {
            LLSpatialGroup* group = *iter;
            check_references(group, drawable);
        }

        for (LLCullResult::drawable_iterator iter = sCull->beginVisibleList(); iter != sCull->endVisibleList(); ++iter)
        {
            if (drawable == *iter)
            {
                LL_ERRS() << "LLDrawable deleted while actively referenced by LLPipeline." << LL_ENDL;
            }
        }
    }
#endif
}

void check_references(LLSpatialGroup* group, LLDrawInfo* draw_info)
{
    for (LLSpatialGroup::draw_map_t::iterator i = group->mDrawMap.begin(); i != group->mDrawMap.end(); ++i)
    {
        LLSpatialGroup::drawmap_elem_t& draw_vec = i->second;
        for (LLSpatialGroup::drawmap_elem_t::iterator j = draw_vec.begin(); j != draw_vec.end(); ++j)
        {
            LLDrawInfo* params = *j;
            if (params == draw_info)
            {
                LL_ERRS() << "LLDrawInfo deleted while actively referenced by LLPipeline." << LL_ENDL;
            }
        }
    }
}


void LLPipeline::checkReferences(LLDrawInfo* draw_info)
{
#if 0
    if (sCull)
    {
        for (LLCullResult::sg_iterator iter = sCull->beginVisibleGroups(); iter != sCull->endVisibleGroups(); ++iter)
        {
            LLSpatialGroup* group = *iter;
            check_references(group, draw_info);
        }

        for (LLCullResult::sg_iterator iter = sCull->beginAlphaGroups(); iter != sCull->endAlphaGroups(); ++iter)
        {
            LLSpatialGroup* group = *iter;
            check_references(group, draw_info);
        }

        for (LLCullResult::sg_iterator iter = sCull->beginDrawableGroups(); iter != sCull->endDrawableGroups(); ++iter)
        {
            LLSpatialGroup* group = *iter;
            check_references(group, draw_info);
        }
    }
#endif
}

void LLPipeline::checkReferences(LLSpatialGroup* group)
{
#if CHECK_PIPELINE_REFERENCES
    if (sCull)
    {
        for (LLCullResult::sg_iterator iter = sCull->beginVisibleGroups(); iter != sCull->endVisibleGroups(); ++iter)
        {
            if (group == *iter)
            {
                LL_ERRS() << "LLSpatialGroup deleted while actively referenced by LLPipeline." << LL_ENDL;
            }
        }

        for (LLCullResult::sg_iterator iter = sCull->beginAlphaGroups(); iter != sCull->endAlphaGroups(); ++iter)
        {
            if (group == *iter)
            {
                LL_ERRS() << "LLSpatialGroup deleted while actively referenced by LLPipeline." << LL_ENDL;
            }
        }

        for (LLCullResult::sg_iterator iter = sCull->beginDrawableGroups(); iter != sCull->endDrawableGroups(); ++iter)
        {
            if (group == *iter)
            {
                LL_ERRS() << "LLSpatialGroup deleted while actively referenced by LLPipeline." << LL_ENDL;
            }
        }
    }
#endif
}


bool LLPipeline::visibleObjectsInFrustum(LLCamera& camera)
{
    for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin();
            iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
    {
        LLViewerRegion* region = *iter;

        for (U32 i = 0; i < LLViewerRegion::NUM_PARTITIONS; i++)
        {
            LLSpatialPartition* part = region->getSpatialPartition(i);
            if (part)
            {
                if (hasRenderType(part->mDrawableType))
                {
                    if (part->visibleObjectsInFrustum(camera))
                    {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

bool LLPipeline::getVisibleExtents(LLCamera& camera, LLVector3& min, LLVector3& max)
{
    const F32 X = 65536.f;

    min = LLVector3(X,X,X);
    max = LLVector3(-X,-X,-X);

    LLViewerCamera::eCameraID saved_camera_id = LLViewerCamera::sCurCameraID;
    LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;

    bool res = true;

    for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin();
            iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
    {
        LLViewerRegion* region = *iter;

        for (U32 i = 0; i < LLViewerRegion::NUM_PARTITIONS; i++)
        {
            LLSpatialPartition* part = region->getSpatialPartition(i);
            if (part)
            {
                if (hasRenderType(part->mDrawableType))
                {
                    if (!part->getVisibleExtents(camera, min, max))
                    {
                        res = false;
                    }
                }
            }
        }
    }

    LLViewerCamera::sCurCameraID = saved_camera_id;
    return res;
}

// static
bool LLPipeline::isWaterClip()
{
    // We always pretend that we're not clipping water when rendering mirrors.
    return (gPipeline.mHeroProbeManager.isMirrorPass()) ? false : (!sRenderTransparentWater || gCubeSnapshot) && !sRenderingHUDs;
}

void LLPipeline::updateCull(LLCamera& camera, LLCullResult& result)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE; //LL_RECORD_BLOCK_TIME(FTM_CULL);
    LL_PROFILE_GPU_ZONE("updateCull"); // should always be zero GPU time, but drop a timer to flush stuff out

    bool water_clip = isWaterClip();

    LLPlane prism_clip_plane;
    const bool prism_surface_clip =
        sPrismLensRender &&
        LLPrismLens::getActiveClipPlane(prism_clip_plane);

    if (prism_surface_clip)
    {
        // Surface Lens uses its destination as an optical aperture, so retain
        // the caller-installed cull plane. getActiveClipPlane() is the fragment
        // predicate (keeps >= 0), while LLCamera culling keeps the opposite sign;
        // renderAuxiliaryView() has already installed that required negation.
        // Camera Feed reports no Prism plane and follows water clipping below.
    }
    else if (water_clip)
    {

        LLVector3 pnorm;

        // Prism's scoped override is the source-eye region height. The same
        // value also feeds alpha grouping, water shaders, and fog uniforms.
        const F32 water_height = getRenderWaterHeight();

        if (sUnderWaterRender)
        {
            //camera is below water, cull above water
            pnorm.setVec(0, 0, 1);
        }
        else
        {
            //camera is above water, cull below water
            pnorm = LLVector3(0, 0, -1);
        }

        LLPlane plane;
        plane.setVec(LLVector3(0, 0, water_height), pnorm);

        camera.setUserClipPlane(plane);
    }
    else
    {
        camera.disableUserClipPlane();
    }

    grabReferences(result);

    sCull->clear();

    for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin();
            iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
    {
        LLViewerRegion* region = *iter;

        for (U32 i = 0; i < LLViewerRegion::NUM_PARTITIONS; i++)
        {
            LLSpatialPartition* part = region->getSpatialPartition(i);
            if (part)
            {
                if (hasRenderType(part->mDrawableType))
                {
                    part->cull(camera);
                }
            }
        }

        //scan the VO Cache tree
        LLVOCachePartition* vo_part = region->getVOCachePartition();
        if(vo_part)
        {
            vo_part->cull(camera, sUseOcclusion > 0);
        }
    }

    if (hasRenderType(LLPipeline::RENDER_TYPE_SKY) &&
        gSky.mVOSkyp.notNull() &&
        gSky.mVOSkyp->mDrawable.notNull())
    {
        gSky.mVOSkyp->mDrawable->setVisible(camera);
        sCull->pushDrawable(gSky.mVOSkyp->mDrawable);
        gSky.updateCull();
        stop_glerror();
    }

    if (hasRenderType(LLPipeline::RENDER_TYPE_WL_SKY) &&
        gPipeline.canUseWindLightShaders() &&
        gSky.mVOWLSkyp.notNull() &&
        gSky.mVOWLSkyp->mDrawable.notNull())
    {
        gSky.mVOWLSkyp->mDrawable->setVisible(camera);
        sCull->pushDrawable(gSky.mVOWLSkyp->mDrawable);
    }
}

void LLPipeline::markNotCulled(LLSpatialGroup* group, LLCamera& camera)
{
    if (group->isEmpty())
    {
        return;
    }

    group->setVisible();

    if (LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD && !gCubeSnapshot)
    {
        group->updateDistance(camera);
    }

    assertInitialized();

    if (!group->getSpatialPartition()->mRenderByGroup)
    { //render by drawable
        sCull->pushDrawableGroup(group);
    }
    else
    {   //render by group
        sCull->pushVisibleGroup(group);
    }

    if (group->needsUpdate() ||
        group->getVisible(LLViewerCamera::sCurCameraID) < LLDrawable::getCurrentFrame() - 1)
    {
        // include this group in occlusion groups, not because it is an occluder, but because we want to run
        // an occlusion query to find out if it's an occluder
        markOccluder(group);
    }
    mNumVisibleNodes++;
}

void LLPipeline::markOccluder(LLSpatialGroup* group)
{
    if (sUseOcclusion > 1 && group && !group->isOcclusionState(LLSpatialGroup::ACTIVE_OCCLUSION))
    {
        LLSpatialGroup* parent = group->getParent();

        if (!parent || !parent->isOcclusionState(LLSpatialGroup::OCCLUDED))
        { //only mark top most occluders as active occlusion
            sCull->pushOcclusionGroup(group);
            group->setOcclusionState(LLSpatialGroup::ACTIVE_OCCLUSION);

            if (parent &&
                !parent->isOcclusionState(LLSpatialGroup::ACTIVE_OCCLUSION) &&
                parent->getElementCount() == 0 &&
                parent->needsUpdate())
            {
                sCull->pushOcclusionGroup(group);
                parent->setOcclusionState(LLSpatialGroup::ACTIVE_OCCLUSION);
            }
        }
    }
}

void LLPipeline::doOcclusion(LLCamera& camera)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    LL_PROFILE_GPU_ZONE("doOcclusion");
    llassert(!gCubeSnapshot);

    if (sReflectionProbesEnabled && sUseOcclusion > 1 && !LLPipeline::sShadowRender && !gCubeSnapshot)
    {
        gGL.setColorMask(false, false);
        LLGLDepthTest depth(GL_TRUE, GL_FALSE);
        LLGLDisable cull(GL_CULL_FACE);

        gOcclusionCubeProgram.bind();

        if (mCubeVB.isNull())
        { //cube VB will be used for issuing occlusion queries
            mCubeVB = ll_create_cube_vb(LLVertexBuffer::MAP_VERTEX);
        }
        mCubeVB->setBuffer();

        mReflectionMapManager.doOcclusion();
        mHeroProbeManager.doOcclusion();
        gOcclusionCubeProgram.unbind();

        gGL.setColorMask(true, true);
    }

    if (LLPipeline::sUseOcclusion > 1 &&
        (sCull->hasOcclusionGroups() || LLVOCachePartition::sNeedsOcclusionCheck))
    {
        LLVertexBuffer::unbind();

        gGL.setColorMask(false, false);

        LLGLDisable blend(GL_BLEND);
        gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
        LLGLDepthTest depth(GL_TRUE, GL_FALSE);

        LLGLDisable cull(GL_CULL_FACE);

        gOcclusionCubeProgram.bind();

        if (mCubeVB.isNull())
        { //cube VB will be used for issuing occlusion queries
            mCubeVB = ll_create_cube_vb(LLVertexBuffer::MAP_VERTEX);
        }
        mCubeVB->setBuffer();

        for (LLCullResult::sg_iterator iter = sCull->beginOcclusionGroups(); iter != sCull->endOcclusionGroups(); ++iter)
        {
            LLSpatialGroup* group = *iter;
            if (!group->isDead())
            {
                group->doOcclusion(&camera);
                group->clearOcclusionState(LLSpatialGroup::ACTIVE_OCCLUSION);
            }
        }

        //apply occlusion culling to object cache tree
        for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin();
            iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
        {
            LLVOCachePartition* vo_part = (*iter)->getVOCachePartition();
            if(vo_part)
            {
                vo_part->processOccluders(&camera);
            }
        }

        gGL.setColorMask(true, true);
    }
}

bool LLPipeline::updateDrawableGeom(LLDrawable* drawablep)
{
    bool update_complete = drawablep->updateGeometry();
    if (update_complete && assertInitialized())
    {
        drawablep->setState(LLDrawable::BUILT);
    }
    return update_complete;
}

void LLPipeline::updateGL()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    {
        while (!LLGLUpdate::sGLQ.empty())
        {
            LLGLUpdate* glu = LLGLUpdate::sGLQ.front();
            glu->updateGL();
            glu->mInQ = false;
            LLGLUpdate::sGLQ.pop_front();
        }
    }
}

void LLPipeline::clearRebuildGroups()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    LLSpatialGroup::sg_vector_t hudGroups;

    mGroupQ1Locked = true;
    // Iterate through all drawables on the priority build queue,
    for (LLSpatialGroup::sg_vector_t::iterator iter = mGroupQ1.begin();
         iter != mGroupQ1.end(); ++iter)
    {
        LLSpatialGroup* group = *iter;

        if (!group || group->isDead())
        {
            continue;
        }
        // If the group contains HUD objects, save the group
        if (group->isHUDGroup())
        {
            hudGroups.push_back(group);
        }
        // Else, no HUD objects so clear the build state
        else
        {
            group->clearState(LLSpatialGroup::IN_BUILD_Q1);
        }
    }

    // Clear the group
    mGroupQ1.clear();

    // Copy the saved HUD groups back in
    mGroupQ1.assign(hudGroups.begin(), hudGroups.end());
    mGroupQ1Locked = false;
}

void LLPipeline::clearRebuildDrawables()
{
    // Clear all drawables on the priority build queue,
    for (LLDrawable::drawable_list_t::iterator iter = mBuildQ1.begin();
         iter != mBuildQ1.end(); ++iter)
    {
        LLDrawable* drawablep = *iter;
        if (drawablep && !drawablep->isDead())
        {
            drawablep->clearState(LLDrawable::IN_REBUILD_Q);
        }
    }
    mBuildQ1.clear();

    //clear all moving bridges
    for (LLDrawable::drawable_vector_t::iterator iter = mMovedBridge.begin();
         iter != mMovedBridge.end(); ++iter)
    {
        LLDrawable *drawablep = *iter;
        drawablep->clearState(LLDrawable::EARLY_MOVE | LLDrawable::MOVE_UNDAMPED | LLDrawable::ON_MOVE_LIST | LLDrawable::ANIMATED_CHILD);
    }
    mMovedBridge.clear();

    //clear all moving drawables
    for (LLDrawable::drawable_vector_t::iterator iter = mMovedList.begin();
         iter != mMovedList.end(); ++iter)
    {
        LLDrawable *drawablep = *iter;
        drawablep->clearState(LLDrawable::EARLY_MOVE | LLDrawable::MOVE_UNDAMPED | LLDrawable::ON_MOVE_LIST | LLDrawable::ANIMATED_CHILD);
    }
    mMovedList.clear();

    for (LLDrawable::drawable_vector_t::iterator iter = mShiftList.begin();
        iter != mShiftList.end(); ++iter)
    {
        LLDrawable *drawablep = *iter;
        drawablep->clearState(LLDrawable::EARLY_MOVE | LLDrawable::MOVE_UNDAMPED | LLDrawable::ON_MOVE_LIST | LLDrawable::ANIMATED_CHILD | LLDrawable::ON_SHIFT_LIST);
    }
    mShiftList.clear();
}

void LLPipeline::rebuildPriorityGroups()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    LL_PROFILE_GPU_ZONE("rebuildPriorityGroups");

    LLTimer update_timer;
    assertInitialized();

    gMeshRepo.notifyLoadedMeshes();

    mGroupQ1Locked = true;
    // Iterate through all drawables on the priority build queue,
    for (LLSpatialGroup::sg_vector_t::iterator iter = mGroupQ1.begin();
         iter != mGroupQ1.end(); ++iter)
    {
        LLSpatialGroup* group = *iter;
        group->rebuildGeom();
        group->clearState(LLSpatialGroup::IN_BUILD_Q1);
    }

    mGroupSaveQ1 = std::move(mGroupQ1);
    mGroupQ1.clear();
    mGroupQ1Locked = false;

}

void LLPipeline::updateGeom(F32 max_dtime)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;

    if (gCubeSnapshot)
    {
        return;
    }

    assertInitialized();

    // notify various object types to reset internal cost metrics, etc.
    // for now, only LLVOVolume does this to throttle LOD changes
    LLVOVolume::preUpdateGeom();

    // Iterate through all drawables on the priority build queue,
    for (LLDrawable::drawable_list_t::iterator iter = mBuildQ1.begin();
         iter != mBuildQ1.end();)
    {
        LLDrawable::drawable_list_t::iterator curiter = iter++;
        LLDrawable* drawablep = *curiter;
        if (drawablep && !drawablep->isDead())
        {
            if (drawablep->isUnload())
            {
                drawablep->unload();
                drawablep->clearState(LLDrawable::FOR_UNLOAD);
            }

            if (updateDrawableGeom(drawablep))
            {
                drawablep->clearState(LLDrawable::IN_REBUILD_Q);
                mBuildQ1.erase(curiter);
            }
        }
        else
        {
            mBuildQ1.erase(curiter);
        }
    }

    updateMovedList(mMovedBridge);
}

void LLPipeline::markVisible(LLDrawable *drawablep, LLCamera& camera)
{
    if(drawablep && !drawablep->isDead())
    {
        if (drawablep->isSpatialBridge())
        {
            const LLDrawable* root = ((LLSpatialBridge*) drawablep)->mDrawable;
            llassert(root); // trying to catch a bad assumption

            if (root && //  // this test may not be needed, see above
                    root->getVObj()->isAttachment())
            {
                LLDrawable* rootparent = root->getParent();
                if (rootparent) // this IS sometimes NULL
                {
                    LLViewerObject *vobj = rootparent->getVObj();
                    llassert(vobj); // trying to catch a bad assumption
                    if (vobj) // this test may not be needed, see above
                    {
                        LLVOAvatar* av = vobj->asAvatar();
                        if (av &&
                            ((!sImpostorRender && av->isImpostor()) //ignore impostor flag during impostor pass
                             || av->isInMuteList()
                             || (LLVOAvatar::AOA_JELLYDOLL == av->getOverallAppearance() && !av->needsImpostorUpdate()) ))
                        {
                            return;
                        }
                    }
                }
            }
            sCull->pushBridge((LLSpatialBridge*) drawablep);
        }
        else
        {

            sCull->pushDrawable(drawablep);
        }

        drawablep->setVisible(camera);
    }
}

void LLPipeline::markMoved(LLDrawable *drawablep, bool damped_motion)
{
    if (!drawablep)
    {
        //LL_ERRS() << "Sending null drawable to moved list!" << LL_ENDL;
        return;
    }

    if (drawablep->isDead())
    {
        LL_WARNS() << "Marking NULL or dead drawable moved!" << LL_ENDL;
        return;
    }

    if (drawablep->getParent())
    {
        //ensure that parent drawables are moved first
        markMoved(drawablep->getParent(), damped_motion);
    }

    assertInitialized();

    if (!drawablep->isState(LLDrawable::ON_MOVE_LIST))
    {
        if (drawablep->isSpatialBridge())
        {
            mMovedBridge.push_back(drawablep);
        }
        else
        {
            mMovedList.push_back(drawablep);
        }
        drawablep->setState(LLDrawable::ON_MOVE_LIST);
    }
    if (! damped_motion)
    {
        drawablep->setState(LLDrawable::MOVE_UNDAMPED); // UNDAMPED trumps DAMPED
    }
    else if (drawablep->isState(LLDrawable::MOVE_UNDAMPED))
    {
        drawablep->clearState(LLDrawable::MOVE_UNDAMPED);
    }
}

void LLPipeline::markShift(LLDrawable *drawablep)
{
    if (!drawablep || drawablep->isDead() || !drawablep->getVObj())
    {
        return;
    }

    assertInitialized();

    if (!drawablep->isState(LLDrawable::ON_SHIFT_LIST))
    {
        drawablep->getVObj()->setChanged(LLXform::SHIFTED | LLXform::SILHOUETTE);
        if (drawablep->getParent())
        {
            markShift(drawablep->getParent());
        }
        mShiftList.push_back(drawablep);
        drawablep->setState(LLDrawable::ON_SHIFT_LIST);
    }
}

void LLPipeline::shiftObjects(const LLVector3 &offset)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    assertInitialized();

    glClear(GL_DEPTH_BUFFER_BIT);
    gDepthDirty = true;

    LLVector4a offseta;
    offseta.load3(offset.mV);

    for (LLDrawable::drawable_vector_t::iterator iter = mShiftList.begin();
            iter != mShiftList.end(); iter++)
    {
        LLDrawable *drawablep = *iter;
        if (drawablep->isDead() || !drawablep->getVObj())
        {
            continue;
        }
        drawablep->shiftPos(offseta);
        drawablep->clearState(LLDrawable::ON_SHIFT_LIST);
    }
    mShiftList.resize(0);

    for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin();
            iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
    {
        LLViewerRegion* region = *iter;
        for (U32 i = 0; i < LLViewerRegion::NUM_PARTITIONS; i++)
        {
            LLSpatialPartition* part = region->getSpatialPartition(i);
            if (part)
            {
                part->shift(offseta);
            }
        }
    }

    mReflectionMapManager.shift(offseta);

    LLHUDText::shiftAll(offset);
    LLHUDNameTag::shiftAll(offset);

    display_update_camera();
}

void LLPipeline::markTextured(LLDrawable *drawablep)
{
    if (drawablep && !drawablep->isDead() && assertInitialized())
    {
        mRetexturedList.insert(drawablep);
    }
}

void LLPipeline::markGLRebuild(LLGLUpdate* glu)
{
    if (glu && !glu->mInQ)
    {
        LLGLUpdate::sGLQ.push_back(glu);
        glu->mInQ = true;
    }
}

void LLPipeline::markPartitionMove(LLDrawable* drawable)
{
    if (!drawable->isState(LLDrawable::PARTITION_MOVE) &&
        !drawable->getPositionGroup().equals3(LLVector4a::getZero()))
    {
        drawable->setState(LLDrawable::PARTITION_MOVE);
        mPartitionQ.push_back(drawable);
    }
}

void LLPipeline::processPartitionQ()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    for (LLDrawable::drawable_list_t::iterator iter = mPartitionQ.begin(); iter != mPartitionQ.end(); ++iter)
    {
        LLDrawable* drawable = *iter;
        if (!drawable->isDead())
        {
            drawable->updateBinRadius();
            drawable->movePartition();
        }
        drawable->clearState(LLDrawable::PARTITION_MOVE);
    }

    mPartitionQ.clear();
}

void LLPipeline::markMeshDirty(LLSpatialGroup* group)
{
    mMeshDirtyGroup.push_back(group);
}

void LLPipeline::markRebuild(LLSpatialGroup* group)
{
    if (group && !group->isDead() && group->getSpatialPartition())
    {
        if (!group->hasState(LLSpatialGroup::IN_BUILD_Q1))
        {
            llassert_always(!mGroupQ1Locked);

            mGroupQ1.push_back(group);
            group->setState(LLSpatialGroup::IN_BUILD_Q1);
        }
    }
}

void LLPipeline::markRebuild(LLDrawable *drawablep, LLDrawable::EDrawableFlags flag)
{
    if (drawablep && !drawablep->isDead() && assertInitialized())
    {
        if (!drawablep->isState(LLDrawable::IN_REBUILD_Q))
        {
            mBuildQ1.push_back(drawablep);
            drawablep->setState(LLDrawable::IN_REBUILD_Q); // mark drawable as being in priority queue
        }

        if (flag & (LLDrawable::REBUILD_VOLUME | LLDrawable::REBUILD_POSITION))
        {
            drawablep->getVObj()->setChanged(LLXform::SILHOUETTE);
        }
        drawablep->setState(flag);
    }
}

void LLPipeline::stateSort(LLCamera& camera, LLCullResult &result)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    LL_PROFILE_GPU_ZONE("stateSort");

    if (hasAnyRenderType(LLPipeline::RENDER_TYPE_AVATAR,
                      LLPipeline::RENDER_TYPE_CONTROL_AV,
                      LLPipeline::RENDER_TYPE_TERRAIN,
                      LLPipeline::RENDER_TYPE_TREE,
                      LLPipeline::RENDER_TYPE_SKY,
                      LLPipeline::RENDER_TYPE_VOIDWATER,
                      LLPipeline::RENDER_TYPE_WATER,
                      LLPipeline::END_RENDER_TYPES))
    {
        //clear faces from face pools
        gPipeline.resetDrawOrders();
    }

    //LLVertexBuffer::unbind();

    grabReferences(result);
    for (LLCullResult::sg_iterator iter = sCull->beginDrawableGroups(); iter != sCull->endDrawableGroups(); ++iter)
    {
        LLSpatialGroup* group = *iter;
        if (group->isDead())
        {
            continue;
        }
        group->checkOcclusion();
        if (sUseOcclusion > 1 && group->isOcclusionState(LLSpatialGroup::OCCLUDED))
        {
            markOccluder(group);
        }
        else
        {
            group->setVisible();
            for (LLSpatialGroup::element_iter i = group->getDataBegin(); i != group->getDataEnd(); ++i)
            {
                LLDrawable* drawablep = (LLDrawable*)(*i)->getDrawable();
                markVisible(drawablep, camera);
            }

            { //rebuild mesh as soon as we know it's visible
                group->rebuildMesh();
            }
        }
    }

    if (LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD && !gCubeSnapshot)
    {
        LLSpatialGroup* last_group = NULL;
        bool fov_changed = LLViewerCamera::getInstance()->isDefaultFOVChanged();
        for (LLCullResult::bridge_iterator i = sCull->beginVisibleBridge(); i != sCull->endVisibleBridge(); ++i)
        {
            LLCullResult::bridge_iterator cur_iter = i;
            LLSpatialBridge* bridge = *cur_iter;
            LLSpatialGroup* group = bridge->getSpatialGroup();

            if (last_group == NULL)
            {
                last_group = group;
            }

            if (!bridge->isDead() && group && !group->isOcclusionState(LLSpatialGroup::OCCLUDED))
            {
                stateSort(bridge, camera, fov_changed);
            }

            if (LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD &&
                last_group != group && last_group->changeLOD())
            {
                last_group->mLastUpdateDistance = last_group->mDistance;
            }

            last_group = group;
        }

        if (LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD &&
            last_group && last_group->changeLOD())
        {
            last_group->mLastUpdateDistance = last_group->mDistance;
        }
    }

    for (LLCullResult::sg_iterator iter = sCull->beginVisibleGroups(); iter != sCull->endVisibleGroups(); ++iter)
    {
        LLSpatialGroup* group = *iter;
        if (group->isDead())
        {
            continue;
        }
        group->checkOcclusion();
        if (sUseOcclusion > 1 && group->isOcclusionState(LLSpatialGroup::OCCLUDED))
        {
            markOccluder(group);
        }
        else
        {
            group->setVisible();
            stateSort(group, camera);

            { //rebuild mesh as soon as we know it's visible
                group->rebuildMesh();
            }
        }
    }

    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DRAWABLE("stateSort"); // LL_RECORD_BLOCK_TIME(FTM_STATESORT_DRAWABLE);
        for (LLCullResult::drawable_iterator iter = sCull->beginVisibleList();
             iter != sCull->endVisibleList(); ++iter)
        {
            LLDrawable *drawablep = *iter;
            if (!drawablep->isDead())
            {
                stateSort(drawablep, camera);
            }
        }
    }

    postSort(camera);
}

void LLPipeline::stateSort(LLSpatialGroup* group, LLCamera& camera)
{
    if (group->changeLOD())
    {
        for (LLSpatialGroup::element_iter i = group->getDataBegin(); i != group->getDataEnd(); ++i)
        {
            LLDrawable* drawablep = (LLDrawable*)(*i)->getDrawable();
            stateSort(drawablep, camera);
        }

        if (LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD && !gCubeSnapshot)
        { //avoid redundant stateSort calls
            group->mLastUpdateDistance = group->mDistance;
        }
    }
}

void LLPipeline::stateSort(LLSpatialBridge* bridge, LLCamera& camera, bool fov_changed)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    if (bridge->getSpatialGroup()->changeLOD() || fov_changed)
    {
        bool force_update = false;
        bridge->updateDistance(camera, force_update);
    }
}

void LLPipeline::stateSort(LLDrawable* drawablep, LLCamera& camera)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    if (!drawablep
        || drawablep->isDead()
        || !hasRenderType(drawablep->getRenderType()))
    {
        return;
    }

    // SL-11353
    // ignore our own geo when rendering spotlight shadowmaps...
    //
    if (RenderSpotLight && drawablep == RenderSpotLight)
    {
        return;
    }

    if (LLSelectMgr::getInstance()->mHideSelectedObjects)
    {
//      if (drawablep->getVObj().notNull() &&
//          drawablep->getVObj()->isSelected())
// [RLVa:KB] - Checked: 2010-09-28 (RLVa-1.2.1f) | Modified: RLVa-1.2.1f
        const LLViewerObject* pObj = drawablep->getVObj();
        if ( (pObj) && (pObj->isSelected()) &&
             ( (!RlvActions::isRlvEnabled()) ||
               ( ((!pObj->isHUDAttachment()) || (!gRlvAttachmentLocks.isLockedAttachment(pObj->getRootEdit()))) &&
                 (RlvActions::canEdit(pObj)) ) ) )
// [/RVLa:KB]
        {
            return;
        }
    }

    if (drawablep->isAvatar())
    { //don't draw avatars beyond render distance or if we don't have a spatial group.
        if ((drawablep->getSpatialGroup() == NULL) ||
            (drawablep->getSpatialGroup()->mDistance > LLVOAvatar::sRenderDistance))
        {
            return;
        }

        LLVOAvatar* avatarp = (LLVOAvatar*) drawablep->getVObj().get();
        if (!avatarp->isVisible())
        {
            return;
        }
    }

    assertInitialized();

    if (hasRenderType(drawablep->mRenderType))
    {
        if (!drawablep->isState(LLDrawable::INVISIBLE|LLDrawable::FORCE_INVISIBLE))
        {
            drawablep->setVisible(camera, NULL, false);
        }
    }

    if (LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD && !gCubeSnapshot)
    {
        //if (drawablep->isVisible()) isVisible() check here is redundant, if it wasn't visible, it wouldn't be here
        {
            if (!drawablep->isActive())
            {
                bool force_update = false;
                drawablep->updateDistance(camera, force_update);
            }
            else if (drawablep->isAvatar())
            {
                bool force_update = false;
                drawablep->updateDistance(camera, force_update); // calls vobj->updateLOD() which calls LLVOAvatar::updateVisibility()
            }
        }
    }

    if (!drawablep->getVOVolume())
    {
        for (LLDrawable::face_list_t::iterator iter = drawablep->mFaces.begin();
                iter != drawablep->mFaces.end(); iter++)
        {
            LLFace* facep = *iter;

            if (facep->hasGeometry())
            {
                if (facep->getPool())
                {
                    facep->getPool()->enqueue(facep);
                }
                else
                {
                    break;
                }
            }
        }
    }

    mNumVisibleFaces += drawablep->getNumFaces();
}


void forAllDrawables(LLCullResult::sg_iterator begin,
                     LLCullResult::sg_iterator end,
                     void (*func)(LLDrawable*))
{
    for (LLCullResult::sg_iterator i = begin; i != end; ++i)
    {
        LLSpatialGroup* group = *i;
        if (group->isDead())
        {
            continue;
        }
        for (LLSpatialGroup::element_iter j = group->getDataBegin(); j != group->getDataEnd(); ++j)
        {
            if((*j)->hasDrawable())
            {
                func((LLDrawable*)(*j)->getDrawable());
            }
        }
    }
}

void LLPipeline::forAllVisibleDrawables(void (*func)(LLDrawable*))
{
    forAllDrawables(sCull->beginDrawableGroups(), sCull->endDrawableGroups(), func);
    forAllDrawables(sCull->beginVisibleGroups(), sCull->endVisibleGroups(), func);
}

//function for creating scripted beacons
void renderScriptedBeacons(LLDrawable* drawablep)
{
    LLViewerObject *vobj = drawablep->getVObj();
    if (vobj
        && !vobj->isAvatar()
        && !vobj->getParent()
        && vobj->flagScripted())
    {
        if (gPipeline.sRenderBeacons)
        {
            gObjectList.addDebugBeacon(vobj->getPositionAgent(), "", LLColor4(1.f, 0.f, 0.f, 0.5f), LLColor4(1.f, 1.f, 1.f, 0.5f), LLPipeline::DebugBeaconLineWidth);
        }

        if (gPipeline.sRenderHighlight)
        {
            S32 face_id;
            S32 count = drawablep->getNumFaces();
            for (face_id = 0; face_id < count; face_id++)
            {
                LLFace * facep = drawablep->getFace(face_id);
                if (facep)
                {
                    gPipeline.mHighlightFaces.push_back(facep);
                }
            }
        }
    }
}

void renderScriptedTouchBeacons(LLDrawable *drawablep)
{
    LLViewerObject *vobj = drawablep->getVObj();
    if (vobj && !vobj->isAvatar() && !vobj->getParent() && vobj->flagScripted() && vobj->flagHandleTouch())
    {
        if (gPipeline.sRenderBeacons)
        {
            gObjectList.addDebugBeacon(vobj->getPositionAgent(), "", LLColor4(1.f, 0.f, 0.f, 0.5f), LLColor4(1.f, 1.f, 1.f, 0.5f),
                                       LLPipeline::DebugBeaconLineWidth);
        }

        if (gPipeline.sRenderHighlight)
        {
            S32 face_id;
            S32 count = drawablep->getNumFaces();
            for (face_id = 0; face_id < count; face_id++)
            {
                LLFace *facep = drawablep->getFace(face_id);
                if (facep)
                {
                    gPipeline.mHighlightFaces.push_back(facep);
                }
            }
        }
    }
}

void renderPhysicalBeacons(LLDrawable *drawablep)
{
    LLViewerObject *vobj = drawablep->getVObj();
    if (vobj &&
        !vobj->isAvatar()
        //&& !vobj->getParent()
        && vobj->flagUsePhysics())
    {
        if (gPipeline.sRenderBeacons)
        {
            gObjectList.addDebugBeacon(vobj->getPositionAgent(), "", LLColor4(0.f, 1.f, 0.f, 0.5f), LLColor4(1.f, 1.f, 1.f, 0.5f),
                                       LLPipeline::DebugBeaconLineWidth);
        }

        if (gPipeline.sRenderHighlight)
        {
            S32 face_id;
            S32 count = drawablep->getNumFaces();
            for (face_id = 0; face_id < count; face_id++)
            {
                LLFace *facep = drawablep->getFace(face_id);
                if (facep)
                {
                    gPipeline.mHighlightFaces.push_back(facep);
                }
            }
        }
    }
}

void renderMOAPBeacons(LLDrawable *drawablep)
{
    LLViewerObject *vobj = drawablep->getVObj();

    if (!vobj || vobj->isAvatar())
        return;

    bool beacon  = false;
    U8   tecount = vobj->getNumTEs();
    for (int x = 0; x < tecount; x++)
    {
        if (vobj->getTE(x)->hasMedia())
        {
            beacon = true;
            break;
        }
    }
    if (beacon)
    {
        if (gPipeline.sRenderBeacons)
        {
            gObjectList.addDebugBeacon(vobj->getPositionAgent(), "", LLColor4(1.f, 1.f, 1.f, 0.5f), LLColor4(1.f, 1.f, 1.f, 0.5f),
                                       LLPipeline::DebugBeaconLineWidth);
        }

        if (gPipeline.sRenderHighlight)
        {
            S32 face_id;
            S32 count = drawablep->getNumFaces();
            for (face_id = 0; face_id < count; face_id++)
            {
                LLFace *facep = drawablep->getFace(face_id);
                if (facep)
                {
                    gPipeline.mHighlightFaces.push_back(facep);
                }
            }
        }
    }
}

void renderParticleBeacons(LLDrawable *drawablep)
{
    // Look for attachments, objects, etc.
    LLViewerObject *vobj = drawablep->getVObj();
    if (vobj && vobj->isParticleSource())
    {
        if (gPipeline.sRenderBeacons)
        {
            LLColor4 light_blue(0.5f, 0.5f, 1.f, 0.5f);
            gObjectList.addDebugBeacon(vobj->getPositionAgent(), "", light_blue, LLColor4(1.f, 1.f, 1.f, 0.5f),
                                       LLPipeline::DebugBeaconLineWidth);
        }

        if (gPipeline.sRenderHighlight)
        {
            S32 face_id;
            S32 count = drawablep->getNumFaces();
            for (face_id = 0; face_id < count; face_id++)
            {
                LLFace *facep = drawablep->getFace(face_id);
                if (facep)
                {
                    gPipeline.mHighlightFaces.push_back(facep);
                }
            }
        }
    }
}

void renderSoundHighlights(LLDrawable *drawablep)
{
    // Look for attachments, objects, etc.
    LLViewerObject *vobj = drawablep->getVObj();
    if (vobj && vobj->isAudioSource())
    {
        if (gPipeline.sRenderHighlight)
        {
            S32 face_id;
            S32 count = drawablep->getNumFaces();
            for (face_id = 0; face_id < count; face_id++)
            {
                LLFace *facep = drawablep->getFace(face_id);
                if (facep)
                {
                    gPipeline.mHighlightFaces.push_back(facep);
                }
            }
        }
    }
}

void LLPipeline::postSort(LLCamera &camera)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;

    assertInitialized();

    if (!gCubeSnapshot)
    {
        // rebuild drawable geometry
        for (LLCullResult::sg_iterator i = sCull->beginDrawableGroups(); i != sCull->endDrawableGroups(); ++i)
        {
            LLSpatialGroup *group = *i;
            if (group->isDead())
            {
                continue;
            }
            if (!sUseOcclusion || !group->isOcclusionState(LLSpatialGroup::OCCLUDED))
            {
                group->rebuildGeom();
            }
        }
        // rebuild groups
        sCull->assertDrawMapsEmpty();

        rebuildPriorityGroups();
    }

    // build render map
    for (LLCullResult::sg_iterator i = sCull->beginVisibleGroups(); i != sCull->endVisibleGroups(); ++i)
    {
        LLSpatialGroup *group = *i;

        if (group->isDead())
        {
            continue;
        }

        if ((sUseOcclusion && group->isOcclusionState(LLSpatialGroup::OCCLUDED)) ||
            (RenderAutoHideSurfaceAreaLimit > 0.f &&
             group->mSurfaceArea > RenderAutoHideSurfaceAreaLimit * llmax(group->mObjectBoxSize, 10.f)))
        {
            continue;
        }

        if (group->hasState(LLSpatialGroup::NEW_DRAWINFO) && group->hasState(LLSpatialGroup::GEOM_DIRTY) && !gCubeSnapshot)
        {  // no way this group is going to be drawable without a rebuild
            group->rebuildGeom();
        }

        for (LLSpatialGroup::draw_map_t::iterator j = group->mDrawMap.begin(); j != group->mDrawMap.end(); ++j)
        {
            LLSpatialGroup::drawmap_elem_t &src_vec = j->second;
            if (!hasRenderType(j->first))
            {
                continue;
            }

            for (LLSpatialGroup::drawmap_elem_t::iterator k = src_vec.begin(); k != src_vec.end(); ++k)
            {
                LLDrawInfo *info = *k;

                sCull->pushDrawInfo(j->first, info);
                if (!sShadowRender && !sReflectionRender && !gCubeSnapshot)
                {
                    addTrianglesDrawn(info->mCount);
                }
            }
        }

        if (hasRenderType(LLPipeline::RENDER_TYPE_PASS_ALPHA))
        {
            LLSpatialGroup::draw_map_t::iterator alpha = group->mDrawMap.find(LLRenderPass::PASS_ALPHA);

            if (alpha != group->mDrawMap.end())
            {  // store alpha groups for sorting
                LLSpatialBridge *bridge = group->getSpatialPartition()->asBridge();
                if (LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD && !gCubeSnapshot)
                {
                    if (bridge)
                    {
                        LLCamera trans_camera = bridge->transformCamera(camera);
                        group->updateDistance(trans_camera);
                    }
                    else
                    {
                        group->updateDistance(camera);
                    }
                }

                if (hasRenderType(LLDrawPool::POOL_ALPHA))
                {
                    sCull->pushAlphaGroup(group);
                }
            }

            LLSpatialGroup::draw_map_t::iterator rigged_alpha = group->mDrawMap.find(LLRenderPass::PASS_ALPHA_RIGGED);

            if (rigged_alpha != group->mDrawMap.end())
            {  // store rigged alpha groups for LLDrawPoolAlpha prepass (skip distance update, rigged attachments use depth buffer)
                if (hasRenderType(LLDrawPool::POOL_ALPHA))
                {
                    sCull->pushRiggedAlphaGroup(group);
                }
            }
        }
    }

    // pack vertex buffers for groups that chose to delay their updates
    {
        LL_PROFILE_GPU_ZONE("rebuildMesh");
        for (LLSpatialGroup::sg_vector_t::iterator iter = mMeshDirtyGroup.begin(); iter != mMeshDirtyGroup.end(); ++iter)
        {
            (*iter)->rebuildMesh();
        }
    }

    mMeshDirtyGroup.clear();

    if (!sShadowRender)
    {
        // order alpha groups by distance
        std::sort(sCull->beginAlphaGroups(), sCull->endAlphaGroups(), LLSpatialGroup::CompareDepthGreater());

        // order rigged alpha groups by avatar attachment order
        std::sort(sCull->beginRiggedAlphaGroups(), sCull->endRiggedAlphaGroups(), LLSpatialGroup::CompareRenderOrder());
    }

    // only render if the flag is set. The flag is only set if we are in edit mode or the toggle is set in the menus
    if (LLFloaterReg::instanceVisible("beacons") && !sShadowRender && !gCubeSnapshot)
    {
        if (sRenderScriptedTouchBeacons)
        {
            // Only show the beacon on the root object.
            forAllVisibleDrawables(renderScriptedTouchBeacons);
        }
        else if (sRenderScriptedBeacons)
        {
            // Only show the beacon on the root object.
            forAllVisibleDrawables(renderScriptedBeacons);
        }

        if (sRenderPhysicalBeacons)
        {
            // Only show the beacon on the root object.
            forAllVisibleDrawables(renderPhysicalBeacons);
        }

        if (sRenderMOAPBeacons)
        {
            forAllVisibleDrawables(renderMOAPBeacons);
        }

        if (sRenderParticleBeacons)
        {
            forAllVisibleDrawables(renderParticleBeacons);
        }

        // If god mode, also show audio cues
        if (sRenderSoundBeacons && gAudiop)
        {
            // Walk all sound sources and render out beacons for them. Note, this isn't done in the ForAllVisibleDrawables function, because
            // some are not visible.
            LLAudioEngine::source_map::iterator iter;
            for (iter = gAudiop->mAllSources.begin(); iter != gAudiop->mAllSources.end(); ++iter)
            {
                LLAudioSource *sourcep = iter->second;

                LLVector3d pos_global = sourcep->getPositionGlobal();
                LLVector3  pos        = gAgent.getPosAgentFromGlobal(pos_global);
                if (gPipeline.sRenderBeacons)
                {
                    // pos += LLVector3(0.f, 0.f, 0.2f);
                    gObjectList.addDebugBeacon(pos, "", LLColor4(1.f, 1.f, 0.f, 0.5f), LLColor4(1.f, 1.f, 1.f, 0.5f), DebugBeaconLineWidth);
                }
            }
            // now deal with highlights for all those seeable sound sources
            forAllVisibleDrawables(renderSoundHighlights);
        }
    }

    // If managing your telehub, draw beacons at telehub and currently selected spawnpoint.
    if (LLFloaterTelehub::renderBeacons() && !sShadowRender && !gCubeSnapshot)
    {
        LLFloaterTelehub::addBeacons();
    }

    if (!sShadowRender && !gCubeSnapshot)
    {
        mSelectedFaces.clear();

        bool tex_index_changed = false;
        if (!gNonInteractive)
        {
            LLRender::eTexIndex tex_index = sRenderHighlightTextureChannel;
            setRenderHighlightTextureChannel(gFloaterTools->getPanelFace()->getTextureChannelToEdit());
            tex_index_changed = sRenderHighlightTextureChannel != tex_index;
        }

        // Draw face highlights for selected faces.
        if (LLSelectMgr::getInstance()->getTEMode())
        {
            struct f : public LLSelectedTEFunctor
            {
                virtual bool apply(LLViewerObject *object, S32 te)
                {
                    if (object->mDrawable)
                    {
                        LLFace *facep = object->mDrawable->getFace(te);
                        if (facep)
                        {
                            gPipeline.mSelectedFaces.push_back(facep);
                        }
                    }
                    return true;
                }
            } func;
            LLSelectMgr::getInstance()->getSelection()->applyToTEs(&func);

            if (tex_index_changed)
            {
                // Rebuild geometry for all selected faces with PBR textures
                for (const LLFace* face : gPipeline.mSelectedFaces)
                {
                    if (const LLViewerObject* vobj = face->getViewerObject())
                    {
                        if (const LLTextureEntry* tep = vobj->getTE(face->getTEOffset()))
                        {
                            if (tep->getGLTFRenderMaterial())
                            {
                                gPipeline.markRebuild(face->getDrawable(), LLDrawable::REBUILD_VOLUME);
                            }
                        }
                    }
                }
            }
        }
    }

    LLVertexBuffer::flushBuffers();
    // LLSpatialGroup::sNoDelete = false;
}


void render_hud_elements()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_UI; //LL_RECORD_BLOCK_TIME(FTM_RENDER_UI);
    gPipeline.disableLights();

    LLGLSUIDefault gls_ui;

    //LLGLEnable stencil(GL_STENCIL_TEST);
    //glStencilFunc(GL_ALWAYS, 255, 0xFFFFFFFF);
    //glStencilMask(0xFFFFFFFF);
    //glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    gUIProgram.bind();
    gGL.color4f(1, 1, 1, 1);
    LLGLDepthTest depth(GL_TRUE, GL_FALSE);

    if (!LLPipeline::sReflectionRender && gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))
    {
        gViewerWindow->renderSelections(false, false, false); // For HUD version in render_ui_3d()

        // Draw the tracking overlays
        LLTracker::render3D();

        if (LLWorld::instanceExists())
        {
            // Show the property lines
            LLWorld::getInstance()->renderPropertyLines();
        }
        LLViewerParcelMgr::getInstance()->render();
        LLViewerParcelMgr::getInstance()->renderParcelCollision();
    }
    else if (gForceRenderLandFence)
    {
        // This is only set when not rendering the UI, for parcel snapshots
        LLViewerParcelMgr::getInstance()->render();
    }
    else if (gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_HUD))
    {
        LLHUDText::renderAllHUD();
    }

    gUIProgram.unbind();
}

static inline void bindHighlightProgram(LLGLSLShader& program)
{
    if ((LLViewerShaderMgr::instance()->getShaderLevel(LLViewerShaderMgr::SHADER_INTERFACE) > 0))
    {
        program.bind();
        gGL.diffuseColor4f(1, 1, 1, 0.5f);
    }
}

static inline void unbindHighlightProgram(LLGLSLShader& program)
{
    if (LLViewerShaderMgr::instance()->getShaderLevel(LLViewerShaderMgr::SHADER_INTERFACE) > 0)
    {
        program.unbind();
    }
}

void LLPipeline::renderSelectedFaces(const LLColor4& color)
{
    if (!mFaceSelectImagep)
    {
        mFaceSelectImagep = LLViewerTextureManager::getFetchedTexture(IMG_FACE_SELECT);
    }

    if (mFaceSelectImagep)
    {
        // Make sure the selection image gets downloaded and decoded
        mFaceSelectImagep->addTextureStats((F32)MAX_IMAGE_AREA);

        for (auto facep : mSelectedFaces)
        {
            if (!facep || !facep->getViewerObject())
            {
                LLSelectMgr::getInstance()->clearSelections();
                return;
            }
            if (!facep->getDrawable() || facep->getDrawable()->isDead())
            {
                LL_ERRS() << "Bad face on selection" << LL_ENDL;
                return;
            }

            facep->renderSelected(mFaceSelectImagep, color);
        }
    }
}

void LLPipeline::renderHighlights()
{
    assertInitialized();

    // Draw 3D UI elements here (before we clear the Z buffer in POOL_HUD)
    // Render highlighted faces.
    LLGLSPipelineAlpha gls_pipeline_alpha;
    disableLights();

    // [BDMerge HideUI] Face highlights (white selected-face glow, red beacon
    // highlight) are selection/debug indicators - hide them with the interface.
    if (hasRenderDebugFeatureMask(RENDER_DEBUG_FEATURE_SELECTED) &&
        gViewerWindow && gViewerWindow->getIndicatorsVisible())
    {
        bindHighlightProgram(gHighlightProgram);

        if (sRenderHighlightTextureChannel == LLRender::DIFFUSE_MAP ||
            sRenderHighlightTextureChannel == LLRender::BASECOLOR_MAP ||
            sRenderHighlightTextureChannel == LLRender::METALLIC_ROUGHNESS_MAP ||
            sRenderHighlightTextureChannel == LLRender::GLTF_NORMAL_MAP ||
            sRenderHighlightTextureChannel == LLRender::EMISSIVE_MAP ||
            sRenderHighlightTextureChannel == LLRender::NUM_TEXTURE_CHANNELS)
        {
            static const LLColor4 highlight_selected_color(1.f, 1.f, 1.f, 0.5f);
            renderSelectedFaces(highlight_selected_color);
        }

        // Paint 'em red!
        static const LLColor4 highlight_face_color(1.f, 0.f, 0.f, 0.5f);
        for (auto facep : mHighlightFaces)
        {
            facep->renderSelected(LLViewerTexture::sNullImagep, highlight_face_color);
        }

        unbindHighlightProgram(gHighlightProgram);
    }

    // Contains a list of the faces of objects that are physical or
    // have touch-handlers.
    mHighlightFaces.clear();

    if (hasRenderDebugFeatureMask(RENDER_DEBUG_FEATURE_SELECTED) &&
        gViewerWindow && gViewerWindow->getIndicatorsVisible()) // [BDMerge HideUI]
    {
        if (sRenderHighlightTextureChannel == LLRender::NORMAL_MAP)
        {
            static const LLColor4 highlight_normal_color(1.0f, 0.5f, 0.5f, 0.5f);
            bindHighlightProgram(gHighlightNormalProgram);
            renderSelectedFaces(highlight_normal_color);
            unbindHighlightProgram(gHighlightNormalProgram);
        }
        else if (sRenderHighlightTextureChannel == LLRender::SPECULAR_MAP)
        {
            static const LLColor4 highlight_specular_color(0.0f, 0.3f, 1.0f, 0.8f);
            bindHighlightProgram(gHighlightSpecularProgram);
            renderSelectedFaces(highlight_specular_color);
            unbindHighlightProgram(gHighlightSpecularProgram);
        }
    }
}

//debug use
U32 LLPipeline::sCurRenderPoolType = 0 ;

// ===========================================================================
// [GhostDeferred] Scene-lit Ghost Studio clone submission into the deferred
// G-buffer. See doc/SCENE_LIT_CLONE_P1_DRAW_RECIPE.md. First-light cut: rigged
// opaque+masked only (PASS_SIMPLE_RIGGED, PASS_ALPHA_MASK_RIGGED, scalar
// PASS_GLTF_PBR[_ALPHA_MASK]_RIGGED); multi-material (indexed) PBR is skipped.
// The pushGhost* variants are the stock single-batch helpers with the
// applyModelMatrix() call REMOVED -- that call would reload gGLModelView and
// destroy the clone's T(foot)*R*S*T(-pivot) placement.
// ===========================================================================
namespace
{
void setup_ghost_texture_matrix(LLDrawInfo& params)
{
    if (params.mTextureMatrix)
    {
        gGL.getTexUnit(0)->activate();
        gGL.matrixMode(LLRender::MM_TEXTURE);
        gGL.loadMatrix((GLfloat*)params.mTextureMatrix->mMatrix);
        ++gPipeline.mTextureMatrixOps;
    }
}

void teardown_ghost_texture_matrix(LLDrawInfo& params)
{
    if (params.mTextureMatrix)
    {
        gGL.matrixMode(LLRender::MM_TEXTURE0);
        gGL.loadIdentity();
        gGL.matrixMode(LLRender::MM_MODELVIEW);
    }
}

// pushBatch (lldrawpool.cpp) minus applyModelMatrix.
// Returns true iff the drawRange was actually issued (the caller's per-category
// coverage accounting depends on knowing every eligible batch really drew).
bool pushGhostBatch(LLDrawInfo& params, bool batch_textures)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;

    if (!params.mCount || params.mVertexBuffer.isNull())
    {
        return false;
    }

    bool tex_setup = false;

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
    {
        if (params.mTexture.notNull())
        {
            gGL.getTexUnit(0)->bindFast(params.mTexture);

            if (params.mTextureMatrix)
            {
                tex_setup = true;
                gGL.getTexUnit(0)->activate();
                gGL.matrixMode(LLRender::MM_TEXTURE);
                gGL.loadMatrix((GLfloat*)params.mTextureMatrix->mMatrix);
                ++gPipeline.mTextureMatrixOps;
            }
        }
        else
        {
            gGL.getTexUnit(0)->unbindFast(LLTexUnit::TT_TEXTURE);
        }
    }

    // Deliberately no LLRenderPass::applyModelMatrix(params).
    params.mVertexBuffer->setBuffer();
    params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart,
                                    params.mEnd, params.mCount, params.mOffset);

    if (tex_setup)
    {
        gGL.matrixMode(LLRender::MM_TEXTURE0);
        gGL.loadIdentity();
        gGL.matrixMode(LLRender::MM_MODELVIEW);
    }
    return true;
}

// pushGLTFBatch (lldrawpool.cpp) minus applyModelMatrix (scalar/single-material).
// Returns true iff the drawRange was actually issued.
bool pushGhostGLTFBatch(LLDrawInfo& params, LLFetchedGLTFMaterial*& last_mat,
                        LLViewerTexture*& last_tex)
{
    if (!params.mCount || params.mVertexBuffer.isNull())
    {
        return false;
    }

    LLFetchedGLTFMaterial* mat = params.mGLTFMaterial.get();
    if (mat)
    {
        LLViewerTexture* tex = params.mTexture.get();
        if (mat != last_mat || tex != last_tex)
        {
            mat->bind(params.mTexture);
            last_mat = mat;
            last_tex = tex;
        }
    }

    LLGLDisable cull_face(mat && mat->mDoubleSided ? GL_CULL_FACE : 0);

    setup_ghost_texture_matrix(params);

    // Deliberately no LLRenderPass::applyModelMatrix(params).
    params.mVertexBuffer->setBuffer();
    params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart,
                                    params.mEnd, params.mCount, params.mOffset);

    teardown_ghost_texture_matrix(params);
    return true;
}

// Legacy-material rigged pass -> gDeferredMaterialProgram permutation index, or -1.
// index = (has_normal?8:0)|(has_specular?4:0)|diffuse_alpha_mode; alpha modes here:
// 0 opaque, 2 mask, 3 emissive (1=blend is the forward path, excluded). Verified
// against LLDrawPoolMaterials + llviewershadermgr.
S32 ghostMaterialShaderIndex(U32 pass)
{
    switch (pass)
    {
    case LLRenderPass::PASS_MATERIAL_RIGGED:                return 0;
    case LLRenderPass::PASS_MATERIAL_ALPHA_MASK_RIGGED:     return 2;
    case LLRenderPass::PASS_MATERIAL_ALPHA_EMISSIVE_RIGGED: return 3;
    case LLRenderPass::PASS_SPECMAP_RIGGED:                 return 4;
    case LLRenderPass::PASS_SPECMAP_MASK_RIGGED:            return 6;
    case LLRenderPass::PASS_SPECMAP_EMISSIVE_RIGGED:        return 7;
    case LLRenderPass::PASS_NORMMAP_RIGGED:                 return 8;
    case LLRenderPass::PASS_NORMMAP_MASK_RIGGED:            return 10;
    case LLRenderPass::PASS_NORMMAP_EMISSIVE_RIGGED:        return 11;
    case LLRenderPass::PASS_NORMSPEC_RIGGED:                return 12;
    case LLRenderPass::PASS_NORMSPEC_MASK_RIGGED:           return 14;
    case LLRenderPass::PASS_NORMSPEC_EMISSIVE_RIGGED:       return 15;
    default:                                               return -1;
    }
}

// Scalar part of LLDrawPoolMaterials::renderDeferred for one harvested batch, minus
// applyModelMatrix. The caller binds the shader (via bindDeferredShader) + uploads
// the palette. Defensive white/flat-normal fallbacks: harvested draw infos can
// outlive a texture transition.
bool pushGhostMaterialBatch(LLDrawInfo& params, LLGLSLShader& shader,
                            bool upload_alpha_cutoff = true)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;

    if (!params.mCount || params.mVertexBuffer.isNull())
    {
        return false;
    }

    const GLint intensity  = shader.getUniformLocation(LLShaderMgr::ENVIRONMENT_INTENSITY);
    const GLint brightness = shader.getUniformLocation(LLShaderMgr::EMISSIVE_BRIGHTNESS);
    const GLint min_alpha  = shader.getUniformLocation(LLShaderMgr::MINIMUM_ALPHA);
    const GLint specular   = shader.getUniformLocation(LLShaderMgr::SPECULAR_COLOR);

    if (intensity >= 0)  { glUniform1f(intensity, params.mEnvIntensity); }
    if (brightness >= 0) { glUniform1f(brightness, params.mFullbright ? 1.f : 0.f); }
    if (upload_alpha_cutoff && min_alpha >= 0) { glUniform1f(min_alpha, params.mAlphaMaskCutoff); }
    if (specular >= 0)   { glUniform4fv(specular, 1, params.mSpecColor.mV); }

    const GLint diffuse_channel  = shader.enableTexture(LLShaderMgr::DIFFUSE_MAP);
    const GLint specular_channel = shader.enableTexture(LLShaderMgr::SPECULAR_MAP);
    const GLint normal_channel   = shader.enableTexture(LLShaderMgr::BUMP_MAP);

    if (diffuse_channel >= 0)
    {
        if (params.mTexture.notNull())
        {
            gGL.getTexUnit(diffuse_channel)->bindFast(params.mTexture);
        }
        else
        {
            gGL.getTexUnit(diffuse_channel)->unbindFast(LLTexUnit::TT_TEXTURE);
        }
    }
    if (specular_channel >= 0)
    {
        LLViewerTexture* tex = params.mSpecularMap.notNull()
            ? params.mSpecularMap.get() : LLViewerFetchedTexture::sWhiteImagep.get();
        gGL.getTexUnit(specular_channel)->bindFast(tex);
    }
    if (normal_channel >= 0)
    {
        LLViewerTexture* tex = params.mNormalMap.notNull()
            ? params.mNormalMap.get() : LLViewerFetchedTexture::sFlatNormalImagep.get();
        gGL.getTexUnit(normal_channel)->bindFast(tex);
    }

    setup_ghost_texture_matrix(params);

    // Deliberately no LLRenderPass::applyModelMatrix(params).
    params.mVertexBuffer->setBuffer();
    params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart,
                                    params.mEnd, params.mCount, params.mOffset);

    teardown_ghost_texture_matrix(params);
    return true;
}

// LLRenderPass::pushGLTFBatchIndexed (lldrawpool.cpp) minus applyModelMatrix.
// Returns true iff the drawRange was actually issued.
bool pushGhostGLTFBatchIndexed(LLDrawInfo& params, LLRenderPass::eGLTFIndexedMaps maps)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;

    if (!params.mCount || params.mVertexBuffer.isNull()
        || params.mGLTFMaterialList.size() < 2)
    {
        return false;
    }

    const bool want_emissive = (maps == LLRenderPass::GLTF_MAPS_FULL || maps == LLRenderPass::GLTF_MAPS_GLOW);
    const bool want_full     = (maps == LLRenderPass::GLTF_MAPS_FULL);

    const S32 N = LLGLSLShader::sIndexedGLTFChannels;
    llassert((S32)params.mGLTFMaterialList.size() <= N);
    const S32 n = llmin((S32)params.mGLTFMaterialList.size(), N);

    LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;

    F32 roughness[8] = { 0.f };
    F32 metallic[8]  = { 0.f };
    F32 min_alpha[8] = { 0.f };
    F32 emissive[3 * 8] = { 0.f };
    F32 bc_xform[8 * 8] = { 0.f };
    F32 nm_xform[8 * 8] = { 0.f };
    F32 mr_xform[8 * 8] = { 0.f };
    F32 em_xform[8 * 8] = { 0.f };

    bool double_sided = false;

    for (S32 s = 0; s < n; ++s)
    {
        LLFetchedGLTFMaterial* mat = params.mGLTFMaterialList[s].get();
        if (mat == nullptr)
        {
            min_alpha[s] = -1.f;
            continue;
        }

        double_sided = double_sided || mat->mDoubleSided;

        LLViewerTexture* base = mat->mBaseColorTexture.notNull() ? mat->mBaseColorTexture.get() : LLViewerFetchedTexture::sWhiteImagep.get();
        gGL.getTexUnit(s)->bindFast(base);

        min_alpha[s] = (mat->mAlphaMode == LLGLTFMaterial::ALPHA_MODE_MASK) ? mat->mAlphaCutoff : -1.f;

        LLGLTFMaterial::TextureTransform::Pack packed;
        mat->mTextureTransform[LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR].getPacked(packed);
        memcpy(&bc_xform[8 * s], packed, sizeof(packed));

        if (!want_emissive) { continue; }

        LLViewerTexture* em = mat->mEmissiveTexture.notNull() ? mat->mEmissiveTexture.get() : LLViewerFetchedTexture::sWhiteImagep.get();
        gGL.getTexUnit(3 * N + s)->bindFast(em);

        emissive[3 * s + 0] = mat->mEmissiveColor.mV[0];
        emissive[3 * s + 1] = mat->mEmissiveColor.mV[1];
        emissive[3 * s + 2] = mat->mEmissiveColor.mV[2];

        mat->mTextureTransform[LLGLTFMaterial::GLTF_TEXTURE_INFO_EMISSIVE].getPacked(packed);
        memcpy(&em_xform[8 * s], packed, sizeof(packed));

        if (!want_full) { continue; }

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
    shader->uniform1fv(sMinAlpha, n, min_alpha);
    shader->uniform4fv(sBcXform, 2 * n, bc_xform);

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

    // Deliberately no LLRenderPass::applyModelMatrix(params).
    params.mVertexBuffer->setBuffer();
    params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart, params.mEnd, params.mCount, params.mOffset);
    return true;
}

// Composes view * T(foot) * R(rotation) * S(scale) * T(-pivot) onto the
// modelview for one clone (matches drawGeometryGhost). Invalidates gGLLastMatrix
// per placement so the batch matrix cache cannot skip the reload.
class ScopedGhostTransform
{
public:
    explicit ScopedGhostTransform(const LLActorMover::GhostProxy& proxy)
    {
        gGL.matrixMode(LLRender::MM_MODELVIEW);
        gGL.pushMatrix();

        gGL.translatef(proxy.mFootAgent.mV[VX], proxy.mFootAgent.mV[VY],
                       proxy.mFootAgent.mV[VZ]);

        if (!proxy.mRotation.isIdentity())
        {
            LLMatrix4 rotation(proxy.mRotation);
            gGL.multMatrix((GLfloat*)rotation.mMatrix);
        }

        if (proxy.mScale != 1.f)
        {
            gGL.scalef(proxy.mScale, proxy.mScale, proxy.mScale);
        }

        gGL.translatef(-proxy.mPivotFootAgent.mV[VX], -proxy.mPivotFootAgent.mV[VY],
                       -proxy.mPivotFootAgent.mV[VZ]);

        gGLLastMatrix = nullptr;
        gGL.syncMatrices();
    }

    ~ScopedGhostTransform()
    {
        gGL.matrixMode(LLRender::MM_MODELVIEW);
        gGL.popMatrix();
        gGLLastMatrix = nullptr;
        gGL.syncMatrices();
    }

    ScopedGhostTransform(const ScopedGhostTransform&) = delete;
    ScopedGhostTransform& operator=(const ScopedGhostTransform&) = delete;
};

// [GhostDeferred] SINGLE execution-domain classifier for a harvested rigged
// pass, shared by the G-buffer submission (which counts required solids + draws
// the G-buffer ones) and the forward pass (which draws its assigned stages).
// One classifier keeps the two phases' notion of "solid" identical -- the domain
// drift that produced the wardrobe fallback lives entirely in disagreement here.
enum class EGhostBatchPhase : U8
{
    GBUFFER_SOLID,              // drawn into the G-buffer this frame
    FORWARD_FULLBRIGHT,         // post-deferred forward (gDeferredFullbright)
    FORWARD_FULLBRIGHT_SHINY,   // post-deferred forward env/reflection
    FORWARD_FULLBRIGHT_MASKED,  // post-deferred forward, alpha-mask cutoff
    BLEND,                      // rigged alpha-blend (later slice)
    GLOW,                       // GLTF additive emissive (overlay)
    UNSUPPORTED_SOLID,          // an overlay-solid pass no ghost path can cover
};

EGhostBatchPhase classifyGhostBatchPhase(U32 pass)
{
    switch (pass)
    {
    case LLRenderPass::PASS_SIMPLE_RIGGED:
    case LLRenderPass::PASS_ALPHA_MASK_RIGGED:
    case LLRenderPass::PASS_BUMP_RIGGED:
    case LLRenderPass::PASS_SHINY_RIGGED:   // anomalous -> normalized to simple
    case LLRenderPass::PASS_GLTF_PBR_RIGGED:
    case LLRenderPass::PASS_GLTF_PBR_ALPHA_MASK_RIGGED:
        return EGhostBatchPhase::GBUFFER_SOLID;

    case LLRenderPass::PASS_FULLBRIGHT_RIGGED:
        return EGhostBatchPhase::FORWARD_FULLBRIGHT;
    case LLRenderPass::PASS_FULLBRIGHT_SHINY_RIGGED:
        return EGhostBatchPhase::FORWARD_FULLBRIGHT_SHINY;
    case LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK_RIGGED:
        return EGhostBatchPhase::FORWARD_FULLBRIGHT_MASKED;

    default:
        // Legacy material passes (material/specmap/normmap/normspec + mask/
        // emissive) enter the G-buffer via ghostMaterialShaderIndex.
        if (ghostMaterialShaderIndex(pass) >= 0)
        {
            return EGhostBatchPhase::GBUFFER_SOLID;
        }
        if (ghost_pass_is_blend(pass))  { return EGhostBatchPhase::BLEND; }
        if (ghost_pass_is_glow(pass))   { return EGhostBatchPhase::GLOW; }
        return EGhostBatchPhase::UNSUPPORTED_SOLID;
    }
}

// [GhostDeferred] LLDrawPoolBump::pushBumpBatch (lldrawpoolbump.cpp) minus
// applyModelMatrix and minus all file-static bump-pool state (shiny/diffuse_
// channel/shader-level). The caller has bound gDeferredBumpProgram's rigged
// variant, enabled DIFFUSE_MAP + BUMP_MAP, bound the bump map (bindBumpMap) and
// uploaded the palette. Returns true iff the drawRange was issued.
bool pushGhostBumpBatch(LLDrawInfo& params, S32 diffuse_channel)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;

    if (!params.mCount || params.mVertexBuffer.isNull() || diffuse_channel < 0)
    {
        return false;
    }

    if (params.mTexture.notNull())
    {
        gGL.getTexUnit(diffuse_channel)->bindFast(params.mTexture);
    }
    else
    {
        gGL.getTexUnit(diffuse_channel)->unbind(LLTexUnit::TT_TEXTURE);
    }

    bool tex_setup = false;
    if (params.mTextureMatrix)
    {
        gGL.getTexUnit(diffuse_channel)->activate();
        gGL.matrixMode(LLRender::MM_TEXTURE);
        gGL.loadMatrix((GLfloat*)params.mTextureMatrix->mMatrix);
        gGL.matrixMode(LLRender::MM_MODELVIEW);
        ++gPipeline.mTextureMatrixOps;
        tex_setup = true;
    }

    // Deliberately no LLRenderPass::applyModelMatrix(params).
    params.mVertexBuffer->setBuffer();
    params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart,
                                    params.mEnd, params.mCount, params.mOffset);

    if (tex_setup)
    {
        gGL.getTexUnit(diffuse_channel)->activate();
        gGL.matrixMode(LLRender::MM_TEXTURE);
        gGL.loadIdentity();
        gGL.matrixMode(LLRender::MM_MODELVIEW);
    }
    return true;
}

// [GhostDeferred] fullbright-shiny rigged draw for one batch: the diffuse bind +
// texture matrix + drawRange, minus applyModelMatrix and minus LLDrawPoolBump's
// file-static shiny/channel state. The caller has bound gDeferredFullbrightShiny
// rigged + set up exposure / reflection-probe-or-cube env / SHINY_ORIGIN (all in
// CAMERA space, before the clone transform). `indexed` selects the vertex
// texture-index attribute path (channel 0 diffuse base) vs a scalar diffuse bind.
bool pushGhostFullbrightShinyBatch(LLDrawInfo& params, S32 diffuse_channel, bool indexed)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;

    if (!params.mCount || params.mVertexBuffer.isNull())
    {
        return false;
    }

    bool tex_setup = false;
    if (indexed)
    {
        // multi-texture: the vertex texture_index attribute selects per vertex
        for (U32 i = 0; i < params.mTextureList.size(); ++i)
        {
            if (params.mTextureList[i].notNull())
            {
                gGL.getTexUnit(i)->bindFast(params.mTextureList[i]);
            }
        }
    }
    else
    {
        if (diffuse_channel < 0)
        {
            return false;   // scalar path needs a real diffuse sampler
        }
        if (params.mTexture.notNull())
        {
            gGL.getTexUnit(diffuse_channel)->bindFast(params.mTexture);
        }
        else
        {
            gGL.getTexUnit(diffuse_channel)->unbind(LLTexUnit::TT_TEXTURE);
        }
        if (params.mTextureMatrix)
        {
            gGL.getTexUnit(diffuse_channel)->activate();
            gGL.matrixMode(LLRender::MM_TEXTURE);
            gGL.loadMatrix((GLfloat*)params.mTextureMatrix->mMatrix);
            gGL.matrixMode(LLRender::MM_MODELVIEW);
            ++gPipeline.mTextureMatrixOps;
            tex_setup = true;
        }
    }

    // Deliberately no LLRenderPass::applyModelMatrix(params).
    params.mVertexBuffer->setBuffer();
    params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart,
                                    params.mEnd, params.mCount, params.mOffset);

    if (tex_setup)
    {
        gGL.getTexUnit(diffuse_channel)->activate();
        gGL.matrixMode(LLRender::MM_TEXTURE);
        gGL.loadIdentity();
        gGL.matrixMode(LLRender::MM_MODELVIEW);
    }
    return true;
}
} // anonymous namespace

GhostCoverageMask LLPipeline::getGhostDeferredCoverageThisFrame(const LLUUID& instance_id) const
{
    if (mGhostDeferredCoverageFrame != LLFrameTimer::getFrameCount())
    {
        return GHOST_COVERAGE_NONE;     // stale frame (or submission never ran)
    }
    auto it = mGhostDeferredCoverage.find(instance_id);
    return it != mGhostDeferredCoverage.end() ? it->second : GHOST_COVERAGE_NONE;
}

void LLPipeline::renderGhostDeferredOpaqueMasked(const LLCamera& camera)
{
    static LLCachedControl<bool> user_enabled(gSavedSettings, "GhostDeferredEnable", false);
    // The contamination test can force submission off/on for its OFF/ON captures
    // without touching the user's setting; NONE = honor the user setting.
    if (!ghostDeferredSubmissionEnabled(user_enabled()))
    {
        return;     // OFF issues no GL op -> stock deferred path is byte-identical
    }

    LLActorMover& mover = LLActorMover::instance();
    const LLActorMover::GhostProxyQueue& queue =
        mover.getGhostDeferredQueue(camera, LLActorMover::GHOST_VIEW_WORLD_MAIN);
    LLActorMover::GhostDeferredCounters& counters = mover.ghostDeferredCounters();

    const U32 frame = LLFrameTimer::getFrameCount();
    if (mGhostDeferredCoverageFrame != frame)
    {
        mGhostDeferredCoverageFrame = frame;
        mGhostDeferredCoverage.clear();
    }
    // [Coverage] PENDING solid-completion progress rebuilds each frame alongside
    // the coverage map. A clone's solid faces draw across TWO phases (here in the
    // G-buffer, plus the post-deferred forward pass for fullbright/fullbright-
    // shiny/mask); this map accumulates required-vs-drawn per instance and the
    // FINALIZE forward stage promotes RIGGED_SOLID only when every required solid
    // batch drew. Reset here and NOT from the forward pass (which only consumes it).
    if (mGhostSubmissionProgressFrame != frame)
    {
        mGhostSubmissionProgressFrame = frame;
        mGhostSubmissionProgress.clear();
    }

    // Preserve the actual incoming colour mask (renderGeomDeferred exits with
    // (true,false)); restore through gGL so its cache stays synced with GL.
    GLboolean saved_color_mask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
    glGetBooleanv(GL_COLOR_WRITEMASK, saved_color_mask);

    struct ScopedGhostColorMask
    {
        explicit ScopedGhostColorMask(const GLboolean (&saved)[4])
        {
            for (S32 i = 0; i < 4; ++i) { mSaved[i] = saved[i]; }
            gGL.setColorMask(true, true);
        }
        ~ScopedGhostColorMask()
        {
            gGL.setColorMask(mSaved[0] != GL_FALSE, mSaved[1] != GL_FALSE,
                             mSaved[2] != GL_FALSE, mSaved[3] != GL_FALSE);
        }
        GLboolean mSaved[4];
    } color_mask_scope(saved_color_mask);

    LLGLDepthTest depth_state(GL_TRUE, GL_TRUE, GL_LEQUAL);   // (enabled, write, func)
    LLGLDisable blend_state(GL_BLEND);
    LLGLEnable cull_state(GL_CULL_FACE);

    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGLLastMatrix = nullptr;
    gGL.loadMatrix(gGLModelView);

    // Restore the incoming active texture unit at exit -- pushGhostBatch, the
    // texture-matrix setup, and LLFetchedGLTFMaterial::bind() all activate units.
    const U32 saved_texture_unit = gGL.getCurrentTexUnitIndex();
    // Disable the bump shader's samplers at teardown if it drew (stock renderDeferred
    // does; the invariant checker does not track individual texture bindings).
    bool bump_shader_used = false;

    for (const LLActorMover::GhostProxy& proxy : queue.mProxies)
    {
        if (!proxy.mFrustumVisible || !proxy.mBatches || proxy.mBatches->empty())
        {
            continue;
        }

        ScopedGhostTransform transform(proxy);

        // [Coverage] per-instance cross-phase progress. RIGGED_SOLID completeness
        // is decided by the FINALIZE forward stage (NOT here) -- a clone's solid
        // faces may still be pending in the fullbright/shiny forward pass. Every
        // overlay-solid batch is counted required wherever it draws; a G-buffer
        // solid draws now, a forward solid is recorded as an expected stage.
        GhostSubmissionProgress& submission = mGhostSubmissionProgress[proxy.mInstanceId];
        GhostCategoryProgress& solid = submission.mRiggedSolid;
        solid.mClassified = true;

        LLGLSLShader* last_shader = nullptr;
        LLFetchedGLTFMaterial* last_gltf_material = nullptr;
        LLViewerTexture* last_gltf_texture = nullptr;
        S32 bump_diffuse_channel = -1;
        S32 bump_map_channel = -1;

        for (const LLActorMover::GhostBatch& batch : *proxy.mBatches)
        {
            LLDrawInfo* di = batch.mInfo;

            // ---- classify the execution phase ONCE (shared with the forward pass
            // via classifyGhostBatchPhase, so the two never disagree about what is
            // "solid"). Forward-solid passes are counted required here but DRAWN in
            // the post-deferred forward pass; blend/glow belong to other categories;
            // an unsupported solid fails the category (keeps the hole-proof rule).
            switch (classifyGhostBatchPhase(batch.mPass))
            {
            case EGhostBatchPhase::BLEND:
                // The forward-alpha stage owns this category. Count every eligible
                // batch here, before any forward draw is attempted, so partial
                // shader/material/palette failures cannot claim coverage.
                ++submission.mRiggedBlend.mRequired;
                submission.mRiggedBlend.mClassified = true;
                submission.mExpectedForwardBlendStages |= GHOST_FORWARD_BLEND_RIGGED;
                continue;
            case EGhostBatchPhase::GLOW:
                continue;   // not part of solid completeness

            case EGhostBatchPhase::FORWARD_FULLBRIGHT:
                ++solid.mRequired;
                submission.mExpectedForwardSolidStages |= GHOST_FORWARD_SOLID_FULLBRIGHT;
                continue;
            case EGhostBatchPhase::FORWARD_FULLBRIGHT_SHINY:
                ++solid.mRequired;
                submission.mExpectedForwardSolidStages |= GHOST_FORWARD_SOLID_SHINY;
                continue;
            case EGhostBatchPhase::FORWARD_FULLBRIGHT_MASKED:
                ++solid.mRequired;
                submission.mExpectedForwardSolidStages |= GHOST_FORWARD_SOLID_MASKED;
                continue;

            case EGhostBatchPhase::UNSUPPORTED_SOLID:
                ++solid.mRequired;
                solid.mFailed = true;   // overlay-solid, no ghost path can cover it
                continue;

            case EGhostBatchPhase::GBUFFER_SOLID:
                break;                  // draw it into the G-buffer below
            }

            // ---- GBUFFER_SOLID: draw into the G-buffer this frame ----
            ++solid.mRequired;
            if (!di)
            {
                // a null draw-info on a solid batch cannot claim completeness
                solid.mFailed = true;
                continue;
            }

            LLGLSLShader* shader = nullptr;
            bool legacy_textured = false;
            bool legacy_material = false;
            bool gltf_scalar = false;
            bool gltf_indexed = false;
            bool bump_deferred = false;

            switch (batch.mPass)
            {
            case LLRenderPass::PASS_SIMPLE_RIGGED:
            case LLRenderPass::PASS_SHINY_RIGGED:   // anomalous residue -> deferred simple
                shader = gDeferredDiffuseProgram.mRiggedVariant;
                legacy_textured = true;
                break;

            case LLRenderPass::PASS_ALPHA_MASK_RIGGED:
                shader = gDeferredDiffuseAlphaMaskProgram.mRiggedVariant;
                legacy_textured = true;
                break;

            case LLRenderPass::PASS_BUMP_RIGGED:
                // bump writes normal + diffuse into the G-buffer (its own pool's
                // renderDeferred path); own draw branch, NOT legacy_textured.
                shader = gDeferredBumpProgram.mRiggedVariant;
                bump_deferred = true;
                break;

            case LLRenderPass::PASS_GLTF_PBR_RIGGED:
            case LLRenderPass::PASS_GLTF_PBR_ALPHA_MASK_RIGGED:
                if (di->mGLTFMaterialList.size() > 1)
                {
                    // multi-material -> indexed PBR (its own palette + slot arrays).
                    shader = gDeferredPBROpaqueIndexedProgram.mRiggedVariant;
                    gltf_indexed = true;
                }
                else
                {
                    if (!di->mGLTFMaterial)
                    {
                        solid.mFailed = true;   // null material would draw stale
                        continue;
                    }
                    shader = gDeferredPBROpaqueProgram.mRiggedVariant;
                    gltf_scalar = true;
                }
                break;

            default:
            {
                // classifyGhostBatchPhase() guaranteed GBUFFER_SOLID here, so this
                // is a legacy material pass with a valid index.
                const S32 material_index = ghostMaterialShaderIndex(batch.mPass);
                shader = gDeferredMaterialProgram[material_index].mRiggedVariant;
                legacy_material = true;
                break;
            }
            }

            if (di->mVertexBuffer.isNull() || !di->mCount
                || !di->mAvatar || !di->mSkinInfo)
            {
                solid.mFailed = true;
                continue;
            }

            if (!shader || !shader->isComplete())
            {
                if (gltf_indexed)
                {
                    // Indexed PBR shader is optional in the loader; a multi-material
                    // batch can't be drawn through the scalar path, so skip + note it.
                    LL_WARNS_ONCE("GhostDeferred")
                        << "indexed PBR rigged shader unavailable; multi-material PBR "
                           "clone batches will be skipped" << LL_ENDL;
                }
                solid.mFailed = true;
                continue;
            }

            if (shader != last_shader)
            {
                // Legacy material shaders need the deferred pool's bind path (sets
                // the deferred uniforms/samplers); simple/PBR/bump shaders bind
                // directly as their stock pools do.
                if (legacy_material)
                {
                    bindDeferredShader(*shader);
                }
                else
                {
                    shader->bind();
                }
                last_shader = shader;
                // Program-local caches: reset at every shader switch.
                last_gltf_material = nullptr;
                last_gltf_texture = nullptr;
                bump_diffuse_channel = -1;
                bump_map_channel = -1;
                if (bump_deferred)
                {
                    // enable the bump shader's two samplers once per activation;
                    // disabled at teardown (see bump_shader_used below).
                    bump_diffuse_channel = shader->enableTexture(LLViewerShaderMgr::DIFFUSE_MAP);
                    bump_map_channel     = shader->enableTexture(LLViewerShaderMgr::BUMP_MAP);
                    bump_shader_used = true;
                }
            }

            // Correctness-first cut: upload the skin palette for every batch (also
            // avoids any palette-dedup cache crossing a shader switch).
            if (!LLRenderPass::uploadMatrixPalette(*di))
            {
                solid.mFailed = true;
                continue;
            }

            bool drew = false;
            if (legacy_textured)
            {
                // Masked passes need the per-batch cutoff uploaded to the bound
                // shader (stock pushRiggedMaskBatches does this); otherwise the
                // draw uses a stale MINIMUM_ALPHA from an earlier batch.
                if (batch.mPass == LLRenderPass::PASS_ALPHA_MASK_RIGGED)
                {
                    shader->setMinimumAlpha(di->mAlphaMaskCutoff);
                }
                drew = pushGhostBatch(*di, true);
            }
            else if (bump_deferred)
            {
                if (bump_diffuse_channel < 0 || bump_map_channel < 0)
                {
                    solid.mFailed = true;
                    continue;
                }
                // stock deferred bump uses the batch alpha-mask cutoff + generated
                // bump map (mBump + source texture select it; bindBumpMap resolves).
                shader->setMinimumAlpha(di->mAlphaMaskCutoff);
                if (!LLDrawPoolBump::bindBumpMap(*di, bump_map_channel))
                {
                    solid.mFailed = true;
                    continue;
                }
                drew = pushGhostBumpBatch(*di, bump_diffuse_channel);
            }
            else if (legacy_material)
            {
                drew = pushGhostMaterialBatch(*di, *shader);
            }
            else if (gltf_scalar)
            {
                // Stock PBR deferred rendering enables framebuffer sRGB; scope it
                // to this individual PBR draw.
                LLGLEnable framebuffer_srgb(GL_FRAMEBUFFER_SRGB);
                drew = pushGhostGLTFBatch(*di, last_gltf_material, last_gltf_texture);
            }
            else if (gltf_indexed)
            {
                LLGLEnable framebuffer_srgb(GL_FRAMEBUFFER_SRGB);
                drew = pushGhostGLTFBatchIndexed(*di, LLRenderPass::GLTF_MAPS_FULL);
            }

            if (!drew)
            {
                solid.mFailed = true;
                continue;
            }

            ++counters.mActualDrawCalls;
            ++counters.mRiggedSolidDrawCalls;
            ++solid.mDrawn;
            if (!submission.mAnyDrawSubmitted)
            {
                submission.mAnyDrawSubmitted = true;
                ++counters.mProxiesSubmitted;   // aggregate: >=1 draw in ANY phase
            }
        }
        // [Coverage] NO finalize here -- RIGGED_SOLID is settled by the FINALIZE
        // forward stage once the fullbright/shiny forward draws have run.
    }

    // Disable the bump shader's samplers if it drew (stock renderDeferred does
    // this before unbinding; the invariant checker does not track individual
    // texture bindings, so leaving them could leak a texture into a later pass).
    if (bump_shader_used && gDeferredBumpProgram.mRiggedVariant)
    {
        gDeferredBumpProgram.mRiggedVariant->bind();
        gDeferredBumpProgram.mRiggedVariant->disableTexture(LLViewerShaderMgr::DIFFUSE_MAP);
        gDeferredBumpProgram.mRiggedVariant->disableTexture(LLViewerShaderMgr::BUMP_MAP);
    }

    LLVertexBuffer::unbind();
    LLGLSLShader::unbind();

    gGL.getTexUnit(saved_texture_unit)->activate();

    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGLLastMatrix = nullptr;
    gGL.loadMatrix(gGLModelView);
    gGL.syncMatrices();
}

void LLPipeline::finalizeGhostRiggedSolidCoverage()
{
    // Promote RIGGED_SOLID for every instance whose solid faces ALL drew this
    // frame -- across both phases (G-buffer + fullbright/shiny forward). Iterates
    // the progress map directly (its keys are this frame's submitted instances).
    // Monotonic: a bit is only ever SET here, never cleared, so no observer sees
    // premature or retracted coverage.
    LLActorMover::GhostDeferredCounters& counters =
        LLActorMover::instance().ghostDeferredCounters();

    for (auto& entry : mGhostSubmissionProgress)
    {
        GhostSubmissionProgress& submission = entry.second;
        GhostCategoryProgress& solid = submission.mRiggedSolid;

        if (solid.mFinalized)
        {
            continue;   // idempotent (FINALIZE could be reached more than once)
        }
        solid.mFinalized = true;

        // Every forward-solid stage the instance expected must have actually run
        // (a stage marks itself completed even when its batches fail, so a missing
        // completion bit means the stage entry point never executed -> incomplete).
        const bool stages_complete =
            (submission.mCompletedForwardSolidStages
             & submission.mExpectedForwardSolidStages)
            == submission.mExpectedForwardSolidStages;

        const bool complete = solid.mClassified
            && !solid.mFailed
            && solid.mRequired > 0
            && solid.mDrawn == solid.mRequired
            && stages_complete;

        if (!complete)
        {
            continue;   // absent from coverage -> overlay keeps the full body
        }

        GhostCoverageMask& coverage = mGhostDeferredCoverage[entry.first];
        if (!(coverage & GHOST_COVERAGE_RIGGED_SOLID))
        {
            coverage |= GHOST_COVERAGE_RIGGED_SOLID;
            ++counters.mRiggedSolidInstancesSubmitted;
        }
    }
}

bool LLPipeline::ghostPostDeferredSolidsPending(const LLCamera& camera) const
{
    // Same guards as renderGhostPostDeferred's drawing stages (world view + this
    // frame's queue) plus "some instance expects a forward-solid stage". When the
    // toggle is off the G-buffer pass returns before stamping the progress frame,
    // so the frame check fails here and the caller changes NO state (byte-identical).
    if (gCubeSnapshot
        || LLPipeline::sReflectionRender
        || LLPipeline::sRenderingHUDs
        || LLViewerCamera::sCurCameraID != LLViewerCamera::CAMERA_WORLD)
    {
        return false;
    }
    if (mGhostSubmissionProgressFrame != LLFrameTimer::getFrameCount())
    {
        return false;
    }
    if (!LLActorMover::instance().hasValidGhostDeferredQueueThisFrame(
            camera, LLActorMover::GHOST_VIEW_WORLD_MAIN))
    {
        return false;
    }
    const U32 all_forward_solid = GHOST_FORWARD_SOLID_FULLBRIGHT
                                | GHOST_FORWARD_SOLID_SHINY
                                | GHOST_FORWARD_SOLID_MASKED;
    for (const auto& entry : mGhostSubmissionProgress)
    {
        if (entry.second.mExpectedForwardSolidStages & all_forward_solid)
        {
            return true;
        }
    }
    return false;
}

bool LLPipeline::ghostPostDeferredBlendPending(const LLCamera& camera) const
{
    // Keep this probe entirely read-only. In particular, the disabled path must
    // not bind a shader, touch GL state, or force construction of the invariant.
    if (gCubeSnapshot
        || LLPipeline::sReflectionRender
        || LLPipeline::sRenderingHUDs
        || LLViewerCamera::sCurCameraID != LLViewerCamera::CAMERA_WORLD)
    {
        return false;
    }
    if (mGhostSubmissionProgressFrame != LLFrameTimer::getFrameCount())
    {
        return false;
    }
    if (!LLActorMover::instance().hasValidGhostDeferredQueueThisFrame(
            camera, LLActorMover::GHOST_VIEW_WORLD_MAIN))
    {
        return false;
    }
    for (const auto& entry : mGhostSubmissionProgress)
    {
        if (entry.second.mExpectedForwardBlendStages & GHOST_FORWARD_BLEND_RIGGED)
        {
            return true;
        }
    }
    return false;
}

void LLPipeline::renderGhostRiggedBlend(const LLCamera& camera)
{
    // This function is reached only through renderGhostPostDeferred, after its
    // world-camera, frame-stamp and queue-identity guards. Repeat the no-work
    // check before touching GL so a disabled/no-blend frame is a true GL no-op.
    bool any_expected = false;
    for (const auto& entry : mGhostSubmissionProgress)
    {
        if (entry.second.mExpectedForwardBlendStages & GHOST_FORWARD_BLEND_RIGGED)
        {
            any_expected = true;
            break;
        }
    }
    if (!any_expected)
    {
        return;
    }

    LLActorMover& mover = LLActorMover::instance();
    const LLActorMover::GhostProxyQueue& queue =
        mover.getGhostDeferredQueue(camera, LLActorMover::GHOST_VIEW_WORLD_MAIN);
    LLActorMover::GhostDeferredCounters& counters = mover.ghostDeferredCounters();

    // renderGeomPostDeferred leaves the stock alpha blend factors active. Save the
    // non-RAII state this stage changes and restore the same canonical post-deferred
    // state at exit.
    const U32 saved_texture_unit = gGL.getCurrentTexUnitIndex();
    GLboolean saved_color_mask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
    glGetBooleanv(GL_COLOR_WRITEMASK, saved_color_mask);

    LLGLDepthTest depth_state(GL_TRUE, GL_FALSE, GL_LEQUAL);
    LLGLEnable cull_state(GL_CULL_FACE);
    LLGLEnable blend_state(GL_BLEND);
    gGL.setColorMask(true, true);
    gGL.setSceneBlendType(LLRender::BT_ALPHA);

    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGLLastMatrix = nullptr;
    gGL.loadMatrix(gGLModelView);

    // Alpha instances composite far-to-near. Preserve harvested draw-map order
    // within each instance, matching the stock avatar-alpha behavior.
    std::vector<const LLActorMover::GhostProxy*> sorted_proxies;
    sorted_proxies.reserve(queue.mProxies.size());
    for (const LLActorMover::GhostProxy& proxy : queue.mProxies)
    {
        if (proxy.mFrustumVisible && proxy.mBatches && !proxy.mBatches->empty())
        {
            sorted_proxies.push_back(&proxy);
        }
    }

    const LLVector3 camera_origin = camera.getOrigin();
    std::sort(sorted_proxies.begin(), sorted_proxies.end(),
        [&camera_origin](const LLActorMover::GhostProxy* lhs,
                         const LLActorMover::GhostProxy* rhs)
        {
            const F32 lhs_distance = (lhs->mWorldBoundsCenter - camera_origin).magVecSquared();
            const F32 rhs_distance = (rhs->mWorldBoundsCenter - camera_origin).magVecSquared();
            if (lhs_distance != rhs_distance)
            {
                return lhs_distance > rhs_distance;
            }
            return lhs->mInstanceId < rhs->mInstanceId;
        });

    LLGLSLShader* current_shader = nullptr;
    LLFetchedGLTFMaterial* last_gltf_material = nullptr;
    LLViewerTexture* last_gltf_texture = nullptr;

    enum class EGhostBlendBatchKind : U8 { SIMPLE, FULLBRIGHT, MATERIAL, GLTF };

    // Resolve the real forward-alpha shader + kind for a batch, or return false if
    // Slice 1 cannot faithfully draw it (indexed PBR/legacy, non-blend GLTF,
    // out-of-range shader mask, incomplete shader, invalid draw info). Used FIRST
    // as a whole-category validation pass so a partial draw can never commit
    // coverage and leave the bespoke sweep to double-alpha the batches already
    // drawn -- and again in the draw pass, where it is guaranteed to succeed.
    auto resolve_blend_shader = [](LLDrawInfo* di, LLGLSLShader*& out_shader,
                                   EGhostBlendBatchKind& out_kind) -> bool
    {
        out_shader = nullptr;
        out_kind = EGhostBlendBatchKind::SIMPLE;
        if (!di || di->mVertexBuffer.isNull() || !di->mCount
            || !di->mAvatar || !di->mSkinInfo)
        {
            return false;
        }
        if (di->mGLTFMaterial.notNull())
        {
            if (di->mGLTFMaterial->mAlphaMode != LLGLTFMaterial::ALPHA_MODE_BLEND
                || di->mGLTFMaterialList.size() > 1)
            {
                return false;   // indexed / non-blend PBR: outside Slice 1
            }
            out_shader = gDeferredPBRAlphaProgram.mRiggedVariant;
            out_kind = EGhostBlendBatchKind::GLTF;
        }
        else if (di->mMaterial.notNull())
        {
            // Indexed legacy material (mTextureList > 1) needs per-slot binds the
            // scalar pushGhostMaterialBatch does not do -- fail rather than mirror
            // wrong slots and falsely claim coverage.
            if (di->mShaderMask >= LLMaterial::SHADER_COUNT
                || di->mTextureList.size() > 1)
            {
                return false;
            }
            out_shader = gDeferredMaterialProgram[di->mShaderMask].mRiggedVariant;
            out_kind = EGhostBlendBatchKind::MATERIAL;
        }
        else if (di->mFullbright)
        {
            out_shader = gDeferredFullbrightAlphaMaskAlphaProgram.mRiggedVariant;
            out_kind = EGhostBlendBatchKind::FULLBRIGHT;
        }
        else
        {
            out_shader = gDeferredAlphaProgram.mRiggedVariant;
        }
        return out_shader && out_shader->isComplete();
    };

    for (const LLActorMover::GhostProxy* proxy_ptr : sorted_proxies)
    {
        const LLActorMover::GhostProxy& proxy = *proxy_ptr;
        auto progress_it = mGhostSubmissionProgress.find(proxy.mInstanceId);
        if (progress_it == mGhostSubmissionProgress.end()
            || !(progress_it->second.mExpectedForwardBlendStages & GHOST_FORWARD_BLEND_RIGGED))
        {
            continue;
        }

        GhostSubmissionProgress& submission = progress_it->second;
        GhostCategoryProgress& blend = submission.mRiggedBlend;

        // Slice-1 conservative gate: the real forward blend may run only over a
        // fully covered rigged body, and only when no uncovered static solid can
        // later overwrite it from the UI fallback.
        bool static_solid_present = false;
        if (proxy.mStaticFaces)
        {
            for (const LLActorMover::GhostStaticFace& face : *proxy.mStaticFaces)
            {
                if (face.mAlphaKind != 2)
                {
                    static_solid_present = true;
                    break;
                }
            }
        }

        const GhostCoverageMask covered = getGhostDeferredCoverageThisFrame(proxy.mInstanceId);
        if (!(covered & GHOST_COVERAGE_RIGGED_SOLID)
            || (static_solid_present && !(covered & GHOST_COVERAGE_STATIC_SOLID)))
        {
            blend.mFailed = true;
            blend.mFinalized = true;
            submission.mCompletedForwardBlendStages |= GHOST_FORWARD_BLEND_RIGGED;
            continue;
        }

        // Whole-category validation BEFORE any draw (transactional): if a single
        // eligible batch is unsupported, draw NONE and leave RIGGED_BLEND clear so
        // the bespoke sweep stays the sole owner -- otherwise a partial draw here
        // plus the fallback redraw would double-alpha the batches already drawn.
        {
            bool category_ok = true;
            for (const LLActorMover::GhostBatch& batch : *proxy.mBatches)
            {
                if (classifyGhostBatchPhase(batch.mPass) != EGhostBatchPhase::BLEND)
                {
                    continue;
                }
                LLGLSLShader* probe_shader = nullptr;
                EGhostBlendBatchKind probe_kind = EGhostBlendBatchKind::SIMPLE;
                // Deterministic support check only (no GL): resolve the real shader
                // + material kind. Palette readiness (which needs a bound shader) is
                // validated separately in the pre-pass just below.
                if (!resolve_blend_shader(batch.mInfo, probe_shader, probe_kind))
                {
                    category_ok = false;
                    break;
                }
            }
            if (!category_ok)
            {
                blend.mFailed = true;
                blend.mFinalized = true;
                submission.mCompletedForwardBlendStages |= GHOST_FORWARD_BLEND_RIGGED;
                continue;
            }
        }

        {
            ScopedGhostTransform transform(proxy);

            for (const LLActorMover::GhostBatch& batch : *proxy.mBatches)
            {
                if (classifyGhostBatchPhase(batch.mPass) != EGhostBatchPhase::BLEND)
                {
                    continue;
                }

                LLDrawInfo* di = batch.mInfo;
                LLGLSLShader* shader = nullptr;
                EGhostBlendBatchKind kind = EGhostBlendBatchKind::SIMPLE;
                if (!resolve_blend_shader(di, shader, kind))
                {
                    // Validated above; defensive -- keep the category uncovered.
                    blend.mFailed = true;
                    continue;
                }

                if (shader != current_shader)
                {
                    // Reuse the complete lit-alpha environment the stock alpha pool
                    // prepared earlier in renderGeomPostDeferred (gamma/water/sun/
                    // shadows/reflection probes) instead of reconstructing it.
                    bindDeferredShaderFast(*shader);
                    // Forward blend must not discard low-alpha fragments; stock alpha
                    // zeroes MINIMUM_ALPHA for blended draws (pushGhostMaterialBatch is
                    // called with upload_alpha_cutoff=false so it cannot re-raise it).
                    shader->setMinimumAlpha(0.f);
                    current_shader = shader;
                    last_gltf_material = nullptr;
                    last_gltf_texture = nullptr;
                }

                // Fullbright alpha cancels exposure exactly as the stock alpha pool
                // does when this shader becomes active.
                if (kind == EGhostBlendBatchKind::FULLBRIGHT)
                {
                    const S32 exposure_channel = shader->enableTexture(LLShaderMgr::EXPOSURE_MAP);
                    if (exposure_channel >= 0)
                    {
                        gGL.getTexUnit(exposure_channel)->bind(&mExposureMap);
                    }
                }

                if (!LLRenderPass::uploadMatrixPalette(*di))
                {
                    blend.mFailed = true;
                    continue;
                }

                // Match stock alpha: authored RGB factors, with glow suppressed from
                // the framebuffer alpha channel. Glow remains independently owned by
                // GHOST_COVERAGE_RIGGED_GLOW.
                gGL.blendFunc(
                    static_cast<LLRender::eBlendFactor>(di->mBlendFuncSrc),
                    static_cast<LLRender::eBlendFactor>(di->mBlendFuncDst),
                    LLRender::BF_ZERO,
                    LLRender::BF_ONE_MINUS_SOURCE_ALPHA);

                bool drew = false;
                switch (kind)
                {
                case EGhostBlendBatchKind::GLTF:
                    // Shader is deliberately bound before material->bind().
                    drew = pushGhostGLTFBatch(*di, last_gltf_material, last_gltf_texture);
                    break;

                case EGhostBlendBatchKind::MATERIAL:
                    // Alpha shaders retain the stock alpha floor prepared by
                    // LLDrawPoolAlpha. Do not replace it with a mask cutoff.
                    drew = pushGhostMaterialBatch(*di, *shader, /*upload_alpha_cutoff=*/false);
                    break;

                case EGhostBlendBatchKind::FULLBRIGHT:
                case EGhostBlendBatchKind::SIMPLE:
                    drew = pushGhostBatch(*di, true);
                    break;
                }

                if (!drew)
                {
                    blend.mFailed = true;
                    continue;
                }

                ++counters.mActualDrawCalls;
                ++counters.mRiggedBlendDrawCalls;
                ++blend.mDrawn;
                if (!submission.mAnyDrawSubmitted)
                {
                    submission.mAnyDrawSubmitted = true;
                    ++counters.mProxiesSubmitted;
                }
            }
        }

        submission.mCompletedForwardBlendStages |= GHOST_FORWARD_BLEND_RIGGED;
        blend.mFinalized = true;

        const bool stages_complete =
            (submission.mCompletedForwardBlendStages & submission.mExpectedForwardBlendStages)
            == submission.mExpectedForwardBlendStages;
        const bool complete =
            blend.mClassified && !blend.mFailed && blend.mRequired > 0
            && blend.mDrawn == blend.mRequired && stages_complete;

        if (complete)
        {
            GhostCoverageMask& coverage = mGhostDeferredCoverage[proxy.mInstanceId];
            if (!(coverage & GHOST_COVERAGE_RIGGED_BLEND))
            {
                // Monotonic all-success commit. Until this exact point the bespoke
                // SWEEP_BLEND remains authoritative and hole-safe.
                coverage |= GHOST_COVERAGE_RIGGED_BLEND;
                ++counters.mRiggedBlendInstancesSubmitted;
            }
        }
    }

    LLVertexBuffer::unbind();
    LLGLSLShader::unbind();

    gGL.getTexUnit(saved_texture_unit)->activate();
    gGL.setColorMask(saved_color_mask[0] != GL_FALSE, saved_color_mask[3] != GL_FALSE);
    gGL.setSceneBlendType(LLRender::BT_ALPHA);

    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGLLastMatrix = nullptr;
    gGL.loadMatrix(gGLModelView);
    gGL.syncMatrices();
}

void LLPipeline::renderGhostPostDeferred(const LLCamera& camera, EGhostForwardStage stage)
{
    // ---- world-view guards. renderDeferredLighting also runs for reflection
    // probes / cube snapshots / HUDs, where no world ghost queue was built this
    // frame -- submitting there would draw world clones into a probe and spam the
    // strict-accessor warn/assert. The explicit view guard is REQUIRED: a same-
    // frame cube render can reuse the same LLViewerCamera address, so the pointer/
    // frame checks alone can pass. ----
    if (gCubeSnapshot
        || LLPipeline::sReflectionRender
        || LLPipeline::sRenderingHUDs
        || LLViewerCamera::sCurCameraID != LLViewerCamera::CAMERA_WORLD)
    {
        return;
    }
    if (mGhostSubmissionProgressFrame != LLFrameTimer::getFrameCount())
    {
        return;     // this frame's G-buffer ghost pass produced no progress
    }
    LLActorMover& mover = LLActorMover::instance();
    if (!mover.hasValidGhostDeferredQueueThisFrame(camera, LLActorMover::GHOST_VIEW_WORLD_MAIN))
    {
        return;     // SILENT probe (the strict accessor would warn + assert)
    }

    // FINALIZE is bookkeeping only -- no GL op, no queue walk.
    if (stage == EGhostForwardStage::FINALIZE)
    {
        finalizeGhostRiggedSolidCoverage();
        return;
    }
    if (stage == EGhostForwardStage::RIGGED_BLEND)
    {
        renderGhostRiggedBlend(camera);
        return;
    }

    // Map the stage to its progress bit, batch phase and forward shader.
    U32 stage_bit = GHOST_FORWARD_SOLID_NONE;
    EGhostBatchPhase want_phase = EGhostBatchPhase::FORWARD_FULLBRIGHT;
    LLGLSLShader* shader = nullptr;
    bool is_shiny = false;
    bool is_masked = false;
    switch (stage)
    {
    case EGhostForwardStage::FULLBRIGHT_OPAQUE:
        stage_bit = GHOST_FORWARD_SOLID_FULLBRIGHT;
        want_phase = EGhostBatchPhase::FORWARD_FULLBRIGHT;
        shader = gDeferredFullbrightProgram.mRiggedVariant;
        break;
    case EGhostForwardStage::FULLBRIGHT_MASKED:
        stage_bit = GHOST_FORWARD_SOLID_MASKED;
        want_phase = EGhostBatchPhase::FORWARD_FULLBRIGHT_MASKED;
        shader = gDeferredFullbrightAlphaMaskProgram.mRiggedVariant;
        is_masked = true;
        break;
    case EGhostForwardStage::FULLBRIGHT_SHINY:
        stage_bit = GHOST_FORWARD_SOLID_SHINY;
        want_phase = EGhostBatchPhase::FORWARD_FULLBRIGHT_SHINY;
        shader = gDeferredFullbrightShinyProgram.mRiggedVariant;
        is_shiny = true;
        break;
    case EGhostForwardStage::RIGGED_BLEND:
        return;     // handled above
    case EGhostForwardStage::FINALIZE:
        return;     // handled above
    }

    // Issue NO GL op if no instance expects this stage (keeps the OFF path cheap).
    bool any_expected = false;
    for (const auto& entry : mGhostSubmissionProgress)
    {
        if (entry.second.mExpectedForwardSolidStages & stage_bit)
        {
            any_expected = true;
            break;
        }
    }
    if (!any_expected)
    {
        return;
    }

    const LLActorMover::GhostProxyQueue& queue =
        mover.getGhostDeferredQueue(camera, LLActorMover::GHOST_VIEW_WORLD_MAIN);
    LLActorMover::GhostDeferredCounters& counters = mover.ghostDeferredCounters();

    // Helper: mark this stage completed for every visible instance that expected
    // it -- EVEN on shader/batch failure, so FINALIZE can tell "stage ran + failed"
    // from "stage never ran". Absent/culled/unexpected instances are untouched.
    auto mark_stage_complete = [&](bool force_fail)
    {
        for (const LLActorMover::GhostProxy& proxy : queue.mProxies)
        {
            if (!proxy.mFrustumVisible || !proxy.mBatches || proxy.mBatches->empty())
            {
                continue;
            }
            auto it = mGhostSubmissionProgress.find(proxy.mInstanceId);
            if (it == mGhostSubmissionProgress.end()
                || !(it->second.mExpectedForwardSolidStages & stage_bit))
            {
                continue;
            }
            if (force_fail)
            {
                it->second.mRiggedSolid.mFailed = true;
            }
            it->second.mCompletedForwardSolidStages |= stage_bit;
        }
    };

    // Save the non-RAII state the stages touch (LLGL* guards restore enable state
    // only, not blend factors / active unit / colour mask).
    const U32 saved_texture_unit = gGL.getCurrentTexUnitIndex();
    GLboolean saved_color_mask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
    glGetBooleanv(GL_COLOR_WRITEMASK, saved_color_mask);

    if (!shader || !shader->isComplete())
    {
        // Stage cannot run: record it as attempted-and-failed so the instance
        // falls back to the full overlay rather than claiming completeness.
        mark_stage_complete(/*force_fail=*/true);
        return;
    }

    // ---- stage GL state. Fullbright solids write depth (they extend the shared
    // opaque depth buffer for later stock alpha); opaque/masked do not blend,
    // shiny blends BT_ALPHA (matches stock renderFullbrightShiny). ----
    LLGLDepthTest depth_state(GL_TRUE, GL_TRUE, GL_LEQUAL);
    LLGLEnable cull_state(GL_CULL_FACE);
    LLGLDisable blend_off(is_shiny ? 0 : GL_BLEND);
    LLGLEnable  blend_on(is_shiny ? GL_BLEND : 0);
    gGL.setColorMask(true, false);      // stock post-deferred: RGB, no FB alpha
    if (is_shiny)
    {
        gGL.setSceneBlendType(LLRender::BT_ALPHA);
    }

    // camera modelview must be active for the shiny origin / env matrix (computed
    // BEFORE any ScopedGhostTransform, exactly like stock beginFullbrightShiny).
    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGLLastMatrix = nullptr;
    gGL.loadMatrix(gGLModelView);

    shader->bind();

    const S32 exposure_channel = shader->enableTexture(LLShaderMgr::EXPOSURE_MAP);
    if (exposure_channel >= 0)
    {
        gGL.getTexUnit(exposure_channel)->bind(&mExposureMap);
    }

    // shiny-only: exposure + SHINY_ORIGIN + reflection-probe-or-cube environment,
    // all in CAMERA space before the clone transforms.
    S32 shiny_diffuse_channel = 0;   // indexed shaders use channel 0 diffuse base
    S32 shiny_env_channel = -1;
    LLCubeMap* shiny_cube = nullptr;
    bool shiny_probe_path = false;
    if (is_shiny)
    {
        LLMatrix4 cam_mv;
        cam_mv.initRows(LLVector4(gGLModelView + 0), LLVector4(gGLModelView + 4),
                        LLVector4(gGLModelView + 8), LLVector4(gGLModelView + 12));
        LLVector3 so = LLVector3(gShinyOrigin) * cam_mv;
        LLVector4 so4(so, gShinyOrigin.mV[3]);
        shader->uniform4fv(LLViewerShaderMgr::SHINY_ORIGIN, 1, so4.mV);

        shiny_probe_path = LLPipeline::sReflectionProbesEnabled;
        if (shiny_probe_path)
        {
            bindReflectionProbes(*shader);      // sets the env matrix (camera space)
        }
        else
        {
            shiny_cube = gSky.mVOSkyp ? gSky.mVOSkyp->getCubeMap() : nullptr;
            if (shiny_cube)
            {
                shiny_env_channel =
                    shader->enableTexture(LLViewerShaderMgr::ENVIRONMENT_MAP, LLTexUnit::TT_CUBE_MAP);
                shiny_diffuse_channel = shader->enableTexture(LLViewerShaderMgr::DIFFUSE_MAP);
                if (shiny_env_channel >= 0)
                {
                    shiny_cube->enableTexture(shiny_env_channel);
                    gGL.getTexUnit(shiny_env_channel)->bind(shiny_cube);
                }
            }
            // stock beginFullbrightShiny sets the env matrix whenever probes are
            // disabled, even if the sky cube is momentarily unavailable.
            setEnvMat(*shader);
        }
    }

    for (const LLActorMover::GhostProxy& proxy : queue.mProxies)
    {
        if (!proxy.mFrustumVisible || !proxy.mBatches || proxy.mBatches->empty())
        {
            continue;
        }
        auto it = mGhostSubmissionProgress.find(proxy.mInstanceId);
        if (it == mGhostSubmissionProgress.end()
            || !(it->second.mExpectedForwardSolidStages & stage_bit))
        {
            continue;   // this instance has no batch for this stage
        }
        GhostSubmissionProgress& submission = it->second;
        GhostCategoryProgress& solid = submission.mRiggedSolid;

        {
            ScopedGhostTransform transform(proxy);

            for (const LLActorMover::GhostBatch& batch : *proxy.mBatches)
            {
                if (classifyGhostBatchPhase(batch.mPass) != want_phase)
                {
                    continue;
                }
                LLDrawInfo* di = batch.mInfo;
                if (!di || di->mVertexBuffer.isNull() || !di->mCount
                    || !di->mAvatar || !di->mSkinInfo)
                {
                    solid.mFailed = true;
                    continue;
                }
                if (!LLRenderPass::uploadMatrixPalette(*di))
                {
                    solid.mFailed = true;
                    continue;
                }

                bool drew = false;
                if (is_shiny)
                {
                    const bool indexed = di->mTextureList.size() > 1
                        && LLGLSLShader::sIndexedTextureChannels > 1;
                    drew = pushGhostFullbrightShinyBatch(
                        *di, indexed ? 0 : shiny_diffuse_channel, indexed);
                }
                else
                {
                    if (is_masked)
                    {
                        shader->setMinimumAlpha(di->mAlphaMaskCutoff);
                    }
                    drew = pushGhostBatch(*di, true);
                }

                if (!drew)
                {
                    solid.mFailed = true;
                    continue;
                }

                ++counters.mActualDrawCalls;
                ++counters.mRiggedSolidDrawCalls;
                ++solid.mDrawn;
                if (!submission.mAnyDrawSubmitted)
                {
                    submission.mAnyDrawSubmitted = true;
                    ++counters.mProxiesSubmitted;
                }
            }
        }   // ScopedGhostTransform restores the camera modelview

        submission.mCompletedForwardSolidStages |= stage_bit;
    }

    // ---- teardown: unbind every sampler the stage touched (the invariant checker
    // does not track individual texture bindings) + restore saved state. ----
    if (is_shiny)
    {
        if (shiny_probe_path)
        {
            unbindReflectionProbes(*shader);
        }
        else if (shiny_cube)
        {
            shiny_cube->disable();
            shiny_cube->restoreMatrix();
            if (shiny_env_channel >= 0)
            {
                shader->disableTexture(LLViewerShaderMgr::ENVIRONMENT_MAP, LLTexUnit::TT_CUBE_MAP);
            }
            shader->disableTexture(LLViewerShaderMgr::DIFFUSE_MAP);
        }
    }
    if (exposure_channel >= 0)
    {
        shader->disableTexture(LLShaderMgr::EXPOSURE_MAP);
    }

    LLVertexBuffer::unbind();
    LLGLSLShader::unbind();

    gGL.getTexUnit(saved_texture_unit)->activate();
    gGL.setColorMask(saved_color_mask[0] != GL_FALSE, saved_color_mask[3] != GL_FALSE);
    gGL.setSceneBlendType(LLRender::BT_ALPHA);      // leave standard blend state

    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGLLastMatrix = nullptr;
    gGL.loadMatrix(gGLModelView);
    gGL.syncMatrices();
}

void LLPipeline::renderGeomDeferred(LLCamera& camera, bool do_occlusion)
{
    LLAppViewer::instance()->pingMainloopTimeout("Pipeline:RenderGeomDeferred");
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL; //LL_RECORD_BLOCK_TIME(FTM_RENDER_GEOMETRY);
    LL_PROFILE_GPU_ZONE("renderGeomDeferred");

    llassert(!sRenderingHUDs);

    if (gUseWireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    if (&camera == LLViewerCamera::getInstance())
    {   // a bit hacky, this is the start of the main render frame, figure out delta between last modelview matrix and
        // current modelview matrix
        glm::mat4 last_modelview = get_last_modelview();
        glm::mat4 cur_modelview = get_current_modelview();

        // goal is to have a matrix here that goes from the last frame's camera space to the current frame's camera space
        glm::mat4 m = glm::inverse(last_modelview);  // last camera space to world space
        m = cur_modelview * m; // world space to camera space

        glm::mat4 n = glm::inverse(m);

        gGLDeltaModelView = m;
        gGLInverseDeltaModelView = n;
    }

    bool occlude = LLPipeline::sUseOcclusion > 1 && do_occlusion && !LLGLSLShader::sProfileEnabled;

    setupHWLights();

    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DRAWPOOL("deferred pools");

        LLGLEnable cull(GL_CULL_FACE);

        for (pool_set_t::iterator iter = mPools.begin(); iter != mPools.end(); ++iter)
        {
            LLDrawPool *poolp = *iter;
            if (hasRenderType(poolp->getType()))
            {
                poolp->prerender();
            }
        }

        LLVertexBuffer::unbind();

        LLGLState::checkStates();

        if (LLViewerShaderMgr::instance()->mShaderLevel[LLViewerShaderMgr::SHADER_DEFERRED] > 1 &&
            !sPrismLensRender)
        {
            // Probe selection writes camera-space CPU data and the shared UBO.
            // Prism stages a default-probe-only UBO in its outer state scope;
            // never replace the main-eye selection from the auxiliary camera.
            mReflectionMapManager.updateUniforms();
            mHeroProbeManager.updateUniforms();
        }

        U32 cur_type = 0;

        gGL.setColorMask(true, true);

        pool_set_t::iterator iter1 = mPools.begin();

        while ( iter1 != mPools.end() )
        {
            LLDrawPool *poolp = *iter1;

            cur_type = poolp->getType();

            if (occlude && cur_type >= LLDrawPool::POOL_GRASS)
            {
                llassert(!gCubeSnapshot); // never do occlusion culling on cube snapshots
                occlude = false;
                gGLLastMatrix = NULL;
                gGL.loadMatrix(gGLModelView);
                doOcclusion(camera);
            }

            pool_set_t::iterator iter2 = iter1;
            if (hasRenderType(poolp->getType()) && poolp->getNumDeferredPasses() > 0)
            {
                LL_PROFILE_ZONE_NAMED_CATEGORY_DRAWPOOL("deferred pool render");

                gGLLastMatrix = NULL;
                gGL.loadMatrix(gGLModelView);

                for( S32 i = 0; i < poolp->getNumDeferredPasses(); i++ )
                {
                    LLVertexBuffer::unbind();
                    poolp->beginDeferredPass(i);
                    for (iter2 = iter1; iter2 != mPools.end(); iter2++)
                    {
                        LLDrawPool *p = *iter2;
                        if (p->getType() != cur_type)
                        {
                            break;
                        }

                        if ( !p->getSkipRenderFlag() ) { p->renderDeferred(i); }
                    }
                    poolp->endDeferredPass(i);
                    LLVertexBuffer::unbind();

                    LLGLState::checkStates();
                }
            }
            else
            {
                // Skip all pools of this type
                for (iter2 = iter1; iter2 != mPools.end(); iter2++)
                {
                    LLDrawPool *p = *iter2;
                    if (p->getType() != cur_type)
                    {
                        break;
                    }
                }
            }
            iter1 = iter2;
            stop_glerror();
        }

        gGLLastMatrix = NULL;
        gGL.matrixMode(LLRender::MM_MODELVIEW);
        gGL.loadMatrix(gGLModelView);

        gGL.setColorMask(true, false);

    } // Tracy ZoneScoped

    if (gUseWireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}

// [BDMerge A5.4-1a] Velocity / motion-vector geometry pass. Donor: Black Dragon
// LLPipeline::renderGeomMotionBlur (pipeline.cpp:4305-4334). Re-rasterizes the
// opaque scene into mVelocityMap (RG16F) writing per-pixel screen-space velocity
// (current NDC - previous NDC). Depth-tests (no write) against the shared
// deferred depth so only visible surfaces are stamped. Each draw pool emits its
// own velocity via the getNumVelocityPasses / begin / render / endVelocityPass
// hooks (rigid + camera since Phase 1a; rigged/skinned + classic avatar since
// Phase 1b -- prev-frame palettes via MatrixPaletteCache.mLastGLMp and
// llviewerjointmesh.mLastMatrixPalette).
//
// Exclusions: blended alpha (order-dependent, double-stamp hazard), HUD/UI (this
// runs before UI compositing), and cube snapshots / reflection probes (guarded by
// the caller's !gCubeSnapshot). First-frame / no-prev-matrix drawables emit zero
// velocity via the identity fallback in pushVelocityBatches.
void LLPipeline::renderGeomVelocity()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    LL_PROFILE_GPU_ZONE("renderGeomVelocity");

    if (!mVelocityMap.isComplete())
    { // buffer not allocated (e.g. setting toggled without a buffer realloc yet)
        return;
    }

    mVelocityMap.bindTarget();
    mVelocityMap.clear(GL_COLOR_BUFFER_BIT);

    bool projection_discontinuity = !sHasLastVelocityProjection;
    if (sHasLastVelocityProjection)
    {
        F32 delta_squared = 0.f;
        F32 magnitude_squared = 0.f;
        for (U32 i = 0; i < 16; ++i)
        {
            const F32 delta = mVelocityProjMat[i] - sLastVelocityProjMat[i];
            delta_squared += delta * delta;
            magnitude_squared += llmax(mVelocityProjMat[i] * mVelocityProjMat[i],
                                       sLastVelocityProjMat[i] * sLastVelocityProjMat[i]);
        }
        // A >=10% relative Frobenius jump is a discontinuous lens/viewport
        // step. Smooth FOV pulls remain well below this per frame and retain
        // history; the true previous projection still makes their motion valid.
        projection_discontinuity =
            delta_squared > 0.01f * llmax(magnitude_squared, 0.000001f);
    }
    const F32* last_projection =
        sHasLastVelocityProjection ? sLastVelocityProjMat : mVelocityProjMat;
    if (projection_discontinuity)
    {
        // Missing history falls back to current projection; a large single-frame
        // jump uses the true previous endpoint but tells consumers to reset.
        LLReShadeBridge::instance().noteProjectionChange();
    }

    // This uniform is intentionally named rather than added to the global
    // reserved-uniform table: all affected programs are local to this pass,
    // and uploading once here persists across their later draw-pool binds.
    LLGLSLShader* velocity_programs[] =
    {
        &gVelocityProgram, &gVelocityAlphaProgram,
        &gVelocitySkinnedProgram, &gVelocityAlphaSkinnedProgram,
        &gAvatarVelocityProgram, &gVelocityCameraProgram
    };
    for (LLGLSLShader* shader : velocity_programs)
    {
        if (shader->isComplete())
        {
            shader->bind();
            shader->uniformMatrix4fv(sLastProjectionMatrixUnjittered,
                                     1, GL_FALSE, last_projection);
            shader->unbind();
        }
    }

    gGL.setColorMask(true, true);
    LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_LEQUAL); // test against scene depth, no write

    // [BDMerge A5.4-1c] Camera-motion fallback FIRST: fill every pixel with the
    // camera-induced motion reprojected from scene depth, so pixels no draw pool
    // stamps (sky, excluded blended alpha, impostors) don't read "static" and
    // ghost in temporal consumers (SMAA T2x, ReShade bridge/RTGI). Rigged and
    // classic avatars get true limb velocity from the geometry passes (1b).
    // The geometry passes below then overwrite covered pixels with true
    // per-object motion. Depth test/write fully off: we shade all pixels and the
    // shared depth attachment is only SAMPLED (legal: no depth writes occur).
    if (gVelocityCameraProgram.isComplete())
    {
        LLGLDepthTest fallback_no_depth(GL_FALSE, GL_FALSE);

        // deterministic camera matrices for the auto-fed inv_proj/inv_modelview
        gGLLastMatrix = NULL;
        gGL.matrixMode(LLRender::MM_MODELVIEW);
        gGL.loadMatrix(gGLModelView);

        gVelocityCameraProgram.bind();
        gVelocityCameraProgram.bindTexture(LLShaderMgr::DEFERRED_DEPTH, &mRT->deferredScreen, true);
        gVelocityCameraProgram.uniformMatrix4fv(LLShaderMgr::LAST_MODELVIEW_MATRIX, 1, GL_FALSE, gGLLastModelView);
        gVelocityCameraProgram.uniformMatrix4fv(LLShaderMgr::PROJECTION_MATRIX_UNJITTERED, 1, GL_FALSE, mVelocityProjMat);

        mScreenTriangleVB->setBuffer();
        mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

        gVelocityCameraProgram.unbind();
        LLReShadeBridge::instance().noteMotionCoverage(
            SLRESHADE_MOTION_COVERAGE_CAMERA_DEPTH |
            SLRESHADE_MOTION_COVERAGE_SKY);
    }

    sVelocityRender = true;

    // Each draw pool is responsible for producing its own velocity.
    for (pool_set_t::iterator iter = mPools.begin(); iter != mPools.end(); ++iter)
    {
        LLDrawPool* poolp = *iter;
        S32 num_passes = poolp->getNumVelocityPasses();
        for (S32 i = 0; i < num_passes; ++i)
        {
            poolp->beginVelocityPass(i);
            poolp->renderVelocity(i);
            poolp->endVelocityPass(i);
        }
    }

    sVelocityRender = false;

    // These are the categories for which the geometry loop above has live
    // velocity hooks. Coverage is "attempted", not inferred from pixel values.
    LLReShadeBridge::instance().noteMotionCoverage(
        SLRESHADE_MOTION_COVERAGE_RIGID |
        SLRESHADE_MOTION_COVERAGE_RIGGED_MESH |
        SLRESHADE_MOTION_COVERAGE_CLASSIC_AVATAR |
        SLRESHADE_MOTION_COVERAGE_ALPHA_TEST |
        SLRESHADE_MOTION_COVERAGE_FULLBRIGHT);

    gGLLastMatrix = NULL;
    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGL.loadMatrix(gGLModelView);

    mVelocityMap.flush();
}

// Render all of our geometry that's required after our deferred pass.
// This is gonna be stuff like alpha, water, etc.
void LLPipeline::renderGeomPostDeferred(LLCamera& camera)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    LL_PROFILE_GPU_ZONE("renderGeomPostDeferred");

    if (gUseWireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    U32 cur_type = 0;

    LLGLEnable cull(GL_CULL_FACE);

    // Visible-diffuse sidecar. renderGeomPostDeferred has other callers
    // (impostors, HUDs, reflection/hero probes), so gate on target identity
    // rather than on the setting alone (H4), and additionally exclude probe
    // snapshots and HUD renders outright (S1) -- the sidecar describes the
    // main view, and a probe pass writing it would publish the probe's view
    // as the player's.
    static LLCachedControl<bool> visible_diffuse(gSavedSettings, "RenderVisibleDiffuseSidecar", false);
    const bool publish_visible_diffuse =
        visible_diffuse && mRT == &mMainRT &&
        // The seed program is optional and may have failed to compile on this
        // driver. Without it nothing classified the deferred-opaque pixels, so
        // writing forward surfaces into the attachment would produce a buffer
        // that is correct in the minority of pixels and unwritten in the rest.
        // Match the seed pass's own predicate exactly, or the two disagree and
        // the readiness latch is asserted for a frame that was never seeded.
        gVisibleDiffuseSeedProgram.isComplete() &&
        !gCubeSnapshot && !LLPipeline::sRenderingHUDs && !sImpostorRender &&
        LLRenderTarget::getCurrentBoundTarget() == &mRT->screen &&
        mRT->screen.getNumTextures() > SL_COVERAGE_ATTACHMENT;

    // Arm the indexed guard CLOSED (attachment 1 write-disabled) for the whole
    // pass. From here on nothing writes the sidecar unless it explicitly opens
    // the guard, and -- crucially -- every global glColorMask/glBlendFunc
    // issued by any pool or helper in between is followed by an automatic
    // reassert inside LLRender. That is what contains doAtmospherics /
    // doWaterHaze (M1), the glow pool (M2) and the alpha pool's emissive
    // sub-passes (M3), none of which are reachable from this function's own
    // call sites.
    // ONE owner guarding BOTH attachments -- not two nested guards. A nested
    // guard's destructor would clear the outer owner's state instead of
    // restoring it, leaving the outer pass silently unguarded from that point.
    static const U32 sSidecarGuarded[] = { SL_SIDECAR_ATTACHMENT, SL_COVERAGE_ATTACHMENT };
    LLScopedIndexedDrawBufferGuard sidecar_guard(
        publish_visible_diffuse, sSidecarGuarded, LL_ARRAY_SIZE(sSidecarGuarded));

    LLGLEnable visible_diffuse_srgb(
        publish_visible_diffuse ? GL_FRAMEBUFFER_SRGB : 0);

    bool done_atmospherics = LLPipeline::sRenderingHUDs; //skip atmospherics on huds
    bool done_water_haze = done_atmospherics;
    bool done_water_exclusion = false;

    // do water exclusion just before water pass.
    U32 water_exclusion_pass = LLDrawPool::POOL_WATEREXCLUSION;

    // do atmospheric haze just before post water alpha
    U32 atmospherics_pass = LLDrawPool::POOL_ALPHA_POST_WATER;

    if (LLPipeline::sUnderWaterRender)
    { // if under water, do atmospherics just before the water pass
        atmospherics_pass = LLDrawPool::POOL_WATER;
    }

    // do water haze just before pre water alpha
    U32 water_haze_pass = LLDrawPool::POOL_ALPHA_PRE_WATER;

    calcNearbyLights(camera);
    setupHWLights();

    gGL.setSceneBlendType(LLRender::BT_ALPHA);
    gGL.setColorMask(true, false);

    pool_set_t::iterator iter1 = mPools.begin();

    if (gDebugGL || gDebugPipeline)
    {
        LLGLState::checkStates(GL_FALSE);
    }

    // turn off atmospherics and water haze for low detail reflection probe
    static LLCachedControl<S32> probe_level(gSavedSettings, "RenderReflectionProbeLevel", 0);
    bool low_detail_probe = probe_level == 0 && gCubeSnapshot;
    done_atmospherics = done_atmospherics || low_detail_probe;
    done_water_haze   = done_water_haze || low_detail_probe;


    while ( iter1 != mPools.end() )
    {
        LLDrawPool *poolp = *iter1;

        cur_type = poolp->getType();

        if (cur_type >= water_exclusion_pass && !done_water_exclusion)
        { // do water exclusion against depth buffer before rendering alpha
            doWaterExclusionMask();
            done_water_exclusion = true;
        }

        if (cur_type >= atmospherics_pass && !done_atmospherics)
        { // do atmospherics against depth buffer before rendering alpha
            doAtmospherics();
            done_atmospherics = true;
        }

        if (cur_type >= water_haze_pass && !done_water_haze)
        { // do water haze against depth buffer before rendering alpha
            doWaterHaze();
            done_water_haze = true;
        }

        pool_set_t::iterator iter2 = iter1;
        if (hasRenderType(poolp->getType()) && poolp->getNumPostDeferredPasses() > 0)
        {
            LL_PROFILE_ZONE_NAMED_CATEGORY_DRAWPOOL("deferred poolrender");

            gGLLastMatrix = NULL;
            gGL.loadMatrix(gGLModelView);

            for( S32 i = 0; i < poolp->getNumPostDeferredPasses(); i++ )
            {
                LLVertexBuffer::unbind();
                const bool contributes_diffuse =
                    cur_type == LLDrawPool::POOL_FULLBRIGHT ||
                    cur_type == LLDrawPool::POOL_FULLBRIGHT_ALPHA_MASK ||
                    cur_type == LLDrawPool::POOL_ALPHA_PRE_WATER ||
                    cur_type == LLDrawPool::POOL_ALPHA_POST_WATER ||
                    // S3: water publishes zero diffuse at K=1 (specular-only
                    // BRDF). Both the above-water and underwater programs
                    // declare the extra outputs, so opening the guard here
                    // cannot produce undefined writes.
                    cur_type == LLDrawPool::POOL_WATER;
                // Open the guard only for the pools that actually resolve
                // visible diffuse. Everything else -- including whatever
                // global state beginPostDeferredPass issues, which no longer
                // needs a manual reassert after it -- runs with attachment 1
                // write-disabled.
                if (contributes_diffuse)
                {
                    gGL.setIndexedDrawBufferGuardMask(SL_SIDECAR_ATTACHMENT, true, true, true, true);
                    gGL.setIndexedDrawBufferGuardBlend(SL_SIDECAR_ATTACHMENT, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                                                       GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

                    // Coverage: open the FORWARD channel (G) only. R is the
                    // deferred coverage the seed pass wrote, and the forward
                    // shaders emit vec2(0.0, alpha) -- leaving R writable would
                    // let every forward fragment ZERO the seed's answer.
                    gGL.setIndexedDrawBufferGuardMask(SL_COVERAGE_ATTACHMENT, false, true, false, false);

                    // Coverage accumulates as a UNION, not a sum:
                    //     Gout = Gsrc + Gdst * (1 - Gsrc)
                    // GL_ONE_MINUS_SRC_COLOR supplies (1 - Gsrc) on the G
                    // channel, so overlapping forward layers saturate toward 1
                    // instead of adding past it.
                    gGL.setIndexedDrawBufferGuardBlend(SL_COVERAGE_ATTACHMENT,
                                                       GL_ONE, GL_ONE_MINUS_SRC_COLOR,
                                                       GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                }
                poolp->beginPostDeferredPass(i);
                for (iter2 = iter1; iter2 != mPools.end(); iter2++)
                {
                    LLDrawPool *p = *iter2;
                    if (p->getType() != cur_type)
                    {
                        break;
                    }

                    p->renderPostDeferred(i);
                }
                poolp->endPostDeferredPass(i);
                gGL.setIndexedDrawBufferGuardMask(SL_SIDECAR_ATTACHMENT, false, false, false, false);
                gGL.clearIndexedDrawBufferGuardBlend(SL_SIDECAR_ATTACHMENT);
                gGL.setIndexedDrawBufferGuardMask(SL_COVERAGE_ATTACHMENT, false, false, false, false);
                gGL.clearIndexedDrawBufferGuardBlend(SL_COVERAGE_ATTACHMENT);
                LLVertexBuffer::unbind();

                if (gDebugGL || gDebugPipeline)
                {
                    LLGLState::checkStates(GL_FALSE);
                }
            }
        }
        else
        {
            // Skip all pools of this type
            for (iter2 = iter1; iter2 != mPools.end(); iter2++)
            {
                LLDrawPool *p = *iter2;
                if (p->getType() != cur_type)
                {
                    break;
                }
            }
        }
        iter1 = iter2;
        stop_glerror();
    }

    gGLLastMatrix = NULL;
    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGL.loadMatrix(gGLModelView);

    if (!gCubeSnapshot)
    {
        // debug displays
        renderHighlights();
        mHighlightFaces.clear();

        renderDebug();
    }

    if (gUseWireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    if (publish_visible_diffuse)
    {
        // Assert validity only now, having actually completed the forward pool
        // loop over the main view's sidecar attachment. The bridge must never
        // infer this from the attachment merely existing. (M4)
        LLReShadeBridge::instance().noteVisibleDiffuseResolved();
        LLReShadeBridge::instance().noteSurfaceCoverageResolved();
    }
    // sidecar_guard disarms both attachments here.
}

void LLPipeline::renderGeomShadow(LLCamera& camera)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    LL_PROFILE_GPU_ZONE("renderGeomShadow");
    U32 cur_type = 0;

    LLGLEnable cull(GL_CULL_FACE);

    LLVertexBuffer::unbind();

    pool_set_t::iterator iter1 = mPools.begin();

    while ( iter1 != mPools.end() )
    {
        LLDrawPool *poolp = *iter1;

        cur_type = poolp->getType();

        pool_set_t::iterator iter2 = iter1;
        if (hasRenderType(poolp->getType()) && poolp->getNumShadowPasses() > 0)
        {
            poolp->prerender() ;

            gGLLastMatrix = NULL;
            gGL.loadMatrix(gGLModelView);

            for( S32 i = 0; i < poolp->getNumShadowPasses(); i++ )
            {
                LLVertexBuffer::unbind();
                poolp->beginShadowPass(i);
                for (iter2 = iter1; iter2 != mPools.end(); iter2++)
                {
                    LLDrawPool *p = *iter2;
                    if (p->getType() != cur_type)
                    {
                        break;
                    }

                    p->renderShadow(i);
                }
                poolp->endShadowPass(i);
                LLVertexBuffer::unbind();
            }
        }
        else
        {
            // Skip all pools of this type
            for (iter2 = iter1; iter2 != mPools.end(); iter2++)
            {
                LLDrawPool *p = *iter2;
                if (p->getType() != cur_type)
                {
                    break;
                }
            }
        }
        iter1 = iter2;
        stop_glerror();
    }

    gGLLastMatrix = NULL;
    gGL.loadMatrix(gGLModelView);
}


static U32 sIndicesDrawnCount = 0;

void LLPipeline::addTrianglesDrawn(S32 index_count)
{
    sIndicesDrawnCount += index_count;
}

void LLPipeline::recordTrianglesDrawn()
{
    assertInitialized();
    U32 count = sIndicesDrawnCount / 3;
    sIndicesDrawnCount = 0;
    add(LLStatViewer::TRIANGLES_DRAWN, LLUnits::Triangles::fromValue(count));
}

void LLPipeline::renderPhysicsDisplay()
{
    if (!hasRenderDebugMask(LLPipeline::RENDER_DEBUG_PHYSICS_SHAPES))
    {
        return;
    }

    gGL.flush();
    gDebugProgram.bind();

    LLGLEnable polygon_offset_line(GL_POLYGON_OFFSET_LINE);
    glPolygonOffset(3.f, 3.f);
    gGL.setLineWidth(3.f);
    LLGLEnable blend(GL_BLEND);
    gGL.setSceneBlendType(LLRender::BT_ALPHA);

    for (int pass = 0; pass < 3; ++pass)
    {
        // pass 0 - depth write enabled, color write disabled, fill
        // pass 1 - depth write disabled, color write enabled, fill
        // pass 2 - depth write disabled, color write enabled, wireframe
        gGL.setColorMask(pass >= 1, false);
        LLGLDepthTest depth(GL_TRUE, pass == 0);

        bool wireframe = (pass == 2);

        if (wireframe)
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        }

        for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin();
            iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
        {
            LLViewerRegion* region = *iter;
            for (U32 i = 0; i < LLViewerRegion::NUM_PARTITIONS; i++)
            {
                LLSpatialPartition* part = region->getSpatialPartition(i);
                if (part)
                {
                    if (hasRenderType(part->mDrawableType))
                    {
                        part->renderPhysicsShapes(wireframe);
                    }
                }
            }
        }
        gGL.flush();

        if (wireframe)
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
    }
    gGL.setLineWidth(1.f);
    gDebugProgram.unbind();

}

// [F4] DoF focus point crosshair (donor: Firestorm I:\enve
// indra/newview/pipeline.cpp LLPipeline::renderFocusPoint, FIRE-32023; BD/
// Alchemy merge campaign item F4). Bound to a new key, RenderFocusPointCrosshair
// (donor: FSFocusPointRender), since Alchemy already ports the lock/follow
// pair as RenderFocusPointLocked/RenderFocusPointFollowsPointer but has no
// crosshair render toggle of its own. UI-pass only, gated on sDoFEnabled and
// RENDER_DEBUG_FEATURE_UI so it is excluded from snapshots the same way FS
// excludes it (see ADAPTATION RULE 5) - show_ui=false snapshots flip that
// mask off around the raw capture (llviewerwindow.cpp rawSnapshot()).
void LLPipeline::renderFocusPoint()
{
    static LLCachedControl<bool> render_focus_point_crosshair(gSavedSettings, "RenderFocusPointCrosshair", false);
    if (sDoFEnabled && render_focus_point_crosshair && gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))
    {
        gDebugProgram.bind();
        LLVector3 focus_point = sLastFocusPoint;
        F32 size = 0.02f;
        LLGLDepthTest gls_depth(GL_FALSE);
        gGL.pushMatrix();
        gGL.translatef(focus_point.mV[VX], focus_point.mV[VY], focus_point.mV[VZ]);

        gGL.begin(LLRender::LINES);
        static LLCachedControl<bool> render_focus_point_locked(gSavedSettings, "RenderFocusPointLocked", false);
        if (render_focus_point_locked)
        {
            gGL.color4f(1.0f, 0.0f, 0.0f, 0.5f);
        }
        else
        {
            gGL.color4f(1.0f, 1.0f, 0.0f, 0.5f);
        }
        gGL.vertex3f(-size, 0.0f, 0.0f);
        gGL.vertex3f(size, 0.0f, 0.0f);

        // Y-axis (Green)
        gGL.vertex3f(0.0f, -size, 0.0f);
        gGL.vertex3f(0.0f, size, 0.0f);

        // Z-axis (Blue)
        gGL.vertex3f(0.0f, 0.0f, -size);
        gGL.vertex3f(0.0f, 0.0f, size);

        gGL.end();

        gGL.popMatrix();
        gGL.flush();
        gDebugProgram.unbind();
    }
}

// [F8] Rule-of-thirds / golden-ratio / diagonal composition guide overlay
// (donor: Firestorm I:\enve indra/newview/pipeline.cpp
// LLPipeline::renderSnapshotGuidesOverlay; campaign item F8). The donor
// version only populated its guide state from a companion shader-based
// "capture frame" pass (FSSnapshotShowCaptureFrame / gPostSnapshotFrameProgram)
// tied to the snapshot floater's crop rectangle; that shader pass was not
// ported (out of spec scope - see F4/F8 campaign report pruned-controls
// list), so this reads the new debug settings directly every frame and draws
// full-viewport guides whenever RenderCompositionGuide is on, independent of
// any snapshot floater being open. UI-pass only, gated on
// RENDER_DEBUG_FEATURE_UI so guides are excluded from snapshots the same way
// FS excludes them (see ADAPTATION RULE 5), and matching precedent set by
// [BDMerge C6] BDMergeSnapshotExtras's UI-exclusion handling elsewhere in the
// snapshot path (llviewerwindow.cpp rawSnapshot()).
void LLPipeline::renderCompositionGuideOverlay()
{
    static LLCachedControl<bool> show_guides(gSavedSettings, "RenderCompositionGuide", false);
    if (!show_guides || !gViewerWindow || !gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))
    {
        return;
    }

    LLRect view_rect = gViewerWindow->getWorldViewRectRaw();
    const F32 width = (F32)view_rect.getWidth();
    const F32 height = (F32)view_rect.getHeight();
    if (width <= 0.f || height <= 0.f)
    {
        return;
    }

    static LLCachedControl<LLColor3> guide_color(gSavedSettings, "RenderCompositionGuideColor", LLColor3(1.f, 1.f, 0.f));
    static LLCachedControl<F32> guide_thickness(gSavedSettings, "RenderCompositionGuideWidth", 2.0f);
    static LLCachedControl<F32> guide_visibility(gSavedSettings, "RenderCompositionGuideVisibility", 0.5f);
    static LLCachedControl<std::string> guide_style_setting(gSavedSettings, "RenderCompositionGuideStyle", std::string("rule_of_thirds"));

    enum class GuideStyle : U8 { RuleOfThirds, GoldenRatio, Diagonal };
    enum class GoldenOrientation : U8 { TopLeft, TopRight, BottomLeft, BottomRight };

    GuideStyle guide_style = GuideStyle::RuleOfThirds;
    GoldenOrientation golden_orientation = GoldenOrientation::TopLeft;
    const std::string style_value = guide_style_setting();
    if (style_value == "golden_ratio_top_left")
    {
        guide_style = GuideStyle::GoldenRatio;
        golden_orientation = GoldenOrientation::TopLeft;
    }
    else if (style_value == "golden_ratio_top_right")
    {
        guide_style = GuideStyle::GoldenRatio;
        golden_orientation = GoldenOrientation::TopRight;
    }
    else if (style_value == "golden_ratio_bottom_left")
    {
        guide_style = GuideStyle::GoldenRatio;
        golden_orientation = GoldenOrientation::BottomLeft;
    }
    else if (style_value == "golden_ratio_bottom_right")
    {
        guide_style = GuideStyle::GoldenRatio;
        golden_orientation = GoldenOrientation::BottomRight;
    }
    else if (style_value == "diagonal")
    {
        guide_style = GuideStyle::Diagonal;
    }

    const F32 alpha = llclamp((F32)guide_visibility, 0.f, 1.f);
    if (alpha <= 0.f)
    {
        return;
    }

    // Full viewport - unlike the donor there is no snapshot capture-frame
    // rectangle to inset against (see comment above).
    const F32 left_px = 0.f;
    const F32 right_px = width;
    const F32 bottom_px = 0.f;
    const F32 top_px = height;
    const F32 frame_width = width;
    const F32 frame_height = height;

    LLGLDisable depth(GL_DEPTH_TEST);
    LLGLDisable cull(GL_CULL_FACE);
    LLGLDisable stencil(GL_STENCIL_TEST);
    LLGLEnable blend(GL_BLEND);
    gGL.setSceneBlendType(LLRender::BT_ALPHA);

    LLGLSLShader* ui_shader = &gUIProgram;
    ui_shader->bind();

    if (!LLViewerFetchedTexture::sWhiteImagep.isNull())
    {
        gGL.getTexUnit(0)->bind(LLViewerFetchedTexture::sWhiteImagep);
    }
    else
    {
        gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, LLTexUnit::sWhiteTexture);
    }

    gGL.matrixMode(LLRender::MM_PROJECTION);
    gGL.pushMatrix();
    gGL.loadIdentity();
    gGL.ortho(0.f, width, 0.f, height, -1.f, 1.f);

    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGL.pushMatrix();
    gGL.loadIdentity();
    gGLLastMatrix = nullptr;

    const LLColor4 line_color(guide_color(), alpha);
    gGL.color4fv(line_color.mV);

    const F32 thickness = llmax((F32)guide_thickness, 0.f);
    const F32 half_thickness = thickness * 0.5f;
    auto draw_filled_rect = [&](F32 l, F32 b, F32 r, F32 t)
    {
        const S32 left_i = ll_round(l);
        const S32 right_i = ll_round(r);
        const S32 top_i = ll_round(t);
        const S32 bottom_i = ll_round(b);
        gl_rect_2d(left_i, top_i, right_i, bottom_i, line_color, true);
    };

    auto draw_vertical_norm = [&](F32 norm)
    {
        const F32 x = left_px + frame_width * norm;
        draw_filled_rect(x - half_thickness, bottom_px, x + half_thickness, top_px);
    };

    auto draw_horizontal_norm = [&](F32 norm)
    {
        const F32 y = bottom_px + frame_height * norm;
        draw_filled_rect(left_px, y - half_thickness, right_px, y + half_thickness);
    };

    switch (guide_style)
    {
        case GuideStyle::RuleOfThirds:
        {
            constexpr std::array<F32, 2> offsets = { 1.f / 3.f, 2.f / 3.f };
            for (F32 offset : offsets)
            {
                draw_vertical_norm(offset);
                draw_horizontal_norm(offset);
            }
            break;
        }
        case GuideStyle::GoldenRatio:
        {
            constexpr F32 phi = 1.61803398875f;

            const F32 scale = llmin(frame_width / phi, frame_height);
            if (scale <= 0.f)
            {
                break;
            }

            const F32 golden_width = phi * scale;
            const F32 golden_height = scale;
            const F32 pad_x = frame_width - golden_width;
            const F32 pad_y = frame_height - golden_height;

            F32 anchor_x = left_px;
            F32 anchor_y = bottom_px;
            switch (golden_orientation)
            {
                case GoldenOrientation::TopLeft:
                    anchor_y += pad_y;
                    break;
                case GoldenOrientation::TopRight:
                    anchor_x += pad_x;
                    anchor_y += pad_y;
                    break;
                case GoldenOrientation::BottomRight:
                    anchor_x += pad_x;
                    break;
                case GoldenOrientation::BottomLeft:
                default:
                    break;
            }

            auto map_point = [&](F32 local_x, F32 local_y) -> LLVector2
            {
                F32 x = local_x;
                F32 y = local_y;

                if (golden_orientation == GoldenOrientation::TopLeft ||
                    golden_orientation == GoldenOrientation::BottomLeft)
                {
                    x = golden_width - local_x;
                }

                if (golden_orientation == GoldenOrientation::BottomLeft ||
                    golden_orientation == GoldenOrientation::BottomRight)
                {
                    y = golden_height - local_y;
                }

                return LLVector2(anchor_x + x, anchor_y + y);
            };

            std::vector<std::pair<LLVector2, LLVector2>> line_segments;
            line_segments.reserve(24);

            auto add_line = [&](F32 x0, F32 y0, F32 x1, F32 y1)
            {
                line_segments.emplace_back(map_point(x0, y0), map_point(x1, y1));
            };

            // Outline of the fitted golden rectangle.
            add_line(0.f, 0.f, golden_width, 0.f);
            add_line(0.f, golden_height, golden_width, golden_height);
            add_line(0.f, 0.f, 0.f, golden_height);
            add_line(golden_width, 0.f, golden_width, golden_height);

            // Generate subdivision lines while we walk the squares.
            F32 x0 = 0.f;
            F32 y0 = 0.f;
            F32 x1 = golden_width;
            F32 y1 = golden_height;

            for (U32 step = 0; step < 12; ++step)
            {
                const F32 w = x1 - x0;
                const F32 h = y1 - y0;
                if (w <= 1.f || h <= 1.f)
                {
                    break;
                }

                switch (step % 4)
                {
                    case 0:
                        x0 += h;
                        add_line(x0, y0, x0, y1);
                        break;
                    case 1:
                        y0 += w;
                        add_line(x0, y0, x1, y0);
                        break;
                    case 2:
                        x1 -= h;
                        add_line(x1, y0, x1, y1);
                        break;
                    default:
                        y1 -= w;
                        add_line(x0, y1, x1, y1);
                        break;
                }
            }

            auto draw_golden_spiral = [&](U32 max_depth)
            {
                gGL.begin(LLRender::LINE_STRIP);

                F32 spiral_x0 = 0.f;
                F32 spiral_y0 = 0.f;
                F32 spiral_x1 = golden_width;
                F32 spiral_y1 = golden_height;

                for (U32 step = 0; step < max_depth; ++step)
                {
                    const F32 w = spiral_x1 - spiral_x0;
                    const F32 h = spiral_y1 - spiral_y0;
                    if (w <= 1.f || h <= 1.f)
                    {
                        break;
                    }

                    F32 size = 0.f;
                    F32 cx = 0.f;
                    F32 cy = 0.f;
                    F32 start_angle = 0.f;
                    F32 end_angle = 0.f;

                    switch (step % 4)
                    {
                        case 0: // left square
                            size = h;
                            cx = spiral_x0 + size;
                            cy = spiral_y0 + size;
                            start_angle = F_PI;
                            end_angle = 1.5f * F_PI;
                            spiral_x0 += size;
                            break;
                        case 1: // bottom square
                            size = w;
                            cx = spiral_x0;
                            cy = spiral_y0 + size;
                            start_angle = 1.5f * F_PI;
                            end_angle = 2.f * F_PI;
                            spiral_y0 += size;
                            break;
                        case 2: // right square
                            size = h;
                            cx = spiral_x1 - size;
                            cy = spiral_y0;
                            start_angle = 0.f;
                            end_angle = F_PI_BY_TWO;
                            spiral_x1 -= size;
                            break;
                        case 3: // top square
                        default:
                            size = w;
                            cx = spiral_x0 + size;
                            cy = spiral_y1 - size;
                            start_angle = F_PI_BY_TWO;
                            end_angle = F_PI;
                            spiral_y1 -= size;
                            break;
                    }

                    if (size <= 0.f)
                    {
                        break;
                    }

                    const S32 segments = llclamp((S32)(size / 4.f), 12, 64);
                    for (S32 i = 0; i <= segments; ++i)
                    {
                        const F32 t = start_angle + (end_angle - start_angle) * (F32)i / (F32)segments;
                        const F32 local_x = cx + cosf(t) * size;
                        const F32 local_y = cy + sinf(t) * size;
                        LLVector2 mapped = map_point(local_x, local_y);
                        gGL.vertex2f(mapped.mV[0], mapped.mV[1]);
                    }
                }

                gGL.end();
            };

            gGL.flush();
            const F32 line_width = llmax(thickness, 1.f);
            gGL.setLineWidth(line_width);
            draw_golden_spiral(12);
            gGL.setLineWidth(1.f);

            if (!line_segments.empty())
            {
                gGL.flush();
                gGL.setLineWidth(line_width);
                gGL.begin(LLRender::LINES);
                for (const auto& segment : line_segments)
                {
                    gGL.vertex2f(segment.first.mV[VX], segment.first.mV[VY]);
                    gGL.vertex2f(segment.second.mV[VX], segment.second.mV[VY]);
                }
                gGL.end();
                gGL.setLineWidth(1.f);
            }
            break;
        }
        case GuideStyle::Diagonal:
        {
            const F32 line_width = llmax(thickness, 1.f);
            gGL.flush();
            gGL.setLineWidth(line_width);
            gGL.begin(LLRender::LINES);
            gGL.vertex2f(left_px, bottom_px);
            gGL.vertex2f(right_px, top_px);
            gGL.vertex2f(left_px, top_px);
            gGL.vertex2f(right_px, bottom_px);
            gGL.end();
            gGL.setLineWidth(1.f);
            break;
        }
    }

    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGL.popMatrix();
    gGL.matrixMode(LLRender::MM_PROJECTION);
    gGL.popMatrix();
    gGLLastMatrix = nullptr;

    ui_shader->unbind();
}

extern std::set<LLSpatialGroup*> visible_selected_groups;

void LLPipeline::renderDebug()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;

    assertInitialized();

    bool hud_only = hasRenderType(LLPipeline::RENDER_TYPE_HUD);

    if (!hud_only )
    {
        //Render any navmesh geometry
        LLPathingLib *llPathingLibInstance = LLPathingLib::getInstance();
        if ( llPathingLibInstance != NULL )
        {
            //character floater renderables

            LLHandle<LLFloaterPathfindingCharacters> pathfindingCharacterHandle = LLFloaterPathfindingCharacters::getInstanceHandle();
            if ( !pathfindingCharacterHandle.isDead() )
            {
                LLFloaterPathfindingCharacters *pathfindingCharacter = pathfindingCharacterHandle.get();

                if ( pathfindingCharacter->getVisible() || gAgentCamera.cameraMouselook() )
                {
                    gPathfindingProgram.bind();
                    gPathfindingProgram.uniform1f(sTint, 1.f);
                    gPathfindingProgram.uniform1f(sAmbiance, 1.f);
                    gPathfindingProgram.uniform1f(sAlphaScale, 1.f);

                    //Requried character physics capsule render parameters
                    LLUUID id;
                    LLVector3 pos;
                    LLQuaternion rot;

                    if ( pathfindingCharacter->isPhysicsCapsuleEnabled( id, pos, rot ) )
                    {
                        //remove blending artifacts
                        gGL.setColorMask(false, false);
                        llPathingLibInstance->renderSimpleShapeCapsuleID( gGL, id, pos, rot );
                        gGL.setColorMask(true, false);
                        LLGLEnable blend(GL_BLEND);
                        gPathfindingProgram.uniform1f(sAlphaScale, 0.90f);
                        llPathingLibInstance->renderSimpleShapeCapsuleID( gGL, id, pos, rot );
                        gPathfindingProgram.bind();
                    }
                }
            }


            //pathing console renderables
            LLHandle<LLFloaterPathfindingConsole> pathfindingConsoleHandle = LLFloaterPathfindingConsole::getInstanceHandle();
            if (!pathfindingConsoleHandle.isDead())
            {
                LLFloaterPathfindingConsole *pathfindingConsole = pathfindingConsoleHandle.get();

                if ( pathfindingConsole->getVisible() || gAgentCamera.cameraMouselook() )
                {
                    F32 ambiance = gSavedSettings.getF32("PathfindingAmbiance");

                    gPathfindingProgram.bind();

                    gPathfindingProgram.uniform1f(sTint, 1.f);
                    gPathfindingProgram.uniform1f(sAmbiance, ambiance);
                    gPathfindingProgram.uniform1f(sAlphaScale, 1.f);

                    if ( !pathfindingConsole->isRenderWorld() )
                    {
                        const LLColor4 clearColor = gSavedSettings.getColor4("PathfindingNavMeshClear");
                        gGL.setColorMask(true, true);
                        glClearColor(clearColor.mV[0],clearColor.mV[1],clearColor.mV[2],0);
                        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT); // no stencil -- deprecated | GL_STENCIL_BUFFER_BIT);
                        gGL.setColorMask(true, false);
                        glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
                    }

                    //NavMesh
                    if ( pathfindingConsole->isRenderNavMesh() )
                    {
                        gGL.setLineWidth(2.0f);
                        LLGLEnable cull(GL_CULL_FACE);
                        LLGLDisable blend(GL_BLEND);

                        if ( pathfindingConsole->isRenderWorld() )
                        {
                            LLGLEnable blend(GL_BLEND);
                            gPathfindingProgram.uniform1f(sAlphaScale, 0.66f);
                            llPathingLibInstance->renderNavMesh();
                        }
                        else
                        {
                            llPathingLibInstance->renderNavMesh();
                        }

                        //render edges
                        gPathfindingNoNormalsProgram.bind();
                        gPathfindingNoNormalsProgram.uniform1f(sTint, 1.f);
                        gPathfindingNoNormalsProgram.uniform1f(sAlphaScale, 1.f);
                        llPathingLibInstance->renderNavMeshEdges();
                        gPathfindingProgram.bind();

                        gGL.flush();
                        glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
                        gGL.setLineWidth(1.0f);
                        gGL.flush();
                    }
                    //User designated path
                    if ( LLPathfindingPathTool::getInstance()->isRenderPath() )
                    {
                        //The path
                        gUIProgram.bind();
                        gGL.getTexUnit(0)->bind(LLViewerFetchedTexture::sWhiteImagep);
                        llPathingLibInstance->renderPath();
                        gPathfindingProgram.bind();

                        //The bookends
                        //remove blending artifacts
                        gGL.setColorMask(false, false);
                        llPathingLibInstance->renderPathBookend( gGL, LLPathingLib::LLPL_START );
                        llPathingLibInstance->renderPathBookend( gGL, LLPathingLib::LLPL_END );

                        gGL.setColorMask(true, false);
                        //render the bookends
                        LLGLEnable blend(GL_BLEND);
                        gPathfindingProgram.uniform1f(sAlphaScale, 0.90f);
                        llPathingLibInstance->renderPathBookend( gGL, LLPathingLib::LLPL_START );
                        llPathingLibInstance->renderPathBookend( gGL, LLPathingLib::LLPL_END );
                        gPathfindingProgram.bind();
                    }

                    if ( pathfindingConsole->isRenderWaterPlane() )
                    {
                        LLGLEnable blend(GL_BLEND);
                        gPathfindingProgram.uniform1f(sAlphaScale, 0.90f);
                        llPathingLibInstance->renderSimpleShapes( gGL, gAgent.getRegion()->getWaterHeight() );
                    }
                //physics/exclusion shapes
                if ( pathfindingConsole->isRenderAnyShapes() )
                {
                        U32 render_order[] = {
                            1 << LLPathingLib::LLST_ObstacleObjects,
                            1 << LLPathingLib::LLST_WalkableObjects,
                            1 << LLPathingLib::LLST_ExclusionPhantoms,
                            1 << LLPathingLib::LLST_MaterialPhantoms,
                        };

                        U32 flags = pathfindingConsole->getRenderShapeFlags();

                        for (U32 i = 0; i < 4; i++)
                        {
                            if (!(flags & render_order[i]))
                            {
                                continue;
                            }

                            //turn off backface culling for volumes so they are visible when camera is inside volume
                            LLGLDisable cull(i >= 2 ? GL_CULL_FACE : 0);

                            gGL.flush();
                            glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

                            //get rid of some z-fighting
                            LLGLEnable polyOffset(GL_POLYGON_OFFSET_FILL);
                            glPolygonOffset(1.0f, 1.0f);

                            //render to depth first to avoid blending artifacts
                            gGL.setColorMask(false, false);
                            llPathingLibInstance->renderNavMeshShapesVBO( render_order[i] );
                            gGL.setColorMask(true, false);

                            //get rid of some z-fighting
                            glPolygonOffset(0.f, 0.f);

                            LLGLEnable blend(GL_BLEND);

                            {
                                gPathfindingProgram.uniform1f(sAmbiance, ambiance);

                                { //draw solid overlay
                                    LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_LEQUAL);
                                    llPathingLibInstance->renderNavMeshShapesVBO( render_order[i] );
                                    gGL.flush();
                                }

                                LLGLEnable lineOffset(GL_POLYGON_OFFSET_LINE);
                                glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

                                F32 offset = gSavedSettings.getF32("PathfindingLineOffset");

                                if (pathfindingConsole->isRenderXRay())
                                {
                                    gPathfindingProgram.uniform1f(sTint, gSavedSettings.getF32("PathfindingXRayTint"));
                                    gPathfindingProgram.uniform1f(sAlphaScale, gSavedSettings.getF32("PathfindingXRayOpacity"));
                                    LLGLEnable blend(GL_BLEND);
                                    LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_GREATER);

                                    glPolygonOffset(offset, -offset);

                                    if (gSavedSettings.getBOOL("PathfindingXRayWireframe"))
                                    { //draw hidden wireframe as darker and less opaque
                                        gPathfindingProgram.uniform1f(sAmbiance, 1.f);
                                        llPathingLibInstance->renderNavMeshShapesVBO( render_order[i] );
                                    }
                                    else
                                    {
                                        glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
                                        gPathfindingProgram.uniform1f(sAmbiance, ambiance);
                                        llPathingLibInstance->renderNavMeshShapesVBO( render_order[i] );
                                        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                                    }
                                }

                                { //draw visible wireframe as brighter, thicker and more opaque
                                    glPolygonOffset(offset, offset);
                                    gPathfindingProgram.uniform1f(sAmbiance, 1.f);
                                    gPathfindingProgram.uniform1f(sTint, 1.f);
                                    gPathfindingProgram.uniform1f(sAlphaScale, 1.f);

                                    gGL.setLineWidth(gSavedSettings.getF32("PathfindingLineWidth"));
                                    LLGLDisable blendOut(GL_BLEND);
                                    llPathingLibInstance->renderNavMeshShapesVBO( render_order[i] );
                                    gGL.flush();
                                    gGL.setLineWidth(1.f);
                                }

                                glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
                            }
                        }
                    }

                    glPolygonOffset(0.f, 0.f);

                    if ( pathfindingConsole->isRenderNavMesh() && pathfindingConsole->isRenderXRay() )
                    {   //render navmesh xray
                        F32 ambiance = gSavedSettings.getF32("PathfindingAmbiance");

                        LLGLEnable lineOffset(GL_POLYGON_OFFSET_LINE);
                        LLGLEnable polyOffset(GL_POLYGON_OFFSET_FILL);

                        F32 offset = gSavedSettings.getF32("PathfindingLineOffset");
                        glPolygonOffset(offset, -offset);

                        LLGLEnable blend(GL_BLEND);
                        LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_GREATER);
                        gGL.flush();
                        gGL.setLineWidth(2.0f);
                        LLGLEnable cull(GL_CULL_FACE);

                        gPathfindingProgram.uniform1f(sTint, gSavedSettings.getF32("PathfindingXRayTint"));
                        gPathfindingProgram.uniform1f(sAlphaScale, gSavedSettings.getF32("PathfindingXRayOpacity"));

                        if (gSavedSettings.getBOOL("PathfindingXRayWireframe"))
                        { //draw hidden wireframe as darker and less opaque
                            glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
                            gPathfindingProgram.uniform1f(sAmbiance, 1.f);
                            llPathingLibInstance->renderNavMesh();
                            glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
                        }
                        else
                        {
                            gPathfindingProgram.uniform1f(sAmbiance, ambiance);
                            llPathingLibInstance->renderNavMesh();
                        }

                        //render edges
                        gPathfindingNoNormalsProgram.bind();
                        gPathfindingNoNormalsProgram.uniform1f(sTint, gSavedSettings.getF32("PathfindingXRayTint"));
                        gPathfindingNoNormalsProgram.uniform1f(sAlphaScale, gSavedSettings.getF32("PathfindingXRayOpacity"));
                        llPathingLibInstance->renderNavMeshEdges();
                        gPathfindingProgram.bind();

                        gGL.flush();
                        gGL.setLineWidth(1.0f);
                    }

                    glPolygonOffset(0.f, 0.f);

                    gGL.flush();
                    gPathfindingProgram.unbind();
                }
            }
        }
    }

    gGLLastMatrix = NULL;
    gGL.loadMatrix(gGLModelView);
    gGL.setColorMask(true, false);


    if (!hud_only && !mDebugBlips.empty())
    { //render debug blips
        gUIProgram.bind();
        gGL.color4f(1, 1, 1, 1);

        gGL.getTexUnit(0)->bind(LLViewerFetchedTexture::sWhiteImagep, true);

        glPointSize(8.f);
        LLGLDepthTest depth(GL_TRUE, GL_TRUE, GL_ALWAYS);

        gGL.begin(LLRender::POINTS);
        for (std::list<DebugBlip>::iterator iter = mDebugBlips.begin(); iter != mDebugBlips.end(); )
        {
            DebugBlip& blip = *iter;

            blip.mAge += gFrameIntervalSeconds.value();
            if (blip.mAge > 2.f)
            {
                mDebugBlips.erase(iter++);
            }
            else
            {
                iter++;
            }

            blip.mPosition.mV[2] += gFrameIntervalSeconds.value()*2.f;

            gGL.color4fv(blip.mColor.mV);
            gGL.vertex3fv(blip.mPosition.mV);
        }
        gGL.end();
        gGL.flush();
        glPointSize(1.f);
    }

    // Debug stuff.
    if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_OCTREE |
        LLPipeline::RENDER_DEBUG_OCCLUSION |
        LLPipeline::RENDER_DEBUG_LIGHTS |
        LLPipeline::RENDER_DEBUG_BATCH_SIZE |
        LLPipeline::RENDER_DEBUG_UPDATE_TYPE |
        LLPipeline::RENDER_DEBUG_BBOXES |
        LLPipeline::RENDER_DEBUG_NORMALS |
        LLPipeline::RENDER_DEBUG_POINTS |
        LLPipeline::RENDER_DEBUG_TEXTURE_AREA |
        LLPipeline::RENDER_DEBUG_TEXTURE_ANIM |
        LLPipeline::RENDER_DEBUG_RAYCAST |
        LLPipeline::RENDER_DEBUG_AVATAR_VOLUME |
        LLPipeline::RENDER_DEBUG_AVATAR_JOINTS |
        LLPipeline::RENDER_DEBUG_AGENT_TARGET |
        LLPipeline::RENDER_DEBUG_SHADOW_FRUSTA |
        LLPipeline::RENDER_DEBUG_TEXEL_DENSITY))
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("render debug bridges");

        for (LLViewerRegion* region : LLWorld::getInstance()->getRegionList())
        {
            for (U32 i = 0; i < LLViewerRegion::NUM_PARTITIONS; i++)
            {
                LLSpatialPartition* part = region->getSpatialPartition(i);
                if (part)
                {
                    if ((hud_only && (part->mDrawableType == RENDER_TYPE_HUD || part->mDrawableType == RENDER_TYPE_HUD_PARTICLES)) ||
                        (!hud_only && hasRenderType(part->mDrawableType)))
                    {
                        part->renderDebug();
                    }
                }
            }
        }

        for (LLCullResult::bridge_iterator i = sCull->beginVisibleBridge(); i != sCull->endVisibleBridge(); ++i)
        {
            LLSpatialBridge* bridge = *i;
            if (!bridge->isDead() && hasRenderType(bridge->mDrawableType))
            {
                gGL.pushMatrix();
                gGL.multMatrix((F32*)bridge->mDrawable->getRenderMatrix().mMatrix);
                bridge->renderDebug();
                gGL.popMatrix();
            }
        }
    }

    if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_OCCLUSION))
    { //render visible selected group occlusion geometry
        gDebugProgram.bind();
        LLGLDepthTest depth(GL_TRUE, GL_FALSE);
        gGL.diffuseColor3f(1,0,1);
        for (std::set<LLSpatialGroup*>::iterator iter = visible_selected_groups.begin(); iter != visible_selected_groups.end(); ++iter)
        {
            LLSpatialGroup* group = *iter;

            LLVector4a fudge;
            fudge.splat(0.25f); //SG_OCCLUSION_FUDGE

            LLVector4a size;
            const LLVector4a* bounds = group->getBounds();
            size.setAdd(fudge, bounds[1]);

            drawBox(bounds[0], size);
        }
    }

    visible_selected_groups.clear();

    //draw reflection probes and links between them
    if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_REFLECTION_PROBES) && !hud_only)
    {
        mReflectionMapManager.renderDebug();
    }

    static LLCachedControl<bool> render_ref_probe_volumes(gSavedSettings, "RenderReflectionProbeVolumes");
    if (render_ref_probe_volumes && !hud_only)
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE("probe debug display");

        bindDeferredShader(gReflectionProbeDisplayProgram, NULL);
        mScreenTriangleVB->setBuffer();

        LLGLEnable blend(GL_BLEND);
        LLGLDepthTest depth(GL_FALSE);

        mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

        unbindDeferredShader(gReflectionProbeDisplayProgram);
    }

    gUIProgram.bind();

    if (hasRenderDebugMask(LLPipeline::RENDER_DEBUG_RAYCAST) && !hud_only)
    { //draw crosshairs on particle intersection
        if (gDebugRaycastParticle)
        {
            gDebugProgram.bind();

            gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

            LLVector3 center(gDebugRaycastParticleIntersection.getF32ptr());
            LLVector3 size(0.1f, 0.1f, 0.1f);

            LLVector3 p[6];

            p[0] = center + size.scaledVec(LLVector3(1,0,0));
            p[1] = center + size.scaledVec(LLVector3(-1,0,0));
            p[2] = center + size.scaledVec(LLVector3(0,1,0));
            p[3] = center + size.scaledVec(LLVector3(0,-1,0));
            p[4] = center + size.scaledVec(LLVector3(0,0,1));
            p[5] = center + size.scaledVec(LLVector3(0,0,-1));

            gGL.begin(LLRender::LINES);
            gGL.diffuseColor3f(1.f, 1.f, 0.f);
            for (U32 i = 0; i < 6; i++)
            {
                gGL.vertex3fv(p[i].mV);
            }
            gGL.end();
            gGL.flush();

            gDebugProgram.unbind();
        }
    }

    if (hasRenderDebugMask(LLPipeline::RENDER_DEBUG_SHADOW_FRUSTA) && !hud_only)
    {
        LLVertexBuffer::unbind();

        LLGLEnable blend(GL_BLEND);
        LLGLDepthTest depth(true, false);
        LLGLDisable cull(GL_CULL_FACE);

        gGL.color4f(1,1,1,1);
        gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

        F32 a = 0.1f;

        F32 col[] =
        {
            1,0,0,a,
            0,1,0,a,
            0,0,1,a,
            1,0,1,a,

            1,1,0,a,
            0,1,1,a,
            1,1,1,a,
            1,0,1,a,
        };

        for (U32 i = 0; i < 8; i++)
        {
            LLVector3* frust = mShadowCamera[i].mAgentFrustum;

            if (i > 3)
            { //render shadow frusta as volumes
                if (mShadowFrustPoints[i-4].empty())
                {
                    continue;
                }

                gGL.color4fv(col+(i-4)*4);

                gGL.begin(LLRender::TRIANGLE_STRIP);
                gGL.vertex3fv(frust[0].mV); gGL.vertex3fv(frust[4].mV);
                gGL.vertex3fv(frust[1].mV); gGL.vertex3fv(frust[5].mV);
                gGL.vertex3fv(frust[2].mV); gGL.vertex3fv(frust[6].mV);
                gGL.vertex3fv(frust[3].mV); gGL.vertex3fv(frust[7].mV);
                gGL.vertex3fv(frust[0].mV); gGL.vertex3fv(frust[4].mV);
                gGL.end();


                gGL.begin(LLRender::TRIANGLE_STRIP);
                gGL.vertex3fv(frust[0].mV);
                gGL.vertex3fv(frust[1].mV);
                gGL.vertex3fv(frust[3].mV);
                gGL.vertex3fv(frust[2].mV);
                gGL.end();

                gGL.begin(LLRender::TRIANGLE_STRIP);
                gGL.vertex3fv(frust[4].mV);
                gGL.vertex3fv(frust[5].mV);
                gGL.vertex3fv(frust[7].mV);
                gGL.vertex3fv(frust[6].mV);
                gGL.end();
            }


            if (i < 4)
            {

                //if (i == 0 || !mShadowFrustPoints[i].empty())
                {
                    //render visible point cloud
                    gGL.flush();
                    glPointSize(8.f);
                    gGL.begin(LLRender::POINTS);

                    F32* c = col+i*4;
                    gGL.color3fv(c);

                    for (U32 j = 0; j < mShadowFrustPoints[i].size(); ++j)
                        {
                            gGL.vertex3fv(mShadowFrustPoints[i][j].mV);

                        }
                    gGL.end();

                    gGL.flush();
                    glPointSize(1.f);

                    LLVector3* ext = mShadowExtents[i];
                    LLVector3 pos = (ext[0]+ext[1])*0.5f;
                    LLVector3 size = (ext[1]-ext[0])*0.5f;
                    drawBoxOutline(pos, size);

                    //render camera frustum splits as outlines
                    gGL.begin(LLRender::LINES);
                    gGL.vertex3fv(frust[0].mV); gGL.vertex3fv(frust[1].mV);
                    gGL.vertex3fv(frust[1].mV); gGL.vertex3fv(frust[2].mV);
                    gGL.vertex3fv(frust[2].mV); gGL.vertex3fv(frust[3].mV);
                    gGL.vertex3fv(frust[3].mV); gGL.vertex3fv(frust[0].mV);
                    gGL.vertex3fv(frust[4].mV); gGL.vertex3fv(frust[5].mV);
                    gGL.vertex3fv(frust[5].mV); gGL.vertex3fv(frust[6].mV);
                    gGL.vertex3fv(frust[6].mV); gGL.vertex3fv(frust[7].mV);
                    gGL.vertex3fv(frust[7].mV); gGL.vertex3fv(frust[4].mV);
                    gGL.vertex3fv(frust[0].mV); gGL.vertex3fv(frust[4].mV);
                    gGL.vertex3fv(frust[1].mV); gGL.vertex3fv(frust[5].mV);
                    gGL.vertex3fv(frust[2].mV); gGL.vertex3fv(frust[6].mV);
                    gGL.vertex3fv(frust[3].mV); gGL.vertex3fv(frust[7].mV);
                    gGL.end();
                }
            }

            /*gGL.flush();
             gGL.setLineWidth(16-i*2);
            for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin();
                    iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
            {
                LLViewerRegion* region = *iter;
                for (U32 j = 0; j < LLViewerRegion::NUM_PARTITIONS; j++)
                {
                    LLSpatialPartition* part = region->getSpatialPartition(j);
                    if (part)
                    {
                        if (hasRenderType(part->mDrawableType))
                        {
                            part->renderIntersectingBBoxes(&mShadowCamera[i]);
                        }
                    }
                }
            }
            gGL.flush();
             gGL.setLineWidth(1.f);*/
        }
    }

    if (mRenderDebugMask & RENDER_DEBUG_WIND_VECTORS)
    {
        gAgent.getRegion()->mWind.renderVectors();
    }

    if (mRenderDebugMask & RENDER_DEBUG_COMPOSITION)
    {
        // Debug composition layers
        F32 x, y;

        gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

        if (gAgent.getRegion())
        {
            gGL.begin(LLRender::POINTS);
            // Draw the composition layer for the region that I'm in.
            for (x = 0; x <= 260; x++)
            {
                for (y = 0; y <= 260; y++)
                {
                    if ((x > 255) || (y > 255))
                    {
                        gGL.color4f(1.f, 0.f, 0.f, 1.f);
                    }
                    else
                    {
                        gGL.color4f(0.f, 0.f, 1.f, 1.f);
                    }
                    F32 z = gAgent.getRegion()->getCompositionXY((S32)x, (S32)y);
                    z *= 5.f;
                    z += 50.f;
                    gGL.vertex3f(x, y, z);
                }
            }
            gGL.end();
        }
    }

    gGL.flush();
    gUIProgram.unbind();
}

void LLPipeline::rebuildPools()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;

    assertInitialized();

    auto max_count = mPools.size();
    pool_set_t::iterator iter1 = mPools.upper_bound(mLastRebuildPool);
    while(max_count > 0 && mPools.size() > 0) // && num_rebuilds < MAX_REBUILDS)
    {
        if (iter1 == mPools.end())
        {
            iter1 = mPools.begin();
        }
        LLDrawPool* poolp = *iter1;

        if (poolp->isDead())
        {
            mPools.erase(iter1++);
            removeFromQuickLookup( poolp );
            if (poolp == mLastRebuildPool)
            {
                mLastRebuildPool = NULL;
            }
            delete poolp;
        }
        else
        {
            mLastRebuildPool = poolp;
            iter1++;
        }
        max_count--;
    }
}

void LLPipeline::addToQuickLookup( LLDrawPool* new_poolp )
{
    assertInitialized();

    switch( new_poolp->getType() )
    {
    case LLDrawPool::POOL_SIMPLE:
        if (mSimplePool)
        {
            llassert(0);
            LL_WARNS() << "Ignoring duplicate simple pool." << LL_ENDL;
        }
        else
        {
            mSimplePool = (LLRenderPass*) new_poolp;
        }
        break;

    case LLDrawPool::POOL_ALPHA_MASK:
        if (mAlphaMaskPool)
        {
            llassert(0);
            LL_WARNS() << "Ignoring duplicate alpha mask pool." << LL_ENDL;
            break;
        }
        else
        {
            mAlphaMaskPool = (LLRenderPass*) new_poolp;
        }
        break;

    case LLDrawPool::POOL_FULLBRIGHT_ALPHA_MASK:
        if (mFullbrightAlphaMaskPool)
        {
            llassert(0);
            LL_WARNS() << "Ignoring duplicate alpha mask pool." << LL_ENDL;
            break;
        }
        else
        {
            mFullbrightAlphaMaskPool = (LLRenderPass*) new_poolp;
        }
        break;

    case LLDrawPool::POOL_GRASS:
        if (mGrassPool)
        {
            llassert(0);
            LL_WARNS() << "Ignoring duplicate grass pool." << LL_ENDL;
        }
        else
        {
            mGrassPool = (LLRenderPass*) new_poolp;
        }
        break;

    case LLDrawPool::POOL_FULLBRIGHT:
        if (mFullbrightPool)
        {
            llassert(0);
            LL_WARNS() << "Ignoring duplicate simple pool." << LL_ENDL;
        }
        else
        {
            mFullbrightPool = (LLRenderPass*) new_poolp;
        }
        break;

    case LLDrawPool::POOL_GLOW:
        if (mGlowPool)
        {
            llassert(0);
            LL_WARNS() << "Ignoring duplicate glow pool." << LL_ENDL;
        }
        else
        {
            mGlowPool = (LLRenderPass*) new_poolp;
        }
        break;

    case LLDrawPool::POOL_TREE:
        mTreePools[ uintptr_t(new_poolp->getTexture()) ] = new_poolp ;
        break;

    case LLDrawPool::POOL_TERRAIN:
        mTerrainPools[ uintptr_t(new_poolp->getTexture()) ] = new_poolp ;
        break;

    case LLDrawPool::POOL_BUMP:
        if (mBumpPool)
        {
            llassert(0);
            LL_WARNS() << "Ignoring duplicate bump pool." << LL_ENDL;
        }
        else
        {
            mBumpPool = new_poolp;
        }
        break;
    case LLDrawPool::POOL_MATERIALS:
        if (mMaterialsPool)
        {
            llassert(0);
            LL_WARNS() << "Ignorning duplicate materials pool." << LL_ENDL;
        }
        else
        {
            mMaterialsPool = new_poolp;
        }
        break;
    case LLDrawPool::POOL_ALPHA_PRE_WATER:
        if( mAlphaPoolPreWater )
        {
            llassert(0);
            LL_WARNS() << "LLPipeline::addPool(): Ignoring duplicate Alpha pre-water pool" << LL_ENDL;
        }
        else
        {
            mAlphaPoolPreWater = (LLDrawPoolAlpha*) new_poolp;
        }
        break;
    case LLDrawPool::POOL_ALPHA_POST_WATER:
        if (mAlphaPoolPostWater)
        {
            llassert(0);
            LL_WARNS() << "LLPipeline::addPool(): Ignoring duplicate Alpha post-water pool" << LL_ENDL;
        }
        else
        {
            mAlphaPoolPostWater = (LLDrawPoolAlpha*)new_poolp;
        }
        break;

    case LLDrawPool::POOL_AVATAR:
    case LLDrawPool::POOL_CONTROL_AV:
        break; // Do nothing

    case LLDrawPool::POOL_SKY:
        if( mSkyPool )
        {
            llassert(0);
            LL_WARNS() << "LLPipeline::addPool(): Ignoring duplicate Sky pool" << LL_ENDL;
        }
        else
        {
            mSkyPool = new_poolp;
        }
        break;

    case LLDrawPool::POOL_WATER:
        if( mWaterPool )
        {
            llassert(0);
            LL_WARNS() << "LLPipeline::addPool(): Ignoring duplicate Water pool" << LL_ENDL;
        }
        else
        {
            mWaterPool = new_poolp;
        }
        break;

    case LLDrawPool::POOL_WL_SKY:
        if( mWLSkyPool )
        {
            llassert(0);
            LL_WARNS() << "LLPipeline::addPool(): Ignoring duplicate WLSky Pool" << LL_ENDL;
        }
        else
        {
            mWLSkyPool = new_poolp;
        }
        break;

    case LLDrawPool::POOL_GLTF_PBR:
        if( mPBROpaquePool )
        {
            llassert(0);
            LL_WARNS() << "LLPipeline::addPool(): Ignoring duplicate PBR Opaque Pool" << LL_ENDL;
        }
        else
        {
            mPBROpaquePool = new_poolp;
        }
        break;

    case LLDrawPool::POOL_GLTF_PBR_ALPHA_MASK:
        if (mPBRAlphaMaskPool)
        {
            llassert(0);
            LL_WARNS() << "LLPipeline::addPool(): Ignoring duplicate PBR Alpha Mask Pool" << LL_ENDL;
        }
        else
        {
            mPBRAlphaMaskPool = new_poolp;
        }
        break;

    case LLDrawPool::POOL_WATEREXCLUSION:
        if (mWaterExclusionPool)
        {
            llassert(0);
            LL_WARNS() << "LLPipeline::addPool(): Ignoring duplicate Water Exclusion Pool" << LL_ENDL;
        }
        else
        {
            mWaterExclusionPool = new_poolp;
        }
        break;

    default:
        llassert(0);
        LL_WARNS() << "Invalid Pool Type in  LLPipeline::addPool()" << LL_ENDL;
        break;
    }
}

void LLPipeline::removePool( LLDrawPool* poolp )
{
    assertInitialized();
    removeFromQuickLookup(poolp);
    mPools.erase(poolp);
    delete poolp;
}

void LLPipeline::removeFromQuickLookup( LLDrawPool* poolp )
{
    assertInitialized();
    switch( poolp->getType() )
    {
    case LLDrawPool::POOL_SIMPLE:
        llassert(mSimplePool == poolp);
        mSimplePool = NULL;
        break;

    case LLDrawPool::POOL_ALPHA_MASK:
        llassert(mAlphaMaskPool == poolp);
        mAlphaMaskPool = NULL;
        break;

    case LLDrawPool::POOL_FULLBRIGHT_ALPHA_MASK:
        llassert(mFullbrightAlphaMaskPool == poolp);
        mFullbrightAlphaMaskPool = NULL;
        break;

    case LLDrawPool::POOL_GRASS:
        llassert(mGrassPool == poolp);
        mGrassPool = NULL;
        break;

    case LLDrawPool::POOL_FULLBRIGHT:
        llassert(mFullbrightPool == poolp);
        mFullbrightPool = NULL;
        break;

    case LLDrawPool::POOL_WL_SKY:
        llassert(mWLSkyPool == poolp);
        mWLSkyPool = NULL;
        break;

    case LLDrawPool::POOL_GLOW:
        llassert(mGlowPool == poolp);
        mGlowPool = NULL;
        break;

    case LLDrawPool::POOL_TREE:
        mTreePools.erase( (uintptr_t)poolp->getTexture() );
        break;

    case LLDrawPool::POOL_TERRAIN:
        mTerrainPools.erase( (uintptr_t)poolp->getTexture() );
        break;

    case LLDrawPool::POOL_BUMP:
        llassert( poolp == mBumpPool );
        mBumpPool = NULL;
        break;

    case LLDrawPool::POOL_MATERIALS:
        llassert(poolp == mMaterialsPool);
        mMaterialsPool = NULL;
        break;

    case LLDrawPool::POOL_ALPHA_PRE_WATER:
        llassert( poolp == mAlphaPoolPreWater );
        mAlphaPoolPreWater = nullptr;
        break;

    case LLDrawPool::POOL_ALPHA_POST_WATER:
        llassert(poolp == mAlphaPoolPostWater);
        mAlphaPoolPostWater = nullptr;
        break;

    case LLDrawPool::POOL_AVATAR:
    case LLDrawPool::POOL_CONTROL_AV:
        break; // Do nothing

    case LLDrawPool::POOL_SKY:
        llassert( poolp == mSkyPool );
        mSkyPool = NULL;
        break;

    case LLDrawPool::POOL_WATER:
        llassert( poolp == mWaterPool );
        mWaterPool = NULL;
        break;

    case LLDrawPool::POOL_GLTF_PBR:
        llassert( poolp == mPBROpaquePool );
        mPBROpaquePool = NULL;
        break;

    case LLDrawPool::POOL_GLTF_PBR_ALPHA_MASK:
        llassert(poolp == mPBRAlphaMaskPool);
        mPBRAlphaMaskPool = NULL;
        break;

    case LLDrawPool::POOL_WATEREXCLUSION:
        llassert(poolp == mWaterExclusionPool);
        mWaterExclusionPool = nullptr;
        break;

    default:
        llassert(0);
        LL_WARNS() << "Invalid Pool Type in  LLPipeline::removeFromQuickLookup() type=" << poolp->getType() << LL_ENDL;
        break;
    }
}

void LLPipeline::resetDrawOrders()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    assertInitialized();
    // Iterate through all of the draw pools and rebuild them.
    for (pool_set_t::iterator iter = mPools.begin(); iter != mPools.end(); ++iter)
    {
        LLDrawPool *poolp = *iter;
        poolp->resetDrawOrders();
    }
}

//============================================================================
// Once-per-frame setup of hardware lights,
// including sun/moon, avatar backlight, and up to 6 local lights

void LLPipeline::setupAvatarLights(bool for_edit)
{
    assertInitialized();

    LLEnvironment& environment = LLEnvironment::instance();
    LLSettingsSky::ptr_t psky = environment.getCurrentSky();

    bool sun_up = environment.getIsSunUp();


    if (for_edit)
    {
        LLColor4 diffuse(1.f, 1.f, 1.f, 0.f);
        LLVector4 light_pos_cam(-8.f, 0.25f, 10.f, 0.f);  // w==0 => directional light
        LLMatrix4 camera_mat = LLViewerCamera::getInstance()->getModelview();
        LLMatrix4 camera_rot(camera_mat.getMat3());
        camera_rot.invert();
        LLVector4 light_pos = light_pos_cam * camera_rot;

        light_pos.normalize();

        LLLightState* light = gGL.getLight(1);

        mHWLightColors[1] = diffuse;

        light->setDiffuse(diffuse);
        light->setAmbient(LLColor4::black);
        light->setSpecular(LLColor4::black);
        light->setPosition(light_pos);
        light->setConstantAttenuation(1.f);
        light->setLinearAttenuation(0.f);
        light->setQuadraticAttenuation(0.f);
        light->setSpotExponent(0.f);
        light->setSpotCutoff(180.f);
    }
    else if (gAvatarBacklight)
    {
        LLVector3 light_dir = sun_up ? LLVector3(mSunDir) : LLVector3(mMoonDir);
        LLVector3 opposite_pos = -light_dir;
        LLVector3 orthog_light_pos = light_dir % LLVector3::z_axis;
        LLVector4 backlight_pos = LLVector4(lerp(opposite_pos, orthog_light_pos, 0.3f), 0.0f);
        backlight_pos.normalize();

        LLColor4 light_diffuse = sun_up ? mSunDiffuse : mMoonDiffuse;

        LLColor4 backlight_diffuse(1.f - light_diffuse.mV[VRED], 1.f - light_diffuse.mV[VGREEN], 1.f - light_diffuse.mV[VBLUE], 1.f);
        F32 max_component = 0.001f;
        for (S32 i = 0; i < 3; i++)
        {
            if (backlight_diffuse.mV[i] > max_component)
            {
                max_component = backlight_diffuse.mV[i];
            }
        }
        F32 backlight_mag;
        if (LLEnvironment::instance().getIsSunUp())
        {
            backlight_mag = BACKLIGHT_DAY_MAGNITUDE_OBJECT;
        }
        else
        {
            backlight_mag = BACKLIGHT_NIGHT_MAGNITUDE_OBJECT;
        }
        backlight_diffuse *= backlight_mag / max_component;

        mHWLightColors[1] = backlight_diffuse;

        LLLightState* light = gGL.getLight(1);

        light->setPosition(backlight_pos);
        light->setDiffuse(backlight_diffuse);
        light->setAmbient(LLColor4::black);
        light->setSpecular(LLColor4::black);
        light->setConstantAttenuation(1.f);
        light->setLinearAttenuation(0.f);
        light->setQuadraticAttenuation(0.f);
        light->setSpotExponent(0.f);
        light->setSpotCutoff(180.f);
    }
    else
    {
        LLLightState* light = gGL.getLight(1);

        mHWLightColors[1] = LLColor4::black;

        light->setDiffuse(LLColor4::black);
        light->setAmbient(LLColor4::black);
        light->setSpecular(LLColor4::black);
    }
}

static F32 calc_light_dist(LLVOVolume* light, const LLVector3& cam_pos, F32 max_dist)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    F32 inten = light->getLightIntensity();
    if (inten < .001f)
    {
        return max_dist;
    }
    bool selected = light->isSelected();
    if (selected)
    {
        return 0.f; // selected lights get highest priority
    }
    F32 radius = light->getLightRadius();
    F32 dist = dist_vec(light->getRenderPosition(), cam_pos);
    dist = llmax(dist - radius, 0.f);
    if (light->mDrawable.notNull() && light->mDrawable->isState(LLDrawable::ACTIVE))
    {
        // moving lights get a little higher priority (too much causes artifacts)
        dist = llmax(dist - radius * 0.25f, 0.f);
    }
    return dist;
}

// [BDMerge A5.6] Independent light-source class toggles: world lights / own attached
// lights / others' attached lights / projector (spotlight projection) rendering, each
// toggled without affecting the others. Donor: I:\black-dragon indra/newview/pipeline.cpp
// sRenderOtherAttachedLights / sRenderOwnAttachedLights / sRenderDeferredLights (own/other
// split + a third static that -- despite the name -- gates *non-attachment* "world" lights;
// see settings_blackdragon.xml key RenderDeferredLights, label "Render World Lights"),
// handlers handleRenderOtherAttachedLightsChanged/handleRenderOwnAttachedLightsChanged/
// handleRenderDeferredLightsChanged in llviewercontrol.cpp:738-753, wired at :1207-1209.
// Stock Alchemy only has the blanket LLPipeline::sRenderAttachedLights (all attachment
// lights on/off) applied redundantly in calcNearbyLights/setupHWLights/renderDeferredLighting
// -- that gate is left untouched and always applies first. BD has no dedicated projector
// toggle; that fourth class is a novel addition here, reusing the existing
// LLVOVolume::isLightSpotlight() gate that already distinguishes spot-vs-omni light state
// (setupHWLights) and spot/fullscreen_spot light routing (renderDeferredLighting). When off,
// a projector prim keeps illuminating as a plain omni light (falls back) instead of
// disappearing -- it only loses the directional cone / projected image.
// Gated by master BDMergeLightToggles (default OFF); while off, or while master is on but a
// given per-class Boolean is left at its default (ON), this is a no-op and rendering is
// bit-identical to stock. LLCachedControl is used throughout since these run in the hot
// per-frame light-gathering/render paths.
static bool bdmerge_should_render_light(bool is_attachment, bool is_own_avatar)
{
    static LLCachedControl<bool> bdmerge_light_toggles(gSavedSettings, "BDMergeLightToggles", false);
    if (!bdmerge_light_toggles)
    {
        return true;
    }

    if (is_attachment)
    {
        if (is_own_avatar)
        {
            static LLCachedControl<bool> bdmerge_render_own(gSavedSettings, "BDMergeRenderOwnAttachedLights", true);
            return bdmerge_render_own;
        }
        static LLCachedControl<bool> bdmerge_render_others(gSavedSettings, "BDMergeRenderOthersAttachedLights", true);
        return bdmerge_render_others;
    }

    static LLCachedControl<bool> bdmerge_render_world(gSavedSettings, "BDMergeRenderWorldLights", true);
    return bdmerge_render_world;
}

static bool bdmerge_should_render_projector()
{
    static LLCachedControl<bool> bdmerge_light_toggles(gSavedSettings, "BDMergeLightToggles", false);
    if (!bdmerge_light_toggles)
    {
        return true;
    }

    static LLCachedControl<bool> bdmerge_render_projectors(gSavedSettings, "BDMergeRenderProjectors", true);
    return bdmerge_render_projectors;
}
// [/BDMerge A5.6]

// [BDMerge A1.2] Resolution-aware autoscale (SSAO / shadow blur / DoF).
// llfloatersnapshot.cpp:Impl::updateResolution derives BDMergeSnapshotAutoscaleMultiplier
// (output snapshot height / current window height) whenever the requested snapshot
// resolution changes, but only while BDMergeSnapshotAutoscale is enabled. This helper is
// the single read-back point: gate off (default) always returns 1.0 regardless of what is
// stored, so rendering is bit-identical to stock. Consumed at the three sites BD scaled:
// SSAO radius/max-radius (bindDeferredShader), the shared shadow+SSAO soften blur size
// (renderDeferredLighting), and the DoF field-of-view input (renderFinalize). NOT covered
// (per merge doc gotcha): screen-space reflections (RenderScreenSpaceReflection*) and
// volumetric lighting/godrays (RenderGodrays*) still need manual reconfiguration at other
// resolutions -- BD's donor commit never touched either.
// donor: Black Dragon (NiranV Dean) 8eed1a6768 ("Added: Simple automatic SSAO/Shadow/DoF
// scaling option..."), non-persist multiplier per fa96df28ee, gPipeline-gated read per
// 33556578a1.
static F32 bdmerge_snapshot_autoscale_multiplier()
{
    static LLCachedControl<bool> bdmerge_snapshot_autoscale(gSavedSettings, "BDMergeSnapshotAutoscale", false);
    if (!bdmerge_snapshot_autoscale)
    {
        return 1.0f;
    }

    static LLCachedControl<F32> bdmerge_multiplier(gSavedSettings, "BDMergeSnapshotAutoscaleMultiplier", 1.0f);
    // guard against a degenerate stored value (e.g. window minimized to zero height)
    return llclamp((F32)bdmerge_multiplier, 0.01f, 100.f);
}

void LLPipeline::calcNearbyLights(LLCamera& camera)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    assertInitialized();

    if (LLPipeline::sReflectionRender || gCubeSnapshot || LLPipeline::sRenderingHUDs || LLApp::isExiting())
    {
        return;
    }

    if (sPrismLensRender)
    {
        // Build an independent source-eye list. Main-view entries were moved
        // into mPrismSavedNearbyLights at scope entry, and their drawable
        // NEARBY_LIGHT bits deliberately remain untouched. In particular, do
        // not call setVisible() or advance fades: both are persistent main-view
        // state, and a dense main list must not consume the source list's cap.
        mNearbyLights.clear();
        static LLCachedControl<S32> prism_local_light_count(
            gSavedSettings, "RenderLocalLightCount", 256);
        static LLCachedControl<F32> prism_light_scale(
            gSavedSettings, "AlchemyGlobalLightScale", 1.f);
        if (prism_local_light_count < 1)
        {
            return;
        }

        const LLVector3 cam_pos = camera.getOrigin();
        const F32 max_dist = sRenderDeferred
            ? llmin(RenderFarClip, camera.getFar())
            : llmin(llmin(RenderFarClip, camera.getFar()), LIGHT_MAX_RADIUS * 4.f);

        for (LLDrawable* drawable : mLights)
        {
            LLVOVolume* light = drawable ? drawable->getVOVolume() : nullptr;
            if (!light || !drawable->isState(LLDrawable::LIGHT) ||
                light->isHUDAttachment())
            {
                continue;
            }

            if (light->isAttachment())
            {
                if (!sRenderAttachedLights)
                {
                    continue;
                }
                LLVOAvatar* avatar = light->getAvatar();
                if (!bdmerge_should_render_light(true, avatar == gAgentAvatarp) ||
                    (avatar && (avatar->isTooComplex() || avatar->isInMuteList() ||
                                avatar->isTooSlow())))
                {
                    continue;
                }
            }
            else if (!bdmerge_should_render_light(false, false))
            {
                continue;
            }

            const F32 light_radius = light->getLightRadius() * 1.5f;
            const LLColor3 light_color =
                light->getLightLinearColor() * (F32)prism_light_scale;
            if (light_radius <= 0.001f ||
                light_color.magVecSquared() < 0.001f)
            {
                continue;
            }
            LLVector4a center;
            center.load3(drawable->getPositionAgent().mV);
            LLVector4a radius;
            radius.splat(light_radius);

            // Spotlights/projectors inside or near the auxiliary camera frustum must never be culled by eye-near clipping.
            const bool is_spot = light->isLightSpotlight();
            if (!is_spot && camera.AABBInFrustumNoFarClip(center, radius) == 0)
            {
                continue;
            }

            const F32 dist = calc_light_dist(light, cam_pos, max_dist);
            if (dist < max_dist || is_spot)
            {
                // Fully visible avoids setupHWLights' in-place fade-clock write.
                mNearbyLights.insert(Light(drawable, dist, LIGHT_FADE_TIME));
            }
        }
        return;
    }

    static LLCachedControl<S32> local_light_count(gSavedSettings, "RenderLocalLightCount", 256);

    if (local_light_count >= 1)
    {
        // mNearbyLight (and all light_set_t's) are sorted such that
        // begin() == the closest light and rbegin() == the farthest light
        const S32 MAX_LOCAL_LIGHTS = 6;
        LLVector3 cam_pos = camera.getOrigin();

        F32 max_dist;
        if (LLPipeline::sRenderDeferred)
        {
            max_dist = RenderFarClip;
        }
        else
        {
            max_dist = llmin(RenderFarClip, LIGHT_MAX_RADIUS * 4.f);
        }

        // UPDATE THE EXISTING NEARBY LIGHTS
        for (light_set_t::iterator iter = mNearbyLights.begin();
            iter != mNearbyLights.end();)
        {
            const Light* light = &(*iter);
            LLDrawable* drawable = light->drawable;
            const LLViewerObject *vobj = light->drawable->getVObj();
            if(vobj && vobj->isAttachment())
            {
                if (!sRenderAttachedLights)
                {
                    drawable->clearState(LLDrawable::NEARBY_LIGHT);
                    iter = mNearbyLights.erase(iter);
                    continue;
                }

                LLVOAvatar *avatar = vobj->getAvatar();

                // [BDMerge A5.6] independent own/other attached-light toggles
                if (!bdmerge_should_render_light(true, avatar == gAgentAvatarp))
                {
                    drawable->clearState(LLDrawable::NEARBY_LIGHT);
                    iter = mNearbyLights.erase(iter);
                    continue;
                }

                if (avatar && (avatar->isTooComplex() || avatar->isInMuteList() || avatar->isTooSlow()))
                {
                    drawable->clearState(LLDrawable::NEARBY_LIGHT);
                    iter = mNearbyLights.erase(iter);
                    continue;
                }
            }
            // [BDMerge A5.6] independent world-light toggle (non-attachment lights had no toggle at all in stock)
            else if (vobj && !bdmerge_should_render_light(false, false))
            {
                drawable->clearState(LLDrawable::NEARBY_LIGHT);
                iter = mNearbyLights.erase(iter);
                continue;
            }

            LLVOVolume* volight = drawable->getVOVolume();
            if (!volight || !drawable->isState(LLDrawable::LIGHT))
            {
                drawable->clearState(LLDrawable::NEARBY_LIGHT);
                iter = mNearbyLights.erase(iter);
                continue;
            }
            if (light->fade <= -LIGHT_FADE_TIME)
            {
                drawable->clearState(LLDrawable::NEARBY_LIGHT);
                iter = mNearbyLights.erase(iter);
                continue;
            }

            F32 dist = calc_light_dist(volight, cam_pos, max_dist);
            F32 fade = light->fade;
            // actual fade gets decreased/increased by setupHWLights
            // light->fade value is 'time'.
            // >=0 and light will become visible as value increases
            // <0 and light will fade out
            if (dist < max_dist)
            {
                if (fade < 0)
                {
                    // mark light to fade in
                    // if fade was -LIGHT_FADE_TIME - it was fully invisible
                    // if fade -0 - it was fully visible
                    // visibility goes up from 0 to LIGHT_FADE_TIME.
                    fade += LIGHT_FADE_TIME;
                }
            }
            else
            {
                // mark light to fade out
                // visibility goes down from -0 to -LIGHT_FADE_TIME.
                if (fade >= LIGHT_FADE_TIME)
                {
                    fade = -0.0001f; // was fully visible
                }
                else if (fade >= 0)
                {
                    // 0.75 visible light should stay 0.75 visible, but should reverse direction
                    fade -= LIGHT_FADE_TIME;
                }
            }

            ++iter; // Advance to next light
        }

        // FIND NEW LIGHTS THAT ARE IN RANGE
        for (LLDrawable::ordered_drawable_set_t::iterator iter = mLights.begin();
             iter != mLights.end(); ++iter)
        {
            LLDrawable* drawable = *iter;
            LLVOVolume* light = drawable->getVOVolume();
            if (!light || drawable->isState(LLDrawable::NEARBY_LIGHT))
            {
                continue;
            }
            if (light->isHUDAttachment())
            {
                continue; // no lighting from HUD objects
            }
            if (light->isAttachment())
            {
                if (!sRenderAttachedLights)
                {
                    continue;
                }

                LLVOAvatar* av = light->getAvatar();

                // [BDMerge A5.6] independent own/other attached-light toggles
                if (!bdmerge_should_render_light(true, av == gAgentAvatarp))
                {
                    continue;
                }

                if (av && (av->isTooComplex() || av->isInMuteList() || av->isTooSlow()))
                {
                    // avatars that are already in the list will be removed by removeMutedAVsLights
                    continue;
                }
            }
            // [BDMerge A5.6] independent world-light toggle
            else if (!bdmerge_should_render_light(false, false))
            {
                continue;
            }
            F32 dist = calc_light_dist(light, cam_pos, max_dist);
            if (dist >= max_dist)
            {
                continue;
            }

            mNearbyLights.insert(Light(drawable, dist, 0.f));
            drawable->setState(LLDrawable::NEARBY_LIGHT);
        }

        //mark nearby lights not-removable.
        for (light_set_t::iterator iter = mNearbyLights.begin();
             iter != mNearbyLights.end(); iter++)
        {
            const Light* light = &(*iter);
            ((LLViewerOctreeEntryData*) light->drawable)->setVisible();
        }
    }
}

bool LLPipeline::beginPrismAuxiliaryState()
{
    if (mPrismAuxiliaryStateActive)
    {
        LL_WARNS("PrismLens")
            << "Rejected nested Prism auxiliary renderer state scope" << LL_ENDL;
        return false;
    }

    mPrismAuxiliaryStateActive = true;
    mPrismSavedNearbyLights = mNearbyLights;
    // No inherited main-eye entries in the auxiliary list. Main drawable bits
    // remain as-is and are ignored by calcNearbyLights' Prism-only builder.
    mNearbyLights.clear();
    mPrismSavedLightMask = mLightMask;
    mPrismSavedLightMovingMask = mLightMovingMask;
    gGL.getLightStateSnapshot(mPrismSavedGLLights, mPrismSavedGLAmbient);
    for (U32 i = 0; i < 8; ++i)
    {
        mPrismSavedHWLightColors[i] = mHWLightColors[i];
    }
    mPrismSavedSunDiffuse = mSunDiffuse;
    mPrismSavedMoonDiffuse = mMoonDiffuse;
    mPrismSavedSunDir = mSunDir;
    mPrismSavedMoonDir = mMoonDir;
    mPrismSavedPoissonOffset = mPoissonOffset;
    for (U32 i = 0; i < MAX_SPOT_SHADOWS; ++i)
    {
        mPrismSavedShadowSpotLight[i] = mShadowSpotLight[i];
        mPrismSavedTargetShadowSpotLight[i] = mTargetShadowSpotLight[i];
        mPrismSavedSpotLightFade[i] = mSpotLightFade[i];
        // [Prism spot shadows Stage 1] the aux pass re-expresses the projector
        // sampling matrices with the aux camera's inverse view
        mPrismSavedSunShadowMatrix[i] = mSunShadowMatrix[i + 4];
        // [Prism spot shadows Stage 2] the aux generation pass rewrites the
        // per-slot projector view/projection for freshly selected projectors
        mPrismSavedShadowModelview[i] = mShadowModelview[i + 4];
        mPrismSavedShadowProjection[i] = mShadowProjection[i + 4];
    }

    // Probe selection is camera-space state. Build the UBO once from the main
    // eye if it has not been initialized yet, then stage an auxiliary view that
    // contains only the global/default probe. This keeps remote geometry lit
    // without allowing a source-eye probe/hero selection to replace the main
    // view's UBO. Dynamic/local probes and hero mirrors resume after the scope.
    mPrismSavedProbeDataValid = false;
    mPrismProbeUBOStaged = false;
    mPrismAuxiliaryWaterHeightValid = false;
    if (sReflectionProbesEnabled)
    {
        GLint saved_uniform_buffer = 0;
        glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &saved_uniform_buffer);
        if (mReflectionMapManager.mUBO == 0)
        {
            // beginPrismAuxiliaryState() is called before sPrismLensRender and
            // before the camera is replaced, so this is a main-eye initialization.
            mReflectionMapManager.updateUniforms();
        }

        if (mReflectionMapManager.mUBO != 0)
        {
            mPrismSavedProbeData = mReflectionMapManager.mProbeData;
            mPrismSavedProbeDataValid = true;
        }
        glBindBuffer(GL_UNIFORM_BUFFER, saved_uniform_buffer);
    }

    return true;
}

void LLPipeline::activatePrismAuxiliaryProbeState()
{
    if (!mPrismAuxiliaryStateActive || !mPrismSavedProbeDataValid ||
        mReflectionMapManager.mUBO == 0)
    {
        return;
    }

    LLReflectionMapManager::ReflectionProbeData prism_probe_data =
        mPrismSavedProbeData;
    prism_probe_data.refmapCount = llmin(prism_probe_data.refmapCount, 1);
    prism_probe_data.heroProbeCount = 0;

    // Re-express the global/default probe in the auxiliary camera space without
    // touching LLReflectionMap's main-eye depth/index/last-bind bookkeeping.
    // Local probes are intentionally omitted: selecting them safely would
    // mutate that shared bookkeeping and double the per-capture UBO work.
    if (LLReflectionMap* default_probe = mReflectionMapManager.mDefaultProbe)
    {
        LLMatrix4a modelview;
        LLVector4a camera_origin;
        modelview.loadu(gGLModelView);
        modelview.affineTransform(default_probe->mOrigin, camera_origin);
        prism_probe_data.refSphere[0].set(camera_origin.getF32ptr());
        prism_probe_data.refSphere[0].mV[3] = default_probe->mRadius;
        prism_probe_data.refParams[0].mV[3] =
            camera_origin.getF32ptr()[2] - default_probe->mRadius;

        // The reflection-probe manager renders probe cube faces AFTER display() with
        // gCubeSnapshot on; that render's updateUniforms leaves mProbeData's ambiance/
        // radiance scale in the cube-snapshot pass state (ambscale = 0 on an ambiance/
        // irradiance pass, radscale = 0.5). The aux runs BEFORE the main-view
        // updateUniforms that corrects it, snapshots that stale struct, and the
        // re-expression above overwrites only the probe's position -- so the feed's
        // single default probe inherits a diffuse-ambient scale that toggles 0<->full
        // across the probe refresh schedule, flickering PBR ambient (visible only when
        // the sky/probe ambient is bright: midday / high Reflection Probe Ambiance).
        // Force the two scale fields to the stable, fully-converged (non-ambiance-pass)
        // values so the feed is steady regardless of which probe pass the previous frame
        // ended on. Mirrors LLReflectionMapManager::updateUniforms with
        // ambscale = radscale = mResetFade. Feed-only: edits the aux's local
        // prism_probe_data copy, never the main-eye mProbeData, so the main view and
        // VCam-off stay byte-identical.
        static LLCachedControl<bool> should_auto_adjust(gSavedSettings, "RenderSkyAutoAdjustLegacy", false);
        const F32 minimum_ambiance =
            LLEnvironment::instance().getCurrentSky()->getReflectionProbeAmbiance(should_auto_adjust);
        const F32 stable_scale = llmax(0.f, mReflectionMapManager.mResetFade);
        prism_probe_data.refParams[0].mV[0] =
            llmax(minimum_ambiance, default_probe->getAmbiance()) * stable_scale;
        prism_probe_data.refParams[0].mV[1] = stable_scale;
    }

    GLint saved_uniform_buffer = 0;
    glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &saved_uniform_buffer);
    glBindBuffer(GL_UNIFORM_BUFFER, mReflectionMapManager.mUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0,
                    sizeof(LLReflectionMapManager::ReflectionProbeData),
                    &prism_probe_data);
    glBindBuffer(GL_UNIFORM_BUFFER, saved_uniform_buffer);
    mPrismProbeUBOStaged = true;
}

void LLPipeline::setPrismAuxiliaryWaterHeight(const LLVector3& eye)
{
    if (!mPrismAuxiliaryStateActive)
    {
        LL_WARNS("PrismLens")
            << "Ignored water-height override outside the auxiliary state scope"
            << LL_ENDL;
        return;
    }

    F32 water_height = LLEnvironment::instance().getWaterHeight();
    if (LLViewerRegion* camera_region =
            LLWorld::instance().getRegionFromPosAgent(eye))
    {
        water_height = camera_region->getWaterHeight();
    }

    mPrismAuxiliaryWaterHeight = water_height;
    mPrismAuxiliaryWaterHeightValid = true;
    sUnderWaterRender = eye.mV[VZ] < water_height;
}

F32 LLPipeline::getRenderWaterHeight() const
{
    if (sPrismLensRender && mPrismAuxiliaryWaterHeightValid)
    {
        return mPrismAuxiliaryWaterHeight;
    }
    return LLEnvironment::instance().getWaterHeight();
}

void LLPipeline::endPrismAuxiliaryState()
{
    if (!mPrismAuxiliaryStateActive)
    {
        return;
    }

    // Discard the temporary source-eye set and restore the main-eye membership.
    // The Prism-only builder never touches drawable NEARBY_LIGHT bits, so leave
    // those bits exactly as they were for the complete transaction.
    mNearbyLights = mPrismSavedNearbyLights;

    for (U32 i = 0; i < MAX_SPOT_SHADOWS; ++i)
    {
        mShadowSpotLight[i] = mPrismSavedShadowSpotLight[i];
        mTargetShadowSpotLight[i] = mPrismSavedTargetShadowSpotLight[i];
        mSpotLightFade[i] = mPrismSavedSpotLightFade[i];
        // [Prism spot shadows Stage 1] restore the main-view projector sampling
        // matrices before the main stateSort / deferred lighting consume them
        mSunShadowMatrix[i + 4] = mPrismSavedSunShadowMatrix[i];
        // [Prism spot shadows Stage 2] restore the main-view per-slot projector
        // view/projection matrices likewise
        mShadowModelview[i + 4] = mPrismSavedShadowModelview[i];
        mShadowProjection[i + 4] = mPrismSavedShadowProjection[i];
    }
    mPoissonOffset = mPrismSavedPoissonOffset;

    if (mPrismProbeUBOStaged && mPrismSavedProbeDataValid &&
        mReflectionMapManager.mUBO != 0)
    {
        GLint saved_uniform_buffer = 0;
        glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &saved_uniform_buffer);
        glBindBuffer(GL_UNIFORM_BUFFER, mReflectionMapManager.mUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0,
                        sizeof(LLReflectionMapManager::ReflectionProbeData),
                        &mPrismSavedProbeData);
        glBindBuffer(GL_UNIFORM_BUFFER, saved_uniform_buffer);
    }

    // setupHWLights() transformed positions into the auxiliary modelview and
    // advanced fade clocks. Restore both the pipeline cache and LLRender's
    // complete light records; the hash bump forces fresh uniforms on next bind.
    gGL.restoreLightStateSnapshot(mPrismSavedGLLights, mPrismSavedGLAmbient);
    for (U32 i = 0; i < 8; ++i)
    {
        mHWLightColors[i] = mPrismSavedHWLightColors[i];
    }
    mSunDiffuse = mPrismSavedSunDiffuse;
    mMoonDiffuse = mPrismSavedMoonDiffuse;
    mSunDir = mPrismSavedSunDir;
    mMoonDir = mPrismSavedMoonDir;
    mLightMask = mPrismSavedLightMask;
    mLightMovingMask = mPrismSavedLightMovingMask;

    mPrismSavedNearbyLights.clear();
    for (U32 i = 0; i < MAX_SPOT_SHADOWS; ++i)
    {
        mPrismSavedShadowSpotLight[i] = nullptr;
        mPrismSavedTargetShadowSpotLight[i] = nullptr;
    }
    mPrismSavedProbeDataValid = false;
    mPrismProbeUBOStaged = false;
    mPrismAuxiliaryWaterHeightValid = false;
    mPrismAuxiliaryStateActive = false;
}

void LLPipeline::setupHWLights()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_DRAWPOOL;
    assertInitialized();

    if (LLPipeline::sRenderingHUDs)
    {
        return;
    }

    F32 light_scale = 1.f;

    if (gCubeSnapshot)
    { //darken local lights when probe ambiance is above 1
        light_scale = mReflectionMapManager.mLightScale;
    }
    else
    {
        static LLCachedControl<F32> alchemy_light_scale(gSavedSettings, "AlchemyGlobalLightScale", 1.f);
        light_scale = alchemy_light_scale;
    }


    LLEnvironment& environment = LLEnvironment::instance();
    LLSettingsSky::ptr_t psky = environment.getCurrentSky();

    // Ambient
    LLColor4 ambient = psky->getTotalAmbient();

    gGL.setAmbientLightColor(ambient);

    bool sun_up  = environment.getIsSunUp();
    bool moon_up = environment.getIsMoonUp();

    // Light 0 = Sun or Moon (All objects)
    {
        LLVector4 sun_dir(environment.getSunDirection(), 0.0f);
        LLVector4 moon_dir(environment.getMoonDirection(), 0.0f);

        mSunDir.setVec(sun_dir);
        mMoonDir.setVec(moon_dir);

        mSunDiffuse.setVec(psky->getSunlightColor());
        mMoonDiffuse.setVec(psky->getMoonlightColor());

        F32 max_color = llmax(mSunDiffuse.mV[0], mSunDiffuse.mV[1], mSunDiffuse.mV[2]);
        if (max_color > 1.f)
        {
            mSunDiffuse *= 1.f/max_color;
        }
        mSunDiffuse.clamp();

        max_color = llmax(mMoonDiffuse.mV[0], mMoonDiffuse.mV[1], mMoonDiffuse.mV[2]);
        if (max_color > 1.f)
        {
            mMoonDiffuse *= 1.f/max_color;
        }
        mMoonDiffuse.clamp();

        // prevent underlighting from having neither lightsource facing us
        if (!sun_up && !moon_up)
        {
            mSunDiffuse.setVec(LLColor4(0.0, 0.0, 0.0, 1.0));
            mMoonDiffuse.setVec(LLColor4(0.0, 0.0, 0.0, 1.0));
            mSunDir.setVec(LLVector4(0.0, 1.0, 0.0, 0.0));
            mMoonDir.setVec(LLVector4(0.0, 1.0, 0.0, 0.0));
        }

        LLVector4 light_dir = sun_up ? mSunDir : mMoonDir;

        mHWLightColors[0] = sun_up ? mSunDiffuse : mMoonDiffuse;

        LLLightState* light = gGL.getLight(0);
        light->setPosition(light_dir);

        light->setSunPrimary(sun_up);
        light->setDiffuse(mHWLightColors[0]);
        light->setDiffuseB(mMoonDiffuse);
        light->setAmbient(psky->getTotalAmbient());
        light->setSpecular(LLColor4::black);
        light->setConstantAttenuation(1.f);
        light->setLinearAttenuation(0.f);
        light->setQuadraticAttenuation(0.f);
        light->setSpotExponent(0.f);
        light->setSpotCutoff(180.f);
    }

    // Light 1 = Backlight (for avatars)
    // (set by enableLightsAvatar)

    S32 cur_light = 2;

    // Nearby lights = LIGHT 2-7

    mLightMovingMask = 0;

    static LLCachedControl<S32> local_light_count(gSavedSettings, "RenderLocalLightCount", 256);

    if (local_light_count >= 1)
    {
        for (light_set_t::iterator iter = mNearbyLights.begin();
             iter != mNearbyLights.end(); ++iter)
        {
            LLDrawable* drawable = iter->drawable;
            LLVOVolume* light = drawable->getVOVolume();
            if (!light)
            {
                continue;
            }

            if (light->isAttachment())
            {
                if (!sRenderAttachedLights)
                {
                    continue;
                }

                // [BDMerge A5.6] independent own/other attached-light toggles
                if (!bdmerge_should_render_light(true, light->getAvatar() == gAgentAvatarp))
                {
                    continue;
                }
            }
            // [BDMerge A5.6] independent world-light toggle
            else if (!bdmerge_should_render_light(false, false))
            {
                continue;
            }

            if (drawable->isState(LLDrawable::ACTIVE))
            {
                mLightMovingMask |= (1<<cur_light);
            }

            //send linear light color to shader
            LLColor4  light_color = light->getLightLinearColor() * light_scale;
            light_color.mV[3] = 0.0f;

            F32 fade = iter->fade;
            if (fade < LIGHT_FADE_TIME)
            {
                // fade in/out light
                if (fade >= 0.f)
                {
                    fade = fade / LIGHT_FADE_TIME;
                    ((Light*) (&(*iter)))->fade += gFrameIntervalSeconds.value();
                }
                else
                {
                    fade = 1.f + fade / LIGHT_FADE_TIME;
                    ((Light*) (&(*iter)))->fade -= gFrameIntervalSeconds.value();
                }
                fade = llclamp(fade,0.f,1.f);
                light_color *= fade;
            }

            if (light_color.magVecSquared() < 0.001f)
            {
                continue;
            }

            LLVector3 light_pos(light->getRenderPosition());
            LLVector4 light_pos_gl(light_pos, 1.0f);

            F32 adjusted_radius = light->getLightRadius() * (sRenderDeferred ? 1.5f : 1.0f);
            if (adjusted_radius <= 0.001f)
            {
                continue;
            }

            F32 x = (3.f * (1.f + (light->getLightFalloff() * 2.0f)));  // why this magic?  probably trying to match a historic behavior.
            F32 linatten = x / adjusted_radius;                         // % of brightness at radius

            mHWLightColors[cur_light] = light_color;
            LLLightState* light_state = gGL.getLight(cur_light);

            light_state->setPosition(light_pos_gl);
            light_state->setDiffuse(light_color);
            light_state->setAmbient(LLColor4::black);
            light_state->setConstantAttenuation(0.f);
            light_state->setSize(light->getLightRadius() * 1.5f);
            light_state->setFalloff(light->getLightFalloff(DEFERRED_LIGHT_FALLOFF));

            if (sRenderDeferred)
            {
                light_state->setLinearAttenuation(linatten);
                light_state->setQuadraticAttenuation(light->getLightFalloff(DEFERRED_LIGHT_FALLOFF) + 1.f); // get falloff to match for forward deferred rendering lights
            }
            else
            {
                light_state->setLinearAttenuation(linatten);
                light_state->setQuadraticAttenuation(0.f);
            }


            if (light->isLightSpotlight() // directional (spot-)light
                && (LLPipeline::sRenderDeferred || RenderSpotLightsInNondeferred) // these are only rendered as GL spotlights if we're in deferred rendering mode *or* the setting forces them on
                && bdmerge_should_render_projector()) // [BDMerge A5.6] projector toggle: falls back to omni light below when off
            {
                LLQuaternion quat = light->getRenderRotation();
                LLVector3 at_axis(0,0,-1); // this matches deferred rendering's object light direction
                at_axis *= quat;

                light_state->setSpotDirection(at_axis);
                light_state->setSpotCutoff(90.f);
                light_state->setSpotExponent(2.f);

                LLVector3 spotParams = light->getSpotLightParams();

                const LLColor4 specular(0.f, 0.f, 0.f, spotParams[2]);
                light_state->setSpecular(specular);
            }
            else // omnidirectional (point) light
            {
                light_state->setSpotExponent(0.f);
                light_state->setSpotCutoff(180.f);

                // we use specular.z = 1.0 as a cheap hack for the shaders to know that this is omnidirectional rather than a spotlight
                const LLColor4 specular(0.f, 0.f, 1.f, 0.f);
                light_state->setSpecular(specular);
            }
            cur_light++;
            if (cur_light >= 8)
            {
                break; // safety
            }
        }
    }
    for ( ; cur_light < 8 ; cur_light++)
    {
        mHWLightColors[cur_light] = LLColor4::black;
        LLLightState* light = gGL.getLight(cur_light);
        light->setSunPrimary(true);
        light->setDiffuse(LLColor4::black);
        light->setAmbient(LLColor4::black);
        light->setSpecular(LLColor4::black);
    }

    // Bookmark comment to allow searching for mSpecialRenderMode == 3 (avatar edit mode),
    // prev site of forward (non-deferred) character light injection, removed by SL-13522 09/20

    // Init GL state
    for (S32 i = 0; i < 8; ++i)
    {
        gGL.getLight(i)->disable();
    }
    mLightMask = 0;
}

void LLPipeline::enableLights(U32 mask)
{
    assertInitialized();

    if (mLightMask != mask)
    {
        stop_glerror();
        if (mask)
        {
            stop_glerror();
            for (S32 i=0; i<8; i++)
            {
                LLLightState* light = gGL.getLight(i);
                if (mask & (1<<i))
                {
                    light->enable();
                    light->setDiffuse(mHWLightColors[i]);
                }
                else
                {
                    light->disable();
                    light->setDiffuse(LLColor4::black);
                }
            }
            stop_glerror();
        }
        mLightMask = mask;
        stop_glerror();
    }
}

void LLPipeline::enableLightsDynamic()
{
    assertInitialized();
    U32 mask = 0xff & (~2); // Local lights
    enableLights(mask);

    if (isAgentAvatarValid())
    {
        if (gAgentAvatarp->mSpecialRenderMode == 0) // normal
        {
            gPipeline.enableLightsAvatar();
        }
        else if (gAgentAvatarp->mSpecialRenderMode == 2)  // anim preview
        {
            gPipeline.enableLightsAvatarEdit(LLColor4(0.7f, 0.6f, 0.3f, 1.f));
        }
    }
}

void LLPipeline::enableLightsAvatar()
{
    U32 mask = 0xff; // All lights
    setupAvatarLights(false);
    enableLights(mask);
}

void LLPipeline::enableLightsPreview()
{
    disableLights();

    LLColor4 ambient = PreviewAmbientColor;
    gGL.setAmbientLightColor(ambient);

    LLColor4 diffuse0 = PreviewDiffuse0;
    LLColor4 specular0 = PreviewSpecular0;
    LLColor4 diffuse1 = PreviewDiffuse1;
    LLColor4 specular1 = PreviewSpecular1;
    LLColor4 diffuse2 = PreviewDiffuse2;
    LLColor4 specular2 = PreviewSpecular2;

    LLVector3 dir0 = PreviewDirection0;
    LLVector3 dir1 = PreviewDirection1;
    LLVector3 dir2 = PreviewDirection2;

    dir0.normVec();
    dir1.normVec();
    dir2.normVec();

    LLVector4 light_pos(dir0, 0.0f);

    LLLightState* light = gGL.getLight(1);

    light->enable();
    light->setPosition(light_pos);
    light->setDiffuse(diffuse0);
    light->setAmbient(ambient);
    light->setSpecular(specular0);
    light->setSpotExponent(0.f);
    light->setSpotCutoff(180.f);

    light_pos = LLVector4(dir1, 0.f);

    light = gGL.getLight(2);
    light->enable();
    light->setPosition(light_pos);
    light->setDiffuse(diffuse1);
    light->setAmbient(ambient);
    light->setSpecular(specular1);
    light->setSpotExponent(0.f);
    light->setSpotCutoff(180.f);

    light_pos = LLVector4(dir2, 0.f);
    light = gGL.getLight(3);
    light->enable();
    light->setPosition(light_pos);
    light->setDiffuse(diffuse2);
    light->setAmbient(ambient);
    light->setSpecular(specular2);
    light->setSpotExponent(0.f);
    light->setSpotCutoff(180.f);
}


void LLPipeline::enableLightsAvatarEdit(const LLColor4& color)
{
    U32 mask = 0x2002; // Avatar backlight only, set ambient
    setupAvatarLights(true);
    enableLights(mask);

    gGL.setAmbientLightColor(color);
}

void LLPipeline::enableLightsFullbright()
{
    assertInitialized();
    U32 mask = 0x1000; // Non-0 mask, set ambient
    enableLights(mask);
}

void LLPipeline::disableLights()
{
    enableLights(0); // no lighting (full bright)
}

//============================================================================

class LLMenuItemGL;
class LLInvFVBridge;
struct cat_folder_pair;
class LLVOBranch;
class LLVOLeaf;

void LLPipeline::findReferences(LLDrawable *drawablep)
{
    assertInitialized();
    if (mLights.find(drawablep) != mLights.end())
    {
        LL_INFOS() << "In mLights" << LL_ENDL;
    }
    if (std::find(mMovedList.begin(), mMovedList.end(), drawablep) != mMovedList.end())
    {
        LL_INFOS() << "In mMovedList" << LL_ENDL;
    }
    if (std::find(mShiftList.begin(), mShiftList.end(), drawablep) != mShiftList.end())
    {
        LL_INFOS() << "In mShiftList" << LL_ENDL;
    }
    if (mRetexturedList.find(drawablep) != mRetexturedList.end())
    {
        LL_INFOS() << "In mRetexturedList" << LL_ENDL;
    }

    if (std::find(mBuildQ1.begin(), mBuildQ1.end(), drawablep) != mBuildQ1.end())
    {
        LL_INFOS() << "In mBuildQ1" << LL_ENDL;
    }

    S32 count;

    count = gObjectList.findReferences(drawablep);
    if (count)
    {
        LL_INFOS() << "In other drawables: " << count << " references" << LL_ENDL;
    }
}

bool LLPipeline::verify()
{
    bool ok = assertInitialized();
    if (ok)
    {
        for (pool_set_t::iterator iter = mPools.begin(); iter != mPools.end(); ++iter)
        {
            LLDrawPool *poolp = *iter;
            if (!poolp->verify())
            {
                ok = false;
            }
        }
    }

    if (!ok)
    {
        LL_WARNS() << "Pipeline verify failed!" << LL_ENDL;
    }
    return ok;
}

//////////////////////////////
//
// Collision detection
//
//

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 *  A method to compute a ray-AABB intersection.
 *  Original code by Andrew Woo, from "Graphics Gems", Academic Press, 1990
 *  Optimized code by Pierre Terdiman, 2000 (~20-30% faster on my Celeron 500)
 *  Epsilon value added by Klaus Hartmann. (discarding it saves a few cycles only)
 *
 *  Hence this version is faster as well as more robust than the original one.
 *
 *  Should work provided:
 *  1) the integer representation of 0.0f is 0x00000000
 *  2) the sign bit of the float is the most significant one
 *
 *  Report bugs: p.terdiman@codercorner.com
 *
 *  \param      aabb        [in] the axis-aligned bounding box
 *  \param      origin      [in] ray origin
 *  \param      dir         [in] ray direction
 *  \param      coord       [out] impact coordinates
 *  \return     true if ray intersects AABB
 */
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//#define RAYAABB_EPSILON 0.00001f
// Read a float's IEEE-754 bit pattern as U32. The previous (U32&)x form
// was a strict-aliasing violation; std::bit_cast lowers to the same load
// without UB.
#define IR(x)   (std::bit_cast<U32>(x))

bool LLRayAABB(const LLVector3 &center, const LLVector3 &size, const LLVector3& origin, const LLVector3& dir, LLVector3 &coord, F32 epsilon)
{
    bool Inside = true;
    LLVector3 MinB = center - size;
    LLVector3 MaxB = center + size;
    LLVector3 MaxT;
    MaxT.mV[VX]=MaxT.mV[VY]=MaxT.mV[VZ]=-1.0f;

    // Find candidate planes.
    for(U32 i=0;i<3;i++)
    {
        if(origin.mV[i] < MinB.mV[i])
        {
            coord.mV[i] = MinB.mV[i];
            Inside      = false;

            // Calculate T distances to candidate planes
            if(IR(dir.mV[i]))   MaxT.mV[i] = (MinB.mV[i] - origin.mV[i]) / dir.mV[i];
        }
        else if(origin.mV[i] > MaxB.mV[i])
        {
            coord.mV[i] = MaxB.mV[i];
            Inside      = false;

            // Calculate T distances to candidate planes
            if(IR(dir.mV[i]))   MaxT.mV[i] = (MaxB.mV[i] - origin.mV[i]) / dir.mV[i];
        }
    }

    // Ray origin inside bounding box
    if(Inside)
    {
        coord = origin;
        return true;
    }

    // Get largest of the maxT's for final choice of intersection
    U32 WhichPlane = 0;
    if(MaxT.mV[1] > MaxT.mV[WhichPlane])    WhichPlane = 1;
    if(MaxT.mV[2] > MaxT.mV[WhichPlane])    WhichPlane = 2;

    // Check final candidate actually inside box
    if(IR(MaxT.mV[WhichPlane])&0x80000000) return false;

    for(U32 i=0;i<3;i++)
    {
        if(i!=WhichPlane)
        {
            coord.mV[i] = origin.mV[i] + MaxT.mV[WhichPlane] * dir.mV[i];
            if (epsilon > 0)
            {
                if(coord.mV[i] < MinB.mV[i] - epsilon || coord.mV[i] > MaxB.mV[i] + epsilon)    return false;
            }
            else
            {
                if(coord.mV[i] < MinB.mV[i] || coord.mV[i] > MaxB.mV[i])    return false;
            }
        }
    }
    return true;    // ray hits box
}

//////////////////////////////
//
// Macros, functions, and inline methods from other classes
//
//

void LLPipeline::setLight(LLDrawable *drawablep, bool is_light)
{
    if (drawablep && assertInitialized())
    {
        if (is_light)
        {
            mLights.insert(drawablep);
            drawablep->setState(LLDrawable::LIGHT);
        }
        else
        {
            drawablep->clearState(LLDrawable::LIGHT);
            mLights.erase(drawablep);
        }
    }
}

//static
void LLPipeline::toggleRenderType(U32 type)
{
    gPipeline.mRenderTypeEnabled[type] = !gPipeline.mRenderTypeEnabled[type];
    if (type == LLPipeline::RENDER_TYPE_WATER)
    {
        gPipeline.mRenderTypeEnabled[LLPipeline::RENDER_TYPE_VOIDWATER] = !gPipeline.mRenderTypeEnabled[LLPipeline::RENDER_TYPE_VOIDWATER];
    }
}

//static
void LLPipeline::toggleRenderTypeControl(U32 type)
{
    gPipeline.toggleRenderType(type);
}

//static
bool LLPipeline::hasRenderTypeControl(U32 type)
{
    return gPipeline.hasRenderType(type);
}

// Allows UI items labeled "Hide foo" instead of "Show foo"
//static
bool LLPipeline::toggleRenderTypeControlNegated(S32 type)
{
    return !gPipeline.hasRenderType(type);
}

//static
void LLPipeline::toggleRenderDebug(U64 bit)
{
    if (gPipeline.hasRenderDebugMask(bit))
    {
        LL_INFOS() << "Toggling render debug mask " << std::hex << bit << " off" << std::dec << LL_ENDL;
    }
    else
    {
        LL_INFOS() << "Toggling render debug mask " << std::hex << bit << " on" << std::dec << LL_ENDL;
    }
    gPipeline.mRenderDebugMask ^= bit;
}


//static
bool LLPipeline::toggleRenderDebugControl(U64 bit)
{
    return gPipeline.hasRenderDebugMask(bit);
}

//static
void LLPipeline::toggleRenderDebugFeature(U32 bit)
{
    gPipeline.mRenderDebugFeatureMask ^= bit;
}


//static
bool LLPipeline::toggleRenderDebugFeatureControl(U32 bit)
{
    return gPipeline.hasRenderDebugFeatureMask(bit);
}

void LLPipeline::setRenderDebugFeatureControl(U32 bit, bool value)
{
    if (value)
    {
        gPipeline.mRenderDebugFeatureMask |= bit;
    }
    else
    {
        gPipeline.mRenderDebugFeatureMask &= !bit;
    }
}

void LLPipeline::pushRenderDebugFeatureMask()
{
    mRenderDebugFeatureStack.push(mRenderDebugFeatureMask);
}

void LLPipeline::popRenderDebugFeatureMask()
{
    if (mRenderDebugFeatureStack.empty())
    {
        LL_ERRS() << "Depleted render feature stack." << LL_ENDL;
    }

    mRenderDebugFeatureMask = mRenderDebugFeatureStack.top();
    mRenderDebugFeatureStack.pop();
}

// static
void LLPipeline::setRenderScriptedBeacons(bool val)
{
    sRenderScriptedBeacons = val;
}

// static
void LLPipeline::toggleRenderScriptedBeacons()
{
    sRenderScriptedBeacons = !sRenderScriptedBeacons;
}

// static
bool LLPipeline::getRenderScriptedBeacons()
{
    return sRenderScriptedBeacons;
}

// static
void LLPipeline::setRenderScriptedTouchBeacons(bool val)
{
    sRenderScriptedTouchBeacons = val;
}

// static
void LLPipeline::toggleRenderScriptedTouchBeacons()
{
    sRenderScriptedTouchBeacons = !sRenderScriptedTouchBeacons;
}

// static
bool LLPipeline::getRenderScriptedTouchBeacons()
{
    return sRenderScriptedTouchBeacons;
}

// static
void LLPipeline::setRenderMOAPBeacons(bool val)
{
    sRenderMOAPBeacons = val;
}

// static
void LLPipeline::toggleRenderMOAPBeacons()
{
    sRenderMOAPBeacons = !sRenderMOAPBeacons;
}

// static
bool LLPipeline::getRenderMOAPBeacons()
{
    return sRenderMOAPBeacons;
}

// static
void LLPipeline::setRenderPhysicalBeacons(bool val)
{
    sRenderPhysicalBeacons = val;
}

// static
void LLPipeline::toggleRenderPhysicalBeacons()
{
    sRenderPhysicalBeacons = !sRenderPhysicalBeacons;
}

// static
bool LLPipeline::getRenderPhysicalBeacons()
{
    return sRenderPhysicalBeacons;
}

// static
void LLPipeline::setRenderParticleBeacons(bool val)
{
    sRenderParticleBeacons = val;
}

// static
void LLPipeline::toggleRenderParticleBeacons()
{
    sRenderParticleBeacons = !sRenderParticleBeacons;
}

// static
bool LLPipeline::getRenderParticleBeacons()
{
    return sRenderParticleBeacons;
}

// static
void LLPipeline::setRenderSoundBeacons(bool val)
{
    sRenderSoundBeacons = val;
}

// static
void LLPipeline::toggleRenderSoundBeacons()
{
    sRenderSoundBeacons = !sRenderSoundBeacons;
}

// static
bool LLPipeline::getRenderSoundBeacons()
{
    return sRenderSoundBeacons;
}

// static
void LLPipeline::setRenderBeacons(bool val)
{
    sRenderBeacons = val;
}

// static
void LLPipeline::toggleRenderBeacons()
{
    sRenderBeacons = !sRenderBeacons;
}

// static
bool LLPipeline::getRenderBeacons()
{
    return sRenderBeacons;
}

// static
void LLPipeline::setRenderHighlights(bool val)
{
    sRenderHighlight = val;
}

// static
void LLPipeline::toggleRenderHighlights()
{
    sRenderHighlight = !sRenderHighlight;
}

// static
bool LLPipeline::getRenderHighlights()
{
    return sRenderHighlight;
}

// static
void LLPipeline::setRenderHighlightTextureChannel(LLRender::eTexIndex channel)
{
    if (channel != sRenderHighlightTextureChannel)
    {
        sRenderHighlightTextureChannel = channel;
    }
}

LLVOPartGroup* LLPipeline::lineSegmentIntersectParticle(const LLVector4a& start, const LLVector4a& end, LLVector4a* intersection,
                                                        S32* face_hit)
{
    LLVector4a local_end = end;

    LLVector4a position;

    LLDrawable* drawable = NULL;

    for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin();
            iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
    {
        LLViewerRegion* region = *iter;

        LLSpatialPartition* part = region->getSpatialPartition(LLViewerRegion::PARTITION_PARTICLE);
        if (part && hasRenderType(part->mDrawableType))
        {
            LLDrawable* hit = part->lineSegmentIntersect(start, local_end, true, false, true, false, face_hit, &position, NULL, NULL, NULL);
            if (hit)
            {
                drawable = hit;
                local_end = position;
            }
        }
    }

    LLVOPartGroup* ret = NULL;
    if (drawable)
    {
        //make sure we're returning an LLVOPartGroup
        llassert(drawable->getVObj()->getPCode() == LLViewerObject::LL_VO_PART_GROUP);
        ret = (LLVOPartGroup*) drawable->getVObj().get();
    }

    if (intersection)
    {
        *intersection = position;
    }

    return ret;
}

LLViewerObject* LLPipeline::lineSegmentIntersectInWorld(const LLVector4a& start, const LLVector4a& end,
                                                        bool pick_transparent,
                                                        bool pick_rigged,
                                                        bool pick_unselectable,
                                                        bool pick_reflection_probe,
                                                        S32* face_hit,
                                                        LLVector4a* intersection,         // return the intersection point
                                                        LLVector2* tex_coord,            // return the texture coordinates of the intersection point
                                                        LLVector4a* normal,               // return the surface normal at the intersection point
                                                        LLVector4a* tangent             // return the surface tangent at the intersection point
    )
{
    LLDrawable* drawable = NULL;

    LLVector4a local_end = end;

    LLVector4a position;

    sPickAvatar = false; //! LLToolMgr::getInstance()->inBuildMode();

    for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin();
            iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
    {
        LLViewerRegion* region = *iter;

        for (U32 j = 0; j < LLViewerRegion::NUM_PARTITIONS; j++)
        {
            if ((j == LLViewerRegion::PARTITION_VOLUME) ||
                (j == LLViewerRegion::PARTITION_BRIDGE) ||
                (j == LLViewerRegion::PARTITION_AVATAR) || // for attachments
                (j == LLViewerRegion::PARTITION_CONTROL_AV) ||
                (j == LLViewerRegion::PARTITION_TERRAIN) ||
                (j == LLViewerRegion::PARTITION_TREE) ||
                (j == LLViewerRegion::PARTITION_GRASS))  // only check these partitions for now
            {
                LLSpatialPartition* part = region->getSpatialPartition(j);
                if (part && hasRenderType(part->mDrawableType))
                {
                    LLDrawable* hit = part->lineSegmentIntersect(start, local_end, pick_transparent, pick_rigged, pick_unselectable, pick_reflection_probe, face_hit, &position, tex_coord, normal, tangent);
                    if (hit)
                    {
                        drawable = hit;
                        local_end = position;
                    }
                }
            }
        }
    }

    if (!sPickAvatar)
    {
        //save hit info in case we need to restore
        //due to attachment override
        LLVector4a local_normal;
        LLVector4a local_tangent;
        LLVector2 local_texcoord;
        S32 local_face_hit = -1;

        if (face_hit)
        {
            local_face_hit = *face_hit;
        }
        if (tex_coord)
        {
            local_texcoord = *tex_coord;
        }
        if (tangent)
        {
            local_tangent = *tangent;
        }
        else
        {
            local_tangent.clear();
        }
        if (normal)
        {
            local_normal = *normal;
        }
        else
        {
            local_normal.clear();
        }

        const F32 ATTACHMENT_OVERRIDE_DIST = 0.1f;

        //check against avatars
        sPickAvatar = true;
        for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin();
                iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
        {
            LLViewerRegion* region = *iter;

            LLSpatialPartition* part = region->getSpatialPartition(LLViewerRegion::PARTITION_AVATAR);
            if (part && hasRenderType(part->mDrawableType))
            {
                LLDrawable* hit = part->lineSegmentIntersect(start, local_end, pick_transparent, pick_rigged, pick_unselectable, pick_reflection_probe, face_hit, &position, tex_coord, normal, tangent);
                if (hit)
                {
                    LLVector4a delta;
                    delta.setSub(position, local_end);

                    if (!drawable ||
                        !drawable->getVObj()->isAttachment() ||
                        delta.getLength3().getF32() > ATTACHMENT_OVERRIDE_DIST)
                    { //avatar overrides if previously hit drawable is not an attachment or
                      //attachment is far enough away from detected intersection
                        drawable = hit;
                        local_end = position;
                    }
                    else
                    { //prioritize attachments over avatars
                        position = local_end;

                        if (face_hit)
                        {
                            *face_hit = local_face_hit;
                        }
                        if (tex_coord)
                        {
                            *tex_coord = local_texcoord;
                        }
                        if (tangent)
                        {
                            *tangent = local_tangent;
                        }
                        if (normal)
                        {
                            *normal = local_normal;
                        }
                    }
                }
            }
        }
    }

    // check all avatar nametags (silly, isn't it?)
    for (LLCharacter* character : LLCharacter::sInstances)
    {
        LLVOAvatar* avatar = (LLVOAvatar*)character;
        if (avatar->mNameText.notNull() &&
            avatar->mNameText->lineSegmentIntersect(start, local_end, position))
        {
            drawable = avatar->mDrawable;
            local_end = position;
        }
    }

    if (intersection)
    {
        *intersection = position;
    }

    return drawable ? drawable->getVObj().get() : NULL;
}

LLViewerObject* LLPipeline::lineSegmentIntersectInHUD(const LLVector4a& start, const LLVector4a& end,
                                                      bool pick_transparent,
                                                      S32* face_hit,
                                                      LLVector4a* intersection,         // return the intersection point
                                                      LLVector2* tex_coord,            // return the texture coordinates of the intersection point
                                                      LLVector4a* normal,               // return the surface normal at the intersection point
                                                      LLVector4a* tangent               // return the surface tangent at the intersection point
    )
{
    LLDrawable* drawable = NULL;

    for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin();
            iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
    {
        LLViewerRegion* region = *iter;

        bool toggle = false;
        if (!hasRenderType(LLPipeline::RENDER_TYPE_HUD))
        {
            toggleRenderType(LLPipeline::RENDER_TYPE_HUD);
            toggle = true;
        }

        LLSpatialPartition* part = region->getSpatialPartition(LLViewerRegion::PARTITION_HUD);
        if (part)
        {
            LLDrawable* hit = part->lineSegmentIntersect(start, end, pick_transparent, false, true, false, face_hit, intersection, tex_coord, normal, tangent);
            if (hit)
            {
                drawable = hit;
            }
        }

        if (toggle)
        {
            toggleRenderType(LLPipeline::RENDER_TYPE_HUD);
        }
    }
    return drawable ? drawable->getVObj().get() : NULL;
}

LLSpatialPartition* LLPipeline::getSpatialPartition(LLViewerObject* vobj)
{
    if (vobj)
    {
        LLViewerRegion* region = vobj->getRegion();
        if (region)
        {
            return region->getSpatialPartition(vobj->getPartitionType());
        }
    }
    return NULL;
}

void LLPipeline::resetVertexBuffers(LLDrawable* drawable)
{
    if (!drawable)
    {
        return;
    }

    for (S32 i = 0; i < drawable->getNumFaces(); i++)
    {
        LLFace* facep = drawable->getFace(i);
        if (facep)
        {
            facep->clearVertexBuffer();
        }
    }
}

void LLPipeline::renderObjects(U32 type, bool texture, bool batch_texture, bool rigged)
{
    assertInitialized();
    gGL.loadMatrix(gGLModelView);
    gGLLastMatrix = NULL;

    if (rigged)
    {
        mSimplePool->pushRiggedBatches(type + 1, texture, batch_texture);
    }
    else
    {
        mSimplePool->pushBatches(type, texture, batch_texture);
    }

    gGL.loadMatrix(gGLModelView);
    gGLLastMatrix = NULL;
}

void LLPipeline::renderGLTFObjects(U32 type, bool texture, bool rigged)
{
    assertInitialized();
    gGL.loadMatrix(gGLModelView);
    gGLLastMatrix = NULL;

    if (rigged)
    {
        mSimplePool->pushRiggedGLTFBatches(type + 1, texture);
    }
    else
    {
        mSimplePool->pushGLTFBatches(type, texture);
    }

    gGL.loadMatrix(gGLModelView);
    gGLLastMatrix = NULL;
}

// Currently only used for shadows -Cosmic,2023-04-19
void LLPipeline::renderAlphaObjects(bool rigged)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    assertInitialized();
    gGL.loadMatrix(gGLModelView);
    gGLLastMatrix = NULL;
    S32 sun_up = LLEnvironment::instance().getIsSunUp() ? 1 : 0;
    U32 target_width = LLRenderTarget::sCurResX;
    U32 type = LLRenderPass::PASS_ALPHA;
    // for gDeferredShadowAlphaMaskProgram
    const LLVOAvatar* lastAvatar = nullptr;
    U64 lastMeshId = 0;
    bool skipLastSkin;
    // for gDeferredShadowGLTFAlphaBlendProgram
    const LLVOAvatar* lastAvatarGLTF = nullptr;
    U64 lastMeshIdGLTF = 0;
    bool skipLastSkinGLTF;
    // GLTF material bind cache; invalidated in the non-GLTF branches below since
    // mSimplePool->pushBatch rebinds texture units and would clobber the material
    LLFetchedGLTFMaterial* lastMatGLTF = nullptr;
    LLViewerTexture* lastTexGLTF = nullptr;
    auto* begin = gPipeline.beginRenderMap(type);
    auto* end = gPipeline.endRenderMap(type);

    for (LLCullResult::drawinfo_iterator i = begin; i != end; )
    {
        LLDrawInfo* pparams = *i;
        LLCullResult::increment_iterator(i, end);

        if (rigged != (pparams->mAvatar != nullptr))
        {
            // Pool contains both rigged and non-rigged DrawInfos. Only draw
            // the objects we're interested in in this pass.
            continue;
        }

        if (rigged)
        {
            if (pparams->mGLTFMaterial)
            {
                gDeferredShadowGLTFAlphaBlendProgram.bind(rigged);
                LLGLSLShader::sCurBoundShaderPtr->uniform1i(LLShaderMgr::SUN_UP_FACTOR, sun_up);
                LLGLSLShader::sCurBoundShaderPtr->uniform1f(LLShaderMgr::DEFERRED_SHADOW_TARGET_WIDTH, (float)target_width);
                LLGLSLShader::sCurBoundShaderPtr->setMinimumAlpha(ALPHA_BLEND_CUTOFF);
                LLRenderPass::pushRiggedGLTFBatch(*pparams, lastAvatarGLTF, lastMeshIdGLTF, skipLastSkinGLTF, lastMatGLTF, lastTexGLTF);
            }
            else
            {
                gDeferredShadowAlphaMaskProgram.bind(rigged);
                LLGLSLShader::sCurBoundShaderPtr->uniform1i(LLShaderMgr::SUN_UP_FACTOR, sun_up);
                LLGLSLShader::sCurBoundShaderPtr->uniform1f(LLShaderMgr::DEFERRED_SHADOW_TARGET_WIDTH, (float)target_width);
                LLGLSLShader::sCurBoundShaderPtr->setMinimumAlpha(ALPHA_BLEND_CUTOFF);
                lastMatGLTF = nullptr; // pushBatch clobbers texture units
                lastTexGLTF = nullptr;
                if (mSimplePool->uploadMatrixPalette(pparams->mAvatar, pparams->mSkinInfo, lastAvatar, lastMeshId, skipLastSkin))
                {
                    mSimplePool->pushBatch(*pparams, true, true);
                }
            }
        }
        else
        {
            if (pparams->mGLTFMaterial)
            {
                gDeferredShadowGLTFAlphaBlendProgram.bind(rigged);
                LLGLSLShader::sCurBoundShaderPtr->uniform1i(LLShaderMgr::SUN_UP_FACTOR, sun_up);
                LLGLSLShader::sCurBoundShaderPtr->uniform1f(LLShaderMgr::DEFERRED_SHADOW_TARGET_WIDTH, (float)target_width);
                LLGLSLShader::sCurBoundShaderPtr->setMinimumAlpha(ALPHA_BLEND_CUTOFF);
                LLRenderPass::pushGLTFBatch(*pparams, lastMatGLTF, lastTexGLTF);
            }
            else
            {
                gDeferredShadowAlphaMaskProgram.bind(rigged);
                LLGLSLShader::sCurBoundShaderPtr->uniform1i(LLShaderMgr::SUN_UP_FACTOR, sun_up);
                LLGLSLShader::sCurBoundShaderPtr->uniform1f(LLShaderMgr::DEFERRED_SHADOW_TARGET_WIDTH, (float)target_width);
                LLGLSLShader::sCurBoundShaderPtr->setMinimumAlpha(ALPHA_BLEND_CUTOFF);
                lastMatGLTF = nullptr; // pushBatch clobbers texture units
                lastTexGLTF = nullptr;
                mSimplePool->pushBatch(*pparams, true, true);
            }
        }
    }

    gGL.loadMatrix(gGLModelView);
    gGLLastMatrix = NULL;
}

// Currently only used for shadows -Cosmic,2023-04-19
void LLPipeline::renderMaskedObjects(U32 type, bool texture, bool batch_texture, bool rigged)
{
    assertInitialized();
    gGL.loadMatrix(gGLModelView);
    gGLLastMatrix = NULL;
    if (rigged)
    {
        mAlphaMaskPool->pushRiggedMaskBatches(type+1, texture, batch_texture);
    }
    else
    {
        mAlphaMaskPool->pushMaskBatches(type, texture, batch_texture);
    }
    gGL.loadMatrix(gGLModelView);
    gGLLastMatrix = NULL;
}

// Currently only used for shadows -Cosmic,2023-04-19
void LLPipeline::renderFullbrightMaskedObjects(U32 type, bool texture, bool batch_texture, bool rigged)
{
    assertInitialized();
    gGL.loadMatrix(gGLModelView);
    gGLLastMatrix = NULL;
    if (rigged)
    {
        mFullbrightAlphaMaskPool->pushRiggedMaskBatches(type+1, texture, batch_texture);
    }
    else
    {
        mFullbrightAlphaMaskPool->pushMaskBatches(type, texture, batch_texture);
    }
    gGL.loadMatrix(gGLModelView);
    gGLLastMatrix = NULL;
}

void apply_cube_face_rotation(U32 face)
{
    switch (face)
    {
        case 0:
            gGL.rotatef(90.f, 0, 1, 0);
            gGL.rotatef(180.f, 1, 0, 0);
        break;
        case 2:
            gGL.rotatef(-90.f, 1, 0, 0);
        break;
        case 4:
            gGL.rotatef(180.f, 0, 1, 0);
            gGL.rotatef(180.f, 0, 0, 1);
        break;
        case 1:
            gGL.rotatef(-90.f, 0, 1, 0);
            gGL.rotatef(180.f, 1, 0, 0);
        break;
        case 3:
            gGL.rotatef(90, 1, 0, 0);
        break;
        case 5:
            gGL.rotatef(180, 0, 0, 1);
        break;
    }
}

void validate_framebuffer_object()
{
    GLenum status;
    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    switch(status)
    {
        case GL_FRAMEBUFFER_COMPLETE:
            //framebuffer OK, no error.
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            // frame buffer not OK: probably means unsupported depth buffer format
            LL_ERRS() << "Framebuffer Incomplete Missing Attachment." << LL_ENDL;
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            // frame buffer not OK: probably means unsupported depth buffer format
            LL_ERRS() << "Framebuffer Incomplete Attachment." << LL_ENDL;
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED:
            /* choose different formats */
            LL_ERRS() << "Framebuffer unsupported." << LL_ENDL;
            break;
        default:
            LL_ERRS() << "Unknown framebuffer status." << LL_ENDL;
            break;
    }
}

void LLPipeline::bindScreenToTexture()
{

}

void LLPipeline::visualizeBuffers(LLRenderTarget* src, LLRenderTarget* dst, U32 bufferIndex)
{
    dst->bindTarget();
    gDeferredBufferVisualProgram.bind();
    gDeferredBufferVisualProgram.bindTexture(LLShaderMgr::DEFERRED_DIFFUSE, src, false, LLTexUnit::TFO_BILINEAR, bufferIndex);

    static LLStaticHashedString mipLevel("mipLevel");
    if (RenderBufferVisualization != 4)
        gDeferredBufferVisualProgram.uniform1f(mipLevel, 0);
    else
        gDeferredBufferVisualProgram.uniform1f(mipLevel, 8);

    mScreenTriangleVB->setBuffer();
    mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
    gDeferredBufferVisualProgram.unbind();
    dst->flush();
}

// [BDMerge A5.4-1a] Velocity buffer debug visualization (BDMergeVelocityDebug).
// Blits mVelocityMap into dst as a colour field: rightward screen motion -> red,
// upward -> green, static -> neutral grey. Used to VALIDATE the motion vectors in
// world before Phase 1b/2 build on them. No-op if the buffer is not allocated.
void LLPipeline::renderVelocityDebug(LLRenderTarget* dst)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;

    if (!mVelocityMap.isComplete() || !gVelocityDebugProgram.isComplete())
    {
        return;
    }

    dst->bindTarget();
    gVelocityDebugProgram.bind();
    gVelocityDebugProgram.bindTexture(LLShaderMgr::DEFERRED_VELOCITY, &mVelocityMap, false, LLTexUnit::TFO_POINT);
    gVelocityDebugProgram.uniform1f(LLShaderMgr::MOTION_BLUR_STRENGTH, (F32)BDMergeMotionBlurStrength);

    mScreenTriangleVB->setBuffer();
    mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

    gVelocityDebugProgram.unbind();
    dst->flush();
}

// [BDMerge A5.4-3] Motion blur composite. Donor: Black Dragon
// renderMotionBlurComposite (pipeline.cpp:8229-8249). 32-tap triangle-weighted
// gather along the per-pixel velocity (camera + rigid + rigged + classic since
// Phase 1b); the shader early-outs under 0.5px of motion, so a static scene is
// a near-passthrough. Runs in the post chain after glow-combine, before DoF
// and all UI/HUD compositing.
void LLPipeline::renderMotionBlurComposite(LLRenderTarget* src, LLRenderTarget* dst)
{
    LL_PROFILE_GPU_ZONE("motion blur");

    dst->bindTarget();

    gDeferredMotionBlurProgram.bind();
    gDeferredMotionBlurProgram.bindTexture(LLShaderMgr::DEFERRED_DIFFUSE, src);
    gDeferredMotionBlurProgram.bindTexture(LLShaderMgr::DEFERRED_VELOCITY, &mVelocityMap);
    gDeferredMotionBlurProgram.uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES,
        (GLfloat)src->getWidth(), (GLfloat)src->getHeight());
    gDeferredMotionBlurProgram.uniform1i(LLShaderMgr::MOTION_BLUR_STRENGTH, BDMergeMotionBlurStrength);

    mScreenTriangleVB->setBuffer();
    mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

    gDeferredMotionBlurProgram.unbind();
    dst->flush();
}

void LLPipeline::generateLuminance(LLRenderTarget* src, LLRenderTarget* dst)
{
    // luminance sample and mipmap generation
    {
        LL_PROFILE_GPU_ZONE("luminance sample");

        dst->bindTarget();

        LLGLDepthTest depth(GL_FALSE, GL_FALSE);

        gLuminanceProgram.bind();

        static LLCachedControl<F32> diffuse_luminance_scale(gSavedSettings, "RenderDiffuseLuminanceScale", 1.0f);

        S32 channel = 0;
        channel = gLuminanceProgram.enableTexture(LLShaderMgr::DEFERRED_DIFFUSE);
        if (channel > -1)
        {
            src->bindTexture(0, channel, LLTexUnit::TFO_POINT);
        }

        channel = gLuminanceProgram.enableTexture(LLShaderMgr::DEFERRED_EMISSIVE);
        if (channel > -1)
        {
            mRT->bloomMip[0].bindTexture(0, channel);
        }

        channel = gLuminanceProgram.enableTexture(LLShaderMgr::NORMAL_MAP);
        if (channel > -1)
        {
            // bind the normal map to get the environment mask
            mRT->deferredScreen.bindTexture(2, channel, LLTexUnit::TFO_POINT);
        }

        static LLStaticHashedString diffuse_luminance_scale_s("diffuse_luminance_scale");
        gLuminanceProgram.uniform1f(diffuse_luminance_scale_s, diffuse_luminance_scale);

        mScreenTriangleVB->setBuffer();
        mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
        dst->flush();

        // note -- unbind AFTER the glGenerateMipMap so time in generatemipmap can be profiled under "Luminance"
        // also note -- keep an eye on the performance of glGenerateMipmap, might need to replace it with a mip generation shader
        gLuminanceProgram.unbind();
    }
}

void LLPipeline::generateExposure(LLRenderTarget* src, LLRenderTarget* dst, bool use_history) {
    // exposure sample
    {
        LL_PROFILE_GPU_ZONE("exposure sample");

        if (use_history)
        {
            // copy last frame's exposure into mLastExposure
            mLastExposure.copyContents(*dst, 0, 0, dst->getWidth(), dst->getHeight(), 0, 0, mLastExposure.getWidth(), mLastExposure.getHeight(),
                             GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }

        dst->bindTarget();

        LLGLDepthTest depth(GL_FALSE, GL_FALSE);

        LLGLSLShader* shader;
        if (use_history)
        {
            shader = &gExposureProgram;
        }
        else
        {
            shader = &gExposureProgramNoFade;
        }

        shader->bind();

        S32 channel = shader->enableTexture(LLShaderMgr::DEFERRED_EMISSIVE);
        if (channel > -1)
        {
            src->bindTexture(0, channel, LLTexUnit::TFO_TRILINEAR);
        }

        if (use_history)
        {
            channel = shader->enableTexture(LLShaderMgr::EXPOSURE_MAP);
            if (channel > -1)
            {
                mLastExposure.bindTexture(0, channel);
            }
        }

        static LLStaticHashedString dt("dt");
        static LLStaticHashedString noiseVec("noiseVec");
        static LLStaticHashedString dynamic_exposure_params("dynamic_exposure_params");
        static LLStaticHashedString dynamic_exposure_params2("dynamic_exposure_params2");
        static LLStaticHashedString dynamic_exposure_e("dynamic_exposure_enabled");
        static LLCachedControl<bool> should_auto_adjust(gSavedSettings, "RenderSkyAutoAdjustLegacy", false);
        static LLCachedControl<bool> dynamic_exposure_enabled(gSavedSettings, "RenderDynamicExposureEnabled", true);
        static LLCachedControl<F32> dynamic_exposure_coefficient(gSavedSettings, "RenderDynamicExposureCoefficient", 0.175f);
        static LLCachedControl<F32> dynamic_exposure_speed_error(gSavedSettings, "RenderDynamicExposureSpeedError", 0.1f);
        static LLCachedControl<F32> dynamic_exposure_speed_target(gSavedSettings, "RenderDynamicExposureSpeedTarget", 2.f);

        LLSettingsSky::ptr_t sky = LLEnvironment::instance().getCurrentSky();

        F32 probe_ambiance = LLEnvironment::instance().getCurrentSky()->getReflectionProbeAmbiance(should_auto_adjust());

        F32 exp_min = 1.f;
        F32 exp_max = 1.f;

        static LLCachedControl<bool> use_exposure_sky_settings(gSavedSettings, "RenderUseExposureSkySettings", false);

        if (use_exposure_sky_settings)
        {
            if (dynamic_exposure_enabled)
            {
                exp_min = sky->getHDROffset(should_auto_adjust()) - sky->getHDRMin(should_auto_adjust());
                exp_max = sky->getHDROffset(should_auto_adjust()) + sky->getHDRMax(should_auto_adjust());
            }
            else
            {
                exp_min = sky->getHDROffset(should_auto_adjust());
                exp_max = sky->getHDROffset(should_auto_adjust());
            }
        }
        else if (dynamic_exposure_enabled)
        {
            if (probe_ambiance > 0.f)
            {
                F32 hdr_scale = sqrtf(LLEnvironment::instance().getCurrentSky()->getGamma()) * 2.f;

                if (hdr_scale > 1.f)
                {
                    exp_min = 1.f / hdr_scale;
                    exp_max = hdr_scale;
                }
            }
        }

        shader->uniform1f(dt, gFrameIntervalSeconds);
        shader->uniform2f(noiseVec, ll_frand() * 2.0f - 1.0f, ll_frand() * 2.0f - 1.0f);
        shader->uniform4f(dynamic_exposure_params, dynamic_exposure_coefficient, exp_min, exp_max, dynamic_exposure_speed_error);
        shader->uniform4f(dynamic_exposure_params2, sky->getHDROffset(should_auto_adjust()), exp_min, exp_max, dynamic_exposure_speed_target);

        mScreenTriangleVB->setBuffer();
        mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

        if (use_history)
        {
            gGL.getTexUnit(channel)->unbind(mLastExposure.getUsage());
        }
        shader->unbind();
        dst->flush();
    }
}

namespace
{
    // Port of the Kim et al. 2002 Planckian-locus polynomial used by the
    // original white-balance shader. Valid 1667K..25000K to ~1% of the true
    // locus. Output is CIE xy chromaticity at Y = 1.
    inline void cg_cct_to_xy(F32 cct, F32& out_x, F32& out_y)
    {
        F32 t = cct, t2 = t * t, t3 = t2 * t;
        if (t <= 4000.0f)
            out_x = -0.2661239e9f / t3 - 0.2343589e6f / t2 + 0.8776956e3f / t + 0.179910f;
        else
            out_x = -3.0258469e9f / t3 + 2.1070379e6f / t2 + 0.2226347e3f / t + 0.240390f;

        F32 x2 = out_x * out_x, x3 = x2 * out_x;
        if (t <= 2222.0f)
            out_y = -1.1063814f  * x3 - 1.34811020f * x2 + 2.18555832f * out_x - 0.20219683f;
        else if (t <= 4000.0f)
            out_y = -0.9549476f  * x3 - 1.37418593f * x2 + 2.09137015f * out_x - 0.16748867f;
        else
            out_y =  3.0817580f  * x3 - 5.87338670f * x2 + 3.75112997f * out_x - 0.37001483f;
    }

    // Apply a Duv offset perpendicular to the Planckian locus via a central-
    // difference tangent in CIE 1960 u,v space (O(h²) accurate). Returns the
    // new CIE xy chromaticity.
    inline void cg_apply_duv(F32 cct, F32 duv, F32& out_x, F32& out_y)
    {
        F32 xA, yA, xB, yB;
        cg_cct_to_xy(cct - 1.0f, xA, yA);
        cg_cct_to_xy(cct + 1.0f, xB, yB);

        F32 dA  = -2.0f * xA + 12.0f * yA + 3.0f;
        F32 uA  = 4.0f * xA / dA;
        F32 vA  = 6.0f * yA / dA;
        F32 dB  = -2.0f * xB + 12.0f * yB + 3.0f;
        F32 uB  = 4.0f * xB / dB;
        F32 vB  = 6.0f * yB / dB;

        F32 uMid = 0.5f * (uA + uB);
        F32 vMid = 0.5f * (vA + vB);
        F32 tx = uB - uA;
        F32 ty = vB - vA;
        F32 tlen = sqrtf(tx * tx + ty * ty);
        if (tlen > 1e-12f) { tx /= tlen; ty /= tlen; }

        F32 u = uMid + (-ty) * duv;  // perp = (-tangent.y, tangent.x)
        F32 v = vMid + ( tx) * duv;

        F32 dBack = 2.0f * u - 8.0f * v + 4.0f;
        out_x = 3.0f * u / dBack;
        out_y = 2.0f * v / dBack;
    }

    // Resolve the artist's (CCT offset, Duv) pair into a linear-sRGB gain,
    // normalised so green pins to 1 (preserves luminance). Duv sign is
    // flipped to match the tint convention: +Duv pushes green, -Duv magenta.
    inline LLVector3 cg_compute_white_balance_gain(F32 cct_offset, F32 duv)
    {
        if (fabsf(cct_offset) < 1e-3f && fabsf(duv) < 1e-5f)
            return LLVector3(1.f, 1.f, 1.f);

        constexpr F32 D65_CCT = 6504.0f;
        F32 target_cct = llclamp(D65_CCT + cct_offset, 1667.0f, 25000.0f);

        F32 x, y;
        cg_apply_duv(target_cct, -duv, x, y);

        F32 X = x / y;
        F32 Y = 1.0f;
        F32 Z = (1.0f - x - y) / y;

        F32 r =  3.2404542f * X - 1.5371385f * Y - 0.4985314f * Z;
        F32 g = -0.9692660f * X + 1.8760108f * Y + 0.0415560f * Z;
        F32 b =  0.0556434f * X - 0.2040259f * Y + 1.0572252f * Z;

        F32 inv_g = 1.0f / llmax(g, 1e-6f);
        return LLVector3(r * inv_g, 1.0f, b * inv_g);
    }
}

void LLPipeline::colorCorrect(LLRenderTarget* src, LLRenderTarget* dst, bool apply_tonemap, bool apply_color_grade)
{
    LL_PROFILE_GPU_ZONE("colorcorrect");

    dst->bindTarget();
    {
        LLGLDepthTest depth(GL_FALSE, GL_FALSE);

        // Apply gamma correction to the frame here.
        static LLCachedControl<bool> color_grade_cc(gSavedSettings, "RenderColorGrade", false);
        static LLCachedControl<bool> should_auto_adjust(gSavedSettings, "RenderSkyAutoAdjustLegacy", false);
        static LLCachedControl<bool> buildNoPost(gSavedSettings, "RenderDisablePostProcessing", false);

        LLSettingsSky::ptr_t psky = LLEnvironment::instance().getCurrentSky();

        bool color_grade = apply_color_grade && color_grade_cc;
        bool legacy_gamma = psky->getReflectionProbeAmbiance(should_auto_adjust) == 0.f;
        bool no_post = gSnapshotNoPost || legacy_gamma || (buildNoPost && gFloaterTools && gFloaterTools->isAvailable());
        LLGLSLShader* shader = nullptr;
        if (apply_tonemap)
        {
            if (legacy_gamma)
            {
                shader = no_post       ? color_grade ? &gCGColorgradeLegacyGammaProgram : &gCGLegacyGammaProgram
                         : color_grade ? &gCGTonemapColorgradeLegacyGammaProgram
                                       : &gCGTonemapLegacyGammaProgram;
            }
            else
            {
                shader = no_post       ? color_grade ? &gCGColorgradeGammaProgram : &gCGGammaProgram
                         : color_grade ? &gCGTonemapColorgradeProgram
                                       : &gCGTonemapProgram;
            }
        }
        else
        {
            shader = legacy_gamma ? &gCGLegacyGammaProgram : &gCGGammaProgram;
        }

        shader->bind();

        S32 diffuse_channel = shader->bindTexture(LLShaderMgr::DEFERRED_DIFFUSE, src, false, LLTexUnit::TFO_POINT);
        S32 depth_channel = shader->bindTexture(LLShaderMgr::DEFERRED_DEPTH, &mRT->deferredScreen, true);
        S32 exposure_channel = shader->bindTexture(LLShaderMgr::EXPOSURE_MAP, &mExposureMap);

        // HDR bloom pyramid is folded into the tonemap variants of this shader
        // (permutation BLOOM_COMPOSITE). Bind the pyramid top + strength here so
        // the composite no longer needs its own pass. When HDR is off the shader
        // variant lacks the sampler and bindTexture is a no-op via getTextureChannel.
        S32 bloom_channel = -1;
        if (mRT->bloomMipCount > 0)
        {
            bloom_channel = shader->bindTexture(LLShaderMgr::BLOOM_SAMPLER, &mRT->bloomMip[0], false, LLTexUnit::TFO_BILINEAR);
            if (bloom_channel > -1)
            {
                static LLCachedControl<F32>      bloom_strength(gSavedSettings, "RenderBloomStrength", 0.325f);
                static LLCachedControl<F32>      halation_strength(gSavedSettings, "RenderBloomHalationStrength", 0.0f);
                static LLCachedControl<LLColor3> halation_tint(gSavedSettings, "RenderBloomHalationTint", LLColor3(1.0f, 0.35f, 0.15f));
                // Pyramid needs at least 3 mips for the downsample/upsample chain
                // to be meaningful; otherwise gate the signal to zero so a partial
                // allocation doesn't leak an unfiltered mip into the scene.
                const F32 strength_gate = (mRT->bloomMipCount >= 3) ? 1.0f : 0.0f;
                shader->uniform1f(LLShaderMgr::BLOOM_STRENGTH,    llmax(bloom_strength(), 0.0f)    * strength_gate);
                shader->uniform1f(LLShaderMgr::HALATION_STRENGTH, llmax(halation_strength(), 0.0f) * strength_gate);
                const LLColor3& tint = halation_tint();
                shader->uniform3f(LLShaderMgr::HALATION_TINT, tint.mV[0], tint.mV[1], tint.mV[2]);
            }
        }

        shader->uniform2f(LLShaderMgr::SCREEN_RESOLUTION, (GLfloat)src->getWidth(), (GLfloat)src->getHeight());

        // Chromatic aberration parameters
        static LLCachedControl<F32> chromatic_aberration_strength(gSavedSettings, "RenderChromaticAberrationStrength", 0.f);
        static LLCachedControl<F32> chromatic_aberration_falloff(gSavedSettings, "RenderChromaticAberrationFalloff", 1.f);
        static LLCachedControl<F32> chromatic_aberration_angle(gSavedSettings, "RenderChromaticAberrationAngle", 0.f);
        static LLCachedControl<F32> chromatic_aberration_offset_r_x(gSavedSettings, "RenderChromaticAberrationOffsetRX", -1.f);
        static LLCachedControl<F32> chromatic_aberration_offset_r_y(gSavedSettings, "RenderChromaticAberrationOffsetRY", 0.f);
        static LLCachedControl<F32> chromatic_aberration_offset_b_x(gSavedSettings, "RenderChromaticAberrationOffsetBX", 1.f);
        static LLCachedControl<F32> chromatic_aberration_offset_b_y(gSavedSettings, "RenderChromaticAberrationOffsetBY", 0.f);
        static LLCachedControl<F32> chromatic_aberration_anisotropy(gSavedSettings, "RenderChromaticAberrationAnisotropy", 0.f);
        // Precompute shader-friendly forms once on the CPU: square the
        // strength (and pre-apply the 0.02 peak-offset scale), take the
        // reciprocal of falloff, and turn the angle into a (sin, cos) pair.
        // Saves one mul, one div, and one sincos per fragment.
        F32 ca_strength = llclamp(chromatic_aberration_strength(), 0.f, 1.f);
        F32 ca_falloff  = llclamp(chromatic_aberration_falloff(),  0.5f, 4.f);
        F32 ca_angle_rad = llclamp(chromatic_aberration_angle(), 0.f, 360.f) * 0.01745329252f;
        shader->uniform1f(LLShaderMgr::CA_AMOUNT, ca_strength * ca_strength * 0.02f);
        shader->uniform1f(LLShaderMgr::CA_FALLOFF, 1.f / ca_falloff);
        shader->uniform2f(LLShaderMgr::CA_ANGLE_SIN_COS, sinf(ca_angle_rad), cosf(ca_angle_rad));
        shader->uniform2f(LLShaderMgr::CA_OFFSET_R, llclamp(chromatic_aberration_offset_r_x(), -1.f, 1.f), llclamp(chromatic_aberration_offset_r_y(), -1.f, 1.f));
        shader->uniform2f(LLShaderMgr::CA_OFFSET_B, llclamp(chromatic_aberration_offset_b_x(), -1.f, 1.f), llclamp(chromatic_aberration_offset_b_y(), -1.f, 1.f));
        shader->uniform1f(LLShaderMgr::CA_ANISOTROPY, llclamp(chromatic_aberration_anisotropy(), -1.f, 1.f));

        // Lens flare parameters
        {
            static LLCachedControl<F32> lens_flare_strength(gSavedSettings, "RenderLensFlareStrength", 0.f);
            static LLCachedControl<F32> lens_flare_streak_length(gSavedSettings, "RenderLensFlareStreakLength", 0.5f);
            static LLCachedControl<F32> lens_flare_streak_falloff(gSavedSettings, "RenderLensFlareStreakFalloff", 1.5f);
            static LLCachedControl<F32> lens_flare_streak_thickness(gSavedSettings, "RenderLensFlareStreakThickness", 0.08f);
            static LLCachedControl<F32> lens_flare_streak_intensity(gSavedSettings, "RenderLensFlareStreakIntensity", 1.f);
            static LLCachedControl<LLColor3> lens_flare_streak_tint(gSavedSettings, "RenderLensFlareStreakTint", LLColor3(0.6f, 0.7f, 1.0f));
            static LLCachedControl<F32> lens_flare_chromatic_spread(gSavedSettings, "RenderLensFlareChromaticSpread", 0.08f);
            static LLCachedControl<F32> lens_flare_glow_radius(gSavedSettings, "RenderLensFlareGlowRadius", 0.12f);
            static LLCachedControl<F32> lens_flare_glow_falloff(gSavedSettings, "RenderLensFlareGlowFalloff", 8.f);
            static LLCachedControl<F32> lens_flare_glow(gSavedSettings, "RenderLensFlareGlow", 1.f);
            static LLCachedControl<F32> lens_flare_ghost(gSavedSettings, "RenderLensFlareGhost", 0.f);
            static LLCachedControl<S32> lens_flare_ghost_count(gSavedSettings, "RenderLensFlareGhostCount", 4);
            static LLCachedControl<F32> lens_flare_ghost_spacing(gSavedSettings, "RenderLensFlareGhostSpacing", 0.3f);
            static LLCachedControl<F32> lens_flare_halo(gSavedSettings, "RenderLensFlareHalo", 0.f);
            static LLCachedControl<F32> lens_flare_halo_radius(gSavedSettings, "RenderLensFlareHaloRadius", 0.5f);
            static LLCachedControl<F32> lens_flare_halo_width(gSavedSettings, "RenderLensFlareHaloWidth", 0.15f);
            static LLCachedControl<F32> lens_flare_starburst(gSavedSettings, "RenderLensFlareStarburst", 0.f);
            static LLCachedControl<S32> lens_flare_starburst_spikes(gSavedSettings, "RenderLensFlareStarburstSpikes", 4);
            static LLCachedControl<F32> lens_flare_starburst_sharpness(gSavedSettings, "RenderLensFlareStarburstSharpness", 24.f);
            static LLCachedControl<F32> lens_flare_starburst_length(gSavedSettings, "RenderLensFlareStarburstLength", 0.25f);
            static LLCachedControl<F32> lens_flare_occlusion_radius(gSavedSettings, "RenderLensFlareOcclusionRadius", 0.02f);
            static LLCachedControl<S32> lens_flare_occlusion_taps(gSavedSettings, "RenderLensFlareOcclusionTaps", 9);

            F32 strength = llclamp(lens_flare_strength(), 0.f, 1.f);
            shader->uniform1f(LLShaderMgr::LENS_FLARE_STRENGTH, strength);

            if (strength > 0.f)
            {
                // Project sun direction to screen UV
                LLEnvironment& environment = LLEnvironment::instance();
                bool sun_up = environment.getIsSunUp();
                LLVector4 light_dir = sun_up ? mSunDir : mMoonDir;

                glm::vec4 sun_clip = get_current_projection() * get_current_modelview() * glm::vec4(light_dir.mV[0], light_dir.mV[1], light_dir.mV[2], 0.0f);

                F32 target_visibility = 0.f;
                if (sun_clip.z > 0.f)
                {
                    glm::vec2 sun_ndc = glm::vec2(sun_clip.x, sun_clip.y) / sun_clip.z;
                    glm::vec2 sun_uv = sun_ndc * 0.5f + 0.5f;

                    // Soft fade as sun approaches screen edges — generous margin
                    // lets the streak persist even when the sun is slightly off-screen.
                    F32 edge_fade = 1.f;
                    F32 margin = 0.2f;
                    edge_fade *= llclamp((sun_uv.x - (-margin)) / margin, 0.f, 1.f);
                    edge_fade *= llclamp(((1.f + margin) - sun_uv.x) / margin, 0.f, 1.f);
                    edge_fade *= llclamp((sun_uv.y - (-margin)) / margin, 0.f, 1.f);
                    edge_fade *= llclamp(((1.f + margin) - sun_uv.y) / margin, 0.f, 1.f);

                    target_visibility = edge_fade;
                    shader->uniform2f(LLShaderMgr::LENS_FLARE_SUN_POS, sun_uv.x, sun_uv.y);
                }

                // Temporally smooth visibility to avoid flicker from depth sampling noise.
                // Fade out faster than fade in for responsive occlusion.
                F32 fade_speed = (target_visibility < mLensFlareSunVisibility) ? 0.15f : 0.05f;
                mLensFlareSunVisibility = std::lerp(mLensFlareSunVisibility, target_visibility, fade_speed);

                shader->uniform1f(LLShaderMgr::LENS_FLARE_SUN_VISIBILITY, mLensFlareSunVisibility);

                // Anamorphic streak
                shader->uniform1f(LLShaderMgr::LENS_FLARE_STREAK_LENGTH, llclamp(lens_flare_streak_length(), 0.01f, 2.f));
                shader->uniform1f(LLShaderMgr::LENS_FLARE_STREAK_FALLOFF, llclamp(lens_flare_streak_falloff(), 0.1f, 10.f));
                // User-facing "thickness" (0-1) maps to the shader's vertical half-thickness in UV space.
                F32 streak_thickness = llclamp(lens_flare_streak_thickness(), 0.f, 1.f) * 0.05f;
                shader->uniform1f(LLShaderMgr::LENS_FLARE_STREAK_WIDTH, llmax(streak_thickness, 0.001f));
                shader->uniform1f(LLShaderMgr::LENS_FLARE_STREAK_INTENSITY, llclamp(lens_flare_streak_intensity(), 0.f, 5.f));
                LLColor3 tint = linearColor3(lens_flare_streak_tint());
                shader->uniform3f(LLShaderMgr::LENS_FLARE_STREAK_TINT, tint.mV[0], tint.mV[1], tint.mV[2]);
                // User-facing 0-1 spread maps to shader's internal UV offset (0-0.1).
                shader->uniform1f(LLShaderMgr::LENS_FLARE_CHROMATIC_SPREAD, llclamp(lens_flare_chromatic_spread(), 0.f, 1.f) * 0.1f);

                // Central glow
                shader->uniform1f(LLShaderMgr::LENS_FLARE_GLOW_RADIUS, llclamp(lens_flare_glow_radius(), 0.01f, 0.5f));
                shader->uniform1f(LLShaderMgr::LENS_FLARE_GLOW_FALLOFF, llclamp(lens_flare_glow_falloff(), 1.f, 30.f));
                shader->uniform1f(LLShaderMgr::LENS_FLARE_GLOW, llclamp(lens_flare_glow(), 0.f, 5.f));

                // Optional ghosts & halo — intensity doubles as the on/off switch.
                shader->uniform1f(LLShaderMgr::LENS_FLARE_GHOST, llclamp(lens_flare_ghost(), 0.f, 5.f));
                shader->uniform1i(LLShaderMgr::LENS_FLARE_GHOST_COUNT, llclamp(lens_flare_ghost_count(), 0, 8));
                shader->uniform1f(LLShaderMgr::LENS_FLARE_GHOST_SPACING, llclamp(lens_flare_ghost_spacing(), 0.1f, 1.f));
                shader->uniform1f(LLShaderMgr::LENS_FLARE_HALO, llclamp(lens_flare_halo(), 0.f, 5.f));
                shader->uniform1f(LLShaderMgr::LENS_FLARE_HALO_RADIUS, llclamp(lens_flare_halo_radius(), 0.01f, 1.f));
                shader->uniform1f(LLShaderMgr::LENS_FLARE_HALO_WIDTH, llclamp(lens_flare_halo_width(), 0.01f, 0.5f));
                shader->uniform1f(LLShaderMgr::LENS_FLARE_STARBURST, llclamp(lens_flare_starburst(), 0.f, 5.f));
                shader->uniform1i(LLShaderMgr::LENS_FLARE_STARBURST_SPIKES, llclamp(lens_flare_starburst_spikes(), 1, 32));
                shader->uniform1f(LLShaderMgr::LENS_FLARE_STARBURST_SHARPNESS, llclamp(lens_flare_starburst_sharpness(), 1.f, 256.f));
                // User-facing "length" (0 = short, 1 = long spikes) maps inversely to the shader's
                // exponential radial falloff rate. Use a reciprocal so the perceived spike extent
                // is roughly linear in the control; the +0.05 keeps falloff finite at length=0.
                F32 starburst_length = llclamp(lens_flare_starburst_length(), 0.f, 1.f);
                F32 starburst_falloff = 4.f / (starburst_length + 0.05f);
                shader->uniform1f(LLShaderMgr::LENS_FLARE_STARBURST_FALLOFF, starburst_falloff);
                shader->uniform1f(LLShaderMgr::LENS_FLARE_OCCLUSION_RADIUS, llclamp(lens_flare_occlusion_radius(), 0.005f, 0.1f));
                shader->uniform1i(LLShaderMgr::LENS_FLARE_OCCLUSION_TAPS, llclamp(lens_flare_occlusion_taps(), 1, 32));

                LLColor4 light_color = linearColor3(sun_up ? mSunDiffuse : mMoonDiffuse);
                shader->uniform3f(LLShaderMgr::LENS_FLARE_LIGHT_COLOR, light_color.mV[0], light_color.mV[1], light_color.mV[2]);
            }
            else
            {
                mLensFlareSunVisibility = 0.f;
            }
        }

        if (apply_tonemap)
        {
            // Exposure parameters
            static LLCachedControl<F32> exposure(gSavedSettings, "RenderExposure", 1.f);
            shader->uniform1f(LLShaderMgr::EXPOSURE, llclamp(exposure(), 0.5f, 4.f));

            // Tonemap type and parameters
            static LLCachedControl<S32> tonemap_type_setting(gSavedSettings, "AlchemyRenderTonemapType", 0U);
            shader->uniform1i(LLShaderMgr::TONEMAP_TYPE, tonemap_type_setting);
            shader->uniform1f(LLShaderMgr::TONEMAP_MIX, psky->getTonemapMix(should_auto_adjust()));

            constexpr F32 max_screen_brightness = 1.f;
            switch (tonemap_type_setting)
            {
                case 2: // ACES Godot
                {
                    static LLCachedControl<F32> tonemap_aces_white(gSavedSettings, "RenderTonemapACESWhite", 6.f);
                    F32                         white = llmax(1.0f, tonemap_aces_white());

                    // These constants must match those in the shader code.
                    const float exposure_bias = 1.8f;
                    const float A             = 0.0245786f;
                    const float B             = 0.000090537f;
                    const float C             = 0.983729f;
                    const float D             = 0.432951f;
                    const float E             = 0.238081f;

                    white *= exposure_bias;
                    float white_tonemapped = (white * (white + A) - B) / (white * (C * white + D) + E);
                    shader->uniform4f(LLShaderMgr::TONEMAP_PARAMS, white_tonemapped, 0.f, 0.f, 0.f);
                    break;
                }
                case 3: // Reinhard
                {
                    // The Reinhard tonemapper is not designed to have a white parameter
                    // that is less than the output max value. This is especially important
                    // in the variable Extended Dynamic Range (EDR) paradigm where the
                    // output max value may change to be greater or less than the white
                    // parameter, depending on the available dynamic range.
                    static LLCachedControl<F32> tonemap_reinhard_white(gSavedSettings, "RenderTonemapReinhardWhite", 6.f);

                    F32 white         = llmax(max_screen_brightness, tonemap_reinhard_white());
                    F32 white_squared = (white * white) / max_screen_brightness;

                    shader->uniform4f(LLShaderMgr::TONEMAP_PARAMS, white_squared, 0.f, 0.f, 0.f);
                    break;
                }
                case 4: // Filmic
                {
                    static LLCachedControl<F32> tonemap_filmic_white(gSavedSettings, "RenderTonemapFilmicWhite", 6.f);
                    F32                         white = llmax(1.0f, tonemap_filmic_white());

                    // These constants must match those in the shader code.
                    const float exposure_bias = 2.0f;
                    const float A             = 0.22f * exposure_bias * exposure_bias; // bias baked into constants for performance
                    const float B             = 0.30f * exposure_bias;
                    const float C             = 0.10f;
                    const float D             = 0.20f;
                    const float E             = 0.01f;
                    const float F             = 0.30f;

                    F32 white_tonemapped = ((white * (A * white + C * B) + D * E) / (white * (A * white + B) + D * F)) - E / F;

                    shader->uniform4f(LLShaderMgr::TONEMAP_PARAMS, white_tonemapped, 0.f, 0.f, 0.f);
                    break;
                }
                case 6: // AgX
                {
                    static LLCachedControl<F32> tonemap_agx_contrast(gSavedSettings, "RenderTonemapAgxContrast", 1.25f);
                    static LLCachedControl<F32> tonemap_agx_white(gSavedSettings, "RenderTonemapAgxWhite", 16.29f);

                    float agx_white = llmax(2.f, tonemap_agx_white());

                    // Calculate allenwp tonemapping curve parameters on the CPU to improve shader performance.
                    // Source and details: https://allenwp.com/blog/2025/05/29/allenwp-tonemapping-curve/

                    // These constants must match the those in the shader code.
                    // 18% "middle gray" is perceptually 50% of the brightness of reference white.
                    const float awp_crossover_point = 0.18f;
                    // When output_max_value and/or awp_crossover_point are no longer constant, awp_shoulder_max can
                    // be calculated on the CPU and passed in as tonemap_parameters.tonemap_e.
                    const float awp_shoulder_max = max_screen_brightness - awp_crossover_point;

                    float awp_high_clip = agx_white;

                    // awp_toe_a is a solution generated by Mathematica that ensures intersection at awp_crossover_point.
                    float awp_toe_a = ((1.0f / awp_crossover_point) - 1.0f) * pow(awp_crossover_point, tonemap_agx_contrast);
                    // Slope formula is simply the derivative of the toe function with an input of awp_crossover_point.
                    float awp_slope_denom = pow(awp_crossover_point, tonemap_agx_contrast) + awp_toe_a;
                    float awp_slope       = (tonemap_agx_contrast * pow(awp_crossover_point, tonemap_agx_contrast - 1.0f) * awp_toe_a) /
                                      (awp_slope_denom * awp_slope_denom);

                    float awp_w = awp_high_clip - awp_crossover_point;
                    awp_w       = awp_w * awp_w;
                    awp_w       = awp_w / awp_shoulder_max;
                    awp_w       = awp_w * awp_slope;

                    shader->uniform4f(LLShaderMgr::TONEMAP_PARAMS, tonemap_agx_contrast, awp_toe_a, awp_slope, awp_w);
                    break;
                }
                case 7: // AMD FidelityFX LPM (Luma Preserving Mapper)
                {
                    static LLCachedControl<F32> lpm_hdr_max(gSavedSettings, "AlchemyToneMapAMDHDRMax", 256.f);
                    static LLCachedControl<F32> lpm_exposure(gSavedSettings, "AlchemyToneMapAMDExposure", 7.4f);
                    static LLCachedControl<F32> lpm_contrast(gSavedSettings, "AlchemyToneMapAMDContrast", 0.05f);
                    static LLCachedControl<F32> lpm_sat_r(gSavedSettings, "AlchemyToneMapAMDSaturationR", 0.f);
                    static LLCachedControl<F32> lpm_sat_g(gSavedSettings, "AlchemyToneMapAMDSaturationG", 0.f);
                    static LLCachedControl<F32> lpm_sat_b(gSavedSettings, "AlchemyToneMapAMDSaturationB", 0.f);

                    // The shipping UI exposes no shoulder control: keep the fast
                    // path (shoulderContrast == 1.0, shoulder == false). Crosstalk
                    // uses AMD's suggested Rec.709 default {1.0, 0.5, 1/32}.
                    const bool  lpm_shoulder          = false;
                    const F32   lpm_shoulder_contrast = 1.0f;
                    const F32   lpm_crosstalk_r        = 1.0f;
                    const F32   lpm_crosstalk_g        = 0.5f;
                    const F32   lpm_crosstalk_b        = 1.0f / 32.0f;

                    // Cache the control block and recompute only when inputs change.
                    static U32  s_lpm_ctl[ALLPM::CONTROL_BLOCK_WORDS] = {};
                    static F32  s_lpm_inputs[6] = {};
                    static bool s_lpm_valid = false;
                    const F32 cur_inputs[6] = { lpm_hdr_max(), lpm_exposure(), lpm_contrast(),
                                                lpm_sat_r(), lpm_sat_g(), lpm_sat_b() };
                    if (!s_lpm_valid || memcmp(cur_inputs, s_lpm_inputs, sizeof(cur_inputs)) != 0)
                    {
                        memcpy(s_lpm_inputs, cur_inputs, sizeof(cur_inputs));
                        s_lpm_valid = true;
                        ALLPM::setup709(lpm_shoulder,
                                        cur_inputs[0], cur_inputs[1], cur_inputs[2],
                                        lpm_shoulder_contrast,
                                        cur_inputs[3], cur_inputs[4], cur_inputs[5],
                                        lpm_crosstalk_r, lpm_crosstalk_g, lpm_crosstalk_b,
                                        s_lpm_ctl);
                    }

                    shader->uniform4uiv(LLShaderMgr::TONEMAP_AMD, 24, s_lpm_ctl);
                    shader->uniform1i(LLShaderMgr::TONEMAP_AMD_SHOULDER, lpm_shoulder ? 1 : 0);
                    break;
                }
                default:
                    break;
            }
        }

        // Color correction LUT
        S32 cglut_channel = -1;
        if (color_grade)
        {
            if (mCGLut != 0)
            {
                cglut_channel = shader->getTextureChannel(LLShaderMgr::COLOR_GRADE_LUT);
                if (cglut_channel > -1)
                {
                    gGL.getTexUnit(cglut_channel)->bindManual(LLTexUnit::TT_TEXTURE_3D, mCGLut);
                    gGL.getTexUnit(cglut_channel)->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
                    gGL.getTexUnit(cglut_channel)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
                }

                shader->uniform4fv(LLShaderMgr::COLOR_GRADE_LUT_SIZE, 1, mCGLutSize.mV);

                static LLCachedControl<F32> cglut_strength(gSavedSettings, "RenderColorGradeLUTStrength", 1.f);
                shader->uniform1f(LLShaderMgr::COLOR_GRADE_LUT_STRENGTH, cglut_strength);
            }
            else
            {
                shader->uniform1f(LLShaderMgr::COLOR_GRADE_LUT_STRENGTH, 0.f); // Disable lut path
            }

            // --- Linear-space grading (pre-tonemap) ---
            // White balance: resolve (CCT offset, Duv) into a linear-sRGB gain
            // on the CPU. Duv is exposed to artists on a friendly [-1, 1]
            // scale and scaled into CIE 1960 uv units (±0.02) here.
            static LLCachedControl<F32>       cg_wb_cct(gSavedSettings, "RenderColorGradeWhiteBalanceCCT", 0.f);
            static LLCachedControl<F32>       cg_wb_duv(gSavedSettings, "RenderColorGradeWhiteBalanceDuv", 0.f);
            const F32 wb_cct = llclamp((F32)cg_wb_cct(), -5000.0f, 5000.0f);
            const F32 wb_duv = llclamp((F32)cg_wb_duv(), -1.0f, 1.0f) * 0.02f;
            const LLVector3 wb_gain = cg_compute_white_balance_gain(wb_cct, wb_duv);
            shader->uniform3fv(LLShaderMgr::COLOR_GRADE_WHITE_BALANCE_GAIN, 1, wb_gain.mV);

            // Lift / Gamma / Gain — gamma is inverted on the CPU.
            static LLCachedControl<LLVector3> cg_lift(gSavedSettings, "RenderColorGradeLift", LLVector3(0.f, 0.f, 0.f));
            static LLCachedControl<LLVector3> cg_gamma_cc(gSavedSettings, "RenderColorGradeGamma", LLVector3(1.f, 1.f, 1.f));
            static LLCachedControl<LLVector3> cg_gain(gSavedSettings, "RenderColorGradeGain", LLVector3(1.f, 1.f, 1.f));
            const LLVector3 lift_v  = cg_lift();
            const LLVector3 gamma_v = cg_gamma_cc();
            const LLVector3 gain_v  = cg_gain();
            const F32 lift_arr[3] = {
                llclamp(lift_v.mV[0], -0.5f, 0.5f),
                llclamp(lift_v.mV[1], -0.5f, 0.5f),
                llclamp(lift_v.mV[2], -0.5f, 0.5f) };
            const F32 gain_arr[3] = {
                llclamp(gain_v.mV[0], 0.5f, 1.5f),
                llclamp(gain_v.mV[1], 0.5f, 1.5f),
                llclamp(gain_v.mV[2], 0.5f, 1.5f) };
            const F32 inv_gamma_arr[3] = {
                1.0f / llclamp(gamma_v.mV[0], 0.5f, 1.5f),
                1.0f / llclamp(gamma_v.mV[1], 0.5f, 1.5f),
                1.0f / llclamp(gamma_v.mV[2], 0.5f, 1.5f) };
            shader->uniform3fv(LLShaderMgr::COLOR_GRADE_LIFT,         1, lift_arr);
            shader->uniform3fv(LLShaderMgr::COLOR_GRADE_INV_GAMMA_CC, 1, inv_gamma_arr);
            shader->uniform3fv(LLShaderMgr::COLOR_GRADE_GAIN,         1, gain_arr);

            // --- Split toning ---
            // Tints → ratios: `tint / max(dot(tint, LUMA), 1e-4)` precomputed.
            // Identity (tint == vec3(0.5)) maps to ratio == vec3(1).
            static LLCachedControl<LLColor3> split_shadow_tint(gSavedSettings, "RenderSplitToneShadowTint", LLColor3(0.5f, 0.5f, 0.5f));
            static LLCachedControl<LLColor3> split_highlight_tint(gSavedSettings, "RenderSplitToneHighlightTint", LLColor3(0.5f, 0.5f, 0.5f));
            static LLCachedControl<LLColor3> split_midtone_tint(gSavedSettings, "RenderSplitToneMidtoneTint", LLColor3(0.5f, 0.5f, 0.5f));
            static LLCachedControl<F32>      split_midtone_amount(gSavedSettings, "RenderSplitToneMidtoneAmount", 0.f);
            static LLCachedControl<F32>      split_balance(gSavedSettings, "RenderSplitToneBalance", 0.f);
            static LLCachedControl<F32>      split_amount(gSavedSettings, "RenderSplitToneAmount", 0.f);

            constexpr F32 CG_LUMA_R = 0.2126f, CG_LUMA_G = 0.7152f, CG_LUMA_B = 0.0722f;
            auto tint_to_ratio = [](const LLColor3& tint, F32 out[3]) {
                F32 l = llmax(tint.mV[0] * CG_LUMA_R + tint.mV[1] * CG_LUMA_G + tint.mV[2] * CG_LUMA_B, 1e-4f);
                F32 inv = 1.0f / l;
                out[0] = tint.mV[0] * inv;
                out[1] = tint.mV[1] * inv;
                out[2] = tint.mV[2] * inv;
            };
            F32 shadow_ratio[3], highlight_ratio[3], midtone_ratio[3];
            tint_to_ratio(split_shadow_tint(),    shadow_ratio);
            tint_to_ratio(split_highlight_tint(), highlight_ratio);
            tint_to_ratio(split_midtone_tint(),   midtone_ratio);
            const F32 tone_balance = llclamp(split_balance(), -1.0f, 1.0f);
            shader->uniform3fv(LLShaderMgr::SPLIT_TONE_SHADOW_RATIO,    1, shadow_ratio);
            shader->uniform3fv(LLShaderMgr::SPLIT_TONE_HIGHLIGHT_RATIO, 1, highlight_ratio);
            shader->uniform3fv(LLShaderMgr::SPLIT_TONE_MIDTONE_RATIO,   1, midtone_ratio);
            shader->uniform1f(LLShaderMgr::SPLIT_TONE_MIDTONE_AMOUNT, llclamp(split_midtone_amount(), 0.0f, 1.0f));
            shader->uniform1f(LLShaderMgr::SPLIT_TONE_MID,            0.5f + tone_balance * 0.4f);
            shader->uniform1f(LLShaderMgr::SPLIT_TONE_AMOUNT,         llclamp(split_amount(), 0.0f, 1.0f));

            // --- Display-space grading ---
            // Every slider is folded into a {scale, bias} pair on the CPU
            // so the shader is one FMA per helper. Identity defaults land
            // exactly on the scale=1/bias=0 fast-path.
            static LLCachedControl<F32> cg_black_point(gSavedSettings, "RenderColorGradeBlackPoint", 0.f);
            static LLCachedControl<F32> cg_white_point(gSavedSettings, "RenderColorGradeWhitePoint", 1.f);
            static LLCachedControl<F32> cg_brightness(gSavedSettings, "RenderColorGradeBrightness", 0.f);
            static LLCachedControl<F32> cg_contrast(gSavedSettings, "RenderColorGradeContrast", 1.f);
            static LLCachedControl<F32> cg_highlights(gSavedSettings, "RenderColorGradeHighlights", 0.f);
            static LLCachedControl<F32> cg_shadows(gSavedSettings, "RenderColorGradeShadows", 0.f);
            static LLCachedControl<F32> cg_saturation(gSavedSettings, "RenderColorGradeSaturation", 1.f);
            static LLCachedControl<F32> cg_vibrance(gSavedSettings, "RenderColorGradeVibrance", 0.f);
            static LLCachedControl<F32> cg_hue_shift(gSavedSettings, "RenderColorGradeHueShift", 0.f);

            const F32 black_point = llclamp(cg_black_point(), 0.0f, 0.5f);
            const F32 white_point = llclamp(cg_white_point(), 0.5f, 1.0f);
            const F32 bwp_scale   = 1.0f / llmax(white_point - black_point, 1e-4f);
            const F32 bwp_bias    = -black_point * bwp_scale;
            const F32 brightness  = llclamp(cg_brightness(), -0.5f, 0.5f);
            const F32 contrast    = llclamp(cg_contrast(),    0.0f, 2.0f);
            const F32 bc_scale    = contrast;
            const F32 bc_bias     = (brightness - 0.5f) * contrast + 0.5f;
            shader->uniform1f(LLShaderMgr::COLOR_GRADE_BWP_SCALE,         bwp_scale);
            shader->uniform1f(LLShaderMgr::COLOR_GRADE_BWP_BIAS,          bwp_bias);
            shader->uniform1f(LLShaderMgr::COLOR_GRADE_BC_SCALE,          bc_scale);
            shader->uniform1f(LLShaderMgr::COLOR_GRADE_BC_BIAS,           bc_bias);
            shader->uniform1f(LLShaderMgr::COLOR_GRADE_HIGHLIGHTS_SCALED, llclamp(cg_highlights(), -1.0f, 1.0f) * 0.3f);
            shader->uniform1f(LLShaderMgr::COLOR_GRADE_SHADOWS_SCALED,    llclamp(cg_shadows(),    -1.0f, 1.0f) * 0.3f);
            shader->uniform1f(LLShaderMgr::COLOR_GRADE_SATURATION,        llclamp(cg_saturation(),  0.0f, 2.0f));
            shader->uniform1f(LLShaderMgr::COLOR_GRADE_VIBRANCE,          llclamp(cg_vibrance(),   -1.0f, 1.0f));
            shader->uniform1f(LLShaderMgr::COLOR_GRADE_HUE_SHIFT_NORM,    llclamp(cg_hue_shift(), -180.0f, 180.0f) / 360.0f);

            // Per-channel filmic curves. Per-channel `(shoulder - toe)` is
            // pre-inverted on the CPU so the shader avoids three divisions.
            static LLCachedControl<LLColor3> cg_curve_toe(gSavedSettings, "RenderColorGradeCurveToe", LLColor3(0.f, 0.f, 0.f));
            static LLCachedControl<LLColor3> cg_curve_shoulder(gSavedSettings, "RenderColorGradeCurveShoulder", LLColor3(1.f, 1.f, 1.f));
            static LLCachedControl<LLColor3> cg_curve_strength(gSavedSettings, "RenderColorGradeCurveStrength", LLColor3(0.f, 0.f, 0.f));
            const LLColor3 curve_toe      = cg_curve_toe();
            const LLColor3 curve_shoulder = cg_curve_shoulder();
            const LLColor3 curve_strength = cg_curve_strength();
            const F32 curve_inv_range[3] = {
                1.0f / llmax(curve_shoulder.mV[0] - curve_toe.mV[0], 1e-4f),
                1.0f / llmax(curve_shoulder.mV[1] - curve_toe.mV[1], 1e-4f),
                1.0f / llmax(curve_shoulder.mV[2] - curve_toe.mV[2], 1e-4f) };
            const F32 curve_strength_arr[3] = {
                llclamp(curve_strength.mV[0], 0.0f, 1.0f),
                llclamp(curve_strength.mV[1], 0.0f, 1.0f),
                llclamp(curve_strength.mV[2], 0.0f, 1.0f) };
            shader->uniform3fv(LLShaderMgr::COLOR_GRADE_CURVE_TOE,       1, curve_toe.mV);
            shader->uniform3fv(LLShaderMgr::COLOR_GRADE_CURVE_INV_RANGE, 1, curve_inv_range);
            shader->uniform3fv(LLShaderMgr::COLOR_GRADE_CURVE_STRENGTH,  1, curve_strength_arr);
        }

        mScreenTriangleVB->setBuffer();
        mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

        if (cglut_channel > -1)
        {
            gGL.getTexUnit(cglut_channel)->unbind(LLTexUnit::TT_TEXTURE_3D);
        }
        if (exposure_channel > -1)
        {
            gGL.getTexUnit(exposure_channel)->unbind(LLTexUnit::TT_TEXTURE);
        }
        if (bloom_channel > -1)
        {
            gGL.getTexUnit(bloom_channel)->unbind(LLTexUnit::TT_TEXTURE);
        }
        if (depth_channel > -1)
        {
            gGL.getTexUnit(depth_channel)->unbind(LLTexUnit::TT_TEXTURE);
        }
        gGL.getTexUnit(diffuse_channel)->unbind(src->getUsage());
        shader->unbind();
    }
    dst->flush();
}

void LLPipeline::copyScreenSpaceReflections(LLRenderTarget* src, LLRenderTarget* dst)
{

    if (RenderScreenSpaceReflections && !gCubeSnapshot)
    {
        LL_PROFILE_GPU_ZONE("ssr copy");
        LLGLDepthTest depth(GL_TRUE, GL_TRUE, GL_ALWAYS);
        dst->copyContents(*src, 0, 0, src->getWidth(), src->getHeight(), 0, 0, dst->getWidth(), dst->getHeight(),
                         GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    }
}

void LLPipeline::generateGlow(LLRenderTarget* src)
{
    LL_PROFILE_GPU_ZONE("glow generate");
    if (sRenderGlow)
    {
        mGlow[2].bindTarget();
        mGlow[2].clear();

        gGlowExtractProgram.bind();
        F32 maxAlpha = RenderGlowMaxExtractAlpha;
        F32 warmthAmount = RenderGlowWarmthAmount;
        LLVector3 lumWeights = RenderGlowLumWeights;
        LLVector3 warmthWeights = RenderGlowWarmthWeights;

        gGlowExtractProgram.uniform1f(LLShaderMgr::GLOW_MIN_LUMINANCE, 9999);
        gGlowExtractProgram.uniform1f(LLShaderMgr::GLOW_MAX_EXTRACT_ALPHA, maxAlpha);
        gGlowExtractProgram.uniform3f(LLShaderMgr::GLOW_LUM_WEIGHTS, lumWeights.mV[0], lumWeights.mV[1],
            lumWeights.mV[2]);
        gGlowExtractProgram.uniform3f(LLShaderMgr::GLOW_WARMTH_WEIGHTS, warmthWeights.mV[0], warmthWeights.mV[1],
            warmthWeights.mV[2]);
        gGlowExtractProgram.uniform1f(LLShaderMgr::GLOW_WARMTH_AMOUNT, warmthAmount);

        if (RenderGlowNoise)
        {
            S32 channel = gGlowExtractProgram.enableTexture(LLShaderMgr::GLOW_NOISE_MAP);
            if (channel > -1)
            {
                gGL.getTexUnit(channel)->bindManual(LLTexUnit::TT_TEXTURE, mTrueNoiseMap);
                gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
            }
            gGlowExtractProgram.uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES,
                                          (GLfloat)mGlow[2].getWidth(),
                                          (GLfloat)mGlow[2].getHeight());
        }

        {
            LLGLEnable blend_on(GL_BLEND);

            gGL.setSceneBlendType(LLRender::BT_ADD_WITH_ALPHA);

            gGlowExtractProgram.bindTexture(LLShaderMgr::DIFFUSE_MAP, src);

            gGL.color4f(1, 1, 1, 1);
            gPipeline.enableLightsFullbright();

            mScreenTriangleVB->setBuffer();
            mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

            mGlow[2].flush();
        }

        gGlowExtractProgram.unbind();

        // power of two between 1 and 1024
        U32 glowResPow = RenderGlowResolutionPow;
        const U32 glow_res = llmax(1, llmin(1024, 1 << glowResPow));

        S32 kernel = RenderGlowIterations * 2;
        F32 delta = RenderGlowWidth / glow_res;
        // Use half the glow width if we have the res set to less than 9 so that it looks
        // almost the same in either case.
        if (glowResPow < 9)
        {
            delta *= 0.5f;
        }
        F32 strength = RenderGlowStrength;

        gGlowProgram.bind();
        gGlowProgram.uniform1f(LLShaderMgr::GLOW_STRENGTH, strength);

        for (S32 i = 0; i < kernel; i++)
        {
            mGlow[i % 2].bindTarget();
            mGlow[i % 2].clear();

            if (i == 0)
            {
                gGlowProgram.bindTexture(LLShaderMgr::DIFFUSE_MAP, &mGlow[2]);
            }
            else
            {
                gGlowProgram.bindTexture(LLShaderMgr::DIFFUSE_MAP, &mGlow[(i - 1) % 2]);
            }

            if (i % 2 == 0)
            {
                gGlowProgram.uniform2f(LLShaderMgr::GLOW_DELTA, delta, 0);
            }
            else
            {
                gGlowProgram.uniform2f(LLShaderMgr::GLOW_DELTA, 0, delta);
            }

            mScreenTriangleVB->setBuffer();
            mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

            mGlow[i % 2].flush();
        }

        gGlowProgram.unbind();

    }
    else // !sRenderGlow, skip the glow ping-pong and just clear the result target
    {
        mGlow[1].bindTarget();
        mGlow[1].clear();
        mGlow[1].flush();
    }
}

// HDR bloom pyramid: threshold-extract into mBloomMip[0], downsample down the
// pyramid with a Karis-averaged 13-tap filter, then upsample back with an
// additive 3x3 tent filter. Halation is carried alongside in the alpha channel.
void LLPipeline::generateBloomHDR(LLRenderTarget* src)
{
    LL_PROFILE_GPU_ZONE("bloom hdr generate");

    if (mRT->bloomMipCount < 3 ||
        !gBloomExtractProgram.isComplete() ||
        !gBloomDownsampleProgram.isComplete() ||
        !gBloomDownsampleFirstProgram.isComplete() ||
        !gBloomUpsampleProgram.isComplete())
    {
        return;
    }

    static LLCachedControl<F32> bloom_threshold(gSavedSettings, "RenderBloomThreshold", 1.0f);
    static LLCachedControl<F32> bloom_knee(gSavedSettings, "RenderBloomKnee", 0.5f);
    static LLCachedControl<F32> bloom_scatter(gSavedSettings, "RenderBloomScatter", 0.7f);
    static LLCachedControl<F32> alpha_glow_boost(gSavedSettings, "RenderBloomAlphaGlowBoost", 2.0f);

    static LLCachedControl<bool> should_auto_adjust(gSavedSettings, "RenderSkyAutoAdjustLegacy", false);
    static LLCachedControl<bool> buildNoPost(gSavedSettings, "RenderDisablePostProcessing", false);
    LLSettingsSky::ptr_t psky = LLEnvironment::instance().getCurrentSky();
    bool legacy_gamma = psky->getReflectionProbeAmbiance(should_auto_adjust) == 0.f;
    bool no_post = gSnapshotNoPost || legacy_gamma || (buildNoPost && gFloaterTools && gFloaterTools->isAvailable());

    LLGLDepthTest depth(GL_FALSE);
    LLGLDisable cull(GL_CULL_FACE);

    // Extract pass: write thresholded bloom + halation into mip 0.
    {
        LLGLDisable blend(GL_BLEND);
        mRT->bloomMip[0].bindTarget();
        mRT->bloomMip[0].clear();

        gBloomExtractProgram.bind();
        gBloomExtractProgram.bindTexture(LLShaderMgr::DIFFUSE_MAP, src);
        gBloomExtractProgram.uniform1f(LLShaderMgr::BLOOM_THRESHOLD, no_post ? 99999.f :bloom_threshold());
        gBloomExtractProgram.uniform1f(LLShaderMgr::BLOOM_KNEE, llmax(bloom_knee(), 0.0f));
        gBloomExtractProgram.uniform1f(LLShaderMgr::BLOOM_ALPHA_GLOW_BOOST, llmax(alpha_glow_boost(), 0.0f));

        // Reuse the warmth weights from legacy glow so the halation red-bias
        // matches artist expectations set by the old RenderGlowWarmthWeights knob.
        LLVector3 warmth = RenderGlowWarmthWeights;
        gBloomExtractProgram.uniform3f(LLShaderMgr::HALATION_LUM_WEIGHTS, warmth.mV[0], warmth.mV[1], warmth.mV[2]);

        mScreenTriangleVB->setBuffer();
        mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

        mRT->bloomMip[0].flush();
        gBloomExtractProgram.unbind();
    }

    // Downsample chain: mip[i-1] -> mip[i]. The first downsample uses the
    // partial-Karis variant (1 / (1 + lum) per 2x2 group) to suppress fireflies
    // from full-res specular highlights before they bleed across the pyramid.
    // Subsequent levels run the plain 13-tap since they operate on already-
    // averaged data where firefly energy has been amortized out.
    {
        LLGLDisable blend(GL_BLEND);
        for (U32 i = 1; i < mRT->bloomMipCount; ++i)
        {
            LLGLSLShader* shader = (i == 1) ? &gBloomDownsampleFirstProgram
                                            : &gBloomDownsampleProgram;
            LLRenderTarget* srcMip = &mRT->bloomMip[i - 1];

            mRT->bloomMip[i].bindTarget();
            mRT->bloomMip[i].clear();

            shader->bind();
            shader->bindTexture(LLShaderMgr::DIFFUSE_MAP, srcMip, false, LLTexUnit::TFO_BILINEAR);
            shader->uniform2f(LLShaderMgr::BLOOM_TEXEL_SIZE,
                              1.0f / (F32)srcMip->getWidth(),
                              1.0f / (F32)srcMip->getHeight());

            mScreenTriangleVB->setBuffer();
            mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

            mRT->bloomMip[i].flush();
            shader->unbind();
        }
    }

    // Upsample chain: mip[i] -> mip[i-1] with additive blend. Walks from the
    // smallest mip back up to mip 0, leaving the final bloom in mBloomMip[0].
    {
        LLGLEnable blend(GL_BLEND);
        gGL.setSceneBlendType(LLRender::BT_ADD);

        gBloomUpsampleProgram.bind();
        gBloomUpsampleProgram.uniform1f(LLShaderMgr::BLOOM_SCATTER, llmax(bloom_scatter(), 0.0f));

        for (S32 i = (S32)mRT->bloomMipCount - 1; i > 0; --i)
        {
            LLRenderTarget* srcMip = &mRT->bloomMip[i];
            LLRenderTarget* dstMip = &mRT->bloomMip[i - 1];

            dstMip->bindTarget();

            gBloomUpsampleProgram.bindTexture(LLShaderMgr::DIFFUSE_MAP, srcMip, false, LLTexUnit::TFO_BILINEAR);
            gBloomUpsampleProgram.uniform2f(LLShaderMgr::BLOOM_TEXEL_SIZE,
                                            1.0f / (F32)srcMip->getWidth(),
                                            1.0f / (F32)srcMip->getHeight());

            mScreenTriangleVB->setBuffer();
            mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

            dstMip->flush();
        }

        gBloomUpsampleProgram.unbind();
        gGL.setSceneBlendType(LLRender::BT_ALPHA);
    }
}

// Composite the bloom pyramid (mBloomMip[0]) additively into the pre-tonemap
// scene buffer. Halation rides in the alpha channel and is tinted at composite.
// The main render path folds this into colorCorrectF (BLOOM_COMPOSITE); this
// function is retained for standalone use (e.g. offline capture paths).
void LLPipeline::compositeBloomHDR(LLRenderTarget* scene)
{
    LL_PROFILE_GPU_ZONE("bloom hdr composite");

    if (mRT->bloomMipCount < 3 || !gBloomCompositeProgram.isComplete())
    {
        return;
    }

    static LLCachedControl<F32> bloom_strength(gSavedSettings, "RenderBloomStrength", 0.325f);
    static LLCachedControl<F32> halation_strength(gSavedSettings, "RenderBloomHalationStrength", 0.0f);
    static LLCachedControl<LLColor3> halation_tint(gSavedSettings, "RenderBloomHalationTint", LLColor3(1.0f, 0.35f, 0.15f));

    LLGLDepthTest depth(GL_FALSE);
    LLGLDisable cull(GL_CULL_FACE);
    LLGLEnable blend(GL_BLEND);
    gGL.setSceneBlendType(LLRender::BT_ADD);

    scene->bindTarget();

    gBloomCompositeProgram.bind();
    gBloomCompositeProgram.bindTexture(LLShaderMgr::BLOOM_SAMPLER, &mRT->bloomMip[0], false, LLTexUnit::TFO_BILINEAR);
    gBloomCompositeProgram.uniform1f(LLShaderMgr::BLOOM_STRENGTH, llmax(bloom_strength(), 0.0f));
    gBloomCompositeProgram.uniform1f(LLShaderMgr::HALATION_STRENGTH, llmax(halation_strength(), 0.0f));
    const LLColor3& tint = halation_tint();
    gBloomCompositeProgram.uniform3f(LLShaderMgr::HALATION_TINT, tint.mV[0], tint.mV[1], tint.mV[2]);

    mScreenTriangleVB->setBuffer();
    mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

    gBloomCompositeProgram.unbind();

    scene->flush();

    gGL.setSceneBlendType(LLRender::BT_ALPHA);
}

void LLPipeline::applyCAS(LLRenderTarget* src, LLRenderTarget* dst)
{
    static LLCachedControl<F32> cas_sharpness(gSavedSettings, "RenderCASSharpness", 0.4f);
    LL_PROFILE_GPU_ZONE("cas");
    if (cas_sharpness == 0.0f || !gCASProgram.isComplete())
    {
        gPipeline.copyRenderTarget(src, dst);
        return;
    }

    // Bind setup:
    dst->bindTarget();

    gCASProgram.bind();

    {
        static LLStaticHashedString cas_param_0("cas_param_0");
        static LLStaticHashedString cas_param_1("cas_param_1");
        static LLStaticHashedString out_screen_res("out_screen_res");

        varAU4(const0);
        varAU4(const1);
        CasSetup(const0, const1,
            cas_sharpness(),             // Sharpness tuning knob (0.0 to 1.0).
            (AF1)src->getWidth(), (AF1)src->getHeight(),  // Input size.
            (AF1)dst->getWidth(), (AF1)dst->getHeight()); // Output size.

        gCASProgram.uniform4uiv(cas_param_0, 1, const0);
        gCASProgram.uniform4uiv(cas_param_1, 1, const1);

        gCASProgram.uniform2f(out_screen_res, (AF1)dst->getWidth(), (AF1)dst->getHeight());
    }

    gCASProgram.bindTexture(LLShaderMgr::DEFERRED_DIFFUSE, src, false, LLTexUnit::TFO_POINT);

    // Draw
    gPipeline.mScreenTriangleVB->setBuffer();
    gPipeline.mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

    gCASProgram.unbind();

    dst->flush();
}

void LLPipeline::applyFXAA(LLRenderTarget* src, LLRenderTarget* dst)
{
    LL_PROFILE_GPU_ZONE("FXAA");
    {
        llassert(!gCubeSnapshot);
        bool multisample = RenderFSAAType == 1 && gFXAAProgram[0].isComplete() && mFXAAMap.isComplete();

        // Present everything.
        if (multisample)
        {
            LL_PROFILE_GPU_ZONE("aa");
            S32 width = dst->getWidth();
            S32 height = dst->getHeight();

            // bake out texture2D with RGBL for FXAA shader
            mFXAAMap.bindTarget();
            mFXAAMap.clear(GL_COLOR_BUFFER_BIT);

            LLGLSLShader* shader = &gGlowCombineFXAAProgram;
            shader->bind();

            S32 channel = shader->enableTexture(LLShaderMgr::DEFERRED_DIFFUSE, src->getUsage());
            if (channel > -1)
            {
                src->bindTexture(0, channel, LLTexUnit::TFO_BILINEAR);
            }

            {
                LLGLDepthTest depth_test(GL_TRUE, GL_TRUE, GL_ALWAYS);
                mScreenTriangleVB->setBuffer();
                mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
            }

            shader->disableTexture(LLShaderMgr::DEFERRED_DIFFUSE, src->getUsage());
            shader->unbind();

            mFXAAMap.flush();

            dst->bindTarget();

            static LLCachedControl<U32> aa_quality(gSavedSettings, "RenderFSAASamples", 0U);
            U32 fsaa_quality = std::clamp(aa_quality(), 0U, 3U);

            shader = &gFXAAProgram[fsaa_quality];
            shader->bind();

            channel = shader->enableTexture(LLShaderMgr::DIFFUSE_MAP, mFXAAMap.getUsage());
            if (channel > -1)
            {
                mFXAAMap.bindTexture(0, channel, LLTexUnit::TFO_BILINEAR);
            }

            // The destination is an FBO; bindTarget already set its viewport.
            // Forcing the full world-view viewport here makes RenderResolutionDivisor
            // write only the lower-left fraction of the reduced-size target.

            F32 scale_x = (F32)width / mFXAAMap.getWidth();
            F32 scale_y = (F32)height / mFXAAMap.getHeight();
            shader->uniform2f(LLShaderMgr::FXAA_TC_SCALE, scale_x, scale_y);
            shader->uniform2f(LLShaderMgr::FXAA_RCP_SCREEN_RES, 1.f / width * scale_x, 1.f / height * scale_y);
            shader->uniform4f(LLShaderMgr::FXAA_RCP_FRAME_OPT, -0.5f / width * scale_x, -0.5f / height * scale_y,
                0.5f / width * scale_x, 0.5f / height * scale_y);
            shader->uniform4f(LLShaderMgr::FXAA_RCP_FRAME_OPT2, -2.f / width * scale_x, -2.f / height * scale_y,
                2.f / width * scale_x, 2.f / height * scale_y);

            {
                LLGLDepthTest depth_test(GL_TRUE, GL_TRUE, GL_ALWAYS);
                S32 depth_channel = shader->getTextureChannel(LLShaderMgr::DEFERRED_DEPTH);
                gGL.getTexUnit(depth_channel)->bind(&mRT->deferredScreen, true);

                mScreenTriangleVB->setBuffer();
                mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
            }

            shader->unbind();
            dst->flush();
        }
        else {
            copyRenderTarget(src, dst);
        }
    }
}

void LLPipeline::generateSMAABuffers(LLRenderTarget* src)
{
    llassert(!gCubeSnapshot);
    bool multisample = RenderFSAAType == 2 && gSMAAEdgeDetectProgram[0].isComplete() && mFXAAMap.isComplete() && mSMAABlendBuffer.isComplete();

    // Present everything.
    if (multisample)
    {
        LL_PROFILE_GPU_ZONE("SMAA Edge");
        static LLCachedControl<U32> aa_quality(gSavedSettings, "RenderFSAASamples", 0U);
        U32 fsaa_quality = std::clamp(aa_quality(), 0U, 3U);

        S32 width = src->getWidth();
        S32 height = src->getHeight();

        float rt_metrics[] = { 1.f / width, 1.f / height, (float)width, (float)height };

        LLGLDepthTest depth(GL_FALSE, GL_FALSE);

        static LLCachedControl<bool> use_sample(gSavedSettings, "RenderSMAAUseSample", false);
        static LLCachedControl<bool> use_predication(gSavedSettings, "RenderSMAAPredication", true);
        static LLCachedControl<bool> use_stencil_setting(gSavedSettings, "RenderSMAAUseStencil", true);
        // Stencil optimization requires all three passes to share the same stencil attachment.
        bool use_stencil = use_stencil_setting && mFXAAMap.hasStencil() && mSMAABlendBuffer.hasStencil();
        LLGLState stencil(GL_STENCIL_TEST, use_stencil);
        {
            // Bind setup:
            LLRenderTarget& dest = mFXAAMap;
            LLGLSLShader& edge_shader = gSMAAEdgeDetectProgram[fsaa_quality];

            dest.bindTarget();
            dest.clear(use_stencil ? (GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT) : GL_COLOR_BUFFER_BIT);

            edge_shader.bind();
            edge_shader.uniform4fv(sSmaaRTMetrics, 1, rt_metrics);

            S32 channel = edge_shader.enableTexture(LLShaderMgr::DEFERRED_DIFFUSE, src->getUsage());
            if (channel > -1)
            {
                if (!use_sample)
                {
                    src->bindTexture(0, channel, LLTexUnit::TFO_BILINEAR);
                }
                else
                {
                    gGL.getTexUnit(channel)->bindManual(LLTexUnit::TT_TEXTURE, mSMAASampleMap);
                    gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
                }
                gGL.getTexUnit(channel)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
            }

            S32 pred_channel = -1;
            if (use_predication)
            {
                pred_channel = edge_shader.enableTexture(LLShaderMgr::SMAA_PREDICATION_TEX, mRT->deferredScreen.getUsage());
                if (pred_channel > -1)
                {
                    gGL.getTexUnit(pred_channel)->bind(&mRT->deferredScreen, true);
                    gGL.getTexUnit(pred_channel)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
                    gGL.getTexUnit(pred_channel)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
                }
            }

            if (use_stencil)
            {
                glStencilFunc(GL_ALWAYS, 1, 0xFF);
                glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
                glStencilMask(0xFF);
            }
            mScreenTriangleVB->setBuffer();
            mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

            edge_shader.unbind();
            dest.flush();

            gGL.getTexUnit(channel)->unbindFast(LLTexUnit::TT_TEXTURE);
            if (pred_channel > -1)
            {
                gGL.getTexUnit(pred_channel)->unbindFast(LLTexUnit::TT_TEXTURE);
            }
        }

        {
            // Bind setup:
            LLRenderTarget& dest = mSMAABlendBuffer;
            LLGLSLShader& blend_weights_shader = gSMAABlendWeightsProgram[fsaa_quality];

            dest.bindTarget();
            // Preserve the stencil mask written by the edge-detect pass.
            dest.clear(GL_COLOR_BUFFER_BIT);

            blend_weights_shader.bind();
            blend_weights_shader.uniform4fv(sSmaaRTMetrics, 1, rt_metrics);

            S32 edge_tex_channel = blend_weights_shader.enableTexture(LLShaderMgr::SMAA_EDGE_TEX, mFXAAMap.getUsage());
            if (edge_tex_channel > -1)
            {
                mFXAAMap.bindTexture(0, edge_tex_channel, LLTexUnit::TFO_BILINEAR);
                gGL.getTexUnit(edge_tex_channel)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
            }
            S32 area_tex_channel = blend_weights_shader.enableTexture(LLShaderMgr::SMAA_AREA_TEX, LLTexUnit::TT_TEXTURE);
            if (area_tex_channel > -1)
            {
                gGL.getTexUnit(area_tex_channel)->bindManual(LLTexUnit::TT_TEXTURE, mSMAAAreaMap);
                gGL.getTexUnit(area_tex_channel)->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
                gGL.getTexUnit(area_tex_channel)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
            }
            S32 search_tex_channel = blend_weights_shader.enableTexture(LLShaderMgr::SMAA_SEARCH_TEX, LLTexUnit::TT_TEXTURE);
            if (search_tex_channel > -1)
            {
                gGL.getTexUnit(search_tex_channel)->bindManual(LLTexUnit::TT_TEXTURE, mSMAASearchMap);
                gGL.getTexUnit(search_tex_channel)->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
                gGL.getTexUnit(search_tex_channel)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
            }

            if (use_stencil)
            {
                glStencilFunc(GL_EQUAL, 1, 0xFF);
                glStencilMask(0x00);
            }
            mScreenTriangleVB->setBuffer();
            mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
            if (use_stencil)
            {
                glStencilFunc(GL_ALWAYS, 0, 0xFF);
                glStencilMask(0xFF);
            }
            blend_weights_shader.unbind();
            dest.flush();
            gGL.getTexUnit(edge_tex_channel)->unbindFast(LLTexUnit::TT_TEXTURE);
            gGL.getTexUnit(area_tex_channel)->unbindFast(LLTexUnit::TT_TEXTURE);
            gGL.getTexUnit(search_tex_channel)->unbindFast(LLTexUnit::TT_TEXTURE);
        }
    }
}

void LLPipeline::applySMAA(LLRenderTarget* src, LLRenderTarget* dst)
{
    LL_PROFILE_GPU_ZONE("SMAA");
    llassert(!gCubeSnapshot);
    bool multisample = RenderFSAAType == 2 && gSMAAEdgeDetectProgram[0].isComplete() && mFXAAMap.isComplete() && mSMAABlendBuffer.isComplete();

    // Present everything.
    if (multisample)
    {
        static LLCachedControl<U32> aa_quality(gSavedSettings, "RenderFSAASamples", 0U);
        U32 fsaa_quality = std::clamp(aa_quality(), 0U, 3U);

        S32 width = src->getWidth();
        S32 height = src->getHeight();

        float rt_metrics[] = { 1.f / width, 1.f / height, (float)width, (float)height };

        LLGLDepthTest    depth(GL_FALSE, GL_FALSE);

        static LLCachedControl<bool> use_sample(gSavedSettings, "RenderSMAAUseSample", false);

        {
            // Bind setup:
            LLRenderTarget* bound_target = dst;
            LLGLSLShader& blend_shader = gSMAANeighborhoodBlendProgram[fsaa_quality];

            bound_target->bindTarget();
            bound_target->clear(GL_COLOR_BUFFER_BIT);

            blend_shader.bind();
            blend_shader.uniform4fv(sSmaaRTMetrics, 1, rt_metrics);

            S32 diffuse_channel = blend_shader.enableTexture(LLShaderMgr::DEFERRED_DIFFUSE);
            if(diffuse_channel > -1)
            {
                src->bindTexture(0, diffuse_channel, LLTexUnit::TFO_BILINEAR);
                gGL.getTexUnit(diffuse_channel)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
            }

            S32 blend_channel = blend_shader.enableTexture(LLShaderMgr::SMAA_BLEND_TEX);
            if (blend_channel > -1)
            {
                mSMAABlendBuffer.bindTexture(0, blend_channel, LLTexUnit::TFO_BILINEAR);
            }

            mScreenTriangleVB->setBuffer();
            mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

            bound_target->flush();
            blend_shader.unbind();
            gGL.getTexUnit(diffuse_channel)->unbindFast(LLTexUnit::TT_TEXTURE);
            gGL.getTexUnit(blend_channel)->unbindFast(LLTexUnit::TT_TEXTURE);
        }
    }
    else
    {
        copyRenderTarget(src, dst);
    }
}

void LLPipeline::copyRenderTarget(LLRenderTarget* src, LLRenderTarget* dst)
{

    LL_PROFILE_GPU_ZONE("copyRenderTarget");
    dst->bindTarget();

    gDeferredPostNoDoFProgram.bind();

    gDeferredPostNoDoFProgram.bindTexture(LLShaderMgr::DEFERRED_DIFFUSE, src);
    gDeferredPostNoDoFProgram.bindTexture(LLShaderMgr::DEFERRED_DEPTH, &mRT->deferredScreen, true);

    {
        mScreenTriangleVB->setBuffer();
        mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
    }

    gDeferredPostNoDoFProgram.unbind();

    dst->flush();
}

// [BDMerge G3.2] volumetric lighting / godrays pass. Donor: Black Dragon
// (NiranV Dean; Tofu Buzzard lineage), renderVolumetric at BD pipeline.cpp:8470.
// Fix-forward vs donor: BD renders src->src while sampling the same target
// (read/write feedback, undefined behavior); here it ping-pongs src->dst.
// Requires shadow maps: the raymarch samples the sun cascades, so the pass
// only runs when shadows are on.
void LLPipeline::renderVolumetric(LLRenderTarget* src, LLRenderTarget* dst)
{
    if (RenderVolumetricLighting && RenderShadowDetail > 0 && !gCubeSnapshot &&
        gVolumetricLightProgram.isComplete())
    {
        LL_PROFILE_GPU_ZONE("renderVolumetric");
        dst->bindTarget();
        gGL.setColorMask(true, false);

        bindDeferredShader(gVolumetricLightProgram);
        gVolumetricLightProgram.bindTexture(LLShaderMgr::DEFERRED_DIFFUSE, src, LLTexUnit::TFO_POINT);

        gVolumetricLightProgram.uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES, (GLfloat)src->getWidth(), (GLfloat)src->getHeight());
        gVolumetricLightProgram.uniform1i(LLShaderMgr::GODRAY_RES, RenderVolumetricLightingResolution);
        gVolumetricLightProgram.uniform1f(LLShaderMgr::GODRAY_MULTIPLIER, RenderVolumetricLightingMultiplier);
        gVolumetricLightProgram.uniform1f(LLShaderMgr::FALLOFF_MULTIPLIER, RenderVolumetricLightingFalloffMultiplier);

        mScreenTriangleVB->setBuffer();
        mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

        unbindDeferredShader(gVolumetricLightProgram);
        gGL.setColorMask(true, true);
        dst->flush();
    }
    else if (src != dst)
    { // pass disabled: keep the ping-pong chain coherent
        gGL.getTexUnit(0)->disable();
        copyRenderTarget(src, dst);
    }
}

void LLPipeline::renderWeather(LLRenderTarget* target)
{
    static bool sLightningSheetSnapshotValid = false;
    static U64 sLightningSheetStrikeId = 0;
    static F64 sLightningSheetStrikeTime = 0.0;
    static U64 sLightningSheetSeed = 0;
    static F32 sLightningSheetCoverage = 0.f;
    static F32 sLightningSheetDensity1 = 1.f;
    static F32 sLightningSheetDensity2 = 1.f;
    static F32 sLightningSheetVariance = 0.f;
    static F32 sLightningSheetScale = 0.42f;
    static LLVector2 sLightningSheetBaseOffset(0.f, 0.f);
    static LLVector2 sLightningSheetScrollRate(0.f, 0.f);

    static LLCachedControl<bool> enabled(gSavedSettings, "AlchemyWeatherEnabled", false);
    if (!enabled())
    {
        sWeatherController.reset();
        sWeatherShelterExposure.reset();
        sLightningSheetSnapshotValid = false;
        mWeatherRainOcclusionValid = false;
        mWeatherRainOcclusionFrame = 0;
        mWeatherRainOcclusionFailedResolution = 0;
        mWeatherRainOcclusionDepthRange = 1.f;
        mWeatherRainOcclusionMatrix = glm::mat4(1.f);
        if (mWeatherRainOcclusion.getWidth() != 0)
        {
            mWeatherRainOcclusion.release();
        }
        return;
    }
    if (gCubeSnapshot || !target || !mScreenTriangleVB)
    {
        return;
    }

    LL_PROFILE_GPU_ZONE("weather");

    static LLCachedControl<bool> rain_enabled(gSavedSettings, "AlchemyWeatherRainEnabled", true);
    static LLCachedControl<F32> rain_intensity_setting(gSavedSettings, "AlchemyWeatherRainIntensity", 0.55f);
    static LLCachedControl<F32> rain_density_setting(gSavedSettings, "AlchemyWeatherRainDensity", 0.65f);
    static LLCachedControl<F32> rain_fall_speed_setting(gSavedSettings, "AlchemyWeatherRainFallSpeed", 28.f);
    static LLCachedControl<F32> rain_distance_setting(gSavedSettings, "AlchemyWeatherRainMaxDistance", 80.f);
    static LLCachedControl<U32> rain_samples_setting(gSavedSettings, "AlchemyWeatherRainSamples", 12U);
    static LLCachedControl<U32> rain_divisor_setting(gSavedSettings, "AlchemyWeatherRainResolutionDivisor", 2U);
    static LLCachedControl<F32> wind_scale_setting(gSavedSettings, "AlchemyWeatherWindScale", 1.f);
    static LLCachedControl<LLColor3> rain_color_setting(gSavedSettings, "AlchemyWeatherRainColor", LLColor3(0.55f, 0.68f, 0.78f));
    static LLCachedControl<F32> eep_coupling_setting(gSavedSettings, "AlchemyWeatherEEPCoupling", 0.35f);
    static LLCachedControl<U32> rain_layers_setting(gSavedSettings, "AlchemyWeatherRainLayers", 3U);
    static LLCachedControl<F32> rain_shutter_setting(gSavedSettings, "AlchemyWeatherRainVirtualShutter", 0.04f);
    static LLCachedControl<F32> rain_near_setting(gSavedSettings, "AlchemyWeatherRainNearEmphasis", 0.65f);
    static LLCachedControl<F32> rain_gust_setting(gSavedSettings, "AlchemyWeatherRainGustStrength", 0.35f);
    static LLCachedControl<bool> shelter_fade_setting(gSavedSettings, "AlchemyWeatherShelterFade", true);
    static LLCachedControl<F32> shelter_height_setting(gSavedSettings, "AlchemyWeatherShelterHeight", 48.f);
    static LLCachedControl<bool> rain_occlusion_setting(
        gSavedSettings, "AlchemyWeatherRainOcclusion", false);
    static LLCachedControl<F32> rain_occlusion_bias_setting(
        gSavedSettings, "AlchemyWeatherRainOcclusionBias", 0.12f);
    static LLCachedControl<F32> rain_occlusion_softness_setting(
        gSavedSettings, "AlchemyWeatherRainOcclusionSoftness", 0.35f);

    static LLCachedControl<bool> splash_enabled(gSavedSettings, "AlchemyWeatherSplashEnabled", true);
    static LLCachedControl<F32> splash_density_setting(gSavedSettings, "AlchemyWeatherSplashDensity", 0.65f);
    static LLCachedControl<F32> splash_size_setting(gSavedSettings, "AlchemyWeatherSplashRingSize", 0.28f);
    static LLCachedControl<F32> splash_lifetime_setting(gSavedSettings, "AlchemyWeatherSplashLifetime", 0.65f);
    static LLCachedControl<F32> splash_up_setting(gSavedSettings, "AlchemyWeatherSplashUpThreshold", 0.55f);
    static LLCachedControl<F32> splash_distance_setting(gSavedSettings, "AlchemyWeatherSplashMaxDistance", 48.f);
    static LLCachedControl<bool> wetness_enabled(gSavedSettings, "AlchemyWeatherWetnessEnabled", true);
    static LLCachedControl<F32> wetness_strength_setting(gSavedSettings, "AlchemyWeatherWetnessStrength", 0.35f);
    static LLCachedControl<bool> mist_enabled(gSavedSettings, "AlchemyWeatherMistEnabled", false);
    static LLCachedControl<F32> mist_strength_setting(gSavedSettings, "AlchemyWeatherMistStrength", 0.25f);
    static LLCachedControl<F32> mist_height_setting(gSavedSettings, "AlchemyWeatherMistHeight", 2.5f);
    static LLCachedControl<bool> lens_enabled(gSavedSettings, "AlchemyWeatherLensDropsEnabled", false);
    static LLCachedControl<F32> lens_strength_setting(gSavedSettings, "AlchemyWeatherLensDropsStrength", 0.25f);

    static LLCachedControl<bool> lightning_enabled(gSavedSettings, "AlchemyWeatherLightningEnabled", true);
    static LLCachedControl<F32> lightning_rate_setting(gSavedSettings, "AlchemyWeatherLightningRate", 1.5f);
    static LLCachedControl<F32> flash_duration_setting(gSavedSettings, "AlchemyWeatherLightningFlashDuration", 0.35f);
    static LLCachedControl<F32> bolt_duration_setting(gSavedSettings, "AlchemyWeatherLightningBoltDuration", 0.18f);
    static LLCachedControl<F32> min_distance_setting(gSavedSettings, "AlchemyWeatherLightningMinDistance", 45.f);
    static LLCachedControl<F32> max_distance_setting(gSavedSettings, "AlchemyWeatherLightningMaxDistance", 130.f);
    static LLCachedControl<F32> bolt_height_setting(gSavedSettings, "AlchemyWeatherLightningBoltHeight", 160.f);
    static LLCachedControl<F32> bolt_width_setting(gSavedSettings, "AlchemyWeatherLightningBoltWidth", 1.25f);
    static LLCachedControl<F32> lightning_brightness_setting(gSavedSettings, "AlchemyWeatherLightningBrightness", 4.f);
    static LLCachedControl<F32> lightning_ambient_setting(gSavedSettings, "AlchemyWeatherLightningAmbient", 0.15f);
    static LLCachedControl<LLColor3> lightning_color_setting(gSavedSettings, "AlchemyWeatherLightningColor", LLColor3(0.72f, 0.82f, 1.f));
    static LLCachedControl<U32> lightning_seed_setting(gSavedSettings, "AlchemyWeatherLightningSeed", 1597463007U);
    static LLCachedControl<bool> lightning_quality_enabled_setting(
        gSavedSettings, "AlchemyWeatherLightningQualityEnabled", false);
    static LLCachedControl<U32> lightning_quality_tier_setting(
        gSavedSettings, "AlchemyWeatherLightningQualityTier", 2U);
    static LLCachedControl<bool> lightning_sheet_enabled_setting(
        gSavedSettings, "AlchemyWeatherLightningSheetEnabled", false);
    static LLCachedControl<F32> lightning_sheet_strength_setting(
        gSavedSettings, "AlchemyWeatherLightningSheetStrength", 0.85f);
    static LLCachedControl<F32> lightning_corona_strength_setting(
        gSavedSettings, "AlchemyWeatherLightningCoronaStrength", 0.65f);
    static LLCachedControl<bool> lightning_wet_glint_enabled_setting(
        gSavedSettings, "AlchemyWeatherLightningWetGlintEnabled", false);
    static LLCachedControl<F32> lightning_wet_glint_strength_setting(
        gSavedSettings, "AlchemyWeatherLightningWetGlintStrength", 0.75f);
    static LLCachedControl<F32> lightning_distance_grading_setting(
        gSavedSettings, "AlchemyWeatherLightningDistanceGrading", 0.8f);
    static LLCachedControl<F32> lightning_afterglow_strength_setting(
        gSavedSettings, "AlchemyWeatherLightningAfterglowStrength", 0.35f);
    static LLCachedControl<F32> lightning_energy_ceiling_setting(
        gSavedSettings, "AlchemyWeatherLightningEnergyCeiling", 64.f);
    static LLCachedControl<bool> hdr_enabled_setting(
        gSavedSettings, "RenderHDREnabled");

    const auto finite_clamp = [](F32 value, F32 fallback, F32 low, F32 high)
    {
        return llclamp(std::isfinite(value) ? value : fallback, low, high);
    };

    const bool use_lightning_quality =
        lightning_quality_enabled_setting() &&
        hdr_enabled_setting() &&
        gGLManager.mGLVersion > 4.05f &&
        gDeferredWeatherLightningQualityProgram.isComplete();

    F32 cloud_factor = 1.f;
    F32 lightning_cloud_coverage = 0.f;
    F32 lightning_cloud_density1 = 1.f;
    F32 lightning_cloud_density2 = 1.f;
    F32 lightning_cloud_variance = 0.f;
    F32 lightning_cloud_scale = 0.42f;
    LLVector2 lightning_cloud_offset(0.f, 0.f);
    LLSettingsSky::ptr_t weather_sky =
        LLEnvironment::instance().getCurrentSky();
    if (weather_sky)
    {
        const F32 cloud = finite_clamp(
            weather_sky->getCloudShadow(), 1.f, 0.f, 1.f);
        const F32 coupling = finite_clamp(eep_coupling_setting, 0.35f, 0.f, 1.f);
        cloud_factor = lerp(1.f, cloud, coupling);
    }

    // Use the render iteration's frozen presentation clock, not a separate
    // wall-clock read. This stays monotonic in Live mode and is ready for
    // presentation scaling without coupling the model to viewer globals.
    const F64 now =
        LLPresentationTime::currentFrame().presentation_time;
    LLVector3 anchor_agent = gAgent.getPositionAgent();
    F32 anchor_ground = LLWorld::getInstance()->resolveLandHeightAgent(anchor_agent);
    if (!std::isfinite(anchor_ground))
    {
        anchor_ground = 0.f;
    }
    const ALWeatherModel::Position anchor{
        anchor_agent.mV[VX], anchor_agent.mV[VY], anchor_ground
    };

    ALWeatherModel::Config lightning_config;
    lightning_config.mEnabled = lightning_enabled();
    lightning_config.mRatePerMinute =
        finite_clamp(lightning_rate_setting, 1.5f, 0.f, 60.f) * cloud_factor;
    lightning_config.mFlashDurationSeconds =
        finite_clamp(flash_duration_setting, 0.35f, 0.05f, 4.f);
    lightning_config.mBoltDurationSeconds =
        finite_clamp(bolt_duration_setting, 0.18f, 0.02f, 4.f);
    lightning_config.mMinDistanceMeters =
        finite_clamp(min_distance_setting, 45.f, 0.f, 512.f);
    lightning_config.mMaxDistanceMeters =
        finite_clamp(max_distance_setting, 130.f,
                     static_cast<F32>(lightning_config.mMinDistanceMeters), 512.f);
    lightning_config.mBoltHeightMeters =
        finite_clamp(bolt_height_setting, 160.f, 16.f, 512.f);
    lightning_config.mQualityEnabled = use_lightning_quality;
    lightning_config.mQualityAfterglowStrength = use_lightning_quality
        ? finite_clamp(
            lightning_afterglow_strength_setting, 0.35f, 0.f, 1.f)
        : 0.f;
    lightning_config.mSeed = lightning_seed_setting();

    if (gSavedSettings.getBOOL("AlchemyWeatherLightningTrigger"))
    {
        gSavedSettings.setBOOL("AlchemyWeatherLightningTrigger", false);
        sWeatherController.trigger(now, lightning_config, anchor);
    }
    ALWeatherModel::Frame weather =
        sWeatherController.update(now, lightning_config, anchor);
    const bool lightning_sheet_active =
        use_lightning_quality && weather.mActive &&
        weather_sky &&
        lightning_sheet_enabled_setting() &&
        finite_clamp(
            lightning_sheet_strength_setting, 0.85f, 0.f, 2.f) > 0.f;
    if (lightning_sheet_active)
    {
        if (!sLightningSheetSnapshotValid ||
            weather.mStrikeId != sLightningSheetStrikeId ||
            weather.mStrikeTime != sLightningSheetStrikeTime ||
            lightning_config.mSeed != sLightningSheetSeed)
        {
            const LLColor3 density1 = weather_sky->getCloudPosDensity1();
            const LLColor3 density2 = weather_sky->getCloudPosDensity2();
            sLightningSheetCoverage = finite_clamp(
                weather_sky->getCloudShadow(), 0.f, 0.f, 1.f);
            sLightningSheetDensity1 =
                finite_clamp(density1.mV[2], 1.f, 0.f, 3.f);
            sLightningSheetDensity2 =
                finite_clamp(density2.mV[2], 1.f, 0.f, 1.f);
            sLightningSheetVariance = finite_clamp(
                weather_sky->getCloudVariance(), 0.f, 0.f, 1.f);
            sLightningSheetScale = finite_clamp(
                weather_sky->getCloudScale(), 0.42f, 0.001f, 3.f);
            sLightningSheetBaseOffset.set(
                finite_clamp(
                    density1.mV[0], 0.f, -65536.f, 65536.f),
                finite_clamp(
                    density1.mV[1], 0.f, -65536.f, 65536.f));
            sLightningSheetScrollRate = weather_sky->getCloudScrollRate();
            if (LLEnvironment::instance().isCloudScrollPaused())
            {
                sLightningSheetScrollRate.set(0.f, 0.f);
            }
            else
            {
                if (LLEnvironment::instance().isCloudScrollXLocked())
                {
                    sLightningSheetScrollRate.mV[0] = 0.f;
                }
                if (LLEnvironment::instance().isCloudScrollYLocked())
                {
                    sLightningSheetScrollRate.mV[1] = 0.f;
                }
            }
            sLightningSheetScrollRate.mV[0] = finite_clamp(
                sLightningSheetScrollRate.mV[0], 0.f, -50.f, 50.f);
            sLightningSheetScrollRate.mV[1] = finite_clamp(
                sLightningSheetScrollRate.mV[1], 0.f, -50.f, 50.f);
            sLightningSheetStrikeId = weather.mStrikeId;
            sLightningSheetStrikeTime = weather.mStrikeTime;
            sLightningSheetSeed = lightning_config.mSeed;
            sLightningSheetSnapshotValid = true;
        }

        lightning_cloud_coverage = sLightningSheetCoverage;
        lightning_cloud_density1 = sLightningSheetDensity1;
        lightning_cloud_density2 = sLightningSheetDensity2;
        lightning_cloud_variance = sLightningSheetVariance;
        lightning_cloud_scale = sLightningSheetScale;
        lightning_cloud_offset = sLightningSheetBaseOffset;

        // Snapshot the EEP proxy and effective scroll rate once per strike,
        // then integrate against strike presentation age. Mid-flash EEP edits,
        // pause changes, and lock changes cannot introduce a sheet phase jump.
        const F64 cloud_time =
            std::isfinite(now) && std::isfinite(weather.mStrikeTime)
                ? std::max(0.0, now - weather.mStrikeTime)
                : 0.0;
        const F64 scroll_x = std::fmod(
            cloud_time * sLightningSheetScrollRate.mV[0] / 100.0,
            65536.0);
        const F64 scroll_y = std::fmod(
            cloud_time * sLightningSheetScrollRate.mV[1] / 100.0,
            65536.0);
        lightning_cloud_offset.mV[0] = finite_clamp(
            lightning_cloud_offset.mV[0] -
                static_cast<F32>(scroll_x),
            0.f, -65536.f, 65536.f);
        lightning_cloud_offset.mV[1] = finite_clamp(
            lightning_cloud_offset.mV[1] +
                static_cast<F32>(scroll_y),
            0.f, -65536.f, 65536.f);
    }
    const F32 weather_flash =
        finite_clamp(weather.mFlash, 0.f, 0.f, 1.f);
    const F32 weather_bolt =
        finite_clamp(weather.mBolt, 0.f, 0.f, 1.f);
    const F32 weather_quality_bolt =
        finite_clamp(weather.mQualityBolt, 0.f, 0.f, 1.f);
    const F32 weather_afterglow =
        finite_clamp(weather.mAfterglow, 0.f, 0.f, 1.f);
    const F32 weather_color_variation =
        finite_clamp(weather.mColorVariation, 0.f, -1.f, 1.f);

    const F32 base_rain_intensity = rain_enabled()
        ? finite_clamp(rain_intensity_setting, 0.55f, 0.f, 1.f) * cloud_factor
        : 0.f;
    const bool splash_active =
        splash_enabled() &&
        finite_clamp(splash_density_setting, 0.65f, 0.f, 1.f) > 0.f;
    const bool wetness_active =
        wetness_enabled() &&
        finite_clamp(wetness_strength_setting, 0.35f, 0.f, 1.f) > 0.f;
    const bool lightning_wet_glint_active =
        use_lightning_quality && weather.mActive &&
        lightning_wet_glint_enabled_setting() &&
        finite_clamp(
            lightning_wet_glint_strength_setting, 0.75f, 0.f, 2.f) > 0.f &&
        wetness_active && base_rain_intensity > 0.f;
    const bool mist_active =
        mist_enabled() &&
        finite_clamp(mist_strength_setting, 0.25f, 0.f, 1.f) > 0.f;
    const bool lens_active =
        lens_enabled() &&
        finite_clamp(lens_strength_setting, 0.25f, 0.f, 1.f) > 0.f;
    const bool surface_active =
        splash_active || wetness_active || mist_active || lens_active;

    // A cover map is consumable only in the frame that produced it. This
    // prevents a camera teleport, origin shift, disabled producer, or failed
    // allocation from ever sampling stale world-space coverage.
    const bool rain_occlusion_current =
        rain_occlusion_setting() &&
        mWeatherRainOcclusionValid &&
        mWeatherRainOcclusionFrame == gFrameCount &&
        mWeatherRainOcclusion.getWidth() != 0 &&
        std::isfinite(mWeatherRainOcclusionDepthRange) &&
        mWeatherRainOcclusionDepthRange > 0.f;
    const bool use_rain_occlusion =
        rain_occlusion_current &&
        gDeferredWeatherRainOcclusionProgram.isComplete() &&
        gDeferredWeatherRainOcclusionProgram.getTextureChannel(
            LLShaderMgr::WEATHER_RAIN_OCCLUSION_MAP) >= 0;
    const bool use_surface_occlusion =
        rain_occlusion_current &&
        gDeferredWeatherSurfaceOcclusionProgram.isComplete() &&
        gDeferredWeatherSurfaceOcclusionProgram.getTextureChannel(
            LLShaderMgr::WEATHER_RAIN_OCCLUSION_MAP) >= 0;
    const bool use_lightning_occlusion =
        lightning_wet_glint_active &&
        rain_occlusion_current &&
        gDeferredWeatherLightningQualityProgram.getTextureChannel(
            LLShaderMgr::WEATHER_RAIN_OCCLUSION_MAP) >= 0;

    // The camera ray is a coarse whole-pass fallback and the one appropriate
    // exposure source for camera-space lens drops. A valid map keeps all world
    // effects spatially correct instead of globally suppressing doorway views.
    const bool needs_coarse_fallback =
        !use_rain_occlusion ||
        (surface_active && !use_surface_occlusion) ||
        (lightning_wet_glint_active && !use_lightning_occlusion);
    const bool needs_camera_shelter =
        needs_coarse_fallback || (lens_active && use_surface_occlusion);
    F32 exposed = 1.f;
    if (base_rain_intensity > 0.f && shelter_fade_setting() &&
        needs_camera_shelter)
    {
        const F32 shelter_height =
            finite_clamp(shelter_height_setting, 48.f, 8.f, 256.f);
        const LLVector3 camera_position =
            LLViewerCamera::getInstance()->getOrigin();
        F32 target_exposed = 1.f;
        if (camera_position.isFinite())
        {
            LLVector4a ray_start;
            LLVector4a ray_end;
            ray_start.set(
                camera_position.mV[VX], camera_position.mV[VY],
                camera_position.mV[VZ]);
            ray_end.set(
                camera_position.mV[VX], camera_position.mV[VY],
                camera_position.mV[VZ] + shelter_height);

            LLVector4a intersection;
            S32 face_hit = -1;
            LLViewerObject* occluder = lineSegmentIntersectInWorld(
                ray_start, ray_end,
                /*pick_transparent*/ false,
                /*pick_rigged*/ false,
                /*pick_unselectable*/ true,
                /*pick_reflection_probe*/ false,
                &face_hit, &intersection);

            // Rigged attachments are not picked. Reject a returned self avatar
            // or non-rigged self attachment too; a failed/rejected hit is sky.
            LLVOAvatar* hit_avatar =
                occluder ? occluder->asAvatar() : nullptr;
            if (occluder && !hit_avatar)
            {
                hit_avatar = occluder->getAvatar();
            }
            if (occluder && (!hit_avatar || !hit_avatar->isSelf()))
            {
                LLVector4a clearance;
                clearance.setSub(intersection, ray_start);
                const F32 hit_distance = clearance.getLength3().getF32();
                if (std::isfinite(hit_distance))
                {
                    // A normal nearby roof fully shelters the camera. Only
                    // soften the configured range boundary's upper 20%.
                    target_exposed = llsmoothstep(
                        shelter_height * 0.8f, shelter_height, hit_distance);
                }
            }
        }
        exposed = sWeatherShelterExposure.update(now, target_exposed);
    }
    else
    {
        // Disabling this optional gate is bit-for-bit factor 1 and raycast-free.
        sWeatherShelterExposure.reset();
    }
    const F32 rain_pass_intensity =
        base_rain_intensity * (use_rain_occlusion ? 1.f : exposed);
    const F32 surface_pass_intensity =
        base_rain_intensity * (use_surface_occlusion ? 1.f : exposed);
    LLGLSLShader& rain_program = use_rain_occlusion
        ? gDeferredWeatherRainOcclusionProgram
        : gDeferredWeatherRainProgram;
    LLGLSLShader& surface_program = use_surface_occlusion
        ? gDeferredWeatherSurfaceOcclusionProgram
        : gDeferredWeatherSurfaceProgram;
    LLGLSLShader& lightning_program = use_lightning_quality
        ? gDeferredWeatherLightningQualityProgram
        : gDeferredWeatherLightningProgram;
    const bool draw_rain =
        rain_pass_intensity > 0.f && rain_program.isComplete();
    const bool draw_surface =
        surface_pass_intensity > 0.f &&
        surface_active && surface_program.isComplete();
    const bool draw_lightning =
        weather.mActive && lightning_program.isComplete();
    if (!draw_rain && !draw_surface && !draw_lightning)
    {
        return;
    }

    // renderWeather runs from renderFinalize(), after render_ui() has begun
    // manipulating the live matrix stacks.  The deferred scene depth was produced
    // with the matrices snapshotted at the end of renderDeferredLighting(), so use
    // that same authoritative pair instead of depending on incidental post-pass
    // GL state.  The snapshot is current-frame here (renderDeferredLighting updates
    // gGLLast* before renderFinalize), despite the historical "Last" names.
    const glm::mat4 modelview = glm::make_mat4(gGLLastModelView);
    const glm::mat4 projection = glm::make_mat4(gGLLastProjection);
    const glm::mat4 inverse_modelview = glm::inverse(modelview);
    const glm::mat4 inverse_projection = glm::inverse(projection);
    const glm::mat4 view_projection = projection * modelview;

    static const LLStaticHashedString sInvModelview("weather_inv_modelview");
    static const LLStaticHashedString sWind("weather_wind");
    static const LLStaticHashedString sRainColor("weather_rain_color");
    static const LLStaticHashedString sTime("weather_time");
    static const LLStaticHashedString sIntensity("weather_intensity");
    static const LLStaticHashedString sDensity("weather_density");
    static const LLStaticHashedString sFallSpeed("weather_fall_speed");
    static const LLStaticHashedString sMaxDistance("weather_max_distance");
    static const LLStaticHashedString sLightning("weather_lightning");
    static const LLStaticHashedString sVirtualShutter("weather_virtual_shutter");
    static const LLStaticHashedString sNearEmphasis("weather_near_emphasis");
    static const LLStaticHashedString sGustStrength("weather_gust_strength");
    static const LLStaticHashedString sLayers("weather_layers");
    static const LLStaticHashedString sSamples("weather_samples");
    static const LLStaticHashedString sRainMapRes("weather_rain_map_res");
    static const LLStaticHashedString sSurfaceColor("weather_surface_color");
    static const LLStaticHashedString sSurfaceScreenRes("weather_screen_res");
    static const LLStaticHashedString sGroundHeight("weather_ground_height");
    static const LLStaticHashedString sSplashEnabled("weather_splash_enabled");
    static const LLStaticHashedString sSplashDensity("weather_splash_density");
    static const LLStaticHashedString sSplashRingSize("weather_splash_ring_size");
    static const LLStaticHashedString sSplashLifetime("weather_splash_lifetime");
    static const LLStaticHashedString sSplashUpThreshold("weather_splash_up_threshold");
    static const LLStaticHashedString sSplashMaxDistance("weather_splash_max_distance");
    static const LLStaticHashedString sWetnessEnabled("weather_wetness_enabled");
    static const LLStaticHashedString sWetnessStrength("weather_wetness_strength");
    static const LLStaticHashedString sMistEnabled("weather_mist_enabled");
    static const LLStaticHashedString sMistStrength("weather_mist_strength");
    static const LLStaticHashedString sMistHeight("weather_mist_height");
    static const LLStaticHashedString sLensEnabled("weather_lens_enabled");
    static const LLStaticHashedString sLensStrength("weather_lens_strength");
    static const LLStaticHashedString sLensExposure("weather_lens_exposure");
    static const LLStaticHashedString sRainOcclusionMatrix(
        "weather_rain_occlusion_matrix");
    static const LLStaticHashedString sRainOcclusionDepthRange(
        "weather_rain_occlusion_depth_range");
    static const LLStaticHashedString sRainOcclusionBias(
        "weather_rain_occlusion_bias");
    static const LLStaticHashedString sRainOcclusionSoftness(
        "weather_rain_occlusion_softness");
    static const LLStaticHashedString sRainOcclusionEnabled(
        "weather_rain_occlusion_enabled");

    if (draw_rain)
    {
        const U32 divisor = llclamp(rain_divisor_setting(), 1U, 4U);
        bool low_resolution =
            divisor > 1U &&
            gDeferredWeatherRainUpsampleProgram.isComplete() &&
            target->getWidth() >= divisor * 4U &&
            target->getHeight() >= divisor * 4U;
        if (low_resolution)
        {
            const U32 width = llmax(1U, target->getWidth() / divisor);
            const U32 height = llmax(1U, target->getHeight() / divisor);
            if (mWeatherRainHalf.getWidth() != width ||
                mWeatherRainHalf.getHeight() != height)
            {
                mWeatherRainHalf.release();
                if (!mWeatherRainHalf.allocate(width, height, GL_RGBA16F))
                {
                    low_resolution = false;
                }
            }
        }

        LLRenderTarget* rain_target =
            low_resolution ? &mWeatherRainHalf : target;
        rain_target->bindTarget();
        if (low_resolution)
        {
            LLGLDisable no_scissor(GL_SCISSOR_TEST);
            glClearColor(0.f, 0.f, 0.f, 0.f);
            rain_target->clear(GL_COLOR_BUFFER_BIT);
        }

        {
            LLGLDepthTest no_depth(GL_FALSE);
            LLGLEnable blend(GL_BLEND);
            LLGLDisable no_scissor(GL_SCISSOR_TEST);
            gGL.setSceneBlendType(LLRender::BT_ADD);
            gGL.setColorMask(true, false);

            bindDeferredShader(rain_program);
            S32 rain_occlusion_channel = -1;
            if (use_rain_occlusion)
            {
                rain_occlusion_channel = rain_program.bindTexture(
                    LLShaderMgr::WEATHER_RAIN_OCCLUSION_MAP,
                    &mWeatherRainOcclusion, true, LLTexUnit::TFO_POINT);
                rain_program.uniformMatrix4fv(
                    sRainOcclusionMatrix, 1, false,
                    glm::value_ptr(mWeatherRainOcclusionMatrix));
                rain_program.uniform1f(
                    sRainOcclusionDepthRange,
                    mWeatherRainOcclusionDepthRange);
                rain_program.uniform1f(
                    sRainOcclusionBias,
                    finite_clamp(
                        rain_occlusion_bias_setting, 0.12f, 0.f, 2.f));
                rain_program.uniform1f(
                    sRainOcclusionSoftness,
                    finite_clamp(
                        rain_occlusion_softness_setting, 0.35f,
                        0.001f, 4.f));
                rain_program.uniform1i(
                    sRainOcclusionEnabled,
                    rain_occlusion_channel >= 0 ? 1 : 0);
            }
            // getPosition() comes from deferredUtil and normally receives inv_proj
            // from the live GL projection.  Override it with the projection that
            // produced the sampled depth so view and world reconstruction use one
            // coherent current-frame camera pair.
            rain_program.uniformMatrix4fv(
                LLShaderMgr::INVERSE_PROJECTION_MATRIX, 1, false,
                glm::value_ptr(inverse_projection));
            const F32 wind_scale =
                finite_clamp(wind_scale_setting, 1.f, 0.f, 8.f);
            const F32 wind_x = finite_clamp(gWindVec.mV[VX], 0.f, -50.f, 50.f) * wind_scale;
            const F32 wind_y = finite_clamp(gWindVec.mV[VY], 0.f, -50.f, 50.f) * wind_scale;
            const LLColor3 rain_color_raw = rain_color_setting;
            const F32 rain_r =
                finite_clamp(rain_color_raw.mV[0], 0.55f, 0.f, 4.f);
            const F32 rain_g =
                finite_clamp(rain_color_raw.mV[1], 0.68f, 0.f, 4.f);
            const F32 rain_b =
                finite_clamp(rain_color_raw.mV[2], 0.78f, 0.f, 4.f);
            rain_program.uniformMatrix4fv(
                sInvModelview, 1, false, glm::value_ptr(inverse_modelview));
            rain_program.uniform3f(sWind, wind_x, wind_y, 0.f);
            rain_program.uniform3f(sRainColor, rain_r, rain_g, rain_b);
            rain_program.uniform1f(
                sTime, static_cast<F32>(std::fmod(now, 3600.0)));
            rain_program.uniform1f(sIntensity, rain_pass_intensity);
            rain_program.uniform1f(
                sDensity, finite_clamp(rain_density_setting, 0.65f, 0.02f, 1.f));
            rain_program.uniform1f(
                sFallSpeed, finite_clamp(rain_fall_speed_setting, 28.f, 1.f, 80.f));
            rain_program.uniform1f(
                sMaxDistance, finite_clamp(rain_distance_setting, 80.f, 8.f, 256.f));
            rain_program.uniform1f(sLightning, weather_flash);
            rain_program.uniform1f(
                sVirtualShutter,
                finite_clamp(rain_shutter_setting, 0.04f, 0.005f, 0.12f));
            rain_program.uniform1f(
                sNearEmphasis,
                finite_clamp(rain_near_setting, 0.65f, 0.f, 1.f));
            rain_program.uniform1f(
                sGustStrength,
                finite_clamp(rain_gust_setting, 0.35f, 0.f, 1.f));
            rain_program.uniform1i(
                sLayers, static_cast<S32>(llclamp(rain_layers_setting(), 1U, 3U)));
            rain_program.uniform1i(
                sSamples, static_cast<S32>(llclamp(rain_samples_setting(), 4U, 24U)));

            mScreenTriangleVB->setBuffer();
            mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
            if (rain_occlusion_channel >= 0)
            {
                rain_program.unbindTexture(
                    LLShaderMgr::WEATHER_RAIN_OCCLUSION_MAP);
            }
            unbindDeferredShader(rain_program);
        }
        rain_target->flush();

        if (low_resolution)
        {
            target->bindTarget();
            LLGLDepthTest no_depth(GL_FALSE);
            LLGLEnable blend(GL_BLEND);
            LLGLDisable no_scissor(GL_SCISSOR_TEST);
            gGL.setSceneBlendType(LLRender::BT_ADD);
            gGL.setColorMask(true, false);

            bindDeferredShader(gDeferredWeatherRainUpsampleProgram);
            gDeferredWeatherRainUpsampleProgram.uniformMatrix4fv(
                LLShaderMgr::INVERSE_PROJECTION_MATRIX, 1, false,
                glm::value_ptr(inverse_projection));
            const S32 rain_channel =
                gDeferredWeatherRainUpsampleProgram.bindTexture(
                    LLShaderMgr::WEATHER_RAIN_MAP, &mWeatherRainHalf, false,
                    LLTexUnit::TFO_BILINEAR);
            gDeferredWeatherRainUpsampleProgram.uniform2f(
                sRainMapRes, static_cast<F32>(mWeatherRainHalf.getWidth()),
                static_cast<F32>(mWeatherRainHalf.getHeight()));
            mScreenTriangleVB->setBuffer();
            mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
            if (rain_channel >= 0)
            {
                gDeferredWeatherRainUpsampleProgram.unbindTexture(
                    LLShaderMgr::WEATHER_RAIN_MAP);
            }
            unbindDeferredShader(gDeferredWeatherRainUpsampleProgram);
            target->flush();
        }
    }

    if (draw_surface)
    {
        target->bindTarget();
        {
            LLGLDepthTest no_depth(GL_FALSE);
            LLGLEnable blend(GL_BLEND);
            LLGLDisable no_scissor(GL_SCISSOR_TEST);
            gGL.setSceneBlendType(LLRender::BT_ADD);
            gGL.setColorMask(true, false);

            bindDeferredShader(surface_program);
            S32 surface_occlusion_channel = -1;
            if (use_surface_occlusion)
            {
                surface_occlusion_channel = surface_program.bindTexture(
                    LLShaderMgr::WEATHER_RAIN_OCCLUSION_MAP,
                    &mWeatherRainOcclusion, true, LLTexUnit::TFO_POINT);
                surface_program.uniformMatrix4fv(
                    sRainOcclusionMatrix, 1, false,
                    glm::value_ptr(mWeatherRainOcclusionMatrix));
                surface_program.uniform1f(
                    sRainOcclusionDepthRange,
                    mWeatherRainOcclusionDepthRange);
                surface_program.uniform1f(
                    sRainOcclusionBias,
                    finite_clamp(
                        rain_occlusion_bias_setting, 0.12f, 0.f, 2.f));
                surface_program.uniform1f(
                    sRainOcclusionSoftness,
                    finite_clamp(
                        rain_occlusion_softness_setting, 0.35f,
                        0.001f, 4.f));
                surface_program.uniform1i(
                    sRainOcclusionEnabled,
                    surface_occlusion_channel >= 0 ? 1 : 0);
                surface_program.uniform1f(
                    sLensExposure,
                    finite_clamp(exposed, 1.f, 0.f, 1.f));
            }
            surface_program.uniformMatrix4fv(
                LLShaderMgr::INVERSE_PROJECTION_MATRIX, 1, false,
                glm::value_ptr(inverse_projection));
            const LLColor3 rain_color_raw = rain_color_setting;
            const F32 rain_r =
                finite_clamp(rain_color_raw.mV[0], 0.55f, 0.f, 4.f);
            const F32 rain_g =
                finite_clamp(rain_color_raw.mV[1], 0.68f, 0.f, 4.f);
            const F32 rain_b =
                finite_clamp(rain_color_raw.mV[2], 0.78f, 0.f, 4.f);

            surface_program.uniformMatrix4fv(
                sInvModelview, 1, false, glm::value_ptr(inverse_modelview));
            surface_program.uniform3f(
                sSurfaceColor, rain_r, rain_g, rain_b);
            surface_program.uniform2f(
                sSurfaceScreenRes, static_cast<F32>(target->getWidth()),
                static_cast<F32>(target->getHeight()));
            surface_program.uniform1f(
                sTime, static_cast<F32>(std::fmod(now, 3600.0)));
            surface_program.uniform1f(
                sIntensity, surface_pass_intensity);
            surface_program.uniform1f(
                sLightning, weather_flash);
            surface_program.uniform1f(
                sGustStrength,
                finite_clamp(rain_gust_setting, 0.35f, 0.f, 1.f));
            surface_program.uniform1f(
                sGroundHeight, finite_clamp(anchor_ground, 0.f, -4096.f, 4096.f));

            surface_program.uniform1i(
                sSplashEnabled, splash_active ? 1 : 0);
            surface_program.uniform1f(
                sSplashDensity,
                finite_clamp(splash_density_setting, 0.65f, 0.f, 1.f));
            surface_program.uniform1f(
                sSplashRingSize,
                finite_clamp(splash_size_setting, 0.28f, 0.04f, 1.5f));
            surface_program.uniform1f(
                sSplashLifetime,
                finite_clamp(splash_lifetime_setting, 0.65f, 0.08f, 2.f));
            surface_program.uniform1f(
                sSplashUpThreshold,
                finite_clamp(splash_up_setting, 0.55f, 0.f, 0.98f));
            surface_program.uniform1f(
                sSplashMaxDistance,
                finite_clamp(splash_distance_setting, 48.f, 4.f, 128.f));

            surface_program.uniform1i(
                sWetnessEnabled, wetness_active ? 1 : 0);
            surface_program.uniform1f(
                sWetnessStrength,
                finite_clamp(wetness_strength_setting, 0.35f, 0.f, 1.f));
            surface_program.uniform1i(
                sMistEnabled, mist_active ? 1 : 0);
            surface_program.uniform1f(
                sMistStrength,
                finite_clamp(mist_strength_setting, 0.25f, 0.f, 1.f));
            surface_program.uniform1f(
                sMistHeight,
                finite_clamp(mist_height_setting, 2.5f, 0.25f, 12.f));
            surface_program.uniform1i(
                sLensEnabled, lens_active ? 1 : 0);
            surface_program.uniform1f(
                sLensStrength,
                finite_clamp(lens_strength_setting, 0.25f, 0.f, 1.f));

            mScreenTriangleVB->setBuffer();
            mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
            if (surface_occlusion_channel >= 0)
            {
                surface_program.unbindTexture(
                    LLShaderMgr::WEATHER_RAIN_OCCLUSION_MAP);
            }
            unbindDeferredShader(surface_program);
        }
        target->flush();
    }

    if (draw_lightning)
    {
        LLVector3 strike_base(
            static_cast<F32>(weather.mStrikeBase.mX),
            static_cast<F32>(weather.mStrikeBase.mY),
            static_cast<F32>(weather.mStrikeBase.mZ));
        const F32 resolved_ground =
            LLWorld::getInstance()->resolveLandHeightAgent(strike_base);
        if (std::isfinite(resolved_ground))
        {
            strike_base.mV[VZ] = resolved_ground;
        }
        LLVector3 strike_top = strike_base;
        strike_top.mV[VZ] += static_cast<F32>(weather.mBoltHeightMeters);

        // CPU-project a conservative bolt AABB once, so most fragments avoid
        // the quality tier's bounded but expensive segment search.
        F32 bolt_grade = 1.f;
        F32 surface_grade = 1.f;
        F32 sheet_grade = 1.f;
        F32 bolt_min_x = 2.f;
        F32 bolt_min_y = 2.f;
        F32 bolt_max_x = -1.f;
        F32 bolt_max_y = -1.f;
        bool bolt_bounds_valid = false;
        bool bolt_bounds_fullscreen = false;
        if (use_lightning_quality)
        {
            // Distance is renderer/camera state rather than weather simulation
            // state. Use the free-camera position for machinima, with the
            // model's area-uniform placement radius as a finite fallback.
            F32 strike_distance = finite_clamp(
                static_cast<F32>(weather.mStrikeDistanceMeters),
                static_cast<F32>(lightning_config.mMinDistanceMeters),
                0.f, 65536.f);
            const LLVector3 camera_position =
                LLViewerCamera::getInstance()->getOrigin();
            if (camera_position.isFinite() && strike_base.isFinite())
            {
                const F32 dx =
                    strike_base.mV[VX] - camera_position.mV[VX];
                const F32 dy =
                    strike_base.mV[VY] - camera_position.mV[VY];
                const F32 camera_distance = std::hypot(dx, dy);
                if (std::isfinite(camera_distance))
                {
                    strike_distance = camera_distance;
                }
            }
            const F32 grade_near = finite_clamp(
                static_cast<F32>(lightning_config.mMinDistanceMeters),
                45.f, 0.f, 512.f);
            const F32 grade_far = finite_clamp(
                static_cast<F32>(lightning_config.mMaxDistanceMeters),
                130.f, grade_near, 512.f);
            F32 distance_character = 0.f;
            const F32 squared_interval =
                grade_far * grade_far - grade_near * grade_near;
            if (squared_interval > 1.0e-4f)
            {
                distance_character = llclamp(
                    (strike_distance * strike_distance -
                     grade_near * grade_near) / squared_interval,
                    0.f, 1.f);
            }
            else
            {
                distance_character =
                    strike_distance > grade_far ? 1.f : 0.f;
            }
            distance_character = llsmoothstep(
                0.f, 1.f, distance_character);
            const F32 distance_strength = finite_clamp(
                lightning_distance_grading_setting, 0.8f, 0.f, 1.f);
            const F32 near_character = 1.f - distance_character;
            bolt_grade = lerp(
                1.f, near_character * near_character, distance_strength);
            surface_grade = lerp(
                1.f, lerp(1.f, 0.25f, distance_character),
                distance_strength);
            sheet_grade = lerp(
                1.f, lerp(1.f, 0.42f, distance_character),
                distance_strength);

            const F32 bolt_height =
                llmax(strike_top.mV[VZ] - strike_base.mV[VZ], 1.f);
            // Includes the worst fixed-depth subfork reach (including its
            // small below-base tail) and the return-stroke shimmer multiplier,
            // with a numerical guard band.
            const F32 lateral_extent = bolt_height * 0.32f;
            for (S32 x_side = -1; x_side <= 1; x_side += 2)
            {
                for (S32 y_side = -1; y_side <= 1; y_side += 2)
                {
                    for (S32 z_side = 0; z_side <= 1; ++z_side)
                    {
                        const glm::vec4 world_corner(
                            strike_base.mV[VX] +
                                static_cast<F32>(x_side) * lateral_extent,
                            strike_base.mV[VY] +
                                static_cast<F32>(y_side) * lateral_extent,
                            z_side ? strike_top.mV[VZ] :
                                     strike_base.mV[VZ] -
                                         bolt_height * 0.06f,
                            1.f);
                        const glm::vec4 clip =
                            view_projection * world_corner;
                        if (!std::isfinite(clip.x) ||
                            !std::isfinite(clip.y) ||
                            !std::isfinite(clip.w))
                        {
                            continue;
                        }
                        if (!(clip.w > 1.0e-4f))
                        {
                            bolt_bounds_fullscreen = true;
                            continue;
                        }
                        const F32 uv_x =
                            (clip.x / clip.w) * 0.5f + 0.5f;
                        const F32 uv_y =
                            (clip.y / clip.w) * 0.5f + 0.5f;
                        if (!std::isfinite(uv_x) || !std::isfinite(uv_y))
                        {
                            continue;
                        }
                        bolt_min_x = llmin(bolt_min_x, uv_x);
                        bolt_min_y = llmin(bolt_min_y, uv_y);
                        bolt_max_x = llmax(bolt_max_x, uv_x);
                        bolt_max_y = llmax(bolt_max_y, uv_y);
                        bolt_bounds_valid = true;
                    }
                }
            }
            if (bolt_bounds_fullscreen)
            {
                bolt_min_x = -0.25f;
                bolt_min_y = -0.25f;
                bolt_max_x = 1.25f;
                bolt_max_y = 1.25f;
            }
            else if (bolt_bounds_valid)
            {
                // Four corona sigmas leave less than 0.04% of a Gaussian
                // outside the rectangle and prevent a visible hard cutoff at
                // the maximum eight-pixel bolt width.
                const F32 quality_width = finite_clamp(
                    bolt_width_setting, 1.25f, 0.25f, 8.f);
                const F32 padding_pixels =
                    llmax(16.f, quality_width * 5.5f * 4.f);
                const F32 expand_x =
                    padding_pixels /
                    llmax(static_cast<F32>(target->getWidth()), 1.f);
                const F32 expand_y =
                    padding_pixels /
                    llmax(static_cast<F32>(target->getHeight()), 1.f);
                bolt_min_x -= expand_x;
                bolt_min_y -= expand_y;
                bolt_max_x += expand_x;
                bolt_max_y += expand_y;
            }
        }

        static const LLStaticHashedString sWorldToView("weather_world_to_view");
        static const LLStaticHashedString sViewProjection("weather_view_projection");
        static const LLStaticHashedString sStrikeBase("weather_strike_base");
        static const LLStaticHashedString sStrikeTop("weather_strike_top");
        static const LLStaticHashedString sLightningColor("weather_lightning_color");
        static const LLStaticHashedString sScreenRes("weather_screen_res");
        static const LLStaticHashedString sFlash("weather_lightning_flash");
        static const LLStaticHashedString sBolt("weather_lightning_bolt");
        static const LLStaticHashedString sWidth("weather_lightning_width");
        static const LLStaticHashedString sBrightness("weather_lightning_brightness");
        static const LLStaticHashedString sAmbient("weather_lightning_ambient");
        static const LLStaticHashedString sSeed("weather_lightning_seed");
        static const LLStaticHashedString sFarClip("weather_far_clip");
        static const LLStaticHashedString sViewToWorld("weather_view_to_world");
        static const LLStaticHashedString sBoltBounds("weather_bolt_bounds");
        static const LLStaticHashedString sCloudParams("weather_cloud_params");
        static const LLStaticHashedString sCloudOffset("weather_cloud_offset");
        static const LLStaticHashedString sDistanceGrade("weather_distance_grade");
        static const LLStaticHashedString sCloudScale("weather_cloud_scale");
        static const LLStaticHashedString sQualityBolt("weather_quality_bolt");
        static const LLStaticHashedString sAfterglow("weather_lightning_afterglow");
        static const LLStaticHashedString sColorVariation("weather_color_variation");
        static const LLStaticHashedString sSheetStrength("weather_sheet_strength");
        static const LLStaticHashedString sCoronaStrength("weather_corona_strength");
        static const LLStaticHashedString sWetGlintStrength("weather_wet_glint_strength");
        static const LLStaticHashedString sWetness("weather_wetness");
        static const LLStaticHashedString sWetExposure("weather_wet_exposure");
        static const LLStaticHashedString sEnergyCeiling("weather_lightning_energy_ceiling");
        static const LLStaticHashedString sQualityTier("weather_quality_tier");
        static const LLStaticHashedString sStrokeIndex("weather_stroke_index");
        static const LLStaticHashedString sSheetEnabled("weather_sheet_enabled");
        static const LLStaticHashedString sWetGlintEnabled("weather_wet_glint_enabled");

        target->bindTarget();
        {
            LLGLDepthTest no_depth(GL_FALSE);
            LLGLEnable blend(GL_BLEND);
            LLGLDisable no_scissor(GL_SCISSOR_TEST);
            gGL.setSceneBlendType(LLRender::BT_ADD);
            gGL.setColorMask(true, false);

            bindDeferredShader(lightning_program);
            S32 lightning_occlusion_channel = -1;
            if (use_lightning_quality && use_lightning_occlusion)
            {
                lightning_occlusion_channel =
                    lightning_program.bindTexture(
                        LLShaderMgr::WEATHER_RAIN_OCCLUSION_MAP,
                        &mWeatherRainOcclusion, true,
                        LLTexUnit::TFO_POINT);
            }
            const bool lightning_occlusion_bound =
                lightning_occlusion_channel >= 0;

            lightning_program.uniformMatrix4fv(
                LLShaderMgr::INVERSE_PROJECTION_MATRIX, 1, false,
                glm::value_ptr(inverse_projection));
            const LLColor3 lightning_color_raw = lightning_color_setting;
            const F32 lightning_r =
                finite_clamp(lightning_color_raw.mV[0], 0.72f, 0.f, 4.f);
            const F32 lightning_g =
                finite_clamp(lightning_color_raw.mV[1], 0.82f, 0.f, 4.f);
            const F32 lightning_b =
                finite_clamp(lightning_color_raw.mV[2], 1.f, 0.f, 4.f);
            lightning_program.uniformMatrix4fv(
                sWorldToView, 1, false, glm::value_ptr(modelview));
            lightning_program.uniformMatrix4fv(
                sViewProjection, 1, false, glm::value_ptr(view_projection));
            lightning_program.uniform3fv(
                sStrikeBase, 1, strike_base.mV);
            lightning_program.uniform3fv(
                sStrikeTop, 1, strike_top.mV);
            lightning_program.uniform3f(
                sLightningColor, lightning_r, lightning_g, lightning_b);
            lightning_program.uniform2f(
                sScreenRes, static_cast<F32>(target->getWidth()),
                static_cast<F32>(target->getHeight()));
            lightning_program.uniform1f(sFlash, weather_flash);
            lightning_program.uniform1f(sBolt, weather_bolt);
            lightning_program.uniform1f(
                sWidth, finite_clamp(bolt_width_setting, 1.25f, 0.25f, 8.f));
            lightning_program.uniform1f(
                sBrightness,
                finite_clamp(lightning_brightness_setting, 4.f, 0.f, 32.f));
            lightning_program.uniform1f(
                sAmbient,
                finite_clamp(lightning_ambient_setting, 0.15f, 0.f, 1.f));
            lightning_program.uniform1f(
                sSeed,
                use_lightning_quality
                    ? static_cast<F32>(weather.mVisualSeed)
                    : static_cast<F32>(weather.mStrikeId % 65521ULL));
            lightning_program.uniform1f(
                sFarClip,
                finite_clamp(
                    LLViewerCamera::getInstance()->getFar(),
                    RenderFarClip, 1.f, 4096.f));

            if (use_lightning_quality)
            {
                lightning_program.uniformMatrix4fv(
                    sViewToWorld, 1, false,
                    glm::value_ptr(inverse_modelview));
                lightning_program.uniform4f(
                    sBoltBounds,
                    bolt_min_x, bolt_min_y, bolt_max_x, bolt_max_y);
                lightning_program.uniform4f(
                    sCloudParams,
                    lightning_cloud_coverage,
                    lightning_cloud_density1,
                    lightning_cloud_density2,
                    lightning_cloud_variance);
                lightning_program.uniform2f(
                    sCloudOffset,
                    lightning_cloud_offset.mV[0],
                    lightning_cloud_offset.mV[1]);
                lightning_program.uniform3f(
                    sDistanceGrade,
                    bolt_grade, surface_grade, sheet_grade);
                lightning_program.uniform1f(
                    sCloudScale, lightning_cloud_scale);
                lightning_program.uniform1f(
                    sQualityBolt, weather_quality_bolt);
                lightning_program.uniform1f(
                    sAfterglow, weather_afterglow);
                lightning_program.uniform1f(
                    sColorVariation, weather_color_variation);
                lightning_program.uniform1f(
                    sSheetStrength,
                    finite_clamp(
                        lightning_sheet_strength_setting, 0.85f, 0.f, 2.f));
                lightning_program.uniform1f(
                    sCoronaStrength,
                    finite_clamp(
                        lightning_corona_strength_setting, 0.65f, 0.f, 2.f));
                lightning_program.uniform1f(
                    sWetGlintStrength,
                    finite_clamp(
                        lightning_wet_glint_strength_setting,
                        0.75f, 0.f, 2.f));
                lightning_program.uniform1f(
                    sWetness,
                    lightning_wet_glint_active
                        ? base_rain_intensity *
                            finite_clamp(
                                wetness_strength_setting, 0.35f, 0.f, 1.f)
                        : 0.f);
                lightning_program.uniform1f(
                    sWetExposure,
                    lightning_occlusion_bound ? 1.f : exposed);
                lightning_program.uniform1f(
                    sSplashUpThreshold,
                    finite_clamp(splash_up_setting, 0.55f, 0.f, 0.98f));
                lightning_program.uniform1f(
                    sSplashMaxDistance,
                    finite_clamp(
                        splash_distance_setting, 48.f, 4.f, 128.f));
                lightning_program.uniform1f(
                    sEnergyCeiling,
                    finite_clamp(
                        lightning_energy_ceiling_setting, 64.f, 1.f, 128.f));
                lightning_program.uniform1i(
                    sQualityTier,
                    static_cast<S32>(
                        llclamp(lightning_quality_tier_setting(), 1U, 3U)));
                lightning_program.uniform1i(
                    sStrokeIndex,
                    static_cast<S32>(llclamp(weather.mStrokeIndex, 0U, 3U)));
                lightning_program.uniform1i(
                    sSheetEnabled,
                    lightning_sheet_active ? 1 : 0);
                lightning_program.uniform1i(
                    sWetGlintEnabled,
                    lightning_wet_glint_active ? 1 : 0);
                lightning_program.uniform1i(
                    sRainOcclusionEnabled,
                    lightning_occlusion_bound ? 1 : 0);
                if (lightning_occlusion_bound)
                {
                    lightning_program.uniformMatrix4fv(
                        sRainOcclusionMatrix, 1, false,
                        glm::value_ptr(mWeatherRainOcclusionMatrix));
                    lightning_program.uniform1f(
                        sRainOcclusionDepthRange,
                        mWeatherRainOcclusionDepthRange);
                    lightning_program.uniform1f(
                        sRainOcclusionBias,
                        finite_clamp(
                            rain_occlusion_bias_setting,
                            0.12f, 0.f, 2.f));
                    lightning_program.uniform1f(
                        sRainOcclusionSoftness,
                        finite_clamp(
                            rain_occlusion_softness_setting,
                            0.35f, 0.001f, 4.f));
                }
            }

            mScreenTriangleVB->setBuffer();
            mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
            if (lightning_occlusion_channel >= 0)
            {
                lightning_program.unbindTexture(
                    LLShaderMgr::WEATHER_RAIN_OCCLUSION_MAP);
            }
            unbindDeferredShader(lightning_program);
        }
        target->flush();
    }

    gGL.setColorMask(true, true);
    gGL.setSceneBlendType(LLRender::BT_ALPHA);
}

// [BDMerge Froxel F0] Hybrid froxel volumetrics - foundation batch. Builds the
// camera-frustum froxel grid's media atlas (P1) and, when the debug lever is on,
// overlays a visualization of it. NO lighting / integration / scene composite yet
// [BDMerge] Shared dust-wind direction from the intuitive Azimuth / Elevation /
// Inverted controls (replaces the raw WindX/Y/Z sliders). Returns a UNIT direction
// in agent axes (X=east, Y=north, Z=up); the caller scales by its own NoiseSpeed.
//   Azimuth  : compass bearing the wind comes FROM, clockwise from true north
//              (0 = N, 90 = E). Default drift blows the OPPOSITE way, so az 0 =>
//              wind blows toward the south ("true north, southerly-facing").
//   Elevation: tilt; positive = comes from above => dust drifts downward.
//   Inverted : blow TOWARD the azimuth instead of away from it.
static void bdmerge_dust_wind_dir(F32 out[3])
{
    static LLCachedControl<F32>  az_deg(gSavedSettings, "BDMergeFroxelWindAzimuth",   0.f);
    static LLCachedControl<F32>  el_deg(gSavedSettings, "BDMergeFroxelWindElevation", 0.f);
    static LLCachedControl<bool> inverted(gSavedSettings, "BDMergeFroxelWindInverted", false);

    const F32 az = az_deg() * DEG_TO_RAD;
    const F32 el = el_deg() * DEG_TO_RAD;
    const F32 ce = cosf(el);
    // Direction the wind comes FROM (compass), then blow the opposite way by default.
    const F32 from_x = sinf(az) * ce;
    const F32 from_y = cosf(az) * ce;
    const F32 from_z = sinf(el);
    const F32 sign = inverted() ? 1.f : -1.f;
    out[0] = sign * from_x;
    out[1] = sign * from_y;
    out[2] = sign * from_z;
}

// (later batches). The WHOLE function is gated on BDMergeFroxelVolumetrics: at the
// default (off) it early-returns before allocating anything or running any pass, so
// the frame is byte-identical to today. Called right before renderProjectorVolumetric.
void LLPipeline::renderFroxelVolumetrics(LLRenderTarget* target)
{
    // [BDMerge Froxel F2] Clear the per-frame injected-projector set up front, BEFORE
    // the master gate's early-return, so that when the froxel master (or the lights
    // lever) is off the set is empty and renderProjectorVolumetric's per-cone loop is
    // never excluded - the per-cone path stays exactly as it was.
    mFroxelInjectedProjectors.clear();
    mFroxelLightValid = false;

    // Master gate: OFF => no alloc, no passes, no debug. Also require a non-cube
    // frame and a compiled media program (the debug pass is separately gated below).
    if (!BDMergeFroxelVolumetrics || gCubeSnapshot || !gFroxelMediaProgram.isComplete())
    {
        mFroxelMediaValid = false;
        mFroxelIntegratedValid = false; // [BDMerge Froxel F1]
        return;
    }

    LL_PROFILE_GPU_ZONE("renderFroxelVolumetrics");

    // ---- Grid + atlas layout ------------------------------------------------
    // Grid dims (clamped). GridZ is clamped to [16,128] per the atlas-tiling
    // requirement; X/Y get a generous clamp so a bad setting can't blow up VRAM.
    const U32 gx = llclamp(BDMergeFroxelGridX, (U32)16, (U32)512);
    const U32 gy = llclamp(BDMergeFroxelGridY, (U32)16, (U32)512);
    const U32 gz = llclamp(BDMergeFroxelGridZ, (U32)16, (U32)128);

    // Square-ish tiling: tilesX = ceil(sqrt(gz)), tilesY = ceil(gz / tilesX). The
    // atlas may hold a few dead tiles past slice gz-1 (the media pass clears them).
    const U32 tilesX = (U32)llceil(sqrtf((F32)gz));
    const U32 tilesY = (U32)llceil((F32)gz / (F32)tilesX);
    const U32 atlasW = gx * tilesX;
    const U32 atlasH = gy * tilesY;

    // Lazily (re)allocate the atlas ONLY inside this gated block. Mirrors the
    // mProjVolHalf on-demand allocate/release pattern; released in releaseGLBuffers.
    if (mFroxelMedia.getWidth() != atlasW || mFroxelMedia.getHeight() != atlasH)
    {
        mFroxelMedia.release();
        if (!mFroxelMedia.allocate(atlasW, atlasH, GL_RGBA16F))
        {
            mFroxelMediaValid = false;
            return; // allocation failed -> skip the whole subsystem this frame
        }
        mFroxelMediaValid = false;
    }

    // [BDMerge Froxel F1] Integrated atlas: same dims/format, allocated alongside the
    // media atlas (and only here, inside the gated block). Held separate so the
    // integrate pass reads media and writes integrated - two different textures, no
    // GL feedback-loop hazard.
    if (mFroxelIntegrated.getWidth() != atlasW || mFroxelIntegrated.getHeight() != atlasH)
    {
        mFroxelIntegrated.release();
        if (!mFroxelIntegrated.allocate(atlasW, atlasH, GL_RGBA16F))
        {
            mFroxelIntegratedValid = false;
            return; // allocation failed -> skip the whole subsystem this frame
        }
        mFroxelIntegratedValid = false;
    }

    // ---- Shared froxel uniforms (view-pos reconstruction) -------------------
    glm::mat4 mat  = get_current_modelview();
    glm::mat4 proj = get_current_projection();
    glm::mat4 inv_mv = glm::inverse(mat);

    // Centered perspective: proj[0][0] = 1/tan(fovx/2), proj[1][1] = 1/tan(fovy/2)
    // (glm is column-major: proj[col][row]). The froxel grid runs only in the main
    // centered view, so deriving the tan-half-fov terms this way is exact here.
    const F32 tan_half_fov_x = (fabsf(proj[0][0]) > 1e-6f) ? (1.f / proj[0][0]) : 1.f;
    const F32 tan_half_fov_y = (fabsf(proj[1][1]) > 1e-6f) ? (1.f / proj[1][1]) : 1.f;
    const F32 near_clip = LLViewerCamera::getInstance()->getNear();
    const F32 far_clip  = llmax(BDMergeFroxelFar, near_clip + 1.f);

    const F32 grid3[3]   = { (F32)gx, (F32)gy, (F32)gz };
    const F32 atlas4[4]  = { (F32)tilesX, (F32)tilesY, (F32)atlasW, (F32)atlasH };
    const F32 nearfar[2] = { near_clip, far_clip };
    const F32 thf2[2]    = { tan_half_fov_x, tan_half_fov_y };

    // ---- P1 media pass into the atlas ---------------------------------------
    {
        LL_PROFILE_GPU_ZONE("froxel media");
        mFroxelMedia.bindTarget(); // sets viewport to the atlas size

        LLGLDepthTest depth(GL_FALSE);
        LLGLDisable   no_blend(GL_BLEND);   // full overwrite: every froxel is written
        LLGLDisable   no_scissor(GL_SCISSOR_TEST);
        gGL.setColorMask(true, true);

        gFroxelMediaProgram.bind();
        gFroxelMediaProgram.uniform3fv(LLShaderMgr::FROXEL_GRID, 1, grid3);
        gFroxelMediaProgram.uniform4fv(LLShaderMgr::FROXEL_ATLAS, 1, atlas4);
        gFroxelMediaProgram.uniform2fv(LLShaderMgr::FROXEL_NEAR_FAR, 1, nearfar);
        gFroxelMediaProgram.uniform2fv(LLShaderMgr::FROXEL_TAN_HALF_FOV, 1, thf2);
        gFroxelMediaProgram.uniformMatrix4fv(LLShaderMgr::FROXEL_INV_MODELVIEW, 1, false, glm::value_ptr(inv_mv));
        gFroxelMediaProgram.uniform1f(LLShaderMgr::FROXEL_DENSITY, llmax(BDMergeFroxelDensity, 0.f));
        gFroxelMediaProgram.uniform1f(LLShaderMgr::FROXEL_FOG_STRENGTH, llclamp(BDMergeFroxelFogStrength, 0.f, 1.f));
        gFroxelMediaProgram.uniform1f(LLShaderMgr::FROXEL_FOG_GROUND, llmax(BDMergeFroxelFogGroundDensity, 0.f));
        gFroxelMediaProgram.uniform1f(LLShaderMgr::FROXEL_FOG_FALLOFF, llmax(BDMergeFroxelFogFalloff, 0.01f));
        // [F4 UX fix] FogBase as an absolute region altitude is unusable (the useful
        // band is +-10m out of 0-4096, and it breaks entirely in a skybox). In the
        // default agent-relative mode the setting is an OFFSET from the avatar's feet:
        // 0 = fog layer top right at your feet, +2 = knee/waist fog above the floor,
        // negative sinks it. Anchored to the AVATAR (not the camera) so the layer
        // stays put during flycam moves. Mode 0 keeps the legacy absolute semantics.
        {
            static LLCachedControl<U32> fog_base_mode(gSavedSettings, "BDMergeFroxelFogBaseMode", 1U);
            F32 eff_base = BDMergeFroxelFogBase;
            if (fog_base_mode != 0)
            {
                eff_base += gAgent.getPositionAgent().mV[VZ];
            }
            gFroxelMediaProgram.uniform1f(LLShaderMgr::FROXEL_FOG_BASE, eff_base);
        }
        gFroxelMediaProgram.uniform1f(LLShaderMgr::FROXEL_NOISE_STRENGTH, llclamp(BDMergeFroxelNoiseStrength, 0.f, 1.f));
        gFroxelMediaProgram.uniform1f(LLShaderMgr::FROXEL_NOISE_SCALE, llmax(BDMergeFroxelNoiseScale, 0.f));
        gFroxelMediaProgram.uniform1f(LLShaderMgr::FROXEL_NOISE_SPEED, BDMergeFroxelNoiseSpeed);
        // [F5] Directional drift from Azimuth/Elevation/Inverted (unit dir) scaled by
        // the master NoiseSpeed. Replaces the old raw WindX/Y/Z (and the hardwired
        // (1,1,1) diagonal before that).
        {
            F32 wdir[3];
            bdmerge_dust_wind_dir(wdir);
            F32 wind3[3] = {
                wdir[0] * BDMergeFroxelNoiseSpeed,
                wdir[1] * BDMergeFroxelNoiseSpeed,
                wdir[2] * BDMergeFroxelNoiseSpeed
            };
            gFroxelMediaProgram.uniform3fv(LLShaderMgr::FROXEL_WIND, 1, wind3);
        }
        gFroxelMediaProgram.uniform1f(LLShaderMgr::FROXEL_TIME, fmodf(gFrameTimeSeconds, 3600.f));

        mScreenTriangleVB->setBuffer();
        mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

        gFroxelMediaProgram.unbind();
        mFroxelMedia.flush();
        mFroxelMediaValid = true;
    }

    // ---- P2 light injection into the light atlas ----------------------------
    // ONE additive pass per volumetric-flagged projector (blend GL_ONE,GL_ONE) into
    // mFroxelLight, each binding ONE projector's cookie + shadow slot with the SAME
    // helpers the per-cone path uses (setupSpotLightVolumetric + the per-cone uniform
    // list). Every fragment is one froxel; the shader evaluates that projector at the
    // froxel centre (frustum/cookie/atten/shadow/phase) x the shared media sigma_s and
    // outputs the in-scatter source, so N lights = N ~2MP passes and O(1) integrate.
    // Gated on BDMergeFroxelLights AND shadow detail (the injection samples the spot
    // shadow maps, populated only when shadows are on). When off, nothing here runs,
    // the light atlas is neither allocated nor bound, and the integrate pass runs
    // exactly as F1 (ambient-only) via froxel_light_enable = 0.
    bool inject = BDMergeFroxelLights && RenderShadowDetail > 0 && !gCubeSnapshot &&
                  gFroxelInjectProgram.isComplete();
    if (inject &&
        (mFroxelLight.getWidth() != atlasW || mFroxelLight.getHeight() != atlasH))
    {
        mFroxelLight.release();
        if (!mFroxelLight.allocate(atlasW, atlasH, GL_RGBA16F))
        {
            inject = false; // allocation failed -> skip injection this frame
        }
    }

    // ---- P3 temporal eligibility + history atlases --------------------------
    // [BDMerge Froxel F3] The light atlas is the noisy/jittered component, so it (not
    // the deterministic media) is temporalized. Temporal runs only when the lever is
    // on, lights are actually injecting (nothing to resolve otherwise), and the resolve
    // program compiled. When off: no jitter is uploaded and the integrate pass reads the
    // raw light atlas directly => the exact F2 behavior. Two ping-pong RGBA16F history
    // atlases (atlas dims) mirror the per-cone mProjVolHistory pattern exactly.
    U32  froxel_injected = 0; // set by the inject loop; gates whether the resolve runs
    bool temporal = BDMergeFroxelTemporal && inject && gFroxelTemporalProgram.isComplete();

    // Grid-to-grid reprojection assumes the grid dims / near / far are frame-to-frame
    // constant; a change makes the stored history meaningless -> invalidate (blend
    // forced 0 this frame). Atlas-dim changes are also caught by the (re)alloc below.
    if (temporal && (mFroxelPrevGridX != gx || mFroxelPrevGridY != gy || mFroxelPrevGridZ != gz ||
                     mFroxelPrevNear != near_clip || mFroxelPrevFar != far_clip))
    {
        mFroxelHistoryValid = false;
    }

    if (temporal)
    {
        bool need_clear = false;
        for (int k = 0; k < 2; ++k)
        {
            if (mFroxelLightHistory[k].getWidth() != atlasW || mFroxelLightHistory[k].getHeight() != atlasH)
            {
                mFroxelLightHistory[k].release();
                if (!mFroxelLightHistory[k].allocate(atlasW, atlasH, GL_RGBA16F))
                    temporal = false;
                else
                    need_clear = true;
            }
        }
        if (temporal && need_clear)
        {
            mFroxelHistoryValid = false; // freshly (re)allocated -> no valid prev
            LLGLDisable no_scissor(GL_SCISSOR_TEST);
            for (int k = 0; k < 2; ++k)
            {
                mFroxelLightHistory[k].bindTarget();
                glClearColor(0.f, 0.f, 0.f, 0.f);
                mFroxelLightHistory[k].clear(GL_COLOR_BUFFER_BIT);
                mFroxelLightHistory[k].flush();
            }
        }
    }

    if (inject)
    {
        LL_PROFILE_GPU_ZONE("froxel inject");

        // Match the deferred spot loop's light-color scale (same as the per-cone path;
        // renderFinalize asserts !gCubeSnapshot so this is the plain global scale).
        static LLCachedControl<F32> alchemy_light_scale(gSavedSettings, "AlchemyGlobalLightScale", 1.f);
        const F32 light_scale = alchemy_light_scale;

        mFroxelLight.bindTarget(); // viewport = atlas size

        // Clear to 0 WITHOUT scissor (mirror the mProjVolHalf clear's guard against a
        // stale scissor rect leaving garbage in the atlas border), then accumulate.
        {
            LLGLDisable no_scissor(GL_SCISSOR_TEST);
            glClearColor(0.f, 0.f, 0.f, 0.f);
            mFroxelLight.clear(GL_COLOR_BUFFER_BIT);
        }

        LLGLDepthTest depth(GL_FALSE);
        LLGLEnable    blend(GL_BLEND);
        LLGLDisable   no_scissor(GL_SCISSOR_TEST);
        gGL.setSceneBlendType(LLRender::BT_ADD); // GL_ONE, GL_ONE additive over lights
        gGL.setColorMask(true, true);

        // bindDeferredShader ONCE (binds the full per-slot spot-shadow set so
        // sampleSpotShadow resolves); per-light cookie/geometry come from
        // setupSpotLightVolumetric inside the loop, exactly like the per-cone path.
        bindDeferredShader(gFroxelInjectProgram);

        // Shared froxel uniforms (froxel-centre reconstruction + media coupling).
        gFroxelInjectProgram.uniform3fv(LLShaderMgr::FROXEL_GRID, 1, grid3);
        gFroxelInjectProgram.uniform4fv(LLShaderMgr::FROXEL_ATLAS, 1, atlas4);
        gFroxelInjectProgram.uniform2fv(LLShaderMgr::FROXEL_NEAR_FAR, 1, nearfar);
        gFroxelInjectProgram.uniform2fv(LLShaderMgr::FROXEL_TAN_HALF_FOV, 1, thf2);

        // [BDMerge Froxel F3] Jitter gate + wrapped frame counter. Jitter is ON only
        // when the temporal resolve will run (it needs a resolve to average the noise);
        // with temporal off the gate is 0 => the exact deterministic F2 froxel-centre
        // sample. Frame counter mirrors the per-cone PROJVOL_FRAME upload.
        gFroxelInjectProgram.uniform1i(LLShaderMgr::FROXEL_JITTER, temporal ? 1 : 0);
        gFroxelInjectProgram.uniform1f(LLShaderMgr::FROXEL_FRAME, (F32)(LLFrameTimer::getFrameCount() % 1024u));

        S32 mch = gFroxelInjectProgram.enableTexture(LLShaderMgr::FROXEL_MEDIA);
        if (mch > -1)
        {
            mFroxelMedia.bindTexture(0, mch, LLTexUnit::TFO_POINT); // atlas-aligned point fetch
        }
        gFroxelInjectProgram.enableTexture(LLShaderMgr::DEFERRED_PROJECTION);

        mScreenTriangleVB->setBuffer();

        const U32 max_lights = llclamp(BDMergeFroxelMaxLights, (U32)1, (U32)16);
        U32 injected = 0;
        U32 overflow = 0;

        // Same slot iteration + validity + volumetric-flag matching as the per-cone
        // loop (renderProjectorVolumetric); slots beyond max_lights are counted as
        // overflow but not drawn (no per-frame spam - LL_DEBUGS only).
        for (U32 i = 0; i < bdmergeMaxSpotShadows(); ++i)
        {
            LLDrawable* drawablep = mShadowSpotLight[i];
            if (drawablep == NULL)
            {
                continue;
            }
            LLRenderTarget* shadow_target = getSpotShadowTarget(i);
            if (shadow_target == NULL || shadow_target->getWidth() == 0)
            {
                continue;
            }
            LLVOVolume* volume = drawablep->getVOVolume();
            if (volume == NULL)
            {
                continue;
            }

            // Match the light-source prim's own ID and its root-edit ID (identical to
            // the per-cone loop's session-only art-direction filter).
            LLUUID matched_id;
            if (isVolumetricShaftEnabled(volume->getID()))
            {
                matched_id = volume->getID();
            }
            else if (LLViewerObject* root = volume->getRootEdit())
            {
                if (isVolumetricShaftEnabled(root->getID()))
                    matched_id = root->getID();
            }
            if (matched_id.isNull())
            {
                continue;
            }

            // [BDMerge F4] Hero Beam split: a hero-flagged projector is NEVER
            // injected into the grid - it must render its sharp per-cone shaft on top
            // of the froxel atmosphere. Skipping here (before the cap and before
            // insertion into mFroxelInjectedProjectors) is exactly what makes the
            // per-cone loop march it fully (not rim-only). Non-hero projectors fall
            // through and inject as in F2/F3. Does not consume an injection slot.
            // [F4 fix] Match BOTH the light prim's own id AND its root id, NOT just
            // matched_id: the shaft flag and the hero flag can legitimately land on
            // different ids of the same linkset (Edit Linked selects the child as
            // its own selection root; a normal selection roots at the linkset root),
            // and testing only the shaft-matched id made Hero silently never match
            // in that case (reported in-world: "no change when selecting hero beam").
            bool is_hero = isHeroProjector(volume->getID());
            if (!is_hero)
            {
                if (LLViewerObject* root = volume->getRootEdit())
                {
                    is_hero = isHeroProjector(root->getID());
                }
            }
            if (is_hero)
            {
                // [F4 debug] Once per second at most: proves the hero exclusion is
                // actually firing for this projector (pairs with the HeroBeam toggle
                // log in alviewermenu). If a "hero does nothing" report shows the
                // toggle log but never this line, the flag/id plumbing is broken; if
                // both appear, the per-cone march side is where to look.
                static LLFrameTimer sHeroLogTimer;
                if (sHeroLogTimer.getElapsedTimeF32() > 1.f)
                {
                    sHeroLogTimer.reset();
                    LL_INFOS("HeroBeam") << "injection skip (hero) " << matched_id << LL_ENDL;
                }
                continue;
            }

            if (injected >= max_lights)
            {
                ++overflow; // flagged but over the per-frame cap -> not injected
                continue;
            }

            // Side-effect-free geometry + cookie upload; slot passed in directly.
            setupSpotLightVolumetric(gFroxelInjectProgram, drawablep, (S32)i);

            // Per-projector override, resolved exactly like the per-cone loop so the
            // grid beam honors the same captured art-direction (brightness/feather/g/
            // tint). Density is NOT applied here - the shared media atlas already
            // carries it via sigma_s (the physical scattering coupling).
            VolumetricShaftOverride ov;
            const bool has_ov = getVolumetricShaftOverride(matched_id, ov);
            const F32 e_mult     = has_ov ? ov.multiplier   : BDMergeProjectorVolumetricsMultiplier;
            const F32 e_feather  = has_ov ? ov.feather      : BDMergeProjectorVolumetricsFeather;
            const F32 e_g        = has_ov ? ov.anisotropy   : BDMergeProjectorVolumetricsAnisotropy;
            const LLColor3 e_tint = has_ov ? ov.tint        : BDMergeProjectorVolumetricsTint;
            const F32 e_tintStr  = has_ov ? ov.tintStrength : BDMergeProjectorVolumetricsTintStrength;

            gFroxelInjectProgram.uniform1f(LLShaderMgr::GODRAY_MULTIPLIER, e_mult);
            gFroxelInjectProgram.uniform1f(LLShaderMgr::PROJVOL_FEATHER, e_feather);
            gFroxelInjectProgram.uniform1f(LLShaderMgr::PROJVOL_G, e_g);

            LLColor3 col = volume->getLightLinearColor() * light_scale;
            if (e_tintStr > 0.f) // shaft tint lerp (no-op at TintStrength 0), same as per-cone
            {
                const F32 t = llclamp(e_tintStr, 0.f, 1.f);
                col = col * (1.f - t) + e_tint * (light_scale * t);
            }
            glm::vec3 c(drawablep->getPositionAgent());
            c = mul_mat4_vec3(mat, c); // agent -> view space
            const F32 radius = volume->getLightRadius() * 1.5f;

            gFroxelInjectProgram.uniform3fv(LLShaderMgr::LIGHT_CENTER, 1, glm::value_ptr(c));
            gFroxelInjectProgram.uniform1f(LLShaderMgr::LIGHT_SIZE, radius);
            gFroxelInjectProgram.uniform3fv(LLShaderMgr::DIFFUSE_COLOR, 1, col.mV);
            gFroxelInjectProgram.uniform1f(LLShaderMgr::LIGHT_FALLOFF, volume->getLightFalloff(DEFERRED_LIGHT_FALLOFF));

            mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

            // Mark this projector injected so the per-cone loop skips its shaft.
            mFroxelInjectedProjectors.insert(matched_id);
            ++injected;
        }

        if (overflow > 0)
        {
            LL_DEBUGS("Pipeline") << "Froxel light injection capped at " << max_lights
                                  << " projectors (" << overflow << " more flagged, skipped this frame)" << LL_ENDL;
        }

        gFroxelInjectProgram.disableTexture(LLShaderMgr::DEFERRED_PROJECTION);
        if (mch > -1)
        {
            gFroxelInjectProgram.disableTexture(LLShaderMgr::FROXEL_MEDIA);
        }
        unbindDeferredShader(gFroxelInjectProgram);

        gGL.setColorMask(true, true);
        gGL.setSceneBlendType(LLRender::BT_ALPHA); // restore default
        mFroxelLight.flush();
        mFroxelLightValid = true;
        froxel_injected = injected; // [F3] number of lights that actually injected
    }

    // ---- P3 temporal resolve into the light-atlas history -------------------
    // [BDMerge Froxel F3] One full-atlas draw: reproject each froxel's WORLD position
    // into the previous frame's grid (pure grid-to-grid, no surface depth - so it
    // cannot ghost a beam against a wall), trilinear-sample the previous RESOLVED light
    // atlas, clamp it to the current froxel's 6-neighborhood, and EMA-blend with the
    // freshly-injected light. Writes the current history slot; the integrate pass below
    // reads THAT resolved atlas (and it becomes next frame's history). Reads mFroxelLight
    // + history[prev], writes history[cur] - all distinct textures, no feedback hazard.
    // Runs only when temporal is active AND at least one light injected this frame.
    LLRenderTarget* froxel_light_src = &mFroxelLight; // integrate reads this (raw by default)
    if (temporal && mFroxelLightValid && froxel_injected > 0)
    {
        LL_PROFILE_GPU_ZONE("froxel temporal");
        const U32 cur  = mFroxelHistoryIdx & 1u;
        const U32 prev = cur ^ 1u;
        LLRenderTarget& dst  = mFroxelLightHistory[cur];
        LLRenderTarget& hist = mFroxelLightHistory[prev];

        dst.bindTarget(); // viewport = atlas size

        LLGLDepthTest depth(GL_FALSE);
        LLGLDisable   no_blend(GL_BLEND);   // full overwrite of the slot
        LLGLDisable   no_scissor(GL_SCISSOR_TEST);
        gGL.setColorMask(true, true);

        gFroxelTemporalProgram.bind();

        S32 lch = gFroxelTemporalProgram.enableTexture(LLShaderMgr::FROXEL_LIGHT);
        if (lch > -1)
        {
            mFroxelLight.bindTexture(0, lch, LLTexUnit::TFO_POINT); // current, atlas-aligned point fetch
        }
        S32 hch = gFroxelTemporalProgram.enableTexture(LLShaderMgr::FROXEL_LIGHT_HISTORY);
        if (hch > -1)
        {
            hist.bindTexture(0, hch, LLTexUnit::TFO_BILINEAR); // previous resolved, trilinear
        }

        gFroxelTemporalProgram.uniform3fv(LLShaderMgr::FROXEL_GRID, 1, grid3);
        gFroxelTemporalProgram.uniform4fv(LLShaderMgr::FROXEL_ATLAS, 1, atlas4);
        gFroxelTemporalProgram.uniform2fv(LLShaderMgr::FROXEL_NEAR_FAR, 1, nearfar);
        gFroxelTemporalProgram.uniform2fv(LLShaderMgr::FROXEL_TAN_HALF_FOV, 1, thf2);
        gFroxelTemporalProgram.uniformMatrix4fv(LLShaderMgr::FROXEL_INV_MODELVIEW, 1, false, glm::value_ptr(inv_mv));
        // Previous frame's world->view; meaningless until history valid, so the blend is
        // forced 0 that first frame (below), exactly like the per-cone prev-viewproj.
        gFroxelTemporalProgram.uniformMatrix4fv(LLShaderMgr::FROXEL_PREV_MODELVIEW, 1, false, mFroxelPrevModelview);
        const F32 blend = mFroxelHistoryValid ? llclamp(BDMergeFroxelTemporalBlend, 0.f, 0.95f) : 0.f;
        gFroxelTemporalProgram.uniform1f(LLShaderMgr::FROXEL_TEMPORAL_BLEND, blend);

        mScreenTriangleVB->setBuffer();
        mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

        if (lch > -1)
        {
            gFroxelTemporalProgram.disableTexture(LLShaderMgr::FROXEL_LIGHT);
        }
        if (hch > -1)
        {
            gFroxelTemporalProgram.disableTexture(LLShaderMgr::FROXEL_LIGHT_HISTORY);
        }
        gFroxelTemporalProgram.unbind();
        dst.flush();

        // Snapshot this frame's world->view + grid params for next frame's reprojection;
        // advance the ping-pong (next frame reads the slot we just wrote); the resolved
        // slot is what integrate reads; history is valid from here on.
        memcpy(mFroxelPrevModelview, glm::value_ptr(mat), sizeof(mFroxelPrevModelview));
        mFroxelPrevGridX = gx; mFroxelPrevGridY = gy; mFroxelPrevGridZ = gz;
        mFroxelPrevNear  = near_clip; mFroxelPrevFar = far_clip;
        froxel_light_src      = &dst;
        mFroxelHistoryIdx     = prev;
        mFroxelHistoryValid   = true;
    }
    else
    {
        // No resolve this frame -> the stored history can't be cleanly reprojected next
        // frame; drop it so the next temporal frame restarts from pure current (blend 0).
        mFroxelHistoryValid = false;
    }

    // ---- P4 integrate pass into the integrated atlas ------------------------
    // One full-atlas draw: every output fragment (fx,fy,k) front-to-back integrates
    // the media column j=0..k with Hillaire's energy-conserving slice integral,
    // writing rgb = accumulated in-scatter L, a = transmittance T. Reads ONLY the
    // media atlas (a different texture than the write target), so there is no GL
    // read/write feedback loop. Skipped if the program failed to compile - the apply
    // pass below is guarded on mFroxelIntegratedValid so the scene is untouched.
    if (gFroxelIntegrateProgram.isComplete())
    {
        LL_PROFILE_GPU_ZONE("froxel integrate");
        mFroxelIntegrated.bindTarget(); // viewport = atlas size

        LLGLDepthTest depth(GL_FALSE);
        LLGLDisable   no_blend(GL_BLEND);   // full overwrite: every froxel is written
        LLGLDisable   no_scissor(GL_SCISSOR_TEST);
        gGL.setColorMask(true, true);

        gFroxelIntegrateProgram.bind();

        S32 ch = gFroxelIntegrateProgram.enableTexture(LLShaderMgr::FROXEL_MEDIA);
        if (ch > -1)
        {
            mFroxelMedia.bindTexture(0, ch, LLTexUnit::TFO_POINT); // point-fetch the aligned column
        }

        // [BDMerge Froxel F2] Bind the light atlas + gate the light term. When lights
        // are off (mFroxelLightValid false) the gate is 0 and the shader never samples
        // the light atlas -> the integrate output is exactly F1 (ambient-only).
        // [F3] Bind the RESOLVED light atlas when the temporal resolve ran this frame
        // (froxel_light_src points at the history slot it wrote); otherwise this is the
        // raw injection atlas => exactly F2. Same layout/format either way.
        const bool light_on = mFroxelLightValid;
        S32 lch = -1;
        gFroxelIntegrateProgram.uniform1i(LLShaderMgr::FROXEL_LIGHT_ENABLE, light_on ? 1 : 0);
        if (light_on)
        {
            lch = gFroxelIntegrateProgram.enableTexture(LLShaderMgr::FROXEL_LIGHT);
            if (lch > -1)
            {
                froxel_light_src->bindTexture(0, lch, LLTexUnit::TFO_POINT); // atlas-aligned point fetch
            }
        }

        gFroxelIntegrateProgram.uniform3fv(LLShaderMgr::FROXEL_GRID, 1, grid3);
        gFroxelIntegrateProgram.uniform4fv(LLShaderMgr::FROXEL_ATLAS, 1, atlas4);
        gFroxelIntegrateProgram.uniform2fv(LLShaderMgr::FROXEL_NEAR_FAR, 1, nearfar);
        gFroxelIntegrateProgram.uniform1f(LLShaderMgr::FROXEL_AMBIENT, llmax(BDMergeFroxelAmbient, 0.f));

        mScreenTriangleVB->setBuffer();
        mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

        if (ch > -1)
        {
            gFroxelIntegrateProgram.disableTexture(LLShaderMgr::FROXEL_MEDIA);
        }
        if (lch > -1)
        {
            gFroxelIntegrateProgram.disableTexture(LLShaderMgr::FROXEL_LIGHT);
        }
        gFroxelIntegrateProgram.unbind();
        mFroxelIntegrated.flush();
        mFroxelIntegratedValid = true;
    }

    // ---- P5 apply composite onto the scene ----------------------------------
    // Fullscreen pass onto `target` (mRT->screen). Per pixel: view depth -> continuous
    // slice coord (minus the 0.5 slice-centre offset), trilinear-sample the integrated
    // atlas -> (L, T), output frag_color = vec4(L, T). Blend GL_ONE / GL_SRC_ALPHA so
    // the framebuffer becomes dst' = L + dst*T == scene*T + L (fog occludes AND glows)
    // WITHOUT ever sampling the target it writes. Runs before the function returns so
    // the per-cone hero shafts composite additively on top afterward.
    if (mFroxelIntegratedValid && target != nullptr && gFroxelApplyProgram.isComplete())
    {
        LL_PROFILE_GPU_ZONE("froxel apply");
        target->bindTarget();

        LLGLDepthTest depth(GL_FALSE);   // depth test off, depth write off
        LLGLEnable    blend(GL_BLEND);
        LLGLDisable   no_scissor(GL_SCISSOR_TEST);
        // dst' = src*ONE + dst*SRC_ALPHA = L + scene*T. Restored to BT_ALPHA below.
        gGL.blendFunc(LLRender::BF_ONE, LLRender::BF_SOURCE_ALPHA);
        gGL.setColorMask(true, false); // scene rgb only; leave scene alpha intact

        // isDeferred bind: getPosition()/depthMap/inv_proj for the surface view depth.
        bindDeferredShader(gFroxelApplyProgram);

        S32 ch = gFroxelApplyProgram.enableTexture(LLShaderMgr::FROXEL_INTEGRATED);
        if (ch > -1)
        {
            mFroxelIntegrated.bindTexture(0, ch, LLTexUnit::TFO_BILINEAR); // trilinear = manual Z lerp of two bilinear taps
        }

        gFroxelApplyProgram.uniform3fv(LLShaderMgr::FROXEL_GRID, 1, grid3);
        gFroxelApplyProgram.uniform4fv(LLShaderMgr::FROXEL_ATLAS, 1, atlas4);
        gFroxelApplyProgram.uniform2fv(LLShaderMgr::FROXEL_NEAR_FAR, 1, nearfar);

        mScreenTriangleVB->setBuffer();
        mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

        if (ch > -1)
        {
            gFroxelApplyProgram.disableTexture(LLShaderMgr::FROXEL_INTEGRATED);
        }
        unbindDeferredShader(gFroxelApplyProgram);
        gGL.setSceneBlendType(LLRender::BT_ALPHA); // restore default src_alpha / one-minus-src_alpha
        gGL.setColorMask(true, true);
        target->flush();
    }

    // ---- Debug overlay (optional, gated separately) -------------------------
    // Draws AFTER the media pass into the scene target at 50% opacity. This is the
    // F0 checkpoint deliverable. No-op when BDMergeFroxelDebug == 0.
    const U32 debug_mode = BDMergeFroxelDebug;
    if (debug_mode != 0 && target != nullptr && gFroxelDebugProgram.isComplete())
    {
        LL_PROFILE_GPU_ZONE("froxel debug");
        target->bindTarget();

        LLGLDepthTest depth(GL_FALSE);
        LLGLEnable    blend(GL_BLEND);
        LLGLDisable   no_scissor(GL_SCISSOR_TEST);
        gGL.setSceneBlendType(LLRender::BT_ALPHA); // src_alpha / one-minus-src-alpha
        gGL.setColorMask(true, false);

        // isDeferred bind: gives getPosition()/depthMap/inv_proj for the surface sample.
        bindDeferredShader(gFroxelDebugProgram);

        S32 ch = gFroxelDebugProgram.enableTexture(LLShaderMgr::FROXEL_MEDIA);
        if (ch > -1)
        {
            mFroxelMedia.bindTexture(0, ch, LLTexUnit::TFO_BILINEAR);
        }

        // [BDMerge Froxel F1] Mode 3 visualizes the integrated atlas; bind it too
        // (valid by now - the integrate pass ran above). Harmless for modes 1/2.
        S32 chi = gFroxelDebugProgram.enableTexture(LLShaderMgr::FROXEL_INTEGRATED);
        if (chi > -1 && mFroxelIntegratedValid)
        {
            mFroxelIntegrated.bindTexture(0, chi, LLTexUnit::TFO_BILINEAR);
        }

        // [BDMerge Froxel F2] Mode 4 visualizes the light atlas; bind it too (valid
        // only when injection ran this frame). Harmless for modes 1/2/3.
        S32 chl = gFroxelDebugProgram.enableTexture(LLShaderMgr::FROXEL_LIGHT);
        if (chl > -1 && mFroxelLightValid)
        {
            mFroxelLight.bindTexture(0, chl, LLTexUnit::TFO_BILINEAR);
        }

        // Mode 2 shows a fixed middle Z-slice tile (simple, deterministic).
        const F32 debug_slice = floorf((F32)gz * 0.5f);

        gFroxelDebugProgram.uniform3fv(LLShaderMgr::FROXEL_GRID, 1, grid3);
        gFroxelDebugProgram.uniform4fv(LLShaderMgr::FROXEL_ATLAS, 1, atlas4);
        gFroxelDebugProgram.uniform2fv(LLShaderMgr::FROXEL_NEAR_FAR, 1, nearfar);
        gFroxelDebugProgram.uniform1f(LLShaderMgr::FROXEL_DENSITY, llmax(BDMergeFroxelDensity, 1e-4f));
        gFroxelDebugProgram.uniform1i(LLShaderMgr::FROXEL_DEBUG_MODE, (S32)debug_mode);
        gFroxelDebugProgram.uniform1f(LLShaderMgr::FROXEL_DEBUG_SLICE, debug_slice);

        mScreenTriangleVB->setBuffer();
        mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

        if (ch > -1)
        {
            gFroxelDebugProgram.disableTexture(LLShaderMgr::FROXEL_MEDIA);
        }
        if (chi > -1)
        {
            gFroxelDebugProgram.disableTexture(LLShaderMgr::FROXEL_INTEGRATED); // [BDMerge Froxel F1]
        }
        if (chl > -1)
        {
            gFroxelDebugProgram.disableTexture(LLShaderMgr::FROXEL_LIGHT); // [BDMerge Froxel F2]
        }
        unbindDeferredShader(gFroxelDebugProgram);
        gGL.setColorMask(true, true);
        target->flush();
    }
}

// [BDMerge G3.3] per-projector volumetric light cones (visible spotlight shafts).
// NET-NEW local-light companion to G3.2. Runs in renderFinalize right after the
// sun volumetric block, ADDITIVELY IN PLACE onto the scene buffer: one fullscreen
// cone per shadow-casting projector slot, each outputting ONLY its shaft delta
// (frag_color = shaft), blended GL_ONE,GL_ONE. Because the shader never samples
// the buffer it writes, there is no read/write feedback and no extra ping-pong
// target is needed (it still reads depthMap - a separate texture - to clamp the
// march at the first opaque surface). Cost scales with N x resolution; N is
// naturally capped by BDMergeMaxSpotShadows and resolution is the primary lever.
void LLPipeline::renderProjectorVolumetric(LLRenderTarget* target, bool aux_direct)
{
    // [Prism camera feed] The Prism auxiliary (VCam) capture calls this pass
    // BEFORE the main view every frame (LLPrismLens::renderAuxiliaryView). This
    // function mutates shared main-view members - mProjVolHalfValid,
    // mProjVolShaftSrc, and CRITICALLY mProjVolHistoryValid, which the main view's
    // OWN call reads (below, at the temporal resolve) to gate its temporal
    // reprojection. If the aux left those reset, the main view would silently lose
    // its projector-volumetric temporal accumulation (noisier main-view shafts when
    // BDMergeProjectorVolumetricsTemporal is on). Snapshot the three at entry and
    // restore them at EVERY exit: this RAII guard's destructor fires on the
    // early-out return below AND on the normal end, so no exit path can leak the
    // aux's mutations into the main view. Completely inert when aux_direct is false
    // (the main-view call), so that path stays byte-identical to before.
    struct ProjVolAuxStateGuard
    {
        bool             mActive;
        bool*            mHalfValidPtr;
        LLRenderTarget** mShaftSrcPtr;
        bool*            mHistoryValidPtr;
        bool             mHalfValid;
        LLRenderTarget*  mShaftSrc;
        bool             mHistoryValid;
        ProjVolAuxStateGuard(bool active, bool* halfv, LLRenderTarget** shaft, bool* histv)
        :   mActive(active), mHalfValidPtr(halfv), mShaftSrcPtr(shaft),
            mHistoryValidPtr(histv), mHalfValid(*halfv), mShaftSrc(*shaft),
            mHistoryValid(*histv)
        {}
        ~ProjVolAuxStateGuard()
        {
            if (mActive)
            {
                *mHalfValidPtr    = mHalfValid;
                *mShaftSrcPtr     = mShaftSrc;
                *mHistoryValidPtr = mHistoryValid;
            }
        }
    } projvol_aux_guard(aux_direct, &mProjVolHalfValid, &mProjVolShaftSrc,
                        &mProjVolHistoryValid);

    // [Phase 3 item 4] Invalidate the bloom-feed source each frame up front: it is
    // only re-validated below if the half-res path actually marches a shaft, so a
    // disabled/empty frame can never let the feed sample a stale mProjVolHalf.
    mProjVolHalfValid = false;
    mProjVolShaftSrc = nullptr;

    if (!BDMergeProjectorVolumetrics || RenderShadowDetail <= 0 || gCubeSnapshot ||
        !gDeferredProjectorVolumetricProgram.isComplete())
    {
        mProjVolHistoryValid = false; // effect off -> stale history can't be reused
        return;
    }

    LL_PROFILE_GPU_ZONE("renderProjectorVolumetric");

    // Match the deferred spot loop's light-color scale (no cube snapshot here -
    // renderFinalize asserts !gCubeSnapshot - so this is the plain global scale).
    static LLCachedControl<F32> alchemy_light_scale(gSavedSettings, "AlchemyGlobalLightScale", 1.f);
    const F32 light_scale = alchemy_light_scale;

    // View-space transform for the light center (same as the fullscreen spot
    // loop): center must live in the same space getPosition() reconstructs.
    // [Phase 1 items 4/5] projection matrix is needed to project each cone's
    // sphere of influence to screen space (scissor + adaptive sample count).
    glm::mat4 mat  = get_current_modelview();
    glm::mat4 proj = get_current_projection();

    // [Phase 1 item 3] Half-res march path: march the cones into a half-resolution
    // scratch target (mProjVolHalf) and depth-aware bilateral-upsample the result
    // onto the scene. Big fill-rate win (quarter the marched fragments) for the
    // common 2+ flagged-projector case. Falls back to the fullscreen path if the
    // half-res target can't be allocated or the scene is too small to halve.
    // [Prism camera feed] The aux capture FORCES the direct fullscreen march
    // (halfres=false, which also forces temporal=false below): the shafts land
    // straight onto `target` (== the aux rt.screen) and NONE of the shared
    // half-res / history buffers (mProjVolHalf, mProjVolHistory) are touched, so
    // the aux run stays isolated from the main view's projector-volumetric state.
    bool halfres = !aux_direct &&
                   BDMergeProjectorVolumetricsHalfRes &&
                   gDeferredProjectorVolumetricUpsampleProgram.isComplete() &&
                   target->getWidth() >= 8 && target->getHeight() >= 8;
    if (halfres)
    {
        U32 hw = llmax(1U, target->getWidth() / 2);
        U32 hh = llmax(1U, target->getHeight() / 2);
        if (mProjVolHalf.getWidth() != hw || mProjVolHalf.getHeight() != hh)
        {
            mProjVolHalf.release();
            if (!mProjVolHalf.allocate(hw, hh, GL_RGBA16F))
            {
                halfres = false; // allocation failed -> fullscreen fallback
            }
        }
    }

    // [Batch 1 A] Temporal reprojection accumulation. Requires the half-res path
    // (its history + resolve live in half-res) and its own program. Two ping-pong
    // half-res RGBA16F history targets carry the accumulated shaft (rgb) + stored
    // view depth (a). On (re)allocation the pair is cleared and history is marked
    // invalid so the first frame blends against black.
    bool temporal = halfres && BDMergeProjectorVolumetricsTemporal &&
                    gDeferredProjectorVolumetricTemporalProgram.isComplete();
    if (temporal)
    {
        U32 hw = mProjVolHalf.getWidth();
        U32 hh = mProjVolHalf.getHeight();
        bool need_clear = false;
        for (int k = 0; k < 2; ++k)
        {
            if (mProjVolHistory[k].getWidth() != hw || mProjVolHistory[k].getHeight() != hh)
            {
                mProjVolHistory[k].release();
                if (!mProjVolHistory[k].allocate(hw, hh, GL_RGBA16F))
                    temporal = false;
                else
                    need_clear = true;
            }
        }
        if (temporal && need_clear)
        {
            mProjVolHistoryValid = false; // freshly (re)allocated -> no valid prev
            LLGLDisable no_scissor(GL_SCISSOR_TEST);
            for (int k = 0; k < 2; ++k)
            {
                mProjVolHistory[k].bindTarget();
                glClearColor(0.f, 0.f, 0.f, 0.f);
                mProjVolHistory[k].clear(GL_COLOR_BUFFER_BIT);
                mProjVolHistory[k].flush();
            }
        }
    }
    if (!temporal)
    {
        mProjVolHistoryValid = false; // temporal off -> don't reproject next frame
    }

    LLRenderTarget* march_target = halfres ? &mProjVolHalf : target;

    // Scissor + sphere projection work in the MARCH target's pixel space so the
    // half-res path scissors correctly. LIGHT_CENTER etc are view-space and thus
    // resolution independent.
    const F32 tgt_w = (F32)march_target->getWidth();
    const F32 tgt_h = (F32)march_target->getHeight();

    march_target->bindTarget();
    if (halfres)
    {
        // Additive accumulation needs a black base in the half-res target (the
        // fullscreen path accumulates directly onto the existing scene instead).
        // Disable scissor for the clear so a stale rect can't leave last frame's
        // shaft in the border and let it accumulate.
        LLGLDisable no_scissor(GL_SCISSOR_TEST);
        gGL.setColorMask(true, true); // alpha moment must start at exactly zero
        glClearColor(0.f, 0.f, 0.f, 0.f);
        march_target->clear(GL_COLOR_BUFFER_BIT);
    }

    LLGLDepthTest depth(GL_FALSE);
    LLGLEnable    blend(GL_BLEND);
    // [Phase 1 item 4] per-cone screen scissor; enabled only if requested, reset
    // to the full target for each cone that cannot be safely bounded.
    LLGLEnable    scissor_test(BDMergeProjectorVolumetricsScissor ? GL_SCISSOR_TEST : GL_NONE);
    gGL.setSceneBlendType(LLRender::BT_ADD); // GL_ONE, GL_ONE - additive accumulation over slots
    // Only the half-res temporal beam-depth path transports an additive first
    // distance moment in alpha. Direct scene / non-temporal paths stay rgb-only.
    gGL.setColorMask(true, temporal && BDMergeProjectorVolumetricsTemporalBeamDepth);

    // [Phase 1 item 3] union of all marched cone rects (march-target pixel space)
    // so the upsample pass only touches pixels a shaft could have reached.
    F32  union_min_x = tgt_w, union_min_y = tgt_h, union_max_x = 0.f, union_max_y = 0.f;
    bool union_full  = false; // a cone needed the full-screen fallback
    U32  cones_drawn = 0;

    // [BDMerge G3.3 Dust] Lazy-load the baked dust volume the first frame the
    // lever is on (a no-op afterwards; one disk attempt per GL context). Done
    // BEFORE binding the march program because the loader parks on texture
    // unit 0, which bindDeferredShader is about to reassign anyway.
    if (BDMergeProjectorVolumetricsDust)
    {
        loadProjVolDustMap();
    }

    bindDeferredShader(gDeferredProjectorVolumetricProgram); // binds the full per-slot shadow set

    // Shared (per-frame) uniforms - GODRAY_RES is uploaded per cone below because
    // item 5 scales it adaptively. [Batch 1 C] GODRAY_MULTIPLIER / PROJVOL_FEATHER /
    // PROJVOL_G / PROJVOL_DENSITY moved to per-cone uploads so a projector's
    // per-UUID override can replace them independently of the globals.
    gDeferredProjectorVolumetricProgram.uniform1i(LLShaderMgr::PROJVOL_SHADOW_SAMPLES, (S32)llclamp(BDMergeProjectorVolumetricsShadowSamples, (U32)1, (U32)4));
    gDeferredProjectorVolumetricProgram.uniform1i(LLShaderMgr::PROJVOL_DITHER, (S32)llclamp(BDMergeProjectorVolumetricsDither, (U32)0, (U32)2));
    gDeferredProjectorVolumetricProgram.uniform1f(LLShaderMgr::PROJVOL_FRAME, (F32)(LLFrameTimer::getFrameCount() % 1024u));
    gDeferredProjectorVolumetricProgram.uniform1f(LLShaderMgr::PROJVOL_MAX, BDMergeProjectorVolumetricsMaxLuminance);
    // [BDMerge G3.3 Batch A] look-neutral performance gates. E1 frustum clip
    // (default on) refines [t0,t1] against the projector's frustum (the
    // per-cone planes are uploaded in setupSpotLightVolumetric); E2 replaces the
    // shadow sub-tap loop with one IGN-jittered tap. E4 (luminance early-out) needs
    // no uniform - it is provably invisible via the existing projvol_max clamp.
    gDeferredProjectorVolumetricProgram.uniform1i(LLShaderMgr::PROJVOL_FRUSTUM_CLIP, BDMergeProjectorVolumetricsFrustumClip ? 1 : 0);
    gDeferredProjectorVolumetricProgram.uniform1i(LLShaderMgr::PROJVOL_SHADOW_JITTER_TAP, BDMergeProjectorVolumetricsShadowJitterTap ? 1 : 0);
    // [BDMerge G3.3 Batch B - R1] Beam-depth reprojection: the march writes its
    // luminance-premultiplied first distance moment to ALPHA only when this is 1.
    // Upload 1 ONLY on the temporal path (which requires halfres, so the march writes
    // to mProjVolHalf - never the additive scene buffer). On the direct/non-halfres
    // path `temporal` is false here, so the gate stays 0 and alpha stays 0 - the
    // additive composite is never corrupted and the output is byte-identical.
    gDeferredProjectorVolumetricProgram.uniform1i(LLShaderMgr::PROJVOL_TEMPORAL_BEAM_DEPTH, (temporal && BDMergeProjectorVolumetricsTemporalBeamDepth) ? 1 : 0);
    // [Batch 1 B] gobo-colored occluder shadows: 0 = classic hard black shadow
    // (the shipped look), >0 lets occluded march samples carry a dimmed, gobo-shaped
    // colored contribution ("stained glass" banding) instead of pure black.
    gDeferredProjectorVolumetricProgram.uniform1f(LLShaderMgr::PROJVOL_SHADOW_TINT, llclamp(BDMergeProjectorVolumetricsShadowTint, 0.f, 1.f));

    // [BDMerge G3.3 Rim] Physical surface-coupled rim / wrap glow. Driven entirely
    // by this projector's own light at the surface (cookie x atten x its shadow map)
    // and the real G-buffer normal, added into the additive HDR shaft so it rides
    // the existing bloom-feed into a soft halo. Strength 0 (default) = no-op.
    // [F4] Rim STRENGTH/POWER/WRAP/SOFTNESS are now uploaded PER-CONE (override-aware)
    // in the cone loop below; only the quality-gate THRESHOLD stays global here.
    gDeferredProjectorVolumetricProgram.uniform1f(LLShaderMgr::PROJVOL_RIM_THRESHOLD, llmax(BDMergeProjectorVolumetricsRimThreshold, 0.f));

    // [Phase 3] atmosphere levers (all no-ops at their defaults). The inverse
    // modelview turns a view-space march sample back into agent(world, Z-up) space
    // so the noise medium is world-anchored (drifts, never crawls) and the height
    // fog keys off true altitude. projvol_time is continuous seconds (wrapped) for a
    // smooth noise scroll - NOT the wrapped frame counter used by the dither.
    glm::mat4 inv_mv = glm::inverse(mat);
    gDeferredProjectorVolumetricProgram.uniformMatrix4fv(LLShaderMgr::PROJVOL_INV_MODELVIEW, 1, false, glm::value_ptr(inv_mv));
    gDeferredProjectorVolumetricProgram.uniform1f(LLShaderMgr::PROJVOL_TIME, fmodf(gFrameTimeSeconds, 3600.f));
    // [Batch 1 C] PROJVOL_DENSITY moved to the per-cone upload (overridable).
    // [F4 flattening REMOVED] F4 zeroed the per-cone noise/fog (and forced density 1)
    // whenever the froxel master was on, to prevent double-fog. In practice it stole
    // the beam-dust look entirely (in-world report: "can't enable the dust" - the
    // user's beams live on per-cone noise). Per-cone media levers now work regardless
    // of froxel; whether beams carry their own dust on top of the shared air is the
    // artist's call, not the renderer's.
    gDeferredProjectorVolumetricProgram.uniform1f(LLShaderMgr::PROJVOL_NOISE_STRENGTH, llclamp(BDMergeProjectorVolumetricsNoiseStrength, 0.f, 1.f));
    gDeferredProjectorVolumetricProgram.uniform1f(LLShaderMgr::PROJVOL_NOISE_SCALE, llmax(BDMergeProjectorVolumetricsNoiseScale, 0.f));
    gDeferredProjectorVolumetricProgram.uniform1f(LLShaderMgr::PROJVOL_NOISE_SPEED, BDMergeProjectorVolumetricsNoiseSpeed);
    // [F5 follow-up] Shared dust-wind direction for the BEAM dust (same
    // BDMergeFroxelWind* trio the froxel air uses - one coherent air), scaled by the
    // per-cone NoiseSpeed so beams keep their own speed/reverse control. Replaces
    // the old hardwired (1,1,1) diagonal drift; steer it from the Lightbox Froxel
    // tab's Wind sliders (they apply to beam dust whether or not froxel is on).
    {
        F32 wdir[3];
        bdmerge_dust_wind_dir(wdir); // unit dir from Azimuth/Elevation/Inverted
        const F32 s = BDMergeProjectorVolumetricsNoiseSpeed;
        gDeferredProjectorVolumetricProgram.uniform3f(LLShaderMgr::PROJVOL_WIND, wdir[0] * s, wdir[1] * s, wdir[2] * s);
    }
    gDeferredProjectorVolumetricProgram.uniform1f(LLShaderMgr::PROJVOL_FOG_STRENGTH, llclamp(BDMergeProjectorVolumetricsFogStrength, 0.f, 1.f));
    gDeferredProjectorVolumetricProgram.uniform1f(LLShaderMgr::PROJVOL_FOG_GROUND, llmax(BDMergeProjectorVolumetricsFogGroundDensity, 0.f));
    gDeferredProjectorVolumetricProgram.uniform1f(LLShaderMgr::PROJVOL_FOG_FALLOFF, llmax(BDMergeProjectorVolumetricsFogFalloff, 0.01f));
    gDeferredProjectorVolumetricProgram.uniform1f(LLShaderMgr::PROJVOL_FOG_BASE, BDMergeProjectorVolumetricsFogBase);

    // [BDMerge G3.3 ConservativeShadow] Airborne-march shadow sampler A/B gate.
    // [Review fix] OPT-IN, default OFF: the conservative sampler replaces the
    // wide 12/Vogel-tap penumbra of EVERY marching projector with a tight 5-tap
    // kernel, and the hero-shaft root cause is UNCONFIRMED - so it must not be a
    // default-on global change. OFF (default) = the legacy lit-surface sampler,
    // byte-identical to the shipped look for all normal soft-shadow shafts.
    // ON (hero-shaft experiment) = the conservative volume sampler in
    // shadowUtil.glsl plus the shader's on-axis numerical guards, for in-world
    // A/B of the shaft leaking through an occluder's center.
    // [Round-2 fix] Conservative-shadow is now a compile-time PERMUTATION
    // (PROJVOL_CONSERVATIVE_SHADOW, mirroring the Dust permutation below): with
    // the lever OFF the uniform and every conservative branch are compiled OUT,
    // so the default-off march is instruction-identical to the legacy path, not
    // merely equivalent. Only upload the runtime gate when the lever is on (the
    // uniform does not exist in the OFF program). An ON-permutation program
    // whose gate somehow went un-uploaded reads the GL default 0 and falls back
    // to the legacy branch - fail-safe across the toggle/rebuild window.
    if (BDMergeProjectorVolumetricsConservativeShadow)
    {
        gDeferredProjectorVolumetricProgram.uniform1i(LLShaderMgr::PROJVOL_CONSERVATIVE_SHADOW, 1);
    }

    // [BDMerge G3.3 Dust] Baked 64^3 dust-volume particulate breakup. The shader
    // gate (projvol_dust) is only raised while the volume is actually live in GL
    // AND verifiably bound this pass, so a missing/corrupt asset - or a failed
    // channel/bind - degrades to exactly the shipped look instead of sampling an
    // unbound unit into the density. Wind: the same shared
    // Azimuth/Elevation/Inverted air direction the fbm beam noise drifts with
    // (bdmerge_dust_wind_dir - one coherent air), scaled by DustDrift (m/s) and
    // folded on the CPU; the shader advects world-space metres with projvol_time
    // (continuous seconds, uploaded above), matching the asset's
    // sampleDustVolume3D reference helper units.
    //
    // [Review fix - sampler slot] Dust is now a compile-time PERMUTATION of
    // projectorVolumetricF.glsl (PROJVOL_DUST_ENABLE, keyed off the same setting
    // in llviewershadermgr.cpp; toggling it rebuilds shaders via
    // handleSetShaderChanged). With Dust OFF the program contains no sampler3D at
    // all - no fragment texture unit is consumed - and everything below except
    // the one gate upload (a no-op on the compiled-out uniform) is skipped, so
    // dust-off costs neither a sampler slot nor per-pass uniform work.
    //
    // [Review fix - Inf/NaN] The three dust levers are persisted F32s: sanitize
    // to finite, documented ranges on upload so an extreme/corrupt value can
    // never reach the shader (wind*time or world*scale -> Inf -> fract(Inf) =
    // NaN poisoning the march, unbounded intensity -> unbounded density).
    F32 dust_intensity = BDMergeProjectorVolumetricsDustIntensity;
    // [Round-2 fix] cap = 2.0, matching the declared settings.xml range (0.0-2.0)
    // and the shader's documented ~0..2 domain - the code used to allow 4.0,
    // letting a hand-edited settings file push past the tuned range.
    dust_intensity = llfinite(dust_intensity) ? llclamp(dust_intensity, 0.f, 2.f) : 0.f;
    bool dust_on = BDMergeProjectorVolumetricsDust &&
                   mProjVolDustMap != 0 &&
                   dust_intensity > 0.f;
    if (dust_on)
    {
        // Filtering/wrap (LINEAR + REPEAT) are texture-object state set at load.
        // Drain stale errors BEFORE enableTexture(), because it activates the
        // sampler's unit and updates the cached active-unit index even when that
        // activation fails. The checks below must attribute that error to this
        // dust bind instead of letting bindManual() trust a stale cache entry.
        for (S32 drain = 0; drain < 8 && glGetError() != GL_NO_ERROR; ++drain)
        {
        }
        S32 dust_channel = gDeferredProjectorVolumetricProgram.enableTexture(LLShaderMgr::PROJVOL_DUST_MAP, LLTexUnit::TT_TEXTURE_3D);
        const GLenum activation_err = glGetError();
        bool bind_failed = activation_err != GL_NO_ERROR;
        if (!bind_failed && dust_channel < 0)
        {
            // Sampler not present in this program (permutation off / rebuild in
            // flight): gate the effect off this pass but KEEP the loaded volume -
            // the rebuilt shader picks it up next frame.
            dust_on = false;
        }
        else if (!bind_failed)
        {
            // [Round-2 fix] bindManual() == true is NOT a successful GL bind: it
            // returns false only for an invalid texture-unit index and never
            // checks GL errors after glBindTexture (llrender.cpp). Verify with an
            // explicit glGetError(): an invalidated texture name or driver bind
            // error must never leave dust_on true sampling an unbound/incomplete
            // texture.
            bool bound = gGL.getTexUnit(dust_channel)->bindManual(LLTexUnit::TT_TEXTURE_3D, mProjVolDustMap);
            const GLenum bind_err = glGetError();
            bind_failed = !bound || bind_err != GL_NO_ERROR;
        }
        if (bind_failed)
        {
            // Activation or bind failure: never march with the gate up over a dead
            // sampler. Release the handle so every later dust_on test is false
            // (clean OFF until the GL context is rebuilt; the loader is
            // one-attempt-per-context by design). disableTexture first so the
            // unit's currency cache cannot keep pointing at the deleted name.
            LL_WARNS_ONCE() << "[BDMerge Dust] dust volume bind failed - disabling dust" << LL_ENDL;
            gDeferredProjectorVolumetricProgram.disableTexture(LLShaderMgr::PROJVOL_DUST_MAP, LLTexUnit::TT_TEXTURE_3D);
            LLImageGL::deleteTextures(1, &mProjVolDustMap);
            mProjVolDustMap = 0;
            dust_on = false;
        }
    }
    gDeferredProjectorVolumetricProgram.uniform1i(LLShaderMgr::PROJVOL_DUST, dust_on ? 1 : 0);
    if (dust_on)
    {
        gDeferredProjectorVolumetricProgram.uniform1f(LLShaderMgr::PROJVOL_DUST_INTENSITY, dust_intensity);

        F32 dust_scale = BDMergeProjectorVolumetricsDustScale;
        dust_scale = llfinite(dust_scale) ? llclamp(dust_scale, 0.f, 64.f) : 0.5f;
        gDeferredProjectorVolumetricProgram.uniform1f(LLShaderMgr::PROJVOL_DUST_SCALE, dust_scale);

        F32 drift = BDMergeProjectorVolumetricsDustDrift;
        drift = llfinite(drift) ? llclamp(drift, -10.f, 10.f) : 0.f;
        F32 wdir[3];
        bdmerge_dust_wind_dir(wdir); // unit dir from Azimuth/Elevation/Inverted
        F32 wx = wdir[0] * drift;
        F32 wy = wdir[1] * drift;
        F32 wz = wdir[2] * drift;
        if (!llfinite(wx) || !llfinite(wy) || !llfinite(wz))
        {
            // The direction comes from sinf/cosf of the shared (persisted) froxel
            // wind Azimuth/Elevation - a corrupt value there is NaN through a
            // different door. Frozen dust beats a NaN-poisoned march.
            wx = wy = wz = 0.f;
        }
        gDeferredProjectorVolumetricProgram.uniform3f(LLShaderMgr::PROJVOL_DUST_WIND, wx, wy, wz);
    }

    gDeferredProjectorVolumetricProgram.enableTexture(LLShaderMgr::DEFERRED_PROJECTION);

    mScreenTriangleVB->setBuffer();

    const U32 max_res = llclamp(BDMergeProjectorVolumetricsResolution, (U32)4, (U32)64);
    const U32 min_res = llclamp(BDMergeProjectorVolumetricsMinResolution, (U32)4, max_res);

    // Shadow-casting projectors only (slots 0..N-1). mShadowSpotLight[] is
    // populated during generateSunShadow earlier this frame and valid here.
    for (U32 i = 0; i < bdmergeMaxSpotShadows(); ++i)
    {
        LLDrawable* drawablep = mShadowSpotLight[i];
        if (drawablep == NULL)
        {
            continue;
        }
        // Skip slots whose shadow map was never allocated this frame.
        LLRenderTarget* shadow_target = getSpotShadowTarget(i);
        if (shadow_target == NULL || shadow_target->getWidth() == 0)
        {
            continue;
        }
        LLVOVolume* volume = drawablep->getVOVolume();
        if (volume == NULL)
        {
            continue;
        }

        // [Phase 2 item 1] Session-only art-direction filter: a projector emits
        // a shaft only after its object UUID has been opted in this session via
        // the right-click "Volumetric Shaft" toggle. Empty set => nothing marches
        // (off by default). The context menu flags root prims, but the light
        // feature can live on either the root or a child, so match both the
        // light-source prim's own ID and its root-edit ID.
        LLUUID matched_id;
        {
            if (isVolumetricShaftEnabled(volume->getID()))
            {
                matched_id = volume->getID();
            }
            else if (LLViewerObject* root = volume->getRootEdit())
            {
                if (isVolumetricShaftEnabled(root->getID()))
                    matched_id = root->getID();
            }
            if (matched_id.isNull())
            {
                continue;
            }
        }

        // [Batch 1 C / F4] Per-projector override, resolved UP FRONT: the rim-only
        // demotion below needs the EFFECTIVE (override-aware) rim strength, and the
        // rim uniforms are now uploaded per-cone (see [F4] below). If this flagged
        // projector has a captured override, use its values in place of the global
        // sliders for this cone; otherwise fall back to the globals. Overridable
        // levers: brightness, feather, forward-glow (g), density, tint + tint
        // strength, and [F4] rim strength/power/wrap/softness (RimThreshold stays
        // global - a quality gate, not a per-light art lever).
        VolumetricShaftOverride ov;
        const bool has_ov = getVolumetricShaftOverride(matched_id, ov);
        const F32 e_rimStrength = has_ov ? ov.rimStrength : BDMergeProjectorVolumetricsRimStrength;
        const F32 e_rimPower    = has_ov ? ov.rimPower    : BDMergeProjectorVolumetricsRimPower;
        const F32 e_rimWrap     = has_ov ? ov.rimWrap     : BDMergeProjectorVolumetricsRimWrap;
        const F32 e_rimSoftness = has_ov ? ov.rimSoftness : BDMergeProjectorVolumetricsRimSoftness;

        // [BDMerge Froxel F2] A projector injected into the froxel light grid this
        // frame lights via the grid (soft, volumetric) - marching its shaft here too
        // would double-light. BUT the surface-coupled RIM/wrap glow (and the
        // bloom-feed halo it rides into) is a per-cone SURFACE effect the grid can't
        // reproduce, so instead of skipping the cone entirely we demote it to a
        // RIM-ONLY pass: GODRAY_RES is forced to 0 below (march loop never runs,
        // shaft = 0) and only the rim block evaluates at the capped surface. If the
        // rim is off there is nothing per-cone left to draw - skip. [F4] The test now
        // uses the EFFECTIVE rim strength so a per-light rim=0 skips cleanly even when
        // the global is up, and a per-light rim>0 still draws when the global is 0.
        // (Hero-flagged projectors are never injected -> never rim-only -> they march
        // fully here.) The injected set is empty whenever the froxel master or lights
        // lever is off, so the per-cone path is unchanged in that case.
        const bool froxel_rim_only = BDMergeFroxelVolumetrics && BDMergeFroxelLights &&
            mFroxelInjectedProjectors.count(matched_id) != 0;
        if (froxel_rim_only && e_rimStrength <= 0.f)
        {
            continue;
        }

        // Side-effect-free geometry + cookie upload; slot passed in directly
        // (NO mTargetShadowSpotLight priority reshuffle - R1).
        setupSpotLightVolumetric(gDeferredProjectorVolumetricProgram, drawablep, (S32)i);

        // Remaining per-cone override levers (brightness/feather/g/density/tint).
        const F32 e_mult     = has_ov ? ov.multiplier   : BDMergeProjectorVolumetricsMultiplier;
        const F32 e_feather  = has_ov ? ov.feather      : BDMergeProjectorVolumetricsFeather;
        const F32 e_g        = has_ov ? ov.anisotropy   : BDMergeProjectorVolumetricsAnisotropy;
        const F32 e_density  = has_ov ? ov.density       : BDMergeProjectorVolumetricsDensity;
        const LLColor3 e_tint = has_ov ? ov.tint        : BDMergeProjectorVolumetricsTint;
        const F32 e_tintStr  = has_ov ? ov.tintStrength : BDMergeProjectorVolumetricsTintStrength;

        gDeferredProjectorVolumetricProgram.uniform1f(LLShaderMgr::GODRAY_MULTIPLIER, e_mult);
        gDeferredProjectorVolumetricProgram.uniform1f(LLShaderMgr::PROJVOL_FEATHER, e_feather);
        gDeferredProjectorVolumetricProgram.uniform1f(LLShaderMgr::PROJVOL_G, e_g);
        // [BDMerge F4] Per-cone media flattening when the froxel master is ON: the
        // shared atmosphere (density/height-fog/noise) now lives in the froxel grid,
        // so a per-cone march (hero cone or rim-only cone) marching its OWN fog on top
        // would double-fog. Upload a FLAT medium (density 1, noise/fog strengths 0 -
        // the latter two are the global uploads gated the same way above) regardless
        // of the per-cone media sliders/overrides. When froxel is OFF this is exactly
        // the old per-cone density path (override-aware), byte-identical.
        // [F4 flattening REMOVED] density lever live regardless of froxel (see the
        // noise/fog note at the per-frame uploads above).
        gDeferredProjectorVolumetricProgram.uniform1f(LLShaderMgr::PROJVOL_DENSITY, llmax(e_density, 0.f));

        // [BDMerge F4] Per-projector RIM overrides, uploaded per-cone (moved from the
        // global pre-loop block). Effective values were resolved up front so the
        // rim-only demotion could consult the strength. Identical to the global path
        // when no override is captured -> default behavior unchanged.
        gDeferredProjectorVolumetricProgram.uniform1f(LLShaderMgr::PROJVOL_RIM_STRENGTH, llmax(e_rimStrength, 0.f));
        gDeferredProjectorVolumetricProgram.uniform1f(LLShaderMgr::PROJVOL_RIM_POWER, llmax(e_rimPower, 0.01f));
        gDeferredProjectorVolumetricProgram.uniform1f(LLShaderMgr::PROJVOL_RIM_WRAP, llclamp(e_rimWrap, 0.f, 1.f));
        gDeferredProjectorVolumetricProgram.uniform1f(LLShaderMgr::PROJVOL_RIM_SOFTNESS, llclamp(e_rimSoftness, 0.f, 1.f));

        LLColor3  col = volume->getLightLinearColor() * light_scale;
        // [Phase 2 item 2 / Batch 1 C] Shaft tint (global or per-projector override):
        // pull the shaft color toward the art-direction tint so it can differ from
        // the light's own color. At TintStrength 0 (default) this is a no-op and
        // shafts carry pure light color per the locked design.
        if (e_tintStr > 0.f)
        {
            const F32 t = llclamp(e_tintStr, 0.f, 1.f);
            col = col * (1.f - t) + e_tint * (light_scale * t);
        }
        glm::vec3 c(drawablep->getPositionAgent());
        c = mul_mat4_vec3(mat, c); // agent -> view space

        // Sphere of influence radius, matching the shader's LIGHT_SIZE.
        const F32 radius = volume->getLightRadius() * 1.5f;

        // ---- [Phase 1 items 4/5] project the sphere to screen space ----------
        // Conservative screen AABB from the 8 corners of the view-space bounding
        // box (c +/- radius). Perspective projection preserves containment for
        // points in front of the camera, so min/max of the projected corners
        // bounds the projected sphere silhouette. If any corner falls at/behind
        // the camera plane the bound is unreliable -> fall back to full screen.
        F32  min_x = tgt_w, min_y = tgt_h, max_x = 0.f, max_y = 0.f;
        bool bounds_valid = true;
        for (S32 cx = -1; cx <= 1 && bounds_valid; cx += 2)
        for (S32 cy = -1; cy <= 1 && bounds_valid; cy += 2)
        for (S32 cz = -1; cz <= 1 && bounds_valid; cz += 2)
        {
            glm::vec4 corner(c.x + cx * radius, c.y + cy * radius, c.z + cz * radius, 1.f);
            glm::vec4 clip = proj * corner;
            if (clip.w <= 0.0001f) // corner at/behind camera -> silhouette not bounded
            {
                bounds_valid = false;
                break;
            }
            F32 px = (clip.x / clip.w * 0.5f + 0.5f) * tgt_w;
            F32 py = (clip.y / clip.w * 0.5f + 0.5f) * tgt_h;
            min_x = llmin(min_x, px); max_x = llmax(max_x, px);
            min_y = llmin(min_y, py); max_y = llmax(max_y, py);
        }

        if (bounds_valid)
        {
            min_x = llclamp(min_x, 0.f, tgt_w); max_x = llclamp(max_x, 0.f, tgt_w);
            min_y = llclamp(min_y, 0.f, tgt_h); max_y = llclamp(max_y, 0.f, tgt_h);
            // Cone projects entirely off screen -> nothing to march.
            if (max_x <= min_x || max_y <= min_y)
            {
                continue;
            }
        }
        else
        {
            min_x = 0.f; min_y = 0.f; max_x = tgt_w; max_y = tgt_h;
            union_full = true; // this cone couldn't be bounded -> upsample full screen
        }

        // Grow the upsample union (march-target pixel space).
        union_min_x = llmin(union_min_x, min_x); union_max_x = llmax(union_max_x, max_x);
        union_min_y = llmin(union_min_y, min_y); union_max_y = llmax(union_max_y, max_y);
        ++cones_drawn;

        if (BDMergeProjectorVolumetricsScissor)
        {
            glScissor((GLint)min_x, (GLint)min_y,
                      (GLsizei)llceil(max_x - min_x), (GLsizei)llceil(max_y - min_y));
        }

        // [Phase 1 item 5] adaptive sample count: scale by the on-screen coverage
        // of the cone (fraction of screen height its projected box spans). A cone
        // filling the view marches the full Resolution; a small/distant one drops
        // toward the floor. Bounded march means even the floor stays clean.
        U32 cone_res = max_res;
        if (BDMergeProjectorVolumetricsAdaptive)
        {
            F32 coverage = bounds_valid ? llclamp((max_y - min_y) / llmax(tgt_h, 1.f), 0.f, 1.f) : 1.f;
            // sqrt gives small cones a fairer share than raw linear coverage.
            F32 scaled   = (F32)min_res + (F32)(max_res - min_res) * sqrtf(coverage);
            cone_res     = llclamp((U32)(scaled + 0.5f), min_res, max_res);
        }
        // [BDMerge Froxel F2] Rim-only demotion: res 0 skips the march loop entirely
        // (shaft = 0; the shader's dt guard handles the division) so only the surface
        // rim evaluates - the beam itself lives in the froxel grid.
        gDeferredProjectorVolumetricProgram.uniform1i(LLShaderMgr::GODRAY_RES, froxel_rim_only ? 0 : (S32)cone_res);

        gDeferredProjectorVolumetricProgram.uniform3fv(LLShaderMgr::LIGHT_CENTER, 1, glm::value_ptr(c));
        gDeferredProjectorVolumetricProgram.uniform1f(LLShaderMgr::LIGHT_SIZE, radius);
        gDeferredProjectorVolumetricProgram.uniform3fv(LLShaderMgr::DIFFUSE_COLOR, 1, col.mV);
        gDeferredProjectorVolumetricProgram.uniform1f(LLShaderMgr::LIGHT_FALLOFF, volume->getLightFalloff(DEFERRED_LIGHT_FALLOFF));

        mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
    }

    // Restore full-screen scissor for anything downstream that assumes it (the
    // LLGLEnable above only toggles the enable bit, not the rectangle).
    if (BDMergeProjectorVolumetricsScissor)
    {
        glScissor(0, 0, (GLsizei)march_target->getWidth(), (GLsizei)march_target->getHeight());
    }

    if (dust_on)
    {
        gDeferredProjectorVolumetricProgram.disableTexture(LLShaderMgr::PROJVOL_DUST_MAP, LLTexUnit::TT_TEXTURE_3D);
    }
    gDeferredProjectorVolumetricProgram.disableTexture(LLShaderMgr::DEFERRED_PROJECTION);
    unbindDeferredShader(gDeferredProjectorVolumetricProgram);

    march_target->flush();

    // The upsample + bloom passes sample this frame's shaft from here. Default to
    // the freshly-marched half-res target; the temporal resolve below repoints it
    // at the accumulated (denoised) history slot when temporal is active.
    mProjVolShaftSrc = &mProjVolHalf;

    // [Batch 1 A] Temporal reprojection resolve (half-res). Blend this frame's raw
    // shaft with the reprojected previous accumulation - neighborhood-clamped and
    // depth/camera-cut rejected so it can't ghost or smear - into the current
    // history slot, which then becomes the shaft source for the upsample + bloom.
    if (halfres && cones_drawn > 0 && temporal)
    {
        const U32 cur  = mProjVolHistoryIdx & 1u;
        const U32 prev = cur ^ 1u;
        LLRenderTarget& dst      = mProjVolHistory[cur];
        LLRenderTarget& histprev = mProjVolHistory[prev];
        const glm::mat4 viewproj = proj * mat;

        dst.bindTarget();
        {
            LLGLDisable no_scissor(GL_SCISSOR_TEST);
            LLGLDisable no_blend(GL_BLEND);           // full overwrite of the slot
            gGL.setColorMask(true, true);

            bindDeferredShader(gDeferredProjectorVolumetricTemporalProgram);

            // Current raw half-res shaft -> projectionMap. POINT sampled so the
            // neighborhood min/max clamp reads exact texels.
            S32 cur_ch = gDeferredProjectorVolumetricTemporalProgram.enableTexture(LLShaderMgr::DEFERRED_PROJECTION);
            if (cur_ch > -1)
            {
                mProjVolHalf.bindTexture(0, cur_ch, LLTexUnit::TFO_POINT);
            }
            // Previous accumulation -> projvol_history. BILINEAR for reprojection.
            S32 hist_ch = gDeferredProjectorVolumetricTemporalProgram.enableTexture(LLShaderMgr::PROJVOL_HISTORY);
            if (hist_ch > -1)
            {
                histprev.bindTexture(0, hist_ch, LLTexUnit::TFO_BILINEAR);
            }

            const F32 half_res[2] = { (F32)mProjVolHalf.getWidth(), (F32)mProjVolHalf.getHeight() };
            static const LLStaticHashedString sHalfResT("projvol_half_res");
            gDeferredProjectorVolumetricTemporalProgram.uniform2fv(sHalfResT, 1, half_res);

            // view -> agent(world) for reprojection (same inverse-modelview as march).
            gDeferredProjectorVolumetricTemporalProgram.uniformMatrix4fv(LLShaderMgr::PROJVOL_INV_MODELVIEW, 1, false, glm::value_ptr(inv_mv));
            // Previous frame's world -> clip matrix (used to find each current
            // sample's screen position last frame). Meaningless until history valid,
            // so the EMA weight is forced to 0 that first frame.
            gDeferredProjectorVolumetricTemporalProgram.uniformMatrix4fv(LLShaderMgr::PROJVOL_PREV_VIEWPROJ, 1, false, mProjVolPrevViewProj);
            // Current world points must also be measured in the previous camera's
            // view space for beam-depth disocclusion comparisons.
            gDeferredProjectorVolumetricTemporalProgram.uniformMatrix4fv(LLShaderMgr::PROJVOL_PREV_MODELVIEW, 1, false, mProjVolPrevModelview);

            // Unconditional hard-cut fail-safe: use the velocity pass's >=10%
            // relative-Frobenius discontinuity test, here on projector-volumetric
            // view-projection. A cut gets one current-only resolve, then becomes
            // the valid history endpoint for the following frame.
            bool projection_discontinuity = false;
            if (mProjVolHistoryValid)
            {
                const F32* current_viewproj = glm::value_ptr(viewproj);
                F32 delta_squared = 0.f;
                F32 magnitude_squared = 0.f;
                for (U32 i = 0; i < 16; ++i)
                {
                    const F32 delta = current_viewproj[i] - mProjVolPrevViewProj[i];
                    delta_squared += delta * delta;
                    magnitude_squared += llmax(current_viewproj[i] * current_viewproj[i],
                                               mProjVolPrevViewProj[i] * mProjVolPrevViewProj[i]);
                }
                projection_discontinuity =
                    delta_squared > 0.01f * llmax(magnitude_squared, 0.000001f);
            }
            const F32 blend = (mProjVolHistoryValid && !projection_discontinuity)
                            ? llclamp(BDMergeProjectorVolumetricsTemporalBlend, 0.f, 0.98f)
                            : 0.f;
            gDeferredProjectorVolumetricTemporalProgram.uniform1f(LLShaderMgr::PROJVOL_TEMPORAL_BLEND, blend);
            // [BDMerge G3.3 Batch B] R2 contrast-aware reject (0 = no-op) + R1 beam-depth
            // reprojection gate (must match the march-side upload above: on this path
            // `temporal` is true, so the setting alone selects it, exactly like the march).
            gDeferredProjectorVolumetricTemporalProgram.uniform1f(LLShaderMgr::PROJVOL_TEMPORAL_REJECT, llmax(BDMergeProjectorVolumetricsTemporalReject, 0.f));
            gDeferredProjectorVolumetricTemporalProgram.uniform1i(LLShaderMgr::PROJVOL_TEMPORAL_BEAM_DEPTH, BDMergeProjectorVolumetricsTemporalBeamDepth ? 1 : 0);

            mScreenTriangleVB->setBuffer();
            mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

            gDeferredProjectorVolumetricTemporalProgram.disableTexture(LLShaderMgr::PROJVOL_HISTORY);
            gDeferredProjectorVolumetricTemporalProgram.disableTexture(LLShaderMgr::DEFERRED_PROJECTION);
            unbindDeferredShader(gDeferredProjectorVolumetricTemporalProgram);
        }
        dst.flush();

        // Persist this frame's world->clip for next frame's reprojection; advance
        // the ping-pong (next frame reads the slot we just wrote); the resolved slot
        // is now the shaft source; history is valid from here on.
        memcpy(mProjVolPrevViewProj, glm::value_ptr(viewproj), sizeof(mProjVolPrevViewProj));
        memcpy(mProjVolPrevModelview, glm::value_ptr(mat), sizeof(mProjVolPrevModelview));
        mProjVolShaftSrc     = &dst;
        mProjVolHistoryIdx   = prev;
        mProjVolHistoryValid = true;

        // The temporal pass wrote alpha (stored depth); restore the rgb-only mask
        // the additive upsample composite below expects.
        gGL.setColorMask(true, false);
    }
    else
    {
        // No temporal resolve this frame -> the stored history can't be reprojected
        // cleanly next frame; drop it so the next temporal frame restarts from black.
        mProjVolHistoryValid = false;
    }

    // [Phase 1 item 3] Resolve the (raw or temporally-accumulated) half-res shaft to
    // full resolution with a depth-aware bilateral upsample, compositing additively
    // onto the scene. Skip entirely if no cone marched (nothing to upsample).
    if (halfres && cones_drawn > 0)
    {
        target->bindTarget();

        // Scissor the upsample to the union of the marched cone rects, scaled from
        // half-res march space to full-res target space (2x). A cone that fell back
        // to full-screen forces a full-screen resolve. We force-enable scissor and
        // set an explicit rect either way so a stale half-res rect from the march
        // loop can never clip this full-res pass.
        bool up_scissor = BDMergeProjectorVolumetricsScissor && !union_full;
        LLGLEnable up_scissor_test(GL_SCISSOR_TEST);
        if (up_scissor)
        {
            F32 fx0 = llclamp(union_min_x * 2.f, 0.f, (F32)target->getWidth());
            F32 fy0 = llclamp(union_min_y * 2.f, 0.f, (F32)target->getHeight());
            F32 fx1 = llclamp(union_max_x * 2.f, 0.f, (F32)target->getWidth());
            F32 fy1 = llclamp(union_max_y * 2.f, 0.f, (F32)target->getHeight());
            glScissor((GLint)fx0, (GLint)fy0, (GLsizei)llceil(fx1 - fx0), (GLsizei)llceil(fy1 - fy0));
        }
        else
        {
            glScissor(0, 0, (GLsizei)target->getWidth(), (GLsizei)target->getHeight());
        }

        bindDeferredShader(gDeferredProjectorVolumetricUpsampleProgram);
        // Bind the half-res shaft to the reserved projectionMap sampler (unused by
        // bindDeferredShader) so it gets a proper texture channel; bilinear so the
        // shader's explicit 4-tap bilateral weighting reads clean texel centers.
        S32 half_ch = gDeferredProjectorVolumetricUpsampleProgram.enableTexture(LLShaderMgr::DEFERRED_PROJECTION);
        if (half_ch > -1)
        {
            mProjVolShaftSrc->bindTexture(0, half_ch, LLTexUnit::TFO_BILINEAR);
        }
        const F32 half_res[2] = { (F32)mProjVolShaftSrc->getWidth(), (F32)mProjVolShaftSrc->getHeight() };
        static const LLStaticHashedString sHalfRes("projvol_half_res");
        gDeferredProjectorVolumetricUpsampleProgram.uniform2fv(sHalfRes, 1, half_res);

        mScreenTriangleVB->setBuffer();
        mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

        // Restore full-screen scissor rect (the LLGLEnable only toggles the bit).
        glScissor(0, 0, (GLsizei)target->getWidth(), (GLsizei)target->getHeight());

        gDeferredProjectorVolumetricUpsampleProgram.disableTexture(LLShaderMgr::DEFERRED_PROJECTION);
        unbindDeferredShader(gDeferredProjectorVolumetricUpsampleProgram);

        target->flush();

        // [Phase 3 item 4] mProjVolHalf now holds this frame's shaft at half res:
        // mark it valid so feedProjectorVolumetricBloom (called after bloom is built)
        // may sample it. Only set on the half-res path with real cones drawn - the
        // fullscreen path composites in place and has no reusable shaft texture.
        mProjVolHalfValid = true;
    }

    gGL.setColorMask(true, true);
    gGL.setSceneBlendType(LLRender::BT_ALPHA);
}

// [BDMerge G3.3 Phase 3 item 4] Feed a controlled amount of the projector shaft
// into the HDR bloom pyramid so bright shaft cores gain a soft glow halo. Called
// from renderFinalize AFTER generateBloomHDR has built the pyramid from the CLEAN
// (pre-shaft) scene and AFTER renderProjectorVolumetric has composited the shaft
// and left it in mProjVolHalf. We add a separate, scaled, tent-blurred copy of the
// half-res shaft additively into bloomMip[0] - the buffer colorCorrect samples as
// the final bloom - so the scene bloom itself is untouched (no uncontrolled
// leakage) and only projvol_bloom_feed worth of shaft becomes a halo. Default feed
// 0 => early-out, Phase 1 bloom is byte-for-byte preserved. Requires the half-res
// path (the reusable shaft texture) and the HDR bloom pyramid.
void LLPipeline::feedProjectorVolumetricBloom()
{
    if (!BDMergeProjectorVolumetrics || gCubeSnapshot ||
        BDMergeProjectorVolumetricsBloomFeed <= 0.f ||
        !mProjVolHalfValid ||
        mProjVolShaftSrc == nullptr ||
        mRT->bloomMipCount < 1 ||
        mProjVolShaftSrc->getWidth() == 0 ||
        !gDeferredProjectorVolumetricBloomFeedProgram.isComplete())
    {
        return;
    }

    LL_PROFILE_GPU_ZONE("projvol bloom feed");

    LLGLDepthTest depth(GL_FALSE);
    LLGLEnable    blend(GL_BLEND);
    LLGLDisable   cull(GL_CULL_FACE);
    gGL.setSceneBlendType(LLRender::BT_ADD); // additive halo into the bloom base
    gGL.setColorMask(true, false);

    mRT->bloomMip[0].bindTarget();

    gDeferredProjectorVolumetricBloomFeedProgram.bind();
    // Half-res shaft -> diffuseMap (bilinear; the shader tent-blurs it into a halo).
    gDeferredProjectorVolumetricBloomFeedProgram.bindTexture(LLShaderMgr::DIFFUSE_MAP, mProjVolShaftSrc, false, LLTexUnit::TFO_BILINEAR);
    gDeferredProjectorVolumetricBloomFeedProgram.uniform2f(LLShaderMgr::BLOOM_TEXEL_SIZE,
        1.f / (F32)mProjVolShaftSrc->getWidth(), 1.f / (F32)mProjVolShaftSrc->getHeight());
    gDeferredProjectorVolumetricBloomFeedProgram.uniform1f(LLShaderMgr::PROJVOL_BLOOM_FEED, BDMergeProjectorVolumetricsBloomFeed);
    // [item C4] Anamorphic X-stretch of the tent taps (1.0 = exact isotropic tent, no-op).
    gDeferredProjectorVolumetricBloomFeedProgram.uniform1f(LLShaderMgr::PROJVOL_BLOOM_ANAMORPHIC, llmax(BDMergeProjectorVolumetricsBloomFeedAnamorphic, 1.f));

    mScreenTriangleVB->setBuffer();
    mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

    gDeferredProjectorVolumetricBloomFeedProgram.unbind();

    mRT->bloomMip[0].flush();

    gGL.setColorMask(true, true);
    gGL.setSceneBlendType(LLRender::BT_ALPHA);
}

// [BDMerge G3.3 Phase 2] Session-only per-projector volumetric opt-in. The set
// lives on gPipeline for the life of the process but is explicitly cleared on
// logout/relog (clearVolumetricShafts, from LLAppViewer::disconnectViewer), so
// flags never persist across a session or a viewer restart.
void LLPipeline::toggleVolumetricShaft(const LLUUID& id)
{
    if (id.isNull())
        return;
    auto it = sVolumetricShaftObjects.find(id);
    if (it != sVolumetricShaftObjects.end())
        sVolumetricShaftObjects.erase(it);
    else
        sVolumetricShaftObjects.insert(id);
}

bool LLPipeline::isVolumetricShaftEnabled(const LLUUID& id)
{
    return !sVolumetricShaftObjects.empty() && sVolumetricShaftObjects.count(id) != 0;
}

void LLPipeline::clearVolumetricShafts()
{
    sVolumetricShaftObjects.clear();
    sVolumetricShaftOverrides.clear();
    sNoShadowProjectors.clear(); // [BDMerge Batch 3] cast-shadows opt-out is session-only too
    sHeroProjectors.clear();     // [BDMerge F4] Hero Beam flags are session-only too
    sAlphaModeOverride.clear();  // [BDMerge G2.3 per-target] alpha-mode overrides are session-only too
}

// [BDMerge G3.3 Batch 3] Session-only per-projector "cast shadows" opt-OUT.
// Toggling adds/removes the projector's object UUID; a projector in the set is
// skipped in setupSpotLight's shadow-slot (mTargetShadowSpotLight[]) priority
// assignment, so it lights normally but never occupies a shadow slot. Cleared on
// relog with the shaft flag set (clearVolumetricShafts). Not persisted.
void LLPipeline::toggleProjectorCastShadows(const LLUUID& id)
{
    if (id.isNull())
        return;
    auto it = sNoShadowProjectors.find(id);
    if (it != sNoShadowProjectors.end())
        sNoShadowProjectors.erase(it);
    else
        sNoShadowProjectors.insert(id);
}

bool LLPipeline::isProjectorNoShadow(const LLUUID& id)
{
    return !sNoShadowProjectors.empty() && sNoShadowProjectors.count(id) != 0;
}

// [BDMerge F4] Session-only per-projector "Hero Beam" flag. Toggling adds/removes
// the projector's (root) object UUID; a projector in the set is skipped by the
// froxel light-injection loop and therefore marches its sharp shaft per-cone on top
// of the froxel atmosphere. Cleared on relog with the shaft flag
// (clearVolumetricShafts). Not persisted. Mirrors toggleProjectorCastShadows.
void LLPipeline::toggleHeroProjector(const LLUUID& id)
{
    if (id.isNull())
        return;
    auto it = sHeroProjectors.find(id);
    if (it != sHeroProjectors.end())
        sHeroProjectors.erase(it);
    else
        sHeroProjectors.insert(id);
}

bool LLPipeline::isHeroProjector(const LLUUID& id)
{
    return !sHeroProjectors.empty() && sHeroProjectors.count(id) != 0;
}

// [BDMerge G2.3 per-target] Rebuild the geometry keyed by an override id NOW, so a
// mode change re-routes immediately (canRenderAsMask() is consulted at batch build;
// the global flag only takes effect lazily). The id may be an object ROOT id (rebuild
// the whole linkset) or an AVATAR id (rebuild every non-HUD attachment's linkset).
// All drawable derefs are null-guarded.
static void bdmerge_rebuild_for_alpha_target(const LLUUID& id)
{
    if (id.isNull())
        return;
    LLViewerObject* obj = gObjectList.findObject(id);
    if (!obj)
        return;

    if (LLVOAvatar* av = obj->asAvatar())
    {
        // Avatar id: re-route every attachment (and its linkset children).
        for (LLVOAvatar::attachment_map_t::iterator it = av->mAttachmentPoints.begin();
             it != av->mAttachmentPoints.end(); ++it)
        {
            LLViewerJointAttachment* attachment = it->second;
            if (!attachment)
                continue;
            for (LLViewerJointAttachment::attachedobjs_vec_t::iterator ait = attachment->mAttachedObjects.begin();
                 ait != attachment->mAttachedObjects.end(); ++ait)
            {
                LLViewerObject* att = ait->get();
                if (!att)
                    continue;
                if (att->mDrawable.notNull())
                    gPipeline.markRebuild(att->mDrawable, LLDrawable::REBUILD_ALL);
                for (const LLPointer<LLViewerObject>& child : att->getChildren())
                {
                    if (child.notNull() && child->mDrawable.notNull())
                        gPipeline.markRebuild(child->mDrawable, LLDrawable::REBUILD_ALL);
                }
            }
        }
        return;
    }

    // Object id: rebuild this object and its linkset children.
    if (obj->mDrawable.notNull())
        gPipeline.markRebuild(obj->mDrawable, LLDrawable::REBUILD_ALL);
    for (const LLPointer<LLViewerObject>& child : obj->getChildren())
    {
        if (child.notNull() && child->mDrawable.notNull())
            gPipeline.markRebuild(child->mDrawable, LLDrawable::REBUILD_ALL);
    }
}

// [BDMerge G2.3 per-target] Session-only per-object/per-avatar alpha-mode override.
// mode 0 ERASES the entry (back to Default); 1/2 insert/overwrite. After changing we
// re-route the affected geometry so the change is visible immediately.
void LLPipeline::setAlphaModeOverride(const LLUUID& id, S32 mode)
{
    if (id.isNull())
        return;
    if (mode == 0)
    {
        auto it = sAlphaModeOverride.find(id);
        if (it != sAlphaModeOverride.end())
            sAlphaModeOverride.erase(it);
    }
    else
    {
        sAlphaModeOverride[id] = mode;
    }
    bdmerge_rebuild_for_alpha_target(id);
}

S32 LLPipeline::getAlphaModeOverride(const LLUUID& id)
{
    if (sAlphaModeOverride.empty() || id.isNull())
        return 0;
    auto it = sAlphaModeOverride.find(id);
    return (it != sAlphaModeOverride.end()) ? it->second : 0;
}

// Object-specific override beats avatar-wide: OBJECT override wins if present
// (non-0); else the AVATAR override; else 0 (no override).
S32 LLPipeline::resolveAlphaMode(const LLUUID& objRootId, const LLUUID& avatarId)
{
    if (sAlphaModeOverride.empty())
        return 0;
    S32 objMode = getAlphaModeOverride(objRootId);
    if (objMode != 0)
        return objMode;
    return getAlphaModeOverride(avatarId);
}

// Match the light-source prim's own ID and its root-edit ID (the context menu
// flags root prims, but the light feature can live on either the root or a
// child) - identical fallback to the volumetric shaft filter.
bool LLPipeline::isProjectorShadowSuppressed(LLVOVolume* volume)
{
    if (sNoShadowProjectors.empty() || volume == NULL)
        return false;
    if (isProjectorNoShadow(volume->getID()))
        return true;
    if (LLViewerObject* root = volume->getRootEdit())
    {
        if (isProjectorNoShadow(root->getID()))
            return true;
    }
    return false;
}

// [BDMerge G3.3 Batch 1 C] Session-only per-projector art-direction overrides.
// Setting an override also implicitly flags the projector so it emits a shaft; the
// render loop consults getVolumetricShaftOverride() per cone. Not persisted -
// cleared with the flag set on relog (clearVolumetricShafts).
void LLPipeline::setVolumetricShaftOverride(const LLUUID& id, const VolumetricShaftOverride& ov)
{
    if (id.isNull())
        return;
    sVolumetricShaftOverrides[id] = ov;
    sVolumetricShaftObjects.insert(id); // capturing implies enabling the shaft
}

void LLPipeline::clearVolumetricShaftOverride(const LLUUID& id)
{
    auto it = sVolumetricShaftOverrides.find(id);
    if (it != sVolumetricShaftOverrides.end())
        sVolumetricShaftOverrides.erase(it);
}

bool LLPipeline::getVolumetricShaftOverride(const LLUUID& id, VolumetricShaftOverride& out)
{
    if (sVolumetricShaftOverrides.empty())
        return false;
    auto it = sVolumetricShaftOverrides.find(id);
    if (it == sVolumetricShaftOverrides.end())
        return false;
    out = it->second;
    return true;
}

bool LLPipeline::hasVolumetricShaftOverride(const LLUUID& id)
{
    return !sVolumetricShaftOverrides.empty() && sVolumetricShaftOverrides.count(id) != 0;
}

void LLPipeline::combineGlow(LLRenderTarget* src, LLRenderTarget* dst)
{
    LL_PROFILE_GPU_ZONE("glow combine");

    // Go ahead and do our glow combine here in our destination.  We blit this later into the front buffer.
    dst->bindTarget();

    {

        gGlowCombineProgram.bind();

        gGlowCombineProgram.bindTexture(LLShaderMgr::DEFERRED_DIFFUSE, src);
        gGlowCombineProgram.bindTexture(LLShaderMgr::DEFERRED_EMISSIVE, &mGlow[1]);

        mScreenTriangleVB->setBuffer();
        mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
    }

    dst->flush();
}

void LLPipeline::renderDoF(LLRenderTarget* src, LLRenderTarget* dst)
{
    LL_PROFILE_GPU_ZONE("dof");
    {
        static LLCachedControl<bool> RenderDepthOfFieldInEditMode(gSavedSettings, "RenderDepthOfFieldInEditMode", false);
        static LLCachedControl<bool> RenderFocusPointLocked(gSavedSettings, "RenderFocusPointLocked", false);
        static LLCachedControl<bool> RenderFocusPointFollowsPointer(gSavedSettings, "RenderFocusPointFollowsPointer", false);
        // [F4] sDoFEnabled promoted to a class static (was a local bool) so
        // renderFocusPoint() can gate the crosshair on it later in
        // renderFinalize(). donor: Firestorm pipeline.cpp sDoFEnabled, FIRE-32023.
        sDoFEnabled =
            (RenderDepthOfFieldInEditMode || !LLToolMgr::getInstance()->inBuildMode()) &&
            RenderDepthOfField &&
            !gCubeSnapshot;

        gViewerWindow->setup3DViewport();

        if (sDoFEnabled)
        {
            LLGLDisable blend(GL_BLEND);

            // depth of field focal plane calculations
            static F32 current_distance = 16.f;
            static F32 start_distance = 16.f;
            static F32 transition_time = 1.f;

            LLVector3 focus_point;
            // [F4] sLastFocusPoint promoted to a class static (was a local
            // "last_focus_point") so renderFocusPoint() can draw a crosshair
            // at the current DoF target. donor: Firestorm pipeline.cpp
            // sLastFocusPoint, FIRE-16728.
            if (RenderFocusPointLocked && !sLastFocusPoint.isExactlyZero())
            {
                focus_point = sLastFocusPoint;
            }
            else
            {
                LLViewerObject* obj = LLViewerMediaFocus::getInstance()->getFocusedObject();
                if (obj && obj->mDrawable && obj->isSelected())
                { // focus on selected media object
                    S32 face_idx = LLViewerMediaFocus::getInstance()->getFocusedFace();
                    if (obj && obj->mDrawable)
                    {
                        LLFace* face = obj->mDrawable->getFace(face_idx);
                        if (face)
                        {
                            focus_point = face->getPositionAgent();
                        }
                    }
                }

                if (focus_point.isExactlyZero())
                {
                        if (LLViewerJoystick::getInstance()->getOverrideCamera() || RenderFocusPointFollowsPointer)
                    { // focus on point under cursor
                        focus_point.set(gDebugRaycastIntersection.getF32ptr());
                    }
                    else if (gAgentCamera.cameraMouselook())
                    { // focus on point under mouselook crosshairs
                        LLVector4a result;
                        result.clear();

                        gViewerWindow->cursorIntersect(-1, -1, 512.f, nullptr, -1, false, false, true, true, nullptr, nullptr, nullptr, &result);

                        focus_point.set(result.getF32ptr());
                    }
                    else
                    {
                        // focus on alt-zoom target
                        LLViewerRegion* region = gAgent.getRegion();
                        if (region)
                        {
                            focus_point = LLVector3(gAgentCamera.getFocusGlobal() - region->getOriginGlobal());
                        }
                    }
                }
            }
            sLastFocusPoint = focus_point;

            LLVector3 eye = LLViewerCamera::getInstance()->getOrigin();
            F32 target_distance = 16.f;
            if (!focus_point.isExactlyZero())
            {
                target_distance = LLViewerCamera::getInstance()->getAtAxis() * (focus_point - eye);
            }

            if (transition_time >= 1.f && fabsf(current_distance - target_distance) / current_distance > 0.01f)
            { // large shift happened, interpolate smoothly to new target distance
                transition_time = 0.f;
                start_distance = current_distance;
            }
            else if (transition_time < 1.f)
            { // currently in a transition, continue interpolating
                transition_time += 1.f / CameraFocusTransitionTime * gFrameIntervalSeconds.value();
                transition_time = llmin(transition_time, 1.f);

                F32 t = cosf(transition_time * F_PI + F_PI) * 0.5f + 0.5f;
                current_distance = start_distance + (target_distance - start_distance) * t;
            }
            else
            { // small or no change, just snap to target distance
                current_distance = target_distance;
            }

            // convert to mm
            F32 subject_distance = current_distance * 1000.f;
            F32 fnumber = CameraFNumber;
            F32 default_focal_length = CameraFocalLength;

            F32 fov = LLViewerCamera::getInstance()->getView();

            // [BDMerge A1.2] scale the FOV feeding the CoF blur_constant derivation below
            // so DoF blur strength keeps its window-resolution weighting at higher snapshot
            // output resolutions; see bdmerge_snapshot_autoscale_multiplier() above.
            const F32 default_fov = CameraFieldOfView * bdmerge_snapshot_autoscale_multiplier() * F_PI / 180.f;

            // F32 aspect_ratio = (F32) mRT->screen.getWidth()/(F32)mRT->screen.getHeight();

            F32 dv = 2.f * default_focal_length * tanf(default_fov / 2.f);

            F32 focal_length = dv / (2 * tanf(fov / 2.f));

            // F32 tan_pixel_angle = tanf(LLDrawable::sCurPixelAngle);

            // from wikipedia -- c = |s2-s1|/s2 * f^2/(N(S1-f))
            // where     N = fnumber
            //           s2 = dot distance
            //           s1 = subject distance
            //           f = focal length
            //

            F32 blur_constant = focal_length * focal_length / (fnumber * (subject_distance - focal_length));
            blur_constant /= 1000.f; // convert to meters for shader
            F32 magnification = focal_length / (subject_distance - focal_length);

            // [F4] WYSIWYG DoF fix (donor: Firestorm FIRE-13989 "DOF should be
            // equivalent in all resolutions of the same rendered image",
            // I:\enve indra/newview/pipeline.cpp LLPipeline::renderDoF). Scales
            // the pixel-angle and max circle-of-confusion the CoF/combine
            // shaders consume by the ratio of the on-screen window height to
            // this pass's render target height, so a DoF-enabled high-res
            // snapshot (dst taller/shorter than the window) blurs by the same
            // *visual* amount the user saw on screen, not the same *pixel*
            // amount.
            //
            // Composition with [BDMerge A1.2] BDMergeSnapshotAutoscale: BD's
            // gate (bdmerge_snapshot_autoscale_multiplier(), applied to
            // default_fov above) rescales the FOV baseline that feeds
            // focal_length/blur_constant/magnification - i.e. it changes the
            // DoF *strength* going into this pass. FSSnapshotDoFWysiwyg below
            // instead rescales DOF_TAN_PIXEL_ANGLE/DOF_MAX_COF downstream,
            // after blur_constant/magnification are already fixed - i.e. it
            // corrects the blur-radius *unit* mismatch between window and
            // target resolution. The two touch different stages of the same
            // pipeline and are numerically independent, so they compose
            // cleanly (no double-scaling) when both gates are on. Per
            // campaign ADAPTATION RULE 4, the FS behavior only applies while
            // its own gate (FSSnapshotDoFWysiwyg, default true) is on; with
            // it off, DOF_MAX_COF/DOF_TAN_PIXEL_ANGLE are unscaled exactly as
            // upstream Alchemy today, and BDMergeSnapshotAutoscale continues
            // to work unchanged either way.
            static LLCachedControl<bool> fs_dof_wysiwyg(gSavedSettings, "FSSnapshotDoFWysiwyg", true);
            F32 dof_tan_pixel_angle_scale = 1.f;
            F32 adj_cof = CameraMaxCoF;
            if (fs_dof_wysiwyg)
            {
                F32 screen_to_target_scale_factor = (F32)gViewerWindow->getWindowHeightRaw() / (F32)dst->getHeight();
                dof_tan_pixel_angle_scale = screen_to_target_scale_factor;
                adj_cof = CameraMaxCoF / screen_to_target_scale_factor;
            }

            { // build diffuse+bloom+CoF
                mRT->deferredLight.bindTarget();

                gDeferredCoFProgram.bind();

                gDeferredCoFProgram.bindTexture(LLShaderMgr::DEFERRED_DIFFUSE, src, LLTexUnit::TFO_POINT);
                gDeferredCoFProgram.bindTexture(LLShaderMgr::DEFERRED_DEPTH, &mRT->deferredScreen, true);

                gDeferredCoFProgram.uniform1f(LLShaderMgr::DEFERRED_DEPTH_CUTOFF, RenderEdgeDepthCutoff);
                gDeferredCoFProgram.uniform1f(LLShaderMgr::DEFERRED_NORM_CUTOFF, RenderEdgeNormCutoff);
                gDeferredCoFProgram.uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES, (GLfloat)dst->getWidth(), (GLfloat)dst->getHeight());
                gDeferredCoFProgram.uniform1f(LLShaderMgr::DOF_FOCAL_DISTANCE, -subject_distance / 1000.f);
                gDeferredCoFProgram.uniform1f(LLShaderMgr::DOF_BLUR_CONSTANT, blur_constant);
                gDeferredCoFProgram.uniform1f(LLShaderMgr::DOF_TAN_PIXEL_ANGLE, tanf(1.f / LLDrawable::sCurPixelAngle) * dof_tan_pixel_angle_scale);
                gDeferredCoFProgram.uniform1f(LLShaderMgr::DOF_MAGNIFICATION, magnification);
                gDeferredCoFProgram.uniform1f(LLShaderMgr::DOF_MAX_COF, adj_cof);
                gDeferredCoFProgram.uniform1f(LLShaderMgr::DOF_RES_SCALE, CameraDoFResScale);

                mScreenTriangleVB->setBuffer();
                mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
                gDeferredCoFProgram.unbind();
                mRT->deferredLight.flush();
            }

            U32 dof_width = (U32)(mRT->screen.getWidth() * CameraDoFResScale);
            U32 dof_height = (U32)(mRT->screen.getHeight() * CameraDoFResScale);

            { // perform DoF sampling at half-res (preserve alpha channel)
                src->bindTarget();
                glViewport(0, 0, dof_width, dof_height);

                gGL.setColorMask(true, false);

                static LLCachedControl<bool> RenderDepthOfFieldNearBlur(gSavedSettings, "RenderDepthOfFieldNearBlur", false);
                LLGLSLShader& post_program = RenderDepthOfFieldNearBlur ? gDeferredPostProgram : gDeferredPostProgramNoNear;

                post_program.bind();
                post_program.bindTexture(LLShaderMgr::DEFERRED_DIFFUSE, &mRT->deferredLight, LLTexUnit::TFO_POINT);

                post_program.uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES, (GLfloat)dst->getWidth(), (GLfloat)dst->getHeight());
                post_program.uniform1f(LLShaderMgr::DOF_MAX_COF, adj_cof);
                post_program.uniform1f(LLShaderMgr::DOF_RES_SCALE, CameraDoFResScale);

                mScreenTriangleVB->setBuffer();
                mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

                post_program.unbind();

                src->flush();
                gGL.setColorMask(true, true);
            }

            { // combine result based on alpha

                dst->bindTarget();
                glViewport(0, 0, dst->getWidth(), dst->getHeight());

                gDeferredDoFCombineProgram.bind();
                gDeferredDoFCombineProgram.bindTexture(LLShaderMgr::DEFERRED_DIFFUSE, src, LLTexUnit::TFO_POINT);
                gDeferredDoFCombineProgram.bindTexture(LLShaderMgr::DEFERRED_LIGHT, &mRT->deferredLight, LLTexUnit::TFO_POINT);

                gDeferredDoFCombineProgram.uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES, (GLfloat)dst->getWidth(), (GLfloat)dst->getHeight());
                gDeferredDoFCombineProgram.uniform1f(LLShaderMgr::DOF_MAX_COF, adj_cof);
                gDeferredDoFCombineProgram.uniform1f(LLShaderMgr::DOF_RES_SCALE, CameraDoFResScale);
                gDeferredDoFCombineProgram.uniform1f(LLShaderMgr::DOF_WIDTH, (dof_width - 1) / (F32)src->getWidth());
                gDeferredDoFCombineProgram.uniform1f(LLShaderMgr::DOF_HEIGHT, (dof_height - 1) / (F32)src->getHeight());

                mScreenTriangleVB->setBuffer();
                mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

                gDeferredDoFCombineProgram.unbind();

                dst->flush();
            }
        }
        else
        {
            copyRenderTarget(src, dst);
        }
    }
}

void LLPipeline::renderFinalize()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    LL_PROFILE_GPU_ZONE("renderFinalize");

    llassert(!gCubeSnapshot);
    LLVertexBuffer::unbind();
    LLGLState::checkStates();

    assertInitialized();

    gGL.color4f(1, 1, 1, 1);
    LLGLDepthTest depth(GL_FALSE);
    LLGLDisable blend(GL_BLEND);
    LLGLDisable cull(GL_CULL_FACE);

    enableLightsFullbright();

    gGL.setColorMask(true, true);
    glClearColor(0, 0, 0, 0);

    static LLCachedControl<bool> has_hdr(gSavedSettings, "RenderHDREnabled", true);
    bool hdr = gGLManager.mGLVersion > 4.05f && has_hdr();
    if (hdr)
    {
        copyScreenSpaceReflections(&mRT->screen, &mSceneMap);
    }

    // Weather is an HDR scene-lighting contribution: lightning affects the
    // exposure adaptation and bloom chain instead of bypassing the tonemapper.
    renderWeather(&mRT->screen);

    if (hdr)
    {
        generateLuminance(&mRT->screen, &mLuminanceMap);

        generateExposure(&mLuminanceMap, &mExposureMap);

        // HDR bloom runs pre-tonemap against the linear scene buffer. The pyramid
        // is generated here; the additive composite is folded into colorCorrect's
        // tonemap variants (BLOOM_COMPOSITE permutation) so we avoid a separate
        // fullscreen pass over the scene buffer. The legacy alpha-tagged prim-glow
        // signal is carried into the extract pass, so prim glow survives the
        // migration. compositeBloomHDR is preserved for standalone use cases.
        generateBloomHDR(&mRT->screen);
    }

    // [BDMerge G3.3 Phase 1 item 1] HDR-space projector volumetrics: composite the
    // shafts onto the still-linear HDR scene buffer, AFTER luminance/exposure/bloom
    // are generated (so shafts don't skew auto-exposure or leak into bloom yet -
    // feeding bloom is a deliberate Phase 3 item) but BEFORE colorCorrect, so the
    // active tonemapper (AMD LPM / ACES) rolls off the bright cores filmically
    // instead of the old post-tonemap placement clipping them. Additive in place
    // onto mRT->screen; shafts carry each light's colour.
    // [BDMerge Froxel F0] Hybrid froxel volumetrics grid (media atlas + debug
    // overlay). Runs right before the per-cone projector volumetrics, at the same
    // composite point. Fully gated on BDMergeFroxelVolumetrics (default OFF = no-op).
    renderFroxelVolumetrics(&mRT->screen);

    renderProjectorVolumetric(&mRT->screen);

    // [BDMerge G3.3 Phase 3 item 4] Optional, controlled soft-glow halo: feed a
    // scaled, blurred copy of the just-marched half-res shaft into the HDR bloom
    // pyramid base. Runs only in the HDR path (the bloom pyramid exists there) and
    // only when BDMergeProjectorVolumetricsBloomFeed > 0 (default 0 = no-op). The
    // bloom pyramid was already built from the clean pre-shaft scene above, so this
    // never skews the scene bloom - it only adds the deliberate halo contribution.
    if (hdr)
    {
        feedProjectorVolumetricBloom();
    }

    // Handles tonemap, colorgrading, and gamma correction in one pass. In the HDR
    // path, this also applies eye adaptation and bloom. In the non-HDR path, this
    // is just a linear copy with color correction.
    colorCorrect(&mRT->screen, &mRT->postPingMap, hdr, true);

    LLVertexBuffer::unbind();

    // Legacy glow for non-HDR path.  In the HDR path, glow is extracted as part of the
    // bloom process and composited back in after tonemapping.
    if (!hdr)
    {
        generateGlow(&mRT->postPingMap);
    }

    LLRenderTarget* sourceBuffer = &mRT->postPingMap;
    LLRenderTarget* targetBuffer = &mRT->postPongMap;

    // [BDMerge G3.2] volumetrics before AA (AA smooths raymarch banding) and
    // before glow combine (donor ordering; glow stays intact at high
    // brightness per spec acceptance)
    if (RenderVolumetricLighting && RenderShadowDetail > 0 && !gCubeSnapshot && gVolumetricLightProgram.isComplete())
    {
        renderVolumetric(sourceBuffer, targetBuffer);
        std::swap(sourceBuffer, targetBuffer);
    }

    // [BDMerge G3.3 Phase 1] the projector volumetric pass now runs earlier, in
    // linear HDR before colorCorrect (see above). The sun godray block above keeps
    // its original display-stage placement (G3.2 unchanged).

    if (RenderFSAAType == 1)
    {
        applyFXAA(sourceBuffer, targetBuffer);
        std::swap(sourceBuffer, targetBuffer);
    }
    else if (RenderFSAAType == 2)
    {
        generateSMAABuffers(sourceBuffer);
        applySMAA(sourceBuffer, targetBuffer);
        std::swap(sourceBuffer, targetBuffer);
    }

    static LLCachedControl<F32> cas_sharpness(gSavedSettings, "RenderCASSharpness", 0.4f);
    if (cas_sharpness > 0.0f && gCASProgram.isComplete())
    {
        applyCAS(sourceBuffer, targetBuffer);
        std::swap(sourceBuffer, targetBuffer);
    }

    if (!hdr)
    {
        combineGlow(sourceBuffer, targetBuffer);
        std::swap(sourceBuffer, targetBuffer);
    }

    // [BDMerge A5.4-3] Motion blur: after glow-combine, before DoF (mirror BD
    // pipeline.cpp:8556-8560). Requires the velocity buffer (which this gate
    // also forces on via the alloc/pass sites); never in cube snapshots.
    if (BDMergeMotionBlur && !gCubeSnapshot && mVelocityMap.isComplete()
        && gDeferredMotionBlurProgram.isComplete())
    {
        renderMotionBlurComposite(sourceBuffer, targetBuffer);
        std::swap(sourceBuffer, targetBuffer);
    }

    gGLViewport[0] = gViewerWindow->getWorldViewRectRaw().mLeft;
    gGLViewport[1] = gViewerWindow->getWorldViewRectRaw().mBottom;
    gGLViewport[2] = gViewerWindow->getWorldViewRectRaw().getWidth();
    gGLViewport[3] = gViewerWindow->getWorldViewRectRaw().getHeight();
    glViewport(gGLViewport[0], gGLViewport[1], gGLViewport[2], gGLViewport[3]);

    static LLCachedControl<bool> RenderDepthOfFieldInEditMode(gSavedSettings, "RenderDepthOfFieldInEditMode", false);
    if (RenderDepthOfField && (RenderDepthOfFieldInEditMode || !LLToolMgr::getInstance()->inBuildMode()) && !gCubeSnapshot)
    {
        renderDoF(sourceBuffer, targetBuffer);
        std::swap(sourceBuffer, targetBuffer);
    }

    gGLViewport[0] = gViewerWindow->getWorldViewRectRaw().mLeft;
    gGLViewport[1] = gViewerWindow->getWorldViewRectRaw().mBottom;
    gGLViewport[2] = gViewerWindow->getWorldViewRectRaw().getWidth();
    gGLViewport[3] = gViewerWindow->getWorldViewRectRaw().getHeight();
    glViewport(gGLViewport[0], gGLViewport[1], gGLViewport[2], gGLViewport[3]);

// [RLVa:KB] - @setsphere
    if (RlvActions::hasBehaviour(RLV_BHVR_SETSPHERE))
    {
        LLShaderEffectParams params(sourceBuffer, targetBuffer, false);
        LLVfxManager::instance().runEffect(EVisualEffect::RlvSphere, &params);
        std::swap(sourceBuffer, targetBuffer);
    }
// [/RLVa:KB]

    if (RenderBufferVisualization > -1)
    {
        switch (RenderBufferVisualization)
        {
        case 0:
        case 1:
        case 2:
        case 3:
            visualizeBuffers(&mRT->deferredScreen, sourceBuffer, RenderBufferVisualization);
            break;
        case 4:
            visualizeBuffers(&mLuminanceMap, sourceBuffer, 0);
            break;
        case 5:
        {
            if (RenderFSAAType > 0)
            {
                visualizeBuffers(&mFXAAMap, sourceBuffer, 0);
            }
            break;
        }
        case 6:
        {
            if (RenderFSAAType == 2)
            {
                visualizeBuffers(&mSMAABlendBuffer, sourceBuffer, 0);
            }
            break;
        }
        default:
            break;
        }
    }

    // [BDMerge A5.4-1a] Velocity buffer debug overlay. Overwrites the final image
    // with a colour visualization of mVelocityMap so the motion vectors can be
    // validated in-world. Runs last (after AA/DoF) but before the present blit.
    if (BDMergeVelocityBuffer && BDMergeVelocityDebug && !gCubeSnapshot)
    {
        renderVelocityDebug(sourceBuffer);
    }

    // Present the screen target.
    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE("renderFinalize - final blit");
        LL_PROFILE_GPU_ZONE("renderFinalize - final blit");

        gBlitWithEffectsProgram.bind();

        // Whatever is last in the above post processing chain should _always_ be rendered directly here.  If not, expect problems.
        gBlitWithEffectsProgram.bindTexture(LLShaderMgr::DEFERRED_DIFFUSE, sourceBuffer);
        gBlitWithEffectsProgram.bindTexture(LLShaderMgr::DEFERRED_DEPTH, &mRT->deferredScreen, true);

        // Setup Uniforms
        gBlitWithEffectsProgram.uniform1ui(LLShaderMgr::FRAME_ID, LLFrameTimer::getFrameCount());
        gBlitWithEffectsProgram.uniform2f(LLShaderMgr::SCREEN_RESOLUTION, (F32)gViewerWindow->getWorldViewRectRaw().getWidth(),
                                          (F32)gViewerWindow->getWorldViewRectRaw().getHeight());

        // Vignette
        static LLCachedControl<F32> vignette_amount(gSavedSettings, "RenderVignetteAmount", 0.0f, "[0, 1] default 0.");
        static LLCachedControl<F32> vignette_radius(gSavedSettings, "RenderVignetteRadius", 1.0f);
        static LLCachedControl<F32> vignette_soft(gSavedSettings, "RenderVignetteSoft", 0.5f);
        static LLCachedControl<F32> vignette_shape(gSavedSettings, "RenderVignetteShape", 0.0f, "[0, 1] 0 circular, 1 rounded square.");
        static LLCachedControl<LLColor3>  vignette_color(gSavedSettings, "RenderVignetteColor", LLColor3(0.0f, 0.0f, 0.0f));
        static LLCachedControl<LLColor3>  vignette_mid_color(gSavedSettings, "RenderVignetteMidColor", LLColor3(0.0f, 0.0f, 0.0f));
        static LLCachedControl<F32>       vignette_mid_point(gSavedSettings, "RenderVignetteMidPoint", 0.0f);
        static LLCachedControl<LLVector3> vignette_center(gSavedSettings, "RenderVignetteCenter", LLVector3(0.0f, 0.0f, 0.0f));
        static LLCachedControl<bool>      vignette_correct_aspect(gSavedSettings, "RenderVignetteCorrectAspect", false);
        static LLCachedControl<F32>       vignette_feather(gSavedSettings, "RenderVignetteFeather", 1.0f);
        gBlitWithEffectsProgram.uniform1f(LLShaderMgr::VIGNETTE_AMOUNT, llclamp(vignette_amount(), 0.0f, 1.0f));
        gBlitWithEffectsProgram.uniform1f(LLShaderMgr::VIGNETTE_RADIUS, llclamp(vignette_radius(), 0.25f, 1.5f));
        gBlitWithEffectsProgram.uniform1f(LLShaderMgr::VIGNETTE_SOFT, llclamp(vignette_soft(), 0.05f, 1.0f));
        gBlitWithEffectsProgram.uniform1f(LLShaderMgr::VIGNETTE_SHAPE, llclamp(vignette_shape(), 0.0f, 1.0f));
        gBlitWithEffectsProgram.uniform3fv(LLShaderMgr::VIGNETTE_COLOR, 1, vignette_color().mV);
        gBlitWithEffectsProgram.uniform3fv(LLShaderMgr::VIGNETTE_MID_COLOR, 1, vignette_mid_color().mV);
        gBlitWithEffectsProgram.uniform1f(LLShaderMgr::VIGNETTE_MID_POINT, llclamp(vignette_mid_point(), 0.0f, 1.0f));
        if (vignette_correct_aspect)
        {
            gBlitWithEffectsProgram.uniform2f(LLShaderMgr::VIGNETTE_ASPECT, (F32)sourceBuffer->getWidth(), (F32)sourceBuffer->getHeight());
        }
        else
        {
            gBlitWithEffectsProgram.uniform2f(LLShaderMgr::VIGNETTE_ASPECT, 1.f, 1.f);
        }
        gBlitWithEffectsProgram.uniform2fv(LLShaderMgr::VIGNETTE_CENTER, 1, vignette_center().mV);
        gBlitWithEffectsProgram.uniform1f(LLShaderMgr::VIGNETTE_FEATHER, llclamp(vignette_feather(), 0.2f, 4.0f));

        // CVD Compensation
        static LLCachedControl<S32> cvd_mode(gSavedSettings, "RenderCVDMode", 0);
        static LLCachedControl<F32> cvd_amount(gSavedSettings, "RenderCVDAmount", 0.0f);
        gBlitWithEffectsProgram.uniform1i(LLShaderMgr::CVD_MODE, llclamp(cvd_mode(), 0, 3));
        gBlitWithEffectsProgram.uniform1f(LLShaderMgr::CVD_AMOUNT, llclamp(cvd_amount(), 0.0f, 1.0f));

        // Film Grain
        static LLCachedControl<bool>     film_grain_animated(gSavedSettings, "RenderFilmGrainAnimated", true);
        static LLCachedControl<F32>      film_grain_amount(gSavedSettings, "RenderFilmGrainAmount", 0.0f);
        static LLCachedControl<S32>      film_grain_style(gSavedSettings, "RenderFilmGrainStyle", 0);
        static LLCachedControl<F32>      film_grain_size(gSavedSettings, "RenderFilmGrainSize", 1.0f);
        static LLCachedControl<F32>      film_grain_range(gSavedSettings, "RenderFilmGrainRange", 0.5f);
        static LLCachedControl<LLColor3> film_grain_tint(gSavedSettings, "RenderFilmGrainTint", LLColor3(1.0f, 1.0f, 1.0f));
        gBlitWithEffectsProgram.uniform1f(LLShaderMgr::GRAIN_AMOUNT, llclamp(film_grain_amount(), 0.0f, 1.0f));
        gBlitWithEffectsProgram.uniform1i(LLShaderMgr::GRAIN_STYLE, llclamp(film_grain_style(), 0, 3));
        gBlitWithEffectsProgram.uniform1f(LLShaderMgr::GRAIN_SIZE, llclamp(film_grain_size(), 1.0f, 8.0f));
        gBlitWithEffectsProgram.uniform1f(LLShaderMgr::GRAIN_RANGE, llclamp(film_grain_range(), 0.0f, 1.0f));
        gBlitWithEffectsProgram.uniform3fv(LLShaderMgr::GRAIN_TINT, 1, film_grain_tint().mV);
        gBlitWithEffectsProgram.uniform1i(LLShaderMgr::GRAIN_ANIMATE, film_grain_animated ? 1 : 0);

        // Dithering
        static LLCachedControl<bool> dither_enabled(gSavedSettings, "RenderDitherEnabled", true);
        static LLCachedControl<bool> dither_animated(gSavedSettings, "RenderDitherAnimated", true);
        gBlitWithEffectsProgram.uniform1f(LLShaderMgr::DITHER_AMOUNT, dither_enabled() ? 1.0f : 0.0f);
        gBlitWithEffectsProgram.uniform1i(LLShaderMgr::DITHER_BITS, LLRender::s10bitBackBuffer ? 10 : 8);
        gBlitWithEffectsProgram.uniform1i(LLShaderMgr::DITHER_ANIMATE, dither_animated ? 1 : 0);

        // Previews
        static LLCachedControl<S32> preview_mode(gSavedSettings, "RenderEffectPreviewMode", 0);
        gBlitWithEffectsProgram.uniform1i(LLShaderMgr::PREVIEW_MODE, llclamp(preview_mode(), 0, 7));

        {
            LLGLDepthTest depth_test(GL_TRUE, GL_TRUE, GL_ALWAYS);
            mScreenTriangleVB->setBuffer();
            mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
        }

        gBlitWithEffectsProgram.unbind();
    }

    gGL.setSceneBlendType(LLRender::BT_ALPHA);

    // [F4/F8] campaign item F4/F8: composition guide overlay + DoF focus
    // point crosshair. Placed here (after the final blit, before physics
    // debug) to mirror donor Firestorm's call site in LLPipeline::renderFinalize
    // (I:\enve indra/newview/pipeline.cpp, right after its final present
    // blit). Both are UI-pass draws gated on RENDER_DEBUG_FEATURE_UI, so they
    // are automatically excluded from snapshots exactly like the rest of the
    // UI (see the two functions' own comments for detail).
    renderCompositionGuideOverlay();
    renderFocusPoint();

    if (hasRenderDebugMask(LLPipeline::RENDER_DEBUG_PHYSICS_SHAPES))
    {
        renderPhysicsDisplay();
    }

    /*if (LLRenderTarget::sUseFBO && !gCubeSnapshot)
    { // copy depth buffer from mRT->screen to framebuffer
        LLRenderTarget::copyContentsToFramebuffer(mRT->screen, 0, 0, mRT->screen.getWidth(), mRT->screen.getHeight(), 0, 0,
                                                  mRT->screen.getWidth(), mRT->screen.getHeight(),
                                                  GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
    }*/

    LLVertexBuffer::unbind();

    LLGLState::checkStates();

    // flush calls made to "addTrianglesDrawn" so far to stats machinery
    recordTrianglesDrawn();
}

void LLPipeline::bindLightFunc(LLGLSLShader& shader)
{
    S32 channel = shader.enableTexture(LLShaderMgr::DEFERRED_LIGHTFUNC);
    if (channel > -1)
    {
        gGL.getTexUnit(channel)->bindManual(LLTexUnit::TT_TEXTURE, mLightFunc);
    }

    channel = shader.enableTexture(LLShaderMgr::DEFERRED_BRDF_LUT, LLTexUnit::TT_TEXTURE);
    if (channel > -1)
    {
        mPbrBrdfLut.bindTexture(0, channel);
    }
}

void LLPipeline::bindShadowMaps(LLGLSLShader& shader)
{
    for (U32 i = 0; i < 4; i++)
    {
        LLRenderTarget* shadow_target = getSunShadowTarget(i);
        if (shadow_target)
        {
            S32 channel = shader.enableTexture(LLShaderMgr::DEFERRED_SHADOW0 + i, LLTexUnit::TT_TEXTURE);
            if (channel > -1)
            {
                gGL.getTexUnit(channel)->bind(getSunShadowTarget(i), true);
            }
        }
    }

    for (U32 i = 4; i < 4 + MAX_SPOT_SHADOWS; i++) // [BDMerge NSpot]
    {
        S32 channel = shader.enableTexture(LLShaderMgr::DEFERRED_SHADOW0 + i);
        if (channel > -1)
        {
            LLRenderTarget* shadow_target = getSpotShadowTarget(i - 4);
            if (shadow_target && shadow_target->getWidth() > 0)
            {
                gGL.getTexUnit(channel)->bind(shadow_target, true);
            }
        }
    }
}

void LLPipeline::clearPrismLensDirtyScreenShaderTracking()
{
    mPrismLensDirtyScreenShaders.clear();
}

void LLPipeline::bindDeferredShaderFast(LLGLSLShader& shader)
{
    if (shader.mCanBindFast)
    { // was previously fully bound, use fast path
        shader.bind();
        bindLightFunc(shader);
        bindShadowMaps(shader);
        bindReflectionProbes(shader);

        auto dirty_it = mPrismLensDirtyScreenShaders.end();
        bool update_screen_extent = sPrismLensRender;
        if (!update_screen_extent && !mPrismLensDirtyScreenShaders.empty())
        {
            dirty_it = mPrismLensDirtyScreenShaders.find(&shader);
            update_screen_extent = dirty_it != mPrismLensDirtyScreenShaders.end();
        }
        if (update_screen_extent)
        {
            // Prism scratch packs are exact-size, so the bound deferred
            // target's physical dimensions ARE the render dimensions.
            U32 render_width = mRT->deferredScreen.getWidth();
            U32 render_height = mRT->deferredScreen.getHeight();
            if (shader.getUniformLocation(LLShaderMgr::VIEWPORT) != -1)
            {
                shader.uniform4f(LLShaderMgr::VIEWPORT,
                                 static_cast<F32>(gGLViewport[0]),
                                 static_cast<F32>(gGLViewport[1]),
                                 static_cast<F32>(gGLViewport[2]),
                                 static_cast<F32>(gGLViewport[3]));
            }
            shader.uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES,
                             static_cast<F32>(render_width),
                             static_cast<F32>(render_height));
            if (sPrismLensRender)
            {
                mPrismLensDirtyScreenShaders.insert(&shader);
            }
            else
            {
                mPrismLensDirtyScreenShaders.erase(dirty_it);
            }
        }
    }
    else
    { //wasn't previously bound, use slow path
        bindDeferredShader(shader);
        shader.mCanBindFast = true;
    }
}

void LLPipeline::bindDeferredShader(LLGLSLShader& shader, LLRenderTarget* light_target, LLRenderTarget* depth_target)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    LLRenderTarget* deferred_target       = &mRT->deferredScreen;
    LLRenderTarget* deferred_light_target = &mRT->deferredLight;

    shader.bind();
    S32 channel = 0;
    channel = shader.enableTexture(LLShaderMgr::DEFERRED_DIFFUSE, deferred_target->getUsage());
    if (channel > -1)
    {
        deferred_target->bindTexture(0,channel, LLTexUnit::TFO_POINT); // frag_data[0]
        gGL.getTexUnit(channel)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
    }

    channel = shader.enableTexture(LLShaderMgr::DEFERRED_SPECULAR, deferred_target->getUsage());
    if (channel > -1)
    {
        deferred_target->bindTexture(1, channel, LLTexUnit::TFO_POINT); // frag_data[1]
        gGL.getTexUnit(channel)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
    }

    channel = shader.enableTexture(LLShaderMgr::NORMAL_MAP, deferred_target->getUsage());
    if (channel > -1)
    {
        deferred_target->bindTexture(2, channel, LLTexUnit::TFO_POINT); // frag_data[2]
        gGL.getTexUnit(channel)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
    }

    channel = shader.enableTexture(LLShaderMgr::DEFERRED_EMISSIVE, deferred_target->getUsage());
    if (channel > -1)
    {
        deferred_target->bindTexture(3, channel, LLTexUnit::TFO_POINT); // frag_data[3]
        gGL.getTexUnit(channel)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
    }

    channel = shader.enableTexture(LLShaderMgr::DEFERRED_DEPTH, deferred_target->getUsage());
    if (channel > -1)
    {
        if (depth_target)
        {
            gGL.getTexUnit(channel)->bind(depth_target, true);
        }
        else
        {
            gGL.getTexUnit(channel)->bind(deferred_target, true);
        }
        stop_glerror();
    }

    channel = shader.enableTexture(LLShaderMgr::EXPOSURE_MAP);
    if (channel > -1)
    {
        gGL.getTexUnit(channel)->bind(&mExposureMap);
    }

    if (shader.getUniformLocation(LLShaderMgr::VIEWPORT) != -1)
    {
        shader.uniform4f(LLShaderMgr::VIEWPORT, (F32) gGLViewport[0],
                                    (F32) gGLViewport[1],
                                    (F32) gGLViewport[2],
                                    (F32) gGLViewport[3]);
    }

    if (sReflectionRender && shader.getUniformLocation(LLShaderMgr::MODELVIEW_MATRIX) != -1)
    {
        shader.uniformMatrix4fv(LLShaderMgr::MODELVIEW_MATRIX, 1, false, glm::value_ptr(mReflectionModelView));
    }

    channel = shader.enableTexture(LLShaderMgr::DEFERRED_NOISE);
    if (channel > -1)
    {
        gGL.getTexUnit(channel)->bindManual(LLTexUnit::TT_TEXTURE, mNoiseMap);
        gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
    }

    bindLightFunc(shader);

    stop_glerror();

    light_target = light_target ? light_target : deferred_light_target;
    channel = shader.enableTexture(LLShaderMgr::DEFERRED_LIGHT, light_target->getUsage());
    if (channel > -1)
    {
        if (light_target->isComplete())
        {
            light_target->bindTexture(0, channel, LLTexUnit::TFO_POINT);
        }
        else
        {
            gGL.getTexUnit(channel)->bindFast(LLViewerFetchedTexture::sWhiteImagep);
        }
    }

    stop_glerror();

    bindShadowMaps(shader);

    stop_glerror();

    // [BDMerge NSpot] 4 sun + MAX_SPOT_SHADOWS projector matrices
    F32 mat[16*MAX_SHADOW_MATS];
    for (U32 j = 0; j < MAX_SHADOW_MATS; j++)
    {
        for (U32 i = 0; i < 16; i++)
        {
            mat[j*16+i] = glm::value_ptr(mSunShadowMatrix[j])[i];
        }
    }

    shader.uniformMatrix4fv(LLShaderMgr::DEFERRED_SHADOW_MATRIX, MAX_SHADOW_MATS, false, mat);

    stop_glerror();

    if (!LLPipeline::sReflectionProbesEnabled)
    {
        channel = shader.enableTexture(LLShaderMgr::ENVIRONMENT_MAP, LLTexUnit::TT_CUBE_MAP);
        if (channel > -1)
        {
            LLCubeMap* cube_map = gSky.mVOSkyp ? gSky.mVOSkyp->getCubeMap() : NULL;
            if (cube_map)
            {
                cube_map->enable(channel);
                cube_map->bind();
            }

            F32* m = gGLModelView;

            F32 mat[] = { m[0], m[1], m[2],
                          m[4], m[5], m[6],
                          m[8], m[9], m[10] };

            shader.uniformMatrix3fv(LLShaderMgr::DEFERRED_ENV_MAT, 1, true, mat);
        }
    }

    bindReflectionProbes(shader);

    /*if (gCubeSnapshot)
    { // we only really care about the first two values, but the shader needs increasing separation between clip planes
        shader.uniform4f(LLShaderMgr::DEFERRED_SHADOW_CLIP, 1.f, 64.f, 128.f, 256.f);
    }
    else*/
    {
        shader.uniform4fv(LLShaderMgr::DEFERRED_SHADOW_CLIP, 1, mSunClipPlanes.mV);
    }
    shader.uniform1f(LLShaderMgr::DEFERRED_SUN_WASH, RenderDeferredSunWash);
    shader.uniform1f(LLShaderMgr::DEFERRED_SHADOW_NOISE, RenderShadowNoise);
    shader.uniform1f(LLShaderMgr::DEFERRED_BLUR_SIZE, RenderShadowBlurSize);

    // [BDMerge A1.2] scale SSAO radius/max-radius so a high-res snapshot keeps the same
    // ambient-occlusion footprint (in effective screen-space terms) it had at window
    // resolution; see bdmerge_snapshot_autoscale_multiplier() above.
    const F32 bdmerge_autoscale = bdmerge_snapshot_autoscale_multiplier();
    shader.uniform1f(LLShaderMgr::DEFERRED_SSAO_RADIUS, RenderSSAOScale * bdmerge_autoscale);
    shader.uniform1f(LLShaderMgr::DEFERRED_SSAO_MAX_RADIUS, (GLfloat)RenderSSAOMaxScale * bdmerge_autoscale);

    F32 ssao_factor = RenderSSAOFactor;
    shader.uniform1f(LLShaderMgr::DEFERRED_SSAO_FACTOR, ssao_factor);
    shader.uniform1f(LLShaderMgr::DEFERRED_SSAO_FACTOR_INV, 1.0f/ssao_factor);

    LLVector3 ssao_effect = RenderSSAOEffect;
    F32 matrix_diag = (ssao_effect[0] + 2.0f*ssao_effect[1])/3.0f;
    F32 matrix_nondiag = (ssao_effect[0] - ssao_effect[1])/3.0f;
    // This matrix scales (proj of color onto <1/rt(3),1/rt(3),1/rt(3)>) by
    // value factor, and scales remainder by saturation factor
    F32 ssao_effect_mat[] = {   matrix_diag, matrix_nondiag, matrix_nondiag,
                                matrix_nondiag, matrix_diag, matrix_nondiag,
                                matrix_nondiag, matrix_nondiag, matrix_diag};
    shader.uniformMatrix3fv(LLShaderMgr::DEFERRED_SSAO_EFFECT_MAT, 1, GL_FALSE, ssao_effect_mat);

    //F32 shadow_offset_error = 1.f + RenderShadowOffsetError * fabsf(LLViewerCamera::getInstance()->getOrigin().mV[2]);
    F32 shadow_bias_error = RenderShadowBiasError * fabsf(LLViewerCamera::getInstance()->getOrigin().mV[2])/3000.f;
    F32 shadow_bias       = RenderShadowBias + shadow_bias_error;

    U32 deferred_width = deferred_target->getWidth();
    U32 deferred_height = deferred_target->getHeight();
    shader.uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES,
                     static_cast<F32>(deferred_width),
                     static_cast<F32>(deferred_height));
    if (sPrismLensRender)
    {
        mPrismLensDirtyScreenShaders.insert(&shader);
    }
    else if (!mPrismLensDirtyScreenShaders.empty())
    {
        mPrismLensDirtyScreenShaders.erase(&shader);
    }
    shader.uniform1f(LLShaderMgr::DEFERRED_NEAR_CLIP, LLViewerCamera::getInstance()->getNear()*2.f);
    shader.uniform1f (LLShaderMgr::DEFERRED_SHADOW_OFFSET, RenderShadowOffset); //*shadow_offset_error);
    shader.uniform1f(LLShaderMgr::DEFERRED_SHADOW_BIAS, shadow_bias);
    shader.uniform1f(LLShaderMgr::DEFERRED_SPOT_SHADOW_OFFSET, RenderSpotShadowOffset);
    shader.uniform1f(LLShaderMgr::DEFERRED_SPOT_SHADOW_BIAS, RenderSpotShadowBias);

    // [BDMerge Batch 2] Feature 1: soft (contact-hardening + filled) shadow uniforms.
    // Consumed by shadowUtil.glsl (pcfSpotShadow / pcfShadow). Gate off (default)
    // => the classic hard 5-tap kernels run and the look is unchanged.
    shader.uniform1i(LLShaderMgr::SOFT_SHADOW_ENABLE, BDMergeSoftProjectorShadows ? 1 : 0);
    shader.uniform1f(LLShaderMgr::SOFT_SHADOW_SCALE, BDMergeSoftShadowSoftness);
    shader.uniform1f(LLShaderMgr::SOFT_SHADOW_MAX, llmax(BDMergeSoftShadowMaxPenumbra, 1.f));
    shader.uniform1f(LLShaderMgr::SOFT_SHADOW_FILL, llclamp(BDMergeSoftShadowFill, 0.f, 1.f));
    shader.uniform1i(LLShaderMgr::SOFT_SHADOW_SUN, BDMergeSoftShadowSun ? 1 : 0);
    // [Vogel A/B] kernel selector: 0 = fixed 12-tap Poisson (baseline), 1 = Vogel
    // disk with a per-pixel spatial rotation. Tap count clamped to the shader max.
    shader.uniform1i(LLShaderMgr::SOFT_SHADOW_VOGEL, BDMergeSoftShadowVogel ? 1 : 0);
    shader.uniform1i(LLShaderMgr::SOFT_SHADOW_TAPS, llclamp(BDMergeSoftShadowTaps, 1, 32));

    shader.uniform3fv(LLShaderMgr::DEFERRED_SUN_DIR, 1, mTransformedSunDir.mV);
    shader.uniform3fv(LLShaderMgr::DEFERRED_MOON_DIR, 1, mTransformedMoonDir.mV);
    LLRenderTarget* sun_shadow = getSunShadowTarget(0);
    shader.uniform2f(LLShaderMgr::DEFERRED_SHADOW_RES,
                     (GLfloat)sun_shadow->getWidth(),
                     (GLfloat)sun_shadow->getHeight());
    // [Prism spot shadows Stage 2] getSpotShadowTarget redirects to the
    // dedicated aux maps while sPrismLensRender is set (harmless if the sizes
    // match, correct if they ever diverge); the main path is unchanged.
    LLRenderTarget* spot_shadow = getSpotShadowTarget(0);
    shader.uniform2f(LLShaderMgr::DEFERRED_PROJ_SHADOW_RES, (GLfloat)spot_shadow->getWidth(), (GLfloat)spot_shadow->getHeight());
    shader.uniform1f(LLShaderMgr::DEFERRED_DEPTH_CUTOFF, RenderEdgeDepthCutoff);
    shader.uniform1f(LLShaderMgr::DEFERRED_NORM_CUTOFF, RenderEdgeNormCutoff);

    shader.uniformMatrix4fv(LLShaderMgr::MODELVIEW_DELTA_MATRIX, 1, GL_FALSE, glm::value_ptr(gGLDeltaModelView));
    shader.uniformMatrix4fv(LLShaderMgr::INVERSE_MODELVIEW_DELTA_MATRIX, 1, GL_FALSE, glm::value_ptr(gGLInverseDeltaModelView));

    shader.uniform1i(LLShaderMgr::CUBE_SNAPSHOT, gCubeSnapshot ? 1 : 0);

    if (shader.getUniformLocation(LLShaderMgr::DEFERRED_NORM_MATRIX) >= 0)
    {
        glm::mat4 norm_mat = glm::transpose(glm::inverse(get_current_modelview()));
        shader.uniformMatrix4fv(LLShaderMgr::DEFERRED_NORM_MATRIX, 1, false, glm::value_ptr(norm_mat));
    }

    // auto adjust legacy sun color if needed
    static LLCachedControl<bool> should_auto_adjust(gSavedSettings, "RenderSkyAutoAdjustLegacy", false);
    static LLCachedControl<F32> auto_adjust_sun_color_scale(gSavedSettings, "RenderSkyAutoAdjustSunColorScale", 1.f);
    LLSettingsSky::ptr_t psky = LLEnvironment::instance().getCurrentSky();
    LLColor3 sun_diffuse(mSunDiffuse.mV);
    if (should_auto_adjust && psky->canAutoAdjust())
    {
        sun_diffuse *= auto_adjust_sun_color_scale;
    }

    shader.uniform3fv(LLShaderMgr::SUNLIGHT_COLOR, 1, sun_diffuse.mV);
    shader.uniform3fv(LLShaderMgr::MOONLIGHT_COLOR, 1, mMoonDiffuse.mV);

    shader.uniform1f(LLShaderMgr::REFLECTION_PROBE_MAX_LOD, mReflectionMapManager.mMaxProbeLOD);
}


LLColor3 pow3f(LLColor3 v, F32 f)
{
    v.mV[0] = powf(v.mV[0], f);
    v.mV[1] = powf(v.mV[1], f);
    v.mV[2] = powf(v.mV[2], f);
    return v;
}

LLVector4 pow4fsrgb(LLVector4 v, F32 f)
{
    v.mV[0] = powf(v.mV[0], f);
    v.mV[1] = powf(v.mV[1], f);
    v.mV[2] = powf(v.mV[2], f);
    return v;
}

void LLPipeline::renderDeferredLighting()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    LL_PROFILE_GPU_ZONE("renderDeferredLighting");
    if (!sCull)
    {
        return;
    }

    llassert(!sRenderingHUDs);

    F32 light_scale = 1.f;

    if (gCubeSnapshot)
    { //darken local lights when probe ambiance is above 1
        light_scale = mReflectionMapManager.mLightScale;
    }
    else
    {
        static LLCachedControl<F32> alchemy_light_scale(gSavedSettings, "AlchemyGlobalLightScale", 1.f);
        light_scale = alchemy_light_scale;
    }

    LLRenderTarget *screen_target         = &mRT->screen;
    LLRenderTarget* deferred_light_target = &mRT->deferredLight;

    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE("deferred");
        LLViewerCamera *camera = LLViewerCamera::getInstance();

        if (gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_HUD))
        {
            gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD);
        }

        gGL.setColorMask(true, true);

        // draw a cube around every light
        LLVertexBuffer::unbind();

        LLGLEnable cull(GL_CULL_FACE);
        LLGLEnable blend(GL_BLEND);

        glm::mat4 mat = get_current_modelview();

        setupHWLights();  // to set mSun/MoonDir;

        glm::vec4 tc(mSunDir);
        tc = mat * tc;
        mTransformedSunDir.set(tc);

        glm::vec4 tc_moon(mMoonDir);
        tc_moon = mat * tc_moon;
        mTransformedMoonDir.set(tc_moon);

        if ((RenderDeferredSSAO && !gCubeSnapshot) || RenderShadowDetail > 0)
        {
            LL_PROFILE_GPU_ZONE("sun program");
            bindPrismLensTarget(*deferred_light_target);
            {  // paint shadow/SSAO light map (direct lighting lightmap)
                LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE("renderDeferredLighting - sun shadow");

                LLGLSLShader& sun_shader = gCubeSnapshot ? gDeferredSunProbeProgram : gDeferredSunProgram;
                bindDeferredShader(sun_shader, deferred_light_target);
                mScreenTriangleVB->setBuffer();
                glClearColor(1, 1, 1, 1);
                deferred_light_target->clear(GL_COLOR_BUFFER_BIT);
                glClearColor(0, 0, 0, 0);

                U32 sun_width = deferred_light_target->getWidth();
                U32 sun_height = deferred_light_target->getHeight();
                sun_shader.uniform2f(LLShaderMgr::DEFERRED_SCREEN_RES,
                                     static_cast<F32>(sun_width),
                                     static_cast<F32>(sun_height));

                {
                    LLGLDisable   blend(GL_BLEND);
                    LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_ALWAYS);
                    mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
                }

                unbindDeferredShader(sun_shader);
            }
            deferred_light_target->flush();
        }

        if (RenderDeferredSSAO && !gCubeSnapshot)
        {
            // soften direct lighting lightmap
            LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE("renderDeferredLighting - soften shadow");
            LL_PROFILE_GPU_ZONE("soften shadow");
            // blur lightmap
            bindPrismLensTarget(*screen_target);
            glClearColor(1, 1, 1, 1);
            screen_target->clear(GL_COLOR_BUFFER_BIT);
            glClearColor(0, 0, 0, 0);

            bindDeferredShader(gDeferredBlurLightProgram);

            LLVector3 go = RenderShadowGaussian;
            const U32 kern_length = 4;
            F32       blur_size = RenderShadowBlurSize;
            F32       dist_factor = RenderShadowBlurDistFactor;

            // [BDMerge A1.2] scale the shared shadow+SSAO soften blur kernel so a high-res
            // snapshot keeps the same blur weighting it had at window resolution; this pass
            // softens both the sun shadow and (when RenderDeferredSSAO is on, as gated
            // above) SSAO together, matching BD's donor which scaled both together too.
            // See bdmerge_snapshot_autoscale_multiplier() above.
            blur_size *= bdmerge_snapshot_autoscale_multiplier();

            // sample symmetrically with the middle sample falling exactly on 0.0
            F32 x = 0.f;

            LLVector3 gauss[32];  // xweight, yweight, offset

            for (U32 i = 0; i < kern_length; i++)
            {
                gauss[i].mV[0] = llgaussian(x, go.mV[0]);
                gauss[i].mV[1] = llgaussian(x, go.mV[1]);
                gauss[i].mV[2] = x;
                x += 1.f;
            }

            gDeferredBlurLightProgram.uniform2f(sDelta, 1.f, 0.f);
            gDeferredBlurLightProgram.uniform1f(sDistFactor, dist_factor);
            gDeferredBlurLightProgram.uniform3fv(sKern, kern_length, gauss[0].mV);
            gDeferredBlurLightProgram.uniform1f(sKernScale, blur_size * (kern_length / 2.f - 0.5f));

            {
                LLGLDisable   blend(GL_BLEND);
                LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_ALWAYS);
                mScreenTriangleVB->setBuffer();
                mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
            }

            screen_target->flush();
            unbindDeferredShader(gDeferredBlurLightProgram);

            bindDeferredShader(gDeferredBlurLightProgram, screen_target);

            bindPrismLensTarget(*deferred_light_target);

            gDeferredBlurLightProgram.uniform2f(sDelta, 0.f, 1.f);

            {
                LLGLDisable   blend(GL_BLEND);
                LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_ALWAYS);
                mScreenTriangleVB->setBuffer();
                mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
            }
            deferred_light_target->flush();
            unbindDeferredShader(gDeferredBlurLightProgram);
        }

        bindPrismLensTarget(*screen_target);
        static LLCachedControl<bool> visible_diffuse(gSavedSettings, "RenderVisibleDiffuseSidecar", false);
        // isComplete() is NOT optional. Without it a shader that fails to
        // compile still reaches bindDeferredShader(), which asserts on
        // mProgramObject == 0 and takes the viewer down at world init -- which
        // is exactly what happened: visibleDiffuseSeedF.glsl failed to compile
        // and the crash was the ASSERT, not the shader. Same guard the velocity
        // camera-fallback pass already uses (gVelocityCameraProgram.isComplete()).
        // A missing program must degrade to "sidecar not published", never to a
        // crash, because a shader can always fail on someone else's driver.
        const bool publish_visible_diffuse =
            visible_diffuse &&
            gVisibleDiffuseSeedProgram.isComplete() &&
            // Mirror renderGeomPostDeferred's predicate EXACTLY. Today the
            // main-view restriction is implied, because only mMainRT is given a
            // second attachment -- but the two predicates feed the two halves
            // of one readiness latch, and if they ever diverge the latch can be
            // seeded by one render invocation and resolved by another, which
            // publishes VALID for a frame that was never coherently produced.
            // Stating it explicitly costs nothing and removes the coupling to
            // an allocation detail elsewhere in the file. (S1)
            mRT == &mMainRT &&
            !gCubeSnapshot && !LLPipeline::sRenderingHUDs && !sImpostorRender &&
            screen_target == &mRT->screen &&
            LLRenderTarget::getCurrentBoundTarget() == &mRT->screen &&
            screen_target->getNumTextures() > SL_COVERAGE_ATTACHMENT;
        if (publish_visible_diffuse)
        {
            // The sidecar must survive the beauty clear; coverage must NOT --
            // it has to start at (0,0) every frame or last frame's forward
            // coverage would mark pixels the sidecar no longer owns. The engine
            // clear colour is glClearColor(1,0,1,1) elsewhere in the frame, so
            // coverage is cleared explicitly here rather than trusted to it.
            glColorMaski(SL_SIDECAR_ATTACHMENT,  GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            glColorMaski(SL_COVERAGE_ATTACHMENT, GL_TRUE,  GL_TRUE,  GL_FALSE, GL_FALSE);
        }
        // clear color buffer here - zeroing alpha (glow) is important or it will accumulate against sky
        glClearColor(0, 0, 0, 0);
        screen_target->clear(GL_COLOR_BUFFER_BIT);

        if (publish_visible_diffuse)
        {
            // Raw indexed masks are safe HERE, without the LLRender guard used
            // by the pool loop, because this window contains no global
            // glColorMask/glBlendFunc: bindDeferredShader and
            // unbindDeferredShader were both audited and issue neither, and the
            // only draw between them is ours.
            //
            // INVARIANT: if either of those ever starts issuing a global mask
            // or blend call, attachment 0 is re-enabled here and the seed pass
            // writes its classification into the BEAUTY buffer. Route this
            // block through LLScopedIndexedDrawBufferGuard if that changes.
            LLGLDisable blend(GL_BLEND);
            LLGLDepthTest depth(GL_FALSE);
            LLGLEnable srgb(GL_FRAMEBUFFER_SRGB);
            glColorMaski(0, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            glColorMaski(SL_SIDECAR_ATTACHMENT,  GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glColorMaski(SL_COVERAGE_ATTACHMENT, GL_TRUE, GL_TRUE, GL_FALSE, GL_FALSE);
            bindDeferredShader(gVisibleDiffuseSeedProgram);
            mScreenTriangleVB->setBuffer();
            mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
            unbindDeferredShader(gVisibleDiffuseSeedProgram);
            glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glColorMaski(SL_SIDECAR_ATTACHMENT,  GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            glColorMaski(SL_COVERAGE_ATTACHMENT, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

            // Stage 1 of both readiness latches: deferred-opaque pixels are now
            // classified and coverage.r is defined. renderGeomPostDeferred
            // supplies stage 2 for both. (M4)
            LLReShadeBridge::instance().noteVisibleDiffuseSeeded();
            LLReShadeBridge::instance().noteSurfaceCoverageSeeded();
        }

        if (RenderDeferredAtmospheric)
        {  // apply sunlight contribution
            LLGLSLShader &soften_shader = gDeferredSoftenProgram;

            LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE("renderDeferredLighting - atmospherics");
            LL_PROFILE_GPU_ZONE("atmospherics");
            bindDeferredShader(soften_shader);

            static LLCachedControl<F32> ssao_scale(gSavedSettings, "RenderSSAOIrradianceScale", 0.5f);
            static LLCachedControl<F32> ssao_max(gSavedSettings, "RenderSSAOIrradianceMax", 0.25f);
            static LLStaticHashedString ssao_scale_str("ssao_irradiance_scale");
            static LLStaticHashedString ssao_max_str("ssao_irradiance_max");

            soften_shader.uniform1f(ssao_scale_str, ssao_scale);
            soften_shader.uniform1f(ssao_max_str, ssao_max);

            LLEnvironment &environment = LLEnvironment::instance();

            soften_shader.uniform1i(LLShaderMgr::SUN_UP_FACTOR, environment.getIsSunUp() ? 1 : 0);
            soften_shader.uniform3fv(LLShaderMgr::LIGHTNORM, 1, environment.getClampedLightNorm().mV);

            soften_shader.uniform4fv(LLShaderMgr::WATER_WATERPLANE, 1, LLDrawPoolAlpha::sWaterPlane.mV);

            {
                LLGLDepthTest depth(GL_FALSE);
                LLGLDisable   blend(GL_BLEND);

                // full screen blit
                mScreenTriangleVB->setBuffer();
                mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
            }

            unbindDeferredShader(gDeferredSoftenProgram);
        }

        static LLCachedControl<S32> local_light_count(gSavedSettings, "RenderLocalLightCount", 256);
        static LLCachedControl<S32> probe_level(gSavedSettings, "RenderReflectionProbeLevel", 0);

        if (local_light_count > 0 && (!gCubeSnapshot || probe_level > 0))
        {
            static std::vector<LLVector4>        fullscreen_lights;
            static LLDrawable::drawable_vector_t spot_lights;
            static LLDrawable::drawable_vector_t fullscreen_spot_lights;
            static std::vector<LLVector4>        light_colors;

            gGL.setSceneBlendType(LLRender::BT_ADD);
            LLSettingsSky::ptr_t        psky        = LLEnvironment::instance().getCurrentSky();

            if (!gCubeSnapshot && !sPrismLensRender)
            {
                for (U32 i = 0; i < MAX_SPOT_SHADOWS; i++)
                {
                    mTargetShadowSpotLight[i] = NULL;
                }
            }

            LLVertexBuffer::unbind();

            {
                LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE("renderDeferredLighting - local lights");
                LL_PROFILE_GPU_ZONE("local lights");
                bindDeferredShader(gDeferredLightProgram);

                if (mCubeVB.isNull())
                {
                    mCubeVB = ll_create_cube_vb(LLVertexBuffer::MAP_VERTEX);
                }

                mCubeVB->setBuffer();

                LLGLDepthTest depth(GL_TRUE, GL_FALSE);
                // mNearbyLights already includes distance calculation and excludes muted avatars.
                // It is calculated from mLights
                // mNearbyLights also provides fade value to gracefully fade-out out of range lights
                S32 count = 0;
                for (light_set_t::iterator iter = mNearbyLights.begin(); iter != mNearbyLights.end(); ++iter)
                {
                    count++;
                    if (count > local_light_count)
                    { //stop collecting lights once we hit the limit
                        break;
                    }

                    LLDrawable * drawablep = iter->drawable;
                    LLVOVolume * volume = drawablep->getVOVolume();
                    if (!volume)
                    {
                        continue;
                    }

                    if (volume->isAttachment())
                    {
                        if (!sRenderAttachedLights)
                        {
                            continue;
                        }

                        // [BDMerge A5.6] independent own/other attached-light toggles
                        if (!bdmerge_should_render_light(true, volume->getAvatar() == gAgentAvatarp))
                        {
                            continue;
                        }
                    }
                    // [BDMerge A5.6] independent world-light toggle
                    else if (!bdmerge_should_render_light(false, false))
                    {
                        continue;
                    }

                    LLVector4a center;
                    center.load3(drawablep->getPositionAgent().mV);
                    const F32 *c = center.getF32ptr();
                    F32        s = volume->getLightRadius() * 1.5f;

                    // send light color to shader in linear space
                    LLColor3 col = volume->getLightLinearColor() * light_scale;

                    if (col.magVecSquared() < 0.001f)
                    {
                        continue;
                    }

                    if (s <= 0.001f)
                    {
                        continue;
                    }

                    LLVector4a sa;
                    sa.splat(s);
                    if (camera->AABBInFrustumNoFarClip(center, sa) == 0)
                    {
                        continue;
                    }

                    sVisibleLightCount++;

                    if (camera->getOrigin().mV[0] > c[0] + s + 0.2f || camera->getOrigin().mV[0] < c[0] - s - 0.2f ||
                        camera->getOrigin().mV[1] > c[1] + s + 0.2f || camera->getOrigin().mV[1] < c[1] - s - 0.2f ||
                        camera->getOrigin().mV[2] > c[2] + s + 0.2f || camera->getOrigin().mV[2] < c[2] - s - 0.2f)
                    {  // draw box if camera is outside box
                        if (volume->isLightSpotlight() && bdmerge_should_render_projector()) // [BDMerge A5.6] projector toggle: falls back to a regular box light below when off
                        {
                            // Priority is persistent LLVOVolume state used by
                            // next-frame main-view shadow-slot assignment.
                            if (!sPrismLensRender)
                            {
                                drawablep->getVOVolume()->updateSpotLightPriority();
                            }
                            spot_lights.push_back(drawablep);
                            continue;
                        }

                        gDeferredLightProgram.uniform3fv(LLShaderMgr::LIGHT_CENTER, 1, c);
                        gDeferredLightProgram.uniform1f(LLShaderMgr::LIGHT_SIZE, s);
                        gDeferredLightProgram.uniform3fv(LLShaderMgr::DIFFUSE_COLOR, 1, col.mV);
                        gDeferredLightProgram.uniform1f(LLShaderMgr::LIGHT_FALLOFF, volume->getLightFalloff(DEFERRED_LIGHT_FALLOFF));
                        gDeferredLightProgram.uniform1i(LLShaderMgr::CLASSIC_MODE, (psky->canAutoAdjust()) ? 1 : 0);

                        gGL.syncMatrices();

                        mCubeVB->drawRange(LLRender::TRIANGLE_FAN, 0, 7, 8, get_box_fan_indices(camera, center));
                    }
                    else
                    {
                        if (volume->isLightSpotlight() && bdmerge_should_render_projector()) // [BDMerge A5.6] projector toggle: falls back to a fullscreen point light below when off
                        {
                            if (!sPrismLensRender)
                            {
                                drawablep->getVOVolume()->updateSpotLightPriority();
                            }
                            fullscreen_spot_lights.push_back(drawablep);
                            continue;
                        }

                        glm::vec3 tc(center);
                        tc = mul_mat4_vec3(mat, tc);

                        fullscreen_lights.push_back(LLVector4(tc.x, tc.y, tc.z, s));
                        light_colors.push_back(LLVector4(col.mV[0], col.mV[1], col.mV[2], volume->getLightFalloff(DEFERRED_LIGHT_FALLOFF)));
                    }
                }

                // Bookmark comment to allow searching for mSpecialRenderMode == 3 (avatar edit mode),
                // prev site of appended deferred character light, removed by SL-13522 09/20

                unbindDeferredShader(gDeferredLightProgram);
            }

            if (!spot_lights.empty())
            {
                LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE("renderDeferredLighting - projectors");
                LL_PROFILE_GPU_ZONE("projectors");
                LLGLDepthTest depth(GL_TRUE, GL_FALSE);
                bindDeferredShader(gDeferredSpotLightProgram);

                mCubeVB->setBuffer();

                gDeferredSpotLightProgram.enableTexture(LLShaderMgr::DEFERRED_PROJECTION);

                for (LLDrawable* drawablep : spot_lights)
                {
                    LLVOVolume *volume = drawablep->getVOVolume();

                    LLVector4a center;
                    center.load3(drawablep->getPositionAgent().mV);
                    const F32* c = center.getF32ptr();
                    F32        s = volume->getLightRadius() * 1.5f;

                    sVisibleLightCount++;

                    setupSpotLight(gDeferredSpotLightProgram, drawablep);

                    // send light color to shader in linear space
                    LLColor3 col = volume->getLightLinearColor() * light_scale;

                    gDeferredSpotLightProgram.uniform3fv(LLShaderMgr::LIGHT_CENTER, 1, c);
                    gDeferredSpotLightProgram.uniform1f(LLShaderMgr::LIGHT_SIZE, s);
                    gDeferredSpotLightProgram.uniform3fv(LLShaderMgr::DIFFUSE_COLOR, 1, col.mV);
                    gDeferredSpotLightProgram.uniform1f(LLShaderMgr::LIGHT_FALLOFF, volume->getLightFalloff(DEFERRED_LIGHT_FALLOFF));
                    gDeferredSpotLightProgram.uniform1i(LLShaderMgr::CLASSIC_MODE, (psky->canAutoAdjust()) ? 1 : 0);

                    gGL.syncMatrices();

                    mCubeVB->drawRange(LLRender::TRIANGLE_FAN, 0, 7, 8, get_box_fan_indices(camera, center));
                }
                gDeferredSpotLightProgram.disableTexture(LLShaderMgr::DEFERRED_PROJECTION);
                unbindDeferredShader(gDeferredSpotLightProgram);
            }

            {
                LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE("renderDeferredLighting - fullscreen lights");
                LLGLDepthTest depth(GL_FALSE);
                LL_PROFILE_GPU_ZONE("fullscreen lights");

                U32 count = 0;
                U32 total_count = 0;

                const U32 max_count = LL_DEFERRED_MULTI_LIGHT_COUNT;
                LLVector4 light[max_count];
                LLVector4 col[max_count];

                F32 far_z = 0.f;

                for (size_t i = 0, num_fullscreen_lights = fullscreen_lights.size(); i < num_fullscreen_lights; ++i)
                {
                    light[count] = fullscreen_lights[i];
                    col[count] = light_colors[i];

                    far_z = llmin(light[count].mV[2] - light[count].mV[3], far_z);
                    count++;
                    total_count++;
                    if (count == max_count || total_count == num_fullscreen_lights)
                    {
                        U32 idx = count - 1;
                        bindDeferredShader(gDeferredMultiLightProgram[idx]);
                        gDeferredMultiLightProgram[idx].uniform1i(LLShaderMgr::MULTI_LIGHT_COUNT, count);
                        gDeferredMultiLightProgram[idx].uniform4fv(LLShaderMgr::MULTI_LIGHT, count, light[0].mV);
                        gDeferredMultiLightProgram[idx].uniform4fv(LLShaderMgr::MULTI_LIGHT_COL, count, col[0].mV);
                        gDeferredMultiLightProgram[idx].uniform1f(LLShaderMgr::MULTI_LIGHT_FAR_Z, far_z);
                        gDeferredMultiLightProgram[idx].uniform1i(LLShaderMgr::CLASSIC_MODE, (psky->canAutoAdjust()) ? 1 : 0);
                        far_z = 0.f;
                        count = 0;
                        mScreenTriangleVB->setBuffer();
                        mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
                        unbindDeferredShader(gDeferredMultiLightProgram[idx]);
                    }
                }

                bindDeferredShader(gDeferredMultiSpotLightProgram);

                gDeferredMultiSpotLightProgram.enableTexture(LLShaderMgr::DEFERRED_PROJECTION);

                mScreenTriangleVB->setBuffer();

                for (LLDrawable* drawablep : fullscreen_spot_lights)
                {
                    LLVOVolume* volume = drawablep->getVOVolume();
                    LLVector3   center = drawablep->getPositionAgent();
                    F32         light_size_final = volume->getLightRadius() * 1.5f;
                    F32         light_falloff_final = volume->getLightFalloff(DEFERRED_LIGHT_FALLOFF);

                    sVisibleLightCount++;

                    glm::vec3 tc(center);
                    tc = mul_mat4_vec3(mat, tc);

                    setupSpotLight(gDeferredMultiSpotLightProgram, drawablep);

                    // send light color to shader in linear space
                    LLColor3 col = volume->getLightLinearColor() * light_scale;

                    gDeferredMultiSpotLightProgram.uniform3fv(LLShaderMgr::LIGHT_CENTER, 1, glm::value_ptr(tc));
                    gDeferredMultiSpotLightProgram.uniform1f(LLShaderMgr::LIGHT_SIZE, light_size_final);
                    gDeferredMultiSpotLightProgram.uniform3fv(LLShaderMgr::DIFFUSE_COLOR, 1, col.mV);
                    gDeferredMultiSpotLightProgram.uniform1f(LLShaderMgr::LIGHT_FALLOFF, light_falloff_final);
                    gDeferredMultiSpotLightProgram.uniform1i(LLShaderMgr::CLASSIC_MODE, (psky->canAutoAdjust()) ? 1 : 0);

                    mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
                }

                gDeferredMultiSpotLightProgram.disableTexture(LLShaderMgr::DEFERRED_PROJECTION);
                unbindDeferredShader(gDeferredMultiSpotLightProgram);
            }

            // Clear does not free internal vector storage, so this is more efficient than creating new vectors each frame
            fullscreen_lights.clear();
            spot_lights.clear();
            fullscreen_spot_lights.clear();
            light_colors.clear();
        }

        gGL.setColorMask(true, true);
    }

    {  // render non-deferred geometry (alpha, fullbright, glow)
        LLGLDisable blend(GL_BLEND);

        pushRenderTypeMask();
        andRenderTypeMask(LLPipeline::RENDER_TYPE_ALPHA,
                          LLPipeline::RENDER_TYPE_ALPHA_PRE_WATER,
                          LLPipeline::RENDER_TYPE_ALPHA_POST_WATER,
                          LLPipeline::RENDER_TYPE_FULLBRIGHT,
                          LLPipeline::RENDER_TYPE_VOLUME,
                          LLPipeline::RENDER_TYPE_GLOW,
                          LLPipeline::RENDER_TYPE_BUMP,
                          LLPipeline::RENDER_TYPE_GLTF_PBR,
                          LLPipeline::RENDER_TYPE_PASS_SIMPLE,
                          LLPipeline::RENDER_TYPE_PASS_ALPHA,
                          LLPipeline::RENDER_TYPE_PASS_ALPHA_MASK,
                          LLPipeline::RENDER_TYPE_PASS_BUMP,
                          LLPipeline::RENDER_TYPE_PASS_POST_BUMP,
                          LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT,
                          LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK,
                          LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_SHINY,
                          LLPipeline::RENDER_TYPE_PASS_GLOW,
                          LLPipeline::RENDER_TYPE_PASS_GLTF_GLOW,
                          LLPipeline::RENDER_TYPE_PASS_GRASS,
                          LLPipeline::RENDER_TYPE_PASS_SHINY,
                          LLPipeline::RENDER_TYPE_PASS_INVISIBLE,
                          LLPipeline::RENDER_TYPE_PASS_INVISI_SHINY,
                          LLPipeline::RENDER_TYPE_AVATAR,
                          LLPipeline::RENDER_TYPE_CONTROL_AV,
                          LLPipeline::RENDER_TYPE_ALPHA_MASK,
                          LLPipeline::RENDER_TYPE_FULLBRIGHT_ALPHA_MASK,
                          LLPipeline::RENDER_TYPE_TERRAIN,
                          LLPipeline::RENDER_TYPE_WATER,
                          LLPipeline::RENDER_TYPE_WATEREXCLUSION,
                          END_RENDER_TYPES);

        // [GhostDeferred] Solid Completion Rescue: draw the clone's fullbright /
        // fullbright-shiny / fullbright-mask rigged faces (which cannot enter the
        // G-buffer) here in the post-deferred forward pass, so metallic-gold /
        // fullbright wardrobes render scene-lit instead of forcing the whole clone
        // to the fallback overlay. Solids run BEFORE stock post-deferred (opaque
        // order; each writes depth) so world alpha still composites correctly over
        // them. (Two-hook early-testing layout; the later alpha slice moves these
        // to true stock pool boundaries.)
        const LLCamera& ghost_camera = *LLViewerCamera::getInstance();
        if (ghostPostDeferredSolidsPending(ghost_camera))
        {
            // The local-light accumulation above exits with BT_ADD blend factors,
            // mScreenTriangleVB still bound, and a non-null gGLLastMatrix. The
            // forward stages END at the stock post-deferred canonical state
            // (BT_ALPHA, unbound VB, camera modelview, gGLLastMatrix=null). Set
            // that canonical state HERE, before the invariant snapshot, so the
            // stages return to exactly what was snapshotted (else the invariant
            // reports a false leak on every local-light frame). Guarded by the
            // pending check so an OFF / no-clone frame changes NO state.
            gGL.setSceneBlendType(LLRender::BT_ALPHA);
            LLVertexBuffer::unbind();
            gGL.matrixMode(LLRender::MM_MODELVIEW);
            gGLLastMatrix = nullptr;
            gGL.loadMatrix(gGLModelView);
            gGL.syncMatrices();

            LLScopedGhostRenderInvariant ghost_invariant(
                "renderGhostPostDeferredSolids",
                &LLActorMover::instance().ghostDeferredCounters().mInvariantViolations);
            renderGhostPostDeferred(ghost_camera, EGhostForwardStage::FULLBRIGHT_OPAQUE);
            renderGhostPostDeferred(ghost_camera, EGhostForwardStage::FULLBRIGHT_SHINY);
            renderGhostPostDeferred(ghost_camera, EGhostForwardStage::FULLBRIGHT_MASKED);
            ghost_invariant.finish();
        }

        renderGeomPostDeferred(*LLViewerCamera::getInstance());

        // [GhostDeferred] FINALIZE (bookkeeping only): promote RIGGED_SOLID for
        // clones whose G-buffer + forward solid draws all succeeded, so the
        // overlay suppresses exactly the covered solids. Runs UNCONDITIONALLY (no
        // GL op, self-guarded) -- a clone with only G-buffer solids (no fullbright
        // content) has no pending forward stage but must still be finalized. Must
        // settle BEFORE the blend slice evaluates its RIGGED_SOLID prerequisite.
        renderGhostPostDeferred(ghost_camera, EGhostForwardStage::FINALIZE);

        // [GhostDeferred] Rigged forward-alpha parity. Stock post-deferred alpha
        // (renderGeomPostDeferred, above) has already prepared the real alpha
        // shaders and applied deferred lighting; replay the harvested blend batches
        // at the ghost transform before the render-type mask is popped and the
        // screen target is flushed. The read-only pending probe keeps a true GL
        // no-op when submission is disabled or no blend category exists this frame.
        if (ghostPostDeferredBlendPending(ghost_camera))
        {
            LLScopedGhostRenderInvariant ghost_blend_invariant(
                "renderGhostPostDeferredRiggedBlend",
                &LLActorMover::instance().ghostDeferredCounters().mInvariantViolations);
            renderGhostPostDeferred(ghost_camera, EGhostForwardStage::RIGGED_BLEND);
            ghost_blend_invariant.finish();
        }

        popRenderTypeMask();
    }

    // Composite every current visible Prism face from its independently retained
    // linear-HDR beauty immediately before the main screen flush. Fixed-
    // function depth provides opaque foreground occlusion; transparent pool
    // ordering remains explicitly out of scope for this slice.
    // [Prism camera feed - recursive mirror] During the Prism aux (VCam) capture
    // (sPrismLensRender), select the auxiliary composite path so the monitor faces
    // show the PREVIOUS frame's retained feed while this aux frame is being drawn -
    // a 1-frame-lagged recursive tunnel. Outside the aux (the main view, the only
    // time sPrismLensRender is false here) this calls getCompositeStates exactly as
    // before, so the main-view composite is byte-identical. getAuxCompositeStates
    // is itself gated on sPrismLensRender and on the bound target being the active
    // aux pack screen, so it can never affect the main view even if reached.
    LLPrismLens::CompositeState prism_states[LLPrismLens::MAX_DISPLAY_BINDINGS];
    const U32 prism_state_count = gPrismLensProgram.isComplete()
        ? llmin(sPrismLensRender
                    ? LLPrismLens::getAuxCompositeStates(
                          screen_target, prism_states,
                          LLPrismLens::MAX_DISPLAY_BINDINGS)
                    : LLPrismLens::getCompositeStates(
                          screen_target, prism_states,
                          LLPrismLens::MAX_DISPLAY_BINDINGS),
                LLPrismLens::MAX_DISPLAY_BINDINGS)
        : 0;
    if (prism_state_count > 0)
    {
        struct ScopedPrismCompositeRestore
        {
            ScopedPrismCompositeRestore()
                : mSavedShader(LLGLSLShader::sCurBoundShaderPtr),
                  mSavedMatrixMode(gGL.getMatrixMode()),
                  mSavedScissorEnabled(glIsEnabled(GL_SCISSOR_TEST))
            {
                glGetIntegerv(GL_SCISSOR_BOX, mSavedScissorBox);
                glGetBooleanv(GL_COLOR_WRITEMASK, mSavedColorMask);
            }

            ~ScopedPrismCompositeRestore()
            {
                if (mFaceMatrixPushed)
                {
                    gGL.matrixMode(LLRender::MM_MODELVIEW);
                    gGL.popMatrix();
                }
                gGL.matrixMode(mSavedMatrixMode);
                gGL.syncMatrices();

                gPrismLensProgram.disableTexture(LLShaderMgr::PRISM_LENS_MAP);
                LLVertexBuffer::unbind();
                if (mSavedShader && mSavedShader->isComplete())
                {
                    mSavedShader->bind();
                }
                else
                {
                    LLGLSLShader::unbind();
                }

                glScissor(mSavedScissorBox[0], mSavedScissorBox[1],
                          mSavedScissorBox[2], mSavedScissorBox[3]);
                if (mSavedScissorEnabled)
                {
                    glEnable(GL_SCISSOR_TEST);
                }
                else
                {
                    glDisable(GL_SCISSOR_TEST);
                }
                gGL.setColorMask(mSavedColorMask[0] != GL_FALSE,
                                 mSavedColorMask[1] != GL_FALSE,
                                 mSavedColorMask[2] != GL_FALSE,
                                 mSavedColorMask[3] != GL_FALSE);
            }

            void pushFaceMatrix(LLDrawable* drawable)
            {
                gGL.matrixMode(LLRender::MM_MODELVIEW);
                gGL.pushMatrix();
                mFaceMatrixPushed = true;

                // Match LLVolumeGeometryManager::registerFace() and
                // LLRenderPass::applyModelMatrix(): face vertices are in the
                // drawable space selected below, and the object matrix must be
                // applied to the camera model-view -- never to the matrix left
                // behind by the preceding deferred draw.
                gGL.loadMatrix(gGLModelView);
                if (drawable->isState(LLDrawable::ANIMATED_CHILD))
                {
                    gGL.multMatrix((GLfloat*)drawable->getWorldMatrix().mMatrix);
                }
                else if (drawable->isActive())
                {
                    gGL.multMatrix((GLfloat*)drawable->getRenderMatrix().mMatrix);
                }
                else
                {
                    gGL.multMatrix((GLfloat*)drawable->getRegion()->mRenderMatrix.mMatrix);
                }
                gGL.syncMatrices();
            }

            // Prim-free virtual screen: push the BASE world model-view with NO
            // face matrix applied. The quad vertices are already in world/agent
            // space (the same space gGLModelView maps to eye space), so unlike
            // pushFaceMatrix() there is no per-object multiply. popFaceMatrix()
            // restores it exactly as for a real face.
            void pushWorldMatrix()
            {
                gGL.matrixMode(LLRender::MM_MODELVIEW);
                gGL.pushMatrix();
                mFaceMatrixPushed = true;
                gGL.loadMatrix(gGLModelView);
                gGL.syncMatrices();
            }

            void popFaceMatrix()
            {
                if (mFaceMatrixPushed)
                {
                    gGL.matrixMode(LLRender::MM_MODELVIEW);
                    gGL.popMatrix();
                    mFaceMatrixPushed = false;
                    gGL.syncMatrices();
                }
            }

            LLGLSLShader* mSavedShader;
            LLRender::eMatrixMode mSavedMatrixMode;
            GLboolean mSavedScissorEnabled;
            GLint mSavedScissorBox[4];
            GLboolean mSavedColorMask[4];
            bool mFaceMatrixPushed = false;
        };

        // One complete restore scope covers the batch. getCompositeStates()
        // groups siblings by capture slot, so all displays of one feed can
        // share a texture bind while retaining per-face matrices and uniforms.
        ScopedPrismCompositeRestore scoped_composite_restore;
        LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_LEQUAL);
        LLGLDisable blend(GL_BLEND);
        LLGLDisable cull(GL_CULL_FACE); // Displays are deliberately two-sided.
        gGL.setColorMask(true, true);
        glEnable(GL_SCISSOR_TEST);

        gPrismLensProgram.bind();
        const S32 prism_channel =
            gPrismLensProgram.enableTexture(LLShaderMgr::PRISM_LENS_MAP);
        if (prism_channel >= 0)
        {
            static const LLStaticHashedString sSurfaceOrigin("surfaceOrigin");
            static const LLStaticHashedString sSurfaceUDual("surfaceUDual");
            static const LLStaticHashedString sSurfaceVDual("surfaceVDual");
            static const LLStaticHashedString sDisplayToCaptureScale(
                "displayToCaptureScale");
            static const LLStaticHashedString sDisplayToCaptureOffset(
                "displayToCaptureOffset");
            static const LLStaticHashedString sRetainedOrientationScale(
                "retainedOrientationScale");
            static const LLStaticHashedString sRetainedOrientationOffset(
                "retainedOrientationOffset");
            static const LLStaticHashedString sTextureRegionScale(
                "textureRegionScale");
            static const LLStaticHashedString sTextureRegionOffset(
                "textureRegionOffset");
            static const LLStaticHashedString sLetterbox("letterbox");
            static const LLStaticHashedString sBarColorLinear("barColorLinear");
            static const LLStaticHashedString sEdgeFeather("edgeFeather");
            static const LLStaticHashedString sPrismLensOptics("prismLensOptics");
            static const LLStaticHashedString sScreenEffect0("screenEffect0");
            static const LLStaticHashedString sScreenEffect1("screenEffect1");
            static const LLStaticHashedString sScreenEffect2("screenEffect2");
            static const LLStaticHashedString sScreenEffect3("screenEffect3");
            static const LLStaticHashedString sScreenEffect4("screenEffect4");
            static const LLStaticHashedString sScreenEffectTime("screenEffectTime");
            static const LLStaticHashedString sPrismEnvSheenSky("prismEnvSheenSky");
            static const LLStaticHashedString sPrismEnvSheenGround(
                "prismEnvSheenGround");

            // Animated screen effects advance on the shared frame clock, and
            // the fmodf wrap keeps the uniform small so long sessions never
            // lose float precision (the FROXEL_TIME / PROJVOL_TIME pattern).
            // The clock is capture-independent, so upload it once per batch.
            gPrismLensProgram.uniform1f(sScreenEffectTime,
                                        fmodf(gFrameTimeSeconds, 3600.f));

            // Environmental sheen palette (Feature B). A two-color hemisphere
            // (sky/ground) approximating the room environment for the glossy
            // Fresnel reflection in prismLensF.glsl. Derived from the live sky
            // ambient/horizon so the sheen tracks the scene mood (warm at
            // sunset, cool/dim at night). This is global to the batch (not
            // per-display, like the screen-effect clock), so it is uploaded
            // once here rather than through the CompositeState builders; the
            // per-display strength lives in screenEffect4.w. Only the shader's
            // sheen>0 branch reads these, so they never affect the default
            // (byte-identical) composite. Constants are creative tuning.
            {
                const LLSettingsSky::ptr_t psky =
                    LLEnvironment::instance().getCurrentSky();
                LLColor3 sheen_sky(0.f, 0.f, 0.f);
                LLColor3 sheen_ground(0.f, 0.f, 0.f);
                if (psky)
                {
                    const LLColor3 ambient = psky->getAmbientColor();
                    const LLColor3 horizon = psky->getBlueHorizon();
                    sheen_sky = ambient * 1.5f + horizon * 0.4f;
                    sheen_ground = ambient * 0.5f;
                }
                const F32 sheen_sky_v[3] = {
                    sheen_sky.mV[0], sheen_sky.mV[1], sheen_sky.mV[2] };
                const F32 sheen_ground_v[3] = {
                    sheen_ground.mV[0], sheen_ground.mV[1], sheen_ground.mV[2] };
                gPrismLensProgram.uniform3fv(sPrismEnvSheenSky, 1, sheen_sky_v);
                gPrismLensProgram.uniform3fv(sPrismEnvSheenGround, 1,
                                             sheen_ground_v);
            }

            U32 bound_capture_slot = LLPrismLens::MAX_CAPTURES;
            for (U32 prism_index = 0; prism_index < prism_state_count;
                 ++prism_index)
            {
                const LLPrismLens::CompositeState& prism_state =
                    prism_states[prism_index];
                // A prim-free virtual screen has no face; it is drawn as a
                // world-space quad below. Only a real-face state requires mFace.
                if (prism_state.mCaptureSlot >= LLPrismLens::MAX_CAPTURES ||
                    (!prism_state.mFace && !prism_state.mVirtual) ||
                    prism_state.mScissor[2] <= 0 ||
                    prism_state.mScissor[3] <= 0 ||
                    !mPrismLensOutput[prism_state.mCaptureSlot].isComplete())
                {
                    continue;
                }

                if (bound_capture_slot != prism_state.mCaptureSlot)
                {
                    mPrismLensOutput[prism_state.mCaptureSlot].bindTexture(
                        0, prism_channel, LLTexUnit::TFO_BILINEAR);
                    bound_capture_slot = prism_state.mCaptureSlot;
                }

                glScissor(prism_state.mScissor[0], prism_state.mScissor[1],
                          prism_state.mScissor[2], prism_state.mScissor[3]);
                gPrismLensProgram.uniform3fv(sSurfaceOrigin, 1, prism_state.mSurfaceOrigin);
                gPrismLensProgram.uniform3fv(sSurfaceUDual, 1, prism_state.mSurfaceUDual);
                gPrismLensProgram.uniform3fv(sSurfaceVDual, 1, prism_state.mSurfaceVDual);
                gPrismLensProgram.uniform2fv(sDisplayToCaptureScale, 1,
                                             prism_state.mDisplayToCaptureScale);
                gPrismLensProgram.uniform2fv(sDisplayToCaptureOffset, 1,
                                             prism_state.mDisplayToCaptureOffset);
                gPrismLensProgram.uniform2fv(sRetainedOrientationScale, 1,
                                             prism_state.mRetainedOrientationScale);
                gPrismLensProgram.uniform2fv(sRetainedOrientationOffset, 1,
                                             prism_state.mRetainedOrientationOffset);
                gPrismLensProgram.uniform2fv(sTextureRegionScale, 1,
                                             prism_state.mTextureRegionScale);
                gPrismLensProgram.uniform2fv(sTextureRegionOffset, 1,
                                             prism_state.mTextureRegionOffset);
                gPrismLensProgram.uniform1i(sLetterbox, prism_state.mLetterbox);
                gPrismLensProgram.uniform3fv(sBarColorLinear, 1,
                                             prism_state.mBarColorLinear);
                gPrismLensProgram.uniform1f(sEdgeFeather, prism_state.mEdgeFeather);
                gPrismLensProgram.uniform4fv(sPrismLensOptics, 1, prism_state.mOpticsParams);
                gPrismLensProgram.uniform4fv(sScreenEffect0, 1, prism_state.mScreenEffect0);
                gPrismLensProgram.uniform4fv(sScreenEffect1, 1, prism_state.mScreenEffect1);
                gPrismLensProgram.uniform4fv(sScreenEffect2, 1, prism_state.mScreenEffect2);
                gPrismLensProgram.uniform4fv(sScreenEffect3, 1, prism_state.mScreenEffect3);
                gPrismLensProgram.uniform4fv(sScreenEffect4, 1, prism_state.mScreenEffect4);

                if (prism_state.mFace)
                {
                    LLDrawable* lens_drawable = prism_state.mFace->getDrawable();
                    if (lens_drawable &&
                        (lens_drawable->isActive() || lens_drawable->getRegion()))
                    {
                        scoped_composite_restore.pushFaceMatrix(lens_drawable);
                        prism_state.mFace->renderIndexed();
                        scoped_composite_restore.popFaceMatrix();
                    }
                }
                else if (prism_state.mVirtual)
                {
                    // Prim-free virtual screen: two triangles from the stored
                    // world-space corners (TL, TR, BR, BL), drawn under the base
                    // world model-view with the SAME gPrismLensProgram bound. The
                    // vertex shader reads only `position` (gGL immediate mode
                    // supplies it as ATTRIBUTE_POSITION) and derives prism_uv from
                    // the world-space surface uniforms already uploaded above, so
                    // no shader change is needed. The batch runs under
                    // LLGLDisable cull(GL_CULL_FACE), so the quad is two-sided --
                    // matching a real display face.
                    const F32 (&c)[4][3] = prism_state.mVirtualCorners;
                    scoped_composite_restore.pushWorldMatrix();
                    gGL.begin(LLRender::TRIANGLES);
                    gGL.vertex3fv(c[0]); // TL
                    gGL.vertex3fv(c[1]); // TR
                    gGL.vertex3fv(c[2]); // BR
                    gGL.vertex3fv(c[0]); // TL
                    gGL.vertex3fv(c[2]); // BR
                    gGL.vertex3fv(c[3]); // BL
                    gGL.end();
                    gGL.flush();
                    scoped_composite_restore.popFaceMatrix();
                }
            }
        }
    }

    screen_target->flush();

    // [BDMerge A5.4-1a] Velocity / motion-vector pass. Runs after the opaque +
    // post-deferred geometry and BEFORE the last-frame matrix snapshot below (so it
    // reads gGLLastModelView == the PREVIOUS frame's camera). Guarded off by default
    // and never in cube snapshots / reflection probes. [A5.4-3] Motion blur
    // consumes the buffer, so it also forces the pass.
    if ((BDMergeVelocityBuffer || BDMergeMotionBlur) &&
        !gCubeSnapshot && !sPrismLensRender)
    {
        renderGeomVelocity();
    }

    if (!gCubeSnapshot && !sPrismLensRender)
    {
        // this is the end of the 3D scene render, grab a copy of the modelview and projection
        // matrix for use in off-by-one-frame effects in the next frame
        for (U32 i = 0; i < 16; i++)
        {
            gGLLastModelView[i] = gGLModelView[i];
            gGLLastProjection[i] = gGLProjection[i];
            // Snapshot the un-jittered velocity projection at the same scene
            // boundary as the previous modelview.
            sLastVelocityProjMat[i] = mVelocityProjMat[i];
        }
        sHasLastVelocityProjection = true;
    }
    // NOTE: deliberately no else-branch. Cube/probe passes do not advance the
    // main-view history, so there is nothing to invalidate -- the snapshot from
    // the last main-view frame is still the correct previous projection. An
    // earlier version cleared the latch here, but reflection probes update
    // continuously, so that published SLRESHADE_RESET_PROJECTION_CHANGE on
    // essentially every frame and left temporal consumers (TAA, denoising)
    // permanently reset.
    gGL.setColorMask(true, true);
}

void LLPipeline::doAtmospherics()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;

    if (sImpostorRender)
    { // do not attempt atmospherics on impostors
        return;
    }

    if (RenderDeferredAtmospheric)
    {
        {
            // copy depth buffer for use in haze shader (use water displacement map as temp storage)
            LLGLDepthTest depth(GL_TRUE, GL_TRUE, GL_ALWAYS);

            LLRenderTarget& src = gPipeline.mRT->screen;
            LLRenderTarget& dst = getWaterDisTarget();

            U32 source_width = src.getWidth();
            U32 source_height = src.getHeight();
            src.flush();
            dst.copyContents(src, 0, 0, source_width, source_height,
                             0, 0, dst.getWidth(), dst.getHeight(),
                             GL_DEPTH_BUFFER_BIT, GL_NEAREST);
            bindPrismLensTarget(mRT->screen);
        }

        LLGLEnable blend(GL_BLEND);
        gGL.blendFunc(LLRender::BF_ONE, LLRender::BF_SOURCE_ALPHA, LLRender::BF_ZERO, LLRender::BF_SOURCE_ALPHA);
        gGL.setColorMask(true, true);

        // apply haze
        LLGLSLShader& haze_shader = gHazeProgram;

        LL_PROFILE_GPU_ZONE("haze");
        bindDeferredShader(haze_shader, nullptr, &getWaterDisTarget());

        LLEnvironment& environment = LLEnvironment::instance();
        haze_shader.uniform1i(LLShaderMgr::SUN_UP_FACTOR, environment.getIsSunUp() ? 1 : 0);
        haze_shader.uniform3fv(LLShaderMgr::LIGHTNORM, 1, environment.getClampedLightNorm().mV);

        haze_shader.uniform4fv(LLShaderMgr::WATER_WATERPLANE, 1, LLDrawPoolAlpha::sWaterPlane.mV);

        LLGLDepthTest depth(GL_FALSE);

        // full screen blit
        mScreenTriangleVB->setBuffer();
        mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);

        unbindDeferredShader(haze_shader);

        gGL.setSceneBlendType(LLRender::BT_ALPHA);
    }
}

void LLPipeline::doWaterHaze()
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    if (sImpostorRender)
    { // do not attempt water haze on impostors
        return;
    }

    if (RenderDeferredAtmospheric)
    {
        // copy depth buffer for use in haze shader (use water displacement map as temp storage)
        {
            LLGLDepthTest depth(GL_TRUE, GL_TRUE, GL_ALWAYS);

            LLRenderTarget& src = gPipeline.mRT->screen;
            LLRenderTarget& dst = getWaterDisTarget();

            U32 source_width = src.getWidth();
            U32 source_height = src.getHeight();
            src.flush();
            dst.copyContents(src, 0, 0, source_width, source_height,
                             0, 0, dst.getWidth(), dst.getHeight(),
                             GL_DEPTH_BUFFER_BIT, GL_NEAREST);
            bindPrismLensTarget(mRT->screen);
        }

        LLGLEnable blend(GL_BLEND);
        gGL.blendFunc(LLRender::BF_ONE, LLRender::BF_SOURCE_ALPHA, LLRender::BF_ZERO, LLRender::BF_SOURCE_ALPHA);

        gGL.setColorMask(true, true);

        // apply haze
        LLGLSLShader& haze_shader = gHazeWaterProgram;

        LL_PROFILE_GPU_ZONE("haze");
        bindDeferredShader(haze_shader, nullptr, &getWaterDisTarget());

        haze_shader.uniform4fv(LLShaderMgr::WATER_WATERPLANE, 1, LLDrawPoolAlpha::sWaterPlane.mV);

        static LLStaticHashedString above_water_str("above_water");
        haze_shader.uniform1i(above_water_str, sUnderWaterRender ? -1 : 1);

        haze_shader.bindTexture(LLShaderMgr::WATER_EXCLUSIONTEX,
                                &getWaterExclusionMaskTarget());

        if (LLPipeline::sUnderWaterRender)
        {
            LLGLDepthTest depth(GL_FALSE);

            // full screen blit
            mScreenTriangleVB->setBuffer();
            mScreenTriangleVB->drawArrays(LLRender::TRIANGLES, 0, 3);
        }
        else
        {
            //render water patches like LLDrawPoolWater does
            LLGLDepthTest depth(GL_TRUE, GL_FALSE);
            LLGLDisable   cull(GL_CULL_FACE);

            gGLLastMatrix = NULL;
            gGL.loadMatrix(gGLModelView);

            if (mWaterPool)
            {
                mWaterPool->pushFaceGeometry();
            }
        }

        unbindDeferredShader(haze_shader);


        gGL.setSceneBlendType(LLRender::BT_ALPHA);
    }
}

void LLPipeline::doWaterExclusionMask()
{
    LLRenderTarget& exclusion = getWaterExclusionMaskTarget();
    bindPrismLensTarget(exclusion);
    glClearColor(1, 1, 1, 1);
    exclusion.clear();
    mWaterExclusionPool->render();

    exclusion.flush();
    // flush() rebinds the preceding FBO, and bindTarget() installs that
    // target's full-size viewport -- with exact-size Prism scratch packs the
    // restored viewport is already correct, so no reassert is needed.
    glClearColor(0, 0, 0, 0);
}

void LLPipeline::setupSpotLight(LLGLSLShader& shader, LLDrawable* drawablep)
{
    //construct frustum
    LLVOVolume* volume = drawablep->getVOVolume();
    LLVector3 params = volume->getSpotLightParams();

    F32 fov = params.mV[0];
    F32 focus = params.mV[1];

    LLVector3 pos = drawablep->getPositionAgent();
    LLQuaternion quat = volume->getRenderRotation();
    LLVector3 scale = volume->getScale();

    //get near clip plane
    LLVector3 at_axis(0,0,-scale.mV[2]*0.5f);
    at_axis *= quat;

    LLVector3 np = pos+at_axis;
    at_axis.normVec();

    //get origin that has given fov for plane np, at_axis, and given scale
    F32 dist = (scale.mV[1]*0.5f)/tanf(fov*0.5f);

    LLVector3 origin = np - at_axis*dist;

    //matrix from volume space to agent space
    LLMatrix4 light_mat(quat, LLVector4(origin,1.f));

    glm::mat4 light_to_agent(glm::make_mat4((F32*) light_mat.mMatrix));
    glm::mat4 light_to_screen = get_current_modelview() * light_to_agent;

    glm::mat4 screen_to_light = glm::inverse(light_to_screen);

    F32 s = volume->getLightRadius()*1.5f;
    F32 near_clip = dist;
    F32 width = scale.mV[VX];
    F32 height = scale.mV[VY];
    F32 far_clip = s+dist-scale.mV[VZ];

    F32 fovy = fov; // radians
    F32 aspect = width/height;

    glm::mat4 trans(0.5f, 0.0f, 0.0f, 0.0f,
                        0.0f, 0.5f, 0.0f, 0.0f,
                        0.0f, 0.0f, 0.5f, 0.0f,
                        0.5f, 0.5f, 0.5f, 1.0f);

    glm::vec3 p1(0, 0, -(near_clip+0.01f));
    glm::vec3 p2(0, 0, -(near_clip+1.f));

    glm::vec3 screen_origin(0, 0, 0);

    p1 = mul_mat4_vec3(light_to_screen, p1);
    p2 = mul_mat4_vec3(light_to_screen, p2);
    screen_origin = mul_mat4_vec3(light_to_screen, screen_origin);

    glm::vec3 n = p2-p1;
    n = glm::normalize(n);

    F32 proj_range = far_clip - near_clip;
    glm::mat4 light_proj = glm::perspective(fovy, aspect, near_clip, far_clip);
    screen_to_light = trans * light_proj * screen_to_light;
    shader.uniformMatrix4fv(LLShaderMgr::PROJECTOR_MATRIX, 1, false, glm::value_ptr(screen_to_light));
    shader.uniform1f(LLShaderMgr::PROJECTOR_NEAR, near_clip);
    shader.uniform3fv(LLShaderMgr::PROJECTOR_P, 1, glm::value_ptr(p1));
    shader.uniform3fv(LLShaderMgr::PROJECTOR_N, 1, glm::value_ptr(n));
    shader.uniform3fv(LLShaderMgr::PROJECTOR_ORIGIN, 1, glm::value_ptr(screen_origin));
    shader.uniform1f(LLShaderMgr::PROJECTOR_RANGE, proj_range);
    shader.uniform1f(LLShaderMgr::PROJECTOR_AMBIANCE, params.mV[2]);
    S32 s_idx = -1;

    for (U32 i = 0; i < MAX_SPOT_SHADOWS; i++) // [BDMerge NSpot]
    {
        if (mShadowSpotLight[i] == drawablep)
        {
            s_idx = i;
        }
    }

    shader.uniform1i(LLShaderMgr::PROJECTOR_SHADOW_INDEX, s_idx);

    if (s_idx >= 0)
    {
        shader.uniform1f(LLShaderMgr::PROJECTOR_SHADOW_FADE, 1.f-mSpotLightFade[s_idx]);
    }
    else
    {
        shader.uniform1f(LLShaderMgr::PROJECTOR_SHADOW_FADE, 1.f);
    }

    // make sure we're not already targeting the same spot light with both shadow maps
    llassert(mTargetShadowSpotLight[0] != mTargetShadowSpotLight[1] || mTargetShadowSpotLight[0].isNull());

    // [BDMerge G3.3 Batch 3] Per-projector "cast shadows" opt-out: a projector in
    // the no-shadow set still lights the scene (all the projection uniforms above
    // are still set) but is excluded from the shadow-slot priority assignment
    // below, so it never occupies an mTargetShadowSpotLight[] slot and casts no
    // shadow. mTargetShadowSpotLight[] is rebuilt from scratch each frame (reset
    // to NULL in renderDeferredLighting), so simply not re-adding it here frees
    // the slot for other projectors via the normal fade-out path.
    if (!gCubeSnapshot && !sPrismLensRender &&
        !isProjectorShadowSuppressed(volume))
    {
        LLDrawable* potential = drawablep;
        //determine if this light is higher priority than one of the existing spot shadows
        F32 m_pri = volume->getSpotLightPriority();

        for (U32 i = 0; i < bdmergeMaxSpotShadows(); i++) // [BDMerge NSpot]
        {
            F32 pri = 0.f;

            if (mTargetShadowSpotLight[i].notNull())
            {
                pri = mTargetShadowSpotLight[i]->getVOVolume()->getSpotLightPriority();
            }

            if (m_pri > pri)
            {
                LLDrawable* temp = mTargetShadowSpotLight[i];
                mTargetShadowSpotLight[i] = potential;
                potential = temp;
                m_pri = pri;
            }
        }
    }

    // make sure we didn't end up targeting the same spot light with both shadow maps
    llassert(mTargetShadowSpotLight[0] != mTargetShadowSpotLight[1] || mTargetShadowSpotLight[0].isNull());

    LLViewerTexture* img = volume->getLightTexture();

    if (img == NULL)
    {
        img = LLViewerFetchedTexture::sWhiteImagep;
    }

    S32 channel = shader.enableTexture(LLShaderMgr::DEFERRED_PROJECTION);

    if (channel > -1)
    {
        if (img)
        {
            gGL.getTexUnit(channel)->bind(img);

            // [BDMerge Batch 2] Feature 2: gobo/cookie mip + anisotropic filtering.
            // Force a trilinear (mipmapped) min filter + anisotropic filter state on
            // the cookie texture so a projected gobo viewed at a grazing angle or from
            // far away samples coarser mips instead of aliasing. Requires the cookie to
            // carry a mip chain (fetched projector textures do); if it has none this
            // safely degrades to plain LINEAR (no regression). The matching shader-side
            // LOD clamp (goboLod, gated by gobo_aniso) actually selects those mips.
            if (BDMergeGoboAnisotropic)
            {
                gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_ANISOTROPIC);
            }

            F32 lod_range = logf((F32)img->getWidth())/logf(2.f);

            shader.uniform1f(LLShaderMgr::PROJECTOR_FOCUS, focus);
            shader.uniform1f(LLShaderMgr::PROJECTOR_LOD, lod_range);
            shader.uniform1f(LLShaderMgr::PROJECTOR_AMBIENT_LOD, llclamp((proj_range-focus)/proj_range*lod_range, 0.f, 1.f));
        }
    }

    // Tell the cookie samplers (deferredUtil::goboLod) whether to clamp the
    // focus LOD up to the screen-space minification LOD. Only the surface
    // projector programs get this; the volumetric march leaves it at 0.
    shader.uniform1i(LLShaderMgr::GOBO_ANISO, BDMergeGoboAnisotropic ? 1 : 0);
}

// [BDMerge G3.3] Side-effect-free variant of setupSpotLight for the finalize-stage
// projector volumetric pass (R1). It uploads ONLY the projector geometry (proj_mat
// /near/p/n/origin/range/ambiance) and the cookie (projectionMap + focus/lod), and
// takes the shadow slot as a parameter instead of searching mShadowSpotLight[]. It
// deliberately OMITS setupSpotLight's mTargetShadowSpotLight[] priority reshuffle:
// that mutation runs once per frame during renderDeferredLighting, and re-triggering
// it from renderFinalize would double-apply the swap and corrupt next-frame shadow
// slot assignment. Uses get_current_modelview() (== gGLModelView, the camera view
// matrix) so proj_mat is consistent with getPosition()'s view-space reconstruction.
void LLPipeline::setupSpotLightVolumetric(LLGLSLShader& shader, LLDrawable* drawablep, S32 slot)
{
    //construct frustum
    LLVOVolume* volume = drawablep->getVOVolume();
    LLVector3 params = volume->getSpotLightParams();

    F32 fov = params.mV[0];
    F32 focus = params.mV[1];

    LLVector3 pos = drawablep->getPositionAgent();
    LLQuaternion quat = volume->getRenderRotation();
    LLVector3 scale = volume->getScale();

    //get near clip plane
    LLVector3 at_axis(0,0,-scale.mV[2]*0.5f);
    at_axis *= quat;

    LLVector3 np = pos+at_axis;
    at_axis.normVec();

    //get origin that has given fov for plane np, at_axis, and given scale
    F32 dist = (scale.mV[1]*0.5f)/tanf(fov*0.5f);

    LLVector3 origin = np - at_axis*dist;

    //matrix from volume space to agent space
    LLMatrix4 light_mat(quat, LLVector4(origin,1.f));

    glm::mat4 light_to_agent(glm::make_mat4((F32*) light_mat.mMatrix));
    glm::mat4 light_to_screen = get_current_modelview() * light_to_agent;

    glm::mat4 screen_to_light = glm::inverse(light_to_screen);

    F32 s = volume->getLightRadius()*1.5f;
    F32 near_clip = dist;
    F32 width = scale.mV[VX];
    F32 height = scale.mV[VY];
    F32 far_clip = s+dist-scale.mV[VZ];

    F32 fovy = fov; // radians
    F32 aspect = width/height;

    glm::mat4 trans(0.5f, 0.0f, 0.0f, 0.0f,
                        0.0f, 0.5f, 0.0f, 0.0f,
                        0.0f, 0.0f, 0.5f, 0.0f,
                        0.5f, 0.5f, 0.5f, 1.0f);

    glm::vec3 p1(0, 0, -(near_clip+0.01f));
    glm::vec3 p2(0, 0, -(near_clip+1.f));

    glm::vec3 screen_origin(0, 0, 0);

    p1 = mul_mat4_vec3(light_to_screen, p1);
    p2 = mul_mat4_vec3(light_to_screen, p2);
    screen_origin = mul_mat4_vec3(light_to_screen, screen_origin);

    glm::vec3 n = p2-p1;
    n = glm::normalize(n);

    F32 proj_range = far_clip - near_clip;
    glm::mat4 light_proj = glm::perspective(fovy, aspect, near_clip, far_clip);

    // [BDMerge G3.3 Batch A - E1] Frustum-clipped march bounds. Extract the
    // projector's 6 frustum planes in VIEW space - the exact space the shaft
    // marches in - so the shader can slab-clip [t0,t1] to the real cone instead of
    // the loose sphere. `screen_to_light` here is still inverse(light_to_screen) =
    // view -> light-eye space (it is overwritten into the [0,1] texture matrix on
    // the next line, so this MUST run before it). Composing with light_proj gives
    // view -> projector clip; Gribb-Hartmann on its rows yields inside-positive
    // half-spaces (a*x+b*y+c*z+d >= 0 for a view-space point). Order matches the
    // shader: 0=left 1=right 2=bottom 3=top 4=near 5=far. Computed + uploaded per
    // cone only when the gate is on; the shader ignores the planes when off.
    if (BDMergeProjectorVolumetricsFrustumClip)
    {
        glm::mat4 view_to_clip = light_proj * screen_to_light; // view -> projector clip
        // glm is column-major: row i of the matrix is (m[0][i], m[1][i], m[2][i], m[3][i]).
        glm::vec4 r0(view_to_clip[0][0], view_to_clip[1][0], view_to_clip[2][0], view_to_clip[3][0]);
        glm::vec4 r1(view_to_clip[0][1], view_to_clip[1][1], view_to_clip[2][1], view_to_clip[3][1]);
        glm::vec4 r2(view_to_clip[0][2], view_to_clip[1][2], view_to_clip[2][2], view_to_clip[3][2]);
        glm::vec4 r3(view_to_clip[0][3], view_to_clip[1][3], view_to_clip[2][3], view_to_clip[3][3]);
        glm::vec4 planes[6];
        planes[0] = r3 + r0; // left
        planes[1] = r3 - r0; // right
        planes[2] = r3 + r1; // bottom
        planes[3] = r3 - r1; // top
        planes[4] = r3 + r2; // near
        planes[5] = r3 - r2; // far (uploaded for completeness; the shader skips it)
        for (S32 p = 0; p < 6; ++p)
        {
            F32 len = glm::length(glm::vec3(planes[p]));
            if (len > 1e-6f)
            {
                planes[p] /= len; // normalize so the shader's parallel-epsilon is metric
            }
        }
        shader.uniform4fv(LLShaderMgr::PROJVOL_FRUSTUM_PLANES, 6, glm::value_ptr(planes[0]));
    }

    screen_to_light = trans * light_proj * screen_to_light;
    shader.uniformMatrix4fv(LLShaderMgr::PROJECTOR_MATRIX, 1, false, glm::value_ptr(screen_to_light));
    shader.uniform1f(LLShaderMgr::PROJECTOR_NEAR, near_clip);
    shader.uniform3fv(LLShaderMgr::PROJECTOR_P, 1, glm::value_ptr(p1));
    shader.uniform3fv(LLShaderMgr::PROJECTOR_N, 1, glm::value_ptr(n));
    shader.uniform3fv(LLShaderMgr::PROJECTOR_ORIGIN, 1, glm::value_ptr(screen_origin));
    shader.uniform1f(LLShaderMgr::PROJECTOR_RANGE, proj_range);
    shader.uniform1f(LLShaderMgr::PROJECTOR_AMBIANCE, params.mV[2]);

    // Shadow slot is known by the caller - no mShadowSpotLight[] search, and no
    // mTargetShadowSpotLight[] priority mutation (that is setupSpotLight's job).
    shader.uniform1i(LLShaderMgr::PROJECTOR_SHADOW_INDEX, slot);
    if (slot >= 0 && slot < (S32)MAX_SPOT_SHADOWS)
    {
        shader.uniform1f(LLShaderMgr::PROJECTOR_SHADOW_FADE, 1.f-mSpotLightFade[slot]);
    }
    else
    {
        shader.uniform1f(LLShaderMgr::PROJECTOR_SHADOW_FADE, 1.f);
    }

    LLViewerTexture* img = volume->getLightTexture();

    if (img == NULL)
    {
        img = LLViewerFetchedTexture::sWhiteImagep;
    }

    S32 channel = shader.enableTexture(LLShaderMgr::DEFERRED_PROJECTION);

    if (channel > -1)
    {
        if (img)
        {
            gGL.getTexUnit(channel)->bind(img);

            F32 lod_range = logf((F32)img->getWidth())/logf(2.f);

            shader.uniform1f(LLShaderMgr::PROJECTOR_FOCUS, focus);
            shader.uniform1f(LLShaderMgr::PROJECTOR_LOD, lod_range);
            shader.uniform1f(LLShaderMgr::PROJECTOR_AMBIENT_LOD, llclamp((proj_range-focus)/proj_range*lod_range, 0.f, 1.f));
        }
    }
}

void LLPipeline::unbindDeferredShader(LLGLSLShader &shader)
{
    LLRenderTarget* deferred_target       = &mRT->deferredScreen;
    LLRenderTarget* deferred_light_target = &mRT->deferredLight;

    stop_glerror();
    shader.disableTexture(LLShaderMgr::NORMAL_MAP, deferred_target->getUsage());
    shader.disableTexture(LLShaderMgr::DEFERRED_DIFFUSE, deferred_target->getUsage());
    shader.disableTexture(LLShaderMgr::DEFERRED_SPECULAR, deferred_target->getUsage());
    shader.disableTexture(LLShaderMgr::DEFERRED_EMISSIVE, deferred_target->getUsage());
    shader.disableTexture(LLShaderMgr::DEFERRED_BRDF_LUT);
    //shader.disableTexture(LLShaderMgr::DEFERRED_DEPTH, deferred_depth_target->getUsage());
    shader.disableTexture(LLShaderMgr::DEFERRED_DEPTH, deferred_target->getUsage());
    shader.disableTexture(LLShaderMgr::DEFERRED_LIGHT, deferred_light_target->getUsage());
    shader.disableTexture(LLShaderMgr::DIFFUSE_MAP);

    for (U32 i = 0; i < 4; i++)
    {
        if (shader.disableTexture(LLShaderMgr::DEFERRED_SHADOW0+i) > -1)
        {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        }
    }

    for (U32 i = 4; i < 6; i++)
    {
        if (shader.disableTexture(LLShaderMgr::DEFERRED_SHADOW0+i) > -1)
        {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        }
    }

    shader.disableTexture(LLShaderMgr::DEFERRED_NOISE);
    shader.disableTexture(LLShaderMgr::DEFERRED_LIGHTFUNC);

    if (!LLPipeline::sReflectionProbesEnabled)
    {
        S32 channel = shader.disableTexture(LLShaderMgr::ENVIRONMENT_MAP, LLTexUnit::TT_CUBE_MAP);
        if (channel > -1)
        {
            LLCubeMap* cube_map = gSky.mVOSkyp ? gSky.mVOSkyp->getCubeMap() : NULL;
            if (cube_map)
            {
                cube_map->disable();
            }
        }
    }

    unbindReflectionProbes(shader);

    gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
    gGL.getTexUnit(0)->activate();
    shader.unbind();
}

void LLPipeline::setEnvMat(LLGLSLShader& shader)
{
    F32* m = gGLModelView;

    F32 mat[] = { m[0], m[1], m[2],
                    m[4], m[5], m[6],
                    m[8], m[9], m[10] };

    shader.uniformMatrix3fv(LLShaderMgr::DEFERRED_ENV_MAT, 1, true, mat);
}

void LLPipeline::bindReflectionProbes(LLGLSLShader& shader)
{
    static const LLStaticHashedString sPrismAuxiliary("prism_auxiliary");
    shader.uniform1i(sPrismAuxiliary, sPrismLensRender ? 1 : 0);

    if (!sReflectionProbesEnabled)
    {
        return;
    }

    const bool prism_auxiliary = sPrismLensRender;
    S32 channel = shader.enableTexture(LLShaderMgr::REFLECTION_PROBES, LLTexUnit::TT_CUBE_MAP_ARRAY);
    bool bound = false;
    if (channel > -1 && mReflectionMapManager.mTexture.notNull())
    {
        mReflectionMapManager.mTexture->bind(channel);
        bound = true;
    }

    channel = shader.enableTexture(LLShaderMgr::IRRADIANCE_PROBES, LLTexUnit::TT_CUBE_MAP_ARRAY);
    if (channel > -1 && mReflectionMapManager.mIrradianceMaps.notNull())
    {
        mReflectionMapManager.mIrradianceMaps->bind(channel);
        bound = true;
    }

    if (RenderMirrors && !prism_auxiliary)
    {
        channel = shader.enableTexture(LLShaderMgr::HERO_PROBE, LLTexUnit::TT_CUBE_MAP_ARRAY);
        if (channel > -1 && mHeroProbeManager.mTexture.notNull())
        {
            mHeroProbeManager.mTexture->bind(channel);
            bound = true;
        }
    }


    if (bound &&
        (!prism_auxiliary ||
         (mPrismSavedProbeDataValid && mReflectionMapManager.mUBO != 0)))
    {
        // On an auxiliary no-UBO allocation failure, never let setUniforms()
        // lazily run updateUniforms() from the source camera. The explicit
        // Prism shader guard still suppresses SSR/hero; probe lighting simply
        // degrades for this failed capture instead of contaminating main state.
        mReflectionMapManager.setUniforms();

        setEnvMat(shader);
    }

    // The shared scene map belongs to the main camera. Sampling it from a
    // remote G-buffer produces unrelated rays and advancing its Poisson phase
    // makes the main view's temporal sequence depend on capture cadence.
    if (!prism_auxiliary)
    {
        channel = shader.enableTexture(LLShaderMgr::SCENE_MAP);
        if (channel > -1)
        {
            gGL.getTexUnit(channel)->bind(&mSceneMap);
        }
    }

    shader.uniform1f(LLShaderMgr::DEFERRED_SSR_ITR_COUNT,
                     prism_auxiliary ? 0.f :
                         static_cast<GLfloat>(RenderScreenSpaceReflectionIterations));
    shader.uniform1f(LLShaderMgr::DEFERRED_SSR_DIST_BIAS, RenderScreenSpaceReflectionDistanceBias);
    shader.uniform1f(LLShaderMgr::DEFERRED_SSR_RAY_STEP, RenderScreenSpaceReflectionRayStep);
    shader.uniform1f(LLShaderMgr::DEFERRED_SSR_GLOSSY_SAMPLES, (GLfloat)RenderScreenSpaceReflectionGlossySamples);
    shader.uniform1f(LLShaderMgr::DEFERRED_SSR_REJECT_BIAS, RenderScreenSpaceReflectionDepthRejectBias);
    if (!prism_auxiliary)
    {
        mPoissonOffset++;

        if (mPoissonOffset > 128 - RenderScreenSpaceReflectionGlossySamples)
        {
            mPoissonOffset = 0;
        }
    }

    shader.uniform1f(LLShaderMgr::DEFERRED_SSR_NOISE_SINE,
                     prism_auxiliary ? 0.f : static_cast<GLfloat>(mPoissonOffset));
    shader.uniform1f(LLShaderMgr::DEFERRED_SSR_ADAPTIVE_STEP_MULT, RenderScreenSpaceReflectionAdaptiveStepMultiplier);

    if (!prism_auxiliary)
    {
        channel = shader.enableTexture(LLShaderMgr::SCENE_DEPTH);
        if (channel > -1)
        {
            gGL.getTexUnit(channel)->bind(&mSceneMap, true);
        }
    }


}

void LLPipeline::unbindReflectionProbes(LLGLSLShader& shader)
{
    S32 channel = shader.disableTexture(LLShaderMgr::REFLECTION_PROBES, LLTexUnit::TT_CUBE_MAP_ARRAY);
    if (channel > -1 && mReflectionMapManager.mTexture.notNull())
    {
        mReflectionMapManager.mTexture->unbind();
        if (channel == 0)
        {
            gGL.getTexUnit(channel)->enable(LLTexUnit::TT_TEXTURE);
        }
    }
}


inline float sgn(float a)
{
    if (a > 0.0F) return (1.0F);
    if (a < 0.0F) return (-1.0F);
    return (0.0F);
}

glm::mat4 look(const LLVector3 pos, const LLVector3 dir, const LLVector3 up)
{
    LLVector3 dirN;
    LLVector3 upN;
    LLVector3 lftN;

    lftN = dir % up;
    lftN.normVec();

    upN = lftN % dir;
    upN.normVec();

    dirN = dir;
    dirN.normVec();

    F32 ret[16];
    ret[ 0] = lftN[0];
    ret[ 1] = upN[0];
    ret[ 2] = -dirN[0];
    ret[ 3] = 0.f;

    ret[ 4] = lftN[1];
    ret[ 5] = upN[1];
    ret[ 6] = -dirN[1];
    ret[ 7] = 0.f;

    ret[ 8] = lftN[2];
    ret[ 9] = upN[2];
    ret[10] = -dirN[2];
    ret[11] = 0.f;

    ret[12] = -(lftN*pos);
    ret[13] = -(upN*pos);
    ret[14] = dirN*pos;
    ret[15] = 1.f;

    return glm::make_mat4(ret);
}

void LLPipeline::generateWeatherRainOcclusion(LLCamera& camera)
{
    static LLCachedControl<bool> weather_enabled(
        gSavedSettings, "AlchemyWeatherEnabled", false);
    static LLCachedControl<bool> occlusion_enabled(
        gSavedSettings, "AlchemyWeatherRainOcclusion", false);

    // The default-off path is only two cached-control checks: do not touch
    // producer state or inspect any other setting or shader when disabled.
    if (!weather_enabled() || !occlusion_enabled())
    {
        return;
    }

    const auto release_map = [this]()
    {
        mWeatherRainOcclusionValid = false;
        mWeatherRainOcclusionFrame = 0;
        mWeatherRainOcclusionFailedResolution = 0;
        mWeatherRainOcclusionDepthRange = 1.f;
        mWeatherRainOcclusionMatrix = glm::mat4(1.f);
        if (mWeatherRainOcclusion.getWidth() != 0)
        {
            mWeatherRainOcclusion.release();
        }
    };

    static LLCachedControl<bool> rain_enabled(
        gSavedSettings, "AlchemyWeatherRainEnabled", true);
    static LLCachedControl<F32> rain_intensity(
        gSavedSettings, "AlchemyWeatherRainIntensity", 0.55f);
    static LLCachedControl<U32> resolution_setting(
        gSavedSettings, "AlchemyWeatherRainOcclusionResolution", 512U);
    static LLCachedControl<F32> extent_setting(
        gSavedSettings, "AlchemyWeatherRainOcclusionExtent", 128.f);
    static LLCachedControl<F32> rain_distance_setting(
        gSavedSettings, "AlchemyWeatherRainMaxDistance", 80.f);
    static LLCachedControl<F32> shelter_height_setting(
        gSavedSettings, "AlchemyWeatherShelterHeight", 48.f);
    static LLCachedControl<bool> debug_occlusion(
        gSavedSettings, "AlchemyWeatherRainOcclusionDebug", false);

    const bool weather_consumers =
        (gDeferredWeatherRainOcclusionProgram.isComplete() &&
         gDeferredWeatherRainOcclusionProgram.getTextureChannel(
             LLShaderMgr::WEATHER_RAIN_OCCLUSION_MAP) >= 0) ||
        (gDeferredWeatherSurfaceOcclusionProgram.isComplete() &&
         gDeferredWeatherSurfaceOcclusionProgram.getTextureChannel(
             LLShaderMgr::WEATHER_RAIN_OCCLUSION_MAP) >= 0);
    const bool shadow_programs =
        gDeferredShadowProgram.isComplete() &&
        gDeferredShadowAlphaMaskProgram.isComplete() &&
        gDeferredShadowFullbrightAlphaMaskProgram.isComplete() &&
        gDeferredTreeShadowProgram.isComplete() &&
        gDeferredShadowGLTFAlphaMaskProgram.isComplete() &&
        gDeferredShadowCubeProgram.isComplete();
    const F32 configured_intensity = rain_intensity();
    if (!rain_enabled() ||
        !std::isfinite(configured_intensity) ||
        !(configured_intensity > 0.f) ||
        !sRenderDeferred || gCubeSnapshot ||
        !weather_consumers || !shadow_programs)
    {
        release_map();
        return;
    }

    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    LL_PROFILE_GPU_ZONE("generateWeatherRainOcclusion");

    const U32 requested_resolution =
        llclamp(resolution_setting(), 256U, 1024U);
    if (mWeatherRainOcclusion.getWidth() == 0 &&
        mWeatherRainOcclusionFailedResolution == requested_resolution)
    {
        return;
    }
    if (mWeatherRainOcclusion.getWidth() != requested_resolution ||
        mWeatherRainOcclusion.getHeight() != requested_resolution)
    {
        release_map();
        if (!mWeatherRainOcclusion.allocate(
                requested_resolution, requested_resolution, 0,
                true, false, LLTexUnit::TT_TEXTURE,
                LLTexUnit::TMG_NONE, LLRenderTarget::DEPTH_FMT_24))
        {
            release_map();
            mWeatherRainOcclusionFailedResolution = requested_resolution;
            gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
            return;
        }
        mWeatherRainOcclusionFailedResolution = 0;

        // Manual sampler2D comparisons require raw depth, never a shadow
        // compare sampler. Keep the dedicated target isolated from sun-map
        // filtering/compare state.
        gGL.getTexUnit(0)->bind(&mWeatherRainOcclusion, true);
        gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
        gGL.getTexUnit(0)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
    }

    const F32 extent_raw = extent_setting();
    const F32 extent = llclamp(
        std::isfinite(extent_raw) ? extent_raw : 128.f, 32.f, 320.f);
    const U32 map_resolution = mWeatherRainOcclusion.getWidth();
    if (map_resolution == 0)
    {
        release_map();
        return;
    }

    // Snap in global space so an agent-coordinate origin shift at a region
    // crossing cannot move the world-aligned texel grid.
    LLVector3d center_global =
        gAgent.getPosGlobalFromAgent(camera.getOrigin());
    if (!center_global.isFinite())
    {
        release_map();
        return;
    }
    const F64 world_texel =
        (2.0 * static_cast<F64>(extent)) /
        static_cast<F64>(map_resolution);
    center_global.mdV[VX] =
        std::floor(center_global.mdV[VX] / world_texel + 0.5) *
        world_texel;
    center_global.mdV[VY] =
        std::floor(center_global.mdV[VY] / world_texel + 0.5) *
        world_texel;
    center_global.mdV[VZ] =
        std::floor(center_global.mdV[VZ] + 0.5);
    const LLVector3 center_agent =
        gAgent.getPosAgentFromGlobal(center_global);
    if (!center_agent.isFinite())
    {
        release_map();
        return;
    }

    const F32 rain_distance_raw = rain_distance_setting();
    const F32 rain_distance = llclamp(
        std::isfinite(rain_distance_raw) ? rain_distance_raw : 80.f,
        8.f, 256.f);
    const F32 shelter_height_raw = shelter_height_setting();
    const F32 shelter_height = llclamp(
        std::isfinite(shelter_height_raw) ? shelter_height_raw : 48.f,
        8.f, 256.f);
    const F32 vertical_half = llclamp(
        llmax(extent, llmax(rain_distance, shelter_height)) + 32.f,
        64.f, 512.f);
    const F32 near_clip = 0.1f;
    const F32 far_clip = near_clip + vertical_half * 2.f;
    const F32 depth_range = far_clip - near_clip;
    LLVector3 top_origin = center_agent;
    top_origin.mV[VZ] += vertical_half;

    const glm::mat4 view = look(
        top_origin, LLVector3(0.f, 0.f, -1.f),
        LLVector3(0.f, 1.f, 0.f));
    const glm::mat4 projection = glm::ortho(
        -extent, extent, -extent, extent, near_clip, far_clip);
    const glm::mat4 view_projection = projection * view;

    const glm::mat4 saved_modelview = get_current_modelview();
    const glm::mat4 saved_projection = get_current_projection();
    const glm::mat4 saved_last_modelview = get_last_modelview();
    const glm::mat4 saved_last_projection = get_last_projection();
    const LLViewerCamera::eCameraID saved_camera_id =
        LLViewerCamera::sCurCameraID;
    S32 saved_viewport[4];
    std::memcpy(saved_viewport, gGLViewport, sizeof(saved_viewport));
    GLboolean saved_color_mask[4];
    glGetBooleanv(GL_COLOR_WRITEMASK, saved_color_mask);
    LLGLSLShader* saved_shader = LLGLSLShader::sCurBoundShaderPtr;

    pushRenderTypeMask();
    andRenderTypeMask(
        LLPipeline::RENDER_TYPE_TERRAIN,
        LLPipeline::RENDER_TYPE_SIMPLE,
        LLPipeline::RENDER_TYPE_ALPHA,
        LLPipeline::RENDER_TYPE_ALPHA_PRE_WATER,
        LLPipeline::RENDER_TYPE_ALPHA_POST_WATER,
        LLPipeline::RENDER_TYPE_GLTF_PBR,
        LLPipeline::RENDER_TYPE_FULLBRIGHT,
        LLPipeline::RENDER_TYPE_BUMP,
        LLPipeline::RENDER_TYPE_VOLUME,
        LLPipeline::RENDER_TYPE_TREE,
        LLPipeline::RENDER_TYPE_PASS_ALPHA_MASK,
        LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK,
        LLPipeline::RENDER_TYPE_PASS_SIMPLE,
        LLPipeline::RENDER_TYPE_PASS_BUMP,
        LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT,
        LLPipeline::RENDER_TYPE_PASS_SHINY,
        LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_SHINY,
        LLPipeline::RENDER_TYPE_PASS_MATERIAL,
        LLPipeline::RENDER_TYPE_PASS_MATERIAL_ALPHA_MASK,
        LLPipeline::RENDER_TYPE_PASS_MATERIAL_ALPHA_EMISSIVE,
        LLPipeline::RENDER_TYPE_PASS_SPECMAP,
        LLPipeline::RENDER_TYPE_PASS_SPECMAP_MASK,
        LLPipeline::RENDER_TYPE_PASS_SPECMAP_EMISSIVE,
        LLPipeline::RENDER_TYPE_PASS_NORMMAP,
        LLPipeline::RENDER_TYPE_PASS_NORMMAP_MASK,
        LLPipeline::RENDER_TYPE_PASS_NORMMAP_EMISSIVE,
        LLPipeline::RENDER_TYPE_PASS_NORMSPEC,
        LLPipeline::RENDER_TYPE_PASS_NORMSPEC_MASK,
        LLPipeline::RENDER_TYPE_PASS_NORMSPEC_EMISSIVE,
        LLPipeline::RENDER_TYPE_PASS_GLTF_PBR,
        LLPipeline::RENDER_TYPE_PASS_GLTF_PBR_ALPHA_MASK,
        END_RENDER_TYPES);

    LLGLDisable no_blend(GL_BLEND);
    LLGLDisable no_scissor(GL_SCISSOR_TEST);
    LLGLEnable depth_clamp(GL_DEPTH_CLAMP);
    LLGLDepthTest depth_test(GL_TRUE, GL_TRUE, GL_LESS);
    set_current_modelview(view);
    set_current_projection(projection);
    LLViewerCamera::sCurCameraID =
        LLViewerCamera::CAMERA_WEATHER_RAIN_OCCLUSION;

    mWeatherRainOcclusion.bindTarget();
    gGLViewport[0] = 0;
    gGLViewport[1] = 0;
    gGLViewport[2] = static_cast<S32>(map_resolution);
    gGLViewport[3] = static_cast<S32>(map_resolution);
    mWeatherRainOcclusion.clear(GL_DEPTH_BUFFER_BIT);

    LLCamera shadow_camera = camera;
    shadow_camera.setOrigin(top_origin);
    shadow_camera.lookDir(
        LLVector3(0.f, 0.f, -1.f),
        LLVector3(0.f, 1.f, 0.f));
    shadow_camera.setNear(near_clip);
    shadow_camera.setFar(far_clip);
    LLViewerCamera::updateFrustumPlanes(
        shadow_camera, true, false, true);

    static LLCullResult rain_occlusion_result;
    const bool saved_rain_occlusion_render = sRainOcclusionRender;
    sRainOcclusionRender = true;
    // Unrigged opaque + alpha-mask world geometry only. Ordinary alpha blend
    // has no reliable "stops rain" material semantic; rigid worn attachments
    // are rejected centrally while this scoped flag is true.
    renderShadow(
        view, projection, shadow_camera, rain_occlusion_result,
        true, true, false, false, false);
    sRainOcclusionRender = saved_rain_occlusion_render;

    // Publish the map only when stateSort built at least one caster batch for
    // a pass renderShadow submitted. A cleared D24 map is all-far and the
    // shader intentionally interprets that as exposed, so treating an empty
    // map as valid would suppress the coarse camera-ray fallback and fail open.
    static const U32 caster_types[] = {
        LLRenderPass::PASS_SIMPLE,
        LLRenderPass::PASS_FULLBRIGHT,
        LLRenderPass::PASS_SHINY,
        LLRenderPass::PASS_BUMP,
        LLRenderPass::PASS_FULLBRIGHT_SHINY,
        LLRenderPass::PASS_MATERIAL,
        LLRenderPass::PASS_MATERIAL_ALPHA_EMISSIVE,
        LLRenderPass::PASS_SPECMAP,
        LLRenderPass::PASS_SPECMAP_EMISSIVE,
        LLRenderPass::PASS_NORMMAP,
        LLRenderPass::PASS_NORMMAP_EMISSIVE,
        LLRenderPass::PASS_NORMSPEC,
        LLRenderPass::PASS_NORMSPEC_EMISSIVE,
        LLRenderPass::PASS_GLTF_PBR,
        LLRenderPass::PASS_ALPHA_MASK,
        LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK,
        LLRenderPass::PASS_MATERIAL_ALPHA_MASK,
        LLRenderPass::PASS_SPECMAP_MASK,
        LLRenderPass::PASS_NORMMAP_MASK,
        LLRenderPass::PASS_NORMSPEC_MASK,
        LLRenderPass::PASS_GLTF_PBR_ALPHA_MASK
    };
    U32 caster_batches = 0;
    for (U32 type : caster_types)
    {
        caster_batches += rain_occlusion_result.getRenderMapSize(type);
    }

    // Diagnostic readback is deliberately expensive but completely gated and
    // throttled. It distinguishes cull starvation from submitted geometry that
    // nevertheless left the depth target at its all-far clear value.
    if (debug_occlusion() && (gFrameCount & 63U) == 0)
    {
        static std::vector<F32> depth_samples;
        depth_samples.assign(
            static_cast<size_t>(map_resolution) * map_resolution, 1.f);
        glReadPixels(
            0, 0,
            static_cast<GLsizei>(map_resolution),
            static_cast<GLsizei>(map_resolution),
            GL_DEPTH_COMPONENT, GL_FLOAT, depth_samples.data());
        const GLenum gl_error = glGetError();
        F32 depth_min = 1.f;
        F32 depth_max = 0.f;
        for (F32 depth : depth_samples)
        {
            if (std::isfinite(depth))
            {
                depth_min = llmin(depth_min, depth);
                depth_max = llmax(depth_max, depth);
            }
        }
        LL_INFOS("Weather")
            << "Rain occlusion: caster_batches=" << caster_batches
            << ", target_width=" << map_resolution
            << ", depth_range=" << depth_range
            << ", depth_min=" << depth_min
            << ", depth_max=" << depth_max
            << ", gl_error=" << static_cast<U32>(gl_error)
            << LL_ENDL;
    }

    // LLRenderTarget::flush uses gGLViewport when returning to the default
    // framebuffer, so restore the global array before popping the target.
    std::memcpy(gGLViewport, saved_viewport, sizeof(saved_viewport));
    mWeatherRainOcclusion.flush();
    glViewport(
        saved_viewport[0], saved_viewport[1],
        saved_viewport[2], saved_viewport[3]);

    set_current_modelview(saved_modelview);
    set_current_projection(saved_projection);
    set_last_modelview(saved_last_modelview);
    set_last_projection(saved_last_projection);
    LLViewerCamera::sCurCameraID = saved_camera_id;
    popRenderTypeMask();
    gGL.setColorMask(
        saved_color_mask[0] != GL_FALSE,
        saved_color_mask[1] != GL_FALSE,
        saved_color_mask[2] != GL_FALSE,
        saved_color_mask[3] != GL_FALSE);
    if (saved_shader)
    {
        saved_shader->bind();
    }
    else
    {
        LLGLSLShader::unbind();
    }

    mWeatherRainOcclusionMatrix = view_projection;
    mWeatherRainOcclusionDepthRange = depth_range;
    mWeatherRainOcclusionFrame = gFrameCount;
    mWeatherRainOcclusionValid = caster_batches > 0;
}

void LLPipeline::renderShadow(const glm::mat4& view, const glm::mat4& proj,
                              LLCamera& shadow_cam, LLCullResult& result,
                              bool depth_clamp, bool do_cull,
                              bool include_rigged,
                              bool include_alpha_blend,
                              bool cull_faces)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE; //LL_RECORD_BLOCK_TIME(FTM_SHADOW_RENDER);
    LL_PROFILE_GPU_ZONE("renderShadow");

    LLPipeline::sShadowRender = true;

    // disable occlusion culling during shadow render
    U32 saved_occlusion = sUseOcclusion;
    sUseOcclusion = 0;

    // List of render pass types that use the prim volume as the shadow,
    // ignoring textures.
    static const U32 types[] = {
        LLRenderPass::PASS_SIMPLE,
        LLRenderPass::PASS_FULLBRIGHT,
        LLRenderPass::PASS_SHINY,
        LLRenderPass::PASS_BUMP,
        LLRenderPass::PASS_FULLBRIGHT_SHINY,
        LLRenderPass::PASS_MATERIAL,
        LLRenderPass::PASS_MATERIAL_ALPHA_EMISSIVE,
        LLRenderPass::PASS_SPECMAP,
        LLRenderPass::PASS_SPECMAP_EMISSIVE,
        LLRenderPass::PASS_NORMMAP,
        LLRenderPass::PASS_NORMMAP_EMISSIVE,
        LLRenderPass::PASS_NORMSPEC,
        LLRenderPass::PASS_NORMSPEC_EMISSIVE
    };

    LLGLState cull(
        GL_CULL_FACE,
        cull_faces ? LLGLState::ENABLED_STATE : LLGLState::DISABLED_STATE);

    //enable depth clamping if available
    LLGLEnable clamp_depth(depth_clamp ? GL_DEPTH_CLAMP : 0);

    LLGLDepthTest depth_test(GL_TRUE, GL_TRUE, GL_LESS);

    // In RenderShadowCullMode 1, do_cull is false: generateSunShadow did the single union
    // octree cull and pre-filtered `result` to this cascade's frustum (bucketShadowCull),
    // so skip the per-cascade octree walk and only sort/build this cascade's render map.
    if (do_cull)
    {
        updateCull(shadow_cam, result);
    }

    stateSort(shadow_cam, result);

    //generate shadow map
    gGL.matrixMode(LLRender::MM_PROJECTION);
    gGL.pushMatrix();
    gGL.loadMatrix(glm::value_ptr(proj));
    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGL.pushMatrix();
    gGL.loadMatrix(glm::value_ptr(view));

    stop_glerror();
    gGLLastMatrix = NULL;

    gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

    stop_glerror();

    struct CompareVertexBuffer
    {
        bool operator()(const LLDrawInfo* const& lhs, const LLDrawInfo* const& rhs)
        {
            return lhs->mVertexBuffer > rhs->mVertexBuffer;
        }
    };


    const int pass_count = include_rigged ? 2 : 1;
    LLVertexBuffer::unbind();
    for (int j = 0; j < pass_count; ++j) // 0 -- static, 1 -- rigged
    {
        bool rigged = j == 1;
        gDeferredShadowProgram.bind(rigged);

        gGL.diffuseColor4f(1, 1, 1, 1);

        S32 shadow_detail = RenderShadowDetail;

        // if not using VSM, disable color writes
        if (shadow_detail <= 2)
        {
            gGL.setColorMask(false, false);
        }

        LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE("shadow simple"); //LL_RECORD_BLOCK_TIME(FTM_SHADOW_SIMPLE);
        LL_PROFILE_GPU_ZONE("shadow simple");
        gGL.getTexUnit(0)->disable();

        for (U32 type : types)
        {
            renderObjects(type, false, false, rigged);
        }

        renderGLTFObjects(LLRenderPass::PASS_GLTF_PBR, false, rigged);

        gGL.getTexUnit(0)->enable(LLTexUnit::TT_TEXTURE);
    }

    if (LLPipeline::sUseOcclusion > 1)
    { // do occlusion culling against non-masked only to take advantage of hierarchical Z
        doOcclusion(shadow_cam);
    }


    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE("shadow geom");
        renderGeomShadow(shadow_cam);
    }

    {
        LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE("shadow alpha");
        LL_PROFILE_GPU_ZONE("shadow alpha");
        const S32 sun_up = LLEnvironment::instance().getIsSunUp() ? 1 : 0;
        U32 target_width = LLRenderTarget::sCurResX;

        for (int i = 0; i < pass_count; ++i)
        {
            bool rigged = i == 1;

            {
                LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE("shadow alpha masked");
                LL_PROFILE_GPU_ZONE("shadow alpha masked");
                gDeferredShadowAlphaMaskProgram.bind(rigged);
                LLGLSLShader::sCurBoundShaderPtr->uniform1i(LLShaderMgr::SUN_UP_FACTOR, sun_up);
                LLGLSLShader::sCurBoundShaderPtr->uniform1f(LLShaderMgr::DEFERRED_SHADOW_TARGET_WIDTH, (float)target_width);
                renderMaskedObjects(LLRenderPass::PASS_ALPHA_MASK, true, true, rigged);
            }

            if (include_alpha_blend)
            {
                LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE("shadow alpha blend");
                LL_PROFILE_GPU_ZONE("shadow alpha blend");
                renderAlphaObjects(rigged);
            }

            {
                LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE("shadow fullbright alpha masked");
                LL_PROFILE_GPU_ZONE("shadow alpha masked");
                gDeferredShadowFullbrightAlphaMaskProgram.bind(rigged);
                LLGLSLShader::sCurBoundShaderPtr->uniform1i(LLShaderMgr::SUN_UP_FACTOR, sun_up);
                LLGLSLShader::sCurBoundShaderPtr->uniform1f(LLShaderMgr::DEFERRED_SHADOW_TARGET_WIDTH, (float)target_width);
                renderFullbrightMaskedObjects(LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK, true, true, rigged);
            }

            {
                LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE("shadow alpha grass");
                LL_PROFILE_GPU_ZONE("shadow alpha grass");
                gDeferredTreeShadowProgram.bind(rigged);
                LLGLSLShader::sCurBoundShaderPtr->setMinimumAlpha(ALPHA_BLEND_CUTOFF);

                if (i == 0)
                {
                    renderObjects(LLRenderPass::PASS_GRASS, true);
                }

                {
                    LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE("shadow alpha material");
                    LL_PROFILE_GPU_ZONE("shadow alpha material");
                    renderMaskedObjects(LLRenderPass::PASS_NORMSPEC_MASK, true, false, rigged);
                    renderMaskedObjects(LLRenderPass::PASS_MATERIAL_ALPHA_MASK, true, false, rigged);
                    renderMaskedObjects(LLRenderPass::PASS_SPECMAP_MASK, true, false, rigged);
                    renderMaskedObjects(LLRenderPass::PASS_NORMMAP_MASK, true, false, rigged);

                    // multi-material indexed legacy mask batches alpha-test per-slot
                    if (LLGLSLShader::sIndexedLegacyMaterials && gDeferredShadowMaterialIndexedProgram.isComplete())
                    {
                        gDeferredShadowMaterialIndexedProgram.bind(rigged);
                        LLGLSLShader::sCurBoundShaderPtr->uniform1f(LLShaderMgr::DEFERRED_SHADOW_TARGET_WIDTH, (float)target_width);
                        U32 off = rigged ? 1 : 0;
                        mAlphaMaskPool->pushMaskBatchesIndexed(LLRenderPass::PASS_NORMSPEC_MASK + off, rigged);
                        mAlphaMaskPool->pushMaskBatchesIndexed(LLRenderPass::PASS_MATERIAL_ALPHA_MASK + off, rigged);
                        mAlphaMaskPool->pushMaskBatchesIndexed(LLRenderPass::PASS_SPECMAP_MASK + off, rigged);
                        mAlphaMaskPool->pushMaskBatchesIndexed(LLRenderPass::PASS_NORMMAP_MASK + off, rigged);
                    }
                }
            }
        }

        for (int i = 0; i < pass_count; ++i)
        {
            bool rigged = i == 1;
            gDeferredShadowGLTFAlphaMaskProgram.bind(rigged);
            LLGLSLShader::sCurBoundShaderPtr->uniform1i(LLShaderMgr::SUN_UP_FACTOR, sun_up);
            LLGLSLShader::sCurBoundShaderPtr->uniform1f(LLShaderMgr::DEFERRED_SHADOW_TARGET_WIDTH, (float)target_width);

            gGL.loadMatrix(gGLModelView);
            gGLLastMatrix = NULL;

            U32 type = LLRenderPass::PASS_GLTF_PBR_ALPHA_MASK;

            // multi-material batches alpha-test per-slot; render them with the
            // indexed shadow program so batched cutouts stay correct
            bool gltf_indexed = LLGLSLShader::sIndexedGLTFChannels >= 2 && gDeferredShadowGLTFAlphaMaskIndexedProgram.isComplete();

            if (rigged)
            {
                if (gltf_indexed)
                {
                    mAlphaMaskPool->pushRiggedGLTFBatchesScalar(type + 1);

                    gDeferredShadowGLTFAlphaMaskIndexedProgram.bind(true);
                    LLGLSLShader::sCurBoundShaderPtr->uniform1i(LLShaderMgr::SUN_UP_FACTOR, sun_up);
                    LLGLSLShader::sCurBoundShaderPtr->uniform1f(LLShaderMgr::DEFERRED_SHADOW_TARGET_WIDTH, (float)target_width);
                    mAlphaMaskPool->pushRiggedGLTFBatchesIndexed(type + 1, LLRenderPass::GLTF_MAPS_BASE_COLOR); // shadow samples base color only
                }
                else
                {
                    mAlphaMaskPool->pushRiggedGLTFBatches(type + 1);
                }
            }
            else
            {
                if (gltf_indexed)
                {
                    mAlphaMaskPool->pushGLTFBatchesScalar(type);

                    gDeferredShadowGLTFAlphaMaskIndexedProgram.bind();
                    LLGLSLShader::sCurBoundShaderPtr->uniform1i(LLShaderMgr::SUN_UP_FACTOR, sun_up);
                    LLGLSLShader::sCurBoundShaderPtr->uniform1f(LLShaderMgr::DEFERRED_SHADOW_TARGET_WIDTH, (float)target_width);
                    mAlphaMaskPool->pushGLTFBatchesIndexed(type, LLRenderPass::GLTF_MAPS_BASE_COLOR); // shadow samples base color only
                }
                else
                {
                    mAlphaMaskPool->pushGLTFBatches(type);
                }
            }

            gGL.loadMatrix(gGLModelView);
            gGLLastMatrix = NULL;
        }
    }

    gDeferredShadowCubeProgram.bind();
    gGLLastMatrix = NULL;
    gGL.loadMatrix(gGLModelView);

    gGL.setColorMask(true, true);

    gGL.matrixMode(LLRender::MM_PROJECTION);
    gGL.popMatrix();
    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGL.popMatrix();
    gGLLastMatrix = NULL;

    // reset occlusion culling flag
    sUseOcclusion = saved_occlusion;
    LLPipeline::sShadowRender = false;
}

bool LLPipeline::getVisiblePointCloud(LLCamera& camera, LLVector3& min, LLVector3& max, std::vector<LLVector3>& fp, LLVector3 light_dir)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    //get point cloud of intersection of frust and min, max

    if (getVisibleExtents(camera, min, max))
    {
        return false;
    }

    //get set of planes on bounding box
    LLPlane bp[] = {
        LLPlane(min, LLVector3(-1,0,0)),
        LLPlane(min, LLVector3(0,-1,0)),
        LLPlane(min, LLVector3(0,0,-1)),
        LLPlane(max, LLVector3(1,0,0)),
        LLPlane(max, LLVector3(0,1,0)),
        LLPlane(max, LLVector3(0,0,1))};

    //potential points
    static std::vector<LLVector3> pp;
    pp.clear();

    //add corners of AABB
    pp.push_back(LLVector3(min.mV[0], min.mV[1], min.mV[2]));
    pp.push_back(LLVector3(max.mV[0], min.mV[1], min.mV[2]));
    pp.push_back(LLVector3(min.mV[0], max.mV[1], min.mV[2]));
    pp.push_back(LLVector3(max.mV[0], max.mV[1], min.mV[2]));
    pp.push_back(LLVector3(min.mV[0], min.mV[1], max.mV[2]));
    pp.push_back(LLVector3(max.mV[0], min.mV[1], max.mV[2]));
    pp.push_back(LLVector3(min.mV[0], max.mV[1], max.mV[2]));
    pp.push_back(LLVector3(max.mV[0], max.mV[1], max.mV[2]));

    //add corners of camera frustum
    for (U32 i = 0; i < LLCamera::AGENT_FRUSTRUM_NUM; i++)
    {
        pp.push_back(camera.mAgentFrustum[i]);
    }


    //bounding box line segments
    U32 bs[] =
            {
        0,1,
        1,3,
        3,2,
        2,0,

        4,5,
        5,7,
        7,6,
        6,4,

        0,4,
        1,5,
        3,7,
        2,6
    };

    for (U32 i = 0; i < 12; i++)
    { //for each line segment in bounding box
        for (U32 j = 0; j < LLCamera::AGENT_PLANE_NO_USER_CLIP_NUM; j++)
        { //for each plane in camera frustum
            const LLPlane& cp = camera.getAgentPlane(j);
            const LLVector3& v1 = pp[bs[i*2+0]];
            const LLVector3& v2 = pp[bs[i*2+1]];
            LLVector3 n;
            cp.getVector3(n);

            LLVector3 line = v1-v2;

            F32 d1 = line*n;
            F32 d2 = -cp.dist(v2);

            F32 t = d2/d1;

            if (t > 0.f && t < 1.f)
            {
                LLVector3 intersect = v2+line*t;
                pp.push_back(intersect);
            }
        }
    }

    //camera frustum line segments
    const U32 fs[] =
    {
        0,1,
        1,2,
        2,3,
        3,0,

        4,5,
        5,6,
        6,7,
        7,4,

        0,4,
        1,5,
        2,6,
        3,7
    };

    for (U32 i = 0; i < 12; i++)
    {
        for (U32 j = 0; j < 6; ++j)
        {
            const LLVector3& v1 = pp[fs[i*2+0]+8];
            const LLVector3& v2 = pp[fs[i*2+1]+8];
            const LLPlane& cp = bp[j];
            LLVector3 n;
            cp.getVector3(n);

            LLVector3 line = v1-v2;

            F32 d1 = line*n;
            F32 d2 = -cp.dist(v2);

            F32 t = d2/d1;

            if (t > 0.f && t < 1.f)
            {
                LLVector3 intersect = v2+line*t;
                pp.push_back(intersect);
            }
        }
    }

    LLVector3 ext[] = { min-LLVector3(0.05f,0.05f,0.05f),
        max+LLVector3(0.05f,0.05f,0.05f) };

    for (U32 i = 0; i < pp.size(); ++i)
    {
        bool found = true;

        const F32* p = pp[i].mV;

        for (U32 j = 0; j < 3; ++j)
        {
            if (p[j] < ext[0].mV[j] ||
                p[j] > ext[1].mV[j])
            {
                found = false;
                break;
            }
        }

        for (U32 j = 0; j < LLCamera::AGENT_PLANE_NO_USER_CLIP_NUM; ++j)
        {
            const LLPlane& cp = camera.getAgentPlane(j);
            F32 dist = cp.dist(pp[i]);
            if (dist > 0.05f) //point is above some plane, not contained
            {
                found = false;
                break;
            }
        }

        if (found)
        {
            fp.push_back(pp[i]);
        }
    }

    if (fp.empty())
    {
        return false;
    }

    return true;
}

void LLPipeline::renderHighlight(const LLViewerObject* obj, F32 fade)
{
    if (obj && obj->getVolume())
    {
        for (LLViewerObject::child_list_t::const_iterator iter = obj->getChildren().begin(); iter != obj->getChildren().end(); ++iter)
        {
            renderHighlight(*iter, fade);
        }

        LLDrawable* drawable = obj->mDrawable;
        if (drawable)
        {
            for (S32 i = 0; i < drawable->getNumFaces(); ++i)
            {
                LLFace* face = drawable->getFace(i);
                if (face)
                {
                    face->renderSelected(LLViewerTexture::sNullImagep, LLColor4(1,1,1,fade));
                }
            }
        }
    }
}


LLRenderTarget* LLPipeline::getSunShadowTarget(U32 i)
{
    llassert(i < 4);
    return sPrismLensRender ? &mMainRT.shadow[i] : &mRT->shadow[i];
}

LLRenderTarget* LLPipeline::getSpotShadowTarget(U32 i)
{
    llassert(i < MAX_SPOT_SHADOWS);
    // [Prism spot shadows Stage 2] the aux capture samples its own dedicated
    // maps; the main-view maps must survive untouched for the main deferred
    // lighting that runs after the aux pass (parallel to getSunShadowTarget).
    return sPrismLensRender ? &mPrismSpotShadow[i] : &mSpotShadow[i];
}

// helper class for disabling occlusion culling for the current stack frame
class LLDisableOcclusionCulling
{
public:
    S32 mUseOcclusion;

    LLDisableOcclusionCulling()
    {
        mUseOcclusion = LLPipeline::sUseOcclusion;
        LLPipeline::sUseOcclusion = 0;
    }

    ~LLDisableOcclusionCulling()
    {
        LLPipeline::sUseOcclusion = mUseOcclusion;
    }
};

// Re-bucket a shared sun-shadow cull (produced by a single union octree walk) down to one
// cascade: copy the union's visible/drawable groups whose object bounds intersect this
// cascade's frustum into `dst` (the same AABBInFrustumObjectBounds test the per-cascade
// cull uses, so the geometry matches mode 0 exactly), then pass the small individual-
// drawable and bridge lists through unfiltered. stateSort then builds the cascade's render
// map from `dst`. Lets RenderShadowCullMode 1 share one octree walk across all cascades.
static void bucketShadowCull(LLCullResult& src, LLCamera& cam, LLCullResult& dst)
{
    dst.clear();

    for (LLCullResult::sg_iterator i = src.beginVisibleGroups(), end = src.endVisibleGroups(); i != end; ++i)
    {
        LLSpatialGroup* group = *i;
        if (!group->isDead() &&
            cam.AABBInFrustum(group->getObjectBounds()[0], group->getObjectBounds()[1]) > 0)
        {
            dst.pushVisibleGroup(group);
        }
    }

    for (LLCullResult::sg_iterator i = src.beginDrawableGroups(), end = src.endDrawableGroups(); i != end; ++i)
    {
        LLSpatialGroup* group = *i;
        if (!group->isDead() &&
            cam.AABBInFrustum(group->getObjectBounds()[0], group->getObjectBounds()[1]) > 0)
        {
            dst.pushDrawableGroup(group);
        }
    }

    // Individual drawables and spatial bridges (attachments/animesh) are few; pass them
    // through unfiltered -- conservative (they render into every cascade) but correct.
    for (LLCullResult::drawable_iterator i = src.beginVisibleList(), end = src.endVisibleList(); i != end; ++i)
    {
        dst.pushDrawable(*i);
    }

    for (LLCullResult::bridge_iterator i = src.beginVisibleBridge(), end = src.endVisibleBridge(); i != end; ++i)
    {
        dst.pushBridge(*i);
    }
}

void LLPipeline::generateSunShadow(LLCamera& camera)
{
    if (!sRenderDeferred || RenderShadowDetail <= 0)
    {
        return;
    }

    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE; //LL_RECORD_BLOCK_TIME(FTM_GEN_SUN_SHADOW);
    LL_PROFILE_GPU_ZONE("generateSunShadow");

    LLDisableOcclusionCulling no_occlusion;

    bool skip_avatar_update = false;
    if (!isAgentAvatarValid() || gAgentCamera.getCameraAnimating() || gAgentCamera.getCameraMode() != CAMERA_MODE_MOUSELOOK || !LLVOAvatar::sVisibleInFirstPerson)
    {
        skip_avatar_update = true;
    }

    if (!skip_avatar_update)
    {
        gAgentAvatarp->updateAttachmentVisibility(CAMERA_MODE_THIRD_PERSON);
    }

    glm::mat4 last_modelview = get_last_modelview();
    glm::mat4 last_projection = get_last_projection();

    pushRenderTypeMask();
    andRenderTypeMask(LLPipeline::RENDER_TYPE_SIMPLE,
                    LLPipeline::RENDER_TYPE_ALPHA,
                    LLPipeline::RENDER_TYPE_ALPHA_PRE_WATER,
                    LLPipeline::RENDER_TYPE_ALPHA_POST_WATER,
                    LLPipeline::RENDER_TYPE_GRASS,
                    LLPipeline::RENDER_TYPE_GLTF_PBR,
                    LLPipeline::RENDER_TYPE_FULLBRIGHT,
                    LLPipeline::RENDER_TYPE_BUMP,
                    LLPipeline::RENDER_TYPE_VOLUME,
                    LLPipeline::RENDER_TYPE_AVATAR,
                    LLPipeline::RENDER_TYPE_CONTROL_AV,
                    LLPipeline::RENDER_TYPE_TREE,
                    LLPipeline::RENDER_TYPE_TERRAIN,
                    LLPipeline::RENDER_TYPE_WATER,
                    LLPipeline::RENDER_TYPE_VOIDWATER,
                    LLPipeline::RENDER_TYPE_PASS_ALPHA,
                    LLPipeline::RENDER_TYPE_PASS_ALPHA_MASK,
                    LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK,
                    LLPipeline::RENDER_TYPE_PASS_GRASS,
                    LLPipeline::RENDER_TYPE_PASS_SIMPLE,
                    LLPipeline::RENDER_TYPE_PASS_BUMP,
                    LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT,
                    LLPipeline::RENDER_TYPE_PASS_SHINY,
                    LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_SHINY,
                    LLPipeline::RENDER_TYPE_PASS_MATERIAL,
                    LLPipeline::RENDER_TYPE_PASS_MATERIAL_ALPHA,
                    LLPipeline::RENDER_TYPE_PASS_MATERIAL_ALPHA_MASK,
                    LLPipeline::RENDER_TYPE_PASS_MATERIAL_ALPHA_EMISSIVE,
                    LLPipeline::RENDER_TYPE_PASS_SPECMAP,
                    LLPipeline::RENDER_TYPE_PASS_SPECMAP_BLEND,
                    LLPipeline::RENDER_TYPE_PASS_SPECMAP_MASK,
                    LLPipeline::RENDER_TYPE_PASS_SPECMAP_EMISSIVE,
                    LLPipeline::RENDER_TYPE_PASS_NORMMAP,
                    LLPipeline::RENDER_TYPE_PASS_NORMMAP_BLEND,
                    LLPipeline::RENDER_TYPE_PASS_NORMMAP_MASK,
                    LLPipeline::RENDER_TYPE_PASS_NORMMAP_EMISSIVE,
                    LLPipeline::RENDER_TYPE_PASS_NORMSPEC,
                    LLPipeline::RENDER_TYPE_PASS_NORMSPEC_BLEND,
                    LLPipeline::RENDER_TYPE_PASS_NORMSPEC_MASK,
                    LLPipeline::RENDER_TYPE_PASS_NORMSPEC_EMISSIVE,
                    LLPipeline::RENDER_TYPE_PASS_ALPHA_MASK_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_SIMPLE_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_BUMP_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_SHINY_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_SHINY_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_MATERIAL_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_MATERIAL_ALPHA_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_MATERIAL_ALPHA_MASK_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_MATERIAL_ALPHA_EMISSIVE_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_SPECMAP_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_SPECMAP_BLEND_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_SPECMAP_MASK_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_SPECMAP_EMISSIVE_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_NORMMAP_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_NORMMAP_BLEND_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_NORMMAP_MASK_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_NORMMAP_EMISSIVE_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_NORMSPEC_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_NORMSPEC_BLEND_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_NORMSPEC_MASK_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_NORMSPEC_EMISSIVE_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_GLTF_PBR,
                    LLPipeline::RENDER_TYPE_PASS_GLTF_PBR_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_GLTF_PBR_ALPHA_MASK,
                    LLPipeline::RENDER_TYPE_PASS_GLTF_PBR_ALPHA_MASK_RIGGED,
                    END_RENDER_TYPES);

    gGL.setColorMask(false, false);

    LLEnvironment& environment = LLEnvironment::instance();

    //get sun view matrix

    //store current projection/modelview matrix
    glm::mat4 saved_proj = get_current_projection();
    glm::mat4 saved_view = get_current_modelview();
    glm::mat4 inv_view = glm::inverse(saved_view);

    glm::mat4 view[MAX_SHADOW_MATS]; // [BDMerge NSpot]
    glm::mat4 proj[MAX_SHADOW_MATS];

    LLVector3 caster_dir(environment.getIsSunUp() ? mSunDir : mMoonDir);

    //put together a universal "near clip" plane for shadow frusta
    LLPlane shadow_near_clip;
    {
        LLVector3 p = camera.getOrigin(); // gAgent.getPositionAgent();
        p += caster_dir * RenderFarClip*2.f;
        shadow_near_clip.setVec(p, caster_dir);
    }

    LLVector3 lightDir = -caster_dir;
    lightDir.normVec();

    //create light space camera matrix
    LLVector3 at = lightDir;

    LLVector3 up = camera.getAtAxis();

    if (fabsf(up*lightDir) > 0.75f)
    {
        up = camera.getUpAxis();
    }

    up.normVec();
    at.normVec();


    LLCamera main_camera = camera;

    F32 near_clip = 0.f;
    {
        //get visible point cloud
        static std::vector<LLVector3> fp;
        fp.clear();

        main_camera.calcAgentFrustumPlanes(main_camera.mAgentFrustum);

        LLVector3 min,max;
        getVisiblePointCloud(main_camera,min,max,fp);

        if (fp.empty())
        {
            if (!hasRenderDebugMask(RENDER_DEBUG_SHADOW_FRUSTA) && !gCubeSnapshot)
            {
                mShadowCamera[0] = main_camera;
                mShadowExtents[0][0] = min;
                mShadowExtents[0][1] = max;

                mShadowFrustPoints[0].clear();
                mShadowFrustPoints[1].clear();
                mShadowFrustPoints[2].clear();
                mShadowFrustPoints[3].clear();
            }
            popRenderTypeMask();

            if (!skip_avatar_update)
            {
                gAgentAvatarp->updateAttachmentVisibility(gAgentCamera.getCameraMode());
            }

            return;
        }

        //get good split distances for frustum
        for (U32 i = 0; i < fp.size(); ++i)
        {
            glm::vec3 v(fp[i]);
            v = mul_mat4_vec3(saved_view, v);
            fp[i] = LLVector3(v);
        }

        min = fp[0];
        max = fp[0];

        //get camera space bounding box
        for (U32 i = 1; i < fp.size(); ++i)
        {
            update_min_max(min, max, fp[i]);
        }

        near_clip    = llclamp(-max.mV[2], 0.01f, 4.0f);
        F32 far_clip = llclamp(-min.mV[2]*2.f, 16.0f, 512.0f);

        //far_clip = llmin(far_clip, 128.f);
        far_clip = llmin(far_clip, camera.getFar());

        F32 range = far_clip-near_clip;

        LLVector3 split_exp = RenderShadowSplitExponent;

        F32 da = 1.f-llmax( fabsf(lightDir*up), fabsf(lightDir*camera.getLeftAxis()) );

        da = powf(da, split_exp.mV[2]);

        F32 sxp = split_exp.mV[1] + (split_exp.mV[0]-split_exp.mV[1])*da;

        for (U32 i = 0; i < 4; ++i)
        {
            F32 x = (F32)(i+1)/4.f;
            x = powf(x, sxp);
            mSunClipPlanes.mV[i] = near_clip+range*x;
        }

        mSunClipPlanes.mV[0] *= 1.25f; //bump back first split for transition padding
    }

    if (gCubeSnapshot)
    { // stretch clip planes for reflection probe renders to reduce number of shadow passes
        mSunClipPlanes.mV[1] = mSunClipPlanes.mV[2];
        mSunClipPlanes.mV[2] = mSunClipPlanes.mV[3];
        mSunClipPlanes.mV[3] *= 1.5f;
    }


    // convenience array of 4 near clip plane distances
    F32 dist[] = { near_clip, mSunClipPlanes.mV[0], mSunClipPlanes.mV[1], mSunClipPlanes.mV[2], mSunClipPlanes.mV[3] };

    if (mSunDiffuse == LLColor4::black)
    { //sun diffuse is totally black shadows don't matter
        skipRenderingShadows();
    }
    else
    {
        // RenderShadowCullMode 1: do the expensive octree cull ONCE against a frustum
        // spanning every sun cascade, then have each cascade cheaply re-bucket the union's
        // visible groups by its own frustum (bucketShadowCull) and build its own render
        // map. Saves 3 of 4 octree walks per frame while each cascade still renders only
        // its own slice -- GPU-neutral vs. per-cascade culling, so it helps CPU-bound
        // targets without regressing GPU-bound ones. Disabled in cube snapshots. Default 0.
        static LLCachedControl<S32> sShadowCullMode(gSavedSettings, "RenderShadowCullMode", 0);
        bool have_union_cull = false;
        static LLCullResult sUnionShadowResult;
        if (sShadowCullMode() == 1 && !gCubeSnapshot)
        {
            // updateFrustumPlanes below seeds the frustum corners from the *current* GL
            // matrices, and earlier setup in this function leaves them in a non-main-view
            // state. Restore the saved (main-view) matrices first, as the cascade loop
            // does each iteration, so the corner directions used below are correct.
            set_current_modelview(saved_view);
            set_current_projection(saved_proj);

            LLCamera ucam = camera;
            ucam.setFar(16.f);
            LLViewerCamera::updateFrustumPlanes(ucam, false, false, true);

            LLVector3 ueye = camera.getOrigin();
            LLVector3* ufrust = ucam.mAgentFrustum;
            LLVector3 upn = ucam.getAtAxis();
            for (U32 i = 0; i < 4; i++)
            {
                LLVector3 delta = ufrust[i+4]-ueye;
                delta += (ufrust[i+4]-ufrust[(i+2)%4+4])*0.05f;
                delta.normVec();
                F32 dp = delta*upn;
                ufrust[i]   = ueye + (delta*dist[0]*0.75f)/dp;
                ufrust[i+4] = ueye + (delta*dist[4]*1.25f)/dp;
            }

            {
                glm::mat4 uview = look(camera.getOrigin(), lightDir, -up);

                // AABB the 8 full-range frustum corners directly in light space. ufrust
                // spans [dist[0], dist[4]] (built above), so this box is a guaranteed
                // superset of every cascade. getVisiblePointCloud is NOT usable here: the
                // far corners sit past the view far plane, so it clips the cloud down to
                // the 4 near corners and the union collapses to a dot at the camera.
                LLVector3 mn(mul_mat4_vec3(uview, glm::vec3(ufrust[0])));
                LLVector3 mx = mn;
                for (U32 i = 1; i < 8; i++)
                {
                    LLVector3 p(mul_mat4_vec3(uview, glm::vec3(ufrust[i])));
                    update_min_max(mn, mx, p);
                }

                LLVector3 ucenter = (mn+mx)*0.5f;

                // Conservative ortho light-space projection bounding the whole point
                // cloud. updateFrustumPlanes derives the cull frustum from the *current*
                // GL modelview/projection, so set them here. Ortho is looser than the
                // per-cascade perspective fit, so the result is a superset of every
                // cascade frustum -- no dropped casters.
                //
                // Pad the depth range: with the sun near-overhead the light-space
                // footprint is nearly planar (znear ~= zfar), which makes glm::ortho
                // singular and updateFrustumPlanes unproject to NaN frustum corners --
                // shadows then drop and flip with camera angle. The near plane is
                // replaced by shadow_near_clip below and the far only needs to clear the
                // receivers, so widening the depth range is always safe.
                F32 zpad = llmax(mx.mV[0] - mn.mV[0], mx.mV[1] - mn.mV[1]) * 0.5f + 1.f;
                glm::mat4 uproj = glm::ortho(mn.mV[0], mx.mV[0], mn.mV[1], mx.mV[1], -mx.mV[2] - zpad, -mn.mV[2] + zpad);

                ucam.setOriginAndLookAt(ueye, up, ucenter);
                ucam.setOrigin(0, 0, 0);

                LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_SUN_SHADOW0;
                set_current_modelview(uview);
                set_current_projection(uproj);
                LLViewerCamera::updateFrustumPlanes(ucam, false, false, true);
                ucam.getAgentPlane(LLCamera::AGENT_PLANE_NEAR).set(shadow_near_clip);

                bool saved_shadow_render = LLPipeline::sShadowRender;
                U32 saved_occlusion = sUseOcclusion;
                LLPipeline::sShadowRender = true;
                // Disable occlusion culling for the shadow cull exactly as renderShadow
                // does: occlusion queries are main-camera and previous-frame based, so
                // leaving them on wrongly culls casters hidden from the main view (their
                // shadows still show) and flickers as the queries resolve frame to frame.
                sUseOcclusion = 0;
                // One octree walk for the whole sun shadow. No stateSort here -- each
                // cascade re-buckets these visible groups and sorts its own render map.
                updateCull(ucam, sUnionShadowResult);
                sUseOcclusion = saved_occlusion;
                LLPipeline::sShadowRender = saved_shadow_render;

                // restore main matrices (the cascade loop sets its own each iteration)
                set_current_modelview(saved_view);
                set_current_projection(saved_proj);

                have_union_cull = true;
            }
        }

        for (S32 j = 0; j < (gCubeSnapshot ? 2 : 4); j++)
        {
            if (!hasRenderDebugMask(RENDER_DEBUG_SHADOW_FRUSTA) && !gCubeSnapshot)
            {
                mShadowFrustPoints[j].clear();
            }

            LLViewerCamera::sCurCameraID = (LLViewerCamera::eCameraID)(LLViewerCamera::CAMERA_SUN_SHADOW0+j);

            //restore render matrices
            set_current_modelview(saved_view);
            set_current_projection(saved_proj);

            LLVector3 eye = camera.getOrigin();
            llassert(eye.isFinite());

            //camera used for shadow cull/render
            LLCamera shadow_cam;

            //create world space camera frustum for this split
            shadow_cam = camera;
            shadow_cam.setFar(16.f);

            LLViewerCamera::updateFrustumPlanes(shadow_cam, false, false, true);

            LLVector3* frust = shadow_cam.mAgentFrustum;

            LLVector3 pn = shadow_cam.getAtAxis();

            LLVector3 min, max;

            //construct 8 corners of split frustum section
            for (U32 i = 0; i < 4; i++)
            {
                LLVector3 delta = frust[i+4]-eye;
                delta += (frust[i+4]-frust[(i+2)%4+4])*0.05f;
                delta.normVec();
                F32 dp = delta*pn;
                frust[i] = eye + (delta*dist[j]*0.75f)/dp;
                frust[i+4] = eye + (delta*dist[j+1]*1.25f)/dp;
            }

            shadow_cam.calcAgentFrustumPlanes(frust);
            shadow_cam.mFrustumCornerDist = 0.f;

            if (!gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_SHADOW_FRUSTA) && !gCubeSnapshot)
            {
                mShadowCamera[j] = shadow_cam;
            }

            static std::vector<LLVector3> fp;
            fp.clear();

            if (!gPipeline.getVisiblePointCloud(shadow_cam, min, max, fp, lightDir)
                || j > RenderShadowSplits)
            {
                //no possible shadow receivers
                if (!gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_SHADOW_FRUSTA) && !gCubeSnapshot)
                {
                    mShadowExtents[j][0] = LLVector3();
                    mShadowExtents[j][1] = LLVector3();
                    mShadowCamera[j+4] = shadow_cam;
                }

                mRT->shadow[j].bindTarget();
                {
                    LLGLDepthTest depth(GL_TRUE);
                    mRT->shadow[j].clear();
                }
                mRT->shadow[j].flush();

                mShadowError.mV[j] = 0.f;
                mShadowFOV.mV[j] = 0.f;

                continue;
            }

            if (!gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_SHADOW_FRUSTA) && !gCubeSnapshot)
            {
                mShadowExtents[j][0] = min;
                mShadowExtents[j][1] = max;
                mShadowFrustPoints[j] = fp;
            }


            //find a good origin for shadow projection
            LLVector3 origin;

            //get a temporary view projection
            view[j] = look(camera.getOrigin(), lightDir, -up);

            std::vector<LLVector3> wpf;

            for (U32 i = 0; i < fp.size(); i++)
            {
                glm::vec3 p(fp[i]);
                p = mul_mat4_vec3(view[j], p);
                wpf.push_back(LLVector3(p));
            }

            min = wpf[0];
            max = wpf[0];

            for (U32 i = 0; i < fp.size(); ++i)
            { //get AABB in camera space
                update_min_max(min, max, wpf[i]);
            }

            // Construct a perspective transform with perspective along y-axis that contains
            // points in wpf
            //Known:
            // - far clip plane
            // - near clip plane
            // - points in frustum
            //Find:
            // - origin

            //get some "interesting" points of reference
            LLVector3 center = (min+max)*0.5f;
            LLVector3 size = (max-min)*0.5f;
            LLVector3 near_center = center;
            near_center.mV[1] += size.mV[1]*2.f;


            //put all points in wpf in quadrant 0, reletive to center of min/max
            //get the best fit line using least squares
            F32 bfm = 0.f;
            F32 bfb = 0.f;

            for (U32 i = 0; i < wpf.size(); ++i)
            {
                wpf[i] -= center;
                wpf[i].mV[0] = fabsf(wpf[i].mV[0]);
                wpf[i].mV[2] = fabsf(wpf[i].mV[2]);
            }

            if (!wpf.empty())
            {
                F32 sx = 0.f;
                F32 sx2 = 0.f;
                F32 sy = 0.f;
                F32 sxy = 0.f;

                for (U32 i = 0; i < wpf.size(); ++i)
                {
                    sx += wpf[i].mV[0];
                    sx2 += wpf[i].mV[0]*wpf[i].mV[0];
                    sy += wpf[i].mV[1];
                    sxy += wpf[i].mV[0]*wpf[i].mV[1];
                }

                bfm = (sy*sx-wpf.size()*sxy)/(sx*sx-wpf.size()*sx2);
                bfb = (sx*sxy-sy*sx2)/(sx*sx-bfm*sx2);
            }

            {
                // best fit line is y=bfm*x+bfb

                //find point that is furthest to the right of line
                F32 off_x = -1.f;
                LLVector3 lp;

                for (U32 i = 0; i < wpf.size(); ++i)
                {
                    //y = bfm*x+bfb
                    //x = (y-bfb)/bfm
                    F32 lx = (wpf[i].mV[1]-bfb)/bfm;

                    lx = wpf[i].mV[0]-lx;

                    if (off_x < lx)
                    {
                        off_x = lx;
                        lp = wpf[i];
                    }
                }

                //get line with slope bfm through lp
                // bfb = y-bfm*x
                bfb = lp.mV[1]-bfm*lp.mV[0];

                //calculate error
                mShadowError.mV[j] = 0.f;

                for (U32 i = 0; i < wpf.size(); ++i)
                {
                    F32 lx = (wpf[i].mV[1]-bfb)/bfm;
                    mShadowError.mV[j] += fabsf(wpf[i].mV[0]-lx);
                }

                mShadowError.mV[j] /= wpf.size();
                mShadowError.mV[j] /= size.mV[0];

                if (mShadowError.mV[j] > RenderShadowErrorCutoff)
                { //just use ortho projection
                    mShadowFOV.mV[j] = -1.f;
                    origin.clearVec();
                    proj[j] = glm::ortho(min.mV[0], max.mV[0],
                                        min.mV[1], max.mV[1],
                                        -max.mV[2], -min.mV[2]);
                }
                else
                {
                    //origin is where line x = 0;
                    origin.setVec(0,bfb,0);

                    F32 fovz = 1.f;
                    F32 fovx = 1.f;

                    LLVector3 zp;
                    LLVector3 xp;

                    for (U32 i = 0; i < wpf.size(); ++i)
                    {
                        LLVector3 atz = wpf[i]-origin;
                        atz.mV[0] = 0.f;
                        atz.normVec();
                        if (fovz > -atz.mV[1])
                        {
                            zp = wpf[i];
                            fovz = -atz.mV[1];
                        }

                        LLVector3 atx = wpf[i]-origin;
                        atx.mV[2] = 0.f;
                        atx.normVec();
                        if (fovx > -atx.mV[1])
                        {
                            fovx = -atx.mV[1];
                            xp = wpf[i];
                        }
                    }

                    fovx = acos(fovx);
                    fovz = acos(fovz);

                    F32 cutoff = llmin((F32) RenderShadowFOVCutoff, 1.4f);

                    mShadowFOV.mV[j] = fovx;

                    if (fovx < cutoff && fovz > cutoff)
                    {
                        //x is a good fit, but z is too big, move away from zp enough so that fovz matches cutoff
                        F32 d = zp.mV[2]/tan(cutoff);
                        F32 ny = zp.mV[1] + fabsf(d);

                        origin.mV[1] = ny;

                        fovz = 1.f;
                        fovx = 1.f;

                        for (U32 i = 0; i < wpf.size(); ++i)
                        {
                            LLVector3 atz = wpf[i]-origin;
                            atz.mV[0] = 0.f;
                            atz.normVec();
                            fovz = llmin(fovz, -atz.mV[1]);

                            LLVector3 atx = wpf[i]-origin;
                            atx.mV[2] = 0.f;
                            atx.normVec();
                            fovx = llmin(fovx, -atx.mV[1]);
                        }

                        fovx = acos(fovx);
                        fovz = acos(fovz);

                        mShadowFOV.mV[j] = cutoff;
                    }


                    origin += center;

                    F32 ynear = -(max.mV[1]-origin.mV[1]);
                    F32 yfar = -(min.mV[1]-origin.mV[1]);

                    if (ynear < 0.1f) //keep a sensible near clip plane
                    {
                        F32 diff = 0.1f-ynear;
                        origin.mV[1] += diff;
                        ynear += diff;
                        yfar += diff;
                    }

                    if (fovx > cutoff)
                    { //just use ortho projection
                        origin.clearVec();
                        mShadowError.mV[j] = -1.f;
                        proj[j] = glm::ortho(min.mV[0], max.mV[0],
                                min.mV[1], max.mV[1],
                                -max.mV[2], -min.mV[2]);
                    }
                    else
                    {
                        //get perspective projection
                        view[j] = glm::inverse(view[j]);
                        //llassert(origin.isFinite());

                        glm::vec3 origin_agent(origin);

                        //translate view to origin
                        origin_agent = mul_mat4_vec3(view[j], origin_agent);

                        eye = LLVector3(origin_agent);
                        //llassert(eye.isFinite());
                        if (!hasRenderDebugMask(LLPipeline::RENDER_DEBUG_SHADOW_FRUSTA) && !gCubeSnapshot)
                        {
                            mShadowFrustOrigin[j] = eye;
                        }

                        view[j] = look(LLVector3(origin_agent), lightDir, -up);

                        F32 fx = 1.f/tanf(fovx);
                        F32 fz = 1.f/tanf(fovz);

                        proj[j] = glm::mat4(-fx, 0, 0, 0,
                            0, (yfar + ynear) / (ynear - yfar), 0, -1.0f,
                            0, 0, -fz, 0,
                            0, (2.f * yfar * ynear) / (ynear - yfar), 0, 0);
                    }
                }
            }

            //shadow_cam.setFar(128.f);
            shadow_cam.setOriginAndLookAt(eye, up, center);

            shadow_cam.setOrigin(0,0,0);

            set_current_modelview(view[j]);
            set_current_projection(proj[j]);

            LLViewerCamera::updateFrustumPlanes(shadow_cam, false, false, true);

            //shadow_cam.ignoreAgentFrustumPlane(LLCamera::AGENT_PLANE_NEAR);
            shadow_cam.getAgentPlane(LLCamera::AGENT_PLANE_NEAR).set(shadow_near_clip);

            //translate and scale to from [-1, 1] to [0, 1]
            glm::mat4 trans(0.5f, 0.0f, 0.0f, 0.0f,
                            0.0f, 0.5f, 0.0f, 0.0f,
                            0.0f, 0.0f, 0.5f, 0.0f,
                            0.5f, 0.5f, 0.5f, 1.0f);

            set_current_modelview(view[j]);
            set_current_projection(proj[j]);

            set_last_modelview(mShadowModelview[j]);
            set_last_projection(mShadowProjection[j]);

            mShadowModelview[j] = view[j];
            mShadowProjection[j] = proj[j];
            mSunShadowMatrix[j] = trans*proj[j]*view[j]*inv_view;

            stop_glerror();

            mRT->shadow[j].bindTarget();
            mRT->shadow[j].getViewport(gGLViewport);
            mRT->shadow[j].clear();

            {
                static LLCullResult result[4];
                if (have_union_cull)
                {   // re-bucket the shared union cull down to this cascade's frustum
                    bucketShadowCull(sUnionShadowResult, shadow_cam, result[j]);
                }
                renderShadow(view[j], proj[j], shadow_cam, result[j], true, !have_union_cull);
            }

            mRT->shadow[j].flush();

            if (!gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_SHADOW_FRUSTA) && !gCubeSnapshot)
            {
                mShadowCamera[j+4] = shadow_cam;
            }
        }
    }

    //hack to disable projector shadows
    bool gen_shadow = RenderShadowDetail > 1;

    if (gen_shadow)
    {
        if (!gCubeSnapshot) //skip updating spot shadow maps during cubemap updates
        {
            LLTrace::CountStatHandle<>* velocity_stat = LLViewerCamera::getVelocityStat();
            F32 fade_amt = gFrameIntervalSeconds.value()
                * (F32)llmax(LLTrace::get_frame_recording().getLastRecording().getSum(*velocity_stat) / LLTrace::get_frame_recording().getLastRecording().getDuration().value(), 1.0);

            // should never happen
            llassert(mTargetShadowSpotLight[0] != mTargetShadowSpotLight[1] || mTargetShadowSpotLight[0].isNull());

            //update shadow targets
            const U32 num_spots = bdmergeMaxSpotShadows(); // [BDMerge NSpot]

            // [BDMerge NSpot debug] throttled slot-state dump for diagnosing
            // assignment bugs; enable BDMergeSpotDebug and grep the log
            static LLCachedControl<bool> spot_debug(gSavedSettings, "BDMergeSpotDebug", false);
            static LLFrameTimer spot_debug_timer;
            if (spot_debug && spot_debug_timer.getElapsedTimeF32() > 2.f)
            {
                spot_debug_timer.reset();
                std::ostringstream out;
                out << "[NSpotDbg] n=" << num_spots;
                for (U32 i = 0; i < num_spots; i++)
                {
                    out << " | s" << i << " w=" << mSpotShadow[i].getWidth()
                        << " light=" << (mShadowSpotLight[i].notNull() ? "Y" : "-")
                        << " fade=" << mSpotLightFade[i];
                }
                out << " || targets:";
                for (U32 i = 0; i < num_spots; i++)
                {
                    if (mTargetShadowSpotLight[i].notNull())
                    {
                        LLVOVolume* v = mTargetShadowSpotLight[i]->getVOVolume();
                        out << " t" << i << " pri=" << (v ? v->getSpotLightPriority() : -1.f);
                    }
                    else
                    {
                        out << " t" << i << "=-";
                    }
                }
                LL_INFOS("NSpotDbg") << out.str() << LL_ENDL;
            }

            for (U32 i = 0; i < num_spots; i++)
            { //for each current shadow
                if (mSpotShadow[i].getWidth() == 0)
                { // not allocated yet (deferred realloc); skip this frame
                    continue;
                }
                LLViewerCamera::sCurCameraID = (LLViewerCamera::eCameraID)(LLViewerCamera::CAMERA_SPOT_SHADOW0 + i);

                bool is_target = false;
                if (mShadowSpotLight[i].notNull())
                {
                    for (U32 j = 0; j < num_spots; j++)
                    {
                        if (mShadowSpotLight[i] == mTargetShadowSpotLight[j])
                        {
                            is_target = true;
                            break;
                        }
                    }
                }

                if (is_target)
                { //keep this spotlight
                    mSpotLightFade[i] = llmin(mSpotLightFade[i] + fade_amt, 1.f);
                }
                else
                { //fade out this light
                    mSpotLightFade[i] = llmax(mSpotLightFade[i] - fade_amt, 0.f);

                    if (mSpotLightFade[i] == 0.f || mShadowSpotLight[i].isNull())
                    { //faded out, grab the first pending spot not already held by another slot
                        for (U32 j = 0; j < num_spots; j++)
                        {
                            if (mTargetShadowSpotLight[j].isNull())
                            {
                                continue;
                            }
                            bool held = false;
                            for (U32 k = 0; k < num_spots; k++)
                            {
                                if (k != i && mShadowSpotLight[k] == mTargetShadowSpotLight[j])
                                {
                                    held = true;
                                    break;
                                }
                            }
                            if (!held)
                            {
                                mShadowSpotLight[i] = mTargetShadowSpotLight[j];
                                break;
                            }
                        }
                    }
                }
            }
        }

        // this should never happen
        llassert(mShadowSpotLight[0] != mShadowSpotLight[1] || mShadowSpotLight[0].isNull());

        for (S32 i = 0; i < (S32)bdmergeMaxSpotShadows(); i++) // [BDMerge NSpot]
        {
            if (mSpotShadow[i].getWidth() == 0)
            { // slot count raised before the deferred realloc ran - target
              // not allocated yet this frame; skip (crash guard)
                mShadowSpotLight[i] = NULL;
                continue;
            }

            set_current_modelview(saved_view);
            set_current_projection(saved_proj);

            if (mShadowSpotLight[i].isNull())
            {
                continue;
            }

            LLVOVolume* volume = mShadowSpotLight[i]->getVOVolume();

            if (!volume)
            {
                mShadowSpotLight[i] = NULL;
                continue;
            }

            LLDrawable* drawable = mShadowSpotLight[i];

            LLVector3 params = volume->getSpotLightParams();
            F32 fov = params.mV[0];

            //get agent->light space matrix (modelview)
            LLVector3 center = drawable->getPositionAgent();
            LLQuaternion quat = volume->getRenderRotation();

            //get near clip plane
            LLVector3 scale = volume->getScale();
            LLVector3 at_axis(0, 0, -scale.mV[2] * 0.5f);
            at_axis *= quat;

            LLVector3 np = center + at_axis;
            at_axis.normVec();

            //get origin that has given fov for plane np, at_axis, and given scale
            F32 dist = (scale.mV[1] * 0.5f) / tanf(fov * 0.5f);

            LLVector3 origin = np - at_axis * dist;

            LLMatrix4 mat(quat, LLVector4(origin, 1.f));

            view[i + 4] = glm::make_mat4((F32*)mat.mMatrix);

            view[i + 4] = glm::inverse(view[i + 4]);

            //get perspective matrix
            F32 near_clip = dist + 0.01f;
            F32 width = scale.mV[VX];
            F32 height = scale.mV[VY];
            F32 far_clip = dist + volume->getLightRadius() * 1.5f;

            F32 fovy = fov; // radians
            F32 aspect = width / height;

            proj[i + 4] = glm::perspective(fovy, aspect, near_clip, far_clip);

            //translate and scale to from [-1, 1] to [0, 1]
            glm::mat4 trans(0.5f, 0.0f, 0.0f, 0.0f,
                            0.0f, 0.5f, 0.0f, 0.0f,
                            0.0f, 0.0f, 0.5f, 0.0f,
                            0.5f, 0.5f, 0.5f, 1.0f);

            set_current_modelview(view[i + 4]);
            set_current_projection(proj[i + 4]);

            mSunShadowMatrix[i + 4] = trans * proj[i + 4] * view[i + 4] * inv_view;

            set_last_modelview(mShadowModelview[i + 4]);
            set_last_projection(mShadowProjection[i + 4]);

            mShadowModelview[i + 4] = view[i + 4];
            mShadowProjection[i + 4] = proj[i + 4];

            if (!gCubeSnapshot) //skip updating spot shadow maps during cubemap updates
            {
                LLCamera shadow_cam = camera;
                shadow_cam.setFar(far_clip);
                shadow_cam.setOrigin(origin);

                LLViewerCamera::updateFrustumPlanes(shadow_cam, false, false, true);

                //

                mSpotShadow[i].bindTarget();
                mSpotShadow[i].getViewport(gGLViewport);
                mSpotShadow[i].clear();

                static LLCullResult result[MAX_SPOT_SHADOWS]; // [BDMerge NSpot fix2] was [2] - slot 3+ stomped past the array (the actual hard-crash root cause, symbolized to LLCullResult::pushVisibleGroup)

                LLViewerCamera::sCurCameraID = (LLViewerCamera::eCameraID)(LLViewerCamera::CAMERA_SPOT_SHADOW0 + i);

                RenderSpotLight = drawable;

                renderShadow(view[i + 4], proj[i + 4], shadow_cam, result[i], false);

                RenderSpotLight = nullptr;

                mSpotShadow[i].flush();
            }
        }
    }
    else
    { //no spotlight shadows
        mShadowSpotLight[0] = mShadowSpotLight[1] = NULL;
    }


    if (!CameraOffset)
    {
        set_current_modelview(saved_view);
        set_current_projection(saved_proj);
    }
    else
    {
        set_current_modelview(view[1]);
        set_current_projection(proj[1]);
        gGL.loadMatrix(glm::value_ptr(view[1]));
        gGL.matrixMode(LLRender::MM_PROJECTION);
        gGL.loadMatrix(glm::value_ptr(proj[1]));
        gGL.matrixMode(LLRender::MM_MODELVIEW);
    }
    gGL.setColorMask(true, true);

    set_last_modelview(last_modelview);
    set_last_projection(last_projection);

    popRenderTypeMask();

    if (!skip_avatar_update)
    {
        gAgentAvatarp->updateAttachmentVisibility(gAgentCamera.getCameraMode());
    }
}

// [Prism spot shadows Stage 2] Generate projector/spot shadow maps for the
// Prism auxiliary capture (surface lens + camera feed). This is a contained
// mirror of generateSunShadow's spot-shadow loop (same frustum math, same
// guards, same renderShadow call) with exactly four differences:
//  - depth renders into the DEDICATED mPrismSpotShadow[] targets, never into
//    mSpotShadow[] (the main view consumes those maps after this aux pass, at
//    the main stateSort / deferred lighting later in the same frame);
//  - the sampling matrix embeds the AUX camera's inverse view, so the shader's
//    aux view-space positions project into the map correctly;
//  - projector selection is STATELESS: it iterates mLights with the same
//    validity filter as calcNearbyLights' Prism-only branch and ranks locally
//    by distance. It never calls updateSpotLightPriority(), which mutates the
//    persistent (unsaved) LLVOVolume::mSpotLightPriority;
//  - it runs from LLPrismLens::renderAuxiliaryView, immediately BEFORE the aux
//    updateCull/stateSort. renderShadow runs its own stateSort per map, so the
//    aux scene stateSort must come after to rebuild the aux draw maps.
// Main-view byte-identity: every shared scalar/matrix mutated here
// (mShadowSpotLight[], mSpotLightFade[], mShadowModelview[4..9],
// mShadowProjection[4..9], mSunShadowMatrix[4..9]) was saved by
// beginPrismAuxiliaryState and is restored by endPrismAuxiliaryState, which
// fires (via ScopedPrismRenderState's dtor) before the main stateSort.
void LLPipeline::generatePrismSpotShadows(LLCamera& camera)
{
    // Match the main-view projector-shadow gate (generateSunShadow's
    // "gen_shadow"): if the user's spot shadows are globally off, the feed
    // shows none either. Never runs outside the aux scope or in a snapshot.
    if (!sRenderDeferred || RenderShadowDetail <= 1 || !sPrismLensRender || gCubeSnapshot)
    {
        return;
    }

    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    LL_PROFILE_GPU_ZONE("generatePrismSpotShadows");

    LLDisableOcclusionCulling no_occlusion;

    // Aux camera state to re-install before returning: the aux
    // updateCull/stateSort run right after this, so the depth passes below
    // must not leave their matrices/viewport/camera-id behind.
    const glm::mat4 saved_proj = get_current_projection();
    const glm::mat4 saved_view = get_current_modelview();
    const glm::mat4 last_modelview = get_last_modelview();
    const glm::mat4 last_projection = get_last_projection();
    const glm::mat4 inv_view_aux = glm::inverse(saved_view);
    S32 saved_viewport[4];
    for (U32 v = 0; v < 4; ++v)
    {
        saved_viewport[v] = gGLViewport[v];
    }
    const U32 saved_cur_res_x = LLRenderTarget::sCurResX;
    const U32 saved_cur_res_y = LLRenderTarget::sCurResY;
    const LLViewerCamera::eCameraID saved_camera_id = LLViewerCamera::sCurCameraID;

    // --- Stateless projector selection ------------------------------------
    // Same admission filter as calcNearbyLights' Prism branch (is-light, not
    // HUD, attachment toggles, radius/color), narrowed to spotlights and the
    // per-projector cast-shadows opt-out. Ranked nearest-first with a pointer
    // tie-break so the choice is deterministic frame to frame.
    struct PrismSpotShadowCandidate
    {
        F32 dist;
        LLDrawable* drawable;
    };
    std::vector<PrismSpotShadowCandidate> candidates;
    static LLCachedControl<S32> prism_local_light_count(
        gSavedSettings, "RenderLocalLightCount", 256);
    static LLCachedControl<F32> prism_light_scale(
        gSavedSettings, "AlchemyGlobalLightScale", 1.f);
    const U32 num_spots = bdmergeMaxSpotShadows(); // [BDMerge NSpot]
    if (prism_local_light_count >= 1)
    {
        const LLVector3 cam_pos = camera.getOrigin();
        const F32 max_dist = llmin(RenderFarClip, camera.getFar());
        for (LLDrawable* drawable : mLights)
        {
            LLVOVolume* light = drawable ? drawable->getVOVolume() : nullptr;
            if (!light || !drawable->isState(LLDrawable::LIGHT) ||
                light->isHUDAttachment())
            {
                continue;
            }
            if (!light->isLightSpotlight())
            { // only projectors cast spot shadows
                continue;
            }
            if (light->isAttachment())
            {
                if (!sRenderAttachedLights)
                {
                    continue;
                }
                LLVOAvatar* avatar = light->getAvatar();
                if (!bdmerge_should_render_light(true, avatar == gAgentAvatarp) ||
                    (avatar && (avatar->isTooComplex() || avatar->isInMuteList() ||
                                avatar->isTooSlow())))
                {
                    continue;
                }
            }
            else if (!bdmerge_should_render_light(false, false))
            {
                continue;
            }
            const F32 light_radius = light->getLightRadius() * 1.5f;
            const LLColor3 light_color =
                light->getLightLinearColor() * (F32)prism_light_scale;
            if (light_radius <= 0.001f ||
                light_color.magVecSquared() < 0.001f)
            {
                continue;
            }
            if (isProjectorShadowSuppressed(light))
            { // per-projector "cast shadows" opt-out applies in the feed too
                continue;
            }
            candidates.push_back(
                { calc_light_dist(light, cam_pos, max_dist), drawable });
        }
        std::sort(candidates.begin(), candidates.end(),
                  [](const PrismSpotShadowCandidate& a,
                     const PrismSpotShadowCandidate& b)
                  {
                      if (a.dist < b.dist) return true;
                      if (b.dist < a.dist) return false;
                      return a.drawable < b.drawable;
                  });
        if (candidates.size() > num_spots)
        {
            candidates.resize(num_spots);
        }
    }
    const U32 chosen = static_cast<U32>(candidates.size());

    // Same shadow render-type mask generateSunShadow pushes.
    pushRenderTypeMask();
    andRenderTypeMask(LLPipeline::RENDER_TYPE_SIMPLE,
                    LLPipeline::RENDER_TYPE_ALPHA,
                    LLPipeline::RENDER_TYPE_ALPHA_PRE_WATER,
                    LLPipeline::RENDER_TYPE_ALPHA_POST_WATER,
                    LLPipeline::RENDER_TYPE_GRASS,
                    LLPipeline::RENDER_TYPE_GLTF_PBR,
                    LLPipeline::RENDER_TYPE_FULLBRIGHT,
                    LLPipeline::RENDER_TYPE_BUMP,
                    LLPipeline::RENDER_TYPE_VOLUME,
                    LLPipeline::RENDER_TYPE_AVATAR,
                    LLPipeline::RENDER_TYPE_CONTROL_AV,
                    LLPipeline::RENDER_TYPE_TREE,
                    LLPipeline::RENDER_TYPE_TERRAIN,
                    LLPipeline::RENDER_TYPE_WATER,
                    LLPipeline::RENDER_TYPE_VOIDWATER,
                    LLPipeline::RENDER_TYPE_PASS_ALPHA,
                    LLPipeline::RENDER_TYPE_PASS_ALPHA_MASK,
                    LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK,
                    LLPipeline::RENDER_TYPE_PASS_GRASS,
                    LLPipeline::RENDER_TYPE_PASS_SIMPLE,
                    LLPipeline::RENDER_TYPE_PASS_BUMP,
                    LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT,
                    LLPipeline::RENDER_TYPE_PASS_SHINY,
                    LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_SHINY,
                    LLPipeline::RENDER_TYPE_PASS_MATERIAL,
                    LLPipeline::RENDER_TYPE_PASS_MATERIAL_ALPHA,
                    LLPipeline::RENDER_TYPE_PASS_MATERIAL_ALPHA_MASK,
                    LLPipeline::RENDER_TYPE_PASS_MATERIAL_ALPHA_EMISSIVE,
                    LLPipeline::RENDER_TYPE_PASS_SPECMAP,
                    LLPipeline::RENDER_TYPE_PASS_SPECMAP_BLEND,
                    LLPipeline::RENDER_TYPE_PASS_SPECMAP_MASK,
                    LLPipeline::RENDER_TYPE_PASS_SPECMAP_EMISSIVE,
                    LLPipeline::RENDER_TYPE_PASS_NORMMAP,
                    LLPipeline::RENDER_TYPE_PASS_NORMMAP_BLEND,
                    LLPipeline::RENDER_TYPE_PASS_NORMMAP_MASK,
                    LLPipeline::RENDER_TYPE_PASS_NORMMAP_EMISSIVE,
                    LLPipeline::RENDER_TYPE_PASS_NORMSPEC,
                    LLPipeline::RENDER_TYPE_PASS_NORMSPEC_BLEND,
                    LLPipeline::RENDER_TYPE_PASS_NORMSPEC_MASK,
                    LLPipeline::RENDER_TYPE_PASS_NORMSPEC_EMISSIVE,
                    LLPipeline::RENDER_TYPE_PASS_ALPHA_MASK_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_SIMPLE_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_BUMP_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_SHINY_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_SHINY_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_MATERIAL_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_MATERIAL_ALPHA_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_MATERIAL_ALPHA_MASK_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_MATERIAL_ALPHA_EMISSIVE_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_SPECMAP_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_SPECMAP_BLEND_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_SPECMAP_MASK_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_SPECMAP_EMISSIVE_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_NORMMAP_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_NORMMAP_BLEND_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_NORMMAP_MASK_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_NORMMAP_EMISSIVE_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_NORMSPEC_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_NORMSPEC_BLEND_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_NORMSPEC_MASK_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_NORMSPEC_EMISSIVE_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_GLTF_PBR,
                    LLPipeline::RENDER_TYPE_PASS_GLTF_PBR_RIGGED,
                    LLPipeline::RENDER_TYPE_PASS_GLTF_PBR_ALPHA_MASK,
                    LLPipeline::RENDER_TYPE_PASS_GLTF_PBR_ALPHA_MASK_RIGGED,
                    END_RENDER_TYPES);

    gGL.setColorMask(false, false);

    // The aux scope runs with GL_SCISSOR_TEST enabled and a box of the capture
    // dimensions (renderAuxiliaryView); a spot map larger than the capture
    // would be partially cleared/rendered. The main view generates shadows
    // with scissor off - match it, then restore the aux scope's state.
    const bool saved_scissor_enabled = (glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE);
    if (saved_scissor_enabled)
    {
        glDisable(GL_SCISSOR_TEST);
    }

    // same [-1,1] -> [0,1] bias matrix generateSunShadow builds for spot slots
    glm::mat4 trans(0.5f, 0.0f, 0.0f, 0.0f,
                    0.0f, 0.5f, 0.0f, 0.0f,
                    0.0f, 0.0f, 0.5f, 0.0f,
                    0.5f, 0.5f, 0.5f, 1.0f);

    // sized MAX_SPOT_SHADOWS: the main-view equivalent was once [2] and slot 3+
    // stomped past the array (the NSpot fix2 hard crash) - do not shrink this
    static LLCullResult aux_result[MAX_SPOT_SHADOWS];

    for (U32 i = 0; i < MAX_SPOT_SHADOWS; i++)
    {
        if (i >= chosen || i >= num_spots)
        { // absent projector: setupSpotLight must resolve s_idx == -1
          // (unshadowed) instead of matching a stale main-view assignment
            mShadowSpotLight[i] = NULL;
            continue;
        }

        if (mSpotShadow[i].getWidth() == 0)
        { // main-view slot not allocated yet this frame (deferred realloc);
          // no resolution to mirror - skip (crash guard, mirrors main loop)
            mShadowSpotLight[i] = NULL;
            continue;
        }

        // Lazy allocation at mSpotShadow's current resolution (tracks the
        // BDMergeProjectorShadowResolution / autoscale reallocation for free).
        if (mPrismSpotShadow[i].getWidth() != mSpotShadow[i].getWidth() ||
            mPrismSpotShadow[i].getHeight() != mSpotShadow[i].getHeight())
        {
            mPrismSpotShadow[i].release();
            if (mPrismSpotShadow[i].allocate(mSpotShadow[i].getWidth(),
                                             mSpotShadow[i].getHeight(), 0, true))
            {
                // shadow-compare sampler state (mirrors allocateShadowBuffer's
                // spot shadow setup; shadowUtil samples these as sampler2DShadow)
                gGL.getTexUnit(0)->bind(&mPrismSpotShadow[i], true);
                gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_ANISOTROPIC);
                gGL.getTexUnit(0)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
                gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
            }
        }
        if (mPrismSpotShadow[i].getWidth() == 0)
        { // allocation failed; leave the slot unshadowed this frame
            mShadowSpotLight[i] = NULL;
            continue;
        }

        set_current_modelview(saved_view);
        set_current_projection(saved_proj);

        LLDrawable* drawable = candidates[i].drawable;
        LLVOVolume* volume = drawable->getVOVolume(); // validated in selection

        // --- projector frustum: exact mirror of generateSunShadow's spot loop ---
        LLVector3 params = volume->getSpotLightParams();
        F32 fov = params.mV[0];

        //get agent->light space matrix (modelview)
        LLVector3 center = drawable->getPositionAgent();
        LLQuaternion quat = volume->getRenderRotation();

        //get near clip plane
        LLVector3 scale = volume->getScale();
        LLVector3 at_axis(0, 0, -scale.mV[2] * 0.5f);
        at_axis *= quat;

        LLVector3 np = center + at_axis;
        at_axis.normVec();

        // degenerate-projector guards: a bad fov/scale/transform must skip the
        // slot rather than feed NaN into the matrices or renderShadow
        const F32 tan_half_fov = tanf(fov * 0.5f);
        if (!std::isfinite(fov) || fov <= 0.f ||
            !std::isfinite(tan_half_fov) ||
            fabsf(tan_half_fov) < F_APPROXIMATELY_ZERO ||
            scale.mV[VX] < F_APPROXIMATELY_ZERO ||
            scale.mV[VY] < F_APPROXIMATELY_ZERO ||
            scale.mV[VZ] < F_APPROXIMATELY_ZERO ||
            !center.isFinite() || !quat.isFinite())
        {
            mShadowSpotLight[i] = NULL;
            continue;
        }

        //get origin that has given fov for plane np, at_axis, and given scale
        F32 dist = (scale.mV[1] * 0.5f) / tan_half_fov;

        LLVector3 origin = np - at_axis * dist;

        //get perspective matrix
        F32 near_clip = dist + 0.01f;
        F32 width = scale.mV[VX];
        F32 height = scale.mV[VY];
        F32 far_clip = dist + volume->getLightRadius() * 1.5f;

        F32 fovy = fov; // radians
        F32 aspect = width / height;

        if (!std::isfinite(dist) || !origin.isFinite() ||
            !std::isfinite(far_clip) || far_clip <= near_clip ||
            !std::isfinite(aspect) || aspect <= 0.f)
        {
            mShadowSpotLight[i] = NULL;
            continue;
        }

        LLMatrix4 mat(quat, LLVector4(origin, 1.f));

        glm::mat4 view = glm::make_mat4((F32*)mat.mMatrix);

        view = glm::inverse(view);

        glm::mat4 proj = glm::perspective(fovy, aspect, near_clip, far_clip);

        mShadowSpotLight[i] = drawable;
        mSpotLightFade[i] = 1.f; // fully shadowed, no fade-in (transient; restored)

        set_current_modelview(view);
        set_current_projection(proj);

        // the ONLY camera-dependent term: the AUX camera's inverse view
        mSunShadowMatrix[i + 4] = trans * proj * view * inv_view_aux;

        set_last_modelview(mShadowModelview[i + 4]);
        set_last_projection(mShadowProjection[i + 4]);

        mShadowModelview[i + 4] = view;
        mShadowProjection[i + 4] = proj;

        LLCamera shadow_cam = camera;
        // the surface-lens keep-plane (setUserClipPlane) is aux-eye state; it
        // must not cull the projector's shadow casters
        shadow_cam.disableUserClipPlane();
        shadow_cam.setFar(far_clip);
        shadow_cam.setOrigin(origin);

        LLViewerCamera::updateFrustumPlanes(shadow_cam, false, false, true);

        mPrismSpotShadow[i].bindTarget();
        mPrismSpotShadow[i].getViewport(gGLViewport);
        mPrismSpotShadow[i].clear();

        LLViewerCamera::sCurCameraID = (LLViewerCamera::eCameraID)(LLViewerCamera::CAMERA_SPOT_SHADOW0 + i);

        RenderSpotLight = drawable;

        renderShadow(view, proj, shadow_cam, aux_result[i], false);

        RenderSpotLight = nullptr;

        mPrismSpotShadow[i].flush();
    }

    gGL.setColorMask(true, true);

    if (saved_scissor_enabled)
    { // restore the aux scope's scissor state (box was never modified)
        glEnable(GL_SCISSOR_TEST);
    }

    popRenderTypeMask();

    // Re-install the aux camera state for the aux updateCull/stateSort that
    // run immediately after this returns.
    set_current_modelview(saved_view);
    set_current_projection(saved_proj);
    set_last_modelview(last_modelview);
    set_last_projection(last_projection);
    gGL.matrixMode(LLRender::MM_PROJECTION);
    gGL.loadMatrix(glm::value_ptr(saved_proj));
    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGL.loadMatrix(glm::value_ptr(saved_view));
    for (U32 v = 0; v < 4; ++v)
    {
        gGLViewport[v] = saved_viewport[v];
    }
    glViewport(gGLViewport[0], gGLViewport[1], gGLViewport[2], gGLViewport[3]);
    LLRenderTarget::sCurResX = saved_cur_res_x;
    LLRenderTarget::sCurResY = saved_cur_res_y;
    LLViewerCamera::sCurCameraID = saved_camera_id;
}

void LLPipeline::renderGroups(LLRenderPass* pass, U32 type, bool texture)
{
    for (LLCullResult::sg_iterator i = sCull->beginVisibleGroups(); i != sCull->endVisibleGroups(); ++i)
    {
        LLSpatialGroup* group = *i;
        if (!group->isDead() &&
            (!sUseOcclusion || !group->isOcclusionState(LLSpatialGroup::OCCLUDED)) &&
            gPipeline.hasRenderType(group->getSpatialPartition()->mDrawableType) &&
            group->mDrawMap.find(type) != group->mDrawMap.end())
        {
            pass->renderGroup(group,type,texture);
        }
    }
}

void LLPipeline::renderRiggedGroups(LLRenderPass* pass, U32 type, bool texture)
{
    for (LLCullResult::sg_iterator i = sCull->beginVisibleGroups(); i != sCull->endVisibleGroups(); ++i)
    {
        LLSpatialGroup* group = *i;
        if (!group->isDead() &&
            (!sUseOcclusion || !group->isOcclusionState(LLSpatialGroup::OCCLUDED)) &&
            gPipeline.hasRenderType(group->getSpatialPartition()->mDrawableType) &&
            group->mDrawMap.find(type) != group->mDrawMap.end())
        {
            pass->renderRiggedGroup(group, type, texture);
        }
    }
}

void LLPipeline::profileAvatar(LLVOAvatar* avatar, bool profile_attachments)
{
    if (gGLManager.mGLVersion < 3.25f)
    { // profiling requires GL 3.3 or later
        return;
    }

    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;

    // don't continue to profile an avatar that is known to be too slow
    llassert(!avatar->isTooSlow());

    LLGLSLShader* cur_shader = LLGLSLShader::sCurBoundShaderPtr;

    mRT->deferredScreen.bindTarget();
    mRT->deferredScreen.clear();

    if (!profile_attachments)
    {
        // profile entire avatar all at once and readback asynchronously
        avatar->placeProfileQuery();

        LLTimer cpu_timer;

        generateImpostor(avatar, false, true);

        avatar->mCPURenderTime = (F32)cpu_timer.getElapsedTimeF32() * 1000.f;

        avatar->readProfileQuery(5); // allow up to 5 frames of latency
    }
    else
    {
        // profile attachments one at a time
        LLVOAvatar::attachment_map_t::iterator iter;
        LLVOAvatar::attachment_map_t::iterator begin = avatar->mAttachmentPoints.begin();
        LLVOAvatar::attachment_map_t::iterator end = avatar->mAttachmentPoints.end();

        for (iter = begin;
            iter != end;
            ++iter)
        {
            LLViewerJointAttachment* attachment = iter->second;
            for (LLViewerJointAttachment::attachedobjs_vec_t::iterator attachment_iter = attachment->mAttachedObjects.begin();
                attachment_iter != attachment->mAttachedObjects.end();
                ++attachment_iter)
            {
                LLViewerObject* attached_object = attachment_iter->get();
                if (attached_object)
                {
                    // use gDebugProgram to do the GPU queries
                    gDebugProgram.clearStats();
                    gDebugProgram.placeProfileQuery(true);

                    generateImpostor(avatar, false, true, attached_object);
                    gDebugProgram.readProfileQuery(true, true);

                    attached_object->mGPURenderTime = gDebugProgram.mTimeElapsed / 1000000.f;
                }
            }
        }
    }

    mRT->deferredScreen.flush();

    if (cur_shader)
    {
        cur_shader->bind();
    }
}

void LLPipeline::generateImpostor(LLVOAvatar* avatar, bool preview_avatar, bool for_profile, LLViewerObject* specific_attachment)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_PIPELINE;
    LL_PROFILE_GPU_ZONE("generateImpostor");
    LLGLState::checkStates();

    static LLCullResult result;
    result.clear();
    grabReferences(result);

    if (!avatar || avatar->isDead() || !avatar->mDrawable)
    {
        LL_WARNS_ONCE("AvatarRenderPipeline") << "Avatar is " << (avatar ? "not drawable" : "null") << LL_ENDL;
        return;
    }
    LL_DEBUGS_ONCE("AvatarRenderPipeline") << "Avatar " << avatar->getID() << " is drawable" << LL_ENDL;

    assertInitialized();

    // previews can't be muted or impostered
    bool visually_muted = !for_profile && !preview_avatar && avatar->isVisuallyMuted();
    LL_DEBUGS_ONCE("AvatarRenderPipeline") << "Avatar " << avatar->getID()
                              << " is " << ( visually_muted ? "" : "not ") << "visually muted"
                              << LL_ENDL;
    bool too_complex = !for_profile && !preview_avatar && avatar->isTooComplex();
    LL_DEBUGS_ONCE("AvatarRenderPipeline") << "Avatar " << avatar->getID()
                              << " is " << ( too_complex ? "" : "not ") << "too complex"
                              << LL_ENDL;

    pushRenderTypeMask();

    if (visually_muted || too_complex)
    {
        // only show jelly doll geometry
        andRenderTypeMask(LLPipeline::RENDER_TYPE_AVATAR,
                            LLPipeline::RENDER_TYPE_CONTROL_AV,
                            END_RENDER_TYPES);
    }
    else
    {
        //hide world geometry
        clearRenderTypeMask(
            RENDER_TYPE_SKY,
            RENDER_TYPE_WL_SKY,
            RENDER_TYPE_TERRAIN,
            RENDER_TYPE_GRASS,
            RENDER_TYPE_CONTROL_AV, // Animesh
            RENDER_TYPE_TREE,
            RENDER_TYPE_VOIDWATER,
            RENDER_TYPE_WATER,
            RENDER_TYPE_ALPHA_PRE_WATER,
            RENDER_TYPE_PASS_GRASS,
            RENDER_TYPE_HUD,
            RENDER_TYPE_PARTICLES,
            RENDER_TYPE_CLOUDS,
            RENDER_TYPE_HUD_PARTICLES,
            END_RENDER_TYPES
         );
    }

    if (specific_attachment && specific_attachment->isHUDAttachment())
    { //enable HUD rendering
        setRenderTypeMask(RENDER_TYPE_HUD, END_RENDER_TYPES);
    }

    S32 occlusion = sUseOcclusion;
    sUseOcclusion = 0;

    sReflectionRender = ! sRenderDeferred;

    sShadowRender = true;
    sImpostorRender = true;

    LLViewerCamera* viewer_camera = LLViewerCamera::getInstance();

    {
        markVisible(avatar->mDrawable, *viewer_camera);

        if (preview_avatar)
        {
            // Only show rigged attachments for preview
            // For the sake of performance and so that static
            // objects won't obstruct previewing changes
            LLVOAvatar::attachment_map_t::iterator iter;
            for (iter = avatar->mAttachmentPoints.begin();
                iter != avatar->mAttachmentPoints.end();
                ++iter)
            {
                LLViewerJointAttachment *attachment = iter->second;
                for (LLViewerJointAttachment::attachedobjs_vec_t::iterator attachment_iter = attachment->mAttachedObjects.begin();
                    attachment_iter != attachment->mAttachedObjects.end();
                    ++attachment_iter)
                {
                    LLViewerObject* attached_object = attachment_iter->get();
                    if (attached_object)
                    {
                        if (attached_object->isRiggedMesh())
                        {
                            markVisible(attached_object->mDrawable->getSpatialBridge(), *viewer_camera);
                        }
                        else
                        {
                            // sometimes object is a linkset and rigged mesh is a child
                            LLViewerObject::const_child_list_t& child_list = attached_object->getChildren();
                            for (LLViewerObject::child_list_t::const_iterator iter = child_list.begin();
                                iter != child_list.end(); iter++)
                            {
                                LLViewerObject* child = *iter;
                                if (child->isRiggedMesh())
                                {
                                    markVisible(attached_object->mDrawable->getSpatialBridge(), *viewer_camera);
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
        else
        {
            if (specific_attachment)
            {
                markVisible(specific_attachment->mDrawable->getSpatialBridge(), *viewer_camera);
            }
            else
            {
                LLVOAvatar::attachment_map_t::iterator iter;
                LLVOAvatar::attachment_map_t::iterator begin = avatar->mAttachmentPoints.begin();
                LLVOAvatar::attachment_map_t::iterator end = avatar->mAttachmentPoints.end();

                for (iter = begin;
                    iter != end;
                    ++iter)
                {
                    LLViewerJointAttachment* attachment = iter->second;
                    for (LLViewerJointAttachment::attachedobjs_vec_t::iterator attachment_iter = attachment->mAttachedObjects.begin();
                        attachment_iter != attachment->mAttachedObjects.end();
                        ++attachment_iter)
                    {
                        LLViewerObject* attached_object = attachment_iter->get();
                        if (attached_object)
                        {
                            markVisible(attached_object->mDrawable->getSpatialBridge(), *viewer_camera);
                        }
                    }
                }
            }
        }
    }

    stateSort(*LLViewerCamera::getInstance(), result);

    LLCamera camera = *viewer_camera;
    LLVector2 tdim;
    U32 resY = 0;
    U32 resX = 0;

    if (!preview_avatar)
    {
        const LLVector4a* ext = avatar->mDrawable->getSpatialExtents();
        LLVector3 pos(avatar->getRenderPosition()+avatar->getImpostorOffset());

        camera.lookAt(viewer_camera->getOrigin(), pos, viewer_camera->getUpAxis());

        LLVector4a half_height;
        half_height.setSub(ext[1], ext[0]);
        half_height.mul(0.5f);

        LLVector4a left;
        left.load3(camera.getLeftAxis().mV);
        left.mul(left);
        llassert(left.dot3(left).getF32() > F_APPROXIMATELY_ZERO);
        left.normalize3fast();

        LLVector4a up;
        up.load3(camera.getUpAxis().mV);
        up.mul(up);
        llassert(up.dot3(up).getF32() > F_APPROXIMATELY_ZERO);
        up.normalize3fast();

        tdim.mV[0] = fabsf(half_height.dot3(left).getF32());
        tdim.mV[1] = fabsf(half_height.dot3(up).getF32());

        gGL.matrixMode(LLRender::MM_PROJECTION);
        gGL.pushMatrix();

        F32 distance = (pos-camera.getOrigin()).length();
        F32 fov = atanf(tdim.mV[1]/distance)*2.f*RAD_TO_DEG;
        F32 aspect = tdim.mV[0]/tdim.mV[1];
        glm::mat4 persp = glm::perspective(glm::radians(fov), aspect, 1.f, 256.f);
        set_current_projection(persp);
        gGL.loadMatrix(glm::value_ptr(persp));

        gGL.matrixMode(LLRender::MM_MODELVIEW);
        gGL.pushMatrix();

        F32 ogl_mat[16];
        camera.getOpenGLTransform(ogl_mat);
        glm::mat4 mat = glm::make_mat4((GLfloat*) OGL_TO_CFR_ROTATION) * glm::make_mat4(ogl_mat);

        gGL.loadMatrix(glm::value_ptr(mat));
        set_current_modelview(mat);

        glClearColor(0.0f,0.0f,0.0f,0.0f);
        gGL.setColorMask(true, true);

        // get the number of pixels per angle
        F32 pa = gViewerWindow->getWindowHeightRaw() / (RAD_TO_DEG * viewer_camera->getView());

        //get resolution based on angle width and height of impostor (double desired resolution to prevent aliasing)
        resY = llmin(nhpo2((U32) (fov*pa)), (U32) 512);
        resX = llmin(nhpo2((U32) (atanf(tdim.mV[0]/distance)*2.f*RAD_TO_DEG*pa)), (U32) 512);

        if (!for_profile)
        {
            if (!avatar->mImpostor.isComplete())
            {
                avatar->mImpostor.allocate(resX, resY, GL_RGBA, true);

                if (LLPipeline::sRenderDeferred)
                {
                    addDeferredAttachments(avatar->mImpostor, true);
                }

                gGL.getTexUnit(0)->bind(&avatar->mImpostor);
                gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
                gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
            }
            else if (resX != avatar->mImpostor.getWidth() || resY != avatar->mImpostor.getHeight())
            {
                avatar->mImpostor.resize(resX, resY);
            }

            avatar->mImpostor.bindTarget();
        }
    }

    F32 old_alpha = LLDrawPoolAvatar::sMinimumAlpha;

    if (visually_muted || too_complex)
    { //disable alpha masking for muted avatars (get whole skin silhouette)
        LLDrawPoolAvatar::sMinimumAlpha = 0.f;
    }

    if (preview_avatar || for_profile)
    {
        // previews and profiles don't care about imposters
        renderGeomDeferred(camera);
        renderGeomPostDeferred(camera);
    }
    else
    {
        avatar->mImpostor.clear();
        renderGeomDeferred(camera);

        renderGeomPostDeferred(camera);

        // Shameless hack time: render it all again,
        // this time writing the depth
        // values we need to generate the alpha mask below
        // while preserving the alpha-sorted color rendering
        // from the previous pass
        //
        sImpostorRenderAlphaDepthPass = true;
        // depth-only here...
        //
        gGL.setColorMask(false,false);
        renderGeomPostDeferred(camera);

        sImpostorRenderAlphaDepthPass = false;

    }

    LLDrawPoolAvatar::sMinimumAlpha = old_alpha;

    if (!for_profile)
    { //create alpha mask based on depth buffer (grey out if muted)
        if (LLPipeline::sRenderDeferred)
        {
            GLuint buff = GL_COLOR_ATTACHMENT0;
            glDrawBuffers(1, &buff);
        }

        LLGLDisable blend(GL_BLEND);

        if (visually_muted || too_complex)
        {
            gGL.setColorMask(true, true);
        }
        else
        {
            gGL.setColorMask(false, true);
        }

        gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

        LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_GREATER);

        gGL.flush();

        gGL.pushMatrix();
        gGL.loadIdentity();
        gGL.matrixMode(LLRender::MM_PROJECTION);
        gGL.pushMatrix();
        gGL.loadIdentity();

        static const F32 clip_plane = 0.99999f;

        gDebugProgram.bind();

        if (visually_muted)
        {   // Visually muted avatar
            LLColor4 muted_color(avatar->getMutedAVColor());
            LL_DEBUGS_ONCE("AvatarRenderPipeline") << "Avatar " << avatar->getID() << " MUTED set solid color " << muted_color << LL_ENDL;
            gGL.diffuseColor4fv( muted_color.mV );
        }
        else if (!preview_avatar)
        { //grey muted avatar
            LL_DEBUGS_ONCE("AvatarRenderPipeline") << "Avatar " << avatar->getID() << " MUTED set grey" << LL_ENDL;
            gGL.diffuseColor4fv(LLColor4::pink.mV );
        }

        gGL.begin(LLRender::TRIANGLES);
        {
            gGL.vertex3f(-1.f, -1.f, clip_plane);
            gGL.vertex3f(1.f, -1.f, clip_plane);
            gGL.vertex3f(1.f, 1.f, clip_plane);

            gGL.vertex3f(-1.f, -1.f, clip_plane);
            gGL.vertex3f(1.f, 1.f, clip_plane);
            gGL.vertex3f(-1.f, 1.f, clip_plane);
        }
        gGL.end();
        gGL.flush();

        gDebugProgram.unbind();

        gGL.popMatrix();
        gGL.matrixMode(LLRender::MM_MODELVIEW);
        gGL.popMatrix();
    }

    if (!preview_avatar && !for_profile)
    {
        avatar->mImpostor.flush();
        avatar->setImpostorDim(tdim);
    }

    sUseOcclusion = occlusion;
    sReflectionRender = false;
    sImpostorRender = false;
    sShadowRender = false;
    popRenderTypeMask();

    if (!preview_avatar)
    {
        gGL.matrixMode(LLRender::MM_PROJECTION);
        gGL.popMatrix();
        gGL.matrixMode(LLRender::MM_MODELVIEW);
        gGL.popMatrix();
    }

    if (!preview_avatar && !for_profile)
    {
        avatar->mNeedsImpostorUpdate = false;
        avatar->cacheImpostorValues();
        avatar->mLastImpostorUpdateFrameTime = gFrameTimeSeconds;
    }

    LLVertexBuffer::unbind();
    LLGLState::checkStates();
}

bool LLPipeline::hasRenderBatches(const U32 type) const
{
    return sCull->getRenderMapSize(type) > 0;
}

LLCullResult::drawinfo_iterator LLPipeline::beginRenderMap(U32 type)
{
    return sCull->beginRenderMap(type);
}

LLCullResult::drawinfo_iterator LLPipeline::endRenderMap(U32 type)
{
    return sCull->endRenderMap(type);
}

LLCullResult::sg_iterator LLPipeline::beginAlphaGroups()
{
    return sCull->beginAlphaGroups();
}

LLCullResult::sg_iterator LLPipeline::endAlphaGroups()
{
    return sCull->endAlphaGroups();
}

LLCullResult::sg_iterator LLPipeline::beginRiggedAlphaGroups()
{
    return sCull->beginRiggedAlphaGroups();
}

LLCullResult::sg_iterator LLPipeline::endRiggedAlphaGroups()
{
    return sCull->endRiggedAlphaGroups();
}

bool LLPipeline::hasRenderType(const U32 type) const
{
    // STORM-365 : LLViewerJointAttachment::setAttachmentVisibility() is setting type to 0 to actually mean "do not render"
    // We then need to test that value here and return false to prevent attachment to render (in mouselook for instance)
    // TODO: reintroduce RENDER_TYPE_NONE in LLRenderTypeMask and initialize its mRenderTypeEnabled[RENDER_TYPE_NONE] to false explicitely
    return (type == 0 ? false : mRenderTypeEnabled[type]);
}

void LLPipeline::setRenderTypeMask(U32 type, ...)
{
    va_list args;

    va_start(args, type);
    while (type < END_RENDER_TYPES)
    {
        mRenderTypeEnabled[type] = true;
        type = va_arg(args, U32);
    }
    va_end(args);

    if (type > END_RENDER_TYPES)
    {
        LL_ERRS() << "Invalid render type." << LL_ENDL;
    }
}

bool LLPipeline::hasAnyRenderType(U32 type, ...) const
{
    va_list args;

    va_start(args, type);
    while (type < END_RENDER_TYPES)
    {
        if (mRenderTypeEnabled[type])
        {
            va_end(args);
            return true;
        }
        type = va_arg(args, U32);
    }
    va_end(args);

    if (type > END_RENDER_TYPES)
    {
        LL_ERRS() << "Invalid render type." << LL_ENDL;
    }

    return false;
}

void LLPipeline::pushRenderTypeMask()
{
    std::string cur_mask;
    cur_mask.assign((const char*) mRenderTypeEnabled, sizeof(mRenderTypeEnabled));
    mRenderTypeEnableStack.push(cur_mask);
}

void LLPipeline::popRenderTypeMask()
{
    if (mRenderTypeEnableStack.empty())
    {
        LL_ERRS() << "Depleted render type stack." << LL_ENDL;
    }

    memcpy(mRenderTypeEnabled, mRenderTypeEnableStack.top().data(), sizeof(mRenderTypeEnabled));
    mRenderTypeEnableStack.pop();
}

void LLPipeline::andRenderTypeMask(U32 type, ...)
{
    va_list args;

    bool tmp[NUM_RENDER_TYPES];
    for (U32 i = 0; i < NUM_RENDER_TYPES; ++i)
    {
        tmp[i] = false;
    }

    va_start(args, type);
    while (type < END_RENDER_TYPES)
    {
        if (mRenderTypeEnabled[type])
        {
            tmp[type] = true;
        }

        type = va_arg(args, U32);
    }
    va_end(args);

    if (type > END_RENDER_TYPES)
    {
        LL_ERRS() << "Invalid render type." << LL_ENDL;
    }

    for (U32 i = 0; i < LLPipeline::NUM_RENDER_TYPES; ++i)
    {
        mRenderTypeEnabled[i] = tmp[i];
    }

}

void LLPipeline::clearRenderTypeMask(U32 type, ...)
{
    va_list args;

    va_start(args, type);
    while (type < END_RENDER_TYPES)
    {
        mRenderTypeEnabled[type] = false;

        type = va_arg(args, U32);
    }
    va_end(args);

    if (type > END_RENDER_TYPES)
    {
        LL_ERRS() << "Invalid render type." << LL_ENDL;
    }
}

void LLPipeline::setAllRenderTypes()
{
    for (U32 i = 0; i < NUM_RENDER_TYPES; ++i)
    {
        mRenderTypeEnabled[i] = true;
    }
}

void LLPipeline::clearAllRenderTypes()
{
    for (U32 i = 0; i < NUM_RENDER_TYPES; ++i)
    {
        mRenderTypeEnabled[i] = false;
    }
}

void LLPipeline::addDebugBlip(const LLVector3& position, const LLColor4& color)
{
    DebugBlip blip(position, color);
    mDebugBlips.push_back(blip);
}

void LLPipeline::hidePermanentObjects( std::vector<U32>& restoreList )
{
    //This method is used to hide any vo's from the object list that may have
    //the permanent flag set.

    U32 objCnt = gObjectList.getNumObjects();
    for (U32 i = 0; i < objCnt; ++i)
    {
        LLViewerObject* pObject = gObjectList.getObject(i);
        if ( pObject && pObject->flagObjectPermanent() )
        {
            LLDrawable *pDrawable = pObject->mDrawable;

            if ( pDrawable )
            {
                restoreList.push_back( i );
                hideDrawable( pDrawable );
            }
        }
    }

    skipRenderingOfTerrain( true );
}

void LLPipeline::restorePermanentObjects( const std::vector<U32>& restoreList )
{
    //This method is used to restore(unhide) any vo's from the object list that may have
    //been hidden because their permanency flag was set.

    std::vector<U32>::const_iterator itCurrent  = restoreList.begin();
    std::vector<U32>::const_iterator itEnd      = restoreList.end();

    U32 objCnt = gObjectList.getNumObjects();

    while ( itCurrent != itEnd )
    {
        U32 index = *itCurrent;
        LLViewerObject* pObject = NULL;
        if ( index < objCnt )
        {
            pObject = gObjectList.getObject( index );
        }
        if ( pObject )
        {
            LLDrawable *pDrawable = pObject->mDrawable;
            if ( pDrawable )
            {
                pDrawable->clearState( LLDrawable::FORCE_INVISIBLE );
                unhideDrawable( pDrawable );
            }
        }
        ++itCurrent;
    }

    skipRenderingOfTerrain( false );
}

void LLPipeline::skipRenderingOfTerrain( bool flag )
{
    pool_set_t::iterator iter = mPools.begin();
    while ( iter != mPools.end() )
    {
        LLDrawPool* pPool = *iter;
        U32 poolType = pPool->getType();
        if ( hasRenderType( pPool->getType() ) && poolType == LLDrawPool::POOL_TERRAIN )
        {
            pPool->setSkipRenderFlag( flag );
        }
        ++iter;
    }
}

void LLPipeline::hideObject( const LLUUID& id )
{
    LLViewerObject *pVO = gObjectList.findObject( id );

    if ( pVO )
    {
        LLDrawable *pDrawable = pVO->mDrawable;

        if ( pDrawable )
        {
            hideDrawable( pDrawable );
        }
    }
}

void LLPipeline::hideDrawable( LLDrawable *pDrawable )
{
    pDrawable->setState( LLDrawable::FORCE_INVISIBLE );
    markRebuild( pDrawable, LLDrawable::REBUILD_ALL);
    //hide the children
    LLViewerObject::const_child_list_t& child_list = pDrawable->getVObj()->getChildren();
    for ( LLViewerObject::child_list_t::const_iterator iter = child_list.begin();
          iter != child_list.end(); iter++ )
    {
        LLViewerObject* child = *iter;
        LLDrawable* drawable = child->mDrawable;
        if ( drawable )
        {
            drawable->setState( LLDrawable::FORCE_INVISIBLE );
            markRebuild( drawable, LLDrawable::REBUILD_ALL);
        }
    }
}
void LLPipeline::unhideDrawable( LLDrawable *pDrawable )
{
    pDrawable->clearState( LLDrawable::FORCE_INVISIBLE );
    markRebuild( pDrawable, LLDrawable::REBUILD_ALL);
    //restore children
    LLViewerObject::const_child_list_t& child_list = pDrawable->getVObj()->getChildren();
    for ( LLViewerObject::child_list_t::const_iterator iter = child_list.begin();
          iter != child_list.end(); iter++)
    {
        LLViewerObject* child = *iter;
        LLDrawable* drawable = child->mDrawable;
        if ( drawable )
        {
            drawable->clearState( LLDrawable::FORCE_INVISIBLE );
            markRebuild( drawable, LLDrawable::REBUILD_ALL);
        }
    }
}
void LLPipeline::restoreHiddenObject( const LLUUID& id )
{
    LLViewerObject *pVO = gObjectList.findObject( id );

    if ( pVO )
    {
        LLDrawable *pDrawable = pVO->mDrawable;
        if ( pDrawable )
        {
            unhideDrawable( pDrawable );
        }
    }
}

void LLPipeline::skipRenderingShadows()
{
    LLGLDepthTest depth(GL_TRUE);

    for (S32 j = 0; j < 4; j++)
    {
        mRT->shadow[j].bindTarget();
        mRT->shadow[j].clear();
        mRT->shadow[j].flush();
    }
}

void LLPipeline::handleShadowDetailChanged()
{
    if (RenderShadowDetail > gSavedSettings.getS32("RenderShadowDetail"))
    {
        skipRenderingShadows();
    }
    else
    {
        LLViewerShaderMgr::instance()->setShaders();
    }
}

class LLOctreeDirty : public OctreeTraveler
{
public:
    virtual void visit(const OctreeNode* state)
    {
        LLSpatialGroup* group = (LLSpatialGroup*)state->getListener(0);

        if (group->getSpatialPartition()->mRenderByGroup)
        {
            group->setState(LLSpatialGroup::GEOM_DIRTY);
            gPipeline.markRebuild(group);
        }

        for (LLSpatialGroup::bridge_list_t::iterator i = group->mBridgeList.begin(); i != group->mBridgeList.end(); ++i)
        {
            LLSpatialBridge* bridge = *i;
            traverse(bridge->mOctree);
        }
    }
};

// Called from LLViewHighlightTransparent when "Highlight Transparent" is toggled
void LLPipeline::rebuildDrawInfo()
{
    const U32 types_to_traverse[] =
    {
        LLViewerRegion::PARTITION_VOLUME,
        LLViewerRegion::PARTITION_BRIDGE,
        LLViewerRegion::PARTITION_AVATAR
    };

    LLOctreeDirty dirty;
    for (LLViewerRegion* region : LLWorld::getInstance()->getRegionList())
    {
        for (U32 type : types_to_traverse)
        {
            LLSpatialPartition* part = region->getSpatialPartition(type);
            dirty.traverse(part->mOctree);
        }
    }
}

void LLPipeline::rebuildTerrain()
{
    for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin();
        iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
    {
        LLViewerRegion* region = *iter;
        region->dirtyAllPatches();
    }
}
