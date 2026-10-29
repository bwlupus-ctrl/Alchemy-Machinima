/**
 * @file class1/deferred/froxelMediaF.glsl
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
 * [BDMerge Froxel F0] P1 media pass. Fullscreen-triangle pass over the RGBA16F
 * Z-slice ATLAS: every atlas fragment is one froxel. Decode the atlas pixel into
 * (fx,fy,slice), reconstruct the froxel-centre view-space position, transform it to
 * agent(world, Z-up) space, and evaluate the participating medium there:
 *   base density -> * animated 3D value-noise fbm -> * height fog.
 * Output rgb = scattering coefficient sigma_s (= sigma_t * albedo, albedo hardcoded
 * 1.0 this batch), a = extinction sigma_t. Dead atlas tiles (slice >= GridZ) write 0.
 * NO lighting, NO integration this batch - just the medium.
 */

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;

in vec2 vary_fragcoord;

// Grid + atlas layout.
uniform vec3  froxel_grid;         // (GridX, GridY, GridZ)
uniform vec4  froxel_atlas;        // (tilesX, tilesY, atlasW, atlasH)
uniform vec2  froxel_near_far;     // (camera near, BDMergeFroxelFar) metres
uniform vec2  froxel_tan_half_fov; // (tan(fovx/2), tan(fovy/2)) for view-pos reconstruct
uniform mat4  froxel_inv_modelview;// view -> agent (world, Z-up)

// Media levers (froxel-side; mirror the per-cone convention exactly).
uniform float froxel_density;             // base sigma_t per metre (BDMergeFroxelDensity)
uniform float froxel_fog_strength;        // height-fog blend (0 = off)
uniform float froxel_fog_ground_density;  // density at/below the ground reference
uniform float froxel_fog_falloff;         // e-fold altitude falloff (metres)
uniform float froxel_fog_base;            // ground reference altitude (agent Z)
uniform float froxel_noise_strength;      // animated noise amount (0 = off)
uniform float froxel_noise_scale;         // noise spatial scale (cycles/m)
uniform float froxel_noise_speed;         // master noise scroll speed (m/s); folded into froxel_wind CPU-side
uniform vec3  froxel_wind;                // [F5] scroll velocity in agent axes = wind_dir * noise_speed (m/s)
uniform float froxel_time;                // continuous seconds (noise scroll)

const int LOCALFOG_MAX_VOLUMES = 8;
uniform vec3 localfog_center[LOCALFOG_MAX_VOLUMES];
uniform vec3 localfog_extents[LOCALFOG_MAX_VOLUMES];
uniform mat3 localfog_inv_rot[LOCALFOG_MAX_VOLUMES];
uniform vec4 localfog_params[LOCALFOG_MAX_VOLUMES]; // density, feather, shape, height falloff
uniform vec3 localfog_tint[LOCALFOG_MAX_VOLUMES];
uniform vec4 localfog_noise[LOCALFOG_MAX_VOLUMES];  // scale, speed, reserved, reserved
uniform int localfog_count;

// froxelUtil.glsl
float froxelSliceToViewZ(float slice, vec2 nf, float gz);
vec3  froxelViewPos(vec2 fxy, float viewz, vec3 grid, vec2 thf);
float froxelFbm(vec3 p);

float localFogDensity(vec3 wpos, out vec3 tint_accum, out float tint_w)
{
    float density_accum = 0.0;
    tint_accum = vec3(0.0);
    tint_w = 0.0;
    for (int i = 0; i < LOCALFOG_MAX_VOLUMES; ++i)
    {
        if (i >= localfog_count)
        {
            break;
        }

        vec3 extents = max(localfog_extents[i], vec3(0.001));
        // Conservative world AABB reject comes before the OBB/ellipsoid
        // transform, SDF and noise. transpose(inv_rot) is local-to-world.
        mat3 world_rotation = transpose(localfog_inv_rot[i]);
        vec3 world_extent = abs(world_rotation[0]) * extents.x +
                            abs(world_rotation[1]) * extents.y +
                            abs(world_rotation[2]) * extents.z;
        vec3 world_delta = abs(wpos - localfog_center[i]);
        if (any(greaterThan(world_delta, world_extent)))
        {
            continue;
        }

        vec3 p = localfog_inv_rot[i] * (wpos - localfog_center[i]);
        float feather = clamp(localfog_params[i].y, 0.0, 1.0);
        float mask;
        if (localfog_params[i].z < 0.5)
        {
            vec3 d = abs(p) - extents;
            float dist = max(d.x, max(d.y, d.z));
            float edge = max(feather * min(extents.x, min(extents.y, extents.z)), 0.0001);
            mask = 1.0 - smoothstep(-edge, 0.0, dist);
        }
        else
        {
            vec3 ep = p / extents;
            float q = dot(ep, ep);
            mask = 1.0 - smoothstep(1.0 - max(feather, 0.0001), 1.0, q);
        }

        if (localfog_params[i].w > 0.0)
        {
            float height01 = clamp((p.z + extents.z) / (2.0 * extents.z), 0.0, 1.0);
            mask *= exp(-height01 * localfog_params[i].w);
        }

        float turbulence = 1.0;
        if (localfog_noise[i].x > 0.0)
        {
            vec3 noise_pos = wpos * localfog_noise[i].x +
                             vec3(localfog_noise[i].y * froxel_time);
            turbulence = mix(0.65, 1.35, froxelFbm(noise_pos));
        }
        float density = mask * max(localfog_params[i].x, 0.0) * turbulence;
        density_accum += density;
        tint_accum += density * max(localfog_tint[i], vec3(0.0));
        tint_w += density;
    }
    return density_accum;
}

void main()
{
    int gx = int(froxel_grid.x);
    int gy = int(froxel_grid.y);
    int gz = int(froxel_grid.z);

    // Decode the atlas pixel -> tile (tx,ty) -> froxel (fx,fy,fz). gl_FragCoord is
    // pixel-centred (x+0.5); integer truncation recovers the froxel index.
    ivec2 apix = ivec2(gl_FragCoord.xy);
    int   tx   = apix.x / gx;
    int   ty   = apix.y / gy;
    int   fx   = apix.x - tx * gx;
    int   fy   = apix.y - ty * gy;
    int   fz   = ty * int(froxel_atlas.x) + tx; // row-major tile -> slice index

    // Dead tiles past the last real slice: the square atlas can have tilesX*tilesY
    // > GridZ. Write clear so trilinear taps that stray here read nothing.
    if (fz >= gz)
    {
        frag_color = vec4(0.0);
        return;
    }

    // Froxel-centre view depth (exponential distribution) and view-space position.
    float viewz = froxelSliceToViewZ(float(fz) + 0.5, froxel_near_far, froxel_grid.z);
    vec3  vpos  = froxelViewPos(vec2(float(fx) + 0.5, float(fy) + 0.5), viewz,
                                froxel_grid, froxel_tan_half_fov);

    // Agent-space (world, Z-up) position for the world-anchored medium.
    vec3 wpos = (froxel_inv_modelview * vec4(vpos, 1.0)).xyz;

    // Base extinction.
    float sigma_t = max(froxel_density, 0.0);

    // Animated value-noise fbm -> drifting dust motes. World-anchored and scrolled
    // along froxel_wind (a directional wind velocity in agent axes = wind_dir *
    // noise_speed, computed CPU-side), centred on 1.0 so it varies density
    // symmetrically. [F5] The old drift was the fixed (1,1,1) diagonal; the wind
    // vector now steers the direction (X/Y region-horizontal, Z vertical, negatives
    // reverse) so fog can be made to blow across the scene naturally.
    if (froxel_noise_strength > 0.0)
    {
        vec3  np   = wpos * froxel_noise_scale + froxel_wind * froxel_time;
        float n    = froxelFbm(np);                       // ~[0,1], mean ~0.5
        float mote = 1.0 + froxel_noise_strength * (n * 2.0 - 1.0);
        sigma_t   *= max(mote, 0.0);
    }

    // Height fog: denser near the ground reference, thinning with altitude, blended
    // in by fog_strength. (Same form as the per-cone projvol height fog.)
    if (froxel_fog_strength > 0.0)
    {
        float h  = wpos.z - froxel_fog_base;               // metres above ground ref
        float hf = froxel_fog_ground_density * exp(-max(h, 0.0) / max(froxel_fog_falloff, 0.01));
        sigma_t *= mix(1.0, hf, froxel_fog_strength);
    }

    // Scattering coefficient sigma_s = sigma_t * albedo (albedo = 1.0 this batch).
    const float albedo = 1.0;
    vec3 sigma_s = vec3(sigma_t * albedo);
    if (localfog_count > 0)
    {
        vec3 local_tint;
        float local_tint_w;
        float local_density = localFogDensity(wpos, local_tint, local_tint_w);
        sigma_t += local_density;
        if (local_tint_w > 0.0)
        {
            sigma_s += local_tint * albedo;
        }
    }

    frag_color = vec4(sigma_s, sigma_t);
}
