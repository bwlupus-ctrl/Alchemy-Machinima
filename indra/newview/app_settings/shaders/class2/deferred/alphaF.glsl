/**
 * @file alphaF.glsl
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



//class2/deferred/alphaF.glsl

/*[EXTRA_CODE_HERE]*/

uniform int relight_enabled;
#define RELIGHT_PARAM vec4 shape,
#define RELIGHT_FORWARD shape,
#define RELIGHT_DATA(i) light_shapes[i],

#define INDEXED 1
#define NON_INDEXED 2
#define NON_INDEXED_NO_COLOR 3

#if __VERSION__ >= 330
layout(location = 0) out vec4 frag_color;
#else
out vec4 frag_color;
#endif
#ifdef HAS_VISIBLE_DIFFUSE
#if __VERSION__ >= 330
layout(location = 1) out vec4 visible_diffuse;
layout(location = 2) out vec2 surface_coverage;
#else
out vec4 visible_diffuse;
out vec2 surface_coverage;
#endif
#endif

uniform mat3 env_mat;
uniform vec3 sun_dir;
uniform vec3 moon_dir;
uniform int classic_mode;
// Particle ribbons supply a camera-facing normal independently of winding.
uniform int particle_normal;

#ifdef USE_DIFFUSE_TEX
uniform sampler2D diffuseMap;
#endif

in vec3 vary_fragcoord;
in vec3 vary_position;
in vec2 vary_texcoord0;
in vec3 vary_norm;

#ifdef USE_VERTEX_COLOR
in vec4 vertex_color; //vertex color should be treated as sRGB
#endif

uniform float minimum_alpha;

uniform mat4 proj_mat;
uniform mat4 inv_proj;
uniform vec2 screen_res;
uniform int sun_up_factor;
uniform vec4 light_position[8];
uniform vec3 light_direction[8];
uniform vec4 light_attenuation[8];
uniform vec3 light_diffuse[8];
uniform vec4 rig_rim_lights[8]; // [RigRim] per-unit rim params (x gain, y k, z wrap, w back bias), see LLRender::syncLightState()
uniform vec4 light_shapes[8];

void waterClip(vec3 pos);

vec3 srgb_to_linear(vec3 c);
vec3 linear_to_srgb(vec3 c);

// [RigRim] shared rim helper (deferredUtil.glsl)
int rigRimMode();
// [RigRimQ] per-pixel quality inputs (deferredUtil.glsl; struct must match there)
#ifndef RIG_RIM_PIXEL_DEFINED
#define RIG_RIM_PIXEL_DEFINED 1
struct RigRimPixel
{
    vec3  n_geo;
    float n_len;
    float prox;
    float subject;
};
#endif
RigRimPixel rigRimNeutralPixel(vec3 n);
RigRimPixel rigRimForwardPixel(vec3 n_sh, vec3 n_geo, float n_fwidth);
vec3 rigRimTermEx(vec3 n, vec3 v, vec3 l, vec3 c_l, vec3 diffuseColor, float roughness, vec4 rim, RigRimPixel px);

vec4 applySkyAndWaterFog(vec3 pos, vec3 additive, vec3 atten, vec4 color);
#ifdef HAS_ACTOR_FX
vec3 actorFxApply(vec3 source, vec3 normal_eye, vec3 position_eye, vec2 authored_uv);
vec3 actorFxEmissive(vec3 authored_emissive, vec3 styled_color);
void actorFxTronPrime(); // [TronT2 fix] seam footprint, uniform flow
bool actorFxUvTransformEnabled();
bool actorFxRgbSplitEnabled();
vec2 actorFxUv(vec2 authored_uv, vec3 position_eye);
vec2 actorFxRgbSplitUv(vec2 transformed_uv, float direction);
vec3 alrAlbedoLinear(vec3 c);
bool alrSilOn();
float alrSilKeep();
float alrSilRimKeep();
bool alrSilNeedsFogZero();
vec3 alrSilPreFog(vec3 c, float w);
vec3 alrSilPostFog(vec3 c, vec3 fog_zero, float w);
bool alrDebug();
#endif
void calcAtmosphericVarsLinear(vec3 inPositionEye, vec3 norm, vec3 light_dir, out vec3 sunlit, out vec3 amblit, out vec3 atten, out vec3 additive);

#ifdef HAS_SUN_SHADOW
float sampleDirectionalShadow(vec3 pos, vec3 norm, vec2 pos_screen);
#endif

float getAmbientClamp();

void mirrorClip(vec3 pos);

vec3 applyShadowLift(vec3 irradiance, vec3 norm, vec3 light_dir, float scol); // [EnvIntensity v2] deferredUtil.glsl

void sampleReflectionProbesLegacy(inout vec3 ambenv, inout vec3 glossenv, inout vec3 legacyenv,
        vec2 tc, vec3 pos, vec3 norm, float glossiness, float envIntensity, bool transparent, vec3 amblit_linear);

// [RigRim] `rim` = this light unit's rig rim params (rig_rim_lights[i]); zero
// for non-rig lights.  NOTE: `v` is the eye-space POSITION here (legacy naming).

vec3 relightUnit(vec3 v, vec3 fallback);
float calcLegacyDistanceAttenuation(float distance, float falloff);
float relightRatio(vec3 n, vec3 lv, float radius);
float relightAtten(float d, float range, float falloff, vec4 shape);
vec3 relightRepresentative(vec3 n, vec3 v, vec3 lv, float radius);
vec3 relightPbr(vec3 diffuseColor, vec3 specularColor, float roughness, float metallic,
    vec3 n, vec3 v, vec3 lv, float range, float falloff, vec4 shape, float specKeep);
vec3 relightLegacy(sampler2D glossLut, vec3 diffuse, vec4 spec, vec3 n, vec3 v,
    vec3 lv, float range, float falloff, vec4 shape, out vec3 highlight);
vec3 calcPointLightOrSpotLight(vec3 light_col, vec3 diffuse, vec3 v, vec3 n, vec4 lp, vec3 ln, float la, float fa, float is_pointlight, float ambiance, RELIGHT_PARAM vec4 rim,
    RigRimPixel rim_px // [RigRimQ] per-pixel quality inputs
#ifdef HAS_ACTOR_FX
    , bool sil_on, float sil_keep, float rim_keep
#endif
)
{
    if ((relight_enabled & 1) != 0 && (shape.x > 0.0 || shape.y > 0.0))
    {
        // Invert setupHWLights' attenuation encoding to recover shader range R.
        float range = ((12.0*fa)-9.0)/la;
        float falloff = fa-1.0;
        vec3 delta = lp.xyz-v;
        vec3 centre = relightUnit(delta,n);
        vec3 view = relightUnit(-v,n);
        float spot = max(dot(-ln,centre),is_pointlight);
        float attenuation = relightAtten(length(delta),range,falloff,shape);
        vec3 highlight;
        vec3 col = max(spot*spot*attenuation*relightRatio(n,delta,shape.x),0.0)*light_col*diffuse;
#ifdef HAS_ACTOR_FX
        if (sil_on) col *= sil_keep;
#endif
        if (rigRimMode() != 0)
        {
            float rim_atten = attenuation;
            vec3 rim_col = rigRimTermEx(n,view,centre,spot*spot*rim_atten*light_col,
                diffuse,1.0,rim,rim_px);
            if (rigRimMode() == 2) col = vec3(0);
#ifdef HAS_ACTOR_FX
            if (sil_on) rim_col *= rim_keep;
#endif
            col += rim_col;
        }
        return max(col * (classic_mode > 0 ? .9 : 1.0),vec3(0));
    }

    // SL-14895 inverted attenuation work-around
    // This routine is tweaked to match deferred lighting, but previously used an inverted la value. To reconstruct
    // that previous value now that the inversion is corrected, we reverse the calculations in LLPipeline::setupHWLights()
    // to recover the `adjusted_radius` value previously being sent as la.
    float falloff_factor = (12.0 * fa) - 9.0;
    float inverted_la = falloff_factor / la;
    // Yes, it makes me want to cry as well. DJH

    vec3 col = vec3(0);

    //get light vector
    vec3 lv = lp.xyz-v;

    //get distance
    float dist = length(lv);
    float da = 1.0;

    /*if (dist > inverted_la)
    {
        return col;
    }

    clip to projector bounds
     vec4 proj_tc = proj_mat * lp;

    if (proj_tc.z < 0
     || proj_tc.z > 1
     || proj_tc.x < 0
     || proj_tc.x > 1
     || proj_tc.y < 0
     || proj_tc.y > 1)
    {
        return col;
    }*/

    if (dist > 0.0 && inverted_la > 0.0)
    {
        dist /= inverted_la;

        //normalize light vector
        lv = normalize(lv);

        //distance attenuation
        float dist_atten = clamp(1.0-(dist-1.0*(1.0-fa))/fa, 0.0, 1.0);
        dist_atten *= dist_atten;
        dist_atten *= 2.0f;

        if (dist_atten <= 0.0)
        {
           return col;
        }

        // spotlight coefficient.
        float spot = max(dot(-ln, lv), is_pointlight);
        da *= spot*spot; // GL_SPOT_EXPONENT=2

        //angular attenuation
        da *= dot(n, lv);
        da = max(0.0, da);

        float lit = 0.0f;

        float amb_da = 0.0;//ambiance;
        if (da > 0)
        {
            lit = max(da * dist_atten, 0.0);
            col = lit * light_col * diffuse;
            amb_da += (da*0.5+0.5) * ambiance;
        }
        amb_da += (da*da*0.5 + 0.5) * ambiance;
        amb_da *= dist_atten;
        amb_da = min(amb_da, 1.0f - lit);

        // SL-10969 ... need to work out why this blows out in many setups...
        //col.rgb += amb_da * light_col * diffuse;

        // no spec for alpha shader...

#ifdef HAS_ACTOR_FX
        if (sil_on)
        {
            col *= sil_keep;
        }
#endif

        // [RigRim] additive rim (cone + attenuation weighted, no shadow).  No
        // glossiness on this path, so roughness = 1 (softest).  Independent of
        // `da` so wrap can reach past the terminator.  Mode 0 never enters.
        if (rigRimMode() != 0)
        {
            vec3 rim_v   = normalize(-v);
            vec3 rim_col = rigRimTermEx(n, rim_v, lv, (spot * spot) * dist_atten * light_col, diffuse, 1.0, rim, rim_px);
            if (rigRimMode() == 2)
            {
                col = vec3(0.0); // debug: rim only
            }
#ifdef HAS_ACTOR_FX
            if (sil_on)
            {
                col += rim_col * rim_keep;
            }
            else
#endif
            {
                col += rim_col;
            }
        }
    }
    float final_scale = 1.0;
    if (classic_mode > 0)
        final_scale = 0.9;
    col = max(col * final_scale, vec3(0));
    return col;
}

void main()
{
#ifdef HAS_ACTOR_FX
    actorFxTronPrime(); // [TronT2 fix] before any discard/return
#endif
    mirrorClip(vary_position);

    vec2 frag = vary_fragcoord.xy/vary_fragcoord.z*0.5+0.5;

    vec4 pos = vec4(vary_position, 1.0);
#ifndef IS_AVATAR_SKIN
    // clip against water plane unless this is a legacy avatar skin
    waterClip(pos.xyz);
#endif
    vec3 norm = normalize(vary_norm);
    // As in pbralphaF: culling controls whether a back face can reach lighting.
    norm *= (gl_FrontFacing || particle_normal != 0) ? 1.0 : -1.0;

    // [RigRimQ] Toksvig input, unconditional at main scope and BEFORE the alpha
    // discards below (review P2-5); unused unless rig_rim_mode != 0.
    float rim_fw = length(fwidth(norm));

    float shadow = 1.0f;

#ifdef HAS_SUN_SHADOW
    shadow = sampleDirectionalShadow(pos.xyz, norm.xyz, frag);
#endif

#ifdef USE_DIFFUSE_TEX
    vec4 diffuse_tap = texture(diffuseMap,vary_texcoord0.xy);
#endif

#ifdef USE_INDEXED_TEX
    vec4 diffuse_tap = diffuseLookup(vary_texcoord0.xy);
#endif

#ifdef HAS_ACTOR_FX
    vec2 fx_uv = vary_texcoord0.xy;
    bool fx_uv_transform = actorFxUvTransformEnabled();
    bool fx_rgb_split = actorFxRgbSplitEnabled();
    if (fx_uv_transform)
    {
        fx_uv = actorFxUv(fx_uv, vary_position);
#ifdef USE_DIFFUSE_TEX
        diffuse_tap.rgb = texture(diffuseMap, fx_uv).rgb;
#endif
#ifdef USE_INDEXED_TEX
        diffuse_tap.rgb = diffuseLookup(fx_uv).rgb;
#endif
    }
    if (fx_rgb_split)
    {
#ifdef USE_DIFFUSE_TEX
        diffuse_tap.r = texture(diffuseMap, actorFxRgbSplitUv(fx_uv, -1.0)).r;
        diffuse_tap.b = texture(diffuseMap, actorFxRgbSplitUv(fx_uv,  1.0)).b;
#endif
#ifdef USE_INDEXED_TEX
        diffuse_tap.r = diffuseLookup(actorFxRgbSplitUv(fx_uv, -1.0)).r;
        diffuse_tap.b = diffuseLookup(actorFxRgbSplitUv(fx_uv,  1.0)).b;
#endif
    }
#endif

    vec4 diffuse_srgb = diffuse_tap;

#ifdef FOR_IMPOSTOR
    vec4 color;
    color.rgb = diffuse_srgb.rgb;
    color.a = 1.0;

    float final_alpha = diffuse_srgb.a * vertex_color.a;
    diffuse_srgb.rgb *= vertex_color.rgb;

    // Insure we don't pollute depth with invis pixels in impostor rendering
    //
    if (final_alpha < minimum_alpha)
    {
        discard;
    }

    color.rgb = diffuse_srgb.rgb;
    color.a = final_alpha;

#else // FOR_IMPOSTOR

    vec4 diffuse_linear = vec4(srgb_to_linear(diffuse_srgb.rgb), diffuse_srgb.a);

    vec3 light_dir = (sun_up_factor == 1) ? sun_dir: moon_dir; // TODO -- factor out "sun_up_factor" and just send in the appropriate light vector

    float final_alpha = diffuse_linear.a;

#ifdef IS_AVATAR_SKIN
    if(final_alpha < minimum_alpha)
    {
        discard;
    }
#endif

#ifdef USE_VERTEX_COLOR
    final_alpha *= vertex_color.a;

    if (final_alpha < minimum_alpha)
    { // TODO: figure out how to get invisible faces out of
        // render batches without breaking glow
        discard;
    }

    diffuse_srgb.rgb *= vertex_color.rgb;
    diffuse_linear.rgb = srgb_to_linear(diffuse_srgb.rgb);
#endif // USE_VERTEX_COLOR

#ifdef HAS_ACTOR_FX
    diffuse_linear.rgb = actorFxApply(diffuse_linear.rgb, norm, vary_position,
                                      vary_texcoord0.xy);
    // Transparent legacy surfaces have no GBuffer emissive attachment.  Add
    // the styled emission to their forward beauty result; the separate glow
    // sub-pass only publishes bloom metadata and never changes coverage.
    vec3 actor_fx_emissive = actorFxEmissive(vec3(0.0), diffuse_linear.rgb);
    diffuse_linear.rgb = alrAlbedoLinear(diffuse_linear.rgb); // [AvatarLightResponse] presented surface
#endif

    vec3 sunlit;
    vec3 amblit;
    vec3 additive;
    vec3 atten;

    calcAtmosphericVarsLinear(pos.xyz, norm, light_dir, sunlit, amblit, additive, atten);
    if (classic_mode > 0)
        sunlit *= 1.35;
    vec3 sunlit_linear = sunlit;
    vec3 amblit_linear = amblit;

    vec3 irradiance = amblit;
    vec3 glossenv = vec3(0.0);
    vec3 legacyenv = vec3(0.0);
    sampleReflectionProbesLegacy(irradiance, glossenv, legacyenv, frag, pos.xyz, norm.xyz, 0.0, 0.0, true, amblit_linear);
    irradiance = applyShadowLift(irradiance, norm.xyz, light_dir, shadow); // [EnvIntensity v2] no SSAO here; last irradiance-side op

    float da = dot(norm.xyz, light_dir.xyz);
          da = clamp(da, -1.0, 1.0);

    float final_da = da;
          final_da = clamp(final_da, 0.0f, 1.0f);

    vec4 color = vec4(0.0);

    color.a   = final_alpha;

    color.rgb = irradiance;
    if (classic_mode > 0)
    {
        final_da = pow(final_da,1.2);
        vec3 sun_contrib = vec3(min(final_da, shadow));

        color.rgb = srgb_to_linear(color.rgb * 0.9 + linear_to_srgb(sun_contrib) * sunlit_linear * 0.7);
        sunlit_linear = srgb_to_linear(sunlit_linear);
    }
    else
    {
        vec3 sun_contrib = min(final_da, shadow) * sunlit_linear;
        color.rgb += sun_contrib;
    }

    color.rgb *= diffuse_linear.rgb;
#ifdef HAS_ACTOR_FX
    bool alr_sil_on = alrSilOn();
    float alr_sil_keep = alrSilKeep();
    float alr_rim_keep = alrSilRimKeep();
    if (alr_sil_on) color.rgb *= alr_sil_keep;
#endif

    vec4 light = vec4(0,0,0,0);

    // [RigRimQ] forward-alpha local pixel: no normal map on this path, so the
    // vertex normal IS the shading normal; only the Toksvig length carries
    // information.  rim_fw hoisted to main scope; mode 0 keeps neutral.
    RigRimPixel rim_px = rigRimNeutralPixel(norm);
    if (rigRimMode() != 0)
    {
        rim_px = rigRimForwardPixel(norm, norm, rim_fw);
    }

   // [RigRim] trailing rig_rim_lights[i] argument (+ [RigRimQ] rim_px)
#ifdef HAS_ACTOR_FX
   #define LIGHT_LOOP(i) light.rgb += calcPointLightOrSpotLight(light_diffuse[i].rgb, diffuse_linear.rgb, pos.xyz, norm, light_position[i], light_direction[i].xyz, light_attenuation[i].x, light_attenuation[i].y, light_attenuation[i].z, light_attenuation[i].w, RELIGHT_DATA(i) rig_rim_lights[i], rim_px, alr_sil_on, alr_sil_keep, alr_rim_keep);
#else
   #define LIGHT_LOOP(i) light.rgb += calcPointLightOrSpotLight(light_diffuse[i].rgb, diffuse_linear.rgb, pos.xyz, norm, light_position[i], light_direction[i].xyz, light_attenuation[i].x, light_attenuation[i].y, light_attenuation[i].z, light_attenuation[i].w, RELIGHT_DATA(i) rig_rim_lights[i], rim_px);
#endif

    LIGHT_LOOP(1)
    LIGHT_LOOP(2)
    LIGHT_LOOP(3)
    LIGHT_LOOP(4)
    LIGHT_LOOP(5)
    LIGHT_LOOP(6)
    LIGHT_LOOP(7)

    // sum local light contrib in linear colorspace
    color.rgb += light.rgb;
#ifdef HAS_ACTOR_FX
    if (alrSilOn()) actor_fx_emissive *= alr_sil_keep;
    color.rgb += actor_fx_emissive;
    if (alrSilOn())
    {
        float alr_fill_scale = classic_mode > 0 ? 1.0 / 1.1 : 1.0;
        color.rgb = alrSilPreFog(color.rgb, alr_fill_scale);
    }
#endif

    color.rgb = applySkyAndWaterFog(pos.xyz, additive, atten, color).rgb;
#ifdef HAS_ACTOR_FX
    if (alrSilOn() && alrSilNeedsFogZero())
    {
        vec3 alr_fog_zero = applySkyAndWaterFog(
            pos.xyz, additive, atten, vec4(0.0, 0.0, 0.0, 1.0)).rgb;
        float alr_fill_scale = classic_mode > 0 ? 1.0 / 1.1 : 1.0;
        color.rgb = alrSilPostFog(color.rgb, alr_fog_zero, alr_fill_scale);
    }
#endif

#endif // #else // FOR_IMPOSTOR
    float final_scale = 1;
    if (classic_mode > 0)
        final_scale = 1.1;
#ifdef IS_HUD
    color.rgb = linear_to_srgb(color.rgb);
    final_scale = 1;
#endif

    color.rgb *= final_scale;
#ifdef HAS_ACTOR_FX
    if (alrDebug()) color.rgb = vec3(1.0, 0.0, 1.0); // [AvatarLightResponse] debug override
#endif
    frag_color = max(color, vec4(0));
#ifdef HAS_VISIBLE_DIFFUSE
#ifdef FOR_IMPOSTOR
    visible_diffuse = vec4(0.0);
    surface_coverage = vec2(0.0);
#else
    vec3 alr_visible_diffuse = diffuse_linear.rgb;
#ifdef HAS_ACTOR_FX
    if (alrSilOn()) alr_visible_diffuse *= alr_sil_keep;
#endif
    visible_diffuse = vec4(max(alr_visible_diffuse, vec3(0)), final_alpha);
    // R = 0 is written but masked off by the pool; G claims forward coverage
    // proportional to how much of this pixel this layer actually covers.
    surface_coverage = vec2(0.0, final_alpha);
#endif
#endif
}
