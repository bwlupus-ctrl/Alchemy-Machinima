/**
 * @file sensorOverlayF.glsl
 * @brief Un-noised affine layers using the presentation source-UV map.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

/*[EXTRA_CODE_HERE]*/
in vec2 vary_fragcoord;
out vec4 frag_color;
uniform sampler2D diffuseRect, diffuseMap, emissiveRect, diopterWarpMap;
uniform vec4 depth_warp_params, fisheye_params, fisheye_params2;
uniform int sensor_overlay_mode;
void main()
{
    vec2 duv = vary_fragcoord.xy;
    if (depth_warp_params.y > 0.5)
    {
        // exact forward source-uv mapping from cineFisheyeF.glsl; the color
        // pass blacks out-of-range samples, depth just clamps instead.
        // aspect comes from depth_warp_params.z (the actual post-chain
        // target) so it is byte-identical to the fisheye pass's screen_res
        // aspect even under a resolution divisor.
        float aspect = max(depth_warp_params.z, 1e-4);
        vec2 c = duv * 2.0 - 1.0 - fisheye_params2.xy * 2.0;
        c.x *= aspect;
        float r = length(c);
        float rn = r / max(fisheye_params.z, 0.05);
        float rn2 = rn * rn;
        float g = (1.0 + fisheye_params.x * rn2 +
                   fisheye_params.y * rn2 * rn2) /
                  (1.0 + fisheye_params.x + fisheye_params.y);
        vec2 src = c * g / max(fisheye_params2.z, 0.25);
        duv = clamp(vec2(src.x / aspect, src.y) * 0.5 + 0.5 +
                    fisheye_params2.xy, 0.0, 1.0);
    }
    if (depth_warp_params.x > 0.5)
    {
        duv = clamp(texture(diopterWarpMap, duv).rg, 0.0, 1.0);
    }

    vec4 scene = texture(diffuseRect, vary_fragcoord);
    vec3 addition = texture(diffuseMap, duv).rgb;
    vec3 transmission = texture(emissiveRect, duv).rgb;
    if (sensor_overlay_mode == 1) transmission -= addition;
    frag_color = vec4(scene.rgb * transmission + addition, scene.a);
}
