/**
 * @file alweathermodel.h
 * @brief Deterministic, renderer-independent weather/lightning state.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_WEATHER_MODEL_H
#define AL_WEATHER_MODEL_H

#include "stdtypes.h"

namespace ALWeatherModel
{
    // Hard limits are enforced here as well as in the UI. The model is fed by
    // persisted settings and must remain safe if settings.xml is edited by hand.
    constexpr F64 MAX_RATE_PER_MINUTE = 60.0;
    constexpr F64 MAX_DISTANCE_METERS = 512.0;
    constexpr F64 MAX_BOLT_HEIGHT_METERS = 512.0;
    constexpr F64 MAX_DURATION_SECONDS = 4.0;
    constexpr F64 MAX_QUALITY_AFTERGLOW_STRENGTH = 1.0;
    constexpr F64 QUALITY_AFTERGLOW_SECONDS = 0.30;

    struct Position
    {
        F64 mX = 0.0;
        F64 mY = 0.0;
        F64 mZ = 0.0;
    };

    struct Config
    {
        bool mEnabled = false;
        F64  mRatePerMinute = 1.5;
        F64  mFlashDurationSeconds = 0.35;
        F64  mBoltDurationSeconds = 0.18;
        F64  mMinDistanceMeters = 45.0;
        F64  mMaxDistanceMeters = 130.0;
        F64  mBoltHeightMeters = 160.0;
        bool mQualityEnabled = false;
        F64  mQualityAfterglowStrength = 0.35;
        U64  mSeed = 0x416c6368656d79ULL; // "Alchemy", and never a zero PRNG state.
    };

    struct Frame
    {
        bool     mActive = false;
        bool     mStrikeStarted = false;
        F32      mFlash = 0.f;
        F32      mBolt = 0.f;
        F32      mQualityBolt = 0.f;
        F32      mAfterglow = 0.f;
        F32      mColorVariation = 0.f;
        U32      mStrokeIndex = 0;
        U32      mVisualSeed = 0;
        F64      mStrikeTime = 0.0;
        Position mStrikeBase;
        F64      mStrikeDistanceMeters = 0.0;
        F64      mBoltHeightMeters = 0.0;
        U64      mStrikeId = 0;
    };

    Config sanitizeConfig(const Config& config);
    bool isFinite(const Position& position);

    class Controller
    {
    public:
        Controller() = default;

        // Advances to an absolute monotonic/presentation time. At most one
        // automatic strike can start per call, even after a large frame hitch.
        Frame update(F64 now_seconds, const Config& config,
                     const Position& anchor);

        // Starts a strike immediately. Invalid time/anchor or a disabled config
        // is rejected without mutating the current valid state.
        bool trigger(F64 now_seconds, const Config& config,
                     const Position& anchor);

        // Clears active/scheduled state and deterministically reseeds.
        void reset(U64 seed = 0x416c6368656d79ULL);

        U64 strikeCount() const { return mStrikeId; }

    private:
        static F32 flashPulse(F64 age_seconds, F64 duration_seconds);
        static F32 boltPulse(F64 age_seconds, F64 duration_seconds);
        static F32 qualityBoltPulse(F64 age_seconds, F64 duration_seconds,
                                    U64 strike_key, U32& stroke_index);
        static F32 afterglowPulse(F64 age_seconds, F64 flash_duration_seconds,
                                  F64 strength);

        U64 nextRandomU64();
        F64 nextUnit();
        F64 nextIntervalWork();
        void scheduleAfter(F64 now_seconds, const Config& config);
        void rescaleSchedule(F64 now_seconds, F64 new_rate_per_minute);
        void beginStrike(F64 strike_time, const Config& config,
                         const Position& anchor, bool carry_remainder);
        void initializeTimeline(F64 now_seconds, const Config& config);
        void advanceFixedClock(F64 now_seconds);
        F64 fixedTickAtOrAfter(F64 time_seconds) const;
        F64 interpolatedTime() const;
        Frame makeFrame(F64 now_seconds, const Config& config,
                         bool strike_started) const;

        bool     mInitialized = false;
        bool     mWasEnabled = false;
        U64      mConfiguredSeed = 0;
        U64      mRandomState = 0;
        F64      mLastTime = 0.0;
        F64      mSimulationOrigin = 0.0;
        F64      mSimulationTime = 0.0;
        F64      mSimAccumulator = 0.0;
        F64      mNextStrikeTime = 0.0;
        F64      mPausedIntervalWork = 0.0;
        F64      mScheduledRatePerMinute = 0.0;
        F64      mStrikeStartTime = 0.0;
        Position mStrikeBase;
        F64      mStrikeDistanceMeters = 0.0;
        F64      mBoltHeightMeters = 0.0;
        U64      mStrikeId = 0;
        bool     mHasStrike = false;
    };
}

#endif // AL_WEATHER_MODEL_H
