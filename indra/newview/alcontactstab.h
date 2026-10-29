/**
 * @file alcontactstab.h
 * @brief Milestone 2 Pose Polish: pure per-foot contact inference + world lock.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Contact-inference half of the contact stabilizer (see
 * docs/pose_polish_integration_plan.md, "Milestone 2 — Contact stabilizer",
 * algorithm steps 1-2). Header-only and free of any LLVOAvatar / LLJoint
 * dependency: it is a pure function of per-frame foot world samples + dt, so
 * it is unit-testable in isolation (tests/alposepolish_test.cpp). The actual
 * leg IK solve (LLJointSolverRP3 against live joints) is wired separately by
 * the integrator; this module only decides WHEN a foot is planted and WHERE
 * it is locked.
 *
 * Contract highlights:
 *  - Contact is inferred primarily from low world-space foot SPEED, with
 *    hysteresis: an enter threshold (mPlantSpeed) + dwell (mPlantDwell) to
 *    plant, and a strictly higher exit threshold (mReleaseSpeed) + dwell
 *    (mReleaseDwell) to release. Speeds inside the band never flip state, so
 *    contact cannot flicker.
 *  - Height above ground is SUPPORTING evidence only: a foot more than
 *    mMaxGroundDist above the supplied ground height cannot plant (and its
 *    plant dwell does not accrue), but height never forces a release. Pass
 *    ground_z = -FLT_MAX (or anything non-greater-than it, e.g. -inf/NaN) to
 *    ignore height entirely and run velocity-only.
 *  - While planted, updateFoot() returns the world position captured at plant
 *    time (mLockPos) — the anti-footskate lock — regardless of small raw-foot
 *    drift. When not planted it returns the raw sample untouched.
 *  - Non-finite or <= 0 dt and non-finite positions are safe: no NaN ever
 *    enters the state, timers do not advance, and no spurious plant/release
 *    occurs on such frames.
 */

#ifndef AL_ALCONTACTSTAB_H
#define AL_ALCONTACTSTAB_H

#include "llmath.h"
#include "v3math.h"

#include <cfloat>
#include <cmath>

namespace ALContactStab
{
// ---------------------------------------------------------------------------
// Per-foot state
// ---------------------------------------------------------------------------
struct ContactFoot
{
    bool      mValid = false;
    bool      mPlanted = false;      // current (hysteretic) contact state
    LLVector3 mLockPos;              // world position the foot is locked to while planted
    LLVector3 mLastPos;              // previous world sample (for velocity)
    F32       mBelowSpeedSec = 0.f;  // dwell timer under the speed threshold
    F32       mAboveSpeedSec = 0.f;  // dwell timer over the speed threshold
};

// ---------------------------------------------------------------------------
// Tunables (hysteresis band: mPlantSpeed < mReleaseSpeed)
// ---------------------------------------------------------------------------
struct ContactParams
{
    F32 mPlantSpeed   = 0.15f;   // m/s: below (with dwell) => candidate planted
    F32 mReleaseSpeed = 0.45f;   // m/s: above (with dwell) => release  (hysteresis band)
    F32 mPlantDwell   = 0.06f;   // s under plant speed before planting
    F32 mReleaseDwell = 0.04f;   // s over release speed before releasing
    F32 mMaxGroundDist = 0.15f;  // m: supporting evidence only; too high above ground => cannot plant
};

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace detail
{
inline bool finiteF(F32 f)
{
    return std::isfinite(f);
}

inline bool finiteVec(const LLVector3& v)
{
    return finiteF(v.mV[VX]) && finiteF(v.mV[VY]) && finiteF(v.mV[VZ]);
}
} // namespace detail

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

// Forget everything: the next updateFoot() call reseeds from its sample with
// no contact (teleport / region cross / scrub / skeleton rebuild).
inline void resetFoot(ContactFoot& f)
{
    f = ContactFoot();
}

// Update one foot from its current world position this frame. @ground_z is the
// ground/terrain height under the foot (caller supplies; may be -FLT_MAX to
// ignore height evidence). Returns the world position the IK should hold this
// frame (mLockPos when planted, else the raw foot position). Sets out_planted.
inline LLVector3 updateFoot(ContactFoot& f, const ContactParams& p,
                            const LLVector3& foot_world_pos, F32 ground_z,
                            F32 dt, bool& out_planted)
{
    const bool pos_ok = detail::finiteVec(foot_world_pos);
    const bool dt_ok  = detail::finiteF(dt) && dt > 0.f;

    // Degenerate sample: hold state exactly (no timer advance, no state flip,
    // no NaN admitted). Return the lock while planted, else the last finite
    // sample we have (or the raw sample when it is at least finite).
    if (!pos_ok)
    {
        out_planted = f.mPlanted;
        return f.mPlanted ? f.mLockPos : (f.mValid ? f.mLastPos : LLVector3());
    }

    if (!f.mValid)
    {
        // First-ever call: seed the velocity reference and pass through.
        f = ContactFoot();
        f.mValid = true;
        f.mLastPos = foot_world_pos;
        out_planted = false;
        return foot_world_pos;
    }

    if (!dt_ok)
    {
        // No time elapsed (or nonsense dt): speed is unobservable this frame.
        // Hold state and timers; keep the velocity reference untouched so the
        // next good frame differences across the gap consistently.
        out_planted = f.mPlanted;
        return f.mPlanted ? f.mLockPos : foot_world_pos;
    }

    const F32 speed = (foot_world_pos - f.mLastPos).length() / dt;
    f.mLastPos = foot_world_pos;

    // Height evidence: only meaningful when a real ground height is supplied.
    // ground_z <= -FLT_MAX (incl. -inf) or NaN => ignore height (velocity-only).
    const bool ignore_height = !(ground_z > -FLT_MAX);
    const bool height_ok = ignore_height ||
                           (foot_world_pos.mV[VZ] - ground_z <= p.mMaxGroundDist);

    if (!f.mPlanted)
    {
        if (speed < p.mPlantSpeed && height_ok)
        {
            f.mBelowSpeedSec += dt;
        }
        else
        {
            // Too fast, or lifted too high: contact evidence is broken, the
            // dwell restarts (a momentarily slow airborne foot cannot plant).
            f.mBelowSpeedSec = 0.f;
        }
        f.mAboveSpeedSec = 0.f;

        if (f.mBelowSpeedSec >= p.mPlantDwell)
        {
            f.mPlanted = true;
            f.mLockPos = foot_world_pos;
            f.mBelowSpeedSec = 0.f;
            f.mAboveSpeedSec = 0.f;
        }
    }
    else
    {
        // Hysteresis: only speeds ABOVE the (higher) release threshold accrue
        // release dwell; anything inside the band resets it, so a brief blip
        // between the thresholds never releases the lock.
        if (speed > p.mReleaseSpeed)
        {
            f.mAboveSpeedSec += dt;
        }
        else
        {
            f.mAboveSpeedSec = 0.f;
        }
        f.mBelowSpeedSec = 0.f;

        if (f.mAboveSpeedSec >= p.mReleaseDwell)
        {
            f.mPlanted = false;
            f.mBelowSpeedSec = 0.f;
            f.mAboveSpeedSec = 0.f;
        }
    }

    out_planted = f.mPlanted;
    return f.mPlanted ? f.mLockPos : foot_world_pos;
}

} // namespace ALContactStab

#endif // AL_ALCONTACTSTAB_H
