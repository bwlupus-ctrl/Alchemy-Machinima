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

class ALToolGhostPlace final : public LLTool, public LLSingleton<ALToolGhostPlace>
{
    LLSINGLETON(ALToolGhostPlace);

public:
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleHover(S32 x, S32 y, MASK mask) override;
    bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleKey(KEY key, MASK mask) override;

    void handleSelect() override;
    void handleDeselect() override;

    // arm placement for one instance; the panel activates the tool right after
    void armFor(const LLUUID& instance_id) { mInstance = instance_id; }
    const LLUUID& armedInstance() const { return mInstance; }

private:
    LLUUID mInstance;   // ghost instance the next ground click places
};

#endif // AL_ALTOOLGHOSTPLACE_H
