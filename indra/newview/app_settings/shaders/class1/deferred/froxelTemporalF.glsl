/**
 * @file class1/deferred/froxelTemporalF.glsl
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
 * [BDMerge Froxel F3] P3 temporal resolve pass. ONE full-atlas draw over the RGBA16F
 * light atlas: every output fragment is one froxel (fx,fy,fz). It temporalizes ONLY
 * the noisy/jittered LIGHT atlas (the media atlas stays deterministic), producing the
 * RESOLVED light atlas the integrate pass then reads. Per froxel:
 *   1. cur = the current froxel's freshly-injected light (point fetch).
 *   2. Reconstruct this froxel's CURRENT world position (froxel centre view pos ->
 *      froxel_inv_modelview).
 *   3. Reproject into the PREVIOUS frame's grid: world pos -> prev view pos (via
 *      froxel_prev_modelview = last frame's world->view) -> prev froxel coords using
 *      the SAME grid params (grid dims / near / far are frame-to-frame constant; C++
 *      invalidates history when they change). Off the grid [0..1]^3 -> validity 0.
 *   4. Sample the previous RESOLVED atlas (froxelLightHistory) with froxelUtil trilinear
 *      at the reprojected coords.
 *   5. Neighborhood clamp: clamp the history rgb to the min/max of the CURRENT froxel's
 *      6-neighborhood (+-x,+-y,+-z texel fetches of this light atlas) - cheaper than a
 *      full 3^3 box and sufficient because the froxel field is smooth in the grid.
 *   6. EMA blend: out = mix(cur, clamped_hist, blend * validity). C++ passes blend 0 on
 *      the first frame / after a history-invalidating change, so it degrades to pure
 *      current with no smear.
 * Output rgb = the resolved in-scatter source; a = 0 (unused). No surface depth is used
 * anywhere - the reprojection is pure world-space grid-to-grid, which is exactly why the
 * froxel temporal path cannot exhibit the wall-vs-beam ghost the per-cone path had to
 * fight (there is no wall in the equation, only the airborne grid).
 */

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;

in vec2 vary_fragcoord;

uniform sampler2D froxelLight;         // RGBA16F current light atlas (rgb = injected source)
uniform sampler2D froxelLightHistory;  // RGBA16F previous RESOLVED light atlas (trilinear)
uniform sampler2D froxelMedia;         // current media; alpha is extinction density

uniform vec3  froxel_grid;             // (GridX, GridY, GridZ)
uniform vec4  froxel_atlas;            // (tilesX, tilesY, atlasW, atlasH)
uniform vec2  froxel_near_far;         // (camera near, BDMergeFroxelFar) metres
uniform vec2  froxel_tan_half_fov;     // (tan(fovx/2), tan(fovy/2)) for view-pos reconstruct
uniform mat4  froxel_inv_modelview;    // current view -> agent(world)
uniform mat4  froxel_prev_modelview;   // agent(world) -> PREVIOUS-frame view
uniform float froxel_temporal_blend;   // EMA history weight (0 => pure current)
uniform float froxel_temporal_reject;  // density-edge history rejection strength

// froxelUtil.glsl
float froxelSliceToViewZ(float slice, vec2 nf, float gz);
float froxelViewZToSlice(float z, vec2 nf, float gz);
vec3  froxelViewPos(vec2 fxy, float viewz, vec3 grid, vec2 thf);
vec4  froxelTrilinear(sampler2D atlas, vec3 fc, vec3 grid, vec4 atlasP);

// Atlas pixel of froxel (fx,fy,slice): its tile is (slice % tilesX, slice / tilesX),
// origin (tx*gx, ty*gy); the in-tile texel is (fx,fy). Used for the 6-neighborhood
// fetches (a +-z neighbor lives in a DIFFERENT tile, so it needs the full remap).
ivec2 froxelAtlasPixel(int fx, int fy, int slice, int gx, int gy, int tilesX)
{
    int tx = slice - (slice / tilesX) * tilesX; // slice % tilesX
    int ty = slice / tilesX;
    return ivec2(tx * gx + fx, ty * gy + fy);
}

void main()
{
    int gx     = int(froxel_grid.x);
    int gy     = int(froxel_grid.y);
    int gz     = int(froxel_grid.z);
    int tilesX = int(froxel_atlas.x);

    // Decode the atlas pixel -> tile (tx,ty) -> froxel (fx,fy,fz). Same convention as
    // the media / inject / integrate passes (gl_FragCoord pixel-centred; truncate).
    ivec2 apix = ivec2(gl_FragCoord.xy);
    int   tx   = apix.x / gx;
    int   ty   = apix.y / gy;
    int   fx   = apix.x - tx * gx;
    int   fy   = apix.y - ty * gy;
    int   fz   = ty * tilesX + tx;

    // Dead tiles past the last real slice: pass through 0 (never sampled by integrate).
    if (fz >= gz)
    {
        frag_color = vec4(0.0);
        return;
    }

    // Current froxel's injected source.
    vec3 cur = texelFetch(froxelLight, apix, 0).rgb;

    // This froxel's CURRENT world position (exponential-Z centre, same 0.5 convention).
    float viewz = froxelSliceToViewZ(float(fz) + 0.5, froxel_near_far, froxel_grid.z);
    vec3  vpos  = froxelViewPos(vec2(float(fx) + 0.5, float(fy) + 0.5), viewz,
                                froxel_grid, froxel_tan_half_fov);
    vec3  wpos  = (froxel_inv_modelview * vec4(vpos, 1.0)).xyz;

    // Reproject into the PREVIOUS frame's grid (pure world-space -> prev view -> prev
    // froxel coords). Invert froxelViewPos: ndc = pos.xy / (thf * viewz);
    // fxy = (ndc*0.5 + 0.5) * grid.xy. The prev slice coordinate is the analytic
    // exp-Z inverse of the prev view depth.
    vec3  pv       = (froxel_prev_modelview * vec4(wpos, 1.0)).xyz;
    float validity = 1.0;
    vec3  histrgb  = cur;

    float prev_viewz = -pv.z; // positive distance in front of the previous eye
    if (prev_viewz <= froxel_near_far.x)
    {
        validity = 0.0; // nearer than near / behind the previous camera -> no history
    }
    else
    {
        vec2  ndc      = pv.xy / (froxel_tan_half_fov * prev_viewz);
        vec2  prev_fxy = (ndc * 0.5 + 0.5) * froxel_grid.xy;
        float prev_sl  = froxelViewZToSlice(prev_viewz, froxel_near_far, froxel_grid.z);

        // Outside the previous grid (camera cut / fast motion at an edge) -> validity 0.
        if (prev_fxy.x < 0.0 || prev_fxy.x > froxel_grid.x ||
            prev_fxy.y < 0.0 || prev_fxy.y > froxel_grid.y ||
            prev_sl   < 0.0 || prev_sl   > froxel_grid.z)
        {
            validity = 0.0;
        }
        else
        {
            // froxelTrilinear wants fc.z in sample-centre space (integer lands on a
            // stored slice centre), so subtract 0.5 exactly like the apply pass.
            vec3 fc = vec3(prev_fxy, prev_sl - 0.5);
            histrgb = froxelTrilinear(froxelLightHistory, fc, froxel_grid, froxel_atlas).rgb;
        }
    }

    // ---- 6-neighborhood clamp (anti-ghost safety net) -----------------------
    // Bound the reprojected history to the current froxel's +-x,+-y,+-z neighbourhood
    // min/max in the light atlas. Neighbours that fall outside the grid are skipped
    // (the froxel field has no defined value there), leaving the centre in the bound.
    vec3 nmin = cur;
    vec3 nmax = cur;
    if (fx - 1 >= 0)   { vec3 n = texelFetch(froxelLight, froxelAtlasPixel(fx - 1, fy, fz, gx, gy, tilesX), 0).rgb; nmin = min(nmin, n); nmax = max(nmax, n); }
    if (fx + 1 < gx)   { vec3 n = texelFetch(froxelLight, froxelAtlasPixel(fx + 1, fy, fz, gx, gy, tilesX), 0).rgb; nmin = min(nmin, n); nmax = max(nmax, n); }
    if (fy - 1 >= 0)   { vec3 n = texelFetch(froxelLight, froxelAtlasPixel(fx, fy - 1, fz, gx, gy, tilesX), 0).rgb; nmin = min(nmin, n); nmax = max(nmax, n); }
    if (fy + 1 < gy)   { vec3 n = texelFetch(froxelLight, froxelAtlasPixel(fx, fy + 1, fz, gx, gy, tilesX), 0).rgb; nmin = min(nmin, n); nmax = max(nmax, n); }
    if (fz - 1 >= 0)   { vec3 n = texelFetch(froxelLight, froxelAtlasPixel(fx, fy, fz - 1, gx, gy, tilesX), 0).rgb; nmin = min(nmin, n); nmax = max(nmax, n); }
    if (fz + 1 < gz)   { vec3 n = texelFetch(froxelLight, froxelAtlasPixel(fx, fy, fz + 1, gx, gy, tilesX), 0).rgb; nmin = min(nmin, n); nmax = max(nmax, n); }

    vec3 hclamp = clamp(histrgb, nmin, nmax);

    // EMA blend toward the clamped/validated history. HARD select on w>0 (not a mix
    // with w=0): on the first frame froxel_prev_modelview is uninitialized, so histrgb
    // can be NaN, and mix(cur, NaN, 0.0) would poison the froxel to NaN. When w<=0
    // (first frame / invalidated history / off-grid reprojection) we output cur exactly.
    float w    = clamp(froxel_temporal_blend * validity, 0.0, 0.95);
    if (froxel_temporal_reject > 0.0)
    {
        float density = texelFetch(froxelMedia, apix, 0).a;
        float density_min = density;
        float density_max = density;
        if (fx - 1 >= 0) { float d = texelFetch(froxelMedia, froxelAtlasPixel(fx - 1, fy, fz, gx, gy, tilesX), 0).a; density_min = min(density_min, d); density_max = max(density_max, d); }
        if (fx + 1 < gx) { float d = texelFetch(froxelMedia, froxelAtlasPixel(fx + 1, fy, fz, gx, gy, tilesX), 0).a; density_min = min(density_min, d); density_max = max(density_max, d); }
        if (fy - 1 >= 0) { float d = texelFetch(froxelMedia, froxelAtlasPixel(fx, fy - 1, fz, gx, gy, tilesX), 0).a; density_min = min(density_min, d); density_max = max(density_max, d); }
        if (fy + 1 < gy) { float d = texelFetch(froxelMedia, froxelAtlasPixel(fx, fy + 1, fz, gx, gy, tilesX), 0).a; density_min = min(density_min, d); density_max = max(density_max, d); }
        if (fz - 1 >= 0) { float d = texelFetch(froxelMedia, froxelAtlasPixel(fx, fy, fz - 1, gx, gy, tilesX), 0).a; density_min = min(density_min, d); density_max = max(density_max, d); }
        if (fz + 1 < gz) { float d = texelFetch(froxelMedia, froxelAtlasPixel(fx, fy, fz + 1, gx, gy, tilesX), 0).a; density_min = min(density_min, d); density_max = max(density_max, d); }
        float change = (density_max - density_min) /
                       (density_max + density_min + 1e-3);
        w *= exp(-froxel_temporal_reject * change);
    }
    vec3  outc = (w > 0.0) ? mix(cur, hclamp, w) : cur;

    frag_color = vec4(outc, 0.0);
}
