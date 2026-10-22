/** Phase 2: fixed disk samples, linear screen-depth visibility. No history. */
/*[EXTRA_CODE_HERE]*/
out vec4 frag_color;
in vec2 vary_fragcoord;
uniform sampler2D depthMap;
uniform vec4 area_lights[4]; // view-space centre, source radius
uniform uvec4 area_seeds;   // hash of light UUID, NOT channel
uniform vec4 area_contact_dist; // contact hit distance; never changes ray samples
uniform vec4 area_contact_fade; // owning native slot's fade; zero = full visibility
uniform int area_count;
uniform int area_first; // first channel in this budget-limited draw
uniform int area_samples;
uniform int area_steps;
uniform bool area_thin_objects;
uniform vec3 area_params; // max distance, thickness, pixels relative to 1080p
uniform float area_light_bias; // CPU-sanitized housing metres, independent of thickness
uniform vec2 area_native_clip; // the native spot sampler's shadow_clip.z/.w; w <= z (default 0) = valid everywhere
uniform float area_native_offset; // the native sampler's spot_shadow_offset (metres along the normal); default 0
vec4 getPositionWithDepth(vec2 uv, float depth);
vec2 generateProjectedPosition(vec3 pos);
float getLinearDepth(vec2 uv);
vec3 areaNormal(vec2 uv);
vec2 areaTexelUV(vec2 uv);
float areaReceiverBias(vec2 uv, vec3 pos);
vec2 areaTrace(vec3 origin, vec3 target, vec3 light, float end_gap,
                float light_bias, float max_distance, float thickness, int steps, float offset,
                float contact_distance);
uint areaHash(uint x)
{
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; return x ^ (x >> 16);
}
// Weight of the native SL projector shadow at this receiver depth. shadowUtil.glsl
// sampleSpotShadow() ramps to LIT over the last half of [clip.z, clip.w] (u = .5 .. 1), so
// the screen trace takes over across u = .25 .. .45, finished BEFORE that ramp starts (the
// 0.05 margin absorbs the bilateral filter, whose radius reaches 8 px and would otherwise
// leak a few /255 of the lit side): a mapped light's contact fade is scaled by this weight,
// and for a blocker both systems see every term of min(native, screen) stays dark
// (ALAreaShadow::nativeShadowStrength). The window MUST match ALAreaShadow::kHandoverStart
// (0.25) and kHandoverEnd (0.45): the slope below is 1/(0.45-0.25) = 5. The depth passed in
// is the SHIFTED receiver depth the native sampler compares (pos += norm *
// spot_shadow_offset first; ALAreaShadow::nativeReceiverDepth), never the raw one. A
// non-finite depth (NaN/Inf from a bad reconstruction) means strength 1.0, like the CPU
// isfinite rule.
float areaNativeStrength(float depth)
{
    if (isnan(depth) || isinf(depth)) return 1.0;
    if (area_native_clip.x <= 0.0 || area_native_clip.y <= area_native_clip.x) return 1.0;
    float u = (depth-area_native_clip.x)/(area_native_clip.y-area_native_clip.x);
    return 1.0-clamp((u-0.25)*5.0,0.0,1.0);
}
void main()
{
    vec2 uv = areaTexelUV(vary_fragcoord);
    float depth = textureLod(depthMap,uv,0.0).r;
    frag_color = vec4(1);
    if (depth >= 1.0) return;
    vec3 pos = getPositionWithDepth(uv,depth).xyz;
    vec3 normal = areaNormal(uv);
    vec3 origin = pos+normal*areaReceiverBias(uv,pos);
    float native_strength = areaNativeStrength(-(pos.z+normal.z*area_native_offset));
    // Spatial noise cells scale with render resolution; no time/frame inputs.
    uvec2 pixel = uvec2(floor(gl_FragCoord.xy/max(area_params.z,0.125)));
    for (int light = area_first; light < area_count; ++light)
    {
        vec3 delta = area_lights[light].xyz-origin;
        if (length(delta) < 0.0001) continue;
        vec3 direction = normalize(delta);
        vec3 tangent = normalize(cross(direction,abs(direction.z)<0.99 ? vec3(0,0,1):vec3(0,1,0)));
        vec3 bitangent = cross(direction,tangent);
        uint seed = areaHash(pixel.x ^ areaHash(pixel.y) ^ area_seeds[light]);
        float rotation = float(seed & 0xffffu)*(6.28318530718/65536.0);
        float jitter = float((seed >> 16) & 0xffffu)/65536.0;
        // Preserve Phase 1 radii; a point source gets a 10 cm shadow-only disk.
        float source_radius = area_lights[light].w > 0.0 ? area_lights[light].w : 0.1;
        float visibility = 0.0;
        float contact_visibility = 0.0;
        for (int sample_idx = 0; sample_idx < area_samples; ++sample_idx)
        {
            // Equal-area radial strata and fixed golden-angle azimuths.
            float radius = sqrt((float(sample_idx)+0.5)/float(area_samples))*source_radius;
            float angle = rotation+float(sample_idx)*2.39996322973;
            vec3 target = area_lights[light].xyz+radius*(cos(angle)*tangent+sin(angle)*bitangent);
            // One pixel/UUID offset, stratified across the disk rays. 37 is
            // coprime to every supported count (1..32), so this permutes N strata.
            // Keep step zero away from curved/grazing receivers: [0.15,0.95).
            float offset = 0.25+0.5*jitter;
            if (area_thin_objects)
            {
                float phase = (float((seed >> 16) & 0xffffu)+0.5)/65536.0;
                int phase_idx = (sample_idx*37)%area_samples;
                offset = 0.15+0.8*(float(phase_idx)+phase)/float(area_samples);
            }
            float end_gap = max(source_radius,0.1)+area_light_bias;
            vec2 traced = areaTrace(origin,target,area_lights[light].xyz,end_gap,
                area_light_bias,area_params.x,area_params.y,area_steps,offset,area_contact_dist[light]);
            visibility += traced.x;
            contact_visibility += traced.y;
        }
        // Filter the blended visibility once, with the existing spatial passes.
        frag_color[light] = mix(visibility/float(area_samples),
            contact_visibility/float(area_samples),area_contact_fade[light]*native_strength);
    }
}
