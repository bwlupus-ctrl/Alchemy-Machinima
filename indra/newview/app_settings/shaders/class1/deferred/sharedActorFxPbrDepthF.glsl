/** Minimal shared Actor FX PBR depth-prime fragment shader. */

#define SHARED_ACTOR_FX_ALPHA_OPAQUE 0
#define SHARED_ACTOR_FX_ALPHA_MASK   1
#define SHARED_ACTOR_FX_ALPHA_BLEND  2

#ifndef SHARED_ACTOR_FX_ALPHA_MODE
#error SHARED_ACTOR_FX_ALPHA_MODE must be OPAQUE or MASK
#endif

out vec4 frag_color;
in vec3 vary_actor_fx_position;
in vec3 vary_position;

void mirrorClip(vec3 pos);
bool actorFxDissolveDiscard(vec3 object_position);

#if SHARED_ACTOR_FX_ALPHA_MODE == SHARED_ACTOR_FX_ALPHA_MASK
in vec4 vertex_color;
in vec2 base_color_texcoord;
#ifdef SHARED_ACTOR_FX_SLOT_FILTER
flat in int vary_shared_material_slot;
uniform float gltf_minimum_alpha[GLTF_INDEXED_CHANNELS];
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

float shared_depth_alpha(vec2 uv)
{
    if (vary_shared_material_slot == 0) return texture(basecolor0, uv).a;
#if GLTF_INDEXED_CHANNELS > 1
    if (vary_shared_material_slot == 1) return texture(basecolor1, uv).a;
#endif
#if GLTF_INDEXED_CHANNELS > 2
    if (vary_shared_material_slot == 2) return texture(basecolor2, uv).a;
#endif
#if GLTF_INDEXED_CHANNELS > 3
    if (vary_shared_material_slot == 3) return texture(basecolor3, uv).a;
#endif
#if GLTF_INDEXED_CHANNELS > 4
    if (vary_shared_material_slot == 4) return texture(basecolor4, uv).a;
#endif
#if GLTF_INDEXED_CHANNELS > 5
    if (vary_shared_material_slot == 5) return texture(basecolor5, uv).a;
#endif
#if GLTF_INDEXED_CHANNELS > 6
    if (vary_shared_material_slot == 6) return texture(basecolor6, uv).a;
#endif
#if GLTF_INDEXED_CHANNELS > 7
    if (vary_shared_material_slot == 7) return texture(basecolor7, uv).a;
#endif
    return 1.0;
}

float shared_depth_cutoff()
{
    return gltf_minimum_alpha[vary_shared_material_slot];
}
#else
uniform sampler2D diffuseMap;
uniform float minimum_alpha;
float shared_depth_alpha(vec2 uv) { return texture(diffuseMap, uv).a; }
float shared_depth_cutoff() { return minimum_alpha; }
#endif
#endif

void main()
{
    mirrorClip(vary_position);

#if SHARED_ACTOR_FX_ALPHA_MODE == SHARED_ACTOR_FX_ALPHA_MASK
    if (shared_depth_alpha(base_color_texcoord) * vertex_color.a
        < shared_depth_cutoff())
    {
        discard;
    }
#endif
    if (actorFxDissolveDiscard(vary_actor_fx_position))
    {
        discard;
    }
    frag_color = vec4(1.0);
}
