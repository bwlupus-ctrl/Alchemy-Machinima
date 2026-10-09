/**
 * @file aldanceball.cpp
 * @brief Session-local, client-only multi-avatar dance playback engine.
 */

#include "llviewerprecompiledheaders.h"

#include "aldanceball.h"

#include "alghostanimassetindex.h"
#include "alghoststudio.h"
#include "llactormover.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llappviewer.h"
#include "llcontrolavatar.h"
#include "lldirectorcast.h"
#include "llkeyframemotion.h"
#include "llmotion.h"
#include "llmutelist.h"
#include "llpresentationtime.h"
#include "llviewerobjectlist.h"
#include "llviewerobject.h"
#include "llvoavatar.h"
#include "llvoavatarself.h"
#include "llworld.h"
#include "lltimer.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace
{
    F64 finite_nonnegative(F64 value, F64 fallback = 0.0)
    {
        return std::isfinite(value) && value >= 0.0 ? value : fallback;
    }

    F64 clamp01(F64 value)
    {
        return std::clamp(std::isfinite(value) ? value : 0.0, 0.0, 1.0);
    }

    const char* verdict(ALDanceBall::DancerState state)
    {
        switch (state)
        {
        case ALDanceBall::DancerState::PLAYING: return "PLAYING";
        case ALDanceBall::DancerState::PENDING_ASSET: return "PENDING_ASSET";
        case ALDanceBall::DancerState::ASSET_FAILED: return "ASSET_FAILED";
        case ALDanceBall::DancerState::IN_USE: return "IN_USE";
        case ALDanceBall::DancerState::SERVER_OWNS_UUID: return "SERVER_OWNS_UUID";
        case ALDanceBall::DancerState::SKIPPED_SEATED: return "SKIPPED_SEATED";
        case ALDanceBall::DancerState::ABSENT: return "ABSENT";
        case ALDanceBall::DancerState::TAKEN_OVER: return "TAKEN_OVER";
        default: return nullptr;
        }
    }

    S32 sanitize_priority(S32 priority)
    {
        return priority == 4 || priority == 5 || priority == 6 ? priority : -1;
    }

    LLUUID canonical_dancer_id(const LLUUID& input)
    {
        if (const ALGhostStudio::Instance* instance =
                ALGhostStudio::instance().findInstanceByRuntime(input))
        {
            return instance->mId;
        }
        return LLDirectorCast::canonicalActorId(input);
    }
}

ALDanceBall& ALDanceBall::instance()
{
    static ALDanceBall dance_ball;
    return dance_ball;
}

ALDanceBall::~ALDanceBall()
{
}

void ALDanceBall::MotionHandle::clear()
{
    mAnim.setNull();
    mToken.setNull();
    mOwnsMotion = false;
    mControlsMotion = false;
    mPlaybackStarted = false;
    mPrioritySaved = false;
    mSavedPriority = -1;
    mItem = -1;
    mStartedAt = 0.0;
    mUnitStartedAt = 0.0;
    mDelay = 0.0;
    mDuration = mLoopIn = mLoopOut = 0.0;
    mWeight = 0.f;
    mLoop = true;
    mState = State::PENDING;
    mMix = ALDanceBallModel::LegWeight();
    mSampleClock = ALDanceBallModel::SampleClock();
    mSampleInitialized = false;
    mSharedSync = false;
    mSharedSyncError = 0.0;
    mSharedSyncElapsed = 0.0;
}

void ALDanceBall::setEnabled(bool enabled)
{
    if (mEnabled == enabled)
    {
        return;
    }
    mEnabled = enabled;
    if (enabled && gAgent.getRegion())
    {
        mHadWorld = true;
    }
    ++mStatusRevision;
    if (!mEnabled)
    {
        for (auto& pair : mRuntime) pair.second.mManualItem = -1;
        releaseAll();
        mScheduler.reset();
    }
}

void ALDanceBall::setFrozen(bool frozen)
{
    if (mClock.frozen() != frozen)
    {
        mClock.setFrozen(frozen);
        ++mStatusRevision;
    }
}

void ALDanceBall::setSpeed(F64 speed)
{
    mTiming.mSpeed = std::clamp(finite_nonnegative(speed, 1.0), 0.25, 4.0);
    mClock.setSpeed(mTiming.mSpeed);
    ++mStatusRevision;
}

void ALDanceBall::rampSpeed(F64 target_speed, F64 seconds,
                            ALDanceBallModel::RampCurve curve)
{
    mTiming.mSpeed = std::clamp(finite_nonnegative(target_speed, mTiming.mSpeed),
                                0.25, 4.0);
    mClock.rampTo(mTiming.mSpeed, finite_nonnegative(seconds), curve);
    ++mStatusRevision;
}

void ALDanceBall::reshuffle()
{
    ++mReshuffle;
    ++mStatusRevision;
}

std::vector<ALDanceBallModel::PlaylistItem> ALDanceBall::modelPlaylist() const
{
    std::vector<ALDanceBallModel::PlaylistItem> result;
    result.reserve(mPlaylist.size());
    for (const PlaylistEntry& entry : mPlaylist)
    {
        ALDanceBallModel::PlaylistItem item;
        item.mAvailable = entry.mStatus != AssetStatus::UNAVAILABLE;
        result.push_back(item);
    }
    return result;
}

ALDanceBallModel::SchedulerConfig ALDanceBall::schedulerConfig() const
{
    ALDanceBallModel::SchedulerConfig config;
    config.mMode = mTiming.mMode;
    config.mAutoAdvance = mTiming.mAutoAdvance;
    config.mIntervalSeconds = mTiming.mAdvanceSeconds;
    config.mJitterSeconds = mTiming.mAdvanceJitterSeconds;
    config.mSeed = mTiming.mSeed;
    return config;
}

void ALDanceBall::next()
{
    for (auto& pair : mRuntime) pair.second.mManualItem = -1;
    const auto items = modelPlaylist();
    const auto config = schedulerConfig();
    std::set<U64> addressed;
    for (const Dancer& dancer : mRoster)
    {
        const U64 key = config.mMode == ALDanceBallModel::PlaylistMode::SAME_FOR_ALL
            ? 0ULL : dancer.mId.getDigest64();
        if (addressed.insert(key).second)
        {
            mScheduler.next(mClock.time(), key, items, config);
        }
    }
    ++mStatusRevision;
}

void ALDanceBall::previous()
{
    for (auto& pair : mRuntime) pair.second.mManualItem = -1;
    const auto items = modelPlaylist();
    const auto config = schedulerConfig();
    std::set<U64> addressed;
    for (const Dancer& dancer : mRoster)
    {
        const U64 key = config.mMode == ALDanceBallModel::PlaylistMode::SAME_FOR_ALL
            ? 0ULL : dancer.mId.getDigest64();
        if (addressed.insert(key).second)
        {
            mScheduler.previous(mClock.time(), key, items, config);
        }
    }
    ++mStatusRevision;
}

bool ALDanceBall::play(std::size_t playlist_index)
{
    std::set<LLUUID> all;
    for (const Dancer& dancer : mRoster) all.insert(dancer.mId);
    return play(playlist_index, all);
}

bool ALDanceBall::play(std::size_t playlist_index,
                       const std::set<LLUUID>& selected_dancers)
{
    if (playlist_index >= mPlaylist.size() ||
        mPlaylist[playlist_index].mStatus == AssetStatus::UNAVAILABLE)
    {
        return false;
    }
    if (mRoster.empty()) return false;
    if (selected_dancers.empty())
    {
        std::set<LLUUID> all;
        for (const Dancer& dancer : mRoster) all.insert(dancer.mId);
        return play(playlist_index, all);
    }
    const auto items = modelPlaylist();
    const auto config = schedulerConfig();
    const bool all_selected = std::all_of(mRoster.begin(), mRoster.end(),
        [&](const Dancer& dancer)
        { return selected_dancers.count(dancer.mId) != 0; });
    if (config.mMode == ALDanceBallModel::PlaylistMode::SAME_FOR_ALL &&
        !all_selected)
    {
        const ALDanceBallModel::Selection shared =
            mScheduler.update(mClock.time(), 0, items, config);
        bool any = false;
        for (const Dancer& dancer : mRoster)
        {
            if (!selected_dancers.count(dancer.mId)) continue;
            Runtime& runtime = mRuntime[dancer.mId];
            runtime.mKind = dancer.mKind;
            runtime.mManualItem = static_cast<S32>(playlist_index);
            runtime.mManualBaseItem = shared.mItem;
            runtime.mManualBoundary = runtime.mCurrentItem ==
                static_cast<S32>(playlist_index)
                    ? runtime.mCurrentBoundary : mClock.time();
            any = true;
        }
        if (any) ++mStatusRevision;
        return any;
    }
    for (auto& pair : mRuntime) pair.second.mManualItem = -1;
    std::set<U64> addressed;
    for (const Dancer& dancer : mRoster)
    {
        if (!selected_dancers.count(dancer.mId)) continue;
        const U64 key = config.mMode == ALDanceBallModel::PlaylistMode::SAME_FOR_ALL
            ? 0ULL : dancer.mId.getDigest64();
        if (addressed.insert(key).second)
        {
            mScheduler.select(mClock.time(), key,
                              static_cast<S32>(playlist_index), items, config);
        }
    }
    const bool played = !addressed.empty();
    if (played) ++mStatusRevision;
    return played;
}

S32 ALDanceBall::currentItem(const LLUUID& dancer_id) const
{
    const auto found = mRuntime.find(canonical_dancer_id(dancer_id));
    return found == mRuntime.end() ? -1 : found->second.mCurrentItem;
}

std::string ALDanceBall::currentLabel(const LLUUID& dancer_id) const
{
    const S32 item = currentItem(dancer_id);
    return item >= 0 && static_cast<std::size_t>(item) < mPlaylist.size()
        ? mPlaylist[static_cast<std::size_t>(item)].mLabel : std::string();
}

S32 ALDanceBall::sameForAllCurrentItem() const
{
    return mScheduler.current(0, ALDanceBallModel::PlaylistMode::SAME_FOR_ALL);
}

std::string ALDanceBall::sameForAllCurrentLabel() const
{
    const S32 item = sameForAllCurrentItem();
    return item >= 0 && static_cast<std::size_t>(item) < mPlaylist.size()
        ? mPlaylist[static_cast<std::size_t>(item)].mLabel : std::string();
}

bool ALDanceBall::addDancer(const LLUUID& input_id)
{
    LLVOAvatar* avatar = dynamic_cast<LLVOAvatar*>(
        gObjectList.findObject(input_id));
    return addDancer(input_id, classify(input_id, avatar));
}

bool ALDanceBall::addDancer(const LLUUID& input_id, DancerKind kind)
{
    LLUUID id = input_id;
    include(id); // an explicit add always reverses a prior exclusion
    if (kind == DancerKind::SELF)
    {
        if (!mIncludeSelf || !isAgentAvatarValid())
        {
            return false;
        }
        id = gAgentID;
    }
    else if (kind == DancerKind::ANIMESH || kind == DancerKind::RESIDENT)
    {
        id = LLDirectorCast::canonicalActorId(id);
        if (LLVOAvatar* resolved = LLDirectorCast::instance().resolve(id))
        {
            kind = classify(id, resolved);
        }
    }
    else if (kind == DancerKind::GHOST)
    {
        // Runtime clone UUIDs may be regenerated with the entity. Store the
        // Studio instance UUID so an absent/recreated clone resumes.
        const ALGhostStudio::Instance* instance =
            ALGhostStudio::instance().findInstanceByRuntime(id);
        if (!instance)
        {
            instance = ALGhostStudio::instance().getInstance(id);
        }
        if (!instance)
        {
            return false;
        }
        id = instance->mId;
    }
    if (id.isNull())
    {
        return false;
    }
    const auto found = std::find_if(mRoster.begin(), mRoster.end(),
        [&](const Dancer& dancer) { return dancer.mId == id; });
    if (found != mRoster.end())
    {
        return false;
    }
    Dancer dancer;
    dancer.mId = id;
    dancer.mShortId = id.asString().substr(0, 8);
    dancer.mKind = kind;
    mRoster.push_back(dancer);
    auto existing_runtime = mRuntime.find(id);
    if (existing_runtime != mRuntime.end())
        existing_runtime->second.mGhostTakenOver = false;
    if (gAgent.getRegion()) mHadWorld = true;
    ++mRosterRevision;
    return true;
}

bool ALDanceBall::removeDancer(const LLUUID& id)
{
    const LLUUID canonical_id = canonical_dancer_id(id);
    auto found = std::find_if(mRoster.begin(), mRoster.end(),
        [&](const Dancer& dancer) { return dancer.mId == canonical_id; });
    if (found == mRoster.end())
    {
        return false;
    }
    auto runtime = mRuntime.find(found->mId);
    if (runtime != mRuntime.end())
    {
        releaseRuntime(*found, runtime->second);
    }
    mScheduler.erase(found->mId.getDigest64(), mTiming.mMode);
    ALAvatarStaging::instance().remove(found->mId);
    mRoster.erase(found);
    ++mRosterRevision;
    return true;
}

void ALDanceBall::setBlockingEnabled(bool enabled)
{
    ALAvatarStaging::instance().setEnabled(enabled);
}

bool ALDanceBall::blockingEnabled() const
{
    return ALAvatarStaging::instance().enabled();
}

U64 ALDanceBall::stagingRevision() const
{
    return ALAvatarStaging::instance().revision();
}

const ALAvatarStaging::Entry* ALDanceBall::blockingEntry(const LLUUID& id) const
{
    return ALAvatarStaging::instance().getEntry(canonical_dancer_id(id));
}

ALAvatarStaging::Status ALDanceBall::blockingStatus(const LLUUID& id) const
{
    return ALAvatarStaging::instance().status(canonical_dancer_id(id));
}

bool ALDanceBall::blockingWorldYaw(const LLUUID& id, F32& yaw) const
{
    return ALAvatarStaging::instance().currentWorldYaw(
        canonical_dancer_id(id), yaw);
}

bool ALDanceBall::setBlockingYaw(const LLUUID& id, F32 radians)
{
    return ALAvatarStaging::instance().setYaw(canonical_dancer_id(id), radians);
}

bool ALDanceBall::nudgeBlockingYaw(const LLUUID& id, F32 radians)
{
    return ALAvatarStaging::instance().nudgeYaw(canonical_dancer_id(id), radians);
}

bool ALDanceBall::setBlockingMode(const LLUUID& id,
                                  ALAvatarStaging::FacingMode mode)
{
    return ALAvatarStaging::instance().setMode(canonical_dancer_id(id), mode);
}

bool ALDanceBall::setDancerBlocking(const LLUUID& id, bool enabled)
{
    const LLUUID key = canonical_dancer_id(id);
    if (enabled && !ALAvatarStaging::instance().getEntry(key))
        return ALAvatarStaging::instance().setYaw(key, 0.f);
    return ALAvatarStaging::instance().setEntryEnabled(key, enabled);
}

bool ALDanceBall::faceBlockingCamera(const LLUUID& id, bool keep)
{ return ALAvatarStaging::instance().faceCamera(canonical_dancer_id(id), keep); }
bool ALDanceBall::faceBlockingSubjectA(const LLUUID& id, bool keep)
{ return ALAvatarStaging::instance().faceSubjectA(canonical_dancer_id(id), keep); }
bool ALDanceBall::faceBlockingCentre(const LLUUID& id, bool away, bool keep)
{ return ALAvatarStaging::instance().faceBallCentre(canonical_dancer_id(id), away, keep); }
bool ALDanceBall::faceBlockingTarget(const LLUUID& id, const LLUUID& target, bool keep)
{ return ALAvatarStaging::instance().faceTarget(canonical_dancer_id(id), canonical_dancer_id(target), keep); }
bool ALDanceBall::nudgeBlockingLocal(const LLUUID& id, const LLVector3& delta)
{ return ALAvatarStaging::instance().nudgeLocalOffset(canonical_dancer_id(id), delta); }
bool ALDanceBall::pinBlockingHere(const LLUUID& id)
{ return ALAvatarStaging::instance().pinHere(canonical_dancer_id(id)); }
bool ALDanceBall::releaseBlockingPin(const LLUUID& id)
{ return ALAvatarStaging::instance().releasePin(canonical_dancer_id(id)); }
bool ALDanceBall::resetBlocking(const LLUUID& id)
{ return ALAvatarStaging::instance().reset(canonical_dancer_id(id)); }
void ALDanceBall::resetAllBlocking()
{ ALAvatarStaging::instance().resetAll(); }

bool ALDanceBall::setBlockingCentre(AutoJoinFocus focus)
{
    LLVector3d point;
    if (!focalPoint(focus, point)) return false;
    ALAvatarStaging::instance().setBallCentre(point);
    return true;
}

void ALDanceBall::clearBlockingCentre()
{
    ALAvatarStaging::instance().clearBallCentre();
}

bool ALDanceBall::hasBlockingCentre() const
{
    return ALAvatarStaging::instance().hasBallCentre();
}

ALAvatarStaging::BallCentreSource ALDanceBall::blockingCentreSource() const
{
    return ALAvatarStaging::instance().ballCentreSource();
}

std::size_t ALDanceBall::blockingCentreDancerCount() const
{
    return ALAvatarStaging::instance().ballCentreDancerCount();
}

bool ALDanceBall::ensureBlockingCentre(const std::vector<LLUUID>& ids)
{
    std::vector<LLUUID> canonical_ids;
    canonical_ids.reserve(ids.size());
    for (const LLUUID& id : ids) canonical_ids.push_back(canonical_dancer_id(id));
    return ALAvatarStaging::instance().ensureBallCentre(canonical_ids);
}

bool ALDanceBall::applyBlockingFormation(const std::vector<LLUUID>& ids,
    ALAvatarStagingModel::Formation formation, F32 spacing)
{
    return ALAvatarStaging::instance().applyFormation(ids, formation, spacing);
}

void ALDanceBall::exclude(const LLUUID& id)
{
    const LLUUID canonical_id = canonical_dancer_id(id);
    if (canonical_id.notNull() && mExcluded.insert(canonical_id).second)
    {
        removeDancer(canonical_id);
        ++mRosterRevision;
    }
}

void ALDanceBall::include(const LLUUID& id)
{
    if (mExcluded.erase(canonical_dancer_id(id)))
    {
        ++mRosterRevision;
    }
}

bool ALDanceBall::isExcluded(const LLUUID& id) const
{
    return mExcluded.count(canonical_dancer_id(id)) != 0;
}

ALDanceBall::DancerKind ALDanceBall::classify(const LLUUID& id,
                                              LLVOAvatar* avatar) const
{
    if (id == gAgentID || (avatar && avatar->isSelf()))
    {
        return DancerKind::SELF;
    }
    if ((avatar && avatar->isGhostAvatar()) ||
        ALGhostStudio::instance().findInstanceByRuntime(id) ||
        ALGhostStudio::instance().getInstance(id))
    {
        return DancerKind::GHOST;
    }
    return avatar && avatar->isControlAvatar()
        ? DancerKind::ANIMESH : DancerKind::RESIDENT;
}

bool ALDanceBall::focalPoint(AutoJoinFocus focus, LLVector3d& point) const
{
    switch (focus)
    {
    case AutoJoinFocus::CAMERA_FOCUS:
        point = gAgentCamera.getFocusGlobal();
        return true;
    case AutoJoinFocus::SUBJECT_A:
        if (LLVOAvatar* avatar = LLDirectorCast::instance().resolveSubjectA())
        {
            point = avatar->getPositionGlobal();
            return true;
        }
        return false;
    case AutoJoinFocus::ME:
    default:
        if (isAgentAvatarValid())
        {
            point = gAgent.getPositionGlobal();
            return true;
        }
        return false;
    }
}

U32 ALDanceBall::addNearby(F32 radius_meters, AutoJoinFocus focus)
{
    if (!LLWorld::instanceExists())
    {
        return 0;
    }
    LLVector3d focal;
    if (!focalPoint(focus, focal))
    {
        return 0;
    }
    uuid_vec_t ids;
    LLWorld::getInstance()->getAvatars(&ids, nullptr, focal,
        std::max(0.f, radius_meters));
    U32 added = 0;
    for (const LLUUID& runtime_id : ids)
    {
        LLVOAvatar* avatar = dynamic_cast<LLVOAvatar*>(
            gObjectList.findObject(runtime_id));
        DancerKind kind = classify(runtime_id, avatar);
        LLUUID id = kind == DancerKind::ANIMESH
            ? LLDirectorCast::canonicalActorId(runtime_id) : runtime_id;
        if (kind == DancerKind::SELF && !mIncludeSelf)
        {
            continue;
        }
        added += addDancer(id, kind) ? 1U : 0U;
    }
    return added;
}

U32 ALDanceBall::addCast()
{
    U32 added = 0;
    for (const LLUUID& id : LLDirectorCast::instance().getIds())
    {
        LLVOAvatar* avatar = LLDirectorCast::instance().resolve(id);
        added += addDancer(id, classify(id, avatar)) ? 1U : 0U;
    }
    return added;
}

void ALDanceBall::setAutoJoin(bool enabled, F32 radius_meters,
                              AutoJoinFocus focus)
{
    mAutoJoin = enabled;
    mAutoJoinRadius = std::max(0.f, radius_meters);
    mAutoJoinFocus = focus;
    ++mStatusRevision;
}

void ALDanceBall::setIncludeSeated(bool include)
{
    if (mIncludeSeated != include)
    {
        mIncludeSeated = include;
        ++mScanGeneration;
        ++mStatusRevision;
    }
}

void ALDanceBall::setIncludeSelf(bool include)
{
    if (mIncludeSelf == include)
    {
        return;
    }
    mIncludeSelf = include;
    if (!include)
    {
        const auto found = std::find_if(mRoster.begin(), mRoster.end(),
            [](const Dancer& dancer)
            { return dancer.mKind == DancerKind::SELF; });
        if (found != mRoster.end())
        {
            removeDancer(found->mId);
        }
    }
    ++mStatusRevision;
}

std::string ALDanceBall::defaultLabel(const LLUUID& asset_id) const
{
    if (const ALGhostAnimAssetIndex::Entry* indexed =
            ALGhostAnimAssetIndex::instance().find(asset_id))
    {
        if (!indexed->mName.empty())
        {
            return indexed->mName;
        }
    }
    return "Anim " + asset_id.asString().substr(0, 8);
}

bool ALDanceBall::addAnimation(const LLUUID& asset_id,
                               const std::string& label)
{
    constexpr std::size_t MAX_ANIMATIONS = 1024;
    if (asset_id.isNull() || mPlaylist.size() >= MAX_ANIMATIONS ||
        std::any_of(mPlaylist.begin(), mPlaylist.end(),
            [&](const PlaylistEntry& entry)
            { return entry.mAssetId == asset_id; }))
    {
        return false;
    }
    PlaylistEntry entry;
    entry.mAssetId = asset_id;
    entry.mLabel = label.empty() ? defaultLabel(asset_id) : label;
    if (const ALGhostAnimAssetIndex::Entry* indexed =
            ALGhostAnimAssetIndex::instance().find(asset_id))
    {
        if (indexed->mAvailability == ALGhostAnimAssetIndex::AVAILABLE_UNAVAILABLE)
        {
            entry.mStatus = AssetStatus::UNAVAILABLE;
        }
    }
    ALGhostAnimAssetIndex::instance().requestMetadata(asset_id);
    mPlaylist.push_back(entry);
    if (gAgent.getRegion()) mHadWorld = true;
    ++mPlaylistRevision;
    return true;
}

bool ALDanceBall::removeAnimation(std::size_t index)
{
    if (index >= mPlaylist.size())
    {
        return false;
    }
    std::vector<S32> remap(mPlaylist.size(), -1);
    for (std::size_t i = 0; i < mPlaylist.size(); ++i)
    {
        if (i < index) remap[i] = static_cast<S32>(i);
        else if (i > index) remap[i] = static_cast<S32>(i - 1);
    }
    mPlaylist.erase(mPlaylist.begin() + index);
    remapPlaylist(remap);
    ++mPlaylistRevision;
    return true;
}

bool ALDanceBall::renameAnimation(std::size_t index, const std::string& label)
{
    if (index >= mPlaylist.size() || label.empty() ||
        mPlaylist[index].mLabel == label)
    {
        return false;
    }
    mPlaylist[index].mLabel = label;
    ++mPlaylistRevision;
    return true;
}

bool ALDanceBall::moveAnimation(std::size_t from, std::size_t to)
{
    if (from >= mPlaylist.size() || to >= mPlaylist.size() || from == to)
    {
        return false;
    }
    std::vector<S32> remap(mPlaylist.size(), -1);
    for (std::size_t i = 0; i < mPlaylist.size(); ++i)
    {
        if (i == from) remap[i] = static_cast<S32>(to);
        else if (from < to && i > from && i <= to)
            remap[i] = static_cast<S32>(i - 1);
        else if (to < from && i >= to && i < from)
            remap[i] = static_cast<S32>(i + 1);
        else remap[i] = static_cast<S32>(i);
    }
    PlaylistEntry moved = mPlaylist[from];
    mPlaylist.erase(mPlaylist.begin() + from);
    mPlaylist.insert(mPlaylist.begin() + to, moved);
    remapPlaylist(remap);
    ++mPlaylistRevision;
    return true;
}

void ALDanceBall::clearPlaylist()
{
    if (!mPlaylist.empty())
    {
        const std::vector<S32> remap(mPlaylist.size(), -1);
        mPlaylist.clear();
        remapPlaylist(remap);
        ++mPlaylistRevision;
    }
}

void ALDanceBall::setTiming(const Timing& input)
{
    Timing timing = input;
    timing.mAdvanceSeconds = std::max(0.001,
        finite_nonnegative(timing.mAdvanceSeconds, 8.0));
    timing.mAdvanceJitterSeconds = std::clamp(
        finite_nonnegative(timing.mAdvanceJitterSeconds), 0.0,
        timing.mAdvanceSeconds * 0.49);
    timing.mDesync = clamp01(timing.mDesync);
    timing.mSpeed = std::clamp(finite_nonnegative(timing.mSpeed, 1.0), 0.25, 4.0);
    timing.mSpeedJitter = clamp01(timing.mSpeedJitter);
    timing.mCrossfadeSeconds = finite_nonnegative(timing.mCrossfadeSeconds, 0.5);
    timing.mPriority = sanitize_priority(timing.mPriority);
    if (timing.mSeed == 0)
    {
        timing.mSeed = ALDanceBallModel::DEFAULT_SEED;
    }
    const bool speed_changed = timing.mSpeed != mTiming.mSpeed;
    const bool mode_changed = timing.mMode != mTiming.mMode;
    const bool face_changed =
        timing.mDanceDrivesFace != mTiming.mDanceDrivesFace;
    mTiming = timing;
    if (speed_changed)
    {
        mClock.setSpeed(mTiming.mSpeed);
    }
    if (mode_changed)
    {
        for (auto& pair : mRuntime) pair.second.mManualItem = -1;
    }
    if (face_changed)
    {
        const LLMotion::PresentationJointPredicate exclusion =
            mTiming.mDanceDrivesFace ? nullptr : &LLMotion::isFaceJoint;
        for (auto& pair : mRuntime)
        {
            Dancer dancer;
            dancer.mId = pair.first;
            dancer.mKind = pair.second.mKind;
            LLVOAvatar* avatar = resolve(dancer);
            if (!avatar)
            {
                continue;
            }
            for (MotionHandle& leg : pair.second.mLegs)
            {
                if (LLMotion* motion = avatar->findMotion(leg.mAnim))
                {
                    motion->setPresentationJointExclusion(
                        leg.mToken, exclusion);
                }
            }
        }
    }
    ++mStatusRevision;
}

LLVOAvatar* ALDanceBall::resolve(const Dancer& dancer) const
{
    if (dancer.mKind == DancerKind::SELF)
    {
        return isAgentAvatarValid() ? gAgentAvatarp : nullptr;
    }
    if (dancer.mKind == DancerKind::GHOST)
    {
        const ALGhostStudio::Instance* instance =
            ALGhostStudio::instance().getInstance(dancer.mId);
        return instance ? dynamic_cast<LLVOAvatar*>(
            gObjectList.findObject(instance->mEntityId)) : nullptr;
    }
    LLViewerObject* object = gObjectList.findObject(dancer.mId);
    LLVOAvatar* avatar = object ? object->asAvatar() : nullptr;
    if (!avatar && object)
    {
        LLViewerObject* root = object->getRootEdit();
        avatar = root ? root->getControlAvatar() : nullptr;
    }
    return avatar && !avatar->isDead() ? avatar : nullptr;
}

ALDanceBall::DancerState ALDanceBall::skipState(const Dancer& dancer,
                                                LLVOAvatar* avatar) const
{
    if (!avatar || avatar->isDead())
    {
        return DancerState::ABSENT;
    }
    if (!mIncludeSeated && avatar->isSitting())
    {
        return DancerState::SKIPPED_SEATED;
    }
    if (LLMuteList::getInstance()->isMuted(avatar->getID()) ||
        avatar->getOverallAppearance() != LLVOAvatar::AOA_NORMAL)
    {
        return DancerState::SKIPPED_MUTED;
    }
    if (LLActorMover::instance().isMoving(avatar->getID()))
    {
        return DancerState::SKIPPED_MOVING;
    }
    return DancerState::IDLE;
}

bool ALDanceBall::claimGhost(const Dancer& dancer, Runtime& runtime)
{
    if (dancer.mKind != DancerKind::GHOST) return true;
    ALGhostStudio& studio = ALGhostStudio::instance();
    const ALGhostStudio::Instance* instance = studio.getInstance(dancer.mId);
    if (!instance || !studio.resolveEntityClone(dancer.mId)) return false;
    if (!runtime.mGhostClaimed)
    {
        runtime.mGhostClaimed = true;
        runtime.mGhostInstance = instance->mId;
        runtime.mGhostPriorMode = static_cast<S32>(instance->mDriveMode);
        runtime.mGhostPriorAnim = instance->mDirectedAnim;
        runtime.mGhostPriorLoopMode = static_cast<S32>(instance->mLoopMode);
        runtime.mGhostDirected = false;
    }
    return true;
}

bool ALDanceBall::directGhost(const Dancer& dancer, Runtime& runtime,
                              const LLUUID& animation)
{
    if (!claimGhost(dancer, runtime)) return false;
    ALGhostStudio& studio = ALGhostStudio::instance();
    const ALGhostStudio::Instance* instance = studio.getInstance(dancer.mId);
    if (!instance) return false;
    const ALGhostStudio::ELoopMode prior_loop = instance->mLoopMode;
    bool changed_loop = false;
    if (instance->mLoopMode != ALGhostStudio::LOOP_RETRIGGER)
    {
        studio.setInstanceLoopMode(instance->mId,
                                   ALGhostStudio::LOOP_RETRIGGER);
        changed_loop = true;
    }
    if (instance->mDriveMode != ALGhostStudio::DRIVE_DIRECTED ||
        instance->mDirectedAnim != animation)
    {
        if (!studio.setInstanceDriveMode(instance->mId,
                ALGhostStudio::DRIVE_DIRECTED, animation))
        {
            if (changed_loop)
                studio.setInstanceLoopMode(instance->mId, prior_loop);
            return false;
        }
    }
    runtime.mGhostExpectedAnim = animation;
    runtime.mGhostDirected = true;
    return true;
}

void ALDanceBall::remapPlaylist(const std::vector<S32>& old_to_new)
{
    mScheduler.remapItems(old_to_new);
    auto remap_next = [&](S32 item)
    {
        if (item < 0 || old_to_new.empty()) return -1;
        for (std::size_t offset = 0; offset < old_to_new.size(); ++offset)
        {
            const std::size_t old_index =
                (static_cast<std::size_t>(item) + offset) % old_to_new.size();
            if (old_to_new[old_index] >= 0) return old_to_new[old_index];
        }
        return -1;
    };
    for (auto& pair : mRuntime)
    {
        Runtime& runtime = pair.second;
        for (MotionHandle& handle : runtime.mLegs)
        {
            if (!handle.valid()) continue;
            handle.mItem = handle.mItem >= 0 &&
                static_cast<std::size_t>(handle.mItem) < old_to_new.size()
                    ? old_to_new[static_cast<std::size_t>(handle.mItem)] : -1;
        }
        runtime.mCurrentItem = runtime.mCurrentItem >= 0 &&
            static_cast<std::size_t>(runtime.mCurrentItem) < old_to_new.size()
                ? old_to_new[static_cast<std::size_t>(runtime.mCurrentItem)] : -1;
        runtime.mManualItem = remap_next(runtime.mManualItem);
        runtime.mManualBaseItem = remap_next(runtime.mManualBaseItem);
    }
}

void ALDanceBall::setDancerState(Dancer& dancer, Runtime& runtime,
                                 DancerState state)
{
    const bool present = state != DancerState::ABSENT;
    if (dancer.mState != state || dancer.mPresent != present)
    {
        dancer.mState = state;
        dancer.mPresent = present;
        ++mStatusRevision;
    }
    if (runtime.mLoggedState != state)
    {
        runtime.mLoggedState = state;
        if (const char* text = verdict(state))
        {
            LL_INFOS("DanceBall") << text << " dancer=" << dancer.mId << LL_ENDL;
        }
    }
}

void ALDanceBall::restoreGhost(Runtime& runtime)
{
    if (runtime.mGhostClaimed)
    {
        if (!runtime.mGhostDirected)
        {
            runtime.mGhostClaimed = false;
            runtime.mGhostInstance.setNull();
            runtime.mGhostPriorAnim.setNull();
            runtime.mGhostExpectedAnim.setNull();
            return;
        }
        const ALGhostStudio::Instance* instance =
            ALGhostStudio::instance().getInstance(runtime.mGhostInstance);
        const bool gone = !instance;
        const bool live = !gone &&
            ALGhostStudio::instance().resolveEntityClone(runtime.mGhostInstance);
        bool restored = false;
        if (live)
        {
            ALGhostStudio::instance().setInstanceLoopMode(
                runtime.mGhostInstance,
                static_cast<ALGhostStudio::ELoopMode>(runtime.mGhostPriorLoopMode));
            restored = ALGhostStudio::instance().setInstanceDriveMode(
                runtime.mGhostInstance,
                static_cast<ALGhostStudio::EDriveMode>(runtime.mGhostPriorMode),
                runtime.mGhostPriorAnim);
        }
        if (gone || restored)
        {
            runtime.mGhostClaimed = false;
            runtime.mGhostDirected = false;
            runtime.mGhostInstance.setNull();
            runtime.mGhostPriorAnim.setNull();
            runtime.mGhostExpectedAnim.setNull();
        }
    }
}

void ALDanceBall::beginTransition(Runtime& runtime, std::size_t incoming)
{
    std::vector<ALDanceBallModel::LegWeight> weights;
    weights.reserve(runtime.mLegs.size());
    for (const MotionHandle& leg : runtime.mLegs) weights.push_back(leg.mMix);
    ALDanceBallModel::retargetLegWeights(weights, incoming);
    for (std::size_t i = 0; i < weights.size(); ++i)
    {
        runtime.mLegs[i].mMix = weights[i];
        runtime.mLegs[i].mWeight = static_cast<F32>(weights[i].mCurrent);
        runtime.mLegs[i].mState = i == incoming
            ? MotionHandle::State::PENDING : MotionHandle::State::FADING_OUT;
    }
    runtime.mTransitionActive = false;
}

void ALDanceBall::finishReleaseMotion(LLVOAvatar* avatar, MotionHandle& handle)
{
    if (!handle.valid()) return;
    if (avatar)
    {
        bool still_ours = false;
        const bool signaled = avatar->isAnyAnimationSignaled(&handle.mAnim, 1);
        if (LLMotion* motion = avatar->findMotion(handle.mAnim))
        {
            if (!signaled && handle.mOwnsMotion &&
                avatar->isMotionActive(handle.mAnim) &&
                !motion->hasPresentationControl() &&
                motion->claimPresentationControl(handle.mToken))
            {
                motion->setOwnerManagedLifetime(handle.mToken, true);
                motion->setPresentationJointExclusion(handle.mToken,
                    mTiming.mDanceDrivesFace ? nullptr : &LLMotion::isFaceJoint);
                handle.mControlsMotion = true;
            }
            still_ours = handle.mControlsMotion &&
                motion->isPresentationControlledBy(handle.mToken);
            if (still_ours)
            {
                motion->setPresentationWeight(handle.mToken, 0.f);
                if (handle.mOwnsMotion)
                {
                    llassert(!avatar->isSelf() ||
                             motion->hasOwnerManagedLifetime());
                }
                motion->setOwnerManagedLifetime(handle.mToken, false);
                if (handle.mPrioritySaved)
                {
                    motion->setPriorityOverride(handle.mSavedPriority);
                }
            }
        }
        if (handle.mOwnsMotion && still_ours && !signaled)
        {
            // LLCharacter-qualified and immediate: this cannot enter the
            // LLVOAvatarSelf/LLAgent simulator request path.
            avatar->LLCharacter::stopMotion(handle.mAnim, true);
        }
        if (LLMotion* motion = avatar->findMotion(handle.mAnim))
        {
            if (motion->isPresentationControlledBy(handle.mToken))
            {
                motion->releasePresentationControl(handle.mToken);
            }
        }
    }
    handle.clear();
}

void ALDanceBall::updateLegs(const Dancer& dancer, Runtime& runtime,
                             F64, bool releasing)
{
    if (!releasing || !runtime.mReleaseActive) return;
    LLVOAvatar* avatar = resolve(dancer);
    const F64 duration = std::max(0.0, mTiming.mCrossfadeSeconds);
    const F64 progress = duration > 0.0
        ? clamp01((mFadeTime - runtime.mTransitionStartedAt) / duration) : 1.0;
    for (auto it = runtime.mLegs.begin(); it != runtime.mLegs.end(); )
    {
        MotionHandle& handle = *it;
        if (avatar && handle.mOwnsMotion &&
            !avatar->isAnyAnimationSignaled(&handle.mAnim, 1) &&
            avatar->isMotionActive(handle.mAnim))
        {
            if (LLMotion* motion = avatar->findMotion(handle.mAnim))
            {
                if (!motion->hasPresentationControl() &&
                    motion->claimPresentationControl(handle.mToken))
                {
                    motion->setOwnerManagedLifetime(handle.mToken, true);
                    motion->setPresentationJointExclusion(handle.mToken,
                        mTiming.mDanceDrivesFace ? nullptr : &LLMotion::isFaceJoint);
                    handle.mControlsMotion = true;
                }
            }
        }
        handle.mMix.mCurrent = handle.mMix.mStart * (1.0 - progress);
        handle.mWeight = static_cast<F32>(handle.mMix.mCurrent);
        handle.mSampleClock.advance(mTransportDelta, mTransportDelta);
        ALDanceBallModel::Clip clip;
        clip.mLoop = handle.mLoop;
        clip.mDuration = handle.mDuration;
        clip.mLoopIn = handle.mLoopIn;
        clip.mLoopOut = handle.mLoopOut;
        const F64 sample = ALDanceBallModel::mapSample(
            clip, handle.mSampleClock.time(), 1.0, 0.0).mTime;
        DancerState state = DancerState::IDLE;
        if (!handle.mPlaybackStarted || handle.mWeight <= 0.0001f ||
            !ensureMotion(avatar, handle, handle.mWeight, sample, true, state) ||
            progress >= 1.0)
        {
            finishReleaseMotion(avatar, handle);
            it = runtime.mLegs.erase(it);
        }
        else
        {
            ++it;
        }
    }
    if (runtime.mLegs.empty())
    {
        runtime.mReleaseActive = false;
        if (runtime.mGhostClaimed && !runtime.mGhostTakenOver) restoreGhost(runtime);
    }
}

void ALDanceBall::releaseRuntime(const Dancer& dancer, Runtime& runtime)
{
    if (runtime.mReleaseActive) return;
    // Put the prior Studio performance underneath our fade before reducing
    // any Dance Ball weight. Absent ghosts retain the pending restore record.
    if (dancer.mKind == DancerKind::GHOST && !runtime.mGhostTakenOver)
        restoreGhost(runtime);
    for (MotionHandle& leg : runtime.mLegs)
    {
        leg.mMix.mStart = leg.mMix.mCurrent = leg.mWeight;
        leg.mMix.mTarget = 0.0;
        leg.mState = MotionHandle::State::RELEASING;
    }
    runtime.mCurrentItem = -1;
    runtime.mTransitionStartedAt = mFadeTime;
    runtime.mTransitionActive = false;
    runtime.mReleaseActive = !runtime.mLegs.empty();
    if (!runtime.mReleaseActive && !runtime.mGhostTakenOver) restoreGhost(runtime);
}

void ALDanceBall::releaseAll(bool immediate)
{
    for (auto& pair : mRuntime)
    {
        Dancer dancer;
        dancer.mId = pair.first;
        dancer.mKind = pair.second.mKind;
        Runtime& runtime = pair.second;
        releaseRuntime(dancer, runtime);
        if (immediate)
        {
            LLVOAvatar* avatar = resolve(dancer);
            for (MotionHandle& handle : runtime.mLegs)
                finishReleaseMotion(avatar, handle);
            runtime.mLegs.clear();
            runtime.mReleaseActive = false;
            restoreGhost(runtime);
        }
    }
    for (auto it = mRuntime.begin(); it != mRuntime.end(); )
    {
        if (!it->second.mGhostClaimed && it->second.mLegs.empty())
        {
            it = mRuntime.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

bool ALDanceBall::ensureMotion(LLVOAvatar* avatar, MotionHandle& handle,
                               F32 weight, F64 sample, bool start_allowed,
                               DancerState& state)
{
    if (!avatar || !handle.valid())
    {
        return false;
    }
    const bool signaled = avatar->isAnyAnimationSignaled(&handle.mAnim, 1);
    if (signaled)
    {
        bool controlled = false;
        if (LLMotion* motion = avatar->findMotion(handle.mAnim))
        {
            controlled = handle.mControlsMotion &&
                motion->isPresentationControlledBy(handle.mToken);
            if (controlled)
            {
                motion->setPresentationWeight(handle.mToken, 0.f);
                motion->setOwnerManagedLifetime(handle.mToken, false);
                if (handle.mPrioritySaved)
                    motion->setPriorityOverride(handle.mSavedPriority);
            }
        }
        if (avatar->isSelf() && handle.mOwnsMotion && controlled)
        {
            // Retire our local instance before handing the UUID to the
            // simulator. This remains LLCharacter-qualified and cannot send.
            avatar->LLCharacter::stopMotion(handle.mAnim, true);
        }
        if (LLMotion* motion = avatar->findMotion(handle.mAnim))
        {
            if (motion->isPresentationControlledBy(handle.mToken))
                motion->releasePresentationControl(handle.mToken);
        }
        handle.mControlsMotion = false;
        handle.mOwnsMotion = false;
        state = DancerState::SERVER_OWNS_UUID;
        return false;
    }

    LLMotion* motion = avatar->findMotion(handle.mAnim);
    const bool active = avatar->isMotionActive(handle.mAnim);
    // flushAllMotions/resetAnimations rebuilds an active instance without its
    // presentation owner. The handle is the durable per-avatar proof that
    // Dance Ball started this UUID, so reclaim before any IN_USE decision.
    if (active && motion && handle.mOwnsMotion &&
        !motion->hasPresentationControl())
    {
        if (motion->claimPresentationControl(handle.mToken))
        {
            motion->setOwnerManagedLifetime(handle.mToken, true);
            motion->setPresentationJointExclusion(handle.mToken,
                mTiming.mDanceDrivesFace ? nullptr : &LLMotion::isFaceJoint);
            handle.mControlsMotion = true;
        }
    }
    if (active && (!handle.mPlaybackStarted ||
        (motion && !motion->isPresentationControlledBy(handle.mToken))) &&
        !avatar->isGhostAvatar())
    {
        state = DancerState::IN_USE;
        return false;
    }
    if (!motion)
    {
        motion = avatar->createMotion(handle.mAnim);
    }
    if (!motion || !motion->supportsExternalSampling() ||
        (!motion->isPresentationControlledBy(handle.mToken) &&
         !motion->claimPresentationControl(handle.mToken)))
    {
        state = motion ? DancerState::IN_USE : DancerState::ASSET_FAILED;
        return false;
    }
    if (!handle.mPrioritySaved)
    {
        handle.mSavedPriority = motion->getPriorityOverride();
        handle.mPrioritySaved = true;
    }
    handle.mControlsMotion = true;
    motion->setOwnerManagedLifetime(handle.mToken, true);
    motion->setPresentationJointExclusion(handle.mToken,
        mTiming.mDanceDrivesFace ? nullptr : &LLMotion::isFaceJoint);
    LLKeyframeMotion* keyframe = dynamic_cast<LLKeyframeMotion*>(motion);
    if (!keyframe)
    {
        state = DancerState::ASSET_FAILED;
        return false;
    }
    if (keyframe->hasLoadFailed())
    {
        state = DancerState::ASSET_FAILED;
        return false;
    }
    motion->setExternalSampleTime(handle.mToken, static_cast<F32>(
        std::clamp(sample, 0.0, static_cast<F64>(std::numeric_limits<F32>::max()))));
    motion->setPresentationWeight(handle.mToken, llclamp(weight, 0.f, 1.f));
    handle.mWeight = llclamp(weight, 0.f, 1.f);
    if (!keyframe->isLoaded() ||
        avatar->getMotionController().isMotionLoading(motion) ||
        !start_allowed)
    {
        state = DancerState::PENDING_ASSET;
        return true;
    }
    if (!active)
    {
        if (motion->isStopped())
        {
            avatar->LLCharacter::stopMotion(handle.mAnim, true);
            motion = avatar->findMotion(handle.mAnim);
            if (!motion || !motion->claimPresentationControl(handle.mToken))
            {
                state = DancerState::ASSET_FAILED;
                return false;
            }
            motion->setOwnerManagedLifetime(handle.mToken, true);
            motion->setPresentationJointExclusion(handle.mToken,
                mTiming.mDanceDrivesFace ? nullptr : &LLMotion::isFaceJoint);
            motion->setExternalSampleTime(handle.mToken, static_cast<F32>(sample));
            motion->setPresentationWeight(handle.mToken, handle.mWeight);
        }
        F64 activation_offset = keyframe->getLoopIn();
        const F64 cycle = keyframe->getLoopOut() - keyframe->getLoopIn();
        const F64 threshold = motion->getEaseInDuration() + 0.001;
        if (cycle > 0.001 && activation_offset <= threshold)
        {
            activation_offset +=
                (std::floor((threshold - activation_offset) / cycle) + 1.0) * cycle;
        }
        else if (activation_offset <= threshold)
        {
            activation_offset = threshold;
        }
        llassert(!avatar->isSelf() || motion->hasOwnerManagedLifetime());
        if (!avatar->LLCharacter::startMotion(handle.mAnim,
                static_cast<F32>(std::min(activation_offset,
                    static_cast<F64>(std::numeric_limits<F32>::max())))))
        {
            motion->releasePresentationControl(handle.mToken);
            handle.mControlsMotion = false;
            state = DancerState::ASSET_FAILED;
            return false;
        }
        if (!avatar->isMotionActive(handle.mAnim))
        {
            state = DancerState::PENDING_ASSET;
            return true;
        }
        handle.mOwnsMotion = true;
        handle.mPlaybackStarted = true;
        handle.mMix.mStarted = true;
        handle.mUnitStartedAt = mUnitTime;
        motion = avatar->findMotion(handle.mAnim);
        if (!motion || (!motion->isPresentationControlledBy(handle.mToken) &&
                        !motion->claimPresentationControl(handle.mToken)))
        {
            state = DancerState::ASSET_FAILED;
            return false;
        }
        motion->setOwnerManagedLifetime(handle.mToken, true);
        motion->setPresentationJointExclusion(handle.mToken,
            mTiming.mDanceDrivesFace ? nullptr : &LLMotion::isFaceJoint);
    }
    else if (!handle.mPlaybackStarted)
    {
        handle.mPlaybackStarted = true;
        handle.mMix.mStarted = true;
        handle.mUnitStartedAt = mUnitTime;
    }
    motion->setPriorityOverride(mTiming.mPriority >= 0
        ? mTiming.mPriority : handle.mSavedPriority);
    motion->setExternalSampleTime(handle.mToken, static_cast<F32>(
        std::clamp(sample, 0.0, static_cast<F64>(std::numeric_limits<F32>::max()))));
    motion->setPresentationWeight(handle.mToken, llclamp(weight, 0.f, 1.f));
    state = DancerState::PLAYING;
    return true;
}

void ALDanceBall::refreshEntryFacts(PlaylistEntry& entry, LLVOAvatar* avatar,
                                    const MotionHandle& handle)
{
    if (!avatar || !handle.valid())
    {
        return;
    }
    if (LLKeyframeMotion* motion = dynamic_cast<LLKeyframeMotion*>(
            avatar->findMotion(handle.mAnim)))
    {
        if (motion->hasLoadFailed())
        {
            if (entry.mStatus != AssetStatus::UNAVAILABLE)
            {
                entry.mStatus = AssetStatus::UNAVAILABLE;
                ++mStatusRevision;
            }
            return;
        }
        if (!motion->isLoaded())
        {
            return;
        }
        const F64 duration = finite_nonnegative(motion->getDuration());
        const F64 loop_in = finite_nonnegative(motion->getLoopIn());
        const F64 loop_out = finite_nonnegative(motion->getLoopOut());
        const bool loop = motion->getLoop();
        if (entry.mStatus != AssetStatus::READY || entry.mDuration != duration ||
            entry.mLoop != loop || entry.mLoopIn != loop_in ||
            entry.mLoopOut != loop_out)
        {
            entry.mStatus = AssetStatus::READY;
            entry.mDuration = duration;
            entry.mLoop = loop;
            entry.mLoopIn = loop_in;
            entry.mLoopOut = loop_out;
            ++mStatusRevision;
        }
    }
}

F64 ALDanceBall::dancerDistance(LLVOAvatar* avatar) const
{
    LLVector3d focal;
    if (!avatar || !focalPoint(mWave.mFocus, focal))
    {
        return 0.0;
    }
    return dist_vec(avatar->getPositionGlobal(), focal);
}

void ALDanceBall::transition(Dancer& dancer, LLVOAvatar* avatar,
                             Runtime& runtime, S32 item, F64 boundary,
                             F64 farthest_distance, bool is_switch)
{
    if (item < 0 || static_cast<std::size_t>(item) >= mPlaylist.size())
    {
        return;
    }
    PlaylistEntry& entry = mPlaylist[static_cast<std::size_t>(item)];
    if (entry.mStatus == AssetStatus::UNAVAILABLE)
    {
        // Nothing starts for an unavailable target, so mCurrentItem and the
        // legs stay exactly as they were (the caller treats an out-of-range
        // mCurrentItem as "nothing to drive"). A manual pick that cannot play
        // is dropped so the scheduler takes over instead of retrying this
        // item, and flipping the state, every frame.
        if (runtime.mManualItem == item)
        {
            runtime.mManualItem = -1;
        }
        setDancerState(dancer, runtime, DancerState::ASSET_FAILED);
        return;
    }
    if (runtime.mCurrentItem == item && runtime.mCurrentBoundary == boundary)
    {
        return;
    }

    // A never-started incoming leg has contributed no pose and must not become
    // an outgoing leg on a rapid Next/Next sequence.
    for (auto it = runtime.mLegs.begin(); it != runtime.mLegs.end(); )
    {
        if (!it->mPlaybackStarted && it->mWeight <= 0.0001f &&
            it->mAnim != entry.mAssetId)
        {
            finishReleaseMotion(avatar, *it);
            it = runtime.mLegs.erase(it);
        }
        else
        {
            ++it;
        }
    }

    auto existing = std::find_if(runtime.mLegs.begin(), runtime.mLegs.end(),
        [&](const MotionHandle& leg) { return leg.mAnim == entry.mAssetId; });
    const bool leg_existed = existing != runtime.mLegs.end();
    std::size_t incoming = 0;
    if (!leg_existed)
    {
        MotionHandle leg;
        leg.mAnim = entry.mAssetId;
        leg.mToken.generate();
        runtime.mLegs.push_back(leg);
        incoming = runtime.mLegs.size() - 1;
    }
    else
    {
        incoming = static_cast<std::size_t>(existing - runtime.mLegs.begin());
    }
    MotionHandle& current = runtime.mLegs[incoming];
    // A leg that is already playing keeps its sample; it is only re-pointed at
    // the new boundary. Drop its shared-sync state so the error against the
    // new boundary is recomputed and slewed (no snap), and give it no wave
    // delay, which would freeze it at the loop start.
    const bool reused_playing = leg_existed && current.mPlaybackStarted;
    current.mItem = item;
    current.mStartedAt = boundary;
    current.mUnitStartedAt = mUnitTime;
    current.mDelay = 0.0;
    current.mSharedSync = false;
    current.mSharedSyncError = 0.0;
    current.mSharedSyncElapsed = 0.0;
    current.mLoop = entry.mLoop;
    current.mDuration = entry.mDuration;
    current.mLoopIn = entry.mLoopIn;
    current.mLoopOut = entry.mLoopOut;
    if (!reused_playing && mWave.mEnabled &&
        ALDanceBallModel::waveApplies(mWave.mApply, is_switch))
    {
        current.mDelay = ALDanceBallModel::waveDelay(
            dancerDistance(avatar), farthest_distance,
            mWave.mSpreadMetersPerSecond, mWave.mDirection);
    }

    runtime.mCurrentItem = item;
    runtime.mCurrentBoundary = boundary;
    runtime.mReleaseActive = false;
    if (dancer.mKind == DancerKind::GHOST)
        claimGhost(dancer, runtime);
    beginTransition(runtime, incoming);

    ++mStatusRevision;
}

void ALDanceBall::updateDancer(
    Dancer& dancer, F64 now, F64 farthest_distance,
    const std::vector<ALDanceBallModel::PlaylistItem>& items,
    const ALDanceBallModel::SchedulerConfig& config)
{
    Runtime& runtime = mRuntime[dancer.mId];
    runtime.mKind = dancer.mKind;
    LLVOAvatar* avatar = resolve(dancer);
    // Skeleton/animation resets replace controller instances. Reclaim every
    // still-owned UUID before skip, scheduler, transition, or release logic so
    // self never spends a controller update on native lifetime handling.
    if (avatar)
    {
        for (MotionHandle& leg : runtime.mLegs)
        {
            if (!leg.mOwnsMotion ||
                avatar->isAnyAnimationSignaled(&leg.mAnim, 1) ||
                !avatar->isMotionActive(leg.mAnim))
            {
                continue;
            }
            if (LLMotion* motion = avatar->findMotion(leg.mAnim))
            {
                if (!motion->hasPresentationControl() &&
                    motion->claimPresentationControl(leg.mToken))
                {
                    motion->setOwnerManagedLifetime(leg.mToken, true);
                    motion->setPresentationJointExclusion(leg.mToken,
                        mTiming.mDanceDrivesFace ? nullptr : &LLMotion::isFaceJoint);
                    leg.mControlsMotion = true;
                }
            }
        }
    }
    const bool was_releasing = runtime.mReleaseActive;
    updateLegs(dancer, runtime, now, was_releasing);
    if (runtime.mCheckedScan != mScanGeneration)
    {
        runtime.mCheckedScan = mScanGeneration;
        runtime.mCachedSkip = skipState(dancer, avatar);
    }
    const DancerState skipped = runtime.mCachedSkip;
    if (skipped != DancerState::IDLE)
    {
        releaseRuntime(dancer, runtime);
        if (!was_releasing)
            updateLegs(dancer, runtime, now, true);
        setDancerState(dancer, runtime, skipped);
        return;
    }

    if (runtime.mGhostTakenOver)
    {
        setDancerState(dancer, runtime, DancerState::TAKEN_OVER);
        return;
    }
    if (dancer.mKind == DancerKind::GHOST && runtime.mGhostClaimed)
    {
        const ALGhostStudio::Instance* instance =
            ALGhostStudio::instance().getInstance(runtime.mGhostInstance);
        const bool edited = instance && (runtime.mGhostDirected
            ? (instance->mLoopMode != ALGhostStudio::LOOP_RETRIGGER ||
               instance->mDriveMode != ALGhostStudio::DRIVE_DIRECTED ||
               instance->mDirectedAnim != runtime.mGhostExpectedAnim)
            : (static_cast<S32>(instance->mLoopMode) !=
                   runtime.mGhostPriorLoopMode ||
               static_cast<S32>(instance->mDriveMode) !=
                   runtime.mGhostPriorMode ||
               instance->mDirectedAnim != runtime.mGhostPriorAnim));
        if (edited)
        {
            // A Studio edit is an explicit user takeover. Fade our motions but
            // do not restore over the user's newly selected mode.
            runtime.mGhostClaimed = false;
            runtime.mGhostTakenOver = true;
            releaseRuntime(dancer, runtime);
            setDancerState(dancer, runtime, DancerState::TAKEN_OVER);
            return;
        }
    }

    const U64 key = dancer.mId.getDigest64();
    ALDanceBallModel::Selection selected =
        mScheduler.update(now, key, items, config);
    // A manual pick that can no longer play (removed row, failed asset) falls
    // back to the scheduler instead of retrying every frame.
    if (runtime.mManualItem >= 0 &&
        (static_cast<std::size_t>(runtime.mManualItem) >= mPlaylist.size() ||
         mPlaylist[static_cast<std::size_t>(runtime.mManualItem)].mStatus ==
             AssetStatus::UNAVAILABLE))
    {
        runtime.mManualItem = -1;
    }
    if (runtime.mManualItem >= 0)
    {
        const bool shared_advanced = selected.mBoundary > runtime.mManualBoundary ||
            (selected.mItem != runtime.mManualBaseItem &&
             selected.mBoundary >= runtime.mManualBoundary);
        if (shared_advanced)
        {
            runtime.mManualItem = -1;
        }
        else
        {
            selected.mChanged = runtime.mCurrentItem != runtime.mManualItem;
            selected.mPrevious = runtime.mCurrentItem;
            selected.mItem = runtime.mManualItem;
            selected.mBoundary = runtime.mManualBoundary;
        }
    }
    if (selected.mItem < 0)
    {
        releaseRuntime(dancer, runtime);
        const bool all_unavailable = !mPlaylist.empty() &&
            std::none_of(mPlaylist.begin(), mPlaylist.end(),
                [](const PlaylistEntry& entry)
                { return entry.mStatus != AssetStatus::UNAVAILABLE; });
        setDancerState(dancer, runtime, all_unavailable
            ? DancerState::ASSET_FAILED : DancerState::IDLE);
        return;
    }
    if (runtime.mCurrentItem != selected.mItem ||
        runtime.mCurrentBoundary != selected.mBoundary)
    {
        transition(dancer, avatar, runtime, selected.mItem,
                   selected.mBoundary, farthest_distance,
                   !runtime.mLegs.empty());
    }
    // transition() may have declined (unavailable target), and a playlist
    // remap can leave mCurrentItem or a leg's mItem at -1 or past the end.
    // Anything outside [0, size) means there is nothing to drive this frame.
    if (runtime.mCurrentItem < 0 ||
        static_cast<std::size_t>(runtime.mCurrentItem) >= mPlaylist.size())
    {
        return;
    }
    auto incoming_it = std::find_if(runtime.mLegs.begin(), runtime.mLegs.end(),
        [&](const MotionHandle& leg)
        {
            return leg.mItem >= 0 &&
                   static_cast<std::size_t>(leg.mItem) < mPlaylist.size() &&
                   leg.mItem == runtime.mCurrentItem &&
                   leg.mMix.mTarget == 1.0;
        });
    if (incoming_it == runtime.mLegs.end())
    {
        return;
    }
    const std::size_t incoming_index = static_cast<std::size_t>(
        incoming_it - runtime.mLegs.begin());
    MotionHandle& incoming = *incoming_it;
    PlaylistEntry& current_entry = mPlaylist[static_cast<std::size_t>(
        runtime.mCurrentItem)];
    incoming.mLoop = current_entry.mLoop;
    incoming.mDuration = current_entry.mDuration;
    incoming.mLoopIn = current_entry.mLoopIn;
    incoming.mLoopOut = current_entry.mLoopOut;

    const bool shared_sync =
        config.mMode == ALDanceBallModel::PlaylistMode::SAME_FOR_ALL &&
        mTiming.mDesync == 0.0;
    const F64 target_rate = ALDanceBallModel::effectiveSpeedMultiplier(
        key, config.mMode, mTiming.mDesync, mTiming.mSpeedJitter,
        mTiming.mSeed, mReshuffle);
    const F64 target_phase = shared_sync ? 0.0 :
        ALDanceBallModel::phaseOffset(
            key, mTiming.mDesync, mTiming.mSeed, mReshuffle);
    ALDanceBallModel::Clip incoming_clip;
    incoming_clip.mLoop = incoming.mLoop;
    incoming_clip.mDuration = incoming.mDuration;
    incoming_clip.mLoopIn = incoming.mLoopIn;
    incoming_clip.mLoopOut = incoming.mLoopOut;
    F64 incoming_cycle = incoming.mLoopOut > incoming.mLoopIn
        ? incoming.mLoopOut - incoming.mLoopIn : 0.0;
    // Play-once clips ignore rate and phase in every mode: they start at their
    // intro, run at 1x on unit time, and end at their natural length.
    F64 base_elapsed = incoming.mLoop
        ? std::max(0.0, now - incoming.mStartedAt - incoming.mDelay) : 0.0;
    F64 initial_time = base_elapsed +
        (incoming.mLoop ? target_phase * incoming_cycle : 0.0);
    const F64 pending_sample = ALDanceBallModel::mapSample(
        incoming_clip, initial_time, 1.0, 0.0).mTime;
    DancerState state = DancerState::IDLE;
    const bool start_allowed = incoming.mPlaybackStarted ||
        mUnitTime - incoming.mUnitStartedAt >= incoming.mDelay;
    ensureMotion(avatar, incoming, incoming.mWeight, pending_sample,
                 start_allowed, state);
    refreshEntryFacts(current_entry, avatar, incoming);
    incoming.mLoop = current_entry.mLoop;
    incoming.mDuration = current_entry.mDuration;
    incoming.mLoopIn = current_entry.mLoopIn;
    incoming.mLoopOut = current_entry.mLoopOut;
    incoming_clip.mLoop = incoming.mLoop;
    incoming_clip.mDuration = incoming.mDuration;
    incoming_clip.mLoopIn = incoming.mLoopIn;
    incoming_clip.mLoopOut = incoming.mLoopOut;
    incoming_cycle = incoming.mLoopOut > incoming.mLoopIn
        ? incoming.mLoopOut - incoming.mLoopIn : 0.0;
    base_elapsed = incoming.mLoop
        ? std::max(0.0, now - incoming.mStartedAt - incoming.mDelay) : 0.0;
    const F64 incoming_phase_seconds =
        incoming.mLoop ? target_phase * incoming_cycle : 0.0;
    const F64 incoming_rate = incoming.mLoop ? target_rate : 1.0;
    initial_time = base_elapsed + incoming_phase_seconds;
    if (state == DancerState::ASSET_FAILED &&
        current_entry.mStatus != AssetStatus::UNAVAILABLE)
    {
        current_entry.mStatus = AssetStatus::UNAVAILABLE;
        ++mStatusRevision;
    }
    const bool incoming_ready = state == DancerState::PLAYING;
    const bool incoming_just_initialized =
        incoming_ready && !incoming.mSampleInitialized;
    if (incoming_just_initialized)
    {
        incoming.mSampleClock.reset(initial_time, incoming_rate,
                                    incoming_phase_seconds);
        incoming.mSampleInitialized = true;
    }
    if (incoming_ready && !runtime.mTransitionActive &&
        incoming.mState != MotionHandle::State::LIVE)
    {
        // Asset readiness is the transition boundary. Until this point every
        // existing leg held its exact live weight.
        beginTransition(runtime, incoming_index);
        runtime.mTransitionActive = true;
        runtime.mTransitionStartedAt = mFadeTime;
    }

    F64 progress = 0.0;
    if (runtime.mTransitionActive)
    {
        const F64 duration = std::max(0.0, mTiming.mCrossfadeSeconds);
        progress = duration > 0.0
            ? clamp01((mFadeTime - runtime.mTransitionStartedAt) / duration)
            : 1.0;
        std::vector<ALDanceBallModel::LegWeight> weights;
        weights.reserve(runtime.mLegs.size());
        for (const MotionHandle& leg : runtime.mLegs) weights.push_back(leg.mMix);
        ALDanceBallModel::advanceLegWeights(weights, progress);
        for (std::size_t i = 0; i < weights.size(); ++i)
            runtime.mLegs[i].mMix = weights[i];
    }

    bool transition_complete = incoming_ready && progress >= 1.0;
    const LLUUID incoming_token = incoming.mToken;
    const LLUUID incoming_anim = incoming.mAnim;
    if (transition_complete && dancer.mKind == DancerKind::GHOST)
    {
        // First starts and switches both leave Studio on the prior performance
        // until the Dance Ball incoming leg has reached full weight.
        transition_complete = directGhost(dancer, runtime, incoming_anim);
    }
    for (auto it = runtime.mLegs.begin(); it != runtime.mLegs.end(); )
    {
        MotionHandle& leg = *it;
        if (transition_complete && leg.mMix.mTarget == 0.0)
        {
            finishReleaseMotion(avatar, leg);
            it = runtime.mLegs.erase(it);
            continue;
        }
        if (leg.mPlaybackStarted)
        {
            const F64 cycle = leg.mLoopOut > leg.mLoopIn
                ? leg.mLoopOut - leg.mLoopIn : 0.0;
            const F64 phase_seconds = target_phase * cycle;
            const F64 shared_time = leg.mLoop
                ? std::max(0.0, now - leg.mStartedAt - leg.mDelay)
                : std::max(0.0, mUnitTime - leg.mUnitStartedAt);
            // Shared-time lanes: every leg in synced Same-for-all, and every
            // play-once clip in any mode (play-once ignores rate and phase).
            if (shared_sync || !leg.mLoop)
            {
                if (!leg.mSampleInitialized)
                {
                    leg.mSampleClock.reset(shared_time, 1.0, 0.0);
                    leg.mSampleInitialized = true;
                }
                // Only a looped lane may wrap its error around the loop.
                if (!leg.mSharedSync)
                {
                    leg.mSharedSync = true;
                    leg.mSharedSyncError = ALDanceBallModel::nearestLoopError(
                        leg.mSampleClock.time() - shared_time,
                        leg.mLoop ? cycle : 0.0);
                    leg.mSharedSyncElapsed = 0.0;
                }
                // Slew progress follows the lane's own timeline: looped lanes
                // are sampled from the speed-scaled transport clock, so their
                // correction must advance with the transport delta (otherwise
                // a global speed below 0.5x would play it in reverse). Play-
                // once lanes run on unscaled unit time and use the wall delta.
                leg.mSharedSyncElapsed += leg.mLoop ? mTransportDelta : mSlewDelta;
                // Stretch the slew so the correction stays within +-0.5x of
                // playback rate (never backward, never a whip).
                const F64 sync_duration = ALDanceBallModel::phaseSlewDuration(
                    mTiming.mCrossfadeSeconds, leg.mSharedSyncError);
                const F64 sync_progress = sync_duration > 0.0
                    ? leg.mSharedSyncElapsed / sync_duration : 1.0;
                leg.mSampleClock.reset(ALDanceBallModel::slewToSharedSample(
                    shared_time, leg.mSharedSyncError, sync_progress),
                    1.0, 0.0);
            }
            else if (!leg.mSampleInitialized)
            {
                leg.mSampleClock.reset(phase_seconds, target_rate,
                                       phase_seconds);
                leg.mSampleInitialized = true;
                leg.mSharedSync = false;
            }
            else if (leg.mToken != incoming_token ||
                     (incoming_ready && !incoming_just_initialized))
            {
                leg.mSharedSync = false;
                leg.mSampleClock.retarget(target_rate, phase_seconds,
                                          mTiming.mCrossfadeSeconds, cycle);
                leg.mSampleClock.advance(mTransportDelta, mTransportDelta);
            }
            ALDanceBallModel::Clip clip;
            clip.mLoop = leg.mLoop;
            clip.mDuration = leg.mDuration;
            clip.mLoopIn = leg.mLoopIn;
            clip.mLoopOut = leg.mLoopOut;
            const ALDanceBallModel::Sample leg_sample =
                ALDanceBallModel::mapSample(
                    clip, leg.mSampleClock.time(), 1.0, 0.0);
            leg.mWeight = static_cast<F32>(leg.mMix.mCurrent);
            DancerState leg_state = state;
            ensureMotion(avatar, leg, leg.mWeight,
                         leg_sample.mTime, true, leg_state);
        }
        ++it;
    }
    if (transition_complete)
    {
        runtime.mTransitionActive = false;
        const auto settled = std::find_if(runtime.mLegs.begin(),
            runtime.mLegs.end(), [&](const MotionHandle& leg)
            { return leg.mToken == incoming_token; });
        if (settled != runtime.mLegs.end())
            settled->mState = MotionHandle::State::LIVE;
    }
    setDancerState(dancer, runtime, state);

    const auto current_leg = std::find_if(runtime.mLegs.begin(),
        runtime.mLegs.end(), [&](const MotionHandle& leg)
        { return leg.mToken == incoming_token; });
    const ALDanceBallModel::Sample current_sample = current_leg != runtime.mLegs.end()
        ? ALDanceBallModel::mapSample(
            incoming_clip, current_leg->mSampleClock.time(), 1.0, 0.0)
        : ALDanceBallModel::Sample();
    if (incoming_ready && current_sample.mEnded && !current_entry.mLoop)
    {
        ALDanceBallModel::Selection advanced;
        if (runtime.mManualItem >= 0)
        {
            runtime.mManualItem = -1;
            advanced = mScheduler.update(now, key, items, config);
        }
        else
        {
            advanced = mScheduler.next(now, key, items, config);
        }
        if (advanced.mItem >= 0 && advanced.mItem != runtime.mCurrentItem)
        {
            transition(dancer, avatar, runtime, advanced.mItem,
                       advanced.mBoundary,
                       farthest_distance, true);
        }
    }
}

void ALDanceBall::scanRoster()
{
    ++mScanGeneration;
    for (PlaylistEntry& entry : mPlaylist)
    {
        if (const ALGhostAnimAssetIndex::Entry* indexed =
                ALGhostAnimAssetIndex::instance().find(entry.mAssetId))
        {
            if (indexed->mAvailability ==
                    ALGhostAnimAssetIndex::AVAILABLE_UNAVAILABLE &&
                entry.mStatus != AssetStatus::UNAVAILABLE)
            {
                entry.mStatus = AssetStatus::UNAVAILABLE;
                ++mStatusRevision;
            }
            else if (indexed->mAvailability ==
                         ALGhostAnimAssetIndex::AVAILABLE_READY)
            {
                bool changed = false;
                if (entry.mStatus != AssetStatus::READY)
                {
                    entry.mStatus = AssetStatus::READY;
                    entry.mDuration = finite_nonnegative(indexed->mDuration);
                    entry.mLoop = indexed->mLoop;
                    entry.mLoopIn = finite_nonnegative(indexed->mLoopIn);
                    entry.mLoopOut = finite_nonnegative(indexed->mLoopOut);
                    changed = true;
                }
                if (!indexed->mName.empty() &&
                    entry.mLabel == "Anim " + entry.mAssetId.asString().substr(0, 8))
                {
                    entry.mLabel = indexed->mName;
                    ++mPlaylistRevision;
                    changed = true;
                }
                if (changed) ++mStatusRevision;
            }
        }
    }
    if (mAutoJoin)
    {
        addNearby(mAutoJoinRadius, mAutoJoinFocus);
    }
}

void ALDanceBall::tick()
{
    // The normal app idle path returns before this hook after disconnect;
    // keep the check as a defensive guard for direct/test callers. Logout is
    // the reachable teardown path while the world still exists.
    if (gDisconnected ||
        (LLAppViewer::instance() && LLAppViewer::instance()->logoutRequestSent()))
    {
        if (mHadWorld || mEnabled || !mRoster.empty() || !mPlaylist.empty())
        {
            shutdown();
        }
        return;
    }
    if ((!mEnabled || mRoster.empty()) && mRuntime.empty() &&
        (!mHadWorld || gAgent.getRegion()))
    {
        return;
    }
    LL_PROFILE_ZONE_NAMED_CATEGORY_APP("ALDanceBall::tick");

    const bool have_world = LLWorld::instanceExists() && gAgent.getRegion();
    if (!have_world)
    {
        if (mHadWorld)
        {
            shutdown();
        }
        return;
    }
    mHadWorld = true;

    const F64 presentation_delta = LLPresentationTime::presentationDelta();
    mFadeTime += finite_nonnegative(presentation_delta);
    if (!mClock.frozen())
    {
        mUnitTime += finite_nonnegative(presentation_delta);
    }
    const F64 prior_transport = mClock.time();
    mClock.advance(presentation_delta);
    const F64 now = mClock.time();
    mTransportDelta = std::max(0.0, now - prior_transport);
    mSlewDelta = mClock.frozen() ? 0.0 : finite_nonnegative(presentation_delta);
    const F64 wall = LLTimer::getTotalSeconds();
    const bool scan_due = mLastScanWall < 0.0 || wall - mLastScanWall >= 1.0;
    if (scan_due)
    {
        mLastScanWall = wall;
        scanRoster();
        mFarthestDistance = 0.0;
        if (mWave.mEnabled)
        {
            for (const Dancer& dancer : mRoster)
            {
                mFarthestDistance = std::max(mFarthestDistance,
                    dancerDistance(resolve(dancer)));
            }
        }
    }

    std::set<LLUUID> roster_ids;
    for (const Dancer& dancer : mRoster) roster_ids.insert(dancer.mId);
    for (auto it = mRuntime.begin(); it != mRuntime.end(); )
    {
        if (!mEnabled || !roster_ids.count(it->first))
        {
            Dancer orphan;
            orphan.mId = it->first;
            orphan.mKind = it->second.mKind;
            releaseRuntime(orphan, it->second);
            updateLegs(orphan, it->second, now, true);
        }
        if (scan_due && it->second.mGhostClaimed &&
            it->second.mLegs.empty())
        {
            restoreGhost(it->second);
        }
        if (!roster_ids.count(it->first) && !it->second.mGhostClaimed &&
            it->second.mLegs.empty())
            it = mRuntime.erase(it);
        else
            ++it;
    }
    if (!mEnabled || mRoster.empty()) return;

    const auto items = modelPlaylist();
    const auto config = schedulerConfig();
    for (Dancer& dancer : mRoster)
    {
        updateDancer(dancer, now, mFarthestDistance, items, config);
    }
}

void ALDanceBall::shutdown()
{
    releaseAll(true);
    ALAvatarStaging::instance().clear(true);
    // Logout is terminal for this session; Ghost Studio tears down its own
    // instance ledger too, so no retry record crosses into a relog.
    mRuntime.clear();
    mRoster.clear();
    mExcluded.clear();
    mPlaylist.clear();
    mScheduler.reset();
    mClock.reset();
    mTiming = Timing();
    mWave = Wave();
    mReshuffle = 0;
    mUnitTime = 0.0;
    mFadeTime = 0.0;
    mTransportDelta = 0.0;
    mSlewDelta = 0.0;
    mFarthestDistance = 0.0;
    mEnabled = false;
    mAutoJoin = false;
    mIncludeSeated = false;
    mIncludeSelf = false;
    mAutoJoinRadius = 20.f;
    mAutoJoinFocus = AutoJoinFocus::ME;
    mHadWorld = false;
    mLastScanWall = -1.0;
    ++mRosterRevision;
    ++mPlaylistRevision;
    ++mStatusRevision;
}

LLSD ALDanceBall::exportSession() const
{
    LLSD root;
    root["version"] = 2;
    root["rules"]["include_self"] = mIncludeSelf;
    root["rules"]["include_seated"] = mIncludeSeated;
    root["rules"]["auto_join"] = mAutoJoin;
    root["rules"]["auto_join_radius"] = mAutoJoinRadius;
    root["rules"]["auto_join_focus"] = static_cast<S32>(mAutoJoinFocus);
    root["roster"] = LLSD::emptyArray();
    for (const Dancer& dancer : mRoster)
    {
        LLSD value;
        value["id"] = dancer.mId;
        value["kind"] = static_cast<S32>(dancer.mKind);
        root["roster"].append(value);
    }
    root["excluded"] = LLSD::emptyArray();
    for (const LLUUID& id : mExcluded)
    {
        root["excluded"].append(id);
    }
    root["playlist"] = LLSD::emptyArray();
    for (const PlaylistEntry& entry : mPlaylist)
    {
        LLSD value;
        value["asset"] = entry.mAssetId;
        value["label"] = entry.mLabel;
        root["playlist"].append(value);
    }
    root["timing"]["mode"] = static_cast<S32>(mTiming.mMode);
    root["timing"]["auto"] = mTiming.mAutoAdvance;
    root["timing"]["interval"] = mTiming.mAdvanceSeconds;
    root["timing"]["jitter"] = mTiming.mAdvanceJitterSeconds;
    root["timing"]["desync"] = mTiming.mDesync;
    root["timing"]["speed"] = mTiming.mSpeed;
    root["timing"]["speed_jitter"] = mTiming.mSpeedJitter;
    root["timing"]["crossfade"] = mTiming.mCrossfadeSeconds;
    root["timing"]["priority"] = mTiming.mPriority;
    root["timing"]["dance_drives_face"] = mTiming.mDanceDrivesFace;
    root["timing"]["seed"] = llformat("%llu",
        static_cast<unsigned long long>(mTiming.mSeed));
    root["wave"]["enabled"] = mWave.mEnabled;
    root["wave"]["direction"] = static_cast<S32>(mWave.mDirection);
    root["wave"]["apply"] = static_cast<S32>(mWave.mApply);
    root["wave"]["spread"] = mWave.mSpreadMetersPerSecond;
    root["wave"]["focus"] = static_cast<S32>(mWave.mFocus);
    const LLSD staging = ALAvatarStaging::instance().exportEntries();
    std::set<LLUUID> dancer_ids;
    for (const Dancer& dancer : mRoster)
        if (dancer.mKind != DancerKind::SELF) dancer_ids.insert(dancer.mId);
    root["blocking"] = LLSD::emptyArray();
    for (LLSD::array_const_iterator it = staging["blocking"].beginArray();
         it != staging["blocking"].endArray(); ++it)
    {
        if (dancer_ids.count((*it)["id"].asUUID()))
            root["blocking"].append(*it);
    }
    if (staging.has("ball_centre"))
        root["ball_centre"] = staging["ball_centre"];
    return root;
}

bool ALDanceBall::importSession(const LLSD& root)
{
    mImportErrorReason.clear();
    auto fail = [&](const std::string& reason)
    {
        mImportErrorReason = reason;
        return false;
    };
    if (!root.isMap()) return fail("Session root is not a map");
    const S32 version = root.has("version") ? root["version"].asInteger() : 0;
    if (version != 1 && version != 2)
        return fail("Unsupported Dance Ball session version");
    if (!root["roster"].isArray()) return fail("Roster is not an array");
    if (!root["playlist"].isArray()) return fail("Playlist is not an array");
    if (version == 2 && !root["blocking"].isArray())
        return fail("Blocking is not an array");

    constexpr std::size_t MAX_DANCERS = 256;
    constexpr std::size_t MAX_ANIMATIONS = 1024;
    constexpr std::size_t MAX_LABEL_BYTES = 256;

    std::vector<Dancer> roster;
    std::set<LLUUID> excluded;
    std::set<LLUUID> roster_ids;
    std::set<LLUUID> playlist_ids;
    std::vector<PlaylistEntry> playlist;
    const LLSD& rules = root["rules"];
    const bool include_self = rules.has("include_self")
        ? rules["include_self"].asBoolean() : false;
    if (root["excluded"].isArray())
    {
        for (LLSD::array_const_iterator it = root["excluded"].beginArray();
             it != root["excluded"].endArray(); ++it)
        {
            if (excluded.size() >= MAX_DANCERS) break;
            const LLUUID id = it->asUUID();
            if (id.notNull()) excluded.insert(canonical_dancer_id(id));
        }
    }
    for (LLSD::array_const_iterator it = root["roster"].beginArray();
         it != root["roster"].endArray(); ++it)
    {
        if (roster.size() >= MAX_DANCERS) break;
        LLUUID id = (*it)["id"].asUUID();
        const S32 kind = (*it)["kind"].asInteger();
        if (id.isNull() || kind < static_cast<S32>(DancerKind::SELF) ||
            kind > static_cast<S32>(DancerKind::GHOST) ||
            (static_cast<DancerKind>(kind) == DancerKind::SELF && !include_self))
        {
            continue;
        }
        Dancer dancer;
        if (static_cast<DancerKind>(kind) == DancerKind::SELF) id = gAgentID;
        if (static_cast<DancerKind>(kind) == DancerKind::GHOST)
        {
            const ALGhostStudio::Instance* ghost =
                ALGhostStudio::instance().getInstance(id);
            if (!ghost) ghost = ALGhostStudio::instance().findInstanceByRuntime(id);
            if (!ghost) continue;
            dancer.mId = ghost->mId;
        }
        else
        {
            dancer.mId = LLDirectorCast::canonicalActorId(id);
        }
        dancer.mShortId = dancer.mId.asString().substr(0, 8);
        if (excluded.count(dancer.mId)) continue;
        dancer.mKind = static_cast<DancerKind>(kind);
        if (LLVOAvatar* avatar = LLDirectorCast::instance().resolve(dancer.mId))
            dancer.mKind = classify(dancer.mId, avatar);
        if (roster_ids.insert(dancer.mId).second)
        {
            roster.push_back(dancer);
        }
    }
    for (LLSD::array_const_iterator it = root["playlist"].beginArray();
         it != root["playlist"].endArray(); ++it)
    {
        if (playlist.size() >= MAX_ANIMATIONS) break;
        const LLUUID id = (*it)["asset"].asUUID();
        if (id.isNull() || !playlist_ids.insert(id).second) continue;
        PlaylistEntry entry;
        entry.mAssetId = id;
        entry.mLabel = (*it)["label"].asString();
        if (entry.mLabel.size() > MAX_LABEL_BYTES)
            entry.mLabel.resize(MAX_LABEL_BYTES);
        if (entry.mLabel.empty()) entry.mLabel = defaultLabel(id);
        playlist.push_back(entry);
    }

    Timing timing;
    const LLSD& time = root["timing"];
    if (time.has("mode")) timing.mMode =
        static_cast<ALDanceBallModel::PlaylistMode>(
            std::clamp(time["mode"].asInteger(), 0, 2));
    if (time.has("auto")) timing.mAutoAdvance = time["auto"].asBoolean();
    if (time.has("interval")) timing.mAdvanceSeconds = std::clamp(
        finite_nonnegative(time["interval"].asReal(), 8.0), 0.5, 600.0);
    if (time.has("jitter")) timing.mAdvanceJitterSeconds = std::clamp(
        finite_nonnegative(time["jitter"].asReal()), 0.0, 300.0);
    if (time.has("desync")) timing.mDesync = time["desync"].asReal();
    if (time.has("speed")) timing.mSpeed = time["speed"].asReal();
    if (time.has("speed_jitter")) timing.mSpeedJitter = time["speed_jitter"].asReal();
    if (time.has("crossfade")) timing.mCrossfadeSeconds = std::clamp(
        finite_nonnegative(time["crossfade"].asReal(), 0.5), 0.0, 5.0);
    if (time.has("priority")) timing.mPriority = time["priority"].asInteger();
    if (time.has("dance_drives_face"))
        timing.mDanceDrivesFace = time["dance_drives_face"].asBoolean();
    const std::string seed_text = time.has("seed")
        ? time["seed"].asString() : std::string();
    char* seed_end = nullptr;
    const unsigned long long parsed_seed =
        std::strtoull(seed_text.c_str(), &seed_end, 10);
    if (seed_end && seed_end != seed_text.c_str())
    {
        timing.mSeed = static_cast<U64>(parsed_seed);
    }

    std::vector<S32> remap(mPlaylist.size(), -1);
    for (std::size_t old_index = 0; old_index < mPlaylist.size(); ++old_index)
    {
        for (std::size_t new_index = 0; new_index < playlist.size(); ++new_index)
        {
            if (mPlaylist[old_index].mAssetId == playlist[new_index].mAssetId)
            {
                remap[old_index] = static_cast<S32>(new_index);
                break;
            }
        }
    }
    for (const Dancer& old_dancer : mRoster)
    {
        if (!roster_ids.count(old_dancer.mId))
        {
            auto runtime = mRuntime.find(old_dancer.mId);
            if (runtime != mRuntime.end()) releaseRuntime(old_dancer, runtime->second);
            mScheduler.erase(old_dancer.mId.getDigest64(), mTiming.mMode);
        }
    }
    mRoster = roster;
    mExcluded = excluded;
    mPlaylist = playlist;
    remapPlaylist(remap);
    if (gAgent.getRegion()) mHadWorld = true;
    mIncludeSelf = include_self;
    mIncludeSeated = rules.has("include_seated")
        ? rules["include_seated"].asBoolean() : false;
    mAutoJoin = rules.has("auto_join")
        ? rules["auto_join"].asBoolean() : false;
    mAutoJoinRadius = rules.has("auto_join_radius")
        ? static_cast<F32>(std::clamp(finite_nonnegative(
            rules["auto_join_radius"].asReal(), 20.0), 0.0, 4096.0))
        : 20.f;
    mAutoJoinFocus = rules.has("auto_join_focus")
        ? static_cast<AutoJoinFocus>(std::clamp(
            rules["auto_join_focus"].asInteger(), 0, 2))
        : AutoJoinFocus::ME;
    setTiming(timing);
    const LLSD& wave = root["wave"];
    Wave imported_wave;
    if (wave.has("enabled")) imported_wave.mEnabled = wave["enabled"].asBoolean();
    if (wave.has("direction")) imported_wave.mDirection =
        static_cast<ALDanceBallModel::WaveDirection>(
            std::clamp(wave["direction"].asInteger(), 0, 1));
    if (wave.has("apply")) imported_wave.mApply =
        static_cast<ALDanceBallModel::WaveApply>(
            std::clamp(wave["apply"].asInteger(), 0, 2));
    if (wave.has("spread")) imported_wave.mSpreadMetersPerSecond =
        std::clamp(finite_nonnegative(wave["spread"].asReal(), 10.0),
                   0.5, 100.0);
    if (wave.has("focus")) imported_wave.mFocus = static_cast<AutoJoinFocus>(
        std::clamp(wave["focus"].asInteger(), 0, 2));
    mWave = imported_wave;
    LLSD staging;
    staging["version"] = version;
    if (version == 2)
    {
        staging["blocking"] = LLSD::emptyArray();
        for (LLSD::array_const_iterator it = root["blocking"].beginArray();
             it != root["blocking"].endArray() &&
             staging["blocking"].size() < MAX_DANCERS; ++it)
        {
            if (roster_ids.count(canonical_dancer_id((*it)["id"].asUUID())))
                staging["blocking"].append(*it);
        }
        if (root.has("ball_centre"))
            staging["ball_centre"] = root["ball_centre"];
    }
    if (!ALAvatarStaging::instance().importEntries(staging))
        return fail("Blocking data is invalid");
    for (const PlaylistEntry& entry : mPlaylist)
    {
        ALGhostAnimAssetIndex::instance().requestMetadata(entry.mAssetId);
    }
    ++mRosterRevision;
    ++mPlaylistRevision;
    ++mStatusRevision;
    return true;
}
