/**
 * @file class1/deferred/froxelApplyF.glsl
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
 * [BDMerge Froxel F1] P5 apply pass. Fullscreen composite of the integrated froxel
 * atlas onto the scene. Per pixel: reconstruct the surface view depth, convert it to
 * a continuous slice coordinate (minus the 0.5 slice-centre offset so an integer
 * lands on a stored slice centre), trilinear-sample the integrated atlas -> (L, T),
 * and output frag_color = vec4(L, T). C++ sets blend GL_ONE / GL_SRC_ALPHA so the
 * framebuffer becomes dst' = L + dst*T == scene*T + L: the fog both extinguishes the
 * scene (T) and adds its own in-scatter glow (L), WITHOUT ever sampling the target it
 * writes. Sky / no-depth pixels sit beyond froxel_far and clamp to the last slice
 * (full accumulated fog).
 */

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;

in vec2 vary_fragcoord;

uniform sampler2D froxelIntegrated; // RGBA16F integrated atlas (rgb = L, a = T)

uniform vec3 froxel_grid;           // (GridX, GridY, GridZ)
uniform vec4 froxel_atlas;          // (tilesX, tilesY, atlasW, atlasH)
uniform vec2 froxel_near_far;       // (near, far) metres

// deferredUtil.glsl (this program is isDeferred -> depthMap/inv_proj are bound)
vec4 getPosition(vec2 pos_screen);

// froxelUtil.glsl
float froxelViewZToSlice(float z, vec2 nf, float gz);
vec4  froxelTrilinear(sampler2D atlas, vec3 fc, vec3 grid, vec4 atlasP);

void main()
{
    vec2 tc = vary_fragcoord.xy; // screen UV in [0,1]

    // Surface view depth (positive distance in front of the eye). Sky pixels sit at
    // the far plane, giving a large viewz that clamps to froxel_far below (=> the last
    // slice, full accumulated fog). The viewz<=0 guard covers degenerate positions.
    vec4  pos   = getPosition(tc);
    float viewz = -pos.z;
    if (viewz <= 0.0)
    {
        viewz = froxel_near_far.y;
    }
    viewz = clamp(viewz, froxel_near_far.x, froxel_near_far.y);

    // Continuous slice coordinate for this depth, in sample-centre space (subtract
    // 0.5 so integers land on stored slice centres - the froxelUtil convention). The
    // grid tiles the frustum across the screen, so screen UV gives the froxel xy.
    float scoord = froxelViewZToSlice(viewz, froxel_near_far, froxel_grid.z) - 0.5;
    vec2  local  = tc * froxel_grid.xy;
    vec3  fc     = vec3(local, scoord);

    vec4  acc = froxelTrilinear(froxelIntegrated, fc, froxel_grid, froxel_atlas);
    vec3  L   = acc.rgb; // integrated in-scatter radiance to this depth
    float T   = clamp(acc.a, 0.0, 1.0); // transmittance camera -> this depth

    // Output (L, T); the GL_ONE / GL_SRC_ALPHA blend does scene*T + L in place.
    frag_color = vec4(L, T);
}
