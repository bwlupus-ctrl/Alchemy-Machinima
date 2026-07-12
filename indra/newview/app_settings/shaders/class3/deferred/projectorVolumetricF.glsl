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
 * the NSpot per-slot shadow dispatch (sampleSpotShadow, shadowMap4-9).
 */

#extension GL_ARB_texture_rectangle : enable
#extension GL_ARB_shader_texture_lod : enable

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;

in vec2 vary_fragcoord;

// Local-light params (uploaded per slot by setupSpotLightVolumetric + the
// per-cone color/size/falloff/shadow-index uploads in renderProjectorVolumetric).
// proj_mat/proj_n/proj_focus/proj_lod/proj_range/size/color/projectionMap are
// declared (and consumed) by deferredUtil.glsl - do not redeclare them here.
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

// Shared godray controls (this program uploads its OWN values into these).
uniform int   godray_res;         // raymarch sample count (bounded local march;
                                  // Phase 1 item 5: adaptively scaled per cone in C++)
uniform float godray_multiplier;  // shaft brightness multiplier
uniform float projvol_g;          // Henyey-Greenstein anisotropy (forward scatter)

// [Phase 1] per-lever sub-controls (uploaded by renderProjectorVolumetric).
uniform float projvol_feather;    // item 6: angular cone-edge softness (0 = hard)
uniform int   projvol_shadow_samples; // item 7: occlusion sub-taps per march step
uniform int   projvol_dither;     // item 2: 0=off(centre) 1=static bluenoise 2=animated
uniform float projvol_frame;      // item 2: temporal seed (frame counter, wrapped)
uniform float projvol_max;        // item 1: HDR headroom clamp (large in linear HDR)

// [BDMerge G3.3 Batch A] look-neutral performance gates (all default to a no-op).
uniform int   projvol_frustum_clip;       // E1: 0 = sphere bounds (default); !=0 = frustum-clipped [t0,t1]
uniform vec4  projvol_frustum_planes[6];  // E1: view-space frustum planes, inside>=0: 0=L 1=R 2=B 3=T 4=near 5=far
uniform int   projvol_shadow_jitter_tap;  // E2: 0 = sub-tap loop (default); !=0 = one IGN-jittered shadow tap

// [BDMerge G3.3 Batch B - R1] Beam-depth reprojection gate. 0 (default) = alpha of
// the output stays 0, exactly as before. !=0 = write the scatter-weighted mean
// sample distance (Sum(w*t)/Sum(w)) into alpha so the temporal pass can reproject
// the airborne shaft instead of the opaque surface behind it. C++ only ever uploads
// this as 1 on the temporal (half-res) path, where the march writes to mProjVolHalf;
// on the direct/non-half-res path it stays 0 so the ADDITIVE scene composite (whose
// alpha must stay 0) is never corrupted.
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
uniform float projvol_noise_speed;       // item 1: noise scroll speed (m/s)
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
bool clipProjectedLightVars(vec3 center, vec3 pos, out float dist, out float l_dist, out vec3 lv, out vec4 proj_tc);
vec3 getProjectedLightDiffuseColor(float light_distance, vec2 projected_uv);
vec4 getTexture2DLodDiffuse(vec2 tc, float lod); // gobo sample WITHOUT the light color
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

void main()
{
    vec2 tc = vary_fragcoord.xy;

    // View-space surface position at this pixel (opaque depth defines the far
    // limit of the march - never scatter behind solid geometry).
    vec4  pos       = getPosition(tc);
    float t_surface = length(pos.xyz);
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
    // samples, so clipping far would remove lit space. A degenerate/empty refined
    // interval falls back to the sphere bounds (never a worse result than today).
    // At the gate default (off) this whole block is skipped -> byte-identical.
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
    }

    float march_len = t1 - t0;
    // max() guards godray_res == 0: the rim-only mode for froxel-injected projectors
    // (beam lives in the grid; only the surface rim + bloom-feed halo render here).
    // Without it dt = inf and 0*inf = NaN would poison the final shaft.
    float dt        = march_len / max(float(godray_res), 1.0); // physical step length

    // [Phase 1 item 2] blue-noise (interleaved gradient) march-start offset.
    // Static per pixel => no crawl under camera motion; optional frame rotation.
    float roffset;
    if (projvol_dither == 0)
    {
        roffset = 0.5; // centred samples, no dither
    }
    else
    {
        float ign = interleavedGradientNoise(gl_FragCoord.xy);
        if (projvol_dither == 2)
        {
            ign = fract(ign + projvol_frame * 0.61803399); // golden-ratio temporal walk
        }
        roffset = ign;
    }

    vec3 accum = vec3(0.0);

    // [BDMerge G3.3 Batch B - R1] Scatter-weighted mean sample distance. Only
    // accumulated when the beam-depth gate is on; otherwise these stay 0 and cost
    // nothing beyond the loop's existing work (byte-identical output path).
    float depth_w = 0.0;
    float w_sum   = 0.0;

    for (int i = 0; i < godray_res; ++i)
    {
        float t    = t0 + (float(i) + roffset) * dt;
        vec3  spos = d * t;               // view-space sample point

        // Frustum clip + projector coords, reusing spotLightF's exact math.
        // clipProjectedLightVars gives dist (|C-spos|/size), l_dist (depth along
        // the projector axis) and proj_tc in one call. Returns true = discard.
        vec3  lv;
        vec4  proj_tc;
        float dist, l_dist;
        if (clipProjectedLightVars(C, spos, dist, l_dist, lv, proj_tc))
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

        // Volumetric self-shadowing: sample THIS projector's own shadow map.
        // No surface normal for an airborne sample, so pass 0 (the norm*offset
        // bias term is negligible in the shaft - accepted approximation).
        // [Phase 1 item 7] Crisp occluder shadows: take projvol_shadow_samples
        // sub-taps spread across the step so thin occluders (bars/foliage) resolve
        // into sharp god-ray bands instead of being blurred by the coarse march.
        float vis;
        // [BDMerge G3.3 Batch A - E2] One IGN-jittered shadow tap instead of N
        // sub-taps. spos = d*(t0+(i+roffset)*dt) already carries the per-pixel
        // interleaved-gradient offset ALONG the step, so a single tap here is
        // decorrelated exactly like the sub-taps and the dither/temporal path
        // averages it - at up to 4x fewer shadow fetches. When the jitter gate is
        // on it overrides ShadowSamples>1. Gate off (default) => the condition is
        // exactly `projvol_shadow_samples <= 1` and both legacy paths are unchanged.
        if (projvol_shadow_jitter_tap != 0 || projvol_shadow_samples <= 1)
        {
            vis = sampleSpotShadow(spos, vec3(0.0), proj_shadow_idx, tc);
        }
        else
        {
            vis = 0.0;
            for (int s = 0; s < projvol_shadow_samples; ++s)
            {
                float ts   = t + (float(s) - float(projvol_shadow_samples - 1) * 0.5) * (dt / float(projvol_shadow_samples));
                vec3  sp   = d * ts;
                vis       += sampleSpotShadow(sp, vec3(0.0), proj_shadow_idx, tc);
            }
            vis /= float(projvol_shadow_samples);
        }

        // Distance + range attenuation, identical to how spotLightF dims the
        // lit surface (inverse-square-ish + range falloff via calcLegacy...).
        float atten = calcLegacyDistanceAttenuation(dist, falloff);

        // Gobo/cone-edge shaped in-scatter. TEXTURE ONLY here - the light color is
        // applied once to the whole shaft below so changing the projector's Light
        // Color visibly re-tints the beam (fix: was baked into the cookie sample).
        vec3 cookie = projGoboTexture(l_dist, proj_tc.xy);

        // [Phase 3] Participating-medium density modulation for this sample. The
        // global haziness master, the animated noise medium and the height fog all
        // fold into one scalar that scales the in-scatter. Defaults keep this at
        // exactly projvol_density (1.0) -> a flat, uniform cone (the shipped look).
        float density = projvol_density;
        if (projvol_noise_strength > 0.0 || projvol_fog_strength > 0.0)
        {
            // Agent-space (world, Z-up) position of this airborne sample.
            vec3 wpos = (projvol_inv_modelview * vec4(spos, 1.0)).xyz;

            // [item 1] Animated 3D noise -> drifting dust motes / gentle turbulence.
            // World-anchored + slowly scrolled so motes sit in the air and drift,
            // never crawling with the camera. Centred on 1.0 so it varies density
            // symmetrically instead of only dimming it.
            if (projvol_noise_strength > 0.0)
            {
                vec3  np    = wpos * projvol_noise_scale + vec3(projvol_time * projvol_noise_speed);
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
        }

        // Per-sample Henyey-Greenstein phase. Because the light is LOCAL the
        // scatter geometry varies per step: Ldir is the light's travel
        // direction at this sample, Vdir points back to the camera. Forward
        // scatter (g>0) glows brightly when the camera looks toward the light.
        vec3  Ldir  = normalize(spos - C);
        vec3  Vdir  = -d;
        float cosT  = dot(Ldir, Vdir);
        float g     = projvol_g;
        float denom = 1.0 + g * g - 2.0 * g * cosT;
        float phase = (1.0 - g * g) / (4.0 * M_PI * pow(max(denom, 1e-4), 1.5));

        // [Batch 1 B] Gobo-colored occluder shadows. The lit term is the full gobo
        // in-scatter (cookie); the occluded term is that same gobo dimmed by
        // projvol_shadow_tint, so shadow bands stay colored/shaped by the cookie
        // instead of going pure black. mix by visibility: vis=1 -> lit, vis=0 ->
        // occluded. At projvol_shadow_tint == 0 this reduces exactly to vis*cookie
        // (the classic crisp black occluder shadow).
        vec3 scatter = mix(cookie * projvol_shadow_tint, cookie, vis);
        accum += atten * phase * scatter * edge_feather * density;

        // [BDMerge G3.3 Batch B - R1] Reproject the BEAM, not the wall. Accumulate a
        // scatter-weighted mean sample distance: weight is this sample's scalar
        // in-scatter contribution (atten*phase*edge_feather*density - the luminance-
        // ish factor already multiplying the vec3 cookie above, a consistent scalar
        // proxy for how much this sample brightens the shaft). t is the distance
        // along the normalized ray (spos = d*t, so |spos| == t), matching how the
        // temporal pass measures length(vpos). Gated: default path does no extra
        // work and the loop result is unchanged. Placed before the E4 early-out so a
        // clamped-out tail still contributes to the mean over the samples taken.
        if (projvol_temporal_beam_depth != 0)
        {
            float w_beam = atten * phase * edge_feather * density;
            depth_w += w_beam * t;
            w_sum   += w_beam;
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
        if (all(greaterThanEqual(accum * dt * PROJVOL_SCATTER * godray_multiplier * color, vec3(projvol_max))))
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
    vec3 shaft = accum * dt * PROJVOL_SCATTER * godray_multiplier * color;

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

                // Brightness gate (RimGlow's "ignore light dimmer than"): keep the rim on
                // meaningfully lit surfaces so faint spill can't paint a grey outline.
                float lum  = dot(E, vec3(0.2126, 0.7152, 0.0722));
                float gate = (projvol_rim_threshold > 0.0)
                           ? smoothstep(projvol_rim_threshold * 0.5, projvol_rim_threshold, lum)
                           : 1.0;

                // Carry the light's chroma (E's cookie + `color`); the scalar terms drive
                // brightness only, so it reads as colored light instead of clipping to a
                // white sticker edge. Added into the additive HDR shaft -> rides bloom-feed.
                shaft += E * (geom * gate * PROJVOL_RIM_SCALE) * color;
            }
        }
    }

    // [Phase 1 item 1] HDR-space composite: this pass now runs BEFORE colorCorrect
    // on the linear HDR scene buffer, so the active tonemapper (AMD LPM / ACES)
    // rolls off the bright cores filmically. projvol_max is therefore a generous
    // linear-HDR headroom clamp (guarding NaN/inf and lone fireflies) rather than
    // the tight display-space clamp the old post-tonemap placement needed.
    shaft = clamp(shaft, vec3(0.0), vec3(projvol_max));

    // [BDMerge G3.3 Batch B - R1] Scatter-weighted mean beam distance for temporal
    // reprojection. Fall back to the opaque surface distance when the shaft is empty
    // (no in-scatter accumulated) so a beam-free pixel reprojects like the wall.
    float beam_dist = (w_sum > 1e-5) ? (depth_w / w_sum) : t_surface;

    // Output ONLY the shaft delta - additive GL_ONE,GL_ONE onto the scene
    // buffer, so the pass never samples what it writes (no feedback).
    // [BDMerge G3.3 Batch B - R1] Alpha carries the mean beam distance ONLY when the
    // gate is on (temporal/half-res path -> mProjVolHalf). On the direct/non-half-res
    // path C++ leaves this uniform at 0, so alpha stays 0 and the ADDITIVE scene
    // composite is never corrupted (byte-identical to today at default settings).
    frag_color = vec4(shaft, (projvol_temporal_beam_depth != 0) ? beam_dist : 0.0);
}
