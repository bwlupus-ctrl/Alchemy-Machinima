// [ActorMask] Discard before calling the unchanged stock light body.
/*[EXTRA_CODE_HERE]*/
#undef main
uniform sampler2D actorMaskMap;
uniform int actor_mask_bits;
void actorMaskedMain();
void main()
{
    uint b = uint(texelFetch(actorMaskMap, ivec2(gl_FragCoord.xy), 0).r * 255.0 + 0.5);
    if ((b & uint(actor_mask_bits)) == 0u) discard;
    actorMaskedMain();
}
