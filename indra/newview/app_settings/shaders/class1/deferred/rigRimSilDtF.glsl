/**
 * @file rigRimSilDtF.glsl
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

// [RigRimQ] Windowed exact Euclidean distance transform, separable
// (doc/RIGRIM_QUALITY_CONTRACT.md, passes B2 / B3).
//
// Two permutations of this file, HALF-res ping-pong on rigRimSilMap[0/1]:
//   (none)                 horizontal: rigRimSilMap[0] (seeds) -> rigRimSilMap[1]
//   RIG_RIM_DT_VERTICAL    vertical  : rigRimSilMap[1]         -> rigRimSilMap[0]
// Layout in and out: xy signed OFFSET to the nearest seed in half-res texels
// (fp16-exact integers, |offset| <= R), z seed view depth (m), w distance
// (half-res texels; RIG_RIM_DT_BIG = none within RIG_RIM_DT_R).  The H pass
// stores offset (dx, 0) and distance |dx|; the V pass reads the H result of
// row y+dy, whose seed therefore sits at offset (dx_h, dy) from this pixel,
// and combines sqrt(dH^2 + dy^2) over the column window: the exact windowed
// EDT.  Carrying the seed data along gives every pixel the position + depth of
// its nearest seed for the near-side test and the light-wrap sampling
// direction (an offset, not an absolute uv, so fp16 cannot jitter it).
//
// Program: no features (texelFetch only).  Vertex: deferred/postDeferredNoTCV.glsl.
// GL: blend off, depth off; the source is bound to rigRimSilMap (NEAREST).

out vec4 frag_color;

in vec2 vary_fragcoord;

uniform sampler2D rigRimSilMap;   // previous stage (same size as the target)

const int   RIG_RIM_DT_R   = 24;
const float RIG_RIM_DT_BIG = 1.0e4;

void main()
{
    ivec2 size = textureSize(rigRimSilMap, 0);
    ivec2 c    = ivec2(gl_FragCoord.xy);
    c = clamp(c, ivec2(0), size - ivec2(1));

#ifdef RIG_RIM_DT_VERTICAL
    const ivec2 dstep = ivec2(0, 1);
    const int   lim   = 1; // y
#else
    const ivec2 dstep = ivec2(1, 0);
    const int   lim   = 0; // x
#endif

    vec4  best  = texelFetch(rigRimSilMap, c, 0);
    float bestd = best.w;   // H: 0 (seed) or BIG; V: the row distance dH
    vec2  besto = best.xy;  // offset to that seed from THIS pixel (H: (0,0) for a seed; V: (dx_h, 0))

    for (int i = 1; i <= RIG_RIM_DT_R; ++i)
    {
        float fi = float(i);
        if (fi >= bestd)
        {
            break;          // no candidate at >= i texels can beat the current best
        }
        for (int s = -1; s <= 1; s += 2)
        {
            ivec2 q = c + dstep * (i * s);
            if (q[lim] < 0 || q[lim] >= size[lim])
            {
                continue;
            }
            vec4 cand = texelFetch(rigRimSilMap, q, 0);
#ifdef RIG_RIM_DT_VERTICAL
            if (cand.w >= RIG_RIM_DT_BIG)
            {
                continue;   // that row had no seed within the window
            }
            float d    = sqrt(cand.w * cand.w + fi * fi);
            vec2  offs = vec2(cand.x, fi * float(s));   // (dx_h from that row, dy to that row)
#else
            if (cand.w > 0.5)
            {
                continue;   // not a seed
            }
            float d    = fi;
            vec2  offs = vec2(fi * float(s), 0.0);
#endif
            if (d < bestd)
            {
                bestd = d;
                best  = cand;
                besto = offs;
            }
        }
    }

    frag_color = vec4(besto, best.z, min(bestd, RIG_RIM_DT_BIG));
}
