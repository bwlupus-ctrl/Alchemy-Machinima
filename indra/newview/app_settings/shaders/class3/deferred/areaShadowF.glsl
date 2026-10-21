/** Phase 2: fixed disk samples, linear screen-depth visibility. No history. */
/*[EXTRA_CODE_HERE]*/
out vec4 frag_color;
in vec2 vary_fragcoord;
uniform sampler2D depthMap;
uniform vec4 area_lights[4]; // view-space centre, source radius
uniform uvec4 area_seeds;   // hash of light UUID, NOT channel
uniform int area_count;
uniform int area_samples;
uniform int area_steps;
uniform vec3 area_params; // max distance, thickness, pixels relative to 1080p
uniform vec2 area_output_res;
vec4 getPositionWithDepth(vec2 uv, float depth);
vec2 generateProjectedPosition(vec3 pos);
float getLinearDepth(vec2 uv);
vec3 areaNormal(vec2 uv);
uint areaHash(uint x)
{
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; return x ^ (x >> 16);
}
float areaRay(vec3 origin, vec3 target, float offset)
{
    vec3 delta = target-origin;
    float distance = length(delta);
    if (distance < 0.001) return 1.0;
    vec3 direction = delta/distance;
    float limit = min(distance,area_params.x);
    float dt = limit/float(area_steps);
    for (int step = 0; step < area_steps; ++step)
    {
        float t = min((float(step)+offset)*dt+0.05,limit);
        vec3 p = origin+direction*t;
        if (p.z >= -0.001) break; // behind camera is unsupported => lit
        vec2 uv = generateProjectedPosition(p);
        if (any(lessThan(uv,vec2(0))) || any(greaterThanEqual(uv,vec2(1)))) break;
        // SSGI design section 5.2 thickness test: depth behind the front surface.
        if (-p.z > getLinearDepth(uv)+area_params.y) return 0.0;
    }
    return 1.0;
}
void main()
{
    vec2 uv = vary_fragcoord;
    float depth = textureLod(depthMap,uv,0.0).r;
    frag_color = vec4(1);
    if (depth >= 1.0) return;
    vec3 pos = getPositionWithDepth(uv,depth).xyz;
    vec3 origin = pos+areaNormal(uv)*0.02;
    // Spatial noise cells scale with render resolution; no time/frame inputs.
    uvec2 pixel = uvec2(floor(gl_FragCoord.xy/max(area_params.z,0.125)));
    for (int light = 0; light < area_count; ++light)
    {
        vec3 delta = area_lights[light].xyz-origin;
        if (length(delta) < 0.0001) continue;
        vec3 direction = normalize(delta);
        vec3 tangent = normalize(cross(direction,abs(direction.z)<0.99 ? vec3(0,0,1):vec3(0,1,0)));
        vec3 bitangent = cross(direction,tangent);
        uint seed = areaHash(pixel.x ^ areaHash(pixel.y) ^ area_seeds[light]);
        float rotation = float(seed & 0xffffu)*(6.28318530718/65536.0);
        float jitter = float((seed >> 16) & 0xffffu)/65536.0;
        float visibility = 0.0;
        for (int sample_idx = 0; sample_idx < area_samples; ++sample_idx)
        {
            // Equal-area radial strata and fixed golden-angle azimuths.
            float radius = sqrt((float(sample_idx)+0.5)/float(area_samples))*area_lights[light].w;
            float angle = rotation+float(sample_idx)*2.39996322973;
            vec3 target = area_lights[light].xyz+radius*(cos(angle)*tangent+sin(angle)*bitangent);
            visibility += areaRay(origin,target,0.25+0.5*jitter);
        }
        frag_color[light] = visibility/float(area_samples);
    }
}
