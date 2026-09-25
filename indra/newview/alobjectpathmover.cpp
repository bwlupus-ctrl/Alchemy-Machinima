/**
 * @file alobjectpathmover.cpp
 * @brief Client-side object path mover -- see alobjectpathmover.h and
 *        doc/OBJECT_PATHING.md.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"
#include "llpresentationtime.h"    // [Temporal Capture]

#include "alobjectpathmover.h"

#include "llactormover.h"           // Path model + evalPathSpeed/evalPathYawOffset
#include "llagent.h"                // agent <-> global conversion
#include "lldrawable.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"     // gObjectList
#include "pipeline.h"               // gPipeline.markMoved

#include <algorithm>

namespace
{
// resolve a roster id to a live, drawable-bearing ROOT object; null when the
// object is gone / not yet meshed. (The roster stores root ids, but re-check
// isRootEdit defensively: a relink while enrolled could demote the prim.)
LLViewerObject* resolve_object(const LLUUID& id)
{
    LLViewerObject* obj = gObjectList.findObject(id);
    if (!obj || obj->isDead() || obj->mDrawable.isNull() || !obj->isRootEdit())
    {
        return nullptr;
    }
    return obj;
}

// Re-assert the rendered transform of a linkset root this frame: position +
// rotation on the root, then the root AND every child drawable marked moved
// UNDAMPED. This is LLManip::rebuild's traversal minus its REBUILD_VOLUME --
// a pure move needs no geometry rebuild (the first move makes the drawables
// ACTIVE, after which they render through their transform), so the per-frame
// cost is the move-list processing only.
void place_linkset(LLViewerObject* obj, const LLVector3& pos_agent, const LLQuaternion& rot)
{
    obj->setPositionAgent(pos_agent);
    obj->setRotation(rot);
    gPipeline.markMoved(obj->mDrawable, /*damped*/ false);
    for (LLViewerObject* child : obj->getChildren())
    {
        if (child && child->mDrawable.notNull())
        {
            gPipeline.markMoved(child->mDrawable, /*damped*/ false);
        }
    }
}
} // anonymous namespace

// ---------------------------------------------------------------------------
ALObjectPathMover& ALObjectPathMover::instance()
{
    static ALObjectPathMover sInstance;
    return sInstance;
}

// ---------------------------------------------------------------------------
// roster
// ---------------------------------------------------------------------------
//static
void ALObjectPathMover::toggleTarget(const LLUUID& root_id)
{
    if (root_id.isNull())
    {
        return;
    }
    ALObjectPathMover& self = instance();
    auto it = std::find(self.mRoster.begin(), self.mRoster.end(), root_id);
    if (it != self.mRoster.end())
    {
        // leaving the roster stops the drive and drops the authored path --
        // the toggle IS the object's whole enrollment (unlike cast members,
        // props have no other identity to hang session state on)
        self.stop(root_id);
        LLActorMover::instance().clearPath(root_id);
        self.mEffects.erase(root_id);
        self.mRoster.erase(it);
    }
    else
    {
        self.mRoster.push_back(root_id);
    }
}

//static
bool ALObjectPathMover::isTarget(const LLUUID& root_id)
{
    const uuid_vec_t& r = instance().mRoster;
    return std::find(r.begin(), r.end(), root_id) != r.end();
}

// ---------------------------------------------------------------------------
// authoring
// ---------------------------------------------------------------------------
bool ALObjectPathMover::appendWaypointHere(const LLUUID& root_id)
{
    LLViewerObject* obj = resolve_object(root_id);
    if (!obj)
    {
        return false;
    }
    LLActorMover& mover = LLActorMover::instance();
    LLActorMover::Path& path = mover.editPath(root_id);
    LLActorMover::Waypoint wp;
    // root-centre, global: a car's path is authored at its own height, so the
    // avatar-walk fields (mRootAbove, ground snapping) stay zero/unused
    wp.mPosGlobal = gAgent.getPosGlobalFromAgent(obj->getPositionAgent());
    path.mNodes.push_back(wp);
    path.markDirty();
    LL_INFOS("ObjectPath") << "object " << root_id << ": node "
                           << path.mNodes.size() - 1 << " at "
                           << obj->getPositionAgent() << LL_ENDL;
    return true;
}

// ---------------------------------------------------------------------------
// orientation capture
// ---------------------------------------------------------------------------
//static
bool ALObjectPathMover::getWorldRotation(const LLUUID& root_id, LLQuaternion& out_rot)
{
    LLViewerObject* obj = resolve_object(root_id);
    if (!obj)
    {
        return false;
    }
    out_rot = obj->getRotationRegion();
    return true;
}

// ---------------------------------------------------------------------------
// oscillation / rock / spin config
// ---------------------------------------------------------------------------
ALObjectPathMover::Effects& ALObjectPathMover::editEffects(const LLUUID& root_id)
{
    return mEffects[root_id];  // default-constructs an all-off entry if absent
}

const ALObjectPathMover::Effects* ALObjectPathMover::getEffects(const LLUUID& root_id) const
{
    auto it = mEffects.find(root_id);
    return it != mEffects.end() ? &it->second : nullptr;
}

void ALObjectPathMover::clearEffects(const LLUUID& root_id)
{
    mEffects.erase(root_id);
}

// ---------------------------------------------------------------------------
// simple axis shuttle
// ---------------------------------------------------------------------------
bool ALObjectPathMover::buildAxisShuttle(const LLUUID& root_id, S32 axis, bool negative,
                                         F32 distance_m, F32 speed, F32 xz_slope_deg)
{
    LLViewerObject* obj = resolve_object(root_id);
    if (!obj)
    {
        return false;
    }

    LLVector3 dir;
    switch (axis)
    {
        case SHUTTLE_AXIS_X:  dir = LLVector3(1.f, 0.f, 0.f); break;
        case SHUTTLE_AXIS_Y:  dir = LLVector3(0.f, 1.f, 0.f); break;
        case SHUTTLE_AXIS_Z:  dir = LLVector3(0.f, 0.f, 1.f); break;
        case SHUTTLE_AXIS_XZ:
        default:
        {
            const F32 rad = (std::isfinite(xz_slope_deg) ? xz_slope_deg : 0.f) * DEG_TO_RAD;
            dir = LLVector3(cosf(rad), 0.f, sinf(rad));
            break;
        }
    }
    if (negative)
    {
        dir *= -1.f;
    }
    // guard: a degenerate axis vector (should not happen -- every case above is
    // already unit-length -- but never divide/normalize on faith)
    const F32 mag = dir.length();
    if (mag < 1.0e-5f)
    {
        return false;
    }
    dir *= (1.f / mag);

    distance_m = llmax(distance_m, 0.01f);
    speed = llmax(speed, 0.05f);

    LLActorMover& mover = LLActorMover::instance();
    LLActorMover::Path& path = mover.editPath(root_id);
    path.mNodes.clear();
    const LLVector3d origin_global = gAgent.getPosGlobalFromAgent(obj->getPositionAgent());
    LLActorMover::Waypoint node_a, node_b;
    node_a.mPosGlobal = origin_global;
    node_b.mPosGlobal = origin_global + LLVector3d(dir) * (F64)distance_m;
    path.mNodes.push_back(node_a);
    path.mNodes.push_back(node_b);
    // Force WAYPOINTS geometry: if the prop previously had a PRIMITIVE path,
    // syncPrimitiveNodes() would otherwise regenerate its nodes and clobber the
    // two shuttle endpoints we just placed (#7).
    path.mShape = ALPathGeometry::WAYPOINTS;
    path.mSpeed = speed;
    path.mEndMode = 2;      // ping-pong
    path.markDirty();

    LL_INFOS("ObjectPath") << "object " << root_id << ": shuttle path built ("
                           << distance_m << "m @ " << speed << "m/s)" << LL_ENDL;
    return start(root_id);
}

// ---------------------------------------------------------------------------
// transport
// ---------------------------------------------------------------------------
bool ALObjectPathMover::start(const LLUUID& root_id)
{
    LLViewerObject* obj = resolve_object(root_id);
    if (!obj)
    {
        return false;
    }
    LLActorMover& mover = LLActorMover::instance();
    const LLActorMover::Path* path = mover.getPath(root_id);
    if (!path || path->mNodes.size() < 2)
    {
        return false;
    }
    if (path->mDirty)
    {
        mover.editPath(root_id).rebuild();
        path = mover.getPath(root_id);
    }

    Drive drive;
    // alignment capture: tangent yaw at the path start vs the object's current
    // rotation. Driving applies rot = Rz(tangent_yaw + skid - start_tangent_yaw)
    // * R0, so a model built X-forward, Y-forward or askew keeps whatever
    // heading (and tilt) it had when the operator pressed start.
    LLVector3d pos, tangent;
    path->evalAtDistance(0.f, pos, tangent);
    // Vertical-tangent guard: a pure-Z path has a horizontal tangent of 0, where
    // atan2 is undefined -- hold 0 rather than let a signed-zero flip the facing.
    {
        const F32 tx = (F32)tangent.mdV[VX], ty = (F32)tangent.mdV[VY];
        drive.mStartTangentYaw = (tx * tx + ty * ty > 1e-8f) ? atan2f(ty, tx) : 0.f;
    }
    drive.mLastTangentYaw = drive.mStartTangentYaw;
    drive.mStartRot = obj->getRotationRegion();
    mDrives[root_id] = drive;
    LL_INFOS("ObjectPath") << "object " << root_id << ": driving "
                           << path->mTotalLength << " m path" << LL_ENDL;
    return true;
}

void ALObjectPathMover::stop(const LLUUID& root_id)
{
    // release only -- the object stays rendered where the drive left it until
    // the next server update for it arrives (documented prototype behavior;
    // there is no server-true pose to snap back to client-side)
    mDrives.erase(root_id);
}

void ALObjectPathMover::startAll()
{
    for (const LLUUID& id : mRoster)
    {
        start(id);
    }
}

void ALObjectPathMover::stopAll()
{
    mDrives.clear();
}

// ---------------------------------------------------------------------------
// per-frame drive
// ---------------------------------------------------------------------------
void ALObjectPathMover::update(F32 frame_dt)
{
    if (mDrives.empty())
    {
        return;     // default zero-cost path
    }

    // Freeze-world: hold BOTH the arc clock and the pose. Because this now runs
    // BEFORE gObjectList.update() (which still updates avatars while frozen),
    // advancing here would let a seated avatar consume a moved mount even though
    // the world is meant to be frozen. Returning early freezes mDist and the
    // placement together, so nothing jumps on unfreeze.
    if (LLPipeline::FreezeTime)
    {
        return;
    }

    LLActorMover& mover = LLActorMover::instance();
    // hitch cap (0.25s) preserved so a stall never teleports a prop. dt is the
    // caller's current-frame delta -- we can no longer read gFrameIntervalSeconds
    // here, since this runs before LLViewerObjectList::update() computes it.
    // [Temporal Capture] Objects drive: advance the prop's path arc on the
    // presentation clock (0x -> 0 holds the prop) so it slows with the world.
    F32 dt;
    if (LLPresentationTime::drives(LLTemporalFeature::OBJECTS))
    {
        // presentation-scaled, capped at the SAME 0.25s hitch cap the stock path
        // uses, so the path/ping-pong evaluator never gets an out-of-range step
        dt = llclamp((F32)LLPresentationTime::presentationDelta(), 0.f, 0.25f);
    }
    else
    {
        dt = llclamp(frame_dt, 0.f, 0.25f);   // stock hitch cap
    }

    for (auto it = mDrives.begin(); it != mDrives.end(); )
    {
        const LLUUID& id = it->first;
        Drive& drive = it->second;

        // an unresolvable object HOLDS its drive (derez/interest-list churn is
        // often transient; the drive resumes the frame it re-resolves). The
        // enrollment toggle is the deliberate way out.
        LLViewerObject* obj = resolve_object(id);
        if (!obj)
        {
            ++it;
            continue;
        }
        const LLActorMover::Path* path = mover.getPath(id);
        if (!path || path->mNodes.size() < 2)
        {
            it = mDrives.erase(it);     // path cleared out from under the drive
            continue;
        }
        if (path->mDirty)
        {
            mover.editPath(id).rebuild();
            path = mover.getPath(id);
        }
        const F32 total = llmax(path->mTotalLength, 0.01f);

        // [Oscillation/Spin] the effect clock runs whenever the drive exists,
        // INCLUDING while mArrived holds the prop at a stop-mode end -- a
        // hovering/spinning prop keeps hovering/spinning after it parks.
        drive.mEffectTime += dt;

        // ---- advance the arc clock (speed overrides + ease, walk-identical) --
        if (!drive.mArrived)
        {
            const bool loop     = path->mEndMode == 1;
            const bool pingpong = path->mEndMode == 2;
            const F32 speed = LLActorMover::evalPathSpeed(*path, drive.mDist,
                                                          loop || pingpong);
            drive.mDist += speed * dt * drive.mDir;
            if (loop)
            {
                while (drive.mDist >= total) { drive.mDist -= total; }
                while (drive.mDist < 0.f)   { drive.mDist += total; }
            }
            else if (pingpong)
            {
                if (drive.mDist >= total) { drive.mDist = total; drive.mDir = -1.f; }
                else if (drive.mDist <= 0.f) { drive.mDist = 0.f; drive.mDir = 1.f; }
            }
            else if (drive.mDist >= total)
            {
                drive.mDist = total;
                drive.mArrived = true;      // hold at the end, keep asserting
            }
        }

        // ---- evaluate + re-assert the rendered transform ---------------------
        LLVector3d pos_global, tangent;
        path->evalAtDistance(drive.mDist, pos_global, tangent);
        // travel direction flips on the return leg of a ping-pong
        if (drive.mDir < 0.f)
        {
            tangent = -tangent;
        }
        // Vertical-tangent guard (pure-Z shuttle): hold the last valid yaw so the
        // ping-pong return leg's atan2(-0,-0) = -pi cannot flip the prop 180 deg.
        F32 tangent_yaw = drive.mLastTangentYaw;
        {
            const F32 tx = (F32)tangent.mdV[VX], ty = (F32)tangent.mdV[VY];
            if (tx * tx + ty * ty > 1e-8f)
            {
                tangent_yaw = atan2f(ty, tx);
                drive.mLastTangentYaw = tangent_yaw;
            }
        }
        const F32 skid = LLActorMover::evalPathYawOffset(*path, drive.mDist);

        LLQuaternion delta;
        delta.setEulerAngles(0.f, 0.f, tangent_yaw + skid - drive.mStartTangentYaw);
        // FACE_PATH world rot = start rotation re-aimed by how far the tangent
        // has swung (+ the authored skid), so heading convention and tilt are
        // preserved. This is also the "tangent facing" half of
        // FACE_PATH_PLUS_OFFSET below -- both read this exact formula, so an
        // un-authored FACE_PATH_PLUS_OFFSET path is byte-identical to FACE_PATH.
        const LLQuaternion face_path_rot = drive.mStartRot * delta;

        // ---- [OrientMode] base rotation, selected by Path::mRotationMode -----
        // FACE_PATH (default, value 0) takes the unmodified branch below, so an
        // existing/untouched path renders byte-identically to before this
        // feature existed.
        LLQuaternion base_rot = face_path_rot;
        if (path->mRotationMode == LLActorMover::Path::OBJPATH_ROT_AUTHORED)
        {
            base_rot = LLActorMover::evalPathOrientation(*path, drive.mDist, /*as_offset*/ false);
        }
        else if (path->mRotationMode == LLActorMover::Path::OBJPATH_ROT_FACE_PATH_PLUS_OFFSET)
        {
            // Offset is a LOCAL rotation captured against PURE tangent facing
            // (Rz(tangent_yaw), no skid/start-alignment -- the same reference the
            // offset was factored against in pathOrientNodeValue). Apply it as
            // offset * pure_facing (LL: offset first). Using face_path_rot here
            // would re-apply the start heading and double the captured turn (#2).
            LLQuaternion pure_facing;
            pure_facing.setEulerAngles(0.f, 0.f, tangent_yaw);
            base_rot = LLActorMover::evalPathOrientation(*path, drive.mDist, /*as_offset*/ true) *
                       pure_facing;
        }

        // ---- [Oscillation] rock + [Spin]: extra LOCAL rotations, neutral
        // (identity) with no config or with every effect off -------------------
        LLQuaternion rock_rot;   // identity by default
        LLQuaternion spin_rot;   // identity by default
        bool         rot_fx = false;    // any local rotation effect active this frame
        F32 hover_z = 0.f;
        if (const Effects* fx = getEffects(id))
        {
            if (fx->mRock.mOn && fx->mRock.mAmpDeg != 0.f)
            {
                // Single axis-angle rotation about a normalized axis -- matches the
                // source LSL llAxisAngle2Rot(llVecNorm(<...>), angle), NOT stacked
                // Euler angles (which would give a larger, order-dependent tilt).
                const F32 angle = fx->mRock.mAmpDeg * DEG_TO_RAD *
                                  sinf(F_TWO_PI * fx->mRock.mFreqHz * drive.mEffectTime);
                LLVector3 axis = (fx->mRock.mAxis == ROCK_AXIS_XY)  ? LLVector3(1.f, 1.f, 0.f)
                               : (fx->mRock.mAxis == ROCK_AXIS_XYZ) ? LLVector3(1.f, 1.f, 1.f)
                               :                                      LLVector3::x_axis;
                axis.normalize();                       // setAngleAxis also normalizes; explicit
                rock_rot.setAngleAxis(angle, axis);
                rot_fx = true;
            }
            if (fx->mSpin.mOn && fx->mSpin.mDegPerSec != 0.f)
            {
                // mirrors the LSL continuous-spin idiom (llTargetOmega), but
                // driven by the same frame clock as everything else here
                // fmod to [0,2pi): rotation is 2pi-periodic, so wrapping is
                // invisible and keeps the angle from growing unbounded (and
                // losing float precision) over a long-running drive.
                const F32 angle = fmodf(fx->mSpin.mDegPerSec * DEG_TO_RAD * drive.mEffectTime, F_TWO_PI);
                const LLVector3 axis(fx->mSpin.mAxis == SPIN_AXIS_X ? 1.f : 0.f,
                                     fx->mSpin.mAxis == SPIN_AXIS_Y ? 1.f : 0.f,
                                     fx->mSpin.mAxis == SPIN_AXIS_Z ? 1.f : 0.f);
                spin_rot.setAngleAxis(angle, axis);
                rot_fx = true;
            }
            if (fx->mHover.mOn && fx->mHover.mAmpMeters != 0.f)
            {
                // mirrors the LSL hover/buildHoverSegment math (sine, or a
                // half-wave rectified sine for a "bounce" feel), continuous
                const F32 s = sinf(F_TWO_PI * fx->mHover.mFreqHz * drive.mEffectTime);
                hover_z = fx->mHover.mAmpMeters *
                          (fx->mHover.mWave == HOVER_WAVE_HALF ? fabsf(s) : s);
            }
        }

        // COMPOSE ORDER (LL a*b applies a FIRST; local rotations pre-multiply):
        //   final_rot = spin * rock * base_rot(mode)   -- spin innermost, so the
        //     model spins on its own axis and the whole spinning thing rocks in
        //     the UN-spun body frame (a steady wobble, not a precessing sweep).
        //   final_pos = path_pos + worldUpZ * hover
        // With no effects active the multiply is SKIPPED entirely (not just the
        // normalize): an identity*identity*base product can still flip signed-zero
        // bits, so gating the whole thing keeps the off path bit-for-bit identical
        // to today's FACE_PATH behavior. Normalize only guards F32 product drift.
        LLQuaternion rot = base_rot;
        if (rot_fx)
        {
            rot = spin_rot * rock_rot * base_rot;
            rot.normalize();
        }
        pos_global.mdV[VZ] += (F64)hover_z;

        place_linkset(obj, gAgent.getPosAgentFromGlobal(pos_global), rot);
        ++it;
    }
}
