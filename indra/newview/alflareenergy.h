#ifndef AL_FLARE_ENERGY_H
#define AL_FLARE_ENERGY_H

#include <cmath>
#include <algorithm>

namespace ALFlareEnergy
{
// Normalize elapsed time once for both CPU visibility and GPU energy.
inline float finiteDelta(float dt) { return std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f; }
inline float alpha(float dt, float seconds)
{
    if (!std::isfinite(seconds) || seconds <= 0.f) return 1.f;
    return 1.f - std::exp(-dt / seconds);
}
inline float apply(float previous, float target, float attackAlpha, float releaseAlpha)
{
    if (!std::isfinite(target)) target = 0.f;
    if (!std::isfinite(previous)) previous = target;
    return previous + (target - previous) * (target > previous ? attackAlpha : releaseAlpha);
}
inline float smooth(float previous, float target, float dt, float attack = .100f, float release = .450f)
{
    dt = finiteDelta(dt);
    return apply(previous, target, alpha(dt, attack), alpha(dt, release));
}
// Mirrors the established GLSL soft knee. Steady-state energy is unchanged.
inline float energy(float luma, float threshold)
{
    const float knee = std::max(threshold * .5f, .1f);
    const float t = std::clamp((luma - threshold) / knee, 0.f, 1.f);
    return t * t * (3.f - 2.f * t) * std::min(luma / std::max(threshold + knee, .001f), 4.f);
}
}
#endif
