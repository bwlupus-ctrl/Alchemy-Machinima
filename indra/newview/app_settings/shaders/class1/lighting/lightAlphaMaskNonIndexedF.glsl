/**
 * @file class1\lighting\lightAlphaMaskNonIndexedF.glsl
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

out vec4 frag_color;

uniform float minimum_alpha;

uniform sampler2D diffuseMap;

vec3 atmosLighting(vec3 light);
vec3 scaleSoftClip(vec3 light);
vec3 srgb_to_linear(vec3 c);
vec3 linear_to_srgb(vec3 c);

in vec4 vertex_color;
in vec2 vary_texcoord0;
#ifdef HAS_ACTOR_FX
in vec3 vary_actor_fx_normal;
in vec3 vary_actor_fx_eye_position;
vec3 actorFxApply(vec3 source, vec3 normal_eye, vec3 position_eye, vec2 authored_uv);
vec3 actorFxEmissive(vec3 authored_emissive, vec3 styled_color);
bool actorFxActive();
bool actorFxUvTransformEnabled();
bool actorFxRgbSplitEnabled();
vec2 actorFxUv(vec2 authored_uv, vec3 position_eye);
vec2 actorFxRgbSplitUv(vec2 transformed_uv, float direction);
#endif

void default_lighting()
{
    vec4 color = texture(diffuseMap,vary_texcoord0.xy);

    if (color.a < minimum_alpha)
    {
        discard;
    }

#ifdef HAS_ACTOR_FX
    bool actor_fx_active = actorFxActive();
    vec3 actor_fx_emissive = vec3(0.0);
    if (actor_fx_active)
    {
        vec2 fx_uv = vary_texcoord0.xy;
        if (actorFxUvTransformEnabled())
        {
            fx_uv = actorFxUv(fx_uv, vary_actor_fx_eye_position);
            color.rgb = texture(diffuseMap, fx_uv).rgb;
        }
        if (actorFxRgbSplitEnabled())
        {
            color.r = texture(diffuseMap, actorFxRgbSplitUv(fx_uv, -1.0)).r;
            color.b = texture(diffuseMap, actorFxRgbSplitUv(fx_uv,  1.0)).b;
        }
    }
#endif

    color *= vertex_color;

#ifdef HAS_ACTOR_FX
    if (actor_fx_active)
    {
        vec3 actor_fx_styled = actorFxApply(srgb_to_linear(color.rgb),
                                            normalize(vary_actor_fx_normal),
                                            vary_actor_fx_eye_position,
                                            vary_texcoord0.xy);
        actor_fx_emissive = actorFxEmissive(vec3(0.0), actor_fx_styled);
        color.rgb = linear_to_srgb(actor_fx_styled);
    }
#endif

    color.rgb = atmosLighting(color.rgb);
#ifdef HAS_ACTOR_FX
    if (actor_fx_active &&
        max(max(actor_fx_emissive.r, actor_fx_emissive.g), actor_fx_emissive.b) > 0.0)
    {
        color.rgb = linear_to_srgb(srgb_to_linear(color.rgb) + actor_fx_emissive);
    }
#endif

    color.rgb = scaleSoftClip(color.rgb);

    frag_color = max(color, vec4(0));
}
