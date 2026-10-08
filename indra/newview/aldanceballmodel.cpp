/**
 * @file aldanceballmodel.cpp
 * @brief Dependency-free timing and playlist model for Dance Ball.
 */

#include "linden_common.h"

#include "aldanceballmodel.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ALDanceBallModel
{
    namespace
    {
        constexpr U64 PHASE_SALT = 0x243f6a8885a308d3ULL;
        constexpr U64 SPEED_SALT = 0x13198a2e03707344ULL;
        constexpr U64 PICK_SALT = 0xa4093822299f31d0ULL;
        constexpr U64 JITTER_SALT = 0x082efa98ec4e6c89ULL;
        constexpr F64 MIN_INTERVAL = 0.001;
        // Floor for the lane rate used to bound a phase slew, so a nearly
        // stopped lane cannot demand an unbounded slew duration.
        constexpr F64 MIN_SLEW_LANE_RATE = 0.05;

        U64 splitMix64(U64 value)
        {
            value += 0x9e3779b97f4a7c15ULL;
            value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
            value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
            return value ^ (value >> 31);
        }

        U64 lane(U64 seed, U64 salt, U64 key, U64 generation)
        {
            return splitMix64(seed ^ salt ^ splitMix64(key) ^
                              splitMix64(generation));
        }

        F64 unit(U64 value)
        {
            return static_cast<F64>(value >> 11) *
                   (1.0 / 9007199254740992.0);
        }

        F64 finiteNonNegative(F64 value, F64 fallback = 0.0)
        {
            return std::isfinite(value) && value >= 0.0 ? value : fallback;
        }

        F64 clamp01(F64 value)
        {
            return std::clamp(std::isfinite(value) ? value : 0.0, 0.0, 1.0);
        }

        F64 rampIntegral(RampCurve curve, F64 x)
        {
            // Integral from 0..x of the normalized interpolation curve.
            if (curve == RampCurve::LINEAR)
            {
                return 0.5 * x * x;
            }
            return x * x * x - 0.5 * x * x * x * x;
        }

        F64 rampValue(RampCurve curve, F64 x)
        {
            if (curve == RampCurve::LINEAR)
            {
                return x;
            }
            return x * x * (3.0 - 2.0 * x);
        }
    }

    void Clock::reset(F64 time, F64 speed)
    {
        mTime = finiteNonNegative(time);
        mSpeed = finiteNonNegative(speed, 1.0);
        mRampStart = mRampTarget = mSpeed;
        mRampDuration = mRampElapsed = 0.0;
        mFrozen = false;
        mRamping = false;
    }

    F64 Clock::advance(F64 delta_seconds)
    {
        const F64 delta = finiteNonNegative(delta_seconds);
        if (mFrozen || delta == 0.0)
        {
            return mTime;
        }

        F64 remaining = delta;
        if (mRamping)
        {
            const F64 used = std::min(remaining, mRampDuration - mRampElapsed);
            const F64 x0 = mRampElapsed / mRampDuration;
            const F64 x1 = (mRampElapsed + used) / mRampDuration;
            const F64 shape_area = mRampDuration *
                (rampIntegral(mRampCurve, x1) - rampIntegral(mRampCurve, x0));
            mTime += used * mRampStart +
                     (mRampTarget - mRampStart) * shape_area;
            mRampElapsed += used;
            remaining -= used;
            mSpeed = mRampStart + (mRampTarget - mRampStart) *
                rampValue(mRampCurve, x1);
            if (mRampElapsed >= mRampDuration)
            {
                mSpeed = mRampTarget;
                mRamping = false;
            }
        }
        mTime += remaining * mSpeed;
        return mTime;
    }

    void Clock::setSpeed(F64 speed)
    {
        mSpeed = finiteNonNegative(speed, 1.0);
        mRampStart = mRampTarget = mSpeed;
        mRampDuration = mRampElapsed = 0.0;
        mRamping = false;
    }

    void Clock::rampTo(F64 target_speed, F64 duration_seconds, RampCurve curve)
    {
        const F64 target = finiteNonNegative(target_speed, mSpeed);
        const F64 duration = finiteNonNegative(duration_seconds);
        if (duration == 0.0)
        {
            setSpeed(target);
            return;
        }
        mRampStart = mSpeed;
        mRampTarget = target;
        mRampDuration = duration;
        mRampElapsed = 0.0;
        mRampCurve = curve;
        mRamping = true;
    }

    F64 Clock::rampProgress() const
    {
        return mRamping && mRampDuration > 0.0
            ? clamp01(mRampElapsed / mRampDuration) : 1.0;
    }

    F64 phaseOffset(U64 dancer_key, F64 desync, U64 seed, U64 reshuffle)
    {
        return unit(lane(seed, PHASE_SALT, dancer_key, reshuffle)) *
               clamp01(desync);
    }

    F64 speedMultiplier(U64 dancer_key, F64 jitter_fraction, U64 seed,
                        U64 reshuffle)
    {
        const F64 jitter = clamp01(jitter_fraction);
        return std::max(0.0, 1.0 +
            (unit(lane(seed, SPEED_SALT, dancer_key, reshuffle)) * 2.0 - 1.0) *
            jitter);
    }

    F64 effectiveSpeedMultiplier(U64 dancer_key, PlaylistMode mode,
                                 F64 desync, F64 jitter_fraction, U64 seed,
                                 U64 reshuffle)
    {
        if (mode == PlaylistMode::SAME_FOR_ALL && clamp01(desync) == 0.0)
        {
            return 1.0;
        }
        return speedMultiplier(dancer_key, jitter_fraction, seed, reshuffle);
    }

    void SampleClock::reset(F64 sample_time, F64 rate, F64 phase_seconds)
    {
        mTime = finiteNonNegative(sample_time);
        mRate = finiteNonNegative(rate, 1.0);
        mPhase = std::isfinite(phase_seconds) ? phase_seconds : 0.0;
        mRateStart = mRateTarget = mRate;
        mPhaseStart = mPhaseTarget = mPhase;
        mSlewDuration = mSlewElapsed = 0.0;
        mBoundedSlew = false;
    }

    F64 phaseSlewDuration(F64 crossfade_seconds, F64 phase_error,
                          F64 lane_rate)
    {
        const F64 crossfade = finiteNonNegative(crossfade_seconds);
        if (crossfade <= 0.0)
        {
            return 0.0;
        }
        const F64 error = std::isfinite(phase_error)
            ? std::fabs(phase_error) : 0.0;
        // The correction is measured against the lane's own playback rate, so
        // a slow lane gets a proportionally gentler (longer) correction.
        const F64 rate = std::max(finiteNonNegative(lane_rate, 1.0),
                                  MIN_SLEW_LANE_RATE);
        return std::max(crossfade, error / (MAX_PHASE_SLEW_RATE * rate));
    }

    void SampleClock::retarget(F64 rate, F64 phase_seconds,
                               F64 slew_seconds, F64 loop_cycle)
    {
        const F64 target_rate = finiteNonNegative(rate, mRate);
        F64 target_phase = std::isfinite(phase_seconds)
            ? phase_seconds : mPhase;
        const bool looped = finiteNonNegative(loop_cycle) > 0.0;
        F64 duration = finiteNonNegative(slew_seconds);
        bool bounded = false;
        if (looped)
        {
            // A request that is congruent to the current target modulo the
            // loop is not a change (the target may have been stored
            // unwrapped); otherwise take the shortest way around the loop
            // from where the phase is now and bound the slew rate.
            constexpr F64 SAME_PHASE_EPSILON = 1e-9;
            if (target_rate == mRateTarget &&
                std::fabs(nearestLoopError(target_phase - mPhaseTarget,
                                           loop_cycle)) <= SAME_PHASE_EPSILON)
            {
                return;
            }
            target_phase = mPhase +
                nearestLoopError(target_phase - mPhase, loop_cycle);
            // The lane rate sweeps linearly from the current to the target
            // rate during the slew; bounding the correction by the slower end
            // keeps the effective rate within [0.5x, 1.5x] of the lane rate
            // at every point.
            duration = phaseSlewDuration(duration, target_phase - mPhase,
                                         std::min(mRate, target_rate));
            bounded = true;
        }
        else if (target_rate == mRateTarget && target_phase == mPhaseTarget)
        {
            return;
        }
        if (duration == 0.0)
        {
            mTime += target_phase - mPhase;
            mRate = mRateStart = mRateTarget = target_rate;
            mPhase = mPhaseStart = mPhaseTarget = target_phase;
            mSlewDuration = mSlewElapsed = 0.0;
            mBoundedSlew = false;
            return;
        }
        mRateStart = mRate;
        mRateTarget = target_rate;
        mPhaseStart = mPhase;
        mPhaseTarget = target_phase;
        mSlewDuration = duration;
        mSlewElapsed = 0.0;
        mBoundedSlew = bounded;
    }

    F64 SampleClock::advance(F64 transport_delta, F64 slew_delta)
    {
        F64 remaining_transport = finiteNonNegative(transport_delta);
        F64 remaining_slew = finiteNonNegative(slew_delta);
        if (remaining_transport == 0.0 && remaining_slew == 0.0)
        {
            return mTime;
        }
        if (remaining_slew > 0.0 && mSlewDuration > 0.0 &&
            mSlewElapsed < mSlewDuration)
        {
            const F64 used_slew = std::min(remaining_slew,
                mSlewDuration - mSlewElapsed);
            const F64 fraction = used_slew / remaining_slew;
            const F64 used_transport = remaining_transport * fraction;
            const F64 x0 = mSlewElapsed / mSlewDuration;
            const F64 x1 = (mSlewElapsed + used_slew) / mSlewDuration;
            const F64 rate0 = mRateStart + (mRateTarget - mRateStart) * x0;
            const F64 rate1 = mRateStart + (mRateTarget - mRateStart) * x1;
            const F64 phase0 = mPhaseStart +
                (mPhaseTarget - mPhaseStart) * x0;
            const F64 phase1 = mPhaseStart +
                (mPhaseTarget - mPhaseStart) * x1;
            F64 chunk = used_transport * (rate0 + rate1) * 0.5 + phase1 - phase0;
            if (mBoundedSlew)
            {
                // Hard guarantee for a bounded (looped) slew: the lane never
                // plays backward, even when the lane rate sits below the
                // MIN_SLEW_LANE_RATE floor used to size the slew.
                chunk = std::max(0.0, chunk);
            }
            mTime += chunk;
            mRate = rate1;
            mPhase = phase1;
            mSlewElapsed += used_slew;
            remaining_transport -= used_transport;
            remaining_slew -= used_slew;
            if (mSlewElapsed >= mSlewDuration)
            {
                mRate = mRateTarget;
                mPhase = mPhaseTarget;
                mSlewDuration = mSlewElapsed = 0.0;
            }
        }
        mTime += remaining_transport * mRate;
        return mTime;
    }

    void retargetLegWeights(std::vector<LegWeight>& legs,
                            std::size_t incoming)
    {
        if (incoming >= legs.size()) return;
        F64 live_sum = 0.0;
        for (std::size_t i = 0; i < legs.size(); ++i)
        {
            if (legs[i].mStarted)
            {
                live_sum += std::max(0.0, legs[i].mCurrent);
            }
        }
        // Never scale up: a partial live sum (mid-fade) must not jump to full
        // weight at the retarget.
        const F64 scale = live_sum > 1.0 ? 1.0 / live_sum : 1.0;
        for (std::size_t i = 0; i < legs.size(); ++i)
        {
            LegWeight& leg = legs[i];
            leg.mCurrent = leg.mStart = leg.mStarted
                ? std::max(0.0, leg.mCurrent) * scale : 0.0;
            leg.mTarget = i == incoming ? 1.0 : 0.0;
        }
    }

    void advanceLegWeights(std::vector<LegWeight>& legs, F64 progress)
    {
        const F64 t = clamp01(progress);
        for (LegWeight& leg : legs)
        {
            leg.mCurrent = leg.mStart + (leg.mTarget - leg.mStart) * t;
        }
    }

    F64 slewToSharedSample(F64 shared_sample, F64 initial_error,
                           F64 progress)
    {
        const F64 shared = finiteNonNegative(shared_sample);
        const F64 error = std::isfinite(initial_error) ? initial_error : 0.0;
        return shared + error * (1.0 - clamp01(progress));
    }

    F64 nearestLoopError(F64 error, F64 cycle)
    {
        if (!std::isfinite(error)) return 0.0;
        const F64 span = finiteNonNegative(cycle);
        return span > 0.0 ? std::remainder(error, span) : error;
    }

    F64 waveDelay(F64 distance, F64 farthest_distance, F64 spread_speed,
                  WaveDirection direction)
    {
        const F64 speed = finiteNonNegative(spread_speed);
        if (speed <= 0.0)
        {
            return 0.0;
        }
        const F64 d = finiteNonNegative(distance);
        const F64 farthest = finiteNonNegative(farthest_distance);
        const F64 travel = direction == WaveDirection::OUTWARD
            ? d : std::max(0.0, farthest - d);
        return travel / speed;
    }

    bool waveApplies(WaveApply apply, bool is_switch)
    {
        return apply == WaveApply::START_AND_SWITCH ||
               (is_switch && apply == WaveApply::SWITCH_ONLY) ||
               (!is_switch && apply == WaveApply::START_ONLY);
    }

    Sample mapSample(const Clip& input, F64 elapsed_seconds,
                     F64 speed, F64 phase, F64 start_delay)
    {
        Sample result;
        const F64 elapsed = finiteNonNegative(elapsed_seconds);
        const F64 delay = finiteNonNegative(start_delay);
        if (elapsed < delay)
        {
            result.mStarted = false;
            return result;
        }
        const F64 local = elapsed - delay;
        const F64 duration = finiteNonNegative(input.mDuration);
        if (!input.mLoop)
        {
            result.mTime = duration > 0.0 ? std::min(local, duration) : 0.0;
            result.mEnded = duration <= 0.0 || local >= duration;
            return result;
        }

        F64 loop_in = finiteNonNegative(input.mLoopIn);
        F64 loop_out = finiteNonNegative(input.mLoopOut);
        if (duration > 0.0)
        {
            loop_in = std::min(loop_in, duration);
            loop_out = std::min(loop_out, duration);
        }
        if (loop_out <= loop_in)
        {
            // Match stock play-through semantics when authoring supplies no
            // usable loop span instead of jumping straight to loop-in.
            result.mTime = duration > 0.0 ? std::min(local, duration) : 0.0;
            return result;
        }
        const F64 rate = finiteNonNegative(speed, 1.0);
        const F64 cycle = loop_out - loop_in;
        F64 wrapped = std::fmod(local * rate + clamp01(phase) * cycle, cycle);
        if (wrapped < 0.0 || !std::isfinite(wrapped))
        {
            wrapped = 0.0;
        }
        result.mTime = loop_in + wrapped;
        return result;
    }

    U64 Scheduler::stateKey(U64 dancer_key, PlaylistMode mode)
    {
        return mode == PlaylistMode::SAME_FOR_ALL ? 0ULL : dancer_key;
    }

    bool Scheduler::validTime(F64 value)
    {
        return std::isfinite(value) && value >= 0.0;
    }

    bool Scheduler::sameConfig(const SchedulerConfig& lhs,
                               const SchedulerConfig& rhs)
    {
        return lhs.mMode == rhs.mMode &&
               lhs.mAutoAdvance == rhs.mAutoAdvance &&
               lhs.mIntervalSeconds == rhs.mIntervalSeconds &&
               lhs.mJitterSeconds == rhs.mJitterSeconds &&
               lhs.mSeed == rhs.mSeed;
    }

    S32 Scheduler::stepAvailable(S32 from, S32 direction,
                                 const std::vector<PlaylistItem>& items)
    {
        if (items.empty())
        {
            return -1;
        }
        const S32 count = static_cast<S32>(items.size());
        S32 cursor = from;
        if (cursor < 0)
        {
            cursor = direction > 0 ? -1 : 0;
        }
        for (S32 i = 0; i < count; ++i)
        {
            cursor = (cursor + direction + count) % count;
            if (items[static_cast<std::size_t>(cursor)].mAvailable)
            {
                return cursor;
            }
        }
        return -1;
    }

    S32 Scheduler::remapItem(S32 item,
                             const std::vector<S32>& old_to_new)
    {
        if (item < 0 || old_to_new.empty()) return -1;
        const std::size_t count = old_to_new.size();
        for (std::size_t offset = 0; offset < count; ++offset)
        {
            const std::size_t old_index =
                (static_cast<std::size_t>(item) + offset) % count;
            if (old_to_new[old_index] >= 0)
            {
                return old_to_new[old_index];
            }
        }
        return -1;
    }

    S32 Scheduler::randomAvailable(S32 current, U64 key, U64 event,
                                   const std::vector<PlaylistItem>& items,
                                   U64 seed)
    {
        const std::size_t available_count = static_cast<std::size_t>(
            std::count_if(items.begin(), items.end(),
                [](const PlaylistItem& item) { return item.mAvailable; }));
        std::vector<S32> candidates;
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            if (items[i].mAvailable &&
                (available_count <= 1 || static_cast<S32>(i) != current))
            {
                candidates.push_back(static_cast<S32>(i));
            }
        }
        if (candidates.empty()) return -1;
        return candidates[static_cast<std::size_t>(
            lane(seed, PICK_SALT, key, event) % candidates.size())];
    }

    S32 Scheduler::shuffleAtEvent(U64 key, U64 event,
                                  const std::vector<PlaylistItem>& items,
                                  U64 seed)
    {
        std::vector<S32> candidates;
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            if (items[i].mAvailable) candidates.push_back(static_cast<S32>(i));
        }
        if (candidates.empty()) return -1;
        const U64 base = lane(seed, PICK_SALT, key, 0);
        const U64 offset = event == ~0ULL ? 0ULL : event + 1ULL;
        return candidates[static_cast<std::size_t>(
            (base + offset) % candidates.size())];
    }

    F64 Scheduler::boundary(const State& state, U64 event,
                            const SchedulerConfig& input)
    {
        const F64 interval = std::max(MIN_INTERVAL,
            finiteNonNegative(input.mIntervalSeconds, 8.0));
        const F64 jitter = std::min(finiteNonNegative(input.mJitterSeconds),
                                    interval * 0.49);
        const U64 pair = event / 2ULL;
        const F64 pair_start = state.mAnchor + static_cast<F64>(pair) *
                               2.0 * interval;
        if ((event & 1ULL) != 0ULL)
        {
            return pair_start + 2.0 * interval;
        }
        const F64 signed_unit = unit(lane(input.mSeed, JITTER_SALT,
                                          state.mKey, pair)) *
                                2.0 - 1.0;
        return pair_start + interval + signed_unit * jitter;
    }

    Selection Scheduler::update(F64 now_seconds, U64 dancer_key,
                                const std::vector<PlaylistItem>& items,
                                const SchedulerConfig& config)
    {
        Selection result;
        if (!validTime(now_seconds))
        {
            return result;
        }
        reconfigure(now_seconds, dancer_key, config);
        const U64 key = stateKey(dancer_key, config.mMode);
        State& state = mStates[key];
        if (!state.mInitialized)
        {
            if (mHaveSeedState)
            {
                state = mSeedState;
                state.mKey = key;
                state.mAnchor = now_seconds;
                state.mHaveProcessedEvent = false;
            }
            else
            {
                state.mInitialized = true;
                state.mKey = key;
                state.mAnchor = now_seconds;
                state.mItemStart = now_seconds;
                state.mCurrent = config.mMode == PlaylistMode::SAME_FOR_ALL
                    ? stepAvailable(-1, 1, items)
                    : shuffleAtEvent(key, ~0ULL, items, config.mSeed);
                result.mChanged = state.mCurrent >= 0;
            }
            result.mItem = state.mCurrent;
            result.mBoundary = state.mItemStart;
            return result;
        }
        if (state.mCurrent < 0 ||
            static_cast<std::size_t>(state.mCurrent) >= items.size() ||
            !items[static_cast<std::size_t>(state.mCurrent)].mAvailable)
        {
            result.mPrevious = state.mCurrent;
            state.mCurrent = stepAvailable(state.mCurrent, 1, items);
            result.mItem = state.mCurrent;
            result.mChanged = result.mItem != result.mPrevious;
            state.mAnchor = now_seconds;
            state.mItemStart = now_seconds;
            state.mHaveProcessedEvent = false;
            result.mBoundary = state.mItemStart;
            return result;
        }
        result.mItem = state.mCurrent;
        result.mBoundary = state.mItemStart;
        if (!config.mAutoAdvance || items.empty())
        {
            return result;
        }

        // Pair-balanced jitter bounds the final due event to the nominal event
        // and its immediate neighbours, avoiding an unbounded hitch replay.
        const F64 interval = std::max(MIN_INTERVAL,
            finiteNonNegative(config.mIntervalSeconds, 8.0));
        const F64 elapsed = std::max(0.0, now_seconds - state.mAnchor);
        const U64 nominal = static_cast<U64>(std::min<F64>(
            std::floor(elapsed / interval),
            static_cast<F64>((1ULL << 52) - 2ULL)));
        bool found = false;
        U64 final_event = 0;
        const U64 first = nominal > 1 ? nominal - 1 : 0;
        const U64 last = nominal + 1;
        for (U64 event = first; event <= last; ++event)
        {
            if (boundary(state, event, config) <= now_seconds)
            {
                final_event = event;
                found = true;
            }
        }
        if (!found || (state.mHaveProcessedEvent &&
                       final_event <= state.mProcessedEvent))
        {
            return result;
        }

        result.mPrevious = state.mCurrent;
        const U64 elapsed_events = state.mHaveProcessedEvent
            ? final_event - state.mProcessedEvent
            : final_event + 1ULL;
        if (config.mMode == PlaylistMode::SHUFFLE_EACH_DANCER)
        {
            state.mCurrent = shuffleAtEvent(key, final_event,
                                            items, config.mSeed);
        }
        else
        {
            U64 available_count = 0;
            for (const PlaylistItem& item : items)
            {
                available_count += item.mAvailable ? 1ULL : 0ULL;
            }
            U64 steps = available_count > 0
                ? elapsed_events % available_count : 0;
            while (steps-- > 0)
            {
                state.mCurrent = stepAvailable(state.mCurrent, 1, items);
            }
        }
        state.mHaveProcessedEvent = true;
        state.mProcessedEvent = final_event;
        result.mItem = state.mCurrent;
        result.mChanged = result.mItem >= 0 && result.mItem != result.mPrevious;
        if (result.mChanged)
        {
            state.mItemStart = boundary(state, final_event, config);
        }
        result.mBoundary = state.mItemStart;
        result.mEventIndex = final_event;
        return result;
    }

    Selection Scheduler::manual(F64 now_seconds, U64 dancer_key,
                                const std::vector<PlaylistItem>& items,
                                const SchedulerConfig& config, S32 direction)
    {
        Selection result;
        if (!validTime(now_seconds))
        {
            return result;
        }
        reconfigure(now_seconds, dancer_key, config);
        State& state = mStates[stateKey(dancer_key, config.mMode)];
        state.mKey = stateKey(dancer_key, config.mMode);
        if (!state.mInitialized && mHaveSeedState)
        {
            const U64 key = state.mKey;
            state = mSeedState;
            state.mKey = key;
        }
        result.mPrevious = state.mCurrent;
        state.mCurrent = config.mMode == PlaylistMode::SHUFFLE_EACH_DANCER
            ? randomAvailable(state.mCurrent, state.mKey,
                              ++state.mManualEvent, items, config.mSeed)
            : stepAvailable(state.mCurrent, direction, items);
        state.mInitialized = true;
        state.mAnchor = now_seconds;
        if (state.mCurrent != result.mPrevious)
        {
            state.mItemStart = now_seconds;
        }
        state.mHaveProcessedEvent = false;
        result.mItem = state.mCurrent;
        result.mChanged = result.mItem >= 0 && result.mItem != result.mPrevious;
        result.mBoundary = state.mItemStart;
        return result;
    }

    Selection Scheduler::next(F64 now_seconds, U64 dancer_key,
                              const std::vector<PlaylistItem>& items,
                              const SchedulerConfig& config)
    {
        return manual(now_seconds, dancer_key, items, config, 1);
    }

    Selection Scheduler::previous(F64 now_seconds, U64 dancer_key,
                                  const std::vector<PlaylistItem>& items,
                                  const SchedulerConfig& config)
    {
        return manual(now_seconds, dancer_key, items, config, -1);
    }

    Selection Scheduler::select(F64 now_seconds, U64 dancer_key, S32 item,
                                const std::vector<PlaylistItem>& items,
                                const SchedulerConfig& config)
    {
        Selection result;
        if (!validTime(now_seconds) || item < 0 ||
            static_cast<std::size_t>(item) >= items.size() ||
            !items[static_cast<std::size_t>(item)].mAvailable)
        {
            return result;
        }
        reconfigure(now_seconds, dancer_key, config);
        const U64 key = stateKey(dancer_key, config.mMode);
        State& state = mStates[key];
        if (!state.mInitialized && mHaveSeedState)
        {
            state = mSeedState;
            state.mKey = key;
        }
        state.mKey = key;
        result.mPrevious = state.mCurrent;
        state.mCurrent = item;
        state.mInitialized = true;
        state.mAnchor = now_seconds;
        if (item != result.mPrevious)
        {
            state.mItemStart = now_seconds;
        }
        state.mHaveProcessedEvent = false;
        result.mItem = item;
        result.mChanged = item != result.mPrevious;
        result.mBoundary = state.mItemStart;
        return result;
    }

    void Scheduler::reset()
    {
        mStates.clear();
        mHaveSeedState = false;
        mHaveConfig = false;
    }

    void Scheduler::reconfigure(F64 now_seconds, U64 dancer_key,
                                const SchedulerConfig& config)
    {
        if (!mHaveConfig)
        {
            mConfig = config;
            mHaveConfig = true;
            return;
        }
        if (sameConfig(config, mConfig)) return;

        if (config.mMode == mConfig.mMode)
        {
            for (auto& pair : mStates)
            {
                pair.second.mAnchor = now_seconds;
                pair.second.mHaveProcessedEvent = false;
            }
        }
        else
        {
            const auto preferred = mStates.find(
                stateKey(dancer_key, mConfig.mMode));
            const auto source = preferred != mStates.end()
                ? preferred : mStates.begin();
            mHaveSeedState = source != mStates.end() &&
                             source->second.mInitialized;
            if (mHaveSeedState) mSeedState = source->second;
            mStates.clear();
        }
        mConfig = config;
    }

    S32 Scheduler::current(U64 dancer_key, PlaylistMode mode) const
    {
        const auto found = mStates.find(stateKey(dancer_key, mode));
        return found == mStates.end() ? -1 : found->second.mCurrent;
    }

    F64 Scheduler::currentBoundary(U64 dancer_key, PlaylistMode mode) const
    {
        const auto found = mStates.find(stateKey(dancer_key, mode));
        return found == mStates.end() ? 0.0 : found->second.mItemStart;
    }

    void Scheduler::erase(U64 dancer_key, PlaylistMode mode)
    {
        (void)mode;
        // Per-dancer states are keyed by the digest directly. Keep key zero,
        // which is the shared SAME_FOR_ALL state and may still serve others.
        if (dancer_key != 0ULL) mStates.erase(dancer_key);
    }

    void Scheduler::remapItems(const std::vector<S32>& old_to_new)
    {
        for (auto& pair : mStates)
        {
            State& state = pair.second;
            state.mCurrent = remapItem(state.mCurrent, old_to_new);
        }
        if (mHaveSeedState)
        {
            mSeedState.mCurrent = remapItem(mSeedState.mCurrent, old_to_new);
        }
    }

    Crossfade crossfadeWeights(F64 elapsed_seconds, F64 duration_seconds)
    {
        Crossfade result;
        const F64 duration = finiteNonNegative(duration_seconds);
        if (duration <= 0.0)
        {
            return result;
        }
        const F64 t = clamp01(finiteNonNegative(elapsed_seconds) / duration);
        result.mFrom = 1.0 - t;
        result.mTo = t;
        result.mComplete = t >= 1.0;
        return result;
    }
}
