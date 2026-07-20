/**
 * @file altoolghostedit.h
 * @brief Persistent in-world edit tool-mode for Ghost Studio instances.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * [R2-3] The Ghost Studio panel's "Edit ghosts" toggle activates this
 * transient LLTool (modeled on ALToolPathEdit; the one-shot ALToolGhostPlace
 * stays for the Place button's single-click contract). While active:
 *
 *   - left-click a ghost        -> select it (shared ALGhostStudio selection;
 *                                  the panel lists follow, and the selected
 *                                  ghost shows a pulsing ground ring)
 *   - left-drag a ghost         -> move it on the ground (world pick per hover)
 *   - SHIFT + drag a ghost      -> turn it: horizontal mouse travel adjusts
 *                                  yaw, position untouched
 *   - left-click empty ground   -> re-place the SELECTED ghost there
 *   - Delete / Backspace        -> remove the selected ghost
 *   - Esc                       -> leave the tool (camera control returns)
 *   - right-click               -> passes through (camera orbit keeps working;
 *                                  never trap the user)
 *
 * Ghost picking is SCREEN-SPACE like the path tool's node pick: each enabled
 * instance's foot and a mid-body point project to the screen and the nearest
 * within a pixel radius wins. Numeric spinners in the panel stay the
 * precision path; this is the blocking-by-hand path.
 */

#ifndef AL_ALTOOLGHOSTEDIT_H
#define AL_ALTOOLGHOSTEDIT_H

#include "lltool.h"
#include "llsingleton.h"
#include "lluuid.h"

class ALToolGhostEdit final : public LLTool, public LLSingleton<ALToolGhostEdit>
{
    LLSINGLETON(ALToolGhostEdit);

public:
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleMouseUp(S32 x, S32 y, MASK mask) override;
    bool handleHover(S32 x, S32 y, MASK mask) override;
    bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleKey(KEY key, MASK mask) override;

    void handleSelect() override;
    void handleDeselect() override;
    void onMouseCaptureLost() override;

private:
    // nearest enabled instance to (x,y) within the pick radius; null = none
    LLUUID pickInstance(S32 x, S32 y) const;
    // world pick -> global surface point; false when the pick misses (sky)
    bool   groundPointAt(S32 x, S32 y, LLVector3d& out_global) const;

    LLUUID mDragInstance;       // instance being dragged (null = none)
    bool   mYawDrag = false;    // Shift-drag: turning instead of moving
    S32    mYawAnchorX = 0;     // screen x where the yaw drag started
    F32    mYawStart = 0.f;     // instance yaw at yaw-drag start, radians
};

#endif // AL_ALTOOLGHOSTEDIT_H
