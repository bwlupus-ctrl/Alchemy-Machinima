/**
 * @file alavatarstagingmodel_test.cpp
 * @brief Tests for viewer-independent avatar staging math.
 */

#include "linden_common.h"

#include "../test/lltut.h"
#include "../alavatarstagingmodel.h"

#include <cmath>
#include <limits>

namespace tut
{
    using namespace ALAvatarStagingModel;

    namespace
    {
        bool approxNear(double lhs, double rhs, double epsilon = 1e-5)
        {
            return std::fabs(lhs - rhs) <= epsilon;
        }
    }

    struct avatar_staging_model_data {};
    typedef test_group<avatar_staging_model_data> avatar_staging_model_group;
    typedef avatar_staging_model_group::object avatar_staging_model_object;
    avatar_staging_model_group avatar_staging_model_tests(
        "ALAvatarStagingModel");

    template<> template<>
    void avatar_staging_model_object::test<1>()
    {
        ensure("positive wrap", approxNear(wrapYaw(3.0 * PI), -PI));
        ensure("negative wrap", approxNear(wrapYaw(-3.0 * PI), -PI));
        ensure("short path across seam",
               approxNear(shortestArc(179.0 * PI / 180.0,
                                -179.0 * PI / 180.0),
                    2.0 * PI / 180.0));
    }

    template<> template<>
    void avatar_staging_model_object::test<2>()
    {
        ScalarSpring yaw;
        snap(yaw, 0.0);
        const double first = stepCritical(yaw, PI, 0.1);
        ensure("yaw cap", std::fabs(first) <= MAX_YAW_RATE * 0.1 + 1e-9);
        for (int i = 0; i < 200; ++i)
        {
            stepCritical(yaw, PI - 0.001, 0.01);
        }
        ensure("yaw converges",
               std::fabs(shortestArc(yaw.value, PI - 0.001)) < 1e-3);

        VectorSpring position;
        snap(position, Vec3());
        const Vec3 moved = stepCritical(position, Vec3(100.0, 0.0, 0.0), 0.1);
        ensure("position cap", moved.x <= MAX_POSITION_RATE * 0.1 + 1e-9);
        ensure("no lateral drift", approxNear(moved.y, 0.0));
    }

    template<> template<>
    void avatar_staging_model_object::test<3>()
    {
        ensure("north", approxNear(faceYaw(Vec3(), Vec3(0.0, 2.0, 9.0)),
                             PI / 2.0));
        ensure("west", approxNear(std::fabs(faceYaw(Vec3(), Vec3(-1.0, 0.0, 0.0))),
                            PI));
        ensure("coincident fallback",
               approxNear(faceYaw(Vec3(1.0, 1.0, 0.0), Vec3(1.0, 1.0, 5.0), 0.7),
                    0.7));
    }

    template<> template<>
    void avatar_staging_model_object::test<4>()
    {
        const std::vector<Vec3> feet = {
            Vec3(-1.0, 0.0, 0.0), Vec3(1.0, 0.0, 0.0),
            Vec3(0.0, -1.0, 0.0), Vec3(0.0, 1.0, 0.0),
            Vec3(2.0, 0.0, 0.0) };
        const std::vector<double> yaw = pairingYaws(feet, Vec3());
        ensure("one faces two", approxNear(yaw[0], 0.0));
        ensure("two faces one", approxNear(std::fabs(yaw[1]), PI));
        ensure("three faces four", approxNear(yaw[2], PI / 2.0));
        ensure("four faces three", approxNear(yaw[3], -PI / 2.0));
        ensure("odd faces centre", approxNear(std::fabs(yaw[4]), PI));
    }

    template<> template<>
    void avatar_staging_model_object::test<5>()
    {
        const auto circle = formationSlots(Formation::CIRCLE_IN, 4, 2.0);
        ensure("circle count", circle.size() == 4);
        ensure("circle first foot", approxNear(circle[0].foot.x, 2.0));
        ensure("circle inward", approxNear(std::fabs(circle[0].yaw), PI));
        ensure("circle quarter", approxNear(circle[1].foot.y, 2.0));
        const auto circle_out = formationSlots(Formation::CIRCLE_OUT, 1, 2.0);
        ensure("circle outward", approxNear(circle_out[0].yaw, 0.0));

        const auto arc = formationSlots(Formation::ARC, 3, 2.0, PI);
        ensure("arc starts below", approxNear(arc[0].foot.y, -2.0));
        ensure("arc midpoint forward", approxNear(arc[1].foot.x, 2.0));

        const auto line = formationSlots(Formation::LINE, 3, 1.0);
        ensure("line centred", approxNear(line[0].foot.y, -1.0) &&
                               approxNear(line[2].foot.y, 1.0));

        const auto grid = formationSlots(Formation::GRID, 5, 1.0);
        ensure("grid count", grid.size() == 5);
        ensure("short row independently centred",
               approxNear(grid[3].foot.y, -0.5) && approxNear(grid[4].foot.y, 0.5));
    }

    template<> template<>
    void avatar_staging_model_object::test<6>()
    {
        const Vec3 anchor;
        ensure("pin holds at boundary",
               updatePinState(true, true, Vec3(2.0, 0.0, 0.0), anchor));
        ensure("pin releases beyond tolerance",
               !updatePinState(true, true, Vec3(2.01, 0.0, 0.0), anchor));
        ensure("released pin remains released in hysteresis band",
               !updatePinState(true, false, Vec3(1.9, 0.0, 0.0), anchor));
        ensure("released pin re-pins near anchor",
               updatePinState(true, false, Vec3(1.7, 0.0, 0.0), anchor));
        ensure("release request wins",
               !updatePinState(false, true, anchor, anchor));
        ensure("target location does not affect anchor drift",
               updatePinState(true, true, Vec3(0.5, 0.0, 99.0), anchor));
        ensure("pin drift is XY-only",
               updatePinState(true, true, Vec3(0.0, 0.0, 1000.0), anchor));

        const Vec3 clamped = clampOffsetXY(Vec3(64.0, 0.0, 12.0));
        ensure("pin target is clamped in XY",
               approxNear(clamped.x, MAX_OFFSET_METERS));
        ensure("pin target never changes Z", approxNear(clamped.z, 0.0));
        const Vec3 elevated = clampPinOffset(Vec3(64.0, 0.0, 12.0));
        ensure("pinned Z nudge is retained", approxNear(elevated.z, 12.0));
        ensure("pinned XY remains capped",
               approxNear(elevated.x, MAX_OFFSET_METERS));
    }

    template<> template<>
    void avatar_staging_model_object::test<7>()
    {
        ensure("five metres is continuous",
               !isDiscontinuity(Vec3(), Vec3(3.0, 4.0, 0.0)));
        ensure("more than five is discontinuous",
               isDiscontinuity(Vec3(), Vec3(5.001, 0.0, 0.0)));
    }

    template<> template<>
    void avatar_staging_model_object::test<8>()
    {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const double inf = std::numeric_limits<double>::infinity();
        ensure("nan scalar fallback", clampFinite(nan, -1.0, 1.0, 0.25) == 0.25);
        ensure("infinity yaw guarded", wrapYaw(inf) == 0.0);
        const Vec3 guarded = clampOffset(Vec3(nan, 1.0, 2.0));
        ensure("invalid vector cleared",
               guarded.x == 0.0 && guarded.y == 0.0 && guarded.z == 0.0);
        const Vec3 clamped = clampOffset(Vec3(64.0, 0.0, 0.0));
        ensure("offset magnitude capped", approxNear(clamped.x, MAX_OFFSET_METERS));

        ScalarSpring scalar;
        scalar.initialized = true;
        scalar.value = nan;
        scalar.velocity = inf;
        ensure("corrupt spring snaps finite",
               finite(stepCritical(scalar, 1.0, nan)));
        const auto slots = formationSlots(Formation::CIRCLE_OUT, 2, inf);
        ensure("formation input guarded", finite(slots[0].foot));
    }

    template<> template<>
    void avatar_staging_model_object::test<9>()
    {
        const Vec3 world = localOffsetToWorld(Vec3(1.0, 0.0, 0.5), PI / 2.0);
        ensure("visible north yaw sends forward nudge north",
               approxNear(world.x, 0.0) && approxNear(world.y, 1.0));
        ensure("local nudge preserves vertical offset", approxNear(world.z, 0.5));
        const Vec3 local = worldOffsetToLocal(Vec3(1.0, 0.0, 0.5), PI / 2.0);
        ensure("world east becomes seat-local right",
               approxNear(local.x, 0.0) && approxNear(local.y, -1.0));
        ensure("seat transform preserves vertical offset", approxNear(local.z, 0.5));

        // Global coordinates remain continuous across a 256 m agent-origin shift.
        const Vec3 before_global(1000.0, 1000.0, 20.0);
        const Vec3 after_global(1000.25, 1000.0, 20.0);
        ensure("global discontinuity ignores origin shift",
               !isDiscontinuity(before_global, after_global));
    }

    template<> template<>
    void avatar_staging_model_object::test<10>()
    {
        Vec3 centre;
        ensure("empty centroid is unavailable",
               !centroid(std::vector<Vec3>(), centre));
        const std::vector<Vec3> dancers = {
            Vec3(10.0, 20.0, 2.0), Vec3(14.0, 24.0, 4.0) };
        ensure("formation fallback uses dancer centroid",
               centroid(dancers, centre) && approxNear(centre.x, 12.0) &&
               approxNear(centre.y, 22.0) && approxNear(centre.z, 3.0));
        const double nan = std::numeric_limits<double>::quiet_NaN();
        ensure("centroid ignores invalid dancers",
               centroid({ Vec3(nan, 0.0, 0.0), Vec3(2.0, 4.0, 6.0) },
                        centre) && approxNear(centre.x, 2.0));
    }

    template<> template<>
    void avatar_staging_model_object::test<11>()
    {
        const Vec3 frozen_centre(10.0, 20.0, 3.0);
        const std::vector<Vec3> same_dancers = {
            Vec3(11.0, 20.0, 3.0), Vec3(9.0, 20.0, 3.0) };
        ensure("same nearby dancer set reuses frozen centroid",
               reuseFrozenCentroid(true, true, frozen_centre, same_dancers));
        ensure("different dancer set re-derives centroid",
               !reuseFrozenCentroid(true, false, frozen_centre, same_dancers));
        ensure("explicit centre is not treated as frozen centroid",
               !reuseFrozenCentroid(false, true, frozen_centre, same_dancers));
        ensure("distant dancers re-derive centroid",
               !reuseFrozenCentroid(true, true, frozen_centre,
                                    { Vec3(75.0, 20.0, 3.0),
                                      Vec3(80.0, 20.0, 3.0) }));
    }

    template<> template<>
    void avatar_staging_model_object::test<12>()
    {
        const Vec3 formation_centre(104.0, 203.0, 50.0);
        const auto slots = formationSlots(Formation::LINE, 2, 2.0);
        const Vec3 anchors[] = {
            Vec3(100.0, 200.0, 10.0), Vec3(108.0, 206.0, 20.0) };
        const double visible_z_offsets[] = { 0.75, -0.25 };
        Vec3 pins[2];
        for (std::size_t i = 0; i < 2; ++i)
        {
            pins[i] = makePinTarget(formation_centre + slots[i].foot,
                                    anchors[i], visible_z_offsets[i]);
            ensure("pins store each dancer's relative Z offset",
                   approxNear(pins[i].z, visible_z_offsets[i]));
        }
        ensure("formation takes absolute XY",
               approxNear(pins[0].x, formation_centre.x + slots[0].foot.x) &&
               approxNear(pins[0].y, formation_centre.y + slots[0].foot.y));

        const Vec3 after_jump(101.0, 201.0, 14.0);
        const Vec3 target = pinTargetOffset(pins[0], after_jump);
        ensure("pinned dancer follows live vertical motion",
               approxNear(target.z, 0.75));
        ensure("pinned dancer still corrects toward absolute XY",
               approxNear(target.x, 3.0) && approxNear(target.y, 1.0));
    }
}
