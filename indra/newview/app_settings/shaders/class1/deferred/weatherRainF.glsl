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
uniform float weather_virtual_shutter;
uniform float weather_near_emphasis;
uniform float weather_gust_strength;
uniform int   weather_layers;
uniform int   weather_samples;

#ifdef WEATHER_RAIN_OCCLUSION
uniform sampler2D weather_rain_occlusion_map;
uniform mat4  weather_rain_occlusion_matrix;
uniform float weather_rain_occlusion_depth_range;
uniform float weather_rain_occlusion_bias;
uniform float weather_rain_occlusion_softness;
uniform int   weather_rain_occlusion_enabled;
#endif

vec4 getPosition(vec2 pos_screen);

const int WEATHER_MAX_SAMPLES = 24;

#ifdef WEATHER_RAIN_OCCLUSION
float weatherRainExposure(vec3 world_pos)
{
    if (weather_rain_occlusion_enabled == 0 ||
        !(weather_rain_occlusion_depth_range > 0.0) ||
        isnan(weather_rain_occlusion_depth_range) ||
        isinf(weather_rain_occlusion_depth_range) ||
        isnan(weather_rain_occlusion_bias) ||
        isinf(weather_rain_occlusion_bias) ||
        isnan(weather_rain_occlusion_softness) ||
        isinf(weather_rain_occlusion_softness))
    {
        return 1.0;
    }

    vec4 clip = weather_rain_occlusion_matrix * vec4(world_pos, 1.0);
    if (any(isnan(clip)) || any(isinf(clip)) ||
        !(abs(clip.w) > 1.0e-6))
    {
        return 1.0;
    }

    vec3 ndc = clip.xyz / clip.w;
    if (any(isnan(ndc)) || any(isinf(ndc)) ||
        any(lessThan(ndc, vec3(-1.0))) ||
        any(greaterThan(ndc, vec3(1.0))))
    {
        return 1.0;
    }

    vec2 uv = ndc.xy * 0.5 + 0.5;
    float point_depth = ndc.z * 0.5 + 0.5;
    ivec2 map_size = textureSize(weather_rain_occlusion_map, 0);
    if (any(lessThanEqual(map_size, ivec2(0))))
    {
        return 1.0;
    }

    // texelFetch is deliberate: filtering raw depths across an eave invents
    // an occluder height that does not exist. Surface response performs
    // compare-first filtering; the rain march uses one stable nearest tap.
    ivec2 map_texel = clamp(
        ivec2(floor(uv * vec2(map_size))),
        ivec2(0), map_size - ivec2(1));
    float map_depth = texelFetch(
        weather_rain_occlusion_map, map_texel, 0).r;
    if (isnan(map_depth) || isinf(map_depth))
    {
        return 1.0;
    }

    float cover_height =
        (point_depth - map_depth) * weather_rain_occlusion_depth_range;
    float bias = max(weather_rain_occlusion_bias, 0.0);
    float softness = max(weather_rain_occlusion_softness, 1.0e-4);
    float exposure =
        1.0 - smoothstep(bias, bias + softness, cover_height);

    // The final map texels fail open, avoiding a hard rain wall when the
    // quantized camera-centered map moves across the scene.
    float edge_texels =
        min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y)) *
        float(min(map_size.x, map_size.y));
    float interior = smoothstep(1.0, 3.0, edge_texels);
    return clamp(mix(1.0, exposure, interior), 0.0, 1.0);
}
#endif

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

vec3 weatherSafeNormalize(vec3 value, vec3 fallback)
{
    float length_squared = dot(value, value);
    if (!(length_squared > 1.0e-8) ||
        any(isnan(value)) || any(isinf(value)))
    {
        return fallback;
    }
    return value * inversesqrt(length_squared);
}

vec2 weatherGust(vec3 world_pos)
{
    float p = dot(world_pos.xy, vec2(0.021, 0.017)) +
              weather_time * 0.31;
    float q = dot(world_pos.xy, vec2(-0.013, 0.029)) -
              weather_time * 0.23;
    vec2 gust = vec2(sin(p) + 0.5 * sin(q * 2.13),
                     cos(q) + 0.5 * cos(p * 1.71)) / 1.5;
    return clamp(gust, vec2(-1.0), vec2(1.0));
}

vec3 weatherSafeContribution(vec3 contribution, float ceiling)
{
    if (any(isnan(contribution)) || any(isinf(contribution)))
    {
        return vec3(0.0);
    }
    contribution = max(contribution, vec3(0.0));
    float peak = max(contribution.r,
                     max(contribution.g, contribution.b));
    contribution *= min(1.0, ceiling / max(peak, 1.0e-5));
    return contribution;
}

float weatherDropField(vec3 world_pos, float distance_fraction)
{
    const float min_spacing = 0.24;
    const float max_spacing = 1.25;
    int layer_count = clamp(weather_layers, 1, 3);
    int layer = 1;
    if (layer_count == 2)
    {
        layer = distance_fraction < 0.38 ? 0 : 2;
    }
    else if (layer_count == 3)
    {
        layer = min(2, int(floor(distance_fraction * 3.0)));
    }

    // Near drops are sparse and substantial, while distant layers contain
    // finer populations. They are still evaluated in one ray march, so adding
    // layers changes the distribution rather than tripling shader work.
    float layer_scale = layer == 0 ? 1.55 : (layer == 1 ? 0.95 : 0.58);
    float near_shape = 1.0 +
        weather_near_emphasis *
        (1.0 - distance_fraction) * (1.0 - distance_fraction);
    float spacing = mix(max_spacing, min_spacing, weather_density) *
                    layer_scale;

    vec2 gust = weatherGust(world_pos) * weather_gust_strength;
    float gust_speed = max(2.0, length(weather_wind.xy) + 2.0);
    vec3 fall_dir = weatherSafeNormalize(
        vec3(weather_wind.xy + gust * gust_speed,
             -max(weather_fall_speed, 1.0)),
        vec3(0.0, 0.0, -1.0));
    vec3 reference = abs(fall_dir.z) < 0.95
        ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 side = weatherSafeNormalize(
        cross(fall_dir, reference), vec3(1.0, 0.0, 0.0));
    vec3 across = weatherSafeNormalize(
        cross(fall_dir, side), vec3(0.0, 1.0, 0.0));

    vec2 transverse = vec2(dot(world_pos, side),
                           dot(world_pos, across));
    vec2 cell = floor(transverse / spacing);
    vec2 layer_seed = vec2(float(layer) * 47.0,
                           float(layer) * -31.0);
    float seed = weatherHash21(cell + layer_seed);
    vec2 seed_offset = vec2(seed, fract(seed * 19.371));
    vec2 in_cell = fract(transverse / spacing + seed_offset) - 0.5;

    // A very narrow transverse footprint produces filaments rather than
    // snow-like dots. radius includes fract() plus a per-cell hash, so its
    // derivative is discontinuous at cell boundaries and grows rapidly for
    // distant half-resolution samples. Never let that derivative expand one
    // filament across most of a cell; distant sub-pixel drops should disappear
    // instead of accumulating into a uniform screen wash.
    float radius = length(in_cell);
    float filament_radius = min(0.09, 0.035 * near_shape);
    float aa = clamp(fwidth(radius), 0.008, filament_radius);
    float filament = 1.0 - smoothstep(
        filament_radius - aa, filament_radius + aa, radius);

    float along = dot(world_pos, fall_dir);
    float period = mix(11.0, 4.0, weather_density);
    float phase = abs(fract(
        (along - weather_time * weather_fall_speed) / period + seed) - 0.5);
    // A shutter interval produces a world-space motion trail. Projection of
    // that trail then supplies the correct near-to-far apparent shortening.
    float streak_length = clamp(
        weather_fall_speed * weather_virtual_shutter * near_shape,
        0.08, period * 0.82);
    float streak_fraction = clamp(
        0.5 * streak_length / max(period, 1.0e-3), 0.01, 0.41);
    float streak = 1.0 - smoothstep(streak_fraction,
                                    streak_fraction + 0.025, phase);

    // A second hash rejects some columns. Density changes the population,
    // while spacing changes their apparent separation.
    float population = step(1.0 - weather_density,
                            weatherHash21(
                                cell + layer_seed + vec2(17.0, 43.0)));
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

    vec3 ray_dir = weatherSafeNormalize(
        surface_view, vec3(0.0, 0.0, -1.0));

    int sample_count = clamp(weather_samples, 4, WEATHER_MAX_SAMPLES);
    // Stable per-pixel jitter avoids frame-count-driven shimmer. All actual
    // animation is a pure function of the presentation clock.
    float jitter = weatherGradientNoise(
        gl_FragCoord.xy + vec2(37.0, 17.0));
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

        float distance_fraction = clamp(t / max_distance, 0.0, 1.0);
        float distance_weight = 1.0 - distance_fraction;
        distance_weight *= distance_weight;
        float near_weight = 1.0 + weather_near_emphasis * 1.5 *
            (1.0 - distance_fraction) * (1.0 - distance_fraction);
        float sample_value = weatherDropField(
            world_pos, distance_fraction) * near_weight;
#ifdef WEATHER_RAIN_OCCLUSION
        if (sample_value > 0.0)
        {
            sample_value *= weatherRainExposure(world_pos);
        }
#endif
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
    vec3 contribution = weatherSafeContribution(color * energy, 0.25);
    frag_color = vec4(contribution, 0.0);
}
