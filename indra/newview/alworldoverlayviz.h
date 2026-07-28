/**
 * @file alworldoverlayviz.h
 * @brief Screen-readable, untextured world-overlay drawing helpers.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#ifndef AL_WORLD_OVERLAY_VIZ_H
#define AL_WORLD_OVERLAY_VIZ_H

#include <string>
#include <vector>

#include "v3math.h"
#include "v4color.h"

/**
 * Reusable immediate-mode geometry for the viewer's 2.5D world-overlay pass.
 *
 * All positions are in agent coordinates. World geometry (ring/disc radii and
 * arrow length) is expressed in metres; visual stroke widths, arrow heads, and
 * billboard labels are expressed in display pixels so they remain readable as
 * the camera moves.
 *
 * Construct one ScopedRenderer for a group of draws. Its lifetime establishes
 * the normal UI-overlay state (gUIProgram, alpha blending, no culling, no depth
 * test, texture unit zero unbound) and restores the previous shader, texture,
 * active texture unit, blend/cull/depth state on destruction. It deliberately
 * never changes GL line width: every visible stroke is triangle geometry.
 *
 * Caller requirements:
 *  - call on the render thread with a current GL context and valid world-view
 *    matrices (normally from render_ui_3d());
 *  - do not construct the scope inside gGL.begin()/gGL.end();
 *  - retain the pass's standard alpha blend function and writable colour mask;
 *  - do not interleave unrelated shader or texture changes inside the scope.
 *
 * The scope does not alter or restore matrix stacks, stencil/scissor state,
 * blend functions, or colour masks. It leaves the immediate vertex colour
 * white on exit. As in the existing beacon pass, depth is disabled, so these
 * shapes intentionally remain visible through scene geometry.
 */
namespace ALWorldOverlayViz
{

struct StrokeStyle
{
    LLColor4 mColor;
    LLColor4 mOutlineColor;
    F32      mWidthPixels;
    F32      mOutlinePixels;

    StrokeStyle(const LLColor4& color = LLColor4(1.f, 1.f, 1.f, 1.f),
                F32 width_pixels = 3.f,
                const LLColor4& outline_color = LLColor4(0.f, 0.f, 0.f, 0.72f),
                F32 outline_pixels = 1.5f)
        : mColor(color),
          mOutlineColor(outline_color),
          mWidthPixels(width_pixels),
          mOutlinePixels(outline_pixels)
    {
    }
};

struct FillStyle
{
    LLColor4 mFillColor;
    LLColor4 mOutlineColor;
    F32      mOutlinePixels;

    FillStyle(const LLColor4& fill_color = LLColor4(1.f, 1.f, 1.f, 0.28f),
              const LLColor4& outline_color = LLColor4(0.f, 0.f, 0.f, 0.72f),
              F32 outline_pixels = 1.5f)
        : mFillColor(fill_color),
          mOutlineColor(outline_color),
          mOutlinePixels(outline_pixels)
    {
    }
};

class ScopedRenderer final
{
public:
    ScopedRenderer();
    ~ScopedRenderer();

    ScopedRenderer(const ScopedRenderer&) = delete;
    ScopedRenderer& operator=(const ScopedRenderer&) = delete;

    /**
     * False when the UI shader is unavailable. All draw methods then no-op and
     * return false.
     */
    bool isReady() const;

    /**
     * Draw a rounded, outlined screen-stable segment/polyline. Invalid or
     * non-finite input rejects the complete call. Closed polylines require at
     * least three points; open polylines require two.
     */
    bool drawSegment(const LLVector3& start_agent,
                     const LLVector3& end_agent,
                     const StrokeStyle& style) const;
    bool drawPolyline(const std::vector<LLVector3>& points_agent,
                      const StrokeStyle& style,
                      bool closed = false) const;

    /**
     * Draw a screen-stable stroked circle in an arbitrary world plane.
     * Terrain-conforming rings can instead be supplied as sampled points to
     * drawPolyline(..., true).
     */
    bool drawRing(const LLVector3& center_agent,
                  const LLVector3& plane_normal,
                  F32 radius_metres,
                  const StrokeStyle& style,
                  S32 segments = 48) const;

    /**
     * Draw a world-space planar annulus with screen-stable outlines on its
     * inner and outer boundaries. Set inner_radius_metres to zero for a disc.
     */
    bool drawAnnulus(const LLVector3& center_agent,
                     const LLVector3& plane_normal,
                     F32 inner_radius_metres,
                     F32 outer_radius_metres,
                     const FillStyle& style,
                     S32 segments = 48) const;

    /**
     * Convenience wrapper for a filled footprint disc.
     */
    bool drawFootprintDisc(const LLVector3& center_agent,
                           const LLVector3& plane_normal,
                           F32 radius_metres,
                           const FillStyle& style,
                           S32 segments = 40) const;

    /**
     * Draw an outlined facing arrow. Direction may be any finite non-zero
     * world vector. The shaft length is in metres; head dimensions are pixels.
     */
    bool drawFacingArrow(const LLVector3& origin_agent,
                         const LLVector3& direction,
                         F32 length_metres,
                         const StrokeStyle& style,
                         F32 head_length_pixels = 16.f,
                         F32 head_width_pixels = 13.f) const;

    /**
     * Draw compact outlined 5x7 billboard glyphs above the supplied lower
     * centre anchor. Labels support ASCII A-Z, 0-9, space and basic punctuation;
     * lowercase is folded to uppercase and unsupported glyphs become '?'.
     * Labels are capped at 16 characters.
     */
    bool drawBillboardNumber(S32 value,
                             const LLVector3& lower_center_agent,
                             F32 height_pixels,
                             const FillStyle& style) const;
    bool drawBillboardLabel(const std::string& label,
                            const LLVector3& lower_center_agent,
                            F32 height_pixels,
                            const FillStyle& style) const;

private:
    struct State;
    State* mState;
};

} // namespace ALWorldOverlayViz

#endif // AL_WORLD_OVERLAY_VIZ_H
