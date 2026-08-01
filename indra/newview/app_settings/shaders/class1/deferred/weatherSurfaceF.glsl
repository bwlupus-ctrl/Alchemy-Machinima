/**
 * @file weatherSurfaceF.glsl
 * @brief Stateless rain impacts, wet sheen, ground mist, and lens caustics.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;
in vec2 vary_fragcoord;

uniform mat4  weather_inv_modelview;
uniform vec3  weather_surface_color;
uniform vec2  weather_screen_res;
uniform float weather_time;
uniform float weather_intensity;
uniform float weather_lightning;
uniform float weather_gust_strength;
uniform float weather_ground_height;

uniform int   weather_splash_enabled;
uniform float weather_splash_density;
uniform float weather_splash_ring_size;
uniform float weather_splash_lifetime;
uniform float weather_splash_up_threshold;
uniform float weather_splash_max_distance;

uniform int   weather_wetness_enabled;
uniform float weather_wetness_strength;

uniform int   weather_mist_enabled;
uniform float weather_mist_strength;
uniform float weather_mist_height;

uniform int   weather_lens_enabled;
uniform float weather_lens_strength;

#ifdef WEATHER_RAIN_OCCLUSION
uniform sampler2D weather_rain_occlusion_map;
uniform mat4  weather_rain_occlusion_matrix;
uniform float weather_rain_occlusion_depth_range;
uniform float weather_rain_occlusion_bias;
uniform float weather_rain_occlusion_softness;
uniform int   weather_rain_occlusion_enabled;
uniform float weather_lens_exposure;
#endif

vec4 getPosition(vec2 pos_screen);
vec4 getNorm(vec2 pos_screen);

#ifdef WEATHER_RAIN_OCCLUSION
float weatherRainExposureTap(ivec2 map_texel, ivec2 map_size,
                             float point_depth)
{
    if (any(lessThan(map_texel, ivec2(0))) ||
        any(greaterThanEqual(map_texel, map_size)))
    {
        return 1.0;
    }

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
    return clamp(
        1.0 - smoothstep(bias, bias + softness, cover_height),
        0.0, 1.0);
}

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

    // Compare first, then bilinearly filter the four exposure results. Raw
    // depth interpolation would create phantom sloped blockers at roof edges.
    vec2 texel_position = uv * vec2(map_size) - 0.5;
    ivec2 base_texel = ivec2(floor(texel_position));
    vec2 fraction = fract(texel_position);
    float e00 = weatherRainExposureTap(
        base_texel + ivec2(0, 0), map_size, point_depth);
    float e10 = weatherRainExposureTap(
        base_texel + ivec2(1, 0), map_size, point_depth);
    float e01 = weatherRainExposureTap(
        base_texel + ivec2(0, 1), map_size, point_depth);
    float e11 = weatherRainExposureTap(
        base_texel + ivec2(1, 1), map_size, point_depth);
    float exposure = mix(
        mix(e00, e10, fraction.x),
        mix(e01, e11, fraction.x), fraction.y);

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

vec2 weatherHash22(vec2 p)
{
    float n = weatherHash21(p);
    return vec2(n, weatherHash21(p + vec2(31.7, 91.1)));
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

float weatherSplashEvent(vec2 world_xz, vec2 cell, float bucket,
                         float cycle, float lifetime, float cell_size,
                         float probability)
{
    vec2 event_key = cell + vec2(bucket * 17.0, bucket * -11.0);
    float seed = weatherHash21(event_key);
    if (seed >= probability)
    {
        return 0.0;
    }

    vec2 center_jitter = mix(vec2(0.18), vec2(0.82),
                             weatherHash22(event_key + vec2(7.0, 19.0)));
    vec2 center = (cell + center_jitter) * cell_size;
    float start = (bucket + weatherHash21(
        event_key + vec2(53.0, 29.0))) * cycle;
    float age = (weather_time - start) / max(lifetime, 1.0e-3);
    if (!(age >= 0.0 && age < 1.0))
    {
        return 0.0;
    }

    float radius = weather_splash_ring_size *
                   mix(0.08, 1.0, smoothstep(0.0, 1.0, age));
    float distance_to_center = length(world_xz - center);
    float ring_width = max(weather_splash_ring_size * 0.055, 0.004);
    float ring = 1.0 - smoothstep(
        ring_width, ring_width * 2.2,
        abs(distance_to_center - radius));

    // A short-lived radial crown suggests micro-splash droplets without a
    // geometry or particle pass. It remains surface-clamped by construction.
    vec2 radial = (world_xz - center) /
                  max(weather_splash_ring_size, 1.0e-3);
    float crown_radius = length(radial);
    vec2 angle_vector = crown_radius > 1.0e-5
        ? radial / crown_radius : vec2(1.0, 0.0);
    float angle = atan(angle_vector.y, angle_vector.x);
    float spokes = pow(max(0.0, cos(angle * 6.0 + seed * 6.2831853)), 10.0);
    float crown = spokes *
        (1.0 - smoothstep(0.08, 0.38, crown_radius)) *
        (1.0 - smoothstep(0.0, 0.28, age));

    float fade = (1.0 - age) * (1.0 - age) *
                 smoothstep(0.0, 0.06, age);
    return min((ring + crown * 0.7) * fade, 1.0);
}

float weatherLensField(vec2 tc)
{
    vec2 resolution = max(weather_screen_res, vec2(1.0));
    float aspect = resolution.x / resolution.y;
    vec2 p = vec2(tc.x * aspect, tc.y);
    const float lens_scale = 6.0;
    vec2 base_cell = floor(p * lens_scale);
    float result = 0.0;

    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            vec2 cell = base_cell + vec2(float(x), float(y));
            float seed = weatherHash21(cell + vec2(101.0, 37.0));
            if (seed > min(0.72, 0.12 + weather_lens_strength * 0.6))
            {
                continue;
            }

            float age = fract(weather_time *
                mix(0.035, 0.085, weatherHash21(cell + 5.0)) + seed);
            vec2 offset = weatherHash22(cell + vec2(13.0, 71.0));
            vec2 center = (cell + offset) / lens_scale;
            center.y -= age * mix(0.03, 0.15, seed);

            vec2 delta = p - center;
            float drop_radius = mix(0.018, 0.055, seed);
            vec2 ellipse = delta /
                vec2(drop_radius, drop_radius * mix(1.0, 1.8, age));
            float distance_to_drop = length(ellipse);
            float aa = max(
                1.0 / (min(resolution.x, resolution.y) *
                       max(drop_radius, 1.0e-3)),
                0.02);
            float rim = 1.0 - smoothstep(
                aa * 1.2, aa * 3.0, abs(distance_to_drop - 1.0));
            float lower_glint = smoothstep(0.05, 0.55, -ellipse.y) *
                                (1.0 - smoothstep(0.55, 1.0, -ellipse.y));
            float life_fade = smoothstep(0.0, 0.08, age) *
                              (1.0 - smoothstep(0.78, 1.0, age));
            result = max(result, rim * (0.45 + lower_glint) * life_fade);
        }
    }
    return min(result, 1.0);
}

void main()
{
    vec2 tc = vary_fragcoord.xy;
    vec3 contribution = vec3(0.0);

    if (weather_lens_enabled != 0 && weather_lens_strength > 0.0)
    {
        float lens = weatherLensField(tc) *
                     weather_lens_strength * weather_intensity;
#ifdef WEATHER_RAIN_OCCLUSION
        // Lens drops have one camera-space exposure, not one world-space
        // exposure per fragment. The CPU camera ray supplies this uniform so
        // the full-screen pass does not repeat an identical map lookup.
        lens *= clamp(weather_lens_exposure, 0.0, 1.0);
#endif
        contribution += weatherSafeContribution(
            vec3(0.62, 0.74, 0.86) * lens * 0.08, 0.08);
    }

#ifdef WEATHER_RAIN_OCCLUSION
    if ((weather_splash_enabled == 0 ||
         !(weather_splash_density > 0.0)) &&
        (weather_wetness_enabled == 0 ||
         !(weather_wetness_strength > 0.0)) &&
        (weather_mist_enabled == 0 ||
         !(weather_mist_strength > 0.0)))
    {
        frag_color = vec4(
            weatherSafeContribution(contribution, 0.18), 0.0);
        return;
    }
#endif

    vec3 surface_view = getPosition(tc).xyz;
    float surface_distance = length(surface_view);
    bool valid_surface =
        surface_distance > 0.0 &&
        !any(isnan(surface_view)) && !any(isinf(surface_view));
    if (!valid_surface || weather_intensity <= 0.0)
    {
        frag_color = vec4(
            weatherSafeContribution(contribution, 0.18), 0.0);
        return;
    }

    vec3 world_pos =
        (weather_inv_modelview * vec4(surface_view, 1.0)).xyz;
    if (any(isnan(world_pos)) || any(isinf(world_pos)))
    {
        frag_color = vec4(
            weatherSafeContribution(contribution, 0.18), 0.0);
        return;
    }
    vec3 normal_view = weatherSafeNormalize(
        getNorm(tc).xyz, vec3(0.0, 0.0, 1.0));
    vec3 normal_world = weatherSafeNormalize(
        mat3(weather_inv_modelview) * normal_view,
        vec3(0.0, 0.0, 1.0));
    float threshold = clamp(weather_splash_up_threshold, 0.0, 0.98);
    float up_facing = smoothstep(
        threshold, min(1.0, threshold + 0.12), normal_world.z);

#ifdef WEATHER_RAIN_OCCLUSION
    // Do not pay four map fetches for a surface fragment unless at least one
    // world-space rain response can contribute. Lens-only frames returned
    // before the G-buffer fetch above and use the CPU camera exposure.
    bool splash_candidate =
        weather_splash_enabled != 0 &&
        weather_splash_density > 0.0 &&
        up_facing > 0.0 &&
        surface_distance <= max(weather_splash_max_distance, 1.0);
    bool wetness_candidate =
        weather_wetness_enabled != 0 &&
        weather_wetness_strength > 0.0 &&
        up_facing > 0.0 &&
        surface_distance <= max(weather_splash_max_distance, 1.0);
    float mist_candidate_height = max(weather_mist_height, 0.25);
    bool mist_candidate =
        weather_mist_enabled != 0 &&
        weather_mist_strength > 0.0 &&
        abs(world_pos.z - weather_ground_height) < mist_candidate_height &&
        surface_distance < 48.0;
    if (!splash_candidate && !wetness_candidate && !mist_candidate)
    {
        frag_color = vec4(
            weatherSafeContribution(contribution, 0.18), 0.0);
        return;
    }

    float rain_exposure = weatherRainExposure(world_pos);
    if (!(rain_exposure > 0.0))
    {
        frag_color = vec4(
            weatherSafeContribution(contribution, 0.18), 0.0);
        return;
    }
#endif

    float splash_value = 0.0;
    if (weather_splash_enabled != 0 && up_facing > 0.0 &&
        surface_distance <= max(weather_splash_max_distance, 1.0))
    {
        float ring_size = max(weather_splash_ring_size, 0.02);
        float lifetime = max(weather_splash_lifetime, 0.05);
        float cell_size = max(ring_size * 2.8, 0.08);
        vec2 cell = floor(world_pos.xy / cell_size);
        float cycle = max(lifetime * 1.35, 0.10);
        float bucket = floor(weather_time / cycle);
        vec2 gust = weatherGust(world_pos);
        float gust_energy = mix(
            1.0, clamp(0.75 + 0.35 * length(gust), 0.65, 1.35),
            weather_gust_strength);
        float probability = clamp(
            weather_splash_density * weather_intensity * gust_energy,
            0.0, 0.92);

        // Current and previous buckets overlap, avoiding a global pop when a
        // quantized event interval rolls over.
        splash_value = weatherSplashEvent(
            world_pos.xy, cell, bucket, cycle, lifetime, cell_size,
            probability);
        splash_value += weatherSplashEvent(
            world_pos.xy, cell, bucket - 1.0, cycle, lifetime, cell_size,
            probability);
        splash_value = min(splash_value * up_facing, 1.0);
#ifdef WEATHER_RAIN_OCCLUSION
        splash_value *= rain_exposure;
#endif

        vec3 splash_color = weather_surface_color *
            mix(1.0, 1.7, clamp(weather_lightning, 0.0, 1.0));
        contribution += weatherSafeContribution(
            splash_color * splash_value * weather_intensity * 0.13,
            0.13);
    }

    if (weather_wetness_enabled != 0 &&
        weather_wetness_strength > 0.0 && up_facing > 0.0 &&
        surface_distance <= max(weather_splash_max_distance, 1.0))
    {
        vec3 view_to_camera_world = weatherSafeNormalize(
            mat3(weather_inv_modelview) * (-surface_view),
            vec3(0.0, 0.0, 1.0));
        float ndv = clamp(dot(normal_world, view_to_camera_world), 0.0, 1.0);
        float fresnel_base = clamp(1.0 - ndv, 0.0, 1.0);
        float fresnel = fresnel_base * fresnel_base;
        fresnel *= fresnel * fresnel_base;
        float fine_ripple = 0.82 + 0.18 * sin(
            dot(world_pos.xy, vec2(19.1, 23.7)) +
            weather_time * 3.1);
        float wet_energy = weather_wetness_strength * weather_intensity *
            up_facing * (0.025 + 0.28 * fresnel) *
            mix(fine_ripple, 1.0, splash_value);
#ifdef WEATHER_RAIN_OCCLUSION
        wet_energy *= rain_exposure;
#endif
        contribution += weatherSafeContribution(
            weather_surface_color * wet_energy, 0.09);
    }

    if (weather_mist_enabled != 0 && weather_mist_strength > 0.0)
    {
        float height = max(weather_mist_height, 0.25);
        float height_delta = abs(world_pos.z - weather_ground_height);
        float height_fade = 1.0 - smoothstep(
            height * 0.15, height, height_delta);
        float distance_fade =
            1.0 - smoothstep(4.0, 48.0, surface_distance);
        float mist = weather_mist_strength * weather_intensity *
            height_fade * distance_fade * (0.35 + 0.65 * up_facing);
#ifdef WEATHER_RAIN_OCCLUSION
        mist *= rain_exposure;
#endif
        contribution += weatherSafeContribution(
            vec3(0.42, 0.50, 0.58) * mist * 0.08, 0.08);
    }

    frag_color = vec4(
        weatherSafeContribution(contribution, 0.18), 0.0);
}
