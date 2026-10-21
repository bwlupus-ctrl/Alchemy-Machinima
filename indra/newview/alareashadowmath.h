/** Deterministic, allocation-free Phase 2 selection and view policy. */
#ifndef AL_AREA_SHADOW_MATH_H
#define AL_AREA_SHADOW_MATH_H
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace ALAreaShadow
{
struct View
{
    bool cube = false, prism = false, hud = false, impostor = false;
    bool reflection = false, tiled = false;
};
inline bool allowed(const View& v)
{
    return !(v.cube || v.prism || v.hud || v.impostor || v.reflection || v.tiled);
}
inline double contribution(double radius, double distance, double luminance, double attenuation)
{
    const double score = radius / std::max(distance, .01) * luminance * attenuation;
    return std::isfinite(score) ? std::max(0.0, score) : 0.0;
}
template<class ID> struct Candidate
{
    ID id{};
    bool shadowMap = false;
    double score = 0.0;
};
template<class ID> inline bool before(const Candidate<ID>& a, const Candidate<ID>& b)
{
    if (a.shadowMap != b.shadowMap) return !a.shadowMap;
    if (a.score != b.score) return a.score > b.score;
    return a.id < b.id;
}
template<class ID> struct Selection
{
    std::array<Candidate<ID>, 8> lights{};
    unsigned size = 0, candidates = 0;
    void add(const Candidate<ID>& light, unsigned cap)
    {
        cap = std::clamp(cap, 1u, 8u);
        ++candidates;
        unsigned at = 0;
        while (at < size && !before(light, lights[at])) ++at;
        if (at >= cap) return;
        const unsigned last = std::min(size, cap - 1);
        for (unsigned i = last; i > at; --i) lights[i] = lights[i-1];
        lights[at] = light;
        size = std::min(size + 1, cap);
    }
};
// Integer avalanche also used by the GLSL disk sampler. Seed by ID, never slot.
inline std::uint32_t hash(std::uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; return x ^ (x >> 16);
}
struct Quality { int samples, steps; bool half; };
inline Quality quality(int preset)
{
    constexpr Quality presets[] = {{2,8,true}, {4,12,true}, {8,20,true}, {16,32,false}};
    return presets[std::clamp(preset,0,3)];
}
}
#endif
