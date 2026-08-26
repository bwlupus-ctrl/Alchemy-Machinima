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
    "BDMergeProjectorVolumetricsMaxDistance",
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
    "BDMergeProjectorVolumetricsFrustumClip",
    "BDMergeProjectorVolumetricsShadowJitterTap",
    "BDMergeProjectorVolumetricsShadowSamples",
    "BDMergeProjectorVolumetricsFeather",
    "BDMergeProjectorVolumetricsShadowTint",
    "BDMergeProjectorVolumetricsTint",
    "BDMergeProjectorVolumetricsTintStrength",
    "BDMergeProjectorVolumetricsDensity",
    "BDMergeProjectorVolumetricsNoiseStrength",
    "BDMergeProjectorVolumetricsNoiseScale",
    "BDMergeProjectorVolumetricsNoiseSpeed",
    "BDMergeProjectorVolumetricsDustIntensity",
    "BDMergeProjectorVolumetricsDustScale",
    "BDMergeProjectorVolumetricsDustDrift",
    // Froxel/Voxel Air is a separate engine and is intentionally not reset or
    // authored by projector-shaft presets.
    "BDMergeProjectorVolumetricsFogStrength",
    "BDMergeProjectorVolumetricsFogGroundDensity",
    "BDMergeProjectorVolumetricsFogFalloff",
    "BDMergeProjectorVolumetricsFogBase",
    "BDMergeProjectorVolumetricsBloomFeed",
    "BDMergeProjectorVolumetricsBloomFeedAnamorphic",
    "BDMergeProjectorVolumetricsRimStrength",
    "BDMergeProjectorVolumetricsRimPower",
    "BDMergeProjectorVolumetricsRimWrap",
    "BDMergeProjectorVolumetricsRimThreshold",
    "BDMergeProjectorVolumetricsRimSoftness",
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
    if (preset < FAST_PREVIEW || preset > DREAM_MIST)
    {
        return;
    }

    // Dust is a compile-time shader permutation. Keep it out of the generic
    // reset loop and write its final state once after all scalar controls, so
    // reselecting Dust Storm does not rebuild shaders off/on and every genuine
    // transition needs at most one rebuild.
    const bool wants_dust = preset == DUST_STORM;
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

    case CLEAR_AIR:
        // Crisp, restrained air: enough scatter to reveal the cone without
        // turning the whole set milky or introducing animated particulate.
        gSavedSettings.setF32("BDMergeProjectorVolumetricsMultiplier", 0.80f);
        gSavedSettings.setU32("BDMergeProjectorVolumetricsResolution", 32u);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsAnisotropy", 0.58f);
        gSavedSettings.setBOOL("BDMergeProjectorVolumetricsHalfRes", true);
        gSavedSettings.setBOOL("BDMergeProjectorVolumetricsTemporal", true);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsTemporalBlend", 0.82f);
        gSavedSettings.setU32("BDMergeProjectorVolumetricsShadowSamples", 2u);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsFeather", 0.08f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsDensity", 0.28f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsBloomFeed", 0.f);
        gSavedSettings.setBOOL("BDMergeSoftProjectorShadows", false);
        gSavedSettings.setBOOL("BDMergeGoboAnisotropic", true);
        break;

    case SEARCHLIGHT:
        // A long, tight, high-contrast beam with just enough moving air to
        // keep it photographic instead of reading as a solid cone.
        gSavedSettings.setF32("BDMergeProjectorVolumetricsMultiplier", 1.55f);
        gSavedSettings.setU32("BDMergeProjectorVolumetricsResolution", 40u);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsAnisotropy", 0.90f);
        gSavedSettings.setBOOL("BDMergeProjectorVolumetricsHalfRes", true);
        gSavedSettings.setBOOL("BDMergeProjectorVolumetricsTemporal", true);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsTemporalBlend", 0.88f);
        gSavedSettings.setU32("BDMergeProjectorVolumetricsShadowSamples", 3u);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsFeather", 0.06f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsDensity", 0.72f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsNoiseStrength", 0.08f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsNoiseScale", 1.20f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsNoiseSpeed", 0.12f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsBloomFeed", 0.15f);
        gSavedSettings.setBOOL("BDMergeSoftProjectorShadows", true);
        gSavedSettings.setF32("BDMergeSoftShadowSoftness", 2.f);
        gSavedSettings.setF32("BDMergeSoftShadowMaxPenumbra", 5.f);
        gSavedSettings.setF32("BDMergeSoftShadowFill", 0.02f);
        gSavedSettings.setBOOL("BDMergeGoboAnisotropic", true);
        break;

    case DUST_STORM:
        // Dense, turbulent, wind-driven particulate. A finite cap keeps the
        // high-density beam readable instead of filling the whole projector range.
        gSavedSettings.setF32("BDMergeProjectorVolumetricsMaxDistance", 14.f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsMultiplier", 1.10f);
        gSavedSettings.setU32("BDMergeProjectorVolumetricsResolution", 36u);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsAnisotropy", 0.68f);
        gSavedSettings.setBOOL("BDMergeProjectorVolumetricsHalfRes", true);
        gSavedSettings.setBOOL("BDMergeProjectorVolumetricsTemporal", true);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsTemporalBlend", 0.80f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsTemporalReject", 5.f);
        gSavedSettings.setU32("BDMergeProjectorVolumetricsShadowSamples", 2u);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsFeather", 0.28f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsDensity", 3.0f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsNoiseStrength", 0.75f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsNoiseScale", 0.35f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsNoiseSpeed", 0.45f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsDustIntensity", 1.60f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsDustScale", 0.60f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsDustDrift", 0.35f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsFogStrength", 0.55f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsFogGroundDensity", 3.f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsFogFalloff", 6.f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsBloomFeed", 0.18f);
        gSavedSettings.setBOOL("BDMergeSoftProjectorShadows", true);
        gSavedSettings.setF32("BDMergeSoftShadowSoftness", 4.5f);
        gSavedSettings.setF32("BDMergeSoftShadowMaxPenumbra", 9.f);
        gSavedSettings.setF32("BDMergeSoftShadowFill", 0.06f);
        gSavedSettings.setBOOL("BDMergeGoboAnisotropic", true);
        break;

    case DREAM_MIST:
        // Broad, low-contrast mist with a gentle halo and slow large-scale drift.
        gSavedSettings.setF32("BDMergeProjectorVolumetricsMaxDistance", 18.f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsMultiplier", 0.90f);
        gSavedSettings.setU32("BDMergeProjectorVolumetricsResolution", 32u);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsAnisotropy", 0.74f);
        gSavedSettings.setBOOL("BDMergeProjectorVolumetricsHalfRes", true);
        gSavedSettings.setBOOL("BDMergeProjectorVolumetricsTemporal", true);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsTemporalBlend", 0.90f);
        gSavedSettings.setU32("BDMergeProjectorVolumetricsShadowSamples", 2u);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsFeather", 0.34f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsDensity", 0.65f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsNoiseStrength", 0.12f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsNoiseScale", 0.20f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsNoiseSpeed", 0.08f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsFogStrength", 0.75f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsFogGroundDensity", 2.2f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsFogFalloff", 12.f);
        gSavedSettings.setF32("BDMergeProjectorVolumetricsBloomFeed", 0.50f);
        gSavedSettings.setBOOL("BDMergeSoftProjectorShadows", true);
        gSavedSettings.setF32("BDMergeSoftShadowSoftness", 5.f);
        gSavedSettings.setF32("BDMergeSoftShadowMaxPenumbra", 9.f);
        gSavedSettings.setF32("BDMergeSoftShadowFill", 0.08f);
        gSavedSettings.setBOOL("BDMergeGoboAnisotropic", true);
        break;
    }

    if (gSavedSettings.getBOOL("BDMergeProjectorVolumetricsDust") !=
        wants_dust)
    {
        gSavedSettings.setBOOL(
            "BDMergeProjectorVolumetricsDust", wants_dust);
    }
}
}
