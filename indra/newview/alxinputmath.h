// [XInputPad] Pure trigger math and sample eligibility; no platform dependencies.
#ifndef AL_XINPUT_MATH_H
#define AL_XINPUT_MATH_H

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ALXInput
{
inline float bounded(float value, float low, float high, float fallback)
{
    return std::isfinite(value) ? std::max(low, std::min(high, value)) : fallback;
}

inline float shape(float value, float deadzone, float curve)
{
    value = bounded(value, 0.f, 1.f, 0.f);
    deadzone = bounded(deadzone, 0.f, .5f, .06f);
    curve = bounded(curve, 1.f, 4.f, 2.f);
    return value <= deadzone ? 0.f : std::pow((value - deadzone) / (1.f - deadzone), curve);
}

inline float roll(float left, float right, float deadzone, float curve)
{
    return bounded(shape(right, deadzone, curve) - shape(left, deadzone, curve), -1.f, 1.f, 0.f);
}

inline float throttle(float left, float right, float minimum, float maximum)
{
    minimum = bounded(minimum, .01f, 1.f, .1f);
    maximum = bounded(maximum, 1.f, 20.f, 4.f);
    return left == 0.f && right == 0.f ? 1.f
        : std::pow(maximum, right) * std::pow(minimum, left);
}

inline bool compatible(const int* axes, bool cursor)
{
    if (cursor || axes[3] != 2) return false;
    for (int i = 0; i < 7; ++i)
        if (i != 3 && axes[i] == 2) return false;
    return true;
}

inline unsigned short edges(unsigned short current, unsigned short previous,
                            bool prime, bool focused)
{
    return prime || !focused ? 0 : static_cast<unsigned short>(current & ~previous);
}

inline bool fresh(bool valid, bool focused, std::uint64_t sample, std::uint64_t generation)
{
    return valid && focused && sample == generation;
}

inline bool primeSample(bool first, bool focused, bool was_focused,
                        std::uint64_t generation, std::uint64_t last)
{
    return first || (focused && !was_focused) || generation != last + 1;
}
}
#endif
