/**
 * @file aldirectorswitchermodel.h
 * @brief Deterministic, renderer-independent Director camera switch scheduler.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALDIRECTORSWITCHERMODEL_H
#define AL_ALDIRECTORSWITCHERMODEL_H

#include "stdtypes.h"

#include <array>
#include <cstddef>

namespace ALDirectorSwitcherModel
{
    constexpr std::size_t SLOT_COUNT = 12;
    constexpr F64 DEFAULT_INTERVAL_SECONDS = 8.0;
    constexpr F64 MIN_INTERVAL_SECONDS = 0.1;
    constexpr F64 MAX_INTERVAL_SECONDS = 3600.0;
    constexpr F64 MAX_JITTER_FRACTION = 0.45;
    constexpr U64 DEFAULT_SEED = 0x416c6368656d79ULL; // "Alchemy"

    struct Config
    {
        bool mAuto = false;
        bool mSequence = false;
        F64  mIntervalSeconds = DEFAULT_INTERVAL_SECONDS;
        F64  mJitterSeconds = 0.0;
        U64  mSeed = DEFAULT_SEED;
        std::array<bool, SLOT_COUNT> mEnabled{};
    };

    struct Frame
    {
        bool mCut = false;
        S32  mSlot = -1;
        F64  mBoundary = 0.0;
        U64  mEventIndex = 0;     // zero-based event index since the last rebase
        U64  mEventsElapsed = 0;  // newly elapsed events, including skipped ones
    };

    // Applies the same defensive bounds used internally by Controller.
    Config sanitizeConfig(const Config& config);

    class Controller
    {
    public:
        Controller() = default;

        // Evaluate an absolute presentation time. A large forward jump advances
        // directly to the final elapsed event and emits at most one cut.
        Frame update(F64 now_seconds, const Config& config);

        // A manual punch is valid for any slot, including one disabled for auto
        // selection. It becomes the active slot and restarts the auto interval.
        bool manualPunch(S32 slot, F64 now_seconds);

        // Restart the schedule at an absolute time without changing the active
        // slot. Invalid times fail closed and leave state untouched.
        bool rebase(F64 now_seconds);

        // Clear all scheduler and active-slot state.
        void reset();

        S32 activeSlot() const { return mActiveSlot; }

    private:
        static bool validTime(F64 now_seconds);
        static bool sameConfig(const Config& lhs, const Config& rhs);
        static U64 splitMix64(U64 value);
        static U64 hashLane(U64 seed, U64 salt, U64 index, U64 lane);
        static F64 unitFromHash(U64 value);

        void rebaseInternal(F64 now_seconds);
        F64 eventBoundary(U64 event_index) const;
        bool findLastDueEvent(F64 now_seconds, U64& event_index) const;
        S32 selectSlot(U64 event_index) const;
        S32 selectSequenceSlot(U64 event_index) const;
        S32 selectRandomSlot(U64 event_index) const;
        U32 enabledSlots(std::array<S32, SLOT_COUNT>& slots) const;
        void buildBasePermutation(
            U64 cycle_index,
            std::array<S32, SLOT_COUNT>& slots,
            U32& count) const;

        bool   mInitialized = false;
        bool   mHaveConfig = false;
        bool   mHaveProcessedEvent = false;
        Config mConfig;
        F64    mLastTime = 0.0;
        F64    mAnchorTime = 0.0;
        U64    mProcessedEvent = 0;
        S32    mActiveSlot = -1;
        S32    mAnchorSlot = -1;
    };
}

#endif // AL_ALDIRECTORSWITCHERMODEL_H
