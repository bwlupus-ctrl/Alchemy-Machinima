/**
 * @file ultimateKaleidoF.glsl
 *
 * [Ultimate Diopter] Kaleidoscope tool mode - single full-screen pass to MRT.
 *   frag_data[0] = final composited kaleidoscope color (protect + blend done)
 *   frag_data[1] = unused, vec4(0) (kept so mDiopterMap's 3-attachment layout
 *                  and the diopter gather's out declarations match exactly)
 *   frag_data[2] = warp uv in RG (RG16F; the present pass re-warps scene
 *                  depth through it so ReShade depth effects align with the
 *                  folded color). Where the visible color is a fold tap this
 *                  is the wrapped fold uv; where the protect hold mask or the
 *                  master blend pulls the ORIGINAL frame back on top, the
 *                  warp uv is blended back toward the identity uv by the same
 *                  weight: warpUV = mix(foldUV, uv, max(hold, 1 - blend)).
 *
 * Native port of VirtualCinema_Kaleidoscope.fx v1.2 (GShade / ReShade): all
 * 24 fold modes, Cell Variety (size scatter, breathing, quadtree subdivide,
 * hex merge, iridescent tint), seam soften (Radial Mirror only), protect
 * hold (disc and/or native-depth subject), master blend, 3 debug views.
 * Cell Desync and the TPDF output dither are intentionally absent (desync is
 * deferred; the client post chain already dithers). ReShade's depth
 * near/far/curve decompression is replaced by a native view-space cut in
 * meters (deferredUtil getPosition).
 *
 * This shader is deterministic given its uniforms: NO time, NO motion
 * branches. All animated values (center/zoom/twist/offset after motion, the
 * total rotation with the spin envelope, the source-spin feed angle, the FX
 * flow phase, wave phase/gain) are resolved on the CPU each frame.
 *
 * ORIENTATION: the reference is authored for ReShade's y-down uv frame; the
 * client's fullscreen pass is y-up (the diopter needed the same treatment
 * for its heart shapes). The fold math here runs VERBATIM in a
 * reference-oriented local frame: p = (uv - center) aspect-corrected with
 * p.y negated. Every conversion back to an absolute uv negates the local y
 * again (and only then adds the source offset), so:
 *   - patterns keep the reference's on-screen chirality (positive Pattern
 *     Angle / Twist / Spin rotate clockwise on screen, positive Shape Bias
 *     winds spirals the same handedness as the reference),
 *   - centers, protect centers, and Source Offset stay client-native y-up
 *     (+Y = up on screen, matching the diopter's CenterY convention).
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

// mode, segments, ringCount, edgeWrap (0 mirror / 1 tile / 2 clamp)
uniform vec4 kal_pattern;
// rotRadTotal (pattern angle + motion rot + spin envelope), twistRadTotal
// (incl. Pulse:Twist wobble), starSharp, shapeBias
uniform vec4 kal_shape;
// pattern center.xy (uv, y-up, post-motion), protect center.xy (uv, y-up,
// anchor already resolved on CPU: pattern center / fixed / focus)
uniform vec4 kal_center;
// feedRadTotal (source angle + accumulated source spin), srcZoom
// (incl. Pulse/Heartbeat), srcOffset.xy (uv, y-up, incl. Pulse drift)
uniform vec4 kal_source;
// fxBand, fxAmount, ftPhase (FX Flow clock in cycles = t * flow), fxFreq
uniform vec4 kal_fx;
// protectMode (0 off / 1 disc / 2 depth / 3 disc*depth), discRadius,
// discFeather, depthInvert (0/1)
uniform vec4 kal_protect;
// depthCutM (view meters), depthFeatherM (view meters), reserved, reserved
uniform vec4 kal_depth;
// wavePhase, waveGain (Wave motion outputs), waveAmp, waveFreq
uniform vec4 kal_wave;
// seamSoften, blend (master A/B), debugView (0 off / 1 wedges /
// 2 center-protect / 3 subject mask), reserved
uniform vec4 kal_look;
// cellSizeVar, cellBreathe, cellSubdiv, cellMerge
uniform vec4 kal_cell;
// cellTint, reserved, reserved, reserved
uniform vec4 kal_cell2;

// deferredUtil.glsl is linked in as a separate compile unit (isDeferred), so
// its functions must be forward-declared here or this unit fails to compile
vec4 getPosition(vec2 pos_screen);   // view space, managed inv_proj

const float KAL_TAU = 6.2831853072;

//----------------------------------------------------------------- helpers

// classic sin hash (VCK_Hash) - used where the reference uses it: Shatter
// shard jitter and Gem Facets tilt/glint
float kal_hash(vec2 p)
{
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

// sin-free hash for integer cell ids (Hoskins-style, VCK_HashCell). The sin
// hash amplifies last-ulp input differences at large arguments, which tears
// per-cell values apart when the id is not bit-exact across a cell.
float kal_hashCell(vec2 ci)
{
    vec3 q = fract(vec3(ci.x, ci.y, ci.x) * 0.1031);
    q += dot(q, q.yzx + 33.33);
    return fract((q.x + q.y) * q.z);
}

// per-cell content scale: hashed size scatter + phase-offset breathing.
// >1 = that cell's imagery is magnified. Cheap no-op when both are 0.
float kal_cellScale(float key, float ftime)
{
    if (kal_cell.x <= 0.0 && kal_cell.y <= 0.0) return 1.0;
    float hz = kal_hashCell(vec2(key, 3.33));
    float s = 1.0 + (hz - 0.5) * kal_cell.x * 1.2;
    s *= 1.0 + kal_cell.y * 0.35 * sin(ftime * KAL_TAU + hz * KAL_TAU);
    return max(s, 0.2);
}

// triangle mirror-repeat: identity on [0,1], reflects beyond. No fmod.
float kal_mirror1(float x)
{
    float t = fract(x * 0.5) * 2.0;
    return 1.0 - abs(t - 1.0);
}
vec2 kal_mirror2(vec2 v) { return vec2(kal_mirror1(v.x), kal_mirror1(v.y)); }

vec2 kal_wrapUV(vec2 uv)
{
    int wrap = int(kal_pattern.w + 0.5);
    if (wrap == 0) return kal_mirror2(uv);           // mirror
    if (wrap == 1) return fract(uv);                 // tile
    return clamp(uv, 0.0, 1.0);                      // clamp
}

// hold mask: 1 = show untouched subject over the pattern, 0 = kaleidoscope.
// Depth term is a native view-space cut in meters (no ReShade near/far/curve
// decompression needed).
float kal_holdMask(vec2 uv, float pradN)
{
    int pmode = int(kal_protect.x + 0.5);
    float holdR = 1.0 - smoothstep(kal_protect.y - kal_protect.z,
                                   kal_protect.y + kal_protect.z, pradN);
    if (pmode == 1) return holdR;                    // disc (no depth tap needed)

    float distM = max(-getPosition(uv).z, 0.01);
    float holdD = 1.0 - smoothstep(kal_depth.x - kal_depth.y,
                                   kal_depth.x + kal_depth.y, distM);
    if (kal_protect.w > 0.5) holdD = 1.0 - holdD;

    if (pmode == 2) return holdD;                    // depth (subject in front)
    if (pmode == 3) return holdR * holdD;            // depth, confined to disc
    return 0.0;
}

vec2 kal_rot(vec2 v, float a)
{
    float s = sin(a), c = cos(a);
    return vec2(v.x * c - v.y * s, v.x * s + v.y * c);
}

// reference-oriented (y-down) local offset -> client (y-up) local offset.
// Applied wherever a fold result converts back to an absolute uv; keeps the
// pattern's on-screen chirality identical to the reference.
vec2 kal_toClient(vec2 sp) { return vec2(sp.x, -sp.y); }

// atan(0,0) is undefined in GLSL (HLSL's atan2 returns 0, which the
// reference relies on at the exact pattern/cell centre) - pin it to 0.
float kal_atan2(float y, float x)
{
    if (abs(x) < 1e-20 && abs(y) < 1e-20) return 0.0;
    return atan(y, x);
}

// fold polar angle into a mirrored / rotated wedge, return the feed angle.
float kal_foldAngle(float ang, float seg, out float seamDist)
{
    float au = ang / seg;
    float idx = floor(au);
    float local = ang - idx * seg;                   // 0 .. seg

    if (int(kal_pattern.x + 0.5) == 0)               // Radial Mirror
    {
        float isOdd = step(0.25, fract(idx * 0.5));  // 0 even, 1 odd
        local = mix(local, seg - local, isOdd);
    }
    // Radial Rotate (mode 1): leave local as-is -> pinwheel

    // distance to the nearest wedge boundary, normalized 0..1 (seam soften)
    float toEdge = min(local, seg - local);
    seamDist = clamp(toEdge / (seg * 0.5), 0.0, 1.0);
    return local;
}

//--- lattice / fold helpers for the extra pattern modes --------------------
float kal_mod1(float x, float m)  { return x - m * floor(x / m); }
vec2  kal_mod2(vec2 v, vec2 m)   { return v - m * floor(v / m); }

// reflect x into [0,m] with mirroring (1-D dihedral fold)
float kal_foldRefl(float x, float m)
{
    float t = kal_mod1(x, 2.0 * m);
    return m - abs(t - m);
}

// position relative to the nearest hex-lattice cell centre
vec2 kal_hexLocal(vec2 hp)
{
    const vec2 S = vec2(1.0, 1.7320508);
    vec2 a = kal_mod2(hp, S) - 0.5 * S;
    vec2 b = kal_mod2(hp + 0.5 * S, S) - 0.5 * S;
    return dot(a, a) < dot(b, b) ? a : b;
}

// fold a cell-local vector into an N-fold mirror rosette, remap to a source
// uv (y-unflipped back to the client frame; srcOff applied client-native)
vec2 kal_cellRosette(vec2 lp, float nFold, float feed, float aspect,
                     vec2 centerUV, vec2 srcOff, float zoom, float fill)
{
    float la  = kal_atan2(lp.y, lp.x);
    float lr  = length(lp);
    float seg = KAL_TAU / max(nFold, 2.0);
    float local = kal_foldRefl(la, seg);
    vec2 dir = vec2(cos(local + feed), sin(local + feed));
    vec2 sp  = dir * lr * zoom * fill;
    sp.x /= aspect;
    return centerUV + kal_toClient(sp) + srcOff;
}

//----------------------------------------------------------------- main
void main()
{
    vec2 uv = vary_fragcoord.xy;
    float aspect = screen_res.x / screen_res.y;

    int   kmode = int(kal_pattern.x + 0.5);
    float segsF = kal_pattern.y;
    float ringCountF = kal_pattern.z;

    // CPU-resolved animated placement (motion + spin already composed)
    vec2  center = kal_center.xy;
    float zoom   = kal_source.y;
    vec2  srcOff = kal_source.zw;
    float twist  = kal_shape.y;      // radians, over normalized radius
    float outRot = kal_shape.x;      // radians, total output rotation
    float feed   = kal_source.x;     // radians, feed slice angle
    float ft     = kal_fx.z;         // FX Flow clock (cycles)
    float wavePhase = kal_wave.x;
    float waveGain  = kal_wave.y;
    float waveAmp   = kal_wave.z;
    float waveFreq  = kal_wave.w;

    // reference-oriented local frame (y-down): all fold math runs verbatim
    vec2 p = uv - center;
    p.x *= aspect;
    p.y = -p.y;
    float rad  = length(p);
    float maxR = length(vec2(aspect * 0.5, 0.5));
    float radN = clamp(rad / max(maxR, 1e-4), 0.0, 1.0);

    // protect-disc frame: anchor already resolved on the CPU (pattern
    // center / fixed / focus); only the radial length is used, so the local
    // y orientation is irrelevant here
    vec2 pp = uv - kal_center.zw;
    pp.x *= aspect;
    float pradN = clamp(length(pp) / max(maxR, 1e-4), 0.0, 1.0);

    vec2  srcUV;
    float seamDist = 1.0;
    float cellKey  = -1e5;   // cell identity for tint/scale; sentinel = no cells

    if (kmode == 2)                                 // Mirror Tile (mirror box)
    {
        vec2 q = kal_rot(p, -outRot);               // aspect-corrected, rotated
        q.y = -q.y;                                 // back to client frame (grid is y-symmetric)
        q *= segsF * 0.5 * zoom;                    // grid frequency
        q += srcOff * segsF;
        q.x /= aspect;
        cellKey = dot(floor(q + 0.5), vec2(1.0, 131.0));
        srcUV = kal_mirror2(q + 0.5);
    }
    else if (kmode == 3)                            // Honeycomb (hex lattice, 6-fold)
    {
        float freq = segsF * 0.25;
        vec2 hcp = kal_rot(p, -outRot) * freq;
        vec2 lp  = kal_hexLocal(hcp);
        cellKey = dot(round((hcp - lp) / vec2(0.5, 0.8660254)), vec2(1.0, 131.0));
        srcUV = kal_cellRosette(lp, 6.0, feed, aspect, center, srcOff,
                                zoom / kal_cellScale(cellKey, ft), 2.0);
    }
    else if (kmode == 5)                            // Triangle (hex lattice, 3-fold)
    {
        float freq = segsF * 0.25;
        vec2 hcp = kal_rot(p, -outRot) * freq;
        vec2 lp  = kal_hexLocal(hcp);
        cellKey = dot(round((hcp - lp) / vec2(0.5, 0.8660254)), vec2(1.0, 131.0));
        srcUV = kal_cellRosette(lp, 3.0, feed, aspect, center, srcOff,
                                zoom / kal_cellScale(cellKey, ft), 2.0);
    }
    else if (kmode == 4)                            // Square Grid (square lattice, 4-fold)
    {
        float freq = segsF * 0.25;
        vec2 hp  = kal_rot(p, -outRot) * freq;
        float sublvl = 0.0;                         // hashed quadtree subdivision
        if (kal_cell.z > 0.0)
        {
            if (kal_hashCell(floor(hp) + 17.7) < kal_cell.z)       { hp *= 2.0; sublvl = 1.0; }
            if (sublvl > 0.5 &&
                kal_hashCell(floor(hp) + 29.3) < kal_cell.z * 0.7) { hp *= 2.0; sublvl = 2.0; }
        }
        vec2 lp = (hp - floor(hp)) - 0.5;
        cellKey = dot(floor(hp), vec2(1.0, 131.0)) + sublvl * 7777.0;
        srcUV = kal_cellRosette(lp, 4.0, feed, aspect, center, srcOff,
                                zoom / kal_cellScale(cellKey, ft), 2.0);
    }
    else if (kmode == 8)                            // Pinwheel (rotational lattice, no mirror)
    {
        float freq = segsF * 0.25;
        vec2 hp  = kal_rot(p, -outRot) * freq;
        float sublvl = 0.0;                         // hashed quadtree subdivision
        if (kal_cell.z > 0.0)
        {
            if (kal_hashCell(floor(hp) + 17.7) < kal_cell.z)       { hp *= 2.0; sublvl = 1.0; }
            if (sublvl > 0.5 &&
                kal_hashCell(floor(hp) + 29.3) < kal_cell.z * 0.7) { hp *= 2.0; sublvl = 2.0; }
        }
        vec2 cell = floor(hp);
        cellKey = dot(cell, vec2(1.0, 131.0)) + sublvl * 7777.0;
        vec2 lp = (hp - cell) - 0.5;
        float par = kal_mod1(cell.x + cell.y, 4.0); // 0..3
        lp = kal_rot(lp, par * 1.5707963);          // 90 deg * parity
        vec2 sp = lp * (zoom / kal_cellScale(cellKey, ft)) * 2.0;
        sp.x /= aspect;
        srcUV = center + kal_toClient(sp) + srcOff;
    }
    else if (kmode == 6)                            // Concentric Rings (angular + radial mirror)
    {
        float ang = kal_atan2(p.y, p.x) - outRot;
        ang += twist * radN;
        float seg   = KAL_TAU / max(segsF, 2.0);
        float local = kal_foldRefl(ang, seg);
        float ringW = maxR / max(ringCountF, 1.0);
        float rr    = kal_foldRefl(rad, ringW) * (maxR / ringW);   // reflect radius into rings
        rr *= 1.0 + waveGain * waveAmp * sin(local * waveFreq + wavePhase);
        vec2 dir = vec2(cos(local + feed), sin(local + feed));
        vec2 sp  = dir * rr * zoom;
        sp.x /= aspect;
        srcUV = center + kal_toClient(sp) + srcOff;
    }
    else if (kmode == 7)                            // Star (angular mirror + star radial warp)
    {
        float ang = kal_atan2(p.y, p.x) - outRot;
        ang += twist * radN;
        float seg   = KAL_TAU / max(segsF, 2.0);
        float local = kal_foldRefl(ang, seg);
        float edge  = clamp(local / seg, 0.0, 1.0);                // 0..1 across wedge
        float star  = mix(1.0, abs(edge * 2.0 - 1.0), clamp(kal_shape.z, 0.0, 1.0));
        float sampR = rad * zoom * mix(1.0, star, 0.9);
        sampR *= 1.0 + waveGain * waveAmp * sin(local * waveFreq + wavePhase);
        vec2 dir = vec2(cos(local + feed), sin(local + feed));
        vec2 sp  = dir * sampR;
        sp.x /= aspect;
        srcUV = center + kal_toClient(sp) + srcOff;
    }
    //------------------------------------------------------------------------
    //  Subject FX modes (v1.1). All anchored on `center` -> Motion = Track
    //  locks them to the camera-focus subject; Protect (Depth) drops the
    //  clean figure back on top. `ft` = FX Flow clock (CPU, freeze-aware).
    //------------------------------------------------------------------------
    else if (kmode == 9)                            // Echo Rings (subject repeated in shells)
    {
        float ang = kal_atan2(p.y, p.x) - outRot;
        ang += twist * radN;

        float ringW = maxR / max(ringCountF, 1.0);
        float s  = rad / max(ringW, 1e-4) - ft;              // Flow: shells pour out (+) / in (-)
        float lm = 1.0 - abs(fract(s) * 2.0 - 1.0);          // mirrored 0..1..0 -> seamless shells
        lm *= 1.0 + waveGain * waveAmp * sin(ang * waveFreq + wavePhase);

        float sampR = lm * kal_fx.x * maxR * zoom;           // every shell re-samples the subject disc
        vec2 dir = vec2(cos(ang + feed), sin(ang + feed));
        vec2 sp  = dir * sampR;
        sp.x /= aspect;
        srcUV = center + kal_toClient(sp) + srcOff;
    }
    else if (kmode == 10)                           // Orbit Clones (N copies of the subject)
    {
        float ang = kal_atan2(p.y, p.x) - outRot;
        float seg = KAL_TAU / max(segsF, 2.0);
        float idx = floor(ang / seg + 0.5);                  // nearest wedge bisector
        cellKey = idx;
        float bis = idx * seg + outRot;                      // back to screen space -> Spin orbits

        float orbitR = kal_fx.x * maxR;
        vec2 cloneC = vec2(cos(bis), sin(bis)) * orbitR;

        vec2 lp = p - cloneC;                                // position inside this clone
        lp = kal_rot(lp, -feed);                             // Source Spin: clones self-rotate
        lp *= 1.0 + waveGain * waveAmp                       // Wave motion: clones breathe
                  * sin(idx * waveFreq + wavePhase);
        vec2 sp = lp / max(zoom * kal_cellScale(cellKey, ft), 0.05); // Source Zoom x per-clone scale
        sp.x /= aspect;
        srcUV = center + kal_toClient(sp) + srcOff;
    }
    else if (kmode == 11)                           // Vortex (world churns, subject readable)
    {
        float ang = kal_atan2(p.y, p.x) - outRot;
        float swirl = kal_fx.y * KAL_TAU * radN * radN;      // ~0 at the subject, winds up outward
        ang += swirl + ft * KAL_TAU * radN;                  // Flow: outer field keeps rotating
        ang += twist * radN;

        float sampR = rad * zoom;
        sampR *= 1.0 + waveGain * waveAmp * sin(ang * waveFreq + wavePhase);
        vec2 dir = vec2(cos(ang + feed), sin(ang + feed));
        vec2 sp  = dir * sampR;
        sp.x /= aspect;
        srcUV = center + kal_toClient(sp) + srcOff;
    }
    else if (kmode == 12)                           // Ripple (shockwaves radiating from subject)
    {
        float ang  = kal_atan2(p.y, p.x) - outRot;
        float calm = smoothstep(0.0, max(kal_fx.x, 0.02), radN);  // hold the subject core still
        float wave = sin(radN * kal_fx.w * KAL_TAU - ft * KAL_TAU);
        wave += waveGain * waveAmp * 8.0 * sin(ang * waveFreq + wavePhase);

        float sampR = max(rad + wave * kal_fx.y * 0.08 * maxR * calm, 0.0) * zoom;
        vec2 dir = vec2(cos(ang + feed), sin(ang + feed));
        vec2 sp  = dir * sampR;
        sp.x /= aspect;
        srcUV = center + kal_toClient(sp) + srcOff;
    }
    else if (kmode == 13)                           // Petal Mandala (petals grown from the subject)
    {
        float ang = kal_atan2(p.y, p.x) - outRot;
        ang += twist * radN;
        float seg   = KAL_TAU / max(segsF, 2.0);
        float local = kal_foldRefl(ang, seg);

        float band = max(kal_fx.x * maxR, 1e-3);
        float rr = kal_foldRefl(rad + ft * band, band);      // radial fold at subject scale, flowing
        rr *= 1.0 + waveGain * waveAmp * sin(local * waveFreq + wavePhase);

        vec2 dir = vec2(cos(local + feed), sin(local + feed));
        vec2 sp  = dir * rr * zoom;
        sp.x /= aspect;
        srcUV = center + kal_toClient(sp) + srcOff;
    }
    else if (kmode == 14)                           // Infinity Tunnel (subject at every scale)
    {
        float ang = kal_atan2(p.y, p.x) - outRot;
        float ratio = mix(1.6, 4.0, clamp(kal_fx.y, 0.0, 1.0));   // zoom step per shell
        float lgR = log(ratio);
        float r0  = max(kal_fx.x * maxR, 1e-4);               // innermost subject band

        float s  = log(max(rad, 1e-5) / r0) / lgR - ft;       // Flow: continuous dolly-through
        float lm = 1.0 - abs(fract(s) * 2.0 - 1.0);           // mirrored shell coord -> seamless
        float sampR = r0 * exp(lm * lgR) * zoom;

        ang += twist * lm;                                    // per-shell twist (seamless via lm)
        ang += waveGain * waveAmp * 4.0 * sin(lm * waveFreq + wavePhase);
        vec2 dir = vec2(cos(ang + feed), sin(ang + feed));
        vec2 sp  = dir * sampR;
        sp.x /= aspect;
        srcUV = center + kal_toClient(sp) + srcOff;
    }
    else if (kmode == 15)                           // Shatter (drifting shards, whole subject core)
    {
        float ang = kal_atan2(p.y, p.x) - outRot;
        float seg = KAL_TAU / max(segsF, 2.0);
        float idx = floor(ang / seg);
        cellKey = idx;

        float h1 = kal_hash(vec2(idx, 3.17));
        float h2 = kal_hash(vec2(idx, 7.71));
        float churn = sin(ft * KAL_TAU + h1 * KAL_TAU);       // per-shard slow drift
        churn += waveGain * waveAmp * 8.0 * sin(idx * waveFreq + wavePhase);

        float calm = smoothstep(0.0, max(kal_fx.x, 0.02), radN);  // subject core stays whole
        float jA = (h1 - 0.5) * seg  * kal_fx.y * (0.6 + 0.4 * churn) * calm;
        float jR = (h2 - 0.5) * maxR * kal_fx.y * 0.25 * (0.6 + 0.4 * churn) * calm;

        float sampR = max(rad + jR, 0.0) * zoom;
        vec2 dir = vec2(cos(ang + jA + feed), sin(ang + jA + feed));
        vec2 sp  = dir * sampR;
        sp.x /= aspect;
        srcUV = center + kal_toClient(sp) + srcOff;
    }
    //------------------------------------------------------------------------
    //  Pattern set (v1.2). Shape Bias sculpts, FX Flow animates.
    //------------------------------------------------------------------------
    else if (kmode == 16)                           // Spiral Arms (log-spiral galaxy fold)
    {
        float ang = kal_atan2(p.y, p.x) - outRot;
        float seg = KAL_TAU / max(segsF, 2.0);
        float pitch = kal_shape.w * 10.0;                // arm tightness & handedness
        float su = ang + pitch * log(max(radN, 1e-4));   // log-spiral coordinate
        su += ft * seg;                                  // Flow: material crawls along the arms
        float local = kal_foldRefl(su, seg);

        float sampR = rad * zoom;
        sampR *= 1.0 + waveGain * waveAmp * sin(local * waveFreq + wavePhase);
        vec2 dir = vec2(cos(local + feed), sin(local + feed));
        vec2 sp  = dir * sampR;
        sp.x /= aspect;
        srcUV = center + kal_toClient(sp) + srcOff;
    }
    else if (kmode == 17)                           // Droste Spiral (endless zoom-rotate loop)
    {
        float ang = kal_atan2(p.y, p.x) - outRot;
        float ratio = mix(1.6, 4.0, clamp(kal_fx.y, 0.0, 1.0));   // zoom step per octave
        float lgR = log(ratio);
        float r0  = max(kal_fx.x * maxR, 1e-4);

        float s  = log(max(rad, 1e-5) / r0) / lgR - ft;       // Flow: the infinite zoom
        float lm = 1.0 - abs(fract(s) * 2.0 - 1.0);           // mirrored octave -> seamless
        float sampR = r0 * exp(lm * lgR) * zoom;

        float couple = kal_shape.w * KAL_TAU;                 // rotation per octave = the spiral
        float sampA = ang + (lm - 0.5) * couple + twist * radN;
        sampA += waveGain * waveAmp * 4.0 * sin(lm * waveFreq + wavePhase);
        vec2 dir = vec2(cos(sampA + feed), sin(sampA + feed));
        vec2 sp  = dir * sampR;
        sp.x /= aspect;
        srcUV = center + kal_toClient(sp) + srcOff;
    }
    else if (kmode == 18)                           // Polygon Prism (mirrored N-gon room)
    {
        float ang = kal_atan2(p.y, p.x) - outRot;
        float seg   = KAL_TAU / max(segsF, 3.0);
        float local = kal_foldRefl(ang, seg);

        // polygon metric: distance measured to flat N-gon walls, not a circle
        float polyR = rad * cos(local - seg * 0.5) / max(cos(seg * 0.5), 1e-3);
        float R0 = max(kal_fx.x * maxR, 1e-3);                // room size
        float rr = kal_foldRefl(polyR + ft * R0, R0);         // Flow: the walls slide through
        rr *= 1.0 + waveGain * waveAmp * sin(local * waveFreq + wavePhase);

        vec2 dir = vec2(cos(local + feed), sin(local + feed));
        vec2 sp  = dir * rr * zoom;
        sp.x /= aspect;
        srcUV = center + kal_toClient(sp) + srcOff;
    }
    else if (kmode == 19)                           // Strip Mirror (mirrored parallel bands)
    {
        float aA = outRot + feed;                             // Source Spin rotates the axis
        vec2 axis = vec2(cos(aA), sin(aA));
        vec2 perp = vec2(-axis.y, axis.x);
        float su = dot(p, axis);
        float sv = dot(p, perp);
        sv += kal_shape.w * 0.5 * sin(su * kal_fx.w + wavePhase * waveGain); // wavy strips

        float W  = 2.0 * maxR / max(segsF, 2.0);
        float uf = kal_foldRefl(su + ft * W, W);              // Flow: the bands slide

        vec2 sp = (axis * uf + perp * sv) * zoom;
        sp.x /= aspect;
        srcUV = center + kal_toClient(sp) + srcOff;
    }
    else if (kmode == 20)                           // Fly's Eye (compound-eye hex facets)
    {
        float freq = max(segsF, 2.0) * 0.5;
        vec2 hp = kal_rot(p, -outRot) * freq;
        vec2 lp = kal_hexLocal(hp);
        // facet id must be EXACT integers before hashing: hp - lp lands on
        // the cell centre only up to float rounding, so pixels of the SAME
        // facet got slightly different ids, and the hash amplified those
        // last-ulp differences into different tilts inside one facet ->
        // tearing lines whenever a parameter shifted the lattice. Hex
        // centres sit on exact multiples of the half lattice vector, so
        // round() recovers exact ids.
        const vec2 HEXS = vec2(1.0, 1.7320508);
        vec2 cellId = round((hp - lp) / (0.5 * HEXS));        // exact per-facet id

        // Big Facet Chance: hexes can't subdivide, but they CAN merge -
        // regions hash to a coarser lattice with double-size facets, like
        // the mixed facet sizes of a real compound eye. Merged facets get
        // their own id namespace so tilt / size scatter treat them as new
        // cells.
        if (kal_cell.w > 0.0)
        {
            vec2 hpB = hp * 0.5;
            vec2 lpB = kal_hexLocal(hpB);
            vec2 idB = round((hpB - lpB) / (0.5 * HEXS));
            if (kal_hashCell(idB + 71.3) < kal_cell.w)
            {
                lp = lpB * 2.0;          // coarse local coords in fine-lattice units
                cellId = idB + 4096.0;   // separate id namespace from fine facets
            }
        }
        cellKey = dot(cellId, vec2(1.0, 131.0));

        float h = kal_hashCell(cellId);
        float facetRot = (h - 0.5) * kal_fx.y * KAL_TAU       // hashed facet tilt
                       + ft * KAL_TAU * (h - 0.5);            // Flow: facets shimmer
        lp = kal_rot(lp, facetRot + feed);

        vec2 sp = lp / max(zoom * kal_cellScale(cellKey, ft), 0.05); // Source Zoom x per-facet scale
        sp.x /= aspect;
        srcUV = vec2(0.5, 0.5) + kal_toClient(sp) + srcOff;   // every facet re-images the frame
    }
    else if (kmode == 21)                           // Inversion (world turned inside-out)
    {
        float ang = kal_atan2(p.y, p.x) - outRot;
        float R0 = max(kal_fx.x * maxR, 1e-3);
        R0 *= 1.0 + 0.3 * kal_fx.y * sin(ft * KAL_TAU);       // Flow: the inversion breathes

        float inv   = R0 * R0 / max(rad, 1e-4);               // circle inversion
        float sampR = kal_foldRefl(inv, maxR) * zoom;         // keep it on-frame, mirrored
        ang += twist * radN;
        ang += waveGain * waveAmp * 4.0 * sin(radN * waveFreq + wavePhase);
        vec2 dir = vec2(cos(ang + feed), sin(ang + feed));
        vec2 sp  = dir * sampR;
        sp.x /= aspect;
        srcUV = center + kal_toClient(sp) + srcOff;
    }
    else if (kmode == 22)                           // Gem Facets (cut-stone sparkle)
    {
        float ang = kal_atan2(p.y, p.x) - outRot;
        float seg   = KAL_TAU / max(segsF, 2.0);
        float ringW = maxR / max(ringCountF, 1.0);
        float wIdx = floor(ang / seg);
        float rIdx = floor(rad / ringW);
        cellKey = dot(vec2(wIdx, rIdx), vec2(1.0, 131.0));

        float h1 = kal_hash(vec2(wIdx, rIdx) + 0.13);
        float h2 = kal_hash(vec2(rIdx, wIdx) + 0.71);
        float glint = sin(ft * KAL_TAU + h1 * KAL_TAU);       // Flow: facets glint in turn
        glint += waveGain * waveAmp * 8.0 * sin(wIdx * waveFreq + wavePhase);

        float sampA = ang + (h1 - 0.5) * kal_fx.y * 1.2
                    + glint * kal_fx.y * 0.15 + feed;
        float sampR = rad * (1.0 + (h2 - 0.5) * kal_fx.y * 0.35)
                    * (zoom / kal_cellScale(cellKey, ft));
        vec2 dir = vec2(cos(sampA), sin(sampA));
        vec2 sp  = dir * sampR;
        sp.x /= aspect;
        srcUV = center + kal_toClient(sp) + srcOff;
    }
    else if (kmode == 23)                           // Sawtooth Sun (asymmetric flame rays)
    {
        float ang = kal_atan2(p.y, p.x) - outRot;
        ang += twist * radN;
        float seg = KAL_TAU / max(segsF, 2.0);
        float su = ang / seg;
        float e = fract(su + ft);                             // Flow: teeth march around
        float k = exp(kal_shape.w * 2.0);                     // skew: leaning flame teeth
        e = e / max(e + (1.0 - e) * k, 1e-4);
        float local = e * seg;

        float sampR = rad * zoom;
        sampR *= 1.0 + waveGain * waveAmp * sin(local * waveFreq + wavePhase);
        vec2 dir = vec2(cos(local + feed), sin(local + feed));
        vec2 sp  = dir * sampR;
        sp.x /= aspect;
        srcUV = center + kal_toClient(sp) + srcOff;
    }
    else                                            // Radial Mirror / Rotate (modes 0 / 1)
    {
        float ang = kal_atan2(p.y, p.x) - outRot;
        ang += twist * radN;

        float seg   = KAL_TAU / max(segsF, 2.0);
        float local = kal_foldAngle(ang, seg, seamDist);

        float sampA = local + feed;
        float sampR = rad * zoom;
        sampR *= 1.0 + waveGain * waveAmp * sin(local * waveFreq + wavePhase);
        vec2 dir = vec2(cos(sampA), sin(sampA));
        vec2 sp  = dir * sampR;
        sp.x /= aspect;                             // un-correct aspect
        srcUV = center + kal_toClient(sp) + srcOff;
    }

    // single bilinear tap of the scene at the folded uv (the pattern is a
    // coordinate fold, not a gather -> stays full-res)
    vec2 fuv = kal_wrapUV(srcUV);
    vec3 col = textureLod(diffuseRect, fuv, 0.0).rgb;

    // Iridescence: hashed per-cell tint, like light catching each facet of
    // a cut surface differently. Mild by design (+-18% per channel at full).
    if (kal_cell2.x > 0.0 && cellKey > -1e4)
    {
        vec3 th = vec3(kal_hashCell(vec2(cellKey, 101.3)),
                       kal_hashCell(vec2(cellKey, 107.9)),
                       kal_hashCell(vec2(cellKey, 113.1)));
        col *= mix(vec3(1.0), 0.82 + 0.36 * th, kal_cell2.x);
    }

    // seam soften: feather the mirror lines (TAA crawl on a locked camera).
    // Radial Mirror only; the other modes are seamless folds or
    // intentionally hard-edged. The 2 extra taps are the only non-single-tap
    // path in this shader.
    if (kal_look.x > 1e-4 && kmode == 0)
    {
        float fb = kal_look.x * 0.04;
        float w  = 1.0 - smoothstep(0.0, fb, seamDist);   // 1 at the seam
        if (w > 1e-3)
        {
            vec2 off = (1.0 / screen_res) * (1.0 + kal_look.x * 3.0);
            vec3 a = textureLod(diffuseRect, kal_wrapUV(srcUV + off), 0.0).rgb;
            vec3 b = textureLod(diffuseRect, kal_wrapUV(srcUV - off), 0.0).rgb;
            col = mix(col, 0.5 * (a + b), w * 0.5);
        }
    }

    // hold the subject over the kaleidoscope (disc and/or depth mask)
    float hold = 0.0;
    int pmode = int(kal_protect.x + 0.5);
    if (pmode > 0)
    {
        hold = kal_holdMask(uv, pradN);
        vec3 orig = textureLod(diffuseRect, uv, 0.0).rgb;
        col = mix(col, orig, hold);                 // hold = 1 -> subject in front
    }

    // master A/B against the untouched frame
    float blendW = clamp(kal_look.y, 0.0, 1.0);
    if (blendW < 0.999)
    {
        vec3 orig = textureLod(diffuseRect, uv, 0.0).rgb;
        col = mix(orig, col, blendW);
    }

    // warp map: fold uv where the fold is visible, pulled back to the
    // identity uv by the same weight the original frame is pulled back on
    // top (hold mask, master blend) - keeps the ReShade depth re-warp
    // aligned with what is actually on screen. Computed before the debug
    // views (setup aids; not a capture path).
    vec2 warpUV = mix(fuv, uv, max(hold, 1.0 - blendW));

    // ---- setup views ------------------------------------------------------
    int dv = int(kal_look.z + 0.5);
    if (dv == 1)                                    // Wedges
    {
        float ang = kal_atan2(p.y, p.x) - outRot;
        float seg = KAL_TAU / max(segsF, 2.0);
        float idx = floor(ang / seg);
        float sel = fract(idx * 0.5) > 0.25 ? 1.0 : 0.0;
        vec3 wmap = mix(vec3(0.15, 0.35, 0.55), vec3(0.55, 0.30, 0.15), sel);
        col = mix(col, wmap, 0.45);
    }
    else if (dv == 2)                               // Center / Protect
    {
        // ring around the ACTUAL protect centre (pradN) - the reference drew
        // it around the pattern centre, wrong under Fixed/Focus anchors
        float ring = smoothstep(0.004, 0.0, abs(pradN - kal_protect.y));
        float band = smoothstep(0.004, 0.0, abs(radN - kal_fx.x));  // Subject Band preview
        float mdot = smoothstep(0.02, 0.0, rad);
        vec3 mark  = vec3(0.1, 1.0, 0.3);
        vec3 markB = vec3(1.0, 0.8, 0.1);
        col = mix(col, markB, clamp(band * ((kmode >= 9) ? 1.0 : 0.0), 0.0, 1.0));
        col = mix(col, mark, clamp(ring + mdot, 0.0, 1.0));
    }
    else if (dv == 3)                               // Subject Mask
    {
        float h;
        if (pmode > 0)
        {
            h = hold;                               // full effective hold (disc + depth)
        }
        else
        {
            // depth preview for calibration (meters, native)
            float distM = max(-getPosition(uv).z, 0.01);
            h = 1.0 - smoothstep(kal_depth.x - kal_depth.y,
                                 kal_depth.x + kal_depth.y, distM);
            if (kal_protect.w > 0.5) h = 1.0 - h;
        }
        // white = subject (held in front), black = kaleidoscope background
        col = vec3(h);
    }

    frag_data[0] = vec4(clamp(col, 0.0, 1.0), 1.0);
    frag_data[1] = vec4(0.0);
    frag_data[2] = vec4(warpUV, 0.0, 0.0);
}
