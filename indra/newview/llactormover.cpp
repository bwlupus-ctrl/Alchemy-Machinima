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
#include <set>                      // collectGhostBatches (wanted-actor set)

#include "alobjectpathmover.h"      // heading preview also draws enrolled PROP paths
#include "llfetchedgltfmaterial.h"  // static-face PBR base-colour texture resolution

#include "alghoststudio.h"          // [GhostStudio] free-standing ghost instances
#include "altoolghostedit.h"        // [R2-3] selected-ghost ring gates on the edit tool
#include "llagent.h"                // gAgent global<->agent coord conversion (pathing)
#include "llanimationstates.h"      // ANIM_AGENT_WALK
#include "llappviewer.h"            // gFrameIntervalSeconds
#include "llcontrolavatar.h"        // animesh attachments bucket under the wearer (model ghost)
#include "lldirectorcast.h"         // [Director] roster storage + per-actor loco anim
#include "lldrawpool.h"             // LLRenderPass (rigged pass enum + uploadMatrixPalette)
#include "llflycamrecorder.h"       // sync-to-take: the recorder playhead is the clock
#include "llfloaterreg.h"           // heading preview only draws with the floater open
#include "llframetimer.h"           // per-frame idempotency for applyOverride()
#include "llmaterial.h"             // legacy alpha-mode classification (static ghost faces)
#include "llmotion.h"               // LLMotion::setPriorityOverride (custom-anim priority)
#include "llgl.h"
#include "llglstates.h"             // LLGLSUIDefault (heading preview)
#include "lljoint.h"
#include "llrender.h"               // gGL (heading preview)
#include "llspatialpartition.h"     // LLDrawInfo / LLCullResult (model-ghost geometry sweep)
#include "lltoolmgr.h"              // [R2-3] edit-tool gate for the highlight ring
#include "llvector4a.h"             // downward ground raycast (pathing ground-follow)
#include "llvertexbuffer.h"         // draw the actor's rigged batches for the model ghost
#include "llviewercamera.h"         // camera basis for billboarded path node numbers
#include "llviewercontrol.h"        // gSavedSettings, LLCachedControl
#include "llviewershadermgr.h"      // gUIProgram / gHighlightProgram (heading preview + model ghost)
#include "llviewertexture.h"        // sWhiteImagep (flat-tint texture for the model ghost)
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

// Apply the client-side custom-anim priority (ActorMoverCustomAnimPriority) to a
// just-started loco motion. -1 = baked (no override, byte-identical); 0..4 force
// LOW..HIGHEST. Skipped for the built-in system walk. The override is PER-INSTANCE
// (see LLMotion::setPriorityOverride) so other avatars are unaffected; it also
// survives the not-yet-loaded case (the instance exists in STATUS_HOLD and honors
// the override when its joints load). Cleared automatically when the anim stops.
void apply_custom_anim_priority(LLVOAvatar* av, const LLUUID& anim)
{
    if (!av || anim.isNull() || anim == ANIM_AGENT_WALK)
    {
        return;
    }
    static LLCachedControl<S32> prio(gSavedSettings, "ActorMoverCustomAnimPriority", -1);
    if (prio < 0)
    {
        return;
    }
    if (LLMotion* m = av->findMotion(anim))
    {
        m->setPriorityOverride(prio);
    }
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

// [ObjectPath] public wrappers over the file-scope evaluators above, so the
// object path mover drives with EXACTLY the walk's speed math (overrides +
// ease) and the interpolated per-node skid yaw. Named eval* (not pathSpeedAt)
// so class-scope lookup inside member functions never shadows the free helper.
F32 LLActorMover::evalPathSpeed(const Path& path, F32 dist, bool skip_ease)
{
    return pathSpeedAt(path, dist, skip_ease);
}

F32 LLActorMover::evalPathYawOffset(const Path& path, F32 dist)
{
    return pathNodeScalarAt(path, dist,
                            [](const Waypoint& w) { return w.mYawOffset; });
}

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

// [ObjectPath] per-node skid yaw (same field-setter shape as its siblings)
bool LLActorMover::setNodeYawOffset(const LLUUID& actor_id, S32 index, F32 yaw_rad)
{
    const Path* cp = getPath(actor_id);
    if (!cp || index < 0 || index >= (S32)cp->mNodes.size())
    {
        return false;
    }
    Path& path = editPath(actor_id);
    path.mNodes[index].mYawOffset = yaw_rad;
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

// ===========================================================================
// P3 Look-at while walking (procedural gaze): per-actor config accessors. The
// paint itself is applyGaze()/gazePaint() further down; these just read/write
// the session-only Gaze map keyed by the resolved actor id. Getters return the
// struct defaults when the actor has no entry yet, so the panel can show sane
// values before anything is authored.
// ===========================================================================
void LLActorMover::setGazeEnabled(const LLUUID& actor_id, bool on)
{
    mGazes[path_key(actor_id)].mEnabled = on;
}

bool LLActorMover::isGazeEnabled(const LLUUID& actor_id) const
{
    auto it = mGazes.find(path_key(actor_id));
    return it != mGazes.end() && it->second.mEnabled;
}

void LLActorMover::setGazeTargetMode(const LLUUID& actor_id, S32 mode)
{
    mGazes[path_key(actor_id)].mTarget = llclamp(mode, (S32)GAZE_TANGENT, (S32)GAZE_POINT);
}

S32 LLActorMover::getGazeTargetMode(const LLUUID& actor_id) const
{
    auto it = mGazes.find(path_key(actor_id));
    return it != mGazes.end() ? it->second.mTarget : (S32)GAZE_TANGENT;
}

void LLActorMover::setGazeCastTarget(const LLUUID& actor_id, const LLUUID& cast_id)
{
    mGazes[path_key(actor_id)].mCastTarget = cast_id;
}

LLUUID LLActorMover::getGazeCastTarget(const LLUUID& actor_id) const
{
    auto it = mGazes.find(path_key(actor_id));
    return it != mGazes.end() ? it->second.mCastTarget : LLUUID::null;
}

void LLActorMover::setGazePointGlobal(const LLUUID& actor_id, const LLVector3d& p)
{
    mGazes[path_key(actor_id)].mPoint = p;
}

void LLActorMover::setGazeHeadEyeBlend(const LLUUID& actor_id, F32 v)
{
    mGazes[path_key(actor_id)].mHeadEyeBlend = llclamp(v, 0.f, 1.f);
}

F32 LLActorMover::getGazeHeadEyeBlend(const LLUUID& actor_id) const
{
    auto it = mGazes.find(path_key(actor_id));
    return it != mGazes.end() ? it->second.mHeadEyeBlend : 0.7f;
}

void LLActorMover::setGazeIntensity(const LLUUID& actor_id, F32 v)
{
    mGazes[path_key(actor_id)].mIntensity = llclamp(v, 0.f, 1.f);
}

F32 LLActorMover::getGazeIntensity(const LLUUID& actor_id) const
{
    auto it = mGazes.find(path_key(actor_id));
    return it != mGazes.end() ? it->second.mIntensity : 1.f;
}

void LLActorMover::setGazeSmoothing(const LLUUID& actor_id, F32 v)
{
    mGazes[path_key(actor_id)].mSmoothing = llclamp(v, 0.f, 1.f);
}

F32 LLActorMover::getGazeSmoothing(const LLUUID& actor_id) const
{
    auto it = mGazes.find(path_key(actor_id));
    return it != mGazes.end() ? it->second.mSmoothing : 0.5f;
}

bool LLActorMover::getGazeStatus(const LLUUID& actor_id, std::string& out) const
{
    if (actor_id.isNull() && !isAgentAvatarValid())
    {
        out = "Select a cast member";
        return false;
    }
    const LLUUID key = path_key(actor_id);
    auto it = mGazes.find(key);
    if (it == mGazes.end() || !it->second.mEnabled)
    {
        out = "Gaze off \xE2\x80\x94 the walk's own head motion plays";
        return true;
    }
    const Gaze& g = it->second;

    // describe the target
    std::string tgt;
    switch (g.mTarget)
    {
        case GAZE_CAMERA: tgt = "the camera"; break;
        case GAZE_CAST:
        {
            std::string name;
            if (const LLDirectorCast::CastMember* m =
                    LLDirectorCast::instance().getMember(g.mCastTarget))
            {
                name = m->mLastName;
            }
            tgt = g.mCastTarget.isNull()
                      ? std::string("a cast member (none picked)")
                      : (name.empty() ? std::string("a cast member") : name);
            break;
        }
        case GAZE_POINT:
            tgt = g.mPoint.isExactlyZero() ? std::string("a fixed point (unset)")
                                           : std::string("a fixed point");
            break;
        case GAZE_TANGENT:
        default:
            tgt = "where it's going";
            break;
    }

    // is it actually painting right now (under an active, non-suspended Move)?
    auto mit = mMoves.find(key);
    const bool painting = mit != mMoves.end() && !mit->second.mSuspended;
    if (!painting)
    {
        out = llformat("Gaze armed \xE2\x80\x94 will look at %s when the walk starts",
                       tgt.c_str());
    }
    else
    {
        out = llformat("Looking at %s", tgt.c_str());
    }
    return true;
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
        apply_custom_anim_priority(av, mv.mAnim);
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
        apply_custom_anim_priority(av, mv.mAnim);
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
    apply_custom_anim_priority(av, mv.mAnim);
    av->setAnimTimeFactor(mv.mSpeed / llmax((F32)nominal, 0.5f));

    LL_INFOS("ActorMover") << "ghost move: " << av->getID() << " speed " << mv.mSpeed
                           << " dist " << mv.mDistance << " anim " << mv.mAnim << LL_ENDL;
}

void LLActorMover::startAll()
{
    // [Director] delay-aware: members of a cast group carrying a start delay
    // are queued and begin N seconds later (staggered starts) -- so ACTION,
    // the console group Start and the shared panel's Walk-everyone all honor
    // the same per-group delays. With no delays configured this reduces to
    // the plain per-member start loop (self when the roster is empty).
    LLDirectorCast::instance().startMovesStaggered(getRoster());
}

void LLActorMover::stop(const LLUUID& actor_id)
{
    // [Director] a deliberate stop also disarms this actor's queued staggered
    // start (group delay), so no Stop button can leave a surprise walk armed
    LLDirectorCast::instance().cancelPendingStart(actor_id);
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
    // [Director] stop-everything also empties the staggered-start queue
    LLDirectorCast::instance().cancelPendingStarts();
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
            apply_custom_anim_priority(av, mv.mAnim);
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
                apply_custom_anim_priority(av, mv.mAnim);
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

// ===========================================================================
// P3 Look-at while walking (procedural gaze): the per-frame paint. Ported from
// the viewer's own look-at motions (LLHeadRotMotion + LLEyeMotion) so it reads
// as natural as stock head tracking, then wrapped in the knobs the built-in
// lacks (head-vs-eyes blend, intensity, smoothing) plus an ease-in/out envelope.
//
// WHERE IN THE FRAME: applyGaze() is called from LLVOAvatar::updateCharacter
// AFTER updateMotions() has posed the skeleton, so the override layers on the
// current anim pose (unlike applyOverride(), which runs pre-motion and only
// touches the root). Because ANIM_AGENT_HEAD_ROT / ANIM_AGENT_EYE re-pose
// mHead/mNeck/mTorso/mEye* every frame for ordinary avatars, joint->getRotation()
// at entry is the anim pose: the nlerp below layers on it, and easing the weight
// to 0 returns cleanly to it (no frozen head-lock). We never permanently corrupt
// the skeleton -- the motion controller rebuilds it next frame and we re-assert.
// ===========================================================================
namespace
{
const F32 GAZE_EASE_TIME      = 0.35f;   // ease-in / ease-out envelope, s
const F32 GAZE_TAU_MIN        = 0.04f;   // dir smoothing time constant, s (snappy)
const F32 GAZE_TAU_MAX        = 0.50f;   // (very smooth)
const F32 GAZE_LOOKAHEAD      = 6.0f;    // tangent look-ahead distance, m
const F32 GAZE_MIN_DIST       = 0.25f;   // ignore a target closer than this to the head
const F32 GAZE_TORSO_LAG      = 0.25f;   // torso share of the combined head aim
const F32 GAZE_NECK_LAG       = 0.50f;   // neck vs head split of the remaining aim
const F32 GAZE_HEAD_YAW_MAX   = 72.f * DEG_TO_RAD;  // combined body-relative yaw clamp
const F32 GAZE_HEAD_PITCH_MAX = 45.f * DEG_TO_RAD;  // combined body-relative pitch clamp
const F32 GAZE_EYE_YAW_MAX    = 35.f * DEG_TO_RAD;  // per spec
const F32 GAZE_EYE_PITCH_MAX  = 25.f * DEG_TO_RAD;  // per spec
} // anonymous namespace

void LLActorMover::applyGaze(LLVOAvatar* av)
{
    if (mGazes.empty() || !av)
    {
        return;                 // default no-op: no gaze configured anywhere
    }
    auto git = mGazes.find(av->getID());
    if (git == mGazes.end())
    {
        return;                 // this avatar has no gaze entry -> byte-identical
    }
    Gaze& g = git->second;

    // gaze PAINTS only while enabled AND the actor is under an active, non-
    // suspended Move (a walk or a placeAt hold). Anything else -> ease out and
    // release. This is what makes it compose with walk-and-talk and release
    // cleanly on walk end / suspend / derez / disable.
    const Move* mv = nullptr;
    if (g.mEnabled)
    {
        auto mit = mMoves.find(av->getID());
        if (mit != mMoves.end() && !mit->second.mSuspended)
        {
            mv = &mit->second;
        }
    }
    const bool active = (mv != nullptr);

    // advance the envelope + direction smoothing once per frame; the joint set is
    // re-asserted on every call (same idempotent idiom as applyOverride)
    const U32  frame   = LLFrameTimer::getFrameCount();
    const bool advance = (g.mLastFrame != frame);
    F32        dt      = 0.f;
    if (advance)
    {
        g.mLastFrame = frame;
        dt = llclamp(gFrameIntervalSeconds.value(), 0.f, 0.25f);
        const F32 step = (GAZE_EASE_TIME > 0.f) ? dt / GAZE_EASE_TIME : 1.f;
        g.mEnv = llclamp(g.mEnv + (active ? step : -step), 0.f, 1.f);
    }

    // fully released and inactive: stop touching the joints entirely (the walk /
    // AO plays exactly as today). Reseed the smoother so a later re-activation
    // never snaps in from a stale direction.
    if (!active && g.mEnv <= 0.001f)
    {
        g.mDirValid = false;
        return;
    }
    if (av->isDead() || !av->getRootJoint())
    {
        return;                 // never paint a dead / rootless actor
    }

    gazePaint(av, g, mv, dt, advance);
}

void LLActorMover::gazePaint(LLVOAvatar* av, Gaze& g, const Move* mv, F32 dt, bool advance)
{
    LLJoint* head = av->getJoint("mHead");
    LLJoint* root = av->getJoint("mRoot");
    if (!head || !root)
    {
        return;                 // no head/root to drive (animesh without them)
    }

    const LLVector3 headPos = head->getWorldPosition();

    // ---- resolve the LIVE desired look direction (world / agent frame) --------
    // A direction along the current travel (path tangent, look-ahead) is the
    // default; camera / cast / point resolve to a world point instead. An
    // unresolvable cast target or a too-close point falls back to the tangent.
    auto tangentDir = [&]() -> LLVector3
    {
        if (mv && mv->mIsPath)
        {
            const Path* p = getPath(av->getID());
            if (p && p->mNodes.size() >= 2 && !p->mDirty && p->mTotalLength > 0.01f)
            {
                LLVector3d pos_g, tan_g;
                const F32 d = llclamp(mv->mDist + GAZE_LOOKAHEAD, 0.f, p->mTotalLength);
                p->evalAtDistance(d, pos_g, tan_g);
                LLVector3 t((F32)tan_g.mdV[VX], (F32)tan_g.mdV[VY], (F32)tan_g.mdV[VZ]);
                if (mv->mDir < 0.f) { t = -t; }     // ping-pong return leg
                if (t.magVecSquared() > 1e-6f) { return t; }
            }
        }
        // straight move / hold: look along the (turn-rate-smoothed) facing
        return mv ? (LLVector3(1.f, 0.f, 0.f) * mv->mCurRot) : LLVector3(1.f, 0.f, 0.f);
    };

    LLVector3 dir(0.f, 0.f, 0.f);
    bool      haveDir = false;
    if (mv)                     // only chase a live target while active
    {
        LLVector3 target(0.f, 0.f, 0.f);
        bool usePoint = false;
        switch (g.mTarget)
        {
            case GAZE_CAMERA:
                target   = LLViewerCamera::getInstance()->getOrigin();
                usePoint = true;
                break;
            case GAZE_POINT:
                if (!g.mPoint.isExactlyZero())
                {
                    target   = gAgent.getPosAgentFromGlobal(g.mPoint);
                    usePoint = true;
                }
                break;
            case GAZE_CAST:
            {
                LLVOAvatar* tgt = (g.mCastTarget.notNull() && g.mCastTarget != av->getID())
                    ? LLDirectorCast::instance().resolve(g.mCastTarget) : nullptr;
                if (tgt)
                {
                    // aim at the target's head (chest-ish); guard a rootless one
                    if (LLJoint* th = tgt->getJoint("mHead"))
                    {
                        target   = th->getWorldPosition();
                        usePoint = true;
                    }
                }
                break;
            }
            case GAZE_TANGENT:
            default:
                break;
        }

        if (usePoint)
        {
            dir = target - headPos;
            if (dir.magVecSquared() < GAZE_MIN_DIST * GAZE_MIN_DIST)
            {
                dir = tangentDir();     // target on top of the head -> fall back
            }
        }
        else
        {
            dir = tangentDir();
        }
        if (dir.magVecSquared() > 1e-6f)
        {
            dir.normVec();
            haveDir = true;
        }
    }

    // ---- smooth the direction (exp toward the target; reseed on activation) ----
    if (haveDir)
    {
        if (!g.mDirValid)
        {
            g.mSmoothDir = dir;
            g.mDirValid  = true;
        }
        else if (advance)
        {
            const F32 tau = GAZE_TAU_MIN + (GAZE_TAU_MAX - GAZE_TAU_MIN) * g.mSmoothing;
            const F32 a   = 1.f - expf(-dt / llmax(tau, 0.01f));
            g.mSmoothDir += (dir - g.mSmoothDir) * a;
            if (g.mSmoothDir.normVec() < 1e-4f)
            {
                g.mSmoothDir = dir;     // degenerate average -> snap to the target
            }
        }
    }
    if (!g.mDirValid)
    {
        return;                 // nothing to aim at yet -> leave the anim pose
    }

    // ---- solve + apply (LLHeadRotMotion + LLEyeMotion math, weighted) ---------
    // eased envelope * intensity = overall paint weight; the head/eyes blend gates
    // the body chain (torso/neck/head) so blend=0 is eyes-only.
    const F32 env_eased = g.mEnv * g.mEnv * (3.f - 2.f * g.mEnv);    // smoothstep
    const F32 env_i     = env_eased * g.mIntensity;
    const F32 wEye      = env_i;
    const F32 wBody     = env_i * g.mHeadEyeBlend;

    const LLQuaternion rootWorld = root->getWorldRotation();
    const LLQuaternion invRoot   = ~rootWorld;

    LLVector3 look = g.mSmoothDir;      // unit world direction

    // head aim in the root frame, with the built-in's degenerate guard: a target
    // directly overhead / underfoot makes left ~ 0, so lerp toward straight ahead.
    LLVector3 root_up = LLVector3(0.f, 0.f, 1.f) * rootWorld;
    LLVector3 left    = root_up % look;
    if (left.magVecSquared() < 0.15f)
    {
        LLVector3 root_at = LLVector3(1.f, 0.f, 0.f) * rootWorld;
        root_at.mV[VZ] = 0.f;
        root_at.normVec();
        look = lerp(look, root_at, 0.4f);
        look.normVec();
        left = root_up % look;
    }
    left.normVec();
    LLVector3 up = look % left;
    up.normVec();
    const LLQuaternion targetHeadWorld(look, left, up);
    LLQuaternion       head_rot_local = targetHeadWorld * invRoot;   // root-relative

    // clamp yaw AND pitch to human head+neck+torso capacity and zero the roll, so
    // a target behind the actor eases to the max and HOLDS there (no neck-wrap, no
    // snap) -- the "give up gracefully" behavior.
    {
        F32 roll = 0.f, pitch = 0.f, yaw = 0.f;
        head_rot_local.getEulerAngles(&roll, &pitch, &yaw);
        yaw   = llclamp(yaw,   -GAZE_HEAD_YAW_MAX,   GAZE_HEAD_YAW_MAX);
        pitch = llclamp(pitch, -GAZE_HEAD_PITCH_MAX, GAZE_HEAD_PITCH_MAX);
        head_rot_local.setEulerAngles(0.f, pitch, yaw);
    }

    if (wBody > 0.001f)
    {
        // torso takes a small share (large turns lean the chest). Root-relative
        // approximation as in the built-in; nlerp from the anim pose so the walk's
        // torso sway still reads at partial weight.
        if (LLJoint* torso = av->getJoint("mTorso"))
        {
            const LLQuaternion torso_target =
                nlerp(GAZE_TORSO_LAG, LLQuaternion::DEFAULT, head_rot_local);
            torso->setRotation(nlerp(wBody, torso->getRotation(), torso_target));
        }

        // neck + head split, converted into the neck's parent-local frame exactly
        // like LLHeadRotMotion so the chain composes correctly.
        LLJoint* neck = av->getJoint("mNeck");
        if (neck && neck->getParent())
        {
            const LLQuaternion torsoRotLocal = neck->getParent()->getWorldRotation() * invRoot;
            const LLQuaternion head_rel      = head_rot_local * ~torsoRotLocal;
            const LLQuaternion neck_target   = nlerp(GAZE_NECK_LAG,       LLQuaternion::DEFAULT, head_rel);
            const LLQuaternion head_target   = nlerp(1.f - GAZE_NECK_LAG, LLQuaternion::DEFAULT, head_rel);
            neck->setRotation(nlerp(wBody, neck->getRotation(), neck_target));
            head->setRotation(nlerp(wBody, head->getRotation(), head_target));
        }
    }

    // eyes: residual toward the target RELATIVE to the head's actual world rotation
    // (recomputed from the locals just set). Eyes carry whatever the head did not
    // -- the full angle when blend=0 (eyes-only), near zero when the head turned.
    if (wEye > 0.001f)
    {
        const LLQuaternion headWorld = head->getWorldRotation();
        auto applyEye = [&](LLJoint* eye)
        {
            if (!eye)
            {
                return;
            }
            const LLVector3 skyward(0.f, 0.f, 1.f);
            LLVector3 eleft = skyward % look;
            if (eleft.magVecSquared() < 1e-4f)
            {
                return;         // looking straight up/down: leave the eyes be
            }
            eleft.normVec();
            LLVector3 eup = look % eleft;
            eup.normVec();
            LLQuaternion tgt(look, eleft, eup);     // world
            tgt = tgt * ~headWorld;                 // head-local
            F32 roll = 0.f, pitch = 0.f, yaw = 0.f;
            tgt.getEulerAngles(&roll, &pitch, &yaw);
            yaw   = llclamp(yaw,   -GAZE_EYE_YAW_MAX,   GAZE_EYE_YAW_MAX);
            pitch = llclamp(pitch, -GAZE_EYE_PITCH_MAX, GAZE_EYE_PITCH_MAX);
            tgt.setEulerAngles(0.f, pitch, yaw);
            eye->setRotation(nlerp(wEye, eye->getRotation(), tgt));
        };
        applyEye(av->getJoint("mEyeLeft"));
        applyEye(av->getJoint("mEyeRight"));
        applyEye(av->getJoint("mFaceEyeAltLeft"));      // Bento alt eyes, if present
        applyEye(av->getJoint("mFaceEyeAltRight"));
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

// onion-skin FALLBACK: a humanoid SILHOUETTE at a ghost position so a director
// can read the blocking without pressing ACTION. A cheap stick figure -- spine,
// billboarded head, shoulders/arms, splayed legs -- plus a ground facing arrow,
// tinted to the actor. Lines/triangles only, in the same no-depth UI pass;
// deliberately NOT a skinned-mesh instance (too costly). Used only when a REAL
// translucent avatar impostor is unavailable (see drawImpostorGhost). Now drawn
// with a dark outline underlay + a readable body alpha so it reads over BOTH
// bright and dark ground. base = foot/ground point (agent frame), face =
// horizontal travel direction, height = figure height (m), bb_right/bb_up =
// camera billboard axes for the head ring.
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

    const LLVector3 hipC  = base + LLVector3(0.f, 0.f, hipZ);
    const LLVector3 shC   = base + LLVector3(0.f, 0.f, shZ);
    const LLVector3 shL   = shC - perp * halfsh;
    const LLVector3 shR   = shC + perp * halfsh;
    const LLVector3 hc    = base + LLVector3(0.f, 0.f, headZ);  // head centre
    const LLVector3 handL = base + perp * (halfsh * 1.15f) + LLVector3(0.f, 0.f, hipZ * 0.72f);
    const LLVector3 handR = base - perp * (halfsh * 1.15f) + LLVector3(0.f, 0.f, hipZ * 0.72f);
    const LLVector3 footL = base - perp * halfft;
    const LLVector3 footR = base + perp * halfft;

    // emit the whole stick figure (spine/shoulders/arms/legs + billboarded head
    // ring) as one LINES batch in the given color, at the current line width, so
    // it can be drawn twice: a wide dark underlay then the bright body.
    const S32 SEG = 10;
    auto emit_figure = [&](const LLColor4& col)
    {
        gGL.begin(LLRender::LINES);
        gGL.color4fv(col.mV);
        gGL.vertex3fv(hipC.mV); gGL.vertex3fv(shC.mV);      // spine
        gGL.vertex3fv(shL.mV);  gGL.vertex3fv(shR.mV);      // shoulders
        gGL.vertex3fv(shL.mV);  gGL.vertex3fv(handL.mV);    // arms
        gGL.vertex3fv(shR.mV);  gGL.vertex3fv(handR.mV);
        gGL.vertex3fv(hipC.mV); gGL.vertex3fv(footL.mV);    // legs
        gGL.vertex3fv(hipC.mV); gGL.vertex3fv(footR.mV);
        for (S32 s = 0; s < SEG; ++s)                       // head ring (billboard)
        {
            const F32 a = (F32)s       / SEG * F_TWO_PI;
            const F32 b = (F32)(s + 1) / SEG * F_TWO_PI;
            gGL.vertex3fv((hc + bb_right * (hr * cosf(a)) + bb_up * (hr * sinf(a))).mV);
            gGL.vertex3fv((hc + bb_right * (hr * cosf(b)) + bb_up * (hr * sinf(b))).mV);
        }
        gGL.end();
    };

    gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);   // lines, no texture
    // dark outline underlay (wider) so the figure reads over any ground ...
    gGL.setLineWidth(5.f);
    emit_figure(LLColor4(0.f, 0.f, 0.f, 0.5f));
    // ... then the readable body on top
    LLColor4 c = tint; c.mV[VW] = 0.55f;
    gGL.setLineWidth(2.f);
    emit_figure(c);

    // ground facing arrow (a touch stronger than the body so heading reads),
    // with its own dark outline so it never washes out over the ground/ribbon
    const F32 AL = 0.5f;
    const LLVector3 g   = base + LLVector3(0.f, 0.f, 0.02f);
    const LLVector3 tip = g + f * AL;
    const LLVector3 al  = g + f * (AL * 0.55f) + perp * (AL * 0.28f);
    const LLVector3 ar  = g + f * (AL * 0.55f) - perp * (AL * 0.28f);
    // outline: three thick dark edges around the arrowhead
    gGL.setLineWidth(4.f);
    gGL.begin(LLRender::LINES);
    gGL.color4f(0.f, 0.f, 0.f, 0.55f);
    gGL.vertex3fv(tip.mV); gGL.vertex3fv(al.mV);
    gGL.vertex3fv(al.mV);  gGL.vertex3fv(ar.mV);
    gGL.vertex3fv(ar.mV);  gGL.vertex3fv(tip.mV);
    gGL.end();
    // filled arrowhead
    LLColor4 ac = tint; ac.mV[VW] = 0.7f;
    gGL.begin(LLRender::TRIANGLES);
    gGL.color4fv(ac.mV);
    gGL.vertex3fv(tip.mV); gGL.vertex3fv(al.mV); gGL.vertex3fv(ar.mV);
    gGL.end();
}

// REAL avatar ghost: stamp the actor's cached impostor snapshot (a billboard
// image of the ACTUAL rendered avatar, produced by gPipeline.generateImpostor)
// as a translucent, camera-facing quad centred at `center` (agent frame). This
// mirrors LLVOAvatar::renderImpostor()'s billboard construction -- same
// mImpostorDim half-extents projected onto the camera left/up basis -- but at an
// arbitrary position and with a ghost alpha, so the director sees a translucent
// copy of the real avatar rather than a stick figure. Returns false (draws
// nothing) when the actor has no usable snapshot yet, so the caller falls back
// to drawPoseGhost(). CAVEATS (inherent to impostors, documented in the report):
// the card is a FROZEN 2D snapshot of the pose/appearance at capture and faces
// the camera flat, so orbiting far from the capture angle reveals the flatness
// until the snapshot is refreshed by updateGhostImpostors().
bool drawImpostorGhost(LLVOAvatar* av, const LLVector3& center,
                       const LLColor4& tint, F32 alpha)
{
    if (!av || !av->mImpostor.isComplete())
    {
        return false;
    }
    const LLVector2 dim = av->getImpostorDim();
    if (dim.mV[0] <= 0.001f || dim.mV[1] <= 0.001f)
    {
        return false;
    }
    LLViewerCamera* cam = LLViewerCamera::getInstance();
    LLVector3 at = center - cam->getOrigin();
    if (at.normalize() < 1e-4f)
    {
        return false;
    }
    LLVector3 left = cam->getUpAxis() % at;
    if (left.normalize() < 1e-4f)
    {
        return false;
    }
    LLVector3 up = at % left;
    up.normalize();
    left *= dim.mV[0];
    up   *= dim.mV[1];

    // translucent, camera-facing quad textured with the impostor snapshot. The
    // snapshot already carries a per-texel alpha silhouette (transparent
    // background), so alpha-blending it against a mostly-white tint*alpha color
    // yields a clean translucent avatar cutout. A faint pull toward the actor
    // hue keeps multiple actors' ghosts distinguishable without hiding the image.
    LLGLEnable blend(GL_BLEND);
    gGL.setSceneBlendType(LLRender::BT_ALPHA);
    gGL.getTexUnit(0)->bind(&av->mImpostor);
    const F32 t = 0.22f;
    gGL.color4f(1.f - t + tint.mV[VX] * t,
                1.f - t + tint.mV[VY] * t,
                1.f - t + tint.mV[VZ] * t,
                alpha);
    gGL.begin(LLRender::TRIANGLES);
    gGL.texCoord2f(0.f, 0.f); gGL.vertex3fv((center + left - up).mV);
    gGL.texCoord2f(1.f, 0.f); gGL.vertex3fv((center - left - up).mV);
    gGL.texCoord2f(1.f, 1.f); gGL.vertex3fv((center - left + up).mV);
    gGL.texCoord2f(0.f, 0.f); gGL.vertex3fv((center + left - up).mV);
    gGL.texCoord2f(1.f, 1.f); gGL.vertex3fv((center - left + up).mV);
    gGL.texCoord2f(0.f, 1.f); gGL.vertex3fv((center + left + up).mV);
    gGL.end();
    gGL.flush();
    gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
    return true;
}

// Ghost STYLES (PathGhostStyle) -- how the true 3D model ghost is shaded. Keep
// the ids in sync with panel_path_editor.xml's ghost_style_combo and the
// PathGhostStyle setting comment. Every style shares the same geometry path
// (snapshotted rigged batches, world-space palette, depth prime); only the
// colour pass differs.
enum EGhostStyle : S32
{
    GHOST_STYLE_GHOST     = 0,  // classic translucent flat tint
    GHOST_STYLE_CLONE     = 1,  // unlit textured copy (opaque)
    GHOST_STYLE_HOLOGRAM  = 2,  // animated cyan hologram (scanlines + rim + flicker)
    GHOST_STYLE_WIREFRAME = 3,  // hidden-line wireframe
    GHOST_STYLE_XRAY      = 4,  // rim-lit x-ray (hologram shader, rim-only params)
};

// custom uniforms of the actor-ghost FX shader (actorghostF.glsl); hashed once
static LLStaticHashedString sGhostTime("ghostTime");
static LLStaticHashedString sGhostParams("ghostParams");
static LLStaticHashedString sGhostAux("ghostAux");
static LLStaticHashedString sGhostFx("ghostFx");
static LLStaticHashedString sGhostSlot("ghostSlot");

// ---------------------------------------------------------------------------
// Per-batch alpha semantics: how does the REAL render treat this rigged pass's
// texels? Masked passes cutoff-discard in their fragment shaders; blend passes
// alpha-blend in the alpha pool. The ghost draw mirrors both (discard via the
// FX shader's ghostAux.x, blending via a separate sweep) instead of drawing
// everything opaque -- drawing a masked hair sheet or a sheer clothing layer
// solid is what produced the white halos / solid fringes on the styled clone.
bool ghost_pass_is_mask(U32 pass)
{
    switch (pass)
    {
    case LLRenderPass::PASS_ALPHA_MASK_RIGGED:
    case LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK_RIGGED:
    case LLRenderPass::PASS_MATERIAL_ALPHA_MASK_RIGGED:
    case LLRenderPass::PASS_SPECMAP_MASK_RIGGED:
    case LLRenderPass::PASS_NORMMAP_MASK_RIGGED:
    case LLRenderPass::PASS_NORMSPEC_MASK_RIGGED:
    case LLRenderPass::PASS_GLTF_PBR_ALPHA_MASK_RIGGED:
        return true;
    default:
        return false;
    }
}

bool ghost_pass_is_blend(U32 pass)
{
    switch (pass)
    {
    case LLRenderPass::PASS_ALPHA_RIGGED:
    case LLRenderPass::PASS_MATERIAL_ALPHA_RIGGED:
    case LLRenderPass::PASS_SPECMAP_BLEND_RIGGED:
    case LLRenderPass::PASS_NORMMAP_BLEND_RIGGED:
    case LLRenderPass::PASS_NORMSPEC_BLEND_RIGGED:
        return true;
    default:
        return false;
    }
}

// [R2-2] GLOW batches are DUPLICATES of base-pass geometry (a face with PBR
// emissive renders in its base pass AND again, additively, in the glow pass),
// so they are excluded from every sweep except the clone's dedicated additive
// emissive sweep -- otherwise the body would double-draw. Only the GLTF glow
// pass is collected: PBR emissive is real surface COLOUR (emissive map x
// emissive colour -- the missing iris on emissive-driven eyes); legacy
// PASS_GLOW_RIGGED is a bloom intensity whose per-vertex glow amount lives in
// the EMISSIVE vertex attribute this shader does not read, so honoring it
// faithfully is out of scope (its faces' base colour already draws via their
// base pass).
bool ghost_pass_is_glow(U32 pass)
{
    return pass == LLRenderPass::PASS_GLTF_GLOW_RIGGED;
}

// [R2-2] Indexed (multi-material) batches: how many slots does this batch
// carry? 1 = scalar (mGLTFMaterial / mTexture as usual). >1 = the vertex
// buffer's texture_index attribute selects the material per vertex, and a
// single bound texture is WRONG for every non-anchor slot (this is how eye
// materials merged into a head's indexed batch came out white) -- the clone
// redraws such a batch once per slot with the ghostSlot shader filter.
S32 ghost_batch_slot_count(LLDrawInfo* di)
{
    if (di->mGLTFMaterialList.size() > 1)  return (S32)di->mGLTFMaterialList.size();
    if (di->mMaterialSlotList.size() > 1)  return (S32)di->mMaterialSlotList.size();
    if (di->mTextureList.size() > 1)       return (S32)di->mTextureList.size();
    return 1;
}

// [R2-2] resolve one slot of an indexed batch for the clone: the slot's colour
// texture (emissive map instead when `emissive`), its mask cutoff (0 = keep
// all; only bites when the batch's pass masks), and its colour factor (GLTF
// base colour / emissive colour, linear -- caller gamma-approximates like the
// scalar path; white for legacy slots, whose tint rides vertex colours).
// Returns nullptr for a gap slot (fragmented batch) -- caller skips the pass.
LLViewerTexture* ghost_batch_slot_texture(LLDrawInfo* di, S32 slot, bool emissive,
                                          F32& out_cutoff, LLColor4& out_factor)
{
    out_cutoff = 0.f;
    out_factor = LLColor4::white;
    if (di->mGLTFMaterialList.size() > 1)
    {
        LLFetchedGLTFMaterial* m = ((size_t)slot < di->mGLTFMaterialList.size())
            ? di->mGLTFMaterialList[slot].get() : nullptr;
        if (!m)
        {
            return nullptr;     // gap left by a fragmented batch (never sampled)
        }
        if (m->mAlphaMode == LLGLTFMaterial::ALPHA_MODE_MASK)
        {
            out_cutoff = m->mAlphaCutoff;
        }
        if (emissive)
        {
            out_factor = LLColor4(m->mEmissiveColor.mV[0], m->mEmissiveColor.mV[1],
                                  m->mEmissiveColor.mV[2], 1.f);
            return m->mEmissiveTexture.get();   // null = flat emissive colour
        }
        out_factor = m->mBaseColor;
        return m->mBaseColorTexture.get();
    }
    if (di->mMaterialSlotList.size() > 1)
    {
        const LLDrawInfo::MaterialSlot& s = di->mMaterialSlotList[slot];
        out_cutoff = s.mAlphaMaskCutoff;    // caller gates on the pass masking
        return s.mDiffuse.get();
    }
    if ((size_t)slot < di->mTextureList.size())
    {
        return di->mTextureList[slot].get();
    }
    return nullptr;
}

// Resolve the texture that colours this batch the way the REAL render does.
// GLTF PBR batches carry no mTexture in the usual case -- their base colour
// map lives on the fetched material, and mTexture is only the media override,
// which wins when present (mirrors LLRenderPass::pushGLTFBatch handing
// params.mTexture into LLFetchedGLTFMaterial::bind). Legacy Blinn-Phong /
// diffuse batches use mTexture directly. May return null (no texture resolves,
// e.g. a multi-material indexed batch); the clone caller then shades mid-grey
// rather than white -- white reads as GLOW, grey reads as "untextured".
LLViewerTexture* ghost_batch_texture(LLDrawInfo* di)
{
    if (di->mGLTFMaterial.notNull())
    {
        if (di->mTexture.notNull())
        {
            return di->mTexture.get();      // media override on a PBR face
        }
        return di->mGLTFMaterial->mBaseColorTexture.get();
    }
    return di->mTexture.get();
}

// The batch's alpha-mask cutoff for the ghost shader's discard: the GLTF
// material's authored cutoff for PBR mask batches, the LLDrawInfo cutoff
// (already normalized 0..1 by registerFace) for legacy mask batches, and 0
// ("keep every texel") for everything else.
F32 ghost_batch_cutoff(LLDrawInfo* di, U32 pass)
{
    if (!ghost_pass_is_mask(pass))
    {
        return 0.f;
    }
    if (di->mGLTFMaterial.notNull())
    {
        return di->mGLTFMaterial->mAlphaCutoff;
    }
    return di->mAlphaMaskCutoff;
}

// Build the texture_matrix0 this batch needs so its UVs land where the real
// render puts them: the legacy SL texture-anim matrix (di->mTextureMatrix, the
// same one setup_texture_matrix() in lldrawpool.cpp uploads) composed with the
// GLTF KHR_texture_transform of the base colour map (which the PBR shaders
// apply from uniforms the highlight/ghost shaders do not have). The KHR
// transform runs in a flipped-Y (left-handed) UV frame; this is the closed
// form of textureUtilV.glsl's texture_transform() -- flip, offset*rot*scale,
// flip back -- collapsed into one affine, written in LLMatrix4's row-vector
// convention (v * M, which is what the shader's texture_matrix0 computes after
// the column-major reinterpretation on upload). Returns false when identity
// would do, so the common case skips the matrix load entirely.
// the KHR closed form alone (flip / offset*rot*scale / flip collapsed to one
// affine, row-vector convention); composed after `out`'s current contents.
// Split out so the indexed per-slot redraw can apply each SLOT's transform.
bool ghost_compose_khr_uv(const LLGLTFMaterial::TextureTransform& tt, LLMatrix4& out)
{
    if (tt.mOffset.mV[VX] == 0.f && tt.mOffset.mV[VY] == 0.f
        && tt.mScale.mV[VX] == 1.f && tt.mScale.mV[VY] == 1.f
        && tt.mRotation == 0.f)
    {
        return false;
    }
    const F32 c = cosf(tt.mRotation);
    const F32 s = sinf(tt.mRotation);
    const F32 sx = tt.mScale.mV[VX], sy = tt.mScale.mV[VY];
    const F32 ox = tt.mOffset.mV[VX], oy = tt.mOffset.mV[VY];
    // u' = c*sx*u - s*sy*v + (s*sy + ox)
    // v' = s*sx*u + c*sy*v + (1 - c*sy - oy)
    LLMatrix4 k;                        // identity-constructed
    k.mMatrix[0][0] = c * sx;
    k.mMatrix[0][1] = s * sx;
    k.mMatrix[1][0] = -s * sy;
    k.mMatrix[1][1] = c * sy;
    k.mMatrix[3][0] = s * sy + ox;
    k.mMatrix[3][1] = 1.f - c * sy - oy;
    out *= k;       // row-vector composition: prior transform first, then KHR
    return true;
}

bool ghost_batch_uv_matrix(LLDrawInfo* di, LLMatrix4& out)
{
    bool have = false;
    if (di->mTextureMatrix)
    {
        out = *di->mTextureMatrix;      // SL texture animation, applied FIRST
        have = true;
    }
    else
    {
        out.setIdentity();
    }
    if (di->mGLTFMaterial.notNull())
    {
        have |= ghost_compose_khr_uv(
            di->mGLTFMaterial->mTextureTransform[LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR],
            out);
    }
    return have;
}

// TRUE 3D ghost: re-render the actor's own worn rigged geometry as a styled
// copy standing on `foot` (agent frame), skinned from the actor's LIVE joint
// pose. Unlike drawImpostorGhost's flat card this is real three-dimensional
// geometry -- the actual mesh the avatar is wearing, re-skinned this frame and
// re-placed at the ghost spot -- so it orbits correctly and reads as a second
// body rather than a photo of one.
//
// The seam that makes this cheap: a rigged mesh's matrix palette
// (LLVOAvatar::updateSkinInfoMatrixPalette) already bakes every vertex into WORLD
// space (invBind * joint world matrix), so placing the ghost elsewhere is just a
// world-space translation premultiplied into the modelview -- no per-vertex work,
// no second skeleton. ONE shader covers every style when it compiled: the
// dedicated gActorGhostProgram (actorghostF.glsl) reduces exactly to the
// highlight look (color * texture) with its FX params zeroed, and adds what no
// stock interface shader has -- a per-batch alpha-mask DISCARD (ghostAux.x) and
// texture-alpha-aware blending, which is what keeps masked hair sheets / lace
// from drawing as solid halos. If it failed to compile, gHighlightProgram's
// rigged variant is the fallback: classic styles keep working, FX styles
// degrade to the classic ghost, masked batches draw unmasked (degraded, never
// blank). Per batch, upload the drawing avatar's live palette and draw the same
// vertex ranges the world pass drew -- but offset and re-shaded. (The drawing
// avatar is the batch's own mAvatar: for an animesh attachment that is the
// attachment's control avatar, whose world-space palette rides along under the
// same translation as the wearer's.)
//
// Two passes so the styled skin reads as ONE clean layer instead of showing its
// own backfaces through the front: pass 1 primes depth (colour masked off,
// masked texels discarded), pass 2 shades only where depth is equal (the
// front-most layer). The wireframe style polygon-offsets the prime slightly
// back so its LEQUAL line pass wins cleanly -- classic hidden-line removal.
//
// Styles (see EGhostStyle):
//   GHOST     -- alpha-blended flat tint, faintly pulled toward the actor hue.
//   CLONE     -- UNLIT textured copy: each batch re-binds its own resolved
//                colour map (legacy diffuse or PBR base colour -- see
//                ghost_batch_texture) and draws near-white * base-colour-factor
//                so the texture reads as authored. Opaque + masked batches draw
//                unblended (mask holes come from the shader discard); the
//                batches the real render alpha-BLENDS get their own blended
//                sweep over the primed body. This is a fullbright-style clone;
//                a scene-LIT clone needs deferred-pass integration (gbuffer +
//                light apply) and is future work.
//   HOLOGRAM  -- cyan tint, animated screen-space scanlines, fresnel-ish rim
//                boost, subtle time flicker.
//   WIREFRAME -- glPolygonMode(GL_LINE) over the offset depth prime: thin
//                hidden-line wireframe in a brighter actor tint (deliberately
//                unmasked: the wireframe wants the whole mesh's edges).
//   XRAY      -- rim-only params: body interior nearly clear, silhouette edges
//                glow (cheap creative bonus style).
//
// Returns the number of rigged batches drawn; 0 means this actor had no usable
// rigged geometry in view this frame, so the caller falls back to the impostor
// card / stick figure. SCOPE: covers WORN MESH (rigged attachments, including
// animesh attachments bucketed under the wearer) -- essentially the whole
// visible body of a modern mesh avatar. The legacy SYSTEM avatar body uses a
// different skinning path and is not drawn here (a pure system-avatar actor
// falls back). NOTE: uses the LIVE pose (a copy of the body as it stands right
// now); an independent, held ("out of sync") pose is the next step, and drops in
// by uploading a snapshotted palette here instead of the live one.
S32 drawGeometryGhost(LLVOAvatar* av, const std::vector<LLActorMover::GhostBatch>& batches,
                      const LLVector3& foot, const LLColor4& tint, F32 alpha,
                      S32 style,
                      const LLActorMover::GhostDrawParams& gp = LLActorMover::GhostDrawParams(),
                      const std::vector<LLActorMover::GhostStaticFace>* static_faces = nullptr)
{
    if (!av || av->isDead() || (batches.empty() && (!static_faces || static_faces->empty())))
    {
        return 0;
    }

    // shader choice + graceful degradation (see the header comment). The
    // rigged variant skins the batches; the BASE variant places the NON-RIGGED
    // attachment faces (collar/jewelry/flexi) through their own render matrix.
    LLGLSLShader* fx = gActorGhostProgram.mRiggedVariant;
    const bool have_fx = fx && fx->mProgramObject;
    if (!have_fx && (style == GHOST_STYLE_HOLOGRAM || style == GHOST_STYLE_XRAY))
    {
        style = GHOST_STYLE_GHOST;
    }
    LLGLSLShader* shader = have_fx ? fx : gHighlightProgram.mRiggedVariant;
    LLGLSLShader* static_shader = have_fx ? &gActorGhostProgram : &gHighlightProgram;
    if (!shader)
    {
        return 0;
    }
    if (!static_shader->mProgramObject)
    {
        static_faces = nullptr;     // base variant unavailable: rigged-only ghost
    }

    // Placement: move the whole (world-space-skinned) body from where it is
    // skinned to the ghost spot: T(ghost_foot) * Rz(yaw) * S(scale) *
    // T(-pivot_foot) premultiplied into the modelview. The pivot is the
    // actor's LIVE foot (root minus pelvisToFoot) -- or, for a FROZEN studio
    // instance, the capture-frame anchor that matches the frozen palettes.
    // Pivoting at the foot keeps rotated/scaled feet planted on `foot`; with
    // the default params (yaw 0, scale 1) this collapses to the classic pure
    // translation T(foot - live_foot).
    const LLVector3 live_root = av->getRenderPosition();
    const F32       p2f       = av->getPelvisToFoot();
    LLVector3 pivot = gp.mHavePivot
        ? gp.mPivotFootAgent
        : LLVector3(live_root.mV[VX], live_root.mV[VY], live_root.mV[VZ] - p2f);
    const F32 scale = llclamp(gp.mScale, 0.05f, 10.f);

    // ---- per-program setup, shared by the rigged and static sweeps -----------
    // frag_color = color * texture(diffuseMap): a white texture makes the output
    // exactly the `color` uniform (driven by gGL.diffuseColor4f); batches/faces
    // that need their own map (clone / masked / blended) re-bind per draw.
    // Uniforms are PER PROGRAM, so the style params are captured in locals and
    // (re)applied to whichever variant a sweep binds.
    LLColor4  style_color(1.f, 1.f, 1.f, alpha);    // set per style below
    LLVector4 style_params(0.f, 0.f, 0.f, 6.f);     // ghostParams per style
    const F32 ghost_now = (F32)LLFrameTimer::getElapsedSeconds();
    auto apply_program = [&](LLGLSLShader* sh)
    {
        sh->bind();
        if (have_fx)
        {
            // neutral alpha state (per-draw code retargets ghostAux.xy and
            // ghostSlot); the per-instance creative FX ride zw / ghostFx
            sh->uniform1f(sGhostTime, ghost_now);
            sh->uniform4fv(sGhostParams, 1, style_params.mV);
            sh->uniform4f(sGhostAux, 0.f, 0.f, gp.mPixelSize, gp.mPhase);
            sh->uniform4f(sGhostFx, gp.mShimmerSpeed, gp.mShimmerIntensity,
                          gp.mGlitch, llclamp(gp.mBrightness, 0.05f, 1.5f));
            sh->uniform1i(sGhostSlot, -1);
        }
        // [R2-4] park the diffuse_color GENERIC at white: buffers WITHOUT a
        // COLOR array (PBR) read the generic, whose GL boot default is BLACK
        // -- unparked, every PBR batch would shade black the moment the
        // shader gained the attribute. Buffers WITH the array (legacy faces,
        // carrying the baked TE tint + transparency) ignore the generic.
        // Generic state is global per attribute location; restored at exit.
        sh->vertexAttrib4f(LLVertexBuffer::TYPE_COLOR, 1.f, 1.f, 1.f, 1.f);
        gGL.diffuseColor4fv(style_color.mV);
        // texture_matrix0 could hold a stale value from an earlier frame's
        // sync to this program; start from a known-identity UV transform
        gGL.getTexUnit(0)->activate();
        gGL.matrixMode(LLRender::MM_TEXTURE);
        gGL.loadIdentity();
        gGL.matrixMode(LLRender::MM_MODELVIEW);
        gGL.getTexUnit(0)->bind(LLViewerFetchedTexture::sWhiteImagep);
    };
    apply_program(shader);

    gGL.pushMatrix();
    // modelview = view * T(ghost_foot) * Rz(yaw) * S(s) * T(-pivot)
    gGL.translatef(foot.mV[VX], foot.mV[VY], foot.mV[VZ]);
    if (gp.mYaw != 0.f)
    {
        gGL.rotatef(gp.mYaw * RAD_TO_DEG, 0.f, 0.f, 1.f);
    }
    if (scale != 1.f)
    {
        gGL.scalef(scale, scale, scale);
    }
    gGL.translatef(-pivot.mV[VX], -pivot.mV[VY], -pivot.mV[VZ]);
    gGL.syncMatrices();

    // [R2-5] CULL-FAITHFUL ghost: the overlay context (LLGLSUIDefault)
    // DISABLES backface culling, so before this fix every batch drew
    // double-sided -- single-sided geometry showed interior backfaces in the
    // silhouette prime, subtly wrong against the real render. Mirror the
    // world pipeline instead: cull as the baseline, per-batch/per-face
    // LLGLDisable for double-sided GLTF materials (the exact idiom of
    // lldrawpoolalpha.cpp:528/575/697 and pushGLTFBatch; legacy geometry is
    // single-sided in the real render too -- "two-sided" legacy content ships
    // flipped duplicate triangles, which the buffers already contain).
    LLGLEnable ghost_cull(GL_CULL_FACE);

    // ---- ONE parameterized sweep over the snapshotted batches ----------------
    // subset      -- SOLID (opaque + cutoff-masked), BLEND (the batches the real
    //                render alpha-blends), GLOW ([R2-2] the clone's additive
    //                emissive re-draws -- glow batches are DUPLICATE geometry,
    //                so every other subset excludes them), or ALL (solid+blend).
    // clone_tex   -- bind every batch's own resolved colour map + per-batch
    //                clone colour (the clone colour sweeps).
    // alpha_aware -- honor the real render's alpha semantics: masked batches
    //                bind their texture and cutoff-discard (ghostAux.x), blend
    //                batches bind their texture so its alpha shapes the blend.
    //                FX shader only -- the highlight fallback has no discard,
    //                so it keeps the pre-fix unmasked behavior.
    enum ESweep : S32 { SWEEP_SOLID, SWEEP_BLEND, SWEEP_GLOW, SWEEP_ALL };
    // texture-RGB mix for ghostAux.y: the clone wants the texture's colours,
    // every other style wants the flat tint even when a masked/blended batch
    // has its real texture bound for its ALPHA channel
    auto draw_batches = [&](S32 subset, bool clone_tex, bool alpha_aware)
    {
        const LLVOAvatar* lastAvatar = nullptr;
        U64  lastMeshId = 0;
        bool skipLastSkin = false;
        const F32 tex_mix = clone_tex ? 1.f : 0.f;
        F32  cur_cutoff = -1.f;     // force the first ghostAux upload per sweep
        bool tex_mat_on = false;    // a non-identity texture_matrix0 is loaded
        for (const LLActorMover::GhostBatch& gb : batches)
        {
            LLDrawInfo* di = gb.mInfo;
            const bool is_blend = ghost_pass_is_blend(gb.mPass);
            const bool is_glow  = ghost_pass_is_glow(gb.mPass);
            // glow duplicates base-pass geometry: only the glow sweep draws it
            if (is_glow != (subset == SWEEP_GLOW))
            {
                continue;
            }
            if ((subset == SWEEP_SOLID && is_blend)
                || (subset == SWEEP_BLEND && !is_blend))
            {
                continue;
            }

            // matrix palette: a FROZEN studio instance uploads the snapshot
            // captured at freeze time (per drawing-avatar + skin hash, so the
            // count matches this batch's skin by construction); a hash miss
            // falls back to the LIVE palette rather than dropping the batch.
            bool palette_ok = false;
            if (gp.mFrozenPalettes)
            {
                auto fit = gp.mFrozenPalettes->find(
                    std::make_pair(di->mAvatar->getID(), di->getSkinHash()));
                if (fit != gp.mFrozenPalettes->end() && !fit->second.empty())
                {
                    // same uniform the live path drives (AVATAR_MATRIX, GL-ready
                    // 3x4 floats, 12 per joint)
                    shader->uniformMatrix3x4fv(LLViewerShaderMgr::AVATAR_MATRIX,
                                               (U32)(fit->second.size() / 12),
                                               false, fit->second.data());
                    // poison the live-upload cache so a following live batch
                    // re-uploads instead of "already bound" skipping
                    lastAvatar = nullptr;
                    lastMeshId = 0;
                    palette_ok = true;
                }
            }
            if (!palette_ok
                && !LLRenderPass::uploadMatrixPalette(di->mAvatar, di->mSkinInfo,
                                                      lastAvatar, lastMeshId, skipLastSkin))
            {
                continue;
            }

            const bool is_mask = ghost_pass_is_mask(gb.mPass);

            // [R2-5] double-sided GLTF unculls for this batch, exactly like
            // the real render; indexed batches uncull when ANY slot is
            // double-sided (pushGLTFBatchIndexed's OR-over-slots rule)
            bool two_sided = di->mGLTFMaterial.notNull() && di->mGLTFMaterial->mDoubleSided;
            for (size_t ds = 0; !two_sided && ds < di->mGLTFMaterialList.size(); ++ds)
            {
                two_sided = di->mGLTFMaterialList[ds].notNull()
                         && di->mGLTFMaterialList[ds]->mDoubleSided;
            }
            LLGLDisable no_cull(two_sided ? GL_CULL_FACE : 0);

            // [R2-2] indexed multi-material batches: the vertex buffer's
            // texture_index attribute picks the material per vertex, so ONE
            // bound texture is wrong for every non-anchor slot (white eyes).
            // Where the slot data matters -- clone colour sweeps, and masked
            // batches whose cutoffs differ per slot -- redraw the batch once
            // per slot with the ghostSlot shader filter; flat tints that never
            // sample texture RGB keep the single unfiltered draw.
            const S32 slot_count = ghost_batch_slot_count(di);
            const bool per_slot = have_fx && slot_count > 1
                && (clone_tex || (alpha_aware && is_mask))
                && di->mVertexBuffer->hasDataType(LLVertexBuffer::TYPE_TEXTURE_INDEX);
            const S32 draws = per_slot ? slot_count : 1;

            for (S32 s = 0; s < draws; ++s)
            {
                // ---- resolve this draw's texture + colour factor + cutoff ----
                LLViewerTexture* tex = nullptr;
                F32 cutoff = 0.f;
                LLColor4 factor(1.f, 1.f, 1.f, 1.f);
                bool have_factor = false;
                const LLGLTFMaterial::TextureTransform* khr = nullptr;

                if (per_slot)
                {
                    tex = ghost_batch_slot_texture(di, s, subset == SWEEP_GLOW,
                                                   cutoff, factor);
                    const bool gltf_slots = di->mGLTFMaterialList.size() > 1;
                    if (gltf_slots
                        && ((size_t)s >= di->mGLTFMaterialList.size()
                            || di->mGLTFMaterialList[s].isNull()))
                    {
                        continue;   // gap slot of a fragmented batch: never sampled
                    }
                    have_factor = gltf_slots;
                    if (!(alpha_aware && is_mask))
                    {
                        cutoff = 0.f;   // legacy slots carry a cutoff even unmasked
                    }
                    if (gltf_slots)
                    {
                        khr = &di->mGLTFMaterialList[s]->mTextureTransform[
                            subset == SWEEP_GLOW
                                ? LLGLTFMaterial::GLTF_TEXTURE_INFO_EMISSIVE
                                : LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR];
                    }
                }
                else if (subset == SWEEP_GLOW)
                {
                    // scalar GLTF glow: emissive map x emissive colour (the
                    // additive sweep only ever collects PASS_GLTF_GLOW_RIGGED)
                    if (di->mGLTFMaterial.isNull())
                    {
                        continue;
                    }
                    const LLColor3& e = di->mGLTFMaterial->mEmissiveColor;
                    factor = LLColor4(e.mV[0], e.mV[1], e.mV[2], 1.f);
                    have_factor = true;
                    tex = di->mGLTFMaterial->mEmissiveTexture.get();
                    if (!tex && e.mV[0] + e.mV[1] + e.mV[2] < 0.01f)
                    {
                        continue;   // black flat emissive adds nothing
                    }
                    khr = &di->mGLTFMaterial->mTextureTransform[
                        LLGLTFMaterial::GLTF_TEXTURE_INFO_EMISSIVE];
                }
                else
                {
                    // scalar path (the common case, unchanged semantics)
                    if (clone_tex || (alpha_aware && have_fx && (is_mask || is_blend)))
                    {
                        tex = ghost_batch_texture(di);
                    }
                    cutoff = alpha_aware ? ghost_batch_cutoff(di, gb.mPass) : 0.f;
                    if (di->mGLTFMaterial.notNull())
                    {
                        factor = di->mGLTFMaterial->mBaseColor;
                        have_factor = true;
                    }
                }

                gGL.getTexUnit(0)->bind(
                    tex ? tex : (LLViewerTexture*)LLViewerFetchedTexture::sWhiteImagep);

                if (clone_tex)
                {
                    // per-draw clone colour: near-white so the texture reads
                    // as-is; an unresolvable texture shades MID-GREY (white
                    // reads as glow); a GLTF base-colour / emissive factor
                    // tints as authored (linear pushed to gamma space --
                    // documented approximation for an unlit clone).
                    F32 r = 0.98f, g = 0.98f, b = 0.98f, a = 1.f;
                    if (!tex && subset != SWEEP_GLOW
                        && !(di->mVertexBuffer->getTypeMask() & LLVertexBuffer::MAP_COLOR))
                    {
                        // [R3] see the static sweep: grey only a batch with NO
                        // colour at all, never one carrying a per-vertex TE
                        // tint -- greying a tinted face halves the authored
                        // colour, which is why colour-only rigged surfaces read
                        // as missing in a dark set.
                        r = g = b = 0.5f;
                    }
                    if (have_factor)
                    {
                        r *= powf(llmax(factor.mV[0], 0.f), 0.4545f);
                        g *= powf(llmax(factor.mV[1], 0.f), 0.4545f);
                        b *= powf(llmax(factor.mV[2], 0.f), 0.4545f);
                        if (subset != SWEEP_GLOW)
                        {
                            a = factor.mV[3];   // factor alpha shapes the blended sweep
                        }
                    }
                    // Ghost Studio's custom hue applies to the clone too (the fix
                    // for "the hue slider does nothing in Clone"): a deliberate art
                    // direction beats texture fidelity. Brightness-normalized so
                    // the tint shifts hue without dimming the body; the default
                    // identity tint stays ignored so a stock clone matches the
                    // avatar exactly.
                    if (gp.mTintCustom)
                    {
                        const F32 mx = llmax(llmax(tint.mV[0], tint.mV[1]),
                                             llmax(tint.mV[2], 0.001f));
                        const F32 st = 0.65f;   // mix strength: clearly tinted, still textured
                        r *= 1.f - st + st * (tint.mV[0] / mx);
                        g *= 1.f - st + st * (tint.mV[1] / mx);
                        b *= 1.f - st + st * (tint.mV[2] / mx);
                    }
                    gGL.diffuseColor4f(r, g, b, a);
                }

                if (have_fx)
                {
                    // per-draw cutoff (0 keeps everything); re-upload only on
                    // change, always carrying the instance FX in zw along
                    if (cutoff != cur_cutoff)
                    {
                        shader->uniform4f(sGhostAux, cutoff, tex_mix, gp.mPixelSize, gp.mPhase);
                        cur_cutoff = cutoff;
                    }
                    // slot filter: exact slot for per-slot re-draws, -1 = off
                    shader->uniform1i(sGhostSlot, per_slot ? s : -1);
                }

                // per-draw UV transform; identity restored when unused so a
                // transformed draw never bleeds its UVs into the next. The
                // scalar path composes anim + base KHR as before; slot / glow
                // draws compose the slot's (or emissive's) own KHR transform.
                LLMatrix4 uvm;
                bool have_uv = false;
                if (tex)
                {
                    if (per_slot || subset == SWEEP_GLOW)
                    {
                        if (di->mTextureMatrix)
                        {
                            uvm = *di->mTextureMatrix;
                            have_uv = true;
                        }
                        if (khr)
                        {
                            have_uv |= ghost_compose_khr_uv(*khr, uvm);
                        }
                    }
                    else
                    {
                        have_uv = ghost_batch_uv_matrix(di, uvm);
                    }
                }
                if (have_uv)
                {
                    gGL.getTexUnit(0)->activate();
                    gGL.matrixMode(LLRender::MM_TEXTURE);
                    gGL.loadMatrix((GLfloat*)uvm.mMatrix);
                    gGL.matrixMode(LLRender::MM_MODELVIEW);
                    tex_mat_on = true;
                }
                else if (tex_mat_on)
                {
                    gGL.matrixMode(LLRender::MM_TEXTURE);
                    gGL.loadIdentity();
                    gGL.matrixMode(LLRender::MM_MODELVIEW);
                    tex_mat_on = false;
                }

                di->mVertexBuffer->setBuffer();
                di->mVertexBuffer->drawRange(LLRender::TRIANGLES,
                                             di->mStart, di->mEnd, di->mCount, di->mOffset);
            }
        }
        if (tex_mat_on)
        {   // leave the UV transform clean for the next sweep / caller
            gGL.matrixMode(LLRender::MM_TEXTURE);
            gGL.loadIdentity();
            gGL.matrixMode(LLRender::MM_MODELVIEW);
        }
        if (have_fx)
        {
            shader->uniform1i(sGhostSlot, -1);      // never leak the slot filter
        }
    };

    // ---- [R2-2] the NON-RIGGED worn-attachment sweep -------------------------
    // Same subset semantics as draw_batches, drawn with the BASE shader variant:
    // each face renders through the shared ghost placement COMPOSED with its own
    // render matrix (view * T(foot)*Rz*S*T(-pivot) * M_face), so a collar rides
    // its joint exactly as worn. A FROZEN instance swaps in the object matrix
    // captured at freeze (capture-frame, consistent with the palettes); a miss
    // keeps the LIVE matrix -- flexi is always live (its verts are CPU-deformed
    // in the shared buffer every frame; documented limitation). Legacy editor
    // tints ride the vertex colours (R2-4), not a per-face constant.
    auto draw_static = [&](S32 subset, bool clone_tex, bool alpha_aware)
    {
        if (!static_faces || static_faces->empty() || subset == SWEEP_GLOW)
        {
            return;
        }
        apply_program(static_shader);
        for (const LLActorMover::GhostStaticFace& gf : *static_faces)
        {
            const bool is_blend = gf.mAlphaKind == 2;
            if ((subset == SWEEP_SOLID && is_blend)
                || (subset == SWEEP_BLEND && !is_blend))
            {
                continue;
            }
            LLFace* face = gf.mFace;
            LLVertexBuffer* vb = face ? face->getVertexBuffer() : nullptr;
            if (!vb)
            {
                continue;
            }

            // [R2-5] cull-faithful: double-sided GLTF unculls per face
            LLGLDisable no_cull(gf.mDoubleSided ? GL_CULL_FACE : 0);

            // texture + clone colour, mirroring the rigged sweep's semantics
            LLViewerTexture* ftex = nullptr;
            const LLTextureEntry* te = face->getTextureEntry();
            LLGLTFMaterial* gmat = te ? te->getGLTFRenderMaterial() : nullptr;
            if (clone_tex || (alpha_aware && have_fx && gf.mAlphaKind != 0))
            {
                // [R3] Mirror ghost_batch_texture's rule for rigged batches. On
                // a PBR face the colour map is the MATERIAL's base-colour
                // texture; LLFace::getTexture() defaults to the DIFFUSE channel,
                // which a GLTF face does not populate -- so every PBR static
                // attachment resolved to "no texture", fell back to white +
                // mid-grey, and read as "unrigged mesh textures not loading" on
                // the clone. Diffuse still wins when there is no GLTF material
                // (legacy faces) or no base-colour map (media override case).
                // getGLTFRenderMaterial() hands back the BASE type; the resolved
                // textures live on the FETCHED subclass, which is what a render
                // material always is in practice (same assumption the tree
                // asserts, e.g. llviewerobject.cpp:5166).
                if (const LLFetchedGLTFMaterial* fmat =
                        dynamic_cast<const LLFetchedGLTFMaterial*>(gmat))
                {
                    ftex = fmat->mBaseColorTexture.get();
                }
                if (!ftex)
                {
                    ftex = face->getTexture();
                }
            }
            gGL.getTexUnit(0)->bind(
                ftex ? ftex : (LLViewerTexture*)LLViewerFetchedTexture::sWhiteImagep);
            if (clone_tex)
            {
                F32 r = 0.98f, g = 0.98f, b = 0.98f, a = 1.f;
                if (!ftex && !(vb->getTypeMask() & LLVertexBuffer::MAP_COLOR))
                {
                    // [R3] mid-grey says "untextured" -- but ONLY for a face
                    // with no colour information at all. A face carrying a
                    // per-vertex TE tint has real authored colour, and greying
                    // it HALVES that tint (the untextured fallback stacking on
                    // the R2-4 vertex-colour multiply is why colour-only
                    // surfaces read as missing in a dark set). Tinted faces
                    // keep the near-white base so tint x white == authored.
                    r = g = b = 0.5f;
                }
                if (gmat)
                {
                    const LLColor4& f = gmat->mBaseColor;
                    r *= powf(llmax(f.mV[0], 0.f), 0.4545f);
                    g *= powf(llmax(f.mV[1], 0.f), 0.4545f);
                    b *= powf(llmax(f.mV[2], 0.f), 0.4545f);
                    a  = f.mV[3];
                }
                if (gp.mTintCustom)
                {
                    const F32 mx = llmax(llmax(tint.mV[0], tint.mV[1]),
                                         llmax(tint.mV[2], 0.001f));
                    const F32 st = 0.65f;
                    r *= 1.f - st + st * (tint.mV[0] / mx);
                    g *= 1.f - st + st * (tint.mV[1] / mx);
                    b *= 1.f - st + st * (tint.mV[2] / mx);
                }
                gGL.diffuseColor4f(r, g, b, a);
            }
            if (have_fx)
            {
                static_shader->uniform4f(sGhostAux,
                    (alpha_aware && gf.mAlphaKind == 1) ? gf.mCutoff : 0.f,
                    clone_tex ? 1.f : 0.f, gp.mPixelSize, gp.mPhase);
            }
            // per-face UV transform (SL texture animation / GLTF KHR)
            {
                LLMatrix4 uvm;
                bool have_uv = false;
                if (ftex)
                {
                    if (face->mTextureMatrix)
                    {
                        uvm = *face->mTextureMatrix;
                        have_uv = true;
                    }
                    if (gmat)
                    {
                        have_uv |= ghost_compose_khr_uv(
                            gmat->mTextureTransform[LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR],
                            uvm);
                    }
                }
                gGL.getTexUnit(0)->activate();
                gGL.matrixMode(LLRender::MM_TEXTURE);
                if (have_uv)
                {
                    gGL.loadMatrix((GLfloat*)uvm.mMatrix);
                }
                else
                {
                    gGL.loadIdentity();
                }
                gGL.matrixMode(LLRender::MM_MODELVIEW);
            }

            // placement * this face's model matrix (frozen capture wins)
            gGL.pushMatrix();
            bool used_frozen = false;
            if (gp.mFrozenAttachMats)
            {
                auto fit = gp.mFrozenAttachMats->find(gf.mObjectId);
                if (fit != gp.mFrozenAttachMats->end())
                {
                    gGL.multMatrix((GLfloat*)fit->second.mMatrix);
                    used_frozen = true;
                }
            }
            if (!used_frozen)
            {
                gGL.multMatrix((GLfloat*)face->getRenderMatrix().mMatrix);
            }
            gGL.syncMatrices();

            vb->setBuffer();
            vb->drawRange(LLRender::TRIANGLES, face->getGeomIndex(),
                          face->getGeomIndex() + face->getGeomCount() - 1,
                          face->getIndicesCount(), face->getIndicesStart());
            gGL.popMatrix();
        }
        gGL.syncMatrices();
        // hand the GL back to the rigged program for the next sweep
        apply_program(shader);
    };

    // --- pass 1: prime depth only (single-layer silhouette), no colour ---
    // Masked batches cutoff-discard here too, so the depth silhouette matches
    // the real body -- no more solid halos around hair sheets / lace. The clone
    // excludes BLENDED layers from the prime so the body under a sheer skirt
    // still shades (they get their own blended sweep below); the flat-tint
    // styles keep them (a translucent layer still contributes silhouette to a
    // flat ghost). Wireframe stays fully unmasked: hidden-line wants the whole
    // mesh's edges, holes included. [R2-2] the non-rigged attachment faces
    // prime alongside the batches so collar and body occlude each other right.
    {
        LLGLDepthTest depth(GL_TRUE, GL_TRUE, GL_LESS);
        LLGLDisable   blend(GL_BLEND);
        // wireframe: push the fill prime slightly back so the LEQUAL line pass
        // below wins depth without z-fighting (standard hidden-line removal)
        LLGLEnable offset(style == GHOST_STYLE_WIREFRAME ? GL_POLYGON_OFFSET_FILL : 0);
        if (style == GHOST_STYLE_WIREFRAME)
        {
            glPolygonOffset(1.f, 1.f);
        }
        gGL.setColorMask(false, false);
        if (style == GHOST_STYLE_WIREFRAME)
        {
            draw_batches(SWEEP_ALL, false, false);
            draw_static(SWEEP_ALL, false, false);
        }
        else
        {
            const S32 prime_subset = (style == GHOST_STYLE_CLONE) ? SWEEP_SOLID : SWEEP_ALL;
            draw_batches(prime_subset, false, true);
            draw_static(prime_subset, false, true);
        }
        if (style == GHOST_STYLE_WIREFRAME)
        {
            glPolygonOffset(0.f, 0.f);
        }
    }

    // --- pass 2: shade the primed front layer, per style ---
    // each style sets style_color / style_params FIRST (apply_program pushes
    // them to whichever shader variant a sweep binds), then runs its sweeps
    switch (style)
    {
    case GHOST_STYLE_CLONE:
    {
        // unlit textured copy, three sweeps. Sweep 1: opaque + masked batches,
        // no blending (mask holes come from the shader discard). Sweep 2: the
        // batches the real render alpha-BLENDS, blended over the primed body
        // -- depth-tested against the prime, so a layer behind the body stays
        // hidden and a sheer layer in front shades over it (layer-vs-layer
        // order is snapshot order, not a depth sort). Sweep 3 [R2-2]: ADDITIVE
        // emissive -- PBR faces whose visible colour lives in the emissive map
        // (stylized eyes) get it back; black emissive adds nothing.
        {
            LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_LEQUAL);
            LLGLDisable   blend(GL_BLEND);
            gGL.setColorMask(true, true);
            draw_batches(SWEEP_SOLID, true, true);
            draw_static(SWEEP_SOLID, true, true);
        }
        {
            LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_LEQUAL);
            LLGLEnable    blend(GL_BLEND);
            gGL.setSceneBlendType(LLRender::BT_ALPHA);
            draw_batches(SWEEP_BLEND, true, true);
            draw_static(SWEEP_BLEND, true, true);
        }
        {
            LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_LEQUAL);
            LLGLEnable    blend(GL_BLEND);
            gGL.setSceneBlendType(LLRender::BT_ADD);
            draw_batches(SWEEP_GLOW, true, true);
            gGL.setSceneBlendType(LLRender::BT_ALPHA);  // leave standard state
        }
        break;
    }
    case GHOST_STYLE_WIREFRAME:
    {
        // thin hidden-line wireframe in a brighter tint (pulled further toward
        // white than the classic ghost, and more opaque, so 1px lines read)
        LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_LEQUAL);
        LLGLEnable    blend(GL_BLEND);
        gGL.setSceneBlendType(LLRender::BT_ALPHA);
        gGL.setColorMask(true, true);
        const F32 t = 0.55f;
        style_color.set(tint.mV[VX] * (1.f - t) + t,
                        tint.mV[VY] * (1.f - t) + t,
                        tint.mV[VZ] * (1.f - t) + t,
                        llmin(1.f, alpha * 1.5f));
        gGL.diffuseColor4fv(style_color.mV);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        draw_batches(SWEEP_ALL, false, false);
        draw_static(SWEEP_ALL, false, false);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        break;
    }
    case GHOST_STYLE_HOLOGRAM:
    case GHOST_STYLE_XRAY:
    {
        // FX shader: colour carries the tint + base alpha; ghostParams shape
        // the look (x scanlines, y rim, z flicker, w scanline period px)
        LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_LEQUAL);
        LLGLEnable    blend(GL_BLEND);
        gGL.setSceneBlendType(LLRender::BT_ALPHA);
        gGL.setColorMask(true, true);
        if (style == GHOST_STYLE_HOLOGRAM)
        {
            // classic sci-fi cyan, faintly pulled toward the actor hue so
            // overlapping actors' holograms stay tellable-apart
            const F32 t = 0.25f;
            style_color.set(0.25f * (1.f - t) + tint.mV[VX] * t,
                            0.85f * (1.f - t) + tint.mV[VY] * t,
                            1.00f * (1.f - t) + tint.mV[VZ] * t,
                            alpha);
            style_params = LLVector4(1.f, 0.8f, 1.f, 6.f);
        }
        else
        {
            // x-ray: rim-only (no scanlines/flicker) over a faint cool body --
            // interior nearly clear, silhouette edges glow in the actor hue
            const F32 t = 0.35f;
            style_color.set(0.55f * (1.f - t) + tint.mV[VX] * t,
                            0.75f * (1.f - t) + tint.mV[VY] * t,
                            1.00f * (1.f - t) + tint.mV[VZ] * t,
                            alpha * 0.4f);
            style_params = LLVector4(0.f, 2.2f, 0.f, 6.f);
        }
        gGL.diffuseColor4fv(style_color.mV);
        shader->uniform4fv(sGhostParams, 1, style_params.mV);
        draw_batches(SWEEP_ALL, false, true);
        draw_static(SWEEP_ALL, false, true);
        break;
    }
    case GHOST_STYLE_GHOST:
    default:
    {
        // classic: blend the flat tint only on the primed front layer --
        // mostly white, with a faint pull toward the actor hue so overlapping
        // actors' ghosts stay distinguishable without hiding the body shape
        LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_LEQUAL);
        LLGLEnable    blend(GL_BLEND);
        gGL.setSceneBlendType(LLRender::BT_ALPHA);
        gGL.setColorMask(true, true);
        const F32 t = 0.25f;
        style_color.set(1.f - t + tint.mV[VX] * t,
                        1.f - t + tint.mV[VY] * t,
                        1.f - t + tint.mV[VZ] * t,
                        alpha);
        gGL.diffuseColor4fv(style_color.mV);
        draw_batches(SWEEP_ALL, false, true);
        draw_static(SWEEP_ALL, false, true);
        break;
    }
    }

    gGL.popMatrix();
    gGL.syncMatrices();
    gGL.setColorMask(true, true);
    gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
    // [R2-4] restore the diffuse_color generic to the GL boot default so no
    // later pass inherits our white park (generic state is global)
    shader->vertexAttrib4f(LLVertexBuffer::TYPE_COLOR, 0.f, 0.f, 0.f, 1.f);
    shader->unbind();   // draw_static always hands back to the rigged program
    return (S32)(batches.size() + (static_faces ? static_faces->size() : 0));
}
} // anonymous namespace

// ---------------------------------------------------------------------------
void LLActorMover::renderHeadingPreview()
{
    // (Ghost Studio instances do NOT draw here: they are scene dressing, not
    // an editing indicator, so render_ui_3d() calls renderStudioGhosts()
    // OUTSIDE the beacon/UI-visibility gate -- studio ghosts stay on screen
    // while actually filming with the UI hidden.)
    static LLCachedControl<bool> show(gSavedSettings, "ActorMoverShowHeading", true);
    if (!show)
    {
        return;
    }
    // an operator floater must be up: the standalone mover, the Director
    // Console (its Move tab drives the same heading/distance settings), or the
    // Prop Mover (whose enrolled object paths draw in this same pass below)
    bool operator_open = false;
    for (const char* name : { "actor_mover", "director", "prop_mover" })
    {
        LLFloater* floaterp = LLFloaterReg::findInstance(name);
        if (floaterp && floaterp->getVisible())
        {
            operator_open = true;
            break;
        }
    }
    if (!operator_open)
    {
        return;
    }

    static LLCachedControl<F32> distance(gSavedSettings, "ActorMoverDistance", 6.f);
    static LLCachedControl<F32> heading(gSavedSettings, "ActorMoverHeading", 0.f);
    // pose/blocking ghosts: styled copies of the actor at every path node.
    // Opt-in, default off, so the overlay pass adds nothing per frame unless
    // the director asks for it. Renderer precedence: the true 3D model ghost
    // (PathGhostUseModel, default) wins; the impostor billboard
    // (PathGhostUseImpostor) and the stick figure are fallbacks for actors the
    // model ghost can't cover (no rigged geometry) or when the model ghost is
    // switched off. PathGhostStyle picks the model ghost's LOOK (ghost / clone /
    // hologram / wireframe / x-ray, see EGhostStyle); the fallbacks are
    // unaffected by it.
    static LLCachedControl<bool> onion(gSavedSettings, "PathShowOnionSkin", false);
    static LLCachedControl<bool> use_model(gSavedSettings, "PathGhostUseModel", true);
    static LLCachedControl<bool> use_impostor(gSavedSettings, "PathGhostUseImpostor", true);
    static LLCachedControl<S32>  ghost_style(gSavedSettings, "PathGhostStyle", 0);
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

    // Ghost billboards are collected across the whole roster and drawn LAST, in a
    // single far-to-near pass, so the translucent avatar cards blend correctly
    // against each other (the UI 3D pass runs on a cleared depth buffer, so there
    // is no scene depth to sort against -- painter order is what we control) and
    // do not interleave with the opaque line/ribbon viz. Only populated when the
    // ghost toggle is on, so the default-off path allocates nothing.
    struct GhostItem
    {
        LLVOAvatar* mAv;
        LLVector3   mFoot;      // ground point, agent frame
        LLVector3   mFace;      // horizontal travel direction (stick fallback)
        F32         mHeight;    // figure height for the stick fallback, m
        LLColor4    mTint;      // actor hue
    };
    std::vector<GhostItem> ghosts;

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
                // the final node / the seam for a loop). Collected for the sorted
                // ghost pass below (real impostor billboard, or stick fallback).
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
                    ghosts.push_back({ av, base, face, gh, col_mid });
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

            // NOTE: no pose ghosts in the no-path case. Previously this stamped a
            // ghost at the CURRENT position AND the projected DESTINATION for every
            // roster/cast actor, which (a) put a translucent double right on top of
            // each idle actor's live body and (b) drew a speculative destination
            // ghost from the shared heading/distance on cast members that had no
            // path at all -- the "multiple ghosts despite no path" the director saw.
            // Ghosts now appear only where they mean something: at the nodes of an
            // actual walkable path (the branch above). Opt-in blocking ghosts for an
            // idle actor return with the per-actor standalone-ghost feature.

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

    // ---- enrolled PROP paths (object path mover) ------------------------------
    // Same ribbon + chevrons + numbered nodes as an actor path, per-prop tinted
    // (actorPathColor hashes any UUID), drawn AT the authored node heights --
    // object paths are authored at the prop's own root height (a car's axle),
    // so no ground lift is wanted. This is the visual feedback that makes the
    // Prop Mover legible: enroll a prop, drop nodes, SEE the route.
    for (const LLUUID& prop_id : ALObjectPathMover::instance().getRoster())
    {
        auto pit = mPaths.find(prop_id);
        if (pit == mPaths.end() || pit->second.mNodes.size() < 2)
        {
            continue;
        }
        Path& path = pit->second;
        if (path.mDirty)
        {
            path.rebuild();
        }

        LLColor4 col = actorPathColor(prop_id);
        col.mV[VW] = 0.9f;
        LLColor4 col_chev = blend(col, LLColor4(1.f, 1.f, 1.f, 1.f), 0.45f);
        col_chev.mV[VW] = 0.95f;

        const F32 total = llmax(path.mTotalLength, 0.01f);
        const S32 steps = llclamp((S32)ceilf(total / 0.35f), 1, 4096);
        std::vector<LLVector3> pts;
        pts.reserve(steps + 1);
        for (S32 i = 0; i <= steps; ++i)
        {
            LLVector3d gp, gt;
            path.evalAtDistance(total * (F32)i / (F32)steps, gp, gt);
            pts.push_back(gAgent.getPosAgentFromGlobal(gp));
        }
        drawThickLine(pts, 0.09f, col);
        drawChevrons(pts, col_chev);

        for (S32 i = 0; i < (S32)path.mNodes.size(); ++i)
        {
            const LLVector3 base = gAgent.getPosAgentFromGlobal(path.mNodes[i].mPosGlobal);
            drawNodeMarker(base, col, false);
            LLVector3 num_at = base;
            num_at.mV[VZ] += NODE_STICK_H + 0.06f;
            drawNumber(i + 1, num_at, NODE_NUM_H, bb_right, bb_up, col_num);
        }
    }

    // ---- ghost pass: draw every collected pose ghost far-to-near --------------
    // Preference order per actor: a TRUE 3D model ghost (drawGeometryGhost -- the
    // actor's own worn mesh re-skinned and re-placed, depth-tested so it reads as
    // a real translucent body), else a real avatar impostor billboard (drawImpostor-
    // Ghost), else the readable stick figure (drawPoseGhost). Sorted far-to-near so
    // the translucent draws stack correctly. NOTE (documented caveat): the ghost
    // pass runs on a cleared depth buffer, so world geometry does NOT occlude the
    // ghosts -- they read as a client overlay ON TOP of the scene (each model
    // ghost still self-occludes cleanly via its own depth prime).
    if (!ghosts.empty())
    {
        const LLVector3 cam_pos = cam->getOrigin();
        std::sort(ghosts.begin(), ghosts.end(),
                  [&](const GhostItem& a, const GhostItem& b)
                  {
                      return (a.mFoot - cam_pos).magVecSquared()
                           > (b.mFoot - cam_pos).magVecSquared();
                  });
        const F32 GHOST_ALPHA = 0.6f;

        // Pass A: true 3D model ghosts. Each is a real geometry draw with its own
        // shader + depth state, so they are drawn as a group; actors with no rigged
        // geometry drawn (return 0) drop through to the billboard/stick pass below.
        std::vector<const GhostItem*> fallback;
        if (use_model)
        {
            for (const GhostItem& g : ghosts)
            {
                // batches were snapshotted by collectGhostBatches() earlier this
                // frame (while the world render maps were valid); a miss / empty
                // bucket drops through to the billboard/stick fallback. [R2-2]
                // the wearer's non-rigged attachment faces ride along so the
                // node ghosts wear their collars/jewelry too.
                S32 drew = 0;
                auto bit = mGhostBatches.find(g.mAv->getID());
                if (bit != mGhostBatches.end() && !bit->second.empty())
                {
                    drew = drawGeometryGhost(g.mAv, bit->second, g.mFoot, g.mTint,
                                             GHOST_ALPHA, (S32)ghost_style,
                                             LLActorMover::GhostDrawParams(),
                                             ghostStaticFacesFor(g.mAv->getID()));
                }
                if (drew == 0)
                {
                    fallback.push_back(&g);
                }
            }
            // the model pass left the rigged-highlight shader / depth state; restore
            // the UI-overlay context the billboard + stick draws below expect.
            gUIProgram.bind();
        }
        else
        {
            for (const GhostItem& g : ghosts)
            {
                fallback.push_back(&g);
            }
        }

        // Pass B: billboard / stick fallback on the cleared-depth UI overlay.
        for (const GhostItem* gp : fallback)
        {
            const GhostItem& g = *gp;
            // billboard centre = foot + the centre-above-foot height captured at
            // snapshot time (or half the figure height when no snapshot exists)
            F32 center_above = g.mHeight * 0.5f;
            auto it = mGhostImpostors.find(g.mAv->getID());
            if (it != mGhostImpostors.end() && it->second.mValid)
            {
                center_above = it->second.mCenterAboveFoot;
            }
            LLVector3 center = g.mFoot;
            center.mV[VZ] += center_above;

            bool drew = false;
            if (use_impostor)
            {
                drew = drawImpostorGhost(g.mAv, center, g.mTint, GHOST_ALPHA);
            }
            if (!drew)
            {
                drawPoseGhost(g.mFoot, g.mFace, g.mHeight, g.mTint, bb_right, bb_up);
            }
        }
        gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);   // leave UI state untextured
    }

    gGL.setLineWidth(1.f);
    gGL.flush();
}

// ---------------------------------------------------------------------------
// A cached ghost snapshot is stale when there is no usable texture yet, when a
// max age has elapsed (catches pose / appearance / attachment changes we do not
// otherwise track), or when the camera has swung far enough AROUND the actor
// that the frozen 2D card would no longer read as the same view.
bool LLActorMover::ghostImpostorStale(LLVOAvatar* av, const GhostImpostor& gi)
{
    if (!gi.mValid || !av->mImpostor.isComplete())
    {
        return true;
    }
    const F32 MAX_AGE = 2.0f;   // seconds
    if ((F32)LLFrameTimer::getElapsedSeconds() - gi.mLastGenTime > MAX_AGE)
    {
        return true;
    }
    LLVector3 dir = LLViewerCamera::getInstance()->getOrigin()
                  - (av->getRenderPosition() + av->getImpostorOffset());
    if (dir.normalize() < 1e-4f)
    {
        return false;   // degenerate: keep what we have
    }
    const F32 COS_THRESH = 0.990f;   // ~8 degrees of camera swing
    return (dir * gi.mCamDir) < COS_THRESH;
}

// ---------------------------------------------------------------------------
void LLActorMover::updateGhostImpostors()
{
    // same gate as the preview: opt-in, impostor mode on, and an operator
    // floater up. Any miss is an immediate zero-cost return (default no-op).
    // The true 3D model ghost skins live geometry and needs no impostor snapshot,
    // so when it is active we skip the (expensive) generateImpostor re-render
    // entirely -- a pure system-avatar actor the model ghost can't cover then
    // falls back to the stick figure rather than a billboard.
    static LLCachedControl<bool> show(gSavedSettings, "ActorMoverShowHeading", true);
    static LLCachedControl<bool> onion(gSavedSettings, "PathShowOnionSkin", false);
    static LLCachedControl<bool> use_model(gSavedSettings, "PathGhostUseModel", true);
    static LLCachedControl<bool> use_impostor(gSavedSettings, "PathGhostUseImpostor", true);
    if (!show || !onion || use_model || !use_impostor)
    {
        return;
    }
    LLFloater* floaterp = LLFloaterReg::findInstance("actor_mover");
    if (!floaterp || !floaterp->getVisible())
    {
        floaterp = LLFloaterReg::findInstance("director");
        if (!floaterp || !floaterp->getVisible())
        {
            return;
        }
    }

    uuid_vec_t roster = getRoster();
    if (roster.empty())
    {
        roster.push_back(LLUUID::null);     // my avatar
    }

    // Budget: at most a couple of full avatar re-renders per frame, so a busy
    // roster catches up over a few frames instead of spiking one frame. A static
    // frame snapshot is fine for a blocking preview, so a small lag is invisible.
    S32 regen_budget = 2;
    for (const LLUUID& id : roster)
    {
        if (regen_budget <= 0)
        {
            break;
        }
        LLVOAvatar* av = resolve_actor(id);
        if (!av || av->isDead() || !av->mDrawable || !av->getRootJoint())
        {
            continue;
        }
        GhostImpostor& gi = mGhostImpostors[av->getID()];
        if (!ghostImpostorStale(av, gi))
        {
            continue;
        }

        // Render the ACTUAL avatar into its own impostor render target. Called
        // here (the updateImpostors() site) so it runs in the frame's dedicated
        // impostor context -- the caller in llviewerdisplay.cpp already brackets
        // this with the viewport + projection/modelview save-restore.
        gPipeline.generateImpostor(av);
        --regen_budget;

        gi.mValid       = av->mImpostor.isComplete();
        gi.mLastGenTime = (F32)LLFrameTimer::getElapsedSeconds();
        LLVector3 dir = LLViewerCamera::getInstance()->getOrigin()
                      - (av->getRenderPosition() + av->getImpostorOffset());
        dir.normalize();
        gi.mCamDir = dir;
        // billboard centre height above the feet at capture, so a ghost stamped
        // at a foot position sits at the right height without a per-frame probe.
        // renderPosition + impostorOffset is the avatar bbox centre (see
        // LLVOAvatar::calculateSpatialExtents), and feet = root - pelvisToFoot.
        const F32 foot_z   = av->getRootJoint()->getWorldPosition().mV[VZ] - av->getPelvisToFoot();
        const F32 center_z = (av->getRenderPosition() + av->getImpostorOffset()).mV[VZ];
        gi.mCenterAboveFoot = llmax(0.2f, center_z - foot_z);
    }
}

// ---------------------------------------------------------------------------
// Snapshot the rigged draw batches for each ghosted actor WHILE the frame's
// render maps still hold world-camera geometry (called from display() after the
// world render, before render_ui()). render_hud_attachments() re-runs stateSort
// with the HUD camera and repopulates these maps, so a model ghost that read
// them at draw time went blank whenever a HUD was worn -- this decouples the
// enumeration from that timing. One sweep over each rigged pass, bucketed per
// actor and de-duped by buffer range.
void LLActorMover::collectGhostBatches()
{
    mGhostBatches.clear();
    mGhostStaticFaces.clear();

    // Two independent reasons to collect: the PATH-NODE ghost preview (its
    // classic gate: setting trio + an operator floater up) and the GHOST
    // STUDIO (enabled instances render floater-or-not -- they are scene
    // dressing, not an editing overlay). Neither active = byte-identical
    // zero cost.
    static LLCachedControl<bool> show(gSavedSettings, "ActorMoverShowHeading", true);
    static LLCachedControl<bool> onion(gSavedSettings, "PathShowOnionSkin", false);
    static LLCachedControl<bool> use_model(gSavedSettings, "PathGhostUseModel", true);
    bool path_ghosts = show && onion && use_model;
    if (path_ghosts)
    {
        LLFloater* floaterp = LLFloaterReg::findInstance("actor_mover");
        if (!floaterp || !floaterp->getVisible())
        {
            floaterp = LLFloaterReg::findInstance("director");
            path_ghosts = floaterp && floaterp->getVisible();
        }
    }
    const bool studio = ALGhostStudio::instance().anyEnabled();
    if (!path_ghosts && !studio)
    {
        return;
    }

    // the bodies the ghost passes will draw this frame: roster members with a
    // walkable path (>= 2 nodes) for the node preview, plus every source an
    // enabled studio instance references. Collect batches only for those.
    std::set<LLVOAvatar*> wanted;
    if (path_ghosts)
    {
        uuid_vec_t roster = getRoster();
        if (roster.empty())
        {
            roster.push_back(LLUUID::null);     // my avatar
        }
        for (const LLUUID& id : roster)
        {
            LLVOAvatar* av = resolve_actor(id);
            if (!av || av->isDead())
            {
                continue;
            }
            auto pit = mPaths.find(av->getID());
            if (pit != mPaths.end() && pit->second.mNodes.size() >= 2)
            {
                wanted.insert(av);
            }
        }
    }
    if (studio)
    {
        uuid_vec_t sources;
        ALGhostStudio::instance().getWantedSources(sources);
        for (const LLUUID& id : sources)
        {
            LLVOAvatar* av = resolve_actor(id);
            if (av && !av->isDead())
            {
                wanted.insert(av);
            }
        }
    }
    if (wanted.empty())
    {
        return;
    }

    static const U32 kRiggedPasses[] = {
        LLRenderPass::PASS_SIMPLE_RIGGED,
        LLRenderPass::PASS_FULLBRIGHT_RIGGED,
        LLRenderPass::PASS_FULLBRIGHT_SHINY_RIGGED,
        LLRenderPass::PASS_SHINY_RIGGED,
        LLRenderPass::PASS_BUMP_RIGGED,
        LLRenderPass::PASS_MATERIAL_RIGGED,
        LLRenderPass::PASS_MATERIAL_ALPHA_RIGGED,
        LLRenderPass::PASS_MATERIAL_ALPHA_MASK_RIGGED,
        LLRenderPass::PASS_SPECMAP_RIGGED,
        LLRenderPass::PASS_SPECMAP_BLEND_RIGGED,
        LLRenderPass::PASS_SPECMAP_MASK_RIGGED,
        LLRenderPass::PASS_NORMMAP_RIGGED,
        LLRenderPass::PASS_NORMMAP_BLEND_RIGGED,
        LLRenderPass::PASS_NORMMAP_MASK_RIGGED,
        LLRenderPass::PASS_NORMSPEC_RIGGED,
        LLRenderPass::PASS_NORMSPEC_BLEND_RIGGED,
        LLRenderPass::PASS_NORMSPEC_MASK_RIGGED,
        // [R2-2] legacy EMISSIVE-mode materials are BASE geometry passes (a
        // face whose material uses diffuse-alpha-mode EMISSIVE renders ONLY
        // here) -- missing them dropped whole faces from the clone
        LLRenderPass::PASS_MATERIAL_ALPHA_EMISSIVE_RIGGED,
        LLRenderPass::PASS_SPECMAP_EMISSIVE_RIGGED,
        LLRenderPass::PASS_NORMMAP_EMISSIVE_RIGGED,
        LLRenderPass::PASS_NORMSPEC_EMISSIVE_RIGGED,
        LLRenderPass::PASS_ALPHA_RIGGED,
        LLRenderPass::PASS_ALPHA_MASK_RIGGED,
        LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK_RIGGED,
        LLRenderPass::PASS_GLTF_PBR_RIGGED,
        LLRenderPass::PASS_GLTF_PBR_ALPHA_MASK_RIGGED,
        // [R2-2] PBR glow: DUPLICATE geometry carrying the emissive map; only
        // the clone's additive sweep draws it (see ghost_pass_is_glow) so an
        // emissive-driven iris keeps its colour. Deliberately still excluded:
        // PASS_GLOW_RIGGED (legacy glow is a bloom intensity riding the
        // EMISSIVE vertex attribute, not surface colour), the INVISIBLE
        // passes (water exclusion), and PASS_POST_BUMP_RIGGED (cosmetic).
        LLRenderPass::PASS_GLTF_GLOW_RIGGED,
    };
    // [R2-6] ONE attachment walk per wanted wearer harvests BOTH collections
    // straight from the SPATIAL-GROUP DRAW MAPS -- never the frame's cull
    // results. The cull-result render maps are just aggregations of visible
    // groups' mDrawMap entries (direct group access is an existing idiom --
    // lldrawpoolalpha's group->mDrawMap[PASS_ALPHA_RIGGED]), so this is
    // strictly more complete: ghosts keep rendering while the source avatar
    // is OFF-FRAME, occluded, or IMPOSTORED -- exactly the filming case
    // (camera on the ghosts, actor outside the shot). It also retires the old
    // HUD-stateSort timing hazard outright: nothing here reads cull maps, so
    // render_hud_attachments() repopulating them cannot blank a ghost. Draw
    // maps are geometry-scoped (rebuilt on geometry change, not per frame);
    // the LLDrawInfo pointers are still consumed same-frame only. Caveats
    // (documented): an off-frame avatar's motion updates throttle, so a LIVE
    // ghost of an out-of-shot actor holds a coarsely-updated pose (FROZEN
    // instances are unaffected -- that is what freezing is for), and the
    // source's geometry must have been built once since login (seen nearby)
    // for its draw maps to exist at all.
    for (LLVOAvatar* av : wanted)
    {
        std::vector<GhostBatch>& bucket = mGhostBatches[av->getID()];
        std::vector<GhostStaticFace>& faces = mGhostStaticFaces[av->getID()];
        std::set<LLSpatialGroup*> groups;   // attachment drawables' groups, deduped

        for (const auto& ap_pair : av->mAttachmentPoints)
        {
            LLViewerJointAttachment* ap = ap_pair.second;
            if (!ap || ap->getIsHUDAttachment())
            {
                continue;   // HUDs are screen chrome, never body geometry
            }
            for (const LLPointer<LLViewerObject>& attached : ap->mAttachedObjects)
            {
                // the attachment root plus every child link
                std::vector<LLViewerObject*> objs;
                if (attached.notNull())
                {
                    objs.push_back(attached.get());
                    for (LLViewerObject* child : attached->getChildren())
                    {
                        objs.push_back(child);
                    }
                }
                for (LLViewerObject* obj : objs)
                {
                    if (!obj || obj->isDead() || obj->mDrawable.isNull()
                        || obj->mDrawable->isDead())
                    {
                        continue;
                    }
                    LLDrawable* drawable = obj->mDrawable.get();

                    // rigged batches live in this drawable's spatial group's
                    // draw maps (inside the attachment's bridge partition)
                    if (LLSpatialGroup* group = drawable->getSpatialGroup())
                    {
                        groups.insert(group);
                    }

                    // ---- [R2-2] NON-RIGGED faces (collar / jewelry / flexi):
                    // they never carry LLFace::mAvatar, so no rigged pass owns
                    // them -- collected per face with real-render alpha
                    // classification (GLTF mode wins; legacy mask from the
                    // material; the alpha POOL marks its faces blended)
                    const S32 n = drawable->getNumFaces();
                    for (S32 f = 0; f < n; ++f)
                    {
                        LLFace* face = drawable->getFace(f);
                        if (!face || face->isState(LLFace::RIGGED)
                            || !face->getVertexBuffer() || face->getIndicesCount() == 0)
                        {
                            continue;   // rigged faces came via the batch sweep
                        }
                        GhostStaticFace gf;
                        gf.mFace = face;
                        gf.mObjectId = obj->getID();
                        // alpha classification, mirroring the real pools: GLTF
                        // mode wins; legacy mask comes from the material; the
                        // alpha POOL marks everything it owns as blended
                        const LLTextureEntry* te = face->getTextureEntry();
                        LLGLTFMaterial* gmat = te ? te->getGLTFRenderMaterial() : nullptr;
                        if (gmat)
                        {
                            gf.mDoubleSided = gmat->mDoubleSided;
                            if (gmat->mAlphaMode == LLGLTFMaterial::ALPHA_MODE_MASK)
                            {
                                gf.mAlphaKind = 1;
                                gf.mCutoff = gmat->mAlphaCutoff;
                            }
                            else if (gmat->mAlphaMode == LLGLTFMaterial::ALPHA_MODE_BLEND)
                            {
                                gf.mAlphaKind = 2;
                            }
                        }
                        else if (te)
                        {
                            const LLMaterial* mat = te->getMaterialParams().get();
                            if (mat && mat->getDiffuseAlphaMode() == LLMaterial::DIFFUSE_ALPHA_MODE_MASK)
                            {
                                gf.mAlphaKind = 1;
                                gf.mCutoff = mat->getAlphaMaskCutoff() * (1.f / 255.f);
                            }
                            else if (face->getPoolType() == LLDrawPool::POOL_ALPHA
                                     || te->getColor().mV[VW] < 0.999f)
                            {
                                gf.mAlphaKind = 2;
                            }
                        }
                        faces.push_back(gf);
                    }
                }
            }
        }

        // [R2-6] rigged batches out of the deduped groups' draw maps.
        // Everything in an attachment group belongs to THIS wearer by
        // construction (the animesh case included: those draw infos carry the
        // attachment's own control avatar as mAvatar, and the draw uploads
        // per-batch palettes), so bucketing needs no owner resolution.
        for (LLSpatialGroup* group : groups)
        {
            for (U32 pass : kRiggedPasses)
            {
                auto dit = group->mDrawMap.find(pass);      // find(): never insert
                if (dit == group->mDrawMap.end())
                {
                    continue;
                }
                for (const LLPointer<LLDrawInfo>& dip : dit->second)
                {
                    LLDrawInfo* di = dip.get();
                    if (!di || di->mAvatar.isNull()
                        || di->mSkinInfo.isNull() || di->mVertexBuffer.isNull())
                    {
                        continue;
                    }
                    bool dup = false;
                    for (const GhostBatch& b : bucket)
                    {
                        if (b.mInfo->mVertexBuffer.get() == di->mVertexBuffer.get()
                            && b.mInfo->mStart == di->mStart && b.mInfo->mEnd == di->mEnd
                            && b.mInfo->mOffset == di->mOffset)
                        {
                            dup = true;
                            break;
                        }
                    }
                    if (!dup)
                    {
                        bucket.push_back({ di, pass });
                    }
                }
            }
        }

        // keep both maps miss-cheap
        if (bucket.empty())
        {
            mGhostBatches.erase(av->getID());
        }
        if (faces.empty())
        {
            mGhostStaticFaces.erase(av->getID());
        }
    }
}

// ---------------------------------------------------------------------------
// [GhostStudio] frame-lifetime batch access for the studio (freeze snapshot +
// per-instance draw). Null when the pipeline is not rendering that body.
const std::vector<LLActorMover::GhostBatch>* LLActorMover::ghostBatchesFor(const LLUUID& wearer_id) const
{
    auto it = mGhostBatches.find(wearer_id);
    return (it != mGhostBatches.end() && !it->second.empty()) ? &it->second : nullptr;
}

// [R2-2] frame-lifetime static-face access (freeze matrix capture + draw)
const std::vector<LLActorMover::GhostStaticFace>* LLActorMover::ghostStaticFacesFor(const LLUUID& wearer_id) const
{
    auto it = mGhostStaticFaces.find(wearer_id);
    return (it != mGhostStaticFaces.end() && !it->second.empty()) ? &it->second : nullptr;
}

// ---------------------------------------------------------------------------
// [GhostStudio] draw every enabled studio instance. Self-contained UI-overlay
// state (its render_ui() call site sets nothing up for it). MODEL ghosts only:
// an instance whose source yielded neither rigged batches nor attachment faces
// this frame (out of world, geometry never built, pure system avatar) is
// skipped quietly -- the panel surfaces why. [R2-6] off-frame/occluded/
// impostored sources DO yield (the collector reads spatial-group draw maps,
// not cull results). Sorted far-to-near so translucent styles stack correctly
// against each other on the cleared-depth overlay.
void LLActorMover::renderStudioGhosts()
{
    ALGhostStudio& studio = ALGhostStudio::instance();
    if (!studio.anyEnabled())
    {
        return;
    }

    // per-instance draw items, resolved once. [R2-2] a wearer's non-rigged
    // attachment faces ride along; an instance needs rigged batches OR static
    // faces to draw (an empty frame skips quietly -- the panel surfaces why).
    static const std::vector<GhostBatch> sNoBatches;
    struct StudioItem
    {
        const ALGhostStudio::Instance* mInst;
        LLVOAvatar* mAv;
        const std::vector<GhostBatch>* mBatches;
        const std::vector<GhostStaticFace>* mStatic;
        LLVector3   mFootAgent;
    };
    std::vector<StudioItem> items;
    for (const ALGhostStudio::Instance& inst : studio.getInstances())
    {
        if (!inst.mEnabled)
        {
            continue;
        }
        LLVOAvatar* av = resolve_actor(inst.mSource);
        if (!av || av->isDead())
        {
            continue;
        }
        const std::vector<GhostBatch>* batches = ghostBatchesFor(av->getID());
        const std::vector<GhostStaticFace>* statics = ghostStaticFacesFor(av->getID());
        if (!batches && !statics)
        {
            continue;
        }
        items.push_back({ &inst, av, batches ? batches : &sNoBatches, statics,
                          gAgent.getPosAgentFromGlobal(inst.mFootGlobal) });
    }
    if (items.empty())
    {
        return;
    }

    // same overlay idiom as the path preview: UI shader, no depth vs the world
    LLGLSUIDefault gls_ui;
    gUIProgram.bind();
    gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

    const LLVector3 cam_pos = LLViewerCamera::getInstance()->getOrigin();
    std::sort(items.begin(), items.end(),
              [&](const StudioItem& a, const StudioItem& b)
              {
                  return (a.mFootAgent - cam_pos).magVecSquared()
                       > (b.mFootAgent - cam_pos).magVecSquared();
              });

    for (const StudioItem& item : items)
    {
        const ALGhostStudio::Instance& inst = *item.mInst;

        // tint: the source's stable path hue by default, or the instance's own
        // authored hue (pastel-bright so every style's tint pull reads)
        LLColor4 tint;
        if (inst.mUseActorTint)
        {
            tint = actorPathColor(item.mAv->getID());
        }
        else
        {
            tint.setHSL(fmodf(llmax(inst.mHue, 0.f), 360.f) / 360.f, 0.9f, 0.6f);
            tint.mV[VW] = 1.f;
        }

        GhostDrawParams gp;
        gp.mYaw   = inst.mYaw;
        gp.mScale = inst.mScale;
        if (inst.mPose == ALGhostStudio::POSE_FROZEN && !inst.mFrozenPalettes.empty())
        {
            gp.mHavePivot      = true;
            gp.mPivotFootAgent = inst.mFrozenFootAgent;
            gp.mFrozenPalettes = &inst.mFrozenPalettes;
            // [R2-2] frozen non-rigged attachment placement (capture-frame
            // matrices; a lookup miss -- e.g. flexi -- stays LIVE, documented)
            if (!inst.mFrozenAttachMats.empty())
            {
                gp.mFrozenAttachMats = &inst.mFrozenAttachMats;
            }
        }
        gp.mShimmerSpeed     = inst.mShimmerSpeed;
        gp.mShimmerIntensity = inst.mShimmerIntensity;
        gp.mPixelSize        = inst.mPixelSize;
        gp.mGlitch           = inst.mGlitch;
        gp.mBrightness       = inst.mBrightness;      // [R2-1] night-scene dimmer
        gp.mTintCustom       = !inst.mUseActorTint;   // hue slider reaches the clone
        // stable per-instance FX phase from the id, so a crowd of ghosts
        // shimmers/glitches out of sync instead of strobing as one
        gp.mPhase = (F32)(inst.mId.mData[0] | (inst.mId.mData[1] << 8)) * (F_TWO_PI / 65536.f);

        drawGeometryGhost(item.mAv, *item.mBatches, item.mFootAgent, tint,
                          llclamp(inst.mAlpha, 0.f, 1.f), inst.mStyle, gp,
                          item.mStatic);
    }

    // leave clean UI-overlay state for whoever draws next
    gUIProgram.bind();
    gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

    // ---- [R2-3] selected-ghost highlight ring (edit tool active only) --------
    // A pulsing ground ring in the ghost's tint at its foot. This is an EDIT
    // indicator, not scene dressing, so it only draws while ALToolGhostEdit is
    // the current tool -- it can never leak into a filmed frame (the tool is
    // gone the moment the operator leaves edit mode, and studio ghosts render
    // with the UI hidden precisely because they are NOT gated like this).
    if (LLToolMgr::getInstance()->getCurrentTool() == ALToolGhostEdit::getInstance())
    {
        if (ALGhostStudio::Instance* inst = studio.getInstance(studio.getSelected()))
        {
            LLColor4 ring_tint;
            if (!inst->mUseActorTint)
            {
                ring_tint.setHSL(fmodf(llmax(inst->mHue, 0.f), 360.f) / 360.f, 0.9f, 0.6f);
            }
            else if (LLVOAvatar* av = resolve_actor(inst->mSource))
            {
                ring_tint = actorPathColor(av->getID());
            }
            else
            {
                ring_tint = LLColor4(1.f, 1.f, 1.f, 1.f);
            }
            // slow breathe, same clock as the path editor's selected node
            const F32 now   = (F32)LLFrameTimer::getElapsedSeconds();
            const F32 pulse = 0.55f + 0.45f * sinf(now * 3.2f);
            ring_tint.mV[VW] = 0.85f * pulse;

            LLVector3 foot = gAgent.getPosAgentFromGlobal(inst->mFootGlobal);
            foot.mV[VZ] += 0.05f;   // sit the ring just off the ground
            const F32 r0 = 0.45f * llmax(inst->mScale, 0.3f);
            const F32 r1 = r0 + 0.09f;
            constexpr S32 SEGS = 32;
            gGL.begin(LLRender::TRIANGLE_STRIP);
            gGL.color4fv(ring_tint.mV);
            for (S32 i = 0; i <= SEGS; ++i)
            {
                const F32 a = F_TWO_PI * (F32)i / (F32)SEGS;
                const F32 c = cosf(a), s = sinf(a);
                gGL.vertex3f(foot.mV[VX] + c * r0, foot.mV[VY] + s * r0, foot.mV[VZ]);
                gGL.vertex3f(foot.mV[VX] + c * r1, foot.mV[VY] + s * r1, foot.mV[VZ]);
            }
            gGL.end();
        }
    }

    gGL.flush();
}
