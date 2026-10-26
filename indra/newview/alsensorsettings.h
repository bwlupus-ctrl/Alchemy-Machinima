/**
 * @file alsensorsettings.h
 * @brief Shared sensor calibration resolution for rendering and Director readouts.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
#ifndef AL_SENSOR_SETTINGS_H
#define AL_SENSOR_SETTINGS_H

#include "alsensormodel.h"
#include "llviewercontrol.h"
#include <cmath>

namespace ALSensorSettings
{
inline ALSensorModel::Preset preset()
{
    using namespace ALSensorModel;
    const U32 index = gSavedSettings.getU32("CineSensorPreset");
    Preset result = illustrativePresets()[llmin(index, 2u)];
    if (index != 3) return result;
    result.name = "Custom illustrative sensor";
    const auto value = [](const std::string& name, F32 fallback, F32 low, F32 high)
    {
        const F32 setting = gSavedSettings.getF32(name);
        return llclamp(std::isfinite(setting) ? setting : fallback, low, high);
    };
    result.baseISO = value("CineSensorCustomBaseISO", 100.f, 1.f, 102400.f);
    result.switchISO = value("CineSensorCustomSwitchISO", 0.f, 0.f, 102400.f);
    result.headroom = value("CineSensorCustomHeadroom", 3.5f, 0.f, 12.f);
    const auto calibration = [&](const std::string& mode)
    {
        const std::string prefix = "CineSensorCustom" + mode;
        const F32 pedestal = value(prefix + "Pedestal", 512.f, 0.f, 65534.f);
        return Calibration{ value(prefix + "FullWell", 50000.f, 1.f, 1e7f),
            value(prefix + "ReadNoise", 3.f, .001f, 1e4f),
            value(prefix + "AnalogBase", .25f, .0001f, 100.f), pedestal,
            value(prefix + "White", 16383.f, pedestal + 1.f, 65535.f) };
    };
    result.lcg = calibration("LCG");
    result.hcg = calibration("HCG");
    return result;
}
}
#endif
