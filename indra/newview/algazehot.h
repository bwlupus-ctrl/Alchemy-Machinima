/** Live gaze envelope. Authored slots and runtime deliberately have separate lifetimes. */
#ifndef AL_GAZE_HOT_H
#define AL_GAZE_HOT_H
#include "algazemath.h"

namespace ALGazeHot
{
struct State
{
    S32 mSlot = -1;
    F64 mStart = 0.0, mRelease = -1.0;
    F32 mAcquire = .12f, mReturn = 1.2f, mFrom = 0.f, mReleaseWeight = 0.f;
    F64 mDwell = 2.0;
    bool mHold = false, mStay = false;

    F32 weight(F64 now) const
    {
        if (mSlot < 0) return 0.f;
        const F64 age = llmax(now - mStart, 0.0);
        ALGazeMath::GazeMacroParams p;
        p.mAcquireSec = mAcquire;
        // Open hold; release is explicit so releasing during acquisition is continuous.
        const F32 acquired = mFrom + (1.f - mFrom) * ALGazeMath::evalGazeMacro(
            ALGazeMath::GAZE_MACRO_NONE, 0, age, p).mTargetWeight;
        const F64 automatic = mStart + mAcquire + mDwell;
        const F64 release = mRelease >= 0.0 ? mRelease
            : (!mHold && !mStay ? automatic : -1.0);
        if (release < 0.0 || now < release) return acquired;
        const F32 at_release = mRelease >= 0.0 ? mReleaseWeight : 1.f;
        return at_release * (mReturn > 0.f
            ? 1.f - ALGazeMath::cueSmoothStep((now - release) / mReturn) : 0.f);
    }
    bool returning(F64 now) const
    {
        return mRelease >= 0.0 || (!mHold && !mStay && now >= mStart + mAcquire + mDwell);
    }
    F32 acquireFraction(F64 now) const
    { return mAcquire > 0.f ? ALGazeMath::cueSmoothStep((now - mStart) / mAcquire) : 1.f; }
    bool finished(F64 now) const
    {
        const F64 release = mRelease >= 0.0 ? mRelease
            : (!mHold && !mStay ? mStart + mAcquire + mDwell : -1.0);
        return mSlot < 0 || (release >= 0.0 && now >= release + mReturn);
    }
    void fire(S32 slot, F64 now, F32 acquire, F64 dwell, F32 back, bool hold, bool stay)
    {
        mFrom = weight(now);
        mSlot = slot; mStart = now; mRelease = -1.0;
        mAcquire = acquire; mDwell = dwell; mReturn = back; mHold = hold; mStay = stay;
    }
    void release(F64 now)
    {
        if (mRelease >= 0.0) return;
        mReleaseWeight = weight(now); mRelease = now;
    }
};
}
#endif
