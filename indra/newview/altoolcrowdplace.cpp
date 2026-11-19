/**
 * @file altoolcrowdplace.cpp
 * @brief Persistent Ghost Studio crowd-placement tool.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "altoolcrowdplace.h"

#include "alghoststudio.h"
#include "alghostplacementadapter.h"
#include "altoolghostedit.h"
#include "indra_constants.h"
#include "llagent.h"
#include "lltoolfocus.h"
#include "lltoolmgr.h"
#include "lltoolpie.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"

#include <cmath>

namespace
{
constexpr F32 ANCHOR_PICK_RADIUS_PX = 20.f;
constexpr F32 SLOT_PICK_RADIUS_PX = 15.f;

bool pinnedState(const ALGhostInteractionState::State& state)
{
    using namespace ALGhostInteractionState;
    return state.mMode == MODE_PINNED ||
           (state.mMode == MODE_HEIGHT_DRAG &&
            state.mResumeMode == MODE_PINNED);
}

ALGhostInteractionState::WorldPoint worldPoint(const LLVector3d& point)
{
    ALGhostInteractionState::WorldPoint result;
    result.mX = point.mdV[VX];
    result.mY = point.mdV[VY];
    result.mZ = point.mdV[VZ];
    return result;
}

LLVector3d globalPoint(const ALGhostInteractionState::WorldPoint& point)
{
    return LLVector3d(point.mX, point.mY, point.mZ);
}
} // anonymous namespace

ALToolCrowdPlace::ALToolCrowdPlace()
:   LLTool(std::string("GhostCrowdPlace"))
{
}

bool ALToolCrowdPlace::isActive() const
{
    return ALGhostInteractionState::isActive(mState) &&
           sessionMatchesDraft();
}

bool ALToolCrowdPlace::sessionMatchesDraft() const
{
    const ALGhostStudio::CrowdPlacementDraft& draft =
        ALGhostStudio::instance().getCrowdPlacementDraft();
    return draft.mActive &&
           mOwnerId.notNull() &&
           mSessionId.notNull() &&
           draft.mOwnerId == mOwnerId &&
           draft.mSessionId == mSessionId;
}

bool ALToolCrowdPlace::armFor(const LLUUID& owner_id,
                              const LLUUID& session_id)
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    const auto requestMatchesDraft = [&studio, &owner_id, &session_id]()
    {
        const ALGhostStudio::CrowdPlacementDraft& current =
            studio.getCrowdPlacementDraft();
        return owner_id.notNull() && session_id.notNull() &&
               current.mActive && current.mOwnerId == owner_id &&
               current.mSessionId == session_id &&
               studio.crowdPlacementSourceUnchanged();
    };
    // A stale/non-owner call must be observationally inert. In particular it
    // cannot retire the active owner's tool before proving that the Studio has
    // already committed to this exact replacement owner/session pair.
    if (!requestMatchesDraft())
    {
        return false;
    }

    if (ALGhostInteractionState::isActive(mState))
    {
        if (mOwnerId == owner_id && mSessionId == session_id)
        {
            return true;
        }
        stopPlacement(false, true);
        // Tool restoration can invoke viewer callbacks. Revalidate the exact
        // authoritative draft before taking control of it.
        if (!requestMatchesDraft())
        {
            return false;
        }
    }

    const ALGhostStudio::CrowdPlacementDraft& draft =
        studio.getCrowdPlacementDraft();
    LLToolMgr* tool_mgr = LLToolMgr::getInstance();
    if (!tool_mgr || !gBasicToolset ||
        tool_mgr->usingTransientTool() ||
        (ALToolGhostEdit::instanceExists() &&
         ALToolGhostEdit::getInstance()->isEditModeActive()))
    {
        mLastMessage =
            "Finish the active one-shot or edit tool before placing a crowd.";
        return false;
    }

    mSavedToolset = tool_mgr->getCurrentToolset();
    mSavedBaseTool = tool_mgr->getBaseTool();
    // A manually selected, inactive copy of this tool is not a useful restore
    // target.  Fall back to the normal basic select tool in that edge case.
    if (mSavedBaseTool == this)
    {
        mSavedToolset = gBasicToolset;
        mSavedBaseTool = LLToolPie::getInstance();
    }

    mOwnerId = owner_id;
    mSessionId = session_id;
    mState = ALGhostInteractionState::State();
    mRestorePending = false;
    mLastCommitSucceeded = false;
    mLastMessage.clear();
    const F32 rotation_degrees =
        gSavedSettings.getF32("GhostStudioCrowdRotationStepDegrees");
    const F32 height_per_pixel =
        gSavedSettings.getF32("GhostStudioCrowdHeightUnitsPerPixel");
    mConfig.mRadiansPerWheelStep =
        llclamp(llfinite(rotation_degrees) ? rotation_degrees : 5.625f,
                0.01f, 90.f) * DEG_TO_RAD;
    mConfig.mHeightUnitsPerPixel =
        llclamp(llfinite(height_per_pixel) ? height_per_pixel : 0.02f,
                0.001f, 1.f);

    ALGhostInteractionState::Event start;
    start.mType = ALGhostInteractionState::EVENT_START;
    start.mPrototypeAvailable = true;
    start.mHasWorldHit = true;
    start.mWorldHit = worldPoint(draft.mAnchor);
    start.mInitialYaw = draft.mPatternYaw;
    start.mInitialHeight = draft.mHeight;
    ALGhostInteractionState::Transition started =
        ALGhostInteractionState::reduce(mState, start, mConfig);
    mState = started.mState;

    // A panel can re-arm an already pinned draft after a toolset refresh.
    if (draft.mPinned)
    {
        ALGhostInteractionState::Event pin;
        pin.mType = ALGhostInteractionState::EVENT_LEFT_CLICK;
        mState = ALGhostInteractionState::reduce(mState, pin, mConfig).mState;
    }

    // selectTool() writes LLToolMgr::mBaseTool.  Deliberately do not use the
    // transient tool slot: Alt must be a temporary camera override.
    tool_mgr->setCurrentToolset(gBasicToolset);
    gBasicToolset->selectTool(this);
    return true;
}

void ALToolCrowdPlace::stopPlacement(bool cancel_draft, bool restore_tool)
{
    if (mStopping)
    {
        return;
    }
    mStopping = true;

    ALGhostStudio& studio = ALGhostStudio::instance();
    if (cancel_draft)
    {
        if (sessionMatchesDraft())
        {
            mLastMessage = studio.cancelCrowdPlacement(mOwnerId)
                ? "Crowd placement cancelled."
                : "The placement session could not be cancelled.";
        }
        else
        {
            mLastMessage =
                "This placement session is no longer active.";
        }
    }

    if (hasMouseCapture())
    {
        mIgnoreCaptureLoss = true;
        setMouseCapture(false);
        mIgnoreCaptureLoss = false;
    }

    mState = ALGhostInteractionState::State();
    mOwnerId.setNull();
    mSessionId.setNull();

    if (restore_tool)
    {
        LLToolMgr* tool_mgr = LLToolMgr::getInstance();
        if (tool_mgr && tool_mgr->usingTransientTool())
        {
            // The crowd tool is the base tool. A one-shot owned elsewhere may
            // temporarily sit above it; tearing down this placement must not
            // clear that unrelated transient. Keep the saved base selection
            // until the transient naturally leaves and exposes us again.
            mRestorePending = true;
        }
        else
        {
            restoreSavedTool();
            mRestorePending = false;
        }
    }
    else
    {
        mRestorePending = false;
    }
    if (!mRestorePending)
    {
        mSavedToolset = nullptr;
        mSavedBaseTool = nullptr;
    }
    mStopping = false;
}

void ALToolCrowdPlace::restoreSavedTool()
{
    LLToolMgr* tool_mgr = LLToolMgr::getInstance();
    if (!tool_mgr)
    {
        return;
    }

    LLToolset* restore_set = mSavedToolset
        ? mSavedToolset : gBasicToolset;
    LLTool* restore_base = mSavedBaseTool;
    if (!restore_base && restore_set == gBasicToolset)
    {
        restore_base = LLToolPie::getInstance();
    }
    if (!restore_set)
    {
        return;
    }

    // We restore the BASE selection, not whichever Alt override happened to be
    // visible when placement ended.  LLToolMgr may immediately present its
    // camera override while Alt remains down, then naturally return afterward.
    tool_mgr->setCurrentToolset(restore_set);
    if (restore_base)
    {
        restore_set->selectTool(restore_base);
    }
    else
    {
        restore_set->selectFirstTool();
    }
}

bool ALToolCrowdPlace::ensureLiveSession()
{
    if (!ALGhostInteractionState::isActive(mState))
    {
        return false;
    }
    if (!sessionMatchesDraft())
    {
        mLastMessage = "This placement session is no longer active.";
        ALGhostInteractionState::Event lost;
        lost.mType = ALGhostInteractionState::EVENT_TOOL_LOST;
        reduceEvent(lost);
        return false;
    }
    std::string stale_reason;
    if (!ALGhostStudio::instance().crowdPlacementSourceUnchanged(
            &stale_reason))
    {
        mLastMessage = stale_reason;
        ALGhostInteractionState::Event lost;
        lost.mType = ALGhostInteractionState::EVENT_PROTOTYPE_LOST;
        reduceEvent(lost);
        return false;
    }
    return true;
}

ALGhostPlacementResolver::ALGhostPlacementHit
ALToolCrowdPlace::resolveAnchorAt(S32 x, S32 y) const
{
    // Work-plane fallback height: the last authored anchor's Z if we have
    // one yet, otherwise the prototype's own foot Z at the moment placement
    // began. Either way this is a caller-supplied "last-valid/source-foot"
    // height, never a magic constant.
    const ALGhostStudio::CrowdPlacementDraft& draft =
        ALGhostStudio::instance().getCrowdPlacementDraft();
    const F64 work_plane_z = mState.mHasAnchor
        ? mState.mAnchor.mZ : draft.mSource.mFoot.mdV[VZ];

    const ALGhostPlacementResolver::Request request =
        ALGhostPlacementAdapter::screenRequest(x, y, work_plane_z);
    const ALGhostPlacementResolver::Probes probes =
        ALGhostPlacementAdapter::realProbes(x, y);
    const ALGhostPlacementResolver::ALGhostPlacementHit hit =
        ALGhostPlacementResolver::resolve(request, probes);

    if (sessionMatchesDraft())
    {
        ALGhostStudio::instance().setCrowdPlacementAnchorInfo(
            mOwnerId, hit.mBasis, hit.mValid, hit.mWarning);
    }
    return hit;
}

bool ALToolCrowdPlace::projectedNear(const LLVector3d& point_global,
                                     S32 x, S32 y,
                                     F32 radius_pixels) const
{
    LLViewerCamera* camera = LLViewerCamera::getInstance();
    if (!camera || !point_global.isFinite())
    {
        return false;
    }
    LLCoordGL screen;
    const LLVector3 point_agent =
        gAgent.getPosAgentFromGlobal(point_global);
    if (!camera->projectPosAgentToScreen(point_agent, screen, false))
    {
        return false;
    }
    const F32 dx = (F32)screen.mX - (F32)x;
    const F32 dy = (F32)screen.mY - (F32)y;
    return dx * dx + dy * dy <= radius_pixels * radius_pixels;
}

ALToolCrowdPlace::ScreenHit ALToolCrowdPlace::screenHitAt(
    S32 x, S32 y) const
{
    ScreenHit hit;
    if (!sessionMatchesDraft())
    {
        return hit;
    }
    const ALGhostStudio::CrowdPlacementDraft& draft =
        ALGhostStudio::instance().getCrowdPlacementDraft();
    LLVector3d anchor = draft.mAnchor;
    anchor.mdV[VZ] += draft.mHeight;
    hit.mAnchor = projectedNear(anchor, x, y, ANCHOR_PICK_RADIUS_PX);
    hit.mPreview = hit.mAnchor;
    if (!hit.mPreview)
    {
        for (const ALGhostStudio::ResolvedFormationSlot& slot : draft.mSlots)
        {
            if (projectedNear(slot.mFoot, x, y, SLOT_PICK_RADIUS_PX))
            {
                hit.mPreview = true;
                break;
            }
        }
    }
    return hit;
}

ALGhostInteractionState::EventContext ALToolCrowdPlace::eventContext(
    S32 x, S32 y, MASK mask) const
{
    const ScreenHit hit = screenHitAt(x, y);
    ALGhostInteractionState::EventContext context;
    // The viewer dispatches tools only after UI/top controls decline the
    // event, so arrival here is itself the UI-first guarantee.
    context.mUIHandled = false;
    context.mAltDown = (mask & MASK_ALT) != 0;
    context.mHoveringPreview = hit.mPreview;
    context.mActiveAnchorHit = hit.mAnchor;
    context.mFineModifier = (mask & MASK_SHIFT) != 0;
    return context;
}

bool ALToolCrowdPlace::reduceEvent(
    const ALGhostInteractionState::Event& event,
    bool restore_on_terminal)
{
    const ALGhostInteractionState::State before = mState;
    const ALGhostInteractionState::Transition transition =
        ALGhostInteractionState::reduce(mState, event, mConfig);
    mState = transition.mState;
    const ALGhostInteractionState::Commands& command =
        transition.mCommands;

    ALGhostStudio& studio = ALGhostStudio::instance();
    if ((command.mUpdateAnchor ||
         command.mUpdateYaw ||
         command.mUpdateHeight) &&
        sessionMatchesDraft() && mState.mHasAnchor)
    {
        // Always submit one complete authored transform.  In particular,
        // follow-cursor hover changes only the anchor and preserves the exact
        // yaw/height accumulated by the wheel and middle-drag reducers.
        studio.updateCrowdPlacementTransform(
            mOwnerId, globalPoint(mState.mAnchor),
            (F32)mState.mYaw, (F32)mState.mHeight);
    }

    if (!command.mCommit && !command.mCancel &&
        sessionMatchesDraft() &&
        pinnedState(before) != pinnedState(mState))
    {
        studio.setCrowdPlacementPinned(
            mOwnerId, pinnedState(mState));
    }

    if (command.mCaptureMouse && !hasMouseCapture())
    {
        setMouseCapture(true);
    }
    if (command.mReleaseMouse && hasMouseCapture())
    {
        // gFocusMgr invokes onMouseCaptureLost() for an intentional release.
        // The reducer has already restored the resume mode, so suppress the
        // redundant platform-loss event here.
        mIgnoreCaptureLoss = true;
        setMouseCapture(false);
        mIgnoreCaptureLoss = false;
    }

    if (command.mCommit)
    {
        const bool was_pinned = pinnedState(before);
        const ALGhostStudio::PlacementCommitResult result =
            studio.commitCrowdPlacement(mOwnerId, mSessionId);
        mLastCommitSucceeded = result.mSuccess;
        mLastMessage = result.mMessage;
        if (result.mSuccess)
        {
            stopPlacement(false, restore_on_terminal);
        }
        else if (sessionMatchesDraft())
        {
            // A too-small radius or temporary clone allocation failure should
            // not strand the panel with an active draft and an inactive tool.
            restoreReducerAfterFailedCommit(was_pinned);
        }
        else
        {
            stopPlacement(false, restore_on_terminal);
        }
    }
    else if (command.mCancel)
    {
        const bool preserve_loss_reason =
            event.mType == ALGhostInteractionState::EVENT_PROTOTYPE_LOST ||
            event.mType == ALGhostInteractionState::EVENT_TOOL_LOST;
        if (sessionMatchesDraft())
        {
            const bool cancelled = studio.cancelCrowdPlacement(mOwnerId);
            if (!preserve_loss_reason || mLastMessage.empty())
            {
                mLastMessage = cancelled
                    ? "Crowd placement cancelled."
                    : "The placement session could not be cancelled.";
            }
        }
        else if (mLastMessage.empty())
        {
            mLastMessage =
                "This placement session is no longer active.";
        }
        stopPlacement(false, restore_on_terminal);
    }

    return command.mConsume;
}

void ALToolCrowdPlace::restoreReducerAfterFailedCommit(bool pinned)
{
    if (!sessionMatchesDraft())
    {
        return;
    }
    const ALGhostStudio::CrowdPlacementDraft& draft =
        ALGhostStudio::instance().getCrowdPlacementDraft();
    ALGhostInteractionState::Event start;
    start.mType = ALGhostInteractionState::EVENT_START;
    start.mPrototypeAvailable = true;
    start.mHasWorldHit = true;
    start.mWorldHit = worldPoint(draft.mAnchor);
    start.mInitialYaw = draft.mPatternYaw;
    start.mInitialHeight = draft.mHeight;
    mState = ALGhostInteractionState::reduce(
        ALGhostInteractionState::State(), start, mConfig).mState;
    if (pinned)
    {
        ALGhostInteractionState::Event pin;
        pin.mType = ALGhostInteractionState::EVENT_LEFT_CLICK;
        mState = ALGhostInteractionState::reduce(
            mState, pin, mConfig).mState;
    }
    ALGhostStudio::instance().setCrowdPlacementPinned(mOwnerId, pinned);
}

bool ALToolCrowdPlace::handleHover(S32 x, S32 y, MASK mask)
{
    if (!ensureLiveSession())
    {
        return false;
    }

    if (mState.mMode == ALGhostInteractionState::MODE_HEIGHT_DRAG &&
        mState.mOwnsCapture)
    {
        ALGhostInteractionState::Event move;
        move.mType = ALGhostInteractionState::EVENT_MIDDLE_MOVE;
        move.mPointerY = (F64)y;
        const bool consumed = reduceEvent(move);
        gViewerWindow->setCursor(UI_CURSOR_SIZENS);
        return consumed;
    }

    ALGhostInteractionState::Event hover;
    hover.mType = ALGhostInteractionState::EVENT_HOVER_HIT;
    hover.mContext = eventContext(x, y, mask);
    // The ladder always produces a finite candidate anchor; only legality can
    // make it invalid (Blocked). Either way this is a real resolution, never
    // the old render-hit-only "sky miss = no anchor".
    const ALGhostPlacementResolver::ALGhostPlacementHit hit =
        resolveAnchorAt(x, y);
    hover.mHasWorldHit = hit.mValid;
    if (hover.mHasWorldHit)
    {
        hover.mWorldHit = worldPoint(hit.mPointGlobal);
    }
    const bool consumed = reduceEvent(hover);
    gViewerWindow->setCursor(
        mState.mMode == ALGhostInteractionState::MODE_PINNED
            ? UI_CURSOR_CROSS : UI_CURSOR_TOOLCREATE);
    return consumed;
}

void ALToolCrowdPlace::refreshHoverBeforeCommit(S32 x, S32 y, MASK mask)
{
    if (mState.mMode != ALGhostInteractionState::MODE_FOLLOW_CURSOR)
    {
        return;
    }
    ALGhostInteractionState::Event hover;
    hover.mType = ALGhostInteractionState::EVENT_HOVER_HIT;
    hover.mContext = eventContext(x, y, mask);
    const ALGhostPlacementResolver::ALGhostPlacementHit hit =
        resolveAnchorAt(x, y);
    hover.mHasWorldHit = hit.mValid;
    if (hover.mHasWorldHit)
    {
        hover.mWorldHit = worldPoint(hit.mPointGlobal);
    }
    reduceEvent(hover);
}

bool ALToolCrowdPlace::handleMouseDown(S32 x, S32 y, MASK mask)
{
    if (!ensureLiveSession())
    {
        return false;
    }

    // Resolve the click location one last time before pinning.  This prevents
    // a low-frame-rate hover from pinning the previous cursor position.
    refreshHoverBeforeCommit(x, y, mask);

    ALGhostInteractionState::Event click;
    click.mType = ALGhostInteractionState::EVENT_LEFT_CLICK;
    click.mContext = eventContext(x, y, mask);
    return reduceEvent(click);
}

bool ALToolCrowdPlace::handleMiddleMouseDown(
    S32 x, S32 y, MASK mask)
{
    if (!ensureLiveSession())
    {
        return false;
    }
    ALGhostInteractionState::Event begin;
    begin.mType = ALGhostInteractionState::EVENT_MIDDLE_BEGIN;
    begin.mContext = eventContext(x, y, mask);
    begin.mPointerY = (F64)y;
    return reduceEvent(begin);
}

bool ALToolCrowdPlace::handleMiddleMouseUp(
    S32 x, S32 y, MASK mask)
{
    if (!ensureLiveSession())
    {
        return false;
    }
    ALGhostInteractionState::Event end;
    end.mType = ALGhostInteractionState::EVENT_MIDDLE_END;
    end.mContext = eventContext(x, y, mask);
    end.mPointerY = (F64)y;
    return reduceEvent(end);
}

bool ALToolCrowdPlace::handleScrollWheel(
    S32 x, S32 y, LLScrollDelta delta)
{
    if (!ensureLiveSession())
    {
        return false;
    }
    ALGhostInteractionState::Event wheel;
    wheel.mType = ALGhostInteractionState::EVENT_WHEEL;
    wheel.mContext = eventContext(
        x, y, gKeyboard ? gKeyboard->currentMask(true) : MASK_NONE);
    wheel.mDiscreteWheelSteps = delta.mClicks;
    // A malformed nonfinite precise component is still "present": pass it to
    // the pure reducer so it is rejected instead of silently falling back to
    // the discrete click channel.
    wheel.mHasPreciseWheel =
        delta.mPrecise != 0.f ||
        !std::isfinite((double)delta.mPrecise);
    wheel.mPreciseWheelSteps = delta.mPrecise;
    wheel.mWheelSequence = ++mWheelSequence;
    return reduceEvent(wheel);
}

bool ALToolCrowdPlace::handleRightMouseDown(
    S32 x, S32 y, MASK mask)
{
    if (!ensureLiveSession())
    {
        return false;
    }
    ALGhostInteractionState::Event cancel;
    cancel.mType = ALGhostInteractionState::EVENT_CANCEL;
    cancel.mContext = eventContext(x, y, mask);
    return reduceEvent(cancel);
}

bool ALToolCrowdPlace::handleKey(KEY key, MASK mask)
{
    if (!ensureLiveSession())
    {
        return false;
    }
    ALGhostInteractionState::Event event;
    if (key == KEY_RETURN)
    {
        event.mType = ALGhostInteractionState::EVENT_ENTER;
        // KEY events carry no cursor position, so use the viewer's own last
        // known one. Re-resolving here -- exactly like handleMouseDown does
        // before pinning -- is what makes Enter and click genuinely
        // symmetric instead of Enter trusting a cached mCurrentHoverValid
        // that could be stale relative to where the cursor actually is now.
        if (gViewerWindow)
        {
            refreshHoverBeforeCommit(gViewerWindow->getCurrentMouseX(),
                                     gViewerWindow->getCurrentMouseY(), mask);
        }
    }
    else if (key == KEY_ESCAPE)
    {
        event.mType = ALGhostInteractionState::EVENT_CANCEL;
    }
    else
    {
        return false;
    }
    event.mContext.mAltDown = (mask & MASK_ALT) != 0;
    return reduceEvent(event);
}

bool ALToolCrowdPlace::unpin()
{
    if (!ensureLiveSession())
    {
        return false;
    }
    ALGhostInteractionState::Event event;
    event.mType = ALGhostInteractionState::EVENT_UNPIN;
    return reduceEvent(event);
}

bool ALToolCrowdPlace::commitNow()
{
    if (!ensureLiveSession())
    {
        return false;
    }
    mLastCommitSucceeded = false;
    ALGhostInteractionState::Event event;
    event.mType = ALGhostInteractionState::EVENT_ENTER;
    reduceEvent(event);
    return mLastCommitSucceeded;
}

void ALToolCrowdPlace::handleSelect()
{
    if (mRestorePending && !mStopping)
    {
        // An unrelated transient has just cleared, exposing the now-inactive
        // crowd base. Restore beneath that transient only at this safe point.
        // mStopping makes the recursive deselect caused by restoration inert.
        mStopping = true;
        mRestorePending = false;
        restoreSavedTool();
        mSavedToolset = nullptr;
        mSavedBaseTool = nullptr;
        mStopping = false;
        return;
    }
    if (!mStopping)
    {
        ensureLiveSession();
    }
    if (gViewerWindow)
    {
        gViewerWindow->setCursor(
            mState.mMode == ALGhostInteractionState::MODE_PINNED
                ? UI_CURSOR_CROSS : UI_CURSOR_TOOLCREATE);
    }
}

void ALToolCrowdPlace::handleDeselect()
{
    if (mStopping || !ALGhostInteractionState::isActive(mState))
    {
        return;
    }

    LLToolMgr* tool_mgr = LLToolMgr::getInstance();
    if (tool_mgr && tool_mgr->getBaseTool() == this)
    {
        // LLToolMgr calls handleDeselect when Alt exposes LLToolCamera.  The
        // crowd tool is still the base and must remain armed for Alt release.
        return;
    }

    // A genuine base-tool/toolset change is a terminal loss, but restoring the
    // old tool here would fight the user's newly selected tool.
    ALGhostInteractionState::Event lost;
    lost.mType = ALGhostInteractionState::EVENT_TOOL_LOST;
    reduceEvent(lost, false);
}

void ALToolCrowdPlace::onMouseCaptureLost()
{
    if (mIgnoreCaptureLoss || mStopping ||
        !ALGhostInteractionState::isActive(mState))
    {
        return;
    }
    ALGhostInteractionState::Event lost;
    lost.mType = ALGhostInteractionState::EVENT_CAPTURE_LOST;
    reduceEvent(lost);
}

void ALToolCrowdPlace::draw()
{
    // Per-frame lifecycle check while the crowd tool is current.  This makes
    // prototype removal terminate safely even with a stationary pointer.
    if (!mStopping)
    {
        ensureLiveSession();
    }
}

LLTool* ALToolCrowdPlace::getOverrideTool(MASK mask)
{
    // Mouse capture makes LLToolMgr keep this tool before asking for an
    // override.  For every uncaptured Alt combination, camera navigation wins
    // regardless of the optional EnableAltZoom preference.
    if (!hasMouseCapture() && (mask & MASK_ALT))
    {
        return LLToolCamera::getInstance();
    }
    return nullptr;
}
