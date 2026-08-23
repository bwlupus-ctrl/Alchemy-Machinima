/**
 * @file alposepolish_test.cpp
 * @brief Unit tests for the pure Pose Polish math modules
 *        (inertialization, contact, phase, secondary motion).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * The Pose Polish milestone modules are header-only and operate on plain
 * transform inputs (no LLVOAvatar dependency), so each is unit-tested here
 * directly. Milestones add their headers + tests as they land:
 *   M1 inertialization  -> #include "../alposecontinuity.h"
 *   M2 contact           -> #include "../alcontactstab.h"
 *   M6 secondary motion  -> #include "../alposesecondary.h"
 */

#include "linden_common.h"

#include "../test/lltut.h"

#include "../alposecontinuity.h"
#include "../alcontactstab.h"

#include <cfloat>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>

namespace tut
{
namespace
{
// Bit-pattern equality (distinguishes +0.0f from -0.0f) for the no-op / idle
// bit-parity acceptance the Pose Polish contract requires.
inline bool bitwiseEqual(F32 a, F32 b)
{
    U32 au = 0, bu = 0;
    memcpy(&au, &a, sizeof(au));
    memcpy(&bu, &b, sizeof(bu));
    return au == bu;
}

inline bool bitwiseEqual(const LLQuaternion& a, const LLQuaternion& b)
{
    return bitwiseEqual(a.mQ[VX], b.mQ[VX]) && bitwiseEqual(a.mQ[VY], b.mQ[VY]) &&
           bitwiseEqual(a.mQ[VZ], b.mQ[VZ]) && bitwiseEqual(a.mQ[VW], b.mQ[VW]);
}

inline bool bitwiseEqual(const LLVector3& a, const LLVector3& b)
{
    return bitwiseEqual(a.mV[VX], b.mV[VX]) && bitwiseEqual(a.mV[VY], b.mV[VY]) &&
           bitwiseEqual(a.mV[VZ], b.mV[VZ]);
}

inline LLQuaternion rotZ(F32 angle_rad)
{
    LLQuaternion q;
    q.setAngleAxis(angle_rad, 0.f, 0.f, 1.f);
    return q;
}

// Shortest-arc angle between two rotations, radians.
inline F32 angleBetween(const LLQuaternion& a, const LLQuaternion& b)
{
    F32 angle = 0.f;
    LLVector3 axis;
    LLQuaternion delta = a * ~b;
    delta.getAngleAxis(&angle, axis);
    return angle;
}

inline F32 quatAngle(const LLQuaternion& q)
{
    F32 angle = 0.f;
    LLVector3 axis;
    q.getAngleAxis(&angle, axis);
    return angle;
}

inline bool finiteQuat(const LLQuaternion& q)
{
    return std::isfinite(q.mQ[VX]) && std::isfinite(q.mQ[VY]) &&
           std::isfinite(q.mQ[VZ]) && std::isfinite(q.mQ[VW]);
}

inline bool finiteVec(const LLVector3& v)
{
    return std::isfinite(v.mV[VX]) && std::isfinite(v.mV[VY]) && std::isfinite(v.mV[VZ]);
}
} // namespace

struct alposepolish_data {};
typedef test_group<alposepolish_data> alposepolish_test_group;
typedef alposepolish_test_group::object alposepolish_test_object;
alposepolish_test_group alposepolish_test("alposepolish");

// Placeholder so the target builds before Milestone 1 lands; the inertializer
// tests replace/extend this.
template<> template<>
void alposepolish_test_object::test<1>()
{
    ensure("pose polish test target builds", bitwiseEqual(0.f, 0.f));
}

// ---------------------------------------------------------------------------
// Milestone 1 — ALPoseContinuity::inertialize
// ---------------------------------------------------------------------------
using ALPoseContinuity::InertiaJoint;
using ALPoseContinuity::inertialize;
using ALPoseContinuity::resetJoint;

// Invariant 1a: the first-ever call seeds the shadow state and passes the
// input through bit-identically (including signed zeros).
template<> template<>
void alposepolish_test_object::test<2>()
{
    InertiaJoint s;
    const LLQuaternion in_rot = rotZ(0.3f);
    const LLVector3 in_pos(1.5f, -0.f, 2.25f); // -0.f: bit test must see it survive

    LLQuaternion io_rot = in_rot;
    LLVector3 io_pos = in_pos;
    inertialize(s, io_rot, io_pos, 1.f / 60.f, 0.12f, 0.12f, false);

    ensure("first-frame rot passthrough is bitwise", bitwiseEqual(io_rot, in_rot));
    ensure("first-frame pos passthrough is bitwise", bitwiseEqual(io_pos, in_pos));
    ensure("seeded valid", s.mValid);
    ensure("seeded settled", s.mSettled);
    ensure("seeded rot offset is identity", bitwiseEqual(s.mRotOffset, LLQuaternion()));
    ensure("seeded pos offset is zero", bitwiseEqual(s.mPosOffset, LLVector3()));
}

// Invariant 1b: settled + no transition + continuous input => bit-identical
// output every frame (the no-op path composes nothing).
template<> template<>
void alposepolish_test_object::test<3>()
{
    InertiaJoint s;
    const F32 dt = 1.f / 60.f;
    for (S32 i = 0; i < 120; ++i)
    {
        // Continuous motion: 1 rad/s rotation, 0.5 m/s translation.
        const LLQuaternion in_rot = rotZ(1.f * dt * (F32)i);
        const LLVector3 in_pos(0.5f * dt * (F32)i, -0.f, 3.f);

        LLQuaternion io_rot = in_rot;
        LLVector3 io_pos = in_pos;
        inertialize(s, io_rot, io_pos, dt, 0.12f, 0.12f, false);

        ensure("steady rot is bitwise no-op", bitwiseEqual(io_rot, in_rot));
        ensure("steady pos is bitwise no-op", bitwiseEqual(io_pos, in_pos));
        ensure("steady state stays settled", s.mSettled);
    }
}

// Invariant 2: on a transition (flagged or self-detected) the first output
// reproduces the old displayed pose — no snap.
template<> template<>
void alposepolish_test_object::test<4>()
{
    const F32 dt = 1.f / 60.f;
    const LLQuaternion rot_a = rotZ(0.8f);
    const LLVector3 pos_a(1.f, 2.f, 3.f);
    const LLQuaternion rot_b = rotZ(0.1f);
    const LLVector3 pos_b(1.4f, 2.f, 2.6f);

    // (a) caller-flagged transition
    InertiaJoint s;
    for (S32 i = 0; i < 3; ++i)
    {
        LLQuaternion io_rot = rot_a;
        LLVector3 io_pos = pos_a;
        inertialize(s, io_rot, io_pos, dt, 0.12f, 0.12f, false);
    }
    LLQuaternion io_rot = rot_b;
    LLVector3 io_pos = pos_b;
    inertialize(s, io_rot, io_pos, dt, 0.12f, 0.12f, true);
    ensure("flagged transition: first rot output == old displayed",
           angleBetween(io_rot, rot_a) < 1e-4f);
    ensure("flagged transition: first pos output == old displayed",
           (io_pos - pos_a).length() < 1e-4f);
    ensure("flagged transition: offset is live", !s.mSettled);

    // (b) self-detected jump (transition flag NOT passed)
    InertiaJoint s2;
    for (S32 i = 0; i < 3; ++i)
    {
        LLQuaternion r = rot_a;
        LLVector3 p = pos_a;
        inertialize(s2, r, p, dt, 0.12f, 0.12f, false);
    }
    LLQuaternion io_rot2 = rot_b;
    LLVector3 io_pos2 = pos_b;
    inertialize(s2, io_rot2, io_pos2, dt, 0.12f, 0.12f, false);
    ensure("self-detected jump: first rot output == old displayed",
           angleBetween(io_rot2, rot_a) < 1e-4f);
    ensure("self-detected jump: first pos output == old displayed",
           (io_pos2 - pos_a).length() < 1e-4f);
    ensure("self-detected jump: offset is live", !s2.mSettled);
}

// Invariants 3+4: monotone-ish critically damped decay (no oscillation with a
// zero seed velocity), then settle to EXACT bit-parity with the input forever.
template<> template<>
void alposepolish_test_object::test<5>()
{
    const F32 dt = 1.f / 60.f;
    const LLQuaternion rot_a = rotZ(0.8f);
    const LLVector3 pos_a(1.f, 2.f, 3.f);
    const LLQuaternion rot_b; // identity
    const LLVector3 pos_b(1.f, 2.f, 3.5f);

    InertiaJoint s;
    for (S32 i = 0; i < 3; ++i)
    {
        LLQuaternion r = rot_a;
        LLVector3 p = pos_a;
        inertialize(s, r, p, dt, 0.1f, 0.1f, false);
    }
    {
        LLQuaternion r = rot_b;
        LLVector3 p = pos_b;
        inertialize(s, r, p, dt, 0.1f, 0.1f, true);
    }

    F32 prev_rot_mag = quatAngle(s.mRotOffset);
    F32 prev_pos_mag = s.mPosOffset.length();
    S32 settled_frames = 0;
    for (S32 i = 0; i < 300; ++i)
    {
        LLQuaternion io_rot = rot_b;
        LLVector3 io_pos = pos_b;
        inertialize(s, io_rot, io_pos, dt, 0.1f, 0.1f, false);

        const F32 rot_mag = quatAngle(s.mRotOffset);
        const F32 pos_mag = s.mPosOffset.length();
        ensure("rot offset decays monotonically", rot_mag <= prev_rot_mag + 1e-5f);
        ensure("pos offset decays monotonically", pos_mag <= prev_pos_mag + 1e-6f);
        prev_rot_mag = rot_mag;
        prev_pos_mag = pos_mag;

        if (s.mSettled)
        {
            ++settled_frames;
            ensure("settled rot output is bit-identical", bitwiseEqual(io_rot, rot_b));
            ensure("settled pos output is bit-identical", bitwiseEqual(io_pos, pos_b));
            ensure("settled rot offset is exactly identity",
                   bitwiseEqual(s.mRotOffset, LLQuaternion()));
            ensure("settled pos offset is exactly zero",
                   bitwiseEqual(s.mPosOffset, LLVector3()));
        }
    }
    ensure("offset settled within the run and stayed settled", settled_frames >= 100);
}

// Invariant 2 (velocity preservation): a transition captured from a MOVING
// displayed pose seeds the offset velocity, so its first decay step travels
// farther along the old motion direction than a velocity-free capture of the
// same pose.
template<> template<>
void alposepolish_test_object::test<6>()
{
    const F32 dt = 1.f / 60.f;
    const F32 rot_speed = 2.f;  // rad/s about +Z
    const F32 pos_speed = 0.5f; // m/s along +X

    // A: history moving at the speeds above, ending displayed at 1 rad / 0.25 m.
    InertiaJoint a;
    for (S32 i = 0; i <= 30; ++i)
    {
        LLQuaternion r = rotZ(rot_speed * dt * (F32)i);
        LLVector3 p(pos_speed * dt * (F32)i, 0.f, 0.f);
        inertialize(a, r, p, dt, 0.12f, 0.12f, false);
    }
    // B: static history at the SAME final displayed pose.
    InertiaJoint b;
    for (S32 i = 0; i < 5; ++i)
    {
        LLQuaternion r = rotZ(rot_speed * dt * 30.f);
        LLVector3 p(pos_speed * dt * 30.f, 0.f, 0.f);
        inertialize(b, r, p, dt, 0.12f, 0.12f, false);
    }

    // Transition both to a static rest input.
    const LLQuaternion rest_rot;
    const LLVector3 rest_pos;
    {
        LLQuaternion r = rest_rot; LLVector3 p = rest_pos;
        inertialize(a, r, p, dt, 0.12f, 0.12f, true);
        ensure("moving capture: first output holds the old pose",
               angleBetween(r, rotZ(rot_speed * dt * 30.f)) < 1e-3f);
    }
    {
        LLQuaternion r = rest_rot; LLVector3 p = rest_pos;
        inertialize(b, r, p, dt, 0.12f, 0.12f, true);
    }

    // The seeded offset velocity reflects the tracked display velocity.
    ensure("seeded rot offset velocity ~ +2 rad/s about Z",
           a.mRotOffsetVel.mV[VZ] > 1.5f && fabsf(a.mRotOffsetVel.mV[VZ] - rot_speed) < 0.5f);
    ensure("seeded pos offset velocity ~ +0.5 m/s along X",
           a.mPosOffsetVel.mV[VX] > 0.3f);
    ensure("static capture has ~zero seed velocity",
           b.mRotOffsetVel.length() < 1e-3f && b.mPosOffsetVel.length() < 1e-4f);

    // First decay step: the velocity-seeded offset stays farther out (its
    // motion carries through), the velocity-free one decays straight down.
    {
        LLQuaternion r = rest_rot; LLVector3 p = rest_pos;
        inertialize(a, r, p, dt, 0.12f, 0.12f, false);
    }
    {
        LLQuaternion r = rest_rot; LLVector3 p = rest_pos;
        inertialize(b, r, p, dt, 0.12f, 0.12f, false);
    }
    ensure("velocity-seeded rot offset leads the velocity-free one",
           quatAngle(a.mRotOffset) > quatAngle(b.mRotOffset) + 1e-3f);
    ensure("velocity-seeded pos offset leads the velocity-free one",
           a.mPosOffset.mV[VX] > b.mPosOffset.mV[VX] + 1e-4f);
}

// Invariant 5: resetJoint() forgets everything; the next call reseeds and
// passes the raw input through bit-identically.
template<> template<>
void alposepolish_test_object::test<7>()
{
    const F32 dt = 1.f / 60.f;
    InertiaJoint s;
    for (S32 i = 0; i < 3; ++i)
    {
        LLQuaternion r = rotZ(0.8f);
        LLVector3 p(1.f, 2.f, 3.f);
        inertialize(s, r, p, dt, 0.12f, 0.12f, false);
    }
    {
        LLQuaternion r; LLVector3 p; // big jump -> live offset
        inertialize(s, r, p, dt, 0.12f, 0.12f, true);
    }
    ensure("offset live before reset", !s.mSettled);

    resetJoint(s);
    ensure("reset clears valid", !s.mValid);

    const LLQuaternion in_rot = rotZ(0.42f);
    const LLVector3 in_pos(-4.f, 0.25f, -0.f);
    LLQuaternion io_rot = in_rot;
    LLVector3 io_pos = in_pos;
    inertialize(s, io_rot, io_pos, dt, 0.12f, 0.12f, false);
    ensure("post-reset rot passthrough is bitwise", bitwiseEqual(io_rot, in_rot));
    ensure("post-reset pos passthrough is bitwise", bitwiseEqual(io_pos, in_pos));
    ensure("post-reset settled with identity offset",
           s.mSettled && bitwiseEqual(s.mRotOffset, LLQuaternion()) &&
           bitwiseEqual(s.mPosOffset, LLVector3()));
}

// Invariant 6: dt <= 0 / non-finite dt never produces NaN. Settled path stays
// bit-parity; a live offset holds (no advance) and remains finite.
template<> template<>
void alposepolish_test_object::test<8>()
{
    const F32 dt = 1.f / 60.f;
    const F32 nan = std::numeric_limits<F32>::quiet_NaN();
    const F32 inf = std::numeric_limits<F32>::infinity();

    // Settled state + bad dt => still a bitwise no-op.
    InertiaJoint s;
    const LLQuaternion in_rot = rotZ(0.3f);
    const LLVector3 in_pos(1.f, -0.f, 2.f);
    {
        LLQuaternion r = in_rot; LLVector3 p = in_pos;
        inertialize(s, r, p, dt, 0.12f, 0.12f, false); // seed
    }
    const F32 bad_dts[] = { 0.f, -1.f, nan, inf };
    for (F32 bad : bad_dts)
    {
        LLQuaternion io_rot = in_rot;
        LLVector3 io_pos = in_pos;
        inertialize(s, io_rot, io_pos, bad, 0.12f, 0.12f, false);
        ensure("settled + bad dt: rot bitwise no-op", bitwiseEqual(io_rot, in_rot));
        ensure("settled + bad dt: pos bitwise no-op", bitwiseEqual(io_pos, in_pos));
        ensure("settled + bad dt: still settled", s.mSettled);
    }

    // Live offset + bad dt => offset holds (no advance), everything finite.
    InertiaJoint s2;
    {
        LLQuaternion r = rotZ(0.8f); LLVector3 p(1.f, 2.f, 3.f);
        inertialize(s2, r, p, dt, 0.12f, 0.12f, false); // seed
    }
    {
        LLQuaternion r; LLVector3 p;
        inertialize(s2, r, p, dt, 0.12f, 0.12f, true); // capture offset
    }
    const F32 rot_mag_before = quatAngle(s2.mRotOffset);
    const F32 pos_mag_before = s2.mPosOffset.length();
    for (F32 bad : bad_dts)
    {
        LLQuaternion io_rot;
        LLVector3 io_pos;
        inertialize(s2, io_rot, io_pos, bad, 0.12f, 0.12f, false);
        ensure("live + bad dt: output rot finite", finiteQuat(io_rot));
        ensure("live + bad dt: output pos finite", finiteVec(io_pos));
        ensure("live + bad dt: state finite",
               finiteQuat(s2.mRotOffset) && finiteVec(s2.mPosOffset) &&
               finiteVec(s2.mRotOffsetVel) && finiteVec(s2.mPosOffsetVel));
        ensure("live + bad dt: rot offset did not advance",
               fabsf(quatAngle(s2.mRotOffset) - rot_mag_before) < 1e-4f);
        ensure("live + bad dt: pos offset did not advance",
               fabsf(s2.mPosOffset.length() - pos_mag_before) < 1e-5f);
    }
    // And a subsequent good frame decays normally.
    {
        LLQuaternion io_rot;
        LLVector3 io_pos;
        inertialize(s2, io_rot, io_pos, dt, 0.12f, 0.12f, false);
        ensure("recovery frame decays", quatAngle(s2.mRotOffset) < rot_mag_before);
        ensure("recovery frame is finite", finiteQuat(io_rot) && finiteVec(io_pos));
    }
}

// Invariant 6 (half-life): non-finite or <= 0 half-lives snap the channel
// closed instead of producing NaN — the output falls back to the raw input.
template<> template<>
void alposepolish_test_object::test<9>()
{
    const F32 dt = 1.f / 60.f;
    const F32 nan = std::numeric_limits<F32>::quiet_NaN();

    InertiaJoint s;
    {
        LLQuaternion r = rotZ(0.8f); LLVector3 p(1.f, 2.f, 3.f);
        inertialize(s, r, p, dt, 0.12f, 0.12f, false); // seed
    }
    {
        LLQuaternion r; LLVector3 p;
        inertialize(s, r, p, dt, 0.12f, 0.12f, true); // capture offset
    }
    ensure("offset live before bad half-life", !s.mSettled);

    // Small enough steps from the last input (identity / zero) that the
    // self-jump detector stays quiet; the bad half-life path is what settles.
    const LLQuaternion in_rot = rotZ(0.05f);
    const LLVector3 in_pos(0.005f, 0.f, -0.f);
    LLQuaternion io_rot = in_rot;
    LLVector3 io_pos = in_pos;
    inertialize(s, io_rot, io_pos, dt, nan, 0.f, false);

    ensure("bad half-life: rot output falls back to raw input bitwise",
           bitwiseEqual(io_rot, in_rot));
    ensure("bad half-life: pos output falls back to raw input bitwise",
           bitwiseEqual(io_pos, in_pos));
    ensure("bad half-life: settled with exact identity/zero offsets",
           s.mSettled && bitwiseEqual(s.mRotOffset, LLQuaternion()) &&
           bitwiseEqual(s.mPosOffset, LLVector3()));
}

// ---------------------------------------------------------------------------
// Milestone 2 — ALContactStab::updateFoot (contact inference + world lock)
// ---------------------------------------------------------------------------
using ALContactStab::ContactFoot;
using ALContactStab::ContactParams;
using ALContactStab::updateFoot;
using ALContactStab::resetFoot;

namespace
{
constexpr F32 CONTACT_DT = 1.f / 60.f;

// Step the foot once and return the held position.
inline LLVector3 stepFoot(ContactFoot& f, const ContactParams& p,
                          const LLVector3& pos, F32 ground_z, F32 dt,
                          bool& planted)
{
    return updateFoot(f, p, pos, ground_z, dt, planted);
}

// Feed a stationary foot until it plants (or the frame budget runs out).
// Returns the number of post-seed frames it took to plant, or -1.
inline S32 framesToPlant(ContactFoot& f, const ContactParams& p,
                         const LLVector3& pos, F32 ground_z, S32 max_frames)
{
    bool planted = false;
    for (S32 i = 1; i <= max_frames; ++i)
    {
        stepFoot(f, p, pos, ground_z, CONTACT_DT, planted);
        if (planted)
        {
            return i;
        }
    }
    return -1;
}
} // namespace

// Contact 1: the first-ever call seeds mLastPos and passes the raw sample
// through, not planted.
template<> template<>
void alposepolish_test_object::test<10>()
{
    ContactFoot f;
    const ContactParams p;
    const LLVector3 in_pos(3.25f, -1.5f, 0.05f);

    bool planted = true; // wrong on purpose: updateFoot must overwrite it
    const LLVector3 out = updateFoot(f, p, in_pos, 0.f, CONTACT_DT, planted);

    ensure("seed frame returns the raw foot pos", bitwiseEqual(out, in_pos));
    ensure("seed frame is not planted", !planted && !f.mPlanted);
    ensure("seed frame marks state valid", f.mValid);
    ensure("seed frame stores mLastPos", bitwiseEqual(f.mLastPos, in_pos));
    ensure("seed frame accrues no dwell",
           f.mBelowSpeedSec == 0.f && f.mAboveSpeedSec == 0.f);
}

// Contact 2 (HYSTERESIS): still for >= plant dwell => planted; then a blip
// BETWEEN the plant and release speeds must not flip it; only sustained speed
// above the release threshold for >= release dwell releases. No flicker.
template<> template<>
void alposepolish_test_object::test<11>()
{
    ContactFoot f;
    const ContactParams p; // 0.15/0.45 m/s, 0.06/0.04 s dwell
    const F32 ground_z = 0.f;
    LLVector3 pos(1.f, 2.f, 0.05f); // 5 cm above ground: height evidence ok
    bool planted = false;

    stepFoot(f, p, pos, ground_z, CONTACT_DT, planted); // seed

    // Under the plant dwell (3 frames = 0.05 s < 0.06 s): still swing.
    for (S32 i = 0; i < 3; ++i)
    {
        stepFoot(f, p, pos, ground_z, CONTACT_DT, planted);
        ensure("still under plant dwell: not planted yet", !planted);
    }
    // 4th still frame crosses 0.06 s: planted.
    stepFoot(f, p, pos, ground_z, CONTACT_DT, planted);
    ensure("still past plant dwell: planted", planted && f.mPlanted);

    // Blip INSIDE the hysteresis band (0.30 m/s, between 0.15 and 0.45) for
    // 6 frames (0.1 s, far beyond the release dwell): must stay planted.
    const LLVector3 band_step(0.30f * CONTACT_DT, 0.f, 0.f);
    for (S32 i = 0; i < 6; ++i)
    {
        pos += band_step;
        stepFoot(f, p, pos, ground_z, CONTACT_DT, planted);
        ensure("in-band speed blip never releases", planted && f.mPlanted);
        ensure("in-band blip accrues no release dwell", f.mAboveSpeedSec == 0.f);
    }

    // Above the release threshold (1.0 m/s) — but under the release dwell
    // (2 frames = 0.033 s < 0.04 s): still planted.
    const LLVector3 fast_step(1.0f * CONTACT_DT, 0.f, 0.f);
    for (S32 i = 0; i < 2; ++i)
    {
        pos += fast_step;
        stepFoot(f, p, pos, ground_z, CONTACT_DT, planted);
        ensure("fast under release dwell: still planted", planted && f.mPlanted);
    }
    // 3rd fast frame crosses 0.04 s: released, and no re-flicker afterwards.
    pos += fast_step;
    stepFoot(f, p, pos, ground_z, CONTACT_DT, planted);
    ensure("fast past release dwell: released", !planted && !f.mPlanted);
    for (S32 i = 0; i < 4; ++i)
    {
        pos += fast_step;
        stepFoot(f, p, pos, ground_z, CONTACT_DT, planted);
        ensure("stays released while moving fast", !planted);
    }
}

// Contact 3 (LOCK): while planted the held position is the STABLE plant-time
// mLockPos even as the raw foot drifts a few mm per frame (anti-footskate);
// on release it returns the raw pos again and re-tracks.
template<> template<>
void alposepolish_test_object::test<12>()
{
    ContactFoot f;
    const ContactParams p;
    const F32 ground_z = 0.f;
    LLVector3 pos(4.f, -2.f, 0.04f);
    bool planted = false;

    stepFoot(f, p, pos, ground_z, CONTACT_DT, planted); // seed
    ensure("lock test plants", framesToPlant(f, p, pos, ground_z, 30) > 0);
    const LLVector3 lock = f.mLockPos;
    ensure("lock captured at the plant position", (lock - pos).length() < 1e-6f);

    // Raw foot drifts 2 mm/frame (0.12 m/s, under the plant speed): the held
    // output must remain the ORIGINAL lock, not the drifting raw pos.
    const LLVector3 drift(0.002f, 0.f, 0.f);
    for (S32 i = 0; i < 20; ++i)
    {
        pos += drift;
        const LLVector3 out = stepFoot(f, p, pos, ground_z, CONTACT_DT, planted);
        ensure("drifting foot stays planted", planted);
        ensure("held position is the stable lock", (out - lock).length() < 1e-6f);
        ensure("lock does not creep with the drift",
               (f.mLockPos - lock).length() < 1e-6f);
    }
    // The raw foot has drifted 40 mm off the lock by now; the lock held.
    ensure("raw foot really diverged from the lock",
           (pos - lock).length() > 0.035f);

    // Release with sustained fast motion; the output snaps back to raw.
    const LLVector3 fast_step(1.0f * CONTACT_DT, 0.f, 0.f);
    LLVector3 out;
    for (S32 i = 0; i < 10 && planted; ++i)
    {
        pos += fast_step;
        out = stepFoot(f, p, pos, ground_z, CONTACT_DT, planted);
    }
    ensure("sustained fast motion releases", !planted);
    ensure("released output re-tracks the raw pos", bitwiseEqual(out, pos));

    // And a later still period re-plants at the NEW location.
    const S32 replant = framesToPlant(f, p, pos, ground_z, 30);
    ensure("re-plants after coming to rest", replant > 0);
    ensure("new lock is at the new rest position",
           (f.mLockPos - pos).length() < 1e-6f);
    ensure("new lock is not the old lock", (f.mLockPos - lock).length() > 0.03f);
}

// Contact 4 (HEIGHT GATE): a foot far above ground cannot plant even at low
// speed; with ground_z = -FLT_MAX height evidence is ignored (velocity-only).
template<> template<>
void alposepolish_test_object::test<13>()
{
    const ContactParams p;
    const LLVector3 lifted_pos(0.f, 0.f, 1.0f); // 1 m above ground_z = 0

    // Stationary but 1.0 m above ground (> mMaxGroundDist = 0.15): never plants.
    ContactFoot f;
    bool planted = false;
    stepFoot(f, p, lifted_pos, 0.f, CONTACT_DT, planted); // seed
    for (S32 i = 0; i < 120; ++i)
    {
        const LLVector3 out = stepFoot(f, p, lifted_pos, 0.f, CONTACT_DT, planted);
        ensure("lifted slow foot never plants", !planted && !f.mPlanted);
        ensure("lifted foot passes raw pos through", bitwiseEqual(out, lifted_pos));
    }
    ensure("lifted foot accrues no plant dwell", f.mBelowSpeedSec == 0.f);

    // Just inside the ground distance: plants normally.
    ContactFoot f2;
    const LLVector3 near_pos(0.f, 0.f, 0.10f); // 0.10 <= 0.15
    stepFoot(f2, p, near_pos, 0.f, CONTACT_DT, planted); // seed
    ensure("near-ground foot plants", framesToPlant(f2, p, near_pos, 0.f, 30) > 0);

    // ground_z = -FLT_MAX: height ignored, the same lifted foot plants on
    // velocity evidence alone.
    ContactFoot f3;
    stepFoot(f3, p, lifted_pos, -FLT_MAX, CONTACT_DT, planted); // seed
    ensure("ground_z = -FLT_MAX ignores height",
           framesToPlant(f3, p, lifted_pos, -FLT_MAX, 30) > 0);
    ensure("velocity-only lock is at the foot",
           (f3.mLockPos - lifted_pos).length() < 1e-6f);
}

// Contact 5: dt <= 0 / non-finite dt or a non-finite position are safe — no
// NaN in state or output, no timer advance, no spurious plant or release.
template<> template<>
void alposepolish_test_object::test<14>()
{
    const ContactParams p;
    const F32 nan = std::numeric_limits<F32>::quiet_NaN();
    const F32 inf = std::numeric_limits<F32>::infinity();
    const F32 bad_dts[] = { 0.f, -1.f, nan, inf };
    const LLVector3 still_pos(2.f, 3.f, 0.05f);
    bool planted = false;

    // (a) Unplanted foot + bad dt frames, far more of them than the plant
    // dwell would need: dwell must not accrue, no plant, output finite.
    ContactFoot f;
    stepFoot(f, p, still_pos, 0.f, CONTACT_DT, planted); // seed
    for (S32 i = 0; i < 8; ++i)
    {
        const F32 bad = bad_dts[i % 4];
        const LLVector3 out = stepFoot(f, p, still_pos, 0.f, bad, planted);
        ensure("bad dt: no spurious plant", !planted && !f.mPlanted);
        ensure("bad dt: output is the raw pos", bitwiseEqual(out, still_pos));
        ensure("bad dt: no dwell accrual",
               f.mBelowSpeedSec == 0.f && f.mAboveSpeedSec == 0.f);
        ensure("bad dt: state stays finite",
               finiteVec(f.mLastPos) && finiteVec(f.mLockPos) &&
               std::isfinite(f.mBelowSpeedSec) && std::isfinite(f.mAboveSpeedSec));
    }
    // Good frames afterwards still plant normally.
    ensure("recovers and plants after bad dt", framesToPlant(f, p, still_pos, 0.f, 30) > 0);

    // (b) Non-finite position samples: unplanted foot returns its last finite
    // sample; a planted foot holds its lock; nothing flips, nothing goes NaN.
    const LLVector3 nan_pos(nan, 0.f, 0.f);
    const LLVector3 inf_pos(0.f, inf, 0.f);
    ContactFoot f2;
    stepFoot(f2, p, still_pos, 0.f, CONTACT_DT, planted); // seed
    {
        const LLVector3 out = stepFoot(f2, p, nan_pos, 0.f, CONTACT_DT, planted);
        ensure("nan pos (unplanted): finite fallback output", finiteVec(out));
        ensure("nan pos (unplanted): falls back to last sample",
               bitwiseEqual(out, still_pos));
        ensure("nan pos (unplanted): no plant", !planted);
        ensure("nan pos: mLastPos untouched", bitwiseEqual(f2.mLastPos, still_pos));
    }
    ensure("plants after the nan sample", framesToPlant(f2, p, still_pos, 0.f, 30) > 0);
    const LLVector3 lock = f2.mLockPos;
    for (S32 i = 0; i < 6; ++i)
    {
        const LLVector3& bad_pos = (i % 2 == 0) ? nan_pos : inf_pos;
        const LLVector3 out = stepFoot(f2, p, bad_pos, 0.f, CONTACT_DT, planted);
        ensure("bad pos (planted): stays planted", planted && f2.mPlanted);
        ensure("bad pos (planted): returns the finite lock",
               finiteVec(out) && bitwiseEqual(out, lock));
        ensure("bad pos (planted): state stays finite",
               finiteVec(f2.mLastPos) && finiteVec(f2.mLockPos));
    }

    // (c) Planted foot + bad dt: holds the lock, no spurious release.
    for (F32 bad : bad_dts)
    {
        const LLVector3 out = stepFoot(f2, p, still_pos, 0.f, bad, planted);
        ensure("bad dt (planted): stays planted", planted && f2.mPlanted);
        ensure("bad dt (planted): returns the lock", bitwiseEqual(out, lock));
        ensure("bad dt (planted): no release dwell", f2.mAboveSpeedSec == 0.f);
    }
}

// Contact 6: resetFoot() forgets everything; the next call reseeds (raw
// passthrough, not planted, full dwell required again).
template<> template<>
void alposepolish_test_object::test<15>()
{
    ContactFoot f;
    const ContactParams p;
    const LLVector3 pos_a(1.f, 1.f, 0.05f);
    bool planted = false;

    stepFoot(f, p, pos_a, 0.f, CONTACT_DT, planted); // seed
    ensure("reset test plants first", framesToPlant(f, p, pos_a, 0.f, 30) > 0);

    resetFoot(f);
    ensure("reset clears valid", !f.mValid);
    ensure("reset clears planted", !f.mPlanted);
    ensure("reset clears timers",
           f.mBelowSpeedSec == 0.f && f.mAboveSpeedSec == 0.f);

    // Next call reseeds at a NEW position: raw passthrough, not planted, and
    // it takes the full plant dwell again before contact re-engages.
    const LLVector3 pos_b(-3.f, 7.f, 0.02f);
    const LLVector3 out = stepFoot(f, p, pos_b, 0.f, CONTACT_DT, planted);
    ensure("post-reset seed returns raw pos", bitwiseEqual(out, pos_b));
    ensure("post-reset seed not planted", !planted && !f.mPlanted);
    ensure("post-reset seed stores mLastPos", bitwiseEqual(f.mLastPos, pos_b));

    bool early_plant = false;
    for (S32 i = 0; i < 3; ++i) // 0.05 s < 0.06 s dwell
    {
        stepFoot(f, p, pos_b, 0.f, CONTACT_DT, planted);
        early_plant = early_plant || planted;
    }
    ensure("post-reset requires the full dwell again", !early_plant);
    stepFoot(f, p, pos_b, 0.f, CONTACT_DT, planted);
    ensure("post-reset plants after the full dwell", planted);
    ensure("post-reset lock is at the new position",
           (f.mLockPos - pos_b).length() < 1e-6f);
}

} // namespace tut
