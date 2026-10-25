/**
 * @file alweatherbridge.h
 * @brief Immutable per-iteration adapter for Slipstream weather and native rain/lightning.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork; weather formulas credited in slweather.h.
 * $/LicenseInfo$
 */
#ifndef AL_WEATHER_BRIDGE_H
#define AL_WEATHER_BRIDGE_H
#include "alweatherclock.h"
#include "alweathermodel.h"
#include "slweather.h"

namespace ALWeatherBridge
{
    struct Settings
    {
        bool enabled = false, local = false;
        U32 mode = 0, windSource = 0, lightningTiming = 0, seed = 1597463007U;
        // WP2 uses the same production fixed-step integrator in either rain mode.
        bool persistentWet = false, puddles = false, legacyRain = true;
        F32 wetAmount = 0.5f, puddleAmount = 0.35f, rippleStrength = 0.25f;
        F32 wetSeconds = 120.f, drySeconds = 300.f, startWetness = 0.f;
        F32 legacyIntensity = 0.55f, coverExtent = 128.f;
        U32 coverResolution = 512;
        F32 rainRate = 5.f, bands = 0.35f, transition = 20.f;
        F32 windDirection = 225.f, windSpeed = 3.f, windVariation = 0.3f;
        F32 gustiness = 0.5f, directionSwing = 0.f, storm = 0.f;
    };
    Settings sanitize(const Settings& settings);
    SLWeather::Config configuration(const Settings& settings, F32 simulator_x, F32 simulator_y);
    struct Rain
    {
        F32 intensity = 0.f, density = 0.f, windX = 0.f, windY = 0.f;
    };
    Rain rain(const SLWeather::State& state, F32 intensity, F32 density);

    class Controller
    {
    public:
        static Controller& instance();
        // Exactly once before all view draws. Draw code can only read getters.
        void update(U64 iteration, F64 presentation, const Settings& settings,
                    F32 simulator_x, F32 simulator_y, U64 region,
                    const ALWeatherModel::Config& lightning,
                    const ALWeatherModel::Position& anchor, bool manual_strike = false);
        void teleportSucceeded() { mTeleportPending = true; }
        bool active() const { return mActive; }
        bool surfacesActive() const { return mSettings.enabled && mSettings.persistentWet; }
        const Settings& settings() const { return mSettings; }
        void applyStartingSurfaces() { mSurfaceCommand = 1; }
        void clearSurfaces() { mSurfaceCommand = 2; }
        U32 surfaceSerial() const { return mSurfaceSerial; }
        const SLWeather::State& state() const { return mWeather.state(); }
        const ALWeatherModel::Frame& lightning() const { return mLightningFrame; }
        const ALWeatherClock& clock() const { return mClock; }
        U32 placeSerial() const { return mWeather.placeSerial(); }
        U32 clearSerial() const { return mWeather.clearSerial(); }
        bool catchingUp() const { return mClock.catchingUp(); }
    private:
        SLWeather mWeather;
        ALWeatherClock mClock;
        ALWeatherModel::Controller mLightning;
        ALWeatherModel::Frame mLightningFrame;
        U64 mIteration = ~U64(0), mCoreTick = 0;
        F64 mLastPresentation = -1.0;
        U32 mLastStrike = 0, mTiming = 0, mSeed = 0;
        Settings mSettings;
        U32 mSurfaceCommand = 0, mSurfaceSerial = 0, mMode = 0;
        bool mSimActive = false;
        bool mActive = false, mTeleportPending = false;
    };
}
#endif
