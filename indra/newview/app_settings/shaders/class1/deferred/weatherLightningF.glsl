/**
 * @file weatherLightningF.glsl
 * @brief World-space procedural lightning bolt and deferred surface flash.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;
in vec2 vary_fragcoord;

uniform mat4  weather_world_to_view;
uniform mat4  weather_view_projection;
uniform vec3  weather_strike_base;
uniform vec3  weather_strike_top;
uniform vec3  weather_lightning_color;
uniform vec2  weather_screen_res;
uniform float weather_lightning_flash;
uniform float weather_lightning_bolt;
uniform float weather_lightning_width;
uniform float weather_lightning_brightness;
uniform float weather_lightning_ambient;
uniform float weather_lightning_seed;
uniform float weather_far_clip;

#ifdef WEATHER_LIGHTNING_QUALITY
uniform mat4  weather_view_to_world;
uniform vec4  weather_bolt_bounds;
uniform vec4  weather_cloud_params;
uniform vec2  weather_cloud_offset;
uniform vec3  weather_distance_grade;
uniform float weather_cloud_scale;
uniform float weather_quality_bolt;
uniform float weather_lightning_afterglow;
uniform float weather_color_variation;
uniform float weather_sheet_strength;
uniform float weather_corona_strength;
uniform float weather_wet_glint_strength;
uniform float weather_wetness;
uniform float weather_wet_exposure;
uniform float weather_splash_up_threshold;
uniform float weather_splash_max_distance;
uniform float weather_lightning_energy_ceiling;
uniform int   weather_quality_tier;
uniform int   weather_stroke_index;
uniform int   weather_sheet_enabled;
uniform int   weather_wet_glint_enabled;

// Reuses the sibling rain features' already-reserved sampler. It is bound only
// while the quality wet-glint is active and the current-frame map is valid.
uniform sampler2D weather_rain_occlusion_map;
uniform mat4  weather_rain_occlusion_matrix;
uniform float weather_rain_occlusion_depth_range;
uniform float weather_rain_occlusion_bias;
uniform float weather_rain_occlusion_softness;
uniform int   weather_rain_occlusion_enabled;
#endif

vec4 getPosition(vec2 pos_screen);
vec4 getNorm(vec2 screenpos);

const int PRIMARY_SEGMENTS = 24;
const int BRANCH_SEGMENTS = 8;

float weatherHash11(float p)
{
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

vec2 weatherBoltOffset(float u, float seed, float amplitude)
{
    // Multi-frequency coherent displacement. sin(pi*u) anchors the primary
    // bolt exactly at cloud and ground endpoints.
    float envelope = sin(3.14159265 * clamp(u, 0.0, 1.0));
    float x = sin(u * 17.0 + seed * 5.1) +
              0.55 * sin(u * 41.0 + seed * 11.7) +
              0.25 * sin(u * 93.0 + seed * 2.3);
    float y = sin(u * 19.0 + seed * 8.4) +
              0.50 * sin(u * 47.0 + seed * 3.9) +
              0.22 * sin(u * 87.0 + seed * 13.1);
    return vec2(x, y) * amplitude * envelope;
}

vec3 weatherPrimaryPoint(float u)
{
    vec3 point = mix(weather_strike_base, weather_strike_top, u);
    float height = max(weather_strike_top.z - weather_strike_base.z, 1.0);
    vec2 offset = weatherBoltOffset(
        u, weather_lightning_seed, height * 0.022);
    point.xy += offset;
    return point;
}

vec3 weatherBranchPoint(float v, int branch)
{
    float branch_f = float(branch);
    float start_u = branch == 0 ? 0.38 : 0.61;
    vec3 start = weatherPrimaryPoint(start_u);
    float height = max(weather_strike_top.z - weather_strike_base.z, 1.0);
    float sign_x = branch == 0 ? -1.0 : 1.0;
    vec3 end = start + vec3(
        sign_x * height * (0.12 + 0.05 *
            weatherHash11(weather_lightning_seed + branch_f)),
        height * (weatherHash11(
            weather_lightning_seed + branch_f + 8.0) - 0.5) * 0.16,
        -height * (0.16 + 0.08 *
            weatherHash11(weather_lightning_seed + branch_f + 19.0)));
    vec3 point = mix(start, end, v);
    point.xy += weatherBoltOffset(
        v, weather_lightning_seed + 31.0 + branch_f * 7.0,
        height * 0.010);
    return point;
}

bool weatherProject(vec3 world, out vec2 uv, out float view_depth)
{
    vec4 view = weather_world_to_view * vec4(world, 1.0);
    vec4 clip = weather_view_projection * vec4(world, 1.0);
    view_depth = length(view.xyz);
    if (clip.w <= 1.0e-4)
    {
        uv = vec2(-10.0);
        return false;
    }
    vec2 ndc = clip.xy / clip.w;
    uv = ndc * 0.5 + 0.5;
    return all(lessThan(abs(ndc), vec2(1.3)));
}

void weatherSegmentDistance(vec2 pixel, vec3 first, vec3 second,
                            inout float nearest_pixels,
                            inout float nearest_depth)
{
    vec2 a;
    vec2 b;
    float depth_a;
    float depth_b;
    bool valid_a = weatherProject(first, a, depth_a);
    bool valid_b = weatherProject(second, b, depth_b);
    if (!valid_a || !valid_b)
    {
        return;
    }

    vec2 a_px = a * weather_screen_res;
    vec2 b_px = b * weather_screen_res;
    vec2 ab = b_px - a_px;
    float denominator = max(dot(ab, ab), 1.0e-5);
    float h = clamp(dot(pixel - a_px, ab) / denominator, 0.0, 1.0);
    float distance_px = length(pixel - (a_px + ab * h));
    if (distance_px < nearest_pixels)
    {
        nearest_pixels = distance_px;
        nearest_depth = mix(depth_a, depth_b, h);
    }
}

#ifndef WEATHER_LIGHTNING_QUALITY
void main()
{
    vec2 tc = vary_fragcoord.xy;
    vec2 pixel = tc * weather_screen_res;
    float nearest_pixels = 1.0e20;
    float nearest_bolt_depth = 1.0e20;

    vec3 previous = weatherPrimaryPoint(0.0);
    for (int i = 1; i <= PRIMARY_SEGMENTS; ++i)
    {
        float u = float(i) / float(PRIMARY_SEGMENTS);
        vec3 current = weatherPrimaryPoint(u);
        weatherSegmentDistance(pixel, previous, current,
                               nearest_pixels, nearest_bolt_depth);
        previous = current;
    }

    for (int branch = 0; branch < 2; ++branch)
    {
        previous = weatherBranchPoint(0.0, branch);
        for (int i = 1; i <= BRANCH_SEGMENTS; ++i)
        {
            float v = float(i) / float(BRANCH_SEGMENTS);
            vec3 current = weatherBranchPoint(v, branch);
            weatherSegmentDistance(pixel, previous, current,
                                   nearest_pixels, nearest_bolt_depth);
            previous = current;
        }
    }

    vec3 surface_view = getPosition(tc).xyz;
    float scene_depth = length(surface_view);
    float width = max(weather_lightning_width, 0.25);
    float core = exp(-0.5 * nearest_pixels * nearest_pixels /
                     (width * width));
    float halo_width = width * 4.5;
    float halo = exp(-0.5 * nearest_pixels * nearest_pixels /
                     (halo_width * halo_width)) * 0.28;
    float visible = nearest_bolt_depth <= scene_depth + 0.75 ? 1.0 : 0.0;
    float bolt = (core + halo) * visible * weather_lightning_bolt;

    // The discharge also behaves like a short-lived local light. Real G-buffer
    // normals make nearby SL surfaces respond directionally instead of merely
    // receiving a flat post-process white flash.
    vec3 strike_mid = mix(weather_strike_base, weather_strike_top, 0.55);
    vec3 light_view =
        (weather_world_to_view * vec4(strike_mid, 1.0)).xyz;
    vec3 to_light = light_view - surface_view;
    float light_distance = max(length(to_light), 0.01);
    vec3 light_direction = to_light / light_distance;
    vec3 normal_raw = getNorm(tc).xyz;
    float normal_length = length(normal_raw);
    vec3 normal = normal_length > 1.0e-5
        ? normal_raw / normal_length : vec3(0.0, 0.0, 1.0);
    float diffuse = max(dot(normal, light_direction), 0.0);
    float range = max(length(weather_strike_top -
                             weather_strike_base) * 1.5, 64.0);
    float attenuation = 1.0 /
        (1.0 + light_distance * light_distance / (range * range));
    // Deferred sky reconstructs at the active camera far plane. Classifying
    // relative to that plane keeps the headline flash full-strength for draw
    // distances from 64 m through 512 m (and beyond), instead of assuming a
    // fixed 512 m world distance.
    float sky = scene_depth > 0.95 * weather_far_clip ? 1.0 : 0.0;
    // Use a branch rather than mix: some GLSL implementations propagate a
    // NaN from the unused surface term even when the sky weight is exactly 1.
    float surface_flash = sky > 0.5 ? 1.0 : diffuse * attenuation;
    float flash = weather_lightning_flash *
        (weather_lightning_ambient + surface_flash);

    vec3 energy = weather_lightning_color *
        weather_lightning_brightness * (flash + bolt * 2.5);
    frag_color = vec4(energy, 0.0);
}
#else

const int QUALITY_PRIMARY_SEGMENTS = 40;
const int QUALITY_BRANCH_SEGMENTS = 8;
const int QUALITY_BRANCH_COUNT = 6;

bool weatherQualityFinite(float value)
{
    return !isnan(value) && !isinf(value);
}

bool weatherQualityFinite3(vec3 value)
{
    return !any(isnan(value)) && !any(isinf(value));
}

bool weatherQualityFinite4(vec4 value)
{
    return !any(isnan(value)) && !any(isinf(value));
}

vec3 weatherQualitySafeNormalize(vec3 value, vec3 fallback)
{
    float length_squared = dot(value, value);
    if (!weatherQualityFinite3(value) ||
        !(length_squared > 1.0e-8) ||
        !weatherQualityFinite(length_squared))
    {
        return fallback;
    }
    return value * inversesqrt(length_squared);
}

float weatherQualityNoise1(float value, float seed)
{
    float cell = floor(value);
    float fraction = fract(value);
    fraction = fraction * fraction * (3.0 - 2.0 * fraction);
    float a = weatherHash11(cell + seed);
    float b = weatherHash11(cell + 1.0 + seed);
    return mix(a, b, fraction) * 2.0 - 1.0;
}

float weatherQualityFbm1(float value, float seed)
{
    float sum = 0.0;
    float weight_sum = 0.0;
    float weight = 1.0;
    int octave_count = clamp(weather_quality_tier, 1, 3) + 2;
    for (int octave = 0; octave < 5; ++octave)
    {
        if (octave >= octave_count)
        {
            break;
        }
        sum += weatherQualityNoise1(
            value, seed + float(octave) * 37.17) * weight;
        weight_sum += weight;
        value = value * 2.03 + 11.71;
        weight *= 0.52;
    }
    return sum / max(weight_sum, 1.0e-4);
}

vec2 weatherQualityBoltOffset(float u, float seed, float amplitude)
{
    // Dyadic value-noise fBm is the iterative shader equivalent of bounded
    // midpoint displacement. The endpoint envelope retains exact attachment.
    float envelope = sin(3.14159265 * clamp(u, 0.0, 1.0));
    vec2 displacement = vec2(
        weatherQualityFbm1(u * 4.0 + 2.7, seed + 13.0),
        weatherQualityFbm1(u * 4.0 + 8.1, seed + 79.0));
    if (weather_stroke_index > 0)
    {
        // A return stroke follows the established channel but shimmers along a
        // small deterministic secondary displacement.
        float stroke_seed =
            seed + 211.0 * float(clamp(weather_stroke_index, 0, 3));
        displacement += vec2(
            weatherQualityFbm1(u * 7.0 + 1.3, stroke_seed),
            weatherQualityFbm1(u * 7.0 + 5.9, stroke_seed + 47.0)) * 0.16;
    }
    return displacement * amplitude * envelope;
}

vec3 weatherQualityPrimaryPoint(float u)
{
    vec3 point = mix(weather_strike_base, weather_strike_top, u);
    float height = max(weather_strike_top.z - weather_strike_base.z, 1.0);
    point.xy += weatherQualityBoltOffset(
        u, weather_lightning_seed, height * 0.034);
    return point;
}

vec3 weatherQualityFirstBranchPoint(float v, int branch)
{
    float branch_f = float(branch);
    float attach = 0.25 + branch_f * 0.23 +
        (weatherHash11(weather_lightning_seed + branch_f * 17.0) - 0.5) *
            0.06;
    vec3 start = weatherQualityPrimaryPoint(clamp(attach, 0.18, 0.82));
    float height = max(weather_strike_top.z - weather_strike_base.z, 1.0);
    float angle = 6.28318531 *
        weatherHash11(weather_lightning_seed + 101.0 + branch_f * 29.0);
    float reach = height * (0.10 + 0.065 *
        weatherHash11(weather_lightning_seed + 149.0 + branch_f));
    vec3 end = start + vec3(
        cos(angle) * reach,
        sin(angle) * reach,
        -height * (0.14 + 0.08 *
            weatherHash11(weather_lightning_seed + 193.0 + branch_f)));
    vec3 point = mix(start, end, v);
    point.xy += weatherQualityBoltOffset(
        v, weather_lightning_seed + 251.0 + branch_f * 41.0,
        height * 0.014);
    return point;
}

vec3 weatherQualityBranchPoint(float v, int branch)
{
    if (branch < 3)
    {
        return weatherQualityFirstBranchPoint(v, branch);
    }

    // Fixed-depth hierarchy avoids GLSL recursion while allowing a second
    // generation to sprout from each first-generation fork.
    int parent = branch - 3;
    float branch_f = float(branch);
    float parent_v = 0.38 + 0.28 *
        weatherHash11(weather_lightning_seed + 307.0 + branch_f);
    vec3 start = weatherQualityFirstBranchPoint(parent_v, parent);
    float height = max(weather_strike_top.z - weather_strike_base.z, 1.0);
    float angle = 6.28318531 *
        weatherHash11(weather_lightning_seed + 359.0 + branch_f * 23.0);
    float reach = height * (0.055 + 0.04 *
        weatherHash11(weather_lightning_seed + 401.0 + branch_f));
    vec3 end = start + vec3(
        cos(angle) * reach,
        sin(angle) * reach,
        -height * (0.07 + 0.05 *
            weatherHash11(weather_lightning_seed + 443.0 + branch_f)));
    vec3 point = mix(start, end, v);
    point.xy += weatherQualityBoltOffset(
        v, weather_lightning_seed + 487.0 + branch_f * 31.0,
        height * 0.008);
    return point;
}

bool weatherQualityClipPlane(
    inout vec4 clip_a, inout vec3 view_a,
    inout vec4 clip_b, inout vec3 view_b,
    float distance_a, float distance_b)
{
    if (!weatherQualityFinite(distance_a) ||
        !weatherQualityFinite(distance_b) ||
        (distance_a < 0.0 && distance_b < 0.0))
    {
        return false;
    }
    if (distance_a < 0.0)
    {
        float denominator = distance_a - distance_b;
        if (!weatherQualityFinite(denominator) ||
            !(abs(denominator) > 1.0e-8))
        {
            return false;
        }
        float amount = clamp(distance_a / denominator, 0.0, 1.0);
        clip_a = mix(clip_a, clip_b, amount);
        view_a = mix(view_a, view_b, amount);
    }
    else if (distance_b < 0.0)
    {
        float denominator = distance_b - distance_a;
        if (!weatherQualityFinite(denominator) ||
            !(abs(denominator) > 1.0e-8))
        {
            return false;
        }
        float amount = clamp(distance_b / denominator, 0.0, 1.0);
        clip_b = mix(clip_b, clip_a, amount);
        view_b = mix(view_b, view_a, amount);
    }
    return true;
}

void weatherQualitySegmentDistance(
    vec2 pixel, vec3 first, vec3 second, float segment_weight,
    inout float nearest_pixels, inout float nearest_depth,
    inout float nearest_weight)
{
    vec3 view_a =
        (weather_world_to_view * vec4(first, 1.0)).xyz;
    vec3 view_b =
        (weather_world_to_view * vec4(second, 1.0)).xyz;
    vec4 clip_a = weather_view_projection * vec4(first, 1.0);
    vec4 clip_b = weather_view_projection * vec4(second, 1.0);
    if (!weatherQualityFinite3(view_a) ||
        !weatherQualityFinite3(view_b) ||
        !weatherQualityFinite4(clip_a) ||
        !weatherQualityFinite4(clip_b))
    {
        return;
    }

    // Clip in homogeneous space instead of rejecting a whole segment when one
    // endpoint crosses the near plane or the generous NDC guard rectangle.
    if (!weatherQualityClipPlane(
            clip_a, view_a, clip_b, view_b,
            clip_a.z + clip_a.w, clip_b.z + clip_b.w) ||
        !weatherQualityClipPlane(
            clip_a, view_a, clip_b, view_b,
            clip_a.w - 1.0e-4, clip_b.w - 1.0e-4) ||
        !weatherQualityClipPlane(
            clip_a, view_a, clip_b, view_b,
            clip_a.x + 8.0 * clip_a.w,
            clip_b.x + 8.0 * clip_b.w) ||
        !weatherQualityClipPlane(
            clip_a, view_a, clip_b, view_b,
            8.0 * clip_a.w - clip_a.x,
            8.0 * clip_b.w - clip_b.x) ||
        !weatherQualityClipPlane(
            clip_a, view_a, clip_b, view_b,
            clip_a.y + 8.0 * clip_a.w,
            clip_b.y + 8.0 * clip_b.w) ||
        !weatherQualityClipPlane(
            clip_a, view_a, clip_b, view_b,
            8.0 * clip_a.w - clip_a.y,
            8.0 * clip_b.w - clip_b.y))
    {
        return;
    }

    vec2 a = clip_a.xy / clip_a.w * 0.5 + 0.5;
    vec2 b = clip_b.xy / clip_b.w * 0.5 + 0.5;
    if (any(isnan(a)) || any(isinf(a)) ||
        any(isnan(b)) || any(isinf(b)))
    {
        return;
    }
    vec2 a_px = a * weather_screen_res;
    vec2 b_px = b * weather_screen_res;
    vec2 ab = b_px - a_px;
    float denominator = dot(ab, ab);
    if (!weatherQualityFinite(denominator) ||
        !(denominator > 1.0e-5))
    {
        return;
    }
    float h = clamp(dot(pixel - a_px, ab) / denominator, 0.0, 1.0);
    float distance_px = length(pixel - (a_px + ab * h));
    float inverse_w_a = 1.0 / clip_a.w;
    float inverse_w_b = 1.0 / clip_b.w;
    float inverse_w =
        mix(inverse_w_a, inverse_w_b, h);
    if (!weatherQualityFinite(inverse_w) ||
        !(abs(inverse_w) > 1.0e-8))
    {
        return;
    }
    vec3 view_at_nearest = mix(
        view_a * inverse_w_a, view_b * inverse_w_b, h) / inverse_w;
    float depth = length(view_at_nearest);
    if (weatherQualityFinite(distance_px) &&
        weatherQualityFinite(depth) &&
        distance_px < nearest_pixels)
    {
        nearest_pixels = distance_px;
        nearest_depth = depth;
        nearest_weight = segment_weight;
    }
}

float weatherQualityGaussian(float distance_pixels, float width_pixels)
{
    if (!weatherQualityFinite(distance_pixels) ||
        !weatherQualityFinite(width_pixels))
    {
        return 0.0;
    }
    float ratio = clamp(
        distance_pixels / max(width_pixels, 0.125), 0.0, 16.0);
    return exp(clamp(-0.5 * ratio * ratio, -80.0, 0.0));
}

float weatherQualityHash21(vec2 value)
{
    vec3 p3 = fract(vec3(value.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float weatherQualityNoise2(vec2 value)
{
    vec2 cell = floor(value);
    vec2 fraction = fract(value);
    fraction = fraction * fraction * (3.0 - 2.0 * fraction);
    float a = weatherQualityHash21(cell);
    float b = weatherQualityHash21(cell + vec2(1.0, 0.0));
    float c = weatherQualityHash21(cell + vec2(0.0, 1.0));
    float d = weatherQualityHash21(cell + vec2(1.0, 1.0));
    return mix(mix(a, b, fraction.x),
               mix(c, d, fraction.x), fraction.y);
}

float weatherQualityCloudFbm(vec2 value)
{
    float sum = 0.0;
    float weight_sum = 0.0;
    float weight = 1.0;
    for (int octave = 0; octave < 4; ++octave)
    {
        sum += weatherQualityNoise2(value) * weight;
        weight_sum += weight;
        value = value * 2.03 + vec2(7.1, 13.7);
        weight *= 0.52;
    }
    return sum / max(weight_sum, 1.0e-4);
}

float weatherQualityRainExposureTap(
    ivec2 map_texel, ivec2 map_size, float point_depth)
{
    if (any(lessThan(map_texel, ivec2(0))) ||
        any(greaterThanEqual(map_texel, map_size)))
    {
        return 1.0;
    }
    float map_depth = texelFetch(
        weather_rain_occlusion_map, map_texel, 0).r;
    if (!weatherQualityFinite(map_depth))
    {
        return 1.0;
    }
    float cover_height =
        (point_depth - map_depth) * weather_rain_occlusion_depth_range;
    float bias = max(weather_rain_occlusion_bias, 0.0);
    float softness = max(weather_rain_occlusion_softness, 1.0e-4);
    return clamp(
        1.0 - smoothstep(bias, bias + softness, cover_height), 0.0, 1.0);
}

float weatherQualityRainExposure(vec3 world_position)
{
    float fallback = clamp(weather_wet_exposure, 0.0, 1.0);
    if (weather_rain_occlusion_enabled == 0 ||
        !(weather_rain_occlusion_depth_range > 0.0) ||
        !weatherQualityFinite(weather_rain_occlusion_depth_range) ||
        !weatherQualityFinite(weather_rain_occlusion_bias) ||
        !weatherQualityFinite(weather_rain_occlusion_softness))
    {
        return fallback;
    }

    vec4 clip =
        weather_rain_occlusion_matrix * vec4(world_position, 1.0);
    if (any(isnan(clip)) || any(isinf(clip)) ||
        !(abs(clip.w) > 1.0e-6))
    {
        return fallback;
    }
    vec3 ndc = clip.xyz / clip.w;
    if (any(isnan(ndc)) || any(isinf(ndc)) ||
        any(lessThan(ndc, vec3(-1.0))) ||
        any(greaterThan(ndc, vec3(1.0))))
    {
        return fallback;
    }

    ivec2 map_size = textureSize(weather_rain_occlusion_map, 0);
    if (any(lessThanEqual(map_size, ivec2(0))))
    {
        return fallback;
    }
    vec2 uv = ndc.xy * 0.5 + 0.5;
    float point_depth = ndc.z * 0.5 + 0.5;
    vec2 texel_position = uv * vec2(map_size) - 0.5;
    ivec2 base_texel = ivec2(floor(texel_position));
    vec2 fraction = fract(texel_position);
    float e00 = weatherQualityRainExposureTap(
        base_texel + ivec2(0, 0), map_size, point_depth);
    float e10 = weatherQualityRainExposureTap(
        base_texel + ivec2(1, 0), map_size, point_depth);
    float e01 = weatherQualityRainExposureTap(
        base_texel + ivec2(0, 1), map_size, point_depth);
    float e11 = weatherQualityRainExposureTap(
        base_texel + ivec2(1, 1), map_size, point_depth);
    float exposure = mix(
        mix(e00, e10, fraction.x),
        mix(e01, e11, fraction.x), fraction.y);
    float edge_texels =
        min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y)) *
        float(min(map_size.x, map_size.y));
    float interior = smoothstep(1.0, 3.0, edge_texels);
    return clamp(mix(fallback, exposure, interior), 0.0, 1.0);
}

vec3 weatherQualityBoundEnergy(vec3 energy, float ceiling)
{
    if (!weatherQualityFinite3(energy) ||
        !weatherQualityFinite(ceiling) ||
        !(ceiling > 0.0))
    {
        return vec3(0.0);
    }
    energy = max(energy, vec3(0.0));
    float peak = max(energy.r, max(energy.g, energy.b));
    energy *= min(1.0, ceiling / max(peak, 1.0e-5));
    return energy;
}

void main()
{
    vec2 tc = vary_fragcoord.xy;
    vec2 pixel = tc * weather_screen_res;
    vec3 surface_view = getPosition(tc).xyz;
    bool surface_position_valid = weatherQualityFinite3(surface_view);
    float scene_depth = surface_position_valid
        ? length(surface_view) : max(weather_far_clip, 1.0);
    if (!weatherQualityFinite(scene_depth))
    {
        scene_depth = max(weather_far_clip, 1.0);
        surface_position_valid = false;
    }
    bool sky = !surface_position_valid ||
        scene_depth > 0.95 * max(weather_far_clip, 1.0);

    float quality_bolt = clamp(weather_quality_bolt, 0.0, 1.0);
    float nearest_pixels = 1.0e20;
    float nearest_bolt_depth = 1.0e20;
    float nearest_weight = 1.0;
    bool inside_bolt_bounds =
        tc.x >= weather_bolt_bounds.x &&
        tc.y >= weather_bolt_bounds.y &&
        tc.x <= weather_bolt_bounds.z &&
        tc.y <= weather_bolt_bounds.w;
    if (quality_bolt > 1.0e-4 && inside_bolt_bounds)
    {
        int tier = clamp(weather_quality_tier, 1, 3);
        int primary_segments =
            tier == 1 ? 24 : (tier == 2 ? 32 : 40);
        vec3 previous = weatherQualityPrimaryPoint(0.0);
        for (int i = 1; i <= QUALITY_PRIMARY_SEGMENTS; ++i)
        {
            if (i > primary_segments)
            {
                break;
            }
            float u = float(i) / float(primary_segments);
            vec3 current = weatherQualityPrimaryPoint(u);
            weatherQualitySegmentDistance(
                pixel, previous, current, 1.0,
                nearest_pixels, nearest_bolt_depth, nearest_weight);
            previous = current;
        }

        int branch_count = tier == 1 ? 3 : (tier == 2 ? 4 : 6);
        int branch_segments = tier == 1 ? 6 : (tier == 2 ? 7 : 8);
        for (int branch = 0; branch < QUALITY_BRANCH_COUNT; ++branch)
        {
            if (branch >= branch_count)
            {
                break;
            }
            previous = weatherQualityBranchPoint(0.0, branch);
            float branch_weight = branch < 3 ? 0.62 : 0.38;
            for (int i = 1; i <= QUALITY_BRANCH_SEGMENTS; ++i)
            {
                if (i > branch_segments)
                {
                    break;
                }
                float v = float(i) / float(branch_segments);
                vec3 current = weatherQualityBranchPoint(v, branch);
                weatherQualitySegmentDistance(
                    pixel, previous, current, branch_weight,
                    nearest_pixels, nearest_bolt_depth, nearest_weight);
                previous = current;
            }
        }
    }

    float width = max(weather_lightning_width, 0.25) *
        max(nearest_weight, 0.25);
    bool has_bolt_sample =
        nearest_pixels < 1.0e19 &&
        weatherQualityFinite(nearest_bolt_depth);
    float visible = has_bolt_sample &&
        nearest_bolt_depth <= scene_depth + 0.75 ? 1.0 : 0.0;
    float core = weatherQualityGaussian(nearest_pixels, width * 0.58);
    float channel = weatherQualityGaussian(nearest_pixels, width * 1.65);
    float corona = weatherQualityGaussian(nearest_pixels, width * 5.5) *
        clamp(weather_corona_strength, 0.0, 2.0);
    float segment_energy = clamp(nearest_weight, 0.25, 1.0);

    float color_variation = clamp(weather_color_variation, -1.0, 1.0);
    vec3 strike_color = clamp(
        weather_lightning_color *
            vec3(1.0 + 0.08 * color_variation,
                 1.0 + 0.025 * color_variation,
                 1.0 - 0.06 * color_variation),
        vec3(0.0), vec3(4.0));
    vec3 corona_color =
        strike_color * vec3(0.72, 0.80, 1.12);
    vec3 bolt_energy =
        (vec3(1.0, 0.985, 1.0) * core * 2.6 +
         strike_color * channel * 0.72 +
         corona_color * corona * 0.34) *
        segment_energy * quality_bolt * visible *
        clamp(weather_distance_grade.x, 0.0, 1.0);

    float flash_pulse = clamp(
        weather_lightning_flash + weather_lightning_afterglow, 0.0, 1.25);
    vec3 flash_energy = vec3(0.0);
    vec3 wet_energy = vec3(0.0);
    vec3 strike_mid = mix(weather_strike_base, weather_strike_top, 0.55);
    vec3 light_view =
        (weather_world_to_view * vec4(strike_mid, 1.0)).xyz;

    if (sky)
    {
        float sky_response = 1.0;
        if (weather_sheet_enabled != 0 &&
            weather_sheet_strength > 0.0)
        {
            vec3 view_direction = weatherQualitySafeNormalize(
                surface_view, vec3(0.0, 0.0, -1.0));
            vec3 strike_direction = weatherQualitySafeNormalize(
                light_view, vec3(0.0, 0.0, -1.0));
            vec3 world_direction = weatherQualitySafeNormalize(
                (weather_view_to_world *
                    vec4(view_direction, 0.0)).xyz,
                vec3(0.0, 0.0, 1.0));
            float cloud_scale = clamp(
                weatherQualityFinite(weather_cloud_scale)
                    ? weather_cloud_scale : 0.42,
                0.02, 3.0);
            vec2 cloud_uv =
                world_direction.xy * (2.0 / cloud_scale) +
                weather_cloud_offset +
                vec2(weather_lightning_seed * 0.0017,
                     weather_lightning_seed * 0.0023);
            float cloud_noise = weatherQualityCloudFbm(cloud_uv);
            float coverage = clamp(weather_cloud_params.x, 0.0, 1.0);
            float density = clamp(
                0.75 * weather_cloud_params.y +
                0.25 * weather_cloud_params.z, 0.0, 2.0);
            float variance = clamp(weather_cloud_params.w, 0.0, 1.0);
            float threshold = mix(0.78, 0.32, coverage);
            float softness = 0.08 + variance * 0.10;
            float shaped_noise =
                cloud_noise * mix(0.72, 1.30, min(density, 1.0));
            float cloud_mask = smoothstep(
                threshold - softness, threshold + softness, shaped_noise);
            cloud_mask *= smoothstep(0.02, 0.30, coverage);
            float alignment =
                clamp(dot(view_direction, strike_direction), 0.0, 1.0);
            float broad_lobe = smoothstep(0.35, 0.90, alignment);
            float core_lobe = smoothstep(0.76, 0.985, alignment);
            float sheet_mask = cloud_mask *
                (0.28 * broad_lobe + 0.72 * core_lobe);
            // Keep a low global floor so custom EEP cloud textures that do not
            // match the procedural mask never make the working sky flash vanish.
            float sheet_strength =
                clamp(weather_sheet_strength, 0.0, 2.0);
            float shaped_response = 0.22 + sheet_mask * 1.45;
            // Zero strength is exactly the unshaped baseline. Blend through
            // the first unit, then let the upper artist range add only shaped
            // energy; this avoids a darkening discontinuity above zero.
            sky_response = mix(
                1.0, shaped_response, min(sheet_strength, 1.0));
            sky_response += max(sheet_strength - 1.0, 0.0) *
                sheet_mask * 1.45;
        }
        flash_energy = strike_color * flash_pulse *
            (clamp(weather_lightning_ambient, 0.0, 1.0) + sky_response) *
            clamp(weather_distance_grade.z, 0.0, 1.0);
    }
    else
    {
        vec3 to_light = light_view - surface_view;
        float light_distance = length(to_light);
        vec3 light_direction = weatherQualitySafeNormalize(
            to_light, vec3(0.0, 0.0, 1.0));
        vec3 normal_view = weatherQualitySafeNormalize(
            getNorm(tc).xyz, vec3(0.0, 0.0, 1.0));
        float diffuse = max(dot(normal_view, light_direction), 0.0);
        float range = max(length(weather_strike_top -
                                 weather_strike_base) * 1.5, 64.0);
        float attenuation = 0.0;
        if (weatherQualityFinite(light_distance) &&
            weatherQualityFinite(range) && range > 0.0)
        {
            float normalized_distance =
                clamp(light_distance / range, 0.0, 64.0);
            attenuation =
                1.0 / (1.0 + normalized_distance * normalized_distance);
        }
        float surface_response =
            clamp(weather_lightning_ambient, 0.0, 1.0) +
            diffuse * attenuation;
        flash_energy = strike_color * flash_pulse * surface_response *
            clamp(weather_distance_grade.y, 0.0, 1.0);

        if (weather_wet_glint_enabled != 0 &&
            weather_wet_glint_strength > 0.0 &&
            weather_wetness > 0.0 &&
            scene_depth > 0.0 &&
            scene_depth <= max(weather_splash_max_distance, 1.0))
        {
            vec3 world_position =
                (weather_view_to_world * vec4(surface_view, 1.0)).xyz;
            vec3 normal_world = weatherQualitySafeNormalize(
                (weather_view_to_world *
                    vec4(normal_view, 0.0)).xyz,
                vec3(0.0, 0.0, 1.0));
            if (weatherQualityFinite3(world_position) &&
                weatherQualityFinite3(normal_world))
            {
                vec3 view_direction = weatherQualitySafeNormalize(
                    -surface_view, vec3(0.0, 0.0, 1.0));
                vec3 half_direction = weatherQualitySafeNormalize(
                    light_direction + view_direction, normal_view);
                float ndh = clamp(
                    dot(normal_view, half_direction), 0.0, 1.0);
                float ndv = clamp(
                    dot(normal_view, view_direction), 0.0, 1.0);
                float specular = pow(ndh, 96.0);
                float fresnel_base = 1.0 - ndv;
                float fresnel = fresnel_base * fresnel_base;
                fresnel *= fresnel * fresnel_base;
                float threshold = clamp(
                    weather_splash_up_threshold, 0.0, 0.98);
                float up_facing = smoothstep(
                    threshold, min(1.0, threshold + 0.12),
                    normal_world.z);
                float rain_exposure =
                    weatherQualityRainExposure(world_position);
                float wet_glint = clamp(
                    weather_wet_glint_strength, 0.0, 2.0) *
                    clamp(weather_wetness, 0.0, 1.0) *
                    up_facing * rain_exposure * attenuation *
                    (0.08 + 2.8 * specular) *
                    (0.25 + 0.75 * fresnel) *
                    flash_pulse *
                    clamp(weather_distance_grade.y, 0.0, 1.0);
                wet_energy = strike_color * min(wet_glint, 8.0);
            }
        }
    }

    float brightness =
        clamp(weather_lightning_brightness, 0.0, 32.0);
    float energy_ceiling =
        clamp(weather_lightning_energy_ceiling, 1.0, 128.0);
    // Sanitize layers independently so a malformed optional sheet or wet
    // input cannot erase an otherwise valid bolt. Broad-area layers have
    // tighter caps than the thin channel to limit exposure pumping.
    vec3 bounded_bolt = weatherQualityBoundEnergy(
        bolt_energy * brightness, energy_ceiling);
    vec3 bounded_flash = weatherQualityBoundEnergy(
        flash_energy * brightness, min(energy_ceiling, 16.0));
    vec3 bounded_wet = weatherQualityBoundEnergy(
        wet_energy * brightness, min(energy_ceiling, 8.0));
    frag_color = vec4(weatherQualityBoundEnergy(
        bounded_bolt + bounded_flash + bounded_wet,
        energy_ceiling), 0.0);
}
#endif
