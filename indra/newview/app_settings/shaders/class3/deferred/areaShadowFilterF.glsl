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
const float AW_OVERLAP = 0.95;     // a face also reads a ray whose axis component is within 5 % of the dominant one (gutter: 5.3 %, less the gather footprint)
const float AW_MAX_RADIUS = 12.0;  // texels; 12 + 2 <= the 16 texel gutter
const int AW_SEARCH_TAPS = 16, AW_FILTER_TAPS = 32;
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
    return awLinear(textureGather(area_world_atlas,corner*inv_size),n,f);
}
// Lit fraction of one bilinear compare tap (atlas texel coordinates p, texel centres at +0.5): a texel is lit when its
// blocker depth is not nearer than the receiver minus the bias, in metres.
float awTap(vec2 p, vec2 lo, vec2 hi, vec2 inv_size, float zr, float bias, float n, float f)
{
    vec2 q = clamp(p-0.5,lo,hi);
    vec2 base = floor(q);
    vec2 fr = q-base;
    vec4 z = awGather(base+1.0,inv_size,n,f);   // x=(0,1) y=(1,1) z=(1,0) w=(0,0)
    vec4 lit = vec4(greaterThanEqual(z,vec4(zr-bias)));
    return mix(mix(lit.w,lit.z,fr.x),mix(lit.x,lit.y,fr.x),fr.y);
}
// Receiver-plane depth (Isidoro 2006): the axial depth at which the ray of face slope `slope` meets the receiver's tangent
// plane (normal n, plane through the offset receiver: nd = n . (P' - C)). Comparing each tap against ITS OWN plane depth
// instead of the centre's keeps a floor seen at a grazing angle from shadowing itself across the filter footprint (a
// constant bias would need to grow with the tap distance). Capped at the slope cap per texel of tap distance, and the
// centre depth where the ray is parallel to the plane.
float awPlane(vec3 n, vec3 ax, vec3 rt, vec3 up, float nd, vec2 slope, float zr, float cap)
{
    float den = dot(n,ax+slope.x*rt+slope.y*up);
    if (!(den < -1.e-3)) return zr;
    return clamp(nd/den,zr-cap,zr+cap);
}
float awFaceVisibility(int c)
{
    vec4 light = area_world_light[c];
    vec3 dv = area_p-light.xyz;
    vec3 d = vec3(dot(area_world_rot[0],dv),dot(area_world_rot[1],dv),dot(area_world_rot[2],dv));
    float dist = length(d);
    float far = light.w;
    if (!(dist > 1.e-4) || dist > far) return 1.0;
    vec3 n_a = vec3(dot(area_world_rot[0],area_n_geo),dot(area_world_rot[1],area_n_geo),dot(area_world_rot[2],area_n_geo));
    float nl = dot(n_a,-d/dist);
    if (nl <= 0.0) return 1.0;
    float near = area_world_info[c].y;
    float r_map = area_world_info[c].z;
    // Normal offset along the geometric normal, sized by the texel footprint at this depth (never beyond 5 cm).
    float z0 = dot(d,AW_AXIS[awFace(d)]);
    float texel_w = 2.0*z0/AW_USABLE;
    d += n_a*(area_world_info[c].w > 0.5
        ? clamp(0.10*texel_w,0.00025,0.0025)
        : min(AW_NORMAL_TEXELS*texel_w,AW_NORMAL_CAP));
    int face = awFace(d);
    float zr = dot(d,AW_AXIS[face]);
    if (!(zr > near)) return 1.0;
    texel_w = 2.0*zr/AW_USABLE;
    float tan_t = sqrt(max(1.0-nl*nl,0.0))/max(nl,1.e-3);
    float bias = area_world_info[c].w > 0.5
        ? clamp(0.0005+0.10*texel_w*min(tan_t,AW_TAN_CAP),0.0005,0.003)
        : AW_BIAS_CONST+AW_BIAS_SLOPE*texel_w*min(tan_t,AW_TAN_CAP);
    vec2 tile = vec2(AW_TILE*float(face%3),AW_TILE*(area_world_info[c].x+float(face/3)));
    vec2 centre = vec2(AW_HALF_TILE+AW_HALF_USABLE*dot(d,AW_RIGHT[face])/zr,
                       AW_HALF_TILE+AW_HALF_USABLE*dot(d,AW_UP[face])/zr);
    vec2 lo = tile, hi = tile+vec2(AW_TILE-2.0);   // base texel range that keeps the 2x2 footprint inside the tile
    vec2 inv_size = 1.0/vec2(textureSize(area_world_atlas,0));
    vec3 ax = AW_AXIS[face], rt = AW_RIGHT[face], up = AW_UP[face];
    float nd = dot(n_a,d);
    // The rotation belongs to the receiver's shadow texel: camera-invariant, no time input (R2 rank + light shift).
    uvec2 cell = uvec2(ivec2(floor(centre)));
    uint h = cell.x*AW_R2_A+cell.y*AW_R2_B+uint(face)*0x9e3779b9u+area_world_seed[c];
    float rot = float(h >> 16)*(6.28318530718/65536.0);
    // Blocker search: where does the source disk see blockers? Search plane z_s, radius R_s texels.
    float z_s = max(near,r_map+0.05);
    float search = clamp(r_map*max(zr-z_s,0.0)/(2.0*z_s*zr)*AW_USABLE,1.0,AW_MAX_RADIUS);
    float blocker_sum = 0.0;
    float blocker_n = 0.0;
    for (int k = 0; k < AW_SEARCH_TAPS; ++k)
    {
        vec2 off = search*awVogel(k,AW_SEARCH_TAPS,rot,true);
        vec2 p = tile+centre+off;
        vec2 base = floor(clamp(p-0.5,lo,hi));
        vec4 z = awGather(base+1.0,inv_size,near,far);
        float zp = awPlane(n_a,ax,rt,up,nd,(centre+off-AW_HALF_TILE)/AW_HALF_USABLE,zr,AW_TAN_CAP*texel_w*(length(off)+1.0));
        vec4 blocked = vec4(lessThan(z,vec4(zp-bias)));
        blocker_sum += dot(z,blocked);
        blocker_n += blocked.x+blocked.y+blocked.z+blocked.w;
    }
    if (blocker_n < 0.5) return 1.0;
    float z_b = max(blocker_sum/blocker_n,near);
    // Penumbra radius on the receiver plane -> texels of this face at the receiver's axial depth.
    float radius = clamp(r_map*max(zr-z_b,0.0)/z_b*AW_USABLE/(2.0*zr),1.0,AW_MAX_RADIUS);
    float lit = 0.0;
    for (int k = 0; k < AW_FILTER_TAPS; ++k)
    {
        vec2 off = radius*awVogel(k,AW_FILTER_TAPS,rot,false);
        float zp = awPlane(n_a,ax,rt,up,nd,(centre+off-AW_HALF_TILE)/AW_HALF_USABLE,zr,AW_TAN_CAP*texel_w*(length(off)+1.0));
        lit += awTap(tile+centre+off,lo,hi,inv_size,zp,bias,near,far);
    }
    return lit/float(AW_FILTER_TAPS);
}
// One face's share of a direction-space tap. The tap ray was already chosen by the caller (a single, consistent world
// direction per tap); this reads it from `face`'s own atlas tile, whose 16 texel gutter holds every ray within 5.3 % of
// the face boundary. Compare each gathered texel against the receiver plane at THAT texel's ray, in this face's own axial
// units. The base texel is clamped to the tile exactly like awTap, so a tap never reads another face's tile (or the
// other light's row), and the plane is guarded and capped exactly like awPlane.
vec2 awFaceTap(int c, int face, vec3 ray, vec3 normal, float nd, float bias, vec3 direction, float distance, float blend, bool search)
{
    vec3 ax = AW_AXIS[face], rt = AW_RIGHT[face], up = AW_UP[face];
    float cosine = dot(ray,ax);
    vec2 ray_slope = vec2(dot(ray,rt),dot(ray,up))/cosine;
    vec2 centre = AW_HALF_TILE+AW_HALF_USABLE*ray_slope;
    vec2 q = clamp(centre-0.5,vec2(0.0),vec2(AW_TILE-2.0));
    vec2 base = floor(q), fr = q-base;
    vec2 tile = vec2(AW_TILE*float(face%3),AW_TILE*(area_world_info[c].x+float(face/3)));
    vec4 z = awGather(tile+base+1.0,1.0/vec2(textureSize(area_world_atlas,0)),area_world_info[c].y,area_world_light[c].w);
    vec2 slope = (base+0.5-AW_HALF_TILE)/AW_HALF_USABLE;
    vec3 v = ax+slope.x*rt+slope.y*up;
    float den = dot(normal,v), dr = dot(normal,rt)/AW_HALF_USABLE, du = dot(normal,up)/AW_HALF_USABLE;
    vec4 dens = vec4(den+du,den+dr+du,den+dr,den);
    // The receiver in THIS face's axial units (the same units as the gathered depths), and the plane-slope cap per texel of
    // tap distance from the receiver's own ray.
    float centre_cosine = max(dot(direction,ax),0.1);
    float zr = distance*centre_cosine;
    vec2 receiver_slope = vec2(dot(direction,rt),dot(direction,up))/centre_cosine;
    float cap = AW_TAN_CAP*(2.0*zr/AW_USABLE)*(length(ray_slope-receiver_slope)*AW_HALF_USABLE+1.0);
    // A ray parallel to / behind the receiver plane cannot reveal a blocker on it: awPlane's guard (the centre depth) and
    // clamp (zr +- cap), per corner texel. Without them nd/dens explodes at grazing angles and every texel reads blocked.
    vec4 plane = mix(vec4(zr),clamp(vec4(nd)/min(dens,vec4(-1.e-3)),vec4(zr-cap),vec4(zr+cap)),vec4(lessThan(dens,vec4(-1.e-3))));
    float depth_scale = dot(ray,direction)/cosine;
    if (blend < 1.0)
    {
        // At the band's inner edge this is exactly the old face footprint; positions, receiver plane and depth units morph
        // together, once per tap. Both planes are this FACE's own: the receiver is expressed in the tap face's axes.
        float old_plane = awPlane(normal,ax,rt,up,nd,ray_slope,zr,cap);
        plane = mix(vec4(old_plane),plane,blend);
        depth_scale = mix(1.0/centre_cosine,depth_scale,blend);
    }
    vec4 blocked = vec4(lessThan(z,plane-bias*mix(1.0,cosine,blend)))*mix(vec4(1.0),vec4(lessThan(dens,vec4(-1.e-5))),blend);
    if (search)
        return vec2(dot(z,blocked)*depth_scale,dot(blocked,vec4(1.0)));
    return vec2(1.0-mix(mix(blocked.w,blocked.z,fr.x),mix(blocked.x,blocked.y,fr.x),fr.y),0.0);
}
// One footprint in direction space. A face reads the ray whenever the ray's axis component is within 5 % of the dominant
// one (the 16 texel gutter makes that valid), and the faces that do are blended with a smoothstep weight that is 1 at the
// dominant axis: a partition of unity over the cube, so the blocker moments and the visibility are continuous across all
// twelve edges and eight corners whatever the footprint's size. (A hard choice of the single dominant face changed which
// texel lattice a tap read at the boundary; the 2 m softbox's blocker mean and radius, which hang on a handful of binary
// flags, jumped by up to 25/255 across a three-face corner.) At most two faces per edge tap, three per corner tap.
vec2 awDirectionTap(int c, vec3 ray, vec3 normal, float nd, float bias, vec3 direction, float distance, float blend, bool search)
{
    vec3 a = abs(ray);
    float major = max(a.x,max(a.y,a.z));
    vec3 w = smoothstep(vec3(AW_OVERLAP*major),vec3(major),a);
    float inv_total = 1.0/(w.x+w.y+w.z);
    // Three explicit calls: a loop over the axes with dynamically indexed vectors costs ~70 % more at 4K than this (register pressure).
    vec2 sum = vec2(0.0);
    if (w.x > 0.0) sum += (w.x*inv_total)*awFaceTap(c,ray.x < 0.0 ? 1 : 0,ray,normal,nd,bias,direction,distance,blend,search);
    if (w.y > 0.0) sum += (w.y*inv_total)*awFaceTap(c,ray.y < 0.0 ? 3 : 2,ray,normal,nd,bias,direction,distance,blend,search);
    if (w.z > 0.0) sum += (w.z*inv_total)*awFaceTap(c,ray.z < 0.0 ? 5 : 4,ray,normal,nd,bias,direction,distance,blend,search);
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
    if (nl <= 0.0) return 1.0;
    float near = area_world_info[c].y;
    float r_map = area_world_info[c].z;
    // Normal offset along the geometric normal, sized by the texel footprint at this depth (never beyond 5 cm).
    float z0 = max(abs(d.x),max(abs(d.y),abs(d.z)));   // the dominant component: continuous across every edge and corner
    float texel_w = 2.0*z0/AW_USABLE;
    d += n_a*(area_world_info[c].w > 0.5
        ? clamp(0.10*texel_w,0.00025,0.0025)
        : min(AW_NORMAL_TEXELS*texel_w,AW_NORMAL_CAP));
    int face = awFace(d);
    float zr = dot(d,AW_AXIS[face]);
    if (!(zr > near)) return 1.0;
    texel_w = 2.0*zr/AW_USABLE;
    float tan_t = sqrt(max(1.0-nl*nl,0.0))/max(nl,1.e-3);
    float bias = area_world_info[c].w > 0.5
        ? clamp(0.0005+0.10*texel_w*min(tan_t,AW_TAN_CAP),0.0005,0.003)
        : AW_BIAS_CONST+AW_BIAS_SLOPE*texel_w*min(tan_t,AW_TAN_CAP);
    dist = length(d);
    vec3 direction = d/dist;
    // Frisvad's continuous orthonormal basis (only singular at the south pole,
    // away from every face boundary). Phase varies smoothly in WORLD metres;
    // no face/cell hash can jump between two micrometre-separated receivers.
    float a = 1.0/max(1.0+direction.z,1.e-6);
    vec3 rt = direction.z < -0.99999 ? vec3(0,-1,0) : vec3(1.0-direction.x*direction.x*a,-direction.x*direction.y*a,-direction.x);
    vec3 up = cross(direction,rt);
    vec3 old_rt = AW_RIGHT[face], old_up = AW_UP[face];
    vec2 centre = AW_HALF_TILE+AW_HALF_USABLE*vec2(dot(d,old_rt),dot(d,old_up))/zr;
    uvec2 cell = uvec2(ivec2(floor(centre)));
    uint old_h = cell.x*AW_R2_A+cell.y*AW_R2_B+uint(face)*0x9e3779b9u+area_world_seed[c];
    float old_rot = float(old_h >> 16)*(6.28318530718/65536.0);
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
    for (int k = 0; k < search_taps; ++k)
    {
        vec2 off = search*awVogel(k,search_taps,rot,true);
        vec2 old_off = old_search*awVogel(k,search_taps,old_rot,true);
        vec3 tap = mix(old_off.x*old_rt+old_off.y*old_up,off.x*rt+off.y*up,blend);
        vec2 hit = awDirectionTap(c,normalize(direction+tap),n_a,nd,bias,direction,dist,blend,true);
        blocker_sum += hit.x;
        blocker_n += hit.y;
    }
    if (blocker_n < 0.5) return 1.0;
    float z_b = max(blocker_sum/blocker_n,near);
    // Penumbra radius on the receiver plane -> texels of this face at the receiver's axial depth.
    float radius = max(r_map*max(dist-z_b,0.0)/(z_b*dist),2.0/AW_USABLE);
    radius = min(radius,max_angular);
    float old_radius = clamp(r_map*max(zr-z_b*zr/dist,0.0)/(z_b*zr/dist)*AW_USABLE/(2.0*zr),1.0,AW_MAX_RADIUS)*(2.0*zr/(AW_USABLE*dist));
    int filter_taps = large ? AW_FILTER_TAPS/2 : AW_FILTER_TAPS;
    float lit = 0.0;
    for (int k = 0; k < filter_taps; ++k)
    {
        vec2 off = radius*awVogel(k,filter_taps,rot,false);
        vec2 old_off = old_radius*awVogel(k,filter_taps,old_rot,false);
        vec3 tap = mix(old_off.x*old_rt+old_off.y*old_up,off.x*rt+off.y*up,blend);
        lit += awDirectionTap(c,normalize(direction+tap),n_a,nd,bias,direction,dist,blend,false).x;
    }
    return lit/float(filter_taps);
}
// Interior small-source footprints use the original cache-local projection.
// In the last two receiver texels, morph the SAME taps into direction space;
// taps cross faces using the real 16-texel raster gutters. No second evaluation,
// no new tap-count boundary: at most 16+32 gathers anywhere (8+16 for softboxes).
float awVisibility(int c)
{
    vec3 dv = area_p-area_world_light[c].xyz;
    vec3 d = vec3(dot(area_world_rot[0],dv),dot(area_world_rot[1],dv),dot(area_world_rot[2],dv));
    vec3 normal = vec3(dot(area_world_rot[0],area_n_geo),dot(area_world_rot[1],area_n_geo),dot(area_world_rot[2],area_n_geo));
    float texel_w = 2.0*max(abs(d.x),max(abs(d.y),abs(d.z)))/AW_USABLE;
    d += normal*(area_world_info[c].w > 0.5
        ? clamp(0.10*texel_w,0.00025,0.0025)
        : min(AW_NORMAL_TEXELS*2.0*max(abs(d.x),max(abs(d.y),abs(d.z)))/AW_USABLE,AW_NORMAL_CAP));
    vec3 a = abs(d);
    float major = max(a.x,max(a.y,a.z));
    float second = min(max(a.x,a.y),min(max(a.y,a.z),max(a.x,a.z)));
    float edge = second/max(major,1.e-6);
    if (area_world_info[c].z > 0.25) return awDirectionVisibility(c,1.0);
    if (edge <= 0.9921875) return awFaceVisibility(c);
    return awDirectionVisibility(c,smoothstep(0.9921875,1.0,edge));
}
// Diagnostic values for the channel's visibility (RenderAreaShadowWorldDebug, runtime only): 1 the world term alone, 2 the face index as grey (face / 5),
// 3 receiver axial depth minus the atlas texel's axial depth at the receiver's own texel: 0.5 = the receiver is the recorded surface, brighter = a blocker
// in front (scale 5 per metre), black = nothing recorded there.
float awDebugValue(int c, int mode)
{
    if (mode == 1) return awVisibility(c);
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
    if (all(equal(area_axis,vec2(0))) && !area_sky && depth > 0.0)
        for (int c = 0; c < 4; ++c)
            if (area_world[c] > 0.5)
                frag_color[c] = area_world_debug > 0 ? awDebugValue(c,area_world_debug) : min(frag_color[c],area_look.z != 0.0 ? mix(1.0,awVisibility(c),1.0-area_look.z) : awVisibility(c));
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
