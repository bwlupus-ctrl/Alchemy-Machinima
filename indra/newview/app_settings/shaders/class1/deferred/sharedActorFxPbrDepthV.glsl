/**
 * Minimal shared Actor FX PBR depth-prime vertex shader.
 *
 * OPAQUE carries only position. MASK additionally carries the exact GLTF base
 * colour UV/factor used by beauty. Indexed MASK selects its KHR transform per
 * vertex so one draw covers the complete multi-material batch.
 */

#define SHARED_ACTOR_FX_ALPHA_OPAQUE 0
#define SHARED_ACTOR_FX_ALPHA_MASK   1
#define SHARED_ACTOR_FX_ALPHA_BLEND  2

#ifndef SHARED_ACTOR_FX_ALPHA_MODE
#error SHARED_ACTOR_FX_ALPHA_MODE must be OPAQUE or MASK
#endif

#ifdef HAS_SKIN
uniform mat4 modelview_matrix;
uniform mat4 projection_matrix;
mat4 getObjectSkinnedTransform();
#else
uniform mat4 modelview_matrix;
uniform mat4 modelview_projection_matrix;
#endif

in vec3 position;
out vec3 vary_actor_fx_position;
out vec3 vary_position;

#if SHARED_ACTOR_FX_ALPHA_MODE == SHARED_ACTOR_FX_ALPHA_MASK
#ifdef SHARED_ACTOR_FX_SLOT_FILTER
uniform vec4 gltf_basecolor_transform[2*GLTF_INDEXED_CHANNELS];
in int texture_index;
flat out int vary_shared_material_slot;
#else
uniform mat4 texture_matrix0;
uniform vec4[2] texture_base_color_transform;
#endif
in vec4 diffuse_color;
in vec2 texcoord0;
out vec4 vertex_color;
out vec2 base_color_texcoord;
vec2 texture_transform(vec2 vertex_texcoord,
                       vec4[2] khr_gltf_transform,
                       mat4 sl_animation_transform);
#endif

void main()
{
    vary_actor_fx_position = position;
#ifdef HAS_SKIN
    mat4 mat = modelview_matrix * getObjectSkinnedTransform();
    vec4 pos = mat * vec4(position, 1.0);
    gl_Position = projection_matrix * pos;
#else
    vec4 pos = modelview_matrix * vec4(position, 1.0);
    gl_Position = modelview_projection_matrix * vec4(position, 1.0);
#endif
    vary_position = pos.xyz;

#if SHARED_ACTOR_FX_ALPHA_MODE == SHARED_ACTOR_FX_ALPHA_MASK
    vec4 base_transform[2];
    mat4 sl_transform;
#ifdef SHARED_ACTOR_FX_SLOT_FILTER
    int mi = texture_index;
    vary_shared_material_slot = mi;
    base_transform[0] = gltf_basecolor_transform[2*mi];
    base_transform[1] = gltf_basecolor_transform[2*mi+1];
    sl_transform = mat4(1.0);
#else
    base_transform[0] = texture_base_color_transform[0];
    base_transform[1] = texture_base_color_transform[1];
    sl_transform = texture_matrix0;
#endif
    base_color_texcoord = texture_transform(texcoord0, base_transform,
                                            sl_transform);
    vertex_color = diffuse_color;
#endif
}
