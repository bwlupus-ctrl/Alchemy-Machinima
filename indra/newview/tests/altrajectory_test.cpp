/**
 * @file altrajectory_test.cpp
 * @brief Unit tests for the pure ALTrajectory continuity core.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "../test/lltut.h"
#include "../altrajectory.h"

#include <cmath>
#include <limits>

namespace tut
{
namespace
{
// Independent F64 reference for the quintic Hermite basis of companion
// section 3.3, written in a different (Horner) factoring than the header so
// the two implementations cross-check each other.
struct RefSample
{
    F64 p;
    F64 v;
    F64 a;
};

RefSample refQuintic(F64 t0, F64 T,
                     F64 p0, F64 v0, F64 a0,
                     F64 p1, F64 v1, F64 a1,
                     F64 t)
{
    const F64 u = llclamp((t - t0) / T, 0.0, 1.0);
    const F64 tv0 = T * v0;
    const F64 ta0 = T * T * a0;
    const F64 tv1 = T * v1;
    const F64 ta1 = T * T * a1;

    const F64 u2 = u * u;
    const F64 u3 = u2 * u;

    const F64 h00 = 1.0 + u3 * (-10.0 + u * (15.0 - 6.0 * u));
    const F64 h10 = u + u3 * (-6.0 + u * (8.0 - 3.0 * u));
    const F64 h20 = 0.5 * u2 * (1.0 + u * (-3.0 + u * (3.0 - u)));
    const F64 h01 = u3 * (10.0 + u * (-15.0 + 6.0 * u));
    const F64 h11 = u3 * (-4.0 + u * (7.0 - 3.0 * u));
    const F64 h21 = 0.5 * u3 * (1.0 + u * (-2.0 + u));

    const F64 g00 = u2 * (-30.0 + u * (60.0 - 30.0 * u));
    const F64 g10 = 1.0 + u2 * (-18.0 + u * (32.0 - 15.0 * u));
    const F64 g20 = u * (1.0 + u * (-4.5 + u * (6.0 - 2.5 * u)));
    const F64 g01 = u2 * (30.0 + u * (-60.0 + 30.0 * u));
    const F64 g11 = u2 * (-12.0 + u * (28.0 - 15.0 * u));
    const F64 g21 = u2 * (1.5 + u * (-4.0 + 2.5 * u));

    const F64 q00 = u * (-60.0 + u * (180.0 - 120.0 * u));
    const F64 q10 = u * (-36.0 + u * (96.0 - 60.0 * u));
    const F64 q20 = 1.0 + u * (-9.0 + u * (18.0 - 10.0 * u));
    const F64 q01 = u * (60.0 + u * (-180.0 + 120.0 * u));
    const F64 q11 = u * (-24.0 + u * (84.0 - 60.0 * u));
    const F64 q21 = u * (3.0 + u * (-12.0 + 10.0 * u));

    RefSample out;
    out.p = h00 * p0 + h10 * tv0 + h20 * ta0 +
            h01 * p1 + h11 * tv1 + h21 * ta1;
    out.v = (g00 * p0 + g10 * tv0 + g20 * ta0 +
             g01 * p1 + g11 * tv1 + g21 * ta1) / T;
    out.a = (q00 * p0 + q10 * tv0 + q20 * ta0 +
             q01 * p1 + q11 * tv1 + q21 * ta1) / (T * T);
    return out;
}

F32 programMaxPosition(const ALTrajectory::ScalarProgram& program,
                       F64 start, F64 end, S32 samples)
{
    F32 max_p = -1e30f;
    for (S32 i = 0; i <= samples; ++i)
    {
        const F64 t = start + (end - start) *
            static_cast<F64>(i) / static_cast<F64>(samples);
        max_p = llmax(max_p, ALTrajectory::sample(program, t).mP);
    }
    return max_p;
}

F32 programMinPosition(const ALTrajectory::ScalarProgram& program,
                       F64 start, F64 end, S32 samples)
{
    F32 min_p = 1e30f;
    for (S32 i = 0; i <= samples; ++i)
    {
        const F64 t = start + (end - start) *
            static_cast<F64>(i) / static_cast<F64>(samples);
        min_p = llmin(min_p, ALTrajectory::sample(program, t).mP);
    }
    return min_p;
}
} // anonymous namespace

struct altrajectory_data
{
};

typedef test_group<altrajectory_data> altrajectory_test_group;
typedef altrajectory_test_group::object altrajectory_test_object;
altrajectory_test_group altrajectory_test("altrajectory");

template<> template<>
void altrajectory_test_object::test<1>()
{
    set_test_name("endpoint p/v/a exactness");
    // Dyadic start/duration so the end time hits u == 1 exactly.
    const ALTrajectory::ScalarSegment seg = ALTrajectory::buildSegment(
        2.5, 0.75, 0.3f, -1.2f, 2.0f, -0.9f, 0.7f, -1.5f);

    const ALTrajectory::ScalarSample at_start = ALTrajectory::sample(seg, 2.5);
    ensure("start position is exact", at_start.mP == 0.3f);
    ensure("start velocity is exact", at_start.mV == -1.2f);
    ensure("start acceleration is exact", at_start.mA == 2.0f);

    const ALTrajectory::ScalarSample at_end = ALTrajectory::sample(seg, 3.25);
    ensure("end position is exact", at_end.mP == -0.9f);
    ensure("end velocity is exact", at_end.mV == 0.7f);
    ensure("end acceleration is exact", at_end.mA == -1.5f);

    // Interior samples near (but not at) the endpoints go through the full
    // analytic basis — u is strictly inside (0, 1), so these cannot be
    // satisfied by the early-return endpoint clamps — and must both
    // approach the endpoint state and sit on the independent reference
    // basis.
    const F64 near_times[] = { 2.5 + 1e-5, 3.25 - 1e-5 };
    const F64 expect_p[] = { 0.3, -0.9 };
    const F64 expect_v[] = { -1.2, 0.7 };
    const F64 expect_a[] = { 2.0, -1.5 };
    for (S32 i = 0; i < 2; ++i)
    {
        const ALTrajectory::ScalarSample near_end =
            ALTrajectory::sample(seg, near_times[i]);
        ensure("interior limit approaches the endpoint position",
               std::fabs(near_end.mP - expect_p[i]) <= 1e-3);
        ensure("interior limit approaches the endpoint velocity",
               std::fabs(near_end.mV - expect_v[i]) <= 1e-2);
        ensure("interior limit approaches the endpoint acceleration",
               std::fabs(near_end.mA - expect_a[i]) <= 1e-1);

        const RefSample r = refQuintic(2.5, 0.75, 0.3, -1.2, 2.0,
                                       -0.9, 0.7, -1.5, near_times[i]);
        ensure("near-endpoint position uses the analytic basis",
               std::fabs(near_end.mP - static_cast<F32>(r.p)) <= 5e-4f);
        ensure("near-endpoint velocity uses the analytic derivative",
               std::fabs(near_end.mV - static_cast<F32>(r.v)) <= 5e-3f);
        ensure("near-endpoint acceleration uses the analytic derivative",
               std::fabs(near_end.mA - static_cast<F32>(r.a)) <= 5e-2f);
    }
}

template<> template<>
void altrajectory_test_object::test<2>()
{
    set_test_name("interior samples match the analytic quintic basis");
    constexpr F64 T0 = 1.75;
    constexpr F64 T = 0.9;
    const ALTrajectory::ScalarSegment seg = ALTrajectory::buildSegment(
        T0, T, -0.4f, 0.9f, -1.1f, 1.7f, -0.3f, 0.6f);
    for (S32 i = 1; i < 20; ++i)
    {
        const F64 t = T0 + T * static_cast<F64>(i) / 20.0;
        const ALTrajectory::ScalarSample s = ALTrajectory::sample(seg, t);
        const RefSample r = refQuintic(
            T0, T, -0.4, 0.9, -1.1, 1.7, -0.3, 0.6, t);
        ensure("position matches the reference basis",
               std::fabs(s.mP - static_cast<F32>(r.p)) <= 5e-4f);
        ensure("velocity matches the analytic first derivative",
               std::fabs(s.mV - static_cast<F32>(r.v)) <= 5e-3f);
        ensure("acceleration matches the analytic second derivative",
               std::fabs(s.mA - static_cast<F32>(r.a)) <= 5e-2f);
    }
}

template<> template<>
void altrajectory_test_object::test<3>()
{
    set_test_name("degenerate durations stay finite and sensible");
    const F64 bad_durations[] = {
        0.0, -5.0, std::numeric_limits<F64>::quiet_NaN(),
        std::numeric_limits<F64>::infinity() };
    for (const F64 bad : bad_durations)
    {
        ALTrajectory::ScalarSegment seg = ALTrajectory::buildSegment(
            1.0, bad, 0.2f, 3.0f, -4.0f, 0.8f, 0.5f, 1.5f);
        if (!std::isfinite(bad) && bad > 0.0)
        {
            // +inf clamps to the max duration rather than stepping.
            ensure("infinite duration clamps finite",
                   std::isfinite(seg.mDuration) && seg.mDuration > 0.0);
            continue;
        }
        const ALTrajectory::ScalarSample before =
            ALTrajectory::sample(seg, 0.5);
        const ALTrajectory::ScalarSample after =
            ALTrajectory::sample(seg, 1.0);
        ensure("degenerate segment steps to start before start time",
               before.mP == 0.2f && before.mV == 3.0f && before.mA == -4.0f);
        ensure("degenerate segment steps to end at/after start time",
               after.mP == 0.8f && after.mV == 0.5f && after.mA == 1.5f);
    }

    // Huge duration: build clamps, sampling stays finite everywhere.
    const ALTrajectory::ScalarSegment huge = ALTrajectory::buildSegment(
        0.0, 1.0e12, 0.f, 1.f, 1.f, 10.f, 0.f, 0.f);
    ensure("huge duration is clamped", huge.mDuration <= 1.01e7);
    const ALTrajectory::ScalarSample mid = ALTrajectory::sample(huge, 1.0e6);
    ensure("huge-duration sample is finite",
           std::isfinite(mid.mP) && std::isfinite(mid.mV) &&
           std::isfinite(mid.mA));

    // A raw segment with an unclamped huge duration is sanitized on sample.
    ALTrajectory::ScalarSegment raw;
    raw.mStartTime = 0.0;
    raw.mDuration = 1.0e300;
    raw.mP0 = 0.f; raw.mV0 = 0.f; raw.mA0 = 5.f;
    raw.mP1 = 1.f; raw.mV1 = 0.f; raw.mA1 = 0.f;
    const ALTrajectory::ScalarSample raw_mid = ALTrajectory::sample(raw, 42.0);
    ensure("raw huge duration still samples finite",
           std::isfinite(raw_mid.mP) && std::isfinite(raw_mid.mV) &&
           std::isfinite(raw_mid.mA));

    // Non-finite sample time resolves to the start state.
    const ALTrajectory::ScalarSample nan_time = ALTrajectory::sample(
        huge, std::numeric_limits<F64>::quiet_NaN());
    ensure("NaN time returns the finite start state",
           nan_time.mP == 0.f && nan_time.mV == 1.f && nan_time.mA == 1.f);

    // Non-finite boundary values are sanitized by the scalar solver.
    const ALTrajectory::ScalarProgram cleaned = ALTrajectory::solveScalar(
        0.0, 1.0, std::numeric_limits<F32>::quiet_NaN(), 0.f, 0.f,
        1.f, 0.f, 0.f, false);
    const ALTrajectory::ScalarSample cleaned_mid =
        ALTrajectory::sample(cleaned, 0.5);
    ensure("NaN boundary value is sanitized to a finite curve",
           std::isfinite(cleaned_mid.mP) && std::isfinite(cleaned_mid.mV) &&
           std::isfinite(cleaned_mid.mA));
}

template<> template<>
void altrajectory_test_object::test<4>()
{
    set_test_name("sampling before and after clamps to the endpoints");
    const ALTrajectory::ScalarSegment seg = ALTrajectory::buildSegment(
        10.0, 2.0, -1.f, 0.4f, -0.2f, 3.f, -0.6f, 0.1f);

    const ALTrajectory::ScalarSample way_before =
        ALTrajectory::sample(seg, -500.0);
    ensure("sample before returns exactly (p0, v0, a0)",
           way_before.mP == -1.f && way_before.mV == 0.4f &&
           way_before.mA == -0.2f);

    const ALTrajectory::ScalarSample way_after =
        ALTrajectory::sample(seg, 1.0e300);
    ensure("sample after returns exactly (p1, v1, a1)",
           way_after.mP == 3.f && way_after.mV == -0.6f &&
           way_after.mA == 0.1f);
}

template<> template<>
void altrajectory_test_object::test<5>()
{
    set_test_name("retarget handoff is C0/C1/C2 at 10/50/90 percent");
    constexpr F64 T0 = 0.0;
    constexpr F64 T = 1.0;
    const ALTrajectory::ScalarSegment old_seg = ALTrajectory::buildSegment(
        T0, T, 0.f, 0.f, 0.f, 2.f, 0.f, 0.f);

    const F64 fractions[] = { 0.1, 0.5, 0.9 };
    constexpr F64 H = 1.0 / 512.0;
    for (const F64 f : fractions)
    {
        const F64 now = T0 + f * T;
        const ALTrajectory::ScalarSample handoff =
            ALTrajectory::sample(old_seg, now);
        const ALTrajectory::ScalarSegment new_seg =
            ALTrajectory::retargetSettle(old_seg, now, 0.7, -1.f);

        // Analytic continuity: the new segment starts on the exact old triple.
        const ALTrajectory::ScalarSample new_start =
            ALTrajectory::sample(new_seg, now);
        ensure("C0: position is continuous at the handoff",
               std::fabs(new_start.mP - handoff.mP) <= 1e-5f);
        ensure("C1: velocity is continuous at the handoff",
               std::fabs(new_start.mV - handoff.mV) <= 1e-4f);
        ensure("C2: acceleration is continuous at the handoff",
               std::fabs(new_start.mA - handoff.mA) <= 1e-3f);

        // Finite-difference the composite curve across the boundary.
        auto composite = [&](F64 t) -> F64
        {
            return t < now
                ? static_cast<F64>(ALTrajectory::sample(old_seg, t).mP)
                : static_cast<F64>(ALTrajectory::sample(new_seg, t).mP);
        };
        const F64 v_left  = (composite(now) - composite(now - H)) / H;
        const F64 v_right = (composite(now + H) - composite(now)) / H;
        ensure("finite-difference velocity has no seam",
               std::fabs(v_right - v_left) <= 0.1);

        // Second-order one-sided stencils (truncation O(H^2 p'''') instead
        // of O(H p''') for the plain one-sided second difference) keep the
        // stencil on one side of the seam while letting the tolerance
        // tighten from the old 2.0 to 0.35.
        constexpr F64 HA = 1.0 / 192.0;
        const F64 a_left = (2.0 * composite(now) -
                            5.0 * composite(now - HA) +
                            4.0 * composite(now - 2.0 * HA) -
                            composite(now - 3.0 * HA)) / (HA * HA);
        const F64 a_right = (2.0 * composite(now) -
                             5.0 * composite(now + HA) +
                             4.0 * composite(now + 2.0 * HA) -
                             composite(now + 3.0 * HA)) / (HA * HA);
        // Observed worst-case jump is ~0.12 (FD truncation + F32 sampling
        // noise, not a real seam); 0.35 keeps ~3x margin while being ~6x
        // tighter than the old 2.0.
        const F64 a_jump = std::fabs(a_right - a_left);
        ensure("finite-difference acceleration has no seam", a_jump <= 0.35);

        // Comparative: a naive C1 handoff that drops the sampled acceleration
        // must show a visibly larger acceleration seam where accel matters.
        if (std::fabs(handoff.mA) > 1.f)
        {
            const ALTrajectory::ScalarSegment naive =
                ALTrajectory::buildSegment(now, 0.7,
                                           handoff.mP, handoff.mV, 0.f,
                                           -1.f, 0.f, 0.f);
            auto naive_composite = [&](F64 t) -> F64
            {
                return t < now
                    ? static_cast<F64>(ALTrajectory::sample(old_seg, t).mP)
                    : static_cast<F64>(ALTrajectory::sample(naive, t).mP);
            };
            const F64 na_right = (2.0 * naive_composite(now) -
                                  5.0 * naive_composite(now + HA) +
                                  4.0 * naive_composite(now + 2.0 * HA) -
                                  naive_composite(now + 3.0 * HA)) /
                                 (HA * HA);
            ensure("the C2 handoff beats a naive C1 handoff",
                   a_jump < std::fabs(na_right - a_left));
        }
    }
}

template<> template<>
void altrajectory_test_object::test<6>()
{
    set_test_name("moving-target handoff honors the terminal velocity");
    const ALTrajectory::ScalarSegment old_seg = ALTrajectory::buildSegment(
        0.0, 1.0, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f);
    const ALTrajectory::ScalarSegment chase = ALTrajectory::retarget(
        old_seg, 0.4, 0.6, 2.5f, 2.f, 0.f);
    // Sample past the segment end so the endpoint clamp is exercised
    // regardless of how 0.4 + 0.6 rounds in binary.
    const ALTrajectory::ScalarSample landed =
        ALTrajectory::sample(chase, 1.5);
    ensure("terminal position is the moving target", landed.mP == 2.5f);
    ensure("terminal velocity is the requested v1", landed.mV == 2.f);
    ensure("terminal acceleration is the requested a1", landed.mA == 0.f);
}

template<> template<>
void altrajectory_test_object::test<7>()
{
    set_test_name("shortest arc crosses the +/-pi seam");
    const F32 from = 179.f * DEG_TO_RAD;
    const F32 to = -179.f * DEG_TO_RAD;

    const F32 delta = ALTrajectory::shortestArcDelta(from, to);
    ensure("+179 to -179 is +2 degrees, not -358",
           std::fabs(delta - 2.f * DEG_TO_RAD) <= 1e-4f);
    const F32 back = ALTrajectory::shortestArcDelta(to, from);
    ensure("-179 to +179 is -2 degrees",
           std::fabs(back + 2.f * DEG_TO_RAD) <= 1e-4f);
    ensure("the exact-opposite tie resolves deterministically positive",
           ALTrajectory::shortestArcDelta(0.f, F_PI) > 0.f);

    const ALTrajectory::ScalarProgram program =
        ALTrajectory::solveShortestArc(0.0, 0.4, from, 0.f, 0.f,
                                       to, 0.f, 0.f, true);
    ensure("seam crossing needs no fallback",
           program.mSegmentCount == 1 && !program.mBrakeFallback &&
           program.mLengthenRetries == 0);

    // The whole path stays within the 2-degree short arc (unwrapped).
    for (S32 i = 0; i <= 64; ++i)
    {
        const F64 t = 0.4 * static_cast<F64>(i) / 64.0;
        const F32 p = ALTrajectory::sample(program, t).mP;
        ensure("path travels only the short arc",
               std::fabs(p - from) <= 2.1f * DEG_TO_RAD);
    }
    const F32 wrapped_end = ALTrajectory::wrapToPi(
        ALTrajectory::sample(program, 0.4).mP);
    ensure("re-wrapped landing angle is the target",
           std::fabs(wrapped_end - to) <= 1e-4f);
}

template<> template<>
void altrajectory_test_object::test<8>()
{
    set_test_name("monotonic solve does not overshoot the target");
    // Hostile incoming velocity toward the target: the naive quintic
    // overshoots well past p1 before settling back.
    const ALTrajectory::ScalarSegment naive = ALTrajectory::buildSegment(
        0.0, 1.0, 0.f, 8.f, 0.f, 1.f, 0.f, 0.f);
    F32 naive_max = -1e30f;
    for (S32 i = 0; i <= 128; ++i)
    {
        naive_max = llmax(naive_max,
                          ALTrajectory::sample(naive, i / 128.0).mP);
    }
    ensure("sanity: the naive segment overshoots", naive_max > 1.01f);

    const ALTrajectory::ScalarProgram program = ALTrajectory::solveScalar(
        0.0, 1.0, 0.f, 8.f, 0.f, 1.f, 0.f, 0.f, true);
    const F64 end_time = ALTrajectory::programEndTime(program);
    const F32 max_p = programMaxPosition(program, 0.0, end_time, 256);
    const F32 min_p = programMinPosition(program, 0.0, end_time, 256);
    ensure("monotonic mode never passes the target", max_p <= 1.f + 1e-3f);
    ensure("monotonic mode never backs below the start", min_p >= -1e-3f);

    const ALTrajectory::ScalarSample landed =
        ALTrajectory::sample(program, end_time);
    ensure("monotonic mode still lands exactly on the target",
           landed.mP == 1.f && landed.mV == 0.f && landed.mA == 0.f);
}

template<> template<>
void altrajectory_test_object::test<9>()
{
    set_test_name("non-monotonic mode permits the requested overshoot");
    const ALTrajectory::ScalarProgram program = ALTrajectory::solveScalar(
        0.0, 1.0, 0.f, 8.f, 0.f, 1.f, 0.f, 0.f, false);
    ensure("non-monotonic mode keeps the single requested segment",
           program.mSegmentCount == 1 && !program.mBrakeFallback &&
           program.mLengthenRetries == 0);
    const F32 max_p = programMaxPosition(program, 0.0, 1.0, 256);
    ensure("requested momentum overshoot is preserved", max_p > 1.01f);
}

template<> template<>
void altrajectory_test_object::test<10>()
{
    set_test_name("brake-then-go fallback for away-moving handoff");
    // Incoming velocity points away from the target: a truly monotone path
    // is impossible (C1), so the solver must brake to rest, then go.
    const ALTrajectory::ScalarProgram program = ALTrajectory::solveScalar(
        0.0, 1.0, 0.f, -6.f, 0.f, 1.f, 0.f, 0.f, true);
    ensure("away velocity forces the two-stage fallback",
           program.mBrakeFallback && program.mSegmentCount == 2);

    const F64 end_time = ALTrajectory::programEndTime(program);
    const F32 max_p = programMaxPosition(program, 0.0, end_time, 256);
    ensure("fallback never passes the target", max_p <= 1.f + 1e-3f);

    const ALTrajectory::ScalarSample landed =
        ALTrajectory::sample(program, end_time);
    ensure("fallback lands exactly on the target",
           landed.mP == 1.f && landed.mV == 0.f && landed.mA == 0.f);

    // The two stages must share an exact C2 rest state at their boundary.
    const F64 boundary = program.mSegments[1].mStartTime;
    const ALTrajectory::ScalarSample stage1_end =
        ALTrajectory::sample(program.mSegments[0], boundary);
    const ALTrajectory::ScalarSample stage2_start =
        ALTrajectory::sample(program.mSegments[1], boundary);
    ensure("stage boundary is an exact shared rest state",
           stage1_end.mP == stage2_start.mP &&
           stage1_end.mV == 0.f && stage2_start.mV == 0.f &&
           stage1_end.mA == 0.f && stage2_start.mA == 0.f);
    const F32 just_before =
        ALTrajectory::sample(program, boundary - 1e-4).mP;
    const F32 just_after =
        ALTrajectory::sample(program, boundary + 1e-4).mP;
    ensure("program position is continuous across the stage boundary",
           std::fabs(just_after - just_before) <= 1e-2f);
}

template<> template<>
void altrajectory_test_object::test<11>()
{
    set_test_name("24/30/60/120 fps samples lie on one analytic curve");
    constexpr F64 T0 = 0.25;
    constexpr F64 T = 1.3;
    const ALTrajectory::ScalarSegment seg = ALTrajectory::buildSegment(
        T0, T, -0.4f, 0.9f, -1.1f, 1.7f, -0.3f, 0.6f);

    const F64 rates[] = { 24.0, 30.0, 60.0, 120.0 };
    for (const F64 fps : rates)
    {
        for (S32 k = 0; ; ++k)
        {
            const F64 t = T0 + static_cast<F64>(k) / fps;
            if (t > T0 + T)
            {
                break;
            }
            const ALTrajectory::ScalarSample s = ALTrajectory::sample(seg, t);
            const RefSample r = refQuintic(
                T0, T, -0.4, 0.9, -1.1, 1.7, -0.3, 0.6, t);
            ensure("every frame-rate sample sits on the analytic curve",
                   std::fabs(s.mP - static_cast<F32>(r.p)) <= 1e-3f);
            ensure("every frame-rate velocity sits on the analytic "
                   "first derivative",
                   std::fabs(s.mV - static_cast<F32>(r.v)) <= 5e-3f);
            ensure("every frame-rate acceleration sits on the analytic "
                   "second derivative",
                   std::fabs(s.mA - static_cast<F32>(r.a)) <= 5e-2f);
        }
    }

    // There is no per-step state, only the closed form of absolute time:
    // resampling any time is bit-identical, and the 24 fps step sequence
    // meets the matching 120 fps steps on the same curve. (The two time
    // computations may differ in the last ulp under /fp:fast, so the time
    // comparison allows FP rounding while resampling stays bit-exact.)
    for (S32 k = 0; k <= 31; ++k)
    {
        const F64 t24 = T0 + static_cast<F64>(k) / 24.0;
        const F64 t120 = T0 + static_cast<F64>(5 * k) / 120.0;
        ensure("shared frame times agree to FP rounding",
               std::fabs(t24 - t120) <= 1e-12);
        const ALTrajectory::ScalarSample a = ALTrajectory::sample(seg, t24);
        const ALTrajectory::ScalarSample a_dup = ALTrajectory::sample(seg, t24);
        ensure("resampling the same time is bit-identical",
               a.mP == a_dup.mP && a.mV == a_dup.mV && a.mA == a_dup.mA);
        const ALTrajectory::ScalarSample b = ALTrajectory::sample(seg, t120);
        ensure("shared frame samples lie together on the analytic curve",
               std::fabs(a.mP - b.mP) <= 1e-5f &&
               std::fabs(a.mV - b.mV) <= 1e-4f &&
               std::fabs(a.mA - b.mA) <= 1e-3f);
    }
}

template<> template<>
void altrajectory_test_object::test<12>()
{
    set_test_name("degenerate travel and benign solves stay trivial");
    // Zero travel from rest: every sample is the hold position.
    const ALTrajectory::ScalarProgram hold = ALTrajectory::solveScalar(
        0.0, 0.5, 0.7f, 0.f, 0.f, 0.7f, 0.f, 0.f, true);
    ensure("zero-travel hold needs no feasibility work",
           hold.mSegmentCount == 1 && !hold.mBrakeFallback &&
           hold.mLengthenRetries == 0);
    for (S32 i = 0; i <= 32; ++i)
    {
        const ALTrajectory::ScalarSample s =
            ALTrajectory::sample(hold, 0.5 * i / 32.0);
        ensure("hold position never wanders",
               std::fabs(s.mP - 0.7f) <= 1e-5f);
    }

    // A rest-to-rest move is already monotone: no retries, no fallback.
    const ALTrajectory::ScalarProgram calm = ALTrajectory::solveScalar(
        0.0, 0.5, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, true);
    ensure("rest-to-rest quintic passes feasibility untouched",
           calm.mSegmentCount == 1 && !calm.mBrakeFallback &&
           calm.mLengthenRetries == 0);
    ensure("rest-to-rest quintic is monotone by construction",
           ALTrajectory::checkMonotone(calm.mSegments[0]));
}

template<> template<>
void altrajectory_test_object::test<13>()
{
    set_test_name("brake fallback accounts for a0 and never passes the target");
    // Codex counterexample: p0=0, v0=0, a0=2000, p1=1, T=1, monotonic.
    // The naive quintic reaches ~31.75; the old fallback chose its stop
    // position from v0 alone, so its brake stage reached ~1.95 > p1.
    const ALTrajectory::ScalarSegment naive = ALTrajectory::buildSegment(
        0.0, 1.0, 0.f, 0.f, 2000.f, 1.f, 0.f, 0.f);
    F32 naive_max = -1e30f;
    for (S32 i = 0; i <= 256; ++i)
    {
        naive_max = llmax(naive_max,
                          ALTrajectory::sample(naive, i / 256.0).mP);
    }
    ensure("sanity: the naive segment overshoots badly", naive_max > 1.5f);

    const ALTrajectory::ScalarProgram program = ALTrajectory::solveScalar(
        0.0, 1.0, 0.f, 0.f, 2000.f, 1.f, 0.f, 0.f, true);
    ensure("hostile a0 forces the two-stage fallback",
           program.mBrakeFallback && program.mSegmentCount == 2);

    const F64 end_time = ALTrajectory::programEndTime(program);
    const F32 max_p = programMaxPosition(program, 0.0, end_time, 2048);
    ensure("fallback never passes the target even under raw a0",
           max_p <= 1.f + 1e-3f);
    const F32 min_p = programMinPosition(program, 0.0, end_time, 2048);
    ensure("a0 toward the target causes no wrong-side excursion either",
           min_p >= -1e-3f);

    const ALTrajectory::ScalarSample landed =
        ALTrajectory::sample(program, end_time);
    ensure("fallback still lands exactly on the target",
           landed.mP == 1.f && landed.mV == 0.f && landed.mA == 0.f);

    // Both fallback stages satisfy their own feasibility contracts.
    ensure("the brake stage provably never crosses the target",
           ALTrajectory::checkNeverCrosses(program.mSegments[0], 1.0, 1.0,
                                           1e-3));
    ensure("the go stage passes the same monotone test as the primary path",
           ALTrajectory::checkMonotone(program.mSegments[1]));

    // C2 at the internal boundary: exact shared rest state.
    const F64 boundary = program.mSegments[1].mStartTime;
    const ALTrajectory::ScalarSample s1 =
        ALTrajectory::sample(program.mSegments[0], boundary);
    const ALTrajectory::ScalarSample s2 =
        ALTrajectory::sample(program.mSegments[1], boundary);
    ensure("stage boundary is an exact shared rest state",
           s1.mP == s2.mP && s1.mV == 0.f && s2.mV == 0.f &&
           s1.mA == 0.f && s2.mA == 0.f);
}

template<> template<>
void altrajectory_test_object::test<14>()
{
    set_test_name("min-duration fallback keeps the internal boundary C0");
    // Zero requested duration with an approach fast enough to force the
    // fallback. The old code let the go stage's duration sanitize to a step
    // (t_req - t_brake ~ 0 < MIN_DURATION), so the program jumped from the
    // brake's end value straight to p1 with no continuity at all.
    const ALTrajectory::ScalarProgram program = ALTrajectory::solveScalar(
        0.0, 0.0, 0.f, 4.0e6f, 0.f, 1.f, 0.f, 0.f, true);
    ensure("fast zero-duration solve takes the two-stage fallback",
           program.mBrakeFallback && program.mSegmentCount == 2);
    ensure("neither stage collapses below MIN_DURATION",
           program.mSegments[0].mDuration >= ALTrajectory::MIN_DURATION &&
           program.mSegments[1].mDuration >= ALTrajectory::MIN_DURATION);
    {
        const F64 boundary = program.mSegments[1].mStartTime;
        const ALTrajectory::ScalarSample s1 =
            ALTrajectory::sample(program.mSegments[0], boundary);
        const ALTrajectory::ScalarSample s2 =
            ALTrajectory::sample(program.mSegments[1], boundary);
        ensure("stage values meet exactly at the shared time (C2 here)",
               s1.mP == s2.mP && s1.mV == s2.mV && s1.mA == s2.mA);
        ensure("the program sampler agrees with the shared boundary state",
               ALTrajectory::sample(program, boundary).mP == s2.mP);
    }

    // Codex counterexample: duration = 0 with v0 at the sanitize bound.
    // No C2 brake of >= MIN_DURATION can stay on the near side of p1, so
    // the program must degrade to a position-continuous step to rest —
    // never an internal value jump — and still never pass the target.
    const ALTrajectory::ScalarProgram extreme = ALTrajectory::solveScalar(
        0.0, 0.0, 0.f, 1e12f, 0.f, 1.f, 0.f, 0.f, true);
    ensure("pathological solve still yields the two-stage fallback",
           extreme.mBrakeFallback && extreme.mSegmentCount == 2);

    const F64 boundary = extreme.mSegments[1].mStartTime;
    const ALTrajectory::ScalarSample s1 =
        ALTrajectory::sample(extreme.mSegments[0], boundary);
    const ALTrajectory::ScalarSample s2 =
        ALTrajectory::sample(extreme.mSegments[1], boundary);
    ensure("stage-one end value equals stage-two start value (C0)",
           s1.mP == s2.mP);
    ensure("the program sampler returns the shared value at the boundary",
           ALTrajectory::sample(extreme, boundary).mP == s2.mP);

    const F64 end_time = ALTrajectory::programEndTime(extreme);
    for (S32 i = 0; i <= 256; ++i)
    {
        const F64 t = boundary + (end_time - boundary) *
            static_cast<F64>(i) / 256.0;
        const ALTrajectory::ScalarSample s = ALTrajectory::sample(extreme, t);
        ensure("pathological program samples finite",
               ALTrajectory::isFiniteBits(s.mP) &&
               ALTrajectory::isFiniteBits(s.mV) &&
               ALTrajectory::isFiniteBits(s.mA));
        ensure("pathological program never passes the target",
               s.mP <= 1.f + 1e-3f);
    }
    const ALTrajectory::ScalarSample landed =
        ALTrajectory::sample(extreme, end_time);
    ensure("pathological program still lands exactly on the target",
           landed.mP == 1.f && landed.mV == 0.f && landed.mA == 0.f);
}

template<> template<>
void altrajectory_test_object::test<15>()
{
    set_test_name("analytic monotone test catches a grid-invisible reversal");
    // Codex counterexample: with p0=0, p1=1/12-1e-6, v0=v1=0.25-1e-6,
    // a0=-1, a1=1, T=1 the quintic Hermite reproduces the cubic whose
    // velocity is v(u) = (u-0.5)^2 - 1e-6 — a genuine reversal of depth
    // 1e-6 in a ~2e-3-wide window around u=0.5, which the old fixed grid
    // (u = i/65) straddled without ever sampling. The analytic test finds
    // the velocity quartic's critical point at u=0.5 exactly.
    const F32 v_end = 0.25f - 1e-6f;
    const F32 p_end = 1.f / 12.f - 1e-6f;
    const ALTrajectory::ScalarSegment dip = ALTrajectory::buildSegment(
        0.0, 1.0, 0.f, v_end, -1.f, p_end, v_end, 1.f);
    ensure("strict analytic test detects the hidden reversal",
           !ALTrajectory::checkMonotone(dip, 1e-3f, 0.f));

    // Control: raising the parabola to v(u) = (u-0.5)^2 + 1e-6 removes the
    // reversal, and the same strict test must accept it — proving the
    // detection is the genuine root analysis, not a blanket rejection.
    const F32 v_end_up = 0.25f + 1e-6f;
    const F32 p_end_up = 1.f / 12.f + 1e-6f;
    const ALTrajectory::ScalarSegment graze = ALTrajectory::buildSegment(
        0.0, 1.0, 0.f, v_end_up, -1.f, p_end_up, v_end_up, 1.f);
    ensure("strict analytic test accepts the reversal-free twin",
           ALTrajectory::checkMonotone(graze, 1e-3f, 0.f));
}

template<> template<>
void altrajectory_test_object::test<16>()
{
    set_test_name("non-finite boundary values sanitize on every API path");
    const F32 bads[] = { std::numeric_limits<F32>::quiet_NaN(),
                         std::numeric_limits<F32>::infinity(),
                         -std::numeric_limits<F32>::infinity(),
                         3.0e38f }; // oversized-but-finite must clamp too
    const F64 times[] = { -1.0, 1.0, 1.25, 1.5, 2.0, 3.5, 1.0e300 };
    for (S32 field = 0; field < 6; ++field)
    {
        for (const F32 bad : bads)
        {
            F32 v[6] = { 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f };
            v[field] = bad;

            // buildSegment sanitizes at construction...
            const ALTrajectory::ScalarSegment built =
                ALTrajectory::buildSegment(1.0, 1.0, v[0], v[1], v[2],
                                           v[3], v[4], v[5]);
            // ...and a raw segment that bypassed it sanitizes on sampling.
            ALTrajectory::ScalarSegment raw;
            raw.mStartTime = 1.0;
            raw.mDuration = 1.0;
            raw.mP0 = v[0]; raw.mV0 = v[1]; raw.mA0 = v[2];
            raw.mP1 = v[3]; raw.mV1 = v[4]; raw.mA1 = v[5];

            // retarget/retargetSettle from the poisoned segment, toward a
            // poisoned endpoint, at a non-finite handoff time.
            const ALTrajectory::ScalarSegment rt = ALTrajectory::retarget(
                raw, std::numeric_limits<F64>::quiet_NaN(), 0.5,
                bad, bad, bad);
            const ALTrajectory::ScalarSegment rs =
                ALTrajectory::retargetSettle(raw, 1.5, 0.5, bad);

            const ALTrajectory::ScalarSegment* segs[] =
                { &built, &raw, &rt, &rs };
            for (const ALTrajectory::ScalarSegment* seg : segs)
            {
                for (const F64 t : times)
                {
                    const ALTrajectory::ScalarSample s =
                        ALTrajectory::sample(*seg, t);
                    ensure("every path samples finite for any single "
                           "poisoned boundary field",
                           ALTrajectory::isFiniteBits(s.mP) &&
                           ALTrajectory::isFiniteBits(s.mV) &&
                           ALTrajectory::isFiniteBits(s.mA));
                }
            }
        }
    }
}

template<> template<>
void altrajectory_test_object::test<17>()
{
    set_test_name("+inf duration is sampled finite, not just constructed");
    const F64 inf = std::numeric_limits<F64>::infinity();
    const ALTrajectory::ScalarSegment seg = ALTrajectory::buildSegment(
        5.0, inf, 0.f, 1.f, -0.5f, 10.f, 0.f, 0.f);
    ensure("+inf duration clamps to MAX_DURATION at construction",
           seg.mDuration == ALTrajectory::MAX_DURATION);

    ALTrajectory::ScalarSegment raw;
    raw.mStartTime = 5.0;
    raw.mDuration = inf;
    raw.mP0 = 0.f; raw.mV0 = 1.f; raw.mA0 = -0.5f;
    raw.mP1 = 10.f; raw.mV1 = 0.f; raw.mA1 = 0.f;

    const F64 times[] = { 4.0, 5.0, 6.0, 5.0 + 1.0e3, 5.0 + 1.0e6,
                          5.0 + ALTrajectory::MAX_DURATION, 1.0e300 };
    for (const F64 t : times)
    {
        const ALTrajectory::ScalarSample sb = ALTrajectory::sample(seg, t);
        const ALTrajectory::ScalarSample sr = ALTrajectory::sample(raw, t);
        ensure("clamped +inf duration samples finite",
               ALTrajectory::isFiniteBits(sb.mP) &&
               ALTrajectory::isFiniteBits(sb.mV) &&
               ALTrajectory::isFiniteBits(sb.mA));
        ensure("raw +inf duration also samples finite",
               ALTrajectory::isFiniteBits(sr.mP) &&
               ALTrajectory::isFiniteBits(sr.mV) &&
               ALTrajectory::isFiniteBits(sr.mA));
    }
    const ALTrajectory::ScalarSample before = ALTrajectory::sample(seg, 4.0);
    ensure("before the segment the start state is exact",
           before.mP == 0.f && before.mV == 1.f && before.mA == -0.5f);
    const ALTrajectory::ScalarSample after =
        ALTrajectory::sample(seg, 1.0e300);
    ensure("past the clamped duration the end state is exact",
           after.mP == 10.f && after.mV == 0.f && after.mA == 0.f);
}

template<> template<>
void altrajectory_test_object::test<18>()
{
    set_test_name("program end time respects a large nonzero start time");
    const F64 start = 1.0e6;
    const ALTrajectory::ScalarProgram calm = ALTrajectory::solveScalar(
        start, 1.0, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, true);
    ensure("calm solve stays single-segment", calm.mSegmentCount == 1);
    const F64 calm_end = ALTrajectory::programEndTime(calm);
    ensure("end time is start + duration, not just duration",
           std::fabs(calm_end - (start + 1.0)) <= 1e-6);
    const ALTrajectory::ScalarSample calm_landed =
        ALTrajectory::sample(calm, calm_end);
    ensure("sampling the reported end time lands on the target",
           calm_landed.mP == 1.f && calm_landed.mV == 0.f &&
           calm_landed.mA == 0.f);

    const ALTrajectory::ScalarProgram braked = ALTrajectory::solveScalar(
        start, 1.0, 0.f, -6.f, 0.f, 1.f, 0.f, 0.f, true);
    ensure("away velocity still takes the fallback at a large start time",
           braked.mBrakeFallback && braked.mSegmentCount == 2);
    const F64 end = ALTrajectory::programEndTime(braked);
    ensure("fallback end time is stage-two start + stage-two duration",
           end == braked.mSegments[1].mStartTime +
                  braked.mSegments[1].mDuration);
    ensure("fallback end time sits inside (start, start + T]",
           end > start && end <= start + 1.0 + 1e-9);
    const ALTrajectory::ScalarSample landed =
        ALTrajectory::sample(braked, end);
    ensure("fallback lands on the target at its reported end time",
           landed.mP == 1.f && landed.mV == 0.f && landed.mA == 0.f);
}

template<> template<>
void altrajectory_test_object::test<19>()
{
    set_test_name("tiny remaining travel settles C1 with bounded overshoot");
    // Codex counterexample: ordinary gaze values, NOT near the sanitize
    // bound. p0 = 0, v0 = 1 toward a target only 1e-9 away: no capped C2
    // brake fits inside the crossing tolerance, and the old code degraded
    // to a C0 step whose velocity jumped 1 -> 0. The fix must instead emit
    // a real (>= MIN_DURATION) smooth settle that may overshoot p1 by at
    // most SETTLE_OVERSHOOT_TOL.
    const F32 target = 1e-9f;
    const ALTrajectory::ScalarProgram program = ALTrajectory::solveScalar(
        0.0, 0.2, 0.f, 1.f, 0.f, target, 0.f, 0.f, true);
    ensure("tiny-travel solve takes the two-stage fallback",
           program.mBrakeFallback && program.mSegmentCount == 2);

    // The brake is a genuine segment, not the C0 velocity-step escape.
    ensure("the brake stage did not degenerate to a step",
           program.mSegments[0].mDuration >= ALTrajectory::MIN_DURATION);

    // Velocity is continuous at the program start: the incoming v0 = 1 is
    // reproduced exactly (the old C0 step returned v = 0 here).
    const ALTrajectory::ScalarSample s0 = ALTrajectory::sample(program, 0.0);
    ensure("no velocity discontinuity at the program start",
           s0.mP == 0.f && s0.mV == 1.f && s0.mA == 0.f);

    // Exact shared C2 rest state at the internal boundary.
    const F64 boundary = program.mSegments[1].mStartTime;
    const ALTrajectory::ScalarSample s1 =
        ALTrajectory::sample(program.mSegments[0], boundary);
    const ALTrajectory::ScalarSample s2 =
        ALTrajectory::sample(program.mSegments[1], boundary);
    ensure("stage boundary is an exact shared rest state",
           s1.mP == s2.mP && s1.mV == 0.f && s2.mV == 0.f &&
           s1.mA == 0.f && s2.mA == 0.f);

    // No perceptible velocity discontinuity anywhere: the brake's velocity
    // ramps 1 -> 0 smoothly, so adjacent samples on a grid fine relative to
    // the brake duration can only move in small steps.
    F32 prev_v = s0.mV;
    for (S32 i = 1; i <= 512; ++i)
    {
        const F64 t = boundary * static_cast<F64>(i) / 512.0;
        const F32 v = ALTrajectory::sample(program, t).mV;
        ensure("brake velocity ramps smoothly (no jump between samples)",
               std::fabs(v - prev_v) <= 0.05f);
        prev_v = v;
    }
    ensure("brake velocity reaches rest smoothly", std::fabs(prev_v) <= 0.05f);

    // Overshoot is bounded by the documented tolerance (dense over the
    // brake, where the excursion peaks, plus the whole program).
    const F64 end_time = ALTrajectory::programEndTime(program);
    const F32 max_brake = programMaxPosition(program, 0.0, boundary, 512);
    const F32 max_all   = programMaxPosition(program, 0.0, end_time, 512);
    const F32 bound = target +
        static_cast<F32>(ALTrajectory::SETTLE_OVERSHOOT_TOL) * 1.05f;
    ensure("overshoot beyond the target is tiny and bounded",
           max_brake <= bound && max_all <= bound);

    // The go stage eases back to the target from rest, monotone.
    ensure("go stage passes the monotone feasibility test",
           ALTrajectory::checkMonotone(program.mSegments[1]));
    const ALTrajectory::ScalarSample landed =
        ALTrajectory::sample(program, end_time + 1.0);
    ensure("program still lands exactly on the target at rest",
           landed.mP == target && landed.mV == 0.f && landed.mA == 0.f);
}

template<> template<>
void altrajectory_test_object::test<20>()
{
    set_test_name("even-multiplicity velocity root cannot break monotonicity");
    // Codex counterexample: c(u) = 121 - 330u + 225u^2 = (15u - 11)^2 has
    // an exact double root at u = 11/15, but its Horner evaluation there is
    // ~1.4e-14, not 0, so polyRootsInUnit does not isolate it. Its contract
    // is sign-changing completeness: this polynomial never changes sign, so
    // no sign-changing root may be reported...
    const F64 c[5] = { 121.0, -330.0, 225.0, 0.0, 0.0 };
    for (S32 i = 0; i <= 64; ++i)
    {
        const F64 u = static_cast<F64>(i) / 64.0;
        ensure("the double-root polynomial never goes negative",
               ALTrajectory::detail::evalPoly(c, 4, u) >= 0.0);
    }
    F64 roots[4];
    const S32 nroots = ALTrajectory::detail::polyRootsInUnit(c, 4, roots);
    for (S32 i = 0; i < nroots; ++i)
    {
        ensure("anything reported can only be the tangency itself",
               std::fabs(roots[i] - 11.0 / 15.0) <= 1e-9);
    }
    ensure("no sign-changing root exists to isolate", nroots == 0);

    // ...and the segment whose scaled velocity IS that polynomial must
    // still be decided monotone: V(u) = (15u - 11)^2 >= 0 everywhere.
    // Position is its exact integral: p(u) = 121u - 165u^2 + 75u^3, so
    // p1 = 31, v1 = V(1) = 16, a0 = V'(0) = -330, a1 = V'(1) = 120 (T = 1).
    const ALTrajectory::ScalarSegment tangent = ALTrajectory::buildSegment(
        0.0, 1.0, 0.f, 121.f, -330.f, 31.f, 16.f, 120.f);
    ensure("velocity tangency (no sign change) is still monotone",
           ALTrajectory::checkMonotone(tangent));
    const ALTrajectory::ScalarProgram program = ALTrajectory::solveScalar(
        0.0, 1.0, 0.f, 121.f, -330.f, 31.f, 16.f, 120.f, true);
    ensure("the monotonic solver accepts it without fallback or retries",
           program.mSegmentCount == 1 && !program.mBrakeFallback &&
           program.mLengthenRetries == 0);
}

template<> template<>
void altrajectory_test_object::test<21>()
{
    set_test_name("raw non-finite start times sanitize on every program path");
    const F64 bad_starts[] = { std::numeric_limits<F64>::quiet_NaN(),
                               std::numeric_limits<F64>::infinity(),
                               -std::numeric_limits<F64>::infinity() };
    for (const F64 bad : bad_starts)
    {
        // Raw degenerate (step) segment: the step must key off the
        // SANITIZED start (0.0), not compare a time against NaN/Inf.
        ALTrajectory::ScalarProgram prog;
        prog.mSegments[0].mStartTime = bad;
        prog.mSegments[0].mDuration  = 0.0;
        prog.mSegments[0].mP0 = 0.25f; prog.mSegments[0].mV0 = 1.f;
        prog.mSegments[0].mA0 = 0.f;
        prog.mSegments[0].mP1 = 0.75f; prog.mSegments[0].mV1 = 0.f;
        prog.mSegments[0].mA1 = 0.f;

        const ALTrajectory::ScalarSample before =
            ALTrajectory::sample(prog, -1.0);
        const ALTrajectory::ScalarSample after =
            ALTrajectory::sample(prog, 1.0);
        ensure("raw step keys off the sanitized start: before -> start",
               before.mP == 0.25f && before.mV == 1.f);
        ensure("raw step keys off the sanitized start: after -> end",
               after.mP == 0.75f && after.mV == 0.f);
        ensure("raw step program samples finite (bit-checked)",
               ALTrajectory::isFiniteBits(before.mP) &&
               ALTrajectory::isFiniteBits(before.mV) &&
               ALTrajectory::isFiniteBits(before.mA) &&
               ALTrajectory::isFiniteBits(after.mP) &&
               ALTrajectory::isFiniteBits(after.mV) &&
               ALTrajectory::isFiniteBits(after.mA));

        // Raw positive-duration segment sampled directly at absolute times.
        ALTrajectory::ScalarSegment seg;
        seg.mStartTime = bad;
        seg.mDuration  = 1.0;
        seg.mP0 = 0.f; seg.mV0 = 0.f; seg.mA0 = 0.f;
        seg.mP1 = 1.f; seg.mV1 = 0.f; seg.mA1 = 0.f;
        const F64 seg_times[] = { -0.5, 0.0, 0.5, 1.0, 2.0 };
        for (const F64 t : seg_times)
        {
            const ALTrajectory::ScalarSample s = ALTrajectory::sample(seg, t);
            ensure("raw non-finite mStartTime samples finite",
                   ALTrajectory::isFiniteBits(s.mP) &&
                   ALTrajectory::isFiniteBits(s.mV) &&
                   ALTrajectory::isFiniteBits(s.mA));
        }
        ensure("raw segment endpoints follow the sanitized start",
               ALTrajectory::sample(seg, -0.5).mP == 0.f &&
               ALTrajectory::sample(seg, 2.0).mP == 1.f);

        // Two-stage raw program with poisoned stage-two start and offsets.
        prog.mSegmentCount = 2;
        prog.mSegments[1] = seg;
        prog.mStartTime   = bad;
        prog.mStageOffset = bad;
        prog.mEndOffset   = bad;
        const F64 prog_times[] = { -1.0, 0.0, 0.5, 2.0, 1.0e300 };
        for (const F64 t : prog_times)
        {
            const ALTrajectory::ScalarSample s =
                ALTrajectory::sample(prog, t);
            ensure("poisoned two-stage program samples finite",
                   ALTrajectory::isFiniteBits(s.mP) &&
                   ALTrajectory::isFiniteBits(s.mV) &&
                   ALTrajectory::isFiniteBits(s.mA));
        }

        // programEndTime with poisoned epoch/offset stays finite.
        ensure("programEndTime is finite for poisoned program fields",
               ALTrajectory::isFiniteBits(
                   ALTrajectory::programEndTime(prog)));
    }
}

template<> template<>
void altrajectory_test_object::test<22>()
{
    set_test_name("start at 2^53 keeps the fallback seam and end time");
    // Codex counterexample: at start = 2^53 the F64 ulp is 2 seconds, so
    // start + t_brake == start and the old absolute-time stage routing put
    // the go stage on top of the brake — sampling AT the start returned the
    // stage-two rest state (C0/C1 lost), and programEndTime collapsed onto
    // the start.
    const F64 start = 9007199254740992.0; // 2^53
    const ALTrajectory::ScalarProgram program = ALTrajectory::solveScalar(
        start, 1.0, 0.f, -6.f, 0.f, 1.f, 0.f, 0.f, true);
    ensure("away velocity takes the two-stage fallback at 2^53",
           program.mBrakeFallback && program.mSegmentCount == 2);
    ensure("stage offsets are stored relative and did not collapse",
           program.mStartTime == start &&
           program.mStageOffset > 0.0 &&
           program.mEndOffset >= program.mStageOffset +
               program.mSegments[1].mDuration);

    // C0/C1/C2 at the program start: sampling AT the start returns the
    // exact incoming state, not stage two's rest state.
    const ALTrajectory::ScalarSample s0 =
        ALTrajectory::sample(program, start);
    ensure("sampling at the start returns the exact incoming state",
           s0.mP == 0.f && s0.mV == -6.f && s0.mA == 0.f);
    const ALTrajectory::ScalarSample sb =
        ALTrajectory::sample(program, start - 2.0);
    ensure("sampling before the start clamps to the incoming state",
           sb.mP == 0.f && sb.mV == -6.f && sb.mA == 0.f);

    // C0 (indeed C2) at the internal seam, checked in the program-relative
    // frame where the boundary is exactly representable.
    ensure("the stages share the exact seam state by construction",
           program.mSegments[0].mP1 == program.mSegments[1].mP0 &&
           program.mSegments[0].mV1 == 0.f &&
           program.mSegments[0].mA1 == 0.f &&
           program.mSegments[1].mV0 == 0.f &&
           program.mSegments[1].mA0 == 0.f);
    const ALTrajectory::ScalarSample seam1 = ALTrajectory::sampleOffset(
        program.mSegments[0], program.mStageOffset);
    const ALTrajectory::ScalarSample seam2 = ALTrajectory::sampleOffset(
        program.mSegments[1], 0.0);
    ensure("relative sampling shows an exact C2 seam",
           seam1.mP == seam2.mP && seam1.mV == 0.f && seam2.mV == 0.f &&
           seam1.mA == 0.f && seam2.mA == 0.f);

    // programEndTime did not collapse onto the start: it is the next
    // representable time at/after start + 1, and sampling there has landed.
    const F64 end = ALTrajectory::programEndTime(program);
    ensure("end time is strictly after the start", end > start);
    ensure("end time covers the full program duration",
           end - start >= 1.0 && end - start <= 4.0);
    const ALTrajectory::ScalarSample landed =
        ALTrajectory::sample(program, end);
    ensure("sampling the reported end time lands exactly on the target",
           landed.mP == 1.f && landed.mV == 0.f && landed.mA == 0.f);

    // The single-segment path is robust at 2^53 as well.
    const ALTrajectory::ScalarProgram calm = ALTrajectory::solveScalar(
        start, 1.0, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, true);
    ensure("calm solve stays single-segment at 2^53",
           calm.mSegmentCount == 1);
    const F64 calm_end = ALTrajectory::programEndTime(calm);
    ensure("calm end time is after the start and covers the duration",
           calm_end > start && calm_end - start >= 1.0);
    const ALTrajectory::ScalarSample calm_landed =
        ALTrajectory::sample(calm, calm_end);
    ensure("calm program lands exactly at its reported end time",
           calm_landed.mP == 1.f && calm_landed.mV == 0.f &&
           calm_landed.mA == 0.f);
    const ALTrajectory::ScalarSample calm_start =
        ALTrajectory::sample(calm, start);
    ensure("calm program starts exactly at rest on p0",
           calm_start.mP == 0.f && calm_start.mV == 0.f &&
           calm_start.mA == 0.f);
}

template<> template<>
void altrajectory_test_object::test<23>()
{
    set_test_name("sampling exactly at programEndTime lands bit-exact");
    // Codex counterexample: mEndOffset = fl(t_brake + t_go) can round below
    // t_brake + t_go, so an end time checked only against mEndOffset leaves
    // stage two at u < 1 and the exact-endpoint clamp in the sampler is
    // bypassed. programEndTime must instead guarantee the final stage's
    // LOCAL offset reaches that segment's full duration.
    struct Case
    {
        const char* name;
        F64 start, duration;
        F32 p0, v0, a0, p1, v1, a1;
        S32 expected_segments; // 0 = don't care
    };
    const Case cases[] = {
        // (a) the endpoint-rounding counterexample (fast approach, moving
        //     endpoint): final go duration's sum with t_brake rounds short.
        { "counterexample 0.1s epoch",
          0.1, 0.5, 0.f, 4.2f, 0.f, 1.f, -1.f, 0.f, 2 },
        // (b) an ordinary small-start two-stage fallback (away velocity,
        //     settling endpoint).
        { "normal two-stage fallback",
          2.0, 1.0, 0.f, -6.f, 0.f, 1.f, 0.f, 0.f, 2 },
        // (c) a single-segment program with non-dyadic epoch/duration.
        { "single-segment program",
          0.3, 0.7, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 1 },
        // (d) the large-epoch case: start = 2^53, where the F64 ulp is 2 s.
        { "2^53 epoch fallback",
          9007199254740992.0, 1.0, 0.f, -6.f, 0.f, 1.f, 0.f, 0.f, 2 },
    };

    for (const Case& c : cases)
    {
        const ALTrajectory::ScalarProgram program = ALTrajectory::solveScalar(
            c.start, c.duration, c.p0, c.v0, c.a0, c.p1, c.v1, c.a1, true);
        if (c.expected_segments != 0)
        {
            ensure(std::string(c.name) + ": expected segment count",
                   program.mSegmentCount == c.expected_segments);
        }

        // The landed terminal state is the final segment's u == 1 endpoint.
        const ALTrajectory::ScalarSegment& last =
            program.mSegments[program.mSegmentCount - 1];
        const ALTrajectory::ScalarSample want = ALTrajectory::endSample(last);

        const F64 end = ALTrajectory::programEndTime(program);
        ensure(std::string(c.name) + ": end time is at/after the start",
               end >= program.mStartTime);
        const ALTrajectory::ScalarSample got =
            ALTrajectory::sample(program, end);
        ensure(std::string(c.name) +
                   ": sampling exactly at programEndTime is bit-exact",
               got.mP == want.mP && got.mV == want.mV && got.mA == want.mA);
    }
}

} // namespace tut
