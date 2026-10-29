/**
 * @file alghostmotionmath.h
 * @brief Pure evaluator for Ghost Studio's procedural motion modes (hover,
 *        jump, space walk, float, ascend, vortex, pendulum, bounce).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALGHOSTMOTIONMATH_H
#define AL_ALGHOSTMOTIONMATH_H

#include "llmath.h"
#include "llquaternion.h"
#include "v3math.h"

#include <cmath>

// Header-only so the Studio runtime and the TUT share one evaluator. Every
// output is a pure function of the Input: no clocks, no hidden state, so the
// same elapsed time always gives the same pose. (The Studio's motion clock
// only runs forward; elapsed time is measured from when Apply was clicked.)
namespace ALGhostMotionMath
{
enum Kind : S32
{
    KIND_HOVER = 0,
    KIND_JUMP,
    KIND_SPACE_WALK,
    KIND_FLOAT,
    KIND_ASCEND,
    KIND_VORTEX,
    KIND_PENDULUM,
    KIND_BOUNCE,
    KIND_COUNT
};

// Which built-in animation the mode wants on the clone this frame. The Studio
// maps these to ANIM_AGENT_* ids; NONE leaves the clone's own drive alone.
enum AnimCue : S32
{
    CUE_NONE = 0,
    CUE_HOVER,
    CUE_HOVER_UP,
    CUE_PRE_JUMP,
    CUE_JUMP,
    CUE_LAND
};

constexpr S32 SEED_COUNT = 6;
// Per-slot phase lag, so a group hops or bobs as a travelling wave. Hop modes
// use a fraction of a cycle; sinusoidal modes use radians (Ripple's value).
constexpr F64 HOP_SLOT_CYCLES = 0.11;
constexpr F32 WAVE_SLOT_RADIANS = 0.7f;
// Jump timeline within one cycle: crouch, air, land.
constexpr F32 JUMP_AIR_START = 0.2f;
constexpr F32 JUMP_AIR_END = 0.8f;
// Space walk: a short ground contact, then a long low-gravity arc.
constexpr F32 SPACE_WALK_CONTACT = 0.08f;
constexpr F32 PENDULUM_MAX_DEG = 25.f;
constexpr F32 PENDULUM_MIN_LENGTH = 0.1f;
// A tumbling body pivots at the pelvis; below this lift its feet would sweep
// through the ground.
constexpr F32 FLOAT_MIN_LIFT = 1.2f;
// Every mode eases in from the authored pose over this long, so Apply never
// teleports a clone (hover/float lift, mid-arc hop slots, vortex height).
constexpr F32 RAMP_IN_SECONDS = 0.5f;

struct Input
{
    Kind      mKind = KIND_HOVER;
    F64       mElapsed = 0.0;    // seconds since the motion started (presentation clock)
    F32       mSpeed = 0.2f;     // cycles per second (Ascend: 1 / rise duration)
    F32       mAmplitude = 1.f;  // metres (Pendulum: rope length)
    S32       mSlot = 0;         // order within the motion group
    F32       mSeeds[SEED_COUNT] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f }; // [0,1) per clone
    LLVector3 mRadial;           // base foot minus group centre, world metres
    LLVector3 mBaseForward = LLVector3::x_axis; // base heading, world, horizontal
};

struct Output
{
    // Added to the base foot (world metres).
    LLVector3    mOffset;
    // Applied in the clone's LOCAL frame before its base rotation:
    // rotation = mLocalRotation * baseRotation (LL order: apply left first).
    LLQuaternion mLocalRotation;
    AnimCue      mCue = CUE_NONE;
};

inline bool ownsRotation(Kind kind)
{
    return kind == KIND_HOVER || kind == KIND_SPACE_WALK ||
           kind == KIND_FLOAT || kind == KIND_VORTEX ||
           kind == KIND_PENDULUM;
}

// Non-yaw rotation (pitch/roll/tumble). These need the root re-asserted after
// the avatar update, which otherwise slerps every root back toward upright.
inline bool ownsTilt(Kind kind)
{
    return kind == KIND_HOVER || kind == KIND_SPACE_WALK ||
           kind == KIND_FLOAT || kind == KIND_PENDULUM;
}

inline F32 finiteOr(F32 value, F32 fallback)
{
    return std::isfinite(value) ? value : fallback;
}

// Fractional part in [0,1), computed in F64 so long sessions keep precision.
inline F32 cycleFraction(F64 cycles)
{
    if (!std::isfinite(cycles))
    {
        return 0.f;
    }
    F64 frac = cycles - std::floor(cycles);
    if (frac < 0.0 || frac >= 1.0)
    {
        frac = 0.0;
    }
    return (F32)frac;
}

// Sine of 2*pi*cycles*rate + offset, reduced in F64 so the argument stays
// small however long the take runs.
inline F32 wave(F64 cycles, F64 rate, F64 offset_cycles)
{
    const F32 frac = cycleFraction(cycles * rate + offset_cycles);
    return sinf(F_TWO_PI * frac);
}

// 4v(1-v): a parabolic arc that is 0 at both ends and 1 at the apex.
inline F32 arc(F32 v)
{
    v = llclamp(v, 0.f, 1.f);
    return 4.f * v * (1.f - v);
}

inline F32 smoothstep01(F32 x)
{
    x = llclamp(x, 0.f, 1.f);
    return x * x * (3.f - 2.f * x);
}

inline LLVector3 horizontalForward(const LLVector3& forward)
{
    LLVector3 f(forward.mV[VX], forward.mV[VY], 0.f);
    const F32 len = f.length();
    if (!std::isfinite(len) || len < 1e-4f)
    {
        return LLVector3::x_axis;
    }
    return f / len;
}

inline LLVector3 seededAxis(const F32 seeds[SEED_COUNT])
{
    LLVector3 axis(seeds[0] * 2.f - 1.f, seeds[1] * 2.f - 1.f,
                   seeds[2] * 2.f - 1.f);
    const F32 len = axis.length();
    if (!std::isfinite(len) || len < 0.1f)
    {
        return LLVector3(0.35f, 0.25f, 0.9f) / LLVector3(0.35f, 0.25f, 0.9f).length();
    }
    return axis / len;
}

inline Output evaluate(const Input& in)
{
    Output out;
    const F32 speed = llclamp(finiteOr(in.mSpeed, 0.f), 0.f, 10.f);
    const F32 amp = llclamp(finiteOr(in.mAmplitude, 0.f), 0.f, 100.f);
    const F64 elapsed = std::isfinite(in.mElapsed) ? llmax(in.mElapsed, 0.0) : 0.0;
    const F64 cycles = (F64)speed * elapsed;
    const F64 slot = (F64)llmax(in.mSlot, 0);
    const F64 slot_wave_cycles = -slot * (F64)WAVE_SLOT_RADIANS / (F64)F_TWO_PI;
    // Ease in from the authored pose. Ascend is excluded: it already starts at
    // rest and its rise time is the user's speed setting. The ramp scales
    // ANGLES before quaternions are built (never slerps a finished rotation:
    // a wrapped angle past 180 deg would make that take the short way round
    // and spin the clone half a turn in one frame).
    const F32 ramp = in.mKind == KIND_ASCEND
        ? 1.f : smoothstep01((F32)(elapsed / (F64)RAMP_IN_SECONDS));
    // A continuously growing angle for spins and tumbles. Its RATE eases in
    // (angle = integral of speed * ramp), so the clone spins up smoothly with
    // no overshoot: integral of smoothstep(t/T) dt = T*(x^3 - x^4/2) for
    // x = t/T < 1, and t - T/2 after. Unwrapped while easing in (small), then
    // reduced in F64; the two agree at x == 1, so the hand-over is continuous.
    const F64 ramp_x = elapsed / (F64)RAMP_IN_SECONDS;
    const F64 spin_cycles = (F64)speed * (ramp_x < 1.0
        ? (F64)RAMP_IN_SECONDS * (ramp_x * ramp_x * ramp_x -
                                  0.5 * ramp_x * ramp_x * ramp_x * ramp_x)
        : elapsed - 0.5 * (F64)RAMP_IN_SECONDS);
    auto spin_angle = [&](F64 rate) -> F32
    {
        if (ramp_x < 1.0)
        {
            return (F32)((F64)F_TWO_PI * spin_cycles * rate);
        }
        return F_TWO_PI * cycleFraction(spin_cycles * rate);
    };

    switch (in.mKind)
    {
    case KIND_HOVER:
    {
        const F32 bob = wave(cycles, 1.0, slot_wave_cycles);
        out.mOffset.mV[VZ] = amp + 0.15f * amp * bob;
        const F32 roll = ramp * 3.f * DEG_TO_RAD * wave(cycles, 0.5, 0.21);
        const F32 pitch = ramp * 2.f * DEG_TO_RAD * wave(cycles, 0.37, 0.0);
        out.mLocalRotation = LLQuaternion(roll, LLVector3::x_axis) *
                             LLQuaternion(pitch, LLVector3::y_axis);
        out.mCue = CUE_HOVER;
        break;
    }
    case KIND_JUMP:
    {
        const F32 u = cycleFraction(cycles + slot * HOP_SLOT_CYCLES);
        if (u < JUMP_AIR_START)
        {
            out.mCue = CUE_PRE_JUMP;
        }
        else if (u < JUMP_AIR_END)
        {
            const F32 v = (u - JUMP_AIR_START) / (JUMP_AIR_END - JUMP_AIR_START);
            out.mOffset.mV[VZ] = amp * arc(v);
            out.mCue = CUE_JUMP;
        }
        else
        {
            out.mCue = CUE_LAND;
        }
        break;
    }
    case KIND_SPACE_WALK:
    {
        const F32 u = cycleFraction(cycles + slot * HOP_SLOT_CYCLES);
        if (u >= SPACE_WALK_CONTACT)
        {
            const F32 v = (u - SPACE_WALK_CONTACT) / (1.f - SPACE_WALK_CONTACT);
            out.mOffset.mV[VZ] = amp * arc(v);
        }
        const LLVector3 fwd = horizontalForward(in.mBaseForward);
        out.mOffset += fwd * (0.25f * amp * wave(cycles, 1.0, 0.0));
        const F32 pitch = ramp * 8.f * DEG_TO_RAD * wave(cycles, 1.0, 0.064);
        const F32 roll = ramp * 5.f * DEG_TO_RAD * wave(cycles, 0.5, 0.0);
        out.mLocalRotation = LLQuaternion(roll, LLVector3::x_axis) *
                             LLQuaternion(pitch, LLVector3::y_axis);
        out.mCue = CUE_HOVER;
        break;
    }
    case KIND_FLOAT:
    {
        out.mOffset.mV[VX] = 0.5f * amp * wave(cycles, 0.31, in.mSeeds[3]);
        out.mOffset.mV[VY] = 0.5f * amp * wave(cycles, 0.23, in.mSeeds[4]);
        out.mOffset.mV[VZ] = llmax(amp, FLOAT_MIN_LIFT) +
                             0.3f * amp * wave(cycles, 0.17, in.mSeeds[5]);
        // Half a revolution per cycle: 10 s per turn at the 0.2 default.
        out.mLocalRotation = LLQuaternion(spin_angle(0.5), seededAxis(in.mSeeds));
        out.mCue = CUE_HOVER;
        break;
    }
    case KIND_ASCEND:
    {
        // One-shot: rise over 1/speed seconds, then hold. Not looped, so no slot.
        if (speed <= 0.f)
        {
            break; // never rises: no offset, no cue
        }
        const F32 x = (F32)llclamp(cycles, 0.0, 1.0);
        out.mOffset.mV[VZ] = amp * smoothstep01(x);
        out.mCue = x < 1.f ? CUE_HOVER_UP : CUE_HOVER;
        break;
    }
    case KIND_VORTEX:
    {
        const F32 a = F_TWO_PI * cycleFraction(cycles);
        const F32 h = 0.5f - 0.5f * wave(cycles, 0.5, 0.25 + slot_wave_cycles);
        // The authored radius at ground level, narrowing as the clone rises.
        const F32 radius_scale = 1.f - 0.4f * h;
        const F32 c = cosf(a), s = sinf(a);
        const LLVector3 rotated(
            in.mRadial.mV[VX] * c - in.mRadial.mV[VY] * s,
            in.mRadial.mV[VX] * s + in.mRadial.mV[VY] * c,
            0.f);
        const LLVector3 flat(in.mRadial.mV[VX], in.mRadial.mV[VY], 0.f);
        out.mOffset = rotated * radius_scale - flat;
        out.mOffset.mV[VZ] = amp * h;
        out.mLocalRotation = LLQuaternion(spin_angle(2.0), LLVector3::z_axis);
        out.mCue = CUE_HOVER;
        break;
    }
    case KIND_PENDULUM:
    {
        const F32 length = llmax(amp, PENDULUM_MIN_LENGTH);
        const F32 theta = ramp * PENDULUM_MAX_DEG * DEG_TO_RAD *
                          wave(cycles, 1.0, slot_wave_cycles);
        const LLVector3 fwd = horizontalForward(in.mBaseForward);
        out.mOffset = fwd * (length * sinf(theta));
        out.mOffset.mV[VZ] = length * (1.f - cosf(theta));
        // Swung forward, the body leans back toward the pivot above it.
        out.mLocalRotation = LLQuaternion(-theta, LLVector3::y_axis);
        break;
    }
    case KIND_BOUNCE:
    {
        const F32 u = cycleFraction(cycles + slot * HOP_SLOT_CYCLES);
        out.mOffset.mV[VZ] = amp * sinf(F_PI * u);
        break;
    }
    default:
        break;
    }

    if (!out.mOffset.isFinite())
    {
        out.mOffset.clear();
    }
    if (!out.mLocalRotation.isFinite())
    {
        out.mLocalRotation = LLQuaternion::DEFAULT;
    }
    out.mLocalRotation.normalize();
    // Positions ease in linearly with the ramp (continuous whatever the angle).
    if (ramp < 1.f)
    {
        out.mOffset *= ramp;
    }
    return out;
}
} // namespace ALGhostMotionMath

#endif // AL_ALGHOSTMOTIONMATH_H
