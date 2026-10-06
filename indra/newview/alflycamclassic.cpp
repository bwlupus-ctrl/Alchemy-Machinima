/**
 * @file alflycamclassic.cpp
 * @brief Extracted Classic flycam integration.
 */

#include "llviewerprecompiledheaders.h"

#include "alflycamclassic.h"

#define  X_I    1
#define  Y_I    2
#define  Z_I    0

namespace
{
constexpr U32 Z_AXIS = 2;       // mirrors E_Axes in llviewerjoystick.h
constexpr U32 CAM_X_AXIS = 3;
constexpr U32 CAM_Y_AXIS = 4;
constexpr U32 CAM_Z_AXIS = 5;
constexpr U32 CAM_W_AXIS = 6;
constexpr U32 MAX_AXES = 7;
constexpr U32 FLYCAM_AXIS_6 = 6;

ALFlycamClassic::Intermediate integrateBlackDragonDeltas(const ALFlycamClassic::Input& in,
                                                         ALFlycamClassic::State& state)
{
    F32 time = in.frameIntervalSeconds;

    //BD - Avoid making ridiculously big movements if there's a big drop in fps
    time = llclamp(time, 0.016f, 0.033f);

    F32 flycam_feather = in.flycamFeathering;
    F32 cur_delta[MAX_AXES];
    F32 max_angle = in.maxView;
    F32 min_angle = in.minView;

    //BD - Slam zoom back to default and kill any delta we might have.
    if (in.zoomDefault == 1)
    {
        state.flycamZoom = in.cameraAngle;
        state.delta[CAM_W_AXIS] = 0.0f;
    }

    //BD - Only smooth flycam zoom if we are not capping at the min/max otherwise the feathering
    //     ends up working against previous input, delaying zoom in movement when we just zoomed
    //     out beyond capped max for a bit and vise versa.
    if ((state.flycamZoom <= min_angle
        || state.flycamZoom >= max_angle))
    {
        flycam_feather = 3.0f;
    }

    bool is_zero = true;
    for (U32 i = 0; i < 7; i++)
    {
        cur_delta[i] = -in.axis[i];

        if (i == CAM_W_AXIS)
        {
            cur_delta[i] -= (F32)in.zoomOut;
            cur_delta[i] += (F32)in.zoomIn;
        }
        if (i == Z_AXIS)
        {
            cur_delta[i] += (F32)in.jump;
            cur_delta[i] -= (F32)in.crouch;
        }
        if (i == CAM_X_AXIS)
        {
            cur_delta[i] -= (F32)in.rollLeft;
            cur_delta[i] += (F32)in.rollRight;
        }

        F32 tmp = cur_delta[i];
        F32 axis_deadzone = in.axisDeadZone[i];
        if (in.cursor3D || llabs(cur_delta[i]) < axis_deadzone)
        {
            cur_delta[i] = cur_delta[i] - state.lastDelta[i];
        }
        state.lastDelta[i] = tmp;

        //BD - We assume that delta 1.0 is the maximum.
        if (llabs(cur_delta[i]) > axis_deadzone)
        {
            //BD - Clamp the delta between 1 and -1 while taking the deadzone into account.
            if (cur_delta[i] > 0)
            {
                cur_delta[i] = llclamp(cur_delta[i] - axis_deadzone, 0.f, 1.f - axis_deadzone);
            }
            else
            {
                cur_delta[i] = llclamp(cur_delta[i] + axis_deadzone, -1.f + axis_deadzone, 0.f);
            }
            //BD - Rescale the remaining delta to match the maximum to get a new clean 0 to 1 range.
            cur_delta[i] = cur_delta[i] / (1.f - axis_deadzone);
        }

        if (in.xiTakeover && in.xiMode == 0 && i == CAM_X_AXIS)
            cur_delta[i] = llclamp(cur_delta[i] + in.triggerRoll, -1.f, 1.f);
        // [XInputPad] Throttle scales movement AND pan/tilt/roll; zoom (CAM_W) excluded.
        if (in.xiTakeover && i <= CAM_Z_AXIS && in.triggerThrottle != 1.f)
            cur_delta[i] *= in.triggerThrottle;

        // We may want to scale camera movements up or down in build mode.
        // NOTE: this needs to remain after the deadzone calculation, otherwise
        // we have issues with flycam "jumping" when the build dialog is opened/closed  -Nyx
        if (in.buildMode)
        {
            if (i == X_I || i == Y_I || i == Z_I)
            {
                cur_delta[i] *= in.buildModeScale;
            }
        }

        cur_delta[i] *= in.axisScale[i];

        if (!in.cursor3D)
        {
            cur_delta[i] *= time;
        }

        state.delta[i] = state.delta[i] + (cur_delta[i] - state.delta[i]) * time * flycam_feather;
        is_zero = is_zero && (cur_delta[i] == 0.f);
    }

//  //BD - Invertable Pitch Controls
    if (in.invertPitch)
        cur_delta[CAM_Y_AXIS] = -cur_delta[CAM_Y_AXIS];

    return { true, is_zero, time, flycam_feather };
}

void finishBlackDragonIntegration(const ALFlycamClassic::Input& in,
                                  const ALFlycamClassic::Intermediate& intermediate,
                                  ALFlycamClassic::State& state)
{
    const F32 time = intermediate.time;
    const F32 flycam_feather = intermediate.feather;

    state.flycamPosition += LLVector3(state.delta.data()) * state.flycamRotation;
    LLMatrix3 rot_mat(state.delta[CAM_X_AXIS], state.delta[CAM_Y_AXIS], state.delta[CAM_Z_AXIS]);
    state.flycamRotation = LLQuaternion(rot_mat)*state.flycamRotation;

    if (in.levelHorizon || in.autoLeveling || in.rollDefault == 1)
    {
        LLMatrix3 level(state.flycamRotation);

        LLVector3 x = LLVector3(level.mMatrix[0]);
        LLVector3 y = LLVector3(level.mMatrix[1]);
        LLVector3 z = LLVector3(level.mMatrix[2]);

        y.mV[2] = 0.f;
        y.normVec();

        level.setRows(x,y,z);
        level.orthogonalize();

        LLQuaternion quat(level);
        LLQuaternion lerp = nlerp(llmin(flycam_feather * time, 1.f), state.flycamRotation, quat);
        state.flycamRotation = in.levelHorizon || in.rollDefault == 1 ? quat : lerp;
    }

    if (in.zoomDirect)
    {
        state.flycamZoom = state.lastDelta[CAM_W_AXIS] * in.axisScale[FLYCAM_AXIS_6] + in.axisScale[FLYCAM_AXIS_6];
    }
    else
    {
        //BD - We need to cap zoom otherwise it internally counts higher causing
        //     the zoom level to not react until that extra has been removed first.
        state.flycamZoom = llclamp(state.flycamZoom + state.delta[CAM_W_AXIS], in.minView, in.maxView);
    }

}

ALFlycamClassic::Intermediate integrateNonBlackDragonDeltas(const ALFlycamClassic::Input& in,
                                                            ALFlycamClassic::State& state)
{
    F32 time = in.frameIntervalSeconds;

    // avoid making ridiculously big movements if there's a big drop in fps
    if (time > .2f)
    {
        time = .2f;
    }

    F32 cur_delta[7];
    F32 feather = in.flycamFeathering;
    bool absolute = in.cursor3D;
    bool is_zero = true;

    for (U32 i = 0; i < 7; i++)
    {
        cur_delta[i] = -in.axis[i];

        if (!in.invertPitch && i == 4)
            cur_delta[i] = -cur_delta[i];

        F32 tmp = cur_delta[i];
        if (absolute)
        {
            cur_delta[i] = cur_delta[i] - state.lastDelta[i];
        }
        state.lastDelta[i] = tmp;

        if (cur_delta[i] > 0)
        {
            cur_delta[i] = llmax(cur_delta[i]- in.axisDeadZone[i], 0.f);
        }
        else
        {
            cur_delta[i] = llmin(cur_delta[i]+ in.axisDeadZone[i], 0.f);
        }

        if (in.xiTakeover && in.xiMode == 0 && i == CAM_X_AXIS)
            cur_delta[i] = llclamp(cur_delta[i] + in.triggerRoll, -1.f, 1.f);
        // [XInputPad] Throttle scales movement AND pan/tilt/roll; zoom (CAM_W) excluded.
        if (in.xiTakeover && i <= CAM_Z_AXIS && in.triggerThrottle != 1.f)
            cur_delta[i] *= in.triggerThrottle;

        // We may want to scale camera movements up or down in build mode.
        // NOTE: this needs to remain after the deadzone calculation, otherwise
        // we have issues with flycam "jumping" when the build dialog is opened/closed  -Nyx
        if (in.buildMode)
        {
            if (i == X_I || i == Y_I || i == Z_I)
            {
                cur_delta[i] *= in.buildModeScale;
            }
        }

        cur_delta[i] *= in.axisScale[i];

        if (!absolute)
        {
            cur_delta[i] *= time;
        }

        state.delta[i] = state.delta[i] + (cur_delta[i]-state.delta[i])*time*feather;

        is_zero = is_zero && (cur_delta[i] == 0.f);

    }

    return { false, is_zero, time, feather };
}

void finishNonBlackDragonIntegration(const ALFlycamClassic::Input& in,
                                     const ALFlycamClassic::Intermediate& intermediate,
                                     ALFlycamClassic::State& state)
{
    const F32 time = intermediate.time;
    const F32 feather = intermediate.feather;

    state.flycamPosition += LLVector3(state.delta.data()) * state.flycamRotation;

    LLMatrix3 rot_mat(state.delta[3], state.delta[4], state.delta[5]);
    state.flycamRotation = LLQuaternion(rot_mat)*state.flycamRotation;

    if (in.levelHorizon || in.autoLeveling)
    {
        LLMatrix3 level(state.flycamRotation);

        LLVector3 x = LLVector3(level.mMatrix[0]);
        LLVector3 y = LLVector3(level.mMatrix[1]);
        LLVector3 z = LLVector3(level.mMatrix[2]);

        y.mV[2] = 0.f;
        y.normVec();

        level.setRows(x,y,z);
        level.orthogonalize();

        LLQuaternion quat(level);
        state.flycamRotation = in.levelHorizon ? quat : nlerp(llmin(feather*time,1.f), state.flycamRotation, quat);
    }

    if (in.zoomDirect)
    {
        state.flycamZoom = state.lastDelta[6]* in.axisScale[6]+ in.axisDeadZone[6];
    }
    else
    {
        //BD - We need to cap zoom otherwise it internally counts higher causing
        //     the zoom level to not react until that extra has been removed first.
        state.flycamZoom = llclamp(state.flycamZoom + state.delta[6], in.minView, in.maxView);
    }

}
}

ALFlycamClassic::Intermediate ALFlycamClassic::integrateDeltas(const Input& in, State& state)
{
    return in.blackDragon
        ? integrateBlackDragonDeltas(in, state)
        : integrateNonBlackDragonDeltas(in, state);
}

void ALFlycamClassic::finishIntegration(const Input& in,
                                        const Intermediate& intermediate,
                                        State& state)
{
    if (intermediate.blackDragon)
    {
        finishBlackDragonIntegration(in, intermediate, state);
    }
    else
    {
        finishNonBlackDragonIntegration(in, intermediate, state);
    }

    if (in.levelHorizon)
    {
        state.delta[CAM_X_AXIS] = 0.f;
        state.lastDelta[CAM_X_AXIS] = -in.axis[CAM_X_AXIS];
    }

}
