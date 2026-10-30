#include "linden_common.h"
#include "../test/lltut.h"
#include "alavataranimationtime.h"
#include "alavatarconstraint.h"
#include "../alpathgeometry.h"
#include "../alposesecondary.h"
#include "../alposecontinuity.h"
#include "../alcontactstab.h"
#include "../algazemotor.h"
#include <limits>
#include <cstring>

namespace tut
{
struct avatar_animation_time_data
{
    // The avatar supplies runtime phase. Keep /fp:fast constant-folding of a
    // literal fixture from selecting a different evaluator path than the loop.
    static F32 runtimePhase(F32 phase)
    {
        volatile F32 runtime = phase;
        return runtime;
    }
    static bool same(F32 a, F32 b) { return std::memcmp(&a, &b, sizeof(a)) == 0; }
    static void samePose(const ALPoseSecondary::SecondaryPose& a,
                         const ALPoseSecondary::SecondaryPose& b)
    {
        ensure("held chest bits", same(a.mChestPitch, b.mChestPitch));
        ensure("held spine roll bits", same(a.mSpineRoll, b.mSpineRoll));
        ensure("held spine pitch bits", same(a.mSpinePitch, b.mSpinePitch));
    }
};
typedef test_group<avatar_animation_time_data> avatar_animation_time_group;
typedef avatar_animation_time_group::object avatar_animation_time_object;
avatar_animation_time_group avatar_animation_time_tests("alavataranimationtime");

template<> template<> void avatar_animation_time_object::test<1>()
{
    F32 time = runtimePhase(1.375f);
    const ALPoseSecondary::SecondaryParams params;
    const auto before = ALPoseSecondary::evalSecondary(time, params);
    for (S32 frame = 0; frame < 120; ++frame)
    {
        const auto dt = ALAvatarAnimationTime::delta(1.f / 60.f, 0.f, 1.f);
        ensure("freeze is not a hitch", !dt.mReseed);
        ensure("hold still paints", ALPoseSecondary::advanceTime(time, dt));
        ensure_equals("phase holds away from origin", time, 1.375f);
        samePose(before, ALPoseSecondary::evalSecondary(time, params));
    }
    ALPoseSecondary::advanceTime(time, ALAvatarAnimationTime::delta(0.125f, 1.f, 1.f));
    ensure_equals("resume continuously", time, 1.5f);
}

template<> template<> void avatar_animation_time_object::test<2>()
{
    F32 slow = 0.f, live = 0.f;
    for (S32 frame = 0; frame < 32; ++frame)
    {
        ALPoseSecondary::advanceTime(slow, ALAvatarAnimationTime::delta(0.0625f, 0.5f, 1.f));
        ALPoseSecondary::advanceTime(live, ALAvatarAnimationTime::delta(0.0625f, 1.f, 1.f));
    }
    ensure_equals("half world speed halves phase", slow, live * 0.5f);
    ensure_equals("avatar speed multiplies world speed",
        ALAvatarAnimationTime::delta(0.125f, 0.5f, 0.25f).mSeconds, 0.015625f);
    ensure_equals("avatar zero freezes too",
        ALAvatarAnimationTime::delta(0.125f, 1.f, 0.f).mSeconds, 0.f);
}

template<> template<> void avatar_animation_time_object::test<3>()
{
    F32 actual = 0.f, legacy = 0.f;
    const ALPoseSecondary::SecondaryParams params;
    for (S32 frame = 0; frame < 32000; ++frame)
    {
        const F32 wall_dt = frame % 2 ? 0.017f : 0.031f;
        const auto dt = ALAvatarAnimationTime::delta(wall_dt, 1.f, 1.f);
        ensure("unit delta bit identical", same(dt.mSeconds, wall_dt));
        legacy += wall_dt;
        if (legacy >= ALPoseSecondary::COMMON_PERIOD_SEC)
            legacy -= ALPoseSecondary::COMMON_PERIOD_SEC;
        ALPoseSecondary::advanceTime(actual, dt);
        ensure("legacy phase bits including wrap", same(actual, legacy));
        samePose(ALPoseSecondary::evalSecondary(actual, params),
                 ALPoseSecondary::evalSecondary(legacy, params));
    }
}

template<> template<> void avatar_animation_time_object::test<4>()
{
    F32 time = 3.f;
    const auto hitch = ALAvatarAnimationTime::delta(0.5f, 0.01f, 1.f);
    ensure("small animation delta still a wall hitch", hitch.mReseed);
    ensure("hitch skips pose", !ALPoseSecondary::advanceTime(time, hitch));
    ensure_equals("hitch reseeds", time, 0.f);
    ensure("fast animation is not a wall hitch",
        !ALAvatarAnimationTime::delta(0.125f, 4.f, 4.f).mReseed);
    ensure("zero wall delta holds", !ALAvatarAnimationTime::delta(0.f, 1.f, 1.f).mReseed);
    ensure("invalid wall reseeds", ALAvatarAnimationTime::delta(
        std::numeric_limits<F32>::quiet_NaN(), 1.f, 1.f).mReseed);
    ensure("infinite wall reseeds", ALAvatarAnimationTime::delta(
        std::numeric_limits<F32>::infinity(), 1.f, 1.f).mReseed);
}

template<> template<> void avatar_animation_time_object::test<5>()
{
    ALAvatarAnimationTime::Clock clock;
    ensure_equals("legacy epoch exact", clock.sample(123.123456789,
        ALAvatarAnimationTime::delta(0.125f, 1.f, 1.f), true), 123.123456789);
    ensure_equals("legacy next epoch exact", clock.sample(123.25,
        ALAvatarAnimationTime::delta(0.125f, 1.f, 1.f), true), 123.25);
    for (S32 i = 0; i < 20; ++i)
        ensure_equals("absolute samplers hold", clock.sample(124.0 + i,
            ALAvatarAnimationTime::delta(0.125f, 0.f, 1.f), false), 123.25);
    ensure_equals("no wall catch up on resume", clock.sample(200.0,
        ALAvatarAnimationTime::delta(0.125f, 1.f, 1.f), true), 123.375);
    ensure_equals("half speed absolute phase", clock.sample(201.0,
        ALAvatarAnimationTime::delta(0.125f, 0.5f, 1.f), false), 123.4375);
}
template<> template<> void avatar_animation_time_object::test<6>()
{
    for (S32 reset = 0; reset < 4; ++reset)
    {
        ALAvatarAnimationTime::Clock timer{0.0, false, true};
        ensure_equals("reset timer scales first elapsed once", timer.sample(0.125,
            ALAvatarAnimationTime::delta(0.125f, 0.5f, 1.f), false), 0.0625);
        for (S32 i = 0; i < 10; ++i)
            ensure_equals("timer freeze holds elapsed", timer.sample(1.0 + i,
                ALAvatarAnimationTime::delta(0.125f, 0.f, 1.f), false), 0.0625);
        ensure_equals("timer resume excludes freeze", timer.sample(20.0,
            ALAvatarAnimationTime::delta(0.125f, 1.f, 1.f), true), 0.1875);
    }
    ALAvatarAnimationTime::Clock fresh;
    ensure_equals("fresh absolute clock anchors previous frame", fresh.sample(100.125,
        ALAvatarAnimationTime::delta(0.125f, 0.5f, 1.f), false), 100.0625);
}

template<> template<> void avatar_animation_time_object::test<7>()
{
    ALAvatarAnimationTime::DrivenClock source;
    ensure_equals("unit source exact", source.sample(10.0, 1.f, 0.0), 10.0);
    ensure_equals("source half advance", source.sample(10.25, 0.5f, 0.0), 10.125);
    ensure_equals("source freeze", source.sample(20.0, 0.f, 0.0), 10.125);
    ensure_equals("source resume without catch up", source.sample(20.25, 1.f, 0.0), 10.375);
    ensure_equals("backward source remains backward", source.sample(20.0, 0.5f, 0.0), 10.25);
    ALAvatarAnimationTime::DrivenClock loop;
    loop.sample(1.0, 1.f, 0.0);
    ensure_equals("unwrapped source retains long forward step", loop.sample(8.0, 0.5f, 0.0), 4.5);
    ensure_equals("unwrapped source retains multiple laps", loop.sample(38.0, 0.5f, 0.0), 19.5);
}

template<> template<> void avatar_animation_time_object::test<8>()
{
    ALAvatarAnimationTime::DrivenClock clock;
    clock.sample(9.875, 1.f, 0.0);
    const F64 phase = clock.sample(10.125, 0.5f, 0.0);
    ensure_equals("repeat advances through wrap at half speed", phase, 10.0);
    ensure_equals("repeat maps at loop start", ALAvatarAnimationTime::playhead(phase, 10.0, 1), 0.0);
    ensure_equals("pingpong maps at endpoint", ALAvatarAnimationTime::playhead(phase, 10.0, 2), 10.0);
    ensure_equals("pingpong reverses after endpoint", ALAvatarAnimationTime::playhead(10.125, 10.0, 2), 9.875);
    ensure_equals("hold parks at endpoint", ALAvatarAnimationTime::playhead(12.0, 10.0, 0), 10.0);
    ensure_equals("negative repeat phase", ALAvatarAnimationTime::playhead(-0.125, 10.0, 1), 9.875);
}

template<> template<> void avatar_animation_time_object::test<9>()
{
    const F32 legacy = 0.25f;
    ensure("unit damping preserves bits", same(ALAvatarAnimationTime::interpolant(
        legacy, ALAvatarAnimationTime::delta(0.125f, 1.f, 1.f), 0.125f), legacy));
    ensure_equals("frozen damping holds", ALAvatarAnimationTime::interpolant(
        legacy, ALAvatarAnimationTime::delta(0.125f, 0.f, 1.f), 0.125f), 0.f);
    const F32 half = ALAvatarAnimationTime::interpolant(
        legacy, ALAvatarAnimationTime::delta(0.125f, 0.5f, 1.f), 0.125f);
    ensure_distance("two half-clock steps equal one live step",
        1.f - (1.f - half) * (1.f - half), legacy, 1e-6f);
}

template<> template<> void avatar_animation_time_object::test<10>()
{
    const ALPoseSecondary::SecondaryParams params;
    F32 time = runtimePhase(1.375f);
    const auto held = ALPoseSecondary::evalSecondary(time, params);
    for (S32 frame = 0; frame < 60; ++frame)
    {
        const auto off = ALAvatarAnimationTime::lifeDelta(0.015625f, 0.f, 1.f, false);
        ensure("off still paints held pose", ALPoseSecondary::advanceTime(time, off));
        samePose(held, ALPoseSecondary::evalSecondary(time, params));
    }
    const F32 before = time;
    for (S32 frame = 0; frame < 32; ++frame)
    {
        const auto on = ALAvatarAnimationTime::lifeDelta(0.015625f, 0.f, 0.25f, true);
        ensure("on uses exact wall bits", same(on.mSeconds, 0.015625f));
        ALPoseSecondary::advanceTime(time, on);
        ensure_equals("non-life stays frozen", ALAvatarAnimationTime::delta(0.015625f, 0.f, 0.25f).mSeconds, 0.f);
    }
    ensure_equals("keep-life advances phase during world freeze", time, before + 0.5f);
    ensure("breath pose changes", !same(held.mChestPitch, ALPoseSecondary::evalSecondary(time, params).mChestPitch));
    ensure_equals("keep-life bypasses slow world and avatar clocks",
        ALAvatarAnimationTime::lifeDelta(0.125f, 0.5f, 0.25f, true).mSeconds, 0.125f);
    ensure_equals("off respects both speed factors",
        ALAvatarAnimationTime::lifeDelta(0.125f, 0.5f, 0.25f, false).mSeconds, 0.015625f);
}

template<> template<> void avatar_animation_time_object::test<11>()
{
    ALGazeMotor::GazeMotorState state;
    ALGazeMotor::GazeMotorInput in;
    in.mSettings.mMasterGate = true;
    in.mSeed = 42;
    in.mTargetYaw = 0.2f;
    in.mTimeSeconds = 10.0;
    in.mUseLifeTime = true;
    in.mLifeTimeSeconds = 10.0;
    ALGazeMotor::GazeMotorPose before;
    ALGazeMotor::step(state, in, before);
    in.mTimeSeconds = 10.0625;
    in.mLifeTimeSeconds = 10.0625;
    in.mTargetYaw = 0.6f;
    in.mAcquire = true;
    ALGazeMotor::step(state, in, before);
    ALGazeMotor::TargetInput held_input;
    held_input.capture(in);
    const auto retargets = state.mRetargetCounter;
    const F64 target_start = state.mChannels[ALGazeMotor::CH_HEAD_YAW].mProgram.mStartTime;
    bool life_changed = false;
    for (S32 frame = 1; frame <= 32; ++frame)
    {
        auto frozen_input = in;
        held_input.hold(frozen_input, 10.0625 + frame * 0.0625);
        ALGazeMotor::GazeMotorPose after;
        ALGazeMotor::step(state, frozen_input, after);
        ensure_equals("frozen acquire edge is not replayed", state.mRetargetCounter, retargets);
        ensure("target neck clock holds", same(after.mNeckYaw, before.mNeckYaw));
        ensure("target torso clock holds", same(after.mTorsoYaw, before.mTorsoYaw));
        ensure("target roll holds", same(after.mHeadRoll, before.mHeadRoll));
        ensure_equals("target program start unchanged", state.mChannels[ALGazeMotor::CH_HEAD_YAW].mProgram.mStartTime, target_start);
        life_changed |= !same(after.mEyeYawMicro, before.mEyeYawMicro) ||
                        !same(after.mEyePitchMicro, before.mEyePitchMicro);
    }
    ensure("motor ambient life moves at frozen target time", life_changed);
}

template<> template<> void avatar_animation_time_object::test<12>()
{
    const auto unit_zero = ALAvatarAnimationTime::delta(0.f, 1.f, 1.f);
    ensure("zero wall interval retains direct Live path", unit_zero.mUnscaled);
    ensure_equals("Live zero-delta damping uses original interpolant",
        ALAvatarAnimationTime::interpolant(0.25f, unit_zero, 0.f), 0.25f);
    ensure_equals("frozen zero-delta damping holds",
        ALAvatarAnimationTime::interpolant(0.25f, ALAvatarAnimationTime::delta(0.f, 0.f, 1.f), 0.f), 0.f);
}

template<> template<> void avatar_animation_time_object::test<13>()
{
    ALAvatarAnimationTime::DrivenClock clock;
    clock.sample(10.0, 1.f, 0.0);
    clock.sample(11.0, 0.f, 0.0);
    const F64 before = clock.mTime;
    ensure_equals("resume advances source without catch-up jump",
        clock.sample(11.0625, 1.f, 0.0, 0.0625f), before + 0.0625);
    for (S32 i = 1; i <= 8; ++i)
        clock.sample(11.0625 + i * 0.0625, 1.f, 0.0, 0.0625f);
    ensure_equals("recovery reaches live source in half a second", clock.mTime, clock.mSource);
    ensure("unit clears scaled latch", !clock.mScaled);
    ensure_equals("following live source stays exact", clock.sample(20.123456789, 1.f, 0.0, 0.0625f), 20.123456789);
    clock.sample(21.0, 0.5f, 0.0);
    ensure_equals("transport revision resets to authored seek", clock.sample(3.0, 1.f, 0.0, 0.0625f, 1), 3.0);
    ensure("seek drops scaled state", !clock.mScaled);
    clock = {};
    ensure_equals("new assignment starts at its live source", clock.sample(7.0, 1.f, 0.0), 7.0);
}

template<> template<> void avatar_animation_time_object::test<14>()
{
    for (F32 avatar_speed : {0.f, 0.5f, 0.8f, 1.f, 2.f})
    {
        const F64 leader_arc = 14.25;
        const F64 offset = 2.0;
        ensure_equals("follower authored arc ignores controller speed",
            ALAvatarAnimationTime::followerSource(leader_arc, 3.0, 1.f) - offset, leader_arc - offset);
        ensure_equals("world movement ignores controller speed", ALAvatarAnimationTime::worldDelta(0.125f, 1.f).mSeconds, 0.125f);
        ensure_equals("joint animation still respects controller speed",
            ALAvatarAnimationTime::delta(0.125f, 1.f, avatar_speed).mSeconds, 0.125f * avatar_speed);
    }
    ensure_equals("follower holds at world freeze", ALAvatarAnimationTime::followerSource(14.0, 3.0, 0.f), 3.0);
    ensure_equals("follower consumes already slowed leader arc", ALAvatarAnimationTime::followerSource(14.0, 3.0, 0.5f), 14.0);
    ensure_equals("root motion follows slow world", ALAvatarAnimationTime::worldDelta(0.125f, 0.5f).mSeconds, 0.0625f);
}

template<> template<> void avatar_animation_time_object::test<15>()
{
    const ALPoseSecondary::SecondaryParams params;
    F32 phase = 1.375f;
    const auto pose = ALPoseSecondary::evalSecondary(phase, params);
    const auto paused_hitch = ALAvatarAnimationTime::delta(0.5f, 0.f, 1.f);
    ensure("frozen hitch still paints held secondary", ALPoseSecondary::advanceTime(phase, paused_hitch));
    ensure_equals("frozen hitch keeps phase", phase, 1.375f);
    samePose(pose, ALPoseSecondary::evalSecondary(phase, params));
    ensure("frozen hitch must not reseed repair", !ALAvatarAnimationTime::polishReseed(paused_hitch));
    const auto live_zero = ALAvatarAnimationTime::delta(0.f, 1.f, 1.f);
    ensure("Live zero retains old reseed path", !ALPoseSecondary::advanceTime(phase, live_zero));
    ensure_equals("Live zero reseeds to original phase", phase, 0.f);
    for (F32 invalid_wall : {-0.125f, std::numeric_limits<F32>::quiet_NaN(),
                             std::numeric_limits<F32>::infinity()})
    {
        for (F32 rate : {0.5f, 1.f})
        {
            const auto invalid = ALAvatarAnimationTime::delta(invalid_wall, rate, 1.f);
            ensure("invalid wall on a running clock reseeds", ALAvatarAnimationTime::polishReseed(invalid));
            ensure("invalid wall is not a paused clock", !ALAvatarAnimationTime::frozen(invalid));
        }
        const auto frozen_invalid = ALAvatarAnimationTime::delta(invalid_wall, 0.f, 1.f);
        ensure("actual frozen clock ignores invalid wall discontinuity", ALAvatarAnimationTime::frozen(frozen_invalid));
        ensure("actual frozen clock retains repair pose", !ALAvatarAnimationTime::polishReseed(frozen_invalid));
    }
}

template<> template<> void avatar_animation_time_object::test<16>()
{
    ALAvatarAnimationTime::Clock clock;
    clock.sample(100.0, ALAvatarAnimationTime::delta(0.125f, 1.f, 1.f), true);
    clock.sample(101.0, ALAvatarAnimationTime::delta(0.125f, 0.f, 1.f), false);
    ensure_equals("busy program defers reanchor", clock.sample(102.0,
        ALAvatarAnimationTime::delta(0.125f, 1.f, 1.f), true, false), 100.125);
    ensure_equals("safe gaze returns to exact legacy epoch", clock.sample(103.0,
        ALAvatarAnimationTime::delta(0.125f, 1.f, 1.f), true, true), 103.0);
    ensure("safe gaze clears latch", !clock.mScaled);
    ensure("non-animation capture selects legacy presentation", ALAvatarAnimationTime::legacyGazeClock(true, false));
    ensure("animation capture uses animation clock", !ALAvatarAnimationTime::legacyGazeClock(true, true));
    ensure_equals("legacy path follows presentation instead of wall delta", clock.sample(50.0,
        ALAvatarAnimationTime::delta(0.125f, 1.f, 1.f), true, true), 50.0);
}

template<> template<> void avatar_animation_time_object::test<17>()
{
    ALGazeMotor::GazeMotorState state;
    ALGazeMotor::GazeMotorInput in;
    in.mSettings.mMasterGate = true;
    in.mTimeSeconds = 100.0;
    ALGazeMotor::GazeMotorPose pose;
    ALGazeMotor::step(state, in, pose);
    in.mTimeSeconds = 100.0625;
    in.mTargetYaw = 0.6f; in.mAcquire = true;
    ALGazeMotor::step(state, in, pose);
    ALGazeMotor::ClockSafety safety;
    ensure("motor in flight blocks reanchor", !ALGazeMotor::canReanchor(state, safety, 100.0625, 100.0625, 10.0));
    in.mAcquire = false; in.mTimeSeconds = 104.0;
    ALGazeMotor::step(state, in, pose);
    ensure("settled motor permits reanchor", ALGazeMotor::canReanchor(state, safety, 104.0, 104.0, 10.0));
    const auto old = ALTrajectory::sample(state.mChannels[ALGazeMotor::CH_HEAD_YAW].mProgram, 104.0);
    ALGazeMotor::rebaseClock(state, -94.0);
    const auto rebased = ALTrajectory::sample(state.mChannels[ALGazeMotor::CH_HEAD_YAW].mProgram, 10.0);
    ensure("backward epoch jump cannot restart settled head program", same(old.mP, rebased.mP));
    ensure_equals("rebased velocity holds", rebased.mV, old.mV);
    state.mChannels[ALGazeMotor::CH_HEAD_YAW].mPendingValid = true;
    ensure("delayed program blocks reanchor", !ALGazeMotor::canReanchor(state, safety, 10.0, 10.0, 200.0));
}

template<> template<> void avatar_animation_time_object::test<18>()
{
    ALGazeMotor::ClockSafety safety;
    safety.mValid = true;
    safety.mSeed = 42;
    safety.mLife.mBlinks = true;
    safety.mLife.mBlinkRate = 1.f;
    safety.mLife.mBreakFrequency = 1.f;
    ALGazeMotor::GazeMotorState state;
    F64 blink = -1.0, break_time = -1.0, idle = -1.0;
    for (S32 i = 0; i < 4096; ++i)
    {
        const F64 t = i * 0.03125;
        if (ALGazeMath::evalMicroLife(42, t, 1.f, true, 1.f).mBlinkFraction > 0.f) blink = t;
        F32 yaw, pitch, weight;
        ALGazeMath::evalNaturalBreak(42, t, 1.f, yaw, pitch, weight);
        if (weight > 0.f) break_time = t;
        safety.mNaturalBreaks = true;
        if (ALGazeMotor::lifeIdle(state, safety, t)) idle = t;
    }
    ensure("fixture finds blink, break and idle", blink >= 0.0 && break_time >= 0.0 && idle >= 0.0);
    safety.mNaturalBreaks = false;
    ensure("current blink blocks epoch change", !ALGazeMotor::canReanchor(state, safety, idle, blink, idle));
    ensure("destination blink blocks epoch change", !ALGazeMotor::canReanchor(state, safety, idle, idle, blink));
    safety.mNaturalBreaks = true;
    ensure("destination break blocks epoch change", !ALGazeMotor::canReanchor(state, safety, idle, idle, break_time));
    ensure("idle endpoints allow epoch change", ALGazeMotor::canReanchor(state, safety, idle, idle, idle));
}

template<> template<> void avatar_animation_time_object::test<19>()
{
    bool seeded = false, has = false;
    LLVector3 held, target(3.f, 4.f, 2.f);
    auto* sampled = ALAvatarAnimationTime::eyeTarget(true, seeded, has, held, &target);
    ensure("first frozen eye target is captured", sampled && *sampled == target);
    target = LLVector3(-4.f, 2.f, 1.f);
    sampled = ALAvatarAnimationTime::eyeTarget(true, seeded, has, held, &target);
    ensure("subsequent frozen target holds", sampled && sampled->mV[VX] == 3.f);
    ensure("Live returns exact target pointer", ALAvatarAnimationTime::eyeTarget(false, seeded, has, held, &target) == &target);
    ensure("freeze after Live has updated target", *ALAvatarAnimationTime::eyeTarget(true, seeded, has, held, &target) == target);
}

template<> template<> void avatar_animation_time_object::test<20>()
{
    ensure("new frozen constraint seeds once", ALAvatarAnimationTime::seedFrozenConstraint(true, false));
    ensure("seeded frozen constraint replays", !ALAvatarAnimationTime::seedFrozenConstraint(true, true));
    ensure("Live constraint uses normal solver", !ALAvatarAnimationTime::seedFrozenConstraint(false, false));
    bool seeded = false;
    LLVector3 cached;
    LLQuaternion rotation;
    const LLVector3 root(18.f, -12.f, 6.f);
    const LLQuaternion root_rotation(0.5f, LLVector3::z_axis);
    ALAvatarAnimationTime::seedPose(seeded, cached, rotation, root, root_rotation);
    ensure("new frozen move has current non-origin root", cached == root);
    ensure("new frozen move has current rotation", rotation == root_rotation);
    ALAvatarAnimationTime::seedPose(seeded, cached, rotation, LLVector3(), LLQuaternion());
    ensure("repeated frozen paints retain assigned pose", cached == root && rotation == root_rotation);
}

template<> template<> void avatar_animation_time_object::test<21>()
{
    ALGazeMotor::ClockSafety safety;
    safety.mValid = true; safety.mMotor = true; safety.mSeed = 42;
    safety.mBlinkRate = 1.f;
    safety.mCloseMs = ALGazeBlink::DEFAULT_CLOSE_MS;
    safety.mHoldMs = ALGazeBlink::DEFAULT_HOLD_MS;
    safety.mOpenMs = ALGazeBlink::DEFAULT_OPEN_MS;
    safety.mPartialChance = 0.3f;
    safety.mRefractory = 0.5f;
    ALGazeMotor::GazeMotorState state;
    const F32 lag = (ALGazeNoise::unitHash(42, ALGazeMotor::NOISE_CH_ASYM, 0, 0, 0) * 2.f - 1.f) * 0.006f;
    bool found = false;
    for (S32 i = 0; i < 64000 && !found; ++i)
    {
        const F64 t = i * 0.001;
        const auto left = ALGazeMotor::sampleSpontaneousBlink(42, t, 1.f,
            safety.mCloseMs, safety.mHoldMs, safety.mOpenMs, 0.3f, -1.0e18, 0.5f);
        const auto right = ALGazeMotor::sampleSpontaneousBlink(42, t - lag, 1.f,
            safety.mCloseMs, safety.mHoldMs, safety.mOpenMs, 0.3f, -1.0e18, 0.5f);
        if (left.mClosure == 0.f && right.mClosure > 0.f)
        {
            found = true;
            ensure("right-eye-only blink prevents reanchor", !ALGazeMotor::lifeIdle(state, safety, t));
        }
    }
    ensure("fixture finds asymmetric blink edge", found);
    state.mInitialized = true;
    state.mEvokedBlinkStart = 10.0;
    state.mEvokedBlinkDepth = 1.f;
    ALGazeMotor::rebaseClock(state, 0.0);
    ensure_equals("consecutive legacy frames preserve evoked onset", state.mEvokedBlinkStart, 10.0);
    ensure_equals("consecutive legacy frames preserve evoked depth", state.mEvokedBlinkDepth, 1.f);
    const F32 closure = ALGazeBlink::blinkClosure(0.0625,
        safety.mCloseMs, safety.mHoldMs, safety.mOpenMs, state.mEvokedBlinkDepth);
    ALGazeMotor::rebaseClock(state, -8.0);
    ensure("program epoch shift preserves independent in-flight blink",
        same(closure, ALGazeBlink::blinkClosure(10.0625 - state.mEvokedBlinkStart,
            safety.mCloseMs, safety.mHoldMs, safety.mOpenMs, state.mEvokedBlinkDepth)));
}

template<> template<> void avatar_animation_time_object::test<22>()
{
    ALAvatarAnimationTime::OwnerIdentity owner;
    S32 first = 0, second = 0;
    const LLUUID id("12345678-1234-1234-1234-123456789abc");
    const LLUUID other("12345678-1234-1234-1234-123456789abd");
    owner.set(&first, id, 7);
    ensure("current instance owns pointer cache", owner.matches(&first, id, 7));
    ensure("same UUID at different address invalidates", !owner.matches(&second, id, 7));
    ensure("reused address with different UUID invalidates", !owner.matches(&first, other, 7));
    ensure("reused address and UUID with new incarnation invalidates", !owner.matches(&first, id, 8));
    owner.set(&first, id, 8);
    ensure("late departure of old incarnation cannot clear replacement", !owner.matches(&first, id, 7));
    owner = {};
    ensure("removed owner cannot authorize dereference", !owner.matches(&first, id, 8));
}

template<> template<> void avatar_animation_time_object::test<23>()
{
    using namespace ALAvatarAnimationTime;
    for (bool keep_life : {false, true})
    {
        F32 phase = runtimePhase(1.375f);
        const ALPoseSecondary::SecondaryParams params;
        const auto held = ALPoseSecondary::evalSecondary(phase, params);
        const auto animation = delta(0.125f, 0.f, 1.f);
        const bool seed = seedFrozenLayer(frozen(animation), true);
        ensure("frozen rebuild executes a live seed path", seed);
        const auto life = seed ? seedDelta() : lifeDelta(0.125f, 0.f, 1.f, keep_life);
        ensure("seed remains a paintable zero step", ALPoseSecondary::advanceTime(phase, life));
        ensure_equals("rebuild retains prior phase even with keep-life ON", phase, 1.375f);
        samePose(held, ALPoseSecondary::evalSecondary(phase, params));
        ensure("unchanged skeleton replays held pose", !seedFrozenLayer(true, false));
        ensure("Live rebuild takes original path", !seedFrozenLayer(false, true));
    }
}

template<> template<> void avatar_animation_time_object::test<24>()
{
    using namespace ALAvatarAnimationTime;
    for (bool keep_life : {false, true})
    {
        Clock program, life;
        sampleGazeClocks(program, life, 100.0, delta(0.125f, 1.f, 1.f),
            lifeDelta(0.125f, 1.f, 1.f, keep_life), true, false, true);
        for (S32 i = 0; i < 80; ++i)
            sampleGazeClocks(program, life, 100.0 + i * 0.125,
                delta(0.125f, 0.5f, 1.f), lifeDelta(0.125f, 0.5f, 1.f, keep_life), false, false, false);
        const F64 life_before = life.mTime;
        const F32 drift = ALGazeNoise::driftOffset(42, 1, life_before, 1.25f, 0.01f);
        const F32 micro = ALGazeNoise::microsaccadeOffset(42, 2, life_before, 1.5f, 0.01f);
        const auto legacy = ALGazeMath::evalMicroLife(42, life_before, 1.f, true, 1.f);
        const F64 shift = sampleGazeClocks(program, life, 200.0, delta(0.f, 1.f, 1.f),
            delta(0.f, 1.f, 1.f), true, false, true);
        ensure("safe program reanchors", shift != 0.0 && !program.mScaled && program.mTime == 200.0);
        ensure("life stays integrated forever", life.mScaled);
        ensure_equals("reanchor does not move the life epoch", life.mTime, life_before);
        ensure("drift has identical bits on reanchor", same(drift, ALGazeNoise::driftOffset(42, 1, life.mTime, 1.25f, 0.01f)));
        ensure("microsaccade has identical bits on reanchor", same(micro, ALGazeNoise::microsaccadeOffset(42, 2, life.mTime, 1.5f, 0.01f)));
        const auto after = ALGazeMath::evalMicroLife(42, life.mTime, 1.f, true, 1.f);
        ensure("legacy blink closure is continuous", same(legacy.mBlinkFraction, after.mBlinkFraction));
        sampleGazeClocks(program, life, 300.0, delta(0.125f, 1.f, 1.f),
            delta(0.125f, 1.f, 1.f), true, true, true);
        ensure_equals("later legacy-only frame integrates life without epoch jump", life.mTime, life_before + 0.125);
    }
}

template<> template<> void avatar_animation_time_object::test<25>()
{
    enum class Slot { FROM, BASE, EYES, COUNT };
    ALAvatarAnimationTime::SlotSamples<S32, static_cast<size_t>(Slot::COUNT)> samples;
    ensure_equals("FROM captured", samples.sample(Slot::FROM, false, [] { return 11; }), 11);
    ensure_equals("BASE captured", samples.sample(Slot::BASE, false, [] { return 22; }), 22);
    ensure_equals("optional EYES first added during freeze seeds own slot", samples.sample(Slot::EYES, true, [] { return 33; }), 33);
    ensure_equals("reordered BASE cannot consume eye sample", samples.sample(Slot::BASE, true, [] { return 99; }), 22);
    ensure_equals("omitted FROM can return without consuming BASE", samples.sample(Slot::FROM, true, [] { return 99; }), 11);
    ensure_equals("eyes retain own held direction", samples.sample(Slot::EYES, true, [] { return 99; }), 33);
    samples.clear();
    ensure_equals("rebuilt rig seeds new direction", samples.sample(Slot::BASE, true, [] { return 44; }), 44);
}

template<> template<> void avatar_animation_time_object::test<26>()
{
    using namespace ALAvatarAnimationTime;
    DrivenClock clock;
    clock.sample(0.0, 1.f, 0.0);
    clock.sample(10.0, 0.f, 0.0);
    clock.sample(10.0625, 1.f, 0.0, 0.0625f, 0, 1.25);
    ensure("large offset extends half-second recovery", clock.mRecoveryDuration > 0.5);
    for (S32 i = 0; i < 8; ++i)
        clock.sample(clock.mSource + 0.0625, 1.f, 0.0, 0.0625f, 0, 1.25);
    const F32 elapsed = clock.mRecoveryTime;
    const F64 duration = clock.mRecoveryDuration;
    const F64 held = clock.mTime;
    clock.remapSource(clock.mSource, held);
    ensure_equals("metadata remap keeps remaining elapsed ease", clock.mRecoveryTime, elapsed);
    ensure_equals("metadata remap keeps recovery window", clock.mRecoveryDuration, duration);
    ensure_equals("metadata remap keeps displayed sample", clock.mTime, held);
    const F64 before = clock.mTime;
    clock.sample(clock.mSource + 0.0625, 1.f, 0.0, 0.0625f, 0, 0.25);
    ensure("speed drop caps total catch-up", std::fabs(clock.mTime - before) <= 0.25 * 0.0625 + 1.e-10);
    const F64 stopped = clock.mTime;
    clock.sample(clock.mSource + 0.0625, 1.f, 0.0, 0.0625f, 0, 0.0);
    ensure_equals("zero locomotion speed holds recovery", clock.mTime, stopped);
    ensure_equals("authored seek deliberately resets recovery", clock.sample(3.0, 1.f, 0.0, 0.0625f, 1, 1.25), 3.0);
}

template<> template<> void avatar_animation_time_object::test<27>()
{
    using namespace ALAvatarAnimationTime;
    ensure_equals("hold clamps at end", playhead(6.0, 5.0, 0), 5.0);
    ensure_equals("repeat maps modulo", playhead(6.0, 5.0, 1), 1.0);
    ensure_equals("ping-pong reflects", playhead(6.0, 5.0, 2), 4.0);
    for (S32 mode : {0, 1, 2})
        ensure_equals("loop metadata remap retains displayed pose", playhead(sourceCoordinate(3.0, 5.0, mode, 19.0), 5.0, mode), 3.0);
    const F64 before = 29.75;
    const F64 desired = playhead(5.125, 5.0, 1) / 5.0 * 30.0;
    ensure("fixture crosses repeat seam", before - desired > 20.0);
    ensure_equals("mapped repeat seam is physically bounded", boundedRecoveryArc(desired, before, 0.5), before - 0.5);
}

template<> template<> void avatar_animation_time_object::test<28>()
{
    using namespace ALAvatarAnimationTime;
    const F32 wall = runtimePhase(0.017f), presentation = runtimePhase(0.031f);
    ensure("ANIMATION capture preserves presentation-delta bits", same(placementSeconds(wall, 1.f, presentation, true), presentation));
    ensure("joint program at avatar speed one preserves presentation bits", same(animationSeconds(wall, 1.f, 1.f, presentation, true), presentation));
    ensure_equals("reverse capture keeps old zero traversal", placementSeconds(wall, 1.f, -0.02f, true), 0.f);
    ensure_equals("scrub capture keeps zero traversal", placementSeconds(wall, 1.f, 0.f, true), 0.f);
    ensure("Live preserves wall delta bits", same(placementSeconds(wall, 1.f, presentation, false), wall));
    ensure_equals("world slow still applies without presentation drive", placementSeconds(0.125f, 0.5f, 0.2f, false), 0.0625f);
    ensure_equals("unclamped ghost Live path retains large dt", placementSeconds(0.5f, 1.f, 0.2f, false, false), 0.5f);
    ensure_equals("phase clock holds reverse scrub", gazePhaseDelta(delta(wall, 1.f, 1.f), 0.f, true, 1.f).mSeconds, 0.f);
    ensure_equals("phase clock preserves full presentation interval", gazePhaseDelta(delta(wall, 1.f, 1.f), 0.5f, true, 1.f).mSeconds, 0.5f);
    ensure_equals("phase clock applies only avatar factor to driven delta", gazePhaseDelta(delta(wall, 0.5f, 0.5f), 0.125f, true, 0.5f).mSeconds, 0.0625f);
    ensure("Ghost unit zero executes original near-target path", !holdWorldTurn(0.f, 1.f));
    ensure("Ghost paused world holds", holdWorldTurn(0.f, 0.f));
}

template<> template<> void avatar_animation_time_object::test<29>()
{
    using namespace ALAvatarAnimationTime;
    for (S32 mode : {1, 2})
    {
        DrivenClock clock;
        F64 source = 4.5, displayed = 27.0;
        clock.sample(source, 1.f, source);
        for (S32 i = 0; i < 40; ++i)
        {
            source += 0.0625;
            clock.sample(source, 0.f, source);
        }
        bool landed = false;
        for (S32 i = 0; i < 4000 && !landed; ++i)
        {
            source += 0.0625;
            const F64 max_arc_speed = recoveryArcSpeed(1.0, source - clock.mSource, 0.0625f, 30.0, 5.0);
            ensure_equals("sync speed cap accounts for faster take", max_arc_speed, 7.5);
            const F64 sampled = clock.sample(source, 1.f, 0.0, 0.0625f, 0, max_arc_speed * 5.0 / 30.0);
            const F64 desired = playhead(sampled, 5.0, mode) / 5.0 * 30.0;
            const F64 bounded = boundedRecoveryArc(desired, displayed, max_arc_speed * 0.0625);
            ensure("mapped recovery never sprints, including seam", std::fabs(bounded - displayed) <= max_arc_speed * 0.0625 + 1.e-10);
            displayed = bounded;
            if (bounded != desired)
            {
                clock.remapSource(source, sourceCoordinate(bounded / 30.0 * 5.0, 5.0, mode, source));
                clock.mScaled = clock.mRecovering = true;
            }
            landed = !clock.mScaled;
        }
        ensure("faster looping take can still be caught", landed);
        ensure("landing matches source pose", std::fabs(displayed - playhead(source, 5.0, mode) / 5.0 * 30.0) < 1.e-7);
    }
}

template<> template<> void avatar_animation_time_object::test<30>()
{
    using namespace ALAvatarAnimationTime;
    const F32 legacy = runtimePhase(0.25f), wall = 0.125f;
    const LLQuaternion old_pose;
    const LLQuaternion ik_target(0.8f, LLVector3::y_axis);
    // Match HEAD editing motion: first endpoint is SOLVED, second is OLD.
    const auto paint = [&](F32 rate) {
        const F32 retention = retainedInterpolant(legacy, delta(wall, rate, 1.f), wall);
        return slerp(retention, ik_target, old_pose);
    };
    const auto angle = [](const LLQuaternion& q) {
        F32 radians; LLVector3 axis; q.getAngleAxis(&radians, axis); return radians;
    };
    const auto live = paint(1.f);
    const auto base = slerp(legacy, ik_target, old_pose);
    ensure("unit retention is bit-identical", same(retainedInterpolant(legacy, delta(wall, 1.f, 1.f), wall), legacy));
    for (S32 i = 0; i < 4; ++i) ensure("unit IK output matches HEAD bits", same(live.mQ[i], base.mQ[i]));
    ensure("quarter speed moves less toward IK target", angle(paint(0.25f)) < angle(live));
    ensure("double speed moves further toward IK target", angle(paint(2.f)) > angle(live));
    const auto paused = paint(0.f);
    for (S32 i = 0; i < 4; ++i) ensure("freeze retains old pose bits", same(paused.mQ[i], old_pose.mQ[i]));
    const F32 quarter = retainedInterpolant(legacy, delta(wall, 0.25f, 1.f), wall);
    ensure_distance("four quarter steps equal one live decay", quarter * quarter * quarter * quarter, legacy, 1e-6f);
    ensure_distance("double step has twice the decay exponent", retainedInterpolant(legacy, delta(wall, 2.f, 1.f), wall), legacy * legacy, 1e-6f);
}

template<> template<> void avatar_animation_time_object::test<31>()
{
    using namespace ALAvatarAnimationTime;
    bool active = true;
    LLUUID source("12345678-1234-1234-1234-123456789abc");
    const LLUUID expected = source;
    S32 releases = 0;
    const auto release = [&](const LLUUID& canonical) {
        ensure("AO ownership consumed before callback", !active);
        source.setNull(); // a reentrant reset cannot destroy the owned sample
        ensure_equals("release uses canonical source snapshot", canonical, expected);
        ++releases;
        releaseTurnOverride(active, source, [&](const LLUUID&) { ++releases; });
    };
    releaseTurnOverride(active, source, release);
    ensure_equals("owner mismatch/erase balances once", releases, 1);
    releaseTurnOverride(active, source, release);
    ensure_equals("markDead plus destructor is idempotent", releases, 1);
}

template<> template<> void avatar_animation_time_object::test<32>()
{
    ALPoseContinuity::InertiaJoint inertia;
    LLQuaternion raw(0.8f, LLVector3::y_axis), rot = raw;
    LLVector3 pos;
    ALPoseContinuity::inertialize(inertia, rot, pos, 0.125f, 0.1f, 0.f, false);
    raw = LLQuaternion(0.1f, LLVector3::y_axis); rot = raw;
    ALPoseContinuity::inertialize(inertia, rot, pos, 0.f, 0.1f, 0.f, true);
    ensure("old rig can carry nonzero offset at zero dt", !inertia.mSettled);
    ensure("old offset changes new raw input", std::fabs(dot(rot, raw)) < 0.99f);
    ALPoseContinuity::resetJoint(inertia);
    ensure("rig invalidation drops stale inertia", !inertia.mValid && inertia.mSettled);
    rot = raw;
    ALPoseContinuity::inertialize(inertia, rot, pos, 0.f, 0.1f, 0.f, false);
    for (S32 i = 0; i < 4; ++i) ensure("new rig zero-step seeds raw bits", same(rot.mQ[i], raw.mQ[i]));
    ALContactStab::ContactFoot foot;
    foot.mValid = foot.mPlanted = true;
    foot.mLockPos = LLVector3(100.f, 20.f, 3.f);
    ALContactStab::resetFoot(foot);
    const LLVector3 new_foot(2.f, 4.f, 6.f);
    bool planted = true;
    const auto held = ALContactStab::updateFoot(foot, {}, new_foot, 6.f, 0.f, planted);
    ensure("new foot cannot inherit old lock at dt0", !planted && held == new_foot);
}

template<> template<> void avatar_animation_time_object::test<33>()
{
    using namespace ALAvatarAnimationTime;
    DrivenClock clock;
    clock.sample(10.0, 1.f, 0.0);
    clock.sample(10.125, 0.5f, 0.0);
    const F64 held = clock.mTime;
    clock.holdSource(10.25); // presentationDelta == 0, source still moves
    ensure_equals("zero presentation frame holds scaled phase", clock.mTime, held);
    ensure_equals("zero frame consumes source baseline", clock.mSource, 10.25);
    ensure_equals("resume integrates only the new interval", clock.sample(10.375, 0.5f, 0.0), held + 0.0625);
    clock.sample(10.5, 1.f, 0.0, 0.125f);
    clock.sample(10.625, 1.f, 0.0, 0.125f);
    const F64 phase = clock.mTime, offset = clock.mRecoveryOffset, duration = clock.mRecoveryDuration;
    const F32 elapsed = clock.mRecoveryTime;
    ensure("fixture has in-flight recovery", clock.mRecovering);
    DrivenClock reference = clock;
    clock.holdSource(11.625);
    ensure_equals("zero frame holds recovery phase", clock.mTime, phase);
    ensure_equals("zero frame holds recovery offset", clock.mRecoveryOffset, offset);
    ensure_equals("zero frame holds recovery duration", clock.mRecoveryDuration, duration);
    ensure_equals("zero frame holds recovery elapsed", clock.mRecoveryTime, elapsed);
    ensure("zero frame retains recovery latch", clock.mRecovering);
    const F64 reference_phase = reference.sample(10.75, 1.f, 0.0, 0.125f);
    const F64 resumed_phase = clock.sample(11.75, 1.f, 0.0, 0.125f);
    ensure_distance("recovery does not replay skipped source progress", resumed_phase, reference_phase, 1e-9);
    clock.sample(11.875, 0.f, 0.0);
    const F64 frozen_phase = clock.mTime;
    clock.holdSource(12.75);
    ensure_equals("world freeze also observes source without integration", clock.mTime, frozen_phase);
}

template<> template<> void avatar_animation_time_object::test<34>()
{
    using namespace ALAvatarAnimationTime;
    ensure("unit zero wall frame still tracks crowd target", !frozen(worldDelta(0.f, 1.f)));
    ensure("world pause holds crowd target", frozen(worldDelta(0.125f, 0.f)));
    ensure("slow zero frame holds crowd tracking", frozen(worldDelta(0.f, 0.25f)));
}

template<> template<> void avatar_animation_time_object::test<35>()
{
    using namespace ALAvatarAnimationTime;
    Clock old_clock, replacement;
    old_clock.sample(100.0, delta(0.125f, 1.f, 1.f), true);
    replacement.sample(130.0, delta(0.125f, 1.f, 1.f), true);
    F64 acquire_start = 99.9;
    acquire_start += replacement.epochOffset() - old_clock.epochOffset();
    ensure_equals("unit owner replacement preserves reaction timestamp across absence", acquire_start, 99.9);
    old_clock.mTime = 90.0;
    ensure_equals("scaled owner correction excludes time absent", replacement.epochOffset() - old_clock.epochOffset(), 10.0);
    acquire_start = 89.0;
    acquire_start += replacement.epochOffset() - old_clock.epochOffset();
    ensure_equals("scaled epoch correction retains elapsed absence", replacement.mTime - acquire_start, 31.0);
    DrivenClock direct;
    direct.sample(10.0, 1.f, 0.0);
    direct.holdSource(11.0);
    ensure_equals("direct zero frame holds phase", direct.mTime, 10.0);
    ensure("direct clock marks held source gap for continuous recovery", direct.mScaled);
    ensure_equals("first unit resume consumes only current interval", direct.sample(11.125, 1.f, 0.0, 0.125f), 10.125);
    for (S32 i = 0; i < 8; ++i)
        direct.sample(direct.mSource + 0.125, 1.f, 0.0, 0.125f);
    ensure("unit source reanchors after recovering held gap", !direct.mScaled && direct.mTime == direct.mSource);
}

template<> template<> void avatar_animation_time_object::test<36>()
{
    using namespace ALAvatarAnimationTime;
    DrivenClock clock;
    clock.sample(10.0, 1.f, 0.0);
    clock.sample(11.0, 0.f, 0.0);
    clock.sample(11.125, 1.f, 0.0, 0.125f);
    clock.sample(11.25, 1.f, 0.0, 0.125f);
    DrivenClock reference = clock;
    clock.holdSource(12.25);
    const F64 held = clock.mTime;
    const F32 elapsed = clock.mRecoveryTime;
    clock.remapSource(12.25, held);
    reference.remapSource(12.25, held);
    ensure_equals("metadata remap consumes already-rebased held gap", clock.mHeldProgress, 0.0);
    ensure_equals("metadata edit keeps elapsed recovery", clock.mRecoveryTime, elapsed);
    ensure_equals("metadata edit keeps held display", clock.mTime, held);
    ensure_distance("resume cannot subtract a held gap twice",
        clock.sample(12.375, 1.f, 0.0, 0.125f), reference.sample(12.375, 1.f, 0.0, 0.125f), 1e-9);
}

template<> template<> void avatar_animation_time_object::test<37>()
{
    using namespace ALAvatarAnimationTime;
    DrivenClock clock;
    clock.holdSource(12.5, 3.25, 9);
    ensure_equals("new frozen path seeds displayed phase", clock.mTime, 3.25);
    ensure_equals("new frozen path observes current source", clock.mSource, 12.5);
    ensure_equals("frozen assignment retains authored revision", clock.mRevision, U64(9));
    ensure("frozen assignment has valid held phase", clock.mValid && clock.mScaled);
    clock.holdSource(12.75, 99.0, 9);
    ensure_equals("subsequent zero frames never reseed valid phase", clock.mTime, 3.25);
    ensure_equals("new frozen assignment resumes continuously", clock.sample(12.875, 1.f, 0.0, 0.125f, 9, 1.25), 3.375);
    bool landed = false;
    for (S32 i = 0; i < 1600 && !landed; ++i)
    {
        const F64 before = clock.mTime;
        clock.sample(clock.mSource + 0.125, 1.f, 0.0, 0.125f, 9, 1.25);
        ensure("new assignment catch-up remains bounded", std::fabs(clock.mTime - before) <= 1.25 * 0.125 + 1e-9);
        landed = !clock.mScaled;
    }
    ensure("new assignment eventually relocks to source", landed && clock.mTime == clock.mSource);
}

template<> template<> void avatar_animation_time_object::test<38>()
{
    using namespace ALAvatarAnimationTime;
    DrivenClock clock;
    clock.sample(4.0, 0.5f, 4.0);
    clock.seekSource(20.0, 7);
    ensure_equals("authored seek at half speed snaps to actual source", clock.mTime, 20.0);
    ensure_equals("source baseline changes with seek", clock.mSource, 20.0);
    ensure("authored seek discards old recovery", !clock.mRecovering && !clock.mScaled);
    const F64 sampled = clock.sample(20.0, 0.5f, 4.0, 0.125f, 7);
    ensure_equals("scaled first seek frame cannot fall back to actor arc", sampled, 20.0);
    ensure_equals("seek uses HEAD arc mapping", sampled / 30.0 * 60.0, 40.0);
    ensure_equals("post-seek intervals remain half speed", clock.sample(20.25, 0.5f, 0.0, 0.125f, 7), 20.125);
    clock.seekSource(8.0, 8);
    clock.holdSource(8.0, 20.125, 8);
    ensure_equals("authored frozen seek snaps on static frame", clock.mTime, 8.0);
    ensure("authored tiny/zero-time seek still snaps facing/clip baseline", pathSeek(true, 0.f, 0.01f, 2.f));
    ensure("fresh frozen placement does not invent scrub history", !pathSeek(false, 0.f, 30.f, 2.f));
    ensure("ordinary large movement keeps HEAD seek threshold", pathSeek(false, 0.125f, 3.f, 2.f));
}

template<> template<> void avatar_animation_time_object::test<39>()
{
    using namespace ALAvatarAnimationTime;
    ALPathGeometry::Primitive path;
    path.mType = ALPathGeometry::CIRCLE_ARC;
    path.mCenter = LLVector3d(100.0, -30.0, 5.0);
    path.mRadiusX = path.mRadiusY = 10.f;
    LLVector3d placed(5.0, 5.0, 3.0);
    const auto paint = [&](F32 dt, bool evaluated) {
        if (placementNeedsSample(dt, evaluated))
        {
            placed = ALPathGeometry::evaluate(path, 0.0);
            placed.mdV[VZ] += 1.75; // authored root-above-ground
        }
    };
    paint(0.f, false);
    ensure_distance("new paused path samples authored X immediately", placed.mdV[VX], 110.0, 1e-6);
    ensure_distance("new paused path samples authored Y immediately", placed.mdV[VY], -30.0, 1e-6);
    ensure_distance("new paused path uses authored standing height", placed.mdV[VZ], 6.75, 1e-6);
    const auto frozen_pose = placed;
    paint(0.f, true);
    ensure("established pause retains placement", placed == frozen_pose);
    paint(0.125f, true);
    ensure("first moving frame has no delayed placement snap", placed == frozen_pose);
    ensure("explicit seek may resample while paused", placementNeedsSample(0.f, true, true));
    // clearFollow retains an own-path Move but invalidates leader placement.
    bool evaluated = true;
    evaluated = false;
    placed = LLVector3d(-40.0, 90.0, 3.0);
    paint(0.f, evaluated);
    ensure("paused clearFollow samples own path before resume", placed == frozen_pose);
}

template<> template<> void avatar_animation_time_object::test<40>()
{
    using namespace ALAvatarAnimationTime;
    ALPathGeometry::Primitive path;
    path.mType = ALPathGeometry::CIRCLE_ARC;
    path.mCenter = LLVector3d(100.0, -30.0, 5.0);
    path.mRadiusX = path.mRadiusY = 10.f;
    const F64 offset = 5.0, length = 40.0;
    const F64 leader = followerSource(15.0, offset, 0.f, true);
    const F64 arc = leader - offset;
    auto pose = ALPathGeometry::evaluate(path, arc / length);
    pose.mdV[VZ] += 2.0;
    ensure_distance("new paused follower samples current leader arc", arc, 10.0, 1e-9);
    ensure_distance("follower is placed on leader curve while paused", pose.mdV[VY], -20.0, 1e-5);
    ensure_distance("follower keeps its own standing height", pose.mdV[VZ], 7.0, 1e-9);
    ensure_equals("established follower still holds global0", followerSource(19.0, arc + offset, 0.f, false) - offset, arc);
    ensure_equals("normal follower keeps authored relationship", followerSource(15.0, arc + offset, 1.f, false) - offset, arc);
}

template<> template<> void avatar_animation_time_object::test<41>()
{
    using namespace ALAvatarAnimationTime;
    const LLVector3 initial[3] = {LLVector3(), LLVector3(0.72f, 0.68f, 0.f), LLVector3(1.5f, 0.f, 0.f)};
    LLVector3 normal[3], seeded[3], untouched[3];
    std::copy(std::begin(initial), std::end(initial), normal);
    std::copy(std::begin(initial), std::end(initial), seeded);
    std::copy(std::begin(initial), std::end(initial), untouched);
    const F32 lengths[2] = {1.f, 1.f};
    LLVector3 normal_velocity[2], seed_velocity[2], paused_velocity[2];
    ALAvatarConstraint::relax(normal, normal_velocity, lengths, 2, 20, true, 1.f);
    ALAvatarConstraint::relax(seeded, seed_velocity, lengths, 2, 20,
        ALAvatarConstraint::fullStep(false, true), 0.f);
    ALAvatarConstraint::relax(untouched, paused_velocity, lengths, 2, 20,
        ALAvatarConstraint::fullStep(false, false), 0.f);
    ensure("fixture needs a nontrivial spatial solve", std::fabs(dist_vec(initial[1], initial[2]) - 1.f) > 0.03f);
    for (S32 i = 0; i < 2; ++i)
        ensure("frozen seed reaches normal-step chain residual", std::fabs(dist_vec(seeded[i], seeded[i+1]) - lengths[i]) < 0.01f);
    for (S32 j = 0; j < 3; ++j)
        for (S32 axis = 0; axis < 3; ++axis)
        {
            ensure("full frozen seed matches native spatial bits", same(seeded[j].mV[axis], normal[j].mV[axis]));
            ensure("ordinary paused solver never advances", same(untouched[j].mV[axis], initial[j].mV[axis]));
        }
    const LLVector3 held = seeded[1];
    for (S32 frame = 0; frame < 120; ++frame)
    {
        if (seedFrozenConstraint(true, true))
            ALAvatarConstraint::relax(seeded, seed_velocity, lengths, 2, 20, true, 1.f);
        for (S32 axis = 0; axis < 3; ++axis) ensure("constrained pose stays identical across frozen frames", same(seeded[1].mV[axis], held.mV[axis]));
    }
}

template<> template<> void avatar_animation_time_object::test<42>()
{
    const ALContactStab::ContactParams params;
    ALContactStab::ContactFoot cold;
    const LLVector3 first(1.f, 2.f, 3.f);
    bool planted = true;
    ensure("cold zero-dt contact seeds raw position", ALContactStab::updateFoot(cold, params, first, 3.f, 0.f, planted) == first && !planted);
    cold.mPlanted = true;
    cold.mLockPos = first;
    cold.mBelowSpeedSec = 0.04f; cold.mAboveSpeedSec = 0.02f;
    const auto before = cold;
    const LLVector3 moved(8.f, 9.f, 10.f);
    ensure("warm zero-dt contact keeps planted lock", ALContactStab::updateFoot(cold, params, moved, 10.f, 0.f, planted) == first && planted);
    ensure("zero dt does not compute/change velocity reference", cold.mLastPos == before.mLastPos);
    ensure_equals("zero dt holds plant timer", cold.mBelowSpeedSec, before.mBelowSpeedSec);
    ensure_equals("zero dt holds release timer", cold.mAboveSpeedSec, before.mAboveSpeedSec);
}

template<> template<> void avatar_animation_time_object::test<43>()
{
    ALGazeMotor::GazeMotorState state;
    state.mInitialized = true;
    state.mCandidateSince = 40.0;
    state.mEvokedBlinkStart = 7.0;
    for (S32 i = 0; i < ALGazeMotor::CHANNEL_COUNT; ++i)
    {
        auto& channel = state.mChannels[i];
        channel.mPendingValid = true;
        channel.mProgram.mStartTime = 42.0 + i;
        channel.mPending.mStartTime = 52.0 + i;
        for (S32 j = 0; j < 2; ++j)
        {
            channel.mProgram.mSegments[j].mStartTime = 42.0 + i + j;
            channel.mPending.mSegments[j].mStartTime = 52.0 + i + j;
            channel.mProgram.mSegments[j].mDuration = 0.25;
            channel.mPending.mSegments[j].mDuration = 0.5;
        }
        channel.mProgram.mStageOffset = 0.25;
        channel.mProgram.mEndOffset = 0.5;
        channel.mPending.mStageOffset = 0.5;
        channel.mPending.mEndOffset = 1.0;
    }
    ALGazeMotor::rebaseClock(state, -30.0);
    ensure_equals("candidate onset rebases with program epoch", state.mCandidateSince, 10.0);
    ensure_equals("life blink onset stays on independent epoch", state.mEvokedBlinkStart, 7.0);
    for (S32 i = 0; i < ALGazeMotor::CHANNEL_COUNT; ++i)
    {
        const auto& channel = state.mChannels[i];
        ensure_equals("active program rebased", channel.mProgram.mStartTime, 12.0 + i);
        ensure_equals("pending program rebased", channel.mPending.mStartTime, 22.0 + i);
        for (S32 j = 0; j < 2; ++j)
        {
            ensure_equals("active segment epoch rebased", channel.mProgram.mSegments[j].mStartTime, 12.0 + i + j);
            ensure_equals("pending segment epoch rebased", channel.mPending.mSegments[j].mStartTime, 22.0 + i + j);
            ensure_equals("active segment duration unchanged", channel.mProgram.mSegments[j].mDuration, 0.25);
            ensure_equals("pending segment duration unchanged", channel.mPending.mSegments[j].mDuration, 0.5);
        }
        ensure_equals("active stage offset unchanged", channel.mProgram.mStageOffset, 0.25);
        ensure_equals("active end offset unchanged", channel.mProgram.mEndOffset, 0.5);
        ensure_equals("pending stage offset unchanged", channel.mPending.mStageOffset, 0.5);
        ensure_equals("pending end offset unchanged", channel.mPending.mEndOffset, 1.0);
    }
}

template<> template<> void avatar_animation_time_object::test<44>()
{
    using namespace ALAvatarAnimationTime;
    F64 progress = 24.0, offset = 0.0;
    F32 direction = 1.f;
    S32 mode = 1;
    U64 revision = 7, mapping = 3;
    DrivenClock clock;
    clock.sample(24.0, 0.5f, 20.0, 0.125f, revision);
    const auto before = clock;
    startPlaybackProgress(true, 4.f, 10.f, 1, progress, mode, direction, offset, revision, mapping);
    ensure_equals("ordinary resume is not an authored seek", revision, U64(7));
    ensure_equals("unchanged repeat mapping is not revised", mapping, U64(3));
    ensure_equals("resume keeps unwrapped lap count", progress, 24.0);
    ensure_equals("scaled resume does not snap to source", clock.sample(progress, 0.5f, 4.0, 0.f, revision), before.mTime);
    ensure_equals("next interval still scales by half", clock.sample(progress + 0.25, 0.5f, 0.0, 0.125f, revision), before.mTime + 0.125);
    clock.sample(24.5, 1.f, 0.0, 0.125f, revision);
    clock.sample(24.75, 1.f, 0.0, 0.125f, revision);
    const auto recovering = clock;
    progress = 24.75;
    startPlaybackProgress(true, 4.75f, 10.f, 1, progress, mode, direction, offset, revision, mapping);
    clock.sample(progress, 1.f, 0.0, 0.f, revision);
    ensure("resume retains active ease", clock.mRecovering && clock.mScaled);
    ensure_equals("resume retains ease elapsed", clock.mRecoveryTime, recovering.mRecoveryTime);
    ensure_equals("resume retains ease offset", clock.mRecoveryOffset, recovering.mRecoveryOffset);
    ensure_equals("resume retains displayed arc", clock.mTime, recovering.mTime);
    startPlaybackProgress(false, 4.f, 10.f, 1, progress, mode, direction, offset, revision, mapping);
    ensure_equals("fresh playback revises authored transport", revision, U64(8));
    ensure_equals("fresh playback starts at actual playhead", progress, 4.0);
    ensure_equals("fresh metadata is ready before first update", mode, S32(1));
    clock.seekSource(progress, revision);
    ensure_equals("fresh scaled playback seeks", clock.sample(progress, 0.5f, 20.0, 0.f, revision), 4.0);
    startPlaybackProgress(false, 0.f, 10.f, 1, progress, mode, direction, offset, revision, mapping);
    ensure_equals("end-reset playback is a real restart", revision, U64(9));
    ensure_equals("end-reset metadata starts at zero", progress, 0.0);
    startPlaybackProgress(false, 0.f, 0.f, 2, progress, mode, direction, offset, revision, mapping);
    ensure_equals("single-key take does not fold a zero duration", progress, 0.0);
}

template<> template<> void avatar_animation_time_object::test<45>()
{
    using namespace ALAvatarAnimationTime;
    for (S32 changed = 0; changed < 2; ++changed)
    {
        F64 progress = changed ? 13.0 : 14.0, offset = 0.0;
        F32 direction = 1.f;
        S32 mode = changed ? 1 : 2;
        U64 revision = 7, mapping = 3;
        F32 camera = changed ? 3.f : 6.f;
        startPlaybackProgress(true, camera, 10.f, 2, progress, mode, direction, offset, revision, mapping);
        ensure_equals("ping-pong resume is not an authored seek", revision, U64(7));
        ensure_equals("changed orientation maps once", mapping, U64(4));
        ensure_equals("raw unwrapped source preserved", progress, changed ? 13.0 : 14.0);
        ensure_equals("new reverse-leg source runs backward", direction, -1.f);
        ensure_equals("paused loop edit maps effective coordinate", offset, changed ? 4.0 : 0.0);
        F32 camera_direction = 1.f; // HEAD always restarts forward on resume.
        for (S32 frame = 0; frame < 120; ++frame)
        {
            progress += direction == 1.f ? 0.25f : -0.25f;
            camera += 0.25f * camera_direction;
            if (camera >= 10.f || camera <= 0.f)
            {
                camera = camera >= 10.f ? 20.f - camera : -camera;
                camera_direction = -camera_direction;
            }
            camera = llclamp(camera, 0.f, 10.f);
            ensure_equals("source metadata follows unchanged HEAD camera across bounces", playhead(progress + offset, 10.0, 2), F64(camera));
            if (changed && frame == 19)
            {
                // Raw 8 and effective 12 are on opposite ping-pong legs.
                startPlaybackProgress(true, camera, 10.f, 2, progress, mode, direction, offset, revision, mapping);
                camera_direction = 1.f;
                ensure_equals("second resume derives direction from effective phase", direction, -1.f);
                ensure_equals("second resume retains mapping offset", offset, 4.0);
                ensure_equals("second resume still preserves authored revision", revision, U64(7));
            }
        }
    }
}

template<> template<> void avatar_animation_time_object::test<46>()
{
    const F32 stale = runtimePhase(0.125f);
    const F32 sampled = runtimePhase(0.375f);
    ensure("normal frame preserves exact sampled weight bits", same(ALAvatarConstraint::seedWeight(sampled, 1.f, false), sampled));
    const F32 seed = ALAvatarConstraint::seedWeight(stale, 1.f, true);
    ensure_equals("reachable frozen seed has steady constraint weight", seed, 1.f);
    ensure_equals("unreachable frozen seed has steady release weight", ALAvatarConstraint::seedWeight(1.f, 0.f, true), 0.f);
    const LLVector3 raw(0.f, 0.f, 0.f), solved(1.f, 2.f, 3.f);
    const LLVector3 painted = raw + (solved - raw) * seed;
    ensure("seed paints constrained chain fully", painted == solved);
    for (S32 frame = 0; frame < 120; ++frame)
        ensure("held seeded constraint never drifts", raw + (solved - raw) * seed == painted);
}

}
