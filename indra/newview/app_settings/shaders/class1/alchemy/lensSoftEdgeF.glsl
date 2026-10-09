/**
 * @file lensSoftEdgeF.glsl
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

// [LensLooks] Soft-Edge / Sweet Spot composite (doc/LENS_LOOKS_CONTRACT.md,
// stage S3).  Draws PREMULTIPLIED over mRT->screen (blend ONE,
// ONE_MINUS_SRC_ALPHA; colour mask RGB only so the glow tag in alpha survives)
// after applyOnLensFilters' copy-back and before generateBloomHDR.  Inside the
// sweet spot the fragment is DISCARDED (the sharp scene stays untouched);
// outside, a blurred, optionally glowing / desaturated / tinted version of the
// scene fades in with the mask.
//
// Mask: full-frame UV (tile-aware for hi-res snapshots), distances in units of
// frame HEIGHT:
//   lens_softedge   x shape (0 circle, 1 oval, 2 band), y size, z aspect (oval
//                   x/y), w angle (rad)
//   lens_softedge2  xy centre (uv, y-up), z feather, w blur 0..1 (0 = Defocus
//                   mode: the blur comes from DoF via cofF.glsl and this pass
//                   only contributes glow / desaturate / tint)
//   lens_softedge3  x glow, y glow threshold (exposed luma), z desaturate 0..1,
//                   w tint amount 0..1
//   lens_softedge4  xyz tint (linear), w show mask (1 = debug: output the mask)
//   lens_softedge5  xy frame size (px, all tiles), zw this tile's offset (px)
//   lens_softedge6  x exposure scale (manual * auto) for the glow threshold,
//                   y mask gain (1; <1 fades the whole effect), zw reserved
// Blur source: the lazy 5-level lens pyramid (lensPyramid0..4 = 1/2 .. 1/32 of
// the screen, R11G11B10F, built with the bloom downsample programs, Karis on
// the first level), sampled BILINEAR with a 3x3 tent on two adjacent levels.
//
// Program: no features.  Vertex: deferred/postDeferredNoTCV.glsl.

out vec4 frag_color;

in vec2 vary_fragcoord;

uniform sampler2D lensPyramid0;
uniform sampler2D lensPyramid1;
uniform sampler2D lensPyramid2;
uniform sampler2D lensPyramid3;
uniform sampler2D lensPyramid4;
uniform sampler2D exposureMap;

uniform vec4 lens_softedge;
uniform vec4 lens_softedge2;
uniform vec4 lens_softedge3;
uniform vec4 lens_softedge4;
uniform vec4 lens_softedge5;
uniform vec4 lens_softedge6;

const vec3 LENS_LUMA = vec3(0.2126, 0.7152, 0.0722);

// 3x3 tent (1 2 1 / 2 4 2 / 1 2 1) / 16 on one pyramid level.  textureLod:
// this runs after the sweet-spot discard (non-uniform flow); the pyramid
// targets have one level anyway.
vec3 lensTent(sampler2D s, vec2 uv)
{
    vec2 t = 1.0 / vec2(textureSize(s, 0));
    vec3 c  = textureLod(s, uv, 0.0).rgb * 4.0;
    c += (textureLod(s, uv + vec2( t.x, 0.0), 0.0).rgb + textureLod(s, uv + vec2(-t.x, 0.0), 0.0).rgb +
          textureLod(s, uv + vec2(0.0,  t.y), 0.0).rgb + textureLod(s, uv + vec2(0.0, -t.y), 0.0).rgb) * 2.0;
    c += (textureLod(s, uv + vec2( t.x,  t.y), 0.0).rgb + textureLod(s, uv + vec2(-t.x,  t.y), 0.0).rgb +
          textureLod(s, uv + vec2( t.x, -t.y), 0.0).rgb + textureLod(s, uv + vec2(-t.x, -t.y), 0.0).rgb);
    return c * (1.0 / 16.0);
}

// Sampler indexing must be constant: branch on the level.
vec3 lensLevel(int level, vec2 uv)
{
    if (level <= 0) return lensTent(lensPyramid0, uv);
    if (level == 1) return lensTent(lensPyramid1, uv);
    if (level == 2) return lensTent(lensPyramid2, uv);
    if (level == 3) return lensTent(lensPyramid3, uv);
    return lensTent(lensPyramid4, uv);
}

// Soft-spot mask 0 (inside) .. 1 (fully outside), full-frame, tile-aware.
float lensSoftEdgeMask(vec2 frag_px)
{
    vec2  frame  = max(lens_softedge5.xy, vec2(1.0));
    vec2  uv_f   = (frag_px + lens_softedge5.zw) / frame;
    float aspect = frame.x / frame.y;

    vec2 p = uv_f - lens_softedge2.xy;
    p.x *= aspect;                                   // frame-height units
    float ang = lens_softedge.w;
    float ca = cos(ang), sa = sin(ang);
    p = vec2(ca * p.x + sa * p.y, -sa * p.x + ca * p.y);

    float size  = max(lens_softedge.y, 1.0e-3);
    int   shape = int(lens_softedge.x + 0.5);
    float dist;
    if (shape == 2)
    {
        dist = abs(p.y) / size;                      // band: sharp strip through the centre
    }
    else if (shape == 1)
    {
        dist = length(p / vec2(max(lens_softedge.z, 1.0e-3), 1.0)) / size;
    }
    else
    {
        dist = length(p) / size;
    }
    float feather = max(lens_softedge2.z, 1.0e-3);
    return smoothstep(1.0, 1.0 + feather, dist);
}

void main()
{
    float m = lensSoftEdgeMask(gl_FragCoord.xy) * clamp(lens_softedge6.y, 0.0, 1.0);

    if (lens_softedge4.w > 0.5)
    {
        frag_color = vec4(vec3(m), 1.0);              // debug: show the mask (host: blend off)
        return;
    }

    if (m <= 1.0e-4)
    {
        discard;                                      // inside the sweet spot: sharp scene stays
    }

    vec2  uv   = vary_fragcoord;
    float blur = clamp(lens_softedge2.w, 0.0, 1.0);

    // --- blur: two adjacent pyramid levels, tent-filtered, level = blur*m*4 --
    float lvl_f = blur * m * 4.0;
    int   lvl0  = int(floor(lvl_f));
    float lvl_t = lvl_f - float(lvl0);
    vec3  c     = mix(lensLevel(lvl0, uv), lensLevel(min(lvl0 + 1, 4), uv), lvl_t);

    // --- glow from the coarse levels, soft knee -----------------------------
    // Kept OUT of the covered colour and emitted as a separate additive term
    // (review P1): under the premultiplied ONE / ONE_MINUS_SRC_ALPHA blend
    // `glow_add` lands on the frame as-is in BOTH modes, scaled by m once (not
    // m^2, and not gated by the Defocus coverage).  Levels 3/4 (1/16, 1/32)
    // are already wide blurs, so single bilinear taps suffice (review P2-8).
    vec3  glow_add = vec3(0.0);
    float glow_amt = max(lens_softedge3.x, 0.0);
    if (glow_amt > 0.0)
    {
        vec3  g   = max(textureLod(lensPyramid3, uv, 0.0).rgb, textureLod(lensPyramid4, uv, 0.0).rgb);
        float gl  = dot(g, LENS_LUMA) * texture(exposureMap, vec2(0.5)).r *
                    max(lens_softedge6.x, 1.0e-4);
        float thr = max(lens_softedge3.y, 1.0e-3);
        float knee = thr * 0.5;
        float soft = clamp(gl - thr + knee, 0.0, 2.0 * knee);
        soft = soft * soft / (4.0 * knee + 1.0e-5);
        float w = max(gl - thr, soft) / max(gl, 1.0e-4);
        glow_add = max(g * w * glow_amt * m, vec3(0.0));
    }

    // --- desaturate and tint toward the edge ---------------------------------
    float lum = dot(c, LENS_LUMA);
    c = mix(c, vec3(lum), clamp(lens_softedge3.z, 0.0, 1.0) * m);
    c *= mix(vec3(1.0), max(lens_softedge4.rgb, vec3(0.0)), clamp(lens_softedge3.w, 0.0, 1.0) * m);

    // --- coverage: Diffusion replaces the edge by m; Defocus (blur 0) keeps
    //     the DoF-blurred scene and only covers by the desaturate / tint amount
    float cover = (blur > 0.0) ? 1.0 : clamp(max(lens_softedge3.z, lens_softedge3.w), 0.0, 1.0);
    float a = m * cover;

    c = max(c, vec3(0.0));
    // Tap budget outside the spot: 2 x 9 (blur tents) + 2 (glow) = 20 bilinear taps.
    frag_color = vec4(c * a + glow_add, a);           // premultiplied colour + additive glow
}
