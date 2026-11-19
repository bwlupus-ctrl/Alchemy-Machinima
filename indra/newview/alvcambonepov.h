/**
 * @file alvcambonepov.h
 * @brief Per-frame skeleton driver for prim-free virtual cameras.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_VCAM_BONE_POV_H
#define AL_VCAM_BONE_POV_H

#include "llprismlens.h"

#include <array>
#include <string>

class LLVOAvatar;

class ALVCamBonePov
{
public:
    static ALVCamBonePov& instance();
    static LLVOAvatar* resolveAnchorAvatar(U8 slot);

    void tick(F64 presentation_time);
    std::string status(const LLPrismLens::CaptureHandle& capture) const;

private:
    struct Smoother
    {
        LLPrismLens::CaptureHandle mHandle;
        LLVector3 mSmoothedPos;
        LLQuaternion mSmoothedRot;
        LLVector3 mLastGoodUp;
        bool mHavePosition = false;
        bool mHaveRotation = false;
        bool mHaveLastGoodUp = false;
        bool mResolved = false;
        F64 mLastTime = -1.0;

        bool mHaveFingerprint = false;
        U8 mAnchorSlot = 0;
        U8 mJointSelection = LLPrismLens::BONE_JOINT_EYELINE;
        U8 mAimMode = 0;
        U8 mRollMode = 0;
        std::string mSelectedJointName;
        std::string mStatus;
    };

    std::array<Smoother, LLPrismLens::MAX_CAPTURES> mSmoothers;
};

#endif // AL_VCAM_BONE_POV_H
