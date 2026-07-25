/**
 * @file velocityCameraF.glsl
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
 */

// [BDMerge A5.4-1c] Fullscreen CAMERA-motion fallback for the velocity buffer.
//
// The geometry velocity pass (velocityV/F) only stamps pixels whose draw pools
// implement the velocity hooks (rigid opaque in Phase 1a). Everything else --
// avatars/rigged meshes (Phase 1b), sky, excluded alpha -- previously kept the
// clear value (0,0) == "static", which is WRONG whenever the camera moves:
// temporal consumers (SMAA T2x, ReShade bridge -> iMMERSE RTGI) then ghost on
// exactly those pixels. This pass runs FIRST and fills every pixel with the
// camera-induced motion reprojected from scene depth; the geometry pass then
// overwrites covered pixels with true per-object motion.
//
// Scene-depth pixels are static world points. Clear/far depth is sky, where a
// finite reconstructed point would incorrectly acquire camera translation; it
// is therefore reprojected as a direction (w=0), giving rotation-only motion.
//
// Same un-jittered convention as velocityF.glsl (pitfall 1): reconstruct with
// the JITTERED inverse projection (matches the rasterized depth), but reproject
// BOTH endpoints through the UN-jittered projection so T2x jitter cannot leak
// a ~0.5px per-frame wobble into the vectors.

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;

in vec2 vary_fragcoord;

uniform sampler2D depthMap;

uniform mat4 inv_proj;                      // current JITTERED projection inverse (auto-fed on draw)
uniform mat4 inv_modelview;                 // current camera modelview inverse (auto-fed on draw)
uniform mat4 last_modelview_matrix;         // previous frame camera modelview
uniform mat4 projection_matrix_unjittered;  // current projection without T2x jitter
uniform mat4 last_projection_matrix_unjittered; // previous un-jittered projection

void main()
{
    float depth = texture(depthMap, vary_fragcoord).r;

    // current view-space position of this pixel (jittered path matches raster)
    vec4 ndc = vec4(vary_fragcoord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view_pos = inv_proj * ndc;
    view_pos /= view_pos.w;

    // A far-clear sample has no finite world position. Reproject its direction
    // with w=0 so camera translation cannot move the sky.
    bool is_sky = depth >= 0.999999;
    if (is_sky)
    {
        view_pos.w = 0.0;
    }
    vec4 world_pos = inv_modelview * view_pos;
    vec4 last_view_pos = last_modelview_matrix * world_pos;

    // Each endpoint uses its own un-jittered projection.
    vec4 cur_clip  = projection_matrix_unjittered * view_pos;
    vec4 last_clip = last_projection_matrix_unjittered * last_view_pos;

    vec2 cur_ndc  = cur_clip.xy  / cur_clip.w;
    vec2 last_ndc = last_clip.xy / last_clip.w;

    frag_color = vec4(cur_ndc - last_ndc, 0.0, 1.0);
}
