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

    // ---- orientation capture (panel "Capture Orient") ----
    // resolves root_id and writes its LIVE world rotation to out_rot; false
    // when unresolvable. A thin public wrapper over the same resolve used
    // internally, so the panel never needs its own object lookup.
    static bool getWorldRotation(const LLUUID& root_id, LLQuaternion& out_rot);

    // ---- oscillation / rock / spin (session-only, additive, neutral by
    // default) ----
    // Config lives keyed by root id like mDrives, but in its OWN map: it
    // persists across Drive/stop (tuning a hover doesn't need re-entering every
    // time you hit Stop), and is dropped only when the prop leaves the roster
    // (toggleTarget), same lifetime as its path.
    enum EHoverWave { HOVER_WAVE_FULL = 0, HOVER_WAVE_HALF = 1 };
    enum ERockAxis  { ROCK_AXIS_X = 0, ROCK_AXIS_XY = 1, ROCK_AXIS_XYZ = 2 };
    enum ESpinAxis  { SPIN_AXIS_X = 0, SPIN_AXIS_Y = 1, SPIN_AXIS_Z = 2 };
    enum EShuttleAxis { SHUTTLE_AXIS_X = 0, SHUTTLE_AXIS_Y = 1, SHUTTLE_AXIS_Z = 2,
                        SHUTTLE_AXIS_XZ = 3 };

    struct Hover
    {
        bool mOn = false;
        F32  mAmpMeters = 0.2f;   // sine amplitude, m
        F32  mFreqHz = 0.25f;
        S32  mWave = HOVER_WAVE_FULL; // FULL = sine, HALF = |sine| (LSL half-wave hover)
    };
    struct Rock
    {
        bool mOn = false;
        S32  mAxis = ROCK_AXIS_X;  // which local axes the sine angle drives
        F32  mAmpDeg = 5.f;
        F32  mFreqHz = 0.25f;
    };
    struct Spin
    {
        bool mOn = false;
        S32  mAxis = SPIN_AXIS_Z;
        F32  mDegPerSec = 30.f;
    };
    struct Effects
    {
        Hover mHover;
        Rock  mRock;
        Spin  mSpin;
    };

    Effects&       editEffects(const LLUUID& root_id);        // creates a default (all-off) entry if absent
    const Effects* getEffects(const LLUUID& root_id) const;   // nullptr when none configured
    void           clearEffects(const LLUUID& root_id);

    // ---- simple axis shuttle ----
    // Builds a straight 2-node ping-pong path at the prop's CURRENT position
    // (replacing any existing path on it) and starts driving it -- a thin
    // convenience over appendWaypointHere()+editPath()+start() that reuses the
    // whole engine, so a shuttle inherits oscillation/spin/orientation/seated-
    // carry same as any authored path. False when the prop is unresolvable or
    // the axis selection is degenerate.
    bool buildAxisShuttle(const LLUUID& root_id, S32 axis, bool negative,
                          F32 distance_m, F32 speed, F32 xz_slope_deg);

    // ---- per-frame drive ----
    // called once from the main idle loop (llappviewer): advances every drive
    // and re-asserts the rendered transform. Zero cost with no drives.
    //
    // dt is the current frame's clamped delta, passed IN rather than fetched
    // from gFrameIntervalSeconds, because this now runs BEFORE
    // LLViewerObjectList::update() (which is where gFrameIntervalSeconds is
    // computed). Running early is deliberate: it lets the object move land in
    // this frame's object/drawable/seated-avatar/skeleton sequence instead of
    // one frame late, which was the source of the bone-lock camera jitter on
    // driven props.
    void update(F32 dt);

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
        // Last VALID tangent yaw, held when the tangent is vertical (a pure-Z
        // shuttle): atan2(0,0) is undefined and the ping-pong return leg's
        // atan2(-0,-0) = -pi would spuriously flip the prop 180 deg. Init to
        // mStartTangentYaw; only updated when the horizontal tangent is nonzero.
        F32          mLastTangentYaw = 0.f;
        // [Oscillation/Spin] elapsed drive time, s -- the clock hover/rock/spin
        // phase off. Own accumulator (not mDist/arc-length) so it keeps running
        // while mArrived holds the prop at a stop-mode end, and resets cleanly
        // on the next start() like mDist does. Frozen by the same FreezeTime
        // early-return that freezes mDist.
        F32          mEffectTime = 0.f;
    };
    std::map<LLUUID, Drive> mDrives;

    // [Oscillation/Spin] session-only per-prop effect config; see Effects above.
    std::map<LLUUID, Effects> mEffects;

    uuid_vec_t mRoster;
};

#endif // AL_ALOBJECTPATHMOVER_H
