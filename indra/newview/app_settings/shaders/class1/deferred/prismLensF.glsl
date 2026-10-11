/**
 * @file prismLensF.glsl
 * @brief World entry point for the shared Prism signal compositor.
 *
 * The default world path calls prismSignalSample directly with the original
 * varyings and uniforms. The implementation was moved without algebraic edits,
 * so default pixels execute the identical operations in the identical order.
 */
in vec2 prism_uv;
in vec3 prism_eye_pos;
in vec3 prism_eye_normal;
out vec4 frag_color;
vec4 prismSignalSample(vec2 surface_uv, vec3 eye_pos, vec3 eye_normal);
void main()
{
    frag_color = prismSignalSample(prism_uv, prism_eye_pos, prism_eye_normal);
}
