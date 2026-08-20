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
        vec4 near_split = shadow_clip*-0.75;
        vec4 far_split = shadow_clip*-1.25;
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

        vec4 near_split = shadow_clip*-0.75;
        vec4 far_split = shadow_clip*-1.25;
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

float nonpcfShadowAtPos(vec4 pos_world, vec2 pos_screen)
{
#if defined(SUN_SHADOW)
    // BD: no shadow_clip.w early-out - volumetrics must not fade where the
    // shadow maps end or the effect becomes draw-distance dependent
    {
        vec4 near_split = shadow_clip*-0.75;
        vec4 far_split = shadow_clip*-1.25;

        if (pos_world.z < near_split.z)
        {
            pos_world = shadow_matrix[3]*pos_world;
            return nonpcfShadow(shadowMap3, pos_world, pos_screen, shadow_res.x, shadow_bias);
        }
        else if (pos_world.z < near_split.y)
        {
            pos_world = shadow_matrix[2]*pos_world;
            return nonpcfShadow(shadowMap2, pos_world, pos_screen, shadow_res.x, shadow_bias);
        }
        else if (pos_world.z < near_split.x)
        {
            pos_world = shadow_matrix[1]*pos_world;
            return nonpcfShadow(shadowMap1, pos_world, pos_screen, shadow_res.x, shadow_bias);
        }
        else if (pos_world.z > far_split.x)
        {
            pos_world = shadow_matrix[0]*pos_world;
            return nonpcfShadow(shadowMap0, pos_world, pos_screen, shadow_res.x, shadow_bias);
        }
    }
#endif
    return 1.0;
}
