/**
 * @file weatherShelterF.glsl
 * @brief World-snapped, compare-first near/far precipitation reach.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork; native GL implementation.
 * $/LicenseInfo$
 */
/*[EXTRA_CODE_HERE]*/
uniform sampler2D weather_shelter_close_map;
uniform sampler2D weather_shelter_distant_map;
ivec2 weatherShelterCloseSize() { return textureSize(weather_shelter_close_map,0); }
ivec2 weatherShelterDistantSize() { return textureSize(weather_shelter_distant_map,0); }
uniform mat4 weather_shelter_close_matrix;
uniform mat4 weather_shelter_distant_matrix;
uniform vec4 weather_shelter_params; // bias m, softness m, close/distant vertical ranges
vec2 weatherShelterThresholds(bool close)
{
    return vec2(weather_shelter_params.x,weather_shelter_params.x+weather_shelter_params.y)/
        (close?weather_shelter_params.z:weather_shelter_params.w);
}
uniform int weather_shelter_debug;

float weatherShelterLayer(sampler2D map, vec3 q, float range_m)
{
    if (any(lessThan(q,vec3(0))) || any(greaterThan(q,vec3(1)))) return -1.0;
    // Compare depths before mixing exposures: never interpolate roof heights.
    float roof = texture(map,q.xy).r;
    if (roof >= 1.0) return 1.0; // Exact clear depth: no caster in this cell.
    float cover_m = (q.z-roof)*range_m;
    return 1.0-smoothstep(weather_shelter_params.x,
        weather_shelter_params.x+weather_shelter_params.y,cover_m);
}
float weatherShelterOpenCoordinates(vec3 close_q, vec3 distant_q)
{
    float close_open = weatherShelterLayer(weather_shelter_close_map,close_q,weather_shelter_params.z);
    float border = max(abs(close_q.x*2.0-1.0),abs(close_q.y*2.0-1.0));
    if (close_open >= 0.0 && border <= 0.70) return close_open;
    float distant_open = weatherShelterLayer(weather_shelter_distant_map,distant_q,weather_shelter_params.w);
    if (close_open < 0.0) return distant_open;
    if (distant_open < 0.0) return close_open;
    return mix(close_open,distant_open,smoothstep(0.70,0.95,border));
}
float weatherShelterOpen(vec3 agent)
{
    vec3 close_q = (weather_shelter_close_matrix*vec4(agent,1)).xyz*0.5+0.5;
    vec3 distant_q = (weather_shelter_distant_matrix*vec4(agent,1)).xyz*0.5+0.5;
    return weatherShelterOpenCoordinates(close_q,distant_q);
}
float weatherShelterSurfaceLayer(sampler2D map, vec3 q, float range_m)
{
    // Correct the receiver plane at the sampled texel's centre. A fixed bias
    // alone cannot prevent sloping exposed roofs from sheltering themselves.
    vec3 dx=dFdx(q),dy=dFdy(q);
    float determinant=dx.x*dy.y-dx.y*dy.x;
    vec2 slope=vec2(0);
    if(abs(determinant)>1e-20)
        slope=vec2(dy.y*dx.z-dx.y*dy.z,dx.x*dy.z-dy.x*dx.z)/determinant;
    if(any(lessThan(q,vec3(0))) || any(greaterThan(q,vec3(1))))return -1.0;
    vec2 size=vec2(textureSize(map,0));
    vec2 texel=clamp(floor(q.xy*size),vec2(0),size-1.0);
    float receiver=q.z+dot(slope,(texel+0.5)/size-q.xy);
    float roof=texture(map,q.xy).r;
    if(roof>=1.0)return 1.0;
    return 1.0-smoothstep(weather_shelter_params.x,
        weather_shelter_params.x+weather_shelter_params.y,(receiver-roof)*range_m);
}
float weatherShelterSurfaceOpen(vec3 agent)
{
    vec3 close_q=(weather_shelter_close_matrix*vec4(agent,1)).xyz*0.5+0.5;
    vec3 distant_q=(weather_shelter_distant_matrix*vec4(agent,1)).xyz*0.5+0.5;
    float close_open=weatherShelterSurfaceLayer(weather_shelter_close_map,close_q,weather_shelter_params.z);
    float distant_open=weatherShelterSurfaceLayer(weather_shelter_distant_map,distant_q,weather_shelter_params.w);
    if(close_open<0.0)return distant_open;
    if(distant_open<0.0)return close_open;
    float border=max(abs(close_q.x*2.0-1.0),abs(close_q.y*2.0-1.0));
    return mix(close_open,distant_open,smoothstep(0.70,0.95,border));
}
vec3 weatherShelterDebug(vec3 agent)
{
    float open = weatherShelterOpen(agent);
    if (open < 0.0) return vec3(0.3,0,0.3); // Missing coverage.
    vec3 q = (weather_shelter_close_matrix*vec4(agent,1)).xyz;
    float close_weight = 1.0-smoothstep(0.70,0.95,max(abs(q.x),abs(q.y)));
    return mix(vec3(0.03,0.08,0.3),vec3(0.03,0.3,0.08),close_weight)*(0.25+0.75*open);
}
