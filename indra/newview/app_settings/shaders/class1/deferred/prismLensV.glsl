/**
 * @file prismLensV.glsl
 * @brief Real-face vertex stage for the Prism Lens HDR composite.
 */

uniform mat4 modelview_projection_matrix;
uniform mat4 modelview_matrix;
uniform mat3 normal_matrix;
uniform vec3 surfaceOrigin;
uniform vec3 surfaceUDual;
uniform vec3 surfaceVDual;

in vec3 position;

out vec2 prism_uv;

// Eye-space position and face normal for the per-display environmental sheen
// (Feature B). Both are derived from the same current modelview the composite
// draws with, so they are correct for static and matrix-pushed faces alike and
// need no camera-origin uniform (the camera is the eye-space origin). They are
// unused unless a display's sheen strength is > 0, so this adds no cost at the
// default (sheen 0) and never perturbs the byte-identical picture path.
out vec3 prism_eye_pos;
out vec3 prism_eye_normal;

void main()
{
    vec3 surface_position = position - surfaceOrigin;
    prism_uv = vec2(dot(surface_position, surfaceUDual),
                    dot(surface_position, surfaceVDual));

    prism_eye_pos = (modelview_matrix * vec4(position, 1.0)).xyz;
    // The dual basis lies in the face plane, so their cross is the face normal.
    prism_eye_normal = normal_matrix * cross(surfaceUDual, surfaceVDual);

    gl_Position = modelview_projection_matrix * vec4(position, 1.0);
}
