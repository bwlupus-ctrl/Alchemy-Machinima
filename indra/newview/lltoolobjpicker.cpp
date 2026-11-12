/**
 * @file lltoolobjpicker.cpp
 * @brief LLToolObjPicker class implementation
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

// LLToolObjPicker is a transient tool, useful for a single object pick.

#include "llviewerprecompiledheaders.h"

#include "lltoolobjpicker.h"

#include "llagent.h"
#include "llselectmgr.h"
#include "llworld.h"
#include "llviewercontrol.h"
#include "llmenugl.h"
#include "lltoolmgr.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewermenu.h"
#include "llviewercamera.h"
#include "llviewerwindow.h"
#include "lldrawable.h"
#include "llrootview.h"
#include "lleventtimer.h"
#include "llnotificationsutil.h"


LLToolObjPicker::LLToolObjPicker()
:   LLTool( std::string("ObjPicker"), NULL ),
    mPicked( false ),
    mHitObjectID( LLUUID::null ),
    mExitCallback( NULL ),
    mExitCallbackData( NULL )
{ }


// returns true if an object was selected
bool LLToolObjPicker::handleMouseDown(S32 x, S32 y, MASK mask)
{
    LLRootView* viewp = gViewerWindow->getRootView();
    bool handled = viewp->handleMouseDown(x, y, mask);

    mHitObjectID.setNull();

    if (! handled)
    {
        // didn't click in any UI object, so must have clicked in the world
        if (mCancelOnEscape)
        {
            if (mPickPending || mMouseReleased) return true;
            mPickPending = true;
            gViewerWindow->pickAsync(x, y, mask, pickCallback, false, false, false, false, mPickGeneration);
        }
        else gViewerWindow->pickAsync(x, y, mask, pickCallback);
        handled = true;
    }
    else
    {
        if (hasMouseCapture())
        {
            setMouseCapture(false);
        }
        else
        {
            LL_WARNS() << "PickerTool doesn't have mouse capture on mouseDown" << LL_ENDL;
        }
    }

    // Pass mousedown to base class
    LLTool::handleMouseDown(x, y, mask);

    return handled;
}

void LLToolObjPicker::pickCallback(const LLPickInfo& pick_info)
{
    auto* picker = LLToolObjPicker::getInstance();
    if (pick_info.mPickGeneration != 0)
    {
        if (!picker->mSessionLive || !picker->mCancelOnEscape ||
            pick_info.mPickGeneration != picker->mPickGeneration) return;
        picker->mPickPending = false;
        picker->mHitObjectID = pick_info.mObjectID;
        picker->mPicked = pick_info.mObjectID.notNull();
        if (picker->mMouseReleased) picker->completeGazePick();
        return;
    }
    // A legacy result must not supply a target to an opt-in session either.
    if (picker->mCancelOnEscape) return;
    LLToolObjPicker::getInstance()->mHitObjectID = pick_info.mObjectID;
    LLToolObjPicker::getInstance()->mPicked = pick_info.mObjectID.notNull();
}


bool LLToolObjPicker::handleMouseUp(S32 x, S32 y, MASK mask)
{
    LLView* viewp = gViewerWindow->getRootView();
    bool handled = viewp->handleHover(x, y, mask);
    if (handled)
    {
        // let UI handle this
    }

    if (mCancelOnEscape && mSessionLive)
    {
        LLTool::handleMouseUp(x, y, mask);
        mMouseReleased = true;
        if (mPickPending)
        {
            const U64 generation = mPickGeneration;
            LLEventTimer::run_after(1.f, [generation]()
            {
                auto* picker = LLToolObjPicker::getInstance();
                if (!picker->mSessionLive || picker->mPickGeneration != generation || !picker->mPickPending) return;
                LLSD args;
                args["MESSAGE"] = "Object pick timed out; try again";
                LLNotificationsUtil::add("GenericTip", args);
                picker->cancelPick();
            });
        }
        else completeGazePick();
        return handled;
    }
    mCompletedPick = hasMouseCapture() && mHitObjectID.notNull();
    LLTool::handleMouseUp(x, y, mask);
    if (hasMouseCapture())
    {
        setMouseCapture(false);
    }
    else
    {
        LL_WARNS() << "PickerTool doesn't have mouse capture on mouseUp" << LL_ENDL;
    }
    return handled;
}


bool LLToolObjPicker::handleHover(S32 x, S32 y, MASK mask)
{
    LLView *viewp = gViewerWindow->getRootView();
    bool handled = viewp->handleHover(x, y, mask);
    if (!handled)
    {
        // Used to do pick on hover.  Now we just always display the cursor.
        ECursorType cursor = UI_CURSOR_ARROWLOCKED;

        cursor = UI_CURSOR_TOOLPICKOBJECT3;

        gViewerWindow->setCursor(cursor);
    }
    return handled;
}


void LLToolObjPicker::onMouseCaptureLost()
{
    if (mCancelOnEscape)
    {
        finishSession();
        return;
    }
    mSessionLive = false;
    if (mExitCallback)
    {
        mExitCallback(mExitCallbackData);

        mExitCallback = NULL;
        mExitCallbackData = NULL;
    }

    mPicked = false;
    mHitObjectID.setNull();
}

// virtual
void LLToolObjPicker::setExitCallback(void (*callback)(void *), void *callback_data, bool cancel_on_escape)
{
    // A new owner must finish the old owner's exit path before replacing it.
    // Capture loss runs legacy callbacks too (particle editor and reporter).
    if (mSessionLive)
    {
        cancelPick();
        LLToolMgr::getInstance()->clearTransientTool();
    }
    mCancelOnEscape = cancel_on_escape;
    mExitCallback = callback;
    mExitCallbackData = callback_data;
}

// virtual
void LLToolObjPicker::handleSelect()
{
    ++mPickGeneration;
    mPickPending = false;
    mMouseReleased = false;
    mSessionLive = true;
    mCompletedPick = false;
    mHitObjectID.setNull();
    LLTool::handleSelect();
    setMouseCapture(true);
}

// virtual
void LLToolObjPicker::handleDeselect()
{
    mCompletedPick = false;
    if (mCancelOnEscape && mSessionLive) cancelPick();
    if (hasMouseCapture())
    {
        LLTool::handleDeselect();
        setMouseCapture(false);
    }
}




bool LLToolObjPicker::handleKey(KEY key, MASK mask)
{
    if (mCancelOnEscape && key == KEY_ESCAPE)
    {
        mCompletedPick = false;
        LLToolMgr::getInstance()->clearTransientTool();
        return true;
    }
    return LLTool::handleKey(key, mask);
}

void LLToolObjPicker::cancelPick()
{
    mCompletedPick = false;
    mHitObjectID.setNull();
    if (mCancelOnEscape) ++mPickGeneration;
    if (hasMouseCapture()) setMouseCapture(false);
    if (mCancelOnEscape && mSessionLive) finishSession();
}

// Mark ended and detach the callback before invoking owner cleanup: it can reenter
// the tool manager or clear/replace the callback. Capture may already be absent.
void LLToolObjPicker::finishSession()
{
    if (!mSessionLive) return;
    mSessionLive = false;
    ++mPickGeneration;
    mPickPending = false;
    auto callback = mExitCallback;
    void* data = mExitCallbackData;
    mExitCallback = nullptr;
    mExitCallbackData = nullptr;
    if (callback) callback(data);
    mPicked = false;
    mHitObjectID.setNull();
}

void LLToolObjPicker::completeGazePick()
{
    mCompletedPick = hasMouseCapture() && mHitObjectID.notNull();
    if (hasMouseCapture()) setMouseCapture(false);
    if (mSessionLive) finishSession();
}
