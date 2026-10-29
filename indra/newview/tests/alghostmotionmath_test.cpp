/**
 * @file alghostmotionmath_test.cpp
 * @brief Unit tests for Ghost Studio's procedural motion evaluator.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "../test/lltut.h"
#include "../alghostmotionmath.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace tut
{
namespace
{
using namespace ALGhostMotionMath;

Input makeInput(Kind kind, F32 speed, F32 amplitude, F64 elapsed, S32 slot = 0)
{
    Input in;
    in.mKind = kind;
    in.mSpeed = speed;
    in.mAmplitude = amplitude;
    in.mElapsed = elapsed;
    in.mSlot = slot;
    return in;
}

LLVector3 rotateVec(const LLVector3& v, const LLQuaternion& q)
{
    return v * q;
}
}

struct alghostmotionmath_data {};
typedef test_group<alghostmotionmath_data> alghostmotionmath_test_group;
typedef alghostmotionmath_test_group::object alghostmotionmath_test_object;
alghostmotionmath_test_group alghostmotionmath_test("alghostmotionmath");

template<> template<>
void alghostmotionmath_test_object::test<1>()
{
    set_test_name("jump timeline: crouch, parabolic air arc with apex = amplitude, land");
    // speed 1 -> one hop per second; u == frac(elapsed) for slot 0. Samples
    // are taken in the second cycle, after the 0.5 s ease-in.
    Output o = evaluate(makeInput(KIND_JUMP, 1.f, 2.f, 1.1));
    ensure_equals("crouch cue", (S32)o.mCue, (S32)CUE_PRE_JUMP);
    ensure_distance("crouch on the ground", o.mOffset.mV[VZ], 0.f, 1e-6f);

    o = evaluate(makeInput(KIND_JUMP, 1.f, 2.f, 1.5));
    ensure_equals("air cue", (S32)o.mCue, (S32)CUE_JUMP);
    ensure_distance("apex height == amplitude", o.mOffset.mV[VZ], 2.f, 1e-4f);

    // v = (0.35 - 0.2) / 0.6 = 0.25 -> 4 * 0.25 * 0.75 = 0.75 of the apex.
    o = evaluate(makeInput(KIND_JUMP, 1.f, 2.f, 1.35));
    ensure_distance("quarter-arc height", o.mOffset.mV[VZ], 1.5f, 1e-4f);

    o = evaluate(makeInput(KIND_JUMP, 1.f, 2.f, 1.9));
    ensure_equals("land cue", (S32)o.mCue, (S32)CUE_LAND);
    ensure_distance("landed on the ground", o.mOffset.mV[VZ], 0.f, 1e-6f);
    ensure_distance("jump never moves sideways", o.mOffset.mV[VX], 0.f, 1e-6f);
}

template<> template<>
void alghostmotionmath_test_object::test<2>()
{
    set_test_name("hop modes lag by slot so a group jumps as a wave");
    // Slot 1 is 0.11 cycles ahead: at elapsed 1.39 its u == 0.5 (apex).
    const Output slot1 = evaluate(makeInput(KIND_JUMP, 1.f, 1.f, 1.39, 1));
    ensure_distance("slot 1 at apex", slot1.mOffset.mV[VZ], 1.f, 1e-3f);
    const Output slot0 = evaluate(makeInput(KIND_JUMP, 1.f, 1.f, 1.39, 0));
    ensure("slot 0 is not at apex at the same time", slot0.mOffset.mV[VZ] < 0.99f);
}

template<> template<>
void alghostmotionmath_test_object::test<3>()
{
    set_test_name("hover: eases up from the ground, lift = amplitude, bob +-15%, small real tilt, slot lag");
    Output o = evaluate(makeInput(KIND_HOVER, 0.25f, 1.f, 0.0));
    ensure_distance("starts on the authored spot (ease-in)", o.mOffset.mV[VZ], 0.f, 1e-6f);
    // 4 s at 0.25 cycles/s = one whole cycle: bob back at zero, full lift.
    o = evaluate(makeInput(KIND_HOVER, 0.25f, 1.f, 4.0));
    ensure_distance("rest lift", o.mOffset.mV[VZ], 1.f, 1e-4f);
    ensure_equals("hover cue", (S32)o.mCue, (S32)CUE_HOVER);
    // roll = 3deg*sin(2pi*0.71), pitch = 2deg*sin(2pi*0.37): about 3.2deg of tilt.
    const LLVector3 tilt_up = rotateVec(LLVector3::z_axis, o.mLocalRotation);
    ensure("hover really tilts (> 1 degree)", tilt_up.mV[VZ] < cosf(1.f * DEG_TO_RAD));
    const Output lagged = evaluate(makeInput(KIND_HOVER, 0.25f, 1.f, 4.0, 1));
    ensure("slot 1 bobs on a different phase",
           fabsf(lagged.mOffset.mV[VZ] - o.mOffset.mV[VZ]) > 0.05f);
    // Quarter cycle: sin(pi/2) = 1 -> bob = +0.15 * amplitude.
    o = evaluate(makeInput(KIND_HOVER, 0.25f, 1.f, 1.0));
    ensure_distance("bob peak", o.mOffset.mV[VZ], 1.15f, 1e-4f);
    for (F64 t = 0.0; t < 20.0; t += 0.37)
    {
        const Output h = evaluate(makeInput(KIND_HOVER, 0.25f, 1.f, t));
        const LLVector3 up = rotateVec(LLVector3::z_axis, h.mLocalRotation);
        ensure("hover tilt stays under 4 degrees", up.mV[VZ] > cosf(4.f * DEG_TO_RAD));
    }
}

template<> template<>
void alghostmotionmath_test_object::test<4>()
{
    set_test_name("ascend: smoothstep rise over 1/speed seconds, then hold");
    // speed 0.5 -> 2 s rise.
    Output o = evaluate(makeInput(KIND_ASCEND, 0.5f, 3.f, 1.0));
    ensure_distance("half-way height", o.mOffset.mV[VZ], 1.5f, 1e-5f);
    ensure_equals("rising cue", (S32)o.mCue, (S32)CUE_HOVER_UP);
    o = evaluate(makeInput(KIND_ASCEND, 0.5f, 3.f, 0.5));
    // x = 0.25 -> 3x^2 - 2x^3 = 0.15625
    ensure_distance("quarter-time height", o.mOffset.mV[VZ], 3.f * 0.15625f, 1e-5f);
    o = evaluate(makeInput(KIND_ASCEND, 0.5f, 3.f, 50.0));
    ensure_distance("holds at the top", o.mOffset.mV[VZ], 3.f, 1e-5f);
    ensure_equals("holding cue", (S32)o.mCue, (S32)CUE_HOVER);
    o = evaluate(makeInput(KIND_ASCEND, 0.f, 3.f, 50.0));
    ensure_distance("speed 0 never rises", o.mOffset.mV[VZ], 0.f, 1e-6f);
    ensure_equals("speed 0 plays no rising cue", (S32)o.mCue, (S32)CUE_NONE);
}

template<> template<>
void alghostmotionmath_test_object::test<5>()
{
    set_test_name("vortex: orbit about the group centre with a rising, widening spiral");
    Input in = makeInput(KIND_VORTEX, 1.f, 2.f, 1.25);
    in.mRadial = LLVector3(2.f, 0.f, 0.f);
    const Output o = evaluate(in);
    // a = 90 deg; h = 0.5 - 0.5 * sin(2*pi*frac(0.625 + 0.25)) = 0.5 - 0.5*sin(1.75*pi)
    const F32 h = 0.5f - 0.5f * sinf(1.75f * F_PI);
    const F32 rs = 1.f - 0.4f * h;
    const LLVector3 foot = in.mRadial + o.mOffset; // relative to the centre
    ensure_distance("rotated x", foot.mV[VX], 0.f, 1e-4f);
    ensure_distance("rotated y", foot.mV[VY], 2.f * rs, 1e-4f);
    ensure_distance("spiral height", o.mOffset.mV[VZ], 2.f * h, 1e-4f);
    // At t=0 nothing moves at all: same radius, same height (ease-in, and the
    // ground radius is the authored one).
    Input at_apply = in;
    at_apply.mElapsed = 0.0;
    const Output start = evaluate(at_apply);
    ensure_distance("no jump on Apply", start.mOffset.length(), 0.f, 1e-6f);
    // One full turn later (h back at 0) the clone is exactly where it was placed.
    Input full = in; full.mElapsed = 2.0;
    ensure_distance("ground radius is the authored radius",
                    evaluate(full).mOffset.length(), 0.f, 1e-4f);
    // Spin: two turns per cycle, with its rate eased in, so the steady phase
    // lags by speed * T/2 = 0.25 cycles. At 1.375 s the spin has done
    // 1.125 cycles * 2 = 2.25 turns -> 90 deg: +X maps to +Y.
    Input yaw = in; yaw.mElapsed = 1.375;
    const LLVector3 fwd = rotateVec(LLVector3::x_axis, evaluate(yaw).mLocalRotation);
    ensure_distance("vortex spin x", fwd.mV[VX], 0.f, 1e-4f);
    ensure_distance("vortex spin y", fwd.mV[VY], 1.f, 1e-4f);
}

template<> template<>
void alghostmotionmath_test_object::test<6>()
{
    set_test_name("pendulum: the body hangs from a pivot one rope length above, leaning back toward it");
    Input in = makeInput(KIND_PENDULUM, 1.f, 1.5f, 1.25); // theta = +25 deg
    in.mBaseForward = LLVector3(0.f, 1.f, 0.f);              // facing +Y
    const Output o = evaluate(in);
    const F32 theta = 25.f * DEG_TO_RAD;
    ensure_distance("swung forward along the facing", o.mOffset.mV[VY], 1.5f * sinf(theta), 1e-4f);
    ensure_distance("no sideways swing", o.mOffset.mV[VX], 0.f, 1e-5f);
    ensure_distance("rises on the arc", o.mOffset.mV[VZ], 1.5f * (1.f - cosf(theta)), 1e-4f);
    // Body-local up, after the base rotation that faces +Y (yaw 90 deg).
    const LLQuaternion base(F_PI_BY_TWO, LLVector3::z_axis);
    const LLVector3 up = rotateVec(LLVector3::z_axis, o.mLocalRotation * base);
    LLVector3 rope = LLVector3(0.f, 0.f, 1.5f) - o.mOffset; // offset -> pivot
    rope.normVec();
    ensure_distance("body up points at the pivot (x)", up.mV[VX], rope.mV[VX], 1e-4f);
    ensure_distance("body up points at the pivot (y)", up.mV[VY], rope.mV[VY], 1e-4f);
    ensure_distance("body up points at the pivot (z)", up.mV[VZ], rope.mV[VZ], 1e-4f);
}

template<> template<>
void alghostmotionmath_test_object::test<7>()
{
    set_test_name("float: bounded drift around the lift height, tumble about a fixed seeded axis");
    Input in = makeInput(KIND_FLOAT, 0.2f, 2.f, 0.0);
    const F32 seeds[SEED_COUNT] = { 0.9f, 0.2f, 0.7f, 0.1f, 0.4f, 0.8f };
    memcpy(in.mSeeds, seeds, sizeof(seeds));
    const LLVector3 axis = seededAxis(in.mSeeds);
    for (F64 t = 0.5; t < 200.0; t += 1.3) // from the end of the ease-in
    {
        in.mElapsed = t;
        const Output o = evaluate(in);
        ensure("x within +-0.5A", fabsf(o.mOffset.mV[VX]) <= 1.f + 1e-4f);
        ensure("y within +-0.5A", fabsf(o.mOffset.mV[VY]) <= 1.f + 1e-4f);
        ensure("z within A +- 0.3A",
               o.mOffset.mV[VZ] >= 1.4f - 1e-4f && o.mOffset.mV[VZ] <= 2.6f + 1e-4f);
        const LLVector3 spun = rotateVec(axis, o.mLocalRotation);
        ensure_distance("tumble axis is invariant", (spun - axis).length(), 0.f, 1e-4f);
    }
    // Eased spin: 0.2 cycles/s * (5.25 - 0.25) s = 1 cycle = half a revolution.
    in.mElapsed = 5.25;
    const Output half = evaluate(in);
    LLVector3 perp = axis % LLVector3::z_axis;
    if (perp.length() < 1e-3f) perp = axis % LLVector3::x_axis;
    perp.normVec();
    const LLVector3 turned = rotateVec(perp, half.mLocalRotation);
    ensure_distance("half revolution flips a perpendicular vector",
                    (turned + perp).length(), 0.f, 1e-3f);
    // A small Amp still lifts the pelvis 1.2 m, so the tumble clears the ground.
    Input low = makeInput(KIND_FLOAT, 0.2f, 0.2f, 5.0);
    ensure("lift floor for small amplitude", evaluate(low).mOffset.mV[VZ] >= 1.2f - 0.06f - 1e-4f);
}

template<> template<>
void alghostmotionmath_test_object::test<8>()
{
    set_test_name("space walk: ground contact, then a long arc with apex = amplitude");
    Output o = evaluate(makeInput(KIND_SPACE_WALK, 1.f, 1.f, 1.04));
    ensure_distance("contact on the ground", o.mOffset.mV[VZ], 0.f, 1e-6f);
    o = evaluate(makeInput(KIND_SPACE_WALK, 1.f, 1.f, 1.08 + 0.46));
    ensure_distance("apex", o.mOffset.mV[VZ], 1.f, 1e-3f);
    ensure_equals("floating cue", (S32)o.mCue, (S32)CUE_HOVER);
    // Quarter cycle: forward glide = 0.25 * A along the facing (+X by default),
    // and a real lean (roll 5deg*sin(2pi*0.625) = -3.5deg).
    Input glide = makeInput(KIND_SPACE_WALK, 1.f, 1.f, 1.25);
    const Output g = evaluate(glide);
    ensure_distance("forward glide", g.mOffset.mV[VX], 0.25f, 1e-4f);
    const LLVector3 lean_up = rotateVec(LLVector3::z_axis, g.mLocalRotation);
    ensure("space walk really leans (> 1 degree)", lean_up.mV[VZ] < cosf(1.f * DEG_TO_RAD));
    // Roll (about the body's X) is the only term that tips "up" sideways; pitch
    // alone would leave up.y at 0. Expected |up.y| = sin(3.54 deg) ~ 0.062.
    ensure("space walk rolls as well as pitches", fabsf(lean_up.mV[VY]) > sinf(1.f * DEG_TO_RAD));
}

template<> template<>
void alghostmotionmath_test_object::test<9>()
{
    set_test_name("bounce: continuous hops, zero at each landing, amplitude at mid-cycle");
    ensure_distance("landing", evaluate(makeInput(KIND_BOUNCE, 1.f, 0.8f, 0.0)).mOffset.mV[VZ], 0.f, 1e-6f);
    ensure_distance("top", evaluate(makeInput(KIND_BOUNCE, 1.f, 0.8f, 0.5)).mOffset.mV[VZ], 0.8f, 1e-5f);
    ensure_equals("no animation cue", (S32)evaluate(makeInput(KIND_BOUNCE, 1.f, 0.8f, 0.5)).mCue,
                  (S32)CUE_NONE);
}

template<> template<>
void alghostmotionmath_test_object::test<10>()
{
    set_test_name("deterministic and precise over long takes; non-finite input is safe");
    for (S32 k = 0; k < KIND_COUNT; ++k)
    {
        // 0.25 is exact in binary, so 0.25 * 1e6 is a whole number of cycles.
        Input in = makeInput((Kind)k, 0.25f, 1.f, 1.3);
        in.mRadial = LLVector3(1.f, 0.5f, 0.f);
        const Output a = evaluate(in);
        const Output b = evaluate(in);
        ensure("bitwise repeatable offset", memcmp(&a.mOffset, &b.mOffset, sizeof(a.mOffset)) == 0);
        ensure("bitwise repeatable rotation",
               memcmp(&a.mLocalRotation, &b.mLocalRotation, sizeof(a.mLocalRotation)) == 0);
        // 0.25 cycles/s * 1e6 s = 250000 whole cycles: the same phase as t = 1.3,
        // except Ascend, which holds after its rise.
        if (k != KIND_ASCEND)
        {
            in.mElapsed = 1.0e6 + 1.3;
            const Output late = evaluate(in);
            ensure_distance("phase survives a long take", (late.mOffset - a.mOffset).length(), 0.f, 2e-3f);
            // Rotations too (catches a tumble/spin angle reduced in F32).
            const LLVector3 f0 = rotateVec(LLVector3::x_axis, a.mLocalRotation);
            const LLVector3 f1 = rotateVec(LLVector3::x_axis, late.mLocalRotation);
            const LLVector3 u0 = rotateVec(LLVector3::z_axis, a.mLocalRotation);
            const LLVector3 u1 = rotateVec(LLVector3::z_axis, late.mLocalRotation);
            ensure_distance("rotation survives a long take (fwd)", (f1 - f0).length(), 0.f, 2e-3f);
            ensure_distance("rotation survives a long take (up)", (u1 - u0).length(), 0.f, 2e-3f);
        }
        Input bad = in;
        bad.mElapsed = std::numeric_limits<F64>::quiet_NaN();
        bad.mSpeed = std::numeric_limits<F32>::infinity();
        bad.mAmplitude = std::numeric_limits<F32>::quiet_NaN();
        const Output safe = evaluate(bad);
        ensure("finite offset on bad input", safe.mOffset.isFinite());
        ensure("finite rotation on bad input", safe.mLocalRotation.isFinite());
    }
}

template<> template<>
void alghostmotionmath_test_object::test<12>()
{
    set_test_name("ease-in never pops: every mode turns smoothly through the ramp, even at high speed");
    // Round-2 review finding: slerping a finished, wrapped rotation took the
    // short way round past 180 deg (Vortex at speed 1: 81 -> -90 deg in 10 ms).
    for (S32 k = 0; k < KIND_COUNT; ++k)
    {
        for (F32 speed : { 1.f, 4.f, 10.f })
        {
            Input in = makeInput((Kind)k, speed, 1.f, 0.0);
            // 0.5 m: at speed 10 the orbit itself legitimately covers ~3 cm/ms.
            in.mRadial = LLVector3(0.5f, 0.f, 0.f);
            LLQuaternion prev = evaluate(in).mLocalRotation;
            LLVector3 prev_off = evaluate(in).mOffset;
            for (S32 step = 1; step <= 800; ++step) // 0.8 s at 1 ms: through and past the ramp
            {
                in.mElapsed = 0.001 * step;
                const Output o = evaluate(in);
                const F32 d = fabsf(o.mLocalRotation.mQ[VX] * prev.mQ[VX] +
                                    o.mLocalRotation.mQ[VY] * prev.mQ[VY] +
                                    o.mLocalRotation.mQ[VZ] * prev.mQ[VZ] +
                                    o.mLocalRotation.mQ[VW] * prev.mQ[VW]);
                const F32 step_deg = 2.f * acosf(llmin(d, 1.f)) * RAD_TO_DEG;
                // Fastest legit rate: vortex spin at speed 10 = 7200 deg/s = 7.2 deg/ms.
                ensure("rotation step under 8 degrees per ms", step_deg < 8.f);
                ensure("offset step under 10 cm per ms", (o.mOffset - prev_off).length() < 0.1f);
                prev = o.mLocalRotation;
                prev_off = o.mOffset;
            }
        }
    }
    // The specific reported case: Vortex speed 1 over the ramp turns by well
    // under a degree per millisecond (2 turns/s * ramp <= 0.72 deg/ms + ramp growth).
    Input v = makeInput(KIND_VORTEX, 1.f, 1.f, 0.24);
    const LLQuaternion a = evaluate(v).mLocalRotation;
    v.mElapsed = 0.25;
    const LLQuaternion b = evaluate(v).mLocalRotation;
    const F32 d = fabsf(a.mQ[VX] * b.mQ[VX] + a.mQ[VY] * b.mQ[VY] + a.mQ[VZ] * b.mQ[VZ] + a.mQ[VW] * b.mQ[VW]);
    ensure("vortex 0.24 -> 0.25 s turns less than 15 degrees", 2.f * acosf(llmin(d, 1.f)) * RAD_TO_DEG < 15.f);
}

template<> template<>
void alghostmotionmath_test_object::test<11>()
{
    set_test_name("ownership table: tilt modes are a subset of rotation modes");
    ensure("hover rotates and tilts", ownsRotation(KIND_HOVER) && ownsTilt(KIND_HOVER));
    ensure("float rotates and tilts", ownsRotation(KIND_FLOAT) && ownsTilt(KIND_FLOAT));
    ensure("pendulum rotates and tilts", ownsRotation(KIND_PENDULUM) && ownsTilt(KIND_PENDULUM));
    ensure("space walk rotates and tilts", ownsRotation(KIND_SPACE_WALK) && ownsTilt(KIND_SPACE_WALK));
    ensure("vortex spins but stays upright", ownsRotation(KIND_VORTEX) && !ownsTilt(KIND_VORTEX));
    ensure("jump leaves facing alone", !ownsRotation(KIND_JUMP));
    ensure("bounce leaves facing alone", !ownsRotation(KIND_BOUNCE));
    ensure("ascend leaves facing alone", !ownsRotation(KIND_ASCEND));
    for (S32 k = 0; k < KIND_COUNT; ++k)
    {
        ensure("tilt implies rotation", !ownsTilt((Kind)k) || ownsRotation((Kind)k));
    }
}
}
