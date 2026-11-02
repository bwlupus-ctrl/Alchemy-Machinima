/** Live POV eyelids, after all colour/present effects and before UI. */
/*[EXTRA_CODE_HERE]*/
out vec4 frag_color;
in vec2 vary_fragcoord;
uniform sampler2D diffuseMap;
uniform sampler2D depthMap;
uniform vec2 screen_res;
// closure, curve, feather (frame-height units), warm
uniform vec4 blink_shape;
// darken, blur
uniform vec2 blink_look;

void main()
{
    vec2 uv = vary_fragcoord;
    gl_FragDepth = texture(depthMap, uv).r;
    float c = clamp(blink_shape.x, 0.0, 1.0);
    vec3 image = texture(diffuseMap, uv).rgb;
    // Fixture can exercise identity; production skips the entire pass at zero.
    if (c <= 0.0) { frag_color = vec4(image, 1.0); return; }
    float feather = max(blink_shape.z * .15, 1.0 / screen_res.y);
    // Circle-like arch in height units: curvature stays physical on ultrawide.
    float x = (uv.x - .5) * screen_res.x / screen_res.y;
    float arch = blink_shape.y * x*x / (1.0 + x*x);
    // Upper lid travels 58%, lower 42%; seam is slightly below centre.
    float top = mix(1.0 + feather, .42 - feather, c) - arch*c*(1.0-c);
    float bottom = mix(-feather, .42 + feather, c) + arch*c*(1.0-c);
    // Signed distance to the open gap. At full closure edges overlap by 2f:
    // every pixel has mask 1, including the meeting line and all corners.
    float distance_to_lid = min(top-uv.y, uv.y-bottom);
    float mask = 1.0 - smoothstep(-feather, feather, distance_to_lid);
    float near_edge = exp(-abs(distance_to_lid) / max(.015, feather*4.0));
    float blur = blink_look.y * c * (.25 + .75*near_edge);
    if (blur > 0.0)
    {
        vec2 r = vec2(1.0 / screen_res.x, 1.0 / screen_res.y) * (1.0 + 10.0*blur);
        vec3 gather = texture(diffuseMap, clamp(uv+vec2(r.x,r.y), 0.0, 1.0)).rgb;
        gather += texture(diffuseMap, clamp(uv+vec2(-r.x,r.y), 0.0, 1.0)).rgb;
        gather += texture(diffuseMap, clamp(uv+vec2(r.x,-r.y), 0.0, 1.0)).rgb;
        gather += texture(diffuseMap, clamp(uv-r, 0.0, 1.0)).rgb;
        image = mix(image, gather*.25, blur);
    }
    image *= 1.0 - blink_look.x * c * (.25 + .75*near_edge);
    vec3 dark_lid = vec3(.004, .003, .005);
    // Faint transmitted daylight, with a little scene transmission through skin.
    vec3 warm_lid = vec3(.11, .022, .006) + image*.055;
    vec3 lid = mix(dark_lid, warm_lid, blink_shape.w);
    // Subtle lash shadow on the visible gap side of the Dark lid edge.
    image *= 1.0 - (1.0-blink_shape.w)*.28*c*near_edge;
    frag_color = vec4(mix(image, lid, mask), 1.0);
}
