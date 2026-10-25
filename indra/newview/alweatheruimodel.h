/**
 * @file alweatheruimodel.h
 * @brief Shared weather commands and revisioned status; no widget work in draw().
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
#ifndef AL_WEATHER_UI_MODEL_H
#define AL_WEATHER_UI_MODEL_H
#include "alweatherbridge.h"
#include <array>
#include <string>

class ALWeatherUIModel
{
public:
    static constexpr U32 PRESET_VERSION = 3; // WP2 surface settings; older looks reset new gates off.
    static F32 presetRate(U32 preset);
    static const char* presetName(U32 preset);
    static void applyPreset(ALWeatherBridge::Settings& settings, U32 preset);
    static void stop(ALWeatherBridge::Settings& settings);
    static bool parseSeed(const std::string& text, U32& seed);
    bool refresh(const ALWeatherBridge::Settings& settings, const SLWeather::State& state,
                 bool catching_up, F64 weather_seconds = 0.0);
    U64 revision() const { return mRevision; }
    const std::string& status() const { return mStatus; }
private:
    U64 mRevision = 0;
    std::string mStatus;
    std::array<F64, 15> mSignature{};
    F64 mNextRefresh = 0.0, mLastSample = 0.0, mTransitionEnd = 0.0;
    bool mInitialized = false, mCatchingUp = false;
};
#endif
