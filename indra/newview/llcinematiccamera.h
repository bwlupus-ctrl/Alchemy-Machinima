/**
 * @file llcinematiccamera.h
 * @brief Automated cinematic camera: bone-lock (GoPro) and motion patterns.
 *
 * Takes over the render camera in the idle camera dispatch (below pilot,
 * recorder playback, and actor path cameras; above joystick/agent camera)
 * and drives it from one of several cinematic sources:
 *
 *   Bone Lock  -- rigidly attach the camera to an avatar skeleton joint,
 *                 like a GoPro strapped to that bone, with local position
 *                 and aim offsets, optional horizon lock and smoothing.
 *   Orbit      -- circle the target at a radius/height with optional bob.
 *   Fly Hover  -- drone/fly-like wander around the target on fBm noise,
 *                 always framing the target.
 *   Sweep      -- linear dolly pass by the target (ping-pong or loop).
 *   Crane      -- slow orbit while the height eases between two levels.
 *
 * The target is your own avatar, or the selected avatar when
 * CinematicCamUseSelected is set. Output can optionally be routed through
 * LLCameraOperator for the handheld texture on top of the pattern.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Phoenix / Firestorm Viewer Source Code
 * $/LicenseInfo$
 */

#ifndef LL_LLCINEMATICCAMERA_H
#define LL_LLCINEMATICCAMERA_H

#include "stdtypes.h"
#include "v3math.h"
#include "llquaternion.h"

class LLVOAvatar;
class LLViewerCamera;

class LLCinematicCamera
{
public:
    enum EMode
    {
        MODE_OFF        = 0,
        MODE_BONE_LOCK  = 1,
        MODE_ORBIT      = 2,
        MODE_FLY_HOVER  = 3,
        MODE_SWEEP      = 4,
        MODE_CRANE      = 5,
        // film-grammar modes: named for the emotion they carry
        MODE_DOLLY_ZOOM = 6,    // Vertigo: subject constant, world stretches (dread)
        MODE_PUSH_IN    = 7,    // slow creep to close-up (intimacy / tension)
        MODE_LOW_HERO   = 8,    // low-angle drift-arc push (power / awe)
        MODE_OVERHEAD   = 9,    // God's-eye rise + spin (isolation / fate)
        MODE_OTS        = 10,   // over-the-shoulder on the selected target (dialogue)
        MODE_CRASH_ZOOM = 11,   // snap zoom w/ overshoot, tripod-locked (shock)
        MODE_SLOW_ZOOM  = 12,   // Kubrick creep, position frozen (slow dread)
        MODE_WHIP_ARC   = 13,   // violent 0.4s arc around subject (transition energy)
        MODE_ARC        = 14,   // eased one-shot partial orbit, holds (the oner block)
        MODE_REVEAL     = 15,   // low-behind rise, focus lifts to the horizon (arrival)
        MODE_PULL_BACK  = 16,   // eased retreat + rise close->wide (farewell)
        MODE_TWO_SHOT   = 17,   // perpendicular to the self<->target line (dialogue master)
        MODE_LEAD_FOLLOW= 18,   // backpedal ahead of the subject (walk-and-talk)
        MODE_ECU_EYES   = 19,   // locked face micro-frame, narrow lens (standoff)
        MODE_LONG_LENS  = 20,   // distant compressed telephoto w/ drift (surveillance)
        MODE_SPIRAL     = 21,   // orbit tightening + rising as it turns (euphoria)
        MODE_PEDESTAL   = 22,   // boots-to-face vertical rise, level gaze (introduction)
        // acrobatic / dance / closeup / impact modes
        MODE_BARREL_ROLL  = 23, // hold + continuous/oscillating roll (dance energy)
        MODE_CORKSCREW    = 24, // rising, tightening orbit while rolling (aggressive spiral)
        MODE_PENDULUM     = 25, // eased horizontal swing arc, always on the subject
        MODE_CONTRA_ORBIT = 26, // orbit while the FOV warps (dolly-zoom-while-circling)
        MODE_FISHEYE_LUNGE= 27, // wide-lens push to the face and recoil (impact)
        MODE_FLOOR_SKIMMER= 28, // ground-level lateral track aimed up (low hero)
        MODE_BOOST_RISE   = 29, // accelerating vertical launch, tilting to hold frame
        MODE_BOOM_OVER    = 30, // jib arc up over the subject and down the far side
        MODE_TOP_SPIN     = 31, // locked straight overhead, spinning (Busby Berkeley)
        MODE_TURNTABLE    = 32, // subject-centered orbit, height eases (showcase crane)
        MODE_FLOATING_ECU = 33, // drifting intimate face close-up, narrow lens
        MODE_TILT_WHIP    = 34, // percussive vertical pitch whips (impact accent)
        MODE_BODY_HELIX   = 35, // slow feet-to-face orbiting reveal (introduction)
        MODE_DESCENT      = 36, // above-head descent to the boots (menace)
        MODE_PARALLAX_SLIDE=37, // straight lateral truck past the subject (depth)
        MODE_FIGURE_EIGHT = 38, // reversing two-lobe orbit (dance energy)
        MODE_DETAIL_SWEEP = 39, // close costume-height drift (admiration)
        MODE_CABLE_CAM    = 41, // fast straight chord pass (sports energy)
        MODE_BREATHING_HOLD=42, // nearly locked frame with subtle life (intimacy)
        // Director Switcher fixed framings. Appended: persisted values 0..42
        // remain stable.
        MODE_STATIC_WIDE    = 43,
        MODE_STATIC_MEDIUM  = 44,
        MODE_STATIC_CLOSE   = 45,
        MODE_STATIC_PROFILE_L=46,
        MODE_STATIC_PROFILE_R=47,
        MODE_STATIC_LOW     = 48,
        MODE_STATIC_HIGH    = 49,
        MODE_STATIC_FULL    = 50,
    };
    static_assert(MODE_BREATHING_HOLD == 42,
                  "Persisted legacy CineCam mode values must not move");
    static_assert(MODE_STATIC_WIDE == 43 && MODE_STATIC_FULL == 50,
                  "Switcher static modes must remain appended");

    static LLCinematicCamera& instance();

    // Shared aspect-aware Frame lens used by cinematic camera owners. Identity
    // when the lens is disabled or the target frame is Native.
    static F32 applyFrameLens(F32 plain_fov, LLViewerCamera* cam);

    // Session-only locked follow subject (right-click avatar > Cinematic Cam
    // Follow). Beats the selection-based targeting while set; toggling the
    // same avatar clears it. Null = stock behavior (selection / self).
    static void toggleFollowTarget(const LLUUID& id);
    static bool isFollowTarget(const LLUUID& id);
    static void onRuntimeTargetReplaced(const LLUUID& old_id,
                                        const LLUUID& new_id);

    // Stable display label shared by the CineCam panel, Director status, and
    // Director Switcher. Unknown values return "Unknown".
    static S32 migrateLegacyMode(S32 mode);
    static const char* modeName(S32 mode);

    // True when the system should own the render camera this frame.
    bool isActive() const;

    // True only while Bone Lock is actively mounted on this exact avatar.
    bool isActiveBoneLockTarget(const LLUUID& avatar_id) const;

    // True while a non-Bone-Lock shot is actively framing this avatar from its
    // head joint. Render-only joint overrides must not feed that camera input.
    bool isActiveHeadFramingTarget(const LLUUID& avatar_id) const;

    // True only while Flycam Orbit is the active camera driver and this avatar
    // resolves as its external-rider anchor.
    bool isActiveOrbitAnchor(const LLUUID& avatar_id) const;

    // Anchor transform for external riders (Flycam Orbit): the resolved
    // target's CinematicCamJoint world pose in agent region coordinates.
    // level_horizon strips bone roll/pitch so the frame's Z stays world-up.
    // Returns false when no target avatar resolves.
    bool resolveAnchor(LLVector3& pos, LLQuaternion& rot, bool level_horizon) const;

    // Snapshot the currently rendered camera as a subject-relative static rig.
    // FOV is returned in degrees. False means no valid subject/camera pose.
    bool captureCurrentSwitcherView(S32 subject,
                                     F32& yaw_offset_deg, F32& pitch_deg,
                                     F32& distance_m, F32& height_m,
                                     F32& fov_deg) const;

    // Queue one deterministic skeleton fit at the next cinematic-camera
    // sample. Used by the Frame tab's explicit Re-solve button.
    void requestAutoReframe();

    // Compute and write this frame's camera. Call from the idle camera
    // dispatch INSTEAD of gAgentCamera.updateCamera() when isActive().
    void updateCamera();

private:
    LLCinematicCamera() = default;

    LLVOAvatar* resolveTarget() const;
    LLVOAvatar* resolveDefaultTarget() const;
    LLVOAvatar* resolveMarkedTarget(S32 subject) const;
    LLVOAvatar* resolveSecondaryTarget() const;
    void captureMotionStart(LLVOAvatar* av, const LLVector3& center,
                            S32 mode, S32 switcher_slot, U64 shot_index);
    F32 motionStartAzimuth(F32 classic_azimuth) const
    {
        return mMotionStartClassic ? classic_azimuth : mMotionStartAzimuth;
    }

    // pattern generators: produce a desired camera position and the point
    // to frame, in agent region coordinates
    void patternBoneLock(LLVOAvatar* av, F32 phase, LLVector3& pos, LLQuaternion& rot, bool& have_rot);
    LLVector3 patternOrbit(const LLVector3& center, F32 phase);
    LLVector3 patternHover(const LLVector3& center, F32 phase);
    LLVector3 patternSweep(const LLVector3& center, F32 phase);
    LLVector3 patternCrane(const LLVector3& center, F32 phase);
    // film-grammar patterns. Some produce a FOV multiplier (dolly zoom), a
    // roll contribution (overhead spin) or their own focus point (OTS).
    LLVector3 patternDollyZoom(LLVOAvatar* av, const LLVector3& focus, F32 phase, F32& fov_mul);
    LLVector3 patternPushIn(LLVOAvatar* av, const LLVector3& focus, F32 phase);
    LLVector3 patternLowHero(LLVOAvatar* av, const LLVector3& center, F32 phase);
    LLVector3 patternOverhead(const LLVector3& center, F32 phase, F32& roll_out);
    LLVector3 patternOTS(LLVOAvatar* target, LLVector3& focus_io);
    LLVector3 patternCrashZoom(F32 phase, F32& fov_mul);
    LLVector3 patternSlowZoom(F32 phase, F32& fov_mul);
    LLVector3 patternWhipArc(LLVOAvatar* av, const LLVector3& center, F32 phase);
    LLVector3 patternArc(LLVOAvatar* av, const LLVector3& center, F32 phase);
    LLVector3 patternReveal(LLVOAvatar* av, const LLVector3& center, F32 phase, LLVector3& focus_io);
    LLVector3 patternPullBack(LLVOAvatar* av, const LLVector3& focus, F32 phase);
    LLVector3 patternTwoShot(LLVOAvatar* target, LLVector3& focus_io);
    LLVector3 patternLeadFollow(LLVOAvatar* av, const LLVector3& focus, F32 phase);
    LLVector3 patternECU(LLVOAvatar* av, const LLVector3& focus, F32 phase, F32& fov_mul);
    LLVector3 patternLongLens(LLVOAvatar* av, const LLVector3& focus, F32 phase, F32& fov_mul);
    LLVector3 patternSpiral(const LLVector3& center, F32 phase);
    LLVector3 patternPedestal(LLVOAvatar* av, const LLVector3& center, F32 phase, LLVector3& focus_io);
    // acrobatic / dance / closeup / impact patterns. Some output a roll
    // (barrel / corkscrew / top-spin), a FOV multiplier (contra-orbit /
    // fisheye lunge / floating ECU) or drive their own aim through the focus
    // point (tilt whip).
    LLVector3 patternBarrelRoll(LLVOAvatar* av, const LLVector3& center, F32 phase, F32& roll_out);
    LLVector3 patternCorkscrew(const LLVector3& center, F32 phase, F32& roll_out);
    LLVector3 patternPendulum(LLVOAvatar* av, const LLVector3& center, F32 phase);
    LLVector3 patternContraOrbit(const LLVector3& center, F32 phase, F32& fov_mul);
    LLVector3 patternFisheyeLunge(LLVOAvatar* av, const LLVector3& focus, F32 phase, F32& fov_mul);
    LLVector3 patternFloorSkimmer(LLVOAvatar* av, const LLVector3& center, F32 phase);
    LLVector3 patternBoostRise(LLVOAvatar* av, const LLVector3& center, F32 phase);
    LLVector3 patternBoomOver(LLVOAvatar* av, const LLVector3& center, F32 phase);
    LLVector3 patternTopSpin(const LLVector3& center, F32 phase, F32& roll_out);
    LLVector3 patternTurntable(const LLVector3& center, F32 phase);
    LLVector3 patternFloatingECU(LLVOAvatar* av, const LLVector3& focus, F32 phase, F32& fov_mul);
    LLVector3 patternTiltWhip(LLVOAvatar* av, const LLVector3& center, F32 phase, LLVector3& focus_io);
    LLVector3 patternBodyHelix(LLVOAvatar* av, const LLVector3& center, F32 phase, LLVector3& focus_io);
    LLVector3 patternDescent(LLVOAvatar* av, const LLVector3& center, F32 phase, LLVector3& focus_io);
    LLVector3 patternParallaxSlide(LLVOAvatar* av, const LLVector3& center, F32 phase);
    LLVector3 patternFigureEight(LLVOAvatar* av, const LLVector3& center, F32 phase);
    LLVector3 patternDetailSweep(LLVOAvatar* av, const LLVector3& center, F32 phase, LLVector3& focus_io, F32& fov_mul);
    LLVector3 patternCableCam(LLVOAvatar* av, const LLVector3& center, F32 phase);
    LLVector3 patternBreathingHold(LLVOAvatar* av, const LLVector3& center, F32 phase);
    LLVector3 patternStaticShot(LLVOAvatar* av, const LLVector3& center,
                                S32 mode, LLVector3& focus_io,
                                F32& fov_mul);
    void applyAutoReframe(LLVOAvatar* av, S32 mode, F32 plain_fov,
                          bool force_solve, bool allow_glide, LLVector3& pos,
                          LLVector3& focus);

    // ---- state ----
    U32         mLastUpdateFrame = 0;   // fresh-activation pose/state detection
    S32         mLastMode = MODE_OFF;
    LLUUID      mLastTargetId;
    LLVector3   mTripodPos = LLVector3::zero;   // camera pos captured at activation (zoom modes)
    bool        mWasActive = false;
    F32         mPhase = 0.f;           // wrapped pattern clock, seconds*speed
    F32         mPrevTime = 0.f;
    // Shared per-shot motion-start basis. Captured once after the subject
    // center resolves so CameraRelative never samples a moving camera mid-shot.
    bool        mMotionStartCaptured = false;
    bool        mMotionStartClassic = true;
    F32         mMotionStartAzimuth = 0.f;
    F32         mMotionDir = 1.f;
    U64         mMotionShotIndex = 0;
    // smoothed pose
    bool        mHavePose = false;
    LLVector3   mSmPos = LLVector3::zero;
    LLQuaternion mSmRot;
    // for feeding the camera operator with velocities
    LLVector3   mPrevPos = LLVector3::zero;
    LLQuaternion mPrevRot;
    // Director Switcher one-shot transition envelope. Legacy CineCam paths
    // never set a switcher cut serial and therefore never enter this branch.
    U64          mLastSwitcherCutSerial = 0;
    F64          mSwitcherPhaseAnchor = 0.0;
    bool         mEaseActive = false;
    F32          mEaseDuration = 0.f;
    S32          mEaseCurveId = 0;
    F32          mEaseBezier[4] = { 0.42f, 0.f, 0.58f, 1.f };
    F32          mEaseFeather = 0.f;
    LLVector3    mEaseFromPos = LLVector3::zero;
    LLQuaternion mEaseFromRot;
    F32          mEaseFromFov = 0.f;
    // Skeleton Auto-Reframe: a solved subject-relative rig held between
    // discrete changes. Manual trims are read live and never stored here.
    bool         mAutoFrameHaveSolve = false;
    F32          mAutoFrameSettleFromDistance = 0.f; // pre-trim distance captured at the last boundary
    F32          mAutoFrameSettleStartPhase = 0.f;   // mPhase at settle start
    S32          mAutoFrameSettleModeLatched = 0;
    F32          mAutoFrameSettleDurationLatched = 0.f;
    S32          mAutoFrameSettleCurveLatched = 0;
    F32          mAutoFrameSettleBezierLatched[4] = {
        0.42f, 0.f, 0.58f, 1.f
    };
    F32          mAutoFrameSettleFeatherLatched = 0.f;
    bool         mAutoFrameSettleActive = false;     // false == latched/settled
    F32          mAutoFrameAppliedDistance = 0.f;    // pre-trim distance actually applied last frame
    bool         mAutoFrameHaveApplied = false;
    bool         mAutoFrameEyeLevel = false;
    bool         mAutoFrameLastEnabled = false;
    bool         mAutoFrameLastLensEnabled = false;
    F32          mAutoFrameDistance = 0.f;
    F32          mAutoFrameFocusZOffset = 0.f;
    F32          mAutoFrameFocusXOffset = 0.f;
    F32          mAutoFrameFocusYOffset = 0.f;
    F32          mAutoFrameEyeZOffset = 0.f;
    F32          mAutoFrameEyeAimSlope = 0.f;
    F32          mAutoFrameLastFill = 0.f;
    F32          mAutoFrameLastCompose = 0.f;
    F32          mAutoFrameLastAspect = 0.f;
    F32          mAutoFrameLastCustomAspect = 0.f;
    F32          mAutoFrameLastFocalMM = 0.f;
    F32          mAutoFrameLastWindowAspect = 0.f;
    F32          mAutoFrameLastBaseFOV = 0.f;
    U32          mAutoFrameRequestSerial = 0;
    U32          mAutoFrameSolvedRequestSerial = 0;
};

#endif // LL_LLCINEMATICCAMERA_H
