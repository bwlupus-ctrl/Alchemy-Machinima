/**
 * @file sensorAAEncodeF.glsl
 * @brief Perceptual detection auxiliary; linear radiance is preserved.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

/*[EXTRA_CODE_HERE]*/
in vec2 vary_fragcoord;
out vec4 frag_color;
uniform sampler2D diffuseRect;
uniform int sensor_aa_mode;
void main()
{
    vec3 linear = texture(diffuseRect, vary_fragcoord).rgb;
    vec3 positive = max(linear, vec3(0.0));
    vec3 perceptual = sqrt(positive / (vec3(1.0) + positive));
    float luma = dot(perceptual, vec3(0.299, 0.587, 0.114));
    frag_color = vec4(sensor_aa_mode == 1 ? linear : perceptual, luma);
}
