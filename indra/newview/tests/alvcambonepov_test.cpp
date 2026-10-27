/**
 * @file alvcambonepov_test.cpp
 * @brief Adversarial tests for pure skeleton-attached virtual-camera math.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "../test/lltut.h"
#include "../alvcambonepovmath.h"
#include "../llprismlens.h"
#include "../alvcamobjectpovsettings.h"
#include "llsdserialize.h"

#include <cmath>
#include <limits>
#include <random>
#include <sstream>

namespace tut
{
using namespace ALVCamBonePovMath;

namespace
{
bool nearVector(const LLVector3& actual, const LLVector3& expected,
                F32 tolerance = 1e-5f)
{
    return std::fabs(actual.mV[VX] - expected.mV[VX]) <= tolerance &&
           std::fabs(actual.mV[VY] - expected.mV[VY]) <= tolerance &&
           std::fabs(actual.mV[VZ] - expected.mV[VZ]) <= tolerance;
}

bool sameRotation(const LLQuaternion& actual, const LLQuaternion& expected,
                  F32 tolerance = 1e-5f)
{
    // Compare all basis axes, independently of quaternion sign.
    return actual.isFinite() &&
        nearVector(LLVector3::x_axis * actual, LLVector3::x_axis * expected, tolerance) &&
        nearVector(LLVector3::y_axis * actual, LLVector3::y_axis * expected, tolerance) &&
        nearVector(LLVector3::z_axis * actual, LLVector3::z_axis * expected, tolerance);
}

F32 rotationBasisStep(const LLQuaternion& first, const LLQuaternion& second)
{
    return llmax(dist_vec(LLVector3::x_axis * first, LLVector3::x_axis * second),
        llmax(dist_vec(LLVector3::y_axis * first, LLVector3::y_axis * second),
              dist_vec(LLVector3::z_axis * first, LLVector3::z_axis * second)));
}

struct ObjectDrawableFixture
{
    LLVector3 mModelPosition;
    LLQuaternion mModelRotation;
    LLVector3 mPosition;
    LLQuaternion mRotation;
    LLMatrix4 mWorld;
    ObjectDrawableFixture* mParent = nullptr;
    bool mActive = true;
    bool mDead = false;
    S32 mGeneration = 1;

    bool isDead() const { return mDead; }
    bool isActive() const { return mActive; }
    bool isRoot() const { return !mParent; }
    S32 getGeneration() const { return mGeneration; }
    const ObjectDrawableFixture* getParent() const { return mParent; }
    const LLVector3& getPosition() const { return mPosition; }
    const LLQuaternion& getRotation() const { return mRotation; }
    const LLMatrix4& getWorldMatrix() const { return mWorld; }
};
} // anonymous namespace

struct alvcambonepov_data
{
};

typedef test_group<alvcambonepov_data> alvcambonepov_test_group;
typedef alvcambonepov_test_group::object alvcambonepov_test_object;
alvcambonepov_test_group alvcambonepov_test("alvcambonepov");

template<> template<>
void alvcambonepov_test_object::test<1>()
{
    set_test_name("foot-pivot clone scale");
    const LLVector3 joint(7.f, -2.f, 3.f);
    const LLVector3 foot(5.f, -4.f, 1.f);
    ensure("scale one is a bit-exact no-op",
           boneScaledPoint(joint, foot, 1.f) == joint);
    ensure("half scale uses the supplied foot pivot",
           nearVector(boneScaledPoint(joint, foot, 0.5f),
                      LLVector3(6.f, -3.f, 2.f)));
    ensure("half clone maps a two-metre joint to one metre",
           nearVector(boneScaledPoint(LLVector3(0.f, 0.f, 2.f),
                                      LLVector3::zero, 0.5f),
                      LLVector3(0.f, 0.f, 1.f)));
    ensure("0.05 clone remains correctly scaled",
           nearVector(boneScaledPoint(LLVector3(0.f, 0.f, 2.f),
                                      LLVector3::zero, 0.05f),
                      LLVector3(0.f, 0.f, 0.1f)));
}

template<> template<>
void alvcambonepov_test_object::test<2>()
{
    set_test_name("local offset rotation and clone scale");
    const LLQuaternion quarter_turn(F_PI_BY_TWO, LLVector3::z_axis);
    const LLVector3 base(1.f, 0.f, 0.f);
    const LLVector3 offset(0.2f, 0.f, 0.f);
    ensure("joint-local +X rotates to agent +Y",
           nearVector(boneEye(base, offset, 1.f, quarter_turn),
                      LLVector3(1.f, 0.2f, 0.f)));
    ensure("clone scale changes offset magnitude",
           nearVector(boneEye(base, offset, 0.5f, quarter_turn),
                      LLVector3(1.f, 0.1f, 0.f)));
}

template<> template<>
void alvcambonepov_test_object::test<3>()
{
    set_test_name("X-forward Z-up camera basis remap");
    const LLQuaternion rotation = followRotation(
        LLVector3::x_axis, LLVector3::z_axis);
    const LLVector3 render_forward = LLVector3(0.f, 0.f, -1.f) * rotation;
    const LLVector3 render_up = LLVector3::y_axis * rotation;
    ensure("camera local -Z becomes skeleton +X",
           nearVector(render_forward, LLVector3::x_axis, 1e-4f));
    ensure("camera local +Y becomes skeleton +Z",
           nearVector(render_up, LLVector3::z_axis, 1e-4f));

    // Mirror the render consumer's independent re-derivation exactly. This is
    // intentionally not hidden behind followRotation so a convention change in
    // forward/up/right is visible here.
    LLVector3 render_right = render_forward % render_up;
    render_right.normVec();
    ensure("render forward cross up yields agent -Y right",
           nearVector(render_right, LLVector3(0.f, -1.f, 0.f), 1e-4f));
}

template<> template<>
void alvcambonepov_test_object::test<4>()
{
    set_test_name("basis remap yaw case");
    const LLQuaternion rotation = followRotation(
        LLVector3::y_axis, LLVector3::z_axis);
    ensure("an agent +Y head points the rendered camera +Y",
           nearVector(LLVector3(0.f, 0.f, -1.f) * rotation,
                      LLVector3::y_axis, 1e-4f));
}

template<> template<>
void alvcambonepov_test_object::test<5>()
{
    set_test_name("horizon lock and vertical degeneracy");
    LLVector3 tilted_up(0.f, 1.f, 1.f);
    tilted_up.normVec();
    LLVector3 level_up;
    ensure("horizontal forward can be leveled",
           horizonLevelUp(LLVector3::x_axis, level_up));
    ensure("level result is world up",
           nearVector(level_up, LLVector3::z_axis));
    ensure("leveling strictly removes roll",
           level_up * LLVector3::z_axis >
               tilted_up * LLVector3::z_axis);

    LLVector3 untouched(4.f, 5.f, 6.f);
    ensure("straight-up forward reports degeneracy for hold-last fallback",
           !horizonLevelUp(LLVector3::z_axis, untouched));
    ensure("degeneracy does not emit a fabricated up",
           untouched == LLVector3(4.f, 5.f, 6.f));

    LLVector3 generic_forward(1.f, 2.f, 0.4f);
    generic_forward.normVec();
    ensure("generic forward can be leveled",
           horizonLevelUp(generic_forward, level_up));
    ensure("leveled up is orthogonal to forward",
           std::fabs(level_up * generic_forward) <= 1e-5f);
}

template<> template<>
void alvcambonepov_test_object::test<6>()
{
    set_test_name("exponential position smoother");
    const LLVector3 current(0.f, 0.f, 0.f);
    const LLVector3 target(10.f, 0.f, 0.f);
    ensure("zero tau snaps exactly",
           smoothStep(current, target, 0.1f, 0.f) == target);
    const LLVector3 one_tau = smoothStep(current, target, 2.f, 2.f);
    ensure("dt equal tau is 10 times one minus e^-1",
           std::fabs(one_tau.mV[VX] - 6.3212056f) <= 1e-5f);
    const LLVector3 second = smoothStep(one_tau, target, 2.f, 2.f);
    ensure("successive steps converge monotonically",
           second.mV[VX] > one_tau.mV[VX] && second.mV[VX] < 10.f);
}

template<> template<>
void alvcambonepov_test_object::test<7>()
{
    set_test_name("exponential quaternion slerp");
    constexpr F32 LN_TWO = 0.6931471805599453f;
    constexpr F32 SQRT_HALF = 0.7071067811865475f;
    const LLQuaternion start;
    const LLQuaternion target(F_PI_BY_TWO, LLVector3::z_axis);
    const LLQuaternion halfway = smoothRotation(start, target, LN_TWO, 1.f);
    ensure("one exponential half-life slerps to 45 degrees",
           nearVector(LLVector3::x_axis * halfway,
                      LLVector3(SQRT_HALF, SQRT_HALF, 0.f), 1e-5f));
    ensure("zero tau snaps quaternion exactly",
           smoothRotation(start, target, 1.f, 0.f) == target);
}

template<> template<>
void alvcambonepov_test_object::test<8>()
{
    set_test_name("eye rotation bisector");
    const LLQuaternion eyes[2] = {
        LLQuaternion(10.f * DEG_TO_RAD, LLVector3::z_axis),
        LLQuaternion(-10.f * DEG_TO_RAD, LLVector3::z_axis)
    };
    LLVector3 forward;
    ensure("two eye rotations produce a valid eyeline",
           averageEyeForward(eyes, 2, forward));
    ensure("symmetric eye yaw bisects on agent +X",
           nearVector(forward, LLVector3::x_axis, 1e-4f));
}

template<> template<>
void alvcambonepov_test_object::test<9>()
{
    set_test_name("legacy joint selection normalization");
    const LLPrismLens::NormalizedBonePovJointSelection head =
        LLPrismLens::normalizeBonePovJointSelection(0, "ignored");
    ensure("legacy Head becomes a named mHead selection",
           head.mTag == LLPrismLens::BONE_JOINT_NAMED &&
           head.mName == "mHead");

    const LLPrismLens::NormalizedBonePovJointSelection eye =
        LLPrismLens::normalizeBonePovJointSelection(1, "ignored");
    ensure("legacy Eye becomes the nameless synthetic eyeline",
           eye.mTag == LLPrismLens::BONE_JOINT_EYELINE &&
           eye.mName.empty());

    const LLPrismLens::NormalizedBonePovJointSelection neck =
        LLPrismLens::normalizeBonePovJointSelection(2, "ignored");
    ensure("legacy Neck becomes a named mNeck selection",
           neck.mTag == LLPrismLens::BONE_JOINT_NAMED &&
           neck.mName == "mNeck");

    const LLPrismLens::NormalizedBonePovJointSelection custom =
        LLPrismLens::normalizeBonePovJointSelection(3, "mWing4FanLeft");
    ensure("legacy Custom retains its exact selected joint name",
           custom.mTag == LLPrismLens::BONE_JOINT_NAMED &&
           custom.mName == "mWing4FanLeft");

    const LLPrismLens::NormalizedBonePovJointSelection unknown =
        LLPrismLens::normalizeBonePovJointSelection(255, "mMystery");
    ensure("an unknown future tag degrades to eyeline",
           unknown.mTag == LLPrismLens::BONE_JOINT_EYELINE &&
           unknown.mName.empty());
}

template<> template<>
void alvcambonepov_test_object::test<10>()
{
    set_test_name("picked-joint spine classification");
    ensure("pelvis is on the full-follow torso chain",
           LLPrismLens::isBonePovSpineJoint("mPelvis"));
    ensure("Bento spine is on the full-follow torso chain",
           LLPrismLens::isBonePovSpineJoint("mSpine3"));
    ensure("head is on the full-follow torso chain",
           LLPrismLens::isBonePovSpineJoint("mHead"));
    ensure("wrist defaults to stabilized",
           !LLPrismLens::isBonePovSpineJoint("mWristLeft"));
    ensure("face joints default to stabilized",
           !LLPrismLens::isBonePovSpineJoint("mFaceJaw"));
}
template<> template<>
void alvcambonepov_test_object::test<11>()
{
    set_test_name("Object POV yaw-only spin and level horizon");
    ObjectFollowHistory history;
    const LLQuaternion operator_aim = LLQuaternion(.3f, LLVector3::x_axis) *
        LLQuaternion(-.4f, LLVector3::y_axis);
    for (S32 degrees = -720; degrees <= 720; degrees += 3)
    {
        const LLQuaternion object(degrees * DEG_TO_RAD, LLVector3::z_axis);
        const LLQuaternion followed = objectFollowRotation(object, true, false, false, history);
        ensure("yaw matches object through multiple wraps", sameRotation(followed, object));
        const LLQuaternion camera = followRotation(
            LLVector3::x_axis * followed, LLVector3::z_axis * followed);
        ensure("camera keeps world-up horizon",
               nearVector(LLVector3::y_axis * camera, LLVector3::z_axis));
        ensure("camera yaw matches object",
               nearVector(LLVector3(0.f, 0.f, -1.f) * camera, LLVector3::x_axis * object));
        const LLQuaternion trimmed = operator_aim * followed;
        ensure("yaw preserves operator pitch", std::fabs(
            (LLVector3::x_axis * trimmed).mV[VZ] -
            (LLVector3::x_axis * operator_aim).mV[VZ]) <= 1e-5f);
        ensure("yaw preserves operator roll", std::fabs(
            (LLVector3::z_axis * trimmed).mV[VZ] -
            (LLVector3::z_axis * operator_aim).mV[VZ]) <= 1e-5f);
    }
}

template<> template<>
void alvcambonepov_test_object::test<12>()
{
    set_test_name("Object POV pitch-only, roll-only and none");
    ObjectFollowHistory history;
    const LLQuaternion yaw(0.8f, LLVector3::z_axis);
    const LLQuaternion pitch(0.6f, LLVector3::y_axis);
    const LLQuaternion roll(-0.4f, LLVector3::x_axis);
    ensure("pitch-only discards yaw", sameRotation(
        objectFollowRotation(pitch * yaw, false, true, false, history), pitch));
    ensure("roll-only discards yaw", sameRotation(
        objectFollowRotation(roll * yaw, false, false, true, history), roll));
    ensure("pitch is absent in roll-only", sameRotation(
        objectFollowRotation(pitch, false, false, true, history), LLQuaternion()));
    ensure("roll is absent in pitch-only", sameRotation(
        objectFollowRotation(roll, false, true, false, history), LLQuaternion()));
    ensure("none is exact identity", objectFollowRotation(
        roll * pitch * yaw, false, false, false, history) == LLQuaternion());
    const LLQuaternion boat = roll * pitch * yaw;
    ensure("pitch and roll reconstruct the boat tilt", sameRotation(
        objectFollowRotation(boat, false, true, true, history), roll * pitch));
    ensure("boat pitch follows theta independently of roll", sameRotation(
        objectFollowRotation(boat, false, true, false, history), pitch));
    ensure("boat roll follows phi independently of pitch", sameRotation(
        objectFollowRotation(boat, false, false, true, history), roll));
}

template<> template<>
void alvcambonepov_test_object::test<13>()
{
    set_test_name("Object POV full follow and quaternion sign invariance");
    for (S32 index = 1; index < 150; ++index)
    {
        LLVector3 axis(std::sin(index * .17f), std::cos(index * .23f), std::sin(index * .31f));
        axis.normVec();
        const LLQuaternion object(index * .13f, axis);
        ObjectFollowHistory positive_history;
        ObjectFollowHistory negative_history;
        ensure("all axes equal the full object rotation", sameRotation(
            objectFollowRotation(object, true, true, true, positive_history), object));
        for (U8 mask = 0; mask < 8; ++mask)
        {
            const LLQuaternion positive = objectFollowRotation(
                object, mask & 1, mask & 2, mask & 4, positive_history);
            const LLQuaternion negative = objectFollowRotation(
                -1.f * object, mask & 1, mask & 2, mask & 4, negative_history);
            ensure("q and -q select the same followed axes", sameRotation(positive, negative));
        }
    }
}

template<> template<>
void alvcambonepov_test_object::test<14>()
{
    set_test_name("Object POV continuity at yaw wrap");
    for (U8 mask = 0; mask < 8; ++mask)
    {
        ObjectFollowHistory history;
        LLQuaternion previous;
        for (S32 step = 0; step <= 40; ++step)
        {
            const F32 degrees = 178.f + step * .1f;
            const F32 wrapped = degrees > 180.f ? degrees - 360.f : degrees;
            const LLQuaternion object = LLQuaternion(.4f, LLVector3::y_axis) *
                LLQuaternion(wrapped * DEG_TO_RAD, LLVector3::z_axis);
            const LLQuaternion followed = objectFollowRotation(
                object, mask & 1, mask & 2, mask & 4, history);
            if (step > 0)
                ensure("yaw wrap has no flip", sameRotation(followed, previous, .003f));
            previous = followed;
        }
        // Vertical pitch now uses the explicit principal-Euler gimbal policy,
        // tested on both sides of +/-90 in test 27 rather than swing continuity.
    }
}

template<> template<>
void alvcambonepov_test_object::test<15>()
{
    set_test_name("Object POV offsets use only followed axes");
    ObjectFollowHistory history;
    const LLQuaternion pitch(.7f, LLVector3::y_axis);
    const LLQuaternion yaw(F_PI_BY_TWO, LLVector3::z_axis);
    const LLVector3 offset(2.f, 3.f, 4.f);
    const LLQuaternion object = pitch * yaw;
    ensure("yaw-only offset ignores object pitch", nearVector(offset *
        objectFollowRotation(object, true, false, false, history), LLVector3(-3.f, 2.f, 4.f)));
    ensure("pitch-only offset ignores object yaw", nearVector(offset *
        objectFollowRotation(object, false, true, false, history), offset * pitch));
    ensure("position-only offset is world aligned", offset *
        objectFollowRotation(object, false, false, false, history) == offset);
    ensure("all axes rotate offset with object", nearVector(offset *
        objectFollowRotation(object, true, true, true, history), offset * object));
}

template<> template<>
void alvcambonepov_test_object::test<16>()
{
    set_test_name("Object POV reuses exact-zero and frame-independent smoothing");
    const LLVector3 target_position(12.f, -4.f, 9.f);
    const LLQuaternion target_rotation(1.3f, LLVector3::z_axis);
    LLVector3 positions[2];
    LLQuaternion rotations[2];
    const S32 rates[] = {30, 120};
    for (S32 index = 0; index < 2; ++index)
    {
        for (S32 frame = 0; frame < rates[index]; ++frame)
        {
            const F32 dt = 1.f / rates[index];
            positions[index] = smoothStep(positions[index], target_position, dt, .5f);
            rotations[index] = smoothRotation(rotations[index], target_rotation, dt, .5f);
        }
    }
    ensure("30 and 120 FPS positions converge equally", nearVector(positions[0], positions[1], 1e-4f));
    ensure("30 and 120 FPS rotations converge equally", sameRotation(rotations[0], rotations[1], 2e-4f));
    ensure("zero smoothing is exact position lock", smoothStep(
        LLVector3::zero, target_position, 0.f, 0.f) == target_position);
    ensure("zero smoothing is exact rotation lock", smoothRotation(
        LLQuaternion(), target_rotation, 0.f, 0.f) == target_rotation);
}

template<> template<>
void alvcambonepov_test_object::test<17>()
{
    set_test_name("Object POV persistence XML round trip and legacy scene");
    using namespace ALVCamObjectPovSettings;
    LLPrismLens::ObjectPovSettings original;
    original.mEnabled = true;
    original.mObjectId = LLUUID("12345678-1234-5678-9012-123456789abc");
    original.mOffset.setVec(1.5f, -2.f, 3.f);
    original.mTrimPitchDeg = -17.f;
    original.mTrimYawDeg = 123.f;
    original.mFollowYaw = false;
    original.mFollowPitch = true;
    original.mFollowRoll = true;
    original.mSmoothingSec = .75f;
    std::stringstream xml;
    LLSDSerialize::toXML(serialize(original), xml);
    LLSD parsed;
    LLSDSerialize::fromXML(parsed, xml);
    LLPrismLens::ObjectPovSettings loaded;
    ensure("valid attachment loads", deserialize(parsed, loaded));
    ensure("round trip preserves every field", loaded.mEnabled == original.mEnabled &&
        loaded.mObjectId == original.mObjectId && loaded.mOffset == original.mOffset &&
        loaded.mTrimPitchDeg == original.mTrimPitchDeg && loaded.mTrimYawDeg == original.mTrimYawDeg &&
        loaded.mFollowYaw == original.mFollowYaw && loaded.mFollowPitch == original.mFollowPitch &&
        loaded.mFollowRoll == original.mFollowRoll && loaded.mSmoothingSec == original.mSmoothingSec);
    ensure("legacy missing key loads", deserialize(LLSD(), loaded));
    ensure("legacy attachment stays off", !loaded.mEnabled && loaded.mObjectId.isNull());
    ensure("legacy defaults to rigid yaw follow", loaded.mFollowYaw && !loaded.mFollowPitch &&
        !loaded.mFollowRoll && loaded.mSmoothingSec == 0.f);
}

template<> template<>
void alvcambonepov_test_object::test<18>()
{
    set_test_name("Object POV rejects malformed persistence transactionally");
    using namespace ALVCamObjectPovSettings;
    LLPrismLens::ObjectPovSettings settings;
    settings.mEnabled = true;
    const LLSD baseline = serialize(settings);
    for (const char* key : {"enabled", "follow_yaw", "follow_pitch", "follow_roll"})
    {
        LLSD invalid = baseline;
        invalid[key] = "true";
        ensure("boolean strings rejected", !deserialize(invalid, settings));
        ensure("refusal leaves output untouched", settings.mEnabled);
    }
    LLSD invalid = baseline;
    invalid["object_id"] = "not-a-uuid";
    ensure("malformed UUID rejected", !deserialize(invalid, settings));
    invalid = baseline;
    invalid["offset"][1] = "nan";
    ensure("nonnumeric offset rejected", !deserialize(invalid, settings));
    invalid = baseline;
    invalid["offset"][0] = std::numeric_limits<F64>::infinity();
    ensure("infinite offset rejected", !deserialize(invalid, settings));
    invalid = baseline;
    invalid["smoothing_seconds"] = -1.;
    ensure("negative smoothing rejected", !deserialize(invalid, settings));
    invalid["smoothing_seconds"] = 11.;
    ensure("excess smoothing rejected", !deserialize(invalid, settings));
    invalid = baseline;
    invalid["trim_pitch_degrees"] = 181.;
    ensure("out of range trim rejected", !deserialize(invalid, settings));
    invalid["trim_pitch_degrees"] = std::numeric_limits<F64>::quiet_NaN();
    ensure("NaN trim rejected", !deserialize(invalid, settings));
    ensure("non-map rejected", !deserialize(LLSD(5), settings));
}

template<> template<>
void alvcambonepov_test_object::test<19>()
{
    set_test_name("Object POV samples the active child's geometry frame");
    ObjectDrawableFixture parent;
    const LLQuaternion parent_rotation(.8f, LLVector3::z_axis);
    parent.mWorld.initAll(LLVector3(1.f, 1.f, 1.f), parent_rotation,
                          LLVector3(40.f, 50.f, 60.f));
    ObjectDrawableFixture child;
    child.mParent = &parent;
    child.mPosition.setVec(1.f, 2.f, 3.f);
    child.mRotation = LLQuaternion(.3f, LLVector3::y_axis);
    child.mModelPosition.setVec(7.f, 8.f, 9.f);
    child.mModelRotation = LLQuaternion(1.2f, LLVector3::y_axis);
    ObjectRenderPose pose;
    ensure("live child can be sampled", objectDrawablePose(child, pose));
    const LLVector3 geometry_position = child.mPosition * parent.mWorld;
    const LLQuaternion geometry_rotation = child.mRotation * parent_rotation;
    ensure("eye matches drawable geometry", nearVector(pose.mPosition, geometry_position));
    ensure("orientation matches drawable geometry", sameRotation(pose.mRotation, geometry_rotation));
    ensure("fixture distinguishes raw model position", !nearVector(
        pose.mPosition, child.mModelPosition * parent.mWorld));
    ensure("fixture distinguishes raw model rotation", !sameRotation(
        pose.mRotation, child.mModelRotation * parent_rotation));
    const LLVector3 offset(2.f, -1.f, .5f);
    ensure("followed offset matches geometry frame", nearVector(
        pose.mPosition + offset * pose.mRotation,
        geometry_position + offset * geometry_rotation));
    ensure("camera basis matches geometry", sameRotation(
        followRotation(LLVector3::x_axis * pose.mRotation, LLVector3::z_axis * pose.mRotation),
        followRotation(LLVector3::x_axis * geometry_rotation, LLVector3::z_axis * geometry_rotation)));

    child.mActive = false;
    child.mWorld.initAll(LLVector3(1.f, 1.f, 1.f), child.mRotation, LLVector3(10.f, 20.f, 30.f));
    ensure("static uses its world transform", objectDrawablePose(child, pose) &&
        nearVector(pose.mPosition, LLVector3(10.f, 20.f, 30.f)) &&
        sameRotation(pose.mRotation, child.mRotation));
    child.mActive = true;
    parent.mDead = true;
    ensure("dead parent is refused", !objectDrawablePose(child, pose));
    parent.mDead = false;
    child.mGeneration = -1;
    ensure("uninitialized drawable is refused", !objectDrawablePose(child, pose));
    ensure("active root uses its drawable world transform", objectDrawablePose(parent, pose) &&
        nearVector(pose.mPosition, LLVector3(40.f, 50.f, 60.f)) &&
        sameRotation(pose.mRotation, parent_rotation));
}

template<> template<>
void alvcambonepov_test_object::test<20>()
{
    set_test_name("Object POV target-omega child rigidly follows interpolated geometry");
    ObjectDrawableFixture parent;
    const LLQuaternion parent_rotation(.7f, LLVector3::z_axis);
    parent.mWorld.initAll(LLVector3(1.f, 1.f, 1.f), parent_rotation, LLVector3(3.f, 4.f, 5.f));
    ObjectDrawableFixture child;
    child.mParent = &parent;
    child.mPosition.setVec(2.f, 0.f, 0.f);
    ObjectFollowHistory history;
    ObjectPositionHistory position_history;
    const LLVector3 offset(1.f, 2.f, .5f);
    const LLVector3d origin(1000000.0, 1000000.0, 0.0);
    for (S32 frame = 0; frame < 180; ++frame)
    {
        // The model advances target omega; the drawable damps toward it.
        child.mModelRotation *= LLQuaternion(.04f, LLVector3::z_axis);
        child.mRotation = nlerp(.2f, child.mRotation, child.mModelRotation);
        ObjectRenderPose pose;
        ensure("animated child samples", objectDrawablePose(child, pose));
        const LLQuaternion followed = objectFollowRotation(pose.mRotation, true, true, true, history);
        const LLQuaternion geometry_rotation = child.mRotation * parent_rotation;
        const LLVector3 geometry_eye = child.mPosition * parent.mWorld + offset * geometry_rotation;
        position_history.sample(origin + LLVector3d(pose.mPosition + offset * followed),
                                1.f / 60.f, 0.f, false);
        ensure("zero smoothing eye equals this frame's geometry", nearVector(
            position_history.agentPosition(origin), geometry_eye));
        ensure("zero smoothing rotation equals this frame's geometry", sameRotation(
            smoothRotation(LLQuaternion(), followed, 1.f / 60.f, 0.f), geometry_rotation));
        ensure("model is ahead of the rendered child", !sameRotation(
            pose.mRotation, child.mModelRotation * parent_rotation));
    }
}

template<> template<>
void alvcambonepov_test_object::test<21>()
{
    set_test_name("Object POV global smoothing and loss survive origin shifts");
    const LLVector3d origin(1000000.0, 1000000.0, 0.0);
    const LLVector3d shifted_origin = origin + LLVector3d(256.0, 0.0, 0.0);
    const LLVector3d target = origin + LLVector3d(20.0, 30.0, 40.0);
    ObjectPositionHistory history;
    history.sample(target, 1.f / 60.f, .5f, false);
    const LLVector3 shifted_target(-236.f, 30.f, 40.f);
    history.sample(shifted_origin + LLVector3d(shifted_target), 1.f / 60.f, .5f, false);
    ensure("stationary global target is unchanged", history.mGlobalPosition == target);
    ensure("new agent coordinates preserve world position", LLVector3d(
        history.agentPosition(shifted_origin)) + shifted_origin == target);
    ensure("origin shift is exactly compensated", nearVector(
        history.agentPosition(shifted_origin), LLVector3(-236.f, 30.f, 40.f)));

    history.lose();
    ensure("loss retains the last good global position", history.mHaveGood &&
        !history.mHaveSmoothing && history.mGlobalPosition == target);
    ensure("hold publishes the same world position after a shift", LLVector3d(
        history.agentPosition(shifted_origin)) + shifted_origin == target);
    const LLVector3d second_origin = shifted_origin + LLVector3d(0.0, 256.0, 0.0);
    ensure("continued loss survives another shift", LLVector3d(
        history.agentPosition(second_origin)) + second_origin == target);
    const LLVector3d reacquired = target + LLVector3d(4.0, 0.0, 0.0);
    history.sample(reacquired, 1.f / 60.f, .5f, false);
    ensure("reacquisition snaps without stale smoothing", history.mGlobalPosition == reacquired);
}

template<> template<>
void alvcambonepov_test_object::test<22>()
{
    set_test_name("Object POV exact half-turn signs agree for all axis masks");
    const LLQuaternion half_turn(.6f, .8f, 0.f, 0.f);
    for (U8 mask = 0; mask < 8; ++mask)
    {
        ObjectFollowHistory positive_history;
        ObjectFollowHistory negative_history;
        ensure("exact q and -q have identical selected rotations", sameRotation(
            objectFollowRotation(half_turn, mask & 1, mask & 2, mask & 4, positive_history),
            objectFollowRotation(-1.f * half_turn, mask & 1, mask & 2, mask & 4, negative_history)));
        ObjectFollowHistory alternating_history;
        const LLQuaternion first = objectFollowRotation(
            half_turn, mask & 1, mask & 2, mask & 4, alternating_history);
        ensure("a sign change at a stationary half-turn cannot move the camera", sameRotation(
            objectFollowRotation(-1.f * half_turn, mask & 1, mask & 2, mask & 4, alternating_history), first));
    }
}

template<> template<>
void alvcambonepov_test_object::test<23>()
{
    set_test_name("Object POV Euler masks continue through mixed-axis half-turns");
    const LLVector3 swing_axis(.6f, .8f, 0.f);
    for (U8 mask = 0; mask < 8; ++mask)
    {
        for (S32 direction : {-1, 1})
        {
            ObjectFollowHistory history;
            ObjectFollowHistory opposite_history;
            LLQuaternion previous;
            for (S32 step = 0; step < 21; ++step)
            {
                const F32 degrees = 180.f + static_cast<F32>(direction) *
                    (static_cast<F32>(step) - 10.f) * .1f;
                LLQuaternion object = LLQuaternion(degrees * DEG_TO_RAD, swing_axis) *
                    LLQuaternion(.7f, LLVector3::z_axis);
                if (step % 2) object = -1.f * object;
                const LLQuaternion followed = objectFollowRotation(
                    object, mask & 1, mask & 2, mask & 4, history);
                const LLQuaternion opposite = objectFollowRotation(
                    -1.f * object, mask & 1, mask & 2, mask & 4, opposite_history);
                ensure("path is sign invariant", sameRotation(followed, opposite));
                if (step > 0)
                    ensure("0.1 degree input steps cannot produce large partial-axis jumps",
                           sameRotation(followed, previous, .003f));
                previous = followed;
            }
        }
    }
}
template<> template<>
void alvcambonepov_test_object::test<24>()
{
    set_test_name("Object POV intrinsic Z-Y-X dense matrix round trip");
    // Independent construction spells out the viewer order, including pitches
    // outside the principal range and exact +/-90-degree singularities.
    for (S32 yaw = -180; yaw <= 180; yaw += 15)
    {
        for (S32 pitch = -180; pitch <= 180; pitch += 5)
        {
            for (S32 roll = -180; roll <= 180; roll += 15)
            {
                const LLQuaternion object =
                    LLQuaternion(static_cast<F32>(roll) * DEG_TO_RAD, LLVector3::x_axis) *
                    LLQuaternion(static_cast<F32>(pitch) * DEG_TO_RAD, LLVector3::y_axis) *
                    LLQuaternion(static_cast<F32>(yaw) * DEG_TO_RAD, LLVector3::z_axis);
                ObjectFollowHistory history;
                const ObjectEulerAngles angles = objectEulerAngles(object, history);
                ensure("principal pitch is bounded", std::fabs(angles.mPitch) <= F_PI_BY_TWO);
                ensure("compose(decompose(R)) reproduces every grid orientation", sameRotation(
                    composeObjectRotation(angles.mYaw, angles.mPitch, angles.mRoll), object));
            }
        }
    }
}

template<> template<>
void alvcambonepov_test_object::test<25>()
{
    set_test_name("Object POV see-saw, rocking and combined boat Euler angles");
    ObjectFollowHistory pitch_history;
    ObjectFollowHistory roll_history;
    for (S32 frame = 0; frame < 720; ++frame)
    {
        const F32 time = static_cast<F32>(frame) * .03f;
        // A see-saw stays inside the principal pitch range. Roll can turn
        // through multiple complete circles without any accumulated branch.
        const F32 theta = 1.3f * std::sin(time);
        const F32 phi = static_cast<F32>(frame - 360) * .04f;
        const F32 psi = time * .7f;
        const LLQuaternion pitch(theta, LLVector3::y_axis);
        const LLQuaternion roll(phi, LLVector3::x_axis);
        const LLQuaternion yaw(psi, LLVector3::z_axis);
        ensure("pure see-saw tracks theta exactly", sameRotation(
            objectFollowRotation(pitch, false, true, false, pitch_history), pitch));
        ensure("pure rocking/rolling tracks phi exactly", sameRotation(
            objectFollowRotation(roll, false, false, true, roll_history), roll));
        const LLQuaternion boat = roll * pitch * yaw;
        ensure("combined boat pitch tracks theta exactly", sameRotation(
            objectFollowRotation(boat, false, true, false, pitch_history), pitch));
        ensure("combined boat roll tracks phi exactly", sameRotation(
            objectFollowRotation(boat, false, false, true, roll_history), roll));
    }
    std::mt19937 random(0x504f56u);
    std::uniform_real_distribution<F32> component(-1.f, 1.f);
    for (S32 sample = 0; sample < 2000; ++sample)
    {
        LLQuaternion object(component(random), component(random), component(random), component(random));
        object.normalize();
        LLQuaternion normalized = object;
        normalized.normalize();
        for (U8 mask = 0; mask < 8; ++mask)
        {
            ObjectFollowHistory positive;
            ObjectFollowHistory negative;
            const LLQuaternion followed = objectFollowRotation(
                object, mask & 1, mask & 2, mask & 4, positive);
            ensure("all random masks are sign invariant", sameRotation(followed,
                objectFollowRotation(-1.f * object, mask & 1, mask & 2, mask & 4, negative)));
            if (mask == 7)
                ensure("full follow is exact normalized passthrough", followed == normalized);
            if (mask == 0)
                ensure("no axes is exact identity", followed == LLQuaternion());
        }
    }
}

template<> template<>
void alvcambonepov_test_object::test<26>()
{
    set_test_name("Object POV bounded Euler masks on random smooth non-gimbal paths");
    std::mt19937 random(0x5a5958u);
    std::uniform_real_distribution<F32> phase(-F_PI, F_PI);
    for (S32 path = 0; path < 24; ++path)
    {
        const F32 yaw_phase = phase(random);
        const F32 pitch_phase = phase(random);
        const F32 roll_phase = phase(random);
        for (U8 mask = 0; mask < 8; ++mask)
        {
            ObjectFollowHistory history;
            LLQuaternion previous_object;
            LLQuaternion previous_followed;
            for (S32 frame = 0; frame < 600; ++frame)
            {
                const F32 time = static_cast<F32>(frame) * .02f;
                const F32 psi = yaw_phase + .8f * time + .3f * std::sin(time);
                const F32 theta = .85f * std::sin(.7f * time + pitch_phase);
                const F32 phi = 1.1f * std::sin(.9f * time + roll_phase);
                const LLQuaternion object = LLQuaternion(phi, LLVector3::x_axis) *
                    LLQuaternion(theta, LLVector3::y_axis) * LLQuaternion(psi, LLVector3::z_axis);
                const LLQuaternion expected =
                    LLQuaternion(mask & 4 ? phi : 0.f, LLVector3::x_axis) *
                    LLQuaternion(mask & 2 ? theta : 0.f, LLVector3::y_axis) *
                    LLQuaternion(mask & 1 ? psi : 0.f, LLVector3::z_axis);
                const LLQuaternion followed = objectFollowRotation(
                    object, mask & 1, mask & 2, mask & 4, history);
                ensure("smooth path selects conventional angles", sameRotation(followed, expected));
                if (frame > 0)
                {
                    // |theta| <= .85 keeps cos(theta) > .65. The Euler chart
                    // has a bounded inverse here, including yaw's +/-pi wrap.
                    ensure("selected step is bounded relative to the input step",
                        rotationBasisStep(followed, previous_followed) <=
                            4.f * rotationBasisStep(object, previous_object) + 2e-6f);
                }
                previous_object = object;
                previous_followed = followed;
            }
        }
    }
}

template<> template<>
void alvcambonepov_test_object::test<27>()
{
    set_test_name("Object POV held-yaw gimbal band, exact full follow and principal exit");
    for (S32 direction : {-1, 1})
    {
        const F32 sign = static_cast<F32>(direction);
        for (U8 mask = 0; mask < 8; ++mask)
        {
            ObjectFollowHistory history;
            const LLQuaternion seed = composeObjectRotation(.6f, sign * 89.99f * DEG_TO_RAD, -.4f);
            objectFollowRotation(seed, mask & 1, mask & 2, mask & 4, history);
            const F32 held_yaw = history.mYaw;
            LLQuaternion previous;
            for (S32 step = 0; step <= 4; ++step)
            {
                const F32 delta = (static_cast<F32>(step) - 2.f) * .0025f;
                const F32 psi = .6f + static_cast<F32>(step) * .02f;
                const F32 phi = -.4f + static_cast<F32>(step) * .03f;
                const F32 theta = sign * (90.f + delta) * DEG_TO_RAD;
                ensure("fixture is inside the named gimbal band",
                    std::fabs(std::cos(theta)) < OBJECT_POV_GIMBAL_COS_THRESHOLD);
                const LLQuaternion object = composeObjectRotation(psi, theta, phi);
                const LLQuaternion followed = objectFollowRotation(
                    object, mask & 1, mask & 2, mask & 4, history);
                ensure("gimbal never emits NaN", followed.isFinite());
                ensure("yaw is held throughout the guard band", history.mYaw == held_yaw);
                LLQuaternion normalized = object;
                normalized.normalize();
                if (mask == 7)
                {
                    ensure("full follow bypasses Euler even near gimbal", followed == normalized);
                }
                else
                {
                    const LLQuaternion expected = composeObjectRotation(
                        mask & 1 ? held_yaw : 0.f,
                        mask & 2 ? sign * (90.f - std::fabs(delta)) * DEG_TO_RAD : 0.f,
                        mask & 4 ? phi + sign * (held_yaw - psi) : 0.f);
                    ensure("partial masks obey the coupled held-yaw policy", sameRotation(followed, expected));
                }
                if (step > 0)
                    ensure("in-band steps obey the prescribed angular-rate bound",
                           rotationBasisStep(followed, previous) < .051f);
                previous = followed;

                ObjectFollowHistory opposite_history = history;
                ensure("gimbal partial masks remain q/-q invariant", sameRotation(followed,
                    objectFollowRotation(-1.f * object, mask & 1, mask & 2, mask & 4, opposite_history)));
                ObjectFollowHistory fresh;
                const ObjectEulerAngles fresh_angles = objectEulerAngles(object, fresh);
                ensure("fresh gimbal yaw starts at zero", fresh_angles.mYaw == 0.f);
                if (step == 2)
                    ensure("exact singularity reconstructs with fresh zero yaw", sameRotation(
                        composeObjectRotation(fresh_angles.mYaw, fresh_angles.mPitch, fresh_angles.mRoll), object));
            }
            // Beyond +/-90, the principal chart changes yaw and roll by pi.
            // The finite-band hold ends here: partial masks may make that
            // explicit branch change; full follow remains the exact pose.
            // Immediately outside the band the Euler chart is ill-conditioned:
            // float quaternion quantization changes its individual angles.
            // Check branch selection and exact full-follow there, then use a
            // well-conditioned authored-angle oracle for the strict mask check.
            ObjectFollowHistory edge_history = history;
            const LLQuaternion edge = composeObjectRotation(.68f, sign * 90.006f * DEG_TO_RAD, -.28f);
            const LLQuaternion edge_followed = objectFollowRotation(
                edge, mask & 1, mask & 2, mask & 4, edge_history);
            ensure("yaw resumes its principal branch immediately outside the band",
                   std::fabs(edge_history.mYaw - held_yaw) > 3.f);
            ensure("guard exit is finite", edge_followed.isFinite());
            if (mask == 7)
            {
                LLQuaternion normalized = edge;
                normalized.normalize();
                ensure("full follow is exact immediately outside the band", edge_followed == normalized);
            }
            const LLQuaternion outside = composeObjectRotation(.68f, sign * 90.5f * DEG_TO_RAD, -.28f);
            const LLQuaternion followed = objectFollowRotation(
                outside, mask & 1, mask & 2, mask & 4, history);
            const LLQuaternion expected = composeObjectRotation(mask & 1 ? .68f + F_PI : 0.f,
                mask & 2 ? sign * 89.5f * DEG_TO_RAD : 0.f, mask & 4 ? -.28f + F_PI : 0.f);
            ensure("guard exit selects the prescribed principal branch", sameRotation(followed, expected));
            if (mask == 1 || mask == 4)
                ensure("yaw/roll-only guard exit has the documented pi branch change",
                       rotationBasisStep(followed, previous) > 1.99f);
            if (mask == 7)
                ensure("full follow remains exact on guard exit", sameRotation(followed, outside));
        }
    }
}

template<> template<>
void alvcambonepov_test_object::test<28>()
{
    set_test_name("Object POV full mixed-axis turn then a new small rotation axis");
    const LLVector3 axis(.6f, .8f, 0.f);
    for (U8 mask = 0; mask < 8; ++mask)
    {
        ObjectFollowHistory history;
        LLQuaternion previous;
        for (S32 step = 0; step <= 3600; ++step)
        {
            const LLQuaternion object(static_cast<F32>(step) * .1f * DEG_TO_RAD, axis);
            const LLQuaternion followed = objectFollowRotation(
                object, mask & 1, mask & 2, mask & 4, history);
            if (step > 0)
                ensure("the entire mixed-axis turn has bounded partial steps",
                       sameRotation(followed, previous, .003f));
            previous = followed;
        }
        ensure("a complete turn retains no swing-log winding", sameRotation(previous, LLQuaternion()));
        for (F32 degrees : {.010f, .012f, .1f})
        {
            const LLQuaternion object(degrees * DEG_TO_RAD, LLVector3::x_axis);
            const LLQuaternion followed = objectFollowRotation(
                object, mask & 1, mask & 2, mask & 4, history);
            ObjectFollowHistory fresh;
            ensure("new-axis result equals a fresh decomposition", sameRotation(followed,
                objectFollowRotation(object, mask & 1, mask & 2, mask & 4, fresh)));
            ensure("small new-axis motion cannot cause 72/144-degree jumps",
                   sameRotation(followed, previous, .003f));
            previous = followed;
        }
    }
}

template<> template<>
void alvcambonepov_test_object::test<29>()
{
    set_test_name("Object POV yaw history resets before rewind and after loss/invalid pose");
    const LLQuaternion seed = composeObjectRotation(.7f, .3f, -.4f);
    const LLQuaternion reacquired = composeObjectRotation(1.2f, F_PI_BY_TWO, -.6f);
    for (U8 mask = 0; mask < 8; ++mask)
    {
        ObjectFollowHistory fresh;
        const LLQuaternion expected = objectFollowRotation(
            reacquired, mask & 1, mask & 2, mask & 4, fresh);
        ObjectFollowHistory rewind;
        objectFollowRotation(seed, mask & 1, mask & 2, mask & 4, rewind);
        ensure("ordinary forward time retains yaw", !rewind.resetForTimeReversal(6.0, 5.0) && rewind.mHaveYaw);
        ensure("rewind clears yaw before decomposition", rewind.resetForTimeReversal(1.0, 5.0) && !rewind.mHaveYaw);
        ensure("rewind gives the fresh attachment orientation", sameRotation(expected,
            objectFollowRotation(reacquired, mask & 1, mask & 2, mask & 4, rewind)));

        ObjectFollowHistory lost;
        objectFollowRotation(seed, mask & 1, mask & 2, mask & 4, lost);
        // tickObject's hold_position calls this on loss, invalid render pose,
        // invalid offset or a failed publication, even if the UUID is unchanged.
        lost.reset();
        ensure("same-UUID reacquisition starts with fresh yaw", sameRotation(expected,
            objectFollowRotation(reacquired, mask & 1, mask & 2, mask & 4, lost)));
        objectFollowRotation(seed, mask & 1, mask & 2, mask & 4, lost);
        const LLQuaternion invalid(std::numeric_limits<F32>::quiet_NaN(), 0.f, 0.f, 1.f);
        ensure("invalid orientation safely returns identity", objectFollowRotation(
            invalid, mask & 1, mask & 2, mask & 4, lost) == LLQuaternion());
        ensure("invalid orientation clears history", !lost.mHaveYaw);
        ensure("valid pose after invalid input equals fresh attachment", sameRotation(expected,
            objectFollowRotation(reacquired, mask & 1, mask & 2, mask & 4, lost)));
    }
}
} // namespace tut
