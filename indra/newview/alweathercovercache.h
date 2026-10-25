/**
 * @file alweathercovercache.h
 * @brief World-cell cache for precipitation-only cover traversals.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
#ifndef AL_WEATHER_COVER_CACHE_H
#define AL_WEATHER_COVER_CACHE_H
#include "stdtypes.h"
#include <array>
#include <cmath>

class ALWeatherCoverCache
{
public:
    bool reuse(const std::array<double, 3>& eye, const std::array<double, 3>& origin,
               U64 geometry, U32 place, U32 resolution, float extent)
    {
        const std::array<double, 3> cell{std::floor(eye[0]/32.), std::floor(eye[1]/32.), std::floor(eye[2]/32.)};
        const bool same = mValid && cell == mCell && origin == mOrigin && geometry == mGeometry &&
            place == mPlace && resolution == mResolution && extent == mExtent;
        mCell = cell; mOrigin = origin; mGeometry = geometry;
        mPlace = place; mResolution = resolution; mExtent = extent;
        mValid = false; // Only a complete traversal or validated reuse publishes.
        return same;
    }
    void publish() { mValid = true; }
    void invalidate() { mValid = false; }
private:
    std::array<double, 3> mCell{}, mOrigin{};
    U64 mGeometry = 0;
    U32 mPlace = 0, mResolution = 0;
    float mExtent = 0.f;
    bool mValid = false;
};
#endif
