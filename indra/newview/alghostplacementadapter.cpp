/**
 * @file alghostplacementadapter.cpp
 * @brief Thin viewer adapter for ALGhostPlacementResolver -- see the header.
 *
 * The source code in this file is provided to you under the terms of the
 * GNU Lesser General Public License, version 2.1, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. Terms of the LGPL can be found in doc/LGPL-licence.txt
 * in this distribution, or online at http://www.gnu.org/licenses/lgpl-2.1.txt
 *
 */

#include "llviewerprecompiledheaders.h"

#include "alghostplacementadapter.h"

#include "llagent.h"
#include "llsurface.h"
#include "llsurfacepatch.h"
#include "llviewercamera.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llworld.h"

#include <cmath>
#include <limits>

using namespace ALGhostPlacementResolver;

namespace ALGhostPlacementAdapter
{

Request screenRequest(S32 x, S32 y, F64 work_plane_height_z,
                      F64 max_distance_meters)
{
    Request request;
    const LLVector3 camera_origin = LLViewerCamera::getInstance()->getOrigin();
    request.mRayOriginGlobal = gAgent.getPosGlobalFromAgent(camera_origin);
    request.mCameraPositionGlobal = request.mRayOriginGlobal;
    request.mRayDirection = gViewerWindow
        ? gViewerWindow->mouseDirectionGlobal(x, y)
        : LLVector3(0.f, 0.f, -1.f);
    request.mWorkPlaneHeightZ = work_plane_height_z;
    request.mMaxDistanceMeters = max_distance_meters;
    return request;
}

Probes realProbes(S32 x, S32 y)
{
    Probes probes;

    probes.mSurfaceProbe =
        [x, y](const LLVector3d&, const LLVector3&) -> SurfaceHit
    {
        SurfaceHit hit;
        if (!gViewerWindow)
        {
            return hit;
        }
        const LLPickInfo pick = gViewerWindow->pickImmediate(
            x, y, /*pick_transparent*/ false, /*pick_rigged*/ false);
        if (!pick.mPosGlobal.isExactlyZero() && pick.mPosGlobal.isFinite())
        {
            hit.mHit = true;
            hit.mPointGlobal = pick.mPosGlobal;
            hit.mNormal = pick.mNormal.isFinite() &&
                          pick.mNormal.lengthSquared() > 1.0e-8f
                ? pick.mNormal : LLVector3::z_axis;
        }
        return hit;
    };

    probes.mTerrainProbe = [](F64 global_x, F64 global_y) -> TerrainHit
    {
        TerrainHit hit;
        const LLVector3d column_global(global_x, global_y, 0.0);
        LLViewerRegion* region =
            LLWorld::getInstance()->getRegionFromPosGlobal(column_global);
        if (!region)
        {
            return hit;
        }
        // A region existing is NOT proof its terrain data has arrived --
        // LLSurface's height array starts at Z=0 everywhere until real patch
        // data streams in (see LLSurfacePatch::setHasReceivedData()). Without
        // this check an unloaded patch silently reports "flat ground at sea
        // level" instead of "no data", which both this probe's callers (the
        // resolver ladder) and the crowd's own per-slot terrain-conform loop
        // (alghoststudio.cpp) would otherwise treat as a real, trustworthy
        // height.
        const LLVector3 pos_region = region->getPosRegionFromGlobal(column_global);
        LLSurfacePatch* patch = region->getLand().resolvePatchRegion(pos_region);
        if (!patch || !patch->getHasReceivedData())
        {
            return hit;
        }
        LLVector3 agent = gAgent.getPosAgentFromGlobal(column_global);
        agent.mV[VZ] = LLWorld::getInstance()->resolveLandHeightAgent(agent);
        if (!std::isfinite((double)agent.mV[VZ]))
        {
            return hit;
        }
        hit.mHit = true;
        hit.mHeightZ =
            gAgent.getPosGlobalFromAgent(agent).mdV[VZ];
        return hit;
    };

    probes.mWaterHeightProbe = [](F64 global_x, F64 global_y) -> F64
    {
        const LLVector3d column_global(global_x, global_y, 0.0);
        LLViewerRegion* region =
            LLWorld::getInstance()->getRegionFromPosGlobal(column_global);
        if (!region)
        {
            return std::numeric_limits<F64>::infinity();
        }
        LLVector3 agent = gAgent.getPosAgentFromGlobal(column_global);
        agent.mV[VZ] = region->getWaterHeight();
        return gAgent.getPosGlobalFromAgent(agent).mdV[VZ];
    };

    probes.mLegalityProbe = [](const LLVector3d& point) -> LegalityResult
    {
        LegalityResult result;
        if (LLWorld::getInstance()->getRegionFromPosGlobal(point))
        {
            result.mOk = true;
            return result;
        }
        result.mOk = false;
        result.mReason = "The point falls outside any loaded region.";
        LLViewerRegion* fallback = gAgent.getRegion();
        if (fallback)
        {
            const LLVector3d origin = fallback->getOriginGlobal();
            const F64 width = (F64)fallback->getWidth();
            if (width > 0.0)
            {
                // LLViewerRegion::pointInRegionGlobal() rejects a region-local
                // coordinate >= mWidth (it is an exclusive upper bound), so
                // clamping to exactly origin+width still fails the re-check
                // this clamp exists to satisfy -- or worse, silently resolves
                // into a neighboring region instead. Land strictly inside.
                constexpr F64 kEdgeEpsilonMeters = 0.01;
                result.mCanClamp = true;
                result.mClampedPointGlobal = LLVector3d(
                    llclamp(point.mdV[VX], origin.mdV[VX],
                           origin.mdV[VX] + width - kEdgeEpsilonMeters),
                    llclamp(point.mdV[VY], origin.mdV[VY],
                           origin.mdV[VY] + width - kEdgeEpsilonMeters),
                    point.mdV[VZ]);
            }
        }
        return result;
    };

    return probes;
}

} // namespace ALGhostPlacementAdapter
