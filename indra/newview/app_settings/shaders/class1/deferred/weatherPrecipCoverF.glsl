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

// Integrate only the part of a world ray whose cover data is known. Clipping
// first gives sky and geometry the same boundary and avoids long outside taps.
float weatherPrecipExposedDistance(vec3 start, vec3 direction, float distance_m)
{
    if (snow_cover_valid == 0) return 0.0;
    vec3 a = (weather_surface_cover_matrix * vec4(start,1.0)).xyz * 0.5 + 0.5;
    vec3 d = (weather_surface_cover_matrix * vec4(direction,0.0)).xyz * 0.5;
    float lo = 0.0, hi = max(distance_m,0.0);
    for (int axis = 0; axis < 3; ++axis)
    {
        if (abs(d[axis]) < 1e-8)
        {
            if (a[axis] < 0.0 || a[axis] > 1.0) return 0.0;
        }
        else
        {
            float t0 = -a[axis]/d[axis], t1 = (1.0-a[axis])/d[axis];
            lo = max(lo,min(t0,t1)); hi = min(hi,max(t0,t1));
        }
    }
    if (hi <= lo) return 0.0;
    // Bounded midpoint quadrature; never use endpoint exposure for the ray.
    float step_m = (hi-lo)/16.0, exposed = 0.0;
    for (int i = 0; i < 16; ++i)
    {
        vec3 q = a+d*(lo+(float(i)+0.5)*step_m);
        float roof = texture(weather_surface_cover_map,q.xy).r;
        exposed += 1.0-smoothstep(0.00005,0.0005,q.z-roof);
    }
    return exposed*step_m;
}
