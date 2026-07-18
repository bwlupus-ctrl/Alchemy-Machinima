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

#include <algorithm>                // std::reverse (path reverse op)

#include "llagent.h"                // gAgent global<->agent coord conversion (pathing)
#include "llanimationstates.h"      // ANIM_AGENT_WALK
#include "llappviewer.h"            // gFrameIntervalSeconds
#include "lldirectorcast.h"         // [Director] roster storage + per-actor loco anim
#include "llflycamrecorder.h"       // sync-to-take: the recorder playhead is the clock
#include "llfloaterreg.h"           // heading preview only draws with the floater open
#include "llframetimer.h"           // per-frame idempotency for applyOverride()
#include "llgl.h"
#include "llglstates.h"             // LLGLSUIDefault (heading preview)
#include "lljoint.h"
#include "llrender.h"               // gGL (heading preview)
#include "llvector4a.h"             // downward ground raycast (pathing ground-follow)
#include "llviewercamera.h"         // camera basis for billboarded path node numbers
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

// per-node scalar (ground-offset nudge or authored root-above-ground height),
// interpolated across the bracketing nodes at arc distance d. Both fields ride
// the same node-distance bracket, so one helper covers them.
F32 pathNodeScalarAt(const LLActorMover::Path& path, F32 d,
                     F32 (*field)(const LLActorMover::Waypoint&))
{
    const S32 n = (S32)path.mNodes.size();
    if (n == 0)
    {
        return 0.f;
    }
    if (path.mNodeDist.size() < 2)
    {
        return field(path.mNodes[0]);
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
    const F32 va = field(path.mNodes[llclamp(i, 0, n - 1)]);
    const F32 vb = field(path.mNodes[llclamp(i + 1, 0, n - 1)]);
    return va * (1.f - f) + vb * f;
}

F32 pathGroundOffsetAt(const LLActorMover::Path& path, F32 d)
{
    return pathNodeScalarAt(path, d,
        [](const LLActorMover::Waypoint& w) { return w.mGroundOffset; });
}

// authored root height above the ground at arc distance d: the stable standing
// height the placement adds on top of the (authored or live) ground, matching
// the legacy straight-move convention (root held at its captured height).
F32 pathRootAboveAt(const LLActorMover::Path& path, F32 d)
{
    return pathNodeScalarAt(path, d,
        [](const LLActorMover::Waypoint& w) { return w.mRootAbove; });
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
    // Store the node at FOOT/GROUND level (mPosGlobal) AND the authored root
    // height above that ground (mRootAbove). The ground level: drop the
    // pelvis-to-foot from the rendered root, then ground-snap so the node lands
    // on terrain/stairs under the actor (P2's click-to-place snaps the same
    // way). mRootAbove is the RESIDUAL back up to the true rendered root, so the
    // capture->walk round-trip reproduces the exact captured root height no
    // matter what the ground-snap returned or how the walk pose differs -- the
    // same "hold the captured root" convention the legacy straight move uses.
    const LLVector3 root = av->getRootJoint()->getWorldPosition();
    const F32 p2f = av->getPelvisToFoot();
    LLVector3 foot = root;
    foot.mV[VZ] -= p2f;
    bool hit = false;
    const F32 ground_z = resolveGroundZ(av, foot, foot.mV[VZ], hit);
    foot.mV[VZ] = ground_z;

    Waypoint wp;
    wp.mPosGlobal = gAgent.getPosGlobalFromAgent(foot);
    wp.mRootAbove = root.mV[VZ] - ground_z;     // captured standing height above ground

    Path& path = mPaths[av->getID()];
    path.mNodes.push_back(wp);
    path.markDirty();
    LL_INFOS("ActorMover") << "path waypoint " << path.mNodes.size()
                           << " for " << av->getID() << " at " << foot
                           << (hit ? " (ground-snapped)" : "") << LL_ENDL;

    static LLCachedControl<bool> zdbg(gSavedSettings, "ActorMoverPathZDebug", false);
    if (zdbg)
    {
        LL_INFOS("ActorPath") << "capture node " << path.mNodes.size()
            << " root.z=" << root.mV[VZ] << " pelvisToFoot=" << p2f
            << " foot.z(pre-snap)=" << (root.mV[VZ] - p2f)
            << " groundZ=" << ground_z << " hit=" << (hit ? 1 : 0)
            << " rootAbove=" << wp.mRootAbove << LL_ENDL;
    }
}

// ---------------------------------------------------------------------------
// P2 index-based editing. All operate on mPaths[path_key(actor)] and mark the
// path dirty so the arc-length table rebuilds on the next eval/render. A placed
// or moved node stores its foot/ground position (mPosGlobal) plus mRootAbove --
// the actor's standing height above that ground -- so a walked node sits
// feet-on-ground exactly like appendWaypointHere (no hover). ground_pos is the
// GLOBAL surface point (an in-world pick already snaps to terrain/prim).
// ---------------------------------------------------------------------------
namespace
{
// captured standing height above the ground for a placed node: the resolving
// actor's live pelvis-to-foot (feet-on-ground convention). A fallback keeps a
// node from sitting at Z=0 when the actor is momentarily unresolvable.
F32 capture_root_above(LLVOAvatar* av)
{
    return (av && av->getRootJoint()) ? llmax(0.f, av->getPelvisToFoot()) : 0.9f;
}
} // anonymous namespace

S32 LLActorMover::appendWaypointAt(const LLUUID& actor_id, const LLVector3d& ground_pos)
{
    Path& path = editPath(actor_id);
    Waypoint wp;
    wp.mPosGlobal = ground_pos;
    wp.mRootAbove = capture_root_above(resolve_actor(actor_id));
    path.mNodes.push_back(wp);
    path.markDirty();
    return (S32)path.mNodes.size() - 1;
}

S32 LLActorMover::insertWaypoint(const LLUUID& actor_id, S32 index, const LLVector3d& ground_pos)
{
    Path& path = editPath(actor_id);
    const S32 at = llclamp(index, 0, (S32)path.mNodes.size());
    Waypoint wp;
    wp.mPosGlobal = ground_pos;
    wp.mRootAbove = capture_root_above(resolve_actor(actor_id));
    path.mNodes.insert(path.mNodes.begin() + at, wp);
    path.markDirty();
    return at;
}

bool LLActorMover::moveWaypoint(const LLUUID& actor_id, S32 index, const LLVector3d& new_ground_pos)
{
    const Path* cp = getPath(actor_id);
    if (!cp || index < 0 || index >= (S32)cp->mNodes.size())
    {
        return false;
    }
    Path& path = editPath(actor_id);
    Waypoint& wp = path.mNodes[index];
    wp.mPosGlobal = new_ground_pos;
    // re-capture the standing height so a node dragged onto higher/lower ground
    // still plants feet-on-ground rather than keeping its old lift
    wp.mRootAbove = capture_root_above(resolve_actor(actor_id));
    path.markDirty();
    return true;
}

bool LLActorMover::deleteWaypoint(const LLUUID& actor_id, S32 index)
{
    const Path* cp = getPath(actor_id);
    if (!cp || index < 0 || index >= (S32)cp->mNodes.size())
    {
        return false;
    }
    Path& path = editPath(actor_id);
    path.mNodes.erase(path.mNodes.begin() + index);
    path.markDirty();
    // an emptied path is dropped so hasWalkablePath()/render fall back cleanly
    if (path.mNodes.empty())
    {
        clearPath(actor_id);
    }
    return true;
}

bool LLActorMover::setNodeDwell(const LLUUID& actor_id, S32 index, F32 dwell_s)
{
    const Path* cp = getPath(actor_id);
    if (!cp || index < 0 || index >= (S32)cp->mNodes.size())
    {
        return false;
    }
    Path& path = editPath(actor_id);
    path.mNodes[index].mDwell = llmax(0.f, dwell_s);
    path.markDirty();
    return true;
}

bool LLActorMover::setNodeSpeed(const LLUUID& actor_id, S32 index, F32 speed_override)
{
    const Path* cp = getPath(actor_id);
    if (!cp || index < 0 || index >= (S32)cp->mNodes.size())
    {
        return false;
    }
    Path& path = editPath(actor_id);
    path.mNodes[index].mSpeedOverride = llmax(0.f, speed_override);
    path.markDirty();
    return true;
}

bool LLActorMover::setNodeAnim(const LLUUID& actor_id, S32 index, const LLUUID& anim)
{
    const Path* cp = getPath(actor_id);
    if (!cp || index < 0 || index >= (S32)cp->mNodes.size())
    {
        return false;
    }
    Path& path = editPath(actor_id);
    path.mNodes[index].mAnim = anim;
    path.markDirty();
    return true;
}

bool LLActorMover::setNodeGroundOffset(const LLUUID& actor_id, S32 index, F32 offset_m)
{
    const Path* cp = getPath(actor_id);
    if (!cp || index < 0 || index >= (S32)cp->mNodes.size())
    {
        return false;
    }
    Path& path = editPath(actor_id);
    path.mNodes[index].mGroundOffset = offset_m;
    path.markDirty();
    return true;
}

// ===========================================================================
// P3 QOL bundle: length/duration readout, undo/redo, reverse/mirror/loop-close/
// copy-to-actor, walk-to-here. Structural ops keep every node's per-node data
// attached (they operate on the whole Waypoint) and rebuild the arc table; they
// are gated off while the actor is walking a path so a running arc clock is
// never left indexing rewritten geometry.
// ===========================================================================
bool LLActorMover::isPathWalking(const LLUUID& actor_id) const
{
    auto it = mMoves.find(path_key(actor_id));
    return it != mMoves.end() && it->second.mIsPath;
}

// ===========================================================================
// P3 Choreography: follow-the-leader (procession by reference). A follower
// rides the leader's spline at an offset; see advanceFollower() for traversal.
// ===========================================================================
bool LLActorMover::wouldFollowCycle(const LLUUID& follower,
                                    const LLUUID& candidate_leader) const
{
    const LLUUID fk = path_key(follower);
    LLUUID cur = path_key(candidate_leader);
    if (cur.isNull() || cur == fk)
    {
        return true;        // self-follow is a degenerate cycle
    }
    // walk the candidate's leader chain: if it reaches the follower, the new edge
    // would close a loop. The depth cap is a belt-and-braces guard against a
    // pre-existing corrupt chain (setFollow already refuses cycles, so none can
    // form, but a bounded walk can never hang).
    for (S32 i = 0; i < 64 && cur.notNull(); ++i)
    {
        if (cur == fk)
        {
            return true;
        }
        auto it = mFollows.find(cur);
        if (it == mFollows.end())
        {
            break;          // chain ends at a non-follower: no cycle
        }
        cur = it->second.mLeader;
    }
    return false;
}

bool LLActorMover::setFollow(const LLUUID& follower, const LLUUID& leader,
                             S32 mode, F32 offset)
{
    const LLUUID fk = path_key(follower);
    const LLUUID lk = path_key(leader);
    if (fk.isNull() || lk.isNull() || lk == fk)
    {
        return false;       // no self-follow, no null actor
    }
    if (wouldFollowCycle(follower, leader))
    {
        return false;       // refuse a relationship that would loop the procession
    }
    Follow f;
    f.mLeader = lk;
    f.mMode   = llclamp(mode, 0, 1);
    f.mOffset = llmax(0.f, offset);
    mFollows[fk] = f;
    LL_INFOS("ActorMover") << "follow set: " << fk << " -> " << lk
                           << " mode " << f.mMode << " offset " << f.mOffset << LL_ENDL;
    return true;
}

void LLActorMover::clearFollow(const LLUUID& follower)
{
    const LLUUID fk = path_key(follower);
    auto it = mFollows.find(fk);
    if (it == mFollows.end())
    {
        return;
    }
    mFollows.erase(it);
    // If a follow-driven ghost walk is running and the actor has no path of its
    // own, end it cleanly here -- otherwise advancePath would fall through to an
    // empty own-path and hold the actor at the coordinate origin.
    auto mit = mMoves.find(fk);
    if (mit != mMoves.end() && mit->second.mIsPath && !hasWalkablePath(follower))
    {
        stop(follower);
    }
}

bool LLActorMover::getFollow(const LLUUID& follower, LLUUID& leader,
                             S32& mode, F32& offset) const
{
    auto it = mFollows.find(path_key(follower));
    if (it == mFollows.end())
    {
        return false;
    }
    leader = it->second.mLeader;
    mode   = it->second.mMode;
    offset = it->second.mOffset;
    return true;
}

bool LLActorMover::isFollowing(const LLUUID& follower) const
{
    return mFollows.count(path_key(follower)) != 0;
}

bool LLActorMover::getPathStats(const LLUUID& actor_id, F32& out_length, F32& out_duration)
{
    const Path* cp = getPath(actor_id);
    if (!cp || cp->mNodes.size() < 2)
    {
        return false;
    }
    Path& p = editPath(actor_id);       // non-const: may need the arc table
    if (p.mDirty)
    {
        p.rebuild();
    }
    out_length = p.mTotalLength;

    // integrate travel time over the arc samples at the same eased / per-node
    // override ground speed the walk uses; loop/ping-pong skip ease (as the walk
    // does). One pass: one lap in loop mode, one-way in ping-pong.
    const bool skip_ease = (p.mEndMode == 1 || p.mEndMode == 2);
    F32 dur = 0.f;
    for (size_t i = 1; i < p.mArc.size(); ++i)
    {
        const F32 ds = p.mArc[i].mDist - p.mArc[i - 1].mDist;
        if (ds <= 0.f)
        {
            continue;
        }
        const F32 dmid = 0.5f * (p.mArc[i].mDist + p.mArc[i - 1].mDist);
        dur += ds / llmax(pathSpeedAt(p, dmid, skip_ease), 0.05f);
    }
    // the walk only dwells at INTERIOR nodes (1 .. n-2); match that here
    const S32 n = (S32)p.mNodes.size();
    for (S32 i = 1; i <= n - 2; ++i)
    {
        dur += llmax(0.f, p.mNodes[i].mDwell);
    }
    out_duration = dur;
    return true;
}

// ---- undo/redo: authored-state snapshots ----------------------------------
LLActorMover::PathState LLActorMover::captureState(const LLUUID& key) const
{
    PathState st;
    auto it = mPaths.find(key);
    if (it != mPaths.end())
    {
        const Path& p = it->second;
        st.mNodes             = p.mNodes;
        st.mSpeed             = p.mSpeed;
        st.mEndMode           = p.mEndMode;
        st.mTension           = p.mTension;
        st.mEaseIn            = p.mEaseIn;
        st.mEaseOut           = p.mEaseOut;
        st.mArrivalFacingMode = p.mArrivalFacingMode;
        st.mArrivalDir        = p.mArrivalDir;
        st.mArrivalTarget     = p.mArrivalTarget;
        st.mGroundFollow      = p.mGroundFollow;
        st.mPitchToSlope      = p.mPitchToSlope;
        st.mSyncToTake        = p.mSyncToTake;
        st.mSyncLeadTrail     = p.mSyncLeadTrail;
    }
    return st;
}

void LLActorMover::applyState(const LLUUID& key, const PathState& st)
{
    if (st.mNodes.empty())
    {
        mPaths.erase(key);      // undo of the first placement drops the path
    }
    else
    {
        Path& p = mPaths[key];
        p.mNodes             = st.mNodes;
        p.mSpeed             = st.mSpeed;
        p.mEndMode           = st.mEndMode;
        p.mTension           = st.mTension;
        p.mEaseIn            = st.mEaseIn;
        p.mEaseOut           = st.mEaseOut;
        p.mArrivalFacingMode = st.mArrivalFacingMode;
        p.mArrivalDir        = st.mArrivalDir;
        p.mArrivalTarget     = st.mArrivalTarget;
        p.mGroundFollow      = st.mGroundFollow;
        p.mPitchToSlope      = st.mPitchToSlope;
        p.mSyncToTake        = st.mSyncToTake;
        p.mSyncLeadTrail     = st.mSyncLeadTrail;
        p.markDirty();
        p.rebuild();
    }
    // keep the shared edit-node selection inside the restored range
    if (mEditActor == key)
    {
        const S32 cnt = (S32)st.mNodes.size();
        if (mEditNode >= cnt)
        {
            mEditNode = cnt > 0 ? cnt - 1 : -1;
        }
    }
}

void LLActorMover::snapshotForUndo(const LLUUID& actor_id)
{
    const LLUUID key = path_key(actor_id);
    EditHistory& h = mHistory[key];
    h.mUndo.push_back(captureState(key));
    if ((S32)h.mUndo.size() > UNDO_DEPTH)
    {
        h.mUndo.erase(h.mUndo.begin());     // bounded depth: drop the oldest
    }
    h.mRedo.clear();                        // a fresh edit invalidates the redo branch
}

bool LLActorMover::canUndoPath(const LLUUID& actor_id) const
{
    auto it = mHistory.find(path_key(actor_id));
    return it != mHistory.end() && !it->second.mUndo.empty();
}

bool LLActorMover::canRedoPath(const LLUUID& actor_id) const
{
    auto it = mHistory.find(path_key(actor_id));
    return it != mHistory.end() && !it->second.mRedo.empty();
}

bool LLActorMover::undoPath(const LLUUID& actor_id)
{
    if (isPathWalking(actor_id))
    {
        return false;       // never swap geometry under a running arc clock
    }
    const LLUUID key = path_key(actor_id);
    auto hit = mHistory.find(key);
    if (hit == mHistory.end() || hit->second.mUndo.empty())
    {
        return false;
    }
    hit->second.mRedo.push_back(captureState(key));     // current -> redo
    PathState st = hit->second.mUndo.back();
    hit->second.mUndo.pop_back();
    applyState(key, st);
    return true;
}

bool LLActorMover::redoPath(const LLUUID& actor_id)
{
    if (isPathWalking(actor_id))
    {
        return false;
    }
    const LLUUID key = path_key(actor_id);
    auto hit = mHistory.find(key);
    if (hit == mHistory.end() || hit->second.mRedo.empty())
    {
        return false;
    }
    hit->second.mUndo.push_back(captureState(key));     // current -> undo
    PathState st = hit->second.mRedo.back();
    hit->second.mRedo.pop_back();
    applyState(key, st);
    return true;
}

// ---- structural ops --------------------------------------------------------
bool LLActorMover::reversePath(const LLUUID& actor_id)
{
    if (isPathWalking(actor_id))
    {
        return false;
    }
    const Path* cp = getPath(actor_id);
    if (!cp || cp->mNodes.size() < 2)
    {
        return false;
    }
    Path& p = editPath(actor_id);
    std::reverse(p.mNodes.begin(), p.mNodes.end());     // per-node data rides each node
    p.markDirty();
    p.rebuild();
    return true;
}

bool LLActorMover::mirrorPath(const LLUUID& actor_id)
{
    if (isPathWalking(actor_id))
    {
        return false;
    }
    const Path* cp = getPath(actor_id);
    if (!cp || cp->mNodes.size() < 2)
    {
        return false;
    }
    Path& p = editPath(actor_id);
    const S32 n = (S32)p.mNodes.size();

    // centroid of the node positions
    LLVector3d c(0.0, 0.0, 0.0);
    for (const Waypoint& w : p.mNodes)
    {
        c += w.mPosGlobal;
    }
    c *= (1.0 / (F64)n);

    // dominant travel direction (start -> end), horizontal; world X if degenerate
    LLVector3d dir = p.mNodes[n - 1].mPosGlobal - p.mNodes[0].mPosGlobal;
    dir.mdV[VZ] = 0.0;
    if (dir.length() < 1e-3)
    {
        dir = LLVector3d(1.0, 0.0, 0.0);
    }
    else
    {
        dir.normalize();
    }
    // horizontal normal of the vertical mirror plane (perp to travel, XY)
    const LLVector3d nrm(-dir.mdV[VY], dir.mdV[VX], 0.0);
    const LLVector3 nrmf((F32)nrm.mdV[VX], (F32)nrm.mdV[VY], 0.f);

    // reflect a global point across the vertical plane {c, nrm} (Z unchanged)
    auto reflectPoint = [&](const LLVector3d& pt) -> LLVector3d
    {
        const LLVector3d d = pt - c;
        const F64 dn = d.mdV[VX] * nrm.mdV[VX] + d.mdV[VY] * nrm.mdV[VY];
        return LLVector3d(pt.mdV[VX] - 2.0 * dn * nrm.mdV[VX],
                          pt.mdV[VY] - 2.0 * dn * nrm.mdV[VY],
                          pt.mdV[VZ]);
    };
    // reflect a world direction across the same plane (Z component untouched)
    auto reflectDir = [&](const LLVector3& v) -> LLVector3
    {
        const F32 dn = v.mV[VX] * nrmf.mV[VX] + v.mV[VY] * nrmf.mV[VY];
        return LLVector3(v.mV[VX] - 2.f * dn * nrmf.mV[VX],
                         v.mV[VY] - 2.f * dn * nrmf.mV[VY],
                         v.mV[VZ]);
    };

    for (Waypoint& w : p.mNodes)
    {
        w.mPosGlobal = reflectPoint(w.mPosGlobal);
        if (w.mHasCam)
        {
            w.mCamPosGlobal = reflectPoint(w.mCamPosGlobal);
            // mirror the camera basis: reflect forward + up, then rebuild a valid
            // right-handed frame (SL local axes: +X forward, +Y left, +Z up), so
            // the shot looks at the mirrored subject with roll/dutch preserved.
            LLVector3 fwd = reflectDir(LLVector3(1.f, 0.f, 0.f) * w.mCamRot);
            LLVector3 up  = reflectDir(LLVector3(0.f, 0.f, 1.f) * w.mCamRot);
            fwd.normalize();
            up = up - fwd * (up * fwd);     // re-orthogonalize up against forward
            up.normalize();
            LLVector3 left = up % fwd;       // Y = Z x X (right-handed)
            left.normalize();
            w.mCamRot = LLQuaternion(fwd, left, up);
        }
    }
    p.markDirty();
    p.rebuild();
    return true;
}

bool LLActorMover::loopClosePath(const LLUUID& actor_id)
{
    if (isPathWalking(actor_id))
    {
        return false;
    }
    const Path* cp = getPath(actor_id);
    if (!cp || cp->mNodes.size() < 2)
    {
        return false;
    }
    Path& p = editPath(actor_id);
    // snap the last node onto the first (position + standing height + nudge) so
    // the loop seam is a single shared point, and set the end mode to loop
    const Waypoint& first = p.mNodes.front();
    Waypoint& last = p.mNodes.back();
    last.mPosGlobal    = first.mPosGlobal;
    last.mRootAbove    = first.mRootAbove;
    last.mGroundOffset = first.mGroundOffset;
    p.mEndMode           = 1;   // loop
    p.mArrivalFacingMode = 0;   // arrival facing is meaningless for a loop
    p.markDirty();
    p.rebuild();
    return true;
}

bool LLActorMover::copyPathTo(const LLUUID& src_actor, const LLUUID& dst_actor)
{
    const LLUUID src_key = path_key(src_actor);
    const LLUUID dst_key = path_key(dst_actor);
    if (src_key == dst_key)
    {
        return false;       // no self-copy
    }
    auto sit = mPaths.find(src_key);
    if (sit == mPaths.end() || sit->second.mNodes.size() < 2)
    {
        return false;
    }
    if (isPathWalking(dst_actor))
    {
        return false;       // don't clobber a walk in progress on the destination
    }
    // deep-copy the authored path onto the destination slot; global coords are
    // unchanged so it overlays the source (per-actor overlay color keeps them
    // distinct in world). std::map insert never invalidates other nodes, so the
    // source reference stays valid across the destination insertion.
    Path& dst = mPaths[dst_key];
    const Path& src = sit->second;
    dst.mNodes             = src.mNodes;
    dst.mSpeed             = src.mSpeed;
    dst.mEndMode           = src.mEndMode;
    dst.mTension           = src.mTension;
    dst.mEaseIn            = src.mEaseIn;
    dst.mEaseOut           = src.mEaseOut;
    dst.mArrivalFacingMode = src.mArrivalFacingMode;
    dst.mArrivalDir        = src.mArrivalDir;
    dst.mArrivalTarget     = src.mArrivalTarget;
    dst.mGroundFollow      = src.mGroundFollow;
    dst.mPitchToSlope      = src.mPitchToSlope;
    dst.mSyncToTake        = src.mSyncToTake;
    dst.mSyncLeadTrail     = src.mSyncLeadTrail;
    dst.markDirty();
    dst.rebuild();
    return true;
}

bool LLActorMover::startWalkTo(const LLUUID& actor_id, const LLVector3d& ground_global)
{
    LLVOAvatar* av = resolve_actor(actor_id);
    if (!av || !av->getRootJoint())
    {
        return false;
    }
    snapshotForUndo(actor_id);
    // fresh 2-node straight path: node 0 at the actor's current rendered foot
    // (with its authored standing height), node 1 at the clicked ground point
    clearPath(actor_id);
    appendWaypointHere(actor_id);
    appendWaypointAt(actor_id, ground_global);
    if (!hasWalkablePath(actor_id))
    {
        return false;
    }
    start(actor_id);        // walks the path (rebuilds the arc table if dirty)
    return true;
}

// ---------------------------------------------------------------------------
// P3 per-node camera: capture / clear / read. Camera data is world-anchored
// (global coords) and does NOT touch the arc-length geometry, so these never
// mark the path dirty (no needless spline rebuild).
// ---------------------------------------------------------------------------
bool LLActorMover::setNodeCamera(const LLUUID& actor_id, S32 index,
                                 const LLVector3d& cam_pos_global,
                                 const LLQuaternion& cam_rot, F32 cam_fov, S32 transition)
{
    const Path* cp = getPath(actor_id);
    if (!cp || index < 0 || index >= (S32)cp->mNodes.size())
    {
        return false;
    }
    Path& path = editPath(actor_id);
    Waypoint& w = path.mNodes[index];
    w.mHasCam       = true;
    w.mCamPosGlobal = cam_pos_global;
    w.mCamRot       = cam_rot;
    w.mCamFov       = cam_fov;
    w.mCamTransition = llclamp(transition, 0, 1);
    return true;
}

bool LLActorMover::clearNodeCamera(const LLUUID& actor_id, S32 index)
{
    const Path* cp = getPath(actor_id);
    if (!cp || index < 0 || index >= (S32)cp->mNodes.size())
    {
        return false;
    }
    Path& path = editPath(actor_id);
    Waypoint& w = path.mNodes[index];
    w.mHasCam = false;
    w.mCamPosGlobal.setZero();
    w.mCamRot.loadIdentity();
    w.mCamFov = 0.f;
    // keep mCamTransition as the node's remembered cut/ease preference
    return true;
}

bool LLActorMover::setNodeCamTransition(const LLUUID& actor_id, S32 index, S32 transition)
{
    const Path* cp = getPath(actor_id);
    if (!cp || index < 0 || index >= (S32)cp->mNodes.size())
    {
        return false;
    }
    editPath(actor_id).mNodes[index].mCamTransition = llclamp(transition, 0, 1);
    return true;
}

bool LLActorMover::getNodeCamera(const LLUUID& actor_id, S32 index,
                                 LLVector3d& cam_pos_global, LLQuaternion& cam_rot,
                                 F32& cam_fov, S32& transition) const
{
    const Path* cp = getPath(actor_id);
    if (!cp || index < 0 || index >= (S32)cp->mNodes.size())
    {
        return false;
    }
    const Waypoint& w = cp->mNodes[index];
    transition = w.mCamTransition;      // valid even with no camera (remembered pref)
    if (!w.mHasCam)
    {
        return false;
    }
    cam_pos_global = w.mCamPosGlobal;
    cam_rot        = w.mCamRot;
    cam_fov        = w.mCamFov;
    return true;
}

S32 LLActorMover::pathCameraNodeCount(const LLUUID& actor_id) const
{
    const Path* cp = getPath(actor_id);
    if (!cp)
    {
        return 0;
    }
    S32 count = 0;
    for (const Waypoint& w : cp->mNodes)
    {
        if (w.mHasCam)
        {
            ++count;
        }
    }
    return count;
}

// ---------------------------------------------------------------------------
// P3 path-camera SOURCE. hasActivePathCamera() is the ownership gate; it must
// go false the same frame the walk ends so the render camera is released.
// ---------------------------------------------------------------------------
bool LLActorMover::hasActivePathCamera(const LLUUID& actor_id) const
{
    const LLUUID key = path_key(actor_id);
    auto mit = mMoves.find(key);
    if (mit == mMoves.end() || !mit->second.mIsPath)
    {
        return false;                   // not walking a path -> release
    }
    if (mit->second.mSuspended)
    {
        return false;                   // a suspended walk owns no camera (composes
                                        // with the LLPathCamera Subject-A release)
    }
    // a stop-mode walk that has settled at the final node is FINISHED; hand the
    // camera back rather than latching on the parked actor. Loop / ping-pong
    // never "arrive", so they keep the camera until Stop / CUT removes the Move.
    if (mit->second.mArrived && mit->second.mEndMode == 0)
    {
        return false;
    }
    auto pit = mPaths.find(key);
    if (pit == mPaths.end())
    {
        return false;
    }
    for (const Waypoint& w : pit->second.mNodes)
    {
        if (w.mHasCam)
        {
            return true;
        }
    }
    return false;                       // no camera nodes -> nothing to drive
}

bool LLActorMover::getPathCameraPose(const LLUUID& actor_id, LLVector3d& out_pos,
                                     LLQuaternion& out_rot, F32& out_fov) const
{
    const LLUUID key = path_key(actor_id);
    auto mit = mMoves.find(key);
    if (mit == mMoves.end() || !mit->second.mIsPath || mit->second.mSuspended)
    {
        return false;
    }
    auto pit = mPaths.find(key);
    if (pit == mPaths.end())
    {
        return false;
    }
    const Path& path = pit->second;
    const S32   n     = (S32)path.mNodes.size();
    // Need the arc-length table to map node index -> arc distance. It is rebuilt
    // in start()/advancePath(); a structural mismatch (edit not yet recompiled)
    // is rare and safe to skip for a frame rather than read a stale index.
    if (n < 1 || (S32)path.mNodeDist.size() < n)
    {
        return false;
    }

    // camera nodes in path order (mNodeDist is monotonic, so this stays sorted
    // by arc distance)
    struct CamRef { F32 dist; S32 idx; };
    std::vector<CamRef> cams;
    cams.reserve(n);
    for (S32 i = 0; i < n; ++i)
    {
        if (path.mNodes[i].mHasCam)
        {
            cams.push_back({ path.mNodeDist[i], i });
        }
    }
    if (cams.empty())
    {
        return false;
    }

    struct Pose { LLVector3d p; LLQuaternion r; F32 f; };
    auto poseOf = [&](S32 ci) -> Pose
    {
        const Waypoint& w = path.mNodes[cams[ci].idx];
        return { w.mCamPosGlobal, w.mCamRot, w.mCamFov };
    };
    // smoothstep-eased blend between two camera poses: slerp rotation (short
    // way -- hemisphere-aligned first), lerp position + FOV. u is raw 0..1.
    auto easeBetween = [](const Pose& a, const Pose& b, F32 u) -> Pose
    {
        const F32 s = u * u * (3.f - 2.f * u);
        Pose o;
        o.p = a.p * (1.0 - (F64)s) + b.p * (F64)s;
        const F32 dot = a.r.mQ[0] * b.r.mQ[0] + a.r.mQ[1] * b.r.mQ[1]
                      + a.r.mQ[2] * b.r.mQ[2] + a.r.mQ[3] * b.r.mQ[3];
        const LLQuaternion bb = (dot < 0.f) ? (-b.r) : b.r;
        o.r = slerp(s, a.r, bb);
        o.f = a.f * (1.f - s) + b.f * s;
        return o;
    };

    const S32 last = (S32)cams.size() - 1;
    Pose result;

    if (last == 0)
    {
        result = poseOf(0);             // single camera node: hold it
    }
    else
    {
        // interior bracket: transition INTO cams[k+1] governs the segment. CUT
        // holds cams[k] until the actor crosses cams[k+1]; EASE interpolates.
        auto interior = [&](F32 dd) -> Pose
        {
            S32 k = 0;
            while (k + 1 <= last && cams[k + 1].dist <= dd)
            {
                ++k;
            }
            k = llclamp(k, 0, last - 1);
            const S32 into = cams[k + 1].idx;
            if (path.mNodes[into].mCamTransition == 0)      // CUT: hold previous
            {
                return poseOf(k);
            }
            const F32 span = cams[k + 1].dist - cams[k].dist;
            const F32 u = (span > 1e-4f)
                              ? llclamp((dd - cams[k].dist) / span, 0.f, 1.f) : 1.f;
            return easeBetween(poseOf(k), poseOf(k + 1), u);
        };

        const F32 d = mit->second.mDist;

        if (path.mEndMode == 1)         // loop: the camera track wraps closed
        {
            const F32 total = llmax(path.mTotalLength, 1e-4f);
            if (d >= cams[last].dist || d < cams[0].dist)
            {
                // wrap segment: last camera -> first camera (through the seam).
                // Transition into the FIRST node governs it.
                if (path.mNodes[cams[0].idx].mCamTransition == 0)
                {
                    result = poseOf(last);
                }
                else
                {
                    const F32 dd = (d >= cams[last].dist) ? d : d + total;
                    const F32 a  = cams[last].dist;
                    const F32 b  = cams[0].dist + total;
                    const F32 span = b - a;
                    const F32 u = (span > 1e-4f)
                                      ? llclamp((dd - a) / span, 0.f, 1.f) : 1.f;
                    result = easeBetween(poseOf(last), poseOf(0), u);
                }
            }
            else
            {
                result = interior(d);
            }
        }
        else                            // stop / ping-pong: hold ends
        {
            if (d <= cams[0].dist)         { result = poseOf(0); }
            else if (d >= cams[last].dist) { result = poseOf(last); }
            else                           { result = interior(d); }
        }
    }

    out_pos = result.p;
    out_rot = result.r;
    // guard a degenerate / unset FOV so we never write a zero vertical FOV
    out_fov = (result.f > 0.01f) ? result.f
                                 : LLViewerCamera::getInstance()->getDefaultFOV();
    return true;
}

// ---------------------------------------------------------------------------
// P2 edit selection (shared spine for the panel, the in-world tool and the viz)
// ---------------------------------------------------------------------------
void LLActorMover::setEditActor(const LLUUID& actor_id)
{
    // A concrete avatar id IS the path key (path_key is identity for non-null),
    // so the viz -- which compares against a live av->getID() -- and the editing
    // ops agree on one identity. Null does NOT normalize to self here: "nothing
    // selected" must clear editing, not silently target your own avatar's path.
    if (actor_id != mEditActor)
    {
        mEditActor = actor_id;
        mEditNode = -1;     // a fresh actor starts with nothing selected
    }
}

// ---------------------------------------------------------------------------
// Stable per-actor overlay color: fold the 16 id bytes to a hue, then a vivid
// HSL. Deterministic, so an actor's path keeps the same color all session and
// multiple actors' paths stay distinguishable at a glance.
// ---------------------------------------------------------------------------
//static
LLColor4 LLActorMover::actorPathColor(const LLUUID& actor_id)
{
    U32 h = 2166136261u;                // FNV-1a fold
    for (S32 i = 0; i < 16; ++i)
    {
        h = (h ^ actor_id.mData[i]) * 16777619u;
    }
    const F32 hue = (F32)(h % 1000u) / 1000.f;
    LLColor4 c;
    c.setHSL(hue, 0.72f, 0.58f);        // vivid but not blown out
    c.mV[VW] = 1.f;
    return c;
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

    // [Follow-the-leader] a follower rides the leader's path by reference. It
    // starts a path Move regardless of its own (ignored) nodes, so advancePath's
    // follower branch drives it; the actual position/facing come from the leader
    // each frame. Takes precedence over the actor's own path when both exist.
    if (isFollowing(av->getID()))
    {
        auto old_it = mMoves.find(av->getID());
        if (old_it != mMoves.end() && old_it->second.mAnim.notNull())
        {
            av->stopMotion(old_it->second.mAnim);
        }

        Move mv;
        mv.mIsPath   = true;
        mv.mSpeed    = 1.f;                 // filled from the leader path each frame
        mv.mDistance = 0.f;                 // "
        mv.mDist     = 0.f;
        mv.mDir      = 1.f;
        mv.mNominal  = llmax((F32)nominal, 0.5f);
        mv.mAnim     = locomotion_anim(av->getID());
        mv.mFaceInit = false;
        mMoves[av->getID()] = mv;

        av->startMotion(mv.mAnim);          // cadence is retimed per frame by ground speed
        LL_INFOS("ActorMover") << "ghost follow walk: " << av->getID() << LL_ENDL;
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

    Move& mv = it->second;

    // A suspended walk (actor derezzed / left the region / teleported) paints
    // NOTHING: freezing the override here is exactly what stops a stale pose
    // being left at old-region coords. The idle-loop state machine
    // (updateSuspendState) owns the suspend/resume lifecycle, so a dead actor is
    // no longer ERASED here -- the Move is kept for a smart resume.
    if (mv.mSuspended)
    {
        return false;
    }
    if (av->isDead() || !av->getRootJoint())
    {
        // Not yet suspended (still inside the debounce window, or the first dead
        // frame): never paint a dead actor and never advance the clock, but keep
        // the Move so updateSuspendState() can suspend rather than destroy it.
        return false;
    }

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

// ===========================================================================
// TP-away suspend / resume: keep an in-progress walk alive across a derez /
// region change / teleport instead of destroying it. See the header contract.
// ===========================================================================
namespace
{
// how long an actor must stay unresolvable before we suspend, in FRAMES: a
// short debounce so a 1-frame resolve hiccup never suspends a live walk.
const S32 SUSPEND_DEBOUNCE_FRAMES = 6;
// a single-frame sim-true global jump this large is a teleport / region change,
// never a walk: an actor's sim-true position is otherwise frozen during a ghost
// walk, and global coords stay continuous across region borders. Far below one
// region (256 m), far above any real per-frame avatar motion.
const F64 TP_JUMP_METERS = 40.0;
// "came back to the same place": resolvable again within this distance of the
// suspend anchor -> auto-resume seamlessly. Above a walk step, below "wandered
// off / different sim" (which stays suspended for a manual Resume / Re-anchor).
const F64 RESUME_NEAR_METERS = 10.0;
} // anonymous namespace

// ---------------------------------------------------------------------------
void LLActorMover::enterSuspend(const LLUUID& key, Move& mv, LLVOAvatar* av)
{
    if (mv.mSuspended)
    {
        return;
    }
    mv.mSuspended = true;
    // the return test compares the actor's future sim-true position to where it
    // was when it went away: the last frozen sim-true position (the real spot
    // the walk was anchored at), or -- lacking one -- the live position.
    mv.mSuspendTrueGlobal = mv.mHasTrueGlobal
                                ? mv.mLastTrueGlobal
                                : (av ? av->getPositionGlobal() : LLVector3d());
    // a still-present actor (e.g. the user's own avatar on teleport) must not be
    // left standing there walking in place: stop the loco / dwell anim. The walk
    // clock is already frozen because applyOverride() bails while suspended.
    if (av && !av->isDead())
    {
        if (mv.mAnim.notNull() && av->findMotion(mv.mAnim))
        {
            av->stopMotion(mv.mAnim);
        }
        if (mv.mDwellAnim.notNull() && av->findMotion(mv.mDwellAnim))
        {
            av->stopMotion(mv.mDwellAnim);
        }
        av->setAnimTimeFactor(1.f);
    }
    LL_INFOS("ActorMover") << "walk SUSPENDED for " << key
                           << " at dist " << mv.mDist << LL_ENDL;
}

void LLActorMover::resumeMove(const LLUUID& key, Move& mv, LLVOAvatar* av)
{
    mv.mSuspended = false;
    mv.mUnresolvedFrames = 0;
    if (av && !av->isDead() && av->getRootJoint())
    {
        // reseed the jump probe from the current sim-true position so the
        // resumed walk does not instantly re-detect the (old) teleport delta
        mv.mLastTrueGlobal = av->getPositionGlobal();
        mv.mHasTrueGlobal = true;
        // restart the loco cadence unless the walk had already arrived (a
        // finished stop-mode walk resumes as a settled stand, facing still eases)
        if (!mv.mArrived && mv.mAnim.notNull())
        {
            av->startMotion(mv.mAnim);
            av->setAnimTimeFactor(llclamp(mv.mSpeed, 0.05f, 10.f)
                                  / llmax(mv.mNominal, 0.5f));
        }
        // ease facing from the actor's current yaw on the next advance instead
        // of snapping to the stored travel facing
        mv.mFaceInit = false;
    }
    else
    {
        mv.mHasTrueGlobal = false;
    }
    LL_INFOS("ActorMover") << "walk RESUMED for " << key
                           << " at dist " << mv.mDist << LL_ENDL;
}

// ---------------------------------------------------------------------------
// Per-frame state machine (idle loop, before the character update). Each move
// runs its own WALKING <-> SUSPENDED transition independently.
// ---------------------------------------------------------------------------
void LLActorMover::updateSuspendState()
{
    if (mMoves.empty())
    {
        return;
    }
    for (auto& pair : mMoves)
    {
        const LLUUID& key = pair.first;
        Move&         mv  = pair.second;
        LLVOAvatar*   av  = resolve_actor(key);     // null = unresolvable (dead/gone)

        if (!mv.mSuspended)
        {
            if (!av || !av->getRootJoint())
            {
                // actor gone: debounce before suspending so a 1-frame resolve
                // hiccup does not suspend a live walk
                if (++mv.mUnresolvedFrames >= SUSPEND_DEBOUNCE_FRAMES)
                {
                    enterSuspend(key, mv, av);
                }
            }
            else
            {
                mv.mUnresolvedFrames = 0;
                // teleport / region-jump on the sim-true position (unaffected by
                // the ghost root override, and frozen during a walk)
                const LLVector3d cur = av->getPositionGlobal();
                if (mv.mHasTrueGlobal &&
                    (cur - mv.mLastTrueGlobal).length() > TP_JUMP_METERS)
                {
                    enterSuspend(key, mv, av);      // keeps mLastTrueGlobal (pre-jump)
                }
                else
                {
                    mv.mLastTrueGlobal = cur;
                    mv.mHasTrueGlobal = true;
                }
            }
        }
        else if (av && av->getRootJoint())
        {
            // SUSPENDED and the actor is resolvable again: auto-resume the
            // instant it is back NEAR where the walk left off. Farther away
            // (wandered off / different sim) it stays suspended so the walk is
            // never yanked to old coords -- the Path tab offers Resume / Re-
            // anchor / Cancel.
            if ((av->getPositionGlobal() - mv.mSuspendTrueGlobal).length()
                    <= RESUME_NEAR_METERS)
            {
                resumeMove(key, mv, av);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Path-tab UI queries + actions
// ---------------------------------------------------------------------------
bool LLActorMover::isWalkSuspended(const LLUUID& actor_id) const
{
    auto it = mMoves.find(path_key(actor_id));
    return it != mMoves.end() && it->second.mSuspended;
}

bool LLActorMover::suspendedActorResolvable(const LLUUID& actor_id) const
{
    const LLUUID key = path_key(actor_id);
    auto it = mMoves.find(key);
    if (it == mMoves.end() || !it->second.mSuspended)
    {
        return false;
    }
    LLVOAvatar* av = resolve_actor(key);
    return av && av->getRootJoint();
}

bool LLActorMover::suspendedActorNearAnchor(const LLUUID& actor_id) const
{
    const LLUUID key = path_key(actor_id);
    auto it = mMoves.find(key);
    if (it == mMoves.end() || !it->second.mSuspended)
    {
        return false;
    }
    LLVOAvatar* av = resolve_actor(key);
    if (!av || !av->getRootJoint())
    {
        return false;
    }
    return (av->getPositionGlobal() - it->second.mSuspendTrueGlobal).length()
               <= RESUME_NEAR_METERS;
}

bool LLActorMover::resumeWalk(const LLUUID& actor_id)
{
    const LLUUID key = path_key(actor_id);
    auto it = mMoves.find(key);
    if (it == mMoves.end() || !it->second.mSuspended)
    {
        return false;
    }
    LLVOAvatar* av = resolve_actor(key);
    if (!av || !av->getRootJoint())
    {
        return false;               // cannot resume an actor that is not here
    }
    resumeMove(key, it->second, av);
    return true;
}

bool LLActorMover::reanchorWalk(const LLUUID& actor_id)
{
    const LLUUID key = path_key(actor_id);
    auto it = mMoves.find(key);
    if (it == mMoves.end() || !it->second.mSuspended || !it->second.mIsPath)
    {
        return false;               // re-anchor only makes sense for a path walk
    }
    LLVOAvatar* av = resolve_actor(key);
    if (!av || !av->getRootJoint())
    {
        return false;
    }
    auto pit = mPaths.find(key);
    if (pit == mPaths.end() || pit->second.mNodes.size() < 2)
    {
        return false;
    }
    Path& path = pit->second;
    if (path.mDirty)
    {
        path.rebuild();
    }
    // where the walk currently sits on the (old) path, in global foot coords...
    LLVector3d cur_pos, cur_tan;
    path.evalAtDistance(it->second.mDist, cur_pos, cur_tan);
    // ...and the actor's current foot position now (rendered root minus the
    // pelvis-to-foot, matching the node capture convention). Suspended, the root
    // is NOT overridden, so it reflects the actor's real standing pose.
    LLVector3  root_agent  = av->getRootJoint()->getWorldPosition();
    LLVector3d foot_global = gAgent.getPosGlobalFromAgent(root_agent);
    foot_global.mdV[VZ] -= av->getPelvisToFoot();
    // translate the WHOLE path (and every node camera) so the current arc
    // position lands on the actor -> the walk continues from here on the new sim
    const LLVector3d delta = foot_global - cur_pos;
    for (Waypoint& w : path.mNodes)
    {
        w.mPosGlobal += delta;
        if (w.mHasCam)
        {
            w.mCamPosGlobal += delta;
        }
    }
    path.markDirty();
    path.rebuild();
    resumeMove(key, it->second, av);
    return true;
}

void LLActorMover::cancelSuspended(const LLUUID& actor_id)
{
    const LLUUID key = path_key(actor_id);
    auto it = mMoves.find(key);
    if (it == mMoves.end() || !it->second.mSuspended)
    {
        return;
    }
    // drop the walk back to idle. enterSuspend() already stopped the loco anim
    // on a resolvable actor, but clear any anim that might still be resolving so
    // a cancel never leaves a looping walk cycle behind.
    if (LLVOAvatar* av = resolve_actor(key))
    {
        if (it->second.mAnim.notNull() && av->findMotion(it->second.mAnim))
        {
            av->stopMotion(it->second.mAnim);
        }
        if (it->second.mDwellAnim.notNull() && av->findMotion(it->second.mDwellAnim))
        {
            av->stopMotion(it->second.mDwellAnim);
        }
        av->setAnimTimeFactor(1.f);
    }
    mMoves.erase(it);
}

// ---------------------------------------------------------------------------
// Path traversal: one frame of advancing along the Catmull-Rom curve. Called
// once per frame from applyOverride() after the path clock has ticked. Sets
// mv.mCurPos (agent frame) + mv.mCurRot (turn-rate-smoothed facing).
// ---------------------------------------------------------------------------
void LLActorMover::advancePath(LLVOAvatar* av, Move& mv, F32 dt)
{
    // [Follow-the-leader] a follower rides the LEADER's spline by reference; hand
    // off here so it never touches its own (ignored) path.
    auto fit = mFollows.find(av->getID());
    if (fit != mFollows.end())
    {
        advanceFollower(av, mv, fit->second, dt);
        return;
    }

    Path& path = mPaths[av->getID()];       // start() guaranteed this exists
    if (path.mDirty)
    {
        path.rebuild();
    }
    const S32 n = (S32)path.mNodes.size();
    if (n < 2 || path.mTotalLength <= 0.001f)
    {
        // degenerate (all nodes coincident): hold at node 0 so we never place
        // the root at the origin. Lift by the AUTHORED root-above-ground, not the
        // live pelvisToFoot, to match the placement convention below.
        LLVector3 a = gAgent.getPosAgentFromGlobal(path.mNodes.empty()
                          ? LLVector3d() : path.mNodes[0].mPosGlobal);
        a.mV[VZ] += path.mNodes.empty() ? 0.f : path.mNodes[0].mRootAbove;
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

    // ---- Sync-to-take? the recorder playhead becomes the master arc clock ----
    // A sync-flagged path with a loaded take derives its arc position directly
    // from the playhead each frame (no dt integration, no dwell/ease/loop wrap):
    // PLAY or SCRUB the recorder and the actor moves in lockstep. An empty take /
    // zero duration falls through to the normal walk below (byte-identical), so
    // the flag is safe when no take is loaded. A single-frame arc jump larger than
    // this is a scrub SEEK -> snap facing rather than turn-rate-lag toward it.
    const F32 SYNC_SEEK_SNAP_M = 1.0f;
    bool sync_active = false;
    bool sync_seek   = false;
    if (path.mSyncToTake)
    {
        const LLFlycamRecorder& rec = LLFlycamRecorder::instance();
        if (rec.getNumKeyframes() > 0 && rec.getDuration() > 0.001f)
        {
            sync_active = true;
        }
    }

    // ---- dwell: hold at a node, walk clock frozen (not while sync-driven) ----
    if (!sync_active && mv.mDwellNode >= 0)
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

    // ---- advance arc-length distance ----
    F32       speed;
    const F32 old_d = mv.mDist;
    F32       nd;
    if (sync_active)
    {
        // playhead -> normalized (with lead/trail) -> arc distance. Scrub the take
        // and the actor scrubs with it; play it and the actor plays in lockstep.
        const LLFlycamRecorder& rec = LLFlycamRecorder::instance();
        const F32 dur = rec.getDuration();                  // > 0 (guarded above)
        F32 u = (rec.getPlayhead() + path.mSyncLeadTrail) / dur;
        u = llclamp(u, 0.f, 1.f);
        nd = u * path.mTotalLength;
        // cadence tracks the ground speed IMPLIED by the playhead motion (dArc/dt),
        // so feet stay planted at any play/scrub rate; a paused playhead -> 0 speed
        // -> a frozen stride. A large one-frame jump is a scrub seek (snap facing).
        const F32 d_arc = nd - old_d;
        speed     = (dt > 1e-4f) ? fabsf(d_arc) / dt : 0.f;
        sync_seek = fabsf(d_arc) > SYNC_SEEK_SNAP_M;
        mv.mDir     = 1.f;      // always face forward along the path
        mv.mArrived = false;    // a synced actor never settles; a scrub can move it
    }
    else
    {
        speed = pathSpeedAt(path, mv.mDist, loop || pingpong);
        nd    = mv.mDist + mv.mDir * speed * dt;

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
    }

    mv.mDist = nd;

    // ---- evaluate the spline (global) ----
    LLVector3d pos_global, tan_global;
    path.evalAtDistance(mv.mDist, pos_global, tan_global);
    if (mv.mDir < 0.f)
    {
        tan_global = -tan_global;   // ping-pong return leg faces backward
    }

    // ---- agent frame + ground placement --------------------------------------
    // pos_global.Z is the AUTHORED ground/foot level the node was snapped to.
    // Place the root at a STABLE authored standing height (mRootAbove) above the
    // ground -- the same convention as the legacy straight move, which captures
    // its root once and never rebuilds it from the live, pose-dependent
    // getPelvisToFoot(). On flat ground this reproduces the exact captured root
    // (feet on the ground, byte-matching the legacy move); ground-follow swaps
    // the authored ground for the live ground so the root rides slopes/stairs.
    LLVector3 agent = gAgent.getPosAgentFromGlobal(pos_global);
    const F32 spline_ground_z = agent.mV[VZ];
    F32 ground_z = spline_ground_z;
    bool ground_hit = false;
    if (path.mGroundFollow)
    {
        ground_z = resolveGroundZ(av, agent, agent.mV[VZ], ground_hit);
    }
    ground_z += pathGroundOffsetAt(path, mv.mDist);
    const F32 root_above = pathRootAboveAt(path, mv.mDist);
    agent.mV[VZ] = ground_z + root_above;
    mv.mCurPos = agent;

    static LLCachedControl<bool> zdbg(gSavedSettings, "ActorMoverPathZDebug", false);
    if (zdbg && mv.mDbgFrames < 5)
    {
        ++mv.mDbgFrames;
        LL_INFOS("ActorPath") << "walk f" << mv.mDbgFrames
            << " d=" << mv.mDist
            << " splineGroundZ=" << spline_ground_z
            << " groundFollow=" << (path.mGroundFollow ? 1 : 0)
            << " resolvedGroundZ=" << ground_z << " hit=" << (ground_hit ? 1 : 0)
            << " rootAbove=" << root_above
            << " placedRootZ=" << agent.mV[VZ]
            << " liveRootZ=" << av->getRootJoint()->getWorldPosition().mV[VZ]
            << " livePelvisToFoot=" << av->getPelvisToFoot() << LL_ENDL;
    }

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
    // a scrub seek snaps facing (up to a half turn in one frame); normal playback
    // and free walks ease at the configured turn rate
    const F32 max_step = sync_seek ? F_PI
                                   : llmax(1.f, (F32)turn_rate) * DEG_TO_RAD * dt;
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
// Follow-the-leader traversal: ride the leader's spline at an offset arc. Robust
// by construction -- any moment the leader is not actively path-walking, or the
// leader path is degenerate, the follower HOLDS in place (frozen stride) rather
// than crashing or snapping. Distance offset is shipped; time offset is deferred
// (it holds until implemented). Facing follows the leader tangent; cadence locks
// to the follower's OWN instantaneous ground speed so its feet stay planted.
// ---------------------------------------------------------------------------
void LLActorMover::advanceFollower(LLVOAvatar* av, Move& mv, const Follow& f, F32 dt)
{
    // hold helper: paint the last good pose (seed from the live pose the first
    // time so we never place at the coordinate origin) with a frozen stride
    auto holdInPlace = [&]()
    {
        if (!mv.mFaceInit)
        {
            mv.mCurPos          = av->getRootJoint()->getWorldPosition();
            mv.mCurRot          = av->getRenderRotation();
            mv.mFollowRootAbove = llmax(0.f, av->getPelvisToFoot());
            mv.mFaceInit        = true;
        }
        if (mv.mAnim.notNull())
        {
            av->setAnimTimeFactor(0.f);     // planted stride while waiting
        }
    };

    // resolve the leader's authored path + live walk. The leader must have a
    // walkable path AND be actively path-walking (not merely a hold, not
    // suspended) for the follower to have something to ride. Time offset (mode 1)
    // is DEFERRED, so it holds until implemented.
    auto pit = mPaths.find(f.mLeader);
    auto lit = mMoves.find(f.mLeader);
    const bool leader_ok =
        f.mMode == 0 &&
        pit != mPaths.end() && pit->second.mNodes.size() >= 2 &&
        lit != mMoves.end() && lit->second.mIsPath && !lit->second.mSuspended;
    if (!leader_ok)
    {
        holdInPlace();
        return;
    }

    Path& lpath = pit->second;
    if (lpath.mDirty)
    {
        lpath.rebuild();
    }
    const F32 total = lpath.mTotalLength;
    if (total <= 0.001f)
    {
        holdInPlace();
        return;
    }
    mv.mDistance = total;                       // progress readout vs the leader path

    // ---- follower arc = leader arc minus the distance offset ----
    const F32 leaderArc = lit->second.mDist;
    F32 fArc;
    if (lpath.mEndMode == 1)        // loop: trail continuously around the ring
    {
        fArc = leaderArc - f.mOffset;
        fArc -= floorf(fArc / total) * total;   // positive modulo into [0, total)
    }
    else                            // stop / ping-pong: clamp, so the follower
    {                               // waits at the start until the leader has
        fArc = llclamp(leaderArc - f.mOffset, 0.f, total);  // travelled the offset
    }

    const F32 old_d   = mv.mDist;
    const F32 implied = (dt > 1e-4f) ? fabsf(fArc - old_d) / dt : 0.f;
    // a loop-seam wrap (arc jumps total->0) would spike the implied speed for one
    // frame; cap the cadence so the stride never flickers at the seam
    const F32 cad_speed = llmin(implied, 12.f);
    mv.mDist = fArc;

    // ---- evaluate the LEADER's spline at the follower's arc ----
    LLVector3d pos_global, tan_global;
    lpath.evalAtDistance(fArc, pos_global, tan_global);

    // seed facing + capture the follower's own standing height on the first frame
    if (!mv.mFaceInit)
    {
        LLVector3 at = LLVector3(1.f, 0.f, 0.f) * av->getRenderRotation();
        mv.mCurRot.setEulerAngles(0.f, 0.f, atan2f(at.mV[VY], at.mV[VX]));
        mv.mFollowRootAbove = llmax(0.f, av->getPelvisToFoot());
        mv.mFaceInit = true;
    }

    // ---- agent frame + ground placement (follower's OWN standing height) ----
    LLVector3 agent = gAgent.getPosAgentFromGlobal(pos_global);
    F32  ground_z   = agent.mV[VZ];
    bool ground_hit = false;
    if (lpath.mGroundFollow)
    {
        ground_z = resolveGroundZ(av, agent, agent.mV[VZ], ground_hit);
    }
    ground_z += pathGroundOffsetAt(lpath, fArc);
    agent.mV[VZ] = ground_z + mv.mFollowRootAbove;
    mv.mCurPos = agent;

    // ---- facing: leader tangent at the follower's position, turn-rate clamped ----
    const F32 target_yaw = atan2f((F32)tan_global.mdV[VY], (F32)tan_global.mdV[VX]);
    F32 target_pitch = 0.f;
    if (lpath.mPitchToSlope)
    {
        const F32 horiz = sqrtf((F32)(tan_global.mdV[VX] * tan_global.mdV[VX]
                                    + tan_global.mdV[VY] * tan_global.mdV[VY]));
        target_pitch = atan2f(-(F32)tan_global.mdV[VZ], llmax(horiz, 1e-4f));
    }
    static LLCachedControl<F32> turn_rate(gSavedSettings, "PathTurnRateDegPerSec", 180.f);
    LLVector3 cur_at = LLVector3(1.f, 0.f, 0.f) * mv.mCurRot;
    const F32 cur_yaw = atan2f(cur_at.mV[VY], cur_at.mV[VX]);
    F32 dyaw = target_yaw - cur_yaw;
    while (dyaw >  F_PI) { dyaw -= 2.f * F_PI; }
    while (dyaw < -F_PI) { dyaw += 2.f * F_PI; }
    const F32 max_step = llmax(1.f, (F32)turn_rate) * DEG_TO_RAD * dt;
    dyaw = llclamp(dyaw, -max_step, max_step);
    mv.mCurRot.setEulerAngles(0.f, target_pitch, cur_yaw + dyaw);

    // ---- cadence lock from the follower's own instantaneous ground speed ----
    if (mv.mAnim.notNull())
    {
        av->setAnimTimeFactor(cad_speed / llmax(mv.mNominal, 0.5f));
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

// ===========================================================================
// Path visualization helpers (Problem 2 + 3). All draw in the same beacon-style
// UI pass as renderHeadingPreview(): gUIProgram / LLGLSUIDefault / no depth
// writes, so they read as a client-side overlay over any ground. GL line width
// is unreliable across drivers, so "thickness" is faked with filled triangle
// ribbons; only thin accents (sticks, digits) use lines.
// ===========================================================================
namespace
{
// small lifts so overlay geometry sits just above the ground it describes
const F32 PATH_RIBBON_LIFT = 0.08f;
const F32 NODE_BASE_LIFT   = 0.03f;
const F32 NODE_STICK_H     = 0.75f;     // marker post height, m
const F32 NODE_NUM_H       = 0.26f;     // billboarded digit height, m

// filled ground ribbon along a polyline: for each segment emit a quad (2 tris)
// offset +/- half_width along the in-plane perpendicular. No mitre joins -- the
// node markers / chevrons cover the small corner gaps, which is cheaper and
// cannot self-overlap into dark seams.
void drawRibbon(const std::vector<LLVector3>& pts, F32 half_width, const LLColor4& col)
{
    if (pts.size() < 2)
    {
        return;
    }
    gGL.begin(LLRender::TRIANGLES);
    gGL.color4fv(col.mV);
    for (size_t i = 0; i + 1 < pts.size(); ++i)
    {
        LLVector3 seg = pts[i + 1] - pts[i];
        seg.mV[VZ] = 0.f;
        const F32 len = seg.length();
        if (len < 1e-4f)
        {
            continue;
        }
        seg *= (1.f / len);
        const LLVector3 perp(-seg.mV[VY] * half_width, seg.mV[VX] * half_width, 0.f);
        const LLVector3 a0 = pts[i]     - perp;
        const LLVector3 a1 = pts[i]     + perp;
        const LLVector3 b0 = pts[i + 1] - perp;
        const LLVector3 b1 = pts[i + 1] + perp;
        gGL.vertex3fv(a0.mV); gGL.vertex3fv(b0.mV); gGL.vertex3fv(b1.mV);
        gGL.vertex3fv(a0.mV); gGL.vertex3fv(b1.mV); gGL.vertex3fv(a1.mV);
    }
    gGL.end();
}

// a dark, slightly-wider outline pass under a brighter fill pass, so the line
// reads over both bright and dark ground
void drawThickLine(const std::vector<LLVector3>& pts, F32 half_width, const LLColor4& fill)
{
    drawRibbon(pts, half_width * 1.7f, LLColor4(0.f, 0.f, 0.f, 0.65f));
    drawRibbon(pts, half_width, fill);
}

// forward-pointing filled chevrons spaced along the polyline's arc length
void drawChevrons(const std::vector<LLVector3>& pts, const LLColor4& col)
{
    if (pts.size() < 2)
    {
        return;
    }
    const F32 SPACING = 1.75f;
    const F32 SZ      = 0.22f;
    gGL.begin(LLRender::TRIANGLES);
    gGL.color4fv(col.mV);
    F32 acc = 0.f, next = 0.9f;
    for (size_t i = 0; i + 1 < pts.size(); ++i)
    {
        LLVector3 seg = pts[i + 1] - pts[i];
        seg.mV[VZ] = 0.f;
        const F32 len = seg.length();
        if (len < 1e-4f)
        {
            continue;
        }
        seg *= (1.f / len);
        const LLVector3 perp(-seg.mV[VY], seg.mV[VX], 0.f);
        while (next <= acc + len)
        {
            const F32 f = (next - acc) / len;
            const LLVector3 c = pts[i] + (pts[i + 1] - pts[i]) * f;
            const LLVector3 tip  = c + seg * SZ;
            const LLVector3 left  = c - seg * (SZ * 0.35f) + perp * (SZ * 0.75f);
            const LLVector3 right = c - seg * (SZ * 0.35f) - perp * (SZ * 0.75f);
            gGL.vertex3fv(tip.mV); gGL.vertex3fv(left.mV); gGL.vertex3fv(right.mV);
            next += SPACING;
        }
        acc += len;
    }
    gGL.end();
}

// a flat filled diamond lying on the ground at base, plus an upright post
void drawNodeMarker(const LLVector3& base, const LLColor4& col, bool dwell)
{
    const F32 R = 0.24f;
    gGL.begin(LLRender::TRIANGLES);
    gGL.color4fv(col.mV);
    const LLVector3 e0 = base + LLVector3( R, 0.f, 0.f);
    const LLVector3 e1 = base + LLVector3(0.f,  R, 0.f);
    const LLVector3 e2 = base + LLVector3(-R, 0.f, 0.f);
    const LLVector3 e3 = base + LLVector3(0.f, -R, 0.f);
    gGL.vertex3fv(e0.mV); gGL.vertex3fv(e1.mV); gGL.vertex3fv(e2.mV);
    gGL.vertex3fv(e0.mV); gGL.vertex3fv(e2.mV); gGL.vertex3fv(e3.mV);
    gGL.end();

    // dwell nodes get a cyan ring on the ground around the diamond
    if (dwell)
    {
        const F32 RR = R * 1.9f;
        gGL.setLineWidth(2.f);
        gGL.begin(LLRender::LINES);
        gGL.color4f(0.4f, 0.9f, 1.f, 0.95f);
        const S32 SEGS = 20;
        for (S32 s = 0; s < SEGS; ++s)
        {
            const F32 a = (F32)s        / SEGS * F_TWO_PI;
            const F32 b = (F32)(s + 1)  / SEGS * F_TWO_PI;
            gGL.vertex3f(base.mV[VX] + RR * cosf(a), base.mV[VY] + RR * sinf(a), base.mV[VZ]);
            gGL.vertex3f(base.mV[VX] + RR * cosf(b), base.mV[VY] + RR * sinf(b), base.mV[VZ]);
        }
        gGL.end();
    }

    // upright post so the number floats legibly above the node
    gGL.setLineWidth(2.5f);
    gGL.begin(LLRender::LINES);
    gGL.color4fv(col.mV);
    gGL.vertex3fv(base.mV);
    gGL.vertex3f(base.mV[VX], base.mV[VY], base.mV[VZ] + NODE_STICK_H);
    gGL.end();
}

// selected-node emphasis drawn on top of the normal marker: an enlarged,
// breathing filled diamond plus a pulsing ground ring. pulse in [0,1] comes
// from the frame clock (LLFrameTimer, NOT Math-random), so the selected node
// reads as "live" without any per-frame randomness.
void drawSelectedHighlight(const LLVector3& base, const LLColor4& col, F32 pulse)
{
    const F32 R = 0.30f + 0.12f * pulse;            // breathing radius, m
    LLColor4 hi = col;
    hi.mV[VX] = llmin(1.f, hi.mV[VX] * 1.3f + 0.25f);
    hi.mV[VY] = llmin(1.f, hi.mV[VY] * 1.3f + 0.25f);
    hi.mV[VZ] = llmin(1.f, hi.mV[VZ] * 1.3f + 0.25f);
    hi.mV[VW] = 0.55f + 0.4f * pulse;

    gGL.begin(LLRender::TRIANGLES);
    gGL.color4fv(hi.mV);
    const LLVector3 e0 = base + LLVector3( R, 0.f, 0.f);
    const LLVector3 e1 = base + LLVector3(0.f,  R, 0.f);
    const LLVector3 e2 = base + LLVector3(-R, 0.f, 0.f);
    const LLVector3 e3 = base + LLVector3(0.f, -R, 0.f);
    gGL.vertex3fv(e0.mV); gGL.vertex3fv(e1.mV); gGL.vertex3fv(e2.mV);
    gGL.vertex3fv(e0.mV); gGL.vertex3fv(e2.mV); gGL.vertex3fv(e3.mV);
    gGL.end();

    const F32 RR = R * 2.0f;
    gGL.setLineWidth(2.5f);
    gGL.begin(LLRender::LINES);
    gGL.color4fv(hi.mV);
    const S32 SEGS = 24;
    for (S32 s = 0; s < SEGS; ++s)
    {
        const F32 a = (F32)s       / SEGS * F_TWO_PI;
        const F32 b = (F32)(s + 1) / SEGS * F_TWO_PI;
        gGL.vertex3f(base.mV[VX] + RR * cosf(a), base.mV[VY] + RR * sinf(a), base.mV[VZ]);
        gGL.vertex3f(base.mV[VX] + RR * cosf(b), base.mV[VY] + RR * sinf(b), base.mV[VZ]);
    }
    gGL.end();
}

// a camera-billboarded integer drawn as 7-segment line digits at anchor
// (anchor = baseline centre). right/up are the screen-facing axes.
void drawNumber(S32 value, const LLVector3& anchor, F32 h,
                const LLVector3& right, const LLVector3& up, const LLColor4& col)
{
    // segment endpoints in a unit cell [0..1] x [0..1]: a b c d e f g
    static const F32 SEG[7][4] = {
        {0.f, 1.f, 1.f, 1.f},   // a  top
        {1.f, 1.f, 1.f, .5f},   // b  upper-right
        {1.f, .5f, 1.f, 0.f},   // c  lower-right
        {0.f, 0.f, 1.f, 0.f},   // d  bottom
        {0.f, .5f, 0.f, 0.f},   // e  lower-left
        {0.f, 1.f, 0.f, .5f},   // f  upper-left
        {0.f, .5f, 1.f, .5f}    // g  middle
    };
    // bit per segment: a=1 b=2 c=4 d=8 e=16 f=32 g=64
    static const U8 DIG[10] = { 63, 6, 91, 79, 102, 109, 125, 7, 127, 111 };

    const std::string s = std::to_string(llmax(0, value));
    const F32 w   = h * 0.6f;               // digit cell width
    const F32 gap = w * 0.4f;               // inter-digit gap
    const F32 total_w = s.size() * w + (s.size() - 1) * gap;
    LLVector3 pen = anchor - right * (total_w * 0.5f);   // left edge, centred

    gGL.setLineWidth(2.5f);
    gGL.begin(LLRender::LINES);
    gGL.color4fv(col.mV);
    for (char ch : s)
    {
        const S32 d = ch - '0';
        if (d >= 0 && d <= 9)
        {
            const U8 mask = DIG[d];
            for (S32 seg = 0; seg < 7; ++seg)
            {
                if (!(mask & (1 << seg)))
                {
                    continue;
                }
                const LLVector3 p0 = pen + right * (SEG[seg][0] * w) + up * (SEG[seg][1] * h);
                const LLVector3 p1 = pen + right * (SEG[seg][2] * w) + up * (SEG[seg][3] * h);
                gGL.vertex3fv(p0.mV);
                gGL.vertex3fv(p1.mV);
            }
        }
        pen += right * (w + gap);
    }
    gGL.end();
}

// a small camera frustum gizmo at a node's authored camera pose: the apex
// (camera origin), four edges out to a lens rectangle a short way along the
// aim, the rectangle itself, and a short "up" nub so roll/tilt reads. Tinted
// by the transition so the director sees cut vs ease at a glance. Lines only,
// in the same no-depth UI pass. rot is the stored camera orientation (the SL
// camera looks down its +X axis; +Z is up, +Y is left).
void drawCameraGizmo(const LLVector3& apex, const LLQuaternion& rot, F32 vfov,
                     const LLColor4& col)
{
    const LLVector3 fwd = LLVector3(1.f, 0.f, 0.f) * rot;
    const LLVector3 up  = LLVector3(0.f, 0.f, 1.f) * rot;
    const LLVector3 rgt = LLVector3(0.f, -1.f, 0.f) * rot;   // right = -left(Y)

    const F32 L  = 0.7f;                                     // gizmo depth, m
    const F32 hh = L * tanf(llclamp(vfov, 0.15f, 2.6f) * 0.5f);
    const F32 hw = hh * 1.5f;                                // gizmo lens aspect
    const LLVector3 c  = apex + fwd * L;
    const LLVector3 tl = c - rgt * hw + up * hh;
    const LLVector3 tr = c + rgt * hw + up * hh;
    const LLVector3 bl = c - rgt * hw - up * hh;
    const LLVector3 br = c + rgt * hw - up * hh;

    gGL.setLineWidth(2.f);
    gGL.begin(LLRender::LINES);
    gGL.color4fv(col.mV);
    // apex -> lens corners
    gGL.vertex3fv(apex.mV); gGL.vertex3fv(tl.mV);
    gGL.vertex3fv(apex.mV); gGL.vertex3fv(tr.mV);
    gGL.vertex3fv(apex.mV); gGL.vertex3fv(bl.mV);
    gGL.vertex3fv(apex.mV); gGL.vertex3fv(br.mV);
    // lens rectangle
    gGL.vertex3fv(tl.mV); gGL.vertex3fv(tr.mV);
    gGL.vertex3fv(tr.mV); gGL.vertex3fv(br.mV);
    gGL.vertex3fv(br.mV); gGL.vertex3fv(bl.mV);
    gGL.vertex3fv(bl.mV); gGL.vertex3fv(tl.mV);
    // up nub (roll indicator) rising from the top edge centre
    const LLVector3 tc = (tl + tr) * 0.5f;
    gGL.vertex3fv(tc.mV); gGL.vertex3fv((tc + up * (hh * 0.6f)).mV);
    gGL.end();

    // a small filled diamond at the apex so the camera origin reads as a point
    const F32 R = 0.09f;
    gGL.begin(LLRender::TRIANGLES);
    gGL.color4fv(col.mV);
    const LLVector3 a0 = apex + rgt * R;
    const LLVector3 a1 = apex + up  * R;
    const LLVector3 a2 = apex - rgt * R;
    const LLVector3 a3 = apex - up  * R;
    gGL.vertex3fv(a0.mV); gGL.vertex3fv(a1.mV); gGL.vertex3fv(a2.mV);
    gGL.vertex3fv(a0.mV); gGL.vertex3fv(a2.mV); gGL.vertex3fv(a3.mV);
    gGL.end();
}

// onion-skin: a faint humanoid SILHOUETTE at a node so a director can read the
// blocking at each waypoint without pressing ACTION. A cheap stick figure --
// spine, billboarded head, shoulders/arms, splayed legs -- plus a ground facing
// arrow, tinted to the actor. Lines/triangles only, in the same no-depth UI
// pass; deliberately NOT a skinned-mesh instance (too costly). base = foot/ground
// point (agent frame), face = horizontal travel direction, height = figure
// height (m), bb_right/bb_up = camera billboard axes for the head ring.
void drawPoseGhost(const LLVector3& base, const LLVector3& face, F32 height,
                   const LLColor4& tint, const LLVector3& bb_right, const LLVector3& bb_up)
{
    const F32 H      = llclamp(height, 1.2f, 2.2f);
    const F32 hipZ   = H * 0.52f;
    const F32 shZ    = H * 0.82f;
    const F32 headZ  = H * 0.92f;
    const F32 halfsh = H * 0.11f;   // half shoulder width
    const F32 halfft = H * 0.09f;   // half stance width
    const F32 hr     = H * 0.07f;   // head radius

    LLVector3 f = face; f.mV[VZ] = 0.f;
    if (f.magVecSquared() < 1e-6f)
    {
        f = LLVector3(1.f, 0.f, 0.f);
    }
    f.normalize();
    const LLVector3 perp(-f.mV[VY], f.mV[VX], 0.f);     // horizontal, across facing

    const LLVector3 hipC = base + LLVector3(0.f, 0.f, hipZ);
    const LLVector3 shC  = base + LLVector3(0.f, 0.f, shZ);
    const LLVector3 shL  = shC - perp * halfsh;
    const LLVector3 shR  = shC + perp * halfsh;

    LLColor4 c = tint; c.mV[VW] = 0.30f;               // faint body
    gGL.setLineWidth(2.f);
    gGL.begin(LLRender::LINES);
    gGL.color4fv(c.mV);
    // spine + shoulders
    gGL.vertex3fv(hipC.mV); gGL.vertex3fv(shC.mV);
    gGL.vertex3fv(shL.mV);  gGL.vertex3fv(shR.mV);
    // arms angling down toward the hands
    gGL.vertex3fv(shL.mV);
    gGL.vertex3fv((base + perp * (halfsh * 1.15f) + LLVector3(0.f, 0.f, hipZ * 0.72f)).mV);
    gGL.vertex3fv(shR.mV);
    gGL.vertex3fv((base - perp * (halfsh * 1.15f) + LLVector3(0.f, 0.f, hipZ * 0.72f)).mV);
    // legs from the hip to splayed feet
    gGL.vertex3fv(hipC.mV); gGL.vertex3fv((base - perp * halfft).mV);
    gGL.vertex3fv(hipC.mV); gGL.vertex3fv((base + perp * halfft).mV);
    // head: a small billboarded ring (always faces the viewer)
    const LLVector3 hc = base + LLVector3(0.f, 0.f, headZ);
    const S32 SEG = 10;
    for (S32 s = 0; s < SEG; ++s)
    {
        const F32 a = (F32)s       / SEG * F_TWO_PI;
        const F32 b = (F32)(s + 1) / SEG * F_TWO_PI;
        gGL.vertex3fv((hc + bb_right * (hr * cosf(a)) + bb_up * (hr * sinf(a))).mV);
        gGL.vertex3fv((hc + bb_right * (hr * cosf(b)) + bb_up * (hr * sinf(b))).mV);
    }
    gGL.end();

    // ground facing arrow (a touch stronger than the body so heading reads)
    LLColor4 ac = tint; ac.mV[VW] = 0.5f;
    const F32 AL = 0.5f;
    const LLVector3 g   = base + LLVector3(0.f, 0.f, 0.02f);
    const LLVector3 tip = g + f * AL;
    const LLVector3 al  = g + f * (AL * 0.55f) + perp * (AL * 0.28f);
    const LLVector3 ar  = g + f * (AL * 0.55f) - perp * (AL * 0.28f);
    gGL.begin(LLRender::TRIANGLES);
    gGL.color4fv(ac.mV);
    gGL.vertex3fv(tip.mV); gGL.vertex3fv(al.mV); gGL.vertex3fv(ar.mV);
    gGL.end();
}
} // anonymous namespace

// ---------------------------------------------------------------------------
void LLActorMover::renderHeadingPreview()
{
    static LLCachedControl<bool> show(gSavedSettings, "ActorMoverShowHeading", true);
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
    // onion-skin: faint pose ghosts at each node. Opt-in, default off, so the
    // overlay pass adds nothing per frame unless the director asks for it.
    static LLCachedControl<bool> onion(gSavedSettings, "PathShowOnionSkin", false);
    const F32 dist = llmax((F32)distance, 0.1f);

    // same beacon-style local overlay as renderObjectBeacons(): UI shader, no
    // texture, no depth writes -- strictly a client-side overlay
    LLGLSUIDefault gls_ui;
    gUIProgram.bind();
    gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

    // camera basis for billboarded node numbers (face the viewer, stay upright)
    LLViewerCamera* cam = LLViewerCamera::getInstance();
    const LLVector3 bb_right = -cam->getLeftAxis();
    const LLVector3 bb_up    = cam->getUpAxis();

    // digits stay white for legibility over any ribbon hue; every other color is
    // now derived PER ACTOR (see actorPathColor) so multiple paths are distinct.
    const LLColor4 col_num(1.f, 1.f, 1.f, 1.f);

    // pulse phase for the selected-node highlight, from the frame clock (never
    // Math-random): a slow breathe shared by every actor's selected node.
    const F32 now   = (F32)LLFrameTimer::getElapsedSeconds();
    const F32 pulse = 0.5f + 0.5f * sinf(now * 3.2f);

    // blend toward a role color while staying in the actor's hue family, so
    // start reads greenish and end reddish but a viewer can still tell whose
    // path it is at a glance
    auto blend = [](const LLColor4& a, const LLColor4& b, F32 t) -> LLColor4
    {
        return LLColor4(a.mV[VX] * (1.f - t) + b.mV[VX] * t,
                        a.mV[VY] * (1.f - t) + b.mV[VY] * t,
                        a.mV[VZ] * (1.f - t) + b.mV[VZ] * t,
                        a.mV[VW] * (1.f - t) + b.mV[VW] * t);
    };

    const LLUUID edit_actor = mEditActor;
    const S32    edit_node  = mEditNode;

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

        // per-actor tint: ribbon = the stable hue, chevrons brighter, start
        // blended toward green, end toward red, interior nodes the base hue
        const LLColor4 actor_col = actorPathColor(av->getID());
        LLColor4 col_line  = actor_col; col_line.mV[VW]  = 0.9f;
        LLColor4 col_chev  = blend(actor_col, LLColor4(1.f, 1.f, 1.f, 1.f), 0.45f);
        col_chev.mV[VW]    = 0.95f;
        LLColor4 col_start = blend(actor_col, LLColor4(0.3f, 1.f, 0.35f, 1.f), 0.55f);
        col_start.mV[VW]   = 0.98f;
        LLColor4 col_end   = blend(actor_col, LLColor4(1.f, 0.28f, 0.2f, 1.f), 0.55f);
        col_end.mV[VW]     = 0.98f;
        LLColor4 col_mid   = actor_col; col_mid.mV[VW]   = 0.95f;
        const bool is_edit_actor = (edit_actor.notNull() && edit_actor == av->getID());

        // a walkable path (>= 2 nodes) draws the full spline + numbered markers;
        // otherwise the upgraded thick straight heading arrow (today's preview)
        auto pit = mPaths.find(av->getID());
        if (pit != mPaths.end() && pit->second.mNodes.size() >= 2)
        {
            Path& path = pit->second;
            if (path.mDirty)
            {
                path.rebuild();
            }

            // sample the spline at short arc-length steps for the polyline
            const F32 total = llmax(path.mTotalLength, 0.01f);
            const S32 steps = llclamp((S32)ceilf(total / 0.35f), 1, 4096);
            std::vector<LLVector3> pts;
            pts.reserve(steps + 1);
            for (S32 i = 0; i <= steps; ++i)
            {
                const F32 d = total * (F32)i / (F32)steps;
                LLVector3d gp, gt;
                path.evalAtDistance(d, gp, gt);
                LLVector3 a = gAgent.getPosAgentFromGlobal(gp);
                a.mV[VZ] += PATH_RIBBON_LIFT;
                pts.push_back(a);
            }

            drawThickLine(pts, 0.11f, col_line);
            drawChevrons(pts, col_chev);

            // numbered node markers, start/end distinguished, dwell flagged
            const S32 n = (S32)path.mNodes.size();
            const bool loop = (path.mEndMode == 1);
            for (S32 i = 0; i < n; ++i)
            {
                LLVector3 base = gAgent.getPosAgentFromGlobal(path.mNodes[i].mPosGlobal);
                base.mV[VZ] += NODE_BASE_LIFT;
                const bool is_start = (i == 0);
                const bool is_end   = (i == n - 1) && !loop;    // a loop has no distinct end
                const LLColor4& c = is_start ? col_start : (is_end ? col_end : col_mid);

                // the edit-selected node breathes under an enlarged highlight
                if (is_edit_actor && i == edit_node)
                {
                    drawSelectedHighlight(base, c, pulse);
                }
                drawNodeMarker(base, c, path.mNodes[i].mDwell > 0.f);

                LLVector3 num_at = base;
                num_at.mV[VZ] += NODE_STICK_H + 0.06f;
                drawNumber(i + 1, num_at, NODE_NUM_H, bb_right, bb_up, col_num);

                // onion-skin ghost of the actor's blocking at this node, facing
                // the way it would travel through it (next node, or previous for
                // the final node / the seam for a loop)
                if (onion)
                {
                    S32 fromIdx = i;
                    S32 toIdx   = (i + 1 < n) ? (i + 1) : (loop ? 0 : i);
                    if (toIdx == fromIdx && i > 0)
                    {
                        fromIdx = i - 1;    // last node of an open path: face along the incoming leg
                        toIdx   = i;
                    }
                    LLVector3 face(1.f, 0.f, 0.f);
                    if (toIdx != fromIdx)
                    {
                        face = gAgent.getPosAgentFromGlobal(path.mNodes[toIdx].mPosGlobal)
                             - gAgent.getPosAgentFromGlobal(path.mNodes[fromIdx].mPosGlobal);
                    }
                    const F32 gh = path.mNodes[i].mRootAbove * 1.9f;
                    drawPoseGhost(base, face, gh, col_mid, bb_right, bb_up);
                }

                // authored camera on this node: a frustum gizmo at the camera
                // pose plus a thin leader from the node up to the camera, so the
                // shot layout is legible in world. Cut = warm amber, ease = cyan.
                const LLActorMover::Waypoint& wp = path.mNodes[i];
                if (wp.mHasCam)
                {
                    const LLVector3 apex = gAgent.getPosAgentFromGlobal(wp.mCamPosGlobal);
                    const bool cut = (wp.mCamTransition == 0);
                    const LLColor4 cam_col = cut
                        ? LLColor4(1.f, 0.58f, 0.15f, 0.95f)     // hard cut: amber
                        : LLColor4(0.35f, 0.85f, 1.f, 0.95f);    // ease: cyan

                    // leader line node -> camera apex (dim, so it doesn't shout)
                    gGL.setLineWidth(1.5f);
                    gGL.begin(LLRender::LINES);
                    gGL.color4f(cam_col.mV[VX], cam_col.mV[VY], cam_col.mV[VZ], 0.45f);
                    gGL.vertex3f(base.mV[VX], base.mV[VY], base.mV[VZ] + NODE_STICK_H * 0.5f);
                    gGL.vertex3fv(apex.mV);
                    gGL.end();

                    drawCameraGizmo(apex, wp.mCamRot, wp.mCamFov > 0.01f
                                        ? wp.mCamFov
                                        : LLViewerCamera::getInstance()->getDefaultFOV(),
                                    cam_col);
                }
            }
        }
        else
        {
            // ---- upgraded straight heading arrow (no path set) ----
            LLVector3 at = LLVector3(1.f, 0.f, 0.f) * av->getRenderRotation();
            const F32 yaw = atan2f(at.mV[VY], at.mV[VX]) + (F32)heading * DEG_TO_RAD;
            const LLVector3 dir(cosf(yaw), sinf(yaw), 0.f);

            LLVector3 origin = av->getRootJoint()->getWorldPosition();
            origin.mV[VZ] += PATH_RIBBON_LIFT - av->getPelvisToFoot();   // just above ground
            const LLVector3 end = origin + dir * dist;

            std::vector<LLVector3> pts{ origin, end };
            drawThickLine(pts, 0.10f, col_line);

            // filled arrowhead + a small vertical tick at the endpoint
            const LLVector3 perp(-dir.mV[VY], dir.mV[VX], 0.f);
            const F32 hb = llmin(0.5f, dist * 0.28f);
            const LLVector3 tip = end + dir * (hb * 0.6f);
            const LLVector3 lft = end + perp * (hb * 0.5f) - dir * (hb * 0.1f);
            const LLVector3 rgt = end - perp * (hb * 0.5f) - dir * (hb * 0.1f);
            gGL.begin(LLRender::TRIANGLES);
            gGL.color4fv(col_line.mV);
            gGL.vertex3fv(tip.mV); gGL.vertex3fv(lft.mV); gGL.vertex3fv(rgt.mV);
            gGL.end();

            gGL.setLineWidth(2.5f);
            gGL.begin(LLRender::LINES);
            gGL.color4fv(col_line.mV);
            gGL.vertex3fv(end.mV);
            gGL.vertex3f(end.mV[VX], end.mV[VY], end.mV[VZ] + 0.3f);
            gGL.end();
        }
    }

    gGL.setLineWidth(1.f);
    gGL.flush();
}
