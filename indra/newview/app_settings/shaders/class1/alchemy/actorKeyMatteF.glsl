// [ActorKey] WP1, retained Part B 3.6. G reserved for WP4 actor shadows (zero).
/*[EXTRA_CODE_HERE]*/
uniform sampler2D keyCoverage;
uniform sampler2D keyBlur;
uniform vec2 keyRemap; // lo, hi
uniform int keyRaw;
uniform int keyKeepInterior;
uniform int keyDebug;
in vec2 vary_fragcoord;
out vec4 frag_color;
void main()
{
    float coverage = texture(keyCoverage, vary_fragcoord).r;
    float m = coverage;
    if (keyRaw == 0)
    {
        float b = texture(keyBlur, vary_fragcoord).r;
        // Handle clamped equal endpoints without smoothstep's undefined 0/0.
        m = keyRemap.x < keyRemap.y ? smoothstep(keyRemap.x, keyRemap.y, b) : step(keyRemap.x, b);
        if (keyKeepInterior != 0) m = max(m, step(0.999, coverage));
    }
    frag_color = keyDebug != 0 ? vec4(m, m, m, 1.0) : vec4(m, 0.0, 0.0, 1.0);
}
