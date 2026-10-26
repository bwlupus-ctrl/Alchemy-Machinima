/**
 * @file alweatheruimodel.cpp
 * @brief Physical rain preset commands and event-driven status.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork; rates follow the credited Slipstream weather model.
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "alweatheruimodel.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>

F32 ALWeatherUIModel::presetRate(U32 preset)
{
    static const F32 rates[] = {1.f, 5.f, 20.f, 80.f, 0.f, 0.3f, 1.5f, 5.f, 12.f};
    return rates[std::min(preset, 8U)];
}
const char* ALWeatherUIModel::presetName(U32 preset)
{
    static const char* names[] = {"Drizzle", "Steady rain", "Heavy rain", "Downpour", "Custom",
        "Flurries", "Steady snow", "Heavy snow", "Blizzard"};
    return names[std::min(preset, 8U)];
}
void ALWeatherUIModel::applyPreset(ALWeatherBridge::Settings& settings, U32 preset)
{
    if (preset == 4 || preset > 8) return; // Custom: keep the directly entered rates.
    settings.mode = 1;
    settings.local = true;
    settings.precipType = preset < 4 ? 0 : 1;
    settings.snowEnabled = preset >= 5;
    if (preset < 4) settings.rainRate = presetRate(preset);
    else settings.snowRate = presetRate(preset);
    if (preset == 8)
    {
        settings.windSource = 1;
        settings.windSpeed = std::max(15.f, settings.windSpeed);
    }
}
void ALWeatherUIModel::stop(ALWeatherBridge::Settings& settings)
{
    settings.rainRate = 0.f;
    settings.snowRate = 0.f;
    settings.storm = 0.f;
}
bool ALWeatherUIModel::parseSeed(const std::string& text, U32& seed)
{
    U32 parsed = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (text.empty() || result.ec != std::errc{} || result.ptr != text.data() + text.size()) return false;
    seed = parsed;
    return true;
}
const char* ALWeatherUIModel::snowRequirement(const ALWeatherBridge::Settings& s)
{
    if (!s.enabled) return "Weather controls off - turn on Weather";
    if (s.mode == 0) return "Library mode: Existing rain - apply a snow preset";
    if (!s.local) return "Local weather off - enable it or apply a snow preset";
    if (!s.snowEnabled) return "Falling snow off - enable it or apply a snow preset";
    if (s.precipType == 0) return "Precipitation type is Rain - choose Snow or Mixed";
    if (s.snowRate <= 0.f) return "Snow rate is zero - raise it or apply a snow preset";
    return nullptr;
}
bool ALWeatherUIModel::refresh(const ALWeatherBridge::Settings& settings, const SLWeather::State& state,
                               bool catching_up, F64 weather_seconds)
{
    const auto s = ALWeatherBridge::sanitize(settings);
    const std::array<F64, 18> signature{{static_cast<F64>(s.enabled), static_cast<F64>(s.local),
        static_cast<F64>(s.mode), static_cast<F64>(s.windSource), static_cast<F64>(s.lightningTiming),
        static_cast<F64>(s.seed), s.rainRate, s.bands, s.transition, s.windDirection,
        s.windSpeed, s.windVariation, s.gustiness, s.directionSwing, s.storm,
        static_cast<F64>(s.precipType), s.snowRate, static_cast<F64>(s.snowEnabled)}};
    const bool changed = !mInitialized || signature != mSignature || weather_seconds < mLastSample;
    if (!changed && catching_up == mCatchingUp && weather_seconds < mNextRefresh) return false;
    if (changed) mTransitionEnd = weather_seconds + s.transition;
    mInitialized = true;
    mSignature = signature;
    mCatchingUp = catching_up;
    mLastSample = weather_seconds;
    mNextRefresh = weather_seconds + 0.1; // Live numeric readouts: at most 10 Hz.
    std::ostringstream out;
    out.imbue(std::locale::classic());
    if (!s.enabled) out << "Weather controls off - turn on Weather";
    else if (s.mode == 0) out << "Library mode: Existing rain - apply a preset";
    else
    {
        out << (s.local ? (s.precipType == 0 ? "Local rain" : (s.precipType == 1 ? "Local snow" : "Local mixed")) : "Local weather off (clear)")
            << " - " << std::fixed << std::setprecision(1);
        if (s.precipType != 1)
            out << state.rainFall << " mm/h now; target " << (s.local ? s.rainRate : 0.f);
        if (s.precipType != 0)
            out << (s.precipType == 2 ? "; snow " : "") << state.snowFall << " cm/h now; target " << (s.local ? s.snowRate : 0.f);
        if (weather_seconds < mTransitionEnd)
            out << " (transition, " << static_cast<U32>(std::ceil(mTransitionEnd - weather_seconds)) << " s left)";
        else if (s.bands > 0.f) out << " (bands)";
        out << "; wind " << std::hypot(state.wind[0], state.wind[1]) << " m/s";
        if (catching_up) out << " (catching up)";
    }
    if (out.str() == mStatus) return false;
    mStatus = out.str();
    ++mRevision;
    return true;
}
