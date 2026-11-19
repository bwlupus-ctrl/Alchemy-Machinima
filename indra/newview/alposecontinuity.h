/**
 * @file alposecontinuity.h
 * @brief Milestone 1 Pose Polish: pure per-joint transition inertialization.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * Gears-of-War-style post-process inertializer (see
 * docs/pose_polish_integration_plan.md, "Milestone 1 — Transition
 * inertialization"). Header-only and free of any LLVOAvatar dependency: it
 * operates on plain LLQuaternion/LLVector3 local transforms plus a per-joint
 * InertiaJoint state struct, so it is unit-testable in isolation
 * (tests/alposepolish_test.cpp) and the avatar-facing hook simply calls
 * inertialize() per blended joint.
 *
 * Contract highlights:
 *  - Settled + no transition + continuous input => the input passes through
 *    BIT-IDENTICAL (no quaternion multiply runs on that path at all).
 *  - On a transition (caller-flagged, or self-detected value jump beyond a
 *    velocity-consistent threshold) the first output reproduces the last
 *    DISPLAYED pose exactly and carries its tracked velocity.
 *  - The captured offset decays to identity/zero with a critically damped
 *    response evaluated in quaternion-log (angle-axis) space, then snaps to
 *    exactly identity/zero so the authored animation is reproduced bit-for-bit
 *    forever after.
 */

#ifndef AL_ALPOSECONTINUITY_H
#define AL_ALPOSECONTINUITY_H

#include "llmath.h"
#include "llquaternion.h"
#include "v3math.h"

#include <cmath>

namespace ALPoseContinuity
{
// ---------------------------------------------------------------------------
// Tunables / numeric guards
// ---------------------------------------------------------------------------

// Below these magnitudes an offset channel snaps to EXACTLY identity/zero and
// stops contributing (the bit-parity guarantee).
constexpr F32 SETTLE_ROT_EPS     = 1e-4f;   // rad
constexpr F32 SETTLE_ROT_VEL_EPS = 1e-3f;   // rad/s
constexpr F32 SETTLE_POS_EPS     = 1e-5f;   // m
constexpr F32 SETTLE_POS_VEL_EPS = 1e-4f;   // m/s

// Self-detection of a discontinuity: a per-frame input step larger than the
// base threshold plus a slack multiple of the step the input stream's tracked
// velocity already predicts is treated as a transition.
constexpr F32 JUMP_ROT_BASE  = 0.12f;       // rad (~7 degrees)
constexpr F32 JUMP_POS_BASE  = 0.02f;       // m
constexpr F32 JUMP_VEL_SLACK = 4.f;         // x velocity-predicted travel

// Critically damped decay. omega = HALF_LIFE_OMEGA_SCALE * ln2 / half_life;
// the factor 4 (Holden-style halflife_to_damping) makes the coupled
// position+velocity envelope decay roughly by half per half-life.
constexpr F32 LN2                   = 0.69314718056f;
constexpr F32 HALF_LIFE_OMEGA_SCALE = 4.f;
constexpr F32 MIN_HALF_LIFE_SEC     = 1e-3f;
constexpr F32 MAX_HALF_LIFE_SEC     = 100.f;
constexpr F32 FULL_DECAY_EXP_ARG    = 30.f; // omega*dt beyond this => fully decayed

// ---------------------------------------------------------------------------
// Per-joint shadow state
// ---------------------------------------------------------------------------
struct InertiaJoint
{
    // --- core contract fields ---
    bool         mValid = false;
    LLQuaternion mLastRot;          // last DISPLAYED local rotation
    LLVector3    mLastPos;          // last DISPLAYED local position
    LLQuaternion mRotOffset;        // current decaying rotational offset (identity when settled)
    LLVector3    mPosOffset;        // current decaying positional offset (zero when settled)
    LLVector3    mRotOffsetVel;     // angle-axis rad/s (velocity preservation)
    LLVector3    mPosOffsetVel;     // m/s
    bool         mSettled = true;   // true => offset is EXACTLY identity/zero

    // --- tracking state (finite-difference velocities + jump detection) ---
    LLQuaternion mLastInputRot;     // last RAW blended input rotation
    LLVector3    mLastInputPos;     // last RAW blended input position
    LLVector3    mDispRotVel;       // displayed angular velocity, angle-axis rad/s
    LLVector3    mDispPosVel;       // displayed linear velocity, m/s
    LLVector3    mInputRotVel;      // raw input angular velocity, angle-axis rad/s
    LLVector3    mInputPosVel;      // raw input linear velocity, m/s
    bool         mRotSettled = true;
    bool         mPosSettled = true;
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

// Quaternion log as a scaled angle-axis vector (radians). getAngleAxis()
// already resolves the double cover (forces w >= 0), so this is the shortest
// rotation.
inline LLVector3 quatLog(const LLQuaternion& q)
{
    F32 angle = 0.f;
    LLVector3 axis;
    q.getAngleAxis(&angle, axis);
    return axis * angle;
}

// Inverse of quatLog. setAngleAxis() returns identity for a near-zero axis.
inline LLQuaternion quatExp(const LLVector3& v)
{
    LLQuaternion q;
    q.setAngleAxis(v.length(), v);
    return q;
}

// Advance one critically damped spring channel (state x, velocity v) toward
// zero by dt, using the exact closed form
//   x(t) = (x0 + (v0 + w*x0) t) e^{-w t}
//   v(t) = (v0 - (v0 + w*x0) w t) e^{-w t}
// which is smooth (C1), never oscillates, and with v0 = 0 decays strictly
// monotonically. Returns false when the half-life is invalid (non-finite or
// <= 0): the caller must snap the channel closed. A non-finite or <= 0 dt is
// safe: no advance, state untouched.
inline bool springAdvance(LLVector3& x, LLVector3& v, F32 dt, F32 half_life)
{
    if (!finiteF(half_life) || half_life <= 0.f)
    {
        return false;
    }
    if (!finiteF(dt) || dt <= 0.f)
    {
        return true;
    }
    F32 hl = half_life < MIN_HALF_LIFE_SEC ? MIN_HALF_LIFE_SEC
           : (half_life > MAX_HALF_LIFE_SEC ? MAX_HALF_LIFE_SEC : half_life);
    const F32 omega = HALF_LIFE_OMEGA_SCALE * LN2 / hl;
    const F32 a = omega * dt;
    if (a > FULL_DECAY_EXP_ARG)
    {
        // e^{-a} underflows any representable state: fully decayed.
        x = LLVector3();
        v = LLVector3();
        return true;
    }
    const F32 e = expf(-a);
    const LLVector3 j = v + x * omega;
    const LLVector3 x1 = (x + j * dt) * e;
    const LLVector3 v1 = (v - j * a) * e;
    x = x1;
    v = v1;
    return true;
}
} // namespace detail

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

// Forget everything: the next inertialize() call reseeds from its input with
// no offset (teleport / region cross / scrub / skeleton rebuild).
inline void resetJoint(InertiaJoint& s)
{
    s = InertiaJoint();
}

// Filter one joint's freshly blended local transform IN PLACE toward polished.
// @io_rot/@io_pos: in = blended (authored) pose; out = polished pose.
// @dt: real frame seconds (<= 0 or non-finite => no decay advance this frame).
// @rot_half_life_sec/@pos_half_life_sec: per-joint-group decay half-lives.
// @transition: caller-detected discontinuity this frame (motion set / priority
//   winner changed). Large input jumps are also self-detected.
inline void inertialize(InertiaJoint& s, LLQuaternion& io_rot, LLVector3& io_pos,
                        F32 dt, F32 rot_half_life_sec, F32 pos_half_life_sec,
                        bool transition)
{
    const bool dt_ok = detail::finiteF(dt) && dt > 0.f;

    if (!s.mValid)
    {
        // First-ever call: seed the shadow state and pass the input through
        // untouched (bit-identical by construction: io is never written).
        s = InertiaJoint();
        s.mValid = true;
        s.mLastRot = io_rot;
        s.mLastPos = io_pos;
        s.mLastInputRot = io_rot;
        s.mLastInputPos = io_pos;
        return;
    }

    const LLQuaternion in_rot = io_rot;
    const LLVector3 in_pos = io_pos;

    // This frame's raw input step (shortest-arc log for rotation).
    const LLVector3 rot_step = detail::quatLog(in_rot * ~s.mLastInputRot);
    const LLVector3 pos_step = in_pos - s.mLastInputPos;

    // Discontinuity: caller-flagged, or a value jump beyond what the input
    // stream's tracked velocity makes plausible this frame.
    bool jump = transition;
    if (!jump)
    {
        F32 rot_allow = JUMP_ROT_BASE;
        F32 pos_allow = JUMP_POS_BASE;
        if (dt_ok)
        {
            rot_allow += JUMP_VEL_SLACK * s.mInputRotVel.length() * dt;
            pos_allow += JUMP_VEL_SLACK * s.mInputPosVel.length() * dt;
        }
        jump = rot_step.length() > rot_allow || pos_step.length() > pos_allow;
    }

    if (jump)
    {
        // Capture the offset of the last DISPLAYED pose relative to the new
        // input, so that (offset composed with input) == old displayed pose.
        LLQuaternion rot_off = s.mLastRot * ~in_rot;
        rot_off.normalize();
        const LLVector3 rot_off_log = detail::quatLog(rot_off);
        const LLVector3 pos_off = s.mLastPos - in_pos;

        // Seed the offset velocity from the tracked display velocity. The new
        // stream's own velocity is unobservable on the capture frame (only one
        // sample of it exists yet), so it is taken as zero here; from the next
        // frame on the new input's motion rides through the composition while
        // the offset decays independently.
        const LLVector3 rot_vel = s.mDispRotVel;
        const LLVector3 pos_vel = s.mDispPosVel;

        s.mRotSettled = rot_off_log.length() < SETTLE_ROT_EPS &&
                        rot_vel.length() < SETTLE_ROT_VEL_EPS;
        if (s.mRotSettled)
        {
            s.mRotOffset = LLQuaternion();
            s.mRotOffsetVel = LLVector3();
        }
        else
        {
            s.mRotOffset = rot_off;
            s.mRotOffsetVel = rot_vel;
        }

        s.mPosSettled = pos_off.length() < SETTLE_POS_EPS &&
                        pos_vel.length() < SETTLE_POS_VEL_EPS;
        if (s.mPosSettled)
        {
            s.mPosOffset = LLVector3();
            s.mPosOffsetVel = LLVector3();
        }
        else
        {
            s.mPosOffset = pos_off;
            s.mPosOffsetVel = pos_vel;
        }

        s.mSettled = s.mRotSettled && s.mPosSettled;

        // No decay on the capture frame: the first post-transition output must
        // reproduce the old displayed pose. Input velocity tracking restarts
        // for the new stream.
        s.mInputRotVel = LLVector3();
        s.mInputPosVel = LLVector3();
    }
    else
    {
        if (dt_ok)
        {
            const F32 oodt = 1.f / dt;
            s.mInputRotVel = rot_step * oodt;
            s.mInputPosVel = pos_step * oodt;
        }

        if (!s.mRotSettled)
        {
            LLVector3 x = detail::quatLog(s.mRotOffset);
            LLVector3 v = s.mRotOffsetVel;
            const bool ok = detail::springAdvance(x, v, dt, rot_half_life_sec);
            if (!ok || (x.length() < SETTLE_ROT_EPS && v.length() < SETTLE_ROT_VEL_EPS))
            {
                // Snap to EXACT identity so the settled path is bit-parity.
                s.mRotOffset = LLQuaternion();
                s.mRotOffsetVel = LLVector3();
                s.mRotSettled = true;
            }
            else
            {
                s.mRotOffset = detail::quatExp(x);
                s.mRotOffset.normalize();
                s.mRotOffsetVel = v;
            }
        }

        if (!s.mPosSettled)
        {
            LLVector3 x = s.mPosOffset;
            LLVector3 v = s.mPosOffsetVel;
            const bool ok = detail::springAdvance(x, v, dt, pos_half_life_sec);
            if (!ok || (x.length() < SETTLE_POS_EPS && v.length() < SETTLE_POS_VEL_EPS))
            {
                s.mPosOffset = LLVector3();
                s.mPosOffsetVel = LLVector3();
                s.mPosSettled = true;
            }
            else
            {
                s.mPosOffset = x;
                s.mPosOffsetVel = v;
            }
        }

        s.mSettled = s.mRotSettled && s.mPosSettled;
    }

    // Compose the output. A settled channel composes NOTHING: io is left
    // untouched (not even multiplied by identity), which is what guarantees
    // bit-parity on the no-op path.
    LLQuaternion out_rot = in_rot;
    LLVector3 out_pos = in_pos;
    if (!s.mRotSettled)
    {
        out_rot = s.mRotOffset * in_rot;
        out_rot.normalize();
        io_rot = out_rot;
    }
    if (!s.mPosSettled)
    {
        out_pos = in_pos + s.mPosOffset;
        io_pos = out_pos;
    }

    // Track what was displayed (finite-differenced display velocity feeds the
    // next transition's velocity seed).
    if (dt_ok)
    {
        const F32 oodt = 1.f / dt;
        s.mDispRotVel = detail::quatLog(out_rot * ~s.mLastRot) * oodt;
        s.mDispPosVel = (out_pos - s.mLastPos) * oodt;
    }
    s.mLastRot = out_rot;
    s.mLastPos = out_pos;
    s.mLastInputRot = in_rot;
    s.mLastInputPos = in_pos;
}

} // namespace ALPoseContinuity

#endif // AL_ALPOSECONTINUITY_H
