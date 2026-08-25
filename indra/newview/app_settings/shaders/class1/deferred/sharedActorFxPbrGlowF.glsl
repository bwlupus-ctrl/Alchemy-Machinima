/**
 * Shared Actor FX PBR glow replay fragment shader.
 *
 * The RGB destination is preserved by the glow blend state; this shader writes
 * HDR bloom energy to alpha.  Authored and synthetic Actor FX emission are
 * evaluated for every look and share the beauty pass's alpha/slot contract.
 */

/*[EXTRA_CODE_HERE]*/

#define SHARED_ACTOR_FX_ALPHA_OPAQUE 0
#define SHARED_ACTOR_FX_ALPHA_MASK   1
#define SHARED_ACTOR_FX_ALPHA_BLEND  2

#ifndef SHARED_ACTOR_FX_ALPHA_MODE
#error SHARED_ACTOR_FX_ALPHA_MODE must be OPAQUE, MASK, or BLEND
#endif

#ifdef SHARED_ACTOR_FX_SLOT_FILTER
uniform sampler2D basecolor0; uniform sampler2D emissivemap0;
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
uniform vec3 gltf_emissive_color[GLTF_INDEXED_CHANNELS];
#if SHARED_ACTOR_FX_ALPHA_MODE == SHARED_ACTOR_FX_ALPHA_MASK
uniform float gltf_minimum_alpha[GLTF_INDEXED_CHANNELS];
#endif
#else
uniform sampler2D diffuseMap;
uniform sampler2D emissiveMap;
uniform vec3 emissiveColor;
#if SHARED_ACTOR_FX_ALPHA_MODE == SHARED_ACTOR_FX_ALPHA_MASK
uniform float minimum_alpha;
#endif
#endif

uniform float sharedActorFxOpacity;
// Authored bloom always survives. Style/treatment bloom is added here only for
// the same deliberately synthetic look set used by non-emissive components,
// preventing material-boundary bloom on ordinary looks.
uniform int sharedActorFxSyntheticEnabled;

out vec4 frag_color;

in vec3 vary_position;
in vec4 vertex_emissive;
in vec4 vertex_color;
in vec2 base_color_texcoord;
in vec2 emissive_texcoord;
#ifdef SHARED_ACTOR_FX_SLOT_FILTER
flat in int vary_shared_material_slot;
#endif

vec3 srgb_to_linear(vec3 c);

#ifdef HAS_ACTOR_FX
vec3 actorFxApply(vec3 source, vec3 normal_eye, vec3 position_eye,
                  vec2 authored_uv);
vec3 actorFxEmissive(vec3 authored_emissive, vec3 styled_color);
bool actorFxActive();
float actorFxAuthoredMaterialResponse();
bool actorFxUvTransformEnabled();
bool actorFxRgbSplitEnabled();
vec2 actorFxUv(vec2 authored_uv, vec3 position_eye);
vec2 actorFxRgbSplitUv(vec2 transformed_uv, float direction);
#endif

#ifdef SHARED_ACTOR_FX_SLOT_FILTER
vec4 shared_glow_sample_basecolor(vec2 uv)
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

vec3 shared_glow_sample_emissive(vec2 uv)
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

vec3 shared_glow_emissive_factor()
{
    return gltf_emissive_color[vary_shared_material_slot];
}
#if SHARED_ACTOR_FX_ALPHA_MODE == SHARED_ACTOR_FX_ALPHA_MASK
float shared_glow_minimum_alpha()
{
    return gltf_minimum_alpha[vary_shared_material_slot];
}
#endif
#else
vec4 shared_glow_sample_basecolor(vec2 uv) { return texture(diffuseMap, uv); }
vec3 shared_glow_sample_emissive(vec2 uv) { return texture(emissiveMap, uv).rgb; }
vec3 shared_glow_emissive_factor() { return emissiveColor; }
#if SHARED_ACTOR_FX_ALPHA_MODE == SHARED_ACTOR_FX_ALPHA_MASK
float shared_glow_minimum_alpha() { return minimum_alpha; }
#endif
#endif

void main()
{
    vec4 basecolor = shared_glow_sample_basecolor(base_color_texcoord);
    float authored_alpha = basecolor.a * vertex_color.a;

#if SHARED_ACTOR_FX_ALPHA_MODE == SHARED_ACTOR_FX_ALPHA_MASK
    if (authored_alpha < shared_glow_minimum_alpha())
    {
        discard;
    }
#endif

#ifdef HAS_ACTOR_FX
    bool actor_fx_active = actorFxActive();
    bool fx_uv_transform = actor_fx_active && actorFxUvTransformEnabled();
    bool fx_rgb_split = actor_fx_active && actorFxRgbSplitEnabled();
    vec2 fx_base_uv = base_color_texcoord;
    if (fx_uv_transform)
    {
        fx_base_uv = actorFxUv(fx_base_uv, vary_position);
        basecolor.rgb = shared_glow_sample_basecolor(fx_base_uv).rgb;
    }
    if (fx_rgb_split)
    {
        basecolor.r = shared_glow_sample_basecolor(
            actorFxRgbSplitUv(fx_base_uv, -1.0)).r;
        basecolor.b = shared_glow_sample_basecolor(
            actorFxRgbSplitUv(fx_base_uv, 1.0)).b;
    }
#endif

    vec3 emissive = shared_glow_emissive_factor();
#ifdef HAS_ACTOR_FX
    vec2 fx_emissive_uv = emissive_texcoord;
    if (fx_uv_transform)
    {
        fx_emissive_uv = actorFxUv(fx_emissive_uv, vary_position);
    }
    vec3 emissive_texel = shared_glow_sample_emissive(fx_emissive_uv);
    if (fx_rgb_split)
    {
        emissive_texel.r = shared_glow_sample_emissive(
            actorFxRgbSplitUv(fx_emissive_uv, -1.0)).r;
        emissive_texel.b = shared_glow_sample_emissive(
            actorFxRgbSplitUv(fx_emissive_uv, 1.0)).b;
    }
    emissive *= srgb_to_linear(emissive_texel);
#else
    emissive *= srgb_to_linear(shared_glow_sample_emissive(emissive_texcoord));
#endif

    vec3 covered_authored = emissive;
    vec3 synthetic_emissive = vec3(0.0);

#ifdef HAS_ACTOR_FX
    if (actor_fx_active)
    {
        covered_authored *= actorFxAuthoredMaterialResponse();

        // Glow VBOs do not require tangent/normal streams.  Match the native
        // Actor FX glow program and reconstruct the eye-space geometric normal.
        vec3 actor_fx_normal = cross(dFdx(vary_position), dFdy(vary_position));
        float actor_fx_normal_len2 = dot(actor_fx_normal, actor_fx_normal);
        actor_fx_normal = actor_fx_normal_len2 > 1e-12
            ? actor_fx_normal * inversesqrt(actor_fx_normal_len2)
            : vec3(0.0, 0.0, 1.0);

        // Decode sRGB first; vertex_color is the linear GLTF baseColorFactor.
        vec3 styled_source = srgb_to_linear(basecolor.rgb) * vertex_color.rgb;
        vec3 styled = actorFxApply(styled_source,
                                   actor_fx_normal,
                                   vary_position,
                                   base_color_texcoord);
        if (sharedActorFxSyntheticEnabled != 0)
        {
            synthetic_emissive = actorFxEmissive(vec3(0.0), styled);
        }
    }
#endif

    float authored_lum = max(max(covered_authored.r, covered_authored.g),
                             covered_authored.b) * vertex_emissive.a;
    float synthetic_lum = max(max(synthetic_emissive.r,
                                  synthetic_emissive.g),
                              synthetic_emissive.b);
    float coverage = 1.0;
#if SHARED_ACTOR_FX_ALPHA_MODE == SHARED_ACTOR_FX_ALPHA_BLEND
    coverage = authored_alpha;
#endif

    float opacity = clamp(sharedActorFxOpacity, 0.0, 1.0);
    float lum = max(authored_lum + synthetic_lum, 0.0) * coverage * opacity;

    frag_color.rgb = vec3(0.0);
    frag_color.a = lum;
}
