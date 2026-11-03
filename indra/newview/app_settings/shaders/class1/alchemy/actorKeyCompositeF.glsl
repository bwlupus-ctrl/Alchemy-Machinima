/** Late display-code composite. No blending, encoding, noise or dithering. */
out vec4 frag_color;
in vec2 vary_fragcoord;
uniform sampler2D keyScene;
uniform sampler2D keyCoverage;
uniform vec2 keyOrigin;
uniform vec3 keyColour;
uniform int keyMode;
uniform int keyInvert;
uniform int keyHard;
uniform int keyMatteShadow;
uniform int keyPerChannel;
uniform float keyShadowStrength;
void main()
{
    vec3 scene = texelFetch(keyScene, ivec2(gl_FragCoord.xy - keyOrigin), 0).rgb;
    vec4 coverage = texture(keyCoverage, vary_fragcoord);
    vec3 m = keyPerChannel != 0 ? coverage.rgb : vec3(coverage.r);
    float g = keyPerChannel != 0 ? coverage.a : coverage.g;
    if (keyHard != 0) m = step(vec3(0.5), m);
    vec3 colour;
    if (keyMode == 0) colour = mix(keyColour * (1.0 - keyShadowStrength * g), scene, m);
    else if (keyMode == 1) colour = mix(scene, keyColour, m);
    else
    {
        m = vec3(dot(m, vec3(0.2126, 0.7152, 0.0722)));
        colour = keyInvert != 0 ? vec3(1.0) - m : m;
        if (keyMatteShadow != 0) colour = max(colour, vec3(0.5 * g));
    }
    frag_color = vec4(colour, 1.0);
}
