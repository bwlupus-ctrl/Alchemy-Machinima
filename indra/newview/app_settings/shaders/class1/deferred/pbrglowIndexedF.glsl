/**
 * @file pbrglowIndexedF.glsl
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

// Indexed (multi-material) PBR glow/emissive fragment shader. vary_material_index
// selects this primitive's material slot; each slot binds its base-color map to
// unit s and its emissive map to unit 3N+s (N == GLTF_INDEXED_CHANNELS), matching
// the GBuffer indexed layout so pushGLTFBatchIndexed can drive both. If-chains
// (not switch) are used for driver portability. See pbrglowF.glsl for the
// single-material equivalent.

uniform vec3  gltf_emissive_color[GLTF_INDEXED_CHANNELS];
uniform float gltf_minimum_alpha[GLTF_INDEXED_CHANNELS]; // PBR alphaMode MASK cutoff, -1 for opaque

uniform sampler2D basecolor0;    // always in sRGB space
uniform sampler2D emissivemap0;
#if GLTF_INDEXED_CHANNELS > 1
uniform sampler2D basecolor1; uniform sampler2D emissivemap1;
#endif
#if GLTF_INDEXED_CHANNELS > 2
uniform sampler2D basecolor2; uniform sampler2D emissivemap2;
#endif
#if GLTF_INDEXED_CHANNELS > 3
uniform sampler2D basecolor3; uniform sampler2D emissivemap3;
#endif
#if GLTF_INDEXED_CHANNELS > 4
uniform sampler2D basecolor4; uniform sampler2D emissivemap4;
#endif
#if GLTF_INDEXED_CHANNELS > 5
uniform sampler2D basecolor5; uniform sampler2D emissivemap5;
#endif
#if GLTF_INDEXED_CHANNELS > 6
uniform sampler2D basecolor6; uniform sampler2D emissivemap6;
#endif
#if GLTF_INDEXED_CHANNELS > 7
uniform sampler2D basecolor7; uniform sampler2D emissivemap7;
#endif

out vec4 frag_color;

in vec4 vertex_emissive;
in vec4 vertex_color;
flat in int vary_material_index;
in vec3 vary_position;

in vec2 base_color_texcoord;
in vec2 emissive_texcoord;

vec3 linear_to_srgb(vec3 c);
vec3 srgb_to_linear(vec3 c);
#ifdef HAS_ACTOR_FX
vec3 actorFxApply(vec3 source, vec3 normal_eye, vec3 position_eye, vec2 authored_uv);
vec3 actorFxEmissive(vec3 authored_emissive, vec3 styled_color);
bool actorFxActive();
bool actorFxUvTransformEnabled();
bool actorFxRgbSplitEnabled();
vec2 actorFxUv(vec2 authored_uv, vec3 position_eye);
vec2 actorFxRgbSplitUv(vec2 transformed_uv, float direction);
#endif

vec4 sample_basecolor(vec2 uv)
{
    if (vary_material_index == 0) return texture(basecolor0, uv);
#if GLTF_INDEXED_CHANNELS > 1
    if (vary_material_index == 1) return texture(basecolor1, uv);
#endif
#if GLTF_INDEXED_CHANNELS > 2
    if (vary_material_index == 2) return texture(basecolor2, uv);
#endif
#if GLTF_INDEXED_CHANNELS > 3
    if (vary_material_index == 3) return texture(basecolor3, uv);
#endif
#if GLTF_INDEXED_CHANNELS > 4
    if (vary_material_index == 4) return texture(basecolor4, uv);
#endif
#if GLTF_INDEXED_CHANNELS > 5
    if (vary_material_index == 5) return texture(basecolor5, uv);
#endif
#if GLTF_INDEXED_CHANNELS > 6
    if (vary_material_index == 6) return texture(basecolor6, uv);
#endif
#if GLTF_INDEXED_CHANNELS > 7
    if (vary_material_index == 7) return texture(basecolor7, uv);
#endif
    return vec4(1, 0, 1, 1);
}

vec3 sample_emissive(vec2 uv)
{
    if (vary_material_index == 0) return texture(emissivemap0, uv).rgb;
#if GLTF_INDEXED_CHANNELS > 1
    if (vary_material_index == 1) return texture(emissivemap1, uv).rgb;
#endif
#if GLTF_INDEXED_CHANNELS > 2
    if (vary_material_index == 2) return texture(emissivemap2, uv).rgb;
#endif
#if GLTF_INDEXED_CHANNELS > 3
    if (vary_material_index == 3) return texture(emissivemap3, uv).rgb;
#endif
#if GLTF_INDEXED_CHANNELS > 4
    if (vary_material_index == 4) return texture(emissivemap4, uv).rgb;
#endif
#if GLTF_INDEXED_CHANNELS > 5
    if (vary_material_index == 5) return texture(emissivemap5, uv).rgb;
#endif
#if GLTF_INDEXED_CHANNELS > 6
    if (vary_material_index == 6) return texture(emissivemap6, uv).rgb;
#endif
#if GLTF_INDEXED_CHANNELS > 7
    if (vary_material_index == 7) return texture(emissivemap7, uv).rgb;
#endif
    return vec3(1.0);
}

void main()
{
    int mi = vary_material_index;

    vec4 basecolor = sample_basecolor(base_color_texcoord.xy).rgba;

    if (basecolor.a * vertex_color.a < gltf_minimum_alpha[mi])
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
        basecolor.rgb = sample_basecolor(fx_base_uv).rgb;
    }
    if (fx_rgb_split)
    {
        basecolor.r = sample_basecolor(actorFxRgbSplitUv(fx_base_uv, -1.0)).r;
        basecolor.b = sample_basecolor(actorFxRgbSplitUv(fx_base_uv,  1.0)).b;
    }
#endif

    vec3 emissive = gltf_emissive_color[mi];
#ifdef HAS_ACTOR_FX
    vec2 fx_emissive_uv = emissive_texcoord.xy;
    if (fx_uv_transform)
    {
        fx_emissive_uv = actorFxUv(fx_emissive_uv, vary_position);
    }
    vec3 emissive_texel = sample_emissive(fx_emissive_uv);
    if (fx_rgb_split)
    {
        emissive_texel.r = sample_emissive(actorFxRgbSplitUv(fx_emissive_uv, -1.0)).r;
        emissive_texel.b = sample_emissive(actorFxRgbSplitUv(fx_emissive_uv,  1.0)).b;
    }
    emissive *= srgb_to_linear(emissive_texel);
#else
    emissive *= srgb_to_linear(sample_emissive(emissive_texcoord.xy));
#endif

    float lum = max(max(emissive.r, emissive.g), emissive.b);
    lum *= vertex_emissive.a;
#ifdef HAS_ACTOR_FX
    if (actor_fx_active)
    {
        // Match PBR beauty's Layer/Cover handling of authored emissive before
        // adding the style's independent synthetic bloom contribution.
        vec3 covered_authored = actorFxEmissive(emissive, vec3(0.0));
        lum = max(max(covered_authored.r, covered_authored.g),
                  covered_authored.b) * vertex_emissive.a;

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
        lum += max(max(fx_emissive.r, fx_emissive.g), fx_emissive.b);
    }
#endif

    frag_color.rgb = vec3(0);
    frag_color.a = lum;
}
