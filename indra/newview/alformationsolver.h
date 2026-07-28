/**
 * @file alformationsolver.h
 * @brief Pure, deterministic crowd-formation geometry solver.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALFORMATIONSOLVER_H
#define AL_ALFORMATIONSOLVER_H

#include "stdtypes.h"

#include <string>
#include <vector>

namespace ALFormationSolver
{

// The solver is intended for interactive crowd layouts. Keeping the cap here,
// rather than relying on an XUI spinner, prevents unchecked allocations and
// cubic work when requests arrive through another caller.
constexpr U32 MAX_MEMBER_COUNT = 256;
constexpr F64 MAX_INPUT_METERS = 1.0e9;
constexpr F64 MAX_LAYOUT_RADIUS_METERS = 1.0e12;

enum class Shape : U32
{
    Row = 0,
    Grid,
    StaggeredRows,
    Ring,
    Arc,
    ConcentricRings,
    Organic,

    // Kept as explicit adapter destinations while the old special-purpose
    // formulas are redesigned. Returning UnsupportedShape is safer than
    // pretending they obey the radius and clearance contract.
    LegacyV,
    LegacySpiral,
    LegacyStaircase,
    LegacyTunnel,
    LegacyAmphitheater,
    LegacyWedge,
    LegacyCheckerboard,
    LegacyCrescent,
    LegacyPerimeterLine,
    LegacyPath,
    LegacyClusters,

    // Contract-safe Studio formations. These are intentionally appended so
    // persisted values for the original and legacy shapes never change.
    SplitRow,
    SoulTrain,
    Chevron,
    Zigzag,
    Horseshoe,
    Spiral,
    Infinity,
    Pentagram,
    StarOutline,
    Diamond,
    Cross,
    Arrow,
    Heart,
    Sunburst,
    Brigade,

    // Two-block military corridor and a configurable staggered/echelon rank
    // grid. Appended so persisted crowd formation values never shift.
    SoulTrainMilitary,
    StaggeredMilitary,

    // A military brigade wrapped into concentric rings: the grid's files become
    // the members on each ring and its ranks become the number of rings, with a
    // tunable ring spacing and an angular stagger between rings. Appended so
    // persisted crowd formation values never shift.
    RingBrigade
};

enum class EnvelopePolicy : U32
{
    // mRadiusMeters is a hard outer radius. A deterministic prefix is placed
    // and the remaining stable member IDs are reported as overflow.
    FixedRadius = 0,

    // Every member is placed and the minimum radius used by this generator is
    // returned. mRadiusMeters is not used as a constraint.
    FitCount
};

enum class Status : U32
{
    Ok = 0,
    PartialOverflow,
    InvalidInput,
    UnsupportedShape,
    NoFeasibleLayout
};

struct Member
{
    U64 mId = 0;
    F64 mFootprintRadiusMeters = 0.30;
};

struct Request
{
    Shape          mShape = Shape::Row;
    EnvelopePolicy mPolicy = EnvelopePolicy::FitCount;
    std::vector<Member> mMembers;

    // A hard outer radius only when mPolicy == FixedRadius. The boundary
    // contract is hypot(slot.x, slot.y) + member footprint <= radius.
    F64 mRadiusMeters = 0.0;

    // Minimum centre-to-centre spacing. The actual pair requirement is:
    // max(center spacing, footprint_i + footprint_j + edge gap).
    F64 mCenterSpacingMeters = 1.0;
    F64 mEdgeGapMeters = 0.0;

    // Maximum displacement of each member from its regular slot. Geometry
    // reserves twice this amount between base slots so the final jittered
    // result still satisfies the pair contract.
    F64 mJitterMeters = 0.0;

    // Rotation about the explicit local origin/pivot. All solver positions are
    // local F64 coordinates; conversion to viewer/world coordinates is an
    // adapter responsibility.
    F64 mYawRadians = 0.0;

    // Zero chooses the most compact deterministic row/column count. A non-zero
    // value fixes the maximum members per row for Grid/StaggeredRows.
    U32 mGridColumns = 0;
    U32 mGridRows = 0;
    F64 mColumnSpacingMeters = 0.0;
    F64 mRowSpacingMeters = 0.0;

    // Arc is interpreted as a maximum sweep in (0, 2*pi]. For a near-complete
    // arc the solver may use a smaller actual sweep so the two ends cannot
    // overlap. Exactly 2*pi is solved as a ring.
    F64 mArcSweepRadians = 2.0943951023931954923; // 120 degrees

    // Clear aisle requested between the two rails of SplitRow/SoulTrain.
    // Centre spacing and footprint clearance can require a wider aisle.
    F64 mLaneGapMeters = 1.5;

    // Included angle at the Chevron apex, in degrees.
    F64 mChevronAngleDegrees = 60.0;

    // Missing wedge of the Horseshoe, centred on canonical +X, in degrees.
    F64 mHorseshoeOpeningDegrees = 90.0;

    // Number of complete turns in the open Archimedean Spiral.
    F64 mSpiralTurns = 2.0;

    // Canonical Y/X scale used by the applicable guide shapes.
    F64 mAspectRatio = 1.0;

    // Radial arms in Sunburst and alternating stations in Zigzag.
    U32 mRayCount = 8;
    U32 mZigzagColumns = 6;

    // Alternate-rank lateral offset for StaggeredMilitary, expressed as a
    // fraction of the file pitch. A non-negative value is a brick stagger (odd
    // ranks shifted by value * file pitch); a negative value is a cumulative
    // echelon that steps each successive rank diagonally by |value| * file
    // pitch. Zero collapses to a plain aligned grid.
    F64 mRankOffsetFraction = 0.5;

    U64 mSeed = 0;
};

struct Slot
{
    U64 mMemberId = 0;
    U32 mInputIndex = 0;
    F64 mLocalX = 0.0;
    F64 mLocalY = 0.0;
    F64 mFootprintRadiusMeters = 0.0;
};

struct Result
{
    Status mStatus = Status::InvalidInput;
    std::string mDiagnostic;

    U32 mRequestedCount = 0;
    U32 mPlacedCount = 0;
    U32 mOverflowCount = 0;

    std::vector<Slot> mSlots;
    std::vector<U64>  mOverflowMemberIds;

    // For FixedRadius this is the requested hard radius; for FitCount it is
    // the fitted radius. mRequiredRadiusMeters describes the full requested
    // member set and mPlacedRadiusMeters describes mSlots. If the full
    // FixedRadius layout exceeds the solver's numeric ceiling,
    // mRequiredRadiusMeters is saturated at that ceiling while a representable
    // fitting prefix is still returned.
    F64 mEnvelopeRadiusMeters = 0.0;
    F64 mRequiredRadiusMeters = 0.0;
    F64 mPlacedRadiusMeters = 0.0;

    // Pairwise edge gap of the returned slots. It is only meaningful when
    // mHasPairwiseGap is true.
    bool mHasPairwiseGap = false;
    F64  mActualMinEdgeGapMeters = 0.0;

    // Arc diagnostic. It is zero for non-arc shapes and one-member arcs.
    F64 mActualArcSweepRadians = 0.0;

    // Stable within the same viewer build/runtime. The solution fingerprint
    // includes positions and overflow; it is suitable for preview/commit
    // identity checks, not cross-platform file interchange.
    U64 mInputFingerprint = 0;
    U64 mSolutionFingerprint = 0;
};

Result solve(const Request& request);
const char* statusName(Status status);

} // namespace ALFormationSolver

#endif // AL_ALFORMATIONSOLVER_H
