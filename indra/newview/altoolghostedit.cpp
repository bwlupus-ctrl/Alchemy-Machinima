/**
 * @file altoolghostedit.cpp
 * @brief Persistent Ghost Studio edit tool -- see altoolghostedit.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "altoolghostedit.h"

#include "indra_constants.h"        // KEY_DELETE / KEY_BACKSPACE / KEY_ESCAPE
#include "alghoststudio.h"
#include "aldirectorselect.h"       // cloneInFrontOfPick: the same foreground rule as the menu / Director pick
#include "altoolghostplace.h"
#include "llagent.h"                // global <-> agent (pick projection)
#include "llghostavatar.h"
#include "lltoolmgr.h"
#include "llviewercamera.h"         // projectPosAgentToScreen (screen-space pick)
#include "llviewerobject.h"         // [GhostEditFix] LLPointer<LLViewerObject> needs the full type
#include "llviewerwindow.h"         // gViewerWindow

namespace
{
constexpr F32 GHOST_PICK_PX = ALGhostStudio::SCREEN_PICK_PX; // screen radius for a ghost hit, px
} // anonymous namespace

ALToolGhostEdit::ALToolGhostEdit()
:   LLTool(std::string("GhostEdit"))
{
}

bool ALToolGhostEdit::beginEditFor(const LLUUID& owner_id)
{
    if (owner_id.isNull() ||
        (mOwnerId.notNull() && mOwnerId != owner_id) ||
        ALGhostStudio::instance().getCrowdPlacementDraft().mActive)
    {
        return false;
    }
    if (ALToolGhostPlace::instanceExists() &&
        ALToolGhostPlace::getInstance()->armedOwner().notNull())
    {
        return false;
    }
    mOwnerId = owner_id;
    return true;
}

bool ALToolGhostEdit::stopEditModeFor(
    const LLUUID& owner_id, bool restore_toolset)
{
    if (owner_id.isNull() || mOwnerId != owner_id)
    {
        return false;
    }
    stopEditMode(restore_toolset);
    return true;
}

// ---------------------------------------------------------------------------
LLUUID ALToolGhostEdit::pickInstance(S32 x, S32 y) const
{
    // [CloneAnimDrop] the screen-space pick (foot / mid-body probes within
    // GHOST_PICK_PX) lives in ALGhostStudio so 3D drag-and-drop hits clones
    // exactly the way this tool does.
    return ALGhostStudio::instance().pickInstanceAtScreen(x, y, GHOST_PICK_PX);
}

// ---------------------------------------------------------------------------
bool ALToolGhostEdit::handleMouseDown(S32 x, S32 y, MASK mask)
{
    // Select the clicked ghost; the per-frame proxy tick then spawns the proxy on
    // the selection and hands off to the stock move/rotate gizmos. This only runs
    // while NO ghost is selected yet -- once one is, the stock tool is current.
    const LLUUID hit = pickInstance(x, y);
    if (hit.notNull())
    {
        ALGhostStudio& studio = ALGhostStudio::instance();
        studio.setSelected(hit);
    }
    return true;        // tool mode: clicks never fall through to world selection
}

bool ALToolGhostEdit::handleStockPick(const LLPickInfo& pick_info, bool gizmo_part_hit)
{
    // [GhostEditFix] Only while a ghost session is in the stock-gizmo state
    // (proxy handed off) and the click is not on a gizmo part.
    if (!mEditModeActive || !mManipProxy.isActive() ||
        !mManipProxy.hasToolHandoff() || gizmo_part_hit)
    {
        return false;
    }
    ALGhostStudio& studio = ALGhostStudio::instance();
    if (!studio.getShowAll())
    {
        return false;   // tick() ends the session on master-hide
    }
    const LLPointer<LLViewerObject> hit_obj = pick_info.getObject();
    if (hit_obj.notNull() && mManipProxy.owns(hit_obj.get()))
    {
        return false;   // the (invisible) proxy itself: stock keeps it selected
    }
    LLVector3 probe_agent;
    LLUUID ghost = studio.pickInstanceAtScreen(pick_info.mMousePt.mX, pick_info.mMousePt.mY,
                                               GHOST_PICK_PX, false, &probe_agent);
    if (ghost.notNull() && !ALDirectorSelect::cloneInFrontOfPick(pick_info, probe_agent))
    {
        ghost.setNull();    // a wall / avatar in front of the clone keeps its stock selection click
    }
    if (ghost.notNull())
    {
        if (ghost != studio.getSelected())
        {
            studio.setSelected(ghost);  // tick() rebuilds the proxy on the new ghost
        }
        return true;                    // consume: stock selection unchanged
    }
    if (hit_obj.notNull())
    {
        return false;   // real prim / avatar: today's behaviour exactly
    }
    // Only a genuine land or sky click counts as "empty ground"; HUD icons,
    // parcel walls and other pick kinds keep today's stock behaviour.
    if (pick_info.mPickType != LLPickInfo::PICK_LAND &&
        pick_info.mPickType != LLPickInfo::PICK_INVALID)
    {
        return false;
    }
    mManipProxy.noteEmptyGroundClick(); // confirmed empty ground: tick() ends edit mode
    return true;
}

bool ALToolGhostEdit::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
    // let the normal right-drag camera work -- never trap the user without
    // camera control (the path tool's rule)
    return false;
}

bool ALToolGhostEdit::handleKey(KEY key, MASK mask)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    if ((key == KEY_DELETE || key == KEY_BACKSPACE) && studio.getSelected().notNull())
    {
        LLUUID delete_id = studio.getSelected();
        // Expanded members are an individual-positioning view. Deleting one
        // must never silently invoke removeInstance()'s whole-group semantics;
        // an explicitly selected header, however, is the whole authoring unit.
        if (const ALGhostGroupModel::Group* group =
                studio.groupForMember(delete_id))
        {
            if (group->mEditMembers && delete_id != group->mId)
            {
                return true;
            }
            if (delete_id == group->mId)
            {
                if (group->mMembers.empty())
                {
                    return true;
                }
                delete_id = group->mMembers.front().mInstanceId;
            }
        }
        studio.requestRemoveInstance(delete_id);    // also clears the selection
        return true;
    }
    if (key == KEY_ESCAPE)
    {
        stopEditMode();
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
void ALToolGhostEdit::handleSelect()
{
    // The singleton is only meaningful after one panel has acquired it.
    // Refuse an accidental unowned activation rather than creating a session
    // that either panel could later destroy.
    if (mOwnerId.isNull())
    {
        mEditModeActive = false;
        return;
    }
    mEditModeActive = true;
    mManipProxy.begin();
    gViewerWindow->setCursor(UI_CURSOR_TOOLTRANSLATE);
}

void ALToolGhostEdit::handleDeselect()
{
    // selectProxy() records the saved toolset before its intentional handoff,
    // so that deselect must keep editing alive. If no handoff has happened,
    // another tool replaced the picker itself; treat that as a real departure
    // and never let the next proxy tick reinstall us over the user's choice.
    if (mManipProxy.isActive() && !mManipProxy.hasToolHandoff())
    {
        stopEditMode(/*restore_toolset*/ false);
    }
    else if (!mManipProxy.isActive())
    {
        mEditModeActive = false;
    }
}

void ALToolGhostEdit::stopEditMode(bool restore_toolset)
{
    LLToolMgr* tool_mgr = LLToolMgr::getInstance();
    const bool picker_is_transient =
        tool_mgr && tool_mgr->usingTransientTool() &&
        tool_mgr->getCurrentTool() == this;
    // Clear ownership before teardown: restoring a tool can synchronously
    // invoke selection callbacks, and none of them may inherit the old claim.
    mOwnerId.setNull();
    mEditModeActive = false;
    mManipProxy.teardown(restore_toolset);
    // State B has no stock-gizmo handoff to replace the transient picker.
    // Clear it here for every terminal path (including master-hide from the
    // non-owner panel), while preserving any different tool installed
    // reentrantly during teardown.
    if (picker_is_transient &&
        tool_mgr->usingTransientTool() &&
        tool_mgr->getCurrentTool() == this)
    {
        tool_mgr->clearTransientTool();
    }
}
