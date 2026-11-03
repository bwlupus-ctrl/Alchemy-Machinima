// [ActorKey] The display is set content, independent of displayed feed/mode.
out vec4 frag_color;
uniform float edgeFeather;
void main()
{
    // Same reserved discard as prismSignalF.glsl. Letterbox bars are opaque.
    if (edgeFeather < 0.0) discard;
    frag_color = vec4(0.0);
}
