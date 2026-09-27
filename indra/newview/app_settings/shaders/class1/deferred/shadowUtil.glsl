/**
 * @file class1/deferred/shadowUtil.glsl
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
 */

uniform sampler2D   normalMap;

#if defined(SUN_SHADOW)
uniform sampler2DShadow shadowMap0;
uniform sampler2DShadow shadowMap1;
uniform sampler2DShadow shadowMap2;
uniform sampler2DShadow shadowMap3;
#endif

#if defined(SPOT_SHADOW)
uniform sampler2DShadow shadowMap4;
uniform sampler2DShadow shadowMap5;
// [BDMerge NSpot] extended projector shadow slots
uniform sampler2DShadow shadowMap6;
uniform sampler2DShadow shadowMap7;
uniform sampler2DShadow shadowMap8;
uniform sampler2DShadow shadowMap9;
uniform sampler2DShadow shadowMap10;
uniform sampler2DShadow shadowMap11;
uniform sampler2DShadow shadowMap12;
uniform sampler2DShadow shadowMap13;
#endif

uniform vec3 sun_dir;
uniform vec3 moon_dir;
uniform vec2 shadow_res;
uniform vec2 proj_shadow_res;
uniform mat4 shadow_matrix[14]; // [BDMerge NSpot] 4 sun + up to 10 spot
uniform vec4 shadow_clip;
// [ShadowDist P1] Control A: cascade blend width as a fraction of each split
// (stock 0.25 -> bands 0.75x .. 1.25x). Uploaded from the sampled pack's
// ShadowMeta (LLPipeline::uploadShadowUniforms); the initialiser keeps any
// program that never receives it stock. 1.0 - 0.25 and 1.0 + 0.25 are exact,
// so the default multiplies see the same operands as the old literals.
uniform float shadow_split_blend = 0.25;
// [ShadowDist P2] Control C: world-unit sun-shadow bias / receiver offset / soft
// kernel (design v2 s4, v3 s5, v4 s2.3+s5). shadow_units_mode comes from the
// sampled pack's ShadowMeta (0 = legacy fractions, 1 = world units), never from
// live settings; the tunables below are ignored unless it is 1. Initialisers =
// stock values, so a program never bound through bindDeferredShader stays stock.
uniform int   shadow_units_mode = 0;
uniform int   shadow_units_scope = 0;       // 0 = all cascades in world units, 1 = cascade 0 only (1-3 legacy)
uniform float shadow_bias_mm = 2.5;         // depth bias, millimetres (absolute; no altitude term)
uniform float shadow_offset_texels = 1.5;   // receiver offset along N, in this cascade's texels (x sin a)
uniform float shadow_soft_world_mm = 0.0;   // soft sun penumbra radius in mm; 0 = legacy texel-based kernel
uniform float shadow_slope_scale = 1.0;     // receiver-plane slope bias scale (1 = cover the sampled footprint exactly)
// per cascade: (k_depth, texel_x_coef, texel_y_coef, is_persp) -- ortho: d*k, texel = coef;
// perspective: d*k/w, texel_x = coef.y*w, texel_y = coef.z*w*w (first order). Uploaded on a
// stamp change whenever the maps were generated in units mode; only read when shadow_units_mode != 0.
uniform vec4  shadow_cascade_coef[4];
uniform vec4  shadow_res_cascade   = vec4(2048.0); // REAL width  of shadowMap0..3 (fixes shadow_res = cascade 0 for all)
uniform vec4  shadow_res_cascade_h = vec4(2048.0); // REAL height of shadowMap0..3 ([ShadowDist P2 fix] non-square maps)
// [P3] Control B: (active, feather_uv, depth_feather_uv, 0). Always 0 until the subject cascade lands.
uniform vec4  shadow_subject = vec4(0.0);
uniform float shadow_bias;
uniform float shadow_offset;
uniform float spot_shadow_bias;
uniform float spot_shadow_offset;
uniform mat4 inv_proj;
uniform vec2 screen_res;
uniform int sun_up_factor;

// [BDMerge Batch 2] Feature 1 - soft (contact-hardening + filled) shadows.
// All gated: soft_shadow_enable == 0 -> the classic hard 5-tap paths run
// byte-identically, so default behavior is unchanged.
uniform int   soft_shadow_enable; // 0 = classic hard shadows (default)
uniform float soft_shadow_scale;  // penumbra rate: texels of kernel growth per unit
                                  // receiver depth (light source-size term)
uniform float soft_shadow_max;    // hard cap on the penumbra kernel radius (texels)
uniform float soft_shadow_fill;   // ambient floor: shadowed regions lift toward this
                                  // instead of crushing to pure black (0 = no fill)
uniform int   soft_shadow_sun;    // also apply contact-hardening+fill to the sun

// [Vogel A/B] Runtime upgrade of the soft-shadow kernel. soft_shadow_vogel == 0
// keeps the fixed 12-tap Poisson disk below (the byte-identical baseline for the
// A/B). soft_shadow_vogel != 0 switches BOTH the sun and spot soft branches to a
// Vogel-disk PCF whose taps are rotated per pixel by a SPATIAL-ONLY hash
// (interleaved gradient noise on gl_FragCoord). A static rotation plus a higher
// tap count converts the fixed disk's wide-penumbra banding into fine grain that
// reads as smooth. There is deliberately NO per-frame / time term: an animated
// shadow dither would feed rolling noise into the ReShade TAAU / motion-vector
// chain (the specular-jitter failure class) and ghost on moving actors, so the
// rotation is a pure function of screen position.
uniform int   soft_shadow_vogel;  // 0 = fixed 12-tap Poisson (default), 1 = Vogel
uniform int   soft_shadow_taps;   // Vogel tap count (clamped to SOFT_SHADOW_VOGEL_MAX)
// Per projector-shadow slot. Zero is a strict sentinel for the existing
// global behavior; positive values override only the penumbra growth scale.
uniform float spot_shadow_softness[10];

// 12-tap Poisson-ish disk for the widened soft-shadow PCF kernel.
const vec2 SOFT_SHADOW_DISK[12] = vec2[12](
    vec2( 0.0000,  0.0000),
    vec2( 0.5279,  0.1600),
    vec2(-0.3251,  0.4331),
    vec2(-0.4247, -0.3900),
    vec2( 0.2148, -0.5352),
    vec2( 0.7623, -0.4218),
    vec2(-0.7841, -0.1594),
    vec2(-0.1417,  0.8531),
    vec2( 0.5560,  0.7139),
    vec2( 0.9330,  0.2100),
    vec2(-0.6521,  0.5928),
    vec2( 0.0730, -0.9420)
);

const vec2 SPOT_BLOCKER_SEARCH_DISK[4] = vec2[4](
    vec2( 0.7071,  0.7071), vec2(-0.7071,  0.7071),
    vec2(-0.7071, -0.7071), vec2( 0.7071, -0.7071));

// [Vogel A/B] Constant upper bound on the Vogel tap loop so it stays
// constant-bounded (and unrollable); soft_shadow_taps selects how many of these
// samples are actually summed. Golden angle (radians) drives the spiral.
const int   SOFT_SHADOW_VOGEL_MAX   = 32;
const float SOFT_SHADOW_GOLDEN_ANGLE = 2.3999632;

// Cinematic Lighting 2.0 PCSS-lite blocker search.  A comparison sampler does
// not expose raw blocker depth, so estimate receiver-minus-blocker separation by
// summing shadow-test results across several evenly spaced depth deltas at each
// of four nearby taps: the deeper an occluder sits toward the light, the more
// tests report occlusion, so the normalized sum is a CONTINUOUS separation proxy
// (0 at contact -> 1 far).  This replaces the earlier 5-level quantization, whose
// hard bands produced visible penumbra-width contours on slanted receivers.
// Moving an occluder onto the receiver still collapses the penumbra.  Spatial
// only; no time-dependent noise is used.
float estimateSpotBlockerGap(sampler2DShadow shadowMap, vec3 stc,
                             float source_softness)
{
    vec2 search_texel = (1.5 + min(max(source_softness, 0.0), 8.0) * 0.5) /
                        proj_shadow_res;
    const int   GAP_STEPS = 6;
    const float GAP_MAX_DELTA = 0.20;
    float best_gap = 0.0;
    for (int i = 0; i < 4; ++i)
    {
        vec2 uv = stc.xy + SPOT_BLOCKER_SEARCH_DISK[i] * search_texel;
        if (texture(shadowMap, vec3(uv, stc.z)) < 0.5)
        {
            float sep = 0.0;
            for (int s = 1; s <= GAP_STEPS; ++s)
            {
                float delta = GAP_MAX_DELTA * (float(s) / float(GAP_STEPS));
                if (texture(shadowMap, vec3(uv, max(stc.z - delta, 0.0))) < 0.5)
                    sep += 1.0;
            }
            best_gap = max(best_gap, sep / float(GAP_STEPS));
        }
    }
    return best_gap;
}

float pcfShadow(sampler2DShadow shadowMap, vec3 norm, vec4 stc, float bias_mul, vec2 pos_screen, vec3 light_dir)
{
#if defined(SUN_SHADOW)
    float offset = shadow_bias * bias_mul;
    stc.xyz /= stc.w;
    stc.z += offset * 2.0;

    // [BDMerge Batch 2] Optional soft (contact-hardening + filled) sun path.
    // Gated by BOTH soft_shadow_enable and soft_shadow_sun so the default is the
    // byte-identical classic 5-tap kernel below.
    if (soft_shadow_enable != 0 && soft_shadow_sun != 0)
    {
        float pr = clamp(1.0 + soft_shadow_scale * clamp(stc.z, 0.0, 1.0),
                         1.0, max(soft_shadow_max, 1.0));
        vec2 texel = pr / shadow_res;
        float shadow = 0.0;

        if (soft_shadow_vogel != 0)
        {
            // [Vogel A/B] Vogel-disk PCF over the SAME penumbra radius (texel).
            // Per-pixel base rotation phi = interleaved gradient noise on
            // gl_FragCoord.xy - SPATIAL ONLY, no frame/time term, so the dither
            // is stable under the ReShade temporal chain. gl_FragCoord.xy (not
            // pos_screen) is used because pos_screen's units differ per caller
            // (0..1 UV here, world xy in the spot path); gl_FragCoord is always
            // window pixels, exactly what IGN expects for true per-pixel grain.
            int taps = clamp(soft_shadow_taps, 1, SOFT_SHADOW_VOGEL_MAX);
            float inv_n = 1.0 / float(taps);
            float phi = 6.2831853 * fract(52.9829189 *
                        fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
            for (int i = 0; i < SOFT_SHADOW_VOGEL_MAX; ++i)
            {
                if (i >= taps) break;
                float r = sqrt((float(i) + 0.5) * inv_n);
                float theta = float(i) * SOFT_SHADOW_GOLDEN_ANGLE + phi;
                vec2 o = r * vec2(cos(theta), sin(theta)) * texel;
                shadow += texture(shadowMap, vec3(stc.xy + o, stc.z));
            }
            shadow *= inv_n;
        }
        else
        {
            float jit = fract(pos_screen.y * shadow_res.y);
            for (int i = 0; i < 12; ++i)
            {
                vec2 o = SOFT_SHADOW_DISK[i] * texel;
                o.x += (jit - 0.5) * texel.x;
                shadow += texture(shadowMap, vec3(stc.xy + o, stc.z));
            }
            shadow /= 12.0;
        }
        // [BDMerge fix] Same partial-visibility guard as the spot path below: the
        // fill floor must never lift a FULLY occluded pixel (e.g. a room interior),
        // or the sun leaks through walls wherever soft sun shadows are enabled.
        if (soft_shadow_fill > 0.0 && shadow > 0.0)
            shadow = mix(soft_shadow_fill, 1.0, shadow);
        return clamp(shadow, 0.0, 1.0);
    }

    stc.x = floor(stc.x*shadow_res.x + fract(pos_screen.y*shadow_res.y))/shadow_res.x; // add some chaotic jitter to X sample pos according to Y to disguise the snapping going on here
    float cs = texture(shadowMap, stc.xyz);
    float shadow = cs * 4.0;
    shadow += texture(shadowMap, stc.xyz+vec3( 1.5/shadow_res.x,  0.5/shadow_res.y, 0.0));
    shadow += texture(shadowMap, stc.xyz+vec3( 0.5/shadow_res.x, -1.5/shadow_res.y, 0.0));
    shadow += texture(shadowMap, stc.xyz+vec3(-1.5/shadow_res.x, -0.5/shadow_res.y, 0.0));
    shadow += texture(shadowMap, stc.xyz+vec3(-0.5/shadow_res.x,  1.5/shadow_res.y, 0.0));
    return clamp(shadow * 0.125, 0.0, 1.0);
#else
    return 1.0;
#endif
}

// [ShadowDist P2] Control C sun kernel: same tap patterns as pcfShadow (classic
// 5-tap / Poisson / Vogel, same fill guard) but
//  - depth bias in world metres, converted per cascade and (perspective) per
//    receiver: stc.z -= bias * k [/ w], slope-scaled by the kernel radius
//    actually sampled so coarse cascades stay acne-free (v4 s5.1),
//  - separate X/Y texel sizes in metres at this receiver (v3 s5.3),
//  - soft penumbra optionally in world mm (same softness in every cascade),
//  - snap / offsets / jitter with THIS cascade's real resolution (v2 s4.5).
// No shadow_bias, no altitude term. Legacy pcfShadow above is untouched.
// plane_dd = (|dd/du|, |dd/dv|): the receiver plane's depth slope in MAP space (stc.z
// per stc.x / stc.y), and cover_stc = how far (in stc.z) the normal-offset sample point
// already sits toward the light off that plane -- both from sunBandC. res = (w, h) of
// THIS map. [ShadowDist P2 fix2] everything here is in map units: no metre conversion
// and no ortho / perspective special case in the bias itself.
float pcfShadowC(sampler2DShadow shadowMap, vec4 stc, vec4 coef, vec2 res, vec2 plane_dd, float cover_stc, vec2 pos_screen)
{
#if defined(SUN_SHADOW)
    float w = max(stc.w, 1e-4);
    stc.xyz /= w;
    bool  persp   = coef.w > 0.5;
    vec2  texel_m = persp ? vec2(coef.y * w, coef.z * w * w) : coef.yz;   // metres per texel (x, y), for the world-mm penumbra
    texel_m = max(texel_m, vec2(1e-6));

    bool  soft = (soft_shadow_enable != 0 && soft_shadow_sun != 0);
    float cap  = max(soft_shadow_max, 1.0);
    vec2  pr   = vec2(1.0);
    if (soft)
    {
        pr = (shadow_soft_world_mm > 0.0)
           ? clamp(vec2(0.001 * shadow_soft_world_mm) / texel_m, vec2(1.0), vec2(cap))
           : vec2(clamp(1.0 + soft_shadow_scale * clamp(stc.z, 0.0, 1.0), 1.0, cap));
    }

    // [ShadowDist P2 fix3, Codex r3] PER-TAP receiver-plane depth. plane_dd is the SIGNED
    // map-space slope of the receiver plane (stc.z per stc.x / stc.y, from sunBandC); every
    // tap at uv offset o compares against z_ref = stc.z + dot(plane_dd, o), i.e. the sample
    // depth follows the receiver's own plane across the kernel, so the bias no longer has
    // to span the footprint. What remains to cover is the hardware bilinear compare, which
    // reads texel CENTRES up to one full texel from the lookup (floor(coord - 0.5)
    // neighbour selection): one texel of slope per axis, times RenderShadowSlopeScale,
    // minus the normal-offset credit, floored by the mm bias. A light-facing receiver on a
    // perspective map (dd != 0 although its physical slope is 0) thus pays one texel of
    // map-depth slope instead of the whole footprint -- thin contact shadows survive.
    float allow_stc = shadow_slope_scale * (abs(plane_dd.x) / res.x + abs(plane_dd.y) / res.y);
    float floor_stc = persp ? (0.001 * shadow_bias_mm * coef.x / w) : (0.001 * shadow_bias_mm * coef.x);   // mm -> stc.z
    float bias_stc  = max(floor_stc, allow_stc - cover_stc);
    stc.z -= bias_stc;   // toward the light (legacy sign convention); per-tap slope added below

    if (soft)
    {
        vec2 texel = pr / res;
        float shadow = 0.0;
        if (soft_shadow_vogel != 0)
        {
            int taps = clamp(soft_shadow_taps, 1, SOFT_SHADOW_VOGEL_MAX);
            float inv_n = 1.0 / float(taps);
            float phi = 6.2831853 * fract(52.9829189 *
                        fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
            for (int i = 0; i < SOFT_SHADOW_VOGEL_MAX; ++i)
            {
                if (i >= taps) break;
                float r = sqrt((float(i) + 0.5) * inv_n);
                float theta = float(i) * SOFT_SHADOW_GOLDEN_ANGLE + phi;
                vec2 o = r * vec2(cos(theta), sin(theta)) * texel;
                shadow += texture(shadowMap, vec3(stc.xy + o, stc.z + dot(plane_dd, o)));
            }
            shadow *= inv_n;
        }
        else
        {
            float jit = fract(pos_screen.y * res.y);
            for (int i = 0; i < 12; ++i)
            {
                vec2 o = SOFT_SHADOW_DISK[i] * texel;
                o.x += (jit - 0.5) * texel.x;   // the jitter is part of this tap's offset, so it is tracked too
                shadow += texture(shadowMap, vec3(stc.xy + o, stc.z + dot(plane_dd, o)));
            }
            shadow /= 12.0;
        }
        if (soft_shadow_fill > 0.0 && shadow > 0.0)
            shadow = mix(soft_shadow_fill, 1.0, shadow);
        return clamp(shadow, 0.0, 1.0);
    }

    // [ShadowDist P2 fix2, Opus] UNSNAPPED 5-tap kernel in units mode: the legacy
    // floor(stc.x*res + jitter) x-snap disguises its own quantisation but costs a whole
    // texel of extra receiver-plane bias; bilinear compares at the exact position are
    // smooth on their own. [fix3] each tap follows the receiver plane.
    vec2 o1 = vec2( 1.5 / res.x,  0.5 / res.y);
    vec2 o2 = vec2( 0.5 / res.x, -1.5 / res.y);
    vec2 o3 = vec2(-1.5 / res.x, -0.5 / res.y);
    vec2 o4 = vec2(-0.5 / res.x,  1.5 / res.y);
    float shadow = texture(shadowMap, stc.xyz) * 4.0;
    shadow += texture(shadowMap, vec3(stc.xy + o1, stc.z + dot(plane_dd, o1)));
    shadow += texture(shadowMap, vec3(stc.xy + o2, stc.z + dot(plane_dd, o2)));
    shadow += texture(shadowMap, vec3(stc.xy + o3, stc.z + dot(plane_dd, o3)));
    shadow += texture(shadowMap, vec3(stc.xy + o4, stc.z + dot(plane_dd, o4)));
    return clamp(shadow * 0.125, 0.0, 1.0);
#else
    return 1.0;
#endif
}

// [ShadowDist P2] One sun cascade band in the units/subject path (v4 s2.3).
// units == false (RenderShadowUnitsScope == 1, cascades 1-3): the exact legacy
// result -- world-metre L offset + pcfShadow (normalised bias incl. altitude,
// shadow_res snap); the caller also selected/weighted that band with the legacy
// (offset) receiver z. units == true: normal offset in THIS cascade's texels,
// scaled by sin(a) (v3 s5.2), then the receiver plane is carried into MAP space
// for the slope bias ([ShadowDist P2 fix2], see below) and pcfShadowC samples.
float sunBandC(sampler2DShadow shadowMap, mat4 smat, vec4 coef, vec2 res, bool units,
               vec3 pos, vec3 norm, float NdotL, float sin_a, vec2 pos_screen, vec3 L)
{
#if defined(SUN_SHADOW)
    if (!units)
    {
        vec3 p = pos + L * (1.0 - NdotL) * shadow_offset * 2.0;
        return pcfShadow(shadowMap, norm, smat * vec4(p, 1.0), 1.0, pos_screen, L);
    }
    vec4  lp0 = smat * vec4(pos, 1.0);                 // un-offset projection
    // [ShadowDist P2 fix3] w <= 0 (receiver behind a perspective map's projection origin)
    // is OUTSIDE the map: lit, per the existing outside-the-map convention -- never
    // clamped into a bogus projection. Ortho maps have w == 1 and never take this exit.
    if (lp0.w <= 1e-4)
    {
        return 1.0;
    }
    float w0  = lp0.w;
    vec3  st0 = lp0.xyz / w0;
    vec2  tm  = (coef.w > 0.5) ? vec2(coef.y * w0, coef.z * w0 * w0) : coef.yz;
    vec4  lp  = smat * vec4(pos + norm * (shadow_offset_texels * max(tm.x, tm.y) * sin_a), 1.0);
    if (lp.w <= 1e-4)
    {
        return 1.0;
    }
    vec3  st1 = lp.xyz / lp.w;

    // [ShadowDist P2 fix2, Codex P1 / Opus] Receiver plane in MAP space. With
    // stc = (ru.p, rv.p, rz.p) / (rw.p) (+ constants; smat is column-major, row r of the
    // linear part is (smat[0][r], smat[1][r], smat[2][r])), the Jacobian d(stc)/dp at the
    // receiver has rows g_u = (ru - u*rw)/w, g_v = (rv - v*rw)/w, g_d = (rz - d*rw)/w.
    // The map-space TANGENTS (camera-space step that moves exactly one unit of u, v or d
    // and nothing else) are the columns of its inverse, t_u = cross(g_v, g_d)/det (cyclic).
    // det cancels in every ratio below, so it is never divided by: np = det * n' with
    // n' = (N.t_u, N.t_v, N.t_d), and the plane N.dp = 0 becomes n'_u du + n'_v dv + n'_d dd = 0
    // -> dd/du = -n'_u/n'_d, dd/dv = -n'_v/n'_d. Exact for ortho AND perspective maps (this
    // is the true inverse map, not the gradient direction), first order only in the
    // linearisation at the receiver. For an ortho map rw = 0 and it reduces to the
    // light-space normal decomposition.
    vec3 ru = vec3(smat[0][0], smat[1][0], smat[2][0]);
    vec3 rv = vec3(smat[0][1], smat[1][1], smat[2][1]);
    vec3 rz = vec3(smat[0][2], smat[1][2], smat[2][2]);
    vec3 rw = vec3(smat[0][3], smat[1][3], smat[2][3]);
    vec3 gu = (ru - st0.x * rw) / w0;
    vec3 gv = (rv - st0.y * rw) / w0;
    vec3 gd = (rz - st0.z * rw) / w0;
    vec3 np = vec3(dot(norm, cross(gv, gd)), dot(norm, cross(gd, gu)), dot(norm, cross(gu, gv)));
    // [fix3] SIGNED slopes dd/du = -n'_u/n'_d, dd/dv = -n'_v/n'_d so pcfShadowC can follow
    // the plane per tap. Grazing cap: |n'_d| >= 1/8 |n'| bounds the slope magnitudes at 8
    // (the plane is then >= 83 deg from the light) while keeping the sign. Above that,
    // normal-mapped pixels with N.L ~ 0 inside a shadow would otherwise receive tens of
    // texels of bias and read lit; the direct light term there is < 12%, so the residual
    // acne risk is the lesser evil. A degenerate Jacobian (np == 0) yields no slope term:
    // the mm floor alone applies.
    float npz   = np.z;
    bool  np_ok = abs(npz) > 1e-20;
    float npz_c = max(abs(npz), 0.125 * length(np)) * (npz < 0.0 ? -1.0 : 1.0);
    vec2  plane_dd = np_ok ? (-np.xy / npz_c) : vec2(0.0);
    // How far the normal-offset point already sits toward the light off the receiver
    // plane, in stc.z: the plane through the receiver, evaluated at the offset sample's
    // (u, v), minus the offset sample's own d. Measured in map space, so it is exact for
    // both projections; clamped at 0 (an offset that lands deeper never adds margin).
    float cover_stc = np_ok ? max(-dot(np, st1 - st0) / npz, 0.0) : 0.0;
    return pcfShadowC(shadowMap, lp, coef, res, plane_dd, cover_stc, pos_screen);
#else
    return 1.0;
#endif
}

// [ShadowDist P2] world units for cascade i? (scope 1 keeps cascades 1-3 legacy)
bool sunUnitsCascade(int i)
{
    return (shadow_units_mode != 0) && ((shadow_units_scope == 0) || (i == 0));
}

float pcfSpotShadow(sampler2DShadow shadowMap, vec4 stc, float bias_scale, vec2 pos_screen, float projector_softness)
{
#if defined(SPOT_SHADOW)
    stc.xyz /= stc.w;
    stc.z += spot_shadow_bias * bias_scale;

    // [BDMerge Batch 2 / Cinematic Lighting 2.0] Soft projector shadows.
    // A bounded blocker search estimates receiver-minus-occluder separation,
    // then scales that gap by the per-light source-size term (the per-projector
    // override, or soft_shadow_scale). Sharp at contact, softer as the receiver
    // separates from the blocker. Plus an ambient fill floor so shadowed pixels
    // never crush to pure black. Gated: soft_shadow_enable == 0 falls through to
    // the byte-identical classic 5-tap kernel below unless this projector has
    // an explicit positive softness override.
    if (soft_shadow_enable != 0 || projector_softness > 0.0)
    {
        float softness = projector_softness > 0.0
                       ? projector_softness : soft_shadow_scale;
        float blocker_gap = estimateSpotBlockerGap(
            shadowMap, stc.xyz, softness);
        // estimateSpotBlockerGap already returns a continuous 0..1 separation.
        float gap_scale = clamp(blocker_gap, 0.0, 1.0);
        float pr = clamp(1.0 + softness * gap_scale,
                          1.0, max(soft_shadow_max, 1.0));
        vec2 texel = pr / proj_shadow_res;
        texel.y *= 1.5;
        float shadow = 0.0;

        if (soft_shadow_vogel != 0)
        {
            // [Vogel A/B] Vogel-disk PCF over the SAME (anisotropic) texel radius.
            // Same spatial-only per-pixel rotation as the sun path (interleaved
            // gradient noise on gl_FragCoord; no temporal term).
            int taps = clamp(soft_shadow_taps, 1, SOFT_SHADOW_VOGEL_MAX);
            float inv_n = 1.0 / float(taps);
            float phi = 6.2831853 * fract(52.9829189 *
                        fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
            for (int i = 0; i < SOFT_SHADOW_VOGEL_MAX; ++i)
            {
                if (i >= taps) break;
                float r = sqrt((float(i) + 0.5) * inv_n);
                float theta = float(i) * SOFT_SHADOW_GOLDEN_ANGLE + phi;
                vec2 o = r * vec2(cos(theta), sin(theta)) * texel;
                shadow += texture(shadowMap, vec3(stc.xy + o, stc.z));
            }
            shadow *= inv_n;
        }
        else
        {
            float jit = fract(pos_screen.y * 0.666666666);
            for (int i = 0; i < 12; ++i)
            {
                vec2 o = SOFT_SHADOW_DISK[i] * texel;
                o.x += (jit - 0.5) * texel.x;
                shadow += texture(shadowMap, vec3(stc.xy + o, stc.z));
            }
            shadow /= 12.0;
        }
        // [BDMerge fix] Fill lifts only PARTIALLY lit pixels (>= 1 of the taps
        // saw the light - genuine penumbra / terminator regions on a lit subject).
        // A fully occluded pixel (all taps blocked, e.g. behind a solid wall) must
        // stay 0: an unconditional floor paints the projector's cookie - and the
        // volumetric beam, which shares this sampler - straight through solid
        // objects (reported in-world with soft shadows on, Fill default 0.15).
        if (soft_shadow_fill > 0.0 && shadow > 0.0)
            shadow = mix(soft_shadow_fill, 1.0, shadow);
        return clamp(shadow, 0.0, 1.0);
    }

    stc.x = floor(proj_shadow_res.x * stc.x + fract(pos_screen.y*0.666666666)) / proj_shadow_res.x; // snap

    float cs = texture(shadowMap, stc.xyz);
    float shadow = cs;

    vec2 off = 1.0/proj_shadow_res;
    off.y *= 1.5;

    shadow += texture(shadowMap, stc.xyz+vec3(off.x*2.0, off.y, 0.0));
    shadow += texture(shadowMap, stc.xyz+vec3(off.x, -off.y, 0.0));
    shadow += texture(shadowMap, stc.xyz+vec3(-off.x, off.y, 0.0));
    shadow += texture(shadowMap, stc.xyz+vec3(-off.x*2.0, -off.y, 0.0));
    return shadow*0.2;
#else
    return 1.0;
#endif
}

float sampleDirectionalShadow(vec3 pos, vec3 norm, vec2 pos_screen)
{
#if defined(SUN_SHADOW)
    // [ShadowDist P2] three-way structure (design v3 s5.4 / v4 s2.2):
    //   LEGACY  -- the original function body, verbatim (only the P1 blend
    //              substitution), when the maps were generated in legacy units
    //              and no subject column is active;
    //   UNITS   -- Control C world-unit path below (4-cascade layout);
    //   SUBJECT -- [P3] same path with the three-cascade layout + column.
    if (shadow_units_mode == 0 && shadow_subject.x < 0.5)
    {
    float shadow = 0.0f;
    vec3 light_dir = normalize((sun_up_factor == 1) ? sun_dir : moon_dir);

    float dp_directional_light = max(0.0, dot(norm.xyz, light_dir));
          dp_directional_light = clamp(dp_directional_light, 0.0, 1.0);

    vec3 shadow_pos = pos.xyz;

    vec3 offset = light_dir.xyz * (1.0 - dp_directional_light);

    shadow_pos += offset * shadow_offset * 2.0;

    vec4 spos = vec4(shadow_pos.xyz, 1.0);

    if (spos.z > -shadow_clip.w)
    {
        vec4 lpos;
        vec4 near_split = shadow_clip * -(1.0 - shadow_split_blend); // [ShadowDist P1] was *-0.75
        vec4 far_split = shadow_clip * -(1.0 + shadow_split_blend);  // [ShadowDist P1] was *-1.25
        vec4 transition_domain = near_split-far_split;
        float weight = 0.0;

        if (spos.z < near_split.z)
        {
            lpos = shadow_matrix[3]*spos;

            float w = 1.0;
            w -= max(spos.z-far_split.z, 0.0)/transition_domain.z;
            //w = clamp(w, 0.0, 1.0);
            float contrib = pcfShadow(shadowMap3, norm, lpos, 1.0, pos_screen, light_dir)*w;
            //if (contrib > 0)
            {
                shadow += contrib;
                weight += w;
            }
            shadow += max((pos.z+shadow_clip.z)/(shadow_clip.z-shadow_clip.w)*2.0-1.0, 0.0);
        }

        if (spos.z < near_split.y && spos.z > far_split.z)
        {
            lpos = shadow_matrix[2]*spos;

            float w = 1.0;
            w -= max(spos.z-far_split.y, 0.0)/transition_domain.y;
            w -= max(near_split.z-spos.z, 0.0)/transition_domain.z;
            //w = clamp(w, 0.0, 1.0);
            float contrib = pcfShadow(shadowMap2, norm, lpos, 1.0, pos_screen, light_dir)*w;
            //if (contrib > 0)
            {
                shadow += contrib;
                weight += w;
            }
        }

        if (spos.z < near_split.x && spos.z > far_split.y)
        {
            lpos = shadow_matrix[1]*spos;

            float w = 1.0;
            w -= max(spos.z-far_split.x, 0.0)/transition_domain.x;
            w -= max(near_split.y-spos.z, 0.0)/transition_domain.y;
            //w = clamp(w, 0.0, 1.0);
            float contrib = pcfShadow(shadowMap1, norm, lpos, 1.0, pos_screen, light_dir)*w;
            //if (contrib > 0)
            {
                shadow += contrib;
                weight += w;
            }
        }

        if (spos.z > far_split.x)
        {
            lpos = shadow_matrix[0]*spos;

            float w = 1.0;
            w -= max(near_split.x-spos.z, 0.0)/transition_domain.x;
            //w = clamp(w, 0.0, 1.0);
            float contrib = pcfShadow(shadowMap0, norm, lpos, 1.0, pos_screen, light_dir)*w;
            //if (contrib > 0)
            {
                shadow += contrib;
                weight += w;
            }
        }

        shadow /= weight;
    }
    else
    {
        return 1.0f; // lit beyond the far split...
    }
    //shadow = min(dp_directional_light,shadow);
    return shadow;
    } // ===== end LEGACY =====

    // ===== [ShadowDist P2] UNITS (and [P3] SUBJECT) path =====
    vec3  L     = normalize((sun_up_factor == 1) ? sun_dir : moon_dir);
    float NdotL = clamp(dot(norm, L), 0.0, 1.0);
    float sin_a = sqrt(max(1.0 - NdotL * NdotL, 0.0));

    // [ShadowDist P2 fix, Codex P2] selection coordinates: world-unit cascades select
    // and weight bands with the UN-offset receiver; cascades left on the legacy
    // kernel (RenderShadowUnitsScope == 1 -> cascades 1-3) keep the legacy
    // coordinate, i.e. the receiver offset along L, so their bands, weights and the
    // far guard are exactly the legacy ones.
    bool  u123    = (shadow_units_scope == 0);
    vec3  pos_leg = pos + L * (1.0 - NdotL) * shadow_offset * 2.0;
    float z123    = u123 ? pos.z : pos_leg.z;
    float z0      = pos.z;                          // cascade 0 is always world units here
    if (z123 <= -shadow_clip.w)
    {
        return 1.0; // lit beyond the far split (outer guard kept)
    }

    vec4 near_split = shadow_clip * -(1.0 - shadow_split_blend);
    vec4 far_split  = shadow_clip * -(1.0 + shadow_split_blend);
    vec4 transition_domain = near_split - far_split;

    float shadow = 0.0;
    float weight = 0.0;
    vec2  res1 = vec2(shadow_res_cascade.y, shadow_res_cascade_h.y);
    vec2  res2 = vec2(shadow_res_cascade.z, shadow_res_cascade_h.z);
    vec2  res3 = vec2(shadow_res_cascade.w, shadow_res_cascade_h.w);

    if (z123 < near_split.z)
    {
        float w = 1.0 - max(z123 - far_split.z, 0.0) / transition_domain.z;
        shadow += sunBandC(shadowMap3, shadow_matrix[3], shadow_cascade_coef[3], res3, u123,
                           pos, norm, NdotL, sin_a, pos_screen, L) * w;
        weight += w;
        shadow += max((pos.z + shadow_clip.z) / (shadow_clip.z - shadow_clip.w) * 2.0 - 1.0, 0.0); // legacy uses pos.z here too
    }
    if (z123 < near_split.y && z123 > far_split.z)
    {
        float w = 1.0 - max(z123 - far_split.y, 0.0) / transition_domain.y
                      - max(near_split.z - z123, 0.0) / transition_domain.z;
        shadow += sunBandC(shadowMap2, shadow_matrix[2], shadow_cascade_coef[2], res2, u123,
                           pos, norm, NdotL, sin_a, pos_screen, L) * w;
        weight += w;
    }
    // [P3 slot] subject mode: the cascade-1 band becomes `z123 > far_split.y` with only the
    // y-side transition and the cascade-0 band is skipped (three-cascade layout, v3 s5.4);
    // kept out of phase 2 so the inert variant does not double the inlined band code.
    // [ShadowDist P2 fix2, Opus] the cascade-1 band's cascade-0 side is selected and
    // weighted with z0 (the coordinate cascade 0 uses) so scope 1 can never open a gap
    // between the two coordinates when RenderShadowOffset is large and the sharp range
    // small; the y side keeps its own coordinate. Identical when scope == 0 (z0 == z123).
    if (z0 < near_split.x && z123 > far_split.y)
    {
        float w = 1.0 - max(z0 - far_split.x, 0.0) / transition_domain.x
                      - max(near_split.y - z123, 0.0) / transition_domain.y;
        shadow += sunBandC(shadowMap1, shadow_matrix[1], shadow_cascade_coef[1], res1, u123,
                           pos, norm, NdotL, sin_a, pos_screen, L) * w;
        weight += w;
    }
    if (z0 > far_split.x)
    {
        float w = 1.0 - max(near_split.x - z0, 0.0) / transition_domain.x;
        shadow += sunBandC(shadowMap0, shadow_matrix[0], shadow_cascade_coef[0],
                           vec2(shadow_res_cascade.x, shadow_res_cascade_h.x), true,
                           pos, norm, NdotL, sin_a, pos_screen, L) * w;
        weight += w;
    }
    shadow = (weight > 0.0) ? shadow / weight : 1.0;

    // [P3 slot] subject column: lp = shadow_subject_matrix * (pos + N offset), feathered
    //           XY/depth test, shadow = mix(shadow, pcfShadowC(shadowMap0, ...), sw)

    return shadow;
#else
    return 1.0;
#endif
}

float sampleSpotShadow(vec3 pos, vec3 norm, int index, vec2 pos_screen)
{
#if defined(SPOT_SHADOW)
    float shadow = 0.0f;
    pos += norm * spot_shadow_offset;

    vec4 spos = vec4(pos,1.0);
    if (spos.z > -shadow_clip.w)
    {
        vec4 lpos;

        vec4 near_split = shadow_clip * -(1.0 - shadow_split_blend); // [ShadowDist P1] was *-0.75
        vec4 far_split = shadow_clip * -(1.0 + shadow_split_blend);  // [ShadowDist P1] was *-1.25
        vec4 transition_domain = near_split-far_split;
        float weight = 0.0;

        {
            float w = 1.0;
            w -= max(spos.z-far_split.z, 0.0)/transition_domain.z;

            // KEEP IN SYNC: this 10-case GLSL dispatch MUST mirror
            // LLPipeline::spotShadowMapIndex(): spot slot s -> shadowMap[4 + s].
            if (index == 0)
            {
                lpos = shadow_matrix[4]*spos;
                shadow += pcfSpotShadow(shadowMap4, lpos, 0.8, spos.xy, spot_shadow_softness[0])*w;
            }
            else if (index == 1)
            {
                lpos = shadow_matrix[5]*spos;
                shadow += pcfSpotShadow(shadowMap5, lpos, 0.8, spos.xy, spot_shadow_softness[1])*w;
            }
            else if (index == 2)
            {
                lpos = shadow_matrix[6]*spos;
                shadow += pcfSpotShadow(shadowMap6, lpos, 0.8, spos.xy, spot_shadow_softness[2])*w;
            }
            else if (index == 3)
            {
                lpos = shadow_matrix[7]*spos;
                shadow += pcfSpotShadow(shadowMap7, lpos, 0.8, spos.xy, spot_shadow_softness[3])*w;
            }
            else if (index == 4)
            {
                lpos = shadow_matrix[8]*spos;
                shadow += pcfSpotShadow(shadowMap8, lpos, 0.8, spos.xy, spot_shadow_softness[4])*w;
            }
            else if (index == 5)
            {
                lpos = shadow_matrix[9]*spos;
                shadow += pcfSpotShadow(shadowMap9, lpos, 0.8, spos.xy, spot_shadow_softness[5])*w;
            }
            else if (index == 6)
            {
                lpos = shadow_matrix[10]*spos;
                shadow += pcfSpotShadow(shadowMap10, lpos, 0.8, spos.xy, spot_shadow_softness[6])*w;
            }
            else if (index == 7)
            {
                lpos = shadow_matrix[11]*spos;
                shadow += pcfSpotShadow(shadowMap11, lpos, 0.8, spos.xy, spot_shadow_softness[7])*w;
            }
            else if (index == 8)
            {
                lpos = shadow_matrix[12]*spos;
                shadow += pcfSpotShadow(shadowMap12, lpos, 0.8, spos.xy, spot_shadow_softness[8])*w;
            }
            else
            {
                lpos = shadow_matrix[13]*spos;
                shadow += pcfSpotShadow(shadowMap13, lpos, 0.8, spos.xy, spot_shadow_softness[9])*w;
            }
            weight += w;
            shadow += max((pos.z+shadow_clip.z)/(shadow_clip.z-shadow_clip.w)*2.0-1.0, 0.0);
        }

        shadow /= weight;
    }
    else
    {
        shadow = 1.0f;
    }
    return shadow;
#else
    return 1.0;
#endif
}

// [BDMerge G3.3 ConservativeShadow] Volume-march spot-shadow sampler for the
// AIRBORNE raymarch samples of projectorVolumetricF. The surface pipeline above
// (sampleSpotShadow -> pcfSpotShadow) is built for lit RECEIVER SURFACES and has
// four separate mechanisms that each read "lit" for a point that is actually
// inside an occluder's shadow volume:
//   1. receiver depth bias (spot_shadow_bias) pushes the compare depth past thin
//      occluders,
//   2. the wide PCF kernel averages lit+shadowed taps, so silhouette texels leak
//      partial light into fully-occluded space,
//   3. the soft-shadow fill floor lifts any partially-lit result toward
//      soft_shadow_fill,
//   4. the sun-cascade terms: an additive camera-distance fade keyed to
//      shadow_clip (which is the SUN cascade split vector, amplified by the /w
//      weight near the far split) and an unconditional "fully lit" for samples
//      beyond shadow_clip.w.
// For a mid-air sample the sum of those paints a bright filament straight through
// the CENTER of an opaque occluder when viewer, occluder and light line up. This
// sampler is deliberately conservative instead:
//   - no receiver bias (an airborne sample has no surface to acne against),
//   - a degenerate projection (w <= 1e-5) or a sample outside the shadow map's
//     [0,1] footprint returns OCCLUDED (0.0), never lit,
//   - depth refs beyond the shadow far plane are clamped to 1.0 (matching the
//     hardware ref-clamp the legacy path relies on) so the beam segment past the
//     projector's far clip still resolves against real occluders instead of
//     truncating,
//   - a tight 5-tap 1-texel plus-pattern PCF for edge antialiasing only,
//   - the leak clamp: a mostly-occluded CENTER (center < 0.5) stays black unless
//     a strong majority (>= 3 of 4) of its edge taps are lit - a true silhouette
//     edge. One or two leaking edge taps can never light an occluded sample,
//   - no camera-distance fade or far-clip auto-lit: the spot map is light-space
//     and valid regardless of camera distance (same rationale as the BD
//     nonpcfShadowAtPos note below).
float pcfSpotShadowConservative(sampler2DShadow shadowMap, vec4 stc)
{
#if defined(SPOT_SHADOW)
    if (stc.w <= 1e-5)
    {
        return 0.0; // at/behind the shadow camera plane - projection is invalid
    }
    stc.xyz /= stc.w;
    if (stc.x < 0.0 || stc.x > 1.0 ||
        stc.y < 0.0 || stc.y > 1.0 ||
        stc.z < 0.0)
    {
        return 0.0; // outside the map footprint / nearer than the shadow near plane
    }
    stc.z = min(stc.z, 1.0); // beyond-far ref clamps like the hardware compare

    float center = texture(shadowMap, stc.xyz);

    vec2  texel = 1.0 / proj_shadow_res;
    float e0 = texture(shadowMap, vec3(stc.xy + vec2( texel.x, 0.0), stc.z));
    float e1 = texture(shadowMap, vec3(stc.xy + vec2(-texel.x, 0.0), stc.z));
    float e2 = texture(shadowMap, vec3(stc.xy + vec2(0.0,  texel.y), stc.z));
    float e3 = texture(shadowMap, vec3(stc.xy + vec2(0.0, -texel.y), stc.z));
    float pcf = (center + e0 + e1 + e2 + e3) * 0.2;

    // Leak clamp: an occluded center with a minority of lit edge taps is the
    // exact signature of PCF bleed through a silhouette - clamp it to black
    // instead of letting it light the middle of the occluder's shadow volume.
    // [Review fix] The old `pcf < 0.30` test did NOT enforce that: center
    // occluded + TWO lit neighbors gives pcf = 0.4 and passed, preserving the
    // filament. Enforce a strict consensus rule on the neighbors themselves.
    // [Round-2 fix] The consensus must be a DISCRETE count of lit taps, not the
    // fractional sum: these taps are LINEARLY-FILTERED shadow compares, so
    // summing them against 3.0 measured coverage mass - center 0.49 + three 0.9
    // neighbors + one 0.0 sums to 2.7 and was wrongly blacked out, darkening a
    // genuinely lit penumbra. Count each edge tap as lit at >= 0.5 and require
    // COUNT >= 3: when the center tap is occluded (< 0.5) the sample stays
    // black unless at least 3 of the 4 edge taps are lit (a true silhouette
    // edge crossing) - one or two leaking edge taps still can never light an
    // occluded sample.
    float lit_neighbors = step(0.5, e0) + step(0.5, e1) + step(0.5, e2) + step(0.5, e3);
    if (center < 0.5 && lit_neighbors < 2.5) // < 2.5 == integer count <= 2
    {
        return 0.0;
    }
    return pcf;
#else
    return 0.0;
#endif
}

float sampleSpotShadowConservative(vec3 pos, int index)
{
#if defined(SPOT_SHADOW)
    // No norm * spot_shadow_offset receiver nudge and no shadow_clip camera-space
    // early-out/fade/weighting - just the sample projected straight into this
    // projector's own map.
    // KEEP IN SYNC: this 10-case GLSL dispatch MUST mirror
    // LLPipeline::spotShadowMapIndex(): spot slot s -> shadowMap[4 + s].
    vec4 spos = vec4(pos, 1.0);
    if (index == 0)
    {
        return pcfSpotShadowConservative(shadowMap4, shadow_matrix[4] * spos);
    }
    else if (index == 1)
    {
        return pcfSpotShadowConservative(shadowMap5, shadow_matrix[5] * spos);
    }
    else if (index == 2)
    {
        return pcfSpotShadowConservative(shadowMap6, shadow_matrix[6] * spos);
    }
    else if (index == 3)
    {
        return pcfSpotShadowConservative(shadowMap7, shadow_matrix[7] * spos);
    }
    else if (index == 4)
    {
        return pcfSpotShadowConservative(shadowMap8, shadow_matrix[8] * spos);
    }
    else if (index == 5)
    {
        return pcfSpotShadowConservative(shadowMap9, shadow_matrix[9] * spos);
    }
    else if (index == 6)
    {
        return pcfSpotShadowConservative(shadowMap10, shadow_matrix[10] * spos);
    }
    else if (index == 7)
    {
        return pcfSpotShadowConservative(shadowMap11, shadow_matrix[11] * spos);
    }
    else if (index == 8)
    {
        return pcfSpotShadowConservative(shadowMap12, shadow_matrix[12] * spos);
    }
    return pcfSpotShadowConservative(shadowMap13, shadow_matrix[13] * spos);
#else
    return 1.0;
#endif
}


// [BDMerge G3.2] Donor: Black Dragon (NiranV Dean) shadowUtil.glsl - cheap
// non-PCF cascade sampling for the volumetric raymarch (Tofu Buzzard lineage).
float nonpcfShadow(sampler2DShadow shadowMap, vec4 stc, vec2 pos_screen, float shad_res, float bias)
{
#if defined(SUN_SHADOW)
    float recip_shadow_res = 1.0 / shad_res;
    stc.xyz /= stc.w;
    stc.z += bias;

    stc.x = floor(stc.x*shad_res + fract(pos_screen.y)) * recip_shadow_res;

    float cs = texture(shadowMap, stc.xyz);
    float shadow = cs * 4.0;
    return shadow;
#else
    return 0.0;
#endif
}

// [ShadowDist P2] world-unit twin of nonpcfShadow for the volumetric raymarch
// (v3 s5.5): bias in metres converted once per cascade (no slope term -- an
// airborne sample has no receiver normal; applied ONCE like the legacy single
// `stc.z += bias`), and the snap uses THIS cascade's real resolution.
float nonpcfShadowC(sampler2DShadow shadowMap, vec4 stc, vec2 pos_screen, vec4 coef, float res)
{
#if defined(SUN_SHADOW)
    // [ShadowDist P2 fix4] w <= 0 = behind a perspective map's origin = outside the map:
    // lit (the in-band "lit" value of this sampler is cs * 4.0), consistent with sunBandC;
    // never clamped into a bogus projection.
    if (stc.w <= 1e-4)
    {
        return 4.0;
    }
    float w = stc.w;
    stc.xyz /= w;
    float bias_m = 0.001 * shadow_bias_mm;
    stc.z -= (coef.w > 0.5) ? (bias_m * coef.x / w) : (bias_m * coef.x);

    stc.x = floor(stc.x * res + fract(pos_screen.y)) / res;

    float cs = texture(shadowMap, stc.xyz);
    return cs * 4.0;
#else
    return 0.0;
#endif
}

float nonpcfShadowAtPos(vec4 pos_world, vec2 pos_screen)
{
#if defined(SUN_SHADOW)
    // BD: no shadow_clip.w early-out - volumetrics must not fade where the
    // shadow maps end or the effect becomes draw-distance dependent
    {
        vec4 near_split = shadow_clip * -(1.0 - shadow_split_blend); // [ShadowDist P1] was *-0.75
        vec4 far_split = shadow_clip * -(1.0 + shadow_split_blend);  // [ShadowDist P1] was *-1.25

        // [ShadowDist P2] per cascade: world-unit bias/res when sunUnitsCascade(i),
        // else the legacy statement verbatim. [P3] subject mode merges the two
        // innermost branches onto cascade 1.
        if (pos_world.z < near_split.z)
        {
            pos_world = shadow_matrix[3]*pos_world;
            if (sunUnitsCascade(3)) return nonpcfShadowC(shadowMap3, pos_world, pos_screen, shadow_cascade_coef[3], shadow_res_cascade.w);
            return nonpcfShadow(shadowMap3, pos_world, pos_screen, shadow_res.x, shadow_bias);
        }
        else if (pos_world.z < near_split.y)
        {
            pos_world = shadow_matrix[2]*pos_world;
            if (sunUnitsCascade(2)) return nonpcfShadowC(shadowMap2, pos_world, pos_screen, shadow_cascade_coef[2], shadow_res_cascade.z);
            return nonpcfShadow(shadowMap2, pos_world, pos_screen, shadow_res.x, shadow_bias);
        }
        else if (pos_world.z < near_split.x)
        {
            pos_world = shadow_matrix[1]*pos_world;
            if (sunUnitsCascade(1)) return nonpcfShadowC(shadowMap1, pos_world, pos_screen, shadow_cascade_coef[1], shadow_res_cascade.y);
            return nonpcfShadow(shadowMap1, pos_world, pos_screen, shadow_res.x, shadow_bias);
        }
        else if (pos_world.z > far_split.x)
        {
            pos_world = shadow_matrix[0]*pos_world;
            if (sunUnitsCascade(0)) return nonpcfShadowC(shadowMap0, pos_world, pos_screen, shadow_cascade_coef[0], shadow_res_cascade.x);
            return nonpcfShadow(shadowMap0, pos_world, pos_screen, shadow_res.x, shadow_bias);
        }
    }
#endif
    return 1.0;
}
