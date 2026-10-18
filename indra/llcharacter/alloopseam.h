/**
 * @file alloopseam.h
 * @brief Pure offset-decay math for keyframe loop seams.
 */
#ifndef AL_ALLOOPSEAM_H
#define AL_ALLOOPSEAM_H

#include "llmath.h"
#include "llquaternion.h"
#include "v3math.h"

namespace ALLoopSeam
{
inline F32 weight(F32 elapsed, F32 window)
{
    const F32 remain = 1.f - llclamp(elapsed / window, 0.f, 1.f);
    return remain * remain * (3.f - 2.f * remain);
}

struct Offset
{
    LLQuaternion mRotation;
    LLVector3 mPosition;
    LLVector3 mScale;

    void capture(const LLQuaternion& previous_rot, const LLVector3& previous_pos,
                 const LLVector3& previous_scale, const LLQuaternion& curve_rot,
                 const LLVector3& curve_pos, const LLVector3& curve_scale)
    {
        // Preserve the exact identity path even for nontrivial closed poses.
        mRotation = LLQuaternion();
        if (previous_rot != curve_rot)
        {
            mRotation = previous_rot * ~curve_rot;
            mRotation.normalize();
        }
        mPosition = previous_pos - curve_pos;
        mScale = previous_scale - curve_scale;
    }

    LLQuaternion rotation(const LLQuaternion& curve, F32 ease) const
    {
        if (ease <= 0.f || mRotation == LLQuaternion())
        {
            return curve;
        }
        return slerp(ease, LLQuaternion(), mRotation) * curve;
    }

    static LLVector3 vector(const LLVector3& curve, const LLVector3& offset, F32 ease)
    {
        return ease <= 0.f || offset == LLVector3::zero ? curve : curve + offset * ease;
    }
};
}

#endif
