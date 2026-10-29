/**
 * @file altoolfocuspick.h
 * @brief One-shot in-world eyedropper for Ultimate Diopter focus/depth
 *        controls -- see doc/DIOPTER_SMART_UI_DESIGN.md §5.2.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Modelled on ALToolGhostPlace's data-only point-pick mode
 * (altoolghostplace.h/.cpp, armForPointPick/handleMouseDown) with the
 * ghost/resolver branches stripped: this tool never touches a scene object,
 * it only resolves a world point under the pointer and reports the
 * camera-forward distance to it, in metres, using the EXACT convention
 * renderDoF (pipeline.cpp ~18672) and renderUltimateDiopter (~14207) both
 * consume -- (point_agent - camera_origin) . camera_at_axis, never
 * Euclidean, so the eyedropper and "Camera focus point" mode can never
 * disagree (§5.2).
 *
 * One button arms it for exactly one owner; the next left-click resolves
 * and fires the one-shot commit callback, then hands the camera straight
 * back (LLToolMgr::clearTransientTool(), same idiom as every other
 * transient tool in this fork). Esc / right-click / losing the tool to
 * another transient tool all cancel cleanly through the same disarm() path,
 * so the commit callback fires exactly once either way -- accepted, or not.
 *
 * A second, REPEATING (not one-shot) preview callback fires at ~10 Hz while
 * armed and the pointer is hovering a valid world point, so the floater can
 * show the metre value in its readout strip before the director commits
 * (§5.2: "throttle a hover pick to ~10 Hz ... committing only on click",
 * applied uniformly to all three pickers -- "not worth a behavioural
 * inconsistency between two buttons that look identical").
 */

#ifndef AL_ALTOOLFOCUSPICK_H
#define AL_ALTOOLFOCUSPICK_H

#include "lltool.h"
#include "llsingleton.h"
#include "lluuid.h"
#include "v3dmath.h"

#include <functional>

class ALToolFocusPick final : public LLTool, public LLSingleton<ALToolFocusPick>
{
    LLSINGLETON(ALToolFocusPick);

public:
    // One-shot: fires exactly once per successful arm() -- accepted=true on
    // a committed click, accepted=false on Esc / right-click / tool
    // replacement / a competing arm() refusing this owner. depth_m and
    // point_global are only meaningful when accepted.
    using DepthPickCallback =
        std::function<void(bool accepted, F32 depth_m, const LLVector3d& point_global)>;

    // Repeating: fires at most ~10 Hz while armed and hovering a valid
    // world point. Never fires on a miss -- the caller's last shown value
    // simply holds rather than flickering every frame the pointer wanders
    // off the world during ordinary mouse travel toward the intended point.
    using DepthPreviewCallback = std::function<void(F32 depth_m)>;

    // Arms for exactly one owner; refuses (returns false, changes nothing)
    // if another owner is already armed. on_preview is optional.
    bool arm(const LLUUID& owner_id, DepthPickCallback on_commit,
             DepthPreviewCallback on_preview = DepthPreviewCallback());

    // Cancels only the matching owner's arm -- mirrors
    // ALToolGhostPlace::cancelForOwner so one caller tearing down (e.g. the
    // floater closing) cannot clear a DIFFERENT owner's still-active pick.
    bool cancelForOwner(const LLUUID& owner_id);

    bool isArmed() const { return static_cast<bool>(mCommitCallback); }
    const LLUUID& armedOwner() const { return mOwnerId; }

    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleHover(S32 x, S32 y, MASK mask) override;
    bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleKey(KEY key, MASK mask) override;

    void handleSelect() override;
    void handleDeselect() override;

private:
    void disarm(bool notify_cancel);

    // Shared by handleMouseDown (commit) and handleHover (preview):
    // resolves the world point under (x,y) via pickImmediate (synchronous,
    // no LLHandle lifetime window, no far-land fetchResults() early return
    // -- §5.2) and its camera-forward distance. Uses mPosGlobal, NEVER
    // mIntersection (zero on a terrain hit, llviewerwindow.cpp
    // ~6987-6990/:7038 -- mIntersection is only filled on an object/flora
    // hit). Returns false on a sky/void miss or a behind-the-lens result;
    // never touches mCommitCallback/mPreviewCallback.
    bool resolveDepth(S32 x, S32 y, F32& out_depth_m, LLVector3d& out_point_global) const;

    LLUUID               mOwnerId;
    DepthPickCallback     mCommitCallback;
    DepthPreviewCallback  mPreviewCallback;
    F64                   mLastPreviewTime = 0.0;
};

#endif // AL_ALTOOLFOCUSPICK_H
