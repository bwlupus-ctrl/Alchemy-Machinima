/**
 * @file algazeblink_test.cpp
 * @brief Unit tests for the pure asymmetric blink aperture shape.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "../test/lltut.h"
#include "../algazeblink.h"

#include <cmath>

namespace tut
{
struct algazeblink_data
{
};

typedef test_group<algazeblink_data> algazeblink_test_group;
typedef algazeblink_test_group::object algazeblink_test_object;
algazeblink_test_group algazeblink_test("algazeblink");

template<> template<>
void algazeblink_test_object::test<1>()
{
    set_test_name("closure is zero before onset, at onset, and after the full span");
    constexpr F32 EPS = 1e-6f;

    ensure("negative time is zero",
           std::fabs(ALGazeBlink::blinkClosure(-0.05)) <= EPS);
    ensure("t=0 (onset) is zero",
           std::fabs(ALGazeBlink::blinkClosure(0.0)) <= EPS);

    const F32 close_ms = 85.f, hold_ms = 20.f, open_ms = 165.f;
    const F64 total_s = static_cast<F64>(
        ALGazeBlink::blinkTotalMs(close_ms, hold_ms, open_ms)) / 1000.0;
    ensure("t == total span is zero",
           std::fabs(ALGazeBlink::blinkClosure(
               total_s, close_ms, hold_ms, open_ms)) <= EPS);
    ensure("t well past the total span is zero",
           std::fabs(ALGazeBlink::blinkClosure(
               total_s * 4.0, close_ms, hold_ms, open_ms)) <= EPS);

    // Defaults (hold_ms = 0) behave the same way.
    const F64 default_total_s =
        static_cast<F64>(ALGazeBlink::blinkTotalMs()) / 1000.0;
    ensure("default-envelope onset is zero",
           std::fabs(ALGazeBlink::blinkClosure(0.0)) <= EPS);
    ensure("default-envelope total span is zero",
           std::fabs(ALGazeBlink::blinkClosure(default_total_s)) <= EPS);
}

template<> template<>
void algazeblink_test_object::test<2>()
{
    set_test_name("peak closure equals depth at the close/open transition");
    constexpr F32 EPS = 1e-5f;

    // No hold: the close ramp and open ramp meet exactly at close_ms.
    const F32 close_ms = ALGazeBlink::DEFAULT_CLOSE_MS;
    const F32 open_ms  = ALGazeBlink::DEFAULT_OPEN_MS;
    const F64 transition_s = static_cast<F64>(close_ms) / 1000.0;

    const F32 full = ALGazeBlink::blinkClosure(
        transition_s, close_ms, 0.f, open_ms, 1.f);
    ensure("full-depth peak at the close/open junction is 1.0",
           std::fabs(full - 1.f) <= EPS);

    const F32 partial = ALGazeBlink::blinkClosure(
        transition_s, close_ms, 0.f, open_ms, 0.4f);
    ensure("partial-depth peak at the close/open junction equals depth",
           std::fabs(partial - 0.4f) <= EPS);

    // With a hold, the whole plateau -- including both its ends -- sits at
    // depth exactly.
    const F32 hold_ms = 20.f;
    const F64 hold_start_s = transition_s;
    const F64 hold_mid_s = transition_s + 0.5 * (hold_ms / 1000.0);
    const F64 hold_end_s = transition_s + (hold_ms / 1000.0);
    ensure("plateau start is at depth",
           std::fabs(ALGazeBlink::blinkClosure(
               hold_start_s, close_ms, hold_ms, open_ms, 0.7f) - 0.7f) <= EPS);
    ensure("plateau middle is at depth",
           std::fabs(ALGazeBlink::blinkClosure(
               hold_mid_s, close_ms, hold_ms, open_ms, 0.7f) - 0.7f) <= EPS);
    ensure("plateau end is at depth",
           std::fabs(ALGazeBlink::blinkClosure(
               hold_end_s, close_ms, hold_ms, open_ms, 0.7f) - 0.7f) <= EPS);
}

template<> template<>
void algazeblink_test_object::test<3>()
{
    set_test_name("close phase is monotonically increasing");
    const F32 close_ms = ALGazeBlink::DEFAULT_CLOSE_MS;
    const F32 open_ms  = ALGazeBlink::DEFAULT_OPEN_MS;

    F32 previous = -1.f;
    constexpr S32 STEPS = 40;
    for (S32 i = 1; i <= STEPS; ++i)
    {
        // Sample strictly inside (0, close_ms), excluding the endpoints.
        const F64 t_s = (static_cast<F64>(i) / (STEPS + 1)) *
                        (static_cast<F64>(close_ms) / 1000.0);
        const F32 closure = ALGazeBlink::blinkClosure(
            t_s, close_ms, 0.f, open_ms, 1.f);
        ensure("close-phase closure strictly increases", closure > previous);
        previous = closure;
    }
    ensure("close phase approaches full depth by its end", previous > 0.95f);
}

template<> template<>
void algazeblink_test_object::test<4>()
{
    set_test_name("open phase is monotonically decreasing");
    const F32 close_ms = ALGazeBlink::DEFAULT_CLOSE_MS;
    const F32 open_ms  = ALGazeBlink::DEFAULT_OPEN_MS;
    const F64 open_start_s = static_cast<F64>(close_ms) / 1000.0;

    F32 previous = 2.f; // above any valid closure
    constexpr S32 STEPS = 40;
    for (S32 i = 1; i <= STEPS; ++i)
    {
        // Sample strictly inside (close_ms, close_ms + open_ms).
        const F64 t_s = open_start_s +
            (static_cast<F64>(i) / (STEPS + 1)) *
            (static_cast<F64>(open_ms) / 1000.0);
        const F32 closure = ALGazeBlink::blinkClosure(
            t_s, close_ms, 0.f, open_ms, 1.f);
        ensure("open-phase closure strictly decreases", closure < previous);
        previous = closure;
    }
    ensure("open phase approaches fully open by its end", previous < 0.05f);
}

template<> template<>
void algazeblink_test_object::test<5>()
{
    set_test_name("close duration is shorter than open duration (asymmetry)");
    ensure("default close is shorter than default open",
           ALGazeBlink::DEFAULT_CLOSE_MS < ALGazeBlink::DEFAULT_OPEN_MS);

    // The shape itself is asymmetric, not just the constants: the time to
    // cross the halfway closure point on the way in is shorter than the
    // time to cross it on the way out, for the default envelope.
    const F32 close_ms = ALGazeBlink::DEFAULT_CLOSE_MS;
    const F32 open_ms  = ALGazeBlink::DEFAULT_OPEN_MS;
    const F64 open_start_s = static_cast<F64>(close_ms) / 1000.0;

    F64 half_close_time_s = -1.0;
    for (F64 t_s = 0.0; t_s < open_start_s; t_s += 0.0005)
    {
        if (ALGazeBlink::blinkClosure(t_s, close_ms, 0.f, open_ms, 1.f) >= 0.5f)
        {
            half_close_time_s = t_s;
            break;
        }
    }
    F64 half_open_time_s = -1.0;
    const F64 total_s = open_start_s + static_cast<F64>(open_ms) / 1000.0;
    for (F64 t_s = open_start_s; t_s < total_s; t_s += 0.0005)
    {
        if (ALGazeBlink::blinkClosure(t_s, close_ms, 0.f, open_ms, 1.f) <= 0.5f)
        {
            half_open_time_s = t_s - open_start_s;
            break;
        }
    }
    ensure("halfway point was found on both ramps",
           half_close_time_s >= 0.0 && half_open_time_s >= 0.0);
    ensure("closing reaches halfway sooner (relative to its own ramp) than opening",
           half_close_time_s < half_open_time_s);
}

template<> template<>
void algazeblink_test_object::test<6>()
{
    set_test_name("depth scales peak closure linearly");
    constexpr F32 EPS = 1e-5f;
    const F32 close_ms = ALGazeBlink::DEFAULT_CLOSE_MS;
    const F32 open_ms  = ALGazeBlink::DEFAULT_OPEN_MS;
    const F64 peak_s = static_cast<F64>(close_ms) / 1000.0;

    const F32 d25 = ALGazeBlink::blinkClosure(peak_s, close_ms, 0.f, open_ms, 0.25f);
    const F32 d50 = ALGazeBlink::blinkClosure(peak_s, close_ms, 0.f, open_ms, 0.50f);
    const F32 d100 = ALGazeBlink::blinkClosure(peak_s, close_ms, 0.f, open_ms, 1.00f);

    ensure("depth 0.25 peak matches 0.25", std::fabs(d25 - 0.25f) <= EPS);
    ensure("depth 0.50 peak matches 0.50", std::fabs(d50 - 0.50f) <= EPS);
    ensure("depth 1.00 peak matches 1.00", std::fabs(d100 - 1.00f) <= EPS);
    ensure("doubling depth doubles peak closure",
           std::fabs(d50 - 2.f * d25) <= EPS);
    ensure("quadrupling depth quadruples peak closure",
           std::fabs(d100 - 4.f * d25) <= EPS);
}

template<> template<>
void algazeblink_test_object::test<7>()
{
    set_test_name("closure is deterministic");
    const F64 t_s = 0.061;
    const F32 first = ALGazeBlink::blinkClosure(t_s, 85.f, 15.f, 165.f, 0.85f);
    const F32 second = ALGazeBlink::blinkClosure(t_s, 85.f, 15.f, 165.f, 0.85f);
    ensure("identical arguments give a bit-identical result", first == second);

    // Determinism holds across the whole span, not just one sample.
    for (F64 t = -0.05; t < 0.35; t += 0.013)
    {
        const F32 a = ALGazeBlink::blinkClosure(t, 85.f, 15.f, 165.f, 0.85f);
        const F32 b = ALGazeBlink::blinkClosure(t, 85.f, 15.f, 165.f, 0.85f);
        ensure("every sampled point is repeatable", a == b);
    }
}

template<> template<>
void algazeblink_test_object::test<8>()
{
    set_test_name("closure is always bounded to [0, 1]");
    const F32 close_ms = 90.f, hold_ms = 25.f, open_ms = 180.f;
    for (F32 depth = 0.f; depth <= 1.001f; depth += 0.1f)
    {
        for (F64 t_s = -0.1; t_s < 0.4; t_s += 0.005)
        {
            const F32 closure = ALGazeBlink::blinkClosure(
                t_s, close_ms, hold_ms, open_ms, depth);
            ensure("closure never goes negative", closure >= 0.f);
            ensure("closure never exceeds 1", closure <= 1.0001f);
        }
    }
}

template<> template<>
void algazeblink_test_object::test<9>()
{
    set_test_name("low-fps readability helpers: peak time, window, and total span");
    const F32 close_ms = 85.f, hold_ms = 20.f, open_ms = 165.f;

    ensure("blinkTotalMs sums the three phases",
           std::fabs(ALGazeBlink::blinkTotalMs(close_ms, hold_ms, open_ms) -
                     (close_ms + hold_ms + open_ms)) <= 1e-4f);

    F32 peak_start = -1.f, peak_end = -1.f;
    ALGazeBlink::blinkPeakWindowMs(close_ms, hold_ms, peak_start, peak_end);
    ensure("peak window starts at the close duration",
           std::fabs(peak_start - close_ms) <= 1e-4f);
    ensure("peak window ends at close+hold",
           std::fabs(peak_end - (close_ms + hold_ms)) <= 1e-4f);

    const F32 peak_time = ALGazeBlink::blinkPeakTimeMs(close_ms, hold_ms);
    ensure("peak time sits inside the peak window",
           peak_time >= peak_start - 1e-4f && peak_time <= peak_end + 1e-4f);
    ensure("peak time is the hold midpoint",
           std::fabs(peak_time - (close_ms + 0.5f * hold_ms)) <= 1e-4f);

    // With no hold, the window collapses to the single close/open junction.
    F32 zero_hold_start = -1.f, zero_hold_end = -1.f;
    ALGazeBlink::blinkPeakWindowMs(close_ms, 0.f, zero_hold_start, zero_hold_end);
    ensure("zero-hold peak window collapses to a single instant",
           std::fabs(zero_hold_start - zero_hold_end) <= 1e-4f);
    ensure("zero-hold peak time equals that instant",
           std::fabs(ALGazeBlink::blinkPeakTimeMs(close_ms, 0.f) -
                     zero_hold_start) <= 1e-4f);

    // Sampling exactly at the peak time reaches full depth (this is the
    // frame a scheduler would phase-align a low-fps render to).
    ensure("sampling at the peak time reads as fully closed",
           std::fabs(ALGazeBlink::blinkClosure(
               static_cast<F64>(peak_time) / 1000.0,
               close_ms, hold_ms, open_ms, 1.f) - 1.f) <= 1e-5f);
}

} // namespace tut
