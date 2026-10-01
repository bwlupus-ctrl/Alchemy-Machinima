// [ActorMask] CPU owner union is an exact R8 overwrite.
/*[EXTRA_CODE_HERE]*/
uniform int actor_mask_bits;
uniform sampler2D mask_textures[MASK_CHANNELS];
uniform float mask_cutoff[8];
uniform int mask_material_slots;
uniform int mask_vertex_alpha;
in vec3 vary_position;
in vec2 vary_texcoord0;
in vec4 vertex_color;
flat in int mask_slot;
out vec4 frag_color;
void mirrorClip(vec3 pos);
float authoredAlpha()
{
    switch (mask_slot)
    {
#if MASK_CHANNELS > 0
    case 0: return texture(mask_textures[0], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 1
    case 1: return texture(mask_textures[1], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 2
    case 2: return texture(mask_textures[2], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 3
    case 3: return texture(mask_textures[3], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 4
    case 4: return texture(mask_textures[4], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 5
    case 5: return texture(mask_textures[5], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 6
    case 6: return texture(mask_textures[6], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 7
    case 7: return texture(mask_textures[7], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 8
    case 8: return texture(mask_textures[8], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 9
    case 9: return texture(mask_textures[9], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 10
    case 10: return texture(mask_textures[10], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 11
    case 11: return texture(mask_textures[11], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 12
    case 12: return texture(mask_textures[12], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 13
    case 13: return texture(mask_textures[13], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 14
    case 14: return texture(mask_textures[14], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 15
    case 15: return texture(mask_textures[15], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 16
    case 16: return texture(mask_textures[16], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 17
    case 17: return texture(mask_textures[17], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 18
    case 18: return texture(mask_textures[18], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 19
    case 19: return texture(mask_textures[19], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 20
    case 20: return texture(mask_textures[20], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 21
    case 21: return texture(mask_textures[21], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 22
    case 22: return texture(mask_textures[22], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 23
    case 23: return texture(mask_textures[23], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 24
    case 24: return texture(mask_textures[24], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 25
    case 25: return texture(mask_textures[25], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 26
    case 26: return texture(mask_textures[26], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 27
    case 27: return texture(mask_textures[27], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 28
    case 28: return texture(mask_textures[28], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 29
    case 29: return texture(mask_textures[29], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 30
    case 30: return texture(mask_textures[30], vary_texcoord0).a;
#endif
#if MASK_CHANNELS > 31
    case 31: return texture(mask_textures[31], vary_texcoord0).a;
#endif
    default: return 0.0;
    }
}
void main()
{
    mirrorClip(vary_position);
    if (mask_slot < 0 || mask_slot >= MASK_CHANNELS) discard;
    int mi = mask_material_slots != 0 ? mask_slot : 0;
    if (mi >= 8) discard;
    float alpha = authoredAlpha();
    if (mask_vertex_alpha != 0) alpha *= vertex_color.a;
    if (alpha < mask_cutoff[mi]) discard;
    frag_color = vec4(float(actor_mask_bits) / 255.0, 0.0, 0.0, 0.0);
}
