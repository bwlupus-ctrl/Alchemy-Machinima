/**
 * @file altooldirectorpick.cpp
 * @brief Persistent click-to-pick mode for the Director Actor bar -- see
 *        altooldirectorpick.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "altooldirectorpick.h"

#include "aldirectorselect.h"
#include "alghostclonemenulogic.h"
#include "alghoststudio.h"
#include "altoolghostedit.h"
#include "altoolghostplace.h"
#include "llnotificationsutil.h"
#include "lltrans.h"
#include "llagentcamera.h"
#include "llcallbacklist.h"
#include "indra_constants.h"        // KEY_ESCAPE, MASK_CONTROL
#include "lltoolfocus.h"
#include "lltoolmgr.h"
#include "llvoavatar.h"
#include "llviewerwindow.h"         // gViewerWindow, pickImmediate, LLPickInfo

ALToolDirectorPick::ALToolDirectorPick()
:   LLTool(std::string("DirectorPick"))
{
}

// static
bool ALToolDirectorPick::isActive()
{
    return instanceExists() && getInstance()->mActive;
}

// static
void ALToolDirectorPick::stopIfActive()
{
    if (isActive())
    {
        getInstance()->stop();
    }
}

bool ALToolDirectorPick::handlePendingEscape()
{
    if (!isActive() || LLToolMgr::getInstance()->getCurrentTool() == getInstance())
    {
        return false;
    }
    getInstance()->stop();
    return true;
}

bool ALToolDirectorPick::handleDoubleClick(S32 x, S32 y, MASK mask)
{
    return handleMouseDown(x, y, mask);
}

void ALToolDirectorPick::toggle()
{
    if (mActive)
    {
        stop();
    }
    else
    {
        start();
    }
}

bool ALToolDirectorPick::start()
{
    if (mActive)
    {
        return true;
    }
    LLToolMgr* tool_mgr = LLToolMgr::getInstance();
    const bool busy =
        tool_mgr->usingTransientTool() ||
        (ALToolGhostEdit::instanceExists() && ALToolGhostEdit::getInstance()->isEditModeActive()) ||
        (ALToolGhostPlace::instanceExists() && ALToolGhostPlace::getInstance()->armedOwner().notNull()) ||
        ALGhostStudio::instance().getCrowdPlacementDraft().mActive;
    if (busy)
    {
        LLNotificationsUtil::add("SystemMessageTip",
            LLSD().with("MESSAGE", LLTrans::getString("DirectorSelectPickBusy")));
        return false;
    }
    ++mGeneration;
    mCameraForwarded = false;
    mRearmPending = false;
    mActive = true;
    tool_mgr->setTransientTool(this);
    return true;
}

void ALToolDirectorPick::stop()
{
    if (!mActive)
    {
        return;
    }
    ++mGeneration;                      // any queued re-arm belongs to a finished activation
    mCameraForwarded = false;
    mRearmPending = false;
    mActive = false;
    LLToolMgr* tool_mgr = LLToolMgr::getInstance();
    // clearTransientTool() restores the previous tool, which calls
    // handleDeselect(); mActive is already false so that is a no-op.
    if (tool_mgr->usingTransientTool() && tool_mgr->getCurrentTool() == this)
    {
        tool_mgr->clearTransientTool();
    }
}

bool ALToolDirectorPick::handleMouseDown(S32 x, S32 y, MASK mask)
{
    if (!ALDirectorSelect::directorVisible())
    {
        stop();                 // the Director went away: the mode ends with it
        return true;
    }
    // The transient slot suppresses the keyboard-driven override tools, so
    // Alt (+ camera modifiers) left-drag is handed to the camera tool here:
    // it takes the mouse capture and receives the drag and the release itself.
    LLTool* base = LLToolMgr::getInstance()->getBaseTool();
    if (LLTool* override_tool = base ? base->getOverrideTool(mask) : nullptr)
    {
        if (override_tool == LLToolCamera::getInstance())
        {
            mCameraForwarded = true;
            mForwardBase = base;
            return override_tool->handleMouseDown(x, y, mask);
        }
    }
    const LLPickInfo pick = gViewerWindow->pickImmediate(
        x, y, /*pick_transparent=*/false, /*pick_rigged=*/true);
    const bool ctrl = (mask & MASK_CONTROL) != 0;
    const LLUUID clone = ALDirectorSelect::cloneAtPick(pick);
    if (clone.notNull())
    {
        ALDirectorSelect::selectClone(clone,
            ALGhostCloneMenuLogic::ctrlAddsToSet(/*is_clone=*/true, ctrl));
        return true;
    }
    if (LLVOAvatar* avatar = ALDirectorSelect::avatarAtPick(pick))
    {
        // An avatar never joins a set: Ctrl is a plain click here.
        ALDirectorSelect::selectAvatar(avatar->getID(),
            ALGhostCloneMenuLogic::ctrlAddsToSet(/*is_clone=*/false, ctrl));
    }
    return true;                // stay in the mode on a miss
}

void ALToolDirectorPick::rearmAfterCamera()
{
    if (mRearmPending)
    {
        return;                         // one pending re-arm at a time
    }
    mRearmPending = true;
    const U32 generation = mGeneration;
    LLTool* const forward_base = mForwardBase;
    doOnIdleOneTime([generation, forward_base]()
    {
        if (!instanceExists())
        {
            return;
        }
        ALToolDirectorPick* self = getInstance();
        if (self->mGeneration != generation)
        {
            return;                     // stopped / restarted since: not this activation's business
        }
        self->mRearmPending = false;
        LLToolMgr* tool_mgr = LLToolMgr::getInstance();
        switch (ALGhostCloneMenuLogic::rearmDecision(
                    self->mActive, true, tool_mgr->getCurrentTool() == self,
                    ALDirectorSelect::directorVisible(),
                    gAgentCamera.getCameraMode() == CAMERA_MODE_MOUSELOOK,
                    tool_mgr->getBaseTool() == forward_base,
                    tool_mgr->usingTransientTool()))
        {
        case ALGhostCloneMenuLogic::RearmAction::Install:
            tool_mgr->setTransientTool(self);
            break;
        case ALGhostCloneMenuLogic::RearmAction::EndMode:
            self->mActive = false;      // the picker is not installed here; the slot is left alone
            break;
        case ALGhostCloneMenuLogic::RearmAction::Ignore:
            break;
        }
    });
}

bool ALToolDirectorPick::handleHover(S32 x, S32 y, MASK mask)
{
    mCameraForwarded = false;       // we are current again: the camera gesture is over
    if (!ALDirectorSelect::directorVisible())
    {
        stop();
        return true;
    }
    gViewerWindow->setCursor(UI_CURSOR_TOOLPICKOBJECT3);
    return true;
}

bool ALToolDirectorPick::handleKey(KEY key, MASK mask)
{
    if (key == KEY_ESCAPE && mask == MASK_NONE)
    {
        stop();
        return true;
    }
    return false;               // camera and movement keys are not ours
}

void ALToolDirectorPick::handleSelect()
{
    gViewerWindow->setCursor(UI_CURSOR_TOOLPICKOBJECT3);
}

void ALToolDirectorPick::handleDeselect()
{
    if (!mActive)
    {
        return;
    }
    // App focus loss deselects without removing the transient tool: keep the
    // mode. A real tool change has already updated the selected tool:
    // LLToolMgr::getCurrentTool() sets mSelectedTool = cur_tool BEFORE it calls
    // prev_tool->handleDeselect() (lltoolmgr.cpp), and a transient replacement
    // is already mTransientTool, so getCurrentTool() names the new tool here.
    if (LLToolMgr::getInstance()->getCurrentTool() == this)
    {
        return;
    }
    if (mCameraForwarded)
    {
        // The camera tool ended its own gesture and cleared the transient
        // slot: the mode survives and re-arms when the slot is still free.
        mCameraForwarded = false;
        rearmAfterCamera();
        return;
    }
    mActive = false;
}
