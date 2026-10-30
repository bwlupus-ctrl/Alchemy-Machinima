/**
 * @file alprobereach.h
 * @brief Pure reflection-probe reach and dynamic allocation policy.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#pragma once

#include "stdtypes.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace ALProbeReach
{
// Bit checks survive Release /fp:fast. Uncertain geometry must retain a probe.
inline bool finite(F32 value)
{
    U32 bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return (bits & 0x7f800000u) != 0x7f800000u;
}

inline bool inReach(const F32* origin, F32 radius, const F32* camera, F32 draw_distance)
{
    if (!finite(radius) || radius < 0.f || !finite(draw_distance))
    {
        return true;
    }
    F64 distance_squared = 0.0;
    for (U32 axis = 0; axis < 3; ++axis)
    {
        if (!finite(origin[axis]) || !finite(camera[axis]))
        {
            return true;
        }
        const F64 delta = static_cast<F64>(origin[axis]) - camera[axis];
        distance_squared += delta * delta;
    }
    const F64 reach = static_cast<F64>(std::max(draw_distance, 0.f)) + radius;
    return distance_squared <= reach * reach;
}

// Main eye plus the bounded secondary-camera registry. No heap storage or
// frustum test: turning back must not require all twelve probe passes again.
struct Views
{
    struct View
    {
        std::array<F32, 3> mOrigin;
        F32 mDrawDistance;
    };
    std::array<View, 9> mViews;
    U32 mCount = 0;
    bool mKeepAll = false;

    void add(const F32* origin, F32 draw_distance)
    {
        if (mCount == mViews.size())
        {
            mKeepAll = true;
            return;
        }
        mViews[mCount++] = { { origin[0], origin[1], origin[2] }, draw_distance };
    }

    bool inReach(const F32* origin, F32 radius, bool exempt = false) const
    {
        if (exempt || mKeepAll || mCount == 0)
        {
            return true;
        }
        for (U32 i = 0; i < mCount; ++i)
        {
            if (ALProbeReach::inReach(origin, radius, mViews[i].mOrigin.data(), mViews[i].mDrawDistance))
            {
                return true;
            }
        }
        return false;
    }
};

constexpr U32 RELEVANT_STEP = 16;
inline U32 stepRelevant(U32 relevant, U32 current)
{
    const U32 stepped = ((relevant + RELEVANT_STEP - 1) / RELEVANT_STEP) * RELEVANT_STEP;
    return stepped > current || (current > RELEVANT_STEP && stepped < current - RELEVANT_STEP)
        ? stepped : current;
}

// Preserve the existing level/buffer/rounding formula; only its input count
// changes. Allocation -1 remains OFF, independently of reach culling.
inline U32 dynamicProbeCount(S32 level, S32 allocation, U32 relevant, U32 limit)
{
    if (allocation < 0)
    {
        return limit;
    }
    U32 count = level == 0 ? 1u : level == 1 ? relevant : level == 2 ? std::max(relevant, 128u) : 256u;
    if (allocation > 1)
    {
        const U32 buffer = static_cast<U32>(allocation);
        count = ((count + buffer / 2) / buffer) * 16;
    }
    else
    {
        count += static_cast<U32>(allocation);
    }
    return std::clamp(count, 1u, limit);
}
}
