/**
 * @file alghostinteractionstate.h
 * @brief Pure state reducer for interactive Ghost Studio crowd placement.
 *
 * The source code in this file is provided to you under the terms of the
 * GNU Lesser General Public License, version 2.1, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. Terms of the LGPL can be found in doc/LGPL-licence.txt
 * in this distribution, or online at http://www.gnu.org/licenses/lgpl-2.1.txt
 *
 */
#ifndef AL_GHOSTINTERACTIONSTATE_H
#define AL_GHOSTINTERACTIONSTATE_H

#include <cstdint>

namespace ALGhostInteractionState
{
    enum EMode : std::uint8_t
    {
        MODE_INACTIVE = 0,
        MODE_FOLLOW_CURSOR,
        MODE_PINNED,
        MODE_HEIGHT_DRAG
    };

    enum EEventType : std::uint8_t
    {
        EVENT_START = 0,
        EVENT_HOVER_HIT,
        EVENT_LEFT_CLICK,
        EVENT_WHEEL,
        EVENT_MIDDLE_BEGIN,
        EVENT_MIDDLE_MOVE,
        EVENT_MIDDLE_END,
        EVENT_ENTER,
        EVENT_UNPIN,
        EVENT_CANCEL,
        EVENT_CAPTURE_LOST,
        EVENT_TOOL_LOST,
        EVENT_PROTOTYPE_LOST
    };

    struct WorldPoint
    {
        double mX = 0.0;
        double mY = 0.0;
        double mZ = 0.0;
    };

    // Facts resolved before the tool sees an input event. mUIHandled enforces
    // UI-first dispatch. Alt always belongs to the camera for uncaptured input.
    // A wheel or middle press may start placement work only over its preview or
    // an explicit hit on the currently active anchor.
    struct EventContext
    {
        bool mUIHandled       = false;
        bool mAltDown         = false;
        bool mHoveringPreview = false;
        bool mActiveAnchorHit = false;
        bool mFineModifier    = false;
    };

    struct Event
    {
        EEventType  mType = EVENT_HOVER_HIT;
        EventContext mContext;

        // START/HOVER_HIT: a valid world hit is an anchor candidate. START also
        // initializes yaw/height. A start without a hit is valid and waits for
        // the first finite hover hit.
        bool       mPrototypeAvailable = true;
        bool       mHasWorldHit = false;
        WorldPoint mWorldHit;
        double     mInitialYaw = 0.0;
        double     mInitialHeight = 0.0;

        // WHEEL: if precise data is present it is authoritative and discrete
        // steps are ignored. A non-zero sequence lets adapters coalesce two
        // callbacks for one physical gesture; the second is consumed but never
        // applied again.
        int           mDiscreteWheelSteps = 0;
        bool          mHasPreciseWheel = false;
        double        mPreciseWheelSteps = 0.0;
        std::uint64_t mWheelSequence = 0;

        // MIDDLE_BEGIN/MIDDLE_MOVE: current pointer Y in screen coordinates.
        // The reducer derives height from the begin position, making duplicate
        // move events at the same coordinate idempotent.
        double mPointerY = 0.0;
    };

    struct Config
    {
        double mRadiansPerWheelStep = 0.08726646259971647; // five degrees
        double mFineWheelScale = 0.2;
        double mHeightUnitsPerPixel = 0.02;
    };

    struct State
    {
        EMode      mMode = MODE_INACTIVE;
        WorldPoint mAnchor;
        bool       mHasAnchor = false;
        bool       mCurrentHoverValid = false;
        double     mYaw = 0.0;
        double     mHeight = 0.0;

        // Valid only in MODE_HEIGHT_DRAG. mResumeMode is the exact mode to
        // restore on release/capture loss, so a pinned placement stays pinned.
        EMode  mResumeMode = MODE_FOLLOW_CURSOR;
        bool   mOwnsCapture = false;
        double mDragOriginY = 0.0;
        double mDragOriginHeight = 0.0;
        double mDragUnitsPerPixel = 0.0;

        std::uint64_t mLastWheelSequence = 0;
    };

    // One-shot effects for the caller. The next State never stores commit or
    // cancel pulses, so replaying terminal/loss events after the transition is
    // harmless.
    struct Commands
    {
        bool mConsume = false;

        bool       mUpdateAnchor = false;
        WorldPoint mAnchor;
        bool       mUpdateYaw = false;
        double     mYaw = 0.0;
        bool       mUpdateHeight = false;
        double     mHeight = 0.0;

        bool mCaptureMouse = false;
        bool mReleaseMouse = false;
        bool mCommit = false;
        bool mCancel = false;
    };

    struct Transition
    {
        State    mState;
        Commands mCommands;
    };

    bool isActive(const State& state);
    Transition reduce(const State& state, const Event& event,
                      const Config& config = Config());
}

#endif // AL_GHOSTINTERACTIONSTATE_H
