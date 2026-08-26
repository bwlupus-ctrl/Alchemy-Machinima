/**
 * Shared Actor FX forward-PBR replay fragment shader.
 *
 * One HDR colour output only: the optional visible-diffuse/coverage sidecars
 * intentionally do not participate in shared replay.  Lighting and material
 * evaluation mirror pbralphaF.glsl, while SHARED_ACTOR_FX_ALPHA_MODE gives the
 * replay an explicit GLTF OPAQUE/MASK/BLEND coverage contract.
 */

/*[EXTRA_CODE_HERE]*/

#define SHARED_ACTOR_FX_ALPHA_OPAQUE 0
#define SHARED_ACTOR_FX_ALPHA_MASK   1
#define SHARED_ACTOR_FX_ALPHA_BLEND  2

#ifndef SHARED_ACTOR_FX_ALPHA_MODE
#error SHARED_ACTOR_FX_ALPHA_MODE must be OPAQUE, MASK, or BLEND
#endif

#ifdef SHARED_ACTOR_FX_SLOT_FILTER
// Same four-block sampler layout as native indexed PBR:
// base=s, normal=N+s, ORM=2N+s, emissive=3N+s.
uniform sampler2D basecolor0;
uniform sampler2D normalmap0;
uniform sampler2D ormmap0;
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
uniform float gltf_metallic_factor[GLTF_INDEXED_CHANNELS];
uniform float gltf_roughness_factor[GLTF_INDEXED_CHANNELS];
uniform vec3 gltf_emissive_color[GLTF_INDEXED_CHANNELS];
#if SHARED_ACTOR_FX_ALPHA_MODE == SHARED_ACTOR_FX_ALPHA_MASK
uniform float gltf_minimum_alpha[GLTF_INDEXED_CHANNELS];
#endif
#else
uniform sampler2D diffuseMap;  // GLTF base colour, encoded as sRGB
uniform sampler2D bumpMap;
uniform sampler2D emissiveMap; // GLTF emissive, encoded as sRGB
uniform sampler2D specularMap; // packed linear ORM
uniform float metallicFactor;
uniform float roughnessFactor;
uniform vec3 emissiveColor;    // linear emissive factor * strength
#if SHARED_ACTOR_FX_ALPHA_MODE == SHARED_ACTOR_FX_ALPHA_MASK
uniform float minimum_alpha;
#endif
#endif

// Treatment compositing is intentionally independent of actorFxParams0.x,
// which remains the visual style strength.  Layer dispatch uploads 1 here;
// Cover may upload its independent cinematic fade.
uniform float sharedActorFxOpacity;

#if defined(HAS_SUN_SHADOW) || defined(HAS_SSAO)
uniform sampler2D lightMap;
#endif

uniform int sun_up_factor;
uniform vec3 sun_dir;
uniform vec3 moon_dir;
uniform int classic_mode;

out vec4 frag_color;

in vec3 vary_fragcoord;
in vec3 vary_position;
in vec2 base_color_texcoord;
in vec2 normal_texcoord;
in vec2 metallic_roughness_texcoord;
in vec2 emissive_texcoord;
in vec4 vertex_color;
in vec3 vary_normal;
in vec3 vary_tangent;
flat in float vary_sign;
#ifdef SHARED_ACTOR_FX_SLOT_FILTER
flat in int vary_shared_material_slot;
#endif

#ifdef HAS_SUN_SHADOW
uniform vec2 screen_res;
#endif

// Lights.  LLRender::syncLightState supplies these exactly as it does for the
// native forward PBR alpha shader.
uniform vec4 light_position[8];
uniform vec3 light_direction[8];
uniform vec4 light_attenuation[8];
uniform vec3 light_diffuse[8];
uniform vec2 light_deferred_attenuation[8];

vec3 srgb_to_linear(vec3 c);
void calcAtmosphericVarsLinear(vec3 inPositionEye, vec3 norm,
                               vec3 light_dir, out vec3 sunlit,
                               out vec3 amblit, out vec3 additive,
                               out vec3 atten);
vec4 applySkyAndWaterFog(vec3 pos, vec3 additive, vec3 atten, vec4 color);

#ifdef HAS_ACTOR_FX
vec3 actorFxPbrPreLight(vec3 source);
vec3 actorFxPbrPostLight(vec3 lit_color, vec3 authored_source,
                         vec3 geometry_normal_eye, vec3 position_eye,
                         vec2 authored_uv, float dissolve_coverage,
                         out vec3 synthetic_emission);
vec2 actorFxPbrMaterial(vec2 roughness_metallic);
float actorFxPbrDissolveCoverage();
float actorFxPbrDissolveAlpha(float coverage);
float actorFxPbrNormalAoResponse();
float actorFxPbrAuthoredEmissiveResponse();
vec3 actorFxPbrVhsColor(vec3 source);
bool actorFxUvTransformEnabled();
bool actorFxRgbSplitEnabled();
vec2 actorFxUv(vec2 authored_uv, vec3 position_eye);
vec2 actorFxRgbSplitUv(vec2 transformed_uv, float direction);
#endif

void calcHalfVectors(vec3 lv, vec3 n, vec3 v, out vec3 h, out vec3 l,
                     out float nh, out float nl, out float nv, out float vh,
                     out float lightDist);
float calcLegacyDistanceAttenuation(float distance, float falloff);
float sampleDirectionalShadow(vec3 pos, vec3 norm, vec2 pos_screen);
void sampleReflectionProbes(inout vec3 ambenv, inout vec3 glossenv,
                            vec2 tc, vec3 pos, vec3 norm,
                            float glossiness, bool transparent,
                            vec3 amblit_linear);
void mirrorClip(vec3 pos);
void waterClip(vec3 pos);
void calcDiffuseSpecular(vec3 baseColor, float metallic,
                         inout vec3 diffuseColor,
                         inout vec3 specularColor);

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

vec3 pbrCalcPointLightOrSpotLight(vec3 diffuseColor,
                                  vec3 specularColor,
                                  float perceptualRoughness,
                                  float metallic,
                                  vec3 n,
                                  vec3 p,
                                  vec3 v,
                                  vec3 lp,
                                  vec3 ld,
                                  vec3 lightColor,
                                  float lightSize,
                                  float falloff,
                                  float is_pointlight,
                                  float ambiance);

#ifdef SHARED_ACTOR_FX_SLOT_FILTER
vec4 shared_sample_basecolor(vec2 uv)
{
    if (vary_shared_material_slot == 0) return texture(basecolor0, uv);
#if GLTF_INDEXED_CHANNELS > 1
    if (vary_shared_material_slot == 1) return texture(basecolor1, uv);
#endif
#if GLTF_INDEXED_CHANNELS > 2
    if (vary_shared_material_slot == 2) return texture(basecolor2, uv);
#endif
#if GLTF_INDEXED_CHANNELS > 3
    if (vary_shared_material_slot == 3) return texture(basecolor3, uv);
#endif
#if GLTF_INDEXED_CHANNELS > 4
    if (vary_shared_material_slot == 4) return texture(basecolor4, uv);
#endif
#if GLTF_INDEXED_CHANNELS > 5
    if (vary_shared_material_slot == 5) return texture(basecolor5, uv);
#endif
#if GLTF_INDEXED_CHANNELS > 6
    if (vary_shared_material_slot == 6) return texture(basecolor6, uv);
#endif
#if GLTF_INDEXED_CHANNELS > 7
    if (vary_shared_material_slot == 7) return texture(basecolor7, uv);
#endif
    return vec4(1.0, 0.0, 1.0, 1.0);
}

vec3 shared_sample_normal(vec2 uv)
{
    if (vary_shared_material_slot == 0) return texture(normalmap0, uv).xyz;
#if GLTF_INDEXED_CHANNELS > 1
    if (vary_shared_material_slot == 1) return texture(normalmap1, uv).xyz;
#endif
#if GLTF_INDEXED_CHANNELS > 2
    if (vary_shared_material_slot == 2) return texture(normalmap2, uv).xyz;
#endif
#if GLTF_INDEXED_CHANNELS > 3
    if (vary_shared_material_slot == 3) return texture(normalmap3, uv).xyz;
#endif
#if GLTF_INDEXED_CHANNELS > 4
    if (vary_shared_material_slot == 4) return texture(normalmap4, uv).xyz;
#endif
#if GLTF_INDEXED_CHANNELS > 5
    if (vary_shared_material_slot == 5) return texture(normalmap5, uv).xyz;
#endif
#if GLTF_INDEXED_CHANNELS > 6
    if (vary_shared_material_slot == 6) return texture(normalmap6, uv).xyz;
#endif
#if GLTF_INDEXED_CHANNELS > 7
    if (vary_shared_material_slot == 7) return texture(normalmap7, uv).xyz;
#endif
    return vec3(0.5, 0.5, 1.0);
}

vec3 shared_sample_orm(vec2 uv)
{
    if (vary_shared_material_slot == 0) return texture(ormmap0, uv).rgb;
#if GLTF_INDEXED_CHANNELS > 1
    if (vary_shared_material_slot == 1) return texture(ormmap1, uv).rgb;
#endif
#if GLTF_INDEXED_CHANNELS > 2
    if (vary_shared_material_slot == 2) return texture(ormmap2, uv).rgb;
#endif
#if GLTF_INDEXED_CHANNELS > 3
    if (vary_shared_material_slot == 3) return texture(ormmap3, uv).rgb;
#endif
#if GLTF_INDEXED_CHANNELS > 4
    if (vary_shared_material_slot == 4) return texture(ormmap4, uv).rgb;
#endif
#if GLTF_INDEXED_CHANNELS > 5
    if (vary_shared_material_slot == 5) return texture(ormmap5, uv).rgb;
#endif
#if GLTF_INDEXED_CHANNELS > 6
    if (vary_shared_material_slot == 6) return texture(ormmap6, uv).rgb;
#endif
#if GLTF_INDEXED_CHANNELS > 7
    if (vary_shared_material_slot == 7) return texture(ormmap7, uv).rgb;
#endif
    return vec3(1.0, 1.0, 0.0);
}

vec3 shared_sample_emissive(vec2 uv)
{
    if (vary_shared_material_slot == 0) return texture(emissivemap0, uv).rgb;
#if GLTF_INDEXED_CHANNELS > 1
    if (vary_shared_material_slot == 1) return texture(emissivemap1, uv).rgb;
#endif
#if GLTF_INDEXED_CHANNELS > 2
    if (vary_shared_material_slot == 2) return texture(emissivemap2, uv).rgb;
#endif
#if GLTF_INDEXED_CHANNELS > 3
    if (vary_shared_material_slot == 3) return texture(emissivemap3, uv).rgb;
#endif
#if GLTF_INDEXED_CHANNELS > 4
    if (vary_shared_material_slot == 4) return texture(emissivemap4, uv).rgb;
#endif
#if GLTF_INDEXED_CHANNELS > 5
    if (vary_shared_material_slot == 5) return texture(emissivemap5, uv).rgb;
#endif
#if GLTF_INDEXED_CHANNELS > 6
    if (vary_shared_material_slot == 6) return texture(emissivemap6, uv).rgb;
#endif
#if GLTF_INDEXED_CHANNELS > 7
    if (vary_shared_material_slot == 7) return texture(emissivemap7, uv).rgb;
#endif
    return vec3(1.0);
}

float shared_roughness_factor()
{
    return gltf_roughness_factor[vary_shared_material_slot];
}

float shared_metallic_factor()
{
    return gltf_metallic_factor[vary_shared_material_slot];
}

vec3 shared_emissive_factor()
{
    return gltf_emissive_color[vary_shared_material_slot];
}

#if SHARED_ACTOR_FX_ALPHA_MODE == SHARED_ACTOR_FX_ALPHA_MASK
float shared_minimum_alpha()
{
    return gltf_minimum_alpha[vary_shared_material_slot];
}
#endif
#else
vec4 shared_sample_basecolor(vec2 uv) { return texture(diffuseMap, uv); }
vec3 shared_sample_normal(vec2 uv) { return texture(bumpMap, uv).xyz; }
vec3 shared_sample_orm(vec2 uv) { return texture(specularMap, uv).rgb; }
vec3 shared_sample_emissive(vec2 uv) { return texture(emissiveMap, uv).rgb; }
float shared_roughness_factor() { return roughnessFactor; }
float shared_metallic_factor() { return metallicFactor; }
vec3 shared_emissive_factor() { return emissiveColor; }
#if SHARED_ACTOR_FX_ALPHA_MODE == SHARED_ACTOR_FX_ALPHA_MASK
float shared_minimum_alpha() { return minimum_alpha; }
#endif
#endif

void main()
{
    mirrorClip(vary_position);
    vec3 pos = vary_position;
    waterClip(pos);

    vec4 basecolor = shared_sample_basecolor(base_color_texcoord.xy);
    float authored_alpha = basecolor.a * vertex_color.a;

#if SHARED_ACTOR_FX_ALPHA_MODE == SHARED_ACTOR_FX_ALPHA_MASK
    // GLTF MASK tests the sampled alpha multiplied by baseColorFactor.a.
    // Surviving texels are material-opaque; shared opacity is applied later.
    if (authored_alpha < shared_minimum_alpha())
    {
        discard;
    }
#endif

#ifdef HAS_ACTOR_FX
    // Evaluate the coverage-changing look once.  The same rest-space value is
    // reused by beauty alpha and the incandescent edge, avoiding three FBM
    // evaluations per fragment and keeping MASK/BLEND boundaries identical.
    float actor_fx_dissolve_coverage = actorFxPbrDissolveCoverage();
    if (actor_fx_dissolve_coverage < 0.0)
    {
        discard;
    }
#endif

#ifdef HAS_ACTOR_FX
    bool fx_uv_transform = actorFxUvTransformEnabled();
    bool fx_rgb_split = actorFxRgbSplitEnabled();
    vec2 fx_base_uv = base_color_texcoord.xy;
    if (fx_uv_transform)
    {
        fx_base_uv = actorFxUv(fx_base_uv, vary_position);
        basecolor.rgb = shared_sample_basecolor(fx_base_uv).rgb;
    }
    if (fx_rgb_split)
    {
        basecolor.r = shared_sample_basecolor(
            actorFxRgbSplitUv(fx_base_uv, -1.0)).r;
        basecolor.b = shared_sample_basecolor(
            actorFxRgbSplitUv(fx_base_uv, 1.0)).b;
    }
#endif

    // GLTF baseColorTexture is sRGB; baseColorFactor/vertex_color is linear.
    // Decode first, multiply the factor second.
    basecolor.rgb = srgb_to_linear(basecolor.rgb);
    vec3 col = basecolor.rgb * vertex_color.rgb;
#ifdef HAS_ACTOR_FX
    // Synthetic emission is based on this stable authored sample in beauty and
    // glow alike; it must not depend on the result of PBR lighting.
    vec3 actor_fx_authored_source = col;
#endif

#ifdef HAS_ACTOR_FX
    vec2 fx_normal_uv = normal_texcoord;
    if (fx_uv_transform)
    {
        fx_normal_uv = actorFxUv(fx_normal_uv, vary_position);
    }
    vec3 vNt = shared_sample_normal(fx_normal_uv) * 2.0 - 1.0;
#else
    vec3 vNt = shared_sample_normal(normal_texcoord) * 2.0 - 1.0;
#endif

    vec3 vN = vary_normal;
    vec3 vT = vary_tangent;
    vec3 vB = vary_sign * cross(vN, vT);

#ifdef HAS_ACTOR_FX
    // Style rims use the unperturbed geometric normal.  Material normal maps
    // still feed the BRDF below, but UV seams can no longer split a silhouette
    // rim, sonar sweep, prism edge, or synthetic bloom mask.
    vec3 actor_fx_geometry_normal = vN;
    float actor_fx_geometry_len2 = dot(actor_fx_geometry_normal,
                                       actor_fx_geometry_normal);
    actor_fx_geometry_normal = actor_fx_geometry_len2 > 1e-12
        ? actor_fx_geometry_normal * inversesqrt(actor_fx_geometry_len2)
        : vec3(0.0, 0.0, 1.0);
    actor_fx_geometry_normal *= gl_FrontFacing ? 1.0 : -1.0;
#endif

#ifdef HAS_ACTOR_FX
    float actor_fx_normal_ao_response = actorFxPbrNormalAoResponse();
    vNt = mix(vec3(0.0, 0.0, 1.0), vNt,
              actor_fx_normal_ao_response);
#endif

    vec3 material_normal = vNt.x * vT + vNt.y * vB + vNt.z * vN;
    float material_normal_len2 = dot(material_normal, material_normal);
    float vertex_normal_len2 = dot(vN, vN);
    vec3 material_normal_fallback = vertex_normal_len2 > 1e-12
        ? vN * inversesqrt(vertex_normal_len2) : vec3(0.0, 0.0, 1.0);
    vec3 norm = material_normal_len2 > 1e-12
        ? material_normal * inversesqrt(material_normal_len2)
        : material_normal_fallback;
    norm *= gl_FrontFacing ? 1.0 : -1.0;

    vec3 light_dir = (sun_up_factor == 1) ? sun_dir : moon_dir;
    vec3 sunlit;
    vec3 amblit;
    vec3 additive;
    vec3 atten;
    calcAtmosphericVarsLinear(pos, norm, light_dir,
                              sunlit, amblit, additive, atten);
    if (classic_mode > 0)
    {
        sunlit *= 1.35;
    }
    vec3 sunlit_linear = sunlit;

    float scol = 1.0;
    float frag_w = abs(vary_fragcoord.z) > 1e-6
        ? vary_fragcoord.z : (vary_fragcoord.z < 0.0 ? -1e-6 : 1e-6);
    vec2 frag = vary_fragcoord.xy / frag_w * 0.5 + 0.5;
#ifdef HAS_SUN_SHADOW
    scol = sampleDirectionalShadow(pos, norm, frag);
#endif

#ifdef HAS_ACTOR_FX
    vec2 fx_orm_uv = metallic_roughness_texcoord;
    if (fx_uv_transform)
    {
        fx_orm_uv = actorFxUv(fx_orm_uv, vary_position);
    }
    vec3 orm = shared_sample_orm(fx_orm_uv);
#else
    vec3 orm = shared_sample_orm(metallic_roughness_texcoord);
#endif

    float perceptualRoughness = orm.g * shared_roughness_factor();
    float metallic = orm.b * shared_metallic_factor();
    float ao = orm.r;
#ifdef HAS_ACTOR_FX
    ao = mix(1.0, ao, actor_fx_normal_ao_response);
#endif

    // Sample authored emission independently. Actor FX filters this exact value
    // once, but it does not enter the BRDF result that the post-light creative
    // treatment classifies and modulates.
    vec3 colorEmissive = shared_emissive_factor();
#ifdef HAS_ACTOR_FX
    vec2 fx_emissive_uv = emissive_texcoord;
    if (fx_uv_transform)
    {
        fx_emissive_uv = actorFxUv(fx_emissive_uv, vary_position);
    }
    vec3 emissive_texel = shared_sample_emissive(fx_emissive_uv);
    if (fx_rgb_split)
    {
        emissive_texel.r = shared_sample_emissive(
            actorFxRgbSplitUv(fx_emissive_uv, -1.0)).r;
        emissive_texel.b = shared_sample_emissive(
            actorFxRgbSplitUv(fx_emissive_uv, 1.0)).b;
    }
    colorEmissive *= srgb_to_linear(emissive_texel);
#else
    colorEmissive *= srgb_to_linear(
        shared_sample_emissive(emissive_texcoord));
#endif
#ifdef HAS_ACTOR_FX
    colorEmissive = actorFxPbrVhsColor(colorEmissive);
#endif

#ifdef HAS_ACTOR_FX
    // Only physical replacement materials touch the inputs to the single PBR
    // evaluation. Graphic/sensor looks are applied to lit HDR below.
    col = actorFxPbrPreLight(col);
    vec2 fx_rm = actorFxPbrMaterial(
        vec2(perceptualRoughness, metallic));
    perceptualRoughness = fx_rm.x;
    metallic = fx_rm.y;
    vec3 actor_fx_authored_emission = colorEmissive
        * actorFxPbrAuthoredEmissiveResponse();
#endif

    float gloss = 1.0 - perceptualRoughness;
    vec3 irradiance = amblit;
    vec3 radiance = vec3(0.0);
    // Only GLTF BLEND is a transparent probe consumer. OPAQUE/MASK replay must
    // follow the native opaque probe policy or polished surfaces visibly jump
    // as shared activation takes ownership.
#if SHARED_ACTOR_FX_ALPHA_MODE == SHARED_ACTOR_FX_ALPHA_BLEND
    const bool shared_actor_transparent = true;
#else
    const bool shared_actor_transparent = false;
#endif
    sampleReflectionProbes(irradiance, radiance,
                           vary_position.xy * 0.5 + 0.5,
                           pos, norm, gloss, shared_actor_transparent, amblit);

    vec3 diffuseColor = vec3(0.0);
    vec3 specularColor = vec3(0.0);
    calcDiffuseSpecular(col, metallic, diffuseColor, specularColor);

    vec3 view_eye = -pos;
    float view_len2 = dot(view_eye, view_eye);
    vec3 v = view_len2 > 1e-12
        ? view_eye * inversesqrt(view_len2) : vec3(0.0, 0.0, 1.0);
#ifdef HAS_ACTOR_FX
    // Authored emission is recombined after the creative post-light treatment,
    // so the single PBR evaluation receives no emission to recolour or pulse.
    vec3 pbr_authored_emission = vec3(0.0);
#else
    vec3 pbr_authored_emission = colorEmissive;
#endif
    vec3 color = pbrBaseLight(diffuseColor, specularColor, metallic,
                              v, norm, perceptualRoughness, light_dir,
                              sunlit_linear, scol, radiance, irradiance,
                              pbr_authored_emission, ao, additive, atten);

    vec3 light = vec3(0.0);
#define LIGHT_LOOP(i) light += pbrCalcPointLightOrSpotLight(                 \
        diffuseColor, specularColor, perceptualRoughness, metallic,          \
        norm, pos, v, light_position[i].xyz, light_direction[i].xyz,         \
        light_diffuse[i].rgb, light_deferred_attenuation[i].x,               \
        light_deferred_attenuation[i].y, light_attenuation[i].z,             \
        light_attenuation[i].w);

    LIGHT_LOOP(1)
    LIGHT_LOOP(2)
    LIGHT_LOOP(3)
    LIGHT_LOOP(4)
    LIGHT_LOOP(5)
    LIGHT_LOOP(6)
    LIGHT_LOOP(7)
#undef LIGHT_LOOP

    color += light;
#ifdef HAS_ACTOR_FX
    vec3 actor_fx_synthetic_emission;
    color = actorFxPbrPostLight(color, actor_fx_authored_source,
                                actor_fx_geometry_normal, vary_position,
                                base_color_texcoord,
                                actor_fx_dissolve_coverage,
                                actor_fx_synthetic_emission);
    // Authored and style emission are now independent and each is added once.
    // Both remain inside the normal atmosphere/fog operation below.
    color += actor_fx_authored_emission + actor_fx_synthetic_emission;
#endif
    color = applySkyAndWaterFog(pos, additive, atten,
                                vec4(color, 1.0)).rgb;

    float output_alpha = clamp(sharedActorFxOpacity, 0.0, 1.0);
#if SHARED_ACTOR_FX_ALPHA_MODE == SHARED_ACTOR_FX_ALPHA_BLEND
    output_alpha *= authored_alpha;
#endif
#ifdef HAS_ACTOR_FX
    output_alpha *= actorFxPbrDissolveAlpha(actor_fx_dissolve_coverage);
#endif

    float final_scale = classic_mode > 0 ? 1.1 : 1.0;
    frag_color = max(vec4(color * final_scale, output_alpha), vec4(0.0));
}
