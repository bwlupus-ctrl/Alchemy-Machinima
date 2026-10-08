/**
 * @file aldanceballmodel_test.cpp
 * @brief Tests for the dependency-free Dance Ball model.
 */

#include "linden_common.h"

#include "../test/lltut.h"
#include "../aldanceballmodel.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace tut
{
    using namespace ALDanceBallModel;

    namespace
    {
        bool approxNear(F64 lhs, F64 rhs, F64 epsilon = 1e-9)
        {
            return std::fabs(lhs - rhs) <= epsilon;
        }

        std::vector<PlaylistItem> available(std::size_t count)
        {
            return std::vector<PlaylistItem>(count, PlaylistItem());
        }
    }

    struct dance_ball_model_data {};
    typedef test_group<dance_ball_model_data> dance_ball_model_group;
    typedef dance_ball_model_group::object dance_ball_model_object;
    dance_ball_model_group dance_ball_model_tests("ALDanceBallModel");

    template<> template<>
    void dance_ball_model_object::test<1>()
    {
        Clock clock;
        ensure("base integration", approxNear(clock.advance(2.0), 2.0));
        clock.setSpeed(2.0);
        ensure("rate integration", approxNear(clock.advance(1.5), 5.0));
        ensure("negative delta ignored", approxNear(clock.advance(-4.0), 5.0));
        ensure("nan delta ignored", approxNear(clock.advance(
            std::numeric_limits<F64>::quiet_NaN()), 5.0));
    }

    template<> template<>
    void dance_ball_model_object::test<2>()
    {
        Clock clock;
        clock.rampTo(3.0, 2.0, RampCurve::LINEAR);
        ensure("linear ramp exact integral", approxNear(clock.advance(1.0), 1.5));
        ensure("linear midpoint speed", approxNear(clock.speed(), 2.0));
        clock.setFrozen(true);
        ensure("freeze holds time", approxNear(clock.advance(20.0), 1.5));
        ensure("freeze holds ramp progress", approxNear(clock.speed(), 2.0));
        clock.setFrozen(false);
        ensure("ramp resumes without jump", approxNear(clock.advance(1.0), 4.0));
        ensure("ramp reaches target", approxNear(clock.speed(), 3.0));

        clock.reset();
        clock.rampTo(3.0, 2.0, RampCurve::SMOOTHSTEP);
        ensure("smooth ramp integral", approxNear(clock.advance(2.0), 4.0));
        ensure("zero duration is immediate",
               (clock.rampTo(0.5, 0.0), approxNear(clock.speed(), 0.5)));
    }

    template<> template<>
    void dance_ball_model_object::test<3>()
    {
        ensure("sync is phase locked", phaseOffset(1, 0.0, 2, 3) == 0.0);
        const F64 a = phaseOffset(42, 1.0, 99, 0);
        ensure("phase normalized", a >= 0.0 && a < 1.0);
        ensure("phase deterministic", a == phaseOffset(42, 1.0, 99, 0));
        ensure("blend scales phase", approxNear(phaseOffset(42, 0.25, 99, 0), a * 0.25));
        ensure("different dancer lane", a != phaseOffset(43, 1.0, 99, 0));
        ensure("reshuffle changes lane", a != phaseOffset(42, 1.0, 99, 1));

        const F64 speed = speedMultiplier(42, 0.2, 99, 0);
        ensure("speed jitter bounded", speed >= 0.8 && speed <= 1.2);
        ensure("zero jitter is one", speedMultiplier(42, 0.0, 99, 0) == 1.0);
    }

    template<> template<>
    void dance_ball_model_object::test<4>()
    {
        ensure("outward delay", approxNear(waveDelay(10, 30, 5,
                                               WaveDirection::OUTWARD), 2.0));
        ensure("inward delay", approxNear(waveDelay(10, 30, 5,
                                              WaveDirection::INWARD), 4.0));
        ensure("zero spread safe", waveDelay(10, 30, 0,
                                              WaveDirection::OUTWARD) == 0.0);
        ensure("start mode applies to start",
               waveApplies(WaveApply::START_ONLY, false));
        ensure("start mode excludes switch",
               !waveApplies(WaveApply::START_ONLY, true));
        ensure("switch mode applies to switch",
               waveApplies(WaveApply::SWITCH_ONLY, true));
        ensure("both applies", waveApplies(WaveApply::START_AND_SWITCH, false) &&
                               waveApplies(WaveApply::START_AND_SWITCH, true));
    }

    template<> template<>
    void dance_ball_model_object::test<5>()
    {
        Clip clip;
        clip.mLoop = true;
        clip.mDuration = 10.0;
        clip.mLoopIn = 2.0;
        clip.mLoopOut = 6.0;
        Sample sample = mapSample(clip, 9.0, 1.0, 0.0);
        ensure("loop wraps inside authored range", approxNear(sample.mTime, 3.0));
        sample = mapSample(clip, 1.0, 1.0, 0.5, 2.0);
        ensure("delay has not started", !sample.mStarted && sample.mTime == 0.0);
        sample = mapSample(clip, 2.0, 1.0, 0.5, 2.0);
        ensure("phase applied at start", sample.mStarted && approxNear(sample.mTime, 4.0));
    }

    template<> template<>
    void dance_ball_model_object::test<6>()
    {
        Clip clip;
        clip.mLoop = true;
        clip.mDuration = std::numeric_limits<F64>::infinity();
        clip.mLoopIn = -5.0;
        clip.mLoopOut = std::numeric_limits<F64>::quiet_NaN();
        Sample sample = mapSample(clip, -1.0,
                                  std::numeric_limits<F64>::infinity(), -2.0);
        ensure("invalid loop holds finite zero",
               sample.mTime == 0.0 && std::isfinite(sample.mTime));
        clip.mDuration = 10.0;
        clip.mLoopIn = 3.0;
        clip.mLoopOut = 3.0;
        sample = mapSample(clip, 1.0, 9.0, 1.0);
        ensure("degenerate loop preserves intro", sample.mTime == 1.0);
        sample = mapSample(clip, 100.0, 9.0, 1.0);
        ensure("equal loop points play through", sample.mTime == 10.0);
    }

    template<> template<>
    void dance_ball_model_object::test<7>()
    {
        Clip clip;
        clip.mLoop = false;
        clip.mDuration = 3.0;
        Sample sample = mapSample(clip, 2.0, 99.0, 0.75);
        ensure("play once is fixed 1x", sample.mTime == 2.0);
        ensure("play once not ended early", !sample.mEnded);
        sample = mapSample(clip, 3.0, 0.0, 0.0);
        ensure("play once reports end", sample.mEnded && sample.mTime == 3.0);
        clip.mDuration = -1.0;
        ensure("invalid play once ends", mapSample(clip, 0.0, 1.0, 0.0).mEnded);
    }

    template<> template<>
    void dance_ball_model_object::test<8>()
    {
        Scheduler scheduler;
        SchedulerConfig config;
        std::vector<PlaylistItem> empty;
        ensure("empty playlist", scheduler.update(0.0, 1, empty, config).mItem == -1);

        std::vector<PlaylistItem> none = available(3);
        for (PlaylistItem& item : none) item.mAvailable = false;
        scheduler.reset();
        ensure("all unavailable", scheduler.update(0.0, 1, none, config).mItem == -1);

        scheduler.reset();
        std::vector<PlaylistItem> one = available(1);
        config.mAutoAdvance = true;
        config.mIntervalSeconds = 1.0;
        ensure("single item starts", scheduler.update(0.0, 1, one, config).mItem == 0);
        Selection same = scheduler.update(100.0, 1, one, config);
        ensure("single item does not fake a switch", !same.mChanged && same.mItem == 0);
    }

    template<> template<>
    void dance_ball_model_object::test<9>()
    {
        Scheduler scheduler;
        SchedulerConfig config;
        config.mMode = PlaylistMode::SEQUENCE_EACH_DANCER;
        config.mAutoAdvance = true;
        config.mIntervalSeconds = 2.0;
        std::vector<PlaylistItem> items = available(4);
        items[1].mAvailable = false;
        const S32 first = scheduler.update(0.0, 7, items, config).mItem;
        ensure("sequence starts on an available offset", first >= 0 && first != 1);
        const S32 expected_second = first == 0 ? 2 : (first == 2 ? 3 : 0);
        const S32 second = scheduler.update(2.0, 7, items, config).mItem;
        ensure("sequence skips unavailable", second == expected_second);
        const S32 expected_third = second == 0 ? 2 : (second == 2 ? 3 : 0);
        ensure("next wraps", scheduler.next(2.1, 7, items, config).mItem == expected_third);
        ensure("previous works", scheduler.previous(2.2, 7, items, config).mItem == second);

        items[static_cast<std::size_t>(second)].mAvailable = false;
        ensure("newly unavailable current is skipped",
               scheduler.update(2.3, 7, items, config).mItem == expected_third);
    }

    template<> template<>
    void dance_ball_model_object::test<10>()
    {
        Scheduler a;
        Scheduler b;
        SchedulerConfig config;
        config.mMode = PlaylistMode::SHUFFLE_EACH_DANCER;
        config.mAutoAdvance = true;
        config.mIntervalSeconds = 1.0;
        config.mJitterSeconds = 0.25;
        config.mSeed = 12345;
        const std::vector<PlaylistItem> items = available(5);
        a.update(0.0, 10, items, config);
        b.update(0.0, 10, items, config);
        const Selection sa = a.update(1000.0, 10, items, config);
        const Selection sb = b.update(1000.0, 10, items, config);
        ensure("shuffle is deterministic", sa.mItem == sb.mItem &&
                                               sa.mEventIndex == sb.mEventIndex &&
                                               sa.mBoundary == sb.mBoundary);

        Scheduler incremental;
        incremental.update(0.0, 10, items, config);
        for (S32 second = 1; second <= 1000; ++second)
        {
            incremental.update(static_cast<F64>(second), 10, items, config);
        }
        ensure("shuffle ignores frame grouping",
               incremental.current(10, config.mMode) == sa.mItem);

        Scheduler per_dancer;
        per_dancer.update(0.0, 1, items, config);
        per_dancer.update(0.0, 2, items, config);
        ensure("per dancer owns state",
               per_dancer.current(1, config.mMode) >= 0 &&
               per_dancer.current(2, config.mMode) >= 0);
        const S32 dancer_two = per_dancer.current(2, config.mMode);
        const S32 dancer_one = per_dancer.current(1, config.mMode);
        per_dancer.next(0.1, 1, items, config);
        ensure("shuffle next excludes current",
               per_dancer.current(1, config.mMode) != dancer_one);
        ensure("manual step is dancer-local",
               per_dancer.current(2, config.mMode) == dancer_two);

        config.mMode = PlaylistMode::SAME_FOR_ALL;
        Scheduler shared;
        const Selection first = shared.update(0.0, 1, items, config);
        const Selection second = shared.update(0.2, 999, items, config);
        ensure("same mode shares selection",
               second.mItem ==
               shared.current(1, config.mMode));
        ensure("same mode shares initial boundary",
               first.mBoundary == second.mBoundary);
        Clip sync_clip;
        sync_clip.mDuration = 8.0;
        sync_clip.mLoopIn = 0.0;
        sync_clip.mLoopOut = 8.0;
        ensure("same initial boundary produces same sample",
               mapSample(sync_clip, 0.2 - first.mBoundary, 1.0, 0.0).mTime ==
               mapSample(sync_clip, 0.2 - second.mBoundary, 1.0, 0.0).mTime);
        const Selection advanced = shared.update(2.0, 1, items, config);
        const Selection follower = shared.update(2.1, 999, items, config);
        ensure("same mode shares automatic boundary",
               advanced.mBoundary == follower.mBoundary);
        ensure("same automatic boundary produces same sample",
               mapSample(sync_clip, 2.1 - advanced.mBoundary, 1.0, 0.0).mTime ==
               mapSample(sync_clip, 2.1 - follower.mBoundary, 1.0, 0.0).mTime);
        const Selection selected = shared.select(3.0, 1, 3, items, config);
        const Selection selected_follower = shared.update(3.1, 999, items, config);
        ensure("same mode shares selected boundary and item",
               selected.mItem == selected_follower.mItem &&
               selected.mBoundary == selected_follower.mBoundary);
        ensure("same selected boundary produces same sample",
               mapSample(sync_clip, 3.1 - selected.mBoundary, 1.0, 0.0).mTime ==
               mapSample(sync_clip, 3.1 - selected_follower.mBoundary,
                         1.0, 0.0).mTime);
    }

    template<> template<>
    void dance_ball_model_object::test<11>()
    {
        Crossfade fade = crossfadeWeights(0.25, 0.5);
        ensure("half crossfade", approxNear(fade.mFrom, 0.5) && approxNear(fade.mTo, 0.5));
        ensure("half incomplete", !fade.mComplete);
        fade = crossfadeWeights(10.0, 0.5);
        ensure("crossfade clamps complete",
               fade.mFrom == 0.0 && fade.mTo == 1.0 && fade.mComplete);
        fade = crossfadeWeights(0.0, 0.0);
        ensure("zero duration is cut",
               fade.mFrom == 0.0 && fade.mTo == 1.0 && fade.mComplete);
    }

    template<> template<>
    void dance_ball_model_object::test<12>()
    {
        Scheduler scheduler;
        SchedulerConfig config;
        config.mMode = PlaylistMode::SEQUENCE_EACH_DANCER;
        const std::vector<PlaylistItem> items = available(3);
        ensure("invalid time ignored", !scheduler.update(
            std::numeric_limits<F64>::quiet_NaN(), 1, items, config).mChanged);
        scheduler.update(0.0, 1, items, config);
        ensure("negative manual time ignored",
               !scheduler.next(-1.0, 1, items, config).mChanged);
        ensure("direct select validates availability",
               scheduler.select(1.0, 1, 2, items, config).mItem == 2);
        std::vector<PlaylistItem> unavailable = items;
        unavailable[1].mAvailable = false;
        ensure("direct select rejects unavailable",
               scheduler.select(2.0, 1, 1, unavailable, config).mItem == -1);
    }

    template<> template<>
    void dance_ball_model_object::test<13>()
    {
        Scheduler scheduler;
        SchedulerConfig config;
        config.mMode = PlaylistMode::SEQUENCE_EACH_DANCER;
        config.mAutoAdvance = true;
        config.mIntervalSeconds = 10.0;
        const std::vector<PlaylistItem> items = available(3);
        const Selection initial = scheduler.update(2.0, 7, items, config);
        config.mIntervalSeconds = 20.0;
        config.mJitterSeconds = 1.0;
        const Selection after = scheduler.update(5.0, 7, items, config);
        ensure("setting change preserves current item",
               after.mItem == initial.mItem && !after.mChanged);
        ensure("setting change preserves item start",
               after.mBoundary == initial.mBoundary);
    }

    template<> template<>
    void dance_ball_model_object::test<14>()
    {
        for (S32 i = 0; i <= 10; ++i)
        {
            const Crossfade fade = crossfadeWeights(i / 10.0, 1.0);
            ensure("crossfade weights sum to one",
                   approxNear(fade.mFrom + fade.mTo, 1.0));
        }
    }

    template<> template<>
    void dance_ball_model_object::test<15>()
    {
        Clock clock;
        clock.rampTo(3.0, 2.0, RampCurve::LINEAR);
        ensure("ramp reaches midpoint", approxNear(clock.advance(1.0), 1.5));
        clock.rampTo(0.0, 2.0, RampCurve::LINEAR);
        ensure("retarget starts from live speed", approxNear(clock.speed(), 2.0));
        ensure("ramp down integrates continuously", approxNear(clock.advance(2.0), 3.5));
        ensure("ramp down reaches zero", approxNear(clock.speed(), 0.0));

        Clip clip;
        clip.mDuration = 6.0;
        clip.mLoopIn = 2.0;
        clip.mLoopOut = 6.0;
        ensure("phase one wraps to loop in",
               approxNear(mapSample(clip, 0.0, 1.0, 1.0).mTime, 2.0));
    }

    template<> template<>
    void dance_ball_model_object::test<16>()
    {
        Scheduler scheduler;
        SchedulerConfig config;
        config.mMode = PlaylistMode::SEQUENCE_EACH_DANCER;
        const std::vector<PlaylistItem> items = available(3);
        scheduler.select(0.0, 11, 2, items, config);
        scheduler.select(0.0, 22, 1, items, config);
        scheduler.remapItems({ 1, 2, 0 });
        ensure("playlist identity remap preserves current row",
               scheduler.current(11, config.mMode) == 0 &&
               scheduler.current(22, config.mMode) == 2);
        scheduler.erase(11, config.mMode);
        ensure("removed dancer state erased",
               scheduler.current(11, config.mMode) == -1 &&
               scheduler.current(22, config.mMode) == 2);
    }

    template<> template<>
    void dance_ball_model_object::test<17>()
    {
        Scheduler scheduler;
        SchedulerConfig config;
        config.mMode = PlaylistMode::SAME_FOR_ALL;
        const std::vector<PlaylistItem> items = available(1);
        const Selection initial = scheduler.update(2.0, 1, items, config);
        const Selection selected = scheduler.select(8.0, 1, 0, items, config);
        const Selection late_joiner = scheduler.update(12.0, 999, items, config);
        ensure("same-item select keeps boundary",
               selected.mBoundary == initial.mBoundary);
        ensure("late joiner receives original phase boundary",
               late_joiner.mBoundary == initial.mBoundary);
        const Selection next_same = scheduler.next(20.0, 1, items, config);
        ensure("single-item next keeps boundary",
               next_same.mBoundary == initial.mBoundary);
    }

    template<> template<>
    void dance_ball_model_object::test<18>()
    {
        const F64 interruptions[] = { 0.1, 0.35, 0.75, 0.99 };
        for (F64 at : interruptions)
        {
            std::vector<LegWeight> legs(2);
            legs[0].mCurrent = 1.0;
            legs[0].mStarted = true;
            retargetLegWeights(legs, 1);
            advanceLegWeights(legs, at);
            legs[1].mStarted = true;

            legs.emplace_back();
            retargetLegWeights(legs, 2);
            for (S32 step = 0; step <= 10; ++step)
            {
                advanceLegWeights(legs, step / 10.0);
                const F64 sum = legs[0].mCurrent + legs[1].mCurrent +
                                legs[2].mCurrent;
                ensure("A-B-C interruption remains normalized", approxNear(sum, 1.0));
            }
        }
    }

    template<> template<>
    void dance_ball_model_object::test<19>()
    {
        std::vector<LegWeight> legs(2);
        legs[0].mCurrent = 1.0;
        legs[0].mStarted = true;
        retargetLegWeights(legs, 1); // B is cold and remains at zero.
        legs.erase(legs.begin() + 1);
        legs.emplace_back();          // rapid Next selects C instead.
        retargetLegWeights(legs, 1);
        advanceLegWeights(legs, 0.5);
        ensure("rapid Next Next keeps outgoing pose",
               approxNear(legs[0].mCurrent, 0.5));
        ensure("rapid Next Next fades only real incoming",
               approxNear(legs[1].mCurrent, 0.5));
        ensure("rapid Next Next stays normalized",
               approxNear(legs[0].mCurrent + legs[1].mCurrent, 1.0));
    }

    template<> template<>
    void dance_ball_model_object::test<20>()
    {
        const F64 rate_a = effectiveSpeedMultiplier(
            11, PlaylistMode::SAME_FOR_ALL, 0.0, 0.8, 123, 4);
        const F64 rate_b = effectiveSpeedMultiplier(
            99, PlaylistMode::SAME_FOR_ALL, 0.0, 0.8, 123, 4);
        ensure("synced same-for-all has one speed lane",
               rate_a == 1.0 && rate_b == 1.0);
        SampleClock a;
        SampleClock b;
        a.reset(3.0, rate_a, 0.0);
        b.reset(3.0, rate_b, 0.0);
        for (S32 i = 0; i < 20; ++i)
        {
            ensure("synced samples remain equal",
                   approxNear(a.advance(0.125, 0.125), b.advance(0.125, 0.125)));
        }
        const F64 shared = 20.0;
        ensure("historically drifted lane starts continuous",
               approxNear(slewToSharedSample(shared, 3.0, 0.0), 23.0));
        ensure("historically drifted lanes converge to shared sample",
               approxNear(slewToSharedSample(shared, 3.0, 1.0), shared) &&
               approxNear(slewToSharedSample(shared, -2.0, 1.0), shared));
        ensure("whole-loop drift collapses to nearest phase error",
               approxNear(nearestLoopError(15.25, 4.0), -0.75));
        ensure("sync slew removes only fractional loop error",
               approxNear(slewToSharedSample(shared,
                   nearestLoopError(60.25, 4.0), 0.0), 20.25));
    }

    template<> template<>
    void dance_ball_model_object::test<21>()
    {
        SampleClock clock;
        clock.reset(10.0, 1.0, 0.0);
        clock.retarget(2.0, 4.0, 2.0);
        ensure("retarget has no retroactive jump", approxNear(clock.time(), 10.0));
        ensure("rate and phase slew integrate",
               approxNear(clock.advance(1.0, 1.0), 13.25));
        ensure("zero transport delta freezes every slew",
               approxNear(clock.advance(0.0, 0.0), 13.25));
        ensure("slew completes continuously",
               approxNear(clock.advance(1.0, 1.0), 17.0));

        Scheduler scheduler;
        SchedulerConfig config;
        config.mMode = PlaylistMode::SEQUENCE_EACH_DANCER;
        const std::vector<PlaylistItem> items = available(4);
        scheduler.select(0.0, 7, 2, items, config);
        scheduler.remapItems({ 0, 1, -1, 2 });
        ensure("removed playing row continues to next survivor",
               scheduler.current(7, config.mMode) == 2);
    }

    template<> template<>
    void dance_ball_model_object::test<22>()
    {
        // Turn ON, then Next while the first leg is only 0.3 faded in.
        std::vector<LegWeight> legs(2);
        legs[0].mCurrent = 0.3;
        legs[0].mStarted = true;
        retargetLegWeights(legs, 1);
        ensure("partial leg is not scaled up at retarget",
               approxNear(legs[0].mCurrent, 0.3) && approxNear(legs[0].mStart, 0.3));
        ensure("cold incoming leg stays at zero", approxNear(legs[1].mCurrent, 0.0));
        F64 previous_sum = legs[0].mCurrent + legs[1].mCurrent;
        ensure("live sum is unchanged by the retarget", approxNear(previous_sum, 0.3));
        for (S32 step = 0; step <= 20; ++step)
        {
            advanceLegWeights(legs, step / 20.0);
            const F64 sum = legs[0].mCurrent + legs[1].mCurrent;
            ensure("partial fade never exceeds full weight", sum <= 1.0 + 1e-12);
            ensure("partial fade sum never dips", sum >= previous_sum - 1e-12);
            previous_sum = sum;
        }
        ensure("partial fade converges to a single full leg",
               approxNear(previous_sum, 1.0) && approxNear(legs[0].mCurrent, 0.0) &&
               approxNear(legs[1].mCurrent, 1.0));

        // OFF then ON while a release is mid-fade: two started legs, sum 0.3,
        // the second one is re-targeted. No leg may jump upward.
        std::vector<LegWeight> two(2);
        two[0].mCurrent = 0.2;
        two[0].mStarted = true;
        two[1].mCurrent = 0.1;
        two[1].mStarted = true;
        retargetLegWeights(two, 1);
        ensure("no leg pops up at retarget",
               two[0].mCurrent <= 0.2 + 1e-12 && two[1].mCurrent <= 0.1 + 1e-12 &&
               approxNear(two[0].mCurrent, 0.2) && approxNear(two[1].mCurrent, 0.1));
        F64 prior = two[0].mCurrent + two[1].mCurrent;
        for (S32 step = 0; step <= 10; ++step)
        {
            advanceLegWeights(two, step / 10.0);
            const F64 sum = two[0].mCurrent + two[1].mCurrent;
            ensure("two-leg partial sum stays within [live, 1]",
                   sum <= 1.0 + 1e-12 && sum >= prior - 1e-12);
            prior = sum;
        }
        ensure("two-leg partial fade lands on the incoming leg",
               approxNear(two[0].mCurrent, 0.0) && approxNear(two[1].mCurrent, 1.0));

        // An over-full envelope is still scaled down to exactly 1.
        std::vector<LegWeight> over(2);
        over[0].mCurrent = 0.8;
        over[0].mStarted = true;
        over[1].mCurrent = 0.6;
        over[1].mStarted = true;
        retargetLegWeights(over, 1);
        ensure("over-full envelope is normalized down",
               approxNear(over[0].mCurrent + over[1].mCurrent, 1.0) &&
               over[0].mCurrent < 0.8 && over[1].mCurrent < 0.6);
    }

    namespace
    {
        // Steps the lane and checks the instantaneous rate every step.
        bool rateStaysInBand(SampleClock& clock, F64 duration, F64 step,
                             F64 low, F64 high)
        {
            const S32 steps = static_cast<S32>(std::ceil(duration / step));
            for (S32 i = 0; i < steps; ++i)
            {
                const F64 before = clock.time();
                const F64 after = clock.advance(step, step);
                const F64 rate = (after - before) / step;
                if (rate < low - 1e-9 || rate > high + 1e-9)
                {
                    return false;
                }
            }
            return true;
        }
    }

    template<> template<>
    void dance_ball_model_object::test<23>()
    {
        const F64 cycle = 4.0;
        const F64 band_low = 1.0 - MAX_PHASE_SLEW_RATE;
        const F64 band_high = 1.0 + MAX_PHASE_SLEW_RATE;

        ensure("slew keeps the crossfade when the error is small",
               approxNear(phaseSlewDuration(0.5, 0.1), 0.5) &&
               approxNear(phaseSlewDuration(0.5, -0.1), 0.5));
        ensure("slew stretches to cap the rate",
               approxNear(phaseSlewDuration(0.5, 2.0), 4.0) &&
               approxNear(phaseSlewDuration(0.5, -2.0), 4.0));
        ensure("zero crossfade stays an explicit snap",
               phaseSlewDuration(0.0, 2.0) == 0.0);

        // Half a cycle ahead over a 0.5 s crossfade: previously 4x.
        SampleClock forward;
        forward.reset(10.0, 1.0, 0.0);
        forward.retarget(1.0, 2.0, 0.5, cycle);
        ensure("half-cycle forward error stays within 0.5x..1.5x",
               rateStaysInBand(forward, 4.0, 0.05, band_low, band_high));
        ensure("forward slew lands exactly on the phase",
               approxNear(forward.time(), 16.0, 1e-6));

        // Half a cycle behind: previously -3x (backward).
        SampleClock backward;
        backward.reset(10.0, 1.0, 0.0);
        backward.retarget(1.0, -2.0, 0.5, cycle);
        ensure("half-cycle backward error never plays backward",
               rateStaysInBand(backward, 4.0, 0.05, band_low, band_high));
        ensure("backward slew lands exactly on the phase",
               approxNear(backward.time(), 12.0, 1e-6));

        // More than half a cycle takes the short way round.
        SampleClock wrapped;
        wrapped.reset(10.0, 1.0, 0.0);
        wrapped.retarget(1.0, 3.0, 0.5, cycle);
        ensure("wrapped slew stays within band",
               rateStaysInBand(wrapped, 2.0, 0.05, band_low, band_high));
        ensure("three quarters of a cycle is one quarter backward",
               approxNear(wrapped.time(), 11.0, 1e-6));

        // A phase congruent to the current target is not a change.
        SampleClock steady;
        steady.reset(10.0, 1.0, 0.4);
        steady.retarget(1.0, 0.4 + cycle, 0.5, cycle);
        ensure("congruent phase request is a no-op",
               approxNear(steady.advance(0.1, 0.1), 10.1));
    }

    template<> template<>
    void dance_ball_model_object::test<24>()
    {
        // Reshuffle: every old phase to every new phase across a full cycle.
        const F64 cycle = 4.0;
        const F64 band_low = 1.0 - MAX_PHASE_SLEW_RATE;
        const F64 band_high = 1.0 + MAX_PHASE_SLEW_RATE;
        for (S32 from = 0; from < 10; ++from)
        {
            for (S32 to = 0; to < 10; ++to)
            {
                const F64 old_phase = cycle * from / 10.0;
                const F64 new_phase = cycle * to / 10.0;
                SampleClock clock;
                clock.reset(10.0, 1.0, old_phase);
                clock.retarget(1.0, new_phase, 0.5, cycle);
                ensure("reshuffle slew stays within 0.5x..1.5x",
                       rateStaysInBand(clock, 4.0, 0.025, band_low, band_high));
                const F64 delta = std::remainder(new_phase - old_phase, cycle);
                // 4 s covers the longest possible slew (half a cycle), so the
                // lane has taken exactly the shortest-path phase change.
                ensure("reshuffle lands on the shortest-path phase",
                       approxNear(clock.time(), 10.0 + 4.0 + delta, 1e-6));
            }
        }

        // A second reshuffle landing mid-slew restarts from the live phase.
        SampleClock clock;
        clock.reset(10.0, 1.0, 0.0);
        clock.retarget(1.0, 2.0, 0.5, cycle);
        ensure("first reshuffle slew in band",
               rateStaysInBand(clock, 1.0, 0.05, band_low, band_high));
        clock.retarget(1.0, -1.5, 0.5, cycle);
        ensure("second reshuffle slew in band",
               rateStaysInBand(clock, 6.0, 0.05, band_low, band_high));
    }

    template<> template<>
    void dance_ball_model_object::test<25>()
    {
        // Shared-path slew: shared(t) + e * (1 - t / D) has rate 1 - e / D.
        const F64 errors[] = { 2.0, -2.0, 0.3, -1.9, 1.99, 0.0 };
        for (F64 error : errors)
        {
            const F64 duration = phaseSlewDuration(0.5, error);
            const F64 step = 0.05;
            F64 previous = slewToSharedSample(100.0, error, 0.0);
            for (F64 t = step; t <= duration + 1e-9; t += step)
            {
                const F64 shared = 100.0 + t;
                const F64 sample = slewToSharedSample(shared, error, t / duration);
                const F64 rate = (sample - previous) / step;
                previous = sample;
                ensure("shared sync slew stays within 0.5x..1.5x",
                       rate >= 0.5 - 1e-9 && rate <= 1.5 + 1e-9);
            }
        }
    }

    template<> template<>
    void dance_ball_model_object::test<26>()
    {
        // Global speed 0.25x with a backward correction: slew progress follows
        // the speed-scaled transport delta, so the effective wall rate stays
        // within [0.5, 1.5] x the current transport rate and is never negative.
        const F64 cycle = 4.0;
        const F64 global_speed = 0.25;
        const F64 wall_step = 0.05;
        const F64 targets[] = { -2.0, 2.0, -1.0, 1.5 };
        for (F64 target : targets)
        {
            Clock transport;
            transport.setSpeed(global_speed);
            SampleClock lane;
            lane.reset(10.0, 1.0, 0.0);
            lane.retarget(1.0, target, 0.5, cycle);
            F64 min_rate = 1e9;
            F64 max_rate = -1e9;
            const F64 wall_total = 24.0;
            const S32 steps = static_cast<S32>(std::ceil(wall_total / wall_step));
            for (S32 i = 0; i < steps; ++i)
            {
                const F64 prior_transport = transport.time();
                transport.advance(wall_step);
                const F64 transport_delta = transport.time() - prior_transport;
                const F64 before = lane.time();
                const F64 after = lane.advance(transport_delta, transport_delta);
                const F64 wall_rate = (after - before) / wall_step;
                min_rate = std::min(min_rate, wall_rate);
                max_rate = std::max(max_rate, wall_rate);
            }
            ensure("slow global speed never plays a correction in reverse",
                   min_rate >= 0.0);
            ensure("slow global speed stays within 0.5x of the transport rate",
                   min_rate >= global_speed * 0.5 - 1e-9);
            ensure("slow global speed stays within 1.5x of the transport rate",
                   max_rate <= global_speed * 1.5 + 1e-9);
            // 24 s of wall time is 6 s of transport: the longest slew (half a
            // cycle) takes 4 s, then the lane plays at 1x transport rate.
            const F64 delta = std::remainder(target, cycle);
            ensure("slow global speed lands on the shortest-path phase",
                   approxNear(lane.time(), 10.0 + 6.0 + delta, 1e-6));
        }

        // The shared-time lane: sample = shared(t) + e * (1 - p), with p driven
        // by the transport delta and shared(t) advancing at the transport rate.
        const F64 errors[] = { 2.0, -2.0, 0.3, -1.9 };
        for (F64 error : errors)
        {
            const F64 duration = phaseSlewDuration(0.5, error);
            const F64 transport_step = global_speed * wall_step;
            F64 elapsed = 0.0;
            F64 shared = 100.0;
            F64 previous = slewToSharedSample(shared, error, 0.0);
            while (elapsed < duration - 1e-12)
            {
                elapsed += transport_step;
                shared += transport_step;
                const F64 sample = slewToSharedSample(shared, error,
                                                      elapsed / duration);
                const F64 wall_rate = (sample - previous) / wall_step;
                previous = sample;
                ensure("shared lane slow-speed correction is never reversed",
                       wall_rate >= 0.0);
                // The last partial step may overshoot the slew end; its rate
                // is then a blend that is still bounded below by the band.
                ensure("shared lane slow-speed rate stays in the transport band",
                       wall_rate >= global_speed * 0.5 - 1e-9 &&
                       wall_rate <= global_speed * 1.5 + 1e-9);
            }
        }
    }

    template<> template<>
    void dance_ball_model_object::test<27>()
    {
        // A slow lane (Speed Jitter above 0.5 gives rates below 0.5) must get a
        // proportionally gentler correction, never a reverse playback.
        const F64 cycle = 4.0;
        ensure("slow lane stretches the slew by its own rate",
               approxNear(phaseSlewDuration(0.5, 2.0, 0.5), 8.0) &&
               approxNear(phaseSlewDuration(0.5, -2.0, 0.5), 8.0) &&
               approxNear(phaseSlewDuration(0.5, 2.0, 1.0), 4.0));
        ensure("zero crossfade is still a snap at any lane rate",
               phaseSlewDuration(0.0, 2.0, 0.3) == 0.0);

        const F64 lane_rates[] = { 0.3, 0.45, 0.8, 1.5 };
        const F64 targets[] = { -2.0, 2.0, -1.2 };
        for (F64 lane_rate : lane_rates)
        {
            for (F64 target : targets)
            {
                SampleClock lane;
                lane.reset(10.0, lane_rate, 0.0);
                lane.retarget(lane_rate, target, 0.5, cycle);
                ensure("slow lane correction stays within 0.5x..1.5x of its rate",
                       rateStaysInBand(lane, 40.0, 0.05, lane_rate * 0.5,
                                       lane_rate * 1.5));
                ensure("slow lane correction lands on the shortest-path phase",
                       approxNear(lane.time(),
                                  10.0 + 40.0 * lane_rate +
                                      std::remainder(target, cycle), 1e-6));
            }
        }

        // A lane below the slew-sizing floor still never plays backward.
        SampleClock stalled;
        stalled.reset(10.0, 0.01, 0.0);
        stalled.retarget(0.01, -2.0, 0.5, cycle);
        ensure("near-stopped lane never plays a correction in reverse",
               rateStaysInBand(stalled, 20.0, 0.1, 0.0, 1.0));

        // A rate change that speeds the lane up mid-correction is bounded by
        // the slower end of the sweep.
        SampleClock sweep;
        sweep.reset(10.0, 0.4, 0.0);
        sweep.retarget(1.0, -2.0, 0.5, cycle);
        ensure("rate sweep with a backward correction never reverses",
               rateStaysInBand(sweep, 30.0, 0.05, 0.0, 2.0));
    }
}
