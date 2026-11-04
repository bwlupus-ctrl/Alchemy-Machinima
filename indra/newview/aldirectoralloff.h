/** Director v2's deliberately bounded reset contract. */
#ifndef AL_DIRECTOR_ALL_OFF_H
#define AL_DIRECTOR_ALL_OFF_H

#include "llsd.h"
#include "llcontrol.h"
#include <cmath>
#include <string>

namespace ALDirectorAllOff
{
enum class Operation { Default, Off, Rig, Switcher };
struct Feature { const char* setting; Operation operation; };

// THE feature list. Capture, validation, reset and restore all traverse this table.
inline constexpr Feature FEATURES[] = {
    { "AlchemyEnvSunEV", Operation::Default },
    { "AlchemyEnvMoonEV", Operation::Default },
    { "AlchemyEnvSkyGIEV", Operation::Default },
    { "AlchemyEnvGIProbeDiffuseEV", Operation::Default },
    { "AlchemyEnvGIProbeSpecEV", Operation::Default },
    { "AlchemyEnvGIAmbientEV", Operation::Default },
    { "AlchemyEnvLocalLightEV", Operation::Default },
    { "AlchemyEnvLocalLightSize", Operation::Default },
    { "AlchemyEnvShadowLiftEV", Operation::Default },
    { "AlchemyEnvEnvironmentEV", Operation::Default },
    { "AlchemyEnvSunKelvin", Operation::Default },
    { "AlchemyEnvSunTintColor", Operation::Default },
    { "AlchemyEnvSunTintStrength", Operation::Default },
    { "AlchemyEnvMoonTintColor", Operation::Default },
    { "AlchemyEnvMoonTintStrength", Operation::Default },
    { "AlchemyEnvAmbientTintColor", Operation::Default },
    { "AlchemyEnvAmbientTintStrength", Operation::Default },
    { "AlchemyEnvMoonLinked", Operation::Default },
    { "AlchemyEnvLocalLightIncludeRig", Operation::Default },
    { "CineLightRigEnabled", Operation::Rig },
    { "CineLightRigLiveProbeEnabled", Operation::Off },
    { "CineRigRimEnabled", Operation::Off },
    { "BDMergeProjectorVolumetrics", Operation::Off },
    { "BDMergeSoftProjectorShadows", Operation::Off },
    { "CineOutlineEnabled", Operation::Off },
    { "TronEnabled", Operation::Off },
    { "StepAnimEnabled", Operation::Off },
    { "RenderGradNDEnabled", Operation::Off },
    { "RenderPolarizerEnabled", Operation::Off },
    { "RenderVariableNDEnabled", Operation::Off },
    { "RenderInfraredEnabled", Operation::Off },
    { "RenderSoftEdgeEnabled", Operation::Off },
    { "CineDiopterEnabled", Operation::Off },
    { "DirectorLookAtCameraEnabled", Operation::Off },
    { "DirectorSwitcherArmed", Operation::Switcher },
    { "CinematicCamEnabled", Operation::Off },
    { "FlycamOrbitEnabled", Operation::Off },
    { "CinematicAutoFrameEnabled", Operation::Off },
    { "CameraShakeApplyActive", Operation::Off },
    { "PrismLensEnabled", Operation::Off },
    { "ActorKeyEnabled", Operation::Off },
    { "AlchemyWeatherEnabled", Operation::Off },
    { "BDMergeLocalFogVolumes", Operation::Off },
};

template<typename Read>
LLSD captureSettings(Read read)
{
    LLSD result = LLSD::emptyMap();
    for (const Feature& feature : FEATURES)
        result[feature.setting] = read(feature);
    return result;
}

template<typename ReadType>
bool validSettings(const LLSD& settings, ReadType readType)
{
    if (!settings.isMap() || settings.size() != sizeof(FEATURES) / sizeof(FEATURES[0])) return false;
    for (const Feature& feature : FEATURES)
    {
        const LLSD& value = settings[feature.setting];
        switch (readType(feature))
        {
        case TYPE_F32:
            if (!value.isReal() || !std::isfinite(value.asReal())) return false;
            break;
        case TYPE_BOOLEAN:
            if (!value.isBoolean()) return false;
            break;
        case TYPE_COL4:
            if (!value.isArray() || value.size() != 4) return false;
            for (S32 component = 0; component < 4; ++component)
                if (!value[component].isReal() || !std::isfinite(value[component].asReal())) return false;
            break;
        default:
            return false;
        }
    }
    return true;
}

template<typename Write>
void applySettings(const LLSD& saved, bool restore, Write write)
{
    for (const Feature& feature : FEATURES)
        write(feature, restore ? saved[feature.setting] : LLSD());
}

// Atomic replace: a complete, flushed, closed temporary file, then one rename.
// Shared with ALR's reset snapshot restore; never removes the previous target first.
bool atomicWrite(const std::string& path, const std::string& bytes);
bool cameraTakeActive();
bool available(std::string& reason);
bool reset(std::string& message);
bool restore(std::string& message);
}
#endif
