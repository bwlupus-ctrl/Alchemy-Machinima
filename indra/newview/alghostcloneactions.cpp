/**
 * @file alghostcloneactions.cpp
 * @brief Shared Clones page actions on list rows -- see alghostcloneactions.h.
 *
 * Every body below is the body the Clones page handler had before the menus
 * existed (alpanelghoststudio.cpp), moved here unchanged except that the rows
 * come in as an argument instead of from the list widget.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alghostcloneactions.h"

#include "alghostanimdrop.h"
#include "alghostclonemenulogic.h"
#include "alghostgroupmodel.h"
#include "llviewercontrol.h"

#include <limits>
#include <map>
#include <set>

namespace ALGhostCloneActions
{
std::vector<LLUUID> instances(const std::vector<LLUUID>& rows)
{
    std::vector<LLUUID> ids;
    std::set<LLUUID> unique;
    for (const LLUUID& value : rows)
    {
        const ALGhostGroupModel::Group* group =
            ALGhostStudio::instance().groupForMember(value);
        if (group && value == group->mId)
        {
            for (const ALGhostGroupModel::Member& member : group->mMembers)
            {
                if (unique.insert(member.mInstanceId).second)
                    ids.push_back(member.mInstanceId);
            }
        }
        else if (unique.insert(value).second)
        {
            ids.push_back(value);
        }
    }
    ids.erase(std::remove_if(ids.begin(), ids.end(), [](const LLUUID& id)
    { return ALGhostStudio::instance().isDeleting(id); }), ids.end());
    return ids;
}

std::vector<LLUUID> units(const std::vector<LLUUID>& rows)
{
    std::vector<LLUUID> ids;
    std::set<LLUUID> unique;
    std::set<LLUUID> selected_group_headers;
    ALGhostStudio& studio = ALGhostStudio::instance();
    for (const LLUUID& row_value : rows)
    {
        const ALGhostGroupModel::Group* group =
            studio.groupForMember(row_value);
        if (group && row_value == group->mId)
        {
            selected_group_headers.insert(group->mId);
        }
    }
    for (const LLUUID& row_value : rows)
    {
        const ALGhostGroupModel::Group* group =
            studio.groupForMember(row_value);
        if (group && row_value != group->mId &&
            selected_group_headers.find(group->mId) !=
                selected_group_headers.end())
        {
            // A selected header already owns this authoring unit. Applying the
            // same command to one of its selected child rows as well would move
            // the group and then add an unintended member-local edit.
            continue;
        }
        const LLUUID unit_id =
            group && (row_value == group->mId || !group->mEditMembers)
                ? group->mId : row_value;
        if (unit_id.notNull() && unique.insert(unit_id).second)
        {
            ids.push_back(unit_id);
        }
    }
    return ids;
}

LLUUID rowFor(const LLUUID& instance_id)
{
    const ALGhostGroupModel::Group* group =
        ALGhostStudio::instance().groupForMember(instance_id);
    return group && !group->mEditMembers ? group->mId : instance_id;
}

bool applyUnitTransform(const LLUUID& unit_id, const LLVector3d& foot,
                        const LLQuaternion& rotation, F32 scale)
{
    if (unit_id.isNull())
    {
        return false;
    }
    ALGhostStudio& studio = ALGhostStudio::instance();
    const ALGhostGroupModel::Group* group =
        studio.groupForMember(unit_id);
    return group && unit_id == group->mId
        ? studio.transformGroup(unit_id, foot, rotation, scale)
        : studio.transformUnit(unit_id, foot, rotation, scale);
}

void setPaused(const std::vector<LLUUID>& rows, bool paused)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    for (const LLUUID& id : instances(rows))
    {
        studio.setInstancePaused(id, paused);
    }
}

void setDriveMode(const std::vector<LLUUID>& rows, ALGhostStudio::EDriveMode mode,
                  const LLUUID& directed_anim)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    for (const LLUUID& id : instances(rows))
    {
        const ALGhostStudio::Instance* inst = studio.getInstance(id);
        if (inst && inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE)
        {
            studio.setInstanceDriveMode(id, mode, directed_anim);
        }
    }
}

void setOverlayStyle(const std::vector<LLUUID>& rows, S32 style)
{
    style = ALGhostStudio::resolveOverlayStyle(style);
    if (!ALGhostStudio::isOverlayStyle(style)) return;
    for (const LLUUID& id : instances(rows))
    {
        if (ALGhostStudio::Instance* inst =
                ALGhostStudio::instance().getInstance(id))
        {
            inst->mStyle = style;
            ALGhostStudio::instance().touchInstances(); // [GhostPanelPerf] Style column
        }
    }
}

bool freezeOverlays(const std::vector<LLUUID>& rows)
{
    bool any_failed = false;
    for (const LLUUID& id : instances(rows))
        any_failed = !ALGhostStudio::instance().freezeInstance(id) || any_failed;
    return any_failed;
}

bool deleteRows(const std::vector<LLUUID>& rows, bool* skipped_members)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    std::set<LLUUID> removed_units;
    bool removed_any = false;
    for (const LLUUID& value : rows)
    {
        const ALGhostGroupModel::Group* group =
            studio.groupForMember(value);
        if (group)
        {
            if (value != group->mId)
            {
                // Expanded child rows are positioning controls, not implicit
                // whole-crowd delete buttons.
                if (skipped_members) *skipped_members = true;
                continue;
            }
            if (!removed_units.insert(group->mId).second ||
                group->mMembers.empty())
            {
                continue;
            }
            studio.requestRemoveInstance(group->mMembers.front().mInstanceId);
            removed_any = true;
        }
        else if (removed_units.insert(value).second)
        {
            studio.requestRemoveInstance(value);
            removed_any = true;
        }
    }
    return removed_any;
}

DuplicateResult duplicateRows(const std::vector<LLUUID>& rows, bool in_place)
{
    DuplicateResult result;
    std::set<LLUUID> handled;
    ALGhostStudio& studio = ALGhostStudio::instance();
    std::set<LLUUID> selected_group_headers;
    for (const LLUUID& value : rows)
    {
        const ALGhostGroupModel::Group* group =
            studio.groupForMember(value);
        if (group && value == group->mId)
        {
            selected_group_headers.insert(group->mId);
        }
    }
    for (const LLUUID& value : rows)
    {
        if (!handled.insert(value).second)
        {
            continue;
        }
        const ALGhostGroupModel::Group* group =
            studio.groupForMember(value);
        if (group && value != group->mId &&
            selected_group_headers.find(group->mId) !=
                selected_group_headers.end())
        {
            continue;
        }
        if (group && value == group->mId)
        {
            const std::vector<LLUUID> copies =
                studio.duplicateGroup(group->mId, in_place);
            if (!copies.empty())
            {
                const ALGhostStudio::Instance* first =
                    studio.getInstance(copies.front());
                if (first)
                {
                    result.mRows.push_back(first->mGroupId);
                    if (result.mFirst.isNull())
                        result.mFirst = first->mGroupId;
                }
            }
        }
        else if (ALGhostStudio::Instance* inst =
                     in_place ? studio.duplicateInstanceInPlace(value)
                              : studio.duplicateInstance(value))
        {
            result.mRows.push_back(inst->mId);
            if (result.mFirst.isNull()) result.mFirst = inst->mId;
        }
    }
    return result;
}

void toggleSolo(const std::vector<LLUUID>& rows)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    studio.setSolo(studio.getSolo() ? std::vector<LLUUID>() : instances(rows));
}

const ALGhostGroupModel::Group* collapsedGroup(const LLUUID& instance_id)
{
    const ALGhostGroupModel::Group* group = ALGhostStudio::instance().groupForMember(instance_id);
    return group && !group->mEditMembers ? group : nullptr;
}

AnimationDropPlan planAnimationDrop(const LLUUID& hit_instance)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    AnimationDropPlan plan;
    const ALGhostGroupModel::Group* group = collapsedGroup(hit_instance);
    std::vector<LLUUID> members;
    if (group)
    {
        members = instances(std::vector<LLUUID>{ group->mId });
        plan.mCollapsedGroup = true;
        plan.mGroupName = group->mName;
    }
    for (const LLUUID& id : ALGhostCloneMenuLogic::dropTargets(hit_instance, group != nullptr, members))
    {
        const ALGhostStudio::Instance* inst = studio.getInstance(id);
        // Only clones that can actually play: a live backing avatar is required.
        if (inst && !inst->mDeleting && inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE &&
            studio.resolveEntityClone(id))
        {
            plan.mTargets.push_back(id);
        }
    }
    return plan;
}

void syncRows(const std::vector<LLUUID>& rows)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    const std::vector<LLUUID> ids = instances(rows);
    studio.syncOffsets(ids);
    for (const LLUUID& id : ids) studio.restartInstanceAnimation(id);
}

bool applyStartOffsets(const std::vector<LLUUID>& ids, S32 mode, U32 seed)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    if (ids.empty() || mode < 0 || mode > 2) return false;
    const S32 direction = gSavedSettings.getS32("GhostStudioRippleFrom");
    const F32 spread = llclamp(gSavedSettings.getF32("GhostStudioStartSpread"), 0.f, 10.f);
    F64 low = std::numeric_limits<F64>::max(), high = -low;
    for (const LLUUID& id : ids)
    {
        const ALGhostStudio::Instance* inst = studio.getInstance(id);
        if (!inst || inst->mDeleting) return false;
        const F64 v = ALCloneQol::ripple(inst->mCrowdLocalX, inst->mCrowdLocalY, direction);
        low = llmin(low, v); high = llmax(high, v);
    }
    for (const LLUUID& id : ids)
    {
        const ALGhostStudio::Instance* inst = studio.getInstance(id);
        U64 stable = 14695981039346656037ULL;
        for (U8 byte : inst->mId.mData) stable = (stable ^ byte)*1099511628211ULL;
        const F64 unit = mode == 1 ? ALCloneQol::randomUnit(seed, stable) : mode == 2 ?
            ALCloneQol::normalise(ALCloneQol::ripple(inst->mCrowdLocalX, inst->mCrowdLocalY, direction), low, high) : 0.;
        studio.setStartOffset(id, F32(unit*spread));
    }
    studio.touchInstances();
    return true;
}

namespace
{
// Session-only: the Stagger mode last applied to each group, with the offsets it
// produced. The mode counts only while the members still hold those offsets (a
// Crowd tab edit or any other change makes it unknown); entries of dissolved or
// deleted groups are erased.
struct AppliedStagger
{
    S32 mMode = -1;
    std::vector<std::pair<LLUUID, F32>> mOffsets;
};
std::map<LLUUID, AppliedStagger>& staggerModes()
{
    static std::map<LLUUID, AppliedStagger> modes;
    return modes;
}

void pruneStaggerModes()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    auto& modes = staggerModes();
    for (auto it = modes.begin(); it != modes.end();)
    {
        const ALGhostGroupModel::Group* group = studio.groupForMember(it->first);
        it = group && group->mId == it->first ? std::next(it) : modes.erase(it);
    }
}

// The group's live entity members.
std::vector<LLUUID> staggerMembers(const ALGhostGroupModel::Group& group)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    std::vector<LLUUID> ids;
    for (const ALGhostGroupModel::Member& member : group.mMembers)
    {
        const ALGhostStudio::Instance* inst = studio.getInstance(member.mInstanceId);
        if (inst && !inst->mDeleting && inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE)
        {
            ids.push_back(member.mInstanceId);
        }
    }
    return ids;
}
}

const char* staggerRefusal(const LLUUID& instance_id)
{
    const ALGhostGroupModel::Group* group = collapsedGroup(instance_id);
    if (!group)
    {
        return "GhostCloneMenuNoCrowd";
    }
    ALGhostStudio& studio = ALGhostStudio::instance();
    const std::vector<LLUUID> members = staggerMembers(*group);
    if (members.empty())
    {
        return "GhostCloneMenuNotReady";
    }
    // Ripple reads each member's place in its crowd frame, so the members must
    // share exactly one crowd (a regrouped subset of a crowd is fine: only the
    // members are touched).
    LLUUID crowd;
    for (const LLUUID& id : members)
    {
        const ALGhostStudio::Instance* inst = studio.getInstance(id);
        if (inst->mCrowdId.isNull() || (crowd.notNull() && inst->mCrowdId != crowd))
        {
            return "GhostCloneMenuNoCrowd";
        }
        crowd = inst->mCrowdId;
    }
    return nullptr;
}

bool staggerAvailable(const LLUUID& instance_id)
{
    return staggerRefusal(instance_id) == nullptr;
}

S32 staggerCurrent(const LLUUID& instance_id)
{
    const ALGhostGroupModel::Group* group = collapsedGroup(instance_id);
    if (!group)
    {
        return -1;
    }
    ALGhostStudio& studio = ALGhostStudio::instance();
    std::vector<std::pair<LLUUID, F32>> offsets;
    for (const LLUUID& id : staggerMembers(*group))
    {
        offsets.emplace_back(id, studio.getInstance(id)->mStartOffset);
    }
    bool all_zero = true;
    for (const auto& offset : offsets) all_zero = all_zero && offset.second <= 0.f;
    pruneStaggerModes();
    const auto found = staggerModes().find(group->mId);
    const bool still_applied = found != staggerModes().end() && found->second.mOffsets == offsets;
    return ALGhostCloneMenuLogic::staggerChecked(still_applied ? found->second.mMode : -1, all_zero);
}

StaggerState snapshotStagger(const LLUUID& instance_id)
{
    StaggerState state;
    const ALGhostGroupModel::Group* group = collapsedGroup(instance_id);
    if (!group)
    {
        return state;
    }
    ALGhostStudio& studio = ALGhostStudio::instance();
    state.mGroup = group->mId;
    for (const LLUUID& id : staggerMembers(*group))
    {
        state.mOffsets.emplace_back(id, studio.getInstance(id)->mStartOffset);
    }
    pruneStaggerModes();
    const auto found = staggerModes().find(group->mId);
    state.mMode = found != staggerModes().end() && found->second.mOffsets == state.mOffsets ? found->second.mMode : -1;
    return state;
}

bool setStagger(const LLUUID& instance_id, S32 mode)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    const ALGhostGroupModel::Group* group = collapsedGroup(instance_id);
    if (!group || staggerRefusal(instance_id) || mode < 0 || mode > 2)
    {
        return false;
    }
    const std::vector<LLUUID> members = staggerMembers(*group);
    const U32 seed = studio.getInstance(members.front())->mCrowdSeed;
    if (!applyStartOffsets(members, mode, seed))
    {
        return false;
    }
    if (mode == 0)
    {
        for (const LLUUID& id : members)
        {
            studio.restartInstanceAnimation(id);       // the Sync path restarts each animation
        }
    }
    AppliedStagger applied;
    applied.mMode = mode;
    for (const LLUUID& id : members)
    {
        applied.mOffsets.emplace_back(id, studio.getInstance(id)->mStartOffset);
    }
    staggerModes()[group->mId] = applied;
    return true;
}

bool restoreStagger(const StaggerState& state)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    std::vector<std::pair<LLUUID, F32>> alive;
    for (const auto& offset : state.mOffsets)
    {
        const ALGhostStudio::Instance* inst = studio.getInstance(offset.first);
        if (inst && !inst->mDeleting)
        {
            alive.push_back(offset);
        }
    }
    if (alive.empty())
    {
        return false;
    }
    for (const auto& offset : alive)
    {
        studio.setStartOffset(offset.first, offset.second);
        studio.restartInstanceAnimation(offset.first);
    }
    if (state.mGroup.notNull())
    {
        if (state.mMode < 0)
        {
            staggerModes().erase(state.mGroup);
        }
        else
        {
            AppliedStagger applied;
            applied.mMode = state.mMode;
            applied.mOffsets = state.mOffsets;
            staggerModes()[state.mGroup] = applied;
        }
    }
    return true;
}

namespace
{
// The instance that stands for each authoring unit of the rows, as the page's
// selectedInstance() does: a collapsed group's first live member, else the instance.
std::vector<LLUUID> unitRepresentatives(const std::vector<LLUUID>& rows)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    std::vector<LLUUID> reps;
    for (const LLUUID& unit : units(rows))
    {
        LLUUID rep = unit;
        const ALGhostGroupModel::Group* group = studio.groupForMember(unit);
        if (group && unit == group->mId)
        {
            rep.setNull();
            for (const ALGhostGroupModel::Member& member : group->mMembers)
            {
                if (studio.getInstance(member.mInstanceId) && !studio.isDeleting(member.mInstanceId))
                {
                    rep = member.mInstanceId;
                    break;
                }
            }
        }
        const ALGhostStudio::Instance* inst = rep.notNull() ? studio.getInstance(rep) : nullptr;
        if (inst && !inst->mDeleting && std::find(reps.begin(), reps.end(), rep) == reps.end())
        {
            reps.push_back(rep);
        }
    }
    return reps;
}

std::vector<RecentAnimation>& recentList()
{
    static std::vector<RecentAnimation> list;      // session only: never saved
    return list;
}
}

bool hasExpandedMember(const std::vector<LLUUID>& rows)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    // A selected group header already owns its authoring unit (the units() rule): its member rows add nothing.
    std::set<LLUUID> headers;
    for (const LLUUID& row : rows)
    {
        const ALGhostGroupModel::Group* group = studio.groupForMember(row);
        if (group && row == group->mId)
        {
            headers.insert(group->mId);
        }
    }
    for (const LLUUID& row : rows)
    {
        const ALGhostGroupModel::Group* group = studio.groupForMember(row);
        if (group && group->mEditMembers && row != group->mId && headers.find(group->mId) == headers.end())
        {
            return true;
        }
    }
    return false;
}

bool lookAt(const std::vector<LLUUID>& rows, ALGhostStudio::ELookTarget target, const LLUUID& target_id)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    bool any = false;
    if (hasExpandedMember(rows))
    {
        return false;
    }
    const std::vector<LLUUID> reps = unitRepresentatives(rows);
    if (reps.empty())
    {
        return false;
    }
    // An armed Turn owns body yaw (faceInstance declines while it is set): clear it first so the clone
    // looks at the target right away. setLookTarget writes every member of the representative's
    // authoring unit, so Turn is cleared over exactly those members: a collapsed-group member row
    // cannot leave a sibling with an armed Turn.
    for (const LLUUID& rep : reps)
    {
        for (const LLUUID& member : studio.groupMembers(rep))
        {
            if (studio.getInstance(member) && !studio.isDeleting(member))
            {
                studio.setTurnTarget(member, ALGhostStudio::TURN_MODE_OFF, ALGhostStudio::LOOK_TARGET_CAMERA,
                                     LLUUID::null, LLVector3d());
            }
        }
    }
    for (const LLUUID& rep : reps)
    {
        // The page's Keep facing + Face now: store the target, then aim.
        studio.setLookTarget(rep, target, target_id, true);
        studio.faceInstance(rep);
        any = true;
    }
    return any;
}

void stopLooking(const std::vector<LLUUID>& rows)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    if (hasExpandedMember(rows))
    {
        return;
    }
    for (const LLUUID& rep : unitRepresentatives(rows))
    {
        const ALGhostStudio::Instance* inst = studio.getInstance(rep);
        if (inst)
        {
            const ALGhostStudio::ELookTarget target = inst->mLookTarget;
            const LLUUID target_id = inst->mLookTargetId;
            studio.setLookTarget(rep, target, target_id, false);      // Keep facing off
        }
    }
}

bool turnTo(const std::vector<LLUUID>& rows, ALGhostStudio::ELookTarget target, const LLUUID& target_id)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    bool any = false;
    if (hasExpandedMember(rows))
    {
        return false;
    }
    for (const LLUUID& id : instances(rows))
    {
        const ALGhostStudio::Instance* inst = studio.getInstance(id);
        if (inst && !inst->mDeleting)
        {
            studio.setTurnTarget(id, ALGhostStudio::TURN_MODE_ONCE, target, target_id, LLVector3d());
            any = true;
        }
    }
    return any;
}

void stopTurning(const std::vector<LLUUID>& rows)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    if (hasExpandedMember(rows))
    {
        return;
    }
    for (const LLUUID& id : instances(rows))
    {
        if (studio.getInstance(id))
        {
            studio.setTurnTarget(id, ALGhostStudio::TURN_MODE_OFF, ALGhostStudio::LOOK_TARGET_CAMERA,
                                 LLUUID::null, LLVector3d());
        }
    }
}

bool restartRows(const std::vector<LLUUID>& rows)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    bool any = false;
    for (const LLUUID& id : instances(rows))
    {
        any = studio.restartInstanceAnimation(id) || any;
    }
    return any;
}

void noteRecentAnimation(const LLUUID& asset_id, const std::string& anim_name)
{
    if (asset_id.isNull())
    {
        return;
    }
    // A name that carries the asset id is never kept: the entry shows the fallback label.
    const std::string name = ALGhostAnimDrop::mentionsUuid(anim_name, asset_id) ? std::string() : anim_name;
    ALGhostCloneMenuLogic::recentPush(recentList(), asset_id, name);
}

const std::vector<RecentAnimation>& recentAnimations()
{
    return recentList();
}

void removeRecentAnimation(const LLUUID& asset_id)
{
    std::vector<RecentAnimation>& list = recentList();
    list.erase(std::remove_if(list.begin(), list.end(),
                              [&asset_id](const RecentAnimation& e) { return e.mAsset == asset_id; }),
               list.end());
}

std::string swapSource(const std::vector<LLUUID>& rows, const LLUUID& source, const std::string& label)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    // Snapshot the instances: a runtime replacement synchronously rewrites outside references.
    const std::vector<LLUUID> ids = instances(rows);
    const auto* source_clone = studio.findInstanceByRuntime(source);
    const LLUUID source_instance = source_clone ? source_clone->mId : source;
    bool skipped = false;
    std::string status;
    for (const LLUUID& id : ids)
    {
        if (id == source_instance) skipped = true;
        else if (!studio.swapSource(id, source, label)) status = studio.swapStatus();
    }
    if (skipped && status.empty()) status = "Swap: skipped the picked source clone (cannot copy itself)";
    return status;
}

bool setEntityLook(const std::vector<LLUUID>& rows, ALGhostStudio::EGhostLook look)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    bool any = false;
    for (const LLUUID& id : instances(rows))
    {
        any = studio.setInstanceLook(id, look) || any;
    }
    return any;
}

std::vector<LLUUID> fadeTargets(const std::vector<LLUUID>& rows, bool fade_out)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    std::vector<LLUUID> ids;
    for (const LLUUID& id : instances(rows))
    {
        const ALGhostStudio::Instance* inst = studio.getInstance(id);
        if (inst && !inst->mDeleting &&
            ALGhostCloneMenuLogic::fadeApplies(inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE, fade_out, inst->mUserHidden))
        {
            ids.push_back(id);
        }
    }
    return ids;
}

std::vector<FadeBefore> fadeRows(const std::vector<LLUUID>& rows, bool fade_out)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    std::vector<FadeBefore> before;
    for (const LLUUID& id : fadeTargets(rows, fade_out))
    {
        FadeBefore saved;
        saved.mId = id;
        saved.mHidden = studio.getInstance(id)->mUserHidden;
        if (studio.fadeInstance(id, fade_out, true))
        {
            before.push_back(saved);
        }
    }
    return before;
}

bool restoreFade(const std::vector<FadeBefore>& before, bool redo, bool fade_out)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    bool any = false;
    for (const FadeBefore& saved : before)
    {
        const ALGhostStudio::Instance* inst = studio.getInstance(saved.mId);
        if (inst && !inst->mDeleting)
        {
            any = (redo ? studio.fadeInstance(saved.mId, fade_out, true)
                        : studio.fadeInstance(saved.mId, saved.mHidden, false)) || any;
        }
    }
    return any;
}
}
