/**
 * @file weatherPrecipVeilF.glsl
 * @brief Optional single-pass physical precipitation extinction before tonemap.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork; Slipstream precipitation visibility design reference.
 * New GL implementation; no Vulkan source/ABI copied.
 * $/LicenseInfo$
 */
/*[EXTRA_CODE_HERE]*/
in vec2 vary_fragcoord;
uniform sampler2D depthMap;
uniform mat4 snow_inverse_projection, snow_inverse_view;
uniform vec3 snow_veil_light;
uniform float snow_extinction;
out vec4 frag_color;
float weatherPrecipExposedDistance(vec3 start, vec3 direction, float distance_m);
void main()
{
    vec2 uv = vary_fragcoord;
    float z = texture(depthMap,uv).r;
    vec4 p = snow_inverse_projection*vec4(uv*2.0-1.0,min(z,0.999999)*2.0-1.0,1.0);
    vec3 eye = p.xyz/p.w;
    vec3 start = (snow_inverse_view*vec4(0,0,0,1)).xyz;
    vec3 direction = (snow_inverse_view*vec4(normalize(eye),0)).xyz;
    float distance_m = z >= 0.999999 ? 3000.0 : min(length(eye),3000.0);
    float exposed_m = weatherPrecipExposedDistance(start,direction,distance_m);
    float a = clamp(1.0-exp(-max(snow_extinction,0.0)*exposed_m),0.0,1.0);
    frag_color = vec4(max(snow_veil_light,vec3(0))*a,a);
}
