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
// The proxy volume is a UNIT cube; object scale gives it world dimensions. Its
// local CENTER sits height/2 above its local foot, so the ghost-foot <-> proxy-
// center conversion rotates that offset by the instance orientation (pitch/roll
// safe). Height is the canonical manip bound, NOT the avatar's pelvisToFoot --
// Ghost Studio rendering pivots the real geometry at its own live/frozen foot.
LLVector3d proxyCenterFromFoot(const LLVector3d& foot_global,
                               const LLQuaternion& rotation, F32 height)
{
    const LLVector3 world_offset = LLVector3(0.f, 0.f, 0.5f * height) * rotation;
    return foot_global + LLVector3d(world_offset);
}

LLVector3d footFromProxyCenter(const LLVector3d& center_global,
                               const LLQuaternion& rotation, F32 height)
{
    const LLVector3 world_offset = LLVector3(0.f, 0.f, 0.5f * height) * rotation;
    return center_global - LLVector3d(world_offset);
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
    const ALGhostStudio::Instance* inst = ALGhostStudio::instance().getInstance(mInstanceId);
    if (!inst)
    {
        return;
    }
    // The proxy box tracks the ghost's UNIFORM scale so the gizmo and selection
    // bounds always MATCH the rendered ghost -- a size mismatch (e.g. after a
    // panel scale change) buries the manip handles inside the body.
    const F32 s = llclamp(inst->mScale, 0.05f, 10.f);
    mProxy->setScale(LLVector3(PROXY_WIDTH * s, PROXY_DEPTH * s, PROXY_HEIGHT * s), false);
    mProxy->setRotation(inst->mRotation, false);
    mProxy->setPositionGlobal(
        proxyCenterFromFoot(inst->mFootGlobal, inst->mRotation, PROXY_HEIGHT * s), false);
}

void ALGhostManipProxy::pullProxyToInstance()
{
    if (mProxy.isNull() || mProxy->isDead())
    {
        return;
    }
    ALGhostStudio::Instance* inst = ALGhostStudio::instance().getInstance(mInstanceId);
    if (!inst)
    {
        return;
    }
    // Write fields DIRECTLY (no mTransformRevision bump) -- a bump would make the
    // next non-drag tick push straight back to the proxy, fighting the drag.

    // STRETCH drag: derive a UNIFORM ghost scale from the proxy box height, and
    // KEEP the drag-start foot/rotation so the feet stay planted. Stock scale is
    // per-axis + center-pivot; we deliberately do NOT follow that wander here --
    // endDrag() re-normalises the proxy back to a planted uniform box. This reads
    // only (never writes the proxy mid-drag), so it doesn't fight LLManipScale.
    if (LLToolMgr::getInstance()->getCurrentTool() == LLToolCompScale::getInstance())
    {
        const F32 s = llclamp(mProxy->getScale().mV[VZ] / PROXY_HEIGHT, 0.05f, 10.f);
        inst->mScale      = s;
        inst->mFootGlobal = mDragStartFoot;
        inst->mRotation   = mDragStartRotation;
        return;
    }

    // TRANSLATE / ROTATE drag: position + rotation; scale unchanged.
    const LLQuaternion rotation = mProxy->getRotation();
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
        const F32 h = PROXY_HEIGHT * llclamp(inst->mScale, 0.05f, 10.f);
        inst->mFootGlobal = footFromProxyCenter(mProxy->getPositionGlobal(), rotation, h);
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
    // Snapshot the pre-drag transform so a cancelled drag can restore it.
    if (const ALGhostStudio::Instance* inst = ALGhostStudio::instance().getInstance(mInstanceId))
    {
        mDragStartFoot     = inst->mFootGlobal;
        mDragStartRotation = inst->mRotation;
        mDragStartScale    = inst->mScale;
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
        pushInstanceToProxy();      // re-normalise the proxy: a planted UNIFORM box
                                    // matching the ghost (undoes a Stretch drag's
                                    // per-axis / center-pivot wander on the proxy)
    }
    else if (ALGhostStudio::Instance* inst = ALGhostStudio::instance().getInstance(mInstanceId))
    {
        // Cancel: restore the pre-drag transform + scale (setTransform bumps the
        // revision, so the next tick pushes it all back onto the proxy).
        inst->mScale = mDragStartScale;
        inst->setTransform(mDragStartFoot, mDragStartRotation);
    }
}

// -----------------------------------------------------------------------------
void ALGhostManipProxy::tick()
{
    if (!mEditMode)
    {
        teardown();
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
    ALGhostStudio::Instance* inst = selected.notNull() ? studio.getInstance(selected) : nullptr;
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
        if (inst->mTransformRevision != mSeenRevision)
        {
            pushInstanceToProxy();
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
