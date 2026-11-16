/** Session-only clone transitions: pure maths and render parameters. No viewer dependencies.
 *
 * CONTRACT (lead, 11/16): the render side (native mask, Studio overlay, effect shell) and the
 * engine side (state machine, UI, lifecycle) meet here. Do not change the meaning of an existing
 * field; add fields at the end and note it in the worktree REPORT.
 *
 * One transition = one field value f(x) in [0,1) per surface point, one progress p in [0,1].
 *   outgoing keeps a point  when f >= p   (it leaves as p rises)
 *   incoming keeps a point  when f <  p   (it arrives as p rises)
 * Same style, seed, p and coordinate on both halves => every point is kept by exactly one half
 * (for matched geometry; see PLAN.md 3.5). p == 0: only outgoing; p == 1: only incoming.
 * The GLSL field (render side) must match fieldValue() below to within 1e-5 (gate).
 */
#ifndef AL_CLONETRANSITION_H
#define AL_CLONETRANSITION_H
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ALCloneTransition
{
enum Style : int32_t { STYLE_NONE = 0, STYLE_DISSOLVE = 1, STYLE_WIPE = 2, STYLE_CROSSFADE = 3 };
enum Side : int32_t { SIDE_OUTGOING = 0, SIDE_INCOMING = 1 };
enum Ease : int32_t { EASE_SMOOTH = 0, EASE_LINEAR = 1 };
enum Direction : int32_t { DIR_UP = 0, DIR_DOWN = 1 };

// Body-space transition coordinate (PLAN.md 3.1): final skinned agent position minus the body
// root, rotated into the body heading (+X forward, +Y left, +Z up), divided by the body scale.
// Metres for a scale-1 body. Feet near z = 0.
struct BodyFrame
{
    float rootAgent[3] = {0.f, 0.f, 0.f}; // presented root (agent space), recomputed each frame
    float heading = 0.f;                  // radians about +Z
    float scale = 1.f;
};

// Everything a renderer needs for one half of one transition this frame.
// active == false => renderers must behave byte-identically to "no transition".
struct RenderParams
{
    bool active = false;
    int32_t style = STYLE_NONE;
    int32_t side = SIDE_OUTGOING;
    int32_t direction = DIR_UP;
    float p = 0.f;               // eased progress, snapshotted ONCE per frame by the clock tick
    uint32_t seed = 0;           // same for both halves
    float edgeWidth = 0.06f;     // field units; additive edge band just on the kept side of the cut
    float edgeColor[3] = {1.0f, 0.55f, 0.15f};
    float edgeIntensity = 2.0f;  // linear radiance multiplier for the shell edge
    float bodyHeight = 1.8f;     // metres (scale 1); Wipe normalises z by this
    BodyFrame frame;
};

inline float ease(int32_t mode, float t)
{
    t = std::clamp(t, 0.f, 1.f);
    return mode == EASE_LINEAR ? t : t * t * (3.f - 2.f * t);
}

// Deterministic per-seed hash -> [0,1).
inline float hash3(int32_t x, int32_t y, int32_t z, uint32_t seed)
{
    uint32_t h = seed * 747796405u + 2891336453u;
    h ^= uint32_t(x) * 0x8da6b343u; h ^= uint32_t(y) * 0xd8163841u; h ^= uint32_t(z) * 0xcb1ab31fu;
    h = (h ^ (h >> 16)) * 0x7feb352du;
    h = (h ^ (h >> 15)) * 0x846ca68bu;
    h ^= h >> 16;
    return float(h >> 8) * (1.f / 16777216.f);
}

// Smooth value noise in body space (cell size in metres). Shared reference for the GLSL port.
inline float valueNoise(const float pos[3], float cell, uint32_t seed)
{
    const float x = pos[0] / cell, y = pos[1] / cell, z = pos[2] / cell;
    const float fx = std::floor(x), fy = std::floor(y), fz = std::floor(z);
    const int32_t ix = int32_t(fx), iy = int32_t(fy), iz = int32_t(fz);
    auto s = [](float t) { return t * t * (3.f - 2.f * t); };
    const float ux = s(x - fx), uy = s(y - fy), uz = s(z - fz);
    float c[2][2][2];
    for (int a = 0; a < 2; ++a) for (int b = 0; b < 2; ++b) for (int d = 0; d < 2; ++d)
        c[a][b][d] = hash3(ix + a, iy + b, iz + d, seed);
    auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    const float x00 = lerp(c[0][0][0], c[1][0][0], ux), x10 = lerp(c[0][1][0], c[1][1][0], ux);
    const float x01 = lerp(c[0][0][1], c[1][0][1], ux), x11 = lerp(c[0][1][1], c[1][1][1], ux);
    return lerp(lerp(x00, x10, uy), lerp(x01, x11, uy), uz);
}

// Field value in [0,1) at a body-space position. The renderers port THIS function to GLSL.
inline float fieldValue(const RenderParams& r, const float pos[3])
{
    switch (r.style)
    {
    case STYLE_DISSOLVE:
    {
        // two octaves, renormalised so the histogram covers [0,1) well
        const float n = 0.65f * valueNoise(pos, 0.11f, r.seed) + 0.35f * valueNoise(pos, 0.037f, r.seed ^ 0x5bd1e995u);
        return std::clamp((n - 0.15f) / 0.70f, 0.f, 0.99999f);
    }
    case STYLE_WIPE:
    {
        const float h = std::max(r.bodyHeight, 0.1f);
        const float z = std::clamp(pos[2] / h, 0.f, 0.99999f);
        return r.direction == DIR_DOWN ? 0.99999f - z : z;
    }
    case STYLE_CROSSFADE:
    {
        // object-space dither: 1.5 cm cells (shadow-stable, unlike the screen Bayer)
        const float cell = 0.015f;
        return std::min(hash3(int32_t(std::floor(pos[0] / cell)), int32_t(std::floor(pos[1] / cell)),
                              int32_t(std::floor(pos[2] / cell)), r.seed), 0.99999f);
    }
    default:
        return 0.f;
    }
}

// Coverage test. Inactive params keep everything.
inline bool keeps(const RenderParams& r, float field)
{
    if (!r.active || r.style == STYLE_NONE) return true;
    return r.side == SIDE_OUTGOING ? field >= r.p : field < r.p;
}

// Additive shell edge weight in [0,1] for a kept point (0 = no edge). Crossfade has no edge.
inline float edgeWeight(const RenderParams& r, float field)
{
    if (!r.active || r.style == STYLE_CROSSFADE || r.style == STYLE_NONE || r.p <= 0.f || r.p >= 1.f) return 0.f;
    const float d = std::fabs(field - r.p);
    return d < r.edgeWidth ? 1.f - d / r.edgeWidth : 0.f;
}
}
#endif
