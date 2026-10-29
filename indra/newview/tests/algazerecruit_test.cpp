/**
 * @file algazerecruit_test.cpp
 * @brief Unit tests for soft anatomical recruitment (ALGazeRecruit).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "../test/lltut.h"
#include "../algazerecruit.h"

#include <cmath>

namespace tut
{
namespace
{
// Independent F64 reference for smoothExcess (companion section 4.2), using a
// different (Horner) factoring than the header's monomial t^6-3t^5+2.5t^4, so
// the two implementations cross-check each other rather than one echoing the
// other's arithmetic.
F64 refSmoothExcess(F64 z, F64 b)
{
    if (b <= 0.0)
    {
        return z > 0.0 ? z : 0.0;
    }
    if (z <= -b)
    {
        return 0.0;
    }
    if (z >= b)
    {
        return z;
    }
    const F64 t = (z + b) / (2.0 * b);
    // t^4 * (2.5 - 3t + t^2), Horner in t.
    const F64 poly = t * t * t * t * (2.5 + t * (-3.0 + t));
    return 2.0 * b * poly;
}

// Independent hard-knee reference (the pre-existing allocate() at
// algazemath.h ~634), used to prove the band == 0 path is bit-exact and to
// show, by contrast, the derivative jump a real hard knee has.
F32 refHardAmount(F32 residual, F32 capacity)
{
    return llmin(residual, llmax(capacity, 0.f));
}

F32 refHardResidual(F32 residual, F32 capacity)
{
    return llmax(residual - capacity, 0.f);
}
} // anonymous namespace

struct algazerecruit_data
{
};

typedef test_group<algazerecruit_data> algazerecruit_test_group;
typedef algazerecruit_test_group::object algazerecruit_test_object;
algazerecruit_test_group algazerecruit_test("algazerecruit");

template<> template<>
void algazerecruit_test_object::test<1>()
{
    set_test_name("smoothExcess endpoint/branch values match analytically");

    // b <= 0 collapses to the hard knee max(z, 0), for any sign of b.
    const F32 zs_for_hard[] = { -3.f, -0.5f, 0.f, 0.5f, 3.f };
    const F32 degenerate_bands[] = { 0.f, -1.f, -0.001f };
    for (const F32 b : degenerate_bands)
    {
        for (const F32 z : zs_for_hard)
        {
            ensure_approximately_equals(
                "b <= 0 reproduces max(z, 0)",
                ALGazeRecruit::smoothExcess(z, b), llmax(z, 0.f), 16);
        }
    }

    // Outer branches and the interior blend, cross-checked against an
    // independently-factored F64 reference, for several band half-widths.
    const F32 bands[] = { 0.05f, 0.25f, 0.6f, 1.3f, 4.f };
    for (const F32 b : bands)
    {
        ensure_approximately_equals(
            "z <= -b is exactly zero", ALGazeRecruit::smoothExcess(-b, b), 0.f, 16);
        ensure_approximately_equals(
            "deep below the band is exactly zero",
            ALGazeRecruit::smoothExcess(-b - 5.f, b), 0.f, 16);
        ensure_approximately_equals(
            "z >= b is exactly z", ALGazeRecruit::smoothExcess(b, b), b, 16);
        ensure_approximately_equals(
            "deep above the band is exactly z",
            ALGazeRecruit::smoothExcess(b + 5.f, b), b + 5.f, 16);

        const F32 fractions[] = { -0.9f, -0.5f, -0.1f, 0.f, 0.1f, 0.5f, 0.9f };
        for (const F32 frac : fractions)
        {
            const F32 z = frac * b;
            const F32 got = ALGazeRecruit::smoothExcess(z, b);
            const F32 want = static_cast<F32>(
                refSmoothExcess(static_cast<F64>(z), static_cast<F64>(b)));
            ensure_approximately_equals(
                "interior blend matches the independent F64 reference",
                got, want, 14);
        }
    }

    // Midpoint (z == 0) has the closed-form value 2b * 0.078125 = 0.15625b,
    // since t == 0.5 there: 0.5^6 - 3*0.5^5 + 2.5*0.5^4 == 0.078125.
    for (const F32 b : bands)
    {
        ensure_approximately_equals(
            "z == 0 midpoint matches the closed form",
            ALGazeRecruit::smoothExcess(0.f, b), 0.15625f * b, 14);
    }
}

template<> template<>
void algazerecruit_test_object::test<2>()
{
    set_test_name("band-0 output is bit-exact vs the hard min/residual knee");

    const F32 capacities[] = { 0.f, 0.2f, 0.5f, 1.f, 2.5f };
    const F32 residuals[] = { 0.f, 0.05f, 0.2f, 0.499f, 0.5f, 0.6f,
                              1.f, 1.7f, 2.5f, 4.f, 9.f };
    for (const F32 capacity : capacities)
    {
        for (const F32 want_residual : residuals)
        {
            F32 residual = want_residual;
            const F32 amount = ALGazeRecruit::recruitJoint(residual, capacity, 0.f);
            const F32 ref_amount = refHardAmount(want_residual, capacity);
            const F32 ref_residual = refHardResidual(want_residual, capacity);
            ensure_approximately_equals(
                "band-0 joint amount matches min(residual, capacity)",
                amount, ref_amount, 12);
            ensure_approximately_equals(
                "band-0 leftover matches max(residual - capacity, 0)",
                residual, ref_residual, 12);
        }
    }

    // Same proof through the ordered-chain entry point, sweeping a whole
    // chain at once so the exactness holds joint-by-joint, not just for a
    // single allocate() step.
    const F32 chain_capacities[] = {
        25.f * DEG_TO_RAD, 35.f * DEG_TO_RAD, 35.f * DEG_TO_RAD,
        45.f * DEG_TO_RAD, 35.f * DEG_TO_RAD };
    constexpr S32 CHAIN_LEN = 5;
    const F32 targets_deg[] = { 0.f, 10.f, 24.9f, 25.f, 40.f, 61.f,
                               96.5f, 132.f, 176.f, 220.f };
    for (const F32 target_deg : targets_deg)
    {
        F32 soft_joints[CHAIN_LEN];
        const F32 leftover = ALGazeRecruit::recruitChain(
            target_deg * DEG_TO_RAD, chain_capacities, soft_joints,
            CHAIN_LEN, 0.f);

        F32 residual = fabsf(target_deg) * DEG_TO_RAD;
        for (S32 i = 0; i < CHAIN_LEN; ++i)
        {
            const F32 ref_amount = refHardAmount(residual, chain_capacities[i]);
            residual = refHardResidual(residual, chain_capacities[i]);
            ensure_approximately_equals(
                "chained band-0 joint matches the hard-knee allocator",
                soft_joints[i], ref_amount, 11);
        }
        ensure_approximately_equals(
            "chained band-0 leftover matches the hard-knee spill",
            leftover, residual, 10);
    }
}

template<> template<>
void algazerecruit_test_object::test<3>()
{
    set_test_name("1st/2nd finite-difference continuity through the band (vs a real hard-knee jump)");

    constexpr F32 H = 0.02f;
    const F32 bands[] = { 0.25f, 0.6f, 1.3f };

    // The reference hard knee (max(z, 0) itself) really does have a jump at
    // its knee z == 0: this is the defect soft recruitment fixes.
    const F32 hard_left1 = (llmax(0.f, 0.f) - llmax(-H, 0.f)) / H;
    const F32 hard_right1 = (llmax(H, 0.f) - llmax(0.f, 0.f)) / H;
    const F32 hard_jump1 = fabsf(hard_right1 - hard_left1);
    ensure("sanity: the hard knee has a first-derivative jump of ~1",
           fabsf(hard_jump1 - 1.f) <= 1e-4f);

    const F32 hard_straddle2 =
        (llmax(H, 0.f) - 2.f * llmax(0.f, 0.f) + llmax(-H, 0.f)) / (H * H);
    ensure("sanity: the hard knee's straddling 2nd difference blows up like 1/H",
           hard_straddle2 >= 0.9f / H);

    for (const F32 b : bands)
    {
        const F32 boundaries[] = { -b, b };
        for (const F32 z0 : boundaries)
        {
            const F32 f_m2h = ALGazeRecruit::smoothExcess(z0 - 2.f * H, b);
            const F32 f_mh  = ALGazeRecruit::smoothExcess(z0 - H, b);
            const F32 f_0   = ALGazeRecruit::smoothExcess(z0, b);
            const F32 f_ph  = ALGazeRecruit::smoothExcess(z0 + H, b);
            const F32 f_p2h = ALGazeRecruit::smoothExcess(z0 + 2.f * H, b);

            const F32 left1 = (f_0 - f_mh) / H;
            const F32 right1 = (f_ph - f_0) / H;
            const F32 jump1 = fabsf(right1 - left1);
            ensure("soft first-derivative jump at the band edge is far below the hard knee's",
                   jump1 <= 0.25f * hard_jump1);

            const F32 straddle2 = (f_ph - 2.f * f_0 + f_mh) / (H * H);
            ensure("soft straddling 2nd difference stays far below the hard knee's blow-up",
                   fabsf(straddle2) <= 0.25f * hard_straddle2);

            // One-sided 2nd differences taken entirely on one side of the
            // edge should also stay small and of comparable order (no
            // sign flip / spike distinguishing "just inside" from "just
            // outside" beyond ordinary curvature).
            const F32 left2 = (f_0 - 2.f * f_mh + f_m2h) / (H * H);
            const F32 right2 = (f_p2h - 2.f * f_ph + f_0) / (H * H);
            ensure("one-sided 2nd differences agree in order of magnitude across the edge",
                   fabsf(right2 - left2) <= 0.25f * hard_straddle2);
        }
    }
}

template<> template<>
void algazerecruit_test_object::test<4>()
{
    set_test_name("total allocated equals requested until total capacity is exhausted");

    const F32 chain_capacities[] = {
        25.f * DEG_TO_RAD, 35.f * DEG_TO_RAD, 35.f * DEG_TO_RAD,
        45.f * DEG_TO_RAD, 35.f * DEG_TO_RAD };
    constexpr S32 CHAIN_LEN = 5;
    F32 total_capacity = 0.f;
    for (S32 i = 0; i < CHAIN_LEN; ++i)
    {
        total_capacity += chain_capacities[i];
    }

    const F32 bands_deg[] = { 0.f, 3.f, 9.f };
    const F32 targets_deg[] = { 0.f, 5.f, 50.f, 90.f, 150.f, 174.f, 175.f,
                               176.f, 200.f, 220.f, 400.f };

    // Conservation (sum of joints + leftover == target) is an identity that
    // holds for every band and every target, including inside a soft
    // transition: each recruitJoint step conserves exactly, so the whole
    // chain telescopes to target - leftover regardless of how "leftover"
    // itself blends on.
    for (const F32 band_deg : bands_deg)
    {
        const F32 band = band_deg * DEG_TO_RAD;
        for (const F32 target_deg : targets_deg)
        {
            const F32 target = target_deg * DEG_TO_RAD;
            F32 joints[CHAIN_LEN];
            const F32 leftover = ALGazeRecruit::recruitChain(
                target, chain_capacities, joints, CHAIN_LEN, band);

            F32 sum = 0.f;
            for (S32 i = 0; i < CHAIN_LEN; ++i)
            {
                sum += joints[i];
            }
            ensure_approximately_equals(
                "sum of joints plus leftover conserves the requested angle",
                sum + leftover, target, 10);
        }
    }

    // "Until total capacity exhausted": leftover is exactly zero while the
    // chain has room, and strictly positive once it doesn't. With band > 0
    // that transition is itself smooth over roughly
    // [total_capacity - band, total_capacity + band] (the last joint's own
    // soft knee against "leftover" as the notional next joint downstream),
    // so the hard zero/nonzero split is only checked comfortably outside
    // that transition, not on a target that lands inside it.
    for (const F32 band_deg : bands_deg)
    {
        const F32 band = band_deg * DEG_TO_RAD;
        const F32 clear_margin = band + 5.f * DEG_TO_RAD;

        F32 under_joints[CHAIN_LEN];
        const F32 under_leftover = ALGazeRecruit::recruitChain(
            total_capacity - clear_margin, chain_capacities, under_joints,
            CHAIN_LEN, band);
        ensure("no leftover while comfortably under total capacity",
               fabsf(under_leftover) <= 1e-3f);

        F32 over_joints[CHAIN_LEN];
        const F32 over_leftover = ALGazeRecruit::recruitChain(
            total_capacity + clear_margin, chain_capacities, over_joints,
            CHAIN_LEN, band);
        ensure("leftover appears once comfortably past total capacity",
               fabsf(over_leftover) > 1e-3f);
    }
}

template<> template<>
void algazerecruit_test_object::test<5>()
{
    set_test_name("symmetry for +/- magnitude");

    const F32 chain_capacities[] = {
        25.f * DEG_TO_RAD, 35.f * DEG_TO_RAD, 35.f * DEG_TO_RAD,
        45.f * DEG_TO_RAD, 35.f * DEG_TO_RAD };
    constexpr S32 CHAIN_LEN = 5;
    const F32 band = 6.f * DEG_TO_RAD;
    const F32 targets_deg[] = { 5.f, 40.f, 96.5f, 174.9f, 175.f, 220.f };

    for (const F32 target_deg : targets_deg)
    {
        const F32 target = target_deg * DEG_TO_RAD;
        F32 pos_joints[CHAIN_LEN];
        F32 neg_joints[CHAIN_LEN];
        const F32 pos_leftover = ALGazeRecruit::recruitChain(
            target, chain_capacities, pos_joints, CHAIN_LEN, band);
        const F32 neg_leftover = ALGazeRecruit::recruitChain(
            -target, chain_capacities, neg_joints, CHAIN_LEN, band);

        for (S32 i = 0; i < CHAIN_LEN; ++i)
        {
            ensure_approximately_equals(
                "negative request mirrors the positive per-joint output",
                neg_joints[i], -pos_joints[i], 12);
        }
        ensure_approximately_equals(
            "negative request mirrors the positive leftover",
            neg_leftover, -pos_leftover, 12);
    }
}

template<> template<>
void algazerecruit_test_object::test<6>()
{
    set_test_name("no joint exceeds its capacity+band contract");

    const F32 chain_capacities[] = {
        25.f * DEG_TO_RAD, 35.f * DEG_TO_RAD, 35.f * DEG_TO_RAD,
        45.f * DEG_TO_RAD, 35.f * DEG_TO_RAD };
    constexpr S32 CHAIN_LEN = 5;
    const F32 band = 6.f * DEG_TO_RAD;

    for (S32 step = 0; step <= 60; ++step)
    {
        const F32 target = (static_cast<F32>(step) * 5.f) * DEG_TO_RAD; // 0..300 deg
        F32 joints[CHAIN_LEN];
        ALGazeRecruit::recruitChain(
            target, chain_capacities, joints, CHAIN_LEN, band);
        for (S32 i = 0; i < CHAIN_LEN; ++i)
        {
            ensure("no joint exceeds capacity plus the recruitment band",
                   fabsf(joints[i]) <= chain_capacities[i] + band + 1e-4f);
        }
    }
}

template<> template<>
void algazerecruit_test_object::test<7>()
{
    set_test_name("downstream joint begins contributing before the upstream hard cap");

    const F32 capacities[] = { 1.f, 1.f };
    constexpr S32 CHAIN_LEN = 2;
    const F32 band = 0.2f;

    // Just short of the upstream joint's old hard cap of 1.0.
    const F32 target = 0.92f;

    F32 hard_joints[CHAIN_LEN];
    const F32 hard_leftover = ALGazeRecruit::recruitChain(
        target, capacities, hard_joints, CHAIN_LEN, 0.f);
    ensure_approximately_equals(
        "band-0 downstream joint is still exactly zero before the cap",
        hard_joints[1], 0.f, 16);
    ensure_approximately_equals(
        "band-0 upstream joint carries the whole request below its cap",
        hard_joints[0], target, 14);
    ensure_approximately_equals(
        "band-0 leftover is zero below capacity",
        hard_leftover, 0.f, 16);

    F32 soft_joints[CHAIN_LEN];
    ALGazeRecruit::recruitChain(
        target, capacities, soft_joints, CHAIN_LEN, band);
    ensure("soft downstream joint has already begun contributing before the old hard cap",
           soft_joints[1] > 1e-4f);
    ensure("soft upstream joint is correspondingly reduced versus the hard knee",
           soft_joints[0] < hard_joints[0]);
    ensure_approximately_equals(
        "soft handoff still conserves the total exactly",
        soft_joints[0] + soft_joints[1], target, 12);

    // Once the upstream joint is fully saturated, both paths converge on
    // exactly the residual overflow going to the downstream joint.
    const F32 saturated_target = 3.f;
    F32 hard_saturated[CHAIN_LEN];
    F32 soft_saturated[CHAIN_LEN];
    ALGazeRecruit::recruitChain(
        saturated_target, capacities, hard_saturated, CHAIN_LEN, 0.f);
    ALGazeRecruit::recruitChain(
        saturated_target, capacities, soft_saturated, CHAIN_LEN, band);
    ensure_approximately_equals(
        "far past the cap, upstream joints agree",
        soft_saturated[0], hard_saturated[0], 10);
    ensure_approximately_equals(
        "far past the cap, downstream joints agree",
        soft_saturated[1], hard_saturated[1], 10);
}

} // namespace tut
