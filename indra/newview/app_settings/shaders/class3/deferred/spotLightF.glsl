/**
 * @file class3\deferred\spotLightF.glsl
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
// [RelightQ] The existing master upload carries shape bit 0 and visibility bit 1.
// Explicit LOD is required in the divergent per-light loops.
uniform sampler2D area_shadow_map0;
uniform sampler2D area_shadow_map1;
float areaShadowVisibility(vec2 uv, float channel)
{
    if ((relight_enabled & 2) == 0 || channel < 0.0) return 1.0;
    int slot = int(channel + 0.5);
    vec4 mask = slot < 4 ? textureLod(area_shadow_map0, uv, 0.0) :
                          textureLod(area_shadow_map1, uv, 0.0);
    return mask[slot & 3];
}


out vec4 frag_color;

uniform samplerCube environmentMap;
uniform sampler2D lightMap;
uniform sampler2D lightFunc;

uniform mat4 proj_mat; //screen space to light space
uniform float proj_near; //near clip for projection
uniform vec3 proj_p; //plane projection is emitting from (in screen space)
uniform vec3 proj_n;
uniform float proj_focus; //distance from plane to begin blurring
uniform float proj_lod;  //(number of mips in proj map)
uniform float proj_range; //range between near clip and far clip plane of projection
uniform float proj_ambient_lod;
uniform float proj_ambiance;
uniform float near_clip;
uniform float far_clip;

uniform vec3 proj_origin; //origin of projection to be used for angular attenuation
uniform float sun_wash;
uniform int proj_shadow_idx;
uniform float shadow_fade;
uniform int classic_mode;

// Light params
#if defined(MULTI_SPOTLIGHT)
uniform vec3 center;
#else
in vec3 trans_center;
#endif
uniform float size;
uniform vec3 color;
uniform float falloff;

in vec4 vary_fragcoord;
uniform vec2 screen_res;

uniform mat4 inv_proj;

void calcHalfVectors(vec3 lv, vec3 n, vec3 v, out vec3 h, out vec3 l, out float nh, out float nl, out float nv, out float vh, out float lightDist);
float calcLegacyDistanceAttenuation(float distance, float falloff);
bool clipProjectedLightVars(vec3 center, vec3 pos, out float dist, out float l_dist, out vec3 lv, out vec4 proj_tc );
vec4 getNorm(vec2 screenpos);
vec3 getProjectedLightAmbiance(float amb_da, float attenuation, float lit, float nl, float noise, vec2 projected_uv);
vec3 getProjectedLightDiffuseColor(float light_distance, vec2 projected_uv );
vec2 getScreenCoord(vec4 clip);
vec3 srgb_to_linear(vec3 cs);
vec4 texture2DLodSpecular(vec2 tc, float lod);

vec4 getPosition(vec2 pos_screen);

const float M_PI = 3.14159265;

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

// [BDMerge NSpot] direct per-projector shadow sampling (shadowUtil.glsl)
float sampleSpotShadow(vec3 pos, vec3 norm, int index, vec2 pos_screen);

// [RigRim] per-projector rim params (x gain, y k, z wrap, w back bias); the
// globals + helper live in deferredUtil.glsl.  Uploaded by setupSpotLight();
// zero for every non-rig projector, so only rig fixtures ever add rim.
uniform vec4 rig_rim_light;
uniform vec4 light_shape;
// [RigRim] Rim shadow: 1 = rim shadowed exactly like the light (default, physical).
// Lower values relax the shadow ONLY near/behind the terminator (n.l < 0.3), where
// shadow-map self-shadowing erases the wrap zone that a light directly behind the
// subject depends on; light-facing surfaces (n.l >= 0.3) stay fully shadowed.
uniform float rig_rim_shadow;
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


vec3 relightUnit(vec3 v, vec3 fallback);
float relightRatio(vec3 n, vec3 lv, float radius);
float relightAtten(float d, float range, float falloff, vec4 shape);
vec3 relightRepresentative(vec3 n, vec3 v, vec3 lv, float radius);
vec3 relightPbr(vec3 diffuseColor, vec3 specularColor, float roughness, float metallic,
    vec3 n, vec3 v, vec3 lv, float range, float falloff, vec4 shape, float specKeep);
vec3 relightLegacy(sampler2D glossLut, vec3 diffuse, vec4 spec, vec3 n, vec3 v,
    vec3 lv, float range, float falloff, vec4 shape, out vec3 highlight);
// Projection rays originate at proj_origin; the radial window must retain the
// legacy light-centre distance from clipProjectedLightVars, in BOTH modes.
vec3 relightPbr(vec3 diffuseColor, vec3 specularColor, float roughness, float metallic,
    vec3 n, vec3 v, vec3 lv, float range, float falloff, vec4 shape, float specKeep, float attenDistance);
vec3 relightLegacy(sampler2D glossLut, vec3 diffuse, vec4 spec, vec3 n, vec3 v,
    vec3 lv, float range, float falloff, vec4 shape, out vec3 highlight, float attenDistance);

void main()
{
    vec3 final_color = vec3(0,0,0);
    vec3 rim_color   = vec3(0,0,0); // [RigRim]
    vec2 tc          = getScreenCoord(vary_fragcoord);
    vec3 pos         = getPosition(tc).xyz;

    vec3 lv;
    vec4 proj_tc;
    float dist, l_dist;
    vec3 c;
#if defined(MULTI_SPOTLIGHT)
    c = center;
#else
    c = trans_center;
#endif

    if (clipProjectedLightVars(c, pos, dist, l_dist, lv, proj_tc))
    {
        discard;
    }

    float shadow = 1.0;

    if (proj_shadow_idx >= 0)
    {
        // [BDMerge NSpot] Sample THIS projector's own shadow map directly via
        // shadowUtil's indexed dispatch (shadowMap4-13 / shadow_matrix[4+idx]),
        // instead of reading a pre-resolved channel of the packed screen-space
        // shadow buffer. The old packed path (shd.b / shd.a) had only 2 free
        // channels, hard-capping projector shadows at 2 (raising the slider
        // gave extra lights with no shadow, or blacked out the scene). The
        // shadow maps + matrices for all BDMergeMaxSpotShadows slots are
        // already bound by bindShadowMaps()/bindDeferredShader(), and
        // proj_shadow_idx already carries this light's slot (0..N-1).
        vec3 spot_norm = getNorm(tc).xyz;
        shadow = sampleSpotShadow(pos, spot_norm, proj_shadow_idx, tc);
        shadow += shadow_fade;
        shadow = clamp(shadow, 0.0, 1.0);
    }

    if ((relight_enabled & 2) != 0 && light_shape.w >= 0.0)
        shadow = min(shadow, areaShadowVisibility(tc, light_shape.w));

    GBufferInfo gb = getGBuffer(tc);
    bool alr_sil = false;
    float alr_k = 1.0;
    float alr_r = 1.0;
    bool alr_haze_skip = false;
#if defined(HAS_EMISSIVE)
    alr_sil = alrSilDecode(gb.emissive.a, gb.gbufferFlag, alr_k, alr_r, alr_haze_skip);
#endif

    vec3 n = gb.normal;

    float dist_atten = calcLegacyDistanceAttenuation(dist, falloff);
    if ((relight_enabled & 1) == 0 || (light_shape.x == 0.0 && light_shape.y == 0.0))
    if (dist_atten <= 0.0)
    {
        discard;
    }

    // [RigRimQ] quality inputs: aux / silhouette maps on RIG_RIM_AUX programs
    // (main view only; the host zeroes rig_rim_quality elsewhere), neutral
    // otherwise.  Mode 0 never samples anything.  Loaded AFTER the cone /
    // attenuation discards above so clipped fragments pay nothing (review
    // P2-8); the loader uses textureLod so the post-discard flow is safe.
    RigRimPixel rim_px = rigRimNeutralPixel(n);
#ifdef RIG_RIM_AUX
    if (rigRimMode() != 0)
    {
        rim_px = rigRimLoadPixel(tc, n, -pos.z);
    }
#endif

    lv = proj_origin-pos.xyz;
    vec3  h, l, v = -normalize(pos);
    float nh, nl, nv, vh, lightDist;
    if ((relight_enabled & 1) == 0 || (light_shape.x == 0.0 && light_shape.y == 0.0))
    calcHalfVectors(lv, n, v, h, l, nh, nl, nv, vh, lightDist);

    vec3 diffuse = gb.albedo.rgb;
    vec4 spec    = gb.specular;
    vec3 dlit    = vec3(0, 0, 0);
    vec3 slit    = vec3(0, 0, 0);

    vec3 amb_rgb = vec3(0);

    if ((relight_enabled & 1) != 0 && (light_shape.x > 0.0 || light_shape.y > 0.0))
    {
        // Cookie/cone and the existing projector shadow still gate this light.
        if (proj_tc.x > 0.0 && proj_tc.x < 1.0 && proj_tc.y > 0.0 && proj_tc.y < 1.0)
        {
            vec3 delta = proj_origin-pos;
            vec3 view = relightUnit(-pos,n);
            vec3 centre = relightUnit(delta,n);
            vec3 lamp = getProjectedLightDiffuseColor(l_dist,proj_tc.xy);
            vec3 diffuseColor = diffuse;
            float roughness = 1.0-spec.a;
            float attenuation = relightAtten(dist*size,size,falloff,light_shape);
            if (GET_GBUFFER_FLAG(gb.gbufferFlag, GBUFFER_FLAG_HAS_PBR))
            {
                roughness = spec.g;
                diffuseColor = diffuse * .96 * (1.0-spec.b);
                final_color = lamp * shadow * 3.25 * relightPbr(diffuseColor,
                    mix(vec3(.04),diffuse,spec.b),roughness,spec.b,n,view,delta,size,falloff,
                    light_shape,1.0-gb.envIntensity,dist*size);
                float centre_nl = max(dot(n,centre),.001);
                vec3 ambient = getProjectedLightAmbiance((centre_nl*.5+.5)*proj_ambiance,
                    attenuation,0.0,centre_nl,1.0,proj_tc.xy) * 3.25;
                if (attenuation > 0.0)
                    final_color += ambient * relightPbr(diffuseColor,mix(vec3(.04),diffuse,spec.b),
                        roughness,spec.b,n,view,delta,size,falloff,light_shape,1.0-gb.envIntensity,dist*size) / attenuation;
            }
            else
            {
                vec3 highlight;
                relightLegacy(lightFunc,diffuse,
                    vec4(srgb_to_linear(spec.rgb),spec.a),n,view,delta,size,falloff,light_shape,highlight,dist*size);
                float wrap = relightRatio(n,delta,light_shape.x);
                // Match the projector's original per-light specular clamp.
                final_color = lamp*shadow*diffuse*wrap*attenuation +
                    clamp(lamp*shadow*highlight,vec3(0),vec3(1));
                vec3 ambient = getProjectedLightAmbiance((wrap*.5+.5)*proj_ambiance,
                    attenuation,clamp(wrap*attenuation,0.0,1.0),wrap,1.0,proj_tc.xy);
                final_color += diffuse*ambient*max(dot(-centre,n),0.0);
            }
            if (rigRimMode() != 0)
            {
                vec3 rim_lit = lamp * attenuation * mix(shadow,1.0,
                    (1.0-clamp(rig_rim_shadow,0.0,1.0))*(1.0-smoothstep(0.0,.3,dot(n,centre))));
                rim_color += rigRimTermEx(n,view,centre,rim_lit,diffuseColor,roughness,rig_rim_light,rim_px);
            }
        }
        // relightLegacy already integrates the source highlight. The separate
        // near-plane projector image ignores source size and material width,
        // adding a hard bright dot even to a low-shiny, unmapped prim.
    }
    else
    {
    if (GET_GBUFFER_FLAG(gb.gbufferFlag, GBUFFER_FLAG_HAS_PBR))
    {
        vec3 orm = spec.rgb;
        float perceptualRoughness = orm.g;
        float metallic = orm.b;
        vec3 f0 = vec3(0.04);
        vec3 baseColor = diffuse.rgb;

        vec3 diffuseColor = baseColor.rgb*(vec3(1.0)-f0);
        diffuseColor *= 1.0 - metallic;

        vec3 specularColor = mix(f0, baseColor.rgb, metallic);
        vec3 diffPunc = vec3(0);
        vec3 specPunc = vec3(0);

        // We need this additional test inside a light's frustum since a spotlight's ambiance can be applied
        if (proj_tc.x > 0.0 && proj_tc.x < 1.0
        &&  proj_tc.y > 0.0 && proj_tc.y < 1.0)
        {
            float lit = 0.0;
            float amb_da = 0.0;

            lv = normalize(lv);

            if (nl > 0.0)
            {
                amb_da += (nl*0.5 + 0.5) * proj_ambiance;

                dlit = getProjectedLightDiffuseColor( l_dist, proj_tc.xy );

                vec3 intensity = dist_atten * dlit * 3.25 * shadow; // Legacy attenuation, magic number to balance with legacy materials

                pbrPunctual(diffuseColor, specularColor, perceptualRoughness, metallic, n.xyz, v, normalize(lv), nl, diffPunc, specPunc);
                specPunc *= 1.0 - gb.envIntensity; // [AvatarLightResponse] Tame carrier (0 for every non-avatar PBR pixel)

                final_color += intensity * clamp(nl * (diffPunc + specPunc), vec3(0), vec3(10));
            }

            amb_rgb = getProjectedLightAmbiance( amb_da, dist_atten, lit, nl, 1.0, proj_tc.xy ) * 3.25; //magic number to balance with legacy ambiance
            pbrPunctual(diffuseColor, specularColor, perceptualRoughness, metallic, n.xyz, v, normalize(lv), nl, diffPunc, specPunc);
            specPunc *= 1.0 - gb.envIntensity; // [AvatarLightResponse] Tame carrier (0 for every non-avatar PBR pixel)

            final_color += amb_rgb * clamp(nl * (diffPunc + specPunc), vec3(0), vec3(10));

            // [RigRim] PBR branch: rim weighted by this projector's cookie/cone
            // (getProjectedLightDiffuseColor == colour x gobo x edge fade),
            // distance attenuation and its own shadow.  Sampled OUTSIDE the
            // nl > 0 test so wrap can reach just past the terminator; the
            // 3.25 balance magic is deliberately omitted (see deferredUtil).
            if (rigRimMode() != 0)
            {
                vec3 rim_lit = getProjectedLightDiffuseColor( l_dist, proj_tc.xy ) * dist_atten
                             * mix(shadow, 1.0, (1.0 - clamp(rig_rim_shadow, 0.0, 1.0)) * (1.0 - smoothstep(0.0, 0.3, dot(n.xyz, l))));
                rim_color += rigRimTermEx(n.xyz, v, l, rim_lit, diffuseColor, perceptualRoughness, rig_rim_light, rim_px);
            }
        }
    }
    else
    {
        float envIntensity = gb.envIntensity;
        spec.rgb = srgb_to_linear(spec.rgb);

        if (proj_tc.z > 0.0 &&
            proj_tc.x < 1.0 &&
            proj_tc.y < 1.0 &&
            proj_tc.x > 0.0 &&
            proj_tc.y > 0.0)
        {
            float amb_da = 0;
            float lit = 0.0;

            if (nl > 0.0)
            {
                lit = nl * dist_atten;

                dlit = getProjectedLightDiffuseColor( l_dist, proj_tc.xy );

                final_color = dlit*lit*diffuse*shadow;

                // unshadowed for consistency between forward and deferred?
                amb_da += (nl*0.5+0.5) /* * (1.0-shadow) */ * proj_ambiance;
            }

            amb_rgb = getProjectedLightAmbiance( amb_da, dist_atten, lit, nl, 1.0, proj_tc.xy );
            final_color += diffuse.rgb * amb_rgb * max(dot(-normalize(lv), n), 0.0);

            // [RigRim] legacy (Blinn-Phong) branch: same weighting as the PBR
            // branch; roughness derived from legacy glossiness (spec.a).
            if (rigRimMode() != 0)
            {
                vec3 rim_lit = getProjectedLightDiffuseColor( l_dist, proj_tc.xy ) * dist_atten
                             * mix(shadow, 1.0, (1.0 - clamp(rig_rim_shadow, 0.0, 1.0)) * (1.0 - smoothstep(0.0, 0.3, dot(n, l))));
                rim_color += rigRimTermEx(n, v, l, rim_lit, diffuse.rgb, 1.0 - spec.a, rig_rim_light, rim_px);
            }
        }

        if (spec.a > 0.0)
        {
            dlit *= min(nl*6.0, 1.0) * dist_atten;

            float fres = pow(1 - vh, 5)*0.4+0.5;

            float gtdenom = 2 * nh;
            float gt = max(0, min(gtdenom * nv / vh, gtdenom * nl / vh));

            if (nh > 0.0)
            {
                float scol = fres*texture(lightFunc, vec2(nh, spec.a)).r*gt/(nh*nl);
                vec3 speccol = dlit*scol*spec.rgb*shadow;
                speccol = clamp(speccol, vec3(0), vec3(1));
                final_color += speccol;
            }
        }

        // Apply the same policy at zero radius / zero softness: the BRDF tends
        // continuously to a point highlight without restoring the emitter dot.
        // The disabled feature retains the original projector reflection.
        if ((relight_enabled & 1) == 0 && envIntensity > 0.0)
        {
            vec3 ref = reflect(normalize(pos), n);

            //project from point pos in direction ref to plane proj_p, proj_n
            vec3 pdelta = proj_p-pos;
            float ds = dot(ref, proj_n);

            if (ds < 0.0)
            {
                vec3 pfinal = pos + ref * dot(pdelta, proj_n)/ds;

                vec4 stc = (proj_mat * vec4(pfinal.xyz, 1.0));

                if (stc.z > 0.0)
                {
                    stc /= stc.w;

                    if (stc.x < 1.0 &&
                        stc.y < 1.0 &&
                        stc.x > 0.0 &&
                        stc.y > 0.0)
                    {
                        final_color += color.rgb * texture2DLodSpecular(stc.xy, (1 - spec.a) * (proj_lod * 0.6)).rgb * shadow * envIntensity;
                    }
                }
            }
        }
    }

    }
    //not sure why, but this line prevents MATBUG-194
    final_color = max(final_color, vec3(0.0));
    // [RigRim] additive on top of the projector's own lighting; the classic_mode
    // final_scale below applies to both so the balance is preserved.  Mode 0
    // never reaches this block (exact no-op).
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
    //output linear
    frag_color.rgb = final_color * final_scale;
    frag_color.a = 0.0;
}
