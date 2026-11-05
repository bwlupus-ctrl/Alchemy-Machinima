/**
 * @file alghostrestframe.h
 * @brief Pure stored-mesh to default-avatar rest-frame delta for clone looks.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */
#ifndef AL_GHOST_REST_FRAME_H
#define AL_GHOST_REST_FRAME_H

#include <algorithm>
#include <array>
#include <cmath>

namespace ALGhostRestFrame
{
using Delta = std::array<float, 12>; // three GLSL vec4 rows; zero is inert

// inv_bind uses LLMatrix4a storage: x*m[0]+y*m[1]+z*m[2]+m[3].
// DefaultJointWorld is translation only, so DefaultJointWorld * invBind
// preserves the linear part and adds default_world to its translation.
inline Delta compute(const float* inv_bind, const float* default_world)
{
    Delta delta{};
    for (int i = 0; i < 16; ++i)
        if (!std::isfinite(inv_bind[i])) return delta;
    for (int i = 0; i < 3; ++i)
        if (!std::isfinite(default_world[i])) return delta;

    double lengths[3]{};
    for (int c = 0; c < 3; ++c)
    {
        for (int r = 0; r < 3; ++r)
            lengths[c] += double(inv_bind[c*4+r]) * inv_bind[c*4+r];
        lengths[c] = std::sqrt(lengths[c]);
    }
    const double shortest = *std::min_element(lengths, lengths+3);
    const double longest = *std::max_element(lengths, lengths+3);
    if (shortest <= 0.0 || longest > shortest * 1.05) return delta;
    for (int a = 0; a < 3; ++a)
        for (int b = a+1; b < 3; ++b)
        {
            double dot = 0.0;
            for (int r = 0; r < 3; ++r)
                dot += double(inv_bind[a*4+r]) * inv_bind[b*4+r];
            if (std::abs(dot) > 0.05 * lengths[a] * lengths[b]) return delta;
        }

    bool near_identity = true;
    for (int r = 0; r < 3; ++r)
    {
        for (int c = 0; c < 3; ++c)
        {
            delta[r*4+c] = inv_bind[c*4+r] - (r == c ? 1.f : 0.f);
            near_identity = near_identity && std::abs(delta[r*4+c]) <= 0.2f;
        }
        delta[r*4+3] = inv_bind[12+r] + default_world[r];
        if (!std::isfinite(delta[r*4+3])) return Delta{};
    }
    // Custom bind poses can translate ordinary metre meshes by over a metre.
    // Translation alone never changes the stored axes/units used by looks.
    return near_identity ? Delta{} : delta;
}
}
#endif
