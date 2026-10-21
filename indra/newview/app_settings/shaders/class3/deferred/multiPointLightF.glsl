/**
 * @file class3\deferred\multiPointLightF.glsl
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

uniform int relight_enabled;

out vec4 frag_color;

uniform sampler2D     lightFunc;

uniform vec3  env_mat[3];
uniform float sun_wash;
uniform int   light_count;
uniform vec4  light[LIGHT_COUNT];     // .w = size; see C++ fullscreen_lights.push_back()
uniform vec4  light_col[LIGHT_COUNT]; // .a = falloff
// [RigRim] per-light rim params, parallel to light[] / light_col[] (x gain,
// y k, z wrap, w back bias).  Zero for every non-rig light.  Uploaded with
// the same count as MULTI_LIGHT / MULTI_LIGHT_COL from the fullscreen batch.
uniform vec4  rig_rim_lights[LIGHT_COUNT];
uniform vec4 light_shapes[LIGHT_COUNT];
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
RigRimPixel rigRimLoadPixel(vec2 tc, vec3 n_sh, float z_view);
vec3 rigRimTermEx(vec3 n, vec3 v, vec3 l, vec3 c_l, vec3 diffuseColor, float roughness, vec4 rim, RigRimPixel px);

uniform vec2  screen_res;
uniform float far_z;
uniform mat4  inv_proj;
uniform int classic_mode;

in vec4 vary_fragcoord;

void calcHalfVectors(vec3 lv, vec3 n, vec3 v, out vec3 h, out vec3 l, out float nh, out float nl, out float nv, out float vh, out float lightDist);
float calcLegacyDistanceAttenuation(float distance, float falloff);
vec4 getPosition(vec2 pos_screen);
vec4 getNorm(vec2 screenpos);
vec2 getScreenXY(vec4 clip);
vec2 getScreenCoord(vec4 clip);
vec3 srgb_to_linear(vec3 c);

// Util
vec3 hue_to_rgb(float hue);

void pbrPunctual(vec3 diffuseColor, vec3 specularColor,
                    float perceptualRoughness,
                    float metallic,
                    vec3 n, // normal
                    vec3 v, // surface point to camera
                    vec3 l, // surface point to light
                    out float nl,
                    out vec3 diff,
                    out vec3 spec);

GBufferInfo getGBuffer(vec2 screenpos);
#if defined(HAS_EMISSIVE)
bool alrSilDecode(float carrier, float gbuffer_flag, out float k, out float r, out bool haze_skip);
#endif


vec3 relightUnit(vec3 v, vec3 fallback);
float relightRatio(vec3 n, vec3 lv, float radius);
float relightAtten(float d, float range, float falloff, vec4 shape);
vec3 relightRepresentative(vec3 n, vec3 v, vec3 lv, float radius);
vec3 relightPbr(vec3 diffuseColor, vec3 specularColor, float roughness, float metallic,
    vec3 n, vec3 v, vec3 lv, float range, float falloff, vec4 shape, float specKeep);
vec3 relightLegacy(sampler2D glossLut, vec3 diffuse, vec4 spec, vec3 n, vec3 v,
    vec3 lv, float range, float falloff, vec4 shape, out vec3 highlight);
void main()
{
    vec3 final_color = vec3(0, 0, 0);
    vec3 rim_color   = vec3(0, 0, 0); // [RigRim]
    vec2 tc          = getScreenCoord(vary_fragcoord);
    vec3 pos         = getPosition(tc).xyz;
    if (pos.z < far_z)
    {
        discard;
    }

    GBufferInfo gb = getGBuffer(tc);
    bool alr_sil = false;
    float alr_k = 1.0;
    float alr_r = 1.0;
    bool alr_haze_skip = false;
#if defined(HAS_EMISSIVE)
    alr_sil = alrSilDecode(gb.emissive.a, gb.gbufferFlag, alr_k, alr_r, alr_haze_skip);
#endif

    vec3 n = gb.normal;

    // [RigRimQ] quality inputs (see spotLightF.glsl); mode 0 never samples.
    RigRimPixel rim_px = rigRimNeutralPixel(n);
#ifdef RIG_RIM_AUX
    if (rigRimMode() != 0)
    {
        rim_px = rigRimLoadPixel(tc, n, -pos.z);
    }
#endif

    vec4 spec    = gb.specular;
    vec3 diffuse = gb.albedo.rgb;

    vec3  h, l, v = -normalize(pos);
    float nh, nv, vh, lightDist;

    if (GET_GBUFFER_FLAG(gb.gbufferFlag, GBUFFER_FLAG_HAS_PBR))
    {
        vec3 colorEmissive = gb.emissive.rgb;
        vec3 orm = spec.rgb;
        float perceptualRoughness = orm.g;
        float metallic = orm.b;
        vec3 f0 = vec3(0.04);
        vec3 baseColor = diffuse.rgb;

        vec3 diffuseColor = baseColor.rgb*(vec3(1.0)-f0);
        diffuseColor *= 1.0 - metallic;

        vec3 specularColor = mix(f0, baseColor.rgb, metallic);

        for (int light_idx = 0; light_idx < LIGHT_COUNT; ++light_idx)
        {
            if (relight_enabled != 0 && light_shapes[light_idx].x > 0.0)
            {
                vec4 shape = light_shapes[light_idx];
                vec3 delta = light[light_idx].xyz-pos;
                float range = light[light_idx].w;
                float fa = light_col[light_idx].a;
                if (length(delta) >= range) continue;
                vec3 view = relightUnit(-pos,n);
                final_color += light_col[light_idx].rgb * 3.25 * relightPbr(diffuseColor,specularColor,
                    perceptualRoughness,metallic,n,view,delta,range,fa,shape,1.0-gb.envIntensity);
                if (rigRimMode() != 0)
                    rim_color += rigRimTermEx(n,view,relightUnit(delta,n),
                        relightAtten(length(delta),range,fa,shape)*light_col[light_idx].rgb,
                        diffuseColor,perceptualRoughness,rig_rim_lights[light_idx],rim_px);
                continue;
            }

            vec3  lightColor = light_col[ light_idx ].rgb; // Already in linear, see pipeline.cpp: volume->getLightLinearColor();
            float falloff    = light_col[ light_idx ].a;
            float lightSize  = light[ light_idx ].w;
            vec3  lv         = light[ light_idx ].xyz - pos;

            lightDist = length(lv);

            float dist = lightDist / lightSize;
            if (dist <= 1.0)
            {
                lv /= lightDist;

                float dist_atten = calcLegacyDistanceAttenuation(dist, falloff);

                vec3 intensity = dist_atten * lightColor * 3.25;
                float nl = 0;
                vec3 diff = vec3(0);
                vec3 specPunc = vec3(0);
                pbrPunctual(diffuseColor, specularColor, perceptualRoughness, metallic, n.xyz, v, lv, nl, diff, specPunc);
                specPunc *= 1.0 - gb.envIntensity; // [AvatarLightResponse] Tame carrier (0 for every non-avatar PBR pixel)
                final_color += intensity * clamp(nl * (diff + specPunc), vec3(0), vec3(10));

                // [RigRim] PBR branch (omni: no cone, no per-light shadow)
                if (rigRimMode() != 0)
                {
                    rim_color += rigRimTermEx(n.xyz, v, lv, dist_atten * lightColor, diffuseColor, perceptualRoughness, rig_rim_lights[light_idx], rim_px);
                }
            }
        }
    }
    else
    {
        spec.rgb = srgb_to_linear(spec.rgb);

        // As of OSX 10.6.7 ATI Apple's crash when using a variable size loop
        for (int i = 0; i < LIGHT_COUNT; ++i)
        {
            if (relight_enabled != 0 && light_shapes[i].x > 0.0)
            {
                vec4 shape = light_shapes[i];
                vec3 delta = light[i].xyz-pos;
                float range = light[i].w;
                float fa = light_col[i].a;
                if (length(delta) >= range) continue;
                vec3 view = relightUnit(-pos,n);
                vec3 highlight;
                final_color += light_col[i].rgb * relightLegacy(lightFunc,diffuse,spec,
                    n,view,delta,range,fa,shape,highlight);
                if (rigRimMode() != 0)
                    rim_color += rigRimTermEx(n,view,relightUnit(delta,n),
                        relightAtten(length(delta),range,fa,shape)*light_col[i].rgb,
                        diffuse,1.0-spec.a,rig_rim_lights[i],rim_px);
                continue;
            }

            vec3  lv   = light[i].xyz - pos;
            float dist = length(lv);
            dist /= light[i].w;
            if (dist <= 1.0)
            {
                float nl = dot(n, lv);
                if (nl > 0.0)
                {
                    float lightDist;
                    calcHalfVectors(lv, n, v, h, l, nh, nl, nv, vh, lightDist);

                    float fa         = light_col[i].a;
                    float dist_atten = calcLegacyDistanceAttenuation(dist, fa);

                    float lit = nl * dist_atten;

                    vec3 col = light_col[i].rgb * lit * diffuse;

                    if (spec.a > 0.0)
                    {
                        lit        = min(nl * 6.0, 1.0) * dist_atten;
                        float fres = pow(1 - vh, 5) * 0.4 + 0.5;

                        float gtdenom = 2 * nh;
                        float gt      = max(0, min(gtdenom * nv / vh, gtdenom * nl / vh));

                        if (nh > 0.0)
                        {
                            float scol = fres * texture(lightFunc, vec2(nh, spec.a)).r * gt / (nh * nl);
                            col += lit * scol * light_col[i].rgb * spec.rgb;
                        }
                    }

                    final_color += col;
                }

                // [RigRim] legacy branch: evaluated outside the nl > 0 test so
                // wrap can reach just past the terminator.  Own normalize /
                // attenuation because the existing block only computes them
                // for front-facing pixels; roughness from legacy glossiness.
                if (rigRimMode() != 0)
                {
                    vec3  rim_l     = lv / max(length(lv), 1.0e-6);
                    float rim_atten = calcLegacyDistanceAttenuation(dist, light_col[i].a);
                    rim_color += rigRimTermEx(n, v, rim_l, rim_atten * light_col[i].rgb, diffuse, 1.0 - spec.a, rig_rim_lights[i], rim_px);
                }
            }
        }
    }
    // [RigRim] additive on top of the batch's own lighting (mode 0 never enters)
    if (rigRimMode() != 0)
    {
        if (rigRimMode() == 2)
        {
            final_color = vec3(0.0); // debug: rim only
        }
        if (alr_sil)
        {
            final_color = final_color * alr_k + rim_color * alr_r;
        }
        else
        {
            final_color += rim_color;
        }
    }
    else if (alr_sil)
    {
        final_color *= alr_k;
    }
    float final_scale = 1.0;
    if (classic_mode > 0)
        final_scale = 0.9;
    frag_color.rgb = max(final_color * final_scale, vec3(0));
    frag_color.a   = 0.0;

#ifdef IS_AMD_CARD
    // If it's AMD make sure the GLSL compiler sees the arrays referenced once by static index. Otherwise it seems to optimise the storage
    // away which leads to unfun crashes and artifacts.
    vec4 dummy1 = light[0];
    vec4 dummy2 = light_col[0];
    vec4 dummy3 = light[LIGHT_COUNT - 1];
    vec4 dummy4 = light_col[LIGHT_COUNT - 1];
    vec4 dummy5 = rig_rim_lights[0];               // [RigRim]
    vec4 dummy6 = rig_rim_lights[LIGHT_COUNT - 1]; // [RigRim]
#endif
}
