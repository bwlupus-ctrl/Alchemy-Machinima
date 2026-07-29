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

LLVector3 cc_subjectBase(LLVOAvatar* av)
{
    if (LLJoint* root = av->getRootJoint())
    {
        LLVector3 foot = root->getWorldPosition();
        foot.mV[VZ] -= av->getPelvisToFoot();
        return foot;
    }
    return av->getPositionAgent();
}

LLVector3 cc_scaleAboutSubjectBase(LLVOAvatar* av, const LLVector3& point)
{
    const F32 scale = av->getUniformScale();
    if (scale == 1.f)
    {
        return point;
    }
    const LLVector3 base = cc_subjectBase(av);
    return base + (point - base) * scale;
}

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

//static
void LLCinematicCamera::onRuntimeTargetReplaced(const LLUUID& old_id,
                                                 const LLUUID& new_id)
{
    if (old_id.notNull() && sCinematicFollowTarget == old_id)
    {
        sCinematicFollowTarget = new_id;
    }
}

bool LLCinematicCamera::isActive() const
{
    static LLCachedControl<bool> enabled(gSavedSettings, "CinematicCamEnabled", false);
    static LLCachedControl<S32>  mode(gSavedSettings, "CinematicCamMode", 1);
    if (!enabled || (S32)mode <= MODE_OFF || (S32)mode > MODE_BREATHING_HOLD)
    {
        return false;
    }
    return resolveTarget() != nullptr;
}

bool LLCinematicCamera::isActiveBoneLockTarget(const LLUUID& avatar_id) const
{
    static LLCachedControl<bool> enabled(gSavedSettings, "CinematicCamEnabled", false);
    static LLCachedControl<S32>  mode(gSavedSettings, "CinematicCamMode", 1);
    if (avatar_id.isNull() || !enabled || (S32)mode != MODE_BONE_LOCK)
    {
        return false;
    }
    LLVOAvatar* target = resolveTarget();
    return target && !target->isDead() && target->getID() == avatar_id;
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
    pos = cc_scaleAboutSubjectBase(av, pos);
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

// raw (linear) one-shot / ping-pong / loop progress over a duration
// end_mode: 0 = hold at end, 1 = ping-pong, 2 = loop
F32 cc_progress_raw(F32 phase, F32 duration, S32 end_mode)
{
    const F32 d = llmax(duration, 0.1f);
    switch (end_mode)
    {
        case 1: { const F32 c = cc_frac(phase / (2.f * d)) * 2.f; return (c < 1.f) ? c : 2.f - c; }
        case 2:  return cc_frac(phase / d);
        default: return llclamp(phase / d, 0.f, 1.f);
    }
}

// eased (smoothstep) variant of the above
F32 cc_progress(F32 phase, F32 duration, S32 end_mode)
{
    const F32 u = cc_progress_raw(phase, duration, end_mode);
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
    const F32 self_scale = self->getUniformScale();
    const F32 target_scale = target ? target->getUniformScale() : 1.f;
    const F32 framing_scale = llmax(self_scale, target_scale);
    if (self_scale == 1.f && target_scale == 1.f)
    {
        focus_io = mid + LLVector3(0.f, 0.f, (F32)height);
    }
    else
    {
        // Midpoint the two bodies' individually-scaled head-height proxies.
        focus_io = mid + LLVector3(0.f, 0.f,
                                  (F32)height * 0.5f * (self_scale + target_scale));
    }
    return focus_io + perp * llmax(sep * (F32)pad, (F32)min_d * framing_scale);
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
    shoulder = cc_scaleAboutSubjectBase(self, shoulder);

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
            focus_io = cc_scaleAboutSubjectBase(target, th->getWorldPosition());
        }
        else
        {
            focus_io = cc_scaleAboutSubjectBase(
                target, target->getPositionAgent() + LLVector3(0.f, 0.f, 1.5f));
        }
    }
    else
    {
        focus_io = shoulder + fwd * 3.f * self->getUniformScale();
    }

    const F32 scale = self->getUniformScale();
    return shoulder - fwd * (F32)back * scale + right * s * (F32)out * scale
                    + LLVector3(0.f, 0.f, (F32)up * scale);
}

// ---------------------------------------------------------------------------
// acrobatic / dance / closeup / impact patterns
// ---------------------------------------------------------------------------

// held frontal frame with the camera rolling: continuous spin (RollSpeed) or an
// eased oscillation (RollAmplitude / RollPeriod). Reads as a dance flourish.
LLVector3 LLCinematicCamera::patternBarrelRoll(LLVOAvatar* av, const LLVector3& center,
                                               F32 phase, F32& roll_out)
{
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamBarrelDistance", 3.f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamBarrelHeight", 1.f);
    static LLCachedControl<F32> roll_speed(gSavedSettings, "CinematicCamBarrelRollSpeed", 45.f);   // deg/s
    static LLCachedControl<F32> roll_amp(gSavedSettings, "CinematicCamBarrelRollAmplitude", 30.f); // deg
    static LLCachedControl<F32> roll_period(gSavedSettings, "CinematicCamBarrelRollPeriod", 4.f);  // s
    static LLCachedControl<bool> oscillate(gSavedSettings, "CinematicCamBarrelOscillate", false);

    if (oscillate)
    {
        const F32 w = F_TWO_PI / llmax((F32)roll_period, 0.1f);
        roll_out = (F32)roll_amp * sinf(phase * w) * DEG_TO_RAD;
    }
    else
    {
        roll_out = (F32)roll_speed * phase * DEG_TO_RAD;
    }

    const F32 yaw = cc_avatarYaw(av);
    return center + LLVector3(cosf(yaw) * llmax((F32)distance, 0.3f),
                             sinf(yaw) * llmax((F32)distance, 0.3f), (F32)height);
}

// aggressive spiral: orbit whose radius and height lerp start->end across Turns
// revolutions in Duration, rolling RollPerTurn per revolution. Loop/ping-pong.
LLVector3 LLCinematicCamera::patternCorkscrew(const LLVector3& center, F32 phase, F32& roll_out)
{
    static LLCachedControl<F32> r_start(gSavedSettings, "CinematicCamCorkStartRadius", 5.f);
    static LLCachedControl<F32> r_end(gSavedSettings, "CinematicCamCorkEndRadius", 1.5f);
    static LLCachedControl<F32> h_start(gSavedSettings, "CinematicCamCorkStartHeight", 0.3f);
    static LLCachedControl<F32> h_end(gSavedSettings, "CinematicCamCorkEndHeight", 2.5f);
    static LLCachedControl<F32> turns(gSavedSettings, "CinematicCamCorkTurns", 2.f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamCorkDuration", 8.f);
    static LLCachedControl<F32> roll_per_turn(gSavedSettings, "CinematicCamCorkRollPerTurn", 90.f); // deg
    static LLCachedControl<S32> end_mode(gSavedSettings, "CinematicCamCorkEndMode", 2);             // loop

    const F32 u = cc_progress(phase, duration, end_mode);
    const F32 revs = (F32)turns * u;                    // revolutions completed
    const F32 a = revs * F_TWO_PI;
    const F32 r = cc_lerp((F32)r_start, llmax((F32)r_end, 0.3f), u);
    roll_out = revs * (F32)roll_per_turn * DEG_TO_RAD;
    return center + LLVector3(cosf(a) * r, sinf(a) * r, cc_lerp((F32)h_start, (F32)h_end, u));
}

// eases at the extremes, fastest through center: a horizontal swing of half-
// width SwingAngle about a facing-relative heading, always aimed at the subject
LLVector3 LLCinematicCamera::patternPendulum(LLVOAvatar* av, const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> radius(gSavedSettings, "CinematicCamPendulumRadius", 3.5f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamPendulumHeight", 1.2f);
    static LLCachedControl<F32> swing(gSavedSettings, "CinematicCamPendulumSwing", 45.f);   // deg half-width
    static LLCachedControl<F32> period(gSavedSettings, "CinematicCamPendulumPeriod", 6.f);  // s
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamPendulumHeading", 0.f);// deg, arc facing

    const F32 w = F_TWO_PI / llmax((F32)period, 0.5f);
    const F32 ang = (F32)swing * sinf(phase * w);       // eased at the extremes
    const F32 yaw = cc_avatarYaw(av) + (heading + ang) * DEG_TO_RAD;
    return center + LLVector3(cosf(yaw) * llmax((F32)radius, 0.3f),
                             sinf(yaw) * llmax((F32)radius, 0.3f), (F32)height);
}

// dolly-zoom-while-circling: a steady orbit while the FOV warps between two
// multipliers, so the perspective breathes as the camera comes around
LLVector3 LLCinematicCamera::patternContraOrbit(const LLVector3& center, F32 phase, F32& fov_mul)
{
    static LLCachedControl<F32> radius(gSavedSettings, "CinematicCamContraRadius", 4.f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamContraHeight", 1.f);
    static LLCachedControl<F32> speed(gSavedSettings, "CinematicCamContraSpeed", 25.f);     // deg/s
    static LLCachedControl<F32> fov_start(gSavedSettings, "CinematicCamContraFovStart", 0.6f);
    static LLCachedControl<F32> fov_end(gSavedSettings, "CinematicCamContraFovEnd", 1.5f);
    static LLCachedControl<F32> warp_period(gSavedSettings, "CinematicCamContraWarpPeriod", 8.f); // s

    const F32 a = phase * speed * DEG_TO_RAD;
    const F32 w = F_TWO_PI / llmax((F32)warp_period, 0.5f);
    const F32 t = 0.5f + 0.5f * sinf(phase * w);
    fov_mul = llclamp(cc_lerp((F32)fov_start, (F32)fov_end, t), 0.05f, 4.f);
    return center + LLVector3(cosf(a) * radius, sinf(a) * radius, (F32)height);
}

// wide-lens impact: a constant wide FOV while the camera pushes from far to
// near at the face and recoils over Duration (eased). One-shot / ping-pong.
LLVector3 LLCinematicCamera::patternFisheyeLunge(LLVOAvatar* av, const LLVector3& focus,
                                                 F32 phase, F32& fov_mul)
{
    static LLCachedControl<F32> near_d(gSavedSettings, "CinematicCamFisheyeNear", 0.6f);
    static LLCachedControl<F32> far_d(gSavedSettings, "CinematicCamFisheyeFar", 4.f);
    static LLCachedControl<F32> wide_fov(gSavedSettings, "CinematicCamFisheyeFov", 1.7f);  // >1 = wide
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamFisheyeDuration", 2.5f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamFisheyeHeight", 0.f);  // rel focus
    static LLCachedControl<S32> end_mode(gSavedSettings, "CinematicCamFisheyeEndMode", 1); // ping-pong

    fov_mul = llclamp((F32)wide_fov, 0.05f, 4.f);
    const F32 u = cc_progress(phase, duration, end_mode);
    const F32 lunge = sinf(F_PI * u);                   // 0 -> 1 -> 0 (near at the middle)
    const F32 d = llmax(cc_lerp(llmax((F32)far_d, 0.3f), llmax((F32)near_d, 0.2f), lunge), 0.2f);
    const F32 yaw = cc_avatarYaw(av);
    return focus + LLVector3(cosf(yaw) * d, sinf(yaw) * d, (F32)height);
}

// ground-level lateral track (ping-pong) at a very low height, aimed up at the
// subject so the low angle reads
LLVector3 LLCinematicCamera::patternFloorSkimmer(LLVOAvatar* av, const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamSkimmerHeight", 0.25f);
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamSkimmerDistance", 3.f);
    static LLCachedControl<F32> length(gSavedSettings, "CinematicCamSkimmerLength", 6.f);
    static LLCachedControl<F32> speed(gSavedSettings, "CinematicCamSkimmerSpeed", 1.5f);    // m/s
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamSkimmerHeading", 0.f); // deg

    const F32 h = heading * DEG_TO_RAD;
    const LLVector3 dir(cosf(h), sinf(h), 0.f);
    const LLVector3 perp(-sinf(h), cosf(h), 0.f);

    const F32 len = llmax((F32)length, 0.1f);
    const F32 s = phase * llmax((F32)speed, 0.01f) / len;
    const F32 c = cc_frac(s * 0.5f) * 2.f;              // 0..2
    const F32 t = (c < 1.f) ? c : 2.f - c;              // triangle 0..1..0 (ping-pong)
    return center + perp * llmax((F32)distance, 0.3f) + dir * ((t - 0.5f) * len)
                  + LLVector3(0.f, 0.f, (F32)height);
}

// one-shot rocket launch: from near the floor, accelerating (ease-in) straight
// up to EndHeight; the look-at tilts down to keep the subject as it passes
LLVector3 LLCinematicCamera::patternBoostRise(LLVOAvatar* av, const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> h_start(gSavedSettings, "CinematicCamBoostStartHeight", 0.2f);
    static LLCachedControl<F32> h_end(gSavedSettings, "CinematicCamBoostEndHeight", 8.f);
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamBoostDistance", 3.f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamBoostDuration", 4.f);
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamBoostHeading", 0.f);
    static LLCachedControl<S32> end_mode(gSavedSettings, "CinematicCamBoostEndMode", 0);    // hold

    const F32 p = cc_progress_raw(phase, duration, end_mode);
    const F32 e = p * p;                                // accelerate (ease-in)
    const F32 h = cc_lerp((F32)h_start, (F32)h_end, e);
    const F32 yaw = cc_avatarYaw(av) + heading * DEG_TO_RAD;
    return center + LLVector3(cosf(yaw) * llmax((F32)distance, 0.3f),
                             sinf(yaw) * llmax((F32)distance, 0.3f), h);
}

// jib over the top: an elliptical vertical arc that lifts from one side, up
// over the apex above the subject, and down the far side (eased)
LLVector3 LLCinematicCamera::patternBoomOver(LLVOAvatar* av, const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> radius(gSavedSettings, "CinematicCamBoomRadius", 4.f);      // horizontal
    static LLCachedControl<F32> apex(gSavedSettings, "CinematicCamBoomApex", 5.f);          // peak height
    static LLCachedControl<F32> span(gSavedSettings, "CinematicCamBoomSpan", 180.f);        // deg total
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamBoomDuration", 6.f);
    static LLCachedControl<F32> axis(gSavedSettings, "CinematicCamBoomAxis", 0.f);          // deg heading
    static LLCachedControl<S32> end_mode(gSavedSettings, "CinematicCamBoomEndMode", 0);     // hold

    const F32 u = cc_progress(phase, duration, end_mode);
    const F32 half = 0.5f * (F32)span * DEG_TO_RAD;
    const F32 phi = cc_lerp(-half, half, u);            // -span/2 .. +span/2
    const F32 ax = axis * DEG_TO_RAD;
    const LLVector3 dir(cosf(ax), sinf(ax), 0.f);
    return center + dir * ((F32)radius * sinf(phi)) + LLVector3(0.f, 0.f, (F32)apex * cosf(phi));
}

// Busby Berkeley: locked directly overhead looking straight down, the frame
// spinning about the vertical (carried as camera roll). Offset 0 = pure top-down.
LLVector3 LLCinematicCamera::patternTopSpin(const LLVector3& center, F32 phase, F32& roll_out)
{
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamTopSpinHeight", 5.f);
    static LLCachedControl<F32> speed(gSavedSettings, "CinematicCamTopSpinSpeed", 30.f);    // deg/s
    static LLCachedControl<F32> offset(gSavedSettings, "CinematicCamTopSpinOffset", 0.f);   // small radius

    const F32 a = phase * speed * DEG_TO_RAD;
    roll_out = a;                                       // spin the straight-down view
    // a tiny lateral epsilon keeps the look-straight-down orientation well
    // defined even with Offset 0
    const F32 r = (F32)offset;
    return center + LLVector3(cosf(a) * r + 0.02f, sinf(a) * r, llmax((F32)height, 0.5f));
}

// showcase crane: a subject-centered orbit while the height eases between two
// levels for a full-body reveal
LLVector3 LLCinematicCamera::patternTurntable(const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> radius(gSavedSettings, "CinematicCamTurntableRadius", 4.f);
    static LLCachedControl<F32> speed(gSavedSettings, "CinematicCamTurntableSpeed", 15.f);  // deg/s
    static LLCachedControl<F32> min_h(gSavedSettings, "CinematicCamTurntableMinHeight", 0.5f);
    static LLCachedControl<F32> max_h(gSavedSettings, "CinematicCamTurntableMaxHeight", 3.f);
    static LLCachedControl<F32> period(gSavedSettings, "CinematicCamTurntablePeriod", 20.f);// s

    const F32 a = phase * speed * DEG_TO_RAD;
    const F32 u = 0.5f + 0.5f * sinf(phase * F_TWO_PI / llmax((F32)period, 1.f));
    return center + LLVector3(cosf(a) * radius, sinf(a) * radius, cc_lerp((F32)min_h, (F32)max_h, u));
}

// intimate face close-up: a narrow lens, drifting on low-amplitude noise with a
// slight distance breathing so it never locks off
LLVector3 LLCinematicCamera::patternFloatingECU(LLVOAvatar* av, const LLVector3& focus,
                                                F32 phase, F32& fov_mul)
{
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamFloatDistance", 0.8f);
    static LLCachedControl<F32> drift(gSavedSettings, "CinematicCamFloatDrift", 0.04f);
    static LLCachedControl<F32> speed(gSavedSettings, "CinematicCamFloatSpeed", 0.5f);
    static LLCachedControl<F32> zoom(gSavedSettings, "CinematicCamFloatFov", 0.6f);         // <1 = narrow

    fov_mul = llclamp((F32)zoom, 0.05f, 1.5f);
    const F32 t = phase * llmax((F32)speed, 0.01f);
    const F32 d = llmax((F32)distance, 0.25f) * (1.f + 0.08f * sinf(t * 0.7f));   // breathing
    const F32 yaw = cc_avatarYaw(av);
    LLVector3 p = focus + LLVector3(cosf(yaw), sinf(yaw), 0.f) * d;
    p += LLVector3(cc_fbm(t + 3.1f), cc_fbm(t + 13.7f), cc_fbm(t + 29.3f)) * (F32)drift;
    return p;
}

// percussive accent: held frontal frame while the aim whips vertically -- a
// sharp attack each Period settling on an exponential decay, alternating up/down
LLVector3 LLCinematicCamera::patternTiltWhip(LLVOAvatar* av, const LLVector3& center,
                                             F32 phase, LLVector3& focus_io)
{
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamTiltWhipDistance", 3.f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamTiltWhipHeight", 1.2f);
    static LLCachedControl<F32> amplitude(gSavedSettings, "CinematicCamTiltWhipAmplitude", 25.f); // deg
    static LLCachedControl<F32> period(gSavedSettings, "CinematicCamTiltWhipPeriod", 2.f);        // s
    static LLCachedControl<F32> snap(gSavedSettings, "CinematicCamTiltWhipSnap", 6.f);            // decay

    const F32 per = llmax((F32)period, 0.1f);
    const F32 idx = floorf(phase / per);
    const F32 ph = cc_frac(phase / per);                // 0..1 within a whip
    const F32 sign = (cc_frac(idx * 0.5f) < 0.25f) ? 1.f : -1.f;   // alternate up/down
    const F32 env = expf(-llmax((F32)snap, 0.f) * ph);  // sharp attack, eased settle
    const F32 pitch = llclamp((F32)amplitude * DEG_TO_RAD * env * sign, -1.4f, 1.4f);

    const F32 yaw = cc_avatarYaw(av);
    const F32 d = llmax((F32)distance, 0.3f);
    // whip the aim by raising/lowering the framing point: tan(pitch) * distance
    focus_io.mV[VZ] += d * tanf(pitch);
    return center + LLVector3(cosf(yaw) * d, sinf(yaw) * d, (F32)height);
}

// slow body-scale introduction: orbit at a constant radius while both camera
// and gaze travel from the feet to the actual head joint, holding on the face
LLVector3 LLCinematicCamera::patternBodyHelix(LLVOAvatar* av, const LLVector3& center,
                                              F32 phase, LLVector3& focus_io)
{
    static LLCachedControl<F32> radius(gSavedSettings, "CinematicCamBodyHelixRadius", 2.4f);
    static LLCachedControl<F32> revolutions(gSavedSettings, "CinematicCamBodyHelixRevolutions", 1.25f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamBodyHelixDuration", 14.f);
    static LLCachedControl<F32> start_offset(gSavedSettings, "CinematicCamBodyHelixStartOffset", 0.08f);
    static LLCachedControl<F32> end_offset(gSavedSettings, "CinematicCamBodyHelixEndOffset", 0.f);

    const LLVector3 frame_off = center - av->getPositionAgent();
    const LLVector3 feet = cc_subjectBase(av) + frame_off;
    LLVector3 head = feet + LLVector3(0.f, 0.f, 1.75f);
    if (LLJoint* joint = av->getJoint("mHead"))
    {
        head = joint->getWorldPosition() + frame_off;
    }
    const F32 u = cc_progress(phase, duration, 0);
    const F32 a = cc_avatarYaw(av) + (F32)revolutions * F_TWO_PI * u;
    const F32 z = cc_lerp(feet.mV[VZ] + (F32)start_offset,
                          head.mV[VZ] + (F32)end_offset, u);
    focus_io = LLVector3(cc_lerp(feet.mV[VX], head.mV[VX], u),
                         cc_lerp(feet.mV[VY], head.mV[VY], u), z);
    return focus_io + LLVector3(cosf(a) * llmax((F32)radius, 0.3f),
                                sinf(a) * llmax((F32)radius, 0.3f), 0.f);
}

// menace reveal: a fixed-heading reverse pedestal from above the actual head
// down to the feet, with camera and gaze remaining level throughout
LLVector3 LLCinematicCamera::patternDescent(LLVOAvatar* av, const LLVector3& center,
                                            F32 phase, LLVector3& focus_io)
{
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamDescentDistance", 2.2f);
    static LLCachedControl<F32> above_head(gSavedSettings, "CinematicCamDescentAboveHead", 0.45f);
    static LLCachedControl<F32> foot_offset(gSavedSettings, "CinematicCamDescentFootOffset", 0.12f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamDescentDuration", 9.f);
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamDescentHeading", 0.f);

    const LLVector3 frame_off = center - av->getPositionAgent();
    const LLVector3 feet = cc_subjectBase(av) + frame_off;
    LLVector3 head = feet + LLVector3(0.f, 0.f, 1.75f);
    if (LLJoint* joint = av->getJoint("mHead"))
    {
        head = joint->getWorldPosition() + frame_off;
    }
    const F32 u = cc_progress(phase, duration, 0);
    const F32 z = cc_lerp(head.mV[VZ] + (F32)above_head,
                          feet.mV[VZ] + (F32)foot_offset, u);
    focus_io = LLVector3(cc_lerp(head.mV[VX], feet.mV[VX], u),
                         cc_lerp(head.mV[VY], feet.mV[VY], u), z);
    const F32 yaw = cc_avatarYaw(av) + (F32)heading * DEG_TO_RAD;
    return focus_io + LLVector3(cosf(yaw) * llmax((F32)distance, 0.3f),
                                sinf(yaw) * llmax((F32)distance, 0.3f), 0.f);
}

// one-shot lateral truck: a straight rail perpendicular to the chosen heading
LLVector3 LLCinematicCamera::patternParallaxSlide(LLVOAvatar* av, const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> length(gSavedSettings, "CinematicCamParallaxLength", 8.f);
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamParallaxDistance", 3.f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamParallaxHeight", 1.2f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamParallaxDuration", 10.f);
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamParallaxHeading", 0.f);

    const F32 u = cc_progress(phase, duration, 0);
    const F32 yaw = cc_avatarYaw(av) + (F32)heading * DEG_TO_RAD;
    const LLVector3 away(cosf(yaw), sinf(yaw), 0.f);
    const LLVector3 rail(-sinf(yaw), cosf(yaw), 0.f);
    return center + away * llmax((F32)distance, 0.3f)
                  + rail * ((u - 0.5f) * (F32)length)
                  + LLVector3(0.f, 0.f, (F32)height);
}

// dance loop: horizontal lemniscate with opposite parallax on each lobe
LLVector3 LLCinematicCamera::patternFigureEight(LLVOAvatar* av, const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> radius(gSavedSettings, "CinematicCamFigureEightRadius", 3.f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamFigureEightHeight", 1.2f);
    static LLCachedControl<F32> period(gSavedSettings, "CinematicCamFigureEightPeriod", 10.f);
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamFigureEightHeading", 0.f);

    const F32 t = phase * F_TWO_PI / llmax((F32)period, 0.5f);
    const F32 yaw = cc_avatarYaw(av) + (F32)heading * DEG_TO_RAD;
    const LLVector3 fwd(cosf(yaw), sinf(yaw), 0.f);
    const LLVector3 side(-sinf(yaw), cosf(yaw), 0.f);
    // Offset the lemniscate's crossover in front of the subject so the
    // mathematically central crossing never drives the camera through them.
    return center + fwd * ((F32)radius * (1.f + 0.5f * sinf(2.f * t)))
                  + side * ((F32)radius * sinf(t))
                  + LLVector3(0.f, 0.f, (F32)height);
}

// costume insert: narrow close framing drifting laterally across a selectable
// body-height band derived from the feet-to-head span
LLVector3 LLCinematicCamera::patternDetailSweep(LLVOAvatar* av, const LLVector3& center,
                                                F32 phase, LLVector3& focus_io, F32& fov_mul)
{
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamDetailDistance", 0.9f);
    static LLCachedControl<F32> length(gSavedSettings, "CinematicCamDetailLength", 1.2f);
    static LLCachedControl<F32> band(gSavedSettings, "CinematicCamDetailBand", 0.68f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamDetailDuration", 12.f);
    static LLCachedControl<F32> fov(gSavedSettings, "CinematicCamDetailFov", 0.65f);

    const LLVector3 frame_off = center - av->getPositionAgent();
    const LLVector3 feet = cc_subjectBase(av) + frame_off;
    LLVector3 head = feet + LLVector3(0.f, 0.f, 1.75f);
    if (LLJoint* joint = av->getJoint("mHead"))
    {
        head = joint->getWorldPosition() + frame_off;
    }
    focus_io = feet + (head - feet) * llclamp((F32)band, 0.f, 1.f);
    const F32 yaw = cc_avatarYaw(av);
    const LLVector3 away(cosf(yaw), sinf(yaw), 0.f);
    const LLVector3 side(-sinf(yaw), cosf(yaw), 0.f);
    const F32 u = cc_progress(phase, duration, 0);
    fov_mul = llclamp((F32)fov, 0.1f, 1.5f);
    return focus_io + away * llmax((F32)distance, 0.25f)
                    + side * ((u - 0.5f) * (F32)length);
}

// stop-motion orbit: advance during the first part of each step, then hold
LLVector3 LLCinematicCamera::patternStepOrbit(LLVOAvatar* av, const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> radius(gSavedSettings, "CinematicCamStepOrbitRadius", 3.f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamStepOrbitHeight", 1.2f);
    static LLCachedControl<F32> period(gSavedSettings, "CinematicCamStepOrbitPeriod", 12.f);
    static LLCachedControl<S32> steps(gSavedSettings, "CinematicCamStepOrbitSteps", 12);
    static LLCachedControl<F32> hold(gSavedSettings, "CinematicCamStepOrbitHold", 0.7f);
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamStepOrbitHeading", 0.f);

    const S32 count = llclamp((S32)steps, 2, 72);
    const F32 step_phase = cc_frac(phase / llmax((F32)period, 0.5f)) * count;
    const F32 idx = floorf(step_phase);
    const F32 move_fraction = llmax(1.f - llclamp((F32)hold, 0.f, 0.95f), 0.05f);
    const F32 move = llclamp(cc_frac(step_phase) / move_fraction, 0.f, 1.f);
    const F32 eased = move * move * (3.f - 2.f * move);
    const F32 a = cc_avatarYaw(av) + (F32)heading * DEG_TO_RAD
                + (idx + eased) * F_TWO_PI / count;
    return center + LLVector3(cosf(a) * llmax((F32)radius, 0.3f),
                              sinf(a) * llmax((F32)radius, 0.3f), (F32)height);
}

// drone/sports pass: a fast one-shot straight chord with look-at supplying
// the continuous yaw needed to keep the subject framed
LLVector3 LLCinematicCamera::patternCableCam(LLVOAvatar* av, const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> length(gSavedSettings, "CinematicCamCableLength", 14.f);
    static LLCachedControl<F32> miss(gSavedSettings, "CinematicCamCableMissDistance", 2.f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamCableHeight", 1.6f);
    static LLCachedControl<F32> duration(gSavedSettings, "CinematicCamCableDuration", 5.f);
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamCableHeading", 0.f);

    const F32 u = cc_progress(phase, duration, 0);
    const F32 yaw = cc_avatarYaw(av) + (F32)heading * DEG_TO_RAD;
    const LLVector3 path(cosf(yaw), sinf(yaw), 0.f);
    const LLVector3 side(-sinf(yaw), cosf(yaw), 0.f);
    return center + path * ((u - 0.5f) * (F32)length)
                  + side * (F32)miss + LLVector3(0.f, 0.f, (F32)height);
}

// dialogue hold: deterministic millimetric breathing, no wandering noise
LLVector3 LLCinematicCamera::patternBreathingHold(LLVOAvatar* av, const LLVector3& center, F32 phase)
{
    static LLCachedControl<F32> distance(gSavedSettings, "CinematicCamBreathingDistance", 2.2f);
    static LLCachedControl<F32> height(gSavedSettings, "CinematicCamBreathingHeight", 1.4f);
    static LLCachedControl<F32> amplitude(gSavedSettings, "CinematicCamBreathingAmplitude", 0.025f);
    static LLCachedControl<F32> period(gSavedSettings, "CinematicCamBreathingPeriod", 6.f);
    static LLCachedControl<F32> heading(gSavedSettings, "CinematicCamBreathingHeading", 0.f);

    const F32 t = phase * F_TWO_PI / llmax((F32)period, 1.f);
    const F32 yaw = cc_avatarYaw(av) + (F32)heading * DEG_TO_RAD;
    const LLVector3 away(cosf(yaw), sinf(yaw), 0.f);
    const LLVector3 side(-sinf(yaw), cosf(yaw), 0.f);
    return center + away * (llmax((F32)distance, 0.3f) + (F32)amplitude * sinf(t))
                  + side * ((F32)amplitude * 0.45f * sinf(t * 0.5f))
                  + LLVector3(0.f, 0.f, (F32)height + (F32)amplitude * 0.35f * cosf(t));
}

// ---------------------------------------------------------------------------
void LLCinematicCamera::updateCamera()
{
    static LLCachedControl<S32>  mode(gSavedSettings, "CinematicCamMode", 1);
    static LLCachedControl<F32>  smoothing(gSavedSettings, "CinematicCamSmoothing", 0.35f);   // seconds
    static LLCachedControl<bool> bonelock_bypass(gSavedSettings, "CinematicCamBoneLockBypassSmoothing", true);
    static LLCachedControl<bool> look_at_head(gSavedSettings, "CinematicCamLookAtHead", true);
    static LLCachedControl<bool> use_operator(gSavedSettings, "CinematicCamUseOperator", false);
    static LLCachedControl<F32>  frame_up(gSavedSettings, "CinematicCamFrameOffsetUp", 0.f);

    LLVOAvatar* av = resolveTarget();
    if (!av)
    {
        return;
    }
    const S32 current_mode = (S32)mode;
    const LLUUID current_target = av->getID();
    // Every mode and resolved-target change is a camera cut. Restart pattern,
    // tripod, smoothing, velocity, and operator state so one-shot modes begin
    // at their authored start pose. No mode intentionally preserves phase
    // continuity across a cut.
    if (gFrameCount > mLastUpdateFrame + 3 ||
        current_mode != mLastMode || current_target != mLastTargetId)
    {
        mPhase = 0.f;
        mHavePose = false;
        mWasActive = false;
        mTripodPos = LLViewerCamera::getInstance()->getOrigin();
        mPrevPos = mTripodPos;
        mPrevRot = LLViewerCamera::getInstance()->getQuaternion();
        LLCameraOperator::instance().reset();
    }
    mLastMode = current_mode;
    mLastTargetId = current_target;
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
        case MODE_BARREL_ROLL:  pos = patternBarrelRoll(av, center, mPhase, mode_roll); break;
        case MODE_CORKSCREW:    pos = patternCorkscrew(center, mPhase, mode_roll); break;
        case MODE_PENDULUM:     pos = patternPendulum(av, center, mPhase); break;
        case MODE_CONTRA_ORBIT: pos = patternContraOrbit(center, mPhase, mode_fov_mul); break;
        case MODE_FISHEYE_LUNGE:pos = patternFisheyeLunge(av, focus, mPhase, mode_fov_mul); break;
        case MODE_FLOOR_SKIMMER:pos = patternFloorSkimmer(av, center, mPhase); break;
        case MODE_BOOST_RISE:   pos = patternBoostRise(av, center, mPhase); break;
        case MODE_BOOM_OVER:    pos = patternBoomOver(av, center, mPhase); break;
        case MODE_TOP_SPIN:     pos = patternTopSpin(center, mPhase, mode_roll); break;
        case MODE_TURNTABLE:    pos = patternTurntable(center, mPhase); break;
        case MODE_FLOATING_ECU: pos = patternFloatingECU(av, focus, mPhase, mode_fov_mul); break;
        case MODE_TILT_WHIP:    pos = patternTiltWhip(av, center, mPhase, focus); break;
        case MODE_BODY_HELIX:    pos = patternBodyHelix(av, center, mPhase, focus); break;
        case MODE_DESCENT:       pos = patternDescent(av, center, mPhase, focus); break;
        case MODE_PARALLAX_SLIDE:pos = patternParallaxSlide(av, center, mPhase); break;
        case MODE_FIGURE_EIGHT:  pos = patternFigureEight(av, center, mPhase); break;
        case MODE_DETAIL_SWEEP:  pos = patternDetailSweep(av, center, mPhase, focus, mode_fov_mul); break;
        case MODE_STEP_ORBIT:    pos = patternStepOrbit(av, center, mPhase); break;
        case MODE_CABLE_CAM:     pos = patternCableCam(av, center, mPhase); break;
        case MODE_BREATHING_HOLD:pos = patternBreathingHold(av, center, mPhase); break;
        default:              return;
    }

    // Entity-clone scale is a render-only outer matrix, so both logical joint
    // positions and pattern meter offsets are still scale-1 here. Reproduce
    // that matrix for camera geometry about the same root/foot pivot. Keep the
    // scale-1 branch completely untouched.
    const F32 subject_scale = av->getUniformScale();
    if (subject_scale != 1.f)
    {
        switch ((S32)mode)
        {
            case MODE_OTS:
            case MODE_TWO_SHOT:
                // These multi-subject modes scale each body's geometry inside
                // their generators; a second A-pivot transform would distort
                // the real separation between the actors.
                break;
            case MODE_CRASH_ZOOM:
            case MODE_SLOW_ZOOM:
                // Tripod position is an absolute captured camera location.
                focus = cc_scaleAboutSubjectBase(av, focus);
                break;
            default:
                focus = cc_scaleAboutSubjectBase(av, focus);
                pos = cc_scaleAboutSubjectBase(av, pos);
                break;
        }
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
    // Bone Lock is a RIGID mount: patternBoneLock() already returns the exact
    // joint pose every frame, so the absolute one-pole would only lag it and
    // rubber-band against a MOVING mount (the dominant Bone Lock jitter). Auto-
    // bypass smoothing for Bone Lock (snap) unless the operator opts back in;
    // every other mode smooths exactly as before.
    const bool bypass_smoothing = ((S32)mode == MODE_BONE_LOCK) && bonelock_bypass;
    const F32 tau = bypass_smoothing ? 0.f : llmax((F32)smoothing, 0.f);
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
        static LLCachedControl<S32> operator_locomotion(
            gSavedSettings, "FlycamOperatorLocomotionMode", 0);
        if (!mWasActive)
        {
            LLCameraOperator::instance().reset();
        }
        LLCameraOperatorOutput op;
        if ((S32)operator_locomotion == 0)
        {
            // Legacy retains the exact variable-frame velocity path.
            LLMatrix3 axes(mSmRot);
            const LLVector3 world_vel =
                (mSmPos - mPrevPos) * (1.f / dt);
            LLQuaternion dq = mSmRot * ~mPrevRot;
            F32 d_roll, d_pitch, d_yaw;
            LLMatrix3(dq).getEulerAngles(
                &d_roll, &d_pitch, &d_yaw);

            LLCameraOperatorInput opin;
            opin.mDeltaTime = dt;
            opin.mLinearVel = LLVector3(
                world_vel * LLVector3(axes.mMatrix[0]),
                world_vel * LLVector3(axes.mMatrix[1]),
                world_vel * LLVector3(axes.mMatrix[2]));
            opin.mAngularVel =
                LLVector3(d_roll, d_pitch, d_yaw) * (1.f / dt);
            op = LLCameraOperator::instance().update(opin);
        }
        else
        {
            // Procedural paths are sampled as absolute poses at fixed tick
            // boundaries, rather than as render-frame average velocities.
            op = LLCameraOperator::instance().updateFromPose(
                dt, mSmPos, mSmRot);
        }
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
