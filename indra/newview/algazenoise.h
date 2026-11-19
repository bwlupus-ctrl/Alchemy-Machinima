/**
 * @file algazenoise.h
 * @brief Deterministic band-limited fixation noise: drift + microsaccade
 *        (fixation-relocation) layers for the gaze micro-life system.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALGAZENOISE_H
#define AL_ALGAZENOISE_H

#include "llmath.h"

#include <cmath>

// ALGazeNoise is the Cinematic Gaze build spec's fixation/micro-life noise
// core (doc/CINEMATIC_GAZE_LIFE_BUILD_SPEC.md section 2.2; companion deep
// research doc section 6.1-6.4). It is deliberately pure and standalone:
// every function is a closed-form of (seed, channel, absolute F64 time,
// tunables) with no RNG engine, no internal/static state, and no wall-clock
// or frame-history dependency, so it is scrub-safe the same way
// ALGazeMath::evalMicroLife is -- sampling at time t after a seek gives the
// bit-identical result as playing forward into t.
//
// Two layers are implemented, matching companion 6.1-6.4:
//   1. driftOffset()        -- deterministic band-limited VALUE NOISE
//                               replacing the old fixed-sine Lissajous head
//                               drift. Hashes adjacent integer time cells,
//                               interpolates with a C2 smootherstep, and
//                               sums a small second octave at an
//                               incommensurate cell size so the curve does
//                               not reveal an obvious period.
//   2. microsaccadeOffset() -- discrete hashed fixation-relocation /
//                               microsaccade events: a held hashed target
//                               that eases to the next hashed target near
//                               the start of each hashed-width interval.
//
// Tremor (companion's ~40-100Hz fixational band) is explicitly NOT modeled
// here and must not be added as a style knob (build spec section 5: "No
// true-tremor channel ships").
//
// The integer hash below is an independent re-implementation of the
// splitMix64 / unitHash pattern in algazemath.h (kept local so this header
// has zero dependency beyond llmath.h, per the build spec). It uses only
// integer multiply/xor/shift, so it stays safe under /fp:fast.
namespace ALGazeNoise
{

// ---------------------------------------------------------------------------
// Deterministic closed-form hashing (local re-implementation; see file
// comment above for why this duplicates rather than includes algazemath.h).
// ---------------------------------------------------------------------------
constexpr U64 NOISE_DEFAULT_SEED = 0x9e3779b97f4a7c15ULL;

inline U64 splitMix64(U64 value)
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

// (seed, channel, cell, octave, draw) -> [0, 1). `channel` decorrelates
// independent gaze channels (yaw vs pitch, eye vs head, ...) sharing one
// seed; `octave` decorrelates noise bands layered on the same channel;
// `draw` decorrelates multiple independent values pulled from one cell.
inline F32 unitHash(U64 seed, U32 channel, U64 cell, U32 octave, S32 draw)
{
    U64 key = seed ? seed : NOISE_DEFAULT_SEED;
    key = splitMix64(key ^ (0xd1b54a32d192ed03ULL * (static_cast<U64>(channel) + 1ULL)));
    key = splitMix64(key ^ (0x9e3779b97f4a7c15ULL * (cell + 1ULL)));
    key = splitMix64(key ^ (0x94d049bb133111ebULL * (static_cast<U64>(octave) + 2ULL)));
    key = splitMix64(key ^ (0xbf58476d1ce4e5b9ULL * static_cast<U64>(draw + 3)));
    return static_cast<F32>((key >> 40) * (1.0 / 16777216.0));
}

// Signed draw in [-1, 1). Used for every noise lattice sample below: a
// uniform hash on [0,1) re-centered so the *expected* value of one draw is
// exactly zero, which is what makes the interpolated noise curve zero-mean
// over long windows (see driftOffset()).
inline F32 signedHash(U64 seed, U32 channel, U64 cell, U32 octave, S32 draw)
{
    return unitHash(seed, channel, cell, octave, draw) * 2.f - 1.f;
}

// C2 "smootherstep" (Ken Perlin's improved ease): both the first AND second
// derivative are exactly zero at u=0 and u=1. Tiling this cell-to-cell
// across a value-noise lattice, or using it as a discrete event's ease
// curve, therefore keeps the assembled curve C2-continuous at every
// boundary -- position, velocity (slope), and curvature all match on both
// sides with zero jump.
inline F32 smootherStep(F32 u)
{
    u = llclamp(u, 0.f, 1.f);
    return u * u * u * (u * (6.f * u - 15.f) + 10.f);
}

// ---------------------------------------------------------------------------
// Drift layer (companion 6.2, 6.4): deterministic band-limited value noise.
// ---------------------------------------------------------------------------

// One octave of value noise at `t_sec`, using integer cells of width
// `cell_seconds`. Hashes the two adjacent cell endpoints bracketing t_sec
// and interpolates with the C2 smootherstep. Returns a value in [-1, 1);
// callers combine octaves and apply amplitude. `octave` selects an
// independent hash band so multiple octaves at the same channel do not
// share draws.
inline F32 valueNoiseOctave(U64 seed, U32 channel, F64 t_sec,
                            F32 cell_seconds, U32 octave)
{
    const F32 cell = llmax(cell_seconds, 1.0e-3f);
    const F64 t = std::isfinite(t_sec) ? t_sec : 0.0;
    const F64 coord = t / static_cast<F64>(cell);
    const F64 floor_coord = std::floor(coord);
    const S64 cell_index = static_cast<S64>(floor_coord);

    F32 frac = static_cast<F32>(coord - floor_coord);
    frac = llclamp(frac, 0.f, 1.f);
    const F32 s = smootherStep(frac);

    // S64 -> U64 cast is well-defined two's-complement wraparound, so
    // negative cell indices (t_sec < 0, or a negative local/relative time
    // base) still hash deterministically and distinctly -- no special-cased
    // domain restriction is needed for purity or determinism.
    const U64 cell_a = static_cast<U64>(cell_index);
    const U64 cell_b = static_cast<U64>(cell_index + 1);
    const F32 a = signedHash(seed, channel, cell_a, octave, 0);
    const F32 b = signedHash(seed, channel, cell_b, octave, 0);
    return a + (b - a) * s;
}

// Deterministic band-limited value noise for slow head/eye drift
// (companion 6.4), replacing the old fixed-sine Lissajous. Two octaves:
//   - primary lattice at `cell_seconds`;
//   - a second, smaller-amplitude lattice at an incommensurate cell size
//     (scaled by 1 - 1/phi, so its boundaries never phase-lock with the
//     primary lattice's), decorrelated via a distinct octave index in the
//     hash.
// The combination is a weighted average of two zero-mean, amplitude-bounded
// signals, so the result is itself zero-mean over long windows and bounded
// by +/- amplitude. `amplitude` is in the same units the caller samples in
// (this codebase's convention is radians, matching ALGazeMath).
inline F32 driftOffset(U64 seed, U32 channel, F64 t_sec,
                       F32 cell_seconds, F32 amplitude)
{
    if (!std::isfinite(t_sec) || !std::isfinite(cell_seconds) ||
        !std::isfinite(amplitude) || amplitude <= 0.f)
    {
        return 0.f;
    }

    const F32 base_cell = llmax(cell_seconds, 1.0e-3f);
    constexpr F32 OCTAVE2_CELL_RATIO = 0.381966011f; // 1 - 1/phi; incommensurate
    constexpr F32 OCTAVE2_WEIGHT = 0.35f;             // small second octave
    const F32 octave2_cell = llmax(base_cell * OCTAVE2_CELL_RATIO, 1.0e-3f);

    const F32 n1 = valueNoiseOctave(seed, channel, t_sec, base_cell, 0);
    const F32 n2 = valueNoiseOctave(seed, channel, t_sec, octave2_cell, 1);

    // Weights sum to 1 so the combined signal stays within the same [-1,1)
    // envelope as a single octave -- amplitude remains a hard bound.
    const F32 norm = 1.f / (1.f + OCTAVE2_WEIGHT);
    const F32 combined = (n1 + OCTAVE2_WEIGHT * n2) * norm;
    return combined * amplitude;
}

// ---------------------------------------------------------------------------
// Microsaccade / fixation-relocation layer (companion 6.1, 6.2).
// ---------------------------------------------------------------------------

// Discrete hashed fixation-relocation / microsaccade events: NOT tremor.
// The offset holds a hashed per-interval target and eases to the next
// hashed target over a short hashed "flick" near the start of each
// interval, then holds until the next one -- the same held-then-flick shape
// as ALGazeMath::evalMicroLife's saccade layer, generalized to an arbitrary
// per-subject rate.
//
// Interval width is a fixed (seed, channel)-hashed +/-35% jitter around
// 1/rate_hz -- fixed per subject/channel rather than re-hashed per event, so
// the cell containing any t_sec is still recovered in closed form (a
// variable-width lattice would need forward integration to reconstruct,
// which would break scrub/seek). A hashed phase offset additionally
// decorrelates channels sharing one rate (e.g. yaw vs pitch do not fire in
// lockstep). `amplitude` is expected in radians (companion 6.2 suggests a
// visible microsaccade/fixation-relocation scale of about 0.1-0.5 degrees,
// i.e. roughly 0.0017-0.0087 rad); `rate_hz` should stay in the fixation
// literature's low single-digit Hz range (companion 6.2: ~1-2 Hz) -- this
// function does not clamp rate_hz, but driving it into the tens of Hz would
// synthesize alias-prone tremor-band motion the spec explicitly excludes.
inline F32 microsaccadeOffset(U64 seed, U32 channel, F64 t_sec,
                              F32 rate_hz, F32 amplitude)
{
    if (!std::isfinite(t_sec) || !std::isfinite(rate_hz) || rate_hz <= 0.f ||
        !std::isfinite(amplitude) || amplitude <= 0.f)
    {
        return 0.f;
    }

    const F64 base_interval = 1.0 / static_cast<F64>(llmax(rate_hz, 1.0e-3f));
    const F64 interval_jitter =
        (static_cast<F64>(unitHash(seed, channel, 0, 90, 0)) * 2.0 - 1.0) * 0.35;
    const F64 interval = llmax(base_interval * (1.0 + interval_jitter), 1.0e-3);

    const F64 phase = static_cast<F64>(unitHash(seed, channel, 0, 91, 0)) * interval;
    const F64 t = t_sec + phase;
    const F64 coord = t / interval;
    const F64 floor_coord = std::floor(coord);
    const S64 event_cell = static_cast<S64>(floor_coord);
    const F64 local_t = (coord - floor_coord) * interval; // seconds into this cell

    const U64 cell_curr = static_cast<U64>(event_cell);
    const U64 cell_prev = static_cast<U64>(event_cell - 1);
    const F32 target_prev = signedHash(seed, channel, cell_prev, 0, 1);
    const F32 target_curr = signedHash(seed, channel, cell_curr, 0, 1);

    // Hashed flick duration, 10-20% of this event's interval, so the ease is
    // always a short fraction of the hold and never approaches the
    // interval's own timescale.
    const F32 flick_frac =
        0.10f + 0.10f * unitHash(seed, channel, cell_curr, 1, 2);
    const F64 flick_dur = interval * static_cast<F64>(flick_frac);

    F32 ease = 1.f;
    if (flick_dur > 0.0 && local_t < flick_dur)
    {
        ease = smootherStep(static_cast<F32>(local_t / flick_dur));
    }

    const F32 value = target_prev + (target_curr - target_prev) * ease;
    return value * amplitude;
}

} // namespace ALGazeNoise

#endif // AL_ALGAZENOISE_H
