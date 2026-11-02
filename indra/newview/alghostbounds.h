/** Pure bounds math shared by entity-clone selection and its logic check. */
#ifndef AL_ALGHOSTBOUNDS_H
#define AL_ALGHOSTBOUNDS_H

#include <array>
#include <cstdint>
#include <cstring>

namespace ALGhostBounds
{
using Point = std::array<float, 3>;

// World metres. Separate enter/leave thresholds prevent animation noise at
// 20 cm from switching between the historical standing box and the body box.
constexpr float DISPLACED_ENTER = .20f;
constexpr float DISPLACED_LEAVE = .15f;

inline bool displaced(bool previous, bool seated, const Point& center,
                      const Point& root)
{
    const float x = center[0] - root[0];
    const float y = center[1] - root[1];
    const float threshold = previous ? DISPLACED_LEAVE : DISPLACED_ENTER;
    return seated || x * x + y * y > threshold * threshold;
}

inline bool finite(float value)
{
    // Release uses /fp:fast; a bit check must survive finite-math assumptions.
    std::uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return (bits & 0x7f800000u) != 0x7f800000u;
}

inline bool centerAndSize(const Point& minimum, const Point& maximum,
                          Point& center, Point& size)
{
    Point c, s;
    for (unsigned axis = 0; axis < 3; ++axis)
    {
        if (!finite(minimum[axis]) || !finite(maximum[axis]) ||
            maximum[axis] <= minimum[axis]) return false;
        c[axis] = minimum[axis] * .5f + maximum[axis] * .5f;
        s[axis] = maximum[axis] - minimum[axis];
        if (!finite(c[axis]) || !finite(s[axis])) return false;
    }
    center = c;
    size = s;
    return true;
}
}
#endif
