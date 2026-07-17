/**
 * @file llcinematiccamera.cpp
 * @brief Automated cinematic camera: bone-lock (GoPro) and motion patterns.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Phoenix / Firestorm Viewer Source Code
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llcinematiccamera.h"

#include <cmath>

#include "llcameraoperator.h"
#include "llappviewer.h"            // gFrameIntervalSeconds
#include "lldirectorcast.h"         // [Director] Subject A/B
#include "lljoint.h"
#include "llmath.h"
#include "llselectmgr.h"
#include "llviewerobjectlist.h"     // gObjectList (locked follow target)
#include "llviewercamera.h"
#include "llviewercontrol.h"        // gSavedSettings, LLCachedControl
#include "llviewerobject.h"
#include "llvoavatar.h"
#include "llvoavatarself.h"         // gAgentAvatarp, isAgentAvatarValid()
#include "m3math.h"

namespace
{
constexpr F32 PHASE_WRAP = 4096.f;  // seconds of pattern clock before wrap

inline F32 cc_frac(F32 x)               { return x - floorf(x); }
inline F32 cc_lerp(F32 a, F32 b, F32 u) { return a + (b - a) * u; }

// small periodic value noise (same construction as the camera operator)
F32 cc_hash(F32 p)
{
    p = p - floorf(p / 256.f) * 256.f;
    p = cc_frac(p * 0.1031f);
    p *= p + 33.33f;
    p *= p + p;
    return cc_frac(p);
}

F32 cc_noise(F32 x)
{
    F32 i = floorf(x);
    F32 f = cc_frac(x);
    F32 u = f * f * f * (f * (f * 6.f - 15.f) + 10.f);
    return cc_lerp(cc_hash(i), cc_hash(i + 1.f), u) - 0.5f;
}

F32 cc_fbm(F32 x)
{
    return (cc_noise(x) + 0.5f * cc_noise(x * 2.f + 17.3f) + 0.25f * cc_noise(x * 4.f + 41.9f)) * 1.4f;
}

// build a level (Z-up) look-at orientation: X=at, Y=left, Z=up
LLQuaternion cc_lookAt(const LLVector3& from, const LLVector3& to)
{
    LLVector3 at = to - from;
    if (at.magVecSquared() < 1e-8f)
    {
        return LLQuaternion();
    }
    at.normVec();
    LLVector3 up_world(0.f, 0.f, 1.f);
    LLVector3 left = up_world % at;     // cross: y = z x x
    if (left.magVecSquared() < 1e-6f)   // looking straight up/down
    {
        left = LLVector3(0.f, 1.f, 0.f);
    }
    left.normVec();
    LLVector3 up = at % left;
    LLMatrix3 mat;
    mat.setRows(at, left, up);
    return LLQuaternion(mat);
}

// strip roll from an orientation, keeping its at-axis (horizon lock)
LLQuaternion cc_levelHorizon(const LLQuaternion& q)
{
    LLMatrix3 m(q);
    LLVector3 at(m.mMatrix[0]);
    LLVector3 up_world(0.f, 0.f, 1.f);
    LLVector3 left = up_world % at;
    if (left.magVecSquared() < 1e-6f)
    {
        return q;   // degenerate (looking straight up/down): keep as-is
    }
    left.normVec();
    LLVector3 up = at % left;
    LLMatrix3 level;
    level.setRows(at, left, up);
    return LLQuaternion(level);
}
} // anonymous namespace

// ---------------------------------------------------------------------------
LLCinematicCamera& LLCinematicCamera::instance()
{
    static LLCinematicCamera sInstance;
    return sInstance;
}

// session-only locked follow subject
static LLUUID sCinematicFollowTarget;

//static
void LLCinematicCamera::toggleFollowTarget(const LLUUID& id)
{
    sCinematicFollowTarget = (sCinematicFollowTarget == id) ? LLUUID::null : id;
}

//static
bool LLCinematicCamera::isFollowTarget(const LLUUID& id)
{
    return id.notNull() && sCinematicFollowTarget == id;
}

bool LLCinematicCamera::isActive() const
{
    static LLCachedControl<bool> enabled(gSavedSettings, "CinematicCamEnabled", false);
    static LLCachedControl<S32>  mode(gSavedSettings, "CinematicCamMode", 1);
    if (!enabled || (S32)mode <= MODE_OFF || (S32)mode > MODE_PEDESTAL)
    {
        return false;
    }
    return resolveTarget() != nullptr;
}

LLVOAvatar* LLCinematicCamera::resolveTarget() const
{
    // [Director] Subject A beats everything while set and alive; unset (or
    // out-of-world) falls through to the stock chain, so an empty cast is
    // byte-identical to pre-Director behavior
    if (LLVOAvatar* subject = LLDirectorCast::instance().resolveSubjectA())
    {
        return subject;
    }

    // locked follow subject wins (session-only, set from the avatar context
    // menu); a dead/derezzed subject falls through rather than dropping out
    if (sCinematicFollowTarget.notNull())
    {
        LLViewerObject* obj = gObjectList.findObject(sCinematicFollowTarget);
        LLVOAvatar* av = obj ? obj->asAvatar() : nullptr;
        if (av && !av->isDead())
        {
            return av;
        }
    }

    static LLCachedControl<bool> use_selected(gSavedSettings, "CinematicCamUseSelected", false);
    if (use_selected)
    {
        LLViewerObject* obj = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
        if (obj)
        {
            LLVOAvatar* av = obj->getAvatar();  // avatar itself or attachment parent
            if (av && !av->isDead())
            {
                return av;
            }
        }
        // fall through to self so the camera doesn't drop out mid-shot
    }
    return isAgentAvatarValid() ? (LLVOAvatar*)gAgentAvatarp : nullptr;
}

// anchor transform for external riders (Flycam Orbit): same target/joint
// resolution as Bone Lock, without the mount/aim trim
bool LLCinematicCamera::resolveAnchor(LLVector3& pos, LLQuaternion& rot, bool level_horizon) const
{
    LLVOAvatar* av = resolveTarget();
    if (!av)
    {
        return false;
    }

    static LLCachedControl<std::string> joint_name(gSavedSettings, "CinematicCamJoint", std::string("mHead"));
    LLJoint* joint = av->getJoint(std::string(joint_name));
    if (!joint)
    {
        joint = av->getJoint("mHead");
    }
    if (joint)
    {
        pos = joint->getWorldPosition();
        rot = joint->getWorldRotation();
    }
    else
    {
        pos = av->getPositionAgent() + LLVector3(0.f, 0.f, 1.f);
        rot = av->getRenderRotation();
    }
    if (level_horizon)
    {
        rot = cc_levelHorizon(rot);
    }
    return true;
}

// ---------------------------------------------------------------------------
// pattern generators (agent region coordinates, Z up)
// ---------------------------------------------------------------------------
void LLCinematicCamera::patternBoneLock(LLVOAvatar* av, F32 /*phase*/,
                                        LLVector3& pos, LLQuaternion& rot, bool& have_rot)
{
    static LLCachedControl<std::string> joint_name(gSavedSettings, "CinematicCamJoint", std::string("mHead"));
    static LLCachedControl<F32> off_fwd(gSavedSettings, "CinematicCamBoneOffsetForward", 0.1f);
    static LLCachedControl<F32> off_left(gSavedSettings, "CinematicCamBoneOffsetLeft", 0.f);
    static LLCachedControl<F32> off_up(gSavedSettings, "CinematicCamBoneOffsetUp", 0.05f);
    static LLCachedControl<F32> aim_yaw(gSavedSettings, "CinematicCamBoneAimYaw", 0.f);
    static LLCachedControl<F32> aim_pitch(gSavedSettings, "CinematicCamBoneAimPitch", 0.f);
    static LLCachedControl<F32> aim_roll(gSavedSettings, "CinematicCamBoneAimRoll", 0.f);
    static LLCachedControl<bool> horizon(gSavedSettings, "CinematicCamBoneHorizonLock", false);

    LLJoint* joint = av->getJoint(std::string(joint_name));
    if (!joint)
    {
        joint = av->getJoint("mHead");
    }
    if (!joint)
    {
        pos = av->getPositionAgent() + LLVector3(0.f, 0.f, 1.f);
        have_rot = false;
        return;
    }

    LLQuaternion jrot = joint->getWorldRotation();
    // aim trim (degrees) applied in the joint's local frame
    LLQuaternion trim;
    trim.setEulerAngles(aim_roll * DEG_TO_RAD, aim_pitch * DEG_TO_RAD, aim_yaw * DEG_TO_RAD);
    rot = trim * jrot;
    if (horizon)
    {
        rot = cc_levelHorizon(rot);
    }
    have_rot = true;

    // local mount offset in the (trimmed) camera frame: X=at, Y=left, Z=up
    LLVector3 offset = LLVector3(off_fwd, off_left, off_up) * rot;
    pos = joint->getWorldPosition() + offset;
}

LLVector3 LLCinematicCamera::patternOrbit(const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> radius(gSavedSettings, "CinematicCamOrbitRadius", 3.f);
    static LLCachedControl<F32> speed(gSavedSettings, "CinematicCamOrbitSpeed", 20.f);   // deg/s
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamOrbitHeight", 0.5f);
    static LLCachedControl<F32> bob(gSavedSettings, "CinematicCamOrbitBob", 0.f);

    const F32 a = phase * speed * DEG_TO_RAD;
    return center + LLVector3(cosf(a) * radius,
                              sinf(a) * radius,
                              height + bob * sinf(a * 2.7f));
}

LLVector3 LLCinematicCamera::patternHover(const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamHoverDistance", 2.5f);
    static LLCachedControl<F32> wander(gSavedSettings, "CinematicCamHoverWander", 1.f);
    static LLCachedControl<F32> speed(gSavedSettings, "CinematicCamHoverSpeed", 0.35f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamHoverHeight", 0.6f);

    // a fly: azimuth drifts on noise, elevation and range breathe on
    // decorrelated noise, plus fine jitter
    const F32 t = phase * speed;
    const F32 az = t * 0.9f + cc_fbm(t * 0.7f) * 3.f;
    const F32 el = cc_fbm(t * 0.55f + 31.7f) * 0.6f;
    const F32 rr = distance * (1.f + 0.25f * cc_fbm(t * 0.8f + 57.1f) * wander);
    LLVector3 p(cosf(az) * cosf(el) * rr,
                sinf(az) * cosf(el) * rr,
                height + sinf(el) * rr * 0.5f);
    // fine wing-jitter
    p += LLVector3(cc_fbm(t * 5.3f + 11.f), cc_fbm(t * 5.9f + 23.f), cc_fbm(t * 6.7f + 47.f)) * 0.06f * wander;
    return center + p;
}

LLVector3 LLCinematicCamera::patternSweep(const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> length(gSavedSettings, "CinematicCamSweepLength", 8.f);
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamSweepDistance", 3.f);
    static LLCachedControl<F32> speed(gSavedSettings, "CinematicCamSweepSpeed", 1.f);    // m/s
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamSweepHeading", 0.f); // deg
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamSweepHeight", 0.5f);
    static LLCachedControl<bool> pingpong(gSavedSettings, "CinematicCamSweepPingPong", true);

    const F32 h = heading * DEG_TO_RAD;
    const LLVector3 dir(cosf(h), sinf(h), 0.f);         // travel direction
    const LLVector3 perp(-sinf(h), cosf(h), 0.f);       // offset from subject

    const F32 len = llmax((F32)length, 0.1f);
    F32 s = phase * llmax((F32)speed, 0.01f) / len;     // path cycles
    F32 t;
    if (pingpong)
    {
        const F32 c = cc_frac(s * 0.5f) * 2.f;          // 0..2
        t = (c < 1.f) ? c : 2.f - c;                    // triangle 0..1..0
    }
    else
    {
        t = cc_frac(s);
    }
    return center + perp * distance + dir * ((t - 0.5f) * len) + LLVector3(0.f, 0.f, height);
}

LLVector3 LLCinematicCamera::patternCrane(const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> radius(gSavedSettings, "CinematicCamCraneRadius", 4.f);
    static LLCachedControl<F32> speed(gSavedSettings, "CinematicCamCraneSpeed", 8.f);    // deg/s
    static LLCachedControl<F32> min_h(gSavedSettings, "CinematicCamCraneMinHeight", 0.2f);
    static LLCachedControl<F32> max_h(gSavedSettings, "CinematicCamCraneMaxHeight", 4.f);
    static LLCachedControl<F32> rise_period(gSavedSettings, "CinematicCamCraneRisePeriod", 14.f);

    const F32 a = phase * speed * DEG_TO_RAD;
    const F32 u = 0.5f + 0.5f * sinf(phase * F_TWO_PI / llmax((F32)rise_period, 1.f));
    return center + LLVector3(cosf(a) * radius, sinf(a) * radius, cc_lerp(min_h, max_h, u));
}

// ---------------------------------------------------------------------------
// film-grammar patterns
// ---------------------------------------------------------------------------
namespace
{
// avatar facing yaw (radians, region frame)
F32 cc_avatarYaw(LLVOAvatar* av)
{
    LLVector3 at = LLVector3(1.f, 0.f, 0.f) * av->getRenderRotation();
    return atan2f(at.mV[VY], at.mV[VX]);
}

// eased one-shot / ping-pong / loop progress over a duration
// end_mode: 0 = hold at end, 1 = ping-pong, 2 = loop
F32 cc_progress(F32 phase, F32 duration, S32 end_mode)
{
    const F32 d = llmax(duration, 0.1f);
    F32 u;
    switch (end_mode)
    {
        case 1: { const F32 c = cc_frac(phase / (2.f * d)) * 2.f; u = (c < 1.f) ? c : 2.f - c; break; }
        case 2: u = cc_frac(phase / d); break;
        default: u = llclamp(phase / d, 0.f, 1.f); break;
    }
    return u * u * (3.f - 2.f * u);     // smoothstep ease in/out
}
} // anonymous namespace

// Vertigo shot: camera travels between two distances along a bearing fixed to
// the subject's facing while the FOV compensates so the SUBJECT keeps constant
// angular size -- the background stretches or compresses around them.
LLVector3 LLCinematicCamera::patternDollyZoom(LLVOAvatar* av, const LLVector3& focus,
                                              F32 phase, F32& fov_mul)
{
    static LLCachedControl<F32> d_start(gSavedSettings, "CinematicCamVertigoStartDist", 2.f);
    static LLCachedControl<F32> d_end(gSavedSettings, "CinematicCamVertigoEndDist", 7.f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamVertigoDuration", 8.f);
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamVertigoHeading", 0.f);  // deg from facing
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamVertigoHeight", 0.f);    // rel focus
    static LLCachedControl<S32> end_mode(gSavedSettings, "CinematicCamVertigoEndMode", 1);   // ping-pong

    const F32 u  = cc_progress(phase, duration, end_mode);
    const F32 d0 = llmax((F32)d_start, 0.3f);
    const F32 d  = llmax(cc_lerp(d0, llmax((F32)d_end, 0.3f), u), 0.3f);

    // keep the subject's angular size constant: tan(fov/2) * d == const
    LLViewerCamera* cam = LLViewerCamera::getInstance();
    const F32 half0 = 0.5f * cam->getDefaultFOV();
    const F32 half  = atanf(tanf(half0) * d0 / d);
    fov_mul = llclamp(half / llmax(half0, 0.001f), 0.05f, 4.f);

    const F32 yaw = cc_avatarYaw(av) + heading * DEG_TO_RAD;
    return focus + LLVector3(cosf(yaw) * d, sinf(yaw) * d, (F32)height);
}

// slow creep from a wide start to a close-up on the face; holds at the end
LLVector3 LLCinematicCamera::patternPushIn(LLVOAvatar* av, const LLVector3& focus, F32 phase)
{
    static LLCachedControl<F32> d_start(gSavedSettings, "CinematicCamPushStartDist", 4.f);
    static LLCachedControl<F32> d_end(gSavedSettings, "CinematicCamPushEndDist", 0.8f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamPushDuration", 12.f);
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamPushHeading", 0.f);   // deg from facing
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamPushHeight", 0.f);     // rel focus (eye level)
    static LLCachedControl<S32> end_mode(gSavedSettings, "CinematicCamPushEndMode", 0);    // hold

    const F32 u = cc_progress(phase, duration, end_mode);
    const F32 d = llmax(cc_lerp(llmax((F32)d_start, 0.3f), llmax((F32)d_end, 0.3f), u), 0.3f);
    const F32 yaw = cc_avatarYaw(av) + heading * DEG_TO_RAD;
    return focus + LLVector3(cosf(yaw) * d, sinf(yaw) * d, (F32)height);
}

// low-angle hero shot: camera near the ground in front of the subject looking
// up, drifting on a slow arc with a gentle breathing push
LLVector3 LLCinematicCamera::patternLowHero(LLVOAvatar* av, const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamHeroDistance", 2.2f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamHeroHeight", 0.35f);   // above feet
    static LLCachedControl<F32> arc(gSavedSettings, "CinematicCamHeroArc", 30.f);          // deg total drift
    static LLCachedControl<F32> period(gSavedSettings, "CinematicCamHeroPeriod", 14.f);
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamHeroHeading", 0.f);   // deg from facing

    const F32 w = F_TWO_PI / llmax((F32)period, 1.f);
    const F32 yaw = cc_avatarYaw(av) + (heading + 0.5f * arc * sinf(phase * w)) * DEG_TO_RAD;
    const F32 d = llmax((F32)distance, 0.3f) * (1.f - 0.12f * sinf(phase * w * 0.5f));
    return center + LLVector3(cosf(yaw) * d, sinf(yaw) * d, (F32)height);
}

// God's-eye: straight down on the subject, rising (or descending) between two
// heights, with an optional slow spin carried out as camera roll
LLVector3 LLCinematicCamera::patternOverhead(const LLVector3& center, F32 phase, F32& roll_out)
{
    static LLCachedControl<F32> h_start(gSavedSettings, "CinematicCamOverheadStart", 4.f);
    static LLCachedControl<F32> h_end(gSavedSettings, "CinematicCamOverheadEnd", 14.f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamOverheadDuration", 16.f);
    static LLCachedControl<F32> spin(gSavedSettings, "CinematicCamOverheadSpin", 4.f);     // deg/s
    static LLCachedControl<S32> end_mode(gSavedSettings, "CinematicCamOverheadEndMode", 0); // hold

    const F32 u = cc_progress(phase, duration, end_mode);
    roll_out = spin * phase * DEG_TO_RAD;
    // tiny lateral epsilon keeps the straight-down look-at well-defined
    return center + LLVector3(0.02f, 0.f, llmax(cc_lerp((F32)h_start, (F32)h_end, u), 0.5f));
}

// snap zoom with a slight overshoot from a tripod position captured at
// activation; toggle the mode off/on to retrigger the hit
LLVector3 LLCinematicCamera::patternCrashZoom(F32 phase, F32& fov_mul)
{
    static LLCachedControl<F32> zoom(gSavedSettings, "CinematicCamCrashZoom", 0.35f);      // end fov mul
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamCrashDuration", 0.4f);

    const F32 u = llclamp(phase / llmax((F32)duration, 0.05f), 0.f, 1.f);
    // sharp attack with a ~8% overshoot that settles
    const F32 e = 1.f - powf(1.f - u, 3.f);
    const F32 over = 1.f + 0.08f * sinf(llmin(u * 2.f, 1.f) * F_PI) * (1.f - u);
    fov_mul = llclamp(cc_lerp(1.f, (F32)zoom, e) * (u < 1.f ? over : 1.f), 0.05f, 4.f);
    return mTripodPos;
}

// Kubrick creep: position frozen, FOV drifts imperceptibly over a long time
LLVector3 LLCinematicCamera::patternSlowZoom(F32 phase, F32& fov_mul)
{
    static LLCachedControl<F32> zoom(gSavedSettings, "CinematicCamSlowZoomTarget", 0.55f); // <1 in, >1 out
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamSlowZoomDuration", 45.f);

    const F32 u = cc_progress(phase, duration, 0);
    fov_mul = llclamp(cc_lerp(1.f, (F32)zoom, u), 0.05f, 4.f);
    return mTripodPos;
}

// violent sub-second arc around the subject; reads as a whip with motion blur
LLVector3 LLCinematicCamera::patternWhipArc(LLVOAvatar* av, const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> from_deg(gSavedSettings, "CinematicCamWhipFrom", -60.f);
    static LLCachedControl<F32> to_deg(gSavedSettings, "CinematicCamWhipTo", 60.f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamWhipDuration", 0.45f);
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamWhipDistance", 3.f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamWhipHeight", 1.2f);

    F32 u = llclamp(phase / llmax((F32)duration, 0.05f), 0.f, 1.f);
    u = u * u * (3.f - 2.f * u); u = u * u * (3.f - 2.f * u);   // double smoothstep: hard whip
    const F32 yaw = cc_avatarYaw(av) + cc_lerp((F32)from_deg, (F32)to_deg, u) * DEG_TO_RAD;
    return center + LLVector3(cosf(yaw) * distance, sinf(yaw) * distance, (F32)height);
}

// the universal oner building block: one eased partial orbit, then hold
LLVector3 LLCinematicCamera::patternArc(LLVOAvatar* av, const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> from_deg(gSavedSettings, "CinematicCamArcFrom", -40.f);
    static LLCachedControl<F32> to_deg(gSavedSettings, "CinematicCamArcTo", 40.f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamArcDuration", 9.f);
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamArcDistance", 2.6f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamArcHeight", 1.3f);
    static LLCachedControl<S32> end_mode(gSavedSettings, "CinematicCamArcEndMode", 0);     // hold

    const F32 u = cc_progress(phase, duration, end_mode);
    const F32 yaw = cc_avatarYaw(av) + cc_lerp((F32)from_deg, (F32)to_deg, u) * DEG_TO_RAD;
    return center + LLVector3(cosf(yaw) * distance, sinf(yaw) * distance, (F32)height);
}

// epic arrival: starts low behind the subject, rises over their shoulder while
// the framing lifts from the subject to the horizon ahead of them
LLVector3 LLCinematicCamera::patternReveal(LLVOAvatar* av, const LLVector3& center,
                                           F32 phase, LLVector3& focus_io)
{
    static LLCachedControl<F32> behind(gSavedSettings, "CinematicCamRevealBehind", 1.4f);
    static LLCachedControl<F32> low_h(gSavedSettings, "CinematicCamRevealLowHeight", 0.4f);
    static LLCachedControl<F32> high_h(gSavedSettings, "CinematicCamRevealHighHeight", 2.1f);
    static LLCachedControl<F32> ahead(gSavedSettings, "CinematicCamRevealAhead", 14.f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamRevealDuration", 10.f);

    const F32 u = cc_progress(phase, duration, 0);
    const F32 yaw = cc_avatarYaw(av);
    const LLVector3 fwd(cosf(yaw), sinf(yaw), 0.f);

    const LLVector3 subject = focus_io;     // head focus from the caller
    focus_io = subject * (1.f - u) + (subject + fwd * (F32)ahead + LLVector3(0.f, 0.f, 1.f)) * u;
    return center - fwd * (F32)behind + LLVector3(0.f, 0.f, cc_lerp((F32)low_h, (F32)high_h, u));
}

// closing shot: eased retreat and rise, leaving the subject in the frame
LLVector3 LLCinematicCamera::patternPullBack(LLVOAvatar* av, const LLVector3& focus, F32 phase)
{
    static LLCachedControl<F32> d_start(gSavedSettings, "CinematicCamPullStartDist", 1.2f);
    static LLCachedControl<F32> d_end(gSavedSettings, "CinematicCamPullEndDist", 10.f);
    static LLCachedControl<F32> h_end(gSavedSettings, "CinematicCamPullEndHeight", 2.5f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamPullDuration", 14.f);
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamPullHeading", 0.f);

    const F32 u = cc_progress(phase, duration, 0);
    const F32 yaw = cc_avatarYaw(av) + heading * DEG_TO_RAD;
    const F32 d = cc_lerp(llmax((F32)d_start, 0.3f), llmax((F32)d_end, 0.3f), u);
    return focus + LLVector3(cosf(yaw) * d, sinf(yaw) * d, (F32)h_end * u);
}

// dialogue master: perpendicular to the line between me and the selected
// avatar, framing the midpoint; degrades to a profile shot of the subject
LLVector3 LLCinematicCamera::patternTwoShot(LLVOAvatar* target, LLVector3& focus_io)
{
    static LLCachedControl<S32> side(gSavedSettings, "CinematicCamTwoShotSide", 1);
    static LLCachedControl<F32> pad(gSavedSettings, "CinematicCamTwoShotPad", 1.3f);       // dist = sep * pad
    static LLCachedControl<F32> min_d(gSavedSettings, "CinematicCamTwoShotMinDist", 2.5f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamTwoShotHeight", 1.4f);

    // [Director] Subject B supplies the second body when set and alive;
    // otherwise the stock self+target pairing
    LLVOAvatar* second = LLDirectorCast::instance().resolveSubjectB();
    LLVOAvatar* self = second ? second
                              : (isAgentAvatarValid() ? (LLVOAvatar*)gAgentAvatarp : target);
    const F32 s = ((S32)side != 0) ? 1.f : -1.f;

    LLVector3 a = self->getPositionAgent();
    LLVector3 b = (target && target != self) ? target->getPositionAgent()
                                             : a + LLVector3(cosf(cc_avatarYaw(self)), sinf(cc_avatarYaw(self)), 0.f) * 2.f;
    LLVector3 line = b - a; line.mV[VZ] = 0.f;
    const F32 sep = llmax(line.normVec(), 0.5f);
    const LLVector3 perp(-line.mV[VY] * s, line.mV[VX] * s, 0.f);

    const LLVector3 mid = (a + b) * 0.5f;
    focus_io = mid + LLVector3(0.f, 0.f, (F32)height);
    return focus_io + perp * llmax(sep * (F32)pad, (F32)min_d);
}

// walk-and-talk: camera ahead of the subject looking back, backpedaling as
// they advance (the smoothing constant supplies the steadicam lag)
LLVector3 LLCinematicCamera::patternLeadFollow(LLVOAvatar* av, const LLVector3& focus, F32 phase)
{
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamLeadDistance", 2.2f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamLeadHeight", 0.f);      // rel focus
    static LLCachedControl<F32> sway(gSavedSettings, "CinematicCamLeadSway", 0.15f);

    const F32 yaw = cc_avatarYaw(av);
    LLVector3 fwd(cosf(yaw), sinf(yaw), 0.f);
    LLVector3 left(-fwd.mV[VY], fwd.mV[VX], 0.f);
    return focus + fwd * llmax((F32)distance, 0.5f)
                 + left * (cc_fbm(phase * 0.25f) * (F32)sway)
                 + LLVector3(0.f, 0.f, (F32)height);
}

// Leone standoff: locked micro-frame on the face through a narrow lens, with
// a barely-there float so it breathes
LLVector3 LLCinematicCamera::patternECU(LLVOAvatar* av, const LLVector3& focus,
                                        F32 phase, F32& fov_mul)
{
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamECUDistance", 0.5f);
    static LLCachedControl<F32> zoom(gSavedSettings, "CinematicCamECUZoom", 0.55f);
    static LLCachedControl<F32> drift(gSavedSettings, "CinematicCamECUDrift", 0.006f);

    fov_mul = llclamp((F32)zoom, 0.05f, 1.5f);
    const F32 yaw = cc_avatarYaw(av);
    LLVector3 p = focus + LLVector3(cosf(yaw), sinf(yaw), 0.f) * llmax((F32)distance, 0.25f);
    p += LLVector3(cc_fbm(phase * 0.31f), cc_fbm(phase * 0.27f + 13.f), cc_fbm(phase * 0.23f + 29.f)) * (F32)drift;
    return p;
}

// surveillance: far off through a long lens; the compression plus a slow
// drift reads as "someone is watching"
LLVector3 LLCinematicCamera::patternLongLens(LLVOAvatar* av, const LLVector3& focus,
                                             F32 phase, F32& fov_mul)
{
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamLongDistance", 15.f);
    static LLCachedControl<F32> zoom(gSavedSettings, "CinematicCamLongZoom", 0.22f);
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamLongHeading", 35.f);  // deg from facing
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamLongHeight", 0.4f);
    static LLCachedControl<F32> drift(gSavedSettings, "CinematicCamLongDrift", 0.05f);

    fov_mul = llclamp((F32)zoom, 0.05f, 1.f);
    const F32 yaw = cc_avatarYaw(av) + heading * DEG_TO_RAD;
    LLVector3 p = focus + LLVector3(cosf(yaw), sinf(yaw), 0.f) * llmax((F32)distance, 3.f)
                        + LLVector3(0.f, 0.f, (F32)height);
    p += LLVector3(cc_fbm(phase * 0.11f), cc_fbm(phase * 0.13f + 7.f), cc_fbm(phase * 0.09f + 17.f)) * (F32)drift;
    return p;
}

// oner flourish: the orbit tightens and rises as it turns, ending close
LLVector3 LLCinematicCamera::patternSpiral(const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> r_start(gSavedSettings, "CinematicCamSpiralStartRadius", 6.f);
    static LLCachedControl<F32> r_end(gSavedSettings, "CinematicCamSpiralEndRadius", 1.4f);
    static LLCachedControl<F32> h_start(gSavedSettings, "CinematicCamSpiralStartHeight", 0.3f);
    static LLCachedControl<F32> h_end(gSavedSettings, "CinematicCamSpiralEndHeight", 2.f);
    static LLCachedControl<F32> speed(gSavedSettings, "CinematicCamSpiralSpeed", 40.f);    // deg/s
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamSpiralDuration", 12.f);

    const F32 u = cc_progress(phase, duration, 0);
    const F32 a = phase * speed * DEG_TO_RAD;
    const F32 r = cc_lerp((F32)r_start, llmax((F32)r_end, 0.3f), u);
    return center + LLVector3(cosf(a) * r, sinf(a) * r, cc_lerp((F32)h_start, (F32)h_end, u));
}

// character introduction: boots-to-face vertical rise at a fixed frontal
// distance, the gaze staying level with whatever the frame is passing
LLVector3 LLCinematicCamera::patternPedestal(LLVOAvatar* av, const LLVector3& center,
                                             F32 phase, LLVector3& focus_io)
{
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamPedestalDistance", 1.8f);
    static LLCachedControl<F32> h_start(gSavedSettings, "CinematicCamPedestalStart", 0.2f);
    static LLCachedControl<F32> h_end(gSavedSettings, "CinematicCamPedestalEnd", 1.75f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamPedestalDuration", 8.f);
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamPedestalHeading", 0.f);

    const F32 u = cc_progress(phase, duration, 0);
    const F32 h = cc_lerp((F32)h_start, (F32)h_end, u);
    const F32 yaw = cc_avatarYaw(av) + heading * DEG_TO_RAD;
    focus_io = center + LLVector3(0.f, 0.f, h);
    return center + LLVector3(cosf(yaw) * llmax((F32)distance, 0.4f), sinf(yaw) * llmax((F32)distance, 0.4f), h);
}

// over-the-shoulder: anchored behind MY shoulder, framing the resolved target
// (the selected avatar; falls back to what I'm facing when nothing is selected)
LLVector3 LLCinematicCamera::patternOTS(LLVOAvatar* target, LLVector3& focus_io)
{
    static LLCachedControl<S32> side(gSavedSettings, "CinematicCamOTSSide", 1);        // 1=right, 0=left
    static LLCachedControl<F32> back(gSavedSettings, "CinematicCamOTSBack", 0.45f);
    static LLCachedControl<F32> out(gSavedSettings, "CinematicCamOTSOut", 0.22f);
    static LLCachedControl<F32> up(gSavedSettings, "CinematicCamOTSUp", 0.12f);

    // [Director] Subject B's shoulder anchors the shot when set and alive
    // (B looking at A); otherwise the stock behind-my-shoulder framing
    LLVOAvatar* second = LLDirectorCast::instance().resolveSubjectB();
    LLVOAvatar* self = second ? second
                              : (isAgentAvatarValid() ? (LLVOAvatar*)gAgentAvatarp : target);

    const char* joint_name = ((S32)side != 0) ? "mShoulderRight" : "mShoulderLeft";
    LLVector3 shoulder = self->getPositionAgent() + LLVector3(0.f, 0.f, 1.4f);
    if (LLJoint* j = self->getJoint(joint_name))
    {
        shoulder = j->getWorldPosition();
    }

    const F32 yaw = cc_avatarYaw(self);
    const LLVector3 fwd(cosf(yaw), sinf(yaw), 0.f);
    const LLVector3 right(sinf(yaw), -cosf(yaw), 0.f);
    const F32 s = ((S32)side != 0) ? 1.f : -1.f;

    // frame the conversation partner's head; with no distinct target, frame
    // the space I'm facing so the shot still composes
    if (target && target != self)
    {
        if (LLJoint* th = target->getJoint("mHead"))
        {
            focus_io = th->getWorldPosition();
        }
        else
        {
            focus_io = target->getPositionAgent() + LLVector3(0.f, 0.f, 1.5f);
        }
    }
    else
    {
        focus_io = shoulder + fwd * 3.f;
    }

    return shoulder - fwd * (F32)back + right * s * (F32)out + LLVector3(0.f, 0.f, (F32)up);
}

// ---------------------------------------------------------------------------
void LLCinematicCamera::updateCamera()
{
    static LLCachedControl<S32>  mode(gSavedSettings, "CinematicCamMode", 1);
    static LLCachedControl<F32>  smoothing(gSavedSettings, "CinematicCamSmoothing", 0.35f);   // seconds
    static LLCachedControl<bool> look_at_head(gSavedSettings, "CinematicCamLookAtHead", true);
    static LLCachedControl<bool> use_operator(gSavedSettings, "CinematicCamUseOperator", false);
    static LLCachedControl<F32>  frame_up(gSavedSettings, "CinematicCamFrameOffsetUp", 0.f);

    LLVOAvatar* av = resolveTarget();
    if (!av)
    {
        return;
    }

    // fresh activation (mode was off for a few frames): restart the pattern
    // clock so one-shot moves (dolly zoom, push-in, overhead) begin at their
    // start pose, and let smoothing/operator re-seed instead of lerping from
    // a stale pose
    if (gFrameCount > mLastUpdateFrame + 3)
    {
        mPhase = 0.f;
        mHavePose = false;
        mWasActive = false;
        // tripod modes (crash/slow zoom) shoot from wherever the camera was
        // when the mode engaged
        mTripodPos = LLViewerCamera::getInstance()->getOrigin();
    }
    mLastUpdateFrame = gFrameCount;

    F32 dt = llclamp(gFrameIntervalSeconds.value(), 0.0005f, 0.25f);
    mPhase += dt;
    if (mPhase > PHASE_WRAP)
    {
        mPhase -= PHASE_WRAP;
    }

    // the point patterns frame: head when available, else chest height
    LLVector3 focus = av->getPositionAgent() + LLVector3(0.f, 0.f, 1.f);
    if (look_at_head)
    {
        if (LLJoint* head = av->getJoint("mHead"))
        {
            focus = head->getWorldPosition();
        }
    }
    LLVector3 center = av->getPositionAgent();

    // global frame offset: raise/lower the point every target-framing mode
    // circles around and aims at. Bone lock has its own mount offsets.
    if ((S32)mode != MODE_BONE_LOCK)
    {
        const LLVector3 frame_off(0.f, 0.f, (F32)frame_up);
        focus += frame_off;
        center += frame_off;
    }

    LLVector3 pos;
    LLQuaternion rot;
    bool have_rot = false;
    F32 mode_fov_mul = 1.f;     // dolly zoom writes this
    F32 mode_roll = 0.f;        // overhead spin writes this (radians)

    switch ((S32)mode)
    {
        case MODE_BONE_LOCK:  patternBoneLock(av, mPhase, pos, rot, have_rot); break;
        case MODE_ORBIT:      pos = patternOrbit(center, mPhase); break;
        case MODE_FLY_HOVER:  pos = patternHover(center, mPhase); break;
        case MODE_SWEEP:      pos = patternSweep(center, mPhase); break;
        case MODE_CRANE:      pos = patternCrane(center, mPhase); break;
        case MODE_DOLLY_ZOOM: pos = patternDollyZoom(av, focus, mPhase, mode_fov_mul); break;
        case MODE_PUSH_IN:    pos = patternPushIn(av, focus, mPhase); break;
        case MODE_LOW_HERO:   pos = patternLowHero(av, center, mPhase); break;
        case MODE_OVERHEAD:   pos = patternOverhead(center, mPhase, mode_roll); break;
        case MODE_OTS:        pos = patternOTS(av, focus); break;
        case MODE_CRASH_ZOOM: pos = patternCrashZoom(mPhase, mode_fov_mul); break;
        case MODE_SLOW_ZOOM:  pos = patternSlowZoom(mPhase, mode_fov_mul); break;
        case MODE_WHIP_ARC:   pos = patternWhipArc(av, center, mPhase); break;
        case MODE_ARC:        pos = patternArc(av, center, mPhase); break;
        case MODE_REVEAL:     pos = patternReveal(av, center, mPhase, focus); break;
        case MODE_PULL_BACK:  pos = patternPullBack(av, focus, mPhase); break;
        case MODE_TWO_SHOT:   pos = patternTwoShot(av, focus); break;
        case MODE_LEAD_FOLLOW:pos = patternLeadFollow(av, focus, mPhase); break;
        case MODE_ECU_EYES:   pos = patternECU(av, focus, mPhase, mode_fov_mul); break;
        case MODE_LONG_LENS:  pos = patternLongLens(av, focus, mPhase, mode_fov_mul); break;
        case MODE_SPIRAL:     pos = patternSpiral(center, mPhase); break;
        case MODE_PEDESTAL:   pos = patternPedestal(av, center, mPhase, focus); break;
        default:              return;
    }

    if (!have_rot)
    {
        rot = cc_lookAt(pos, focus);
    }

    // dutch angle (composable unease dial for every mode) + overhead spin,
    // as a roll in the camera's local frame -- same trim idiom as bone lock
    static LLCachedControl<F32> dutch(gSavedSettings, "CinematicCamDutchAngle", 0.f);   // deg
    const F32 total_roll = (F32)dutch * DEG_TO_RAD + mode_roll;
    if (fabsf(total_roll) > 0.0001f)
    {
        LLQuaternion roll_q;
        roll_q.setEulerAngles(total_roll, 0.f, 0.f);
        rot = roll_q * rot;
    }

    // ---- temporal smoothing (one-pole, framerate-independent) -------------
    const F32 tau = llmax((F32)smoothing, 0.f);
    if (!mHavePose || tau < 1e-3f)
    {
        mSmPos = pos;
        mSmRot = rot;
        mHavePose = true;
    }
    else
    {
        const F32 alpha = 1.f - expf(-dt / tau);
        mSmPos = mSmPos + (pos - mSmPos) * alpha;
        mSmRot = nlerp(alpha, mSmRot, rot);
    }

    LLVector3 out_pos = mSmPos;
    LLQuaternion out_rot = mSmRot;
    F32 fov_mul = 1.f;

    // ---- optional handheld texture on top ---------------------------------
    if (use_operator)
    {
        if (!mWasActive)
        {
            LLCameraOperator::instance().reset();
        }
        LLMatrix3 axes(mSmRot);
        const LLVector3 world_vel = (mSmPos - mPrevPos) * (1.f / dt);
        // angular velocity from the frame-to-frame rotation delta
        LLQuaternion dq = mSmRot * ~mPrevRot;
        F32 d_roll, d_pitch, d_yaw;
        LLMatrix3(dq).getEulerAngles(&d_roll, &d_pitch, &d_yaw);

        LLCameraOperatorInput opin;
        opin.mDeltaTime = dt;
        opin.mLinearVel = LLVector3(world_vel * LLVector3(axes.mMatrix[0]),
                                    world_vel * LLVector3(axes.mMatrix[1]),
                                    world_vel * LLVector3(axes.mMatrix[2]));
        opin.mAngularVel = LLVector3(d_roll, d_pitch, d_yaw) * (1.f / dt);

        const LLCameraOperatorOutput op = LLCameraOperator::instance().update(opin);
        LLMatrix3 wobble(op.mRoll, op.mPitch, op.mYaw);
        out_rot = LLQuaternion(wobble) * out_rot;
        LLMatrix3 out_axes(out_rot);
        out_pos += LLVector3(out_axes.mMatrix[0]) * op.mPosOffset.mV[VX]
                 + LLVector3(out_axes.mMatrix[1]) * op.mPosOffset.mV[VY]
                 + LLVector3(out_axes.mMatrix[2]) * op.mPosOffset.mV[VZ];
        fov_mul = op.mFovMul;
    }

    mPrevPos = mSmPos;
    mPrevRot = mSmRot;
    mWasActive = true;

    // ---- write the render camera -------------------------------------------
    LLViewerCamera* cam = LLViewerCamera::getInstance();
    LLMatrix3 final_axes(out_rot);
    cam->setView(cam->getDefaultFOV() * mode_fov_mul * fov_mul);
    cam->setOrigin(out_pos);
    cam->mXAxis = LLVector3(final_axes.mMatrix[0]);
    cam->mYAxis = LLVector3(final_axes.mMatrix[1]);
    cam->mZAxis = LLVector3(final_axes.mMatrix[2]);
}
