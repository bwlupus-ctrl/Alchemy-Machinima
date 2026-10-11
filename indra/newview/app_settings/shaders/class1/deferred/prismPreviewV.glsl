/** @file prismPreviewV.glsl */
in vec3 position;
out vec2 prism_uv;
out vec3 prism_eye_pos;
out vec3 prism_eye_normal;
uniform mat4 modelview_matrix;
uniform vec3 previewWorldOrigin;
uniform vec3 previewWorldUEdge;
uniform vec3 previewWorldVEdge;
void main()
{
    prism_uv = position.xy * 0.5 + vec2(0.5);
    vec3 world_pos = previewWorldOrigin + prism_uv.x * previewWorldUEdge +
                     prism_uv.y * previewWorldVEdge;
    vec3 eye_u = (modelview_matrix * vec4(previewWorldUEdge, 0.0)).xyz;
    vec3 eye_v = (modelview_matrix * vec4(previewWorldVEdge, 0.0)).xyz;
    prism_eye_pos = (modelview_matrix * vec4(world_pos, 1.0)).xyz;
    prism_eye_normal = cross(eye_u, eye_v);
    gl_Position = vec4(position.xy, 0.0, 1.0);
}
