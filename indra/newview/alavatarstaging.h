/**
 * @file alavatarstaging.h
 * @brief Session-only local avatar blocking service.
 */

#ifndef AL_ALAVATARSTAGING_H
#define AL_ALAVATARSTAGING_H

#include "alavatarstagingmodel.h"
#include "llquaternion.h"
#include "llsd.h"
#include "llsingleton.h"
#include "lluuid.h"
#include "v3dmath.h"
#include "v3math.h"

#include <map>
#include <set>
#include <vector>

class LLVOAvatar;

class ALAvatarStaging final : public LLSingleton<ALAvatarStaging>
{
    LLSINGLETON(ALAvatarStaging);
    ~ALAvatarStaging() override;

public:
    enum class FacingMode : U8 { OFFSET, LOCK, TRACK };
    enum class TrackSource : U8
    {
        POINT, ACTOR, CAMERA, SUBJECT_A, BALL_CENTRE, AWAY_FROM_CENTRE
    };
    enum class Status : U8
    {
        OFF,
        ACTIVE,
        SUSPENDED_MOVER,
        SUSPENDED_SEATED_PIN,
        PIN_RELEASED,
        MUTED,
        ABSENT,
        GHOST,
        GHOST_STUDIO_OWNS
    };
    enum class BallCentreSource : U8 { NONE, SET, CENTROID };

    struct Entry
    {
        FacingMode mMode = FacingMode::OFFSET;
        F32 mYawOffset = 0.f;
        F32 mLockYaw = 0.f;
        TrackSource mTrackSource = TrackSource::POINT;
        LLUUID mTrackTarget;
        LLVector3d mTrackPoint;
        bool mTrackPointValid = false;
        LLVector3 mOffset;
        LLVector3d mPin;
        LLVector3d mPinSimAnchor;
        bool mPinAnchorValid = false;
        bool mPinRequested = false;
        bool mPinned = false;
        bool mResetting = false;
        bool mEasingOff = false;
        bool mOn = true;

        ALAvatarStagingModel::ScalarSpring mBaseYawSpring;
        ALAvatarStagingModel::ScalarSpring mYawSpring;
        ALAvatarStagingModel::VectorSpring mOffsetSpring;
        LLVector3 mLastBasePosition;
        LLVector3d mLastBaseGlobal;
        LLQuaternion mLastBaseRotation;
        LLVector3 mLastWrittenPosition;
        LLQuaternion mLastWrittenRotation;
        bool mHaveBase = false;
        bool mWasSeated = false;
        bool mMoverKnown = false;
        bool mMoverOwned = false;
        bool mMoverReleased = false;
        U32 mLastFrame = 0;
        LLVector3 mFrameOffset;
        F32 mFrameYawDelta = 0.f;
        F32 mFrameYaw = 0.f;

        bool mGhostCaptured = false;
        bool mWasGhost = false;
        LLVector3d mGhostBaseFoot;
        LLQuaternion mGhostBaseRotation;
        LLVector3d mGhostLastFoot;
        LLQuaternion mGhostLastRotation;
        U64 mGhostLastWriteRevision = 0;
        bool mGhostApplied = false;
        bool mGhostStudioOwned = false;
    };

    static inline void applyIfStaged(LLVOAvatar* avatar)
    {
        if (sEntryCount != 0 && avatar)
        {
            instance().apply(avatar);
        }
    }

    static inline bool keepsLive(const LLUUID& id)
    {
        return sEntryCount != 0 && instance().keepsLiveImpl(id);
    }

    bool setYaw(const LLUUID& id, F32 radians);
    bool nudgeYaw(const LLUUID& id, F32 radians);
    bool setMode(const LLUUID& id, FacingMode mode);
    bool facePoint(const LLUUID& id, const LLVector3d& point, bool keep_facing);
    bool faceTarget(const LLUUID& id, const LLUUID& target, bool keep_facing);
    bool faceCamera(const LLUUID& id, bool keep_facing = true);
    bool faceSubjectA(const LLUUID& id, bool keep_facing = true);
    bool faceBallCentre(const LLUUID& id, bool away, bool keep_facing);
    bool setOffset(const LLUUID& id, const LLVector3& offset);
    bool nudgeOffset(const LLUUID& id, const LLVector3& delta);
    bool nudgeLocalOffset(const LLUUID& id, const LLVector3& local_delta);
    bool pinHere(const LLUUID& id);
    bool pinAt(const LLUUID& id, const LLVector3d& position);
    bool releasePin(const LLUUID& id);
    bool applyFormation(const std::vector<LLUUID>& ordered_ids,
                        ALAvatarStagingModel::Formation formation,
                        F32 radius_or_spacing, F32 arc_radians = F_PI);
    bool reset(const LLUUID& id, bool eased = true);
    void resetAll(bool eased = true);
    bool remove(const LLUUID& id);
    bool setEntryEnabled(const LLUUID& id, bool enabled);
    void update();

    void setEnabled(bool enabled);
    bool enabled() const { return mEnabled; }
    void setBallCentre(const LLVector3d& centre);
    void clearBallCentre();
    const LLVector3d& ballCentre() const { return mBallCentre; }
    bool hasBallCentre() const { return mBallCentreValid; }
    BallCentreSource ballCentreSource() const { return mBallCentreSource; }
    std::size_t ballCentreDancerCount() const { return mBallCentreDancers.size(); }
    U64 revision() const { return mRevision; }
    Status status(const LLUUID& id) const;
    const Entry* getEntry(const LLUUID& id) const;
    bool currentWorldYaw(const LLUUID& id, F32& yaw) const;
    bool ensureBallCentre(const std::vector<LLUUID>& ids);
    std::vector<LLUUID> entryIds() const;
    LLVector3d stagedPositionGlobal(const LLUUID& id) const;

    LLSD exportEntries() const;
    bool importEntries(const LLSD& data);
    void clear(bool restore_roots = false);
    void cleanupSingleton() override;

private:
    typedef std::map<LLUUID, Entry> entry_map_t;

    static LLUUID canonicalId(const LLUUID& id);
    Entry* edit(const LLUUID& id);
    bool editable(const LLUUID& id) const;
    void changed(const char* action, const LLUUID& id = LLUUID::null,
                 bool state_change = false);
    bool keepsLiveImpl(const LLUUID& id) const;
    void apply(LLVOAvatar* avatar);
    void applyGhost(const LLUUID& stable_id, Entry& entry);
    void restoreGhost(const LLUUID& stable_id, Entry& entry);
    void queueRootRestore(const LLUUID& id);
    void applyRootRestore(LLVOAvatar* avatar, const LLUUID& key);
    LLVector3d resolveTrackPoint(const Entry& entry, bool& found) const;
    bool resolvePosition(const LLUUID& id, LLVector3d& position) const;
    bool stockWorldYaw(const LLUUID& id, F32& yaw) const;
    F32 currentWorldYaw(const LLUUID& id, const Entry& entry) const;
    bool canEaseOff(const LLUUID& id, const Entry& entry) const;
    void erase(entry_map_t::iterator found);
    void updateEntryCount();
    void clearBallCentreState(bool notify);
    void clearBallCentreForRegionChange();

    static U32 sEntryCount;
    entry_map_t mEntries;
    std::set<LLUUID> mRestorePending;
    LLVector3d mBallCentre;
    bool mBallCentreValid = false;
    BallCentreSource mBallCentreSource = BallCentreSource::NONE;
    std::set<LLUUID> mBallCentreDancers;
    U64 mBallCentreRegionHandle = 0;
    U64 mRevision = 1;
    U32 mLastUpdateFrame = 0xFFFFFFFF;
    bool mEnabled = true;
    LLUUID mSessionAgent;
};

#endif
