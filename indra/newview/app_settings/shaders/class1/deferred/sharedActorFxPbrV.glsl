/**
 * Shared Actor FX forward-PBR replay vertex shader.
 *
 * This is deliberately isolated from pbralphaV.glsl: the native material pass
 * remains the fail-open authority until the shared replay family has linked
 * and its draw command has passed preflight. Scalar and native-array indexed
 * variants share one material law; the indexed variant shades its complete
 * multi-material VBO in one draw.
 */

#ifdef HAS_SKIN
uniform mat4 modelview_matrix;
uniform mat4 projection_matrix;
mat4 getObjectSkinnedTransform();
#else
uniform mat3 normal_matrix;
uniform mat4 modelview_projection_matrix;
uniform mat4 modelview_matrix;
#endif
#ifdef SHARED_ACTOR_FX_SLOT_FILTER
// True indexed replay: one draw covers every material slot.  These names and
// strides intentionally match pbropaqueIndexedV.glsl so the native indexed
// material binder can upload the complete batch without per-slot redraws.
uniform vec4 gltf_basecolor_transform[2*GLTF_INDEXED_CHANNELS];
uniform vec4 gltf_normal_transform[2*GLTF_INDEXED_CHANNELS];
uniform vec4 gltf_mr_transform[2*GLTF_INDEXED_CHANNELS];
uniform vec4 gltf_emissive_transform[2*GLTF_INDEXED_CHANNELS];
#else
uniform mat4 texture_matrix0;
uniform vec4[2] texture_base_color_transform;
uniform vec4[2] texture_normal_transform;
uniform vec4[2] texture_metallic_roughness_transform;
uniform vec4[2] texture_emissive_transform;
#endif

in vec3 position;
in vec4 diffuse_color;
in vec3 normal;
in vec4 tangent;
in vec2 texcoord0;
#ifdef SHARED_ACTOR_FX_SLOT_FILTER
in int texture_index;
#endif

#ifdef HAS_ACTOR_FX
out vec3 vary_actor_fx_position;
#endif

out vec3 vary_position;
out vec3 vary_fragcoord;
out vec2 base_color_texcoord;
out vec2 normal_texcoord;
out vec2 metallic_roughness_texcoord;
out vec2 emissive_texcoord;
out vec4 vertex_color;
out vec3 vary_tangent;
flat out float vary_sign;
out vec3 vary_normal;
#ifdef SHARED_ACTOR_FX_SLOT_FILTER
flat out int vary_shared_material_slot;
#endif

vec2 texture_transform(vec2 vertex_texcoord,
                       vec4[2] khr_gltf_transform,
                       mat4 sl_animation_transform);
vec4 tangent_space_transform(vec4 vertex_tangent,
                             vec3 vertex_normal,
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
    vary_position = pos;
    vec4 vert = projection_matrix * vec4(pos, 1.0);
#else
    vec4 vert = modelview_projection_matrix * vec4(position.xyz, 1.0);
    vary_position = (modelview_matrix * vec4(position.xyz, 1.0)).xyz;
#endif

    gl_Position = vert;
    vary_fragcoord = vert.xyz;
    // LLFetchedGLTFMaterial::bind supplies four independent KHR transforms for
    // scalar replay. Indexed batches supply the same values as uniform arrays;
    // their material slot is carried per vertex and they never carry SL texture
    // animation (the batcher excludes animated/media faces).
    vec4 shared_base_color_transform[2];
    vec4 shared_normal_transform[2];
    vec4 shared_mr_transform[2];
    vec4 shared_emissive_transform[2];
    mat4 shared_sl_texture_transform;
#ifdef SHARED_ACTOR_FX_SLOT_FILTER
    int mi = texture_index;
    vary_shared_material_slot = mi;
    shared_base_color_transform[0] = gltf_basecolor_transform[2*mi];
    shared_base_color_transform[1] = gltf_basecolor_transform[2*mi+1];
    shared_normal_transform[0] = gltf_normal_transform[2*mi];
    shared_normal_transform[1] = gltf_normal_transform[2*mi+1];
    shared_mr_transform[0] = gltf_mr_transform[2*mi];
    shared_mr_transform[1] = gltf_mr_transform[2*mi+1];
    shared_emissive_transform[0] = gltf_emissive_transform[2*mi];
    shared_emissive_transform[1] = gltf_emissive_transform[2*mi+1];
    shared_sl_texture_transform = mat4(1.0);
#else
    shared_base_color_transform[0] = texture_base_color_transform[0];
    shared_base_color_transform[1] = texture_base_color_transform[1];
    shared_normal_transform[0] = texture_normal_transform[0];
    shared_normal_transform[1] = texture_normal_transform[1];
    shared_mr_transform[0] = texture_metallic_roughness_transform[0];
    shared_mr_transform[1] = texture_metallic_roughness_transform[1];
    shared_emissive_transform[0] = texture_emissive_transform[0];
    shared_emissive_transform[1] = texture_emissive_transform[1];
    shared_sl_texture_transform = texture_matrix0;
#endif
    base_color_texcoord = texture_transform(
        texcoord0, shared_base_color_transform, shared_sl_texture_transform);
    normal_texcoord = texture_transform(
        texcoord0, shared_normal_transform, shared_sl_texture_transform);
    metallic_roughness_texcoord = texture_transform(
        texcoord0, shared_mr_transform,
        shared_sl_texture_transform);
    emissive_texcoord = texture_transform(
        texcoord0, shared_emissive_transform, shared_sl_texture_transform);

#ifdef HAS_SKIN
    vec3 n = (mat * vec4(normal.xyz + position.xyz, 1.0)).xyz - pos.xyz;
    vec3 t = (mat * vec4(tangent.xyz + position.xyz, 1.0)).xyz - pos.xyz;
#else
    vec3 n = normal_matrix * normal;
    vec3 t = normal_matrix * tangent.xyz;
#endif

    n = normalize(n);
    vec4 transformed_tangent = tangent_space_transform(
        vec4(t, tangent.w), n, shared_normal_transform,
        shared_sl_texture_transform);
    vary_tangent = normalize(transformed_tangent.xyz);
    vary_sign = transformed_tangent.w;
    vary_normal = n;
    vertex_color = diffuse_color;
}
