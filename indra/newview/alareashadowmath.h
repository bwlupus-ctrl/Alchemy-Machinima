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
    // Object UUID in production, never collector order or drawable address.
    // One candidate per emitting object; area channels do not rank faces.
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
inline float lightBias(float bias)
{
    return std::isfinite(bias) ? std::clamp(bias,0.f,4.f) : .25f;
}
inline Quality quality(int preset)
{
    constexpr Quality presets[] = {{4,8,true}, {8,12,true}, {12,20,true}, {24,32,false}};
    return presets[std::clamp(preset,0,3)];
}
// Runtime work admission. Integer arithmetic and fixed degradation order; no timing input.
// The setting can lower the safety ceiling, never remove it.
struct Workload
{
    Quality q;
    unsigned channels = 0;
    std::uint64_t pixels = 0, iterations = 0, budget = 0, drawCap = 0;
};
inline Workload workload(Quality q, unsigned width, unsigned height, unsigned lights,
    int millions, unsigned requestedLights)
{
    Workload w{};
    w.q = {std::clamp(q.samples,1,32),std::clamp(q.steps,1,64),q.half};
    lights = std::min(lights,8u);
    // Reserve for the requested cap, never the changing visible set. At least
    // the actual count keeps malformed callers within the hard frame ceiling.
    const unsigned basis = std::max(lights,std::clamp(requestedLights,1u,8u));
    w.budget = std::uint64_t(std::clamp(millions,1,1500))*1000000;
    w.drawCap = w.budget/2;
    const auto update = [&]()
    {
        w.pixels = w.q.half ? ((std::uint64_t(width)+1)/2)*((std::uint64_t(height)+1)/2) :
            std::uint64_t(width)*height;
        w.iterations = w.pixels*lights*w.q.samples*w.q.steps;
    };
    // Render-target dimensions are GL-sized; bound pathological pure-function inputs too.
    if (width > 65536 || height > 65536) return w;
    update();
    const auto over = [&]() { return w.pixels*basis*w.q.samples*w.q.steps>w.budget ||
        w.pixels*w.q.samples*w.q.steps>w.drawCap; };
    while (over() && w.q.samples>1) { --w.q.samples; update(); }
    while (over() && w.q.steps>1) { --w.q.steps; update(); }
    if (over() && !w.q.half) { w.q.half=true; update(); }
    // Even one channel/one step cannot fit: skip this view, never exceed the cap.
    if (over()) { w.iterations=0; return w; }
    const auto perChannel = w.pixels*w.q.samples*w.q.steps;
    w.channels = lights && perChannel ? unsigned(std::min<std::uint64_t>(4,w.drawCap/perChannel)) : 0;
    return w;
}

}
#endif
