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

// Drone's clutch mode treats the triggers independently: RT is throttle and
// LT is brake. The caller owns sample freshness/association gating.
inline void droneTriggers(float left, float right, float deadzone, float curve,
                          float& throttle_out, float& brake_out)
{
    throttle_out = shape(right, deadzone, curve);
    brake_out = shape(left, deadzone, curve);
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

// The persisted D-pad slots are deliberately independent of viewer settings so
// the None-only default and migration matrices can be exhaustively tested.
struct DpadSettings
{
    int up;
    int left;
    int right;
    int version;
};

enum DpadAction
{
    DPAD_NONE = 0,
    DPAD_CUE_GO = 1,
    DPAD_CUE_BACK = 2,
    DPAD_LEVEL = 3,
    DPAD_FLIGHT_MODEL = 4,
    DPAD_RELEASE = 5,
    DPAD_RECENTRE = 6,
    DPAD_ACTION_COUNT = 7
};

inline DpadSettings fillDroneDpadDefaults(DpadSettings settings)
{
    if (settings.up == DPAD_NONE) settings.up = DPAD_FLIGHT_MODEL;
    if (settings.left == DPAD_NONE) settings.left = DPAD_RELEASE;
    if (settings.right == DPAD_NONE) settings.right = DPAD_RECENTRE;
    return settings;
}

inline DpadSettings migrateDroneDpad(DpadSettings settings)
{
    constexpr int DPAD_MIGRATION_DRONE = 1;
    if (settings.version < DPAD_MIGRATION_DRONE)
    {
        settings = fillDroneDpadDefaults(settings);
        settings.version = DPAD_MIGRATION_DRONE;
    }
    return settings;
}

// [XInputRumble] Pure rumble pattern player. Levels are 0-1 per motor; the
// runtime scales by the strength setting and converts to the XInput U16 range.
struct RumbleStep
{
    float low;  // low-frequency (heavy) motor
    float high; // high-frequency (light) motor
    int ms;
};

enum RumbleId
{
    RUMBLE_CONNECT = 0,
    RUMBLE_FLYCAM_ON,
    RUMBLE_FLYCAM_OFF,
    RUMBLE_MODE_TICK,
    RUMBLE_COUNT
};

struct RumblePattern
{
    int count;
    RumbleStep steps[3];
};

inline const RumblePattern* rumblePattern(int id)
{
    static const RumblePattern patterns[RUMBLE_COUNT] =
    {
        { 3, {{ .35f, 0.f, 90 }, { 0.f, 0.f, 60 }, { 0.f, .7f, 110 }} },  // CONNECT "hello" ramp
        { 3, {{ .5f, .5f, 70 }, { 0.f, 0.f, 70 }, { .5f, .5f, 70 }} },    // FLYCAM_ON double pulse
        { 1, {{ .35f, .1f, 220 }, { 0.f, 0.f, 0 }, { 0.f, 0.f, 0 }} },    // FLYCAM_OFF soft thud
        { 1, {{ 0.f, .45f, 45 }, { 0.f, 0.f, 0 }, { 0.f, 0.f, 0 }} },     // MODE_TICK
    };
    return id >= 0 && id < RUMBLE_COUNT ? &patterns[id] : nullptr;
}

// Motor levels at elapsed_ms into the pattern; false once the pattern is finished
// (or invalid). Negative/NaN elapsed is treated as the start.
inline bool rumbleLevels(const RumblePattern* pattern, float elapsed_ms, float& low, float& high)
{
    low = high = 0.f;
    if (!pattern) return false;
    elapsed_ms = bounded(elapsed_ms, 0.f, 1.0e9f, 0.f);
    float end = 0.f;
    for (int i = 0; i < pattern->count && i < 3; ++i)
    {
        end += static_cast<float>(pattern->steps[i].ms);
        if (elapsed_ms < end)
        {
            low = pattern->steps[i].low;
            high = pattern->steps[i].high;
            return true;
        }
    }
    return false;
}

// 0-1 level times 0-1 strength -> XInput 0-65535, clamped (NaN -> 0).
inline std::uint16_t rumbleMotor(float level, float strength)
{
    const float scaled = bounded(level, 0.f, 1.f, 0.f) * bounded(strength, 0.f, 1.f, 0.f);
    return static_cast<std::uint16_t>(std::lround(bounded(scaled, 0.f, 1.f, 0.f) * 65535.f));
}
}
#endif
