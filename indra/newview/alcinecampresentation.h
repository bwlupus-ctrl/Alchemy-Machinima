/** Shared main-camera cut presentation; independent of avatar/renderer state. */
#ifndef AL_CINECAM_PRESENTATION_H
#define AL_CINECAM_PRESENTATION_H

#include "alcameracurve.h"
#include "llquaternion.h"
#include "m3math.h"
#include "v3math.h"

namespace ALCineCamPresentation
{
struct CutEase
{
    bool mActive = false;
    F32 mDuration = 0.f;
    S32 mCurveId = 0;
    F32 mBezier[4] = { 0.42f, 0.f, 0.58f, 1.f };
    F32 mFeather = 0.f;
    LLVector3 mFromPos;
    LLQuaternion mFromRot;
    F32 mFromFov = 0.f;

    bool begin(F32 duration, F64 age, F64 elapsed, bool stale_reentry,
               const LLVector3& position, const LLQuaternion& rotation,
               F32 fov, S32 curve, const F32* bezier, F32 feather)
    {
        mActive = duration > 0.f && age >= 0.0 && age < duration &&
            elapsed >= 0.0 && elapsed < duration && !stale_reentry;
        if (mActive)
        {
            mDuration = duration;
            mCurveId = curve;
            mFeather = feather;
            for (S32 i = 0; i < 4; ++i) mBezier[i] = bezier[i];
            mFromPos = position;
            mFromRot = rotation;
            mFromFov = fov;
        }
        return mActive;
    }

    // Unscaled switcher elapsed time lets a camera ease finish during a world freeze.
    bool apply(F64 elapsed, LLVector3& position, LLQuaternion& rotation, F32& weight)
    {
        weight = 1.f;
        if (!mActive) return false;
        const F32 u = mDuration > 0.f ? static_cast<F32>(elapsed / mDuration) : 1.f;
        if (u >= 1.f || u < 0.f)
        {
            mActive = false;
            return false;
        }
        weight = ALCameraCurve::evalFeathered(mCurveId, u,
            mBezier[0], mBezier[1], mBezier[2], mBezier[3], mFeather);
        position = mFromPos + (position - mFromPos) * weight;
        rotation = nlerp(weight, mFromRot, rotation);
        return true;
    }

    F32 blendFov(F32 lensed_fov, bool easing, F32 weight) const
    {
        return easing ? mFromFov + (lensed_fov - mFromFov) * weight : lensed_fov;
    }
};

// The real render camera and CPU fixture use this same publication path.
template<class Camera>
void writeCamera(Camera& camera, const LLVector3& position,
                 const LLQuaternion& rotation, F32 fov, bool easing)
{
    // Intermediate lens values stay local; publish the settled FOV normally.
    if (easing)
        camera.setViewNoBroadcast(llclamp(fov, camera.getMinView(), camera.getMaxView()));
    else
        camera.setView(fov);
    camera.setOrigin(position);
    const LLMatrix3 axes(rotation);
    camera.mXAxis = LLVector3(axes.mMatrix[0]);
    camera.mYAxis = LLVector3(axes.mMatrix[1]);
    camera.mZAxis = LLVector3(axes.mMatrix[2]);
}
}

#endif
