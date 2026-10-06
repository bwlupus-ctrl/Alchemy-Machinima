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
extern U32 gCameraDispatchSampleCount;

// Finalize the newest sample against the display clock.  Sampling calls this
// before appending; a regain check calls it for the final foreign sample.
void alCameraDispatchFinalizePresented();
void alCameraDispatchSampleFinal();

#endif
