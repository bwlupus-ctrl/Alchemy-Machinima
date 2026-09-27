/**
 * @file tronWorldF.glsl
 *
 * [TronT1] Tron World: full-screen HDR post pass (dark grade + world-space
 * neon grid with travelling pulses). Composites IN PLACE exactly like
 * Rotoscope Ink (cineOutlineF.glsl): reads a copy of the destination
 * (tronScene), writes every pixel back with blending OFF into the mWaterDis
 * scratch, which the host then blits back into attachment 0 of the
 * destination. Uploaded by LLPipeline::renderTronWorld() (see
 * scratchpad/tron_t1_contract.md for the C++ side).
 *
 * Per pixel (design v2 section 4, v3/v4 deltas):
 *   reconstruct   p_v from the pass's OWN inverse projection (tron_inv_proj,
 *                 never deferredUtil's managed inv_proj, which is re-synced
 *                 from the live GL stack at draw time and is not the scene
 *                 camera in post); p_w = cam_rel + R * p_v with R the pass's
 *                 inverse-modelview rotation and cam_rel the camera offset
 *                 from the CPU's 1024 m world anchor -- no multi-thousand-
 *                 metre coordinate ever enters float32 math.
 *   derivatives   ALL dFdx/dFdy at the top of main() in uniform flow, taken
 *                 in view space and rotated (fw3 = per-axis metres per pixel,
 *                 n_geo = geometric normal, relative degeneracy test, G-buffer
 *                 normal fallback).
 *   water         detected BEFORE the sky classification (agent-space height
 *                 test + horizontal geometric normal; the water pool never
 *                 writes the G-buffer so a water pixel carries the seabed's
 *                 normal and, with sky behind it, the SKIP_ATMOS flag).
 *   sky           cleared depth (d >= 1.0) OR (SKIP_ATMOS flag AND not water).
 *   subject mask  union of screen shape x depth slab over up to 16 Roto
 *                 targets (boxes), combined with the per-pixel G-buffer
 *                 avatar tag per TronSubjectSourceGrid (0 boxes / 1 tag /
 *                 2 tag AND boxes / 3 tag OR boxes; tag is a literal 0 when
 *                 the preamble did not emit GBUFFER_AVATAR_TAG).
 *   grade         desaturate -> tint -> darken (EV) -> crush (exposed units),
 *                 weighted by strength x (1 - keep_bright) x (1 - keep_subject);
 *                 sky pixels use the sky rule (scale toward TronGradeSkyDarken)
 *                 with the same keep terms.
 *   grid          triplanar (floor / ceiling / two wall planes, Z-up) anti-
 *                 aliased minor + major lines on the GLOBAL lattice
 *                 (q = (p_w + r_s) / s, cell id = floor(q) + k_s, see
 *                 ALTron::computeLattice), moire guard, far fade, subject
 *                 exclusion, "grid only near subject" radius, water policy.
 *   pulses        per grid line (global line id hashed with PCG) a travelling
 *                 head+tail wave; along-line term (p + r_L) / L uses the
 *                 WAVELENGTH lattice's own remainder; the time phase is
 *                 computed on the CPU in F64 (tron_pulse.y) -- no absolute
 *                 time reaches the shader.
 *   compose       every emissive Tron term is authored in display-intent
 *                 linear, scaled once by the no-post factor (tron_master.w),
 *                 optionally clamped to <= 1 when colorCorrect will not
 *                 tonemap (tron_palette1.w), then divided ONCE by the
 *                 effective exposure E (Roto rule) and added to the graded
 *                 scene. alpha = scene.a + coverage x glow (legacy bloom feed).
 *
 * T2 (circuit traces, post neon rim) and T3 (light-cycle trails) are NOT in
 * this file yet; their reserved uniforms exist in llshadermgr.h so the enum
 * never reshuffles.
 *
 * Portability: #version 140 compatible (uint / uvec hashing only, no bitfield
 * builtins), derivatives only in uniform control flow, no pow() of a negative
 * base, no reversed smoothstep edges, no GLSL reserved words as identifiers.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;

in vec2 vary_fragcoord;

// [TronT1] copy of the destination target (LLPipeline::mWaterDis scratch draw
// reads dst as a plain texture; never dst while it is bound)
uniform sampler2D tronScene;
// [TronT1] exposure the tonemapper will apply later (see E in main(); same
// signed selector contract as Roto Ink: > 0 manual*auto, < 0 manual only
// (S-Log3 lock), 0 = paint semantics / E = 1 (Scene layer, no-post frames)).
uniform sampler2D exposureMap;   // 1x1 auto-exposure scale
uniform float     exposure;

uniform vec2 screen_res;

// Dedicated matrices of the pass's OWN camera (gGLLast* at the Camera layer,
// live gGLModelView/gGLProjection at the Scene layer). The managed inv_proj /
// inv_modelview must not be used in post (see file header).
uniform mat4 tron_inv_proj;
uniform mat4 tron_inv_modelview;

// xyz = camera position relative to the CPU world anchor (m, |.| < ~1800),
// w   = camera agent-space height (m) -- for the water height test only.
uniform vec4 tron_cam_rel;
// x = layer (0 Scene, 1 Camera; informational), y = shared pulse01 waveform
// (T2 neon rim; unused here), z reserved, w = no-post neon scale (1 normally,
// TronNoPostScale on frames colorCorrect will not expose/tonemap).
uniform vec4 tron_master;
// palette (linear RGB): primary / secondary / accent / pulse.
// palette0.w = master pulse amount (T2), palette1.w = 1 when colorCorrect will
// NOT tonemap this frame (neon is then clamped to <= 1), palette2/3.w reserved.
uniform vec4 tron_palette0;
uniform vec4 tron_palette1;
uniform vec4 tron_palette2;
uniform vec4 tron_palette3;
// x = strength 0..1, y = darken EV 0..8, z = desaturate 0..1, w = black crush 0..0.5 (exposed units)
uniform vec4 tron_grade;
// xyz = tint (linear, max-normalised), w = tint amount 0..1
uniform vec4 tron_grade2;
// x = keep-bright lo, y = keep-bright hi (exposed luminance), z = keep subject 0..1,
// w = sky darken 0..1 (0 = black sky at full strength, 1 = sky untouched)
uniform vec4 tron_grade3;
// x = grid intensity (0 = off; display-intent linear, /E), y = spacing m,
// z = line HALF width m, w = minimum line width in px (already resolution-scaled by the host)
uniform vec4 tron_grid;
// x = major line every N (0/1 = none), y = major width multiplier, z = major
// intensity multiplier, w = far fade distance m (0 = none)
uniform vec4 tron_grid2;
// x = floor weight, y = wall weight, z = ceiling weight (0..1), w = triplanar sharpness k
uniform vec4 tron_grid3;
// x = normal source (0 G-buffer, 1 geometric), y = glow alpha feed 0..1,
// z = subject exclude 0..1, w reserved
uniform vec4 tron_grid4;
// x = subject radius m (0 = whole world), y = radius feather m,
// z = water mode (0 skip, 1 sea: floor grid in the secondary colour, 2 floor),
// w = water height tolerance m
uniform vec4 tron_grid5;
// x = agent-space water height of the current region (m), y = camera underwater 0/1,
// z = water detection enabled (1 at the Camera layer, 0 at the Scene layer where
// water has not been drawn yet), w reserved
uniform vec4 tron_water;
// x = pulse amount (multiple of the grid intensity, 0 = off), y = time phase
// 0..1 = fract(clock * speed / wavelength) computed on the CPU in F64,
// z = pulse length (fraction of the wavelength), w = wavelength m
uniform vec4 tron_pulse;
// x = density 0..1 (fraction of lines carrying a pulse), y = direction mode
// (0 both ways, 1 +axis, 2 -axis, 3 hashed per line), z = seed (integer
// valued), w = colour mode (0 pulse colour, 1 secondary, 2 white)
uniform vec4 tron_pulse2;
// x = target count 0..16, y = target shape (0 ellipse, 1 box, 2 capsule),
// z = shape feather (fraction of the radius), w = depth slab feather m
uniform vec4 tron_subject;
// x = grid mask source 0..3 (see header), y = rim mask source (T2), z = invert 0/1, w reserved
uniform vec4 tron_subject2;
// per target (culled, screen projected; Roto packing): centre uv, half extents uv
uniform vec4 tron_targets[16];
// per target: x view depth m, y slab half depth m, z palette index, w valid 0/1
uniform vec4 tron_targets2[16];
// per UN-culled anchor: xyz centre in the p_w frame (m), w bounding radius m
uniform vec4 tron_anchors[16];
uniform int  tron_anchor_count;
// Global lattice frames (ALTron::computeLattice, design v4 section 2.3):
// [0] grid spacing, [1] major spacing (spacing * N), [2] pulse wavelength,
// [3] trace cell (T2). xyz = remainder r_s in [0, s) (m), w = the scale s.
uniform vec4  tron_lattice_r[4];
// xyz = integer lattice index k_s of the anchor (mod 2^32), w unused.
uniform uvec4 tron_lattice_k[4];

// deferredUtil/gbufferUtil are linked in as separate compile units, so their
// functions must be forward-declared here.
float getDepth(vec2 pos_screen);
vec4  getNorm(vec2 screenpos);
vec4  getNormRaw(vec2 screenpos);

const vec3 TRON_LUM = vec3(0.2126, 0.7152, 0.0722);

// ---------------------------------------------------------------- noise ---
// PCG integer hash, same construction as rotoPcg / rotoHash21
// (cineOutlineF.glsl): uint ops need GLSL 1.30+; every deferred program here
// is compiled at #version 140 or above. Keys are GLOBAL lattice ids (uvec),
// so no float -> int conversion of a large value ever happens.
uint tronPcg(uint v)
{
    uint s = v * 747796405u + 2891336453u;
    uint w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u;
    return (w >> 22u) ^ w;
}

float tronHashU3(uvec3 c)
{
    uint h = tronPcg(c.x ^ tronPcg(c.y ^ tronPcg(c.z + 0x9E3779B9u)));
    return float(h) * (1.0 / 4294967296.0);
}

// -------------------------------------------------------- reconstruction ---
vec3 tronViewPos(vec2 uv, float d)
{
    vec4 p = tron_inv_proj * vec4(uv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    return p.xyz / p.w;
}

// -------------------------------------------------------------- subject ---
// Screen shape weight for a target (x,y centre, z,w half extents, uv).
// shape 0 ellipse, 1 box, 2 vertical capsule. fe = feather as a fraction of
// the radius. Trimmed copy of rotoTargetShape (cineOutlineF.glsl) so the Roto
// pass stays untouched.
float tronTargetShape(vec2 uv, vec4 tg, int shape, float fe)
{
    vec2 radius = max(tg.zw, vec2(1.0e-3));
    if (shape == 1)
    {
        vec2 q = abs(uv - tg.xy) / radius;
        return 1.0 - smoothstep(1.0 - fe, 1.0 + fe, max(q.x, q.y));
    }
    if (shape == 2)
    {
        // capsule in pixel space so the caps stay round on any aspect
        vec2  d_px = (uv - tg.xy) * screen_res;
        float r_px = radius.x * screen_res.x;
        float h_px = max(radius.y * screen_res.y - r_px, 0.0);
        d_px.y = max(abs(d_px.y) - h_px, 0.0);
        return 1.0 - smoothstep(1.0 - fe, 1.0 + fe, length(d_px) / max(r_px, 1.0));
    }
    vec2 q = (uv - tg.xy) / radius;
    return 1.0 - smoothstep(1.0 - fe, 1.0 + fe, length(q));
}

float tronDepthSlab(float z_c, float fz, float half_d, float feather)
{
    return 1.0 - smoothstep(half_d, half_d + feather, abs(z_c - fz));
}

// Per-pixel avatar tag from the RAW G-buffer normal .w. A literal 0.0 when
// the preamble did not emit GBUFFER_AVATAR_TAG (tag off / non-HDR); the host
// also uploads mask source 0 in that case.
float tronAvatarTag(float nraw_w)
{
#ifdef GBUFFER_AVATAR_TAG
    return GBUFFER_AVATAR_TAG_OF(nraw_w) ? 1.0 : 0.0;
#else
    return 0.0;
#endif
}

// Union over the culled screen targets: shape x depth slab (Roto mode-3
// semantics, ellipse amount fixed at 1).
float tronBoxMask(vec2 uv, float z_c)
{
    int   count   = int(tron_subject.x + 0.5);
    int   shape   = int(tron_subject.y + 0.5);
    float fe      = max(tron_subject.z, 0.01);
    float feather = max(tron_subject.w, 1.0e-3);
    float m = 0.0;
    for (int i = 0; i < 16; ++i)
    {
        if (i >= count)
        {
            break;
        }
        vec4 t2 = tron_targets2[i];
        if (t2.w < 0.5)
        {
            continue;
        }
        float w = tronDepthSlab(z_c, t2.x, max(t2.y, 0.0), feather);
        w *= tronTargetShape(uv, tron_targets[i], shape, fe);
        m = max(m, w);
    }
    return m;
}

// 0 boxes, 1 tag, 2 tag AND boxes, 3 tag OR boxes.
float tronApplySource(int source, float m_box, float tag)
{
    if (source <= 0)
    {
        return m_box;
    }
    if (source == 1)
    {
        return tag;
    }
    if (source == 2)
    {
        return tag * m_box;
    }
    return max(tag, m_box);
}

// Distance from p_w to the nearest UN-culled anchor's bounding sphere
// (negative inside). 1e9 when there is no anchor -> "grid only near subject"
// draws nothing, by design.
float tronAnchorDistance(vec3 p_w)
{
    float d = 1.0e9;
    for (int i = 0; i < 16; ++i)
    {
        if (i >= tron_anchor_count)
        {
            break;
        }
        vec4 a = tron_anchors[i];
        d = min(d, length(p_w - a.xyz) - max(a.w, 0.0));
    }
    return d;
}

// ----------------------------------------------------------------- pulses ---
// Head at ph -> 1, tail over `len` of the period behind it. smoothstep output
// is in [0,1], squared for a hotter head (no pow()).
float tronPulseWave(float ph, float len)
{
    float w = smoothstep(1.0 - len, 1.0, ph);
    return w * w;
}

// Travelling pulse on one grid line. line_id is the GLOBAL lattice index of
// the line (stable across region crossings / anchor re-snaps / sessions),
// salt separates planes and axes, along_q = (p_along + r_L) / L with r_L the
// wavelength lattice's own remainder for that axis.
float tronLinePulse(uint line_id, uint salt, float along_q)
{
    float density = clamp(tron_pulse2.x, 0.0, 1.0);
    if (density <= 0.0)
    {
        return 0.0;
    }
    uint  seed   = uint(clamp(tron_pulse2.z, 0.0, 65535.0) + 0.5);
    uvec3 key    = uvec3(line_id, salt * 40503u + seed, 0x7F4A7C15u);
    float h_gate = tronHashU3(key);
    if (h_gate >= density)              // fraction `density` of the lines carry pulses
    {
        return 0.0;
    }
    float h_ph  = tronHashU3(key ^ uvec3(0u, 0u, 0x5BD1E995u));
    float h_dir = tronHashU3(key ^ uvec3(0u, 0u, 0x68E31DA4u));
    int   mode  = int(tron_pulse2.y + 0.5);
    float len   = clamp(tron_pulse.z, 0.02, 1.0);
    // [TronT1 P2-1 fix] The head position for a phase value ph is the along_q
    // where fract(ph) wraps to 1, i.e. where the argument's along_q term
    // cancels the -phase_t term. For forward: arg_fwd = along_q - phase_t,
    // so the ph==1 locus is along_q = phase_t + k -- head moves toward
    // +axis as phase_t (time) advances. For backward we need the head to
    // move toward -axis as time advances, i.e. the locus along_q = -phase_t
    // - k: that means NEGATING ONLY the spatial term (along_q -> -along_q)
    // and KEEPING the time term's sign (still -phase_t), giving
    // arg_bwd = -along_q - phase_t. The previous code instead negated the
    // whole forward argument (-t = -along_q + phase_t), which flips the
    // spatial term but ALSO flips the time term back to +phase_t --
    // reproducing the same "head moves with +time" relationship as forward,
    // just mirrored in space, so "backward" pulses still travelled forward.
    // (A negative TronPulseGridSpeed on the CPU still reverses both, since
    // it flips the sign of phase_t itself before either term sees it.)
    float t_fwd = along_q - tron_pulse.y;
    float t_bwd = -along_q - tron_pulse.y;
    float fwd = tronPulseWave(fract(t_fwd + h_ph), len);
    float bwd = tronPulseWave(fract(t_bwd + h_ph + 0.5), len);
    if (mode == 1)
    {
        return fwd;
    }
    if (mode == 2)
    {
        return bwd;
    }
    if (mode == 3)
    {
        return (h_dir < 0.5) ? bwd : fwd;
    }
    return max(fwd, bwd);               // 0: both directions on every pulsing line
}

// ------------------------------------------------------------------- grid ---
// Anti-aliased line coverage of one lattice on one plane.
//   p2  plane coordinates (m, camera-relative-to-anchor frame)
//   fw2 per-axis metres per pixel (screen derivative of the reconstructed
//       position, rotated into world axes)
//   r2  lattice remainder for these two axes, s = spacing, hw_m = half width,
//       min_px = minimum on-screen line width in pixels.
// Returns per-axis coverage: .x for lines of constant p2.x, .y for lines of
// constant p2.y; q receives the lattice coordinate (line index = floor(q + .5)).
vec2 tronGridLines(vec2 p2, vec2 fw2, vec2 r2, float s, float hw_m, float min_px, out vec2 q)
{
    q = (p2 + r2) / s;
    vec2 dist = (0.5 - abs(fract(q) - 0.5)) * s;            // metres to the nearest line per axis
    vec2 hw   = max(vec2(hw_m), fw2 * (0.5 * min_px));       // never thinner than min_px on screen
    vec2 l    = 1.0 - smoothstep(hw - fw2, hw + fw2, dist);  // 1-px feather (edge0 < edge1 always)
    // moire guard: fade out where a cell covers less than ~3 px on screen
    // (also kills lines across depth discontinuities, where fw2 explodes)
    float cell_px = s / max(max(fw2.x, fw2.y), 1.0e-6);
    return l * smoothstep(2.0, 4.0, cell_px);
}

// Full grid on one plane: returns vec3(minor coverage, major coverage, pulse
// coverage), each 0..1, unweighted.
//   rg/kg minor lattice (index 0), rM/kM major lattice (index 1), rL wavelength
//   lattice remainder (index 2), all already swizzled to this plane's axes.
//   salt: 0 floor/ceiling (xy), 1 wall facing +/-X (yz), 2 wall facing +/-Y (xz).
vec3 tronGridPlane(vec2 p2, vec2 fw2, vec2 rg, uvec2 kg, vec2 rM, uvec2 kM, vec2 rL, uint salt)
{
    float s   = max(tron_grid.y, 0.01);
    float hw  = max(tron_grid.z, 0.0);
    float mpx = max(tron_grid.w, 0.0);

    vec2 q;
    vec2  lmin  = tronGridLines(p2, fw2, rg, s, hw, mpx, q);
    float minor = max(lmin.x, lmin.y);

    float major   = 0.0;
    float n_major = floor(tron_grid2.x + 0.5);
    if (n_major >= 1.5)                    // 0 / 1 = no major lines
    {
        float wmul = max(tron_grid2.y, 1.0);
        vec2  qM;
        vec2  lmaj = tronGridLines(p2, fw2, rM, s * n_major, hw * wmul, mpx * wmul, qM);
        major = max(lmaj.x, lmaj.y);
    }

    float pulse = 0.0;
    if (tron_pulse.x > 0.0)
    {
        float L = max(tron_pulse.w, 0.01);
        if (lmin.y > 1.0e-3)               // line of constant p2.y runs along the first axis
        {
            uint id = uint(int(floor(q.y + 0.5))) + kg.y;
            pulse += lmin.y * tronLinePulse(id, salt * 2u + 1u, (p2.x + rL.x) / L);
        }
        if (lmin.x > 1.0e-3)               // line of constant p2.x runs along the second axis
        {
            uint id = uint(int(floor(q.x + 0.5))) + kg.x;
            pulse += lmin.x * tronLinePulse(id, salt * 2u + 2u, (p2.y + rL.y) / L);
        }
    }
    return vec3(minor, major, min(pulse, 1.0));
}

void main()
{
    vec2 uv    = vary_fragcoord;
    vec4 scene = texture(tronScene, uv);

    // ---- reconstruction (uniform control flow; ALL derivatives here) -----
    float d       = getDepth(uv);
    bool  cleared = d >= 1.0;                        // GL clear depth; every written surface is < 1.0
    vec4  nraw    = getNormRaw(uv);
    // finite substitute ONLY for cleared pixels; surface depth is never altered
    vec3  p_v     = tronViewPos(uv, cleared ? 0.9999 : d);
    mat3  R       = mat3(tron_inv_modelview);        // rigid camera: rotation block maps view -> world axes
    vec3  p_w     = tron_cam_rel.xyz + R * p_v;      // camera-relative-to-anchor world position (m)

    vec3  dpx_v = dFdx(p_v);
    vec3  dpy_v = dFdy(p_v);
    vec3  fw3   = abs(R * dpx_v) + abs(R * dpy_v) + vec3(1.0e-7);   // metres per pixel per world axis

    vec3  ngb_v = getNorm(uv).xyz;                   // eye-space G-buffer normal (.w is never written; do not read it)
    float ngl   = dot(ngb_v, ngb_v);
    vec3  n_gb  = (ngl > 1.0e-8) ? normalize(R * ngb_v) : vec3(0.0, 0.0, 1.0);

    vec3  cr    = cross(dpx_v, dpy_v);
    float crl   = dot(cr, cr);
    bool  n_ok  = crl > 1.0e-10 * dot(dpx_v, dpx_v) * dot(dpy_v, dpy_v);   // relative degeneracy test
    vec3  n_geo = n_gb;                              // fall back to the G-buffer normal, not a constant
    if (n_ok)
    {
        vec3 ng_v = cr * inversesqrt(crl);
        if (dot(ng_v, -p_v) < 0.0)                   // face the camera (no sign())
        {
            ng_v = -ng_v;
        }
        n_geo = R * ng_v;
    }
    float z_c = max(-p_v.z, 1.0e-3);

    // ---- water DETECTION before the sky classification --------------------
    // The water pool overwrites colour and depth but never the G-buffer, so a
    // water pixel carries the seabed's normal and, with sky behind it, the
    // SKIP_ATMOS flag: detect it geometrically (agent-space height + a
    // horizontal reconstructed surface) first. Approximate (a dock at exactly
    // water height passes too); mode 0 (skip) is the safe default.
    float p_agent_z = (p_w.z - tron_cam_rel.z) + tron_cam_rel.w;
    bool  is_water  = !cleared && tron_water.z > 0.5 && tron_water.y < 0.5 &&
                      abs(p_agent_z - tron_water.x) < max(tron_grid5.w, 0.0) + z_c * 0.002 &&
                      n_geo.z > 0.95;
    bool  sky = cleared || (!is_water && GET_GBUFFER_FLAG(nraw.w, GBUFFER_FLAG_SKIP_ATMOS));
    float tag = tronAvatarTag(nraw.w);

    // ---- subject mask (boxes / tag per TronSubjectSourceGrid) --------------
    float m_box = tronBoxMask(uv, z_c);
    float M     = tronApplySource(int(tron_subject2.x + 0.5), m_box, tag);
    if (tron_subject2.z > 0.5)
    {
        M = 1.0 - M;
    }

    // ---- effective exposure (identical to cineOutlineF.glsl) ---------------
    //   exposure > 0 : E = exposureMap * exposure   (manual * auto adaptation)
    //   exposure < 0 : E = -exposure                (manual iris only: S-Log3 lock)
    //   exposure == 0: E = 1                        (Scene layer paint semantics / no-post)
    // Floored at 1/1024; worst case 64 * 1024 = 65536 -> the 65000 clamp below.
    float E = 1.0;
    if (exposure > 0.0)
    {
        E = texture(exposureMap, vec2(0.5)).r * exposure;
    }
    else if (exposure < 0.0)
    {
        E = -exposure;
    }
    E = max(E, 1.0 / 1024.0);

    // ---- dark grade ---------------------------------------------------------
    vec3  graded   = scene.rgb;
    float strength = clamp(tron_grade.x, 0.0, 1.0);
    if (strength > 0.0)
    {
        float lum   = dot(scene.rgb, TRON_LUM);
        float kb_lo = max(tron_grade3.x, 0.0);
        float kb_hi = max(tron_grade3.y, kb_lo + 1.0e-3);       // edge0 < edge1
        // keep-bright on EXPOSED luminance so the slider means the same thing
        // under auto-exposure; authored emissive / neon survives the grade
        float keep_bright = smoothstep(kb_lo, kb_hi, lum * E);
        float keep_subj   = clamp(tron_grade3.z, 0.0, 1.0) * M;
        float amt = strength * (1.0 - keep_bright) * (1.0 - keep_subj);
        if (sky)
        {
            // sky rule: scale toward black by the sky-darken slider (0 = black);
            // keep terms still apply (alpha hair / glass over sky carry sky
            // depth at the Camera layer -- documented limitation)
            graded = mix(scene.rgb, scene.rgb * clamp(tron_grade3.w, 0.0, 1.0), amt);
        }
        else
        {
            vec3 g = mix(scene.rgb, vec3(lum), clamp(tron_grade.z, 0.0, 1.0));       // desaturate
            g *= mix(vec3(1.0), max(tron_grade2.rgb, vec3(0.0)), clamp(tron_grade2.w, 0.0, 1.0)); // tint
            g *= exp2(-clamp(tron_grade.y, 0.0, 16.0));                             // darken in stops
            float crush = clamp(tron_grade.w, 0.0, 0.5);
            g = max(g - vec3(crush / E), vec3(0.0)) / (1.0 - crush);                // black lift in exposed units
            graded = mix(scene.rgb, g, amt);
        }
    }

    // ---- world grid + pulses --------------------------------------------
    vec3  neon = vec3(0.0);
    float cov  = 0.0;
    float grid_int   = max(tron_grid.x, 0.0);
    int   water_mode = int(tron_grid5.z + 0.5);
    bool  draw_grid  = grid_int > 0.0 && !sky && !(is_water && water_mode == 0);
    if (draw_grid)
    {
        vec3 n_w      = (tron_grid4.x > 0.5) ? n_geo : n_gb;   // geometric option avoids normal-map speckle
        vec3 line_col = tron_palette0.rgb;
        if (is_water)
        {
            n_w = vec3(0.0, 0.0, 1.0);                          // water is always a floor
            if (water_mode == 1)
            {
                line_col = tron_palette1.rgb;                   // "Sea of Simulation": secondary colour
            }
        }
        vec3 major_col = mix(line_col, tron_palette2.rgb, 0.5);

        // triplanar weights (SL agent space is Z-up); abs() keeps the pow base >= 0
        float k = clamp(tron_grid3.w, 1.0, 32.0);
        vec3  a = pow(max(abs(n_w), vec3(1.0e-4)), vec3(k));
        // [TronT1 P2-11 fix] a.x+a.y+a.z can only be exactly 0 if abs(n_w)
        // were 0 on every axis, which the max(abs(n_w), 1.0e-4) floor above
        // already rules out -- so this divisor floor exists purely to stop a
        // divide-BY-a-tiny-but-nonzero-number blow-up, not a divide-by-true-
        // zero. 1.0e-6 was itself reachable: at k = 32 (the slider's own
        // max), (1.0e-4)^32 underflows to 0 in fp32 well before the sum
        // does, so a legitimately tiny (not degenerate) triplanar sum could
        // still get clamped up to 1.0e-6, inflating `a` far past 1 and
        // blowing out the neon. 1.0e-30 is still comfortably above fp32's
        // ~1.0e-38 denormal floor (so the divide itself stays finite) while
        // never being hit by any in-range k/normal combination.
        a /= max(a.x + a.y + a.z, 1.0e-30);
        float w_flat = a.z * ((n_w.z >= 0.0) ? clamp(tron_grid3.x, 0.0, 1.0)      // floor
                                             : clamp(tron_grid3.z, 0.0, 1.0));     // ceiling
        float w_wx   = a.x * clamp(tron_grid3.y, 0.0, 1.0);    // wall facing +/-X -> plane (y, z)
        float w_wy   = a.y * clamp(tron_grid3.y, 0.0, 1.0);    // wall facing +/-Y -> plane (x, z)

        vec3  rg = tron_lattice_r[0].xyz;  uvec3 kg = tron_lattice_k[0].xyz;
        vec3  rM = tron_lattice_r[1].xyz;  uvec3 kM = tron_lattice_k[1].xyz;
        vec3  rL = tron_lattice_r[2].xyz;

        vec3 acc = vec3(0.0);   // weighted (minor, major, pulse)
        if (w_flat > 1.0e-3)
        {
            acc += w_flat * tronGridPlane(p_w.xy, fw3.xy, rg.xy, kg.xy, rM.xy, kM.xy, rL.xy, 0u);
        }
        if (w_wx > 1.0e-3)
        {
            acc += w_wx * tronGridPlane(p_w.yz, fw3.yz, rg.yz, kg.yz, rM.yz, kM.yz, rL.yz, 1u);
        }
        if (w_wy > 1.0e-3)
        {
            acc += w_wy * tronGridPlane(p_w.xz, fw3.xz, rg.xz, kg.xz, rM.xz, kM.xz, rL.xz, 2u);
        }

        // far fade (depth quantisation shimmer), subject exclusion, subject radius
        float fade = 1.0;
        float F = tron_grid2.w;
        if (F > 0.0)
        {
            fade = 1.0 - smoothstep(0.7 * F, F, z_c);
        }
        fade *= 1.0 - clamp(tron_grid4.z, 0.0, 1.0) * M;
        if (tron_grid5.x > 0.0)
        {
            float r = tron_grid5.x;
            float f = max(tron_grid5.y, 1.0e-3);
            fade *= 1.0 - smoothstep(r, r + f, tronAnchorDistance(p_w));   // no anchors -> 1e9 -> 0
        }
        acc *= fade;

        float minor = min(acc.x, 1.0);
        float major = min(acc.y, 1.0);
        float pulse = min(acc.z, 1.0);

        int  pmode = int(tron_pulse2.w + 0.5);
        vec3 pulse_col = (pmode == 1) ? tron_palette1.rgb
                       : (pmode == 2) ? vec3(1.0)
                                      : tron_palette3.rgb;

        neon += grid_int * (line_col  * minor * (1.0 - major)
                          + major_col * major * max(tron_grid2.z, 0.0)
                          + pulse_col * pulse * max(tron_pulse.x, 0.0));
        cov = clamp(max(max(minor, major), pulse), 0.0, 1.0);
    }

    // ---- compose: ONE exposure rule ---------------------------------------
    neon *= max(tron_master.w, 0.0);                 // no-post frames: scale, then clamp
    if (tron_palette1.w > 0.5)
    {
        neon = min(neon, vec3(1.0));                 // colorCorrect will not tonemap -> avoid hard clips
    }
    // exposure-independent on screen (Roto rule); E == 1 at the Scene layer.
    // The target is RGBA16F (max ~65504): clamp so an inf can never poison
    // the bloom pyramid.
    vec3  outc  = min(graded + neon / E, vec3(65000.0));
    float out_a = min(scene.a + cov * clamp(tron_grid4.y, 0.0, 1.0), 65000.0);
    frag_color = vec4(outc, out_a);
}
