/**
 * @file rimGlowDownsampleF.glsl
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

// Virtual Cinema Rim Glow -- exact 2x2 box downsample of the premultiplied
// (mask RGBA, dir RG) pair: full-res -> half-res, MRT x2.
//
// Why a separate pass: a half-res blur that samples the full-res mask directly
// keeps only every other ROW, so a one-row silhouette band is present or absent
// by parity (a 2x ripple no kernel can fix). Averaging both axes first keeps the
// band's integrated mass exactly, which is what the composite's lobe
// normalization (host lobePeak()) is calibrated against.
//
// Both attachments are averaged with the SAME weights so the ratio dir/alpha
// read in the composite stays the coverage-weighted mean direction (N2).
//
//   frag_data[0] : mask half-res RGBA16F
//   frag_data[1] : dir  half-res RG16F (signed)
//
// Samples are taken at full-res texel CENTRES so bilinear == exact fetch;
// clamp-to-edge on the source handles odd full-res dimensions.

out vec4 frag_data[2];

in vec2 vary_fragcoord;

uniform sampler2D dsSrc;        // full-res mask (RGBA16F)
uniform sampler2D dsSrcDir;     // full-res dir  (RG16F)
uniform vec2      dsTexelSize;  // 1.0 / full-res source size

const float RIM_HDR_CLAMP = 6.0e4;

void main()
{
    // dst texel (i,j) covers src texels (2i,2j)..(2i+1,2j+1)
    vec2 base = (floor(gl_FragCoord.xy) * 2.0 + 0.5) * dsTexelSize;   // centre of src texel (2i,2j)
    vec2 o    = dsTexelSize;

    vec2 uv00 = base;
    vec2 uv10 = base + vec2(o.x, 0.0);
    vec2 uv01 = base + vec2(0.0, o.y);
    vec2 uv11 = base + o;

    vec4 m = texture(dsSrc, uv00) + texture(dsSrc, uv10) + texture(dsSrc, uv01) + texture(dsSrc, uv11);
    vec2 d = texture(dsSrcDir, uv00).rg + texture(dsSrcDir, uv10).rg
           + texture(dsSrcDir, uv01).rg + texture(dsSrcDir, uv11).rg;

    m *= 0.25;
    d *= 0.25;

    if (any(isnan(m)) || any(isinf(m))) m = vec4(0.0);
    if (any(isnan(d)) || any(isinf(d))) d = vec2(0.0);

    frag_data[0] = clamp(m, vec4(-RIM_HDR_CLAMP), vec4(RIM_HDR_CLAMP));
    frag_data[1] = vec4(d, 0.0, 1.0);
}
