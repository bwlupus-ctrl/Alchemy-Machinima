/**
 * @file algazepolicy_test.cpp
 * @brief Unit tests for pure oculomotor policy math (main sequence, head
 * latency policy, VOR eye-in-head solve).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "../test/lltut.h"
#include "../algazepolicy.h"

#include <cmath>
#include <limits>

namespace tut
{
namespace
{
// Angle between two vectors, measured via atan2(|cross|, dot) rather than
// acos(dot). Root-caused against a real failure (see algazepolicy_test.cpp
// test<5>): acos(x) has a vertical tangent at x == 1, so for two vectors
// that are truly (near-)parallel, a single F32 ULP of round-off in the dot
// product -- unavoidable float32 noise from the VOR round-trip, not a solve
// defect -- gets amplified into an apparent angular error of
// sqrt(2*eps) ~= 3.5e-4 rad (~0.02 deg), comfortably blowing past a 1e-4 rad
// tolerance for an angle that is actually ~1e-7 rad. Measured directly: at
// head_yaw=-10deg in test<5>, dot rounded to 0.9999999404 (exactly one ULP
// below 1.0f), giving acos(dot) ~= 0.0198 deg while the cross-product-based
// |a x b| for the same pair was 4.7e-8 (i.e. atan2(cross,dot) ~= 2.7e-6 deg).
// atan2(|cross|, dot) has no such singularity -- its error tracks the input
// error linearly at every angle, including near zero -- so this is a fix to
// the measurement's conditioning, not a loosened tolerance: the 1e-4 rad
// bound in test<5>/test<6> is unchanged and remains meaningful.
F32 angleBetween(LLVector3 first, LLVector3 second)
{
    first.normVec();
    second.normVec();
    const F32 dot = llclamp(first * second, -1.f, 1.f);
    const F32 cross_mag = (first % second).length();
    return atan2f(cross_mag, dot);
}
} // anonymous namespace

struct algazepolicy_data
{
};

typedef test_group<algazepolicy_data> algazepolicy_test_group;
typedef algazepolicy_test_group::object algazepolicy_test_object;
algazepolicy_test_group algazepolicy_test("algazepolicy");

template<> template<>
void algazepolicy_test_object::test<1>()
{
    set_test_name("eye saccade main sequence: population-prior calibration points");
    // companion 3.5: at 2 deg ~31ms, at 20 deg ~81ms, using the documented
    // default intercept/slope (base=25, per_deg=2.8) -- not a fixed 25ms.
    ensure("2 deg gives ~31ms",
           std::fabs(ALGazePolicy::eyeSaccadeDurationMs(2.f) - 30.6f) <= 0.5f);
    ensure("20 deg gives ~81ms",
           std::fabs(ALGazePolicy::eyeSaccadeDurationMs(20.f) - 81.f) <= 0.5f);
}

template<> template<>
void algazepolicy_test_object::test<2>()
{
    set_test_name("eye saccade main sequence: monotone and clamped");
    const F32 d0 = ALGazePolicy::eyeSaccadeDurationMs(0.f);
    const F32 d2 = ALGazePolicy::eyeSaccadeDurationMs(2.f);
    const F32 d10 = ALGazePolicy::eyeSaccadeDurationMs(10.f);
    const F32 d20 = ALGazePolicy::eyeSaccadeDurationMs(20.f);
    const F32 d_huge = ALGazePolicy::eyeSaccadeDurationMs(10000.f);

    ensure("zero amplitude sits at the base intercept",
           std::fabs(d0 - 25.f) <= 1e-4f);
    ensure("duration is strictly monotone in amplitude below the ceiling",
           d0 < d2 && d2 < d10 && d10 < d20);
    ensure("a huge amplitude clamps to the 110ms ceiling",
           std::fabs(d_huge - 110.f) <= 1e-4f);
    ensure("negative amplitude does not go below the base",
           std::fabs(ALGazePolicy::eyeSaccadeDurationMs(-5.f) - 25.f) <= 1e-4f);
    ensure("non-finite amplitude is finite base",
           std::isfinite(ALGazePolicy::eyeSaccadeDurationMs(
               std::numeric_limits<F32>::quiet_NaN())));

    // Custom base/per-deg are honored (spec: parameters, not a fixed 25ms).
    const F32 custom = ALGazePolicy::eyeSaccadeDurationMs(10.f, 20.f, 1.f);
    ensure("custom base/per-deg calibration is used",
           std::fabs(custom - 30.f) <= 1e-4f);
}

template<> template<>
void algazepolicy_test_object::test<3>()
{
    set_test_name("head latency: base default and finiteness");
    const F32 neutral = ALGazePolicy::headLatencyMs(0.f, 0.f, 0.f, 0.f);
    ensure("zero-everything sits at the ~40ms base",
           std::fabs(neutral - 40.f) <= 1e-3f);
    ensure("head latency is always finite and non-negative",
           std::isfinite(neutral) && neutral >= 0.f);
}

template<> template<>
void algazepolicy_test_object::test<4>()
{
    set_test_name("head latency decreases with each documented input");
    const F32 base = ALGazePolicy::headLatencyMs(2.f, 1.f, 0.f, 0.f);

    ensure("larger amplitude reduces latency",
           ALGazePolicy::headLatencyMs(30.f, 1.f, 0.f, 0.f) < base);
    ensure("greater initial eccentricity reduces latency",
           ALGazePolicy::headLatencyMs(2.f, 15.f, 0.f, 0.f) < base);
    ensure("higher predictability reduces latency",
           ALGazePolicy::headLatencyMs(2.f, 1.f, 0.8f, 0.f) < base);
    ensure("a stronger head-mover trait reduces latency",
           ALGazePolicy::headLatencyMs(2.f, 1.f, 0.f, 0.8f) < base);

    // Every term maxed out at once still stays clamped and non-negative.
    const F32 extreme = ALGazePolicy::headLatencyMs(500.f, 500.f, 1.f, 1.f, 40.f);
    ensure("head latency stays clamped and positive under extreme input",
           std::isfinite(extreme) && extreme >= 0.f && extreme <= 40.f);

    // Non-finite inputs degrade to their documented neutral defaults instead
    // of propagating NaN/inf.
    const F32 nan_amp = ALGazePolicy::headLatencyMs(
        std::numeric_limits<F32>::quiet_NaN(), 0.f, 0.f, 0.f);
    ensure("non-finite amplitude input still returns finite latency",
           std::isfinite(nan_amp));
}

template<> template<>
void algazepolicy_test_object::test<5>()
{
    set_test_name("VOR: eye-in-head solve counter-rotates as the head sweeps");
    // Pure-yaw head sweep (pitch fixed at 0) lets us predict the exact
    // counter-rotation algebraically: with a pure-yaw head_world_rot,
    // raw_eye_yaw == target_world_yaw - head_yaw. That is the falsifiable
    // signature of "solve eye-in-head each sample from world gaze + sampled
    // head orientation" -- a fixed "head progress * constant" recentering
    // would NOT reproduce this exact linear relationship for an arbitrary
    // fixed world target.
    const LLVector3 world_target(1.f, 0.3f, 0.f);

    LLQuaternion identity_head;
    F32 target_world_yaw = 0.f;
    F32 target_world_pitch = 0.f;
    ALGazePolicy::eyeInHeadRawFromWorldGaze(
        world_target, identity_head, target_world_yaw, target_world_pitch);

    for (F32 head_yaw_deg = -40.f; head_yaw_deg <= 40.001f; head_yaw_deg += 5.f)
    {
        LLQuaternion head_rot;
        head_rot.setEulerAngles(0.f, 0.f, head_yaw_deg * DEG_TO_RAD);

        F32 raw_yaw = 0.f;
        F32 raw_pitch = 0.f;
        ALGazePolicy::eyeInHeadRawFromWorldGaze(
            world_target, head_rot, raw_yaw, raw_pitch);

        const F32 expected_yaw = llsimple_angle(
            target_world_yaw - head_yaw_deg * DEG_TO_RAD);
        ensure("eye-in-head yaw is the exact head counter-rotation",
               std::fabs(llsimple_angle(raw_yaw - expected_yaw)) <= 1e-4f);

        // Reconstructing world direction from head_world_rot . eye_in_head
        // must land back on the original world target before soft-limiting.
        const LLVector3 reconstructed = ALGazePolicy::worldDirFromEyeInHead(
            raw_yaw, raw_pitch, head_rot);
        ensure("reconstructed world direction stays on the fixed target",
               angleBetween(reconstructed, world_target) <= 1e-4f);
    }
}

template<> template<>
void algazepolicy_test_object::test<6>()
{
    set_test_name("VOR: reconstruction holds across combined yaw+pitch head sweep");
    const LLVector3 world_target(1.f, 0.4f, 0.2f);

    for (F32 head_yaw_deg = -35.f; head_yaw_deg <= 35.001f; head_yaw_deg += 10.f)
    {
        for (F32 head_pitch_deg = -25.f; head_pitch_deg <= 25.001f; head_pitch_deg += 12.5f)
        {
            LLQuaternion head_rot;
            head_rot.setEulerAngles(
                0.f, head_pitch_deg * DEG_TO_RAD, head_yaw_deg * DEG_TO_RAD);

            F32 raw_yaw = 0.f;
            F32 raw_pitch = 0.f;
            ALGazePolicy::eyeInHeadRawFromWorldGaze(
                world_target, head_rot, raw_yaw, raw_pitch);
            const LLVector3 reconstructed = ALGazePolicy::worldDirFromEyeInHead(
                raw_yaw, raw_pitch, head_rot);
            ensure("world-gaze fixation is exact under any sampled head pose",
                   angleBetween(reconstructed, world_target) <= 1e-4f);
        }
    }
}

template<> template<>
void algazepolicy_test_object::test<7>()
{
    set_test_name("VOR: soft-limit engages beyond the comfort cone");
    constexpr F32 COMFORT_YAW_DEG = 30.f;
    constexpr F32 COMFORT_PITCH_DEG = 20.f;

    // Head facing forward, target well outside the comfort cone.
    const LLVector3 far_target(1.f, 2.5f, 0.6f);
    LLQuaternion identity_head;

    F32 raw_yaw = 0.f;
    F32 raw_pitch = 0.f;
    ALGazePolicy::eyeInHeadRawFromWorldGaze(
        far_target, identity_head, raw_yaw, raw_pitch);
    ensure("test setup actually exceeds the comfort cone",
           fabsf(raw_yaw) > COMFORT_YAW_DEG * DEG_TO_RAD);

    F32 limited_yaw = 0.f;
    F32 limited_pitch = 0.f;
    ALGazePolicy::eyeInHeadFromWorldGaze(
        far_target, identity_head, COMFORT_YAW_DEG, COMFORT_PITCH_DEG,
        limited_yaw, limited_pitch);
    ensure("soft-limited yaw stays strictly inside the comfort cone",
           fabsf(limited_yaw) < COMFORT_YAW_DEG * DEG_TO_RAD);
    ensure("soft limit actually reduced the raw yaw",
           fabsf(limited_yaw) < fabsf(raw_yaw));

    // A small, in-cone angle is nearly unaffected by the soft limit.
    const F32 small_raw = 3.f * DEG_TO_RAD;
    const F32 small_limited =
        ALGazePolicy::softClampAngle(small_raw, COMFORT_YAW_DEG * DEG_TO_RAD);
    ensure("small in-cone angles pass through close to unchanged",
           std::fabs(small_limited - small_raw) <= 0.15f * DEG_TO_RAD);

    ensure("soft clamp at zero is exactly zero",
           ALGazePolicy::softClampAngle(0.f, COMFORT_YAW_DEG * DEG_TO_RAD) == 0.f);
    ensure("soft clamp with a non-positive limit collapses to zero",
           ALGazePolicy::softClampAngle(10.f * DEG_TO_RAD, 0.f) == 0.f);
}

template<> template<>
void algazepolicy_test_object::test<8>()
{
    set_test_name("VOR: degenerate inputs stay finite");
    LLQuaternion identity_head;
    F32 yaw = 123.f;
    F32 pitch = 123.f;

    ALGazePolicy::eyeInHeadRawFromWorldGaze(
        LLVector3::zero, identity_head, yaw, pitch);
    ensure("zero world gaze vector returns finite, zeroed eye-in-head",
           std::isfinite(yaw) && std::isfinite(pitch) &&
           yaw == 0.f && pitch == 0.f);

    yaw = 123.f;
    pitch = 123.f;
    ALGazePolicy::eyeInHeadFromWorldGaze(
        LLVector3::zero, identity_head, 30.f, 20.f, yaw, pitch);
    ensure("zero world gaze vector through the full solve is finite",
           std::isfinite(yaw) && std::isfinite(pitch));

    const LLVector3 non_finite_dir(
        std::numeric_limits<F32>::infinity(), 0.f, 0.f);
    ALGazePolicy::eyeInHeadRawFromWorldGaze(
        non_finite_dir, identity_head, yaw, pitch);
    ensure("non-finite world gaze vector returns finite eye-in-head",
           std::isfinite(yaw) && std::isfinite(pitch));

    const LLVector3 reconstructed_zero =
        ALGazePolicy::worldDirFromEyeInHead(0.f, 0.f, identity_head);
    ensure("zero eye-in-head reconstructs to a finite forward direction",
           reconstructed_zero.isFinite());
}

} // namespace tut
