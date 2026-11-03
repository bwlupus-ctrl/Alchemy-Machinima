// WP4: density before the same half-resolution separable blur used by coverage.
/*[EXTRA_CODE_HERE]*/
uniform sampler2D keyActorVisibility;
uniform sampler2D keySetVisibility;
uniform int keyShadowExact;
out vec4 frag_color;
void main()
{
    vec2 size = vec2(textureSize(keyActorVisibility, 0));
    vec2 base = floor(gl_FragCoord.xy) * 2.0;
    float density = 0.0;
    for (int i = 0; i < 4; ++i)
    {
        vec2 uv = (base + vec2(float(i & 1), float(i >> 1)) + 0.5) / size;
        float actor = texture(keyActorVisibility, uv).r;
        float other = keyShadowExact != 0 ? texture(keySetVisibility, uv).r : 1.0;
        density += (1.0 - actor) * other;
    }
    frag_color = vec4(density * 0.25, 0.0, 0.0, 1.0);
}
