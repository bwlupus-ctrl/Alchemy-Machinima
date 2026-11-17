/**
 * @file shadowCubeV.glsl
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
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

uniform mat4 modelview_projection_matrix;

in vec3 position;
out vec3 vary_actor_fx_position;
uniform vec4 actorFxTransition;      // clone transition: x = style (0 = off); see actorFxDissolveF.glsl
uniform mat4 transitionEyeToBody;     // eye space -> body-space transition coordinate (PLAN 3.1)
out vec3 vary_transition_pos;
uniform mat4 modelview_matrix;

uniform vec3 box_center;
uniform vec3 box_size;

void main()
{
    //transform vertex
    vec3 p = position*box_size+box_center;
    vary_actor_fx_position = p;
    vec4 pos = modelview_projection_matrix*vec4(p.xyz, 1.0);
    vary_transition_pos = actorFxTransition.x > 0.5 ? (transitionEyeToBody * vec4((modelview_matrix * vec4(p.xyz, 1.0)).xyz, 1.0)).xyz : vec3(0.0);

    gl_Position = pos;
}
