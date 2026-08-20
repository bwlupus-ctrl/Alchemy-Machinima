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

template<> template<>
void algazemath_test_object::test<6>()
{
    set_test_name("deterministic hashing & seed extraction");
    const LLUUID id1("12345678-1234-1234-1234-123456789abc");
    const LLUUID id2("12345678-1234-1234-1234-123456789abd");
    const U64 seed1 = ALGazeMath::castSeedFromUUID(id1);
    const U64 seed2 = ALGazeMath::castSeedFromUUID(id2);
    ensure("seed from uuid is nonzero", seed1 != 0 && seed2 != 0);
    ensure("distinct UUIDs give distinct seeds", seed1 != seed2);
    ensure("same UUID gives bit-identical seed", ALGazeMath::castSeedFromUUID(id1) == seed1);

    const F32 h1 = ALGazeMath::unitHash(seed1, 1, 100, 0, 0);
    const F32 h1_dup = ALGazeMath::unitHash(seed1, 1, 100, 0, 0);
    const F32 h2 = ALGazeMath::unitHash(seed2, 1, 100, 0, 0);
    ensure("unitHash is in [0, 1)", h1 >= 0.f && h1 < 1.f);
    ensure("unitHash is bit-exact deterministic", h1 == h1_dup);
    ensure("distinct seeds produce decorrelated hashes", h1 != h2);
}

template<> template<>
void algazemath_test_object::test<7>()
{
    set_test_name("asymmetric ease envelope");
    constexpr F32 EASE_ACQUIRE = 0.25f;
    constexpr F32 EASE_RELEASE = 0.60f;
    constexpr F32 DT = 0.1f;

    const F32 acquired = ALGazeMath::asymmetricEnvStep(0.f, true, DT, EASE_ACQUIRE, EASE_RELEASE);
    const F32 released = ALGazeMath::asymmetricEnvStep(1.f, false, DT, EASE_ACQUIRE, EASE_RELEASE);
    ensure("acquire step matches expected rate", std::fabs(acquired - (DT / EASE_ACQUIRE)) <= 1e-6f);
    ensure("release step matches expected rate", std::fabs(released - (1.f - DT / EASE_RELEASE)) <= 1e-6f);
    ensure("acquire is faster than release", acquired > (1.f - released));

    // Zero ease tests
    ensure("zero acquire snaps to 1", ALGazeMath::asymmetricEnvStep(0.f, true, 0.01f, 0.f, EASE_RELEASE) == 1.f);
    ensure("zero release snaps to 0", ALGazeMath::asymmetricEnvStep(1.f, false, 0.01f, EASE_ACQUIRE, 0.f) == 0.f);
}

template<> template<>
void algazemath_test_object::test<8>()
{
    set_test_name("reaction latency bounds & determinism");
    constexpr U64 SEED_A = 0xabcdef1234567890ULL;
    constexpr U64 SEED_B = 0x9876543210fedcbaULL;
    constexpr F32 MIN_D = 0.15f;
    constexpr F32 MAX_D = 0.55f;

    const F32 delay_a1 = ALGazeMath::evalReactionDelay(SEED_A, MIN_D, MAX_D);
    const F32 delay_a2 = ALGazeMath::evalReactionDelay(SEED_A, MIN_D, MAX_D);
    const F32 delay_b = ALGazeMath::evalReactionDelay(SEED_B, MIN_D, MAX_D);

    ensure("reaction delay is deterministic", delay_a1 == delay_a2);
    ensure("reaction delay respects bounds", delay_a1 >= MIN_D && delay_a1 <= MAX_D);
    ensure("different subjects receive different delays", delay_a1 != delay_b);
}

template<> template<>
void algazemath_test_object::test<9>()
{
    set_test_name("micro-life (saccades, head drift, blinks)");
    constexpr U64 SEED = 0x55aa55aa55aa55aaULL;

    // Zero micro life produces zero offsets
    const auto zero_life = ALGazeMath::evalMicroLife(SEED, 12.34, 0.f, true);
    ensure("zero micro life gives zero saccade yaw", zero_life.mEyeSaccadeYaw == 0.f);
    ensure("zero micro life gives zero saccade pitch", zero_life.mEyeSaccadePitch == 0.f);
    ensure("zero micro life gives zero drift yaw", zero_life.mHeadDriftYaw == 0.f);
    ensure("zero micro life gives zero drift pitch", zero_life.mHeadDriftPitch == 0.f);

    // Active micro life
    const auto life1 = ALGazeMath::evalMicroLife(SEED, 15.0, 0.5f, true);
    const auto life1_dup = ALGazeMath::evalMicroLife(SEED, 15.0, 0.5f, true);
    ensure("micro life is closed-form deterministic",
           life1.mEyeSaccadeYaw == life1_dup.mEyeSaccadeYaw &&
           life1.mHeadDriftYaw == life1_dup.mHeadDriftYaw);

    // Saccades and head drift are within anatomical micro bounds (< 2 degrees)
    ensure("saccade yaw bounded", std::fabs(life1.mEyeSaccadeYaw) <= 2.f * DEG_TO_RAD);
    ensure("head drift yaw bounded", std::fabs(life1.mHeadDriftYaw) <= 1.5f * DEG_TO_RAD);

    const F64 interval = 0.36 +
        static_cast<F64>(ALGazeMath::unitHash(SEED, 1, 0, 1, 0)) * 0.18;
    const F64 phase =
        static_cast<F64>(ALGazeMath::unitHash(SEED, 1, 0, 1, 1)) * interval;
    const F64 boundary = interval - phase;
    const auto before = ALGazeMath::evalMicroLife(
        SEED, llmax(boundary - 1e-7, 0.0), 0.5f, false);
    const auto at = ALGazeMath::evalMicroLife(SEED, boundary, 0.5f, false);
    ensure("saccade interpolation is continuous at its hashed boundary",
           std::fabs(before.mEyeSaccadeYaw - at.mEyeSaccadeYaw) <= 1e-5f &&
           std::fabs(before.mEyeSaccadePitch - at.mEyeSaccadePitch) <= 1e-5f);
}

template<> template<>
void algazemath_test_object::test<10>()
{
    set_test_name("naturalized glance breaks");
    constexpr U64 SEED = 0x1122334455667788ULL;

    F32 yaw = 0.f, pitch = 0.f, weight = 0.f;
    ALGazeMath::evalNaturalBreak(SEED, 10.0, 0.f, yaw, pitch, weight);
    ensure("frequency 0 never breaks", weight == 0.f && yaw == 0.f && pitch == 0.f);

    // Sweep across time to confirm break triggers and remains bounded
    bool found_break = false;
    for (F64 t = 0.0; t < 30.0; t += 0.25)
    {
        ALGazeMath::evalNaturalBreak(SEED, t, 0.8f, yaw, pitch, weight);
        if (weight > 0.f)
        {
            found_break = true;
            ensure("break deflection is bounded",
                   std::fabs(yaw) <= 20.f * DEG_TO_RAD && std::fabs(pitch) <= 15.f * DEG_TO_RAD);
            ensure("break weight in [0, 1]", weight >= 0.f && weight <= 1.f);
        }
    }
    ensure("breaks trigger across a 30s window with high frequency", found_break);
}

template<> template<>
void algazemath_test_object::test<11>()
{
    set_test_name("anatomical chain distribution & body turn trigger");
    ALGazeMath::AnatomicalChainPose pose;

    // 1. Eyes-only blend
    ALGazeMath::distributeAnatomicalChain(30.f * DEG_TO_RAD, 10.f * DEG_TO_RAD, 0.f, 0.25f, 90.f, pose);
    ensure("eyes-only blend leaves head at 0", pose.mHeadYaw == 0.f && pose.mNeckYaw == 0.f);
    ensure("eyes-only blend leaves torso at 0", pose.mTorsoYaw == 0.f);
    ensure("eyes take angle capped at eye limit", std::fabs(pose.mEyeYaw - 25.f * DEG_TO_RAD) <= 1e-5f);
    ensure("no body turn at 30 deg", !pose.mTriggerBodyTurn);

    // 2. Low angle: eyes saturate first, then head; later joints stay idle.
    ALGazeMath::distributeAnatomicalChain(20.f * DEG_TO_RAD, 0.f, 1.f, 0.f, 90.f, pose);
    ensure("eyes recruit before the head", pose.mEyeYaw > 0.f && pose.mHeadYaw > 0.f);
    ensure("neck waits for head saturation", pose.mNeckYaw == 0.f);
    ensure("no torso when torso_amount is 0 and within comfort zone", pose.mTorsoYaw == 0.f);

    // 3. Every joint stays independently clamped at a comfortable 70-degree aim.
    ALGazeMath::distributeAnatomicalChain(70.f * DEG_TO_RAD, 0.f, 1.f, 1.f, 90.f, pose);
    ensure("head respects its independent 35 degree limit",
           std::fabs(pose.mHeadYaw) <= 35.001f * DEG_TO_RAD);
    ensure("neck is recruited only after head saturation", pose.mNeckYaw > 0.f);
    ensure("chest waits until eyes/head/neck saturate", pose.mTorsoYaw == 0.f);

    // 4. High angle (110 deg yaw): exceeds threshold and recruits chest/hips.
    ALGazeMath::distributeAnatomicalChain(110.f * DEG_TO_RAD, 0.f, 1.f, 0.5f, 90.f, pose);
    ensure("exceeding body turn threshold triggers body turn", pose.mTriggerBodyTurn);
    ensure("neck + head saturate at comfortable limit (~70 deg)",
           (pose.mHeadYaw + pose.mNeckYaw) <= 70.01f * DEG_TO_RAD);
    ensure("torso recruits on large angles", pose.mTorsoYaw > 0.f);
    ensure("hips recruit only after the chest saturates", pose.mHipsYaw > 0.f);
}

template<> template<>
void algazemath_test_object::test<12>()
{
    set_test_name("per-subject variation jitter");
    constexpr U64 SEED = 0x9988776655443322ULL;
    ALGazeMath::GazeLifeParams base_params;
    ALGazeMath::GazeLifeParams varied_params;
    F32 int_scale = 1.f;
    F32 sm_scale = 1.f;

    // Variation 0 -> exact base parameters
    ALGazeMath::applySubjectVariation(SEED, 0.f, base_params, varied_params, int_scale, sm_scale);
    ensure("variation 0 preserves intensity scale", int_scale == 1.f);
    ensure("variation 0 preserves smoothing scale", sm_scale == 1.f);
    ensure("variation 0 preserves reaction min", varied_params.mReactionMin == base_params.mReactionMin);

    // Variation 0.3 -> jittered within +/-15%
    ALGazeMath::applySubjectVariation(SEED, 0.3f, base_params, varied_params, int_scale, sm_scale);
    ensure("intensity scale within bounds", int_scale >= 0.8f && int_scale <= 1.2f);
    ensure("smoothing scale within bounds", sm_scale >= 0.8f && sm_scale <= 1.2f);
}

template<> template<>
void algazemath_test_object::test<13>()
{
    set_test_name("lid follow closure");
    ensure("upward gaze does not close lids",
           ALGazeMath::lidFollowClosure(-10.f * DEG_TO_RAD, 0.6f) == 0.f);
    ensure("zero gain disables follow",
           ALGazeMath::lidFollowClosure(10.f * DEG_TO_RAD, 0.f) == 0.f);
    const F32 shallow = ALGazeMath::lidFollowClosure(4.f * DEG_TO_RAD, 0.6f);
    const F32 deep = ALGazeMath::lidFollowClosure(12.f * DEG_TO_RAD, 0.6f);
    ensure("downward lid closure grows monotonically", deep > shallow && shallow > 0.f);
    ensure("follow is bounded by its gain", deep <= 0.6f);
}

template<> template<>
void algazemath_test_object::test<14>()
{
    set_test_name("vergence by target distance");
    const F32 near_angle = ALGazeMath::vergenceAngle(0.6f);
    const F32 mid_angle = ALGazeMath::vergenceAngle(1.5f);
    const F32 far_angle = ALGazeMath::vergenceAngle(5.f);
    ensure("vergence decreases monotonically with distance",
           near_angle > mid_angle && mid_angle > far_angle && far_angle > 0.f);
    ensure("a close-up converges by a plausible few degrees",
           near_angle > 2.f * DEG_TO_RAD && near_angle < 4.f * DEG_TO_RAD);
    ensure("vergence is approximately zero beyond a few metres",
           far_angle < 0.5f * DEG_TO_RAD);
    ensure("relaxed vergence can be exactly parallel",
           ALGazeMath::vergenceAngle(0.6f, 0.064f, 0.f) == 0.f);
}

template<> template<>
void algazemath_test_object::test<15>()
{
    set_test_name("persona primitive mapping");
    ALGazeMath::GazePersona neutral;
    const auto base = ALGazeMath::mapPersona(neutral);

    ALGazeMath::GazePersona dominant;
    dominant.mDominance = 1.f;
    const auto alpha = ALGazeMath::mapPersona(dominant);
    ensure("dominance lengthens contact holds",
           alpha.mContactHoldScale > base.mContactHoldScale);
    ensure("alpha stare suppresses breaks and blinks",
           alpha.mAlphaStare && alpha.mBreakFrequencyScale == 0.f &&
           alpha.mBlinkRateScale == 0.f);
    ensure("alpha stare squares the head and lowers the chin",
           alpha.mHeadEyeBlendAdd > 0.f && alpha.mChinPitchBias > 2.f * DEG_TO_RAD);
    ensure("alpha stare damps micro-life", alpha.mMicroLifeScale < 0.2f);
    ALGazeMath::GazeLifeParams life;
    ALGazeMath::GazeLifeParams alpha_life;
    ALGazeMath::applyPersona(alpha, life, alpha_life);
    ensure("alpha primitive modulation disables the blink lattice",
           !alpha_life.mBlinks && alpha_life.mBlinkRate == 0.f);
    ensure("alpha primitive modulation zeroes authored break frequency",
           alpha_life.mBreakFrequency == 0.f);

    ALGazeMath::GazePersona anxious;
    anxious.mAffection = -1.f;
    anxious.mAnxiety = 1.f;
    const auto shy = ALGazeMath::mapPersona(anxious);
    ensure("anxiety shortens holds and raises break/blink rates",
           shy.mContactHoldScale < base.mContactHoldScale &&
           shy.mBreakFrequencyScale > base.mBreakFrequencyScale &&
           shy.mBlinkRateScale > base.mBlinkRateScale);
    ensure("anxious low-affection persona produces side-eye and down aversion",
           shy.mSideEyeStrength > 0.9f && shy.mDownwardAversion > 0.9f);

    ALGazeMath::GazePersona affectionate;
    affectionate.mAffection = 1.f;
    const auto warm = ALGazeMath::mapPersona(affectionate);
    ensure("affection softens acquire and release",
           warm.mAcquireEaseScale > 1.f && warm.mReleaseEaseScale > 1.f);
    ensure("affection adds a slight lid narrow and upward bias",
           warm.mLidNarrow > 0.f && warm.mEyelinePitchBias < 0.f);
}

template<> template<>
void algazemath_test_object::test<16>()
{
    set_test_name("deterministic contact ration");
    constexpr U64 SEED = 0x1020304050607080ULL;
    ALGazeMath::GazePersona anxious;
    anxious.mAffection = -1.f;
    anxious.mAnxiety = 1.f;
    const auto shy = ALGazeMath::mapPersona(anxious);

    F64 aversion_time = -1.0;
    ALGazeMath::ContactRationOffset sample;
    for (F64 t = 0.0; t < 10.0; t += 0.01)
    {
        sample = ALGazeMath::evalContactRation(SEED, t, shy);
        if (sample.mWeight > 0.9f)
        {
            aversion_time = t;
            break;
        }
    }
    ensure("an anxious persona reaches a mandatory aversion", aversion_time >= 0.0);
    const auto duplicate = ALGazeMath::evalContactRation(SEED, aversion_time, shy);
    ensure("contact schedule is closed-form deterministic",
           sample.mWeight == duplicate.mWeight &&
           sample.mDeflectYaw == duplicate.mDeflectYaw &&
           sample.mDeflectPitch == duplicate.mDeflectPitch);
    ensure("anxious aversion is downward rather than lateral",
           sample.mDeflectPitch > 0.f && std::fabs(sample.mDeflectYaw) < 1e-6f);

    ALGazeMath::GazePersona dominant;
    dominant.mDominance = 1.f;
    const auto alpha = ALGazeMath::mapPersona(dominant);
    for (F64 t = 0.0; t < 30.0; t += 0.25)
    {
        ensure("alpha stare never enters an aversion",
               !ALGazeMath::evalContactRation(SEED, t, alpha).mAverting);
    }
}

template<> template<>
void algazemath_test_object::test<17>()
{
    set_test_name("persona side-eye head clamp");
    ALGazeMath::AnatomicalChainPose pose;
    ALGazeMath::distributeAnatomicalChain(
        80.f * DEG_TO_RAD, 0.f, 1.f, 0.f, 90.f, pose);
    const F32 before = pose.mHeadYaw + pose.mNeckYaw;
    ALGazeMath::applySideEye(1.f, pose);
    const F32 after = pose.mHeadYaw + pose.mNeckYaw;
    ensure("full suspicion reduces head/neck commitment", after < before);
    ensure("full suspicion clamps combined head yaw to fifteen degrees",
           std::fabs(after) <= 15.001f * DEG_TO_RAD);
}

template<> template<>
void algazemath_test_object::test<18>()
{
    set_test_name("plain gaze cue acquire, hold, and release");
    ALGazeMath::GazeMacroParams params;
    params.mAcquireSec = 0.25f;
    params.mReleaseSec = 0.50f;
    params.mDurationSec = 1.0;
    constexpr U64 SEED = 0x1111222233334444ULL;

    const auto start = ALGazeMath::evalGazeMacro(
        ALGazeMath::GAZE_MACRO_NONE, SEED, 0.0, params);
    const auto held = ALGazeMath::evalGazeMacro(
        ALGazeMath::GAZE_MACRO_NONE, SEED, 0.5, params);
    const auto end = ALGazeMath::evalGazeMacro(
        ALGazeMath::GAZE_MACRO_NONE, SEED, 1.6, params);
    ensure("plain cue starts on its prior aim", start.mTargetWeight == 0.f);
    ensure("plain cue reaches a full hold", held.mTargetWeight == 1.f &&
                                             held.mPhase == ALGazeMath::GAZE_CUE_HOLD);
    ensure("plain cue releases fully after duration", end.mTargetWeight == 0.f &&
                                                        end.mPhase == ALGazeMath::GAZE_CUE_RELEASE);
}

template<> template<>
void algazemath_test_object::test<19>()
{
    set_test_name("Double-Take deterministic fixed sequence");
    ALGazeMath::GazeMacroParams params;
    params.mAcquireSec = 0.10f;
    params.mReleaseSec = 0.30f;
    params.mDwellSec = 0.20f;
    params.mGapSec = 0.35f;
    constexpr U64 SEED = 0x5555666677778888ULL;

    const auto gap = ALGazeMath::evalGazeMacro(
        ALGazeMath::GAZE_MACRO_DOUBLE_TAKE, SEED, 0.75, params);
    const auto snap = ALGazeMath::evalGazeMacro(
        ALGazeMath::GAZE_MACRO_DOUBLE_TAKE, SEED, 0.98, params);
    const auto overshoot = ALGazeMath::evalGazeMacro(
        ALGazeMath::GAZE_MACRO_DOUBLE_TAKE, SEED, 1.105, params);
    const auto duplicate = ALGazeMath::evalGazeMacro(
        ALGazeMath::GAZE_MACRO_DOUBLE_TAKE, SEED, 1.105, params);
    ensure("Double-Take contains an authored away gap",
           gap.mPhase == ALGazeMath::GAZE_CUE_GAP && gap.mTargetWeight == 0.f);
    ensure("Double-Take snaps back after the gap",
           snap.mPhase == ALGazeMath::GAZE_CUE_ACQUIRE && snap.mTargetWeight > 0.f);
    ensure("Double-Take overshoots, opens lids, and recoils",
           overshoot.mTargetWeight > 1.f && overshoot.mLidWiden > 0.f &&
           overshoot.mHeadRecoilPitch < 0.f);
    ensure("Double-Take is bit-exact for the same seed and time",
           overshoot.mTargetWeight == duplicate.mTargetWeight &&
           overshoot.mLidWiden == duplicate.mLidWiden &&
           overshoot.mHeadRecoilPitch == duplicate.mHeadRecoilPitch);
}

template<> template<>
void algazemath_test_object::test<20>()
{
    set_test_name("Button Look holds edit frames past duration");
    ALGazeMath::GazeMacroParams params;
    params.mAcquireSec = 0.20f;
    params.mReleaseSec = 0.30f;
    params.mDurationSec = 1.0;
    params.mHoldFramesPast = 3; // 100 ms at the deterministic 30 fps edit basis

    const auto through_cut = ALGazeMath::evalGazeMacro(
        ALGazeMath::GAZE_MACRO_BUTTON_LOOK, 1, 1.05, params);
    const auto after = ALGazeMath::evalGazeMacro(
        ALGazeMath::GAZE_MACRO_BUTTON_LOOK, 1, 1.50, params);
    ensure("Button Look is still held inside hold-frames-past",
           through_cut.mTargetWeight == 1.f &&
           through_cut.mPhase == ALGazeMath::GAZE_CUE_HOLD);
    ensure("Button Look releases after its past-cut hold",
           after.mTargetWeight == 0.f &&
           after.mPhase == ALGazeMath::GAZE_CUE_RELEASE);
}

template<> template<>
void algazemath_test_object::test<21>()
{
    set_test_name("Creep Turn staggered anatomical chain");
    ALGazeMath::GazeMacroParams params;
    params.mDurationSec = 4.0;
    const auto middle = ALGazeMath::evalGazeMacro(
        ALGazeMath::GAZE_MACRO_CREEP_TURN, 2, 2.0, params);
    const auto landed = ALGazeMath::evalGazeMacro(
        ALGazeMath::GAZE_MACRO_CREEP_TURN, 2, 4.0, params);
    const auto duplicate = ALGazeMath::evalGazeMacro(
        ALGazeMath::GAZE_MACRO_CREEP_TURN, 2, 2.0, params);
    ensure("Creep Turn eyes lead head and body",
           middle.mEyeWeight > middle.mHeadWeight &&
           middle.mHeadWeight > middle.mBodyWeight);
    ensure("Creep Turn body is recruited last at duration",
           landed.mEyeWeight == 1.f && landed.mHeadWeight == 1.f &&
           landed.mBodyWeight == 1.f);
    ensure("Creep Turn is closed-form deterministic",
           middle.mEyeWeight == duplicate.mEyeWeight &&
           middle.mHeadWeight == duplicate.mHeadWeight &&
           middle.mBodyWeight == duplicate.mBodyWeight);
}

template<> template<>
void algazemath_test_object::test<22>()
{
    set_test_name("Object Glance dwell, return, and thought residue");
    ALGazeMath::GazeMacroParams params;
    params.mAcquireSec = 0.20f;
    params.mReleaseSec = 0.40f;
    params.mDwellSec = 0.50f;
    const auto dwell = ALGazeMath::evalGazeMacro(
        ALGazeMath::GAZE_MACRO_OBJECT_GLANCE, 3, 0.50, params);
    const auto normal_return = ALGazeMath::evalGazeMacro(
        ALGazeMath::GAZE_MACRO_OBJECT_GLANCE, 3, 0.90, params);
    params.mThoughtResidue = true;
    const auto heavy_return = ALGazeMath::evalGazeMacro(
        ALGazeMath::GAZE_MACRO_OBJECT_GLANCE, 3, 0.90, params);
    const auto duplicate = ALGazeMath::evalGazeMacro(
        ALGazeMath::GAZE_MACRO_OBJECT_GLANCE, 3, 0.90, params);
    ensure("Object Glance holds during dwell",
           dwell.mTargetWeight == 1.f && dwell.mPhase == ALGazeMath::GAZE_CUE_HOLD);
    ensure("thought residue makes the same return heavier/slower",
           heavy_return.mTargetWeight > normal_return.mTargetWeight);
    ensure("Object Glance residue is deterministic",
           heavy_return.mTargetWeight == duplicate.mTargetWeight &&
           heavy_return.mPhase == duplicate.mPhase);
}

template<> template<>
void algazemath_test_object::test<23>()
{
    set_test_name("vergence direction points both eyes inward");
    constexpr F32 IPD = 0.064f;
    constexpr F32 TARGET_DISTANCE = 0.60f;
    const F32 convergence =
        ALGazeMath::vergenceAngle(TARGET_DISTANCE, IPD, 1.f);
    // Positive eye-local yaw points toward avatar-left. The left eye starts at
    // +IPD/2 on that axis, so it needs negative yaw; the right needs positive.
    const F32 left_yaw = -convergence;
    const F32 right_yaw = convergence;
    ensure("left and right eye yaw point inward",
           left_yaw < 0.f && right_yaw > 0.f);

    const F32 left_target_y = 0.5f * IPD +
        tanf(left_yaw) * TARGET_DISTANCE;
    const F32 right_target_y = -0.5f * IPD +
        tanf(right_yaw) * TARGET_DISTANCE;
    ensure("both eye rays cross the center line at the target distance",
           std::fabs(left_target_y) < 1e-6f &&
           std::fabs(right_target_y) < 1e-6f);
}

template<> template<>
void algazemath_test_object::test<24>()
{
    set_test_name("neutral persona has no mandatory contact aversion");
    constexpr U64 SEED = 0x8877665544332211ULL;
    const ALGazeMath::PersonaModulation neutral =
        ALGazeMath::mapPersona(ALGazeMath::GazePersona());
    for (F64 t = 0.0; t < 30.0; t += 0.125)
    {
        const ALGazeMath::ContactRationOffset sample =
            ALGazeMath::evalContactRation(SEED, t, neutral);
        ensure("neutral contact ration is exactly inactive",
               !sample.mAverting && sample.mWeight == 0.f &&
               sample.mDeflectYaw == 0.f && sample.mDeflectPitch == 0.f);
    }
}

template<> template<>
void algazemath_test_object::test<25>()
{
    set_test_name("anatomy exaggeration preserves 1x and expands face chain");
    ALGazeMath::AnatomicalChainPose default_pose;
    ALGazeMath::AnatomicalChainPose explicit_human_pose;
    ALGazeMath::AnatomicalChainPose extreme_pose;
    ALGazeMath::distributeAnatomicalChain(
        180.f * DEG_TO_RAD, 100.f * DEG_TO_RAD,
        1.f, 0.f, 90.f, default_pose);
    ALGazeMath::distributeAnatomicalChain(
        180.f * DEG_TO_RAD, 100.f * DEG_TO_RAD,
        1.f, 0.f, 90.f, explicit_human_pose, 1.f);

    ensure("explicit 1x is exactly the legacy/default chain",
           default_pose.mEyeYaw == explicit_human_pose.mEyeYaw &&
           default_pose.mEyePitch == explicit_human_pose.mEyePitch &&
           default_pose.mHeadYaw == explicit_human_pose.mHeadYaw &&
           default_pose.mHeadPitch == explicit_human_pose.mHeadPitch &&
           default_pose.mNeckYaw == explicit_human_pose.mNeckYaw &&
           default_pose.mNeckPitch == explicit_human_pose.mNeckPitch &&
           default_pose.mTorsoYaw == explicit_human_pose.mTorsoYaw &&
           default_pose.mTorsoPitch == explicit_human_pose.mTorsoPitch &&
           default_pose.mHipsYaw == explicit_human_pose.mHipsYaw &&
           default_pose.mHipsPitch == explicit_human_pose.mHipsPitch &&
           default_pose.mTriggerBodyTurn ==
               explicit_human_pose.mTriggerBodyTurn);

    ALGazeMath::distributeAnatomicalChain(
        180.f * DEG_TO_RAD, 100.f * DEG_TO_RAD,
        1.f, 0.f, 90.f, extreme_pose, 3.f);
    ensure("3x expands the eye yaw allocation",
           extreme_pose.mEyeYaw > default_pose.mEyeYaw);
    ensure("3x expands the head yaw allocation",
           extreme_pose.mHeadYaw > default_pose.mHeadYaw);
    ensure("3x expands the neck yaw allocation",
           extreme_pose.mNeckYaw > default_pose.mNeckYaw);
    ensure("exaggeration does not recruit a disabled torso",
           extreme_pose.mTorsoYaw == 0.f && extreme_pose.mHipsYaw == 0.f);
}

template<> template<>
void algazemath_test_object::test<26>()
{
    set_test_name("camera roll relative to world horizon");
    const LLVector3 forward(-1.f, 0.f, 0.f);
    const LLVector3 world_up(0.f, 0.f, 1.f);
    ensure("level camera has zero roll",
           ALGazeMath::cameraRollAboutForward(forward, world_up) == 0.f);

    constexpr F32 ROLL = 30.f * DEG_TO_RAD;
    const LLVector3 rolled_up = world_up * LLQuaternion(ROLL, forward);
    ensure("signed dutch angle is recovered",
           std::fabs(llsimple_angle(
               ALGazeMath::cameraRollAboutForward(forward, rolled_up) -
               ROLL)) <= 1e-5f);
    ensure("vertical camera uses deterministic neutral fallback",
           ALGazeMath::cameraRollAboutForward(world_up, LLVector3::y_axis) ==
               0.f);
}

} // namespace tut
