/** Spatial depth/normal bilateral blur and full-resolution upsample. */
/*[EXTRA_CODE_HERE]*/
out vec4 frag_color;
in vec2 vary_fragcoord;
uniform sampler2D depthMap;
uniform sampler2D area_source;
uniform vec2 area_axis; // nonzero: separable blur, zero: bilateral upsample
uniform float area_radius;
uniform int area_debug_channel; // 0: filter normally, 1..4: exact grayscale visibility
uniform vec2 area_native_clip; // the gather's copy: the native spot sampler's shadow_clip.z/.w; w <= z (default 0) = valid everywhere
uniform float area_native_offset; // the gather's copy: the native sampler's spot_shadow_offset
uniform vec4 area_contact_fade; // the gather's copy, per channel: the owning native slot's fade; 0 = no native map (the hand-over stop is off)
vec3 areaNormal(vec2 uv);
vec2 areaTexelUV(vec2 uv);
vec4 getPositionWithDepth(vec2 uv, float depth);
// The native hand-over weight, EXACTLY as the gather computes it (areaShadowF.glsl areaNativeStrength; the window
// must match ALAreaShadow::kHandoverStart/kHandoverEnd, relight_rank_check.py pins both copies): 1 where the native
// map decides (contact only), 0 once the full screen trace decides.
float areaNativeStrength(float depth)
{
    if (isnan(depth) || isinf(depth)) return 1.0;
    if (area_native_clip.x <= 0.0 || area_native_clip.y <= area_native_clip.x) return 1.0;
    float u = (depth-area_native_clip.x)/(area_native_clip.y-area_native_clip.x);
    return 1.0-clamp((u-0.25)*5.0,0.0,1.0);
}
// P0.3: the GEOMETRIC normal of the visible surface, from depth alone (the min-difference derivative per axis,
// the rigRimAuxF method), one trace pixel apart. The G-buffer normal also carries normal maps, which are shading
// detail: using it as the plane normal of the edge-stop made normal-mapped walls and creases speckle. Falls back
// to the shading normal at silhouettes, on sky, on surfaces whose depth step is steep AND not planar (a depth
// step larger than 20 pixel footprints is a slope only when both one-sided differences agree: a grazing floor
// is one plane, a thin or edge-on object is not), beyond 128 m (the 24-bit depth is too coarse for derivatives
// there) and wherever the two normals disagree in sign.
bool areaPlanar(vec3 a, vec3 b)
{
    return length(a-b) <= 0.3*max(length(a),length(b));
}
vec3 areaGeoNormal(vec2 uv, vec2 step_uv, vec3 shading)
{
    uv = areaTexelUV(uv);
    float depth = textureLod(depthMap,uv,0.0).r;
    if (depth >= 1.0) return shading;
    vec3 c = getPositionWithDepth(uv,depth).xyz;
    if (-c.z > 128.0) return shading;
    vec2 ul = areaTexelUV(clamp(uv-vec2(step_uv.x,0),vec2(0),vec2(1)));
    vec2 ur = areaTexelUV(clamp(uv+vec2(step_uv.x,0),vec2(0),vec2(1)));
    vec2 ud = areaTexelUV(clamp(uv-vec2(0,step_uv.y),vec2(0),vec2(1)));
    vec2 uu = areaTexelUV(clamp(uv+vec2(0,step_uv.y),vec2(0),vec2(1)));
    float dl = textureLod(depthMap,ul,0.0).r, dr = textureLod(depthMap,ur,0.0).r;
    float dd = textureLod(depthMap,ud,0.0).r, du = textureLod(depthMap,uu,0.0).r;
    // A sky neighbour is no surface: make its difference infinite so the other side wins.
    vec3 l = dl >= 1.0 ? vec3(0,0,1.e9) : c-getPositionWithDepth(ul,dl).xyz;
    vec3 r = dr >= 1.0 ? vec3(0,0,1.e9) : getPositionWithDepth(ur,dr).xyz-c;
    vec3 d = dd >= 1.0 ? vec3(0,0,1.e9) : c-getPositionWithDepth(ud,dd).xyz;
    vec3 u = du >= 1.0 ? vec3(0,0,1.e9) : getPositionWithDepth(uu,du).xyz-c;
    vec3 dx = abs(r.z) < abs(l.z) ? r : l;
    vec3 dy = abs(u.z) < abs(d.z) ? u : d;
    if (abs(dx.z) > 20.0*length(dx.xy)+1.e-4 && !areaPlanar(l,r)) return shading;
    if (abs(dy.z) > 20.0*length(dy.xy)+1.e-4 && !areaPlanar(d,u)) return shading;
    vec3 n = cross(dx,dy);
    float l2 = dot(n,n);
    if (!(l2 > 1.e-20)) return shading;
    n *= inversesqrt(l2);
    if (dot(n,-c) < 0.0) n = -n;          // face the camera
    return dot(n,shading) < 0.0 ? shading : n;
}
// The centre pixel the taps are weighed against (set once in main()).
vec3 area_p;        // view-space position
vec3 area_n_geo;    // geometric normal from depth (P0.3)
vec3 area_n_shade;  // G-buffer (normal-mapped) normal
float area_footprint; // one trace pixel at the centre's depth, metres
float area_ns_p;    // native hand-over weight of the centre
bool area_sky;      // the centre is sky (depth 1): only sky taps count, a surface never averages the lit sky
// P0.3 edge-stops. A tap counts when it lies on the SAME PLANE as the centre
// (|dot(q-p, n_geo)| against max(2 cm, a tolerance of half a trace-pixel footprint at the first tap that grows
// with the tap distance up to 2 footprints)): a floor or wall seen at a grazing angle has a large depth step
// per pixel and is still one surface, which the old raw linear-depth difference tore apart. The price is that a
// real crease only cuts once its plane offset exceeds that tolerance (about 27 degrees at the first tap, 14 at the
// second, at one footprint per pixel); a shallower crease between a lit and a shadowed face still blends
// there, the old normal-mapped stop's behaviour that left brick and fabric grainy. The normal-mapped G-buffer normal only vetoes taps whose shading normals disagree by more than ~60
// degrees (a crease or a different object), never the +/-25 degree bumps of a normal map: pow(dot(n,n'),32)
// gave 0.04 weight at 25 degrees and left the raw ray noise on brick, stone and fabric.
// The hand-over stop is ONE-SIDED: a tap whose native weight is higher than the centre's carries contact-only
// data (the native map is still the authority there). The bilateral pass runs after the gather mixed native
// and screen visibility, so a traced pixel (weight 0) must never average such a tap, or the contact-only
// brightness leaks across the hand-over band into pixels whose native shadow has already faded.
// The stop is PER CHANNEL (a vec4 weight): only a light that owns a native map has contact-only data in the
// band, so the stop scales with that channel's contact fade and is fully off for a channel whose fade is 0
// (an unmapped light keeps its whole kernel across the band; cutting it there made a directional grain band).
// The tap's own hand-over weight is returned in ns_q (2 for a tap that is not a candidate at all), and its geometric weight BEFORE
// the hand-over stop (same plane, compatible shading normal; 0 for sky against surface) in geo_q: the upsample fallback below
// only considers taps that were geometrically on the centre's surface.
vec4 areaWeight(vec2 uv, float reach, out float ns_q, out float geo_q)
{
    ns_q = 2.0;
    geo_q = 0.0;
    uv = areaTexelUV(uv);
    float depth_q = textureLod(depthMap,uv,0.0).r;
    if ((depth_q >= 1.0) != area_sky) return vec4(0.0);
    vec3 q = getPositionWithDepth(uv,depth_q).xyz;
    vec3 nq = areaNormal(uv);
    float tol = max(0.02,area_footprint*clamp(0.25*reach,0.5,2.0));
    float plane = abs(dot(q-area_p,area_n_geo));
    float shading = smoothstep(0.0,0.5,dot(area_n_shade,nq));
    ns_q = areaNativeStrength(-(q.z+nq.z*area_native_offset));
    geo_q = exp(-plane/tol)*shading;
    float cut = mix(1.0,float(ns_q <= area_ns_p+1.0/64.0),1.0-smoothstep(0.0,0.5,area_ns_p));
    vec4 stop = mix(vec4(1.0),vec4(cut),clamp(area_contact_fade,0.0,1.0));
    return vec4(geo_q)*stop;
}
void main()
{
    vec2 uv = vary_fragcoord;
    if (area_debug_channel > 0)
    {
        float visibility = textureLod(area_source,uv,0.0)[area_debug_channel-1];
        frag_color = vec4(vec3(visibility),1.0);
        return;
    }
    vec2 res = vec2(textureSize(area_source,0));
    vec2 texel = areaTexelUV(uv);
    float depth = textureLod(depthMap,texel,0.0).r;
    area_sky = depth >= 1.0;
    area_p = getPositionWithDepth(texel,depth).xyz;
    area_n_shade = areaNormal(uv);
    area_n_geo = areaGeoNormal(uv,1.0/res,area_n_shade);
    area_ns_p = areaNativeStrength(-(area_p.z+area_n_shade.z*area_native_offset));
    // Footprint of one trace pixel at the centre's own depth (as the gather's receiver offset).
    area_footprint = max(length(getPositionWithDepth(texel+vec2(1.0/res.x,0),depth).xyz-area_p),
                         length(getPositionWithDepth(texel+vec2(0,1.0/res.y),depth).xyz-area_p));
    vec4 sum = vec4(0);
    vec4 weight = vec4(0);
    // Used where a channel's every tap was rejected. The separable passes always keep their centre tap (weight 1, its own
    // hand-over weight is the centre's), so there it is the centre texel; the upsample takes, among the taps that were
    // geometrically on the centre's surface (non-trivial weight BEFORE the hand-over stop), the one the CHANNEL prefers (below),
    // never a bilinear mix that would bring the contact band back in. When no tap is (a thin wall or a silhouette: every tap lies
    // across the edge or on the sky) it is the pixel's own texel, never a tap of another surface. Every fallback value is a
    // texelFetch of ONE source texel (fix round 4): a filtered textureLod at the pixel centre would be a bilinear mix of four texels
    // across exactly the edge this fallback exists to respect.
    ivec2 size_i = textureSize(area_source,0);
    vec4 fallback = texelFetch(area_source,clamp(ivec2(floor(uv*res)),ivec2(0),size_i-ivec2(1)),0);
    vec4 best_key = vec4(4.0);
    if (any(notEqual(area_axis,vec2(0))))
    {
        for (int tap = -8; tap <= 8; ++tap)
        {
            if (abs(float(tap)) > area_radius) continue;
            vec2 q = clamp(uv+area_axis*float(tap)/res,0.5/res,1.0-0.5/res);
            float ns_q, geo_q;
            vec4 w = areaWeight(q,abs(float(tap)),ns_q,geo_q)*exp(-float(tap*tap)/max(area_radius*area_radius,1.0));
            sum += textureLod(area_source,q,0.0)*w;
            weight += w;
        }
    }
    else
    {
        vec2 p = uv*res-0.5;
        vec2 base = floor(p);
        // Gather reconstructed the full-res texel actually fetched. Interpolate
        // between those positions, not the nominal half-res cell centres.
        vec2 first = areaTexelUV(clamp((base+0.5)/res,0.5/res,1.0-0.5/res));
        base -= vec2(lessThan(uv,first));
        vec2 lo = areaTexelUV(clamp((base+0.5)/res,0.5/res,1.0-0.5/res));
        vec2 hi = areaTexelUV(clamp((base+1.5)/res,0.5/res,1.0-0.5/res));
        vec2 f = clamp((uv-lo)/max(hi-lo,vec2(1.e-8)),0.0,1.0);
        // The pixel's OWN texel, in the SAME base alignment as the bilinear neighbourhood below (fix round 5): the nearest of the
        // four taps (f >= 1/2, a tie going to the higher texel like areaTexelUV's tie rule), not floor(uv*res), which ignores the
        // base shift above and can name a texel one off the neighbourhood on an odd-sized target.
        fallback = texelFetch(area_source,clamp(ivec2(base)+ivec2(greaterThanEqual(f,vec2(0.5-1.e-3))),ivec2(0),size_i-ivec2(1)),0);
        for (int y=0; y<2; ++y) for (int x=0; x<2; ++x)
        {
            vec2 q = clamp((base+vec2(x,y)+0.5)/res,0.5/res,1.0-0.5/res);
            float bilinear = (x==0 ? 1.0-f.x:f.x)*(y==0 ? 1.0-f.y:f.y);
            float ns_q, geo_q;
            vec4 w = areaWeight(q,1.0,ns_q,geo_q)*bilinear;
            vec4 value = textureLod(area_source,q,0.0);
            if (bilinear > 0.0 && geo_q > 0.05)
            {
                // PER CHANNEL: a channel that owns a native map prefers the most traced tap (the lowest hand-over weight), a channel
                // without one (contact fade 0, no stop) the nearest tap (the highest bilinear weight); ties go to the nearest too.
                vec4 key = ns_q*clamp(area_contact_fade,0.0,1.0)-vec4(0.001*bilinear);
                vec4 better = vec4(lessThan(key,best_key));
                vec4 fetched = texelFetch(area_source,clamp(ivec2(base)+ivec2(x,y),ivec2(0),size_i-ivec2(1)),0);
                fallback = mix(fallback,fetched,better);
                best_key = mix(best_key,key,better);
            }
            sum += value*w;
            weight += w;
        }
    }
    // If every neighbour is across an edge for a channel, take its fallback tap (never a bilinear mix across the stop).
    frag_color = vec4(weight.x > 1.e-8 ? sum.x/weight.x : fallback.x,
                      weight.y > 1.e-8 ? sum.y/weight.y : fallback.y,
                      weight.z > 1.e-8 ? sum.z/weight.z : fallback.z,
                      weight.w > 1.e-8 ? sum.w/weight.w : fallback.w);
}
