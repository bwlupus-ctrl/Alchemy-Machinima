/**
 * @file class1/deferred/froxelInjectF.glsl
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
 * [BDMerge Froxel F2] P2 per-light injection pass. ONE additive fullscreen-triangle
 * pass over the RGBA16F light ATLAS PER injecting projector (C++ draws it N times,
 * blend GL_ONE,GL_ONE), each pass binding ONE projector's cookie + shadow slot via
 * setupSpotLightVolumetric - the SAME upload machinery the per-cone shafts use. This
 * is the per-froxel version of one march sample in projectorVolumetricF.glsl:
 *   decode atlas pixel -> froxel CENTRE view pos -> evaluate this projector there
 *   (frustum/cookie clip, gobo, distance atten, spot-shadow, HG phase) -> multiply
 *   by the shared MEDIA scattering coefficient sigma_s at this froxel -> accumulate.
 * Output rgb = the in-scatter SOURCE radiance S contributed by this light at this
 * froxel (a = 0, unused). The froxel integrate pass turns Sum(S) into radiance with
 * Hillaire's slice integral, so the injection carries NO path-length term (that is
 * the integrate pass's slice thickness) - it is purely the per-froxel source.
 *
 * sigma_s COUPLING (deliberate, physical): a projector beam is only VISIBLE because
 * it scatters off the medium, so the injected source is proportional to the shared
 * atmosphere's sigma_s (media atlas rgb). In dry air (low BDMergeFroxelDensity) the
 * beams are faint by design; raise Density / Fog / Noise to populate the air and the
 * beams read - the "unified atmosphere" the hybrid grid exists to provide.
 */

#extension GL_ARB_texture_rectangle : enable
#extension GL_ARB_shader_texture_lod : enable

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;

in vec2 vary_fragcoord;

// Grid + atlas layout (froxel-centre reconstruction) + the shared media atlas.
uniform sampler2D froxelMedia;      // RGBA16F media atlas (rgb = sigma_s, a = sigma_t)
uniform vec3  froxel_grid;          // (GridX, GridY, GridZ)
uniform vec4  froxel_atlas;         // (tilesX, tilesY, atlasW, atlasH)
uniform vec2  froxel_near_far;      // (camera near, BDMergeFroxelFar) metres
uniform vec2  froxel_tan_half_fov;  // (tan(fovx/2), tan(fovy/2)) for view-pos reconstruct

// Per-light params, uploaded per injection pass by renderFroxelVolumetrics EXACTLY
// like the per-cone loop uploads them (center/size/falloff/color/g/shadow idx +
// setupSpotLightVolumetric's proj_mat/proj_n/cookie/shadow-slot). proj_mat/proj_n/
// projectionMap live in deferredUtil.glsl - do not redeclare them here.
uniform vec3  center;               // LIGHT_CENTER, view space (light sphere centre)
uniform float size;                 // LIGHT_SIZE (sphere-of-influence radius); also
uniform float projvol_max_distance; // metres along projector axis; 0 = full range
                                    // read by clipProjectedLightVars in deferredUtil
uniform float falloff;              // LIGHT_FALLOFF
uniform vec3  color;                // DIFFUSE_COLOR - light's linear diffuse (+tint lerp)
uniform int   proj_shadow_idx;      // this projector's shadow slot (0..N-1)
uniform float godray_multiplier;    // per-projector brightness (global or override multiplier)
uniform float projvol_g;            // Henyey-Greenstein anisotropy (forward scatter)
uniform float projvol_feather;      // angular cone-edge softness (0 = hard)

// deferredUtil cookie-LOD params (declared there too - read directly here, same
// pattern as projectorVolumetricF.glsl; the linker merges the identical uniform).
uniform float proj_focus;
uniform float proj_lod;
uniform float proj_range;

// [BDMerge Froxel F3] Per-frame injection jitter. froxel_jitter gates it (0 = the F2
// deterministic froxel-centre sample, byte-identical); froxel_frame is the wrapped
// frame counter that walks the pattern. C++ turns the gate ON only when the temporal
// resolve is active, so the noise it introduces always has a resolve pass to average.
uniform int   froxel_jitter;
uniform float froxel_frame;

const float M_PI = 3.14159265;

// [BDMerge Froxel F3] Interleaved gradient noise (Jimenez) - the SAME blue-noise-like
// dither the per-cone march uses (class3/deferred/projectorVolumetricF.glsl). Static
// per screen-pixel so it does not crawl; the golden-ratio frame walk below rotates it.
float interleavedGradientNoise(vec2 p)
{
    return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));
}

// deferredUtil.glsl
bool  clipProjectedLightVars(vec3 center, vec3 pos, out float dist, out float l_dist, out vec3 lv, out vec4 proj_tc);
vec4  getTexture2DLodDiffuse(vec2 tc, float lod); // gobo sample WITHOUT the light color
float calcLegacyDistanceAttenuation(float distance, float falloff);

// shadowUtil.glsl (SPOT_SHADOW permutation pulls in the indexed projector dispatch).
// NOTE the 4th arg (pos_screen) is UNUSED by the current sampleSpotShadow - it jitters
// internally with the sample's own view-space xy (shadowUtil.glsl passes spos.xy to
// pcfSpotShadow), so any stable 2D value works; we pass the atlas uv for clarity.
float sampleSpotShadow(vec3 pos, vec3 norm, int index, vec2 pos_screen);

// froxelUtil.glsl
float froxelSliceToViewZ(float slice, vec2 nf, float gz);
vec3  froxelViewPos(vec2 fxy, float viewz, vec3 grid, vec2 thf);

// Texture/gobo footprint ONLY (no light color) - mirrors projectorVolumetricF's
// projGoboTexture so the light color multiplies the whole beam once, explicitly.
vec3 projGoboTexture(float light_distance, vec2 projected_uv)
{
    float diff  = clamp((light_distance - proj_focus) / proj_range, 0.0, 1.0);
    float lod   = diff * proj_lod;
    vec4  plcol = getTexture2DLodDiffuse(projected_uv.xy, lod);
    return plcol.rgb * plcol.a;
}

void main()
{
    int gx = int(froxel_grid.x);
    int gy = int(froxel_grid.y);
    int gz = int(froxel_grid.z);

    // Decode the atlas pixel -> tile (tx,ty) -> froxel (fx,fy,fz). Same convention
    // as the media / integrate passes (gl_FragCoord pixel-centred; truncate).
    ivec2 apix = ivec2(gl_FragCoord.xy);
    int   tx   = apix.x / gx;
    int   ty   = apix.y / gy;
    int   fx   = apix.x - tx * gx;
    int   fy   = apix.y - ty * gy;
    int   fz   = ty * int(froxel_atlas.x) + tx; // row-major tile -> slice index

    // Dead tiles past the last real slice: no medium, contribute nothing.
    if (fz >= gz)
    {
        frag_color = vec4(0.0);
        return;
    }

    // [BDMerge Froxel F3] Per-frame jitter of the evaluated sample point WITHIN this
    // froxel's extent (before the light evaluation) so the temporal resolve pass has
    // decorrelated samples to average into a smooth beam. Construction (documented):
    //   - Three DECORRELATED interleaved-gradient-noise values, one per axis, from the
    //     same IGN at three distinct spatial scrambles of gl_FragCoord.
    //   - Each is advanced by a golden-ratio frame walk (froxel_frame * 0.61803399, the
    //     low-discrepancy step the per-cone dither==2 branch uses) so successive frames
    //     visit fresh offsets rather than repeating.
    //   - Recentred to [-0.5, 0.5] => a 3D offset in FROXEL units (x,y in the tile
    //     plane, z along the slice axis), covering the whole froxel volume over frames.
    // Gated: froxel_jitter == 0 => zero offset => the exact F2 froxel-centre sample.
    vec3 jit = vec3(0.0);
    if (froxel_jitter != 0)
    {
        float walk = froxel_frame * 0.61803399;
        float jx = fract(interleavedGradientNoise(gl_FragCoord.xy)                   + walk);
        float jy = fract(interleavedGradientNoise(gl_FragCoord.xy + vec2(59.0, 37.0)) + walk);
        float jz = fract(interleavedGradientNoise(gl_FragCoord.xy + vec2(11.0, 97.0)) + walk);
        jit = vec3(jx, jy, jz) - 0.5; // [-0.5, 0.5]^3, froxel units
    }

    // Froxel sample view-space position (exponential Z, same 0.5 slice-centre
    // convention as every froxel pass) with the F3 jitter applied to the froxel-centre
    // coordinates BEFORE reconstruction (unjittered when froxel_jitter == 0).
    float viewz = froxelSliceToViewZ(float(fz) + 0.5 + jit.z, froxel_near_far, froxel_grid.z);
    vec3  vpos  = froxelViewPos(vec2(float(fx) + 0.5 + jit.x, float(fy) + 0.5 + jit.y), viewz,
                                froxel_grid, froxel_tan_half_fov);

    // Evaluate THIS projector at the froxel centre - one march-sample's worth of the
    // exact projectorVolumetricF math. clipProjectedLightVars gives dist (normalized
    // by size), l_dist (depth along the projector axis) and proj_tc; a true return or
    // an out-of-cookie coord means this froxel is outside the cone -> no contribution.
    vec3  lv;
    vec4  proj_tc;
    float dist, l_dist;
    if (clipProjectedLightVars(center, vpos, dist, l_dist, lv, proj_tc))
    {
        frag_color = vec4(0.0);
        return;
    }
    if (projvol_max_distance > 0.0 && l_dist > projvol_max_distance)
    {
        frag_color = vec4(0.0);
        return;
    }
    if (proj_tc.x < 0.0 || proj_tc.x > 1.0 ||
        proj_tc.y < 0.0 || proj_tc.y > 1.0)
    {
        frag_color = vec4(0.0);
        return;
    }

    // Feathered cone edge: soften the angular falloff so the beam boundary is not a
    // hard line (matches the per-cone projvol_feather). 0 = hard edge (no-op).
    float edge_feather = 1.0;
    if (projvol_feather > 0.0)
    {
        vec2  e2   = min(proj_tc.xy, vec2(1.0) - proj_tc.xy);
        float edge = min(e2.x, e2.y);
        edge_feather = smoothstep(0.0, projvol_feather, edge);
    }

    // Distance/range attenuation, gobo footprint, and self-shadow (this projector's
    // own shadow map -> opaque occluders block the beam in the grid). No surface
    // normal for an airborne froxel, so pass 0 (the norm*offset bias is negligible).
    float atten  = calcLegacyDistanceAttenuation(dist, falloff);
    vec3  cookie = projGoboTexture(l_dist, proj_tc.xy);
    vec2  atlas_uv = gl_FragCoord.xy / froxel_atlas.zw; // stable dither coord (see note above)
    float vis    = sampleSpotShadow(vpos, vec3(0.0), proj_shadow_idx, atlas_uv);

    // Henyey-Greenstein phase. Ldir = light travel direction at this froxel (light
    // centre -> froxel); Vdir = froxel -> camera (camera at view-space origin).
    // Forward scatter (g>0) glows when the camera looks toward the light.
    vec3  Ldir  = normalize(vpos - center);
    vec3  Vdir  = -normalize(vpos);
    float cosT  = dot(Ldir, Vdir);
    float g     = projvol_g;
    float denom = 1.0 + g * g - 2.0 * g * cosT;
    float phase = (1.0 - g * g) / (4.0 * M_PI * pow(max(denom, 1e-4), 1.5));

    // Shared medium scattering coefficient at this froxel (point fetch - the light
    // atlas is atlas-aligned with the media atlas, so gl_FragCoord maps 1:1). rgb =
    // sigma_s. The beam is proportional to it (physical single-scatter coupling).
    vec3 sigma_s = texelFetch(froxelMedia, apix, 0).rgb;

    // In-scatter SOURCE radiance from this light at this froxel:
    //   S = sigma_s * phase * (light incident: color * cookie * atten * vis * edge)
    //       * per-projector brightness multiplier.
    // Additive GL_ONE,GL_ONE across the N injection passes builds the total source;
    // the integrate pass applies the slice path length. a = 0 (unused).
    vec3 S = color * cookie * atten * vis * edge_feather * phase * sigma_s * godray_multiplier;

    frag_color = vec4(S, 0.0);
}
