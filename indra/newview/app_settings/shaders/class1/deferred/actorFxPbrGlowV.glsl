/**
 * Synthetic PBR Actor FX glow vertex shader. Unlike the authored PBR glow
 * shader this consumes diffuse colour, not an emissive stream, so zero-glow
 * alpha materials can participate without duplicate VBO data.
 */

#ifdef HAS_SKIN
uniform mat4 modelview_matrix;
uniform mat4 projection_matrix;
mat4 getObjectSkinnedTransform();
#else
uniform mat4 modelview_projection_matrix;
uniform mat4 modelview_matrix;
#endif

uniform mat4 texture_matrix0;
uniform vec4[2] texture_base_color_transform;

in vec3 position;
in vec4 diffuse_color;
in vec2 texcoord0;

out vec3 vary_actor_fx_position;
out vec3 vary_position;
out vec4 vertex_color;
out vec2 base_color_texcoord;

vec2 texture_transform(vec2 vertex_texcoord, vec4[2] khr_gltf_transform,
                       mat4 sl_animation_transform);

void main()
{
    vary_actor_fx_position = position;
#ifdef HAS_SKIN
    mat4 mat = modelview_matrix * getObjectSkinnedTransform();
    vec3 pos = (mat * vec4(position, 1.0)).xyz;
    gl_Position = projection_matrix * vec4(pos, 1.0);
    vary_position = pos;
#else
    gl_Position = modelview_projection_matrix * vec4(position, 1.0);
    vary_position = (modelview_matrix * vec4(position, 1.0)).xyz;
#endif

    base_color_texcoord = texture_transform(texcoord0,
        texture_base_color_transform, texture_matrix0);
    vertex_color = diffuse_color;
}
