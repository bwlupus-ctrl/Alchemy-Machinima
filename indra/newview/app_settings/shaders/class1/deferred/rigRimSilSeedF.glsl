/**
 * @file rigRimSilSeedF.glsl
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Machinima Viewer Source Code
 * Copyright (C) 2026, Alchemy Machinima
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
 * $/LicenseInfo$
 */

/*[EXTRA_CODE_HERE]*/

// [RigRimQ] Silhouette seed pass (doc/RIGRIM_QUALITY_CONTRACT.md, pass B1).
//
// HALF-res, MAIN view only, right after the aux pass (pass A is a
// PREREQUISITE: the tangent plane uses the GEOMETRIC guide from rigRimAuxMap),
// into rigRimSilMap[0] (RGBA16F).  Classifies each half-res pixel with the
// Roto Ink rotoRing tangent-plane residual test (cineOutlineF.glsl 845-893):
// 8 taps one half-res texel (2 full-res px) away; a neighbour that lies BEHIND
// the centre's tangent plane by more than the threshold marks the centre as
// the NEAR side of a silhouette = a seed.
//   xy  signed OFFSET to the seed in half-res texels ((0,0): a seed points at
//       itself; the DT passes fill in real offsets, fp16-exact integers)
//   z   this pixel's view depth (m)
//   w   0.0 for a seed, RIG_RIM_DT_BIG otherwise (distance to self)
// Sky pixels are never seeds (the near-side object pixel next to them is).
//
// Program: isDeferred (deferredUtil.glsl getDepth / getPositionWithDepth read
// the FULL-res G-buffer through uv; rigRimOctDecode).  Vertex:
// deferred/postDeferredNoTCV.glsl.  GL: blend off, depth off, viewport =
// half-res target (set by bindTarget).  rigRimAuxMap bound TFO_POINT.

out vec4 frag_color;

in vec2 vary_fragcoord;

uniform vec2 screen_res;          // FULL-res size (bindDeferredShader); taps are 2 px
uniform vec4 rig_rim_aux_params;  // y seed threshold (relative residual), z softness 0..1

float getDepth(vec2 pos_screen);
vec4  getNorm(vec2 screenpos);
vec4  getPositionWithDepth(vec2 pos_screen, float depth);

const float RIG_RIM_DT_BIG = 1.0e4;

const vec2 SEED_DIRS[8] = vec2[8](
    vec2( 1.0,  0.0), vec2( 0.70710678,  0.70710678),
    vec2( 0.0,  1.0), vec2(-0.70710678,  0.70710678),
    vec2(-1.0,  0.0), vec2(-0.70710678, -0.70710678),
    vec2( 0.0, -1.0), vec2( 0.70710678, -0.70710678));

// [Review B3] Grazing taps (|n . ray| <= RIG_RIM_SEED_GRAZE) are SKIPPED, never
// predicted with z_c: on flat ground seen from eye height that fallback kicked
// in beyond ~85 m, where the per-texel depth step alone (dz/z ~ 0.05-0.1)
// exceeds the threshold and every pixel became a seed.  With the plane
// prediction a flat surface has resid == 0 at ANY distance (the plane hit IS
// the neighbour's depth, up to depth-buffer quantisation), so flat ground
// never seeds; the metric floor absorbs the quantisation.  The tangent plane
// uses the GEOMETRIC guide from rigRimAuxMap (pass A is a prerequisite), not the
// normal-mapped shading normal, so a bumpy material cannot fake an edge.
// Output xy is now the signed OFFSET to the seed in half-res texels ((0,0): a
// seed points at itself), never an absolute uv (fp16-exact integers).
uniform sampler2D rigRimAuxMap;   // same declaration as deferredUtil.glsl (separate unit)
vec3 rigRimOctDecode(vec2 e);

const float RIG_RIM_SKY_Z      = 6.0e4;   // finite in fp16 (max 65504)
const float RIG_RIM_SEED_GRAZE = 0.02;    // |n . ray| below this: tap skipped
const float RIG_RIM_SEED_MIN_M = 0.01;    // metric floor on the residual (depth quantisation)

void main()
{
    vec2  uv  = vary_fragcoord;
    // Match nearest depth/guide samples to their actual full-res pixel centres.
    // Keep the stock zero-Smoothness instruction path and results.
#ifdef RIG_RIM_SMOOTH_SEED
    uv = (min(vec2(ivec2(gl_FragCoord.xy)*2+ivec2(1)),screen_res-1.0)+.5)/screen_res;
#endif
    float d_c = getDepth(uv);
    if (d_c >= 0.999999)
    {
        frag_color = vec4(0.0, 0.0, RIG_RIM_SKY_Z, RIG_RIM_DT_BIG);
        return;
    }

    vec3  p_c = getPositionWithDepth(uv, d_c).xyz;
    float z_c = max(-p_c.z, 1.0e-3);
    vec3  n_c = rigRimOctDecode(textureLod(rigRimAuxMap, uv, 0.0).xy);   // geometric guide

    float thr    = max(rig_rim_aux_params.y, 1.0e-5);
    float soft   = clamp(rig_rim_aux_params.z, 0.02, 1.0);
    float sil_lo = thr * (1.0 - 0.5 * soft);
    float sil_hi = thr * (1.0 + 0.5 * soft);

    vec2  px      = 2.0 / screen_res;       // one half-res texel
    float plane_d = dot(n_c, p_c);          // tangent plane: n_c . p = plane_d
    float sil     = 0.0;

    for (int i = 0; i < 8; ++i)
    {
        vec2  tc  = clamp(uv + SEED_DIRS[i] * px, vec2(0.0), vec2(1.0));
#ifdef RIG_RIM_SMOOTH_SEED
        tc = (clamp(floor(tc*screen_res),vec2(0),screen_res-1.0)+.5)/screen_res;
#endif
        float dep = getDepth(tc);
        vec3  p_i = getPositionWithDepth(tc, dep).xyz;
        float z_i = max(-p_i.z, 0.0);

        vec3  ray   = normalize(p_i);
        float denom = dot(n_c, ray);
        if (abs(denom) <= RIG_RIM_SEED_GRAZE)
        {
            continue;                       // grazing: no reliable prediction, no seed from this tap
        }
        float t = plane_d / denom;          // ray parameter of the plane hit
        if (t <= 0.0)
        {
            continue;                       // plane behind the camera along this ray
        }
        float z_pred = clamp(max(-(ray * t).z, 0.0), 0.5 * z_c, 1.5 * z_c);

        float resid_m = z_i - z_pred;       // > 0: neighbour behind the surface (metres)
        if (resid_m <= RIG_RIM_SEED_MIN_M)
        {
            continue;
        }
        float resid = resid_m / max(z_c, 1.0e-3);
        sil = max(sil, smoothstep(sil_lo, sil_hi, resid));
    }

    frag_color = vec4(0.0, 0.0, z_c, (sil > 0.5) ? 0.0 : RIG_RIM_DT_BIG);
}
