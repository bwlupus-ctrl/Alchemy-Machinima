/**
 * @file prismLensV.glsl
 * @brief Real-face vertex stage for the Prism Lens HDR composite.
 */

uniform mat4 modelview_projection_matrix;
uniform vec3 surfaceOrigin;
uniform vec3 surfaceUDual;
uniform vec3 surfaceVDual;

in vec3 position;

out vec2 prism_uv;

void main()
{
    vec3 surface_position = position - surfaceOrigin;
    prism_uv = vec2(dot(surface_position, surfaceUDual),
                    dot(surface_position, surfaceVDual));
    gl_Position = modelview_projection_matrix * vec4(position, 1.0);
}
