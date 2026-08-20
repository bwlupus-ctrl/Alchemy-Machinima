/**
 * @file algazepolicy.h
 * @brief Pure oculomotor policy math: eye duration main sequence, head
 * latency policy, and the VOR eye-in-head solve.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALGAZEPOLICY_H
#define AL_ALGAZEPOLICY_H

#include "llmath.h"
#include "llquaternion.h"
#include "v3math.h"

#include <cmath>

// ALGazePolicy is the Layer-2 oculomotor policy slice of the Cinematic Gaze
// build spec (doc/CINEMATIC_GAZE_LIFE_BUILD_SPEC.md section 2.2), matching
// the companion research doc sections 3.5 (main-sequence eye duration), 3.6
// (head latency policy), and 10.5 (VOR eye-in-head program). Deliberately
// pure: closed-form functions of their arguments, no state, no RNG, no time
// source, no dependency on the actor mover or anatomy chain. Every constant
// below is a documented, tunable population prior, not a claimed human law.
namespace ALGazePolicy
{

// ---------------------------------------------------------------------------
// 1. Eye duration main sequence (companion 3.5)
// ---------------------------------------------------------------------------
// Human saccades link amplitude and duration (the "main sequence"; Bahill,
// Clark & Stark 1975; Chen et al. 1999 report intercepts ~20-30ms and slopes
// ~2.2-3.4 ms/deg with real individual variation). D_eye_ms = clamp(base +
// per_deg*amplitude, base, 110). 25ms is the population-prior *intercept*,
// not a fixed duration for every eye movement: at 2 deg this gives ~31ms, at
// 20 deg ~81ms, and it never exceeds the 110ms anatomical ceiling used for
// large saccades. base/per_deg are exposed as parameters (matching
// DirectorGazeEyeDurationBaseMs/PerDegMs in the build spec) so a later
// data-calibrated, skewed profile can replace this line without touching
// callers.
inline F32 eyeSaccadeDurationMs(F32 amplitude_deg,
                                F32 base_ms = 25.f,
                                F32 per_deg_ms = 2.8f)
{
    constexpr F32 MAX_DURATION_MS = 110.f;
    const F32 base = std::isfinite(base_ms)
        ? llclamp(base_ms, 0.f, MAX_DURATION_MS) : 25.f;
    const F32 per_deg = std::isfinite(per_deg_ms) ? per_deg_ms : 2.8f;
    const F32 amplitude =
        std::isfinite(amplitude_deg) ? llmax(amplitude_deg, 0.f) : 0.f;

    const F32 duration = base + per_deg * amplitude;
    return llclamp(duration, base, MAX_DURATION_MS);
}

// ---------------------------------------------------------------------------
// 2. Head latency policy (companion 3.6) -- NOT a constant
// ---------------------------------------------------------------------------
// In an unpredictable visual gaze shift the eyes typically begin ~25-60ms
// before the head (Freedman 2008); that gap shrinks -- and can invert -- with
// larger/more predictable shifts, a more eccentric starting eye position, and
// a subject's habitual head-mover trait (Andrist et al. 2012). Each input
// below independently *reduces* the base latency by a documented, saturating
// term; the sum is clamped to [0, base] so the result is always finite and
// non-negative even when every term maxes out at once. A 40ms base is a
// reasonable starting point (companion 3.6 / DirectorGazeHeadLatencyMs),
// not a universal constant -- callers/directors can override base_ms.
inline F32 headLatencyMs(F32 amplitude_deg,
                         F32 initial_eye_eccentricity_deg,
                         F32 predictability01,
                         F32 head_mover_trait01,
                         F32 base_ms = 40.f)
{
    const F32 base =
        std::isfinite(base_ms) ? llmax(base_ms, 0.f) : 40.f;
    const F32 amplitude = std::isfinite(amplitude_deg)
        ? llmax(amplitude_deg, 0.f) : 0.f;
    const F32 eccentricity = std::isfinite(initial_eye_eccentricity_deg)
        ? llmax(initial_eye_eccentricity_deg, 0.f) : 0.f;
    const F32 predictability = std::isfinite(predictability01)
        ? llclamp(predictability01, 0.f, 1.f) : 0.f;
    const F32 head_mover = std::isfinite(head_mover_trait01)
        ? llclamp(head_mover_trait01, 0.f, 1.f) : 0.f;

    // Larger shifts recruit the head sooner: a saturating fraction of a
    // comfortable amplitude range, up to 15ms earlier onset.
    constexpr F32 AMPLITUDE_SATURATION_DEG = 40.f;
    constexpr F32 AMPLITUDE_TERM_MS = 15.f;
    const F32 amplitude_term = AMPLITUDE_TERM_MS *
        llclamp(amplitude / AMPLITUDE_SATURATION_DEG, 0.f, 1.f);

    // A target that starts more eccentric in the orbit is already nearer the
    // edge of the comfortable eye range, so the head is recruited earlier
    // ("initial eye position", Andrist et al. 2012), up to 10ms earlier.
    constexpr F32 ECCENTRICITY_SATURATION_DEG = 20.f;
    constexpr F32 ECCENTRICITY_TERM_MS = 10.f;
    const F32 eccentricity_term = ECCENTRICITY_TERM_MS *
        llclamp(eccentricity / ECCENTRICITY_SATURATION_DEG, 0.f, 1.f);

    // A predictable target lets the head anticipate instead of react
    // (companion 3.6: "for large or predictable shifts the head can begin at
    // or before the eyes"), up to 15ms earlier.
    constexpr F32 PREDICTABILITY_TERM_MS = 15.f;
    const F32 predictability_term = PREDICTABILITY_TERM_MS * predictability;

    // A habitual "head mover" recruits the head sooner as a stable
    // per-subject trait rather than a per-shift decision, up to 20ms earlier.
    constexpr F32 HEAD_MOVER_TERM_MS = 20.f;
    const F32 head_mover_term = HEAD_MOVER_TERM_MS * head_mover;

    const F32 latency = base - amplitude_term - eccentricity_term -
        predictability_term - head_mover_term;
    return llclamp(latency, 0.f, base);
}

// ---------------------------------------------------------------------------
// 3. VOR eye-in-head solve (companion 10.5)
// ---------------------------------------------------------------------------
// Actor-mover convention (matches llactormover.cpp / algazemath.h): joint
// local +X is forward, +Y is left, +Z is up; `local_dir * worldRot` maps a
// local-frame direction into world space (see e.g. llactormover.cpp
// `LLVector3(1,0,0) * joint->getWorldRotation()`), and the inverse map is
// `world_dir * ~worldRot`. Yaw is a left turn about +Z, atan2(Y, X); pitch is
// positive looking down, atan2(-Z, horizontal) -- the same sign convention
// algazemath.h's lidFollowClosure documents for eye pitch.

// Head-local forward direction for a given eye-in-head yaw/pitch.
inline LLVector3 eyeDirFromYawPitch(F32 eye_yaw, F32 eye_pitch)
{
    const F32 cos_pitch = cosf(eye_pitch);
    return LLVector3(cos_pitch * cosf(eye_yaw),
                     cos_pitch * sinf(eye_yaw),
                     -sinf(eye_pitch));
}

// Inverse of eyeDirFromYawPitch: yaw/pitch (radians) of a head-local
// direction. Degenerate (non-finite/zero) input returns (0, 0).
inline void yawPitchFromLocalDir(LLVector3 local_dir, F32& out_yaw, F32& out_pitch)
{
    if (!local_dir.isFinite() || local_dir.normVec() <= 1e-4f)
    {
        out_yaw = 0.f;
        out_pitch = 0.f;
        return;
    }
    out_yaw = atan2f(local_dir.mV[VY], local_dir.mV[VX]);
    const F32 horiz = sqrtf(
        local_dir.mV[VX] * local_dir.mV[VX] +
        local_dir.mV[VY] * local_dir.mV[VY]);
    out_pitch = atan2f(-local_dir.mV[VZ], llmax(horiz, 1e-6f));
}

// Exact (unclamped) eye-in-head yaw/pitch that points at world_gaze_dir given
// the head's current world orientation -- solved fresh from geometry, not
// keyed to any notion of "head progress". Feeding a fixed world_gaze_dir and
// a moving head_world_rot back through this function each sample is what
// makes VOR counter-rotation fall out by construction: as the head turns in,
// the returned eye-in-head angle turns the other way by exactly the amount
// needed to keep the reconstructed world direction on target.
inline void eyeInHeadRawFromWorldGaze(const LLVector3& world_gaze_dir,
                                      const LLQuaternion& head_world_rot,
                                      F32& out_eye_yaw, F32& out_eye_pitch)
{
    LLVector3 dir = world_gaze_dir;
    if (!dir.isFinite() || dir.normVec() <= 1e-4f)
    {
        out_eye_yaw = 0.f;
        out_eye_pitch = 0.f;
        return;
    }
    const LLVector3 local_dir = dir * ~head_world_rot;
    yawPitchFromLocalDir(local_dir, out_eye_yaw, out_eye_pitch);
}

// Smooth (C1), monotone soft clamp toward +/-limit: identity near zero,
// asymptotic at the limit, so a gaze angle approaching the edge of the
// comfort cone eases instead of hitting a hard knee. limit <= 0 collapses to
// exactly 0. Non-finite input returns 0.
inline F32 softClampAngle(F32 value, F32 limit)
{
    if (!std::isfinite(value) || !(limit > 1.0e-5f))
    {
        return 0.f;
    }
    return limit * tanhf(value / limit);
}

// Soft-limits an already-solved eye-in-head yaw/pitch into a per-axis comfort
// cone. Kept separate from eyeInHeadRawFromWorldGaze so tests (and callers)
// can verify the pre-limit world-gaze reconstruction independent of the
// comfort clamp.
inline void softLimitEyeInHead(F32 eye_yaw, F32 eye_pitch,
                               F32 comfort_yaw_deg, F32 comfort_pitch_deg,
                               F32& out_eye_yaw, F32& out_eye_pitch)
{
    out_eye_yaw = softClampAngle(eye_yaw, comfort_yaw_deg * DEG_TO_RAD);
    out_eye_pitch = softClampAngle(eye_pitch, comfort_pitch_deg * DEG_TO_RAD);
}

// Full VOR eye-in-head solve: solve the exact eye-in-head yaw/pitch that
// keeps world_gaze_dir fixated given the sampled head_world_rot, then
// soft-limit into the comfort cone. DirectorGazeRecenter (how far the head
// ultimately aligns) belongs upstream, in whatever supplies head_world_rot
// -- it never substitutes for this world-gaze equation.
inline void eyeInHeadFromWorldGaze(const LLVector3& world_gaze_dir,
                                   const LLQuaternion& head_world_rot,
                                   F32 comfort_yaw_deg, F32 comfort_pitch_deg,
                                   F32& out_eye_yaw, F32& out_eye_pitch)
{
    F32 raw_yaw = 0.f;
    F32 raw_pitch = 0.f;
    eyeInHeadRawFromWorldGaze(world_gaze_dir, head_world_rot, raw_yaw, raw_pitch);
    softLimitEyeInHead(raw_yaw, raw_pitch, comfort_yaw_deg, comfort_pitch_deg,
                       out_eye_yaw, out_eye_pitch);
}

// Reconstructs the world-space gaze direction implied by an eye-in-head
// yaw/pitch and a head world orientation (head_world_rot . eye_in_head).
// Exposed for tests and for callers that want to sanity-check a solve;
// calling this with the *raw* (pre-soft-limit) yaw/pitch from
// eyeInHeadRawFromWorldGaze must reproduce the original world_gaze_dir.
inline LLVector3 worldDirFromEyeInHead(F32 eye_yaw, F32 eye_pitch,
                                       const LLQuaternion& head_world_rot)
{
    return eyeDirFromYawPitch(eye_yaw, eye_pitch) * head_world_rot;
}

} // namespace ALGazePolicy

#endif // AL_ALGAZEPOLICY_H
