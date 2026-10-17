in vec3 position;
out vec2 media_uv;
void main()
{
    media_uv = position.xy * 0.5 + vec2(0.5);
    gl_Position = vec4(position.xy, 0.0, 1.0);
}
