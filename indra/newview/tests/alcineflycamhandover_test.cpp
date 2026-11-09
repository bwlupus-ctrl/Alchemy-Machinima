/** Bone Lock flycam arbitration: no avatar, renderer, input device or GL. */
#include "linden_common.h"
#include "../test/lltut.h"
#include "../alcameradispatch.h"
#include "../alcineflycamhandover.h"

namespace tut
{
struct alcineflycamhandover_data {};
typedef test_group<alcineflycamhandover_data> handover_group;
typedef handover_group::object handover_object;
handover_group handover_tests("alcineflycamhandover");

template<> template<> void handover_object::test<1>()
{
    set_test_name("Bone Lock yields to flycam even when the actor cannot resolve");
    for (bool resolved : {false, true})
    {
        const auto decision = alResolveCineFlycamOwner(true, true, true, true, false, resolved);
        ensure("planted CINE-first precedence must fail", decision.owner == ECameraOwner::FLYCAM);
        ensure("mount suspended independently of target availability", decision.handover);
        ensure("unresolved actor selects free flycam", decision.orbit == resolved);
        const auto lost = alResolveCineFlycamOwner(false, true, true, true, false, resolved);
        ensure("loss cannot strand camera ownership", lost.owner == ECameraOwner::FLYCAM);
        ensure("loss keeps handover configured", lost.handover && lost.orbit == resolved);
    }
}

template<> template<> void handover_object::test<2>()
{
    set_test_name("kill switch, other modes, user Orbit and release");
    const auto disabled = alResolveCineFlycamOwner(true, true, true, false, false, true);
    ensure("kill switch restores CINE", disabled.owner == ECameraOwner::CINE &&
        !disabled.handover && !disabled.orbit);
    const auto other = alResolveCineFlycamOwner(true, false, true, true, false, true);
    ensure("other cinematic modes still win", other.owner == ECameraOwner::CINE && !other.handover);
    const auto user_orbit = alResolveCineFlycamOwner(true, true, true, true, true, true);
    ensure("user Orbit must not veto Bone Lock handover", user_orbit.owner == ECameraOwner::FLYCAM &&
        user_orbit.handover && user_orbit.orbit);
    const auto released = alResolveCineFlycamOwner(true, true, false, true, false, true);
    ensure("Bone Lock resumes on release", released.owner == ECameraOwner::CINE &&
        !released.handover && !released.orbit);
    const auto off = alResolveCineFlycamOwner(false, false, false, true, false, true);
    ensure("agent fallback", off.owner == ECameraOwner::AGENT && !off.orbit);
}

template<> template<> void handover_object::test<3>()
{
    set_test_name("all flight models and 3D cursor use the existing user Orbit decision");
    const auto handover = alResolveCineFlycamOwner(true, true, true, true, false, true);
    const auto user_orbit = alResolveCineFlycamOwner(false, false, true, false, true, true);
    for (S32 model : {0, 1, 2})
    for (bool cursor : {false, true})
    for (ALFlycamPoseOwner previous : {ALFlycamPoseOwner::CLASSIC, ALFlycamPoseOwner::DRONE,
                                     ALFlycamPoseOwner::WEIGHTED, ALFlycamPoseOwner::ORBIT})
    {
        const auto frame = alResolveFlycamFrame(model, cursor, handover.orbit, previous, true, false);
        const auto manual = alResolveFlycamFrame(model, cursor, user_orbit.orbit, previous, true, false);
        ensure("orbit owns engage frame for Classic/Drone/Weighted", frame.frameOwner == ALFlycamPoseOwner::ORBIT);
        ensure("orbit remains selected", frame.selectedOwner == ALFlycamPoseOwner::ORBIT);
        ensure("manual Orbit edge and cursor rules", frame.edge == manual.edge &&
            frame.weightedEdge == manual.weightedEdge && frame.resetOperator == manual.resetOperator &&
            frame.discardDroneAccumulator == manual.discardDroneAccumulator &&
            frame.discardClassicShadowPose == manual.discardClassicShadowPose &&
            frame.seedDrone == manual.seedDrone && frame.foreignDroneRegain == manual.foreignDroneRegain);
        const auto missing = alResolveCineFlycamOwner(true, true, true, true, false, false);
        const auto free = alResolveFlycamFrame(model, cursor, missing.orbit, previous, true, false);
        ensure("lost anchor stops orbit immediately", free.frameOwner != ALFlycamPoseOwner::ORBIT);
        if (cursor)
            ensure("3D cursor free flight stays Classic", free.selectedOwner == ALFlycamPoseOwner::CLASSIC);
    }
}

template<> template<> void handover_object::test<4>()
{
    set_test_name("exhaustive lower-priority ownership truth table");
    for (S32 bits = 0; bits < 64; ++bits)
    {
        const bool cine = (bits & 1) != 0;
        const bool bone = (bits & 2) != 0;
        const bool fly = (bits & 4) != 0;
        const bool enabled = (bits & 8) != 0;
        const bool user_orbit = (bits & 16) != 0;
        const bool resolved = (bits & 32) != 0;
        const auto decision = alResolveCineFlycamOwner(cine, bone, fly, enabled, user_orbit, resolved);
        const bool transfer = bone && fly && enabled;
        const auto expected = cine && !transfer ? ECameraOwner::CINE :
            fly ? ECameraOwner::FLYCAM : ECameraOwner::AGENT;
        ensure("owner precedence", decision.owner == expected);
        ensure("runtime-only handover", decision.handover == transfer);
        ensure("resolved orbit only under FLYCAM", decision.orbit ==
            (expected == ECameraOwner::FLYCAM && resolved && (transfer || user_orbit)));
    }
}

template<> template<> void handover_object::test<5>()
{
    set_test_name("authored Bone Lock pose and pre-lens FOV survive zero input");
    LLQuaternion anchorRotation;
    anchorRotation.setEulerAngles(0.3f, -0.5f, 1.2f);
    LLQuaternion aim;
    aim.setEulerAngles(-0.2f, 0.4f, -0.7f);
    const LLVector3 anchor(5.f, -7.f, 2.f);
    for (const LLVector3& offset : {LLVector3(0.1f, 0.f, 0.05f),
             LLVector3(0.001f, 0.f, 0.15f), LLVector3(40.f, 0.f, 0.f), LLVector3()})
    {
        const LLVector3 initial = anchor + offset * anchorRotation;
        ALBoneLockOrbit orbit;
        orbit.seed(initial, aim, anchor, anchorRotation);
        LLVector3 position;
        LLQuaternion rotation;
        orbit.move(0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f, 30.f,
            anchor, anchorRotation, position, rotation);
        ensure("zero input preserves authored position outside limits",
            (position - initial).magVec() < 0.00001f);
        for (const LLVector3& axis : {LLVector3(1.f, 0.f, 0.f), LLVector3(0.f, 1.f, 0.f)})
            ensure("zero input preserves Bone Lock aim, not look-at",
                (axis * rotation - axis * aim).magVec() < 0.00001f);
        ensure("actual initial radius retained", fabsf(orbit.radius - offset.magVec()) < 0.00001f);
        orbit.move(0.02f, 0.f, 0.f, 0.f, 0.f, 0.f, 1.f, 30.f,
            anchor, anchorRotation, position, rotation);
        ensure("yaw alone never projects radius", fabsf(orbit.radius - offset.magVec()) < 0.00001f);
        ensure("yaw alone never projects initial elevation",
            fabsf(((position - anchor) * ~anchorRotation).mV[VZ] - offset.mV[VZ]) < 0.00001f);
    }
    ensure("outside radius moves inward without projection", fabsf(alHandoverLimitedMove(0.1f, 0.02f, 1.f, 30.f) - 0.12f) < 0.000001f);
    ensure("outside radius cannot move further out", alHandoverLimitedMove(40.f, 1.f, 1.f, 30.f) == 40.f);
    for (F32 shift : {0.f, 0.08f, -0.04f, 0.4f})
    for (F32 multiplier : {0.1f, 1.f, 4.f})
    for (F32 captured : {0.1f, 0.8f, 2.f})
    {
        const F32 zoom = alSeedHandoverZoom(captured, shift, multiplier);
        const F32 rendered = 2.f * atanf(tanf(zoom * multiplier * 0.5f) + shift);
        ensure("lens and Orbit zoom compensated", fabsf(rendered - captured) < 0.000001f);
    }
}

template<> template<> void handover_object::test<6>()
{
    set_test_name("foreign owners reseed independently of CineCam suspension");
    ensure("first transfer seeds", alSeedBoneLockFlycam(true, false, false, ECameraOwner::CINE));
    ensure("continuous flycam does not reseed", !alSeedBoneLockFlycam(true, true, false, ECameraOwner::FLYCAM));
    for (ECameraOwner owner : {ECameraOwner::RECORDER, ECameraOwner::PATH, ECameraOwner::PILOT,
                              ECameraOwner::AGENT, ECameraOwner::CINE})
        ensure("latched suspension cannot suppress re-entry seed", alSeedBoneLockFlycam(true, true, true, owner));
    for (bool userOrbit : {false, true})
    {
        const auto held = alResolveCineFlycamOwner(true, true, true, true, userOrbit, true, false);
        ensure("not-ready flycam leaves CINE writing", held.owner == ECameraOwner::CINE && !held.handover);
        const auto ready = alResolveCineFlycamOwner(true, true, true, true, userOrbit, true, true);
        ensure("ready flycam accepts shot with either Orbit setting", ready.owner == ECameraOwner::FLYCAM && ready.orbit);
    }
}

template<> template<> void handover_object::test<7>()
{
    set_test_name("tiny pitch near either pole has a tiny orientation change");
    for (F32 sign : {-1.f, 1.f})
    for (F32 azimuth : {0.f, 0.8f, -2.f})
    {
        const F32 elevation = sign * 89.99f * DEG_TO_RAD;
        const LLVector3 offset(cosf(azimuth) * cosf(elevation),
            sinf(azimuth) * cosf(elevation), sinf(elevation));
        ALBoneLockOrbit orbit;
        LLQuaternion anchor;
        anchor.setEulerAngles(0.3f, -0.5f, 0.7f);
        LLQuaternion initial;
        initial.setEulerAngles(-0.2f, 0.4f, 0.1f);
        orbit.seed(offset * anchor, initial, LLVector3(), anchor);
        LLVector3 position;
        LLQuaternion rotation;
        orbit.move(0.f, -sign * 0.09f * DEG_TO_RAD, 0.f, 0.f, 0.f, 0.f,
            0.1f, 30.f, LLVector3(), anchor, position, rotation);
        const F32 dot = fabsf(initial.mQ[0] * rotation.mQ[0] + initial.mQ[1] * rotation.mQ[1] +
            initial.mQ[2] * rotation.mQ[2] + initial.mQ[3] * rotation.mQ[3]);
        ensure("89.99 to 89.9 changes orientation by less than one degree",
            2.f * acosf(llclamp(dot, 0.f, 1.f)) < DEG_TO_RAD);
        ensure("pitch reaches requested elevation without projection",
            fabsf(orbit.elevation - sign * 89.9f * DEG_TO_RAD) < 0.00001f);
    }
}
}
