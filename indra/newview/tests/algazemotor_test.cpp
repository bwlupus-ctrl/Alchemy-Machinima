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

#include <cmath>
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

} // namespace tut
