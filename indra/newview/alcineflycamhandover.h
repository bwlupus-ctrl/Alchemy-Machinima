/** Bone Lock-only Orbit math. Manual Orbit retains its existing integrator. */
#ifndef AL_CINE_FLYCAM_HANDOVER_H
#define AL_CINE_FLYCAM_HANDOVER_H

#include "llmath.h"
#include "llquaternion.h"
#include "v3math.h"
#include "v3dmath.h"
#include "m3math.h"

inline bool alFlycamFrameLensApplied(bool handover, bool orbit, bool cameraOperator, bool shake)
{
    return handover || orbit || cameraOperator || shake;
}

// Retain both the lens-space boundary and its reversible pre-lens input.
// Inverting the lens-space value alone loses information at either clamp.
// Global positions make this boundary survive a region-origin rebase.
struct ALPreOperatorPresentation
{
    LLVector3d position, presentedPosition;
    LLQuaternion rotation, presentedRotation;
    F32 fov = 0.f, presentedFov = 0.f;
    F32 plainFov = 0.f, seedFov = 0.f, operatorMultiplier = 1.f;
    U64 serial = 0;
    bool valid = false;
    bool reversibleFov = false;

    bool matches(U64 lastSerial, const LLVector3d& pos, const LLQuaternion& rot, F32 view) const
    {
        return valid && serial == lastSerial &&
            (pos - presentedPosition).lengthSquared() < 1e-6 &&
            fabsf(dot(rot, presentedRotation)) > 1.f - 1e-6f && view == presentedFov;
    }
};

// An out-of-range authored pose may move toward the limits, never snap to them.
inline F32 alHandoverLimitedMove(F32 value, F32 delta, F32 low, F32 high)
{
    return llclamp(value + delta, llmin(value, low), llmax(value, high));
}

inline F32 alSeedHandoverZoom(F32 rendered, F32 lensShift, F32 orbitMultiplier)
{
    const F32 halfHeight = tanf(rendered * 0.5f) - lensShift;
    // Pre-lens space may be negative for a narrow foreign-owner FOV. The
    // forward lens adds its shift before converting back to a rendered FOV.
    const F32 plain = 2.f * atanf(halfHeight);
    return plain / orbitMultiplier;
}

struct ALBoneLockOrbit
{
    LLVector3 offset;
    LLVector3 focus;
    LLQuaternion aimOffset;
    F32 azimuth = 0.f;
    F32 elevation = 0.f;
    F32 radius = 0.f;

    static LLQuaternion basis(F32 azimuth, F32 elevation, const LLQuaternion& anchor)
    {
        // The azimuth defines left even at a pole. Unlike a cross-product
        // threshold, this basis has no fallback edge as elevation changes.
        const F32 ce = cosf(elevation);
        const LLVector3 at = -LLVector3(cosf(azimuth) * ce, sinf(azimuth) * ce,
                                       sinf(elevation)) * anchor;
        const LLVector3 left = LLVector3(sinf(azimuth), -cosf(azimuth), 0.f) * anchor;
        LLMatrix3 matrix;
        matrix.setRows(at, left, at % left);
        return LLQuaternion(matrix);
    }

    void seed(const LLVector3& position, const LLQuaternion& rotation,
              const LLVector3& anchorPosition, const LLQuaternion& anchorRotation)
    {
        offset = (position - anchorPosition) * ~anchorRotation;
        focus.setZero();
        radius = offset.magVec();
        azimuth = atan2f(offset.mV[VY], offset.mV[VX]);
        elevation = atan2f(offset.mV[VZ], sqrtf(offset.mV[VX] * offset.mV[VX] +
                                               offset.mV[VY] * offset.mV[VY]));
        aimOffset = rotation * ~basis(azimuth, elevation, anchorRotation);
    }

    void move(F32 yaw, F32 pitch, F32 forward, F32 left, F32 up, F32 roll,
              F32 minRadius, F32 maxRadius, const LLVector3& anchorPosition,
              const LLQuaternion& anchorRotation, LLVector3& position, LLQuaternion& rotation)
    {
        if (yaw != 0.f || pitch != 0.f || forward != 0.f)
        {
            azimuth += yaw;
            elevation = alHandoverLimitedMove(elevation, pitch, -85.f * DEG_TO_RAD, 85.f * DEG_TO_RAD);
            radius = alHandoverLimitedMove(radius, -forward, minRadius, maxRadius);
            const F32 ce = cosf(elevation);
            offset = LLVector3(cosf(azimuth) * ce, sinf(azimuth) * ce, sinf(elevation)) * radius;
        }
        LLQuaternion orbit = basis(azimuth, elevation, anchorRotation);
        LLQuaternion rollRotation;
        rollRotation.setEulerAngles(roll, 0.f, 0.f);
        aimOffset = rollRotation * aimOffset;
        rotation = aimOffset * orbit;
        focus += (LLVector3(0.f, left, up) * rotation) * ~anchorRotation;
        position = anchorPosition + (offset + focus) * anchorRotation;
    }
};
#endif
