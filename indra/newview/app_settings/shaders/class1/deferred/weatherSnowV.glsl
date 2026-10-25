/**
 * @file weatherSnowV.glsl
 * @brief Instanced, coverage-preserving snow billboards (or veil triangle).
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork; Slipstream precipitation design credit.
 * New GL implementation; no Vulkan source/ABI copied.
 * $/LicenseInfo$
 */
/*[EXTRA_CODE_HERE]*/
#ifdef SNOW_VEIL
out vec2 vary_fragcoord;
void main()
{
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vary_fragcoord = p; gl_Position = vec4(p*2.0-1.0,0.0,1.0);
}
#else
in vec4 position;
in vec4 texcoord0;
uniform mat4 snow_view, snow_projection;
uniform vec2 snow_screen;
uniform float snow_size, snow_range;
out vec2 snow_uv;
out vec3 snow_agent;
flat out float snow_alpha;
flat out float snow_eye_depth;
flat out float snow_distance;
void main()
{
    const vec2 corners[6] = vec2[6](vec2(-1,-1),vec2(1,-1),vec2(1,1),
        vec2(-1,-1),vec2(1,1),vec2(-1,1));
    vec4 eye = snow_view * vec4(position.xyz,1.0);
    vec4 clip = snow_projection * eye;
    snow_uv = corners[gl_VertexID]; snow_agent = position.xyz;
    snow_eye_depth = -eye.z;
    snow_distance = length(eye.xyz);
    float diameter = snow_size * texcoord0.x;
    float px = diameter * abs(snow_projection[1][1]) * snow_screen.y / max(2.0*clip.w,0.001);
    float width = max(px,1.5);
    clip.xy += snow_uv * width / snow_screen * clip.w;
    float distance_fade = 1.0-smoothstep(snow_range*0.8,snow_range,length(eye.xyz));
    snow_alpha = position.w * min(px*px/(width*width),1.0) * distance_fade;
    gl_Position = clip;
    if (clip.w <= 0.0 || snow_alpha <= 0.0) gl_Position = vec4(2,2,2,1);
}
#endif
