/**
 * @file alviewfinder.h
 * @brief Allocation-free formatting for the Cinematic Viewfinder readout.
 *
 * Header-only and free of viewer dependencies so it is unit-tested directly
 * (tests/alviewfinder_test.cpp). The overlay itself is drawn by
 * drawCinematicViewfinder() in llviewerwindow.cpp.
 *
 * The readout is a five-line labelled block:
 *
 *   FOV   33.7°
 *   View  0.588
 *   Lens  40 mm
 *   Roll  -12.5°
 *   FPS   30
 *
 * It also holds the Size (CinematicViewfinderScale) layout and font-step maths.
 */
#ifndef AL_VIEWFINDER_H
#define AL_VIEWFINDER_H

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace ALViewfinder
{
constexpr float PI = 3.14159265358979323846f;

// Virtual film back used everywhere else in the cinematic tools: 24 mm high,
// so a vertical FOV maps to a 35 mm-equivalent focal length of
// f = (24 / 2) / tan(fov / 2).
constexpr float SENSOR_HEIGHT_MM = 24.f;

constexpr int LINE_COUNT = 5;
constexpr int LINE_FOV   = 0;
constexpr int LINE_VIEW  = 1; // raw view angle (zoom), not roll
constexpr int LINE_LENS  = 2;
constexpr int LINE_ROLL  = 3;
constexpr int LINE_FPS   = 4;

// Width the label column is padded to in the plain-text form.
constexpr int LABEL_PAD = 6;

// A vertical FOV is usable only strictly between 0 and pi radians.
inline bool validFov(float fov_radians)
{
    return std::isfinite(fov_radians) && fov_radians > 0.f && fov_radians < PI;
}

inline float fovDegrees(float fov_radians)
{
    return fov_radians * (180.f / PI);
}

// 35 mm-equivalent focal length in millimetres; 0 for an unusable FOV.
inline float focalLengthMm(float fov_radians)
{
    if (!validFov(fov_radians))
    {
        return 0.f;
    }
    return (SENSOR_HEIGHT_MM * 0.5f) / std::tan(fov_radians * 0.5f);
}

// Wraps degrees into (-180, 180]: 190 -> -170, -190 -> 170, 180 stays 180.
// Non-finite input is returned unchanged (the caller prints a placeholder).
inline float wrapDegrees(float degrees)
{
    if (!std::isfinite(degrees))
    {
        return degrees;
    }
    degrees = std::fmod(degrees, 360.f);
    if (degrees > 180.f)
    {
        degrees -= 360.f;
    }
    else if (degrees <= -180.f)
    {
        degrees += 360.f;
    }
    return degrees;
}

// Camera roll (LLAgentCamera::getRollAngle(), radians, accumulates unwrapped)
// as signed degrees in (-180, 180].
inline float rollDegrees(float roll_radians)
{
    return wrapDegrees(roll_radians * (180.f / PI));
}

// Whole frames per second, never negative, never non-finite.
inline int roundedFps(float fps)
{
    if (!std::isfinite(fps) || fps <= 0.f)
    {
        return 0;
    }
    return static_cast<int>(fps + 0.5f);
}

inline const char* label(int line)
{
    switch (line)
    {
    case LINE_FOV:   return "FOV";
    case LINE_VIEW:  return "View";
    case LINE_LENS:  return "Lens";
    case LINE_ROLL:  return "Roll";
    case LINE_FPS:   return "FPS";
    default:         return "";
    }
}

// Writes the value column for one line (UTF-8) into buf and always
// NUL-terminates it when size > 0. Returns the snprintf result.
//
//   LINE_FOV   vertical FOV, 1 decimal, degrees      "33.7°"  (U+00B0)
//   LINE_VIEW  raw view angle, 3 decimals, radians   "0.588"
//   LINE_LENS  35 mm-equivalent focal length, whole  "40 mm"
//   LINE_ROLL  camera roll, 1 decimal, signed deg    "-12.5°" (wrapped to -180..180)
//   LINE_FPS   whole frames per second               "30"
//
// An unusable FOV prints "--" for the three FOV-derived values; a non-finite
// roll prints "--" for the roll value.
inline int formatValue(int line, char* buf, std::size_t size, float fov_radians, float roll_radians, float fps)
{
    if (!buf || size == 0)
    {
        return 0;
    }
    if (line == LINE_FPS)
    {
        return std::snprintf(buf, size, "%d", roundedFps(fps));
    }
    if (line == LINE_ROLL)
    {
        float degrees = rollDegrees(roll_radians);
        if (!std::isfinite(degrees))
        {
            return std::snprintf(buf, size, "--");
        }
        if (std::fabs(degrees) < 0.05f)
        {
            degrees = 0.f; // never print "-0.0"
        }
        return std::snprintf(buf, size, "%.1f" "\xC2\xB0", static_cast<double>(degrees));
    }
    if (line < LINE_FOV || line > LINE_FPS)
    {
        buf[0] = '\0';
        return 0;
    }
    if (!validFov(fov_radians))
    {
        return std::snprintf(buf, size, "--");
    }
    switch (line)
    {
    case LINE_FOV:
        return std::snprintf(buf, size, "%.1f" "\xC2\xB0", static_cast<double>(fovDegrees(fov_radians)));
    case LINE_VIEW:
        return std::snprintf(buf, size, "%.3f", static_cast<double>(fov_radians));
    default:
        return std::snprintf(buf, size, "%.0f mm", static_cast<double>(focalLengthMm(fov_radians)));
    }
}

// One plain-text line: the label left-aligned and padded to LABEL_PAD
// columns, then the value, e.g. "View  0.588" or "FPS   30". The overlay draws
// labels and values as two pixel-aligned columns instead (a proportional font
// does not line up padded spaces); this is the canonical text of each line.
inline int formatLine(int line, char* buf, std::size_t size, float fov_radians, float roll_radians, float fps)
{
    if (!buf || size == 0)
    {
        return 0;
    }
    char value[32];
    formatValue(line, value, sizeof(value), fov_radians, roll_radians, fps);
    return std::snprintf(buf, size, "%-*s%s", LABEL_PAD, label(line), value);
}

// ---------------------------------------------------------------------------
// Size (CinematicViewfinderScale)
//
// One user scale multiplies every dimension together: line thickness, bracket
// arm length, crosshair size, margins, the readout offset from the crosshair
// and the font. It stacks on top of the UI scale, because all values are in
// scaled UI units. At scale 1.0 the numbers below are the original design.
// ---------------------------------------------------------------------------
constexpr float SCALE_MIN = 0.5f;
constexpr float SCALE_MAX = 4.0f;

// Out-of-range and non-finite settings (hand-edited files) fall back safely.
inline float clampScale(float scale)
{
    if (!std::isfinite(scale))
    {
        return 1.f;
    }
    return scale < SCALE_MIN ? SCALE_MIN : (scale > SCALE_MAX ? SCALE_MAX : scale);
}

inline int scaledUnits(float units, float scale)
{
    return static_cast<int>(units * scale + 0.5f);
}

struct Layout
{
    int line;       // bar thickness, at least 1
    int shadow;     // shadow offset down-right, at least 1
    int margin;     // inset of the brackets from the frame / view edge
    int arm_min;    // bracket arm length bounds
    int arm_max;
    int min_box;    // smallest inner box worth drawing in
    int cross;      // crosshair half length
    int text_dx;    // readout block offset right of the crosshair centre
    int text_dy;    // readout block offset below the crosshair centre
    int col_gap;    // gap between the label and value columns
    int line_extra; // extra leading between readout lines
};

inline Layout layout(float scale)
{
    scale = clampScale(scale);
    Layout l;
    l.line       = scaledUnits(1.f, scale) < 1 ? 1 : scaledUnits(1.f, scale);
    l.shadow     = l.line;
    l.margin     = scaledUnits(12.f, scale);
    l.arm_min    = scaledUnits(12.f, scale);
    l.arm_max    = scaledUnits(40.f, scale);
    l.min_box    = scaledUnits(64.f, scale);
    l.cross      = scaledUnits(6.f, scale);
    l.text_dx    = l.cross + scaledUnits(4.f, scale);
    l.text_dy    = l.cross + scaledUnits(2.f, scale);
    l.col_gap    = scaledUnits(6.f, scale);
    l.line_extra = scaledUnits(1.f, scale);
    return l;
}

// Bracket arm length for a drawing box: a fraction of its short side, bounded
// by the scaled min/max so it neither vanishes nor swallows the frame.
inline int armLength(const Layout& l, int box_width, int box_height)
{
    const int short_side = box_width < box_height ? box_width : box_height;
    int arm = short_side / 14;
    arm = arm < l.arm_min ? l.arm_min : arm;
    return arm > l.arm_max ? l.arm_max : arm;
}

// The readout font comes from a ladder of FONT_STEPS named point sizes
// (ViewfinderN in fonts.xml), each a 2^(1/4) multiple of the previous one,
// anchored on the 9 pt "Small" UI font: step 0 is 0.5x, step 4 is 1x (the same
// size as Small), step 8 is 2x, step 12 is 4x. Picking the nearest rung keeps
// the glyphs rasterised at their real size (crisp) instead of stretching a
// bitmap; the layout still follows the exact scale and measures the real font,
// so only the glyph size snaps (at most about 9% from the exact scale).
constexpr int FONT_STEPS     = 13;
constexpr int FONT_STEP_UNIT = 4; // steps per doubling

inline int fontStep(float scale)
{
    scale = clampScale(scale);
    // step = round(FONT_STEP_UNIT * log2(scale / SCALE_MIN))
    const float step = FONT_STEP_UNIT * std::log2(scale / SCALE_MIN);
    int rung = static_cast<int>(step + 0.5f);
    rung = rung < 0 ? 0 : rung;
    return rung > FONT_STEPS - 1 ? FONT_STEPS - 1 : rung;
}

// Size names registered as font_size entries in skins/default/xui/en/fonts.xml.
inline const char* fontSizeName(int step)
{
    static const char* const NAMES[FONT_STEPS] = {
        "Viewfinder0",  "Viewfinder1",  "Viewfinder2",  "Viewfinder3",  "Viewfinder4",
        "Viewfinder5",  "Viewfinder6",  "Viewfinder7",  "Viewfinder8",  "Viewfinder9",
        "Viewfinder10", "Viewfinder11", "Viewfinder12"
    };
    step = step < 0 ? 0 : (step > FONT_STEPS - 1 ? FONT_STEPS - 1 : step);
    return NAMES[step];
}
} // namespace ALViewfinder

#endif // AL_VIEWFINDER_H
