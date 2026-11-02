/** Last scene overlay: uncovered pixels never write colour, alpha or depth. */
/*[EXTRA_CODE_HERE]*/
out vec4 frag_color;
in vec2 vary_fragcoord;
uniform sampler2D diffuseMap;
uniform vec2 screen_res;
uniform vec4 blink_shape; // closure, curve, feather, warm
uniform vec2 blink_look; // darken, blur
uniform int blink_sampling;
uniform int uDitherBits;
vec3 sample_screen(vec2 uv)
{
    return clamp(texture(diffuseMap, clamp(uv, 0.0, 1.0)).rgb, 0.0, 1.0);
}
void main()
{
    vec2 uv = vary_fragcoord;
    float c = clamp(blink_shape.x, 0.0, 1.0);
    if (c <= 0.0) discard;
    float feather = max(blink_shape.z * .15, 1.0 / screen_res.y);
    float x = (uv.x - .5) * screen_res.x / screen_res.y;
    float arch = blink_shape.y * x*x / (1.0 + x*x);
    float top = mix(1.0 + feather, .42 - feather, c) - arch*c*(1.0-c);
    float bottom = mix(-feather, .42 + feather, c) + arch*c*(1.0-c);
    float distance_to_lid = min(top-uv.y, uv.y-bottom);
    float mask = 1.0 - smoothstep(-feather, feather, distance_to_lid);
    if (mask <= 0.0) discard;
    vec3 lid = vec3(.004, .003, .005);
    if (blink_sampling != 0)
    {
        vec3 image = sample_screen(uv);
        float near_edge = exp(-abs(distance_to_lid) / max(.015, feather*4.0));
        float blur = blink_look.y * c * (.25 + .75*near_edge);
        if (blur > 0.0)
        {
            vec2 r = vec2(1.0 / screen_res.x, 1.0 / screen_res.y) * (1.0 + 10.0*blur);
            vec3 gather = sample_screen(uv+vec2(r.x,r.y));
            gather += sample_screen(uv+vec2(-r.x,r.y));
            gather += sample_screen(uv+vec2(r.x,-r.y));
            gather += sample_screen(uv-r);
            image = mix(image, gather*.25, blur);
        }
        image *= 1.0 - blink_look.x * c * (.25 + .75*near_edge);
        vec3 warm_lid = vec3(.11, .022, .006) + image*.055;
        float n1 = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898,78.233)))*43758.5453);
        float n2 = fract(sin(dot(gl_FragCoord.yx, vec2(39.3468,11.135)))*24634.6345);
        // Small TPDF dither on transmitted light, before coverage blending.
        warm_lid += vec3((n1-n2)/(uDitherBits >= 10 ? 1023.0 : 255.0));
        lid = mix(lid, warm_lid, blink_shape.w);
        image *= 1.0 - (1.0-blink_shape.w)*.28*c*near_edge;
        lid = mix(image, lid, mask);
    }
    frag_color = vec4(lid, mask);
}
