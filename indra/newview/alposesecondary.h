/**
 * @file alposesecondary.h
 * @brief Milestone 6 Pose Polish: pure procedural secondary motion (ambient life).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Subtle, bounded, ADDITIVE ambient overlays layered on the post-blend pose
 * (see docs/pose_polish_integration_plan.md, "Milestone 6 — Procedural
 * secondary motion"): a slow breathing rise/fall of the chest plus a very slow
 * idle weight-shift sway of the spine. Header-only and free of any LLVOAvatar
 * dependency: evalSecondary() is a pure function of time and amplitude params,
 * so it is unit-testable in isolation (tests/alposepolish_test.cpp) and the
 * avatar-facing hook (ALPosePolish::runSecondary in llvoavatar.cpp) simply
 * composes the returned small deltas over the current joint rotations.
 *
 * NOTE: the gaze-coupled "Gravitate" postural lean is deliberately NOT here —
 * it belongs to the gaze layer, which owns look-target coupling. This module
 * is purely time-driven.
 *
 * Contract highlights:
 *  - Pure & deterministic: same time_sec => bit-identical output. No state,
 *    no randomness, no avatar.
 *  - Amplitude 0 (or non-finite) => that channel's outputs are EXACTLY +0.f
 *    (the fields are never written, so the zero-initialized pose passes
 *    through untouched — no "0 * sin" signed-zero surprises).
 *  - Bounded: every output is clamped to MAX_DELTA_RAD (a few degrees), and
 *    the amplitude multipliers themselves clamp to MAX_AMP.
 *  - C1 continuous in time: sums of sines only, no branches on time.
 */

#ifndef AL_ALPOSESECONDARY_H
#define AL_ALPOSESECONDARY_H

#include "llmath.h"

#include <cmath>

namespace ALPoseSecondary
{
// ---------------------------------------------------------------------------
// Tunables / numeric guards
// ---------------------------------------------------------------------------

// Breath: a slow sine driving a tiny chest pitch (inhale/exhale rise-fall).
constexpr F64 BREATH_HZ       = 0.25;                    // ~one breath / 4 s
constexpr F32 BREATH_BASE_RAD = 0.6f * DEG_TO_RAD;       // base chest pitch amp

// Sway: a very slow quasi-periodic lateral weight shift. Two summed,
// non-harmonic sines (weights sum to 1 so |wave| <= 1) so the shift does not
// read as a metronome; a quarter-phase pitch component turns the shift into a
// gentle, barely-perceptible circular settle rather than a pure side rock.
constexpr F64 SWAY_HZ_A        = 0.08;                   // primary shift, ~12.5 s
constexpr F64 SWAY_HZ_B        = 0.052;                  // detuned secondary, ~19.2 s
constexpr F64 SWAY_PHASE_B     = 0.25;                   // cycles: decorrelate B from A at t=0
constexpr F32 SWAY_WEIGHT_A    = 0.65f;
constexpr F32 SWAY_WEIGHT_B    = 0.35f;                  // A + B = 1 => |wave| <= 1
constexpr F32 SWAY_ROLL_BASE_RAD  = 0.5f  * DEG_TO_RAD;  // base spine roll amp
constexpr F32 SWAY_PITCH_BASE_RAD = 0.15f * DEG_TO_RAD;  // coupled spine pitch amp

// Hard bounds. Amplitude params clamp to [0, MAX_AMP]; every output angle
// clamps to +/-MAX_DELTA_RAD, so no parameter abuse can produce a large pose
// change (plan rule: "each is a clamped additive delta").
constexpr F32 MAX_AMP       = 4.f;
constexpr F32 MAX_DELTA_RAD = 3.f * DEG_TO_RAD;

// All three frequencies complete an integer number of cycles in this many
// seconds (0.25*500=125, 0.08*500=40, 0.052*500=26), so a driving-time
// accumulator may wrap at this period with no phase discontinuity. Exposed for
// the llvoavatar.cpp wiring; evalSecondary itself needs no wrapping.
constexpr F32 COMMON_PERIOD_SEC = 500.f;

// ---------------------------------------------------------------------------
// API types
// ---------------------------------------------------------------------------

// Per-channel amplitude multipliers (1 = designed subtle default, 0 = off).
struct SecondaryParams
{
    F32 mBreathAmp = 1.f;
    F32 mSwayAmp   = 1.f;
};

// Small additive local-rotation deltas, radians. Chest pitch carries breath;
// spine roll + coupled spine pitch carry the idle weight shift. Zero-initialized
// so an untouched channel is exactly the identity contribution.
struct SecondaryPose
{
    F32 mChestPitch = 0.f;
    F32 mSpineRoll  = 0.f;
    F32 mSpinePitch = 0.f;
};

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace detail
{
// sin(2*pi*(t*hz + phase_cycles)), evaluated on the wrapped fractional cycle so
// precision does not degrade for large session times. fmod is exact, so this
// stays deterministic and (periodicity of sin) continuous across the wrap.
inline F32 cycleSin(F64 time_sec, F64 hz, F64 phase_cycles)
{
    const F64 cycles = std::fmod(time_sec * hz + phase_cycles, 1.0);
    return (F32)std::sin(6.283185307179586476925286766559 * cycles);
}

// Sanitize an amplitude: non-finite or negative => 0 (channel off), huge => MAX_AMP.
inline F32 cleanAmp(F32 amp)
{
    if (!std::isfinite(amp) || amp <= 0.f)
    {
        return 0.f;
    }
    return amp > MAX_AMP ? MAX_AMP : amp;
}

// Final per-output bound.
inline F32 clampDelta(F32 rad)
{
    return llclamp(rad, -MAX_DELTA_RAD, MAX_DELTA_RAD);
}
} // namespace detail

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

// Evaluate the ambient secondary-motion pose at @time_sec (seconds; any origin,
// the caller owns the clock/accumulator). Pure: no state, no side effects.
// Non-finite time or zero amplitudes yield the exact-zero pose.
inline SecondaryPose evalSecondary(F64 time_sec, const SecondaryParams& p)
{
    SecondaryPose out;   // all channels exactly 0.f until written

    if (!std::isfinite(time_sec))
    {
        return out;
    }

    const F32 breath_amp = detail::cleanAmp(p.mBreathAmp);
    const F32 sway_amp   = detail::cleanAmp(p.mSwayAmp);

    // Breath: chest rise/fall. Written only when the channel is live, so
    // amplitude 0 leaves the exact +0.f initializer in place.
    if (breath_amp > 0.f)
    {
        const F32 wave = detail::cycleSin(time_sec, BREATH_HZ, 0.0);
        out.mChestPitch = detail::clampDelta(breath_amp * (BREATH_BASE_RAD * wave));
    }

    // Sway: lateral weight shift (spine roll) with a quarter-phase coupled
    // pitch, from the same two-sine quasi-periodic wave family.
    if (sway_amp > 0.f)
    {
        const F32 roll_wave =
            SWAY_WEIGHT_A * detail::cycleSin(time_sec, SWAY_HZ_A, 0.0) +
            SWAY_WEIGHT_B * detail::cycleSin(time_sec, SWAY_HZ_B, SWAY_PHASE_B);
        const F32 pitch_wave =
            SWAY_WEIGHT_A * detail::cycleSin(time_sec, SWAY_HZ_A, 0.25) +
            SWAY_WEIGHT_B * detail::cycleSin(time_sec, SWAY_HZ_B, SWAY_PHASE_B + 0.25);
        out.mSpineRoll  = detail::clampDelta(sway_amp * (SWAY_ROLL_BASE_RAD  * roll_wave));
        out.mSpinePitch = detail::clampDelta(sway_amp * (SWAY_PITCH_BASE_RAD * pitch_wave));
    }

    return out;
}

} // namespace ALPoseSecondary

#endif // AL_ALPOSESECONDARY_H
