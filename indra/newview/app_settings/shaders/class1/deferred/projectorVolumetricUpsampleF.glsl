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

    // Depth similarity falloff. sigma scales with distance so the tolerance is
    // roughly perspective-stable (far surfaces span more depth per pixel).
    float sigma = max(d_full * 0.05, 0.10);

    vec3  sum  = vec3(0.0);
    float wsum = 0.0;

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

        // Depth-similarity (bilateral) weight - suppress taps across an edge.
        float wdepth = exp(-abs(d_full - d_tap) / sigma);

        float w = wbil * wdepth;
        sum  += c * w;
        wsum += w;
    }

    // If every tap sits across a depth discontinuity (wsum collapses), fall back
    // to a plain bilinear sample so we never divide by ~0 or drop the shaft.
    vec3 shaft = (wsum > 1e-4) ? (sum / wsum) : texture(projectionMap, tc).rgb;

    // Additive composite (GL_ONE, GL_ONE) onto the linear HDR scene buffer.
    frag_color = vec4(shaft, 0.0);
}
