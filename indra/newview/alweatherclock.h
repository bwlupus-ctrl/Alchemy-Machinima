/**
 * @file alweatherclock.h
 * @brief Injected 120 Hz weather clock; never reads wall time or changes presentation time.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
#ifndef AL_WEATHER_CLOCK_H
#define AL_WEATHER_CLOCK_H
#include "stdtypes.h"

class ALWeatherClock
{
public:
    static constexpr F64 HZ = 120.0;
    void reset();
    bool advance(F64 presentation_seconds);
    bool step();
    U64 tick() const { return mTick; }
    F64 seconds() const { return static_cast<F64>(mTick) / HZ; }
    F64 sample() const { return mSample; }
    bool catchingUp() const { return mTick < mTarget; }
private:
    bool mInitialized = false;
    F64 mOrigin = 0.0, mLast = 0.0, mSample = 0.0;
    U64 mTick = 0, mTarget = 0;
};
#endif
