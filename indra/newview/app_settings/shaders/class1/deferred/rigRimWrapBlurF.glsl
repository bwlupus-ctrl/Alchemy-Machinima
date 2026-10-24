/**
 * @file rigRimWrapBlurF.glsl
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

// [RigRimQ] Light-wrap background blur (doc/RIGRIM_QUALITY_CONTRACT.md,
// passes C1 / C2 / C3a).  Three QUARTER-res passes, two permutations of this
// file, so the result is isotropic and alias-free (review P2-2):
//   C1  RIG_RIM_WRAP_DOWNSAMPLE : src = mRT->screen (full-res HDR), dst = rigRimWrapBlur[0]
//       4x4 box prefilter (4 bilinear taps at +-1 full-res px around the
//       quarter-texel centre); rig_rim_wrap2.xy unused.
//       Beauty runs before alpha pools: blended hair is neither a background
//       blur source nor an unoccluded recipient. Wrap-only debug runs later.
//   C2  (none)                  : src = rigRimWrapBlur[0], dst = rigRimWrapBlur[1]
//       horizontal 9-tap Gaussian; rig_rim_wrap2.xy = (blur_q / blur0_w, 0)
//   C3a (none)                  : src = rigRimWrapBlur[1], dst = rigRimWrapBlur[0]
//       vertical 9-tap Gaussian;   rig_rim_wrap2.xy = (0, blur_q / blur0_h)
// blur_q = CineRigRimWrapBlur in QUARTER-res texels per tap (the same unit in
// both directions), so 1.0 is sigma ~ 2 quarter texels ~ 8 full-res px.  The
// final blurred scene is rigRimWrapBlur[0]; the composite (rigRimWrapF.glsl)
// binds it as rigRimWrapSrc.  The source is bound BILINEAR in every pass.
//
// Program: no features.  Vertex: deferred/postDeferredNoTCV.glsl.
// GL: blend off, depth off, viewport = the quarter-res target.

out vec4 frag_color;

in vec2 vary_fragcoord;

uniform sampler2D rigRimWrapSrc;
uniform vec4      rig_rim_wrap2;   // xy = uv step per tap for the Gaussian passes

void main()
{
    vec2 uv = vary_fragcoord;
    vec3 c;

#ifdef RIG_RIM_WRAP_DOWNSAMPLE
    // 4x4 box over the full-res texels this quarter texel covers: four bilinear
    // taps centred on the 2x2 blocks at (+-1, +-1) source px.
    vec2 t = 1.0 / vec2(textureSize(rigRimWrapSrc, 0));
    c  = textureLod(rigRimWrapSrc, uv + vec2(-t.x, -t.y), 0.0).rgb;
    c += textureLod(rigRimWrapSrc, uv + vec2( t.x, -t.y), 0.0).rgb;
    c += textureLod(rigRimWrapSrc, uv + vec2(-t.x,  t.y), 0.0).rgb;
    c += textureLod(rigRimWrapSrc, uv + vec2( t.x,  t.y), 0.0).rgb;
    c *= 0.25;
#else
    vec2 tstep = rig_rim_wrap2.xy;

    // sigma ~ 2 taps
    const float w0 = 0.2270270270;
    const float w1 = 0.1945945946;
    const float w2 = 0.1216216216;
    const float w3 = 0.0540540541;
    const float w4 = 0.0162162162;

    c  = textureLod(rigRimWrapSrc, uv, 0.0).rgb * w0;
    c += (textureLod(rigRimWrapSrc, uv + tstep * 1.0, 0.0).rgb + textureLod(rigRimWrapSrc, uv - tstep * 1.0, 0.0).rgb) * w1;
    c += (textureLod(rigRimWrapSrc, uv + tstep * 2.0, 0.0).rgb + textureLod(rigRimWrapSrc, uv - tstep * 2.0, 0.0).rgb) * w2;
    c += (textureLod(rigRimWrapSrc, uv + tstep * 3.0, 0.0).rgb + textureLod(rigRimWrapSrc, uv - tstep * 3.0, 0.0).rgb) * w3;
    c += (textureLod(rigRimWrapSrc, uv + tstep * 4.0, 0.0).rgb + textureLod(rigRimWrapSrc, uv - tstep * 4.0, 0.0).rgb) * w4;
#endif

    // NaN / inf guard: the HDR scene can carry fireflies the blur would smear
    if (any(isnan(c)) || any(isinf(c)))
    {
        c = vec3(0.0);
    }
    frag_color = vec4(max(c, vec3(0.0)), 1.0);
}
