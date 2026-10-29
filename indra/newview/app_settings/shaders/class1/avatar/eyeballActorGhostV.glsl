/**
 * Rigid classic-eyeball Actor FX replay vertex shader.
 */

uniform mat4 texture_matrix0;
uniform mat4 modelview_matrix;
uniform mat4 modelview_projection_matrix;
uniform mat3 normal_matrix;

in vec3 position;
in vec3 normal;
in vec2 texcoord0;

out vec2 vary_texcoord0;
out vec3 vary_position;
out vec3 vary_normal;
out vec3 vary_object_position;
flat out int vary_texture_index;
out vec4 vary_vertex_color;

void calcAtmospherics(vec3 inPositionEye);

void main()
{
    vec4 pos = modelview_matrix * vec4(position, 1.0);
    vary_texcoord0 = (texture_matrix0 * vec4(texcoord0, 0.0, 1.0)).xy;
    vary_position = pos.xyz;
    vary_normal = normalize(normal_matrix * normal);
    vary_object_position = position;
    vary_texture_index = -1;
    vary_vertex_color = vec4(1.0);

    calcAtmospherics(pos.xyz);
    gl_Position = modelview_projection_matrix * vec4(position, 1.0);
}
