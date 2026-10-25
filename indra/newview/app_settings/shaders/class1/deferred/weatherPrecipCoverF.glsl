/**
 * @file weatherPrecipCoverF.glsl
 * @brief Current-generation world coverage shared by snow and visibility.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork; Slipstream weather design reference, new GL code.
 * $/LicenseInfo$
 */
/*[EXTRA_CODE_HERE]*/
#ifndef WEATHER_PARTICLE_ADVANCED
#define WEATHER_PARTICLE_ADVANCED -1
#endif
uniform sampler2D weather_surface_cover_map;
uniform sampler2D weather_surface_cover_min_map, weather_shelter_close_min_map, weather_shelter_distant_min_map;
uniform int weather_cover_minimum_valid, weather_shelter_minimum_valid;
uniform mat4 weather_surface_cover_matrix;
uniform int snow_cover_valid;
uniform mat4 weather_shelter_close_matrix, weather_shelter_distant_matrix;
ivec2 weatherShelterCloseSize();
ivec2 weatherShelterDistantSize();
vec2 weatherShelterThresholds(bool close);
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
#if WEATHER_PARTICLE_ADVANCED != 0
    if (snow_cover_valid == 2) return weatherShelterOpen(agent);
#endif
    vec4 q = weather_surface_cover_matrix * vec4(agent,1.0);
    q.xyz = q.xyz/q.w*0.5+0.5;
    if (any(lessThan(q.xyz,vec3(0))) || any(greaterThan(q.xyz,vec3(1)))) return -1.0;
    float roof = texture(weather_surface_cover_map,q.xy).r;
    return 1.0-smoothstep(0.00005,0.0005,q.z-roof);
}

// Exact distances to the next WORLD cover-texel boundary. Unlike a fixed
// number of ray taps this cannot step over a short doorway as the eye moves.
float weatherPrecipCellEnd(vec2 q, vec2 d, ivec2 size)
{
    vec2 cell = q*vec2(size), velocity = d*vec2(size);
    vec2 boundary = mix(ceil(cell)-1.0, floor(cell)+1.0, greaterThan(velocity,vec2(0)));
    vec2 span = vec2(1e20);
    if (abs(velocity.x)>1e-8) span.x = (boundary.x-cell.x)/velocity.x;
    if (abs(velocity.y)>1e-8) span.y = (boundary.y-cell.y)/velocity.y;
    return max(min(span.x,span.y),1e-5);
}
float weatherPrecipPrimitive(float value)
{
    float u=clamp(value,0.0,1.0);
    return min(value,0.0)+u-u*u*u+0.5*u*u*u*u;
}
vec2 weatherPrecipUniformSpan(sampler2D minimum_map, vec3 q, vec3 d, float limit, vec2 thresholds,
    inout int level, int maximum_level)
{
    ivec2 base_size=textureSize(minimum_map,0);
    // Keep the hierarchy cursor between ray intervals. Restarting at the root
    // for every boundary multiplies texture fetches in a doorway by log(size).
    for(int candidate=level;candidate>=0;--candidate)
    {
        ivec2 size=max(base_size>>candidate,ivec2(1));
        float span=min(limit,weatherPrecipCellEnd(q.xy,d.xy,size));
        vec2 cell_position=q.xy*vec2(size);
        ivec2 cell=clamp(ivec2(mix(ceil(cell_position)-1.0,floor(cell_position),greaterThanEqual(d.xy,vec2(0)))),ivec2(0),size-1);
        vec2 heights=texelFetch(minimum_map,cell,candidate).rg;
        // Min/max proves EVERY child has the same exposure. An average or an
        // endpoint cannot prove that; ambiguous blocks refine to original cells.
        if(heights.x>=1.0 || max(q.z,q.z+d.z*span)<=heights.x+thresholds.x)
        {level=min(candidate+1,maximum_level);return vec2(span,1);}
        if(min(q.z,q.z+d.z*span)>=heights.y+thresholds.y)
        {level=min(candidate+1,maximum_level);return vec2(span,0);}
    }
    level=0;
    return vec2(0);
}
float weatherPrecipCellOpen(float z, float dz, float roof, float span)
{
    if(roof<=0.0)return 0.0;
    float a=(z-roof-0.00005)/0.00045, b=dz/0.00045;
    if(max(a,a+b*span)<=0.0)return span;
    if(min(a,a+b*span)>=1.0)return 0.0;
    if(abs(b)<1e-8)return span*(1.0-smoothstep(0.0,1.0,a));
    return clamp((weatherPrecipPrimitive(a+b*span)-weatherPrecipPrimitive(a))/b,0.0,span);
}
// Integrate only known cover. The loop is bounded by the supported grids'
// combined x/y crossings (including both advanced levels), independent of metres.
float weatherPrecipExposedDistance(vec3 start, vec3 direction, float distance_m)
{
    if (snow_cover_valid == 0) return 0.0;
#if WEATHER_PARTICLE_ADVANCED != 0
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
        float t=interval.x, exposed=0.0;
        ivec2 close_size=weatherShelterCloseSize();
        ivec2 distant_size=weatherShelterDistantSize();
        int close_maximum=findMSB(close_size.x)-1, distant_maximum=findMSB(distant_size.x)-1;
        int close_level=close_maximum, distant_level=distant_maximum;
        for (int i = 0; i < 8192; ++i)
        {
            if(t>=interval.y)break;
            float probe=t+1e-5;
            if(weather_shelter_minimum_valid!=0)
            {
                float remaining=interval.y-t;
                bool close_known=probe>=close_interval.x && probe<close_interval.y;
                bool distant_known=probe>=distant_interval.x && probe<distant_interval.y;
                float close_limit=probe<close_interval.x?close_interval.x-t:
                    close_known?close_interval.y-t:remaining;
                float distant_limit=probe<distant_interval.x?distant_interval.x-t:
                    distant_known?distant_interval.y-t:remaining;
                vec2 close_span=close_known?weatherPrecipUniformSpan(weather_shelter_close_min_map,
                    close_start+close_direction*probe,close_direction,close_limit,weatherShelterThresholds(true),close_level,close_maximum):vec2(close_limit,-1);
                vec2 distant_span=distant_known?weatherPrecipUniformSpan(weather_shelter_distant_min_map,
                    distant_start+distant_direction*probe,distant_direction,distant_limit,weatherShelterThresholds(false),distant_level,distant_maximum):vec2(distant_limit,-1);
                float uniform_span=min(remaining,min(close_span.x,distant_span.x));
                if((close_known || distant_known) && uniform_span>1e-4 &&
                    (!close_known || !distant_known || close_span.y==distant_span.y))
                {
                    exposed+=uniform_span*(close_known?close_span.y:distant_span.y);
                    t+=uniform_span;continue;
                }
            }
            float span=min(interval.y-t,min(
                weatherPrecipCellEnd((close_start+close_direction*probe).xy,close_direction.xy,close_size),
                weatherPrecipCellEnd((distant_start+distant_direction*probe).xy,distant_direction.xy,distant_size)));
            float midpoint=t+span*0.5;
            exposed += span*max(weatherShelterOpenCoordinates(close_start+close_direction*midpoint,
                distant_start+distant_direction*midpoint),0.0);
            t+=span;
        }
        return exposed;
    }
#endif
#if WEATHER_PARTICLE_ADVANCED != 1
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
    float t=lo, exposed=0.0;
    ivec2 size=textureSize(weather_surface_cover_map,0);
    int maximum_level=findMSB(size.x)-1, level=maximum_level;
    for (int i = 0; i < 4096; ++i)
    {
        if(t>=hi)break;
        vec3 q=a+d*(t+1e-5);
        if(weather_cover_minimum_valid!=0)
        {
            vec2 uniform_span=weatherPrecipUniformSpan(weather_surface_cover_min_map,q,d,hi-t,vec2(0.00005,0.0005),level,maximum_level);
            if(uniform_span.x>1e-4){exposed+=uniform_span.x*uniform_span.y;t+=uniform_span.x;continue;}
        }
        float step_m=min(hi-t,weatherPrecipCellEnd(q.xy,d.xy,size));
        float roof = texture(weather_surface_cover_map,(a+d*(t+step_m*0.5)).xy).r;
        exposed += weatherPrecipCellOpen((a+d*t).z,d.z,roof,step_m);
        t+=step_m;
    }
    return exposed;
#else
    return 0.0;
#endif
}
