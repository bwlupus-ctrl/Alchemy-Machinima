/**
 * @file alobjectpathmover.h
 * @brief Client-side OBJECT path mover: drive a linked set along a spline.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * PROTOTYPE (see doc/OBJECT_PATHING.md for the evaluation + full design).
 * Puts a non-avatar OBJECT -- the ROOT of a linked set, e.g. a car -- on an
 * LLActorMover::Path so it drives itself CLIENT-SIDE: position from the
 * spline, base facing from the travel tangent, plus the per-node yaw offset
 * (Waypoint::mYawOffset -- fake a skid through a corner) and the per-node
 * speed overrides / ease the avatar walk already honors. The sim is never
 * told; only this client renders the motion (the Actor Mover's local-only
 * philosophy, applied to props).
 *
 * The mechanism (why this holds, from the evaluation): a parked, non-physical
 * object gets NO viewer-side interpolation (LLViewerObject::idleUpdate only
 * integrates when velocity/accel are nonzero and the object is non-static),
 * so a client-side setPositionAgent/setRotation + markMoved is not fought
 * per frame -- only a fresh server ObjectUpdate resets it, and this module
 * re-asserts every frame so a stray update loses for at most one frame. The
 * placement write is the LLManipTranslate idiom minus its geometry rebuild:
 * set root position/rotation, then mark the root AND every child drawable
 * moved UNDAMPED (children's world transforms derive from the root in
 * updateMove, which is how a local edit moves a whole linkset).
 *
 * Deliberately PARALLEL to the avatar Move machinery -- no shared Move state,
 * no invasive refactors; only the Path model and its arc-length/speed/yaw
 * evaluators are reused (LLActorMover::editPath keyed by the object's UUID,
 * evalPathSpeed / evalPathYawOffset). Roster + drives are session-only.
 *
 * Prototype UI: right-click an object > Director > "Path Object" toggles
 * roster membership; the /objpath* chat commands author and drive (the path
 * editor panel stays avatar-focused for now, per the prototype scope).
 */

#ifndef AL_ALOBJECTPATHMOVER_H
#define AL_ALOBJECTPATHMOVER_H

#include "lluuid.h"
#include "llquaternion.h"

#include <map>

class LLViewerObject;

class ALObjectPathMover
{
public:
    static ALObjectPathMover& instance();

    // ---- roster (session-only; always ROOT-prim ids) ----
    // toggled from the object context menu; the handler resolves the clicked
    // prim to its linkset root before calling, so children never enroll
    static void toggleTarget(const LLUUID& root_id);
    static bool isTarget(const LLUUID& root_id);
    const uuid_vec_t& getRoster() const { return mRoster; }

    // ---- authoring (chat-command harness for the prototype) ----
    // append a waypoint at the object's CURRENT root position (root-centre,
    // not ground-snapped -- a car's path is authored at axle height). False
    // when the object is unresolvable.
    bool appendWaypointHere(const LLUUID& root_id);

    // ---- transport ----
    // start needs a drivable path (>= 2 nodes); it captures the alignment
    // between the object's current rotation and the path tangent, so whatever
    // the model's forward-axis convention, it keeps its authored alignment
    // (and any tilt) while driving. stop releases the drive; the object stays
    // rendered wherever the drive left it until the next server update.
    bool start(const LLUUID& root_id);
    void stop(const LLUUID& root_id);
    void startAll();
    void stopAll();
    bool isDriving(const LLUUID& root_id) const { return mDrives.count(root_id) != 0; }

    // ---- per-frame drive ----
    // called once from the main idle loop (llappviewer, next to the actor
    // mover's state machine): advances every drive and re-asserts the
    // rendered transform. Zero cost with no drives.
    void update();

private:
    ALObjectPathMover() = default;

    struct Drive
    {
        F32          mDist = 0.f;       // arc-length traveled, m
        F32          mDir = 1.f;        // +1/-1 (ping-pong)
        bool         mArrived = false;  // settled at a stop-mode end (still re-asserted)
        // alignment captured at start: rotation the object needs ON TOP of the
        // tangent facing to keep its authored heading + tilt (rot = Rz(yaw
        // delta) * R0, where yaw delta = tangent yaw now - tangent yaw at start)
        LLQuaternion mStartRot;         // object world rotation at start
        F32          mStartTangentYaw = 0.f;    // path tangent yaw at start, radians
    };
    std::map<LLUUID, Drive> mDrives;

    uuid_vec_t mRoster;
};

#endif // AL_ALOBJECTPATHMOVER_H
