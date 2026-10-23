/** Phase 2: fixed disk samples, linear screen-depth visibility. No history. */
/*[EXTRA_CODE_HERE]*/
out vec4 frag_color;
in vec2 vary_fragcoord;
uniform sampler2D depthMap;
uniform vec4 area_lights[4]; // view-space centre, source radius
uniform uvec4 area_seeds;   // hash of light UUID, NOT channel
uniform vec4 area_contact_dist; // contact hit distance; never changes ray samples
uniform vec4 area_contact_fade; // owning native slot's fade; zero = full visibility
uniform vec4 area_world; // P1-lite: 1 = this channel's light owns a world cube map this frame; the channel then outputs the CONTACT term only (the resolve takes the rest from the map)
uniform vec4 area_dropout; // per-channel fade-out: 0 = full soft shadow (default); 1 = none. 1 - the smooth group fade strength of that channel's light (admitted lights near the first dropped one fade, so none pops)
uniform vec4 area_regular_mix; // per-channel share of this light's rays that march the REGULAR way inside a Thin objects draw: 0 = all Thin (default); 1 = all regular. 1 - max(0.5, smoothstep(1, 1.05, score / score of the first regular light)) in production (the CPU's regularShare: tied lamps keep half of their rays Thin); at 1 the Thin light IS the regular light byte for byte (the same ray count, strata and offsets), which is what makes the two marches meet at the Thin/regular boundary
uniform int area_rest_steps; // the regular march's step count for those rays (only read where area_regular_mix > 0)
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
vec2 areaTrace(vec3 origin, vec3 target, vec3 light, float end_gap,
                float light_bias, float max_distance, float thickness, int steps, float offset,
                float contact_distance, bool thin);
uint areaHash(uint x)
{
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; return x ^ (x >> 16);
}
// P0.2: low-discrepancy noise. The rotation and the ray start of every noise cell come from the R2
// rank (Roberts 2018): frac(shift + a1*x + a2*y), a1 = 1/phi2, a2 = 1/phi2^2, evaluated in exact
// 32-bit integer arithmetic. Neighbouring cells then differ by a nearly constant phase, so the
// per-cell estimation error is high-frequency and the small spatial blur removes it. White
// per-cell hashes (the old seeds) put the same error at every frequency, where it cannot be
// blurred away. Two refinements keep a single lattice from printing a pattern:
//  * the ray start uses the lattice turned by 90 degrees, (-a2, a1): a start (a2, a1) made the sum of the two
//    phases a function of x + y alone, which printed 45-degree stripes at 0.02-0.09 cycles per pixel;
//  * the per-light Cranley-Patterson shift (the light UUID hash, never the channel) is also re-rolled per 8x8
//    block of cells, so no line of the lattice stays coherent beyond 8 cells (the same stripes would
//    otherwise survive at 1-5/255 whatever the constants are). Inside a block the lattice still
//    stratifies the rays.
const uint AREA_R2_A = 3242174889u; // 0.7548776662466927 * 2^32
const uint AREA_R2_B = 2447445413u; // 0.5698402909980532 * 2^32
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
    // Spatial noise cells scale with render resolution (one cell per pixel below 1); no time/frame inputs.
    uvec2 pixel = uvec2(floor(gl_FragCoord.xy/max(area_params.z,1.0)));
    for (int light = area_first; light < area_count; ++light)
    {
        // A pixel at window depth 0 is clamped to the near plane (the viewer clips such geometry): a world channel has no contact term there either.
        if (depth <= 0.0 && area_world[light] > 0.5) continue;
        vec3 delta = area_lights[light].xyz-origin;
        if (length(delta) < 0.0001) continue;
        vec3 direction = normalize(delta);
        vec3 tangent = normalize(cross(direction,abs(direction.z)<0.99 ? vec3(0,0,1):vec3(0,1,0)));
        vec3 bitangent = cross(direction,tangent);
        uvec2 block = pixel >> 3u;
        uint shift = areaHash(block.x ^ areaHash(block.y) ^ area_seeds[light]); // per light and 8x8 block
        uint seed = pixel.x*(0u-AREA_R2_B)+pixel.y*AREA_R2_A+areaHash(shift^0x9e3779b9u); // ray start rank
        float rotation = float((pixel.x*AREA_R2_A+pixel.y*AREA_R2_B+shift) >> 16)*(6.28318530718/65536.0);
        float jitter = float((seed >> 16) & 0xffffu)/65536.0;
        // Preserve Phase 1 radii; a point source gets a 10 cm shadow-only disk.
        float source_radius = area_lights[light].w > 0.0 ? area_lights[light].w : 0.1;
        float visibility = 0.0;
        float contact_visibility = 0.0;
        // World contact accepts a surface within 5 mm of the refined hit plane;
        // a camera-sized tolerance admits foreground silhouettes as contact.
        float thickness = area_world[light] > 0.5 ? min(area_params.y,0.005) : area_params.y;
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
            float ray_u = 1.0; // the ray's stratum in [0, 1): the Thin/regular boundary share below picks the rays by it
            if (area_thin_objects)
            {
                float phase = (float((seed >> 16) & 0xffffu)+0.5)/65536.0;
                int phase_idx = (sample_idx*37)%area_samples;
                ray_u = (float(phase_idx)+phase)/float(area_samples);
                offset = 0.15+0.8*(float(phase_idx)+phase)/float(area_samples);
            }
            float end_gap = max(source_radius,0.1)+area_light_bias;
            // Thin/regular boundary (hybrid plans): the first area_regular_mix[light] of the strata march the uniform
            // way, with the uniform offset and the regular step count, exactly the march of a regular light. The share
            // is 0 everywhere else, so every other ray is the unchanged march. The regular step count is the CPU's
            // (at most the authored uniform maximum, 64) and is NOT clamped to the Thin draw's steps (fix round 5): a
            // regular ray of a boundary light marches exactly as many steps as the regular light beside it, and the
            // planner charges the light max(Thin steps, regular steps).
            bool regular_ray = area_thin_objects && ray_u < area_regular_mix[light];
            vec2 traced;
            if (area_world[light] > 0.5)
            {
                // World channels need only the blind/contact band. A full Thin
                // march accepts unrelated foreground silhouettes as contact;
                // use the same short, bounded march with either Thin setting.
                traced = areaTrace(origin,target,area_lights[light].xyz,end_gap,
                    area_light_bias,min(area_params.x,area_contact_dist[light]),thickness,
                    min(area_steps,12),0.25+0.5*jitter,area_contact_dist[light],false);
            }
            else traced = regular_ray ?
                areaTrace(origin,target,area_lights[light].xyz,end_gap,area_light_bias,area_params.x,thickness,
                    clamp(area_rest_steps,1,64),0.25+0.5*jitter,area_contact_dist[light],false) :
                areaTrace(origin,target,area_lights[light].xyz,end_gap,
                    area_light_bias,area_params.x,thickness,area_steps,offset,area_contact_dist[light]);
            visibility += traced.x;
            contact_visibility += traced.y;
        }
        // Filter the blended visibility once, with the existing spatial passes.
        // area_dropout > 0 on the admitted channels whose light is nearly as important as the first dropped one (smooth group fade).
        frag_color[light] = mix(mix(visibility/float(area_samples),
            contact_visibility/float(area_samples),max(area_contact_fade[light]*native_strength,area_world[light])),1.0,area_dropout[light]);
    }
}
