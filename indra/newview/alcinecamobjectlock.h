/**
 * @file alcinecamobjectlock.h
 * @brief Main-camera object mount; renderer-independent global pose history.
 */
#ifndef AL_CINECAM_OBJECT_LOCK_H
#define AL_CINECAM_OBJECT_LOCK_H

#include "alvcambonepovmath.h"
#include "llclientoutertransform.h"
#include "lluuid.h"

namespace ALCineCamObjectLock
{
inline bool isConfigured(S32 configured_mode, S32 switcher_mode, S32 object_mode)
{
    return configured_mode == object_mode || switcher_mode == object_mode;
}

struct RestoreState
{
    S32 mMode = 0;
    bool mEnabled = false;
};

inline RestoreState restoreState(bool have_previous, S32 previous_mode,
                                 bool previous_enabled, S32 object_mode)
{
    const bool restore = have_previous && previous_mode > 0 && previous_mode < object_mode;
    return { restore ? previous_mode : 0, restore && previous_enabled };
}

// Ghost clone prims/attachments draw after this uniform, positive outer scale.
// Transform only the prim center: mount offsets are metres, not scaled geometry.
// VCam Object POV deliberately continues to use its existing sampling semantics.
inline void applyOuterTransform(ALVCamBonePovMath::ObjectRenderPose& pose,
                                const LLClientOuterTransform* outer)
{
    if (outer && outer->mEnabled && !is_approx_equal(outer->mScale, 1.f))
    {
        pose.mPosition = pose.mPosition * outer->mCurrent;
    }
}

struct Settings
{
    LLVector3 mOffset = LLVector3(0.f, 0.f, 0.3f);
    F32 mAimYaw = 0.f;
    F32 mAimPitch = 0.f;
    F32 mSmoothing = 0.f;
    bool mFollowYaw = true;
    bool mFollowPitch = false;
    bool mFollowRoll = false;

    U8 axes() const
    {
        return static_cast<U8>((mFollowYaw ? 1 : 0) |
            (mFollowPitch ? 2 : 0) | (mFollowRoll ? 4 : 0));
    }
};

struct PoseHistory
{
    ALVCamBonePovMath::ObjectPositionHistory mPosition;
    ALVCamBonePovMath::ObjectFollowHistory mFollow;
    LLQuaternion mRotation;
    F64 mLastTime = -1.0;
    bool mHaveRotation = false;
    LLUUID mTargetId;
    U8 mAxes = 0;

    // The caller re-resolves the UUID every tick. A missing/rejected drawable
    // passes null, retaining the last pose while clearing transient history.
    bool update(const LLUUID& target_id,
                const ALVCamBonePovMath::ObjectRenderPose* pose,
                const LLVector3d& origin, const Settings& settings, F64 time)
    {
        if (mTargetId != target_id || mAxes != settings.axes()) lose();
        mTargetId = target_id;
        mAxes = settings.axes();
        if (!pose)
        {
            lose();
            return false;
        }
        return sample(*pose, origin, settings, time);
    }

    void seed(const LLVector3d& position, const LLQuaternion& rotation)
    {
        mPosition.mGlobalPosition = position;
        mPosition.mHaveGood = true;
        mRotation = rotation;
        lose();
    }

    // Loss, target/mask changes and pre-emption retain the global held pose,
    // but the next good sample snaps rather than blending from stale history.
    void lose()
    {
        mPosition.lose();
        mFollow.reset();
        mHaveRotation = false;
        mLastTime = -1.0;
    }

    bool sample(const ALVCamBonePovMath::ObjectRenderPose& pose,
                const LLVector3d& origin, const Settings& settings, F64 time)
    {
        using namespace ALVCamBonePovMath;
        const F32 norm_squared = dot(pose.mRotation, pose.mRotation);
        if (!std::isfinite(time) || !pose.mPosition.isFinite() ||
            !pose.mRotation.isFinite() ||
            !std::isfinite(norm_squared) || std::fabs(norm_squared - 1.f) > 2e-3f ||
            !origin.isFinite() || !settings.mOffset.isFinite() ||
            !std::isfinite(settings.mAimYaw) || !std::isfinite(settings.mAimPitch) ||
            !std::isfinite(settings.mSmoothing) || settings.mSmoothing < 0.f)
        {
            lose();
            return false;
        }
        const bool reversed = mFollow.resetForTimeReversal(time, mLastTime);
        const LLQuaternion followed = objectFollowRotation(pose.mRotation,
            settings.mFollowYaw, settings.mFollowPitch, settings.mFollowRoll, mFollow);
        const LLVector3 offset_position = pose.mPosition + settings.mOffset * followed;
        const LLVector3d target = origin + LLVector3d(offset_position);
        if (!target.isFinite())
        {
            lose();
            return false;
        }
        const F32 dt = mLastTime >= 0.0 && !reversed
            ? static_cast<F32>(time - mLastTime) : 0.f;
        mPosition.sample(target, dt, settings.mSmoothing, reversed);
        if (settings.axes() != 0)
        {
            // Main camera uses +X forward/+Z up. Do not apply VCam's -Z/+Y remap.
            // Zero trim avoids a composed-identity round trip under /fp:fast.
            LLQuaternion target_rotation = followed;
            if (settings.mAimYaw != 0.f || settings.mAimPitch != 0.f)
            {
                target_rotation =
                    LLQuaternion(settings.mAimPitch * DEG_TO_RAD, LLVector3::y_axis) *
                    LLQuaternion(settings.mAimYaw * DEG_TO_RAD, LLVector3::z_axis) * followed;
            }
            if (!mHaveRotation || reversed || settings.mSmoothing <= 0.f)
            {
                mRotation = target_rotation;
            }
            else
            {
                mRotation = smoothRotation(mRotation, target_rotation, dt, settings.mSmoothing);
                mRotation.normalize();
            }
            mHaveRotation = true;
        }
        else
        {
            // Position-only retains the camera's held aim, as VCam Object POV does.
            mHaveRotation = false;
        }
        mLastTime = time;
        return true;
    }
};
}

#endif
