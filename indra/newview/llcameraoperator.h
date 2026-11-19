/**
 * @file llcameraoperator.h
 * @brief Procedural handheld camera operator for the flycam.
 *
 * Native port of the VirtualCinema_Handheld (VCHH) ReShade shader design:
 * OPERATOR STYLE x MOTION PROFILE personas, quintic value-noise idle motion
 * on wrapped phase accumulators (framerate-independent, no pops on slider
 * changes), a motion-reactive layer (onset whip, lead-lag drag, settle
 * bounce along the latched motion axis), figure-8 walking gait with cadence
 * drive, breathing with vertical lift coupling, and sparse recompose nudges.
 *
 * Unlike the shader, this runs on ground-truth camera motion (the flycam's
 * own per-frame deltas), so the estimation machinery of the original
 * (motion vectors, AGC, noise gates, cut detection heuristics) is not
 * needed. Output is applied to the *rendered* camera only -- the flycam's
 * authoritative position/rotation state is never mutated.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Phoenix / Firestorm Viewer Source Code
 * $/LicenseInfo$
 */

#ifndef LL_LLCAMERAOPERATOR_H
#define LL_LLCAMERAOPERATOR_H

#include <deque>

#include "stdtypes.h"
#include "llquaternion.h"
#include "v3math.h"

// Per-frame input: the flycam's real motion, camera-local.
struct LLCameraOperatorInput
{
    F32 mDeltaTime = 0.f;       // seconds, already clamped by caller
    // camera-local translation velocity, m/s (X=at/forward, Y=left, Z=up)
    LLVector3 mLinearVel = LLVector3::zero;
    // angular velocity, rad/s (X=roll, Y=pitch, Z=yaw) -- flycam rot deltas
    LLVector3 mAngularVel = LLVector3::zero;
};

// Per-frame output: offsets to apply to the rendered camera.
struct LLCameraOperatorOutput
{
    // camera-local translation offset, meters (X=at, Y=left, Z=up)
    LLVector3 mPosOffset = LLVector3::zero;
    // orientation wobble, radians
    F32 mRoll = 0.f;            // about at axis
    F32 mPitch = 0.f;           // about left axis
    F32 mYaw = 0.f;             // about up axis
    // multiplier on the vertical FOV (breathing)
    F32 mFovMul = 1.f;
};

class LLCameraOperator
{
public:
    static LLCameraOperator& instance();

    // Advance the simulation and produce this frame's camera offsets.
    // Reads all FlycamOperator* settings live.
    LLCameraOperatorOutput update(const LLCameraOperatorInput& input);

    // Advance from an absolute, timestamped render-pose sample. Recorder and
    // Cinematic Camera callers use this path so each fixed simulation tick can
    // interpolate the pose at its own boundary and derive motion there. The
    // live joystick deliberately uses update() instead: its device sample is
    // held across all substeps in the render frame.
    LLCameraOperatorOutput updateFromPose(F32 frame_dt,
                                          const LLVector3& position,
                                          const LLQuaternion& rotation);

    // Clear all accumulated state (call on flycam toggle / reset so the
    // rig doesn't carry momentum across teleports or mode switches).
    //
    // DETERMINISM: under a LOCOMOTION MODE (or the explicit Reaction Lag opt-in)
    // this also resets the procedural PHASES; zero-lag Legacy does not, so
    // stock behaviour is preserved exactly. An earlier version
    // deliberately left them running to avoid popping the idle motion, but that
    // made takes non-repeatable -- replaying the same recorded path with the
    // same seed started at a different point in the noise, so a re-shoot did not
    // match. reset() is only called at cuts, flycam toggles, playback starts and
    // Cinematic Camera mode/target changes, i.e. moments that are already a
    // discontinuity, so there is nothing to pop.
    void reset();

private:
    LLCameraOperator() = default;

    // One simulation tick. update() routes zero-lag Legacy directly here with
    // the caller's variable dt; opted-in locomotion/latency uses fixed substeps.
    LLCameraOperatorOutput step(const LLCameraOperatorInput& input);
    LLCameraOperatorOutput interpolateOutput() const;
    void prepareFixedPath();
    bool consumePoseTick(LLVector3& position, LLQuaternion& rotation);
    LLCameraOperatorInput delayedReactiveInput(
        const LLCameraOperatorInput& input, F32 latency);

    // ---- persistent state (the shader's FP32 state textures) ----
    // smoothed motion
    F32       mSpeed = 0.f;             // overall normalized speed (EMA)
    F32       mVecX = 0.f;              // smoothed pan-plane motion vector
    F32       mVecY = 0.f;
    // envelopes
    F32       mOnsetEnv = 0.f;
    F32       mSettleEnv = 0.f;
    // latched direction of the last real move (settle bounce axis)
    F32       mLatchX = 0.f;
    F32       mLatchY = 0.f;
    // wrapped phase accumulators
    F32       mPhaseXY = 0.f;
    F32       mPhaseRoll = 0.f;
    F32       mPhaseBreath = 0.f;
    F32       mPhaseGait = 0.f;
    F32       mPhaseSettle = 0.f;
    F32       mPhaseRecompose = 0.f;

    // ---- locomotion ----
    // Blend weight from the transition SOURCE block to the current one while a
    // transition is in flight (0 = fully source, 1 = fully current).
    F32       mModeBlend = 1.f;
    S32       mModeCurrent = -1;        // mode enum currently selected

    // Vehicle layers (Drive, and weakly Float/Unsteady). Separate phases so
    // suspension and road buzz are independent oscillators rather than two
    // views of one -- a real vehicle's body motion and its tyre/road texture
    // are not phase-locked.
    F32       mPhaseSusp = 0.f;
    F32       mPhaseRoad = 0.f;

    // AUTO locomotion. mAutoResolved is the mode Auto has actually committed
    // to; mAutoCandidate is one it is considering, and mAutoCandidateTime is
    // how long that candidate has held its threshold. A transition only commits
    // once the dwell elapses, which is what stops Auto flickering across a
    // threshold in the middle of a take.
    S32       mAutoResolved = -1;
    S32       mAutoCandidate = -1;
    F32       mAutoCandidateTime = 0.f;

    // Fixed-timestep state. Accumulated time is retained when a defensive
    // per-render step cap is reached; it is never silently discarded.
    F64       mSimAccumulator = 0.0;
    F64       mFixedStep = 1.0 / 120.0;
    bool      mFixedPathActive = false;
    LLCameraOperatorOutput mPreviousSimOutput;
    LLCameraOperatorOutput mCurrentSimOutput;

    // Timestamped pose-input history for recorded/procedural camera sources.
    struct RenderPoseSegment
    {
        LLVector3 mStartPosition;
        LLQuaternion mStartRotation;
        LLVector3 mEndPosition;
        LLQuaternion mEndRotation;
        F64 mDuration = 0.0;
        F64 mConsumed = 0.0;
    };
    std::deque<RenderPoseSegment> mPendingPoseSegments;
    bool       mHaveRenderPose = false;
    bool       mHaveSimPose = false;
    LLVector3  mPreviousRenderPosition = LLVector3::zero;
    LLQuaternion mPreviousRenderRotation;
    LLVector3  mLastSimPosition = LLVector3::zero;
    LLQuaternion mLastSimRotation;

    // Bounded input history for reaction lag. Under opted-in locomotion this
    // is sampled exclusively at the fixed simulation tick, so it is seeded,
    // deterministic, and independent of render-frame grouping.
    struct ReactionSample
    {
        F64 mTime = 0.0;
        LLVector3 mLinearVel = LLVector3::zero;
        LLVector3 mAngularVel = LLVector3::zero;
    };
    std::deque<ReactionSample> mReactionSamples;
    F64       mReactionTime = 0.0;
    F32       mReactionLatency = -1.f;

    // The source and live parameter BLOCKS themselves live in the .cpp's
    // anonymous namespace (Locomotion is not a public type, and this class is a
    // singleton, so file-scope state is equivalent to a member here). Storing
    // blocks rather than mode enums is what lets a mid-blend retarget continue
    // from the live mixture instead of snapping back to an unblended table.
};

#endif // LL_LLCAMERAOPERATOR_H
