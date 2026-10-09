/**
 * @file aldanceball.h
 * @brief Session-local, client-only multi-avatar dance playback engine.
 *
 * This header is the UI package's public contract. Mutators are main-thread
 * only. Nothing here sends animation messages to a simulator; playback uses
 * LLCharacter motion instances and Ghost Studio's public drive-mode API.
 */

#ifndef AL_ALDANCEBALL_H
#define AL_ALDANCEBALL_H

#include "aldanceballmodel.h"
#include "alavatarstaging.h"
#include "llsd.h"
#include "lluuid.h"
#include "stdtypes.h"

#include <map>
#include <set>
#include <string>
#include <vector>

class LLVOAvatar;

class ALDanceBall
{
public:
    enum class DancerKind : U8 { SELF, RESIDENT, ANIMESH, GHOST };
    enum class DancerState : U8
    {
        IDLE,
        PLAYING,
        PENDING_ASSET,
        ASSET_FAILED,
        SERVER_OWNS_UUID,
        SKIPPED_SEATED,
        SKIPPED_MUTED,
        SKIPPED_MOVING,
        ABSENT,
        IN_USE,
        TAKEN_OVER
    };
    enum class AssetStatus : U8 { PENDING, READY, UNAVAILABLE };
    enum class AutoJoinFocus : U8 { ME, CAMERA_FOCUS, SUBJECT_A };

    struct Dancer
    {
        LLUUID mId;             // canonical actor id; stable Studio id for GHOST
        std::string mShortId;   // cached for draw-path labels
        DancerKind mKind = DancerKind::RESIDENT;
        DancerState mState = DancerState::IDLE;
        bool mPresent = false;
    };

    struct PlaylistEntry
    {
        LLUUID mAssetId;
        std::string mLabel;
        AssetStatus mStatus = AssetStatus::PENDING;
        bool mLoop = true;
        F64 mDuration = 0.0;
        F64 mLoopIn = 0.0;
        F64 mLoopOut = 0.0;
    };

    struct Timing
    {
        ALDanceBallModel::PlaylistMode mMode =
            ALDanceBallModel::PlaylistMode::SAME_FOR_ALL;
        bool mAutoAdvance = false;
        F64 mAdvanceSeconds = 8.0;
        F64 mAdvanceJitterSeconds = 0.0;
        F64 mDesync = 0.0;
        F64 mSpeed = 1.0;
        F64 mSpeedJitter = 0.0;
        F64 mCrossfadeSeconds = ALDanceBallModel::DEFAULT_CROSSFADE_SECONDS;
        S32 mPriority = 6; // -1 = asset; otherwise 4, 5, or 6
        bool mDanceDrivesFace = false;
        U64 mSeed = ALDanceBallModel::DEFAULT_SEED;
    };

    struct Wave
    {
        bool mEnabled = false;
        ALDanceBallModel::WaveDirection mDirection =
            ALDanceBallModel::WaveDirection::OUTWARD;
        ALDanceBallModel::WaveApply mApply =
            ALDanceBallModel::WaveApply::START_ONLY;
        F64 mSpreadMetersPerSecond = 10.0;
        AutoJoinFocus mFocus = AutoJoinFocus::ME;
    };

    static ALDanceBall& instance();

    // Called once per frame before gObjectList.update(). Near-zero early-out
    // while OFF or roster-empty. shutdown() is idempotent and clears session
    // state; the app tick also invokes it when the login world disappears.
    void tick();
    void shutdown();

    // Master transport. OFF immediately eases/releases owned motions and
    // restores every ghost's prior Studio drive mode.
    void setEnabled(bool enabled);
    bool enabled() const { return mEnabled; }
    void setFrozen(bool frozen);
    bool frozen() const { return mClock.frozen(); }
    void setSpeed(F64 speed);
    void rampSpeed(F64 target_speed, F64 seconds,
                   ALDanceBallModel::RampCurve curve =
                       ALDanceBallModel::RampCurve::SMOOTHSTEP);
    void next();
    void previous();
    bool play(std::size_t playlist_index); // punch one row across the roster
    bool play(std::size_t playlist_index,
              const std::set<LLUUID>& selected_dancers);
    void reshuffle();

    // Roster. addNearby() is the explicit one-click action (20 m default).
    // Excluded ids cannot be re-added by nearby/cast/auto-join until included.
    bool addDancer(const LLUUID& id, DancerKind kind);
    bool addDancer(const LLUUID& id); // auto-classifies and canonicalizes
    bool removeDancer(const LLUUID& id);
    void exclude(const LLUUID& id);
    void include(const LLUUID& id);
    bool isExcluded(const LLUUID& id) const;
    U32 addNearby(F32 radius_meters = 20.f,
                  AutoJoinFocus focus = AutoJoinFocus::ME);
    U32 addCast();
    const std::vector<Dancer>& roster() const { return mRoster; }
    const std::set<LLUUID>& exclusions() const { return mExcluded; }

    // Session-local blocking passthrough. Self is deliberately rejected by
    // ALAvatarStaging; these calls operate on canonical dancer ids.
    void setBlockingEnabled(bool enabled);
    bool blockingEnabled() const;
    U64 stagingRevision() const;
    const ALAvatarStaging::Entry* blockingEntry(const LLUUID& id) const;
    ALAvatarStaging::Status blockingStatus(const LLUUID& id) const;
    bool blockingWorldYaw(const LLUUID& id, F32& yaw) const;
    bool setBlockingYaw(const LLUUID& id, F32 radians);
    bool nudgeBlockingYaw(const LLUUID& id, F32 radians);
    bool setBlockingMode(const LLUUID& id, ALAvatarStaging::FacingMode mode);
    bool setDancerBlocking(const LLUUID& id, bool enabled);
    bool faceBlockingCamera(const LLUUID& id, bool keep);
    bool faceBlockingSubjectA(const LLUUID& id, bool keep);
    bool faceBlockingCentre(const LLUUID& id, bool away, bool keep);
    bool faceBlockingTarget(const LLUUID& id, const LLUUID& target, bool keep);
    bool nudgeBlockingLocal(const LLUUID& id, const class LLVector3& delta);
    bool pinBlockingHere(const LLUUID& id);
    bool releaseBlockingPin(const LLUUID& id);
    bool resetBlocking(const LLUUID& id);
    void resetAllBlocking();
    bool setBlockingCentre(AutoJoinFocus focus);
    void clearBlockingCentre();
    bool hasBlockingCentre() const;
    ALAvatarStaging::BallCentreSource blockingCentreSource() const;
    std::size_t blockingCentreDancerCount() const;
    bool ensureBlockingCentre(const std::vector<LLUUID>& ids);
    bool applyBlockingFormation(const std::vector<LLUUID>& ids,
        ALAvatarStagingModel::Formation formation, F32 spacing);

    void setAutoJoin(bool enabled, F32 radius_meters = 20.f,
                     AutoJoinFocus focus = AutoJoinFocus::ME);
    bool autoJoin() const { return mAutoJoin; }
    F32 autoJoinRadius() const { return mAutoJoinRadius; }
    AutoJoinFocus autoJoinFocus() const { return mAutoJoinFocus; }
    void setIncludeSeated(bool include);
    bool includeSeated() const { return mIncludeSeated; }
    void setIncludeSelf(bool include);
    bool includeSelf() const { return mIncludeSelf; }

    // Playlist entries are session-only. Metadata is refreshed from the live
    // LLKeyframeMotion after load, never trusted from serialized input.
    bool addAnimation(const LLUUID& asset_id,
                      const std::string& label = std::string());
    bool removeAnimation(std::size_t index);
    bool renameAnimation(std::size_t index, const std::string& label);
    bool moveAnimation(std::size_t from, std::size_t to);
    void clearPlaylist();
    const std::vector<PlaylistEntry>& playlist() const { return mPlaylist; }

    void setTiming(const Timing& timing);
    const Timing& timing() const { return mTiming; }
    void setWave(const Wave& wave) { mWave = wave; ++mStatusRevision; }
    const Wave& wave() const { return mWave; }

    // Manual, explicit session interchange. No file I/O and no auto-load.
    LLSD exportSession() const;
    bool importSession(const LLSD& data);
    const std::string& importErrorReason() const { return mImportErrorReason; }

    // Live UI status. -1/empty means no current playable item.
    S32 currentItem(const LLUUID& dancer_id) const;
    std::string currentLabel(const LLUUID& dancer_id) const;
    S32 sameForAllCurrentItem() const;
    std::string sameForAllCurrentLabel() const;
    F64 liveSpeed() const { return mClock.speed(); }
    F64 speedRampProgress() const { return mClock.rampProgress(); }

    // Revision gates for UI mirrors. Roster covers membership/exclusions;
    // playlist covers order/labels; status covers live states/facts/options.
    U64 rosterRevision() const { return mRosterRevision; }
    U64 playlistRevision() const { return mPlaylistRevision; }
    U64 statusRevision() const { return mStatusRevision; }

private:
    ALDanceBall() = default;
    ~ALDanceBall();
    ALDanceBall(const ALDanceBall&) = delete;
    ALDanceBall& operator=(const ALDanceBall&) = delete;

    struct MotionHandle
    {
        enum class State : U8 { PENDING, LIVE, FADING_OUT, RELEASING };

        LLUUID mAnim;
        LLUUID mToken;
        bool mOwnsMotion = false;
        bool mControlsMotion = false;
        bool mPlaybackStarted = false;
        bool mPrioritySaved = false;
        S32 mSavedPriority = -1;
        S32 mItem = -1;
        F64 mStartedAt = 0.0;
        F64 mUnitStartedAt = 0.0;
        F64 mDelay = 0.0;
        F64 mDuration = 0.0;
        F64 mLoopIn = 0.0;
        F64 mLoopOut = 0.0;
        F32 mWeight = 0.f;
        bool mLoop = true;
        State mState = State::PENDING;
        ALDanceBallModel::LegWeight mMix;
        ALDanceBallModel::SampleClock mSampleClock;
        bool mSampleInitialized = false;
        bool mSharedSync = false;
        F64 mSharedSyncError = 0.0;
        F64 mSharedSyncElapsed = 0.0;
        void clear();
        bool valid() const { return mAnim.notNull() && mToken.notNull(); }
    };

    struct Runtime
    {
        std::vector<MotionHandle> mLegs;
        S32 mCurrentItem = -1;
        F64 mCurrentBoundary = 0.0;
        F64 mTransitionStartedAt = 0.0;
        bool mTransitionActive = false;
        bool mReleaseActive = false;
        DancerKind mKind = DancerKind::RESIDENT;
        bool mGhostClaimed = false;
        bool mGhostDirected = false;
        bool mGhostTakenOver = false;
        LLUUID mGhostInstance;
        S32 mGhostPriorMode = 0;
        LLUUID mGhostPriorAnim;
        S32 mGhostPriorLoopMode = 0;
        LLUUID mGhostExpectedAnim;
        S32 mManualItem = -1;
        S32 mManualBaseItem = -1;
        F64 mManualBoundary = 0.0;
        DancerState mLoggedState = DancerState::IDLE;
        DancerState mCachedSkip = DancerState::ABSENT;
        U64 mCheckedScan = ~0ULL;
    };

    LLVOAvatar* resolve(const Dancer& dancer) const;
    DancerKind classify(const LLUUID& id, LLVOAvatar* avatar) const;
    bool focalPoint(AutoJoinFocus focus, class LLVector3d& point) const;
    DancerState skipState(const Dancer& dancer, LLVOAvatar* avatar) const;
    void scanRoster();
    void updateDancer(Dancer& dancer, F64 now, F64 farthest_distance,
                      const std::vector<ALDanceBallModel::PlaylistItem>& items,
                      const ALDanceBallModel::SchedulerConfig& config);
    void transition(Dancer& dancer, LLVOAvatar* avatar, Runtime& runtime,
                    S32 item, F64 boundary, F64 farthest_distance,
                    bool is_switch);
    bool ensureMotion(LLVOAvatar* avatar, MotionHandle& handle,
                      F32 weight, F64 sample, bool start_allowed,
                      DancerState& state);
    void finishReleaseMotion(LLVOAvatar* avatar, MotionHandle& handle);
    void updateLegs(const Dancer& dancer, Runtime& runtime,
                    F64 now, bool releasing);
    void beginTransition(Runtime& runtime, std::size_t incoming);
    void releaseRuntime(const Dancer& dancer, Runtime& runtime);
    void releaseAll(bool immediate = false);
    void restoreGhost(Runtime& runtime);
    bool claimGhost(const Dancer& dancer, Runtime& runtime);
    bool directGhost(const Dancer& dancer, Runtime& runtime,
                     const LLUUID& animation);
    void remapPlaylist(const std::vector<S32>& old_to_new);
    F64 dancerDistance(LLVOAvatar* avatar) const;
    std::vector<ALDanceBallModel::PlaylistItem> modelPlaylist() const;
    ALDanceBallModel::SchedulerConfig schedulerConfig() const;
    void setDancerState(Dancer& dancer, Runtime& runtime, DancerState state);
    void refreshEntryFacts(PlaylistEntry& entry, LLVOAvatar* avatar,
                           const MotionHandle& handle);
    std::string defaultLabel(const LLUUID& asset_id) const;

    std::vector<Dancer> mRoster;
    std::set<LLUUID> mExcluded;
    std::vector<PlaylistEntry> mPlaylist;
    std::map<LLUUID, Runtime> mRuntime;
    ALDanceBallModel::Clock mClock;
    ALDanceBallModel::Scheduler mScheduler;
    Timing mTiming;
    Wave mWave;
    U64 mReshuffle = 0;
    U64 mRosterRevision = 0;
    U64 mPlaylistRevision = 0;
    U64 mStatusRevision = 0;
    U64 mScanGeneration = 0;
    bool mEnabled = false;
    bool mAutoJoin = false;
    bool mIncludeSeated = false;
    bool mIncludeSelf = false;
    bool mHadWorld = false;
    F32 mAutoJoinRadius = 20.f;
    AutoJoinFocus mAutoJoinFocus = AutoJoinFocus::ME;
    F64 mLastScanWall = -1.0;
    F64 mUnitTime = 0.0; // presentation time at fixed 1x (play-once/fades/waves)
    F64 mFadeTime = 0.0; // advances while transport is frozen so releases finish
    F64 mTransportDelta = 0.0; // integrated global-speed delta; zero while frozen
    F64 mSlewDelta = 0.0; // unscaled presentation delta; zero while frozen.
                          // Only for play-once unit-time lanes; looped lanes
                          // slew on mTransportDelta.
    F64 mFarthestDistance = 0.0;
    std::string mImportErrorReason;
};

#endif // AL_ALDANCEBALL_H
