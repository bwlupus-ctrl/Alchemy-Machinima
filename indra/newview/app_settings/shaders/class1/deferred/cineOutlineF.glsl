/**
 * @file cineOutlineF.glsl
 *
 * [RotoInk] Rotoscope Ink: native deferred ink-line post pass (successor of
 * the Cine Outline Phase-1 Sobel pass). Composites IN PLACE by reading a copy
 * of the destination (rotoScene) and writing every pixel back with blending
 * OFF, so the fixed / match-light ink colour, the three blend modes and the
 * legacy alpha glow feed are all resolved here in one draw.
 *
 * Line model (per stroke, evaluated at the stroke's boil-shifted uv):
 *   centre: z_c (view depth, m), p_c (view pos), n_c (G-buffer normal)
 *   two 8-tap rings at radius R0 = floor(width_px) and R1 = R0 + 1,
 *   blended by fract(width_px) for fractional / anti-aliased width.
 *   For each tap i (direction d_i on the unit circle):
 *     z_i    = neighbour view depth
 *     z_pred = depth the CENTRE's tangent plane predicts along the neighbour's
 *              view ray (clamped to [0.5, 1.5] * z_c so a plane can never
 *              explain a > 50 % relative step; near-parallel rays fall back to
 *              z_c, i.e. a plain depth step)
 *     resid  = z_i - z_pred          (metric)   or   / z_c   (relative)
 *              > 0  : neighbour is BEHIND the centre's surface (silhouette,
 *                     drawn on the near / subject side of the edge)
 *     sil_i    = smoothstep(thr*(1-soft/2), thr*(1+soft/2), resid)
 *     crease_i = smoothstep(cthr_r*(1-soft/2), cthr_r*(1+soft/2), 1 - n_c.n_i)
 *                * (1 - smoothstep(.., |resid|))    (only on continuous surface)
 *              cthr_r = 1 - cos(min(angle * r / crease_px, 89 deg)): a fixed
 *              crease_px (1 px at 1080p) ring detects creases at the authored
 *              angle; the width rings only DILATE creases sharper than the
 *              curvature a smooth surface shows over their radius (P2-2).
 *   sil = max_i sil_i, crease = max(crease_1px, max_i crease_i)
 *   edgeDir = normalize(sum d_i sil_i)  |  sum d_i crease_i  |  n_c.xy (P2-6)
 *   line = 1 - (1 - sil * wS) * (1 - crease * wC)
 *   strokes 1..4 (only while boil > 0): line = max over strokes; roughness
 *   grain (PCG integer hash, P2-3) breaks the line.
 * Subject isolation: M = depthSlab(z_c) * mix(1, ellipse(uv), amount * valid),
 *   mode 0 M = 1, mode 1 M, mode 2 1 - M.
 * Ink: cov = line * M * opacity * farFade * motionWeight(edgeDir)
 *   colour = outline_color (fixed) or the max-channel-normalised brightest
 *   HDR scene colour just outside the edge (match light; fades to the fixed
 *   colour / black where the surroundings are dark in exposed terms, P2-5),
 *   * intensity, and for normal/add divided by E = auto * manual exposure so
 *   the on-screen ink brightness is exposure-independent (P2-1).
 *   blend 0 normal: mix(scene, ink, cov); 1 multiply: scene * mix(1, ink, cov);
 *   2 add: scene + ink * cov.   alpha = scene.a + cov * glow  (legacy feed).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2026, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * $/LicenseInfo$
 */

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;

in vec2 vary_fragcoord;

// [RotoInk] copy of the destination target (LLPipeline::mWaterDis scratch)
uniform sampler2D rotoScene;
// [RotoInk] round-1 P2-1: exposure the tonemapper will apply later (manual *
// auto), so fixed-colour / match-light ink can be written pre-exposure and
// still land at a stable on-screen brightness. Host contract: HDR call site
// uploads `exposure` = the same effective manual value onLensFilters uses and
// binds mExposureMap; the non-HDR (post-tonemap) call site uploads
// exposure = 0.0, which makes E = 1 below.
uniform sampler2D exposureMap;   // 1x1 auto-exposure scale
uniform float     exposure;      // manual exposure knob (0 = "already exposed")

uniform vec2 screen_res;
uniform vec3 outline_color;      // ink colour, linear RGB
// x = line width in px (already scaled to the current resolution by the host),
// y = softness 0..1 (threshold feather, fraction of the threshold),
// z = far cutoff in metres (0 = none), w = HDR intensity.
uniform vec4 outline_params;
// x = legacy glow / HDR alpha feed 0..1, y = depth mode (0 relative, 1 metric),
// z = ink colour mode (0 fixed, 1 match light), w = blend (0 normal, 1 multiply, 2 add).
uniform vec4 outline_params2;
// x = silhouette threshold (relative, or metres when metric),
// y = crease threshold as (1 - cos(angle)), z = silhouette weight (0 off),
// w = crease weight (0 off).
uniform vec4 roto_line;
// x = opacity 0..1, y = match-light reach in px (resolution-scaled),
// z = 1.0 when the destination is DISPLAY-encoded (Overlay layer / non-HDR
//     post-colorCorrect site; the ink colour is then sRGB-encoded before the
//     blend and rotoScene samples are decoded before matching), 0.0 for the
//     linear pre-tonemap site. w reserved.
uniform vec4 roto_ink;
// x = subject mode (0 all, 1 subject only, 2 everything except subject),
// y = subject view depth (m), z = slab half depth (m), w = slab feather (m).
uniform vec4 roto_subject;
// screen-space ellipse around the subject's projected bounds: x,y centre (uv), z,w radii (uv).
uniform vec4 roto_subject2;
// x = ellipse amount 0..1 (0 = depth slab only), y = ellipse feather (fraction of
// the radius), z = 1 when the ellipse is valid this frame (subject on screen), w reserved.
uniform vec4 roto_subject3;
// x = sketch enable, y = boil amount px (resolution-scaled), z = detail 0..1, w = redraw fps.
uniform vec4 roto_sketch;
// x = strokes 1..4, y = roughness 0..1, z = seed, w reserved.
uniform vec4 roto_sketch2;
// x = motion style (0 none, 1 pulse, 2 chase, 3 scan, 4 wave, 5 shimmer),
// y = speed, z = amount 0..1, w = scale (per style).
uniform vec4 roto_motion;
// x = scan angle (radians), y = time (seconds, host-wrapped), z/w reserved.
uniform vec4 roto_motion2;

// deferredUtil/gbufferUtil are linked in as separate compile units, so their
// functions must be forward-declared here.
float getDepth(vec2 pos_screen);
vec4 getNorm(vec2 screenpos);
vec4 getNormRaw(vec2 screenpos);
vec4 getPositionWithDepth(vec2 pos_screen, float depth);
// environment/srgbF.glsl (attached for every isDeferred program)
vec3 linear_to_srgb(vec3 cl);
vec3 srgb_to_linear(vec3 cs);

const float ROTO_PI  = 3.14159265;
const float ROTO_TAU = 6.28318531;

// 8 unit-circle tap directions (E, NE, N, NW, W, SW, S, SE)
const vec2 ROTO_DIRS[8] = vec2[8](
    vec2( 1.0,  0.0), vec2( 0.70710678,  0.70710678),
    vec2( 0.0,  1.0), vec2(-0.70710678,  0.70710678),
    vec2(-1.0,  0.0), vec2(-0.70710678, -0.70710678),
    vec2( 0.0, -1.0), vec2( 0.70710678, -0.70710678));

// ---------------------------------------------------------------- noise ---
// [RotoInk] round-1 P2-3: integer (PCG) hash instead of fract(sin(big)).
// The lattice coordinates reach a few thousand cells once the redraw-frame
// and seed offsets are added, which is where fp32 sin() range reduction
// falls apart on AMD/Intel (striped, blocky grain). uint ops need GLSL 1.30+;
// every deferred program here is compiled at #version 140 or above.
uint rotoPcg(uint v)
{
    uint s = v * 747796405u + 2891336453u;
    uint w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u;
    return (w >> 22u) ^ w;
}

// p is a lattice coordinate (integer-valued float, may be negative/large).
float rotoHash21(vec2 p)
{
    ivec2 ip = ivec2(floor(p));
    uint  h  = rotoPcg(uint(ip.x) ^ rotoPcg(uint(ip.y) + 0x9E3779B9u));
    return float(h) * (1.0 / 4294967296.0);
}

float rotoVNoise(vec2 p)
{
    vec2 ip = floor(p);
    vec2 fp = fract(p);
    vec2 u  = fp * fp * (3.0 - 2.0 * fp);
    float a = rotoHash21(ip);
    float b = rotoHash21(ip + vec2(1.0, 0.0));
    float c = rotoHash21(ip + vec2(0.0, 1.0));
    float d = rotoHash21(ip + vec2(1.0, 1.0));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

// ---------------------------------------------------------- sketch/boil ---
// Redraw "frame" counter: the boil pattern only changes SketchFPS times per
// second (12 = on twos, 8 = on threes) regardless of the render frame rate.
// Time wrap: the host wraps time at 3600 s. Motion terms use fract(t) /
// sin(t*TAU) with t = time * speed and are continuous across the wrap for
// every UI speed (multiples of 0.05 -> 3600 * speed is an integer). The
// redraw counter below jumps once per hour (3600 * fps mod 1024 != 0), which
// is one extra reshuffle of an already random pattern -- deliberately left.
float rotoSketchFrame()
{
    float fr = floor(roto_motion2.y * max(roto_sketch.w, 0.5));
    return mod(fr, 1024.0);
}

// Per-stroke wobble offset in [-1, 1]^2 (multiplied by the boil amount in px).
vec2 rotoSketchOffset(vec2 uv, float stroke)
{
    float freq  = mix(40.0, 200.0, clamp(roto_sketch.z, 0.0, 1.0));
    float aspect = screen_res.x / max(screen_res.y, 1.0);
    vec2  P     = uv * vec2(aspect, 1.0) * freq;
    float frm   = rotoSketchFrame();
    vec2  seed  = vec2(frm * 1.7, frm * 2.3) + (stroke + roto_sketch2.z) * vec2(7.13, 3.71);
    float nx = rotoVNoise(P + seed);
    float ny = rotoVNoise(P + seed + 19.7);
    return vec2(nx, ny) * 2.0 - 1.0;
}

// ---------------------------------------------------------------- motion ---
float rotoMotionWeight(vec2 uv, vec2 edgeDir)
{
    int style = int(roto_motion.x + 0.5);
    if (style <= 0)
    {
        return 1.0;
    }
    float amount = clamp(roto_motion.z, 0.0, 1.0);
    float t      = roto_motion2.y * roto_motion.y;
    float scale  = max(roto_motion.w, 1.0e-3);

    if (style == 1) // pulse: breathes the whole line
    {
        return max(1.0 + amount * sin(t * ROTO_TAU), 0.0);
    }

    float ang = atan(edgeDir.y, edgeDir.x);
    float p   = ang / ROTO_TAU + 0.5;   // edge direction as a 0..1 phase

    float w = 1.0;
    if (style == 2)      // chase: a comet draws around the outline
    {
        float d = fract(fract(t) - p);
        float c = clamp(1.0 - d / scale, 0.0, 1.0);
        w = c * c;
    }
    else if (style == 3) // scan: a bar sweeps across the frame
    {
        float a  = roto_motion2.x;
        float ss = dot(uv - 0.5, vec2(cos(a), sin(a))) + 0.5;
        float d  = abs(fract(ss - fract(t) + 0.5) - 0.5);
        w = clamp(1.0 - d / scale, 0.0, 1.0);
    }
    else if (style == 4) // wave: rotating segments keyed on edge direction
    {
        float seg = floor(scale * 12.0) + 1.0;
        w = 0.5 + 0.5 * sin(p * seg * ROTO_TAU - t * ROTO_TAU);
    }
    else                 // shimmer: twinkling cells keyed on edge direction
    {
        float grain = max(scale * 40.0, 1.0);
        float h     = rotoHash21(vec2(floor(p * grain), 7.0)); // [RotoInk] P2-3: int hash
        w = 0.5 + 0.5 * sin(t * ROTO_TAU + h * ROTO_TAU);
    }
    return mix(1.0, w, amount);
}

// ------------------------------------------------------------- line taps ---
// One ring of 8 taps at radius r_px around uv. Accumulates the outward
// silhouette direction into dir_acc and the crease direction into cdir_acc.
// [RotoInk] round-1 P2-2: cr_lo/cr_hi passed in are already scaled for this
// ring's radius (see rotoCreaseBand), so wide rings only dilate creases
// sharper than the curvature a smooth surface shows over that radius.
void rotoRing(vec2 uv, float r_px, vec3 p_c, vec3 n_c, float z_c,
              float sil_lo, float sil_hi, float cr_lo, float cr_hi, bool metric,
              inout float sil, inout float crease, inout vec2 dir_acc, inout vec2 cdir_acc)
{
    vec2  px      = r_px / screen_res;
    float plane_d = dot(n_c, p_c);          // tangent plane: n_c . p = plane_d

    for (int i = 0; i < 8; ++i)
    {
        vec2  d   = ROTO_DIRS[i];
        vec2  tc  = clamp(uv + d * px, vec2(0.0), vec2(1.0));
        float dep = getDepth(tc);
        vec3  p_i = getPositionWithDepth(tc, dep).xyz;
        float z_i = max(-p_i.z, 0.0);

        // Depth the centre's tangent plane predicts along this neighbour's
        // view ray. Flat and gently curved surfaces (even at grazing angles)
        // produce a near-zero residual; a true silhouette does not.
        vec3  ray    = normalize(p_i);
        float denom  = dot(n_c, ray);
        float z_pred = z_c;
        if (abs(denom) > 0.02)
        {
            float t = plane_d / denom;      // ray parameter of the plane hit
            z_pred = (t > 0.0) ? max(-(ray * t).z, 0.0) : z_c;
        }
        z_pred = clamp(z_pred, 0.5 * z_c, 1.5 * z_c);

        float resid = z_i - z_pred;         // > 0: neighbour behind the surface
        if (!metric)
        {
            resid /= max(z_c, 1.0e-3);
        }

        float s = smoothstep(sil_lo, sil_hi, resid);
        sil     = max(sil, s);
        dir_acc += d * s;

        // Inner crease: normal discontinuity on a CONTINUOUS surface. The
        // |resid| gate keeps creases off both sides of a silhouette (never
        // doubles the outline on the background object) and off sky taps.
        float cont = 1.0 - smoothstep(sil_lo, sil_hi, abs(resid));
        vec3  n_i  = getNorm(tc).xyz;
        float cn   = 1.0 - dot(n_c, n_i);
        float c    = smoothstep(cr_lo, cr_hi, cn) * cont;
        crease     = max(crease, c);
        cdir_acc  += d * c;
    }
}

// [RotoInk] round-1 P2-2 / round-2 (4): crease threshold band for a ring of
// radius r_px. A smooth surface of curvature k turns its normal by ~k*r over
// r px; the crease ANGLE threshold is scaled by sqrt(r / crease_px)
// (crease_px = 1 px at 1080p, resolution-scaled) and clamped below 90 deg.
// sqrt is the round-2 compromise between curvature immunity (linear) and
// letting wide presets (widths 4-5) dilate ordinary 45-60 deg creases to
// ~2-3 px instead of 1 px. The fixed 1-px crease ring (rotoLine) always
// detects creases at the authored angle; the width rings only dilate those
// sharp enough to survive the scaled threshold.
void rotoCreaseBand(float c_ang, float r_px, float crease_px, float soft,
                    out float cr_lo, out float cr_hi)
{
    float ang   = min(c_ang * sqrt(max(r_px / crease_px, 1.0)), 1.55);
    float c_thr = max(1.0 - cos(ang), 1.0e-5);
    cr_lo = c_thr * (1.0 - 0.5 * soft);
    cr_hi = c_thr * (1.0 + 0.5 * soft);
}

// Full line evaluation at one (possibly boil-shifted) uv.
// Returns x = line coverage, yz = edge direction (unnormalised): the outward
// silhouette direction when there is one, else the crease direction, else the
// screen-space projection of the surface normal (P2-6: crease-only pixels no
// longer share one fallback direction, so chase/wave/shimmer vary per pixel).
vec3 rotoLine(vec2 uv)
{
    float raw_depth = getDepth(uv);
    vec4  norm_raw  = getNormRaw(uv);
    if (raw_depth >= 0.999999 ||
        GET_GBUFFER_FLAG(norm_raw.w, GBUFFER_FLAG_SKIP_ATMOS))
    {
        return vec3(0.0);
    }

    vec3  p_c = getPositionWithDepth(uv, raw_depth).xyz;
    float z_c = max(-p_c.z, 1.0e-3);
    vec3  n_c = getNorm(uv).xyz;

    bool  metric = outline_params2.y > 0.5;
    float soft   = clamp(outline_params.y, 0.02, 1.0);
    float s_thr  = max(roto_line.x, 1.0e-5);
    float sil_lo = s_thr * (1.0 - 0.5 * soft);
    float sil_hi = s_thr * (1.0 + 0.5 * soft);

    // [RotoInk] P2-2: crease angle (from the uploaded 1 - cos) and the fixed
    // crease test radius: 1 px at 1080p, scaled to the current resolution.
    float c_ang     = acos(clamp(1.0 - max(roto_line.y, 1.0e-5), -1.0, 1.0));
    float crease_px = max(1.0, floor(screen_res.y / 1080.0 + 0.5));
    float wC        = clamp(roto_line.w, 0.0, 1.0);

    float width = clamp(outline_params.x, 0.0, 24.0);
    float r0    = floor(width);
    float wf    = width - r0;

    float sil0 = 0.0, cr0 = 0.0, sil1 = 0.0, cr1 = 0.0, crf = 0.0, silf = 0.0;
    vec2  dir  = vec2(0.0);
    vec2  cdir = vec2(0.0);
    float cr_lo, cr_hi;

    // [RotoInk] round-2 (3): the fixed 1-px crease term crf is ALWAYS its own
    // term. When the r0 ring sits at the same radius its (unscaled, scale = 1)
    // crease result is reused verbatim -- never through the fractional-width
    // mix -- so coverage stays continuous across widths 1.0 -> 1.9 -> 2.0.
    // crf is faded by min(width / crease_px, 1) so sub-pixel widths fade
    // creases exactly like the AA-scaled silhouette; width 0 -> no line.
    bool reuse_r0 = abs(r0 - crease_px) <= 1.0e-3;
    if (wC > 0.0 && !reuse_r0)
    {
        vec2 sink = vec2(0.0);
        rotoCreaseBand(c_ang, crease_px, crease_px, soft, cr_lo, cr_hi);
        rotoRing(uv, crease_px, p_c, n_c, z_c, sil_lo, sil_hi, cr_lo, cr_hi, metric, silf, crf, sink, cdir);
    }
    if (r0 > 0.0)
    {
        rotoCreaseBand(c_ang, r0, crease_px, soft, cr_lo, cr_hi);
        rotoRing(uv, r0, p_c, n_c, z_c, sil_lo, sil_hi, cr_lo, cr_hi, metric, sil0, cr0, dir, cdir);
        if (reuse_r0)
        {
            crf = cr0;
        }
    }
    if (wf > 1.0e-3)
    {
        rotoCreaseBand(c_ang, r0 + 1.0, crease_px, soft, cr_lo, cr_hi);
        rotoRing(uv, r0 + 1.0, p_c, n_c, z_c, sil_lo, sil_hi, cr_lo, cr_hi, metric, sil1, cr1, dir, cdir);
    }
    crf *= clamp(width / crease_px, 0.0, 1.0);
    float sil    = mix(sil0, sil1, wf);
    float crease = max(crf, mix(cr0, cr1, wf));

    float wS = clamp(roto_line.z, 0.0, 1.0);
    float line = 1.0 - (1.0 - sil * wS) * (1.0 - crease * wC);

    // Far cutoff (0 = none): lines fade out over the last 20 % before it.
    float cut = outline_params.z;
    if (cut > 0.0)
    {
        line *= 1.0 - smoothstep(0.8 * cut, cut, z_c);
    }

    // Edge direction priority: silhouette > crease > projected normal.
    vec2 edir = dir;
    if (dot(edir, edir) < 1.0e-8)
    {
        edir = cdir;
    }
    if (dot(edir, edir) < 1.0e-8)
    {
        edir = (dot(n_c.xy, n_c.xy) > 1.0e-6) ? normalize(n_c.xy) * 1.0e-2 : vec2(1.0e-2, 0.0);
    }
    return vec3(line, edir);
}

// --------------------------------------------------------------- subject ---
float rotoSubjectMask(vec2 uv, float z_c)
{
    int mode = int(roto_subject.x + 0.5);
    if (mode <= 0)
    {
        return 1.0;
    }
    float half_d  = max(roto_subject.z, 0.0);
    float feather = max(roto_subject.w, 1.0e-3);
    float dz      = abs(z_c - roto_subject.y);
    float m       = 1.0 - smoothstep(half_d, half_d + feather, dz);

    float amount = clamp(roto_subject3.x, 0.0, 1.0) * roto_subject3.z;
    if (amount > 0.0)
    {
        vec2  radius = max(roto_subject2.zw, vec2(1.0e-3));
        vec2  q      = (uv - roto_subject2.xy) / radius;
        float rr     = length(q);
        float fe     = max(roto_subject3.y, 0.01);
        float gate   = 1.0 - smoothstep(1.0 - fe, 1.0 + fe, rr);
        m *= mix(1.0, gate, amount);
    }
    return (mode == 2) ? 1.0 - m : m;
}

// ------------------------------------------------------------- ink colour ---
// Match light: brightest HDR scene colour just OUTSIDE the edge (along the
// outward edge direction), normalised to unit peak so only the hue is kept.
// [RotoInk] round-1 P2-5: the normalisation is faded out where the sampled
// surroundings are dark in EXPOSED terms (E = effective exposure), toward
// `fallback` (the fixed ink colour for normal/multiply, black for add), so
// near-black backgrounds never turn into full-intensity white ink.
// [RotoInk] round-4: `display` = rotoScene is display (sRGB) encoded; every
// sample is decoded to linear first so the hue, the brightness weighting and
// the dark threshold all work in linear, and the result is ALWAYS a linear
// unit-peak hue (main() encodes it exactly once, together with fixed ink).
vec3 rotoMatchLight(vec2 uv, vec2 edgeDir, float E, vec3 fallback, bool display)
{
    float reach = max(roto_ink.y, 1.0);
    float width = clamp(outline_params.x, 0.0, 24.0);
    vec2  px    = edgeDir / screen_res;
    vec3  acc   = vec3(0.0);
    float wsum  = 0.0;
    for (int k = 0; k < 3; ++k)
    {
        float dist = width + reach * (0.25 + 0.375 * float(k)); // 0.25, 0.625, 1.0 of reach
        vec2  tc   = clamp(uv + px * dist, vec2(0.0), vec2(1.0));
        vec3  sc   = max(texture(rotoScene, tc).rgb, vec3(0.0));
        if (display)
        {
            sc = srgb_to_linear(min(sc, vec3(1.0)));
        }
        float lum  = dot(sc, vec3(0.2126, 0.7152, 0.0722));
        float w    = lum * lum + 1.0e-6;           // brightest source dominates
        acc  += sc * w;
        wsum += w;
    }
    vec3  c    = acc / max(wsum, 1.0e-6);
    float m    = max(max(c.r, c.g), c.b);
    vec3  hue  = (m > 1.0e-4) ? c / m : fallback;
    // exposed luminance of what we matched: below ~2 % it is not a light
    float lumE = dot(c, vec3(0.2126, 0.7152, 0.0722)) * E;
    float lit  = smoothstep(0.02, 0.10, lumE);
    return mix(fallback, hue, lit);
}

void main()
{
    vec2 uv    = vary_fragcoord;
    vec4 scene = texture(rotoScene, uv);

    bool  sketch  = roto_sketch.x > 0.5;
    float boil_px = sketch ? max(roto_sketch.y, 0.0) : 0.0;
    // [RotoInk] P2-6: with no boil every stroke would evaluate the identical
    // line, so collapse to one stroke (roughness grain still applies).
    int   strokes = (boil_px > 0.0) ? int(clamp(roto_sketch2.x, 1.0, 4.0) + 0.5) : 1;

    // Fast path: nothing to draw on sky / atmo-skipped pixels unless the boil
    // can pull a neighbouring line onto them.
    float raw_depth = getDepth(uv);
    vec4  norm_raw  = getNormRaw(uv);
    bool  is_sky    = raw_depth >= 0.999999 ||
                      GET_GBUFFER_FLAG(norm_raw.w, GBUFFER_FLAG_SKIP_ATMOS);
    if (is_sky && boil_px <= 0.0)
    {
        frag_color = scene;
        return;
    }

    // Subject mask at the unshifted pixel (sky = far depth -> outside any slab).
    float z_c = is_sky ? 1.0e6 : max(-getPositionWithDepth(uv, raw_depth).z, 1.0e-3);
    float M   = rotoSubjectMask(uv, z_c);
    if (M <= 1.0e-4)
    {
        frag_color = scene;
        return;
    }

    // Strokes: the line is re-drawn 1..4 times, each with its own wobble.
    float line = 0.0;
    vec2  dir  = vec2(0.0);
    for (int s = 0; s < 4; ++s)
    {
        if (s >= strokes)
        {
            break;
        }
        vec2 off = (boil_px > 0.0)
            ? rotoSketchOffset(uv, float(s)) * boil_px / screen_res
            : vec2(0.0);
        vec3 r = rotoLine(clamp(uv + off, vec2(0.0), vec2(1.0)));
        line = max(line, r.x);
        dir += r.yz;
    }

    // Pencil roughness: grain breaks the line for a dry-media feel.
    float rough = sketch ? clamp(roto_sketch2.y, 0.0, 1.0) : 0.0;
    if (rough > 0.0 && line > 0.0)
    {
        float aspect = screen_res.x / max(screen_res.y, 1.0);
        float gfreq  = mix(260.0, 900.0, clamp(roto_sketch.z, 0.0, 1.0));
        float grain  = rotoVNoise(uv * vec2(aspect, 1.0) * gfreq
                                  + rotoSketchFrame() * 3.7 + roto_sketch2.z * 11.3);
        float thr    = rough * 0.9;
        float keep   = smoothstep(thr - 0.15, thr + 0.15, grain);
        line *= mix(1.0, keep, rough);
    }

    if (line <= 1.0e-4)
    {
        frag_color = scene;
        return;
    }

    float dl = length(dir);
    vec2 edgeDir = (dl > 1.0e-8) ? dir / dl : vec2(1.0, 0.0);

    float cov = line * M * clamp(roto_ink.x, 0.0, 1.0) * rotoMotionWeight(uv, edgeDir);
    cov = clamp(cov, 0.0, 1.0);

    int blend = int(outline_params2.w + 0.5);

    // [RotoInk] P2-1 / round-2 Codex P1: effective exposure the tonemapper
    // applies downstream (tonemapUtilF.glsl applyExposure):
    //   exposure > 0 : E = exposureMap * exposure   (manual * auto adaptation)
    //   exposure < 0 : E = -exposure                (manual iris ONLY: mirrors
    //                  tonemap_type == 8 / S-Log3 capture, which forces the
    //                  auto scale to 1.0; the host sends the negated manual value)
    //   exposure == 0: E = 1                        (non-HDR post-tonemap site)
    // Floored at 1/1024 (round-4 Codex P2: the S-Log3 manual iris can reach
    // 0.5 * exp2(-8) = 1/512, and a 1/64 floor left log-capture ink 8x too
    // dim). Worst case 64 intensity * 1024 = 65536 -> the final 65000 clamp
    // below keeps the RGBA16F write finite.
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

    // [RotoInk] round-4 Opus P2: display-encoded destination (Overlay layer /
    // non-HDR site write into postPingMap AFTER colorCorrect). Keyed on
    // roto_ink.z from the host, NOT on exposure == 0 (which also occurs on the
    // linear mRT->screen under legacy gamma).
    bool display = roto_ink.z > 0.5;

    // Ink colour is built in LINEAR (fixed colour, or a linear unit-peak hue
    // from match-light -- which decodes its display-encoded samples itself).
    vec3 fallback = (blend == 2) ? vec3(0.0) : outline_color;
    vec3 ink = (outline_params2.z > 0.5) ? rotoMatchLight(uv, edgeDir, E, fallback, display) : outline_color;
    ink *= max(outline_params.w, 0.0);
    // Normal / Add write an absolute ink value into the pre-exposure buffer:
    // divide by E so the on-screen ink brightness does not follow the auto
    // exposure (match-light is already reduced to a unit-peak hue, so it is
    // treated exactly like a fixed colour). Multiply is a ratio: unaffected.
    if (blend != 1)
    {
        ink /= E;
    }
    // Encode exactly once for a display-encoded target (LDR: values above 1
    // clip anyway). Normal = ink over the display image; Multiply by a
    // display-encoded ink is the usual comic darkening; Add just clips.
    if (display)
    {
        ink = linear_to_srgb(clamp(ink, vec3(0.0), vec3(1.0)));
    }

    vec3 outc;
    if (blend == 1)      // multiply
    {
        outc = scene.rgb * mix(vec3(1.0), ink, cov);
    }
    else if (blend == 2) // add / glow (legacy Phase-1 behaviour)
    {
        outc = scene.rgb + ink * cov;
    }
    else                 // normal (ink over scene)
    {
        outc = mix(scene.rgb, ink, cov);
    }

    // [RotoInk] round-2 Opus P2: the target is RGBA16F (max ~65504); an inf
    // here would poison the bloom pyramid, so clamp before writing.
    outc = min(outc, vec3(65000.0));
    float out_a = min(scene.a + cov * clamp(outline_params2.x, 0.0, 1.0), 65000.0);
    frag_color = vec4(outc, out_a);
}
