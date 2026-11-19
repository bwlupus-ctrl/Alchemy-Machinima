/**
 * @file alvcambonepovmath.h
 * @brief Pure math for skeleton-attached virtual cameras.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_VCAM_BONE_POV_MATH_H
#define AL_VCAM_BONE_POV_MATH_H

#include "llmath.h"
#include "llquaternion.h"
#include "v3math.h"

#include <cmath>

namespace ALVCamBonePovMath
{
// Foot-pivot uniform scale used by the light rig and rendered clone joints.
inline LLVector3 boneScaledPoint(const LLVector3& joint_pos,
                                 const LLVector3& foot_pos, F32 scale)
{
    if (scale == 1.f)
    {
        return joint_pos;
    }
    return foot_pos + (joint_pos - foot_pos) * scale;
}

// A bone-local offset scales with a clone and then rotates into agent space.
inline LLVector3 boneEye(const LLVector3& base,
                         const LLVector3& local_offset, F32 scale,
                         const LLQuaternion& joint_rotation)
{
    return base + (local_offset * scale) * joint_rotation;
}

// Remap skeleton X-forward/Z-up into the camera's -Z-forward/+Y-up frame.
inline LLQuaternion followRotation(const LLVector3& forward_in,
                                   const LLVector3& up_in)
{
    LLVector3 forward = forward_in;
    LLVector3 up = up_in;
    if (!forward.isFinite() || !up.isFinite() ||
        forward.normVec() <= F_ALMOST_ZERO || up.normVec() <= F_ALMOST_ZERO)
    {
        return LLQuaternion();
    }

    LLVector3 right = forward % up;
    if (right.normVec() <= F_ALMOST_ZERO)
    {
        return LLQuaternion();
    }
    up = right % forward;
    if (up.normVec() <= F_ALMOST_ZERO)
    {
        return LLQuaternion();
    }
    return LLQuaternion(right, up, -forward);
}

// Compute a zero-roll up vector. False means forward is too close to vertical;
// the caller must hold its last good up vector.
inline bool horizonLevelUp(const LLVector3& forward_in, LLVector3& up_out)
{
    LLVector3 forward = forward_in;
    if (!forward.isFinite() || forward.normVec() <= F_ALMOST_ZERO)
    {
        return false;
    }

    constexpr F32 VERTICAL_GUARD = 1.f - 1e-3f;
    if (std::fabs(forward * LLVector3::z_axis) > VERTICAL_GUARD)
    {
        return false;
    }

    LLVector3 right = forward % LLVector3::z_axis;
    if (right.normVec() <= F_ALMOST_ZERO)
    {
        return false;
    }
    up_out = right % forward;
    return up_out.isFinite() && up_out.normVec() > F_ALMOST_ZERO;
}

inline F32 smoothAlpha(F32 dt, F32 tau)
{
    if (tau <= 0.f)
    {
        return 1.f;
    }
    return 1.f - expf(-llmax(dt, 0.f) / tau);
}

inline LLVector3 smoothStep(const LLVector3& current,
                            const LLVector3& target, F32 dt, F32 tau)
{
    if (tau <= 0.f)
    {
        return target;
    }
    return current + (target - current) * smoothAlpha(dt, tau);
}

inline LLQuaternion smoothRotation(const LLQuaternion& current,
                                   const LLQuaternion& target,
                                   F32 dt, F32 tau)
{
    if (tau <= 0.f)
    {
        return target;
    }
    return slerp(smoothAlpha(dt, tau), current, target);
}

// Match the gaze subsystem's average of local +X across the available eyes.
inline bool averageEyeForward(const LLQuaternion* rotations, U32 count,
                              LLVector3& forward_out)
{
    forward_out.clearVec();
    for (U32 index = 0; index < count; ++index)
    {
        if (!rotations[index].isFinite())
        {
            return false;
        }
        forward_out += LLVector3::x_axis * rotations[index];
    }
    return count > 0 && forward_out.isFinite() &&
           forward_out.normVec() > 1e-4f;
}
} // namespace ALVCamBonePovMath

#endif // AL_VCAM_BONE_POV_MATH_H
