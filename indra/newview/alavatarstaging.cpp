/**
 * @file alavatarstaging.cpp
 * @brief Session-only local avatar blocking service.
 */

#include "llviewerprecompiledheaders.h"

#include "alavatarstaging.h"

#include "alghoststudio.h"
#include "llactormover.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llappviewer.h"
#include "llcontrolavatar.h"
#include "lldirectorcast.h"
#include "llframetimer.h"
#include "lljoint.h"
#include "llpresentationtime.h"
#include "llsdutil_math.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "llvoavatar.h"

#include <cmath>

using namespace ALAvatarStagingModel;

U32 ALAvatarStaging::sEntryCount = 0;

namespace
{
    bool isActivelyApplied(const ALAvatarStaging::Entry& entry)
    {
        return (entry.mHaveBase || entry.mGhostCaptured) &&
               !entry.mMoverOwned;
    }

    // ALGhostStudio is a function-local static (ALGhostStudio::instance()), not an
    // LLSingleton: it always exists and outlives LLSingletonBase::deleteAll().
    inline bool ghostStudioReady() { return true; }
}

namespace
{
    Vec3 model(const LLVector3& value)
    {
        return Vec3(value.mV[VX], value.mV[VY], value.mV[VZ]);
    }

    Vec3 model(const LLVector3d& value)
    {
        return Vec3(value.mdV[VX], value.mdV[VY], value.mdV[VZ]);
    }

    LLVector3 viewer(const Vec3& value)
    {
        return LLVector3((F32)value.x, (F32)value.y, (F32)value.z);
    }

    LLVector3d viewerGlobal(const Vec3& value)
    {
        return LLVector3d(value.x, value.y, value.z);
    }

    F32 quaternionYaw(const LLQuaternion& rotation)
    {
        const LLVector3 forward = LLVector3::x_axis * rotation;
        return atan2f(forward.mV[VY], forward.mV[VX]);
    }

    LLVOAvatar* findAvatar(const LLUUID& id)
    {
        return LLDirectorCast::instance().resolve(id);
    }

    bool closeQuaternion(const LLQuaternion& lhs, const LLQuaternion& rhs)
    {
        return fabsf(dot(lhs, rhs)) > 0.999999f;
    }

    bool ghostStudioOwns(const ALGhostStudio::Instance& ghost)
    {
        return ghost.mKeepFacing ||
               ghost.mCrowdFacing == ALGhostStudio::FACING_TRACK_SUBJECT ||
               ghost.mCrowdFacing == ALGhostStudio::FACING_WAVE ||
               (ghost.mKind == ALGhostStudio::BACKING_ENTITY_CLONE &&
                (ghost.mDriveMode == ALGhostStudio::DRIVE_TRUE_MIRROR ||
                 ghost.mDriveMode == ALGhostStudio::DRIVE_REFLECT)) ||
               ghost.mTurnMode != ALGhostStudio::TURN_MODE_OFF ||
               ghost.mMotion != ALGhostStudio::MOTION_OFF ||
               ghost.mChaosEnabled || ghost.mGroupId.notNull();
    }

    F32 stagingDelta()
    {
        static LLCachedControl<bool> bdmerge_freeze_world(
            gSavedSettings, "BDMergeFreezeWorld", false);
        static LLCachedControl<bool> use_freeze_world(
            gSavedSettings, "UseFreezeWorld", false);
        if (bdmerge_freeze_world && use_freeze_world)
        {
            return 0.f;
        }
        return LLPresentationTime::drives(LLTemporalFeature::ANIMATION)
            ? llclamp(LLPresentationTime::presentationDelta(), 0.f, 0.25f)
            : llclamp(gFrameIntervalSeconds.value(), 0.f, 0.25f);
    }
}

ALAvatarStaging::ALAvatarStaging() = default;

ALAvatarStaging::~ALAvatarStaging()
{
    clear();
}

LLUUID ALAvatarStaging::canonicalId(const LLUUID& id)
{
    if (id.isNull())
    {
        return id;
    }
    if (ghostStudioReady())
    {
        if (const ALGhostStudio::Instance* ghost =
                ALGhostStudio::instance().findInstanceByRuntime(id))
        {
            return ghost->mId;
        }
    }
    return LLDirectorCast::canonicalActorId(id);
}

bool ALAvatarStaging::editable(const LLUUID& id) const
{
    return id.notNull() && id != gAgentID;
}

bool ALAvatarStaging::stockWorldYaw(const LLUUID& id, F32& yaw) const
{
    const LLUUID key = canonicalId(id);
    if (const ALGhostStudio::Instance* ghost =
            ALGhostStudio::instance().getInstance(key))
    {
        yaw = quaternionYaw(ghost->mRotation);
        return true;
    }
    if (LLVOAvatar* avatar = findAvatar(key))
    {
        LLJoint* root = avatar->getRootJoint();
        const LLQuaternion rotation = root
            ? root->getWorldRotation() : avatar->getRotation();
        yaw = quaternionYaw(rotation);
        return true;
    }
    return false;
}

F32 ALAvatarStaging::currentWorldYaw(const LLUUID& id,
                                     const Entry& entry) const
{
    if (isActivelyApplied(entry) && entry.mYawSpring.initialized)
    {
        return (F32)entry.mYawSpring.value;
    }
    F32 yaw = 0.f;
    stockWorldYaw(id, yaw);
    return yaw;
}

bool ALAvatarStaging::currentWorldYaw(const LLUUID& id, F32& yaw) const
{
    const LLUUID key = canonicalId(id);
    auto found = mEntries.find(key);
    if (found != mEntries.end() && isActivelyApplied(found->second) &&
        found->second.mYawSpring.initialized)
    {
        yaw = (F32)found->second.mYawSpring.value;
        return true;
    }
    return stockWorldYaw(key, yaw);
}

ALAvatarStaging::Entry* ALAvatarStaging::edit(const LLUUID& id)
{
    const LLUUID key = canonicalId(id);
    if (!editable(key))
    {
        return nullptr;
    }
    if (mSessionAgent.notNull() && mSessionAgent != gAgentID)
    {
        clear();
    }
    mSessionAgent = gAgentID;
    auto inserted = mEntries.emplace(key, Entry());
    if (inserted.second)
    {
        inserted.first->second.mWasGhost =
            ALGhostStudio::instance().getInstance(key) != nullptr;
        updateEntryCount();
        changed("added", key, true);
    }
    return &inserted.first->second;
}

void ALAvatarStaging::changed(const char* action, const LLUUID& id,
                              bool state_change)
{
    ++mRevision;
    if (state_change)
    {
        LL_INFOS("AvatarStaging") << action <<
            (id.notNull() ? " " + id.asString() : std::string()) << LL_ENDL;
    }
    else
    {
        LL_DEBUGS("AvatarStaging") << action <<
            (id.notNull() ? " " + id.asString() : std::string()) << LL_ENDL;
    }
}

void ALAvatarStaging::updateEntryCount()
{
    sEntryCount = (U32)(mEntries.size() + mRestorePending.size());
}

bool ALAvatarStaging::setYaw(const LLUUID& id, F32 radians)
{
    if (!std::isfinite(radians)) return false;
    Entry* entry = edit(id);
    if (!entry) return false;
    if (entry->mMode == FacingMode::OFFSET)
        entry->mYawOffset = (F32)wrapYaw(radians);
    else
    {
        entry->mLockYaw = (F32)wrapYaw(radians);
        entry->mMode = FacingMode::LOCK;
    }
    entry->mResetting = false;
    changed("yaw", id);
    return true;
}

bool ALAvatarStaging::nudgeYaw(const LLUUID& id, F32 radians)
{
    if (!std::isfinite(radians)) return false;
    Entry* entry = edit(id);
    if (!entry) return false;
    if (entry->mMode == FacingMode::OFFSET)
        entry->mYawOffset = (F32)wrapYaw(entry->mYawOffset + radians);
    else
    {
        if (entry->mMode != FacingMode::LOCK)
            entry->mLockYaw = currentWorldYaw(canonicalId(id), *entry);
        entry->mLockYaw = (F32)wrapYaw(entry->mLockYaw + radians);
        entry->mMode = FacingMode::LOCK;
    }
    entry->mResetting = false;
    changed("yaw nudged", id);
    return true;
}

bool ALAvatarStaging::setMode(const LLUUID& id, FacingMode mode)
{
    Entry* entry = edit(id);
    if (!entry) return false;
    if (mode == FacingMode::LOCK && entry->mMode != FacingMode::LOCK)
        entry->mLockYaw = currentWorldYaw(canonicalId(id), *entry);
    entry->mMode = mode;
    entry->mResetting = false;
    changed("mode", id);
    return true;
}

LLVector3d ALAvatarStaging::resolveTrackPoint(const Entry& entry,
                                              bool& found) const
{
    found = false;
    if (entry.mTrackSource == TrackSource::CAMERA)
    {
        found = true;
        return gAgentCamera.getCameraPositionGlobal();
    }
    if (entry.mTrackSource == TrackSource::SUBJECT_A)
    {
        if (LLVOAvatar* subject = LLDirectorCast::instance().resolveSubjectA())
        {
            found = true;
            return stagedPositionGlobal(subject->getID());
        }
        return LLVector3d::zero;
    }
    if ((entry.mTrackSource == TrackSource::BALL_CENTRE ||
         entry.mTrackSource == TrackSource::AWAY_FROM_CENTRE) &&
        mBallCentreValid)
    {
        found = true;
        if (entry.mTrackSource == TrackSource::BALL_CENTRE)
            return mBallCentre;
        LLVector3d origin;
        return resolvePosition(entry.mTrackTarget, origin)
            ? origin + (origin - mBallCentre) : mBallCentre;
    }
    if (entry.mTrackSource == TrackSource::ACTOR &&
        entry.mTrackTarget.notNull())
    {
        LLVector3d position;
        if (resolvePosition(entry.mTrackTarget, position))
        {
            found = true;
            return position;
        }
    }
    if (entry.mTrackSource == TrackSource::POINT &&
        entry.mTrackPointValid && entry.mTrackPoint.isFinite())
    {
        found = true;
        return entry.mTrackPoint;
    }
    return LLVector3d::zero;
}

bool ALAvatarStaging::resolvePosition(const LLUUID& id,
                                      LLVector3d& position) const
{
    const LLUUID key = canonicalId(id);
    if (ghostStudioReady())
    {
        if (const ALGhostStudio::Instance* ghost =
                ALGhostStudio::instance().getInstance(key))
        {
            auto found = mEntries.find(key);
            position = found != mEntries.end() && found->second.mGhostApplied
                ? found->second.mGhostLastFoot : ghost->mFootGlobal;
            return true;
        }
    }
    if (LLVOAvatar* avatar = findAvatar(key))
    {
        auto found = mEntries.find(key);
        if (found != mEntries.end() && mEnabled && found->second.mOn &&
            !found->second.mMoverOwned &&
            found->second.mOffsetSpring.initialized)
        {
            position = avatar->getPositionGlobal() +
                       viewerGlobal(found->second.mOffsetSpring.value);
        }
        else
        {
            LLJoint* root = avatar->getRootJoint();
            position = root
                ? gAgent.getPosGlobalFromAgent(root->getWorldPosition())
                : avatar->getPositionGlobal();
        }
        return true;
    }
    return false;
}

LLVector3d ALAvatarStaging::stagedPositionGlobal(const LLUUID& id) const
{
    LLVector3d position;
    return resolvePosition(id, position) ? position : LLVector3d::zero;
}

bool ALAvatarStaging::facePoint(const LLUUID& id, const LLVector3d& point,
                                bool keep_facing)
{
    LLVector3d origin;
    if (!point.isFinite() || !resolvePosition(id, origin)) return false;
    Entry* entry = edit(id);
    if (!entry) return false;
    entry->mTrackSource = TrackSource::POINT;
    entry->mTrackTarget.setNull();
    entry->mTrackPoint = point;
    entry->mTrackPointValid = true;
    entry->mMode = keep_facing ? FacingMode::TRACK : FacingMode::LOCK;
    if (!keep_facing)
    {
        entry->mLockYaw = (F32)faceYaw(model(origin), model(point));
    }
    entry->mResetting = false;
    changed(keep_facing ? "tracking point" : "faced point", id);
    return true;
}

bool ALAvatarStaging::faceTarget(const LLUUID& id, const LLUUID& target,
                                 bool keep_facing)
{
    if (target.isNull()) return false;
    LLVector3d origin;
    LLVector3d point;
    if (!resolvePosition(id, origin) || !resolvePosition(target, point))
        return false;
    Entry* entry = edit(id);
    if (!entry) return false;
    entry->mTrackSource = TrackSource::ACTOR;
    entry->mTrackTarget = canonicalId(target);
    entry->mTrackPointValid = false;
    if (!keep_facing)
    {
        entry->mLockYaw = (F32)faceYaw(model(origin), model(point));
    }
    entry->mMode = keep_facing ? FacingMode::TRACK : FacingMode::LOCK;
    entry->mResetting = false;
    changed(keep_facing ? "tracking target" : "faced target", id);
    return true;
}

bool ALAvatarStaging::faceCamera(const LLUUID& id, bool keep_facing)
{
    LLVector3d origin;
    if (!resolvePosition(id, origin)) return false;
    const LLVector3d point = gAgentCamera.getCameraPositionGlobal();
    Entry* entry = edit(id);
    if (!entry) return false;
    entry->mTrackSource = TrackSource::CAMERA;
    entry->mTrackTarget.setNull();
    entry->mTrackPointValid = false;
    entry->mMode = keep_facing ? FacingMode::TRACK : FacingMode::LOCK;
    if (!keep_facing)
        entry->mLockYaw = (F32)faceYaw(model(origin), model(point));
    entry->mResetting = false;
    changed(keep_facing ? "tracking camera" : "faced camera", canonicalId(id));
    return true;
}

bool ALAvatarStaging::faceSubjectA(const LLUUID& id, bool keep_facing)
{
    LLVector3d origin;
    LLVOAvatar* subject = LLDirectorCast::instance().resolveSubjectA();
    if (!subject || !resolvePosition(id, origin)) return false;
    const LLVector3d point = stagedPositionGlobal(subject->getID());
    Entry* entry = edit(id);
    if (!entry) return false;
    entry->mTrackSource = TrackSource::SUBJECT_A;
    entry->mTrackTarget.setNull();
    entry->mTrackPointValid = false;
    entry->mMode = keep_facing ? FacingMode::TRACK : FacingMode::LOCK;
    if (!keep_facing)
        entry->mLockYaw = (F32)faceYaw(model(origin), model(point));
    entry->mResetting = false;
    changed(keep_facing ? "tracking Subject A" : "faced Subject A",
            canonicalId(id));
    return true;
}

bool ALAvatarStaging::faceBallCentre(const LLUUID& id, bool away,
                                     bool keep_facing)
{
    LLVector3d origin;
    if (!mBallCentreValid || !resolvePosition(id, origin)) return false;
    Entry* entry = edit(id);
    if (!entry) return false;
    entry->mTrackSource = away ? TrackSource::AWAY_FROM_CENTRE
                               : TrackSource::BALL_CENTRE;
    entry->mTrackTarget = canonicalId(id);
    entry->mTrackPointValid = false;
    const LLVector3d point = away ? origin + (origin - mBallCentre)
                                  : mBallCentre;
    entry->mMode = keep_facing ? FacingMode::TRACK : FacingMode::LOCK;
    if (!keep_facing)
        entry->mLockYaw = (F32)faceYaw(model(origin), model(point));
    entry->mResetting = false;
    changed(away ? "faced away from centre" : "faced ball centre", id);
    return true;
}

bool ALAvatarStaging::setOffset(const LLUUID& id, const LLVector3& offset)
{
    if (!offset.isFinite()) return false;
    Entry* entry = edit(id);
    if (!entry) return false;
    entry->mOffset = viewer(clampOffset(model(offset)));
    entry->mResetting = false;
    changed("offset", id);
    return true;
}

bool ALAvatarStaging::nudgeOffset(const LLUUID& id, const LLVector3& delta)
{
    if (!delta.isFinite()) return false;
    Entry* entry = edit(id);
    if (!entry) return false;
    if (entry->mPinRequested)
    {
        LLVector3d pin = entry->mPin;
        pin.mdV[VX] += delta.mV[VX];
        pin.mdV[VY] += delta.mV[VY];
        pin.mdV[VZ] += delta.mV[VZ];
        if (entry->mPinAnchorValid)
        {
            const Vec3 relative = clampPinOffset(Vec3(
                pin.mdV[VX] - entry->mPinSimAnchor.mdV[VX],
                pin.mdV[VY] - entry->mPinSimAnchor.mdV[VY],
                pin.mdV[VZ]));
            pin.mdV[VX] = entry->mPinSimAnchor.mdV[VX] + relative.x;
            pin.mdV[VY] = entry->mPinSimAnchor.mdV[VY] + relative.y;
            pin.mdV[VZ] = relative.z;
        }
        entry->mPin = pin;
    }
    else
    {
        entry->mOffset = viewer(clampOffset(model(entry->mOffset + delta)));
    }
    entry->mResetting = false;
    changed("offset nudged", id);
    return true;
}

bool ALAvatarStaging::nudgeLocalOffset(const LLUUID& id,
                                       const LLVector3& local_delta)
{
    if (!local_delta.isFinite()) return false;
    const LLUUID key = canonicalId(id);
    F32 yaw = 0.f;
    if (const Entry* entry = getEntry(key))
        yaw = currentWorldYaw(key, *entry);
    else if (!stockWorldYaw(key, yaw))
        return false;
    return nudgeOffset(key, viewer(localOffsetToWorld(model(local_delta), yaw)));
}

bool ALAvatarStaging::pinAt(const LLUUID& id, const LLVector3d& position)
{
    if (!position.isFinite()) return false;
    Entry* entry = edit(id);
    if (!entry) return false;
    const LLUUID key = canonicalId(id);
    LLVector3d anchor;
    bool have_anchor = false;
    if (ghostStudioReady())
    {
        if (const ALGhostStudio::Instance* ghost =
                ALGhostStudio::instance().getInstance(key))
        {
            anchor = entry->mGhostCaptured
                ? entry->mGhostBaseFoot : ghost->mFootGlobal;
            have_anchor = true;
        }
    }
    if (!have_anchor)
    {
        if (LLVOAvatar* avatar = findAvatar(key))
        {
            anchor = avatar->getPositionGlobal();
            have_anchor = true;
        }
    }
    const double visible_z_offset =
        isActivelyApplied(*entry) && entry->mOffsetSpring.initialized
            ? entry->mOffsetSpring.value.z : 0.0;
    entry->mPinSimAnchor = anchor;
    entry->mPinAnchorValid = have_anchor;
    entry->mPin = viewerGlobal(have_anchor
        ? makePinTarget(model(position), model(anchor), visible_z_offset)
        : Vec3(position.mdV[VX], position.mdV[VY], visible_z_offset));
    entry->mPinRequested = true;
    entry->mPinned = have_anchor;
    entry->mResetting = false;
    changed("pinned", id);
    return true;
}

bool ALAvatarStaging::pinHere(const LLUUID& id)
{
    LLVector3d position;
    return resolvePosition(id, position) && pinAt(id, position);
}

bool ALAvatarStaging::releasePin(const LLUUID& id)
{
    auto found = mEntries.find(canonicalId(id));
    if (found == mEntries.end()) return false;
    found->second.mPinRequested = false;
    found->second.mPinned = false;
    found->second.mPinAnchorValid = false;
    changed("pin released", found->first, true);
    return true;
}

bool ALAvatarStaging::applyFormation(const std::vector<LLUUID>& ids,
                                     Formation formation,
                                     F32 radius_or_spacing, F32 arc_radians)
{
    if (!ensureBallCentre(ids)) return false;
    const LLVector3d centre = mBallCentre;
    const auto slots = formationSlots(formation, ids.size(),
                                      radius_or_spacing, arc_radians);
    bool applied = false;
    for (std::size_t i = 0; i < ids.size(); ++i)
    {
        if (!editable(ids[i])) continue;
        const LLVector3d foot = centre + viewerGlobal(slots[i].foot);
        if (pinAt(ids[i], foot))
        {
            Entry* entry = edit(ids[i]);
            entry->mMode = FacingMode::LOCK;
            entry->mLockYaw = (F32)slots[i].yaw;
            applied = true;
        }
    }
    if (applied) changed("formation");
    return applied;
}

bool ALAvatarStaging::ensureBallCentre(const std::vector<LLUUID>& ids)
{
    clearBallCentreForRegionChange();
    std::set<LLUUID> dancer_ids;
    std::vector<Vec3> feet;
    feet.reserve(ids.size());
    for (const LLUUID& id : ids)
    {
        const LLUUID key = canonicalId(id);
        if (!editable(key)) continue;
        dancer_ids.insert(key);
        LLVector3d position;
        if (resolvePosition(key, position))
            feet.push_back(model(position));
    }
    if (mBallCentreValid && mBallCentreSource != BallCentreSource::CENTROID)
        return true;
    if (mBallCentreValid && reuseFrozenCentroid(
            mBallCentreSource == BallCentreSource::CENTROID,
            dancer_ids == mBallCentreDancers, model(mBallCentre), feet))
    {
        return true;
    }
    Vec3 fallback;
    if (!centroid(feet, fallback))
    {
        if (mBallCentreSource == BallCentreSource::CENTROID)
            clearBallCentre();
        return false;
    }
    mBallCentre = viewerGlobal(fallback);
    mBallCentreValid = true;
    mBallCentreSource = BallCentreSource::CENTROID;
    mBallCentreDancers = dancer_ids;
    mBallCentreRegionHandle = gAgent.getRegion()
        ? gAgent.getRegion()->getHandle() : 0;
    changed("ball centre from centroid", LLUUID::null, true);
    return true;
}

bool ALAvatarStaging::reset(const LLUUID& id, bool eased)
{
    auto found = mEntries.find(canonicalId(id));
    if (found == mEntries.end()) return false;
    if (!eased || !mEnabled || !found->second.mOn ||
        status(found->first) == Status::ABSENT)
    {
        erase(found);
        return true;
    }
    Entry& entry = found->second;
    entry.mMode = FacingMode::OFFSET;
    entry.mYawOffset = 0.f;
    entry.mOffset.clear();
    entry.mPinRequested = false;
    entry.mPinned = false;
    entry.mPinAnchorValid = false;
    entry.mResetting = true;
    entry.mOn = true;
    changed("reset easing", found->first, true);
    return true;
}

void ALAvatarStaging::resetAll(bool eased)
{
    clearBallCentre();
    if (!eased)
    {
        while (!mEntries.empty()) erase(mEntries.begin());
        return;
    }
    const std::vector<LLUUID> ids = entryIds();
    for (const LLUUID& id : ids) reset(id, true);
}

void ALAvatarStaging::restoreGhost(const LLUUID& id, Entry& entry)
{
    if (!entry.mGhostCaptured) return;
    if (ghostStudioReady())
    {
        if (ALGhostStudio::Instance* ghost =
                ALGhostStudio::instance().getInstance(id))
        {
            if (entry.mGhostApplied &&
                ghost->mFootGlobal == entry.mGhostLastFoot &&
                ghost->mRotation == entry.mGhostLastRotation)
            {
                ghost->setTransform(entry.mGhostBaseFoot,
                                    entry.mGhostBaseRotation);
                ALGhostStudio::instance().applyEntityTransform(id);
            }
        }
    }
    entry.mGhostCaptured = false;
    entry.mGhostApplied = false;
}

void ALAvatarStaging::erase(entry_map_t::iterator found)
{
    const LLUUID id = found->first;
    const bool was_ghost = found->second.mWasGhost ||
        found->second.mGhostCaptured ||
        (ghostStudioReady() &&
         ALGhostStudio::instance().getInstance(id));
    const bool already_released = !found->second.mHaveBase &&
        !found->second.mEasingOff;
    restoreGhost(id, found->second);
    if (!was_ghost && !already_released)
    {
        queueRootRestore(id);
    }
    mEntries.erase(found);
    updateEntryCount();
    changed("removed", id, true);
}

bool ALAvatarStaging::remove(const LLUUID& id)
{
    auto found = mEntries.find(canonicalId(id));
    if (found == mEntries.end()) return false;
    erase(found);
    return true;
}

void ALAvatarStaging::setEnabled(bool enabled)
{
    if (mEnabled == enabled) return;
    mEnabled = enabled;
    if (!enabled)
    {
        for (auto& item : mEntries)
        {
            const bool already_released = !item.second.mOn &&
                !item.second.mEasingOff && !item.second.mHaveBase;
            if (canEaseOff(item.first, item.second))
            {
                item.second.mEasingOff = true;
                continue;
            }
            item.second.mHaveBase = false;
            const bool is_ghost = ghostStudioReady() &&
                ALGhostStudio::instance().getInstance(item.first);
            restoreGhost(item.first, item.second);
            if (!is_ghost && !already_released) queueRootRestore(item.first);
        }
    }
    else
    {
        for (auto& item : mEntries)
        {
            if (item.second.mOn) item.second.mEasingOff = false;
        }
    }
    changed(enabled ? "enabled" : "disabled", LLUUID::null, true);
}

bool ALAvatarStaging::setEntryEnabled(const LLUUID& id, bool enabled)
{
    auto found = mEntries.find(canonicalId(id));
    if (found == mEntries.end() || found->second.mOn == enabled) return false;
    found->second.mOn = enabled;
    if (!enabled)
    {
        if (canEaseOff(found->first, found->second))
        {
            found->second.mEasingOff = true;
            changed("entry disabled (easing)", found->first, true);
            return true;
        }
        found->second.mHaveBase = false;
        const bool is_ghost = ghostStudioReady() &&
            ALGhostStudio::instance().getInstance(found->first);
        restoreGhost(found->first, found->second);
        if (!is_ghost) queueRootRestore(found->first);
    }
    else
    {
        found->second.mEasingOff = !mEnabled &&
            canEaseOff(found->first, found->second);
    }
    changed(enabled ? "entry enabled" : "entry disabled", found->first, true);
    return true;
}

bool ALAvatarStaging::canEaseOff(const LLUUID& id, const Entry& entry) const
{
    if (entry.mGhostCaptured)
        return ALGhostStudio::instance().getInstance(id) != nullptr;
    LLVOAvatar* avatar = findAvatar(id);
    return entry.mHaveBase && avatar && !avatar->isDead() &&
           !avatar->isGhostAvatar();
}

void ALAvatarStaging::setBallCentre(const LLVector3d& centre)
{
    if (!centre.isFinite()) return;
    mBallCentre = centre;
    mBallCentreValid = true;
    mBallCentreSource = BallCentreSource::SET;
    mBallCentreDancers.clear();
    mBallCentreRegionHandle = gAgent.getRegion()
        ? gAgent.getRegion()->getHandle() : 0;
    changed("ball centre");
}

void ALAvatarStaging::clearBallCentreState(bool notify)
{
    const bool had_centre = mBallCentreValid ||
        mBallCentreSource != BallCentreSource::NONE ||
        !mBallCentreDancers.empty();
    mBallCentre = LLVector3d::zero;
    mBallCentreValid = false;
    mBallCentreSource = BallCentreSource::NONE;
    mBallCentreDancers.clear();
    mBallCentreRegionHandle = 0;
    if (notify && had_centre) changed("ball centre cleared", LLUUID::null, true);
}

void ALAvatarStaging::clearBallCentre()
{
    clearBallCentreState(true);
}

void ALAvatarStaging::clearBallCentreForRegionChange()
{
    if (!mBallCentreValid || !mBallCentreRegionHandle || !gAgent.getRegion())
        return;
    if (mBallCentreRegionHandle != gAgent.getRegion()->getHandle())
        clearBallCentreState(true);
}

const ALAvatarStaging::Entry* ALAvatarStaging::getEntry(const LLUUID& id) const
{
    auto found = mEntries.find(canonicalId(id));
    return found == mEntries.end() ? nullptr : &found->second;
}

std::vector<LLUUID> ALAvatarStaging::entryIds() const
{
    std::vector<LLUUID> result;
    result.reserve(mEntries.size());
    for (const auto& item : mEntries) result.push_back(item.first);
    return result;
}

ALAvatarStaging::Status ALAvatarStaging::status(const LLUUID& id) const
{
    const LLUUID key = canonicalId(id);
    auto found = mEntries.find(key);
    if (found == mEntries.end()) return Status::ABSENT;
    if (!mEnabled || !found->second.mOn) return Status::OFF;
    if (ghostStudioReady())
    {
        if (const ALGhostStudio::Instance* ghost =
                ALGhostStudio::instance().getInstance(key))
        {
            if (LLActorMover::instance().isMoving(ghost->mEntityId) ||
                LLActorMover::instance().isDriving(ghost->mEntityId))
                return Status::SUSPENDED_MOVER;
            return ghostStudioOwns(*ghost)
                ? Status::GHOST_STUDIO_OWNS : Status::GHOST;
        }
    }
    LLVOAvatar* avatar = findAvatar(key);
    if (!avatar || avatar->isDead()) return Status::ABSENT;
    if (avatar->isVisuallyMuted()) return Status::MUTED;
    if (LLActorMover::instance().isMoving(avatar->getID()) ||
        LLActorMover::instance().isDriving(avatar->getID()) ||
        LLActorMover::instance().isMoving(key) ||
        LLActorMover::instance().isDriving(key))
        return Status::SUSPENDED_MOVER;
    if (avatar->isSitting() && avatar->getParent() &&
        found->second.mPinRequested)
        return Status::SUSPENDED_SEATED_PIN;
    if (found->second.mPinRequested && !found->second.mPinned)
        return Status::PIN_RELEASED;
    return Status::ACTIVE;
}

bool ALAvatarStaging::keepsLiveImpl(const LLUUID& id) const
{
    const LLUUID key = canonicalId(id);
    if (mRestorePending.count(key)) return true;
    auto found = mEntries.find(key);
    return found != mEntries.end() &&
           ((mEnabled && found->second.mOn) || found->second.mEasingOff);
}

void ALAvatarStaging::queueRootRestore(const LLUUID& id)
{
    LLVOAvatar* avatar = id.notNull() ? findAvatar(id) : nullptr;
    if (avatar && !avatar->isDead() && !avatar->isGhostAvatar())
    {
        mRestorePending.insert(id);
        updateEntryCount();
    }
}

void ALAvatarStaging::applyRootRestore(LLVOAvatar* avatar, const LLUUID& key)
{
    auto pending = mRestorePending.find(key);
    if (pending == mRestorePending.end() || !avatar || avatar->isDead()) return;
    LLJoint* root = avatar->getRootJoint();
    if (!root) return;
    const bool seated = avatar->isSitting() && avatar->getParent();
    if (!seated && !avatar->isControlAvatar())
    {
        const F32 yaw = quaternionYaw(avatar->getRotation());
        root->setWorldRotation(LLQuaternion(yaw, LLVector3::z_axis));
    }
    mRestorePending.erase(pending);
    updateEntryCount();
}

void ALAvatarStaging::update()
{
    clearBallCentreForRegionChange();
    if (sEntryCount == 0) return;
    const U32 frame = LLFrameTimer::getFrameCount();
    if (mLastUpdateFrame == frame) return;
    mLastUpdateFrame = frame;
    if (mSessionAgent.notNull() && mSessionAgent != gAgentID)
    {
        clear();
        return;
    }

    LLActorMover& mover = LLActorMover::instance();
    for (auto pending = mRestorePending.begin();
         pending != mRestorePending.end();)
    {
        LLVOAvatar* avatar = findAvatar(*pending);
        if (!avatar || avatar->isDead() || avatar->isGhostAvatar())
            pending = mRestorePending.erase(pending);
        else
            ++pending;
    }
    updateEntryCount();
    for (auto iterator = mEntries.begin(); iterator != mEntries.end();)
    {
        auto current = iterator++;
        const LLUUID id = current->first;
        Entry& entry = current->second;
        ALGhostStudio::Instance* ghost = ghostStudioReady()
            ? ALGhostStudio::instance().getInstance(id) : nullptr;
        if (ghost) entry.mWasGhost = true;
        LLVOAvatar* avatar = ghost ? nullptr : findAvatar(id);
        const LLUUID runtime_id = ghost ? ghost->mEntityId
            : (avatar ? avatar->getID() : id);
        const bool mover_owned = mover.isMoving(runtime_id) ||
                                  mover.isDriving(runtime_id) ||
                                  mover.isMoving(id) || mover.isDriving(id);
        if (entry.mMoverKnown && entry.mMoverOwned && !mover_owned)
        {
            entry.mMoverReleased = true;
            changed("mover released", id, true);
        }
        entry.mMoverKnown = true;
        entry.mMoverOwned = mover_owned;

        if (!ghost)
        {
            if (entry.mWasGhost || entry.mGhostCaptured)
            {
                erase(current);
                continue;
            }
            if (!avatar && entry.mResetting)
            {
                erase(current);
            }
            continue;
        }

        const bool studio_owns = ghostStudioOwns(*ghost);
        if (studio_owns != entry.mGhostStudioOwned)
        {
            entry.mGhostStudioOwned = studio_owns;
            changed(studio_owns ? "Ghost Studio owns" : "Ghost Studio released",
                    id, true);
        }
        if (((!mEnabled || !entry.mOn) && !entry.mEasingOff) ||
            mover_owned || studio_owns)
        {
            restoreGhost(id, entry);
            if (entry.mResetting && (!mEnabled || !entry.mOn))
            {
                erase(current);
            }
            continue;
        }

        applyGhost(id, entry);
        if (entry.mEasingOff && entry.mGhostCaptured &&
            distance(entry.mOffsetSpring.value, Vec3()) < 0.001 &&
            std::fabs(shortestArc(quaternionYaw(entry.mGhostBaseRotation),
                                  entry.mYawSpring.value)) < 0.001)
        {
            restoreGhost(id, entry);
            entry.mEasingOff = false;
            continue;
        }
        if (entry.mResetting && entry.mGhostCaptured &&
            distance(entry.mOffsetSpring.value, Vec3()) < 0.001 &&
            std::fabs(shortestArc(quaternionYaw(entry.mGhostBaseRotation),
                                  entry.mYawSpring.value)) < 0.001)
        {
            erase(current);
        }
    }
}

void ALAvatarStaging::applyGhost(const LLUUID& id, Entry& entry)
{
    if (!ghostStudioReady()) return;
    ALGhostStudio::Instance* ghost = ALGhostStudio::instance().getInstance(id);
    if (!ghost) return;
    if (!entry.mGhostCaptured)
    {
        entry.mGhostCaptured = true;
        entry.mGhostBaseFoot = ghost->mFootGlobal;
        entry.mGhostBaseRotation = ghost->mRotation;
        entry.mGhostApplied = false;
        entry.mGhostLastWriteRevision = ghost->mTransformRevision;
        snap(entry.mOffsetSpring, Vec3());
        snap(entry.mYawSpring, quaternionYaw(ghost->mRotation));
    }
    else if (entry.mGhostApplied &&
             (ghost->mTransformRevision != entry.mGhostLastWriteRevision ||
              ghost->mFootGlobal != entry.mGhostLastFoot ||
              !closeQuaternion(ghost->mRotation, entry.mGhostLastRotation)))
    {
        const bool unchanged = ghost->mFootGlobal == entry.mGhostLastFoot &&
            closeQuaternion(ghost->mRotation, entry.mGhostLastRotation);
        if (!unchanged)
        {
            entry.mGhostBaseFoot = ghost->mFootGlobal -
                viewerGlobal(entry.mOffsetSpring.value);
            const F32 external_yaw_delta = (F32)shortestArc(
                quaternionYaw(entry.mGhostLastRotation),
                quaternionYaw(ghost->mRotation));
            const F32 current_delta = (F32)shortestArc(
                quaternionYaw(entry.mGhostBaseRotation),
                entry.mYawSpring.value);
            entry.mGhostBaseRotation = ghost->mRotation *
                LLQuaternion(-current_delta, LLVector3::z_axis);
            entry.mYawSpring.value = wrapYaw(
                entry.mYawSpring.value + external_yaw_delta);
        }
        entry.mGhostLastWriteRevision = ghost->mTransformRevision;
    }

    const F32 dt = stagingDelta();
    Vec3 target_offset = entry.mEasingOff ? Vec3() : model(entry.mOffset);
    if (!entry.mEasingOff && entry.mPinRequested)
    {
        if (!entry.mPinAnchorValid)
        {
            entry.mPinSimAnchor = entry.mGhostBaseFoot;
            entry.mPinAnchorValid = true;
            entry.mPin = viewerGlobal(makePinTarget(model(entry.mPin),
                model(entry.mPinSimAnchor), entry.mPin.mdV[VZ]));
            entry.mPinned = true;
        }
        const bool pinned = updatePinState(true, entry.mPinned,
                                           model(entry.mGhostBaseFoot),
                                           model(entry.mPinSimAnchor));
        if (pinned != entry.mPinned)
        {
            entry.mPinned = pinned;
            changed(pinned ? "pin resumed" : "pin auto-released", id, true);
        }
        if (entry.mPinned)
            target_offset = pinTargetOffset(model(entry.mPin),
                                            model(entry.mGhostBaseFoot));
    }
    Vec3 stepped_offset = stepCritical(entry.mOffsetSpring, target_offset, dt);
    const Vec3 offset = stepped_offset;
    const LLVector3d foot = entry.mGhostBaseFoot + viewerGlobal(offset);
    double target_yaw = quaternionYaw(entry.mGhostBaseRotation);
    if (!entry.mEasingOff && entry.mMode == FacingMode::OFFSET)
        target_yaw += entry.mYawOffset;
    else if (!entry.mEasingOff && entry.mMode == FacingMode::LOCK)
        target_yaw = entry.mLockYaw;
    else if (!entry.mEasingOff && entry.mMode == FacingMode::TRACK)
    {
        bool found = false;
        const LLVector3d target = resolveTrackPoint(entry, found);
        if (found) target_yaw = faceYaw(model(foot), model(target), target_yaw);
    }
    const F32 yaw = (F32)stepCritical(entry.mYawSpring, target_yaw, dt,
                                      TAU_SECONDS, MAX_YAW_RATE, true);
    const F32 base_heading = quaternionYaw(entry.mGhostBaseRotation);
    const LLQuaternion rotation = entry.mGhostBaseRotation * LLQuaternion(
        (F32)shortestArc(base_heading, yaw), LLVector3::z_axis);
    if (!entry.mGhostApplied || foot != entry.mGhostLastFoot ||
        !closeQuaternion(rotation, entry.mGhostLastRotation))
    {
        ghost->setTransform(foot, rotation);
        ALGhostStudio::instance().applyEntityTransform(id);
        entry.mGhostLastFoot = foot;
        entry.mGhostLastRotation = rotation;
        entry.mGhostLastWriteRevision = ghost->mTransformRevision;
        entry.mGhostApplied = true;
    }
}

void ALAvatarStaging::apply(LLVOAvatar* avatar)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_AVATAR;
    if (!avatar || avatar->isSelf()) return;
    if (mSessionAgent.notNull() && mSessionAgent != gAgentID)
    {
        clear();
        return;
    }

    const LLUUID key = canonicalId(avatar->getID());
    applyRootRestore(avatar, key);
    if (avatar->isGhostAvatar())
    {
        // Ghost transforms are driven once from update(), after Ghost Studio's
        // own per-frame drivers have run. Never write their root from here.
        return;
    }
    auto found = mEntries.find(key);
    if (found == mEntries.end()) return;
    Entry& entry = found->second;
    if (((!mEnabled || !entry.mOn) && !entry.mEasingOff) || entry.mMoverOwned)
        return;
    LLJoint* root = avatar->getRootJoint();
    if (avatar->isDead() || !root) return;

    const U32 frame = LLFrameTimer::getFrameCount();
    const bool seated = avatar->isSitting() && avatar->getParent();
    LLQuaternion seat_rotation;
    if (seated)
    {
        if (const LLViewerObject* seat =
                dynamic_cast<const LLViewerObject*>(avatar->getParent()))
            seat_rotation = seat->getRenderRotation();
    }
    const LLVector3 current_position = seated
        ? root->getPosition() : root->getWorldPosition();
    const LLQuaternion current_rotation = seated
        ? root->getRotation() : root->getWorldRotation();
    if (entry.mLastFrame == frame)
    {
        if (current_position == entry.mLastWrittenPosition &&
            closeQuaternion(current_rotation, entry.mLastWrittenRotation))
        {
            return;
        }

        // A later stock writer (notably Animesh matchVolumeTransform) supplied
        // a fresher base in this frame. Recompose the already-stepped delta;
        // do not advance the springs or reassert an absolute stale transform.
        LLVector3 written_position;
        LLQuaternion written_rotation;
        if (seated)
        {
            written_position = current_position +
                entry.mFrameOffset * ~seat_rotation;
            written_rotation = current_rotation * seat_rotation *
                LLQuaternion(entry.mFrameYawDelta, LLVector3::z_axis) *
                ~seat_rotation;
            root->setPosition(written_position);
            root->setRotation(written_rotation);
        }
        else if (avatar->isControlAvatar())
        {
            written_position = current_position + entry.mFrameOffset;
            written_rotation = current_rotation *
                LLQuaternion(entry.mFrameYawDelta, LLVector3::z_axis);
            root->setWorldPosition(written_position);
            root->setWorldRotation(written_rotation);
        }
        else
        {
            written_position = current_position + entry.mFrameOffset;
            written_rotation = LLQuaternion(entry.mFrameYaw,
                                             LLVector3::z_axis);
            root->setWorldPosition(written_position);
            root->setWorldRotation(written_rotation);
        }
        entry.mLastBasePosition = current_position;
        entry.mLastBaseRotation = current_rotation;
        entry.mLastWrittenPosition = written_position;
        entry.mLastWrittenRotation = written_rotation;
        return;
    }

    const bool repeated_override = entry.mHaveBase &&
        current_position == entry.mLastWrittenPosition &&
        closeQuaternion(current_rotation, entry.mLastWrittenRotation);
    LLVector3 base_position = repeated_override
        ? entry.mLastBasePosition : current_position;
    LLQuaternion base_rotation = repeated_override
        ? entry.mLastBaseRotation : current_rotation;
    const LLVector3d base_global = avatar->getPositionGlobal();
    const F32 server_yaw = seated
        ? quaternionYaw(avatar->getRotation() * seat_rotation)
        : quaternionYaw(avatar->getRotation());
    const bool discontinuity = entry.mHaveBase &&
        (isDiscontinuity(model(entry.mLastBaseGlobal), model(base_global)) ||
         seated != entry.mWasSeated || entry.mMoverReleased);

    if (!entry.mHaveBase)
    {
        snap(entry.mOffsetSpring, Vec3());
        snap(entry.mBaseYawSpring, server_yaw);
        // Preserve the currently visible heading on enrollment. Stock may
        // legitimately hold the root up to 45 degrees off getRotation().
        snap(entry.mYawSpring, quaternionYaw(root->getWorldRotation()));
    }
    else if (discontinuity)
    {
        snap(entry.mOffsetSpring, Vec3());
        snap(entry.mBaseYawSpring, server_yaw);
        snap(entry.mYawSpring, server_yaw);
    }
    entry.mHaveBase = true;
    entry.mLastBasePosition = base_position;
    entry.mLastBaseGlobal = base_global;
    entry.mLastBaseRotation = base_rotation;
    entry.mWasSeated = seated;
    entry.mMoverReleased = false;
    entry.mLastFrame = frame;

    const F32 dt = stagingDelta();
    Vec3 target_offset = entry.mEasingOff ? Vec3() : model(entry.mOffset);
    if (!entry.mEasingOff && entry.mPinRequested && !seated)
    {
        const LLVector3d sim_position = avatar->getPositionGlobal();
        if (!entry.mPinAnchorValid)
        {
            entry.mPinSimAnchor = sim_position;
            entry.mPinAnchorValid = true;
            entry.mPin = viewerGlobal(makePinTarget(model(entry.mPin),
                model(entry.mPinSimAnchor), entry.mPin.mdV[VZ]));
            entry.mPinned = true;
        }
        const bool anchored = updatePinState(true, entry.mPinned,
                                             model(sim_position),
                                             model(entry.mPinSimAnchor));
        if (anchored != entry.mPinned)
        {
            entry.mPinned = anchored;
            changed(anchored ? "pin resumed" : "pin auto-released", key, true);
        }
        if (entry.mPinned)
        {
            // A pin anchors the avatar/foot coordinate, not the pelvis-height
            // root. Shift the stock root by the same delta as the sim position
            // so client height and animation-derived root height are retained.
            target_offset = pinTargetOffset(model(entry.mPin),
                                            model(sim_position));
        }
    }
    Vec3 stepped_offset = stepCritical(entry.mOffsetSpring, target_offset, dt);
    const LLVector3 offset = viewer(stepped_offset);

    const double base_yaw = stepCritical(entry.mBaseYawSpring, server_yaw, dt,
                                         0.4, MAX_YAW_RATE, true);
    double target_yaw = seated ? server_yaw : base_yaw;
    if (!entry.mEasingOff && entry.mMode == FacingMode::OFFSET)
        target_yaw += entry.mYawOffset;
    else if (!entry.mEasingOff && entry.mMode == FacingMode::LOCK)
        target_yaw = entry.mLockYaw;
    else if (!entry.mEasingOff && entry.mMode == FacingMode::TRACK)
    {
        bool have_target = false;
        const LLVector3d target = resolveTrackPoint(entry, have_target);
        if (have_target)
        {
            const LLVector3d origin = avatar->getPositionGlobal() +
                                      LLVector3d(offset);
            target_yaw = faceYaw(model(origin), model(target), server_yaw);
        }
    }
    const F32 yaw = (F32)stepCritical(entry.mYawSpring, target_yaw, dt,
                                      TAU_SECONDS, MAX_YAW_RATE, true);

    LLVector3 written_position;
    LLQuaternion written_rotation;
    if (seated)
    {
        written_position = base_position + offset * ~seat_rotation;
        const F32 delta = (F32)shortestArc(server_yaw, yaw);
        written_rotation = base_rotation * seat_rotation *
            LLQuaternion(delta, LLVector3::z_axis) * ~seat_rotation;
        root->setPosition(written_position);
        root->setRotation(written_rotation);
        entry.mFrameYawDelta = delta;
    }
    else if (avatar->isControlAvatar())
    {
        written_position = base_position + offset;
        const F32 control_base_yaw = quaternionYaw(base_rotation);
        written_rotation = base_rotation * LLQuaternion(
            (F32)shortestArc(control_base_yaw, yaw), LLVector3::z_axis);
        root->setWorldPosition(written_position);
        root->setWorldRotation(written_rotation);
        entry.mFrameYawDelta = (F32)shortestArc(control_base_yaw, yaw);
    }
    else
    {
        written_position = base_position + offset;
        written_rotation = LLQuaternion(yaw, LLVector3::z_axis);
        root->setWorldPosition(written_position);
        root->setWorldRotation(written_rotation);
        entry.mFrameYawDelta = (F32)shortestArc(server_yaw, yaw);
    }
    entry.mFrameOffset = offset;
    entry.mFrameYaw = yaw;
    entry.mLastWrittenPosition = written_position;
    entry.mLastWrittenRotation = written_rotation;

    const bool offset_moving = distance(entry.mOffsetSpring.value,
                                        target_offset) > 0.001 ||
        distance(entry.mOffsetSpring.velocity, Vec3()) > 0.001;
    if (discontinuity || offset_moving)
    {
        ALPosePolish::ContactHints hints;
        hints.mFrame = frame;
        hints.mMoverOwnsRoot = true;
        hints.mGrounded = !avatar->mInAir;
        hints.mDiscontinuity = discontinuity;
        if (offset_moving)
        {
            hints.mExpectedPlant[0] = 0.f;
            hints.mExpectedPlant[1] = 0.f;
        }
        avatar->setMoverContactHints(hints);
    }
    if (entry.mEasingOff && offset.lengthSquared() < 1e-6f &&
        fabs(shortestArc(server_yaw, yaw)) < 0.001)
    {
        entry.mEasingOff = false;
        entry.mHaveBase = false;
        queueRootRestore(key);
    }
    else if (entry.mResetting && offset.lengthSquared() < 1e-6f &&
        fabs(shortestArc(server_yaw, yaw)) < 0.001)
    {
        erase(found);
    }
}

LLSD ALAvatarStaging::exportEntries() const
{
    LLSD data;
    data["version"] = 2;
    if (mBallCentreValid)
        data["ball_centre"] = ll_sd_from_vector3d(mBallCentre);
    data["blocking"] = LLSD::emptyArray();
    for (const auto& item : mEntries)
    {
        const Entry& entry = item.second;
        LLSD value;
        value["id"] = item.first;
        value["mode"] = (S32)entry.mMode;
        value["yaw_offset"] = entry.mYawOffset;
        value["lock_yaw"] = entry.mLockYaw;
        value["track_source"] = (S32)entry.mTrackSource;
        value["track_target"] = entry.mTrackTarget;
        value["track_point"] = ll_sd_from_vector3d(entry.mTrackPoint);
        value["track_point_valid"] = entry.mTrackPointValid;
        value["offset"] = ll_sd_from_vector3(entry.mOffset);
        value["pin"] = ll_sd_from_vector3d(entry.mPin);
        value["pin_z_offset"] = entry.mPin.mdV[VZ];
        value["pin_requested"] = entry.mPinRequested;
        value["on"] = entry.mOn;
        data["blocking"].append(value);
    }
    return data;
}

bool ALAvatarStaging::importEntries(const LLSD& data)
{
    const S32 version = data["version"].asInteger();
    if (version != 1 && version != 2)
        return false;
    if (version == 1)
    {
        clear(true);
        changed("imported v1 (empty blocking)", LLUUID::null, true);
        return true;
    }
    if (!data["blocking"].isArray()) return false;
    entry_map_t imported;
    for (LLSD::array_const_iterator iterator = data["blocking"].beginArray();
         iterator != data["blocking"].endArray(); ++iterator)
    {
        const LLSD& value = *iterator;
        const LLUUID id = canonicalId(value["id"].asUUID());
        if (!editable(id)) continue;
        Entry entry;
        entry.mWasGhost = ALGhostStudio::instance().getInstance(id) != nullptr;
        const S32 mode = value["mode"].asInteger();
        entry.mMode = mode == 1 ? FacingMode::LOCK :
                      mode == 2 ? FacingMode::TRACK : FacingMode::OFFSET;
        entry.mYawOffset = (F32)wrapYaw(value["yaw_offset"].asReal());
        entry.mLockYaw = (F32)wrapYaw(value["lock_yaw"].asReal());
        entry.mTrackTarget = canonicalId(value["track_target"].asUUID());
        const S32 track_source = value.has("track_source")
            ? value["track_source"].asInteger()
            : (entry.mTrackTarget.notNull() ? 1 : 0);
        entry.mTrackSource = track_source == 1 ? TrackSource::ACTOR :
                             track_source == 2 ? TrackSource::CAMERA :
                             track_source == 3 ? TrackSource::SUBJECT_A :
                             track_source == 4 ? TrackSource::BALL_CENTRE :
                             track_source == 5 ? TrackSource::AWAY_FROM_CENTRE :
                                                 TrackSource::POINT;
        entry.mTrackPoint = ll_vector3d_from_sd(value["track_point"]);
        entry.mTrackPointValid = value["track_point_valid"].asBoolean() &&
                                 entry.mTrackPoint.isFinite();
        entry.mOffset = viewer(clampOffset(model(
            ll_vector3_from_sd(value["offset"]))));
        entry.mPin = ll_vector3d_from_sd(value["pin"]);
        entry.mPin.mdV[VZ] = value.has("pin_z_offset")
            ? clampFinite(value["pin_z_offset"].asReal(), -MAX_OFFSET_METERS,
                          MAX_OFFSET_METERS, 0.0)
            : 0.0;
        entry.mPinRequested = value["pin_requested"].asBoolean() &&
                               entry.mPin.isFinite();
        entry.mPinned = false;
        entry.mPinAnchorValid = false;
        entry.mOn = !value.has("on") || value["on"].asBoolean();
        if (imported.size() < 256) imported[id] = entry;
    }
    LLVector3d centre = ll_vector3d_from_sd(data["ball_centre"]);
    const bool centre_valid = data.has("ball_centre") && centre.isFinite();
    for (auto& item : mEntries)
    {
        auto replacement = imported.find(item.first);
        if (replacement != imported.end())
        {
            Entry& fresh = replacement->second;
            Entry& old = item.second;
            fresh.mBaseYawSpring = old.mBaseYawSpring;
            fresh.mYawSpring = old.mYawSpring;
            fresh.mOffsetSpring = old.mOffsetSpring;
            fresh.mLastBasePosition = old.mLastBasePosition;
            fresh.mLastBaseGlobal = old.mLastBaseGlobal;
            fresh.mLastBaseRotation = old.mLastBaseRotation;
            fresh.mLastWrittenPosition = old.mLastWrittenPosition;
            fresh.mLastWrittenRotation = old.mLastWrittenRotation;
            fresh.mHaveBase = old.mHaveBase;
            fresh.mWasSeated = old.mWasSeated;
            fresh.mMoverKnown = old.mMoverKnown;
            fresh.mMoverOwned = old.mMoverOwned;
            fresh.mLastFrame = old.mLastFrame;
            fresh.mFrameOffset = old.mFrameOffset;
            fresh.mFrameYawDelta = old.mFrameYawDelta;
            fresh.mFrameYaw = old.mFrameYaw;
            fresh.mGhostCaptured = old.mGhostCaptured;
            fresh.mWasGhost = old.mWasGhost;
            fresh.mGhostBaseFoot = old.mGhostBaseFoot;
            fresh.mGhostBaseRotation = old.mGhostBaseRotation;
            fresh.mGhostLastFoot = old.mGhostLastFoot;
            fresh.mGhostLastRotation = old.mGhostLastRotation;
            fresh.mGhostLastWriteRevision = old.mGhostLastWriteRevision;
            fresh.mGhostApplied = old.mGhostApplied;
            fresh.mEasingOff = old.mEasingOff && (!fresh.mOn || !mEnabled);
            const bool restore_immediately =
                (fresh.mEasingOff && !canEaseOff(item.first, fresh)) ||
                (old.mOn && !fresh.mOn && !fresh.mEasingOff);
            if (restore_immediately)
            {
                const bool was_ghost = old.mGhostCaptured ||
                    ALGhostStudio::instance().getInstance(item.first);
                restoreGhost(item.first, old);
                if (!was_ghost) queueRootRestore(item.first);
                fresh.mEasingOff = false;
                fresh.mGhostCaptured = false;
                fresh.mGhostApplied = false;
                fresh.mHaveBase = false;
            }
            continue;
        }
        const bool is_ghost = ALGhostStudio::instance().getInstance(item.first);
        restoreGhost(item.first, item.second);
        if (!is_ghost) queueRootRestore(item.first);
    }
    for (const auto& item : imported)
    {
        if (mEnabled && item.second.mOn) mRestorePending.erase(item.first);
    }
    mEntries.swap(imported);
    mBallCentre = centre_valid ? centre : LLVector3d::zero;
    mBallCentreValid = centre_valid;
    mBallCentreSource = centre_valid
        ? BallCentreSource::SET : BallCentreSource::NONE;
    mBallCentreDancers.clear();
    mBallCentreRegionHandle = centre_valid && gAgent.getRegion()
        ? gAgent.getRegion()->getHandle() : 0;
    updateEntryCount();
    mSessionAgent = gAgentID;
    changed("imported", LLUUID::null, true);
    return true;
}

void ALAvatarStaging::clear(bool restore_roots)
{
    const bool had_centre = mBallCentreValid;
    if (mEntries.empty() && mRestorePending.empty() && !had_centre) return;
    for (auto& item : mEntries)
    {
        const bool already_released = !item.second.mHaveBase &&
            !item.second.mEasingOff;
        const bool is_ghost = ghostStudioReady() &&
            ALGhostStudio::instance().getInstance(item.first);
        restoreGhost(item.first, item.second);
        if (restore_roots && !is_ghost && !already_released)
            queueRootRestore(item.first);
    }
    mEntries.clear();
    if (!restore_roots) mRestorePending.clear();
    updateEntryCount();
    mSessionAgent.setNull();
    clearBallCentreState(false);
    changed("cleared", LLUUID::null, true);
}

void ALAvatarStaging::cleanupSingleton()
{
    clear();
}
