/**
 * @file weatherSnowF.glsl
 * @brief Lit premultiplied soft flakes against native scene depth.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork; Slipstream precipitation design reference.
 * New GL implementation; no Vulkan source/ABI copied.
 * $/LicenseInfo$
 */
/*[EXTRA_CODE_HERE]*/
in vec2 snow_uv;
in vec3 snow_agent;
flat in float snow_alpha, snow_eye_depth;
flat in float snow_distance;
uniform sampler2D depthMap;
uniform mat4 snow_inverse_projection;
uniform vec2 snow_screen;
uniform vec3 snow_light;
uniform vec3 snow_veil_light;
uniform float snow_extinction;
out vec4 frag_color;
float weatherPrecipOpen(vec3 agent);
void main()
{
    vec2 uv = gl_FragCoord.xy/snow_screen;
    float z = texture(depthMap,uv).r;
    vec4 scene = snow_inverse_projection*vec4(uv*2.0-1.0,z*2.0-1.0,1.0);
    float gap = -scene.z/scene.w - snow_eye_depth;
    float soft = clamp(gap/0.25,0.0,1.0);
    float open = weatherPrecipOpen(snow_agent);
    float a = snow_alpha*(1.0-smoothstep(0.1,1.0,dot(snow_uv,snow_uv)))*soft;
    if (open >= 0.0) a *= open;
    vec3 light = max(snow_light,vec3(0));
    float transmittance = exp(-max(snow_extinction,0.0)*max(snow_distance,0.0)*max(open,0.0));
    light = light*transmittance + max(snow_veil_light,vec3(0))*(1.0-transmittance);
    frag_color = vec4(light*a,a);
}
