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
