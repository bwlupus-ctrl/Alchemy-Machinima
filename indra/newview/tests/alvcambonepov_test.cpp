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

#include <cmath>

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
} // namespace tut
