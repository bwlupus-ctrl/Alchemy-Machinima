/**
 * @file weatherRainUpsampleF.glsl
 * @brief Depth-aware bilateral resolve for low-resolution precipitation.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;
in vec2 vary_fragcoord;

uniform sampler2D weather_rain_map;
uniform vec2 weather_rain_map_res;

vec4 getPosition(vec2 pos_screen);

void main()
{
    vec2 tc = vary_fragcoord.xy;
    vec2 low_res = max(weather_rain_map_res, vec2(1.0));
    float full_depth = length(getPosition(tc).xyz);

    vec2 low_pos = tc * low_res - 0.5;
    vec2 base = floor(low_pos);
    vec2 fraction = fract(low_pos);
    float sigma = max(full_depth * 0.035, 0.08);

    vec3 sum = vec3(0.0);
    float weight_sum = 0.0;

    for (int y = 0; y < 2; ++y)
    {
        for (int x = 0; x < 2; ++x)
        {
            vec2 tap_texel = base + vec2(float(x), float(y)) + 0.5;
            vec2 tap_uv = clamp(tap_texel / low_res,
                                vec2(0.0), vec2(1.0));
            vec3 rain = texture(weather_rain_map, tap_uv).rgb;
            float tap_depth = length(getPosition(tap_uv).xyz);

            float wx = x == 0 ? 1.0 - fraction.x : fraction.x;
            float wy = y == 0 ? 1.0 - fraction.y : fraction.y;
            float footprint = wx * wy;
            float depth_weight =
                exp(-abs(full_depth - tap_depth) / sigma);
            float weight = footprint * depth_weight;
            sum += rain * weight;
            weight_sum += weight;
        }
    }

    vec3 rain = weight_sum > 1.0e-4
        ? sum / weight_sum
        : texture(weather_rain_map, tc).rgb;
    frag_color = vec4(rain, 0.0);
}
