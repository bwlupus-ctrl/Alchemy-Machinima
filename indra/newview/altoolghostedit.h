/**
 * @file altoolghostedit.h
 * @brief Persistent in-world edit tool-mode for Ghost Studio instances.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * The Ghost Studio panel's "Edit ghosts" toggle activates this transient LLTool.
 * While active it is a thin SELECTOR + host for the build-mode manipulation
 * proxy (ALGhostManipProxy):
 *
 *   - left-click a ghost   -> select it (shared ALGhostStudio selection). The
 *                             per-frame proxy tick then spawns an invisible
 *                             local-only proxy on the selection and hands off to
 *                             the stock 3-axis Translate/Rotate gizmos.
 *   - Delete / Backspace    -> remove the selected ghost
 *   - Esc                   -> leave edit mode (proxy torn down, camera returns)
 *
 * Uniform scale stays on the panel numeric for now. Ghost picking is
 * SCREEN-SPACE (each enabled instance's foot + mid-body project to the screen,
 * nearest within a pixel radius wins). Because selecting a ghost switches to the
 * stock toolset, THIS TOOL STOPS BEING CURRENT while editing continues -- so
 * edit-mode is an explicit state (isEditModeActive()), never "am I the current
 * tool", and handleDeselect() must NOT tear the proxy down during that handoff.
 */

#ifndef AL_ALTOOLGHOSTEDIT_H
#define AL_ALTOOLGHOSTEDIT_H

#include "lltool.h"
#include "llsingleton.h"
#include "lluuid.h"

#include "alghostmanipproxy.h"

class ALToolGhostEdit final : public LLTool, public LLSingleton<ALToolGhostEdit>
{
    LLSINGLETON(ALToolGhostEdit);

public:
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleKey(KEY key, MASK mask) override;

    void handleSelect() override;
    void handleDeselect() override;

    // The in-world build-mode manip proxy driving the selected ghost's gizmos.
    ALGhostManipProxy& getManipProxy() { return mManipProxy; }

    // Edit-mode is an EXPLICIT state, NOT "this tool is current" (selecting a
    // ghost hands off to the stock translate tool). Ownership is equally
    // explicit: Ghost Studio has two panel hosts, and hiding one host must not
    // tear down the other host's singleton edit session.
    bool beginEditFor(const LLUUID& owner_id);
    bool stopEditModeFor(const LLUUID& owner_id,
                         bool restore_toolset = true);
    bool isEditModeActive() const { return mEditModeActive; }
    const LLUUID& editOwner() const { return mOwnerId; }
    // restore_toolset: true for an explicit exit (panel toggle / Esc) restores the
    // pre-edit tool; false when the user already switched tools (departure).
    // This unscoped form is reserved for terminal tool events such as Esc,
    // master-hide, and deliberate departure from the edit toolset.
    void stopEditMode(bool restore_toolset = true);

private:
    // nearest enabled instance to (x,y) within a pixel threshold; null = none
    LLUUID pickInstance(S32 x, S32 y) const;

    ALGhostManipProxy mManipProxy;
    LLUUID            mOwnerId;
    bool              mEditModeActive = false;
};

#endif // AL_ALTOOLGHOSTEDIT_H
