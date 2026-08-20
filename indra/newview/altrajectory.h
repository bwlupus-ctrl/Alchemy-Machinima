/**
 * @file altrajectory.h
 * @brief General, gaze-agnostic C2 trajectory continuity core (pure math).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALTRAJECTORY_H
#define AL_ALTRAJECTORY_H

#include "llmath.h"

#include <cmath>
#include <cstring>
#include <limits>

// ALTrajectory is the Layer-3 continuity core from the Cinematic Gaze build
// spec (doc/CINEMATIC_GAZE_LIFE_BUILD_SPEC.md section 2.1). It is deliberately
// pure: closed-form functions of (segment, absolute F64 time) with zero
// dependencies on gaze, anatomy, affect, RNG, viewer joints, or any time
// source. Segments are flat PODs, trivially serializable, and carry a
// behavior version so persisted shots can detect a math change instead of
// silently reinterpreting it.
//
// The channel type T is templated; the required, fully supported channel is
// scalar F32 (yaw, pitch, lid, weight...). The API avoids Euler-only
// assumptions so a vector channel (e.g. an exponential-map rotation vector)
// or a quaternion log/exp adapter can be added later without a rewrite:
// nothing below interprets T beyond "+" and "* scalar".
namespace ALTrajectory
{

// Motion profile of one segment. Quintic Hermite is the professional
// continuity backbone (a motion-design law, not a human-saccade claim).
// The enum leaves room for a future data-calibrated, skewed eye profile to
// be plugged in without touching the segment/event architecture.
enum class Profile : U32
{
    Quintic = 0
};

// Guard rails. Durations outside (MIN_DURATION, MAX_DURATION) are treated as
// degenerate (instantaneous step) or clamped so every sample stays finite.
constexpr F64 MIN_DURATION = 1.0e-6;   // seconds; below this a segment is a step
constexpr F64 MAX_DURATION = 1.0e7;    // seconds; ~115 days, clamps huge/inf T
constexpr F32 MAX_ABS_VALUE = 1.0e12f; // scalar sanitize bound; keeps T^2*a finite

// One sampled state: position, velocity, acceleration.
template<typename T>
struct Sample
{
    T mP;
    T mV;
    T mA;
};

// One closed-form trajectory segment (companion research doc section 10.1).
// Pure value type: sampling it never mutates it, and identical inputs give
// bit-identical outputs regardless of call order or frame rate.
//
// Requirements on T: beyond "+" and "* scalar" (see the header comment), T
// must be default-constructible and copy-assignable — the channel members
// below default-initialize with T(), and buildSegment() constructs a
// Segment and then assigns each channel.
template<typename T>
struct Segment
{
    F64     mStartTime       = 0.0;
    F64     mDuration        = 0.0;
    T       mP0              = T();     // start position
    T       mV0              = T();     // start velocity
    T       mA0              = T();     // start acceleration
    T       mP1              = T();     // end position
    T       mV1              = T();     // end velocity
    T       mA1              = T();     // end acceleration
    Profile mProfile         = Profile::Quintic;
    U64     mSourceEventId   = 0;
    U32     mBehaviorVersion = 0;
};

using ScalarSample  = Sample<F32>;
using ScalarSegment = Segment<F32>;

// ---------------------------------------------------------------------------
// Sanitizers
//
// The viewer builds Release with /fp:fast (cmake/00-Common.cmake), under
// which the optimizer may assume "no NaN/Inf" and constant-fold
// std::isfinite / std::isnan away. Every finiteness guard below therefore
// uses an integer bit-pattern test, which no floating-point optimization
// mode can elide.
// ---------------------------------------------------------------------------
inline bool isFiniteBits(F32 value)
{
    U32 bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return (bits & 0x7f800000u) != 0x7f800000u;
}

inline bool isFiniteBits(F64 value)
{
    U64 bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return (bits & 0x7ff0000000000000ull) != 0x7ff0000000000000ull;
}

inline F64 sanitizeTime(F64 time)
{
    return isFiniteBits(time) ? time : 0.0;
}

// Returns a strictly usable duration, or 0.0 to signal "degenerate step".
// NaN, -inf, and sub-threshold (zero/negative) durations degenerate to a
// step; huge or +infinite durations clamp to MAX_DURATION so T and T^2
// terms stay finite.
inline F64 sanitizeDuration(F64 duration)
{
    U64 bits;
    std::memcpy(&bits, &duration, sizeof(bits));
    if ((bits & 0x7ff0000000000000ull) == 0x7ff0000000000000ull)
    {
        // Non-finite: +inf clamps; NaN and -inf degenerate to a step.
        return bits == 0x7ff0000000000000ull ? MAX_DURATION : 0.0;
    }
    if (duration < MIN_DURATION)
    {
        return 0.0;
    }
    return duration > MAX_DURATION ? MAX_DURATION : duration;
}

inline F32 sanitizeValue(F32 value)
{
    if (!isFiniteBits(value))
    {
        return 0.f;
    }
    return llclamp(value, -MAX_ABS_VALUE, MAX_ABS_VALUE);
}

// Channel sanitize hook, applied at segment construction and on every
// sampling path so finite output is an API-wide property (not just a
// solveScalar courtesy). The scalar overload clamps non-finite/oversized
// values to [-MAX_ABS_VALUE, MAX_ABS_VALUE]; combined with the
// MAX_DURATION clamp this bounds every basis product (|a| * T^2 <= 1e26,
// well inside F32 range), so no sampled term can overflow. A future
// non-scalar channel type keeps the identity passthrough unless it
// provides its own overload.
template<typename T>
inline T sanitizeChannel(const T& value)
{
    return value;
}

inline F32 sanitizeChannel(F32 value)
{
    return sanitizeValue(value);
}

// ---------------------------------------------------------------------------
// Quintic Hermite basis (companion section 3.3)
//   h00 = 1 - 10u^3 + 15u^4 - 6u^5     h01 = 10u^3 - 15u^4 + 6u^5
//   h10 = u - 6u^3 + 8u^4 - 3u^5       h11 = -4u^3 + 7u^4 - 3u^5
//   h20 = 0.5(u^2 - 3u^3 + 3u^4 - u^5) h21 = 0.5(u^3 - 2u^4 + u^5)
//   p(u) = h00 p0 + h10 (T v0) + h20 (T^2 a0)
//        + h01 p1 + h11 (T v1) + h21 (T^2 a1)
// v(u) and a(u) are the exact analytic derivatives divided by T and T^2.
// ---------------------------------------------------------------------------
struct QuinticWeights
{
    F64 mP[6]; // position weights for p0, Tv0, T2a0, p1, Tv1, T2a1
    F64 mV[6]; // d/du of the above (divide by T for velocity)
    F64 mA[6]; // d2/du2 of the above (divide by T^2 for acceleration)
};

inline QuinticWeights quinticBasis(F64 u)
{
    const F64 u2 = u * u;
    const F64 u3 = u2 * u;
    const F64 u4 = u3 * u;
    const F64 u5 = u4 * u;

    QuinticWeights w;
    w.mP[0] = 1.0 - 10.0 * u3 + 15.0 * u4 - 6.0 * u5;
    w.mP[1] = u - 6.0 * u3 + 8.0 * u4 - 3.0 * u5;
    w.mP[2] = 0.5 * (u2 - 3.0 * u3 + 3.0 * u4 - u5);
    w.mP[3] = 10.0 * u3 - 15.0 * u4 + 6.0 * u5;
    w.mP[4] = -4.0 * u3 + 7.0 * u4 - 3.0 * u5;
    w.mP[5] = 0.5 * (u3 - 2.0 * u4 + u5);

    w.mV[0] = -30.0 * u2 + 60.0 * u3 - 30.0 * u4;
    w.mV[1] = 1.0 - 18.0 * u2 + 32.0 * u3 - 15.0 * u4;
    w.mV[2] = 0.5 * (2.0 * u - 9.0 * u2 + 12.0 * u3 - 5.0 * u4);
    w.mV[3] = 30.0 * u2 - 60.0 * u3 + 30.0 * u4;
    w.mV[4] = -12.0 * u2 + 28.0 * u3 - 15.0 * u4;
    w.mV[5] = 0.5 * (3.0 * u2 - 8.0 * u3 + 5.0 * u4);

    w.mA[0] = -60.0 * u + 180.0 * u2 - 120.0 * u3;
    w.mA[1] = -36.0 * u + 96.0 * u2 - 60.0 * u3;
    w.mA[2] = 1.0 - 9.0 * u + 18.0 * u2 - 10.0 * u3;
    w.mA[3] = 60.0 * u - 180.0 * u2 + 120.0 * u3;
    w.mA[4] = -24.0 * u + 84.0 * u2 - 60.0 * u3;
    w.mA[5] = 3.0 * u - 12.0 * u2 + 10.0 * u3;
    return w;
}

// ---------------------------------------------------------------------------
// Sampling
// ---------------------------------------------------------------------------
template<typename T>
inline Sample<T> startSample(const Segment<T>& segment)
{
    return Sample<T>{ sanitizeChannel(segment.mP0),
                      sanitizeChannel(segment.mV0),
                      sanitizeChannel(segment.mA0) };
}

template<typename T>
inline Sample<T> endSample(const Segment<T>& segment)
{
    return Sample<T>{ sanitizeChannel(segment.mP1),
                      sanitizeChannel(segment.mV1),
                      sanitizeChannel(segment.mA1) };
}

// Closed-form p/v/a at a segment-local time offset `dt` (seconds since the
// segment's start). u = clamp(dt / T, 0, 1): negative offsets return
// exactly (p0, v0, a0) and offsets at/past the duration return exactly
// (p1, v1, a1). Degenerate (zero/negative/non-finite) durations behave as
// an instantaneous step at dt = 0. A non-finite dt resolves to the nearer
// endpoint (+inf to the end; NaN and -inf to the start). Boundary values
// are sanitized here as well as at construction, so the result is always
// finite even for a raw Segment that bypassed buildSegment() with
// NaN/Inf/oversized members. The program sampler below reconstructs stage
// times through this entry point from RELATIVE offsets, so no absolute-time
// subtraction can lose a (small) stage duration against a (large) start
// time.
template<typename T>
inline Sample<T> sampleOffset(const Segment<T>& segment, F64 dt)
{
    const F64 t_eff = sanitizeDuration(segment.mDuration);
    if (!isFiniteBits(dt))
    {
        // NaN > 0.0 is false, so NaN and -inf resolve to the start state.
        return dt > 0.0 ? endSample(segment) : startSample(segment);
    }
    if (t_eff <= 0.0)
    {
        return dt < 0.0 ? startSample(segment) : endSample(segment);
    }

    const F64 u = llclamp(dt / t_eff, 0.0, 1.0);
    if (u <= 0.0)
    {
        return startSample(segment);
    }
    if (u >= 1.0)
    {
        return endSample(segment);
    }

    // Profile switch: Quintic is the only Phase-1 profile. A future eye
    // profile plugs in here without changing the segment or event model.
    const QuinticWeights w = quinticBasis(u);
    const F32 t_scale  = static_cast<F32>(t_eff);
    const F32 t2_scale = static_cast<F32>(t_eff * t_eff);
    const T p0   = sanitizeChannel(segment.mP0);
    const T p1   = sanitizeChannel(segment.mP1);
    const T tv0  = sanitizeChannel(segment.mV0) * t_scale;
    const T ta0  = sanitizeChannel(segment.mA0) * t2_scale;
    const T tv1  = sanitizeChannel(segment.mV1) * t_scale;
    const T ta1  = sanitizeChannel(segment.mA1) * t2_scale;

    const F32 inv_t  = static_cast<F32>(1.0 / t_eff);
    const F32 inv_t2 = static_cast<F32>(1.0 / (t_eff * t_eff));

    Sample<T> out;
    out.mP = p0  * static_cast<F32>(w.mP[0]) +
             tv0 * static_cast<F32>(w.mP[1]) +
             ta0 * static_cast<F32>(w.mP[2]) +
             p1  * static_cast<F32>(w.mP[3]) +
             tv1 * static_cast<F32>(w.mP[4]) +
             ta1 * static_cast<F32>(w.mP[5]);
    out.mV = (p0  * static_cast<F32>(w.mV[0]) +
              tv0 * static_cast<F32>(w.mV[1]) +
              ta0 * static_cast<F32>(w.mV[2]) +
              p1  * static_cast<F32>(w.mV[3]) +
              tv1 * static_cast<F32>(w.mV[4]) +
              ta1 * static_cast<F32>(w.mV[5])) * inv_t;
    out.mA = (p0  * static_cast<F32>(w.mA[0]) +
              tv0 * static_cast<F32>(w.mA[1]) +
              ta0 * static_cast<F32>(w.mA[2]) +
              p1  * static_cast<F32>(w.mA[3]) +
              tv1 * static_cast<F32>(w.mA[4]) +
              ta1 * static_cast<F32>(w.mA[5])) * inv_t2;
    return out;
}

// Closed-form p/v/a at absolute time. Non-finite times resolve to the start
// state. The start time is bit-check sanitized on this path too (a raw
// segment with a NaN/Inf mStartTime steps relative to the sanitized start,
// 0.0, instead of comparing a time against a non-finite value), keeping the
// "every public path is bit-check guarded" property true for raw segments.
template<typename T>
inline Sample<T> sample(const Segment<T>& segment, F64 time)
{
    if (!isFiniteBits(time))
    {
        return startSample(segment);
    }
    return sampleOffset(segment, time - sanitizeTime(segment.mStartTime));
}

// ---------------------------------------------------------------------------
// Construction & C2 interruption handoff
// ---------------------------------------------------------------------------
template<typename T>
inline Segment<T> buildSegment(F64 start_time, F64 duration,
                               const T& p0, const T& v0, const T& a0,
                               const T& p1, const T& v1, const T& a1,
                               U64 source_event_id = 0,
                               U32 behavior_version = 0,
                               Profile profile = Profile::Quintic)
{
    Segment<T> segment;
    segment.mStartTime       = sanitizeTime(start_time);
    segment.mDuration        = sanitizeDuration(duration);
    segment.mP0 = sanitizeChannel(p0);
    segment.mV0 = sanitizeChannel(v0);
    segment.mA0 = sanitizeChannel(a0);
    segment.mP1 = sanitizeChannel(p1);
    segment.mV1 = sanitizeChannel(v1);
    segment.mA1 = sanitizeChannel(a1);
    segment.mProfile         = profile;
    segment.mSourceEventId   = source_event_id;
    segment.mBehaviorVersion = behavior_version;
    return segment;
}

// C2 interruption handoff: sample the interrupted segment at `now` including
// acceleration, and start the replacement from that exact triple. Position,
// velocity, and acceleration are all continuous at the boundary (C2; jerk may
// step, which is acceptable for an interruptible system — companion 3.2/3.3).
// A settling look passes v1 = a1 = zero; a predicted moving target may pass
// its predicted terminal velocity.
//
// Guarding: `now`, `duration`, and every boundary value are sanitized on
// entry — non-finite `now` resolves through sample() to the old start
// state, and the sampled handoff triple plus the new endpoint pass through
// buildSegment()'s channel sanitize, so retarget()/retargetSettle() return
// finite segments for arbitrary (NaN/Inf/oversized) inputs.
template<typename T>
inline Segment<T> retarget(const Segment<T>& old_segment, F64 now,
                           F64 duration,
                           const T& p1, const T& v1, const T& a1,
                           U64 source_event_id = 0,
                           U32 behavior_version = 0)
{
    const Sample<T> s = sample(old_segment, now);
    return buildSegment(now, duration, s.mP, s.mV, s.mA, p1, v1, a1,
                        source_event_id, behavior_version,
                        old_segment.mProfile);
}

// Scalar convenience: retarget toward a settling endpoint (v1 = a1 = 0).
inline ScalarSegment retargetSettle(const ScalarSegment& old_segment, F64 now,
                                    F64 duration, F32 p1,
                                    U64 source_event_id = 0,
                                    U32 behavior_version = 0)
{
    return retarget(old_segment, now, duration, p1, 0.f, 0.f,
                    source_event_id, behavior_version);
}

// ---------------------------------------------------------------------------
// Duration feasibility / overshoot (scalar; companion section 3.4)
// ---------------------------------------------------------------------------
constexpr S32 MAX_LENGTHEN_RETRIES   = 3;
constexpr F64 LENGTHEN_FACTOR        = 1.5;

// Absolute position overshoot the brake fallback may accept in exchange for
// keeping velocity continuous when the remaining travel is too small for
// any C2 brake to settle inside the crossing tolerance. 1e-4 in channel
// units is ~0.006 degrees at gaze (radian) scale — imperceptible — while a
// velocity step is a visible hitch at any scale, so tiny bounded overshoot
// is always preferred over a velocity discontinuity in that regime.
constexpr F64 SETTLE_OVERSHOOT_TOL   = 1.0e-4;

// Deterministic polynomial root isolation for the monotonicity tests below.
// All arithmetic is plain F64 polynomial evaluation and comparisons on
// finite values — no NaN-dependent branches — so it behaves identically
// under /fp:fast.
namespace detail
{
constexpr S32 POLY_BISECT_ITERS = 64; // halving [0,1]: ~2^-64 absolute width

// Horner evaluation of c[0] + c[1] u + ... + c[degree] u^degree.
inline F64 evalPoly(const F64* c, S32 degree, F64 u)
{
    F64 v = c[degree];
    for (S32 i = degree - 1; i >= 0; --i)
    {
        v = v * u + c[i];
    }
    return v;
}

// Fixed-count bisection on [a, b] where f(a) = fa and f(b) have opposite
// signs. Deterministic: same inputs, same iteration path, same root.
// Resolution is a fixed ABSOLUTE interval width of ~2^-64 (or the local F64
// spacing where that is coarser, at which point the loop exits early) — for
// roots near zero this is far coarser than 1 ulp of the root itself, so it
// must not be described as "~1 ulp".
inline F64 bisectRoot(const F64* c, S32 degree, F64 a, F64 b, F64 fa)
{
    for (S32 i = 0; i < POLY_BISECT_ITERS; ++i)
    {
        const F64 m = 0.5 * (a + b);
        if (m <= a || m >= b)
        {
            break; // interval exhausted at F64 resolution
        }
        const F64 fm = evalPoly(c, degree, m);
        if (fm == 0.0)
        {
            return m;
        }
        if ((fm < 0.0) == (fa < 0.0))
        {
            a  = m;
            fa = fm;
        }
        else
        {
            b = m;
        }
    }
    return 0.5 * (a + b);
}

// Sign-changing real roots of a polynomial of degree <= 4 strictly inside
// (0, 1), ascending. Contract: every root at which the polynomial CHANGES
// SIGN is isolated — the polynomial is strictly monotone between
// consecutive roots of its derivative (found recursively, bottoming out in
// a closed-form linear root), so each such subinterval holds at most one
// crossing, located by bisection. Even-multiplicity (tangency) roots are
// reported only when a critical point's Horner evaluation is exactly 0.0
// in F64; a double root whose evaluation carries rounding — e.g.
// (15u - 11)^2 evaluates to ~1.4e-14, not 0, at u = 11/15 — is NOT
// isolated. That is sound for the two callers: checkMonotone and
// checkNeverCrosses consume these roots as the candidate locations of
// velocity sign changes and of position extrema, and an even-multiplicity
// velocity root neither changes the velocity's sign (no reversal) nor
// marks a position extremum (position keeps moving the same direction
// through it, so it stays between values already bounded by the endpoints
// and the sign-changing roots). `roots` must hold `degree` entries.
// Returns the root count.
inline S32 polyRootsInUnit(const F64* coeffs, S32 degree, F64* roots)
{
    F64 c[5] = { 0.0, 0.0, 0.0, 0.0, 0.0 };
    S32 deg = llclamp(degree, 0, 4);
    for (S32 i = 0; i <= deg; ++i)
    {
        c[i] = coeffs[i];
    }
    while (deg > 0 && c[deg] == 0.0)
    {
        --deg; // degenerate leading coefficient: lower-degree polynomial
    }
    if (deg == 0)
    {
        return 0;
    }
    if (deg == 1)
    {
        const F64 u = -c[0] / c[1];
        if (u > 0.0 && u < 1.0)
        {
            roots[0] = u;
            return 1;
        }
        return 0;
    }

    F64 dc[4];
    for (S32 i = 1; i <= deg; ++i)
    {
        dc[i - 1] = c[i] * static_cast<F64>(i);
    }
    F64 crit[3];
    const S32 ncrit = polyRootsInUnit(dc, deg - 1, crit);

    S32 count = 0;
    for (S32 i = 0; i < ncrit; ++i)
    {
        if (evalPoly(c, deg, crit[i]) == 0.0)
        {
            roots[count++] = crit[i]; // tangency root at a critical point
        }
    }
    F64 a  = 0.0;
    F64 fa = c[0]; // evalPoly at u = 0
    for (S32 k = 0; k <= ncrit; ++k)
    {
        const F64 b  = k < ncrit ? crit[k] : 1.0;
        const F64 fb = evalPoly(c, deg, b);
        if (fa != 0.0 && fb != 0.0 && (fa < 0.0) != (fb < 0.0))
        {
            roots[count++] = bisectRoot(c, deg, a, b, fa);
        }
        a  = b;
        fa = fb;
    }
    for (S32 i = 1; i < count; ++i) // insertion sort, count <= 4
    {
        const F64 r = roots[i];
        S32 j = i;
        while (j > 0 && roots[j - 1] > r)
        {
            roots[j] = roots[j - 1];
            --j;
        }
        roots[j] = r;
    }
    return count;
}

// Coefficients of the scaled velocity V(u) = T * v(u), a quartic in u,
// expanded from the analytic quintic derivative basis (header section
// "Quintic Hermite basis"). Inputs are the already-sanitized, T-scaled
// boundary values.
inline void velocityQuartic(F64 p0, F64 tv0, F64 ta0,
                            F64 p1, F64 tv1, F64 ta1, F64* c)
{
    c[0] = tv0;
    c[1] = ta0;
    c[2] = -30.0 * p0 - 18.0 * tv0 - 4.5 * ta0 +
            30.0 * p1 - 12.0 * tv1 + 1.5 * ta1;
    c[3] =  60.0 * p0 + 32.0 * tv0 + 6.0 * ta0 -
            60.0 * p1 + 28.0 * tv1 - 4.0 * ta1;
    c[4] = -30.0 * p0 - 15.0 * tv0 - 2.5 * ta0 +
            30.0 * p1 - 15.0 * tv1 + 2.5 * ta1;
}

// F64 position at parameter u from the same scaled boundary values.
inline F64 positionAtU(F64 p0, F64 tv0, F64 ta0,
                       F64 p1, F64 tv1, F64 ta1, F64 u)
{
    const QuinticWeights w = quinticBasis(u);
    return w.mP[0] * p0 + w.mP[1] * tv0 + w.mP[2] * ta0 +
           w.mP[3] * p1 + w.mP[4] * tv1 + w.mP[5] * ta1;
}
} // namespace detail

// True when the segment neither leaves the [p0, p1] band nor reverses
// against the travel direction (beyond small relative tolerances). The
// segment's velocity is the analytic derivative of a quintic — a quartic in
// u — so this test is exact rather than sampled: velocity extremes lie at
// the quartic's critical points (roots of its cubic derivative) and at the
// interval ends, and position extremes lie at the quartic's sign-changing
// roots. All of these are isolated deterministically (recursive monotone
// subdivision + fixed-count bisection to a ~2^-64 absolute resolution in
// u), so a reversal can never slip between grid samples. Even-multiplicity
// velocity roots may go unisolated (see polyRootsInUnit) but cannot flip
// either decision made here. Tolerances: positions may leave the band by
// span * tolerance_fraction; the velocity may oppose the travel direction
// by span/T * velocity_tolerance_fraction (pass 0 for a strict test).
inline bool checkMonotone(const ScalarSegment& segment,
                          F32 tolerance_fraction = 1e-3f,
                          F32 velocity_tolerance_fraction = 0.01f)
{
    const F64 t_eff = sanitizeDuration(segment.mDuration);
    if (t_eff <= 0.0)
    {
        return true; // a step cannot overshoot
    }

    const F64 p0  = static_cast<F64>(sanitizeValue(segment.mP0));
    const F64 p1  = static_cast<F64>(sanitizeValue(segment.mP1));
    const F64 tv0 = t_eff * static_cast<F64>(sanitizeValue(segment.mV0));
    const F64 ta0 = t_eff * t_eff * static_cast<F64>(sanitizeValue(segment.mA0));
    const F64 tv1 = t_eff * static_cast<F64>(sanitizeValue(segment.mV1));
    const F64 ta1 = t_eff * t_eff * static_cast<F64>(sanitizeValue(segment.mA1));

    const F64 d       = p1 - p0;
    const F64 span    = llmax(std::fabs(d), 1.0e-4);
    const F64 pos_tol =
        span * llmax(static_cast<F64>(tolerance_fraction), 0.0);
    const F64 lo      = llmin(p0, p1) - pos_tol;
    const F64 hi      = llmax(p0, p1) + pos_tol;
    const bool check_reversal = std::fabs(d) > 1.0e-6;
    const F64 dir     = d >= 0.0 ? 1.0 : -1.0;
    // V(u) = T * v, so the T-relative velocity tolerance scales to
    // velocity_tolerance_fraction * span in quartic units.
    const F64 vel_tol =
        span * llmax(static_cast<F64>(velocity_tolerance_fraction), 0.0);

    F64 vc[5];
    detail::velocityQuartic(p0, tv0, ta0, p1, tv1, ta1, vc);

    if (check_reversal)
    {
        // Velocity sign at the interval ends and at every interior critical
        // point of the quartic — the exact extremes of V on [0, 1].
        if (vc[0] * dir < -vel_tol ||
            detail::evalPoly(vc, 4, 1.0) * dir < -vel_tol)
        {
            return false;
        }
        F64 dvc[4] = { vc[1], 2.0 * vc[2], 3.0 * vc[3], 4.0 * vc[4] };
        F64 crit[3];
        const S32 ncrit = detail::polyRootsInUnit(dvc, 3, crit);
        for (S32 i = 0; i < ncrit; ++i)
        {
            if (detail::evalPoly(vc, 4, crit[i]) * dir < -vel_tol)
            {
                return false;
            }
        }
    }

    // Position band: extremes occur exactly where V(u) = 0 (endpoints are
    // p0/p1, inside the band by construction).
    F64 roots[4];
    const S32 nroots = detail::polyRootsInUnit(vc, 4, roots);
    for (S32 i = 0; i < nroots; ++i)
    {
        const F64 p = detail::positionAtU(p0, tv0, ta0, p1, tv1, ta1,
                                          roots[i]);
        if (p < lo || p > hi)
        {
            return false;
        }
    }
    return true;
}

// True when the segment's position never crosses the plane `p_limit` in
// travel direction `dir`, i.e. (p_limit - p(u)) * dir >= -pos_tol for all
// u in [0, 1]. Same analytic machinery as checkMonotone: position extremes
// lie at the velocity quartic's roots and the interval ends. Used to prove
// a brake stage cannot pass the target even while it legitimately moves
// against the travel direction.
inline bool checkNeverCrosses(const ScalarSegment& segment, F64 p_limit,
                              F64 dir, F64 pos_tol)
{
    const F64 t_eff = sanitizeDuration(segment.mDuration);
    const F64 p0    = static_cast<F64>(sanitizeValue(segment.mP0));
    const F64 p1    = static_cast<F64>(sanitizeValue(segment.mP1));
    if ((p_limit - p0) * dir < -pos_tol ||
        (p_limit - p1) * dir < -pos_tol)
    {
        return false;
    }
    if (t_eff <= 0.0)
    {
        return true; // a step only ever shows its endpoints
    }

    const F64 tv0 = t_eff * static_cast<F64>(sanitizeValue(segment.mV0));
    const F64 ta0 = t_eff * t_eff * static_cast<F64>(sanitizeValue(segment.mA0));
    const F64 tv1 = t_eff * static_cast<F64>(sanitizeValue(segment.mV1));
    const F64 ta1 = t_eff * t_eff * static_cast<F64>(sanitizeValue(segment.mA1));

    F64 vc[5];
    detail::velocityQuartic(p0, tv0, ta0, p1, tv1, ta1, vc);
    F64 roots[4];
    const S32 nroots = detail::polyRootsInUnit(vc, 4, roots);
    for (S32 i = 0; i < nroots; ++i)
    {
        const F64 p = detail::positionAtU(p0, tv0, ta0, p1, tv1, ta1,
                                          roots[i]);
        if ((p_limit - p) * dir < -pos_tol)
        {
            return false;
        }
    }
    return true;
}

// A short scalar motion program: one segment normally, two when the
// brake-then-go fallback fires. Still a pure value type — sampling picks the
// segment by absolute time, and both segments are C2 at their shared
// boundary by construction.
struct ScalarProgram
{
    ScalarSegment mSegments[2];
    S32  mSegmentCount    = 1;
    bool mBrakeFallback   = false; // two-stage brake-then-go was required
    S32  mLengthenRetries = 0;     // duration extensions that were applied
    // Program-relative time base. Stage boundaries are stored as OFFSETS
    // from mStartTime and are never recovered by subtracting absolute
    // segment start times, so the program stays exact even when the start
    // time's F64 ulp exceeds the stage durations (e.g. start = 2^53, where
    // start + t_brake == start in F64). The segments still carry
    // best-effort absolute start times for direct segment-level use.
    F64  mStartTime       = 0.0;   // sanitized program epoch (== stage-one start)
    F64  mStageOffset     = 0.0;   // stage-two start relative to mStartTime
    F64  mEndOffset       = 0.0;   // program end relative to mStartTime
};

// Absolute program end time, reconstructed from the epoch plus the RELATIVE
// stage boundaries (never from an absolute segment start plus a duration,
// which collapses at large epochs). The result is rounded UP until the
// SAMPLER'S final-stage local offset — (end - start) - mStageOffset for a
// two-stage program, end - start otherwise — reaches the final segment's
// full sanitized duration, so sampling AT the returned time clamps to
// u == 1 and yields the exact landed terminal state, even at start = 2^53.
// Checking only end - start against mEndOffset is NOT enough: mEndOffset =
// fl(t_brake + t_go) can round BELOW t_brake + t_go, so a time whose
// program-local offset covers mEndOffset can still leave stage two at
// u < 1 (e.g. solveScalar(0.1, 0.5, 0, 4.2, 0, 1, -1, 0, monotonic)). The
// deficit is a few ulps at most, so the bounded nextafter loop always
// clears it for builder-produced programs; sampling past the end clamps to
// the same terminal state, so an extra ulp is harmless. All inputs are
// bit-check sanitized so a raw program with NaN/Inf fields returns a
// finite time.
inline F64 programEndTime(const ScalarProgram& program)
{
    const F64 start = sanitizeTime(program.mStartTime);
    F64 rel = program.mEndOffset;
    if (!isFiniteBits(rel) || rel < 0.0)
    {
        rel = 0.0;
    }
    else if (rel > 2.0 * MAX_DURATION)
    {
        rel = 2.0 * MAX_DURATION; // two stages of at most MAX_DURATION each
    }
    // Landing invariant, stated in the exact frame the sampler routes with:
    // the stage-two offset (two-stage programs only) plus the final
    // segment's sanitized duration.
    const bool two_stage = program.mSegmentCount > 1;
    const F64 stage = two_stage ? sanitizeTime(program.mStageOffset) : 0.0;
    const F64 dur   = sanitizeDuration(
        program.mSegments[two_stage ? 1 : 0].mDuration);
    F64 need = stage + dur;
    if (!isFiniteBits(need) || need < 0.0)
    {
        need = 0.0;
    }
    else if (need > 2.0 * MAX_DURATION)
    {
        need = 2.0 * MAX_DURATION; // raw-program guard; builders stay below
    }
    F64 end = start + llmax(rel, need);
    for (S32 i = 0; i < 8 && isFiniteBits(end); ++i)
    {
        // Reproduce the sampler's arithmetic exactly: reduce against the
        // epoch, then subtract the stage-two offset.
        const F64 local = end - start;
        const F64 dt    = two_stage ? local - stage : local;
        if (dt >= dur)
        {
            break; // sampler clamps u >= 1 -> exact terminal state
        }
        end = std::nextafter(end, std::numeric_limits<F64>::infinity());
    }
    return isFiniteBits(end) ? end : start;
}

// Program sampling in the program-relative frame: the absolute time is
// reduced ONCE against the epoch (exact for nearby values), and each stage
// is sampled at a relative offset, so stage routing and in-stage u are
// immune to absolute-time precision loss. The finiteness guard is the
// bit-check (std::isfinite may be constant-folded away under /fp:fast).
inline ScalarSample sample(const ScalarProgram& program, F64 time)
{
    if (!isFiniteBits(time))
    {
        return sample(program.mSegments[0], time); // start state
    }
    const F64 local = time - sanitizeTime(program.mStartTime);
    if (program.mSegmentCount > 1)
    {
        const F64 stage2 = sanitizeTime(program.mStageOffset);
        if (local >= stage2)
        {
            return sampleOffset(program.mSegments[1], local - stage2);
        }
    }
    return sampleOffset(program.mSegments[0], local);
}

// Segment builder with a feasibility pass:
//  1. construct with the requested duration;
//  2. if `monotonic`, grid-check for reversal/overshoot;
//  3. on failure, lengthen T (x1.5, capped retries) and retry;
//  4. final fallback: a two-stage brake-then-go program.
//
// Brake-then-go fallback: stage one is a quintic that decelerates the
// incoming (v0, a0) to full rest at an intermediate stop position; stage two
// is a clean rest-to-target quintic, strictly monotone for a settling
// endpoint. The stop position is the quintic brake's rest projection from
// BOTH incoming derivatives,
//     p_stop = p0 + v0 t/2 + a0 t^2/20,
// which makes the brake's scaled velocity factor as
//     T v(u) = (1-u)^2 [ T v0 (1 + 2u) + T^2 a0 u (1 - u) ],
// i.e. reversal-free whenever v0 and a0 both point toward the target. The
// stop is additionally capped at half the remaining travel, and the brake
// duration is halved (down to MIN_DURATION) until the analytic crossing
// check proves the brake stage never passes p1; the go stage is checked
// with the same monotone test as the primary path. Both stages are built
// with durations >= MIN_DURATION so neither can collapse into a step, and
// they share an exact (p_stop, 0, 0) boundary state, so the program stays
// C2 throughout.
//
// Small-remaining-travel regime: when the travel left to p1 is far smaller
// than what any C2 settle from (v0, a0) can cover — even a MIN_DURATION
// brake sweeps at least ~|v0| MIN_DURATION / 2 — no capped brake can stay
// inside the crossing tolerance. A hard velocity step here is avoidable
// and unacceptable (it fires for ordinary gaze values, e.g. v0 = 1 toward
// a target 1e-9 away), so a second, overshoot-tolerant tier runs first: the
// brake settles to its NATURAL rest projection (uncapped) with the crossing
// tolerance widened to at most SETTLE_OVERSHOOT_TOL, trading a bounded,
// imperceptible overshoot of p1 for full C1/C2 velocity continuity; the go
// stage then eases the tiny distance back to p1 from rest.
//
// Residual C0 escape (the only remaining velocity discontinuity): if even a
// MIN_DURATION natural settle overshoots p1 by more than the tolerance —
// i.e. |v0| MIN_DURATION / 2 + |a0| MIN_DURATION^2 / 20 > |d| + tol, which
// requires |v0| >~ 2e2 channel-units/s or |a0| >~ 2e9 units/s^2 against
// near-zero remaining travel (a SMALL-REMAINING-TRAVEL condition; it is not
// confined to inputs near the 1e12 sanitize bound, merely guaranteed there)
// — the brake degenerates to a position-continuous (C0) step to rest at p0,
// the only way left to honor never-pass-the-target. This seam is
// sub-perceptual: the step is the tau -> 0 limit of admissible C1 settles,
// and the C1 settle over tau = 2 tol / |v0| (< MIN_DURATION exactly in this
// regime, hence far below any frame period) differs from it in position by
// at most |v0| tau / 2 = SETTLE_OVERSHOOT_TOL at every sample time, so no
// frame-sampled render can distinguish the step from a genuinely
// continuous motion to better than the same tolerance the smooth tier is
// allowed.
//
// When the incoming velocity points away from the target, a brief wrong-way
// excursion (on the far side only) is inherent to any C1-continuous motion
// and is kept as short as the brake stage. The never-pass guarantee applies
// to settling terminal conditions; a caller-requested nonzero v1/a1 is
// honored as given and can legitimately carry motion through p1.
inline ScalarProgram solveScalar(F64 start_time, F64 duration,
                                 F32 p0, F32 v0, F32 a0,
                                 F32 p1, F32 v1, F32 a1,
                                 bool monotonic,
                                 U64 source_event_id = 0,
                                 U32 behavior_version = 0)
{
    start_time = sanitizeTime(start_time);
    F64 t_req = sanitizeDuration(duration);
    if (t_req <= 0.0)
    {
        t_req = MIN_DURATION;
    }
    p0 = sanitizeValue(p0); v0 = sanitizeValue(v0); a0 = sanitizeValue(a0);
    p1 = sanitizeValue(p1); v1 = sanitizeValue(v1); a1 = sanitizeValue(a1);

    ScalarProgram out;
    out.mStartTime   = start_time;
    out.mEndOffset   = t_req;
    out.mSegments[0] = buildSegment(start_time, t_req,
                                    p0, v0, a0, p1, v1, a1,
                                    source_event_id, behavior_version);
    if (!monotonic || checkMonotone(out.mSegments[0]))
    {
        return out;
    }

    F64 t_try = t_req;
    for (S32 retry = 1; retry <= MAX_LENGTHEN_RETRIES; ++retry)
    {
        t_try = llmin(t_try * LENGTHEN_FACTOR, MAX_DURATION);
        const ScalarSegment candidate =
            buildSegment(start_time, t_try, p0, v0, a0, p1, v1, a1,
                         source_event_id, behavior_version);
        if (checkMonotone(candidate))
        {
            out.mSegments[0]    = candidate;
            out.mLengthenRetries = retry;
            out.mEndOffset       = t_try;
            return out;
        }
    }

    // Two-stage brake-then-go fallback (see the comment block above).
    const F32 d        = p1 - p0;
    const F64 dir      = d >= 0.f ? 1.0 : -1.0;
    const F64 abs_d    = std::fabs(static_cast<F64>(d));
    const F64 cross_tol = llmax(abs_d, 1.0e-4) * 1.0e-3;
    const F32 speed    = fabsf(v0);
    F64 t_brake;
    if (speed > 1e-6f && v0 * static_cast<F32>(dir) > 0.f && abs_d > 1e-6)
    {
        // Moving toward the target: braking covers ~v0 * t / 2, so stopping
        // after at most half the remaining travel gives t = |d| / |v0|.
        t_brake = llmin(abs_d / static_cast<F64>(speed), t_req * 0.5);
    }
    else
    {
        // Moving away (or from rest under acceleration): brake promptly.
        t_brake = t_req * 0.25;
    }
    t_brake = llclamp(t_brake, MIN_DURATION, MAX_DURATION);
    const F64 t_brake_init = t_brake;

    // Shrink the brake until it provably cannot cross the target. The
    // brake's excursion scales with |v0| t + |a0| t^2, so halving t drives
    // it to zero; 48 halvings span MAX_DURATION -> MIN_DURATION.
    constexpr S32 MAX_BRAKE_SHRINKS = 48;
    ScalarSegment brake;
    F32  p_stop   = p0;
    bool brake_ok = false;
    for (S32 i = 0; i < MAX_BRAKE_SHRINKS; ++i)
    {
        // Rest projection of the quintic brake from BOTH v0 and a0, capped
        // so the planned stop never reaches past the midpoint of the
        // remaining travel.
        F64 rest = 0.5 * static_cast<F64>(v0) * t_brake +
                   0.05 * static_cast<F64>(a0) * t_brake * t_brake;
        if (rest * dir > 0.5 * abs_d)
        {
            rest = dir * 0.5 * abs_d;
        }
        p_stop = sanitizeValue(p0 + static_cast<F32>(rest));
        brake  = buildSegment(start_time, t_brake, p0, v0, a0,
                              p_stop, 0.f, 0.f,
                              source_event_id, behavior_version);
        if (checkNeverCrosses(brake, static_cast<F64>(p1), dir, cross_tol))
        {
            brake_ok = true;
            break;
        }
        if (t_brake <= MIN_DURATION)
        {
            break;
        }
        t_brake = llmax(t_brake * 0.5, MIN_DURATION);
    }
    if (!brake_ok)
    {
        // Overshoot-tolerant settle (small-remaining-travel regime, see the
        // comment block above): no capped brake fits inside cross_tol, so
        // let the brake settle to its NATURAL rest projection and widen the
        // never-cross tolerance to at most SETTLE_OVERSHOOT_TOL. This keeps
        // velocity fully continuous at the price of a bounded, imperceptible
        // overshoot of p1; the go stage returns the tiny distance from rest.
        const F64 settle_tol = llmax(cross_tol, SETTLE_OVERSHOOT_TOL);
        t_brake = t_brake_init;
        for (S32 i = 0; i < MAX_BRAKE_SHRINKS; ++i)
        {
            const F64 rest = 0.5 * static_cast<F64>(v0) * t_brake +
                             0.05 * static_cast<F64>(a0) * t_brake * t_brake;
            p_stop = sanitizeValue(p0 + static_cast<F32>(rest));
            brake  = buildSegment(start_time, t_brake, p0, v0, a0,
                                  p_stop, 0.f, 0.f,
                                  source_event_id, behavior_version);
            if (checkNeverCrosses(brake, static_cast<F64>(p1), dir,
                                  settle_tol))
            {
                brake_ok = true;
                break;
            }
            if (t_brake <= MIN_DURATION)
            {
                break;
            }
            t_brake = llmax(t_brake * 0.5, MIN_DURATION);
        }
    }
    if (!brake_ok)
    {
        // Documented residual escape: even a MIN_DURATION natural settle
        // overshoots p1 by more than SETTLE_OVERSHOOT_TOL (extreme incoming
        // rates against near-zero remaining travel), so step to rest in
        // place. Position remains continuous (C0); velocity steps, and the
        // comment block above proves the step is indistinguishable from a
        // sub-MIN_DURATION C1 settle to within the same tolerance.
        t_brake = 0.0;
        p_stop  = p0;
        brake   = buildSegment(start_time, 0.0, p0, v0, a0, p0, 0.f, 0.f,
                               source_event_id, behavior_version);
    }

    // Go stage: never below MIN_DURATION, so buildSegment cannot sanitize
    // it into a step whose join with the brake stage would be discontinuous.
    F64 t_go = llmax(llmax(t_req - t_brake, t_req * 0.25), MIN_DURATION);
    ScalarSegment go = buildSegment(start_time + t_brake, t_go,
                                    p_stop, 0.f, 0.f, p1, v1, a1,
                                    source_event_id, behavior_version);
    // Feasibility-check the go stage with the same monotone test as the
    // primary path. A settling endpoint (v1 = a1 = 0) from rest is monotone
    // by construction; a non-settling endpoint may need lengthening, and
    // after the capped retries the last candidate is kept as best effort.
    for (S32 retry = 1;
         retry <= MAX_LENGTHEN_RETRIES && !checkMonotone(go); ++retry)
    {
        t_go = llmin(t_go * LENGTHEN_FACTOR, MAX_DURATION);
        go   = buildSegment(start_time + t_brake, t_go,
                            p_stop, 0.f, 0.f, p1, v1, a1,
                            source_event_id, behavior_version);
    }

    out.mSegments[0]   = brake;
    out.mSegments[1]   = go;
    out.mSegmentCount  = 2;
    out.mBrakeFallback = true;
    // Relative stage boundaries: t_brake and t_go are kept as the exact F64
    // durations the stages were built from — never re-derived by
    // subtracting absolute stage start times, which loses them entirely
    // when start_time's ulp exceeds the durations (e.g. start = 2^53).
    out.mStageOffset   = t_brake;
    out.mEndOffset     = t_brake + t_go;
    return out;
}

// C2 handoff + feasibility in one step: sample the interrupted segment at
// `now` (p/v/a) and solve a new program toward the new endpoint.
inline ScalarProgram solveRetarget(const ScalarSegment& old_segment, F64 now,
                                   F64 duration,
                                   F32 p1, F32 v1, F32 a1,
                                   bool monotonic,
                                   U64 source_event_id = 0,
                                   U32 behavior_version = 0)
{
    const ScalarSample s = sample(old_segment, now);
    return solveScalar(now, duration, s.mP, s.mV, s.mA, p1, v1, a1,
                       monotonic, source_event_id, behavior_version);
}

// ---------------------------------------------------------------------------
// Shortest-arc scalar adapter for angular channels (companion section 3.8)
// ---------------------------------------------------------------------------
constexpr F64 TRAJECTORY_TWO_PI = 6.28318530717958647692528676655900577;

// Wrap into (-pi, pi]. The seam convention is deterministic: exactly-opposite
// angles resolve to +pi, so identical inputs always take the same arc.
inline F32 wrapToPi(F32 angle)
{
    if (!isFiniteBits(angle))
    {
        return 0.f;
    }
    F64 a = std::fmod(static_cast<F64>(angle), TRAJECTORY_TWO_PI);
    if (a <= -F_PI)
    {
        a += TRAJECTORY_TWO_PI;
    }
    else if (a > F_PI)
    {
        a -= TRAJECTORY_TWO_PI;
    }
    return static_cast<F32>(a);
}

// Signed shortest angular delta from `from` to `to`, in (-pi, pi]. Stable
// across the +/-pi seam: +179 deg -> -179 deg is +2 deg, never -358 deg.
inline F32 shortestArcDelta(F32 from_angle, F32 to_angle)
{
    if (!isFiniteBits(from_angle) || !isFiniteBits(to_angle))
    {
        return 0.f;
    }
    return wrapToPi(static_cast<F32>(
        static_cast<F64>(to_angle) - static_cast<F64>(from_angle)));
}

// Unwrap `angle` to the representative nearest `reference` (differs from it
// by less than pi). Segments built on unwrapped angles cross the seam
// smoothly; callers re-wrap sampled positions for display/apply.
inline F32 unwrapNear(F32 reference, F32 angle)
{
    return sanitizeValue(reference) + shortestArcDelta(reference, angle);
}

// Build a feasible angular program taking the shortest arc to the target.
// `angle0` is the current (already continuous/unwrapped) channel angle; the
// target is unwrapped next to it before the segment is constructed, so the
// polynomial itself never sees the seam.
inline ScalarProgram solveShortestArc(F64 start_time, F64 duration,
                                      F32 angle0, F32 v0, F32 a0,
                                      F32 target_angle, F32 v1, F32 a1,
                                      bool monotonic,
                                      U64 source_event_id = 0,
                                      U32 behavior_version = 0)
{
    const F32 p1 = unwrapNear(angle0, target_angle);
    return solveScalar(start_time, duration, angle0, v0, a0, p1, v1, a1,
                       monotonic, source_event_id, behavior_version);
}

} // namespace ALTrajectory

#endif // AL_ALTRAJECTORY_H
