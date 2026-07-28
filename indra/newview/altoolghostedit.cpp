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
#include "altoolghostplace.h"
#include "llagent.h"                // global <-> agent (pick projection)
#include "lltoolmgr.h"
#include "llviewercamera.h"         // projectPosAgentToScreen (screen-space pick)
#include "llviewerwindow.h"         // gViewerWindow

namespace
{
constexpr F32 GHOST_PICK_PX = 18.f;     // screen radius for a ghost hit, px
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
    ALGhostStudio& studio = ALGhostStudio::instance();
    if (!studio.getShowAll())
    {
        return LLUUID::null;
    }
    // screen-space pick like the path tool's node pick: the ghosts are overlay
    // draws, not scene objects, so project each enabled instance's FOOT and a
    // MID-BODY point (foot + 1 m, easier to grab) and take the nearest in radius
    LLViewerCamera* cam = LLViewerCamera::getInstance();
    LLUUID best;
    F32 best_d2 = GHOST_PICK_PX * GHOST_PICK_PX;
    for (const ALGhostStudio::Instance& inst : studio.getInstances())
    {
        if (!inst.mEnabled)
        {
            continue;       // you pick what you can see; hidden ghosts via the list
        }
        const LLVector3 foot = gAgent.getPosAgentFromGlobal(inst.mFootGlobal);
        LLVector3 probes[2] = { foot, foot };
        probes[1].mV[VZ] += 1.f * inst.mScale;      // mid-body, scale-aware
        for (const LLVector3& p : probes)
        {
            LLCoordGL screen;
            if (!cam->projectPosAgentToScreen(p, screen, false))
            {
                continue;   // behind the camera
            }
            const F32 dx = (F32)(screen.mX - x);
            const F32 dy = (F32)(screen.mY - y);
            const F32 d2 = dx * dx + dy * dy;
            if (d2 < best_d2)
            {
                best_d2 = d2;
                best = inst.mId;
            }
        }
    }
    return best;
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
        ALGhostStudio::instance().setSelected(hit);
    }
    return true;        // tool mode: clicks never fall through to world selection
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
        studio.removeInstance(delete_id);    // also clears the selection
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
