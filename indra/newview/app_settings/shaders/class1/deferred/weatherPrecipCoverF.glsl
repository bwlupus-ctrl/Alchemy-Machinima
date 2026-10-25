/**
 * @file weatherPrecipCoverF.glsl
 * @brief Current-generation world coverage shared by snow and visibility.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork; Slipstream weather design reference, new GL code.
 * $/LicenseInfo$
 */
/*[EXTRA_CODE_HERE]*/
uniform sampler2D weather_surface_cover_map;
uniform mat4 weather_surface_cover_matrix;
uniform int snow_cover_valid;
float weatherPrecipOpen(vec3 agent)
{
    if (snow_cover_valid == 0) return -1.0;
    vec4 q = weather_surface_cover_matrix * vec4(agent,1.0);
    q.xyz = q.xyz/q.w*0.5+0.5;
    if (any(lessThan(q.xyz,vec3(0))) || any(greaterThan(q.xyz,vec3(1)))) return -1.0;
    float roof = texture(weather_surface_cover_map,q.xy).r;
    return 1.0-smoothstep(0.00005,0.0005,q.z-roof);
}
