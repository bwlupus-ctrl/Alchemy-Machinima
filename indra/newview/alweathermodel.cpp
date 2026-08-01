/**
 * @file alweathermodel.cpp
 * @brief Deterministic, renderer-independent weather/lightning state.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "alweathermodel.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ALWeatherModel
{
    namespace
    {
        constexpr U64 FALLBACK_SEED = 0x9e3779b97f4a7c15ULL;
        constexpr F64 TWO_PI = 6.283185307179586476925286766559;
        constexpr F64 MIN_INTERVAL_SECONDS = 0.25;
        constexpr F64 MAX_INTERVAL_SECONDS = 3600.0;
        constexpr F64 FIXED_SIMULATION_HZ = 120.0;
        constexpr F64 FIXED_STEP_SECONDS = 1.0 / FIXED_SIMULATION_HZ;
        constexpr F64 FIXED_STEP_EPSILON = 1e-9;
        constexpr U64 VISUAL_HASH_SALT = 0xd1b54a32d192ed03ULL;

        F64 finiteOr(F64 value, F64 fallback)
        {
            return std::isfinite(value) ? value : fallback;
        }

        F64 clampFinite(F64 value, F64 fallback, F64 low, F64 high)
        {
            return std::clamp(finiteOr(value, fallback), low, high);
        }

        F64 qualityAfterglowStart(F64 flash_duration_seconds)
        {
            return std::min(0.12, flash_duration_seconds * 0.45);
        }

        U64 mixVisualKey(U64 value)
        {
            // SplitMix64 finalizer. This is a stateless visual lane: it must
            // never consume the scheduler/placement PRNG sequence.
            value ^= value >> 30;
            value *= 0xbf58476d1ce4e5b9ULL;
            value ^= value >> 27;
            value *= 0x94d049bb133111ebULL;
            return value ^ (value >> 31);
        }

        F64 unitFromVisualKey(U64 value)
        {
            return static_cast<F64>(mixVisualKey(value) >> 11) *
                   (1.0 / 9007199254740992.0);
        }
    }

    bool isFinite(const Position& position)
    {
        return std::isfinite(position.mX) &&
               std::isfinite(position.mY) &&
               std::isfinite(position.mZ);
    }

    Config sanitizeConfig(const Config& input)
    {
        Config output = input;
        output.mRatePerMinute = clampFinite(
            input.mRatePerMinute, 1.5, 0.0, MAX_RATE_PER_MINUTE);
        output.mFlashDurationSeconds = clampFinite(
            input.mFlashDurationSeconds, 0.35, 0.05, MAX_DURATION_SECONDS);
        output.mBoltDurationSeconds = clampFinite(
            input.mBoltDurationSeconds, 0.18, 0.02,
            output.mFlashDurationSeconds);
        output.mMinDistanceMeters = clampFinite(
            input.mMinDistanceMeters, 45.0, 0.0, MAX_DISTANCE_METERS);
        output.mMaxDistanceMeters = clampFinite(
            input.mMaxDistanceMeters, 130.0, output.mMinDistanceMeters,
            MAX_DISTANCE_METERS);
        output.mBoltHeightMeters = clampFinite(
            input.mBoltHeightMeters, 160.0, 16.0,
            MAX_BOLT_HEIGHT_METERS);
        output.mQualityAfterglowStrength = clampFinite(
            input.mQualityAfterglowStrength, 0.35, 0.0,
            MAX_QUALITY_AFTERGLOW_STRENGTH);
        if (output.mSeed == 0)
        {
            output.mSeed = FALLBACK_SEED;
        }
        return output;
    }

    void Controller::reset(U64 seed)
    {
        mInitialized = false;
        mWasEnabled = false;
        mConfiguredSeed = seed ? seed : FALLBACK_SEED;
        mRandomState = mConfiguredSeed;
        mLastTime = 0.0;
        mSimulationOrigin = 0.0;
        mSimulationTime = 0.0;
        mSimAccumulator = 0.0;
        mNextStrikeTime = 0.0;
        mPausedIntervalWork = 0.0;
        mScheduledRatePerMinute = 0.0;
        mStrikeStartTime = 0.0;
        mStrikeBase = Position();
        mStrikeDistanceMeters = 0.0;
        mBoltHeightMeters = 0.0;
        mStrikeId = 0;
        mHasStrike = false;
    }

    U64 Controller::nextRandomU64()
    {
        // xorshift64*: compact, deterministic across supported platforms, and
        // adequate for art-direction scheduling. Guard against the absorbing
        // zero state even if a corrupted save supplies a zero seed.
        if (mRandomState == 0)
        {
            mRandomState = FALLBACK_SEED;
        }
        mRandomState ^= mRandomState >> 12;
        mRandomState ^= mRandomState << 25;
        mRandomState ^= mRandomState >> 27;
        return mRandomState * 2685821657736338717ULL;
    }

    F64 Controller::nextUnit()
    {
        // Use the high 53 bits so conversion is exactly representable and the
        // result is in [0,1), never 1 (important for -log(1-u)).
        return static_cast<F64>(nextRandomU64() >> 11) *
               (1.0 / 9007199254740992.0);
    }

    F64 Controller::nextIntervalWork()
    {
        const F64 u = std::min(
            nextUnit(), 1.0 - std::numeric_limits<F64>::epsilon());
        // Rate-normalized exponential interval. Keeping this random work item
        // separate from its rate conversion lets live rate edits rescale the
        // outstanding schedule without consuming another PRNG value.
        return -std::log1p(-u) * 60.0;
    }

    void Controller::scheduleAfter(F64 now_seconds, const Config& config)
    {
        // Draw exactly once per scheduled strike, including while the rate is
        // zero. A later zero->nonzero edit can therefore resume this same work
        // item without perturbing the position RNG sequence.
        mPausedIntervalWork = nextIntervalWork();
        if (config.mRatePerMinute > 0.0)
        {
            const F64 interval = std::clamp(
                mPausedIntervalWork / config.mRatePerMinute,
                MIN_INTERVAL_SECONDS, MAX_INTERVAL_SECONDS);
            mNextStrikeTime = now_seconds + interval;
        }
        else
        {
            mNextStrikeTime = std::numeric_limits<F64>::infinity();
        }
        mScheduledRatePerMinute = config.mRatePerMinute;
    }

    void Controller::rescaleSchedule(F64 now_seconds,
                                     F64 new_rate_per_minute)
    {
        const F64 old_rate = mScheduledRatePerMinute;
        if (old_rate > 0.0 && new_rate_per_minute > 0.0)
        {
            const F64 remaining =
                std::max(0.0, mNextStrikeTime - now_seconds);
            mPausedIntervalWork = remaining * old_rate;
            mNextStrikeTime =
                now_seconds + remaining * (old_rate / new_rate_per_minute);
        }
        else if (old_rate > 0.0)
        {
            mPausedIntervalWork =
                std::max(0.0, mNextStrikeTime - now_seconds) * old_rate;
            mNextStrikeTime = std::numeric_limits<F64>::infinity();
        }
        else if (new_rate_per_minute > 0.0)
        {
            mNextStrikeTime =
                now_seconds + mPausedIntervalWork / new_rate_per_minute;
        }
        mScheduledRatePerMinute = new_rate_per_minute;
    }

    void Controller::beginStrike(F64 strike_time, const Config& config,
                                 const Position& anchor,
                                 bool carry_remainder)
    {
        const F64 schedule_base =
            carry_remainder && std::isfinite(mNextStrikeTime)
                ? mNextStrikeTime : strike_time;
        const F64 angle = nextUnit() * TWO_PI;
        // Area-uniform annulus distribution avoids visibly clustering strikes
        // near the minimum radius.
        const F64 min_squared =
            config.mMinDistanceMeters * config.mMinDistanceMeters;
        const F64 max_squared =
            config.mMaxDistanceMeters * config.mMaxDistanceMeters;
        const F64 radius =
            std::sqrt(min_squared + (max_squared - min_squared) * nextUnit());

        mStrikeBase.mX = anchor.mX + std::cos(angle) * radius;
        mStrikeBase.mY = anchor.mY + std::sin(angle) * radius;
        mStrikeBase.mZ = anchor.mZ;
        mStrikeDistanceMeters = radius;
        mBoltHeightMeters = config.mBoltHeightMeters;
        mStrikeStartTime = strike_time;
        mHasStrike = true;
        ++mStrikeId;

        // Carry schedule overshoot instead of biasing every following interval
        // by the render frame that happened to observe this strike.
        scheduleAfter(schedule_base, config);
    }

    void Controller::initializeTimeline(F64 now_seconds,
                                        const Config& config)
    {
        mWasEnabled = true;
        mHasStrike = false;
        mLastTime = now_seconds;
        mSimulationOrigin = now_seconds;
        mSimulationTime = now_seconds;
        mSimAccumulator = 0.0;
        scheduleAfter(now_seconds, config);
    }

    void Controller::advanceFixedClock(F64 now_seconds)
    {
        const F64 delta = now_seconds - mLastTime;
        mSimAccumulator += delta;

        // Advance an arbitrary number of fixed quanta in O(1). This deliberately
        // is not a catch-up loop: a debugger hitch can start at most one strike
        // in this update call.
        const F64 tick_count = std::floor(
            (mSimAccumulator + FIXED_STEP_EPSILON) / FIXED_STEP_SECONDS);
        if (tick_count >= 1.0)
        {
            mSimulationTime += tick_count * FIXED_STEP_SECONDS;
            mSimAccumulator -= tick_count * FIXED_STEP_SECONDS;
            if (mSimAccumulator < 0.0 &&
                mSimAccumulator > -FIXED_STEP_EPSILON)
            {
                mSimAccumulator = 0.0;
            }
        }
        mLastTime = now_seconds;
    }

    F64 Controller::fixedTickAtOrAfter(F64 time_seconds) const
    {
        const F64 tick = std::ceil(
            (time_seconds - mSimulationOrigin) / FIXED_STEP_SECONDS -
            FIXED_STEP_EPSILON);
        return mSimulationOrigin + std::max(0.0, tick) * FIXED_STEP_SECONDS;
    }

    F64 Controller::interpolatedTime() const
    {
        // Pulse presentation is sampled between the two surrounding fixed
        // ticks. F64 remainder retention makes regrouped render-frame deltas
        // land on the same completed simulation tick.
        return mSimulationTime + mSimAccumulator;
    }

    F32 Controller::flashPulse(F64 age_seconds, F64 duration_seconds)
    {
        if (!(age_seconds >= 0.0) || age_seconds >= duration_seconds ||
            !std::isfinite(age_seconds) || !std::isfinite(duration_seconds))
        {
            return 0.f;
        }

        // Fast primary discharge and two weaker return strokes. max(), rather
        // than sum(), keeps the HDR control bounded and artist-predictable.
        const F64 primary = std::exp(-age_seconds * 18.0);
        const F64 echo_a = age_seconds >= 0.055
            ? 0.72 * std::exp(-(age_seconds - 0.055) * 24.0) : 0.0;
        const F64 echo_b = age_seconds >= 0.120
            ? 0.42 * std::exp(-(age_seconds - 0.120) * 19.0) : 0.0;
        return static_cast<F32>(
            std::clamp(std::max({ primary, echo_a, echo_b }), 0.0, 1.0));
    }

    F32 Controller::boltPulse(F64 age_seconds, F64 duration_seconds)
    {
        if (!(age_seconds >= 0.0) || age_seconds >= duration_seconds ||
            !std::isfinite(age_seconds) || !std::isfinite(duration_seconds))
        {
            return 0.f;
        }
        const F64 normalized = age_seconds / duration_seconds;
        const F64 envelope = (1.0 - normalized) * (1.0 - normalized);
        const F64 flicker = 0.78 + 0.22 * std::fabs(std::sin(age_seconds * 173.0));
        return static_cast<F32>(std::clamp(envelope * flicker, 0.0, 1.0));
    }

    F32 Controller::qualityBoltPulse(F64 age_seconds, F64 duration_seconds,
                                     U64 strike_key, U32& stroke_index)
    {
        stroke_index = 0;
        if (!(age_seconds >= 0.0) || age_seconds >= duration_seconds ||
            !(duration_seconds > 0.0) ||
            !std::isfinite(age_seconds) || !std::isfinite(duration_seconds))
        {
            return 0.f;
        }

        // Four return-stroke lanes stay in ordered absolute-time windows. Their
        // jitter is keyed by the event rather than drawn from the controller
        // PRNG, so enabling quality cannot change any future strike time or
        // position. Absolute millisecond windows keep a long artist-selected
        // bolt duration from stretching return strokes into multi-second gaps.
        F64 starts[4] = {
            0.0,
            0.050 + (unitFromVisualKey(strike_key + 1ULL) - 0.5) * 0.012,
            0.108 + (unitFromVisualKey(strike_key + 2ULL) - 0.5) * 0.018,
            0.164 + (unitFromVisualKey(strike_key + 3ULL) - 0.5) * 0.022
        };
        const F64 amplitudes[4] = { 1.0, 0.82, 0.60, 0.38 };
        const F64 decay_seconds[4] = { 0.012, 0.014, 0.017, 0.020 };
        F64 pulse = static_cast<F64>(
            boltPulse(age_seconds, duration_seconds)) * 0.38;
        for (U32 stroke = 0; stroke < 4; ++stroke)
        {
            if (age_seconds >= starts[stroke])
            {
                stroke_index = stroke;
                const F64 stroke_age =
                    age_seconds - starts[stroke];
                const F64 stroke_pulse =
                    amplitudes[stroke] *
                    std::exp(-stroke_age / decay_seconds[stroke]);
                pulse = std::max(pulse, stroke_pulse);
            }
        }

        // Even a jittered late return stroke must approach the artist-selected
        // bolt cutoff continuously. Use at most the final 20 ms; for the
        // minimum 20 ms duration the whole pulse becomes its smooth envelope.
        const F64 terminal_fade_seconds =
            std::min(duration_seconds, 0.020);
        const F64 terminal_t = std::clamp(
            (duration_seconds - age_seconds) / terminal_fade_seconds,
            0.0, 1.0);
        const F64 terminal_envelope =
            terminal_t * terminal_t * (3.0 - 2.0 * terminal_t);
        return static_cast<F32>(
            std::clamp(pulse * terminal_envelope, 0.0, 1.0));
    }

    F32 Controller::afterglowPulse(F64 age_seconds,
                                   F64 flash_duration_seconds,
                                   F64 strength)
    {
        if (!(age_seconds >= 0.0) || !(flash_duration_seconds > 0.0) ||
            !(strength > 0.0) ||
            age_seconds >= flash_duration_seconds + QUALITY_AFTERGLOW_SECONDS ||
            !std::isfinite(age_seconds) ||
            !std::isfinite(flash_duration_seconds) ||
            !std::isfinite(strength))
        {
            return 0.f;
        }

        const F64 start = qualityAfterglowStart(flash_duration_seconds);
        if (age_seconds < start ||
            age_seconds >= start + QUALITY_AFTERGLOW_SECONDS)
        {
            return 0.f;
        }
        const F64 elapsed = age_seconds - start;
        const F64 remaining =
            start + QUALITY_AFTERGLOW_SECONDS - age_seconds;
        const F64 rise_t = std::clamp(elapsed / 0.025, 0.0, 1.0);
        const F64 end_t = std::clamp(remaining / 0.050, 0.0, 1.0);
        const F64 rise = rise_t * rise_t * (3.0 - 2.0 * rise_t);
        const F64 end_fade = end_t * end_t * (3.0 - 2.0 * end_t);
        return static_cast<F32>(std::clamp(
            strength * rise * end_fade * std::exp(-elapsed * 8.0),
            0.0, 1.0));
    }

    Frame Controller::makeFrame(F64 now_seconds, const Config& config,
                                bool strike_started) const
    {
        Frame frame;
        if (!mHasStrike || !std::isfinite(now_seconds))
        {
            return frame;
        }

        const F64 age = now_seconds - mStrikeStartTime;
        frame.mFlash = flashPulse(age, config.mFlashDurationSeconds);
        frame.mBolt = boltPulse(age, config.mBoltDurationSeconds);
        if (config.mQualityEnabled)
        {
            const U64 strike_key = mixVisualKey(
                mConfiguredSeed ^
                (mStrikeId * 0x9e3779b97f4a7c15ULL) ^
                VISUAL_HASH_SALT);
            frame.mQualityBolt = qualityBoltPulse(
                age, config.mBoltDurationSeconds,
                strike_key, frame.mStrokeIndex);
            frame.mAfterglow = afterglowPulse(
                age, config.mFlashDurationSeconds,
                config.mQualityAfterglowStrength);
            frame.mColorVariation = static_cast<F32>(
                unitFromVisualKey(strike_key + 0x632be59bd9b4e019ULL) *
                    2.0 - 1.0);
            frame.mVisualSeed = static_cast<U32>(
                mixVisualKey(strike_key + 0x8cb92baa3f3d8dd7ULL) & 0xffffULL);
        }
        frame.mActive = frame.mFlash > 0.f || frame.mBolt > 0.f ||
                        frame.mAfterglow > 0.f;
        frame.mStrikeStarted = strike_started && frame.mActive;
        frame.mStrikeTime = mStrikeStartTime;
        frame.mStrikeBase = mStrikeBase;
        frame.mStrikeDistanceMeters = mStrikeDistanceMeters;
        frame.mBoltHeightMeters = mBoltHeightMeters;
        frame.mStrikeId = mStrikeId;
        return frame;
    }

    bool Controller::trigger(F64 now_seconds, const Config& raw_config,
                             const Position& anchor)
    {
        const Config config = sanitizeConfig(raw_config);
        if (!config.mEnabled || !std::isfinite(now_seconds) ||
            now_seconds < 0.0 ||
            !isFinite(anchor))
        {
            return false;
        }

        if (!mInitialized || mConfiguredSeed != config.mSeed)
        {
            reset(config.mSeed);
            mInitialized = true;
            mConfiguredSeed = config.mSeed;
            mRandomState = config.mSeed;
        }

        mWasEnabled = true;
        mLastTime = now_seconds;
        mSimulationOrigin = now_seconds;
        mSimulationTime = now_seconds;
        mSimAccumulator = 0.0;
        beginStrike(now_seconds, config, anchor, false);
        return true;
    }

    Frame Controller::update(F64 now_seconds, const Config& raw_config,
                             const Position& anchor)
    {
        const Config config = sanitizeConfig(raw_config);
        if (!std::isfinite(now_seconds) || now_seconds < 0.0 ||
            !isFinite(anchor))
        {
            // Invalid caller input must not poison a valid schedule/state.
            return Frame();
        }

        if (!mInitialized || mConfiguredSeed != config.mSeed)
        {
            reset(config.mSeed);
            mInitialized = true;
            mConfiguredSeed = config.mSeed;
            mRandomState = config.mSeed;
        }

        if (!config.mEnabled)
        {
            mWasEnabled = false;
            mHasStrike = false;
            mLastTime = now_seconds;
            mSimulationOrigin = now_seconds;
            mSimulationTime = now_seconds;
            mSimAccumulator = 0.0;
            mNextStrikeTime = std::numeric_limits<F64>::infinity();
            return Frame();
        }

        const bool large_hitch =
            mWasEnabled && now_seconds >= mLastTime &&
            now_seconds - mLastTime > 0.25;

        // Presentation-time mode changes and seeks may move backwards. Discard
        // the old time-domain schedule instead of generating negative ages or a
        // burst of catch-up strikes.
        if (mWasEnabled && now_seconds < mLastTime)
        {
            initializeTimeline(now_seconds, config);
        }

        if (!mWasEnabled)
        {
            initializeTimeline(now_seconds, config);
        }
        else
        {
            advanceFixedClock(now_seconds);
        }
        // EEP cloud interpolation can perturb the effective rate every frame.
        // Reschedule on a real edit (or zero crossing), not on tiny animation
        // noise that would otherwise postpone the next strike forever.
        const bool rate_zero_changed =
            (config.mRatePerMinute == 0.0) !=
            (mScheduledRatePerMinute == 0.0);
        const F64 rate_threshold =
            std::max(0.05, std::fabs(mScheduledRatePerMinute) * 0.05);
        if (rate_zero_changed ||
            std::fabs(config.mRatePerMinute - mScheduledRatePerMinute) > rate_threshold)
        {
            rescaleSchedule(now_seconds, config.mRatePerMinute);
        }

        bool started = false;
        if (config.mRatePerMinute > 0.0 &&
            mSimulationTime + FIXED_STEP_EPSILON >= mNextStrikeTime)
        {
            // Deliberately one strike, not a while-loop: a ten-minute debugger
            // pause must not detonate hundreds of deferred events in one frame.
            const F64 due_tick = fixedTickAtOrAfter(mNextStrikeTime);
            beginStrike(large_hitch ? mSimulationTime : due_tick,
                        config, anchor, true);
            started = true;
        }

        const F64 sample_time = interpolatedTime();
        Frame frame = makeFrame(sample_time, config, started);
        F64 visible_duration = std::max(
            config.mFlashDurationSeconds, config.mBoltDurationSeconds);
        if (config.mQualityEnabled &&
            config.mQualityAfterglowStrength > 0.0)
        {
            visible_duration = std::max(
                visible_duration,
                qualityAfterglowStart(config.mFlashDurationSeconds) +
                    QUALITY_AFTERGLOW_SECONDS);
        }
        if (!frame.mActive && mHasStrike &&
            sample_time - mStrikeStartTime >= visible_duration)
        {
            mHasStrike = false;
        }
        return frame;
    }
}
