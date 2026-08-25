/**
 * @file pbropaqueIndexedF.glsl
 *
 * $LicenseInfo:firstyear=2022&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2022, Linden Research, Inc.
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

// Indexed (multi-material) PBR opaque GBuffer-write fragment shader.
// vary_material_index selects this primitive's material slot. Each slot s owns a
// dedicated four-sampler block bound by the CPU to texture units
// [s, N+s, 2N+s, 3N+s] (N == GLTF_INDEXED_CHANNELS); scalar factors arrive as
// uniform arrays indexed by the slot. If-chains (not switch) are used for sampler
// selection for driver portability. See pbropaqueF.glsl for the single-material
// equivalent.

out vec4 frag_data[4];

in vec3 vary_position;
in vec4 vertex_color;
in vec3 vary_normal;
in vec3 vary_tangent;
flat in float vary_sign;
flat in int vary_material_index;

in vec2 base_color_texcoord;
in vec2 normal_texcoord;
in vec2 metallic_roughness_texcoord;
in vec2 emissive_texcoord;

uniform float gltf_roughness_factor[GLTF_INDEXED_CHANNELS];
uniform float gltf_metallic_factor[GLTF_INDEXED_CHANNELS];
uniform vec3  gltf_emissive_color[GLTF_INDEXED_CHANNELS];
uniform float gltf_minimum_alpha[GLTF_INDEXED_CHANNELS]; // PBR alphaMode MASK cutoff, -1 for opaque

uniform sampler2D basecolor0;    // always in sRGB space
uniform sampler2D normalmap0;
uniform sampler2D ormmap0;       // Packed: Occlusion, Roughness, Metal
uniform sampler2D emissivemap0;
#if GLTF_INDEXED_CHANNELS > 1
uniform sampler2D basecolor1; uniform sampler2D normalmap1; uniform sampler2D ormmap1; uniform sampler2D emissivemap1;
#endif
#if GLTF_INDEXED_CHANNELS > 2
uniform sampler2D basecolor2; uniform sampler2D normalmap2; uniform sampler2D ormmap2; uniform sampler2D emissivemap2;
#endif
#if GLTF_INDEXED_CHANNELS > 3
uniform sampler2D basecolor3; uniform sampler2D normalmap3; uniform sampler2D ormmap3; uniform sampler2D emissivemap3;
#endif
#if GLTF_INDEXED_CHANNELS > 4
uniform sampler2D basecolor4; uniform sampler2D normalmap4; uniform sampler2D ormmap4; uniform sampler2D emissivemap4;
#endif
#if GLTF_INDEXED_CHANNELS > 5
uniform sampler2D basecolor5; uniform sampler2D normalmap5; uniform sampler2D ormmap5; uniform sampler2D emissivemap5;
#endif
#if GLTF_INDEXED_CHANNELS > 6
uniform sampler2D basecolor6; uniform sampler2D normalmap6; uniform sampler2D ormmap6; uniform sampler2D emissivemap6;
#endif
#if GLTF_INDEXED_CHANNELS > 7
uniform sampler2D basecolor7; uniform sampler2D normalmap7; uniform sampler2D ormmap7; uniform sampler2D emissivemap7;
#endif

vec3 linear_to_srgb(vec3 c);
vec3 srgb_to_linear(vec3 c);

void mirrorClip(vec3 pos);
vec4 encodeNormal(vec3 n, float env, float gbuffer_flag);
#ifdef HAS_ACTOR_FX
vec3 actorFxApply(vec3 source, vec3 normal_eye, vec3 position_eye, vec2 authored_uv);
vec2 actorFxPbrMaterial(vec2 roughness_metallic);
vec3 actorFxEmissive(vec3 authored_emissive, vec3 styled_color);
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

vec3 sample_normal(vec2 uv)
{
    if (vary_material_index == 0) return texture(normalmap0, uv).xyz;
#if GLTF_INDEXED_CHANNELS > 1
    if (vary_material_index == 1) return texture(normalmap1, uv).xyz;
#endif
#if GLTF_INDEXED_CHANNELS > 2
    if (vary_material_index == 2) return texture(normalmap2, uv).xyz;
#endif
#if GLTF_INDEXED_CHANNELS > 3
    if (vary_material_index == 3) return texture(normalmap3, uv).xyz;
#endif
#if GLTF_INDEXED_CHANNELS > 4
    if (vary_material_index == 4) return texture(normalmap4, uv).xyz;
#endif
#if GLTF_INDEXED_CHANNELS > 5
    if (vary_material_index == 5) return texture(normalmap5, uv).xyz;
#endif
#if GLTF_INDEXED_CHANNELS > 6
    if (vary_material_index == 6) return texture(normalmap6, uv).xyz;
#endif
#if GLTF_INDEXED_CHANNELS > 7
    if (vary_material_index == 7) return texture(normalmap7, uv).xyz;
#endif
    return vec3(0.5, 0.5, 1.0);
}

vec3 sample_orm(vec2 uv)
{
    if (vary_material_index == 0) return texture(ormmap0, uv).rgb;
#if GLTF_INDEXED_CHANNELS > 1
    if (vary_material_index == 1) return texture(ormmap1, uv).rgb;
#endif
#if GLTF_INDEXED_CHANNELS > 2
    if (vary_material_index == 2) return texture(ormmap2, uv).rgb;
#endif
#if GLTF_INDEXED_CHANNELS > 3
    if (vary_material_index == 3) return texture(ormmap3, uv).rgb;
#endif
#if GLTF_INDEXED_CHANNELS > 4
    if (vary_material_index == 4) return texture(ormmap4, uv).rgb;
#endif
#if GLTF_INDEXED_CHANNELS > 5
    if (vary_material_index == 5) return texture(ormmap5, uv).rgb;
#endif
#if GLTF_INDEXED_CHANNELS > 6
    if (vary_material_index == 6) return texture(ormmap6, uv).rgb;
#endif
#if GLTF_INDEXED_CHANNELS > 7
    if (vary_material_index == 7) return texture(ormmap7, uv).rgb;
#endif
    return vec3(1.0, 0.0, 0.0);
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
    mirrorClip(vary_position);

    int mi = vary_material_index;

    vec4 basecolor = sample_basecolor(base_color_texcoord.xy).rgba;

    if (basecolor.a * vertex_color.a < gltf_minimum_alpha[mi])
    {
        discard;
    }

#ifdef HAS_ACTOR_FX
    bool fx_uv_transform = actorFxUvTransformEnabled();
    bool fx_rgb_split = actorFxRgbSplitEnabled();
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
    basecolor.rgb = srgb_to_linear(basecolor.rgb);
    basecolor *= vertex_color;

    vec3 col = basecolor.rgb;

    // from mikktspace.com
#ifdef HAS_ACTOR_FX
    vec2 fx_normal_uv = normal_texcoord.xy;
    if (fx_uv_transform)
    {
        fx_normal_uv = actorFxUv(fx_normal_uv, vary_position);
    }
    vec3 vNt = sample_normal(fx_normal_uv) * 2.0 - 1.0;
#else
    vec3 vNt = sample_normal(normal_texcoord.xy) * 2.0 - 1.0;
#endif
    float sign = vary_sign;
    vec3 vN = vary_normal;
    vec3 vT = vary_tangent.xyz;

    vec3 vB = sign * cross(vN, vT);
    vec3 tnorm = normalize( vNt.x * vT + vNt.y * vB + vNt.z * vN );

    // RGB = Occlusion, Roughness, Metal
#ifdef HAS_ACTOR_FX
    vec2 fx_orm_uv = metallic_roughness_texcoord.xy;
    if (fx_uv_transform)
    {
        fx_orm_uv = actorFxUv(fx_orm_uv, vary_position);
    }
    vec3 spec = sample_orm(fx_orm_uv);
#else
    vec3 spec = sample_orm(metallic_roughness_texcoord.xy);
#endif

    spec.g *= gltf_roughness_factor[mi];
    spec.b *= gltf_metallic_factor[mi];

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

    tnorm *= gl_FrontFacing ? 1.0 : -1.0;

#ifdef HAS_ACTOR_FX
    col = actorFxApply(col, tnorm, vary_position, base_color_texcoord.xy);
    vec2 fx_rm = actorFxPbrMaterial(vec2(spec.g, spec.b));
    spec.g = fx_rm.x;
    spec.b = fx_rm.y;
    emissive = actorFxEmissive(emissive, col);
#endif

    // See: C++: addDeferredAttachments(), GLSL: softenLightF
    frag_data[0] = max(vec4(col, 0.0), vec4(0));                 // Diffuse
    frag_data[1] = max(vec4(spec.rgb, 0.0), vec4(0));            // PBR linear packed Occlusion, Roughness, Metal.
    frag_data[2] = encodeNormal(tnorm, 0, GBUFFER_FLAG_HAS_PBR); // normal, environment intensity, flags

#if defined(HAS_EMISSIVE)
    frag_data[3] = max(vec4(emissive, 0), vec4(0));             // PBR sRGB Emissive
#endif
}
