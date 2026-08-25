/**
 * @file bumpF.glsl
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

uniform float minimum_alpha;
uniform sampler2D diffuseMap;
uniform sampler2D bumpMap;

in vec3 vary_mat0;
in vec3 vary_mat1;
in vec3 vary_mat2;

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
float actorFxAuthoredMaterialResponse();
bool actorFxUvTransformEnabled();
bool actorFxRgbSplitEnabled();
vec2 actorFxUv(vec2 authored_uv, vec3 position_eye);
vec2 actorFxRgbSplitUv(vec2 transformed_uv, float direction);
#endif

void main()
{
    mirrorClip(vary_position);

    vec4 col = texture(diffuseMap, vary_texcoord0.xy);

    if(col.a < minimum_alpha)
    {
        discard;
    }
#ifdef HAS_ACTOR_FX
    bool actor_fx_active = actorFxActive();
    vec2 fx_uv = vary_texcoord0.xy;
    if (actor_fx_active && actorFxUvTransformEnabled())
    {
        fx_uv = actorFxUv(fx_uv, vary_position);
        col.rgb = texture(diffuseMap, fx_uv).rgb;
    }
    if (actor_fx_active && actorFxRgbSplitEnabled())
    {
        col.r = texture(diffuseMap, actorFxRgbSplitUv(fx_uv, -1.0)).r;
        col.b = texture(diffuseMap, actorFxRgbSplitUv(fx_uv,  1.0)).b;
    }
#endif
    col *= vertex_color;

#ifdef HAS_ACTOR_FX
    vec3 norm = texture(bumpMap, fx_uv).rgb * 2.0 - 1.0;
#else
    vec3 norm = texture(bumpMap, vary_texcoord0.xy).rgb * 2.0 - 1.0;
#endif

    vec3 tnorm = vec3(dot(norm,vary_mat0),
            dot(norm,vary_mat1),
            dot(norm,vary_mat2));

    vec3 nvn = normalize(tnorm);
#ifdef HAS_ACTOR_FX
    vec3 actor_fx_styled_linear = vec3(0.0);
    if (actor_fx_active)
    {
        actor_fx_styled_linear = actorFxApply(srgb_to_linear(col.rgb), nvn,
                                               vary_position, vary_texcoord0.xy);
        col.rgb = linear_to_srgb(actor_fx_styled_linear);
    }
#endif
    float actor_fx_material_response = 1.0;
#ifdef HAS_ACTOR_FX
    actor_fx_material_response = actorFxAuthoredMaterialResponse();
#endif
    frag_data[0] = vec4(col.rgb, 0.0);
    frag_data[1] = vertex_color.aaaa * actor_fx_material_response; // spec/gloss
    //frag_data[1] = vec4(vec3(vertex_color.a), vertex_color.a+(1.0-vertex_color.a)*vertex_color.a); // spec - from former class3 - maybe better, but not so well tested
    frag_data[2] = encodeNormal(nvn, vertex_color.a * actor_fx_material_response,
                                GBUFFER_FLAG_HAS_ATMOS);

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
