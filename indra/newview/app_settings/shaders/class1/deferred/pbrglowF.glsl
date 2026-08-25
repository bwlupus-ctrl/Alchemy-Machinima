/**
 * @file pbrglowF.glsl
 *
 * $LicenseInfo:firstyear=2023&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2023, Linden Research, Inc.
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

// forward fullbright implementation for HUDs

uniform sampler2D diffuseMap;  //always in sRGB space

uniform vec3 emissiveColor;
uniform sampler2D emissiveMap;

out vec4 frag_color;

in vec3 vary_position;
in vec4 vertex_emissive;
in vec4 vertex_color;

in vec2 base_color_texcoord;
in vec2 emissive_texcoord;

uniform float minimum_alpha; // PBR alphaMode: MASK, See: mAlphaCutoff, setAlphaCutoff()

vec3 linear_to_srgb(vec3 c);
vec3 srgb_to_linear(vec3 c);
#ifdef HAS_ACTOR_FX
uniform int actorFxUseCoverageAlpha;
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
    vec4 basecolor = texture(diffuseMap, base_color_texcoord.xy).rgba;

    // Opaque/MASK beauty applies the base-colour factor before its cutoff.
    // BLEND beauty intentionally cuts on texture alpha alone, then uses the
    // combined alpha as coverage. actorFxUseCoverageAlpha identifies that
    // latter alpha-pool use of this shared glow shader.
    float cutoff_alpha = actorFxUseCoverageAlpha != 0
        ? basecolor.a : basecolor.a * vertex_color.a;
    if (cutoff_alpha < minimum_alpha)
    {
        discard;
    }

#ifdef HAS_ACTOR_FX
    bool actor_fx_active = actorFxActive();
    bool fx_uv_transform = actor_fx_active && actorFxUvTransformEnabled();
    bool fx_rgb_split = actor_fx_active && actorFxRgbSplitEnabled();
    vec2 fx_base_uv = base_color_texcoord.xy;
    if (fx_uv_transform)
    {
        fx_base_uv = actorFxUv(fx_base_uv, vary_position);
        basecolor.rgb = texture(diffuseMap, fx_base_uv).rgb;
    }
    if (fx_rgb_split)
    {
        basecolor.r = texture(diffuseMap, actorFxRgbSplitUv(fx_base_uv, -1.0)).r;
        basecolor.b = texture(diffuseMap, actorFxRgbSplitUv(fx_base_uv,  1.0)).b;
    }
#endif

    vec3 emissive = emissiveColor;
#ifdef HAS_ACTOR_FX
    vec2 fx_emissive_uv = emissive_texcoord.xy;
    if (fx_uv_transform)
    {
        fx_emissive_uv = actorFxUv(fx_emissive_uv, vary_position);
    }
    vec3 emissive_texel = texture(emissiveMap, fx_emissive_uv).rgb;
    if (fx_rgb_split)
    {
        emissive_texel.r = texture(emissiveMap,
                                   actorFxRgbSplitUv(fx_emissive_uv, -1.0)).r;
        emissive_texel.b = texture(emissiveMap,
                                   actorFxRgbSplitUv(fx_emissive_uv,  1.0)).b;
    }
    emissive *= srgb_to_linear(emissive_texel);
#else
    emissive *= srgb_to_linear(texture(emissiveMap, emissive_texcoord.xy).rgb);
#endif

    float lum = max(max(emissive.r, emissive.g), emissive.b);
    lum *= vertex_emissive.a;
#ifdef HAS_ACTOR_FX
    if (actor_fx_active)
    {
        // Glow batches omit tangent/normal attributes; reconstruct the actual
        // eye-space geometric surface normal from position derivatives.
        vec3 actor_fx_normal = cross(dFdx(vary_position), dFdy(vary_position));
        float actor_fx_normal_len2 = dot(actor_fx_normal, actor_fx_normal);
        actor_fx_normal = actor_fx_normal_len2 > 1e-12
            ? actor_fx_normal * inversesqrt(actor_fx_normal_len2)
            : vec3(0.0, 0.0, 1.0);
        vec3 styled_source = vertex_color.rgb * srgb_to_linear(basecolor.rgb);
        vec3 styled = actorFxApply(styled_source,
                                   actor_fx_normal,
                                   vary_position,
                                   base_color_texcoord.xy);
        vec3 fx_emissive = actorFxEmissive(vec3(0.0), styled);
        float fx_coverage = actorFxUseCoverageAlpha != 0
            ? basecolor.a * vertex_color.a : 1.0;
        lum += max(max(fx_emissive.r, fx_emissive.g), fx_emissive.b) * fx_coverage;
    }
#endif

    // HUDs are rendered after gamma correction, output in sRGB space
    frag_color.rgb = vec3(0);
    frag_color.a = lum;
}
