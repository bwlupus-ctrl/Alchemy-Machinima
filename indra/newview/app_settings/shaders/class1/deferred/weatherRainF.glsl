/**
 * @file weatherRainF.glsl
 * @brief World-anchored, depth-clamped precipitation composite.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;
in vec2 vary_fragcoord;

uniform mat4  weather_inv_modelview;
uniform vec3  weather_wind;
uniform vec3  weather_rain_color;
uniform float weather_time;
uniform float weather_intensity;
uniform float weather_density;
uniform float weather_fall_speed;
uniform float weather_max_distance;
uniform float weather_lightning;
uniform float weather_frame;
uniform int   weather_samples;

vec4 getPosition(vec2 pos_screen);

const int WEATHER_MAX_SAMPLES = 24;

float weatherHash21(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float weatherGradientNoise(vec2 p)
{
    return fract(52.9829189 *
                 fract(dot(p, vec2(0.06711056, 0.00583715))));
}

float weatherDropField(vec3 world_pos, vec3 fall_dir, vec3 side,
                       vec3 across)
{
    const float min_spacing = 0.24;
    const float max_spacing = 1.25;
    float spacing = mix(max_spacing, min_spacing, weather_density);

    vec2 transverse = vec2(dot(world_pos, side),
                           dot(world_pos, across));
    vec2 cell = floor(transverse / spacing);
    float seed = weatherHash21(cell);
    vec2 seed_offset = vec2(seed, fract(seed * 19.371));
    vec2 in_cell = fract(transverse / spacing + seed_offset) - 0.5;

    // A very narrow transverse footprint produces filaments rather than
    // snow-like dots. radius includes fract() plus a per-cell hash, so its
    // derivative is discontinuous at cell boundaries and grows rapidly for
    // distant half-resolution samples. Never let that derivative expand one
    // filament across most of a cell; distant sub-pixel drops should disappear
    // instead of accumulating into a uniform screen wash.
    float radius = length(in_cell);
    const float filament_radius = 0.035;
    float aa = clamp(fwidth(radius), 0.008, filament_radius);
    float filament = 1.0 - smoothstep(
        filament_radius - aa, filament_radius + aa, radius);

    float along = dot(world_pos, fall_dir);
    float period = mix(11.0, 4.0, weather_density);
    float phase = abs(fract(
        (along - weather_time * weather_fall_speed) / period + seed) - 0.5);
    float streak_fraction = mix(0.055, 0.19, weather_intensity);
    float streak = 1.0 - smoothstep(streak_fraction,
                                    streak_fraction + 0.025, phase);

    // A second hash rejects some columns. Density changes the population,
    // while spacing changes their apparent separation.
    float population = step(1.0 - weather_density,
                            weatherHash21(cell + vec2(17.0, 43.0)));
    return filament * streak * population;
}

void main()
{
    vec2 tc = vary_fragcoord.xy;
    vec3 surface_view = getPosition(tc).xyz;
    float surface_distance = length(surface_view);
    float max_distance = max(weather_max_distance, 1.0);

    if (!(surface_distance > 0.0) || weather_intensity <= 0.0)
    {
        frag_color = vec4(0.0);
        return;
    }

    float ray_end = min(surface_distance, max_distance);
    if (ray_end <= 0.75)
    {
        frag_color = vec4(0.0);
        return;
    }

    vec3 ray_dir = surface_view / surface_distance;
    vec3 fall_dir = normalize(vec3(weather_wind.xy,
                                   -max(weather_fall_speed, 1.0)));
    vec3 reference = abs(fall_dir.z) < 0.95
        ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 side = normalize(cross(fall_dir, reference));
    vec3 across = normalize(cross(fall_dir, side));

    int sample_count = clamp(weather_samples, 4, WEATHER_MAX_SAMPLES);
    float jitter = weatherGradientNoise(gl_FragCoord.xy +
                                         vec2(weather_frame * 0.071));
    float sum = 0.0;
    float weight_sum = 0.0;

    for (int i = 0; i < WEATHER_MAX_SAMPLES; ++i)
    {
        if (i >= sample_count)
        {
            break;
        }

        float u = (float(i) + jitter) / float(sample_count);
        // Quadratic distribution spends more work close to the camera, where
        // parallax and readable individual streaks matter most.
        float t = mix(0.75, ray_end, u * u);
        vec3 view_pos = ray_dir * t;
        vec3 world_pos =
            (weather_inv_modelview * vec4(view_pos, 1.0)).xyz;

        float distance_weight = 1.0 - t / max_distance;
        distance_weight *= distance_weight;
        float sample_value =
            weatherDropField(world_pos, fall_dir, side, across);
        sum += sample_value * distance_weight;
        weight_sum += distance_weight;
    }

    float rain = weight_sum > 1.0e-5 ? sum / weight_sum : 0.0;
    // Lightning raises the water highlight without turning the streaks into
    // opaque white cards.
    vec3 color = weather_rain_color *
                 mix(1.0, 2.1, clamp(weather_lightning, 0.0, 1.0));
    float energy = min(rain * weather_intensity * 1.5, 0.25);
    // This pass feeds luminance adaptation and bloom. Bound the final RGB delta,
    // including user color and lightning coupling, so even a degenerate field can
    // only add a restrained highlight rather than dominate the HDR scene. Scale
    // all channels together at the ceiling so the configured rain hue survives.
    vec3 contribution = color * energy;
    float peak = max(contribution.r, max(contribution.g, contribution.b));
    contribution *= min(1.0, 0.25 / max(peak, 1.0e-5));
    frag_color = vec4(contribution, 0.0);
}
