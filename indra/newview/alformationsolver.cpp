/**
 * @file alformationsolver.cpp
 * @brief Pure, deterministic crowd-formation geometry solver.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "alformationsolver.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace ALFormationSolver
{
namespace
{

constexpr F64 PI = 3.141592653589793238462643383279502884;
constexpr F64 TWO_PI = 2.0 * PI;
constexpr F64 GOLDEN_ANGLE = 2.399963229728653322231555506633613853;
constexpr F64 SQRT_THREE_OVER_TWO = 0.866025403784438646763723170752936183;
constexpr F64 ORGANIC_ANGLE_NOISE = 0.35;
constexpr F64 MAX_ASPECT_RATIO = 1000.0;
constexpr F64 MAX_SPIRAL_TURNS = 64.0;
constexpr U32 MAX_GUIDE_SEGMENTS = 4096;
// Comparisons may absorb at most this many adjacent F64 representations.
// Unlike a magnitude-scaled epsilon, the allowance remains sub-millimetre
// even at the solver's 1e12-metre numeric ceiling.
constexpr U32 COMPARISON_ULPS = 2;
constexpr U64 FNV_OFFSET = 14695981039346656037ULL;
constexpr U64 FNV_PRIME = 1099511628211ULL;

struct Vec2
{
    F64 mX = 0.0;
    F64 mY = 0.0;
};

struct Layout
{
    std::vector<Vec2> mPoints;
    F64 mActualArcSweep = 0.0;
};

enum class ShapeClass
{
    Supported,
    Unsupported,
    Unknown
};

F64 advanceUlps(F64 value, bool upward)
{
    if (!std::isfinite(value))
    {
        return value;
    }
    const F64 direction = upward
        ? std::numeric_limits<F64>::infinity()
        : -std::numeric_limits<F64>::infinity();
    for (U32 i = 0; i < COMPARISON_ULPS; ++i)
    {
        value = std::nextafter(value, direction);
    }
    return value;
}

bool equalWithinUlps(F64 first, F64 second)
{
    if (!std::isfinite(first) || !std::isfinite(second))
    {
        return first == second;
    }
    return first >= advanceUlps(second, false) &&
           first <= advanceUlps(second, true);
}

bool lessThanWithTolerance(F64 value, F64 minimum)
{
    return value < advanceUlps(minimum, false);
}

bool fitsWithTolerance(F64 value, F64 maximum)
{
    return value <= advanceUlps(maximum, true);
}

bool validDistanceInput(F64 value)
{
    return std::isfinite(value) && value >= 0.0 &&
        value <= MAX_INPUT_METERS;
}

F64 length(const Vec2& value)
{
    return std::hypot(value.mX, value.mY);
}

F64 distance(const Vec2& a, const Vec2& b)
{
    return std::hypot(a.mX - b.mX, a.mY - b.mY);
}

U64 splitmix64(U64 value)
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}

F64 randomUnit(U64 seed, U64 member_id, U64 channel)
{
    const U64 bits = splitmix64(seed ^ splitmix64(member_id) ^
                                (channel * 0x9e3779b97f4a7c15ULL));
    // The high 53 bits map exactly into the representable [0, 1) interval.
    return static_cast<F64>(bits >> 11) *
        (1.0 / static_cast<F64>(1ULL << 53));
}

void hashBytes(U64& hash, const void* data, size_t size)
{
    const U8* bytes = static_cast<const U8*>(data);
    for (size_t i = 0; i < size; ++i)
    {
        hash ^= static_cast<U64>(bytes[i]);
        hash *= FNV_PRIME;
    }
}

void hashU64(U64& hash, U64 value)
{
    hashBytes(hash, &value, sizeof(value));
}

void hashF64(U64& hash, F64 value)
{
    // Canonicalise signed zero: it is the same solver input/position.
    if (value == 0.0)
    {
        value = 0.0;
    }
    U64 bits = 0;
    static_assert(sizeof(bits) == sizeof(value), "unexpected F64 width");
    std::memcpy(&bits, &value, sizeof(bits));
    hashU64(hash, bits);
}

F64 normalisedYaw(F64 yaw)
{
    F64 result = std::remainder(yaw, TWO_PI);
    return result == 0.0 ? 0.0 : result;
}

ShapeClass classifyShape(Shape shape)
{
    switch (shape)
    {
    case Shape::Row:
    case Shape::Grid:
    case Shape::StaggeredRows:
    case Shape::Ring:
    case Shape::Arc:
    case Shape::ConcentricRings:
    case Shape::Organic:
    case Shape::SplitRow:
    case Shape::SoulTrain:
    case Shape::Chevron:
    case Shape::Zigzag:
    case Shape::Horseshoe:
    case Shape::Spiral:
    case Shape::Infinity:
    case Shape::Pentagram:
    case Shape::StarOutline:
    case Shape::Diamond:
    case Shape::Cross:
    case Shape::Arrow:
    case Shape::Heart:
    case Shape::Sunburst:
    case Shape::Brigade:
    case Shape::SoulTrainMilitary:
    case Shape::StaggeredMilitary:
    case Shape::RingBrigade:
        return ShapeClass::Supported;

    case Shape::LegacyV:
    case Shape::LegacySpiral:
    case Shape::LegacyStaircase:
    case Shape::LegacyTunnel:
    case Shape::LegacyAmphitheater:
    case Shape::LegacyWedge:
    case Shape::LegacyCheckerboard:
    case Shape::LegacyCrescent:
    case Shape::LegacyPerimeterLine:
    case Shape::LegacyPath:
    case Shape::LegacyClusters:
        return ShapeClass::Unsupported;
    }
    return ShapeClass::Unknown;
}

bool validPolicy(EnvelopePolicy policy)
{
    return policy == EnvelopePolicy::FixedRadius ||
           policy == EnvelopePolicy::FitCount;
}

bool validateRequest(const Request& request, std::string& diagnostic)
{
    if (!validPolicy(request.mPolicy))
    {
        diagnostic = "unknown envelope policy";
        return false;
    }
    if (request.mMembers.size() > MAX_MEMBER_COUNT)
    {
        diagnostic = "member count exceeds the solver hard cap";
        return false;
    }
    if (!validDistanceInput(request.mRadiusMeters) ||
        !validDistanceInput(request.mCenterSpacingMeters) ||
        !validDistanceInput(request.mEdgeGapMeters) ||
        !validDistanceInput(request.mJitterMeters) ||
        !validDistanceInput(request.mLaneGapMeters))
    {
        diagnostic = "radius, spacing, gaps, and jitter must be finite non-negative metre values";
        return false;
    }
    if (!std::isfinite(request.mYawRadians))
    {
        diagnostic = "yaw must be finite";
        return false;
    }
    if (!std::isfinite(request.mArcSweepRadians) ||
        request.mArcSweepRadians < 0.0 ||
        request.mArcSweepRadians > advanceUlps(TWO_PI, true))
    {
        diagnostic = "arc sweep must be finite and between zero and 2*pi";
        return false;
    }
    if (request.mShape == Shape::Arc && request.mArcSweepRadians <= 0.0)
    {
        diagnostic = "arc sweep must be greater than zero";
        return false;
    }
    if (request.mGridColumns > MAX_MEMBER_COUNT)
    {
        diagnostic = "grid column count exceeds the solver hard cap";
        return false;
    }
    if (request.mGridRows > MAX_MEMBER_COUNT ||
        !validDistanceInput(request.mColumnSpacingMeters) ||
        !validDistanceInput(request.mRowSpacingMeters))
    {
        diagnostic = "brigade rows and rank/file spacing are invalid";
        return false;
    }
    if (!std::isfinite(request.mChevronAngleDegrees) ||
        request.mChevronAngleDegrees <= 0.0 ||
        request.mChevronAngleDegrees >= 180.0)
    {
        diagnostic = "chevron angle must be finite and between zero and 180 degrees";
        return false;
    }
    if (!std::isfinite(request.mHorseshoeOpeningDegrees) ||
        request.mHorseshoeOpeningDegrees <= 0.0 ||
        request.mHorseshoeOpeningDegrees >= 360.0)
    {
        diagnostic = "horseshoe opening must be finite and between zero and 360 degrees";
        return false;
    }
    if (!std::isfinite(request.mSpiralTurns) ||
        request.mSpiralTurns <= 0.0 ||
        request.mSpiralTurns > MAX_SPIRAL_TURNS)
    {
        diagnostic = "spiral turns must be finite, positive, and no greater than 64";
        return false;
    }
    if (!std::isfinite(request.mAspectRatio) ||
        request.mAspectRatio <= 0.0 ||
        request.mAspectRatio > MAX_ASPECT_RATIO)
    {
        diagnostic = "aspect ratio must be finite, positive, and no greater than 1000";
        return false;
    }
    if (request.mRayCount < 2 || request.mRayCount > MAX_MEMBER_COUNT)
    {
        diagnostic = "ray count must be between two and the solver hard cap";
        return false;
    }
    if (request.mZigzagColumns < 2 ||
        request.mZigzagColumns > MAX_MEMBER_COUNT)
    {
        diagnostic = "zigzag column count must be between two and the solver hard cap";
        return false;
    }
    if (!std::isfinite(request.mRankOffsetFraction) ||
        std::fabs(request.mRankOffsetFraction) > 16.0)
    {
        diagnostic = "rank offset fraction must be finite and within +/-16";
        return false;
    }

    for (size_t i = 0; i < request.mMembers.size(); ++i)
    {
        if (!validDistanceInput(request.mMembers[i].mFootprintRadiusMeters))
        {
            diagnostic = "member footprint radius must be finite and non-negative";
            return false;
        }
        for (size_t j = 0; j < i; ++j)
        {
            if (request.mMembers[i].mId == request.mMembers[j].mId)
            {
                diagnostic = "member IDs must be unique";
                return false;
            }
        }
    }
    return true;
}

U64 inputFingerprint(const Request& request)
{
    U64 hash = FNV_OFFSET;
    hashU64(hash, static_cast<U64>(request.mShape));
    hashU64(hash, static_cast<U64>(request.mPolicy));
    hashF64(hash, request.mRadiusMeters);
    hashF64(hash, request.mCenterSpacingMeters);
    hashF64(hash, request.mEdgeGapMeters);
    hashF64(hash, request.mJitterMeters);
    hashF64(hash, normalisedYaw(request.mYawRadians));
    hashU64(hash, request.mGridColumns);
    hashU64(hash, request.mGridRows);
    hashF64(hash, request.mColumnSpacingMeters);
    hashF64(hash, request.mRowSpacingMeters);
    hashF64(hash, request.mArcSweepRadians);
    hashF64(hash, request.mLaneGapMeters);
    hashF64(hash, request.mChevronAngleDegrees);
    hashF64(hash, request.mHorseshoeOpeningDegrees);
    hashF64(hash, request.mSpiralTurns);
    hashF64(hash, request.mAspectRatio);
    hashU64(hash, request.mRayCount);
    hashU64(hash, request.mZigzagColumns);
    hashF64(hash, request.mRankOffsetFraction);
    hashU64(hash, request.mSeed);
    hashU64(hash, static_cast<U64>(request.mMembers.size()));
    for (const Member& member : request.mMembers)
    {
        hashU64(hash, member.mId);
        hashF64(hash, member.mFootprintRadiusMeters);
    }
    return hash;
}

U64 solutionFingerprint(const Result& result)
{
    U64 hash = FNV_OFFSET;
    hashU64(hash, result.mInputFingerprint);
    hashU64(hash, static_cast<U64>(result.mStatus));
    hashU64(hash, result.mRequestedCount);
    hashU64(hash, result.mPlacedCount);
    hashU64(hash, result.mOverflowCount);
    hashF64(hash, result.mEnvelopeRadiusMeters);
    hashF64(hash, result.mRequiredRadiusMeters);
    hashF64(hash, result.mPlacedRadiusMeters);
    hashU64(hash, result.mHasPairwiseGap ? 1ULL : 0ULL);
    hashF64(hash, result.mActualMinEdgeGapMeters);
    hashF64(hash, result.mActualArcSweepRadians);
    for (const Slot& slot : result.mSlots)
    {
        hashU64(hash, slot.mMemberId);
        hashU64(hash, slot.mInputIndex);
        hashF64(hash, slot.mLocalX);
        hashF64(hash, slot.mLocalY);
        hashF64(hash, slot.mFootprintRadiusMeters);
    }
    for (U64 id : result.mOverflowMemberIds)
    {
        hashU64(hash, id);
    }
    return hash;
}

F64 pairRequirement(const Request& request, size_t first, size_t second)
{
    const F64 footprint_clearance =
        request.mMembers[first].mFootprintRadiusMeters +
        request.mMembers[second].mFootprintRadiusMeters +
        request.mEdgeGapMeters;
    return std::max(request.mCenterSpacingMeters, footprint_clearance);
}

F64 reservedPairRequirement(const Request& request, size_t first, size_t second)
{
    return pairRequirement(request, first, second) +
        2.0 * request.mJitterMeters;
}

F64 maximumReservedPairRequirement(const Request& request, size_t count)
{
    F64 result = 0.0;
    for (size_t i = 0; i < count; ++i)
    {
        for (size_t j = 0; j < i; ++j)
        {
            result = std::max(result, reservedPairRequirement(request, i, j));
        }
    }
    return result;
}

void centreExpandedBounds(const Request& request, size_t count,
                          std::vector<Vec2>& points)
{
    if (count == 0)
    {
        return;
    }

    const F64 first_extent =
        request.mMembers[0].mFootprintRadiusMeters + request.mJitterMeters;
    F64 min_x = points[0].mX - first_extent;
    F64 max_x = points[0].mX + first_extent;
    F64 min_y = points[0].mY - first_extent;
    F64 max_y = points[0].mY + first_extent;

    for (size_t i = 1; i < count; ++i)
    {
        const F64 extent =
            request.mMembers[i].mFootprintRadiusMeters + request.mJitterMeters;
        min_x = std::min(min_x, points[i].mX - extent);
        max_x = std::max(max_x, points[i].mX + extent);
        min_y = std::min(min_y, points[i].mY - extent);
        max_y = std::max(max_y, points[i].mY + extent);
    }

    const F64 shift_x = 0.5 * (min_x + max_x);
    const F64 shift_y = 0.5 * (min_y + max_y);
    for (Vec2& point : points)
    {
        point.mX -= shift_x;
        point.mY -= shift_y;
    }
}

F64 conservativeBaseRadius(const Request& request, size_t count,
                           const std::vector<Vec2>& points)
{
    F64 result = 0.0;
    for (size_t i = 0; i < count; ++i)
    {
        result = std::max(result,
            length(points[i]) +
            request.mMembers[i].mFootprintRadiusMeters +
            request.mJitterMeters);
    }
    return result;
}

void centrePointBounds(std::vector<Vec2>& points)
{
    if (points.empty())
    {
        return;
    }

    F64 min_x = points[0].mX;
    F64 max_x = points[0].mX;
    F64 min_y = points[0].mY;
    F64 max_y = points[0].mY;
    for (size_t i = 1; i < points.size(); ++i)
    {
        min_x = std::min(min_x, points[i].mX);
        max_x = std::max(max_x, points[i].mX);
        min_y = std::min(min_y, points[i].mY);
        max_y = std::max(max_y, points[i].mY);
    }

    const F64 shift_x = 0.5 * (min_x + max_x);
    const F64 shift_y = 0.5 * (min_y + max_y);
    for (Vec2& point : points)
    {
        point.mX -= shift_x;
        point.mY -= shift_y;
    }
}

bool pointsAreFinite(const std::vector<Vec2>& points)
{
    for (const Vec2& point : points)
    {
        if (!std::isfinite(point.mX) || !std::isfinite(point.mY) ||
            std::fabs(point.mX) > MAX_LAYOUT_RADIUS_METERS ||
            std::fabs(point.mY) > MAX_LAYOUT_RADIUS_METERS)
        {
            return false;
        }
    }
    return true;
}

bool scalePointsForReservedClearance(const Request& request, size_t count,
                                     bool preserve_zero_clearance_geometry,
                                     std::vector<Vec2>& points)
{
    F64 scale = 0.0;
    for (size_t i = 0; i < count; ++i)
    {
        for (size_t j = i + 1; j < count; ++j)
        {
            const F64 base_distance = distance(points[i], points[j]);
            const F64 required = reservedPairRequirement(request, i, j);
            // Canonical guides are dimensionless and may legitimately be
            // extremely small before scaling. Reject only a true coincidence;
            // the finite-result ceiling below decides whether a tiny positive
            // separation can be expanded safely.
            if (base_distance == 0.0)
            {
                if (required > 0.0)
                {
                    return false;
                }
                continue;
            }
            scale = std::max(scale, required / base_distance);
        }
    }

    if (preserve_zero_clearance_geometry && scale == 0.0 && count > 1)
    {
        // A Sunburst must remain a set of distinct rays even when callers
        // explicitly request zero-sized, zero-clearance members.
        scale = 1.0;
    }
    // Scale is dimensionless: only the resulting coordinates are subject to
    // the layout ceiling (a very small canonical guide can need scale > 1e12
    // while still producing representable metre positions).
    if (!std::isfinite(scale))
    {
        return false;
    }

    for (Vec2& point : points)
    {
        point.mX *= scale;
        point.mY *= scale;
    }
    centreExpandedBounds(request, count, points);
    return pointsAreFinite(points);
}

struct GuideSegment
{
    Vec2 mStart;
    Vec2 mEnd;
    F64  mLength = 0.0;
};

// Deterministic F64 arc-length sampling is shared by every outline and curve.
// Open guides include both endpoints. Closed guides emit count distinct
// samples and never append a duplicate copy of the first guide vertex.
bool sampleGuideByArcLength(const std::vector<Vec2>& vertices, bool closed,
                            size_t count, F64 phase,
                            std::vector<Vec2>& points)
{
    points.assign(count, Vec2());
    if (count <= 1)
    {
        return true;
    }
    if ((!closed && vertices.size() < 2) ||
        (closed && vertices.size() < 3))
    {
        return false;
    }

    std::vector<GuideSegment> segments;
    const size_t segment_count =
        closed ? vertices.size() : vertices.size() - 1;
    segments.reserve(segment_count);
    F64 total_length = 0.0;
    for (size_t i = 0; i < segment_count; ++i)
    {
        GuideSegment segment;
        segment.mStart = vertices[i];
        segment.mEnd = vertices[(i + 1) % vertices.size()];
        segment.mLength = distance(segment.mStart, segment.mEnd);
        if (!std::isfinite(segment.mLength))
        {
            return false;
        }
        if (segment.mLength > std::numeric_limits<F64>::epsilon())
        {
            total_length += segment.mLength;
            segments.push_back(segment);
        }
    }
    if (!std::isfinite(total_length) ||
        total_length <= std::numeric_limits<F64>::epsilon() ||
        segments.empty())
    {
        return false;
    }

    const F64 step = closed
        ? total_length / static_cast<F64>(count)
        : total_length / static_cast<F64>(count - 1);
    for (size_t i = 0; i < count; ++i)
    {
        F64 target = step * (static_cast<F64>(i) +
                             (closed ? phase : 0.0));
        if (closed)
        {
            target = std::fmod(target, total_length);
        }
        else
        {
            target = std::min(target, total_length);
        }

        F64 preceding = 0.0;
        size_t segment_index = 0;
        while (segment_index + 1 < segments.size() &&
               target > preceding + segments[segment_index].mLength)
        {
            preceding += segments[segment_index].mLength;
            ++segment_index;
        }
        const GuideSegment& segment = segments[segment_index];
        const F64 fraction = std::max(0.0, std::min(
            1.0, (target - preceding) / segment.mLength));
        points[i].mX = segment.mStart.mX +
            fraction * (segment.mEnd.mX - segment.mStart.mX);
        points[i].mY = segment.mStart.mY +
            fraction * (segment.mEnd.mY - segment.mStart.mY);
    }
    return pointsAreFinite(points);
}

bool buildGuideLayout(const Request& request, size_t count,
                      const std::vector<Vec2>& vertices, bool closed,
                      bool crossing, Layout& layout)
{
    if (count <= 1)
    {
        layout.mPoints.assign(count, Vec2());
        return true;
    }

    // Self-crossing guides can land two samples on the same crossing. The
    // fixed phase sequence changes sample locations, never the requested
    // clearance. The feasible candidate with the smallest fitted radius wins.
    static const F64 CROSSING_PHASES[] =
    {
        0.0, 0.5, 0.25, 0.75, 0.125, 0.375, 0.625, 0.875
    };
    const size_t phase_count =
        crossing && closed
            ? sizeof(CROSSING_PHASES) / sizeof(CROSSING_PHASES[0])
            : 1;

    bool have_best = false;
    F64 best_radius = 0.0;
    std::vector<Vec2> candidate;
    for (size_t phase_index = 0; phase_index < phase_count; ++phase_index)
    {
        const F64 phase = crossing && closed
            ? CROSSING_PHASES[phase_index] : 0.0;
        if (!sampleGuideByArcLength(
                vertices, closed, count, phase, candidate))
        {
            continue;
        }
        centrePointBounds(candidate);
        if (!scalePointsForReservedClearance(
                request, count, false, candidate))
        {
            continue;
        }

        const F64 radius =
            conservativeBaseRadius(request, count, candidate);
        const bool smaller = !have_best ||
            radius < advanceUlps(best_radius, false);
        if (smaller)
        {
            have_best = true;
            best_radius = radius;
            layout.mPoints = candidate;
        }
    }
    return have_best;
}

bool buildParallelRails(const Request& request, size_t count, bool staggered,
                        Layout& layout)
{
    layout.mPoints.assign(count, Vec2());
    if (count <= 1)
    {
        return true;
    }

    const F64 pitch = maximumReservedPairRequirement(request, count);
    F64 largest_extent = 0.0;
    for (size_t i = 0; i < count; ++i)
    {
        largest_extent = std::max(
            largest_extent,
            request.mMembers[i].mFootprintRadiusMeters +
            request.mJitterMeters);
    }
    const F64 lane_separation = std::max(
        pitch, request.mLaneGapMeters + 2.0 * largest_extent);
    if (!std::isfinite(lane_separation) ||
        lane_separation > MAX_LAYOUT_RADIUS_METERS)
    {
        return false;
    }

    for (size_t i = 0; i < count; ++i)
    {
        const U32 lane = static_cast<U32>(i & 1U);
        const size_t rank = i / 2;
        layout.mPoints[i].mX =
            (static_cast<F64>(rank) +
             (staggered && lane ? 0.5 : 0.0)) * pitch;
        layout.mPoints[i].mY =
            lane ? 0.5 * lane_separation : -0.5 * lane_separation;
    }
    centreExpandedBounds(request, count, layout.mPoints);
    return pointsAreFinite(layout.mPoints);
}

bool buildChevron(const Request& request, size_t count, Layout& layout)
{
    const F64 half_angle =
        0.5 * request.mChevronAngleDegrees * PI / 180.0;
    const F64 x = -std::cos(half_angle);
    const F64 y = request.mAspectRatio * std::sin(half_angle);
    std::vector<Vec2> guide(3);
    guide[0].mX = x;
    guide[0].mY = y;
    guide[2].mX = x;
    guide[2].mY = -y;
    return buildGuideLayout(request, count, guide, false, false, layout);
}

bool buildZigzag(const Request& request, size_t count, Layout& layout)
{
    std::vector<Vec2> guide(request.mZigzagColumns);
    for (U32 i = 0; i < request.mZigzagColumns; ++i)
    {
        guide[i].mX = static_cast<F64>(i);
        guide[i].mY = (i & 1U ? -0.5 : 0.5) * request.mAspectRatio;
    }
    return buildGuideLayout(request, count, guide, false, false, layout);
}

bool buildHorseshoe(const Request& request, size_t count, Layout& layout)
{
    const F64 opening =
        request.mHorseshoeOpeningDegrees * PI / 180.0;
    const F64 sweep = TWO_PI - opening;
    const U32 segments = std::max<U32>(
        64, static_cast<U32>(std::ceil(512.0 * sweep / TWO_PI)));
    std::vector<Vec2> guide(segments + 1);
    for (U32 i = 0; i <= segments; ++i)
    {
        const F64 angle =
            0.5 * opening +
            sweep * static_cast<F64>(i) / static_cast<F64>(segments);
        guide[i].mX = std::cos(angle);
        guide[i].mY = request.mAspectRatio * std::sin(angle);
    }
    return buildGuideLayout(request, count, guide, false, false, layout);
}

bool buildSpiral(const Request& request, size_t count, Layout& layout)
{
    const U32 segments = std::min<U32>(
        MAX_GUIDE_SEGMENTS,
        std::max<U32>(64, static_cast<U32>(
            std::ceil(request.mSpiralTurns * 128.0))));
    const F64 sweep = TWO_PI * request.mSpiralTurns;
    std::vector<Vec2> guide(segments + 1);
    for (U32 i = 0; i <= segments; ++i)
    {
        const F64 fraction =
            static_cast<F64>(i) / static_cast<F64>(segments);
        const F64 angle = sweep * fraction;
        guide[i].mX = fraction * std::cos(angle);
        guide[i].mY =
            request.mAspectRatio * fraction * std::sin(angle);
    }
    return buildGuideLayout(request, count, guide, false, false, layout);
}

bool buildInfinity(const Request& request, size_t count, Layout& layout)
{
    constexpr U32 SEGMENTS = 1024;
    std::vector<Vec2> guide(SEGMENTS);
    for (U32 i = 0; i < SEGMENTS; ++i)
    {
        const F64 angle =
            TWO_PI * static_cast<F64>(i) / static_cast<F64>(SEGMENTS);
        guide[i].mX = std::sin(angle);
        guide[i].mY =
            request.mAspectRatio * std::sin(angle) * std::cos(angle);
    }
    return buildGuideLayout(request, count, guide, true, true, layout);
}

bool buildPentagram(const Request& request, size_t count, Layout& layout)
{
    std::vector<Vec2> guide(5);
    for (U32 i = 0; i < 5; ++i)
    {
        const U32 vertex = (2U * i) % 5U;
        const F64 angle = TWO_PI * static_cast<F64>(vertex) / 5.0;
        guide[i].mX = std::cos(angle);
        guide[i].mY = request.mAspectRatio * std::sin(angle);
    }
    return buildGuideLayout(request, count, guide, true, true, layout);
}

bool buildStarOutline(const Request& request, size_t count, Layout& layout)
{
    std::vector<Vec2> guide(10);
    for (U32 i = 0; i < 10; ++i)
    {
        const F64 radius = i & 1U ? 0.45 : 1.0;
        const F64 angle = PI * static_cast<F64>(i) / 5.0;
        guide[i].mX = radius * std::cos(angle);
        guide[i].mY =
            request.mAspectRatio * radius * std::sin(angle);
    }
    return buildGuideLayout(request, count, guide, true, false, layout);
}

bool buildDiamond(const Request& request, size_t count, Layout& layout)
{
    std::vector<Vec2> guide(4);
    guide[0].mX = 1.0;
    guide[1].mY = request.mAspectRatio;
    guide[2].mX = -1.0;
    guide[3].mY = -request.mAspectRatio;
    return buildGuideLayout(request, count, guide, true, false, layout);
}

bool buildCross(const Request& request, size_t count, Layout& layout)
{
    constexpr F64 ARM = 0.35;
    const F64 points[][2] =
    {
        { 1.0,  ARM}, { ARM,  ARM}, { ARM,  1.0},
        {-ARM,  1.0}, {-ARM,  ARM}, {-1.0,  ARM},
        {-1.0, -ARM}, {-ARM, -ARM}, {-ARM, -1.0},
        { ARM, -1.0}, { ARM, -ARM}, { 1.0, -ARM}
    };
    std::vector<Vec2> guide(
        sizeof(points) / sizeof(points[0]));
    for (size_t i = 0; i < guide.size(); ++i)
    {
        guide[i].mX = points[i][0];
        guide[i].mY = request.mAspectRatio * points[i][1];
    }
    return buildGuideLayout(request, count, guide, true, false, layout);
}

bool buildArrow(const Request& request, size_t count, Layout& layout)
{
    const F64 points[][2] =
    {
        {-1.0,  0.35}, { 0.20,  0.35}, { 0.20,  0.75},
        { 1.0,  0.0},  { 0.20, -0.75}, { 0.20, -0.35},
        {-1.0, -0.35}
    };
    std::vector<Vec2> guide(
        sizeof(points) / sizeof(points[0]));
    for (size_t i = 0; i < guide.size(); ++i)
    {
        guide[i].mX = points[i][0];
        guide[i].mY = request.mAspectRatio * points[i][1];
    }
    return buildGuideLayout(request, count, guide, true, false, layout);
}

bool buildHeart(const Request& request, size_t count, Layout& layout)
{
    constexpr U32 SEGMENTS = 1024;
    std::vector<Vec2> guide(SEGMENTS);
    for (U32 i = 0; i < SEGMENTS; ++i)
    {
        const F64 angle =
            TWO_PI * static_cast<F64>(i) / static_cast<F64>(SEGMENTS);
        const F64 sine = std::sin(angle);
        guide[i].mX = sine * sine * sine;
        guide[i].mY = request.mAspectRatio *
            (13.0 * std::cos(angle) -
             5.0 * std::cos(2.0 * angle) -
             2.0 * std::cos(3.0 * angle) -
             std::cos(4.0 * angle)) / 17.0;
    }
    return buildGuideLayout(request, count, guide, true, false, layout);
}

bool buildSunburst(const Request& request, size_t count, Layout& layout)
{
    layout.mPoints.assign(count, Vec2());
    if (count <= 1)
    {
        return true;
    }

    const U32 rays = std::min<U32>(
        request.mRayCount, static_cast<U32>(count));
    for (size_t i = 0; i < count; ++i)
    {
        const U32 ray = static_cast<U32>(i % rays);
        const U32 rank = static_cast<U32>(i / rays) + 1U;
        const F64 angle =
            TWO_PI * static_cast<F64>(ray) / static_cast<F64>(rays);
        layout.mPoints[i].mX =
            static_cast<F64>(rank) * std::cos(angle);
        layout.mPoints[i].mY =
            request.mAspectRatio * static_cast<F64>(rank) * std::sin(angle);
    }
    centrePointBounds(layout.mPoints);
    return scalePointsForReservedClearance(
        request, count, true, layout.mPoints);
}

bool buildRow(const Request& request, size_t count, Layout& layout)
{
    layout.mPoints.assign(count, Vec2());
    for (size_t i = 1; i < count; ++i)
    {
        layout.mPoints[i].mX = layout.mPoints[i - 1].mX +
            reservedPairRequirement(request, i - 1, i);
    }
    centreExpandedBounds(request, count, layout.mPoints);
    return true;
}

void buildGridCandidate(const Request& request, size_t count, U32 columns,
                        bool staggered, F64 pitch,
                        std::vector<Vec2>& points)
{
    points.assign(count, Vec2());
    if (count == 0)
    {
        return;
    }

    for (size_t i = 0; i < count; ++i)
    {
        const U32 row = static_cast<U32>(i / columns);
        const U32 column = static_cast<U32>(i % columns);
        points[i].mX = static_cast<F64>(column) * pitch;
        points[i].mY = static_cast<F64>(row) * pitch;
        if (staggered)
        {
            points[i].mX += (row & 1U) ? 0.5 * pitch : 0.0;
            points[i].mY *= SQRT_THREE_OVER_TWO;
        }
    }
    centreExpandedBounds(request, count, points);
}

bool buildSmartGrid(const Request& request, size_t count, bool staggered,
                    Layout& layout)
{
    if (count == 0)
    {
        layout.mPoints.clear();
        return true;
    }

    U32 first_columns = 1;
    U32 last_columns = static_cast<U32>(count);
    if (request.mGridColumns != 0)
    {
        first_columns = std::min(request.mGridColumns,
                                 static_cast<U32>(count));
        last_columns = first_columns;
    }

    bool have_best = false;
    F64 best_radius = 0.0;
    U32 best_aspect = 0;
    U32 best_columns = 0;
    std::vector<Vec2> candidate;
    const F64 pitch = maximumReservedPairRequirement(request, count);

    for (U32 columns = first_columns; columns <= last_columns; ++columns)
    {
        buildGridCandidate(request, count, columns, staggered, pitch, candidate);
        const F64 radius = conservativeBaseRadius(request, count, candidate);
        const U32 rows = static_cast<U32>(
            (count + static_cast<size_t>(columns) - 1) / columns);
        const U32 aspect = columns > rows ? columns - rows : rows - columns;

        const bool smaller = !have_best ||
            radius < advanceUlps(best_radius, false);
        const bool tied = have_best &&
            equalWithinUlps(radius, best_radius);
        if (smaller ||
            (tied && (aspect < best_aspect ||
                      (aspect == best_aspect && columns > best_columns))))
        {
            have_best = true;
            best_radius = radius;
            best_aspect = aspect;
            best_columns = columns;
            layout.mPoints = candidate;
        }
    }
    return have_best;
}

bool buildBrigade(const Request& request, size_t count, Layout& layout)
{
    if (count == 0)
    {
        layout.mPoints.clear();
        return true;
    }
    U32 columns = request.mGridColumns;
    if (columns == 0 && request.mGridRows != 0)
    {
        columns = static_cast<U32>((count + request.mGridRows - 1) /
                                   request.mGridRows);
    }
    if (columns == 0)
    {
        columns = static_cast<U32>(std::ceil(std::sqrt((F64)count)));
    }
    columns = std::max(1U, std::min(columns, static_cast<U32>(count)));
    const F64 clearance = maximumReservedPairRequirement(request, count);
    const F64 file_pitch = std::max(clearance,
        request.mColumnSpacingMeters > 0.0
            ? request.mColumnSpacingMeters : request.mCenterSpacingMeters);
    const F64 rank_pitch = std::max(clearance,
        request.mRowSpacingMeters > 0.0
            ? request.mRowSpacingMeters : request.mCenterSpacingMeters);
    layout.mPoints.assign(count, Vec2());
    for (size_t i = 0; i < count; ++i)
    {
        const U32 rank = static_cast<U32>(i / columns);
        const U32 file = static_cast<U32>(i % columns);
        layout.mPoints[i].mX = static_cast<F64>(file) * file_pitch;
        layout.mPoints[i].mY = -static_cast<F64>(rank) * rank_pitch;
    }
    centreExpandedBounds(request, count, layout.mPoints);
    return true;
}

// Resolve the number of files (columns) for a rank/file grid of `population`
// members. An explicit request column count wins; otherwise the row count is
// inverted, and failing that the most compact near-square grid is chosen.
U32 resolveGridColumns(const Request& request, size_t population)
{
    U32 columns = request.mGridColumns;
    if (columns == 0 && request.mGridRows != 0)
    {
        columns = static_cast<U32>(
            (population + request.mGridRows - 1) / request.mGridRows);
    }
    if (columns == 0)
    {
        columns = static_cast<U32>(
            std::ceil(std::sqrt(static_cast<F64>(population))));
    }
    return std::max(1U, std::min(columns,
        static_cast<U32>(std::max<size_t>(1, population))));
}

// Two facing rank/file blocks separated by a clear central aisle. Each block is
// an identical grid; members fill the near block first, then the far block, so
// the corridor is symmetric across the aisle for the even counts the panel
// produces. The aisle runs along local +X (the yaw axis), blocks straddle it in
// Y, so FACING_FACE_ACROSS/FACING_AISLE naturally turn the ranks inward.
bool buildSoulTrainMilitary(const Request& request, size_t count, Layout& layout)
{
    layout.mPoints.assign(count, Vec2());
    if (count <= 1)
    {
        return true;
    }

    const size_t near_count = (count + 1) / 2;
    const size_t far_count = count - near_count;
    const size_t per_side = std::max(near_count, far_count);
    const U32 columns = resolveGridColumns(request, per_side);

    const F64 clearance = maximumReservedPairRequirement(request, count);
    const F64 file_pitch = std::max(clearance,
        request.mColumnSpacingMeters > 0.0
            ? request.mColumnSpacingMeters : request.mCenterSpacingMeters);
    const F64 rank_pitch = std::max(clearance,
        request.mRowSpacingMeters > 0.0
            ? request.mRowSpacingMeters : request.mCenterSpacingMeters);
    // The two innermost ranks sit one aisle apart, so the aisle can never be
    // narrower than the pairwise clearance without violating the contract.
    const F64 aisle = std::max(clearance, request.mLaneGapMeters);
    if (!std::isfinite(file_pitch) || !std::isfinite(rank_pitch) ||
        !std::isfinite(aisle))
    {
        return false;
    }

    for (size_t i = 0; i < count; ++i)
    {
        const bool near_side = i < near_count;
        const size_t local = near_side ? i : i - near_count;
        const U32 file = static_cast<U32>(local % columns);
        const U32 rank = static_cast<U32>(local / columns);
        const F64 side_sign = near_side ? 1.0 : -1.0;
        layout.mPoints[i].mX = static_cast<F64>(file) * file_pitch;
        layout.mPoints[i].mY = side_sign *
            (0.5 * aisle + static_cast<F64>(rank) * rank_pitch);
    }
    centreExpandedBounds(request, count, layout.mPoints);
    return pointsAreFinite(layout.mPoints);
}

// A rank/file grid whose ranks are laterally offset. A non-negative offset is a
// classic brick stagger (odd ranks shifted half a file by default); a negative
// offset is a cumulative echelon that steps every rank diagonally. Rank pitch
// always covers the pairwise clearance, so any offset stays contract-safe.
bool buildStaggeredMilitary(const Request& request, size_t count, Layout& layout)
{
    layout.mPoints.assign(count, Vec2());
    if (count <= 1)
    {
        return true;
    }

    const U32 columns = resolveGridColumns(request, count);
    const F64 clearance = maximumReservedPairRequirement(request, count);
    const F64 file_pitch = std::max(clearance,
        request.mColumnSpacingMeters > 0.0
            ? request.mColumnSpacingMeters : request.mCenterSpacingMeters);
    const F64 rank_pitch = std::max(clearance,
        request.mRowSpacingMeters > 0.0
            ? request.mRowSpacingMeters : request.mCenterSpacingMeters);
    if (!std::isfinite(file_pitch) || !std::isfinite(rank_pitch))
    {
        return false;
    }
    const F64 offset = request.mRankOffsetFraction;

    for (size_t i = 0; i < count; ++i)
    {
        const U32 file = static_cast<U32>(i % columns);
        const U32 rank = static_cast<U32>(i / columns);
        const F64 shift = offset >= 0.0
            ? ((rank & 1U) ? offset * file_pitch : 0.0)
            : static_cast<F64>(rank) * (-offset) * file_pitch;
        layout.mPoints[i].mX = static_cast<F64>(file) * file_pitch + shift;
        layout.mPoints[i].mY = -static_cast<F64>(rank) * rank_pitch;
    }
    centreExpandedBounds(request, count, layout.mPoints);
    return pointsAreFinite(layout.mPoints);
}

// A military brigade wrapped into concentric rings around the centre. The grid
// "files" (columns) become the number of members placed on every ring and the
// grid "ranks" (rows) become the number of concentric rings, so the same
// ranks x files block used by the Brigade wraps into ranks tiers of files
// members. Members fill ring-by-ring from the inside out, exactly matching the
// Brigade's rank-major fill order.
//
// Geometry contract. Two members on different rings are always at least one
// ring gap apart radially (reverse triangle inequality: |A|=R, |B|=R+d implies
// |A-B| >= d), so the ring gap alone guarantees inter-ring clearance and only
// the along-ring chord constrains each ring's radius. The ring gap therefore is
// floored to the pairwise clearance and the innermost radius is floored so a
// full ring's members clear each other AND honour the requested file gap. Outer
// rings share the member count at a larger radius, so their chords only grow.
// mRankOffsetFraction rotates whole rings: a non-negative value is a polar brick
// (odd rings rotated by that fraction of the slot pitch, 0.5 interleaves members
// exactly halfway between the neighbouring ring's), a negative value is a
// cumulative echelon twist. Because the shift is purely angular it can never
// reduce the guaranteed radial clearance, so any offset stays contract-safe.
bool buildRingBrigade(const Request& request, size_t count, Layout& layout)
{
    layout.mPoints.assign(count, Vec2());
    if (count <= 1)
    {
        return true;
    }

    // resolveGridColumns yields the number of files; here that is the count of
    // members placed on each concentric ring. Ranks (rings) follow implicitly as
    // ceil(count / per_ring) via the rank-major index split below.
    const U32 per_ring = resolveGridColumns(request, count);
    const F64 clearance = maximumReservedPairRequirement(request, count);
    // Rank spacing is the ring spacing: the radial gap between consecutive rings
    // and thus each ring's distance from the centre. File spacing becomes the
    // minimum along-ring chord between adjacent members. Neither can drop below
    // the pairwise clearance, so the layout is contract-safe before any offset.
    const F64 ring_spacing = std::max(clearance,
        request.mRowSpacingMeters > 0.0
            ? request.mRowSpacingMeters : request.mCenterSpacingMeters);
    const F64 file_pitch = std::max(clearance,
        request.mColumnSpacingMeters > 0.0
            ? request.mColumnSpacingMeters : request.mCenterSpacingMeters);
    if (!std::isfinite(ring_spacing) || !std::isfinite(file_pitch))
    {
        return false;
    }

    const F64 angular_step = TWO_PI / static_cast<F64>(per_ring);
    // Innermost radius: at least one ring gap out from the centre, and large
    // enough that a full ring's adjacent members are file_pitch apart along the
    // chord. Only per_ring >= 2 has an intra-ring chord to satisfy.
    F64 base_radius = ring_spacing;
    if (per_ring >= 2)
    {
        const F64 chord_at_unit_radius = 2.0 * std::sin(0.5 * angular_step);
        if (chord_at_unit_radius > std::numeric_limits<F64>::epsilon())
        {
            base_radius =
                std::max(base_radius, file_pitch / chord_at_unit_radius);
        }
    }
    if (!std::isfinite(base_radius) || base_radius > MAX_LAYOUT_RADIUS_METERS)
    {
        return false;
    }

    const F64 offset = request.mRankOffsetFraction;
    for (size_t i = 0; i < count; ++i)
    {
        const U32 slot = static_cast<U32>(i % per_ring);
        const U32 ring = static_cast<U32>(i / per_ring);
        const F64 phase = offset >= 0.0
            ? ((ring & 1U) ? offset * angular_step : 0.0)
            : static_cast<F64>(ring) * (-offset) * angular_step;
        const F64 radius =
            base_radius + static_cast<F64>(ring) * ring_spacing;
        if (!std::isfinite(radius) || radius > MAX_LAYOUT_RADIUS_METERS)
        {
            return false;
        }
        const F64 angle = phase + static_cast<F64>(slot) * angular_step;
        layout.mPoints[i].mX = radius * std::cos(angle);
        layout.mPoints[i].mY = radius * std::sin(angle);
    }
    // Concentric rings are naturally centred on the origin (the anchor), and any
    // whole-ring rotation keeps every member on its circle, so -- like buildRing
    // and buildConcentricRings -- the layout is left uncentred to keep the anchor
    // as the exact ring centre even when a trailing ring is partial.
    return pointsAreFinite(layout.mPoints);
}

bool buildRing(const Request& request, size_t count, Layout& layout)
{
    layout.mPoints.assign(count, Vec2());
    if (count <= 1)
    {
        return true;
    }

    F64 radius = 0.0;
    for (size_t i = 0; i < count; ++i)
    {
        for (size_t j = i + 1; j < count; ++j)
        {
            const F64 angle = TWO_PI * static_cast<F64>(j - i) /
                              static_cast<F64>(count);
            const F64 chord_at_unit_radius = 2.0 * std::fabs(std::sin(0.5 * angle));
            const F64 required = reservedPairRequirement(request, i, j);
            if (chord_at_unit_radius <= std::numeric_limits<F64>::epsilon())
            {
                if (required > 0.0)
                {
                    return false;
                }
                continue;
            }
            radius = std::max(radius, required / chord_at_unit_radius);
        }
    }
    if (!std::isfinite(radius) || radius > MAX_LAYOUT_RADIUS_METERS)
    {
        return false;
    }

    for (size_t i = 0; i < count; ++i)
    {
        const F64 angle = TWO_PI * static_cast<F64>(i) /
                          static_cast<F64>(count);
        layout.mPoints[i].mX = radius * std::cos(angle);
        layout.mPoints[i].mY = radius * std::sin(angle);
    }
    return true;
}

bool buildArc(const Request& request, size_t count, Layout& layout)
{
    layout.mPoints.assign(count, Vec2());
    if (count <= 1)
    {
        layout.mActualArcSweep = 0.0;
        return true;
    }

    const F64 requested_sweep = std::min(request.mArcSweepRadians, TWO_PI);
    if (equalWithinUlps(requested_sweep, TWO_PI))
    {
        layout.mActualArcSweep = TWO_PI;
        return buildRing(request, count, layout);
    }

    const F64 step = std::min(
        requested_sweep / static_cast<F64>(count - 1),
        TWO_PI / static_cast<F64>(count));
    layout.mActualArcSweep = step * static_cast<F64>(count - 1);

    F64 radius = 0.0;
    for (size_t i = 0; i < count; ++i)
    {
        for (size_t j = i + 1; j < count; ++j)
        {
            const F64 angle = step * static_cast<F64>(j - i);
            const F64 chord_at_unit_radius = 2.0 * std::fabs(std::sin(0.5 * angle));
            const F64 required = reservedPairRequirement(request, i, j);
            if (chord_at_unit_radius <= std::numeric_limits<F64>::epsilon())
            {
                if (required > 0.0)
                {
                    return false;
                }
                continue;
            }
            radius = std::max(radius, required / chord_at_unit_radius);
        }
    }
    if (!std::isfinite(radius) || radius > MAX_LAYOUT_RADIUS_METERS)
    {
        return false;
    }

    const F64 first_angle = -0.5 * layout.mActualArcSweep;
    for (size_t i = 0; i < count; ++i)
    {
        const F64 angle = first_angle + step * static_cast<F64>(i);
        layout.mPoints[i].mX = radius * std::cos(angle);
        layout.mPoints[i].mY = radius * std::sin(angle);
    }
    return true;
}

bool buildConcentricRings(const Request& request, size_t count, Layout& layout)
{
    layout.mPoints.assign(count, Vec2());
    if (count <= 1)
    {
        return true;
    }

    const F64 pitch = maximumReservedPairRequirement(request, count);
    if (pitch == 0.0)
    {
        return true;
    }

    size_t next = 1;
    U32 ring = 1;
    while (next < count)
    {
        const F64 radius = pitch * static_cast<F64>(ring);
        if (!std::isfinite(radius) || radius > MAX_LAYOUT_RADIUS_METERS)
        {
            return false;
        }

        const F64 ratio = std::min(1.0, pitch / (2.0 * radius));
        const F64 minimum_angle = 2.0 * std::asin(ratio);
        // Advance only two representable values before floor(), enough to
        // recover an analytically integral capacity rounded just below the
        // integer without the old magnitude-independent decimal fudge.
        U32 capacity = static_cast<U32>(std::floor(
            advanceUlps(TWO_PI / minimum_angle, true)));
        capacity = std::max(1U, capacity);

        const F64 phase = TWO_PI *
            randomUnit(request.mSeed, static_cast<U64>(ring), 0x434f4e43ULL);
        const U32 used = std::min<U32>(
            capacity, static_cast<U32>(count - next));
        for (U32 i = 0; i < used; ++i)
        {
            const F64 angle = phase + TWO_PI * static_cast<F64>(i) /
                              static_cast<F64>(capacity);
            layout.mPoints[next].mX = radius * std::cos(angle);
            layout.mPoints[next].mY = radius * std::sin(angle);
            ++next;
        }
        ++ring;
    }
    return true;
}

bool buildOrganic(const Request& request, size_t count, Layout& layout)
{
    layout.mPoints.assign(count, Vec2());
    if (count <= 1)
    {
        return true;
    }

    const F64 phase = TWO_PI *
        randomUnit(request.mSeed, 0x4f5247414e4943ULL, 0);
    for (size_t i = 1; i < count; ++i)
    {
        const U64 member_id = request.mMembers[i].mId;
        const F64 noise = ORGANIC_ANGLE_NOISE *
            (randomUnit(request.mSeed, member_id,
                        0x414e474c45ULL + static_cast<U64>(i)) - 0.5);
        const F64 angle = phase + GOLDEN_ANGLE * static_cast<F64>(i) + noise;
        const F64 radius = std::sqrt(static_cast<F64>(i));
        layout.mPoints[i].mX = radius * std::cos(angle);
        layout.mPoints[i].mY = radius * std::sin(angle);
    }

    F64 scale = 0.0;
    for (size_t i = 0; i < count; ++i)
    {
        for (size_t j = i + 1; j < count; ++j)
        {
            const F64 base_distance = distance(layout.mPoints[i],
                                               layout.mPoints[j]);
            const F64 required = reservedPairRequirement(request, i, j);
            if (base_distance <= std::numeric_limits<F64>::epsilon())
            {
                if (required > 0.0)
                {
                    return false;
                }
                continue;
            }
            scale = std::max(scale, required / base_distance);
        }
    }
    if (!std::isfinite(scale) || scale > MAX_LAYOUT_RADIUS_METERS)
    {
        return false;
    }

    for (Vec2& point : layout.mPoints)
    {
        point.mX *= scale;
        point.mY *= scale;
    }
    centreExpandedBounds(request, count, layout.mPoints);
    return true;
}

bool buildCanonicalLayout(const Request& request, size_t count, Layout& layout)
{
    layout = Layout();
    switch (request.mShape)
    {
    case Shape::Row:
        return buildRow(request, count, layout);
    case Shape::Grid:
        return buildSmartGrid(request, count, false, layout);
    case Shape::StaggeredRows:
        return buildSmartGrid(request, count, true, layout);
    case Shape::Ring:
        return buildRing(request, count, layout);
    case Shape::Arc:
        return buildArc(request, count, layout);
    case Shape::ConcentricRings:
        return buildConcentricRings(request, count, layout);
    case Shape::Organic:
        return buildOrganic(request, count, layout);
    case Shape::SplitRow:
        return buildParallelRails(request, count, false, layout);
    case Shape::SoulTrain:
        return buildParallelRails(request, count, true, layout);
    case Shape::Chevron:
        return buildChevron(request, count, layout);
    case Shape::Zigzag:
        return buildZigzag(request, count, layout);
    case Shape::Horseshoe:
        return buildHorseshoe(request, count, layout);
    case Shape::Spiral:
        return buildSpiral(request, count, layout);
    case Shape::Infinity:
        return buildInfinity(request, count, layout);
    case Shape::Pentagram:
        return buildPentagram(request, count, layout);
    case Shape::StarOutline:
        return buildStarOutline(request, count, layout);
    case Shape::Diamond:
        return buildDiamond(request, count, layout);
    case Shape::Cross:
        return buildCross(request, count, layout);
    case Shape::Arrow:
        return buildArrow(request, count, layout);
    case Shape::Heart:
        return buildHeart(request, count, layout);
    case Shape::Sunburst:
        return buildSunburst(request, count, layout);
    case Shape::Brigade:
        return buildBrigade(request, count, layout);
    case Shape::SoulTrainMilitary:
        return buildSoulTrainMilitary(request, count, layout);
    case Shape::StaggeredMilitary:
        return buildStaggeredMilitary(request, count, layout);
    case Shape::RingBrigade:
        return buildRingBrigade(request, count, layout);
    default:
        return false;
    }
}

void applyJitter(const Request& request, size_t count, Layout& layout)
{
    if (request.mJitterMeters == 0.0)
    {
        return;
    }

    for (size_t i = 0; i < count; ++i)
    {
        const U64 id = request.mMembers[i].mId;
        const U64 index = static_cast<U64>(i);
        const F64 radial_unit = randomUnit(
            request.mSeed, id, 0x4a495454455252ULL + index * 2ULL);
        const F64 angle_unit = randomUnit(
            request.mSeed, id, 0x4a495454455241ULL + index * 2ULL);
        const F64 radius = request.mJitterMeters * std::sqrt(radial_unit);
        const F64 angle = TWO_PI * angle_unit;
        layout.mPoints[i].mX += radius * std::cos(angle);
        layout.mPoints[i].mY += radius * std::sin(angle);
    }
}

void applyYaw(const Request& request, Layout& layout)
{
    const F64 yaw = normalisedYaw(request.mYawRadians);
    if (yaw == 0.0)
    {
        return;
    }

    const F64 cosine = std::cos(yaw);
    const F64 sine = std::sin(yaw);
    for (Vec2& point : layout.mPoints)
    {
        const F64 x = point.mX;
        const F64 y = point.mY;
        point.mX = cosine * x - sine * y;
        point.mY = sine * x + cosine * y;
    }
}

bool layoutIsFinite(const Layout& layout)
{
    for (const Vec2& point : layout.mPoints)
    {
        if (!std::isfinite(point.mX) || !std::isfinite(point.mY) ||
            std::fabs(point.mX) > MAX_LAYOUT_RADIUS_METERS ||
            std::fabs(point.mY) > MAX_LAYOUT_RADIUS_METERS)
        {
            return false;
        }
    }
    return true;
}

bool pairwiseContractHolds(const Request& request, size_t count,
                           const Layout& layout)
{
    for (size_t i = 0; i < count; ++i)
    {
        for (size_t j = i + 1; j < count; ++j)
        {
            if (lessThanWithTolerance(
                    distance(layout.mPoints[i], layout.mPoints[j]),
                    pairRequirement(request, i, j)))
            {
                return false;
            }
        }
    }
    return true;
}

bool buildSolvedLayout(const Request& request, size_t count, Layout& layout)
{
    if (!buildCanonicalLayout(request, count, layout))
    {
        return false;
    }
    applyJitter(request, count, layout);
    applyYaw(request, layout);
    return layoutIsFinite(layout) &&
           pairwiseContractHolds(request, count, layout);
}

F64 requiredRadius(const Request& request, size_t count, const Layout& layout)
{
    F64 result = 0.0;
    for (size_t i = 0; i < count; ++i)
    {
        result = std::max(result,
            length(layout.mPoints[i]) +
            request.mMembers[i].mFootprintRadiusMeters);
    }
    return result;
}

void populateSlotsAndMetrics(const Request& request, size_t count,
                             const Layout& layout, Result& result)
{
    result.mSlots.clear();
    result.mSlots.reserve(count);
    for (size_t i = 0; i < count; ++i)
    {
        Slot slot;
        slot.mMemberId = request.mMembers[i].mId;
        slot.mInputIndex = static_cast<U32>(i);
        slot.mLocalX = layout.mPoints[i].mX;
        slot.mLocalY = layout.mPoints[i].mY;
        slot.mFootprintRadiusMeters =
            request.mMembers[i].mFootprintRadiusMeters;
        result.mSlots.push_back(slot);
    }

    result.mPlacedCount = static_cast<U32>(count);
    result.mPlacedRadiusMeters = requiredRadius(request, count, layout);
    result.mActualArcSweepRadians =
        request.mShape == Shape::Arc ? layout.mActualArcSweep : 0.0;

    result.mHasPairwiseGap = count >= 2;
    result.mActualMinEdgeGapMeters = 0.0;
    if (result.mHasPairwiseGap)
    {
        F64 minimum_gap = std::numeric_limits<F64>::infinity();
        for (size_t i = 0; i < count; ++i)
        {
            for (size_t j = i + 1; j < count; ++j)
            {
                const F64 gap =
                    distance(layout.mPoints[i], layout.mPoints[j]) -
                    request.mMembers[i].mFootprintRadiusMeters -
                    request.mMembers[j].mFootprintRadiusMeters;
                minimum_gap = std::min(minimum_gap, gap);
            }
        }
        result.mActualMinEdgeGapMeters = minimum_gap;
    }
}

void populateOverflow(const Request& request, size_t placed, Result& result)
{
    result.mOverflowMemberIds.clear();
    result.mOverflowMemberIds.reserve(request.mMembers.size() - placed);
    for (size_t i = placed; i < request.mMembers.size(); ++i)
    {
        result.mOverflowMemberIds.push_back(request.mMembers[i].mId);
    }
    result.mOverflowCount =
        static_cast<U32>(result.mOverflowMemberIds.size());
}

void finishFingerprint(Result& result)
{
    result.mSolutionFingerprint = solutionFingerprint(result);
}

} // anonymous namespace

Result solve(const Request& request)
{
    Result result;
    result.mRequestedCount = static_cast<U32>(std::min<size_t>(
        request.mMembers.size(),
        static_cast<size_t>(std::numeric_limits<U32>::max())));

    std::string diagnostic;
    if (!validateRequest(request, diagnostic))
    {
        result.mStatus = Status::InvalidInput;
        result.mDiagnostic = diagnostic;
        finishFingerprint(result);
        return result;
    }

    result.mInputFingerprint = inputFingerprint(request);
    result.mEnvelopeRadiusMeters =
        request.mPolicy == EnvelopePolicy::FixedRadius
            ? request.mRadiusMeters : 0.0;

    const ShapeClass shape_class = classifyShape(request.mShape);
    if (shape_class != ShapeClass::Supported)
    {
        result.mStatus = Status::UnsupportedShape;
        result.mDiagnostic = shape_class == ShapeClass::Unsupported
            ? "legacy formation has no contract-safe solver yet"
            : "unknown formation shape";
        populateOverflow(request, 0, result);
        finishFingerprint(result);
        return result;
    }

    const size_t requested_count = request.mMembers.size();
    Layout full_layout;
    const bool full_layout_built =
        buildSolvedLayout(request, requested_count, full_layout);
    if (!full_layout_built &&
        request.mPolicy == EnvelopePolicy::FitCount)
    {
        result.mStatus = Status::NoFeasibleLayout;
        result.mDiagnostic =
            "layout exceeded numeric limits or failed its pairwise contract";
        populateOverflow(request, 0, result);
        finishFingerprint(result);
        return result;
    }

    if (full_layout_built)
    {
        result.mRequiredRadiusMeters =
            requiredRadius(request, requested_count, full_layout);
    }
    const bool full_radius_representable =
        full_layout_built &&
        std::isfinite(result.mRequiredRadiusMeters) &&
        result.mRequiredRadiusMeters <= MAX_LAYOUT_RADIUS_METERS;
    if (!full_radius_representable)
    {
        if (request.mPolicy == EnvelopePolicy::FitCount)
        {
            result.mStatus = Status::NoFeasibleLayout;
            result.mDiagnostic =
                "required layout radius exceeds the solver limit";
            populateOverflow(request, 0, result);
            finishFingerprint(result);
            return result;
        }
        // FixedRadius has a stable-prefix contract. A numerically enormous
        // full layout must not hide a smaller representable prefix that fits.
        result.mRequiredRadiusMeters = MAX_LAYOUT_RADIUS_METERS;
    }

    if (request.mPolicy == EnvelopePolicy::FitCount)
    {
        result.mStatus = Status::Ok;
        result.mDiagnostic = "all members placed; radius fitted to count";
        result.mEnvelopeRadiusMeters = result.mRequiredRadiusMeters;
        populateSlotsAndMetrics(request, requested_count, full_layout, result);
        populateOverflow(request, requested_count, result);
        finishFingerprint(result);
        return result;
    }

    if (full_radius_representable &&
        fitsWithTolerance(result.mRequiredRadiusMeters,
                          request.mRadiusMeters))
    {
        result.mStatus = Status::Ok;
        result.mDiagnostic = "all members fit the fixed radius";
        populateSlotsAndMetrics(request, requested_count, full_layout, result);
        populateOverflow(request, requested_count, result);
        finishFingerprint(result);
        return result;
    }

    size_t placed_count = 0;
    Layout placed_layout;
    for (size_t candidate_count = requested_count - (requested_count ? 1 : 0);
         candidate_count > 0; --candidate_count)
    {
        Layout candidate;
        if (!buildSolvedLayout(request, candidate_count, candidate))
        {
            continue;
        }
        const F64 candidate_radius =
            requiredRadius(request, candidate_count, candidate);
        if (fitsWithTolerance(candidate_radius, request.mRadiusMeters))
        {
            placed_count = candidate_count;
            placed_layout = candidate;
            break;
        }
    }
    if (placed_count == 0)
    {
        buildSolvedLayout(request, 0, placed_layout);
    }

    result.mStatus = Status::PartialOverflow;
    result.mDiagnostic = full_radius_representable
        ? "fixed radius placed a stable prefix; remaining members overflow"
        : "full layout exceeded numeric limits; a stable prefix fits the fixed radius";
    populateSlotsAndMetrics(request, placed_count, placed_layout, result);
    populateOverflow(request, placed_count, result);
    finishFingerprint(result);
    return result;
}

const char* statusName(Status status)
{
    switch (status)
    {
    case Status::Ok:               return "ok";
    case Status::PartialOverflow:  return "partial-overflow";
    case Status::InvalidInput:     return "invalid-input";
    case Status::UnsupportedShape: return "unsupported-shape";
    case Status::NoFeasibleLayout: return "no-feasible-layout";
    }
    return "unknown";
}

} // namespace ALFormationSolver
