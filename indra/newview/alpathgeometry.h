/**
 * @file alpathgeometry.h
 * @brief Pure exact primitive geometry for Character Mover paths.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_ALPATHGEOMETRY_H
#define AL_ALPATHGEOMETRY_H

#include "llmath.h"
#include "llquaternion.h"
#include "v3dmath.h"

#include <cmath>

namespace ALPathGeometry
{
enum Type : S32
{
    WAYPOINTS = 0,
    CIRCLE_ARC = 1,
    ELLIPSE_ARC = 2,
    HELIX = 3
};

struct Primitive
{
    S32        mType = WAYPOINTS;
    LLVector3d mCenter;
    F32        mRadiusX = 3.f;
    F32        mRadiusY = 2.f;
    F32        mStartDeg = 0.f;
    F32        mSweepDeg = 360.f;
    F32        mYawDeg = 0.f;
    F32        mPitchDeg = 0.f;
    F32        mRollDeg = 0.f;
    F32        mRise = 3.f;
};

inline bool isPrimitive(S32 type)
{
    return type >= CIRCLE_ARC && type <= HELIX;
}

inline LLQuaternion planeRotation(const Primitive& p)
{
    LLQuaternion rotation;
    rotation.setEulerAngles(p.mRollDeg * DEG_TO_RAD,
                            p.mPitchDeg * DEG_TO_RAD,
                            p.mYawDeg * DEG_TO_RAD);
    rotation.normalize();
    return rotation;
}

inline F64 radiusX(const Primitive& p)
{
    return llmax(0.001, std::fabs(static_cast<F64>(p.mRadiusX)));
}

inline F64 radiusY(const Primitive& p)
{
    return p.mType == ELLIPSE_ARC
        ? llmax(0.001, std::fabs(static_cast<F64>(p.mRadiusY)))
        : radiusX(p);
}

inline LLVector3d evaluate(const Primitive& p, F64 u)
{
    u = llclamp(u, 0.0, 1.0);
    const F64 angle = (static_cast<F64>(p.mStartDeg) +
                       static_cast<F64>(p.mSweepDeg) * u) * DEG_TO_RAD;
    const F64 z = p.mType == HELIX ? static_cast<F64>(p.mRise) * u : 0.0;
    const LLVector3d local(radiusX(p) * std::cos(angle),
                           radiusY(p) * std::sin(angle), z);
    return p.mCenter + local * planeRotation(p);
}

inline LLVector3d derivative(const Primitive& p, F64 u)
{
    u = llclamp(u, 0.0, 1.0);
    const F64 angle = (static_cast<F64>(p.mStartDeg) +
                       static_cast<F64>(p.mSweepDeg) * u) * DEG_TO_RAD;
    const F64 sweep = static_cast<F64>(p.mSweepDeg) * DEG_TO_RAD;
    const F64 dz = p.mType == HELIX ? static_cast<F64>(p.mRise) : 0.0;
    LLVector3d tangent(-radiusX(p) * std::sin(angle) * sweep,
                        radiusY(p) * std::cos(angle) * sweep, dz);
    tangent = tangent * planeRotation(p);
    const F64 length = tangent.length();
    if (length > 1e-12)
    {
        tangent *= 1.0 / length;
    }
    else
    {
        tangent = LLVector3d(1.0, 0.0, 0.0) * planeRotation(p);
    }
    return tangent;
}

// Reverse a primitive exactly: the transformed result evaluates to P(1-u).
// Canonicalizing the new start keeps repeated reverse/save/load operations
// inside the persisted angle range without changing periodic geometry.
inline void reverse(Primitive& p)
{
    if (p.mType == HELIX)
    {
        const LLVector3d rise_local(0.0, 0.0, static_cast<F64>(p.mRise));
        p.mCenter += rise_local * planeRotation(p);
        p.mRise = -p.mRise;
    }
    p.mStartDeg = std::fmod(p.mStartDeg + p.mSweepDeg, 360.f);
    p.mSweepDeg = -p.mSweepDeg;
}

// Circle/arc and circular helix have analytic length. Ellipses deliberately
// return false because their distance inversion is handled by the same bounded
// adaptive arc table as authored waypoint splines.
inline bool analyticLength(const Primitive& p, F64& out_length)
{
    if (p.mType != CIRCLE_ARC && p.mType != HELIX)
    {
        return false;
    }
    const F64 circumferential = radiusX(p) *
        std::fabs(static_cast<F64>(p.mSweepDeg) * DEG_TO_RAD);
    const F64 rise = p.mType == HELIX
        ? std::fabs(static_cast<F64>(p.mRise)) : 0.0;
    out_length = std::sqrt(circumferential * circumferential + rise * rise);
    return std::isfinite(out_length);
}
} // namespace ALPathGeometry

#endif // AL_ALPATHGEOMETRY_H
