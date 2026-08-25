/**
 * @file alprojectorshaftpresets.cpp
 * @brief Shared quick presets for projector volumetrics.
 */

#include "llviewerprecompiledheaders.h"

#include "alprojectorshaftpresets.h"

#include "llcontrol.h"
#include "llviewercontrol.h"

namespace
{
const char* const SHAFT_SETTINGS[] = {
    "BDMergeProjectorVolumetrics",
    "BDMergeProjectorVolumetricsMultiplier",
    "BDMergeProjectorVolumetricsResolution",
    "BDMergeProjectorVolumetricsAnisotropy",
    "BDMergeProjectorVolumetricsHalfRes",
    "BDMergeProjectorVolumetricsTemporal",
    "BDMergeProjectorVolumetricsTemporalBlend",
    "BDMergeProjectorVolumetricsTemporalReject",
    "BDMergeProjectorVolumetricsTemporalBeamDepth",
    "BDMergeProjectorVolumetricsScissor",
    "BDMergeProjectorVolumetricsAdaptive",
    "BDMergeProjectorVolumetricsMinResolution",
    "BDMergeProjectorVolumetricsMaxLuminance",
    "BDMergeProjectorVolumetricsDither",
    "BDMergeProjectorVolumetricsTemporalDither",
    "BDMergeProjectorVolumetricsShadowSamples",
    "BDMergeProjectorVolumetricsFeather",
    "BDMergeProjectorVolumetricsShadowTint",
    "BDMergeProjectorVolumetricsTint",
    "BDMergeProjectorVolumetricsTintStrength",
    "BDMergeProjectorVolumetricsDensity",
    "BDMergeProjectorVolumetricsNoiseStrength",
    "BDMergeProjectorVolumetricsNoiseScale",
    "BDMergeProjectorVolumetricsNoiseSpeed",
    "BDMergeFroxelWindAzimuth",
    "BDMergeFroxelWindElevation",
    "BDMergeFroxelWindInverted",
    "BDMergeProjectorVolumetricsFogStrength",
    "BDMergeProjectorVolumetricsFogGroundDensity",
    "BDMergeProjectorVolumetricsFogFalloff",
    "BDMergeProjectorVolumetricsFogBase",
    "BDMergeProjectorVolumetricsBloomFeed",
    "BDMergeSoftProjectorShadows",
    "BDMergeSoftShadowSoftness",
    "BDMergeSoftShadowMaxPenumbra",
    "BDMergeSoftShadowFill",
    "BDMergeSoftShadowSun",
    "BDMergeGoboAnisotropic",
};

void resetToKnownBase()
{
    for (const char* name : SHAFT_SETTINGS)
    {
        if (LLControlVariable* control = gSavedSettings.getControl(name))
        {
            control->resetToDefault(true);
        }
    }
    // Selecting a preset is an explicit request to use the effect.
    gSavedSettings.setBOOL("BDMergeProjectorVolumetrics", true);
}
}

namespace ALProjectorShaftPresets
{
void apply(S32 preset)
{
    if (preset < FAST_PREVIEW || preset > HAZY_STAGE)
    {
        return;
    }

    resetToKnownBase();
    switch (preset)
    {
    case FAST_PREVIEW:
        gSavedSettings.setF32("BDMergeProjectorVolumetricsMultiplier", 0.85f);
        gSavedSettings.setU32("BDMergeProjectorVolumetricsResolution", 12u);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsAnisotropy", 0.55f);
        gSavedSettings.setBOOL("BDMergeProjectorVolumetricsHalfRes", true);
        gSavedSettings.setBOOL("BDMergeProjectorVolumetricsTemporal", false);
        gSavedSettings.setBOOL("BDMergeProjectorVolumetricsAdaptive", true);
        gSavedSettings.setU32("BDMergeProjectorVolumetricsMinResolution", 4u);
        gSavedSettings.setU32("BDMergeProjectorVolumetricsShadowSamples", 1u);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsFeather", 0.10f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsDensity", 0.65f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsNoiseStrength", 0.05f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsBloomFeed", 0.f);
        gSavedSettings.setBOOL("BDMergeSoftProjectorShadows", false);
        break;

    case BALANCED:
        // The shipped defaults are the balanced recording-safe configuration.
        break;

    case CINEMATIC:
        gSavedSettings.setF32("BDMergeProjectorVolumetricsMultiplier", 1.15f);
        gSavedSettings.setU32("BDMergeProjectorVolumetricsResolution", 32u);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsAnisotropy", 0.72f);
        gSavedSettings.setBOOL("BDMergeProjectorVolumetricsHalfRes", true);
        gSavedSettings.setBOOL("BDMergeProjectorVolumetricsTemporal", true);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsTemporalBlend", 0.86f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsTemporalReject", 3.f);
        gSavedSettings.setBOOL("BDMergeProjectorVolumetricsTemporalBeamDepth", true);
        gSavedSettings.setBOOL("BDMergeProjectorVolumetricsAdaptive", true);
        gSavedSettings.setU32("BDMergeProjectorVolumetricsMinResolution", 8u);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsMaxLuminance", 24.f);
        gSavedSettings.setU32("BDMergeProjectorVolumetricsShadowSamples", 2u);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsFeather", 0.14f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsDensity", 1.20f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsNoiseStrength", 0.12f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsNoiseScale", 1.50f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsNoiseSpeed", 0.15f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsBloomFeed", 0.25f);
        gSavedSettings.setBOOL("BDMergeSoftProjectorShadows", true);
        gSavedSettings.setF32("BDMergeSoftShadowSoftness", 3.5f);
        gSavedSettings.setF32("BDMergeSoftShadowMaxPenumbra", 7.f);
        gSavedSettings.setF32("BDMergeSoftShadowFill", 0.04f);
        gSavedSettings.setBOOL("BDMergeGoboAnisotropic", true);
        break;

    case HAZY_STAGE:
        gSavedSettings.setF32("BDMergeProjectorVolumetricsMultiplier", 1.25f);
        gSavedSettings.setU32("BDMergeProjectorVolumetricsResolution", 24u);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsAnisotropy", 0.82f);
        gSavedSettings.setBOOL("BDMergeProjectorVolumetricsHalfRes", true);
        gSavedSettings.setBOOL("BDMergeProjectorVolumetricsTemporal", true);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsTemporalBlend", 0.84f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsTemporalReject", 3.5f);
        gSavedSettings.setBOOL("BDMergeProjectorVolumetricsTemporalBeamDepth", true);
        gSavedSettings.setBOOL("BDMergeProjectorVolumetricsAdaptive", true);
        gSavedSettings.setU32("BDMergeProjectorVolumetricsMinResolution", 6u);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsMaxLuminance", 20.f);
        gSavedSettings.setU32("BDMergeProjectorVolumetricsShadowSamples", 2u);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsFeather", 0.20f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsDensity", 2.20f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsNoiseStrength", 0.30f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsNoiseScale", 0.60f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsNoiseSpeed", 0.25f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsFogStrength", 0.45f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsFogGroundDensity", 1.50f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsFogFalloff", 10.f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsFogBase", 0.f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsBloomFeed", 0.40f);
        gSavedSettings.setBOOL("BDMergeSoftProjectorShadows", true);
        gSavedSettings.setF32("BDMergeSoftShadowSoftness", 4.f);
        gSavedSettings.setF32("BDMergeSoftShadowMaxPenumbra", 8.f);
        gSavedSettings.setF32("BDMergeSoftShadowFill", 0.06f);
        gSavedSettings.setBOOL("BDMergeGoboAnisotropic", true);
        break;
    }
}
}
