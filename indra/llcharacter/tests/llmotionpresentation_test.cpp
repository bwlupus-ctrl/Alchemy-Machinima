/**
 * @file llmotionpresentation_test.cpp
 * @brief Owner-scoped per-motion presentation control regression tests.
 */

#include "linden_common.h"

#include "../llmotion.h"
#include "../test/lltut.h"

#include <limits>

namespace
{
class TestMotion final : public LLMotion
{
public:
    using LLMotion::deactivate;
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
}
