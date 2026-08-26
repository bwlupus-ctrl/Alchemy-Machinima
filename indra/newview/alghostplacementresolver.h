/**
 * @file alghostplacementresolver.h
 * @brief Pure, dependency-injected placement-anchor resolver shared by every
 *        Ghost Studio placement tool (crowd, single-ghost, hover/click/Enter).
 *
 * The source code in this file is provided to you under the terms of the
 * GNU Lesser General Public License, version 2.1, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. Terms of the LGPL can be found in doc/LGPL-licence.txt
 * in this distribution, or online at http://www.gnu.org/licenses/lgpl-2.1.txt
 *
 * BACKGROUND: every placement call site used to hand-roll its own
 * render-hit-only pick (LLPickInfo via pickImmediate). A sky/air miss meant
 * "no anchor at all", so open-space placement (over water, off in the void,
 * above a terrain dip the render pick skipped) was simply impossible, and the
 * three call sites disagreed with each other about what counted as a miss.
 *
 * This module is the ONE resolution ladder, testable with zero viewer
 * dependency: it takes a ray, a camera position, a work-plane height, a
 * bounded max distance, and a set of injected probe callbacks (render-surface
 * pick, terrain height, water height, parcel/region legality). A thin viewer
 * adapter -- deliberately kept OUT of this file, exactly like
 * ALGhostInteractionState's callers glue in pickImmediate/LLWorld themselves
 * -- supplies the real probes from altoolcrowdplace.cpp / altoolghostplace.cpp
 * / alghostinteractionstate call sites so this translation unit can be linked
 * into the isolated unit-test project with no viewer/render libraries.
 *
 * INVARIANT: for ANY finite-sanitized ray, the resolution ladder itself always
 * produces a finite candidate anchor (worst case: camera-depth). Only the
 * LEGALITY check can turn a resolved anchor into a block (mValid=false with a
 * concrete reason in mWarning) -- that is a "Blocked" status, distinct from
 * the old "no anchor at all".
 */
#ifndef AL_ALGHOSTPLACEMENTRESOLVER_H
#define AL_ALGHOSTPLACEMENTRESOLVER_H

#include "v3dmath.h"
#include "v3math.h"

#include <cstdint>
#include <functional>
#include <string>

namespace ALGhostPlacementResolver
{
    enum class EGhostPlacementBasis : U8
    {
        VISIBLE_SURFACE,
        TERRAIN,
        WATER_SURFACE,
        WORK_PLANE,
        CAMERA_DEPTH
    };

    struct ALGhostPlacementHit
    {
        bool mValid = false;
        EGhostPlacementBasis mBasis = EGhostPlacementBasis::WORK_PLANE;
        LLVector3d mPointGlobal;
        LLVector3 mSurfaceNormal = LLVector3::z_axis;
        std::string mWarning;
    };

    // ---- injected probe results --------------------------------------------

    // Render-hit-only pick (the old pickImmediate path), rung 1 of the ladder.
    struct SurfaceHit
    {
        bool mHit = false;
        LLVector3d mPointGlobal;
        LLVector3  mNormal = LLVector3::z_axis;
    };

    // Land height at a horizontal global position, independent of rendered
    // geometry (rung 2). mHit is false when the position has no loaded region
    // / no terrain data there at all.
    struct TerrainHit
    {
        bool mHit = false;
        F64  mHeightZ = 0.0;
    };

    // Parcel/region legality of a resolved anchor.
    struct LegalityResult
    {
        bool mOk = false;
        // Only meaningful when !mOk: whether the anchor can be pulled back
        // into legal region space and re-checked (used for plane/camera-depth
        // anchors that land out of region bounds).
        bool mCanClamp = false;
        LLVector3d mClampedPointGlobal;
        std::string mReason;
    };

    using SurfaceProbe = std::function<SurfaceHit(
        const LLVector3d& ray_origin_global, const LLVector3& ray_direction)>;
    // (global_x, global_y) -> land height at that column.
    using TerrainProbe = std::function<TerrainHit(F64 global_x, F64 global_y)>;
    // (global_x, global_y) -> water surface Z at that column.
    using WaterHeightProbe = std::function<F64(F64 global_x, F64 global_y)>;
    using LegalityProbe =
        std::function<LegalityResult(const LLVector3d& point_global)>;

    struct Probes
    {
        SurfaceProbe     mSurfaceProbe;
        TerrainProbe     mTerrainProbe;
        WaterHeightProbe mWaterHeightProbe;
        LegalityProbe    mLegalityProbe;
    };

    struct Policy
    {
        // false (default): terrain found below the water surface at that x,y
        // snaps to WATER_SURFACE with a warning instead of placing on the
        // seabed. true: seabed placement is allowed (TERRAIN basis, no snap).
        bool mAllowSeabedPlacement = false;
    };

    struct Request
    {
        LLVector3d mRayOriginGlobal;
        // Need not arrive normalized; sanitized (and defaulted if degenerate
        // or non-finite) inside resolve().
        LLVector3  mRayDirection = LLVector3(0.f, 0.f, -1.f);
        LLVector3d mCameraPositionGlobal;
        // Last-valid / source-foot height, global Z: the work plane the
        // ladder falls back to before camera depth.
        F64        mWorkPlaneHeightZ = 0.0;
        F64        mMaxDistanceMeters = 128.0;
        Policy     mPolicy;
    };

    // Pure resolution: runs the ladder (visible surface -> terrain [with
    // water-surface snap] -> bounded work plane -> bounded camera depth)
    // against the injected probes, then legality-checks the resolved anchor.
    // Never throws; never produces a non-finite mPointGlobal.
    ALGhostPlacementHit resolve(const Request& request, const Probes& probes);

    // Human-readable basis label for studio status text ("Surface" / "Terrain"
    // / "Water" / "Free-space plane" / "Camera depth").
    const char* basisLabel(EGhostPlacementBasis basis);

} // namespace ALGhostPlacementResolver

#endif // AL_ALGHOSTPLACEMENTRESOLVER_H
