/**
 * @file algazemath.h
 * @brief Pure math shared by Actor Mover lens gaze and its unit tests.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALGAZEMATH_H
#define AL_ALGAZEMATH_H

#include "llmath.h"
#include "llquaternion.h"
#include "v3math.h"

#include <cmath>

namespace ALGazeMath
{
inline F32 chaseAlpha(F32 dt, F32 tau)
{
    return 1.f - expf(-llmax(dt, 0.f) / llmax(tau, 0.01f));
}

inline void deadZoneChase(F32& aim_pitch, F32& aim_yaw,
                          F32 raw_pitch, F32 raw_yaw,
                          F32 dead_zone, F32 alpha)
{
    const F32 pitch_delta = raw_pitch - aim_pitch;
    const F32 yaw_delta = llsimple_angle(raw_yaw - aim_yaw);
    const F32 aim_error = sqrtf(pitch_delta * pitch_delta + yaw_delta * yaw_delta);
    if (aim_error <= dead_zone || aim_error <= 0.f)
    {
        return;
    }

    const F32 chase = alpha * (aim_error - dead_zone) / aim_error;
    aim_pitch += pitch_delta * chase;
    aim_yaw = llsimple_angle(aim_yaw + yaw_delta * chase);
}

inline void torsoChase(F32& torso_pitch, F32& torso_yaw,
                       F32 target_pitch, F32 target_yaw,
                       F32 ratio, F32 dt, F32 tau_user)
{
    if (ratio <= 1.001f)
    {
        torso_pitch = target_pitch;
        torso_yaw = target_yaw;
        return;
    }

    const F32 tau_extra = (ratio - 1.f) * tau_user;
    const F32 alpha = chaseAlpha(dt, tau_extra);
    torso_pitch += (target_pitch - torso_pitch) * alpha;
    torso_yaw = llsimple_angle(
        torso_yaw + llsimple_angle(target_yaw - torso_yaw) * alpha);
}

inline F32 behindEnvStep(F32 env, bool behind, F32 dt, F32 ease_time)
{
    const F32 step = ease_time > 0.f ? llmax(dt, 0.f) / ease_time : 1.f;
    return llclamp(env + (behind ? -step : step), 0.f, 1.f);
}

inline LLVector3 eyelineOffsetDir(const LLVector3& direction,
                                 const LLVector3& up_axis,
                                 F32 yaw, F32 pitch)
{
    if (fabsf(yaw) + fabsf(pitch) <= 1e-4f)
    {
        return direction;
    }

    LLVector3 left_axis = up_axis % direction;
    if (left_axis.normVec() <= 1e-4f)
    {
        return direction;
    }

    const LLQuaternion yaw_rotation(yaw, up_axis);
    const LLQuaternion pitch_rotation(pitch, left_axis);
    return direction * (pitch_rotation * yaw_rotation);
}
} // namespace ALGazeMath

#endif // AL_ALGAZEMATH_H
