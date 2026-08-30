/**
 * @file alpathgeometry_test.cpp
 * @brief Unit tests for exact Character Mover primitive paths.
 */

#include "linden_common.h"

#include "../test/lltut.h"
#include "../alpathgeometry.h"

#include <cmath>

namespace tut
{
struct alpathgeometry_data {};
typedef test_group<alpathgeometry_data> alpathgeometry_test_group;
typedef alpathgeometry_test_group::object alpathgeometry_test_object;
alpathgeometry_test_group alpathgeometry_test("alpathgeometry");

template<> template<>
void alpathgeometry_test_object::test<1>()
{
    set_test_name("circle arc endpoints and analytic length");
    ALPathGeometry::Primitive p;
    p.mType = ALPathGeometry::CIRCLE_ARC;
    p.mCenter = LLVector3d(10.0, 20.0, 30.0);
    p.mRadiusX = 2.f;
    p.mStartDeg = 0.f;
    p.mSweepDeg = 90.f;

    const LLVector3d a = ALPathGeometry::evaluate(p, 0.0);
    const LLVector3d b = ALPathGeometry::evaluate(p, 1.0);
    ensure_distance("start x", a.mdV[VX], 12.0, 1e-5);
    ensure_distance("start y", a.mdV[VY], 20.0, 1e-5);
    ensure_distance("end x", b.mdV[VX], 10.0, 1e-5);
    ensure_distance("end y", b.mdV[VY], 22.0, 1e-5);
    F64 length = 0.0;
    ensure("circle length is analytic", ALPathGeometry::analyticLength(p, length));
    ensure_distance("quarter-circle length", length, static_cast<F64>(F_PI), 1e-5);
}

template<> template<>
void alpathgeometry_test_object::test<2>()
{
    set_test_name("ellipse radii and rotated plane");
    ALPathGeometry::Primitive p;
    p.mType = ALPathGeometry::ELLIPSE_ARC;
    p.mRadiusX = 4.f;
    p.mRadiusY = 2.f;
    p.mSweepDeg = 90.f;
    p.mYawDeg = 90.f;

    const LLVector3d a = ALPathGeometry::evaluate(p, 0.0);
    const LLVector3d b = ALPathGeometry::evaluate(p, 1.0);
    ensure_distance("rotated major x", a.mdV[VX], 0.0, 1e-5);
    ensure_distance("rotated major y", a.mdV[VY], 4.0, 1e-5);
    ensure_distance("rotated minor x", b.mdV[VX], -2.0, 1e-5);
    ensure_distance("rotated minor y", b.mdV[VY], 0.0, 1e-5);
    F64 ignored = 0.0;
    ensure("ellipse length uses bounded table", !ALPathGeometry::analyticLength(p, ignored));
}

template<> template<>
void alpathgeometry_test_object::test<3>()
{
    set_test_name("negative sweep reverses tangent");
    ALPathGeometry::Primitive p;
    p.mType = ALPathGeometry::CIRCLE_ARC;
    p.mRadiusX = 3.f;
    p.mSweepDeg = -180.f;
    const LLVector3d t = ALPathGeometry::derivative(p, 0.0);
    ensure("clockwise tangent points -Y", t.mdV[VY] < -0.999);
}

template<> template<>
void alpathgeometry_test_object::test<4>()
{
    set_test_name("helix endpoint and exact length");
    ALPathGeometry::Primitive p;
    p.mType = ALPathGeometry::HELIX;
    p.mRadiusX = 2.f;
    p.mSweepDeg = 360.f;
    p.mRise = 5.f;
    const LLVector3d end = ALPathGeometry::evaluate(p, 1.0);
    ensure_distance("helix closes in x", end.mdV[VX], 2.0, 1e-5);
    ensure_distance("helix closes in y", end.mdV[VY], 0.0, 1e-5);
    ensure_distance("helix rises", end.mdV[VZ], 5.0, 1e-5);
    F64 length = 0.0;
    ensure("helix length is analytic", ALPathGeometry::analyticLength(p, length));
    const F64 circumference = 4.0 * F_PI;
    ensure_distance("helix length", length,
                    std::sqrt(circumference * circumference + 25.0), 1e-5);
}

template<> template<>
void alpathgeometry_test_object::test<5>()
{
    set_test_name("full circle seam is position continuous");
    ALPathGeometry::Primitive p;
    p.mType = ALPathGeometry::CIRCLE_ARC;
    p.mRadiusX = 7.f;
    p.mStartDeg = 37.f;
    p.mSweepDeg = 360.f;
    p.mYawDeg = 18.f;
    p.mPitchDeg = 23.f;
    p.mRollDeg = -11.f;
    const LLVector3d a = ALPathGeometry::evaluate(p, 0.0);
    const LLVector3d b = ALPathGeometry::evaluate(p, 1.0);
    ensure_distance("seam position", (a - b).length(), 0.0, 1e-5);
    const LLVector3d ta = ALPathGeometry::derivative(p, 0.0);
    const LLVector3d tb = ALPathGeometry::derivative(p, 1.0);
    ensure_distance("seam tangent", (ta - tb).length(), 0.0, 1e-5);
}

template<> template<>
void alpathgeometry_test_object::test<6>()
{
    set_test_name("rotated rising helix reverse is P(1-u) and reload-safe");
    ALPathGeometry::Primitive original;
    original.mType = ALPathGeometry::HELIX;
    original.mCenter = LLVector3d(10.0, 20.0, 30.0);
    original.mRadiusX = 2.5f;
    original.mStartDeg = 300.f;
    original.mSweepDeg = 810.f;
    original.mYawDeg = 37.f;
    original.mPitchDeg = -24.f;
    original.mRollDeg = 13.f;
    original.mRise = 5.f;

    ALPathGeometry::Primitive reversed = original;
    ALPathGeometry::reverse(reversed);
    ensure("reversed start is canonical", std::fabs(reversed.mStartDeg) < 360.f);
    for (S32 i = 0; i <= 8; ++i)
    {
        const F64 u = static_cast<F64>(i) / 8.0;
        ensure_distance("reverse matches P(1-u)",
            (ALPathGeometry::evaluate(reversed, u) -
             ALPathGeometry::evaluate(original, 1.0 - u)).length(),
            0.0, 1e-4);
    }

    ALPathGeometry::reverse(reversed);
    ensure("double-reversed start remains canonical",
           std::fabs(reversed.mStartDeg) < 360.f);
    for (S32 i = 0; i <= 8; ++i)
    {
        const F64 u = static_cast<F64>(i) / 8.0;
        ensure_distance("double reverse preserves geometry",
            (ALPathGeometry::evaluate(reversed, u) -
             ALPathGeometry::evaluate(original, u)).length(),
            0.0, 1e-4);
    }
}

template<> template<>
void alpathgeometry_test_object::test<7>()
{
    set_test_name("diamond corners and closed seam");
    ALPathGeometry::Primitive p;
    p.mType = ALPathGeometry::DIAMOND;
    p.mRadiusX = 4.f;
    p.mRadiusY = 2.f;
    ensure_distance("right corner", ALPathGeometry::evaluate(p, 0.0).mdV[VX], 4.0, 1e-5);
    ensure_distance("top corner", ALPathGeometry::evaluate(p, 0.25).mdV[VY], 2.0, 1e-5);
    ensure_distance("left corner", ALPathGeometry::evaluate(p, 0.5).mdV[VX], -4.0, 1e-5);
    ensure_distance("diamond seam",
        (ALPathGeometry::evaluate(p, 0.0) - ALPathGeometry::evaluate(p, 1.0)).length(),
        0.0, 1e-5);
}

template<> template<>
void alpathgeometry_test_object::test<8>()
{
    set_test_name("figure eight crosses center and closes");
    ALPathGeometry::Primitive p;
    p.mType = ALPathGeometry::FIGURE_EIGHT;
    p.mRadiusX = 3.f;
    p.mRadiusY = 2.f;
    const LLVector3d middle = ALPathGeometry::evaluate(p, 0.25);
    ensure_distance("quarter crosses center x", middle.mdV[VX], 0.0, 1e-5);
    ensure_distance("quarter crosses center y", middle.mdV[VY], 0.0, 1e-5);
    ensure_distance("figure eight seam",
        (ALPathGeometry::evaluate(p, 0.0) - ALPathGeometry::evaluate(p, 1.0)).length(),
        0.0, 1e-5);
}

template<> template<>
void alpathgeometry_test_object::test<9>()
{
    set_test_name("sine wave endpoints, rise, and arbitrary-plane reverse");
    ALPathGeometry::Primitive p;
    p.mType = ALPathGeometry::SINE_WAVE;
    p.mCenter = LLVector3d(10.0, 20.0, 30.0);
    p.mRadiusX = 4.f;
    p.mRadiusY = 1.5f;
    p.mStartDeg = 0.f;
    p.mSweepDeg = 720.f;
    p.mYawDeg = 23.f;
    p.mPitchDeg = 17.f;
    p.mRollDeg = -9.f;
    p.mRise = 6.f;
    const LLVector3d a = ALPathGeometry::evaluate(p, 0.0);
    const LLVector3d b = ALPathGeometry::evaluate(p, 1.0);
    ensure_distance("sine endpoint span", (b - a).length(), 10.0, 1e-4);

    ALPathGeometry::Primitive reversed = p;
    ALPathGeometry::reverse(reversed);
    for (S32 i = 0; i <= 8; ++i)
    {
        const F64 u = static_cast<F64>(i) / 8.0;
        ensure_distance("sine reverse matches P(1-u)",
            (ALPathGeometry::evaluate(reversed, u) -
             ALPathGeometry::evaluate(p, 1.0 - u)).length(), 0.0, 1e-4);
    }
}

template<> template<>
void alpathgeometry_test_object::test<10>()
{
    set_test_name("production endpoint fitter spans a tilted 3D half-arc chord");
    const LLVector3d first(100.0, 200.0, 30.0);
    const LLVector3d last(106.0, 208.0, 35.0);
    ALPathGeometry::Primitive p;
    ensure("production half-arc fit succeeds",
        ALPathGeometry::fitPrimitiveToEndpoints(
            p, ALPathGeometry::CIRCLE_ARC, first, last));

    ensure_distance("fitted tilted arc starts at first point",
        (ALPathGeometry::evaluate(p, 0.0) - first).length(), 0.0, 1e-4);
    ensure_distance("fitted tilted arc ends at last point",
        (ALPathGeometry::evaluate(p, 1.0) - last).length(), 0.0, 1e-4);
}

template<> template<>
void alpathgeometry_test_object::test<11>()
{
    set_test_name("production sine fitter hits short endpoints and rejects vertical");
    const LLVector3d first(10.0, 20.0, 3.0);
    const LLVector3d last(10.4, 20.0, 8.0);
    ALPathGeometry::Primitive p;
    ensure("short sine fit succeeds",
        ALPathGeometry::fitPrimitiveToEndpoints(
            p, ALPathGeometry::SINE_WAVE, first, last));
    ensure_distance("short sine starts exactly",
        (ALPathGeometry::evaluate(p, 0.0) - first).length(), 0.0, 1e-5);
    ensure_distance("short sine ends exactly",
        (ALPathGeometry::evaluate(p, 1.0) - last).length(), 0.0, 1e-5);
    ensure_distance("short sine keeps exact half-span",
                    p.mRadiusX, 0.2f, 1e-6f);

    ALPathGeometry::Primitive unchanged = p;
    ensure("vertical sine fit is rejected",
        !ALPathGeometry::fitPrimitiveToEndpoints(
            unchanged, ALPathGeometry::SINE_WAVE,
            first, LLVector3d(first.mdV[VX], first.mdV[VY], 12.0)));
    ensure_distance("rejected fit does not mutate output",
        (ALPathGeometry::evaluate(unchanged, 0.35) -
         ALPathGeometry::evaluate(p, 0.35)).length(), 0.0, 1e-6);
}

template<> template<>
void alpathgeometry_test_object::test<12>()
{
    set_test_name("diamond and figure-eight reverse exactly");
    for (S32 type : { ALPathGeometry::DIAMOND, ALPathGeometry::FIGURE_EIGHT })
    {
        ALPathGeometry::Primitive original;
        original.mType = type;
        original.mCenter = LLVector3d(4.0, -7.0, 2.0);
        original.mRadiusX = 3.5f;
        original.mRadiusY = 1.75f;
        original.mStartDeg = 27.f;
        original.mSweepDeg = 720.f;
        original.mYawDeg = 31.f;
        original.mPitchDeg = -14.f;
        original.mRollDeg = 9.f;
        ALPathGeometry::Primitive reversed = original;
        ALPathGeometry::reverse(reversed);
        for (S32 i = 0; i <= 16; ++i)
        {
            const F64 u = static_cast<F64>(i) / 16.0;
            ensure_distance("closed-shape reverse matches P(1-u)",
                (ALPathGeometry::evaluate(reversed, u) -
                 ALPathGeometry::evaluate(original, 1.0 - u)).length(),
                0.0, 1e-4);
        }
    }
}

template<> template<>
void alpathgeometry_test_object::test<13>()
{
    set_test_name("90-degree corner fillet is tangent, exact, and radius-clamped");
    ALPathGeometry::CornerFillet fillet;
    ensure("90-degree fillet builds",
        ALPathGeometry::buildCornerFillet(
            LLVector3d(0.0, 0.0, 0.0),
            LLVector3d(5.0, 0.0, 0.0),
            LLVector3d(5.0, 5.0, 0.0),
            2.0, fillet));
    ensure_distance("entry x", fillet.mEntry.mdV[VX], 3.0, 1e-6);
    ensure_distance("entry y", fillet.mEntry.mdV[VY], 0.0, 1e-6);
    ensure_distance("exit x", fillet.mExit.mdV[VX], 5.0, 1e-6);
    ensure_distance("exit y", fillet.mExit.mdV[VY], 2.0, 1e-6);
    ensure_distance("requested radius retained", fillet.mRadius, 2.0, 1e-6);
    ensure_distance("entry tangent follows incoming leg",
        (ALPathGeometry::tangentCornerFillet(fillet, 0.0) -
         LLVector3d(1.0, 0.0, 0.0)).length(), 0.0, 1e-6);
    ensure_distance("exit tangent follows outgoing leg",
        (ALPathGeometry::tangentCornerFillet(fillet, 1.0) -
         LLVector3d(0.0, 1.0, 0.0)).length(), 0.0, 1e-6);
    ensure_distance("midpoint stays on exact circle",
        (ALPathGeometry::evaluateCornerFillet(fillet, 0.5) -
         fillet.mCenter).length(), fillet.mRadius, 1e-6);

    ALPathGeometry::CornerFillet clamped;
    ensure("oversized fillet still builds",
        ALPathGeometry::buildCornerFillet(
            LLVector3d(0.0, 0.0, 0.0),
            LLVector3d(1.0, 0.0, 0.0),
            LLVector3d(1.0, 1.0, 0.0),
            50.0, clamped));
    ensure_distance("radius clamps to preserve straight segment",
                    clamped.mRadius, 0.49, 1e-6);
}

template<> template<>
void alpathgeometry_test_object::test<14>()
{
    set_test_name("corner fillet remains exact in an arbitrary 3D plane");
    const LLVector3d prev(0.0, 0.0, 0.0);
    const LLVector3d corner(3.0, 4.0, 5.0);
    const LLVector3d next(1.0, 10.0, 8.0);
    LLVector3d incoming = corner - prev;
    LLVector3d outgoing = next - corner;
    incoming *= 1.0 / incoming.length();
    outgoing *= 1.0 / outgoing.length();

    ALPathGeometry::CornerFillet fillet;
    ensure("tilted 3D fillet builds",
        ALPathGeometry::buildCornerFillet(
            prev, corner, next, 1.2, fillet));
    ensure_distance("3D fillet starts at entry",
        (ALPathGeometry::evaluateCornerFillet(fillet, 0.0) -
         fillet.mEntry).length(), 0.0, 1e-6);
    ensure_distance("3D fillet ends at exit",
        (ALPathGeometry::evaluateCornerFillet(fillet, 1.0) -
         fillet.mExit).length(), 0.0, 1e-6);
    ensure_distance("3D entry tangent follows incoming leg",
        (ALPathGeometry::tangentCornerFillet(fillet, 0.0) -
         incoming).length(), 0.0, 1e-6);
    ensure_distance("3D exit tangent follows outgoing leg",
        (ALPathGeometry::tangentCornerFillet(fillet, 1.0) -
         outgoing).length(), 0.0, 1e-6);
    for (S32 i = 0; i <= 8; ++i)
    {
        const LLVector3d point = ALPathGeometry::evaluateCornerFillet(
            fillet, static_cast<F64>(i) / 8.0);
        ensure_distance("3D samples stay on exact circle",
            (point - fillet.mCenter).length(), fillet.mRadius, 1e-6);
        ensure_distance("3D samples stay in the corner plane",
            (point - fillet.mCenter) * fillet.mNormal, 0.0, 1e-6);
    }
}

template<> template<>
void alpathgeometry_test_object::test<15>()
{
    set_test_name("non-right-angle fillet retains requested effective radius");
    const F64 requested_radius = 2.0;
    const F64 turn_radians = 60.0 * DEG_TO_RAD;
    const F64 tangent_distance =
        requested_radius * std::tan(turn_radians * 0.5);
    const LLVector3d prev(0.0, 0.0, 0.0);
    const LLVector3d corner(10.0, 0.0, 0.0);
    const LLVector3d outgoing(std::cos(turn_radians),
                              std::sin(turn_radians), 0.0);
    const LLVector3d next = corner + outgoing * 10.0;

    ALPathGeometry::CornerFillet fillet;
    ensure("60-degree fillet builds",
        ALPathGeometry::buildCornerFillet(
            prev, corner, next, requested_radius, fillet));
    ensure_distance("60-degree requested radius retained",
                    fillet.mRadius, requested_radius, 1e-6);
    ensure_distance("60-degree tangent distance uses tan(theta/2)",
                    (corner - fillet.mEntry).length(), tangent_distance, 1e-6);
    ensure_distance("60-degree exit uses the same tangent distance",
                    (fillet.mExit - corner).length(), tangent_distance, 1e-6);
    ensure_distance("60-degree sweep retained",
                    fillet.mSweepRadians, turn_radians, 1e-6);
}
} // namespace tut
