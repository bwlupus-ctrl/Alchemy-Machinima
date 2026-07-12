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
uniform float froxel_noise_speed;         // noise scroll speed (m/s)
uniform float froxel_time;                // continuous seconds (noise scroll)

// froxelUtil.glsl
float froxelSliceToViewZ(float slice, vec2 nf, float gz);
vec3  froxelViewPos(vec2 fxy, float viewz, vec3 grid, vec2 thf);
float froxelFbm(vec3 p);

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

    // Animated value-noise fbm -> drifting dust motes. World-anchored + slowly
    // scrolled, centred on 1.0 so it varies density symmetrically. (Same form as
    // the per-cone projvol noise.)
    if (froxel_noise_strength > 0.0)
    {
        vec3  np   = wpos * froxel_noise_scale + vec3(froxel_time * froxel_noise_speed);
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

    frag_color = vec4(sigma_s, sigma_t);
}
