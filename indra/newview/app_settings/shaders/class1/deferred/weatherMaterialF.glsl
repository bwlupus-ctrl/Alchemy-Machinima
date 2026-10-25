/**
 * @file weatherMaterialF.glsl
 * @brief Linear wet material response, after Actor FX and before packing/lighting.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork. Slipstream Viewer contributors' clean_weather_surface.glsl
 * provided the reference design (wet pores, film, world puddles and ripples).
 * This native GL implementation uses no Vulkan interfaces or copied shader code.
 * $/LicenseInfo$
 */
uniform int weather_material_enabled;
uniform int weather_material_excluded;
uniform sampler2D weather_surface_cover_map;
uniform mat4 weather_surface_cover_matrix;
uniform mat4 weather_material_view_to_agent;
uniform vec3 weather_material_origin;
uniform vec4 weather_material_wet; // wet, puddles, ripple strength, falling rain
uniform vec2 weather_material_time_range;

vec3 srgb_to_linear(vec3 c);
vec3 linear_to_srgb(vec3 c);

bool weatherMaterialActive()
{
    return weather_material_enabled != 0 && weather_material_excluded == 0 &&
        (weather_material_wet.x > 0.0 || weather_material_wet.y > 0.0);
}
float weatherSurfaceOpen(vec3 agent)
{
    vec3 q = (weather_surface_cover_matrix * vec4(agent, 1.0)).xyz * 0.5 + 0.5;
    // Outside a published volume is UNKNOWN, including remote auxiliary views.
    if (any(lessThan(q, vec3(0.0))) || any(greaterThan(q, vec3(1.0)))) return 0.0;
    float depth = texture(weather_surface_cover_map, q.xy).r;
    // A successfully cleared empty map is open. Receiver bias is 12 cm.
    return step(q.z - 0.12 / weather_material_time_range.y, depth);
}
float weatherPuddle(vec2 world_xy, float up)
{
    // Periodic in 4096 m so origin rebases keep exact phase. No screen/frame seed.
    vec2 p = world_xy * (6.28318530718 / 16.0);
    float field = sin(p.x + sin(p.y)) * cos(p.y * 0.5 - sin(p.x));
    float width = max(fwidth(field), 0.035);
    float threshold = 1.0 - weather_material_wet.y * 1.8;
    return smoothstep(threshold - width, threshold + width, field) * smoothstep(0.94, 0.995, up);
}
vec2 weatherDropSlope(vec2 world_xy, float seconds)
{
    vec2 cell = mod(floor(world_xy), 4096.0);
    vec2 local = fract(world_xy);
    float seed = fract(sin(dot(cell, vec2(127.1, 311.7))) * 43758.5453);
    vec2 center = vec2(0.5) + 0.22 * vec2(sin(seed * 71.0), cos(seed * 53.0));
    vec2 delta = local - center;
    float radius = length(delta);
    float age = fract(seconds * 0.5 + seed);
    float ring = radius - age * 0.55;
    float edge = min(min(local.x, 1.0-local.x), min(local.y, 1.0-local.y));
    float envelope = smoothstep(0.0, 0.1, edge) * smoothstep(0.0, 0.08, age) * (1.0-age) * (1.0-age);
    // Each seeded drop expands radially and fades to zero before its cell/age wraps.
    return delta / max(radius, 0.04) * cos(ring * 24.0) * exp(-abs(ring) * 18.0) * envelope;
}
void weatherMaterialLinear(vec3 eye, inout vec3 normal_eye, inout vec3 color,
                           inout float roughness, float metallic)
{
    if (!weatherMaterialActive()) return;
    vec3 agent = (weather_material_view_to_agent * vec4(eye, 1.0)).xyz;
    vec3 n = normalize(mat3(weather_material_view_to_agent) * normal_eye);
    float exposed = weatherSurfaceOpen(agent) * smoothstep(0.05, 0.5, n.z);
    float wet = clamp(weather_material_wet.x * exposed, 0.0, 1.0);
    float puddle = weatherPuddle(agent.xy + weather_material_origin.xy, n.z) * exposed;
    float film = max(wet, puddle);
    vec2 world_xy = agent.xy + weather_material_origin.xy;
    vec2 p = world_xy * 24.0;
    float footprint = max(length(dFdx(p)), length(dFdy(p)));
    if (film <= 0.0) return;
    // Dielectrics darken up to 28%; metals up to 2%. F0/metal/AO are never raised.
    color *= 1.0 - wet * mix(0.28, 0.02, clamp(metallic, 0.0, 1.0));
    float target = mix(0.32, 0.12, puddle);
    roughness = max(0.12, mix(roughness, min(roughness, target), film * (1.0 - 0.9 * metallic)));
    // Band-limited drop rings. Two staggered grids avoid visible cell boundaries.
    float filter_keep = 1.0 - smoothstep(0.5, 2.0, footprint);
    vec2 slope = (weatherDropSlope(world_xy, weather_material_time_range.x) +
        weatherDropSlope(world_xy + vec2(0.5), weather_material_time_range.x + 1.0)) *
        (0.025 * puddle * weather_material_wet.z * weather_material_wet.w * filter_keep);
    n = normalize(n + vec3(slope, 0.0));
    normal_eye = normalize(transpose(mat3(weather_material_view_to_agent)) * n);
}
void weatherMaterialLegacyLinear(vec3 eye, inout vec3 n, inout vec3 color,
                                 inout vec3 specular, inout float gloss)
{
    if (!weatherMaterialActive()) return;
    // Native legacy gloss uses (1-gloss) for environment LOD; never store PBR r in gloss.
    float roughness = clamp(1.0 - gloss, 0.0, 1.0);
    float before = roughness;
    weatherMaterialLinear(eye, n, color, roughness, 0.0);
    if (roughness != before)
    {
        gloss = 1.0 - roughness;
        // At most 0.02 absolute extra normal-incidence specular reflectance.
        specular = max(specular, vec3(0.02));
    }
}
void weatherMaterialLegacy(vec3 eye, inout vec3 n, inout vec3 color_srgb,
                           inout vec3 spec_srgb, inout float gloss)
{
    if (!weatherMaterialActive()) return; // exact identity: no colour-space round trip
    vec3 color = srgb_to_linear(color_srgb), spec = srgb_to_linear(spec_srgb);
    vec3 authored_color = color, authored_spec = spec;
    weatherMaterialLegacyLinear(eye, n, color, spec, gloss);
    if (any(notEqual(color, authored_color))) color_srgb = linear_to_srgb(color);
    if (any(notEqual(spec, authored_spec))) spec_srgb = linear_to_srgb(spec);
}
