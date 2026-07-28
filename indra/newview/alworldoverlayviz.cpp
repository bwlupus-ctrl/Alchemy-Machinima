/**
 * @file alworldoverlayviz.cpp
 * @brief Screen-readable, untextured world-overlay drawing helpers.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alworldoverlayviz.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

#include "llgl.h"
#include "llglslshader.h"
#include "llrender.h"
#include "llviewercamera.h"
#include "llviewershadermgr.h"

namespace
{

constexpr F32 MIN_VECTOR_LENGTH       = 1e-5f;
constexpr F32 MIN_PROJECTED_LENGTH    = 1e-4f;
constexpr F32 MIN_STROKE_PIXELS       = 0.25f;
constexpr F32 MAX_STROKE_PIXELS       = 128.f;
constexpr F32 MAX_OUTLINE_PIXELS      = 32.f;
constexpr F32 MIN_LABEL_HEIGHT_PIXELS = 7.f;
constexpr F32 MAX_LABEL_HEIGHT_PIXELS = 256.f;
constexpr S32 MIN_CIRCLE_SEGMENTS     = 8;
constexpr S32 MAX_CIRCLE_SEGMENTS     = 256;
constexpr S32 ROUND_CAP_SEGMENTS      = 12;
constexpr S32 MAX_IMMEDIATE_VERTICES  = 4000;
constexpr size_t MAX_LABEL_CHARACTERS = 16;

bool colorIsFinite(const LLColor4& color)
{
    return llfinite(color.mV[VRED])
        && llfinite(color.mV[VGREEN])
        && llfinite(color.mV[VBLUE])
        && llfinite(color.mV[VALPHA]);
}

bool strokeIsValid(const ALWorldOverlayViz::StrokeStyle& style)
{
    return colorIsFinite(style.mColor)
        && colorIsFinite(style.mOutlineColor)
        && llfinite(style.mWidthPixels)
        && llfinite(style.mOutlinePixels)
        && style.mWidthPixels > 0.f
        && style.mOutlinePixels >= 0.f;
}

bool fillIsValid(const ALWorldOverlayViz::FillStyle& style)
{
    return colorIsFinite(style.mFillColor)
        && colorIsFinite(style.mOutlineColor)
        && llfinite(style.mOutlinePixels)
        && style.mOutlinePixels >= 0.f;
}

bool pointsAreFinite(const std::vector<LLVector3>& points)
{
    for (const LLVector3& point : points)
    {
        if (!point.isFinite())
        {
            return false;
        }
    }
    return true;
}

S32 safeSegments(S32 segments)
{
    return llclamp(segments, MIN_CIRCLE_SEGMENTS, MAX_CIRCLE_SEGMENTS);
}

F32 safeStrokePixels(F32 pixels)
{
    return llclamp(pixels, MIN_STROKE_PIXELS, MAX_STROKE_PIXELS);
}

F32 safeOutlinePixels(F32 pixels)
{
    return llclamp(pixels, 0.f, MAX_OUTLINE_PIXELS);
}

void emitTriangle(const LLVector3& a, const LLVector3& b, const LLVector3& c)
{
    gGL.vertex3fv(a.mV);
    gGL.vertex3fv(b.mV);
    gGL.vertex3fv(c.mV);
}

void emitQuad(const LLVector3& a,
              const LLVector3& b,
              const LLVector3& c,
              const LLVector3& d)
{
    emitTriangle(a, b, c);
    emitTriangle(a, c, d);
}

/**
 * Return the world vectors corresponding to one display pixel at position.
 * Points behind or effectively on the camera plane are deliberately rejected:
 * LLViewerCamera::getPixelVectors() returns signed/degenerate scales there.
 */
bool screenPixelBasis(const LLVector3& position_agent,
                      LLVector3& pixel_up,
                      LLVector3& pixel_right)
{
    if (!position_agent.isFinite())
    {
        return false;
    }

    LLViewerCamera* camera = LLViewerCamera::getInstance();
    if (!camera)
    {
        return false;
    }

    const LLVector3 to_position = position_agent - camera->getOrigin();
    const F32 forward_distance = to_position * camera->getAtAxis();
    if (!llfinite(forward_distance) || forward_distance <= MIN_VECTOR_LENGTH)
    {
        return false;
    }

    camera->getPixelVectors(position_agent, pixel_up, pixel_right);
    return pixel_up.isFinite()
        && pixel_right.isFinite()
        && pixel_up.lengthSquared() > MIN_VECTOR_LENGTH * MIN_VECTOR_LENGTH
        && pixel_right.lengthSquared() > MIN_VECTOR_LENGTH * MIN_VECTOR_LENGTH;
}

/**
 * Compute a one-screen-pixel normal to world_direction at position. The result
 * is camera-facing rather than constrained to a terrain plane; this is what
 * keeps a world guide readable at grazing camera angles.
 */
bool screenPerpendicular(const LLVector3& position_agent,
                         const LLVector3& world_direction,
                         LLVector3& one_pixel_perpendicular,
                         LLVector3* one_pixel_along = nullptr,
                         F32* projected_pixels_per_metre = nullptr)
{
    LLVector3 pixel_up;
    LLVector3 pixel_right;
    if (!world_direction.isFinite()
        || !screenPixelBasis(position_agent, pixel_up, pixel_right))
    {
        return false;
    }

    const F32 up_scale = pixel_up.length();
    const F32 right_scale = pixel_right.length();
    if (!llfinite(up_scale) || !llfinite(right_scale)
        || up_scale <= MIN_VECTOR_LENGTH || right_scale <= MIN_VECTOR_LENGTH)
    {
        return false;
    }

    const LLVector3 up_axis = pixel_up * (1.f / up_scale);
    const LLVector3 right_axis = pixel_right * (1.f / right_scale);
    const F32 dx_pixels = (world_direction * right_axis) / right_scale;
    const F32 dy_pixels = (world_direction * up_axis) / up_scale;
    const F32 projected = sqrtf(dx_pixels * dx_pixels + dy_pixels * dy_pixels);
    if (!llfinite(projected) || projected <= MIN_PROJECTED_LENGTH)
    {
        return false;
    }

    const F32 tx = dx_pixels / projected;
    const F32 ty = dy_pixels / projected;
    one_pixel_perpendicular = pixel_right * (-ty) + pixel_up * tx;
    if (!one_pixel_perpendicular.isFinite())
    {
        return false;
    }

    if (one_pixel_along)
    {
        *one_pixel_along = pixel_right * tx + pixel_up * ty;
        if (!one_pixel_along->isFinite())
        {
            return false;
        }
    }
    if (projected_pixels_per_metre)
    {
        *projected_pixels_per_metre = projected;
    }
    return true;
}

bool screenMiter(const LLVector3& position_agent,
                 const LLVector3& previous_direction,
                 const LLVector3& next_direction,
                 F32 half_width_pixels,
                 LLVector3& miter_offset)
{
    LLVector3 previous_normal;
    LLVector3 next_normal;
    if (!screenPerpendicular(position_agent, previous_direction, previous_normal)
        || !screenPerpendicular(position_agent, next_direction, next_normal))
    {
        return false;
    }

    LLVector3 pixel_up;
    LLVector3 pixel_right;
    if (!screenPixelBasis(position_agent, pixel_up, pixel_right))
    {
        return false;
    }
    const F32 up_scale = pixel_up.length();
    const F32 right_scale = pixel_right.length();
    if (up_scale <= MIN_VECTOR_LENGTH || right_scale <= MIN_VECTOR_LENGTH)
    {
        return false;
    }
    const LLVector3 up_axis = pixel_up * (1.f / up_scale);
    const LLVector3 right_axis = pixel_right * (1.f / right_scale);

    F32 p_x = (previous_normal * right_axis) / right_scale;
    F32 p_y = (previous_normal * up_axis) / up_scale;
    F32 n_x = (next_normal * right_axis) / right_scale;
    F32 n_y = (next_normal * up_axis) / up_scale;
    const F32 p_length = sqrtf(p_x * p_x + p_y * p_y);
    const F32 n_length = sqrtf(n_x * n_x + n_y * n_y);
    if (!llfinite(p_length) || !llfinite(n_length)
        || p_length <= MIN_PROJECTED_LENGTH
        || n_length <= MIN_PROJECTED_LENGTH)
    {
        return false;
    }
    p_x /= p_length;
    p_y /= p_length;
    n_x /= n_length;
    n_y /= n_length;

    F32 miter_x = p_x + n_x;
    F32 miter_y = p_y + n_y;
    const F32 miter_length = sqrtf(miter_x * miter_x + miter_y * miter_y);
    if (!llfinite(miter_length) || miter_length <= 0.2f)
    {
        // A near reversal has no useful miter. A bevel based on the outgoing
        // segment stays bounded and avoids a many-metre spike.
        miter_offset = next_normal * half_width_pixels;
        return miter_offset.isFinite();
    }
    miter_x /= miter_length;
    miter_y /= miter_length;

    const F32 denominator = fabsf(miter_x * n_x + miter_y * n_y);
    const F32 miter_pixels = llmin(half_width_pixels * 3.f,
                                   half_width_pixels / llmax(denominator, 0.25f));
    miter_offset = (pixel_right * miter_x + pixel_up * miter_y) * miter_pixels;
    return miter_offset.isFinite();
}

bool planeBasis(const LLVector3& plane_normal, LLVector3& axis_x, LLVector3& axis_y)
{
    if (!plane_normal.isFinite())
    {
        return false;
    }

    LLVector3 normal = plane_normal;
    const F32 normal_length = normal.length();
    if (!llfinite(normal_length) || normal_length <= MIN_VECTOR_LENGTH)
    {
        return false;
    }
    normal *= 1.f / normal_length;

    const LLVector3 reference = fabsf(normal.mV[VZ]) < 0.9f
        ? LLVector3::z_axis
        : LLVector3::x_axis;
    axis_x = reference % normal;
    const F32 x_length = axis_x.length();
    if (!llfinite(x_length) || x_length <= MIN_VECTOR_LENGTH)
    {
        return false;
    }
    axis_x *= 1.f / x_length;

    axis_y = normal % axis_x;
    const F32 y_length = axis_y.length();
    if (!llfinite(y_length) || y_length <= MIN_VECTOR_LENGTH)
    {
        return false;
    }
    axis_y *= 1.f / y_length;
    return axis_x.isFinite() && axis_y.isFinite();
}

bool buildCircle(const LLVector3& center_agent,
                 const LLVector3& plane_normal,
                 F32 radius_metres,
                 S32 segments,
                 std::vector<LLVector3>& points)
{
    if (!center_agent.isFinite()
        || !llfinite(radius_metres)
        || radius_metres <= 0.f)
    {
        return false;
    }

    LLVector3 axis_x;
    LLVector3 axis_y;
    if (!planeBasis(plane_normal, axis_x, axis_y))
    {
        return false;
    }

    const S32 count = safeSegments(segments);
    points.clear();
    points.reserve(count);
    for (S32 i = 0; i < count; ++i)
    {
        const F32 angle = F_TWO_PI * static_cast<F32>(i) / static_cast<F32>(count);
        const LLVector3 point = center_agent
            + axis_x * (cosf(angle) * radius_metres)
            + axis_y * (sinf(angle) * radius_metres);
        if (!point.isFinite())
        {
            points.clear();
            return false;
        }
        points.push_back(point);
    }
    return true;
}

S32 emitScreenDisc(const LLVector3& center_agent, F32 radius_pixels)
{
    LLVector3 pixel_up;
    LLVector3 pixel_right;
    if (!screenPixelBasis(center_agent, pixel_up, pixel_right))
    {
        return 0;
    }

    const LLVector3 up = pixel_up * radius_pixels;
    const LLVector3 right = pixel_right * radius_pixels;
    S32 emitted_vertices = 0;
    for (S32 i = 0; i < ROUND_CAP_SEGMENTS; ++i)
    {
        const F32 a0 = F_TWO_PI * static_cast<F32>(i)
                     / static_cast<F32>(ROUND_CAP_SEGMENTS);
        const F32 a1 = F_TWO_PI * static_cast<F32>(i + 1)
                     / static_cast<F32>(ROUND_CAP_SEGMENTS);
        const LLVector3 p0 = center_agent + right * cosf(a0) + up * sinf(a0);
        const LLVector3 p1 = center_agent + right * cosf(a1) + up * sinf(a1);
        if (p0.isFinite() && p1.isFinite())
        {
            emitTriangle(center_agent, p0, p1);
            emitted_vertices += 3;
        }
    }
    return emitted_vertices;
}

bool drawRibbonPass(const std::vector<LLVector3>& points,
                    F32 width_pixels,
                    const LLColor4& color,
                    bool closed)
{
    const size_t point_count = points.size();
    const size_t segment_count = closed ? point_count : point_count - 1;
    const F32 half_width = width_pixels * 0.5f;
    std::vector<LLVector3> offsets(point_count);
    std::vector<bool> valid_offsets(point_count, false);

    for (size_t i = 0; i < point_count; ++i)
    {
        if (!closed && i == 0)
        {
            valid_offsets[i] = screenPerpendicular(points[i],
                                                   points[1] - points[0],
                                                   offsets[i]);
            offsets[i] *= half_width;
        }
        else if (!closed && i + 1 == point_count)
        {
            valid_offsets[i] = screenPerpendicular(points[i],
                                                   points[i] - points[i - 1],
                                                   offsets[i]);
            offsets[i] *= half_width;
        }
        else
        {
            const size_t previous = (i + point_count - 1) % point_count;
            const size_t next = (i + 1) % point_count;
            valid_offsets[i] = screenMiter(points[i],
                                           points[i] - points[previous],
                                           points[next] - points[i],
                                           half_width,
                                           offsets[i]);
        }
        valid_offsets[i] = valid_offsets[i] && offsets[i].isFinite();
    }

    bool emitted = false;
    S32 batch_vertices = 0;

    gGL.begin(LLRender::TRIANGLES);
    gGL.color4fv(color.mV);
    for (size_t i = 0; i < segment_count; ++i)
    {
        const LLVector3& a = points[i];
        const LLVector3& b = points[(i + 1) % point_count];
        const LLVector3 direction = b - a;
        if (!direction.isFinite()
            || direction.lengthSquared() <= MIN_VECTOR_LENGTH * MIN_VECTOR_LENGTH)
        {
            continue;
        }
        const size_t next = (i + 1) % point_count;
        if (!valid_offsets[i] || !valid_offsets[next])
        {
            continue;
        }

        const LLVector3 a0 = a - offsets[i];
        const LLVector3 a1 = a + offsets[i];
        const LLVector3 b0 = b - offsets[next];
        const LLVector3 b1 = b + offsets[next];
        if (a0.isFinite() && a1.isFinite() && b0.isFinite() && b1.isFinite())
        {
            if (batch_vertices + 6 > MAX_IMMEDIATE_VERTICES)
            {
                gGL.end();
                gGL.begin(LLRender::TRIANGLES);
                gGL.color4fv(color.mV);
                batch_vertices = 0;
            }
            emitQuad(a0, b0, b1, a1);
            batch_vertices += 6;
            emitted = true;
        }
    }

    // Only open endpoints need caps. Interior joins share the same bounded
    // miter vertices, avoiding alpha overdraw and dark beads on smooth rings.
    if (!closed)
    {
        for (const LLVector3* point : { &points.front(), &points.back() })
        {
            if (batch_vertices + ROUND_CAP_SEGMENTS * 3 > MAX_IMMEDIATE_VERTICES)
            {
                gGL.end();
                gGL.begin(LLRender::TRIANGLES);
                gGL.color4fv(color.mV);
                batch_vertices = 0;
            }
            const S32 cap_vertices = emitScreenDisc(*point, half_width);
            batch_vertices += cap_vertices;
            emitted = emitted || cap_vertices > 0;
        }
    }
    gGL.end();
    return emitted;
}

void drawTrianglePass(const LLVector3& tip,
                      const LLVector3& base_left,
                      const LLVector3& base_right,
                      const LLColor4& color)
{
    gGL.begin(LLRender::TRIANGLES);
    gGL.color4fv(color.mV);
    emitTriangle(tip, base_left, base_right);
    gGL.end();
}

struct Glyph
{
    U8 mRows[7];
};

const Glyph& glyphFor(char character)
{
    static const Glyph unknown = {{ 0x0e, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04 }};
    static const Glyph space   = {{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }};
    static const Glyph dash    = {{ 0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00 }};
    static const Glyph under   = {{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f }};
    static const Glyph dot     = {{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c }};
    static const Glyph colon   = {{ 0x00, 0x0c, 0x0c, 0x00, 0x0c, 0x0c, 0x00 }};
    static const Glyph slash   = {{ 0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10 }};
    static const Glyph plus    = {{ 0x00, 0x04, 0x04, 0x1f, 0x04, 0x04, 0x00 }};

    static const Glyph digits[] =
    {
        {{ 0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e }}, // 0
        {{ 0x04, 0x0c, 0x14, 0x04, 0x04, 0x04, 0x1f }}, // 1
        {{ 0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f }}, // 2
        {{ 0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e }}, // 3
        {{ 0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02 }}, // 4
        {{ 0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e }}, // 5
        {{ 0x0e, 0x10, 0x10, 0x1e, 0x11, 0x11, 0x0e }}, // 6
        {{ 0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 }}, // 7
        {{ 0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e }}, // 8
        {{ 0x0e, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x0e }}  // 9
    };

    static const Glyph letters[] =
    {
        {{ 0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 }}, // A
        {{ 0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e }}, // B
        {{ 0x0f, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0f }}, // C
        {{ 0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e }}, // D
        {{ 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f }}, // E
        {{ 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10 }}, // F
        {{ 0x0f, 0x10, 0x10, 0x17, 0x11, 0x11, 0x0f }}, // G
        {{ 0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 }}, // H
        {{ 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1f }}, // I
        {{ 0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0e }}, // J
        {{ 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 }}, // K
        {{ 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f }}, // L
        {{ 0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11 }}, // M
        {{ 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 }}, // N
        {{ 0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e }}, // O
        {{ 0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10 }}, // P
        {{ 0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d }}, // Q
        {{ 0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11 }}, // R
        {{ 0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e }}, // S
        {{ 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 }}, // T
        {{ 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e }}, // U
        {{ 0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04 }}, // V
        {{ 0x11, 0x11, 0x11, 0x15, 0x15, 0x1b, 0x11 }}, // W
        {{ 0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11 }}, // X
        {{ 0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04 }}, // Y
        {{ 0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f }}  // Z
    };

    const unsigned char raw = static_cast<unsigned char>(character);
    const char upper = static_cast<char>(std::toupper(raw));
    if (upper >= '0' && upper <= '9')
    {
        return digits[upper - '0'];
    }
    if (upper >= 'A' && upper <= 'Z')
    {
        return letters[upper - 'A'];
    }
    switch (upper)
    {
        case ' ': return space;
        case '-': return dash;
        case '_': return under;
        case '.': return dot;
        case ':': return colon;
        case '/': return slash;
        case '+': return plus;
        case '?': return unknown;
        default:  return unknown;
    }
}

bool glyphHasPixels(const Glyph& glyph)
{
    for (U8 row : glyph.mRows)
    {
        if (row != 0)
        {
            return true;
        }
    }
    return false;
}

bool drawGlyphPass(const std::string& label,
                   const LLVector3& lower_center_agent,
                   const LLVector3& pixel_up,
                   const LLVector3& pixel_right,
                   F32 cell_pixels,
                   F32 expand_pixels,
                   const LLColor4& color)
{
    constexpr F32 GLYPH_WIDTH_CELLS = 5.f;
    constexpr F32 GLYPH_HEIGHT_CELLS = 7.f;
    constexpr F32 GLYPH_ADVANCE_CELLS = 6.f;

    if (label.empty())
    {
        return false;
    }

    const F32 total_width_cells = GLYPH_WIDTH_CELLS * static_cast<F32>(label.size())
        + static_cast<F32>(label.size() - 1);
    const LLVector3 lower_left = lower_center_agent
        - pixel_right * (total_width_cells * cell_pixels * 0.5f);
    bool emitted = false;

    gGL.begin(LLRender::TRIANGLES);
    gGL.color4fv(color.mV);
    for (size_t character_index = 0; character_index < label.size(); ++character_index)
    {
        const Glyph& glyph = glyphFor(label[character_index]);
        if (!glyphHasPixels(glyph))
        {
            continue;
        }

        const F32 glyph_left_cells = static_cast<F32>(character_index)
                                   * GLYPH_ADVANCE_CELLS;
        for (S32 row = 0; row < static_cast<S32>(GLYPH_HEIGHT_CELLS); ++row)
        {
            for (S32 column = 0; column < static_cast<S32>(GLYPH_WIDTH_CELLS); ++column)
            {
                if (!(glyph.mRows[row] & (1 << (4 - column))))
                {
                    continue;
                }

                const F32 left = (glyph_left_cells + static_cast<F32>(column))
                               * cell_pixels - expand_pixels;
                const F32 right = (glyph_left_cells + static_cast<F32>(column + 1))
                                * cell_pixels + expand_pixels;
                const F32 bottom = static_cast<F32>(6 - row)
                                 * cell_pixels - expand_pixels;
                const F32 top = static_cast<F32>(7 - row)
                              * cell_pixels + expand_pixels;
                const LLVector3 bl = lower_left + pixel_right * left + pixel_up * bottom;
                const LLVector3 br = lower_left + pixel_right * right + pixel_up * bottom;
                const LLVector3 tr = lower_left + pixel_right * right + pixel_up * top;
                const LLVector3 tl = lower_left + pixel_right * left + pixel_up * top;
                if (bl.isFinite() && br.isFinite() && tr.isFinite() && tl.isFinite())
                {
                    emitQuad(bl, br, tr, tl);
                    emitted = true;
                }
            }
        }
    }
    gGL.end();
    return emitted;
}

} // anonymous namespace

namespace ALWorldOverlayViz
{

struct ScopedRenderer::State
{
    State()
        : mSavedShader(LLGLSLShader::sCurBoundShaderPtr),
          mSavedActiveTextureUnit(gGL.getCurrentTexUnitIndex()),
          mSavedTexture(0),
          mSavedTextureType(LLTexUnit::TT_NONE),
          mUIState(nullptr),
          mReady(false)
    {
        if (!gUIProgram.isComplete())
        {
            return;
        }

        LLTexUnit* texture_unit = gGL.getTexUnit(0);
        mSavedTexture = texture_unit->getCurrTexture();
        mSavedTextureType = texture_unit->getCurrType();

        gGL.flush();
        mUIState = new LLGLSUIDefault();
        gUIProgram.bind();
        const LLTexUnit::eTextureType current_type = texture_unit->getCurrType();
        if (current_type != LLTexUnit::TT_NONE)
        {
            texture_unit->unbind(current_type);
        }
        gGL.color4f(1.f, 1.f, 1.f, 1.f);
        mReady = true;
    }

    ~State()
    {
        if (!mReady)
        {
            delete mUIState;
            return;
        }

        gGL.flush();
        gGL.color4f(1.f, 1.f, 1.f, 1.f);

        // Restore the tracked fixed-function states before returning ownership
        // of shader/texture selection to the enclosing render pass.
        delete mUIState;
        mUIState = nullptr;

        LLTexUnit* texture_unit = gGL.getTexUnit(0);
        if (mSavedTextureType != LLTexUnit::TT_NONE && mSavedTexture != 0)
        {
            texture_unit->bindManual(mSavedTextureType, mSavedTexture);
        }
        else
        {
            const LLTexUnit::eTextureType current_type = texture_unit->getCurrType();
            if (current_type != LLTexUnit::TT_NONE)
            {
                texture_unit->unbind(current_type);
            }
        }

        if (mSavedActiveTextureUnit < LL_NUM_TEXTURE_LAYERS)
        {
            gGL.getTexUnit(mSavedActiveTextureUnit)->activate();
        }

        if (mSavedShader && mSavedShader->isComplete())
        {
            mSavedShader->bind();
        }
        else
        {
            gUIProgram.unbind();
        }
    }

    LLGLSLShader*           mSavedShader;
    U32                     mSavedActiveTextureUnit;
    U32                     mSavedTexture;
    LLTexUnit::eTextureType mSavedTextureType;
    LLGLSUIDefault*         mUIState;
    bool                    mReady;
};

ScopedRenderer::ScopedRenderer()
    : mState(new State())
{
}

ScopedRenderer::~ScopedRenderer()
{
    delete mState;
    mState = nullptr;
}

bool ScopedRenderer::isReady() const
{
    return mState && mState->mReady;
}

bool ScopedRenderer::drawSegment(const LLVector3& start_agent,
                                 const LLVector3& end_agent,
                                 const StrokeStyle& style) const
{
    if (!start_agent.isFinite() || !end_agent.isFinite())
    {
        return false;
    }
    const std::vector<LLVector3> points{ start_agent, end_agent };
    return drawPolyline(points, style, false);
}

bool ScopedRenderer::drawPolyline(const std::vector<LLVector3>& points_agent,
                                  const StrokeStyle& style,
                                  bool closed) const
{
    const size_t minimum_points = closed ? 3 : 2;
    if (!isReady()
        || points_agent.size() < minimum_points
        || !pointsAreFinite(points_agent)
        || !strokeIsValid(style))
    {
        return false;
    }

    // Consecutive duplicate samples have no screen direction and would poison
    // both adjoining mitres. Remove them without changing the authored path.
    std::vector<LLVector3> clean_points;
    clean_points.reserve(points_agent.size());
    for (const LLVector3& point : points_agent)
    {
        if (clean_points.empty()
            || (point - clean_points.back()).lengthSquared()
                > MIN_VECTOR_LENGTH * MIN_VECTOR_LENGTH)
        {
            clean_points.push_back(point);
        }
    }
    if (closed
        && clean_points.size() > 1
        && (clean_points.front() - clean_points.back()).lengthSquared()
            <= MIN_VECTOR_LENGTH * MIN_VECTOR_LENGTH)
    {
        clean_points.pop_back();
    }
    if (clean_points.size() < minimum_points)
    {
        return false;
    }

    const F32 width = safeStrokePixels(style.mWidthPixels);
    const F32 outline = safeOutlinePixels(style.mOutlinePixels);
    bool emitted = false;
    if (outline > 0.f && style.mOutlineColor.mV[VALPHA] > 0.f)
    {
        emitted = drawRibbonPass(clean_points,
                                 width + outline * 2.f,
                                 style.mOutlineColor,
                                 closed);
    }
    if (style.mColor.mV[VALPHA] > 0.f)
    {
        emitted = drawRibbonPass(clean_points, width, style.mColor, closed) || emitted;
    }
    return emitted;
}

bool ScopedRenderer::drawRing(const LLVector3& center_agent,
                              const LLVector3& plane_normal,
                              F32 radius_metres,
                              const StrokeStyle& style,
                              S32 segments) const
{
    if (!isReady() || !strokeIsValid(style))
    {
        return false;
    }

    std::vector<LLVector3> points;
    if (!buildCircle(center_agent, plane_normal, radius_metres, segments, points))
    {
        return false;
    }
    return drawPolyline(points, style, true);
}

bool ScopedRenderer::drawAnnulus(const LLVector3& center_agent,
                                 const LLVector3& plane_normal,
                                 F32 inner_radius_metres,
                                 F32 outer_radius_metres,
                                 const FillStyle& style,
                                 S32 segments) const
{
    if (!isReady()
        || !center_agent.isFinite()
        || !llfinite(inner_radius_metres)
        || !llfinite(outer_radius_metres)
        || inner_radius_metres < 0.f
        || outer_radius_metres <= inner_radius_metres
        || !fillIsValid(style))
    {
        return false;
    }

    std::vector<LLVector3> outer;
    if (!buildCircle(center_agent, plane_normal, outer_radius_metres, segments, outer))
    {
        return false;
    }

    std::vector<LLVector3> inner;
    if (inner_radius_metres > MIN_VECTOR_LENGTH
        && !buildCircle(center_agent, plane_normal, inner_radius_metres,
                        static_cast<S32>(outer.size()), inner))
    {
        return false;
    }

    bool emitted = false;
    if (style.mFillColor.mV[VALPHA] > 0.f)
    {
        gGL.begin(LLRender::TRIANGLES);
        gGL.color4fv(style.mFillColor.mV);
        for (size_t i = 0; i < outer.size(); ++i)
        {
            const size_t next = (i + 1) % outer.size();
            if (inner.empty())
            {
                emitTriangle(center_agent, outer[i], outer[next]);
            }
            else
            {
                emitQuad(inner[i], outer[i], outer[next], inner[next]);
            }
            emitted = true;
        }
        gGL.end();
    }

    const F32 outline = safeOutlinePixels(style.mOutlinePixels);
    if (outline > 0.f && style.mOutlineColor.mV[VALPHA] > 0.f)
    {
        const StrokeStyle boundary(style.mOutlineColor,
                                   outline,
                                   LLColor4(0.f, 0.f, 0.f, 0.f),
                                   0.f);
        emitted = drawPolyline(outer, boundary, true) || emitted;
        if (!inner.empty())
        {
            emitted = drawPolyline(inner, boundary, true) || emitted;
        }
    }
    return emitted;
}

bool ScopedRenderer::drawFootprintDisc(const LLVector3& center_agent,
                                       const LLVector3& plane_normal,
                                       F32 radius_metres,
                                       const FillStyle& style,
                                       S32 segments) const
{
    return drawAnnulus(center_agent, plane_normal, 0.f, radius_metres, style, segments);
}

bool ScopedRenderer::drawFacingArrow(const LLVector3& origin_agent,
                                     const LLVector3& direction,
                                     F32 length_metres,
                                     const StrokeStyle& style,
                                     F32 head_length_pixels,
                                     F32 head_width_pixels) const
{
    if (!isReady()
        || !origin_agent.isFinite()
        || !direction.isFinite()
        || !llfinite(length_metres)
        || !llfinite(head_length_pixels)
        || !llfinite(head_width_pixels)
        || length_metres <= 0.f
        || head_length_pixels <= 0.f
        || head_width_pixels <= 0.f
        || !strokeIsValid(style))
    {
        return false;
    }

    LLVector3 unit_direction = direction;
    const F32 direction_length = unit_direction.length();
    if (!llfinite(direction_length) || direction_length <= MIN_VECTOR_LENGTH)
    {
        return false;
    }
    unit_direction *= 1.f / direction_length;

    const LLVector3 tip = origin_agent + unit_direction * length_metres;
    if (!tip.isFinite())
    {
        return false;
    }

    LLVector3 one_pixel_perp;
    LLVector3 one_pixel_along;
    F32 projected_pixels_per_metre = 0.f;
    if (!screenPerpendicular(tip,
                             unit_direction,
                             one_pixel_perp,
                             &one_pixel_along,
                             &projected_pixels_per_metre))
    {
        return false;
    }

    const F32 safe_head_length = llclamp(head_length_pixels, 2.f, 128.f);
    const F32 safe_head_width = llclamp(head_width_pixels, 2.f, 128.f);
    const F32 head_metres = llmin(length_metres * 0.85f,
                                  safe_head_length / projected_pixels_per_metre);
    const LLVector3 base = tip - unit_direction * head_metres;
    const LLVector3 half_head = one_pixel_perp * (safe_head_width * 0.5f);
    if (!base.isFinite() || !half_head.isFinite())
    {
        return false;
    }

    bool emitted = drawSegment(origin_agent, base, style);
    const F32 outline = safeOutlinePixels(style.mOutlinePixels);
    if (outline > 0.f && style.mOutlineColor.mV[VALPHA] > 0.f)
    {
        const LLVector3 outer_tip = tip + one_pixel_along * outline;
        const LLVector3 outer_base = base - one_pixel_along * outline;
        const LLVector3 outer_half_head = one_pixel_perp
                                       * (safe_head_width * 0.5f + outline);
        if (outer_tip.isFinite() && outer_base.isFinite() && outer_half_head.isFinite())
        {
            drawTrianglePass(outer_tip,
                             outer_base + outer_half_head,
                             outer_base - outer_half_head,
                             style.mOutlineColor);
            emitted = true;
        }
    }
    if (style.mColor.mV[VALPHA] > 0.f)
    {
        drawTrianglePass(tip, base + half_head, base - half_head, style.mColor);
        emitted = true;
    }
    return emitted;
}

bool ScopedRenderer::drawBillboardNumber(S32 value,
                                         const LLVector3& lower_center_agent,
                                         F32 height_pixels,
                                         const FillStyle& style) const
{
    return drawBillboardLabel(std::to_string(value),
                              lower_center_agent,
                              height_pixels,
                              style);
}

bool ScopedRenderer::drawBillboardLabel(const std::string& label,
                                        const LLVector3& lower_center_agent,
                                        F32 height_pixels,
                                        const FillStyle& style) const
{
    if (!isReady()
        || label.empty()
        || !lower_center_agent.isFinite()
        || !llfinite(height_pixels)
        || height_pixels <= 0.f
        || !fillIsValid(style))
    {
        return false;
    }

    LLVector3 pixel_up;
    LLVector3 pixel_right;
    if (!screenPixelBasis(lower_center_agent, pixel_up, pixel_right))
    {
        return false;
    }

    const std::string visible_label = label.substr(0, MAX_LABEL_CHARACTERS);
    const F32 safe_height = llclamp(height_pixels,
                                    MIN_LABEL_HEIGHT_PIXELS,
                                    MAX_LABEL_HEIGHT_PIXELS);
    const F32 cell_pixels = safe_height / 7.f;
    const F32 outline = safeOutlinePixels(style.mOutlinePixels);
    bool emitted = false;
    if (outline > 0.f && style.mOutlineColor.mV[VALPHA] > 0.f)
    {
        emitted = drawGlyphPass(visible_label,
                                lower_center_agent,
                                pixel_up,
                                pixel_right,
                                cell_pixels,
                                outline,
                                style.mOutlineColor);
    }
    if (style.mFillColor.mV[VALPHA] > 0.f)
    {
        emitted = drawGlyphPass(visible_label,
                                lower_center_agent,
                                pixel_up,
                                pixel_right,
                                cell_pixels,
                                0.f,
                                style.mFillColor) || emitted;
    }
    return emitted;
}

} // namespace ALWorldOverlayViz
