/**
 * @file algazerecruit.h
 * @brief Soft anatomical recruitment: C2 replacement for the hard-knee joint
 * allocator, pure math shared by Actor Gaze and its unit tests.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALGAZERECRUIT_H
#define AL_ALGAZERECRUIT_H

#include "llmath.h"

#include <cmath>

// ALGazeRecruit implements the Cinematic Gaze build spec's soft anatomical
// recruitment (doc/CINEMATIC_GAZE_LIFE_BUILD_SPEC.md section 2.2, "Soft
// anatomical recruitment") per the deep-research companion sections 4.1-4.2.
//
// `ALGazeMath::distributeAnatomicalChain` fills each joint in the chain up to
// its hard capacity, then spills the remainder to the next joint
// (`algazemath.h` ~634-653):
//
//     this_joint = min(residual, capacity)
//     residual   = max(residual - capacity, 0)
//
// Both outputs are continuous, but the first derivative of `this_joint` with
// respect to a smoothly moving `residual` breaks exactly at `residual ==
// capacity`. If a trajectory drives `residual` through that point, the
// joint's angular velocity steps. `smoothExcess` below is a compact, C2
// "soft excess" function: the integral of a quintic smootherstep taken across
// a band of half-width `b` centered on the knee. Substituting it for the
// hard `max(z, 0)` in the residual/knee math removes the derivative jump
// while leaving the two joints' combined output exactly angle-conserving,
// and collapses back to the bit-exact hard knee when `b == 0`.
namespace ALGazeRecruit
{

// ---------------------------------------------------------------------------
// smoothExcess (companion section 4.2)
// ---------------------------------------------------------------------------
// A C2 "soft excess": for z far below the band, exactly 0; for z far above
// the band, exactly z; through the compact band (-b, b), a quintic blend
// (the integral of the standard smootherstep 6t^5-15t^4+10t^3) that meets
// both outer branches in value, first derivative, and second derivative.
// At b <= 0 the band collapses and this is exactly the hard knee max(z, 0).
//
//   b <= 0    -> max(z, 0)
//   z <= -b   -> 0
//   z >=  b   -> z
//   otherwise -> t = (z + b) / (2b);  2b * (t^6 - 3t^5 + 2.5t^4)
inline F32 smoothExcess(F32 z, F32 b)
{
    if (!std::isfinite(z))
    {
        z = 0.f;
    }
    if (!std::isfinite(b) || b <= 0.f)
    {
        return llmax(z, 0.f);
    }
    if (z <= -b)
    {
        return 0.f;
    }
    if (z >= b)
    {
        return z;
    }

    const F32 t  = (z + b) / (2.f * b);
    const F32 t2 = t * t;
    const F32 t3 = t2 * t;
    const F32 t4 = t3 * t;
    const F32 t5 = t4 * t;
    const F32 t6 = t5 * t;
    return 2.f * b * (t6 - 3.f * t5 + 2.5f * t4);
}

// ---------------------------------------------------------------------------
// Recruitment allocator (companion section 4.2)
// ---------------------------------------------------------------------------
// One joint's soft fill-then-spill step, mirroring `distributeAnatomicalChain`
// `allocate()` (algazemath.h ~634) but band-aware. `residual` is a
// nonnegative magnitude still available to this joint and everything
// downstream of it. Returns this joint's contribution and rewrites
// `residual` to the (possibly soft) remainder handed to the next joint in
// the chain.
//
// At band == 0 this reproduces the hard knee bit-exactly:
//   this_joint = residual - smoothExcess(residual - capacity, 0)
//              = min(residual, capacity)
//   residual   = smoothExcess(residual - capacity, 0)
//              = max(residual - capacity, 0)
//
// At band > 0, `residual - capacity` crosses into the band `band` before the
// old hard cap, so the downstream joint's remainder rises off zero slightly
// before this joint saturates, and the handoff between the two is C2
// (smoothExcess is C2 at both band edges). Every step conserves the total
// exactly: this_joint + residual_after == residual_before, always, so the
// full chain conserves the requested magnitude down to whatever the chain's
// combined capacity cannot absorb.
inline F32 recruitJoint(F32& residual, F32 capacity, F32 band)
{
    const F32 safe_capacity =
        std::isfinite(capacity) ? llmax(capacity, 0.f) : 0.f;
    const F32 safe_residual =
        std::isfinite(residual) ? llmax(residual, 0.f) : 0.f;

    const F32 downstream = smoothExcess(safe_residual - safe_capacity, band);
    const F32 amount = safe_residual - downstream;
    residual = downstream;
    return amount;
}

// Distributes a single signed magnitude across an ORDERED chain of joints
// (index 0 = filled first, matching eyes -> head -> neck -> ... precedence),
// each with its own nonnegative capacity. Writes each joint's signed
// contribution (same sign as `target_magnitude`) to `out_joints[0..count)`
// and returns the signed leftover beyond what the whole chain could absorb
// (zero once total capacity covers the request). `band` is shared by every
// joint boundary in the chain; pass 0 to reproduce
// `distributeAnatomicalChain`'s hard knee exactly at every joint.
//
// Angle-conserving by construction: sum(out_joints) + returned leftover ==
// target_magnitude for any band, because each `recruitJoint` step conserves
// exactly and the per-step amounts telescope.
inline F32 recruitChain(F32 target_magnitude, const F32* capacities,
                        F32* out_joints, S32 count, F32 band)
{
    const F32 safe_target =
        std::isfinite(target_magnitude) ? target_magnitude : 0.f;
    if (count <= 0)
    {
        return safe_target;
    }

    const F32 sign = safe_target >= 0.f ? 1.f : -1.f;
    F32 residual = fabsf(safe_target);
    for (S32 i = 0; i < count; ++i)
    {
        out_joints[i] = sign * recruitJoint(residual, capacities[i], band);
    }
    return sign * residual;
}

} // namespace ALGazeRecruit

#endif // AL_ALGAZERECRUIT_H
