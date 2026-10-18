/** @file prismLensF.glsl
 * World displays always write scene-linear radiance and the ordinary glow.
 * Each source uses HEAD's scene transfer; the main pipeline tonemaps once.
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
