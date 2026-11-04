/**
 * @file onLensFiltersF.glsl
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

// On-lens filters (Graduated ND + Polarizer + Variable ND + [LensLooks] colour grads +
// Infrared film) as a standalone PRE-PASS.
//
// Runs on the linear HDR scene buffer after exposure is METERED but before it is
// APPLIED, and before bloom and lens-flare generation, so those optics respect
// the filtered scene the way a real on-lens ND/polarizer does. The ND/polarizer/
// Night Mask filter functions live in postEffectUtilsF.glsl (attached via
// mFeatures.hasPostEffects) and are shared with colorCorrectF's declarations;
// only the call site is here.
//
// [LensLooks] (doc/LENS_LOOKS_CONTRACT.md) adds, IN THIS FILE (postEffectUtilsF
// links into 8 colorCorrect variants and must stay lean):
//   * colour grads   : a tint on the Graduated ND wedge (lensGradTint) and a
//                      full-frame 85/80-style warming / cooling multiply
//                      (lensFrameFilter).  Both are luminance-normalised gains
//                      prepared on the CPU, so exposure metering is untouched.
//   * Infrared film  : renderer-aware B&W / Aerochrome emulation (irApply)
//                      driven by the G-buffer (albedo / ORM / normal flags /
//                      avatar tag) with a radiance-only confidence fallback.
//                      Compiled out by the ON_LENS_NO_IR permutation, which the
//                      host falls back to when this program fails to compile,
//                      so a driver failure can never take ND / Pol / Night
//                      Mask down with it.
// Pass order: Night Mask -> [ReShade raw capture, host] -> GradND -> grad tint
// -> Polarizer -> Variable ND -> frame filter -> IR. Every addition early-outs on its own
// uniform (amount / strength 0 == exactly identity, before any fetch).

out vec4 frag_color;

uniform sampler2D diffuseRect;   // linear HDR scene (read; draw target is the scratch)
uniform sampler2D depthMap;      // deferred hardware depth (sky ~ 1.0)
uniform sampler2D exposureMap;   // 1x1 auto-exposure scale
uniform float     exposure;      // manual exposure knob (RenderExposure)

in vec2 vary_fragcoord;

vec3 applyNightMask(vec3 color, vec2 uv, sampler2D depth);
vec3 applyGradND(vec3 color, vec2 uv, sampler2D depth);
vec3 applyPolarizer(vec3 color, vec2 uv, sampler2D depth, float exposure_scale);

const vec3 LENS_LUMA = vec3(0.2126, 0.7152, 0.0722);

// =============================================================================
// [LensLooks] Colour grads
// =============================================================================
// on_lens_grads   xyz linear, luminance-normalised tint multiplier for the ND
//                 wedge (CPU: Kelvin/mired or swatch -> RGB, divided by its
//                 luminance), w amount 0..1 (0 = off, bit-exact)
// on_lens_grads2  xyz full-frame 85/80 gain (same normalisation), w amount 0..1
// The wedge mask mirrors applyGradND (postEffectUtilsF.glsl) statement for
// statement so the tint sits exactly where the density does; the ND uniforms
// are re-declared here (separate compile unit, identical types).
uniform vec4 on_lens_grads;
uniform vec4 on_lens_grads2;
uniform vec4 gradnd_params;   // (density_stops, angle_rad, position, softness)
uniform vec4 gradnd_params2;  // (sky_confine, sun_weight, sun_radius, flip)
uniform vec4 gradnd_sun;      // (sun_uv.x, sun_uv.y, has_sun, aspect)

// Shared with GradND/Polarizer in postEffectUtilsF.glsl.
float lensSkyDepth(float depth);

float lensGradNDMask(vec2 uv, sampler2D depth)
{
    float angle    = gradnd_params.y;
    float position = gradnd_params.z;
    float softness = max(gradnd_params.w, 1e-3);

    float sky_confine = clamp(gradnd_params2.x, 0.0, 1.0);
    float sun_weight  = gradnd_params2.y;
    float sun_radius  = max(gradnd_params2.z, 1e-3);
    float flip        = gradnd_params2.w;

    float aspect = max(gradnd_sun.w, 1e-3);

    vec2 p = uv - vec2(0.5, position);
    p.x *= aspect;
    float axis = -p.x * sin(angle) + p.y * cos(angle);
    float t = clamp(0.5 + (axis / softness) * 0.5, 0.0, 1.0);
    t = smoothstep(0.0, 1.0, t);
    if (flip > 0.5)
        t = 1.0 - t;

    if (sun_weight > 0.0)
    {
        vec2 dv = uv - gradnd_sun.xy;
        dv.x *= aspect;
        float near_sun = clamp(1.0 - length(dv) / sun_radius, 0.0, 1.0);
        near_sun *= gradnd_sun.z;
        t = clamp(t * (1.0 + near_sun * sun_weight), 0.0, 1.0);
    }

    if (sky_confine > 0.0)
    {
        float d = texture(depth, uv).r;
        float sky = lensSkyDepth(d);
        t *= mix(1.0, sky, sky_confine);
    }
    return t;
}

vec3 lensGradTint(vec3 color, vec2 uv, sampler2D depth)
{
    float amount = clamp(on_lens_grads.w, 0.0, 1.0);
    if (amount <= 0.0)
        return color;
    float t = lensGradNDMask(uv, depth);
    return color * mix(vec3(1.0), max(on_lens_grads.rgb, vec3(0.0)), amount * t);
}

vec3 lensFrameFilter(vec3 color)
{
    float amount = clamp(on_lens_grads2.w, 0.0, 1.0);
    if (amount <= 0.0)
        return color;
    return color * mix(vec3(1.0), max(on_lens_grads2.rgb, vec3(0.0)), amount);
}

// =============================================================================
// [LensLooks] Infrared film
// =============================================================================
#ifndef ON_LENS_NO_IR

// Packed uniforms (LLStaticHashedString, uploaded by the lens-looks host):
//   on_lens_ir    x strength 0..1 (0 = off, bit-exact), y mode (0 B&W, 1 Aerochrome),
//                 z exposure gain 2^EV, w red leak 0..1 (B&W)
//   on_lens_ir2   x foliage gain 2^FoliageEV, y sensitivity 0..1, z dry vegetation 0..1,
//                 w sky darken (stops)
//   on_lens_ir3   x water darken (stops), y skin gain 2^SkinEV, z shadow depth 0..1,
//                 w halation 0..1
//   on_lens_ir4   x Aerochrome b (blue bleed), y Aerochrome g (green cut), z Aerochrome
//                 saturation, w debug view (0 off, 1 classes, 2 NIR gain, 3 confidence)
//   on_lens_ir5   x water detection 0/1, y avatar tag usable 0/1, z camera underwater 0/1,
//                 w reserved
//   on_lens_ir_up xyz world +Z in VIEW space (unit), w water height - camera height (m)
// Samplers:
//   onLensAlbedoMap  deferredScreen att0 (GL_SRGB8_ALPHA8: sampled LINEAR, alpha = emissive /
//                    fullbright tag)
//   specularRect     deferredScreen att1 (PBR: ORM in rgb; legacy: spec rgb, glossiness a)
//   normalMap        deferredScreen att2 via deferredUtil getNorm / getNormRaw (flags in .w)
//   depthMap         (above)
// Position reconstruction uses nightMaskGetPosition() against the DEDICATED
// night_mask_inv_proj (postEffectUtilsF.glsl) -- the host uploads it whenever IR
// is active, not only for Night Mask.  Never the managed inv_proj (see the
// Night Mask comment block there).
uniform vec4 on_lens_ir;
uniform vec4 on_lens_ir2;
uniform vec4 on_lens_ir3;
uniform vec4 on_lens_ir4;
uniform vec4 on_lens_ir5;
uniform vec4 on_lens_ir_up;

uniform sampler2D onLensAlbedoMap;
uniform sampler2D specularRect;

vec4 getNorm(vec2 screenpos);
vec4 getNormRaw(vec2 screenpos);
vec4 nightMaskGetPosition(vec2 pos_screen, sampler2D depth);

float irMax3(vec3 c) { return max(c.r, max(c.g, c.b)); }
float irMin3(vec3 c) { return min(c.r, min(c.g, c.b)); }

// hue in degrees 0..360 (0 = red, 120 = green, 240 = blue)
float irHue(vec3 c)
{
    float maxc  = irMax3(c);
    float minc  = irMin3(c);
    float delta = maxc - minc;
    if (delta < 1.0e-5)
        return 0.0;
    float h;
    if (maxc == c.r)
        h = mod((c.g - c.b) / delta, 6.0);
    else if (maxc == c.g)
        h = (c.b - c.r) / delta + 2.0;
    else
        h = (c.r - c.g) / delta + 4.0;
    return h * 60.0;
}

// Chlorophyll score f and skin score s from an albedo-like colour A (linear).
void irClassifyResponse(vec3 A, float metallic, float roughness, float tag, vec2 response, out float f, out float s)
{
    float maxc = irMax3(A);
    float minc = irMin3(A);
    float sat  = (maxc - minc) / max(maxc, 1.0e-4);
    float hue  = irHue(A);

    float dry  = clamp(response.y, 0.0, 1.0);
    float sens = clamp(response.x, 0.0, 1.0);

    // hue window yellow-green .. green; dry vegetation lets olive / straw in
    float hue_lo = mix(55.0, 38.0, dry);
    float hue_w  = smoothstep(hue_lo, hue_lo + 15.0, hue) * (1.0 - smoothstep(150.0, 175.0, hue));

    // saturation threshold (Sensitivity)
    float sat0  = mix(0.35, 0.06, sens);
    float sat_w = smoothstep(sat0, sat0 + 0.2, sat);

    // red edge: leaves reflect clearly more green than red
    float ratio   = A.g / max(A.r, 1.0e-4);
    float edge_lo = mix(1.05, 0.95, dry);
    float edge_hi = mix(1.60, 1.35, dry);
    float edge_w  = smoothstep(edge_lo, edge_hi, ratio);

    // reject near-black and neon green
    float dark_rej = smoothstep(0.015, 0.06, maxc);
    float neon_rej = 1.0 - smoothstep(0.85, 0.97, sat) * smoothstep(0.6, 0.9, maxc);

    f = hue_w * sat_w * edge_w * dark_rej * neon_rej;
    f *= (1.0 - clamp(metallic, 0.0, 1.0)) * smoothstep(0.25, 0.55, roughness);

    // skin: avatar tag x warm hue window x moderate saturation x not dark
    float skin_hue = smoothstep(2.0, 10.0, hue) * (1.0 - smoothstep(35.0, 48.0, hue));
    float skin_sat = smoothstep(0.10, 0.20, sat) * (1.0 - smoothstep(0.65, 0.85, sat));
    float skin_lum = smoothstep(0.08, 0.20, maxc);
    s = clamp(tag, 0.0, 1.0) * skin_hue * skin_sat * skin_lum;
    f *= 1.0 - s;
}

void irClassify(vec3 A, float metallic, float roughness, float tag, out float f, out float s)
{
    irClassifyResponse(A, metallic, roughness, tag, on_lens_ir2.yz, f, s);
}

// NIR reflectance / visible luminance gain for one classified pixel.
float irGainResponse(vec3 A, float f, float s, float metallic, float emissive, float lumA, vec2 response)
{
    float rho_other = mix(lumA, A.r, 0.6);
    float rho_leaf  = 0.5  * max(response.x, 0.0);   // 2^FoliageEV folded in by the host
    float rho_skin  = 0.55 * max(response.y, 0.0);   // 2^SkinEV
    float rho = mix(mix(rho_other, rho_leaf, f), rho_skin, s);
    rho = mix(rho, lumA, clamp(metallic, 0.0, 1.0));
    float gain = clamp(rho / max(lumA, 0.02), 0.0, 16.0);
    return mix(gain, 1.0, clamp(emissive, 0.0, 1.0));
}

float irGain(vec3 A, float f, float s, float metallic, float emissive, float lumA)
{
    return irGainResponse(A, f, s, metallic, emissive, lumA, vec2(on_lens_ir2.x, on_lens_ir3.y));
}

// Water: the Tron geometric test -- a horizontal surface at the water plane
// height (no water in the G-buffer).  Returns 0..1.
float irWater(vec2 uv, vec3 n)
{
    if (on_lens_ir5.x < 0.5 || on_lens_ir5.z > 0.5)
        return 0.0;
    vec3  up    = on_lens_ir_up.xyz;
    vec3  p     = nightMaskGetPosition(uv, depthMap).xyz;
    float z_c   = max(-p.z, 1.0e-3);
    float h_rel = dot(p, up);                       // pixel height above the camera (m)
    float tol   = 0.25 + z_c * 0.002;
    float flat_w = smoothstep(0.85, 0.97, dot(n, up));
    return flat_w * (1.0 - smoothstep(0.5 * tol, tol, abs(h_rel - on_lens_ir_up.w)));
}

vec3 irApply(vec3 L, vec2 uv, float exposure_scale, inout float glow_a)
{
    float strength = clamp(on_lens_ir.x, 0.0, 1.0);
    if (strength <= 0.0)
        return L; // off: untouched, before any fetch

    float d   = texture(depthMap, uv).r;
    float sky = lensSkyDepth(d);

    vec4  alb      = texture(onLensAlbedoMap, uv);  // linear (sRGB8 target)
    vec3  A        = max(alb.rgb, vec3(0.0));
    float emissive = clamp(alb.a, 0.0, 1.0);
    vec4  orm      = texture(specularRect, uv);
    vec4  nraw     = getNormRaw(uv);
    vec3  n        = getNorm(uv).xyz;
    bool  is_pbr   = GET_GBUFFER_FLAG(nraw.w, GBUFFER_FLAG_HAS_PBR);
    float metallic  = is_pbr ? orm.b : 0.0;
    float roughness = is_pbr ? orm.g : 1.0 - orm.a;

    float tag = 0.0;
#ifdef GBUFFER_AVATAR_TAG
    tag = GBUFFER_AVATAR_TAG_OF(nraw.w) ? 1.0 : 0.0;
#endif
    tag *= clamp(on_lens_ir5.y, 0.0, 1.0);

    float lumL = max(dot(L, LENS_LUMA), 0.0);
    float lumA = dot(A, LENS_LUMA);

    // --- G-buffer classification ------------------------------------------
    float f, s;
    irClassify(A, metallic, roughness, tag, f, s);
    float gainG = irGain(A, f, s, metallic, emissive, lumA);

    // --- confidence: irradiance chroma spread (forward alpha / particles
    //     over an unrelated G-buffer surface make L/A inconsistent) -----------
    vec3  I      = L / max(A, vec3(0.02));
    float lumI   = max(dot(I, LENS_LUMA), 1.0e-4);
    float spread = (irMax3(I) - irMin3(I)) / lumI;
    float conf   = 1.0 - smoothstep(0.6, 2.5, spread);
    conf *= smoothstep(0.005, 0.03, lumA);           // black albedo: nothing to trust

    // --- radiance-only fallback: classify the normalised radiance -----------
    vec3  Ln = L / max(irMax3(L), 1.0e-4);
    float ff, sf;
    irClassify(Ln, 0.0, 1.0, tag, ff, sf);
    float gainF = irGain(Ln, ff, sf, 0.0, emissive, dot(Ln, LENS_LUMA));

    float gain = mix(gainF, gainG, conf);
    f = mix(ff, f, conf);
    s = mix(sf, s, conf);

    // --- lighting-correct NIR; blue (sky-lit) shadow irradiance is dark in IR
    float blueI = clamp((I.b - 0.5 * (I.r + I.g)) / lumI, 0.0, 1.0);
    float k_irr = 1.0 - clamp(on_lens_ir3.z, 0.0, 1.0) * blueI * conf;
    float NIR   = lumL * gain * k_irr;

    // --- sky: darken by blueness, clouds (white) stay white ------------------
    float blueL   = clamp((L.b - 0.5 * (L.r + L.g)) / max(lumL, 1.0e-4), 0.0, 1.0);
    float NIR_sky = lumL * exp2(-max(on_lens_ir2.w, 0.0) * blueL);
    NIR = mix(NIR, NIR_sky, sky);

    // --- water: darken, keep glints -----------------------------------------
    float water = 0.0;
    if (sky < 0.5)
    {
        water = irWater(uv, n);
        float glint = smoothstep(2.0, 4.0, lumL * exposure_scale);
        water *= 1.0 - glint;
        NIR *= exp2(-max(on_lens_ir3.x, 0.0) * water);
    }

    // --- film response -------------------------------------------------------
    float ev_gain = max(on_lens_ir.z, 0.0);
    vec3  outc;
    if (on_lens_ir.y < 0.5)
    {
        float Y = mix(NIR, L.r, clamp(on_lens_ir.w, 0.0, 1.0)) * ev_gain;
        outc = vec3(Y);
    }
    else
    {
        float b = max(on_lens_ir4.x, 0.0);
        float g = clamp(on_lens_ir4.y, 0.0, 1.0);
        float R = NIR + b * L.b;
        float G = L.r + b * L.b;
        float B = L.g * (1.0 - g) + b * L.b;
        outc = vec3(R, G, B);
        float lum = dot(outc, LENS_LUMA);
        outc = max(mix(vec3(lum), outc, max(on_lens_ir4.z, 0.0)), vec3(0.0));
        outc *= ev_gain;
    }

    // --- halation: feed this frame's bloom through the alpha glow tag -------
    float hal = clamp(on_lens_ir3.w, 0.0, 1.0) * strength;
    if (hal > 0.0)
    {
        float hot = smoothstep(1.0, 4.0, dot(outc, LENS_LUMA) * exposure_scale);
        glow_a = clamp(glow_a + hal * hot * max(f, 0.35), 0.0, 1.0);
    }

    // --- debug views ---------------------------------------------------------
    int debug = int(on_lens_ir4.w + 0.5);
    if (debug == 1)
        return vec3(f, s, max(sky, water));
    if (debug == 2)
        return vec3(gain * 0.25);
    if (debug == 3)
        return vec3(conf);

    return mix(L, outc, strength);
}
#endif // ON_LENS_NO_IR

// =============================================================================
// Variable ND: artifacts at compensated exposure, never an ND exposure multiply.
// =============================================================================
// params: enabled, dial stops, optical grade (0 Pro / 1 Budget / 2 Cheap; worse upward), ring radians.
// Named presets resolve these controls on the host: Clean Pro, Budget, Pushed Too Far.
// lens: tan(vertical FOV / 2), full-frame aspect, radial-weighted mean leakage,
//       material IR blend (zero selects the material-free shader on the host).
// frame: tile-to-full-frame UV scale.xy, offset.zw (identity for ordinary views).
uniform vec4 vnd_params;
uniform vec4 vnd_lens;
uniform vec4 vnd_frame;
uniform vec4 vnd_draw_size; // actual scratch viewport.xy, full sensor extent.zw in render texels
uniform vec4 vnd_ir; // fixed IR defaults: foliage gain, sensitivity, dry, skin gain
uniform vec4 vnd_sun; // shared sun projection promoted to full-frame UV/visibility

vec3 vndKeepLuma(vec3 original, vec3 shifted)
{
    float target = dot(original, LENS_LUMA);
    float actual = dot(shifted, LENS_LUMA);
    return target > 0.0 && actual > 1e-10 ? shifted * (target / actual) : original;
}

vec3 vndLimitRadiance(vec3 original, vec3 filtered)
{
    // The existing target is RGBA16F. A luminance ceiling alone cannot stop
    // a saturated channel overflowing half-float after a warm/purple cast.
    // Ease back toward the original colour as channel headroom runs out.
    float input_peak = max(original.r, max(original.g, original.b));
    if (!(input_peak < 65504.0)) return original;
    float output_peak = max(filtered.r, max(filtered.g, filtered.b));
    float headroom = clamp((65504.0 - input_peak) / max(output_peak - input_peak, 1e-6), 0.0, 1.0);
    return mix(original, filtered, headroom);
}

vec3 vndPollution(vec3 color, vec2 uv, float sky)
{
#ifndef ON_LENS_NO_IR
    if (vnd_lens.w <= 0.0) return color;
    float foreground = 1.0 - sky;
    if (foreground > 0.0)
    {
        vec4 alb = texture(onLensAlbedoMap, uv);
        vec3 A = max(alb.rgb, vec3(0.0));
        vec4 orm = texture(specularRect, uv);
        vec4 nraw = getNormRaw(uv);
        bool pbr = GET_GBUFFER_FLAG(nraw.w, GBUFFER_FLAG_HAS_PBR);
        float metallic = pbr ? orm.b : 0.0;
        float roughness = pbr ? orm.g : 1.0 - orm.a;
        float f, s;
        // Conservative warm-material protection without requiring an avatar tag:
        // wood/brick/sand in the skin hue window are protected too. Fixed VND
        // response stays independent of IR sliders, including when IR is enabled.
        irClassifyResponse(A, metallic, roughness, 1.0, vnd_ir.yz, f, s);
        float lumA = dot(A, LENS_LUMA);
        float gain = irGainResponse(A, f, s, metallic, alb.a, lumA, vnd_ir.xw);
        vec3 I = color / max(A, vec3(0.02));
        float spread = (irMax3(I) - irMin3(I)) / max(dot(I, LENS_LUMA), 1e-4);
        float confidence = (1.0 - smoothstep(0.6, 2.5, spread));
        float fabric = (1.0 - smoothstep(0.025, 0.12, lumA)) *
            smoothstep(0.3, 0.7, roughness) * (1.0 - metallic) * (1.0 - f);
        float amount = foreground * vnd_lens.w * confidence * (1.0 - s) *
            (1.0 - clamp(alb.a, 0.0, 1.0));
        vec3 stain = mix(vec3(1.65, 1.0, 0.62), vec3(1.9, 0.62, 1.4), f);
        amount *= max(f * clamp(gain * 0.25, 0.0, 1.0), fabric);
        return vndKeepLuma(color, color * mix(vec3(1.0), stain, amount));
    }
#endif
    return color;
}

// Each fixed-position band integrates to zero over the full sensor.
float vndSkyBand(float y, float center)
{
    float t = (y - center) / 0.045;
    float lo = center / 0.045;
    float hi = (1.0 - center) / 0.045;
    float mean = center * exp(-lo * lo) + (1.0 - center) * exp(-hi * hi);
    return (1.0 - 2.0 * t * t) * exp(-t * t) - mean;
}

vec3 applyVariableND(vec3 color, vec2 uv, float exposure_scale)
{
    if (vnd_params.x < 0.5 || !(vnd_lens.z > 0.0 && vnd_lens.z < 1.0)) return color;

    float stops = clamp(vnd_params.y, 1.0, 8.0);
    float quality = clamp(vnd_params.z, 0.0, 2.0);
    // This pass point-samples scene texels. Derive the same full-frame texel
    // centre for a tiled snapshot and a single draw; interpolated UV rounding
    // otherwise becomes visible along the sharper arms.
    vec2 tile_size = max(vnd_draw_size.xy, vec2(1.0));
    vec4 frame = vnd_frame.x <= 0.0 ? vec4(1.0, 1.0, 0.0, 0.0) : vnd_frame;
    vec2 frame_size = vnd_frame.x <= 0.0 ? tile_size : max(vnd_draw_size.zw, vec2(1.0));
    vec2 frame_uv = (floor(uv * tile_size) + 0.5 + frame.zw * frame_size) / frame_size;
    vec2 ray = (2.0 * frame_uv - 1.0) * vec2(vnd_lens.y, 1.0) * vnd_lens.x;
    float radius = length(ray);
    float theta = atan(radius); // REAL lens ray angle, not radius in screen UV
    float phi = radius > 1e-8 ? atan(ray.y, ray.x) : 0.0;
    float corner = atan(length(vec2(vnd_lens.y, 1.0) * vnd_lens.x));
    // True square-pixel azimuth, with the two zero-ring bands aimed at the
    // rectangular sensor's corners. Rotate both physical bands with the ring.
    float axis = atan(1.0 / max(vnd_lens.y, 1e-4));
    float folded = atan(abs(sin(phi - vnd_params.w)), abs(cos(phi - vnd_params.w)));
    float sector = folded < axis ? axis : 1.57079632679 - axis;
    float leak = pow(abs(sin((folded - axis) * 1.57079632679 / max(sector, 1e-4))), 0.85);
    // Rise early enough for bands to enter the centre, rather than making a
    // corner vignette. FOV strength is separate so telephoto stays clean.
    float radial = pow(clamp(theta / max(corner, 1e-4), 0.0, 1.0), 0.25);
    float wide_strength = smoothstep(0.2, 0.7, vnd_lens.x);
    float dial = (stops - 1.0) / 7.0;
    // Keep both low-grade lenses nearly invisible through four stops; preserve
    // Budget at 5/8 and Cheap at 8, while Cheap never has less X than Budget.
    float budget = 0.82 * dial * dial * smoothstep(3.0, 5.0, stops);
    float relative_leak = quality < 0.5 ? 0.002 * (exp2(stops) - 1.0) / 255.0 :
        (quality < 1.5 ? budget : max(budget, 1.3 * max(exp2(stops) - 16.0, 0.0) / 240.0));
    float bias = clamp(vnd_lens.z, 0.0, 1.0);
    relative_leak = min(relative_leak, min(0.75 / max(bias, 1e-4), 0.5 / max(1.0 - bias, 1e-4)));
    // A portrait/ultrawide crop lets one wedge dominate the sensor. Reduce
    // redistribution there; this tends toward identity, never attenuation.
    float sensor_balance = min(1.0, min(vnd_lens.y, 1.77777777778 / max(vnd_lens.y, 1e-4)));
    relative_leak *= wide_strength * sensor_balance;
    // The host integrates this SAME radial/angular profile over the full
    // sensor. Mean preservation is approximate: table interpolation, gain bounds,
    // partial sky masks and the highlight ceiling can shift scene mean luma.
    float gain = clamp(1.0 + relative_leak * radial * (leak - bias), 0.25, 1.5);

    // A narrow sun-phased sky band with bright shoulders. This Gaussian's
    // derivative has an analytic zero-mean correction over the full sensor.
    // Local darkening is visible without a broad sky-exposure reduction.
    float sky = lensSkyDepth(texture(depthMap, uv).r);
    float wide = clamp(vnd_lens.x * vnd_lens.x, 0.0, 1.0);
    // Fade amplitudes, never move a band back across the frame as visibility falls.
    float band = mix(vndSkyBand(frame_uv.y, 0.5),
        vndSkyBand(frame_uv.y, clamp(vnd_sun.y, -0.2, 1.2)), vnd_sun.z);
    gain = clamp(gain - sky * band * wide * 0.16 * (quality * 0.5) * dial * dial, 0.25, 1.5);

    // Blue/purple leakage, optical-grade cast, and NIR contamination each
    // preserve the incoming pixel's Rec.709 luminance, not just grey balance.
    float tint = relative_leak * radial * leak * (0.08 + 0.10 * quality);
    vec3 tinted = vndKeepLuma(color, color * mix(vec3(1.0), vec3(1.06, 0.94, 1.25), tint));
    vec3 cast = quality < 0.5 ? vec3(1.008, 1.0, 0.994) :
        (quality < 1.5 ? vec3(0.90, 1.0, 1.17) : vec3(1.24, 1.0, 0.76));
    tinted = vndKeepLuma(tinted, tinted * mix(vec3(1.0), cast, (stops - 1.0) / 7.0));
    tinted = vndPollution(tinted, uv, sky);

    // Do not add energy to already-hot HDR highlights. Never clip existing
    // radiance: the ceiling only limits VND's relative brightening. Dark arms
    // still darken highlights, so highlight-dominated shots may lose mean luma.
    float lum = max(dot(color, LENS_LUMA), 0.0);
    float ceiling = max(lum, 16.0 / max(exposure_scale, 1e-4));
    gain = min(gain, ceiling / max(lum, 1e-8));
    return vndLimitRadiance(color, tinted * gain);
}

void main()
{
    // Preserve the full texel — alpha carries the prim-glow tag that the bloom
    // extract reads downstream, so only the RGB is filtered (IR halation may
    // RAISE the tag, never lower it).
    vec4 diff = texture(diffuseRect, vary_fragcoord);

    // Effective exposure the tonemap will apply later (manual * auto). The
    // polarizer's glare gate uses this so its threshold matches exposed luma
    // even though this pass runs before exposure is applied.
    float exposure_scale = texture(exposureMap, vec2(0.5, 0.5)).r * exposure;

    // Night Mask runs FIRST: it is a world-lighting fake (not a camera-lens
    // artifact like the ND/polarizer below it), and LLPipeline::applyOnLensFilters
    // snapshots the ReShade raw-scene capture right after this draw, so the
    // mask must already be baked into `diff` by the time that capture happens.
    diff.rgb = applyNightMask(diff.rgb, vary_fragcoord, depthMap);
    diff.rgb = applyGradND(diff.rgb, vary_fragcoord, depthMap);
    diff.rgb = lensGradTint(diff.rgb, vary_fragcoord, depthMap);        // [LensLooks]
    diff.rgb = applyPolarizer(diff.rgb, vary_fragcoord, depthMap, exposure_scale);
    diff.rgb = applyVariableND(diff.rgb, vary_fragcoord, exposure_scale);
    diff.rgb = lensFrameFilter(diff.rgb);                                // [LensLooks]
#ifndef ON_LENS_NO_IR
    diff.rgb = irApply(diff.rgb, vary_fragcoord, exposure_scale, diff.a); // [LensLooks]
#endif
    frag_color = diff;
}
