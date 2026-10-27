in vec2 chart_uv;
out vec4 frag_color;
uniform vec3 chart_colour;
uniform int chart_sample;
uniform int chart_exposed;
uniform vec4 chart_viewport;
uniform sampler2D diffuseRect;
uniform sampler2D exposureMap;
uniform float exposure;
uniform int tonemap_type;
void main()
{
    if (chart_sample != 0)
    {
        vec2 uv = chart_sample == 2 ? (gl_FragCoord.xy-chart_viewport.xy)/chart_viewport.zw : chart_uv;
        frag_color = texture(diffuseRect, uv);
        return;
    }
    float scale = 1.0;
    if (chart_exposed != 0)
    {
        // Same scale as tonemapUtilF::applyExposure. EV belongs to the encoder.
        float adaptation = tonemap_type == 8 ? 1.0 : texture(exposureMap, vec2(0.5)).r;
        scale = exposure * adaptation;
    }
    frag_color = vec4(chart_colour * scale, 1.0);
}
