/**
 * @file alvcambonepov.cpp
 * @brief Per-frame skeleton driver for prim-free virtual cameras.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alvcambonepov.h"

#include "alcinelightrigmodel.h"
#include "algazemath.h"
#include "alvcambonepovmath.h"
#include "lldirectorcast.h"
#include "lljoint.h"
#include "llvoavatar.h"

#include <array>
#include <cmath>

namespace
{
using namespace ALVCamBonePovMath;

struct ScaleFrame
{
    F32 mScale = 1.f;
    bool mHaveFoot = false;
    LLVector3 mFoot;
};

struct ResolvedJoint
{
    LLVector3 mBasePosition;
    LLQuaternion mOffsetRotation;
    LLVector3 mForward;
    LLVector3 mUp;
};

bool sameHandle(const LLPrismLens::CaptureHandle& left,
                const LLPrismLens::CaptureHandle& right)
{
    return left.mId == right.mId && left.mGeneration == right.mGeneration;
}

const char* anchorLabel(U8 slot)
{
    static const char* const LABELS[] = { "Me", "Subject A", "Subject B",
                                          "Subject C", "Subject D" };
    return slot < 5 ? LABELS[slot] : "Unknown subject";
}

F32 sanitizedScale(LLVOAvatar* avatar, bool scale_aware)
{
    if (!scale_aware || !avatar)
    {
        return 1.f;
    }
    return ALCineLightRigModel::sanitizeSubjectScale(
        avatar->getUniformScale());
}

ScaleFrame scaleFrame(LLVOAvatar* avatar, F32 scale)
{
    ScaleFrame result;
    result.mScale = scale;
    if (!avatar || scale == 1.f)
    {
        return result;
    }

    LLJoint* root = avatar->getRootJoint();
    if (!root)
    {
        return result;
    }
    result.mFoot = root->getWorldPosition(); // lazy-recompute, not the cached accessor
    F32 pelvis_to_foot = avatar->getPelvisToFoot();
    if (!std::isfinite(pelvis_to_foot))
    {
        pelvis_to_foot = 0.f;
    }
    result.mFoot.mV[VZ] -= llmax(0.f, pelvis_to_foot);
    result.mHaveFoot = result.mFoot.isFinite();
    return result;
}

bool renderedJointPosition(LLJoint* joint, const ScaleFrame& scale_frame,
                           LLVector3& position)
{
    if (!joint)
    {
        return false;
    }
    position = joint->getWorldPosition(); // lazy-recompute, not the cached accessor
    if (!position.isFinite())
    {
        return false;
    }
    if (scale_frame.mScale != 1.f && scale_frame.mHaveFoot)
    {
        position = boneScaledPoint(position, scale_frame.mFoot,
                                   scale_frame.mScale);
    }
    return position.isFinite();
}

bool skeletonFrameRotation(const LLVector3& forward_in,
                           const LLVector3& up_in,
                           LLQuaternion& rotation)
{
    LLVector3 forward = forward_in;
    LLVector3 up = up_in;
    if (!forward.isFinite() || !up.isFinite() ||
        forward.normVec() <= F_ALMOST_ZERO || up.normVec() <= F_ALMOST_ZERO)
    {
        return false;
    }
    LLVector3 left = up % forward;
    if (left.normVec() <= F_ALMOST_ZERO)
    {
        return false;
    }
    up = forward % left;
    if (up.normVec() <= F_ALMOST_ZERO)
    {
        return false;
    }
    rotation = LLQuaternion(forward, left, up);
    return rotation.isFinite();
}

bool resolveSingleJoint(LLVOAvatar* avatar, const char* name,
                        const ScaleFrame& scale_frame, ResolvedJoint& result)
{
    LLJoint* joint = avatar ? avatar->getJoint(name) : nullptr;
    if (!renderedJointPosition(joint, scale_frame, result.mBasePosition))
    {
        return false;
    }
    result.mOffsetRotation = joint->getWorldRotation(); // lazy-recompute, not cached
    if (!result.mOffsetRotation.isFinite())
    {
        return false;
    }
    result.mForward = LLVector3::x_axis * result.mOffsetRotation;
    result.mUp = LLVector3::z_axis * result.mOffsetRotation;
    return result.mForward.isFinite() && result.mUp.isFinite() &&
           result.mForward.normVec() > F_ALMOST_ZERO &&
           result.mUp.normVec() > F_ALMOST_ZERO;
}

bool resolveEyeJoint(LLVOAvatar* avatar, const ScaleFrame& scale_frame,
                     ResolvedJoint& result)
{
    if (!avatar)
    {
        return false;
    }

    LLJoint* eyes[2] = {
        avatar->getJoint("mEyeLeft"), avatar->getJoint("mEyeRight")
    };
    if (!eyes[0] && !eyes[1])
    {
        eyes[0] = avatar->getJoint("mFaceEyeAltLeft");
        eyes[1] = avatar->getJoint("mFaceEyeAltRight");
    }

    LLQuaternion rotations[2];
    LLVector3 position_sum;
    U32 count = 0;
    for (LLJoint* eye : eyes)
    {
        LLVector3 position;
        if (!eye || !renderedJointPosition(eye, scale_frame, position))
        {
            continue;
        }
        const LLQuaternion rotation = eye->getWorldRotation(); // lazy-recompute, not cached
        if (!rotation.isFinite())
        {
            continue;
        }
        position_sum += position;
        rotations[count++] = rotation;
    }

    if (!count)
    {
        return resolveSingleJoint(avatar, "mHead", scale_frame, result);
    }
    result.mBasePosition = position_sum / static_cast<F32>(count);
    if (!averageEyeForward(rotations, count, result.mForward))
    {
        return false;
    }

    if (LLJoint* head = avatar->getJoint("mHead"))
    {
        result.mUp = LLVector3::z_axis * head->getWorldRotation(); // lazy-recompute
    }
    else
    {
        result.mUp.clearVec();
        for (U32 index = 0; index < count; ++index)
        {
            result.mUp += LLVector3::z_axis * rotations[index];
        }
    }
    if (!result.mUp.isFinite() || result.mUp.normVec() <= F_ALMOST_ZERO)
    {
        return false;
    }
    return skeletonFrameRotation(result.mForward, result.mUp,
                                 result.mOffsetRotation);
}

bool resolveConfiguredJoint(LLVOAvatar* avatar,
                            const LLPrismLens::BonePovSettings& settings,
                            const ScaleFrame& scale_frame,
                            ResolvedJoint& result)
{
    switch (settings.mJointSelection)
    {
        case LLPrismLens::BONE_JOINT_EYELINE:
            return resolveEyeJoint(avatar, scale_frame, result);
        case LLPrismLens::BONE_JOINT_NAMED:
            return !settings.mCustomJoint.empty() &&
                   resolveSingleJoint(avatar, settings.mCustomJoint.c_str(),
                                      scale_frame, result);
        default:
            // Defensive degradation for an in-memory tag from a newer build.
            return resolveEyeJoint(avatar, scale_frame, result);
    }
}

const char* jointLabel(const LLPrismLens::BonePovSettings& settings)
{
    return settings.mJointSelection == LLPrismLens::BONE_JOINT_NAMED
        ? settings.mCustomJoint.c_str() : "Eye (eyeline)";
}

bool finiteUnitQuaternion(const LLQuaternion& rotation)
{
    if (!rotation.isFinite())
    {
        return false;
    }
    const F32 magnitude = std::sqrt(
        rotation.mQ[VX] * rotation.mQ[VX] +
        rotation.mQ[VY] * rotation.mQ[VY] +
        rotation.mQ[VZ] * rotation.mQ[VZ] +
        rotation.mQ[VS] * rotation.mQ[VS]);
    return std::isfinite(magnitude) && std::fabs(magnitude - 1.f) <= 1e-3f;
}

LLVector3 safeDegenerateUp(const LLVector3& forward,
                           const LLVector3& preferred)
{
    LLVector3 up = preferred;
    LLVector3 right = forward % up;
    if (!up.isFinite() || right.normVec() <= F_ALMOST_ZERO)
    {
        // This is only the no-history, first-frame fallback. Once a horizon up
        // has succeeded, the caller always prefers and holds that last good up.
        up = std::fabs(forward * LLVector3::z_axis) < 0.9f
            ? LLVector3::z_axis : LLVector3::x_axis;
        right = forward % up;
        if (right.normVec() <= F_ALMOST_ZERO)
        {
            up = LLVector3::y_axis;
            right = forward % up;
            right.normVec();
        }
    }
    up = right % forward;
    up.normVec();
    return up;
}
} // anonymous namespace

//static
ALVCamBonePov& ALVCamBonePov::instance()
{
    static ALVCamBonePov driver;
    return driver;
}

//static
LLVOAvatar* ALVCamBonePov::resolveAnchorAvatar(U8 slot)
{
    LLDirectorCast& cast = LLDirectorCast::instance();
    switch (slot)
    {
        case LLPrismLens::BONE_ANCHOR_ME: return cast.resolve(LLUUID::null);
        case LLPrismLens::BONE_ANCHOR_A: return cast.resolveSubjectA();
        case LLPrismLens::BONE_ANCHOR_B: return cast.resolveSubjectB();
        case LLPrismLens::BONE_ANCHOR_C: return cast.resolveSubjectC();
        case LLPrismLens::BONE_ANCHOR_D: return cast.resolveSubjectD();
        default: return nullptr;
    }
}

void ALVCamBonePov::tick(F64 presentation_time)
{
    std::array<bool, LLPrismLens::MAX_CAPTURES> seen = {};
    const LLPrismLens::RegistrySnapshot snapshot =
        LLPrismLens::registrySnapshot();

    for (U32 index = 0; index < snapshot.mCaptureCount; ++index)
    {
        const LLPrismLens::CaptureDefinition& capture = snapshot.mCaptures[index];
        if (capture.mSlot >= LLPrismLens::MAX_CAPTURES)
        {
            continue;
        }
        seen[capture.mSlot] = true;
        Smoother& smoother = mSmoothers[capture.mSlot];
        if (!sameHandle(smoother.mHandle, capture.mHandle))
        {
            smoother = Smoother();
            smoother.mHandle = capture.mHandle;
        }

        const LLPrismLens::BonePovSettings& settings = capture.mCamera.mBonePov;
        const bool fingerprint_changed = smoother.mHaveFingerprint &&
            (smoother.mAnchorSlot != settings.mAnchorSlot ||
             smoother.mJointSelection != settings.mJointSelection ||
             smoother.mAimMode != settings.mAimMode ||
             smoother.mRollMode != settings.mRollMode ||
             smoother.mSelectedJointName != settings.mCustomJoint);
        const bool was_resolved = smoother.mResolved;
        smoother.mHaveFingerprint = true;
        smoother.mAnchorSlot = settings.mAnchorSlot;
        smoother.mJointSelection = settings.mJointSelection;
        smoother.mAimMode = settings.mAimMode;
        smoother.mRollMode = settings.mRollMode;
        smoother.mSelectedJointName = settings.mCustomJoint;

        if (fingerprint_changed)
        {
            // Position snaps to the newly selected bone. Rotation instead seeds
            // from the currently stored camera on a live toggle, so a follow
            // transition starts continuously and slerps toward the new target.
            smoother.mHavePosition = false;
            if (was_resolved &&
                settings.mAimMode == LLPrismLens::BONE_AIM_FULL_FOLLOW)
            {
                smoother.mSmoothedRot = capture.mCamera.mVirtualRot;
                smoother.mHaveRotation = finiteUnitQuaternion(
                    smoother.mSmoothedRot);
            }
            else
            {
                smoother.mHaveRotation = false;
            }
        }

        if (capture.mMode != LLPrismLens::ECaptureMode::CAMERA_FEED ||
            !capture.mCamera.mVirtual || !settings.mEnabled)
        {
            smoother.mHavePosition = false;
            smoother.mHaveRotation = false;
            smoother.mHaveLastGoodUp = false;
            smoother.mResolved = false;
            smoother.mLastTime = -1.0;
            smoother.mStatus.clear();
            continue;
        }
        if (!std::isfinite(presentation_time))
        {
            smoother.mHavePosition = false;
            smoother.mHaveRotation = false;
            smoother.mHaveLastGoodUp = false;
            smoother.mResolved = false;
            smoother.mLastTime = -1.0;
            smoother.mStatus = "Bone POV is holding: presentation time is invalid.";
            continue;
        }

        LLVOAvatar* avatar = resolveAnchorAvatar(settings.mAnchorSlot);
        if (!avatar)
        {
            // Anchor loss is hold-last. Clear smoothing so reacquisition snaps
            // to the fresh pose rather than lerping from stale state.
            smoother.mHavePosition = false;
            smoother.mHaveRotation = false;
            smoother.mHaveLastGoodUp = false;
            smoother.mResolved = false;
            smoother.mLastTime = -1.0;
            smoother.mStatus = llformat("Bone POV is holding: %s is unavailable.",
                                        anchorLabel(settings.mAnchorSlot));
            continue;
        }

        const F32 scale = sanitizedScale(avatar, settings.mScaleAware);
        const ScaleFrame scale_frame = scaleFrame(avatar, scale);
        ResolvedJoint joint;
        if (!resolveConfiguredJoint(avatar, settings, scale_frame, joint))
        {
            smoother.mHavePosition = false;
            smoother.mHaveRotation = false;
            smoother.mHaveLastGoodUp = false;
            smoother.mResolved = false;
            smoother.mLastTime = -1.0;
            smoother.mStatus = llformat(
                "Bone POV is holding: joint '%s' was not found on %s.",
                jointLabel(settings), anchorLabel(settings.mAnchorSlot));
            continue;
        }

        const LLVector3 target_position = boneEye(
            joint.mBasePosition, settings.mOffset, scale,
            joint.mOffsetRotation);
        if (!target_position.isFinite())
        {
            smoother.mHavePosition = false;
            smoother.mHaveRotation = false;
            smoother.mResolved = false;
            smoother.mLastTime = -1.0;
            smoother.mStatus = "Bone POV is holding: the resolved position is invalid.";
            continue;
        }

        const bool time_reversed = smoother.mLastTime >= 0.0 &&
                                   presentation_time < smoother.mLastTime;
        const F32 dt = smoother.mLastTime >= 0.0 && !time_reversed
            ? static_cast<F32>(presentation_time - smoother.mLastTime) : 0.f;
        const F32 tau = settings.mSmoothingSec;
        if (!smoother.mHavePosition || tau <= 0.f || time_reversed)
        {
            smoother.mSmoothedPos = target_position;
            smoother.mHavePosition = true;
        }
        else
        {
            smoother.mSmoothedPos = smoothStep(
                smoother.mSmoothedPos, target_position, dt, tau);
        }

        const F32 vertical_fov = settings.mFovDeg * DEG_TO_RAD;
        std::string reason;
        bool wrote = false;
        if (settings.mAimMode == LLPrismLens::BONE_AIM_STABILIZED)
        {
            // This overload intentionally cannot write mVirtualRot. The
            // operator remains the sole owner of stabilized aim.
            wrote = LLPrismLens::setVirtualCameraPosition(
                capture.mHandle, smoother.mSmoothedPos, vertical_fov, &reason);
            smoother.mHaveRotation = false;
        }
        else
        {
            LLVector3 forward = ALGazeMath::eyelineOffsetDir(
                joint.mForward, joint.mUp,
                settings.mTrimYawDeg * DEG_TO_RAD,
                settings.mTrimPitchDeg * DEG_TO_RAD);
            if (!forward.isFinite() || forward.normVec() <= F_ALMOST_ZERO)
            {
                smoother.mHavePosition = false;
                smoother.mHaveRotation = false;
                smoother.mResolved = false;
                smoother.mLastTime = -1.0;
                smoother.mStatus = "Bone POV is holding: the resolved aim is invalid.";
                continue;
            }

            LLVector3 up = joint.mUp;
            if (settings.mRollMode == LLPrismLens::BONE_ROLL_HORIZON_LOCK)
            {
                LLVector3 level_up;
                if (horizonLevelUp(forward, level_up))
                {
                    up = level_up;
                    smoother.mLastGoodUp = level_up;
                    smoother.mHaveLastGoodUp = true;
                }
                else
                {
                    // Normative degeneracy behavior: HOLD LAST GOOD UP first.
                    // Joint up is used only when no successful horizon frame has
                    // ever established history for this resolved attachment.
                    up = safeDegenerateUp(
                        forward, smoother.mHaveLastGoodUp
                            ? smoother.mLastGoodUp : joint.mUp);
                }
            }

            LLVector3 basis_right = forward % up;
            if (!up.isFinite() || up.normVec() <= F_ALMOST_ZERO ||
                basis_right.normVec() <= F_ALMOST_ZERO)
            {
                smoother.mHavePosition = false;
                smoother.mHaveRotation = false;
                smoother.mResolved = false;
                smoother.mLastTime = -1.0;
                smoother.mStatus = "Bone POV is holding: the aim/up basis is degenerate.";
                continue;
            }
            const LLQuaternion target_rotation = followRotation(forward, up);
            if (!finiteUnitQuaternion(target_rotation))
            {
                smoother.mHavePosition = false;
                smoother.mHaveRotation = false;
                smoother.mResolved = false;
                smoother.mLastTime = -1.0;
                smoother.mStatus = "Bone POV is holding: the resolved orientation is invalid.";
                continue;
            }
            if (!smoother.mHaveRotation || tau <= 0.f || time_reversed)
            {
                smoother.mSmoothedRot = target_rotation;
                smoother.mHaveRotation = true;
            }
            else
            {
                smoother.mSmoothedRot = smoothRotation(
                    smoother.mSmoothedRot, target_rotation, dt, tau);
                smoother.mSmoothedRot.normalize();
            }
            wrote = LLPrismLens::setVirtualCameraTransform(
                capture.mHandle, smoother.mSmoothedPos,
                smoother.mSmoothedRot, vertical_fov, &reason);
        }

        if (!wrote)
        {
            smoother.mHavePosition = false;
            smoother.mHaveRotation = false;
            smoother.mResolved = false;
            smoother.mLastTime = -1.0;
            smoother.mStatus = "Bone POV is holding: " + reason;
            continue;
        }

        smoother.mResolved = true;
        smoother.mLastTime = presentation_time;
        smoother.mStatus = llformat("Bone POV attached: %s on %s%s.",
            jointLabel(settings), anchorLabel(settings.mAnchorSlot),
            settings.mJointSelection == LLPrismLens::BONE_JOINT_NAMED &&
            !LLPrismLens::isBonePovSpineJoint(settings.mCustomJoint) &&
            settings.mAimMode == LLPrismLens::BONE_AIM_FULL_FOLLOW
                ? " (off-spine full-follow orientation is best-effort)" : "");
    }

    for (U32 slot = 0; slot < LLPrismLens::MAX_CAPTURES; ++slot)
    {
        if (!seen[slot])
        {
            mSmoothers[slot] = Smoother();
        }
    }
}

std::string ALVCamBonePov::status(
    const LLPrismLens::CaptureHandle& capture) const
{
    for (const Smoother& smoother : mSmoothers)
    {
        if (sameHandle(smoother.mHandle, capture))
        {
            return smoother.mStatus;
        }
    }
    return std::string();
}
