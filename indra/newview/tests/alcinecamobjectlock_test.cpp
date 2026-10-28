/** Main-camera Object Lock regressions. No avatar, renderer or GL fixture. */
#include "linden_common.h"
#include "../test/lltut.h"
#include "../alcinecamobjectlock.h"
#include "../llcinematiccamera.h"

namespace tut
{
using namespace ALCineCamObjectLock;
using namespace ALVCamBonePovMath;

namespace
{
bool closeVector(const LLVector3& first, const LLVector3& second, F32 epsilon = 2e-5f)
{
    return dist_vec(first, second) <= epsilon;
}

bool closeRotation(const LLQuaternion& first, const LLQuaternion& second, F32 epsilon = 2e-5f)
{
    return closeVector(LLVector3::x_axis * first, LLVector3::x_axis * second, epsilon) &&
        closeVector(LLVector3::y_axis * first, LLVector3::y_axis * second, epsilon) &&
        closeVector(LLVector3::z_axis * first, LLVector3::z_axis * second, epsilon);
}

PoseHistory settled(S32 fps)
{
    Settings settings;
    settings.mSmoothing = .4f;
    PoseHistory state;
    ObjectRenderPose pose;
    state.sample(pose, LLVector3d::zero, settings, 0.0);
    pose.mPosition.setVec(10.f, 0.f, 0.f);
    pose.mRotation = LLQuaternion(1.f, LLVector3::z_axis);
    for (S32 frame = 1; frame <= fps; ++frame)
        state.sample(pose, LLVector3d::zero, settings,
                     static_cast<F64>(frame) / static_cast<F64>(fps));
    return state;
}
}

// Same camera publication API as LLViewerCamera, without GL/message globals.
struct PresentedCamera
{
    LLVector3 mPosition;
    LLVector3 mXAxis, mYAxis, mZAxis;
    F32 mFov = .7f;
    S32 mBroadcasts = 0;
    S32 mLocalViews = 0;
    F32 getMinView() const { return .1f; }
    F32 getMaxView() const { return 3.f; }
    void setView(F32 fov) { mFov = fov; ++mBroadcasts; }
    void setViewNoBroadcast(F32 fov) { mFov = fov; ++mLocalViews; }
    void setOrigin(const LLVector3& position) { mPosition = position; }
};

struct alcinecamobjectlock_data {};
typedef test_group<alcinecamobjectlock_data> object_lock_group;
typedef object_lock_group::object object_lock_object;
object_lock_group object_lock_tests("alcinecamobjectlock");

template<> template<> void object_lock_object::test<1>()
{
    set_test_name("mount defaults and append-only mode numbering");
    Settings settings;
    ensure("default up mount", closeVector(settings.mOffset, LLVector3(0.f, 0.f, .3f)));
    ensure("yaw only and rigid", settings.axes() == 1 && settings.mSmoothing == 0.f);
    ensure("zero trims", settings.mAimYaw == 0.f && settings.mAimPitch == 0.f);
    ensure_equals("Bone Lock stable", static_cast<S32>(LLCinematicCamera::MODE_BONE_LOCK), 1);
    ensure_equals("Pedro stable", static_cast<S32>(LLCinematicCamera::MODE_PEDRO_BOB), 51);
    ensure_equals("Object Lock appended", static_cast<S32>(LLCinematicCamera::MODE_OBJECT_LOCK), 52);
}

template<> template<> void object_lock_object::test<2>()
{
    set_test_name("spin the bottle yaw stays level over multiple turns");
    Settings settings;
    PoseHistory state;
    ObjectRenderPose pose;
    for (S32 frame = 0; frame < 2880; ++frame)
    {
        const F32 yaw = static_cast<F32>(frame) * DEG_TO_RAD;
        pose.mRotation = composeObjectRotation(yaw, .3f, -.4f);
        ensure("sample without avatar", state.sample(pose, LLVector3d::zero, settings,
            static_cast<F64>(frame) / 60.0));
        ensure("tracks heading", closeRotation(state.mRotation,
            LLQuaternion(yaw, LLVector3::z_axis)));
        ensure("horizon level", closeVector(LLVector3::z_axis * state.mRotation, LLVector3::z_axis));
    }
}

template<> template<> void object_lock_object::test<3>()
{
    set_test_name("full ride-along including gimbal band uses rendered quaternion");
    Settings settings;
    settings.mFollowPitch = settings.mFollowRoll = true;
    PoseHistory state;
    ObjectRenderPose pose;
    for (F32 pitch : {0.f, F_PI_BY_TWO - 1e-5f, F_PI_BY_TWO, -F_PI_BY_TWO, 2.f})
    {
        pose.mRotation = composeObjectRotation(.8f, pitch, -.6f);
        ensure("full mount sample", state.sample(pose, LLVector3d::zero, settings, 1.0));
        ensure("all basis axes follow", closeRotation(state.mRotation, pose.mRotation));
        LLQuaternion normalized = pose.mRotation;
        normalized.normalize();
        for (S32 component = 0; component < 4; ++component)
            ensure("no Euler round trip", std::fabs(state.mRotation.mQ[component] -
                normalized.mQ[component]) <= 2e-6f);
    }
}

template<> template<> void object_lock_object::test<4>()
{
    set_test_name("position-only holds seeded main camera aim and world offsets");
    Settings settings;
    settings.mFollowYaw = false;
    settings.mAimYaw = 45.f;
    settings.mAimPitch = 30.f;
    settings.mOffset.setVec(1.f, 2.f, 3.f);
    PoseHistory state;
    const LLQuaternion original(.6f, LLVector3::x_axis);
    state.seed(LLVector3d::zero, original);
    ObjectRenderPose pose;
    pose.mPosition.setVec(5.f, 6.f, 7.f);
    pose.mRotation = composeObjectRotation(1.f, .5f, .7f);
    ensure("sample", state.sample(pose, LLVector3d::zero, settings, 0.0));
    ensure("held aim", closeRotation(state.mRotation, original));
    ensure("world offset", closeVector(state.mPosition.agentPosition(LLVector3d::zero),
        LLVector3(6.f, 8.f, 10.f)));
}

template<> template<> void object_lock_object::test<5>()
{
    set_test_name("offsets use followed frame, independently of local aim trim");
    Settings settings;
    settings.mOffset.setVec(2.f, -1.f, .5f);
    settings.mAimYaw = 40.f;
    settings.mAimPitch = 20.f;
    PoseHistory state;
    ObjectRenderPose pose;
    pose.mRotation = composeObjectRotation(.7f, .3f, .5f);
    state.sample(pose, LLVector3d::zero, settings, 0.0);
    const LLQuaternion yaw(.7f, LLVector3::z_axis);
    ensure("offset in partial frame", closeVector(state.mPosition.agentPosition(LLVector3d::zero),
        settings.mOffset * yaw));
    ensure("trim in local frame", closeRotation(state.mRotation,
        LLQuaternion(20.f * DEG_TO_RAD, LLVector3::y_axis) *
        LLQuaternion(40.f * DEG_TO_RAD, LLVector3::z_axis) * yaw));
}

template<> template<> void object_lock_object::test<6>()
{
    set_test_name("zero smoothing snaps every sample");
    Settings settings;
    PoseHistory state;
    ObjectRenderPose pose;
    state.sample(pose, LLVector3d::zero, settings, 0.0);
    pose.mPosition.setVec(50.f, 30.f, 10.f);
    pose.mRotation = LLQuaternion(2.f, LLVector3::z_axis);
    state.sample(pose, LLVector3d::zero, settings, .001);
    ensure("position rigid", closeVector(state.mPosition.agentPosition(LLVector3d::zero),
        pose.mPosition + settings.mOffset));
    ensure("rotation rigid", closeRotation(state.mRotation, pose.mRotation));
}

template<> template<> void object_lock_object::test<7>()
{
    set_test_name("30 and 120 fps exponential convergence match analytical target");
    const PoseHistory slow = settled(30);
    const PoseHistory fast = settled(120);
    ensure("frame rate independent position", closeVector(
        slow.mPosition.agentPosition(LLVector3d::zero),
        fast.mPosition.agentPosition(LLVector3d::zero)));
    ensure("analytical exponential", std::fabs(slow.mPosition.mGlobalPosition.mdV[VX] -
        10.0 * (1.0 - std::exp(-2.5))) < 2e-5);
    ensure("frame rate independent orientation", closeRotation(slow.mRotation, fast.mRotation));
}

template<> template<> void object_lock_object::test<8>()
{
    set_test_name("smoothing and lost hold survive region origin changes");
    Settings settings;
    settings.mSmoothing = 1.f;
    PoseHistory state;
    ObjectRenderPose pose;
    pose.mPosition.setVec(20.f, 30.f, 40.f);
    const LLVector3d first(1000000.0, 1000000.0, 0.0);
    const LLVector3d second = first + LLVector3d(256.0, 256.0, 0.0);
    state.sample(pose, first, settings, 0.0);
    const LLVector3d saved = state.mPosition.mGlobalPosition;
    pose.mPosition -= LLVector3(256.f, 256.f, 0.f);
    state.sample(pose, second, settings, .1);
    ensure("no global swing", (state.mPosition.mGlobalPosition - saved).length() < 1e-8);
    state.lose();
    ensure("held world pose", (second + LLVector3d(state.mPosition.agentPosition(second)) -
        saved).length() < 1e-4);
    pose.mPosition += LLVector3(10.f, 0.f, 0.f);
    state.sample(pose, second, settings, 4.0);
    ensure("reacquire snaps", (state.mPosition.mGlobalPosition - saved -
        LLVector3d(10.0, 0.0, 0.0)).length() < 1e-5);
}

template<> template<> void object_lock_object::test<9>()
{
    set_test_name("llTargetOmega-like active child follows drawable geometry");
    Settings settings;
    settings.mFollowPitch = settings.mFollowRoll = true;
    PoseHistory state;
    LLMatrix4 parent;
    const LLQuaternion parent_rotation(.7f, LLVector3::z_axis);
    parent.initAll(LLVector3(1.f, 1.f, 1.f), parent_rotation, LLVector3(4.f, 5.f, 6.f));
    for (S32 frame = 0; frame < 600; ++frame)
    {
        const LLQuaternion rendered(static_cast<F32>(frame) * .03f, LLVector3::x_axis);
        const ObjectRenderPose pose = objectDrawablePose(true, LLVector3(1.f, 2.f, 3.f), rendered, parent);
        ensure("sample child", state.sample(pose, LLVector3d::zero, settings,
            static_cast<F64>(frame) / 60.0));
        const LLQuaternion expected = rendered * parent_rotation;
        ensure("rides child's spin", closeRotation(state.mRotation, expected));
        ensure("eye at rendered child", closeVector(state.mPosition.agentPosition(LLVector3d::zero),
            LLVector3(1.f, 2.f, 3.f) * parent + settings.mOffset * expected));
    }
}

template<> template<> void object_lock_object::test<10>()
{
    set_test_name("rewind and target/mask reset clear gimbal and smoothing history");
    Settings settings;
    settings.mSmoothing = 1.f;
    PoseHistory state;
    ObjectRenderPose pose;
    pose.mRotation = composeObjectRotation(.8f, 0.f, 0.f);
    state.sample(pose, LLVector3d::zero, settings, 5.0);
    pose.mRotation = composeObjectRotation(.8f, F_PI_BY_TWO, .4f);
    pose.mPosition.setVec(20.f, 0.f, 0.f);
    state.sample(pose, LLVector3d::zero, settings, 1.0);
    ensure("rewind clears held yaw", std::fabs(state.mFollow.mYaw) < 1e-6f);
    ensure("rewind snaps", std::fabs(state.mPosition.mGlobalPosition.mdV[VX] - 20.0) < 1e-5);
    state.lose();
    ensure("target/mask reset", !state.mFollow.mHaveYaw && !state.mPosition.mHaveSmoothing);
    ensure("held pose retained", state.mPosition.mHaveGood);
}

template<> template<> void object_lock_object::test<11>()
{
    set_test_name("invalid rendered quaternion holds last good pose");
    Settings settings;
    PoseHistory state;
    ObjectRenderPose pose;
    state.sample(pose, LLVector3d::zero, settings, 0.0);
    const LLVector3d saved = state.mPosition.mGlobalPosition;
    for (S32 component = 0; component < 4; ++component) pose.mRotation.mQ[component] = 0.f;
    ensure("reject zero quaternion", !state.sample(pose, LLVector3d::zero, settings, 1.0));
    ensure("retain pose", (state.mPosition.mGlobalPosition - saved).length() < 1e-8);
    ensure("clear history", !state.mFollow.mHaveYaw && !state.mHaveRotation);
}

template<> template<> void object_lock_object::test<12>()
{
    set_test_name("all persisted modes migrate and label without renumbering");
    for (S32 mode = 0; mode <= 52; ++mode)
    {
        ensure_equals("only legacy Step Orbit migrates",
            LLCinematicCamera::migrateLegacyMode(mode), mode == 40 ? 2 : mode);
        ensure("every current mode has a label",
            std::string(LLCinematicCamera::modeName(mode)) != "Unknown");
    }
    ensure_equals("legacy label", std::string(LLCinematicCamera::modeName(40)), std::string("Orbit"));
    ensure_equals("new label", std::string(LLCinematicCamera::modeName(52)), std::string("Object Lock"));
    ensure_equals("unknown rejected", std::string(LLCinematicCamera::modeName(53)), std::string("Unknown"));
    ensure_equals("negative rejected", std::string(LLCinematicCamera::modeName(-1)), std::string("Unknown"));
}

template<> template<> void object_lock_object::test<13>()
{
    set_test_name("UUID loss/reacquire and target/mask changes without an avatar");
    Settings settings;
    settings.mSmoothing = 5.f;
    PoseHistory state;
    const LLUUID first("11111111-1111-1111-1111-111111111111");
    const LLUUID second("22222222-2222-2222-2222-222222222222");
    const LLVector3d origin(1000000.0, 2000000.0, 0.0);
    ObjectRenderPose pose;
    pose.mRotation = composeObjectRotation(.8f, 0.f, 0.f);
    ensure("object-only activation", state.update(first, &pose, origin, settings, 0.0));
    const LLVector3d saved = state.mPosition.mGlobalPosition;
    ensure("UUID unavailable", !state.update(first, nullptr, origin, settings, 1.0));
    ensure("held last pose", (state.mPosition.mGlobalPosition - saved).length() < 1e-8);
    ensure("loss clears yaw", !state.mFollow.mHaveYaw);
    pose.mPosition.setVec(10.f, 0.f, 0.f);
    ensure("same UUID reacquires", state.update(first, &pose, origin, settings, 2.0));
    ensure("reacquire has no stale blend", std::fabs(
        state.mPosition.mGlobalPosition.mdV[VX] - origin.mdV[VX] - 10.0) < 1e-5);
    pose.mRotation = composeObjectRotation(.8f, F_PI_BY_TWO, .4f);
    pose.mPosition.setVec(20.f, 0.f, 0.f);
    state.update(second, &pose, origin, settings, 3.0);
    ensure("new UUID clears gimbal history", std::fabs(state.mFollow.mYaw) < 1e-6f);
    ensure("new UUID snaps mount", std::fabs(
        state.mPosition.mGlobalPosition.mdV[VX] - origin.mdV[VX] - 20.0) < 1e-5);
    settings.mFollowRoll = true;
    pose.mPosition.setVec(30.f, 0.f, 0.f);
    state.update(second, &pose, origin, settings, 4.0);
    ensure("mask change resets smoothing", closeVector(
        state.mPosition.agentPosition(origin), pose.mPosition + settings.mOffset * state.mRotation));
}

template<> template<> void object_lock_object::test<14>()
{
    set_test_name("production mount and cut presentation publish during frozen world time");
    ALCineCamPresentation::CutEase cut;
    PresentedCamera camera;
    const LLVector3 from(-10.f, 4.f, 3.f);
    const LLQuaternion from_rotation(-.6f, LLVector3::z_axis);
    const F32 bezier[4] = { .42f, 0.f, .58f, 1.f };
    ensure("switcher take begins", cut.begin(2.f, 0.0, 0.0, false,
        from, from_rotation, camera.mFov, ALCameraCurve::LINEAR, bezier, 0.f));
    Settings settings; // zero mount damping must remain exact under the cut envelope
    PoseHistory mount;
    ObjectRenderPose pose;
    for (S32 frame = 0; frame <= 5; ++frame)
    {
        const F64 elapsed = static_cast<F64>(frame) * .5;
        pose.mPosition.setVec(static_cast<F32>(frame), 2.f, 1.f);
        pose.mRotation = LLQuaternion(static_cast<F32>(frame) * .2f, LLVector3::z_axis);
        ensure("no avatar and frozen presentation clock", mount.sample(
            pose, LLVector3d::zero, settings, 10.0));
        const LLVector3 rigid = pose.mPosition + settings.mOffset * pose.mRotation;
        ensure("mount is rigid before explicit cut ease", closeVector(
            mount.mPosition.agentPosition(LLVector3d::zero), rigid));
        LLVector3 position = mount.mPosition.agentPosition(LLVector3d::zero);
        LLQuaternion rotation = mount.mRotation;
        F32 weight = 1.f;
        const bool easing = cut.apply(elapsed, position, rotation, weight);
        const F32 fov = cut.blendFov(1.2f, easing, weight); // destination already lensed
        ALCineCamPresentation::writeCamera(camera, position, rotation, fov, easing);
        const F32 expected_weight = llmin(static_cast<F32>(elapsed / 2.0), 1.f);
        ensure("published position follows explicit cut", closeVector(camera.mPosition,
            from + (rigid - from) * expected_weight));
        const LLQuaternion expected_rotation = nlerp(expected_weight, from_rotation, pose.mRotation);
        ensure("published axes follow cut", closeVector(camera.mXAxis,
            LLVector3::x_axis * expected_rotation) && closeVector(camera.mZAxis,
            LLVector3::z_axis * expected_rotation));
        ensure("FOV is blended after lens once", std::fabs(camera.mFov -
            (.7f + (1.2f - .7f) * expected_weight)) < 2e-6f);
        ensure_equals("only settled frames broadcast FOV", camera.mBroadcasts, frame < 4 ? 0 : frame - 3);
    }
    ensure_equals("four intermediate lens frames stay local", camera.mLocalViews, 4);
    ensure("ease expires despite frozen world", !cut.mActive);
    ensure("settled camera returns to rigid mount", closeVector(camera.mPosition,
        pose.mPosition + settings.mOffset * pose.mRotation));
}

template<> template<> void object_lock_object::test<15>()
{
    set_test_name("shared cut eligibility, latched curve, interruption and hard cuts");
    ALCineCamPresentation::CutEase cut;
    F32 bezier[4] = { .42f, 0.f, .58f, 1.f };
    const LLVector3 from(4.f, 5.f, 6.f);
    const LLQuaternion rotation(.4f, LLVector3::z_axis);
    ensure("first activation can ease", cut.begin(2.f, .1, .1, false,
        from, rotation, .8f, ALCameraCurve::QUAD_IN, bezier, 0.f));
    bezier[0] = .9f;
    ensure("curve control points latched", std::fabs(cut.mBezier[0] - .42f) < 1e-6f);
    LLVector3 position(12.f, 9.f, 10.f);
    LLQuaternion destination(1.f, LLVector3::z_axis);
    F32 weight = 0.f;
    ensure("nonlinear easing active", cut.apply(1.0, position, destination, weight));
    ensure("selected quadratic curve used", std::fabs(weight - .25f) < 1e-6f);
    // A second switcher take snapshots the already presented result, not the old take.
    ensure("interrupted cut begins at final previous camera", cut.begin(1.f, 0.0, 0.0, false,
        position, destination, .9f, ALCameraCurve::LINEAR, bezier, 0.f));
    const LLVector3 presented = position;
    position.setVec(100.f, 0.f, 0.f);
    cut.apply(0.0, position, destination, weight);
    ensure("new cut has no onset jump", closeVector(position, presented));
    for (S32 rejected = 0; rejected < 6; ++rejected)
    {
        const F32 duration = rejected == 0 ? 0.f : 2.f;
        const F64 age = rejected == 1 ? -1.0 : (rejected == 2 ? 2.0 : 0.0);
        const F64 elapsed = rejected == 3 ? -1.0 : (rejected == 4 ? 2.0 : 0.0);
        ensure("hard/stale/expired cut rejected", !cut.begin(duration, age, elapsed, rejected == 5,
            from, rotation, .8f, ALCameraCurve::LINEAR, bezier, 0.f));
        position.setVec(12.f, 9.f, 10.f);
        destination = rotation;
        ensure("hard cut stays rigid", !cut.apply(0.0, position, destination, weight) &&
            closeVector(position, LLVector3(12.f, 9.f, 10.f)) && weight == 1.f);
    }
}

template<> template<> void object_lock_object::test<16>()
{
    set_test_name("configured lock release restores disabled mode without camera dispatch");
    const S32 object_mode = LLCinematicCamera::MODE_OBJECT_LOCK;
    ensure("disabled configured mode is releasable", isConfigured(object_mode, 0, object_mode));
    ensure("disabled armed switcher slot is releasable", isConfigured(2, object_mode, object_mode));
    ensure("unrelated modes have no configured lock", !isConfigured(2, 1, object_mode));
    for (S32 mode : {0, 1, 2, 40, 51})
    {
        const S32 migrated = LLCinematicCamera::migrateLegacyMode(mode);
        const RestoreState disabled = restoreState(true, migrated, false, object_mode);
        ensure_equals("prior disabled mode restored", disabled.mMode, migrated);
        ensure("no temporary enable needed", !disabled.mEnabled);
        const RestoreState enabled = restoreState(true, migrated, true, object_mode);
        ensure_equals("prior enabled mode restored", enabled.mMode, migrated);
        ensure_equals("Off stays disabled", enabled.mEnabled, mode != 0);
    }
    const RestoreState unknown = restoreState(false, 2, true, object_mode);
    ensure("unknown session history restores Off", unknown.mMode == 0 && !unknown.mEnabled);
    const RestoreState invalid = restoreState(true, object_mode, true, object_mode);
    ensure("cannot recursively restore Object Lock", invalid.mMode == 0 && !invalid.mEnabled);
    // This value is captured before the setting signal clears session history.
    bool have_previous = true;
    const RestoreState captured = restoreState(have_previous, 2, false, object_mode);
    have_previous = false;
    ensure("synchronous mode observer cannot corrupt release snapshot",
        !have_previous && captured.mMode == 2 && !captured.mEnabled);
}

template<> template<> void object_lock_object::test<17>()
{
    set_test_name("outer-scaled clone center precedes global damping; mount offsets stay metres");
    LLClientOuterTransform outer;
    outer.mEnabled = true;
    outer.mScale = 2.f;
    const LLVector3 pivot(10.f, 20.f, 30.f);
    outer.mCurrent.initAll(LLVector3(2.f, 2.f, 2.f), LLQuaternion::DEFAULT, -pivot);
    const ObjectRenderPose native = { LLVector3(11.f, 22.f, 33.f),
        composeObjectRotation(.6f, .2f, -.3f) };
    ObjectRenderPose drawn = native;
    applyOuterTransform(drawn, &outer);
    const LLVector3 drawn_center(12.f, 24.f, 36.f);
    ensure("sample center matches drawing", closeVector(drawn.mPosition, drawn_center));
    ensure("positive uniform outer scale preserves orientation", closeRotation(drawn.mRotation, native.mRotation));
    Settings settings;
    settings.mFollowPitch = settings.mFollowRoll = true;
    settings.mOffset.setVec(.7f, -.3f, .2f);
    const LLVector3 offset = settings.mOffset * native.mRotation;
    const LLVector3d origin(1000000.0, 2000000.0, 0.0);
    PoseHistory state;
    ensure("scaled target samples", state.sample(drawn, origin, settings, 0.0));
    ensure("metre offset is not scaled", closeVector(state.mPosition.agentPosition(origin), drawn_center + offset));
    settings.mSmoothing = 1.f;
    outer.mScale = 3.f;
    outer.mCurrent.initAll(LLVector3(3.f, 3.f, 3.f), LLQuaternion::DEFAULT, pivot * -2.f);
    drawn = native;
    applyOuterTransform(drawn, &outer);
    state.sample(drawn, origin, settings, .5);
    const LLVector3 expected = drawn_center + offset +
        (LLVector3(13.f, 26.f, 39.f) - drawn_center) * (1.f - std::exp(-.5f));
    ensure("global smoother consumes transformed center", closeVector(state.mPosition.agentPosition(origin), expected));
    for (S32 bypass = 0; bypass < 3; ++bypass)
    {
        drawn = native;
        outer.mEnabled = bypass != 0;
        outer.mScale = bypass == 1 ? 1.f : 3.f;
        applyOuterTransform(drawn, bypass == 2 ? nullptr : &outer);
        ensure("disabled, unit scale and absent outer are identity", closeVector(drawn.mPosition, native.mPosition));
    }
    ensure("native VCam sample remains unchanged", closeVector(native.mPosition, LLVector3(11.f, 22.f, 33.f)));
}

}
