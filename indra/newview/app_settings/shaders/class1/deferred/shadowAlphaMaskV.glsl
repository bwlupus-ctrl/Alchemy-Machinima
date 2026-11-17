/**
 * @file shadowAlphaMaskV.glsl
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2011, Linden Research, Inc.
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

uniform mat4 texture_matrix0;
#if defined(HAS_SKIN)
uniform mat4 modelview_matrix;
uniform mat4 projection_matrix;
mat4 getObjectSkinnedTransform();
#else
uniform mat4 modelview_projection_matrix;
#endif


in vec3 position;
in vec4 diffuse_color;
in vec2 texcoord0;

out vec4 post_pos;
out vec4 vertex_color;
out vec2 vary_texcoord0;
out vec3 vary_actor_fx_position;
uniform vec4 actorFxTransition;      // clone transition: x = style (0 = off); see actorFxDissolveF.glsl
uniform mat4 transitionEyeToBody;     // eye space -> body-space transition coordinate (PLAN 3.1)
out vec3 vary_transition_pos;
#ifndef HAS_SKIN
uniform mat4 modelview_matrix;   // rigid path: eye position for the transition coordinate
#endif

void passTextureIndex();

void main()
{
    vary_actor_fx_position = position;
    //transform vertex
#if defined(HAS_SKIN)
    vec4 pre_pos = vec4(position.xyz, 1.0);
    mat4 mat = getObjectSkinnedTransform();
    mat = modelview_matrix * mat;
    vec4 pos = mat * pre_pos;
    pos = projection_matrix * pos;
#else
    vec4 pre_pos = vec4(position.xyz, 1.0);
    vec4 pos = modelview_projection_matrix * pre_pos;
#endif


#ifdef HAS_SKIN
    vary_transition_pos = actorFxTransition.x > 0.5 ? (transitionEyeToBody * vec4((mat * pre_pos).xyz, 1.0)).xyz : vec3(0.0);
#else
    vary_transition_pos = actorFxTransition.x > 0.5 ? (transitionEyeToBody * vec4((modelview_matrix * vec4(position.xyz, 1.0)).xyz, 1.0)).xyz : vec3(0.0);
#endif
    post_pos = pos;

    gl_Position = pos;

    passTextureIndex();

    vary_texcoord0 = (texture_matrix0 * vec4(texcoord0,0,1)).xy;
    vertex_color = diffuse_color;
}
