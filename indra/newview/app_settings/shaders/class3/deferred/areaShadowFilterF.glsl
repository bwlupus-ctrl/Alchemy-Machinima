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
// P1-lite world shadows (upsample pass only; zeros = byte-identical to a build without them). KEEP the face table, the bias constants
// and the tap counts in sync with alworldshadowmath.h (relight_shader_check.py pins them).
uniform vec4 area_world;          // per channel: 1 = this channel's light owns a world cube map this frame
uniform vec4 area_world_light[4]; // per channel: view-space centre xyz, far plane (metres)
uniform vec4 area_world_info[4];  // per channel: x first atlas tile row (2 k), y near plane, z map-side source radius, w unused
uniform uvec4 area_world_seed;    // per channel: light UUID hash (the rotation's Cranley-Patterson shift)
uniform vec3 area_world_rot[3];   // ROWS of the view -> agent rotation (the columns of the view matrix's 3x3): agent = (dot(r0,v), dot(r1,v), dot(r2,v))
uniform sampler2D area_world_atlas; // D32F atlas, 1632 x 1088 per light, compare mode NONE
uniform int area_world_debug;     // diagnostic (RenderAreaShadowWorldDebug), 0 = normal: 1 world visibility only, 2 face / 5, 3 receiver depth minus atlas depth
// [Soft shadow look] upsample pass only; DEVIATIONS from identity so zero (a program never given it) = today's output: x = 1 - Shadow strength,
// y = Shadow contrast - 1, z = 1 - World shadow amount (the world term of world-owned channels). KEEP IN SYNC with ALAreaShadow::lookUniform / lookApply (alareashadowmath.h).
uniform vec3 area_look;
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
float aw_centre_depth; // shared by the final world resolve; historical world-function substitutions retain this declaration
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

#if __VERSION__ >= 400
// World geometry is reconstructed only in the final resolve, at FULL depth
// resolution. It never consumes the G-buffer/material normal or trace spacing.
// Choose the smaller step at an edge, but judge it against the pixel footprint
// and D24 uncertainty, not a percentage of view depth: even a nearby background
// can turn a one-pixel strip's cross product sideways. Consistent opposing
// tangents still admit grazing planes. Widen the stencil as depth quantisation
// grows so a distant plane keeps its geometry, offset and early rejection.
float aw_plane_valid = 1.0;
float aw_depth_support = 0.0;
uniform mat4 inv_proj;
uniform vec4 area_world_depth; // inverse-projection depth denominator: high a,b; low a,b
vec3 awPosition(vec2 uv, float depth)
{
    vec4 h = inv_proj*vec4(uv*2.0-1.0,depth*2.0-1.0,1.0);
    // The homogeneous denominator is a subtraction of nearly equal numbers
    // at distance. One rounding (FMA) avoids a camera-dependent extra D24 unit.
    float ndc = depth*2.0-1.0;
    h.w = fma(area_world_depth.x,ndc,area_world_depth.y)
        + fma(area_world_depth.z,ndc,area_world_depth.w);
    return h.xyz/h.w;
}
float awTangentAgreement(vec3 a, vec3 b, float noise)
{
    return 1.0-smoothstep(0.1+noise,0.3+noise,
        length(a-b)/max(max(length(a),length(b)),1.e-9));
}
vec3 awCentreTangent(vec3 first, vec3 second, float noise)
{
    float scale = max(length(first),1.e-9);
    vec3 a = first/scale, b = (first+second)/scale;
    float bb = dot(b,b), ab = dot(a,b);
    vec3 centre = bb*(1.0-ab)*a+(bb-ab)*b;
    vec3 circle = a-centre*(dot(a,centre)/max(dot(centre,centre),1.e-12));
    vec3 unit_second = second*inversesqrt(max(dot(second,second),1.e-20));
    float bend = length(a-unit_second);
    return mix(1.5*first-0.5*second,scale*circle,smoothstep(noise,noise+0.05,bend));
}
// Only ambiguous axes fetch the next neighbour. A true surface continues on
// at least one side; a thin strip's first step onto its background does not.
// This also prevents the smaller-depth-step rule choosing a nearby silhouette
// instead of the receiver's steeper but consistent tangent on the other side.
float awTangent(ivec2 pixel, ivec2 size, ivec2 axis, int radius,
                vec3 l, vec3 r, float noise, float curve_noise, out vec3 tangent)
{
    bool left_ok = abs(l.z) < 1.e8, right_ok = abs(r.z) < 1.e8;
    if (!left_ok && !right_ok) { tangent = vec3(axis,0); return 0.0; }
    // Never interpolate a finite tangent with the sky sentinel: even a weight
    // of exactly one can lose the finite operand to cancellation in mix().
    l = left_ok ? l : r;
    r = right_ok ? r : l;
    // A tie between the two sides must not switch the tangent. The weights
    // are continuous even when camera motion reverses the smaller depth step.
    float side = smoothstep(-0.1,0.1,(abs(l.z)-abs(r.z))/max(max(length(l),length(r)),1.e-9));
    tangent = mix(l,r,side);
    float centre = float(left_ok && right_ok)*(1.0-smoothstep(0.1,0.3,length(l-r)/max(max(length(l),length(r)),1.e-9)));
    float valid = float(abs(tangent.z) < 1.e8);
    if (centre < 1.0)
    {
        ivec2 lp = max(pixel-2*radius*axis,ivec2(0)), rp = min(pixel+2*radius*axis,size-1);
        float dl = texelFetch(depthMap,lp,0).r, dr = texelFetch(depthMap,rp,0).r;
        vec3 ll = area_p-awPosition((vec2(lp)+0.5)/vec2(size),dl)-l;
        vec3 rr = awPosition((vec2(rp)+0.5)/vec2(size),dr)-area_p-r;
        float vl = left_ok && dl < 1.0 && any(notEqual(lp,max(pixel-radius*axis,ivec2(0)))) && dot(ll,ll) > 1.e-20
            ? awTangentAgreement(l,ll,noise) : 0.0;
        float vr = right_ok && dr < 1.0 && any(notEqual(rp,min(pixel+radius*axis,size-1))) && dot(rr,rr) > 1.e-20
            ? awTangentAgreement(r,rr,noise) : 0.0;
        if (left_ok && dl < 1.0 && dot(ll,ll) > 1.e-20)
            vl = max(vl,1.0-smoothstep(0.3+noise,0.6+noise,length(l-ll)/max(max(length(l),length(ll)),1.e-9)));
        if (right_ok && dr < 1.0 && dot(rr,rr) > 1.e-20)
            vr = max(vr,1.0-smoothstep(0.3+noise,0.6+noise,length(r-rr)/max(max(length(r),length(rr)),1.e-9)));
        // A small step onto a nearby background can imitate curvature with
        // only two secants. Its third secant is flat, whereas real curvature
        // continues. Test the change of curvature before extrapolating back
        // onto a subpixel card; depth uncertainty widens this continuously.
        ivec2 l3p = max(pixel-3*radius*axis,ivec2(0)), r3p = min(pixel+3*radius*axis,size-1);
        float dl3 = texelFetch(depthMap,l3p,0).r, dr3 = texelFetch(depthMap,r3p,0).r;
        vec3 lll = area_p-awPosition((vec2(l3p)+0.5)/vec2(size),dl3)-l-ll;
        vec3 rrr = awPosition((vec2(r3p)+0.5)/vec2(size),dr3)-area_p-r-rr;
        float lc = dl3 < 1.0 && any(notEqual(l3p,lp))
            ? 1.0-smoothstep(curve_noise,curve_noise+0.05,length(ll-lll)/max(max(length(ll),length(lll)),1.e-9)) : 0.0;
        float rc = dr3 < 1.0 && any(notEqual(r3p,rp))
            ? 1.0-smoothstep(curve_noise,curve_noise+0.05,length(rr-rrr)/max(max(length(rr),length(rrr)),1.e-9)) : 0.0;
        vl *= 1.0-lc*smoothstep(0.1+noise,0.3+noise,length(l-ll)/max(max(length(l),length(ll)),1.e-9));
        vr *= 1.0-rc*smoothstep(0.1+noise,0.3+noise,length(r-rr)/max(max(length(r),length(rr)),1.e-9));
        vl *= float(left_ok); vr *= float(right_ok);
        // Extrapolate a coherent one-sided secant back to the centre. Without
        // this quadratic correction its normal belongs half a pixel away,
        // enough to shadow convex skin at a silhouette. Confidence makes the
        // correction vanish continuously at a discontinuity.
        vec3 tl = mix(l,awCentreTangent(l,ll,curve_noise),vl);
        vec3 tr = mix(r,awCentreTangent(r,rr,curve_noise),vr);
        // Coherent continuation wins. When both sides are coherent, retain
        // the smaller-depth-step preference continuously at a crease rather
        // than averaging tangents from two different receiver surfaces.
        float resolved_side = 1.0/(1.0+curve_noise*curve_noise);
        float choice = vr-vl+0.1*(2.0*side-1.0)*min(vl,vr)*resolved_side;
        tangent = mix(tl,tr,smoothstep(-0.1,0.1,choice));
        valid = max(awTangentAgreement(l,r,noise)*valid,max(vl,vr));
        centre *= vl*vr;
    }
    tangent = mix(tangent,0.5*(l+r),centre*valid);
    valid = smoothstep(0.0,0.25,valid);
    return valid;
}
vec4 awGeoStencil(ivec2 pixel, ivec2 size, int radius, float footprint, float uncertainty)
{
    vec2 step_uv = 1.0/vec2(size);
    // Reuse the filter's constant-depth footprint, scaled to full resolution.
    // |dz/dd| times two D24 units includes storage and inverse-projection
    // rounding, without another depth fetch or inverse-projection division.
    ivec2 lo = max(pixel-ivec2(radius),ivec2(0)), hi = min(pixel+ivec2(radius),size-1);
    footprint *= float(radius);
    vec2 ul = (vec2(lo.x,pixel.y)+0.5)*step_uv, ur = (vec2(hi.x,pixel.y)+0.5)*step_uv;
    vec2 ud = (vec2(pixel.x,lo.y)+0.5)*step_uv, uu = (vec2(pixel.x,hi.y)+0.5)*step_uv;
    float dl = texelFetch(depthMap,ivec2(lo.x,pixel.y),0).r;
    float dr = texelFetch(depthMap,ivec2(hi.x,pixel.y),0).r;
    float dd = texelFetch(depthMap,ivec2(pixel.x,lo.y),0).r;
    float du = texelFetch(depthMap,ivec2(pixel.x,hi.y),0).r;
    // A resolved constant-depth patch has an exact view-space normal. Require
    // subpixel depth uncertainty before using this path; distant quantisation
    // plateaus must still use the adaptive stencil. Include the centre, so an
    // isolated receiver surrounded by one background never qualifies.
    if (uncertainty <= 0.02*footprint && all(equal(vec4(dl,dr,dd,du),vec4(aw_centre_depth))))
    {
        return vec4(0,0,1,1);
    }
    vec3 l = dl < 1.0 && lo.x != pixel.x ? area_p-awPosition(ul,dl) : vec3(0,0,1.e9);
    vec3 r = dr < 1.0 && hi.x != pixel.x ? awPosition(ur,dr)-area_p : vec3(0,0,1.e9);
    vec3 d = dd < 1.0 && lo.y != pixel.y ? area_p-awPosition(ud,dd) : vec3(0,0,1.e9);
    vec3 u = du < 1.0 && hi.y != pixel.y ? awPosition(uu,du)-area_p : vec3(0,0,1.e9);
    vec3 dx, dy;
    float curve_noise = 4.0*uncertainty/max(footprint,1.e-9);
    float noise = min(curve_noise,0.2);
    float vx = awTangent(pixel,size,ivec2(1,0),radius,l,r,noise,curve_noise,dx);
    float vy = awTangent(pixel,size,ivec2(0,1),radius,d,u,noise,curve_noise,dy);
    vec3 n = cross(dx,dy), facing = normalize(-area_p);
    float n2 = dot(n,n);
    float valid = vx*vy;
    if (!(n2 > 1.e-20)) return vec4(facing,0);
    n *= inversesqrt(n2);
    n = dot(n,facing) < 0.0 ? -n : n;
    // Window depth is affine on a plane. Symmetric samples recover fractional
    // D24 bits lost at the centre without displacing a receiver along its
    // normal. Sum residuals, rather than nearly-one window depths.
    float plane_residual = max(max(abs(dot(n,l)),abs(dot(n,r))),max(abs(dot(n,d)),abs(dot(n,u))));
    float planar = 1.0-smoothstep(0.01*footprint+2.0*uncertainty,0.05*footprint+2.0*uncertainty,plane_residual);
    float support_weight = valid*planar*float(all(lessThan(vec4(dl,dr,dd,du),vec4(1.0))));
    aw_depth_support = support_weight;
    // Project primary-depth uncertainty onto the geometric plane separately
    // from the fixed shadow bias and normal-offset cap. Retain the plane and
    // its validity at distance; compensate only precision beyond 0.1 mm.
    return vec4(n,valid);
}
float awDepthCentre(ivec2 pixel, ivec2 size, ivec2 axis)
{
    float lower = -0.5, upper = 0.5;
    // Fetch on demand: arrays here force private per-fragment storage even
    // for near pixels that never enter refinement, slowing the entire pass.
    for (int i = 0; i < 16; ++i)
    {
        ivec2 lp = max(pixel-(i+1)*8*axis,ivec2(0));
        vec2 l = vec2(float(dot(pixel-lp,axis)),(texelFetch(depthMap,lp,0).r-aw_centre_depth)*16777216.0);
        for (int j = 0; j < 16; ++j)
        {
            ivec2 rp = min(pixel+(j+1)*8*axis,size-1);
            vec2 r = vec2(float(dot(rp-pixel,axis)),(texelFetch(depthMap,rp,0).r-aw_centre_depth)*16777216.0);
            float centre = (r.x*l.y+l.x*r.y)/max(l.x+r.x,1.0);
            lower = max(lower,centre-0.5);
            upper = min(upper,centre+0.5);
        }
    }
    return (lower+upper)*(0.5/16777216.0)*(1.0-smoothstep(0.0,0.1,lower-upper));
}
vec3 awGeoNormal(vec2 uv)
{
    ivec2 size = textureSize(depthMap,0), pixel = ivec2(uv*vec2(size));
    float footprint = area_footprint*min(float(textureSize(area_source,0).x)/float(size.x),
                                        float(textureSize(area_source,0).y)/float(size.y));
    float uncertainty = abs(inv_proj[2][3])*area_p.z*area_p.z/4194304.0;
    // Keep resolved flat pixels out of the adaptive/curvature machinery. This
    // is the exact endpoint of the general stencil, not a different normal.
    if (uncertainty <= 0.02*footprint)
    {
        ivec2 lo = max(pixel-1,ivec2(0)), hi = min(pixel+1,size-1);
        vec4 depth = vec4(texelFetch(depthMap,ivec2(lo.x,pixel.y),0).r,
                          texelFetch(depthMap,ivec2(hi.x,pixel.y),0).r,
                          texelFetch(depthMap,ivec2(pixel.x,lo.y),0).r,
                          texelFetch(depthMap,ivec2(pixel.x,hi.y),0).r);
        if (all(equal(depth,vec4(aw_centre_depth))))
        {
            aw_plane_valid = 1.0;
            return vec3(0,0,1);
        }
    }
    float radius = clamp(uncertainty/max(0.005*footprint,1.e-8)-16.0,1.0,128.0);
    vec4 geometry = awGeoStencil(pixel,size,1,footprint,uncertainty);
    // Nested supports form a continuous partition: at each endpoint the next
    // interval starts with exactly the previous result. A wider support may
    // refine a coherent fine normal, but can never invalidate it at an edge.
    for (int r = 2; r <= 128; r *= 2)
    {
        float weight = smoothstep(float(r/2),float(r),radius);
        if (weight <= 0.0) break;
        float previous_support = aw_depth_support;
        vec4 coarse = awGeoStencil(pixel,size,r,footprint,uncertainty);
        float angular_noise = min(4.0*uncertainty/max(footprint*float(r/2),1.e-9),0.2);
        float agreement = 1.0-smoothstep(0.1+angular_noise,0.3+angular_noise,length(geometry.xyz-coarse.xyz));
        coarse.w *= mix(1.0,agreement,geometry.w);
        aw_depth_support = mix(previous_support,aw_depth_support,weight*coarse.w);
        geometry.xyz = normalize(mix(geometry.xyz,coarse.xyz,weight*coarse.w));
        geometry.w = mix(geometry.w,max(geometry.w,coarse.w),weight);
    }
    vec3 facing = normalize(-area_p);
    aw_plane_valid = geometry.w;
    vec3 normal = normalize(mix(facing,geometry.xyz,aw_plane_valid));
    float refine = smoothstep(0.0001,0.0002,uncertainty)*aw_depth_support;
    if (refine > 0.0)
    {
        // Each D24 sample constrains the affine plane to a half-unit interval.
        // Interpolate every left/right pair back to this pixel and intersect
        // those intervals. This recovers fractional centre-depth bits without
        // trusting the already quantized centre or a normal displacement.
        float axis_weight = smoothstep(-0.1,0.1,(abs(normal.x)-abs(normal.y))/max(length(normal.xy),1.e-9));
        float correction = 0.0;
        if (axis_weight > 0.0) correction += axis_weight*awDepthCentre(pixel,size,ivec2(1,0));
        if (axis_weight < 1.0) correction += (1.0-axis_weight)*awDepthCentre(pixel,size,ivec2(0,1));
        correction *= refine;
        // Keep the recovered fractional bits through inverse projection.
        float ndc = aw_centre_depth*2.0-1.0;
        float denominator = fma(area_world_depth.x,ndc,area_world_depth.y)
                          + fma(area_world_depth.z,ndc,area_world_depth.w);
        area_p *= denominator/(denominator+2.0*correction*(area_world_depth.x+area_world_depth.z));
    }
    // Precision is a compare interval, never an additional displacement.
    return normal;
}
float awDepthFraction()
{
    float uncertainty = abs(inv_proj[2][3])*area_p.z*area_p.z/4194304.0;
    // One D24 storage/conversion unit plus a 10% float/FMA rounding margin.
    // The small-error endpoint is covered by the existing fixed compare bias.
    float error = 0.55*uncertainty;
    return error*smoothstep(0.0001,0.0002,error)/max(abs(area_p.z),1.e-9);
}
float awDepthError()
{
    return awDepthFraction()*abs(dot(area_n_geo,area_p));
}
#define aw_depth_error awDepthError()
// ------------------------------------------------------------------------------------------------------------------
// World cube shadow resolve (P1-lite). Full resolution, never blurred: vis = min(upsampled screen contact, world).
// Face f: look axis, right, up (alworldshadowmath.h kFaceTable; the probe cube order +X -X +Y -Y +Z -Z).
const vec3 AW_AXIS[6]  = vec3[6](vec3(1,0,0), vec3(-1,0,0), vec3(0,1,0), vec3(0,-1,0), vec3(0,0,1), vec3(0,0,-1));
const vec3 AW_RIGHT[6] = vec3[6](vec3(0,0,-1), vec3(0,0,1), vec3(1,0,0), vec3(1,0,0), vec3(1,0,0), vec3(-1,0,0));
const vec3 AW_UP[6]    = vec3[6](vec3(0,-1,0), vec3(0,-1,0), vec3(0,0,1), vec3(0,0,-1), vec3(0,-1,0), vec3(0,-1,0));
// 512 usable texels + 16 gutter per side: tile 544, texel = 272 + 256 * (x / z)  (alworldshadowmath.h faceSample).
const float AW_TILE = 544.0, AW_HALF_TILE = 272.0, AW_HALF_USABLE = 256.0, AW_USABLE = 512.0;
// One constant set for every scene (kNormalOffsetTexels, kNormalOffsetCap, kConstantBias, kSlopeTexels, kTanCap).
const float AW_NORMAL_TEXELS = 1.0, AW_NORMAL_CAP = 0.05, AW_BIAS_CONST = 0.003, AW_BIAS_SLOPE = 1.0, AW_TAN_CAP = 8.0;
float awReceiverOffset(float texel_w, bool detail)
{
    return detail ? 0.0 : aw_plane_valid*min(AW_NORMAL_TEXELS*texel_w,AW_NORMAL_CAP);
}
const float AW_OVERLAP = 0.95;     // a face also reads a ray whose axis component is within 5 % of the dominant one (gutter: 5.3 %, less the gather footprint)
const float AW_MAX_RADIUS = 12.0;  // texels; 12 + 2 <= the 16 texel gutter
// A bilinear gather already spans a texel in each axis. Use its half-diagonal
// as the minimum extra disk radius so tiny emitters retain their source size.
const float AW_MIN_RADIUS = 0.70710678;
const int AW_SEARCH_TAPS = 16, AW_FILTER_TAPS = 32;
vec2 awPrecision(float nl, float footprint)
{
    float fraction = awDepthFraction();
    if (fraction > 0.0)
    {
        vec2 size = vec2(textureSize(depthMap,0));
        float pixel_angle = 2.0*min(abs(inv_proj[0][0])/size.x,abs(inv_proj[1][1])/size.y);
        // D24 also bounds the direction error of a finite difference. Apply
        // that uncertainty over the light's actual support, in the compare.
        float angular_error = 2.0*fraction/max(pixel_angle,1.e-9);
        float incidence = max(abs(nl),0.05);
        float plane_error = fraction*abs(dot(area_n_geo,area_p))/incidence;
        // Forward rejection cannot consume a precision interval wider than a
        // retained contact. Larger uncertainty belongs only to the self-plane
        // comparison; it must never hide a real foreground slab.
        return vec2(plane_error+angular_error*footprint/incidence,min(plane_error,AW_NORMAL_CAP));
    }
    return vec2(0.0);
}
float awBackfaceWeight(float nl, float distance, float source_radius)
{
    // These are exact saturated endpoints of the conservative band. Keep
    // ordinary front-facing pixels out of the uncertainty calculation.
    if (nl >= 0.0) return 1.0;
    float source = source_radius/sqrt(distance*distance+source_radius*source_radius);
    if (nl >= -source) return 1.0;
    vec2 size = vec2(textureSize(depthMap,0));
    float pixel_angle = 2.0*min(abs(inv_proj[0][0])/size.x,abs(inv_proj[1][1])/size.y);
    float reconstruction = 2.0*awDepthFraction()/max(pixel_angle,1.e-9);
    // Once a retained contact is smaller than the primary-depth interval,
    // a thin receiver and its background can share identical D24 values.
    // Such a stencil cannot certify a back face, even if it looks flat.
    float unresolved = smoothstep(0.00015,AW_BIAS_CONST,awDepthFraction()*abs(area_p.z));
    // An uncertain stencil has a correspondingly wide angular confidence
    // cone. Evaluate the whole source support, including either sign of nl.
    // Both independently reconstructed tangent axes contribute to the
    // normal's uncertainty cone, including partially valid thin receivers.
    float band = min(1.0,source+reconstruction+unresolved+sqrt(2.0*max(1.0-aw_plane_valid*aw_plane_valid,0.0)));
    return smoothstep(-2.0*max(band,1.e-6),-max(band,1.e-6),nl);
}
// Wide direction-space footprints compress their rearward quarter toward the
// light plane. The depth bound stays authoritative; this preserves curved
// contacts without adding gathers. The face morph carries this in smoothly.
const float AW_PLANE_COMPRESS = 0.25;
const uint AW_R2_A = 3242174889u, AW_R2_B = 2447445413u;
int awFace(vec3 d)
{
    vec3 a = abs(d);
    if (a.x >= a.y && a.x >= a.z) return d.x >= 0.0 ? 0 : 1;
    if (a.y >= a.z) return d.y >= 0.0 ? 2 : 3;
    return d.z >= 0.0 ? 4 : 5;
}
// Metres along the face axis for a window depth d (the inverse of the face projection).
vec4 awLinear(vec4 d, float n, float f)
{
    return (2.0*f*n)/((f+n)-(2.0*d-1.0)*(f-n));
}
// Vogel disk tap k of n on the unit disk, turned by rot. includeCentre: tap 0 sits at the centre.
vec2 awVogel(int k, int n, float rot, bool includeCentre)
{
    float r = sqrt((float(k)+(includeCentre ? 0.0 : 0.5))/float(n));
    float a = float(k)*2.39996322973+rot;
    return r*vec2(cos(a),sin(a));
}
// Linearised metres of the 2x2 texel footprint around the texel CORNER `corner` (atlas texels). The gather is
// addressed at an exact texel corner, never at a free coordinate, so the driver's footprint rounding cannot pick a
// different quad.
vec4 awGather(vec2 corner, vec2 inv_size, float n, float f)
{
    vec4 depth = textureGather(area_world_atlas,corner*inv_size);
    // Preserve the exact clear sentinel. Inverting depth 1 in float can round
    // just BELOW far, defeating the empty-gather early return on every tap.
    return mix(awLinear(depth,n,f),vec4(f),equal(depth,vec4(1.0)));
}
// Lit fraction of one bilinear compare tap (atlas texel coordinates p, texel centres at +0.5): a texel is lit when its
// blocker depth is not nearer than the receiver minus the bias, in metres.
// A tap may only see casters on the LIGHT side of the receiver. A tangent
// plane at grazing incidence can intersect an off-axis ray arbitrarily far
// behind it (including normal-map detail). Bound every gathered corner by
// the plane through the receiver perpendicular to its light vector. This is
// the same test in all three paths, in each face's axial depth units.
vec4 awReceiverFront(vec4 z, float bias, vec3 forward, float receiver2, vec2 base)
{
    vec2 pixel = base+0.5-AW_HALF_TILE;
    float den = forward.x+dot(pixel,forward.yz);
    float dr = forward.y, du = forward.z;
    // Cross-multiplied depth ceiling: no four extra divisions per gather.
    return vec4(lessThan((z+bias)*max(vec4(den+du,den+dr+du,den+dr,den),vec4(1.e-5)),vec4(receiver2)));
}
// Blend COMPARES, never visibility toward lit. Both branches retain a
// foreground blocker; the forward-depth bound is applied to both by callers.
// A missing/discontinuous depth stencil uses the plain slope-biased compare.
vec4 awBlocked(vec4 z, vec4 plane, float zr, float bias, vec4 horizon)
{
    // Reliability belongs to the reconstructed surface, not the light angle.
    // Switching a coherent plane to the axial compare at the horizon shadows
    // the receiver's own convex surface. Partial support still blends below.
    vec4 validity = vec4(aw_plane_valid);
    // Fully valid planes are the common case. Keep their original one-compare
    // cost; do not evaluate the fallback compare and two blends at every tap.
    if (all(greaterThanEqual(validity,vec4(1.0)))) return vec4(lessThan(z,plane-bias));
    vec4 plain = vec4(lessThan(z,vec4(zr-bias)));
    vec4 geometric = vec4(lessThan(z,plane-bias));
    // A partial stencil must not promote off-axis background into a caster
    // merely because its axial depth is small. The fallback remains inside
    // the self-plane compare; full foreground coverage is confirmed below.
    plain *= mix(vec4(1.0),geometric,horizon);
    if (all(lessThanEqual(validity,vec4(0.0)))) return plain;
    return mix(plain,geometric,validity);
}
vec4 awPlaneWeight(vec4 den)
{
    if (all(lessThanEqual(den,vec4(-0.004)))) return vec4(1.0);
    if (all(greaterThanEqual(den,vec4(-0.001)))) return vec4(0.0);
    return vec4(1.0)-smoothstep(vec4(-0.004),vec4(-0.001),den);
}
// At the source horizon a coherent plane can correctly reject every self
// sample. Still inspect the original centre to seed the full filter; its actual
// foreground coverage confirms a caster. Do not switch this confirmation at
// an angle or confidence threshold. An invalid stencil must not erase a
// confirmed small contact. Only if the normal search found nothing, inspect
// the centre with the constant bias and keep its real blocker depth. This is
// still a search result: the full filter below decides coverage/penumbra.
vec2 awFallbackBlocker(int c, vec3 d, float bias, float depth_scale, float receiver2)
{
    int face = awFace(d);
    float zr = dot(d,AW_AXIS[face]);
    vec2 centre = AW_HALF_TILE+AW_HALF_USABLE*vec2(dot(d,AW_RIGHT[face]),dot(d,AW_UP[face]))/zr;
    vec2 base = floor(clamp(centre-0.5,vec2(0),vec2(AW_TILE-2.0)));
    vec2 tile = vec2(AW_TILE*float(face%3),AW_TILE*(area_world_info[c].x+float(face/3)));
    vec4 z = awGather(tile+base+1.0,1.0/vec2(textureSize(area_world_atlas,0)),area_world_info[c].y,area_world_light[c].w);
    vec3 forward = vec3(zr,dot(d,AW_RIGHT[face])/AW_HALF_USABLE,dot(d,AW_UP[face])/AW_HALF_USABLE);
    float front_bias = area_world_info[c].w > 0.5 ? 0.00015 : AW_BIAS_CONST;
    vec4 front = awReceiverFront(z,front_bias,forward,receiver2,base);
    return vec2(dot(z,front)*depth_scale,dot(front,vec4(1.0)));
}
vec3 awPlaneTangent(vec3 normal, vec3 direction)
{
    vec3 tangent = normal-dot(normal,direction)*direction;
    // Fade the compression out near a frontal receiver: normal quantisation
    // must not choose an arbitrary tangent when the projected normal is zero.
    return tangent*inversesqrt(max(dot(tangent,tangent),0.01));
}
vec2 awTap(vec2 p, vec2 lo, vec2 hi, vec2 inv_size, float zr, float bias, float n, float f, vec3 forward, float receiver2, float plane_weight)
{
    vec2 q = clamp(p-0.5,lo,hi);
    vec2 base = floor(q);
    vec2 fr = q-base;
    vec4 z = awGather(base+1.0,inv_size,n,f);   // x=(0,1) y=(1,1) z=(1,0) w=(0,0)
    vec4 front = awReceiverFront(z,AW_BIAS_CONST,forward,receiver2,base-lo);
    vec4 blocked = awBlocked(z,vec4(zr),forward.x,bias,vec4(plane_weight))*front;
    vec4 lit = vec4(1.0)-blocked;
    return vec2(mix(mix(lit.w,lit.z,fr.x),mix(lit.x,lit.y,fr.x),fr.y),
                1.0-mix(mix(front.w,front.z,fr.x),mix(front.x,front.y,fr.x),fr.y));
}
// Receiver-plane depth (Isidoro 2006): the axial depth at which the ray of face slope `slope` meets the receiver's tangent
// plane (normal n, plane through the offset receiver: nd = n . (P' - C)). Comparing each tap against ITS OWN plane depth
// instead of the centre's keeps a floor seen at a grazing angle from shadowing itself across the filter footprint (a
// constant bias would need to grow with the tap distance). An upper slope cap bounds extrapolation; a lower clamp
// would lift the plane above a grazing floor and classify its own depth as a blocker. Keep the
// continuous denominator where the ray is parallel to the plane. The shared
// forward-depth test remains the authority for the upper bound.
float awPlane(vec3 n, vec3 ax, vec3 rt, vec3 up, float nd, vec2 slope, float zr, float cap)
{
    float den = dot(n,ax+slope.x*rt+slope.y*up);
    return min(nd/min(den,-1.e-3),zr+cap);
}
float awLocalPlane(vec3 n, float nd, vec2 slope, float zr, float cap)
{
    return min(nd/min(n.x+dot(n.yz,slope),-1.e-3),zr+cap);
}
// Detail fixtures compare each gathered texel with the receiver plane at its
// own ray, including face interiors. The offset/bias above remain bounded;
// the geometric plane is bounded by the light-facing receiver plane too.
vec2 awDetailFaceTap(int c, int face, vec2 centre, vec3 normal, float nd, float bias, vec3 forward, float receiver2, bool search)
{
    vec2 q = clamp(centre-0.5,vec2(0.0),vec2(AW_TILE-2.0));
    vec2 base = floor(q), f = q-base;
    vec2 tile = vec2(AW_TILE*float(face%3),AW_TILE*(area_world_info[c].x+float(face/3)));
    vec4 z = awGather(tile+base+1.0,1.0/vec2(textureSize(area_world_atlas,0)),area_world_info[c].y,area_world_light[c].w);
    if (all(greaterThanEqual(z,vec4(area_world_light[c].w)))) return search ? vec2(0.0) : vec2(1.0);
    vec2 slope = (base+0.5-AW_HALF_TILE)/AW_HALF_USABLE;
    // normal is already expressed in this face's axis/right/up basis.
    float den = normal.x+dot(normal.yz,slope);
    float dr = normal.y/AW_HALF_USABLE;
    float du = normal.z/AW_HALF_USABLE;
    vec4 dens = vec4(den+du,den+dr+du,den+dr,den);
    vec4 front = awReceiverFront(z,0.00015,forward,receiver2,base);
    // The same bounded proof as the direction tap, for small-source detail.
    if (nd < 0.0 && aw_plane_valid >= 1.0)
    {
        float lower_plane = nd/min(min(min(dens.x,dens.y),min(dens.z,dens.w)),-1.e-3)*0.999999;
        if (max(max(z.x,z.y),max(z.z,z.w)) < lower_plane-bias && all(greaterThanEqual(front,vec4(1.0))))
            return search ? vec2(dot(z,vec4(1.0)),4.0) : vec2(0.0);
    }
    vec4 plane = vec4(nd)/min(dens,vec4(-1.e-3));
    vec4 blocked = awBlocked(z,plane,forward.x,bias,awPlaneWeight(dens))*front;
    if (search) return vec2(dot(z,blocked),dot(blocked,vec4(1.0)));
    return vec2(1.0-mix(mix(blocked.w,blocked.z,f.x),mix(blocked.x,blocked.y,f.x),f.y),
                1.0-mix(mix(front.w,front.z,f.x),mix(front.x,front.y,f.x),f.y));
}
float awFaceVisibility(int c)
{
    bool detail = area_world_info[c].w > 0.5;
    vec4 light = area_world_light[c];
    vec3 dv = area_p-light.xyz;
    vec3 d = vec3(dot(area_world_rot[0],dv),dot(area_world_rot[1],dv),dot(area_world_rot[2],dv));
    float dist = length(d);
    float far = light.w;
    if (!(dist > 1.e-4) || dist > far) return 1.0;
    vec3 n_a = vec3(dot(area_world_rot[0],area_n_geo),dot(area_world_rot[1],area_n_geo),dot(area_world_rot[2],area_n_geo));
    float nl = dot(n_a,-d/dist);
    float near = area_world_info[c].y;
    float r_map = area_world_info[c].z;
    // Geometric offset, sized by the texel footprint (at most 5 cm), with a
    // separate primary-depth precision allowance. Detail omits the texel offset.
    float z0 = dot(d,AW_AXIS[awFace(d)]);
    float texel_w = 2.0*z0/AW_USABLE;
    vec2 depth_interval = awPrecision(nl,AW_MAX_RADIUS*texel_w*max(1.0,r_map/0.1));
    vec3 receiver = d;
    d += n_a*awReceiverOffset(texel_w,detail);
    int face = awFace(d);
    float zr = dot(d,AW_AXIS[face]);
    if (!(zr > near)) return 1.0;
    texel_w = 2.0*zr/AW_USABLE;
    float tan_t = sqrt(max(1.0-nl*nl,0.0))/max(nl,1.e-3);
    float bias = area_world_info[c].w > 0.5
        ? clamp(0.00015+0.005*texel_w*min(tan_t,AW_TAN_CAP),0.00015,0.0005)
        : AW_BIAS_CONST+(1.0-aw_plane_valid)*AW_BIAS_SLOPE*texel_w*min(tan_t,AW_TAN_CAP);
    bias += depth_interval.x;
    vec2 tile = vec2(AW_TILE*float(face%3),AW_TILE*(area_world_info[c].x+float(face/3)));
    vec2 centre = vec2(AW_HALF_TILE+AW_HALF_USABLE*dot(d,AW_RIGHT[face])/zr,
                       AW_HALF_TILE+AW_HALF_USABLE*dot(d,AW_UP[face])/zr);
    vec2 lo = tile, hi = tile+vec2(AW_TILE-2.0);   // base texel range that keeps the 2x2 footprint inside the tile
    vec2 inv_size = 1.0/vec2(textureSize(area_world_atlas,0));
    // Project once per light, instead of carrying three basis vectors and
    // rebuilding the same normal projection in every search/filter tap.
    vec3 plane_n = vec3(dot(n_a,AW_AXIS[face]),dot(n_a,AW_RIGHT[face]),dot(n_a,AW_UP[face]));
    vec3 forward = vec3(dot(receiver,AW_AXIS[face]),dot(receiver,AW_RIGHT[face])/AW_HALF_USABLE,dot(receiver,AW_UP[face])/AW_HALF_USABLE);
    float receiver2 = max(0.0,dot(receiver,receiver)-length(receiver)*depth_interval.y);
    float nd = dot(n_a,d);
    // Continuous phase in world metres, also used by the direction path.
    float rot = float(area_world_seed[c] >> 16)*(6.28318530718/65536.0)+dot(d,vec3(0.13,0.21,0.37));
    // Blocker search: where does the source disk see blockers? Search plane z_s, radius R_s texels.
    float z_s = max(near,r_map+0.05);
    float search = clamp(r_map*max(zr-z_s,0.0)/(2.0*z_s*zr)*AW_USABLE,1.0,AW_MAX_RADIUS);
    float blocker_sum = 0.0;
    float blocker_n = 0.0;
    // A valid plane with nd >= 0 has no positive comparison depth. Every
    // search tap would contribute exactly zero; confirm the original centre
    // below, then retain the full foreground filter footprint.
    if (!(aw_plane_valid >= 1.0 && nd >= 0.0))
    for (int k = 0; k < AW_SEARCH_TAPS; ++k)
    {
        vec2 off = search*awVogel(k,AW_SEARCH_TAPS,rot,true);
        if (detail)
        {
            vec2 hit = awDetailFaceTap(c,face,centre+off,plane_n,nd,bias,forward,receiver2,true);
            blocker_sum += hit.x; blocker_n += hit.y;
            continue;
        }
        vec2 p = tile+centre+off;
        vec2 base = floor(clamp(p-0.5,lo,hi));
        vec4 z = awGather(base+1.0,inv_size,near,far);
        if (all(greaterThanEqual(z,vec4(far)))) continue;
        float zp = awLocalPlane(plane_n,nd,(centre+off-AW_HALF_TILE)/AW_HALF_USABLE,zr,AW_TAN_CAP*texel_w*(length(off)+1.0));
        vec4 front = awReceiverFront(z,AW_BIAS_CONST,forward,receiver2,base-tile);
        vec2 slope = (centre+off-AW_HALF_TILE)/AW_HALF_USABLE;
        float plane_weight = awPlaneWeight(vec4(plane_n.x+dot(plane_n.yz,slope))).x;
        vec4 blocked = awBlocked(z,vec4(zp),zr,bias,vec4(plane_weight))*front;
        blocker_sum += dot(z,blocked);
        blocker_n += blocked.x+blocked.y+blocked.z+blocked.w;
    }
    if (blocker_n < 0.5)
    {
        vec2 fallback = awFallbackBlocker(c,receiver,bias,1.0,receiver2);
        blocker_sum = fallback.x; blocker_n = fallback.y;
    }
    if (blocker_n < 0.5) return 1.0;
    float z_b = max(blocker_sum/blocker_n,near);
    // Penumbra radius on the receiver plane -> texels of this face at the receiver's axial depth.
    float radius = clamp(r_map*max(zr-z_b,0.0)/z_b*AW_USABLE/(2.0*zr),AW_MIN_RADIUS,AW_MAX_RADIUS);
    float lit = 0.0;
    float front_lit = 0.0;
    for (int k = 0; k < AW_FILTER_TAPS; ++k)
    {
        vec2 off = radius*awVogel(k,AW_FILTER_TAPS,rot,false);
        if (detail)
        {
            vec2 tap = awDetailFaceTap(c,face,centre+off,plane_n,nd,bias,forward,receiver2,false);
            lit += tap.x; front_lit = max(front_lit,tap.y);
            continue;
        }
        float zp = awLocalPlane(plane_n,nd,(centre+off-AW_HALF_TILE)/AW_HALF_USABLE,zr,AW_TAN_CAP*texel_w*(length(off)+1.0));
        vec2 slope = (centre+off-AW_HALF_TILE)/AW_HALF_USABLE;
        float plane_weight = awPlaneWeight(vec4(plane_n.x+dot(plane_n.yz,slope))).x;
        vec2 tap = awTap(tile+centre+off,lo,hi,inv_size,zp,bias,near,far,forward,receiver2,plane_weight);
        lit += tap.x; front_lit = max(front_lit,tap.y);
    }
    // Confirmation uses the ACTUAL filter footprint, including every blended
    // face. A search disk may be smaller than a broad source's filter disk.
    // Full foreground coverage is a separate confirmation, not a second
    // shadow filter: fractional bilinear self depth must not darken a valid
    // convex plane. Fade confirmation over one tap; zero remains exact.
    float visibility = min(lit/float(AW_FILTER_TAPS),smoothstep(0.0,1.0/float(AW_FILTER_TAPS),front_lit));
    return visibility;
}
// One face's share of a direction-space tap. The tap ray was already chosen by the caller (a single, consistent world
// direction per tap); this reads it from `face`'s own atlas tile, whose 16 texel gutter holds every ray within 5.3 % of
// the face boundary. Compare each gathered texel against the receiver plane at THAT texel's ray, in this face's own axial
// units. The base texel is clamped to the tile exactly like awTap, so a tap never reads another face's tile (or the
// other light's row), and the plane is guarded and capped exactly like awPlane.
vec2 awFaceTap(int c, int face, vec3 ray, vec3 normal, float nd, float bias, vec3 direction, float distance, vec3 receiver, float receiver2, float blend, bool search)
{
    vec3 ax = AW_AXIS[face], rt = AW_RIGHT[face], up = AW_UP[face];
    vec3 ray_face = vec3(dot(ray,ax),dot(ray,rt),dot(ray,up));
    vec3 normal_face = vec3(dot(normal,ax),dot(normal,rt),dot(normal,up));
    vec3 direction_face = vec3(dot(direction,ax),dot(direction,rt),dot(direction,up));
    float cosine = ray_face.x;
    vec2 ray_slope = ray_face.yz/cosine;
    vec2 centre = AW_HALF_TILE+AW_HALF_USABLE*ray_slope;
    vec2 q = clamp(centre-0.5,vec2(0.0),vec2(AW_TILE-2.0));
    vec2 base = floor(q), fr = q-base;
    vec2 tile = vec2(AW_TILE*float(face%3),AW_TILE*(area_world_info[c].x+float(face/3)));
    vec4 z = awGather(tile+base+1.0,1.0/vec2(textureSize(area_world_atlas,0)),area_world_info[c].y,area_world_light[c].w);
    if (all(greaterThanEqual(z,vec4(area_world_light[c].w)))) return search ? vec2(0.0) : vec2(1.0);
    vec2 slope = (base+0.5-AW_HALF_TILE)/AW_HALF_USABLE;
    float den = normal_face.x+dot(normal_face.yz,slope);
    float dr = normal_face.y/AW_HALF_USABLE, du = normal_face.z/AW_HALF_USABLE;
    vec4 dens = vec4(den+du,den+dr+du,den+dr,den);
    // The receiver in THIS face's axial units (the same units as the gathered depths), and the plane-slope cap per texel of
    // tap distance from the receiver's own ray.
    float centre_cosine = max(direction_face.x,0.1);
    float zr = distance*centre_cosine;
    vec2 receiver_slope = direction_face.yz/centre_cosine;
    float cap = AW_TAN_CAP*(2.0*zr/AW_USABLE)*(length(ray_slope-receiver_slope)*AW_HALF_USABLE+1.0);
    bool detail = area_world_info[c].w > 0.5;
    float depth_scale = dot(ray_face,direction_face)/cosine;
    float old_plane = 0.0;
    if (blend < 1.0)
    {
        // Finish the old footprint before the proof, so its face vectors do
        // not stay live across either compare path. Planes and depth units
        // still morph together, once per tap, in this face's own axes.
        if (!detail) old_plane = awLocalPlane(normal_face,nd,ray_slope,zr,cap);
        depth_scale = mix(1.0/centre_cosine,depth_scale,blend);
    }
    float tap_bias = bias*mix(1.0,cosine,blend);
    vec3 forward = vec3(dot(receiver,ax),dot(receiver,rt)/AW_HALF_USABLE,dot(receiver,up)/AW_HALF_USABLE);
    float front_bias = (detail ? 0.00015 : AW_BIAS_CONST)*mix(1.0,cosine,blend);
    vec4 front = awReceiverFront(z,front_bias,forward,receiver2,base);
    // The most negative denominator gives the nearest positive plane depth
    // when nd < 0. Both the plain compare and every clamped plane compare
    // therefore block below min(zr, that depth): the upper clamp is >= zr.
    // Prove this before four divisions/clamps/horizon blends, retaining every
    // gather and the authoritative forward bound. The small inward margin
    // makes rounding conservative; an inconclusive proof uses the full path.
    // The face-morph band also keeps the full path for its old-plane blend.
    if (blend >= 1.0 && nd < 0.0)
    {
        float lower_plane = min(zr,nd/min(min(min(dens.x,dens.y),min(dens.z,dens.w)),-1.e-3))*0.999999;
        float largest_z = max(max(z.x,z.y),max(z.z,z.w));
        if (largest_z < lower_plane-tap_bias && all(greaterThanEqual(front,vec4(1.0))))
            return search ? vec2(dot(z,vec4(1.0))*depth_scale,4.0) : vec2(0.0);
    }
    // The denominator is continuous through the tangent horizon. The bounded
    // forward comparison below rejects every extrapolated background caster.
    vec4 plane;
    // A nonnegative numerator over the negative guarded denominator cannot
    // block a positive atlas depth; the upper cap cannot change that sign.
    // Preserve that compare without divisions;
    // the face morph keeps the full path to preserve its rounding too.
    if (blend >= 1.0 && nd >= 0.0)
        plane = vec4(0.0);
    else
    {
        plane = min(vec4(nd)/min(dens,vec4(-1.e-3)),vec4(zr+cap));
        if (detail) plane = vec4(nd)/min(dens,vec4(-1.e-3));
        if (blend < 1.0 && !detail) plane = mix(vec4(old_plane),plane,blend);
    }
    vec4 blocked = awBlocked(z,plane,zr,tap_bias,awPlaneWeight(dens))*front;
    if (search)
        return vec2(dot(z,blocked)*depth_scale,dot(blocked,vec4(1.0)));
    return vec2(1.0-mix(mix(blocked.w,blocked.z,fr.x),mix(blocked.x,blocked.y,fr.x),fr.y),
                1.0-mix(mix(front.w,front.z,fr.x),mix(front.x,front.y,fr.x),fr.y));
}
// One footprint in direction space. A face reads the ray whenever the ray's axis component is within 5 % of the dominant
// one (the 16 texel gutter makes that valid), and the faces that do are blended with a smoothstep weight that is 1 at the
// dominant axis: a partition of unity over the cube, so the blocker moments and the visibility are continuous across all
// twelve edges and eight corners whatever the footprint's size. (A hard choice of the single dominant face changed which
// texel lattice a tap read at the boundary; the 2 m softbox's blocker mean and radius, which hang on a handful of binary
// flags, jumped by up to 25/255 across a three-face corner.) At most two faces per edge tap, three per corner tap.
vec2 awDirectionTap(int c, vec3 ray, vec3 normal, float nd, float bias, vec3 direction, float distance, vec3 receiver, float receiver2, float blend, bool search, vec3 tangent)
{
    ray -= (AW_PLANE_COMPRESS*blend)*max(dot(tangent,ray),0.0)*tangent;
    vec3 a = abs(ray);
    float major = max(a.x,max(a.y,a.z));
    vec3 w = smoothstep(vec3(AW_OVERLAP*major),vec3(major),a);
    float inv_total = 1.0/(w.x+w.y+w.z);
    // Three explicit calls: a loop over the axes with dynamically indexed vectors costs ~70 % more at 4K than this (register pressure).
    vec2 sum = vec2(0.0);
    if (w.x > 0.0) sum += (w.x*inv_total)*awFaceTap(c,ray.x < 0.0 ? 1 : 0,ray,normal,nd,bias,direction,distance,receiver,receiver2,blend,search);
    if (w.y > 0.0) sum += (w.y*inv_total)*awFaceTap(c,ray.y < 0.0 ? 3 : 2,ray,normal,nd,bias,direction,distance,receiver,receiver2,blend,search);
    if (w.z > 0.0) sum += (w.z*inv_total)*awFaceTap(c,ray.z < 0.0 ? 5 : 4,ray,normal,nd,bias,direction,distance,receiver,receiver2,blend,search);
    return sum;
}
float awDirectionVisibility(int c, float blend)
{
    vec4 light = area_world_light[c];
    vec3 dv = area_p-light.xyz;
    vec3 d = vec3(dot(area_world_rot[0],dv),dot(area_world_rot[1],dv),dot(area_world_rot[2],dv));
    float dist = length(d);
    float far = light.w;
    if (!(dist > 1.e-4) || dist > far) return 1.0;
    vec3 n_a = vec3(dot(area_world_rot[0],area_n_geo),dot(area_world_rot[1],area_n_geo),dot(area_world_rot[2],area_n_geo));
    float nl = dot(n_a,-d/dist);
    float near = area_world_info[c].y;
    float r_map = area_world_info[c].z;
    // Same stable offset / detail contact rule as the face path.
    float z0 = max(abs(d.x),max(abs(d.y),abs(d.z)));   // the dominant component: continuous across every edge and corner
    float texel_w = 2.0*z0/AW_USABLE;
    vec2 depth_interval = awPrecision(nl,AW_MAX_RADIUS*texel_w*max(1.0,r_map/0.1));
    vec3 receiver = d;
    d += n_a*awReceiverOffset(texel_w,area_world_info[c].w > 0.5);
    int face = awFace(d);
    float zr = dot(d,AW_AXIS[face]);
    if (!(zr > near)) return 1.0;
    texel_w = 2.0*zr/AW_USABLE;
    float tan_t = sqrt(max(1.0-nl*nl,0.0))/max(nl,1.e-3);
    float bias = area_world_info[c].w > 0.5
        ? clamp(0.00015+0.005*texel_w*min(tan_t,AW_TAN_CAP),0.00015,0.0005)
        : AW_BIAS_CONST+(1.0-aw_plane_valid)*AW_BIAS_SLOPE*texel_w*min(tan_t,AW_TAN_CAP);
    bias += depth_interval.x;
    dist = length(d);
    float receiver2 = max(0.0,dot(receiver,receiver)-length(receiver)*depth_interval.y);
    vec3 direction = d/dist;
    vec3 tangent = awPlaneTangent(n_a,direction);
    // Frisvad's continuous orthonormal basis (only singular at the south pole,
    // away from every face boundary). Phase varies smoothly in WORLD metres;
    // no face/cell hash can jump between two micrometre-separated receivers.
    float a = 1.0/max(1.0+direction.z,1.e-6);
    vec3 rt = direction.z < -0.99999 ? vec3(0,-1,0) : vec3(1.0-direction.x*direction.x*a,-direction.x*direction.y*a,-direction.x);
    vec3 up = cross(direction,rt);
    vec3 old_rt = AW_RIGHT[face], old_up = AW_UP[face];
    vec2 centre = AW_HALF_TILE+AW_HALF_USABLE*vec2(dot(d,old_rt),dot(d,old_up))/zr;
    float nd = dot(n_a,d);
    uint h = area_world_seed[c];
    float rot = float(h >> 16)*(6.28318530718/65536.0);
    rot += dot(d,vec3(0.13,0.21,0.37));
    // Blocker search: where does the source disk see blockers? Search plane z_s, radius R_s texels.
    float z_s = max(near,r_map+0.05);
    float search = max(r_map*max(dist-z_s,0.0)/(z_s*dist),2.0/AW_USABLE);
    // Small sources retain the existing local-cache footprint. Larger authored
    // apertures grow the bound in proportion to their radius (up to 2 m).
    bool large = r_map > 0.25;
    float max_angular = (2.0*AW_MAX_RADIUS/AW_USABLE)*(large ? r_map/0.1 : 1.0);
    search = min(search,max_angular);
    float old_search = clamp(r_map*max(zr-z_s,0.0)/(2.0*z_s*zr)*AW_USABLE,1.0,AW_MAX_RADIUS)*(2.0*zr/(AW_USABLE*dist));
    int search_taps = large ? AW_SEARCH_TAPS/2 : AW_SEARCH_TAPS;
    float blocker_sum = 0.0;
    float blocker_n = 0.0;
    // Identical zero-search proof to the face path, before any atlas fetch.
    if (!(aw_plane_valid >= 1.0 && nd >= 0.0))
    for (int k = 0; k < search_taps; ++k)
    {
        vec2 off = search*awVogel(k,search_taps,rot,true);
        vec2 old_off = old_search*awVogel(k,search_taps,rot,true);
        vec3 tap = mix(old_off.x*old_rt+old_off.y*old_up,off.x*rt+off.y*up,blend);
        vec2 hit = awDirectionTap(c,normalize(direction+tap),n_a,nd,bias,direction,dist,receiver,receiver2,blend,true,tangent);
        blocker_sum += hit.x;
        blocker_n += hit.y;
    }
    if (blocker_n < 0.5)
    {
        vec2 fallback = awFallbackBlocker(c,receiver,bias,dist/zr,receiver2);
        blocker_sum = fallback.x; blocker_n = fallback.y;
    }
    if (blocker_n < 0.5) return 1.0;
    float z_b = max(blocker_sum/blocker_n,near);
    // Penumbra radius on the receiver plane -> texels of this face at the receiver's axial depth.
    float radius = max(r_map*max(dist-z_b,0.0)/(z_b*dist),2.0*AW_MIN_RADIUS/AW_USABLE);
    radius = min(radius,max_angular);
    float old_radius = clamp(r_map*max(zr-z_b*zr/dist,0.0)/(z_b*zr/dist)*AW_USABLE/(2.0*zr),AW_MIN_RADIUS,AW_MAX_RADIUS)*(2.0*zr/(AW_USABLE*dist));
    int filter_taps = large ? AW_FILTER_TAPS/2 : AW_FILTER_TAPS;
    float lit = 0.0;
    float front_lit = 0.0;
    for (int k = 0; k < filter_taps; ++k)
    {
        vec2 off = radius*awVogel(k,filter_taps,rot,false);
        vec2 old_off = old_radius*awVogel(k,filter_taps,rot,false);
        vec3 tap = mix(old_off.x*old_rt+old_off.y*old_up,off.x*rt+off.y*up,blend);
        vec2 hit = awDirectionTap(c,normalize(direction+tap),n_a,nd,bias,direction,dist,receiver,receiver2,blend,false,tangent);
        lit += hit.x; front_lit = max(front_lit,hit.y);
    }
    float visibility = min(lit/float(filter_taps),smoothstep(0.0,1.0/float(filter_taps),front_lit));
    return visibility;
}
// Interior small-source footprints use the original cache-local projection.
// In the last two receiver texels, morph the SAME taps into direction space;
// taps cross faces using the real 16-texel raster gutters. No second footprint
// evaluation: 16+32 gathers (8+16 for softboxes). An invalid depth stencil may
// additionally confirm the centre once if the conservative search found none.
float awVisibility(int c)
{
    vec3 dv = area_p-area_world_light[c].xyz;
    vec3 d = vec3(dot(area_world_rot[0],dv),dot(area_world_rot[1],dv),dot(area_world_rot[2],dv));
    vec3 normal = vec3(dot(area_world_rot[0],area_n_geo),dot(area_world_rot[1],area_n_geo),dot(area_world_rot[2],area_n_geo));
    float distance = length(d);
    if (!(distance > 1.e-4) || distance > area_world_light[c].w) return 1.0;
    float backface = awBackfaceWeight(dot(normal,-d)/max(distance,1.e-9),distance,area_world_info[c].z);
    if (backface <= 0.0) return 1.0;
    float texel_w = 2.0*max(abs(d.x),max(abs(d.y),abs(d.z)))/AW_USABLE;
    d += normal*awReceiverOffset(texel_w,area_world_info[c].w > 0.5);
    vec3 a = abs(d);
    float major = max(a.x,max(a.y,a.z));
    float second = min(max(a.x,a.y),min(max(a.y,a.z),max(a.x,a.z)));
    float edge = second/max(major,1.e-6);
    float visibility;
    if (area_world_info[c].z > 0.25) visibility = awDirectionVisibility(c,1.0);
    else if (edge <= 0.9921875) visibility = awFaceVisibility(c);
    else visibility = awDirectionVisibility(c,smoothstep(0.9921875,1.0,edge));
    return mix(1.0,visibility,backface);
}
// Diagnostic values for the channel's visibility (RenderAreaShadowWorldDebug, runtime only): 1 the world term alone, 2 the face index as grey (face / 5),
// 3 receiver axial depth minus the atlas texel's axial depth at the receiver's own texel: 0.5 = the receiver is the recorded surface, brighter = a blocker
// in front (scale 5 per metre), black = nothing recorded there.
float awDebugValue(int c, int mode)
{
    vec4 light = area_world_light[c];
    vec3 dv = area_p-light.xyz;
    vec3 d = vec3(dot(area_world_rot[0],dv),dot(area_world_rot[1],dv),dot(area_world_rot[2],dv));
    int face = awFace(d);
    if (mode == 2) return float(face)/5.0;
    float zr = dot(d,AW_AXIS[face]);
    float near = area_world_info[c].y;
    if (!(zr > near)) return 0.0;
    vec2 tile = vec2(AW_TILE*float(face%3),AW_TILE*(area_world_info[c].x+float(face/3)));
    vec2 centre = vec2(AW_HALF_TILE+AW_HALF_USABLE*dot(d,AW_RIGHT[face])/zr,
                       AW_HALF_TILE+AW_HALF_USABLE*dot(d,AW_UP[face])/zr);
    // Clamped to this face's own tile: a grazing ray must not read a neighbouring tile.
    float stored = texelFetch(area_world_atlas,ivec2(clamp(floor(tile+centre),tile,tile+vec2(AW_TILE-1.0))),0).r;
    if (stored >= 1.0) return 0.0;
    float za = awLinear(vec4(stored),near,light.w).x;
    return clamp(0.5+5.0*(zr-za),0.0,1.0);
}
#endif
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
#if __VERSION__ >= 400
    // World-owned channels: the world map decides everything beyond the contact band, at full resolution and unblurred.
    // (A pixel whose window depth is exactly 0 sits on the near plane: its position is clamped, not a surface, so no map can judge it.)
    if (all(equal(area_axis,vec2(0))) && !area_sky && depth > 0.0 && any(greaterThan(area_world,vec4(0.5))))
    {
        aw_centre_depth = depth;
        area_p = awPosition(texel,depth);
        area_n_geo = awGeoNormal(texel);
        for (int c = 0; c < 4; ++c)
            if (area_world[c] > 0.5)
            {
                // Share the full resolve between raw visibility diagnostics
                // and the world-amount control. Duplicated call sites also
                // duplicate the inlined tap loops and raise register pressure.
                float visibility = area_world_debug > 1 ? awDebugValue(c,area_world_debug) : awVisibility(c);
                if (area_world_debug > 0) frag_color[c] = visibility;
                else frag_color[c] = min(frag_color[c],area_look.z != 0.0 ? mix(1.0,visibility,1.0-area_look.z) : visibility);
            }
    }
#endif
    // [Soft shadow look] Applied ONCE to the final per-channel visibility (after the blur, the upsample and the world resolve, before the lighting
    // pass multiplies it into the light): Contrast first, v = pow(v, contrast); then Strength, v = mix(1, v, strength). Each is skipped at exactly
    // its identity (area_look is all zero = today's bytes). The World diagnostic views are shown raw.
    if (all(equal(area_axis,vec2(0))) && area_world_debug == 0)
    {
        if (area_look.y != 0.0) frag_color = pow(max(frag_color,vec4(0.0)),vec4(1.0+area_look.y));
        if (area_look.x != 0.0) frag_color = mix(vec4(1.0),frag_color,1.0-area_look.x);
    }
}
