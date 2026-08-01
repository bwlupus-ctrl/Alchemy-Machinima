/**
 * @file class1\deferred\projectorVolumetricTemporalF.glsl
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
 * [BDMerge G3.3 Batch 1 A] Projector-volumetric TEMPORAL REPROJECTION resolve.
 * Runs in the half-res domain between the cone march (into projectionMap) and the
 * bilateral upsample. For each half-res pixel it:
 *   1. reprojects the current pixel's beam point (or legacy scene surface) into
 *      the PREVIOUS frame's screen and samples the previous accumulation,
 *   2. rejects that history on camera cuts (reprojected UV off-screen) and
 *      disocclusion (large depth delta vs. the stored previous depth),
 *   3. clamps the surviving history to the current frame's 3x3 neighborhood
 *      min/max so it can never ghost or smear, and
 *   4. blends current<->clamped-history with an EMA weight.
 * The output rgb is the accumulated shaft; the alpha stores this pixel's view-space
 * depth for next frame's disocclusion test. Because the clamp bounds the result to
 * the current neighborhood, a static scene converges to (essentially) the current
 * image - just denoised - so the accumulation adds smoothness without changing the
 * look, and lets the march run fewer samples for the same stability.
 */

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;

in vec2 vary_fragcoord;

// Current frame's raw half-res shaft (rgb additive) - reserved projectionMap sampler
// (unused by bindDeferredShader; bound POINT for exact neighborhood taps). Declared
// in deferredUtil.glsl too; the linker merges the identical declarations.
uniform sampler2D projectionMap;
// Previous frame's accumulation: rgb = shaft, a = stored view-space depth. Bound
// BILINEAR for sub-texel reprojection sampling.
uniform sampler2D projvol_history;

uniform vec2  projvol_half_res;        // dimensions of the half-res targets
uniform mat4  projvol_inv_modelview;   // view -> agent(world)
uniform mat4  projvol_prev_viewproj;   // world -> previous-frame clip
uniform mat4  projvol_prev_modelview;  // world -> previous-frame view
uniform float projvol_temporal_blend;  // EMA history weight (0 => pure current)

// [BDMerge G3.3 Batch B - R2] Contrast-aware history rejection. The neighborhood
// clamp below can only stop a trail when local contrast is LOW; a high-contrast beam
// feature reprojected off an approximate depth leaves a plausible-but-stale value
// INSIDE the clamp window, which reads as ghosting. This drops history in proportion
// to how much the (already-clamped) history disagrees with the current shaft at this
// pixel, so steady beam still accumulates (denoise kept) but moving/changing features
// snap to the current frame (trail killed). 0 = off (legacy Batch-1 behavior).
uniform float projvol_temporal_reject;

// [BDMerge G3.3 Batch B - R1] Beam-depth reprojection gate. 0 = the legacy
// surface-based reprojection and surface-depth disocclusion, byte-identical to today.
// !=0 recovers the airborne BEAM distance from the march's luminance-premultiplied
// first moment, then reprojects that beam point instead of the opaque surface.
uniform int   projvol_temporal_beam_depth;

// deferredUtil.glsl - view-space position reconstruction from the full-res depth.
vec4 getPosition(vec2 pos_screen);

void main()
{
    vec2 tc = vary_fragcoord.xy;                 // [0,1] half-res UV

    // Current frame's raw shaft/moment + this pixel's scene view position/depth.
    vec4  cur_sample = texture(projectionMap, tc);
    vec3  cur        = cur_sample.rgb;
    vec3  vpos       = getPosition(tc).xyz;
    float d_cur      = length(vpos);

    // [BDMerge G3.3 Batch B - R1] Choose the point we reproject / disocclusion-test /
    // store. Legacy (gate off): the opaque SURFACE position - byte-identical below.
    // Gate on: recover mean beam distance = first moment / shaft luminance, then
    // reconstruct its current view-space point. Empty pixels fall back to the
    // opaque surface so they reproject like the wall rather than the camera origin.
    vec3  reproj_vpos = vpos;   // point reprojected into the previous frame
    float d_test      = d_cur;  // distance used for the disocclusion compare
    float d_store     = d_cur;  // distance stored in alpha for next frame
    if (projvol_temporal_beam_depth != 0)
    {
        const float luma_eps = 1e-5;
        float cur_luma = dot(cur, vec3(0.2126, 0.7152, 0.0722));
        float beam_dist = (cur_luma >= luma_eps)
                        ? (cur_sample.a / max(cur_luma, luma_eps))
                        : d_cur;
        vec3 vdir = (d_cur > luma_eps) ? (vpos / d_cur) : vec3(0.0);
        reproj_vpos = vdir * beam_dist;
        d_store     = beam_dist;
    }

    // ---- reproject into the previous frame ---------------------------------
    vec3 wpos     = (projvol_inv_modelview * vec4(reproj_vpos, 1.0)).xyz;
    vec4 prevclip = projvol_prev_viewproj * vec4(wpos, 1.0);
    if (projvol_temporal_beam_depth != 0)
    {
        // hist.a was measured in the previous camera's view space. Compare it
        // against this reprojected point in that same space so camera translation
        // does not look like disocclusion.
        d_test = length((projvol_prev_modelview * vec4(wpos, 1.0)).xyz);
    }

    float validity = projvol_temporal_blend; // 0 when C++ has no valid history
    vec2  prevuv   = vec2(0.0);
    if (prevclip.w > 1e-5)
    {
        prevuv = (prevclip.xy / prevclip.w) * 0.5 + 0.5;
        // Camera cut / off-screen reprojection -> reject history entirely.
        if (prevuv.x < 0.0 || prevuv.x > 1.0 || prevuv.y < 0.0 || prevuv.y > 1.0)
        {
            validity = 0.0;
        }
    }
    else
    {
        validity = 0.0; // reprojected behind the previous camera
    }

    vec4 hist = (validity > 0.0) ? texture(projvol_history, prevuv) : vec4(0.0);

    // Disocclusion: if the reprojected point's depth differs too much from the
    // stored previous depth, the history belongs to different geometry -> drop.
    // [R1] d_test is the surface distance when the beam-depth gate is off (legacy)
    // and the beam distance when on; hist.a stores whichever the previous frame wrote.
    if (validity > 0.0)
    {
        float d_hist = hist.a;
        float dref   = max(d_test, 1.0);
        if (abs(d_test - d_hist) > dref * 0.1)
        {
            validity = 0.0;
        }
    }

    // ---- neighborhood clamp (anti-ghost safety net) ------------------------
    // Bound the history to the current 3x3 shaft neighborhood min/max so even an
    // imperfect reprojection can never smear a trail - the worst case degrades to
    // "looks like the current frame".
    vec3 nmin = cur;
    vec3 nmax = cur;
    vec2 texel = 1.0 / projvol_half_res;
    for (int j = -1; j <= 1; ++j)
    for (int i = -1; i <= 1; ++i)
    {
        if (i == 0 && j == 0) continue;
        vec3 n = texture(projectionMap, tc + vec2(float(i), float(j)) * texel).rgb;
        nmin = min(nmin, n);
        nmax = max(nmax, n);
    }
    vec3 hclamp = clamp(hist.rgb, nmin, nmax);

    // EMA blend: weight toward the (clamped, validated) history.
    float w   = clamp(validity, 0.0, 0.98);

    // [BDMerge G3.3 Batch B - R2] Contrast-aware rejection. Measure how much the
    // clamped history still disagrees with the current shaft, RELATIVE to their
    // magnitude (scale-invariant for a dim or a bright beam). Where they agree
    // (steady beam) change ~0 and the weight is untouched -> full denoise. Where a
    // feature is moving through, change rises toward 1 and the weight is pulled down
    // exponentially -> the pixel snaps to the current frame instead of trailing.
    // At reject == 0 this is an exact no-op (the shipped Batch-1 blend).
    if (projvol_temporal_reject > 0.0)
    {
        float lc = dot(cur,    vec3(0.2126, 0.7152, 0.0722));
        float lh = dot(hclamp, vec3(0.2126, 0.7152, 0.0722));
        float change = abs(lc - lh) / (lc + lh + 1e-3);   // 0 steady .. ~1 fully changed
        w *= exp(-projvol_temporal_reject * change);
    }

    vec3  outc = mix(cur, hclamp, w);

    // Store accumulated shaft + this frame's depth for next frame's disocclusion.
    // [R1] d_store is the surface distance (legacy) or the beam distance (gate on).
    frag_color = vec4(outc, d_store);
}
