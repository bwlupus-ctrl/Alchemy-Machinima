/** Shared Actor FX synthetic PBR glow for VBOs without TYPE_EMISSIVE. */

/*[EXTRA_CODE_HERE]*/

#define SHARED_ACTOR_FX_ALPHA_OPAQUE 0
#define SHARED_ACTOR_FX_ALPHA_MASK   1
#define SHARED_ACTOR_FX_ALPHA_BLEND  2

#ifndef SHARED_ACTOR_FX_ALPHA_MODE
#error SHARED_ACTOR_FX_ALPHA_MODE must be OPAQUE, MASK, or BLEND
#endif

#ifdef SHARED_ACTOR_FX_SLOT_FILTER
uniform sampler2D basecolor0;
#if GLTF_INDEXED_CHANNELS > 1
uniform sampler2D basecolor1;
#endif
#if GLTF_INDEXED_CHANNELS > 2
uniform sampler2D basecolor2;
#endif
#if GLTF_INDEXED_CHANNELS > 3
uniform sampler2D basecolor3;
#endif
#if GLTF_INDEXED_CHANNELS > 4
uniform sampler2D basecolor4;
#endif
#if GLTF_INDEXED_CHANNELS > 5
uniform sampler2D basecolor5;
#endif
#if GLTF_INDEXED_CHANNELS > 6
uniform sampler2D basecolor6;
#endif
#if GLTF_INDEXED_CHANNELS > 7
uniform sampler2D basecolor7;
#endif
#if SHARED_ACTOR_FX_ALPHA_MODE == SHARED_ACTOR_FX_ALPHA_MASK
uniform float gltf_minimum_alpha[GLTF_INDEXED_CHANNELS];
#endif
#else
uniform sampler2D diffuseMap;
#if SHARED_ACTOR_FX_ALPHA_MODE == SHARED_ACTOR_FX_ALPHA_MASK
uniform float minimum_alpha;
#endif
#endif

uniform float sharedActorFxOpacity;
uniform vec3 sun_dir;
uniform vec3 moon_dir;
uniform int sun_up_factor;

out vec4 frag_color;

in vec3 vary_position;
in vec3 vary_normal;
in vec4 vertex_color;
in vec2 base_color_texcoord;
#ifdef SHARED_ACTOR_FX_SLOT_FILTER
flat in int vary_shared_material_slot;
#endif

vec3 srgb_to_linear(vec3 c);
vec3 actorFxPbrSyntheticEmission(vec3 authored_source,
                                 vec3 geometry_normal_eye,
                                 vec3 position_eye, vec2 authored_uv,
                                 float dissolve_coverage);
bool actorFxActive();
float actorFxPbrDissolveCoverage();
float actorFxPbrDissolveAlpha(float coverage);
bool actorFxUvTransformEnabled();
bool actorFxRgbSplitEnabled();
vec2 actorFxUv(vec2 authored_uv, vec3 position_eye);
vec2 actorFxRgbSplitUv(vec2 transformed_uv, float direction);

void calcAtmosphericVarsLinear(vec3 inPositionEye, vec3 norm,
                               vec3 light_dir, out vec3 sunlit,
                               out vec3 amblit, out vec3 additive,
                               out vec3 atten);
vec4 applySkyAndWaterFog(vec3 pos, vec3 additive, vec3 atten, vec4 color);

vec3 shared_synthetic_geometry_normal()
{
    vec3 n = vary_normal;
    float n_len2 = dot(n, n);
    n = n_len2 > 1e-12
        ? n * inversesqrt(n_len2) : vec3(0.0, 0.0, 1.0);
    return n * (gl_FrontFacing ? 1.0 : -1.0);
}

vec3 shared_synthetic_attenuate(vec3 emitted, vec3 geometry_normal)
{
    if (max(max(emitted.r, emitted.g), emitted.b) <= 0.0)
    {
        return vec3(0.0);
    }

    vec3 light_dir = (sun_up_factor == 1) ? sun_dir : moon_dir;
    vec3 sunlit;
    vec3 amblit;
    vec3 additive;
    vec3 atten;
    calcAtmosphericVarsLinear(vary_position, geometry_normal, light_dir,
                               sunlit, amblit, additive, atten);

    vec3 fogged = applySkyAndWaterFog(
        vary_position, additive, atten, vec4(emitted, 1.0)).rgb;
    vec3 fogged_zero = applySkyAndWaterFog(
        vary_position, additive, atten, vec4(vec3(0.0), 1.0)).rgb;
    return max(fogged - fogged_zero, vec3(0.0));
}

#ifdef SHARED_ACTOR_FX_SLOT_FILTER
vec4 shared_synthetic_sample_basecolor(vec2 uv)
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
#if SHARED_ACTOR_FX_ALPHA_MODE == SHARED_ACTOR_FX_ALPHA_MASK
float shared_synthetic_minimum_alpha()
{
    return gltf_minimum_alpha[vary_shared_material_slot];
}
#endif
#else
vec4 shared_synthetic_sample_basecolor(vec2 uv)
{
    return texture(diffuseMap, uv);
}
#if SHARED_ACTOR_FX_ALPHA_MODE == SHARED_ACTOR_FX_ALPHA_MASK
float shared_synthetic_minimum_alpha() { return minimum_alpha; }
#endif
#endif

void main()
{
    vec4 authored = shared_synthetic_sample_basecolor(base_color_texcoord);
    float authored_alpha = authored.a * vertex_color.a;

#if SHARED_ACTOR_FX_ALPHA_MODE == SHARED_ACTOR_FX_ALPHA_MASK
    if (authored_alpha < shared_synthetic_minimum_alpha())
    {
        discard;
    }
#endif

    if (!actorFxActive())
    {
        frag_color = vec4(0.0);
        return;
    }

    float actor_fx_dissolve_coverage = actorFxPbrDissolveCoverage();
    if (actor_fx_dissolve_coverage < 0.0)
    {
        discard;
    }

    vec2 fx_uv = base_color_texcoord;
    if (actorFxUvTransformEnabled())
    {
        fx_uv = actorFxUv(fx_uv, vary_position);
        authored.rgb = shared_synthetic_sample_basecolor(fx_uv).rgb;
    }
    if (actorFxRgbSplitEnabled())
    {
        authored.r = shared_synthetic_sample_basecolor(
            actorFxRgbSplitUv(fx_uv, -1.0)).r;
        authored.b = shared_synthetic_sample_basecolor(
            actorFxRgbSplitUv(fx_uv, 1.0)).b;
    }

    vec3 n = shared_synthetic_geometry_normal();

    // The texture is sRGB and baseColorFactor/vertex_color is linear.
    vec3 source = srgb_to_linear(authored.rgb) * vertex_color.rgb;
    vec3 emitted = actorFxPbrSyntheticEmission(
        source, n, vary_position, base_color_texcoord,
        actor_fx_dissolve_coverage);
    emitted = shared_synthetic_attenuate(emitted, n);

    float coverage = actorFxPbrDissolveAlpha(actor_fx_dissolve_coverage);
#if SHARED_ACTOR_FX_ALPHA_MODE == SHARED_ACTOR_FX_ALPHA_BLEND
    coverage *= authored_alpha;
#endif
    float opacity = clamp(sharedActorFxOpacity, 0.0, 1.0);
    float glow = max(max(emitted.r, emitted.g), emitted.b);
    frag_color = vec4(0.0, 0.0, 0.0,
                      max(glow, 0.0) * coverage * opacity);
}
