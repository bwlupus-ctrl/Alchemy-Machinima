/**
 * @file alformationsolver_test.cpp
 * @brief Adversarial tests for the pure crowd-formation solver.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "../test/lltut.h"

#include "../alformationsolver.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace tut
{
    using namespace ALFormationSolver;

    namespace
    {
        constexpr F64 PI = 3.141592653589793238462643383279502884;
        constexpr F64 TWO_PI = 2.0 * PI;
        constexpr F64 TEST_EPSILON = 1.0e-8;
        constexpr U32 TEST_COMPARISON_ULPS = 4;

        F64 advanceTestUlps(F64 value, bool upward)
        {
            const F64 direction = upward
                ? std::numeric_limits<F64>::infinity()
                : -std::numeric_limits<F64>::infinity();
            for (U32 i = 0; i < TEST_COMPARISON_ULPS; ++i)
            {
                value = std::nextafter(value, direction);
            }
            return value;
        }

        Request makeRequest(Shape shape, U32 count,
                            F64 footprint = 0.20,
                            F64 centre_spacing = 1.0)
        {
            Request request;
            request.mShape = shape;
            request.mPolicy = EnvelopePolicy::FitCount;
            request.mCenterSpacingMeters = centre_spacing;
            request.mMembers.reserve(count);
            for (U32 i = 0; i < count; ++i)
            {
                Member member;
                member.mId = 1000ULL + i;
                member.mFootprintRadiusMeters = footprint;
                request.mMembers.push_back(member);
            }
            return request;
        }

        F64 slotDistance(const Slot& first, const Slot& second)
        {
            return std::hypot(first.mLocalX - second.mLocalX,
                              first.mLocalY - second.mLocalY);
        }

        void ensureClose(const std::string& message, F64 actual,
                         F64 expected, F64 epsilon = TEST_EPSILON)
        {
            ensure(message,
                std::isfinite(actual) &&
                std::fabs(actual - expected) <= epsilon);
        }

        void ensureSameSolution(const std::string& message,
                                const Result& first, const Result& second)
        {
            ensure(message + ": fingerprint",
                   first.mSolutionFingerprint == second.mSolutionFingerprint);
            ensure(message + ": slot count",
                   first.mSlots.size() == second.mSlots.size());
            for (size_t i = 0; i < first.mSlots.size(); ++i)
            {
                ensure(message + ": member order",
                       first.mSlots[i].mMemberId == second.mSlots[i].mMemberId);
                ensureClose(message + ": x",
                            first.mSlots[i].mLocalX,
                            second.mSlots[i].mLocalX, 0.0);
                ensureClose(message + ": y",
                            first.mSlots[i].mLocalY,
                            second.mSlots[i].mLocalY, 0.0);
            }
            ensure(message + ": overflow",
                   first.mOverflowMemberIds == second.mOverflowMemberIds);
        }

        void ensureContract(const std::string& message,
                            const Request& request, const Result& result)
        {
            ensure(message + ": successful status",
                   result.mStatus == Status::Ok ||
                   result.mStatus == Status::PartialOverflow);
            ensure(message + ": partition",
                   result.mPlacedCount + result.mOverflowCount ==
                   result.mRequestedCount);
            ensure(message + ": slot count",
                   result.mSlots.size() == result.mPlacedCount);
            ensure(message + ": overflow count",
                   result.mOverflowMemberIds.size() == result.mOverflowCount);
            ensure(message + ": finite envelope",
                   std::isfinite(result.mEnvelopeRadiusMeters));
            ensure(message + ": finite required radius",
                   std::isfinite(result.mRequiredRadiusMeters));
            ensure(message + ": nonzero solution fingerprint",
                   result.mSolutionFingerprint != 0);

            for (size_t i = 0; i < result.mSlots.size(); ++i)
            {
                const Slot& slot = result.mSlots[i];
                ensure(message + ": stable prefix index",
                       slot.mInputIndex == i);
                ensure(message + ": stable member ID",
                       slot.mMemberId == request.mMembers[i].mId);
                ensure(message + ": finite x", std::isfinite(slot.mLocalX));
                ensure(message + ": finite y", std::isfinite(slot.mLocalY));
                const F64 outer = std::hypot(slot.mLocalX, slot.mLocalY) +
                                  slot.mFootprintRadiusMeters;
                ensure(message + ": footprint inside envelope",
                       outer <= advanceTestUlps(
                           result.mEnvelopeRadiusMeters, true));
            }

            for (size_t i = 0; i < result.mSlots.size(); ++i)
            {
                for (size_t j = i + 1; j < result.mSlots.size(); ++j)
                {
                    const F64 footprint_requirement =
                        result.mSlots[i].mFootprintRadiusMeters +
                        result.mSlots[j].mFootprintRadiusMeters +
                        request.mEdgeGapMeters;
                    const F64 requirement = std::max(
                        request.mCenterSpacingMeters, footprint_requirement);
                    ensure(message + ": pairwise clearance",
                           slotDistance(result.mSlots[i], result.mSlots[j]) >=
                           advanceTestUlps(requirement, false));
                }
            }
            if (result.mHasPairwiseGap)
            {
                ensure(message + ": edge gap diagnostic",
                       result.mActualMinEdgeGapMeters + TEST_EPSILON >=
                       request.mEdgeGapMeters);
            }
        }
    }

    struct formation_solver_data
    {
    };

    typedef test_group<formation_solver_data> formation_solver_group;
    typedef formation_solver_group::object    formation_solver_object;
    formation_solver_group formation_solver_tests("ALFormationSolver");

    // Every numeric input is validated at the solver boundary; callers cannot
    // bypass the UI's count/spinner limits.
    template<> template<>
    void formation_solver_object::test<1>()
    {
        Request request = makeRequest(Shape::Row, 2);
        request.mCenterSpacingMeters =
            std::numeric_limits<F64>::quiet_NaN();
        ensure("NaN spacing rejected",
               solve(request).mStatus == Status::InvalidInput);

        request = makeRequest(Shape::Row, 2);
        request.mRadiusMeters = std::numeric_limits<F64>::infinity();
        ensure("infinite radius rejected",
               solve(request).mStatus == Status::InvalidInput);

        request = makeRequest(Shape::Row, 2);
        request.mMembers[1].mFootprintRadiusMeters =
            std::numeric_limits<F64>::infinity();
        ensure("infinite footprint rejected",
               solve(request).mStatus == Status::InvalidInput);

        request = makeRequest(Shape::Row, MAX_MEMBER_COUNT + 1);
        ensure("hard count cap enforced",
               solve(request).mStatus == Status::InvalidInput);

        request = makeRequest(Shape::Row, 2);
        request.mMembers[1].mId = request.mMembers[0].mId;
        ensure("duplicate stable IDs rejected",
               solve(request).mStatus == Status::InvalidInput);

        request = makeRequest(Shape::LegacyPath, 3);
        const Result unsupported = solve(request);
        ensure("legacy path explicitly unsupported",
               unsupported.mStatus == Status::UnsupportedShape);
        ensure("unsupported request reports every member as overflow",
               unsupported.mOverflowCount == 3 &&
               unsupported.mPlacedCount == 0);

        request = makeRequest(Shape::Arc, 3);
        request.mArcSweepRadians = 0.0;
        ensure("zero arc sweep rejected",
               solve(request).mStatus == Status::InvalidInput);
    }

    // Empty and one-member requests are real layouts, and footprint is part of
    // the radius contract even when there are no pair distances.
    template<> template<>
    void formation_solver_object::test<2>()
    {
        Request empty = makeRequest(Shape::Row, 0);
        const Result empty_result = solve(empty);
        ensure("empty request succeeds", empty_result.mStatus == Status::Ok);
        ensure("empty request has zero radius",
               empty_result.mEnvelopeRadiusMeters == 0.0);
        ensureContract("empty", empty, empty_result);

        Request single = makeRequest(Shape::Row, 1, 0.35);
        const Result fitted = solve(single);
        ensure("single member succeeds", fitted.mStatus == Status::Ok);
        ensureClose("single x", fitted.mSlots[0].mLocalX, 0.0);
        ensureClose("single y", fitted.mSlots[0].mLocalY, 0.0);
        ensureClose("single radius includes footprint",
                    fitted.mRequiredRadiusMeters, 0.35);
        ensureContract("single fitted", single, fitted);

        single.mPolicy = EnvelopePolicy::FixedRadius;
        single.mRadiusMeters = 0.35;
        const Result exact = solve(single);
        ensure("exact footprint boundary fits", exact.mStatus == Status::Ok);
        ensureContract("single exact", single, exact);

        single.mRadiusMeters = 0.349;
        const Result too_small = solve(single);
        ensure("footprint outside radius overflows",
               too_small.mStatus == Status::PartialOverflow);
        ensure("single member overflowed",
               too_small.mPlacedCount == 0 &&
               too_small.mOverflowCount == 1);
    }

    // Rows are centred around the explicit origin for both odd and even counts,
    // use exact neighbour spacing, and rotate covariantly.
    template<> template<>
    void formation_solver_object::test<3>()
    {
        Request request = makeRequest(Shape::Row, 4, 0.20, 1.0);
        const Result row = solve(request);
        ensureContract("centred row", request, row);
        ensureClose("row x 0", row.mSlots[0].mLocalX, -1.5);
        ensureClose("row x 1", row.mSlots[1].mLocalX, -0.5);
        ensureClose("row x 2", row.mSlots[2].mLocalX,  0.5);
        ensureClose("row x 3", row.mSlots[3].mLocalX,  1.5);
        ensureClose("row required radius", row.mRequiredRadiusMeters, 1.7);
        for (size_t i = 1; i < row.mSlots.size(); ++i)
        {
            ensureClose("row neighbour spacing",
                        slotDistance(row.mSlots[i - 1], row.mSlots[i]), 1.0);
        }

        request.mYawRadians = 0.5 * PI;
        const Result rotated = solve(request);
        ensureContract("rotated row", request, rotated);
        for (size_t i = 0; i < row.mSlots.size(); ++i)
        {
            ensureClose("90 degree rotated x",
                        rotated.mSlots[i].mLocalX,
                        -row.mSlots[i].mLocalY);
            ensureClose("90 degree rotated y",
                        rotated.mSlots[i].mLocalY,
                        row.mSlots[i].mLocalX);
        }
    }

    // A fixed radius never grows silently. It places the largest fitting stable
    // prefix and reports both the fitted prefix radius and all-member radius.
    template<> template<>
    void formation_solver_object::test<4>()
    {
        Request request = makeRequest(Shape::Row, 5, 0.20, 1.0);
        request.mPolicy = EnvelopePolicy::FixedRadius;
        request.mRadiusMeters = 1.20;

        const Result result = solve(request);
        ensure("row reports overflow",
               result.mStatus == Status::PartialOverflow);
        ensure("three-member prefix fits exact boundary",
               result.mPlacedCount == 3 && result.mOverflowCount == 2);
        ensureClose("placed radius", result.mPlacedRadiusMeters, 1.20);
        ensureClose("all-member required radius",
                    result.mRequiredRadiusMeters, 2.20);
        ensure("overflow order is stable",
               result.mOverflowMemberIds[0] == request.mMembers[3].mId &&
               result.mOverflowMemberIds[1] == request.mMembers[4].mId);
        ensureContract("row overflow", request, result);
    }

    // Automatic grid dimensions minimise the actual footprint-inclusive
    // envelope; ties prefer the balanced landscape-oriented lattice.
    template<> template<>
    void formation_solver_object::test<5>()
    {
        Request request = makeRequest(Shape::Grid, 5, 0.10, 1.0);
        const Result result = solve(request);
        ensureContract("smart grid", request, result);

        ensureClose("smart grid first x", result.mSlots[0].mLocalX, -1.0);
        ensureClose("smart grid first y", result.mSlots[0].mLocalY, -0.5);
        ensureClose("smart grid third x", result.mSlots[2].mLocalX, 1.0);
        ensureClose("smart grid fourth y", result.mSlots[3].mLocalY, 0.5);
        ensureClose("smart grid required radius",
                    result.mRequiredRadiusMeters,
                    std::sqrt(1.25) + 0.10);
    }

    // Staggered rows use a triangular lattice: half-pitch horizontal offset and
    // sqrt(3)/2 vertical pitch preserve the same nearest-neighbour distance.
    template<> template<>
    void formation_solver_object::test<6>()
    {
        Request request = makeRequest(Shape::StaggeredRows, 6, 0.0, 1.0);
        request.mGridColumns = 3;
        const Result result = solve(request);
        ensureContract("staggered rows", request, result);

        const F64 vertical =
            std::fabs(result.mSlots[3].mLocalY - result.mSlots[0].mLocalY);
        ensureClose("triangular vertical pitch",
                    vertical, std::sqrt(3.0) * 0.5);
        ensureClose("triangular diagonal neighbour",
                    slotDistance(result.mSlots[0], result.mSlots[3]), 1.0);
        ensureClose("same-row neighbour",
                    slotDistance(result.mSlots[0], result.mSlots[1]), 1.0);
    }

    // Ring radius is derived from the chord requirement and the footprint;
    // equality at the fixed boundary is accepted without dropping a member.
    template<> template<>
    void formation_solver_object::test<7>()
    {
        Request request = makeRequest(Shape::Ring, 4, 0.25, 1.0);
        const Result fitted = solve(request);
        ensureContract("ring fitted", request, fitted);

        const F64 centre_radius = 1.0 / std::sqrt(2.0);
        ensureClose("ring centre radius",
                    std::hypot(fitted.mSlots[0].mLocalX,
                               fitted.mSlots[0].mLocalY),
                    centre_radius);
        ensureClose("ring footprint radius",
                    fitted.mRequiredRadiusMeters,
                    centre_radius + 0.25);
        for (size_t i = 0; i < fitted.mSlots.size(); ++i)
        {
            const size_t next = (i + 1) % fitted.mSlots.size();
            ensureClose("ring chord",
                        slotDistance(fitted.mSlots[i], fitted.mSlots[next]),
                        1.0);
        }

        request.mPolicy = EnvelopePolicy::FixedRadius;
        request.mRadiusMeters = fitted.mRequiredRadiusMeters;
        const Result exact = solve(request);
        ensure("exact ring boundary fits", exact.mStatus == Status::Ok);
        ensureContract("ring exact", request, exact);

        request.mRadiusMeters = 0.95;
        const Result overflow = solve(request);
        ensure("ring just outside boundary overflows",
               overflow.mStatus == Status::PartialOverflow);
        ensure("ring overflows a deterministic suffix",
               overflow.mPlacedCount < request.mMembers.size() &&
               overflow.mPlacedCount + overflow.mOverflowCount ==
               request.mMembers.size());
        ensureContract("ring overflow", request, overflow);
    }

    // A 359-degree arc must not duplicate its endpoints. It caps the angular
    // step like a ring; exactly 360 degrees uses the ring solution.
    template<> template<>
    void formation_solver_object::test<8>()
    {
        Request almost = makeRequest(Shape::Arc, 8, 0.0, 1.0);
        almost.mArcSweepRadians = 359.0 * PI / 180.0;
        const Result almost_result = solve(almost);
        ensureContract("359 degree arc", almost, almost_result);
        ensureClose("359 degree actual safe sweep",
                    almost_result.mActualArcSweepRadians,
                    315.0 * PI / 180.0);
        ensureClose("359 degree endpoint clearance",
                    slotDistance(almost_result.mSlots.front(),
                                 almost_result.mSlots.back()),
                    1.0);

        Request complete = almost;
        complete.mArcSweepRadians = TWO_PI;
        const Result complete_result = solve(complete);
        ensureContract("360 degree arc", complete, complete_result);
        ensureClose("360 degree diagnostic",
                    complete_result.mActualArcSweepRadians, TWO_PI);
        ensureClose("360 degree closing chord",
                    slotDistance(complete_result.mSlots.front(),
                                 complete_result.mSlots.back()),
                    1.0);
        ensureClose("359 and 360 require the same radius",
                    almost_result.mRequiredRadiusMeters,
                    complete_result.mRequiredRadiusMeters);
    }

    // Concentric rings use exact chord capacity and radial pitch, including
    // clearance between rings with unrelated deterministic phases.
    template<> template<>
    void formation_solver_object::test<9>()
    {
        Request request = makeRequest(Shape::ConcentricRings, 20, 0.0, 1.0);
        request.mSeed = 42;
        const Result first = solve(request);
        const Result repeated = solve(request);
        ensureContract("concentric rings", request, first);
        ensureSameSolution("concentric repeat", first, repeated);
        ensureClose("20 members reach third ring",
                    first.mRequiredRadiusMeters, 3.0);

        request.mSeed = 43;
        const Result changed = solve(request);
        ensureContract("concentric changed seed", request, changed);
        ensure("concentric seed changes solution",
               changed.mSolutionFingerprint != first.mSolutionFingerprint);
    }

    // Organic placement is deterministic for a stable seed/order but genuinely
    // seed-dependent, while its final scaled phyllotaxis obeys every invariant.
    template<> template<>
    void formation_solver_object::test<10>()
    {
        Request request = makeRequest(Shape::Organic, 24, 0.20, 0.70);
        request.mEdgeGapMeters = 0.10;
        request.mSeed = 123456;

        const Result first = solve(request);
        const Result repeated = solve(request);
        ensureContract("organic", request, first);
        ensureSameSolution("organic repeat", first, repeated);

        request.mSeed = 123457;
        const Result changed = solve(request);
        ensureContract("organic changed seed", request, changed);
        ensure("organic seed changes fingerprint",
               changed.mSolutionFingerprint != first.mSolutionFingerprint);

        bool any_position_changed = false;
        for (size_t i = 0; i < first.mSlots.size(); ++i)
        {
            if (std::fabs(first.mSlots[i].mLocalX -
                          changed.mSlots[i].mLocalX) > TEST_EPSILON ||
                std::fabs(first.mSlots[i].mLocalY -
                          changed.mSlots[i].mLocalY) > TEST_EPSILON)
            {
                any_position_changed = true;
                break;
            }
        }
        ensure("organic seed changes geometry", any_position_changed);
    }

    // Jitter reserves clearance before displacement, remains deterministic,
    // and rotates with the cluster instead of being regenerated in world axes.
    template<> template<>
    void formation_solver_object::test<11>()
    {
        Request request = makeRequest(Shape::Row, 8, 0.20, 1.0);
        request.mEdgeGapMeters = 0.10;
        request.mJitterMeters = 0.20;
        request.mSeed = 777;

        const Result first = solve(request);
        const Result repeated = solve(request);
        ensureContract("jittered row", request, first);
        ensureSameSolution("jitter repeat", first, repeated);

        Request changed_seed = request;
        changed_seed.mSeed = 778;
        const Result changed = solve(changed_seed);
        ensureContract("changed jitter seed", changed_seed, changed);
        ensure("jitter seed changes fingerprint",
               changed.mSolutionFingerprint != first.mSolutionFingerprint);

        Request rotated_request = request;
        rotated_request.mYawRadians = 0.5 * PI;
        const Result rotated = solve(rotated_request);
        ensureContract("rotated jitter", rotated_request, rotated);
        for (size_t i = 0; i < first.mSlots.size(); ++i)
        {
            ensureClose("jitter rotation x",
                        rotated.mSlots[i].mLocalX,
                        -first.mSlots[i].mLocalY);
            ensureClose("jitter rotation y",
                        rotated.mSlots[i].mLocalY,
                        first.mSlots[i].mLocalX);
        }
    }

    // Organic fixed-radius overflow still uses the same explicit prefix and
    // partition contract; it does not shrink spacing or expand the envelope.
    template<> template<>
    void formation_solver_object::test<12>()
    {
        Request request = makeRequest(Shape::Organic, 30, 0.20, 0.80);
        request.mPolicy = EnvelopePolicy::FixedRadius;
        request.mRadiusMeters = 1.0;
        request.mSeed = 999;

        const Result result = solve(request);
        ensure("organic fixed radius overflows",
               result.mStatus == Status::PartialOverflow);
        ensure("organic places at least the centre member",
               result.mPlacedCount >= 1);
        ensure("organic does not place every member",
               result.mPlacedCount < result.mRequestedCount);
        ensure("required radius exposes overflow pressure",
               result.mRequiredRadiusMeters > request.mRadiusMeters);
        for (size_t i = 0; i < result.mSlots.size(); ++i)
        {
            ensure("organic stable prefix",
                   result.mSlots[i].mMemberId == request.mMembers[i].mId);
        }
        ensureContract("organic overflow", request, result);
    }

    // Large metre values must not turn a comparison tolerance into centimetres
    // or metres of hidden radius/clearance.  The former 1e-10 relative epsilon
    // accepted this 1 cm hard-radius violation at 1e9.
    template<> template<>
    void formation_solver_object::test<13>()
    {
        Request boundary =
            makeRequest(Shape::Row, 1, MAX_INPUT_METERS, 0.0);
        boundary.mPolicy = EnvelopePolicy::FixedRadius;
        boundary.mRadiusMeters = MAX_INPUT_METERS;
        const Result exact = solve(boundary);
        ensure("1e9 exact footprint boundary fits",
               exact.mStatus == Status::Ok);
        ensureContract("1e9 exact boundary", boundary, exact);

        boundary.mRadiusMeters = MAX_INPUT_METERS - 0.01;
        const Result outside = solve(boundary);
        ensure("1 cm violation at 1e9 overflows",
               outside.mStatus == Status::PartialOverflow);
        ensure("large boundary keeps the member out",
               outside.mPlacedCount == 0 &&
               outside.mOverflowCount == 1);

        Request pair = makeRequest(
            Shape::Row, 2, 0.0, MAX_INPUT_METERS);
        pair.mYawRadians = 0.371;
        const Result pair_result = solve(pair);
        ensure("1e9 pair layout succeeds",
               pair_result.mStatus == Status::Ok);
        ensureContract("1e9 pair", pair, pair_result);
        const F64 actual =
            slotDistance(pair_result.mSlots[0], pair_result.mSlots[1]);
        ensure("1e9 pair cannot hide a millimetre violation",
               actual >= MAX_INPUT_METERS - 0.001);
        ensure("1e9 pair stays within four ULPs",
               actual >= advanceTestUlps(MAX_INPUT_METERS, false));
    }

    // Exercise the independent 1e12 layout ceiling with an arc whose chord is
    // 1e9.  One target is safely one metre inside the limit; the other is one
    // metre outside and must never be admitted as a numerical tie.
    template<> template<>
    void formation_solver_object::test<14>()
    {
        Request inside = makeRequest(
            Shape::Arc, 2, 0.0, MAX_INPUT_METERS);
        const F64 inside_radius = MAX_LAYOUT_RADIUS_METERS - 1.0;
        inside.mArcSweepRadians = 2.0 * std::asin(
            MAX_INPUT_METERS / (2.0 * inside_radius));
        const Result inside_result = solve(inside);
        ensure("near-1e12 arc remains feasible",
               inside_result.mStatus == Status::Ok);
        ensure("near-1e12 radius is reported at scale",
               inside_result.mRequiredRadiusMeters >
               MAX_LAYOUT_RADIUS_METERS - 10.0);
        ensure("near-1e12 radius stays under the hard ceiling",
               inside_result.mRequiredRadiusMeters <=
               MAX_LAYOUT_RADIUS_METERS);
        ensureContract("near-1e12 arc", inside, inside_result);

        Request outside = inside;
        const F64 outside_radius = MAX_LAYOUT_RADIUS_METERS + 1.0;
        outside.mArcSweepRadians = 2.0 * std::asin(
            MAX_INPUT_METERS / (2.0 * outside_radius));
        const Result outside_result = solve(outside);
        ensure("one metre beyond 1e12 is rejected",
               outside_result.mStatus == Status::NoFeasibleLayout);
        ensure("rejected 1e12 layout places nobody",
               outside_result.mPlacedCount == 0 &&
               outside_result.mOverflowCount == 2);
    }

    // A full FixedRadius layout may exceed the independent numeric ceiling
    // while a smaller stable prefix is perfectly representable. Prefix
    // solving remains mandatory in that case.
    template<> template<>
    void formation_solver_object::test<15>()
    {
        Request request = makeRequest(
            Shape::Arc, 2, 0.0, MAX_INPUT_METERS);
        request.mPolicy = EnvelopePolicy::FixedRadius;
        request.mRadiusMeters = 1.0;
        request.mArcSweepRadians = 1.0e-6;

        const Result result = solve(request);
        ensure("numeric-ceiling arc uses fixed-radius overflow",
               result.mStatus == Status::PartialOverflow);
        ensure("one-member prefix remains placeable",
               result.mPlacedCount == 1 &&
               result.mOverflowCount == 1);
        ensure("the placed member is the stable prefix",
               result.mSlots.size() == 1 &&
               result.mSlots.front().mMemberId ==
                   request.mMembers.front().mId);
        ensure("full required radius reports ceiling saturation",
               result.mRequiredRadiusMeters ==
                   MAX_LAYOUT_RADIUS_METERS);
        ensureContract("numeric-ceiling fixed prefix", request, result);
    }

    // Every explicit Studio-shape control is validated at the same pure
    // boundary as the original solver controls and participates in preview /
    // commit identity even when a particular shape does not consume it.
    template<> template<>
    void formation_solver_object::test<16>()
    {
        Request request = makeRequest(Shape::Chevron, 5);
        request.mLaneGapMeters =
            std::numeric_limits<F64>::quiet_NaN();
        ensure("NaN lane gap rejected",
               solve(request).mStatus == Status::InvalidInput);

        request = makeRequest(Shape::Chevron, 5);
        request.mChevronAngleDegrees = 0.0;
        ensure("zero chevron angle rejected",
               solve(request).mStatus == Status::InvalidInput);

        request = makeRequest(
            Shape::Chevron, 2, 0.0, 0.000001);
        request.mChevronAngleDegrees = 0.00000000000001;
        const Result tiny_chevron = solve(request);
        ensure("tiny positive chevron remains scalable",
               tiny_chevron.mStatus == Status::Ok);
        ensureContract("tiny positive chevron", request, tiny_chevron);

        request = makeRequest(Shape::Horseshoe, 5);
        request.mHorseshoeOpeningDegrees = 360.0;
        ensure("complete horseshoe opening rejected",
               solve(request).mStatus == Status::InvalidInput);

        request = makeRequest(Shape::Spiral, 5);
        request.mSpiralTurns = 0.0;
        ensure("zero spiral turns rejected",
               solve(request).mStatus == Status::InvalidInput);

        request = makeRequest(Shape::Heart, 5);
        request.mAspectRatio = std::numeric_limits<F64>::infinity();
        ensure("infinite aspect ratio rejected",
               solve(request).mStatus == Status::InvalidInput);

        request = makeRequest(Shape::Sunburst, 5);
        request.mRayCount = 1;
        ensure("one-ray sunburst rejected",
               solve(request).mStatus == Status::InvalidInput);

        request = makeRequest(Shape::Zigzag, 5);
        request.mZigzagColumns = 1;
        ensure("one-column zigzag rejected",
               solve(request).mStatus == Status::InvalidInput);

        const Request baseline_request =
            makeRequest(Shape::Sunburst, 9);
        const Result baseline = solve(baseline_request);
        ensure("parameter fingerprint baseline succeeds",
               baseline.mStatus == Status::Ok);

        Request changed = baseline_request;
        changed.mLaneGapMeters += 0.25;
        Result changed_result = solve(changed);
        ensure("changed lane gap succeeds",
               changed_result.mStatus == Status::Ok);
        ensure("lane gap is fingerprinted",
               changed_result.mInputFingerprint !=
               baseline.mInputFingerprint);
        changed = baseline_request;
        changed.mChevronAngleDegrees += 1.0;
        changed_result = solve(changed);
        ensure("changed chevron angle succeeds",
               changed_result.mStatus == Status::Ok);
        ensure("chevron angle is fingerprinted",
               changed_result.mInputFingerprint !=
               baseline.mInputFingerprint);
        changed = baseline_request;
        changed.mHorseshoeOpeningDegrees += 1.0;
        changed_result = solve(changed);
        ensure("changed horseshoe opening succeeds",
               changed_result.mStatus == Status::Ok);
        ensure("horseshoe opening is fingerprinted",
               changed_result.mInputFingerprint !=
               baseline.mInputFingerprint);
        changed = baseline_request;
        changed.mSpiralTurns += 0.25;
        changed_result = solve(changed);
        ensure("changed spiral turns succeeds",
               changed_result.mStatus == Status::Ok);
        ensure("spiral turns are fingerprinted",
               changed_result.mInputFingerprint !=
               baseline.mInputFingerprint);
        changed = baseline_request;
        changed.mAspectRatio += 0.25;
        changed_result = solve(changed);
        ensure("changed aspect ratio succeeds",
               changed_result.mStatus == Status::Ok);
        ensure("aspect ratio is fingerprinted",
               changed_result.mInputFingerprint !=
               baseline.mInputFingerprint);
        changed = baseline_request;
        ++changed.mRayCount;
        changed_result = solve(changed);
        ensure("changed ray count succeeds",
               changed_result.mStatus == Status::Ok);
        ensure("ray count is fingerprinted",
               changed_result.mInputFingerprint !=
               baseline.mInputFingerprint);
        changed = baseline_request;
        ++changed.mZigzagColumns;
        changed_result = solve(changed);
        ensure("changed zigzag columns succeeds",
               changed_result.mStatus == Status::Ok);
        ensure("zigzag columns are fingerprinted",
               changed_result.mInputFingerprint !=
               baseline.mInputFingerprint);
    }

    // All newly supported formations share the same count, deterministic
    // member-order, envelope, and pair-clearance contract.
    template<> template<>
    void formation_solver_object::test<17>()
    {
        struct ShapeCase
        {
            Shape mShape;
            const char* mName;
        };
        const ShapeCase cases[] =
        {
            {Shape::SplitRow, "split row"},
            {Shape::SoulTrain, "soul train"},
            {Shape::Chevron, "chevron"},
            {Shape::Zigzag, "zigzag"},
            {Shape::Horseshoe, "horseshoe"},
            {Shape::Spiral, "spiral"},
            {Shape::Infinity, "infinity"},
            {Shape::Pentagram, "pentagram"},
            {Shape::StarOutline, "star outline"},
            {Shape::Diamond, "diamond"},
            {Shape::Cross, "cross"},
            {Shape::Arrow, "arrow"},
            {Shape::Heart, "heart"},
            {Shape::Sunburst, "sunburst"}
        };

        for (const ShapeCase& shape_case : cases)
        {
            Request request =
                makeRequest(shape_case.mShape, 23, 0.16, 0.75);
            request.mEdgeGapMeters = 0.11;
            request.mJitterMeters = 0.04;
            request.mSeed = 0x12345678ULL;
            for (size_t i = 0; i < request.mMembers.size(); ++i)
            {
                request.mMembers[i].mFootprintRadiusMeters +=
                    0.01 * static_cast<F64>(i % 4);
            }

            const Result first = solve(request);
            const Result repeated = solve(request);
            ensure(std::string(shape_case.mName) + ": all members placed",
                   first.mStatus == Status::Ok &&
                   first.mPlacedCount == request.mMembers.size());
            ensureContract(shape_case.mName, request, first);
            ensureSameSolution(
                std::string(shape_case.mName) + " deterministic",
                first, repeated);
        }
    }

    // Even sample counts deliberately exercise the geometric crossings. The
    // deterministic phase candidates may move sampling off a crossing, but
    // must never reduce the requested pair separation.
    template<> template<>
    void formation_solver_object::test<18>()
    {
        struct CrossingCase
        {
            Shape mShape;
            U32 mCount;
            const char* mName;
        };
        const CrossingCase cases[] =
        {
            {Shape::Infinity, 24, "infinity 24"},
            {Shape::Infinity, 40, "infinity 40"},
            {Shape::Pentagram, 20, "pentagram 20"},
            {Shape::Pentagram, 30, "pentagram 30"}
        };

        for (const CrossingCase& crossing_case : cases)
        {
            Request request =
                makeRequest(crossing_case.mShape,
                            crossing_case.mCount, 0.20, 0.90);
            request.mEdgeGapMeters = 0.15;
            const Result first = solve(request);
            const Result repeated = solve(request);
            ensure(std::string(crossing_case.mName) + ": feasible",
                   first.mStatus == Status::Ok);
            ensureContract(crossing_case.mName, request, first);
            ensureSameSolution(
                std::string(crossing_case.mName) + " deterministic",
                first, repeated);
        }
    }

    // Sunburst assigns every member to a positive radial rank. It does not
    // create one coincident hub slot per ray, including when the pair contract
    // itself permits zero separation.
    template<> template<>
    void formation_solver_object::test<19>()
    {
        Request request = makeRequest(Shape::Sunburst, 33, 0.0, 0.0);
        request.mRayCount = 8;
        const Result result = solve(request);
        ensure("zero-clearance sunburst succeeds",
               result.mStatus == Status::Ok);
        ensureContract("zero-clearance sunburst", request, result);

        for (size_t i = 0; i < result.mSlots.size(); ++i)
        {
            for (size_t j = i + 1; j < result.mSlots.size(); ++j)
            {
                ensure("sunburst never stacks hub points",
                       slotDistance(result.mSlots[i], result.mSlots[j]) >
                       TEST_EPSILON);
            }
        }
    }

    // The two rail formations preserve an explicit clear aisle; SoulTrain
    // additionally offsets one rail by half the deterministic pitch.
    template<> template<>
    void formation_solver_object::test<20>()
    {
        Request split = makeRequest(Shape::SplitRow, 6, 0.25, 0.50);
        split.mLaneGapMeters = 2.0;
        const Result split_result = solve(split);
        ensureContract("split rail aisle", split, split_result);
        ensureClose("split rail centre separation",
                    std::fabs(split_result.mSlots[0].mLocalY -
                              split_result.mSlots[1].mLocalY),
                    2.5);
        ensureClose("split rail clear aisle",
                    std::fabs(split_result.mSlots[0].mLocalY -
                              split_result.mSlots[1].mLocalY) -
                    split_result.mSlots[0].mFootprintRadiusMeters -
                    split_result.mSlots[1].mFootprintRadiusMeters,
                    2.0);

        Request soul = makeRequest(Shape::SoulTrain, 6, 0.20, 0.80);
        soul.mLaneGapMeters = 1.0;
        const Result soul_result = solve(soul);
        ensureContract("soul train rails", soul, soul_result);
        ensureClose("soul train half-pitch offset",
                    std::fabs(soul_result.mSlots[0].mLocalX -
                              soul_result.mSlots[1].mLocalX),
                    0.40);
    }
}
