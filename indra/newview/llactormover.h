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
        LLVector3d mPosGlobal;          // node position, global coords (foot/ground level)
        F32        mDwell = 0.f;        // seconds to hold at this node (0 = none)
        F32        mSpeedOverride = 0.f;// local ground speed, m/s (0 = use path speed)
        LLUUID     mAnim;               // per-node anim (null = use loco anim)
        F32        mGroundOffset = 0.f; // manual Z nudge at this node, m
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

    // test harness (chat commands, until the P2 editor exists): append a
    // ground-snapped waypoint at the actor's current rendered position.
    void        appendWaypointHere(const LLUUID& actor_id);

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
    };

    static F32 pathParam(const Move& mv, F32* face); // leg progress s, facing

    // per-frame evaluation of an active path traversal (called from
    // applyOverride once the path clock has advanced this frame)
    void   advancePath(LLVOAvatar* av, Move& mv, F32 dt);
    // resolve the ground Z (agent frame) under an actor for ground-follow:
    // a capped downward object raycast for stairs/prims, else terrain land
    // height when the actor sits near it, else the interpolated spline Z
    // (fallback_z) so an elevated actor the ray missed is not yanked down.
    static F32 resolveGroundZ(LLVOAvatar* av, const LLVector3& agent_pos,
                              F32 fallback_z, bool& out_hit);

    std::map<LLUUID, Move> mMoves;
    std::map<LLUUID, Path> mPaths;      // authored path per actor (session-only)
};

#endif // LL_LLACTORMOVER_H
