/**
 * @file algazeblink.h
 * @brief Pure, closed-form asymmetric blink APERTURE SHAPE (no scheduling).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALGAZEBLINK_H
#define AL_ALGAZEBLINK_H

#include "llmath.h"

#include <cmath>

// ALGazeBlink is the blink-shape half of the Layer-2 blink contract from the
// Cinematic Gaze build spec (doc/CINEMATIC_GAZE_LIFE_BUILD_SPEC.md section
// 2.2 "Blink scheduling" and 2.5 "Lid composition order") and the companion
// deep-research audit (section 7.1, "The current rate is plausible; the
// current shape is the weak point"). It supersedes the 140ms symmetric
// parabola at algazemath.h:447-463 (evalMicroLife's blink_shape) with a
// closed-form, deterministic, ASYMMETRIC close/hold/open envelope:
// high-speed measurement of spontaneous blinks found closure faster than
// reopening (roughly 100ms down / 220ms up), and partial blinks are common.
//
// This module is deliberately narrow: it is the *shape* only (aperture
// closure in [0,1] as a function of time elapsed since a blink started). It
// knows nothing about *when* a blink should start -- spontaneous cadence,
// gaze-evoked probability, authored cues, refractory periods, and bilateral
// coordination (companion 7.2/7.3/7.5) are a later scheduling/arbitration
// assembly layer that composes on top of this shape, same as the lid
// composition order in spec 2.5 layers this as step (3) of five. Zero
// dependencies on gaze, anatomy, affect, RNG, or any time source: pure
// functions of (elapsed time, durations, depth). Time follows the
// convention used elsewhere in the gaze math (evalMicroLife's t_seconds,
// ALTrajectory's F64 time): `t_since_start` is in SECONDS; the phase
// durations are in MILLISECONDS, matching the companion's envelope table
// and the `DirectorGazeBlinkCloseMs`/`...OpenMs` control-surface names
// (spec section 3).
namespace ALGazeBlink
{

// Companion 7.1 production envelope, tuned defaults (spec section 2.2):
// close 70-110ms (use 85), hold 0-35ms (use 0), open 120-220ms (use 165).
constexpr F32 DEFAULT_CLOSE_MS = 85.f;
constexpr F32 DEFAULT_HOLD_MS  = 0.f;
constexpr F32 DEFAULT_OPEN_MS  = 165.f;

// Quintic smootherstep 6u^5 - 15u^4 + 10u^3: monotone on [0,1] with zero
// velocity AND zero acceleration at both ends. This is the same C2 easing
// family as ALTrajectory's quintic-Hermite basis (its h01 term with
// v0=v1=a0=a1=0), so a close or open ramp built from it leaves fully-open,
// and settles back open, with no visible kink -- explicitly NOT the legacy
// symmetric parabola's non-zero terminal slope.
inline F64 smootherstep01(F64 u)
{
    const F64 x = llclamp(u, 0.0, 1.0);
    return x * x * x * (x * (x * 6.0 - 15.0) + 10.0);
}

// Sanitize a phase duration to a finite, non-negative millisecond value.
inline F32 sanitizeMs(F32 ms)
{
    return std::isfinite(ms) ? llmax(ms, 0.f) : 0.f;
}

// Total span, in ms, of one close+hold+open blink envelope.
inline F32 blinkTotalMs(F32 close_ms = DEFAULT_CLOSE_MS,
                        F32 hold_ms  = DEFAULT_HOLD_MS,
                        F32 open_ms  = DEFAULT_OPEN_MS)
{
    return sanitizeMs(close_ms) + sanitizeMs(hold_ms) + sanitizeMs(open_ms);
}

// ---------------------------------------------------------------------------
// Low-FPS readability (companion section 7.1, last paragraph)
// ---------------------------------------------------------------------------
// At 24fps a frame is ~41.7ms, so a 70-110ms close is under two frames, and
// a naively-scheduled blink can render with no sample anywhere near full
// closure if frame phase is unlucky. The implementation must therefore
// guarantee a readable closed/near-closed sample rather than rely on luck of
// frame phase: a later scheduler can phase-align a blink's start time so
// that `blinkPeakTimeMs` (or the peak window, when hold_ms > 0) lands on, or
// very near, a render-frame sample time.

// Time of deepest closure, in ms from blink onset. With a hold this is the
// hold's midpoint (the most robust single instant to phase-align a frame
// to); with no hold it is the instant the close ramp meets the open ramp.
inline F32 blinkPeakTimeMs(F32 close_ms = DEFAULT_CLOSE_MS,
                           F32 hold_ms  = DEFAULT_HOLD_MS)
{
    return sanitizeMs(close_ms) + 0.5f * sanitizeMs(hold_ms);
}

// [start, end] window, in ms from onset, during which closure is exactly at
// its peak -- the closed-hold plateau, or a single instant (start == end)
// when hold_ms is 0. A scheduler with hold_ms > 0 has this whole window to
// land a frame in, not just the midpoint `blinkPeakTimeMs`.
inline void blinkPeakWindowMs(F32 close_ms, F32 hold_ms,
                              F32& out_peak_start_ms, F32& out_peak_end_ms)
{
    const F32 close = sanitizeMs(close_ms);
    out_peak_start_ms = close;
    out_peak_end_ms   = close + sanitizeMs(hold_ms);
}

// ---------------------------------------------------------------------------
// Blink aperture shape
// ---------------------------------------------------------------------------

// Deterministic, closed-form blink aperture CLOSURE at `t_since_start`
// seconds after the blink began. 0 = lid fully open, 1 = fully closed; peak
// closure equals `depth` (clamped to [0,1]; partial blinks use depth < 1).
// Exactly 0 at and before onset (t_since_start <= 0), and exactly 0 at and
// after `blinkTotalMs(close_ms, hold_ms, open_ms)` has elapsed, so
// composing this into a lid channel outside the blink's own span is a
// no-op. Piecewise, in elapsed ms = t_since_start * 1000:
//   [0, close)              depth * smootherstep(t / close)       FAST close
//   [close, close+hold)     depth                                 closed hold
//   [close+hold, total)     depth * (1 - smootherstep(w / open))  SLOWER open
// close_ms/open_ms use independent ramps (asymmetric by construction, not a
// symmetric parabola); the two phases meet continuously at the shared peak.
// Pure and closed-form: identical arguments give bit-identical output
// regardless of call order, frame rate, or how many times it is sampled.
inline F32 blinkClosure(F64 t_since_start,
                        F32 close_ms = DEFAULT_CLOSE_MS,
                        F32 hold_ms  = DEFAULT_HOLD_MS,
                        F32 open_ms  = DEFAULT_OPEN_MS,
                        F32 depth    = 1.f)
{
    if (!std::isfinite(t_since_start) || t_since_start <= 0.0)
    {
        return 0.f;
    }
    const F32 peak = std::isfinite(depth) ? llclamp(depth, 0.f, 1.f) : 0.f;
    if (peak <= 0.f)
    {
        return 0.f;
    }

    const F64 close = static_cast<F64>(sanitizeMs(close_ms));
    const F64 hold  = static_cast<F64>(sanitizeMs(hold_ms));
    const F64 open  = static_cast<F64>(sanitizeMs(open_ms));
    const F64 t_ms  = t_since_start * 1000.0;

    // Phase 1: fast close, eased in from rest, reaching `peak` at t=close.
    // (close <= 0 falls straight through, matching an instantaneous close.)
    if (t_ms < close)
    {
        return peak * static_cast<F32>(smootherstep01(t_ms / close));
    }

    // Phase 2: optional closed hold at the peak.
    const F64 hold_end_ms = close + hold;
    if (t_ms < hold_end_ms)
    {
        return peak;
    }

    // Phase 3: slower eased open, back to rest at t = hold_end + open.
    const F64 total_ms = hold_end_ms + open;
    if (t_ms >= total_ms)
    {
        return 0.f;
    }
    return peak * static_cast<F32>(
        1.0 - smootherstep01((t_ms - hold_end_ms) / open));
}

} // namespace ALGazeBlink

#endif // AL_ALGAZEBLINK_H
