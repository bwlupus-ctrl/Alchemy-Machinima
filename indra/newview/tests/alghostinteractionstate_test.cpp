/**
 * @file alghostinteractionstate_test.cpp
 * @brief Adversarial tests for the Ghost Studio placement interaction reducer.
 *
 * The source code in this file is provided to you under the terms of the
 * GNU Lesser General Public License, version 2.1, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. Terms of the LGPL can be found in doc/LGPL-licence.txt
 * in this distribution, or online at http://www.gnu.org/licenses/lgpl-2.1.txt
 *
 */

#include "linden_common.h"

#include "../test/lltut.h"

#include "../alghostinteractionstate.h"

#include <cmath>
#include <limits>

namespace tut
{
    using namespace ALGhostInteractionState;

    namespace
    {
        constexpr double EPSILON = 1.0e-9;

        WorldPoint point(double x, double y, double z)
        {
            WorldPoint result;
            result.mX = x;
            result.mY = y;
            result.mZ = z;
            return result;
        }

        Event event(EEventType type)
        {
            Event result;
            result.mType = type;
            return result;
        }

        State followState()
        {
            Event start = event(EVENT_START);
            start.mHasWorldHit = true;
            start.mWorldHit = point(10.0, 20.0, 30.0);
            return reduce(State(), start).mState;
        }

        State pinnedState()
        {
            State follow = followState();
            return reduce(follow, event(EVENT_LEFT_CLICK)).mState;
        }

        bool nearlyEqual(double lhs, double rhs)
        {
            return std::fabs(lhs - rhs) <= EPSILON;
        }

        bool samePoint(const WorldPoint& lhs, const WorldPoint& rhs)
        {
            return lhs.mX == rhs.mX &&
                   lhs.mY == rhs.mY &&
                   lhs.mZ == rhs.mZ;
        }
    }

    struct interaction_data
    {
    };

    typedef test_group<interaction_data> interaction_group;
    typedef interaction_group::object    interaction_object;
    tut::interaction_group interaction_tests("ALGhostInteractionState");

    // Start initializes once, accepts a start without a hit, and cannot reset an
    // active interaction when the semantic command is replayed.
    template<> template<>
    void interaction_object::test<1>()
    {
        Event start = event(EVENT_START);
        start.mHasWorldHit = true;
        start.mWorldHit = point(1.0, 2.0, 3.0);
        start.mInitialYaw = 4.0 * 3.14159265358979323846 + 0.25;
        start.mInitialHeight = 2.5;

        Transition first = reduce(State(), start);
        ensure("start enters follow", first.mState.mMode == MODE_FOLLOW_CURSOR);
        ensure("start publishes anchor", first.mCommands.mUpdateAnchor);
        ensure("start publishes yaw", first.mCommands.mUpdateYaw);
        ensure("start publishes height", first.mCommands.mUpdateHeight);
        ensure("start anchor stored", samePoint(first.mState.mAnchor, start.mWorldHit));
        ensure("start yaw normalized", nearlyEqual(first.mState.mYaw, 0.25));
        ensure("start height stored", nearlyEqual(first.mState.mHeight, 2.5));

        Transition replay = reduce(first.mState, start);
        ensure("replayed start keeps mode", replay.mState.mMode == MODE_FOLLOW_CURSOR);
        ensure("replayed start does not publish anchor", !replay.mCommands.mUpdateAnchor);
        ensure("replayed start does not publish yaw", !replay.mCommands.mUpdateYaw);
        ensure("replayed start does not publish height", !replay.mCommands.mUpdateHeight);

        Event unavailable = event(EVENT_START);
        unavailable.mPrototypeAvailable = false;
        Transition refused = reduce(State(), unavailable);
        ensure("missing prototype cannot start", refused.mState.mMode == MODE_INACTIVE);

        Event waiting = event(EVENT_START);
        Transition no_hit = reduce(State(), waiting);
        ensure("start may wait for a hit", no_hit.mState.mMode == MODE_FOLLOW_CURSOR);
        ensure("waiting start has no anchor", !no_hit.mState.mHasAnchor);
    }

    // Follow tracks only finite current hits. A miss invalidates pinning without
    // destroying the last visible anchor; the same hit restores pin eligibility
    // without emitting a redundant anchor update.
    template<> template<>
    void interaction_object::test<2>()
    {
        State state = followState();
        Event hover = event(EVENT_HOVER_HIT);
        hover.mHasWorldHit = true;
        hover.mWorldHit = point(11.0, 22.0, 33.0);
        Transition moved = reduce(state, hover);
        ensure("hover is consumed", moved.mCommands.mConsume);
        ensure("new hit updates anchor", moved.mCommands.mUpdateAnchor);

        Event miss = event(EVENT_HOVER_HIT);
        Transition missed = reduce(moved.mState, miss);
        ensure("miss remains tool-owned", missed.mCommands.mConsume);
        ensure("miss invalidates current hit", !missed.mState.mCurrentHoverValid);
        ensure("miss preserves last anchor", samePoint(missed.mState.mAnchor, hover.mWorldHit));

        Transition stale_click = reduce(missed.mState, event(EVENT_LEFT_CLICK));
        ensure("stale click is consumed", stale_click.mCommands.mConsume);
        ensure("stale click cannot pin", stale_click.mState.mMode == MODE_FOLLOW_CURSOR);

        Transition restored = reduce(missed.mState, hover);
        ensure("same anchor need not republish", !restored.mCommands.mUpdateAnchor);
        ensure("same anchor restores valid hover", restored.mState.mCurrentHoverValid);
        Transition pinned = reduce(restored.mState, event(EVENT_LEFT_CLICK));
        ensure("valid first click pins", pinned.mState.mMode == MODE_PINNED);
    }

    // Left click is pin-then-confirm. Confirmation is a one-shot pulse and a
    // replay against the resulting inactive state cannot commit twice.
    template<> template<>
    void interaction_object::test<3>()
    {
        State follow = followState();
        Transition pin = reduce(follow, event(EVENT_LEFT_CLICK));
        ensure("first click pins", pin.mState.mMode == MODE_PINNED);
        ensure("first click does not commit", !pin.mCommands.mCommit);

        Transition confirm = reduce(pin.mState, event(EVENT_LEFT_CLICK));
        ensure("second click commits", confirm.mCommands.mCommit);
        ensure("commit is not cancel", !confirm.mCommands.mCancel);
        ensure("commit returns inactive", confirm.mState.mMode == MODE_INACTIVE);

        Transition replay = reduce(confirm.mState, event(EVENT_LEFT_CLICK));
        ensure("inactive click falls through", !replay.mCommands.mConsume);
        ensure("inactive click cannot recommit", !replay.mCommands.mCommit);
    }

    // UI-first input and Alt-modified pointer gestures never mutate or consume
    // placement state. In particular, Alt cannot pin, rotate, or capture MMB.
    template<> template<>
    void interaction_object::test<4>()
    {
        State follow = followState();

        Event ui_hover = event(EVENT_HOVER_HIT);
        ui_hover.mContext.mUIHandled = true;
        ui_hover.mHasWorldHit = true;
        ui_hover.mWorldHit = point(99.0, 99.0, 99.0);
        Transition ui = reduce(follow, ui_hover);
        ensure("UI-first hover falls through", !ui.mCommands.mConsume);
        ensure("UI-first hover preserves anchor", samePoint(ui.mState.mAnchor, follow.mAnchor));

        Event alt_click = event(EVENT_LEFT_CLICK);
        alt_click.mContext.mAltDown = true;
        Transition click = reduce(follow, alt_click);
        ensure("Alt click falls through", !click.mCommands.mConsume);
        ensure("Alt click cannot pin", click.mState.mMode == MODE_FOLLOW_CURSOR);

        Event alt_wheel = event(EVENT_WHEEL);
        alt_wheel.mContext.mAltDown = true;
        alt_wheel.mContext.mHoveringPreview = true;
        alt_wheel.mDiscreteWheelSteps = 1;
        Transition wheel = reduce(follow, alt_wheel);
        ensure("Alt wheel falls through", !wheel.mCommands.mConsume);
        ensure("Alt wheel cannot rotate", !wheel.mCommands.mUpdateYaw);

        Event alt_middle = event(EVENT_MIDDLE_BEGIN);
        alt_middle.mContext.mAltDown = true;
        alt_middle.mContext.mActiveAnchorHit = true;
        alt_middle.mPointerY = 100.0;
        Transition middle = reduce(follow, alt_middle);
        ensure("Alt MMB falls through", !middle.mCommands.mConsume);
        ensure("Alt MMB cannot capture", !middle.mCommands.mCaptureMouse);
        ensure("Alt MMB keeps follow", middle.mState.mMode == MODE_FOLLOW_CURSOR);
    }

    // Precise wheel is authoritative when both channels are present. Sequence
    // replay cannot double-apply, and fine adjustment scales exactly once.
    template<> template<>
    void interaction_object::test<5>()
    {
        Config config;
        config.mRadiansPerWheelStep = 0.1;
        config.mFineWheelScale = 0.2;
        State state = followState();

        Event wheel = event(EVENT_WHEEL);
        wheel.mContext.mHoveringPreview = true;
        wheel.mDiscreteWheelSteps = 4;
        wheel.mHasPreciseWheel = true;
        wheel.mPreciseWheelSteps = 0.5;
        wheel.mWheelSequence = 71;
        Transition precise = reduce(state, wheel, config);
        ensure("wheel consumed over preview", precise.mCommands.mConsume);
        ensure("wheel publishes yaw", precise.mCommands.mUpdateYaw);
        ensure("precise wins over discrete", nearlyEqual(precise.mState.mYaw, 0.05));

        Event duplicate = wheel;
        duplicate.mHasPreciseWheel = false;
        duplicate.mDiscreteWheelSteps = 4;
        Transition replay = reduce(precise.mState, duplicate, config);
        ensure("duplicate callback remains consumed", replay.mCommands.mConsume);
        ensure("duplicate callback does not update yaw", !replay.mCommands.mUpdateYaw);
        ensure("duplicate callback does not rotate", nearlyEqual(replay.mState.mYaw, 0.05));

        Event fine = wheel;
        fine.mWheelSequence = 72;
        fine.mPreciseWheelSteps = 1.0;
        fine.mContext.mFineModifier = true;
        Transition adjusted = reduce(replay.mState, fine, config);
        ensure("fine wheel scales once", nearlyEqual(adjusted.mState.mYaw, 0.07));

        Event unsequenced = wheel;
        unsequenced.mWheelSequence = 0;
        unsequenced.mPreciseWheelSteps = 0.5;
        Transition without_sequence =
            reduce(adjusted.mState, unsequenced, config);
        ensure("unsequenced wheel still applies",
               nearlyEqual(without_sequence.mState.mYaw, 0.12));
        Transition late_duplicate =
            reduce(without_sequence.mState, fine, config);
        ensure("unsequenced wheel does not erase dedupe history",
               !late_duplicate.mCommands.mUpdateYaw);
        ensure("late duplicate cannot rotate",
               nearlyEqual(late_duplicate.mState.mYaw, 0.12));

        Event away = wheel;
        away.mWheelSequence = 73;
        away.mContext.mHoveringPreview = false;
        Transition not_owned = reduce(late_duplicate.mState, away, config);
        ensure("wheel away from placement falls through", !not_owned.mCommands.mConsume);
        ensure("wheel away cannot rotate", !not_owned.mCommands.mUpdateYaw);

        Event zero_precise = wheel;
        zero_precise.mWheelSequence = 74;
        zero_precise.mPreciseWheelSteps = 0.0;
        Transition zero = reduce(late_duplicate.mState, zero_precise, config);
        ensure("zero precise does not fall back to discrete", !zero.mCommands.mUpdateYaw);
    }

    // Height drag starts only on the active placement, uses absolute movement,
    // keeps ownership if Alt/UI context changes, and releases exactly once.
    template<> template<>
    void interaction_object::test<6>()
    {
        Config config;
        config.mHeightUnitsPerPixel = 0.1;
        State follow = followState();

        Event miss = event(EVENT_MIDDLE_BEGIN);
        miss.mPointerY = 100.0;
        Transition refused = reduce(follow, miss, config);
        ensure("MMB away falls through", !refused.mCommands.mConsume);
        ensure("MMB away cannot capture", !refused.mCommands.mCaptureMouse);

        Event begin = miss;
        begin.mContext.mActiveAnchorHit = true;
        Transition captured = reduce(follow, begin, config);
        ensure("anchor MMB enters height drag", captured.mState.mMode == MODE_HEIGHT_DRAG);
        ensure("anchor MMB captures", captured.mCommands.mCaptureMouse);

        Transition duplicate_begin = reduce(captured.mState, begin, config);
        ensure("duplicate begin does not recapture", !duplicate_begin.mCommands.mCaptureMouse);

        Event move = event(EVENT_MIDDLE_MOVE);
        move.mPointerY = 90.0;
        move.mContext.mAltDown = true;
        move.mContext.mUIHandled = true;
        Transition raised = reduce(captured.mState, move, config);
        ensure("captured move remains consumed", raised.mCommands.mConsume);
        ensure("Alt cannot steal existing capture", raised.mCommands.mUpdateHeight);
        ensure("drag raises absolutely", nearlyEqual(raised.mState.mHeight, 1.0));

        Config changed_config = config;
        changed_config.mHeightUnitsPerPixel = 10.0;
        Transition duplicate_move =
            reduce(raised.mState, move, changed_config);
        ensure("same move remains consumed", duplicate_move.mCommands.mConsume);
        ensure("same move is idempotent", !duplicate_move.mCommands.mUpdateHeight);
        ensure("mid-drag config cannot change scale",
               nearlyEqual(duplicate_move.mState.mHeight, 1.0));

        Event end = event(EVENT_MIDDLE_END);
        end.mContext.mAltDown = true;
        Transition released = reduce(duplicate_move.mState, end, config);
        ensure("end releases mouse", released.mCommands.mReleaseMouse);
        ensure("end resumes follow", released.mState.mMode == MODE_FOLLOW_CURSOR);
        ensure("end preserves height", nearlyEqual(released.mState.mHeight, 1.0));

        Transition replay_end = reduce(released.mState, end, config);
        ensure("replayed end does not release twice", !replay_end.mCommands.mReleaseMouse);
    }

    // Pinned height drag resumes pinned. Unexpected capture loss does not emit
    // release/cancel, and repeated loss is harmless.
    template<> template<>
    void interaction_object::test<7>()
    {
        State pinned = pinnedState();
        Event begin = event(EVENT_MIDDLE_BEGIN);
        begin.mContext.mHoveringPreview = true;
        begin.mPointerY = 50.0;
        Transition captured = reduce(pinned, begin);
        ensure("pinned drag captures", captured.mCommands.mCaptureMouse);
        ensure("pinned drag remembers resume mode",
               captured.mState.mResumeMode == MODE_PINNED);

        Transition lost = reduce(captured.mState, event(EVENT_CAPTURE_LOST));
        ensure("capture loss resumes pinned", lost.mState.mMode == MODE_PINNED);
        ensure("capture loss knows capture is gone", !lost.mState.mOwnsCapture);
        ensure("capture loss does not release twice", !lost.mCommands.mReleaseMouse);
        ensure("capture loss does not cancel", !lost.mCommands.mCancel);

        Transition replay = reduce(lost.mState, event(EVENT_CAPTURE_LOST));
        ensure("replayed loss keeps pinned", replay.mState.mMode == MODE_PINNED);
        ensure("replayed loss has no release", !replay.mCommands.mReleaseMouse);

        Transition unpinned = reduce(replay.mState, event(EVENT_UNPIN));
        ensure("unpin resumes follow", unpinned.mState.mMode == MODE_FOLLOW_CURSOR);
        ensure("unpin requires a fresh hover", !unpinned.mState.mCurrentHoverValid);
    }

    // Enter confirms from follow, pinned, or an owned height drag. The latter
    // releases capture in the same terminal transition.
    template<> template<>
    void interaction_object::test<8>()
    {
        Transition follow_commit = reduce(followState(), event(EVENT_ENTER));
        ensure("Enter commits follow preview", follow_commit.mCommands.mCommit);
        ensure("Enter consumes active placement", follow_commit.mCommands.mConsume);

        Transition pinned_commit = reduce(pinnedState(), event(EVENT_ENTER));
        ensure("Enter commits pinned preview", pinned_commit.mCommands.mCommit);

        Event begin = event(EVENT_MIDDLE_BEGIN);
        begin.mContext.mHoveringPreview = true;
        begin.mPointerY = 20.0;
        State dragging = reduce(pinnedState(), begin).mState;
        Transition drag_commit = reduce(dragging, event(EVENT_ENTER));
        ensure("Enter commits height drag", drag_commit.mCommands.mCommit);
        ensure("Enter releases height capture", drag_commit.mCommands.mReleaseMouse);
        ensure("Enter ends interaction", drag_commit.mState.mMode == MODE_INACTIVE);
    }

    // Cancel/tool/prototype loss are one-shot cancellation paths. Only a path
    // that still owns capture asks the adapter to release it.
    template<> template<>
    void interaction_object::test<9>()
    {
        Transition cancelled = reduce(followState(), event(EVENT_CANCEL));
        ensure("cancel pulses once", cancelled.mCommands.mCancel);
        ensure("cancel is consumed", cancelled.mCommands.mConsume);
        ensure("cancel returns inactive", cancelled.mState.mMode == MODE_INACTIVE);
        Transition replay = reduce(cancelled.mState, event(EVENT_CANCEL));
        ensure("replayed cancel has no pulse", !replay.mCommands.mCancel);

        Transition tool_lost = reduce(pinnedState(), event(EVENT_TOOL_LOST));
        ensure("tool loss cancels", tool_lost.mCommands.mCancel);
        ensure("system loss is not input-consumed", !tool_lost.mCommands.mConsume);

        Event begin = event(EVENT_MIDDLE_BEGIN);
        begin.mContext.mHoveringPreview = true;
        begin.mPointerY = 10.0;
        State dragging = reduce(followState(), begin).mState;
        Transition prototype_lost =
            reduce(dragging, event(EVENT_PROTOTYPE_LOST));
        ensure("prototype loss cancels", prototype_lost.mCommands.mCancel);
        ensure("prototype loss releases owned capture",
               prototype_lost.mCommands.mReleaseMouse);
        Transition loss_replay =
            reduce(prototype_lost.mState, event(EVENT_PROTOTYPE_LOST));
        ensure("replayed prototype loss cannot recancel",
               !loss_replay.mCommands.mCancel);
        ensure("replayed prototype loss cannot rerelease",
               !loss_replay.mCommands.mReleaseMouse);
    }

    // Non-finite event/config data cannot enter persistent state, produce a
    // commit, or exploit the discrete fallback when precise data is malformed.
    template<> template<>
    void interaction_object::test<10>()
    {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const double inf = std::numeric_limits<double>::infinity();
        Event start = event(EVENT_START);
        start.mInitialYaw = nan;
        start.mInitialHeight = inf;
        start.mHasWorldHit = true;
        start.mWorldHit = point(1.0, nan, 3.0);
        Transition sanitized = reduce(State(), start);
        ensure("invalid start yaw is sanitized", nearlyEqual(sanitized.mState.mYaw, 0.0));
        ensure("invalid start height is sanitized", nearlyEqual(sanitized.mState.mHeight, 0.0));
        ensure("invalid start hit is rejected", !sanitized.mState.mHasAnchor);

        Event hover = event(EVENT_HOVER_HIT);
        hover.mHasWorldHit = true;
        hover.mWorldHit = point(inf, 2.0, 3.0);
        Transition rejected = reduce(sanitized.mState, hover);
        ensure("invalid hover remains unpinnable", !rejected.mState.mCurrentHoverValid);
        Transition click = reduce(rejected.mState, event(EVENT_LEFT_CLICK));
        ensure("invalid placement cannot pin", click.mState.mMode == MODE_FOLLOW_CURSOR);

        State valid = followState();
        Event wheel = event(EVENT_WHEEL);
        wheel.mContext.mHoveringPreview = true;
        wheel.mHasPreciseWheel = true;
        wheel.mPreciseWheelSteps = nan;
        wheel.mDiscreteWheelSteps = 8;
        Transition bad_wheel = reduce(valid, wheel);
        ensure("invalid precise wheel does not use discrete", !bad_wheel.mCommands.mUpdateYaw);

        Event begin = event(EVENT_MIDDLE_BEGIN);
        begin.mContext.mActiveAnchorHit = true;
        begin.mPointerY = nan;
        Transition bad_begin = reduce(valid, begin);
        ensure("invalid MMB coordinate cannot capture", !bad_begin.mCommands.mCaptureMouse);

        Config bad_config;
        bad_config.mHeightUnitsPerPixel = inf;
        begin.mPointerY = 10.0;
        Transition bad_scale_begin = reduce(valid, begin, bad_config);
        ensure("invalid height scale cannot capture",
               !bad_scale_begin.mCommands.mCaptureMouse);

        Config good_config;
        good_config.mHeightUnitsPerPixel = 0.1;
        State dragging = reduce(valid, begin, good_config).mState;
        Event move = event(EVENT_MIDDLE_MOVE);
        move.mPointerY = 0.0;
        Transition bad_move = reduce(dragging, move, bad_config);
        ensure("captured drag uses snapshotted valid scale",
               bad_move.mCommands.mUpdateHeight);
        ensure("later invalid config cannot poison height",
               nearlyEqual(bad_move.mState.mHeight, 1.0));
    }

    // UI-handled terminal input is not stolen, while lifecycle loss still
    // tears down state. This keeps UI-first distinct from model validity.
    template<> template<>
    void interaction_object::test<11>()
    {
        State pinned = pinnedState();
        Event enter = event(EVENT_ENTER);
        enter.mContext.mUIHandled = true;
        Transition ui_enter = reduce(pinned, enter);
        ensure("UI-handled Enter does not commit", !ui_enter.mCommands.mCommit);
        ensure("UI-handled Enter keeps pinned", ui_enter.mState.mMode == MODE_PINNED);

        Event cancel = event(EVENT_CANCEL);
        cancel.mContext.mUIHandled = true;
        Transition ui_cancel = reduce(pinned, cancel);
        ensure("UI-handled cancel does not cancel placement", !ui_cancel.mCommands.mCancel);
        ensure("UI-handled cancel keeps pinned", ui_cancel.mState.mMode == MODE_PINNED);

        Event loss = event(EVENT_TOOL_LOST);
        loss.mContext.mUIHandled = true;
        Transition authoritative = reduce(pinned, loss);
        ensure("tool loss bypasses UI handling", authoritative.mCommands.mCancel);
        ensure("tool loss returns inactive",
               authoritative.mState.mMode == MODE_INACTIVE);
    }

    // Enter and click must agree on whether the CURRENT hover can commit.
    // A hover miss preserves the last-valid anchor for display (so the
    // preview does not vanish), but neither click-to-pin nor Enter may use
    // that stale anchor to commit while the live hover is invalid -- doing
    // so used to let Enter silently commit wherever the cursor last was.
    template<> template<>
    void interaction_object::test<12>()
    {
        State state = followState();
        Event miss = event(EVENT_HOVER_HIT);
        Transition missed = reduce(state, miss);
        ensure("miss keeps the last anchor for display",
               missed.mState.mHasAnchor);
        ensure("miss invalidates the current hover",
               !missed.mState.mCurrentHoverValid);

        Transition stale_click = reduce(missed.mState, event(EVENT_LEFT_CLICK));
        ensure("stale click cannot pin", stale_click.mState.mMode == MODE_FOLLOW_CURSOR);
        ensure("stale click does not commit", !stale_click.mCommands.mCommit);

        Transition stale_enter = reduce(missed.mState, event(EVENT_ENTER));
        ensure("stale Enter agrees with click: no commit",
               !stale_enter.mCommands.mCommit);
        ensure("stale Enter stays active, not silently committed",
               stale_enter.mState.mMode == MODE_FOLLOW_CURSOR);

        // A restored valid hover lets both click and Enter commit again,
        // using the SAME resolved anchor.
        Event hit = event(EVENT_HOVER_HIT);
        hit.mHasWorldHit = true;
        hit.mWorldHit = point(50.0, 60.0, 70.0);
        Transition restored = reduce(missed.mState, hit);
        ensure("restored hover is valid", restored.mState.mCurrentHoverValid);
        Transition enter_commit = reduce(restored.mState, event(EVENT_ENTER));
        ensure("Enter commits once the hover is valid again",
               enter_commit.mCommands.mCommit);

        // PINNED already locked in a valid anchor at click time; a later
        // hover miss (which cannot even reach PINNED -- HOVER_HIT is only
        // handled in FOLLOW_CURSOR) never applies here, so Enter from PINNED
        // is unaffected by this fix.
        Transition pinned_enter = reduce(pinnedState(), event(EVENT_ENTER));
        ensure("pinned Enter still commits", pinned_enter.mCommands.mCommit);
    }
}
