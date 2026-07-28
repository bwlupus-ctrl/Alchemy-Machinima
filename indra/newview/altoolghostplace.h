/**
 * @file altoolghostplace.h
 * @brief One-shot in-world placement tool for Ghost Studio instances.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * The Ghost Studio panel's "Place" button arms this transient LLTool for ONE
 * ghost instance: the next left-click on ground/prim moves that instance's
 * foot to the picked surface point and hands the camera straight back (same
 * clearTransientTool idiom as ALToolPathEdit's "Walk to here" one-shot). A
 * sky-miss stays armed for another try; Esc or right-click cancels without
 * moving anything. Deliberately minimal -- node dragging/selection stays the
 * path tool's business; this only answers "put THIS ghost THERE".
 */

#ifndef AL_ALTOOLGHOSTPLACE_H
#define AL_ALTOOLGHOSTPLACE_H

#include "lltool.h"
#include "llsingleton.h"
#include "lluuid.h"
#include "v3dmath.h"

#include <functional>

class ALToolGhostPlace final : public LLTool, public LLSingleton<ALToolGhostPlace>
{
    LLSINGLETON(ALToolGhostPlace);

public:
    using PointPickCallback =
        std::function<void(bool accepted, const LLVector3d& point_global)>;

    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleHover(S32 x, S32 y, MASK mask) override;
    bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleKey(KEY key, MASK mask) override;

    void handleSelect() override;
    void handleDeselect() override;

    // Arm placement for one instance; the panel activates the tool right
    // after. The facing-target form is retained for legacy callers.
    bool armFor(const LLUUID& owner_id, const LLUUID& instance_id,
                bool facing_target = false);

    // Crowd authoring uses a data-only point pick. A successful pick calls
    // back with accepted=true; Esc, right-click, tool replacement, or panel
    // hide calls back with accepted=false. This mode never touches a ghost.
    bool armForPointPick(const LLUUID& owner_id, PointPickCallback callback);
    // Cancel only the matching panel's arm. This prevents one of the two
    // Ghost Studio hosts from clearing the other host's transient tool.
    bool cancelForOwner(const LLUUID& owner_id);
    bool isPointPickArmed() const
    { return static_cast<bool>(mPointPickCallback); }
    const LLUUID& armedInstance() const { return mInstance; }
    const LLUUID& armedOwner() const { return mOwnerId; }

private:
    void disarm(bool notify_cancel);

    LLUUID mOwnerId;    // panel/session that owns the current one-shot arm
    LLUUID mInstance;   // instance id, or an explicit group-header id
    bool mFacingTarget = false;
    PointPickCallback mPointPickCallback;
};

#endif // AL_ALTOOLGHOSTPLACE_H
