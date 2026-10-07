/**
 * @file alcameradispatch.h
 * @brief Per-idle camera ownership tags and final-camera sample history.
 */
#ifndef AL_CAMERA_DISPATCH_H
#define AL_CAMERA_DISPATCH_H

#include "llmath.h"
#include "llquaternion.h"
#include "stdtypes.h"
#include "v3dmath.h"
#include "v3math.h"

#include <array>
#include <cmath>

enum class ECameraOwner
{
    PILOT,
    RECORDER,
    PATH,
    CINE,
    FLYCAM,
    AGENT
};

enum class ALFlycamPoseOwner
{
    CLASSIC,
    DRONE,
    ORBIT,
    WEIGHTED
};

enum class ALFlycamOwnerEdge
{
    NONE,
    CLASSIC_TO_DRONE,
    DRONE_TO_CLASSIC,
    CLASSIC_TO_ORBIT,
    ORBIT_TO_CLASSIC,
    DRONE_TO_ORBIT,
    ORBIT_TO_DRONE,
    CLASSIC_TO_WEIGHTED,
    WEIGHTED_TO_CLASSIC
};

enum class ALFlycamWeightedEdge
{
    NONE,
    C2W,
    W2C,
    W2D,
    D2W,
    W2O,
    O2W
};

struct ALFlycamFrameDecision
{
    ALFlycamPoseOwner selectedOwner = ALFlycamPoseOwner::CLASSIC;
    ALFlycamPoseOwner frameOwner = ALFlycamPoseOwner::CLASSIC;
    ALFlycamOwnerEdge edge = ALFlycamOwnerEdge::NONE;
    bool resetOperator = false;
    bool discardDroneAccumulator = false;
    bool discardClassicShadowPose = false;
    bool seedDrone = false;
    bool foreignDroneRegain = false;
    ALFlycamWeightedEdge weightedEdge = ALFlycamWeightedEdge::NONE;
};

inline S32 alEffectiveFlycamModel(S32 model, F32 weightPercent)
{
    if (model == 1)
        return 1;
    if (model == 2 && std::isfinite(weightPercent) && weightPercent > 0.f)
        return 2;
    return 0;
}

// This is the pre-Weighted decision function, kept as a literal frozen core so
// models 0/1 retain every legacy field exactly. Weighted is overlaid below.
inline ALFlycamFrameDecision alResolveLegacyFlycamFrame(
    S32 modelSetting, bool cursor3D, bool orbitResolved,
    ALFlycamPoseOwner previousOwner, bool taggedDispatch,
    bool foreignSinceFlycamWrite)
{
    ALFlycamFrameDecision result;
    const bool droneSelected = modelSetting == 1 && !cursor3D;
    result.selectedOwner = orbitResolved ? ALFlycamPoseOwner::ORBIT :
        (droneSelected ? ALFlycamPoseOwner::DRONE : ALFlycamPoseOwner::CLASSIC);
    result.frameOwner = result.selectedOwner;
    result.foreignDroneRegain = taggedDispatch && foreignSinceFlycamWrite &&
        result.selectedOwner == ALFlycamPoseOwner::DRONE;

    if (previousOwner == result.selectedOwner)
    {
        result.discardDroneAccumulator = result.frameOwner == ALFlycamPoseOwner::ORBIT;
        result.seedDrone = result.foreignDroneRegain;
        result.resetOperator = result.foreignDroneRegain;
        return result;
    }

    if (previousOwner == ALFlycamPoseOwner::CLASSIC &&
        result.selectedOwner == ALFlycamPoseOwner::DRONE)
    {
        result.edge = ALFlycamOwnerEdge::CLASSIC_TO_DRONE;
        result.frameOwner = result.foreignDroneRegain
            ? ALFlycamPoseOwner::DRONE : ALFlycamPoseOwner::CLASSIC;
        result.seedDrone = true;
        result.resetOperator = true;
    }
    else if (previousOwner == ALFlycamPoseOwner::DRONE &&
             result.selectedOwner == ALFlycamPoseOwner::CLASSIC)
    {
        result.edge = ALFlycamOwnerEdge::DRONE_TO_CLASSIC;
        result.frameOwner = ALFlycamPoseOwner::DRONE;
        result.resetOperator = true;
    }
    else if (previousOwner == ALFlycamPoseOwner::CLASSIC &&
             result.selectedOwner == ALFlycamPoseOwner::ORBIT)
    {
        result.edge = ALFlycamOwnerEdge::CLASSIC_TO_ORBIT;
    }
    else if (previousOwner == ALFlycamPoseOwner::ORBIT &&
             result.selectedOwner == ALFlycamPoseOwner::CLASSIC)
    {
        result.edge = ALFlycamOwnerEdge::ORBIT_TO_CLASSIC;
    }
    else if (previousOwner == ALFlycamPoseOwner::DRONE &&
             result.selectedOwner == ALFlycamPoseOwner::ORBIT)
    {
        result.edge = ALFlycamOwnerEdge::DRONE_TO_ORBIT;
        result.resetOperator = true;
        result.discardClassicShadowPose = true;
    }
    else if (previousOwner == ALFlycamPoseOwner::ORBIT &&
             result.selectedOwner == ALFlycamPoseOwner::DRONE)
    {
        result.edge = ALFlycamOwnerEdge::ORBIT_TO_DRONE;
        result.seedDrone = true;
        result.resetOperator = true;
    }

    result.discardDroneAccumulator = result.frameOwner == ALFlycamPoseOwner::ORBIT;
    return result;
}

// Pure per-frame ownership decision. Orbit takes/releases ownership on the
// resolveAnchor frame; only Classic<->Drone model changes use the outgoing
// owner for their edge frame.
inline ALFlycamFrameDecision alResolveFlycamFrame(
    S32 modelSetting, bool cursor3D, bool orbitResolved,
    ALFlycamPoseOwner previousOwner, bool taggedDispatch,
    bool foreignSinceFlycamWrite)
{
    const S32 legacyModel = modelSetting == 2 ? 0 : modelSetting;
    const ALFlycamPoseOwner legacyPrevious = previousOwner == ALFlycamPoseOwner::WEIGHTED
        ? ALFlycamPoseOwner::CLASSIC : previousOwner;
    ALFlycamFrameDecision result = alResolveLegacyFlycamFrame(
        legacyModel, cursor3D, orbitResolved, legacyPrevious,
        taggedDispatch, foreignSinceFlycamWrite);

    const bool weightedSelected = modelSetting == 2 && !cursor3D && !orbitResolved;
    if (weightedSelected)
    {
        result.selectedOwner = ALFlycamPoseOwner::WEIGHTED;
        if (previousOwner == ALFlycamPoseOwner::CLASSIC)
        {
            result.frameOwner = ALFlycamPoseOwner::WEIGHTED;
            result.edge = ALFlycamOwnerEdge::CLASSIC_TO_WEIGHTED;
            result.weightedEdge = ALFlycamWeightedEdge::C2W;
        }
        else if (previousOwner == ALFlycamPoseOwner::DRONE)
        {
            result.frameOwner = ALFlycamPoseOwner::DRONE;
            result.weightedEdge = ALFlycamWeightedEdge::D2W;
        }
        else if (previousOwner == ALFlycamPoseOwner::ORBIT)
        {
            result.frameOwner = ALFlycamPoseOwner::WEIGHTED;
            result.weightedEdge = ALFlycamWeightedEdge::O2W;
        }
        else
        {
            result.frameOwner = ALFlycamPoseOwner::WEIGHTED;
        }
    }
    else if (previousOwner == ALFlycamPoseOwner::WEIGHTED)
    {
        if (result.selectedOwner == ALFlycamPoseOwner::DRONE)
        {
            result.edge = ALFlycamOwnerEdge::CLASSIC_TO_DRONE;
            result.frameOwner = result.foreignDroneRegain
                ? ALFlycamPoseOwner::DRONE : ALFlycamPoseOwner::WEIGHTED;
            result.weightedEdge = ALFlycamWeightedEdge::W2D;
        }
        else if (result.selectedOwner == ALFlycamPoseOwner::ORBIT)
        {
            result.edge = ALFlycamOwnerEdge::CLASSIC_TO_ORBIT;
            result.frameOwner = ALFlycamPoseOwner::ORBIT;
            result.weightedEdge = ALFlycamWeightedEdge::W2O;
        }
        else
        {
            result.edge = ALFlycamOwnerEdge::WEIGHTED_TO_CLASSIC;
            result.frameOwner = ALFlycamPoseOwner::CLASSIC;
            result.weightedEdge = ALFlycamWeightedEdge::W2C;
        }
    }
    return result;
}

struct ALCameraDispatchSample
{
    U64 serial = 0;
    ECameraOwner owner = ECameraOwner::AGENT;
    F32 dt = 0.f;
    LLVector3d positionGlobal;
    LLQuaternion rotation;
    LLVector3 xAxis;
    LLVector3 yAxis;
    LLVector3 zAxis;
    bool flycamWrote = false;
    U32 frameCountAtSample = 0;
    bool presented = false;
};

enum class ALFlycamDispatchGap
{
    NONE,
    SELF,
    FOREIGN
};

struct ALFlycamGapDecision
{
    ALFlycamDispatchGap gap = ALFlycamDispatchGap::NONE;
    bool reseed = false;
    bool resetOperator = false;
};

// Pure dispatch-clock classification.  The display clock is deliberately not
// an input: minimized idles still form SELF gaps, while snapshot display passes
// cannot form a gap.
inline ALFlycamGapDecision alClassifyFlycamDispatchGap(
    U64 dispatchSerial, U64 lastWrittenSerial, bool foreignSinceFlycamWrite)
{
    ALFlycamGapDecision result;
    const bool serialGap = dispatchSerial != lastWrittenSerial + 1;
    if (foreignSinceFlycamWrite)
    {
        result.gap = ALFlycamDispatchGap::FOREIGN;
        result.reseed = true;
        result.resetOperator = true;
    }
    else if (serialGap)
    {
        result.gap = ALFlycamDispatchGap::SELF;
        result.reseed = true;
    }
    return result;
}

enum class ALFlycamKeepReject
{
    NONE,
    STALE,
    STRADDLE,
    UNPRESENTED,
    DT,
    NONFINITE,
    CUT,
    INCONSISTENT
};

inline const char* alFlycamKeepRejectName(ALFlycamKeepReject reason)
{
    switch (reason)
    {
    case ALFlycamKeepReject::STALE:        return "stale";
    case ALFlycamKeepReject::STRADDLE:     return "straddle";
    case ALFlycamKeepReject::UNPRESENTED: return "unpresented";
    case ALFlycamKeepReject::DT:           return "dt";
    case ALFlycamKeepReject::NONFINITE:    return "nonfinite";
    case ALFlycamKeepReject::CUT:          return "cut";
    case ALFlycamKeepReject::INCONSISTENT: return "inconsistent";
    default:                               return "";
    }
}

struct ALFlycamKeepResult
{
    bool ok = false;
    LLVector3 velocity;
    LLVector3 angularVelocity;
    ALFlycamKeepReject reason = ALFlycamKeepReject::NONE;
};

inline LLVector3 alCameraAngularVelocity(
    const LLQuaternion& from, const LLQuaternion& to, F32 dt)
{
    if (!(dt > 0.f) || !std::isfinite(dt) || !from.isFinite() || !to.isFinite())
        return LLVector3::zero;
    LLQuaternion delta = to * ~from;
    if (delta.mQ[VW] < 0.f)
        delta = -delta;
    delta.normalize();
    F32 angle = 0.f;
    LLVector3 axis;
    delta.getAngleAxis(&angle, axis);
    return axis * (angle / dt);
}

// Estimate the velocity at the end of S-1 from samples S-3..S-1.  Linear
// limits are supplied unscaled; buildScale is applied exactly once here.
inline ALFlycamKeepResult alEstimateFlycamKeep(
    const std::array<ALCameraDispatchSample, 3>& samples,
    U64 dispatchSerial, F32 velocityCap, F32 totalLimit,
    F32 yawRate, F32 yawAcceleration, F32 buildScale)
{
    ALFlycamKeepResult result;
    const ALCameraDispatchSample& a = samples[0];
    const ALCameraDispatchSample& b = samples[1];
    const ALCameraDispatchSample& c = samples[2];
    if (a.serial + 1 != b.serial || b.serial + 1 != c.serial ||
        c.serial + 1 != dispatchSerial)
        result.reason = ALFlycamKeepReject::STALE;
    else if (a.owner != b.owner || b.owner != c.owner ||
             c.owner == ECameraOwner::FLYCAM)
        result.reason = ALFlycamKeepReject::STRADDLE;
    else if (!a.presented || !b.presented || !c.presented)
        result.reason = ALFlycamKeepReject::UNPRESENTED;
    else if (!(a.dt > .0005f && a.dt <= .25f &&
               b.dt > .0005f && b.dt <= .25f &&
               c.dt > .0005f && c.dt <= .25f))
        result.reason = ALFlycamKeepReject::DT;
    else if (!a.positionGlobal.isFinite() || !b.positionGlobal.isFinite() ||
             !c.positionGlobal.isFinite() || !a.rotation.isFinite() ||
             !b.rotation.isFinite() || !c.rotation.isFinite())
        result.reason = ALFlycamKeepReject::NONFINITE;
    else
    {
        const LLVector3 va = LLVector3(b.positionGlobal - a.positionGlobal) / b.dt;
        const LLVector3 vb = LLVector3(c.positionGlobal - b.positionGlobal) / c.dt;
        const F32 meanDt = (b.dt + c.dt) * .5f;
        const LLVector3 acceleration = (vb - va) / meanDt;
        const LLVector3 omegaA = alCameraAngularVelocity(a.rotation, b.rotation, b.dt);
        const LLVector3 omegaB = alCameraAngularVelocity(b.rotation, c.rotation, c.dt);
        const LLVector3 angularAcceleration = (omegaB - omegaA) / meanDt;
        if (!va.isFinite() || !vb.isFinite() || !acceleration.isFinite() ||
            !omegaA.isFinite() || !omegaB.isFinite() ||
            !angularAcceleration.isFinite())
            result.reason = ALFlycamKeepReject::NONFINITE;
        else if (vb.length() > velocityCap * buildScale ||
                 omegaB.length() > 2.f * yawRate)
            result.reason = ALFlycamKeepReject::CUT;
        else if (acceleration.length() > 1.5f * totalLimit * buildScale ||
                 angularAcceleration.length() > 2.f * yawAcceleration)
            result.reason = ALFlycamKeepReject::INCONSISTENT;
        else
        {
            result.ok = true;
            result.velocity = vb + acceleration * (c.dt * .5f);
            result.angularVelocity = omegaB;
        }
    }
    return result;
}

extern U64 gCameraDispatchSerial;
extern ECameraOwner gCameraDispatchOwner;
extern bool gCameraDispatchFlycamWrote;
extern bool gCameraDispatchForeignSinceFlycamWrite;
extern std::array<ALCameraDispatchSample, 4> gCameraDispatchSamples;
extern U32 gCameraDispatchSampleStart;
extern U32 gCameraDispatchSampleCount;

// Finalize the newest sample against the display clock.  Sampling calls this
// before appending; a regain check calls it for the final foreign sample.
void alCameraDispatchFinalizePresented();
void alCameraDispatchSampleFinal();

#endif
