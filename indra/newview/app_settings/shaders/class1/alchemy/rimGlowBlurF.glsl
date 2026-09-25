/**
 * @file rimGlowBlurF.glsl
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

// Virtual Cinema Rim Glow -- one separable Gaussian pass (H or V) over the
// premultiplied mask (RGBA) and, optionally, the premultiplied dir pair (RG).
//
// CONTRACT (the composite's lobe normalization depends on it):
//   * dst resolution == src resolution (the downsample is a separate pass), so
//     integer-texel offsets land exactly on texel centres and bilinear == fetch.
//   * kernel k_i = exp(-i^2 / (2 sigma^2)), i in [-blurTaps, blurTaps], tap
//     spacing exactly ONE source texel, normalized by the tap sum. The host
//     computes WrapNorm/GlowNorm from this identical kernel (lobePeak()).
//   * blurSigma is in SOURCE texels (host: sigma_fullres_px / 2^level);
//     blurTaps = min(ceil(3 * blurSigma), RIM_TAPS_MAX) (host); 0 = passthrough.
//   * Both attachments use the same weights, so dir/alpha stays the
//     coverage-weighted mean direction (N2). RG output is signed, never clamped.
//
// Chains: Wrap = full-res, sigma_w = InwardBleed/2 (mask only, blurHasDir = 0).
//         Glow = half-res, sigma_g = AutoInteriorReach/2 (mask + dir, blurHasDir = 1).

out vec4 frag_data[2];

in vec2 vary_fragcoord;

uniform sampler2D blurSrc;      // premultiplied mask (RGBA16F) at this pass's resolution
uniform sampler2D blurSrcDir;   // premultiplied dir (RG16F), read only when blurHasDir != 0
uniform vec2      blurTexel;    // 1.0 / source size
uniform vec2      blurAxis;     // (1,0) horizontal pass, (0,1) vertical pass
uniform float     blurSigma;    // sigma in source texels
uniform int       blurTaps;     // taps per side; host = min(ceil(3*sigma), RIM_TAPS_MAX)
uniform int       blurHasDir;   // 1 = also blur blurSrcDir into frag_data[1]

const int   RIM_TAPS_MAX  = 48;
const float RIM_HDR_CLAMP = 6.0e4;

void main()
{
    vec2 uv = vary_fragcoord;

    vec4 sA = texture(blurSrc, uv);
    vec2 sB = (blurHasDir != 0) ? texture(blurSrcDir, uv).rg : vec2(0.0);

    int taps = min(blurTaps, RIM_TAPS_MAX);
    if (taps > 0 && blurSigma >= 0.3)
    {
        float inv2s2 = 0.5 / (blurSigma * blurSigma);
        float wsum   = 1.0;
        vec2  step   = blurAxis * blurTexel;

        for (int i = 1; i <= RIM_TAPS_MAX; ++i)
        {
            if (i > taps) break;
            float w = exp(-float(i * i) * inv2s2);
            vec2  o = step * float(i);
            sA += (texture(blurSrc, uv + o) + texture(blurSrc, uv - o)) * w;
            if (blurHasDir != 0)
            {
                sB += (texture(blurSrcDir, uv + o).rg + texture(blurSrcDir, uv - o).rg) * w;
            }
            wsum += 2.0 * w;
        }
        sA /= wsum;     // wsum >= 1
        sB /= wsum;
    }

    if (any(isnan(sA)) || any(isinf(sA))) sA = vec4(0.0);
    if (any(isnan(sB)) || any(isinf(sB))) sB = vec2(0.0);

    frag_data[0] = clamp(sA, vec4(-RIM_HDR_CLAMP), vec4(RIM_HDR_CLAMP));
    frag_data[1] = vec4(sB, 0.0, 1.0);
}
