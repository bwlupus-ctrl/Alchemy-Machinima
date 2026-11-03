// WP4: stock directional sampling, including subject column, sun/moon and PCF.
/*[EXTRA_CODE_HERE]*/
in vec2 vary_fragcoord;
out vec4 frag_color;
uniform vec3 sun_dir;
uniform vec3 moon_dir;
uniform int sun_up_factor;
vec4 getPosition(vec2 pos_screen);
vec4 getNorm(vec2 pos_screen);
float sampleDirectionalShadow(vec3 pos, vec3 norm, vec2 pos_screen);
void main()
{
    vec3 pos = getPosition(vary_fragcoord).xyz;
    vec3 norm = getNorm(vary_fragcoord).xyz;
    vec3 light = sun_up_factor == 1 ? sun_dir : moon_dir;
    float visibility = sampleDirectionalShadow(pos, norm, vary_fragcoord);
    // Store lit on back-facing receivers; they carry no additional actor shadow.
    visibility = dot(norm, light) >= 0.0 ? visibility : 1.0;
    frag_color = vec4(visibility, 0.0, 0.0, 1.0);
}
