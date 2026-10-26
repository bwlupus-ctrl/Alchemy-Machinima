/**
 * @file alweatherclock.cpp
 * @brief Fixed weather clock, driven solely by the frozen presentation input.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
#include "linden_common.h"
#include "alweatherclock.h"
#include <cmath>

void ALWeatherClock::reset()
{
    *this = ALWeatherClock{};
}

bool ALWeatherClock::advance(F64 presentation_seconds)
{
    if (!std::isfinite(presentation_seconds) || presentation_seconds < 0.0) return false;
    if (mInitialized && presentation_seconds < mLast) reset();
    if (!mInitialized)
    {
        mOrigin = presentation_seconds;
        mInitialized = true;
    }
    mLast = presentation_seconds;
    mSample = presentation_seconds - mOrigin;
    // Bound conversions to representable integer ticks, including hand-edited inputs.
    if (mSample > 1.e10) return false;
    mTarget = static_cast<U64>(std::floor(mSample * HZ + 1.e-7));
    return true;
}

bool ALWeatherClock::step()
{
    if (mTick >= mTarget) return false;
    ++mTick;
    return true;
}
