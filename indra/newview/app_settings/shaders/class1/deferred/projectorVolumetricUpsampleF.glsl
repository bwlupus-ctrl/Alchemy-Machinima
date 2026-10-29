/**
 * @file class1\deferred\projectorVolumetricUpsampleF.glsl
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2007, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 *
 * [BDMerge G3.3 Phase 1 item 3] Depth-aware bilateral upsample for the projector
 * volumetric cones. The cones are marched into a half-resolution target
 * (mProjVolHalf, additive RGB) for a large fill-rate win; this pass resolves that
 * half-res shaft back to full resolution WITHOUT bleeding across depth
 * discontinuities (which a naive bilinear upscale would smear into halos around
 * occluder silhouettes). Each full-res pixel gathers the 4 surrounding half-res
 * texels and weights them by (bilinear footprint) * (view-space depth similarity
 * between this pixel and the tap). The result composites additively onto the
 * linear HDR scene buffer, exactly where the fullscreen path composited (Phase 1
 * item 1 placement: after exposure/bloom, before colorCorrect).
 */

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;

in vec2 vary_fragcoord;

// The half-res marched shaft (rgb additive; alpha unused) is bound to the reserved
// `projectionMap` sampler (LLShaderMgr::DEFERRED_PROJECTION) by the C++ side - a
// reserved sampler so it gets a real texture channel; it is otherwise unused by
// this pass (bindDeferredShader does not touch it). It is also declared in
// deferredUtil.glsl, but a global must be declared in each compilation unit that
// references it directly (same pattern as `size` in projectorVolumetricF.glsl);
// the linker merges the two identical declarations.
uniform sampler2D projectionMap;
// dimensions of that half-res map (half of the scene buffer), for locating taps.
uniform vec2 projvol_half_res;

// deferredUtil.glsl - view-space position reconstruction from the full-res depth,
// and the half-res shaft sampler (projectionMap).
vec4 getPosition(vec2 pos_screen);

void main()
{
    vec2 tc = vary_fragcoord.xy;               // [0,1] full-res UV

    // Full-res view-space depth (distance) at this pixel.
    float d_full = length(getPosition(tc).xyz);

    // Locate the 2x2 block of half-res texels around this UV (texel-center space).
    vec2 hres = projvol_half_res;
    vec2 f    = tc * hres - 0.5;
    vec2 base = floor(f);
    vec2 frac = f - base;

    // The broad kernel preserves the existing smooth reconstruction on continuous
    // surfaces.  A second, strict kernel is accumulated from the SAME four taps
    // for silhouette pixels.  This costs no additional texture fetches or march
    // samples, but prevents a half-res background ray (which integrated fog behind
    // an avatar) from being normalized back to full strength on the avatar.
    float sigma           = max(d_full * 0.05, 0.10);
    float depth_tolerance = max(d_full * 0.0075, 0.015);

    vec3  sum         = vec3(0.0);
    float wsum        = 0.0;
    vec3  strict_sum  = vec3(0.0);
    float strict_wsum = 0.0;

    float depth_min     = 1e30;
    float depth_max     = 0.0;
    float nearest_delta = 1e30;
    float nearest_depth = 0.0;
    vec3  nearest_color = vec3(0.0);

    for (int j = 0; j < 2; ++j)
    for (int i = 0; i < 2; ++i)
    {
        vec2  tap_texel = base + vec2(float(i), float(j)) + 0.5;
        vec2  tap_uv    = tap_texel / hres;

        vec3  c     = texture(projectionMap, tap_uv).rgb;
        float d_tap = length(getPosition(tap_uv).xyz);

        // Bilinear footprint weight.
        float wx = (i == 0) ? (1.0 - frac.x) : frac.x;
        float wy = (j == 0) ? (1.0 - frac.y) : frac.y;
        float wbil = wx * wy;

        float depth_delta = abs(d_full - d_tap);

        // Depth-similarity (bilateral) weight - suppress taps across an edge.
        float wdepth = exp(-depth_delta / sigma);

        float w = wbil * wdepth;
        sum  += c * w;
        wsum += w;

        // Strict same-surface support used only at a detected discontinuity.  The
        // soft shoulder avoids a hard one-pixel transition as either surface moves.
        float strict_depth = 1.0 - smoothstep(depth_tolerance,
                                              depth_tolerance * 3.0,
                                              depth_delta);
        float strict_w = wbil * strict_depth;
        strict_sum  += c * strict_w;
        strict_wsum += strict_w;

        depth_min = min(depth_min, d_tap);
        depth_max = max(depth_max, d_tap);
        if (depth_delta < nearest_delta)
        {
            nearest_delta = depth_delta;
            nearest_depth = d_tap;
            nearest_color = c;
        }
    }

    vec3 smooth_shaft = (wsum > 1e-4) ? (sum / wsum) : nearest_color;

    // A large span between taps catches an edge even when one tap matches this
    // pixel.  A large nearest_delta catches the more dangerous case where ALL four
    // half-res taps landed on the other side of a thin/close avatar silhouette.
    float span_tolerance = max(d_full * 0.015, 0.03);
    float edge_factor = max(smoothstep(span_tolerance,
                                      span_tolerance * 2.0,
                                      depth_max - depth_min),
                            smoothstep(depth_tolerance,
                                      depth_tolerance * 3.0,
                                      nearest_delta));

    // If there is no compatible half-res ray, never borrow a longer/background
    // march for a nearer full-res surface: that is the bright fog-over-avatar
    // failure.  Zero is the conservative estimate there.  In the reverse case a
    // nearer tap is safe (it can only under-integrate the background), so retain it
    // to avoid drawing a dark outline around the avatar.
    vec3 conservative_fallback = (d_full + depth_tolerance < nearest_depth)
                               ? vec3(0.0)
                               : nearest_color;
    vec3 edge_shaft = (strict_wsum > 1e-5)
                    ? (strict_sum / strict_wsum)
                    : conservative_fallback;

    // Continuous blend keeps moving silhouettes stable under temporal resolve.
    vec3 shaft = mix(smooth_shaft, edge_shaft, edge_factor);

    // Additive composite (GL_ONE, GL_ONE) onto the linear HDR scene buffer.
    frag_color = vec4(shaft, 0.0);
}
