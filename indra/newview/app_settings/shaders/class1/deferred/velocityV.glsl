/**
 * @file velocityV.glsl
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

// [BDMerge A5.4-1a] Screen-space velocity (rigid + camera). Donor: Black Dragon
// (NiranV Dean), adapted for Alchemy's SMAA T2x jitter.
//
// CRITICAL - un-jittered velocity: the SMAA T2x subpixel jitter is baked into
// modelview_projection_matrix / projection_matrix (llviewercamera.cpp). We
// rasterize to the JITTERED position (so this buffer aligns with the jittered
// color/depth buffer) but compute the motion vector from the UN-JITTERED
// projection for BOTH the current and previous positions, otherwise reprojection
// inherits a ~0.5px per-frame wobble. See doc/BDMERGE_A54_MOTIONVECTORS_BRIEF.md
// pitfall 1.

uniform mat4 modelview_projection_matrix;   // current, JITTERED (raster position)
uniform mat4 modelview_matrix;              // current camera*object modelview (jitter-free)
uniform mat4 projection_matrix;             // current, JITTERED (skin raster path)
uniform mat4 projection_matrix_unjittered;  // current projection without T2x jitter (velocity)
uniform mat4 last_modelview_matrix;         // previous frame camera modelview
uniform mat4 last_object_matrix;            // previous frame object matrix (identity = first frame)

in vec3 position;

out vec4 vary_cur_clip;
out vec4 vary_last_clip;

#ifdef HAS_SKIN
mat4 getObjectSkinnedTransform();
mat4 getLastObjectSkinnedTransform();
#endif

void main()
{
#ifdef HAS_SKIN
    // Phase 1b path (skinned/rigged). Not built in Phase 1a.
    mat4 cur_mat = getObjectSkinnedTransform();
    vec4 mv_pos = modelview_matrix * cur_mat * vec4(position.xyz, 1.0);
    gl_Position = projection_matrix * mv_pos;                        // jittered raster

    vary_cur_clip = projection_matrix_unjittered * mv_pos;          // un-jittered
    mat4 last_mat = getLastObjectSkinnedTransform();
    vary_last_clip = projection_matrix_unjittered * last_modelview_matrix * last_mat * vec4(position.xyz, 1.0);
#else
    gl_Position = modelview_projection_matrix * vec4(position.xyz, 1.0);   // jittered raster

    vary_cur_clip  = projection_matrix_unjittered * modelview_matrix * vec4(position.xyz, 1.0);
    vary_last_clip = projection_matrix_unjittered * last_modelview_matrix * last_object_matrix * vec4(position.xyz, 1.0);
#endif
}
