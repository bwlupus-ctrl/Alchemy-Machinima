/**
 * @file velocityAlphaF.glsl
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

// [BDMerge A5.4-1a] Alpha-mask (cutout) velocity fragment.

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;

uniform sampler2D diffuseMap;
uniform float minimum_alpha;
uniform int velocity_texture_alpha_only;

in vec4 vary_cur_clip;
in vec4 vary_last_clip;
in vec2 vary_texcoord0;
in vec4 vertex_color;
in vec3 vary_actor_fx_position;

bool actorFxDissolveDiscard(vec3 raw_object_position);

void main()
{
    float alpha = texture(diffuseMap, vary_texcoord0.xy).a;
    if (velocity_texture_alpha_only == 0)
    {
        alpha *= vertex_color.a;
    }

    if (alpha < minimum_alpha)
    {
        discard;
    }

    // Keep the authored cutout test above on its original UV. Actor FX only
    // subtracts further coverage and never changes the material's alpha.
    if (actorFxDissolveDiscard(vary_actor_fx_position))
    {
        discard;
    }

    vec2 cur_ndc  = vary_cur_clip.xy  / vary_cur_clip.w;
    vec2 last_ndc = vary_last_clip.xy / vary_last_clip.w;

    frag_color = vec4(cur_ndc - last_ndc, 0.0, 1.0);
}
