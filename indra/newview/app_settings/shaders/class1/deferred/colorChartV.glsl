// Float uniform colours: LLRender's byte vertex colours cannot represent 0.18.
in vec3 position;
uniform mat4 chart_matrix;
out vec2 chart_uv;
void main()
{
    chart_uv = position.xy * 0.5 + 0.5;
    gl_Position = chart_matrix * vec4(position, 1.0);
}
