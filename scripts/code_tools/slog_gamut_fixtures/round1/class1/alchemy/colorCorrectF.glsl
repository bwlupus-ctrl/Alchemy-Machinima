/**
 * @file colorCorrectF.glsl
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Rye <rye@alchemyviewer.org>
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

in vec2 vary_fragcoord;
out vec4 frag_color;

// =============================================================================
// Uniforms
// =============================================================================

// ---- Input ------------------------------------------------------------------
uniform sampler2D diffuseRect;
uniform sampler2D depthMap;
#if defined(AL_SENSOR_OPTICS) || defined(AL_SENSOR_GODRAYS)
uniform sampler2D diffuseMap; // unexposed flare source, distinct from exposed diffuseRect
#endif
#ifdef LEGACY_GAMMA
uniform float gamma;
#endif

#ifdef BLOOM_COMPOSITE
// Pre-tonemap bloom pyramid (top mip). Read once here so the blend no longer
// needs a standalone pass over the scene buffer. With BLOOM_HALATION the
// halation signal rides in the alpha channel; otherwise the pyramid is RGB-only.
uniform sampler2D bloomMap;
uniform float bloom_strength;
#ifdef BLOOM_HALATION
uniform float halation_strength;
uniform vec3  halation_tint;
#endif
#endif

// =============================================================================
// Forward Declarations
// =============================================================================

vec3 linear_to_srgb(vec3 cl);
vec3 srgb_to_linear(vec3 cl);
vec3 clampHDRRange(vec3 color);

#ifdef TONEMAP
vec3 applyExposure(vec3 color);
vec3 applyToneMap(vec3 color);
#endif

// Shared with tonemapUtilF. 8 == S-Log3 capture: a raw, ungraded camera-style
// transfer, so the color-grade stages below are bypassed for it. Declared
// unconditionally (a COLOR_GRADE-only variant has no TONEMAP define); when the
// pass has no tonemap the CPU leaves this at its default 0, so grading applies.
uniform int tonemap_type;
#ifdef AL_SENSOR_GODRAYS
uniform vec2 tonemap_grade;
uniform int tonemap_slog_gamut;
vec3 decode_slog_family(vec3 code);
#endif

#ifdef COLOR_GRADE
vec3 applyWhiteBalance(vec3 diff);
vec3 applyLiftGammaGain(vec3 diff);
vec3 applyLUTGrading(vec3 diff);
vec3 applySplitToning(vec3 diff);
vec3 applyBlackWhitePoint(vec3 diff);
vec3 applyBrightnessContrast(vec3 diff);
vec3 applyShadowHighlightRecovery(vec3 diff);
vec3 applySaturation(vec3 diff);
vec3 applyVibrance(vec3 diff);
vec3 applyHueShift(vec3 diff);
vec3 applyChannelCurves(vec3 diff);
#endif

#ifdef HAS_POST_EFFECTS
vec3 computeLensFlare(sampler2D diffuse, sampler2D depth, vec2 uv);
vec4 applyChromaticAberration(sampler2D tex, vec2 uv);
// Note: Graduated ND + Polarizer now run in a dedicated pre-pass
// (LLPipeline::applyOnLensFilters) ahead of bloom/flare, not here.
#endif

#ifdef DITHER
vec3 applyDither(vec3 color, vec2 fragCoord);
#endif

// =============================================================================
// Helpers
// =============================================================================

#ifdef LEGACY_GAMMA
vec3 legacyGamma(vec3 color) {
    vec3 c = 1. - clamp(color, vec3(0.), vec3(1.));
    c = 1. - pow(c, vec3(gamma)); // s/b inverted already CPU-side

    return c;
}
#endif

vec3 cameraTransfer(vec3 color)
{
#ifdef COLOR_GRADE
    // White balance and lift/gamma/gain run in linear light, between exposure
    // and tonemap, so chromatic adaptation and the three-way grade act on
    // physically meaningful HDR values rather than rolled-off display ones.
    // Bypassed for S-Log3 capture: grade neutrally, correct in post.
    if (tonemap_type != 8)
    {
        color = applyWhiteBalance(color);
        color = applyLiftGammaGain(color);
    }
#endif

#ifdef TONEMAP
    color = applyToneMap(color);
#else
    color = clamp(color, vec3(0.0), vec3(1.0));
#endif

#ifdef COLOR_GRADE
    // Split toning after tonemap so tints apply to rolled-off values.
    if (tonemap_type != 8) // bypass for S-Log3 capture
        color = applySplitToning(color);
#endif

    color = linear_to_srgb(color);

    // === DISPLAY SPACE =======================================================

#ifdef LEGACY_GAMMA
    color = legacyGamma(color);
#endif

#ifdef COLOR_GRADE
    // 6.5 black/white point → 7 brightness+contrast → 7.5 hi/shadow recovery
    // → 8 saturation → 9 vibrance → 10 hue shift → 11 LUT → 12 curves.
    // All bypassed for S-Log3 capture: the recorder must receive raw log code
    // values, so no display-space grade (especially the LUT) may touch them.
    if (tonemap_type != 8)
    {
        color = applyBlackWhitePoint(color);
        color = applyBrightnessContrast(color);
        color = applyShadowHighlightRecovery(color);
        color = applySaturation(color);
        color = applyVibrance(color);
        color = applyHueShift(color);
        color = applyLUTGrading(color);
        color = applyChannelCurves(color);
    }
#endif

    color = clamp(color, vec3(0.0), vec3(1.0)); // We should always be 0-1 past this point

    return color;
}

// =============================================================================
// Main
// =============================================================================
void main()
{
#ifdef AL_SENSOR_GODRAYS
    vec4 scene = texture(diffuseRect, vary_fragcoord);
    vec3 display_ray = max(texture(diffuseMap, vary_fragcoord).rgb, vec3(0.0));
    if (all(equal(display_ray,vec3(0.0)))) { frag_color = scene; return; }
    vec3 desired = clamp(cameraTransfer(scene.rgb)+display_ray, 0.0, 1.0);
    if (all(greaterThanEqual(desired,vec3(1.0-1e-6))))
    { frag_color = vec4(vec3(65000.0),scene.a); return; }
    if (tonemap_type == 8 && tonemap_slog_gamut != 0)
    {
        // Log codes are camera-primary values. Undo OETF/toe, then the gamut
        // matrix and EV together; independent Rec.709 channel searches are wrong.
        // Retain known scene energy beyond a clipped code: the inverse alone
        // can only recover the curve ceiling. Decode the change around that
        // ceiling instead. In the unclipped domain this inverts log/gamut;
        // the existing downstream sRGB cancellation has finite precision.
        vec3 converted = max(scene.rgb, vec3(0.0)) +
            (decode_slog_family(desired) - decode_slog_family(cameraTransfer(scene.rgb)));
        frag_color = vec4(clamp(converted, vec3(0.0), vec3(65000.0)), scene.a);
        return;
    }
    bool coupled = tonemap_grade.x != 1.0;
#ifdef COLOR_GRADE
    coupled = true; // split tone, saturation, hue, LUT and curves can couple RGB
#endif
    if (coupled || tonemap_type == 0 || tonemap_type == 1 || tonemap_type == 2 || tonemap_type == 6 || tonemap_type == 7)
    {
        // Neutral/ACES/AgX/LPM couple RGB channels. Solve their actual production
        // transfer together; independent channel searches alter ray colour.
        // At a clipped gamut boundary, retain the closest representable light.
        vec3 value = max(scene.rgb,vec3(0.0))+max(srgb_to_linear(display_ray),vec3(1e-6));
        for (int iteration=0; iteration<12; ++iteration)
        {
            vec3 code = cameraTransfer(value);
            vec3 residual = desired-code;
            float error = dot(residual,residual);
            if (error < 1e-10) break;
            vec3 step_size = max(value*0.01,vec3(1e-5));
            mat3 jacobian;
            for (int axis=0; axis<3; ++axis)
            {
                vec3 probe = value; probe[axis] += step_size[axis];
                jacobian[axis] = (cameraTransfer(probe)-code)/step_size[axis];
            }
            mat3 normal = transpose(jacobian)*jacobian;
            float damping = max(normal[0][0]+normal[1][1]+normal[2][2],1e-4)*1e-6;
            vec3 step = inverse(normal+mat3(damping))*transpose(jacobian)*residual;
            step = clamp(step,-max(value,vec3(0.1)),4.0*max(value,vec3(0.1)));
            bool improved = false;
            for (int trial=0; trial<4; ++trial)
            {
                vec3 candidate = clamp(value+step,vec3(0.0),vec3(65000.0));
                vec3 delta = desired-cameraTransfer(candidate);
                if (dot(delta,delta) < error) { value = candidate; improved = true; break; }
                step *= 0.5;
            }
            if (!improved) break;
        }
        frag_color = vec4(value,scene.a);
        return;
    }
    // Reinhard and the S-Log family are separable monotone transfers. Search
    // in log light units so both deep shadows and HDR highlights retain
    // precision. Filmic and GT also use separable channel curves.
    vec3 lower = vec3(-24.0), upper = vec3(16.0);
    for (int i=0; i<24; ++i)
    {
        vec3 mid = (lower+upper)*0.5;
        vec3 code = cameraTransfer(exp2(mid));
        bvec3 below = lessThan(code, desired);
        lower = mix(lower, mid, below);
        upper = mix(mid, upper, below);
    }
    vec3 converted = min(max(exp2((lower+upper)*0.5), scene.rgb), vec3(65000.0));
    frag_color = vec4(mix(scene.rgb, converted, greaterThan(display_ray,vec3(0.0))), scene.a);
    return;
#endif

    // === LINEAR SPACE ========================================================
#ifdef AL_SENSOR_EXPOSURE
    vec4 scene = texture(diffuseRect, vary_fragcoord);
    scene.rgb = applyExposure(scene.rgb);
    // Half-float scratch must never overflow on invalid/extreme scene inputs.
    scene.rgb = mix(clamp(scene.rgb, vec3(-65000.0), vec3(65000.0)), vec3(0.0), isnan(scene.rgb));
    frag_color = scene;
    return;
#endif


#if defined(HAS_POST_EFFECTS) && !defined(AL_SENSOR_CAMERA)
    vec4 diff = applyChromaticAberration(diffuseRect, vary_fragcoord);
#ifdef AL_SENSOR_OPTICS
    diff.rgb += applyExposure(computeLensFlare(diffuseMap, depthMap, vary_fragcoord));
#else
    diff.rgb += computeLensFlare(diffuseRect, depthMap, vary_fragcoord);
#endif
#else
    vec4 diff = texture(diffuseRect, vary_fragcoord);
#endif

#if defined(BLOOM_COMPOSITE) && !defined(AL_SENSOR_CAMERA)
    // Additively composite the HDR bloom pyramid before exposure/tonemap so the
    // glow is rolled through the same exposure curve as the underlying scene.
    {
        vec4 bloom_sample = texture(bloomMap, vary_fragcoord);
#ifdef AL_SENSOR_OPTICS
        bloom_sample.rgb = applyExposure(bloom_sample.rgb);
        bloom_sample.a *= applyExposure(vec3(1.0)).x;
#endif
    #ifdef BLOOM_HALATION
        diff.rgb += bloom_sample.rgb * bloom_strength
                  + bloom_sample.a   * halation_strength * halation_tint;
    #else
        diff.rgb += bloom_sample.rgb * bloom_strength;
    #endif
    }
#endif

#ifdef AL_SENSOR_OPTICS
    frag_color = diff;
    return;
#endif
#if defined(TONEMAP) && !defined(AL_SENSOR_CAMERA)
    diff.rgb = applyExposure(diff.rgb);
#endif

    // Graduated ND + Polarizer are applied earlier, in the on-lens pre-pass
    // (LLPipeline::applyOnLensFilters), so bloom and lens flare — generated
    // between that pass and this one — see the already-filtered scene.

    diff.rgb = cameraTransfer(diff.rgb);

#ifdef DITHER
    // Post chain is 8-bit — dither before the first quantization rather than
    // waiting for the final blit.
    diff.rgb = applyDither(diff.rgb, gl_FragCoord.xy);
#endif

    //debugExposure(diff.rgb);
    frag_color = diff;
}

