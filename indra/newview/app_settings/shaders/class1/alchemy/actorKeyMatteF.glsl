// [ActorKey] WP1, retained Part B 3.6. G carries WP4 actor shadow density.
/*[EXTRA_CODE_HERE]*/
uniform sampler2D keyCoverage;
uniform sampler2D keyBlur;
uniform vec2 keyRemap; // lo, hi
uniform int keyRaw;
uniform int keyKeepInterior;
uniform int keyDebug;
uniform int keyShadowEnabled;
uniform int keyShadowExact;
uniform int keyShadowFiltered;
uniform sampler2D keyActorVisibility;
uniform sampler2D keySetVisibility;
uniform float keyShadowSoftness;
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
    float shadow = 0.0;
    if (keyShadowEnabled != 0)
    {
        float actor = texture(keyActorVisibility, vary_fragcoord).r;
        float other = keyShadowExact != 0 ? texture(keySetVisibility, vary_fragcoord).r : 1.0;
        shadow = keyShadowFiltered != 0 ? actor : (1.0 - actor) * other;
        shadow *= 1.0 - m;
        shadow = pow(clamp(shadow, 0.0, 1.0), 1.0 / max(keyShadowSoftness, 0.05));
    }
    frag_color = keyDebug != 0 ? vec4(m, m, m, 1.0) : vec4(m, shadow, 0.0, 1.0);
}
