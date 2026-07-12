/**
 * @file class1/deferred/froxelDebugF.glsl
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
 * [BDMerge Froxel F0] Debug visualizer for the froxel media atlas. Fullscreen
 * overlay drawn at 50% opacity (alpha-blended) over the scene AFTER the media pass,
 * gated by BDMergeFroxelDebug != 0. THE F0 checkpoint deliverable - it lets the user
 * SEE the grid before any lighting exists.
 *   Mode 1: for each screen pixel, trilinear-sample the atlas at the SURFACE depth's
 *           froxel and visualize sigma_t on a black->green->yellow->red ramp over
 *           [0, 2*density]. Proves the froxel coords line up with the scene.
 *   Mode 2: map one Z-slice tile fullscreen (fixed middle slice) - proves the atlas
 *           layout / tiling.
 *   Mode 3: [F1] trilinear-sample the INTEGRATED atlas at the surface froxel and show
 *           its in-scatter radiance L (rgb) directly - validates the integrate pass
 *           independently of the scene composite.
 */

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;

in vec2 vary_fragcoord;

uniform sampler2D froxelMedia;     // the RGBA16F media atlas (rgb=sigma_s, a=sigma_t)
uniform sampler2D froxelIntegrated;// [F1] the RGBA16F integrated atlas (rgb=L, a=T)

uniform vec3  froxel_grid;         // (GridX, GridY, GridZ)
uniform vec4  froxel_atlas;        // (tilesX, tilesY, atlasW, atlasH)
uniform vec2  froxel_near_far;     // (near, far) metres
uniform float froxel_density;      // base sigma_t (ramp normalisation: 2*density = red)
uniform int   froxel_debug_mode;   // 1 = surface-depth density, 2 = Z-slice sweep, 3 = integrated L
uniform float froxel_debug_slice;  // Mode 2: which slice tile to show

// deferredUtil.glsl (this program is isDeferred -> depthMap/inv_proj are bound)
vec4 getPosition(vec2 pos_screen);

// froxelUtil.glsl
float froxelViewZToSlice(float z, vec2 nf, float gz);
vec4  froxelBilinear(sampler2D atlas, float sliceIdx, vec2 local, vec3 grid, vec4 atlasP);
vec4  froxelTrilinear(sampler2D atlas, vec3 fc, vec3 grid, vec4 atlasP);
vec3  froxelRamp(float t);

void main()
{
    vec2 tc = vary_fragcoord.xy; // screen UV in [0,1]

    // Ramp normalisation: red at 2x the base density (so fog/noise multipliers that
    // push density above base saturate to red, and the flat base sits mid-ramp).
    float ramp_max = max(froxel_density, 1.0e-4) * 2.0;

    vec3 col;

    if (froxel_debug_mode == 2)
    {
        // ---- Mode 2: one Z-slice tile stretched fullscreen ---------------------
        // Screen UV maps straight onto the froxel xy of the chosen slice tile, so
        // the whole GX x GY slice fills the view. Proves the tiling / addressing.
        vec2  local = tc * froxel_grid.xy;
        float slice = clamp(froxel_debug_slice, 0.0, froxel_grid.z - 1.0);
        float sigma_t = froxelBilinear(froxelMedia, slice, local, froxel_grid, froxel_atlas).a;
        col = froxelRamp(sigma_t / ramp_max);
    }
    else
    {
        // ---- Mode 1: density at the surface froxel -----------------------------
        // View-space surface position at this pixel; view depth is its distance in
        // front of the eye (-Z). Skip the sky (no opaque surface).
        vec4  pos   = getPosition(tc);
        float viewz = -pos.z;
        if (viewz <= 0.0)
        {
            frag_color = vec4(0.0); // additive/alpha no-op on the sky
            return;
        }

        // Continuous slice coordinate for this depth, converted to sample-centre
        // space (subtract 0.5 so integers land on stored slice centres). Screen UV
        // gives the froxel xy since the grid tiles the frustum across the screen.
        float scoord = froxelViewZToSlice(viewz, froxel_near_far, froxel_grid.z) - 0.5;
        vec2  local  = tc * froxel_grid.xy;
        vec3  fc     = vec3(local, scoord);

        if (froxel_debug_mode == 3)
        {
            // ---- Mode 3: [F1] integrated in-scatter L at the surface froxel --------
            // Show the radiance the apply composite ADDS at this pixel's depth, so the
            // integrate pass can be validated on its own (before/without the scene
            // composite). rgb = L directly; raise Ambient/Density to see it clearly.
            col = froxelTrilinear(froxelIntegrated, fc, froxel_grid, froxel_atlas).rgb;
        }
        else
        {
            float sigma_t = froxelTrilinear(froxelMedia, fc, froxel_grid, froxel_atlas).a;
            col = froxelRamp(sigma_t / ramp_max);
        }
    }

    // 50% alpha overlay (C++ sets SRC_ALPHA / ONE_MINUS_SRC_ALPHA for this pass).
    frag_color = vec4(col, 0.5);
}
