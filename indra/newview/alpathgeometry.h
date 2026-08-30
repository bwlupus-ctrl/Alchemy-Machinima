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
    HELIX = 3,
    DIAMOND = 4,
    FIGURE_EIGHT = 5,
    SINE_WAVE = 6
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

struct CornerFillet
{
    LLVector3d mEntry;
    LLVector3d mExit;
    LLVector3d mCenter;
    LLVector3d mRadial;
    LLVector3d mRadialQuarterTurn;
    LLVector3d mNormal;
    F64        mRadius = 0.0;
    F64        mSweepRadians = 0.0;
};

inline bool isPrimitive(S32 type)
{
    return type >= CIRCLE_ARC && type <= SINE_WAVE;
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

// Fit an exact endpoint-driven primitive without mutating the destination on
// failure. This is the single production math path used by LLActorMover and the
// geometry tests.
inline bool fitPrimitiveToEndpoints(Primitive& destination, S32 type,
                                    const LLVector3d& first,
                                    const LLVector3d& last)
{
    if ((type != CIRCLE_ARC && type != SINE_WAVE) ||
        !first.isFinite() || !last.isFinite())
    {
        return false;
    }

    const LLVector3d chord = last - first;
    const F64 horizontal = std::sqrt(chord.mdV[VX] * chord.mdV[VX] +
                                     chord.mdV[VY] * chord.mdV[VY]);
    const F64 length = chord.length();
    if (length < 0.02 || (type == SINE_WAVE && horizontal < 0.02))
    {
        // A sine needs a meaningful horizontal baseline; silently inventing
        // one makes its generated endpoints miss the authored marks.
        return false;
    }

    Primitive fitted = destination;
    fitted.mType = type;
    fitted.mCenter = (first + last) * 0.5;
    if (type == CIRCLE_ARC)
    {
        LLVector3 chord_axis(static_cast<F32>(chord.mdV[VX]),
                            static_cast<F32>(chord.mdV[VY]),
                            static_cast<F32>(chord.mdV[VZ]));
        chord_axis.normalize();
        LLQuaternion endpoint_rotation;
        endpoint_rotation.shortestArc(LLVector3::x_axis, chord_axis);
        F32 roll = 0.f;
        F32 pitch = 0.f;
        F32 yaw = 0.f;
        endpoint_rotation.getEulerAngles(&roll, &pitch, &yaw);

        fitted.mRadiusX = fitted.mRadiusY =
            static_cast<F32>(length * 0.5);
        fitted.mStartDeg = 180.f;
        fitted.mSweepDeg = -180.f;
        fitted.mRollDeg = roll * RAD_TO_DEG;
        fitted.mPitchDeg = pitch * RAD_TO_DEG;
        fitted.mYawDeg = yaw * RAD_TO_DEG;
        fitted.mRise = 0.f;
    }
    else
    {
        fitted.mRadiusX = static_cast<F32>(horizontal * 0.5);
        fitted.mRadiusY = static_cast<F32>(llmax(0.5, horizontal * 0.25));
        fitted.mStartDeg = 0.f;
        fitted.mSweepDeg = 360.f;
        fitted.mRollDeg = 0.f;
        fitted.mPitchDeg = 0.f;
        fitted.mYawDeg = static_cast<F32>(
            std::atan2(chord.mdV[VY], chord.mdV[VX]) * RAD_TO_DEG);
        fitted.mRise = static_cast<F32>(chord.mdV[VZ]);
    }
    destination = fitted;
    return true;
}

// Construct a circular fillet at corner from the incoming prev->corner and
// outgoing corner->next legs. Oversized requested radii are reduced so adjacent
// corners retain a straight gap; degenerate/straight/U-turn geometry is rejected.
inline bool buildCornerFillet(const LLVector3d& prev,
                              const LLVector3d& corner,
                              const LLVector3d& next,
                              F64 requested_radius,
                              CornerFillet& out)
{
    if (!prev.isFinite() || !corner.isFinite() || !next.isFinite() ||
        !std::isfinite(requested_radius) || requested_radius <= 0.001)
    {
        return false;
    }

    LLVector3d incoming = corner - prev;
    LLVector3d outgoing = next - corner;
    const F64 incoming_length = incoming.length();
    const F64 outgoing_length = outgoing.length();
    if (incoming_length < 0.02 || outgoing_length < 0.02)
    {
        return false;
    }
    incoming *= 1.0 / incoming_length;
    outgoing *= 1.0 / outgoing_length;

    const F64 direction_dot = llclamp(incoming * outgoing, -1.0, 1.0);
    const F64 turn = std::acos(direction_dot);
    constexpr F64 MIN_TURN = 0.5 * DEG_TO_RAD;
    if (turn < MIN_TURN || F_PI - turn < MIN_TURN)
    {
        return false;
    }

    LLVector3d normal = incoming % outgoing;
    const F64 normal_length = normal.length();
    if (normal_length < 1e-8)
    {
        return false;
    }
    normal *= 1.0 / normal_length;

    const F64 tangent_scale = std::tan(turn * 0.5);
    if (!std::isfinite(tangent_scale) || tangent_scale < 1e-8)
    {
        return false;
    }
    const F64 max_tangent = 0.49 * llmin(incoming_length, outgoing_length);
    const F64 tangent_distance =
        llmin(requested_radius * tangent_scale, max_tangent);
    const F64 effective_radius = tangent_distance / tangent_scale;
    if (effective_radius <= 0.001)
    {
        return false;
    }

    CornerFillet fitted;
    fitted.mEntry = corner - incoming * tangent_distance;
    fitted.mExit = corner + outgoing * tangent_distance;
    fitted.mNormal = normal;
    fitted.mRadius = effective_radius;
    fitted.mSweepRadians = turn;
    fitted.mCenter = fitted.mEntry +
        (normal % incoming) * effective_radius;
    fitted.mRadial = fitted.mEntry - fitted.mCenter;
    fitted.mRadialQuarterTurn = normal % fitted.mRadial;
    out = fitted;
    return true;
}

inline LLVector3d evaluateCornerFillet(const CornerFillet& fillet, F64 u)
{
    u = llclamp(u, 0.0, 1.0);
    const F64 angle = fillet.mSweepRadians * u;
    return fillet.mCenter +
           fillet.mRadial * std::cos(angle) +
           fillet.mRadialQuarterTurn * std::sin(angle);
}

inline LLVector3d tangentCornerFillet(const CornerFillet& fillet, F64 u)
{
    u = llclamp(u, 0.0, 1.0);
    const F64 angle = fillet.mSweepRadians * u;
    LLVector3d tangent =
        fillet.mRadial * -std::sin(angle) +
        fillet.mRadialQuarterTurn * std::cos(angle);
    const F64 length = tangent.length();
    return length > 1e-12 ? tangent * (1.0 / length)
                          : LLVector3d(1.0, 0.0, 0.0);
}

inline F64 radiusX(const Primitive& p)
{
    return llmax(0.001, std::fabs(static_cast<F64>(p.mRadiusX)));
}

inline F64 radiusY(const Primitive& p)
{
    return (p.mType == ELLIPSE_ARC || p.mType == DIAMOND ||
            p.mType == FIGURE_EIGHT || p.mType == SINE_WAVE)
        ? llmax(0.001, std::fabs(static_cast<F64>(p.mRadiusY)))
        : radiusX(p);
}

inline F64 wrapUnit(F64 value)
{
    value -= std::floor(value);
    return value < 0.0 ? value + 1.0 : value;
}

inline LLVector3d diamondPoint(const Primitive& p, F64 angle_deg)
{
    const F64 edge = wrapUnit(angle_deg / 360.0) * 4.0;
    const S32 side = llclamp(static_cast<S32>(std::floor(edge)), 0, 3);
    const F64 t = edge - static_cast<F64>(side);
    const F64 rx = radiusX(p);
    const F64 ry = radiusY(p);
    const LLVector3d vertices[4] = {
        LLVector3d(rx, 0.0, 0.0), LLVector3d(0.0, ry, 0.0),
        LLVector3d(-rx, 0.0, 0.0), LLVector3d(0.0, -ry, 0.0)
    };
    return vertices[side] * (1.0 - t) + vertices[(side + 1) % 4] * t;
}

inline LLVector3d evaluate(const Primitive& p, F64 u)
{
    u = llclamp(u, 0.0, 1.0);
    const F64 angle = (static_cast<F64>(p.mStartDeg) +
                       static_cast<F64>(p.mSweepDeg) * u) * DEG_TO_RAD;
    LLVector3d local;
    if (p.mType == DIAMOND)
    {
        local = diamondPoint(p, angle * RAD_TO_DEG);
    }
    else if (p.mType == FIGURE_EIGHT)
    {
        local.set(radiusX(p) * std::cos(angle),
                  radiusY(p) * std::sin(2.0 * angle), 0.0);
    }
    else if (p.mType == SINE_WAVE)
    {
        // Open, endpoint-friendly wave: local X spans -RadiusX..+RadiusX,
        // Start/Sweep control phase/cycles, and Rise is the end-to-end Z delta.
        local.set(radiusX(p) * (2.0 * u - 1.0),
                  radiusY(p) * std::sin(angle),
                  static_cast<F64>(p.mRise) * (u - 0.5));
    }
    else
    {
        const F64 z = p.mType == HELIX ? static_cast<F64>(p.mRise) * u : 0.0;
        local.set(radiusX(p) * std::cos(angle),
                  radiusY(p) * std::sin(angle), z);
    }
    return p.mCenter + local * planeRotation(p);
}

inline LLVector3d derivative(const Primitive& p, F64 u)
{
    u = llclamp(u, 0.0, 1.0);
    const F64 angle = (static_cast<F64>(p.mStartDeg) +
                       static_cast<F64>(p.mSweepDeg) * u) * DEG_TO_RAD;
    const F64 sweep = static_cast<F64>(p.mSweepDeg) * DEG_TO_RAD;
    LLVector3d tangent;
    if (p.mType == DIAMOND)
    {
        constexpr F64 EPS = 1e-6;
        const F64 ua = llclamp(u - EPS, 0.0, 1.0);
        const F64 ub = llclamp(u + EPS, 0.0, 1.0);
        tangent = diamondPoint(p, p.mStartDeg + p.mSweepDeg * ub) -
                  diamondPoint(p, p.mStartDeg + p.mSweepDeg * ua);
    }
    else if (p.mType == FIGURE_EIGHT)
    {
        tangent.set(-radiusX(p) * std::sin(angle) * sweep,
                     2.0 * radiusY(p) * std::cos(2.0 * angle) * sweep, 0.0);
    }
    else if (p.mType == SINE_WAVE)
    {
        tangent.set(2.0 * radiusX(p),
                    radiusY(p) * std::cos(angle) * sweep,
                    static_cast<F64>(p.mRise));
    }
    else
    {
        const F64 dz = p.mType == HELIX ? static_cast<F64>(p.mRise) : 0.0;
        tangent.set(-radiusX(p) * std::sin(angle) * sweep,
                     radiusY(p) * std::cos(angle) * sweep, dz);
    }
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
    if (p.mType == SINE_WAVE)
    {
        // Sine's X parameter is open rather than angular. Rotate its LOCAL plane
        // half a turn and invert phase/rise so the result remains P(1-u), even
        // after the user has applied arbitrary yaw, tilt, and roll.
        const F32 old_end = p.mStartDeg + p.mSweepDeg;
        p.mStartDeg = std::fmod(-old_end, 360.f);
        LLQuaternion half_turn;
        half_turn.setEulerAngles(0.f, 0.f, F_PI);
        LLQuaternion reversed_rotation = half_turn * planeRotation(p);
        F32 roll = 0.f, pitch = 0.f, yaw = 0.f;
        reversed_rotation.getEulerAngles(&roll, &pitch, &yaw);
        p.mRollDeg = roll * RAD_TO_DEG;
        p.mPitchDeg = pitch * RAD_TO_DEG;
        p.mYawDeg = yaw * RAD_TO_DEG;
        p.mRise = -p.mRise;
        return;
    }
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
