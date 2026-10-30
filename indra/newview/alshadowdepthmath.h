/** Allocation-free shadow-depth weighting and distance-reach uniforms. */
#ifndef AL_SHADOW_DEPTH_MATH_H
#define AL_SHADOW_DEPTH_MATH_H
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

namespace ALShadowDepth
{
// Release uses /fp:fast: floating-point isfinite checks can be folded away.
inline bool finite(float x)
{
    return (std::bit_cast<std::uint32_t>(x)&0x7f800000u) != 0x7f800000u;
}
inline float amount(float x)
{
    return finite(x) ? std::clamp(x,0.f,1.f) : 0.f;
}
inline float weight(float energy, float fill)
{
    if (!finite(energy) || !finite(fill) || energy <= 0.f) return 0.f;
    return energy/(energy+std::max(fill,0.f)+.0001f);
}
inline float occlusion(const float* visibility, const float* energy, unsigned count, float fill)
{
    float occ = 0.f;
    for (unsigned c=0; c<count; ++c)
        occ = std::max(occ,(1.f-amount(visibility[c]))*weight(energy[c],fill));
    return occ;
}
inline float fillFactor(float depth, const float* visibility, const float* energy, unsigned count, float fill)
{
    const float d = amount(depth);
    if (d == 0.f) return 1.f; // Real identity branch: never evaluate poisoned unused inputs.
    return 1.f-d*occlusion(visibility,energy,count,fill);
}
inline std::array<float,4> farReachUniform(bool enabled, float reach, float max_distance)
{
    if (!enabled || !finite(reach) || !finite(max_distance)) return {};
    const float distance = std::clamp(max_distance,.1f,64.f);
    const float far_distance = std::clamp(reach,8.f,48.f);
    if (far_distance <= distance) return {};
    const float z0 = std::max(12.f,1.5f*distance);
    return {far_distance,z0,std::max(z0+4.f,1.5f*far_distance),std::min(std::sqrt(far_distance/distance),2.f)};
}
// Result is (trace distance, thickness multiplier). The near path performs no mix.
inline std::array<float,2> farTrace(float zeq, const std::array<float,4>& u, float max_distance = 8.f)
{
    if (u[0] <= max_distance || !finite(zeq) || zeq <= u[1]) return {max_distance,1.f};
    const float x = std::clamp((zeq-u[1])/(u[2]-u[1]),0.f,1.f);
    const float t = x*x*(3.f-2.f*x);
    return {max_distance+(u[0]-max_distance)*t,1.f+(u[3]-1.f)*t};
}
}
#endif
