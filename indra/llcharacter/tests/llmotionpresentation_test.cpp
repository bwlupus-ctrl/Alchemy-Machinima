/**
 * @file llmotionpresentation_test.cpp
 * @brief Owner-scoped per-motion presentation control regression tests.
 */

#include "linden_common.h"

#include "../lljointstate.h"
#include "../llmotion.h"
#include "../llmotioncontroller.h"
#include "../test/lltut.h"

#include <limits>

namespace
{
class TestMotion final : public LLMotion
{
public:
    using LLMotion::deactivate;
    using LLMotion::addJointState;
    U8 signature(S32 channel, S32 joint_num) const
    {
        return mJointSignature[channel][joint_num];
    }
    explicit TestMotion(const LLUUID& id, bool sample_capable)
    : LLMotion(id), mSampleCapable(sample_capable)
    {
    }

    bool getLoop() override { return true; }
    F32 getDuration() override { return 2.f; }
    F32 getEaseInDuration() override { return 0.f; }
    F32 getEaseOutDuration() override { return 0.f; }
    LLJoint::JointPriority getPriority() override { return LLJoint::LOW_PRIORITY; }
    LLMotionBlendType getBlendType() override { return NORMAL_BLEND; }
    F32 getMinPixelArea() override { return 0.f; }
    LLMotionInitStatus onInitialize(LLCharacter*) override { return STATUS_SUCCESS; }
    bool onUpdate(F32, U8*) override { return true; }
    void onDeactivate() override {}
    bool onActivate() override { return true; }
    bool supportsExternalSampling() const override { return mSampleCapable; }

private:
    bool mSampleCapable;
};

}

namespace tut
{
struct motion_presentation_data {};
typedef test_group<motion_presentation_data> motion_presentation_group;
typedef motion_presentation_group::object motion_presentation_object;
motion_presentation_group motion_presentation_tests("LLMotionPresentation");

template<> template<>
void motion_presentation_object::test<1>()
{
    TestMotion motion(LLUUID::generateNewID(), true);
    const LLUUID owner = LLUUID::generateNewID();
    const LLUUID other = LLUUID::generateNewID();

    ensure("null owner rejected", !motion.claimPresentationControl(LLUUID::null));
    ensure("owner claim accepted", motion.claimPresentationControl(owner));
    ensure("second owner rejected", !motion.claimPresentationControl(other));
    ensure("owner identity retained", motion.isPresentationControlledBy(owner));
}

template<> template<>
void motion_presentation_object::test<2>()
{
    TestMotion motion(LLUUID::generateNewID(), true);
    const LLUUID owner = LLUUID::generateNewID();
    ensure("claim", motion.claimPresentationControl(owner));
    ensure("external sample accepted", motion.setExternalSampleTime(owner, 1.25f));
    ensure("external clock active", motion.usesExternalSampleClock());
    ensure_distance("external time selected", motion.getEffectiveUpdateTime(7.f), 1.25f, 0.0001f);
    ensure("native clock restored", motion.useNativePresentationClock(owner));
    ensure_distance("native time selected", motion.getEffectiveUpdateTime(7.f), 7.f, 0.0001f);
}

template<> template<>
void motion_presentation_object::test<3>()
{
    TestMotion motion(LLUUID::generateNewID(), false);
    const LLUUID owner = LLUUID::generateNewID();
    ensure("claim", motion.claimPresentationControl(owner));
    ensure("unsupported external sample rejected", !motion.setExternalSampleTime(owner, 1.f));
    ensure("unsupported motion stays native", !motion.usesExternalSampleClock());
}

template<> template<>
void motion_presentation_object::test<4>()
{
    TestMotion motion(LLUUID::generateNewID(), true);
    const LLUUID owner = LLUUID::generateNewID();
    ensure("claim", motion.claimPresentationControl(owner));
    ensure("weight accepted", motion.setPresentationWeight(owner, 0.25f));
    ensure_distance("weight applied", motion.applyPresentationWeight(0.8f), 0.2f, 0.0001f);
    ensure_distance("base weight recovered", motion.getPresentationBaseWeight(0.2f),
                    0.8f, 0.0001f);
    motion.releasePresentationControl(owner);
    ensure("owner released", !motion.isPresentationControlledBy(owner));
    ensure_distance("identity restored", motion.applyPresentationWeight(0.8f), 0.8f, 0.0001f);
    ensure("stale owner cannot write", !motion.setPresentationWeight(owner, 0.f));
}

template<> template<>
void motion_presentation_object::test<5>()
{
    TestMotion motion(LLUUID::generateNewID(), true);
    const LLUUID owner = LLUUID::generateNewID();
    ensure("claim", motion.claimPresentationControl(owner));
    ensure("low clamp", motion.setPresentationWeight(owner, -4.f));
    ensure_distance("low clamped", motion.getPresentationWeight(), 0.f, 0.0001f);
    ensure("high clamp", motion.setPresentationWeight(owner, 4.f));
    ensure_distance("high clamped", motion.getPresentationWeight(), 1.f, 0.0001f);
}

template<> template<>
void motion_presentation_object::test<6>()
{
    TestMotion motion(LLUUID::generateNewID(), true);
    const LLUUID owner = LLUUID::generateNewID();
    ensure("claim", motion.claimPresentationControl(owner));
    ensure("NaN sample rejected", !motion.setExternalSampleTime(
        owner, std::numeric_limits<F32>::quiet_NaN()));
    ensure("infinite sample rejected", !motion.setExternalSampleTime(
        owner, std::numeric_limits<F32>::infinity()));
    ensure("NaN weight rejected", !motion.setPresentationWeight(
        owner, std::numeric_limits<F32>::quiet_NaN()));
    ensure("clock remains native after rejected samples",
           !motion.usesExternalSampleClock());
    ensure_distance("weight remains identity after rejected weight",
                    motion.getPresentationWeight(), 1.f, 0.0001f);
}

template<> template<>
void motion_presentation_object::test<7>()
{
    TestMotion motion(LLUUID::generateNewID(), true);
    const LLUUID owner = LLUUID::generateNewID();
    ensure("claim", motion.claimPresentationControl(owner));
    ensure("sample", motion.setExternalSampleTime(owner, 123456.f));
    ensure("weight", motion.setPresentationWeight(owner, 0.4f));
    motion.deactivate();
    ensure("deactivate clears owner", !motion.hasPresentationControl());
    ensure("deactivate restores native clock", !motion.usesExternalSampleClock());
    ensure_distance("deactivate restores identity weight",
                    motion.getPresentationWeight(), 1.f, 0.0001f);
    ensure_equals("native passthrough remains exact at large time",
                  motion.getEffectiveUpdateTime(123456.f),
                  123456.f);
}

template<> template<>
void motion_presentation_object::test<8>()
{
    TestMotion motion(LLUUID::generateNewID(), true);
    const LLUUID owner = LLUUID::generateNewID();
    const LLUUID other = LLUUID::generateNewID();

    ensure("cannot set without a claim",
           !motion.setOwnerManagedLifetime(owner, true));
    ensure("claim", motion.claimPresentationControl(owner));
    ensure("owner can set lifetime",
           motion.setOwnerManagedLifetime(owner, true));
    ensure("native stop suppression is visible to the controller",
           !LLMotionController::usesNativeLifetime(&motion));
    ensure("token mismatch cannot clear lifetime",
           !motion.setOwnerManagedLifetime(other, false));
    ensure("mismatched clear left lifetime set",
           motion.hasOwnerManagedLifetime());
    ensure("owner can clear lifetime",
           motion.setOwnerManagedLifetime(owner, false));
    ensure("lifetime clear took effect", !motion.hasOwnerManagedLifetime());
    ensure("native lifetime restored after clear",
           LLMotionController::usesNativeLifetime(&motion));
}

template<> template<>
void motion_presentation_object::test<9>()
{
    TestMotion motion(LLUUID::generateNewID(), true);
    const LLUUID owner = LLUUID::generateNewID();
    ensure("claim", motion.claimPresentationControl(owner));
    ensure("set lifetime", motion.setOwnerManagedLifetime(owner, true));
    motion.deactivate();
    ensure("deactivate resets lifetime", !motion.hasOwnerManagedLifetime());
}

template<> template<>
void motion_presentation_object::test<10>()
{
    TestMotion motion(LLUUID::generateNewID(), true);
    const LLUUID owner = LLUUID::generateNewID();
    const LLUUID other = LLUUID::generateNewID();
    LLJoint face("mFaceJaw");
    LLJoint eye("mEyeLeft");
    LLJoint skull("mSkull");
    LLJoint head("mHead");
    LLJoint neck("mNeck");

    ensure("face helper includes Bento face", LLMotion::isFaceJoint(&face));
    ensure("face helper includes legacy eye", LLMotion::isFaceJoint(&eye));
    ensure("face helper includes skull", LLMotion::isFaceJoint(&skull));
    ensure("face helper leaves head", !LLMotion::isFaceJoint(&head));
    ensure("face helper leaves neck", !LLMotion::isFaceJoint(&neck));
    ensure("cannot set without claim", !motion.setPresentationJointExclusion(
        owner, &LLMotion::isFaceJoint));
    ensure("claim", motion.claimPresentationControl(owner));
    ensure("owner sets exclusion", motion.setPresentationJointExclusion(
        owner, &LLMotion::isFaceJoint));
    ensure("face is excluded", motion.isPresentationJointExcluded(&face));
    ensure("head is not excluded", !motion.isPresentationJointExcluded(&head));
    ensure("token mismatch cannot clear", !motion.setPresentationJointExclusion(
        other, nullptr));
    ensure("mismatched clear left exclusion", motion.isPresentationJointExcluded(&face));
    ensure("owner clears exclusion", motion.setPresentationJointExclusion(owner, nullptr));
    ensure("clear restored face", !motion.isPresentationJointExcluded(&face));
    ensure("owner restores exclusion", motion.setPresentationJointExclusion(
        owner, &LLMotion::isFaceJoint));
    motion.releasePresentationControl(owner);
    ensure("release clears exclusion", !motion.isPresentationJointExcluded(&face));
    ensure("reclaim", motion.claimPresentationControl(owner));
    ensure("restore exclusion after reclaim", motion.setPresentationJointExclusion(
        owner, &LLMotion::isFaceJoint));
    motion.deactivate();
    ensure("deactivate clears exclusion", !motion.isPresentationJointExcluded(&face));
}

template<> template<>
void motion_presentation_object::test<12>()
{
    TestMotion motion(LLUUID::generateNewID(), true);
    const LLUUID owner = LLUUID::generateNewID();
    const LLUUID other = LLUUID::generateNewID();
    ensure("claim", motion.claimPresentationControl(owner));
    ensure("pre-wrap sample", motion.setExternalSampleTime(owner, 0.98f, 1000.98));
    const U32 revision = motion.getPresentationClockRevision();
    ensure("wrapped sample with advancing clock", motion.setExternalSampleTime(owner, 0.02f, 1001.02));
    ensure_equals("ordinary wrap preserves continuity", motion.getPresentationClockRevision(), revision);
    ensure_distance("unwrapped clock retains double precision", motion.getExternalSampleClockTime(), 1001.02, 1e-10);
    ensure("backward seek", motion.setExternalSampleTime(owner, 0.9f, 1000.9));
    ensure("backward seek invalidates seam", motion.getPresentationClockRevision() != revision);
    const U32 after_backward = motion.getPresentationClockRevision();
    ensure("explicit forward seek", motion.setExternalSampleTime(owner, 0.1f, 1001.1, true));
    ensure("forward seek invalidates seam", motion.getPresentationClockRevision() != after_backward);
    const U32 after_seek = motion.getPresentationClockRevision();
    ensure("invalid clock rejected", !motion.setExternalSampleTime(owner, 0.3f,
        std::numeric_limits<F64>::quiet_NaN()));
    ensure("non-owner cannot seek", !motion.setExternalSampleTime(other, 0.3f, 1001.3, true));
    ensure_equals("rejected writes preserve revision", motion.getPresentationClockRevision(), after_seek);
    ensure("native clock switch", motion.useNativePresentationClock(owner));
    ensure("clock switch invalidates seam", motion.getPresentationClockRevision() != after_seek);
}

namespace
{
// True when any of the three channels carries a non-zero signature entry.
bool anySignature(const TestMotion& motion, S32 joint_num)
{
    return motion.signature(0, joint_num) != 0 ||
        motion.signature(1, joint_num) != 0 ||
        motion.signature(2, joint_num) != 0;
}
}

template<> template<>
void motion_presentation_object::test<11>()
{
    // Joints must outlive the motion (and its pose) that references them.
    LLJoint face("mFaceJaw");
    LLJoint head("mHead");
    LLJoint jaw2("mFaceTeethLower");
    const S32 face_num = 3;
    const S32 head_num = 4;
    face.setJointNum(face_num);
    head.setJointNum(head_num);
    jaw2.setJointNum(5);

    TestMotion motion(LLUUID::generateNewID(), true);
    const LLUUID owner = LLUUID::generateNewID();
    const LLUUID other = LLUUID::generateNewID();

    LLPointer<LLJointState> face_state = new LLJointState(&face);
    face_state->setUsage(LLJointState::POS | LLJointState::ROT | LLJointState::SCALE);
    LLPointer<LLJointState> head_state = new LLJointState(&head);
    head_state->setUsage(LLJointState::ROT);
    motion.addJointState(face_state);
    motion.addJointState(head_state);

    // No exclusion: byte-identical to stock behaviour.
    ensure("face signature present without exclusion", anySignature(motion, face_num));
    ensure("head signature present without exclusion", anySignature(motion, head_num));
    ensure("face blender flag clear without exclusion",
           !face_state->isPresentationExcluded());
    ensure("head blender flag clear without exclusion",
           !head_state->isPresentationExcluded());

    ensure("claim", motion.claimPresentationControl(owner));
    ensure("owner sets exclusion", motion.setPresentationJointExclusion(
        owner, &LLMotion::isFaceJoint));
    for (S32 channel = 0; channel < 3; ++channel)
    {
        ensure("face signature zeroed in every channel", motion.signature(channel, face_num) == 0);
    }
    ensure("head signature retained", anySignature(motion, head_num));
    ensure("head ROT signature retained", motion.signature(1, head_num) != 0);
    ensure("blender skip flag set only for face", face_state->isPresentationExcluded());
    ensure("blender skip flag clear for head", !head_state->isPresentationExcluded());

    ensure("mismatched owner cannot clear", !motion.setPresentationJointExclusion(other, nullptr));
    ensure("mismatched clear left face flag", face_state->isPresentationExcluded());

    ensure("owner clears exclusion", motion.setPresentationJointExclusion(owner, nullptr));
    ensure("clear restores face signature", anySignature(motion, face_num));
    ensure("clear restores all face channels",
           motion.signature(0, face_num) != 0 && motion.signature(1, face_num) != 0 &&
           motion.signature(2, face_num) != 0);
    ensure("clear restores head signature", anySignature(motion, head_num));
    ensure("clear drops face blender flag", !face_state->isPresentationExcluded());
    ensure("clear leaves head blender flag clear", !head_state->isPresentationExcluded());

    // Release restores both too.
    ensure("owner restores exclusion", motion.setPresentationJointExclusion(
        owner, &LLMotion::isFaceJoint));
    ensure("exclusion re-applied", !anySignature(motion, face_num));
    motion.releasePresentationControl(owner);
    ensure("release restores face signature", anySignature(motion, face_num));
    ensure("release drops face blender flag", !face_state->isPresentationExcluded());
    ensure("release keeps head signature", anySignature(motion, head_num));

    // Deactivate restores both as well.
    ensure("reclaim", motion.claimPresentationControl(owner));
    ensure("exclusion after reclaim", motion.setPresentationJointExclusion(
        owner, &LLMotion::isFaceJoint));
    ensure("exclusion active after reclaim", !anySignature(motion, face_num));
    motion.deactivate();
    ensure("deactivate restores face signature", anySignature(motion, face_num));
    ensure("deactivate drops face blender flag", !face_state->isPresentationExcluded());
    ensure("deactivate keeps head signature", anySignature(motion, head_num));

    // A joint state added while the exclusion is already active is flagged at add time.
    ensure("claim for late add", motion.claimPresentationControl(owner));
    ensure("exclusion for late add", motion.setPresentationJointExclusion(
        owner, &LLMotion::isFaceJoint));
    LLPointer<LLJointState> late_state = new LLJointState(&jaw2);
    late_state->setUsage(LLJointState::ROT);
    motion.addJointState(late_state);
    ensure("late face joint flagged", late_state->isPresentationExcluded());
    ensure("late face joint signature zero", !anySignature(motion, 5));
}

}
