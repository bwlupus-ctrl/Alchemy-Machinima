/**
 * @file rimGlowMaskF.glsl
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Machinima Viewer Source Code
 * Copyright (C) 2026, Alchemy Machinima
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * $/LicenseInfo$
 */

/*[EXTRA_CODE_HERE]*/

// Virtual Cinema Rim Glow -- Phase-1 MASK pass (Auto Rim only).
//
// Native port of the ReShade VirtualCinema_RimGlow "Auto Rim" gather onto the
// linear HDR scene + real G-buffer normals. Runs full-res, one fragment per
// screen pixel, MRT x4 (attachment order is the shared interface contract):
//   frag_data[0] mask     RGBA16F : rgb = rim radiance * edge (premultiplied), a = edge coverage
//   frag_data[1] subj     R16F    : subject focus mask M
//   frag_data[2] radHist  RGBA16F : rgb = PRE-shoulder scene-linear rim radiance
//                                    (luminance-capped at Ycap/E), a = metric view depth (m)
//   frag_data[3] dir      RG16F   : gDir * edge * strength (premultiplied, signed)
//
// Reads: diffuseRect = mRT->screen (linear HDR scene, pre-exposure), normalMap,
// depthMap (through deferredUtil), exposureMap, rimRadHist = previous frame's
// attachment-2 (ping-pong copy owned by the pipeline).
//
// Units: every brightness threshold is in EXPOSED-linear units (scene * E),
// E = exposure * exp_scale mirrors tonemapUtilF.glsl applyExposure() exactly
// (rim_exposure_lock reproduces the tonemap_type==8 S-Log3 exception). All
// depth gates are RELATIVE metric depth (dz / z) so tuning is scale-invariant.
//
// Per-fragment order (B4 scheme): gather -> capLum -> settle (linear, E-free
// history) -> shoulder(E) -> crosstalk/saturation -> * edge. Off-edge pixels
// carry the history forward with release-rate decay + the same rejections (B3).

out vec4 frag_data[4];

in vec2 vary_fragcoord;

uniform sampler2D diffuseRect;     // linear HDR scene (mRT->screen)
uniform sampler2D exposureMap;     // 1x1 auto-exposure scale
uniform sampler2D rimRadHist;      // previous frame radiance history (attachment-2 copy)
uniform float     exposure;        // RenderExposure (manual iris)
uniform int       rim_exposure_lock; // 1 when the tonemapper forces exp_scale = 1 (tonemap_type == 8)
uniform vec2      screen_res;
uniform float     dt;              // frame interval, seconds (0 -> treated as 1/60)

// --- Gather (which light to capture) ---
uniform float GatherRadius;          // full-res px, clamped CPU-side to 0.03 * screen_height
uniform float LightThreshold;        // exposed-linear luminance floor
uniform float SourcePriorityStops;   // stops above threshold that reach priority 1
uniform float GatherFalloff;         // 0 flat .. 1 inverse-square across the 3 rings
uniform float BackgroundBias;        // relative depth a sample must be BEHIND the edge pixel
uniform float BackgroundFeather;     // relative depth feather of that gate
uniform float WrapSpread;            // 0 tight (edge-facing only) .. 1 wide capture
uniform float RimIndependence;       // 0 averaged .. 1 soft-max (brightest direction owns the rim)
uniform float AutoRimGain;
uniform float ColorSaturation;
uniform float RimCrosstalk;          // hottest light bleeds toward white
uniform float CrosstalkKnee;         // exposed-linear knee of that bleed (0.667 == .fx's 1.5)

// --- Shoulder / settle ---
uniform float RimShoulderKnee;       // K, exposed-linear (identity below)
uniform float RimWhite;              // W, exposed-linear asymptote (host enforces W > K)
uniform float AutoSettle;            // per-frame-at-60fps release smoothing, 0..0.95
uniform float SettleDepthTol;        // relative depth change that invalidates history
uniform float SettleLightRejectStops;// light DROP (stops) that invalidates history ...
uniform float SettleLightReject;     // ... weighted by this (0 = pure anti-flicker hold)

// --- Edge ---
uniform int   RimWidth;              // silhouette band width in full-res px (h = RimWidth/2)
uniform float EdgeThreshold;         // relative depth step that counts as a silhouette
uniform float EdgeFeather;
uniform float EdgeNormalGate;        // soft N.V gate against stair/table depth steps (never erases)

// --- Focus (subject) -- resolved CPU-side to one stabilized metric depth ---
uniform float focusZ;                // subject view depth (m)
uniform float FocusFeatherRel;       // relative half-width AND feather of the focus slab (Phase-1: one knob)

// deferredUtil / gbufferUtil / globalF are separate compile units: forward-declare.
float getDepth(vec2 pos_screen);
vec4  getNorm(vec2 screenpos);        // decoded, normalized view-space normal in .xyz (.w is UNDEFINED)
vec4  getNormRaw(vec2 screenpos);     // raw G-buffer texel: .w carries the GBUFFER_FLAG_* value
vec4  getPosition(vec2 pos_screen);
vec4  getPositionWithDepth(vec2 pos_screen, float depth);

// ----------------------------------------------------------------------------
// constants (shared with rimGlowCompositeF.glsl -- keep in sync)
const vec3  LUMA              = vec3(0.2126, 0.7152, 0.0722);
const float TWO_PI            = 6.28318530718;
const int   RIM_GATHER_DIRS   = 8;        // directions (45 deg)
const int   RIM_GATHER_RINGS  = 3;        // radii R/3, 2R/3, R
const int   RIM_GATHER        = 24;       // RIM_GATHER_DIRS * RIM_GATHER_RINGS
const float RIM_BETA_MAX      = 18.0;     // exp(18) = 6.57e7: 24 * 6.57e7 * 6e4 = 9.5e13 << fp32 max
const float RIM_PEAK_NORM     = 0.5;      // w level (geometry * priority) that reads as full strength
const float RIM_HDR_CLAMP     = 6.0e4;    // < 65504 (RGBA16F max)
const float RIM_HIST_CAP_SPAN = 4.0;      // Ycap = K + 4 (W - K): shoulder is within 1.8% of W there
const float RIM_SKY_DEPTH     = 0.999999; // raw depth at/after this = no geometry

// ----------------------------------------------------------------------------
// helpers

// Mirrors tonemapUtilF.glsl applyExposure(): E = exposure * exp_scale, with the
// S-Log3 (tonemap_type == 8) lock expressed through rim_exposure_lock. The
// (!(s > 0)) guard is the only addition: it only fires on an unbound/NaN
// exposure map and exists so Ycap / E can never divide by zero.
float rimExposure()
{
    float s = texture(exposureMap, vec2(0.5, 0.5)).r;
    if (!(s > 0.0)) s = 1.0;
    if (rim_exposure_lock != 0) s = 1.0;
    return max(exposure, 1e-4) * s;
}

// metric view-space depth (m) from a raw depth-buffer sample; sky = far plane
float rimZFromRaw(vec2 uv, float raw)
{
    return max(-getPositionWithDepth(uv, raw).z, 0.0);
}
float rimZ(vec2 uv) { return rimZFromRaw(uv, getDepth(uv)); }

vec3 sceneSample(vec2 uv)
{
    vec3 c = texture(diffuseRect, uv).rgb;
    if (any(isnan(c)) || any(isinf(c))) return vec3(0.0);
    return clamp(c, vec3(0.0), vec3(RIM_HDR_CLAMP));
}

vec4 rt16(vec4 v) { return clamp(v, vec4(-RIM_HDR_CLAMP), vec4(RIM_HDR_CLAMP)); }

// per-frame smoothing factor tuned at 60 fps -> same wall-clock response at any fps
float frameIndep(float s)
{
    float d = (dt > 0.0) ? dt : (1.0 / 60.0);
    return pow(clamp(s, 0.0, 0.98), d * 60.0);
}

// bounded log-luminance priority: 0 at LightThreshold, 1 at +SourcePriorityStops
float lightPriority(float Ye)
{
    float stops = log2(max(Ye, 1e-6) / max(LightThreshold, 1e-6));
    return clamp(stops / max(SourcePriorityStops, 1e-3), 0.0, 1.0);
}

// hue-preserving luminance ceiling (idempotent)
vec3 capLum(vec3 c, float capL)
{
    float l = dot(c, LUMA);
    return (l > capL) ? c * (capL / l) : c;
}

float rimHistCapY()
{
    return RimShoulderKnee + RIM_HIST_CAP_SPAN * max(RimWhite - RimShoulderKnee, 1e-3);
}

// Filmic shoulder in exposed units: S(Y) = Y for Y <= K, else
// K + (W-K)(1 - exp(-(Y-K)/(W-K))). C1 at K (value K, slope 1), concave,
// asymptote W. Applied as a scalar S(Y)/Y RGB scale (hue-preserving).
float rimShoulderY(float Y)
{
    float K = RimShoulderKnee;
    float W = max(RimWhite, K + 1e-3);
    if (Y <= K) return Y;
    float span = W - K;
    return K + span * (1.0 - exp(-(Y - K) / span));
}
vec3 applyRimShoulder(vec3 rad, float E)
{
    float Y = dot(rad, LUMA) * E;
    if (!(Y > RimShoulderKnee)) return rad;          // identity region; also NaN / <= 0 safe
    return rad * (rimShoulderY(Y) / Y);
}

// --- normal read with the B1 fail-open predicate --------------------------
// A pixel has a usable normal only if it has geometry (raw depth < sky) and the
// G-buffer flag is not SKIP_ATMOS (0.0 == sky/water/never-written). The flag
// MUST be read from getNormRaw().w -- getNorm() decodes and leaves .w undefined.
// Returns n = vec3(0) when there is no usable normal; callers go neutral then.
void readNormal(vec2 uv, float raw, out vec3 n, out float ndotv)
{
    vec4 nraw = getNormRaw(uv);
    bool hasN = (raw < RIM_SKY_DEPTH) && !GET_GBUFFER_FLAG(nraw.w, GBUFFER_FLAG_SKIP_ATMOS);
    n = vec3(0.0);
    ndotv = 1.0;
    if (!hasN) return;
    vec3 nn = getNorm(uv).xyz;
    float nl = length(nn);
    if (!(nl > 1e-4) || any(isnan(nn))) return;
    n = nn / nl;
    vec3 p = getPositionWithDepth(uv, raw).xyz;      // view space, camera at origin
    vec3 v = -p * inversesqrt(max(dot(p, p), 1e-8));
    ndotv = clamp(abs(dot(n, v)), 0.0, 1.0);          // abs: back-facing / double-sided alpha normals
}

// screen-space outward direction of the normal. |n.xy| <= 1, deliberately NOT
// normalized. GL uv (+v up) matches view +y, so no flip; if an in-world debug
// view shows the directional fill inverted, this is the single place to negate y.
vec2 screenOutward(vec3 n) { return n.xy; }

// --- subject focus mask ------------------------------------------------------
// Phase-1: one knob. Full weight within +/-FocusFeatherRel (relative) of focusZ,
// feathering to zero at +/-2*FocusFeatherRel.
float focusWeight(float zC)
{
    float fz  = max(focusZ, 1e-3);
    float rel = abs(zC - fz) / fz;
    float f   = max(FocusFeatherRel, 1e-4);
    return 1.0 - clamp((rel - f) / f, 0.0, 1.0);
}

// --- silhouette edge (depth step, relative) + soft normal gate ----------------
float silhouetteEdge(vec2 uv, float zC, vec3 n, float ndotv, out vec2 edgeDir)
{
    vec2 px = vec2(float(max(RimWidth, 1))) / screen_res;
    float zL = rimZ(uv - vec2(px.x, 0.0));
    float zR = rimZ(uv + vec2(px.x, 0.0));
    float zB = rimZ(uv - vec2(0.0, px.y));
    float zT = rimZ(uv + vec2(0.0, px.y));
    float stp  = (max(max(zL, zR), max(zB, zT)) - zC) / max(zC, 1e-3);
    float edge = clamp((stp - EdgeThreshold) / max(EdgeFeather, 1e-5), 0.0, 1.0);

    vec2 g = vec2(zR - zL, zT - zB);
    float gl = length(g);
    vec2 no = screenOutward(n);
    float nol = length(no);
    edgeDir = (gl > 1e-6) ? g / gl : ((nol > 1e-3) ? no / nol : vec2(0.0));

    // SOFT gate: true silhouettes have N.V -> 0, stair/table steps do not.
    // Floor 0.25 so the gate can never hard-erase a rim (hair / alpha have no
    // reliable normal); pixels without a normal are not gated at all.
    float gate = 1.0;
    if (n != vec3(0.0))
    {
        float sil = smoothstep(0.3, 0.7, 1.0 - ndotv);
        gate = mix(1.0, mix(0.25, 1.0, sil), clamp(EdgeNormalGate, 0.0, 1.0));
    }
    return edge * gate;
}

// --- soft-max light gather on linear HDR --------------------------------------
// w = g * l in [0,1]: g = geometric weight (behind-gate * wrap * ring falloff),
// l = bounded log-luminance priority. e = exp(beta * w) <= exp(18).
// lightCol = sum(sc w e)/sum(w e) (hue locks to the brightest captured direction),
// wSoft = sum(w e)/sum(e), strength = wSoft / RIM_PEAK_NORM. rad is scene-linear.
void gatherBacklight(vec2 uv, float zC, vec2 edgeDir, float E,
                     out vec3 rad, out float strength, out vec2 gDir)
{
    vec2  px      = 1.0 / screen_res;
    float wrapExp = mix(6.0, 0.3, clamp(WrapSpread, 0.0, 1.0));
    float bI      = clamp(RimIndependence, 0.0, 1.0);
    float beta    = bI * bI * RIM_BETA_MAX;
    float gfall   = clamp(GatherFalloff, 0.0, 1.0);

    vec3  radSum  = vec3(0.0);
    float softNum = 0.0;
    float softDen = 0.0;
    vec2  dirSum  = vec2(0.0);

    for (int k = 0; k < RIM_GATHER; ++k)
    {
        int   di   = k / RIM_GATHER_RINGS;
        int   ri   = k - di * RIM_GATHER_RINGS;
        float a    = (float(di) + 0.5) / float(RIM_GATHER_DIRS) * TWO_PI;
        float rN   = (float(ri) + 1.0) / float(RIM_GATHER_RINGS);          // 1/3, 2/3, 1
        vec2  d    = vec2(cos(a), sin(a));
        vec2  suv  = clamp(uv + d * (GatherRadius * rN) * px, vec2(0.0), vec2(1.0));

        float zS   = rimZ(suv);
        float bg   = clamp(((zS / zC) - 1.0 - BackgroundBias) / max(BackgroundFeather, 1e-4), 0.0, 1.0);
        vec3  sc   = sceneSample(suv);
        float l    = lightPriority(dot(sc, LUMA) * E);
        float wrap = pow(clamp(dot(edgeDir, d) * 0.5 + 0.5, 0.0, 1.0), wrapExp);   // base >= 0, exp >= 0.3
        float fall = clamp(0.111111 / max(rN * rN, 1e-3), 0.0, 1.0);              // 1, .25, .111
        float w    = bg * wrap * mix(1.0, fall, gfall) * l;                         // [0,1] by construction
        float e    = exp(beta * w);
        float we   = w * e;

        softNum += we;
        softDen += e;
        radSum  += sc * we;
        dirSum  += d * we;
    }

    vec3  lightCol = radSum / max(softNum, 1e-6);   // radSum == 0 whenever softNum == 0
    float wSoft    = softNum / softDen;             // softDen >= RIM_GATHER (each e >= 1)
    strength = clamp(wSoft / RIM_PEAK_NORM, 0.0, 1.0);
    rad      = lightCol * strength * AutoRimGain;
    float dl = length(dirSum);
    gDir     = (dl > 1e-6) ? dirSum / dl : edgeDir;
}

// --- temporal settle (B4: pre-shoulder scene-linear history, E-free) ----------
vec4 readHist(vec2 uv)
{
    vec4 h = texture(rimRadHist, uv);
    return (any(isnan(h)) || any(isinf(h))) ? vec4(0.0) : h;
}
float depthReject(float zC, float zH)
{
    float tol = max(SettleDepthTol, 1e-3);
    return smoothstep(0.5 * tol, tol, abs(zC - zH) / max(zC, 1e-3));
}
float lightReject(float lumH, float lumC)   // fires only when the light FELL by > S stops
{
    float S    = max(SettleLightRejectStops, 0.0);
    float drop = log2((lumH + 1e-4) / (lumC + 1e-4));
    return smoothstep(S, S + 1.0, drop) * clamp(SettleLightReject, 0.0, 1.0);
}

// ON-EDGE. rad is fresh, scene-linear, PRE-shoulder. Returns the settled
// pre-shoulder value; histOut is the attachment-2 write.
vec3 settleRad(vec3 rad, vec2 uv, float zC, float E, out vec4 histOut)
{
    float capL = rimHistCapY() / E;                 // scene-linear ceiling under CURRENT E
    rad = capLum(rad, capL);
    vec4  h    = readHist(uv);
    vec3  hr   = capLum(h.rgb, capL);               // re-cap: bounds a capped source stored under a smaller E
    float zH   = h.a;
    float s    = frameIndep(AutoSettle);
    float lumC = dot(rad, LUMA);
    float lumH = dot(hr, LUMA);
    float k    = (lumC > lumH) ? s * 0.5 : s;       // attack = half release; comparison is E-free
    k *= (1.0 - depthReject(zC, zH)) * (1.0 - lightReject(lumH, lumC));
    vec3 o  = mix(rad, hr, k);
    histOut = rt16(vec4(o, zC));
    return o;
}

// OFF-EDGE (B3). No observation: decay at the release rate exactly as if zero
// light had been observed, with the same depth rejection; the light rejection
// is evaluated against lumC = 0 (so SettleLightReject > 0 drops the carry, by
// the user's choice). Depth is always refreshed to the current surface.
vec4 carryHist(vec2 uv, float zC, float E)
{
    vec4  h    = readHist(uv);
    vec3  hr   = capLum(h.rgb, rimHistCapY() / E);
    float zH   = h.a;
    float s    = frameIndep(AutoSettle);
    float keep = s * (1.0 - depthReject(zC, zH)) * (1.0 - lightReject(dot(hr, LUMA), 0.0));
    return rt16(vec4(hr * keep, zC));
}

// ----------------------------------------------------------------------------
void main()
{
    vec2  uv  = vary_fragcoord;
    float E   = rimExposure();
    float raw = getDepth(uv);
    float zC  = rimZFromRaw(uv, raw);
    // DUMB MODE: rim the whole scene. No subject/focus isolation -- every
    // in-scene silhouette edge gets the rim (M=1 on all geometry, 0 on sky).
    // (focusWeight() left in the file for a future optional "subject only" toggle.)
    float M   = (raw < RIM_SKY_DEPTH) ? 1.0 : 0.0;

    vec3 n; float ndotv;
    readNormal(uv, raw, n, ndotv);

    vec2  edgeDir = vec2(0.0);
    float edge    = (M > 0.0) ? silhouetteEdge(uv, zC, n, ndotv, edgeDir) : 0.0;

    vec3  rad      = vec3(0.0);
    vec2  gDir     = vec2(0.0);
    float strength = 0.0;
    vec4  hist;

    if (edge > 0.0)
    {
        gatherBacklight(uv, zC, edgeDir, E, rad, strength, gDir);   // scene-linear, pre-shoulder
        rad = settleRad(rad, uv, zC, E, hist);                       // linear-domain EMA, capped, E-free
        rad = applyRimShoulder(rad, E);                              // display mapping, current E, once

        // highlight crosstalk (hottest light cores toward white), exposed units
        float le = dot(rad, LUMA) * E;
        float ct = (1.0 - exp(-le / max(CrosstalkKnee, 1e-3))) * clamp(RimCrosstalk, 0.0, 1.0);
        rad = mix(rad, vec3(dot(rad, LUMA)), clamp(ct, 0.0, 1.0));

        // artistic saturation (1 = scene-exact)
        rad = max(mix(vec3(dot(rad, LUMA)), rad, max(ColorSaturation, 0.0)), vec3(0.0));
    }
    else
    {
        hist = carryHist(uv, zC, E);
    }

    frag_data[0] = rt16(vec4(rad * edge, edge));            // mask (premultiplied)
    frag_data[1] = vec4(M, 0.0, 0.0, 1.0);                  // subj (R16F takes .r)
    frag_data[2] = hist;                                    // radHist_out
    frag_data[3] = vec4(gDir * (edge * strength), 0.0, 1.0);// dir (RG16F takes .rg; signed, premultiplied)
}
