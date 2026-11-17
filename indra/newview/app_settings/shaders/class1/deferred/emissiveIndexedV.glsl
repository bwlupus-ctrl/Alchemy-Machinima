/**
 * @file emissiveIndexedV.glsl
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

// Indexed (multi-material) variant of emissiveV.glsl (legacy glow). Forwards the
// per-vertex material slot to the fragment shader so each slot's diffuse alpha can
// be sampled. The diffuse texture transform is baked into texcoord0 at buffer build
// (indexed faces exclude texture animation), so texture_matrix0 is not applied.
// HAS_SKIN adds rigged matrix-palette skinning.

#ifdef HAS_SKIN
uniform mat4 modelview_matrix;
uniform mat4 projection_matrix;
mat4 getObjectSkinnedTransform();
#else
uniform mat4 modelview_projection_matrix;
#endif

invariant gl_Position; // [VelZFight] Match indexed beauty depth positions.

in vec3 position;
#ifdef HAS_ACTOR_FX
out vec3 vary_actor_fx_position;
uniform vec4 actorFxTransition;      // clone transition: x = style (0 = off); see actorFxDissolveF.glsl
uniform mat4 transitionEyeToBody;     // eye space -> body-space transition coordinate (PLAN 3.1)
out vec3 vary_transition_pos;
#ifndef HAS_SKIN
uniform mat4 modelview_matrix;   // rigid path: eye position for the transition coordinate
#endif
#endif
in vec4 emissive;
in vec4 diffuse_color;
in vec2 texcoord0;
in int texture_index;

flat out int vary_material_index;
out vec4 vertex_color;
out float vertex_alpha;
out vec2 vary_texcoord0;

void main()
{
#ifdef HAS_ACTOR_FX
    vary_actor_fx_position = position;
#endif
#ifdef HAS_SKIN
    mat4 mat = getObjectSkinnedTransform();
    mat = modelview_matrix * mat;
    vec3 pos = (mat*vec4(position.xyz,1.0)).xyz;
    gl_Position = projection_matrix*vec4(pos,1.0);
#else
    gl_Position = modelview_projection_matrix * vec4(position.xyz, 1.0);
#endif

#ifdef HAS_ACTOR_FX
#ifdef HAS_SKIN
    vary_transition_pos = actorFxTransition.x > 0.5 ? (transitionEyeToBody * vec4(pos, 1.0)).xyz : vec3(0.0);
#else
    vary_transition_pos = actorFxTransition.x > 0.5 ? (transitionEyeToBody * vec4((modelview_matrix * vec4(position.xyz, 1.0)).xyz, 1.0)).xyz : vec3(0.0);
#endif
#endif
    vary_material_index = texture_index;
    vary_texcoord0 = texcoord0;
    vertex_color = emissive;
    vertex_alpha = diffuse_color.a;
}
