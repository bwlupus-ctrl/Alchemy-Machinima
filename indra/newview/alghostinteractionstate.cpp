/**
 * @file alghostinteractionstate.cpp
 * @brief Pure state reducer for interactive Ghost Studio crowd placement.
 *
 * The source code in this file is provided to you under the terms of the
 * GNU Lesser General Public License, version 2.1, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. Terms of the LGPL can be found in doc/LGPL-licence.txt
 * in this distribution, or online at http://www.gnu.org/licenses/lgpl-2.1.txt
 *
 */

#include "linden_common.h"

#include "alghostinteractionstate.h"

#include <cmath>

namespace ALGhostInteractionState
{
namespace
{
    constexpr double PI = 3.1415926535897932384626433832795;
    constexpr double TWO_PI = PI * 2.0;
    constexpr double CHANGE_EPSILON = 1.0e-12;

    bool finitePoint(const WorldPoint& point)
    {
        return std::isfinite(point.mX) &&
               std::isfinite(point.mY) &&
               std::isfinite(point.mZ);
    }

    bool samePoint(const WorldPoint& lhs, const WorldPoint& rhs)
    {
        return lhs.mX == rhs.mX &&
               lhs.mY == rhs.mY &&
               lhs.mZ == rhs.mZ;
    }

    double normalizedYaw(double yaw)
    {
        if (!std::isfinite(yaw))
        {
            return 0.0;
        }
        yaw = std::fmod(yaw + PI, TWO_PI);
        if (yaw < 0.0)
        {
            yaw += TWO_PI;
        }
        return yaw - PI;
    }

    EMode safeResumeMode(EMode mode)
    {
        return mode == MODE_PINNED ? MODE_PINNED : MODE_FOLLOW_CURSOR;
    }

    bool validPlacement(const State& state)
    {
        return state.mHasAnchor &&
               finitePoint(state.mAnchor) &&
               std::isfinite(state.mYaw) &&
               std::isfinite(state.mHeight);
    }

    void finish(Transition& transition, bool commit, bool consume)
    {
        transition.mCommands.mConsume = consume;
        transition.mCommands.mCommit = commit;
        transition.mCommands.mCancel = !commit;
        transition.mCommands.mReleaseMouse =
            transition.mState.mMode == MODE_HEIGHT_DRAG &&
            transition.mState.mOwnsCapture;
        transition.mState = State();
    }

    bool altDefersEvent(EEventType type)
    {
        switch (type)
        {
        case EVENT_HOVER_HIT:
        case EVENT_LEFT_CLICK:
        case EVENT_WHEEL:
        case EVENT_MIDDLE_BEGIN:
            return true;
        default:
            return false;
        }
    }
}

bool isActive(const State& state)
{
    return state.mMode != MODE_INACTIVE;
}

Transition reduce(const State& state, const Event& event, const Config& config)
{
    Transition transition;
    transition.mState = state;

    // START is a semantic command, not an unhandled world input. Repeating it
    // while already active cannot reset or duplicate an in-progress gesture.
    if (event.mType == EVENT_START)
    {
        if (isActive(state) || !event.mPrototypeAvailable)
        {
            return transition;
        }

        transition.mState = State();
        transition.mState.mMode = MODE_FOLLOW_CURSOR;
        transition.mState.mYaw = normalizedYaw(event.mInitialYaw);
        transition.mState.mHeight = std::isfinite(event.mInitialHeight)
            ? event.mInitialHeight : 0.0;
        transition.mCommands.mUpdateYaw = true;
        transition.mCommands.mYaw = transition.mState.mYaw;
        transition.mCommands.mUpdateHeight = true;
        transition.mCommands.mHeight = transition.mState.mHeight;

        if (event.mHasWorldHit && finitePoint(event.mWorldHit))
        {
            transition.mState.mAnchor = event.mWorldHit;
            transition.mState.mHasAnchor = true;
            transition.mState.mCurrentHoverValid = true;
            transition.mCommands.mUpdateAnchor = true;
            transition.mCommands.mAnchor = event.mWorldHit;
        }
        return transition;
    }

    // Lifecycle loss is authoritative even if UI happened to handle the input
    // event that exposed it. It cancels once and releases only capture we still
    // believe we own.
    if (event.mType == EVENT_TOOL_LOST ||
        event.mType == EVENT_PROTOTYPE_LOST)
    {
        if (isActive(state))
        {
            finish(transition, false, false);
        }
        return transition;
    }

    // Capture loss means the platform already released the mouse. Keep the
    // authored height and resume the pre-drag placement mode without emitting a
    // second release or cancelling the whole placement.
    if (event.mType == EVENT_CAPTURE_LOST)
    {
        if (state.mMode == MODE_HEIGHT_DRAG)
        {
            transition.mState.mMode = safeResumeMode(state.mResumeMode);
            transition.mState.mOwnsCapture = false;
            transition.mState.mDragOriginY = 0.0;
            transition.mState.mDragOriginHeight = 0.0;
            transition.mState.mDragUnitsPerPixel = 0.0;
        }
        return transition;
    }

    // Once a height drag owns capture, modifier/UI changes cannot strand it.
    // Move derives an absolute height from the begin point, so duplicate move
    // delivery does not accumulate drift.
    if (state.mMode == MODE_HEIGHT_DRAG && state.mOwnsCapture)
    {
        if (event.mType == EVENT_MIDDLE_MOVE)
        {
            transition.mCommands.mConsume = true;
            if (!std::isfinite(event.mPointerY) ||
                !std::isfinite(state.mDragOriginY) ||
                !std::isfinite(state.mDragOriginHeight) ||
                !std::isfinite(state.mDragUnitsPerPixel) ||
                state.mDragUnitsPerPixel <= 0.0)
            {
                return transition;
            }
            const double height = state.mDragOriginHeight +
                (state.mDragOriginY - event.mPointerY) *
                state.mDragUnitsPerPixel;
            if (!std::isfinite(height) ||
                std::fabs(height - state.mHeight) <= CHANGE_EPSILON)
            {
                return transition;
            }
            transition.mState.mHeight = height;
            transition.mCommands.mUpdateHeight = true;
            transition.mCommands.mHeight = height;
            return transition;
        }
        if (event.mType == EVENT_MIDDLE_END)
        {
            transition.mCommands.mConsume = true;
            transition.mCommands.mReleaseMouse = true;
            transition.mState.mMode = safeResumeMode(state.mResumeMode);
            transition.mState.mOwnsCapture = false;
            transition.mState.mDragOriginY = 0.0;
            transition.mState.mDragOriginHeight = 0.0;
            transition.mState.mDragUnitsPerPixel = 0.0;
            return transition;
        }
    }

    // UI gets first refusal. Alt-modified placement pointer gestures always
    // defer to the camera. Neither rule applies to an already-captured drag,
    // which returned through the block above.
    if (event.mContext.mUIHandled)
    {
        return transition;
    }
    if (event.mContext.mAltDown && altDefersEvent(event.mType))
    {
        return transition;
    }

    switch (event.mType)
    {
    case EVENT_HOVER_HIT:
        if (state.mMode != MODE_FOLLOW_CURSOR)
        {
            break;
        }
        transition.mCommands.mConsume = true;
        if (!event.mHasWorldHit || !finitePoint(event.mWorldHit))
        {
            transition.mState.mCurrentHoverValid = false;
            break;
        }
        transition.mState.mCurrentHoverValid = true;
        if (!state.mHasAnchor || !samePoint(state.mAnchor, event.mWorldHit))
        {
            transition.mState.mAnchor = event.mWorldHit;
            transition.mState.mHasAnchor = true;
            transition.mCommands.mUpdateAnchor = true;
            transition.mCommands.mAnchor = event.mWorldHit;
        }
        break;

    case EVENT_LEFT_CLICK:
        if (!isActive(state))
        {
            break;
        }
        transition.mCommands.mConsume = true;
        if (state.mMode == MODE_FOLLOW_CURSOR)
        {
            if (state.mCurrentHoverValid && validPlacement(state))
            {
                transition.mState.mMode = MODE_PINNED;
            }
        }
        else if (state.mMode == MODE_PINNED && validPlacement(state))
        {
            finish(transition, true, true);
        }
        break;

    case EVENT_WHEEL:
    {
        if ((state.mMode != MODE_FOLLOW_CURSOR &&
             state.mMode != MODE_PINNED) ||
            !validPlacement(state) ||
            (!event.mContext.mHoveringPreview &&
             !event.mContext.mActiveAnchorHit))
        {
            break;
        }

        // A gesture sequence can arrive through both a discrete and a precise
        // adapter. The first accepted callback owns the sequence; later copies
        // are consumed without applying another rotation.
        if (event.mWheelSequence != 0 &&
            event.mWheelSequence == state.mLastWheelSequence)
        {
            transition.mCommands.mConsume = true;
            break;
        }

        const double steps = event.mHasPreciseWheel
            ? event.mPreciseWheelSteps
            : static_cast<double>(event.mDiscreteWheelSteps);
        const double fine_scale = event.mContext.mFineModifier
            ? config.mFineWheelScale : 1.0;
        if (!std::isfinite(steps) || steps == 0.0 ||
            !std::isfinite(config.mRadiansPerWheelStep) ||
            !std::isfinite(fine_scale) || fine_scale <= 0.0)
        {
            break;
        }

        const double yaw = normalizedYaw(state.mYaw +
            steps * config.mRadiansPerWheelStep * fine_scale);
        transition.mCommands.mConsume = true;
        if (event.mWheelSequence != 0)
        {
            transition.mState.mLastWheelSequence = event.mWheelSequence;
        }
        if (std::fabs(yaw - state.mYaw) > CHANGE_EPSILON)
        {
            transition.mState.mYaw = yaw;
            transition.mCommands.mUpdateYaw = true;
            transition.mCommands.mYaw = yaw;
        }
        break;
    }

    case EVENT_MIDDLE_BEGIN:
        if ((state.mMode != MODE_FOLLOW_CURSOR &&
             state.mMode != MODE_PINNED) ||
            !validPlacement(state) ||
            (!event.mContext.mHoveringPreview &&
             !event.mContext.mActiveAnchorHit) ||
            !std::isfinite(event.mPointerY) ||
            !std::isfinite(config.mHeightUnitsPerPixel) ||
            config.mHeightUnitsPerPixel <= 0.0)
        {
            break;
        }
        transition.mState.mResumeMode = state.mMode;
        transition.mState.mMode = MODE_HEIGHT_DRAG;
        transition.mState.mOwnsCapture = true;
        transition.mState.mDragOriginY = event.mPointerY;
        transition.mState.mDragOriginHeight = state.mHeight;
        transition.mState.mDragUnitsPerPixel =
            config.mHeightUnitsPerPixel;
        transition.mCommands.mConsume = true;
        transition.mCommands.mCaptureMouse = true;
        break;

    case EVENT_MIDDLE_MOVE:
    case EVENT_MIDDLE_END:
        // Idempotent after release/capture loss, or malformed without capture.
        break;

    case EVENT_ENTER:
        if (isActive(state))
        {
            transition.mCommands.mConsume = true;
            // FOLLOW_CURSOR must gate on the SAME "is the current hover
            // valid" check EVENT_LEFT_CLICK uses to pin. Previously Enter
            // only checked validPlacement(), which stays true across a
            // hover miss (the last-valid anchor is deliberately preserved
            // for display -- see EVENT_HOVER_HIT/samePoint above). That let
            // Enter commit a STALE previous anchor at the exact moment a
            // click could not even pin one: inconsistent, and effectively a
            // silent teleport to wherever the cursor used to be. PINNED/
            // HEIGHT_DRAG already locked in a valid anchor at click time, so
            // they are unaffected.
            const bool committable = state.mMode == MODE_FOLLOW_CURSOR
                ? (state.mCurrentHoverValid && validPlacement(state))
                : validPlacement(state);
            if (committable)
            {
                finish(transition, true, true);
            }
        }
        break;

    case EVENT_UNPIN:
        if (state.mMode == MODE_PINNED)
        {
            transition.mState.mMode = MODE_FOLLOW_CURSOR;
            transition.mState.mCurrentHoverValid = false;
            transition.mCommands.mConsume = true;
        }
        break;

    case EVENT_CANCEL:
        if (isActive(state))
        {
            finish(transition, false, true);
        }
        break;

    case EVENT_CAPTURE_LOST:
    case EVENT_TOOL_LOST:
    case EVENT_PROTOTYPE_LOST:
    case EVENT_START:
        // Handled before UI/Alt dispatch.
        break;
    }

    return transition;
}

} // namespace ALGhostInteractionState
