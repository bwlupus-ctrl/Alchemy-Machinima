/**
 * @file pbropaqueF.glsl
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


#ifndef IS_HUD

// deferred opaque implementation

uniform sampler2D diffuseMap;  //always in sRGB space

uniform float metallicFactor;
uniform float roughnessFactor;
uniform vec3 emissiveColor;
uniform sampler2D bumpMap;
uniform sampler2D emissiveMap;
uniform sampler2D specularMap; // Packed: Occlusion, Metal, Roughness

out vec4 frag_data[4];

in vec3 vary_position;
in vec4 vertex_color;
in vec3 vary_normal;
in vec3 vary_tangent;
flat in float vary_sign;

in vec2 base_color_texcoord;
in vec2 normal_texcoord;
in vec2 metallic_roughness_texcoord;
in vec2 emissive_texcoord;

uniform float minimum_alpha; // PBR alphaMode: MASK, See: mAlphaCutoff, setAlphaCutoff()

vec3 linear_to_srgb(vec3 c);
vec3 srgb_to_linear(vec3 c);

uniform vec4 clipPlane;
uniform float clipSign;

void mirrorClip(vec3 pos);
vec4 encodeNormal(vec3 n, float env, float gbuffer_flag);
float gbufferAvatarFlag(float flag);   // [TronA0] globalF.glsl
#ifdef HAS_ACTOR_FX
vec3 actorFxApply(vec3 source, vec3 normal_eye, vec3 position_eye, vec2 authored_uv);
vec2 actorFxPbrMaterial(vec2 roughness_metallic);
vec3 actorFxEmissive(vec3 authored_emissive, vec3 styled_color);
void actorFxTronPrime(); // [TronT2 fix] seam footprint, uniform flow
float actorFxAuthoredMaterialResponse();
bool actorFxUvTransformEnabled();
bool actorFxRgbSplitEnabled();
vec2 actorFxUv(vec2 authored_uv, vec3 position_eye);
vec2 actorFxRgbSplitUv(vec2 transformed_uv, float direction);
vec3 alrEmissive(vec3 e);
vec3 alrAlbedoLinear(vec3 c);
float alrRoughness(float r);
float alrPbrCarrier();
vec3 alrDebugEmission(vec3 e);
#endif

uniform mat3 normal_matrix;

void main()
{
#ifdef HAS_ACTOR_FX
    actorFxTronPrime(); // [TronT2 fix] before any discard/return
#endif
    mirrorClip(vary_position);

    vec4 basecolor = texture(diffuseMap, base_color_texcoord.xy).rgba;

    if (basecolor.a * vertex_color.a < minimum_alpha)
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
        basecolor.rgb = texture(diffuseMap, fx_base_uv).rgb;
    }
    if (fx_rgb_split)
    {
        basecolor.r = texture(diffuseMap, actorFxRgbSplitUv(fx_base_uv, -1.0)).r;
        basecolor.b = texture(diffuseMap, actorFxRgbSplitUv(fx_base_uv,  1.0)).b;
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
    vec3 vNt = texture(bumpMap, fx_normal_uv).xyz*2.0-1.0;
#else
    vec3 vNt = texture(bumpMap, normal_texcoord.xy).xyz*2.0-1.0;
#endif
    float sign = vary_sign;
    vec3 vN = vary_normal;
    vec3 vT = vary_tangent.xyz;

    vec3 vB = sign * cross(vN, vT);
#ifdef HAS_ACTOR_FX
    float actor_fx_material_response = actorFxAuthoredMaterialResponse();
    // Material-owning Cover looks keep the geometric surface but do not inherit
    // authored tangent-space bumps. Layer and Clone return exactly 1 here.
    vNt = mix(vec3(0.0, 0.0, 1.0), vNt,
              actor_fx_material_response);
#endif
    vec3 tnorm = normalize( vNt.x * vT + vNt.y * vB + vNt.z * vN );

    // RGB = Occlusion, Roughness, Metal
    // default values, see LLViewerTexture::sDefaultPBRORMImagep
    //   occlusion 1.0
    //   roughness 0.0
    //   metal     0.0
#ifdef HAS_ACTOR_FX
    vec2 fx_orm_uv = metallic_roughness_texcoord.xy;
    if (fx_uv_transform)
    {
        fx_orm_uv = actorFxUv(fx_orm_uv, vary_position);
    }
    vec3 spec = texture(specularMap, fx_orm_uv).rgb;
#else
    vec3 spec = texture(specularMap, metallic_roughness_texcoord.xy).rgb;
#endif

    spec.g *= roughnessFactor;
    spec.b *= metallicFactor;
#ifdef HAS_ACTOR_FX
    // Neutral occlusion prevents the old material's cavities/noise from
    // modulating a flat/sensor/owned-material Cover treatment.
    spec.r = mix(1.0, spec.r, actor_fx_material_response);
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

    tnorm *= gl_FrontFacing ? 1.0 : -1.0;

#ifdef HAS_ACTOR_FX
    emissive = alrEmissive(emissive); // [AvatarLightResponse] authored glow, before the look emissive
    col = actorFxApply(col, tnorm, vary_position, base_color_texcoord.xy);
    vec2 fx_rm = actorFxPbrMaterial(vec2(spec.g, spec.b));
    spec.g = fx_rm.x;
    spec.b = fx_rm.y;
    emissive = actorFxEmissive(emissive, col);
    col = alrAlbedoLinear(col); spec.g = alrRoughness(spec.g); // [AvatarLightResponse] presented surface
#endif

    //spec.rgb = vec3(1,1,0);
    //col = vec3(0,0,0);
    //emissive = vary_tangent.xyz*0.5+0.5;
    //emissive = vec3(sign*0.5+0.5);
    //emissive = vNt * 0.5 + 0.5;
    //emissive = tnorm*0.5+0.5;
    float alr_env_carrier = 0.0; // [AvatarLightResponse] PBR pixels carry Tame in normal.b (legacy-only field)
#ifdef HAS_ACTOR_FX
    alr_env_carrier = alrPbrCarrier();
#endif
    // See: C++: addDeferredAttachments(), GLSL: softenLightF
    frag_data[0] = max(vec4(col, 0.0), vec4(0));                                                   // Diffuse
    frag_data[1] = max(vec4(spec.rgb,0.0), vec4(0));                                    // PBR linear packed Occlusion, Roughness, Metal.
#ifdef HAS_SKIN
    // [TronA0] rigged mesh: avatar geometry -> tagged flag
    frag_data[2] = encodeNormal(tnorm, alr_env_carrier, gbufferAvatarFlag(GBUFFER_FLAG_HAS_PBR)); // normal, environment intensity, flags
#else
    frag_data[2] = encodeNormal(tnorm, alr_env_carrier, GBUFFER_FLAG_HAS_PBR); // normal, environment intensity, flags
#endif

#if defined(HAS_EMISSIVE)
    frag_data[3] = max(vec4(emissive,0), vec4(0));                                                // PBR sRGB Emissive
#ifdef HAS_ACTOR_FX
    frag_data[3].rgb = alrDebugEmission(frag_data[3].rgb); // [AvatarLightResponse] debug REPLACES emission
#endif
#endif
}

#else

// forward fullbright implementation for HUDs

uniform sampler2D diffuseMap;  //always in sRGB space

uniform vec3 emissiveColor;
uniform sampler2D emissiveMap;

out vec4 frag_color;

in vec3 vary_position;
in vec4 vertex_color;

in vec2 base_color_texcoord;
in vec2 emissive_texcoord;

uniform float minimum_alpha; // PBR alphaMode: MASK, See: mAlphaCutoff, setAlphaCutoff()

vec3 linear_to_srgb(vec3 c);
vec3 srgb_to_linear(vec3 c);

void main()
{
    vec4 basecolor = texture(diffuseMap, base_color_texcoord.xy).rgba;

    basecolor.a *= vertex_color.a;

    if (basecolor.a < minimum_alpha)
    {
        discard;
    }

    vec3 col = vertex_color.rgb * srgb_to_linear(basecolor.rgb);

    vec3 emissive = emissiveColor;
    emissive *= srgb_to_linear(texture(emissiveMap, emissive_texcoord.xy).rgb);

    col += emissive;

    // HUDs are rendered after gamma correction, output in sRGB space
    frag_color.rgb = linear_to_srgb(col);
    frag_color.a = 0.0;
}

#endif
