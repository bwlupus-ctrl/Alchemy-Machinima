/**
 * @file diffuseF.glsl
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

/*[EXTRA_CODE_HERE]*/

out vec4 frag_data[4];

uniform sampler2D diffuseMap;

in vec3 vary_normal;
in vec4 vertex_color;
in vec2 vary_texcoord0;
in vec3 vary_position;

void mirrorClip(vec3 pos);
vec4 encodeNormal(vec3 n, float env, float gbuffer_flag);
vec3 srgb_to_linear(vec3 c);
vec3 linear_to_srgb(vec3 c);
#ifdef HAS_ACTOR_FX
vec3 actorFxApply(vec3 source, vec3 normal_eye, vec3 position_eye, vec2 authored_uv);
vec3 actorFxEmissive(vec3 authored_emissive, vec3 styled_color);
bool actorFxActive();
bool actorFxUvTransformEnabled();
bool actorFxRgbSplitEnabled();
vec2 actorFxUv(vec2 authored_uv, vec3 position_eye);
vec2 actorFxRgbSplitUv(vec2 transformed_uv, float direction);
#endif

void main()
{
    mirrorClip(vary_position);
#ifdef HAS_ACTOR_FX
    bool actor_fx_active = actorFxActive();
    vec2 fx_uv = vary_texcoord0.xy;
    if (actor_fx_active && actorFxUvTransformEnabled())
    {
        fx_uv = actorFxUv(fx_uv, vary_position);
    }
    vec3 texel = texture(diffuseMap, fx_uv).rgb;
    if (actor_fx_active && actorFxRgbSplitEnabled())
    {
        texel.r = texture(diffuseMap, actorFxRgbSplitUv(fx_uv, -1.0)).r;
        texel.b = texture(diffuseMap, actorFxRgbSplitUv(fx_uv,  1.0)).b;
    }
    vec3 col = vertex_color.rgb * texel;
#else
    vec3 col = vertex_color.rgb * texture(diffuseMap, vary_texcoord0.xy).rgb;
#endif
    vec3 nvn = normalize(vary_normal);
#ifdef HAS_ACTOR_FX
    vec3 actor_fx_styled_linear = vec3(0.0);
    if (actor_fx_active)
    {
        actor_fx_styled_linear = actorFxApply(srgb_to_linear(col), nvn,
                                               vary_position, vary_texcoord0.xy);
        col = linear_to_srgb(actor_fx_styled_linear);
    }
#endif
    frag_data[0] = vec4(col, 0.0);
    frag_data[1] = vertex_color.aaaa; // spec
    frag_data[2] = encodeNormal(nvn.xyz, vertex_color.a, GBUFFER_FLAG_HAS_ATMOS);

#if defined(HAS_EMISSIVE)
    frag_data[3] = vec4(0, 0, 0, 0);
#ifdef HAS_ACTOR_FX
    if (actor_fx_active)
    {
        frag_data[3].rgb = actorFxEmissive(vec3(0.0), actor_fx_styled_linear);
    }
#endif
#endif
}
