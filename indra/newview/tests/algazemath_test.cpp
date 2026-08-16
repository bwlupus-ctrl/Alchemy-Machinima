/**
 * @file algazemath_test.cpp
 * @brief Unit tests for pure Lens Gaze math.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "../test/lltut.h"
#include "../algazemath.h"

#include <cmath>

namespace tut
{
namespace
{
F32 angleBetween(LLVector3 first, LLVector3 second)
{
    first.normVec();
    second.normVec();
    return acosf(llclamp(first * second, -1.f, 1.f));
}
} // anonymous namespace

struct algazemath_data
{
};

typedef test_group<algazemath_data> algazemath_test_group;
typedef algazemath_test_group::object algazemath_test_object;
algazemath_test_group algazemath_test("algazemath");

template<> template<>
void algazemath_test_object::test<1>()
{
    set_test_name("chase alpha");
    ensure("zero dt has zero alpha",
           std::fabs(ALGazeMath::chaseAlpha(0.f, 0.5f)) <= 1e-7f);
    ensure("alpha grows with dt",
           ALGazeMath::chaseAlpha(0.2f, 0.5f) > ALGazeMath::chaseAlpha(0.1f, 0.5f));
    constexpr F32 LN_TWO = 0.6931471805599453f;
    ensure("one half-life gives one half alpha",
           std::fabs(ALGazeMath::chaseAlpha(0.5f * LN_TWO, 0.5f) - 0.5f) <= 1e-5f);
}

template<> template<>
void algazemath_test_object::test<2>()
{
    set_test_name("dead-zone chase");
    F32 pitch = 0.2f;
    F32 yaw = -0.3f;
    ALGazeMath::deadZoneChase(pitch, yaw, 0.21f, -0.29f, 0.05f, 1.f);
    ensure("an aim inside the zone does not move", std::fabs(pitch - 0.2f) <= 1e-7f);
    ensure("inside-zone yaw does not move", std::fabs(yaw + 0.3f) <= 1e-7f);

    pitch = 0.f;
    yaw = 0.f;
    F32 previous_error = 1.f;
    for (S32 i = 0; i < 80; ++i)
    {
        ALGazeMath::deadZoneChase(pitch, yaw, 0.f, 1.f, 0.1f, 0.2f);
        const F32 error = std::fabs(1.f - yaw);
        ensure("outside-zone convergence is monotone", error <= previous_error + 1e-6f);
        previous_error = error;
    }
    ensure("steady-state error is the dead zone", std::fabs(previous_error - 0.1f) <= 1e-5f);

    pitch = 0.f;
    yaw = 179.f * DEG_TO_RAD;
    ALGazeMath::deadZoneChase(pitch, yaw, 0.f, -179.f * DEG_TO_RAD, 0.f, 1.f);
    ensure("yaw chase takes the wrapped short arc",
           std::fabs(llsimple_angle(yaw - (-179.f * DEG_TO_RAD))) <= 1e-5f);
}

template<> template<>
void algazemath_test_object::test<3>()
{
    set_test_name("torso-only chase");
    F32 pitch = -0.4f;
    F32 yaw = 0.7f;
    ALGazeMath::torsoChase(pitch, yaw, 0.3f, -0.5f, 1.f, 0.01f, 0.27f);
    ensure("ratio one snaps pitch to the legacy target", std::fabs(pitch - 0.3f) <= 1e-7f);
    ensure("ratio one snaps yaw to the legacy target", std::fabs(yaw + 0.5f) <= 1e-7f);

    F32 head_pitch = 0.f;
    F32 head_yaw = 0.f;
    pitch = 0.f;
    yaw = 0.f;
    constexpr F32 TARGET_PITCH = 0.35f;
    constexpr F32 TARGET_YAW = 0.8f;
    constexpr F32 TAU_USER = 0.27f;
    constexpr F32 DT = 1.f / 60.f;
    for (S32 i = 0; i < 600; ++i)
    {
        ALGazeMath::deadZoneChase(
            head_pitch, head_yaw, TARGET_PITCH, TARGET_YAW, 0.f,
            ALGazeMath::chaseAlpha(DT, TAU_USER));
        ALGazeMath::torsoChase(
            pitch, yaw, head_pitch, head_yaw, 1.8f, DT, TAU_USER);
        const F32 head_error = sqrtf(
            (TARGET_PITCH - head_pitch) * (TARGET_PITCH - head_pitch) +
            llsimple_angle(TARGET_YAW - head_yaw) *
                llsimple_angle(TARGET_YAW - head_yaw));
        const F32 torso_error = sqrtf(
            (TARGET_PITCH - pitch) * (TARGET_PITCH - pitch) +
            llsimple_angle(TARGET_YAW - yaw) *
                llsimple_angle(TARGET_YAW - yaw));
        if (head_error > 1e-6f)
        {
            ensure("the torso trails the shared head path", torso_error > head_error);
        }
    }
    ensure("torso pitch converges without a steady twist",
           std::fabs(TARGET_PITCH - pitch) <= 1e-5f);
    ensure("torso yaw converges without a steady twist",
           std::fabs(llsimple_angle(TARGET_YAW - yaw)) <= 1e-5f);
}

template<> template<>
void algazemath_test_object::test<4>()
{
    set_test_name("behind envelope");
    const F32 down = ALGazeMath::behindEnvStep(0.6f, true, 0.1f, 0.5f);
    const F32 up = ALGazeMath::behindEnvStep(0.4f, false, 0.1f, 0.5f);
    ensure("behind decreases the envelope", down < 0.6f);
    ensure("front increases the envelope", up > 0.4f);
    ensure("rise and fall rates are symmetric", std::fabs(down - 0.4f) <= 1e-6f &&
                                             std::fabs(up - 0.6f) <= 1e-6f);
    ensure("fall clamps at zero", ALGazeMath::behindEnvStep(0.1f, true, 1.f, 0.5f) >= 0.f);
    ensure("rise clamps at one", ALGazeMath::behindEnvStep(0.9f, false, 1.f, 0.5f) <= 1.f);
    ensure("zero ease releases immediately",
           std::fabs(ALGazeMath::behindEnvStep(1.f, true, 0.f, 0.f)) <= 1e-7f);
    ensure("zero ease engages immediately",
           std::fabs(ALGazeMath::behindEnvStep(0.f, false, 0.f, 0.f) - 1.f) <= 1e-7f);
}

template<> template<>
void algazemath_test_object::test<5>()
{
    set_test_name("eyeline direction offset");
    const LLVector3 dir(1.f, 0.25f, 0.1f);
    const LLVector3 up(0.f, 0.f, 1.f);
    const LLVector3 zero = ALGazeMath::eyelineOffsetDir(dir, up, 0.f, 0.f);
    ensure("zero eyeline offset is bit-exact", zero == dir);

    const F32 yaw = 8.f * DEG_TO_RAD;
    const LLVector3 forward(1.f, 0.f, 0.f);
    const LLVector3 turned = ALGazeMath::eyelineOffsetDir(forward, up, yaw, 0.f);
    ensure("pure eyeline yaw rotates by the requested angle",
           std::fabs(angleBetween(forward, turned) - yaw) <= 1e-5f);

    const LLVector3 vertical(0.f, 0.f, 1.f);
    const LLVector3 degenerate = ALGazeMath::eyelineOffsetDir(vertical, up, yaw, 0.f);
    ensure("degenerate up cross direction is unchanged", degenerate == vertical);
}
} // namespace tut
