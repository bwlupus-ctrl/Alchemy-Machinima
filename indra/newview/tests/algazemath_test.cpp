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

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

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

// Bit-pattern equality: unlike `==`, this distinguishes +0.0f from -0.0f
// (the planted-hips and chest-split contracts are BYTE-identical, not merely
// value-identical).
bool bitwiseEqual(F32 a, F32 b)
{
    U32 au = 0;
    U32 bu = 0;
    memcpy(&au, &a, sizeof(au));
    memcpy(&bu, &b, sizeof(bu));
    return au == bu;
}

// Raw IEEE-754 bit pattern of an F32 as a hex string, for failure messages
// where the +0.0f/-0.0f (or 1-ulp) distinction is the whole point.
std::string bitsOf(F32 v)
{
    U32 u = 0;
    memcpy(&u, &v, sizeof(u));
    char buf[16];
    snprintf(buf, sizeof(buf), "0x%08x", u);
    return std::string(buf);
}

// Integer ULP distance between two F32s (difference of monotonic bit keys;
// +0.0f and -0.0f are 0 apart). Used for the SOFT profile==constant parity
// contract, which /fp:fast limits to ~1 ULP (see test 29's comment).
S64 ulpKey(F32 v)
{
    U32 u = 0;
    memcpy(&u, &v, sizeof(u));
    const S64 magnitude = static_cast<S64>(u & 0x7fffffffu);
    return (u & 0x80000000u) ? -magnitude : magnitude;
}

S64 ulpDistance(F32 a, F32 b)
{
    const S64 d = ulpKey(a) - ulpKey(b);
    return d < 0 ? -d : d;
}

// SOFT equality for /fp:fast restatement parity: the same real-number
// formula compiled in two places (profile path vs constant path, or a
// test-side restatement vs production) may round a couple ULPs apart when
// the optimizer contracts/reassociates the two shapes differently, and a
// capacity wobble subtracted from a target lands in a small residual's
// finer ULP scale (hence the absolute floor). Anything beyond this bound
// is a genuine arithmetic drift, not compiler noise.
bool softEqual(F32 a, F32 b)
{
    return ulpDistance(a, b) <= 2 ||
           std::fabs(a - b) <= 4.f * FLT_EPSILON * std::fabs(b) ||
           std::fabs(a - b) <= 8.f * FLT_EPSILON;
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

template<> template<>
void algazemath_test_object::test<27>()
{
    set_test_name("planted spine: hips stay bitwise +0 and reach excludes "
                  "hips");
    // 1. Planted hips are bitwise +0.0f (raw bit pattern zero, signbit clear)
    //    for positive, negative, and zero aim -- both below and beyond the
    //    planted reach.
    const F32 aims_deg[] = { 150.f, 60.f, 0.f, -60.f, -150.f };
    for (F32 aim_deg : aims_deg)
    {
        ALGazeMath::AnatomicalChainPose pose;
        ALGazeMath::distributeAnatomicalChain(
            aim_deg * DEG_TO_RAD, aim_deg * 0.6f * DEG_TO_RAD,
            1.f, 1.f, 90.f, pose, 1.f, /*recruit_hips=*/false);
        const std::string tag =
            " (aim " + std::to_string(aim_deg) + " deg)";
        ensure("planted hips yaw is exactly zero" + tag,
               pose.mHipsYaw == 0.f);
        ensure("planted hips pitch is exactly zero" + tag,
               pose.mHipsPitch == 0.f);
        ensure("planted hips yaw is bitwise +0.0f (signbit clear), got " +
                   bitsOf(pose.mHipsYaw) + tag,
               bitwiseEqual(pose.mHipsYaw, 0.f) &&
               !std::signbit(pose.mHipsYaw));
        ensure("planted hips pitch is bitwise +0.0f (signbit clear), got " +
                   bitsOf(pose.mHipsPitch) + tag,
               bitwiseEqual(pose.mHipsPitch, 0.f) &&
               !std::signbit(pose.mHipsPitch));
    }
    // Sanity: the same beyond-reach aim DOES recruit the hips when unplanted,
    // so the zeros above are the planting, not a saturated-earlier chain.
    {
        ALGazeMath::AnatomicalChainPose unplanted;
        ALGazeMath::distributeAnatomicalChain(
            150.f * DEG_TO_RAD, 0.f, 1.f, 1.f, 90.f, unplanted, 1.f, true);
        ensure("unplanted control recruits the hips at 150 deg",
               unplanted.mHipsYaw > 0.f);
    }

    // 2. Planted reach equals the weighted eye+head+neck+spine capacity sum
    //    (hips excluded), mirroring chainReachYaw's own constants and order.
    constexpr F32 EYE_MAX_YAW   = 25.f * DEG_TO_RAD;
    constexpr F32 HEAD_MAX_YAW  = 35.f * DEG_TO_RAD;
    constexpr F32 NECK_MAX_YAW  = 35.f * DEG_TO_RAD;
    constexpr F32 TORSO_MAX_YAW = 45.f * DEG_TO_RAD;
    const F32 blends[] = { 0.3f, 0.7f, 1.f };
    const F32 torsos[] = { 0.f, 0.5f, 1.f };
    for (F32 blend : blends)
    {
        for (F32 torso : torsos)
        {
            const F32 eye_weight = 1.f - 0.75f * blend;
            const F32 head_weight = blend;
            const F32 torso_weight = blend * torso;
            const F32 expected = EYE_MAX_YAW * eye_weight +
                                 HEAD_MAX_YAW * head_weight +
                                 NECK_MAX_YAW * head_weight +
                                 TORSO_MAX_YAW * torso_weight;
            const F32 reach = ALGazeMath::chainReachYaw(
                blend, torso, 1.f, /*recruit_hips=*/false);
            // softEqual, not ==: this is a test-side RESTATEMENT of the
            // production sum, and /fp:fast may contract the two compiles
            // a couple ULPs apart (see softEqual's comment).
            ensure("planted reach is the hips-free weighted capacity sum "
                       "(blend " + std::to_string(blend) + " torso " +
                       std::to_string(torso) + ", reach " + bitsOf(reach) +
                       " expected " + bitsOf(expected) + ")",
                   softEqual(reach, expected));
        }
    }

    // 3. Beyond planted reach the chain saturates with NO pelvis
    //    compensation: the allocation is identical however far past reach the
    //    demand goes (a stable saturation residual), and the hips stay zero.
    const F32 reach = ALGazeMath::chainReachYaw(1.f, 1.f, 1.f, false);
    ALGazeMath::AnatomicalChainPose sat_near;
    ALGazeMath::AnatomicalChainPose sat_far;
    const F32 deep_pitch = 120.f * DEG_TO_RAD; // beyond the ~91.5 deg pitch reach
    ALGazeMath::distributeAnatomicalChain(
        reach + 20.f * DEG_TO_RAD, deep_pitch, 1.f, 1.f, 90.f,
        sat_near, 1.f, false);
    ALGazeMath::distributeAnatomicalChain(
        reach + 100.f * DEG_TO_RAD, deep_pitch, 1.f, 1.f, 90.f,
        sat_far, 1.f, false);
    ensure("saturated planted allocation is stable however far past reach",
           bitwiseEqual(sat_near.mEyeYaw, sat_far.mEyeYaw) &&
           bitwiseEqual(sat_near.mHeadYaw, sat_far.mHeadYaw) &&
           bitwiseEqual(sat_near.mNeckYaw, sat_far.mNeckYaw) &&
           bitwiseEqual(sat_near.mTorsoYaw, sat_far.mTorsoYaw) &&
           bitwiseEqual(sat_near.mHipsYaw, sat_far.mHipsYaw) &&
           bitwiseEqual(sat_near.mEyePitch, sat_far.mEyePitch) &&
           bitwiseEqual(sat_near.mHeadPitch, sat_far.mHeadPitch) &&
           bitwiseEqual(sat_near.mNeckPitch, sat_far.mNeckPitch) &&
           bitwiseEqual(sat_near.mTorsoPitch, sat_far.mTorsoPitch) &&
           bitwiseEqual(sat_near.mHipsPitch, sat_far.mHipsPitch));
    ensure("saturated planted hips yaw is zero (no pelvis compensation)",
           sat_near.mHipsYaw == 0.f && sat_far.mHipsYaw == 0.f);
    ensure("saturated planted hips pitch is zero (no pelvis compensation)",
           sat_near.mHipsPitch == 0.f && sat_far.mHipsPitch == 0.f);
    const F32 yaw_sum = sat_near.mEyeYaw + sat_near.mHeadYaw +
                        sat_near.mNeckYaw + sat_near.mTorsoYaw;
    ensure("saturated planted yaw delivers the planted reach (within "
               "/fp:fast restatement rounding)",
           softEqual(yaw_sum, reach));
    constexpr F32 EYE_MAX_PITCH   = 14.f * DEG_TO_RAD;
    constexpr F32 HEAD_MAX_PITCH  = 42.f * DEG_TO_RAD;
    constexpr F32 NECK_MAX_PITCH  = 26.f * DEG_TO_RAD;
    constexpr F32 TORSO_MAX_PITCH = 20.f * DEG_TO_RAD;
    const F32 pitch_reach = EYE_MAX_PITCH * 0.25f + HEAD_MAX_PITCH +
                            NECK_MAX_PITCH + TORSO_MAX_PITCH;
    const F32 pitch_sum = sat_near.mEyePitch + sat_near.mHeadPitch +
                          sat_near.mNeckPitch + sat_near.mTorsoPitch;
    ensure("saturated planted pitch delivers the hips-free capacity (within "
               "/fp:fast restatement rounding)",
           softEqual(pitch_sum, pitch_reach));
}

template<> template<>
void algazemath_test_object::test<28>()
{
    set_test_name("chest split conserves the spine bucket bit-exactly");
    // The chest split divides the allocated spine (torso) bucket so
    // torso + chest == the chest_share == 0 torso component EXACTLY. The
    // missing-chest fallback is deliberately the CALLER's job (integration
    // folds chest back into torso when the joint is absent), so only the
    // math-level conservation is asserted here -- no joint lookup.
    const F32 shares[] = { 0.f, 0.55f, 1.f };
    const F32 signs[] = { 1.f, -1.f };
    const F32 blends[] = { 0.7f, 1.f };
    const F32 torsos[] = { 0.5f, 1.f };
    for (F32 sign : signs)
    {
        // Angles deep enough that the spine bucket is recruited nonzero for
        // every blend/torso combination below.
        const F32 yaw = sign * 130.f * DEG_TO_RAD;
        const F32 pitch = sign * 80.f * DEG_TO_RAD;
        for (F32 blend : blends)
        {
            for (F32 torso : torsos)
            {
                ALGazeMath::AnatomicalChainPose base;
                ALGazeMath::distributeAnatomicalChain(
                    yaw, pitch, blend, torso, 90.f, base, 1.f, true,
                    nullptr, 0.f);
                ensure("baseline recruits a nonzero spine bucket",
                       base.mTorsoYaw != 0.f && base.mTorsoPitch != 0.f);
                for (F32 share : shares)
                {
                    ALGazeMath::AnatomicalChainPose pose;
                    ALGazeMath::distributeAnatomicalChain(
                        yaw, pitch, blend, torso, 90.f, pose, 1.f, true,
                        nullptr, share);
                    const std::string tag =
                        " (share " + std::to_string(share) + " sign " +
                        std::to_string(sign) + " blend " +
                        std::to_string(blend) + " torso " +
                        std::to_string(torso) + ")";
                    if (share == 0.f || share == 1.f)
                    {
                        // The endpoints are genuinely bit-exact: share 0
                        // never touches the fields, share 1 moves the whole
                        // bucket (torso - torso == +0 exactly).
                        ensure("torso+chest yaw conserves the spine bucket "
                                   "bit-exactly" + tag,
                               bitwiseEqual(pose.mTorsoYaw + pose.mChestYaw,
                                            base.mTorsoYaw));
                        ensure("torso+chest pitch conserves the spine bucket "
                                   "bit-exactly" + tag,
                               bitwiseEqual(pose.mTorsoPitch +
                                                pose.mChestPitch,
                                            base.mTorsoPitch));
                    }
                    else
                    {
                        // Intermediate shares: chest = old * share and
                        // torso = old - chest are each correctly rounded,
                        // so the recombined sum may sit ~1 ulp off the
                        // baseline -- algebraic, not bit, conservation.
                        ensure("torso+chest yaw conserves the spine bucket "
                                   "within rounding" + tag,
                               std::fabs((pose.mTorsoYaw + pose.mChestYaw) -
                                         base.mTorsoYaw) <= 1e-6f);
                        ensure("torso+chest pitch conserves the spine bucket "
                                   "within rounding" + tag,
                               std::fabs((pose.mTorsoPitch +
                                          pose.mChestPitch) -
                                         base.mTorsoPitch) <= 1e-6f);
                    }
                    // The split never touches any other joint.
                    ensure("chest split leaves the rest of the chain "
                               "bit-identical" + tag,
                           bitwiseEqual(pose.mEyeYaw, base.mEyeYaw) &&
                           bitwiseEqual(pose.mEyePitch, base.mEyePitch) &&
                           bitwiseEqual(pose.mHeadYaw, base.mHeadYaw) &&
                           bitwiseEqual(pose.mHeadPitch, base.mHeadPitch) &&
                           bitwiseEqual(pose.mNeckYaw, base.mNeckYaw) &&
                           bitwiseEqual(pose.mNeckPitch, base.mNeckPitch) &&
                           bitwiseEqual(pose.mHipsYaw, base.mHipsYaw) &&
                           bitwiseEqual(pose.mHipsPitch, base.mHipsPitch));
                    if (share == 0.f)
                    {
                        ensure("share 0 leaves chest at bitwise +0" + tag,
                               bitwiseEqual(pose.mChestYaw, 0.f) &&
                               bitwiseEqual(pose.mChestPitch, 0.f) &&
                               !std::signbit(pose.mChestYaw) &&
                               !std::signbit(pose.mChestPitch));
                        ensure("share 0 leaves torso bit-identical to the "
                                   "baseline" + tag,
                               bitwiseEqual(pose.mTorsoYaw, base.mTorsoYaw) &&
                               bitwiseEqual(pose.mTorsoPitch,
                                            base.mTorsoPitch));
                    }
                    if (share == 1.f)
                    {
                        ensure("share 1 zeroes the torso exactly" + tag,
                               bitwiseEqual(pose.mTorsoYaw, 0.f) &&
                               bitwiseEqual(pose.mTorsoPitch, 0.f));
                        ensure("share 1 hands the whole bucket to the chest"
                                   + tag,
                               bitwiseEqual(pose.mChestYaw, base.mTorsoYaw) &&
                               bitwiseEqual(pose.mChestPitch,
                                            base.mTorsoPitch));
                    }
                }
            }
        }
    }
}

template<> template<>
void algazemath_test_object::test<29>()
{
    set_test_name("default limit profile matches the constant path within "
                  "rounding");
    // The lockstep contract here is SOFT: a DEFAULT-constructed
    // AnatomicalLimitProfile carries the exact legacy constants, but the
    // profile path and the constant path are two source-identical
    // RESTATEMENTS of the same arithmetic, and this build compiles with
    // /fp:fast (00-Common.cmake), under which the optimizer may contract or
    // reassociate the two shapes differently -- observed as 1-ULP
    // divergences (e.g. chainReachYaw at blend 0.3: 0x3f3465b2 vs
    // 0x3f3465b1; the deviating side even moves between builds as inlining
    // shifts). The HARD bit-exact contract is default/profile-OFF == the
    // pre-feature arithmetic (test 25 keeps that strict); this test pins
    // the profile==constant parity to softEqual (a couple ULPs, plus a
    // small absolute floor for the small-residual slots where a cap wobble
    // is amplified in the slot's finer ULP scale). Anything beyond that is
    // a real capacity-table drift.
    const ALGazeMath::AnatomicalLimitProfile profile; // defaults == constants
    const F32 blends[] = { 0.f, 0.001f, 0.3f, 0.7f, 1.f };
    const F32 torsos[] = { 0.f, 0.25f, 1.f };
    const F32 scales[] = { 1.f, 1.5f, 2.f, 3.f };
    const bool hips_opts[] = { true, false };
    const F32 angles_deg[] =
        { 0.f, 5.f, -5.f, 20.f, -45.f, 90.f, -120.f, 150.f, -179.f, 179.f };
    constexpr S32 NANGLES = sizeof(angles_deg) / sizeof(angles_deg[0]);

    for (F32 blend : blends)
    {
        for (F32 torso : torsos)
        {
            for (F32 scale : scales)
            {
                for (bool recruit_hips : hips_opts)
                {
                    const F32 reach_base = ALGazeMath::chainReachYaw(
                        blend, torso, scale, recruit_hips, nullptr);
                    const F32 reach_prof = ALGazeMath::chainReachYaw(
                        blend, torso, scale, recruit_hips, &profile);
                    ensure("default-profile reach matches the constant "
                               "reach within rounding (blend " +
                               std::to_string(blend) + " torso " +
                               std::to_string(torso) + " scale " +
                               std::to_string(scale) + " hips " +
                               std::to_string(recruit_hips) +
                               ", profile " + bitsOf(reach_prof) +
                               " constant " + bitsOf(reach_base) + ")",
                           softEqual(reach_prof, reach_base));

                    for (S32 ai = 0; ai < NANGLES; ++ai)
                    {
                        const F32 yaw = angles_deg[ai] * DEG_TO_RAD;
                        const F32 pitch =
                            angles_deg[(ai + 3) % NANGLES] * DEG_TO_RAD;
                        ALGazeMath::AnatomicalChainPose base;
                        ALGazeMath::AnatomicalChainPose prof;
                        ALGazeMath::distributeAnatomicalChain(
                            yaw, pitch, blend, torso, 90.f, base, scale,
                            recruit_hips, nullptr, 0.f);
                        ALGazeMath::distributeAnatomicalChain(
                            yaw, pitch, blend, torso, 90.f, prof, scale,
                            recruit_hips, &profile, 0.f);
                        const std::string tag =
                            " (blend " + std::to_string(blend) + " torso " +
                            std::to_string(torso) + " scale " +
                            std::to_string(scale) + " hips " +
                            std::to_string(recruit_hips) + " yaw_deg " +
                            std::to_string(angles_deg[ai]) + " pitch_deg " +
                            std::to_string(
                                angles_deg[(ai + 3) % NANGLES]) + ")";
                        const F32 prof_fields[12] =
                            { prof.mEyeYaw, prof.mEyePitch,
                              prof.mHeadYaw, prof.mHeadPitch,
                              prof.mNeckYaw, prof.mNeckPitch,
                              prof.mTorsoYaw, prof.mTorsoPitch,
                              prof.mChestYaw, prof.mChestPitch,
                              prof.mHipsYaw, prof.mHipsPitch };
                        const F32 base_fields[12] =
                            { base.mEyeYaw, base.mEyePitch,
                              base.mHeadYaw, base.mHeadPitch,
                              base.mNeckYaw, base.mNeckPitch,
                              base.mTorsoYaw, base.mTorsoPitch,
                              base.mChestYaw, base.mChestPitch,
                              base.mHipsYaw, base.mHipsPitch };
                        const char* field_names[12] =
                            { "mEyeYaw", "mEyePitch",
                              "mHeadYaw", "mHeadPitch",
                              "mNeckYaw", "mNeckPitch",
                              "mTorsoYaw", "mTorsoPitch",
                              "mChestYaw", "mChestPitch",
                              "mHipsYaw", "mHipsPitch" };
                        for (S32 f = 0; f < 12; ++f)
                        {
                            // Soft /fp:fast parity (see softEqual's and the
                            // test's comments).
                            ensure("default-profile " +
                                       std::string(field_names[f]) +
                                       " matches the constant chain within "
                                       "rounding (profile " +
                                       bitsOf(prof_fields[f]) +
                                       " constant " +
                                       bitsOf(base_fields[f]) + ")" + tag,
                                   softEqual(prof_fields[f],
                                             base_fields[f]));
                        }
                        ensure("default-profile body-turn trigger matches"
                                   + tag,
                               prof.mTriggerBodyTurn ==
                                   base.mTriggerBodyTurn);
                    }
                }
            }
        }
    }
}

template<> template<>
void algazemath_test_object::test<30>()
{
    set_test_name("angle-driven lean curve (spineLean)");
    using ALGazeMath::SpineLeanResult;
    using ALGazeMath::spineLean;

    const F32 BIG_DEG = 1000.f; // effectively unbounded max lean
    const F32 BIG_CAP = 100.f;  // effectively unbounded ellipse semi-axis, rad

    // 1. Zero aim: everything exactly (bitwise) zero, no division by zero.
    {
        const SpineLeanResult r =
            spineLean(0.f, 0.f, 25.f, 20.f, 20.f, 1.f, 1.f, 1.f, 1.f);
        ensure("zero aim gives an exactly zero spine and face",
               bitwiseEqual(r.mSpineYaw, 0.f) &&
               bitwiseEqual(r.mSpinePitch, 0.f) &&
               bitwiseEqual(r.mFaceYaw, 0.f) &&
               bitwiseEqual(r.mFacePitch, 0.f));
    }

    // 2. Below threshold: ease 0, spine exact zero, face carries the full aim.
    {
        const F32 yaw = 10.f * DEG_TO_RAD;
        const F32 pitch = -8.f * DEG_TO_RAD; // a ~= 12.8 deg < T = 25 deg
        const SpineLeanResult r = spineLean(
            yaw, pitch, 25.f, 20.f, 20.f, 1.f, 1.f, BIG_CAP, BIG_CAP);
        ensure("below-threshold spine is exactly zero",
               bitwiseEqual(r.mSpineYaw, 0.f) &&
               bitwiseEqual(r.mSpinePitch, 0.f));
        ensure("below-threshold face equals the full aim exactly",
               bitwiseEqual(r.mFaceYaw, yaw) &&
               bitwiseEqual(r.mFacePitch, pitch));
    }

    // 3. torso_amount 0 disables the spine even far above threshold.
    {
        const F32 yaw = -70.f * DEG_TO_RAD;
        const F32 pitch = 20.f * DEG_TO_RAD;
        const SpineLeanResult r = spineLean(
            yaw, pitch, 25.f, 20.f, 20.f, 0.f, 1.f, BIG_CAP, BIG_CAP);
        ensure("torso 0 spine is exactly zero",
               bitwiseEqual(r.mSpineYaw, 0.f) &&
               bitwiseEqual(r.mSpinePitch, 0.f));
        ensure("torso 0 face equals the full aim exactly",
               bitwiseEqual(r.mFaceYaw, yaw) &&
               bitwiseEqual(r.mFacePitch, pitch));
    }

    // 4. C2 continuity of the smootherstep band. With M and the caps huge and
    // torso/blend 1, the returned spine magnitude along a yaw-only aim is
    //   g(a) = 0            for a <= T
    //   g(a) = a * ease(u)  for T < a < T+S, u = (a-T)/S
    //   g(a) = a            for a >= T+S
    // whose analytic derivatives are
    //   g'  = ease + a*ease'(u)/S,  g'' = 2*ease'(u)/S + a*ease''(u)/S^2
    // with ease'(u) = 30u^2(1-u)^2 and ease''(u) = 60u(u-1)(2u-1); both
    // vanish at u=0 and u=1, which is exactly the C2 join. We sample just
    // below/at/above both edges plus a mid-band point, approximate g'/g''
    // by central finite differences, and require them to match the analytic
    // smootherstep formula (value continuity is checked to a tight tol).
    {
        const F32 T_DEG = 20.f;
        const F32 S_DEG = 30.f;
        const F64 T = static_cast<F64>(T_DEG) * DEG_TO_RAD;
        const F64 S = static_cast<F64>(S_DEG) * DEG_TO_RAD;

        auto g = [&](F64 a) -> F64
        {
            const SpineLeanResult r = spineLean(
                static_cast<F32>(a), 0.f, T_DEG, S_DEG, BIG_DEG,
                1.f, 1.f, BIG_CAP, BIG_CAP);
            return static_cast<F64>(r.mSpineYaw);
        };
        auto analytic = [&](F64 a, F64& d1, F64& d2) -> F64
        {
            if (a <= T)
            {
                d1 = 0.0;
                d2 = 0.0;
                return 0.0;
            }
            if (a >= T + S)
            {
                d1 = 1.0;
                d2 = 0.0;
                return a;
            }
            const F64 u = (a - T) / S;
            const F64 ease = u * u * u * (u * (u * 6.0 - 15.0) + 10.0);
            const F64 e1 = 30.0 * u * u * (1.0 - u) * (1.0 - u);
            const F64 e2 = 60.0 * u * (u - 1.0) * (2.0 * u - 1.0);
            d1 = ease + a * e1 / S;
            d2 = 2.0 * e1 / S + a * e2 / (S * S);
            return a * ease;
        };

        // Value continuity across both edges.
        const F64 delta = 1e-4;
        ensure("value continuous at the threshold edge",
               std::fabs(g(T + delta) - g(T - delta)) <= 5e-4);
        ensure("value continuous at the softness edge",
               std::fabs(g(T + S + delta) - g(T + S - delta)) <= 5e-4);

        // Step choice: the function returns F32, so each g() sample carries
        // ~3e-7 abs rounding noise; the d2 stencil amplifies that by 4/h^2
        // (h=2e-3 gave ~0.26 noise and a flaky assert) while truncation only
        // grows as h^2 * g'''' (~0.012 at h=5e-3 near the band edges).
        // h=5e-3 puts noise (~0.04) and truncation both well under the tols.
        const F64 h = 5e-3;
        const F64 samples[] =
        {
            T - 3.0 * h,        // just below the threshold edge
            T + 3.0 * h,        // just above the threshold edge
            T + 0.3 * S,        // mid-band: formula match, not just zeros
            T + S - 3.0 * h,    // just below the softness edge
            T + S + 3.0 * h     // just above the softness edge
        };
        for (F64 a : samples)
        {
            F64 d1_ref = 0.0;
            F64 d2_ref = 0.0;
            const F64 v_ref = analytic(a, d1_ref, d2_ref);
            const F64 v = g(a);
            const F64 d1 = (g(a + h) - g(a - h)) / (2.0 * h);
            const F64 d2 = (g(a + h) - 2.0 * v + g(a - h)) / (h * h);
            const std::string tag = " (a_deg " +
                std::to_string(a * RAD_TO_DEG) + ")";
            ensure("lean value matches the smootherstep formula" + tag,
                   std::fabs(v - v_ref) <= 1e-5);
            ensure("1st derivative matches the smootherstep formula" + tag,
                   std::fabs(d1 - d1_ref) <= 5e-3);
            ensure("2nd derivative matches the smootherstep formula" + tag,
                   std::fabs(d2 - d2_ref) <= 0.15);
        }
    }

    // 5. Max clamp: far past threshold with a small M, lean_mag == M and the
    // face keeps the remainder, both on-axis and diagonal.
    {
        const F32 M_RAD = 10.f * DEG_TO_RAD;
        const F32 yaw = 80.f * DEG_TO_RAD;
        const SpineLeanResult r = spineLean(
            yaw, 0.f, 5.f, 5.f, 10.f, 1.f, 1.f, BIG_CAP, BIG_CAP);
        ensure("yaw-only max clamp caps the lean at M",
               std::fabs(r.mSpineYaw - M_RAD) <= 1e-5f &&
               bitwiseEqual(r.mSpinePitch, 0.f));
        ensure("yaw-only face keeps the remainder v - d*M",
               std::fabs(r.mFaceYaw - (yaw - M_RAD)) <= 1e-5f &&
               std::fabs(r.mFacePitch) <= 1e-6f);

        const F32 dyaw = 60.f * DEG_TO_RAD;
        const F32 dpitch = 45.f * DEG_TO_RAD;
        const F32 mag = sqrtf(dyaw * dyaw + dpitch * dpitch);
        const SpineLeanResult rd = spineLean(
            dyaw, dpitch, 5.f, 5.f, 10.f, 1.f, 1.f, BIG_CAP, BIG_CAP);
        const F32 lean = sqrtf(rd.mSpineYaw * rd.mSpineYaw +
                               rd.mSpinePitch * rd.mSpinePitch);
        ensure("diagonal max clamp caps the lean magnitude at M",
               std::fabs(lean - M_RAD) <= 1e-5f);
        ensure("diagonal spine lies along the aim direction (d * M)",
               std::fabs(rd.mSpineYaw - (dyaw / mag) * M_RAD) <= 1e-5f &&
               std::fabs(rd.mSpinePitch - (dpitch / mag) * M_RAD) <= 1e-5f);
        ensure("diagonal face keeps the remainder v - d*M",
               std::fabs(rd.mFaceYaw - (dyaw - (dyaw / mag) * M_RAD)) <= 1e-5f &&
               std::fabs(rd.mFacePitch -
                         (dpitch - (dpitch / mag) * M_RAD)) <= 1e-5f);
    }

    // 6. Directional ellipse clamp. T=0/S=0 step with a huge M makes
    // requested == a, so a large aim saturates against the ellipse.
    {
        const F32 CY = 10.f * DEG_TO_RAD; // yaw semi-axis
        const F32 CP = 5.f * DEG_TO_RAD;  // pitch semi-axis

        // Yaw-only: cap distance is the yaw semi-axis.
        const SpineLeanResult ry = spineLean(
            40.f * DEG_TO_RAD, 0.f, 0.f, 0.f, BIG_DEG, 1.f, 1.f, CY, CP);
        ensure("yaw-only saturated spine sits on the yaw semi-axis",
               std::fabs(ry.mSpineYaw - CY) <= 1e-6f &&
               bitwiseEqual(ry.mSpinePitch, 0.f));

        // Pitch-only (negative): cap distance is the pitch semi-axis.
        const SpineLeanResult rp = spineLean(
            0.f, -40.f * DEG_TO_RAD, 0.f, 0.f, BIG_DEG, 1.f, 1.f, CY, CP);
        ensure("pitch-only saturated spine sits on the pitch semi-axis",
               std::fabs(rp.mSpinePitch + CP) <= 1e-6f &&
               bitwiseEqual(rp.mSpineYaw, 0.f));

        // Diagonal: the saturated spine lands exactly ON the ellipse, at the
        // analytic cap distance along the aim direction.
        const F32 dyaw = 30.f * DEG_TO_RAD;
        const F32 dpitch = 30.f * DEG_TO_RAD;
        const F32 mag = sqrtf(dyaw * dyaw + dpitch * dpitch);
        const F32 uy = dyaw / mag;
        const F32 up = dpitch / mag;
        const F32 cap_dist = 1.f / sqrtf((uy / CY) * (uy / CY) +
                                         (up / CP) * (up / CP));
        const SpineLeanResult rdg = spineLean(
            dyaw, dpitch, 0.f, 0.f, BIG_DEG, 1.f, 1.f, CY, CP);
        const F32 lean = sqrtf(rdg.mSpineYaw * rdg.mSpineYaw +
                               rdg.mSpinePitch * rdg.mSpinePitch);
        ensure("diagonal saturated lean equals the directional cap distance",
               std::fabs(lean - cap_dist) <= 1e-6f);
        const F32 ell = (rdg.mSpineYaw / CY) * (rdg.mSpineYaw / CY) +
                        (rdg.mSpinePitch / CP) * (rdg.mSpinePitch / CP);
        ensure("diagonal saturated spine sits exactly on the ellipse",
               std::fabs(ell - 1.f) <= 1e-4f);

        // Unsaturated: requested < cap_dist keeps the spine strictly inside
        // the ellipse at the requested magnitude.
        const F32 syaw = 2.f * DEG_TO_RAD;
        const F32 spitch = 2.f * DEG_TO_RAD;
        const F32 smag = sqrtf(syaw * syaw + spitch * spitch);
        const SpineLeanResult rin = spineLean(
            syaw, spitch, 0.f, 0.f, BIG_DEG, 1.f, 1.f, CY, CP);
        const F32 in_lean = sqrtf(rin.mSpineYaw * rin.mSpineYaw +
                                  rin.mSpinePitch * rin.mSpinePitch);
        ensure("unsaturated lean equals the requested magnitude",
               std::fabs(in_lean - smag) <= 1e-6f);
        ensure("unsaturated spine stays inside the ellipse",
               (rin.mSpineYaw / CY) * (rin.mSpineYaw / CY) +
               (rin.mSpinePitch / CP) * (rin.mSpinePitch / CP) <= 1.f + 1e-5f);

        // Zero cap on a needed axis makes the direction unreachable...
        const SpineLeanResult rz = spineLean(
            dyaw, dpitch, 0.f, 0.f, BIG_DEG, 1.f, 1.f, CY, 0.f);
        ensure("diagonal aim with a zero pitch cap gets zero spine",
               bitwiseEqual(rz.mSpineYaw, 0.f) &&
               bitwiseEqual(rz.mSpinePitch, 0.f) &&
               bitwiseEqual(rz.mFaceYaw, dyaw) &&
               bitwiseEqual(rz.mFacePitch, dpitch));
        // ...but a yaw-only aim never touches the zero pitch axis.
        const SpineLeanResult rzy = spineLean(
            40.f * DEG_TO_RAD, 0.f, 0.f, 0.f, BIG_DEG, 1.f, 1.f, CY, 0.f);
        ensure("yaw-only aim ignores a zero pitch cap",
               std::fabs(rzy.mSpineYaw - CY) <= 1e-6f &&
               bitwiseEqual(rzy.mSpinePitch, 0.f));
    }

    // 7. Conservation: spine + face reconstructs the aim component-wise
    // across the regimes above (below threshold, band, max clamp, ellipse
    // clamp, zero-cap axis).
    {
        struct Case
        {
            F32 yaw_deg, pitch_deg, t, s, m, torso, blend, cy, cp;
        };
        const Case cases[] =
        {
            {  10.f,  -8.f, 25.f, 20.f, 20.f, 1.f,  1.f,  BIG_CAP, BIG_CAP },
            {  40.f,  25.f, 20.f, 30.f, 1000.f, 1.f, 1.f, BIG_CAP, BIG_CAP },
            {  80.f,   0.f,  5.f,  5.f, 10.f, 1.f,  1.f,  BIG_CAP, BIG_CAP },
            {  30.f,  30.f,  0.f,  0.f, 1000.f, 1.f, 1.f,
               10.f * DEG_TO_RAD, 5.f * DEG_TO_RAD },
            { -60.f,  45.f,  0.f,  0.f, 1000.f, 1.f, 1.f,
               10.f * DEG_TO_RAD, 0.f },
            { -70.f, -20.f, 25.f, 20.f, 20.f, 0.5f, 0.75f,
               8.f * DEG_TO_RAD, 4.f * DEG_TO_RAD },
        };
        for (const Case& c : cases)
        {
            const F32 yaw = c.yaw_deg * DEG_TO_RAD;
            const F32 pitch = c.pitch_deg * DEG_TO_RAD;
            const SpineLeanResult r = spineLean(
                yaw, pitch, c.t, c.s, c.m, c.torso, c.blend, c.cy, c.cp);
            const std::string tag = " (yaw_deg " + std::to_string(c.yaw_deg) +
                " pitch_deg " + std::to_string(c.pitch_deg) + ")";
            ensure("spine + face reconstructs the aim yaw" + tag,
                   std::fabs((r.mSpineYaw + r.mFaceYaw) - yaw) <= 1e-6f);
            ensure("spine + face reconstructs the aim pitch" + tag,
                   std::fabs((r.mSpinePitch + r.mFacePitch) - pitch) <= 1e-6f);
        }
    }
}

template<> template<>
void algazemath_test_object::test<31>()
{
    set_test_name("asymmetric up/down pitch: defaults byte-identical; scale "
                  "and profile raise only the upward reach");
    using ALGazeMath::AnatomicalChainPose;
    using ALGazeMath::AnatomicalLimitProfile;
    using ALGazeMath::distributeAnatomicalChain;

    auto poseBitEqual = [](const AnatomicalChainPose& a,
                           const AnatomicalChainPose& b) -> bool
    {
        return bitwiseEqual(a.mEyeYaw, b.mEyeYaw) &&
               bitwiseEqual(a.mEyePitch, b.mEyePitch) &&
               bitwiseEqual(a.mHeadYaw, b.mHeadYaw) &&
               bitwiseEqual(a.mHeadPitch, b.mHeadPitch) &&
               bitwiseEqual(a.mNeckYaw, b.mNeckYaw) &&
               bitwiseEqual(a.mNeckPitch, b.mNeckPitch) &&
               bitwiseEqual(a.mTorsoYaw, b.mTorsoYaw) &&
               bitwiseEqual(a.mTorsoPitch, b.mTorsoPitch) &&
               bitwiseEqual(a.mChestYaw, b.mChestYaw) &&
               bitwiseEqual(a.mChestPitch, b.mChestPitch) &&
               bitwiseEqual(a.mHipsYaw, b.mHipsYaw) &&
               bitwiseEqual(a.mHipsPitch, b.mHipsPitch);
    };
    auto pitchMagSum = [](const AnatomicalChainPose& p) -> F32
    {
        return std::fabs(p.mEyePitch) + std::fabs(p.mHeadPitch) +
               std::fabs(p.mNeckPitch) + std::fabs(p.mTorsoPitch) +
               std::fabs(p.mHipsPitch);
    };

    // 1. Byte-parity of the defaults: an explicit pitch_up_scale == 1 call is
    //    bit-identical to the default-argument call across a signed grid, and
    //    the default (symmetric) profile fills a bitwise-identical up table.
    {
        const F32 blends[] = { 0.f, 0.3f, 1.f };
        const F32 pitches_deg[] =
            { -120.f, -60.f, -20.f, -5.f, 0.f, 5.f, 20.f, 60.f, 120.f };
        for (F32 blend : blends)
        {
            for (F32 pitch_deg : pitches_deg)
            {
                AnatomicalChainPose a;
                AnatomicalChainPose b;
                distributeAnatomicalChain(
                    40.f * DEG_TO_RAD, pitch_deg * DEG_TO_RAD,
                    blend, 1.f, 90.f, a);
                distributeAnatomicalChain(
                    40.f * DEG_TO_RAD, pitch_deg * DEG_TO_RAD,
                    blend, 1.f, 90.f, b, 1.f, true, nullptr, 0.f,
                    /*pitch_up_scale=*/1.f);
                ensure("explicit pitch_up_scale 1 is bit-identical to the "
                           "default (blend " + std::to_string(blend) +
                           " pitch_deg " + std::to_string(pitch_deg) + ")",
                       poseBitEqual(a, b));
            }
        }

        const AnatomicalLimitProfile sym; // default: up fields == down fields
        F32 y_dn[ALGazeMath::CHAIN_SLOTS];
        F32 p_dn[ALGazeMath::CHAIN_SLOTS];
        F32 y_up[ALGazeMath::CHAIN_SLOTS];
        F32 p_up[ALGazeMath::CHAIN_SLOTS];
        ALGazeMath::fillEffectiveCapacities(sym, 1.f, 1.f, 1.f, true,
                                            y_dn, p_dn, false);
        ALGazeMath::fillEffectiveCapacities(sym, 1.f, 1.f, 1.f, true,
                                            y_up, p_up, true);
        for (S32 i = 0; i < ALGazeMath::CHAIN_SLOTS; ++i)
        {
            ensure("symmetric-profile up capacity table is bitwise the down "
                       "table, slot " + std::to_string(i),
                   bitwiseEqual(p_up[i], p_dn[i]) &&
                   bitwiseEqual(y_up[i], y_dn[i]));
        }
    }

    // 2. Legacy path, pitch_up_scale 1.3: only the UPWARD pitch changes.
    {
        // Eye-only: up cap 14 -> 18.2 deg; the downward call is bit-identical.
        AnatomicalChainPose up_plain, up_scaled, dn_plain, dn_scaled;
        distributeAnatomicalChain(0.f, -30.f * DEG_TO_RAD, 0.f, 1.f, 90.f,
                                  up_plain);
        distributeAnatomicalChain(0.f, -30.f * DEG_TO_RAD, 0.f, 1.f, 90.f,
                                  up_scaled, 1.f, true, nullptr, 0.f, 1.3f);
        distributeAnatomicalChain(0.f, 30.f * DEG_TO_RAD, 0.f, 1.f, 90.f,
                                  dn_plain);
        distributeAnatomicalChain(0.f, 30.f * DEG_TO_RAD, 0.f, 1.f, 90.f,
                                  dn_scaled, 1.f, true, nullptr, 0.f, 1.3f);
        ensure("eye-only upward cap scales to 18.2 deg",
               std::fabs(up_scaled.mEyePitch + 18.2f * DEG_TO_RAD) <= 1e-5f);
        ensure("eye-only upward reach grew",
               std::fabs(up_scaled.mEyePitch) >
                   std::fabs(up_plain.mEyePitch) + 1.f * DEG_TO_RAD);
        ensure("eye-only downward is bit-identical under the up scale",
               poseBitEqual(dn_plain, dn_scaled));

        // Full chain, beyond the symmetric ~106.5 deg pitch reach: upward
        // delivery grows; the mirrored downward call is bit-identical and
        // yaw is untouched on the upward call.
        AnatomicalChainPose deep_up_plain, deep_up_scaled;
        AnatomicalChainPose deep_dn_plain, deep_dn_scaled;
        distributeAnatomicalChain(50.f * DEG_TO_RAD, -120.f * DEG_TO_RAD,
                                  1.f, 1.f, 90.f, deep_up_plain);
        distributeAnatomicalChain(50.f * DEG_TO_RAD, -120.f * DEG_TO_RAD,
                                  1.f, 1.f, 90.f, deep_up_scaled, 1.f, true,
                                  nullptr, 0.f, 1.3f);
        distributeAnatomicalChain(50.f * DEG_TO_RAD, 120.f * DEG_TO_RAD,
                                  1.f, 1.f, 90.f, deep_dn_plain);
        distributeAnatomicalChain(50.f * DEG_TO_RAD, 120.f * DEG_TO_RAD,
                                  1.f, 1.f, 90.f, deep_dn_scaled, 1.f, true,
                                  nullptr, 0.f, 1.3f);
        ensure("deep upward delivery grows under the up scale",
               pitchMagSum(deep_up_scaled) >
                   pitchMagSum(deep_up_plain) + 5.f * DEG_TO_RAD);
        ensure("deep upward delivery reaches the full 120 deg target",
               std::fabs(pitchMagSum(deep_up_scaled) - 120.f * DEG_TO_RAD) <=
                   1e-4f);
        ensure("deep downward is bit-identical under the up scale",
               poseBitEqual(deep_dn_plain, deep_dn_scaled));
        ensure("the up scale never touches the yaw allocation",
               bitwiseEqual(deep_up_scaled.mEyeYaw, deep_up_plain.mEyeYaw) &&
               bitwiseEqual(deep_up_scaled.mHeadYaw, deep_up_plain.mHeadYaw) &&
               bitwiseEqual(deep_up_scaled.mNeckYaw, deep_up_plain.mNeckYaw) &&
               bitwiseEqual(deep_up_scaled.mTorsoYaw,
                            deep_up_plain.mTorsoYaw) &&
               bitwiseEqual(deep_up_scaled.mHipsYaw, deep_up_plain.mHipsYaw));
    }

    // 3. Profile path: explicit up cones beat the down cones only upward,
    //    and pitch_up_scale is ignored when a profile is active.
    {
        AnatomicalLimitProfile prof; // down fields at defaults
        prof.mEyePitchUpDeg   = 20.f;
        prof.mHeadPitchUpDeg  = 50.f;
        prof.mNeckPitchUpDeg  = 34.f;
        prof.mSpinePitchUpDeg = 24.f;
        prof.mHipsPitchUpDeg  = 18.f;

        AnatomicalChainPose up_pose, dn_pose, dn_sym;
        distributeAnatomicalChain(0.f, -110.f * DEG_TO_RAD, 1.f, 1.f, 90.f,
                                  up_pose, 1.f, true, &prof);
        distributeAnatomicalChain(0.f, 110.f * DEG_TO_RAD, 1.f, 1.f, 90.f,
                                  dn_pose, 1.f, true, &prof);
        const AnatomicalLimitProfile sym;
        distributeAnatomicalChain(0.f, 110.f * DEG_TO_RAD, 1.f, 1.f, 90.f,
                                  dn_sym, 1.f, true, &sym);
        ensure("profile up>down delivers more upward than downward",
               pitchMagSum(up_pose) >
                   pitchMagSum(dn_pose) + 3.f * DEG_TO_RAD);
        ensure("profile downward is untouched by the up cones",
               poseBitEqual(dn_pose, dn_sym));
        ensure("upward head share reaches the 50 deg up cone",
               std::fabs(std::fabs(up_pose.mHeadPitch) -
                         50.f * DEG_TO_RAD) <= 1e-4f);

        AnatomicalChainPose up_pose_scaled;
        distributeAnatomicalChain(0.f, -110.f * DEG_TO_RAD, 1.f, 1.f, 90.f,
                                  up_pose_scaled, 1.f, true, &prof, 0.f, 5.f);
        ensure("pitch_up_scale is ignored on the profile path",
               poseBitEqual(up_pose, up_pose_scaled));
    }
}

template<> template<>
void algazemath_test_object::test<32>()
{
    set_test_name("asymmetric up/down pitch on the secondary paths: "
                  "additiveOverlayLocal delta clamp and spineLean ellipse");
    using ALGazeMath::additiveOverlayLocal;
    using ALGazeMath::spineLean;
    using ALGazeMath::SpineLeanResult;

    auto quatBitEqual = [](const LLQuaternion& a,
                           const LLQuaternion& b) -> bool
    {
        return bitwiseEqual(a.mQ[VX], b.mQ[VX]) &&
               bitwiseEqual(a.mQ[VY], b.mQ[VY]) &&
               bitwiseEqual(a.mQ[VZ], b.mQ[VZ]) &&
               bitwiseEqual(a.mQ[VW], b.mQ[VW]);
    };
    auto leanBitEqual = [](const SpineLeanResult& a,
                           const SpineLeanResult& b) -> bool
    {
        return bitwiseEqual(a.mSpineYaw, b.mSpineYaw) &&
               bitwiseEqual(a.mSpinePitch, b.mSpinePitch) &&
               bitwiseEqual(a.mFaceYaw, b.mFaceYaw) &&
               bitwiseEqual(a.mFacePitch, b.mFacePitch);
    };
    auto pitchRot = [](F32 pitch_rad) -> LLQuaternion
    {
        LLQuaternion q;
        q.setEulerAngles(0.f, pitch_rad, 0.f);
        return q;
    };
    auto pitchOf = [](const LLQuaternion& q) -> F32
    {
        F32 roll = 0.f, pitch = 0.f, yaw = 0.f;
        q.getEulerAngles(&roll, &pitch, &yaw);
        return pitch;
    };

    const F32 cap_yaw = 40.f * DEG_TO_RAD;
    const F32 cap_dn  = 10.f * DEG_TO_RAD;
    const F32 cap_up  = 25.f * DEG_TO_RAD;
    const LLQuaternion anim; // identity animation snapshot

    // 1. additiveOverlayLocal DEFAULT PARITY: the default-argument call, the
    //    -1 sentinel, and an up cap bit-equal to the down cap are all
    //    bit-identical across both pitch signs (the symmetric else-branch is
    //    the original statement verbatim).
    {
        const F32 deltas_deg[] = { -30.f, -8.f, 0.f, 8.f, 30.f };
        for (F32 d : deltas_deg)
        {
            const LLQuaternion desired = pitchRot(d * DEG_TO_RAD);
            const LLQuaternion base = additiveOverlayLocal(
                desired, anim, cap_yaw, cap_dn, true);
            const LLQuaternion sentinel = additiveOverlayLocal(
                desired, anim, cap_yaw, cap_dn, true, -1.f, 1.f, -1.f);
            const LLQuaternion sym_up = additiveOverlayLocal(
                desired, anim, cap_yaw, cap_dn, true, -1.f, 1.f, cap_dn);
            ensure("additive overlay: -1 sentinel is bit-identical to the "
                       "default call (delta_deg " + std::to_string(d) + ")",
                   quatBitEqual(sentinel, base));
            ensure("additive overlay: up cap == down cap is bit-identical "
                       "to the default call (delta_deg " +
                       std::to_string(d) + ")",
                   quatBitEqual(sym_up, base));
        }
    }

    // 2. additiveOverlayLocal asymmetric clamp: an UPWARD (negative) delta
    //    past the down cap reaches the larger up cap; the mirrored DOWNWARD
    //    delta still clamps at the down cap, bit-identical to the symmetric
    //    call.
    {
        const LLQuaternion up_desired = pitchRot(-30.f * DEG_TO_RAD);
        const LLQuaternion up_sym = additiveOverlayLocal(
            up_desired, anim, cap_yaw, cap_dn, true);
        const LLQuaternion up_asym = additiveOverlayLocal(
            up_desired, anim, cap_yaw, cap_dn, true, -1.f, 1.f, cap_up);
        ensure("upward delta clamps at the down cap without the up cap",
               std::fabs(pitchOf(up_sym) + cap_dn) <= 1e-4f);
        ensure("upward delta reaches the up cap when supplied",
               std::fabs(pitchOf(up_asym) + cap_up) <= 1e-4f);
        ensure("upward reach grew past the down cap",
               std::fabs(pitchOf(up_asym)) >
                   std::fabs(pitchOf(up_sym)) + 5.f * DEG_TO_RAD);

        const LLQuaternion dn_desired = pitchRot(30.f * DEG_TO_RAD);
        const LLQuaternion dn_sym = additiveOverlayLocal(
            dn_desired, anim, cap_yaw, cap_dn, true);
        const LLQuaternion dn_asym = additiveOverlayLocal(
            dn_desired, anim, cap_yaw, cap_dn, true, -1.f, 1.f, cap_up);
        ensure("downward delta still clamps at the down cap",
               std::fabs(pitchOf(dn_asym) - cap_dn) <= 1e-4f);
        ensure("downward delta is bit-identical under the up cap",
               quatBitEqual(dn_asym, dn_sym));
    }

    // 3. spineLean DEFAULT PARITY: the default-argument call, the -1
    //    sentinel, and an up semi-axis bit-equal to the down semi-axis are
    //    bit-identical across both pitch signs (cap selection happens before
    //    any arithmetic).
    {
        const F32 aims_deg[] = { -60.f, -10.f, 0.f, 10.f, 60.f };
        for (F32 aim_deg : aims_deg)
        {
            const F32 aim = aim_deg * DEG_TO_RAD;
            const SpineLeanResult base = spineLean(
                20.f * DEG_TO_RAD, aim, 0.f, 0.f, 90.f, 1.f, 1.f,
                45.f * DEG_TO_RAD, 20.f * DEG_TO_RAD);
            const SpineLeanResult sentinel = spineLean(
                20.f * DEG_TO_RAD, aim, 0.f, 0.f, 90.f, 1.f, 1.f,
                45.f * DEG_TO_RAD, 20.f * DEG_TO_RAD, -1.f);
            const SpineLeanResult sym_up = spineLean(
                20.f * DEG_TO_RAD, aim, 0.f, 0.f, 90.f, 1.f, 1.f,
                45.f * DEG_TO_RAD, 20.f * DEG_TO_RAD, 20.f * DEG_TO_RAD);
            ensure("spineLean: -1 sentinel is bit-identical to the default "
                       "call (aim_deg " + std::to_string(aim_deg) + ")",
                   leanBitEqual(sentinel, base));
            ensure("spineLean: up axis == down axis is bit-identical to the "
                       "default call (aim_deg " + std::to_string(aim_deg) +
                       ")",
                   leanBitEqual(sym_up, base));
        }
    }

    // 4. spineLean asymmetric ellipse: a pure UPWARD lean past both caps
    //    reaches the up semi-axis; the mirrored DOWNWARD lean still stops at
    //    the down semi-axis, bit-identical to the symmetric call.
    {
        const F32 spine_dn = 20.f * DEG_TO_RAD;
        const F32 spine_up = 24.f * DEG_TO_RAD;
        const SpineLeanResult up_sym = spineLean(
            0.f, -60.f * DEG_TO_RAD, 0.f, 0.f, 90.f, 1.f, 1.f,
            45.f * DEG_TO_RAD, spine_dn);
        const SpineLeanResult up_asym = spineLean(
            0.f, -60.f * DEG_TO_RAD, 0.f, 0.f, 90.f, 1.f, 1.f,
            45.f * DEG_TO_RAD, spine_dn, spine_up);
        ensure("pure upward lean caps at the down axis without the up axis",
               std::fabs(up_sym.mSpinePitch + spine_dn) <= 1e-5f);
        ensure("pure upward lean reaches the up axis when supplied",
               std::fabs(up_asym.mSpinePitch + spine_up) <= 1e-5f);
        ensure("upward lean still reconstructs the aim (spine + face)",
               std::fabs(up_asym.mSpinePitch + up_asym.mFacePitch +
                         60.f * DEG_TO_RAD) <= 1e-5f);

        const SpineLeanResult dn_sym = spineLean(
            0.f, 60.f * DEG_TO_RAD, 0.f, 0.f, 90.f, 1.f, 1.f,
            45.f * DEG_TO_RAD, spine_dn);
        const SpineLeanResult dn_asym = spineLean(
            0.f, 60.f * DEG_TO_RAD, 0.f, 0.f, 90.f, 1.f, 1.f,
            45.f * DEG_TO_RAD, spine_dn, spine_up);
        ensure("downward lean still stops at the down axis",
               std::fabs(dn_asym.mSpinePitch - spine_dn) <= 1e-5f);
        ensure("downward lean is bit-identical under the up axis",
               leanBitEqual(dn_asym, dn_sym));
    }
}

} // namespace tut
