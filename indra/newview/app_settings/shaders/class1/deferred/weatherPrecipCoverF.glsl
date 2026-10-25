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
uniform mat4 weather_shelter_close_matrix, weather_shelter_distant_matrix;
float weatherShelterOpen(vec3 agent);
float weatherShelterOpenCoordinates(vec3 close_q, vec3 distant_q);
vec2 weatherPrecipInterval(vec3 a, vec3 d, float distance_m)
{
    float lo=0.0,hi=max(distance_m,0.0);
    for(int axis=0;axis<3;++axis)
    {
        if(abs(d[axis])<1e-8)
        {
            if(a[axis]<0.0 || a[axis]>1.0)return vec2(0);
        }
        else
        {
            float t0=-a[axis]/d[axis],t1=(1.0-a[axis])/d[axis];
            lo=max(lo,min(t0,t1));hi=min(hi,max(t0,t1));
        }
    }
    return hi>lo?vec2(lo,hi):vec2(0);
}
float weatherPrecipOpen(vec3 agent)
{
    if (snow_cover_valid == 0) return -1.0;
    if (snow_cover_valid == 2) return weatherShelterOpen(agent);
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
    if (snow_cover_valid == 2)
    {
        // Transform the ray once. Repeating two matrix multiplies at every
        // quadrature tap needlessly multiplies the full-resolution veil cost.
        vec3 close_start=(weather_shelter_close_matrix*vec4(start,1)).xyz*0.5+0.5;
        vec3 close_direction=(weather_shelter_close_matrix*vec4(direction,0)).xyz*0.5;
        vec3 distant_start=(weather_shelter_distant_matrix*vec4(start,1)).xyz*0.5+0.5;
        vec3 distant_direction=(weather_shelter_distant_matrix*vec4(direction,0)).xyz*0.5;
        vec2 close_interval=weatherPrecipInterval(close_start,close_direction,distance_m);
        vec2 distant_interval=weatherPrecipInterval(distant_start,distant_direction,distance_m);
        vec2 interval=close_interval.y>close_interval.x?close_interval:distant_interval;
        if(close_interval.y>close_interval.x && distant_interval.y>distant_interval.x)
            interval=vec2(min(close_interval.x,distant_interval.x),max(close_interval.y,distant_interval.y));
        if(interval.y<=interval.x)return 0.0;
        // Keep WP3's sixteen midpoint samples; WP7 improves their cover source
        // and clips the interval without doubling the full-resolution work.
        float step_m = (interval.y-interval.x)/16.0, exposed = 0.0;
        for (int i = 0; i < 16; ++i)
        {
            float t=interval.x+(float(i)+0.5)*step_m;
            exposed += max(weatherShelterOpenCoordinates(close_start+close_direction*t,
                distant_start+distant_direction*t),0.0);
        }
        return exposed*step_m;
    }
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
