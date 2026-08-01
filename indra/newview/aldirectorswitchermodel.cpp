/**
 * @file aldirectorswitchermodel.cpp
 * @brief Deterministic, renderer-independent Director camera switch scheduler.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "aldirectorswitchermodel.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ALDirectorSwitcherModel
{
    namespace
    {
        constexpr U64 FALLBACK_SEED = 0x9e3779b97f4a7c15ULL;
        constexpr U64 TIMING_SALT = 0x243f6a8885a308d3ULL;
        constexpr U64 PERMUTATION_SALT = 0x13198a2e03707344ULL;
        constexpr U64 MAX_EVENT_INDEX = (1ULL << 52) - 2ULL;

        F64 finiteOr(F64 value, F64 fallback)
        {
            return std::isfinite(value) ? value : fallback;
        }
    }

    Config sanitizeConfig(const Config& input)
    {
        Config output = input;
        output.mIntervalSeconds = std::clamp(
            finiteOr(input.mIntervalSeconds, DEFAULT_INTERVAL_SECONDS),
            MIN_INTERVAL_SECONDS, MAX_INTERVAL_SECONDS);
        output.mJitterSeconds = std::clamp(
            finiteOr(input.mJitterSeconds, 0.0),
            0.0, output.mIntervalSeconds * MAX_JITTER_FRACTION);
        if (output.mSeed == 0)
        {
            output.mSeed = FALLBACK_SEED;
        }
        return output;
    }

    bool Controller::validTime(F64 now_seconds)
    {
        return std::isfinite(now_seconds) && now_seconds >= 0.0;
    }

    bool Controller::sameConfig(const Config& lhs, const Config& rhs)
    {
        return lhs.mAuto == rhs.mAuto &&
               lhs.mSequence == rhs.mSequence &&
               lhs.mIntervalSeconds == rhs.mIntervalSeconds &&
               lhs.mJitterSeconds == rhs.mJitterSeconds &&
               lhs.mSeed == rhs.mSeed &&
               lhs.mEnabled == rhs.mEnabled;
    }

    U64 Controller::splitMix64(U64 value)
    {
        value += 0x9e3779b97f4a7c15ULL;
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31);
    }

    U64 Controller::hashLane(U64 seed, U64 salt, U64 index, U64 lane)
    {
        // Every decision is an addressed SplitMix64 lane. No mutable random
        // state is consumed, so hitches and frame grouping cannot perturb later
        // choices.
        return splitMix64(seed ^ salt ^
                          splitMix64(index + 0x632be59bd9b4e019ULL) ^
                          splitMix64(lane + 0x8cb92ba72f3d8dd7ULL));
    }

    F64 Controller::unitFromHash(U64 value)
    {
        return static_cast<F64>(value >> 11) *
               (1.0 / 9007199254740992.0);
    }

    void Controller::rebaseInternal(F64 now_seconds)
    {
        mInitialized = true;
        mLastTime = now_seconds;
        mAnchorTime = now_seconds;
        mHaveProcessedEvent = false;
        mProcessedEvent = 0;
        mAnchorSlot = mActiveSlot;
    }

    bool Controller::rebase(F64 now_seconds)
    {
        if (!validTime(now_seconds))
        {
            return false;
        }
        rebaseInternal(now_seconds);
        return true;
    }

    bool Controller::manualPunch(S32 slot, F64 now_seconds)
    {
        if (slot < 0 || slot >= static_cast<S32>(SLOT_COUNT) ||
            !validTime(now_seconds))
        {
            return false;
        }
        mActiveSlot = slot;
        rebaseInternal(now_seconds);
        return true;
    }

    void Controller::reset()
    {
        mInitialized = false;
        mHaveConfig = false;
        mHaveProcessedEvent = false;
        mConfig = Config();
        mLastTime = 0.0;
        mAnchorTime = 0.0;
        mProcessedEvent = 0;
        mActiveSlot = -1;
        mAnchorSlot = -1;
    }

    F64 Controller::eventBoundary(U64 event_index) const
    {
        if (mConfig.mJitterSeconds <= 0.0)
        {
            return mAnchorTime +
                   static_cast<F64>(event_index + 1ULL) *
                       mConfig.mIntervalSeconds;
        }

        // Jitter is pair-balanced. Pair P uses intervals I+j(P), I-j(P), so
        // every individual interval stays inside the configured +/- jitter
        // bound while each completed pair lands exactly on 2*I. This retains a
        // direct absolute boundary formula; a large hitch never has to replay
        // an unbounded chain of random interval draws.
        const U64 pair_index = event_index / 2ULL;
        const F64 pair_start =
            mAnchorTime + static_cast<F64>(pair_index) *
                (2.0 * mConfig.mIntervalSeconds);
        if ((event_index & 1ULL) != 0ULL)
        {
            return pair_start + 2.0 * mConfig.mIntervalSeconds;
        }
        const F64 signed_unit =
            unitFromHash(hashLane(
                mConfig.mSeed, TIMING_SALT, pair_index, 0ULL)) *
                2.0 - 1.0;
        return pair_start + mConfig.mIntervalSeconds +
               signed_unit * mConfig.mJitterSeconds;
    }

    bool Controller::findLastDueEvent(
        F64 now_seconds, U64& event_index) const
    {
        const F64 elapsed = now_seconds - mAnchorTime;
        if (elapsed < 0.0)
        {
            return false;
        }

        const F64 raw_quotient =
            std::floor(elapsed / mConfig.mIntervalSeconds);
        U64 nominal_event_number = 0;
        if (raw_quotient >= static_cast<F64>(MAX_EVENT_INDEX))
        {
            nominal_event_number = MAX_EVENT_INDEX;
        }
        else if (raw_quotient > 0.0)
        {
            nominal_event_number = static_cast<U64>(raw_quotient);
        }

        // Boundary N is nominally anchor + N*interval and is displaced by at
        // most 0.45 interval. Therefore the final due boundary can only be
        // N-1, N, or N+1. This bounded lookup is direct even after a huge hitch.
        const U64 first_number =
            nominal_event_number > 1ULL
                ? nominal_event_number - 1ULL : 1ULL;
        const U64 last_number = std::min(
            nominal_event_number + 1ULL, MAX_EVENT_INDEX + 1ULL);

        bool found = false;
        for (U64 event_number = first_number;
             event_number <= last_number; ++event_number)
        {
            const U64 candidate = event_number - 1ULL;
            if (eventBoundary(candidate) <= now_seconds)
            {
                event_index = candidate;
                found = true;
            }
        }
        return found;
    }

    U32 Controller::enabledSlots(
        std::array<S32, SLOT_COUNT>& slots) const
    {
        U32 count = 0;
        for (std::size_t slot = 0; slot < SLOT_COUNT; ++slot)
        {
            if (mConfig.mEnabled[slot])
            {
                slots[count++] = static_cast<S32>(slot);
            }
        }
        return count;
    }

    S32 Controller::selectSequenceSlot(U64 event_index) const
    {
        std::array<S32, SLOT_COUNT> slots{};
        const U32 count = enabledSlots(slots);
        if (count == 0)
        {
            return -1;
        }

        U32 first = 0;
        if (mAnchorSlot >= 0 &&
            mAnchorSlot < static_cast<S32>(SLOT_COUNT))
        {
            // Start at the first enabled bank slot after the framing that was
            // live at the rebase, even when that framing is disabled for auto.
            for (U32 offset = 1; offset <= SLOT_COUNT; ++offset)
            {
                const S32 candidate =
                    (mAnchorSlot + static_cast<S32>(offset)) %
                    static_cast<S32>(SLOT_COUNT);
                if (!mConfig.mEnabled[static_cast<std::size_t>(candidate)])
                {
                    continue;
                }
                for (U32 i = 0; i < count; ++i)
                {
                    if (slots[i] == candidate)
                    {
                        first = i;
                        offset = static_cast<U32>(SLOT_COUNT + 1);
                        break;
                    }
                }
            }
        }

        return slots[
            (first + static_cast<U32>(event_index % count)) % count];
    }

    void Controller::buildBasePermutation(
        U64 cycle_index,
        std::array<S32, SLOT_COUNT>& slots,
        U32& count) const
    {
        count = enabledSlots(slots);
        for (U32 i = count; i > 1; --i)
        {
            const U64 hash = hashLane(
                mConfig.mSeed, PERMUTATION_SALT, cycle_index,
                static_cast<U64>(i));
            const U32 other = static_cast<U32>(hash % i);
            std::swap(slots[i - 1], slots[other]);
        }
    }

    S32 Controller::selectRandomSlot(U64 event_index) const
    {
        std::array<S32, SLOT_COUNT> permutation{};
        const U32 enabled_count = enabledSlots(permutation);
        if (enabled_count == 0)
        {
            return -1;
        }

        // With two enabled slots, avoiding repeats across a full-permutation
        // cycle boundary forces every cycle to use the same orientation.
        const U64 cycle_index =
            enabled_count == 2
                ? 0ULL : event_index / enabled_count;
        U32 count = 0;
        buildBasePermutation(cycle_index, permutation, count);
        if (count == 1)
        {
            return permutation[0];
        }

        if (cycle_index == 0)
        {
            if (permutation[0] == mAnchorSlot)
            {
                std::swap(permutation[0], permutation[1]);
            }
        }
        else
        {
            std::array<S32, SLOT_COUNT> previous{};
            U32 previous_count = 0;
            buildBasePermutation(
                cycle_index - 1ULL, previous, previous_count);
            // For count > 2, all repeat-avoidance swaps affect positions 0/1,
            // so the previous cycle's last element remains its base last.
            if (previous_count == count &&
                permutation[0] == previous[count - 1])
            {
                std::swap(permutation[0], permutation[1]);
            }
        }

        return permutation[
            static_cast<U32>(event_index % count)];
    }

    S32 Controller::selectSlot(U64 event_index) const
    {
        return mConfig.mSequence
            ? selectSequenceSlot(event_index)
            : selectRandomSlot(event_index);
    }

    Frame Controller::update(F64 now_seconds, const Config& input)
    {
        Frame frame;
        if (!validTime(now_seconds))
        {
            return frame;
        }

        const Config config = sanitizeConfig(input);
        if (!mInitialized)
        {
            mConfig = config;
            mHaveConfig = true;
            rebaseInternal(now_seconds);
            return frame;
        }

        if (!mHaveConfig)
        {
            // manualPunch()/rebase() may establish an anchor before the first
            // runtime config sample. Install that sample without moving the
            // explicit anchor.
            mConfig = config;
            mHaveConfig = true;
            if (now_seconds < mLastTime)
            {
                rebaseInternal(now_seconds);
                return frame;
            }
        }
        else if (now_seconds < mLastTime ||
                 !sameConfig(config, mConfig))
        {
            mConfig = config;
            mHaveConfig = true;
            rebaseInternal(now_seconds);
            return frame;
        }
        mLastTime = now_seconds;

        if (!mConfig.mAuto)
        {
            return frame;
        }

        std::array<S32, SLOT_COUNT> slots{};
        if (enabledSlots(slots) == 0)
        {
            return frame;
        }

        U64 final_event = 0;
        if (!findLastDueEvent(now_seconds, final_event) ||
            (mHaveProcessedEvent && final_event <= mProcessedEvent))
        {
            return frame;
        }

        const U64 elapsed_events = mHaveProcessedEvent
            ? final_event - mProcessedEvent
            : final_event + 1ULL;
        const S32 slot = selectSlot(final_event);
        if (slot < 0)
        {
            return frame;
        }

        mHaveProcessedEvent = true;
        mProcessedEvent = final_event;
        mActiveSlot = slot;

        frame.mCut = true;
        frame.mSlot = slot;
        frame.mBoundary = eventBoundary(final_event);
        frame.mEventIndex = final_event;
        frame.mEventsElapsed = elapsed_events;
        return frame;
    }
}
