/**
 * @file llcameraoperator_test.cpp
 * @brief Matched-time determinism and safety tests for handheld locomotion.
 */

#include "../llviewerprecompiledheaders.h"

#include "../test/lltut.h"
#include "../llcameraoperator.h"
#include "llcontrol.h"

#include <cmath>
#include <limits>
#include <string>
#include <vector>

LLControlGroup gSavedSettings("CameraOperatorTest");

namespace tut
{
namespace
{
struct FloatSetting
{
    const char* mName;
    F32 mValue;
};

const FloatSetting FLOAT_SETTINGS[] = {
    { "FlycamOperatorBreathAmount", 1.5f },
    { "FlycamOperatorBreathFreq", 0.25f },
    { "FlycamOperatorBreathLift", 0.2f },
    { "FlycamOperatorCadenceDrive", 0.25f },
    { "FlycamOperatorDrag", 1.f },
    { "FlycamOperatorEnergyTrim", 1.f },
    { "FlycamOperatorForwardWalkBias", 1.f },
    { "FlycamOperatorGaitCoupling", 1.f },
    { "FlycamOperatorIdleIntensity", 1.f },
    { "FlycamOperatorLateralStep", 0.008f },
    { "FlycamOperatorLateralWalkBias", 0.25f },
    { "FlycamOperatorSimulationHz", 120.f },
    { "FlycamOperatorModeBlendTime", 0.85f },
    { "FlycamOperatorGainSurge", 1.f },
    { "FlycamOperatorGainSway", 1.f },
    { "FlycamOperatorGainHeave", 1.f },
    { "FlycamOperatorGainRoll", 1.f },
    { "FlycamOperatorGainPitch", 1.f },
    { "FlycamOperatorGainYaw", 1.f },
    { "FlycamOperatorGainFOV", 1.f },
    { "FlycamOperatorAutoWalkEnter", 0.24f },
    { "FlycamOperatorAutoWalkExit", 0.14f },
    { "FlycamOperatorAutoRunEnter", 0.82f },
    { "FlycamOperatorAutoRunExit", 0.62f },
    { "FlycamOperatorAutoWalkEnterDwell", 0.65f },
    { "FlycamOperatorAutoWalkExitDwell", 1.10f },
    { "FlycamOperatorAutoRunEnterDwell", 0.55f },
    { "FlycamOperatorAutoRunExitDwell", 0.90f },
    { "FlycamOperatorMaster", 1.f },
    { "FlycamOperatorMotionBreath", 1.f },
    { "FlycamOperatorMotionCalm", 1.f },
    { "FlycamOperatorMotionPan", 1.f },
    { "FlycamOperatorMotionRoll", 1.f },
    { "FlycamOperatorMotionTilt", 1.f },
    { "FlycamOperatorOnset", 1.f },
    { "FlycamOperatorPanAmount", 0.6f },
    { "FlycamOperatorPanReactSmoothing", 0.5f },
    { "FlycamOperatorPanSmoothness", 0.6f },
    { "FlycamOperatorPanTiltFreq", 0.6f },
    { "FlycamOperatorProfileInfluence", 0.7f },
    { "FlycamOperatorReactivity", 1.f },
    { "FlycamOperatorRecomposeAmount", 0.35f },
    { "FlycamOperatorRecomposeInterval", 7.f },
    { "FlycamOperatorRefAngularSpeed", 60.f },
    { "FlycamOperatorRefLinearSpeed", 3.f },
    { "FlycamOperatorRollAmount", 0.5f },
    { "FlycamOperatorRollDamping", 0.65f },
    { "FlycamOperatorRollFreq", 0.3f },
    { "FlycamOperatorSeed", 0.f },
    { "FlycamOperatorSettle", 1.f },
    { "FlycamOperatorSettleDecay", 0.85f },
    { "FlycamOperatorSettleFreq", 8.f },
    { "FlycamOperatorSmoothing", 1.f },
    { "FlycamOperatorStepBob", 0.015f },
    { "FlycamOperatorStepRoll", 0.35f },
    { "FlycamOperatorTiltAmount", 0.6f },
    { "FlycamOperatorTimeSpeed", 1.f },
    { "FlycamOperatorTremorDamping", 0.6f },
    { "FlycamOperatorWalkCadence", 1.f },
};

void declareSettings()
{
    static bool initialized = false;
    if (initialized)
    {
        return;
    }
    initialized = true;
    for (const FloatSetting& setting : FLOAT_SETTINGS)
    {
        gSavedSettings.declareF32(
            setting.mName, setting.mValue, "",
            LLControlVariable::PERSIST_NONDFT);
    }
    gSavedSettings.declareS32(
        "FlycamOperatorLocomotionMode", 0, "",
        LLControlVariable::PERSIST_NONDFT);
    gSavedSettings.declareS32(
        "FlycamOperatorPanMode", 0, "",
        LLControlVariable::PERSIST_NONDFT);
    gSavedSettings.declareS32(
        "FlycamOperatorProfile", 0, "",
        LLControlVariable::PERSIST_NONDFT);
    gSavedSettings.declareS32(
        "FlycamOperatorStyle", 3, "",
        LLControlVariable::PERSIST_NONDFT);
    gSavedSettings.declareBOOL(
        "FlycamOperatorEnabled", false, "",
        LLControlVariable::PERSIST_NONDFT);
    gSavedSettings.declareBOOL(
        "FlycamOperatorForceWalk", false, "",
        LLControlVariable::PERSIST_NONDFT);
}

void restoreSettings()
{
    for (const FloatSetting& setting : FLOAT_SETTINGS)
    {
        gSavedSettings.setF32(setting.mName, setting.mValue);
    }
    gSavedSettings.setS32("FlycamOperatorLocomotionMode", 0);
    gSavedSettings.setS32("FlycamOperatorPanMode", 0);
    gSavedSettings.setS32("FlycamOperatorProfile", 0);
    gSavedSettings.setS32("FlycamOperatorStyle", 3);
    gSavedSettings.setBOOL("FlycamOperatorEnabled", false);
    gSavedSettings.setBOOL("FlycamOperatorForceWalk", false);
}

void resetTestState()
{
    restoreSettings();

    // reset() deliberately retains procedural phases in Legacy. Enter a
    // locomotion mode solely for fixture cleanup so every test starts from a
    // known phase even when an earlier ensure() threw.
    gSavedSettings.setS32("FlycamOperatorLocomotionMode", 1);
    LLCameraOperator::instance().reset();
    gSavedSettings.setS32("FlycamOperatorLocomotionMode", 0);
}

void ensureClose(const char* message,
                 const LLCameraOperatorOutput& expected,
                 const LLCameraOperatorOutput& actual,
                 F32 tolerance = 2e-4f)
{
    ensure(message,
           (expected.mPosOffset - actual.mPosOffset).magVec() <= tolerance &&
           fabsf(expected.mRoll - actual.mRoll) <= tolerance &&
           fabsf(expected.mPitch - actual.mPitch) <= tolerance &&
           fabsf(expected.mYaw - actual.mYaw) <= tolerance &&
           fabsf(expected.mFovMul - actual.mFovMul) <= tolerance);
}

LLVector3 autoPosition(F32 time)
{
    if (time <= 1.f)
    {
        return LLVector3::zero;
    }
    if (time <= 2.f)
    {
        return LLVector3(time - 1.f, 0.f, 0.f);
    }
    return LLVector3(1.f + 3.f * (time - 2.f), 0.f, 0.f);
}

LLCameraOperatorOutput runAuto(S32 fps)
{
    gSavedSettings.setS32("FlycamOperatorLocomotionMode", 100);
    LLCameraOperator& camera_operator = LLCameraOperator::instance();
    camera_operator.reset();
    camera_operator.updateFromPose(
        0.f, autoPosition(0.f), LLQuaternion::DEFAULT);

    LLCameraOperatorOutput output;
    for (S32 frame = 1; frame <= fps * 3; ++frame)
    {
        const F32 time = (F32)frame / (F32)fps;
        output = camera_operator.updateFromPose(
            1.f / (F32)fps, autoPosition(time), LLQuaternion::DEFAULT);
    }
    return output;
}

LLCameraOperatorOutput runMidBlendRetarget(S32 fps)
{
    gSavedSettings.setS32("FlycamOperatorLocomotionMode", 1);
    LLCameraOperator& camera_operator = LLCameraOperator::instance();
    camera_operator.reset();
    camera_operator.updateFromPose(0.f, LLVector3::zero,
                                   LLQuaternion::DEFAULT);

    LLCameraOperatorOutput output;
    for (S32 frame = 1; frame <= fps * 2; ++frame)
    {
        const S32 interval_start = frame - 1;
        if (interval_start == fps)
        {
            gSavedSettings.setS32("FlycamOperatorLocomotionMode", 3);
        }
        if (interval_start == fps * 3 / 2)
        {
            // Retarget Run -> Float while the 0.85 s Creep -> Run blend is
            // still live. The source must be the current mixture.
            gSavedSettings.setS32("FlycamOperatorLocomotionMode", 5);
        }
        const F32 time = (F32)frame / (F32)fps;
        output = camera_operator.updateFromPose(
            1.f / (F32)fps,
            LLVector3(time * 1.5f, time * 0.2f, 0.f),
            LLQuaternion::DEFAULT);
    }
    return output;
}

LLCameraOperatorOutput runDrive(S32 fps)
{
    gSavedSettings.setS32("FlycamOperatorLocomotionMode", 4);
    LLCameraOperator& camera_operator = LLCameraOperator::instance();
    camera_operator.reset();
    camera_operator.updateFromPose(0.f, LLVector3::zero,
                                   LLQuaternion::DEFAULT);

    LLCameraOperatorOutput output;
    for (S32 frame = 1; frame <= fps * 2; ++frame)
    {
        const F32 time = (F32)frame / (F32)fps;
        output = camera_operator.updateFromPose(
            1.f / (F32)fps, LLVector3(time * 4.f, 0.f, 0.f),
            LLQuaternion::DEFAULT);
    }
    return output;
}

LLCameraOperatorOutput runTakeStartInMotion(S32 fps, S32 mode)
{
    gSavedSettings.setS32("FlycamOperatorLocomotionMode", mode);
    LLCameraOperator& camera_operator = LLCameraOperator::instance();
    camera_operator.reset();

    const F32 frame_dt = 1.f / (F32)fps;
    // Production pose callers prime with the real render dt. The path is
    // already moving at the cut, so the first derivable velocity is 3 m/s.
    camera_operator.updateFromPose(
        frame_dt, LLVector3::zero, LLQuaternion::DEFAULT);

    LLCameraOperatorOutput output;
    const S32 frames = fps / 6; // all tested rates meet again at t = 1/6 s
    for (S32 frame = 1; frame <= frames; ++frame)
    {
        output = camera_operator.updateFromPose(
            frame_dt,
            LLVector3(3.f * (F32)frame * frame_dt, 0.f, 0.f),
            LLQuaternion::DEFAULT);
    }
    return output;
}

LLVector3 curvedPosition(F32 time)
{
    return LLVector3(1.7f * time,
                     0.35f * sinf(F_PI * time),
                     0.12f * time * time);
}

LLQuaternion curvedRotation(F32 time)
{
    return LLQuaternion(0.18f * sinf(0.7f * time),
                        LLVector3::z_axis);
}

LLCameraOperatorOutput runCurvedTake(S32 fps)
{
    gSavedSettings.setS32("FlycamOperatorLocomotionMode", 2);
    LLCameraOperator& camera_operator = LLCameraOperator::instance();
    camera_operator.reset();

    const F32 frame_dt = 1.f / (F32)fps;
    camera_operator.updateFromPose(
        frame_dt, curvedPosition(0.f), curvedRotation(0.f));

    LLCameraOperatorOutput output;
    for (S32 frame = 1; frame <= fps * 2; ++frame)
    {
        const F32 time = (F32)frame * frame_dt;
        output = camera_operator.updateFromPose(
            frame_dt, curvedPosition(time), curvedRotation(time));
    }
    return output;
}

LLCameraOperatorOutput runLinearBacklog(bool one_hitch)
{
    gSavedSettings.setS32("FlycamOperatorLocomotionMode", 2);
    LLCameraOperator& camera_operator = LLCameraOperator::instance();
    camera_operator.reset();
    camera_operator.updateFromPose(
        1.f / 120.f, LLVector3::zero, LLQuaternion::DEFAULT);

    LLCameraOperatorOutput output;
    if (one_hitch)
    {
        // Five seconds exceeds the 512-step cap at 120 Hz. The last 88 ticks
        // must remain on this segment when the following segment is queued.
        output = camera_operator.updateFromPose(
            5.f, LLVector3(5.f, 0.f, 0.f), LLQuaternion::DEFAULT);
        output = camera_operator.updateFromPose(
            0.1f, LLVector3(5.1f, 0.f, 0.f), LLQuaternion::DEFAULT);
    }
    else
    {
        for (S32 tick = 1; tick <= 612; ++tick)
        {
            const F32 time = (F32)tick / 120.f;
            output = camera_operator.updateFromPose(
                1.f / 120.f, LLVector3(time, 0.f, 0.f),
                LLQuaternion::DEFAULT);
        }
    }
    return output;
}

LLCameraOperatorOutput runLegacyNegativeGolden(F32 time_speed)
{
    // Force a known phase origin, then return to the stock Legacy path.
    gSavedSettings.setS32("FlycamOperatorLocomotionMode", 1);
    LLCameraOperator::instance().reset();
    gSavedSettings.setS32("FlycamOperatorLocomotionMode", 0);
    gSavedSettings.setF32("FlycamOperatorMaster", 1.25f);
    gSavedSettings.setF32("FlycamOperatorTimeSpeed", time_speed);
    LLCameraOperator::instance().reset();

    LLCameraOperatorInput input;
    input.mDeltaTime = 1.f / 37.f;
    input.mLinearVel = LLVector3(2.4f, -0.7f, 0.3f);
    input.mAngularVel = LLVector3(0.08f, -0.14f, 0.22f);
    return LLCameraOperator::instance().update(input);
}
} // anonymous namespace

struct camera_operator_data
{
    camera_operator_data()
    {
        declareSettings();
        resetTestState();
    }

    ~camera_operator_data()
    {
        resetTestState();
    }
};
typedef test_group<camera_operator_data> camera_operator_group;
typedef camera_operator_group::object camera_operator_object;
camera_operator_group camera_operator_tests("LLCameraOperator");

template<> template<>
void camera_operator_object::test<1>()
{
    const S32 rates[] = { 30, 60, 120, 144 };
    const LLCameraOperatorOutput auto_reference = runAuto(rates[0]);
    const LLCameraOperatorOutput blend_reference =
        runMidBlendRetarget(rates[0]);
    const LLCameraOperatorOutput drive_reference = runDrive(rates[0]);
    for (S32 index = 1; index < 4; ++index)
    {
        ensureClose("Auto threshold crossing matches at equal time",
                    auto_reference, runAuto(rates[index]));
        ensureClose("mid-blend retarget matches at equal time",
                    blend_reference, runMidBlendRetarget(rates[index]));
        ensureClose("Drive layers match at equal time",
                    drive_reference, runDrive(rates[index]));
    }
}

template<> template<>
void camera_operator_object::test<2>()
{
    // Reset/replay must reproduce a take exactly with the same seed and path.
    const LLCameraOperatorOutput first = runAuto(60);
    const LLCameraOperatorOutput replay = runAuto(60);
    ensureClose("reset/replay is deterministic", first, replay, 1e-6f);
}

template<> template<>
void camera_operator_object::test<3>()
{
    // Auto entry is an immediate classification, not a no-op Walk blend.
    LLCameraOperatorInput input;
    input.mDeltaTime = 1.f / 120.f;

    gSavedSettings.setS32("FlycamOperatorLocomotionMode", 100);
    LLCameraOperator::instance().reset();
    LLCameraOperator::instance().update(input);
    const LLCameraOperatorOutput auto_creep =
        LLCameraOperator::instance().update(input);
    gSavedSettings.setS32("FlycamOperatorLocomotionMode", 1);
    LLCameraOperator::instance().reset();
    LLCameraOperator::instance().update(input);
    const LLCameraOperatorOutput direct_creep =
        LLCameraOperator::instance().update(input);
    ensureClose("stationary Auto starts in Creep",
                direct_creep, auto_creep, 1e-6f);

    input.mLinearVel = LLVector3(3.f, 0.f, 0.f);
    gSavedSettings.setS32("FlycamOperatorLocomotionMode", 100);
    LLCameraOperator::instance().reset();
    LLCameraOperator::instance().update(input);
    const LLCameraOperatorOutput auto_run =
        LLCameraOperator::instance().update(input);
    gSavedSettings.setS32("FlycamOperatorLocomotionMode", 3);
    LLCameraOperator::instance().reset();
    LLCameraOperator::instance().update(input);
    const LLCameraOperatorOutput direct_run =
        LLCameraOperator::instance().update(input);
    ensureClose("fast Auto starts in Run", direct_run, auto_run, 1e-6f);
}

template<> template<>
void camera_operator_object::test<4>()
{
    // Stock clamps every finite negative TimeSpeed to zero. This is a
    // non-trivial Legacy golden vector (Master > 0, translation and rotation)
    // against that exact stock boundary behavior.
    const LLCameraOperatorOutput stock_clamped =
        runLegacyNegativeGolden(0.f);
    const LLCameraOperatorOutput negative =
        runLegacyNegativeGolden(-2.75f);
    ensure("Legacy golden is non-trivial",
           stock_clamped.mPosOffset.magVec() > 1e-6f ||
           fabsf(stock_clamped.mRoll) > 1e-6f ||
           fabsf(stock_clamped.mPitch) > 1e-6f ||
           fabsf(stock_clamped.mYaw) > 1e-6f ||
           fabsf(stock_clamped.mFovMul - 1.f) > 1e-6f);
    ensureClose("negative Legacy TimeSpeed matches stock clamp",
                stock_clamped, negative, 0.f);
}

template<> template<>
void camera_operator_object::test<5>()
{
    gSavedSettings.setS32("FlycamOperatorLocomotionMode", 4);
    gSavedSettings.setF32(
        "FlycamOperatorTimeSpeed",
        std::numeric_limits<F32>::quiet_NaN());
    gSavedSettings.setF32(
        "FlycamOperatorGainRoll",
        std::numeric_limits<F32>::infinity());
    LLCameraOperator::instance().reset();
    LLCameraOperatorInput input;
    input.mDeltaTime = std::numeric_limits<F32>::quiet_NaN();
    input.mLinearVel = LLVector3(
        std::numeric_limits<F32>::infinity(), 0.f, 0.f);
    LLCameraOperator::instance().update(input);
    const LLCameraOperatorOutput output =
        LLCameraOperator::instance().update(input);
    ensure("non-finite inputs cannot escape",
           output.mPosOffset.isFinite() &&
           std::isfinite(output.mRoll) &&
           std::isfinite(output.mPitch) &&
           std::isfinite(output.mYaw) &&
           std::isfinite(output.mFovMul));
}

template<> template<>
void camera_operator_object::test<6>()
{
    // Parked Drive may retain a tiny vertical idle-engine texture, but road
    // buzz must not emit tyre/chassis surge without linear vehicle speed.
    gSavedSettings.setS32("FlycamOperatorLocomotionMode", 4);
    LLCameraOperator::instance().reset();
    LLCameraOperatorInput parked;
    parked.mDeltaTime = 1.f / 120.f;
    LLCameraOperator::instance().update(parked);
    const LLCameraOperatorOutput output =
        LLCameraOperator::instance().update(parked);
    ensure_equals("parked Drive has no road surge",
                  output.mPosOffset.mV[VX], 0.f);
}

template<> template<>
void camera_operator_object::test<7>()
{
    const S32 rates[] = { 30, 60, 120, 144 };
    const LLCameraOperatorOutput auto_reference =
        runTakeStartInMotion(rates[0], 100);
    for (S32 index = 0; index < 4; ++index)
    {
        const LLCameraOperatorOutput auto_output =
            runTakeStartInMotion(rates[index], 100);
        const LLCameraOperatorOutput direct_run =
            runTakeStartInMotion(rates[index], 3);
        ensureClose("moving pose take opens directly in Run",
                    direct_run, auto_output, 1e-6f);
        ensureClose("moving pose take opening gait is FPS-independent",
                    auto_reference, auto_output, 2e-4f);
    }
}

template<> template<>
void camera_operator_object::test<8>()
{
    // Fixed tick times are exact, but a curved path is reconstructed from each
    // rate's render-frame polyline. Cross-FPS results should converge within
    // this 0.003 output-unit bound; exact bit identity is not the contract.
    const S32 rates[] = { 30, 60, 120, 144 };
    const LLCameraOperatorOutput reference = runCurvedTake(rates[0]);
    for (S32 index = 1; index < 4; ++index)
    {
        ensureClose("curved pose paths converge across render rates",
                    reference, runCurvedTake(rates[index]), 0.003f);
    }
}

template<> template<>
void camera_operator_object::test<9>()
{
    ensureClose("capped backlog retains its original render segment",
                runLinearBacklog(false), runLinearBacklog(true), 2e-4f);
}
} // namespace tut
