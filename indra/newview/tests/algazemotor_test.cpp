/**
 * @file algazemotor_test.cpp
 * @brief Unit tests for the ALGazeMotor assembly core (spec 6A).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "../test/lltut.h"
#include "../algazemotor.h"
#include "../algazemath.h"
#include "../algazepolicy.h"

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace tut
{
namespace
{
constexpr U64 TEST_SEED = 0x0123456789abcdefULL;

// Baseline input: master gate ON, all stochastic layers OFF (no micro-life,
// no blinks), so a test can enable exactly the layer it exercises.
ALGazeMotor::GazeMotorInput quietInput(F64 t, F32 yaw_rad, F32 pitch_rad)
{
    ALGazeMotor::GazeMotorInput in;
    in.mSettings.mMasterGate = true;
    in.mSettings.mEyeDriftAmpRad = 0.f;
    in.mSettings.mHeadDriftAmpRad = 0.f;
    in.mSettings.mMicrosaccadeAmpRad = 0.f;
    in.mSettings.mBlinkRate = 0.f;
    in.mSettings.mGazeEvokedBlink = 0.f;
    in.mTimeSeconds = t;
    in.mDeltaTime = 1.f / 60.f;
    in.mSeed = TEST_SEED;
    in.mTargetYaw = yaw_rad;
    in.mTargetPitch = pitch_rad;
    return in;
}

// Count rising 0 -> nonzero edges of a lid-closure series and record their
// sample times.
S32 countBlinkOnsets(const std::vector<F32>& closure,
                     const std::vector<F64>& times,
                     std::vector<F64>* out_onsets = nullptr)
{
    S32 count = 0;
    for (size_t i = 1; i < closure.size(); ++i)
    {
        if (closure[i - 1] <= 0.f && closure[i] > 0.f)
        {
            ++count;
            if (out_onsets)
            {
                out_onsets->push_back(times[i]);
            }
        }
    }
    return count;
}

// Bit-pattern equality: unlike `==`, this distinguishes +0.0f from -0.0f
// (spec 5's band-0 gate is BYTE-identical to distributeAnatomicalChain, not
// merely value-identical).
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
// contract, which /fp:fast limits to ~1 ULP (see test 26's comment).
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
// formula compiled in two places (profile path vs constant path) may round
// a couple ULPs apart when the optimizer contracts/reassociates the two
// shapes differently, and a capacity wobble subtracted from a target lands
// in a small residual's finer ULP scale (hence the absolute floor).
// Anything beyond this bound is a genuine arithmetic drift, not compiler
// noise.
bool softEqual(F32 a, F32 b)
{
    return ulpDistance(a, b) <= 2 ||
           std::fabs(a - b) <= 4.f * FLT_EPSILON * std::fabs(b) ||
           std::fabs(a - b) <= 8.f * FLT_EPSILON;
}
} // anonymous namespace

struct algazemotor_data
{
};

typedef test_group<algazemotor_data> algazemotor_test_group;
typedef algazemotor_test_group::object algazemotor_test_object;
algazemotor_test_group algazemotor_test("algazemotor");

template<> template<>
void algazemotor_test_object::test<1>()
{
    set_test_name("master gate off is a documented passthrough");
    ALGazeMotor::GazeMotorState state;
    ALGazeMotor::GazeMotorPose pose;
    ALGazeMotor::GazeMotorInput in = quietInput(1.0, 0.5f, 0.2f);
    in.mSettings.mMasterGate = false;
    in.mCueWeight = 0.75f;

    ALGazeMotor::step(state, in, pose);
    ensure("gate off marks the pose inactive", !pose.mActive);
    ensure("gate off leaves a neutral pose",
           std::fabs(pose.mHeadYaw) <= 1e-7f &&
           std::fabs(pose.mEyeYaw) <= 1e-7f &&
           std::fabs(pose.mLidClosureLeft) <= 1e-7f);
    ensure("cue weight still passes through",
           std::fabs(pose.mCueWeight - 0.75f) <= 1e-7f);
    ensure("gate off drops motor state", !state.mInitialized);

    // Calling repeatedly stays a no-op.
    ALGazeMotor::step(state, in, pose);
    ensure("repeat passthrough stays inactive", !pose.mActive);
    ensure("repeat passthrough stays uninitialized", !state.mInitialized);
}

template<> template<>
void algazemotor_test_object::test<2>()
{
    set_test_name("band 0 converged pose matches the legacy hard-knee chain");
    const F32 yaw = 40.f * DEG_TO_RAD;
    const F32 pitch = 20.f * DEG_TO_RAD;

    ALGazeMotor::GazeMotorState state;
    ALGazeMotor::GazeMotorPose pose;
    ALGazeMotor::GazeMotorInput in = quietInput(0.0, yaw, pitch);
    in.mSettings.mHeadEyeBlend = 0.8f;
    in.mSettings.mTorsoAmount = 0.6f;
    in.mSettings.mSoftRecruitBandDeg = 0.f;
    for (S32 i = 0; i < 10; ++i)
    {
        in.mTimeSeconds = i / 60.0;
        ALGazeMotor::step(state, in, pose);
    }
    ensure("pose is active", pose.mActive);

    ALGazeMath::AnatomicalChainPose legacy;
    ALGazeMath::distributeAnatomicalChain(yaw, pitch, 0.8f, 0.6f, 90.f, legacy);
    ensure("head yaw matches legacy",
           std::fabs(pose.mHeadYaw - legacy.mHeadYaw) <= 1e-5f);
    ensure("head pitch matches legacy",
           std::fabs(pose.mHeadPitch - legacy.mHeadPitch) <= 1e-5f);
    ensure("neck yaw matches legacy",
           std::fabs(pose.mNeckYaw - legacy.mNeckYaw) <= 1e-5f);
    ensure("neck pitch matches legacy",
           std::fabs(pose.mNeckPitch - legacy.mNeckPitch) <= 1e-5f);
    ensure("torso yaw matches legacy",
           std::fabs(pose.mTorsoYaw - legacy.mTorsoYaw) <= 1e-5f);
    ensure("torso pitch matches legacy",
           std::fabs(pose.mTorsoPitch - legacy.mTorsoPitch) <= 1e-5f);
    ensure("hips yaw matches legacy",
           std::fabs(pose.mHipsYaw - legacy.mHipsYaw) <= 1e-5f);
    ensure("hips pitch matches legacy",
           std::fabs(pose.mHipsPitch - legacy.mHipsPitch) <= 1e-5f);
    ensure("chain aim reports the full target",
           std::fabs(pose.mChainAimYaw - yaw) <= 1e-5f);
}

template<> template<>
void algazemotor_test_object::test<3>()
{
    set_test_name("eyes lead, head follows on a retarget");
    const F32 target = 30.f * DEG_TO_RAD;
    constexpr F64 DT = 1.0 / 240.0;

    ALGazeMotor::GazeMotorState state;
    ALGazeMotor::GazeMotorPose pose;
    std::vector<F64> times;
    std::vector<F32> chain_aim;
    std::vector<F32> head_yaw;
    F64 commit_time = -1.0;
    for (S32 i = 0; i <= static_cast<S32>(4.0 / DT); ++i)
    {
        const F64 t = i * DT;
        ALGazeMotor::GazeMotorInput in =
            quietInput(t, t >= 1.0 ? target : 0.f, 0.f);
        const U64 before = state.mRetargetCounter;
        ALGazeMotor::step(state, in, pose);
        if (commit_time < 0.0 && state.mRetargetCounter > before)
        {
            commit_time = t;
        }
        times.push_back(t);
        chain_aim.push_back(pose.mChainAimYaw);
        head_yaw.push_back(pose.mHeadYaw);
    }
    ensure("a retarget committed", commit_time > 0.0);
    ensure("commit obeys the 120 ms dwell",
           commit_time >= 1.12 - 1e-9 && commit_time <= 1.20);

    const F32 head_final = head_yaw.back();
    ensure("head recruited a nonzero share",
           head_final > 5.f * DEG_TO_RAD);
    const F32 reach_tol = 0.5f * DEG_TO_RAD;
    F64 eye_reach = -1.0;
    F64 head_reach = -1.0;
    size_t eye_reach_index = 0;
    for (size_t i = 0; i < times.size(); ++i)
    {
        if (times[i] <= commit_time)
        {
            continue;
        }
        if (eye_reach < 0.0 && std::fabs(chain_aim[i] - target) <= reach_tol)
        {
            eye_reach = times[i];
            eye_reach_index = i;
        }
        if (head_reach < 0.0 && std::fabs(head_yaw[i] - head_final) <= reach_tol)
        {
            head_reach = times[i];
        }
    }
    ensure("eye aim reached the target", eye_reach > 0.0);
    ensure("head reached its final share", head_reach > 0.0);
    ensure("eyes land before the head", eye_reach < head_reach);
    ensure("eye landing is main-sequence fast (<= 150 ms after commit)",
           eye_reach - commit_time <= 0.150);
    ensure("head is still early in its move when the eyes land",
           head_yaw[eye_reach_index] < 0.5f * head_final);
}

template<> template<>
void algazemotor_test_object::test<4>()
{
    set_test_name("no velocity/acceleration discontinuity across retargets "
                  "(finite diff)");
    constexpr F64 DT = 1.0 / 960.0;
    const F32 target_a = 25.f * DEG_TO_RAD;
    const F32 target_b = -10.f * DEG_TO_RAD;

    ALGazeMotor::GazeMotorState state;
    ALGazeMotor::GazeMotorPose pose;
    constexpr S32 SERIES = 6;
    std::vector<F32> p[SERIES];
    U64 commits = 0;
    // The second retarget must catch an UNSTARTED pending program so the
    // reversal exercises the abandonment path: with a 120 ms head-latency
    // base, the torso stage starts ~0.28 s after the first commit, safely
    // past the second commit (~0.2 s after it).
    bool second_commit_seen = false;
    bool torso_pending_unstarted_at_reversal = false;
    for (S32 i = 0; i <= static_cast<S32>(3.0 / DT); ++i)
    {
        const F64 t = i * DT;
        F32 target = 0.f;
        if (t >= 1.2)
        {
            target = target_b; // mid-flight reversal retarget
        }
        else if (t >= 1.0)
        {
            target = target_a;
        }
        ALGazeMotor::GazeMotorInput in = quietInput(t, target, 0.f);
        in.mSettings.mSoftRecruitBandDeg = 6.f; // C2 recruitment band
        in.mSettings.mHeadLatencyBaseMs = 120.f;
        const bool torso_unstarted =
            state.mChannels[ALGazeMotor::CH_TORSO_YAW].mPendingValid &&
            state.mChannels[ALGazeMotor::CH_TORSO_YAW].mPending.mStartTime > t;
        const U64 before = state.mRetargetCounter;
        ALGazeMotor::step(state, in, pose);
        if (!second_commit_seen && before == 1 && state.mRetargetCounter == 2)
        {
            second_commit_seen = true;
            torso_pending_unstarted_at_reversal = torso_unstarted;
        }
        commits = state.mRetargetCounter;
        p[0].push_back(pose.mChainAimYaw);
        p[1].push_back(pose.mEyeYaw);
        p[2].push_back(pose.mHeadYaw);
        p[3].push_back(pose.mNeckYaw);
        p[4].push_back(pose.mTorsoYaw);
        p[5].push_back(pose.mHipsYaw);
    }
    ensure("both retargets committed", commits == 2);
    ensure("the reversal abandoned an UNSTARTED pending torso program",
           second_commit_seen && torso_pending_unstarted_at_reversal);

    // Finite-difference velocity must change by at most a_max * dt per frame
    // pair: a genuine velocity step at a bad handoff would appear as a jump
    // of the order of the channel velocity itself (several rad/s), far above
    // these analytic acceleration bounds.
    const F32 bounds[SERIES] = { 800.f, 800.f, 300.f, 300.f, 300.f, 300.f };
    const char* names[SERIES] =
        { "chain aim", "eye-in-head", "head", "neck", "torso", "hips" };
    for (S32 c = 0; c < SERIES; ++c)
    {
        F32 worst = 0.f;
        for (size_t i = 2; i < p[c].size(); ++i)
        {
            const F32 v_prev = (p[c][i - 1] - p[c][i - 2]) / static_cast<F32>(DT);
            const F32 v_curr = (p[c][i] - p[c][i - 1]) / static_cast<F32>(DT);
            worst = llmax(worst, std::fabs(v_curr - v_prev));
        }
        const F32 allowed = bounds[c] * static_cast<F32>(DT) + 1e-3f;
        ensure(std::string(names[c]) + " velocity is continuous (worst " +
                   std::to_string(worst) + " allowed " +
                   std::to_string(allowed) + ")",
               worst <= allowed);
    }

    // Acceleration continuity (spec section 5: no acceleration discontinuity
    // at a C2 retarget): the finite-difference acceleration may change by at
    // most j_max * dt per frame pair. A C1-only handoff would show an
    // acceleration jump of the order of the channel's peak acceleration
    // itself (eye ~280 rad/s^2, head ~35 rad/s^2), well above these bounds.
    const F32 jerk_bounds[SERIES] =
        { 1.2e5f, 1.2e5f, 2.0e4f, 2.0e4f, 2.0e4f, 2.0e4f };
    for (S32 c = 0; c < SERIES; ++c)
    {
        F32 worst = 0.f;
        const F32 inv_dt2 = 1.f / static_cast<F32>(DT * DT);
        for (size_t i = 3; i < p[c].size(); ++i)
        {
            const F32 a_prev =
                (p[c][i - 1] - 2.f * p[c][i - 2] + p[c][i - 3]) * inv_dt2;
            const F32 a_curr =
                (p[c][i] - 2.f * p[c][i - 1] + p[c][i - 2]) * inv_dt2;
            worst = llmax(worst, std::fabs(a_curr - a_prev));
        }
        const F32 allowed = jerk_bounds[c] * static_cast<F32>(DT) + 2.f;
        ensure(std::string(names[c]) + " acceleration is continuous (worst " +
                   std::to_string(worst) + " allowed " +
                   std::to_string(allowed) + ")",
               worst <= allowed);
    }
}

template<> template<>
void algazemotor_test_object::test<5>()
{
    set_test_name("retarget hysteresis prevents thrash on jitter");
    constexpr F64 DT = 1.0 / 120.0;
    ALGazeMotor::GazeMotorState state;
    ALGazeMotor::GazeMotorPose pose;

    // Sub-threshold wander (+/- 2 deg) for two seconds: no commit ever.
    for (S32 i = 0; i <= static_cast<S32>(2.0 / DT); ++i)
    {
        const F64 t = i * DT;
        const F32 wander = ((i % 2) ? 2.f : -2.f) * DEG_TO_RAD;
        ALGazeMotor::GazeMotorInput in = quietInput(t, wander, 0.f);
        ALGazeMotor::step(state, in, pose);
    }
    ensure_equals("sub-threshold jitter never commits",
                  static_cast<S32>(state.mRetargetCounter), 0);

    // A single-frame 5-degree spike (shorter than the dwell) does not commit.
    {
        ALGazeMotor::GazeMotorInput in =
            quietInput(2.0 + DT, 5.f * DEG_TO_RAD, 0.f);
        ALGazeMotor::step(state, in, pose);
        in = quietInput(2.0 + 2.0 * DT, 0.f, 0.f);
        ALGazeMotor::step(state, in, pose);
    }
    ensure_equals("a spike shorter than the dwell never commits",
                  static_cast<S32>(state.mRetargetCounter), 0);

    // A sustained 15-degree move commits exactly once.
    for (S32 i = 0; i <= static_cast<S32>(1.0 / DT); ++i)
    {
        const F64 t = 2.1 + i * DT;
        ALGazeMotor::GazeMotorInput in =
            quietInput(t, 15.f * DEG_TO_RAD, 0.f);
        ALGazeMotor::step(state, in, pose);
    }
    ensure_equals("a sustained move commits exactly once",
                  static_cast<S32>(state.mRetargetCounter), 1);
}

template<> template<>
void algazemotor_test_object::test<6>()
{
    set_test_name("spontaneous blink cadence: plausible, deterministic, "
                  "refractory-spaced");
    constexpr F64 DT = 1.0 / 60.0;
    constexpr F64 SPAN = 120.0;

    auto runBlinks = [](U64 seed, std::vector<F32>& closure,
                        std::vector<F64>& times)
    {
        ALGazeMotor::GazeMotorState state;
        ALGazeMotor::GazeMotorPose pose;
        for (S32 i = 0; i <= static_cast<S32>(SPAN / DT); ++i)
        {
            const F64 t = i * DT;
            ALGazeMotor::GazeMotorInput in = quietInput(t, 0.f, 0.f);
            in.mSeed = seed;
            in.mSettings.mBlinkRate = 1.f;
            ALGazeMotor::step(state, in, pose);
            closure.push_back(pose.mLidClosureLeft);
            times.push_back(t);
        }
    };

    std::vector<F32> run_a, run_b, run_c;
    std::vector<F64> times_a, times_b, times_c;
    runBlinks(TEST_SEED, run_a, times_a);
    runBlinks(TEST_SEED, run_b, times_b);
    runBlinks(TEST_SEED ^ 0xdeadbeefULL, run_c, times_c);

    std::vector<F64> onsets;
    const S32 count = countBlinkOnsets(run_a, times_a, &onsets);
    ensure("blink rate is plausible (10..60 blinks in 120 s)",
           count >= 10 && count <= 60);
    for (size_t i = 1; i < onsets.size(); ++i)
    {
        ensure("consecutive onsets respect the refractory floor",
               onsets[i] - onsets[i - 1] >= 0.4);
    }

    ensure("same seed and times give a bit-identical schedule",
           run_a == run_b);
    ensure("a different seed gives a different schedule", run_a != run_c);
}

template<> template<>
void algazemotor_test_object::test<7>()
{
    set_test_name("gaze-evoked blinks: fire on large retargets, respect "
                  "refractory");
    constexpr F64 DT = 1.0 / 240.0;
    const F32 big = 40.f * DEG_TO_RAD;

    ALGazeMotor::GazeMotorState state;
    ALGazeMotor::GazeMotorPose pose;
    std::vector<F32> closure;
    std::vector<F64> times;
    for (S32 i = 0; i <= static_cast<S32>(8.0 / DT); ++i)
    {
        const F64 t = i * DT;
        F32 target = 0.f;
        if (t >= 4.0)
        {
            target = big;      // third move: past refractory, fires again
        }
        else if (t >= 1.5)
        {
            target = 0.f;      // second move: inside refractory, suppressed
        }
        else if (t >= 1.0)
        {
            target = big;      // first move: evokes a blink
        }
        ALGazeMotor::GazeMotorInput in = quietInput(t, target, 0.f);
        in.mSettings.mBlinkRate = 0.f;      // isolate the evoked source
        in.mSettings.mGazeEvokedBlink = 1.f;
        ALGazeMotor::step(state, in, pose);
        closure.push_back(pose.mLidClosureLeft);
        times.push_back(t);
    }
    ensure_equals("all three retargets committed",
                  static_cast<S32>(state.mRetargetCounter), 3);

    std::vector<F64> onsets;
    const S32 count = countBlinkOnsets(closure, times, &onsets);
    ensure_equals("exactly two evoked blinks (middle one suppressed)",
                  count, 2);
    ensure("first evoked blink lands at the first commit",
           onsets[0] >= 1.12 && onsets[0] <= 1.20);
    ensure("second evoked blink lands at the third commit",
           onsets[1] >= 4.12 && onsets[1] <= 4.20);

    F32 peak = 0.f;
    for (size_t i = 0; i < times.size(); ++i)
    {
        if (times[i] >= onsets[0] && times[i] <= onsets[0] + 0.3)
        {
            peak = llmax(peak, closure[i]);
        }
    }
    ensure("evoked blink reaches a full-depth peak", peak >= 0.9f);
}

template<> template<>
void algazemotor_test_object::test<8>()
{
    set_test_name("micro-life is deterministic and arousal-scaled");
    constexpr F64 DT = 1.0 / 120.0;

    auto runMicro = [](F32 arousal, std::vector<F32>& eye_yaw)
    {
        ALGazeMotor::GazeMotorState state;
        ALGazeMotor::GazeMotorPose pose;
        for (S32 i = 0; i <= static_cast<S32>(2.0 / DT); ++i)
        {
            const F64 t = i * DT;
            ALGazeMotor::GazeMotorInput in =
                quietInput(t, 10.f * DEG_TO_RAD, 0.f);
            // Re-enable the micro layers this test exercises.
            in.mSettings.mEyeDriftAmpRad = 0.15f * DEG_TO_RAD;
            in.mSettings.mHeadDriftAmpRad = 0.5f * DEG_TO_RAD;
            in.mSettings.mMicrosaccadeAmpRad = 0.12f * DEG_TO_RAD;
            in.mAffect.mArousal = arousal;
            ALGazeMotor::step(state, in, pose);
            eye_yaw.push_back(pose.mEyeYaw);
        }
    };

    std::vector<F32> run_a, run_b, low, high;
    runMicro(0.5f, run_a);
    runMicro(0.5f, run_b);
    ensure("same seed and times give bit-identical micro-life",
           run_a == run_b);

    runMicro(0.f, low);
    runMicro(1.f, high);
    auto rms = [](const std::vector<F32>& series) -> F32
    {
        F64 mean = 0.0;
        for (F32 v : series) { mean += v; }
        mean /= series.size();
        F64 acc = 0.0;
        for (F32 v : series)
        {
            const F64 d = v - mean;
            acc += d * d;
        }
        return static_cast<F32>(std::sqrt(acc / series.size()));
    };
    const F32 rms_low = rms(low);
    const F32 rms_high = rms(high);
    ensure("micro-life is present", rms_high > 1e-5f);
    ensure("arousal scales micro-life amplitude up",
           rms_high > 2.f * rms_low);
}

template<> template<>
void algazemotor_test_object::test<9>()
{
    set_test_name("affect aperture posture: droop and widen");
    constexpr F64 DT = 1.0 / 60.0;

    auto lidAfter = [](const ALGazeMotor::AffectState& affect) -> F32
    {
        ALGazeMotor::GazeMotorState state;
        ALGazeMotor::GazeMotorPose pose;
        for (S32 i = 0; i < 5; ++i)
        {
            ALGazeMotor::GazeMotorInput in = quietInput(i * DT, 0.f, 0.f);
            in.mAffect = affect;
            ALGazeMotor::step(state, in, pose);
        }
        return pose.mLidClosureLeft;
    };

    ALGazeMotor::AffectState neutral;
    ensure("neutral affect keeps lids at rest",
           std::fabs(lidAfter(neutral)) <= 1e-6f);

    ALGazeMotor::AffectState sad;
    sad.mValence = -1.f;
    sad.mArousal = 0.1f;
    const F32 sad_lid = lidAfter(sad);
    ensure("low valence + low energy droops the lids", sad_lid > 0.25f);

    ALGazeMotor::AffectState agitated;
    agitated.mValence = -1.f;
    agitated.mArousal = 1.f;
    const F32 agitated_lid = lidAfter(agitated);
    ensure("high arousal widens against the droop", agitated_lid < sad_lid);

    ALGazeMotor::GazeMotorState state;
    ALGazeMotor::GazeMotorPose pose;
    ALGazeMotor::GazeMotorInput in = quietInput(0.0, 0.f, 0.f);
    in.mAffect = agitated;
    ALGazeMotor::step(state, in, pose);
    ensure("aperture widen intent is surfaced", pose.mApertureWiden >= 0.99f);
}

template<> template<>
void algazemotor_test_object::test<10>()
{
    set_test_name("affect tempo: arousal shortens head settle time");
    constexpr F64 DT = 1.0 / 240.0;
    const F32 target = 30.f * DEG_TO_RAD;

    auto headSettleTime = [target](F32 arousal) -> F64
    {
        ALGazeMotor::GazeMotorState state;
        ALGazeMotor::GazeMotorPose pose;
        std::vector<F64> times;
        std::vector<F32> head;
        for (S32 i = 0; i <= static_cast<S32>(4.0 / DT); ++i)
        {
            const F64 t = i * DT;
            ALGazeMotor::GazeMotorInput in =
                quietInput(t, t >= 1.0 ? target : 0.f, 0.f);
            in.mAffect.mArousal = arousal;
            ALGazeMotor::step(state, in, pose);
            times.push_back(t);
            head.push_back(pose.mHeadYaw);
        }
        const F32 final_value = head.back();
        const F32 tol = 0.05f * DEG_TO_RAD;
        for (size_t i = 0; i < head.size(); ++i)
        {
            if (times[i] > 1.2 && std::fabs(head[i] - final_value) <= tol)
            {
                return times[i];
            }
        }
        return -1.0;
    };

    const F64 settle_low = headSettleTime(0.f);
    const F64 settle_high = headSettleTime(1.f);
    ensure("low-arousal run settles", settle_low > 0.0);
    ensure("high-arousal run settles", settle_high > 0.0);
    ensure("high arousal settles the head sooner", settle_high < settle_low);
}

template<> template<>
void algazemotor_test_object::test<11>()
{
    set_test_name("VOR: eye-in-head counter-rotates as the head turns in");
    constexpr F64 DT = 1.0 / 60.0;
    const F32 target = 30.f * DEG_TO_RAD;

    ALGazeMotor::GazeMotorState state;
    ALGazeMotor::GazeMotorPose pose;
    ALGazeMotor::GazeMotorInput in = quietInput(0.0, target, 0.f);
    for (S32 i = 0; i < 5; ++i)
    {
        in.mTimeSeconds = i * DT;
        ALGazeMotor::step(state, in, pose);
    }
    const F32 comfort = in.mSettings.mComfortYawDeg;
    const F32 expected_identity =
        ALGazePolicy::softClampAngle(target, comfort * DEG_TO_RAD);
    ensure("head-at-identity eye yaw matches the soft-clamped solve",
           std::fabs(pose.mEyeYaw - expected_identity) <= 1e-3f);

    // Rotate the head 15 degrees toward the target: the eye-in-head demand
    // drops to the residual 15 degrees, solved fresh from geometry.
    in.mHeadWorldRot = LLQuaternion(15.f * DEG_TO_RAD, LLVector3(0.f, 0.f, 1.f));
    in.mTimeSeconds = 6 * DT;
    ALGazeMotor::step(state, in, pose);
    const F32 expected_rotated = ALGazePolicy::softClampAngle(
        15.f * DEG_TO_RAD, comfort * DEG_TO_RAD);
    ensure("eye-in-head counter-rotates to the residual angle",
           std::fabs(pose.mEyeYaw - expected_rotated) <= 1e-3f);
    ensure("counter-rotation reduced the eye deflection",
           pose.mEyeYaw < expected_identity);
}

template<> template<>
void algazemotor_test_object::test<12>()
{
    set_test_name("gate off then on re-initializes cleanly");
    constexpr F64 DT = 1.0 / 60.0;
    ALGazeMotor::GazeMotorState state;
    ALGazeMotor::GazeMotorPose pose;

    for (S32 i = 0; i < 10; ++i)
    {
        ALGazeMotor::GazeMotorInput in =
            quietInput(i * DT, 20.f * DEG_TO_RAD, 0.f);
        ALGazeMotor::step(state, in, pose);
    }
    ensure("running pose is active", pose.mActive);

    ALGazeMotor::GazeMotorInput off = quietInput(10 * DT, 20.f * DEG_TO_RAD, 0.f);
    off.mSettings.mMasterGate = false;
    ALGazeMotor::step(state, off, pose);
    ensure("gate off deactivates", !pose.mActive && !state.mInitialized);

    ALGazeMotor::GazeMotorInput back =
        quietInput(11 * DT, -15.f * DEG_TO_RAD, 5.f * DEG_TO_RAD);
    ALGazeMotor::step(state, back, pose);
    ensure("re-enable reactivates", pose.mActive && state.mInitialized);
    ensure("re-enable holds the current target (no swing from zero)",
           std::fabs(pose.mChainAimYaw - (-15.f * DEG_TO_RAD)) <= 1e-5f &&
           std::fabs(pose.mChainAimPitch - 5.f * DEG_TO_RAD) <= 1e-5f);
    ensure("re-enabled pose is finite",
           std::isfinite(pose.mHeadYaw) && std::isfinite(pose.mEyeYaw) &&
           std::isfinite(pose.mLidClosureLeft));
}

template<> template<>
void algazemotor_test_object::test<13>()
{
    set_test_name("recruited channels stay continuous across the +/-pi seam");
    // A retarget sweeping +170 deg -> -170 deg travels the SHORT way through
    // the +/-pi seam (aim 170 -> 190 continuous). With anatomy scale 3 the
    // neck slot is mid-recruitment through the whole sweep (active range
    // ~123.8..228.8 deg), so it tracks the aim as it crosses the seam; a
    // wrap before the signed allocator would flip its sign (+56 -> -56 deg)
    // and flip the saturated head slot (+105 -> -105 deg) -- the exact
    // failure this test guards.
    constexpr F64 DT = 1.0 / 960.0;
    const F32 yaw_a = 170.f * DEG_TO_RAD;
    const F32 yaw_b = -170.f * DEG_TO_RAD;

    ALGazeMotor::GazeMotorState state;
    ALGazeMotor::GazeMotorPose pose;
    constexpr S32 SERIES = 4;
    std::vector<F32> p[SERIES];
    for (S32 i = 0; i <= static_cast<S32>(2.5 / DT); ++i)
    {
        const F64 t = i * DT;
        ALGazeMotor::GazeMotorInput in =
            quietInput(t, t >= 0.5 ? yaw_b : yaw_a, 0.f);
        in.mSettings.mAnatomyScale = 3.f;
        in.mSettings.mSoftRecruitBandDeg = 0.f; // legacy hard knee
        ALGazeMotor::step(state, in, pose);
        p[0].push_back(pose.mHeadYaw);
        p[1].push_back(pose.mNeckYaw);
        p[2].push_back(pose.mTorsoYaw);
        p[3].push_back(pose.mHipsYaw);
    }
    ensure_equals("the seam-crossing retarget committed",
                  static_cast<S32>(state.mRetargetCounter), 1);
    ensure("chain aim report ends wrapped at -170 deg",
           std::fabs(pose.mChainAimYaw - yaw_b) <= 1e-4f);

    // No sign flip: the head slot stays saturated positive, the neck slot
    // sweeps continuously up from ~46 deg to ~66 deg.
    const F32 head_final = p[0].back();
    const F32 neck_final = p[1].back();
    ensure("head slot never flips sign at the seam",
           head_final > 100.f * DEG_TO_RAD);
    ensure("neck slot recruits continuously through the seam",
           neck_final > 60.f * DEG_TO_RAD &&
           neck_final < 72.f * DEG_TO_RAD);
    F32 neck_min = p[1][0];
    for (F32 v : p[1]) { neck_min = llmin(neck_min, v); }
    ensure("neck slot stays positive throughout", neck_min > 40.f * DEG_TO_RAD);

    // Finite-difference position AND velocity continuity for every
    // recruited channel across the seam. A wrap-induced flip is a ~2 rad
    // position step in one frame -- orders beyond these bounds.
    const char* names[SERIES] = { "head", "neck", "torso", "hips" };
    for (S32 c = 0; c < SERIES; ++c)
    {
        F32 worst_dp = 0.f;
        F32 worst_dv = 0.f;
        for (size_t i = 2; i < p[c].size(); ++i)
        {
            const F32 v_prev = (p[c][i - 1] - p[c][i - 2]) / static_cast<F32>(DT);
            const F32 v_curr = (p[c][i] - p[c][i - 1]) / static_cast<F32>(DT);
            worst_dp = llmax(worst_dp, std::fabs(p[c][i] - p[c][i - 1]));
            worst_dv = llmax(worst_dv, std::fabs(v_curr - v_prev));
        }
        // Positions move at most v_max * dt (~3 rad/s * dt); velocity jumps
        // at most a_max * dt (~300 rad/s^2 * dt).
        ensure(std::string(names[c]) + " position is continuous at the seam "
                   "(worst " + std::to_string(worst_dp) + ")",
               worst_dp <= 0.01f);
        ensure(std::string(names[c]) + " velocity is continuous at the seam "
                   "(worst " + std::to_string(worst_dv) + ")",
               worst_dv <= 300.f * static_cast<F32>(DT) + 1e-3f);
    }
}

template<> template<>
void algazemotor_test_object::test<14>()
{
    set_test_name("band-0 recruitment is bit-identical to "
                  "distributeAnatomicalChain");
    const F32 blends[] = { 0.f, 0.001f, 0.3f, 0.8f, 1.f };
    const F32 scales[] = { 1.f, 2.f };
    const F32 torsos[] = { 0.6f, 1.f };
    const F32 angles_deg[] =
        { 0.f, 3.f, -7.5f, 12.f, -25.f, 40.f, -60.f, 90.f,
          -120.f, 150.f, -179.f };
    constexpr S32 NANGLES = sizeof(angles_deg) / sizeof(angles_deg[0]);

    for (F32 blend : blends)
    {
        for (F32 scale : scales)
        {
            for (F32 torso : torsos)
            {
                ALGazeMotor::GazeMotorSettings s;
                s.mHeadEyeBlend = blend;
                s.mTorsoAmount  = torso;
                s.mAnatomyScale = scale;
                F32 cy[ALGazeMotor::CHAIN_JOINTS];
                F32 cp[ALGazeMotor::CHAIN_JOINTS];
                ALGazeMotor::effectiveCapacities(s, cy, cp);

                for (S32 ai = 0; ai < NANGLES; ++ai)
                {
                    const F32 yaw = angles_deg[ai] * DEG_TO_RAD;
                    const F32 pitch =
                        angles_deg[(ai + 3) % NANGLES] * DEG_TO_RAD;
                    ALGazeMath::AnatomicalChainPose legacy;
                    ALGazeMath::distributeAnatomicalChain(
                        yaw, pitch, blend, torso, 90.f, legacy, scale);

                    F32 jy[ALGazeMotor::CHAIN_JOINTS];
                    F32 jp[ALGazeMotor::CHAIN_JOINTS];
                    for (S32 slot = 0; slot < ALGazeMotor::CHAIN_JOINTS;
                         ++slot)
                    {
                        jy[slot] = ALGazeMotor::recruitSlot(yaw, cy, 0.f, slot);
                        jp[slot] =
                            ALGazeMotor::recruitSlot(pitch, cp, 0.f, slot);
                    }
                    const bool yaw_exact =
                        jy[0] == legacy.mEyeYaw &&
                        jy[1] == legacy.mHeadYaw &&
                        jy[2] == legacy.mNeckYaw &&
                        jy[3] == legacy.mTorsoYaw &&
                        jy[4] == legacy.mHipsYaw;
                    const bool pitch_exact =
                        jp[0] == legacy.mEyePitch &&
                        jp[1] == legacy.mHeadPitch &&
                        jp[2] == legacy.mNeckPitch &&
                        jp[3] == legacy.mTorsoPitch &&
                        jp[4] == legacy.mHipsPitch;
                    ensure("band-0 yaw allocation bit-identical (blend " +
                               std::to_string(blend) + " scale " +
                               std::to_string(scale) + " torso " +
                               std::to_string(torso) + " deg " +
                               std::to_string(angles_deg[ai]) + ")",
                           yaw_exact);
                    ensure("band-0 pitch allocation bit-identical (blend " +
                               std::to_string(blend) + " scale " +
                               std::to_string(scale) + " torso " +
                               std::to_string(torso) + ")",
                           pitch_exact);

                    // Fix 2: `==` cannot distinguish +0.0f from -0.0f, so
                    // the checks above would silently pass even if the
                    // motor's eye-only branch emitted -0.0f on a downstream
                    // slot where legacy's early return (algazemath.h:
                    // 620-625, which never computes sign * 0 for those
                    // fields) left a default-constructed +0.0f. Assert
                    // BYTEWISE identity for the negative-target eye-only
                    // case specifically, where the motor's `sign * amount`
                    // (sign == -1, amount == 0) is the one place -0.0f could
                    // have leaked through.
                    if (blend <= 0.001f)
                    {
                        const std::string dbg =
                            " (blend " + std::to_string(blend) + " scale " +
                            std::to_string(scale) + " torso " +
                            std::to_string(torso) + " yaw_deg " +
                            std::to_string(angles_deg[ai]) + " pitch_deg " +
                            std::to_string(angles_deg[(ai + 3) % NANGLES]) +
                            ")";
                        if (yaw < 0.f)
                        {
                            ensure("band-0 eye-only head yaw is bytewise "
                                       "+0.0, not -0.0" + dbg,
                                   bitwiseEqual(jy[1], legacy.mHeadYaw));
                            ensure("band-0 eye-only neck yaw is bytewise "
                                       "+0.0, not -0.0" + dbg,
                                   bitwiseEqual(jy[2], legacy.mNeckYaw));
                            ensure("band-0 eye-only torso yaw is bytewise "
                                       "+0.0, not -0.0" + dbg,
                                   bitwiseEqual(jy[3], legacy.mTorsoYaw));
                            ensure("band-0 eye-only hips yaw is bytewise "
                                       "+0.0, not -0.0" + dbg,
                                   bitwiseEqual(jy[4], legacy.mHipsYaw));
                        }
                        const F32 pitch_val = angles_deg[(ai + 3) % NANGLES];
                        if (pitch_val < 0.f)
                        {
                            ensure("band-0 eye-only head pitch is bytewise "
                                       "+0.0, not -0.0" + dbg,
                                   bitwiseEqual(jp[1], legacy.mHeadPitch));
                            ensure("band-0 eye-only neck pitch is bytewise "
                                       "+0.0, not -0.0" + dbg,
                                   bitwiseEqual(jp[2], legacy.mNeckPitch));
                            ensure("band-0 eye-only torso pitch is bytewise "
                                       "+0.0, not -0.0" + dbg,
                                   bitwiseEqual(jp[3], legacy.mTorsoPitch));
                            ensure("band-0 eye-only hips pitch is bytewise "
                                       "+0.0, not -0.0" + dbg,
                                   bitwiseEqual(jp[4], legacy.mHipsPitch));
                        }
                    }
                }
            }
        }
    }
}

template<> template<>
void algazemotor_test_object::test<15>()
{
    set_test_name("evoked blink is deterministic given the commit time + "
                  "seed, across frame cadences");
    // Both runs share the exact frames t = 1.0 (candidate) and t = 1.13
    // (commit: first frame past the 120 ms dwell) but use DIFFERENT frame
    // cadences before and after -- the evoked blink must depend only on the
    // commit time + seed, so both runs produce a bit-identical blink.
    const F32 big = 40.f * DEG_TO_RAD;

    auto runEvoked = [big](S32 hz, std::vector<F64>& times_out,
                           std::vector<F32>& closure_out) -> F64
    {
        ALGazeMotor::GazeMotorState state;
        ALGazeMotor::GazeMotorPose pose;
        std::vector<F64> times;
        for (S32 i = 0; static_cast<F64>(i) / hz < 1.0; ++i)
        {
            times.push_back(static_cast<F64>(i) / hz);
        }
        times.push_back(1.0);
        times.push_back(1.13);
        for (S32 i = 1; i <= 2 * hz; ++i)
        {
            times.push_back(1.13 + static_cast<F64>(i) / hz);
        }
        for (F64 t : times)
        {
            ALGazeMotor::GazeMotorInput in =
                quietInput(t, t >= 1.0 ? big : 0.f, 0.f);
            in.mSettings.mGazeEvokedBlink = 1.f;
            ALGazeMotor::step(state, in, pose);
            times_out.push_back(t);
            closure_out.push_back(pose.mLidClosureLeft);
        }
        return state.mEvokedBlinkStart;
    };

    std::vector<F64> times_a, times_b;
    std::vector<F32> run_a, run_b;
    const F64 evoked_a = runEvoked(60, times_a, run_a);
    const F64 evoked_b = runEvoked(240, times_b, run_b);
    ensure("run A evoked onset is exactly the commit time", evoked_a == 1.13);
    ensure("run B evoked onset is exactly the commit time", evoked_b == 1.13);

    // Compare closures at every shared timestamp (60 Hz grid is a subset of
    // the 240 Hz grid; the times are built by division so equal reals give
    // bit-equal F64s).
    size_t bi = 0;
    S32 compared = 0;
    F32 peak = 0.f;
    for (size_t ai = 0; ai < times_a.size(); ++ai)
    {
        while (bi < times_b.size() && times_b[bi] < times_a[ai])
        {
            ++bi;
        }
        if (bi < times_b.size() && times_b[bi] == times_a[ai])
        {
            ensure("closure bit-identical at shared time " +
                       std::to_string(times_a[ai]),
                   run_a[ai] == run_b[bi]);
            ++compared;
            peak = llmax(peak, run_a[ai]);
        }
    }
    ensure("shared timestamps were actually compared", compared > 100);
    ensure("the evoked blink actually fired", peak >= 0.9f);
}

template<> template<>
void algazemotor_test_object::test<16>()
{
    set_test_name("configured refractory > 0.5 s is enforced between "
                  "spontaneous onsets; evoked arbitration is symmetric");
    // Part A: a 1.5 s configured refractory (3x the intrinsic 0.5 s floor)
    // must hold between EVERY pair of spontaneous onsets.
    constexpr F64 DT = 1.0 / 120.0;
    constexpr F64 SPAN = 120.0;
    {
        ALGazeMotor::GazeMotorState state;
        ALGazeMotor::GazeMotorPose pose;
        std::vector<F32> closure;
        std::vector<F64> times;
        for (S32 i = 0; i <= static_cast<S32>(SPAN / DT); ++i)
        {
            const F64 t = i * DT;
            ALGazeMotor::GazeMotorInput in = quietInput(t, 0.f, 0.f);
            in.mSettings.mBlinkRate = 2.f;
            in.mSettings.mBlinkRefractorySec = 1.5f;
            ALGazeMotor::step(state, in, pose);
            closure.push_back(pose.mLidClosureLeft);
            times.push_back(t);
        }
        std::vector<F64> onsets;
        const S32 count = countBlinkOnsets(closure, times, &onsets);
        ensure("blink cadence still plausible under the wide refractory "
                   "(got " + std::to_string(count) + ")",
               count >= 30 && count <= 60);
        for (size_t i = 1; i < onsets.size(); ++i)
        {
            ensure("no two spontaneous onsets closer than the configured "
                       "refractory (gap " +
                       std::to_string(onsets[i] - onsets[i - 1]) + ")",
                   onsets[i] - onsets[i - 1] >= 1.5 - 2.0 * DT);
        }
    }

    // Part B: evoked-after-spontaneous honors blink duration + refractory.
    // Locate a spontaneous onset closed-form (the same lattice the motor
    // uses: rate_eff 1 -> period 4.5, min_gap 0.5), pick one with a wide
    // following gap, and commit a large retarget near it.
    const F64 period = 4.5;
    const F64 min_gap = 0.5;
    const F64 total_plus_refractory = 0.25 + 0.5; // 85+0+165 ms + 0.5 s
    S64 pick = -1;
    for (S64 cell = 1; cell < 40; ++cell)
    {
        const F64 on = ALGazeMotor::detail::spontOnsetForCell(
            TEST_SEED, cell, period, min_gap);
        const F64 next = ALGazeMotor::detail::spontOnsetForCell(
            TEST_SEED, cell + 1, period, min_gap);
        if (next - on >= 1.6)
        {
            pick = cell;
            break;
        }
    }
    ensure("found a spontaneous onset with a wide following gap", pick > 0);
    const F64 onset = ALGazeMotor::detail::spontOnsetForCell(
        TEST_SEED, pick, period, min_gap);

    auto makeInput = [](F64 t, F32 yaw) -> ALGazeMotor::GazeMotorInput
    {
        ALGazeMotor::GazeMotorInput in = quietInput(t, yaw, 0.f);
        in.mSettings.mBlinkRate = 1.f;       // spontaneous lattice active
        in.mSettings.mGazeEvokedBlink = 1.f; // 40 deg shift -> probability 1
        return in;
    };

    // Inside the spontaneous blink's envelope + refractory: refused.
    {
        ALGazeMotor::GazeMotorState state;
        ALGazeMotor::GazeMotorInput init = makeInput(onset - 2.0, 0.f);
        ALGazeMotor::detail::initializeState(state, init, onset - 2.0);
        const F64 now = onset + 0.1; // < total + refractory after the onset
        ALGazeMotor::GazeMotorInput in = makeInput(now, 40.f * DEG_TO_RAD);
        ALGazeMotor::detail::commitRetarget(state, in, now);
        ensure("evoked onset refused inside spontaneous blink + refractory",
               state.mEvokedBlinkStart < -1.0e17);
    }
    // Past the envelope + refractory (and clear of the next onset): fires.
    {
        ALGazeMotor::GazeMotorState state;
        ALGazeMotor::GazeMotorInput init = makeInput(onset - 2.0, 0.f);
        ALGazeMotor::detail::initializeState(state, init, onset - 2.0);
        const F64 now = onset + total_plus_refractory + 0.05;
        ALGazeMotor::GazeMotorInput in = makeInput(now, 40.f * DEG_TO_RAD);
        ALGazeMotor::detail::commitRetarget(state, in, now);
        ensure("evoked onset fires once clear of the spontaneous blink",
               state.mEvokedBlinkStart == now);
    }
}

template<> template<>
void algazemotor_test_object::test<17>()
{
    set_test_name("zero head latency still staggers neck after head, torso "
                  "after neck");
    constexpr F64 DT = 1.0 / 120.0;
    ALGazeMotor::GazeMotorState state;
    ALGazeMotor::GazeMotorPose pose;
    bool checked = false;
    for (S32 i = 0; i <= static_cast<S32>(1.0 / DT) && !checked; ++i)
    {
        const F64 t = i * DT;
        ALGazeMotor::GazeMotorInput in =
            quietInput(t, t >= 0.3 ? 30.f * DEG_TO_RAD : 0.f, 0.f);
        // Force the head-latency policy to its 0 ms floor.
        in.mSettings.mHeadLatencyBaseMs = 0.f;
        in.mSettings.mPredictability01 = 1.f;
        in.mSettings.mHeadMoverTrait01 = 1.f;
        const U64 before = state.mRetargetCounter;
        ALGazeMotor::step(state, in, pose);
        if (state.mRetargetCounter > before)
        {
            // Right after the committing step: head started immediately
            // (its program is active at the commit time), neck and torso
            // are pending strictly later, in order.
            const ALGazeMotor::ChannelState& head =
                state.mChannels[ALGazeMotor::CH_HEAD_YAW];
            const ALGazeMotor::ChannelState& neck =
                state.mChannels[ALGazeMotor::CH_NECK_YAW];
            const ALGazeMotor::ChannelState& torso =
                state.mChannels[ALGazeMotor::CH_TORSO_YAW];
            ensure("zero-latency head starts at the commit",
                   !head.mPendingValid && head.mProgram.mStartTime == t);
            ensure("neck program is pending (starts later)",
                   neck.mPendingValid);
            ensure("torso program is pending (starts later)",
                   torso.mPendingValid);
            const F64 head_start = head.mProgram.mStartTime;
            const F64 neck_start = neck.mPending.mStartTime;
            const F64 torso_start = torso.mPending.mStartTime;
            ensure("neck starts strictly after the head",
                   neck_start > head_start);
            ensure("torso starts strictly after the neck",
                   torso_start > neck_start);
            ensure("the stagger honors the absolute minimum",
                   neck_start - head_start >=
                       ALGazeMotor::MIN_STAGGER_SEC - 1e-9 &&
                   torso_start - neck_start >=
                       ALGazeMotor::MIN_STAGGER_SEC - 1e-9);
            checked = true;
        }
    }
    ensure("a retarget committed", checked);
}

template<> template<>
void algazemotor_test_object::test<18>()
{
    set_test_name("spontaneous blinks are cadence-independent (closed-form)");
    // Sample the same 30 s window at 60 Hz and 240 Hz; the closure at every
    // SHARED timestamp must be bit-identical (times built by division, so
    // equal reals give bit-equal F64s). This is the closed-form/scrub-safe
    // property of the spontaneous path, checked across cadences rather than
    // by replaying one sequence.
    auto runAt = [](S32 hz, std::vector<F64>& times,
                    std::vector<F32>& closure)
    {
        ALGazeMotor::GazeMotorState state;
        ALGazeMotor::GazeMotorPose pose;
        for (S32 i = 0; i <= 30 * hz; ++i)
        {
            const F64 t = static_cast<F64>(i) / hz;
            ALGazeMotor::GazeMotorInput in = quietInput(t, 0.f, 0.f);
            in.mSettings.mBlinkRate = 1.f;
            ALGazeMotor::step(state, in, pose);
            times.push_back(t);
            closure.push_back(pose.mLidClosureLeft);
        }
    };
    std::vector<F64> times_a, times_b;
    std::vector<F32> run_a, run_b;
    runAt(60, times_a, run_a);
    runAt(240, times_b, run_b);

    S32 mismatches = 0;
    S32 compared = 0;
    size_t bi = 0;
    for (size_t ai = 0; ai < times_a.size(); ++ai)
    {
        while (bi < times_b.size() && times_b[bi] < times_a[ai])
        {
            ++bi;
        }
        if (bi < times_b.size() && times_b[bi] == times_a[ai])
        {
            ++compared;
            if (run_a[ai] != run_b[bi])
            {
                ++mismatches;
            }
        }
    }
    ensure_equals("every 60 Hz sample found its 240 Hz twin",
                  compared, static_cast<S32>(times_a.size()));
    ensure_equals("spontaneous closure identical across cadences",
                  mismatches, 0);
    ensure("blinks actually occurred",
           countBlinkOnsets(run_a, times_a) >= 3);
}

template<> template<>
void algazemotor_test_object::test<19>()
{
    set_test_name("microsaccade rate is clamped to the sub-tremor bound");
    constexpr F64 DT = 1.0 / 120.0;
    auto runRate = [](F32 rate_hz, std::vector<F32>& eye_yaw)
    {
        ALGazeMotor::GazeMotorState state;
        ALGazeMotor::GazeMotorPose pose;
        for (S32 i = 0; i <= static_cast<S32>(3.0 / DT); ++i)
        {
            ALGazeMotor::GazeMotorInput in =
                quietInput(i * DT, 10.f * DEG_TO_RAD, 0.f);
            in.mSettings.mMicrosaccadeAmpRad = 0.12f * DEG_TO_RAD;
            in.mSettings.mMicrosaccadeRateHz = rate_hz;
            ALGazeMotor::step(state, in, pose);
            eye_yaw.push_back(pose.mEyeYaw);
        }
    };
    std::vector<F32> at_100hz, at_bound, at_3hz;
    runRate(100.f, at_100hz);
    runRate(ALGazeMotor::MICROSACCADE_MAX_RATE_HZ, at_bound);
    runRate(3.f, at_3hz);
    ensure("a 100 Hz request is clamped to the bound (identical output)",
           at_100hz == at_bound);
    ensure("rates below the bound still differ (the clamp is a clamp, not "
           "a constant)", at_3hz != at_bound);
}

template<> template<>
void algazemotor_test_object::test<20>()
{
    set_test_name("handoff derivatives are clamped onto the provable C2 path");
    // A channel program with absurd derivatives (far beyond anything the
    // motor builds -- e.g. a zero-ms authored duration) must hand off with
    // clamped (v, a) so solveScalar provably avoids its residual C0 escape.
    ALGazeMotor::ChannelState ch;
    ch.mProgram = ALTrajectory::solveScalar(
        0.0, 1.0, 0.f, 1.0e6f, 5.0e7f, 100.f, 1.0e6f, 0.f,
        false /*monotonic*/, 0, ALGazeMotor::BEHAVIOR_VERSION);
    ALGazeMotor::detail::retargetChannel(ch, 0.0, 0.0, 0.2, 0.5f, 1);
    const ALTrajectory::ScalarSegment& seg = ch.mProgram.mSegments[0];
    ensure("handoff velocity clamped",
           std::fabs(seg.mV0) <= ALGazeMotor::HANDOFF_MAX_VEL_RAD_SEC + 1e-3f);
    ensure("handoff acceleration clamped",
           std::fabs(seg.mA0) <=
               ALGazeMotor::HANDOFF_MAX_ACC_RAD_SEC2 + 1.f);
    // The residual C0 escape builds a zero-duration brake; with clamped
    // handoff derivatives it must be unreachable even when the brake-then-go
    // fallback fires.
    ensure("no C0 escape: any brake stage has a real duration",
           !ch.mProgram.mBrakeFallback ||
           ch.mProgram.mSegments[0].mDuration > 0.0);
    const ALTrajectory::ScalarSample mid = ALTrajectory::sample(
        ch.mProgram, 0.1);
    ensure("clamped retarget samples finite",
           std::isfinite(mid.mP) && std::isfinite(mid.mV) &&
           std::isfinite(mid.mA));

    // Ordinary handoffs are far below the clamps: they pass through intact.
    ALGazeMotor::ChannelState ordinary;
    ordinary.mProgram = ALTrajectory::solveScalar(
        0.0, 0.1, 0.f, 5.f, 20.f, 0.4f, 0.f, 0.f,
        false /*monotonic*/, 0, ALGazeMotor::BEHAVIOR_VERSION);
    const ALTrajectory::ScalarSample at_start =
        ALTrajectory::sample(ordinary.mProgram, 0.0);
    ALGazeMotor::detail::retargetChannel(ordinary, 0.0, 0.0, 0.3, 0.6f, 2);
    ensure("ordinary handoff velocity is untouched by the clamp",
           ordinary.mProgram.mSegments[0].mV0 == at_start.mV);
}

template<> template<>
void algazemotor_test_object::test<21>()
{
    set_test_name("eye-only mode (blend <= 0.001) routes to the exact "
                  "allocator regardless of band: downstream joints stay "
                  "exactly zero with no sign discontinuity across zero aim");
    // Eye-only capacities give every downstream (head/neck/torso/hips) joint
    // EXACTLY zero capacity (effectiveCapacities, algazemotor.h ~429). Before
    // the fix, a band > 0 routed those joints through
    // ALGazeRecruit::recruitChain, whose smoothExcess(residual, capacity,
    // band) evaluates a nonzero mid-band value at the residual == capacity
    // == 0 knee -- a spurious, wrong-signed downstream contribution at zero
    // aim (and near it) even though there is no chain left to soft-recruit
    // into. The fix (recruitSlot's `eye_only` routing) must make every
    // downstream joint EXACTLY 0.0 for every band and every magnitude,
    // across a sweep that straddles zero.
    const F32 bands_deg[] = { 0.f, 3.f, 8.f };
    const F32 mags_deg[] =
        { -20.f, -5.f, -1.f, -0.01f, 0.f, 0.01f, 1.f, 5.f, 20.f };
    constexpr S32 NMAGS = sizeof(mags_deg) / sizeof(mags_deg[0]);

    for (F32 band_deg : bands_deg)
    {
        F32 prev_head_yaw = 0.f;
        for (S32 mi = 0; mi < NMAGS; ++mi)
        {
            const F32 yaw = mags_deg[mi] * DEG_TO_RAD;
            const F32 pitch = mags_deg[(mi + 2) % NMAGS] * DEG_TO_RAD;

            ALGazeMotor::GazeMotorState state;
            ALGazeMotor::GazeMotorPose pose;
            ALGazeMotor::GazeMotorInput in = quietInput(0.0, yaw, pitch);
            in.mSettings.mHeadEyeBlend = 0.f;      // eye-only
            in.mSettings.mSoftRecruitBandDeg = band_deg;
            ALGazeMotor::step(state, in, pose);

            const std::string tag = " (band " + std::to_string(band_deg) +
                " deg, mag " + std::to_string(mags_deg[mi]) + " deg)";
            ensure("pose is active" + tag, pose.mActive);
            ensure_equals(("head yaw exactly zero" + tag).c_str(),
                          pose.mHeadYaw, 0.f);
            ensure_equals(("head pitch exactly zero" + tag).c_str(),
                          pose.mHeadPitch, 0.f);
            ensure_equals(("neck yaw exactly zero" + tag).c_str(),
                          pose.mNeckYaw, 0.f);
            ensure_equals(("neck pitch exactly zero" + tag).c_str(),
                          pose.mNeckPitch, 0.f);
            ensure_equals(("torso yaw exactly zero" + tag).c_str(),
                          pose.mTorsoYaw, 0.f);
            ensure_equals(("torso pitch exactly zero" + tag).c_str(),
                          pose.mTorsoPitch, 0.f);
            ensure_equals(("hips yaw exactly zero" + tag).c_str(),
                          pose.mHipsYaw, 0.f);
            ensure_equals(("hips pitch exactly zero" + tag).c_str(),
                          pose.mHipsPitch, 0.f);
            ensure("chain aim reports the full eye-group target" + tag,
                   std::fabs(pose.mChainAimYaw - yaw) <= 1e-6f);

            // No sign discontinuity across zero: since every downstream
            // joint is pinned at exactly 0.0 on both sides of zero (and at
            // zero itself), the finite-difference "jump" between adjacent
            // magnitude samples is exactly 0.0 too -- the strongest
            // possible form of continuity.
            ensure("no jump in head yaw between adjacent magnitude samples"
                       + tag,
                   pose.mHeadYaw == prev_head_yaw);
            prev_head_yaw = pose.mHeadYaw;
        }
    }
}

template<> template<>
void algazemotor_test_object::test<22>()
{
    set_test_name("activation edge acquires the current target immediately "
                  "(no dwell) and begins aiming within a frame or two");
    constexpr F64 DT = 1.0 / 120.0;
    const F32 target = 30.f * DEG_TO_RAD;

    ALGazeMotor::GazeMotorState state;
    ALGazeMotor::GazeMotorPose pose;

    // Held on the initial (straight-ahead) target: committed == 0, no commits.
    for (S32 i = 0; i < 5; ++i)
    {
        ALGazeMotor::GazeMotorInput in = quietInput(i * DT, 0.f, 0.f);
        ALGazeMotor::step(state, in, pose);
    }
    ensure_equals("no commit while held on the initial target",
                  static_cast<S32>(state.mRetargetCounter), 0);

    // Activation edge: a new method target appears WITH the acquire flag. The
    // commit must land on THIS frame -- not 120 ms later.
    const F64 t_edge = 5 * DT;
    ALGazeMotor::GazeMotorInput edge = quietInput(t_edge, target, 0.f);
    edge.mAcquire = true;
    const U64 before = state.mRetargetCounter;
    ALGazeMotor::step(state, edge, pose);
    ensure_equals("acquire commits on the activation frame (no dwell)",
                  static_cast<S32>(state.mRetargetCounter),
                  static_cast<S32>(before) + 1);
    // At the commit instant the eye program has only just started from the old
    // aim (C2 handoff), so the chain aim is still near zero here.
    const F32 aim_at_commit = pose.mChainAimYaw;

    // The edge is consumed (generation unchanged, flag cleared): no further
    // acquire, and the eye/chain aim sweeps to the target at main-sequence
    // speed -- reaching it within ~150 ms of the activation frame, the
    // legacy-instant feel, instead of only starting to move 120 ms later.
    const F32 reach_tol = 0.5f * DEG_TO_RAD;
    F64 reach_time = -1.0;
    F32 prev_aim = aim_at_commit;
    bool advanced_early = false;
    for (S32 i = 1; i <= static_cast<S32>(0.2 / DT); ++i)
    {
        const F64 t = t_edge + i * DT;
        ALGazeMotor::GazeMotorInput in = quietInput(t, target, 0.f);
        ALGazeMotor::step(state, in, pose);
        if (i <= 3 && pose.mChainAimYaw > prev_aim)
        {
            advanced_early = true;
        }
        prev_aim = pose.mChainAimYaw;
        if (reach_time < 0.0 &&
            std::fabs(pose.mChainAimYaw - target) <= reach_tol)
        {
            reach_time = t;
        }
    }
    ensure_equals("no spurious second commit after the edge",
                  static_cast<S32>(state.mRetargetCounter),
                  static_cast<S32>(before) + 1);
    ensure("eye/chain aim begins advancing within the first few frames",
           advanced_early && prev_aim > aim_at_commit);
    ensure("eye acquires the target within ~150 ms of activation (no dwell)",
           reach_time > 0.0 && reach_time - t_edge <= 0.150);
}

template<> template<>
void algazemotor_test_object::test<23>()
{
    set_test_name("acquire bypasses the 120 ms dwell that unsignaled motion "
                  "of the same size still honors");
    constexpr F64 DT = 1.0 / 240.0;
    const F32 target = 20.f * DEG_TO_RAD;

    auto firstCommitTime = [&](bool use_acquire) -> F64
    {
        ALGazeMotor::GazeMotorState state;
        ALGazeMotor::GazeMotorPose pose;
        F64 commit_t = -1.0;
        for (S32 i = 0; i <= static_cast<S32>(1.0 / DT); ++i)
        {
            const F64 t = i * DT;
            const bool moved = t >= 0.25;
            ALGazeMotor::GazeMotorInput in =
                quietInput(t, moved ? target : 0.f, 0.f);
            // One-shot edge on the first moved frame, mirroring the
            // integration's single-frame method-switch signal.
            if (moved && use_acquire && commit_t < 0.0)
            {
                in.mAcquire = true;
            }
            const U64 before = state.mRetargetCounter;
            ALGazeMotor::step(state, in, pose);
            if (commit_t < 0.0 && state.mRetargetCounter > before)
            {
                commit_t = t;
            }
        }
        return commit_t;
    };

    const F64 acquire_commit = firstCommitTime(true);
    const F64 dwell_commit   = firstCommitTime(false);
    ensure("acquire committed", acquire_commit > 0.0);
    ensure("unsignaled motion committed", dwell_commit > 0.0);
    ensure("acquire commits on the first moved frame (~0.25 s, no dwell)",
           acquire_commit <= 0.25 + 2.0 * DT);
    ensure("unsignaled motion still waits the full 120 ms dwell",
           dwell_commit >= 0.25 + 0.12 - 1e-9);
}

template<> template<>
void algazemotor_test_object::test<24>()
{
    set_test_name("acquire never thrashes: a no-op re-selection and "
                  "sub-threshold wander under a held flag never commit");
    constexpr F64 DT = 1.0 / 120.0;

    // (a) Re-selecting the SAME direction (acquire set, target == committed)
    //     must not bump the retarget counter (acquire dead-band).
    {
        ALGazeMotor::GazeMotorState state;
        ALGazeMotor::GazeMotorPose pose;
        for (S32 i = 0; i < 5; ++i)
        {
            ALGazeMotor::GazeMotorInput in =
                quietInput(i * DT, 10.f * DEG_TO_RAD, 0.f);
            ALGazeMotor::step(state, in, pose);
        }
        ensure_equals("held target: no commit yet",
                      static_cast<S32>(state.mRetargetCounter), 0);
        ALGazeMotor::GazeMotorInput edge =
            quietInput(5 * DT, 10.f * DEG_TO_RAD, 0.f);
        edge.mAcquire = true;   // same direction re-selected
        ALGazeMotor::step(state, edge, pose);
        ensure_equals("acquire on an unchanged target is a no-op",
                      static_cast<S32>(state.mRetargetCounter), 0);
    }

    // (b) A held acquire flag with only sub-epsilon wander must not thrash:
    //     the acquire dead-band ignores negligible motion frame after frame.
    {
        ALGazeMotor::GazeMotorState state;
        ALGazeMotor::GazeMotorPose pose;
        ALGazeMotor::GazeMotorInput seed0 = quietInput(0.0, 0.f, 0.f);
        ALGazeMotor::step(state, seed0, pose);
        for (S32 i = 1; i <= static_cast<S32>(1.0 / DT); ++i)
        {
            const F32 wander = ((i % 2) ? 0.01f : -0.01f) * DEG_TO_RAD;
            ALGazeMotor::GazeMotorInput in = quietInput(i * DT, wander, 0.f);
            in.mAcquire = true;   // pathological: flag stuck on
            ALGazeMotor::step(state, in, pose);
        }
        ensure_equals("held acquire + sub-epsilon wander never commits",
                      static_cast<S32>(state.mRetargetCounter), 0);
    }
}

template<> template<>
void algazemotor_test_object::test<25>()
{
    set_test_name("a changed target generation acts as an implicit acquire; "
                  "an unchanged generation keeps the dwell");
    constexpr F64 DT = 1.0 / 240.0;
    const F32 target = 25.f * DEG_TO_RAD;

    // Implicit acquire via a generation change (no explicit mAcquire flag).
    {
        ALGazeMotor::GazeMotorState state;
        ALGazeMotor::GazeMotorPose pose;
        for (S32 i = 0; i < 5; ++i)
        {
            ALGazeMotor::GazeMotorInput in = quietInput(i * DT, 0.f, 0.f);
            in.mTargetGeneration = 1;
            ALGazeMotor::step(state, in, pose);
        }
        ensure_equals("no commit while the generation is stable",
                      static_cast<S32>(state.mRetargetCounter), 0);

        const F64 t_edge = 5 * DT;
        ALGazeMotor::GazeMotorInput edge = quietInput(t_edge, target, 0.f);
        edge.mTargetGeneration = 2;   // method switch, no flag
        const U64 before = state.mRetargetCounter;
        ALGazeMotor::step(state, edge, pose);
        ensure_equals("a generation change commits immediately (no dwell)",
                      static_cast<S32>(state.mRetargetCounter),
                      static_cast<S32>(before) + 1);
    }

    // A constant generation with a large step still honors the dwell (the
    // pre-fix behavior for ordinary, unsignaled target motion).
    {
        ALGazeMotor::GazeMotorState state;
        ALGazeMotor::GazeMotorPose pose;
        F64 commit_t = -1.0;
        for (S32 i = 0; i <= static_cast<S32>(0.6 / DT); ++i)
        {
            const F64 t = i * DT;
            ALGazeMotor::GazeMotorInput in =
                quietInput(t, t >= 0.2 ? target : 0.f, 0.f);
            in.mTargetGeneration = 7;   // never changes
            const U64 before = state.mRetargetCounter;
            ALGazeMotor::step(state, in, pose);
            if (commit_t < 0.0 && state.mRetargetCounter > before)
            {
                commit_t = t;
            }
        }
        ensure("unsignaled motion still commits", commit_t > 0.0);
        ensure("unsignaled motion waits the full dwell",
               commit_t >= 0.2 + 0.12 - 1e-9);
    }
}

template<> template<>
void algazemotor_test_object::test<26>()
{
    set_test_name("mUseLimitProfile with a default profile matches the "
                  "constant capacity path within rounding");
    // The shared-helper contract here is SOFT: enabling mUseLimitProfile
    // with a DEFAULT-constructed profile reproduces the constant path's
    // capacity table only to ~1 ULP, because the profile path
    // (fillEffectiveCapacities) and the constant path are two
    // source-identical RESTATEMENTS of the same arithmetic and this build
    // compiles with /fp:fast (00-Common.cmake), under which the optimizer
    // may contract or reassociate the two shapes differently (observed:
    // the head pitch cap at blend 0.3, 0x3e61307a vs 0x3e61307b, and a
    // 2-ULP eye yaw cap at blend 0.8; the deviating side even moves between
    // builds as inlining shifts). The HARD bit-exact contract is
    // mUseLimitProfile == false vs the legacy chain (the band-0 grid test
    // above keeps that strict). Capacities and allocated joints are pinned
    // to softEqual (a couple ULPs, plus a small absolute floor for the
    // small-residual slots where a cap wobble is amplified in the slot's
    // finer ULP scale). Anything beyond that is a real capacity-table
    // drift.
    const F32 blends[] = { 0.f, 0.001f, 0.3f, 0.8f, 1.f };
    const F32 scales[] = { 1.f, 2.f };
    const F32 torsos[] = { 0.6f, 1.f };
    const bool plants[] = { false, true };
    const F32 angles_deg[] =
        { 0.f, 3.f, -7.5f, 12.f, -25.f, 40.f, -60.f, 90.f,
          -120.f, 150.f, -179.f };
    constexpr S32 NANGLES = sizeof(angles_deg) / sizeof(angles_deg[0]);

    for (F32 blend : blends)
    {
        for (F32 scale : scales)
        {
            for (F32 torso : torsos)
            {
                for (bool plant : plants)
                {
                    ALGazeMotor::GazeMotorSettings s_const;
                    s_const.mHeadEyeBlend = blend;
                    s_const.mTorsoAmount  = torso;
                    s_const.mAnatomyScale = scale;
                    s_const.mPlantPelvis  = plant;
                    s_const.mUseLimitProfile = false;
                    ALGazeMotor::GazeMotorSettings s_prof = s_const;
                    s_prof.mUseLimitProfile = true; // default mLimitProfile

                    F32 cy_const[ALGazeMotor::CHAIN_JOINTS];
                    F32 cp_const[ALGazeMotor::CHAIN_JOINTS];
                    F32 cy_prof[ALGazeMotor::CHAIN_JOINTS];
                    F32 cp_prof[ALGazeMotor::CHAIN_JOINTS];
                    ALGazeMotor::effectiveCapacities(s_const, cy_const,
                                                     cp_const);
                    ALGazeMotor::effectiveCapacities(s_prof, cy_prof,
                                                     cp_prof);

                    const std::string tag =
                        " (blend " + std::to_string(blend) + " scale " +
                        std::to_string(scale) + " torso " +
                        std::to_string(torso) + " plant " +
                        std::to_string(plant) + ")";
                    for (S32 slot = 0; slot < ALGazeMotor::CHAIN_JOINTS;
                         ++slot)
                    {
                        ensure("default-profile yaw capacity within "
                                   "rounding, slot " + std::to_string(slot) +
                                   " (profile " + bitsOf(cy_prof[slot]) +
                                   " constant " + bitsOf(cy_const[slot]) +
                                   ")" + tag,
                               softEqual(cy_prof[slot], cy_const[slot]));
                        ensure("default-profile pitch capacity within "
                                   "rounding, slot " +
                                   std::to_string(slot) +
                                   " (profile " + bitsOf(cp_prof[slot]) +
                                   " constant " + bitsOf(cp_const[slot]) +
                                   ")" + tag,
                               softEqual(cp_prof[slot], cp_const[slot]));
                    }

                    // Band-0 allocation from those caps stays within
                    // accumulated rounding across the signed angle grid
                    // (incl. the seam values); see softEqual's comment.
                    for (S32 ai = 0; ai < NANGLES; ++ai)
                    {
                        const F32 yaw = angles_deg[ai] * DEG_TO_RAD;
                        const F32 pitch =
                            angles_deg[(ai + 3) % NANGLES] * DEG_TO_RAD;
                        const bool eye_only =
                            ALGazeMotor::isEyeOnlyBlend(s_const);
                        for (S32 slot = 0;
                             slot < ALGazeMotor::CHAIN_JOINTS; ++slot)
                        {
                            ensure("default-profile band-0 yaw allocation "
                                       "within rounding, slot " +
                                       std::to_string(slot) + " deg " +
                                       std::to_string(angles_deg[ai]) + tag,
                                   softEqual(
                                       ALGazeMotor::recruitSlot(
                                           yaw, cy_prof, 0.f, slot,
                                           eye_only),
                                       ALGazeMotor::recruitSlot(
                                           yaw, cy_const, 0.f, slot,
                                           eye_only)));
                            ensure("default-profile band-0 pitch allocation "
                                       "within rounding, slot " +
                                       std::to_string(slot) + tag,
                                   softEqual(
                                       ALGazeMotor::recruitSlot(
                                           pitch, cp_prof, 0.f, slot,
                                           eye_only),
                                       ALGazeMotor::recruitSlot(
                                           pitch, cp_const, 0.f, slot,
                                           eye_only)));
                        }
                    }
                }
            }
        }
    }
}

template<> template<>
void algazemotor_test_object::test<27>()
{
    set_test_name("motor chest split conserves the recruited torso bucket "
                  "bit-exactly");
    // With mChestShare in {0, 0.55, 1}, the converged pose must satisfy
    // mTorsoYaw + mChestYaw == the mTorsoYaw of the share-0 run, BIT-equal
    // (same for pitch). Share 0 leaves mChest* at exactly +0; share 1 hands
    // the whole bucket to the chest.
    auto convergedPose = [](F32 share, F32 yaw,
                            F32 pitch) -> ALGazeMotor::GazeMotorPose
    {
        ALGazeMotor::GazeMotorState state;
        ALGazeMotor::GazeMotorPose pose;
        for (S32 i = 0; i < 10; ++i)
        {
            ALGazeMotor::GazeMotorInput in =
                quietInput(i / 60.0, yaw, pitch);
            in.mSettings.mHeadEyeBlend = 1.f;
            in.mSettings.mTorsoAmount  = 1.f;
            in.mSettings.mChestShare   = share;
            ALGazeMotor::step(state, in, pose);
        }
        return pose;
    };

    const F32 signs[] = { 1.f, -1.f };
    for (F32 sign : signs)
    {
        // Deep enough that the torso slot is recruited nonzero on both axes
        // (yaw needs > ~76.25 deg, pitch > ~71.5 deg cumulative upstream).
        const F32 yaw = sign * 120.f * DEG_TO_RAD;
        const F32 pitch = sign * 80.f * DEG_TO_RAD;
        const std::string tag = " (sign " + std::to_string(sign) + ")";

        const ALGazeMotor::GazeMotorPose base =
            convergedPose(0.f, yaw, pitch);
        ensure("baseline recruits a nonzero torso bucket" + tag,
               base.mTorsoYaw != 0.f && base.mTorsoPitch != 0.f);
        ensure("share 0 leaves chest at bitwise +0" + tag,
               bitwiseEqual(base.mChestYaw, 0.f) &&
               bitwiseEqual(base.mChestPitch, 0.f) &&
               !std::signbit(base.mChestYaw) &&
               !std::signbit(base.mChestPitch));

        const F32 shares[] = { 0.55f, 1.f };
        for (F32 share : shares)
        {
            const ALGazeMotor::GazeMotorPose split =
                convergedPose(share, yaw, pitch);
            const std::string stag =
                " (share " + std::to_string(share) + " sign " +
                std::to_string(sign) + ")";
            if (share == 1.f)
            {
                // Share 1 is genuinely bit-exact: the whole bucket moves
                // (torso - torso == +0 exactly, chest == the old torso).
                ensure("motor torso+chest yaw conserves the share-0 torso "
                           "bit-exactly" + stag,
                       bitwiseEqual(split.mTorsoYaw + split.mChestYaw,
                                    base.mTorsoYaw));
                ensure("motor torso+chest pitch conserves the share-0 torso "
                           "bit-exactly" + stag,
                       bitwiseEqual(split.mTorsoPitch + split.mChestPitch,
                                    base.mTorsoPitch));
            }
            else
            {
                // Intermediate shares: chest = old * share and torso =
                // old - chest are each correctly rounded, so the recombined
                // sum may sit ~1 ulp off the share-0 torso -- algebraic,
                // not bit, conservation.
                ensure("motor torso+chest yaw conserves the share-0 torso "
                           "within rounding" + stag,
                       std::fabs((split.mTorsoYaw + split.mChestYaw) -
                                 base.mTorsoYaw) <= 1e-6f);
                ensure("motor torso+chest pitch conserves the share-0 torso "
                           "within rounding" + stag,
                       std::fabs((split.mTorsoPitch + split.mChestPitch) -
                                 base.mTorsoPitch) <= 1e-6f);
            }
            // The split never leaks into any other recruited joint.
            ensure("chest split leaves the rest of the pose bit-identical"
                       + stag,
                   bitwiseEqual(split.mEyeYaw, base.mEyeYaw) &&
                   bitwiseEqual(split.mEyePitch, base.mEyePitch) &&
                   bitwiseEqual(split.mHeadYaw, base.mHeadYaw) &&
                   bitwiseEqual(split.mHeadPitch, base.mHeadPitch) &&
                   bitwiseEqual(split.mNeckYaw, base.mNeckYaw) &&
                   bitwiseEqual(split.mNeckPitch, base.mNeckPitch) &&
                   bitwiseEqual(split.mHipsYaw, base.mHipsYaw) &&
                   bitwiseEqual(split.mHipsPitch, base.mHipsPitch));
            if (share == 1.f)
            {
                ensure("share 1 zeroes the motor torso exactly" + stag,
                       bitwiseEqual(split.mTorsoYaw, 0.f) &&
                       bitwiseEqual(split.mTorsoPitch, 0.f));
                ensure("share 1 hands the whole bucket to the chest" + stag,
                       bitwiseEqual(split.mChestYaw, base.mTorsoYaw) &&
                       bitwiseEqual(split.mChestPitch, base.mTorsoPitch));
            }
        }
    }
}

template<> template<>
void algazemotor_test_object::test<29>()
{
    set_test_name("angle-ease lean: LEGACY curve is bit-identical; planted "
                  "angle-ease matches spineLean + face-only recruit; below "
                  "threshold behaves face-only");

    // (1) LEGACY curve (mLeanCurve == 0) with every new lean field set to
    // aggressive non-default values must be BIT-identical to a run that
    // never touches them: the default enters NONE of the new math, even
    // under Planted spine and across a retarget.
    {
        ALGazeMotor::GazeMotorState state_a;
        ALGazeMotor::GazeMotorState state_b;
        ALGazeMotor::GazeMotorPose pose_a;
        ALGazeMotor::GazeMotorPose pose_b;
        for (S32 i = 0; i < 240; ++i)
        {
            const F64 t = i / 60.0;
            const F32 yaw = (i < 120 ? 40.f : -70.f) * DEG_TO_RAD;
            const F32 pitch = (i < 120 ? 15.f : -10.f) * DEG_TO_RAD;
            ALGazeMotor::GazeMotorInput in_a = quietInput(t, yaw, pitch);
            in_a.mSettings.mHeadEyeBlend = 0.8f;
            in_a.mSettings.mTorsoAmount  = 0.7f;
            in_a.mSettings.mPlantPelvis  = true; // planted, but LEGACY curve
            ALGazeMotor::GazeMotorInput in_b = in_a;
            in_b.mSettings.mLeanCurve        = 0; // LEGACY, explicit
            in_b.mSettings.mLeanThresholdDeg = 5.f;
            in_b.mSettings.mLeanSoftnessDeg  = 0.f;
            in_b.mSettings.mLeanMaxDeg       = 45.f;
            in_b.mSettings.mSpineCapYawRad   = 0.1f;
            in_b.mSettings.mSpineCapPitchRad = 0.1f;
            ALGazeMotor::step(state_a, in_a, pose_a);
            ALGazeMotor::step(state_b, in_b, pose_b);
            const std::string f = " (frame " + std::to_string(i) + ")";
            ensure("legacy-curve head yaw bit-identical" + f,
                   bitwiseEqual(pose_a.mHeadYaw, pose_b.mHeadYaw));
            ensure("legacy-curve head pitch bit-identical" + f,
                   bitwiseEqual(pose_a.mHeadPitch, pose_b.mHeadPitch));
            ensure("legacy-curve neck yaw bit-identical" + f,
                   bitwiseEqual(pose_a.mNeckYaw, pose_b.mNeckYaw));
            ensure("legacy-curve neck pitch bit-identical" + f,
                   bitwiseEqual(pose_a.mNeckPitch, pose_b.mNeckPitch));
            ensure("legacy-curve torso yaw bit-identical" + f,
                   bitwiseEqual(pose_a.mTorsoYaw, pose_b.mTorsoYaw));
            ensure("legacy-curve torso pitch bit-identical" + f,
                   bitwiseEqual(pose_a.mTorsoPitch, pose_b.mTorsoPitch));
            ensure("legacy-curve chest yaw bit-identical" + f,
                   bitwiseEqual(pose_a.mChestYaw, pose_b.mChestYaw));
            ensure("legacy-curve chest pitch bit-identical" + f,
                   bitwiseEqual(pose_a.mChestPitch, pose_b.mChestPitch));
            ensure("legacy-curve hips yaw bit-identical" + f,
                   bitwiseEqual(pose_a.mHipsYaw, pose_b.mHipsYaw));
            ensure("legacy-curve hips pitch bit-identical" + f,
                   bitwiseEqual(pose_a.mHipsPitch, pose_b.mHipsPitch));
            ensure("legacy-curve eye yaw bit-identical" + f,
                   bitwiseEqual(pose_a.mEyeYaw, pose_b.mEyeYaw));
            ensure("legacy-curve eye pitch bit-identical" + f,
                   bitwiseEqual(pose_a.mEyePitch, pose_b.mEyePitch));
        }
    }

    // (2) Planted + ANGLE_EASE, converged on a large aim: torso+chest carry
    // exactly spineLean's spine vector (split by chest share), head/neck
    // carry the recruited FACE residual, hips are bitwise +0, and the eyes
    // are unaffected (identical to a LEGACY-curve run: they keep fixating
    // the FULL target, never the face residual).
    {
        const F32 yaw = 60.f * DEG_TO_RAD;
        const F32 pitch = 10.f * DEG_TO_RAD;
        auto lean_input = [&](F64 t)
        {
            ALGazeMotor::GazeMotorInput in = quietInput(t, yaw, pitch);
            in.mSettings.mHeadEyeBlend = 1.f;
            in.mSettings.mTorsoAmount  = 1.f;
            in.mSettings.mPlantPelvis  = true;
            in.mSettings.mChestShare   = 0.5f;
            in.mSettings.mLeanCurve        = 1; // ANGLE_EASE
            in.mSettings.mLeanThresholdDeg = 10.f;
            in.mSettings.mLeanSoftnessDeg  = 10.f;
            in.mSettings.mLeanMaxDeg       = 20.f;
            return in;
        };
        ALGazeMotor::GazeMotorState state;
        ALGazeMotor::GazeMotorState state_legacy;
        ALGazeMotor::GazeMotorPose pose;
        ALGazeMotor::GazeMotorPose pose_legacy;
        for (S32 i = 0; i < 30; ++i)
        {
            ALGazeMotor::GazeMotorInput in = lean_input(i / 60.0);
            ALGazeMotor::GazeMotorInput in_legacy = in;
            in_legacy.mSettings.mLeanCurve = 0;
            ALGazeMotor::step(state, in, pose);
            ALGazeMotor::step(state_legacy, in_legacy, pose_legacy);
        }
        const ALGazeMotor::GazeMotorInput ref = lean_input(0.0);
        const ALGazeMath::SpineLeanResult lean = ALGazeMath::spineLean(
            yaw, pitch, 10.f, 10.f, 20.f, 1.f, 1.f,
            ref.mSettings.mSpineCapYawRad, ref.mSettings.mSpineCapPitchRad);
        ensure("lean recruited a nonzero spine",
               std::fabs(lean.mSpineYaw) > 0.01f);
        ensure("converged torso+chest yaw == spineLean spine yaw",
               std::fabs((pose.mTorsoYaw + pose.mChestYaw) - lean.mSpineYaw)
                   <= 1e-5f);
        ensure("converged torso+chest pitch == spineLean spine pitch",
               std::fabs((pose.mTorsoPitch + pose.mChestPitch) -
                         lean.mSpinePitch) <= 1e-5f);
        ensure("chest carries the configured share of the spine",
               std::fabs(pose.mChestYaw - lean.mSpineYaw * 0.5f) <= 1e-5f);
        F32 cy[ALGazeMotor::CHAIN_JOINTS];
        F32 cp[ALGazeMotor::CHAIN_JOINTS];
        ALGazeMotor::effectiveCapacities(ref.mSettings, cy, cp);
        ensure("head yaw is the recruited FACE residual",
               std::fabs(pose.mHeadYaw - ALGazeMotor::recruitSlot(
                             lean.mFaceYaw, cy, 0.f, 1)) <= 1e-5f);
        ensure("head pitch is the recruited FACE residual",
               std::fabs(pose.mHeadPitch - ALGazeMotor::recruitSlot(
                             lean.mFacePitch, cp, 0.f, 1)) <= 1e-5f);
        ensure("neck yaw is the recruited FACE residual",
               std::fabs(pose.mNeckYaw - ALGazeMotor::recruitSlot(
                             lean.mFaceYaw, cy, 0.f, 2)) <= 1e-5f);
        ensure("neck pitch is the recruited FACE residual",
               std::fabs(pose.mNeckPitch - ALGazeMotor::recruitSlot(
                             lean.mFacePitch, cp, 0.f, 2)) <= 1e-5f);
        ensure("planted lean hips yaw is bitwise +0 " + bitsOf(pose.mHipsYaw),
               bitwiseEqual(pose.mHipsYaw, 0.f));
        ensure("planted lean hips pitch is bitwise +0 " +
                   bitsOf(pose.mHipsPitch),
               bitwiseEqual(pose.mHipsPitch, 0.f));
        ensure("eyes keep aiming at the FULL target (yaw as legacy)",
               bitwiseEqual(pose.mEyeYaw, pose_legacy.mEyeYaw));
        ensure("eyes keep aiming at the FULL target (pitch as legacy)",
               bitwiseEqual(pose.mEyePitch, pose_legacy.mEyePitch));
        ensure("chain aim still reports the full target",
               std::fabs(pose.mChainAimYaw - yaw) <= 1e-5f);
    }

    // (3) Below the threshold the spine share is exactly zero and the whole
    // pose behaves face-only: bit-identical to the LEGACY-curve run (the
    // face residual IS the full aim, and legacy's torso allocation at this
    // small angle is zero anyway).
    {
        const F32 yaw = 5.f * DEG_TO_RAD;
        const F32 pitch = 2.f * DEG_TO_RAD;
        ALGazeMotor::GazeMotorState state;
        ALGazeMotor::GazeMotorState state_legacy;
        ALGazeMotor::GazeMotorPose pose;
        ALGazeMotor::GazeMotorPose pose_legacy;
        for (S32 i = 0; i < 30; ++i)
        {
            ALGazeMotor::GazeMotorInput in = quietInput(i / 60.0, yaw, pitch);
            in.mSettings.mHeadEyeBlend = 1.f;
            in.mSettings.mTorsoAmount  = 1.f;
            in.mSettings.mPlantPelvis  = true;
            in.mSettings.mChestShare   = 0.5f;
            in.mSettings.mLeanCurve        = 1; // ANGLE_EASE
            in.mSettings.mLeanThresholdDeg = 10.f;
            in.mSettings.mLeanSoftnessDeg  = 10.f;
            in.mSettings.mLeanMaxDeg       = 20.f;
            ALGazeMotor::GazeMotorInput in_legacy = in;
            in_legacy.mSettings.mLeanCurve = 0;
            ALGazeMotor::step(state, in, pose);
            ALGazeMotor::step(state_legacy, in_legacy, pose_legacy);
        }
        ensure("below threshold: torso yaw exactly +0 " +
                   bitsOf(pose.mTorsoYaw),
               bitwiseEqual(pose.mTorsoYaw, 0.f));
        ensure("below threshold: torso pitch exactly +0 " +
                   bitsOf(pose.mTorsoPitch),
               bitwiseEqual(pose.mTorsoPitch, 0.f));
        ensure("below threshold: chest yaw exactly +0 " +
                   bitsOf(pose.mChestYaw),
               bitwiseEqual(pose.mChestYaw, 0.f));
        ensure("below threshold: chest pitch exactly +0 " +
                   bitsOf(pose.mChestPitch),
               bitwiseEqual(pose.mChestPitch, 0.f));
        ensure("below threshold: head yaw behaves face-only (as legacy)",
               bitwiseEqual(pose.mHeadYaw, pose_legacy.mHeadYaw));
        ensure("below threshold: head pitch behaves face-only (as legacy)",
               bitwiseEqual(pose.mHeadPitch, pose_legacy.mHeadPitch));
        ensure("below threshold: neck yaw behaves face-only (as legacy)",
               bitwiseEqual(pose.mNeckYaw, pose_legacy.mNeckYaw));
        ensure("below threshold: hips stay bitwise +0",
               bitwiseEqual(pose.mHipsYaw, 0.f) &&
                   bitwiseEqual(pose.mHipsPitch, 0.f));
        ensure("below threshold: eyes as legacy",
               bitwiseEqual(pose.mEyeYaw, pose_legacy.mEyeYaw) &&
                   bitwiseEqual(pose.mEyePitch, pose_legacy.mEyePitch));
    }
}

template<> template<>
void algazemotor_test_object::test<28>()
{
    set_test_name("profiled band-0 recruitment is bit-identical to the "
                  "profiled legacy chain");
    // The mUseLimitProfile counterpart of the band-0 legacy-parity grid:
    // with a matching (default) AnatomicalLimitProfile on BOTH sides and
    // planted hips off, the motor's converged band-0 allocation must equal
    // distributeAnatomicalChain(&profile) exactly, slot for slot.
    const ALGazeMath::AnatomicalLimitProfile profile; // matches defaults
    const F32 blends[] = { 0.f, 0.001f, 0.3f, 0.8f, 1.f };
    const F32 scales[] = { 1.f, 2.f };
    const F32 torsos[] = { 0.6f, 1.f };
    const F32 angles_deg[] =
        { 0.f, 3.f, -7.5f, 12.f, -25.f, 40.f, -60.f, 90.f,
          -120.f, 150.f, -179.f };
    constexpr S32 NANGLES = sizeof(angles_deg) / sizeof(angles_deg[0]);

    for (F32 blend : blends)
    {
        for (F32 scale : scales)
        {
            for (F32 torso : torsos)
            {
                ALGazeMotor::GazeMotorSettings s;
                s.mHeadEyeBlend = blend;
                s.mTorsoAmount  = torso;
                s.mAnatomyScale = scale;
                s.mUseLimitProfile = true; // default mLimitProfile == profile
                s.mPlantPelvis = false;    // planted hips off
                F32 cy[ALGazeMotor::CHAIN_JOINTS];
                F32 cp[ALGazeMotor::CHAIN_JOINTS];
                ALGazeMotor::effectiveCapacities(s, cy, cp);
                const bool eye_only = ALGazeMotor::isEyeOnlyBlend(s);

                for (S32 ai = 0; ai < NANGLES; ++ai)
                {
                    const F32 yaw = angles_deg[ai] * DEG_TO_RAD;
                    const F32 pitch =
                        angles_deg[(ai + 3) % NANGLES] * DEG_TO_RAD;
                    ALGazeMath::AnatomicalChainPose legacy;
                    ALGazeMath::distributeAnatomicalChain(
                        yaw, pitch, blend, torso, 90.f, legacy, scale,
                        true /*recruit_hips*/, &profile);

                    F32 jy[ALGazeMotor::CHAIN_JOINTS];
                    F32 jp[ALGazeMotor::CHAIN_JOINTS];
                    for (S32 slot = 0; slot < ALGazeMotor::CHAIN_JOINTS;
                         ++slot)
                    {
                        jy[slot] = ALGazeMotor::recruitSlot(
                            yaw, cy, 0.f, slot, eye_only);
                        jp[slot] = ALGazeMotor::recruitSlot(
                            pitch, cp, 0.f, slot, eye_only);
                    }
                    const bool yaw_exact =
                        jy[0] == legacy.mEyeYaw &&
                        jy[1] == legacy.mHeadYaw &&
                        jy[2] == legacy.mNeckYaw &&
                        jy[3] == legacy.mTorsoYaw &&
                        jy[4] == legacy.mHipsYaw;
                    const bool pitch_exact =
                        jp[0] == legacy.mEyePitch &&
                        jp[1] == legacy.mHeadPitch &&
                        jp[2] == legacy.mNeckPitch &&
                        jp[3] == legacy.mTorsoPitch &&
                        jp[4] == legacy.mHipsPitch;
                    ensure("profiled band-0 yaw allocation bit-identical "
                               "(blend " + std::to_string(blend) +
                               " scale " + std::to_string(scale) +
                               " torso " + std::to_string(torso) + " deg " +
                               std::to_string(angles_deg[ai]) + ")",
                           yaw_exact);
                    ensure("profiled band-0 pitch allocation bit-identical "
                               "(blend " + std::to_string(blend) +
                               " scale " + std::to_string(scale) +
                               " torso " + std::to_string(torso) + ")",
                           pitch_exact);

                    // Same bytewise -0.0 guard as the constant-path grid:
                    // in eye-only mode the profiled legacy early return
                    // leaves downstream fields default-constructed +0.0f;
                    // the motor's canonicalization must match it bytewise
                    // for negative targets.
                    if (blend <= 0.001f)
                    {
                        const std::string dbg =
                            " (blend " + std::to_string(blend) + " scale " +
                            std::to_string(scale) + " torso " +
                            std::to_string(torso) + " yaw_deg " +
                            std::to_string(angles_deg[ai]) + " pitch_deg " +
                            std::to_string(angles_deg[(ai + 3) % NANGLES]) +
                            ")";
                        if (yaw < 0.f)
                        {
                            ensure("profiled eye-only head yaw is bytewise "
                                       "+0.0, not -0.0" + dbg,
                                   bitwiseEqual(jy[1], legacy.mHeadYaw));
                            ensure("profiled eye-only neck yaw is bytewise "
                                       "+0.0, not -0.0" + dbg,
                                   bitwiseEqual(jy[2], legacy.mNeckYaw));
                            ensure("profiled eye-only torso yaw is bytewise "
                                       "+0.0, not -0.0" + dbg,
                                   bitwiseEqual(jy[3], legacy.mTorsoYaw));
                            ensure("profiled eye-only hips yaw is bytewise "
                                       "+0.0, not -0.0" + dbg,
                                   bitwiseEqual(jy[4], legacy.mHipsYaw));
                        }
                        const F32 pitch_val = angles_deg[(ai + 3) % NANGLES];
                        if (pitch_val < 0.f)
                        {
                            ensure("profiled eye-only head pitch is bytewise "
                                       "+0.0, not -0.0" + dbg,
                                   bitwiseEqual(jp[1], legacy.mHeadPitch));
                            ensure("profiled eye-only neck pitch is bytewise "
                                       "+0.0, not -0.0" + dbg,
                                   bitwiseEqual(jp[2], legacy.mNeckPitch));
                            ensure("profiled eye-only torso pitch is "
                                       "bytewise +0.0, not -0.0" + dbg,
                                   bitwiseEqual(jp[3], legacy.mTorsoPitch));
                            ensure("profiled eye-only hips pitch is bytewise "
                                       "+0.0, not -0.0" + dbg,
                                   bitwiseEqual(jp[4], legacy.mHipsPitch));
                        }
                    }
                }
            }
        }
    }
}

template<> template<>
void algazemotor_test_object::test<30>()
{
    set_test_name("asymmetric up/down pitch: symmetric defaults keep "
                  "bit-parity; scale/profile raise only the upward capacity");

    // 1. Symmetric defaults: the pitch_up capacity table is bitwise the down
    //    table (constant AND profiled paths), so the per-channel sign
    //    selection in step() cannot change the default output -- the band-0
    //    bit-parity contract (test 14) is preserved by construction.
    {
        ALGazeMotor::GazeMotorSettings s;
        s.mHeadEyeBlend = 0.8f;
        s.mTorsoAmount  = 0.6f;
        for (S32 mode = 0; mode < 2; ++mode)
        {
            s.mUseLimitProfile = (mode == 1); // default profile is symmetric
            F32 y_dn[ALGazeMotor::CHAIN_JOINTS];
            F32 p_dn[ALGazeMotor::CHAIN_JOINTS];
            F32 y_up[ALGazeMotor::CHAIN_JOINTS];
            F32 p_up[ALGazeMotor::CHAIN_JOINTS];
            ALGazeMotor::effectiveCapacities(s, y_dn, p_dn);
            ALGazeMotor::effectiveCapacities(s, y_up, p_up, true);
            for (S32 i = 0; i < ALGazeMotor::CHAIN_JOINTS; ++i)
            {
                ensure("symmetric up table is bitwise the down table, mode " +
                           std::to_string(mode) + " slot " +
                           std::to_string(i) + " (up " + bitsOf(p_up[i]) +
                           " down " + bitsOf(p_dn[i]) + ")",
                       bitwiseEqual(p_up[i], p_dn[i]) &&
                       bitwiseEqual(y_up[i], y_dn[i]));
            }
        }
    }

    // 2. mPitchUpScale 1.3 (constant path): the UP pitch table scales by 1.3;
    //    the DOWN pitch table and the yaw table stay bit-identical.
    {
        ALGazeMotor::GazeMotorSettings a;
        a.mHeadEyeBlend = 1.f;
        a.mTorsoAmount  = 1.f;
        ALGazeMotor::GazeMotorSettings b = a;
        b.mPitchUpScale = 1.3f;
        F32 ya[ALGazeMotor::CHAIN_JOINTS], pa_dn[ALGazeMotor::CHAIN_JOINTS];
        F32 yb[ALGazeMotor::CHAIN_JOINTS], pb_dn[ALGazeMotor::CHAIN_JOINTS];
        F32 ys[ALGazeMotor::CHAIN_JOINTS], pb_up[ALGazeMotor::CHAIN_JOINTS];
        ALGazeMotor::effectiveCapacities(a, ya, pa_dn);
        ALGazeMotor::effectiveCapacities(b, yb, pb_dn);
        ALGazeMotor::effectiveCapacities(b, ys, pb_up, true);
        for (S32 i = 0; i < ALGazeMotor::CHAIN_JOINTS; ++i)
        {
            ensure("up scale leaves the down pitch table bit-identical, "
                       "slot " + std::to_string(i),
                   bitwiseEqual(pb_dn[i], pa_dn[i]));
            ensure("up scale leaves the yaw table bit-identical, slot " +
                       std::to_string(i),
                   bitwiseEqual(yb[i], ya[i]) && bitwiseEqual(ys[i], ya[i]));
            ensure("up pitch capacity is 1.3x the down capacity, slot " +
                       std::to_string(i) + " (up " + bitsOf(pb_up[i]) +
                       " down " + bitsOf(pa_dn[i]) + ")",
                   softEqual(pb_up[i], pa_dn[i] * 1.3f));
        }
    }

    // 3. Asymmetric-PROFILE band-0 recruit from the up table is bit-identical
    //    to the profiled legacy chain for upward targets (test 28's contract
    //    extended to the sign-selected table).
    {
        ALGazeMath::AnatomicalLimitProfile prof; // down fields at defaults
        prof.mEyePitchUpDeg   = 20.f;
        prof.mHeadPitchUpDeg  = 50.f;
        prof.mNeckPitchUpDeg  = 34.f;
        prof.mSpinePitchUpDeg = 24.f;
        prof.mHipsPitchUpDeg  = 18.f;
        const F32 blends[] = { 0.f, 0.8f, 1.f };
        const F32 pitches_deg[] =
            { -150.f, -100.f, -60.f, -20.f, -3.f, 3.f, 40.f, 110.f };
        for (F32 blend : blends)
        {
            ALGazeMotor::GazeMotorSettings s;
            s.mHeadEyeBlend = blend;
            s.mTorsoAmount  = 1.f;
            s.mUseLimitProfile = true;
            s.mLimitProfile = prof;
            F32 cy[ALGazeMotor::CHAIN_JOINTS];
            F32 cp_dn[ALGazeMotor::CHAIN_JOINTS];
            F32 cp_up[ALGazeMotor::CHAIN_JOINTS];
            ALGazeMotor::effectiveCapacities(s, cy, cp_dn);
            ALGazeMotor::effectiveCapacities(s, cy, cp_up, true);
            const bool eye_only = ALGazeMotor::isEyeOnlyBlend(s);
            for (F32 pitch_deg : pitches_deg)
            {
                const F32 pitch = pitch_deg * DEG_TO_RAD;
                ALGazeMath::AnatomicalChainPose legacy;
                ALGazeMath::distributeAnatomicalChain(
                    20.f * DEG_TO_RAD, pitch, blend, 1.f, 90.f, legacy, 1.f,
                    true, &prof);
                const F32* cp = pitch < 0.f ? cp_up : cp_dn;
                F32 jp[ALGazeMotor::CHAIN_JOINTS];
                for (S32 slot = 0; slot < ALGazeMotor::CHAIN_JOINTS; ++slot)
                {
                    jp[slot] = ALGazeMotor::recruitSlot(pitch, cp, 0.f, slot,
                                                        eye_only);
                }
                ensure("asymmetric-profile band-0 pitch allocation is "
                           "bit-identical to the profiled chain (blend " +
                           std::to_string(blend) + " pitch_deg " +
                           std::to_string(pitch_deg) + ")",
                       jp[0] == legacy.mEyePitch &&
                       jp[1] == legacy.mHeadPitch &&
                       jp[2] == legacy.mNeckPitch &&
                       jp[3] == legacy.mTorsoPitch &&
                       jp[4] == legacy.mHipsPitch);
            }
        }
    }

    // 4. Converged step(): mPitchUpScale 1.3 delivers more recruited body
    //    pitch for an UPWARD target than scale 1, while the same DOWNWARD
    //    target converges bit-identically under either scale.
    {
        auto convergedPose = [](F32 up_scale,
                                F32 pitch) -> ALGazeMotor::GazeMotorPose
        {
            ALGazeMotor::GazeMotorState state;
            ALGazeMotor::GazeMotorPose pose;
            for (S32 i = 0; i < 10; ++i)
            {
                ALGazeMotor::GazeMotorInput in =
                    quietInput(i / 60.0, 0.f, pitch);
                in.mSettings.mHeadEyeBlend = 1.f;
                in.mSettings.mTorsoAmount  = 1.f;
                in.mSettings.mPitchUpScale = up_scale;
                ALGazeMotor::step(state, in, pose);
            }
            return pose;
        };
        auto bodyPitchMag = [](const ALGazeMotor::GazeMotorPose& p) -> F32
        {
            return std::fabs(p.mHeadPitch) + std::fabs(p.mNeckPitch) +
                   std::fabs(p.mTorsoPitch) + std::fabs(p.mChestPitch) +
                   std::fabs(p.mHipsPitch);
        };

        const F32 deep_up = -120.f * DEG_TO_RAD; // beyond the ~106.5 deg reach
        const ALGazeMotor::GazeMotorPose up_plain =
            convergedPose(1.f, deep_up);
        const ALGazeMotor::GazeMotorPose up_scaled =
            convergedPose(1.3f, deep_up);
        ensure("upward target recruits more body pitch under the up scale "
                   "(scaled " + std::to_string(bodyPitchMag(up_scaled)) +
                   " plain " + std::to_string(bodyPitchMag(up_plain)) + ")",
               bodyPitchMag(up_scaled) >
                   bodyPitchMag(up_plain) + 5.f * DEG_TO_RAD);
        ensure("upward recruited pitch keeps the upward sign",
               up_scaled.mHeadPitch < 0.f && up_scaled.mNeckPitch < 0.f);

        const F32 deep_dn = 120.f * DEG_TO_RAD;
        const ALGazeMotor::GazeMotorPose dn_plain =
            convergedPose(1.f, deep_dn);
        const ALGazeMotor::GazeMotorPose dn_scaled =
            convergedPose(1.3f, deep_dn);
        ensure("downward target is bit-identical under the up scale",
               bitwiseEqual(dn_scaled.mEyeYaw, dn_plain.mEyeYaw) &&
               bitwiseEqual(dn_scaled.mEyePitch, dn_plain.mEyePitch) &&
               bitwiseEqual(dn_scaled.mHeadYaw, dn_plain.mHeadYaw) &&
               bitwiseEqual(dn_scaled.mHeadPitch, dn_plain.mHeadPitch) &&
               bitwiseEqual(dn_scaled.mNeckYaw, dn_plain.mNeckYaw) &&
               bitwiseEqual(dn_scaled.mNeckPitch, dn_plain.mNeckPitch) &&
               bitwiseEqual(dn_scaled.mTorsoYaw, dn_plain.mTorsoYaw) &&
               bitwiseEqual(dn_scaled.mTorsoPitch, dn_plain.mTorsoPitch) &&
               bitwiseEqual(dn_scaled.mHipsYaw, dn_plain.mHipsYaw) &&
               bitwiseEqual(dn_scaled.mHipsPitch, dn_plain.mHipsPitch));
    }
}

template<> template<>
void algazemotor_test_object::test<31>()
{
    set_test_name("asymmetric eye soft-limit: upward comfort reaches past "
                  "the downward cap; symmetric defaults keep bit-parity");

    const F32 comfort_yaw_deg   = 25.f;
    const F32 comfort_pitch_deg = 14.f;

    // 1. DEFAULT PARITY: the default-argument call, the -1 sentinel, and an
    //    up comfort bit-equal to the down comfort are all bit-identical
    //    across both pitch signs (the symmetric else-branch is the original
    //    statement verbatim).
    {
        const F32 pitches_deg[] = { -60.f, -20.f, -5.f, 0.f, 5.f, 20.f, 60.f };
        for (F32 pitch_deg : pitches_deg)
        {
            const F32 pitch = pitch_deg * DEG_TO_RAD;
            F32 y0 = 0.f, p0 = 0.f, y1 = 0.f, p1 = 0.f, y2 = 0.f, p2 = 0.f;
            ALGazePolicy::softLimitEyeInHead(
                0.3f, pitch, comfort_yaw_deg, comfort_pitch_deg, y0, p0);
            ALGazePolicy::softLimitEyeInHead(
                0.3f, pitch, comfort_yaw_deg, comfort_pitch_deg, y1, p1,
                -1.f);
            ALGazePolicy::softLimitEyeInHead(
                0.3f, pitch, comfort_yaw_deg, comfort_pitch_deg, y2, p2,
                comfort_pitch_deg);
            ensure("soft limit: -1 sentinel is bit-identical to the default "
                       "call (pitch_deg " + std::to_string(pitch_deg) + ")",
                   bitwiseEqual(y1, y0) && bitwiseEqual(p1, p0));
            ensure("soft limit: up comfort == down comfort is bit-identical "
                       "to the default call (pitch_deg " +
                       std::to_string(pitch_deg) + " up " + bitsOf(p2) +
                       " base " + bitsOf(p0) + ")",
                   bitwiseEqual(y2, y0) && bitwiseEqual(p2, p0));
        }
    }

    // 2. Asymmetric soft limit (defect A): a large UPWARD (negative) request
    //    escapes the downward comfort's tanh ceiling and approaches the up
    //    comfort instead; the mirrored DOWNWARD request is bit-identical
    //    with or without the up comfort.
    {
        const F32 up_comfort_deg = comfort_pitch_deg * 1.3f; // 18.2 deg
        const F32 raw_up = -40.f * DEG_TO_RAD;
        F32 y_sym = 0.f, p_sym = 0.f, y_asym = 0.f, p_asym = 0.f;
        ALGazePolicy::softLimitEyeInHead(
            0.f, raw_up, comfort_yaw_deg, comfort_pitch_deg, y_sym, p_sym);
        ALGazePolicy::softLimitEyeInHead(
            0.f, raw_up, comfort_yaw_deg, comfort_pitch_deg, y_asym, p_asym,
            up_comfort_deg);
        ensure("symmetric soft limit stays inside the down comfort",
               std::fabs(p_sym) <= comfort_pitch_deg * DEG_TO_RAD + 1e-6f);
        ensure("asymmetric upward output exceeds the DOWN comfort cap "
                   "(the up range is genuinely reachable, p " +
                   std::to_string(p_asym * RAD_TO_DEG) + " deg)",
               p_asym < -comfort_pitch_deg * DEG_TO_RAD);
        ensure("asymmetric upward output stays inside the UP comfort cap",
               std::fabs(p_asym) <= up_comfort_deg * DEG_TO_RAD + 1e-6f);
        ensure("upward yaw is untouched by the asymmetric pitch limit",
               bitwiseEqual(y_asym, y_sym));

        F32 y_dn0 = 0.f, p_dn0 = 0.f, y_dn1 = 0.f, p_dn1 = 0.f;
        ALGazePolicy::softLimitEyeInHead(
            0.f, -raw_up, comfort_yaw_deg, comfort_pitch_deg, y_dn0, p_dn0);
        ALGazePolicy::softLimitEyeInHead(
            0.f, -raw_up, comfort_yaw_deg, comfort_pitch_deg, y_dn1, p_dn1,
            up_comfort_deg);
        ensure("downward request is bit-identical under the up comfort",
               bitwiseEqual(y_dn1, y_dn0) && bitwiseEqual(p_dn1, p_dn0));
    }

    // 3. End-to-end eyeInHeadFromWorldGaze: an upward world gaze against an
    //    identity head solves to an upward eye-in-head past the down
    //    comfort when the up comfort is supplied, and the downward solve is
    //    bit-identical either way.
    {
        const F32 up_comfort_deg = comfort_pitch_deg * 1.3f;
        const LLQuaternion head_identity;
        const LLVector3 up_dir =
            ALGazePolicy::eyeDirFromYawPitch(0.f, -40.f * DEG_TO_RAD);
        F32 y_sym = 0.f, p_sym = 0.f, y_asym = 0.f, p_asym = 0.f;
        ALGazePolicy::eyeInHeadFromWorldGaze(
            up_dir, head_identity, comfort_yaw_deg, comfort_pitch_deg,
            y_sym, p_sym);
        ALGazePolicy::eyeInHeadFromWorldGaze(
            up_dir, head_identity, comfort_yaw_deg, comfort_pitch_deg,
            y_asym, p_asym, up_comfort_deg);
        ensure("solved upward eye pitch is upward (negative)",
               p_sym < 0.f && p_asym < 0.f);
        ensure("solved upward eye pitch escapes the down comfort with the "
                   "up comfort supplied",
               p_asym < -comfort_pitch_deg * DEG_TO_RAD &&
               p_asym < p_sym);

        const LLVector3 dn_dir =
            ALGazePolicy::eyeDirFromYawPitch(0.f, 40.f * DEG_TO_RAD);
        F32 y_d0 = 0.f, p_d0 = 0.f, y_d1 = 0.f, p_d1 = 0.f;
        ALGazePolicy::eyeInHeadFromWorldGaze(
            dn_dir, head_identity, comfort_yaw_deg, comfort_pitch_deg,
            y_d0, p_d0);
        ALGazePolicy::eyeInHeadFromWorldGaze(
            dn_dir, head_identity, comfort_yaw_deg, comfort_pitch_deg,
            y_d1, p_d1, up_comfort_deg);
        ensure("downward end-to-end solve is bit-identical under the up "
                   "comfort",
               bitwiseEqual(y_d1, y_d0) && bitwiseEqual(p_d1, p_d0));
    }
}

} // namespace tut
