/**
 * @file llactormover.h
 * @brief Local ("ghost") locomotion rig: moves an avatar's RENDERED body along
 *        a client-side path at arbitrary speed with cadence-locked walk anims.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * The sim is never told: the avatar's true position stays wherever the server
 * thinks it is, and only this client renders the motion (same local-only
 * philosophy as the any-avatar poser). That is the point -- LSL movement tools
 * are quantized by sim ticks and keyframe granularity; this evaluates per
 * rendered frame at any speed, and locks the walk-cycle clock to the actual
 * ground speed so foot plants never slide.
 *
 * Multi-actor: right-click targeting builds an ordered session-only ROSTER of
 * avatars (regular or animesh control avatars). An empty roster still means
 * "my avatar". Each Move captures its own parameters at start, so different
 * actors can run different speeds/headings concurrently.
 *
 * [Director] The roster IS the Director cast: membership storage lives in
 * LLDirectorCast and the static roster API here delegates to it, so
 * "Actor Mover Target" and "Add to Cast" toggle the same list and the
 * Director Console floater is a view over the same actors.
 */

#ifndef LL_LLACTORMOVER_H
#define LL_LLACTORMOVER_H

#include "lluuid.h"
#include "v3math.h"
#include "v3dmath.h"
#include "v4color.h"        // actorPathColor()
#include "llquaternion.h"

#include <map>
#include <vector>

class LLVOAvatar;

class LLActorMover
{
public:
    static LLActorMover& instance();

    // -----------------------------------------------------------------------
    // Actor pathing (P1): a 3D polyline the actor walks with natural curved
    // turning, arc-length constant ground speed, ease in/out, dwell, and
    // ground-follow over slopes/stairs. A path with >= 2 nodes drives the
    // walk; 0 or 1 node falls back to the legacy straight-segment Move
    // (byte-identical default behavior). Waypoints are stored in GLOBAL
    // coordinates so a path survives region crossings (same idiom as
    // LLFlycamRecorder::Keyframe). Session-only for now; the accessors below
    // let scene code read/write nodes + params without touching the internals.
    // -----------------------------------------------------------------------
    struct Waypoint
    {
        LLVector3d mPosGlobal;          // node position, global coords (foot/GROUND level)
        F32        mDwell = 0.f;        // seconds to hold at this node (0 = none)
        F32        mSpeedOverride = 0.f;// local ground speed, m/s (0 = use path speed)
        LLUUID     mAnim;               // per-node anim (null = use loco anim)
        F32        mGroundOffset = 0.f; // manual Z nudge at this node, m
        // Authored root height ABOVE the ground at this node (captured once, like
        // the legacy straight-move captures its root origin). Placement adds this
        // to the resolved ground so the rendered root sits at the SAME stable
        // standing height it had when the node was dropped -- it is NOT rebuilt
        // per frame from the live, pose-dependent getPelvisToFoot(). On flat
        // ground this makes path placement byte-match the legacy straight move;
        // ground-follow rides slopes/stairs by swapping the ground under it.
        F32        mRootAbove = 0.f;    // root Z - snapped ground Z at capture, m

        // ---- P3 optional per-node camera (author a shot inline on the path) ---
        // A world-anchored studio camera captured from the live render camera and
        // stored in GLOBAL coords (consistent with mPosGlobal, so it survives a
        // region crossing exactly like LLFlycamRecorder::Keyframe). When mHasCam,
        // the path-camera source frames this pose as the actor's arc clock passes
        // the node: a CUT node snaps at the crossing, an EASE node interpolates
        // from the previous camera node. Session-only; scene code serializes these
        // through the clean accessors below. (Actor-relative cameras that ride a
        // re-anchor are a deferred later flag -- world-anchored is the default.)
        bool         mHasCam = false;
        LLVector3d   mCamPosGlobal;      // camera origin, global coords
        LLQuaternion mCamRot;            // camera orientation, world frame
        F32          mCamFov = 0.f;      // vertical FOV, radians (0 = viewer default)
        S32          mCamTransition = 1; // how the camera REACHES this node: 0 cut, 1 ease
    };

    struct Path
    {
        // ---- authored data (this is what scene code serializes) ----
        std::vector<Waypoint> mNodes;
        F32    mSpeed        = 1.f;     // nominal ground speed, m/s
        S32    mEndMode      = 0;       // 0 stop, 1 loop, 2 ping-pong
        F32    mTension      = 0.5f;    // 0 = tight/near-polyline, 1 = loose/flowing
        F32    mEaseIn       = 0.f;     // accel ramp at path start, s
        F32    mEaseOut      = 0.f;     // decel ramp into the final node, s
        S32    mArrivalFacingMode = 0;  // 0 none, 1 compass direction, 2 cast member
        F32    mArrivalDir   = 0.f;     // world yaw radians (mode 1)
        LLUUID mArrivalTarget;          // cast member id (mode 2)
        bool   mGroundFollow = false;   // clamp feet to terrain/prims each frame
        bool   mPitchToSlope = false;   // tilt root pitch to the local slope

        // any edit to the geometry/shape must invalidate the arc-length table
        void markDirty() { mDirty = true; }

        // ---- compiled arc-length cache (NOT serialized; rebuilt lazily) ----
        // Treat as opaque. Built in GLOBAL coords so it is origin-independent
        // (a region crossing does not invalidate it).
        struct ArcSample { F32 mDist; S32 mSeg; F32 mLocalT; };
        bool                   mDirty = true;
        F32                    mTotalLength = 0.f;
        std::vector<F32>       mNodeDist;   // cumulative arc distance at each node
        std::vector<ArcSample> mArc;        // cumulative arc-length lookup

        S32        segmentCount() const;    // n-1 (stop/pingpong) or n (loop)
        void       rebuild();               // fills mArc / mNodeDist / mTotalLength
        LLVector3d evalSegment(S32 seg, F32 t) const;   // centripetal CR + tension blend
        // position (global) + unit travel tangent (global) at arc distance d
        void       evalAtDistance(F32 d, LLVector3d& pos, LLVector3d& tangent) const;
    };

    // path accessors (session-only; scene code reads/writes through these)
    Path&       editPath(const LLUUID& actor_id);          // creates an empty path if absent
    const Path* getPath(const LLUUID& actor_id) const;     // nullptr when none
    bool        hasWalkablePath(const LLUUID& actor_id) const;   // path with >= 2 nodes
    void        clearPath(const LLUUID& actor_id);

    // test harness (chat commands) + panel "Add": append a ground-snapped
    // waypoint at the actor's current rendered position.
    void        appendWaypointHere(const LLUUID& actor_id);

    // ---- P2 index-based editing (session-only; operate on the actor's Path) ---
    // ground_pos is a GLOBAL foot/ground position (typically an in-world pick).
    // Each op captures mRootAbove from the resolving actor's standing height so a
    // placed/moved node sits feet-on-ground rather than hovering, and marks the
    // path dirty so the arc-length table rebuilds. Bad index or unresolved actor
    // -> no-op (returns -1 / false). Index ops use std::vector insert semantics.
    S32  appendWaypointAt(const LLUUID& actor_id, const LLVector3d& ground_pos);  // -> new index, -1 fail
    S32  insertWaypoint(const LLUUID& actor_id, S32 index, const LLVector3d& ground_pos); // insert AT index
    bool moveWaypoint(const LLUUID& actor_id, S32 index, const LLVector3d& new_ground_pos);
    bool deleteWaypoint(const LLUUID& actor_id, S32 index);
    // per-node field setters (do not move geometry; still mark dirty per spec)
    bool setNodeDwell(const LLUUID& actor_id, S32 index, F32 dwell_s);
    bool setNodeSpeed(const LLUUID& actor_id, S32 index, F32 speed_override);
    bool setNodeAnim(const LLUUID& actor_id, S32 index, const LLUUID& anim);
    bool setNodeGroundOffset(const LLUUID& actor_id, S32 index, F32 offset_m);

    // ---- P3 per-node camera (author an actor+camera TAKE inline) --------------
    // Capture a camera into a node from a render-camera pose already resolved to
    // GLOBAL coords by the caller (LLViewerCamera origin -> global, orientation,
    // getView() as vertical FOV). transition: 0 = cut, 1 = ease. Marks mHasCam.
    // Bad index / unresolved actor -> false. Camera fields do not affect the
    // arc-length geometry, so these do NOT dirty the path.
    bool setNodeCamera(const LLUUID& actor_id, S32 index, const LLVector3d& cam_pos_global,
                       const LLQuaternion& cam_rot, F32 cam_fov, S32 transition);
    bool clearNodeCamera(const LLUUID& actor_id, S32 index);
    bool setNodeCamTransition(const LLUUID& actor_id, S32 index, S32 transition);
    // read a node's stored camera (panel Preview + inspector). False = bad index
    // or the node carries no camera.
    bool getNodeCamera(const LLUUID& actor_id, S32 index, LLVector3d& cam_pos_global,
                       LLQuaternion& cam_rot, F32& cam_fov, S32& transition) const;
    // how many nodes on this actor's path carry a camera (panel + viz gate)
    S32  pathCameraNodeCount(const LLUUID& actor_id) const;

    // ---- P3 path-camera SOURCE (drives the render camera during a TAKE) -------
    // The camera-driving path is the path of the Subject-A actor (there is only
    // ever one Subject A, so EXACTLY ONE path drives the camera -- no two sources
    // fight). The LLPathCamera source calls these every frame.
    //
    // hasActivePathCamera() is the OWNERSHIP gate: true only while the actor is
    // actively walking its path (an mIsPath Move that has not settled at a
    // stop-mode end) AND that path has >= 1 camera node. It must go false the
    // SAME frame the walk stops / finishes / is cut, so the source releases the
    // render camera back to the agent camera and never latches. (Actor derez /
    // region change is caught upstream: LLPathCamera resolves Subject A each
    // frame and releases the instant it is unresolvable.)
    bool hasActivePathCamera(const LLUUID& actor_id) const;
    // Evaluate the eased/cut camera track at the actor's CURRENT arc distance and
    // return the render-camera pose (global origin, world rotation, vertical FOV
    // radians). False when not path-walking or the path has no camera nodes.
    bool getPathCameraPose(const LLUUID& actor_id, LLVector3d& cam_pos_global,
                           LLQuaternion& cam_rot, F32& cam_fov) const;

    // ---- P2 edit selection (shared by the path-editor panel + in-world tool + viz) ---
    // Which actor's path is being edited (stored as the resolved path key) and
    // which node is selected (-1 = none). Written by the panel (list row <->
    // in-world node) and read by renderHeadingPreview() (highlight) + the tool.
    void          setEditActor(const LLUUID& actor_id);
    const LLUUID& getEditActor() const { return mEditActor; }
    void          setEditNode(S32 index) { mEditNode = index; }
    S32           getEditNode() const { return mEditNode; }

    // stable per-actor overlay color derived from the actor id (path ribbon +
    // interior nodes tint to this hue; the panel shows it as a read-only swatch)
    static LLColor4 actorPathColor(const LLUUID& actor_id);

    // right-click targeting (same session-lock idiom as CineCam Follow), now a
    // roster: toggling an avatar adds/removes it; empty roster = "my avatar"
    static void toggleTarget(const LLUUID& id);
    static bool isTarget(const LLUUID& id);
    static const uuid_vec_t& getRoster();

    // transport, driven by the Actor Mover floater. start(id) begins a move
    // for one actor (null id = my avatar) from its CURRENT rendered pose;
    // startAll()/stopAll() cover every roster member (self when empty).
    void start(const LLUUID& actor_id);
    void startAll();
    void stop(const LLUUID& actor_id);
    void stopAll();

    // [Director] pin an actor's rendered root at a position (a zero-speed
    // hold Move, no locomotion anim): Reset to Marks uses this for the
    // snap-back. Stops the actor's active move first. Released like any
    // move (stop/stopAll), and a later start() walks FROM the held spot.
    void placeAt(const LLUUID& actor_id, const LLVector3& pos);
    bool anyMoving() const { return !mMoves.empty(); }
    bool isMoving(const LLUUID& id) const { return mMoves.count(id) != 0; }

    // floater status readout: distance traveled along the CURRENT leg vs the
    // configured path length. Returns false when the actor is not moving.
    bool getProgress(const LLUUID& actor_id, F32& traveled, F32& total) const;

    // called from avatar root-placement sites each frame; returns true and
    // writes root world pos/rot when this avatar is being ghost-moved. Safe
    // to call more than once per frame (the path clock only advances once);
    // later callers just re-assert the override, which is how it wins against
    // LLControlAvatar::matchVolumeTransform() for animesh.
    bool applyOverride(LLVOAvatar* av);

    // ---- TP-away suspend / resume (walk resilience) ---------------------------
    // Per-frame state machine, called once from the main idle loop BEFORE the
    // avatar character update that runs applyOverride(). When a moving actor
    // derezzes / leaves the region / teleports, the walk is not destroyed but
    // SUSPENDED: the path clock freezes, the override stops painting (so no
    // stale pose is ever left at old-region coords), the loco anim stops, and
    // the Move + Path + progress are kept intact. Detection is either a large
    // sim-true global-position jump (a still-resolvable actor, e.g. the user's
    // own avatar on teleport) or the actor going unresolvable for a short
    // debounce (derez / left region), so a 1-frame hiccup never suspends. A
    // suspended walk auto-resumes the instant the actor is resolvable again NEAR
    // where it left off; otherwise it stays suspended (held for return) and the
    // Path tab surfaces the Resume / Re-anchor / Cancel controls below. Each
    // moving actor suspends and resumes independently. A deliberate stop() still
    // erases as before -- only the derez/teleport path suspends.
    void updateSuspendState();

    // Path-tab UI state: is this actor's walk suspended, is the actor resolvable
    // right now (gates Resume / Re-anchor), and is it back near the suspend
    // anchor (status readout). All key by the resolved path key.
    bool isWalkSuspended(const LLUUID& actor_id) const;
    bool suspendedActorResolvable(const LLUUID& actor_id) const;
    bool suspendedActorNearAnchor(const LLUUID& actor_id) const;

    // Path-tab UI actions. resumeWalk() unfreezes from the preserved progress at
    // the ORIGINAL anchor; reanchorWalk() translates the WHOLE path (every node
    // global position and node camera) by the delta from the current arc
    // position to the actor's current position, then resumes there (keep
    // shooting on the new sim); cancelSuspended() drops the suspended walk back
    // to idle. resume / reanchor no-op and return false when not suspended or
    // the actor is unresolvable (reanchor also requires a walkable path).
    bool resumeWalk(const LLUUID& actor_id);
    bool reanchorWalk(const LLUUID& actor_id);
    void cancelSuspended(const LLUUID& actor_id);

    // in-world heading preview lines (called from render_ui_3d, same pass as
    // the debug beacons). Zero cost unless ActorMoverShowHeading is on AND
    // the Actor Mover floater is open.
    void renderHeadingPreview();

private:
    LLActorMover() = default;

    struct Move
    {
        LLVector3 mOrigin;      // rendered start position (agent region coords)
        F32       mHeading = 0.f;   // world yaw of travel, radians
        F32       mSpeed = 1.f;     // m/s
        F32       mDistance = 5.f;  // path length, m
        S32       mEndMode = 0;     // 0 hold at end, 1 loop, 2 ping-pong
        F32       mT = 0.f;         // elapsed seconds
        LLUUID    mAnim;            // locomotion anim started for this move
        // per-frame cache so repeated applyOverride() calls are idempotent
        U32          mLastFrame = 0xFFFFFFFF;
        LLVector3    mCurPos;
        LLQuaternion mCurRot;
        // diagnostic: frames already logged this walk (gated by ActorMoverPathZDebug,
        // off by default). Logs the first few frames' Z reasoning, then goes quiet.
        S32          mDbgFrames = 0;

        // ---- path-traversal runtime (mIsPath) -----------------------------
        // A Move with mIsPath walks the actor's Path (looked up by the map key)
        // instead of the single straight leg above. mDist is arc-length along
        // the curve (constant ground speed); mCurRot is the turn-rate-smoothed
        // facing carried frame to frame.
        bool      mIsPath   = false;
        F32       mDist     = 0.f;      // arc-length traveled, m
        F32       mDir      = 1.f;      // +1/-1 travel direction (ping-pong)
        F32       mNominal  = 3.f;      // cadence reference (walk design speed)
        bool      mArrived  = false;    // reached the last node (stop mode)
        bool      mFaceInit = false;    // mCurRot seeded from actor facing yet
        // dwell bookkeeping
        S32       mDwellNode = -1;      // node index we are currently holding at (-1 none)
        F32       mDwellT    = 0.f;     // seconds elapsed in the current dwell
        S32       mLastDwellNode = -1;  // last node already dwelled (avoid re-trigger)
        LLUUID    mDwellAnim;           // per-node anim started for the dwell (stop on resume)

        // ---- TP-away suspend/resume state ---------------------------------
        // A suspended walk freezes here: applyOverride() returns early (no clock
        // advance, no stale placement) until the state machine resumes or
        // cancels it. The sim-true global position (NOT the ghost-overridden
        // root) is tracked while walking so a teleport shows up as a large
        // one-frame jump, and is captured at suspend as the anchor the return
        // test compares against.
        bool       mSuspended        = false;
        S32        mUnresolvedFrames  = 0;      // consecutive unresolvable frames (debounce)
        bool       mHasTrueGlobal     = false;  // mLastTrueGlobal seeded yet
        LLVector3d mLastTrueGlobal;             // last sim-true global while walking
        LLVector3d mSuspendTrueGlobal;          // sim-true global captured at suspend
    };

    static F32 pathParam(const Move& mv, F32* face); // leg progress s, facing

    // per-frame evaluation of an active path traversal (called from
    // applyOverride once the path clock has advanced this frame)
    void   advancePath(LLVOAvatar* av, Move& mv, F32 dt);

    // suspend/resume internals: enterSuspend() freezes a walk (capturing the
    // return anchor + stopping the loco anim if the actor is still present);
    // resumeMove() unfreezes it (restarting the cadence + reseeding the jump
    // probe and facing). Both operate on an entry already located in mMoves.
    void   enterSuspend(const LLUUID& key, Move& mv, LLVOAvatar* av);
    void   resumeMove(const LLUUID& key, Move& mv, LLVOAvatar* av);
    // resolve the ground Z (agent frame) under an actor for ground-follow:
    // a capped downward object raycast for stairs/prims, else terrain land
    // height when the actor sits near it, else the interpolated spline Z
    // (fallback_z) so an elevated actor the ray missed is not yanked down.
    static F32 resolveGroundZ(LLVOAvatar* av, const LLVector3& agent_pos,
                              F32 fallback_z, bool& out_hit);

    std::map<LLUUID, Move> mMoves;
    std::map<LLUUID, Path> mPaths;      // authored path per actor (session-only)

    // P2 edit selection: the path key under edit + the selected node index
    LLUUID mEditActor;      // resolved path key (null = nothing being edited)
    S32    mEditNode = -1;  // selected node in that path (-1 = none)
};

#endif // LL_LLACTORMOVER_H
