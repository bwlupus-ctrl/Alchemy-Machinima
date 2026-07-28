/**
 * @file alghostgroupmodel.cpp
 * @brief Pure authoring model for Ghost Studio crowd groups.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "alghostgroupmodel.h"

#include "llmath.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <utility>
#include <vector>

namespace
{
constexpr F32 MIN_GROUP_SCALE = 0.0001f;

LLVector3d rotate_vector(const LLVector3d& value, const LLQuaternion& rotation)
{
    // LLQuaternion supplies a native LLVector3d operator whose intermediates
    // stay F64.  Casting an authored offset to LLVector3 first discarded
    // metres of precision for large scene coordinates.
    return value * rotation;
}

bool valid_mode(ALGhostGroupModel::EMode mode)
{
    return mode == ALGhostGroupModel::MODE_RIGID ||
           mode == ALGhostGroupModel::MODE_MOVE_FACE;
}
}

bool ALGhostGroupModel::Transform::isFinite() const
{
    if (!mFoot.isFinite() || !mRotation.isFinite() ||
        !llfinite(mScale) || mScale <= 0.f)
    {
        return false;
    }
    // Evaluate in F64 so finite-but-huge F32 components cannot overflow the
    // check itself. A zero/near-zero quaternion has no usable orientation.
    F64 norm_squared = 0.0;
    for (S32 i = 0; i < LENGTHOFQUAT; ++i)
    {
        const F64 component = (F64)mRotation.mQ[i];
        norm_squared += component * component;
    }
    return std::isfinite(norm_squared) &&
           norm_squared > (F64)FP_MAG_THRESHOLD * FP_MAG_THRESHOLD;
}

bool ALGhostGroupModel::normalizeTransform(Transform& transform)
{
    if (!transform.isFinite())
    {
        return false;
    }
    const F32 original_magnitude = transform.mRotation.normalize();
    if (!llfinite(original_magnitude) ||
        original_magnitude <= FP_MAG_THRESHOLD ||
        !transform.mRotation.isFinite())
    {
        return false;
    }
    F64 normalized_squared = 0.0;
    for (S32 i = 0; i < LENGTHOFQUAT; ++i)
    {
        const F64 component = (F64)transform.mRotation.mQ[i];
        normalized_squared += component * component;
    }
    return std::isfinite(normalized_squared) &&
           std::fabs(normalized_squared - 1.0) <= 0.00001;
}

ALGhostGroupModel::Transform ALGhostGroupModel::localToWorld(
    const Transform& group, const Transform& local)
{
    Transform world;
    world.mFoot = group.mFoot +
        rotate_vector(local.mFoot * (F64)group.mScale, group.mRotation);
    world.mRotation = local.mRotation * group.mRotation;
    world.mRotation.normalize();
    world.mScale = local.mScale * group.mScale;
    return world;
}

ALGhostGroupModel::Transform ALGhostGroupModel::worldToLocal(
    const Transform& group, const Transform& world)
{
    Transform local;
    LLQuaternion inverse = ~group.mRotation;
    inverse.normalize();
    local.mFoot = rotate_vector(world.mFoot - group.mFoot, inverse) /
                  (F64)group.mScale;
    local.mRotation = world.mRotation * inverse;
    local.mRotation.normalize();
    local.mScale = world.mScale / group.mScale;
    return local;
}

bool ALGhostGroupModel::normalizeGroupCandidate(Group& group)
{
    if (group.mId.isNull() || group.mMembers.empty() ||
        !valid_mode(group.mMode) || !normalizeTransform(group.mWorld))
    {
        return false;
    }

    std::set<LLUUID> member_ids;
    for (Member& member : group.mMembers)
    {
        if (member.mInstanceId.isNull() ||
            !member_ids.insert(member.mInstanceId).second ||
            !normalizeTransform(member.mAuthoredLocal) ||
            !normalizeTransform(member.mLocal))
        {
            return false;
        }

        Transform authored_world =
            localToWorld(group.mWorld, member.mAuthoredLocal);
        Transform current_world =
            localToWorld(group.mWorld, member.mLocal);
        if (!normalizeTransform(authored_world) ||
            !normalizeTransform(current_world))
        {
            return false;
        }
    }
    return true;
}

bool ALGhostGroupModel::createGroup(
    const LLUUID& group_id, const std::string& name, EMode mode,
    const std::vector<WorldMember>& members)
{
    if (group_id.isNull() || members.empty() ||
        (mode != MODE_RIGID && mode != MODE_MOVE_FACE) ||
        mGroups.find(group_id) != mGroups.end())
    {
        return false;
    }

    std::set<LLUUID> unique;
    std::vector<WorldMember> normalized_members;
    normalized_members.reserve(members.size());
    LLVector3d centroid;
    for (size_t i = 0; i < members.size(); ++i)
    {
        const WorldMember& member = members[i];
        Transform world = member.mWorld;
        if (member.mInstanceId.isNull() ||
            !unique.insert(member.mInstanceId).second ||
            mMemberToGroup.find(member.mInstanceId) != mMemberToGroup.end() ||
            !normalizeTransform(world))
        {
            return false;
        }
        WorldMember normalized = member;
        normalized.mWorld = world;
        normalized_members.push_back(normalized);

        // A convex running mean avoids overflowing the intermediate sum when
        // multiple valid world positions are close to F64_MAX.
        const F64 previous_weight = (F64)i / (F64)(i + 1);
        const F64 member_weight = 1.0 / (F64)(i + 1);
        centroid = centroid * previous_weight +
                   world.mFoot * member_weight;
        if (!centroid.isFinite())
        {
            return false;
        }
    }

    Group group;
    group.mId = group_id;
    group.mName = name.empty() ? "Crowd" : name;
    group.mMode = mode;
    group.mWorld.mFoot = centroid;
    group.mWorld.mRotation.loadIdentity();
    group.mWorld.mScale = 1.f;
    group.mMembers.reserve(members.size());

    for (const WorldMember& source : normalized_members)
    {
        Member member;
        member.mInstanceId = source.mInstanceId;
        member.mAuthoredLocal = worldToLocal(group.mWorld, source.mWorld);
        member.mLocal = member.mAuthoredLocal;
        group.mMembers.push_back(member);
    }

    if (!normalizeGroupCandidate(group))
    {
        return false;
    }

    mGroups.emplace(group_id, std::move(group));
    for (const WorldMember& member : normalized_members)
    {
        mMemberToGroup.emplace(member.mInstanceId, group_id);
    }
    return true;
}

bool ALGhostGroupModel::cloneGroup(
    const LLUUID& source_group_id, const LLUUID& clone_group_id,
    const std::vector<LLUUID>& clone_member_ids,
    const LLVector3d& world_offset)
{
    const Group* source = findGroup(source_group_id);
    if (!source || clone_group_id.isNull() ||
        mGroups.find(clone_group_id) != mGroups.end() ||
        clone_member_ids.size() != source->mMembers.size() ||
        !world_offset.isFinite())
    {
        return false;
    }

    Group candidate = *source;
    candidate.mId = clone_group_id;
    candidate.mWorld.mFoot += world_offset;
    candidate.mRevision = 1;
    std::set<LLUUID> unique;
    for (size_t i = 0; i < candidate.mMembers.size(); ++i)
    {
        const LLUUID& id = clone_member_ids[i];
        if (id.isNull() || !unique.insert(id).second ||
            mMemberToGroup.find(id) != mMemberToGroup.end())
        {
            return false;
        }
        candidate.mMembers[i].mInstanceId = id;
    }
    if (!normalizeGroupCandidate(candidate))
    {
        return false;
    }

    mGroups.emplace(clone_group_id, std::move(candidate));
    for (const LLUUID& id : clone_member_ids)
    {
        mMemberToGroup.emplace(id, clone_group_id);
    }
    return true;
}

bool ALGhostGroupModel::eraseGroup(const LLUUID& group_id)
{
    const auto found = mGroups.find(group_id);
    if (found == mGroups.end())
    {
        return false;
    }
    for (const Member& member : found->second.mMembers)
    {
        mMemberToGroup.erase(member.mInstanceId);
    }
    mGroups.erase(found);
    return true;
}

void ALGhostGroupModel::clear()
{
    mGroups.clear();
    mMemberToGroup.clear();
}

ALGhostGroupModel::Group* ALGhostGroupModel::findGroup(const LLUUID& group_id)
{
    const auto found = mGroups.find(group_id);
    return found == mGroups.end() ? nullptr : &found->second;
}

const ALGhostGroupModel::Group* ALGhostGroupModel::findGroup(
    const LLUUID& group_id) const
{
    const auto found = mGroups.find(group_id);
    return found == mGroups.end() ? nullptr : &found->second;
}

ALGhostGroupModel::Group* ALGhostGroupModel::findGroupForMember(
    const LLUUID& instance_id)
{
    const auto found = mMemberToGroup.find(instance_id);
    return found == mMemberToGroup.end() ? nullptr : findGroup(found->second);
}

const ALGhostGroupModel::Group* ALGhostGroupModel::findGroupForMember(
    const LLUUID& instance_id) const
{
    const auto found = mMemberToGroup.find(instance_id);
    return found == mMemberToGroup.end() ? nullptr : findGroup(found->second);
}

bool ALGhostGroupModel::resolveMember(
    const LLUUID& instance_id, Transform& world) const
{
    const Group* group = findGroupForMember(instance_id);
    if (!group)
    {
        return false;
    }
    const auto found = std::find_if(
        group->mMembers.begin(), group->mMembers.end(),
        [&instance_id](const Member& member)
        {
            return member.mInstanceId == instance_id;
        });
    if (found == group->mMembers.end())
    {
        return false;
    }
    world = localToWorld(group->mWorld, found->mLocal);
    return normalizeTransform(world);
}

bool ALGhostGroupModel::setGroupTransform(
    const LLUUID& group_id, const Transform& requested)
{
    Group* group = findGroup(group_id);
    Transform world = requested;
    if (!group || !normalizeTransform(world))
    {
        return false;
    }
    Group candidate = *group;
    candidate.mWorld = world;
    ++candidate.mRevision;
    if (!normalizeGroupCandidate(candidate))
    {
        return false;
    }
    *group = std::move(candidate);
    return true;
}

bool ALGhostGroupModel::transformFromMember(
    const LLUUID& instance_id, const Transform& requested)
{
    Group* group = findGroupForMember(instance_id);
    Transform desired = requested;
    if (!group || !valid_mode(group->mMode) ||
        !normalizeTransform(desired))
    {
        return false;
    }

    auto found = std::find_if(
        group->mMembers.begin(), group->mMembers.end(),
        [&instance_id](const Member& member)
        {
            return member.mInstanceId == instance_id;
        });
    if (found == group->mMembers.end())
    {
        return false;
    }

    const Transform current = localToWorld(group->mWorld, found->mLocal);
    if (!current.isFinite())
    {
        return false;
    }

    LLQuaternion current_rotation = current.mRotation;
    current_rotation.normalize();
    LLQuaternion delta = ~current_rotation * desired.mRotation;
    delta.normalize();

    Transform next = group->mWorld;
    Transform next_local = found->mLocal;
    if (!normalizeTransform(next) || !normalizeTransform(next_local))
    {
        return false;
    }
    next.mRotation *= delta;
    next.mRotation.normalize();
    if (group->mMode == MODE_RIGID)
    {
        const F32 ratio = desired.mScale / current.mScale;
        if (!llfinite(ratio) || ratio <= 0.f)
        {
            return false;
        }
        next.mScale = llmax(MIN_GROUP_SCALE, next.mScale * ratio);
    }
    else
    {
        next_local.mScale = desired.mScale / next.mScale;
    }

    if (!normalizeTransform(next_local))
    {
        return false;
    }
    const LLVector3d resolved_offset =
        rotate_vector(next_local.mFoot * (F64)next.mScale, next.mRotation);
    next.mFoot = desired.mFoot - resolved_offset;
    if (!normalizeTransform(next))
    {
        return false;
    }

    Group candidate = *group;
    if (group->mMode == MODE_MOVE_FACE)
    {
        candidate.mMembers[
            static_cast<size_t>(found - group->mMembers.begin())].mLocal =
                next_local;
    }
    candidate.mWorld = next;
    ++candidate.mRevision;
    if (!normalizeGroupCandidate(candidate))
    {
        return false;
    }
    *group = std::move(candidate);
    return true;
}

bool ALGhostGroupModel::setMemberWorld(
    const LLUUID& instance_id, const Transform& requested, bool pin_member)
{
    Group* group = findGroupForMember(instance_id);
    Transform world = requested;
    if (!group || !group->mEditMembers || !valid_mode(group->mMode) ||
        !normalizeTransform(world))
    {
        return false;
    }
    auto found = std::find_if(
        group->mMembers.begin(), group->mMembers.end(),
        [&instance_id](const Member& member)
        {
            return member.mInstanceId == instance_id;
        });
    if (found == group->mMembers.end())
    {
        return false;
    }
    Transform group_world = group->mWorld;
    if (!normalizeTransform(group_world))
    {
        return false;
    }
    Transform local = worldToLocal(group_world, world);
    if (!normalizeTransform(local))
    {
        return false;
    }
    Transform resolved = localToWorld(group_world, local);
    if (!normalizeTransform(resolved))
    {
        return false;
    }

    Group candidate = *group;
    Member& candidate_member = candidate.mMembers[
        static_cast<size_t>(found - group->mMembers.begin())];
    candidate_member.mLocal = local;
    candidate_member.mPinned = pin_member;
    ++candidate.mRevision;
    if (!normalizeGroupCandidate(candidate))
    {
        return false;
    }
    *group = std::move(candidate);
    return true;
}

bool ALGhostGroupModel::setMemberAuthoredLocal(
    const LLUUID& instance_id, const Transform& requested)
{
    Group* group = findGroupForMember(instance_id);
    Transform authored = requested;
    if (!group || !normalizeTransform(authored))
    {
        return false;
    }
    auto found = std::find_if(
        group->mMembers.begin(), group->mMembers.end(),
        [&instance_id](const Member& member)
        {
            return member.mInstanceId == instance_id;
        });
    if (found == group->mMembers.end())
    {
        return false;
    }
    Group candidate = *group;
    Member& candidate_member = candidate.mMembers[
        static_cast<size_t>(found - group->mMembers.begin())];
    candidate_member.mAuthoredLocal = authored;
    if (!candidate_member.mPinned)
    {
        candidate_member.mLocal = authored;
    }
    ++candidate.mRevision;
    if (!normalizeGroupCandidate(candidate))
    {
        return false;
    }
    *group = std::move(candidate);
    return true;
}

bool ALGhostGroupModel::setMemberWorldRotation(
    const LLUUID& instance_id, const LLQuaternion& requested)
{
    Group* group = findGroupForMember(instance_id);
    LLQuaternion world_rotation = requested;
    const F32 magnitude = world_rotation.normalize();
    if (!group || !llfinite(magnitude) ||
        magnitude <= FP_MAG_THRESHOLD || !world_rotation.isFinite())
    {
        return false;
    }
    auto found = std::find_if(
        group->mMembers.begin(), group->mMembers.end(),
        [&instance_id](const Member& member)
        {
            return member.mInstanceId == instance_id;
        });
    if (found == group->mMembers.end())
    {
        return false;
    }
    LLQuaternion inverse = ~group->mWorld.mRotation;
    inverse.normalize();
    Group candidate = *group;
    Member& member = candidate.mMembers[
        static_cast<size_t>(found - group->mMembers.begin())];
    member.mLocal.mRotation = world_rotation * inverse;
    member.mLocal.mRotation.normalize();
    ++candidate.mRevision;
    if (!normalizeGroupCandidate(candidate))
    {
        return false;
    }
    *group = std::move(candidate);
    return true;
}

bool ALGhostGroupModel::resetMemberOffset(const LLUUID& instance_id)
{
    Group* group = findGroupForMember(instance_id);
    if (!group || !group->mEditMembers)
    {
        return false;
    }
    auto found = std::find_if(
        group->mMembers.begin(), group->mMembers.end(),
        [&instance_id](const Member& member)
        {
            return member.mInstanceId == instance_id;
        });
    if (found == group->mMembers.end())
    {
        return false;
    }
    Group candidate = *group;
    Member& candidate_member = candidate.mMembers[
        static_cast<size_t>(found - group->mMembers.begin())];
    candidate_member.mLocal = candidate_member.mAuthoredLocal;
    candidate_member.mPinned = false;
    ++candidate.mRevision;
    if (!normalizeGroupCandidate(candidate))
    {
        return false;
    }
    *group = std::move(candidate);
    return true;
}

bool ALGhostGroupModel::setMemberPinned(
    const LLUUID& instance_id, bool pinned)
{
    Group* group = findGroupForMember(instance_id);
    if (!group || !group->mEditMembers)
    {
        return false;
    }
    auto found = std::find_if(
        group->mMembers.begin(), group->mMembers.end(),
        [&instance_id](const Member& member)
        {
            return member.mInstanceId == instance_id;
        });
    if (found == group->mMembers.end())
    {
        return false;
    }
    found->mPinned = pinned;
    ++group->mRevision;
    return true;
}

bool ALGhostGroupModel::setEditMembers(
    const LLUUID& group_id, bool editing)
{
    Group* group = findGroup(group_id);
    if (!group)
    {
        return false;
    }
    group->mEditMembers = editing;
    ++group->mRevision;
    return true;
}

bool ALGhostGroupModel::setMode(const LLUUID& group_id, EMode mode)
{
    Group* group = findGroup(group_id);
    if (!group || (mode != MODE_RIGID && mode != MODE_MOVE_FACE))
    {
        return false;
    }
    group->mMode = mode;
    ++group->mRevision;
    return true;
}

bool ALGhostGroupModel::renameGroup(
    const LLUUID& group_id, const std::string& name)
{
    Group* group = findGroup(group_id);
    if (!group || name.empty())
    {
        return false;
    }
    group->mName = name;
    ++group->mRevision;
    return true;
}

bool ALGhostGroupModel::restoreGroupSnapshot(const Group& snapshot)
{
    if (snapshot.mId.isNull() || !valid_mode(snapshot.mMode))
    {
        return false;
    }
    const auto live_it = mGroups.find(snapshot.mId);
    if (live_it == mGroups.end())
    {
        return false;
    }
    const Group& live = live_it->second;
    if (live.mId.isNull() || live.mId != snapshot.mId ||
        snapshot.mMembers.size() != live.mMembers.size())
    {
        return false;
    }

    Group candidate = snapshot;
    if (!normalizeGroupCandidate(candidate))
    {
        return false;
    }

    std::set<LLUUID> candidate_ids;
    for (size_t i = 0; i < candidate.mMembers.size(); ++i)
    {
        Member& member = candidate.mMembers[i];
        if (member.mInstanceId.isNull() ||
            member.mInstanceId != live.mMembers[i].mInstanceId ||
            !candidate_ids.insert(member.mInstanceId).second)
        {
            return false;
        }
        const auto mapped = mMemberToGroup.find(member.mInstanceId);
        if (mapped == mMemberToGroup.end() ||
            mapped->second != snapshot.mId)
        {
            return false;
        }
    }

    // Reject an already-inconsistent mapping rather than silently concealing
    // it during rollback.
    size_t mapped_count = 0;
    for (const auto& mapping : mMemberToGroup)
    {
        if (mapping.second == snapshot.mId)
        {
            if (candidate_ids.find(mapping.first) == candidate_ids.end())
            {
                return false;
            }
            ++mapped_count;
        }
    }
    if (mapped_count != candidate.mMembers.size())
    {
        return false;
    }

    live_it->second = std::move(candidate);
    return true;
}

LLUUID ALGhostGroupModel::representative(const LLUUID& group_id) const
{
    const Group* group = findGroup(group_id);
    return (!group || group->mMembers.empty())
        ? LLUUID::null : group->mMembers.front().mInstanceId;
}

std::vector<LLUUID> ALGhostGroupModel::members(const LLUUID& group_id) const
{
    std::vector<LLUUID> result;
    const Group* group = findGroup(group_id);
    if (!group)
    {
        return result;
    }
    result.reserve(group->mMembers.size());
    for (const Member& member : group->mMembers)
    {
        result.push_back(member.mInstanceId);
    }
    return result;
}

std::vector<LLUUID> ALGhostGroupModel::authoringUnitMembers(
    const LLUUID& id) const
{
    if (const Group* group = findGroup(id))
    {
        return members(group->mId);
    }
    if (const Group* group = findGroupForMember(id))
    {
        return members(group->mId);
    }
    return id.isNull() ? std::vector<LLUUID>() :
        std::vector<LLUUID>(1, id);
}
