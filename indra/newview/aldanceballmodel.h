/**
 * @file aldanceballmodel.h
 * @brief Dependency-free timing and playlist model for Dance Ball.
 */

#ifndef AL_ALDANCEBALLMODEL_H
#define AL_ALDANCEBALLMODEL_H

#include "stdtypes.h"

#include <cstddef>
#include <map>
#include <vector>

namespace ALDanceBallModel
{
    constexpr F64 DEFAULT_CROSSFADE_SECONDS = 0.5;
    constexpr U64 DEFAULT_SEED = 0x44616e636542616cULL; // "DanceBal"
    // A phase correction may add or remove at most this much playback rate, so
    // the effective rate of a corrected lane stays within [0.5x, 1.5x].
    constexpr F64 MAX_PHASE_SLEW_RATE = 0.5;

    enum class RampCurve : U8 { LINEAR, SMOOTHSTEP };
    enum class PlaylistMode : U8
    {
        SAME_FOR_ALL,
        SHUFFLE_EACH_DANCER,
        SEQUENCE_EACH_DANCER
    };
    enum class WaveDirection : U8 { OUTWARD, INWARD };
    enum class WaveApply : U8 { START_ONLY, SWITCH_ONLY, START_AND_SWITCH };

    class Clock
    {
    public:
        void reset(F64 time = 0.0, F64 speed = 1.0);
        F64 advance(F64 delta_seconds);
        void setFrozen(bool frozen) { mFrozen = frozen; }
        bool frozen() const { return mFrozen; }
        void setSpeed(F64 speed);
        void rampTo(F64 target_speed, F64 duration_seconds,
                    RampCurve curve = RampCurve::SMOOTHSTEP);

        F64 time() const { return mTime; }
        F64 speed() const { return mSpeed; }
        bool ramping() const { return mRamping; }
        F64 rampProgress() const;

    private:
        F64 mTime = 0.0;
        F64 mSpeed = 1.0;
        F64 mRampStart = 1.0;
        F64 mRampTarget = 1.0;
        F64 mRampDuration = 0.0;
        F64 mRampElapsed = 0.0;
        RampCurve mRampCurve = RampCurve::SMOOTHSTEP;
        bool mFrozen = false;
        bool mRamping = false;
    };

    // Addressed SplitMix64 lanes: frame grouping never changes a dancer's
    // phase or speed. reshuffle is an explicit user-controlled generation.
    F64 phaseOffset(U64 dancer_key, F64 desync, U64 seed,
                    U64 reshuffle = 0);
    F64 speedMultiplier(U64 dancer_key, F64 jitter_fraction, U64 seed,
                        U64 reshuffle = 0);
    F64 effectiveSpeedMultiplier(U64 dancer_key, PlaylistMode mode,
                                 F64 desync, F64 jitter_fraction, U64 seed,
                                 U64 reshuffle = 0);

    // Duration over which a phase error is slewed: the crossfade, stretched so
    // the correction never adds more than MAX_PHASE_SLEW_RATE times the
    // playback rate of the lane being corrected (lane_rate, default 1 = the
    // transport rate itself; a slower lane gets a proportionally longer
    // slew). The
    // duration is in the same timeline the lane's transport delta uses, so it
    // scales with global speed. A zero crossfade is an explicit "no smoothing"
    // request and returns zero (the caller snaps).
    F64 phaseSlewDuration(F64 crossfade_seconds, F64 phase_error,
                          F64 lane_rate = 1.0);

    // A continuously integrated playback lane. Retargeting never rewrites
    // elapsed history: rate and phase corrections are applied over the slew
    // duration, and a zero delta (Freeze) advances neither one.
    class SampleClock
    {
    public:
        void reset(F64 sample_time, F64 rate, F64 phase_seconds);
        // loop_cycle > 0 marks a looped lane: the phase change takes the
        // shortest path around the loop and the slew is lengthened with
        // phaseSlewDuration so the effective rate never leaves [0.5x, 1.5x] of
        // the lane's own playback rate (and so never goes negative).
        // loop_cycle == 0 applies phase_seconds literally over slew_seconds.
        void retarget(F64 rate, F64 phase_seconds, F64 slew_seconds,
                      F64 loop_cycle = 0.0);
        // slew_delta is the slew's progress clock and MUST be the same
        // speed-scaled transport delta (callers pass transport_delta for
        // both). Driving it from the unscaled wall delta would let a slow
        // global speed (or a slow lane) turn a backward correction into
        // reverse playback. Zero deltas (Freeze) advance neither.
        F64 advance(F64 transport_delta, F64 slew_delta);

        F64 time() const { return mTime; }
        F64 rate() const { return mRate; }
        F64 phase() const { return mPhase; }

    private:
        F64 mTime = 0.0;
        F64 mRate = 1.0;
        F64 mPhase = 0.0;
        F64 mRateStart = 1.0;
        F64 mRateTarget = 1.0;
        F64 mPhaseStart = 0.0;
        F64 mPhaseTarget = 0.0;
        F64 mSlewDuration = 0.0;
        F64 mSlewElapsed = 0.0;
        bool mBoundedSlew = false; // looped slew sized by phaseSlewDuration
    };

    struct LegWeight
    {
        F64 mCurrent = 0.0;
        F64 mStart = 0.0;
        F64 mTarget = 0.0;
        bool mStarted = false;
    };

    // Snapshot a transition from the live weights. Started legs are scaled
    // down as one envelope only when their live sum exceeds 1; a partial sum
    // (a fade in progress) is kept as-is and rises monotonically to 1 as the
    // incoming leg fades in, so a retarget never pops any weight upward.
    void retargetLegWeights(std::vector<LegWeight>& legs,
                            std::size_t incoming);
    void advanceLegWeights(std::vector<LegWeight>& legs, F64 progress);
    F64 slewToSharedSample(F64 shared_sample, F64 initial_error,
                           F64 progress);
    F64 nearestLoopError(F64 error, F64 cycle);

    F64 waveDelay(F64 distance, F64 farthest_distance, F64 spread_speed,
                  WaveDirection direction);
    bool waveApplies(WaveApply apply, bool is_switch);

    struct Clip
    {
        bool mLoop = true;
        F64 mDuration = 0.0;
        F64 mLoopIn = 0.0;
        F64 mLoopOut = 0.0;
    };

    struct Sample
    {
        F64 mTime = 0.0;
        bool mStarted = true;
        bool mEnded = false;
    };

    // phase is normalized [0,1]. Non-loop clips deliberately ignore speed and
    // phase and always run at 1x so native lifetime behavior is deterministic.
    Sample mapSample(const Clip& clip, F64 elapsed_seconds,
                     F64 speed, F64 phase, F64 start_delay = 0.0);

    struct PlaylistItem
    {
        bool mAvailable = true;
    };

    struct SchedulerConfig
    {
        PlaylistMode mMode = PlaylistMode::SAME_FOR_ALL;
        bool mAutoAdvance = false;
        F64 mIntervalSeconds = 8.0;
        F64 mJitterSeconds = 0.0;
        U64 mSeed = DEFAULT_SEED;
    };

    struct Selection
    {
        bool mChanged = false;
        S32 mItem = -1;
        S32 mPrevious = -1;
        F64 mBoundary = 0.0;
        U64 mEventIndex = 0;
    };

    class Scheduler
    {
    public:
        Selection update(F64 now_seconds, U64 dancer_key,
                         const std::vector<PlaylistItem>& items,
                         const SchedulerConfig& config);
        Selection next(F64 now_seconds, U64 dancer_key,
                       const std::vector<PlaylistItem>& items,
                       const SchedulerConfig& config);
        Selection previous(F64 now_seconds, U64 dancer_key,
                           const std::vector<PlaylistItem>& items,
                           const SchedulerConfig& config);
        Selection select(F64 now_seconds, U64 dancer_key, S32 item,
                         const std::vector<PlaylistItem>& items,
                         const SchedulerConfig& config);
        void reset();
        S32 current(U64 dancer_key, PlaylistMode mode) const;
        F64 currentBoundary(U64 dancer_key, PlaylistMode mode) const;
        void erase(U64 dancer_key, PlaylistMode mode);
        void remapItems(const std::vector<S32>& old_to_new);

    private:
        struct State
        {
            U64 mKey = 0;
            S32 mCurrent = -1;
            F64 mAnchor = 0.0;
            F64 mItemStart = 0.0;
            U64 mProcessedEvent = 0;
            U64 mManualEvent = 0;
            bool mHaveProcessedEvent = false;
            bool mInitialized = false;
        };

        static U64 stateKey(U64 dancer_key, PlaylistMode mode);
        static bool validTime(F64 value);
        static S32 stepAvailable(S32 from, S32 direction,
                                 const std::vector<PlaylistItem>& items);
        static S32 remapItem(S32 item,
                             const std::vector<S32>& old_to_new);
        static S32 randomAvailable(S32 current, U64 state_key, U64 event,
                                   const std::vector<PlaylistItem>& items,
                                   U64 seed);
        static S32 shuffleAtEvent(U64 state_key, U64 event,
                                  const std::vector<PlaylistItem>& items,
                                  U64 seed);
        static F64 boundary(const State& state, U64 event,
                            const SchedulerConfig& config);
        static bool sameConfig(const SchedulerConfig& lhs,
                               const SchedulerConfig& rhs);
        Selection manual(F64 now_seconds, U64 dancer_key,
                         const std::vector<PlaylistItem>& items,
                         const SchedulerConfig& config, S32 direction);
        void reconfigure(F64 now_seconds, U64 dancer_key,
                         const SchedulerConfig& config);

        std::map<U64, State> mStates;
        SchedulerConfig mConfig;
        State mSeedState;
        bool mHaveSeedState = false;
        bool mHaveConfig = false;
    };

    struct Crossfade
    {
        F64 mFrom = 0.0;
        F64 mTo = 1.0;
        bool mComplete = true;
    };

    Crossfade crossfadeWeights(F64 elapsed_seconds,
                               F64 duration_seconds = DEFAULT_CROSSFADE_SECONDS);
}

#endif // AL_ALDANCEBALLMODEL_H
