/**
 * @file alweatherbridge.cpp
 * @brief Physical-rate bridge; existing rain uniforms/permutations remain unchanged.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork; Slipstream Viewer contributors (slweather.h).
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "alweatherbridge.h"
#include <algorithm>
#include <cmath>

namespace ALWeatherBridge
{
    namespace
    {
        F32 safe(F32 value, F32 fallback, F32 lo, F32 hi)
        {
            return std::clamp(std::isfinite(value) ? value : fallback, lo, hi);
        }
    }
    Settings sanitize(const Settings& settings)
    {
        Settings s = settings;
        s.mode = std::min(s.mode, 1U);
        s.windSource = std::min(s.windSource, 1U);
        s.lightningTiming = std::min(s.lightningTiming, 1U);
        s.rainRate = safe(s.rainRate, 5.f, 0.f, 200.f);
        s.precipType = std::min(s.precipType, 2U);
        s.snowRate = safe(s.snowRate, 1.5f, 0.f, 360.f);
        s.flakeSize = safe(s.flakeSize, 0.015f, 0.002f, 0.08f);
        s.snowSpeed = safe(s.snowSpeed, 1.f, 0.2f, 4.f);
        s.snowRange = safe(s.snowRange, 64.f, 16.f, 128.f);
        s.snowLimit = std::clamp(s.snowLimit, 4096U, 131072U);
        s.visibilityAmount = safe(s.visibilityAmount, 1.f, 0.f, 1.f);
        s.bands = safe(s.bands, 0.35f, 0.f, 1.f);
        s.transition = safe(s.transition, 20.f, 0.f, 600.f);
        s.windDirection = safe(s.windDirection, 225.f, 0.f, 360.f);
        s.windSpeed = safe(s.windSpeed, 3.f, 0.f, 40.f);
        s.windVariation = safe(s.windVariation, 0.3f, 0.f, 1.f);
        s.gustiness = safe(s.gustiness, 0.5f, 0.f, 1.f);
        s.directionSwing = safe(s.directionSwing, 0.f, 0.f, 180.f);
        s.storm = safe(s.storm, 0.f, 0.f, 1.f);
        s.wetAmount = safe(s.wetAmount, 0.5f, 0.f, 1.f);
        s.puddleAmount = safe(s.puddleAmount, 0.35f, 0.f, 1.f);
        s.rippleStrength = safe(s.rippleStrength, 0.25f, 0.f, 1.f);
        s.wetSeconds = safe(s.wetSeconds, 120.f, 1.f, 3600.f);
        s.drySeconds = safe(s.drySeconds, 300.f, 1.f, 7200.f);
        s.startWetness = safe(s.startWetness, 0.f, 0.f, 1.f);
        s.legacyIntensity = safe(s.legacyIntensity, 0.55f, 0.f, 1.f);
        s.coverExtent = safe(s.coverExtent, 128.f, 32.f, 256.f);
        s.coverResolution = s.coverResolution < 384 ? 256 : (s.coverResolution < 768 ? 512 : 1024);
        if (s.advancedShelter)
        {
            const auto detail = [](U32 n, U32 cap)
            {
                U32 result = 256;
                while (result < cap && n >= result * 2) result *= 2;
                return result;
            };
            s.shelterNearResolution = detail(s.shelterNearResolution, 2048);
            s.shelterFarResolution = detail(s.shelterFarResolution, 1024);
            s.shelterNearExtent = safe(s.shelterNearExtent, 32.f, 16.f, 128.f);
            s.shelterFarExtent = safe(s.shelterFarExtent, 128.f, 64.f, 512.f);
            s.shelterHz = std::clamp(s.shelterHz, 5U, 60U);
            s.shelterBias = safe(s.shelterBias, 0.12f, 0.f, 1.f);
            s.shelterSoftness = safe(s.shelterSoftness, 0.35f, 0.01f, 2.f);
            s.shelterFallSpeed = safe(s.shelterFallSpeed, 28.f, 1.f, 80.f);
        }
        return s;
    }
    SLWeather::Config configuration(const Settings& settings, F32 simulator_x, F32 simulator_y)
    {
        const Settings s = sanitize(settings);
        SLWeather::Config config;
        config.master = s.enabled && (s.mode == 1 || s.persistentWet || s.advancedShelter);
        config.wetSurfaces = s.persistentWet;
        config.wetSeconds = s.wetSeconds;
        config.drySeconds = s.drySeconds;
        config.startWetness = s.startWetness;
        auto& t = config.local;
        t.active = s.mode == 1 ? s.local : s.legacyRain;
        if (s.mode == 0)
        {
            t.setFall(SLWeather::PRECIP_RAIN, SLWeather::fallFromAmount(SLWeather::PRECIP_RAIN, s.legacyIntensity));
            t.transitionSecs = 0.f;
            t.fallVariation = 0.f;
            if (s.advancedShelter)
            {
                const F32 mean_x = safe(simulator_x, 0.f, -40.f, 40.f);
                const F32 mean_y = safe(simulator_y, 0.f, -40.f, 40.f);
                t.windSpeed = std::min(std::hypot(mean_x, mean_y), 40.f);
                t.windDirDeg = std::atan2(-mean_x, -mean_y) * (180.f / 3.14159265358979323846f);
                t.windVariation = 0.f;
            }
            return config;
        }
        t.transitionSecs = s.transition;
        if (!t.active) return config;
        t.setFall(SLWeather::PRECIP_RAIN, s.precipType == 1 ? 0.f : s.rainRate);
        t.setFall(SLWeather::PRECIP_SNOW, s.precipType == 0 ? 0.f : s.snowRate);
        t.fallVariation = s.bands;
        t.seed = s.seed;
        t.storm = s.lightningTiming == 1 ? s.storm : 0.f;
        t.windDirDeg = s.windDirection;
        t.windSpeed = s.windSpeed;
        t.windVariation = s.windVariation;
        t.windGustiness = s.gustiness;
        t.windDirVariation = s.directionSwing;
        if (s.windSource == 0)
        {
            const F32 x = safe(simulator_x, 0.f, -40.f, 40.f);
            const F32 y = safe(simulator_y, 0.f, -40.f, 40.f);
            t.windSpeed = std::min(std::hypot(x, y), 40.f);
            t.windDirDeg = std::atan2(-x, -y) * 57.29577951308232f;
            t.windVariation = t.windDirVariation = 0.f;
        }
        return config;
    }
    Rain rain(const SLWeather::State& state, F32 intensity, F32 density)
    {
        // 5 mm/h maps near the old 0.55 look; physical rates stay independent of art.
        const F32 r = std::clamp(SLWeather::amountFromFall(SLWeather::PRECIP_RAIN, state.rainFall), 0.f, 1.f);
        const F32 sn = std::clamp(SLWeather::amountFromFall(SLWeather::PRECIP_SNOW, state.snowFall), 0.f, 1.f);
        const F32 amount = r / std::max(1.f, r + sn);
        return {safe(intensity, 0.55f, 0.f, 1.f) * amount,
                safe(density, 0.65f, 0.02f, 1.f) * (0.2f + 0.8f * amount),
                state.wind[0], state.wind[1]};
    }
    F32 snowAmount(const SLWeather::State& state)
    {
        const F32 r = std::clamp(SLWeather::amountFromFall(SLWeather::PRECIP_RAIN, state.rainFall), 0.f, 1.f);
        const F32 sn = std::clamp(SLWeather::amountFromFall(SLWeather::PRECIP_SNOW, state.snowFall), 0.f, 1.f);
        return sn / std::max(1.f, r + sn);
    }
    Controller& Controller::instance()
    {
        static Controller controller;
        return controller;
    }
    void Controller::update(U64 iteration, F64 presentation, const Settings& settings,
                            F32 simulator_x, F32 simulator_y, U64 region,
                            const ALWeatherModel::Config& lightning,
                            const ALWeatherModel::Position& anchor, bool manual_strike)
    {
        if (iteration == mIteration || !std::isfinite(presentation) || presentation < 0.0) return;
        mIteration = iteration;
        const Settings s = sanitize(settings);
        const auto config = configuration(s, simulator_x, simulator_y);
        const bool active = config.master;
        const bool reset = active != mSimActive || s.mode != mMode ||
            (s.mode == 1 && s.seed != mSeed) || presentation < mLastPresentation;
        const bool surface_reset = s.persistentWet != mSettings.persistentWet || reset;
        mSettings = s;
        mMode = s.mode;
        if (surface_reset) ++mSurfaceSerial;
        mLastPresentation = presentation;
        if (reset)
        {
            mWeather.requestReset("weather mode/seed change");
            mClock.reset();
            mSnowDrift = {};
            mLightning.reset(lightning.mSeed);
            mLastStrike = 0;
        }
        mSimActive = active;
        mActive = active && s.mode == 1;
        mSeed = s.seed;
        if (!active)
        {
            // No library simulation/clock preparation in Existing rain or master off.
            mClock.reset();
            mLightningFrame = {};
            mTeleportPending = false;
            mSurfaceCommand = 0;
            if (reset)
            {
                SLWeather::Input off;
                off.tick = ++mCoreTick;
                off.regionHandle = region;
                mWeather.update(config, off);
            }
            return;
        }
        if (!mClock.advance(presentation)) return;
        SLWeather::Input input;
        input.regionHandle = region;
        input.tick = ++mCoreTick;
        input.seconds = mClock.seconds();
        input.syncSeconds = input.seconds; // WP1 local epoch: no wall-clock dependency.
        input.teleportSucceeded = mTeleportPending;
        mTeleportPending = false;
        if (reset || surface_reset || mSurfaceCommand || input.teleportSucceeded || !active)
        {
            mWeather.update(config, input);
            if (input.teleportSucceeded)
            {
                mLightning.reset(lightning.mSeed);
                mLastStrike = 0;
            }
        }
        input.teleportSucceeded = false;
        if (mSurfaceCommand)
        {
            mWeather.setSurfaceWetness(mSurfaceCommand == 1 ? s.startWetness : 0.f);
            mSurfaceCommand = 0;
            ++mSurfaceSerial;
        }
        // Budget catch-up without dropping any ticks; expose backlog until consumed.
        for (U32 steps = 0; active && steps < 240 && mClock.step(); ++steps)
        {
            input.tick = ++mCoreTick;
            input.seconds = mClock.seconds();
            input.syncSeconds = input.seconds;
            mWeather.update(config, input);
            if (s.snowEnabled)
            {
                mSnowDrift[0] += mWeather.state().wind[0] * (0.9 / ALWeatherClock::HZ);
                mSnowDrift[1] += mWeather.state().wind[1] * (0.9 / ALWeatherClock::HZ);
                mSnowDrift[2] -= s.snowSpeed / ALWeatherClock::HZ;
            }
        }
        mLightningFrame = {};
        if (!mActive) return;
        auto lc = lightning;
        lc.mEnabled = lc.mEnabled && active;
        if (s.lightningTiming != mTiming)
        {
            mLightning.reset(lc.mSeed);
            mLastStrike = 0;
            mTiming = s.lightningTiming;
        }
        const F64 now = mClock.seconds();
        if (s.lightningTiming == 0)
        {
            if (manual_strike) mLightning.trigger(now, lc, anchor);
            mLightningFrame = mLightning.update(now, lc, anchor);
        }
        else
        {
            lc.mRatePerMinute = 0.0; // Exactly one scheduler: no native automatic strikes.
            bool started = false;
            if (manual_strike) mLightning.trigger(now, lc, anchor);
            for (const auto& strike : mWeather.strikes())
            {
                if (strike.serial <= mLastStrike || strike.t0 > now) continue;
                mLastStrike = strike.serial;
                const F64 horizontal = std::hypot(strike.dir[0], strike.dir[1]);
                const ALWeatherModel::Position base{
                    anchor.mX + strike.dir[0] / std::max(horizontal, 1.e-6) * strike.dist,
                    anchor.mY + strike.dir[1] / std::max(horizontal, 1.e-6) * strike.dist, anchor.mZ};
                // Lifetime serials deduplicate delivery only. The scheduler's hash
                // derives from seed/slot and survives a deterministic storm restart.
                const U64 visual_id = (U64(s.seed) << 32) | strike.hash;
                started = mLightning.scheduled(strike.t0, lc, base, strike.dist, visual_id ? visual_id : 1) || started;
            }
            mLightningFrame = mLightning.update(now, lc, anchor);
            mLightningFrame.mStrikeStarted = started && mLightningFrame.mActive;
        }
    }
}
