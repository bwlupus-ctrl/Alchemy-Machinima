/**
 * @file avatarVelocityV.glsl
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

// [BDMerge A5.4-1b] Classic (system) avatar velocity. Donor: Black Dragon
// avatarVelocityV.glsl, adapted to our un-jittered velocity convention and the
// vary_cur_clip/vary_last_clip interface of velocityF.glsl.
//
// The classic joint palette (llviewerjointmesh uploadJointMatrices) is built
// WITH the modelview baked in (joint_mat *= getModelView()), so the saved
// previous palette already contains the previous camera modelview -- no
// last_modelview multiply here, unlike the mesh-rig path.

uniform mat4 projection_matrix;             // current, JITTERED (raster position)
uniform mat4 projection_matrix_unjittered;  // current projection without T2x jitter (velocity)
uniform mat4 last_projection_matrix_unjittered; // previous un-jittered projection
uniform vec4 lastMatrixPalette[45];

in vec3 position;
in vec4 weight;
in vec2 texcoord0;

out vec4 vary_cur_clip;
out vec4 vary_last_clip;
out vec3 vary_actor_fx_position;
out vec2 vary_texcoord0;
out vec4 vertex_color;

mat4 getSkinnedTransform();

mat4 getLastSkinnedTransform()
{
    mat4 ret;
    int i = int(floor(weight.x));
    float x = fract(weight.x);

    ret[0] = mix(lastMatrixPalette[i+0],  lastMatrixPalette[i+1],  x);
    ret[1] = mix(lastMatrixPalette[i+15], lastMatrixPalette[i+16], x);
    ret[2] = mix(lastMatrixPalette[i+30], lastMatrixPalette[i+31], x);
    ret[3] = vec4(0,0,0,1);

    return ret;

#ifdef IS_AMD_CARD
    vec4 dummy1 = lastMatrixPalette[0];
    vec4 dummy2 = lastMatrixPalette[44];
#endif
}

void main()
{
    vary_actor_fx_position = position;
    // Classic-avatar beauty tests the baked texture alpha only.  Supplying an
    // identity color keeps this interface compatible with velocityAlphaF
    // without requesting a diffuse-color attribute from the avatar VBO.
    vary_texcoord0 = texcoord0;
    vertex_color = vec4(1.0);
    vec4 pos = vec4(position.xyz, 1.0);

    mat4 cur_skin = getSkinnedTransform();
    vec4 cur_view = vec4(pos * cur_skin);       // row-vector convention, see avatarV.glsl
    cur_view.w = 1.0;

    mat4 last_skin = getLastSkinnedTransform();
    vec4 last_view = vec4(pos * last_skin);
    last_view.w = 1.0;

    gl_Position   = projection_matrix * cur_view;              // jittered raster

    vary_cur_clip  = projection_matrix_unjittered * cur_view;  // un-jittered velocity
    vary_last_clip = last_projection_matrix_unjittered * last_view;
}
