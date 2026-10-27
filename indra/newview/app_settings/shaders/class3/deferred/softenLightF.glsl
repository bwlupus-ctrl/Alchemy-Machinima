/**
 * @file class3/deferred/softenLightF.glsl
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

#define FLT_MAX 3.402823466e+38

out vec4 frag_color;

const float M_PI = 3.14159265;

uniform sampler2D lightMap;

uniform sampler2D     lightFunc;

uniform float blur_size;
uniform float blur_fidelity;

#if defined(HAS_SSAO)
uniform float ssao_irradiance_scale;
uniform float ssao_irradiance_max;
#endif

// Inputs
uniform vec4 clipPlane;
uniform mat3 env_mat;
uniform mat3  ssao_effect_mat;
uniform vec3 sun_dir;
uniform vec3 moon_dir;
uniform int  sun_up_factor;
uniform int classic_mode;
uniform int alr_sil_active; // [AvatarSilhouette] frame mask (each compilation unit must declare it)
uniform int render_debug_lighting_term;

in vec2 vary_fragcoord;

uniform mat4 inv_proj;
uniform vec2 screen_res;

vec4 getNorm(vec2 pos_screen);
vec4 getPositionWithDepth(vec2 pos_screen, float depth);

void calcAtmosphericVarsLinear(vec3 inPositionEye, vec3 norm, vec3 light_dir, out vec3 sunlit, out vec3 amblit, out vec3 atten, out vec3 additive);
vec3  atmosFragLightingLinear(vec3 l, vec3 additive, vec3 atten);
vec3  scaleSoftClipFragLinear(vec3 l);

// reflection probe interface
void sampleReflectionProbes(inout vec3 ambenv, inout vec3 glossenv,
    vec2 tc, vec3 pos, vec3 norm, float glossiness, bool transparent, vec3 amblit_linear);
void sampleReflectionProbesLegacy(inout vec3 ambenv, inout vec3 glossenv, inout vec3 legacyenv,
        vec2 tc, vec3 pos, vec3 norm, float glossiness, float envIntensity, bool transparent, vec3 amblit_linear);
void applyGlossEnv(inout vec3 color, vec3 glossenv, vec4 spec, vec3 pos, vec3 norm);
void applyLegacyEnv(inout vec3 color, vec3 legacyenv, vec4 spec, vec3 pos, vec3 norm, float envIntensity);
float getDepth(vec2 pos_screen);

vec3 linear_to_srgb(vec3 c);
vec3 srgb_to_linear(vec3 c);

uniform vec4 waterPlane;

uniform int cube_snapshot;

uniform float sky_hdr_scale;

void calcHalfVectors(vec3 lv, vec3 n, vec3 v, out vec3 h, out vec3 l, out float nh, out float nl, out float nv, out float vh, out float lightDist);
void calcDiffuseSpecular(vec3 baseColor, float metallic, inout vec3 diffuseColor, inout vec3 specularColor);

vec3 pbrBaseLight(vec3 diffuseColor,
                  vec3 specularColor,
                  float metallic,
                  vec3 pos,
                  vec3 norm,
                  float perceptualRoughness,
                  vec3 light_dir,
                  vec3 sunlit,
                  float scol,
                  vec3 radiance,
                  vec3 irradiance,
                  vec3 colorEmissive,
                  float ao,
                  vec3 additive,
                  vec3 atten);

// [AvatarLightResponse] spec_keep variant (deferredUtil.glsl): PBR pixels carry Tame in normal.b (envIntensity)
vec3 pbrBaseLightKeep(vec3 diffuseColor,
                      vec3 specularColor,
                      float metallic,
                      vec3 pos,
                      vec3 norm,
                      float perceptualRoughness,
                      vec3 light_dir,
                      vec3 sunlit,
                      float scol,
                      vec3 radiance,
                      vec3 irradiance,
                      vec3 colorEmissive,
                      float ao,
                      vec3 additive,
                      vec3 atten,
                      float spec_keep);

GBufferInfo getGBuffer(vec2 screenpos);
vec3 clampHDRRange(vec3 color);
#if defined(HAS_EMISSIVE)
bool alrSilDecode(float carrier, float gbuffer_flag, out float k, out float r, out bool haze_skip);
#endif

// [EnvIntensity v2] deferredUtil.glsl: shadow lift (0 gain = untouched).
vec3 applyShadowLift(vec3 irradiance, vec3 norm, vec3 light_dir, float scol);
// [EnvIntensity v2] luminance-normalised ambient tint (vec3(1) = off); the
// SSAO ceiling is an absolute clamp, so it is tinted too or a scalar clamp
// would strip the tint in saturated corners.
uniform vec3 sky_amb_tint;
uniform float ssao_irradiance_max_stock;
bool envOnlyIdentity();

void adjustIrradiance(inout vec3 irradiance, float ambocc)
{
    // use sky settings ambient or irradiance map sample, whichever is brighter
    //irradiance = max(amblit_linear, irradiance);

#if defined(HAS_SSAO)
    vec3 ssao_ceiling = vec3(ssao_irradiance_max);
    if (sky_amb_tint != vec3(1.0)) // [EnvIntensity v2]
    {
        ssao_ceiling *= sky_amb_tint;
    }
    if (envOnlyIdentity()) ssao_ceiling = vec3(ssao_irradiance_max_stock);
    irradiance = mix(ssao_effect_mat * min(irradiance.rgb*ssao_irradiance_scale, ssao_ceiling), irradiance.rgb, ambocc);
#endif
}

void main()
{
    // This is the only uniform comparison paid by mode 0.  All diagnostic
    // evaluation and the output override stay behind the resulting boolean.
    bool debug_lighting = render_debug_lighting_term != 0;
    vec2  tc           = vary_fragcoord.xy;
    float depth        = getDepth(tc.xy);
    vec4  pos          = getPositionWithDepth(tc, depth);

    GBufferInfo gb = getGBuffer(tc);

    vec3 colorEmissive = gb.emissive.rgb;
    bool alr_sil = false;
    float alr_k = 1.0;
    float alr_r = 1.0;
    bool alr_haze_skip = false;
#if defined(HAS_EMISSIVE)
    alr_sil = alrSilDecode(gb.emissive.a, gb.gbufferFlag, alr_k, alr_r, alr_haze_skip);
#if defined(EMISSIVE_SILHOUETTE_ONLY)
    colorEmissive = alr_sil ? gb.emissive.rgb : vec3(0.0);
#endif
#endif
    float envIntensity = gb.envIntensity;
    vec3  light_dir   = (sun_up_factor == 1) ? sun_dir : moon_dir;

    vec4 baseColor     = gb.albedo;
    vec4 spec        = gb.specular; // NOTE: PBR linear Emissive

    vec2 scol_ambocc = vec2(1.0);
#if defined(HAS_SUN_SHADOW) || defined(HAS_SSAO)
    scol_ambocc = texture(lightMap, vary_fragcoord.xy).rg;
#else
    // Keep mode 0 free of a new texture read while allowing diagnostics to be
    // enabled at runtime even if shadows and SSAO were off at shader load.
    if (debug_lighting) scol_ambocc = texture(lightMap, vary_fragcoord.xy).rg;
#endif

#if defined(HAS_SUN_SHADOW)
    float scol       = max(scol_ambocc.r, baseColor.a);
#else
    float scol = 1.0;
#endif
#if defined(HAS_SSAO)
    float ambocc     = scol_ambocc.g;
#else
    float ambocc = 1.0;
#endif
    float debug_scol = max(scol_ambocc.r, baseColor.a);

    vec3  color = vec3(0);
    vec3  debug_ambient = vec3(0.0);
    vec3  debug_direct = vec3(0.0);
    vec3  debug_specular = vec3(0.0);
    float bloom = 0.0;

    vec3 sunlit;
    vec3 amblit;
    vec3 additive;
    vec3 atten;

    calcAtmosphericVarsLinear(pos.xyz, gb.normal, light_dir, sunlit, amblit, additive, atten);

    if (classic_mode > 0)
        sunlit *= 1.35;

    vec3 sunlit_linear = sunlit;
    vec3 amblit_linear = amblit;

    vec3  radiance  = vec3(0);

    if (GET_GBUFFER_FLAG(gb.gbufferFlag, GBUFFER_FLAG_HAS_PBR))
    {
        vec3 orm = spec.rgb;
        float perceptualRoughness = orm.g;
        float metallic = orm.b;
        float ao = orm.r;

        vec3  irradiance = amblit_linear;

        // PBR IBL
        float gloss      = 1.0 - perceptualRoughness;

        sampleReflectionProbes(irradiance, radiance, tc, pos.xyz, gb.normal, gloss, false, amblit_linear);

        adjustIrradiance(irradiance, ambocc);
        irradiance = applyShadowLift(irradiance, gb.normal, light_dir, scol); // [EnvIntensity v2] after the SSAO ceiling

        vec3 diffuseColor = vec3(0.0);
        vec3 specularColor = vec3(0.0);
        calcDiffuseSpecular(baseColor.rgb, metallic, diffuseColor, specularColor);

        vec3 v = -normalize(pos.xyz);
        color = pbrBaseLightKeep(diffuseColor, specularColor, metallic, v, gb.normal, perceptualRoughness, light_dir, sunlit_linear, scol, radiance, irradiance, alr_sil ? vec3(0.0) : colorEmissive, ao, additive, atten, 1.0 - gb.envIntensity); // [AvatarLightResponse] Tame carrier

        if (debug_lighting)
        {
            if (render_debug_lighting_term == 3)
                debug_ambient = pbrBaseLightKeep(diffuseColor, specularColor, metallic, v, gb.normal, perceptualRoughness, light_dir, vec3(0.0), 0.0, vec3(0.0), irradiance, vec3(0.0), ao, vec3(0.0), vec3(1.0), 1.0 - gb.envIntensity);
            else if (render_debug_lighting_term == 4)
                debug_direct = pbrBaseLightKeep(diffuseColor, specularColor, metallic, v, gb.normal, perceptualRoughness, light_dir, sunlit_linear, scol, vec3(0.0), vec3(0.0), vec3(0.0), ao, vec3(0.0), vec3(1.0), 1.0 - gb.envIntensity);
            else if (render_debug_lighting_term == 6)
                debug_specular = pbrBaseLightKeep(diffuseColor, specularColor, metallic, v, gb.normal, perceptualRoughness, light_dir, vec3(0.0), 0.0, radiance, vec3(0.0), vec3(0.0), ao, vec3(0.0), vec3(1.0), 1.0 - gb.envIntensity);
        }
    }
    else if (GET_GBUFFER_FLAG(gb.gbufferFlag, GBUFFER_FLAG_HAS_HDRI))
    {
        // actual HDRI sky, just copy color value
        color = colorEmissive.rgb;
    }
    else if (GET_GBUFFER_FLAG(gb.gbufferFlag, GBUFFER_FLAG_SKIP_ATMOS))
    {
        //should only be true of WL sky, port over base color value and scale for fake HDR
#if defined(HAS_EMISSIVE) && !defined(EMISSIVE_SILHOUETTE_ONLY)
        color = colorEmissive.rgb;
#else
        color = baseColor.rgb;
#endif
        color = srgb_to_linear(color);
        color *= sky_hdr_scale;
    }
    else
    {
        // legacy shaders are still writng sRGB to gbuffer, diffuse is sRGB buffer autodecoded
        // to linear, but specular is not, so convert it here for use in lighting calcs
        spec.rgb = srgb_to_linear(spec.rgb);

        float da = clamp(dot(gb.normal, light_dir.xyz), 0.0, 1.0);

        vec3 irradiance = amblit;
        vec3 glossenv = vec3(0);
        vec3 legacyenv = vec3(0);

        sampleReflectionProbesLegacy(irradiance, glossenv, legacyenv, tc, pos.xyz, gb.normal, spec.a, envIntensity, false, amblit_linear);

        adjustIrradiance(irradiance, ambocc);
        irradiance = applyShadowLift(irradiance, gb.normal, light_dir, scol); // [EnvIntensity v2] gain is 0 in classic mode

        // apply lambertian IBL only (see pbrIbl)
        color.rgb = irradiance;
        if (debug_lighting) debug_ambient = irradiance * baseColor.rgb;
        vec3 ambient_for_direct = irradiance;

        if (classic_mode > 0)
        {
            da = pow(da,1.2);
            vec3 sun_contrib = vec3(min(da, scol));

            color.rgb = srgb_to_linear(color.rgb * 0.9 + (linear_to_srgb(sun_contrib) * sunlit_linear * 0.7));
            ambient_for_direct = srgb_to_linear(irradiance * 0.9);
            sunlit_linear = srgb_to_linear(sunlit_linear);
        }
        else
        {
            vec3 sun_contrib = min(da, scol) * sunlit_linear;
            color.rgb += sun_contrib;
        }

        if (debug_lighting) debug_direct = (color.rgb - ambient_for_direct) * baseColor.rgb;

        color.rgb *= baseColor.rgb;
        vec3 before_direct_spec = color.rgb;

        vec3 refnormpersp = reflect(pos.xyz, gb.normal);

        if (spec.a > 0.0)
        {
            vec3  lv = light_dir.xyz;
            vec3  h, l, v = -normalize(pos.xyz);
            float nh, nl, nv, vh, lightDist;
            vec3 n = gb.normal;
            calcHalfVectors(lv, n, v, h, l, nh, nl, nv, vh, lightDist);

            if (nl > 0.0 && nh > 0.0)
            {
                float lit = min(nl*6.0, 1.0);

                float sa = nh;
                float fres = pow(1 - vh, 5) * 0.4+0.5;
                float gtdenom = 2 * nh;
                float gt = max(0,(min(gtdenom * nv / vh, gtdenom * nl / vh)));

                scol *= fres*texture(lightFunc, vec2(nh, spec.a)).r*gt/(nh*nl);
                color.rgb += lit*scol*sunlit_linear.rgb*spec.rgb;
            }

            if (debug_lighting) debug_direct += color.rgb - before_direct_spec;

            // add radiance map
            vec3 before_reflection = color.rgb;
            applyGlossEnv(color, glossenv, spec, pos.xyz, gb.normal);
            if (debug_lighting) debug_specular += color.rgb - before_reflection;

        }

        color.rgb = mix(color.rgb, baseColor.rgb, baseColor.a);

        if (envIntensity > 0.0)
        {  // add environment map
            vec3 before_reflection = color.rgb;
            applyLegacyEnv(color, legacyenv, spec, pos.xyz, gb.normal, envIntensity);
            if (debug_lighting) debug_specular += color.rgb - before_reflection;
        }
   }

    //color.r = classic_mode > 0 ? 1.0 : 0.0;
    float final_scale = 1;
    if (classic_mode > 0)
        final_scale = 1.1;

    frag_color.rgb = alr_sil
        ? clampHDRRange(color.rgb * final_scale * alr_k + gb.emissive.rgb)
        : clampHDRRange(color.rgb * final_scale); //output linear since local lights will be added to this shader's results
#if defined(HAS_EMISSIVE)
    if (alr_sil && (alr_sil_active & 8) != 0)
    {
        frag_color.rgb = vec3(0.0, 1.0, 1.0) * (0.25 + 0.75 * (1.0 - alr_k));
    }
#endif
    if (debug_lighting)
    {
        vec3 debug_color = vec3(1.0, 0.0, 1.0);
        if (render_debug_lighting_term == 1) debug_color = vec3(debug_scol);
        else if (render_debug_lighting_term == 2) debug_color = texture(lightMap, tc).rgb;
        else if (render_debug_lighting_term == 7) debug_color = vec3(scol_ambocc, 0.0);
        else if (render_debug_lighting_term == 3) debug_color = linear_to_srgb(clamp(debug_ambient, vec3(0.0), vec3(1.0)));
        else if (render_debug_lighting_term == 4) debug_color = linear_to_srgb(clamp(debug_direct, vec3(0.0), vec3(1.0)));
        else if (render_debug_lighting_term == 5) debug_color = vec3(ambocc);
        else if (render_debug_lighting_term == 6) debug_color = linear_to_srgb(clamp(debug_specular, vec3(0.0), vec3(1.0)));
        frag_color.rgb = debug_color; // display-space diagnostic; factors remain numerically exact
    }
    frag_color.a = 0.0;
}
