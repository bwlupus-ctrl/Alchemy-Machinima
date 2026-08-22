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
#include "m4math.h"         // frozen attachment matrices (GhostDrawParams)
#include "llghostcoverage.h" // [GhostDeferred] per-category clone coverage mask
#include "algazemotor.h"    // per-actor coordinated gaze motor state (spec 6A)

#include <map>
#include <vector>
#include <functional>       // [CloneFidelity] shared source-walk callbacks

class LLVOAvatar;
class LLDrawInfo;
class LLFace;
class LLCamera;     // [GhostDeferred] proxy-queue build/cull takes an explicit view
class LLSpatialGroup;   // [CloneFidelity] shared source-walk callback arg
class LLViewerObject;   // [CloneFidelity] shared source-walk callback arg
template <class T> class LLPointer;

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
        // [ObjectPath] authored yaw offset at this node, radians: the object
        // path mover ADDS this to the tangent facing as a drive passes the
        // node (fake a skid / drift through a corner; interpolated across the
        // bracketing nodes like the other per-node scalars). Ignored by the
        // avatar walk, whose facing is the turn-rate-smoothed tangent. Rides
        // PathState/undo automatically -- those copy Waypoint wholesale.
        F32        mYawOffset = 0.f;    // radians, + = counter-clockwise
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
    bool setNodeYawOffset(const LLUUID& actor_id, S32 index, F32 yaw_rad);  // [ObjectPath] skid

    // [ObjectPath] evaluation helpers shared with the object path mover, so an
    // object drive uses the EXACT speed math the avatar walk uses (per-node
    // overrides blended across nodes + ease in/out over the arc -- no parallel
    // implementation to drift) plus the interpolated per-node yaw offset.
    static F32 evalPathSpeed(const Path& path, F32 dist, bool skip_ease);
    static F32 evalPathYawOffset(const Path& path, F32 dist);

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
    // Tangent gaze is PAINTED only during an active, non-suspended Move. Camera,
    // cast and point gaze are locomotion-independent. It eases cleanly out when
    // inactive / derezzed / disabled, then stops touching joints entirely. With
    // no gaze configured for an avatar this is a single map-miss no-op (the walk /
    // AO plays byte-identically to today). Config is session-only, keyed by the
    // resolved actor id, and persists across stop/start like the path.
    //
    // Target modes: 0 = path tangent ("look where I'm going", DEFAULT), 1 =
    // camera, 2 = cast member, 3 = fixed point (global), 4 = object. An unresolvable cast
    // target / degenerate direction falls back to the path tangent gracefully.
    enum { GAZE_TANGENT = 0, GAZE_CAMERA = 1, GAZE_CAST = 2, GAZE_POINT = 3, GAZE_OBJECT = 4 };

    // Global render-pose ownership policy. The anatomical chain still decides
    // each joint's target; priority only decides how completely that target
    // replaces the animation pose at full acquire.
    enum EGazePriority
    {
        GAZE_PRIORITY_BLEND = 0,
        GAZE_PRIORITY_HEAD_EYES = 1,
        GAZE_PRIORITY_UPPER_BODY = 2,
        // [Machinima] Planted-spine "IK-like" scope: owns torso/neck/head/eyes
        // like Upper body but NEVER allocates a hips share and NEVER writes
        // mPelvis, so the base/legs stay put and only the spine leans toward the
        // target (Turn-body mode handles yaw beyond the planted reach). Resolved
        // via explicit predicates -- do NOT rely on numeric >= ordering, so this
        // higher value never accidentally implies pelvis ownership.
        GAZE_PRIORITY_PLANTED_SPINE = 3
    };

    // [Machinima] Pose composition: how an ALLOWED joint endpoint is built.
    // Orthogonal to ownership scope (which joints) and SL anim priority (may
    // veto the write). Replace is the exact current behavior; Additive overlays
    // the smallest gaze correction on top of the animation; Blend nlerps the
    // two local endpoints. Default Replace enters the current statements
    // verbatim, so the default path is byte-identical.
    enum EGazeComposition
    {
        GAZE_COMPOSE_REPLACE  = 0,
        GAZE_COMPOSE_ADDITIVE = 1,
        GAZE_COMPOSE_BLEND    = 2
    };

    // [Machinima] Spine-recruitment curve. Legacy fills the chain by hard
    // capacity order (default, byte-identical); Angle-ease drives an
    // angle-driven smootherstep lean of the spine (Planted spine only in v1).
    enum EGazeLeanCurve
    {
        GAZE_LEAN_LEGACY    = 0,
        GAZE_LEAN_ANGLE_EASE = 1
    };

    struct GazeTarget
    {
        // RELAXED is eyes-only: the eye-target combo's "Relaxed (eyes idle)"
        // choice. It carries no aim data -- gazePaint() skips fixation and
        // vergence for it and drives the eyes with micro-life alone. Never
        // sent as a HEAD target; setGazeTarget()/the head combo never
        // produce it.
        // NEAR_LENS ("just off camera") is also eyes-only: the eyes aim at the
        // SAME direction the head aims (the camera/head target) but with a
        // small, slowly-drifting off-lens offset (DirectorGazeNearLensDeg) so
        // they read as "almost contact but alive". It carries no aim data of
        // its own; the head keeps aiming at the camera normally.
        enum EMode { MOTION = 0, CAMERA = 1, CAST_MEMBER = 2, FIXED_POINT = 3, OBJECT = 4, RELAXED = 5, NEAR_LENS = 6 };
        EMode      mMode = MOTION;
        LLUUID     mCastRef;
        LLVector3d mFixedPoint = LLVector3d::zero;
        LLUUID     mObjectRef;
        // Per-subject expressive config. Neutral/default values preserve old
        // scenes; optional preset overrides use -1 as "inherit the base slider".
        F32        mPersonaDominance = 0.f;
        F32        mPersonaAffection = 0.f;
        F32        mPersonaAnxiety = 0.f;
        // Cue targets inherit the cast member's persona unless an external
        // author explicitly supplied persona fields for the cue.
        bool       mPersonaOverride = false;
        F32        mHeadEyeBlendOverride = -1.f;
        F32        mTorsoAmountOverride = -1.f;
        F32        mIntensityOverride = -1.f;
        F32        mSmoothingOverride = -1.f;
        bool       mEyelineOverride = false;
        F32        mEyelineYawDegOverride = 0.f;
        F32        mEyelinePitchDegOverride = 0.f;
        F32        mMicroLifeOverride = -1.f;
        S32        mBlinksOverride = -1; // -1 inherits the global switch
        F32        mVariationOverride = -1.f;
        F32        mBreakFrequencyOverride = -1.f;
        F32        mEaseAcquireOverride = -1.f;
        F32        mEaseReleaseOverride = -1.f;
        F32        mDeadZoneDegOverride = -1.f;
        F32        mBlinkRateScale = 1.f;
        F32        mVergenceScale = 1.f;
        // Additional per-actor overrides so no operator-edited row bleeds to
        // other cast members. -1 means "inherit the matching global control".
        F32        mCameraRollOverride = -1.f;   // else DirectorGazeCameraRoll
        F32        mExaggerateOverride = -1.f;    // else DirectorGazeExaggerate
        S32        mGazePriorityOverride = -1;    // ownership scope; else DirectorGazePriority
        // [Machinima] SL animation priority for the gaze-yield gate. NOTE the
        // sentinel differs from the others: -2 = inherit DirectorGazeAnimationPriority,
        // -1 = Legacy final (always apply, ignore anim priority), 0..6 = yield per
        // joint to animations of strictly HIGHER effective rotation priority.
        S32        mAnimPriorityOverride = -2;
        S32        mCameraModeOverride = -1;      // else DirectorLookAtCameraMode
        // [Machinima] Phase 2-5 structural overrides. Discrete fields use -1 =
        // inherit the matching global; continuous fields use -1.f = inherit.
        // All default to inherit so old scenes and default configs enter none
        // of the new math.
        S32        mCompositionOverride = -1;     // EGazeComposition; else DirectorGazeComposition
        F32        mCompositionMixOverride = -1.f;// Blend mix; else DirectorGazeCompositionMix
        F32        mChestShareOverride = -1.f;    // else DirectorGazeChestShare (Planted spine)
        // Limit profile: tri-state mode override (-1 inherit global,
        // 0 force legacy constants, 1 use this actor's custom profile) plus the
        // actor's own profile values (only consulted when the resolved mode==1).
        S32        mLimitProfileModeOverride = -1;
        ALGazeMath::AnatomicalLimitProfile mLimitProfile;
        // Lean curve: mode override + continuous threshold/softness/max, each
        // -1/-1.f = inherit the matching global.
        S32        mLeanCurveOverride = -1;
        F32        mLeanThresholdDegOverride = -1.f;
        F32        mLeanSoftnessDegOverride = -1.f;
        F32        mLeanMaxDegOverride = -1.f;
    };

    void        setGazeEnabled(const LLUUID& actor_id, bool on);
    bool        isGazeEnabled(const LLUUID& actor_id) const;
    void        setGazeTargetMode(const LLUUID& actor_id, S32 mode);      // 0..4
    S32         getGazeTargetMode(const LLUUID& actor_id) const;
    void        setGazeCastTarget(const LLUUID& actor_id, const LLUUID& cast_id);
    LLUUID      getGazeCastTarget(const LLUUID& actor_id) const;
    void        setGazePointGlobal(const LLUUID& actor_id, const LLVector3d& p);
    LLVector3d  getGazePointGlobal(const LLUUID& actor_id) const;
    void        setGazeObjectTarget(const LLUUID& actor_id, const LLUUID& object_id);
    LLUUID      getGazeObjectTarget(const LLUUID& actor_id) const;
    void        setGazeTargetConfig(const LLUUID& actor_id, const GazeTarget& target);
    // Preset-transition entry point: applies @target like setGazeTargetConfig,
    // but when DirectorGazeTransitionSec > 0 the CONTINUOUS params ease from
    // the actor's current effective (sentinel-resolved) values to the target's
    // effective values over that duration with the DirectorGazeEasing curve,
    // driven per-frame in applyGaze() on the presentation clock (so a blend
    // keeps running with the gaze panel closed and stays scrub-safe).
    // DISCRETE fields (mode, target refs, blinks on/off, eyeline flag,
    // priority, camera mode) switch immediately at transition start.
    // Duration 0 falls through to the legacy instant snap, byte-identical.
    void        setGazeTargetConfigBlended(const LLUUID& actor_id, const GazeTarget& target);
    GazeTarget  getGazeTargetConfig(const LLUUID& actor_id) const;
    void        setGazeHeadEyeBlend(const LLUUID& actor_id, F32 v);       // 0 eyes-only .. 1 full
    F32         getGazeHeadEyeBlend(const LLUUID& actor_id) const;
    void        setGazeTorsoAmount(const LLUUID& actor_id, F32 v);        // 0 still chest .. 1 full share
    F32         getGazeTorsoAmount(const LLUUID& actor_id) const;
    void        setGazeIntensity(const LLUUID& actor_id, F32 v);          // 0..1
    F32         getGazeIntensity(const LLUUID& actor_id) const;
    void        setGazeSmoothing(const LLUUID& actor_id, F32 v);          // 0 snappy .. 1 very smooth
    F32         getGazeSmoothing(const LLUUID& actor_id) const;
    void        setGazeEyelineOffset(const LLUUID& actor_id, F32 yaw_degrees,
                                     F32 pitch_degrees);
    F32         getGazeEyelineYaw(const LLUUID& actor_id) const;
    F32         getGazeEyelinePitch(const LLUUID& actor_id) const;
    // one-line status for the panel (e.g. "Looking at Kestrel", "Gaze off",
    // "Gaze armed -- starts with the walk"). Always fills out; returns false only
    // when the actor id is null.
    bool        getGazeStatus(const LLUUID& actor_id, std::string& out) const;

    // per-frame gaze paint: called from the avatar update AFTER updateMotions has
    // posed the skeleton, so the override layers on the current anim pose and is
    // re-asserted every frame (like applyOverride does for the root). A no-op for
    // any avatar with no gaze entry. Safe on missing joints / animesh (each joint
    // is guarded; it drives whatever of mHead/mNeck/mTorso/mEye* exist).
    void   applyGaze(LLVOAvatar* av);

    // Director Console camera-facing pose for selected REAL cast avatars. Restore the
    // prior frame's captured animation pose before updateMotions(), then call
    // applyDirectorLookAt() after it. A true return gives Director ownership of
    // look-at pose for the frame, so Actor Mover gaze must be skipped for that avatar.
    void   restoreDirectorLookAtPose(LLVOAvatar* av);
    bool   applyDirectorLookAt(LLVOAvatar* av);
    // Cast lifecycle hooks restore any outstanding pose and prune smoothing.
    void   clearDirectorLookAtRuntime(const LLUUID& avatar_id);
    void   clearAllDirectorLookAtRuntime();

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
    bool isDriving(const LLUUID& id) const;

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

    // ---- [GhostWalk] entity-clone RUNTIME REPLACEMENT migration ---------------
    // Every per-actor record here keys by the RUNTIME avatar id (path_key is
    // identity for a concrete id), and a Ghost Studio clone gets a brand-new
    // runtime UUID whenever it is refreshed (appearance re-pull) or recovered
    // after a despawn. Without a rekey the old walk lives on under the dead id:
    // it suspends via the unresolvable debounce, resolve() can never see that
    // id again, and neither the RESUME_NEAR auto-resume nor the Path tab can
    // ever reach it -- the walk (and the authored path with it) leaks forever.
    // Called by ALGhostStudio::onEntityRuntimeReplaced alongside its
    // Director-cast migration, with the studio's stable instance id so state
    // survives the despawned gap where no runtime id exists at all:
    //   old -> new (live refresh): rekey every id-keyed record -- the Move
    //     (whose suspend/resume state rides the struct wholesale), the
    //     authored Path, undo history, follow relationships (as follower AND
    //     as any other follower's leader), gaze config, edit selection -- and
    //     restart a live move's loco/dwell anim on the new body (the old body
    //     is destroyed right after this returns).
    //   old -> null (despawn, instance kept): records stay put -- the walk
    //     suspends under the old key -- and the key is PARKED under the
    //     stable instance id so the eventual replacement can claim it.
    //   null -> new (recover): the parked key migrates to the new runtime;
    //     the suspended walk then auto-resumes through the ordinary
    //     RESUME_NEAR test, because its anchor and the respawn position are
    //     both the authored Studio placement.
    //   removing_instance: drop the records outright (a removed instance's
    //     runtime uuid can never resolve again) and forget the parked key.
    // Real avatars never pass through here -- their id is permanent.
    void onActorRuntimeReplaced(const LLUUID& stable_id, const LLUUID& old_id,
                                const LLUUID& new_id, bool removing_instance);

    // in-world heading preview lines (called from render_ui_3d, same pass as
    // the debug beacons). Zero cost unless ActorMoverShowHeading is on AND
    // the Actor Mover floater is open.
    void renderHeadingPreview();

    // Lean editing overlay for ONE actor's path (spline + numbered node markers +
    // hover/selection highlight), called from render_ui_3d while the path-edit tool
    // is active so markers always show what is being marked -- from the first node --
    // independent of the Show-path toggle / roster / >=2-node gating above.
    void renderActorPathOverlay(const LLUUID& actor_id, bool editing, S32 hover_node = -1);

    // [GhostStudio] draw every enabled studio ghost instance (far-to-near)
    // through the model-ghost renderer with its per-instance placement, style
    // and FX. Called from render_ui_3d OUTSIDE the beacon/UI-visibility gate
    // and with no floater requirement: studio ghosts are SCENE DRESSING, not
    // an editing indicator -- they stay on screen while filming with the UI
    // hidden and every floater closed. Self-contained overlay state; zero
    // cost when the studio has no enabled instances.
    void renderStudioGhosts();

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

    // Snapshot each ghosted actor's rigged draw batches + non-rigged attachment
    // faces, once per frame from the main render loop. [R2-6] The harvest walks
    // each wanted wearer's attachment objects into their SPATIAL-GROUP DRAW
    // MAPS directly -- never the frame's cull results -- so ghosts keep
    // rendering while the source avatar is off-frame, occluded, or impostored
    // (the filming case: camera on the ghosts, actor outside the shot), and the
    // old HUD-stateSort timing hazard is gone by construction. Zero cost /
    // clears the caches when ghosts are off. Collected LLDrawInfo* / LLFace*
    // stay valid for the frame (owned by spatial groups / drawables) and are
    // never cached across frames.
    // [CloneFidelity] ONE read-only enumeration of a source avatar's ghost-
    // eligible geometry -- the attachment walk (skip HUD, root + children, skip
    // dead), deduped spatial groups, non-rigged static faces, and every rigged
    // draw-map entry across kRiggedPasses. Used by BOTH collectGhostBatches (its
    // callback keeps the existing VB+range dedup + bucketing) and the Clone
    // Fidelity Audit (its callbacks snapshot/compare), so the two can never drift
    // apart. Passes the OWNING LLPointer<LLDrawInfo> so the audit can pin it;
    // the collector still just takes draw_info.get(). Issues no draw calls.
    using ghost_rigged_source_cb_t =
        std::function<void(LLVOAvatar* wearer, LLSpatialGroup* group, U32 pass,
                           const LLPointer<LLDrawInfo>& draw_info)>;
    using ghost_static_source_cb_t =
        std::function<void(LLVOAvatar* wearer, LLViewerObject* object, LLFace* face)>;
    void walkGhostSourceGeometry(LLVOAvatar* avatar,
                                 const ghost_rigged_source_cb_t& rigged_cb,
                                 const ghost_static_source_cb_t& static_cb);

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

    // [GhostStudio] this frame's collected batches for a wearer (nullptr /
    // empty = the pipeline is not rendering that body this frame). Pointers
    // are frame-lifetime; callers must consume them immediately (the studio's
    // freeze snapshot copies palettes out, never the batch pointers).
    const std::vector<GhostBatch>* ghostBatchesFor(const LLUUID& wearer_id) const;

    // [GhostStudio/R2-2] one NON-RIGGED worn-attachment face for the ghost:
    // collars, jewelry, flexi tails -- static prims whose faces never enter
    // the rigged pass maps (LLFace::mAvatar is only ever set on RIGGED faces,
    // so the batch sweep cannot see them; this is why the clone lacked them).
    // Collected per wearer alongside the rigged batches and drawn with the
    // BASE (non-skinned) ghost shader under the same placement composed with
    // each face's own render matrix. Frame-lifetime, never cached.
    struct GhostStaticFace
    {
        LLFace* mFace = nullptr;
        LLUUID  mObjectId;          // owning object (frozen-matrix lookup key)
        S32     mAlphaKind = 0;     // 0 solid, 1 cutoff-mask, 2 blend (real-render semantics)
        F32     mCutoff = 0.f;      // mask cutoff when mAlphaKind == 1
        bool    mDoubleSided = false;   // GLTF double-sided (cull-faithful draw)
    };
    const std::vector<GhostStaticFace>* ghostStaticFacesFor(const LLUUID& wearer_id) const;

    // -----------------------------------------------------------------------
    // [GhostDeferred] Scene-lit clone proxy queue (P0 harness). A frame-local,
    // view-tagged list of the enabled LIVE studio ghosts to be submitted into the
    // DEFERRED pass (so clones get real lighting/shadows + mirror coverage instead
    // of the current post-tonemap overlay). P0 builds + culls + counts the queue
    // but issues NO draw calls; P1 fills the deferred submission. Records reference
    // the frame-local harvest (mGhostBatches/mGhostStaticFaces) -- never copies --
    // and are valid only for the frame they were built in (assert the stamp).
    // See doc/SCENE_LIT_CLONE_P0_IMPL_BLUEPRINT.md.
    // -----------------------------------------------------------------------
    enum class EGhostProxyPose : U8 { LIVE, FROZEN };

    // View identity for the queue so a mirror/probe pass can't consume a queue
    // built for the world view. (Longer term: a real render-view cookie.)
    enum : U32 { GHOST_VIEW_WORLD_MAIN = 1, GHOST_VIEW_TRANSITION = 2,
                 GHOST_VIEW_HERO_PROBE_BASE = 0x100 };

    struct GhostProxy
    {
        LLUUID mInstanceId;
        LLUUID mSourceId;
        LLUUID mWearerId;
        LLVOAvatar* mSourceAvatar = nullptr;        // non-owning; valid only for mFrameStamp
        const std::vector<GhostBatch>* mBatches = nullptr;          // frame-local refs
        const std::vector<GhostStaticFace>* mStaticFaces = nullptr; // (never copies)
        // Placement components: modelview = view * T(mFootAgent) * R(mRotation)
        //                       * S(mScale) * T(-mPivotFootAgent). (P1 builds the
        // GL matrix from these; P0 only needs them for the bounds transform.)
        LLVector3    mFootAgent;                     // ghost spot (agent space)
        LLQuaternion mRotation;
        F32          mScale = 1.f;
        LLVector3    mPivotFootAgent;                // source live foot (root - pelvisToFoot)
        // Agent-space AABB of the source anim-extents after the placement.
        LLVector3    mWorldBoundsCenter;
        LLVector3    mWorldBoundsHalfExtent;
        EGhostProxyPose mPose = EGhostProxyPose::LIVE;
        bool mFrustumVisible = false;
        U32  mFrameStamp = 0;
        U32  mViewStamp  = 0;
    };

    struct GhostProxyQueue
    {
        std::vector<GhostProxy> mProxies;
        U32 mFrameStamp = 0;
        U32 mViewStamp  = 0;
        const LLCamera* mCameraIdentity = nullptr;
        void clear()
        {
            mProxies.clear();
            mFrameStamp = 0;
            mViewStamp  = 0;
            mCameraIdentity = nullptr;
        }
    };

    struct GhostDeferredCounters
    {
        U32 mFrameStamp = 0;
        U64 mLiveInstancesConsidered = 0;
        U64 mFrozenInstancesRejected = 0;
        U64 mUnresolvedSources       = 0;
        U64 mProxiesBuilt            = 0;
        U64 mProxiesFrustumCulled    = 0;
        U64 mProxiesVisible          = 0;
        U64 mProxiesSubmitted        = 0;
        U64 mRiggedBatchesReferenced = 0;
        U64 mStaticFacesReferenced   = 0;
        U64 mIncompleteBounds        = 0;
        U64 mStaleQueueSkips         = 0;
        U64 mActualDrawCalls         = 0;   // MUST remain 0 throughout P0
        // [Coverage] per-category accounting: draw calls per category, plus how
        // many instances reached FULL category coverage this frame (the coverage
        // bit -- draw counts alone cannot prove suppression state). Slice 1
        // populates only the RIGGED_SOLID pair; the blend slice and static-face
        // slice fill in theirs when they land.
        U64 mRiggedSolidDrawCalls = 0;
        U64 mRiggedBlendDrawCalls = 0;
        U64 mRiggedGlowDrawCalls  = 0;
        U64 mStaticSolidDrawCalls = 0;
        U64 mStaticBlendDrawCalls = 0;
        U64 mRiggedSolidInstancesSubmitted = 0;
        U64 mRiggedBlendInstancesSubmitted = 0;
        U64 mRiggedGlowInstancesSubmitted  = 0;
        U64 mStaticSolidInstancesSubmitted = 0;
        U64 mStaticBlendInstancesSubmitted = 0;
        U64 mInvariantViolations     = 0;
        U64 mContaminationPass         = 0;
        U64 mContaminationFail         = 0;
        U64 mContaminationInconclusive = 0;
        void reset(U32 frame)
        {
            *this = GhostDeferredCounters();
            mFrameStamp = frame;
        }
    };

    // Build the frame-local proxy queue for `camera` (world view stamp). Enumerates
    // enabled LIVE studio ghosts, resolves sources, references their harvested
    // batches, computes placement bounds, frustum-culls, and updates counters.
    // Issues NO draw calls (P0). Call AFTER collectGhostBatches(), BEFORE the
    // deferred pass. Zero cost when no studio ghost is enabled.
    void buildGhostDeferredQueue(const LLCamera& camera, U32 view_stamp);
    // Validated accessor for the P1 deferred-submission consumer. Warns + asserts
    // (and counts) on a stale/mismatched queue; returns the queue regardless so a
    // caller that ignores the stamp still gets an empty/last queue, never garbage.
    const GhostProxyQueue& getGhostDeferredQueue(const LLCamera& camera, U32 expected_view_stamp) const;
    // [GhostDeferred] SILENT queue-validity probe for the post-deferred forward
    // pass, which runs from renderDeferredLighting -- also entered for reflection
    // probes / cube snapshots where no world queue was built. Same checks as the
    // strict accessor but NO warn / assert / stale-skip counter, so a probe frame
    // simply skips the ghost forward draw instead of spamming the log.
    bool hasValidGhostDeferredQueueThisFrame(const LLCamera& camera, U32 expected_view_stamp) const;
    GhostDeferredCounters& ghostDeferredCounters() { return mGhostDeferredCounters; }
    // Per-frame counter log (gated on GhostDeferredDebugLog). Call once per frame
    // AFTER the deferred submission so submitted/draw-call counts are final.
    void emitGhostDeferredDebug() const;

    // [GhostStudio] optional per-ghost placement + FX overrides for the model
    // ghost draw. Default-constructed = byte-identical to the classic path-node
    // ghost (pure translation, live pose, no FX). The placement composes
    //   T(ghost_foot) * R(mRotation) * S(mScale) * T(-pivot_foot)
    // into the modelview -- pivoting at the FOOT keeps scaled/rotated feet
    // planted on the ghost spot. The pivot is the source's live foot for LIVE
    // pose, or the capture-frame anchor when a frozen palette map is supplied
    // (the palettes bake vertices into the capture-time agent frame, so the
    // frame-matched anchor keeps placement correct across region crossings).
    struct GhostDrawParams
    {
        LLQuaternion mRotation;        // orientation about the foot (identity = source facing)
        F32        mScale = 1.f;        // uniform, pivoted at the foot
        bool       mHavePivot = false;  // use mPivotFootAgent (FROZEN) instead of the live foot
        LLVector3  mPivotFootAgent;     // capture-frame foot anchor (when mHavePivot)
        // frozen (drawing avatar id, skin hash) -> GL 3x4 palette; null = live
        const std::map<std::pair<LLUUID, U64>, std::vector<F32> >* mFrozenPalettes = nullptr;
        // [R2-2] frozen NON-RIGGED attachment placement: object id -> the
        // object's render matrix captured at freeze time (capture agent frame,
        // consistent with the palettes + pivot). Null / lookup miss = the
        // face's LIVE matrix (documented fallback; flexi always stays live --
        // its vertices are CPU-deformed in the shared buffer every frame).
        const std::map<LLUUID, LLMatrix4>* mFrozenAttachMats = nullptr;
        // cheap creative FX, each 0 = off (see actorghostF.glsl)
        F32        mShimmerSpeed = 0.f;     // Hz
        F32        mShimmerIntensity = 0.f; // 0..1
        F32        mPixelSize = 0.f;        // screen px (0 = off)
        F32        mGlitch = 0.f;           // 0..1 slice offset + chroma split
        S32        mDistort = 0;             // independent distortion selector
        F32        mDistortAmount = 0.5f;    // 0..1 selected distortion strength
        F32        mPhase = 0.f;            // per-instance phase so FX don't sync up
        F32        mEffectFps = 0.f;        // 0 smooth; 1..30 quantized ghostTime
        // [R2-1] output brightness multiplier (rides ghostFx.w; 1 = as-is).
        // The ghost is unlit in the post-tonemap overlay, so this is how a
        // clone sits into a night scene instead of glowing fullbright.
        F32        mBrightness = 1.f;       // 0.05..1.5
        // the tint is an operator-authored custom hue (Ghost Studio hue slider)
        // rather than the default actor identity color. The CLONE style ignores
        // the identity tint (a clone should match the avatar), but a custom hue
        // is an explicit art direction and gets mixed into the clone too.
        bool       mTintCustom = false;
        // [GhostDeferred] CLONE-only per-category coverage: which categories the
        // deferred pass drew this frame (mDeferredCoverage) vs which categories
        // the harvested geometry actually CONTAINS (mPresentCoverage). The draw
        // colors only present & ~covered; covered solids still depth-PRIME so the
        // uncovered translucent layers self-occlude against the body silhouette
        // (the UI-phase overlay has no world depth). Both NONE (the default) =
        // no coverage info -> the classic full draw, byte-identical.
        GhostCoverageMask mDeferredCoverage = GHOST_COVERAGE_NONE;
        GhostCoverageMask mPresentCoverage  = GHOST_COVERAGE_NONE;
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
    // [GhostWalk] runtime-replacement internals (see onActorRuntimeReplaced):
    // migrateActor() rekeys every id-keyed record old -> new (and restarts a
    // live move's anims on the new body); dropActor() erases an actor's
    // records outright with the same guarded anim stop cancelSuspended() uses.
    void   migrateActor(const LLUUID& old_id, const LLUUID& new_id);
    void   dropActor(const LLUUID& actor_id);
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
        S32        mTarget       = 0;       // GAZE_TANGENT..GAZE_OBJECT
        LLUUID     mCastTarget;             // cast member (mode GAZE_CAST)
        LLVector3d mPoint;                  // fixed point, global (mode GAZE_POINT)
        LLUUID     mObjectTarget;           // in-world object target (mode GAZE_OBJECT)
        F32        mPersonaDominance = 0.f;
        F32        mPersonaAffection = 0.f;
        F32        mPersonaAnxiety = 0.f;
        F32        mHeadEyeBlendOverride = -1.f;
        F32        mTorsoAmountOverride = -1.f;
        F32        mIntensityOverride = -1.f;
        F32        mSmoothingOverride = -1.f;
        bool       mEyelineOverride = false;
        F32        mEyelineYawDegOverride = 0.f;
        F32        mEyelinePitchDegOverride = 0.f;
        F32        mMicroLifeOverride = -1.f;
        S32        mBlinksOverride = -1;
        F32        mVariationOverride = -1.f;
        F32        mBreakFrequencyOverride = -1.f;
        F32        mEaseAcquireOverride = -1.f;
        F32        mEaseReleaseOverride = -1.f;
        F32        mDeadZoneDegOverride = -1.f;
        F32        mBlinkRateScale = 1.f;
        F32        mVergenceScale = 1.f;
        // Per-actor override mirrors of the matching global controls (-1 = inherit).
        // Consumed in gazePaint; keep gate-off byte-identical when left at -1.
        F32        mCameraRollOverride = -1.f;
        F32        mExaggerateOverride = -1.f;
        S32        mGazePriorityOverride = -1;
        // [Machinima] SL animation priority for the gaze-yield gate: -2 inherit
        // DirectorGazeAnimationPriority, -1 Legacy final, 0..6 selected priority.
        S32        mAnimPriorityOverride = -2;
        // [Machinima] Phase 2-5 structural override mirrors (see GazeTarget).
        // All inherit by default so gate-off / default output is byte-identical.
        S32        mCompositionOverride = -1;
        F32        mCompositionMixOverride = -1.f;
        F32        mChestShareOverride = -1.f;
        S32        mLimitProfileModeOverride = -1;
        ALGazeMath::AnatomicalLimitProfile mLimitProfile;
        S32        mLeanCurveOverride = -1;
        F32        mLeanThresholdDegOverride = -1.f;
        F32        mLeanSoftnessDegOverride = -1.f;
        F32        mLeanMaxDegOverride = -1.f;
        F32        mHeadEyeBlend  = 0.7f;   // 0 = eyes only, 1 = full head+neck+torso
        F32        mTorsoAmount   = 0.25f;  // torso share; 0 = still chest, 1 = full aim
        F32        mIntensity     = 1.f;    // overall weight 0..1
        F32        mSmoothing     = 0.5f;   // 0 = snappy, 1 = very smooth
        F32        mEyelineYawDeg = 0.f;    // actor-local off-lens trim, +/-15 degrees
        F32        mEyelinePitchDeg = 0.f;  // actor-local off-lens trim, +/-10 degrees
        // runtime (paint carries these; not authored)
        F32        mEnv           = 0.f;    // ease-in/out envelope 0..1
        bool       mDirValid      = false;  // mSmoothDir seeded yet
        LLVector3  mSmoothDir;              // smoothed world look direction from the head
        bool       mBodyAimValid  = false;  // held body aim seeded yet
        F32        mBodyAimPitch  = 0.f;    // accepted root-relative aim, radians
        F32        mBodyAimYaw    = 0.f;
        bool       mAppliedValid  = false;  // cone-clamped pose seeded yet
        bool       mAppliedSlewing = false; // traversing a large clamp seam
        F32        mAppliedPitch  = 0.f;    // actually applied head-chain aim
        F32        mAppliedYaw    = 0.f;
        F32        mTorsoAimPitch = 0.f;    // extra torso-only chase behind applied aim
        F32        mTorsoAimYaw   = 0.f;
        // [Machinima] behind-shoulder / no-snap runtime (see
        // resolveBehindShoulderChainYaw in llactormover.cpp). While the
        // target stays within the chain's physical yaw reach the resolver is
        // a bit-exact pass-through of the raw feed; these only engage once
        // the target goes beyond reach (behind the actor), where they hold
        // the committed shoulder across the +-180 seam and then sweep the
        // chain feed to the other shoulder through the FRONT, rate-limited.
        bool       mChainYawValid = false;  // mChainYaw seeded yet
        F32        mChainYaw      = 0.f;    // resolved no-snap chain-feed yaw
        F32        mBehindShoulderSign = 0.f; // +-1 committed shoulder beyond reach, 0 none
        bool       mBehindSweeping = false; // front sweep to the other shoulder in flight
        F32        mBehindEnv     = 1.f;    // smooth Release-policy weight
        U32        mLastFrame     = 0xFFFFFFFF;  // per-frame temporal-advance guard
        // Preset transition runtime (DirectorGazeTransitionSec / Easing).
        // Captures sentinel-RESOLVED endpoints for every continuous gaze
        // param when a preset is applied through the blended entry point and
        // eases between them on presentation dt in applyGaze(). Inactive by
        // default so the legacy snap path stays byte-identical.
        struct ParamTransition
        {
            // Continuous blended params, indices into mFrom/mTo. Discrete
            // fields (mode, refs, blinks, eyeline flag, priority, camera
            // mode) are never here -- they hard-switch at transition start.
            enum EParam
            {
                P_DOMINANCE = 0, P_AFFECTION, P_ANXIETY,
                P_HEAD_EYE, P_TORSO, P_INTENSITY, P_SMOOTHING,
                P_EYELINE_YAW, P_EYELINE_PITCH,
                P_MICRO_LIFE, P_VARIATION, P_BREAK_FREQ,
                P_EASE_ACQ, P_EASE_REL, P_DEAD_ZONE,
                P_BLINK_RATE, P_VERGENCE,
                P_CAMERA_ROLL, P_EXAGGERATE,
                P_COUNT
            };
            bool mActive   = false;
            F32  mElapsed  = 0.f;
            F32  mDuration = 0.f;
            U32  mEasing   = 2;              // DirectorGazeEasing at start
            // Once-per-frame advance guard OWN to the transition (distinct
            // from Gaze::mLastFrame, which gates the envelope advance): both
            // applyGaze() and the Director look-at path call the advance, and
            // whichever runs first in a frame wins without double-stepping.
            U32  mLastFrame = 0xFFFFFFFF;
            F32  mFrom[P_COUNT] = {};        // resolved effective, pre-commit
            F32  mTo[P_COUNT] = {};          // resolved effective, post-commit
            // The authored target this transition eases toward, raw sentinels
            // included: completion restores these override values exactly so
            // the settled state is byte-identical to a legacy snap, and
            // setGazeTargetConfig() uses it to recognize a re-snap of the
            // identical config (the cast mirror forwarding our own write).
            GazeTarget mAuthored;
        };
        ParamTransition mParamTransition;
        // Coordinated gaze motor state (spec 6A). Only touched on the
        // DirectorGazeMotionPrograms gate-on path; reset on release / runtime
        // replace so re-enabling re-initializes on the current target. The
        // legacy path never reads or writes it (gate-off stays byte-identical).
        ALGazeMotor::GazeMotorState mGazeMotor;
    };
    // ---- Preset transition helpers (see Gaze::ParamTransition) --------------
    // Resolve the CURRENT effective value of every continuous gaze param:
    // direct fields as-is, resolved runtime copies (head/eye blend, torso,
    // intensity, smoothing, eyeline) as-is, and override-or-global fields via
    // the same override>=0 rule gazePaint() consumes them with. A -1/INHERIT
    // sentinel is never used as a numeric endpoint.
    static void resolveGazeBlendValues(const Gaze& g,
                                       F32 out[Gaze::ParamTransition::P_COUNT]);
    // Write one blended frame of continuous values into the runtime Gaze,
    // with the same clamps setGazeTargetConfig() applies.
    static void writeGazeBlendValues(Gaze& g,
                                     const F32 v[Gaze::ParamTransition::P_COUNT]);
    // Land the transition: TO endpoint plus the authored raw override values
    // (sentinels restored) so the settled state matches a legacy snap exactly.
    static void finishGazeParamTransition(Gaze& g);
    // Raw advance: accumulate dt and ease/finish. Callers go through
    // advanceGazeParamTransition(), which owns the once-per-frame guard.
    void stepGazeParamTransition(Gaze& g, F32 dt);
    // Once-per-frame transition advance shared by BOTH per-frame gaze paths
    // (applyGaze and applyDirectorLookAt -- the Director path skips
    // applyGaze entirely, so it must tick the blend itself). Computes the
    // same presentation dt as the envelope advance; no-op when inactive or
    // when the other path already advanced this frame.
    void advanceGazeParamTransition(Gaze& g);
    // Director-path consumption: replace the CONTINUOUS fields the authored
    // @target sets with the current blended concrete values from @g, so a
    // Director-driven actor eases a preset change exactly like the
    // applyGaze() path instead of snapping. Inherited (-1) fields are left
    // alone and keep resolving through the Director base fallbacks.
    static void overlayGazeTransition(const Gaze& g, GazeTarget& target);
    struct DirectorJointPose
    {
        bool         mValid = false;
        LLQuaternion mRotation;
    };
    struct DirectorVisualParamPose
    {
        bool mValid = false;
        F32  mWeight = 0.f;
    };
    struct DirectorGaze
    {
        Gaze             mGaze;
        F32              mTargetStrength = 0.f;
        bool             mStrengthValid = false;
        S32              mMode = 0;
        DirectorJointPose mRoot;
        DirectorJointPose mPelvis;
        DirectorJointPose mTorso;
        DirectorJointPose mChest;   // planted-spine chest split (capture/restore beside mTorso)
        DirectorJointPose mNeck;
        DirectorJointPose mHead;
        DirectorJointPose mEyeLeft;
        DirectorJointPose mEyeRight;
        DirectorJointPose mAltEyeLeft;
        DirectorJointPose mAltEyeRight;
        DirectorVisualParamPose mBlinkLeft;
        DirectorVisualParamPose mBlinkRight;
        bool             mBodyYawValid = false;
        bool             mBodyTurnActive = false;
        F32              mBodyYaw = 0.f;
        U32              mBodyLastFrame = 0xFFFFFFFF;
        LLUUID           mTurnAnim;       // exact built-in/AO UUID Director started
        LLUUID           mTurnSourceAnim; // canonical TURNLEFT/RIGHT AO state
        bool             mOwnsTurnAnim = false;
        bool             mTurnAOOverrideActive = false;
        S32              mTurnPendingDirection = 0;
        F32              mTurnPendingSeconds = 0.f;
        F32              mTurnStopSeconds = 0.f;
        F32              mTurnRestartDelay = 0.f;
        bool             mAcquisitionValid = false;
        bool             mWasSelected = false;
        bool             mReactionPending = false;
        F64              mAcquireStartPresentation = 0.0;
        F32              mReactionDelay = 0.f;
        LLActorMover::GazeTarget mLastTarget;
        bool             mCueOverride = false;
        LLActorMover::GazeTarget mCueFromBaseTarget;
        LLActorMover::GazeTarget mCueFromTarget;
        F32              mCueFromTargetBlend = 1.f;
        F32              mCueTargetBlend = 1.f;
        F32              mCueEyeWeight = 1.f;
        F32              mCueHeadWeight = 1.f;
        F32              mCueBodyWeight = 1.f;
        F32              mCueLidWiden = 0.f;
        F32              mCueHeadRecoilPitch = 0.f;
        bool             mLastGateActive = false;
        U64              mLastGateCutSerial = 0;
    };
    // per-frame gaze solve helpers (file-scope math lives in the cpp)
    void gazePaint(LLVOAvatar* av, Gaze& g, const Move* mv, F32 dt, bool advance,
                   bool constrain_eye_cone, bool allow_natural_break,
                   DirectorGaze* director_runtime = nullptr,
                   F32 body_turn_threshold_deg = 90.f,
                   const GazeTarget* eye_target = nullptr,
                   // [Machinima] Goal 3b: keyframed influence multiplier on the
                   // FINAL write envelope. 1.f (the default / empty track) takes
                   // the exact legacy env assignments; a value < 1 scales the
                   // write so influence 0 performs NO gaze write (animation
                   // shows) while the aim/motor state stays warm.
                   F32 timeline_influence = 1.f);
    bool resolveGazeObjectCenter(const LLUUID& object_id, LLVector3& out_agent);
    void captureDirectorLookAtPose(LLVOAvatar* av, DirectorGaze& runtime,
                                   bool capture_blinks);
    void restoreDirectorLookAtPose(LLVOAvatar* av, DirectorGaze& runtime);
    bool applyDirectorBodyTurn(LLVOAvatar* av, DirectorGaze& runtime,
                               const LLVector3& target_direction);
    void updateDirectorTurnAnimation(LLVOAvatar* av, DirectorGaze& runtime,
                                     S32 direction);
    void stopDirectorTurnAnimation(LLVOAvatar* av, DirectorGaze& runtime);

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
    // [R2-2] per-wearer NON-RIGGED worn-attachment faces (collar/jewelry/flexi),
    // collected alongside the batches each frame; same lifetime rules
    std::map<LLUUID, std::vector<GhostStaticFace> > mGhostStaticFaces;

    // [GhostDeferred/P0] frame-local proxy queue + diagnostics counters, built by
    // buildGhostDeferredQueue() and (P1) consumed by the deferred submission.
    void computeProxyBounds(GhostProxy& proxy, LLVOAvatar* av);
    GhostProxyQueue mGhostDeferredQueue;
    // mutable: the const validated accessor counts stale-queue skips.
    mutable GhostDeferredCounters mGhostDeferredCounters;

    // is this actor's cached ghost snapshot stale (needs a regen)?
    static bool ghostImpostorStale(LLVOAvatar* av, const GhostImpostor& gi);

    std::map<LLUUID, Move> mMoves;
    // [GhostWalk] stable Ghost Studio instance id -> the runtime key whose walk
    // records are parked while that instance has NO live runtime (despawned).
    // Written/claimed/forgotten only by onActorRuntimeReplaced().
    std::map<LLUUID, LLUUID> mParkedWalkKeys;
    std::map<LLUUID, Path> mPaths;      // authored path per actor (session-only)
    std::map<LLUUID, EditHistory> mHistory;   // per-actor bounded undo/redo stacks
    std::map<LLUUID, Follow> mFollows;  // follower key -> leader relationship (session-only)
    std::map<LLUUID, Gaze> mGazes;      // per-actor look-at config + runtime (session-only)
    std::map<LLUUID, DirectorGaze> mDirectorGazes; // render-only real-cast camera gaze runtime
    struct GazeObjectCenterCache
    {
        U32 mFrame = 0xFFFFFFFF;
        bool mValid = false;
        LLVector3 mCenter;
    };
    std::map<LLUUID, GazeObjectCenterCache> mGazeObjectCenters;

    // P2 edit selection: the path key under edit + the selected node index
    LLUUID mEditActor;      // resolved path key (null = nothing being edited)
    S32    mEditNode = -1;  // selected node in that path (-1 = none)
};

#endif // LL_LLACTORMOVER_H
