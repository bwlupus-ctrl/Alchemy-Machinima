#include "linden_common.h"
#include "../test/lltut.h"
#include "../alreflectmath.h"
#include "../alghostanimmirrormath.h"
#include <limits>
#include <algorithm>
#include <set>
#include <vector>

namespace tut
{
namespace
{
// Literal inventory from avatar_skeleton.xml, pinned here rather than parsed at runtime.
const char* const BONES[] = {
    "mPelvis", "mSpine1", "mSpine2", "mTorso",
    "mSpine3", "mSpine4", "mChest", "mNeck",
    "mHead", "mSkull", "mEyeRight", "mEyeLeft",
    "mFaceRoot", "mFaceEyeAltRight", "mFaceEyeAltLeft", "mFaceForeheadLeft",
    "mFaceForeheadRight", "mFaceEyebrowOuterLeft", "mFaceEyebrowCenterLeft", "mFaceEyebrowInnerLeft",
    "mFaceEyebrowOuterRight", "mFaceEyebrowCenterRight", "mFaceEyebrowInnerRight", "mFaceEyeLidUpperLeft",
    "mFaceEyeLidLowerLeft", "mFaceEyeLidUpperRight", "mFaceEyeLidLowerRight", "mFaceEar1Left",
    "mFaceEar2Left", "mFaceEar1Right", "mFaceEar2Right", "mFaceNoseLeft",
    "mFaceNoseCenter", "mFaceNoseRight", "mFaceCheekLowerLeft", "mFaceCheekUpperLeft",
    "mFaceCheekLowerRight", "mFaceCheekUpperRight", "mFaceJaw", "mFaceChin",
    "mFaceTeethLower", "mFaceLipLowerLeft", "mFaceLipLowerRight", "mFaceLipLowerCenter",
    "mFaceTongueBase", "mFaceTongueTip", "mFaceJawShaper", "mFaceForeheadCenter",
    "mFaceNoseBase", "mFaceTeethUpper", "mFaceLipUpperLeft", "mFaceLipUpperRight",
    "mFaceLipCornerLeft", "mFaceLipCornerRight", "mFaceLipUpperCenter", "mFaceEyecornerInnerLeft",
    "mFaceEyecornerInnerRight", "mFaceNoseBridge", "mCollarLeft", "mShoulderLeft",
    "mElbowLeft", "mWristLeft", "mHandMiddle1Left", "mHandMiddle2Left",
    "mHandMiddle3Left", "mHandIndex1Left", "mHandIndex2Left", "mHandIndex3Left",
    "mHandRing1Left", "mHandRing2Left", "mHandRing3Left", "mHandPinky1Left",
    "mHandPinky2Left", "mHandPinky3Left", "mHandThumb1Left", "mHandThumb2Left",
    "mHandThumb3Left", "mCollarRight", "mShoulderRight", "mElbowRight",
    "mWristRight", "mHandMiddle1Right", "mHandMiddle2Right", "mHandMiddle3Right",
    "mHandIndex1Right", "mHandIndex2Right", "mHandIndex3Right", "mHandRing1Right",
    "mHandRing2Right", "mHandRing3Right", "mHandPinky1Right", "mHandPinky2Right",
    "mHandPinky3Right", "mHandThumb1Right", "mHandThumb2Right", "mHandThumb3Right",
    "mWingsRoot", "mWing1Left", "mWing2Left", "mWing3Left",
    "mWing4Left", "mWing4FanLeft", "mWing1Right", "mWing2Right",
    "mWing3Right", "mWing4Right", "mWing4FanRight", "mHipRight",
    "mKneeRight", "mAnkleRight", "mFootRight", "mToeRight",
    "mHipLeft", "mKneeLeft", "mAnkleLeft", "mFootLeft",
    "mToeLeft", "mTail1", "mTail2", "mTail3",
    "mTail4", "mTail5", "mTail6", "mGroin",
    "mHindLimbsRoot", "mHindLimb1Left", "mHindLimb2Left", "mHindLimb3Left",
    "mHindLimb4Left", "mHindLimb1Right", "mHindLimb2Right", "mHindLimb3Right",
    "mHindLimb4Right",
};
const char* const VOLUMES[] = {
    "PELVIS", "BUTT", "BELLY", "LEFT_HANDLE",
    "RIGHT_HANDLE", "LOWER_BACK", "CHEST", "LEFT_PEC",
    "RIGHT_PEC", "UPPER_BACK", "NECK", "HEAD",
    "L_CLAVICLE", "L_UPPER_ARM", "L_LOWER_ARM", "L_HAND",
    "R_CLAVICLE", "R_UPPER_ARM", "R_LOWER_ARM", "R_HAND",
    "R_UPPER_LEG", "R_LOWER_LEG", "R_FOOT", "L_UPPER_LEG",
    "L_LOWER_LEG", "L_FOOT",
};
}
struct ghost_anim_mirror_data {};
typedef test_group<ghost_anim_mirror_data> ghost_anim_mirror_group;
typedef ghost_anim_mirror_group::object ghost_anim_mirror_object;
ghost_anim_mirror_group ghost_anim_mirror_tests("alghostanimmirror");

template<> template<> void ghost_anim_mirror_object::test<1>()
{
    // Independent geometric oracle: a positive left-arm X rotation must become
    // a negative right-arm X rotation, not merely pass a double-flip test.
    const ALReflect::LocalPose left{LLQuaternion(F_PI_BY_TWO, LLVector3::x_axis), LLVector3(.3f, .4f, .5f)};
    ALReflect::LocalPose right, twice;
    ensure("valid pose", ALReflect::reflectLocalPose(left, right));
    ensure_equals("left arm maps to right", ALReflect::partner("mShoulderLeft"), "mShoulderRight");
    const LLVector3 rotated = LLVector3::y_axis * right.rotation;
    ensure_distance("right arm y", rotated.mV[VY], 0.f, 1e-5f);
    ensure_distance("right arm z", rotated.mV[VZ], -1.f, 1e-5f);
    ensure_distance("sagittal position", right.position.mV[VY], -.4f, 1e-6f);
    ensure_distance("position X unchanged", right.position.mV[VX], .3f, 1e-6f);
    ensure_distance("position Z unchanged", right.position.mV[VZ], .5f, 1e-6f);
    const ALReflect::LocalPose left_yaw{LLQuaternion(F_PI_BY_TWO, LLVector3::z_axis), left.position};
    ALReflect::LocalPose right_yaw;
    ensure("yaw valid", ALReflect::reflectLocalPose(left_yaw, right_yaw));
    const LLVector3 yaw_forward = LLVector3::x_axis * right_yaw.rotation;
    ensure_distance("partner yaw X", yaw_forward.mV[VX], 0.f, 1e-5f);
    ensure_distance("partner yaw Y", yaw_forward.mV[VY], -1.f, 1e-5f);
    ensure("second reflection", ALReflect::reflectLocalPose(right, twice));
    for (S32 k = 0; k < 4; ++k) ensure_distance("rotation involution", twice.rotation.mQ[k], left.rotation.mQ[k], 1e-6f);
    for (S32 k = 0; k < 3; ++k) ensure_distance("position involution", twice.position.mV[k], left.position.mV[k], 1e-6f);
}

template<> template<> void ghost_anim_mirror_object::test<2>()
{
    for (F32 invalid : {std::numeric_limits<F32>::infinity(), -std::numeric_limits<F32>::infinity(),
                        std::numeric_limits<F32>::quiet_NaN()})
    {
        ensure("bit check rejects invalid", !ALReflect::finiteBits(invalid));
        for (S32 k = 0; k < 7; ++k)
        {
            ALReflect::LocalPose input, output;
            output.position = LLVector3(7.f, 8.f, 9.f);
            if (k < 4) input.rotation.mQ[k] = invalid;
            else input.position.mV[k - 4] = invalid;
            ensure("invalid pose rejected", !ALReflect::reflectLocalPose(input, output));
            ensure_equals("rejection leaves output untouched", output.position, LLVector3(7.f, 8.f, 9.f));
        }
    }
    ensure("zero finite", ALReflect::finiteBits(0.f));
    ensure("negative zero finite", ALReflect::finiteBits(-0.f));
    ensure("largest finite", ALReflect::finiteBits(std::numeric_limits<F32>::max()));
}
template<> template<> void ghost_anim_mirror_object::test<3>()
{
    std::vector<std::string> joints(std::begin(BONES), std::end(BONES));
    joints.insert(joints.end(), std::begin(VOLUMES), std::end(VOLUMES));
    ensure_equals("complete skeleton and collision table", joints.size(), size_t(159));
    ensure_equals("unique joint names", std::set<std::string>(joints.begin(), joints.end()).size(), joints.size());
    std::vector<size_t> partners;
    std::vector<ALReflect::LocalPose> input(joints.size()), output(joints.size()), twice(joints.size());
    size_t paired = 0;
    for (size_t i = 0; i < joints.size(); ++i)
    {
        const auto other = ALReflect::partner(joints[i]);
        const auto found = std::find(joints.begin(), joints.end(), other);
        ensure("partner exists: " + joints[i], found != joints.end());
        ensure_equals("symmetric partner: " + joints[i], ALReflect::partner(other), joints[i]);
        partners.push_back(static_cast<size_t>(std::distance(joints.begin(), found)));
        if (other != joints[i]) ++paired;
        input[i] = {LLQuaternion(.01f * static_cast<F32>(i), LLVector3::x_axis),
                    LLVector3(.1f * static_cast<F32>(i), .2f, .3f)};
    }
    ensure_equals("Bento and collision pairs", paired, size_t(118));
    ensure("complete pose reflection", ALReflect::reflectPose(input.data(), output.data(), partners.data(), joints.size()));
    ensure("complete pose involution", ALReflect::reflectPose(output.data(), twice.data(), partners.data(), joints.size()));
    for (size_t i = 0; i < joints.size(); ++i)
    {
        for (S32 k = 0; k < 4; ++k) ensure_distance("pose rotation", twice[i].rotation.mQ[k], input[i].rotation.mQ[k], 1e-6f);
        for (S32 k = 0; k < 3; ++k) ensure_distance("pose position", twice[i].position.mV[k], input[i].position.mV[k], 1e-6f);
    }
    partners[0] = joints.size();
    ensure("invalid table rejected", !ALReflect::reflectPose(input.data(), output.data(), partners.data(), joints.size()));
}

template<> template<> void ghost_anim_mirror_object::test<4>()
{
    struct Rig
    {
        ALReflect::LocalPose root;
        ALReflect::LocalPose bones[2];
    } input, output;
    input.root = {LLQuaternion(.7f, LLVector3::z_axis), LLVector3(10.f, 20.f, 30.f)};
    output.root = input.root;
    input.bones[0] = {LLQuaternion(.3f, LLVector3::x_axis), LLVector3(0.f, 1.f, 0.f)};
    input.bones[1] = {LLQuaternion(.2f, LLVector3::z_axis), LLVector3(0.f, -1.f, 0.f)};
    const size_t partners[] = {1, 0};
    ensure("reflect local bones", ALReflect::reflectPose(input.bones, output.bones, partners, 2));
    ensure_equals("root position untouched", output.root.position, input.root.position);
    ensure_equals("root facing untouched", output.root.rotation, input.root.rotation);
    // Distinct sentinels: a premature publication of input[0] into output[1]
    // must be observable even though input[0] itself remains valid/unchanged.
    output.bones[0] = {LLQuaternion(.9f, LLVector3::y_axis), LLVector3(7.f, 8.f, 9.f)};
    output.bones[1] = {LLQuaternion(.8f, LLVector3::x_axis), LLVector3(10.f, 11.f, 12.f)};
    const Rig held = output;
    input.bones[1].position.mV[0] = std::numeric_limits<F32>::quiet_NaN();
    ensure("whole pose rejects nonfinite", !ALReflect::reflectPose(input.bones, output.bones, partners, 2));
    for (S32 i = 0; i < 2; ++i)
    {
        ensure("invalid input publishes no partial rotation", ALGhostAnimMirrorMath::sameRotationBits(output.bones[i].rotation, held.bones[i].rotation));
        ensure("invalid input publishes no partial position", ALGhostAnimMirrorMath::samePositionBits(output.bones[i].position, held.bones[i].position));
    }
    input.bones[1].position.mV[0] = 0.f; // table-only failure below must have finite input
    const size_t asymmetric[] = {1, 1};
    ensure("asymmetric pair table rejected", !ALReflect::reflectPose(input.bones, output.bones, asymmetric, 2));
}

template<> template<> void ghost_anim_mirror_object::test<5>()
{
    // Rows are current drive, columns stored resume drive, in the public enum
    // order Mirror/Directed/Frozen/True Mirror/Reflect. Only Directed and its
    // paused Frozen pose are eligible; unrelated Frozen owners stay unflipped.
    const bool expected[5][5] = {
        {false, false, false, false, false},
        {true,  true,  true,  true,  true },
        {false, true,  false, false, false},
        {false, false, false, false, false},
        {false, false, false, false, false}
    };
    for (S32 drive = 0; drive < 5; ++drive)
    {
        for (S32 resume = 0; resume < 5; ++resume)
        {
            ensure_equals("owner eligibility", ALGhostAnimMirrorMath::directedMirrorActive(drive, resume, true), expected[drive][resume]);
            ensure("checkbox off always releases", !ALGhostAnimMirrorMath::directedMirrorActive(drive, resume, false));
        }
    }
    ensure("Pause from Directed keeps mirror", ALGhostAnimMirrorMath::directedMirrorActive(
        ALGhostAnimMirrorMath::DRIVE_FROZEN, ALGhostAnimMirrorMath::DRIVE_DIRECTED, true));
    ensure("unknown drive is ineligible", !ALGhostAnimMirrorMath::directedMirrorActive(
        -1, ALGhostAnimMirrorMath::DRIVE_DIRECTED, true));
}

template<> template<> void ghost_anim_mirror_object::test<6>()
{
    using namespace ALGhostAnimMirrorMath;
    const ALReflect::LocalPose raw{LLQuaternion(.4f, LLVector3::z_axis), LLVector3(1.f, 2.f, 3.f)};
    ALReflect::LocalPose written;
    ensure("stamp", ALReflect::reflectLocalPose(raw, written));
    auto foreign = written;
    foreign.rotation = LLQuaternion(.8f, LLVector3::x_axis);
    auto restored = guardedRestore(foreign, raw, written);
    ensure("foreign rotation survives", sameRotationBits(restored.rotation, foreign.rotation));
    ensure("own position restored independently", samePositionBits(restored.position, raw.position));
    foreign = written;
    foreign.position = LLVector3(4.f, 5.f, 6.f);
    restored = guardedRestore(foreign, raw, written);
    ensure("foreign position survives", samePositionBits(restored.position, foreign.position));
    ensure("own rotation restored independently", sameRotationBits(restored.rotation, raw.rotation));
    foreign = written;
    written.position.mV[0] = 0.f;
    foreign.position.mV[0] = -0.f;
    restored = guardedRestore(foreign, raw, written);
    ensure("signed-zero foreign bits survive", samePositionBits(restored.position, foreign.position));
    ensure("bit equality differs from numerical equality", !samePositionBits(written.position, foreign.position));
    foreign = written;
    written.rotation.mQ[0] = 0.f;
    foreign.rotation.mQ[0] = -0.f;
    restored = guardedRestore(foreign, raw, written);
    ensure("signed-zero foreign rotation survives", sameRotationBits(restored.rotation, foreign.rotation));
    ensure("rotation equality is bit based", !sameRotationBits(written.rotation, foreign.rotation));
}

template<> template<> void ghost_anim_mirror_object::test<7>()
{
    using namespace ALGhostAnimMirrorMath;
    ensure_equals("pelvis ownership bit", gazeJointBit("mPelvis"), U32(GAZE_PELVIS));
    ensure_equals("torso ownership bit", gazeJointBit("mTorso"), U32(GAZE_TORSO));
    ensure_equals("chest ownership bit", gazeJointBit("mChest"), U32(GAZE_CHEST));
    ensure_equals("neck ownership bit", gazeJointBit("mNeck"), U32(GAZE_NECK));
    ensure_equals("head ownership bit", gazeJointBit("mHead"), U32(GAZE_HEAD));
    ensure_equals("eye ownership bit", gazeJointBit("mEyeLeft"), U32(GAZE_EYES));
    ensure_equals("alt eye ownership bit", gazeJointBit("mFaceEyeAltRight"), U32(GAZE_EYES));
    for (const char* name : {"mPelvis", "mTorso", "mChest", "mNeck", "mHead",
                           "mEyeLeft", "mEyeRight", "mFaceEyeAltLeft", "mFaceEyeAltRight"})
    {
        const U32 bit = gazeJointBit(name);
        ensure("gaze joint recognized", bit != 0);
        ensure("owned joint excluded", excluded(bit, 0, bit));
        ensure("partner cannot write into owned joint", excluded(0, bit, bit));
        ensure("no gaze full mirror", !excluded(bit, bit, 0));
    }
    ensure_equals("arm not a gaze joint", gazeJointBit("mShoulderLeft"), U32(0));
    ensure_equals("unowned face bone", gazeJointBit("mFaceJaw"), U32(0));
    ALReflect::LocalPose input[4], output[4];
    for (S32 i = 0; i < 4; ++i)
        input[i] = {LLQuaternion(.2f * static_cast<F32>(i + 1), LLVector3::z_axis), LLVector3(1.f, 2.f + static_cast<F32>(i), 3.f)};
    const size_t partners[] = {0, 2, 1, 3};
    const U8 skip[] = {1, 1, 0, 0}; // head and one eye; eye partner must also stay
    size_t bad = 0;
    ensure("table valid", validatePartners(partners, 4, bad));
    ensure("exclude publication", reflectPose(input, output, partners, skip, 4, bad));
    for (S32 i = 0; i < 3; ++i)
    {
        ensure("owned rotation unchanged", sameRotationBits(output[i].rotation, input[i].rotation));
        ensure("owned position unchanged", samePositionBits(output[i].position, input[i].position));
    }
    ensure_distance("body still mirrored", output[3].position.mV[VY], -5.f, 1e-6f);
}

template<> template<> void ghost_anim_mirror_object::test<8>()
{
    using namespace ALGhostAnimMirrorMath;
    ensure("Director Freeze retains authored Directed mirror", eligible(DRIVE_DIRECTED, DRIVE_MIRROR, true, DRIVE_FROZEN));
    ensure("no hidden flip on borrowed Directed clip", !eligible(DRIVE_MIRROR, DRIVE_DIRECTED, true, DRIVE_DIRECTED));
    ensure("no borrowed Frozen flip", !eligible(DRIVE_TRUE_MIRROR, DRIVE_DIRECTED, true, DRIVE_FROZEN));
    ensure("paused authored Directed allowed", eligible(DRIVE_FROZEN, DRIVE_DIRECTED, true, DRIVE_FROZEN));
    ensure("checkbox gates temporary drive", !eligible(DRIVE_DIRECTED, DRIVE_DIRECTED, false, DRIVE_FROZEN));
    for (S32 drive : {DRIVE_MIRROR, DRIVE_TRUE_MIRROR, DRIVE_REFLECT})
        ensure("other runtime owns pose", !eligible(DRIVE_DIRECTED, DRIVE_DIRECTED, true, drive));
}

template<> template<> void ghost_anim_mirror_object::test<9>()
{
    using namespace ALGhostAnimMirrorMath;
    const std::vector<std::string> names(std::begin(BONES), std::end(BONES));
    ensure_equals("Directed skeleton only", names.size(), size_t(133));
    std::vector<size_t> partners;
    for (const auto& name : names)
        partners.push_back(static_cast<size_t>(std::distance(names.begin(), std::find(names.begin(), names.end(), ALReflect::partner(name)))));
    size_t bad = 0;
    ensure("Directed Bento table symmetric", validatePartners(partners.data(), names.size(), bad));
    const size_t asymmetric[] = {1, 1};
    ensure("build rejects asymmetry", !validatePartners(asymmetric, 2, bad));
    const size_t paired[] = {1, 0};
    ALReflect::LocalPose input[2], output[2];
    input[0].position = LLVector3(1.f, 2.f, 3.f);
    input[1].position = LLVector3(4.f, 5.f, 6.f);
    output[0].position = LLVector3(7.f, 8.f, 9.f);
    output[1].position = LLVector3(10.f, 11.f, 12.f);
    input[1].position.mV[0] = std::numeric_limits<F32>::quiet_NaN();
    ensure("frame isolates nonfinite pair", reflectPose(input, output, paired, nullptr, 2, bad));
    ensure_equals("isolated index", bad, size_t(1));
    for (S32 i = 0; i < 2; ++i)
    {
        ensure("bad pair rotations held", sameRotationBits(output[i].rotation, input[i].rotation));
        ensure("bad pair positions held", samePositionBits(output[i].position, input[i].position));
    }
    input[1].position.mV[0] = 4.f;
    const size_t outside[] = {1, 2};
    ensure("frame bounds retained", !reflectPose(input, output, outside, nullptr, 2, bad));
}

template<> template<> void ghost_anim_mirror_object::test<10>()
{
    using namespace ALGhostAnimMirrorMath;
    const LLVector3 position(1.f, 2.f, 3.f);
    const LLQuaternion rotation(.1f, .2f, .3f, .9f);
    for (S32 k = 0; k < 3; ++k)
    {
        auto changed = position;
        changed.mV[k] += 1.f;
        ensure("each position component participates", !samePositionBits(position, changed));
    }
    for (S32 k = 0; k < 4; ++k)
    {
        auto changed = rotation;
        changed.mQ[k] += 1.f;
        ensure("each quaternion component participates", !sameRotationBits(rotation, changed));
    }
    // Keep a readable backing cell at the invalid index: without the explicit
    // bounds check the mutant sees an apparently symmetric, but illegal pair.
    const size_t outside[] = {2, 1, 0};
    size_t bad = 99;
    ensure("symmetry alone cannot validate an out-of-range index", !validatePartners(outside, 2, bad));
    ensure_equals("bad table index", bad, size_t(0));
}

template<> template<> void ghost_anim_mirror_object::test<11>()
{
    using namespace ALGhostAnimMirrorMath;
    // Pelvis -> twisted spine -> gaze-owned head -> gaze-owned eye.
    // Arms share the spine but remain wholly animation-owned.
    constexpr size_t count = 6;
    const size_t partners[] = {0, 1, 2, 3, 5, 4};
    const size_t parents[] = {count, 0, 1, 2, 1, 1};
    const F32 alpha[] = {0.f, 0.f, 1.f, 1.f, 0.f, 0.f};
    ALReflect::LocalPose input[count], output[count];
    input[0].rotation = LLQuaternion(.6f, LLVector3::z_axis) * LLQuaternion(.2f, LLVector3::x_axis);
    input[1].rotation = LLQuaternion(-.3f, LLVector3::z_axis) * LLQuaternion(.4f, LLVector3::x_axis);
    input[2].rotation = LLQuaternion(.25f, LLVector3::y_axis);
    input[2].position = LLVector3(.1f, .2f, .3f);
    input[3].rotation = LLQuaternion(.15f, LLVector3::z_axis);
    input[4] = {LLQuaternion(.5f, LLVector3::z_axis), LLVector3(.1f, .4f, .3f)};
    input[5] = {LLQuaternion(.2f, LLVector3::x_axis), LLVector3(.2f, -.5f, .6f)};
    const LLQuaternion root(.7f, LLVector3::z_axis);
    const LLQuaternion head_world = input[2].rotation * input[1].rotation * input[0].rotation * root;
    const LLQuaternion eye_world = input[3].rotation * head_world;
    LLQuaternion before[count], after[count];
    size_t bad = count;
    ensure("mirror body", reflectPose(input, output, partners, nullptr, count, bad));
    ensure("preserve world aim", preserveGaze(input, output, partners, parents, alpha, count, root, before, after));
    const LLQuaternion actual_head = output[2].rotation * output[1].rotation * output[0].rotation * root;
    const LLQuaternion actual_eye = output[3].rotation * actual_head;
    for (const LLVector3& axis : {LLVector3::x_axis, LLVector3::y_axis, LLVector3::z_axis})
    {
        ensure_distance("head world orientation preserved", (axis * actual_head - axis * head_world).length(), 0.f, 1e-5f);
        ensure_distance("owned child world follows parent", (axis * actual_eye - axis * eye_world).length(), 0.f, 1e-5f);
    }
    for (const LLVector3& axis : {LLVector3::x_axis, LLVector3::y_axis, LLVector3::z_axis})
        ensure_distance("fully owned child local unchanged up to roundoff",
            (axis * output[3].rotation - axis * input[3].rotation).length(), 0.f, 1e-5f);
    ensure("gaze retains mirrored position", samePositionBits(output[2].position, ALReflect::vector(input[2].position)));
    ALReflect::LocalPose right;
    ensure("arm oracle", ALReflect::reflectLocalPose(input[4], right));
    ensure("unowned arm mirrored", sameRotationBits(output[5].rotation, right.rotation));
    ensure("arm position mirrored", samePositionBits(output[5].position, right.position));
    auto restored = guardedRestore(output[2], input[2], output[2]);
    ensure("compensated output participates in guarded restore", sameRotationBits(restored.rotation, input[2].rotation));
}

template<> template<> void ghost_anim_mirror_object::test<12>()
{
    using namespace ALGhostAnimMirrorMath;
    GazeWeights weights;
    weights.record(GAZE_HEAD, .25f);
    weights.record(GAZE_HEAD, .1f);
    weights.record(GAZE_EYES, .75f);
    ensure_distance("max applied alpha retained", weights.weight(GAZE_HEAD), .25f, 1e-6f);
    ensure_distance("bits independent", weights.weight(GAZE_EYES), .75f, 1e-6f);
    ensure_distance("unwritten bone has no ownership", weights.weight(GAZE_CHEST), 0.f, 1e-6f);
    GazeWeights director;
    director.record(GAZE_HEAD, .5f);
    weights.merge(director);
    ensure_distance("layer merge uses max", weights.weight(GAZE_HEAD), .5f, 1e-6f);
    weights = {}; // the start of each evaluation, not a frame timer
    ensure_distance("new evaluation forgets previous writes", weights.weight(GAZE_HEAD | GAZE_EYES), 0.f, 1e-6f);
    const size_t partners[] = {0, 1}, parents[] = {2, 0};
    ALReflect::LocalPose input[2], output[2], mirrored[2];
    input[0].rotation = LLQuaternion(.6f, LLVector3::z_axis);
    input[1].rotation = LLQuaternion(.2f, LLVector3::z_axis);
    LLQuaternion before[2], after[2];
    size_t bad = 2;
    ensure("plain mirror", reflectPose(input, mirrored, partners, nullptr, 2, bad));
    F32 last = -1.f;
    for (F32 blend : {0.f, .25f, .5f, .75f, 1.f})
    {
        output[0] = mirrored[0]; output[1] = mirrored[1];
        const F32 alpha[] = {0.f, blend};
        ensure("blend", preserveGaze(input, output, partners, parents, alpha, 2, LLQuaternion::DEFAULT, before, after));
        const LLVector3 forward = LLVector3::x_axis * (output[1].rotation * output[0].rotation);
        const F32 yaw = atan2f(forward.mV[VY], forward.mV[VX]);
        ensure("blend is monotonic", yaw > last);
        last = yaw;
        if (blend == 0.f) ensure("zero equals plain mirror", sameRotationBits(output[1].rotation, mirrored[1].rotation));
        if (blend == .5f) ensure_distance("midpoint world yaw", yaw, 0.f, 1e-5f);
        if (blend == 1.f) ensure_distance("full ownership world yaw", yaw, .8f, 1e-5f);
    }
}

template<> template<> void ghost_anim_mirror_object::test<13>()
{
    using namespace ALGhostAnimMirrorMath;
    const size_t partners[] = {1, 0, 3, 2};
    const size_t parents[] = {4, 4, 4, 4};
    const F32 alpha[] = {1.f, 1.f, 0.f, 0.f};
    for (S32 channel = 0; channel < 7; ++channel)
    {
        ALReflect::LocalPose input[4], output[4];
        for (S32 i = 0; i < 4; ++i)
            input[i] = {LLQuaternion(.1f * static_cast<F32>(i + 1), LLVector3::z_axis),
                LLVector3(static_cast<F32>(i + 1), 2.f, 3.f)};
        const F32 invalid = std::numeric_limits<F32>::quiet_NaN();
        if (channel < 4) input[1].rotation.mQ[channel] = invalid;
        else input[1].position.mV[channel - 4] = invalid;
        size_t bad = 4;
        ensure("valid pairs published despite bad channel", reflectPose(input, output, partners, nullptr, 4, bad));
        ensure_equals("bad joint reported", bad, size_t(1));
        LLQuaternion before[4], after[4];
        ensure("gaze cannot corrupt isolated pair", preserveGaze(input, output, partners, parents, alpha, 4,
            LLQuaternion::DEFAULT, before, after));
        for (S32 i = 0; i < 2; ++i)
        {
            ensure("isolated rotation input bits retained", sameRotationBits(output[i].rotation, input[i].rotation));
            ensure("isolated position input bits retained", samePositionBits(output[i].position, input[i].position));
        }
        for (size_t i = 2; i < 4; ++i)
        {
            ALReflect::LocalPose expected;
            ensure("finite oracle", ALReflect::reflectLocalPose(input[partners[i]], expected));
            ensure("valid rotation still mirrored", sameRotationBits(output[i].rotation, expected.rotation));
            ensure("valid position still mirrored", samePositionBits(output[i].position, expected.position));
        }
    }
}

template<> template<> void ghost_anim_mirror_object::test<14>()
{
    using namespace ALGhostAnimMirrorMath;
    const size_t partners[] = {0, 1, 2}, parents[] = {3, 0, 1};
    const F32 alpha[] = {0.f, .5f, 1.f};
    ALReflect::LocalPose input[3], output[3];
    input[0].rotation = LLQuaternion(.5f, LLVector3::z_axis) * LLQuaternion(.1f, LLVector3::x_axis);
    input[1].rotation = LLQuaternion(.3f, LLVector3::z_axis);
    input[2].rotation = LLQuaternion(.2f, LLVector3::y_axis);
    const LLQuaternion root(.4f, LLVector3::z_axis);
    const LLQuaternion expected_world = input[2].rotation * input[1].rotation * input[0].rotation * root;
    size_t bad = 3;
    ensure("mirror", reflectPose(input, output, partners, nullptr, 3, bad));
    LLQuaternion before[3], after[3];
    ensure("partial parent ownership", preserveGaze(input, output, partners, parents, alpha, 3,
        root, before, after));
    const LLQuaternion actual_world = output[2].rotation * output[1].rotation * output[0].rotation * root;
    for (const LLVector3& axis : {LLVector3::x_axis, LLVector3::y_axis, LLVector3::z_axis})
        ensure_distance("fully owned child aims under partially owned parent",
            (axis * actual_world - axis * expected_world).length(), 0.f, 1e-5f);
}


template<> template<> void ghost_anim_mirror_object::test<15>()
{
    using namespace ALGhostAnimMirrorMath;
    // joint0 unowned spine-like (mirrored, nonzero twist); joints 1<->2 owned eye-like pair under it.
    const size_t partners[] = {0, 2, 1};
    const size_t parents[] = {3, 0, 0};
    const F32 alpha[] = {0.f, 1.f, 1.f};
    ALReflect::LocalPose input[3], output[3];
    input[0].rotation = LLQuaternion(.7f, LLVector3::z_axis) * LLQuaternion(.4f, LLVector3::x_axis);
    input[1].rotation = LLQuaternion(.2f, LLVector3::y_axis);
    input[2].rotation = LLQuaternion(.3f, LLVector3::y_axis);
    input[2].position.mV[1] = std::numeric_limits<F32>::quiet_NaN();
    size_t bad = 3;
    ensure("frame", reflectPose(input, output, partners, nullptr, 3, bad));
    LLQuaternion before[3], after[3];
    ensure("gaze", preserveGaze(input, output, partners, parents, alpha, 3, LLQuaternion(.5f, LLVector3::x_axis), before, after));
    for (int i = 1; i < 3; ++i)
        ensure("isolated pair untouched by compensation", sameRotationBits(output[i].rotation, input[i].rotation));
    // out-of-order parent table must be refused (scratch would be read uninitialised)
    const size_t bad_parents[] = {1, 0, 0};
    ensure("parent-after-child rejected", !preserveGaze(input, output, partners, bad_parents, alpha, 3, LLQuaternion::DEFAULT, before, after));
    const size_t self_parent[] = {0, 0, 0};
    ensure("self parent rejected", !preserveGaze(input, output, partners, self_parent, alpha, 3, LLQuaternion::DEFAULT, before, after));
}

template<> template<> void ghost_anim_mirror_object::test<16>()
{
    using namespace ALGhostAnimMirrorMath;
    // Fable N2: the evaluated input is already half eased (30 degrees).
    // Mirror animation (-20) -> gaze endpoint (+40) must yield +10, not zero.
    const LLQuaternion animation(20.f * DEG_TO_RAD, LLVector3::z_axis);
    const LLQuaternion target(40.f * DEG_TO_RAD, LLVector3::z_axis);
    const size_t partners[] = {0}, parents[] = {1};
    for (F32 blend : {0.f, .5f, 1.f})
    {
        ALReflect::LocalPose input[1], output[1];
        input[0].rotation = nlerp(blend, animation, target);
        GazeWeights gaze;
        gaze.observe("mHead", blend, animation, target);
        const GazeSample samples[] = {gaze.samples[gazeJointSlot("mHead")]};
        const F32 alpha[] = {gaze.weight(GAZE_HEAD)};
        size_t bad = 1;
        ensure("mirror eased input", reflectPose(input, output, partners, nullptr, 1, bad));
        LLQuaternion before[1], after[1];
        ensure("single ease", preserveGaze(input, output, partners, parents, alpha, 1,
            LLQuaternion::DEFAULT, before, after, samples));
        const LLVector3 forward = LLVector3::x_axis * output[0].rotation;
        const F32 yaw = atan2f(forward.mV[VY], forward.mV[VX]) * RAD_TO_DEG;
        ensure_distance("correct eased mirrored yaw", yaw, blend == .5f ? 10.f : blend == 0.f ? -20.f : 40.f, 1e-4f);
    }
}

template<> template<> void ghost_anim_mirror_object::test<17>()
{
    using namespace ALGhostAnimMirrorMath;
    GazeWeights gaze;
    const LLQuaternion first(.2f, LLVector3::z_axis), later(.3f, LLVector3::z_axis);
    const LLQuaternion target1(.5f, LLVector3::z_axis), target2(.6f, LLVector3::z_axis);
    gaze.observe("mHead", .8f, first, target1);
    gaze.observe("mHead", .4f, later, target2);
    const auto& sample = gaze.samples[gazeJointSlot("mHead")];
    ensure("first pre-write local retained", sameRotationBits(sample.preLocal, first));
    ensure("final aimed world retained", sameRotationBits(sample.targetWorld, target2));
    ensure_distance("max alpha across writes", gaze.weight(GAZE_HEAD), .8f, 1e-6f);
    GazeWeights merged;
    merged.merge(gaze);
    ensure("sample survives layer merge", sameRotationBits(merged.samples[gazeJointSlot("mHead")].targetWorld, target2));
    merged = {};
    ensure("evaluation reset clears samples", !merged.samples[gazeJointSlot("mHead")].written);
    const char* eyes[] = {"mEyeLeft", "mEyeRight", "mFaceEyeAltLeft", "mFaceEyeAltRight"};
    for (size_t i = 0; i < 4; ++i)
        gaze.observe(eyes[i], .5f, LLQuaternion(.1f * static_cast<F32>(i), LLVector3::z_axis), target1);
    for (size_t i = 0; i < 4; ++i)
        ensure_distance("eyes retain independent pre-write locals",
            gaze.samples[gazeJointSlot(eyes[i])].preLocal.mQ[VZ], sinf(.05f * static_cast<F32>(i)), 1e-6f);
}

template<> template<> void ghost_anim_mirror_object::test<18>()
{
    using namespace ALGhostAnimMirrorMath;
    // No recorded solver writes: even with animated eye locals, eyes follow
    // the mirrored head. Native motion activation cannot confer ownership.
    const size_t partners[] = {0, 2, 1}, parents[] = {3, 0, 0};
    ALReflect::LocalPose input[3], output[3];
    input[0].rotation = LLQuaternion(.6f, LLVector3::z_axis);
    input[1].rotation = LLQuaternion(.1f, LLVector3::z_axis);
    input[2].rotation = LLQuaternion(-.2f, LLVector3::z_axis);
    for (auto& joint : input) joint.position = LLVector3(1.f, 2.f, 3.f);
    GazeWeights gaze;
    const F32 alpha[] = {gaze.weight(GAZE_HEAD), gaze.weight(GAZE_EYES), gaze.weight(GAZE_EYES)};
    GazeSample samples[3];
    size_t bad = 3;
    ensure("mirror head and eyes", reflectPose(input, output, partners, nullptr, 3, bad));
    LLQuaternion before[3], after[3];
    ensure("no unrecorded gaze", preserveGaze(input, output, partners, parents, alpha, 3,
        LLQuaternion::DEFAULT, before, after, samples));
    for (size_t i = 0; i < 3; ++i)
    {
        ensure("all locals mirrored", sameRotationBits(output[i].rotation, ALReflect::rotation(input[partners[i]].rotation)));
        ensure("no unrecorded position ownership", samePositionBits(output[i].position, ALReflect::vector(input[partners[i]].position)));
        const LLQuaternion expected_world = i == 0 ? ALReflect::rotation(input[0].rotation) :
            ALReflect::rotation(input[partners[i]].rotation) * ALReflect::rotation(input[0].rotation);
        ensure_distance("eyes follow mirrored head",
            (LLVector3::x_axis * after[i] - LLVector3::x_axis * expected_world).length(), 0.f, 1e-5f);
    }
}

template<> template<> void ghost_anim_mirror_object::test<19>()
{
    using namespace ALGhostAnimMirrorMath;
    const size_t partners[] = {0, 1, 2}, parents[] = {3, 0, 1};
    const LLQuaternion root(.2f, LLVector3::x_axis);
    const LLQuaternion animation(.35f, LLVector3::z_axis);
    const LLQuaternion parent_target(.6f, LLVector3::z_axis);
    const LLQuaternion child_target(-.25f, LLVector3::y_axis);
    for (F32 parent_alpha : {.4f, 1.f})
    {
        for (F32 child_alpha : {.2f, .75f, 1.f})
        {
            ALReflect::LocalPose input[3], output[3];
            input[0].rotation = LLQuaternion(.5f, LLVector3::z_axis);
            const LLQuaternion parent_pre_world = input[0].rotation * root;
            input[1].rotation = nlerp(parent_alpha, animation, parent_target);
            const LLQuaternion child_pre_parent = input[1].rotation * parent_pre_world;
            input[2].rotation = nlerp(child_alpha, animation, child_target);
            input[2].position = LLVector3(.1f, .2f, .3f);
            GazeSample samples[3];
            samples[1] = {true, parent_alpha, animation, parent_target * parent_pre_world};
            samples[2] = {true, child_alpha, animation, child_target * child_pre_parent};
            const F32 alpha[] = {0.f, parent_alpha, child_alpha};
            size_t bad = 3;
            ensure("mirror", reflectPose(input, output, partners, nullptr, 3, bad));
            LLQuaternion before[3], after[3];
            ensure("observed child aims under every parent alpha", preserveGaze(input, output, partners, parents, alpha, 3,
                root, before, after, samples));
            const LLQuaternion parent_post = output[1].rotation * output[0].rotation * root;
            const LLQuaternion actual = output[2].rotation * parent_post;
            // Independent world-space interpolation: animation under the final
            // mirrored parent -> unblended solver target under its raw parent.
            const LLQuaternion expected = nlerp(child_alpha,
                ALReflect::rotation(animation) * parent_post, child_target * child_pre_parent);
            for (const LLVector3& axis : {LLVector3::x_axis, LLVector3::y_axis, LLVector3::z_axis})
                ensure_distance("world-space endpoint blend under partial or full parent",
                    (axis * actual - axis * expected).length(), 0.f, 1e-5f);
            ensure("owned rotation retains mirrored position", samePositionBits(output[2].position, ALReflect::vector(input[2].position)));
        }
    }
}

template<> template<> void ghost_anim_mirror_object::test<20>()
{
    using namespace ALGhostAnimMirrorMath;
    const size_t partners[] = {0, 2, 1}, parents[] = {3, 0, 0};
    const F32 alpha[] = {0.f, .5f, .5f};
    const LLQuaternion root(.3f, LLVector3::x_axis);
    const LLQuaternion pre[] = {LLQuaternion(.1f, LLVector3::z_axis), LLQuaternion(-.2f, LLVector3::z_axis)};
    const LLQuaternion target[] = {LLQuaternion(.9f, LLVector3::z_axis), LLQuaternion(1.f, LLVector3::y_axis)};
    ALReflect::LocalPose input[3], output[3];
    input[0].rotation = LLQuaternion(.5f, LLVector3::z_axis);
    const LLQuaternion raw_parent = input[0].rotation * root;
    GazeSample samples[3];
    for (size_t eye = 0; eye < 2; ++eye)
    {
        input[eye + 1].rotation = nlerp(.5f, pre[eye], target[eye] * ~raw_parent);
        samples[eye + 1] = {true, .5f, pre[eye], target[eye]};
    }
    size_t bad = 3;
    ensure("mirror", reflectPose(input, output, partners, nullptr, 3, bad));
    LLQuaternion before[3], after[3];
    ensure("distinct eyes", preserveGaze(input, output, partners, parents, alpha, 3, root, before, after, samples));
    const LLQuaternion post_parent = output[0].rotation * root;
    for (size_t eye = 0; eye < 2; ++eye)
    {
        const LLQuaternion expected = nlerp(.5f, ALReflect::rotation(pre[1 - eye]) * post_parent, target[eye]);
        const LLQuaternion actual = output[eye + 1].rotation * post_parent;
        for (const LLVector3& axis : {LLVector3::x_axis, LLVector3::y_axis, LLVector3::z_axis})
            ensure_distance("mirror partner animation toward own eye target",
                (axis * actual - axis * expected).length(), 0.f, 1e-5f);
    }
}


template<> template<> void ghost_anim_mirror_object::test<21>()
{
    using namespace ALGhostAnimMirrorMath;
    // slots/bits distinct for every gaze joint name
    const char* names[] = {"mPelvis","mTorso","mChest","mNeck","mHead","mEyeLeft","mEyeRight","mFaceEyeAltLeft","mFaceEyeAltRight"};
    std::set<size_t> slots; for (const char* n : names) slots.insert(gazeJointSlot(n));
    ensure_equals("nine distinct slots", slots.size(), (size_t)9);
    std::set<U32> bits; for (int i = 0; i < 5; ++i) bits.insert(gazeJointBit(names[i]));
    ensure_equals("five distinct body bits", bits.size(), (size_t)5);
    // non-finite target never recorded
    GazeWeights nan_gaze;
    nan_gaze.observe("mHead", .5f, LLQuaternion::DEFAULT, LLQuaternion(std::numeric_limits<F32>::quiet_NaN(), 0.f, 0.f, 1.f));
    ensure("nan target rejected", !nan_gaze.samples[gazeJointSlot("mHead")].written);
    ensure_distance("nan target no weight", nan_gaze.weight(GAZE_HEAD), 0.f, 1e-9f);
    // merge keeps the higher-alpha sample in either order
    GazeWeights lo, hi; const LLQuaternion tlo(.2f, LLVector3::z_axis), thi(.7f, LLVector3::z_axis);
    lo.observe("mHead", .3f, LLQuaternion::DEFAULT, tlo); hi.observe("mHead", .8f, LLQuaternion::DEFAULT, thi);
    GazeWeights m1 = lo; m1.merge(hi); GazeWeights m2 = hi; m2.merge(lo);
    ensure("merge hi over lo", sameRotationBits(m1.samples[gazeJointSlot("mHead")].targetWorld, thi));
    ensure("merge keeps hi", sameRotationBits(m2.samples[gazeJointSlot("mHead")].targetWorld, thi));
    // asymmetric eye pair: only joint 1 observed, group alpha differs from sample alpha
    const size_t partners[] = {0, 2, 1}, parents[] = {3, 0, 0};
    const LLQuaternion root(.3f, LLVector3::x_axis);
    ALReflect::LocalPose input[3], output[3];
    input[0].rotation = LLQuaternion(.5f, LLVector3::z_axis);
    input[1].rotation = LLQuaternion(.1f, LLVector3::z_axis);
    input[2].rotation = LLQuaternion(-.3f, LLVector3::z_axis);
    GazeWeights eyes; const LLQuaternion target(.9f, LLVector3::z_axis);
    eyes.observe("mEyeLeft", .3f, input[1].rotation, target);
    GazeSample samples[3]; samples[1] = eyes.samples[gazeJointSlot("mEyeLeft")];
    const F32 alpha[] = {0.f, .8f, .8f};
    size_t bad = 3;
    ensure("mirror", reflectPose(input, output, partners, nullptr, 3, bad));
    LLQuaternion before[3], after[3];
    ensure("asym eyes", preserveGaze(input, output, partners, parents, alpha, 3, root, before, after, samples));
    const LLQuaternion post_parent = output[0].rotation * root;
    const LLQuaternion expected = nlerp(.8f, ALReflect::rotation(input[2].rotation) * post_parent, target);
    const LLQuaternion actual = output[1].rotation * post_parent;
    for (const LLVector3& axis : {LLVector3::x_axis, LLVector3::y_axis, LLVector3::z_axis})
        ensure_distance("observed eye with unobserved partner uses group alpha", (axis * actual - axis * expected).length(), 0.f, 1e-5f);
    // non-finite ancestor must not poison an owned child
    ALReflect::LocalPose in2[2], out2[2];
    in2[0].rotation = LLQuaternion(std::numeric_limits<F32>::quiet_NaN(), 0.f, 0.f, 1.f);
    in2[1].rotation = LLQuaternion(.2f, LLVector3::y_axis);
    const size_t p2[] = {0, 1}, par2[] = {2, 0};
    GazeSample s2[2]; s2[1] = {true, 1.f, in2[1].rotation, LLQuaternion(.4f, LLVector3::y_axis)};
    const F32 a2[] = {0.f, 1.f};
    size_t bad2 = 2; LLQuaternion b2[2], af2[2];
    ensure("mirror2", reflectPose(in2, out2, p2, nullptr, 2, bad2));
    ensure("nan ancestor", preserveGaze(in2, out2, p2, par2, a2, 2, LLQuaternion::DEFAULT, b2, af2, s2));
    ensure("owned child finite under non-finite ancestor", rotationFinite(out2[1].rotation));
}


template<> template<> void ghost_anim_mirror_object::test<22>()
{
    using namespace ALGhostAnimMirrorMath;
    // Head -> two eyes. Only A was painted; B inherits the group's alpha,
    // but must follow the mirrored head rather than pinning its old world.
    const size_t partners[] = {0, 2, 1}, parents[] = {3, 0, 0};
    const LLQuaternion root(.3f, LLVector3::x_axis);
    const LLQuaternion pre_a(.1f, LLVector3::z_axis), target(.9f, LLVector3::z_axis);
    for (F32 blend : {.2f, 1.f})
    {
        ALReflect::LocalPose input[3], output[3];
        input[0].rotation = LLQuaternion(.5f, LLVector3::z_axis);
        const LLQuaternion raw_parent = input[0].rotation * root;
        input[1].rotation = nlerp(blend, pre_a, target * ~raw_parent);
        input[2].rotation = LLQuaternion(-.3f, LLVector3::z_axis);
        input[1].position = LLVector3(.1f, .2f, .3f);
        input[2].position = LLVector3(.4f, -.5f, .6f);
        GazeSample samples[3];
        samples[1] = {true, blend, pre_a, target};
        const F32 alpha[] = {0.f, blend, blend};
        size_t bad = 3;
        ensure("mirror asymmetric eyes", reflectPose(input, output, partners, nullptr, 3, bad));
        LLQuaternion before[3], after[3];
        ensure("observed only", preserveGaze(input, output, partners, parents, alpha, 3,
            root, before, after, samples));
        const LLQuaternion post_parent = output[0].rotation * root;
        const LLQuaternion expected_b = ALReflect::rotation(pre_a) * post_parent;
        const LLQuaternion actual_b = output[2].rotation * post_parent;
        for (const LLVector3& axis : {LLVector3::x_axis, LLVector3::y_axis, LLVector3::z_axis})
            ensure_distance("unobserved eye follows reflected partner animation and mirrored parent",
                (axis * actual_b - axis * expected_b).length(), 0.f, 1e-5f);
        ensure("unobserved eye does not pin its old world",
            (LLVector3::x_axis * actual_b - LLVector3::x_axis * before[2]).length() > .1f);
        ensure("unobserved eye position still mirrors partner",
            samePositionBits(output[2].position, ALReflect::vector(input[1].position)));
        ensure("observed eye position still mirrors partner",
            samePositionBits(output[1].position, ALReflect::vector(input[2].position)));
        // With no observed partner either, an inherited alpha is plain mirror.
        samples[1] = {};
        ensure("mirror with no observations", reflectPose(input, output, partners, nullptr, 3, bad));
        const ALReflect::LocalPose plain_b = output[2];
        ensure("unobserved group", preserveGaze(input, output, partners, parents, alpha, 3,
            root, before, after, samples));
        ensure("unobserved group keeps plain mirrored rotation", sameRotationBits(output[2].rotation, plain_b.rotation));
    }
}

}
