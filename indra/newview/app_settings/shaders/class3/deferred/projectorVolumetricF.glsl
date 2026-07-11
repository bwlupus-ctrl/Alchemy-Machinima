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

// Shared godray controls (this program uploads its OWN values into these).
uniform int   godray_res;         // raymarch sample count (bounded local march)
uniform float godray_multiplier;  // shaft brightness multiplier
uniform float projvol_g;          // Henyey-Greenstein anisotropy (forward scatter)

const float M_PI = 3.14159265;

// Internal scattering coefficient. Folds the per-metre scattering strength into
// a single constant so godray_multiplier reads ~1.0 in typical set-light scale;
// the physical step length (below) still makes longer paths through the cone
// scatter more. Tuned conservatively - in-world brightness tuning is owed.
const float PROJVOL_SCATTER = 0.35;

// HDR clamp (R2): shafts are added after colorCorrect (display stage) and N
// cones accumulate additively, so clamp each cone's contribution to keep the
// post-tonemap buffer from blowing out.
const float PROJVOL_MAX = 4.0;

float rand(vec2 co)
{
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

// deferredUtil.glsl
vec4 getPosition(vec2 pos_screen);
bool clipProjectedLightVars(vec3 center, vec3 pos, out float dist, out float l_dist, out vec3 lv, out vec4 proj_tc);
vec3 getProjectedLightDiffuseColor(float light_distance, vec2 projected_uv);
float calcLegacyDistanceAttenuation(float distance, float falloff);
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
    float roffset   = rand(tc);           // per-pixel jitter breaks up banding (R5)

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

        // Volumetric self-shadowing: sample THIS projector's own shadow map.
        // No surface normal for an airborne sample, so pass 0 (the norm*offset
        // bias term is negligible in the shaft - accepted approximation).
        float vis = sampleSpotShadow(spos, vec3(0.0), proj_shadow_idx, tc);

        // Distance + range attenuation, identical to how spotLightF dims the
        // lit surface (inverse-square-ish + range falloff via calcLegacy...).
        float atten = calcLegacyDistanceAttenuation(dist, falloff);

        // Gobo/cone-edge shaped, light-coloured in-scatter (color * cookie).
        vec3 cookie = getProjectedLightDiffuseColor(l_dist, proj_tc.xy);

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

        accum += vis * atten * phase * cookie;
    }

    // Single-scattering integral: weight by physical step length so a longer
    // chord through the cone scatters more (the local-light look). Clamp for
    // HDR safety across N additive cones.
    vec3 shaft = accum * dt * PROJVOL_SCATTER * godray_multiplier;
    shaft = clamp(shaft, vec3(0.0), vec3(PROJVOL_MAX));

    // Output ONLY the shaft delta - additive GL_ONE,GL_ONE onto the scene
    // buffer, so the pass never samples what it writes (no feedback).
    frag_color = vec4(shaft, 0.0);
}
