// [ActorMask] avatarF and the native deferred rigid-eye pass use texture alpha.
/*[EXTRA_CODE_HERE]*/
uniform sampler2D diffuseMap;
uniform float minimum_alpha;
uniform int actor_mask_bits;
in vec3 vary_position;
in vec2 vary_texcoord0;
out vec4 frag_color;
void mirrorClip(vec3 pos);
void main()
{
    mirrorClip(vary_position);
    if (texture(diffuseMap, vary_texcoord0).a < minimum_alpha) discard;
    frag_color = vec4(float(actor_mask_bits) / 255.0, 0.0, 0.0, 0.0);
}
