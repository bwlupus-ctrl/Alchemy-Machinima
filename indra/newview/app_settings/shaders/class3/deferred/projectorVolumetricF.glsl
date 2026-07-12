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

    float march_len = t1 - t0;
    float dt        = march_len / float(godray_res); // physical step length

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
        if (projvol_shadow_samples <= 1)
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
            // Real G-buffer normal (view space) -> genuine Fresnel, not a depth-slope
            // guess. d is camera->surface, so -d is the view direction (surface->eye).
            vec3  nrm   = normalize(getNorm(tc).xyz);
            float nv    = max(dot(nrm, -d), 0.0);
            float graze = pow(1.0 - nv, max(projvol_rim_power, 0.01));

            // Incident projector light actually reaching this surface point: the
            // gobo footprint, distance/range attenuation, and THIS projector's own
            // shadow map (real normal used for the bias). Matches how spotLightF
            // dims the lit surface - this is the light, before the surface's albedo.
            float atten_s  = calcLegacyDistanceAttenuation(dist_s, falloff);
            float shadow_s = sampleSpotShadow(pos.xyz, nrm, proj_shadow_idx, tc);
            vec3  cookie_s = projGoboTexture(l_dist_s, ptc_s.xy);
            vec3  E        = cookie_s * atten_s * shadow_s;

            // Soft brightness gate (RimGlow's "ignore light dimmer than"): only
            // meaningfully lit surfaces glow, so dim ambient spill can't grey the rim.
            float lum  = dot(E, vec3(0.2126, 0.7152, 0.0722));
            float gate = (projvol_rim_threshold > 0.0)
                       ? smoothstep(projvol_rim_threshold * 0.5, projvol_rim_threshold, lum)
                       : 1.0;

            // Rim carries the light's own color; kept independent of godray_multiplier
            // so beam brightness and rim brightness tune separately.
            shaft += E * graze * gate * projvol_rim_strength * color;
        }
    }

    // [Phase 1 item 1] HDR-space composite: this pass now runs BEFORE colorCorrect
    // on the linear HDR scene buffer, so the active tonemapper (AMD LPM / ACES)
    // rolls off the bright cores filmically. projvol_max is therefore a generous
    // linear-HDR headroom clamp (guarding NaN/inf and lone fireflies) rather than
    // the tight display-space clamp the old post-tonemap placement needed.
    shaft = clamp(shaft, vec3(0.0), vec3(projvol_max));

    // Output ONLY the shaft delta - additive GL_ONE,GL_ONE onto the scene
    // buffer, so the pass never samples what it writes (no feedback).
    frag_color = vec4(shaft, 0.0);
}
