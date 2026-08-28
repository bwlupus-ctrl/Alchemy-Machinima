/**
 * @file altoolfocuspick.cpp
 * @brief One-shot Ultimate Diopter focus/depth eyedropper -- see
 *        altoolfocuspick.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "altoolfocuspick.h"

#include "indra_constants.h"        // KEY_ESCAPE
#include "llagent.h"                // gAgent.getPosAgentFromGlobal
#include "lltimer.h"                // LLTimer::getElapsedSeconds -- preview throttle
#include "lltoolmgr.h"
#include "llviewercamera.h"
#include "llviewerwindow.h"         // gViewerWindow, pickImmediate, LLPickInfo

#include <utility>

namespace
{
// §5.2: "behind the lens" guard on the dot-product distance -- matches the
// renderer's own `d > 0.05f` gate at pipeline.cpp's base-focus/alt-zoom
// resolver branches, so a pick can never accept a depth the renderer itself
// would have rejected as too close to be meaningful.
constexpr F32 AL_FOCUS_PICK_MIN_DEPTH_M = 0.05f;

// ~10 Hz -- §5.2's explicit throttle for the hover preview.
constexpr F64 AL_FOCUS_PICK_PREVIEW_INTERVAL_S = 0.1;
} // namespace

ALToolFocusPick::ALToolFocusPick()
:   LLTool(std::string("FocusPick"))
{
}

bool ALToolFocusPick::arm(const LLUUID& owner_id, DepthPickCallback on_commit,
                           DepthPreviewCallback on_preview)
{
    if (owner_id.isNull() || !on_commit ||
        (mOwnerId.notNull() && mOwnerId != owner_id))
    {
        return false;
    }
    disarm(true);
    if (mOwnerId.notNull())
    {
        // A cancellation callback re-armed the singleton (e.g. from inside
        // the disarm() notification just above). Never clobber that newer
        // ownership claim on return from user code -- ALToolGhostPlace's
        // own precedent, altoolghostplace.cpp armFor()/armForPointPick().
        return false;
    }
    mOwnerId = owner_id;
    mCommitCallback = std::move(on_commit);
    mPreviewCallback = std::move(on_preview);
    mLastPreviewTime = 0.0;
    return true;
}

bool ALToolFocusPick::cancelForOwner(const LLUUID& owner_id)
{
    if (owner_id.isNull() || mOwnerId != owner_id)
    {
        return false;
    }
    LLToolMgr* tool_mgr = LLToolMgr::getInstance();
    if (tool_mgr->usingTransientTool() && tool_mgr->getCurrentTool() == this)
    {
        // clearTransientTool() restores the prior tool, which invokes
        // handleDeselect() -> disarm(true) below; go through it rather than
        // disarming directly so the camera/tool state stays consistent.
        tool_mgr->clearTransientTool();
    }
    else
    {
        disarm(true);
    }
    return true;
}

void ALToolFocusPick::disarm(bool notify_cancel)
{
    DepthPickCallback callback = std::move(mCommitCallback);
    mPreviewCallback = DepthPreviewCallback();
    mOwnerId.setNull();
    if (notify_cancel && callback)
    {
        callback(false, 0.f, LLVector3d());
    }
}

bool ALToolFocusPick::resolveDepth(S32 x, S32 y, F32& out_depth_m,
                                    LLVector3d& out_point_global) const
{
    // §5.2: pickImmediate, never pickAsync (synchronous result, no static-
    // callback relay, no LLHandle lifetime window, and it dodges the
    // fetchResults() early-return at llviewerwindow.cpp ~6960 that swallows
    // the async callback on a far-land miss). pick_rigged = true (unlike
    // the fork's placement tools, which use false) -- a director focusing
    // on an actor's face wants the ray to stop at the body.
    const LLPickInfo pick = gViewerWindow->pickImmediate(
        x, y, /*pick_transparent=*/false, /*pick_rigged=*/true);

    // mPosGlobal, never mIntersection -- mIntersection is only filled on a
    // PICK_OBJECT/PICK_FLORA hit (llviewerwindow.cpp ~6987-6990) and stays
    // (0,0,0) on a terrain hit (~7038), which would silently read as a
    // ground miss. isExactlyZero() is the fork's own miss idiom
    // (altoolpathedit.cpp groundPointAt()).
    if (pick.mPosGlobal.isExactlyZero() || !pick.mPosGlobal.isFinite())
    {
        return false;
    }

    LLViewerCamera* camera = LLViewerCamera::getInstance();
    const LLVector3 pt_agent = gAgent.getPosAgentFromGlobal(pick.mPosGlobal);
    // Dot-product distance along the camera's forward axis -- the EXACT
    // convention renderDoF and renderUltimateDiopter both consume. Euclidean
    // distance would read subtly wrong off-axis and would disagree with
    // "Camera focus point" mode for the same on-screen point (§5.2).
    const F32 depth_m = (pt_agent - camera->getOrigin()) * camera->getAtAxis();
    if (depth_m <= AL_FOCUS_PICK_MIN_DEPTH_M)
    {
        return false;   // behind the lens
    }

    out_depth_m = depth_m;
    out_point_global = pick.mPosGlobal;
    return true;
}

// ---------------------------------------------------------------------------
bool ALToolFocusPick::handleMouseDown(S32 x, S32 y, MASK mask)
{
    F32 depth_m;
    LLVector3d point_global;
    if (!resolveDepth(x, y, depth_m, point_global))
    {
        return true;    // sky/void/behind-lens miss: stay armed, try again
    }

    // Move the callback out before invoking it, and clear the preview
    // callback too, so clearing the transient tool below cannot report a
    // second, spurious cancellation from handleDeselect() -- same guard
    // ALToolGhostPlace::handleMouseDown uses for its crowd-facing branch.
    DepthPickCallback accepted = std::move(mCommitCallback);
    mPreviewCallback = DepthPreviewCallback();
    mOwnerId.setNull();
    accepted(true, depth_m, point_global);

    LLToolMgr::getInstance()->clearTransientTool();
    return true;
}

bool ALToolFocusPick::handleHover(S32 x, S32 y, MASK mask)
{
    gViewerWindow->setCursor(UI_CURSOR_PIPETTE);

    if (mPreviewCallback)
    {
        const F64 now = LLTimer::getElapsedSeconds();
        if (now - mLastPreviewTime >= AL_FOCUS_PICK_PREVIEW_INTERVAL_S)
        {
            mLastPreviewTime = now;
            F32 depth_m;
            LLVector3d point_global;
            if (resolveDepth(x, y, depth_m, point_global))
            {
                mPreviewCallback(depth_m);
            }
            // A miss leaves the floater's last shown preview value alone
            // rather than clearing it every frame the pointer wanders off
            // the world -- avoids a flickering readout during ordinary
            // mouse travel toward the intended point.
        }
    }
    return true;
}

bool ALToolFocusPick::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
    // right-click = cancel; never trap the director in a mode they can't
    // see how to leave.
    LLToolMgr::getInstance()->clearTransientTool();
    return true;
}

bool ALToolFocusPick::handleKey(KEY key, MASK mask)
{
    if (key == KEY_ESCAPE)
    {
        LLToolMgr::getInstance()->clearTransientTool();
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
void ALToolFocusPick::handleSelect()
{
    gViewerWindow->setCursor(UI_CURSOR_PIPETTE);
}

void ALToolFocusPick::handleDeselect()
{
    // Never leave a stale arm pointing at a dead owner, and report a
    // cancellation exactly once if tool replacement ended the session.
    disarm(true);
}
