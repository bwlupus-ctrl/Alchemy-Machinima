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
    static const F32 rates[] = {1.f, 5.f, 20.f, 80.f};
    return rates[std::min(preset, 3U)];
}
const char* ALWeatherUIModel::presetName(U32 preset)
{
    static const char* names[] = {"Drizzle", "Steady rain", "Heavy rain", "Downpour"};
    return names[std::min(preset, 3U)];
}
void ALWeatherUIModel::applyPreset(ALWeatherBridge::Settings& settings, U32 preset)
{
    if (preset >= 4) return; // Custom: keep the directly entered rate.
    settings.mode = 1;
    settings.local = true;
    settings.rainRate = presetRate(preset);
}
void ALWeatherUIModel::stop(ALWeatherBridge::Settings& settings)
{
    settings.rainRate = 0.f;
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
bool ALWeatherUIModel::refresh(const ALWeatherBridge::Settings& settings, const SLWeather::State& state,
                               bool catching_up, F64 weather_seconds)
{
    const auto s = ALWeatherBridge::sanitize(settings);
    const std::array<F64, 15> signature{{static_cast<F64>(s.enabled), static_cast<F64>(s.local),
        static_cast<F64>(s.mode), static_cast<F64>(s.windSource), static_cast<F64>(s.lightningTiming),
        static_cast<F64>(s.seed), s.rainRate, s.bands, s.transition, s.windDirection,
        s.windSpeed, s.windVariation, s.gustiness, s.directionSwing, s.storm}};
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
    if (!s.enabled) out << "Weather off";
    else if (s.mode == 0) out << "Existing rain";
    else
    {
        out << (s.local ? "Local rain" : "Clear") << " - " << std::fixed << std::setprecision(1)
            << state.rainFall << " mm/h now; target " << (s.local ? s.rainRate : 0.f);
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
