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
} // namespace tut
