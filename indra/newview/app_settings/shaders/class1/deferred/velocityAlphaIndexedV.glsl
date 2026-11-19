/**
 * Indexed multi-material alpha-mask velocity vertex shader.
 */

uniform mat4 modelview_projection_matrix;
uniform mat4 modelview_matrix;
uniform mat4 projection_matrix;
uniform mat4 projection_matrix_unjittered;
uniform mat4 last_projection_matrix_unjittered;
uniform mat4 last_modelview_matrix;
uniform mat4 last_object_matrix;

#ifdef PBR_ALPHA_MASK
uniform vec4 gltf_basecolor_transform[2*GLTF_INDEXED_CHANNELS];
vec2 texture_transform(vec2 vertex_texcoord, vec4[2] khr_gltf_transform,
                       mat4 sl_animation_transform);
#endif

in vec3 position;
in vec4 diffuse_color;
in vec2 texcoord0;
in int texture_index;

flat out int vary_material_index;
out vec2 vary_texcoord0;
out vec4 vertex_color;
out vec4 vary_cur_clip;
out vec4 vary_last_clip;
out vec3 vary_actor_fx_position;

#ifdef HAS_SKIN
mat4 getObjectSkinnedTransform();
mat4 getLastObjectSkinnedTransform();
#endif

void main()
{
    int mi = texture_index;
    vary_material_index = mi;
#ifdef PBR_ALPHA_MASK
    vec4 bc[2];
    bc[0] = gltf_basecolor_transform[2*mi];
    bc[1] = gltf_basecolor_transform[2*mi+1];
    vary_texcoord0 = texture_transform(texcoord0, bc, mat4(1.0));
#else
    // Indexed legacy-material faces exclude texture animation and have their
    // diffuse transform baked into texcoord0 during buffer construction.
    vary_texcoord0 = texcoord0;
#endif
    vertex_color = diffuse_color;
    vary_actor_fx_position = position;

#ifdef HAS_SKIN
    mat4 cur_mat = getObjectSkinnedTransform();
    vec4 mv_pos = modelview_matrix * cur_mat * vec4(position.xyz, 1.0);
    gl_Position = projection_matrix * mv_pos;
    vary_cur_clip = projection_matrix_unjittered * mv_pos;
    mat4 last_mat = getLastObjectSkinnedTransform();
    vary_last_clip = last_projection_matrix_unjittered * last_modelview_matrix
                   * last_mat * vec4(position.xyz, 1.0);
#else
    gl_Position = modelview_projection_matrix * vec4(position.xyz, 1.0);
    vary_cur_clip = projection_matrix_unjittered * modelview_matrix
                  * vec4(position.xyz, 1.0);
    vary_last_clip = last_projection_matrix_unjittered * last_modelview_matrix
                   * last_object_matrix * vec4(position.xyz, 1.0);
#endif
}
