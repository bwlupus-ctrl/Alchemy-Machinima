/**
 * @file altoolvirtuallightedit.h
 * @brief In-world drag tool for virtual lights: move a light, or swing its aim tip.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Modelled on ALToolKeyframeEdit: nearest marker within 16 px, a camera-facing drag plane,
 * Esc cancels the gesture, Ctrl+Z / Ctrl+Y use the Practicals page history. Ctrl held while
 * dragging a body moves it vertically only. Clicking a marker also makes it the light the
 * Practicals page edits.
 */
#ifndef AL_ALTOOLVIRTUALLIGHTEDIT_H
#define AL_ALTOOLVIRTUALLIGHTEDIT_H

#include "lltool.h"
#include "llsd.h"
#include "llsingleton.h"
#include "lluuid.h"
#include "v3dmath.h"

class ALToolVirtualLightEdit final : public LLTool, public LLSingleton<ALToolVirtualLightEdit>
{
    LLSINGLETON(ALToolVirtualLightEdit);

public:
    void cancelGesture();
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleMouseUp(S32 x, S32 y, MASK mask) override;
    bool handleHover(S32 x, S32 y, MASK mask) override;
    bool handleKey(KEY key, MASK mask) override;
    void handleSelect() override;
    void handleDeselect() override;
    void onMouseCaptureLost() override { cancelGesture(); }

private:
    LLSD     mBefore;
    LLUUID   mKey;
    S32      mHandle = 0;
    U64      mEpoch = 0;
    bool     mDragging = false;
    bool     mMoved = false;
    LLVector3d mStartGlobal;     // body or tip position when the drag began
    LLVector3d mPlaneHit;        // first plane hit
};

#endif // AL_ALTOOLVIRTUALLIGHTEDIT_H
