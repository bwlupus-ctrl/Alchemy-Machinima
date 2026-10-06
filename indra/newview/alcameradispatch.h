/**
 * @file alcameradispatch.h
 * @brief Per-idle camera ownership tags and final-camera sample history.
 */
#ifndef AL_CAMERA_DISPATCH_H
#define AL_CAMERA_DISPATCH_H

#include "llquaternion.h"
#include "stdtypes.h"
#include "v3dmath.h"
#include "v3math.h"

#include <array>

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
    ORBIT
};

enum class ALFlycamOwnerEdge
{
    NONE,
    CLASSIC_TO_DRONE,
    DRONE_TO_CLASSIC,
    CLASSIC_TO_ORBIT,
    ORBIT_TO_CLASSIC,
    DRONE_TO_ORBIT,
    ORBIT_TO_DRONE
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
};

// Pure per-frame ownership decision. Orbit takes/releases ownership on the
// resolveAnchor frame; only Classic<->Drone model changes use the outgoing
// owner for their edge frame.
inline ALFlycamFrameDecision alResolveFlycamFrame(
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
