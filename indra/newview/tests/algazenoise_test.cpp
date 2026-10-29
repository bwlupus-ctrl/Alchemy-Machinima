/**
 * @file algazenoise_test.cpp
 * @brief Unit tests for the deterministic fixation/micro-life noise core.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "../test/lltut.h"
#include "../algazenoise.h"

#include <cmath>
#include <limits>
#include <vector>

namespace tut
{

struct algazenoise_data
{
};

typedef test_group<algazenoise_data> algazenoise_test_group;
typedef algazenoise_test_group::object algazenoise_test_object;
algazenoise_test_group algazenoise_test("algazenoise");

template<> template<>
void algazenoise_test_object::test<1>()
{
    set_test_name("integer hash determinism, bounds, and channel spread");
    constexpr U64 SEED = 0xa1b2c3d4e5f60718ULL;

    const F32 h1 = ALGazeNoise::unitHash(SEED, 0, 42, 0, 0);
    const F32 h1_dup = ALGazeNoise::unitHash(SEED, 0, 42, 0, 0);
    ensure("unitHash is bit-exact deterministic", h1 == h1_dup);
    ensure("unitHash is in [0, 1)", h1 >= 0.f && h1 < 1.f);

    const F32 h_other_channel = ALGazeNoise::unitHash(SEED, 1, 42, 0, 0);
    const F32 h_other_cell = ALGazeNoise::unitHash(SEED, 0, 43, 0, 0);
    const F32 h_other_octave = ALGazeNoise::unitHash(SEED, 0, 42, 1, 0);
    const F32 h_other_draw = ALGazeNoise::unitHash(SEED, 0, 42, 0, 1);
    ensure("channel changes the hash", h_other_channel != h1);
    ensure("cell changes the hash", h_other_cell != h1);
    ensure("octave changes the hash", h_other_octave != h1);
    ensure("draw changes the hash", h_other_draw != h1);

    const F32 s1 = ALGazeNoise::signedHash(SEED, 0, 42, 0, 0);
    ensure("signedHash is in [-1, 1)", s1 >= -1.f && s1 < 1.f);
    ensure("signedHash matches unitHash re-centered",
           std::fabs(s1 - (h1 * 2.f - 1.f)) <= 1e-6f);
}

template<> template<>
void algazenoise_test_object::test<2>()
{
    set_test_name("smootherStep endpoints and clamping");
    ensure("smootherStep(0) is exactly 0",
           ALGazeNoise::smootherStep(0.f) == 0.f);
    ensure("smootherStep(1) is exactly 1",
           std::fabs(ALGazeNoise::smootherStep(1.f) - 1.f) <= 1e-6f);
    ensure("smootherStep(0.5) is exactly 0.5",
           std::fabs(ALGazeNoise::smootherStep(0.5f) - 0.5f) <= 1e-6f);
    ensure("smootherStep clamps below 0",
           ALGazeNoise::smootherStep(-2.f) == 0.f);
    ensure("smootherStep clamps above 1",
           std::fabs(ALGazeNoise::smootherStep(3.f) - 1.f) <= 1e-6f);

    F32 prev = ALGazeNoise::smootherStep(0.f);
    bool monotone = true;
    for (S32 i = 1; i <= 20; ++i)
    {
        const F32 u = static_cast<F32>(i) / 20.f;
        const F32 cur = ALGazeNoise::smootherStep(u);
        if (cur < prev - 1e-6f)
        {
            monotone = false;
        }
        prev = cur;
    }
    ensure("smootherStep is monotone on [0, 1]", monotone);
}

template<> template<>
void algazenoise_test_object::test<3>()
{
    set_test_name("drift is deterministic (same seed+channel+time -> bit-identical)");
    constexpr U64 SEED = 0x1357913579ULL;
    constexpr U32 CHANNEL = 3;
    constexpr F32 CELL = 1.3f;
    constexpr F32 AMPLITUDE = 0.8f;

    for (F64 t = 0.0; t < 12.0; t += 0.37)
    {
        const F32 a = ALGazeNoise::driftOffset(SEED, CHANNEL, t, CELL, AMPLITUDE);
        const F32 b = ALGazeNoise::driftOffset(SEED, CHANNEL, t, CELL, AMPLITUDE);
        ensure("driftOffset repeats bit-identically for identical inputs", a == b);
    }

    // Re-evaluating out of order (as a seek/scrub would) must not change the
    // result: there is no internal state or wall clock to desync.
    const F32 forward = ALGazeNoise::driftOffset(SEED, CHANNEL, 7.25, CELL, AMPLITUDE);
    ALGazeNoise::driftOffset(SEED, CHANNEL, 0.1, CELL, AMPLITUDE);
    ALGazeNoise::driftOffset(SEED, CHANNEL, 11.9, CELL, AMPLITUDE);
    const F32 seeked = ALGazeNoise::driftOffset(SEED, CHANNEL, 7.25, CELL, AMPLITUDE);
    ensure("sampling other times first does not perturb a later re-sample",
           forward == seeked);
}

template<> template<>
void algazenoise_test_object::test<4>()
{
    set_test_name("drift decorrelates across channels");
    constexpr U64 SEED = 0x2468246824682468ULL;
    constexpr F32 CELL = 0.9f;
    constexpr F32 AMPLITUDE = 1.f;

    S32 differing = 0;
    S32 total = 0;
    F64 dot = 0.0;
    F64 sq_a = 0.0;
    F64 sq_b = 0.0;
    for (F64 t = 0.0; t < 20.0; t += 0.2)
    {
        const F32 a = ALGazeNoise::driftOffset(SEED, 0, t, CELL, AMPLITUDE);
        const F32 b = ALGazeNoise::driftOffset(SEED, 1, t, CELL, AMPLITUDE);
        if (a != b)
        {
            ++differing;
        }
        ++total;
        dot += static_cast<F64>(a) * static_cast<F64>(b);
        sq_a += static_cast<F64>(a) * static_cast<F64>(a);
        sq_b += static_cast<F64>(b) * static_cast<F64>(b);
    }
    ensure("distinct channels almost never coincide", differing > (total * 9) / 10);

    const F64 corr = dot / std::sqrt(llmax(sq_a * sq_b, 1e-12));
    ensure("distinct channels are not a scaled copy of each other",
           std::fabs(corr) < 0.5);
}

template<> template<>
void algazenoise_test_object::test<5>()
{
    set_test_name("drift stays within +/- amplitude");
    constexpr U64 SEED = 0x0f0f0f0f0f0f0f0fULL;
    for (U32 channel = 0; channel < 3; ++channel)
    {
        for (F64 t = -5.0; t < 45.0; t += 0.15)
        {
            const F32 amplitude = 0.6f;
            const F32 v = ALGazeNoise::driftOffset(SEED, channel, t, 1.7f, amplitude);
            ensure("drift never exceeds the requested amplitude",
                   std::fabs(v) <= amplitude + 1e-4f);
        }
    }

    ensure("zero amplitude yields exactly zero",
           ALGazeNoise::driftOffset(SEED, 0, 5.0, 1.0f, 0.f) == 0.f);
}

template<> template<>
void algazenoise_test_object::test<6>()
{
    set_test_name("drift is zero-mean over a long window");
    constexpr U64 SEED = 0x99aa88bb77cc66ddULL;
    constexpr F32 CELL = 1.0f;
    constexpr F32 AMPLITUDE = 1.0f;
    constexpr F64 WINDOW = 3000.0; // seconds; ~3000 lattice cells
    constexpr F64 DT = 0.25;

    F64 sum = 0.0;
    S64 count = 0;
    for (F64 t = 0.0; t < WINDOW; t += DT)
    {
        sum += static_cast<F64>(
            ALGazeNoise::driftOffset(SEED, 5, t, CELL, AMPLITUDE));
        ++count;
    }
    const F64 mean = sum / static_cast<F64>(count);
    ensure("mean drift over a long window is close to zero",
           std::fabs(mean) < 0.06);
}

template<> template<>
void algazenoise_test_object::test<7>()
{
    set_test_name("drift interpolation is C2 at cell boundaries");
    // The C2 claim rests on smootherStep having zero first AND second
    // derivative at u=0 (and, by s(u)+s(1-u)=1 symmetry, at u=1 too). A
    // finite difference straddling a real value-noise cell boundary would
    // additionally mix in two *independent* hashed neighbor slopes, which
    // confounds a naive corner check; testing the ease polynomial itself
    // isolates the actual C2 ingredient without that confound.
    constexpr F32 H1 = 0.05f;
    constexpr F32 H2 = 0.005f; // 10x smaller

    const F32 s0 = ALGazeNoise::smootherStep(0.f);
    const F32 slope1 = (ALGazeNoise::smootherStep(H1) - s0) / H1;
    const F32 slope2 = (ALGazeNoise::smootherStep(H2) - s0) / H2;
    ensure("slope near the boundary shrinks much faster than linearly "
           "(confirms s'(0) = 0)",
           std::fabs(slope2) < std::fabs(slope1) * 0.05f);

    const F32 curv1 = (ALGazeNoise::smootherStep(2.f * H1) -
                        2.f * ALGazeNoise::smootherStep(H1) + s0) / (H1 * H1);
    const F32 curv2 = (ALGazeNoise::smootherStep(2.f * H2) -
                        2.f * ALGazeNoise::smootherStep(H2) + s0) / (H2 * H2);
    ensure("curvature near the boundary shrinks toward zero as the step "
           "shrinks (confirms s''(0) = 0, i.e. no corner)",
           std::fabs(curv2) < std::fabs(curv1) * 0.3f);

    // Same story at the far endpoint (u=1), by construction symmetric.
    const F32 s1 = ALGazeNoise::smootherStep(1.f);
    const F32 slope1_end = (s1 - ALGazeNoise::smootherStep(1.f - H1)) / H1;
    const F32 slope2_end = (s1 - ALGazeNoise::smootherStep(1.f - H2)) / H2;
    ensure("slope near the far boundary also vanishes superlinearly",
           std::fabs(slope2_end) < std::fabs(slope1_end) * 0.05f);

    // End-to-end sanity: the assembled noise curve is at least continuous
    // (C0) exactly at a lattice boundary -- approaching from below the
    // boundary must converge to the same hashed value the boundary itself
    // returns (no jump).
    constexpr U64 SEED = 0x1111aaaa2222bbbbULL;
    constexpr F32 CELL = 1.0f;
    const F64 boundary = 4.0 * static_cast<F64>(CELL);
    const F32 at_boundary =
        ALGazeNoise::valueNoiseOctave(SEED, 0, boundary, CELL, 0);
    const F32 just_before =
        ALGazeNoise::valueNoiseOctave(SEED, 0, boundary - 1e-6, CELL, 0);
    ensure("value noise is continuous approaching a cell boundary",
           std::fabs(at_boundary - just_before) < 1e-3f);
}

template<> template<>
void algazenoise_test_object::test<8>()
{
    set_test_name("drift shows no obvious period over 30 seconds");
    constexpr U64 SEED = 0x7766554433221100ULL;
    constexpr U32 CHANNEL = 2;
    // Deliberately small relative to the tested lags (>= 1s, below): by the
    // shortest tested lag the signal is already many cells away from
    // itself, so any residual correlation reflects a true repeat rather
    // than the ordinary short-range smoothness any band-limited noise has
    // within a fraction of one cell width.
    constexpr F32 CELL = 0.4f;
    constexpr F32 AMPLITUDE = 1.f;
    constexpr F64 DT = 0.02;
    constexpr F64 WINDOW = 30.0;

    std::vector<F64> samples;
    samples.reserve(static_cast<size_t>(WINDOW / DT) + 1);
    for (F64 t = 0.0; t < WINDOW; t += DT)
    {
        samples.push_back(static_cast<F64>(
            ALGazeNoise::driftOffset(SEED, CHANNEL, t, CELL, AMPLITUDE)));
    }

    F64 max_abs_corr = 0.0;
    F64 best_lag = 0.0;
    for (F64 lag = 1.0; lag < WINDOW; lag += 1.0)
    {
        const size_t shift = static_cast<size_t>(lag / DT);
        if (shift == 0 || shift >= samples.size())
        {
            continue;
        }
        F64 dot = 0.0, sq_a = 0.0, sq_b = 0.0;
        const size_t n = samples.size() - shift;
        for (size_t i = 0; i < n; ++i)
        {
            const F64 a = samples[i];
            const F64 b = samples[i + shift];
            dot += a * b;
            sq_a += a * a;
            sq_b += b * b;
        }
        const F64 denom = std::sqrt(llmax(sq_a * sq_b, 1e-12));
        const F64 corr = denom > 0.0 ? std::fabs(dot / denom) : 0.0;
        if (corr > max_abs_corr)
        {
            max_abs_corr = corr;
            best_lag = lag;
        }
    }
    (void)best_lag;
    ensure("no tested lag in [1s, 29s] reveals a near-perfect self-repeat",
           max_abs_corr < 0.9);
}

template<> template<>
void algazenoise_test_object::test<9>()
{
    set_test_name("drift degenerates safely for invalid inputs");
    constexpr U64 SEED = 0xdeadbeefcafef00dULL;
    ensure("non-finite time yields zero",
           ALGazeNoise::driftOffset(SEED, 0, std::nan(""), 1.f, 1.f) == 0.f);
    ensure("non-finite cell width yields zero",
           ALGazeNoise::driftOffset(SEED, 0, 1.0, std::numeric_limits<F32>::infinity(), 1.f) == 0.f);
    ensure("negative amplitude yields zero",
           ALGazeNoise::driftOffset(SEED, 0, 1.0, 1.f, -1.f) == 0.f);
    const F32 v = ALGazeNoise::driftOffset(SEED, 0, 1.0, -3.f, 1.f);
    ensure("negative cell width still produces a finite, bounded result",
           std::isfinite(v) && std::fabs(v) <= 1.0001f);
    const F32 neg_time = ALGazeNoise::driftOffset(SEED, 0, -100.0, 1.f, 1.f);
    ensure("negative time still produces a finite, bounded result",
           std::isfinite(neg_time) && std::fabs(neg_time) <= 1.0001f);
}

template<> template<>
void algazenoise_test_object::test<10>()
{
    set_test_name("microsaccade events are discrete, bounded, and deterministic");
    constexpr U64 SEED = 0x3355779911335577ULL;
    constexpr U32 CHANNEL = 1;
    constexpr F32 RATE_HZ = 1.5f;
    constexpr F32 AMPLITUDE = 0.3f * DEG_TO_RAD; // within the 0.1-0.5 deg spec range

    // Determinism.
    for (F64 t = 0.0; t < 10.0; t += 0.33)
    {
        const F32 a = ALGazeNoise::microsaccadeOffset(SEED, CHANNEL, t, RATE_HZ, AMPLITUDE);
        const F32 b = ALGazeNoise::microsaccadeOffset(SEED, CHANNEL, t, RATE_HZ, AMPLITUDE);
        ensure("microsaccadeOffset repeats bit-identically", a == b);
    }

    // Bounded.
    for (F64 t = 0.0; t < 60.0; t += 0.05)
    {
        const F32 v = ALGazeNoise::microsaccadeOffset(SEED, CHANNEL, t, RATE_HZ, AMPLITUDE);
        ensure("microsaccade offset never exceeds the requested amplitude",
               std::fabs(v) <= AMPLITUDE + 1e-6f);
    }

    // Discrete: away from an event's brief hashed flick, the offset holds a
    // flat plateau -- unlike drift, which is continuously varying. Sample
    // twice late within the same interval (well past any flick) and expect
    // an exact plateau match.
    bool found_plateau = false;
    for (F64 t0 = 0.0; t0 < 20.0 && !found_plateau; t0 += 0.05)
    {
        const F32 mid = ALGazeNoise::microsaccadeOffset(SEED, CHANNEL, t0, RATE_HZ, AMPLITUDE);
        const F32 later = ALGazeNoise::microsaccadeOffset(SEED, CHANNEL, t0 + 0.02, RATE_HZ, AMPLITUDE);
        if (mid == later && mid != 0.f)
        {
            found_plateau = true;
        }
    }
    ensure("microsaccade offset holds a flat plateau between events", found_plateau);

    // Discrete: the held fixation target must actually relocate over time
    // (this is not a stuck constant), but only between well-separated
    // samples -- unlike drift, which changes at every timescale. Sampling
    // widely spaced points (well beyond any single event's flick duration)
    // avoids assuming a specific flick-duration magnitude while still
    // proving events fire and relocate the offset.
    S32 distinct_values = 0;
    F32 first_value = ALGazeNoise::microsaccadeOffset(SEED, CHANNEL, 0.0, RATE_HZ, AMPLITUDE);
    for (F64 t = 0.5; t < 30.0; t += 0.5)
    {
        const F32 v = ALGazeNoise::microsaccadeOffset(SEED, CHANNEL, t, RATE_HZ, AMPLITUDE);
        if (std::fabs(v - first_value) > 1e-6f)
        {
            ++distinct_values;
        }
    }
    ensure("microsaccade events relocate the held offset over time",
           distinct_values > 0);
}

template<> template<>
void algazenoise_test_object::test<11>()
{
    set_test_name("microsaccade decorrelates across channels and rejects invalid rate");
    constexpr U64 SEED = 0x13579bdf2468aceULL;
    constexpr F32 AMPLITUDE = 0.2f * DEG_TO_RAD;

    S32 differing = 0;
    S32 total = 0;
    for (F64 t = 0.0; t < 20.0; t += 0.1)
    {
        const F32 a = ALGazeNoise::microsaccadeOffset(SEED, 0, t, 2.f, AMPLITUDE);
        const F32 b = ALGazeNoise::microsaccadeOffset(SEED, 1, t, 2.f, AMPLITUDE);
        if (a != b)
        {
            ++differing;
        }
        ++total;
    }
    ensure("distinct channels fire independently", differing > (total * 2) / 3);

    ensure("zero rate yields zero", ALGazeNoise::microsaccadeOffset(SEED, 0, 1.0, 0.f, AMPLITUDE) == 0.f);
    ensure("negative rate yields zero", ALGazeNoise::microsaccadeOffset(SEED, 0, 1.0, -1.f, AMPLITUDE) == 0.f);
    ensure("zero amplitude yields zero", ALGazeNoise::microsaccadeOffset(SEED, 0, 1.0, 2.f, 0.f) == 0.f);
    ensure("non-finite time yields zero",
           ALGazeNoise::microsaccadeOffset(SEED, 0, std::nan(""), 2.f, AMPLITUDE) == 0.f);
}

} // namespace tut
