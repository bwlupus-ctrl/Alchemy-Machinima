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
class LLDrawInfo;

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

        // ---- P3 sync-to-take: the Flycam Recorder playhead drives the arc ------
        // When mSyncToTake AND a take is loaded (>=1 keyframe, duration > 0), the
        // actor's arc position is derived every frame from the recorder playhead
        // instead of advancing by dt: arc = clamp((playhead + lead/trail)/duration,
        // 0..1) * mTotalLength. PLAY or SCRUB the recorder and the actor moves in
        // lockstep (deterministic actor+lens takes, body+lens dry-run preview). The
        // lead/trail shifts the normalized playhead by mSyncLeadTrail/duration so
        // the actor can lead (+) or trail (-) the lens by N seconds. An empty take
        // / zero duration is a no-op: the walk advances normally (byte-identical),
        // so the flag is safe with no take loaded. Authored + serialized fields.
        bool   mSyncToTake   = false;   // drive arc from the recorder playhead
        F32    mSyncLeadTrail = 0.f;    // seconds the actor leads(+)/trails(-) the lens

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

    // ---- P3 QOL bundle (readout, undo/redo, structural ops) -------------------
    // Path length + estimated walk duration (seconds), for timing a walk to music
    // or cuts. Duration integrates the eased/per-node-override ground speed over
    // the arc plus interior-node dwell (matching the walk), for ONE pass (one lap
    // in loop mode, one-way in ping-pong). Rebuilds the arc table if dirty. False
    // when the actor has no walkable path (>= 2 nodes).
    bool getPathStats(const LLUUID& actor_id, F32& out_length, F32& out_duration);

    // Bounded per-actor edit-history for path mutations (nodes + per-node
    // dwell/speed/anim/camera/groundOffset + path-wide params). A discrete edit
    // pushes the PRE-edit authored state via snapshotForUndo() before mutating;
    // undo/redo swap authored state and rebuild the arc table. History is keyed by
    // the resolved path key, capped at UNDO_DEPTH, and lives for the session.
    void snapshotForUndo(const LLUUID& actor_id);
    bool undoPath(const LLUUID& actor_id);
    bool redoPath(const LLUUID& actor_id);
    bool canUndoPath(const LLUUID& actor_id) const;
    bool canRedoPath(const LLUUID& actor_id) const;

    // Structural ops (each keeps every node's per-node data attached and rebuilds
    // the arc table). Callers snapshotForUndo() first; these are no-ops (return
    // false) on a bad/short path. reverse: flip node order. mirror: reflect node +
    // camera positions/aim left-to-right across the vertical plane through the
    // path centroid containing the dominant travel direction. loopClose: snap the
    // last node onto the first and set end-mode to loop. copyPathTo: deep-copy the
    // authored path onto another actor's slot (global coords unchanged; overlays).
    bool reversePath(const LLUUID& actor_id);
    bool mirrorPath(const LLUUID& actor_id);
    bool loopClosePath(const LLUUID& actor_id);
    bool copyPathTo(const LLUUID& src_actor, const LLUUID& dst_actor);

    // "Walk to here": replace the actor's path with a fresh 2-node straight path
    // (current rendered foot -> ground_global) and start the walk. Snapshots undo
    // first. False when the actor is unresolvable.
    bool startWalkTo(const LLUUID& actor_id, const LLVector3d& ground_global);

    // is this actor currently walking a PATH (mIsPath Move present, suspended or
    // not)? Structural edits/undo that rewrite geometry are gated off while true,
    // so a running walk is never corrupted underneath its arc clock.
    bool isPathWalking(const LLUUID& actor_id) const;

    // ---- P3 Follow-the-leader (procession by reference) -----------------------
    // A follower rides the LEADER's authored path BY REFERENCE (edit the leader's
    // path and the follower updates automatically -- the follower keeps NO nodes
    // of its own; any it has are ignored while following). The follower evaluates
    // the leader's spline at an OFFSET arc position behind the leader. Chainable
    // (a follower can itself lead another). Facing tracks the leader's path
    // tangent at the follower's position; cadence locks to the follower's own
    // instantaneous ground speed. Robustness: if the leader stops / finishes /
    // suspends / derezzes, the follower HOLDS gracefully (freezes in place, never
    // crashes, never snaps to the origin). Session-only, like the paths.
    //
    // Offset modes: 0 = DISTANCE (N metres behind on the arc -- shipped, robust);
    // 1 = TIME (T seconds later -- DEFERRED, not yet driven, see the report).
    struct Follow
    {
        LLUUID mLeader;         // resolved path key of the leader (never null)
        S32    mMode  = 0;      // 0 distance (m behind), 1 time (s later, deferred)
        F32    mOffset = 3.f;   // metres behind (mode 0) / seconds later (mode 1)
    };
    // Establish/replace a follow. Refuses (returns false) a null/self leader or
    // any relationship that would form a CYCLE (A->B->A), so a procession chain
    // can never loop. mode 1 (time) is accepted into the data model but currently
    // behaves as a HOLD at the follower (time-offset traversal is deferred).
    bool setFollow(const LLUUID& follower, const LLUUID& leader, S32 mode, F32 offset);
    // Drop a follow. If the follower was riding a leader with no path of its own,
    // its ghost walk is ended cleanly so it never falls back to an empty path.
    void clearFollow(const LLUUID& follower);
    bool getFollow(const LLUUID& follower, LLUUID& leader, S32& mode, F32& offset) const;
    bool isFollowing(const LLUUID& follower) const;
    // Would making `follower` follow `candidate_leader` create a cycle (or is it
    // self)? The UI excludes such leaders from the picker.
    bool wouldFollowCycle(const LLUUID& follower, const LLUUID& candidate_leader) const;

    // ---- P3 Look-at while walking (procedural gaze) ---------------------------
    // Per-actor head/neck/torso/eye gaze, layered on the walk each frame AFTER
    // the motion controller poses the skeleton (applyGaze, called post-motion
    // from the avatar update). The aim + distribution + limits are ported from
    // the viewer's own look-at motions (LLHeadRotMotion / LLEyeMotion) so it
    // reads as natural as stock head tracking, with knobs the built-in lacks:
    // head-vs-eyes blend, overall intensity, and a smoothing time constant, plus
    // an ease-in/out envelope so enabling/disabling/releasing never pops.
    //
    // Gaze is only PAINTED while the actor is under an active, non-suspended Move
    // (a walk or a placeAt hold) -- so it composes with walk-and-talk and eases
    // cleanly out to the anim pose the moment the walk ends / suspends / the actor
    // derezzes / gaze is disabled, then stops touching the joints entirely. With
    // no gaze configured for an avatar this is a single map-miss no-op (the walk /
    // AO plays byte-identically to today). Config is session-only, keyed by the
    // resolved actor id, and persists across stop/start like the path.
    //
    // Target modes: 0 = path tangent ("look where I'm going", DEFAULT), 1 =
    // camera, 2 = cast member, 3 = fixed point (global). An unresolvable cast
    // target / degenerate direction falls back to the path tangent gracefully.
    enum { GAZE_TANGENT = 0, GAZE_CAMERA = 1, GAZE_CAST = 2, GAZE_POINT = 3 };

    void   setGazeEnabled(const LLUUID& actor_id, bool on);
    bool   isGazeEnabled(const LLUUID& actor_id) const;
    void   setGazeTargetMode(const LLUUID& actor_id, S32 mode);      // 0..3
    S32    getGazeTargetMode(const LLUUID& actor_id) const;
    void   setGazeCastTarget(const LLUUID& actor_id, const LLUUID& cast_id);
    LLUUID getGazeCastTarget(const LLUUID& actor_id) const;
    void   setGazePointGlobal(const LLUUID& actor_id, const LLVector3d& p);
    void   setGazeHeadEyeBlend(const LLUUID& actor_id, F32 v);       // 0 eyes-only .. 1 full
    F32    getGazeHeadEyeBlend(const LLUUID& actor_id) const;
    void   setGazeIntensity(const LLUUID& actor_id, F32 v);          // 0..1
    F32    getGazeIntensity(const LLUUID& actor_id) const;
    void   setGazeSmoothing(const LLUUID& actor_id, F32 v);          // 0 snappy .. 1 very smooth
    F32    getGazeSmoothing(const LLUUID& actor_id) const;
    // one-line status for the panel (e.g. "Looking at Kestrel", "Gaze off",
    // "Gaze armed -- starts with the walk"). Always fills out; returns false only
    // when the actor id is null.
    bool   getGazeStatus(const LLUUID& actor_id, std::string& out) const;

    // per-frame gaze paint: called from the avatar update AFTER updateMotions has
    // posed the skeleton, so the override layers on the current anim pose and is
    // re-asserted every frame (like applyOverride does for the root). A no-op for
    // any avatar with no gaze entry. Safe on missing joints / animesh (each joint
    // is guarded; it drives whatever of mHead/mNeck/mTorso/mEye* exist).
    void   applyGaze(LLVOAvatar* av);

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

    // ---- Pose/blocking ghosts: translucent REAL-avatar billboards -------------
    // Refresh each roster actor's cached impostor snapshot (a billboard image of
    // the actual rendered avatar) so renderHeadingPreview() can stamp it as a
    // translucent, camera-facing "ghost" at the actor's current position, its
    // planned destination, and every path node. Called ONCE per frame from the
    // main render loop at the SAME site as LLVOAvatar::updateImpostors(), so the
    // generateImpostor() re-render happens in the frame's dedicated impostor
    // context (correct viewport / matrix save-restore). No-op with zero cost
    // unless PathShowOnionSkin AND PathGhostUseImpostor are on AND a mover /
    // Director floater is open. Regenerates an actor's snapshot only when stale
    // (no texture yet, the camera swung far enough around the actor that the
    // frozen 2D card would misread, or a max age elapsed) and at most a small
    // budget of regens per frame, so it is never a per-frame full re-render.
    // Caveat (documented): the snapshot is a FROZEN 2D card of the CURRENT pose;
    // it is a blocking-preview aid, not a live skinned second body.
    void updateGhostImpostors();

    // Snapshot each ghosted actor's rigged draw batches while the frame's render
    // maps still hold WORLD-camera geometry. MUST be called from the main render
    // loop AFTER the world render but BEFORE render_ui() -- render_hud_attachments()
    // re-runs stateSort with the HUD camera and repopulates those maps, so reading
    // them at draw time (render_ui_3d) finds no world geometry whenever a HUD is
    // worn (the "HUD hides the ghost" bug). The model ghost draw then reads this
    // cache instead of the live maps. Zero cost / clears the cache when ghosts are
    // off. Cached LLDrawInfo* stay valid for the frame (owned by spatial groups).
    void collectGhostBatches();

    // One snapshotted rigged draw batch for the true-3D model ghost: the frame-
    // lifetime LLDrawInfo* plus the render pass it was collected FROM. The pass
    // is what tells the ghost draw how the real render treats the batch's alpha
    // (opaque / cutoff-masked / blended) -- an LLDrawInfo alone does not remember
    // which pass map it sat in, and drawing a masked hair sheet or a blended
    // clothing layer fully opaque is exactly the halo/fringe corruption the
    // styled clone suffered from.
    struct GhostBatch
    {
        LLDrawInfo* mInfo = nullptr;
        U32         mPass = 0;      // LLRenderPass::PASS_*_RIGGED it was collected from
    };

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
        // follower-only: this actor's own standing height above the (leader path)
        // ground, captured once on the first followed frame so B stands feet-on-
        // ground without per-frame pose jitter (same reasoning as node mRootAbove)
        F32       mFollowRootAbove = 0.f;
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

    // per-frame evaluation of a FOLLOWER: ride the leader's spline at the offset
    // arc position, face the leader's tangent, cadence-lock to the follower's own
    // ground speed. Holds gracefully (freezes in place) when the leader is not
    // actively path-walking. Called from advancePath when this actor follows.
    void   advanceFollower(LLVOAvatar* av, Move& mv, const Follow& f, F32 dt);

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

    // ---- P3 undo/redo: authored-only path snapshot + per-actor history ---------
    // Captures exactly the serialized/authored fields of a Path (never the arc
    // cache), so applying one back and rebuilding restores nodes + all per-node
    // data + path-wide params. An empty mNodes means "no path" (undo of the first
    // placement clears the path).
    struct PathState
    {
        std::vector<Waypoint> mNodes;
        F32    mSpeed        = 1.f;
        S32    mEndMode      = 0;
        F32    mTension      = 0.5f;
        F32    mEaseIn       = 0.f;
        F32    mEaseOut      = 0.f;
        S32    mArrivalFacingMode = 0;
        F32    mArrivalDir   = 0.f;
        LLUUID mArrivalTarget;
        bool   mGroundFollow = false;
        bool   mPitchToSlope = false;
        bool   mSyncToTake   = false;
        F32    mSyncLeadTrail = 0.f;
    };
    struct EditHistory { std::vector<PathState> mUndo, mRedo; };
    static const S32 UNDO_DEPTH = 30;

    PathState captureState(const LLUUID& key) const;   // authored state of mPaths[key] (empty if none)
    void      applyState(const LLUUID& key, const PathState& st);   // restore + rebuild + clamp edit node

    // ---- P3 gaze: per-actor look-at config + smoothing/ease runtime -----------
    // Authored fields are the knobs the panel writes; the runtime fields are the
    // envelope + smoothed direction the paint carries frame to frame. Kept in its
    // own map (session-only) so a no-gaze avatar is a pure map-miss no-op.
    struct Gaze
    {
        // authored (panel writes these)
        bool       mEnabled      = false;
        S32        mTarget       = 0;       // GAZE_TANGENT..GAZE_POINT
        LLUUID     mCastTarget;             // cast member (mode GAZE_CAST)
        LLVector3d mPoint;                  // fixed point, global (mode GAZE_POINT)
        F32        mHeadEyeBlend  = 0.7f;   // 0 = eyes only, 1 = full head+neck+torso
        F32        mIntensity     = 1.f;    // overall weight 0..1
        F32        mSmoothing     = 0.5f;   // 0 = snappy, 1 = very smooth
        // runtime (paint carries these; not authored)
        F32        mEnv           = 0.f;    // ease-in/out envelope 0..1
        bool       mDirValid      = false;  // mSmoothDir seeded yet
        LLVector3  mSmoothDir;              // smoothed world look direction from the head
        U32        mLastFrame     = 0xFFFFFFFF;  // per-frame temporal-advance guard
    };
    // per-frame gaze solve helpers (file-scope math lives in the cpp)
    void gazePaint(LLVOAvatar* av, Gaze& g, const Move* mv, F32 dt, bool advance);

    // ---- Pose-ghost impostor cache (session-only) -----------------------------
    // The pixel data itself lives in each actor's own LLVOAvatar::mImpostor
    // render target (we do not own a texture); this only tracks WHEN we last
    // refreshed that snapshot and from what camera angle, so the billboard stays
    // a stable frozen card until it is meaningfully stale, plus the billboard
    // centre height above the feet captured at that refresh (so a ghost stamped
    // at a foot position sits at the right height without a per-frame probe).
    struct GhostImpostor
    {
        bool      mValid           = false; // a usable snapshot has been captured
        F32       mLastGenTime     = 0.f;   // frame seconds at last generateImpostor
        LLVector3 mCamDir;                  // normalized actor->camera dir at capture
        F32       mCenterAboveFoot = 0.9f;  // billboard centre height above feet, m
    };
    std::map<LLUUID, GhostImpostor> mGhostImpostors;

    // Per-actor rigged draw batches for the true-3D model ghost, snapshotted by
    // collectGhostBatches() each frame while the world render maps are valid, and
    // consumed by the ghost draw in renderHeadingPreview(). Rebuilt every frame;
    // pointers are frame-lifetime only (never stored across frames). Keyed by the
    // WEARER: batches of an animesh attachment (rigged to its own LLControlAvatar
    // skeleton, so their mAvatar is the control avatar, not the wearer) are
    // bucketed under the wearing actor so the whole outfit ghosts as one body.
    std::map<LLUUID, std::vector<GhostBatch> > mGhostBatches;

    // is this actor's cached ghost snapshot stale (needs a regen)?
    static bool ghostImpostorStale(LLVOAvatar* av, const GhostImpostor& gi);

    std::map<LLUUID, Move> mMoves;
    std::map<LLUUID, Path> mPaths;      // authored path per actor (session-only)
    std::map<LLUUID, EditHistory> mHistory;   // per-actor bounded undo/redo stacks
    std::map<LLUUID, Follow> mFollows;  // follower key -> leader relationship (session-only)
    std::map<LLUUID, Gaze> mGazes;      // per-actor look-at config + runtime (session-only)

    // P2 edit selection: the path key under edit + the selected node index
    LLUUID mEditActor;      // resolved path key (null = nothing being edited)
    S32    mEditNode = -1;  // selected node in that path (-1 = none)
};

#endif // LL_LLACTORMOVER_H
