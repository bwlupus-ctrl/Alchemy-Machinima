/**
 * @file alghostmanipproxy.cpp
 * @brief See alghostmanipproxy.h and doc/CLONE_MANIP_PHASE1B_BLUEPRINT.md.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alghostmanipproxy.h"

#include "alghoststudio.h"
#include "altoolghostedit.h"

#include "alkithistory.h"           // [CloneEditUndo]
#include "alkitshell.h"             // [CloneEditUndo]
#include "llagent.h"
#include "llfloaterreg.h"           // [CloneEditUndo]
#include "llnotificationsutil.h"    // [CloneEditUndo]
#include "lldrawable.h"
#include "llghostavatar.h"
#include "llprimitive.h"            // FLAGS_OBJECT_*, LL_PCODE_* profile/path
#include "llselectmgr.h"
#include "lltoolcomp.h"             // LLToolCompTranslate
#include "lltoolmgr.h"              // LLToolMgr, gBasicToolset, LLToolset
#include "llviewerobjectlist.h"     // gObjectList
#include "llviewerregion.h"
#include "llvolume.h"               // LLVolumeParams
#include "llvolumemgr.h"            // LLVolumeLODGroup::NUM_LODS
#include "llvovolume.h"             // LLVOVolume
#include "pipeline.h"               // gPipeline

namespace
{
// The proxy volume is a UNIT cube. For an individual clone, local_center is the
// middle of the canonical foot-anchored avatar box. For a collapsed group it is
// the centre of the union of every member's rotated/scaled avatar box in GROUP
// LOCAL space. Keeping this conversion explicit is what lets a bounds-centred
// stock gizmo drive a group whose authoring pivot is somewhere else.
LLVector3d proxyCenterFromPivot(const LLVector3d& pivot_global,
                                const LLQuaternion& rotation,
                                const LLVector3& local_center, F32 scale)
{
    LLVector3 world_offset = local_center * scale;
    world_offset *= rotation;
    return pivot_global + LLVector3d(world_offset);
}

LLVector3d pivotFromProxyCenter(const LLVector3d& center_global,
                                const LLQuaternion& rotation,
                                const LLVector3& local_center, F32 scale)
{
    LLVector3 world_offset = local_center * scale;
    world_offset *= rotation;
    return center_global - LLVector3d(world_offset);
}

bool groupLocalBounds(const ALGhostGroupModel::Group& group,
                      F32 avatar_width, F32 avatar_depth, F32 avatar_height,
                      const LLVector3d& world_foot, const LLQuaternion& world_rotation,
                      F32 world_scale,
                      LLVector3& center, LLVector3& dimensions)
{
    bool have_corner = false;
    LLVector3 minimum;
    LLVector3 maximum;
    for (const ALGhostGroupModel::Member& member : group.mMembers)
    {
        LLVector3 visual_center, visual_size;
        const LLGhostAvatar* ghost =
            ALGhostStudio::instance().resolveEntityClone(member.mInstanceId);
        const bool visual = ghost && ghost->getVisualBounds(visual_center, visual_size);
        if (!member.mLocal.isFinite())
        {
            return false;
        }
        const LLVector3 local_foot(
            (F32)member.mLocal.mFoot.mdV[VX],
            (F32)member.mLocal.mFoot.mdV[VY],
            (F32)member.mLocal.mFoot.mdV[VZ]);
        if (!local_foot.isFinite())
        {
            return false;
        }
        for (S32 x = 0; x < 2; ++x)
        {
            for (S32 y = 0; y < 2; ++y)
            {
                for (S32 z = 0; z < 2; ++z)
                {
                    LLVector3 corner(
                        (x ? 0.5f : -0.5f) * avatar_width,
                        (y ? 0.5f : -0.5f) * avatar_depth,
                        z ? avatar_height : 0.f);
                    if (visual)
                    {
                        corner.setVec((x ? .5f : -.5f) * visual_size[VX],
                                      (y ? .5f : -.5f) * visual_size[VY],
                                      (z ? .5f : -.5f) * visual_size[VZ]);
                        corner += visual_center;
                        corner -= gAgent.getPosAgentFromGlobal(world_foot);
                        corner *= ~world_rotation;
                        corner /= world_scale; // rendered extents are already scaled
                    }
                    else
                    {
                        corner *= member.mLocal.mScale;
                        corner *= member.mLocal.mRotation;
                        corner += local_foot;
                    }
                    if (!corner.isFinite())
                    {
                        return false;
                    }
                    if (!have_corner)
                    {
                        minimum = maximum = corner;
                        have_corner = true;
                    }
                    else
                    {
                        minimum.setVec(
                            llmin(minimum.mV[VX], corner.mV[VX]),
                            llmin(minimum.mV[VY], corner.mV[VY]),
                            llmin(minimum.mV[VZ], corner.mV[VZ]));
                        maximum.setVec(
                            llmax(maximum.mV[VX], corner.mV[VX]),
                            llmax(maximum.mV[VY], corner.mV[VY]),
                            llmax(maximum.mV[VZ], corner.mV[VZ]));
                    }
                }
            }
        }
    }
    if (!have_corner)
    {
        return false;
    }
    center = (minimum + maximum) * 0.5f;
    dimensions = maximum - minimum;
    return center.isFinite() && dimensions.isFinite() &&
           dimensions.mV[VX] > 0.f && dimensions.mV[VY] > 0.f &&
           dimensions.mV[VZ] > 0.f;
}

F32 uniformScaleFromProxy(const LLVector3& proxy_dimensions,
                          const LLVector3& local_dimensions,
                          F32 start_scale)
{
    F32 result = start_scale;
    F32 largest_relative_change = -1.f;
    for (S32 axis = VX; axis <= VZ; ++axis)
    {
        const F32 base = local_dimensions.mV[axis];
        const F32 candidate = base > 0.f
            ? proxy_dimensions.mV[axis] / base : start_scale;
        if (!llfinite(candidate) || candidate <= 0.f)
        {
            continue;
        }
        const F32 relative_change =
            fabsf(candidate - start_scale) / llmax(0.0001f, start_scale);
        if (relative_change > largest_relative_change)
        {
            largest_relative_change = relative_change;
            result = candidate;
        }
    }
    return result;
}

// [CloneEditUndo] One committed Edit-box drag = one Director history step.
// The code between the BEGIN/END markers is compiled and executed against
// doubles by scripts/code_tools/clone_edit_undo_check.py; keep it free of
// viewer-only dependencies (it may use only the studio/history calls below).
void cloneEditHistoryBegin();   // cancel any live drag before a replay mutates the clone
void cloneEditHistoryEnd();     // resync the proxy after a replay

// [CloneEditUndo-BEGIN]
ALGhostStudio::Instance* backingInstanceForUnit(
    ALGhostStudio& studio, const LLUUID& unit_id)
{
    if (ALGhostStudio::Instance* inst = studio.getInstance(unit_id))
    {
        return inst;
    }
    const ALGhostGroupModel::Group* group =
        studio.groupForMember(unit_id);
    if (!group || group->mId != unit_id || group->mMembers.empty())
    {
        return nullptr;
    }
    return studio.getInstance(group->mMembers.front().mInstanceId);
}

struct UnitXform
{
    LLVector3d   mFoot;
    LLQuaternion mRotation;
    F32          mScale = 1.f;
};

// What the drag edited: an ungrouped clone, one member of a group with member
// editing on, or a whole group (keyed by the GROUP id, never a member id).
struct EditContext
{
    LLUUID mUnitId;             // whole group: group id; otherwise the clone id
    LLUUID mGroupId;            // null = ungrouped clone
    bool   mWholeGroup = false;
    bool   mPinKnown = false;   // member-only edit inside a group
    bool   mPinnedBefore = false;
    bool   mPinnedAfter = false;
};

bool unitXformChanged(const UnitXform& a, const UnitXform& b)
{
    F32 quat_delta = 0.f;
    for (S32 i = 0; i < 4; ++i)
    {
        quat_delta = llmax(quat_delta, fabsf(a.mRotation.mQ[i] - b.mRotation.mQ[i]));
    }
    return dist_vec(a.mFoot, b.mFoot) > 0.0005 ||
           quat_delta > 0.00001f ||
           fabsf(a.mScale - b.mScale) > 0.00001f;
}

bool editMemberPinned(ALGhostStudio& studio, const LLUUID& unit_id, bool& pinned)
{
    const ALGhostGroupModel::Group* group = studio.groupForMember(unit_id);
    if (!group)
    {
        return false;
    }
    for (const ALGhostGroupModel::Member& member : group->mMembers)
    {
        if (member.mInstanceId == unit_id)
        {
            pinned = member.mPinned;
            return true;
        }
    }
    return false;
}

EditContext captureEditContext(ALGhostStudio& studio, const LLUUID& unit_id)
{
    EditContext ctx;
    ctx.mUnitId = unit_id;
    const ALGhostGroupModel::Group* group = studio.groupForMember(unit_id);
    if (group)
    {
        ctx.mGroupId = group->mId;
        ctx.mWholeGroup = unit_id == group->mId || !group->mEditMembers;
        if (ctx.mWholeGroup)
        {
            ctx.mUnitId = group->mId;
        }
    }
    return ctx;
}

// Existence + context check, with no mutation. Used by the history validator
// (so a dead step is discarded and never blocks older steps) and by replay.
bool editContextValid(ALGhostStudio& studio, const EditContext& ctx, std::string& reason)
{
    const ALGhostGroupModel::Group* group = studio.groupForMember(ctx.mUnitId);
    if (ctx.mWholeGroup)
    {
        if (!group || group->mId != ctx.mGroupId)
        {
            reason = "That clone is no longer available";
            return false;
        }
        if (group->mEditMembers)
        {
            reason = "That group is being edited per member";
            return false;
        }
        return true;
    }
    if (!studio.getInstance(ctx.mUnitId))
    {
        reason = "That clone is no longer available";
        return false;
    }
    if (ctx.mGroupId.isNull())
    {
        if (group)
        {
            reason = "That clone has since joined a group";
            return false;
        }
        return true;
    }
    if (!group || group->mId != ctx.mGroupId || !group->mEditMembers)
    {
        reason = "That clone is no longer edited as a group member";
        return false;
    }
    return true;
}

// Any condition under which the replay could return false is checked HERE, in
// the validator (prepare phase): history discards an invalid step, whereas a
// failed replay stays on the stack and would block every older step.
// Mirrors every public-state rejection of applyGroupTransforms(): transformGroup
// and a member transformUnit (member editing) both apply the ENTIRE group, and
// reject if any member is gone / ungrouped / driven by motion or chaos, or if
// an entity member has no live runtime (region teardown, parked clone).
bool groupApplyFeasible(ALGhostStudio& studio, const ALGhostGroupModel::Group& group,
                        std::string& reason)
{
    for (const ALGhostGroupModel::Member& member : group.mMembers)
    {
        const ALGhostStudio::Instance* inst = studio.getInstance(member.mInstanceId);
        if (!inst || inst->mGroupId != group.mId ||
            inst->mMotion != ALGhostStudio::MOTION_OFF || inst->mMotionHasBase ||
            inst->mChaosEnabled || inst->mChaosHasBase)
        {
            reason = "A clone in that group cannot be moved right now";
            return false;
        }
        if (inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE &&
            !studio.resolveEntityClone(member.mInstanceId))
        {
            reason = "A clone in that group is not loaded right now";
            return false;
        }
    }
    return true;
}

bool editStepFeasible(ALGhostStudio& studio, const EditContext& ctx,
                      const UnitXform& target, std::string& reason)
{
    if (ctx.mWholeGroup || !ctx.mGroupId.isNull())
    {
        const ALGhostGroupModel::Group* group = studio.groupForMember(ctx.mUnitId);
        if (!group || !groupApplyFeasible(studio, *group, reason))
        {
            if (!group)
            {
                reason = "That clone is no longer available";
            }
            return false;
        }
    }
    if (ctx.mWholeGroup)
    {
        F32 minimum = 0.f;
        F32 maximum = 0.f;
        if (!studio.getGroupScaleLimits(ctx.mUnitId, minimum, maximum) ||
            target.mScale < minimum || target.mScale > maximum)
        {
            reason = "That group can no longer be scaled to that size";
            return false;
        }
        return true;
    }
    if (target.mScale < GHOST_SCALE_MIN || target.mScale > GHOST_SCALE_MAX)
    {
        reason = "That clone can no longer be scaled to that size";
        return false;
    }
    return true;
}

bool editStepValid(ALGhostStudio& studio, const EditContext& ctx,
                   const UnitXform& before, const UnitXform& after,
                   bool redo, std::string& reason)
{
    return editContextValid(studio, ctx, reason) &&
           editStepFeasible(studio, ctx, redo ? after : before, reason);
}

// Restore a member pin bit WITHOUT replacing its just-restored transform.
// setGroupMemberPinned(false) resets the member to its authored slot, which
// would undo e.g. an aimed facing; a transform already pinned the member, so
// only the bit is flipped (setGroupMemberPinned(true) keeps the list rows fresh).
void restoreMemberPin(ALGhostStudio& studio, const LLUUID& unit_id, bool pinned)
{
    studio.setGroupMemberPinned(unit_id, true);
    if (pinned)
    {
        return;
    }
    if (ALGhostGroupModel::Group* group = studio.groupForMember(unit_id))
    {
        for (ALGhostGroupModel::Member& member : group->mMembers)
        {
            if (member.mInstanceId == unit_id)
            {
                member.mPinned = false;
                ++group->mRevision;
            }
        }
    }
}

// Same rule as a committed endDrag: a Reflect clone (Fixed plane + Mirror
// position) must not be pulled back by the next Reflect pass.
void invalidateEditReflect(ALGhostStudio& studio, const EditContext& ctx)
{
    const ALGhostGroupModel::Group* group = studio.groupForMember(ctx.mUnitId);
    if (ctx.mWholeGroup && group)
    {
        for (const ALGhostGroupModel::Member& member : group->mMembers)
        {
            studio.invalidateReflectPlane(member.mInstanceId);
        }
    }
    else
    {
        studio.invalidateReflectPlane(ctx.mUnitId);
    }
}

bool applyEditStep(const EditContext& ctx, const UnitXform& before,
                   const UnitXform& after, bool redo, std::string& reason)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    cloneEditHistoryBegin();
    bool ok = editStepValid(studio, ctx, before, after, redo, reason);
    if (ok)
    {
        const UnitXform& target = redo ? after : before;
        ok = ctx.mWholeGroup
            ? studio.transformGroup(ctx.mUnitId, target.mFoot, target.mRotation, target.mScale)
            : studio.transformUnit(ctx.mUnitId, target.mFoot, target.mRotation, target.mScale);
        if (ok && ctx.mPinKnown)
        {
            restoreMemberPin(studio, ctx.mUnitId,
                             redo ? ctx.mPinnedAfter : ctx.mPinnedBefore);
        }
        else if (!ok)
        {
            reason = "That clone is no longer available";
        }
        if (ok)
        {
            invalidateEditReflect(studio, ctx);
        }
    }
    cloneEditHistoryEnd();
    return ok;
}

// Called from endDrag(true) AFTER the final transform was committed.
// Returns true when a step was recorded.
bool recordCommittedDrag(ALGhostStudio& studio, ALKitShell* shell,
                         const LLUUID& drag_unit_id, S32 kind,
                         const UnitXform& before,
                         bool pin_known, bool pin_before)
{
    UnitXform after;
    if (!shell || !studio.getUnitTransform(
            drag_unit_id, after.mFoot, after.mRotation, after.mScale))
    {
        return false;
    }
    if (!unitXformChanged(before, after))
    {
        return false;
    }
    EditContext ctx = captureEditContext(studio, drag_unit_id);
    std::string name;
    if (ctx.mWholeGroup)
    {
        if (const ALGhostGroupModel::Group* group = studio.groupForMember(ctx.mUnitId))
        {
            name = group->mName;
        }
        if (name.empty())
        {
            name = "group";
        }
    }
    else
    {
        name = studio.cloneDisplayName(ctx.mUnitId);
        if (!ctx.mGroupId.isNull() && pin_known)
        {
            ctx.mPinKnown = true;
            ctx.mPinnedBefore = pin_before;
            ctx.mPinnedAfter = pin_before;
            editMemberPinned(studio, ctx.mUnitId, ctx.mPinnedAfter);
        }
    }
    const char* verb = kind == 2 ? "Scale " : kind == 1 ? "Rotate " : "Move ";
    shell->history().recordCustom(shell, "clone_edit_transform", std::string(verb) + name,
        [ctx, before, after](bool redo, std::string& reason)
        {
            return applyEditStep(ctx, before, after, redo, reason);
        },
        false,
        [ctx, before, after](bool redo, std::string& reason)
        {
            return editStepValid(ALGhostStudio::instance(), ctx, before, after, redo, reason);
        });
    return true;
}

// Pre-drag snapshot: what a cancel restores and what an undo returns to.
struct DragSnapshot
{
    LLVector3d   mFoot;
    LLQuaternion mRotation;
    F32          mScale = 1.f;
    S32          mKind = 0;         // 0 move, 1 rotate, 2 scale
    bool         mPinKnown = false;
    bool         mPinned = false;
};

bool dragBegin(DragSnapshot& snap, ALGhostStudio& studio,
               const LLUUID& unit_id, S32 kind)
{
    snap = DragSnapshot();
    snap.mKind = kind;
    const ALGhostStudio::Instance* inst = backingInstanceForUnit(studio, unit_id);
    if (!inst)
    {
        return false;
    }
    if (!studio.getUnitTransform(
            unit_id, snap.mFoot, snap.mRotation, snap.mScale))
    {
        snap.mFoot = inst->mFootGlobal;
        snap.mRotation = inst->mRotation;
        snap.mScale = inst->mScale;
    }
    if (const ALGhostGroupModel::Group* group = studio.groupForMember(unit_id);
        group && group->mEditMembers && unit_id != group->mId)
    {
        for (const ALGhostGroupModel::Member& member : group->mMembers)
        {
            if (member.mInstanceId == inst->mId)
            {
                snap.mPinKnown = true;
                snap.mPinned = member.mPinned;
                break;
            }
        }
    }
    return true;
}

// Drag end. commit: record ONE step (if changed). Cancel: restore the snapshot
// and record nothing. Returns true when a step was recorded.
bool dragFinish(const DragSnapshot& snap, ALGhostStudio& studio, ALKitShell* shell,
                const LLUUID& unit_id, bool commit)
{
    UnitXform before;
    before.mFoot = snap.mFoot;
    before.mRotation = snap.mRotation;
    before.mScale = snap.mScale;
    if (commit)
    {
        return recordCommittedDrag(studio, shell, unit_id, snap.mKind,
                                   before, snap.mPinKnown, snap.mPinned);
    }
    if (!backingInstanceForUnit(studio, unit_id))
    {
        return false;
    }
    const ALGhostGroupModel::Group* group = studio.groupForMember(unit_id);
    const bool whole_group =
        group && (unit_id == group->mId || !group->mEditMembers);
    if (whole_group)
    {
        studio.transformGroup(unit_id, before.mFoot, before.mRotation, before.mScale);
    }
    else
    {
        studio.transformUnit(unit_id, before.mFoot, before.mRotation, before.mScale);
    }
    if (snap.mPinKnown)
    {
        // transformUnit() pins a member as soon as it is edited; a cancelled
        // drag must be neutral, and must keep the restored (possibly aimed) pose.
        restoreMemberPin(studio, unit_id, snap.mPinned);
    }
    return false;
}

// Wrapper-facing forms: production beginDrag/endDrag only forward their
// members to these, so the handoff itself is exercised too.
S32 dragKindForTool(bool scale_tool, bool rotate_tool)
{
    return scale_tool ? 2 : rotate_tool ? 1 : 0;
}

void dragBeginInto(ALGhostStudio& studio, const LLUUID& unit_id, S32 kind,
                   LLVector3d& foot, LLQuaternion& rotation, F32& scale,
                   S32& kind_out, bool& pin_known, bool& pinned)
{
    DragSnapshot snap;
    const bool have = dragBegin(snap, studio, unit_id, kind);
    kind_out = snap.mKind;
    pin_known = snap.mPinKnown;
    pinned = snap.mPinned;
    if (have)
    {
        foot = snap.mFoot;
        rotation = snap.mRotation;
        scale = snap.mScale;
    }
}

bool dragFinishFrom(const LLVector3d& foot, const LLQuaternion& rotation, F32 scale,
                    S32 kind, bool pin_known, bool pinned,
                    ALGhostStudio& studio, ALKitShell* shell,
                    const LLUUID& unit_id, bool commit)
{
    DragSnapshot snap;
    snap.mFoot = foot;
    snap.mRotation = rotation;
    snap.mScale = scale;
    snap.mKind = kind;
    snap.mPinKnown = pin_known;
    snap.mPinned = pinned;
    return dragFinish(snap, studio, shell, unit_id, commit);
}

// Stock Undo/Redo routing: only while clone Edit mode is active AND the stock
// selection is the proxy does Undo/Redo go to the Director shell history.
ALGhostManipProxy::EUndoRoute cloneUndoRouteImpl(bool edit_active, bool owns_selection, bool redo)
{
    if (!edit_active || !owns_selection)
    {
        return ALGhostManipProxy::ROUTE_STOCK;
    }
    return redo ? ALGhostManipProxy::ROUTE_REDO : ALGhostManipProxy::ROUTE_UNDO;
}

// Replay support against an abstract edit host (the proxy in production).
struct EditHost
{
    virtual ~EditHost() = default;
    virtual bool editing() = 0;
    virtual void releaseManip() = 0;
    virtual bool dragging() = 0;
    virtual void cancelDrag() = 0;
    virtual bool hasProxy() = 0;
    virtual void pushProxy() = 0;
    virtual void syncRevision() = 0;
};

void historyBeginImpl(EditHost& host)
{
    if (!host.editing())
    {
        return;
    }
    host.releaseManip();
    if (host.dragging())
    {
        host.cancelDrag();
    }
}

void historyEndImpl(EditHost& host)
{
    if (!host.editing() || !host.hasProxy())
    {
        return;
    }
    host.pushProxy();
    host.syncRevision();
}
// [CloneEditUndo-END]

ALKitShell* directorShell()
{
    if (LLFloater* director = LLFloaterReg::getInstance("director"))
    {
        return director->findChild<ALKitShell>("director_shell");
    }
    return nullptr;
}

void cloneEditHistoryBegin()
{
    ALToolGhostEdit::getInstance()->getManipProxy().historyBegin();
}

void cloneEditHistoryEnd()
{
    ALToolGhostEdit::getInstance()->getManipProxy().historyEnd();
}
} // anonymous namespace

// [CloneEditUndo] Replay support: a live gizmo drag would overwrite the replayed
// transform on the next tick, so cancel it first, then resync the proxy.
void ALGhostManipProxy::historyBegin()
{
    struct Host : EditHost
    {
        ALGhostManipProxy& p;
        explicit Host(ALGhostManipProxy& proxy) : p(proxy) {}
        bool editing() override { return p.mEditMode; }
        void releaseManip() override
        {
            if (LLToolComposite* comp = dynamic_cast<LLToolComposite*>(
                    LLToolMgr::getInstance()->getCurrentTool()))
            {
                comp->releaseManip();
            }
        }
        bool dragging() override { return p.mDragging; }
        void cancelDrag() override { p.endDrag(false); }
        bool hasProxy() override { return p.mProxy.notNull(); }
        void pushProxy() override { p.pushInstanceToProxy(); }
        void syncRevision() override
        {
            if (ALGhostStudio::Instance* inst = backingInstanceForUnit(
                    ALGhostStudio::instance(), p.mInstanceId))
            {
                p.mSeenRevision = inst->mTransformRevision;
            }
        }
    } host(*this);
    historyBeginImpl(host);
}

void ALGhostManipProxy::historyEnd()
{
    struct Host : EditHost
    {
        ALGhostManipProxy& p;
        explicit Host(ALGhostManipProxy& proxy) : p(proxy) {}
        bool editing() override { return p.mEditMode; }
        void releaseManip() override {}
        bool dragging() override { return p.mDragging; }
        void cancelDrag() override {}
        bool hasProxy() override { return p.mProxy.notNull(); }
        void pushProxy() override { p.pushInstanceToProxy(); }
        void syncRevision() override
        {
            if (ALGhostStudio::Instance* inst = backingInstanceForUnit(
                    ALGhostStudio::instance(), p.mInstanceId))
            {
                p.mSeenRevision = inst->mTransformRevision;
            }
        }
    } host(*this);
    historyEndImpl(host);
}

// [CloneEditUndo] Redirect stock Undo/Redo to the Director shell history while
// clone Edit mode owns the selection (see LLSelectMgr::canUndo/undo/canRedo/redo).
ALGhostManipProxy::EUndoRoute ALGhostManipProxy::undoRoute(
    bool edit_active, bool owns_selection, bool redo)
{
    return cloneUndoRouteImpl(edit_active, owns_selection, redo);
}

bool ALGhostManipProxy::historyCan(bool redo)
{
    ALKitShell* shell = directorShell();
    return shell && shell->history().stepSequence("chrome", redo) != 0;
}

bool ALGhostManipProxy::historyRun(bool redo)
{
    ALKitShell* shell = directorShell();
    if (!shell)
    {
        return false;
    }
    shell->history().finishGesture();
    if (!shell->history().stepSequence("chrome", redo))
    {
        return true;    // nothing to replay; still swallow the stock sim undo
    }
    std::string reason;
    if (!shell->history().prepareReplay("chrome", redo, reason))
    {
        shell->history().discardStep("chrome", redo);
        LLNotificationsUtil::add("SystemMessageTip", LLSD().with("MESSAGE", reason));
    }
    else if (redo)
    {
        shell->history().redo("chrome");
    }
    else
    {
        shell->history().undo("chrome");
    }
    return true;
}

ALGhostManipProxy::ALGhostManipProxy() = default;

ALGhostManipProxy::~ALGhostManipProxy()
{
    teardown();
}

void ALGhostManipProxy::begin()
{
    mEditMode = true;
    mEmptyGroundClicked = false;    // [GhostEditFix]
    mSeenRevision = 0;      // force the first-tick push
    mSeenGroupRevision = 0;
}

bool ALGhostManipProxy::owns(const LLViewerObject* object) const
{
    return object && mProxy.notNull() && object == mProxy.get();
}

// -----------------------------------------------------------------------------
bool ALGhostManipProxy::createProxy(LLViewerRegion* region)
{
    if (!region)
    {
        return false;
    }

    LLViewerObject* object = gObjectList.createObjectViewer(LL_PCODE_VOLUME, region);
    LLVOVolume* volume = dynamic_cast<LLVOVolume*>(object);
    if (!volume)
    {
        if (object) { object->markDead(); }
        return false;
    }

    // Client-only + typed BEFORE anything can enqueue sim traffic (mirrors the
    // Local Mesh recipe, lllocalmesh.cpp:1555). The GHOST_MANIP_PROXY kind keeps
    // llselectmgr from routing it into LLLocalMeshMgr on delete/duplicate/attach.
    volume->mbCanSelect = true;
    volume->mIsLocalOnly = true;
    volume->mLocalObjectKind = LLViewerObject::LOCAL_OBJECT_GHOST_MANIP_PROXY;
    volume->setFlagsWithoutUpdate(
        FLAGS_OBJECT_YOU_OWNER | FLAGS_OBJECT_MODIFY | FLAGS_OBJECT_MOVE |
        FLAGS_OBJECT_COPY | FLAGS_OBJECT_TRANSFER, true);

    gPipeline.createObject(volume);
    volume->setLOD(LLVolumeLODGroup::NUM_LODS - 1);

    // Known-valid unit-box params (same sequence as the /prim box harness). Do
    // NOT also bake the dimensions into the params -- setScale below carries them,
    // and doing both would double-scale the manipulator bounds.
    LLVolumeParams params;
    params.setType(LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_LINE);
    params.setBeginAndEndS(0.f, 1.f);
    params.setBeginAndEndT(0.f, 1.f);
    params.setRatio(1.f, 1.f);
    params.setShear(0.f, 0.f);
    if (!volume->setVolume(params, LLVolumeLODGroup::NUM_LODS - 1, true))
    {
        volume->markDead();
        return false;
    }

    volume->setScale(LLVector3(PROXY_WIDTH, PROXY_DEPTH, PROXY_HEIGHT), false);

    mProxy = volume;
    mRegion = region;

    pushInstanceToProxy();

    // Invisible but with valid extents/picking (FORCE_INVISIBLE only suppresses
    // render submission -- it does NOT disable RENDER_TYPE_VOLUME, which would
    // break the manipulators' line-segment picking). Reasserted every tick.
    if (volume->mDrawable.notNull())
    {
        volume->mDrawable->setState(LLDrawable::FORCE_INVISIBLE);
    }
    return true;
}

void ALGhostManipProxy::destroyProxy()
{
    LLPointer<LLVOVolume> dying = mProxy;   // hold a strong ref through markDead
    mProxy = nullptr;
    mRegion = nullptr;
    if (dying.notNull() && !dying->isDead())
    {
        dying->markDead();
    }
}

void ALGhostManipProxy::selectProxy()
{
    if (mProxy.isNull() || mProxy->isDead())
    {
        return;
    }
    LLSelectMgr* select_mgr = LLSelectMgr::getInstance();
    select_mgr->deselectAll();
    // synthesizeLocalPreviewNode() (gated on isLocalOnly) makes this node valid,
    // agent-owned and editable, so the stock tools accept it -- no extra work.
    select_mgr->selectObjectOnly(mProxy.get(), SELECT_ALL_TES);

    LLToolMgr* tool_mgr = LLToolMgr::getInstance();
    if (!mSavedToolset)
    {
        mSavedToolset = tool_mgr->getCurrentToolset();
        mSavedTool = mSavedToolset ? mSavedToolset->getSelectedTool()
                                   : tool_mgr->getCurrentTool();
    }
    // Switching toolsets deselects the transient ghost tool; ALToolGhostEdit must
    // NOT tear us down for that (edit-mode is a separate explicit state).
    tool_mgr->setCurrentToolset(gBasicToolset);
    gBasicToolset->selectTool(LLToolCompTranslate::getInstance());
}

void ALGhostManipProxy::enterPickerState()
{
    LLToolMgr* tool_mgr = LLToolMgr::getInstance();

    // Idempotent: if we already hold no proxy and the in-world picker is current,
    // we ARE in State B -- do nothing. Otherwise this would deselect / re-install /
    // re-fire ALToolGhostEdit::handleSelect() every frame while nothing is picked.
    if (mProxy.isNull() && tool_mgr->getCurrentTool() == ALToolGhostEdit::getInstance())
    {
        return;
    }

    // Cancel any in-progress gizmo drag and release capture. Capture lives on the
    // stock composite's INTERNAL manip, so releaseManip() -- not setMouseCapture()
    // on the composite (which wouldn't release it).
    if (LLToolComposite* comp = dynamic_cast<LLToolComposite*>(tool_mgr->getCurrentTool()))
    {
        comp->releaseManip();
    }
    if (mDragging)
    {
        endDrag(false);
    }

    // Clear the (now stale) proxy selection and destroy the proxy BEFORE handing
    // the tool back, so the stock manipulator never holds a dead object.
    LLSelectMgr::getInstance()->deselectAll();
    destroyProxy();
    mInstanceId.setNull();
    mSeenRevision = 0;
    mSeenGroupRevision = 0;

    // Hand the in-world ghost picker back so the user can select another ghost.
    // The departure detector treats ALToolGhostEdit as an editing tool, so this
    // does NOT read as leaving edit mode. Mirrors the panel's edit-mode entry
    // (alpanelghoststudio.cpp: setTransientTool(ALToolGhostEdit)).
    tool_mgr->setTransientTool(ALToolGhostEdit::getInstance());
}

void ALGhostManipProxy::pushInstanceToProxy()
{
    if (mProxy.isNull() || mProxy->isDead())
    {
        return;
    }
    ALGhostStudio& studio = ALGhostStudio::instance();
    const ALGhostStudio::Instance* inst =
        backingInstanceForUnit(studio, mInstanceId);
    if (!inst)
    {
        return;
    }
    LLVector3d foot;
    LLQuaternion rotation;
    F32 unit_scale = 1.f;
    if (!studio.getUnitTransform(
            mInstanceId, foot, rotation, unit_scale))
    {
        return;
    }

    const ALGhostGroupModel::Group* group =
        studio.groupForMember(mInstanceId);
    const bool whole_group =
        group && (mInstanceId == group->mId || !group->mEditMembers);
    LLVector3 local_center(0.f, 0.f, 0.5f * PROXY_HEIGHT);
    LLVector3 local_dimensions(PROXY_WIDTH, PROXY_DEPTH, PROXY_HEIGHT);

    // Individual/member proxy bounds keep the avatar scale limits. A collapsed
    // group uses its authoring world scale directly: each member's own resolved
    // scale is constrained by transformGroup(), but the group-world factor is
    // not itself an avatar scale and must not be clamped to
    // [GHOST_SCALE_MIN, GHOST_SCALE_MAX].
    const F32 s = whole_group
        ? unit_scale : llclamp(unit_scale, GHOST_SCALE_MIN, GHOST_SCALE_MAX);
    if (!llfinite(s) || s <= 0.f)
    {
        return;
    }
    if (whole_group &&
        !groupLocalBounds(*group, PROXY_WIDTH, PROXY_DEPTH, PROXY_HEIGHT,
                          foot, rotation, s, local_center, local_dimensions))
        return;
    mProxyUsesBodyPivot = false;
    if (!whole_group && inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE)
    {
        LLVector3 center, size;
        if (LLGhostAvatar* ghost = studio.resolveEntityClone(inst->mId);
            ghost && ghost->getVisualBounds(center, size))
        {
            // Inverse the authored frame once. The extents already contain the
            // outer scale. Enclosing all corners preserves the editor's axes.
            LLBBox local_box(gAgent.getPosAgentFromGlobal(foot), rotation,
                            LLVector3::zero, LLVector3::zero);
            for (S32 x = 0; x < 2; ++x)
                for (S32 y = 0; y < 2; ++y)
                    for (S32 z = 0; z < 2; ++z)
                        local_box.addPointAgent(center + LLVector3(
                            (x ? .5f : -.5f) * size[VX],
                            (y ? .5f : -.5f) * size[VY],
                            (z ? .5f : -.5f) * size[VZ]));
            local_center = local_box.getCenterLocal() / s;
            local_dimensions = local_box.getExtentLocal() / s;
            mProxyUsesBodyPivot = true;
            // Foot placement lifts the skeleton root in WORLD Z, not along
            // the authored rotation. Keep that lift unrotated during pitch/roll.
            LLVector3 rendered_root = ghost->getRootJoint()->getWorldPosition() *
                ghost->getClientOuterTransformMatrix();
            // [StayInPlace] The Stay in place shift turns WITH the body, unlike
            // that lift: keep it out, so a rotate drag stores the authored foot.
            if (LLGhostAvatar::hasStayInPlaceAvatars())
                rendered_root -= ghost->getStayInPlaceRenderedOffset();
            mProxyRootOffset = rendered_root - gAgent.getPosAgentFromGlobal(foot);
        }
    }
    mProxyLocalCenter = local_center;
    mProxyLocalDimensions = local_dimensions;
    const LLVector3 dimensions = local_dimensions * s;
    const LLVector3d position = proxyCenterFromPivot(foot, rotation, local_center, s);
    if (mProxy->getScale() != dimensions || mProxy->getRotation() != rotation ||
        mProxy->getPositionGlobal() != position)
    {
        mProxy->setScale(dimensions, false);
        mProxy->setRotation(rotation, false);
        mProxy->setPositionGlobal(position, false);
        // Pose changes carry no authoring revision, but stock handles still
        // need their new bbox. Do not touch an unrelated selection at creation.
        if (owns(LLSelectMgr::getInstance()->getSelection()->getFirstObject()))
            LLSelectMgr::getInstance()->updateSelectionCenter();
    }
    mSeenGroupRevision = group ? group->mRevision : 0;
}

void ALGhostManipProxy::pullProxyToInstance()
{
    if (mProxy.isNull() || mProxy->isDead())
    {
        return;
    }
    ALGhostStudio& studio = ALGhostStudio::instance();
    ALGhostStudio::Instance* inst =
        backingInstanceForUnit(studio, mInstanceId);
    if (!inst)
    {
        return;
    }
    // Ungrouped overlays write fields DIRECTLY (no mTransformRevision bump).
    // Entity clones and group members go through transformUnit/transformGroup,
    // which bump the revision; that is safe because tick() never pushes to the
    // proxy while mDragging, and endDrag re-syncs mSeenRevision.
    if (inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE)
    {
        inst->mChaosHasBase = false;
    }
    ALGhostGroupModel::Group* group =
        studio.groupForMember(mInstanceId);
    const bool whole_group =
        group && (mInstanceId == group->mId || !group->mEditMembers);

    // STRETCH drag: derive one uniform factor from whichever proxy axis changed
    // most relative to its LOCAL base dimension. Individuals/member edits keep
    // their drag-start feet planted. Collapsed groups likewise keep their
    // authored pivot fixed, matching the numeric scale control. This reads only
    // (never writes the proxy mid-drag), so it doesn't fight LLManipScale.
    if (LLToolMgr::getInstance()->getCurrentTool() == LLToolCompScale::getInstance())
    {
        const F32 requested_scale = uniformScaleFromProxy(
            mProxy->getScale(), mDragLocalDimensions, mDragStartScale);
        if (whole_group)
        {
            studio.transformGroup(
                mInstanceId, mDragStartFoot,
                mDragStartRotation, requested_scale);
            return;
        }
        const F32 s = llclamp(requested_scale, GHOST_SCALE_MIN, GHOST_SCALE_MAX);
        // [CloneStretch] Entity clones scale on their outer transform; transformUnit
        // is the path that applies it (setEntityScale + applyEntityTransform), the
        // same one grouped members and the numeric scale control already use.
        if (group || inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE)
        {
            studio.transformUnit(
                mInstanceId, mDragStartFoot, mDragStartRotation, s);
            return;
        }
        inst->mScale      = s;
        inst->mFootGlobal = mDragStartFoot;
        inst->mRotation   = mDragStartRotation;
        return;
    }

    // TRANSLATE / ROTATE drag: position + rotation; scale unchanged.
    const LLQuaternion rotation = mProxy->getRotation();
    if (group)
    {
        if (whole_group)
        {
            LLVector3d pivot = mDragStartFoot;
            if (LLToolMgr::getInstance()->getCurrentTool() !=
                LLToolCompRotate::getInstance())
            {
                // Translation follows the proxy centre; rotation uses the
                // authored group pivot, exactly like the numeric yaw control.
                pivot = pivotFromProxyCenter(
                    mProxy->getPositionGlobal(), rotation,
                    mDragLocalCenter, mDragStartScale);
            }
            studio.transformGroup(
                mInstanceId, pivot, rotation, mDragStartScale);
        }
        else
        {
            LLVector3d foot = mDragStartFoot;
            if (mProxyUsesBodyPivot || LLToolMgr::getInstance()->getCurrentTool() !=
                LLToolCompRotate::getInstance())
            {
                foot = pivotFromProxyCenter(
                    mProxy->getPositionGlobal(), rotation,
                    mDragLocalCenter, mDragStartScale);
                if (mProxyUsesBodyPivot && LLToolMgr::getInstance()->getCurrentTool() ==
                    LLToolCompRotate::getInstance())
                    foot += LLVector3d((mDragRootOffset * ~mDragStartRotation) * rotation -
                                       mDragRootOffset);
            }
            studio.transformUnit(
                mInstanceId, foot, rotation, inst->mScale);
        }
        return;
    }
    inst->mRotation = rotation;
    if (!mProxyUsesBodyPivot &&
        LLToolMgr::getInstance()->getCurrentTool() == LLToolCompRotate::getInstance())
    {
        // ROTATE pivots around the planted FOOT: keep the drag-start foot and take
        // only the new orientation. Stock rotate spins about the box CENTRE, which
        // would swing the foot out for pitch/roll; endDrag() re-normalises the
        // proxy centre back over this foot so the gizmo re-seats on the next tick.
        inst->mFootGlobal = mDragStartFoot;
    }
    else
    {
        // TRANSLATE keeps the exact centre delta. Entity ROTATE keeps the
        // rendered body centre fixed, with the world-Z root lift unchanged.
        inst->mFootGlobal = pivotFromProxyCenter(
            mProxy->getPositionGlobal(), rotation,
            mDragLocalCenter,
            llclamp(inst->mScale, GHOST_SCALE_MIN, GHOST_SCALE_MAX));
        if (mProxyUsesBodyPivot && LLToolMgr::getInstance()->getCurrentTool() ==
            LLToolCompRotate::getInstance())
            inst->mFootGlobal += LLVector3d(
                (mDragRootOffset * ~mDragStartRotation) * rotation - mDragRootOffset);
    }
    if (inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE)
    {
        studio.applyEntityTransform(inst->mId);
    }
}

// -----------------------------------------------------------------------------
bool ALGhostManipProxy::manipHasMouseCapture() const
{
    // Mouse capture during a gizmo drag lives on the composite's INTERNAL LLManip,
    // not on the composite itself (which getCurrentTool() returns) -- so ask the
    // composite's isManipulating(), NOT hasMouseCapture() (which would be
    // permanently false and the drag would never reach the ghost).
    LLTool* tool = LLToolMgr::getInstance()->getCurrentTool();
    if (!tool || tool == ALToolGhostEdit::getInstance())
    {
        return false;
    }
    const LLToolComposite* comp = dynamic_cast<const LLToolComposite*>(tool);
    return comp && comp->isManipulating();
}

void ALGhostManipProxy::beginDrag()
{
    mDragging = true;
    mDragLocalCenter = mProxyLocalCenter;
    mDragLocalDimensions = mProxyLocalDimensions;
    mDragRootOffset = mProxyRootOffset;
    const LLTool* tool = LLToolMgr::getInstance()->getCurrentTool();
    // Snapshot the pre-drag transform so a cancelled drag can restore it.
    dragBeginInto(ALGhostStudio::instance(), mInstanceId,
                  dragKindForTool(tool == LLToolCompScale::getInstance(),
                                  tool == LLToolCompRotate::getInstance()),
                  mDragStartFoot, mDragStartRotation, mDragStartScale,
                  mDragKind, mDragStartMemberPinKnown, mDragStartMemberPinned);
}

void ALGhostManipProxy::endDrag(bool commit)
{
    if (!mDragging)
    {
        return;
    }
    mDragging = false;
    if (commit)
    {
        pullProxyToInstance();      // capture the final transform
        ALGhostStudio& studio = ALGhostStudio::instance();
        if (ALGhostStudio::Instance* inst =
                backingInstanceForUnit(studio, mInstanceId))
        {
            // The live drag deliberately writes an ungrouped overlay directly so
            // a revision push cannot fight the stock manipulator. Commit the final
            // absolute values through the studio once the drag is over so every
            // backing kind gets the normal revision-bearing transform path.
            const ALGhostGroupModel::Group* group =
                studio.groupForMember(mInstanceId);
            const bool whole_group =
                group && (mInstanceId == group->mId ||
                          !group->mEditMembers);
            if (whole_group)
            {
                LLVector3d foot;
                LLQuaternion rotation;
                F32 scale = 1.f;
                if (studio.getUnitTransform(
                        mInstanceId, foot, rotation, scale))
                {
                    studio.transformGroup(
                        mInstanceId, foot, rotation, scale);
                }
            }
            else
            {
                studio.transformUnit(
                    mInstanceId, inst->mFootGlobal,
                    inst->mRotation, inst->mScale);
            }
            mSeenRevision = inst->mTransformRevision;
            // [CloneEditUndo] One committed drag = one history step; unchanged
            // drags record nothing (cancelled drags never reach this branch).
            dragFinishFrom(mDragStartFoot, mDragStartRotation, mDragStartScale,
                           mDragKind, mDragStartMemberPinKnown,
                           mDragStartMemberPinned, studio, directorShell(),
                           mInstanceId, true);
        }
        const auto& studio_reflect = ALGhostStudio::instance();
        const auto* reflect_group = studio_reflect.groupForMember(mInstanceId);
        if (reflect_group && (mInstanceId == reflect_group->mId || !reflect_group->mEditMembers))
            for (const auto& member : reflect_group->mMembers)
                ALGhostStudio::instance().invalidateReflectPlane(member.mInstanceId);
        else ALGhostStudio::instance().invalidateReflectPlane(mInstanceId);
        pushInstanceToProxy();      // re-normalise the proxy: a planted UNIFORM box
                                    // matching the ghost (undoes a Stretch drag's
                                    // per-axis / center-pivot wander on the proxy)
    }
    else
    {
        // Cancel: restore the pre-drag transform + scale + member pin (the
        // revision bump makes the next tick push it back onto the proxy).
        dragFinishFrom(mDragStartFoot, mDragStartRotation, mDragStartScale,
                       mDragKind, mDragStartMemberPinKnown,
                       mDragStartMemberPinned, ALGhostStudio::instance(),
                       nullptr, mInstanceId, false);
    }
    mDragStartMemberPinKnown = false;
}

// -----------------------------------------------------------------------------
bool ALGhostManipProxy::isDraggingInstance(const LLUUID& id) const
{
    // Mouse capture is sampled here too: object-list post-pass precedes tick().
    if (!mEditMode || !(mDragging || manipHasMouseCapture())) return false;
    const ALGhostStudio& studio = ALGhostStudio::instance();
    if (id == mInstanceId) return true;
    const auto* group = studio.groupForMember(mInstanceId);
    if (!group || (group->mEditMembers && mInstanceId != group->mId)) return false;
    for (const auto& member : group->mMembers) if (member.mInstanceId == id) return true;
    return false;
}

void ALGhostManipProxy::tick()
{
    if (!mEditMode)
    {
        teardown();
        return;
    }
    if (!ALGhostStudio::instance().getShowAll())
    {
        // Master hide means there is no visible/pickable edit target. Tear the
        // proxy and stock gizmo down immediately, including when visibility was
        // changed by the other panel or an external caller.
        ALToolGhostEdit::getInstance()->stopEditMode(
            /*restore_toolset*/ true);
        return;
    }

    // Handoff exit: once the proxy is selected (mSavedToolset set), the current
    // tool is one of the ghost-edit tools -- the in-world picker (State B) or a
    // stock manip gizmo (State A). Switching among Translate/Rotate/Scale keeps us
    // editing; the picker keeps us editing; ANY OTHER tool (Camera, Grab, Create,
    // Inspect, Land, ...) is a genuine departure -> exit ghost edit. Check the TOOL
    // identity, NOT merely the toolset: the stock manip tools AND several unrelated
    // tools all live in gBasicToolset, so a toolset compare would miss a departure
    // to e.g. the Camera tool and leave the invisible proxy selected under it.
    if (mSavedToolset)
    {
        const LLTool* cur_tool = LLToolMgr::getInstance()->getCurrentTool();
        const bool on_edit_tool =
            cur_tool == ALToolGhostEdit::getInstance() ||
            cur_tool == LLToolCompTranslate::getInstance() ||
            cur_tool == LLToolCompRotate::getInstance() ||
            cur_tool == LLToolCompScale::getInstance();
        if (!on_edit_tool)
        {
            if (mDragging)
            {
                endDrag(false);
            }
            // Departure: do NOT restore the toolset -- the user chose the new one.
            ALToolGhostEdit::getInstance()->stopEditMode(/*restore_toolset*/ false);
            return;
        }
    }

    ALGhostStudio& studio = ALGhostStudio::instance();

    // [GhostEditFix] Consume the empty-ground provenance flag set by the stock
    // pick path (ALToolGhostEdit::handleStockPick, from the Translate/Rotate/Scale
    // pickCallbacks): the user clicked nothing at all while a ghost was being
    // edited -> deselect it and end edit mode (the panel's draw mirror unchecks
    // Edit). Done here, on the idle tick, never from inside the pick callback.
    if (mEmptyGroundClicked)
    {
        mEmptyGroundClicked = false;
        if (mDragging)
        {
            endDrag(false);
        }
        studio.setSelected(LLUUID::null);
        ALToolGhostEdit::getInstance()->stopEditMode(/*restore_toolset*/ true);
        return;
    }

    const LLUUID selected = studio.getSelected();

    // Follow the shared selection (panel list OR in-world pick both setSelected).
    // A change drops the old proxy so the block below rebuilds for the new target.
    // [GhostEditFix] "Live ghost deselected" = the shared selection went from a
    // ghost that STILL EXISTS to null (list deselect). A deleted ghost, initial
    // entry and picker arming all have prev null or a dead prev -> keep the
    // armed picker as before.
    const LLUUID prev_instance = mInstanceId;
    if (selected != mInstanceId)
    {
        if (selected.isNull() && prev_instance.notNull() &&
            backingInstanceForUnit(studio, prev_instance))
        {
            ALToolGhostEdit::getInstance()->stopEditMode(/*restore_toolset*/ true);
            return;
        }
        mInstanceId = selected;
        destroyProxy();
        mSeenRevision = 0;
        mSeenGroupRevision = 0;
    }

    // Region temporarily unavailable (e.g. a region cross in flight): an
    // AVAILABILITY condition, not a lost selection. Drop the stale proxy and retry
    // next tick -- do NOT clear the still-valid Ghost Studio selection or drop to
    // the picker on a transient region gap.
    LLViewerRegion* region = gAgent.getRegion();
    if (!region)
    {
        destroyProxy();
        return;
    }

    // No resolvable instance (deselected, deleted, or removed under us): stay ARMED
    // in edit mode but hand the in-world picker back (State B) so the user can pick
    // another ghost. Only an explicit stop (toggle off / Esc / departure) tears
    // edit mode down.
    ALGhostStudio::Instance* inst = selected.notNull()
        ? backingInstanceForUnit(studio, selected) : nullptr;
    if (!inst)
    {
        enterPickerState();
        return;
    }

    // (Re)create for a fresh/changed region -- mFootGlobal is authoritative, so a
    // region cross recreates the proxy rather than tearing edit-mode down.
    if (mProxy.isNull() || mProxy->isDead() || mProxy->getRegion() != region)
    {
        destroyProxy();
        if (!createProxy(region))
        {
            return;     // retry next tick
        }
        selectProxy();
        mSeenRevision = inst->mTransformRevision;
    }

    const bool dragging = manipHasMouseCapture();
    if (dragging && !mDragging)
    {
        beginDrag();
    }

    if (dragging)
    {
        pullProxyToInstance();
    }
    else
    {
        if (mDragging)
        {
            endDrag(true);
        }
        const ALGhostGroupModel::Group* group =
            studio.groupForMember(inst->mId);
        const U64 group_revision = group ? group->mRevision : 0;
        const bool transform_changed = studio.reflectOwnsPosition(*inst) ||
            inst->mTransformRevision != mSeenRevision || group_revision != mSeenGroupRevision;
        // Entity poses can change independently of authoring revisions. A group
        // can contain entities even when its representative is an overlay.
        bool pose_bounds = inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE;
        if (group && !pose_bounds)
            for (const auto& member : group->mMembers)
                if (const auto* member_inst = studio.getInstance(member.mInstanceId);
                    member_inst && member_inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE)
                    { pose_bounds = true; break; }
        if (transform_changed || pose_bounds)
        {
            pushInstanceToProxy();
            if (transform_changed && inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE)
            {
                studio.applyEntityTransform(inst->mId);
            }
            mSeenRevision = inst->mTransformRevision;
        }
    }

    if (mProxy.notNull() && mProxy->mDrawable.notNull())
    {
        mProxy->mDrawable->setState(LLDrawable::FORCE_INVISIBLE);
    }
}

void ALGhostManipProxy::teardown(bool restore_toolset)
{
    if (!mEditMode && mProxy.isNull() && !mDragging && !mSavedToolset)
    {
        return;     // already clean (idempotent)
    }
    mEditMode = false;
    mEmptyGroundClicked = false;    // [GhostEditFix]

    // Release any in-progress gizmo drag. Capture lives on the composite's
    // internal manip, so releaseManip() -- NOT setMouseCapture() on the composite
    // (which wouldn't release it, leaving input/cursor state stuck).
    LLTool* current = LLToolMgr::getInstance()->getCurrentTool();
    if (LLToolComposite* comp = dynamic_cast<LLToolComposite*>(current))
    {
        comp->releaseManip();
    }
    else if (current && current->hasMouseCapture())
    {
        current->setMouseCapture(false);
    }
    endDrag(false);

    LLSelectMgr::getInstance()->deselectAll();
    destroyProxy();     // clears mProxy BEFORE markDead

    LLToolset* saved_set  = mSavedToolset;
    LLTool*    saved_tool = mSavedTool;
    mSavedToolset = nullptr;
    mSavedTool    = nullptr;
    mInstanceId.setNull();
    mSeenRevision = 0;
    mSeenGroupRevision = 0;

    // Restore the pre-edit toolset only for an explicit exit. On a departure
    // (the user already switched away from gBasicToolset) restoring would
    // override their choice -- especially since saved_set is often gBasicToolset.
    if (restore_toolset && saved_set)
    {
        LLToolMgr::getInstance()->setCurrentToolset(saved_set);
        if (saved_tool)
        {
            saved_set->selectTool(saved_tool);
        }
    }
}
