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
 *              cthr_r = 1 - cos(min(angle * sqrt(r / crease_px), 89 deg)): a
 *              fixed crease_px (1 px at 1080p) ring detects creases at the
 *              authored angle; the width rings only DILATE creases sharper
 *              than the curvature a smooth surface shows over their radius.
 *   sil = max_i sil_i, crease = max(crease_1px, max_i crease_i)
 *   edgeDir = normalize(sum d_i sil_i)  |  sum d_i crease_i  |  n_c.xy
 *   line = 1 - (1 - sil * wS) * (1 - crease * wC)
 *   strokes 1..4 (only while boil > 0): line = max over strokes; roughness
 *   grain (PCG integer hash) breaks the line.
 * Subject isolation: see rotoSubjectMask() -- single Director subject (depth
 *   slab + projected-bounds ellipse), multi-target union (ellipse / box /
 *   capsule per target, up to 16), depth-band / foreground / background /
 *   screen-shape / focus-point modes, optional invert and per-target palette.
 * Ink: cov = line * M * opacity * farFade * pattern(s) * motionWeight
 *   colour = outline_color (fixed) or the max-channel-normalised brightest
 *   HDR scene colour just outside the edge (match light; fades to the fixed
 *   colour / black where the surroundings are dark in exposed terms),
 *   then per-target palette / colour modes (two-tone, rainbow, highlight,
 *   heat), * intensity * motion intensity, and for normal/add divided by
 *   E = auto * manual exposure so the on-screen ink brightness is
 *   exposure-independent. Display-encoded destinations (roto_ink.z) encode
 *   the ink once with linear_to_srgb before the blend.
 *   blend 0 normal: mix(scene, ink, cov); 1 multiply: scene * mix(1, ink, cov);
 *   2 add: scene + ink * cov.   alpha = scene.a + cov * glow  (legacy feed).
 *
 * [RotoInk Anim] Animated patterns (round A): two motion layers (each: style,
 *   speed, amount, scale, shape, angle) whose coverage weights multiply and
 *   whose intensity weights multiply; a line PATTERN (dashed / dotted /
 *   double / hatch / taper); a secondary colour with colour modes; global
 *   timing (tempo in cycles-per-beat, phase, hold-frame stepping, seed).
 *   Styles are of four kinds:
 *     weight       -> rotoMotionEval()      (coverage x intensity x heat)
 *     displacement -> rotoMotionDisplace()  (uv offset before the line taps)
 *     width        -> rotoMotionWidthScale()
 *     copy         -> rotoMotionCopy()      (ONE extra line evaluation, or two
 *                     for Echo; only the first copy style across the two
 *                     layers runs, on the unshifted stroke)
 *   Everything defaults to off and then reproduces the pre-Anim output bit
 *   for bit (all off paths multiply by exactly 1.0 or add exactly 0.0).
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
// [RotoInk] exposure the tonemapper will apply later (see E in main()).
uniform sampler2D exposureMap;   // 1x1 auto-exposure scale
uniform float     exposure;      // > 0 manual*auto, < 0 manual only (S-Log3), 0 = already exposed

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
// x = subject mode: 0 all, 1 subject only, 2 everything except subject
//     (single Director subject, slab + ellipse), 3 target set (roto_targets),
//     4 manual depth band (y = near m, z = far m), 5 foreground (closer than
//     the subject slab), 6 background (farther), 7 screen rectangle,
//     8 screen ellipse (both from roto_subject2), 9 focus point (slab around
//     y = DoF focus distance supplied by the host)
// y = subject view depth (m), z = slab half depth (m), w = slab feather (m).
uniform vec4 roto_subject;
// screen-space shape around the subject's projected bounds (or the manual
// screen shape in modes 7/8): x,y centre (uv), z,w radii (uv).
uniform vec4 roto_subject2;
// x = ellipse amount 0..1 (0 = depth slab only), y = shape feather (fraction of
// the radius), z = 1 when the ellipse is valid this frame (subject on screen),
// w = [TronA0] subject mask source for modes 1/2/3 (CineOutlineSubjectSource):
//     0 boxes (depth slab / shapes, today's mask), 1 per-pixel avatar tag,
//     2 tag AND boxes, 3 tag OR boxes. The host uploads 0 whenever the
//     G-buffer avatar tag is not compiled in (GBUFFER_AVATAR_TAG absent).
uniform vec4 roto_subject3;
// [RotoInk Anim] x = target shape (0 ellipse, 1 box, 2 capsule), y = invert
// (modes 3..9 only), z = per-target palette colour (mode 3 only), w = target count (0..16).
uniform vec4 roto_subject4;
// [RotoInk Anim] per target (mode 3): x,y centre (uv), z,w half extents (uv)
uniform vec4 roto_targets[16];
// [RotoInk Anim] per target (mode 3): x view depth (m), y slab half depth (m,
// AABB depth half-extent + range), z palette index (0..7), w valid 0/1
uniform vec4 roto_targets2[16];
// x = sketch enable, y = boil amount px (resolution-scaled), z = detail 0..1, w = redraw fps.
uniform vec4 roto_sketch;
// x = strokes 1..4, y = roughness 0..1, z = seed, w reserved.
uniform vec4 roto_sketch2;
// LAYER 1: x = motion style (see rotoMotionEval for the list), y = speed
// (cycles/s, or cycles/beat under tempo), z = amount 0..1, w = scale (per style).
uniform vec4 roto_motion;
// x = layer-1 angle (radians), y = time (seconds, host-wrapped at 3600), z/w reserved.
uniform vec4 roto_motion2;
// [RotoInk Anim] x = layer-1 shape 0..1 (per-style meaning), y = motion seed,
// z = hold-frame stepping fps (0 = off; 12 = on twos, 8 = on threes, 6 = on fours),
// w = tempo in beats per second (BPM / 60; 0 = off -> speed is cycles/s).
uniform vec4 roto_motion3;
// [RotoInk Anim] LAYER 2: x = style, y = speed, z = amount, w = scale.
uniform vec4 roto_motion4;
// [RotoInk Anim] x = layer-2 shape, y = layer-2 angle (radians), z = tempo phase (beats), w reserved.
uniform vec4 roto_motion5;
// [RotoInk Anim] line pattern: x = type (0 none, 1 dashed, 2 dotted, 3 double,
// 4 hatch, 5 taper), y = size px (resolution-scaled), z = ratio 0..1 (dash fill /
// dot size / double stroke fraction / hatch fill / taper minimum), w = drift px/s (resolution-scaled).
uniform vec4 roto_pattern;
// [RotoInk Anim] x = hatch angle (radians), y = hatch reach px (resolution-scaled,
// 0 = hatch the line itself), z = pattern seed, w = cross-hatch 0/1.
uniform vec4 roto_pattern2;
// [RotoInk Anim] xyz = secondary colour (linear), w = colour mode (0 off,
// 1 two-tone along the edge, 2 rainbow cycle, 3 highlight = secondary where the
// motion intensity spikes, 4 heat = secondary where the motion's heat field is hot).
uniform vec4 roto_color2;
// [RotoInk Anim] x = colour speed (cycles/s), y = colour length along the edge
// px (resolution-scaled, 0 = time only), z/w reserved.
uniform vec4 roto_color2b;

// deferredUtil/gbufferUtil are linked in as separate compile units, so their
// functions must be forward-declared here.
float getDepth(vec2 pos_screen);
vec4 getNorm(vec2 screenpos);
vec4 getNormRaw(vec2 screenpos);
vec4 getPositionWithDepth(vec2 pos_screen, float depth);
// environment/srgbF.glsl (attached for every isDeferred program)
vec3 linear_to_srgb(vec3 cl);
vec3 srgb_to_linear(vec3 cs);
// defined below (subject section); used early by the polar-frame weight
float rotoTargetShape(vec2 uv, vec4 tg, int shape, float fe);

const float ROTO_PI  = 3.14159265;
const float ROTO_TAU = 6.28318531;

// 8 unit-circle tap directions (E, NE, N, NW, W, SW, S, SE)
const vec2 ROTO_DIRS[8] = vec2[8](
    vec2( 1.0,  0.0), vec2( 0.70710678,  0.70710678),
    vec2( 0.0,  1.0), vec2(-0.70710678,  0.70710678),
    vec2(-1.0,  0.0), vec2(-0.70710678, -0.70710678),
    vec2( 0.0, -1.0), vec2( 0.70710678, -0.70710678));

// [RotoInk Anim] per-target palette (mode 3 + roto_subject4.z)
const vec3 ROTO_PALETTE[8] = vec3[8](
    vec3(1.00, 0.15, 0.10), vec3(1.00, 0.50, 0.05), vec3(1.00, 0.90, 0.10), vec3(0.20, 1.00, 0.30),
    vec3(0.10, 0.90, 1.00), vec3(0.25, 0.40, 1.00), vec3(1.00, 0.20, 0.80), vec3(1.00, 1.00, 1.00));

// [RotoInk Anim] per-fragment state shared between the stages (all set in
// main() before any reader runs).
float g_width_px   = 0.0;   // effective line width in px (rotoLine / rotoMatchLight)
float g_heat       = 0.0;   // 0..1 "heat" field written by motion styles (colour mode 4)
float g_target_col = -1.0;  // palette index of the winning target (mode 3), -1 = none

float rotoWidthPx()
{
    return g_width_px;
}

// ---------------------------------------------------------------- noise ---
// Integer (PCG) hash instead of fract(sin(big)): the lattice coordinates reach
// a few thousand cells once the redraw-frame and seed offsets are added, which
// is where fp32 sin() range reduction falls apart on AMD/Intel. uint ops need
// GLSL 1.30+; every deferred program here is compiled at #version 140 or above.
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

// [RotoInk Anim] hue 0..1 -> saturated linear RGB
vec3 rotoHue(float h)
{
    vec3 k = abs(fract(h + vec3(1.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0);
    return clamp(k - 1.0, 0.0, 1.0);
}

// ---------------------------------------------------------- sketch/boil ---
// Redraw "frame" counter: the boil pattern only changes SketchFPS times per
// second (12 = on twos, 8 = on threes) regardless of the render frame rate.
// Time wrap: the host wraps time at 3600 s. Motion terms use fract(t) /
// sin(t*TAU) with t = time * speed (and fixed multipliers 0.7 / 0.9 / 1.1 /
// 1.3 / 2 / 4) and are continuous across the wrap for every UI speed
// (multiples of 0.05 -> 3600 * speed * k is an integer). The redraw counter
// below and the hashed-frame styles jump once per hour, which is one extra
// reshuffle of an already random pattern -- deliberately left.
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
// [RotoInk Anim] Motion clock in cycles for a layer with the given speed.
//   stepping (roto_motion3.z > 0): time is held for 1/fps s (on twos/threes)
//   tempo    (roto_motion3.w > 0): speed is cycles per BEAT, phase in beats
// With both off this is exactly time * speed (the pre-Anim expression).
// Known limitation: the host wraps time at 3600 s. Most periodic terms complete
// integer cycles per wrap for UI-grid values (speeds in 0.05 steps, integer
// BPM), but NOT all: tempo-scaled styles with non-integer inner multipliers
// (e.g. Jelly Wobble's x1.3 oscillator at 101 BPM / speed 0.05 = 393.9 cycles),
// Constellation twinkle and the scrolling value-noise styles can still show
// one small phase jump per hour, as can hand-typed off-grid speeds. Accepted --
// not worth a C++ per-layer phase accumulator.
float rotoMotionTime(float speed)
{
    float time = roto_motion2.y;
    if (roto_motion3.z > 0.0)
    {
        time = floor(time * roto_motion3.z) / roto_motion3.z;
    }
    if (roto_motion3.w > 0.0)
    {
        return (time * roto_motion3.w + roto_motion5.z) * speed;
    }
    return time * speed;
}

// Screen anchor for the anchor-based styles: the primary subject's projected
// centre when the host has one this frame, else the frame centre.
vec2 rotoAnchor()
{
    return (roto_subject3.z > 0.5) ? roto_subject2.xy : vec2(0.5);
}

// [RotoInk Anim] along-edge coordinates in px used by the dash / wave / spark /
// colour terms. Two frames exist and the CONSUMERS are evaluated in both and
// their outputs blended by rotoPolarWeight() -- the two coordinates are never
// mixed numerically, so the blend cannot create a seam in s:
//   tangent frame : pixel projected on the edge tangent (the original), valid
//                   anywhere, but bunches / reverses around curved outlines;
//   polar frame   : angle around the primary subject's anchor times its mean
//                   radius in px, a true contour parameter around the subject
//                   silhouette; only meaningful near the subject (an edge that
//                   points at the anchor -- floor lines, buildings -- would
//                   otherwise get a constant s and blink in unison).
float rotoEdgeCoordTan(vec2 uv, vec2 edgeDir)
{
    vec2 tng = vec2(-edgeDir.y, edgeDir.x);
    return dot(uv * screen_res, tng);
}

float rotoEdgeCoordPolar(vec2 uv)
{
    float aspect = screen_res.x / max(screen_res.y, 1.0);
    vec2  da     = (uv - roto_subject2.xy) * vec2(aspect, 1.0);
    if (dot(da, da) < 1.0e-12)
    {
        return 0.0;                        // atan(0,0) is undefined
    }
    float r_px = max(0.5 * (roto_subject2.z * screen_res.x + roto_subject2.w * screen_res.y), 8.0);
    return atan(da.y, da.x) * r_px;
}

// 1 inside the anchor ellipse, fading to 0 at ~1.5x its radius; 0 with no
// anchor or an absurd one (radius <= 0 or > 2 screens).
float rotoPolarWeight(vec2 uv)
{
    if (roto_subject3.z < 0.5)
    {
        return 0.0;
    }
    vec2 r = roto_subject2.zw;
    if (r.x <= 0.0 || r.y <= 0.0 || r.x > 2.0 || r.y > 2.0)
    {
        return 0.0;
    }
    return rotoTargetShape(uv, vec4(roto_subject2.xy, r * 1.5), 0, 0.35);
}

// styles whose look depends on the along-edge coordinate (evaluated in both frames)
bool rotoStyleUsesS(int style)
{
    return style == 6 || style == 12 || style == 15 || style == 16 || style == 17 ||
           style == 18 || style == 19 || style == 25 || style == 31;
}

// [RotoInk Anim] round-A review (6): a per-cell random frequency that still
// completes an integer number of cycles per hourly time wrap for every UI
// speed (multiples of 0.05): quantised to steps of 0.05 in [0.5, 0.95].
float rotoWrapSafeFreq(float h01)
{
    return 0.5 + floor(clamp(h01, 0.0, 0.999) * 10.0) * 0.05;
}

// Reveal progress 0..1 for the reveal styles; mode from shape:
//   < 1/3 loop, < 2/3 ping-pong, else complete in 62 % of the cycle and hold.
float rotoRevealProgress(float t, float shape)
{
    float ph = fract(t);
    if (shape < 0.3333)
    {
        return ph;
    }
    if (shape < 0.6667)
    {
        return 1.0 - abs(2.0 * ph - 1.0);
    }
    return clamp(ph * 1.6, 0.0, 1.0);
}

// Dash mask along a 0..1 phase with a duty (fill) fraction and ~1 px AA.
float rotoDash(float ph, float duty, float aa)
{
    return 1.0 - smoothstep(duty - aa, duty + aa, ph);
}

// Coverage / intensity weights for one motion layer.
//   style: 0 none | 1 pulse | 2 chase | 3 scan | 4 wave | 5 shimmer  (pre-Anim, exact)
//   6 marching ants | 7 heartbeat | 8 strobe | 9 ripple | 10 write-on (angular)
//   11 dissolve | 12 electric | 13 glitch* | 14 breathe* | 15 sparkle comets
//   16 travelling wave | 17 barber pole | 18 neon buzz | 19 morse blink
//   20 film jitter* | 21 8-bit stepped* | 22 echo aura+ | 23 shockwave
//   24 orbit dots | 25 constellation | 26 fire lick | 27 smoke wisps+
//   28 ink drip+ | 29 splatter+ | 30 lightning arcs+ | 31 laser scan
//   32 heat shimmer* | 33 jelly wobble* | 34 paint-on (directional) | 35 radial reveal
//   (* = also a displacement/width style, + = copy style; both handled in main)
//   mp = (speed, amount, scale, shape). Returns (coverage weight, intensity
//   weight); heat accumulates the style's 0..1 heat field for colour mode 4.
//   s_edge = along-edge coordinate (px) in the frame the caller wants.
vec2 rotoMotionEval(int style, vec4 mp, float angle, float seed, vec2 uv, vec2 edgeDir, float s_edge, inout float heat)
{
    if (style <= 0)
    {
        return vec2(1.0);
    }
    float amount = clamp(mp.y, 0.0, 1.0);
    float t      = rotoMotionTime(mp.x);
    float scale  = max(mp.z, 1.0e-3);

    if (style == 1) // pulse: breathes the whole line
    {
        return vec2(max(1.0 + amount * sin(t * ROTO_TAU), 0.0), 1.0);
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
        float a  = angle;
        float ss = dot(uv - 0.5, vec2(cos(a), sin(a))) + 0.5;
        float d  = abs(fract(ss - fract(t) + 0.5) - 0.5);
        w = clamp(1.0 - d / scale, 0.0, 1.0);
    }
    else if (style == 4) // wave: rotating segments keyed on edge direction
    {
        float seg = floor(scale * 12.0) + 1.0;
        w = 0.5 + 0.5 * sin(p * seg * ROTO_TAU - t * ROTO_TAU);
    }
    else if (style == 5) // shimmer: twinkling cells keyed on edge direction
    {
        float grain = max(scale * 40.0, 1.0);
        float h     = rotoHash21(vec2(floor(p * grain), 7.0));
        w = 0.5 + 0.5 * sin(t * ROTO_TAU + h * ROTO_TAU);
    }
    if (style <= 5)
    {
        return vec2(mix(1.0, w, amount), 1.0);
    }

    // ---- [RotoInk Anim] new styles -------------------------------------
    float shape  = clamp(mp.w, 0.0, 1.0);
    float rs     = screen_res.y / 1080.0;                 // px at 1080p -> px here
    float aspect = screen_res.x / max(screen_res.y, 1.0);
    vec2  p_px   = uv * screen_res;
    float s      = s_edge + seed * 37.0;                   // along-edge coordinate, px
    vec2  da     = (uv - rotoAnchor()) * vec2(aspect, 1.0);
    float pa     = (dot(da, da) < 1.0e-12) ? 0.5 : atan(da.y, da.x) / ROTO_TAU + 0.5; // angle around the anchor, 0..1
    float wc = 1.0, wi = 1.0, h = 0.0;

    if (style == 6)       // marching ants: dashes travel along the edge
    {
        float L    = mix(6.0, 64.0, scale) * rs;
        float duty = mix(0.15, 0.85, shape);
        wc = mix(1.0, rotoDash(fract(s / L - t), duty, 1.0 / L), amount);
    }
    else if (style == 7)  // heartbeat: double pulse (lub-dub)
    {
        float ph  = fract(t);
        float k   = mix(6.0, 30.0, scale);
        float gap = mix(0.1, 0.35, shape);
        float b   = exp(-ph * k) + 0.65 * exp(-max(ph - gap, 0.0) * k) * step(gap, ph);
        wc = mix(1.0, clamp(0.35 + b, 0.0, 1.0), amount);
        wi = 1.0 + 2.0 * amount * b;
        h  = b;
    }
    else if (style == 8)  // strobe: stepped random flicker / dropouts
    {
        float fps  = mix(4.0, 24.0, scale);
        float f    = floor(t * fps);
        float h1   = rotoHash21(vec2(f, 3.0 + seed));
        float h2   = rotoHash21(vec2(f, 5.0 + seed));
        float drop = step(h1, shape * 0.7);
        wc = mix(1.0, 1.0 - drop, amount);
        wi = mix(1.0, 0.6 + 0.8 * h2, amount);
    }
    else if (style == 9)  // ripple: rings radiate from the anchor through the lines
    {
        float r    = length(da);
        float lam  = mix(0.03, 0.5, scale);
        float x    = fract(r / lam - t);
        float ring = pow(0.5 + 0.5 * cos(x * ROTO_TAU), mix(1.0, 8.0, shape));
        wc = mix(1.0, ring, amount);
        wi = 1.0 + amount * ring;
        h  = ring;
    }
    else if (style == 10) // write-on: angular reveal around the anchor
    {
        float ph   = fract(pa - angle / ROTO_TAU);
        float prog = rotoRevealProgress(t, shape);
        float fe   = max(scale * 0.3, 0.005);
        wc = mix(1.0, 1.0 - smoothstep(prog, prog + fe, ph), amount);
    }
    else if (style == 11) // dissolve: animated noise threshold crawls the line in/out
    {
        float freq = mix(10.0, 120.0, scale);
        float n    = rotoVNoise(uv * vec2(aspect, 1.0) * freq + vec2(t * 2.0, t * 1.3) + seed);
        float thr  = amount * (0.5 + 0.5 * sin(t * ROTO_TAU)) * 0.95;
        float sf   = mix(0.02, 0.3, shape);
        wc = mix(1.0, smoothstep(thr - sf, thr + sf, n), min(amount * 4.0, 1.0));
    }
    else if (style == 12) // electric: high-frequency jitter + brightness spikes
    {
        float cl   = mix(2.0, 16.0, scale) * rs;
        float c    = floor(s / cl);
        float f    = floor(t * 30.0);
        float n    = rotoHash21(vec2(c, f + seed));
        float rate = mix(0.02, 0.3, shape);
        float spk  = step(1.0 - rate, rotoHash21(vec2(c * 0.37 + f, 11.0 + seed)));
        wc = mix(1.0, 0.55 + 0.45 * n, amount);
        wi = 1.0 + amount * (6.0 * spk + 1.5 * n * n);
        h  = max(spk, n * n);
    }
    else if (style == 13) // glitch: band heat for the RGB-split tint (offset in rotoMotionDisplace)
    {
        float B      = mix(6.0, 60.0, scale);
        float band   = floor(uv.y * B);
        float f      = floor(t * 8.0);
        float act = step(1.0 - amount * 0.5, rotoHash21(vec2(band, f + seed)));
        h = act * step(0.5, rotoHash21(vec2(band, f + 77.0 + seed)));
    }
    else if (style == 15) // sparkle comets: bright sparks travel along the line
    {
        float L    = mix(24.0, 200.0, scale) * rs;
        float cell = floor(s / L);
        float sp   = fract(t + rotoHash21(vec2(cell, 9.0 + seed)));
        float dd   = fract(fract(s / L) - sp);
        float tail = mix(0.05, 0.5, shape);
        float c    = clamp(1.0 - dd / tail, 0.0, 1.0);
        c *= c;
        wi = 1.0 + amount * 6.0 * c;
        h  = c;
    }
    else if (style == 16) // travelling wave: brightness/coverage wave runs along the edge
    {
        float L  = mix(10.0, 200.0, scale) * rs;
        float w0 = 0.5 + 0.5 * sin((s / L - t) * ROTO_TAU);
        w0 = pow(w0, mix(1.0, 6.0, shape));
        wc = mix(1.0, w0, amount);
        wi = 1.0 + amount * 0.5 * w0;
        h  = w0;
    }
    else if (style == 17) // barber pole: dashes with a diagonal screen component
    {
        float L    = mix(6.0, 64.0, scale) * rs;
        float diag = dot(p_px, vec2(cos(angle), sin(angle)));
        float duty = mix(0.15, 0.85, shape);
        wc = mix(1.0, rotoDash(fract((s + 0.7 * diag) / L - t), duty, 1.0 / L), amount);
    }
    else if (style == 18) // neon tube buzz: fast flicker + occasional dead segments
    {
        float buzz = 0.85 + 0.15 * rotoHash21(vec2(floor(t * 60.0), 2.0 + seed));
        float L    = mix(20.0, 200.0, scale) * rs;
        float seg  = floor(s / L);
        float dead = step(rotoHash21(vec2(seg, floor(t * 2.0) + seed)), shape * 0.4);
        wc = mix(1.0, buzz * (1.0 - 0.9 * dead), amount);
        wi = mix(1.0, buzz, amount);
    }
    else if (style == 19) // morse blink: dot/dash segments blinking along the edge
    {
        float L     = mix(8.0, 80.0, scale) * rs;
        float seg   = floor(s / L);
        float along = fract(s / L);
        float dash  = step(0.4, rotoHash21(vec2(seg, 4.0 + seed)));
        float fill  = mix(step(along, 0.3), step(along, 0.8), dash);
        float on    = step(shape * 0.6, rotoHash21(vec2(seg, floor(t) + seed)));
        wc = mix(1.0, fill * on, amount);
    }
    else if (style == 20) // film jitter: brightness flutter (offset in rotoMotionDisplace)
    {
        float f = floor(t * mix(12.0, 24.0, scale));
        wi = mix(1.0, 0.85 + 0.15 * rotoHash21(vec2(f, 6.0 + seed)), amount);
    }
    else if (style == 23) // shockwave: one ring expands from the anchor
    {
        float r    = length(da);
        float R    = fract(t) * mix(0.3, 1.5, scale);
        float wdt  = 0.02 + 0.1 * shape;
        float q    = (r - R) / wdt;
        float ring = exp(-q * q) * (1.0 - fract(t));   // q*q, never pow() of a negative base
        wc = mix(1.0, clamp(0.3 + ring, 0.0, 1.0), amount);
        wi = 1.0 + 3.0 * amount * ring;
        h  = ring;
    }
    else if (style == 24) // orbit dots: N dots circle the silhouette
    {
        float N  = floor(scale * 12.0) + 1.0;
        float dd = fract(pa * N - t);
        float df = mix(0.05, 0.4, shape);
        float dt = 1.0 - smoothstep(df * 0.7, df, dd);
        wc = mix(1.0, dt, amount);
        wi = 1.0 + amount * dt;
        h  = dt;
    }
    else if (style == 25) // constellation: twinkling dots along the line, faint connections
    {
        float L    = mix(10.0, 80.0, scale) * rs;
        float cell = floor(s / L);
        float ph   = fract(s / L) - 0.5;
        float dr   = mix(0.08, 0.4, shape);
        float dt   = 1.0 - smoothstep(dr - 1.0 / L, dr, abs(ph));
        float hc   = rotoHash21(vec2(cell, 13.0 + seed));
        float tw   = 0.5 + 0.5 * sin(t * ROTO_TAU * rotoWrapSafeFreq(hc) + hc * ROTO_TAU);
        wc = max(1.0 - amount, dt * (0.4 + 0.6 * tw));
        wi = 1.0 + dt * tw * amount;
        h  = dt * tw;
    }
    else if (style == 26) // fire lick: upward-flowing noise on upward-facing edges
    {
        float up    = clamp(edgeDir.y, 0.0, 1.0);
        float L     = mix(8.0, 60.0, scale) * rs;
        float n     = rotoVNoise(p_px / L + vec2(seed, -t * 4.0));
        float flame = n * up;
        wc = mix(1.0, 0.3 + 0.7 * n, amount * up);
        wi = 1.0 + amount * 2.0 * flame;
        h  = flame;
    }
    else if (style == 31) // laser scan: sweep bar with sparks
    {
        float ss  = dot(uv - 0.5, vec2(cos(angle), sin(angle))) + 0.5;
        float d   = abs(fract(ss - fract(t) + 0.5) - 0.5);
        float bar = clamp(1.0 - d / scale, 0.0, 1.0);
        float spk = pow(rotoHash21(vec2(floor(s / (4.0 * rs)), floor(t * 30.0) + seed)), 16.0) * bar;
        wc = mix(1.0, bar, amount);
        wi = 1.0 + amount * (2.0 * bar + 8.0 * spk * mix(0.0, 1.0, shape));
        h  = bar;
    }
    else if (style == 34) // paint-on: directional reveal (angle = travel direction)
    {
        float ss   = dot(uv - 0.5, vec2(cos(angle), sin(angle))) * 0.7071 + 0.5;
        float prog = rotoRevealProgress(t, shape);
        float fe   = max(scale * 0.3, 0.005);
        wc = mix(1.0, 1.0 - smoothstep(prog, prog + fe, ss), amount);
    }
    else if (style == 35) // radial reveal from the anchor outward
    {
        float r    = length(da) / 1.2;
        float prog = rotoRevealProgress(t, shape);
        float fe   = max(scale * 0.3, 0.005);
        wc = mix(1.0, 1.0 - smoothstep(prog, prog + fe, r), amount);
    }
    // styles 14, 21, 22, 27, 28, 29, 30, 32, 33: weights stay 1 (handled elsewhere)

    heat = max(heat, h);
    return vec2(wc, wi);
}

// [RotoInk Anim] uv offset applied BEFORE the line taps for the displacement
// styles (13 glitch, 20 film jitter, 21 8-bit, 32 heat shimmer, 33 jelly).
vec2 rotoMotionDisplace(int style, vec4 mp, float angle, float seed, vec2 uv)
{
    if (style != 13 && style != 20 && style != 21 && style != 32 && style != 33)
    {
        return vec2(0.0);
    }
    float amount = clamp(mp.y, 0.0, 1.0);
    float t      = rotoMotionTime(mp.x);
    float scale  = max(mp.z, 1.0e-3);
    float shape  = clamp(mp.w, 0.0, 1.0);
    float rs     = screen_res.y / 1080.0;

    if (style == 13)      // glitch: horizontal band offsets
    {
        float B      = mix(6.0, 60.0, scale);
        float band   = floor(uv.y * B);
        float f      = floor(t * 8.0);
        float act = step(1.0 - amount * 0.5, rotoHash21(vec2(band, f + seed)));
        float off = (rotoHash21(vec2(band, f + 33.0 + seed)) - 0.5) * 2.0 * mix(4.0, 40.0, shape) * rs * act;
        return vec2(off / screen_res.x, 0.0);
    }
    if (style == 20)      // film jitter: per-frame gate weave with occasional held frame
    {
        float f = floor(t * mix(12.0, 24.0, scale));
        if (rotoHash21(vec2(f, 8.0 + seed)) < shape * 0.3)
        {
            f -= 1.0;   // hold the previous frame's position
        }
        vec2 j = (vec2(rotoHash21(vec2(f, 1.0 + seed)), rotoHash21(vec2(f, 2.0 + seed))) - 0.5) * amount * 6.0 * rs;
        return j / screen_res;
    }
    if (style == 21)      // 8-bit: quantise the line to a pixel grid
    {
        float g = mix(2.0, 12.0, scale) * rs;
        vec2  q = (floor(uv * screen_res / g) + 0.5) * g / screen_res;
        return (q - uv) * amount;
    }
    if (style == 32)      // heat shimmer: low-frequency wobble
    {
        float L  = mix(20.0, 200.0, scale) * rs;
        vec2  P  = uv * screen_res / L;
        float nx = rotoVNoise(P + vec2(t * 1.3, t * 0.7) + seed);
        float ny = rotoVNoise(P + vec2(-t * 0.9, t * 1.1) + 19.0 + seed);
        return (vec2(nx, ny) - 0.5) * amount * 8.0 * rs / screen_res;
    }
    // 33 jelly wobble: elastic sway keyed on screen position
    float k = mix(1.0, 6.0, scale);
    float A = amount * 10.0 * rs;
    vec2  o = vec2(A * sin(ROTO_TAU * (t + uv.y * k)), 0.5 * A * cos(ROTO_TAU * (t * 1.3 + uv.x * k)));
    return o / screen_res;
}

// [RotoInk Anim] width multiplier for style 14 (breathe); 1.0 otherwise.
float rotoMotionWidthScale(int style, vec4 mp)
{
    if (style != 14)
    {
        return 1.0;
    }
    float amount = clamp(mp.y, 0.0, 1.0);
    float t      = rotoMotionTime(mp.x);
    float scale  = clamp(mp.z, 0.0, 1.0);
    float shape  = clamp(mp.w, 0.0, 1.0);
    float wave   = (shape < 0.5) ? 0.5 + 0.5 * sin(t * ROTO_TAU) : exp(-fract(t) * 4.0);
    return 1.0 + amount * (1.0 - scale) * (2.0 * wave - 1.0);
}

// [RotoInk Anim] copy styles: ONE extra line evaluation (two for echo).
//   combine: 0 = copy alone, 1 = copy - main line (zone beyond the line),
//            2 = second - first (a shell between two widths)
//   gate:    1 = keep only where the copy's outward direction points down
// Returns false when the style is not a copy style.
bool rotoMotionCopy(int style, vec4 mp, float seed, vec2 uv, float base_w,
                    out vec2 off_uv, out float width_px, out float width2_px,
                    out float fade, out float boost, out int combine, out int gate)
{
    off_uv = vec2(0.0); width_px = base_w; width2_px = -1.0; fade = 0.0; boost = 0.0; combine = 0; gate = 0;
    if (style != 22 && style != 27 && style != 28 && style != 29 && style != 30)
    {
        return false;
    }
    float amount = clamp(mp.y, 0.0, 1.0);
    float t      = rotoMotionTime(mp.x);
    float scale  = max(mp.z, 1.0e-3);
    float shape  = clamp(mp.w, 0.0, 1.0);
    float rs     = screen_res.y / 1080.0;
    vec2  p_px   = uv * screen_res;

    if (style == 22)      // echo aura: a concentric contour; shape >= 0.5 expands like a shockwave
    {
        float g  = mix(2.0, 32.0, scale) * rs;
        float ex = (shape >= 0.5) ? fract(t) : 0.0;
        g *= 1.0 + 2.0 * ex;
        width_px  = base_w + g;
        width2_px = base_w + g + max(base_w, 1.0);
        fade      = amount * (1.0 - ex);
        combine   = 2;
    }
    else if (style == 27) // smoke wisps: a rising, widening, fading copy
    {
        float ph   = fract(t);
        float rise = ph * mix(20.0, 160.0, scale) * rs;
        float nx   = (rotoVNoise(vec2(p_px.x / (40.0 * rs), t * 2.0 + seed)) - 0.5) * 24.0 * rs * ph;
        off_uv   = vec2(nx, -rise) / screen_res;      // sample below -> copy appears above
        width_px = base_w * (1.0 + 2.0 * ph * shape);
        fade     = amount * (1.0 - ph) * (1.0 - ph);
    }
    else if (style == 28) // ink drip: columns run down from lower-facing edges
    {
        float colw = mix(4.0, 24.0, scale) * rs;
        float col  = floor(p_px.x / colw + seed);
        float hc   = rotoHash21(vec2(col, 21.0));
        float drip = step(1.0 - shape, rotoHash21(vec2(col, 22.0)));
        float ph   = fract(t + hc * 0.5);
        float d    = ph * 60.0 * rs * (0.5 + hc) * drip;
        off_uv   = vec2(0.0, d / screen_res.y);      // sample above -> copy appears below
        width_px = base_w + d;                        // band spans the whole drip length
        fade     = amount * (1.0 - smoothstep(0.75, 1.0, ph));
        gate     = 1;
    }
    else if (style == 29) // splatter: wide blobs pop in random cells
    {
        float L      = mix(12.0, 80.0, scale) * rs;
        vec2  cell   = floor(p_px / L);
        float f      = floor(t * 4.0);
        float act = step(1.0 - shape * 0.5, rotoHash21(cell + vec2(f * 131.0, seed)));
        width_px = base_w + 0.5 * L;
        fade     = amount * act * (0.6 + 0.4 * rotoHash21(cell + vec2(f * 7.0, 3.0 + seed)));
        combine  = 1;
    }
    else                  // 30 lightning arcs: jagged bright branches flicker near the edge
    {
        float reach = mix(6.0, 48.0, scale) * rs;
        float f     = floor(t * 24.0);
        float seg   = floor(dot(p_px, vec2(0.7, 0.7)) / (60.0 * rs));
        float flash = step(1.0 - shape * 0.5, rotoHash21(vec2(seg, f + seed)));
        float jag   = rotoVNoise(p_px / (5.0 * rs) + vec2(f * 13.0, seed));
        float arc   = flash * step(0.62, jag);
        width_px = base_w + reach;
        fade     = amount * arc;
        boost    = 4.0 * arc;
        combine  = 1;
    }
    return true;
}

// [RotoInk Anim] line patterns along the edge (1 dashed, 2 dotted); 1.0 otherwise.
float rotoPatternMask(int type, float s)
{
    if (type != 1 && type != 2)
    {
        return 1.0;
    }
    float L     = max(roto_pattern.y, 1.0);
    float ratio = clamp(roto_pattern.z, 0.02, 0.98);
    // Drift as cycles of the dash period per second, quantised to 0.05 steps
    // so the pattern completes integer cycles across the hourly time wrap.
    float dcyc  = floor(roto_pattern.w / L * 20.0 + 0.5) * 0.05;
    float sd    = s + dcyc * roto_motion2.y * L + roto_pattern2.z * 7.3;
    if (type == 1)
    {
        return rotoDash(fract(sd / L), ratio, 1.0 / L);
    }
    float along = (fract(sd / L) - 0.5) * L;          // px from the dot centre
    float rad   = ratio * L * 0.5;
    return 1.0 - smoothstep(rad - 1.0, rad + 1.0, abs(along));
}

// [RotoInk Anim] hatch stripes in screen space (pattern 4).
float rotoHatchMask(vec2 p_px)
{
    float L     = max(roto_pattern.y, 1.0);
    float ratio = clamp(roto_pattern.z, 0.02, 0.98);
    float aa    = 1.0 / L;
    vec2  hd    = vec2(cos(roto_pattern2.x), sin(roto_pattern2.x));
    float m     = rotoDash(fract(dot(p_px, hd) / L + roto_pattern2.z), ratio, aa);
    if (roto_pattern2.w > 0.5)
    {
        m = max(m, rotoDash(fract(dot(p_px, vec2(-hd.y, hd.x)) / L + roto_pattern2.z), ratio, aa));
    }
    return m;
}

// ------------------------------------------------------------- line taps ---
// One ring of 8 taps at radius r_px around uv. Accumulates the outward
// silhouette direction into dir_acc and the crease direction into cdir_acc.
// cr_lo/cr_hi passed in are already scaled for this ring's radius (see
// rotoCreaseBand), so wide rings only dilate creases sharper than the
// curvature a smooth surface shows over that radius.
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

// Crease threshold band for a ring of radius r_px. A smooth surface of
// curvature k turns its normal by ~k*r over r px; the crease ANGLE threshold
// is scaled by sqrt(r / crease_px) (crease_px = 1 px at 1080p, resolution-
// scaled) and clamped below 90 deg -- the compromise between curvature
// immunity (linear) and letting wide presets dilate ordinary 45-60 deg
// creases to ~2-3 px. The fixed 1-px crease ring (rotoLine) always detects
// creases at the authored angle; the width rings only dilate those sharp
// enough to survive the scaled threshold.
void rotoCreaseBand(float c_ang, float r_px, float crease_px, float soft,
                    out float cr_lo, out float cr_hi)
{
    float ang   = min(c_ang * sqrt(max(r_px / crease_px, 1.0)), 1.55);
    float c_thr = max(1.0 - cos(ang), 1.0e-5);
    cr_lo = c_thr * (1.0 - 0.5 * soft);
    cr_hi = c_thr * (1.0 + 0.5 * soft);
}

// Full line evaluation at one (possibly boil-shifted) uv, at width rotoWidthPx().
// Returns x = line coverage, yz = edge direction (unnormalised): the outward
// silhouette direction when there is one, else the crease direction, else the
// screen-space projection of the surface normal.
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

    // crease angle (from the uploaded 1 - cos) and the fixed crease test
    // radius: 1 px at 1080p, scaled to the current resolution.
    float c_ang     = acos(clamp(1.0 - max(roto_line.y, 1.0e-5), -1.0, 1.0));
    float crease_px = max(1.0, floor(screen_res.y / 1080.0 + 0.5));
    float wC        = clamp(roto_line.w, 0.0, 1.0);

    float width = rotoWidthPx();
    float r0    = floor(width);
    float wf    = width - r0;

    float sil0 = 0.0, cr0 = 0.0, sil1 = 0.0, cr1 = 0.0, crf = 0.0, silf = 0.0;
    vec2  dir  = vec2(0.0);
    vec2  cdir = vec2(0.0);
    float cr_lo, cr_hi;

    // The fixed 1-px crease term crf is ALWAYS its own term. When the r0 ring
    // sits at the same radius its (unscaled, scale = 1) crease result is reused
    // verbatim -- never through the fractional-width mix -- so coverage stays
    // continuous across widths 1.0 -> 1.9 -> 2.0. crf is faded by
    // min(width / crease_px, 1) so sub-pixel widths fade creases exactly like
    // the AA-scaled silhouette; width 0 -> no line.
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
// [RotoInk Anim] screen shape weight for a target (x,y centre, z,w half
// extents, uv). shape 0 ellipse, 1 box, 2 vertical capsule. fe = feather as a
// fraction of the radius.
float rotoTargetShape(vec2 uv, vec4 tg, int shape, float fe)
{
    vec2 radius = max(tg.zw, vec2(1.0e-3));
    if (shape == 1)
    {
        vec2  q = abs(uv - tg.xy) / radius;
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

float rotoDepthSlab(float z_c, float fz, float half_d, float feather)
{
    return 1.0 - smoothstep(half_d, half_d + feather, abs(z_c - fz));
}

// [TronA0] Per-pixel avatar tag read from the RAW G-buffer normal .w. Only
// meaningful when the preamble emitted GBUFFER_AVATAR_TAG (HDR / RGBA16
// normals with RenderGBufferAvatarTag on); otherwise the macro does not exist
// and this is a literal 0.0 -- the host also uploads source 0 in that case.
float rotoAvatarTag(vec2 uv)
{
#ifdef GBUFFER_AVATAR_TAG
    return GBUFFER_AVATAR_TAG_OF(getNormRaw(uv).w) ? 1.0 : 0.0;
#else
    return 0.0;
#endif
}

// [TronA0] Combine the geometric ("boxes": depth slab / ellipse / target
// shapes) subject mask with the per-pixel avatar tag per
// CineOutlineSubjectSource (roto_subject3.w). Source 0 returns the box mask
// untouched, so the default path is exactly today's mask.
float rotoApplySubjectSource(float m_box, vec2 uv)
{
    int source = int(roto_subject3.w + 0.5);
    if (source <= 0)
    {
        return m_box;
    }
    float tag = rotoAvatarTag(uv);
    if (source == 1)        // tag only: every tagged avatar pixel
    {
        return tag;
    }
    if (source == 2)        // tag AND boxes: which avatar, pixel-exact
    {
        return tag * m_box;
    }
    return max(tag, m_box); // 3 tag OR boxes: re-admits alpha clothing via the box
}

float rotoSubjectMask(vec2 uv, float z_c)
{
    int mode = int(roto_subject.x + 0.5);
    if (mode <= 0)
    {
        return 1.0;
    }
    float half_d  = max(roto_subject.z, 0.0);
    float feather = max(roto_subject.w, 1.0e-3);

    if (mode <= 2)
    {
        // Single Director subject: depth slab, optionally tightened by the
        // projected-bounds ellipse (pre-Anim path, kept verbatim).
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
        m = rotoApplySubjectSource(m, uv);   // [TronA0] identity when source 0
        return (mode == 2) ? 1.0 - m : m;
    }

    // ---- [RotoInk Anim] extended modes --------------------------------
    float fe = max(roto_subject3.y, 0.01);
    float m  = 0.0;
    if (mode == 3)          // target set: union over up to 16 targets
    {
        int   count  = int(roto_subject4.w + 0.5);
        int   shape  = int(roto_subject4.x + 0.5);
        float amount = clamp(roto_subject3.x, 0.0, 1.0);
        for (int i = 0; i < 16; ++i)
        {
            if (i >= count)
            {
                break;
            }
            vec4 t2 = roto_targets2[i];
            if (t2.w < 0.5)
            {
                continue;
            }
            float w = rotoDepthSlab(z_c, t2.x, max(t2.y, 0.0), feather);
            w *= mix(1.0, rotoTargetShape(uv, roto_targets[i], shape, fe), amount);
            if (w > m)
            {
                m = w;
                if (roto_subject4.z > 0.5)
                {
                    g_target_col = t2.z;
                }
            }
        }
        m = rotoApplySubjectSource(m, uv);   // [TronA0] identity when source 0
    }
    else if (mode == 4)     // manual depth band: y = near, z = far (m)
    {
        m = smoothstep(roto_subject.y - feather, roto_subject.y, z_c) *
            (1.0 - smoothstep(roto_subject.z, roto_subject.z + feather, z_c));
    }
    else if (mode == 5)     // foreground: closer than the subject slab
    {
        m = 1.0 - smoothstep(roto_subject.y - half_d - feather, roto_subject.y - half_d, z_c);
    }
    else if (mode == 6)     // background: farther than the subject slab
    {
        m = smoothstep(roto_subject.y + half_d, roto_subject.y + half_d + feather, z_c);
    }
    else if (mode == 7)     // screen rectangle (uv, from roto_subject2)
    {
        m = rotoTargetShape(uv, roto_subject2, 1, fe);
    }
    else if (mode == 8)     // screen ellipse (uv, from roto_subject2)
    {
        m = rotoTargetShape(uv, roto_subject2, 0, fe);
    }
    else                    // 9 focus point: slab around the host-supplied DoF distance
    {
        m = rotoDepthSlab(z_c, roto_subject.y, half_d, feather);
    }
    if (roto_subject4.y > 0.5)
    {
        m = 1.0 - m;
    }
    return m;
}

// ------------------------------------------------------------- ink colour ---
// Match light: brightest HDR scene colour just OUTSIDE the edge (along the
// outward edge direction), normalised to unit peak so only the hue is kept.
// The normalisation is faded out where the sampled surroundings are dark in
// EXPOSED terms (E = effective exposure), toward `fallback` (the fixed ink
// colour for normal/multiply, black for add), so near-black backgrounds never
// turn into full-intensity white ink. `display` = rotoScene is display (sRGB)
// encoded; every sample is decoded to linear first so the hue, the brightness
// weighting and the dark threshold all work in linear, and the result is
// ALWAYS a linear unit-peak hue (main() encodes it exactly once).
vec3 rotoMatchLight(vec2 uv, vec2 edgeDir, float E, vec3 fallback, bool display)
{
    float reach = max(roto_ink.y, 1.0);
    float width = rotoWidthPx();
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
    // With no boil every stroke would evaluate the identical line, so
    // collapse to one stroke (roughness grain still applies).
    int   strokes = (boil_px > 0.0) ? int(clamp(roto_sketch2.x, 1.0, 4.0) + 0.5) : 1;
    // [RotoInk Anim] round-A review (4): the Double pattern (+2 evaluations)
    // and the hatch shading zone (+1) would multiply with the boil strokes
    // (4 strokes -> 12-14 evaluations at 4K), and a double line only reads as
    // one stroke anyway, so collapse to the single (still boil-wobbled)
    // stroke 0 whenever either is active.
    int   ptype0 = int(roto_pattern.x + 0.5);
    if (ptype0 == 3 || (ptype0 == 4 && roto_pattern2.y > 0.5))
    {
        strokes = 1;
    }

    // ---- [RotoInk Anim] motion layers --------------------------------
    int   style1 = int(roto_motion.x + 0.5);
    int   style2 = int(roto_motion4.x + 0.5);
    vec4  mp1    = vec4(roto_motion.y, roto_motion.z, roto_motion.w, roto_motion3.x);
    vec4  mp2    = vec4(roto_motion4.y, roto_motion4.z, roto_motion4.w, roto_motion5.x);
    float ang1   = roto_motion2.x;
    float ang2   = roto_motion5.y;
    float mseed  = roto_motion3.y;
    int   ptype  = int(roto_pattern.x + 0.5);
    float rs     = screen_res.y / 1080.0;
    float aspect = screen_res.x / max(screen_res.y, 1.0);
    vec2  p_px   = uv * screen_res;

    // displacement (uv offset before the taps) and width scale
    vec2  disp   = rotoMotionDisplace(style1, mp1, ang1, mseed, uv) +
                   rotoMotionDisplace(style2, mp2, ang2, mseed + 5.0, uv);
    float wscale = rotoMotionWidthScale(style1, mp1) * rotoMotionWidthScale(style2, mp2);
    if (ptype == 5)   // taper: brush-pressure width variation from low-frequency noise
    {
        float L = max(roto_pattern.y, 1.0);
        float n = rotoVNoise(p_px / L + roto_pattern2.z * vec2(3.1, 7.7));
        wscale *= mix(clamp(roto_pattern.z, 0.05, 1.0), 1.0, n);
    }
    float base_w = clamp(outline_params.x * wscale, 0.0, 24.0);
    g_width_px = base_w;
    vec2 uv_l = uv + disp;

    // copy style (first across the two layers)
    vec2  c_off; float c_w, c_w2, c_fade, c_boost; int c_comb, c_gate;
    bool  has_copy = rotoMotionCopy(style1, mp1, mseed, uv, base_w, c_off, c_w, c_w2, c_fade, c_boost, c_comb, c_gate);
    if (!has_copy)
    {
        has_copy = rotoMotionCopy(style2, mp2, mseed + 5.0, uv, base_w, c_off, c_w, c_w2, c_fade, c_boost, c_comb, c_gate);
    }
    if (has_copy && c_fade <= 1.0e-4)
    {
        has_copy = false;   // inactive this frame/cell: skip the extra taps
    }

    // Fast path: nothing to draw on sky / atmo-skipped pixels unless the boil
    // or a displacement / copy can pull a neighbouring line onto them.
    float raw_depth = getDepth(uv);
    vec4  norm_raw  = getNormRaw(uv);
    bool  is_sky    = raw_depth >= 0.999999 ||
                      GET_GBUFFER_FLAG(norm_raw.w, GBUFFER_FLAG_SKIP_ATMOS);
    bool  can_shift = boil_px > 0.0 || dot(disp, disp) > 0.0 || has_copy;
    if (is_sky && !can_shift)
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
    float line  = 0.0;
    float extra = 0.0;    // copy / hatch-zone coverage (added, not part of the pattern masks)
    vec2  dir   = vec2(0.0);
    float f_dbl = clamp(roto_pattern.z, 0.1, 0.45);
    for (int s = 0; s < 4; ++s)
    {
        if (s >= strokes)
        {
            break;
        }
        vec2 off = (boil_px > 0.0)
            ? rotoSketchOffset(uv, float(s)) * boil_px / screen_res
            : vec2(0.0);
        vec2 uvs = clamp(uv_l + off, vec2(0.0), vec2(1.0));
        vec3 r   = rotoLine(uvs);
        float ls = r.x;

        if (ptype == 3)   // double line: inner stroke + outer stroke with a gap
        {
            g_width_px = base_w * f_dbl;
            float l_in  = rotoLine(uvs).x;
            g_width_px = base_w * (1.0 - f_dbl);
            float l_mid = rotoLine(uvs).x;
            g_width_px = base_w;
            ls = max(l_in, clamp(r.x - l_mid, 0.0, 1.0));
        }
        else if (ptype == 4 && roto_pattern2.y > 0.5)   // hatch zone beyond the line
        {
            g_width_px = min(base_w + roto_pattern2.y, 96.0);
            float l_wide = rotoLine(uvs).x;
            g_width_px = base_w;
            extra = max(extra, clamp(l_wide - r.x, 0.0, 1.0) * rotoHatchMask(p_px) * 0.85);
        }

        if (has_copy && s == 0)   // copy styles run on the unshifted stroke only
        {
            vec2 uvc = clamp(uvs + c_off, vec2(0.0), vec2(1.0));
            g_width_px = min(c_w, 96.0);
            vec3  rc   = rotoLine(uvc);
            float lc   = rc.x;
            if (c_comb == 2)
            {
                g_width_px = min(c_w2, 96.0);
                lc = clamp(rotoLine(uvc).x - lc, 0.0, 1.0);
            }
            else if (c_comb == 1)
            {
                lc = clamp(lc - r.x, 0.0, 1.0);
            }
            g_width_px = base_w;
            if (c_gate == 1)
            {
                vec2  cd = rc.yz;
                float cl = length(cd);
                lc *= (cl > 1.0e-8) ? 1.0 - smoothstep(-0.3, -0.1, cd.y / cl) : 0.0;   // edge0 < edge1
            }
            extra = max(extra, lc * c_fade);
            if (lc > 0.0)
            {
                g_heat = max(g_heat, c_boost > 0.0 ? lc : 0.0);
            }
        }

        line = max(line, ls);
        dir += r.yz;
    }

    // Pencil roughness: grain breaks the line for a dry-media feel.
    float rough = sketch ? clamp(roto_sketch2.y, 0.0, 1.0) : 0.0;
    if (rough > 0.0 && line > 0.0)
    {
        float gfreq  = mix(260.0, 900.0, clamp(roto_sketch.z, 0.0, 1.0));
        float grain  = rotoVNoise(uv * vec2(aspect, 1.0) * gfreq
                                  + rotoSketchFrame() * 3.7 + roto_sketch2.z * 11.3);
        float thr    = rough * 0.9;
        float keep   = smoothstep(thr - 0.15, thr + 0.15, grain);
        line *= mix(1.0, keep, rough);
    }

    if (line <= 1.0e-4 && extra <= 1.0e-4)
    {
        frag_color = scene;
        return;
    }

    float dl = length(dir);
    vec2 edgeDir = (dl > 1.0e-8) ? dir / dl : vec2(1.0, 0.0);
    // Along-edge coordinates: tangent frame everywhere, polar frame blended in
    // near the subject anchor (k_pol). Consumers are evaluated in both frames
    // and their OUTPUTS mixed, never the coordinates.
    float s_tan = rotoEdgeCoordTan(uv, edgeDir);
    float k_pol = rotoPolarWeight(uv);
    float s_pol = (k_pol > 0.0) ? rotoEdgeCoordPolar(uv) : s_tan;

    // ---- [RotoInk Anim] line pattern -----------------------------------
    if (ptype == 1 || ptype == 2)
    {
        float pm = rotoPatternMask(ptype, s_tan);
        if (k_pol > 0.0)
        {
            pm = mix(pm, rotoPatternMask(ptype, s_pol), k_pol);
        }
        line *= pm;
    }
    else if (ptype == 4 && roto_pattern2.y <= 0.5)
    {
        line *= rotoHatchMask(p_px);   // hatch the line band itself
    }
    line = max(line, extra);

    // ---- motion weights (both layers multiply) ----------------------------
    float heat1 = 0.0, heat2 = 0.0;
    vec2 w1 = rotoMotionEval(style1, mp1, ang1, mseed, uv, edgeDir, s_tan, heat1);
    if (k_pol > 0.0 && rotoStyleUsesS(style1))
    {
        float hp = 0.0;
        vec2  wp = rotoMotionEval(style1, mp1, ang1, mseed, uv, edgeDir, s_pol, hp);
        w1    = mix(w1, wp, k_pol);
        heat1 = mix(heat1, hp, k_pol);
    }
    vec2 w2 = rotoMotionEval(style2, mp2, ang2, mseed + 5.0, uv, edgeDir, s_tan, heat2);
    if (k_pol > 0.0 && rotoStyleUsesS(style2))
    {
        float hp = 0.0;
        vec2  wp = rotoMotionEval(style2, mp2, ang2, mseed + 5.0, uv, edgeDir, s_pol, hp);
        w2    = mix(w2, wp, k_pol);
        heat2 = mix(heat2, hp, k_pol);
    }
    g_heat = max(g_heat, max(heat1, heat2));
    float wcov = w1.x * w2.x;
    float wint = w1.y * w2.y * (1.0 + c_boost * (has_copy ? 1.0 : 0.0));

    float cov = line * M * clamp(roto_ink.x, 0.0, 1.0) * wcov;
    cov = clamp(cov, 0.0, 1.0);

    int blend = int(outline_params2.w + 0.5);

    // Effective exposure the tonemapper applies downstream (tonemapUtilF.glsl
    // applyExposure):
    //   exposure > 0 : E = exposureMap * exposure   (manual * auto adaptation)
    //   exposure < 0 : E = -exposure                (manual iris ONLY: mirrors
    //                  tonemap_type == 8 / S-Log3 capture, which forces the
    //                  auto scale to 1.0; the host sends the negated manual value)
    //   exposure == 0: E = 1                        (non-HDR post-tonemap site)
    // Floored at 1/1024 (the S-Log3 manual iris can reach 0.5 * exp2(-8)).
    // Worst case 64 intensity * 1024 = 65536 -> the final 65000 clamp below
    // keeps the RGBA16F write finite.
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

    // Display-encoded destination (Overlay layer / non-HDR site write into
    // postPingMap AFTER colorCorrect). Keyed on roto_ink.z from the host, NOT
    // on exposure == 0 (which also occurs on the linear mRT->screen under
    // legacy gamma).
    bool display = roto_ink.z > 0.5;

    // Ink colour is built in LINEAR (fixed colour, or a linear unit-peak hue
    // from match-light -- which decodes its display-encoded samples itself).
    vec3 fallback = (blend == 2) ? vec3(0.0) : outline_color;
    vec3 ink = (outline_params2.z > 0.5) ? rotoMatchLight(uv, edgeDir, E, fallback, display) : outline_color;

    // ---- [RotoInk Anim] per-target palette + colour modes ----------------
    if (g_target_col >= 0.0)
    {
        ink = ROTO_PALETTE[int(mod(g_target_col, 8.0))];
    }
    int cmode = int(roto_color2.w + 0.5);
    if (cmode != 0)
    {
        vec3  col2  = roto_color2.xyz;
        float tc    = roto_motion2.y * roto_color2b.x;
        float len   = roto_color2b.y;
        // along-edge phase in both frames, results blended by k_pol
        float along  = (len > 0.5) ? s_tan / len : 0.0;
        float alongP = (len > 0.5) ? s_pol / len : 0.0;
        float kc     = (len > 0.5) ? k_pol : 0.0;
        if (cmode == 1)        // two-tone gradient along the edge, drifting
        {
            float f = 0.5 + 0.5 * sin((along - tc) * ROTO_TAU);
            if (kc > 0.0)
            {
                f = mix(f, 0.5 + 0.5 * sin((alongP - tc) * ROTO_TAU), kc);
            }
            ink = mix(ink, col2, f);
        }
        else if (cmode == 2)   // rainbow cycle
        {
            vec3 rb = rotoHue(fract(tc + along));
            if (kc > 0.0)
            {
                rb = mix(rb, rotoHue(fract(tc + alongP)), kc);
            }
            ink = rb;
        }
        else if (cmode == 3)   // highlight: secondary where the motion spikes
        {
            ink = mix(ink, col2, clamp(wint - 1.0, 0.0, 1.0));
        }
        else                   // 4 heat: secondary where the motion's heat field is hot
        {
            ink = mix(ink, col2, clamp(g_heat, 0.0, 1.0));
        }
    }

    ink *= max(outline_params.w, 0.0);
    ink *= wint;
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

    // The target is RGBA16F (max ~65504); an inf here would poison the bloom
    // pyramid, so clamp before writing.
    outc = min(outc, vec3(65000.0));
    float out_a = min(scene.a + cov * clamp(outline_params2.x, 0.0, 1.0), 65000.0);
    frag_color = vec4(outc, out_a);
}
