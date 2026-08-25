/**
 * Shared Actor FX PBR glow replay vertex shader.
 *
 * Mirrors pbrglowV.glsl. The indexed permutation carries the per-vertex
 * material slot and selects its complete base/emissive transform arrays, so
 * beauty and glow make the same decision in one geometry draw.
 */

#ifdef HAS_SKIN
uniform mat4 modelview_matrix;
uniform mat4 projection_matrix;
mat4 getObjectSkinnedTransform();
#else
uniform mat4 modelview_projection_matrix;
uniform mat4 modelview_matrix;
#endif

#ifdef SHARED_ACTOR_FX_SLOT_FILTER
uniform vec4 gltf_basecolor_transform[2*GLTF_INDEXED_CHANNELS];
uniform vec4 gltf_emissive_transform[2*GLTF_INDEXED_CHANNELS];
#else
uniform mat4 texture_matrix0;
uniform vec4[2] texture_base_color_transform;
uniform vec4[2] texture_emissive_transform;
#endif

in vec3 position;
in vec4 emissive;
in vec4 diffuse_color;
in vec2 texcoord0;
#ifdef SHARED_ACTOR_FX_SLOT_FILTER
in int texture_index;
#endif

#ifdef HAS_ACTOR_FX
out vec3 vary_actor_fx_position;
#endif

out vec2 base_color_texcoord;
out vec2 emissive_texcoord;
out vec4 vertex_emissive;
out vec4 vertex_color;
out vec3 vary_position;
#ifdef SHARED_ACTOR_FX_SLOT_FILTER
flat out int vary_shared_material_slot;
#endif

vec2 texture_transform(vec2 vertex_texcoord,
                       vec4[2] khr_gltf_transform,
                       mat4 sl_animation_transform);

void main()
{
#ifdef HAS_ACTOR_FX
    vary_actor_fx_position = position;
#endif

#ifdef HAS_SKIN
    mat4 mat = getObjectSkinnedTransform();
    mat = modelview_matrix * mat;
    vec3 pos = (mat * vec4(position.xyz, 1.0)).xyz;
    gl_Position = projection_matrix * vec4(pos, 1.0);
    vary_position = pos;
#else
    gl_Position = modelview_projection_matrix * vec4(position.xyz, 1.0);
    vary_position = (modelview_matrix * vec4(position.xyz, 1.0)).xyz;
#endif

#ifdef SHARED_ACTOR_FX_SLOT_FILTER
    int mi = texture_index;
    vary_shared_material_slot = mi;
    vec4 shared_base_color_transform[2];
    shared_base_color_transform[0] = gltf_basecolor_transform[2*mi];
    shared_base_color_transform[1] = gltf_basecolor_transform[2*mi+1];
    vec4 shared_emissive_transform[2];
    shared_emissive_transform[0] = gltf_emissive_transform[2*mi];
    shared_emissive_transform[1] = gltf_emissive_transform[2*mi+1];
    mat4 shared_sl_texture_transform = mat4(1.0);
#else
    vec4 shared_base_color_transform[2];
    shared_base_color_transform[0] = texture_base_color_transform[0];
    shared_base_color_transform[1] = texture_base_color_transform[1];
    vec4 shared_emissive_transform[2];
    shared_emissive_transform[0] = texture_emissive_transform[0];
    shared_emissive_transform[1] = texture_emissive_transform[1];
    mat4 shared_sl_texture_transform = texture_matrix0;
#endif
    base_color_texcoord = texture_transform(
        texcoord0, shared_base_color_transform, shared_sl_texture_transform);
    emissive_texcoord = texture_transform(
        texcoord0, shared_emissive_transform, shared_sl_texture_transform);
    vertex_emissive = emissive;
    vertex_color = diffuse_color;
}
