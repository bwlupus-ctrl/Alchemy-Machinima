// [ActorMask] WP7 debug view of the per-view R8 actor mask (main view only).
// actor_mask_debug_mode 1: tint masked pixels per recipient bit over the scene
//                          (unmasked pixels discarded, blended by the caller).
// actor_mask_debug_mode 2: mask only. Every pixel is written: byte 0 is black,
//                          any other byte shows its bits through the same
//                          per-bit palette (a raw 1/255 grey is invisible).
//                          A mask that was never cleared to 0 shows no black.
/*[EXTRA_CODE_HERE]*/
uniform sampler2D actorMaskMap;
uniform int actor_mask_debug_mode;
out vec4 frag_color;

vec3 actorMaskBitColor(int bit)
{
    if (bit == 0) return vec3(1.0, 0.15, 0.15);
    if (bit == 1) return vec3(0.15, 1.0, 0.15);
    if (bit == 2) return vec3(0.2, 0.4, 1.0);
    if (bit == 3) return vec3(1.0, 0.9, 0.1);
    if (bit == 4) return vec3(1.0, 0.2, 1.0);
    if (bit == 5) return vec3(0.1, 1.0, 1.0);
    if (bit == 6) return vec3(1.0, 0.55, 0.1);
    return vec3(1.0, 1.0, 1.0);
}

void main()
{
    uint b = uint(texelFetch(actorMaskMap, ivec2(gl_FragCoord.xy), 0).r * 255.0 + 0.5);
    vec3 tint = vec3(0.0);
    float n = 0.0;
    for (int i = 0; i < 8; ++i)
    {
        if ((b & (1u << uint(i))) != 0u)
        {
            tint += actorMaskBitColor(i);
            n += 1.0;
        }
    }
    if (n > 0.0)
    {
        tint /= n;
    }
    if (actor_mask_debug_mode == 2)
    {
        frag_color = vec4(tint, 1.0);
        return;
    }
    if (b == 0u)
    {
        discard;
    }
    frag_color = vec4(tint, 0.5);
}
