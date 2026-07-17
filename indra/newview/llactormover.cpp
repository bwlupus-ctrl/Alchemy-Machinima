/**
 * @file llactormover.cpp
 * @brief Local ("ghost") locomotion rig -- see llactormover.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llactormover.h"

#include "llagent.h"                // gAgent global<->agent coord conversion (pathing)
#include "llanimationstates.h"      // ANIM_AGENT_WALK
#include "llappviewer.h"            // gFrameIntervalSeconds
#include "lldirectorcast.h"         // [Director] roster storage + per-actor loco anim
#include "llfloaterreg.h"           // heading preview only draws with the floater open
#include "llframetimer.h"           // per-frame idempotency for applyOverride()
#include "llgl.h"
#include "llglstates.h"             // LLGLSUIDefault (heading preview)
#include "lljoint.h"
#include "llrender.h"               // gGL (heading preview)
#include "llvector4a.h"             // downward ground raycast (pathing ground-follow)
#include "llviewercontrol.h"        // gSavedSettings, LLCachedControl
#include "llviewershadermgr.h"      // gUIProgram (heading preview)
#include "llvoavatar.h"
#include "llvoavatarself.h"         // gAgentAvatarp, isAgentAvatarValid()
#include "llworld.h"                // resolveLandHeightAgent (pathing ground-follow)
#include "pipeline.h"               // gPipeline.lineSegmentIntersectInWorld (raycast)

namespace
{
// [Director] roster storage moved into LLDirectorCast (the roster IS the
// cast); this stays the single resolution path for mover call sites.
// null id = my avatar; a stale roster id (actor left the region) resolves to
// nullptr rather than silently falling back to self, so multi-actor starts
// never double up on the agent avatar.
LLVOAvatar* resolve_actor(const LLUUID& id)
{
    return LLDirectorCast::instance().resolve(id);
}

// the locomotion anim a new Move should start for THIS actor: the cast
// member's per-actor override when set, else the shared custom override
// UUID, else ANIM_AGENT_WALK (all local playback only)
LLUUID locomotion_anim(const LLUUID& actor_id)
{
    // [Director] per-actor loco anim wins over the shared setting
    if (const LLDirectorCast::CastMember* member = LLDirectorCast::instance().getMember(actor_id))
    {
        if (member->mLocoAnim.notNull())
        {
            return member->mLocoAnim;
        }
    }
    static LLCachedControl<bool> use_custom(gSavedSettings, "ActorMoverUseCustomAnim", false);
    static LLCachedControl<std::string> custom(gSavedSettings, "ActorMoverCustomAnim", "");
    if (use_custom)
    {
        LLUUID id;
        if (LLUUID::parseUUID(custom, &id) && id.notNull())
        {
            return id;
        }
    }
    return ANIM_AGENT_WALK;
}

// ===========================================================================
// Actor pathing (P1) -- spline + traversal math
// ===========================================================================

// Centripetal Catmull-Rom, Barry-Goldman form, alpha = 0.5 SPECIFICALLY. The
// centripetal parameterization is the one variant proven free of cusps and
// self-intersections on tight corners; uniform/chordal loop and look drunk.
// P1..P2 is the live segment, P0/P3 the neighbour (or phantom) controls, and
// t runs 0..1 across the segment. Evaluated in GLOBAL double coords so it is
// origin-independent (survives region crossings) and precise over long paths.
LLVector3d centripetalCR(const LLVector3d& p0, const LLVector3d& p1,
                         const LLVector3d& p2, const LLVector3d& p3, F32 t)
{
    // knot spacing: t_{i+1} = t_i + |P_{i+1} - P_i|^alpha, alpha = 0.5.
    // A small floor keeps coincident/near-coincident nodes from dividing by 0.
    auto knot = [](F64 ti, const LLVector3d& a, const LLVector3d& b) -> F64
    {
        return ti + llmax(1e-4, sqrt((b - a).length()));
    };
    const F64 t0 = 0.0;
    const F64 t1 = knot(t0, p0, p1);
    const F64 t2 = knot(t1, p1, p2);
    const F64 t3 = knot(t2, p2, p3);
    const F64 tt = t1 + (F64)t * (t2 - t1);

    auto mix = [](const LLVector3d& a, const LLVector3d& b, F64 u) -> LLVector3d
    {
        return a * (1.0 - u) + b * u;
    };
    const LLVector3d a1 = mix(p0, p1, (tt - t0) / (t1 - t0));
    const LLVector3d a2 = mix(p1, p2, (tt - t1) / (t2 - t1));
    const LLVector3d a3 = mix(p2, p3, (tt - t2) / (t3 - t2));
    const LLVector3d b1 = mix(a1, a2, (tt - t0) / (t2 - t0));
    const LLVector3d b2 = mix(a2, a3, (tt - t1) / (t3 - t1));
    return mix(b1, b2, (tt - t1) / (t2 - t1));
}

// ground speed at arc distance d: the per-node speed override blended across
// nodes (so cadence never step-discontinuities), then the ease-in/out ramp.
F32 pathSpeedAt(const LLActorMover::Path& path, F32 d, bool skip_ease)
{
    F32 base = llmax(path.mSpeed, 0.05f);
    const S32 n = (S32)path.mNodes.size();
    if (n >= 2 && path.mNodeDist.size() >= 2)
    {
        // bracket d between node distances i .. i+1
        S32 i = 0;
        while (i + 1 < (S32)path.mNodeDist.size() && path.mNodeDist[i + 1] <= d)
        {
            ++i;
        }
        auto eff = [&](S32 k) -> F32
        {
            const F32 so = path.mNodes[llclamp(k, 0, n - 1)].mSpeedOverride;
            return so > 0.f ? so : path.mSpeed;
        };
        const F32 da = path.mNodeDist[llmin(i, (S32)path.mNodeDist.size() - 1)];
        const F32 db = (i + 1 < (S32)path.mNodeDist.size())
                           ? path.mNodeDist[i + 1] : path.mTotalLength;
        const F32 f = (db > da) ? llclamp((d - da) / (db - da), 0.f, 1.f) : 0.f;
        base = llmax(eff(i) * (1.f - f) + eff(i + 1) * f, 0.05f);
    }
    if (skip_ease)
    {
        return base;
    }
    // Ease is authored in SECONDS; convert to a lead/trail DISTANCE at the
    // nominal speed and smoothstep the factor across it. Floored just above 0
    // so the actor eases out of / settles into a standstill without sticking.
    F32 factor = 1.f;
    const F32 in_dist  = path.mSpeed * llmax(path.mEaseIn, 0.f);
    const F32 out_dist = path.mSpeed * llmax(path.mEaseOut, 0.f);
    if (in_dist > 0.01f && d < in_dist)
    {
        const F32 u = llclamp(d / in_dist, 0.f, 1.f);
        factor = llmin(factor, u * u * (3.f - 2.f * u));
    }
    if (out_dist > 0.01f && d > path.mTotalLength - out_dist)
    {
        const F32 u = llclamp((path.mTotalLength - d) / out_dist, 0.f, 1.f);
        factor = llmin(factor, u * u * (3.f - 2.f * u));
    }
    return base * llmax(factor, 0.08f);
}

// per-node ground-offset nudge, interpolated across the bracketing nodes
F32 pathGroundOffsetAt(const LLActorMover::Path& path, F32 d)
{
    const S32 n = (S32)path.mNodes.size();
    if (n == 0)
    {
        return 0.f;
    }
    if (path.mNodeDist.size() < 2)
    {
        return path.mNodes[0].mGroundOffset;
    }
    S32 i = 0;
    while (i + 1 < (S32)path.mNodeDist.size() && path.mNodeDist[i + 1] <= d)
    {
        ++i;
    }
    const F32 da = path.mNodeDist[llmin(i, (S32)path.mNodeDist.size() - 1)];
    const F32 db = (i + 1 < (S32)path.mNodeDist.size())
                       ? path.mNodeDist[i + 1] : path.mTotalLength;
    const F32 f = (db > da) ? llclamp((d - da) / (db - da), 0.f, 1.f) : 0.f;
    const F32 ga = path.mNodes[llclamp(i, 0, n - 1)].mGroundOffset;
    const F32 gb = path.mNodes[llclamp(i + 1, 0, n - 1)].mGroundOffset;
    return ga * (1.f - f) + gb * f;
}
} // anonymous namespace

// ---------------------------------------------------------------------------
// Path compilation (arc-length table) + evaluation
// ---------------------------------------------------------------------------
S32 LLActorMover::Path::segmentCount() const
{
    const S32 n = (S32)mNodes.size();
    if (n < 2)
    {
        return 0;
    }
    // loop wraps the last node back to the first (one extra segment)
    return (mEndMode == 1) ? n : (n - 1);
}

LLVector3d LLActorMover::Path::evalSegment(S32 seg, F32 t) const
{
    const S32 n = (S32)mNodes.size();
    const bool loop = (mEndMode == 1);
    auto node = [&](S32 i) -> LLVector3d
    {
        if (loop)
        {
            return mNodes[((i % n) + n) % n].mPosGlobal;
        }
        return mNodes[llclamp(i, 0, n - 1)].mPosGlobal;
    };
    const LLVector3d p1 = node(seg);
    const LLVector3d p2 = node(seg + 1);
    // phantom endpoints for an open path: reflect the end tangents (standard
    // Catmull-Rom trick). A loop instead wraps for seamless, closed tangents.
    LLVector3d p0, p3;
    if (loop)
    {
        p0 = node(seg - 1);
        p3 = node(seg + 2);
    }
    else
    {
        p0 = (seg - 1 >= 0)     ? node(seg - 1) : (p1 * 2.0 - p2);
        p3 = (seg + 2 <= n - 1) ? node(seg + 2) : (p2 * 2.0 - p1);
    }
    const LLVector3d cr  = centripetalCR(p0, p1, p2, p3, t);
    const LLVector3d lin = p1 * (1.0 - (F64)t) + p2 * (F64)t;
    // mTension blends the corner sharpness: 0 = the straight polyline (tight
    // corners, cusps impossible), 1 = the full flowing centripetal CR. Because
    // it is a lerp toward the curve, tension can only relax corners -- it can
    // never introduce a loop or overshoot.
    const F64 k = (F64)llclamp(mTension, 0.f, 1.f);
    return lin * (1.0 - k) + cr * k;
}

void LLActorMover::Path::rebuild()
{
    mArc.clear();
    mNodeDist.clear();
    mTotalLength = 0.f;
    mDirty = false;

    const S32 segs = segmentCount();
    if (segs <= 0)
    {
        return;
    }

    // sub-sample each segment and integrate chord length into a cumulative
    // arc-length table, so traversal can advance at CONSTANT GROUND SPEED
    // (distance) rather than constant spline parameter. 24 samples/segment is
    // plenty for a handful of actors and the table only rebuilds on edit.
    const S32 SUB = 24;
    F64 acc = 0.0;
    LLVector3d prev = evalSegment(0, 0.f);
    mNodeDist.push_back(0.f);               // node 0 sits at distance 0
    mArc.push_back({ 0.f, 0, 0.f });
    for (S32 s = 0; s < segs; ++s)
    {
        for (S32 j = 1; j <= SUB; ++j)
        {
            const F32 lt = (F32)j / (F32)SUB;
            const LLVector3d p = evalSegment(s, lt);
            acc += (p - prev).length();
            prev = p;
            mArc.push_back({ (F32)acc, s, lt });
        }
        mNodeDist.push_back((F32)acc);      // node (s+1) at the end of segment s
    }
    mTotalLength = (F32)acc;
}

void LLActorMover::Path::evalAtDistance(F32 d, LLVector3d& pos, LLVector3d& tangent) const
{
    if (mArc.empty())
    {
        pos = mNodes.empty() ? LLVector3d() : mNodes[0].mPosGlobal;
        tangent = LLVector3d(1.0, 0.0, 0.0);
        return;
    }
    // position at an arbitrary arc distance: binary-search the table for the
    // bracketing samples, then map back to (segment, local t) and evaluate.
    auto posAt = [this](F32 dd) -> LLVector3d
    {
        dd = llclamp(dd, 0.f, mTotalLength);
        S32 lo = 0, hi = (S32)mArc.size() - 1;
        while (lo + 1 < hi)
        {
            const S32 mid = (lo + hi) / 2;
            if (mArc[mid].mDist <= dd) { lo = mid; } else { hi = mid; }
        }
        const ArcSample& a = mArc[lo];
        const ArcSample& b = mArc[hi];
        const F32 span = llmax(1e-4f, b.mDist - a.mDist);
        const F32 f = llclamp((dd - a.mDist) / span, 0.f, 1.f);
        S32 seg; F32 lt;
        if (a.mSeg == b.mSeg)
        {
            seg = a.mSeg;
            lt  = a.mLocalT + f * (b.mLocalT - a.mLocalT);
        }
        else
        {
            // straddling a node boundary: ramp from the start of b's segment
            seg = b.mSeg;
            lt  = b.mLocalT * f;
        }
        return evalSegment(seg, lt);
    };

    pos = posAt(d);
    // unit travel tangent via a short central difference along ARC length, so
    // the facing derivative is speed-independent and stable near the ends.
    const F32 h = llmax(0.02f, mTotalLength * 0.001f);
    const LLVector3d fwd = posAt(d + h);
    const LLVector3d bck = posAt(d - h);
    tangent = fwd - bck;
    const F64 len = tangent.length();
    tangent = (len > 1e-6) ? (tangent * (1.0 / len)) : LLVector3d(1.0, 0.0, 0.0);
}

// ---------------------------------------------------------------------------
LLActorMover& LLActorMover::instance()
{
    static LLActorMover sInstance;
    return sInstance;
}

// ---------------------------------------------------------------------------
// path accessors (session-only; scene code reads/writes through these)
// ---------------------------------------------------------------------------
namespace
{
// map key: paths and moves key by the RESOLVED avatar id (never null), so a
// null "my avatar" request lands on the same key start()/applyOverride() use.
LLUUID path_key(const LLUUID& actor_id)
{
    if (actor_id.isNull() && isAgentAvatarValid())
    {
        return gAgentAvatarp->getID();
    }
    return actor_id;
}
} // anonymous namespace

LLActorMover::Path& LLActorMover::editPath(const LLUUID& actor_id)
{
    return mPaths[path_key(actor_id)];
}

const LLActorMover::Path* LLActorMover::getPath(const LLUUID& actor_id) const
{
    auto it = mPaths.find(path_key(actor_id));
    return (it == mPaths.end()) ? nullptr : &it->second;
}

bool LLActorMover::hasWalkablePath(const LLUUID& actor_id) const
{
    const Path* p = getPath(actor_id);
    return p && p->mNodes.size() >= 2;
}

void LLActorMover::clearPath(const LLUUID& actor_id)
{
    mPaths.erase(path_key(actor_id));
}

void LLActorMover::appendWaypointHere(const LLUUID& actor_id)
{
    LLVOAvatar* av = resolve_actor(actor_id);
    if (!av || !av->getRootJoint())
    {
        return;
    }
    // author at FOOT/ground level: drop the pelvis-to-foot from the rendered
    // root, then ground-snap so the node lands on terrain/stairs under the
    // actor (P2's click-to-place will snap the same way).
    LLVector3 foot = av->getRootJoint()->getWorldPosition();
    foot.mV[VZ] -= av->getPelvisToFoot();
    bool hit = false;
    foot.mV[VZ] = resolveGroundZ(av, foot, foot.mV[VZ], hit);

    Waypoint wp;
    wp.mPosGlobal = gAgent.getPosGlobalFromAgent(foot);

    Path& path = mPaths[av->getID()];
    path.mNodes.push_back(wp);
    path.markDirty();
    LL_INFOS("ActorMover") << "path waypoint " << path.mNodes.size()
                           << " for " << av->getID() << " at " << foot
                           << (hit ? " (ground-snapped)" : "") << LL_ENDL;
}

// [Director] the roster API is now an alias for Director cast membership
//static
void LLActorMover::toggleTarget(const LLUUID& id)
{
    if (id.isNull())
    {
        return;
    }
    LLDirectorCast::instance().toggle(id);
}

//static
bool LLActorMover::isTarget(const LLUUID& id)
{
    return id.notNull() && LLDirectorCast::instance().contains(id);
}

//static
const uuid_vec_t& LLActorMover::getRoster()
{
    return LLDirectorCast::instance().getIds();
}

// ---------------------------------------------------------------------------
void LLActorMover::start(const LLUUID& actor_id)
{
    static LLCachedControl<F32> speed(gSavedSettings, "ActorMoverSpeed", 1.0f);
    static LLCachedControl<F32> distance(gSavedSettings, "ActorMoverDistance", 6.f);
    static LLCachedControl<F32> heading(gSavedSettings, "ActorMoverHeading", 0.f);      // deg, rel facing
    static LLCachedControl<S32> end_mode(gSavedSettings, "ActorMoverEndMode", 0);
    static LLCachedControl<F32> nominal(gSavedSettings, "ActorMoverWalkNominal", 3.0f); // anim design speed

    LLVOAvatar* av = resolve_actor(actor_id);
    if (!av || !av->getRootJoint())
    {
        return;
    }

    // [Pathing] a path with >= 2 nodes drives a curved 3D walk; 0 or 1 node
    // falls through to the legacy straight-segment Move below (byte-identical
    // default behavior when no path is set).
    if (hasWalkablePath(av->getID()))
    {
        Path& path = mPaths[av->getID()];
        if (path.mDirty)
        {
            path.rebuild();
        }

        // restart guard mirrors the legacy path: stop a prior walk's anim so it
        // doesn't keep looping under the new cadence (a placeAt hold has none)
        auto old_it = mMoves.find(av->getID());
        if (old_it != mMoves.end() && old_it->second.mAnim.notNull())
        {
            av->stopMotion(old_it->second.mAnim);
        }

        Move mv;
        mv.mIsPath   = true;
        mv.mEndMode  = path.mEndMode;
        mv.mSpeed    = path.mSpeed;
        mv.mDistance = path.mTotalLength;
        mv.mDist     = 0.f;
        mv.mDir      = 1.f;
        mv.mNominal  = llmax((F32)nominal, 0.5f);
        mv.mAnim     = locomotion_anim(av->getID());
        mv.mFaceInit = false;
        mMoves[av->getID()] = mv;

        // cadence-lock seeds from the path speed; advancePath() then tracks the
        // INSTANTANEOUS ground speed (ease + per-node overrides) each frame
        av->startMotion(mv.mAnim);
        av->setAnimTimeFactor(llclamp(mv.mSpeed, 0.05f, 10.f) / mv.mNominal);

        LL_INFOS("ActorMover") << "ghost path walk: " << av->getID()
                               << " nodes " << path.mNodes.size()
                               << " len " << path.mTotalLength
                               << " endmode " << path.mEndMode << LL_ENDL;
        return;
    }

    // travel bearing = actor's current facing plus the heading trim
    LLVector3 at = LLVector3(1.f, 0.f, 0.f) * av->getRenderRotation();
    const F32 yaw = atan2f(at.mV[VY], at.mV[VX]) + (F32)heading * DEG_TO_RAD;

    Move mv;
    mv.mOrigin   = av->getRootJoint()->getWorldPosition();   // rendered pose, incl. a prior move
    mv.mHeading  = yaw;
    mv.mSpeed    = llclamp((F32)speed, 0.05f, 10.f);
    mv.mDistance = llmax((F32)distance, 0.1f);
    mv.mEndMode  = llclamp((S32)end_mode, 0, 2);
    mv.mT        = 0.f;
    mv.mAnim     = locomotion_anim(av->getID());

    // restarting an actor mid-move with a different anim: stop the old one so
    // it doesn't keep looping under the new cycle (a placeAt() hold has no
    // anim, hence the null guard)
    auto old_it = mMoves.find(av->getID());
    if (old_it != mMoves.end() && old_it->second.mAnim.notNull()
        && old_it->second.mAnim != mv.mAnim)
    {
        av->stopMotion(old_it->second.mAnim);
    }

    mMoves[av->getID()] = mv;

    // cadence lock: walk-cycle clock scaled to actual ground speed, so foot
    // plants match the traversal at ANY speed. Local motion + local clock:
    // nothing is sent to the sim (custom anims play locally too).
    av->startMotion(mv.mAnim);
    av->setAnimTimeFactor(mv.mSpeed / llmax((F32)nominal, 0.5f));

    LL_INFOS("ActorMover") << "ghost move: " << av->getID() << " speed " << mv.mSpeed
                           << " dist " << mv.mDistance << " anim " << mv.mAnim << LL_ENDL;
}

void LLActorMover::startAll()
{
    const uuid_vec_t& roster = getRoster();
    if (roster.empty())
    {
        start(LLUUID::null);
        return;
    }
    for (const LLUUID& id : roster)
    {
        start(id);
    }
}

void LLActorMover::stop(const LLUUID& actor_id)
{
    LLVOAvatar* av = resolve_actor(actor_id);
    if (av)
    {
        auto it = mMoves.find(av->getID());
        if (it != mMoves.end())
        {
            const LLUUID anim = it->second.mAnim;
            const LLUUID dwell_anim = it->second.mDwellAnim;    // path dwell, if any
            mMoves.erase(it);
            if (anim.notNull())     // placeAt() holds carry no anim
            {
                av->stopMotion(anim);
            }
            if (dwell_anim.notNull())   // stopped mid-dwell: kill the node anim too
            {
                av->stopMotion(dwell_anim);
            }
            av->setAnimTimeFactor(1.f);
        }
    }
}

void LLActorMover::stopAll()
{
    for (auto& pair : mMoves)
    {
        if (LLVOAvatar* mav = resolve_actor(pair.first))
        {
            if (pair.second.mAnim.notNull())    // placeAt() holds carry no anim
            {
                mav->stopMotion(pair.second.mAnim);
            }
            if (pair.second.mDwellAnim.notNull())   // path stopped mid-dwell
            {
                mav->stopMotion(pair.second.mDwellAnim);
            }
            mav->setAnimTimeFactor(1.f);
        }
    }
    mMoves.clear();
}

// [Director] Reset-to-Marks snap-back: pin the rendered root at a mark with
// a zero-speed hold Move (same override path as a walk, so it wins against
// LLControlAvatar::matchVolumeTransform() for animesh too). The hold keeps
// the actor's current facing; a later start() walks FROM the mark because
// start() captures the CURRENT rendered pose as its origin.
void LLActorMover::placeAt(const LLUUID& actor_id, const LLVector3& pos)
{
    LLVOAvatar* av = resolve_actor(actor_id);
    if (!av || !av->getRootJoint())
    {
        return;
    }

    // a reset while moving stops the move (and its anim/cadence lock) first
    stop(actor_id);

    LLVector3 at = LLVector3(1.f, 0.f, 0.f) * av->getRenderRotation();

    Move mv;
    mv.mOrigin   = pos;
    mv.mHeading  = atan2f(at.mV[VY], at.mV[VX]);
    mv.mSpeed    = 0.f;         // zero speed + zero distance = pinned hold
    mv.mDistance = 0.f;
    mv.mEndMode  = 0;           // hold
    mv.mT        = 0.f;
    mv.mAnim.setNull();         // no locomotion anim for a hold

    mMoves[av->getID()] = mv;

    LL_INFOS("ActorMover") << "hold at mark: " << av->getID() << " pos " << pos << LL_ENDL;
}

// ---------------------------------------------------------------------------
//static
F32 LLActorMover::pathParam(const Move& mv, F32* face)
{
    // path parameter + travel direction (ping-pong reverses facing on the
    // return leg so the actor walks back rather than moonwalking)
    const F32 total = mv.mSpeed * mv.mT;
    F32 s;
    F32 f = mv.mHeading;
    switch (mv.mEndMode)
    {
        case 1:     // loop
            s = fmodf(total, mv.mDistance);
            break;
        case 2:     // ping-pong
        {
            const F32 c = fmodf(total, 2.f * mv.mDistance);
            if (c < mv.mDistance)
            {
                s = c;
            }
            else
            {
                s = 2.f * mv.mDistance - c;
                f = mv.mHeading + F_PI;
            }
            break;
        }
        default:    // hold at end
            s = llmin(total, mv.mDistance);
            break;
    }
    if (face)
    {
        *face = f;
    }
    return s;
}

bool LLActorMover::getProgress(const LLUUID& actor_id, F32& traveled, F32& total) const
{
    LLUUID key = actor_id;
    if (key.isNull())
    {
        if (!isAgentAvatarValid())
        {
            return false;
        }
        key = gAgentAvatarp->getID();
    }
    auto it = mMoves.find(key);
    if (it == mMoves.end())
    {
        return false;
    }
    if (it->second.mIsPath)
    {
        // arc-length distance traveled vs the path's total length
        traveled = it->second.mDist;
        total = it->second.mDistance;
        return true;
    }
    traveled = pathParam(it->second, nullptr);
    total = it->second.mDistance;
    return true;
}

// ---------------------------------------------------------------------------
bool LLActorMover::applyOverride(LLVOAvatar* av)
{
    if (mMoves.empty() || !av)
    {
        return false;
    }
    auto it = mMoves.find(av->getID());
    if (it == mMoves.end())
    {
        return false;
    }
    if (av->isDead() || !av->getRootJoint())
    {
        mMoves.erase(it);
        return false;
    }

    Move& mv = it->second;

    // advance the path clock only once per frame; later calls in the same
    // frame (e.g. LLControlAvatar::matchVolumeTransform() re-syncing an
    // animesh root to its linkset) just re-assert the cached override pose
    const U32 frame = LLFrameTimer::getFrameCount();
    if (mv.mLastFrame != frame)
    {
        mv.mLastFrame = frame;
        const F32 dt = llclamp(gFrameIntervalSeconds.value(), 0.f, 0.25f);

        if (mv.mIsPath)
        {
            // [Pathing] evaluate the Catmull-Rom path at the actor's arc clock
            advancePath(av, mv, dt);
        }
        else
        {
            // ---- legacy straight-segment move (unchanged) ----
            mv.mT += dt;

            F32 face;
            const F32 s = pathParam(mv, &face);

            if (mv.mEndMode == 0 && s >= mv.mDistance && av->findMotion(mv.mAnim))
            {
                // arrived: settle into a stand instead of walking in place
                av->stopMotion(mv.mAnim);
                av->setAnimTimeFactor(1.f);
            }

            const LLVector3 dir(cosf(mv.mHeading), sinf(mv.mHeading), 0.f);
            mv.mCurPos = mv.mOrigin + dir * s;
            mv.mCurRot.setEulerAngles(0.f, 0.f, face);
        }
    }

    av->getRootJoint()->setWorldPosition(mv.mCurPos);
    av->getRootJoint()->setWorldRotation(mv.mCurRot);
    return true;
}

// ---------------------------------------------------------------------------
// Path traversal: one frame of advancing along the Catmull-Rom curve. Called
// once per frame from applyOverride() after the path clock has ticked. Sets
// mv.mCurPos (agent frame) + mv.mCurRot (turn-rate-smoothed facing).
// ---------------------------------------------------------------------------
void LLActorMover::advancePath(LLVOAvatar* av, Move& mv, F32 dt)
{
    Path& path = mPaths[av->getID()];       // start() guaranteed this exists
    if (path.mDirty)
    {
        path.rebuild();
    }
    const S32 n = (S32)path.mNodes.size();
    if (n < 2 || path.mTotalLength <= 0.001f)
    {
        // degenerate (all nodes coincident): hold at node 0 so we never place
        // the root at the origin
        LLVector3 a = gAgent.getPosAgentFromGlobal(path.mNodes.empty()
                          ? LLVector3d() : path.mNodes[0].mPosGlobal);
        a.mV[VZ] += av->getPelvisToFoot();
        mv.mCurPos = a;
        return;
    }

    const bool loop     = (path.mEndMode == 1);
    const bool pingpong = (path.mEndMode == 2);

    // seed facing from the actor's CURRENT yaw on the first frame, so the
    // turn-rate clamp eases it toward the travel direction rather than snapping
    if (!mv.mFaceInit)
    {
        LLVector3 at = LLVector3(1.f, 0.f, 0.f) * av->getRenderRotation();
        mv.mCurRot.setEulerAngles(0.f, 0.f, atan2f(at.mV[VY], at.mV[VX]));
        mv.mFaceInit = true;
    }

    // ---- dwell: hold at a node, walk clock frozen ----
    if (mv.mDwellNode >= 0)
    {
        mv.mDwellT += dt;
        const F32 want = (mv.mDwellNode < n) ? path.mNodes[mv.mDwellNode].mDwell : 0.f;
        if (mv.mDwellT >= want)
        {
            // resume: stop the dwell anim (if any) and restart the loco walk
            if (mv.mDwellAnim.notNull())
            {
                av->stopMotion(mv.mDwellAnim);
                mv.mDwellAnim.setNull();
            }
            if (mv.mAnim.notNull())
            {
                av->startMotion(mv.mAnim);
            }
            mv.mLastDwellNode = mv.mDwellNode;
            mv.mDwellNode = -1;
        }
        return;     // position/facing stay at the cached dwell pose
    }

    // ---- advance arc-length distance at the instantaneous ground speed ----
    const F32 speed = pathSpeedAt(path, mv.mDist, loop || pingpong);
    const F32 old_d = mv.mDist;
    F32 nd = mv.mDist + mv.mDir * speed * dt;

    mv.mArrived = false;
    if (loop)
    {
        if (nd >= path.mTotalLength) { nd = fmodf(nd, path.mTotalLength); mv.mLastDwellNode = -1; }
        else if (nd < 0.f)           { nd = path.mTotalLength + fmodf(nd, path.mTotalLength); }
    }
    else if (pingpong)
    {
        if (nd > path.mTotalLength)  { nd = 2.f * path.mTotalLength - nd; mv.mDir = -1.f; mv.mLastDwellNode = -1; }
        else if (nd < 0.f)           { nd = -nd; mv.mDir = 1.f; mv.mLastDwellNode = -1; }
    }
    else // stop
    {
        if (nd >= path.mTotalLength) { nd = path.mTotalLength; mv.mArrived = true; }
    }

    // ---- dwell detection: did we cross an interior node with dwell > 0? ----
    if (mv.mDwellNode < 0)
    {
        const F32 lo = llmin(old_d, nd);
        const F32 hi = llmax(old_d, nd);
        S32 best = -1;
        F32 best_d = 0.f;
        for (S32 i = 1; i <= n - 2; ++i)        // interior nodes only
        {
            if (i == mv.mLastDwellNode || path.mNodes[i].mDwell <= 0.f)
            {
                continue;
            }
            const F32 ndpos = (i < (S32)path.mNodeDist.size()) ? path.mNodeDist[i] : -1.f;
            if (ndpos > lo && ndpos <= hi)
            {
                // pick the first node reached in the travel direction
                if (best < 0 || (mv.mDir >= 0.f ? ndpos < best_d : ndpos > best_d))
                {
                    best = i;
                    best_d = ndpos;
                }
            }
        }
        if (best >= 0)
        {
            nd = best_d;
            mv.mDwellNode = best;
            mv.mDwellT = 0.f;
            // switch to a stand (or the node's anim) so the actor isn't walking
            // in place while held
            if (mv.mAnim.notNull())
            {
                av->stopMotion(mv.mAnim);
                av->setAnimTimeFactor(1.f);
            }
            const LLUUID& na = path.mNodes[best].mAnim;
            if (na.notNull())
            {
                av->startMotion(na);
                mv.mDwellAnim = na;
            }
        }
    }

    mv.mDist = nd;

    // ---- evaluate the spline (global) ----
    LLVector3d pos_global, tan_global;
    path.evalAtDistance(mv.mDist, pos_global, tan_global);
    if (mv.mDir < 0.f)
    {
        tan_global = -tan_global;   // ping-pong return leg faces backward
    }

    // ---- agent frame + ground-follow ----
    LLVector3 agent = gAgent.getPosAgentFromGlobal(pos_global);
    F32 foot_z = agent.mV[VZ];
    bool ground_hit = false;
    if (path.mGroundFollow)
    {
        foot_z = resolveGroundZ(av, agent, agent.mV[VZ], ground_hit);
    }
    foot_z += pathGroundOffsetAt(path, mv.mDist);
    agent.mV[VZ] = foot_z + av->getPelvisToFoot();
    mv.mCurPos = agent;

    // ---- facing: target yaw from tangent (or arrival facing), turn-rate clamp ----
    F32 target_yaw;
    if (mv.mArrived && path.mArrivalFacingMode != 0)
    {
        if (path.mArrivalFacingMode == 2 && path.mArrivalTarget.notNull())
        {
            // face a cast member
            if (LLVOAvatar* tgt = LLDirectorCast::instance().resolve(path.mArrivalTarget))
            {
                if (tgt->getRootJoint())
                {
                    LLVector3 d = tgt->getRootJoint()->getWorldPosition() - agent;
                    target_yaw = (d.magVecSquared() > 1e-4f)
                                     ? atan2f(d.mV[VY], d.mV[VX])
                                     : atan2f((F32)tan_global.mdV[VY], (F32)tan_global.mdV[VX]);
                }
                else
                {
                    target_yaw = path.mArrivalDir;
                }
            }
            else
            {
                target_yaw = path.mArrivalDir;
            }
        }
        else
        {
            target_yaw = path.mArrivalDir;      // compass direction (world yaw)
        }
    }
    else
    {
        target_yaw = atan2f((F32)tan_global.mdV[VY], (F32)tan_global.mdV[VX]);
    }

    // optional pitch-to-slope: from the path's climb tangent (a P1 approximation;
    // a surface-normal pitch is a P2 refinement noted in the report)
    F32 target_pitch = 0.f;
    if (path.mPitchToSlope)
    {
        const F32 horiz = sqrtf((F32)(tan_global.mdV[VX] * tan_global.mdV[VX]
                                    + tan_global.mdV[VY] * tan_global.mdV[VY]));
        target_pitch = atan2f(-(F32)tan_global.mdV[VZ], llmax(horiz, 1e-4f));
    }

    // max turn-rate clamp on YAW so sharp corners ease instead of snapping;
    // straightaways (delta ~ 0) are untouched. Pitch tracks directly (slope
    // changes are gradual).
    static LLCachedControl<F32> turn_rate(gSavedSettings, "PathTurnRateDegPerSec", 180.f);
    LLVector3 cur_at = LLVector3(1.f, 0.f, 0.f) * mv.mCurRot;
    const F32 cur_yaw = atan2f(cur_at.mV[VY], cur_at.mV[VX]);
    F32 dyaw = target_yaw - cur_yaw;
    while (dyaw >  F_PI) { dyaw -= 2.f * F_PI; }
    while (dyaw < -F_PI) { dyaw += 2.f * F_PI; }
    const F32 max_step = llmax(1.f, (F32)turn_rate) * DEG_TO_RAD * dt;
    dyaw = llclamp(dyaw, -max_step, max_step);
    mv.mCurRot.setEulerAngles(0.f, target_pitch, cur_yaw + dyaw);

    // ---- cadence lock from the instantaneous ground speed ----
    // TODO(gait): optional walk/run anim swap by a speed threshold -- left as a
    // hook. Swapping the loco anim mid-walk risks foot-slide/pop and would
    // override a per-actor custom anim, so it wants its own tuning pass.
    if (!mv.mArrived && mv.mAnim.notNull())
    {
        av->setAnimTimeFactor(speed / llmax(mv.mNominal, 0.5f));
    }

    // ---- arrival: settle into a stand at the last node (stop mode) ----
    if (mv.mArrived && mv.mAnim.notNull())
    {
        if (av->findMotion(mv.mAnim))
        {
            av->stopMotion(mv.mAnim);
        }
        av->setAnimTimeFactor(1.f);
        mv.mAnim.setNull();     // stop() won't double-stop; facing keeps easing
    }
}

// ---------------------------------------------------------------------------
// Ground-follow resolver: the ground Z (agent frame) under an actor.
//   1) capped downward object raycast -> stairs / prim floors (the robust bit)
//   2) else terrain land height, when the actor sits near it (slopes)
//   3) else the interpolated spline Z (fallback_z), so an elevated actor the
//      short ray missed is never yanked down to the terrain far below.
// Cheap enough for a handful of actors; per-frame per-actor object raycasts
// are the piece that needs in-world A/B on real prim stairs (see report).
// ---------------------------------------------------------------------------
//static
F32 LLActorMover::resolveGroundZ(LLVOAvatar* av, const LLVector3& agent_pos,
                                 F32 fallback_z, bool& out_hit)
{
    (void)av;   // the actor's own rigged mesh is excluded via pick_rigged=false
    out_hit = false;

    // capped downward ray: start a little above the foot target, cast down a
    // bounded distance. pick_rigged = false means the actor's own rigged mesh
    // (body/clothing/attachments) is never hit; starting above the feet keeps
    // the body (which is above) out of the ray too.
    const F32 RAY_UP   = 1.5f;
    const F32 RAY_DOWN = 3.0f;
    LLVector4a ray_start, ray_end;
    ray_start.set(agent_pos.mV[VX], agent_pos.mV[VY], agent_pos.mV[VZ] + RAY_UP);
    ray_end.set(agent_pos.mV[VX], agent_pos.mV[VY], agent_pos.mV[VZ] - RAY_DOWN);

    LLVector4a intersect;
    S32 face_hit = -1;
    LLViewerObject* obj = gPipeline.lineSegmentIntersectInWorld(
        ray_start, ray_end,
        /*pick_transparent*/     false,
        /*pick_rigged*/          false,
        /*pick_unselectable*/    true,
        /*pick_reflection_probe*/false,
        &face_hit, &intersect, /*tex_coord*/ nullptr,
        /*normal*/ nullptr, /*tangent*/ nullptr);
    if (obj)
    {
        out_hit = true;
        return intersect.getF32ptr()[VZ];
    }

    // no prim under the actor: use terrain only when the spline sits near it
    // (walking bare ground), otherwise keep the authored/spline height
    const F32 land = LLWorld::getInstance()->resolveLandHeightAgent(agent_pos);
    if (fallback_z - land <= RAY_DOWN + 0.5f)
    {
        out_hit = true;
        return land;
    }
    return fallback_z;
}

// ---------------------------------------------------------------------------
void LLActorMover::renderHeadingPreview()
{
    static LLCachedControl<bool> show(gSavedSettings, "ActorMoverShowHeading", false);
    if (!show)
    {
        return;
    }
    // an operator floater must be up: the standalone mover or the Director
    // Console (its Move tab drives the same heading/distance settings)
    LLFloater* floaterp = LLFloaterReg::findInstance("actor_mover");
    if (!floaterp || !floaterp->getVisible())
    {
        floaterp = LLFloaterReg::findInstance("director");
        if (!floaterp || !floaterp->getVisible())
        {
            return;
        }
    }

    static LLCachedControl<F32> distance(gSavedSettings, "ActorMoverDistance", 6.f);
    static LLCachedControl<F32> heading(gSavedSettings, "ActorMoverHeading", 0.f);
    const F32 dist = llmax((F32)distance, 0.1f);

    // same beacon-style local debug lines as renderObjectBeacons(): UI shader,
    // no texture, no depth writes -- strictly a client-side overlay
    LLGLSUIDefault gls_ui;
    gUIProgram.bind();
    gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
    gGL.setLineWidth(2.f);
    gGL.begin(LLRender::LINES);
    gGL.color4f(1.f, 0.75f, 0.2f, 0.9f);

    uuid_vec_t roster = getRoster();
    if (roster.empty())
    {
        roster.push_back(LLUUID::null);     // my avatar
    }
    for (const LLUUID& id : roster)
    {
        LLVOAvatar* av = resolve_actor(id);
        if (!av || !av->getRootJoint())
        {
            continue;
        }

        // planned bearing = same math as start(): current facing + trim
        LLVector3 at = LLVector3(1.f, 0.f, 0.f) * av->getRenderRotation();
        const F32 yaw = atan2f(at.mV[VY], at.mV[VX]) + (F32)heading * DEG_TO_RAD;
        const LLVector3 dir(cosf(yaw), sinf(yaw), 0.f);

        LLVector3 origin = av->getRootJoint()->getWorldPosition();
        origin.mV[VZ] += 0.15f - av->getPelvisToFoot();     // just above ground
        const LLVector3 end = origin + dir * dist;

        // main segment (ping-pong/loop share the same single leg)
        gGL.vertex3fv(origin.mV);
        gGL.vertex3fv(end.mV);

        // arrowhead barbs + a small vertical tick at the endpoint
        const F32 barb_len = llmin(0.4f, dist * 0.25f);
        for (F32 side : { 1.f, -1.f })
        {
            const F32 b = yaw + side * 2.6f;    // ~150 deg back from travel
            gGL.vertex3fv(end.mV);
            gGL.vertex3f(end.mV[VX] + barb_len * cosf(b),
                         end.mV[VY] + barb_len * sinf(b),
                         end.mV[VZ]);
        }
        gGL.vertex3fv(end.mV);
        gGL.vertex3f(end.mV[VX], end.mV[VY], end.mV[VZ] + 0.3f);
    }

    gGL.end();
    gGL.setLineWidth(1.f);
    gGL.flush();
}
