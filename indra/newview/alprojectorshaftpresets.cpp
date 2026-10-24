/**
 * @file alprojectorshaftpresets.cpp
 * @brief Applies projector-shaft looks and saved user looks.
 *
 * Ownership (which settings a look resets and writes) is ALProjectorShaftLooks::ownedSettings().
 * Quality, calibration and the other shader permutations are never touched by a look.
 */

#include "llviewerprecompiledheaders.h"
#include "alpresetlook.h"

#include "alprojectorshaftpresets.h"
#include "alprojectorshaftlooks.h"

#include "llcontrol.h"
#include "llviewercontrol.h"

#include <algorithm>

namespace
{
void resetOwned()
{
    for (const std::string& name : ALProjectorShaftLooks::ownedSettings())
    {
        if (LLControlVariable* control = gSavedSettings.getControl(name))
        {
            control->resetToDefault(true);
        }
    }
}

bool dustDefault()
{
    if (LLControlVariable* control = gSavedSettings.getControl(ALProjectorShaftLooks::dustSetting()))
    {
        return control->getDefault().asBoolean();
    }
    return false;
}

// Dust is a compile-time shader permutation. It stays out of the reset loop and is written once,
// after every scalar control, and only when it changes, so any genuine transition costs at most
// one rebuild and reselecting a look never rebuilds off/on.
void writeDust(bool wants_dust)
{
    if (gSavedSettings.getBOOL(ALProjectorShaftLooks::dustSetting()) != wants_dust)
    {
        gSavedSettings.setBOOL(ALProjectorShaftLooks::dustSetting(), wants_dust);
    }
}
}

namespace Classic
{
// Frozen reset set, defaults and preset switch from c4d3614d3e7.
enum Preset : S32
{
    CUSTOM = -1,
    FAST_PREVIEW = 0,
    BALANCED,
    CINEMATIC,
    HAZY_STAGE,
    CLEAR_AIR,
    SEARCHLIGHT,
    DUST_STORM,
    DREAM_MIST,
};
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
    gSavedSettings.setBOOL("BDMergeProjectorVolumetrics", false);
    gSavedSettings.setF32("BDMergeProjectorVolumetricsMultiplier", 1.0f);
    gSavedSettings.setF32("BDMergeProjectorVolumetricsMaxDistance", 0.0f);
    gSavedSettings.setU32("BDMergeProjectorVolumetricsResolution", 16u);
    gSavedSettings.setF32("BDMergeProjectorVolumetricsAnisotropy", 0.72f);
    gSavedSettings.setBOOL("BDMergeProjectorVolumetricsHalfRes", true);
    gSavedSettings.setBOOL("BDMergeProjectorVolumetricsTemporal", true);
    gSavedSettings.setF32("BDMergeProjectorVolumetricsTemporalBlend", 0.75f);
    gSavedSettings.setF32("BDMergeProjectorVolumetricsTemporalReject", 3.5f);
    gSavedSettings.setBOOL("BDMergeProjectorVolumetricsTemporalBeamDepth", true);
    gSavedSettings.setBOOL("BDMergeProjectorVolumetricsScissor", true);
    gSavedSettings.setBOOL("BDMergeProjectorVolumetricsAdaptive", true);
    gSavedSettings.setU32("BDMergeProjectorVolumetricsMinResolution", 6u);
    gSavedSettings.setF32("BDMergeProjectorVolumetricsMaxLuminance", 8.0f);
    gSavedSettings.setU32("BDMergeProjectorVolumetricsDither", 1u);
    gSavedSettings.setBOOL("BDMergeProjectorVolumetricsTemporalDither", true);
    gSavedSettings.setBOOL("BDMergeProjectorVolumetricsFrustumClip", true);
    gSavedSettings.setBOOL("BDMergeProjectorVolumetricsShadowJitterTap", false);
    gSavedSettings.setU32("BDMergeProjectorVolumetricsShadowSamples", 1u);
    gSavedSettings.setF32("BDMergeProjectorVolumetricsFeather", 0.15f);
    gSavedSettings.setF32("BDMergeProjectorVolumetricsShadowTint", 0.0f);
    if (LLControlVariable* control = gSavedSettings.getControl("BDMergeProjectorVolumetricsTint"))
    {
        LLSD tint = LLSD::emptyArray();
        tint.append(1.f); tint.append(1.f); tint.append(1.f);
        control->set(tint);
    }
    gSavedSettings.setF32("BDMergeProjectorVolumetricsTintStrength", 0.0f);
    gSavedSettings.setF32("BDMergeProjectorVolumetricsDensity", 1.0f);
    gSavedSettings.setF32("BDMergeProjectorVolumetricsNoiseStrength", 0.0f);
    gSavedSettings.setF32("BDMergeProjectorVolumetricsNoiseScale", 0.15f);
    gSavedSettings.setF32("BDMergeProjectorVolumetricsNoiseSpeed", 0.15f);
    gSavedSettings.setF32("BDMergeProjectorVolumetricsDustIntensity", 1.0f);
    gSavedSettings.setF32("BDMergeProjectorVolumetricsDustScale", 0.5f);
    gSavedSettings.setF32("BDMergeProjectorVolumetricsDustDrift", 0.05f);
    gSavedSettings.setF32("BDMergeProjectorVolumetricsFogStrength", 0.0f);
    gSavedSettings.setF32("BDMergeProjectorVolumetricsFogGroundDensity", 1.0f);
    gSavedSettings.setF32("BDMergeProjectorVolumetricsFogFalloff", 24.0f);
    gSavedSettings.setF32("BDMergeProjectorVolumetricsFogBase", 0.0f);
    gSavedSettings.setF32("BDMergeProjectorVolumetricsBloomFeed", 0.0f);
    gSavedSettings.setF32("BDMergeProjectorVolumetricsBloomFeedAnamorphic", 1.0f);
    gSavedSettings.setF32("BDMergeProjectorVolumetricsRimStrength", 0.0f);
    gSavedSettings.setF32("BDMergeProjectorVolumetricsRimPower", 2.0f);
    gSavedSettings.setF32("BDMergeProjectorVolumetricsRimWrap", 0.35f);
    gSavedSettings.setF32("BDMergeProjectorVolumetricsRimThreshold", 0.0f);
    gSavedSettings.setF32("BDMergeProjectorVolumetricsRimSoftness", 0.0f);
    gSavedSettings.setBOOL("BDMergeSoftProjectorShadows", false);
    gSavedSettings.setF32("BDMergeSoftShadowSoftness", 3.0f);
    gSavedSettings.setF32("BDMergeSoftShadowMaxPenumbra", 6.0f);
    gSavedSettings.setF32("BDMergeSoftShadowFill", 0.15f);
    gSavedSettings.setBOOL("BDMergeSoftShadowSun", false);
    gSavedSettings.setBOOL("BDMergeGoboAnisotropic", true);
    // The original reset also enabled the effect after restoring its defaults.
    gSavedSettings.setBOOL("BDMergeProjectorVolumetrics", true);
}
void applyClassic(S32 preset)
{
    if (ALPresetLook::replaying()) return;
    if (preset < FAST_PREVIEW || preset > DREAM_MIST)
    {
        return;
    }

    // Dust is a compile-time shader permutation. Keep it out of the generic
    // reset loop and write its final state once after all scalar controls, so
    // reselecting Dust Storm does not rebuild shaders off/on and every genuine
    // transition needs at most one rebuild.
    const bool wants_dust = preset == DUST_STORM;
    ALPresetLook::Load load;
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

namespace ALProjectorShaftPresets
{
const std::vector<std::string>& trackedSettings()
{
    static const std::vector<std::string> settings = []
    {
        std::vector<std::string> result = ALProjectorShaftLooks::ownedSettings();
        result.emplace_back(ALProjectorShaftLooks::dustSetting());
        for (const char* name : Classic::SHAFT_SETTINGS)
            if (std::find(result.begin(), result.end(), name) == result.end()) result.emplace_back(name);
        result.emplace_back("BDMergeProjectorVolumetricsEngine");
        result.emplace_back("BDMergeProjectorVolumetricsPreset");
        return result;
    }();
    return settings;
}

const std::vector<std::string>& pageSettings()
{
    static const std::vector<std::string> settings = []
    {
        std::vector<std::string> result = ALProjectorShaftLooks::ownedSettings();
        result.emplace_back(ALProjectorShaftLooks::dustSetting());
        // Page history covers every control, including controls that looks must leave alone.
        for (const char* name : {
                 "BDMergeProjectorVolumetricsEngine",
                 "BDMergeProjectorVolumetrics", "BDMergeProjectorVolumetricsHalfRes",
                 "BDMergeProjectorVolumetricsTemporal", "BDMergeProjectorVolumetricsTemporalBlend",
                 "BDMergeProjectorVolumetricsTemporalReject", "BDMergeProjectorVolumetricsTemporalBeamDepth",
                 "BDMergeProjectorVolumetricsScissor", "BDMergeProjectorVolumetricsAdaptive",
                 "BDMergeProjectorVolumetricsMinResolution", "BDMergeProjectorVolumetricsDither",
                 "BDMergeProjectorVolumetricsTemporalDither", "BDMergeProjectorVolumetricsFogBase",
                 "BDMergeProjectorVolumetricsPyramidScissor",
                 "BDMergeProjectorVolumetricsDefinition", "BDMergeProjectorVolumetricsMaxSteps", "BDMergeProjectorVolumetricsBigConeCap", "BDMergeProjectorVolumetricsFrameBudget", "BDMergeProjectorVolumetricsBeamShadows",
                 "BDMergeSoftShadowSun", "BDMergeGoboAnisotropic",
                 "BDMergeFroxelWindAzimuth", "BDMergeFroxelWindElevation", "BDMergeFroxelWindInverted"})
            result.emplace_back(name);
        return result;
    }();
    return settings;
}

bool classicEngine()
{
    return gSavedSettings.getU32("BDMergeProjectorVolumetricsEngine") == 0;
}

const std::vector<std::string>& classicLabels()
{
    static const std::vector<std::string> labels = {"Fast Preview", "Balanced", "Cinematic", "Hazy Stage",
        "Clear Air", "Searchlight", "Dust Storm", "Dream Mist"};
    return labels;
}

bool apply(const std::string& id)
{
    if (classicEngine())
    {
        if (id.size() != 1 || id[0] < '0' || id[0] > '7' || ALPresetLook::replaying()) return false;
        Classic::applyClassic(id[0] - '0');
        gSavedSettings.setString("BDMergeProjectorVolumetricsPreset", id);
        return true;
    }
    // Validate before touching anything: an unknown id (including Custom) writes nothing.
    const ALProjectorShaftLooks::Look* look = ALProjectorShaftLooks::findLook(ALProjectorShaftLooks::canonicalLookId(id));
    if (!look)
    {
        return false;
    }
    if (ALPresetLook::replaying()) return false;

    ALPresetLook::Load load;
    resetOwned();
    // Selecting a look is an explicit request to see the effect. The master is not owned.
    gSavedSettings.setBOOL("BDMergeProjectorVolumetrics", true);

    bool wants_dust = dustDefault();
    for (size_t i = 0; i < look->count; ++i)
    {
        const ALProjectorShaftLooks::Value& value = look->values[i];
        if (std::string(value.setting) == ALProjectorShaftLooks::dustSetting())
        {
            wants_dust = value.f[0] != 0.f;
            continue;
        }
        switch (value.type)
        {
        case ALProjectorShaftLooks::Type::F32:
            gSavedSettings.setF32(value.setting, value.f[0]);
            break;
        case ALProjectorShaftLooks::Type::U32:
            gSavedSettings.setU32(value.setting, static_cast<U32>(value.f[0] + 0.5f));
            break;
        case ALProjectorShaftLooks::Type::BOOL:
            gSavedSettings.setBOOL(value.setting, value.f[0] != 0.f);
            break;
        case ALProjectorShaftLooks::Type::COLOR3:
        {
            if (LLControlVariable* control = gSavedSettings.getControl(value.setting))
            {
                LLSD color = LLSD::emptyArray();
                color.append(value.f[0]);
                color.append(value.f[1]);
                color.append(value.f[2]);
                control->set(color);
            }
            break;
        }
        }
    }
    writeDust(wants_dust);
    gSavedSettings.setString("BDMergeProjectorVolumetricsPreset", ALProjectorShaftLooks::canonicalLookId(id));
    return true;
}

bool applyValues(const LLSD& values)
{
    if (!values.isMap())
    {
        return false;
    }
    if (ALPresetLook::replaying()) return false;

    ALPresetLook::Load load;
    gSavedSettings.setString("BDMergeProjectorVolumetricsPreset", "");
    if (classicEngine())
    {
        Classic::resetToKnownBase();
        for (const char* name : Classic::SHAFT_SETTINGS)
            if (values.has(name) && !values[name].isUndefined())
                if (LLControlVariable* control = gSavedSettings.getControl(name)) control->set(values[name]);
        writeDust(values[ALProjectorShaftLooks::dustSetting()].asBoolean());
        return true;
    }
    resetOwned();
    gSavedSettings.setBOOL("BDMergeProjectorVolumetrics", true);
    for (const std::string& name : ALProjectorShaftLooks::ownedSettings())
    {
        if (!values.has(name) || values[name].isUndefined())
        {
            continue;
        }
        if (LLControlVariable* control = gSavedSettings.getControl(name).get())
        {
            control->set(values[name]);
        }
    }
    const std::string dust = ALProjectorShaftLooks::dustSetting();
    writeDust(values.has(dust) && !values[dust].isUndefined() ? values[dust].asBoolean() : dustDefault());
    return true;
}
LLSD sceneData()
{
    LLSD data = LLSD::emptyMap();
    data["engine"] = classicEngine() ? 0 : 1;
    data["preset"] = gSavedSettings.getString("BDMergeProjectorVolumetricsPreset");
    for (const std::string& name : trackedSettings())
        if (LLControlVariable* control = gSavedSettings.getControl(name)) data["settings"][name] = control->get();
    return data;
}

void applySceneData(const LLSD& data)
{
    // A scene predating the selector always means New, even if Classic is live.
    gSavedSettings.setU32("BDMergeProjectorVolumetricsEngine", data.has("engine") && data["engine"].asInteger() == 0 ? 0 : 1);
    gSavedSettings.setString("BDMergeProjectorVolumetricsPreset", "");
    if (data["preset"].isString()) apply(data["preset"].asString());
    for (const std::string& name : trackedSettings())
    {
        if (name == "BDMergeProjectorVolumetricsEngine" || name == "BDMergeProjectorVolumetricsPreset") continue;
        if (data["settings"].has(name))
            if (LLControlVariable* control = gSavedSettings.getControl(name)) control->set(data["settings"][name]);
    }
}

}
