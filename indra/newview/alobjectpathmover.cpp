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
    drive.mStartTangentYaw = atan2f((F32)tangent.mdV[VY], (F32)tangent.mdV[VX]);
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
        const F32 tangent_yaw = atan2f((F32)tangent.mdV[VY], (F32)tangent.mdV[VX]);
        const F32 skid = LLActorMover::evalPathYawOffset(*path, drive.mDist);

        LLQuaternion delta;
        delta.setEulerAngles(0.f, 0.f, tangent_yaw + skid - drive.mStartTangentYaw);
        // world rot = start rotation re-aimed by how far the tangent has swung
        // (+ the authored skid), so heading convention and tilt are preserved
        const LLQuaternion rot = drive.mStartRot * delta;

        place_linkset(obj, gAgent.getPosAgentFromGlobal(pos_global), rot);
        ++it;
    }
}
