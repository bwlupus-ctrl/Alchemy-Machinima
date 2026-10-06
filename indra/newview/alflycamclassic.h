/**
 * @file alflycamclassic.h
 * @brief Extracted Classic flycam integration.
 */

#ifndef AL_FLYCAM_CLASSIC_H
#define AL_FLYCAM_CLASSIC_H

#include "llquaternion.h"
#include "v3math.h"
#include "stdtypes.h"

#include <array>

namespace ALFlycamClassic
{
constexpr U32 AXIS_COUNT = 7;

struct Input
{
    std::array<F32, AXIS_COUNT> axis;
    std::array<F32, AXIS_COUNT> axisDeadZone;
    std::array<F32, AXIS_COUNT> axisScale;

    F32 frameIntervalSeconds;
    F32 flycamFeathering;
    F32 buildModeScale;
    F32 minView;
    F32 maxView;
    F32 cameraAngle;
    F32 triggerRoll;
    F32 triggerThrottle;

    U32 zoomDefault;
    U32 zoomOut;
    U32 zoomIn;
    U32 jump;
    U32 crouch;
    U32 rollLeft;
    U32 rollRight;
    U32 rollDefault;

    S32 xiMode;
    bool blackDragon;
    bool cursor3D;
    bool xiTakeover;
    bool buildMode;
    bool invertPitch;
    bool autoLeveling;
    bool zoomDirect;
    bool levelHorizon;
};

struct State
{
    std::array<F32, AXIS_COUNT>& lastDelta;
    std::array<F32, AXIS_COUNT>& delta;
    LLVector3& flycamPosition;
    LLQuaternion& flycamRotation;
    F32& flycamZoom;
};

struct Intermediate
{
    bool blackDragon;
    bool isZero;
    F32 time;
    F32 feather;
};

// The split keeps caller-owned AFK handling at its original point between
// delta integration and pose/zoom integration.
Intermediate integrateDeltas(const Input& in, State& state);
void finishIntegration(const Input& in, const Intermediate& intermediate, State& state);
}

#endif // AL_FLYCAM_CLASSIC_H
