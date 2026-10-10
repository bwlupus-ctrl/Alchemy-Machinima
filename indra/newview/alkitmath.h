/**
 * @file alkitmath.h
 * @brief Allocation-free maths shared by the Director kit widgets and tests.
 */
#ifndef AL_KIT_MATH_H
#define AL_KIT_MATH_H

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace ALKitMath
{
constexpr float PI = 3.14159265358979323846f;
constexpr float DEG_TO_RAD = PI / 180.f;
constexpr float RAD_TO_DEG = 180.f / PI;

inline float clamp(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }

inline float wrap180(float degrees)
{
    degrees = std::fmod(degrees + 180.f, 360.f);
    if (degrees < 0.f) degrees += 360.f;
    return degrees - 180.f;
}

inline float wrapRange(float value,float minimum,float maximum)
{
    const float span=maximum-minimum;if(!(span>0.f))return minimum;value=std::fmod(value-minimum,span);if(value<0.f)value+=span;return minimum+value;
}

struct DialGeometry{float cx=0.f,cy=0.f,radius=0.f,letter_radius=0.f;};
inline DialGeometry dialGeometry(float width,float height,bool kit)
{
    DialGeometry g;if(kit){const float s=std::min(width,height)/48.f;g.cx=width*.5f;g.cy=height*.5f;g.radius=std::max(1.f,std::min(20.f*s,g.cy-3.f*s));g.letter_radius=g.radius+6.f*s;}else{g.cx=width*.5f;g.cy=height*.5f;g.radius=std::max(4.f,std::min(width,height)*.5f-12.f);g.letter_radius=g.radius+6.f;}return g;
}

inline float arcAngle(float value, float minimum, float maximum)
{
    if (!(maximum > minimum)) return 0.f;
    if (minimum >= -90.f && maximum <= 90.f && minimum <= 0.f && maximum >= 0.f)
        return clamp(value, -90.f, 90.f);
    return -90.f + 180.f * clamp((value - minimum) / (maximum - minimum), 0.f, 1.f);
}

inline float arcValue(float angle, float minimum, float maximum)
{
    if (!(maximum > minimum)) return minimum;
    if (minimum >= -90.f && maximum <= 90.f && minimum <= 0.f && maximum >= 0.f)
        return clamp(angle, minimum, maximum);
    return minimum + (maximum - minimum) * clamp((angle + 90.f) / 180.f, 0.f, 1.f);
}

inline float pDisp(float base, float orbit, float offset_z, float radius)
{
    const float q = clamp(base + orbit, -85.f, 85.f) * DEG_TO_RAD;
    return std::atan2(radius * std::sin(q) + offset_z, radius * std::cos(q)) * RAD_TO_DEG;
}

inline float angularDistance(float a, float b) { return std::fabs(wrap180(a - b)); }

inline float baseFromDisplayPitch(float target, float orbit, float offset_z,
                                  float radius, float current)
{
    if (radius <= 1e-6f) return clamp(current, -85.f - orbit, 85.f - orbit);
    const float t = target * DEG_TO_RAD;
    const float rhs = clamp(-offset_z * std::cos(t) / radius, -1.f, 1.f);
    const float asin_rhs = std::asin(rhs);
    const float candidates[] = {
        (t + asin_rhs) * RAD_TO_DEG - orbit,
        (t + PI - asin_rhs) * RAD_TO_DEG - orbit,
        (t + asin_rhs - 2.f * PI) * RAD_TO_DEG - orbit,
        (t + PI - asin_rhs - 2.f * PI) * RAD_TO_DEG - orbit
    };
    float best = candidates[0];
    float distance = angularDistance(best, current);
    for (float candidate : candidates)
    {
        const float d = angularDistance(candidate, current);
        if (d < distance) { best = candidate; distance = d; }
    }
    return clamp(best, -85.f - orbit, 85.f - orbit);
}

enum View { TOP = 0, SIDE = 1, PERSPECTIVE = 2 };

inline void projectLight(int view, int side_from, float yaw, float pitch,
                         float radius, float cx, float cy,
                         float* x, float* y, float* depth)
{
    const float t = yaw * DEG_TO_RAD;
    const float p = pitch * DEG_TO_RAD;
    const float X = radius * std::cos(p) * std::sin(t);
    const float Y = radius * std::sin(p);
    const float Z = radius * std::cos(p) * std::cos(t);
    if (view == SIDE)
    {
        const float s = side_from == 0 ? 1.f : -1.f;
        *x = cx + s * Z; *y = cy + Y; *depth = -s * X;
        return;
    }
    const float elevation = (view == TOP ? 90.f : 24.f) * DEG_TO_RAD;
    *x = cx + X;
    *y = cy - Z * std::sin(elevation) + Y * std::cos(elevation);
    *depth = Z * std::cos(elevation) + Y * std::sin(elevation);
}

inline float smoothstep(float lo, float hi, float x)
{
    x = clamp((x - lo) / (hi - lo), 0.f, 1.f);
    return x * x * (3.f - 2.f * x);
}

inline float frontFactor(int view, int side_from, float yaw)
{
    const float t = yaw * DEG_TO_RAD;
    if (view == SIDE)
    {
        const float s = side_from == 0 ? 1.f : -1.f;
        return smoothstep(-.2f, .2f, -s * std::sin(t));
    }
    return smoothstep(-.2f, .2f, std::cos(t));
}

inline std::string yawSideText(float camera_relative_yaw, float base_yaw)
{
    const int base = static_cast<int>(std::lround(std::fabs(wrap180(base_yaw))));
    const float side = wrap180(camera_relative_yaw);
    char text[24];
    if (base == 0 || base == 180) std::snprintf(text, sizeof(text), "%d\xC2\xB0", base);
    else std::snprintf(text, sizeof(text), "%c%d\xC2\xB0", side >= 0.f ? 'R' : 'L', base);
    return text;
}

inline float effYaw(float base, float orbit, bool mirror, float facing)
{
    // Matches ALCineLightRig::computeLive() in alcinelightrigmodel.cpp:1057-1059.
    return wrap180(mirror ? 2.f * facing - (base + orbit) : base + orbit);
}

struct DragStart
{
    float x0 = 0.f, y0 = 0.f, grab_x = 0.f, grab_y = 0.f;
    float cx = 0.f, cy = 0.f, radius = 1.f, canvas_radius = 1.f;
    float t0 = 0.f, p0 = 0.f, base_yaw0 = 0.f, base_pitch0 = 0.f;
    float orbit_pitch0 = 0.f, offset_z0 = 0.f, depth0 = 0.f;
    float orbit_yaw0 = 0.f, camera_bearing0 = 0.f;
    int view = PERSPECTIVE, side_from = 0;
    bool mirror = false, live = false;
    float sigma = 1.f;
};

struct DragResult { float yaw = 0.f, pitch = 0.f; };

inline DragResult finishDrag(const DragStart& s, float target_yaw, float target_pitch)
{
    return { wrap180(s.base_yaw0 + s.sigma * wrap180(target_yaw - s.t0)),
             target_pitch == s.p0 ? s.base_pitch0
                                  : baseFromDisplayPitch(target_pitch, s.orbit_pitch0, s.offset_z0,
                                                         s.radius, s.base_pitch0) };
}

inline DragResult dragTopAbsolute(const DragStart& s, float x, float y)
{
    if (x == s.x0 && y == s.y0) return {s.base_yaw0, s.base_pitch0};
    const float a0 = std::atan2(s.x0 - s.cx, -(s.y0 - s.cy));
    const float a = std::atan2(x - s.cx, -(y - s.cy));
    return finishDrag(s, wrap180(s.t0 + wrap180((a - a0) * RAD_TO_DEG)), s.p0);
}

inline bool unproject25D(const DragStart& s, float x, float y, float* yaw, float* pitch)
{
    float u = (x - s.grab_x - s.cx) / s.canvas_radius;
    float v = (y - s.grab_y - s.cy) / s.canvas_radius;
    const float length = std::sqrt(u*u + v*v);
    if (length > 1.f) { u /= length; v /= length; }
    const float e = 24.f * DEG_TO_RAD;
    const float mag = std::sqrt(std::max(0.f, 1.f - u*u - v*v));
    float best_t = s.t0, best_p = s.p0, best_d = 1e9f;
    for (float d : {mag, -mag})
    {
        const float X = u;
        const float Y = v * std::cos(e) + d * std::sin(e);
        const float Z = -v * std::sin(e) + d * std::cos(e);
        const float pt = std::asin(clamp(Y, -1.f, 1.f)) * RAD_TO_DEG;
        const float tt = std::atan2(X, Z) * RAD_TO_DEG;
        const float score = angularDistance(tt, s.t0) + std::fabs(pt - s.p0);
        if ((s.depth0 >= 0.f) == (d >= 0.f) || score < best_d)
        { best_t = tt; best_p = pt; best_d = score; if ((s.depth0 >= 0.f) == (d >= 0.f)) break; }
    }
    *yaw = best_t; *pitch = best_p; return true;
}

inline DragResult drag25DAbsolute(const DragStart& s, float x, float y)
{
    if (x == s.x0 && y == s.y0) return {s.base_yaw0, s.base_pitch0};
    float t, p; unproject25D(s, x, y, &t, &p); return finishDrag(s, t, p);
}

inline DragResult dragSideAbsolute(const DragStart& s, float x, float y)
{
    if (x == s.x0 && y == s.y0) return {s.base_yaw0, s.base_pitch0};
    const float p = std::asin(clamp((y - s.grab_y - s.cy) / s.canvas_radius, -1.f, 1.f));
    float target = s.t0;
    if (std::fabs(std::cos(p)) >= 1e-4f)
    {
        const float side = s.side_from == 0 ? 1.f : -1.f;
        const float c = clamp((x - s.grab_x - s.cx) / (side * s.canvas_radius * std::cos(p)), -1.f, 1.f);
        const float a = std::acos(c) * RAD_TO_DEG;
        const float candidates[] = {a, -a};
        float best_score = 1e9f;
        for (float candidate : candidates)
        {
            const float candidate_depth = -side * std::cos(p) * std::sin(candidate * DEG_TO_RAD);
            const float depth_penalty = ((candidate_depth >= 0.f) == (s.depth0 >= 0.f)) ? 0.f : 360.f;
            const float score = depth_penalty + angularDistance(candidate, s.t0);
            if (score < best_score) { best_score = score; target = candidate; }
        }
    }
    return finishDrag(s, target, p * RAD_TO_DEG);
}

struct ShellLayout { int rail = 56, help = 0, main = 0, columns = 1, track = 0; bool rail_labels = false, help_panel = false; };
inline ShellLayout computeShellLayout(int width, int, const ShellLayout& previous = {})
{
    ShellLayout out;
    out.rail_labels = previous.rail_labels ? width >= 916 : width >= 924;
    out.help_panel = previous.help_panel ? width >= 1142 : width >= 1150;
    out.rail = out.rail_labels ? 176 : 56;
    out.help = out.help_panel ? 224 : 0;
    out.main = std::max(0, width - out.rail - out.help);
    const int content = std::max(0, out.main - 32);
    out.columns = (content - 12) / 2 >= 340 ? 2 : 1;
    const int card = out.columns == 2 ? (content - 12) / 2 : content;
    // Row insets, card padding, scrollbar reservation, label/value/reset and three gaps.
    out.track = card - 8 - 20 - 16 - 100 - 52 - 20 - 18;
    return out;
}

struct PackedItem { int x = 0, y = 0, width = 0, height = 0; bool hidden = false, spanning = false; };
inline std::vector<PackedItem> packRows(const std::vector<int>& widths,
                                        const std::vector<int>& heights, int columns)
{
    std::vector<PackedItem> result;
    const int count = static_cast<int>(std::min(widths.size(), heights.size()));
    int column = 0, y = 0, row_height = 0;
    for (int i = 0; i < count; ++i)
    {
        PackedItem item; item.width = widths[i]; item.height = heights[i]; item.hidden = widths[i] < 0;
        item.spanning = widths[i] == 0;
        if (item.hidden) { result.push_back(item); continue; }
        if (item.spanning || column == columns) { y += row_height; column = 0; row_height = 0; }
        item.x = item.spanning ? 0 : column; item.y = y;
        result.push_back(item);
        if (item.spanning) { y += item.height; column = 0; }
        else { row_height = std::max(row_height, item.height); ++column; }
    }
    return result;
}
} // namespace ALKitMath

#endif
