/**
 * @file alghostplacementresolver.cpp
 * @brief Pure, dependency-injected placement-anchor resolver -- see the
 *        header for the full rationale.
 *
 * The source code in this file is provided to you under the terms of the
 * GNU Lesser General Public License, version 2.1, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. Terms of the LGPL can be found in doc/LGPL-licence.txt
 * in this distribution, or online at http://www.gnu.org/licenses/lgpl-2.1.txt
 *
 */

#include "linden_common.h"

#include "alghostplacementresolver.h"

#include <cmath>
#include <limits>

namespace ALGhostPlacementResolver
{
namespace
{
    constexpr F64 kDefaultMaxDistance = 128.0;
    // 96 coarse samples over [0, maxDistance] before bisecting the first
    // sign-changing interval. RESIDUAL LIMIT: this is still a finite-sample
    // march, not a true continuous ray cast -- a feature narrower than one
    // sample spacing that pokes across the ray and back again entirely
    // between two consecutive samples (both landing on the same side) is
    // missed, same as it would be for any bounded-sample heightfield probe.
    // What IS guaranteed: every sign change that occurs between two actually
    // sampled points is detected and bisected to a precise crossing, because
    // the check below compares every CONSECUTIVE pair of samples (i, i+1),
    // not just the scan's first and last endpoint.
    constexpr S32 kTerrainCoarseSamples = 96;
    constexpr S32 kTerrainBisectSteps = 40;

    bool finiteVec(const LLVector3d& v)
    {
        return std::isfinite(v.mdV[VX]) && std::isfinite(v.mdV[VY]) &&
               std::isfinite(v.mdV[VZ]);
    }

    bool finiteVec(const LLVector3& v)
    {
        return std::isfinite((double)v.mV[VX]) &&
               std::isfinite((double)v.mV[VY]) &&
               std::isfinite((double)v.mV[VZ]);
    }

    LLVector3d pointAt(const LLVector3d& origin, const LLVector3& dir, F64 t)
    {
        return LLVector3d(
            origin.mdV[VX] + (F64)dir.mV[VX] * t,
            origin.mdV[VY] + (F64)dir.mV[VY] * t,
            origin.mdV[VZ] + (F64)dir.mV[VZ] * t);
    }

    // Sanitizes the request into values the ladder can use unconditionally --
    // this is what makes the "always produces a finite candidate" invariant
    // hold even for garbage input, without the caller having to pre-validate.
    struct SanitizedRequest
    {
        LLVector3d mOrigin;
        LLVector3  mDir;           // finite, normalized, never the zero vector
        LLVector3d mCamera;
        F64        mPlaneZ = 0.0;
        F64        mMaxDistance = kDefaultMaxDistance;
        Policy     mPolicy;
    };

    SanitizedRequest sanitize(const Request& request)
    {
        SanitizedRequest out;
        out.mOrigin = finiteVec(request.mRayOriginGlobal)
            ? request.mRayOriginGlobal : LLVector3d();
        out.mCamera = finiteVec(request.mCameraPositionGlobal)
            ? request.mCameraPositionGlobal : out.mOrigin;
        out.mPlaneZ = std::isfinite(request.mWorkPlaneHeightZ)
            ? request.mWorkPlaneHeightZ : 0.0;
        out.mMaxDistance =
            (std::isfinite(request.mMaxDistanceMeters) &&
             request.mMaxDistanceMeters > 0.0)
                ? request.mMaxDistanceMeters : kDefaultMaxDistance;
        out.mPolicy = request.mPolicy;

        LLVector3 dir = request.mRayDirection;
        const F32 len = finiteVec(dir) ? dir.normalize() : 0.f;
        out.mDir = (std::isfinite((double)len) && len > 1.0e-6f)
            ? dir : LLVector3(0.f, 0.f, -1.f);
        return out;
    }

    // Ray-vs-height-surface: coarse sample the height difference
    // f(t) = P(t).z - heightProbe(x,y) over [0, maxDistance] looking for a
    // sign change between two CONSECUTIVE samples that both had coverage,
    // then bisect to refine. Returns false (no hit) when the ray never
    // crosses a surface it has coverage for within range -- the caller
    // falls through to the next rung, exactly like a render-pick miss used
    // to. Shared by intersectTerrain() and intersectWater() below (the
    // terrain probe and the water-height probe are both just "height at
    // (x,y), or no data").
    using HeightProbeFn = std::function<TerrainHit(F64, F64)>;

    bool intersectHeightSurface(const SanitizedRequest& req,
                                const HeightProbeFn& height_probe,
                                LLVector3d& out_point)
    {
        if (!height_probe)
        {
            return false;
        }

        struct Sample { F64 t = 0.0; F64 f = 0.0; bool hit = false; };
        Sample previous;
        bool have_previous = false;
        F64 bracket_lo = 0.0, bracket_hi = 0.0;
        F64 f_lo = 0.0, f_hi = 0.0;
        bool bracketed = false;

        for (S32 i = 0; i < kTerrainCoarseSamples && !bracketed; ++i)
        {
            const F64 t = req.mMaxDistance *
                ((F64)i / (F64)(kTerrainCoarseSamples - 1));
            const LLVector3d p = pointAt(req.mOrigin, req.mDir, t);
            const TerrainHit hit = height_probe(p.mdV[VX], p.mdV[VY]);
            Sample current;
            current.t = t;
            current.hit = hit.mHit && std::isfinite(hit.mHeightZ);
            current.f = p.mdV[VZ] - hit.mHeightZ;

            if (current.hit)
            {
                if (std::fabs(current.f) <= 1.0e-9)
                {
                    bracket_lo = bracket_hi = t;
                    f_lo = f_hi = current.f;
                    bracketed = true;
                }
                else if (have_previous && previous.hit &&
                         ((previous.f > 0.0) != (current.f > 0.0)))
                {
                    // Sign change against the IMMEDIATELY PRECEDING sample --
                    // every interval is checked, not just the scan's overall
                    // endpoints. See kTerrainCoarseSamples' comment for the
                    // still-residual sub-sample-width limit.
                    bracket_lo = previous.t;
                    bracket_hi = current.t;
                    f_lo = previous.f;
                    f_hi = current.f;
                    bracketed = true;
                }
            }
            previous = current;
            have_previous = true;
        }

        if (!bracketed)
        {
            return false;
        }

        F64 lo = bracket_lo, hi = bracket_hi;
        F64 flo = f_lo;
        for (S32 step = 0; step < kTerrainBisectSteps && hi - lo > 1.0e-6;
             ++step)
        {
            const F64 mid = 0.5 * (lo + hi);
            const LLVector3d p = pointAt(req.mOrigin, req.mDir, mid);
            const TerrainHit hit = height_probe(p.mdV[VX], p.mdV[VY]);
            if (!hit.mHit || !std::isfinite(hit.mHeightZ))
            {
                // Lost coverage mid-bisection (patchy region data) -- stop
                // refining and use the tightest bracket found so far.
                break;
            }
            const F64 fmid = p.mdV[VZ] - hit.mHeightZ;
            if ((fmid > 0.0) == (flo > 0.0))
            {
                lo = mid;
                flo = fmid;
            }
            else
            {
                hi = mid;
            }
        }

        const F64 t_final = 0.5 * (lo + hi);
        out_point = pointAt(req.mOrigin, req.mDir, t_final);
        // The bisected x,y may drift a hair from the last probed height; snap
        // Z onto the surface height at the final column for an exact point.
        const TerrainHit final_hit =
            height_probe(out_point.mdV[VX], out_point.mdV[VY]);
        if (final_hit.mHit && std::isfinite(final_hit.mHeightZ))
        {
            out_point.mdV[VZ] = final_hit.mHeightZ;
        }
        return true;
    }

    bool intersectTerrain(const SanitizedRequest& req, const Probes& probes,
                          LLVector3d& out_point)
    {
        return intersectHeightSurface(req, probes.mTerrainProbe, out_point);
    }

    // Ray-vs-water-plane, as its OWN bounded rung (not merely a Z-swap on a
    // terrain hit): needed both for oblique rays -- the point where a ray
    // crosses the water surface is generally NOT the same (x, y) as where it
    // crosses the terrain below, so swapping just Z would put the anchor off
    // the ray -- and for deep water whose seabed lies beyond maxDistance,
    // where intersectTerrain() never finds a crossing at all.
    bool intersectWater(const SanitizedRequest& req, const Probes& probes,
                        LLVector3d& out_point)
    {
        if (!probes.mWaterHeightProbe)
        {
            return false;
        }
        const WaterHeightProbe& water_probe = probes.mWaterHeightProbe;
        const HeightProbeFn wrapped =
            [&water_probe](F64 x, F64 y) -> TerrainHit
        {
            TerrainHit hit;
            const F64 z = water_probe(x, y);
            hit.mHit = std::isfinite(z);
            hit.mHeightZ = z;
            return hit;
        };
        return intersectHeightSurface(req, wrapped, out_point);
    }

    // Applies the surface-vs-seabed water policy to an ALREADY-RESOLVED
    // surface/terrain hit, consistently regardless of which rung produced
    // it (a render-pick VISIBLE_SURFACE hit and an independent TERRAIN ray
    // hit are treated exactly the same way here -- previously only the
    // terrain path consulted water at all). If the point is submerged and
    // seabed placement is not explicitly allowed, re-resolves the anchor to
    // the ray's own actual crossing of the water plane (never a bare Z-swap
    // on the original point, which would walk an oblique ray's anchor off
    // its own ray).
    ALGhostPlacementHit applyWaterPolicy(ALGhostPlacementHit hit,
                                         const SanitizedRequest& req,
                                         const Probes& probes)
    {
        if (!probes.mWaterHeightProbe)
        {
            return hit;
        }
        const F64 water_z = probes.mWaterHeightProbe(
            hit.mPointGlobal.mdV[VX], hit.mPointGlobal.mdV[VY]);
        if (!std::isfinite(water_z) || hit.mPointGlobal.mdV[VZ] >= water_z)
        {
            return hit;
        }
        // Submerged.
        if (req.mPolicy.mAllowSeabedPlacement)
        {
            if (hit.mWarning.empty())
            {
                hit.mWarning = "Placed on the seabed, below the water surface.";
            }
            return hit;
        }
        LLVector3d water_point;
        if (intersectWater(req, probes, water_point))
        {
            hit.mBasis = EGhostPlacementBasis::WATER_SURFACE;
            hit.mPointGlobal = water_point;
            hit.mSurfaceNormal = LLVector3::z_axis;
            hit.mWarning = "Terrain is below water; snapped to the surface.";
            return hit;
        }
        // Degenerate fallback: the ray never re-crosses the water plane
        // within range (e.g. it started already submerged, so there is no
        // "coming down through the surface" crossing to find). Keep the
        // same column and snap Z -- still strictly better than silently
        // reporting a seabed anchor as if it were the surface.
        hit.mBasis = EGhostPlacementBasis::WATER_SURFACE;
        hit.mPointGlobal.mdV[VZ] = water_z;
        hit.mSurfaceNormal = LLVector3::z_axis;
        hit.mWarning = "Terrain is below water; snapped to the surface.";
        return hit;
    }

    ALGhostPlacementHit applyLegality(ALGhostPlacementHit hit,
                                      const Probes& probes)
    {
        if (!probes.mLegalityProbe)
        {
            // No legality probe injected: the resolved anchor stands as-is.
            // (Every production adapter supplies one; tests that omit it are
            // deliberately exercising ladder-only behaviour.)
            hit.mValid = true;
            return hit;
        }

        LegalityResult legality = probes.mLegalityProbe(hit.mPointGlobal);
        if (legality.mOk)
        {
            hit.mValid = true;
            return hit;
        }

        const bool clampable_basis =
            hit.mBasis == EGhostPlacementBasis::WORK_PLANE ||
            hit.mBasis == EGhostPlacementBasis::CAMERA_DEPTH;
        if (clampable_basis && legality.mCanClamp &&
            finiteVec(legality.mClampedPointGlobal))
        {
            const LegalityResult reclamped =
                probes.mLegalityProbe(legality.mClampedPointGlobal);
            if (reclamped.mOk)
            {
                hit.mValid = true;
                hit.mPointGlobal = legality.mClampedPointGlobal;
                hit.mWarning = hit.mWarning.empty()
                    ? std::string("Anchor was outside the region; clamped "
                                   "back into legal space.")
                    : hit.mWarning + " Anchor was clamped into legal space.";
                return hit;
            }
            hit.mValid = false;
            hit.mWarning = reclamped.mReason.empty()
                ? "Placement is blocked here." : reclamped.mReason;
            return hit;
        }

        hit.mValid = false;
        hit.mWarning = legality.mReason.empty()
            ? "Placement is blocked here." : legality.mReason;
        return hit;
    }
} // namespace

ALGhostPlacementHit resolve(const Request& request, const Probes& probes)
{
    const SanitizedRequest req = sanitize(request);
    ALGhostPlacementHit hit;

    // Rung 1: walkable visible surface (render pick). The water policy is
    // applied to EVERY surface/terrain hit uniformly -- an underwater
    // render-pick hit (looking straight down through clear water onto the
    // seabed) is just as subject to the surface-vs-seabed policy as an
    // independent terrain ray hit is; it used to bypass it entirely.
    if (probes.mSurfaceProbe)
    {
        const SurfaceHit surface =
            probes.mSurfaceProbe(req.mOrigin, req.mDir);
        if (surface.mHit && finiteVec(surface.mPointGlobal))
        {
            hit.mBasis = EGhostPlacementBasis::VISIBLE_SURFACE;
            hit.mPointGlobal = surface.mPointGlobal;
            hit.mSurfaceNormal =
                finiteVec(surface.mNormal) && surface.mNormal.lengthSquared() > 1.0e-8f
                    ? surface.mNormal : LLVector3::z_axis;
            hit = applyWaterPolicy(hit, req, probes);
            return applyLegality(hit, probes);
        }
    }

    // Rung 2: terrain ray independent of rendered geometry, then the same
    // shared water policy.
    LLVector3d terrain_point;
    if (intersectTerrain(req, probes, terrain_point))
    {
        hit.mBasis = EGhostPlacementBasis::TERRAIN;
        hit.mPointGlobal = terrain_point;
        hit.mSurfaceNormal = LLVector3::z_axis;
        hit = applyWaterPolicy(hit, req, probes);
        return applyLegality(hit, probes);
    }

    // Rung 3: the water surface AS ITS OWN independent rung, still bounded
    // to maxDistance -- covers deep water whose seabed lies beyond range
    // (intersectTerrain never finds a crossing there) and open water with no
    // terrain coverage at all (well past any loaded region's land).
    LLVector3d water_point;
    if (intersectWater(req, probes, water_point))
    {
        hit.mBasis = EGhostPlacementBasis::WATER_SURFACE;
        hit.mPointGlobal = water_point;
        hit.mSurfaceNormal = LLVector3::z_axis;
        return applyLegality(hit, probes);
    }

    // Rung 4: horizontal work plane, bounded to the same max distance as
    // camera depth so a near-grazing ray intersecting kilometres away falls
    // through instead of producing a garbage anchor.
    if (std::fabs(req.mDir.mV[VZ]) > 1.0e-6f)
    {
        const F64 t_plane =
            (req.mPlaneZ - req.mOrigin.mdV[VZ]) / (F64)req.mDir.mV[VZ];
        if (std::isfinite(t_plane) && t_plane >= 0.0 &&
            t_plane <= req.mMaxDistance)
        {
            hit.mBasis = EGhostPlacementBasis::WORK_PLANE;
            hit.mPointGlobal = pointAt(req.mOrigin, req.mDir, t_plane);
            if (finiteVec(hit.mPointGlobal))
            {
                hit.mSurfaceNormal = LLVector3::z_axis;
                return applyLegality(hit, probes);
            }
            // Fall through to rung 5 on the (extreme-input) overflow case.
        }
    }

    // Rung 5: bounded camera-depth point. Always finite given a sanitized
    // request -- this is the ladder's last resort, never a failure. A final
    // guard still applies: extreme-but-finite inputs (e.g. an origin near
    // DBL_MAX) can overflow this addition to +-inf, and the "always
    // produces a finite candidate" contract must hold UNCONDITIONALLY, so
    // fall back to the already-sanitized camera position itself rather than
    // propagate a non-finite anchor.
    hit.mBasis = EGhostPlacementBasis::CAMERA_DEPTH;
    hit.mPointGlobal = pointAt(req.mCamera, req.mDir, req.mMaxDistance);
    if (!finiteVec(hit.mPointGlobal))
    {
        hit.mPointGlobal = req.mCamera;
    }
    hit.mSurfaceNormal = LLVector3::z_axis;
    return applyLegality(hit, probes);
}

const char* basisLabel(EGhostPlacementBasis basis)
{
    switch (basis)
    {
    case EGhostPlacementBasis::VISIBLE_SURFACE: return "Surface";
    case EGhostPlacementBasis::TERRAIN:         return "Terrain";
    case EGhostPlacementBasis::WATER_SURFACE:   return "Water";
    case EGhostPlacementBasis::WORK_PLANE:      return "Free-space plane";
    case EGhostPlacementBasis::CAMERA_DEPTH:    return "Camera depth";
    }
    return "Unknown";
}

} // namespace ALGhostPlacementResolver
