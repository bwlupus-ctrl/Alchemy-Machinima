/** Live gaze envelope. Authored slots and runtime deliberately have separate lifetimes. */
#ifndef AL_GAZE_HOT_H
#define AL_GAZE_HOT_H
#include "algazemath.h"
#include "algazemotor.h"

namespace ALGazeHot
{
inline void fixatedEyeInHead(const LLVector3& world_direction, const LLQuaternion& head_world,
                            F32 yaw_cap_deg, F32 pitch_cap_deg, F32& yaw, F32& pitch,
                            F32 pitch_up_cap_deg = -1.f)
{
    ALGazePolicy::eyeInHeadRawFromWorldGaze(world_direction, head_world, yaw, pitch);
    const F32 yaw_cap = llmax(yaw_cap_deg, 0.f) * DEG_TO_RAD;
    const F32 pitch_cap = llmax(pitch_cap_deg, 0.f) * DEG_TO_RAD;
    const F32 pitch_up_cap = pitch_up_cap_deg >= 0.f ? pitch_up_cap_deg * DEG_TO_RAD : pitch_cap;
    yaw = llclamp(yaw, -yaw_cap, yaw_cap);
    pitch = llclamp(pitch, -pitch_up_cap, pitch_cap);
}

// Acquisition uses the motor trajectory; fixation re-solves its anatomical
// distribution immediately and carries the unsampled world ray to the final
// post-head eye solve. No dwell, dead band or root-relative eye lag in a hold.
inline void stepMotor(ALGazeMotor::GazeMotorState& state, const ALGazeMotor::GazeMotorInput& input,
                      bool fixated, const LLVector3& world_direction, ALGazeMotor::GazeMotorPose& pose)
{
    if (fixated) ALGazeMotor::detail::initializeState(state, input, input.mTimeSeconds);
    ALGazeMotor::step(state, input, pose);
    if (fixated) pose.mDesiredWorldGaze = world_direction;
}

// Re-distribute the acquired head/neck programs in their actual painted
// parent's frame. A rotated animation/Stepped torso must not consume the eye
// cone, and cancelling that rotation must still respect each joint's capacity.
inline void recruitFace(const ALGazeMotor::GazeMotorState& acquired,
                        const ALGazeMotor::GazeMotorInput& input,
                        const LLQuaternion& parent_world, ALGazeMotor::GazeMotorPose& pose)
{
    auto face = input;
    face.mSettings.mTorsoAmount = 0.f;
    face.mSettings.mChestShare = 0.f;
    face.mSettings.mPlantPelvis = true;
    face.mSettings.mStillness = face.mSettings.mRestraint = 0.f;
    face.mSettings.mEyeDriftAmpRad = face.mSettings.mHeadDriftAmpRad = face.mSettings.mMicrosaccadeAmpRad = 0.f;
    face.mRefWorldRot = parent_world;
    face.mTargetRoll = 0.f;
    auto recruit = [&](S32 yaw_channel, S32 pitch_channel)
    {
        const F32 yaw = ALTrajectory::sample(acquired.mChannels[yaw_channel].mProgram, input.mTimeSeconds).mP;
        const F32 pitch = ALTrajectory::sample(acquired.mChannels[pitch_channel].mProgram, input.mTimeSeconds).mP;
        const LLVector3 local = (ALGazePolicy::eyeDirFromYawPitch(yaw, pitch) * input.mRefWorldRot) * ~parent_world;
        ALGazePolicy::yawPitchFromLocalDir(local, face.mTargetYaw, face.mTargetPitch);
        ALGazeMotor::GazeMotorState state;
        ALGazeMotor::detail::initializeState(state, face, input.mTimeSeconds);
        ALGazeMotor::GazeMotorPose result;
        ALGazeMotor::step(state, face, result);
        return result;
    };
    const auto head = recruit(ALGazeMotor::CH_HEAD_YAW, ALGazeMotor::CH_HEAD_PITCH);
    const auto neck = recruit(ALGazeMotor::CH_NECK_YAW, ALGazeMotor::CH_NECK_PITCH);
    pose.mHeadYaw = head.mHeadYaw; pose.mHeadPitch = head.mHeadPitch;
    pose.mNeckYaw = neck.mNeckYaw; pose.mNeckPitch = neck.mNeckPitch;
}

struct State
{
    S32 mSlot = -1;
    F64 mStart = 0.0, mRelease = -1.0;
    F32 mAcquire = .12f, mReturn = 1.2f, mFrom = 0.f, mReleaseWeight = 0.f;
    F64 mDwell = 2.0;
    bool mHold = false, mStay = false;

    F32 weight(F64 now) const
    {
        if (mSlot < 0) return 0.f;
        const F64 age = llmax(now - mStart, 0.0);
        ALGazeMath::GazeMacroParams p;
        p.mAcquireSec = mAcquire;
        // Open hold; release is explicit so releasing during acquisition is continuous.
        const F32 acquired = mFrom + (1.f - mFrom) * ALGazeMath::evalGazeMacro(
            ALGazeMath::GAZE_MACRO_NONE, 0, age, p).mTargetWeight;
        const F64 automatic = mStart + mAcquire + mDwell;
        const F64 release = mRelease >= 0.0 ? mRelease
            : (!mHold && !mStay ? automatic : -1.0);
        if (release < 0.0 || now < release) return acquired;
        const F32 at_release = mRelease >= 0.0 ? mReleaseWeight : 1.f;
        return at_release * (mReturn > 0.f
            ? 1.f - ALGazeMath::cueSmoothStep((now - release) / mReturn) : 0.f);
    }
    bool returning(F64 now) const
    {
        return mRelease >= 0.0 || (!mHold && !mStay && now >= mStart + mAcquire + mDwell);
    }
    F32 acquireFraction(F64 now) const
    { return mAcquire > 0.f ? ALGazeMath::cueSmoothStep((now - mStart) / mAcquire) : 1.f; }
    bool finished(F64 now) const
    {
        const F64 release = mRelease >= 0.0 ? mRelease
            : (!mHold && !mStay ? mStart + mAcquire + mDwell : -1.0);
        return mSlot < 0 || (release >= 0.0 && now >= release + mReturn);
    }
    void fire(S32 slot, F64 now, F32 acquire, F64 dwell, F32 back, bool hold, bool stay)
    {
        mFrom = weight(now);
        mSlot = slot; mStart = now; mRelease = -1.0;
        mAcquire = acquire; mDwell = dwell; mReturn = back; mHold = hold; mStay = stay;
    }
    void release(F64 now)
    {
        if (mRelease >= 0.0) return;
        mReleaseWeight = weight(now); mRelease = now;
    }
};
}
#endif
