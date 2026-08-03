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

#include "llagent.h"
#include "lldrawable.h"
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
                      LLVector3& center, LLVector3& dimensions)
{
    bool have_corner = false;
    LLVector3 minimum;
    LLVector3 maximum;
    for (const ALGhostGroupModel::Member& member : group.mMembers)
    {
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
                    corner *= member.mLocal.mScale;
                    corner *= member.mLocal.mRotation;
                    corner += local_foot;
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
} // anonymous namespace

ALGhostManipProxy::ALGhostManipProxy() = default;

ALGhostManipProxy::~ALGhostManipProxy()
{
    teardown();
}

void ALGhostManipProxy::begin()
{
    mEditMode = true;
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
    if (whole_group &&
        !groupLocalBounds(*group, PROXY_WIDTH, PROXY_DEPTH, PROXY_HEIGHT,
                          local_center, local_dimensions))
    {
        return;
    }

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
    mProxyLocalCenter = local_center;
    mProxyLocalDimensions = local_dimensions;
    mProxy->setScale(local_dimensions * s, false);
    mProxy->setRotation(rotation, false);
    mProxy->setPositionGlobal(
        proxyCenterFromPivot(foot, rotation, local_center, s), false);
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
    // Write fields DIRECTLY (no mTransformRevision bump) -- a bump would make the
    // next non-drag tick push straight back to the proxy, fighting the drag.
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
        if (inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE && !group)
        {
            return; // entity scale remains exclusively on the outer transform
        }
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
        if (group)
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
            if (LLToolMgr::getInstance()->getCurrentTool() !=
                LLToolCompRotate::getInstance())
            {
                foot = pivotFromProxyCenter(
                    mProxy->getPositionGlobal(), rotation,
                    mDragLocalCenter, mDragStartScale);
            }
            studio.transformUnit(
                mInstanceId, foot, rotation, inst->mScale);
        }
        return;
    }
    inst->mRotation = rotation;
    if (LLToolMgr::getInstance()->getCurrentTool() == LLToolCompRotate::getInstance())
    {
        // ROTATE pivots around the planted FOOT: keep the drag-start foot and take
        // only the new orientation. Stock rotate spins about the box CENTRE, which
        // would swing the foot out for pitch/roll; endDrag() re-normalises the
        // proxy centre back over this foot so the gizmo re-seats on the next tick.
        inst->mFootGlobal = mDragStartFoot;
    }
    else
    {
        // TRANSLATE: the foot follows the box centre (the center<->foot conversion
        // uses the SCALED box height).
        inst->mFootGlobal = pivotFromProxyCenter(
            mProxy->getPositionGlobal(), rotation,
            mDragLocalCenter,
            llclamp(inst->mScale, GHOST_SCALE_MIN, GHOST_SCALE_MAX));
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
    mDragStartMemberPinKnown = false;
    mDragStartMemberPinned = false;
    // Snapshot the pre-drag transform so a cancelled drag can restore it.
    ALGhostStudio& studio = ALGhostStudio::instance();
    if (const ALGhostStudio::Instance* inst =
            backingInstanceForUnit(studio, mInstanceId))
    {
        if (!studio.getUnitTransform(
                mInstanceId, mDragStartFoot,
                mDragStartRotation, mDragStartScale))
        {
            mDragStartFoot = inst->mFootGlobal;
            mDragStartRotation = inst->mRotation;
            mDragStartScale = inst->mScale;
        }
        if (const ALGhostGroupModel::Group* group =
                studio.groupForMember(mInstanceId);
            group && group->mEditMembers &&
            mInstanceId != group->mId)
        {
            for (const ALGhostGroupModel::Member& member : group->mMembers)
            {
                if (member.mInstanceId == inst->mId)
                {
                    mDragStartMemberPinKnown = true;
                    mDragStartMemberPinned = member.mPinned;
                    break;
                }
            }
        }
    }
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
        }
        pushInstanceToProxy();      // re-normalise the proxy: a planted UNIFORM box
                                    // matching the ghost (undoes a Stretch drag's
                                    // per-axis / center-pivot wander on the proxy)
    }
    else
    {
        ALGhostStudio& studio = ALGhostStudio::instance();
        ALGhostStudio::Instance* inst =
            backingInstanceForUnit(studio, mInstanceId);
        if (!inst)
        {
            mDragStartMemberPinKnown = false;
            return;
        }
        // Cancel: restore the pre-drag transform + scale (setTransform bumps the
        // revision, so the next tick pushes it all back onto the proxy).
        const ALGhostGroupModel::Group* group =
            studio.groupForMember(mInstanceId);
        const bool whole_group =
            group && (mInstanceId == group->mId ||
                      !group->mEditMembers);
        if (whole_group)
        {
            studio.transformGroup(
                mInstanceId, mDragStartFoot,
                mDragStartRotation, mDragStartScale);
        }
        else
        {
            studio.transformUnit(
                mInstanceId, mDragStartFoot,
                mDragStartRotation, mDragStartScale);
        }
        if (mDragStartMemberPinKnown)
        {
            // transformUnit() deliberately pins a member as soon as it is
            // edited. A cancelled drag must be observationally neutral, so
            // restore the pin bit that existed before the first live update.
            studio.setGroupMemberPinned(
                mInstanceId, mDragStartMemberPinned);
        }
    }
    mDragStartMemberPinKnown = false;
}

// -----------------------------------------------------------------------------
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
    const LLUUID selected = studio.getSelected();

    // Follow the shared selection (panel list OR in-world pick both setSelected).
    // A change drops the old proxy so the block below rebuilds for the new target.
    if (selected != mInstanceId)
    {
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
        if (inst->mTransformRevision != mSeenRevision ||
            group_revision != mSeenGroupRevision)
        {
            pushInstanceToProxy();
            if (inst->mKind == ALGhostStudio::BACKING_ENTITY_CLONE)
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
