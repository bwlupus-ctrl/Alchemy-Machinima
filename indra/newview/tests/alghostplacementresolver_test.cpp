/**
 * @file alghostplacementresolver_test.cpp
 * @brief Table-driven + property tests for the pure Ghost Studio placement
 *        resolver ladder.
 *
 * The source code in this file is provided to you under the terms of the
 * GNU Lesser General Public License, version 2.1, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. Terms of the LGPL can be found in doc/LGPL-licence.txt
 * in this distribution, or online at http://www.gnu.org/licenses/lgpl-2.1.txt
 *
 */

#include "linden_common.h"

#include "../test/lltut.h"

#include "../alghostplacementresolver.h"

#include <cmath>
#include <cstdint>

namespace tut
{
    using namespace ALGhostPlacementResolver;

    namespace
    {
        // Deterministic LCG -- no std::rand, per test-framework convention
        // (matches the seeded-noise style used elsewhere in the gaze suite).
        class Lcg
        {
        public:
            explicit Lcg(std::uint64_t seed) : mState(seed ? seed : 0x9E3779B97F4A7C15ull) {}

            F64 next01()
            {
                // Numerical Recipes LCG constants.
                mState = mState * 6364136223846793005ull + 1442695040888963407ull;
                const std::uint32_t hi = (std::uint32_t)(mState >> 32);
                return (F64)hi / (F64)0xFFFFFFFFu;
            }

            F64 range(F64 lo, F64 hi) { return lo + next01() * (hi - lo); }

        private:
            std::uint64_t mState;
        };

        LLVector3d point(F64 x, F64 y, F64 z) { return LLVector3d(x, y, z); }

        Request straightDown(F64 ox, F64 oy, F64 oz, F64 max_distance = 128.0)
        {
            Request request;
            request.mRayOriginGlobal = point(ox, oy, oz);
            request.mCameraPositionGlobal = request.mRayOriginGlobal;
            request.mRayDirection = LLVector3(0.f, 0.f, -1.f);
            request.mWorkPlaneHeightZ = 0.0;
            request.mMaxDistanceMeters = max_distance;
            return request;
        }

        // Flat terrain at a fixed height, hit everywhere.
        TerrainProbe flatTerrain(F64 height)
        {
            return [height](F64, F64) -> TerrainHit
            {
                TerrainHit hit;
                hit.mHit = true;
                hit.mHeightZ = height;
                return hit;
            };
        }

        TerrainProbe noTerrain()
        {
            return [](F64, F64) -> TerrainHit { return TerrainHit(); };
        }

        SurfaceProbe noSurface()
        {
            return [](const LLVector3d&, const LLVector3&) -> SurfaceHit
            {
                return SurfaceHit();
            };
        }

        WaterHeightProbe flatWater(F64 height)
        {
            return [height](F64, F64) -> F64 { return height; };
        }

        // Legal everywhere, no clamping ever needed.
        LegalityProbe alwaysLegal()
        {
            return [](const LLVector3d&) -> LegalityResult
            {
                LegalityResult result;
                result.mOk = true;
                return result;
            };
        }

        // Legal only within [-bound, bound] on x and y; outside, clampable
        // back onto the nearest edge and re-checked (always legal once
        // clamped, for this mock).
        LegalityProbe boundedRegion(F64 bound)
        {
            return [bound](const LLVector3d& p) -> LegalityResult
            {
                LegalityResult result;
                if (std::fabs(p.mdV[VX]) <= bound &&
                    std::fabs(p.mdV[VY]) <= bound)
                {
                    result.mOk = true;
                    return result;
                }
                result.mOk = false;
                result.mCanClamp = true;
                result.mReason = "outside region";
                result.mClampedPointGlobal = point(
                    llclamp((F32)p.mdV[VX], (F32)-bound, (F32)bound),
                    llclamp((F32)p.mdV[VY], (F32)-bound, (F32)bound),
                    p.mdV[VZ]);
                return result;
            };
        }

        // A no-rez parcel: never legal, never clampable.
        LegalityProbe neverLegal(const char* reason)
        {
            return [reason](const LLVector3d&) -> LegalityResult
            {
                LegalityResult result;
                result.mOk = false;
                result.mCanClamp = false;
                result.mReason = reason;
                return result;
            };
        }

        bool finitePoint(const LLVector3d& v)
        {
            return std::isfinite(v.mdV[VX]) && std::isfinite(v.mdV[VY]) &&
                   std::isfinite(v.mdV[VZ]);
        }

        // lltut's ensure_approximately_equals(F64, F64, U32 frac_bits) takes
        // an ULP/fractional-bit precision count, not a plain epsilon -- an
        // easy foot-gun (a literal like 1.0e-6 silently truncates to the U32
        // 0). Use an explicit epsilon compare instead so the tolerance here
        // means exactly what it says.
        bool approxEqual(F64 actual, F64 expected, F64 epsilon)
        {
            return std::fabs(actual - expected) <= epsilon;
        }
    }

    struct placement_resolver_data
    {
    };

    typedef test_group<placement_resolver_data> resolver_group;
    typedef resolver_group::object              resolver_object;
    tut::resolver_group g_ALGhostPlacementResolver("ALGhostPlacementResolver");

    // Rung 1: a walkable visible surface hit wins outright and is legality
    // checked but not touched by the terrain/plane/camera rungs.
    template<> template<>
    void resolver_object::test<1>()
    {
        Request request = straightDown(5.0, 5.0, 10.0);
        Probes probes;
        probes.mSurfaceProbe = [](const LLVector3d&, const LLVector3&) -> SurfaceHit
        {
            SurfaceHit hit;
            hit.mHit = true;
            hit.mPointGlobal = point(5.0, 5.0, 2.0);
            hit.mNormal = LLVector3::z_axis;
            return hit;
        };
        probes.mTerrainProbe = flatTerrain(0.0);
        probes.mLegalityProbe = alwaysLegal();

        const ALGhostPlacementHit result = resolve(request, probes);
        ensure("surface hit is valid", result.mValid);
        ensure("surface hit uses VISIBLE_SURFACE",
               result.mBasis == EGhostPlacementBasis::VISIBLE_SURFACE);
        ensure("surface hit keeps probe height",
            approxEqual(result.mPointGlobal.mdV[VZ], 2.0, 1.0e-9));
        ensure("surface hit carries no warning", result.mWarning.empty());
    }

    // Rung 2: a surface miss (sky/air) falls through to the independent
    // terrain ray -- this is the headline fix for the render-hit-only defect.
    template<> template<>
    void resolver_object::test<2>()
    {
        Request request = straightDown(0.0, 0.0, 50.0);
        Probes probes;
        probes.mSurfaceProbe = noSurface();
        probes.mTerrainProbe = flatTerrain(3.0);
        probes.mWaterHeightProbe = flatWater(-100.0);
        probes.mLegalityProbe = alwaysLegal();

        const ALGhostPlacementHit result = resolve(request, probes);
        ensure("terrain fallback is valid", result.mValid);
        ensure("surface miss resolves via TERRAIN",
               result.mBasis == EGhostPlacementBasis::TERRAIN);
        ensure("terrain fallback lands on land height",
            approxEqual(result.mPointGlobal.mdV[VZ], 3.0, 1.0e-6));
    }

    // Terrain below the water surface snaps to WATER_SURFACE with a warning
    // by default; the seabed policy opts back into TERRAIN with no snap.
    template<> template<>
    void resolver_object::test<3>()
    {
        Request request = straightDown(0.0, 0.0, 50.0);
        Probes probes;
        probes.mSurfaceProbe = noSurface();
        probes.mTerrainProbe = flatTerrain(-5.0);
        probes.mWaterHeightProbe = flatWater(0.0);
        probes.mLegalityProbe = alwaysLegal();

        const ALGhostPlacementHit snapped = resolve(request, probes);
        ensure("submerged terrain still resolves", snapped.mValid);
        ensure("submerged terrain snaps to water",
               snapped.mBasis == EGhostPlacementBasis::WATER_SURFACE);
        ensure("water snap uses water height",
            approxEqual(snapped.mPointGlobal.mdV[VZ], 0.0, 1.0e-9));
        ensure("water snap carries a warning", !snapped.mWarning.empty());

        request.mPolicy.mAllowSeabedPlacement = true;
        const ALGhostPlacementHit seabed = resolve(request, probes);
        ensure("seabed policy still resolves", seabed.mValid);
        ensure("seabed policy keeps TERRAIN basis",
               seabed.mBasis == EGhostPlacementBasis::TERRAIN);
        ensure("seabed policy lands on the seabed",
            approxEqual(seabed.mPointGlobal.mdV[VZ], -5.0, 1.0e-6));
    }

    // Rung 3: no surface, no terrain coverage anywhere -- falls to the
    // bounded horizontal work plane.
    template<> template<>
    void resolver_object::test<4>()
    {
        Request request = straightDown(2.0, -3.0, 20.0);
        request.mWorkPlaneHeightZ = 1.5;
        Probes probes;
        probes.mSurfaceProbe = noSurface();
        probes.mTerrainProbe = noTerrain();
        probes.mLegalityProbe = alwaysLegal();

        const ALGhostPlacementHit result = resolve(request, probes);
        ensure("no-land falls back to the work plane", result.mValid);
        ensure("no-land uses WORK_PLANE",
               result.mBasis == EGhostPlacementBasis::WORK_PLANE);
        ensure("plane hit keeps x",
            approxEqual(result.mPointGlobal.mdV[VX], 2.0, 1.0e-6));
        ensure("plane hit keeps y",
            approxEqual(result.mPointGlobal.mdV[VY], -3.0, 1.0e-6));
        ensure("plane hit lands on the work plane",
            approxEqual(result.mPointGlobal.mdV[VZ], 1.5, 1.0e-6));
    }

    // Rung 3->4: a near-grazing ray whose plane intersection lies beyond the
    // bounded max distance must fall through to camera depth rather than
    // producing a garbage kilometres-away anchor.
    template<> template<>
    void resolver_object::test<5>()
    {
        Request request;
        request.mRayOriginGlobal = point(0.0, 0.0, 10.0);
        request.mCameraPositionGlobal = request.mRayOriginGlobal;
        // Nearly horizontal: nudges toward the plane extremely slowly.
        request.mRayDirection = LLVector3(1.f, 0.f, -0.0001f);
        request.mWorkPlaneHeightZ = 0.0;
        request.mMaxDistanceMeters = 64.0;
        Probes probes;
        probes.mSurfaceProbe = noSurface();
        probes.mTerrainProbe = noTerrain();
        probes.mLegalityProbe = alwaysLegal();

        const ALGhostPlacementHit result = resolve(request, probes);
        ensure("grazing ray still resolves", result.mValid);
        ensure("grazing ray falls through to camera depth",
               result.mBasis == EGhostPlacementBasis::CAMERA_DEPTH);
        ensure("camera-depth point stays finite",
               finitePoint(result.mPointGlobal));
        const F64 distance =
            (result.mPointGlobal - request.mCameraPositionGlobal).length();
        ensure("camera-depth point sits at max distance",
            approxEqual(distance, 64.0, 1.0e-3));
    }

    // A ray pointing away from the work plane (behind-camera intersection,
    // t < 0) must also fall through instead of reporting a negative-t hit.
    template<> template<>
    void resolver_object::test<6>()
    {
        Request request;
        request.mRayOriginGlobal = point(0.0, 0.0, -10.0);
        request.mCameraPositionGlobal = request.mRayOriginGlobal;
        // Pointing further down/away: the z=0 plane is behind the origin.
        request.mRayDirection = LLVector3(0.f, 0.f, -1.f);
        request.mWorkPlaneHeightZ = 0.0;
        request.mMaxDistanceMeters = 32.0;
        Probes probes;
        probes.mSurfaceProbe = noSurface();
        probes.mTerrainProbe = noTerrain();
        probes.mLegalityProbe = alwaysLegal();

        const ALGhostPlacementHit result = resolve(request, probes);
        ensure("behind-camera plane still resolves", result.mValid);
        ensure("behind-camera plane falls through to camera depth",
               result.mBasis == EGhostPlacementBasis::CAMERA_DEPTH);
        ensure("behind-camera result stays finite",
               finitePoint(result.mPointGlobal));
    }

    // Legality: a no-rez parcel blocks even a perfectly good surface hit,
    // with a concrete reason and mValid=false (Blocked, not "no anchor").
    template<> template<>
    void resolver_object::test<7>()
    {
        Request request = straightDown(5.0, 5.0, 10.0);
        Probes probes;
        probes.mSurfaceProbe = [](const LLVector3d&, const LLVector3&) -> SurfaceHit
        {
            SurfaceHit hit;
            hit.mHit = true;
            hit.mPointGlobal = point(5.0, 5.0, 2.0);
            return hit;
        };
        probes.mLegalityProbe = neverLegal("This parcel does not allow building.");

        const ALGhostPlacementHit result = resolve(request, probes);
        ensure("no-rez parcel blocks placement", !result.mValid);
        ensure("no-rez parcel keeps the resolved basis",
               result.mBasis == EGhostPlacementBasis::VISIBLE_SURFACE);
        ensure_equals("no-rez parcel reports the concrete reason",
            result.mWarning, std::string("This parcel does not allow building."));
    }

    // Legality: an out-of-bounds plane/camera-depth anchor gets clamped back
    // into legal region space and re-checked, becoming valid with a warning.
    template<> template<>
    void resolver_object::test<8>()
    {
        Request request = straightDown(500.0, 500.0, 20.0);
        request.mWorkPlaneHeightZ = 0.0;
        Probes probes;
        probes.mSurfaceProbe = noSurface();
        probes.mTerrainProbe = noTerrain();
        probes.mLegalityProbe = boundedRegion(256.0);

        const ALGhostPlacementHit result = resolve(request, probes);
        ensure("out-of-bounds plane hit clamps into legality", result.mValid);
        ensure("clamped anchor stays within the legal bound",
            std::fabs(result.mPointGlobal.mdV[VX]) <= 256.0 + 1.0e-6 &&
            std::fabs(result.mPointGlobal.mdV[VY]) <= 256.0 + 1.0e-6);
        ensure("clamp is reported as a warning", !result.mWarning.empty());

        // A surface/terrain basis is NOT eligible for clamping -- only
        // plane/camera-depth anchors are, per the resolver's contract.
        Request surface_request = straightDown(500.0, 500.0, 10.0);
        Probes surface_probes = probes;
        surface_probes.mSurfaceProbe =
            [](const LLVector3d&, const LLVector3&) -> SurfaceHit
        {
            SurfaceHit hit;
            hit.mHit = true;
            hit.mPointGlobal = point(500.0, 500.0, 2.0);
            return hit;
        };
        const ALGhostPlacementHit unclamped =
            resolve(surface_request, surface_probes);
        ensure("out-of-bounds surface hit is not silently clamped",
               !unclamped.mValid);
    }

    // basisLabel() covers every enumerator with a distinct, non-empty label
    // for the studio status line.
    template<> template<>
    void resolver_object::test<9>()
    {
        ensure_equals("surface label", std::string(basisLabel(
            EGhostPlacementBasis::VISIBLE_SURFACE)), std::string("Surface"));
        ensure_equals("terrain label", std::string(basisLabel(
            EGhostPlacementBasis::TERRAIN)), std::string("Terrain"));
        ensure_equals("water label", std::string(basisLabel(
            EGhostPlacementBasis::WATER_SURFACE)), std::string("Water"));
        ensure_equals("plane label", std::string(basisLabel(
            EGhostPlacementBasis::WORK_PLANE)),
            std::string("Free-space plane"));
        ensure_equals("camera label", std::string(basisLabel(
            EGhostPlacementBasis::CAMERA_DEPTH)),
            std::string("Camera depth"));
    }

    // Non-finite/degenerate input (NaN direction, infinite plane height,
    // non-finite camera position) can never propagate into the resolved
    // anchor -- the ladder sanitizes and still produces a finite result.
    template<> template<>
    void resolver_object::test<10>()
    {
        const F64 nan = std::numeric_limits<F64>::quiet_NaN();
        const F32 inf = std::numeric_limits<F32>::infinity();
        Request request;
        request.mRayOriginGlobal = point(1.0, nan, 3.0);
        request.mCameraPositionGlobal = point(nan, nan, nan);
        request.mRayDirection = LLVector3(inf, 0.f, 0.f);
        request.mWorkPlaneHeightZ = std::numeric_limits<F64>::infinity();
        request.mMaxDistanceMeters = nan;
        Probes probes;
        probes.mSurfaceProbe = noSurface();
        probes.mTerrainProbe = noTerrain();
        probes.mLegalityProbe = alwaysLegal();

        const ALGhostPlacementHit result = resolve(request, probes);
        ensure("non-finite input still resolves", result.mValid);
        ensure("non-finite input yields a finite point",
               finitePoint(result.mPointGlobal));
    }

    // Property sweep: a few hundred deterministic pseudo-random rays (seeded
    // LCG, no real rand) against a mixed surface/terrain/water/legality mock.
    // Every result must be finite and either valid or carry a concrete block
    // reason -- never a silent "no anchor". The fixture cycles through four
    // engineered sub-populations (surface / terrain-or-water / work-plane /
    // camera-depth) so all five EGhostPlacementBasis values are actually
    // exercised with the fixed seed -- a purely uniform-random domain made
    // VISIBLE_SURFACE (a tiny platform box) vanishingly unlikely to ever be
    // sampled, and every ray with any downward component was catching the
    // (unbounded-looking) water probe before WORK_PLANE/CAMERA_DEPTH ever
    // got a turn.
    template<> template<>
    void resolver_object::test<11>()
    {
        constexpr F64 kTerrainCoverageBound = 400.0;
        Probes probes;
        probes.mSurfaceProbe = [](const LLVector3d& origin,
                                  const LLVector3&) -> SurfaceHit
        {
            // "Visible surface" directly beneath a small platform box.
            SurfaceHit hit;
            if (std::fabs(origin.mdV[VX]) < 1.0 &&
                std::fabs(origin.mdV[VY]) < 1.0)
            {
                hit.mHit = true;
                hit.mPointGlobal = point(origin.mdV[VX], origin.mdV[VY], 5.0);
            }
            return hit;
        };
        probes.mTerrainProbe = [](F64 x, F64 y) -> TerrainHit
        {
            TerrainHit hit;
            // No coverage beyond the bound, matching a real "outside any
            // loaded region" -- this is what lets WORK_PLANE/CAMERA_DEPTH
            // occur at all instead of the water probe below (which mirrors
            // it) catching every remotely downward ray unconditionally.
            if (std::fabs(x) > kTerrainCoverageBound ||
                std::fabs(y) > kTerrainCoverageBound)
            {
                return hit;
            }
            hit.mHit = true;
            hit.mHeightZ = std::sin(x * 0.05) * 2.0 + std::cos(y * 0.03) * 2.0;
            return hit;
        };
        probes.mWaterHeightProbe = [](F64 x, F64 y) -> F64
        {
            // Water only exists where terrain does (a real region), same
            // bound as the terrain probe above.
            if (std::fabs(x) > kTerrainCoverageBound ||
                std::fabs(y) > kTerrainCoverageBound)
            {
                return std::numeric_limits<F64>::infinity();
            }
            return 0.5;
        };
        probes.mLegalityProbe = boundedRegion(50.0);

        Lcg rng(0xC0FFEEu);
        S32 valid_count = 0;
        S32 blocked_count = 0;
        S32 basis_counts[5] = {0, 0, 0, 0, 0};
        for (S32 i = 0; i < 300; ++i)
        {
            Request request;
            request.mMaxDistanceMeters = rng.range(20.0, 80.0);
            request.mPolicy.mAllowSeabedPlacement = (i % 11) == 0;
            switch (i % 4)
            {
            case 0:
                // "surface": inside the tiny platform box, steeply downward.
                request.mRayOriginGlobal = point(
                    rng.range(-0.9, 0.9), rng.range(-0.9, 0.9),
                    rng.range(6.0, 20.0));
                request.mRayDirection = LLVector3(
                    (F32)rng.range(-0.3, 0.3), (F32)rng.range(-0.3, 0.3),
                    (F32)rng.range(-1.0, -0.5));
                break;
            case 1:
                // "terrain/water": within terrain coverage, well outside the
                // platform box, so the surface probe reliably misses.
                request.mRayOriginGlobal = point(
                    rng.range(-100.0, 100.0), rng.range(-100.0, 100.0),
                    rng.range(6.0, 40.0));
                request.mRayDirection = LLVector3(
                    (F32)rng.range(-0.3, 0.3), (F32)rng.range(-0.3, 0.3),
                    (F32)rng.range(-1.0, -0.3));
                break;
            case 2:
                // "work_plane": outside terrain/water coverage entirely, a
                // meaningfully downward ray reaching the plane in range.
                request.mRayOriginGlobal = point(
                    rng.range(600.0, 900.0), rng.range(600.0, 900.0),
                    rng.range(6.0, 40.0));
                request.mRayDirection = LLVector3(
                    (F32)rng.range(-0.3, 0.3), (F32)rng.range(-0.3, 0.3),
                    (F32)rng.range(-1.0, -0.3));
                break;
            default:
                // "camera_depth": same far-away origin, but a near-horizontal
                // direction so the plane intersection (if any) lies far
                // beyond maxDistance and the ladder falls all the way through.
                request.mRayOriginGlobal = point(
                    rng.range(600.0, 900.0), rng.range(600.0, 900.0),
                    rng.range(6.0, 40.0));
                request.mRayDirection = LLVector3(
                    (F32)rng.range(-1.0, 1.0), (F32)rng.range(-1.0, 1.0),
                    (F32)(rng.range(-1.0, 1.0) * 1.0e-7));
                break;
            }
            request.mCameraPositionGlobal = request.mRayOriginGlobal;
            request.mWorkPlaneHeightZ = rng.range(-5.0, 5.0);

            const ALGhostPlacementHit result = resolve(request, probes);
            std::string label = "sweep case ";
            label += std::to_string(i);
            ensure(label + " point stays finite",
                   finitePoint(result.mPointGlobal));
            ensure(label + " normal stays finite",
                   std::isfinite((double)result.mSurfaceNormal.mV[VX]) &&
                   std::isfinite((double)result.mSurfaceNormal.mV[VY]) &&
                   std::isfinite((double)result.mSurfaceNormal.mV[VZ]));
            ensure(label + " is either valid or carries a concrete reason",
                   result.mValid || !result.mWarning.empty());
            if (result.mValid) ++valid_count;
            else ++blocked_count;
            ++basis_counts[(int)result.mBasis];
        }
        // Sanity: the sweep actually exercises both outcomes, not a mock
        // that trivially always accepts or always blocks.
        ensure("sweep produces some valid placements", valid_count > 0);
        ensure("sweep produces some blocked placements", blocked_count > 0);
        // Coverage: every basis the ladder can produce actually occurred at
        // least once with this fixed seed/fixture.
        ensure("sweep hits VISIBLE_SURFACE at least once",
               basis_counts[(int)EGhostPlacementBasis::VISIBLE_SURFACE] > 0);
        ensure("sweep hits TERRAIN at least once",
               basis_counts[(int)EGhostPlacementBasis::TERRAIN] > 0);
        ensure("sweep hits WATER_SURFACE at least once",
               basis_counts[(int)EGhostPlacementBasis::WATER_SURFACE] > 0);
        ensure("sweep hits WORK_PLANE at least once",
               basis_counts[(int)EGhostPlacementBasis::WORK_PLANE] > 0);
        ensure("sweep hits CAMERA_DEPTH at least once",
               basis_counts[(int)EGhostPlacementBasis::CAMERA_DEPTH] > 0);
    }

    // H1 fix: an OBLIQUE ray's water snap must land on the ray's own actual
    // crossing of the water plane, not on the terrain-crossing's (x, y) with
    // Z merely swapped -- those are generally different points for any ray
    // that is not perfectly vertical.
    template<> template<>
    void resolver_object::test<12>()
    {
        Request request;
        request.mRayOriginGlobal = point(0.0, 0.0, 50.0);
        request.mCameraPositionGlobal = request.mRayOriginGlobal;
        request.mRayDirection = LLVector3(1.f, 0.f, -1.f);   // 45 degrees
        request.mMaxDistanceMeters = 100.0;
        Probes probes;
        probes.mSurfaceProbe = noSurface();
        probes.mTerrainProbe = flatTerrain(-5.0);   // crossed at t ~= 77.8, x ~= 55.0
        probes.mWaterHeightProbe = flatWater(0.0);  // crossed at t ~= 70.7, x ~= 50.0
        probes.mLegalityProbe = alwaysLegal();

        const ALGhostPlacementHit result = resolve(request, probes);
        ensure("oblique water snap resolves", result.mValid);
        ensure("oblique water snap uses WATER_SURFACE",
               result.mBasis == EGhostPlacementBasis::WATER_SURFACE);
        ensure("oblique water snap lands on the water plane",
            approxEqual(result.mPointGlobal.mdV[VZ], 0.0, 1.0e-6));
        ensure("oblique water snap stays ON the ray's own water crossing (x ~= 50)",
            approxEqual(result.mPointGlobal.mdV[VX], 50.0, 0.1));
        ensure("oblique water snap is NOT the terrain crossing's x with Z swapped (x != 55)",
            !approxEqual(result.mPointGlobal.mdV[VX], 55.0, 0.5));
    }

    // H1 fix: deep water whose seabed lies beyond maxDistance is caught by
    // the water rung as its own independent resolution, not missed entirely
    // because intersectTerrain() never found a crossing in range.
    template<> template<>
    void resolver_object::test<13>()
    {
        Request request = straightDown(0.0, 0.0, 50.0, /*max_distance*/ 64.0);
        Probes probes;
        probes.mSurfaceProbe = noSurface();
        probes.mTerrainProbe = flatTerrain(-1000.0);   // seabed far beyond range
        probes.mWaterHeightProbe = flatWater(0.0);      // crossed at t = 50, in range
        probes.mLegalityProbe = alwaysLegal();

        const ALGhostPlacementHit result = resolve(request, probes);
        ensure("deep water still resolves", result.mValid);
        ensure("deep water resolves via WATER_SURFACE as its own rung",
               result.mBasis == EGhostPlacementBasis::WATER_SURFACE);
        ensure("deep water lands on the water plane",
            approxEqual(result.mPointGlobal.mdV[VZ], 0.0, 1.0e-6));
    }

    // H1 fix: an underwater VISIBLE_SURFACE hit (e.g. a render pick landing
    // on the seabed through clear water) is no longer exempt from the water
    // policy -- it snaps to the surface exactly like a terrain hit would,
    // and the seabed policy applies to it exactly the same way too.
    template<> template<>
    void resolver_object::test<14>()
    {
        Request request = straightDown(5.0, 5.0, 50.0);
        Probes probes;
        probes.mSurfaceProbe = [](const LLVector3d&, const LLVector3&) -> SurfaceHit
        {
            SurfaceHit hit;
            hit.mHit = true;
            hit.mPointGlobal = point(5.0, 5.0, -3.0);   // below water
            return hit;
        };
        probes.mWaterHeightProbe = flatWater(0.0);
        probes.mLegalityProbe = alwaysLegal();

        const ALGhostPlacementHit snapped = resolve(request, probes);
        ensure("underwater surface hit still resolves", snapped.mValid);
        ensure("underwater surface hit snaps to WATER_SURFACE",
               snapped.mBasis == EGhostPlacementBasis::WATER_SURFACE);
        ensure("underwater surface hit lands on the water plane",
            approxEqual(snapped.mPointGlobal.mdV[VZ], 0.0, 1.0e-6));
        ensure("underwater surface hit carries a warning", !snapped.mWarning.empty());

        request.mPolicy.mAllowSeabedPlacement = true;
        const ALGhostPlacementHit seabed = resolve(request, probes);
        ensure("underwater surface hit with seabed policy still resolves",
               seabed.mValid);
        ensure("seabed policy keeps VISIBLE_SURFACE for a surface hit",
               seabed.mBasis == EGhostPlacementBasis::VISIBLE_SURFACE);
        ensure("seabed policy leaves the surface hit's own Z alone",
            approxEqual(seabed.mPointGlobal.mdV[VZ], -3.0, 1.0e-6));
    }

    // Finding 12: extreme-but-finite inputs must not overflow the
    // camera-depth rung's addition into a non-finite anchor -- the "always
    // produces a finite candidate" contract holds unconditionally.
    template<> template<>
    void resolver_object::test<15>()
    {
        Request request;
        const F64 huge = 1.7e308;   // near DBL_MAX
        request.mRayOriginGlobal = point(huge, 0.0, 0.0);
        request.mCameraPositionGlobal = point(huge, 0.0, 0.0);
        request.mRayDirection = LLVector3(1.f, 0.f, 0.f);
        request.mMaxDistanceMeters = 1.0e308;   // finite, but adding it overflows
        Probes probes;
        probes.mLegalityProbe = alwaysLegal();
        // No surface/terrain/water probes: falls straight through every
        // earlier rung to camera depth (dir.z == 0 also skips the plane
        // rung outright).

        const ALGhostPlacementHit result = resolve(request, probes);
        ensure("extreme finite input still resolves", result.mValid);
        ensure("extreme finite input yields a finite camera-depth point",
               finitePoint(result.mPointGlobal));
        ensure("extreme finite input basis is CAMERA_DEPTH",
               result.mBasis == EGhostPlacementBasis::CAMERA_DEPTH);
    }
}
