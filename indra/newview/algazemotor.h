/**
 * @file algazemotor.h
 * @brief ALGazeMotor assembly core: per-actor gaze motor state + per-frame
 * step() stitching the validated leaf modules and ALTrajectory into one
 * coherent motor program (v1: live, tier-0 lids, per-actor affect).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALGAZEMOTOR_H
#define AL_ALGAZEMOTOR_H

#include "llmath.h"
#include "llquaternion.h"
#include "v3math.h"

#include "algazeblink.h"
#include "algazenoise.h"
#include "algazepolicy.h"
#include "algazerecruit.h"
#include "altrajectory.h"

#include <cmath>
#include <cstring>

// ALGazeMotor is the serial assembly layer of the Cinematic Gaze build spec
// (doc/CINEMATIC_GAZE_LIFE_BUILD_SPEC.md section 6A): a mostly-pure,
// unit-testable module that owns the per-actor motor STATE and a per-frame
// step() consuming the validated leaves:
//
//   ALTrajectory   - closed-form trajectory programs (every angular channel);
//                    C2 for ordinary gaze ranges -- extreme incoming
//                    derivatives fall back to ALTrajectory's bounded C0
//                    settle, made unreachable here by the handoff clamps
//   ALGazePolicy   - eye main-sequence duration, head latency, VOR solve
//   ALGazeRecruit  - soft anatomical recruitment (band 0 = legacy hard knee)
//   ALGazeNoise    - deterministic drift + microsaccade micro-life
//   ALGazeBlink    - asymmetric blink SHAPE (the SCHEDULER lives here)
//
// v1 decisions honored (spec section 6): LIVE-ONLY (motor state persists
// across frames; no event-sourcing/checkpoint/replay -- the closed-form
// layers, blink timing / drift / microsaccade, stay pure functions of
// presentation time + seed and remain scrub-safe); TIER-0 lids (a single lid
// closure L/R in [0,1] for the existing Blink_Left/Right channel, no Bento
// bone logic); PER-ACTOR Affect (one {valence, arousal, dominance} input, no
// timeline keyframing).
//
// Deliberately NOT here (viewer integration concerns, spec section 2.6):
// joint lookup, apply/restore/capture, animation priority, cue arbitration,
// settings plumbing. The llactormover integration calls step() each frame and
// maps GazeMotorPose onto joints + the Blink_* visual params. Lid-follow and
// the full five-stage lid composition of spec 2.5 also stay in the
// integration (they need the applied eye pitch); this module contributes
// stages (2) aperture posture and (3) blink override, pre-folded.
namespace ALGazeMotor
{

// Bump when any constant below changes motion (spec section 2.4: detect, do
// not silently reinterpret). Stamped into every trajectory segment built here.
// v2: unwrapped (continuous) recruitment input across the +/-pi seam;
// configured blink refractory enforced on the spontaneous lattice; symmetric
// evoked/spontaneous arbitration (duration + refractory); minimum absolute
// neck/torso stagger; C2 handoff derivative clamps; microsaccade rate
// clamped to a sub-tremor bound; band-0 recruitment made bit-exact legacy.
constexpr U32 BEHAVIOR_VERSION = 2;

// ---------------------------------------------------------------------------
// Channels
// ---------------------------------------------------------------------------
// Each channel is a full-chain AIM trajectory (the total desired gaze
// yaw/pitch in the reference frame), not a per-joint angle: the eye group
// tracks the aim fast, the head group follows late and slow, neck/torso
// stagger further. Per-frame recruitment (see step 3 below) then converts
// each group's sampled aim into that group's own joint contribution, so when
// every group has converged the distributed pose equals the legacy
// distributeAnatomicalChain result for the same aim.
enum EChannel : S32
{
    CH_EYE_YAW = 0,
    CH_EYE_PITCH,
    CH_HEAD_YAW,
    CH_HEAD_PITCH,
    CH_HEAD_ROLL,
    CH_NECK_YAW,
    CH_NECK_PITCH,
    CH_TORSO_YAW,
    CH_TORSO_PITCH,
    CHANNEL_COUNT
};

// Recruitment chain slots (matching distributeAnatomicalChain's fill order).
constexpr S32 CHAIN_JOINTS = 5; // eyes, head, neck, torso, hips

// Retarget hysteresis: reuse the Director dead-zone replacement values
// (spec 2.4 / llactormover.cpp:3147-3151 DIRECTOR_BODY_TURN_SWITCH = 3 deg,
// DIRECTOR_BODY_TURN_DEBOUNCE = 0.12 s). The desired target must stay beyond
// the enter threshold CONTINUOUSLY for the dwell before a new motor program
// is committed; sub-threshold wander never touches the running programs.
constexpr F32 RETARGET_ENTER_DEG  = 3.f;
constexpr F64 RETARGET_DWELL_SEC  = 0.12;

// Immediate-acquisition dead-band. When the integration signals an activation
// edge (mAcquire, or a changed mTargetGeneration -- gaze (re)becoming active,
// or the operator selecting a new gaze method while the master gate stays on)
// the motor commits to the current target on THIS frame, bypassing the dwell,
// so acquisition is instant like the legacy path. Below this angle the acquire
// edge is a no-op (re-selecting the same direction), so the retarget counter
// and the gaze-evoked blink are never bumped spuriously. The dwell hysteresis
// above still governs ordinary, unsignaled target motion (small wander is
// damped; a genuine sustained move commits after the 120 ms dwell), so the
// anti-thrash behavior is unchanged whenever no activation edge is signaled.
constexpr F32 RETARGET_ACQUIRE_EPS_DEG = 0.05f;

// Minimum absolute neck/torso stagger past the previous stage's start time.
// ADDITIVE, not multiplicative: when ALGazePolicy::headLatencyMs legitimately
// returns ~0 (high predictability / strong head-mover trait) a purely
// multiplicative scale would start head, neck, and torso simultaneously;
// this floor keeps the anatomical wave ordered (neck strictly after head,
// torso strictly after neck) in every configuration.
constexpr F64 MIN_STAGGER_SEC = 0.004;

// C2 handoff derivative clamps (see detail::retargetChannel). ALTrajectory's
// solveScalar carries a documented residual C0 escape for extreme incoming
// derivatives against near-zero remaining travel; these bounds keep every
// handoff provably on the C2 path (proof at the clamp site) while sitting
// far above anything an ordinary gaze program produces (peak eye-channel
// velocity is ~54 rad/s: a pi-radian saccade at the 110 ms main-sequence
// duration cap), so they are inert in normal operation.
constexpr F32 HANDOFF_MAX_VEL_RAD_SEC  = 100.f;
constexpr F32 HANDOFF_MAX_ACC_RAD_SEC2 = 1.0e5f;

// Microsaccade rate forwarded to ALGazeNoise is clamped to this sub-tremor
// bound at the motor boundary: no configuration may synthesize the
// ~40-100 Hz fixational tremor band the spec explicitly excludes (spec
// section 5: "No true-tremor channel ships"; algazenoise.h documents the
// physiological rate as ~1-2 Hz). 6 Hz leaves generous stylization headroom
// while staying an order of magnitude below the tremor band.
constexpr F32 MICROSACCADE_MAX_RATE_HZ = 6.f;

// ALGazeNoise channel ids used by this module (decorrelated by the noise
// hash; values are arbitrary but must stay stable -- they are part of the
// deterministic output).
constexpr U32 NOISE_CH_EYE_YAW    = 11;
constexpr U32 NOISE_CH_EYE_PITCH  = 12;
constexpr U32 NOISE_CH_HEAD_YAW   = 13;
constexpr U32 NOISE_CH_HEAD_PITCH = 14;
constexpr U32 NOISE_CH_BLINK      = 30; // spontaneous blink lattice
constexpr U32 NOISE_CH_EVOKE      = 31; // gaze-evoked probability draws
constexpr U32 NOISE_CH_ASYM       = 32; // per-eye blink asymmetry

// ---------------------------------------------------------------------------
// POD structs
// ---------------------------------------------------------------------------

// Per-actor affect (spec 2.3 / 6A, Mehrabian-Russell PAD). v1: set once per
// actor; keyframing is a later addition. Neutral is (0, 0.5, 0).
struct AffectState
{
    F32 mValence   = 0.f;  // [-1, 1]
    F32 mArousal   = 0.5f; // [0, 1]; 0.5 = neutral energy
    F32 mDominance = 0.f;  // [-1, 1]
};

// Tunables, mapped by the integration from the Director settings named in
// spec section 3. All defaults are the spec's documented population priors.
struct GazeMotorSettings
{
    // Master gate (DirectorGazeMotionPrograms, spec 10.6): when false step()
    // is a documented passthrough -- no new math evaluated, neutral pose out,
    // state dropped so re-enabling re-initializes on the current target.
    bool mMasterGate = false;

    // Eye main sequence (DirectorGazeEyeDurationBaseMs / PerDegMs).
    F32 mEyeDurationBaseMs   = 25.f;
    F32 mEyeDurationPerDegMs = 2.8f;

    // Head latency policy inputs (DirectorGazeHeadLatencyMs + traits).
    F32 mHeadLatencyBaseMs = 40.f;
    F32 mPredictability01  = 0.f;
    F32 mHeadMoverTrait01  = 0.f;

    // Head follow-through duration (motion-design prior, not physiology:
    // heads settle over a few hundred ms) and the neck/torso stagger scales.
    F32 mHeadDurationBaseMs   = 150.f;
    F32 mHeadDurationPerDegMs = 5.f;
    F32 mHeadDurationMaxMs    = 600.f;
    F32 mNeckLatencyScale     = 1.75f; // x head latency
    F32 mNeckDurationScale    = 1.25f; // x head duration
    F32 mTorsoLatencyScale    = 2.5f;
    F32 mTorsoDurationScale   = 1.6f;

    // Soft recruitment (DirectorGazeSoftRecruitDeg): 0 = legacy hard knee.
    F32 mSoftRecruitBandDeg = 0.f;
    // Chain shaping, mirroring distributeAnatomicalChain's weight inputs so
    // the recruited output drives the same joints with the same authored
    // distribution controls.
    F32 mHeadEyeBlend = 1.f; // [0,1]
    F32 mTorsoAmount  = 1.f; // [0,1]
    // Anatomy exaggeration, mirroring distributeAnatomicalChain's
    // anatomy_scale (algazemath.h ~598-615): scales the eye/head/neck
    // capacities only (chest/hips keep authored limits), clamped to [1, 3],
    // with the 1x path using the exact unscaled constants so the default
    // reproduces the pre-exaggeration result bit-for-bit.
    F32 mAnatomyScale = 1.f; // [1,3]

    // VOR comfort cone (per-axis soft limit, degrees). Defaults match the
    // eye capacities in distributeAnatomicalChain (25 yaw / 14 pitch).
    F32 mComfortYawDeg   = 25.f;
    F32 mComfortPitchDeg = 14.f;

    // Micro-life (post-smoothing additive; arousal-scaled below).
    F32 mDriftCellSec       = 2.4f;                 // head drift lattice cell
    F32 mEyeDriftAmpRad     = 0.15f * DEG_TO_RAD;
    F32 mHeadDriftAmpRad    = 0.5f * DEG_TO_RAD;
    F32 mMicrosaccadeRateHz = 1.4f;
    F32 mMicrosaccadeAmpRad = 0.12f * DEG_TO_RAD;
    F32 mMicroArousalGain   = 1.f; // amplitude scale = 0.5 + gain * arousal

    // Blink scheduler (DirectorGazeBlinkCloseMs / OpenMs / PartialChance).
    F32 mBlinkRate          = 1.f;   // multiplier on the ~4.5 s lattice; 0 = off
    F32 mBlinkCloseMs       = ALGazeBlink::DEFAULT_CLOSE_MS;
    F32 mBlinkHoldMs        = ALGazeBlink::DEFAULT_HOLD_MS;
    F32 mBlinkOpenMs        = ALGazeBlink::DEFAULT_OPEN_MS;
    F32 mBlinkPartialChance = 0.3f;
    F32 mBlinkRefractorySec = 0.5f;  // min extra gap between blink onsets
    F32 mGazeEvokedBlink    = 1.f;   // probability scale; 0 disables evoked

    // Affect couplings (documented modest priors, spec 8.3: arousal is a
    // style gain, not the law).
    F32 mTempoArousalGain = 0.25f; // duration scale 1 -/+ gain at arousal 1/0
    F32 mDominanceRollDeg = 2.f;   // head-tilt bias at |dominance| = 1

    // Cinematic subtlety controls (default 0 = byte-identical). Stillness
    // freezes the recruited HEAD/NECK/TORSO contribution toward zero while
    // leaving the eyes and all micro-life fully alive ("the camera captures
    // the internal thought"). Restraint additionally shrinks the head-turn
    // MAGNITUDE (its micro-life-amplitude and blink-rate reduction is folded
    // into the amplitude/rate fields above by the integration). Both scale the
    // recruited body output by RESTRAINT_HEADTURN_K below.
    F32 mStillness = 0.f; // [0,1]
    F32 mRestraint = 0.f; // [0,1]
};

// Restraint's fractional reduction of the recruited head-turn magnitude at
// restraint == 1 (so 1.0 turns the head/neck/torso half as far -- restrained,
// not frozen). Stillness handles the full freeze separately; the two combine
// multiplicatively. Micro-life amplitude and blink-rate restraint are applied
// upstream (integration) by scaling the settings' amplitude/rate fields, so
// they are shared by the legacy path and need no constant here.
constexpr F32 RESTRAINT_HEADTURN_K = 0.5f;

// Per-frame input. The aim is expressed as chain yaw/pitch (radians) in the
// caller's reference frame (the same frame llactormover's gaze solve derives
// its target yaw/pitch in); mRefWorldRot maps that frame to world for the
// VOR solve against mHeadWorldRot (the actual sampled head orientation).
struct GazeMotorInput
{
    F32 mTargetYaw   = 0.f; // desired total chain aim, radians
    F32 mTargetPitch = 0.f; // positive looking down (viewer convention)
    F32 mTargetRoll  = 0.f; // desired head roll (dutch/cue), radians

    LLQuaternion mHeadWorldRot; // current head world rotation (VOR input)
    LLQuaternion mRefWorldRot;  // frame the aim yaw/pitch are measured in

    F64 mTimeSeconds = 0.0; // presentation time
    F32 mDeltaTime   = 0.f; // frame dt (informational; motor is closed-form)
    U64 mSeed        = 0;   // per-actor seed (castSeedFromUUID pattern)
    F32 mCueWeight   = 1.f; // integration passthrough (cue/priority weight);
                            // copied to the pose, never consumed here

    // Activation edge (spec 2.6 integration): the integration sets mAcquire on
    // the frame gaze (re)becomes active or the operator selects a new gaze
    // method while the master gate stays on, forcing the motor to acquire the
    // current target immediately (no retarget dwell). mTargetGeneration is a
    // cheap signature of the DISCRETE target selection (mode + cast/object
    // reference): the motor stores the last value it saw (mLastTargetGen) so
    // the integration can detect a method switch by comparing this frame's
    // generation against it, and a change is honored as an implicit acquire
    // even if mAcquire was not set. Default 0/false reproduces the pre-fix
    // behavior exactly (existing callers never touch these), so the dwell
    // hysteresis is unchanged unless an edge is signaled.
    bool mAcquire          = false;
    U32  mTargetGeneration = 0;

    AffectState      mAffect;
    GazeMotorSettings mSettings;
};

// One aim channel: the active closed-form program plus at most one pending
// program that has not reached its start time yet (head/neck/torso start
// after their latencies). Programs are pure values; the last-sampled
// (p, v, a) the spec asks the state to carry is recovered exactly by
// sampling mProgram at any time, so no separate cache is stored.
struct ChannelState
{
    ALTrajectory::ScalarProgram mProgram;
    ALTrajectory::ScalarProgram mPending;
    bool mPendingValid = false;
};

// Per-actor persistent motor state (live mode: carrying trajectory-segment
// state across frames is fine; blink/drift/microsaccade timing stays
// closed-form of time + seed and keeps no per-frame state here).
struct GazeMotorState
{
    bool mInitialized = false;

    ChannelState mChannels[CHANNEL_COUNT];

    // Last committed target (wrapped) + retarget hysteresis state.
    F32  mCommittedYaw   = 0.f;
    F32  mCommittedPitch = 0.f;
    F32  mCommittedRoll  = 0.f;
    bool mCandidateValid = false;
    F64  mCandidateSince = 0.0;
    U64  mRetargetCounter = 0; // also the deterministic evoked-blink draw key

    // Last discrete target signature the motor saw (see GazeMotorInput::
    // mTargetGeneration). Updated every gated frame; a mismatch against the
    // incoming generation is an operator method switch and forces immediate
    // acquisition. mHasTargetGen guards the very first gated frame, where there
    // is no prior generation to compare against (initializeState already snaps
    // to the current target, so that frame must NOT be treated as an edge).
    U32  mLastTargetGen = 0;
    bool mHasTargetGen  = false;

    // Gaze-evoked blink (the one live blink source that needs state).
    // LIVE-mode only, not scrub-safe: the onset is the retarget commit time
    // and is deterministic given that commit time + seed (see the blink
    // scheduler determinism contract below); the future recorded/
    // event-sourced mode will re-derive it from recorded commit events.
    F64 mEvokedBlinkStart = -1.0e18;
    F32 mEvokedBlinkDepth = 0.f;

    // Last frame's post-VOR eye-in-head (pre-micro), for the head-latency
    // policy's initial-eye-eccentricity input on the next retarget.
    F32 mLastEyeInHeadYaw   = 0.f;
    F32 mLastEyeInHeadPitch = 0.f;
};

// Per-frame output. The integration maps these onto joints and the
// Blink_Left/Right visual params (Tier 0). All angles are radians.
struct GazeMotorPose
{
    // Post-VOR, post-micro eye-in-head.
    F32 mEyeYaw   = 0.f;
    F32 mEyePitch = 0.f;

    // Recruited joint contributions (drift already added to head, post-
    // smoothing additive, matching llactormover.cpp:4744's ordering).
    F32 mHeadYaw   = 0.f;
    F32 mHeadPitch = 0.f;
    F32 mHeadRoll  = 0.f;
    F32 mNeckYaw   = 0.f;
    F32 mNeckPitch = 0.f;
    F32 mTorsoYaw  = 0.f;
    F32 mTorsoPitch = 0.f;
    F32 mHipsYaw   = 0.f;
    F32 mHipsPitch = 0.f;

    // The leading (eye-group) chain aim, wrapped -- usable by an integration
    // that prefers to run distributeAnatomicalChain itself.
    F32 mChainAimYaw   = 0.f;
    F32 mChainAimPitch = 0.f;

    // Tier-0 lids: single closure per eye in [0,1] (posture + blink folded).
    F32 mLidClosureLeft  = 0.f;
    F32 mLidClosureRight = 0.f;
    // Aperture widen intent in [0,1] (arousal); Tier 0 cannot open lids past
    // rest, so this is surfaced for the integration/cue layer to consume.
    F32 mApertureWiden = 0.f;

    // Fix 1 (VOR against the FINAL painted head, spec 2.6/10.5): the desired
    // WORLD gaze direction the eyes must fixate -- the eye-group aim mapped
    // through mRefWorldRot, carrying the eye saccade dynamics. mEyeYaw/mEyePitch
    // above are solved HERE against the PRE-paint mHeadWorldRot and remain a
    // convenience/back-compat passthrough (and what the unit tests assert); the
    // integration re-solves eye-in-head against the NOW-painted head via
    // ALGazePolicy::eyeInHeadFromWorldGaze(mDesiredWorldGaze, painted_head, ...)
    // so the eyes fixate the target given the actual painted head, then re-adds
    // the micro-life offset below. This avoids double-counting the head rotation
    // that the recruited neck/head paint introduces after step() runs.
    LLVector3 mDesiredWorldGaze;
    // Post-VOR micro-life eye offset (drift + microsaccade), already folded into
    // mEyeYaw/mEyePitch; surfaced separately so the integration's post-paint
    // eye-in-head solve can re-add the SAME deterministic micro-life.
    F32 mEyeYawMicro   = 0.f;
    F32 mEyePitchMicro = 0.f;

    F32  mCueWeight = 1.f;  // passthrough from the input
    bool mActive    = false; // false = master gate off (documented passthrough)
};

// ---------------------------------------------------------------------------
// Affect couplings (documented priors; spec 6A step 7 and 8.3)
// ---------------------------------------------------------------------------

// Arousal tempo: shortens head/neck/torso trajectory durations and the head
// latency within clamps. The eye main sequence is deliberately NOT scaled --
// saccade durations are physiological, not performative.
inline F32 tempoScale(const AffectState& affect, const GazeMotorSettings& s)
{
    const F32 arousal = std::isfinite(affect.mArousal)
        ? llclamp(affect.mArousal, 0.f, 1.f) : 0.5f;
    const F32 gain = std::isfinite(s.mTempoArousalGain)
        ? llclamp(s.mTempoArousalGain, 0.f, 0.4f) : 0.25f;
    return llclamp(1.f - gain * (arousal - 0.5f) * 2.f, 0.6f, 1.4f);
}

// Blink rate style factor: base x style, where arousal is one modest input
// and low valence adds a little (distress blinks more) -- NOT a raw arousal
// dial (spec 2.2 blink-rate law).
inline F32 blinkStyleScale(const AffectState& affect)
{
    const F32 arousal = std::isfinite(affect.mArousal)
        ? llclamp(affect.mArousal, 0.f, 1.f) : 0.5f;
    const F32 valence = std::isfinite(affect.mValence)
        ? llclamp(affect.mValence, -1.f, 1.f) : 0.f;
    return llclamp(1.f + 0.5f * (arousal - 0.5f) + 0.35f * llmax(-valence, 0.f),
                   0.25f, 2.5f);
}

// Aperture widen intent [0,1]: high arousal widens (counteracting droop).
inline F32 apertureWiden01(const AffectState& affect)
{
    const F32 arousal = std::isfinite(affect.mArousal)
        ? llclamp(affect.mArousal, 0.f, 1.f) : 0.5f;
    return llclamp((arousal - 0.6f) / 0.4f, 0.f, 1.f);
}

// Aperture posture: resting lid closure in [0, 0.5]. Low valence and low
// energy droop; widen pulls the droop back toward rest.
inline F32 aperturePosture(const AffectState& affect)
{
    const F32 valence = std::isfinite(affect.mValence)
        ? llclamp(affect.mValence, -1.f, 1.f) : 0.f;
    const F32 arousal = std::isfinite(affect.mArousal)
        ? llclamp(affect.mArousal, 0.f, 1.f) : 0.5f;
    const F32 droop = 0.30f * llmax(-valence, 0.f) +
                      0.20f * llmax(0.35f - arousal, 0.f) / 0.35f;
    return llclamp(droop * (1.f - 0.75f * apertureWiden01(affect)), 0.f, 0.5f);
}

// ---------------------------------------------------------------------------
// Recruitment capacities
// ---------------------------------------------------------------------------
// Must mirror ALGazeMath::distributeAnatomicalChain (algazemath.h ~565-654):
// same per-joint capacities, same anatomy_scale application (eye/head/neck
// only; exact constants on the 1x path), same eye/head/torso weight shaping,
// and the same `blend <= 0.001` eye-only early return (which gives the eyes
// their FULL unweighted capacity and every other joint exactly zero), so
// band 0 reproduces the legacy distribution BIT-EXACTLY for a converged aim
// and the motor output can drive the same joints. (The constants are local
// to that function, hence restated here; a divergence is a behavior-version
// bump.)
inline void effectiveCapacities(const GazeMotorSettings& s,
                                F32* caps_yaw, F32* caps_pitch)
{
    const F32 EYE_MAX_YAW    = 25.f * DEG_TO_RAD;
    const F32 EYE_MAX_PITCH  = 14.f * DEG_TO_RAD;
    const F32 HEAD_MAX_YAW   = 35.f * DEG_TO_RAD;
    const F32 HEAD_MAX_PITCH = 42.f * DEG_TO_RAD;
    const F32 NECK_MAX_YAW   = 35.f * DEG_TO_RAD;
    const F32 NECK_MAX_PITCH = 26.f * DEG_TO_RAD;
    const F32 TORSO_MAX_YAW  = 45.f * DEG_TO_RAD;
    const F32 TORSO_MAX_PITCH = 20.f * DEG_TO_RAD;
    const F32 HIPS_MAX_YAW   = 35.f * DEG_TO_RAD;
    const F32 HIPS_MAX_PITCH = 15.f * DEG_TO_RAD;

    // Anatomy exaggeration expands only the face/head chain, exactly as
    // algazemath.h:602-615: clamp to [1,3], keep the exact constants on the
    // 1x path so the default is bit-for-bit the pre-exaggeration result.
    const F32 scale = std::isfinite(s.mAnatomyScale)
        ? llclamp(s.mAnatomyScale, 1.f, 3.f) : 1.f;
    const F32 eye_max_yaw = scale == 1.f
        ? EYE_MAX_YAW : EYE_MAX_YAW * scale;
    const F32 eye_max_pitch = scale == 1.f
        ? EYE_MAX_PITCH : EYE_MAX_PITCH * scale;
    const F32 head_max_yaw = scale == 1.f
        ? HEAD_MAX_YAW : HEAD_MAX_YAW * scale;
    const F32 head_max_pitch = scale == 1.f
        ? HEAD_MAX_PITCH : HEAD_MAX_PITCH * scale;
    const F32 neck_max_yaw = scale == 1.f
        ? NECK_MAX_YAW : NECK_MAX_YAW * scale;
    const F32 neck_max_pitch = scale == 1.f
        ? NECK_MAX_PITCH : NECK_MAX_PITCH * scale;

    const F32 blend = std::isfinite(s.mHeadEyeBlend)
        ? llclamp(s.mHeadEyeBlend, 0.f, 1.f) : 1.f;
    const F32 torso = std::isfinite(s.mTorsoAmount)
        ? llclamp(s.mTorsoAmount, 0.f, 1.f) : 1.f;

    // Legacy eye-only early return (algazemath.h:620-625): at blend <=
    // 0.001 the eyes get their FULL (unweighted) capacity and the rest of
    // the chain gets exactly zero -- expressed here as capacities so the
    // same allocation loop reproduces it bit-exactly (min(residual, 0) is
    // exactly 0 for every downstream joint).
    if (blend <= 0.001f)
    {
        caps_yaw[0]   = eye_max_yaw;
        caps_pitch[0] = eye_max_pitch;
        for (S32 i = 1; i < CHAIN_JOINTS; ++i)
        {
            caps_yaw[i]   = 0.f;
            caps_pitch[i] = 0.f;
        }
        return;
    }

    const F32 eye_w   = 1.f - 0.75f * blend;
    const F32 head_w  = blend;
    const F32 torso_w = blend * torso;

    caps_yaw[0] = eye_max_yaw * eye_w;
    caps_yaw[1] = head_max_yaw * head_w;
    caps_yaw[2] = neck_max_yaw * head_w;
    caps_yaw[3] = TORSO_MAX_YAW * torso_w;
    caps_yaw[4] = HIPS_MAX_YAW * torso_w;
    caps_pitch[0] = eye_max_pitch * eye_w;
    caps_pitch[1] = head_max_pitch * head_w;
    caps_pitch[2] = neck_max_pitch * head_w;
    caps_pitch[3] = TORSO_MAX_PITCH * torso_w;
    caps_pitch[4] = HIPS_MAX_PITCH * torso_w;
}

// Bit-exact restatement of distributeAnatomicalChain's allocation loop
// (algazemath.h:634-653): this_joint = min(residual, max(capacity, 0)),
// residual -= this_joint. Used for the band == 0 acceptance gate INSTEAD of
// ALGazeRecruit::recruitJoint, whose algebraically equivalent
// `residual - max(residual - capacity, 0)` reconstruction is not portably
// bit-identical to the direct min under FP rounding.
inline void recruitChainLegacyExact(F32 target_magnitude, const F32* caps,
                                    F32* out_joints, S32 count)
{
    const F32 safe = std::isfinite(target_magnitude) ? target_magnitude : 0.f;
    const F32 sign = safe >= 0.f ? 1.f : -1.f;
    F32 residual = fabsf(safe);
    for (S32 i = 0; i < count; ++i)
    {
        const F32 amount = llmin(residual, llmax(caps[i], 0.f));
        residual -= amount;
        F32 signed_amount = sign * amount;
        // Canonicalize -0.0f -> +0.0f via a BIT-PATTERN rewrite, not a
        // floating-point comparison. Legacy's blend <= 0.001 eye-only early
        // return (algazemath.h:620-625) leaves every downstream field at its
        // default-constructed +0.0f -- it never computes sign * 0 for those
        // slots -- so for a negative target this loop's sign * 0 would
        // otherwise emit -0.0f where legacy has +0.0f. A value-domain fix
        // (e.g. `x == 0.f ? 0.f : x`) does NOT reliably survive: this build
        // compiles Release/RelWithDebInfo with /fp:fast (00-Common.cmake),
        // under which the optimizer is free to treat that ternary as
        // equivalent to plain `x` and elide the rewrite entirely (observed:
        // it did, in this exact spot, before this fix). Rewriting the raw
        // bit pattern is an integer operation the fast-math model has no
        // license to "simplify" away, so it survives optimization.
        U32 bits;
        std::memcpy(&bits, &signed_amount, sizeof(bits));
        if (bits == 0x80000000u) // IEEE-754 negative zero
        {
            bits = 0u;
            std::memcpy(&signed_amount, &bits, sizeof(bits));
        }
        out_joints[i] = signed_amount;
    }
}

// True when effectiveCapacities took its blend <= 0.001 eye-only early
// return: the eyes get full (unweighted) capacity and every downstream
// (head/neck/torso/hips) capacity is EXACTLY zero. Shared by
// effectiveCapacities and recruitSlot so the two stay in lockstep on the
// same threshold.
inline bool isEyeOnlyBlend(const GazeMotorSettings& s)
{
    const F32 blend = std::isfinite(s.mHeadEyeBlend)
        ? llclamp(s.mHeadEyeBlend, 0.f, 1.f) : 1.f;
    return blend <= 0.001f;
}

// One group's recruited contribution: run the full chain on that group's
// sampled aim and keep this group's slot. Band 0 routes through the exact
// legacy hard-knee formulas above (bit-identical to
// distributeAnatomicalChain's allocation); band > 0 uses the C2 soft chain
// -- UNLESS `eye_only` is set, in which case the exact path is used
// regardless of band. Eye-only mode gives every downstream joint EXACTLY
// zero capacity, and ALGazeRecruit::smoothExcess's knee sits at
// `residual - capacity`: with capacity == 0 and an eye-saturated residual of
// exactly 0 arriving at that joint, `smoothExcess(0, band)` evaluates
// mid-band (0.15625 * band for a symmetric quintic), handing that downstream
// joint a nonzero, wrong-signed contribution at zero aim -- a sign
// discontinuity across zero that the soft path was never meant to produce
// when there is no chain left to soft-recruit into. The exact allocator
// (min/max, no band term) cannot manufacture that spurious excess: a zero
// residual against a zero capacity is exactly zero, for every sign of aim.
// Each group's aim is C2 in time for ordinary gaze ranges (see
// retargetChannel's handoff derivative clamps; extreme incoming derivatives
// fall back to ALTrajectory's bounded C0 settle) and recruitChain is C2 in
// its input for band > 0, so each joint output inherits the trajectory's
// continuity; the eye-only exact path is likewise continuous (it is exactly
// zero on every downstream slot, identically, on both sides of zero aim).
inline F32 recruitSlot(F32 aim, const F32* caps, F32 band, S32 slot,
                       bool eye_only = false)
{
    F32 joints[CHAIN_JOINTS];
    if (!(band > 0.f) || eye_only)
    {
        recruitChainLegacyExact(aim, caps, joints, CHAIN_JOINTS);
    }
    else
    {
        ALGazeRecruit::recruitChain(aim, caps, joints, CHAIN_JOINTS, band);
    }
    return joints[slot];
}

// ---------------------------------------------------------------------------
// Blink scheduler (spec 6A step 6; the SHAPE is ALGazeBlink's)
// ---------------------------------------------------------------------------
// Spontaneous blinks live on a closed-form hashed lattice mirroring
// algazemath.h's evalMicroLife blink cells: period = max(clamp(4.5 / rate,
// 1.8, 18), min_gap + 0.3) seconds, one hashed onset per cell at
// cell_start + 0.2 + h * (period - min_gap), where min_gap =
// max(configured refractory, 0.5). By CONSTRUCTION consecutive onsets are
// always >= min_gap apart (latest onset of one cell to earliest onset of
// the next), so both the lattice's intrinsic 0.5 s floor AND any configured
// refractory above it are enforced between spontaneous onsets; with the
// default 0.5 s refractory this reproduces the original lattice
// bit-exactly. The refractory also arbitrates against gaze-evoked blinks
// symmetrically (see commitRetarget): a spontaneous onset inside an evoked
// blink's envelope + refractory is suppressed, and an evoked onset inside a
// spontaneous blink's envelope + refractory is refused.
//
// DETERMINISM CONTRACT (v1 LIVE-only decision, spec section 6 item 1):
//   - Spontaneous blinks are a PURE closed-form function of (seed, time,
//     rate, envelope, refractory): frame-cadence-independent and scrub-safe.
//   - Gaze-EVOKED blinks are a live-event response: they fire on a retarget
//     COMMIT, and in live mode the commit instant itself is not closed-form
//     (it lands on the first frame past the hysteresis dwell). Within that
//     constraint the evoked onset is INTERNALLY deterministic: a pure
//     function of the commit time, the per-actor seed, and the committed
//     retarget count (the probability draw hashes the retarget counter) --
//     never of call cadence beyond the commit itself. Same committed
//     retarget time + seed => bit-identical evoked blink.
//   - Evoked blinks are therefore live-mode only (NOT scrub-safe); the
//     future recorded/event-sourced mode will re-derive them from recorded
//     commit events. Revisit when that mode lands.

struct BlinkLatticeSample
{
    F32 mClosure   = 0.f;
    F64 mPrevOnset = -1.0e18; // most recent unsuppressed onset <= t
    F64 mNextOnset =  1.0e18; // earliest unsuppressed onset > t
};

namespace detail
{
// Hashed onset inside one lattice cell. `min_gap` is the guaranteed minimum
// spacing between consecutive cells' onsets: the hashed range is
// period - min_gap, so onset(k)_earliest - onset(k-1)_latest == min_gap
// exactly. Callers keep period >= min_gap + 0.3 so the 0.1 range floor
// (raw-input guard) never erodes the guarantee.
inline F64 spontOnsetForCell(U64 seed, S64 cell, F64 period, F64 min_gap)
{
    const F32 h = ALGazeNoise::unitHash(
        seed, NOISE_CH_BLINK, static_cast<U64>(cell), 0, 1);
    const F64 range = llmax(period - min_gap, 0.1);
    return static_cast<F64>(cell) * period + 0.2 +
           static_cast<F64>(h) * range;
}

inline F32 spontDepthForCell(U64 seed, S64 cell, F32 partial_chance)
{
    const F32 chance = std::isfinite(partial_chance)
        ? llclamp(partial_chance, 0.f, 1.f) : 0.f;
    const F32 roll = ALGazeNoise::unitHash(
        seed, NOISE_CH_BLINK, static_cast<U64>(cell), 0, 2);
    if (roll < chance)
    {
        return 0.6f + 0.35f * ALGazeNoise::unitHash(
            seed, NOISE_CH_BLINK, static_cast<U64>(cell), 0, 3);
    }
    return 1.f;
}
} // namespace detail

// Closed-form spontaneous blink closure at time t, plus the neighboring
// onset times for refractory arbitration. Onsets falling inside the evoked
// blink's shadow (evoked onset .. evoked onset + total + refractory) are
// suppressed so an evoked blink is never immediately doubled.
inline BlinkLatticeSample sampleSpontaneousBlink(
    U64 seed, F64 t, F32 rate,
    F32 close_ms, F32 hold_ms, F32 open_ms, F32 partial_chance,
    F64 evoked_onset, F32 refractory_sec)
{
    BlinkLatticeSample out;
    if (!std::isfinite(t) || !std::isfinite(rate) || rate <= 1e-4f)
    {
        return out;
    }
    const F64 refractory = static_cast<F64>(
        std::isfinite(refractory_sec) ? llmax(refractory_sec, 0.f) : 0.f);
    // Enforce the CONFIGURED refractory between spontaneous onsets by
    // construction (see the section comment): the per-cell hash range is
    // period - min_gap, and the period is stretched when a large refractory
    // would leave no usable range. Defaults (refractory 0.5) reproduce the
    // original lattice bit-exactly.
    const F64 min_gap = llmax(refractory, 0.5);
    F64 period = llclamp(4.5 / static_cast<F64>(rate), 1.8, 18.0);
    period = llmax(period, min_gap + 0.3);
    const F64 total_s = static_cast<F64>(
        ALGazeBlink::blinkTotalMs(close_ms, hold_ms, open_ms)) * 0.001;
    const S64 k = static_cast<S64>(std::floor(t / period));

    for (S64 cell = k - 1; cell <= k + 1; ++cell)
    {
        const F64 onset = detail::spontOnsetForCell(seed, cell, period,
                                                    min_gap);
        if (onset >= evoked_onset &&
            onset < evoked_onset + total_s + refractory)
        {
            continue; // suppressed by the evoked blink's refractory shadow
        }
        if (onset <= t)
        {
            out.mPrevOnset = llmax(out.mPrevOnset, onset);
        }
        else
        {
            out.mNextOnset = llmin(out.mNextOnset, onset);
        }
        const F32 closure = ALGazeBlink::blinkClosure(
            t - onset, close_ms, hold_ms, open_ms,
            detail::spontDepthForCell(seed, cell, partial_chance));
        out.mClosure = llmax(out.mClosure, closure);
    }
    return out;
}

// Gaze-evoked blink probability: a smooth curve rising with gaze-shift
// amplitude (spec 2.2: NOT every saccade). Zero below ~8 deg, certain at
// ~32 deg and beyond.
inline F32 gazeEvokedProbability(F32 amplitude_deg)
{
    const F32 amp = std::isfinite(amplitude_deg)
        ? llmax(amplitude_deg, 0.f) : 0.f;
    return static_cast<F32>(
        ALGazeBlink::smootherstep01(static_cast<F64>((amp - 8.f) / 24.f)));
}

// ---------------------------------------------------------------------------
// Channel program plumbing
// ---------------------------------------------------------------------------
namespace detail
{
// Hold a channel at a constant value from `now` on.
inline void holdChannel(ChannelState& ch, F64 now, F32 value)
{
    ch.mPendingValid = false;
    ch.mProgram = ALTrajectory::solveScalar(
        now, 0.0, value, 0.f, 0.f, value, 0.f, 0.f, false,
        0, BEHAVIOR_VERSION);
}

// Promote a pending program whose start time has arrived. The pending
// program was built from the active program's exact sampled (p, v, a) at
// its start time (with the handoff derivative clamps below), so promotion
// is C2 for ordinary gaze ranges; see retargetChannel for the bound.
inline void promoteChannel(ChannelState& ch, F64 now)
{
    if (ch.mPendingValid && now >= ch.mPending.mStartTime)
    {
        ch.mProgram = ch.mPending;
        ch.mPendingValid = false;
    }
}

// Retarget one channel toward `target_angle` (settling endpoint), starting
// at `start_time` (>= now for the delayed head/neck/torso stages). The C2
// handoff triple is sampled from the ACTIVE program at start_time -- any
// not-yet-started pending program is dropped first, so the program that is
// really driving the channel until start_time is also the one handed off
// from (an abandoned target's head move simply never begins, which is the
// natural behavior for a target retracted inside the head latency).
//
// C2-path guarantee: solveScalar is C2 for ordinary gaze ranges but has a
// documented residual C0 escape when extreme incoming derivatives face
// near-zero remaining travel (altrajectory.h, "Residual C0 escape"). The
// clamps below make that escape UNREACHABLE: with |v0| <= 100 rad/s and
// |a0| <= 1e5 rad/s^2, a MIN_DURATION (1e-6 s) natural-settle brake's
// excursion past its start is bounded by max|h10| * |T v0| + |rest| +
// max|h20| * |T^2 a0| < 0.199 * 1e-4 + 0.5 * 1e-4 + 0.018 * 1e-7
// ~= 7.0e-5 < SETTLE_OVERSHOOT_TOL (1e-4), so the overshoot-tolerant
// settle tier always proves never-cross and the fallback stays C1/C2.
// Ordinary gaze handoffs peak near 54 rad/s (pi radians at the 110 ms eye
// main-sequence cap), far below the clamps, so they are inert in normal
// operation; only a pathological configuration (e.g. a zero-ms authored
// duration) trades a bounded handoff-derivative snap for a guaranteed C2
// solve.
inline void retargetChannel(ChannelState& ch, F64 now, F64 start_time,
                            F64 duration, F32 target_angle, U64 event_id)
{
    ch.mPendingValid = false;
    const ALTrajectory::ScalarSample s =
        ALTrajectory::sample(ch.mProgram, start_time);
    const F32 v0 = llclamp(s.mV, -HANDOFF_MAX_VEL_RAD_SEC,
                           HANDOFF_MAX_VEL_RAD_SEC);
    const F32 a0 = llclamp(s.mA, -HANDOFF_MAX_ACC_RAD_SEC2,
                           HANDOFF_MAX_ACC_RAD_SEC2);
    const F32 p1 = ALTrajectory::unwrapNear(s.mP, target_angle);
    ALTrajectory::ScalarProgram program = ALTrajectory::solveScalar(
        start_time, duration, s.mP, v0, a0, p1, 0.f, 0.f,
        true /*monotonic*/, event_id, BEHAVIOR_VERSION);
    if (start_time <= now)
    {
        ch.mProgram = program;
    }
    else
    {
        ch.mPending = program;
        ch.mPendingValid = true;
    }
}
} // namespace detail

// ---------------------------------------------------------------------------
// Retarget commit (spec 6A steps 1 + 3 + 7-tempo + evoked-blink roll)
// ---------------------------------------------------------------------------
namespace detail
{
inline void commitRetarget(GazeMotorState& state, const GazeMotorInput& input,
                           F64 now)
{
    const GazeMotorSettings& s = input.mSettings;
    const F32 target_yaw = ALTrajectory::wrapToPi(input.mTargetYaw);
    const F32 target_pitch = ALTrajectory::sanitizeValue(input.mTargetPitch);
    const F32 target_roll_in = ALTrajectory::sanitizeValue(input.mTargetRoll);
    const F32 tempo = tempoScale(input.mAffect, s);

    // Amplitude of the shift, measured from the eye group's CURRENT sampled
    // aim (the C2 handoff start) to the new target.
    const ALTrajectory::ScalarSample eye_yaw_s =
        ALTrajectory::sample(state.mChannels[CH_EYE_YAW].mProgram, now);
    const ALTrajectory::ScalarSample eye_pitch_s =
        ALTrajectory::sample(state.mChannels[CH_EYE_PITCH].mProgram, now);
    const F32 dyaw = ALTrajectory::shortestArcDelta(
        ALTrajectory::wrapToPi(eye_yaw_s.mP), target_yaw);
    const F32 dpitch = target_pitch - eye_pitch_s.mP;
    const F32 amp_deg =
        sqrtf(dyaw * dyaw + dpitch * dpitch) * RAD_TO_DEG;

    // Timing policy: eye main sequence (no tempo -- physiological), head
    // latency + duration (tempo-scaled), neck/torso staggered further.
    const F64 eye_dur = static_cast<F64>(ALGazePolicy::eyeSaccadeDurationMs(
        amp_deg, s.mEyeDurationBaseMs, s.mEyeDurationPerDegMs)) * 0.001;
    const F32 ecc_deg = sqrtf(
        state.mLastEyeInHeadYaw * state.mLastEyeInHeadYaw +
        state.mLastEyeInHeadPitch * state.mLastEyeInHeadPitch) * RAD_TO_DEG;
    const F64 head_lat = static_cast<F64>(ALGazePolicy::headLatencyMs(
        amp_deg, ecc_deg, s.mPredictability01, s.mHeadMoverTrait01,
        s.mHeadLatencyBaseMs)) * 0.001 * static_cast<F64>(tempo);
    const F32 head_dur_base = std::isfinite(s.mHeadDurationBaseMs)
        ? llmax(s.mHeadDurationBaseMs, 1.f) : 150.f;
    const F32 head_dur_max = std::isfinite(s.mHeadDurationMaxMs)
        ? llmax(s.mHeadDurationMaxMs, head_dur_base) : 600.f;
    const F32 head_dur_per = std::isfinite(s.mHeadDurationPerDegMs)
        ? llmax(s.mHeadDurationPerDegMs, 0.f) : 5.f;
    const F64 head_dur = static_cast<F64>(llclamp(
        head_dur_base + head_dur_per * amp_deg,
        head_dur_base, head_dur_max)) * 0.001 * static_cast<F64>(tempo);

    state.mRetargetCounter += 1;
    const U64 event_id = state.mRetargetCounter;

    // Eyes lead: retarget now, main-sequence duration.
    retargetChannel(state.mChannels[CH_EYE_YAW], now, now, eye_dur,
                    target_yaw, event_id);
    retargetChannel(state.mChannels[CH_EYE_PITCH], now, now, eye_dur,
                    target_pitch, event_id);

    // Head follows after the latency; dominance biases the head tilt
    // (spec 6A step 7 -- contact rhythm is a later add).
    const F32 dominance = std::isfinite(input.mAffect.mDominance)
        ? llclamp(input.mAffect.mDominance, -1.f, 1.f) : 0.f;
    const F32 roll_bias = std::isfinite(s.mDominanceRollDeg)
        ? s.mDominanceRollDeg * DEG_TO_RAD : 0.f;
    const F32 target_roll = target_roll_in + dominance * roll_bias;
    const F64 head_start = now + head_lat;
    retargetChannel(state.mChannels[CH_HEAD_YAW], now, head_start, head_dur,
                    target_yaw, event_id);
    retargetChannel(state.mChannels[CH_HEAD_PITCH], now, head_start, head_dur,
                    target_pitch, event_id);
    retargetChannel(state.mChannels[CH_HEAD_ROLL], now, head_start, head_dur,
                    target_roll, event_id);

    // Neck and torso staggered further, slower. The latency scales are
    // multiplicative on the head latency, so when the policy returns ~0
    // (high predictability / head-mover trait) the additive MIN_STAGGER_SEC
    // floor is what keeps the wave ordered: neck strictly after head,
    // torso strictly after neck, in every configuration.
    const F32 neck_lat_scale = std::isfinite(s.mNeckLatencyScale)
        ? llmax(s.mNeckLatencyScale, 1.f) : 1.75f;
    const F32 neck_dur_scale = std::isfinite(s.mNeckDurationScale)
        ? llmax(s.mNeckDurationScale, 1.f) : 1.25f;
    const F32 torso_lat_scale = std::isfinite(s.mTorsoLatencyScale)
        ? llmax(s.mTorsoLatencyScale, neck_lat_scale) : 2.5f;
    const F32 torso_dur_scale = std::isfinite(s.mTorsoDurationScale)
        ? llmax(s.mTorsoDurationScale, neck_dur_scale) : 1.6f;
    const F64 neck_start = llmax(
        now + head_lat * static_cast<F64>(neck_lat_scale),
        head_start + MIN_STAGGER_SEC);
    const F64 neck_dur = head_dur * static_cast<F64>(neck_dur_scale);
    retargetChannel(state.mChannels[CH_NECK_YAW], now, neck_start, neck_dur,
                    target_yaw, event_id);
    retargetChannel(state.mChannels[CH_NECK_PITCH], now, neck_start, neck_dur,
                    target_pitch, event_id);
    const F64 torso_start = llmax(
        now + head_lat * static_cast<F64>(torso_lat_scale),
        neck_start + MIN_STAGGER_SEC);
    const F64 torso_dur = head_dur * static_cast<F64>(torso_dur_scale);
    retargetChannel(state.mChannels[CH_TORSO_YAW], now, torso_start, torso_dur,
                    target_yaw, event_id);
    retargetChannel(state.mChannels[CH_TORSO_PITCH], now, torso_start,
                    torso_dur, target_pitch, event_id);

    state.mCommittedYaw   = target_yaw;
    state.mCommittedPitch = target_pitch;
    state.mCommittedRoll  = target_roll_in;

    // Gaze-evoked blink roll. INTERNALLY deterministic (see the blink
    // scheduler contract above): the onset is the commit time `now`, the
    // draw hashes (seed, retarget counter), and the gates below consume
    // only closed-form lattice samples and prior committed state -- nothing
    // depends on call cadence beyond the commit itself.
    if (std::isfinite(s.mGazeEvokedBlink) && s.mGazeEvokedBlink > 1e-4f)
    {
        const F32 probability = llclamp(
            gazeEvokedProbability(amp_deg) * s.mGazeEvokedBlink, 0.f, 1.f);
        const F32 draw = ALGazeNoise::unitHash(
            input.mSeed, NOISE_CH_EVOKE, state.mRetargetCounter, 0, 0);
        const F64 total_s = static_cast<F64>(ALGazeBlink::blinkTotalMs(
            s.mBlinkCloseMs, s.mBlinkHoldMs, s.mBlinkOpenMs)) * 0.001;
        const F64 refractory = static_cast<F64>(
            std::isfinite(s.mBlinkRefractorySec)
                ? llmax(s.mBlinkRefractorySec, 0.f) : 0.f);
        const F32 rate_eff = llmax(s.mBlinkRate, 0.f) *
            blinkStyleScale(input.mAffect);
        const BlinkLatticeSample spont = sampleSpontaneousBlink(
            input.mSeed, now, rate_eff,
            s.mBlinkCloseMs, s.mBlinkHoldMs, s.mBlinkOpenMs,
            s.mBlinkPartialChance, state.mEvokedBlinkStart,
            s.mBlinkRefractorySec);
        // Symmetric arbitration: an evoked onset must clear the previous
        // blink's FULL envelope plus the configured refractory in both
        // directions -- the previous evoked blink, and the most recent
        // spontaneous onset (mirroring the shadow that suppresses
        // spontaneous onsets for total + refractory after an evoked onset).
        // The next-scheduled-spontaneous gate keeps the refractory check:
        // an imminent spontaneous onset closer than total + refractory
        // would fall inside the new evoked blink's shadow and be
        // suppressed, so either way no two onsets land closer than the
        // blink duration + refractory.
        const bool clear_of_evoked =
            now - state.mEvokedBlinkStart >= total_s + refractory;
        const bool clear_of_spontaneous =
            now - spont.mPrevOnset >= total_s + refractory &&
            spont.mNextOnset - now >= refractory;
        if (draw < probability && clear_of_evoked && clear_of_spontaneous)
        {
            state.mEvokedBlinkStart = now;
            state.mEvokedBlinkDepth = 1.f;
        }
    }
}

inline void initializeState(GazeMotorState& state, const GazeMotorInput& input,
                            F64 now)
{
    state = GazeMotorState();
    const F32 target_yaw = ALTrajectory::wrapToPi(input.mTargetYaw);
    const F32 target_pitch = ALTrajectory::sanitizeValue(input.mTargetPitch);
    const F32 target_roll_in = ALTrajectory::sanitizeValue(input.mTargetRoll);
    const F32 dominance = std::isfinite(input.mAffect.mDominance)
        ? llclamp(input.mAffect.mDominance, -1.f, 1.f) : 0.f;
    const F32 roll_bias = std::isfinite(input.mSettings.mDominanceRollDeg)
        ? input.mSettings.mDominanceRollDeg * DEG_TO_RAD : 0.f;
    const F32 target_roll = target_roll_in + dominance * roll_bias;

    holdChannel(state.mChannels[CH_EYE_YAW], now, target_yaw);
    holdChannel(state.mChannels[CH_EYE_PITCH], now, target_pitch);
    holdChannel(state.mChannels[CH_HEAD_YAW], now, target_yaw);
    holdChannel(state.mChannels[CH_HEAD_PITCH], now, target_pitch);
    holdChannel(state.mChannels[CH_HEAD_ROLL], now, target_roll);
    holdChannel(state.mChannels[CH_NECK_YAW], now, target_yaw);
    holdChannel(state.mChannels[CH_NECK_PITCH], now, target_pitch);
    holdChannel(state.mChannels[CH_TORSO_YAW], now, target_yaw);
    holdChannel(state.mChannels[CH_TORSO_PITCH], now, target_pitch);

    state.mCommittedYaw   = target_yaw;
    state.mCommittedPitch = target_pitch;
    state.mCommittedRoll  = target_roll_in;
    state.mInitialized = true;
}

// Step 1: retarget detection with enter hysteresis + dwell. The candidate
// must stay beyond the 3-degree enter threshold continuously for 120 ms
// before a program commit; dipping back inside cancels it, so sub-threshold
// jitter never thrashes the running segments.
//
// `acquire` short-circuits the dwell: on an activation edge (gaze (re)becoming
// active, or the operator selecting a new method while the master gate stays
// on -- see GazeMotorInput::mAcquire / mTargetGeneration) the current target is
// committed on THIS frame, so acquisition is instant like the legacy path
// instead of waiting out the 120 ms dwell (which, fed by the integration's
// heavily smoothed body-aim, could hover near the enter threshold and keep
// cancelling the candidate so a switch never committed). The commit still goes
// through commitRetarget -> retargetChannel, so the eyes-lead-head cascade and
// the C2 handoff are preserved; only the dwell gate is bypassed.
inline void detectRetarget(GazeMotorState& state, const GazeMotorInput& input,
                           F64 now, bool acquire)
{
    const F32 enter_rad = RETARGET_ENTER_DEG * DEG_TO_RAD;
    const F32 dyaw = ALTrajectory::shortestArcDelta(
        state.mCommittedYaw, ALTrajectory::wrapToPi(input.mTargetYaw));
    const F32 dpitch =
        ALTrajectory::sanitizeValue(input.mTargetPitch) - state.mCommittedPitch;
    const F32 droll =
        ALTrajectory::sanitizeValue(input.mTargetRoll) - state.mCommittedRoll;
    const F32 aim_err = sqrtf(dyaw * dyaw + dpitch * dpitch);

    // Immediate acquisition on an activation/method-switch edge. A negligible
    // move (re-selecting the same direction) is ignored so the retarget
    // counter and the evoked-blink draw are not bumped for a non-motion edge.
    if (acquire)
    {
        const F32 acquire_eps = RETARGET_ACQUIRE_EPS_DEG * DEG_TO_RAD;
        if (aim_err > acquire_eps || fabsf(droll) > acquire_eps)
        {
            state.mCandidateValid = false;
            commitRetarget(state, input, now);
            return;
        }
    }

    const bool beyond = aim_err > enter_rad || fabsf(droll) > enter_rad;
    if (!beyond)
    {
        state.mCandidateValid = false;
        return;
    }
    if (!state.mCandidateValid)
    {
        state.mCandidateValid = true;
        state.mCandidateSince = now;
        return;
    }
    if (now - state.mCandidateSince >= RETARGET_DWELL_SEC)
    {
        state.mCandidateValid = false;
        commitRetarget(state, input, now);
    }
}
} // namespace detail

// ---------------------------------------------------------------------------
// step(): the per-frame 7-step motor program (spec 6A)
// ---------------------------------------------------------------------------
inline void step(GazeMotorState& state, const GazeMotorInput& input,
                 GazeMotorPose& out_pose)
{
    out_pose = GazeMotorPose();
    out_pose.mCueWeight = input.mCueWeight;

    // Master gate OFF: documented passthrough/no-op. No new math runs, the
    // pose stays neutral with mActive = false (the integration keeps the
    // legacy path), and the state is dropped so re-enabling re-initializes
    // cleanly on whatever target is current then.
    if (!input.mSettings.mMasterGate)
    {
        state.mInitialized = false;
        return;
    }

    const GazeMotorSettings& s = input.mSettings;
    const F64 now = ALTrajectory::sanitizeTime(input.mTimeSeconds);
    if (!state.mInitialized)
    {
        detail::initializeState(state, input, now);
    }

    // Activation/method-switch edge (spec 2.6 integration). A changed target
    // generation is honored as an implicit acquire even if mAcquire was not
    // set explicitly; the first gated frame after (re)initialization has no
    // prior generation and must NOT count as an edge (initializeState already
    // snapped to the current target). Update the stored generation every frame.
    const bool gen_edge = state.mHasTargetGen &&
        state.mLastTargetGen != input.mTargetGeneration;
    state.mLastTargetGen = input.mTargetGeneration;
    state.mHasTargetGen  = true;
    const bool acquire_now = input.mAcquire || gen_edge;

    // Promote pending (delayed head/neck/torso) programs whose start time
    // has arrived; promotion is C2 for ordinary gaze ranges (extreme
    // incoming derivatives are clamped at the handoff -- see
    // detail::retargetChannel for the documented bound).
    for (S32 i = 0; i < CHANNEL_COUNT; ++i)
    {
        detail::promoteChannel(state.mChannels[i], now);
    }

    // (1) Retarget detection with hysteresis; commits rebuild the channel
    //     programs with eyes-now / head-late / neck-torso-staggered timing.
    //     An activation/method-switch edge (acquire_now) commits immediately,
    //     bypassing the dwell so acquisition is instant like the legacy path.
    detail::detectRetarget(state, input, now, acquire_now);

    // (2) Sample every channel's closed-form program at presentation time.
    //     (p, v, a) for the next C2 handoff live inside the programs.
    F32 aim[CHANNEL_COUNT];
    for (S32 i = 0; i < CHANNEL_COUNT; ++i)
    {
        aim[i] = ALTrajectory::sample(state.mChannels[i].mProgram, now).mP;
    }
    const F32 eye_aim_yaw   = ALTrajectory::wrapToPi(aim[CH_EYE_YAW]);
    const F32 eye_aim_pitch = aim[CH_EYE_PITCH];
    out_pose.mChainAimYaw   = eye_aim_yaw;
    out_pose.mChainAimPitch = eye_aim_pitch;

    // (3) Soft-recruit each group's sampled aim across the anatomical chain
    //     (band from the setting; 0 = legacy hard knee, bit-exact).
    //     CONTINUITY AT THE +/-pi SEAM: the sampled aims are fed to the
    //     signed allocator UNWRAPPED. Each channel's program is continuous
    //     across the seam (targets are unwrapped next to the current sample
    //     at commit, e.g. +179 deg -> +181 deg), and the allocator must see
    //     that continuous value: wrapping first would flip the input's sign
    //     at the seam (+pi -> -pi) and with it every saturated contribution
    //     (e.g. head +35 deg snapping to -35 deg) -- a hard position AND
    //     velocity discontinuity. Per-joint outputs are bounded by their
    //     capacities (all far below pi), so they need no terminal wrap;
    //     only the modular chain-aim report above is wrapped, at the very
    //     end, where a wrap is genuinely modular. (Aims stay within pi of
    //     the last committed target by construction -- each commit takes
    //     the shortest arc -- so the unwrapped value cannot wind through
    //     multiple turns within one program; a target that orbits the actor
    //     repeatedly is a Director body-turn scenario handled upstream.)
    F32 caps_yaw[CHAIN_JOINTS];
    F32 caps_pitch[CHAIN_JOINTS];
    effectiveCapacities(s, caps_yaw, caps_pitch);
    const F32 band = (std::isfinite(s.mSoftRecruitBandDeg)
        ? llmax(s.mSoftRecruitBandDeg, 0.f) : 0.f) * DEG_TO_RAD;
    // Eye-only mode (mHeadEyeBlend <= 0.001) must use the exact allocation
    // REGARDLESS of band -- see recruitSlot's comment: there is no chain to
    // soft-recruit into when only the eye has capacity, and routing it
    // through the soft path manufactures a spurious, wrong-signed downstream
    // contribution at the knee (residual == capacity == 0).
    const bool eye_only = isEyeOnlyBlend(s);
    out_pose.mHeadYaw   = recruitSlot(aim[CH_HEAD_YAW], caps_yaw, band, 1,
                                      eye_only);
    out_pose.mHeadPitch = recruitSlot(aim[CH_HEAD_PITCH], caps_pitch, band, 1,
                                      eye_only);
    out_pose.mNeckYaw   = recruitSlot(aim[CH_NECK_YAW], caps_yaw, band, 2,
                                      eye_only);
    out_pose.mNeckPitch = recruitSlot(aim[CH_NECK_PITCH], caps_pitch, band, 2,
                                      eye_only);
    out_pose.mTorsoYaw   = recruitSlot(aim[CH_TORSO_YAW], caps_yaw, band, 3,
                                       eye_only);
    out_pose.mTorsoPitch = recruitSlot(aim[CH_TORSO_PITCH], caps_pitch, band,
                                       3, eye_only);
    out_pose.mHipsYaw    = recruitSlot(aim[CH_TORSO_YAW], caps_yaw, band, 4,
                                       eye_only);
    out_pose.mHipsPitch  = recruitSlot(aim[CH_TORSO_PITCH], caps_pitch, band,
                                       4, eye_only);
    out_pose.mHeadRoll   = aim[CH_HEAD_ROLL];

    // (3b) Cinematic Stillness + Restraint: scale the RECRUITED head/neck/
    //      torso contribution toward zero BEFORE the micro-life drift is added
    //      below (step 5), so the body barely moves while the eyes and all
    //      micro-life stay fully alive. Stillness (1 -> full freeze) and
    //      Restraint's head-turn magnitude reduction combine multiplicatively;
    //      both default 0 -> body_scale == 1 -> byte-identical. The eye
    //      channels (mEyeYaw/mEyePitch, solved next) and the micro-life added
    //      in step 5 are deliberately untouched.
    const F32 stillness = std::isfinite(s.mStillness)
        ? llclamp(s.mStillness, 0.f, 1.f) : 0.f;
    const F32 restraint = std::isfinite(s.mRestraint)
        ? llclamp(s.mRestraint, 0.f, 1.f) : 0.f;
    const F32 body_scale = (1.f - stillness) *
        (1.f - RESTRAINT_HEADTURN_K * restraint);
    if (body_scale != 1.f)
    {
        out_pose.mHeadYaw   *= body_scale;
        out_pose.mHeadPitch *= body_scale;
        out_pose.mHeadRoll  *= body_scale;
        out_pose.mNeckYaw   *= body_scale;
        out_pose.mNeckPitch *= body_scale;
        out_pose.mTorsoYaw  *= body_scale;
        out_pose.mTorsoPitch *= body_scale;
        out_pose.mHipsYaw   *= body_scale;
        out_pose.mHipsPitch *= body_scale;
    }

    // (4) VOR eye-in-head by construction: the eye group's aim (which sweeps
    //     to the target over the saccade, then holds it) becomes a world
    //     direction, and the eye-in-head that fixates it is solved fresh
    //     against the sampled head orientation every frame -- as the head
    //     rotates in, counter-rotation falls out; never keyed to a constant.
    const LLVector3 world_dir =
        ALGazePolicy::eyeDirFromYawPitch(eye_aim_yaw, eye_aim_pitch) *
        input.mRefWorldRot;
    // Fix 1: surface the desired world gaze so the integration can re-solve the
    // eye-in-head AFTER it paints the recruited head/neck (see GazeMotorPose).
    out_pose.mDesiredWorldGaze = world_dir;
    F32 eye_yaw = 0.f;
    F32 eye_pitch = 0.f;
    ALGazePolicy::eyeInHeadFromWorldGaze(
        world_dir, input.mHeadWorldRot,
        s.mComfortYawDeg, s.mComfortPitchDeg, eye_yaw, eye_pitch);
    state.mLastEyeInHeadYaw = eye_yaw;   // eccentricity input for the next
    state.mLastEyeInHeadPitch = eye_pitch; // retarget's head-latency policy

    // (5) Micro-life, POST-smoothing additive (same ordering as
    //     llactormover.cpp:4744) and arousal-scaled. Eye drift runs on a
    //     faster lattice than head drift; microsaccades are eye-only.
    const F32 arousal = std::isfinite(input.mAffect.mArousal)
        ? llclamp(input.mAffect.mArousal, 0.f, 1.f) : 0.5f;
    const F32 micro = llclamp(
        0.5f + llmax(s.mMicroArousalGain, 0.f) * arousal, 0.f, 1.5f);
    // Clamp the microsaccade rate to the sub-tremor bound at this boundary:
    // ALGazeNoise::microsaccadeOffset deliberately does not clamp, and the
    // spec forbids synthesizing tremor-band motion via configuration.
    const F32 micro_rate = std::isfinite(s.mMicrosaccadeRateHz)
        ? llclamp(s.mMicrosaccadeRateHz, 0.f, MICROSACCADE_MAX_RATE_HZ) : 0.f;
    const F32 eye_cell = llmax(s.mDriftCellSec * 0.4f, 1.0e-3f);
    // Accumulate the eye micro-life offset separately so it can be surfaced
    // (fix 1: re-added by the integration's post-paint eye-in-head solve).
    F32 eye_yaw_micro = 0.f;
    F32 eye_pitch_micro = 0.f;
    eye_yaw_micro += ALGazeNoise::driftOffset(
        input.mSeed, NOISE_CH_EYE_YAW, now, eye_cell,
        s.mEyeDriftAmpRad * micro);
    eye_yaw_micro += ALGazeNoise::microsaccadeOffset(
        input.mSeed, NOISE_CH_EYE_YAW, now, micro_rate,
        s.mMicrosaccadeAmpRad * micro);
    eye_pitch_micro += ALGazeNoise::driftOffset(
        input.mSeed, NOISE_CH_EYE_PITCH, now, eye_cell,
        s.mEyeDriftAmpRad * 0.7f * micro);
    eye_pitch_micro += ALGazeNoise::microsaccadeOffset(
        input.mSeed, NOISE_CH_EYE_PITCH, now, micro_rate,
        s.mMicrosaccadeAmpRad * 0.7f * micro);
    eye_yaw += eye_yaw_micro;
    eye_pitch += eye_pitch_micro;
    out_pose.mEyeYawMicro = eye_yaw_micro;
    out_pose.mEyePitchMicro = eye_pitch_micro;
    out_pose.mHeadYaw += ALGazeNoise::driftOffset(
        input.mSeed, NOISE_CH_HEAD_YAW, now, s.mDriftCellSec,
        s.mHeadDriftAmpRad * micro);
    out_pose.mHeadPitch += ALGazeNoise::driftOffset(
        input.mSeed, NOISE_CH_HEAD_PITCH, now, s.mDriftCellSec,
        s.mHeadDriftAmpRad * 0.75f * micro);
    out_pose.mEyeYaw = eye_yaw;
    out_pose.mEyePitch = eye_pitch;

    // (6) Blink scheduler: spontaneous closed-form lattice (+ evoked live
    //     onset from step 1's commit), shaped by ALGazeBlink, folded into a
    //     Tier-0 lid closure over the affect aperture posture. A tiny hashed
    //     per-eye lag/depth asymmetry -- bilaterally coordinated, never
    //     independent winks.
    const F32 rate_eff = llmax(s.mBlinkRate, 0.f) *
        blinkStyleScale(input.mAffect);
    const F32 asym_lag_s = (ALGazeNoise::unitHash(
        input.mSeed, NOISE_CH_ASYM, 0, 0, 0) * 2.f - 1.f) * 0.006f;
    const F32 asym_depth = 1.f - 0.03f * ALGazeNoise::unitHash(
        input.mSeed, NOISE_CH_ASYM, 0, 0, 1);
    F32 closure[2];
    for (S32 eye = 0; eye < 2; ++eye)
    {
        const F64 t_eye = eye == 0 ? now : now - static_cast<F64>(asym_lag_s);
        const BlinkLatticeSample spont = sampleSpontaneousBlink(
            input.mSeed, t_eye, rate_eff,
            s.mBlinkCloseMs, s.mBlinkHoldMs, s.mBlinkOpenMs,
            s.mBlinkPartialChance, state.mEvokedBlinkStart,
            s.mBlinkRefractorySec);
        const F32 evoked = ALGazeBlink::blinkClosure(
            t_eye - state.mEvokedBlinkStart,
            s.mBlinkCloseMs, s.mBlinkHoldMs, s.mBlinkOpenMs,
            state.mEvokedBlinkDepth);
        F32 c = llmax(spont.mClosure, evoked);
        if (eye == 1)
        {
            c *= asym_depth;
        }
        closure[eye] = c;
    }
    const F32 posture = aperturePosture(input.mAffect);
    out_pose.mLidClosureLeft = llclamp(
        posture + (1.f - posture) * closure[0], 0.f, 1.f);
    out_pose.mLidClosureRight = llclamp(
        posture + (1.f - posture) * closure[1], 0.f, 1.f);
    out_pose.mApertureWiden = apertureWiden01(input.mAffect);

    // (7) Affect tempo and dominance tilt are applied where the programs are
    //     BUILT (commitRetarget): arousal scales head/neck/torso durations
    //     and the head latency within clamps; dominance biases the head-roll
    //     target. Nothing per-frame remains for this step.

    out_pose.mActive = true;
}

} // namespace ALGazeMotor

#endif // AL_ALGAZEMOTOR_H
