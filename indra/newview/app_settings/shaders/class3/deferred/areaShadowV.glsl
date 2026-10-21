/** Fullscreen area-shadow triangle; no atmospheric varyings or history. */
/*[EXTRA_CODE_HERE]*/
in vec3 position;
out vec2 vary_fragcoord;
void main()
{
    gl_Position = vec4(position,1.0);
    vary_fragcoord = position.xy*0.5+0.5;
}
