/**
 * @file sensorF.glsl
 * @brief Final main presentation sensor collection and ADC.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

/*[EXTRA_CODE_HERE]*/
in vec2 vary_fragcoord;
out vec4 frag_color;
uniform sampler2D diffuseRect;
struct SensorParameters
{
    vec4 physics; // FW, sigma_r, analog DN/e-, black pedestal DN
    vec4 scale; // white DN, K electrons/linear, g_N, highlight knee fraction
    vec4 controls; // noise amount, chroma amount, total ISO gain, green read RMS
    mat3 cameraToWorking;
    mat3 workingToCamera;
    uvec4 clock; // capture-relative frame low/high, take seed, presentation stream
    mat3 readCameraBasis; // precomputed camera-space C*orthonormal luma/chroma rotation
    uvec4 randomContext; // capture tuple prefix, computed with identical integer math on CPU
};
uniform SensorParameters sensor;
vec3 applySensor(vec3 workingLinear, ivec2 pixel, SensorParameters p);
void main()
{
    vec4 scene = texture(diffuseRect, vary_fragcoord);
    vec3 collected = applySensor(scene.rgb, ivec2(gl_FragCoord.xy), sensor);
    // The camera scratch is half-float. Preserve signed blacks, with finite
    // storage even for extreme custom calibration headroom.
    frag_color = vec4(clamp(collected, vec3(-65000.0), vec3(65000.0)), scene.a);
}
