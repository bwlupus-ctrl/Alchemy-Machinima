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
        s.bands = safe(s.bands, 0.35f, 0.f, 1.f);
        s.transition = safe(s.transition, 20.f, 0.f, 600.f);
        s.windDirection = safe(s.windDirection, 225.f, 0.f, 360.f);
        s.windSpeed = safe(s.windSpeed, 3.f, 0.f, 40.f);
        s.windVariation = safe(s.windVariation, 0.3f, 0.f, 1.f);
        s.gustiness = safe(s.gustiness, 0.5f, 0.f, 1.f);
        s.directionSwing = safe(s.directionSwing, 0.f, 0.f, 180.f);
        s.storm = safe(s.storm, 0.f, 0.f, 1.f);
        return s;
    }
    SLWeather::Config configuration(const Settings& settings, F32 simulator_x, F32 simulator_y)
    {
        const Settings s = sanitize(settings);
        SLWeather::Config config;
        config.master = s.enabled && s.mode == 1;
        auto& t = config.local;
        t.active = s.local;
        t.transitionSecs = s.transition;
        if (!t.active) return config;
        t.setFall(SLWeather::PRECIP_RAIN, s.rainRate);
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
        const F32 amount = std::clamp(SLWeather::amountFromFall(SLWeather::PRECIP_RAIN, state.rainFall), 0.f, 1.f);
        return {safe(intensity, 0.55f, 0.f, 1.f) * amount,
                safe(density, 0.65f, 0.02f, 1.f) * (0.2f + 0.8f * amount),
                state.wind[0], state.wind[1]};
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
        const bool reset = active != mActive || s.seed != mSeed || presentation < mLastPresentation;
        mLastPresentation = presentation;
        if (reset)
        {
            mWeather.requestReset("weather mode/seed change");
            mClock.reset();
            mLightning.reset(lightning.mSeed);
            mLastStrike = 0;
        }
        mActive = active;
        mSeed = s.seed;
        if (!active)
        {
            // No library simulation/clock preparation in Existing rain or master off.
            mClock.reset();
            mLightningFrame = {};
            mTeleportPending = false;
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
        if (reset || input.teleportSucceeded || !active)
        {
            mWeather.update(config, input);
            if (input.teleportSucceeded)
            {
                mLightning.reset(lightning.mSeed);
                mLastStrike = 0;
            }
        }
        input.teleportSucceeded = false;
        // Budget catch-up without dropping any ticks; expose backlog until consumed.
        for (U32 steps = 0; active && steps < 240 && mClock.step(); ++steps)
        {
            input.tick = ++mCoreTick;
            input.seconds = mClock.seconds();
            input.syncSeconds = input.seconds;
            mWeather.update(config, input);
        }
        mLightningFrame = {};
        if (!active) return;
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
                started = mLightning.scheduled(strike.t0, lc, base, strike.dist, strike.serial) || started;
            }
            mLightningFrame = mLightning.update(now, lc, anchor);
            mLightningFrame.mStrikeStarted = started && mLightningFrame.mActive;
        }
    }
}
