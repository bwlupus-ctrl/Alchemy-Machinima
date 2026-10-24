/**
 * @file class3\deferred\projectorVolumetricF.glsl
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
 *
 * [BDMerge G3.3] Per-projector volumetric light cones (visible spotlight
 * shafts). NET-NEW (not a Black Dragon port): G3.2's godray raymarch is
 * sun-only (parallel rays, whole-view-ray march of the sun cascades). This is
 * a LOCAL-light march: one additive fullscreen pass per shadow-casting
 * projector slot, marching ONLY the segment of the view ray that lies inside
 * that projector's sphere of influence, shaped by its frustum + cookie and
 * self-shadowed by its own shadow map. Reuses the exact spotLightF /
 * deferredUtil projector math (proj_mat/proj_n/size/color/falloff/cookie) and
 * the NSpot per-slot shadow dispatch (sampleSpotShadow, shadowMap4-13).
 */

#extension GL_ARB_texture_rectangle : enable
#extension GL_ARB_shader_texture_lod : enable

/*[EXTRA_CODE_HERE]*/

layout(location = 0) out vec4 frag_color;
// Half-res MRT only: replace (never add) the camera's chosen termination.
// Direct/rim draws enable attachment 0 only, protecting scene sidecars.
layout(location = 1) out float projvol_termination;

in vec2 vary_fragcoord;

// Local-light params (uploaded per slot by setupSpotLightVolumetric + the
// per-cone color/size/falloff/shadow-index uploads in renderProjectorVolumetric).
// Projector uniforms consumed only inside deferredUtil.glsl are declared there.
// Uniforms read directly in this shader object must also be declared here; the
// GLSL linker merges matching declarations across fragment-stage objects.
uniform vec3  center;           // LIGHT_CENTER, view space
uniform float size;             // LIGHT_SIZE (sphere-of-influence radius); also
                                // consumed inside deferredUtil - declared here
                                // too because we read it directly in main().
uniform float falloff;          // LIGHT_FALLOFF
uniform int   proj_shadow_idx;  // this projector's shadow slot (0..N-1)

// [BDMerge G3.3 fix] The shaft's chroma MUST follow the projector's Light Color.
// We redeclare the projector uniforms we read directly here (same pattern as
// `size` above - a uniform of identical type may be declared in more than one
// compilation unit of a stage; the linker merges them). `color` is the light's
// linear diffuse color (LLShaderMgr::DIFFUSE_COLOR, uploaded per cone in
// renderProjectorVolumetric, already lerped toward the art-direction tint).
// Previously the shaft was tinted ONLY via getProjectedLightDiffuseColor(), which
// bakes `color` INTO the gobo sample - entangling the light color with the cookie
// so it read as "the shaft never follows the light color". We now sample the gobo
// as TEXTURE-ONLY and multiply the light `color` in explicitly at the end.
uniform vec3  color;            // DIFFUSE_COLOR - light's linear diffuse color (+tint)
uniform float proj_focus;       // deferredUtil cookie LOD params (read directly here)
uniform float proj_lod;
uniform float proj_range;
uniform vec3  proj_n;           // projector axis used by the shaft-length clip
uniform float projvol_max_distance; // metres along projector axis; 0 = full range
uniform int alr_sil_active;
uniform sampler2D silCoverMap;

// Shared godray controls (this program uploads its OWN values into these).
uniform int   godray_res;         // raymarch sample count (bounded local march;
                                  // Phase 1 item 5: adaptively scaled per cone in C++)
#if defined(PROJVOL_BUDGET_ENABLE) && PROJVOL_BUDGET_ENABLE
uniform float projvol_budget_steps; // 0 = rim-only; >0 = continuous cap
#else
const float projvol_budget_steps = 0.0; // Full compiles out the entire budget kernel
#endif
#if defined(PROJVOL_HARD_BEAM) && PROJVOL_HARD_BEAM
const int projvol_hard_beam_shadows = 1;
#else
const int projvol_hard_beam_shadows = 0;
#endif
uniform float godray_multiplier;  // shaft brightness multiplier
uniform float projvol_g;          // Henyey-Greenstein anisotropy (forward scatter)

// [Phase 1] per-lever sub-controls (uploaded by renderProjectorVolumetric).
uniform float projvol_feather;    // item 6: angular cone-edge softness (0 = hard)
uniform int   projvol_shadow_samples; // item 7: occlusion sub-taps per march step
uniform int   projvol_dither;     // item 2: 0=off(centre) 1=static 2=animated;
                                  // static is never promoted, including with history
uniform float projvol_frame;      // presentation-time phase (60 Hz, wrapped)
uniform float projvol_max;        // item 1: HDR headroom clamp (large in linear HDR)
uniform vec2  projvol_half_res;    // zero = full res; otherwise actual march dimensions
// Keep the recording/default program free of the experimental kernels' register
// pressure. renderProjectorVolumetric rebuilds only this program when entering
// or leaving the A/B group; the three individual levers remain runtime uniforms.
#if defined(PROJVOL_INTEGRATION_AB) && PROJVOL_INTEGRATION_AB
uniform int   projvol_equiangular;
uniform int   projvol_airborne_tap;
uniform int   projvol_cookie_footprint;
#else
const int projvol_equiangular = 0;
const int projvol_airborne_tap = 0;
const int projvol_cookie_footprint = 0;
#endif

uniform mat4  proj_mat;
uniform sampler2D depthMap;
uniform sampler2D projectionMap;
uniform mat4 shadow_matrix[14];
uniform sampler2DShadow shadowMap4;
uniform sampler2DShadow shadowMap5;
uniform sampler2DShadow shadowMap6;
uniform sampler2DShadow shadowMap7;
uniform sampler2DShadow shadowMap8;
uniform sampler2DShadow shadowMap9;
uniform sampler2DShadow shadowMap10;
uniform sampler2DShadow shadowMap11;
uniform sampler2DShadow shadowMap12;
uniform sampler2DShadow shadowMap13;

// [BDMerge G3.3 Batch A] look-neutral performance gates.
uniform int   projvol_frustum_clip;       // E1: 0 = sphere bounds; !=0 = frustum-clipped [t0,t1] (default)
uniform vec4  projvol_frustum_planes[6];  // E1: view-space frustum planes, inside>=0: 0=L 1=R 2=B 3=T 4=near 5=far
uniform int   projvol_shadow_jitter_tap;  // E2: 0 = sub-tap loop (default); !=0 = one IGN-jittered shadow tap

// [BDMerge G3.3 ConservativeShadow] Volume-march shadow sampler gate. The AIRBORNE
// march samples historically reused the SURFACE spot-shadow helper
// (sampleSpotShadow), whose receiver bias, wide lit+shadowed PCF averaging,
// soft-shadow fill floor, sun-cascade camera fade and beyond-shadow_clip.w
// "fully lit" rule are all built for lit receiver SURFACES - for a sample
// floating in mid-air inside an occluder's shadow cone each of those is a leak,
// and together they can paint a bright filament straight through the middle of
// an opaque occluder when viewer, occluder and light line up. !=0 (OPT-IN
// hero-shaft experiment - the root cause is unconfirmed, and the conservative
// kernel is deliberately harder-edged than the legacy wide penumbra) switches
// the march to the conservative volume sampler in shadowUtil.glsl and enables
// the on-axis numerical guards below; 0 (the default) keeps the legacy surface
// sampler byte-identically, so normal soft-shadow shafts are unchanged.
// [Round-2 review fix] The gate, the on-axis guards, the conservative shadow
// dispatch and the guarded normalize are all compiled in/out behind the
// PROJVOL_CONSERVATIVE_SHADOW permutation (llviewershadermgr.cpp keys it off
// BDMergeProjectorVolumetricsConservativeShadow at shader-build time; toggling
// the setting rebuilds shaders - same pattern as PROJVOL_DUST_ENABLE below).
// With the lever OFF this program carries NO extra uniform and NO runtime
// branches: the default march is instruction-identical to the legacy path, not
// merely equivalent. The runtime uniform is kept INSIDE the permutation as a
// belt-and-suspenders default-off (an unset uniform reads 0 = legacy behavior
// across the toggle/rebuild window).
#ifdef PROJVOL_CONSERVATIVE_SHADOW
uniform int   projvol_conservative_shadow;
#endif

// [BDMerge G3.3 Dust] Baked 64^3 seamlessly-tileable dust volume (linear DATA,
// seed 0xD057C1A5: R = macro haze, G = sparse motes, B = fine turbulence,
// A = mote phase). Adds recognizable particulate breakup to the shaft density on
// top of (or instead of) the procedural fbm noise medium. All levers default OFF
// (projvol_dust 0) so the shipped look is byte-identical until enabled.
// [Review fix] The sampler + ALL dust code are compiled in/out behind the
// PROJVOL_DUST_ENABLE permutation (llviewershadermgr.cpp keys it off
// BDMergeProjectorVolumetricsDust at shader-build time; toggling the setting
// rebuilds shaders). A declared sampler is assigned a fragment texture unit at
// shader-map time whether or not the runtime gate is up - in this already
// sampler-heavy program that idle slot could exceed a mid-range GPU's unit
// limit, so with Dust off the sampler must not exist at all. The runtime gate
// (projvol_dust) is kept INSIDE the permutation for the asset-missing /
// bind-failed / intensity-0 cases.
#ifdef PROJVOL_DUST_ENABLE
uniform int       projvol_dust;           // runtime gate: 0 = off (asset missing/failed)
uniform float     projvol_dust_intensity; // master particulate amount (~0..2, CPU-clamped finite)
uniform float     projvol_dust_scale;     // world scale (cycles/metre, CPU-clamped finite)
uniform vec3      projvol_dust_wind;      // drift m/s (unit dust-wind dir * DustDrift, CPU-folded/clamped)
uniform sampler3D projvol_dust_map;       // the 64^3 RGBA8 volume (GL_REPEAT/LINEAR)
#endif

// [BDMerge G3.3 Batch B - R1] Beam-depth reprojection gate. 0 = alpha stays 0.
// !=0 writes a luminance-premultiplied first distance moment into alpha so additive
// overlap across cones remains correct. C++ only uploads 1 on the temporal half-res
// path; direct scene composite / non-temporal paths keep alpha masked and unchanged.
uniform int   projvol_temporal_beam_depth;

// [BDMerge G3.3 Batch 1 B] Gobo-colored occluder shadows. 0 = classic hard black
// occluder shadow (the shipped look). >0 lets occluded march samples still carry a
// dimmed, gobo-shaped colored contribution so occluders tint/dim the beam like
// stained glass (colored god-ray banding shaped by the projector's cookie) rather
// than punching pure-black holes.
uniform float projvol_shadow_tint;

// [Phase 3] atmosphere levers. All default to a no-op (density 1, strengths 0) so
// the shipped look is a flat, uniform cone until a lever is dialed up.
uniform float projvol_density;           // item 3: global haziness master (1 = no-op)
uniform float projvol_noise_strength;    // item 1: animated noise amount (0 = off)
uniform float projvol_noise_scale;       // item 1: noise spatial scale (cycles/m)
uniform float projvol_noise_speed;       // item 1: noise scroll speed (m/s) - kept
                                         // uploaded for compatibility; the scroll now
                                         // uses projvol_wind (speed folded in CPU-side)
// [F5 follow-up] Shared dust-wind direction (region axes, Z up) already scaled by
// the per-cone NoiseSpeed on the CPU. Steers the drifting beam dust; negative
// NoiseSpeed (or negative wind components) reverses. Same wind the froxel air uses.
uniform vec3  projvol_wind;
uniform float projvol_time;              // item 1: continuous seconds (noise scroll)
uniform float projvol_fog_strength;      // item 2: height-fog blend (0 = off)
uniform float projvol_fog_ground_density;// item 2: density at/below the ground ref
uniform float projvol_fog_falloff;       // item 2: e-fold altitude falloff (metres)
uniform float projvol_fog_base;          // item 2: ground reference altitude (region Z)
uniform mat4  projvol_inv_modelview;     // items 1/2: view -> agent(world, Z-up)

// [BDMerge G3.3 Rim] Surface-coupled rim / wrap glow. Physical analog of the
// ReShade "Auto Rim" concept, but driven by the projector's REAL incident light
// (cookie x distance attenuation x this projector's own shadow map) and the REAL
// G-buffer normal instead of a screen-space guess. At the opaque surface that
// caps the march we deposit E = cookie*atten*shadow (the light actually landing
// there), concentrate it toward grazing silhouettes with a Fresnel term built
// from the real normal, and add it to the same additive HDR shaft - so it flows
// straight into the existing bloom-feed and reads as a soft halo hugging the lit
// edge. Strength 0 (default) skips the whole block: the shipped look is unchanged.
uniform float projvol_rim_strength;      // master brightness (0 = off)
uniform float projvol_rim_power;         // Fresnel exponent: higher = tighter to the silhouette
uniform float projvol_rim_threshold;     // ignore incident light dimmer than this (soft gate)
uniform float projvol_rim_wrap;          // directional wrap: 0 = crisp back-only rim, 1 = broad wrap onto the body
uniform float projvol_rim_softness;      // [F4] skin softness: 0 = hard outline, 1 = smoothstep-softened glow

const float M_PI = 3.14159265;

// [Phase 3 item 1] Smooth low-frequency 3D value-noise fbm. TASTEFUL by design:
// value noise with quintic (smootherstep) interpolation gives soft, cloud-like
// lobes - not the blocky/grainy hash look the ruling forbids. A few octaves of
// world-anchored fbm scrolled slowly read as gently drifting dust in the beam.
float projvolHash(vec3 p)
{
    p = fract(p * 0.3183099 + vec3(0.1, 0.2, 0.3));
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float projvolValueNoise(vec3 x)
{
    vec3 p = floor(x);
    vec3 f = fract(x);
    // quintic fade -> C2 continuity, no lattice creases
    f = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);

    float n000 = projvolHash(p + vec3(0.0, 0.0, 0.0));
    float n100 = projvolHash(p + vec3(1.0, 0.0, 0.0));
    float n010 = projvolHash(p + vec3(0.0, 1.0, 0.0));
    float n110 = projvolHash(p + vec3(1.0, 1.0, 0.0));
    float n001 = projvolHash(p + vec3(0.0, 0.0, 1.0));
    float n101 = projvolHash(p + vec3(1.0, 0.0, 1.0));
    float n011 = projvolHash(p + vec3(0.0, 1.0, 1.0));
    float n111 = projvolHash(p + vec3(1.0, 1.0, 1.0));

    float nx00 = mix(n000, n100, f.x);
    float nx10 = mix(n010, n110, f.x);
    float nx01 = mix(n001, n101, f.x);
    float nx11 = mix(n011, n111, f.x);
    float nxy0 = mix(nx00, nx10, f.y);
    float nxy1 = mix(nx01, nx11, f.y);
    return mix(nxy0, nxy1, f.z); // [0,1]
}

float projvolFbm(vec3 p)
{
    // 3 octaves - enough for organic drift, cheap enough for the inner march loop.
    float sum = 0.0;
    float amp = 0.5;
    for (int o = 0; o < 3; ++o)
    {
        sum += amp * projvolValueNoise(p);
        p   *= 2.02;
        amp *= 0.5;
    }
    return sum; // ~[0,1], mean ~0.5
}

// Internal scattering coefficient. Folds the per-metre scattering strength into
// a single constant so godray_multiplier reads ~1.0 in typical set-light scale;
// the physical step length (below) still makes longer paths through the cone
// scatter more. Tuned conservatively - in-world brightness tuning is owed.
const float PROJVOL_SCATTER = 0.35;

// [BDMerge G3.3 Rim] Internal rim brightness scale. In-world tuning found the good
// look sat around RimStrength ~0.15, cramming every usable value into the bottom of
// the slider. This factor spreads the useful range across the slider so the same look
// lands near RimStrength ~0.75 (0.15 / 0.2), giving finer control. Purely a slider-
// ergonomics constant - fold any future global rim-brightness retune in here.
const float PROJVOL_RIM_SCALE = 0.2;

// [Phase 1 item 2] Interleaved gradient noise (Jimenez): a cheap blue-noise-like
// dither that, unlike a plain hash, is STATIC per screen-pixel - so the march
// start offset does not crawl or shimmer while the camera moves (mandatory for
// recording). An optional per-frame rotation (projvol_dither==2) averages the
// residual banding across frames for still shots; it is a subtle opt-in, off by
// default so motion stays shimmer-free by construction.
float interleavedGradientNoise(vec2 p)
{
    return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));
}

// deferredUtil.glsl
vec4 getPosition(vec2 pos_screen);
vec4 getPositionWithDepth(vec2 pos_screen, float depth);
bool clipProjectedLightVars(vec3 center, vec3 pos, out float dist, out float l_dist, out vec3 lv, out vec4 proj_tc);
vec3 getProjectedLightDiffuseColor(float light_distance, vec2 projected_uv);
vec4 getTexture2DLodDiffuse(vec2 tc, float lod); // gobo sample WITHOUT the light color
vec4 sampleGobo(vec2 uv, float lod);
vec2 animatedGoboUV(vec2 uv);
vec3 srgb_to_linear(vec3 col);
float calcLegacyDistanceAttenuation(float distance, float falloff);
vec4 getNorm(vec2 screenpos); // [Rim] real view-space G-buffer normal (deferredUtil)

// [BDMerge G3.3 fix] Texture/gobo footprint ONLY (no light color) - mirrors
// getProjectedLightDiffuseColor()'s LOD math but drops its `color.rgb *` multiply
// so we can apply the true light color once, explicitly, to the whole shaft.
vec3 projGoboTexture(float light_distance, vec2 projected_uv)
{
    float diff  = clamp((light_distance - proj_focus) / proj_range, 0.0, 1.0);
    float lod   = diff * proj_lod;
    vec4  plcol = getTexture2DLodDiffuse(projected_uv.xy, lod);
    return plcol.rgb * plcol.a;
}
// shadowUtil.glsl
float sampleSpotShadow(vec3 pos, vec3 norm, int index, vec2 pos_screen);

// Airborne A/B: one hardware bilinear depth comparison, no surface receiver
// bias, penumbra fill or camera-cascade fade. Surface rims keep sampleSpotShadow.
float projvolAirborneShadow(vec4 clip)
{
    if (clip.w <= 1e-5) return 0.0;
    vec3 q = clip.xyz / clip.w;
    if (any(lessThan(q, vec3(0.0))) || any(greaterThan(q.xy, vec2(1.0)))) return 0.0;
    q.z = min(q.z, 1.0);
    if (proj_shadow_idx == 0) return texture(shadowMap4, q);
    if (proj_shadow_idx == 1) return texture(shadowMap5, q);
    if (proj_shadow_idx == 2) return texture(shadowMap6, q);
    if (proj_shadow_idx == 3) return texture(shadowMap7, q);
    if (proj_shadow_idx == 4) return texture(shadowMap8, q);
    if (proj_shadow_idx == 5) return texture(shadowMap9, q);
    if (proj_shadow_idx == 6) return texture(shadowMap10, q);
    if (proj_shadow_idx == 7) return texture(shadowMap11, q);
    if (proj_shadow_idx == 8) return texture(shadowMap12, q);
    return texture(shadowMap13, q);
}

// Two-point Gauss-Legendre integrates only the smooth, cheap terms. Shadow,
// cookie and heterogeneous medium are sampled once per segment.
float projvolAnalytic(vec3 d, float t, vec4 projected)
{
    if (projected.z < 0.0 || projected.w <= 1e-5) return 0.0;
    vec2 uv = projected.xy / projected.w;
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) return 0.0;
    vec3 delta = d * t - center;
    float r = max(length(delta), 1e-5);
    float cosT = dot(delta / r, -d);
    float g = projvol_g;
    float phase = (1.0 - g*g) / (4.0*M_PI*pow(max(1.0+g*g-2.0*g*cosT, 1e-4), 1.5));
    float feather = 1.0;
    if (projvol_feather > 0.0)
    {
        vec2 edge = min(uv, vec2(1.0)-uv);
        feather = smoothstep(0.0, projvol_feather, min(edge.x, edge.y));
    }
    return calcLegacyDistanceAttenuation(r / size, falloff) * phase * feather;
}
#ifdef PROJVOL_CONSERVATIVE_SHADOW
// [ConservativeShadow] volume-march variant: no receiver bias, out-of-map = 0,
// tight PCF, occluded-center leak clamp, no sun-cascade fade (shadowUtil.glsl).
float sampleSpotShadowConservative(vec3 pos, int index);
#endif

float projvolHardShadow(vec3 pos, vec4 clip)
{
    float visibility = projvolAirborneShadow(clip);
#ifdef PROJVOL_CONSERVATIVE_SHADOW
    if (projvol_conservative_shadow != 0)
        visibility = min(visibility,sampleSpotShadowConservative(pos,proj_shadow_idx));
#endif
    return visibility;
}

// [BDMerge G3.3 Dust] Dust-volume fetch, adapted from the generated asset's
// dust_volume_sampling.glsl helper (sampleDustVolume3D). Correct units: advect
// the WORLD position in metres first, then convert metres to texture cycles.
// The volume is seamlessly periodic on all axes, so fract() keeps the lookup
// coordinate well-conditioned far from the region origin while GL_REPEAT makes
// the wrap itself seamless. projvol_time is the same continuous-seconds clock
// the fbm noise scroll uses.
#ifdef PROJVOL_DUST_ENABLE
vec4 projvolSampleDust(vec3 world_pos, float cycles_per_metre)
{
    vec3 uvw = fract((world_pos + projvol_dust_wind * projvol_time) * cycles_per_metre);
    return texture(projvol_dust_map, uvw);
}
#endif

// Budgeted beams keep a fixed fine lattice for the inexpensive medium/phase
// integral. Changing a shadow budget must not move sparse dust samples. Each
// cell contributes its exact overlap with the coarse segment, continuously.
float projvolMedium(vec3 wpos)
{
#ifdef PROJVOL_DUST_ENABLE
    bool dust_active = (projvol_dust != 0 && projvol_dust_intensity > 0.0);
#else
    const bool dust_active = false;
#endif
float density = projvol_density;
if (projvol_noise_strength > 0.0 || projvol_fog_strength > 0.0 || dust_active)
{
    // Agent-space (world, Z-up) position of this airborne sample.


    // [item 1] Animated 3D noise -> drifting dust motes / gentle turbulence.
    // World-anchored + slowly scrolled so motes sit in the air and drift,
    // never crawling with the camera. Centred on 1.0 so it varies density
    // symmetrically instead of only dimming it.
    if (projvol_noise_strength > 0.0)
    {
        vec3  np    = wpos * projvol_noise_scale + projvol_wind * projvol_time;
        float n     = projvolFbm(np);                 // ~[0,1], mean ~0.5
        float mote  = 1.0 + projvol_noise_strength * (n * 2.0 - 1.0);
        density    *= max(mote, 0.0);
    }

    // [item 2] Height fog: denser near the ground reference, thinning with
    // altitude, blended in by fog_strength so the beam sits in the air.
    if (projvol_fog_strength > 0.0)
    {
        float h    = wpos.z - projvol_fog_base;       // metres above ground ref
        float hf   = projvol_fog_ground_density * exp(-max(h, 0.0) / max(projvol_fog_falloff, 0.01));
        density   *= mix(1.0, hf, projvol_fog_strength);
    }

    // [BDMerge G3.3 Dust] Baked-volume particulate breakup (composition
    // adapted from the asset's applyCinematicDust helper, README starting
    // values baked in: mote threshold 0.78, softness 0.06, amount 0.35,
    // brightness 2.0). Two frequency reads per the asset README: R (macro
    // haze) + B-adjacent breakup at the base scale, and G (sparse motes)
    // at 5x the base scale so individual motes stay small and readable
    // without a second texture. World-anchored + wind-advected, so the
    // motes hang in the air and drift instead of crawling with the camera.
#ifdef PROJVOL_DUST_ENABLE
    if (dust_active)
    {
        vec4  dmac  = projvolSampleDust(wpos, projvol_dust_scale);
        vec4  dfine = projvolSampleDust(wpos, projvol_dust_scale * 5.0);
        // Macro haze: broad breakup of the uniform cone, centred near 1.
        float cloud = mix(1.0, 0.65 + dmac.r * 0.70,
                          clamp(projvol_dust_intensity, 0.0, 1.0));
        // Sparse motes: thresholded G so ~4% of space carries bright grains.
        float motes = smoothstep(0.78, 0.84, dfine.g);
        float particulate = 1.0 + motes * (0.35 * projvol_dust_intensity) * 2.0;
        // [Review fix] cheap belt-and-suspenders guard (one clamp, no
        // branch): the CPU clamps the dust levers to finite ranges, so
        // the combined factor is provably bounded (< 4.6 at the caps) -
        // but a non-finite value slipping in from any future upload path
        // must not be able to poison the march with Inf/NaN density.
        density *= clamp(cloud * particulate, 0.0, 16.0);
    }
#endif
}
    return density;
}

void main()
{
    vec2 tc = vary_fragcoord.xy;

    // View-space surface position at this pixel (opaque depth defines the far
    // limit of the march - never scatter behind solid geometry).
    vec4  pos       = getPosition(tc);
    ivec2 depth_res = textureSize(depthMap, 0);
    // Odd viewport dimensions do not map to aligned 2x2 footprints; retain the
    // centered depth there rather than mislabeling a neighboring surface.
    if (projvol_half_res.x > 0.0 && ((depth_res.x | depth_res.y) & 1) == 0)
    {
        // Alternate near/far termination from each 2x2 footprint. Reconstruct
        // on the HALF texel's ray, and cache this exact selection for upsample.
        ivec2 p = ivec2(gl_FragCoord.xy);
        ivec2 res = depth_res;
        ivec2 base = p * 2;
        vec4 z;
        z.x = texelFetch(depthMap, clamp(base, ivec2(0), res-1), 0).r;
        z.y = texelFetch(depthMap, clamp(base+ivec2(1,0), ivec2(0), res-1), 0).r;
        z.z = texelFetch(depthMap, clamp(base+ivec2(0,1), ivec2(0), res-1), 0).r;
        z.w = texelFetch(depthMap, clamp(base+ivec2(1,1), ivec2(0), res-1), 0).r;
        float near_z = min(min(z.x,z.y), min(z.z,z.w));
        float far_z = max(max(z.x,z.y), max(z.z,z.w));
        vec4 near_pos = getPositionWithDepth(tc, near_z);
        vec4 far_pos = getPositionWithDepth(tc, far_z);
        float near_d = length(near_pos.xyz);
        // Keep the centered depth on continuous/sloped surfaces. Min/max is
        // needed only at an actual silhouette, where the footprint spans layers.
        // Window depth is affine over a projected plane. A first derivative
        // alone mistakes a grazing wall for a silhouette; compare the footprint
        // to its immediate neighbours and require a change in that derivative.
        if (length(far_pos.xyz)-near_d > max(near_d*0.02, 0.03))
        {
            float residual = 0.0;
            for (int j = 0; j < 2; ++j)
            for (int i = 0; i < 2; ++i)
            {
                ivec2 q = base + ivec2(i,j);
                float c = texelFetch(depthMap, clamp(q, ivec2(0), res-1), 0).r;
                // A repeated edge texel turns a second derivative into a first
                // derivative. Test only axes with both opposite neighbours;
                // the other pixel in the footprint still tests that axis.
                vec2 dd = vec2(0.0);
                if (q.x > 0 && q.x < res.x-1)
                    dd.x = texelFetch(depthMap, q-ivec2(1,0), 0).r
                         + texelFetch(depthMap, q+ivec2(1,0), 0).r - 2.0*c;
                if (q.y > 0 && q.y < res.y-1)
                    dd.y = texelFetch(depthMap, q-ivec2(0,1), 0).r
                         + texelFetch(depthMap, q+ivec2(0,1), 0).r - 2.0*c;
                residual = max(residual, max(abs(dd.x), abs(dd.y)));
            }
            bool discontinuity = residual > max((far_z-near_z)*0.25, 2e-6);
            if (discontinuity)
                pos = ((p.x ^ p.y) & 1) == 0 ? near_pos : far_pos;
        }
    }
    float t_surface = length(pos.xyz);
    projvol_termination = t_surface;
    if (t_surface <= 0.0)
    {
        frag_color = vec4(0.0);
        return;
    }

    // View ray: camera origin (0,0,0 in view space) toward this pixel.
    vec3 d = pos.xyz / t_surface;

    // ---- Bounded march (the biggest win over the sun path) -----------------
    // Intersect the view ray with the projector's sphere of influence
    // (center, size). With O = 0: |t*d - C|^2 = R^2 -> t^2 - 2(d.C)t + (|C|^2 - R^2) = 0.
    // Concentrate the whole sample budget in [t0,t1] instead of marching the
    // full view ray, so far fewer samples give a cleaner shaft.
    vec3  C    = center;
    float R    = size;
    float b    = dot(d, C);
    float disc = b * b - (dot(C, C) - R * R);
    if (disc <= 0.0)
    {
        frag_color = vec4(0.0);   // ray misses the light's range entirely
        return;
    }
    float sq = sqrt(disc);
    float t0 = max(b - sq, 0.0);
    float t1 = min(b + sq, t_surface);
    if (t1 <= t0)
    {
        frag_color = vec4(0.0);
        return;
    }

    // [BDMerge G3.3 Batch A - E1] Frustum-clipped march. The sphere [t0,t1] above
    // is a loose bound; for a narrow cone most steps then `continue` out of the
    // cookie test after paying the projection math. When enabled, slab-clip the
    // view ray (origin = view-space 0, dir = d) against the projector's frustum
    // planes (uploaded per cone in view space) and INTERSECT with the sphere
    // interval so the sample budget concentrates in the lit segment. Only the 4
    // side planes + near are applied: they exactly match the per-sample cookie/near
    // test (clipProjectedLightVars: proj_tc.xy in [0,1] and projected_point.z >= 0),
    // so the clipped interval is a strict SUPERSET of the lit interval. The FAR
    // plane (index 5) is deliberately skipped - the sphere of influence extends
    // past the projector's far clip and the per-sample test still lights those
    // samples, so clipping far would remove lit space. This bounds airborne
    // integration; surface rims retain their own projector evaluation below.
    if (projvol_frustum_clip != 0)
    {
        float ct0 = t0;
        float ct1 = t1;
        bool  ok  = true;
        for (int p = 0; p < 5; ++p) // 0=L 1=R 2=B 3=T 4=near  (5=far intentionally skipped)
        {
            vec4  pl    = projvol_frustum_planes[p];
            float denom = dot(pl.xyz, d); // rate of the plane value along the ray
            float f0    = pl.w;           // plane value at t=0 (the ray origin)
            if (denom > 1e-6)
            {
                ct0 = max(ct0, -f0 / denom); // ray ENTERS this half-space at this t
            }
            else if (denom < -1e-6)
            {
                ct1 = min(ct1, -f0 / denom); // ray LEAVES this half-space at this t
            }
            else if (f0 < 0.0)
            {
                ok = false; // parallel to the plane and on its outside -> empty
            }
        }
        if (ok && ct1 > ct0)
        {
            t0 = ct0;
            t1 = ct1;
        }
        else if (projvol_rim_strength <= 0.0)
        {
            frag_color = vec4(0.0);
            return;
        }
        else
        {
            // Surface rim has its own projector test. In particular AirborneTap
            // may reject an air interval that still has a valid surface rim.
            t1 = t0;
        }
    }

    // Artist-facing shaft-length cap. l_dist at a point t*d is affine in t:
    //   l_dist(t) = -dot(C, proj_n) + t*dot(d, proj_n)
    // Clip the march interval against that far plane so the full sample budget
    // stays concentrated in the visible part of the beam. Surface lighting and
    // the post-march rim evaluation remain intentionally uncapped.
    bool shaft_empty = t1 <= t0;
    if (projvol_max_distance > 0.0)
    {
        float axis_origin = -dot(C, proj_n);
        float axis_rate   = dot(d, proj_n);
        if (axis_rate > 1e-6)
        {
            t1 = min(t1, (projvol_max_distance - axis_origin) / axis_rate);
        }
        else if (axis_rate < -1e-6)
        {
            t0 = max(t0, (projvol_max_distance - axis_origin) / axis_rate);
        }
        else if (axis_origin > projvol_max_distance)
        {
            shaft_empty = true;
        }
        if (t1 <= t0)
        {
            // Do not return: the surface-coupled rim below is intentionally
            // independent of the airborne shaft-length cap. Collapse the march
            // interval and let the shader continue to that evaluation.
            shaft_empty = true;
            t1 = t0;
        }
    }

    float march_len = t1 - t0;
    // An empty cap interval and godray_res == 0 both mean rim-only: skip the
    // airborne loop while preserving the later surface evaluation. max() keeps
    // dt finite so 0*inf can never poison the final shaft.
    int shaft_steps = shaft_empty ? 0 : godray_res;
    float dt        = march_len / max(float(shaft_steps), 1.0); // physical step length
#if defined(PROJVOL_BUDGET_ENABLE) && PROJVOL_BUDGET_ENABLE
    // The CPU uploads zero GODRAY_RES for a zero share/rim-only draw. Keep
    // quality specialization compile-time, just like Full, so the driver does
    // not retain a second complete integration kernel in the budget program.
    const bool budgeted = true;
#else
    const bool budgeted = false;
#endif
    float step_count = float(shaft_steps);
    if (budgeted && shaft_steps > 0)
    {
        // A sub-one share uses one whole-chord quadrature cell. Normalize it
        // directly rather than dividing by a vanishing share at screen entry.
        step_count = max(projvol_budget_steps,1.0);
        shaft_steps = int(ceil(step_count));
        dt = march_len / step_count;
    }

    // [Phase 1 item 2] blue-noise (interleaved gradient) march-start offset.
    // Static per pixel-quad => no crawl under camera motion; optional frame rotation.
    // [BDMerge G3.3 DitherQuad] ONE offset per 2x2 pixel QUAD (IGN input quantized
    // with floor(frag*0.5)) instead of fully per-pixel. A per-pixel random march
    // start DECORRELATES the expensive raymarch across neighboring lanes: adjacent
    // pixels march at different depths, so every step issues incoherent shadow-map
    // reads (12-32 of them per step with soft projector shadows), divergent
    // clipProjectedLightVars() clips and a less coherent luminance early-out - that
    // warp divergence, not the hash cost, is what tanked FPS whenever the dither
    // lever left 0. Sharing the offset across each hardware 2x2 quad keeps the
    // march cache/lane-coherent while the dither/temporal averaging hides the
    // coarser pattern. Shader modes: 0 = off (centred 0.5), 1 = static, 2 =
    // animated (golden-ratio presentation-time walk). Mode 1 always stays static.
    float roffset;
    if (projvol_dither == 0)
    {
        roffset = 0.5; // centred samples, no dither
    }
    else
    {
        float ign = interleavedGradientNoise(floor(gl_FragCoord.xy *
                    (projvol_half_res.x > 0.0 ? 1.0 : 0.5)));
        if (projvol_dither == 2)
        {
            ign = fract(ign + projvol_frame * 0.61803399); // golden-ratio temporal walk
        }
        roffset = ign;
    }

    // [BDMerge G3.3 Dust] loop-invariant gate: at the shipped defaults
    // (permutation compiled out / projvol_dust == 0) every dust branch below is
    // skipped and the march is byte-identical to today.
#ifdef PROJVOL_DUST_ENABLE
    bool dust_active = (projvol_dust != 0 && projvol_dust_intensity > 0.0);
#else
    const bool dust_active = false; // dust compiled out - folds every dust branch away
#endif
    bool fine_detail = dust_active ||
        projvol_equiangular != 0 || projvol_cookie_footprint != 0 || projvol_airborne_tap != 0;
    // Weak noise can use a cheaper fixed medium partition. Blend continuously
    // into the fine medium before strong or high-frequency noise; fog and dust
    // always retain it. Sparse weak nodes must not brighten dense shadow lanes.
    // This is a runtime weight, never a shader/history selection key.
    float medium_detail = (dust_active || projvol_fog_strength > 0.0) ? 1.0 :
        max(smoothstep(0.125,0.5,projvol_noise_strength),
            smoothstep(1.2,2.0,projvol_noise_scale)*smoothstep(0.0,0.125,projvol_noise_strength));

    vec3 accum = vec3(0.0);

    // [BDMerge G3.3 Batch B - R1] Luminance-weighted first distance moment.
    // Keeping it premultiplied makes GL_ONE,GL_ONE overlap across cones correct;
    // the temporal resolve divides the summed moment by summed shaft luminance.
    float depth_w = 0.0;
    float w_sum   = 0.0;

    // All transformed coordinates are affine along a view ray. The opt-in
    // airborne path reuses these derivatives instead of per-step mat4 products.
    vec4 projector_origin = vec4(0.0), projector_rate = vec4(0.0);
    if (projvol_airborne_tap != 0 || projvol_equiangular != 0 || projvol_cookie_footprint != 0 || projvol_hard_beam_shadows != 0)
    {
        projector_origin = proj_mat[3];
        projector_rate = proj_mat * vec4(d, 0.0);
    }
    vec4 shadow_origin = vec4(0.0), shadow_rate = vec4(0.0);
    vec3 world_origin = vec3(0.0), world_rate = vec3(0.0);
    if (projvol_airborne_tap != 0 || projvol_hard_beam_shadows != 0)
    {
        shadow_origin = shadow_matrix[4 + clamp(proj_shadow_idx, 0, 9)][3];
        shadow_rate = shadow_matrix[4 + clamp(proj_shadow_idx, 0, 9)] * vec4(d, 0.0);
        world_origin = projvol_inv_modelview[3].xyz;
        world_rate = mat3(projvol_inv_modelview) * d;
    }
    float D = 0.1;
    vec2 angles = vec2(0.0);
    if (projvol_equiangular != 0)
    {
        D = sqrt(max(dot(C,C)-b*b, 0.01)); // >= 0.1 metres
        angles = vec2(atan(t0-b, D), atan(t1-b, D));
    }

    // Nested quadrature: split one fixed cell at a time, cross-fading its
    // parent and children. A fractional budget changes only continuous weights,
    // never a shadow tap's position. At powers of two the partitions coincide.
    // Retain the four-cell visibility floor at very small shares.
    int shadow_base = 4, shadow_splits = 0;
    float shadow_blend = 0.0;
    // Adjacent coarse segments can overlap the same fine cell. Cache its
    // radiance (before interval weight), so hard detail is evaluated once.
    int hard_cached_cell = -1;
    vec3 hard_cached_radiance = vec3(0.0);
    float hard_cached_moment = 0.0;
    if (budgeted && shaft_steps > 0 && projvol_hard_beam_shadows == 0)
    {
        float count = max(step_count,4.0);
        shadow_base = int(exp2(floor(log2(count))));
        shadow_splits = int(floor(count))-shadow_base;
        shadow_blend = fract(count);
        shaft_steps = shadow_base+shadow_splits+(shadow_blend > 0.0 ? 2 : 0);
    }

    if (budgeted && shaft_steps > 0 && projvol_hard_beam_shadows != 0 &&
        projvol_equiangular == 0 && projvol_cookie_footprint == 0)
    {
        // Hard visibility and heterogeneous medium share fixed fine cells.
        // Visit each once, independently of the radiance budget. At low budgets
        // only the smooth attenuation/phase term uses a quadratic interpolant;
        // rapidly varying light falls back continuously to exact evaluation.
        // Uniform runtime branches keep the weak kernel's fine loop free of
        // noise evaluation. Blend complete premultiplied integrals, so changing
        // noise strength never selects a shader or introduces a field jump.
        if (medium_detail < 1.0)
        {
            float fine_dt = march_len/max(float(godray_res),1.0);
            int cached_segment = -1;
            vec3 radiance_nodes = vec3(0.0);
            float radiance_start = t0, radiance_length = dt;
            // Evaluate weak-medium nodes outside the fine loop. Keeping the costly
            // noise function out of its loop-carried state also reduces registers.
            vec2 weak_medium = vec2(projvol_density);
            if (projvol_noise_strength > 0.0)
            {
                weak_medium.x = projvolMedium(world_origin+world_rate*(t0+0.25*march_len));
                weak_medium.y = projvolMedium(world_origin+world_rate*(t0+0.75*march_len));
            }
            for (int j = 0; j < godray_res; ++j)
            {
                float start = t0+float(j)*fine_dt;
                float ft = start+roffset*fine_dt;
                vec4 fp = projector_origin+projector_rate*ft;
                vec2 uv = fp.xy/max(fp.w,1e-5);
                if (fp.z < 0.0 || fp.w <= 1e-5 || any(lessThan(uv,vec2(0.0))) || any(greaterThan(uv,vec2(1.0)))) continue;
                float analytic = 0.0;
                // Above five, the straight fine-cell kernel is cheaper than
                // maintaining interpolants and already meets the Full cost bound.
                float exact_weight = smoothstep(4.0,5.0,step_count);
                if (step_count < 5.0)
                {
                    int segment = int(floor((ft-t0)/max(dt,1e-6)));
                    if (segment != cached_segment)
                    {
                        radiance_start = t0+float(segment)*dt;
                        radiance_length = min(dt,t1-radiance_start);
                        float mid = radiance_start+0.5*radiance_length;
                        float end = radiance_start+radiance_length;
                        radiance_nodes = vec3(
                            projvolAnalytic(d,radiance_start,projector_origin+projector_rate*radiance_start),
                            projvolAnalytic(d,mid,projector_origin+projector_rate*mid),
                            projvolAnalytic(d,end,projector_origin+projector_rate*end));
                        cached_segment = segment;
                    }
                    float x = clamp((ft-radiance_start)/max(radiance_length,1e-6),0.0,1.0);
                    analytic = max(dot(radiance_nodes,vec3(2.0*x*x-3.0*x+1.0,4.0*x*(1.0-x),2.0*x*x-x)),0.0);
                    float largest = max(max(radiance_nodes.x,radiance_nodes.y),radiance_nodes.z);
                    float curvature = abs(radiance_nodes.x-2.0*radiance_nodes.y+radiance_nodes.z)/max(largest,1e-8);
                    exact_weight = max(exact_weight,smoothstep(0.3,0.6,curvature));
                }
                if (exact_weight > 0.0)
                    analytic = mix(analytic,projvolAnalytic(d,ft,fp),exact_weight);
                float visibility = 0.0;
                int taps = projvol_shadow_jitter_tap != 0 ? 1 : max(projvol_shadow_samples,1);
                for (int s = 0; s < taps; ++s)
                {
                    float ts = start+(float(s)+roffset)*fine_dt/float(taps);
                    visibility += projvolHardShadow(d*ts,shadow_origin+shadow_rate*ts);
                }
                vec3 cookie = projGoboTexture(-dot(center-d*ft,proj_n),uv);
                // Interpolate the weak medium on fixed chord nodes, independent
                // of the shadow/radiance budget. Keep medium and visibility jointly
                // weighted in each fine cell, including the premultiplied moment.
                float cell = (float(j)+roffset)/max(float(godray_res),1.0);
                float medium = mix(weak_medium.x,weak_medium.y,clamp(2.0*cell-0.5,0.0,1.0));
                vec3 value = fine_dt*analytic*medium*mix(cookie*projvol_shadow_tint,cookie,visibility/float(taps));
                value *= 1.0-medium_detail;
                accum += value;
                if (projvol_temporal_beam_depth != 0)
                {
                    float luma = dot(value*color,vec3(0.2126,0.7152,0.0722));
                    depth_w += luma*ft;
                    w_sum += luma;
                }
            }
        }
        if (medium_detail > 0.0)
        {
            float fine_dt = march_len/max(float(godray_res),1.0);
            int cached_segment = -1;
            vec3 radiance_nodes = vec3(0.0);
            float radiance_start = t0, radiance_length = dt;
            for (int j = 0; j < godray_res; ++j)
            {
                float start = t0+float(j)*fine_dt;
                float ft = start+roffset*fine_dt;
                vec4 fp = projector_origin+projector_rate*ft;
                vec2 uv = fp.xy/max(fp.w,1e-5);
                if (fp.z < 0.0 || fp.w <= 1e-5 || any(lessThan(uv,vec2(0.0))) || any(greaterThan(uv,vec2(1.0)))) continue;
                float analytic = 0.0;
                // Above five, the straight fine-cell kernel is cheaper than
                // maintaining interpolants and already meets the Full cost bound.
                float exact_weight = smoothstep(4.0,5.0,step_count);
                if (step_count < 5.0)
                {
                    int segment = int(floor((ft-t0)/max(dt,1e-6)));
                    if (segment != cached_segment)
                    {
                        radiance_start = t0+float(segment)*dt;
                        radiance_length = min(dt,t1-radiance_start);
                        float mid = radiance_start+0.5*radiance_length;
                        float end = radiance_start+radiance_length;
                        radiance_nodes = vec3(
                            projvolAnalytic(d,radiance_start,projector_origin+projector_rate*radiance_start),
                            projvolAnalytic(d,mid,projector_origin+projector_rate*mid),
                            projvolAnalytic(d,end,projector_origin+projector_rate*end));
                        cached_segment = segment;
                    }
                    float x = clamp((ft-radiance_start)/max(radiance_length,1e-6),0.0,1.0);
                    analytic = max(dot(radiance_nodes,vec3(2.0*x*x-3.0*x+1.0,4.0*x*(1.0-x),2.0*x*x-x)),0.0);
                    float largest = max(max(radiance_nodes.x,radiance_nodes.y),radiance_nodes.z);
                    float curvature = abs(radiance_nodes.x-2.0*radiance_nodes.y+radiance_nodes.z)/max(largest,1e-8);
                    exact_weight = max(exact_weight,smoothstep(0.3,0.6,curvature));
                }
                if (exact_weight > 0.0)
                    analytic = mix(analytic,projvolAnalytic(d,ft,fp),exact_weight);
                float visibility = 0.0;
                int taps = projvol_shadow_jitter_tap != 0 ? 1 : max(projvol_shadow_samples,1);
                for (int s = 0; s < taps; ++s)
                {
                    float ts = start+(float(s)+roffset)*fine_dt/float(taps);
                    visibility += projvolHardShadow(d*ts,shadow_origin+shadow_rate*ts);
                }
                vec3 cookie = projGoboTexture(-dot(center-d*ft,proj_n),uv);
                float medium = projvolMedium(world_origin+world_rate*ft);
                vec3 value = fine_dt*analytic*medium*mix(cookie*projvol_shadow_tint,cookie,visibility/float(taps));
                value *= medium_detail;
                accum += value;
                if (projvol_temporal_beam_depth != 0)
                {
                    float luma = dot(value*color,vec3(0.2126,0.7152,0.0722));
                    depth_w += luma*ft;
                    w_sum += luma;
                }
            }
        }
        // The common uniform-distance composite multiplies the integral by dt.
        accum /= dt;
        depth_w /= dt;
        w_sum /= dt;
        shaft_steps = 0;
    }

    if (!budgeted || projvol_hard_beam_shadows == 0 || projvol_equiangular != 0 || projvol_cookie_footprint != 0)
    for (int i = 0; i < shaft_steps; ++i)
    {
        float t    = t0 + (float(i) + roffset) * dt;
        float segment_dt = dt;
        float segment_start = t0 + float(i) * dt;
        float fraction = 1.0;
        float quadrature_weight = 1.0;
        float cell_start = 0.0, cell_length = 0.0;
        if (budgeted)
        {
            fraction = min(step_count-float(i), 1.0);
            segment_dt = dt*fraction;
            t = segment_start + roffset*segment_dt;
            if (projvol_hard_beam_shadows == 0)
            {
                int children = 2*shadow_splits;
                if (i < children)
                {
                    cell_start = float(i)*0.5;
                    cell_length = 0.5;
                }
                else if (shadow_blend > 0.0 && i < children+3)
                {
                    int part = i-children;
                    cell_start = float(shadow_splits)+float(max(part-1,0))*0.5;
                    cell_length = part == 0 ? 1.0 : 0.5;
                    quadrature_weight = part == 0 ? 1.0-shadow_blend : shadow_blend;
                }
                else
                {
                    cell_start = float(i-shadow_splits-(shadow_blend > 0.0 ? 2 : 0));
                    cell_length = 1.0;
                }
                cell_start /= float(shadow_base);
                cell_length /= float(shadow_base);
                segment_start = t0+cell_start*march_len;
                segment_dt = cell_length*march_len;
                t = segment_start+roffset*segment_dt;
            }
        }
        if (projvol_equiangular != 0)
        {
            float inv_steps = 1.0 / max(float(shaft_steps), 1.0);
            if (budgeted) inv_steps = 1.0 / max(step_count, 1e-6);
            float start = float(i)*inv_steps;
            float endpoint = float(i+1)*inv_steps;
            if (budgeted) endpoint = min(endpoint, 1.0);
            if (budgeted && projvol_hard_beam_shadows == 0)
            {
                start = cell_start;
                endpoint = cell_start+cell_length;
            }
            segment_start = b + D*tan(mix(angles.x, angles.y, start));
            float segment_end = b + D*tan(mix(angles.x, angles.y, endpoint));
            // Pin endpoints to avoid atan/tan roundoff outside the clipped cone.
            if (start == 0.0) segment_start = t0;
            if (endpoint == 1.0) segment_end = t1;
            segment_dt = max(segment_end-segment_start, 0.0);
            t = segment_start + roffset * segment_dt;
        }
        vec3  spos = d * t;               // view-space sample point

        // Frustum clip + projector coords, reusing spotLightF's exact math.
        // clipProjectedLightVars gives dist (|C-spos|/size), l_dist (depth along
        // the projector axis) and proj_tc in one call. Returns true = discard.
        vec3  lv;
        vec4  proj_tc;
        float dist, l_dist;
        bool clipped;
        if (projvol_airborne_tap != 0)
        {
            lv = C - spos;
            dist = length(lv) / size;
            l_dist = -dot(lv, proj_n);
            proj_tc = projector_origin + projector_rate*t;
            clipped = dist >= 1.0 || proj_tc.z < 0.0 || proj_tc.w <= 1e-5;
            proj_tc.xyz /= max(proj_tc.w, 1e-5);
        }
        else clipped = clipProjectedLightVars(C, spos, dist, l_dist, lv, proj_tc);
        if (clipped)
        {
            continue;
        }
        // Cookie footprint (matches spotLightF proj_tc.xy in [0,1] test).
        if (proj_tc.x < 0.0 || proj_tc.x > 1.0 ||
            proj_tc.y < 0.0 || proj_tc.y > 1.0)
        {
            continue;
        }

        // [Phase 1 item 6] Feathered cone edge: soften the angular falloff so the
        // beam boundary is not a hard line. Distance to the nearest cookie border
        // (0 at the frustum edge), smoothstepped over projvol_feather.
        float edge_feather = 1.0;
        if (projvol_feather > 0.0)
        {
            vec2  e2   = min(proj_tc.xy, vec2(1.0) - proj_tc.xy);
            float edge = min(e2.x, e2.y);
            edge_feather = smoothstep(0.0, projvol_feather, edge);
        }

#ifdef PROJVOL_CONSERVATIVE_SHADOW
        // [BDMerge G3.3 ConservativeShadow] Numerical guards for the axis-aligned
        // failure case (viewer + occluder + light collinear). proj_tc.w is the
        // projector-projected w (clipProjectedLightVars divides only .xyz), so
        // w <= 1e-5 means the sample sits at/behind the projector's eye plane and
        // the projected cookie/footprint coords are meaningless (NaN/inf can even
        // PASS the [0,1] range tests above, since comparisons with NaN are false).
        // light_dist2 <= 1e-8 is the on-axis singularity: the sample essentially
        // AT the light center, where normalize() below explodes. Both are gated
        // behind the permutation + runtime gate, so with the lever off this block
        // does not exist and the legacy path is instruction-identical.
        float light_dist2 = dot(lv, lv); // lv = C - spos (unnormalized, from clip vars)
        if (projvol_conservative_shadow != 0 &&
            (proj_tc.w <= 1e-5 || light_dist2 <= 1e-8))
        {
            continue;
        }
#endif

        // Volumetric self-shadowing: sample THIS projector's own shadow map.
        // No surface normal for an airborne sample, so pass 0 (the norm*offset
        // bias term is negligible in the shaft - accepted approximation).
        // [Phase 1 item 7] Crisp occluder shadows: take projvol_shadow_samples
        // sub-taps spread across the step so thin occluders (bars/foliage) resolve
        // into sharp god-ray bands instead of being blurred by the coarse march.
        float vis;
        vec3 hard_integrated = vec3(0.0), hard_integrated_moment = vec3(0.0);
        float shadow_t = (projvol_equiangular != 0) ? segment_start + 0.5*segment_dt : t;
        vec3 shadow_pos = d * shadow_t;
        if (projvol_hard_beam_shadows != 0)
        {
            // Keep the user's longitudinal sub-taps for thin bars/foliage.
            // Hard only replaces the wide surface penumbra with an unbiased
            // hardware depth comparison. Surface/rim code below stays soft.
            vis = budgeted ? 1.0 : 0.0;
            if (!budgeted)
            {
                int taps = projvol_shadow_jitter_tap != 0 ? 1 : max(projvol_shadow_samples, 1);
                for (int s = 0; s < taps; ++s)
                {
                    float ts = segment_start + (float(s)+roffset)*segment_dt/float(taps);
                    vis += projvolHardShadow(d*ts,shadow_origin + shadow_rate*ts);
                }
                vis /= float(taps);
            }
            else
            {
                // The cap controls the coarse radiance partition. Retain
                // fixed-position cheap DEPTH taps solely to protect thin bars.
                // Integrate shadowed energy and its moment in the SAME fine
                // cell. Occluded dense medium cannot inherit a lit cell's light.
                float fine_dt = march_len/max(float(godray_res),1.0);
                float first = clamp((segment_start-t0)/max(fine_dt,1e-6),0.0,float(godray_res));
                float last = clamp((segment_start+segment_dt-t0)/max(fine_dt,1e-6),first,float(godray_res));
                if (projvol_equiangular != 0)
                {
                    float angle_scale = float(godray_res)/max(angles.y-angles.x,1e-12);
                    first = clamp((atan(segment_start-b,D)-angles.x)*angle_scale,0.0,float(godray_res));
                    last = clamp((atan(segment_start+segment_dt-b,D)-angles.x)*angle_scale,first,float(godray_res));
                }
                for (int j = int(floor(first)); j < int(ceil(last)); ++j)
                {
                    float fine_start = t0+float(j)*fine_dt;
                    float fine_end = fine_start+fine_dt;
                    if (projvol_equiangular != 0)
                    {
                        fine_start = b+D*tan(mix(angles.x,angles.y,float(j)/float(godray_res)));
                        fine_end = b+D*tan(mix(angles.x,angles.y,min(float(j+1)/float(godray_res),1.0)));
                        if (j == 0) fine_start = t0;
                        if (j == godray_res-1) fine_end = t1;
                    }
                    float length = max(fine_end-fine_start,0.0);
                    float weight = max(min(segment_start+segment_dt,fine_end)-max(segment_start,fine_start),0.0);
                    if (j != hard_cached_cell)
                    {
                        float ft = fine_start+roffset*length;
                        vec4 fp = projector_origin+projector_rate*ft;
                        float analytic = projvolAnalytic(d,ft,fp);
                        float fine_moment = ft;
                        if (projvol_equiangular != 0)
                        {
                            // Match the analytic radiance integral below exactly.
                            const float gauss = 0.28867513459;
                            float mid = fine_start+0.5*length;
                            float ta = mid-gauss*length, tb = mid+gauss*length;
                            float fa = projvolAnalytic(d,ta,projector_origin+projector_rate*ta);
                            float fb = projvolAnalytic(d,tb,projector_origin+projector_rate*tb);
                            analytic = 0.5*(fa+fb);
                            fine_moment = fa+fb > 1e-8 ? (fa*ta+fb*tb)/(fa+fb) : mid;
                        }
                        // The segment's footprint-filtered cookie is applied to
                        // the joint shadowed integral below.
                        vec3 fc = projvol_cookie_footprint != 0 ? vec3(1.0) :
                            projGoboTexture(-dot(center-d*ft,proj_n),fp.xy/max(fp.w,1e-5));
                        vec3 energy = analytic*fc;
                        float medium = projvolMedium((projvol_inv_modelview*vec4(d*ft,1.0)).xyz);
                        float visibility = 0.0;
                        int taps = projvol_shadow_jitter_tap != 0 ? 1 : max(projvol_shadow_samples,1);
                        for (int s = 0; s < taps; ++s)
                        {
                            float ts = fine_start+(float(s)+roffset)*length/float(taps);
                            visibility += projvolHardShadow(d*ts,shadow_origin+shadow_rate*ts);
                        }
                        hard_cached_radiance = energy*medium*mix(vec3(projvol_shadow_tint),vec3(1.0),visibility/float(taps));
                        hard_cached_moment = fine_moment;
                        hard_cached_cell = j;
                    }
                    vec3 shadowed_energy = weight*hard_cached_radiance;
                    hard_integrated += shadowed_energy;
                    hard_integrated_moment += shadowed_energy*hard_cached_moment;
                }
            }
        }
        else if (projvol_airborne_tap != 0)
        {
            vis = projvolAirborneShadow(shadow_origin + shadow_rate*shadow_t);
        }
        else
        // [BDMerge G3.3 Batch A - E2] One IGN-jittered shadow tap instead of N
        // sub-taps. spos = d*(t0+(i+roffset)*dt) already carries the per-quad
        // (2x2, see DitherQuad above) interleaved-gradient offset ALONG the step,
        // so a single tap here is decorrelated exactly like the sub-taps and the
        // dither/temporal path averages it - at up to 4x fewer shadow fetches.
        // When the jitter gate is on it overrides ShadowSamples>1. Gate off
        // (default) => the condition is exactly `projvol_shadow_samples <= 1` and
        // both legacy paths are unchanged.
        // [ConservativeShadow] When the conservative gate is compiled in AND on
        // (opt-in, default off), all airborne taps go through
        // sampleSpotShadowConservative instead of the surface sampler - same tap
        // layout/averaging, leak-proof kernel. With the permutation off the
        // whole branch does not exist and the legacy dispatch below compiles
        // exactly as shipped.
#ifdef PROJVOL_CONSERVATIVE_SHADOW
        if (projvol_conservative_shadow != 0)
        {
            if (projvol_equiangular != 0 || projvol_shadow_jitter_tap != 0 || projvol_shadow_samples <= 1)
            {
                vis = sampleSpotShadowConservative(shadow_pos, proj_shadow_idx);
            }
            else
            {
                vis = 0.0;
                for (int s = 0; s < projvol_shadow_samples; ++s)
                {
                    float ts   = t + (float(s) - float(projvol_shadow_samples - 1) * 0.5) * (segment_dt / float(projvol_shadow_samples));
                    if (budgeted) ts = clamp(ts, segment_start, segment_start+segment_dt);
                    vec3  sp   = d * ts;
                    vis       += sampleSpotShadowConservative(sp, proj_shadow_idx);
                }
                vis /= float(projvol_shadow_samples);
            }
        }
        else
#endif
        if (projvol_equiangular != 0 || projvol_shadow_jitter_tap != 0 || projvol_shadow_samples <= 1)
        {
            vis = sampleSpotShadow(shadow_pos, vec3(0.0), proj_shadow_idx, tc);
        }
        else
        {
            vis = 0.0;
            for (int s = 0; s < projvol_shadow_samples; ++s)
            {
                float ts   = t + (float(s) - float(projvol_shadow_samples - 1) * 0.5) * (segment_dt / float(projvol_shadow_samples));
                if (budgeted) ts = clamp(ts, segment_start, segment_start+segment_dt);
                vec3  sp   = d * ts;
                vis       += sampleSpotShadow(sp, vec3(0.0), proj_shadow_idx, tc);
            }
            vis /= float(projvol_shadow_samples);
        }

        // Antithetic fixed taps suppress single-tap camera aliasing at low
        // budgets. Fade only their weights; neither tap follows the budget.
        // Explicit longitudinal sub-taps, jitter and airborne experiments keep
        // their own filtering. Full and surface shadows are unchanged.
        if (budgeted && projvol_hard_beam_shadows == 0 && projvol_equiangular == 0 &&
            projvol_airborne_tap == 0 && projvol_shadow_jitter_tap == 0 && projvol_shadow_samples <= 1 && step_count < 8.0)
        {
            float pair_t = segment_start+(1.0-roffset)*segment_dt;
            float paired;
#ifdef PROJVOL_CONSERVATIVE_SHADOW
            if (projvol_conservative_shadow != 0) paired = sampleSpotShadowConservative(d*pair_t,proj_shadow_idx);
            else
#endif
                paired = sampleSpotShadow(d*pair_t,vec3(0.0),proj_shadow_idx,tc);
            vis = mix(vis,paired,0.5*(1.0-smoothstep(6.0,8.0,step_count)));
        }

        // Distance + range attenuation, identical to how spotLightF dims the
        // lit surface (inverse-square-ish + range falloff via calcLegacy...).
        float atten = calcLegacyDistanceAttenuation(dist, falloff);

        // Gobo/cone-edge shaped in-scatter. TEXTURE ONLY here - the light color is
        // applied once to the whole shaft below so changing the projector's Light
        // Color visibly re-tints the beam (fix: was baked into the cookie sample).
        vec3 cookie;
        if (projvol_cookie_footprint != 0)
        {
            // Segment UV span, not implicit derivatives inside a divergent loop.
            // Keep the existing optical focus/edge law: it uses proj_lod as a
            // feather scale, so feeding footprint LOD into it changes the cone.
            vec4 qa = projector_origin + projector_rate*segment_start;
            vec4 qb = projector_origin + projector_rate*(segment_start+segment_dt);
            vec2 span = animatedGoboUV(qb.xy/max(qb.w,1e-5)) -
                        animatedGoboUV(qa.xy/max(qa.w,1e-5));
            float footprint_lod = log2(max(length(span*vec2(textureSize(projectionMap,0))), 1.0));
            float focus_lod = clamp((l_dist-proj_focus)/proj_range, 0.0, 1.0)*proj_lod;
            vec4 filtered = sampleGobo(proj_tc.xy, max(focus_lod, footprint_lod));
            // RGB conversion + premultiplied alpha, matching projGoboTexture.
            vec3 filtered_rgb = srgb_to_linear(filtered.rgb) * filtered.a;
            // getTexture2DLodDiffuse multiplies its edge factor into RGB AND A.
            float edge = min(min(proj_tc.x,1.0-proj_tc.x), min(proj_tc.y,1.0-proj_tc.y));
            float feather = 0.25*min(focus_lod/max(proj_lod*0.5,1e-6),1.0);
            float edge_weight = feather > 0.0 ? clamp(edge/feather,0.0,1.0) : 1.0;
            cookie = filtered_rgb * edge_weight * edge_weight;
        }
        else cookie = projGoboTexture(l_dist, proj_tc.xy);

        // [Phase 3] Participating-medium density modulation for this sample. The
        // global haziness master, the animated noise medium and the height fog all
        // fold into one scalar that scales the in-scatter. Defaults keep this at
        // exactly projvol_density (1.0) -> a flat, uniform cone (the shipped look).
        float density = projvol_density;
        if ((!budgeted || (!fine_detail && projvol_hard_beam_shadows == 0)) &&
            (projvol_noise_strength > 0.0 || projvol_fog_strength > 0.0 || dust_active))
        {
            // Agent-space (world, Z-up) position of this airborne sample.
            vec3 wpos = (projvol_airborne_tap != 0)
                      ? world_origin + world_rate*t
                      : (projvol_inv_modelview * vec4(spos, 1.0)).xyz;

            // [item 1] Animated 3D noise -> drifting dust motes / gentle turbulence.
            // World-anchored + slowly scrolled so motes sit in the air and drift,
            // never crawling with the camera. Centred on 1.0 so it varies density
            // symmetrically instead of only dimming it.
            if (projvol_noise_strength > 0.0)
            {
                vec3  np    = wpos * projvol_noise_scale + projvol_wind * projvol_time;
                float n     = projvolFbm(np);                 // ~[0,1], mean ~0.5
                float mote  = 1.0 + projvol_noise_strength * (n * 2.0 - 1.0);
                density    *= max(mote, 0.0);
            }

            // [item 2] Height fog: denser near the ground reference, thinning with
            // altitude, blended in by fog_strength so the beam sits in the air.
            if (projvol_fog_strength > 0.0)
            {
                float h    = wpos.z - projvol_fog_base;       // metres above ground ref
                float hf   = projvol_fog_ground_density * exp(-max(h, 0.0) / max(projvol_fog_falloff, 0.01));
                density   *= mix(1.0, hf, projvol_fog_strength);
            }

            // [BDMerge G3.3 Dust] Baked-volume particulate breakup (composition
            // adapted from the asset's applyCinematicDust helper, README starting
            // values baked in: mote threshold 0.78, softness 0.06, amount 0.35,
            // brightness 2.0). Two frequency reads per the asset README: R (macro
            // haze) + B-adjacent breakup at the base scale, and G (sparse motes)
            // at 5x the base scale so individual motes stay small and readable
            // without a second texture. World-anchored + wind-advected, so the
            // motes hang in the air and drift instead of crawling with the camera.
#ifdef PROJVOL_DUST_ENABLE
            if (dust_active)
            {
                vec4  dmac  = projvolSampleDust(wpos, projvol_dust_scale);
                vec4  dfine = projvolSampleDust(wpos, projvol_dust_scale * 5.0);
                // Macro haze: broad breakup of the uniform cone, centred near 1.
                float cloud = mix(1.0, 0.65 + dmac.r * 0.70,
                                  clamp(projvol_dust_intensity, 0.0, 1.0));
                // Sparse motes: thresholded G so ~4% of space carries bright grains.
                float motes = smoothstep(0.78, 0.84, dfine.g);
                float particulate = 1.0 + motes * (0.35 * projvol_dust_intensity) * 2.0;
                // [Review fix] cheap belt-and-suspenders guard (one clamp, no
                // branch): the CPU clamps the dust levers to finite ranges, so
                // the combined factor is provably bounded (< 4.6 at the caps) -
                // but a non-finite value slipping in from any future upload path
                // must not be able to poison the march with Inf/NaN density.
                density *= clamp(cloud * particulate, 0.0, 16.0);
            }
#endif
        }

        // Per-sample Henyey-Greenstein phase. Because the light is LOCAL the
        // scatter geometry varies per step: Ldir is the light's travel
        // direction at this sample, Vdir points back to the camera. Forward
        // scatter (g>0) glows brightly when the camera looks toward the light.
        // [ConservativeShadow] permutation-gated: normalize via inversesqrt of
        // the already-guarded squared distance (the continue above rejected
        // <= 1e-8), so the on-axis sample can never produce a NaN direction.
        // spos - C == -lv. With the permutation off the select does not exist
        // and the legacy path compiles the exact original normalize().
#if !defined(PROJVOL_BUDGET_ENABLE) || !PROJVOL_BUDGET_ENABLE
#ifdef PROJVOL_CONSERVATIVE_SHADOW
        vec3  Ldir  = (projvol_conservative_shadow != 0)
                    ? (-lv * inversesqrt(max(light_dist2, 1e-8)))
                    : normalize(spos - C);
#else
        vec3  Ldir  = normalize(spos - C);
#endif
        vec3  Vdir  = -d;
        float cosT  = dot(Ldir, Vdir);
        float g     = projvol_g;
        float denom = 1.0 + g * g - 2.0 * g * cosT;
        float phase = (1.0 - g * g) / (4.0 * M_PI * pow(max(denom, 1e-4), 1.5));
#else
        // Every budgeted integrator computes analytic phase below. Rim-only
        // draws have no loop, so this legacy sample is never consumed here.
        const float phase = 0.0;
#endif

        // [Batch 1 B] Gobo-colored occluder shadows. The lit term is the full gobo
        // in-scatter (cookie); the occluded term is that same gobo dimmed by
        // projvol_shadow_tint, so shadow bands stay colored/shaped by the cookie
        // instead of going pure black. mix by visibility: vis=1 -> lit, vis=0 ->
        // occluded. At projvol_shadow_tint == 0 this reduces exactly to vis*cookie
        // (the classic crisp black occluder shadow).
        vec3 scatter = mix(cookie * projvol_shadow_tint, cookie, vis);
        vec3 scatter_sample = atten * phase * scatter * edge_feather * density;
        if (budgeted && projvol_equiangular == 0) scatter_sample *= fraction;
        float moment_t = t;
        if (projvol_equiangular != 0 && !budgeted)
        {
            const float gauss = 0.28867513459; // 1/(2*sqrt(3))
            float mid = segment_start + 0.5*segment_dt;
            float ta = mid-gauss*segment_dt;
            float tb = mid+gauss*segment_dt;
            float fa = projvolAnalytic(d, ta, projector_origin+projector_rate*ta);
            float fb = projvolAnalytic(d, tb, projector_origin+projector_rate*tb);
            float analytic = 0.5*(fa+fb);
            scatter_sample = analytic * scatter * density * segment_dt;
            moment_t = (fa+fb > 1e-8) ? (fa*ta+fb*tb)/(fa+fb) : mid;
        }
        if (budgeted && !fine_detail && projvol_hard_beam_shadows == 0 && step_count > 4.0)
        {
            // Smooth ordinary beams need only two cheap analytic evaluations;
            // sparse dust and hard shadows retain the independent fine cells.
            const float gauss = 0.28867513459;
            float mid = segment_start+0.5*segment_dt;
            float ta = mid-gauss*segment_dt, tb = mid+gauss*segment_dt;
            float fa = projvolAnalytic(d,ta,proj_mat*vec4(d*ta,1.0));
            float fb = projvolAnalytic(d,tb,proj_mat*vec4(d*tb,1.0));
            float analytic = 0.5*(fa+fb);
            float analytic_moment = 0.5*(fa*ta+fb*tb);
            scatter_sample = analytic*scatter*density*(segment_dt/dt);
            moment_t = analytic > 1e-8 ? analytic_moment/analytic : mid;
        }
        if (budgeted && projvol_hard_beam_shadows == 0 && (fine_detail || medium_detail > 0.0 || step_count < 5.0))
        {
            // These cells are independent of capped shadow steps. Weak smooth
            // media need at most 20; authored fine detail retains Samples.
            // At most fine_count + coarse_steps cells
            // are visited across the complete chord, rather than N per step.
            int fine_count = fine_detail ? godray_res : min(godray_res,20);
            float fine_dt = march_len / max(float(fine_count), 1.0);
            float first = clamp((segment_start-t0)/max(fine_dt,1e-6),0.0,float(fine_count));
            float last = clamp((segment_start+segment_dt-t0)/max(fine_dt,1e-6),first,float(fine_count));
            if (projvol_equiangular != 0)
            {
                float angle_scale = float(fine_count)/max(angles.y-angles.x,1e-12);
                first = clamp((atan(segment_start-b,D)-angles.x)*angle_scale,0.0,float(fine_count));
                last = clamp((atan(segment_start+segment_dt-b,D)-angles.x)*angle_scale,first,float(fine_count));
            }
            vec3 integrated = vec3(0.0);
            float integrated_moment = 0.0;
            for (int j = int(floor(first)); j < int(ceil(last)); ++j)
            {
                float fine_start = t0+float(j)*fine_dt;
                float fine_end = fine_start+fine_dt;
                if (projvol_equiangular != 0)
                {
                    fine_start = b+D*tan(mix(angles.x,angles.y,float(j)/float(fine_count)));
                    fine_end = b+D*tan(mix(angles.x,angles.y,min(float(j+1)/float(fine_count),1.0)));
                    if (j == 0) fine_start = t0;
                    if (j == fine_count-1) fine_end = t1;
                }
                float fine_length = max(fine_end-fine_start,0.0);
                float weight = max(min(segment_start+segment_dt,fine_end)-max(segment_start,fine_start),0.0);
                float ft = fine_start+roffset*fine_length;
                vec4 fp = proj_mat*vec4(d*ft,1.0);
                float analytic = projvolAnalytic(d,ft,fp);
                float fine_moment = ft;
                if (projvol_equiangular != 0)
                {
                    const float gauss = 0.28867513459;
                    float mid = fine_start+0.5*fine_length;
                    float ta = mid-gauss*fine_length, tb = mid+gauss*fine_length;
                    float fa = projvolAnalytic(d,ta,proj_mat*vec4(d*ta,1.0));
                    float fb = projvolAnalytic(d,tb,proj_mat*vec4(d*tb,1.0));
                    analytic = 0.5*(fa+fb);
                    fine_moment = fa+fb > 1e-8 ? (fa*ta+fb*tb)/(fa+fb) : mid;
                }
                vec3 wpos = (projvol_inv_modelview*vec4(d*ft,1.0)).xyz;
                float medium = density;
                if (fine_detail || medium_detail > 0.0)
                    medium = mix(density,projvolMedium(wpos),fine_detail ? 1.0 : medium_detail);
                // Preserve the look's cookie filtering experiment where used.
                vec3 fc = projvol_cookie_footprint != 0 ? cookie :
                    projGoboTexture(-dot(center-d*ft,proj_n),fp.xy/max(fp.w,1e-5));
                vec3 value = analytic*medium*mix(fc*projvol_shadow_tint,fc,vis)*weight;
                integrated += value;
                integrated_moment += dot(value*color,vec3(0.2126,0.7152,0.0722))*fine_moment;
            }
            vec3 refined_sample = integrated / (projvol_equiangular != 0 ? 1.0 : dt);
            float refined_luma = dot(refined_sample*color,vec3(0.2126,0.7152,0.0722));
            float refined_moment = integrated_moment / (projvol_equiangular != 0 ? 1.0 : dt);
            // Low budgets converge smoothly to a fixed medium lattice.
            // Mix premultiplied moments with the same RGB weight.
            float refinement = fine_detail ? 1.0 : max(medium_detail,1.0-smoothstep(4.0,5.0,step_count));
            float old_luma = dot(scatter_sample*color,vec3(0.2126,0.7152,0.0722));
            float moment = mix(old_luma*moment_t,refined_moment,refinement);
            scatter_sample = mix(scatter_sample,refined_sample,refinement);
            float sample_luma = mix(old_luma,refined_luma,refinement);
            moment_t = sample_luma > 1e-8 ? moment/sample_luma : t;
        }
        if (budgeted && projvol_hard_beam_shadows != 0)
        {
            vec3 factor = vec3(1.0);
            if (projvol_cookie_footprint != 0) factor *= cookie;
            vec3 integrated = hard_integrated*factor;
            scatter_sample = integrated/(projvol_equiangular != 0 ? 1.0 : dt);
            float luma = dot(integrated*color,vec3(0.2126,0.7152,0.0722));
            float moment = dot(hard_integrated_moment*factor*color,vec3(0.2126,0.7152,0.0722));
            moment_t = luma > 1e-8 ? moment/luma : t;
        }
        scatter_sample *= quadrature_weight;
        accum += scatter_sample;

        // Reproject the BEAM, not the wall. The RGB composite applies projector
        // color after the march, so include that same color scale before taking
        // luminance. The remaining dt/scatter/godray factors are applied below.
        if (projvol_temporal_beam_depth != 0)
        {
            float sample_luma = dot(scatter_sample * color,
                                    vec3(0.2126, 0.7152, 0.0722));
            depth_w += sample_luma * moment_t;
            w_sum   += sample_luma;
        }

        // [BDMerge G3.3 Batch A - E4] Luminance early-out (no gate - provably
        // invisible). The final composite below is
        //   shaft = accum * dt * PROJVOL_SCATTER * godray_multiplier * color
        // then clamp(shaft, 0, projvol_max). Every remaining sample adds a
        // NON-NEGATIVE term (atten, phase for the documented g in [0,0.95], cookie,
        // edge_feather, density are all >= 0) so `accum` only grows per channel, and
        // the post-loop rim term is likewise additive - so once this EXACT product
        // reaches projvol_max on all three channels the clamped output can no longer
        // change and further stepping is wasted work. The check reuses the identical
        // expression as the composite, so it can never trigger before the true value
        // is clamped (breaking early would then be visible). Break AFTER the sample's
        // contribution is added, matching the composite's factors precisely.
        // Ordinary integration retains the saturation break, including beam
        // depth. Equi-angular beam moments require the complete integral.
        if ((!budgeted || projvol_temporal_beam_depth == 0) &&
            (projvol_equiangular == 0 || projvol_temporal_beam_depth == 0) && all(greaterThanEqual(accum *
            (projvol_equiangular != 0 ? 1.0 : dt) * PROJVOL_SCATTER * godray_multiplier * color, vec3(projvol_max))))
        {
            break;
        }
    }

    // Single-scattering integral: weight by physical step length so a longer
    // chord through the cone scatters more (the local-light look).
    // [BDMerge G3.3 fix] Multiply the projector's Light Color in HERE, once, over
    // the whole accumulated (texture-only) shaft. `color` is the linear light
    // diffuse already lerped toward the art-direction tint in C++, so the shaft
    // base is the true light color and the tint layers on top of it.
    float march_scale = (projvol_equiangular != 0 ? 1.0 : dt) * PROJVOL_SCATTER * godray_multiplier;
    vec3 shaft = accum * march_scale * color;
    vec2 sil_cov = (alr_sil_active & 32) != 0
        ? texture(silCoverMap, tc).rg : vec2(0.0);
    shaft *= 1.0 - sil_cov.r;
    depth_w *= march_scale;
    w_sum   *= march_scale;

    // [BDMerge G3.3 Rim] Surface-coupled rim / wrap glow at the opaque surface that
    // capped the march. Everything here is the projector's REAL light on the REAL
    // surface, so a rim only appears where the object is genuinely lit by THIS cone
    // (inside the frustum, not in its shadow) and turns away from the camera - it is
    // physically the light landing on the surface back-scattering toward the eye at
    // grazing angles. Added to the same additive HDR shaft, so the existing
    // bloom-feed softens it into a halo for free.
    if (projvol_rim_strength > 0.0)
    {
        // Re-evaluate the projector at the exact surface point (the march only
        // sampled the air between t0 and t1). clipProjectedLightVars gives dist /
        // l_dist / proj_tc for pos; a true return or an out-of-cookie coord means
        // the surface is outside this cone -> no rim.
        vec3  lv_s;
        vec4  ptc_s;
        float dist_s, l_dist_s;
        if (!clipProjectedLightVars(C, pos.xyz, dist_s, l_dist_s, lv_s, ptc_s) &&
            ptc_s.x > 0.0 && ptc_s.x < 1.0 && ptc_s.y > 0.0 && ptc_s.y < 1.0)
        {
            // Real view-space G-buffer normal, plus the two directions that define
            // a rim: V = surface->camera (d is camera->surface), L = surface->light.
            vec3  nrm = normalize(getNorm(tc).xyz);
            vec3  V   = -d;
            vec3  L   = normalize(C - pos.xyz);

            float nv = max(dot(nrm, V), 0.0);
            float nl = dot(nrm, L);

            // (1) Grazing edge: a rim lives on the silhouette where the surface
            // turns perpendicular to view. This alone was the OLD term and it is
            // what made every edge glow uniformly.
            float graze = pow(1.0 - nv, max(projvol_rim_power, 0.01));

            // [F4] Skin-softness lever for AVATAR rims. projvol_rim_softness
            // smoothstep-shapes the graze falloff so the edge reads as a soft glow
            // bleeding onto skin instead of a hard, cutout-like outline. The remap
            // graze*graze*(3-2*graze) is a monotone S-curve on [0,1]; mixing toward
            // it eases both ends of the transition. 0 = identity (exact current
            // look), 1 = fully softened. Cheap (a couple of ALU), no branch.
            graze = mix(graze, graze * graze * (3.0 - 2.0 * graze), projvol_rim_softness);

            // (2) THE FIX - directionality. A rim only exists where the light rakes
            // the form, i.e. where the surface faces the projector (N.L). Soft-wrap
            // it so it bleeds a gradient onto the body instead of a hard terminator;
            // projvol_rim_wrap widens how far past the terminator the glow reaches
            // (0 = crisp back-only rim, 1 = broad wrap / interior fill). This term
            // is what kills the "cutout outline": a frontal/camera-position light
            // has N.L ~ 0 at the silhouette (N _|_ V and L ~ V) -> no rim, which is
            // physically how rim lights behave (they must come from behind/beside).
            float wrap = clamp((nl + projvol_rim_wrap) / (1.0 + projvol_rim_wrap), 0.0, 1.0);

            // [Perf] The shadow-map sample + cookie fetch below are the expensive part
            // of the rim. Gate them on the cheap geometry (one normal fetch already
            // done): skip entirely where the rim is negligible - the flat/camera-facing
            // pixels (graze~0) and shadow-side pixels (wrap~0), i.e. the large majority
            // of a cone's coverage. Only true light-facing silhouette pixels pay for
            // the shadow lookup, so whole non-grazing warps early-out.
            float geom = graze * wrap * projvol_rim_strength;
            if (geom > 0.002)
            {
                // Incident projector light at this point: gobo footprint x distance/range
                // attenuation x THIS projector's own shadow map. The light before albedo -
                // so an unlit/shadowed/out-of-cone edge cannot glow.
                float atten_s  = calcLegacyDistanceAttenuation(dist_s, falloff);
                float shadow_s = sampleSpotShadow(pos.xyz, nrm, proj_shadow_idx, tc);
                vec3  cookie_s = projGoboTexture(l_dist_s, ptc_s.xy);
                vec3  E        = cookie_s * atten_s * shadow_s;

                // Brightness gate ("ignore light dimmer than"): keep the rim on
                // meaningfully lit surfaces so faint spill can't paint a grey outline.
                float lum  = dot(E, vec3(0.2126, 0.7152, 0.0722));
                float gate = (projvol_rim_threshold > 0.0)
                           ? smoothstep(projvol_rim_threshold * 0.5, projvol_rim_threshold, lum)
                           : 1.0;

                // Carry the light's chroma (E's cookie + `color`); the scalar terms drive
                // brightness only, so it reads as colored light instead of clipping to a
                // white sticker edge. Added into the additive HDR shaft -> rides bloom-feed.
                vec3 rim = E * (geom * gate * PROJVOL_RIM_SCALE) * color;
                rim *= 1.0 - sil_cov.g;
                shaft += rim;
                if (projvol_temporal_beam_depth != 0)
                {
                    // The rim shares the shaft RGB target but lives at the opaque
                    // surface, so include its first moment at t_surface. Rim-only
                    // pixels then fall back naturally to surface reprojection.
                    float rim_luma = dot(rim, vec3(0.2126, 0.7152, 0.0722));
                    depth_w += rim_luma * t_surface;
                    w_sum   += rim_luma;
                }
            }
        }
    }

    // [Phase 1 item 1] HDR-space composite: this pass now runs BEFORE colorCorrect
    // on the linear HDR scene buffer, so the active tonemapper (AMD LPM / ACES)
    // rolls off the bright cores filmically. projvol_max is therefore a generous
    // linear-HDR headroom clamp (guarding NaN/inf and lone fireflies) rather than
    // the tight display-space clamp the old post-tonemap placement needed.
    shaft = clamp(shaft, vec3(0.0), vec3(projvol_max));

    // Keep the stored first moment consistent with the SAME clamped shaft
    // luminance the temporal pass divides by. The mean distance itself is
    // clamp-independent; empty shafts reproject at the opaque surface.
    float mean_dist = (w_sum > 1e-5) ? (depth_w / w_sum) : t_surface;
    float beam_moment = dot(shaft, vec3(0.2126, 0.7152, 0.0722)) * mean_dist;
    beam_moment = min(beam_moment, 60000.0); // defensive RGBA16F firefly bound

    // Output ONLY the shaft delta - additive GL_ONE,GL_ONE onto the scene
    // buffer, so the pass never samples what it writes (no feedback).
    // Alpha carries luminance(clamped shaft) * mean_distance.
    // Additive blending therefore sums overlap-safe first moments; the temporal
    // pass divides by luminance(sum(rgb)). Direct/non-temporal paths keep alpha 0.
    frag_color = vec4(shaft, (projvol_temporal_beam_depth != 0) ? beam_moment : 0.0);
}
