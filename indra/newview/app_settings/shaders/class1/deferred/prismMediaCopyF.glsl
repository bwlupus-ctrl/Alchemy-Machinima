// RV32 byte order has already been resolved by the viewer upload. VLC reports
// OpenGL orientation: the WHOLE used buffer is bottom-left, no extra Y flip.
uniform sampler2D diffuseMap;
uniform ivec2 contentSize;
in vec2 media_uv;
out vec4 frag_color;
vec3 decodeSDR(vec3 c)
{
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)),
               step(vec3(0.04045), c));
}
void main()
{
    // Level-zero fetches never change the borrowed texture's sampler state.
    // Clamp to content texel centres, excluding allocation padding only.
    vec2 p = clamp(media_uv * vec2(contentSize) - 0.5,
                   vec2(0.0), vec2(contentSize - 1));
    ivec2 lo = ivec2(floor(p));
    ivec2 hi = min(lo + 1, contentSize - 1);
    vec2 f = fract(p);
    vec3 a = mix(decodeSDR(texelFetch(diffuseMap, lo, 0).rgb),
                 decodeSDR(texelFetch(diffuseMap, ivec2(hi.x, lo.y), 0).rgb), f.x);
    vec3 b = mix(decodeSDR(texelFetch(diffuseMap, ivec2(lo.x, hi.y), 0).rgb),
                 decodeSDR(texelFetch(diffuseMap, hi, 0).rgb), f.x);
    frag_color = vec4(mix(a, b, f.y), 0.0); // retained alpha is camera glow
}
