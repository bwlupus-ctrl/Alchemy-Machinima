// [ActorKey] WP1: R8 downsample and separable Gaussian. Private key program.
/*[EXTRA_CODE_HERE]*/
uniform sampler2D keySource;
uniform vec2 keyDirection;
uniform float keyRadius;
uniform int keyDownsample;
in vec2 vary_fragcoord;
out vec4 frag_color;
void main()
{
    if (keyDownsample != 0)
    {
        // Bilinear at the centre of each 2x2 footprint is the box downsample.
        frag_color = vec4(texture(keySource, (floor(gl_FragCoord.xy) * 2.0 + 1.0) /
            vec2(textureSize(keySource, 0))).r, 0.0, 0.0, 1.0);
        return;
    }
    int taps = clamp(int(ceil(keyRadius)), 1, 16);
    float sigma = max(keyRadius * 0.5, 0.25);
    float total = 0.0, weight = 0.0;
    for (int i = -16; i <= 16; ++i)
    {
        if (abs(i) > taps) continue;
        float w = exp(-0.5 * float(i*i) / (sigma*sigma));
        total += w * texture(keySource, vary_fragcoord + keyDirection * float(i)).r;
        weight += w;
    }
    frag_color = vec4(total / weight, 0.0, 0.0, 1.0);
}
