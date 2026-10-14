/**
 * @file alflycamweighted.h
 * @brief Pure Weighted Classic flycam mass layer and frame coordinator.
 */
#ifndef AL_FLYCAM_WEIGHTED_H
#define AL_FLYCAM_WEIGHTED_H

#include "llmath.h"
#include "llquaternion.h"
#include "v3dmath.h"
#include "v3math.h"
#include "stdtypes.h"

#include "alcameradispatch.h"
#include "alflycamclassic.h"

#include <array>
#include <string>
#include <vector>

namespace ALFlycamWeighted
{
struct Tuning
{
    F64 tauT = 0.0;
    F64 tauR = 0.0;
};

struct Target
{
    LLVector3d v;
    LLVector3d w;
    bool inputZero = true;
};

struct State
{
    LLVector3d uT;
    LLVector3d vT;
    LLVector3d uR;
    LLVector3d vR;
    F64 brakeReferenceT = 0.0;
    F64 brakeReferenceR = 0.0;
    bool brakeLatched = false;
    bool active = false;
    bool settled = true;
};

struct Step
{
    LLVector3d dp;
    LLVector3d dtheta;
    bool settledNow = false;
    bool woke = false;
    bool held = false;
    bool nonFinite = false;
};

struct TriggerMomentum
{
    bool enabled = false;
    F32 gas = 0.f;
    F32 brake = 0.f;
    F32 gasResponse = .35f;
    F32 brakeStopTime = 1.f;
    F32 brakeRotation = 1.f;
    F32 impact = 1.f;
};

Tuning tuningForWeight(F32 weightPercent, F32 tauMax, F32 curveK, F32 rotationShare);
Target targetFromClassic(const F32 d[6], const LLQuaternion& q0, F64 h, bool inputZero);
bool isExtremeInput(const Target& target, F64 h);
Step advance(State& state, const Target& target, const Tuning& tuning, F64 h);
Step advance(State& state, const Target& target, const Tuning& tuning, F64 h,
             const TriggerMomentum& trigger);
void seedAtTarget(State& state, const Target& target);
void seedVelocity(State& state, const LLVector3d& velocity, const LLVector3d& angularVelocity);
void rest(State& state);
void deactivate(State& state);
void zeroRoll(State& state);
LLQuaternion rotationFromVector(const LLVector3d& rotationVector);
F32 presetWeight(S32 preset);
S32 presetForWeight(F32 weightPercent);
const char* presetName(S32 preset);

struct PoseSample
{
    LLVector3d positionGlobal;
    LLQuaternion rotation;
    F64 dt = 0.0;
    bool valid = false;
};

struct Settings
{
    F32 weight = 50.f;
    F32 tauMax = 1.f;
    F32 curve = 20.f;
    F32 rotationShare = .6f;
    bool guardArmed = false;
    TriggerMomentum trigger;
};

struct FrameIn
{
    ALFlycamPoseOwner frameOwner = ALFlycamPoseOwner::CLASSIC;
    ALFlycamWeightedEdge weightedEdge = ALFlycamWeightedEdge::NONE;
    bool foreignDroneRegain = false;
    bool tagged = false;
    U64 dispatchSerial = 0;
    U64 lastWrittenSerial = 0;
    bool foreignSinceWrite = false;
    LLVector3d originGlobal;
    LLVector3 entryPos;
    LLQuaternion entryRot;
    LLVector3 classicPos;
    LLQuaternion classicRot;
    F32 zoom = 0.f;
    std::array<F32, 7> delta{};
    std::array<F32, 7> lastDelta{};
    ALFlycamClassic::Input classicIn{};
    ALFlycamClassic::Intermediate classicIm{};
    F64 frameIntervalSeconds = 0.0;
    PoseSample prevPose;
    PoseSample curPose;
    LLVector3 presentedPos;
    LLQuaternion presentedRot;
    bool presentedComparable = false;
    Settings settings;
};

struct FrameOut
{
    bool writePose = false;
    LLVector3 pos;
    LLQuaternion rot;
    std::array<F32, 6> operatorDelta{};
    std::string log;
    struct Telemetry
    {
        enum class Verdict { NONFINITE, POSECUT_STORM, EXTREME, DRIFT, TRACK, BYPASS, OK };

        Verdict verdict = Verdict::OK;
        F32 weight = 0.f;
        S32 preset = 4;
        Tuning tuning;
        F64 fps = 0.0;
        F64 velocityMaximum = 0.0;
        F64 velocityTargetMaximum = 0.0;
        F64 angularMaximum = 0.0;
        F64 angularTargetMaximum = 0.0;
        F64 trackingErrorPercent = 0.0;
        U32 settles = 0;
        F64 lastCoastSeconds = 0.0;
        F64 lastCoastTau = 0.0;
        U32 reseedForeign = 0;
        U32 reseedSelf = 0;
        U32 reseedOrigin = 0;
        U32 reseedC2W = 0;
        U32 posecutInternal = 0;
        U32 posecutPresented = 0;
        U32 posecutNotComparable = 0;
        bool guardArmed = false;
        bool havePosecutLast = false;
        bool posecutLastPresented = false;
        F64 posecutLastDistance = 0.0;
        F64 posecutLastAngleDegrees = 0.0;
        S64 posecutLastSerialDelta = 0;
        std::array<U32, 6> edges{};
        U32 extreme = 0;
        U32 degenerate = 0;
        U32 nonFinite = 0;
    };
    bool haveTelemetry = false;
    Telemetry telemetry;
};

LLQuaternion classicLevel(const LLQuaternion& rotation, const FrameIn& in);

class Coordinator
{
public:
    FrameOut frame(const FrameIn& in);
    const State& state() const { return mState; }
    bool guardArmed() const { return mGuardArmed; }
    const FrameOut::Telemetry& telemetry() const { return mTelemetry; }

private:
    enum class PendingSeed { NONE, FROM_HISTORY, REST };
    void report(const FrameIn& in, FrameOut& out);

    State mState;
    PendingSeed mPendingSeed = PendingSeed::NONE;
    bool mHaveOrigin = false;
    LLVector3d mOriginGlobal;
    bool mHaveLastWrite = false;
    LLVector3 mLastWritePos;
    LLQuaternion mLastWriteRot;
    bool mLastPresentedComparable = false;
    bool mGuardArmed = false;
    bool mStormDisarmed = false;
    F64 mWeightedTime = 0.0;
    F64 mReportTime = 0.0;
    std::vector<F64> mPresentedCuts;
    U32 mInternalCuts = 0;
    U32 mPresentedCutCount = 0;
    U32 mPresentedNotComparable = 0;
    U32 mNonFiniteCount = 0;
    U32 mExtremeCount = 0;
    U32 mDegenerateCount = 0;
    FrameOut::Telemetry mTelemetry;
    U32 mReportFrames = 0;
    F64 mReportElapsed = 0.0;
    F64 mTrackedDistance = 0.0;
    F64 mTrackedTargetDistance = 0.0;
    F64 mTrackedAngle = 0.0;
    F64 mTrackedTargetAngle = 0.0;
    bool mCoasting = false;
    F64 mCoastStart = 0.0;
    F64 mCoastTau = 0.0;
    bool mDrift = false;
};
}

#endif // AL_FLYCAM_WEIGHTED_H
