/**
 * @file rigRimWrapF.glsl
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

// [RigRimQ] Light wrap composite (doc/RIGRIM_QUALITY_CONTRACT.md, pass C3).
//
// Full-res, additive (BT_ADD, colour mask RGB only) onto mRT->screen at the
// pre-alpha pool boundary in renderGeomPostDeferred. Wrap-only debug remains
// in renderFinalize. The beauty source excludes blended hair; subsequent
// source-alpha blending occludes this opaque-surface light normally. For every
// pixel inside the silhouette band on the NEAR side of its nearest seed, the
// blurred background just beyond that seed is folded onto the edge, scaled by
// the grazing factor of the smooth normal:
//
//   dir     = the DT's signed texel offset to the nearest seed (exact in fp16)
//             when the seed is >= 1.5 texels away; closer than that (the seed
//             texel itself and its immediate neighbours, where any offset is
//             direction noise) the smooth normal's screen-space xy is used
//             (review P2-1)
//   seed    = uv + offset (half-res texels -> uv)
//   bg      = blurred(seed + dir * reach)  clamped per channel, desaturated
//   wrap    = strength * bg * band(dist) * near_ok * (1 - NoV_geo) * subject^gate
//
// Inputs: rigRimAuxMap / rigRimSilMap (deferredUtil.glsl declarations; the
// host guarantees both are valid for THIS main view), rigRimWrapSrc (the
// quarter-res blurred scene = rigRimWrapBlur[0] after C3a, BILINEAR), depth via
// deferredUtil. Beauty uses the live scene projection at this boundary.
// All map reads are textureLod(..., 0): this
// fragment is in non-uniform flow.
//   rig_rim_wrap   x strength, y reach (half-res texels), z saturation 0..1, w clamp (HDR)
//   rig_rim_wrap2  z debug (1 = output the wrap term only; host draws with blend OFF),
//                  w subject gate 0/1 (ActorsOnly)
//
// Program: isDeferred.  Vertex: deferred/postDeferredNoTCV.glsl.
// GL: additive blend and RGB-only mask on attachment 0, depth/cull off.
// The host restores indexed state and routing, preserving sidecar/parity.

out vec4 frag_color;

in vec2 vary_fragcoord;

uniform vec2 screen_res;

uniform sampler2D rigRimWrapSrc;
uniform vec4      rig_rim_wrap;
uniform vec4      rig_rim_wrap2;

uniform sampler2D rigRimAuxMap;   // same declaration as deferredUtil.glsl (separate unit)
uniform sampler2D rigRimSilMap;

float getDepth(vec2 pos_screen);
vec4  getPositionWithDepth(vec2 pos_screen, float depth);
vec3  rigRimOctDecode(vec2 e);
float rigRimSilBandWidth(float z_view);
float rigRimSilProx(vec2 tc, float z_view);

const float RIG_RIM_DT_BIG = 1.0e4;

void main()
{
    vec2  uv = vary_fragcoord;
    float d  = getDepth(uv);
    vec3  wrap = vec3(0.0);

    if (d < 0.999999)
    {
        vec4 s = textureLod(rigRimSilMap, uv, 0.0);   // NEAREST
        if (s.w < RIG_RIM_DT_BIG)
        {
            vec3  p      = getPositionWithDepth(uv, d).xyz;
            float z_view = max(-p.z, 1.0e-3);
            vec3  v      = -normalize(p);

            vec4  a       = textureLod(rigRimAuxMap, uv, 0.0);
            vec3  n_geo   = rigRimOctDecode(a.xy);
            float subject = clamp(a.w, 0.0, 1.0);

            float prox = rigRimSilProx(uv, z_view);           // band * near_ok
            if (prox > 0.0)
            {
                vec2 texel_uv = 2.0 / screen_res;                // one half-res texel in uv

                // outward direction: toward the seed, past it into the background.
                // The smooth normal's screen xy already points outward at a
                // silhouette; it is the only stable direction at the seed itself.
                vec2  nxy  = n_geo.xy;
                float nl2  = dot(nxy, nxy);
                vec2  dir  = (nl2 > 1.0e-8) ? nxy * inversesqrt(nl2) : vec2(1.0, 0.0);
                if (s.w >= 1.5)
                {
                    vec2  o   = s.xy * vec2(1.0, screen_res.x / screen_res.y);   // texel offset -> aspect-correct screen direction
                    float ol2 = dot(o, o);
                    if (ol2 > 1.0e-8)
                    {
                        dir = o * inversesqrt(ol2);
                    }
                }
                vec2 seed_uv   = uv + s.xy * texel_uv;
                vec2 reach_uv  = max(rig_rim_wrap.y, 0.0) * texel_uv;         // half-res texels -> uv
                vec2 sample_uv = clamp(seed_uv + dir * reach_uv, vec2(0.0), vec2(1.0));

                vec3  bg  = textureLod(rigRimWrapSrc, sample_uv, 0.0).rgb;
                bg        = min(max(bg, vec3(0.0)), vec3(max(rig_rim_wrap.w, 0.0)));
                float lum = dot(bg, vec3(0.2126, 0.7152, 0.0722));
                bg        = mix(vec3(lum), bg, clamp(rig_rim_wrap.z, 0.0, 1.0));

                float graze = 1.0 - clamp(abs(dot(n_geo, v)), 0.0, 1.0);
                float gate  = mix(1.0, subject, clamp(rig_rim_wrap2.w, 0.0, 1.0));

                wrap = max(rig_rim_wrap.x, 0.0) * bg * prox * graze * gate;
            }
        }
    }

    // alpha 0: under BT_ADD the glow tag in screen.a is untouched (and the
    // host masks alpha off anyway); in debug (blend off) alpha 0 is harmless.
    frag_color = vec4(wrap, 0.0);
}
