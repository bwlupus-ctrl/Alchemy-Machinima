/**
 * @file alvcambonepovmath.h
 * @brief Pure math for skeleton- and object-attached virtual cameras.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_VCAM_BONE_POV_MATH_H
#define AL_VCAM_BONE_POV_MATH_H

#include "llmath.h"
#include "llquaternion.h"
#include "m4math.h"
#include "v3math.h"
#include "v3dmath.h"

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

struct ObjectRenderPose
{
    LLVector3 mPosition;
    LLQuaternion mRotation;
};

// Match active-child volume geometry: drawable-local pose, then its parent's
// drawable world matrix. Roots/static drawables already have a world matrix.
inline ObjectRenderPose objectDrawablePose(bool active_child,
                                           const LLVector3& local_position,
                                           const LLQuaternion& local_rotation,
                                           const LLMatrix4& world_matrix)
{
    ObjectRenderPose result;
    result.mPosition = (active_child ? local_position : LLVector3::zero) * world_matrix;
    result.mRotation = active_child
        ? local_rotation * LLQuaternion(world_matrix) : LLQuaternion(world_matrix);
    return result;
}

// The drawable interface also permits a renderer-free regression fixture with
// deliberately different model and drawable transforms.
template<class Drawable>
bool objectDrawablePose(const Drawable& drawable, ObjectRenderPose& pose)
{
    if (drawable.isDead() || drawable.getGeneration() < 0)
    {
        return false;
    }
    const bool active_child = drawable.isActive() && !drawable.isRoot();
    const Drawable* world = active_child ? drawable.getParent() : &drawable;
    if (!world || world->isDead() || world->getGeneration() < 0)
    {
        return false;
    }
    pose = objectDrawablePose(active_child, drawable.getPosition(),
                              drawable.getRotation(), world->getWorldMatrix());
    return true;
}

struct ObjectPositionHistory
{
    LLVector3d mGlobalPosition;
    bool mHaveGood = false;
    bool mHaveSmoothing = false;

    void sample(const LLVector3d& target, F32 dt, F32 tau, bool reset)
    {
        if (!mHaveSmoothing || tau <= 0.f || reset)
        {
            mGlobalPosition = target;
        }
        else
        {
            mGlobalPosition += (target - mGlobalPosition) * static_cast<F64>(smoothAlpha(dt, tau));
        }
        mHaveGood = true;
        mHaveSmoothing = true;
    }

    void lose()
    {
        mHaveSmoothing = false;
    }

    LLVector3 agentPosition(const LLVector3d& origin) const
    {
        LLVector3 result;
        result.setVec(mGlobalPosition - origin);
        return result;
    }
};

struct ObjectFollowHistory
{
    F32 mYaw = 0.f;
    bool mHaveYaw = false;

    void reset()
    {
        *this = ObjectFollowHistory();
    }

    bool resetForTimeReversal(F64 presentation_time, F64 last_time)
    {
        const bool reversed = last_time >= 0.0 && presentation_time < last_time;
        if (reversed) reset();
        return reversed;
    }
};

constexpr F64 OBJECT_POV_GIMBAL_COS_THRESHOLD = 1e-4;

struct ObjectEulerAngles
{
    F32 mYaw = 0.f;
    F32 mPitch = 0.f;
    F32 mRoll = 0.f;
};

// Intrinsic Z-Y-X: yaw psi about world +Z, pitch theta about yawed +Y (left),
// then roll phi about the resulting +X (forward). Viewer quaternions multiply
// in reverse Hamilton order, so Rz(psi) Ry(theta) Rx(phi) is roll * pitch * yaw.
// Positive pitch tips +X toward -Z; positive roll tips +Y toward +Z.
inline LLQuaternion composeObjectRotation(F32 yaw, F32 pitch, F32 roll)
{
    return LLQuaternion(roll, LLVector3::x_axis) *
        LLQuaternion(pitch, LLVector3::y_axis) *
        LLQuaternion(yaw, LLVector3::z_axis);
}

// Decompose the row-vector rotation matrix: forward is row 0, left is row 1.
// Quadratic quaternion products make q and -q identical, including at pi.
// Evaluate matrix entries in double to retain precision near vertical forward.
// Principal theta is in [-pi/2, pi/2]; outside the gimbal band psi/phi are the
// ordinary atan2 branches. Inside it hold the previous psi (zero when fresh),
// deriving phi from the coupled horizontal left axis. Only held yaw is history.
// At exact gimbal this still reconstructs R; within the finite guard band a
// held yaw can only approximate R. Full follow therefore bypasses Euler below.
// On leaving the band, partial masks can change by pi in yaw/roll as the
// principal Euler branch changes. This is the explicit gimbal policy, not a
// promise of continuous partial frames across +/-90 degrees.
inline ObjectEulerAngles objectEulerAngles(const LLQuaternion& rotation,
                                           ObjectFollowHistory& history)
{
    if (!rotation.isFinite())
    {
        history.reset();
        return ObjectEulerAngles();
    }
    const F64 x = rotation.mQ[VX];
    const F64 y = rotation.mQ[VY];
    const F64 z = rotation.mQ[VZ];
    const F64 w = rotation.mQ[VS];
    const F64 norm_squared = x*x + y*y + z*z + w*w;
    if (norm_squared <= 1e-12)
    {
        history.reset();
        return ObjectEulerAngles();
    }
    const F64 r00 = 1.0 - 2.0 * (y*y + z*z) / norm_squared;
    const F64 r01 = 2.0 * (x*y + z*w) / norm_squared;
    const F64 r02 = 2.0 * (x*z - y*w) / norm_squared;
    const F64 r10 = 2.0 * (x*y - z*w) / norm_squared;
    const F64 r11 = 1.0 - 2.0 * (x*x + z*z) / norm_squared;
    const F64 r12 = 2.0 * (y*z + x*w) / norm_squared;
    const F64 r22 = 1.0 - 2.0 * (x*x + y*y) / norm_squared;
    const F64 cos_pitch = std::hypot(r00, r01);
    ObjectEulerAngles result;
    result.mPitch = static_cast<F32>(std::atan2(-r02, cos_pitch));
    if (cos_pitch < OBJECT_POV_GIMBAL_COS_THRESHOLD)
    {
        result.mYaw = history.mHaveYaw ? history.mYaw : 0.f;
        const F64 sign = r02 < 0.0 ? 1.0 : -1.0;
        result.mRoll = static_cast<F32>(std::atan2(sign * r10, r11) +
                                       sign * result.mYaw);
    }
    else
    {
        result.mYaw = static_cast<F32>(std::atan2(r01, r00));
        result.mRoll = static_cast<F32>(std::atan2(r12, r22));
    }
    history.mYaw = result.mYaw;
    history.mHaveYaw = true;
    return result;
}

// Partial masks compose selected Euler angles in the same intrinsic Z-Y-X
// order. Full follow returns the normalized rendered quaternion exactly,
// including inside the finite held-yaw gimbal band (no Euler round trip).
// None is identity; the caller retains the position-only publication path.
inline LLQuaternion objectFollowRotation(const LLQuaternion& object_rotation,
                                         bool follow_yaw, bool follow_pitch,
                                         bool follow_roll,
                                         ObjectFollowHistory& history)
{
    // Decompose first so the yaw history is maintained (and reset on invalid
    // input) for every mask. Then return exact identity for invalid input and
    // for no followed axes: composing three zero-angle factors is only
    // approximately identity under the viewer's /fp:fast Release flags.
    const ObjectEulerAngles angles = objectEulerAngles(object_rotation, history);
    if (!object_rotation.isFinite())
    {
        return LLQuaternion();
    }
    if (!follow_yaw && !follow_pitch && !follow_roll)
    {
        return LLQuaternion();
    }
    if (follow_yaw && follow_pitch && follow_roll)
    {
        LLQuaternion rotation = object_rotation;
        if (!rotation.isFinite()) return LLQuaternion();
        rotation.normalize();
        return rotation;
    }
    return composeObjectRotation(follow_yaw ? angles.mYaw : 0.f,
                                  follow_pitch ? angles.mPitch : 0.f,
                                  follow_roll ? angles.mRoll : 0.f);
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
