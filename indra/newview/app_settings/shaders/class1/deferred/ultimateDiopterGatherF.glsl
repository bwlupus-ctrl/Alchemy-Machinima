/**
 * @file ultimateDiopterGatherF.glsl
 *
 * [Ultimate Diopter] Pass 1 of 2: dual-focus gather to MRT.
 *   frag_data[0] = clear-side gather RGB (linear) + region mask in A
 *   frag_data[1] = diopter-side gather RGB (linear) + refraction rim in A
 *   frag_data[2] = mask-blended, mirrored source uv the diopter side samples
 *                  (RG16F; the present pass re-warps scene depth through it
 *                  so ReShade depth effects align with the warped color)
 *
 * Native port of the VirtualCinema_Diopter v2.2 region/glass/gather system
 * plus the Ultimate Diopter halo fold layer (concentric ring fold, twist,
 * lobes) and a kaleidoscope angular fold subset. Focus planes are in view
 * meters from native depth (deferredUtil getPosition) - no depth
 * calibration sliders. All motion is resolved on the CPU per frame; this
 * shader is deterministic given its uniforms.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2026, Linden Research, Inc.
 * $/LicenseInfo$
 */

/*[EXTRA_CODE_HERE]*/

out vec4 frag_data[3];

in vec2 vary_fragcoord;

uniform sampler2D diffuseRect;   // post-tonemap scene (ping-pong source)
uniform vec2 screen_res;

// center.xy (uv), size, stretch
uniform vec4 diopter_shape;
// angleRad, feather, invert (0/1), content (0 diopter / 1 sharp window)
uniform vec4 diopter_shape2;
// shape index, hollow, cornerRound, splitCurvature
uniform vec4 diopter_shape3;
// wobbleAmt, wobbleFreq, wavePhase, waveGain
uniform vec4 diopter_shape4;
// polySides, starPoints, starInner, squirclePow
uniform vec4 diopter_shape5;
// petalCount, petalDepth, blobSeed, blobAmt
uniform vec4 diopter_shape6;
// crescentBite, crescentShift, arcLenRad, brokenCount
uniform vec4 diopter_shape7;
// baseFocusM, lensFocusM, focusWidthM, falloffRate
uniform vec4 diopter_focus;
// falloffCurve, nearStr, farStr, maxBlurPx
uniform vec4 diopter_focus2;
// floorMaxPx, spotBlur, bokehHi, magnify
uniform vec4 diopter_focus3;
// axialCA (m), fieldCurve, depthEdgeM, reserved
uniform vec4 diopter_focus4;
// apShape, blades, bladeRotRad, bladeCurve
uniform vec4 diopter_aperture;
// apInner, anamorph, anamAngleRad, catEye
uniform vec4 diopter_aperture2;
// profile, ior1, thickness, rimWidth
uniform vec4 diopter_glass;
// rimWarp, placementMode (0 framed / 1 on-lens), reserved, reserved
uniform vec4 diopter_glass2;
// ringCount, ringFold, twistRad, ringPhase
uniform vec4 diopter_halo;
// lobeAmt, lobeCount, lobePhaseRad, warpActive (0/1 gate for the halo warp)
uniform vec4 diopter_halo2;
// pattern mode, segments, feedRad, srcZoom
uniform vec4 diopter_pattern;

// deferredUtil.glsl is linked in as a separate compile unit (isDeferred), so
// its functions must be forward-declared here or this unit fails to compile
vec4 getPosition(vec2 pos_screen);   // view space, managed inv_proj

#ifndef DIOPTER_TAPS
#define DIOPTER_TAPS 24
#endif

const float UD_TAU  = 6.2831853;
const float UD_PI   = 3.14159265;
const vec3  UD_LUMA = vec3(0.2126, 0.7152, 0.0722);

float ud_sat(float x) { return clamp(x, 0.0, 1.0); }

float ud_hash(vec2 p)
{
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

// approximate display <-> working linear (self-contained; post chain is
// already tone-mapped so a pure power curve is sufficient for gather energy)
vec3 ud_lin(vec3 c)   { return pow(max(c, vec3(0.0)), vec3(2.2)); }

// triangle mirror-repeat on [0,1]; out-of-frame samples reflect instead of
// clamping to an edge streak or black border
float ud_mirror1(float x)
{
    float t = fract(x * 0.5) * 2.0;
    return 1.0 - abs(t - 1.0);
}
vec2 ud_mirror2(vec2 v) { return vec2(ud_mirror1(v.x), ud_mirror1(v.y)); }

// view-space distance in meters at a screen uv
float ud_viewDist(vec2 tc)
{
    return max(-getPosition(tc).z, 0.01);
}

// artistic circle-of-confusion in output pixels for a focus plane in meters.
// Falloff is relative to the focus distance so the response is scale
// invariant (no near/far/curve calibration needed).
float ud_coc(float distM, float focusM)
{
    float diff = distM - focusM;
    float dead = diopter_focus.z * 0.5;
    float m = max(abs(diff) - dead, 0.0);
    float fall = ud_sat(m * diopter_focus.w / max(focusM, 0.5));
    fall = pow(fall, diopter_focus2.x);
    float strength = (diff < 0.0) ? diopter_focus2.y : diopter_focus2.z;
    return fall * strength * diopter_focus2.w;
}

// normalized radius from the optical center (0 center .. 1 corner), aspect aware
float ud_radiusNorm(vec2 uv)
{
    float aspect = screen_res.x / screen_res.y;
    vec2 dd = uv - diopter_shape.xy;
    dd.x *= aspect;
    float denom = max(length(vec2(aspect * 0.5, 0.5)), 1e-4);
    return ud_sat(length(dd) / denom);
}

//---------------------------------------------------------------- region mask
// unit-radius polar profile for the radial shapes (multiplier on Size)
float ud_edgeProfile(float ang, int shape)
{
    if (shape == 4) return 1.0;                                  // Circle

    if (shape == 5)                                              // Squircle
    {
        float n  = max(diopter_shape5.w, 1.0);
        float cs = max(abs(cos(ang)), 1e-4);
        float sn = max(abs(sin(ang)), 1e-4);
        float d  = pow(pow(cs, n) + pow(sn, n), 1.0 / n);
        return 1.0 / max(d, 1e-3);
    }
    if (shape == 6)                                              // Polygon
    {
        float n = max(diopter_shape5.x, 3.0);
        float seg = UD_TAU / n;
        float aa = ang - seg * floor(ang / seg + 0.5);
        return cos(seg * 0.5) / max(cos(aa), 1e-3);
    }
    if (shape == 7)                                              // Star
    {
        float n = max(diopter_shape5.y, 3.0);
        float seg = UD_TAU / n;
        float aa = ang - seg * floor(ang / seg + 0.5);
        float tn = ud_sat(abs(aa) / (seg * 0.5));
        return mix(1.0, max(diopter_shape5.z, 0.05), tn);
    }
    if (shape == 8)                                              // Heart (polar)
    {
        // reference math was authored for ReShade's y-down UVs; our
        // fullscreen VS is y-up, so the angle negation drops out
        float a  = ang;
        float sa = sin(a), caH = cos(a);
        float r  = 2.0 - 2.0 * sa + sa * sqrt(abs(caH)) / (sa + 1.4);
        return max(r, 0.02) * 0.30;
    }
    if (shape == 9)                                              // Flower
    {
        float k = max(diopter_shape6.x, 2.0);
        float depth = ud_sat(diopter_shape6.y);
        return 1.0 - depth * 0.5 * (1.0 - cos(ang * k));
    }

    // shape 10: Blob - seeded 3-harmonic organic outline
    float s1 = ud_hash(vec2(diopter_shape6.z,  7.0)) * UD_TAU;
    float s2 = ud_hash(vec2(diopter_shape6.z, 13.0)) * UD_TAU;
    float s3 = ud_hash(vec2(diopter_shape6.z, 29.0)) * UD_TAU;
    float w = 0.55 * sin(ang * 2.0 + s1)
            + 0.30 * sin(ang * 3.0 + s2)
            + 0.15 * sin(ang * 5.0 + s3);
    return max(1.0 + ud_sat(diopter_shape6.w) * 0.45 * w, 0.1);
}

// signed angular distance wrapped to [-pi, pi]
float ud_wrapPi(float a)
{
    return a - UD_TAU * floor(a / UD_TAU + 0.5);
}

// region mask: 1 = diopter glass, 0 = clear side
float ud_mask(vec2 uv)
{
    int   shape   = int(diopter_shape3.x + 0.5);
    int   content = int(diopter_shape2.w + 0.5);
    float m;

    if (diopter_glass2.y > 0.5)                      // On-Lens (edge to edge)
    {
        // no shape SDF: the whole frame is glass and the center is only the
        // optical axis. Only the angular arc / broken-ring crops still
        // apply, computed in the same aspect-corrected, rotated local frame
        // as the framed shapes ("full with angular crops").
        float aspect = screen_res.x / screen_res.y;
        vec2 p = uv - diopter_shape.xy;
        p.x *= aspect;
        float sA = sin(diopter_shape2.x), cA = cos(diopter_shape2.x);
        vec2 q = vec2(p.x * cA - p.y * sA, p.x * sA + p.y * cA);

        q.x /= max(diopter_shape.w, 0.05);           // stretch

        float feather = max(diopter_shape2.y, 1e-4);
        float ang = atan(q.y, q.x);

        m = 1.0;

        float arcLen = diopter_shape7.z;
        if (arcLen < UD_TAU - 1e-3)
        {
            float af = max(feather * 4.0, 0.02);
            float ad = abs(ud_wrapPi(ang)) - arcLen * 0.5;
            m *= 1.0 - smoothstep(-af, af, ad);
        }
        float broken = diopter_shape7.w;
        if (broken > 0.5)
        {
            // glass occupies the central 65% of each of N segments
            float ph = abs(fract(ang * broken / UD_TAU + 0.5) * 2.0 - 1.0);
            m *= 1.0 - smoothstep(0.65 - 0.15, 0.65 + 0.15, ph);
        }
    }
    else if (shape == 0)                             // Full Frame
    {
        m = 1.0;
    }
    else
    {
        float aspect = screen_res.x / screen_res.y;
        vec2 p = uv - diopter_shape.xy;
        p.x *= aspect;
        float sA = sin(diopter_shape2.x), cA = cos(diopter_shape2.x);
        vec2 q = vec2(p.x * cA - p.y * sA, p.x * sA + p.y * cA);

        q.x /= max(diopter_shape.w, 0.05);           // stretch

        float feather = max(diopter_shape2.y, 1e-4);
        float S = max(diopter_shape.z, 1e-4);

        if (shape == 8) q.y -= 0.6 * S;              // recentre heart (y-up)

        float ang = atan(q.y, q.x);
        float len = length(q);

        float wobY = diopter_shape4.x * sin(q.y * diopter_shape4.y)
                   + diopter_shape4.w * sin(q.y * diopter_shape4.y * 1.31 + diopter_shape4.z);
        float wobA = diopter_shape4.x * sin(ang * diopter_shape4.y)
                   + diopter_shape4.w * sin(ang * diopter_shape4.y * 1.31 + diopter_shape4.z);

        if (shape == 2)                              // Ramp
        {
            float proj = q.x + wobY;
            float w = max(S, 1e-4);
            m = ud_sat((proj + w * 0.5) / w);
        }
        else
        {
            float sd;

            if (shape == 1)                          // Split half-plane
            {
                sd = -(q.x + diopter_shape3.w * (q.y * q.y) + wobY);
            }
            else if (shape == 3)                     // Bar
            {
                sd = abs(q.x - wobY) - S;
            }
            else if (shape == 11)                    // Crescent
            {
                float dO = len - S;
                vec2 bite = vec2(diopter_shape7.y * S, 0.0);
                float dI = length(q - bite) - S * max(diopter_shape7.x, 0.05);
                sd = max(dO, -dI) - wobA;
            }
            else                                     // radial closed shapes 4..10
            {
                float edge = ud_edgeProfile(ang, shape);
                if (shape == 6 || shape == 7 || shape == 9 || shape == 10)
                    edge = mix(edge, 1.0, ud_sat(diopter_shape3.z));
                sd = len - edge * S - wobA;
            }

            // Hollow: onion the SDF -> ring / outline of any closed shape
            if (shape >= 3 && diopter_shape3.y > 1e-3)
            {
                float shellH = (1.0 - ud_sat(diopter_shape3.y)) * S * 0.5;
                sd = abs(sd + shellH) - shellH;
            }

            m = 1.0 - smoothstep(-feather, feather, sd);

            // angular arc coverage (half moon / split moon / broken ring)
            if (shape >= 4 || shape == 11)
            {
                float arcLen = diopter_shape7.z;
                if (arcLen < UD_TAU - 1e-3)
                {
                    float af = max(feather * 4.0, 0.02);
                    float ad = abs(ud_wrapPi(ang)) - arcLen * 0.5;
                    m *= 1.0 - smoothstep(-af, af, ad);
                }
                float broken = diopter_shape7.w;
                if (broken > 0.5)
                {
                    // glass occupies the central 65% of each of N segments
                    float ph = abs(fract(ang * broken / UD_TAU + 0.5) * 2.0 - 1.0);
                    m *= 1.0 - smoothstep(0.65 - 0.15, 0.65 + 0.15, ph);
                }
            }
        }
    }

    if (content == 1) m = 1.0 - m;                   // Sharp Window
    return ud_sat(diopter_shape2.z > 0.5 ? (1.0 - m) : m);
}

//---------------------------------------------------------------- refraction
// spherical-cap height-field displacement; rim in [0,1]
void ud_glassRefract(vec2 uv, out vec2 disp, out float rim)
{
    disp = vec2(0.0);
    rim  = 0.0;
    int profile = int(diopter_glass.x + 0.5);
    float radius = diopter_shape.z;
    if (profile <= 0 || radius < 1e-4) return;

    float aspect = screen_res.x / screen_res.y;
    vec2 e = uv - diopter_shape.xy;
    e.x *= aspect;
    e /= radius;
    float r = length(e);
    if (r >= 1.0) return;

    vec2 dir = (r > 1e-4) ? (e / r) : vec2(0.0);

    float rc    = min(r, 0.999);
    float cap   = sqrt(max(1.0 - rc * rc, 1e-4));
    float slope = rc / cap;

    float conv = -1.0;                               // converging -> magnify
    if      (profile == 2) slope *= 1.8;             // Biconvex
    else if (profile == 3) slope *= (1.0 - 0.6 * rc); // Meniscus
    else if (profile == 4) conv = 1.0;               // Bubble (diverging)

    float rimWidth = max(diopter_glass.w, 1e-4);
    float rimW = ud_sat((r - (1.0 - rimWidth)) / rimWidth);
    slope *= 1.0 + diopter_glass2.x * rimW * rimW;
    slope = min(slope, 40.0);                        // clamp the singular rim

    float mag = diopter_glass.y * diopter_glass.z * 0.06;
    disp = dir * slope * mag * conv;

    // On-Lens: the unit rim now sits at the frame corners, so the cap-slope
    // singularity would displace edge pixels by most of the frame and
    // mirror-smear them. Keep visible edge character but bound it: gentler
    // slope ceiling and a hard cap on the displacement length.
    if (diopter_glass2.y > 0.5)
    {
        disp = dir * min(slope, 8.0) * mag * conv;
        float dlen = length(disp);
        if (dlen > 0.12)
        {
            disp *= 0.12 / dlen;
        }
    }

    disp.x /= aspect;
    rim = rimW;
}

//---------------------------------------------------------------- halo warp
// pattern fold + concentric ring fold + twist + lobes -> warped gather center
vec2 ud_haloWarp(vec2 uv)
{
    // CPU sets diopter_halo2.w only when some fold/twist/lobe/pattern/zoom
    // term is actually non-neutral; otherwise this must be an exact identity
    // (the reconstruction below is only identity within the annulus, so the
    // gate is required for split/full/bar shapes whose masks extend past it).
    if (diopter_halo2.w < 0.5) return uv;

    float aspect = screen_res.x / screen_res.y;
    vec2 p = uv - diopter_shape.xy;
    p.x *= aspect;
    float r = length(p);
    float theta = atan(p.y, p.x);

    float outerR = max(diopter_shape.z, 1e-4);
    float innerR = outerR * ud_sat(diopter_shape3.y);
    float u = ud_sat((r - innerR) / max(outerR - innerR, 1e-4));

    // optional kaleidoscope angular fold (inside the glass only)
    int pmode = int(diopter_pattern.x + 0.5);
    if (pmode > 0)
    {
        float seg = UD_TAU / max(diopter_pattern.y, 2.0);
        float au = theta / seg;
        float idx = floor(au);
        float local = theta - idx * seg;
        if (pmode == 1)                              // radial mirror
        {
            float isOdd = step(0.25, fract(idx * 0.5));
            local = mix(local, seg - local, isOdd);
        }
        else if (pmode == 3)                         // star radial warp
        {
            float m2 = local - seg * floor(local / seg + 0.5);
            float tn = ud_sat(abs(m2) / (seg * 0.5));
            r *= mix(1.0, abs(tn * 2.0 - 1.0) * 0.9 + 0.1, 0.6);
        }
        // pmode 2 (radial rotate): local as-is -> pinwheel
        theta = local + diopter_pattern.z;
    }

    // concentric ring fold (the Halo echo)
    float ringX = u * diopter_halo.x + diopter_halo.w;
    float folded = 1.0 - abs(fract(ringX * 0.5) * 2.0 - 1.0);
    float rFold = mix(u, folded, ud_sat(diopter_halo.y));
    float rr = (innerR + rFold * (outerR - innerR)) * max(diopter_pattern.w, 0.05);

    // radius-dependent twist + low-frequency handcrafted lobes
    float thetaW = theta
                 + diopter_halo.z * u
                 + diopter_halo2.x * sin(diopter_halo2.y * theta + diopter_halo2.z) * u;

    vec2 np = vec2(cos(thetaW), sin(thetaW)) * rr;
    np.x /= aspect;

    // Band envelope: outside the glass annulus the fold coordinate clamps
    // (u pins at 1 beyond the rim, 0 inside the hole), which would collapse
    // samples onto a rim circle for masks that extend past the band. Fade
    // the warp to identity beyond the outer radius AND inside the hollow
    // core, so out-of-band pixels and the protected center portal stay exact.
    float env = 1.0 - smoothstep(outerR, outerR * 1.2, r);
    if (innerR > 1e-4)
    {
        env *= smoothstep(innerR * 0.8, innerR, r);
    }
    return mix(uv, diopter_shape.xy + np, env);
}

//---------------------------------------------------------------- aperture
// bokeh reach multiplier by tap direction
float ud_apReach(float ang)
{
    int shape = int(diopter_aperture.x + 0.5);
    if (shape <= 0) return 1.0;                      // Round

    float rot = diopter_aperture.z;
    if (shape == 1)                                  // Polygon
    {
        float n = max(diopter_aperture.y, 3.0);
        float seg = UD_TAU / n;
        float a = ang - rot;
        a = a - seg * floor(a / seg + 0.5);
        float pr = cos(seg * 0.5) / max(cos(a), 1e-3);
        return mix(pr, 1.0, ud_sat(diopter_aperture.w));
    }
    if (shape == 2)                                  // Star
    {
        float n = max(diopter_aperture.y, 3.0);
        float seg = UD_TAU / n;
        float a = ang - rot;
        a = a - seg * floor(a / seg + 0.5);
        float tn = ud_sat(abs(a) / (seg * 0.5));
        float rr = mix(1.0, max(diopter_aperture2.x, 0.05), tn);
        return mix(rr, 1.0, ud_sat(diopter_aperture.w) * 0.5);
    }
    if (shape == 3)                                  // Heart (cardioid)
    {
        // y-up frame: negate the tap angle so the heart points the same
        // way as the y-down reference
        float a = -ang - rot + 1.5707963;
        float card = 1.0 - sin(a);
        float rr = max(card * 0.5, 0.05);
        return mix(rr, 1.0, ud_sat(diopter_aperture.w) * 0.5);
    }

    // Anamorphic ellipse
    float phi = ang - diopter_aperture2.z;
    float B = clamp(diopter_aperture2.y, 0.05, 1.0);
    float cphi = cos(phi), sphi = sin(phi);
    float denom = sqrt(cphi * cphi + (sphi / B) * (sphi / B));
    return 1.0 / max(denom, 1e-3);
}

//---------------------------------------------------------------- gather
// source uv a gather actually reads for a given (warped) center: magnify
// about the optical center, then the refraction displacement. Shared with
// the warp-map output in main() so the depth re-warp stays in exact
// agreement with what the diopter gather samples.
vec2 ud_gatherBuv(vec2 gatherCenter, float magnify, vec2 refrDisp)
{
    return diopter_shape.xy + (gatherCenter - diopter_shape.xy) * (1.0 / magnify) + refrDisp;
}

// scatter-as-gather defocus in working linear, per-channel axial CA,
// highlight bias, shaped bokeh reach, cat's-eye clipping, depth-edge protect
vec3 ud_gather(vec2 uv, vec2 gatherCenter, float focusM, float magnify,
               float floorCoC, float axialCA, vec2 refrDisp)
{
    vec2 buv = ud_gatherBuv(gatherCenter, magnify, refrDisp);
    float searchR = max(diopter_focus2.w, floorCoC);
    vec2 px = 1.0 / screen_res;

    float hiBias = diopter_focus3.z;
    float ce = diopter_aperture2.w * ud_radiusNorm(uv);
    vec2 rdir = uv - diopter_shape.xy;
    float rlen = length(rdir);
    rdir = (rlen > 1e-4) ? (rdir / rlen) : vec2(0.0, 1.0);

    // sample-space center: mirror BEFORE both the color read and the depth
    // read so depth-edge protection compares the same image location
    vec2 sbuv = ud_mirror2(buv);
    float centerDist = ud_viewDist(sbuv);
    float edgeM = diopter_focus4.z;

    vec3 cc = ud_lin(textureLod(diffuseRect, sbuv, 0.0).rgb);
    float hb0 = 1.0 + hiBias * dot(cc, UD_LUMA);
    vec3 sum  = cc * hb0;
    vec3 wsum = vec3(hb0);

    for (int i = 0; i < DIOPTER_TAPS; i++)
    {
        float fi = float(i) + 0.5;
        float r  = sqrt(fi / float(DIOPTER_TAPS));
        float a  = fi * 2.39996323;                  // golden angle
        vec2 o = vec2(cos(a), sin(a)) * r;

        vec2 tc = buv + o * searchR * px;
        vec2 stc = ud_mirror2(tc);
        vec3 c  = ud_lin(textureLod(diffuseRect, stc, 0.0).rgb);

        float dt = ud_viewDist(stc);
        float cocR = max(ud_coc(dt, focusM + axialCA), floorCoC);
        float cocG = max(ud_coc(dt, focusM),           floorCoC);
        float cocB = max(ud_coc(dt, focusM - axialCA), floorCoC);

        float reachScale = ud_apReach(atan(o.y, o.x));

        float td = r * searchR;
        vec3 reach = vec3(cocR, cocG, cocB) * reachScale;
        vec3 w = clamp(reach - td + 1.0, 0.0, 1.0);

        // cat's-eye: clip the outward side of the bokeh toward frame edges
        float catProj = dot(o / max(r, 1e-4), rdir);
        w *= ud_sat(1.0 - ce * ud_sat(catProj));

        // depth-edge protect: fade taps that are much nearer than the
        // gather center (foreground silhouettes bleeding over the subject)
        if (edgeM > 1e-3)
        {
            float nearer = centerDist - dt;
            w *= clamp(1.0 - max(nearer - edgeM, 0.0) / max(edgeM, 0.05), 0.1, 1.0);
        }

        w *= 1.0 + hiBias * dot(c, UD_LUMA);

        sum  += c * w;
        wsum += w;
    }
    return sum / max(wsum, vec3(1e-4));
}

//---------------------------------------------------------------- main
void main()
{
    vec2 uv = vary_fragcoord.xy;

    float m = ud_mask(uv);

    vec2 refrDisp;
    float rim;
    ud_glassRefract(uv, refrDisp, rim);

    // fold/twist warp of the gather center, feathered by the mask so the
    // warp never produces a discontinuity at the coverage boundary
    vec2 warped = ud_haloWarp(uv);
    vec2 dioCenter = mix(uv, warped, m);

    // field-curvature / sharp-window floor blur on its own larger budget
    float rNorm = ud_radiusNorm(uv);
    float floorCoC = diopter_focus4.y * rNorm * rNorm * diopter_focus3.x;
    if (int(diopter_shape2.w + 0.5) == 1)
        floorCoC = max(floorCoC, diopter_focus3.y * diopter_focus3.x);

    vec3 cDio   = vec3(0.0);
    vec3 cClear = vec3(0.0);

    bool needDio   = m > 0.001;
    bool needClear = m < 0.999;

    if (needDio)
    {
        cDio = ud_gather(uv, dioCenter, diopter_focus.y, diopter_focus3.w,
                         floorCoC, diopter_focus4.x, refrDisp);
    }
    if (needClear)
    {
        cClear = ud_gather(uv, uv, diopter_focus.x, 1.0, 0.0, 0.0, vec2(0.0));
    }
    else
    {
        cClear = cDio;
    }
    if (!needDio)
    {
        cDio = cClear;
    }

    // warp map: the final mask-blended, mirrored source uv the diopter side
    // samples for this pixel (identity on the clear side). Computed through
    // ud_gatherBuv with exactly the inputs the dio gather above receives, so
    // the present pass can pull scene depth through the same warp as color.
    // Written on every path through main() — the skip branches above only
    // gate the gathers, never this output.
    vec2 warpUV = ud_mirror2(mix(uv,
        ud_gatherBuv(dioCenter, diopter_focus3.w, refrDisp), m));

    frag_data[0] = vec4(cClear, m);
    frag_data[1] = vec4(cDio, rim);
    frag_data[2] = vec4(warpUV, 0.0, 0.0);
}
