/**
 * @file alflycamweighted.cpp
 * @brief Pure Weighted Classic flycam mass layer and frame coordinator.
 */

#include "llviewerprecompiledheaders.h"

#include "alflycamweighted.h"

#include "m3math.h"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <limits>
#include <sstream>

namespace
{
constexpr F64 PI_D = 3.1415926535897932384626433832795;
constexpr F64 VELOCITY_EPSILON = 1.0e-3;
constexpr F64 ANGULAR_EPSILON = 1.0e-3;
constexpr F64 DISTANCE_EPSILON = 2.0e-4;
constexpr F64 ANGLE_EPSILON = 1.0e-4;

bool finiteVector(const LLVector3d& value)
{
    return std::isfinite(value.mdV[VX]) && std::isfinite(value.mdV[VY]) &&
        std::isfinite(value.mdV[VZ]);
}

F64 vectorLength(const LLVector3d& value)
{
    return std::sqrt(value.mdV[VX] * value.mdV[VX] +
                     value.mdV[VY] * value.mdV[VY] +
                     value.mdV[VZ] * value.mdV[VZ]);
}

F64 quaternionDistance(const LLQuaternion& lhs, const LLQuaternion& rhs)
{
    F64 dot = 0.0;
    F64 lhsLength = 0.0;
    F64 rhsLength = 0.0;
    for (S32 index = 0; index < 4; ++index)
    {
        const F64 a = static_cast<F64>(lhs.mQ[index]);
        const F64 b = static_cast<F64>(rhs.mQ[index]);
        dot += a * b;
        lhsLength += a * a;
        rhsLength += b * b;
    }
    if (!(lhsLength > 0.0) || !(rhsLength > 0.0))
        return PI_D;
    const F64 cosine = std::max(-1.0, std::min(1.0,
        std::fabs(dot / std::sqrt(lhsLength * rhsLength))));
    const F64 chord = std::sqrt(std::max(0.0, 2.0 - 2.0 * cosine));
    return 2.0 * std::asin(std::min(1.0, chord * 0.5));
}

bool poseMismatch(const LLVector3& position, const LLQuaternion& rotation,
                  const LLVector3& referencePosition, const LLQuaternion& referenceRotation)
{
    return static_cast<F64>((position - referencePosition).length()) > 1.0e-4 ||
        quaternionDistance(rotation, referenceRotation) > 1.0e-5;
}

const char* verdictName(ALFlycamWeighted::FrameOut::Telemetry::Verdict verdict)
{
    using Verdict = ALFlycamWeighted::FrameOut::Telemetry::Verdict;
    switch (verdict)
    {
    case Verdict::NONFINITE: return "NONFINITE";
    case Verdict::POSECUT_STORM: return "POSECUT-STORM";
    case Verdict::EXTREME: return "EXTREME";
    case Verdict::DRIFT: return "DRIFT";
    case Verdict::TRACK: return "TRACK";
    case Verdict::BYPASS: return "BYPASS";
    case Verdict::OK: return "OK";
    }
    return "OK";
}

S32 edgeIndex(ALFlycamWeightedEdge edge)
{
    switch (edge)
    {
    case ALFlycamWeightedEdge::C2W: return 0;
    case ALFlycamWeightedEdge::W2C: return 1;
    case ALFlycamWeightedEdge::D2W: return 2;
    case ALFlycamWeightedEdge::W2D: return 3;
    case ALFlycamWeightedEdge::W2O: return 4;
    case ALFlycamWeightedEdge::O2W: return 5;
    case ALFlycamWeightedEdge::NONE: return -1;
    }
    return -1;
}

struct CascadeResult
{
    LLVector3d integral;
};

CascadeResult cascade(LLVector3d& first, LLVector3d& second,
                      const LLVector3d& target, F64 tau, F64 h)
{
    CascadeResult result;
    if (!(tau > 0.0))
    {
        first = target;
        second = target;
        result.integral = target * h;
        return result;
    }

    const F64 x = h / tau;
    const F64 a = std::exp(-x);
    const F64 b = x * a;
    const F64 oneMinusA = -std::expm1(-x);
    const F64 oneMinusAMinusB = x < 1.0e-3
        ? x * x * (0.5 + x * (-1.0 / 3.0 + x * 0.125))
        : oneMinusA - b;
    const LLVector3d firstError = first - target;
    const LLVector3d secondError = second - target;
    result.integral = target * h +
        (secondError * oneMinusA + firstError * oneMinusAMinusB) * tau;
    first = target + firstError * a;
    second = target + secondError * a + firstError * b;
    return result;
}

bool targetAtRest(const ALFlycamWeighted::Target& target)
{
    return target.inputZero && vectorLength(target.v) < VELOCITY_EPSILON &&
        vectorLength(target.w) < ANGULAR_EPSILON;
}
}

namespace ALFlycamWeighted
{
Tuning tuningForWeight(F32 weightPercent, F32 tauMax, F32 curveK, F32 rotationShare)
{
    const F64 rawWeight = std::isfinite(weightPercent)
        ? static_cast<F64>(weightPercent) : 0.0;
    const F64 weight = std::max(0.0, std::min(100.0, rawWeight)) / 100.0;
    // Spec §8 ranges: TauMax [0.1, 3] s (default 1), Curve [1, 1000] (default 20),
    // RotationShare [0, 2] (default 0.6). Non-finite values fall back to the default.
    const F64 maximumTau = std::isfinite(tauMax)
        ? std::max(0.1, std::min(3.0, static_cast<F64>(tauMax))) : 1.0;
    const F64 curve = std::isfinite(curveK)
        ? std::max(1.0, std::min(1000.0, static_cast<F64>(curveK))) : 20.0;
    const F64 share = std::isfinite(rotationShare)
        ? std::max(0.0, std::min(2.0, static_cast<F64>(rotationShare))) : 0.6;

    Tuning result;
    result.tauT = curve <= 1.0001
        ? maximumTau * weight
        : maximumTau * (std::pow(curve, weight) - 1.0) / (curve - 1.0);
    result.tauR = share * result.tauT;
    return result;
}

Target targetFromClassic(const F32 d[6], const LLQuaternion& q0, F64 h, bool inputZero)
{
    Target result;
    result.inputZero = inputZero;
    if (!(h > 0.0) || !std::isfinite(h) || !q0.isFinite())
    {
        const F64 invalid = std::numeric_limits<F64>::quiet_NaN();
        result.v.set(invalid, invalid, invalid);
        result.w.set(invalid, invalid, invalid);
        return result;
    }

    const LLVector3 local(d[0], d[1], d[2]);
    result.v = LLVector3d(local * q0) * (1.0 / h);

    LLQuaternion increment(LLMatrix3(d[3], d[4], d[5]));
    if (increment.mQ[VW] < 0.f)
        increment = -increment;
    const F64 x = static_cast<F64>(increment.mQ[VX]);
    const F64 y = static_cast<F64>(increment.mQ[VY]);
    const F64 z = static_cast<F64>(increment.mQ[VZ]);
    const F64 imaginaryLength = std::sqrt(x * x + y * y + z * z);
    if (imaginaryLength >= 1.0e-12)
    {
        const F64 angleScale = 2.0 * std::atan2(
            imaginaryLength, static_cast<F64>(increment.mQ[VW])) /
            (imaginaryLength * h);
        result.w.set(x * angleScale, y * angleScale, z * angleScale);
    }
    return result;
}

bool isExtremeInput(const Target& target, F64 h)
{
    return finiteVector(target.v) && finiteVector(target.w) && std::isfinite(h) &&
        (vectorLength(target.v) > 2000.0 || vectorLength(target.w) * h > PI_D * 0.5);
}

Step advance(State& state, const Target& target, const Tuning& tuning, F64 h)
{
    Step result;
    if (!(h > 0.0) || !std::isfinite(h) || !finiteVector(target.v) ||
        !finiteVector(target.w) || !std::isfinite(tuning.tauT) ||
        !std::isfinite(tuning.tauR))
    {
        result.nonFinite = true;
        rest(state);
        return result;
    }

    const bool wake = !target.inputZero || vectorLength(target.v) >= 2.0 * VELOCITY_EPSILON ||
        vectorLength(target.w) >= 2.0 * ANGULAR_EPSILON;
    if (state.settled && !wake)
    {
        result.held = true;
        return result;
    }
    if (state.settled)
    {
        state.settled = false;
        result.woke = true;
    }
    state.active = true;

    result.dp = cascade(state.uT, state.vT, target.v, tuning.tauT, h).integral;
    result.dtheta = cascade(state.uR, state.vR, target.w, tuning.tauR, h).integral;
    if (!finiteVector(result.dp) || !finiteVector(result.dtheta) ||
        !finiteVector(state.uT) || !finiteVector(state.vT) ||
        !finiteVector(state.uR) || !finiteVector(state.vR))
    {
        result = Step();
        result.nonFinite = true;
        rest(state);
        return result;
    }

    if (targetAtRest(target) &&
        tuning.tauT * vectorLength(state.uT + state.vT) < DISTANCE_EPSILON &&
        tuning.tauR * vectorLength(state.uR + state.vR) < ANGLE_EPSILON)
    {
        rest(state);
        result.settledNow = true;
    }
    return result;
}

void seedAtTarget(State& state, const Target& target)
{
    state.uT = state.vT = target.v;
    state.uR = state.vR = target.w;
    state.active = true;
    state.settled = targetAtRest(target);
}

void seedVelocity(State& state, const LLVector3d& velocity, const LLVector3d& angularVelocity)
{
    if (!finiteVector(velocity) || !finiteVector(angularVelocity))
    {
        rest(state);
        return;
    }
    state.uT = state.vT = velocity;
    state.uR = state.vR = angularVelocity;
    state.active = true;
    state.settled = vectorLength(velocity) < VELOCITY_EPSILON &&
        vectorLength(angularVelocity) < ANGULAR_EPSILON;
}

void rest(State& state)
{
    state.uT.setZero();
    state.vT.setZero();
    state.uR.setZero();
    state.vR.setZero();
    state.active = true;
    state.settled = true;
}

void deactivate(State& state)
{
    rest(state);
    state.active = false;
}

void zeroRoll(State& state)
{
    state.uR.mdV[VX] = 0.0;
    state.vR.mdV[VX] = 0.0;
}

LLQuaternion rotationFromVector(const LLVector3d& rotationVector)
{
    const F64 length = vectorLength(rotationVector);
    if (!(length >= 1.0e-12) || !std::isfinite(length))
        return LLQuaternion();
    const F64 inverseLength = 1.0 / length;
    LLQuaternion result;
    result.setAngleAxis(static_cast<F32>(length), LLVector3(
        static_cast<F32>(rotationVector.mdV[VX] * inverseLength),
        static_cast<F32>(rotationVector.mdV[VY] * inverseLength),
        static_cast<F32>(rotationVector.mdV[VZ] * inverseLength)));
    return result;
}

F32 presetWeight(S32 preset)
{
    static const F32 weights[] = { 25.f, 50.f, 70.f, 90.f };
    return preset >= 0 && preset < 4 ? weights[preset] : 50.f;
}

S32 presetForWeight(F32 weightPercent)
{
    if (!std::isfinite(weightPercent))
        return 4;
    for (S32 preset = 0; preset < 4; ++preset)
        if (std::fabs(weightPercent - presetWeight(preset)) <= .05f)
            return preset;
    return 4;
}

const char* presetName(S32 preset)
{
    static const char* names[] = { "Light", "Steadicam", "Heavy", "Crane", "Custom" };
    return preset >= 0 && preset < 5 ? names[preset] : "Custom";
}

LLQuaternion classicLevel(const LLQuaternion& rotation, const FrameIn& in)
{
    // Design R3 OPEN-12 fallback: this is the leveling block copied verbatim
    // from Classic, with only the state rotation replaced by this local value.
    // Calling finishIntegration with an identity motion quaternion is not
    // bitwise transparent under Release /fp:fast.
    LLQuaternion flycamRotation = rotation;
    const F32 time = in.classicIm.time;
    const F32 feather = in.classicIm.feather;

    if (in.classicIm.blackDragon)
    {
        const F32 flycam_feather = in.classicIm.feather;

        if (in.classicIn.levelHorizon || in.classicIn.autoLeveling || in.classicIn.rollDefault == 1)
        {
            LLMatrix3 level(flycamRotation);

            LLVector3 x = LLVector3(level.mMatrix[0]);
            LLVector3 y = LLVector3(level.mMatrix[1]);
            LLVector3 z = LLVector3(level.mMatrix[2]);

            y.mV[2] = 0.f;
            y.normVec();

            level.setRows(x,y,z);
            level.orthogonalize();

            LLQuaternion quat(level);
            LLQuaternion lerp = nlerp(llmin(flycam_feather * time, 1.f), flycamRotation, quat);
            flycamRotation = in.classicIn.levelHorizon || in.classicIn.rollDefault == 1 ? quat : lerp;
        }
    }
    else if (in.classicIn.levelHorizon || in.classicIn.autoLeveling)
    {
        LLMatrix3 level(flycamRotation);

        LLVector3 x = LLVector3(level.mMatrix[0]);
        LLVector3 y = LLVector3(level.mMatrix[1]);
        LLVector3 z = LLVector3(level.mMatrix[2]);

        y.mV[2] = 0.f;
        y.normVec();

        level.setRows(x,y,z);
        level.orthogonalize();

        LLQuaternion quat(level);
        flycamRotation = in.classicIn.levelHorizon ? quat : nlerp(llmin(feather*time,1.f), flycamRotation, quat);
    }

    return flycamRotation;
}

void Coordinator::report(const FrameIn& in, FrameOut& out)
{
    if (mReportTime < 5.0)
        return;
    mTelemetry.weight = in.settings.weight;
    mTelemetry.preset = presetForWeight(in.settings.weight);
    mTelemetry.tuning = tuningForWeight(in.settings.weight, in.settings.tauMax,
        in.settings.curve, in.settings.rotationShare);
    mTelemetry.fps = mReportElapsed > 0.0
        ? static_cast<F64>(mReportFrames) / mReportElapsed : 0.0;
    const F64 translationError = mTrackedTargetDistance > 0.0
        ? std::fabs(mTrackedDistance / mTrackedTargetDistance - 1.0) : 0.0;
    const F64 rotationError = mTrackedTargetAngle > 0.0
        ? std::fabs(mTrackedAngle / mTrackedTargetAngle - 1.0) : 0.0;
    mTelemetry.trackingErrorPercent = 100.0 * std::max(translationError, rotationError);
    mTelemetry.posecutInternal = mInternalCuts;
    mTelemetry.posecutPresented = mPresentedCutCount;
    mTelemetry.posecutNotComparable = mPresentedNotComparable;
    mTelemetry.guardArmed = mGuardArmed;
    mTelemetry.extreme = mExtremeCount;
    mTelemetry.degenerate = mDegenerateCount;
    mTelemetry.nonFinite = mNonFiniteCount;
    using Verdict = FrameOut::Telemetry::Verdict;
    mTelemetry.verdict = mNonFiniteCount != 0 ? Verdict::NONFINITE :
        (mStormDisarmed ? Verdict::POSECUT_STORM :
        (mExtremeCount != 0 ? Verdict::EXTREME :
        (mDrift ? Verdict::DRIFT :
        ((mTrackedTargetDistance > 0.0 || mTrackedTargetAngle > 0.0) &&
         mTelemetry.trackingErrorPercent > 5.0 ? Verdict::TRACK :
        (!(in.settings.weight > 0.f) ? Verdict::BYPASS : Verdict::OK)))));
    std::ostringstream message;
    message.setf(std::ios::fixed);
    message.precision(3);
    message << "WEIGHTED " << verdictName(mTelemetry.verdict)
            << " w=" << static_cast<S32>(std::lround(in.settings.weight)) << "%"
            << " preset=" << presetName(mTelemetry.preset)
            << " tau_t=" << mTelemetry.tuning.tauT << " tau_r=" << mTelemetry.tuning.tauR
            << " fps=" << mTelemetry.fps
            << " vmax=" << mTelemetry.velocityMaximum << "/" << mTelemetry.velocityTargetMaximum
            << " wmax=" << mTelemetry.angularMaximum << "/" << mTelemetry.angularTargetMaximum
            << " track_err=" << mTelemetry.trackingErrorPercent << "%"
            << " settles=" << mTelemetry.settles
            << " coast_last=" << mTelemetry.lastCoastSeconds << "s(" << mTelemetry.lastCoastTau << "tau)"
            << " reseeds=foreign:" << mTelemetry.reseedForeign << " self:" << mTelemetry.reseedSelf
            << " origin:" << mTelemetry.reseedOrigin << " c2w:" << mTelemetry.reseedC2W
            << " posecut=internal:" << mTelemetry.posecutInternal
            << " presented:" << mTelemetry.posecutPresented
            << (mTelemetry.posecutNotComparable != 0 ? "|n/a" : "")
            << " guard=" << (mGuardArmed ? "armed" : "disarmed")
            << " posecut_last=";
    if (mTelemetry.havePosecutLast)
        message << (mTelemetry.posecutLastPresented ? "presented" : "internal")
                << ":d=" << mTelemetry.posecutLastDistance << "m,a="
                << mTelemetry.posecutLastAngleDegrees << "deg,serial"
                << (mTelemetry.posecutLastSerialDelta >= 0 ? "+" : "")
                << mTelemetry.posecutLastSerialDelta;
    else
        message << "none";
    message << " edges=C2W:" << mTelemetry.edges[0] << " W2C:" << mTelemetry.edges[1]
            << " D2W:" << mTelemetry.edges[2] << " W2D:" << mTelemetry.edges[3]
            << " W2O:" << mTelemetry.edges[4] << " O2W:" << mTelemetry.edges[5]
            << " extreme=" << mExtremeCount << " degenerate=" << mDegenerateCount;
    out.log = message.str();
    out.haveTelemetry = true;
    out.telemetry = mTelemetry;
    mReportTime = 0.0;
    mReportFrames = 0;
    mReportElapsed = 0.0;
    mTrackedDistance = 0.0;
    mTrackedTargetDistance = 0.0;
    mTrackedAngle = 0.0;
    mTrackedTargetAngle = 0.0;
}

FrameOut Coordinator::frame(const FrameIn& in)
{
    FrameOut result;
    mGuardArmed = in.settings.guardArmed && !mStormDisarmed;
    const S32 weightedEdgeIndex = edgeIndex(in.weightedEdge);
    if (weightedEdgeIndex >= 0)
        ++mTelemetry.edges[static_cast<std::size_t>(weightedEdgeIndex)];

    const bool outgoingToDrone = in.weightedEdge == ALFlycamWeightedEdge::W2D &&
        !in.foreignDroneRegain;
    if (in.weightedEdge == ALFlycamWeightedEdge::D2W)
    {
        mPendingSeed = in.tagged && in.foreignSinceWrite
            ? PendingSeed::REST : PendingSeed::FROM_HISTORY;
        mHaveLastWrite = false;
        return result;
    }
    if (in.weightedEdge == ALFlycamWeightedEdge::W2C ||
        in.weightedEdge == ALFlycamWeightedEdge::W2O ||
        (in.weightedEdge == ALFlycamWeightedEdge::W2D && in.foreignDroneRegain))
    {
        deactivate(mState);
        mHaveLastWrite = false;
        mLastPresentedComparable = false;
        if (in.weightedEdge == ALFlycamWeightedEdge::W2C && in.classicIn.cursor3D)
            result.log = "WEIGHTED INACTIVE 3d-cursor";
        return result;
    }
    if (in.frameOwner != ALFlycamPoseOwner::WEIGHTED && !outgoingToDrone)
        return result;

    const F64 rawStep = in.frameIntervalSeconds;
    const bool degenerate = !std::isfinite(rawStep) || rawStep < 1.0e-4;
    const F64 h = degenerate ? 0.0 : std::min(rawStep, 0.25);
    if (!degenerate)
    {
        mWeightedTime += h;
        mReportTime += h;
        mReportElapsed += h;
        ++mReportFrames;
    }

    Target target;
    if (!degenerate)
        target = targetFromClassic(in.delta.data(), in.entryRot, h, in.classicIm.isZero);

    if (in.weightedEdge == ALFlycamWeightedEdge::C2W ||
        in.weightedEdge == ALFlycamWeightedEdge::O2W)
    {
        if (in.weightedEdge == ALFlycamWeightedEdge::C2W)
            ++mTelemetry.reseedC2W;
        if (degenerate || isExtremeInput(target, h))
            rest(mState);
        else
            seedAtTarget(mState, target);
        mHaveLastWrite = false;
        mLastPresentedComparable = false;
    }

    if (mPendingSeed != PendingSeed::NONE)
    {
        if (mPendingSeed == PendingSeed::FROM_HISTORY && in.prevPose.valid && in.curPose.valid &&
            in.prevPose.positionGlobal.isFinite() && in.curPose.positionGlobal.isFinite() &&
            in.prevPose.rotation.isFinite() && in.curPose.rotation.isFinite() &&
            in.curPose.dt > .0005 && in.curPose.dt <= .25 && std::isfinite(in.curPose.dt))
        {
            const LLVector3d velocity = (in.curPose.positionGlobal - in.prevPose.positionGlobal) *
                (1.0 / in.curPose.dt);
            const LLVector3 angularF32 = alCameraAngularVelocity(
                in.prevPose.rotation, in.curPose.rotation, static_cast<F32>(in.curPose.dt));
            const LLVector3d angularVelocity(angularF32);
            Target historyTarget;
            historyTarget.v = velocity;
            historyTarget.w = angularVelocity;
            if (isExtremeInput(historyTarget, in.curPose.dt))
                rest(mState);
            else
                seedVelocity(mState, velocity, angularVelocity);
        }
        else
        {
            rest(mState);
        }
        mPendingSeed = PendingSeed::NONE;
    }
    else if (!mState.active && !degenerate && !isExtremeInput(target, h))
    {
        seedAtTarget(mState, target);
    }

    if (in.weightedEdge == ALFlycamWeightedEdge::NONE)
    {
        bool provenCut = false;
        if (mHaveOrigin && in.originGlobal != mOriginGlobal)
        {
            provenCut = true;
            ++mTelemetry.reseedOrigin;
        }
        if (in.tagged)
        {
            const ALFlycamDispatchGap gap = alClassifyFlycamDispatchGap(
                in.dispatchSerial, in.lastWrittenSerial, in.foreignSinceWrite).gap;
            if (gap != ALFlycamDispatchGap::NONE)
            {
                provenCut = true;
                if (gap == ALFlycamDispatchGap::FOREIGN)
                    ++mTelemetry.reseedForeign;
                else
                    ++mTelemetry.reseedSelf;
            }
        }
        if (mHaveLastWrite && poseMismatch(
                in.entryPos, in.entryRot, mLastWritePos, mLastWriteRot))
        {
            provenCut = true;
            ++mInternalCuts;
            mTelemetry.posecutInternal = mInternalCuts;
            mTelemetry.havePosecutLast = true;
            mTelemetry.posecutLastPresented = false;
            mTelemetry.posecutLastDistance = static_cast<F64>((in.entryPos - mLastWritePos).length());
            mTelemetry.posecutLastAngleDegrees = quaternionDistance(
                in.entryRot, mLastWriteRot) * 180.0 / PI_D;
            mTelemetry.posecutLastSerialDelta = static_cast<S64>(in.dispatchSerial) -
                static_cast<S64>(in.lastWrittenSerial);
        }
        if (provenCut)
            rest(mState);
    }
    mOriginGlobal = in.originGlobal;
    mHaveOrigin = true;

    if (mHaveLastWrite && mLastPresentedComparable && in.presentedComparable &&
        poseMismatch(in.presentedPos, in.presentedRot, mLastWritePos, mLastWriteRot))
    {
        ++mPresentedCutCount;
        mTelemetry.posecutPresented = mPresentedCutCount;
        mTelemetry.havePosecutLast = true;
        mTelemetry.posecutLastPresented = true;
        mTelemetry.posecutLastDistance = static_cast<F64>((in.presentedPos - mLastWritePos).length());
        mTelemetry.posecutLastAngleDegrees = quaternionDistance(
            in.presentedRot, mLastWriteRot) * 180.0 / PI_D;
        mTelemetry.posecutLastSerialDelta = static_cast<S64>(in.dispatchSerial) -
            static_cast<S64>(in.lastWrittenSerial);
        mPresentedCuts.push_back(mWeightedTime);
        while (!mPresentedCuts.empty() && mPresentedCuts.front() < mWeightedTime - 5.0)
            mPresentedCuts.erase(mPresentedCuts.begin());
        if (mPresentedCuts.size() > 10)
        {
            mStormDisarmed = true;
            mGuardArmed = false;
        }
        else if (mGuardArmed)
        {
            rest(mState);
        }
    }
    else if (mHaveLastWrite && (!mLastPresentedComparable || !in.presentedComparable))
    {
        ++mPresentedNotComparable;
        mTelemetry.posecutNotComparable = mPresentedNotComparable;
    }

    LLQuaternion outputRotation = in.entryRot;
    LLVector3 outputPosition = in.entryPos;
    Step step;
    if (degenerate)
    {
        ++mDegenerateCount;
        mTelemetry.degenerate = mDegenerateCount;
    }
    else if (isExtremeInput(target, h))
    {
        ++mExtremeCount;
        mTelemetry.extreme = mExtremeCount;
        rest(mState);
        if (outgoingToDrone)
            deactivate(mState);
        report(in, result);
        return result;
    }
    else
    {
        const Tuning tuning = tuningForWeight(in.settings.weight, in.settings.tauMax,
            in.settings.curve, in.settings.rotationShare);
        const bool wasSettled = mState.settled;
        step = advance(mState, target,
            tuning, h);
        if (step.nonFinite)
        {
            ++mNonFiniteCount;
            mTelemetry.nonFinite = mNonFiniteCount;
            if (outgoingToDrone)
                deactivate(mState);
            report(in, result);
            return result;
        }
        const F64 targetSpeed = vectorLength(target.v);
        const F64 targetAngularSpeed = vectorLength(target.w);
        mTelemetry.velocityMaximum = std::max(mTelemetry.velocityMaximum, vectorLength(mState.vT));
        mTelemetry.velocityTargetMaximum = std::max(mTelemetry.velocityTargetMaximum, targetSpeed);
        mTelemetry.angularMaximum = std::max(mTelemetry.angularMaximum, vectorLength(mState.vR));
        mTelemetry.angularTargetMaximum = std::max(mTelemetry.angularTargetMaximum, targetAngularSpeed);
        if (targetSpeed > VELOCITY_EPSILON &&
            std::fabs(vectorLength(mState.vT) - targetSpeed) <= targetSpeed * 0.1)
        {
            mTrackedDistance += vectorLength(step.dp);
            mTrackedTargetDistance += targetSpeed * h;
        }
        if (targetAngularSpeed > ANGULAR_EPSILON &&
            std::fabs(vectorLength(mState.vR) - targetAngularSpeed) <= targetAngularSpeed * 0.1)
        {
            mTrackedAngle += vectorLength(step.dtheta);
            mTrackedTargetAngle += targetAngularSpeed * h;
        }
        if (targetAtRest(target) && !wasSettled && !mCoasting)
        {
            mCoasting = true;
            mCoastStart = mWeightedTime - h;
            const F64 translationTau = vectorLength(mState.uT + mState.vT) > 0.0
                ? tuning.tauT : 0.0;
            const F64 rotationTau = vectorLength(mState.uR + mState.vR) > 0.0
                ? tuning.tauR : 0.0;
            mCoastTau = std::max(translationTau, rotationTau);
        }
        if (step.settledNow)
        {
            ++mTelemetry.settles;
            mTelemetry.lastCoastSeconds = mCoasting ? mWeightedTime - mCoastStart : 0.0;
            mTelemetry.lastCoastTau = mCoastTau > 0.0
                ? mTelemetry.lastCoastSeconds / mCoastTau : 0.0;
            mCoasting = false;
        }
        if (mCoasting && mCoastTau > 0.0 &&
            mWeightedTime - mCoastStart > 20.0 * mCoastTau)
            mDrift = true;
        outputPosition += LLVector3(step.dp);
        outputRotation = rotationFromVector(step.dtheta) * in.entryRot;
    }

    outputRotation = classicLevel(outputRotation, in);
    if (in.classicIn.levelHorizon ||
        (in.classicIn.blackDragon && in.classicIn.rollDefault == 1))
        zeroRoll(mState);

    const LLVector3 localTranslation = LLVector3(step.dp) * ~in.entryRot;
    result.operatorDelta[0] = localTranslation.mV[VX];
    result.operatorDelta[1] = localTranslation.mV[VY];
    result.operatorDelta[2] = localTranslation.mV[VZ];
    result.operatorDelta[3] = static_cast<F32>(step.dtheta.mdV[VX]);
    result.operatorDelta[4] = static_cast<F32>(step.dtheta.mdV[VY]);
    result.operatorDelta[5] = static_cast<F32>(step.dtheta.mdV[VZ]);
    result.writePose = true;
    result.pos = outputPosition;
    result.rot = outputRotation;
    mLastWritePos = outputPosition;
    mLastWriteRot = outputRotation;
    mHaveLastWrite = true;
    mLastPresentedComparable = in.presentedComparable;

    report(in, result);

    if (outgoingToDrone)
        deactivate(mState);
    return result;
}
}
