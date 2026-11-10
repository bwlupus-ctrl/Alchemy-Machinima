/**
 * @file altooldirectorpick.h
 * @brief Persistent click-to-pick mode behind the Director Actor bar "Select"
 *        toggle.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * While on, a left-click on an avatar or a Studio clone in the world selects
 * it through ALDirectorSelect (the same helper as the right-click "Select in
 * Director" items). Ctrl-click ADDS clones to the clone multi-selection;
 * an avatar stays single and a Ctrl-click on one is a plain click. The tool
 * is transient (pattern of altoolfocuspick.cpp). It only starts when no other
 * transient tool, clone Edit / Place session or crowd placement is active, so
 * the base tool it hands back on every exit is the tool the user had: Esc, the button again, closing the Director, or another
 * tool replacing it. Right-click is not taken, so the world menus and camera
 * keys behave as usual.
 */

#ifndef AL_ALTOOLDIRECTORPICK_H
#define AL_ALTOOLDIRECTORPICK_H

#include "lltool.h"
#include "llsingleton.h"

class ALToolDirectorPick final : public LLTool, public LLSingleton<ALToolDirectorPick>
{
    LLSINGLETON(ALToolDirectorPick);

public:
    // The Actor bar button: starts the mode, or ends it when already on.
    void toggle();
    // Refuses (false, with a tip) while another transient tool or a clone
    // session is active: the single transient slot cannot hold both, so the
    // mode never displaces a session it could not give back.
    bool start();
    // Ends the mode and restores the previous tool. Safe when not active.
    void stop();
    static bool isActive();
    // Closing the Director (or its shell going away) ends the mode.
    static void stopIfActive();
    // Unmodified Esc while the picker is NOT the current tool (a camera gesture
    // cleared the slot and the re-arm is pending): the key reaches the base tool,
    // so the window routes it here. Ends the mode (a new generation, so the
    // queued re-arm is ignored). False when the picker is current (its own
    // handleKey takes Esc) or the mode is off.
    static bool handlePendingEscape();

    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    // A double-click is a click here: consumed, so no walk-to / teleport binding fires.
    bool handleDoubleClick(S32 x, S32 y, MASK mask) override;
    bool handleHover(S32 x, S32 y, MASK mask) override;
    bool handleKey(KEY key, MASK mask) override;
    void handleSelect() override;
    void handleDeselect() override;

private:
    // LLToolCamera::releaseMouse() clears the transient slot after a camera
    // gesture. A gesture forwarded from here ends that way, so the tool puts
    // itself back (on idle, never inside handleDeselect) while nobody else
    // took the slot.
    void rearmAfterCamera();

    bool mActive = false;
    bool mCameraForwarded = false;
    bool mRearmPending = false;         // one re-arm at a time
    U32  mGeneration = 0;               // bumped by every start() and stop()
    LLTool* mForwardBase = nullptr;     // the base tool when the camera gesture was forwarded
};

#endif // AL_ALTOOLDIRECTORPICK_H
