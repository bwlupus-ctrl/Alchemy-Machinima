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

// On-lens filters (Graduated ND + Polarizer + [LensLooks] colour grads +
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
// -> Polarizer -> frame filter -> IR.  Every addition early-outs on its own
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
        float sky = smoothstep(0.9999, 1.0, d);
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
void irClassify(vec3 A, float metallic, float roughness, float tag, out float f, out float s)
{
    float maxc = irMax3(A);
    float minc = irMin3(A);
    float sat  = (maxc - minc) / max(maxc, 1.0e-4);
    float hue  = irHue(A);

    float dry  = clamp(on_lens_ir2.z, 0.0, 1.0);
    float sens = clamp(on_lens_ir2.y, 0.0, 1.0);

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

// NIR reflectance / visible luminance gain for one classified pixel.
float irGain(vec3 A, float f, float s, float metallic, float emissive, float lumA)
{
    float rho_other = mix(lumA, A.r, 0.6);
    float rho_leaf  = 0.5  * max(on_lens_ir2.x, 0.0);   // 2^FoliageEV folded in by the host
    float rho_skin  = 0.55 * max(on_lens_ir3.y, 0.0);   // 2^SkinEV
    float rho = mix(mix(rho_other, rho_leaf, f), rho_skin, s);
    rho = mix(rho, lumA, clamp(metallic, 0.0, 1.0));
    float gain = clamp(rho / max(lumA, 0.02), 0.0, 16.0);
    return mix(gain, 1.0, clamp(emissive, 0.0, 1.0));
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
    float sky = smoothstep(0.9999, 1.0, d);

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
    diff.rgb = lensFrameFilter(diff.rgb);                                // [LensLooks]
#ifndef ON_LENS_NO_IR
    diff.rgb = irApply(diff.rgb, vary_fragcoord, exposure_scale, diff.a); // [LensLooks]
#endif
    frag_color = diff;
}
