/**
 * @file ultimateDiopterF.glsl
 *
 * [Ultimate Diopter] Pass 2 of 2: composite.
 * Reads the pass-1 MRT (clear gather + mask, diopter gather + rim), applies
 * seam double-image, halo ghost copies with soft-knee highlight energy and
 * spectral dispersion, lateral chromatic aberration, edge vignette, rim
 * caustic/darken, debug views, and the master A/B blend against the
 * untouched frame. Output is display-space; the final present pass dithers.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2026, Linden Research, Inc.
 * $/LicenseInfo$
 */

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;

in vec2 vary_fragcoord;

uniform sampler2D diffuseRect;   // pass-1 A: clear RGB (linear) + mask
uniform sampler2D emissiveRect;  // pass-1 B: diopter RGB (linear) + rim
uniform sampler2D diffuseMap;    // untouched source frame (display space)
uniform vec2 screen_res;

// center.xy (uv), size, stretch
uniform vec4 diopter_shape;
// angleRad, feather, invert, content
uniform vec4 diopter_shape2;
// baseFocusM, lensFocusM, focusWidthM, falloffRate  (debug focus map)
uniform vec4 diopter_focus;
// seamGhostPx, seamGhostAmt, caMag, edgeVig
uniform vec4 diopter_comp;
// rimCaustic, rimDarken, blend, debugView
uniform vec4 diopter_comp2;
// ghostCount, ghostSpacing, radialSmear, tangentSmear
uniform vec4 diopter_ghost;
// ghostThreshold, ghostKnee, dispersion, ghostGain
uniform vec4 diopter_ghost2;

// deferredUtil.glsl is linked in as a separate compile unit (isDeferred), so
// its functions must be forward-declared here or this unit fails to compile
vec4 getPosition(vec2 pos_screen);   // view space, managed inv_proj

const float UD_TAU  = 6.2831853;
const vec3  UD_LUMA = vec3(0.2126, 0.7152, 0.0722);
const int   UD_MAX_GHOSTS = 6;

float ud_sat(float x) { return clamp(x, 0.0, 1.0); }

vec3 ud_delin(vec3 c) { return pow(max(c, vec3(0.0)), vec3(1.0 / 2.2)); }

float ud_mirror1(float x)
{
    float t = fract(x * 0.5) * 2.0;
    return 1.0 - abs(t - 1.0);
}
vec2 ud_mirror2(vec2 v) { return vec2(ud_mirror1(v.x), ud_mirror1(v.y)); }

float ud_radiusNorm(vec2 uv)
{
    float aspect = screen_res.x / screen_res.y;
    vec2 dd = uv - diopter_shape.xy;
    dd.x *= aspect;
    float denom = max(length(vec2(aspect * 0.5, 0.5)), 1e-4);
    return ud_sat(length(dd) / denom);
}

vec2 ud_rot(vec2 v, float a)
{
    float s = sin(a), c = cos(a);
    return vec2(v.x * c - v.y * s, v.x * s + v.y * c);
}

void main()
{
    vec2 uv = vary_fragcoord.xy;
    float aspect = screen_res.x / screen_res.y;

    vec4 a = texture(diffuseRect, uv);
    vec3 cClear = a.rgb;
    float m = a.a;
    vec4 b = texture(emissiveRect, uv);
    vec3 cDio = b.rgb;
    float rimW = b.a;

    // ---- seam double-image (optical doubling in the transition band) -----
    if (diopter_comp.x > 1e-4)
    {
        float ar = diopter_shape2.x;
        vec2 n = vec2(cos(ar), -sin(ar)) * diopter_comp.x / screen_res;
        float band = ud_sat(4.0 * m * (1.0 - m));
        vec3 gDio   = textureLod(emissiveRect, ud_mirror2(uv + n), 0.0).rgb;
        vec3 gClear = textureLod(diffuseRect, ud_mirror2(uv - n), 0.0).rgb;
        vec3 ghost  = max(0.5 * (gDio + gClear), 0.5 * (cClear + cDio));
        cClear = mix(cClear, ghost, band * diopter_comp.y);
        cDio   = mix(cDio,   ghost, band * diopter_comp.y);
    }

    vec3 lin = mix(cClear, cDio, m);

    // ---- halo ghost copies: rotated/offset re-samples of the glass with
    //      soft-knee highlight extraction and spectral dispersion ----------
    int ghostCount = int(diopter_ghost.x + 0.5);
    if (ghostCount > 0 && m > 0.001)
    {
        vec2 p = uv - diopter_shape.xy;
        p.x *= aspect;

        float spacing = diopter_ghost.y;
        float disp = diopter_ghost2.z;

        for (int i = 0; i < UD_MAX_GHOSTS; i++)
        {
            if (i >= ghostCount) break;
            float k = float(i + 1);

            // tangential arc rotation + radial scale per ghost
            float rotK = diopter_ghost.w * 0.35 * k;
            float radK = 1.0 + diopter_ghost.z * 0.06 * k * spacing;
            vec2 gp = ud_rot(p, rotK) * radK;

            vec2 gvec = vec2(gp.x / aspect, gp.y);
            vec2 gUV  = diopter_shape.xy + gvec;
            vec2 gUVr = diopter_shape.xy + gvec * (1.0 + disp * k * 0.02);
            vec2 gUVb = diopter_shape.xy + gvec * (1.0 - disp * k * 0.02);

            vec3 g;
            g.r = textureLod(emissiveRect, ud_mirror2(gUVr), 0.0).r;
            g.g = textureLod(emissiveRect, ud_mirror2(gUV),  0.0).g;
            g.b = textureLod(emissiveRect, ud_mirror2(gUVb), 0.0).b;

            // soft-knee highlight energy
            float lum = dot(g, UD_LUMA);
            float t = max(lum - diopter_ghost2.x, 0.0);
            float e = t * t / (t + diopter_ghost2.y + 1e-4);

            float w = diopter_ghost2.w * pow(0.7, k - 1.0) * m;
            lin += g * e * w;
        }
    }

    // ---- lateral chromatic aberration, concentrated into the rim band ----
    float caEff = diopter_comp.z * (1.0 + rimW);
    if (caEff > 1e-5)
    {
        vec2 d = uv - diopter_shape.xy;
        vec2 uvR = ud_mirror2(diopter_shape.xy + d * (1.0 + caEff));
        vec2 uvB = ud_mirror2(diopter_shape.xy + d * (1.0 - caEff));
        vec3 ca;
        ca.r = mix(textureLod(diffuseRect, uvR, 0.0).r,
                   textureLod(emissiveRect, uvR, 0.0).r, m);
        ca.g = lin.g;
        ca.b = mix(textureLod(diffuseRect, uvB, 0.0).b,
                   textureLod(emissiveRect, uvB, 0.0).b, m);
        lin = mix(lin, ca, ud_sat(m));
    }

    // ---- edge optical vignette (single-element glass) --------------------
    float rn = ud_radiusNorm(uv);
    float vig = 1.0 - diopter_comp.w * rn * rn * ud_sat(m);
    lin *= ud_sat(vig);

    // ---- glass rim: soft edge light + contact darken ----------------------
    float rimBand = rimW * rimW;
    lin *= 1.0 - diopter_comp2.y * rimBand;
    lin += diopter_comp2.x * rimBand * 0.4;

    vec3 disp_out = ud_delin(lin);

    // ---- master A/B blend against the untouched frame --------------------
    vec3 orig = texture(diffuseMap, uv).rgb;
    if (diopter_comp2.z < 0.999)
    {
        disp_out = mix(orig, disp_out, ud_sat(diopter_comp2.z));
    }

    // ---- setup / debug views ---------------------------------------------
    int dbg = int(diopter_comp2.w + 0.5);
    if (dbg == 1)        // region mask
    {
        disp_out = mix(disp_out, vec3(1.0, 0.25, 0.25), m * 0.45);
    }
    else if (dbg == 2)   // focus map: green sharp, blue near-blur, red far-blur
    {
        float distM = max(-getPosition(uv).z, 0.01);
        float plane = mix(diopter_focus.x, diopter_focus.y, m);
        float diff = distM - plane;
        float dead = diopter_focus.z * 0.5;
        float rate = diopter_focus.w / max(plane, 0.5);
        float sharp = ud_sat(1.0 - max(abs(diff) - dead, 0.0) * rate);
        float nearB = ud_sat(-diff * rate);
        float farB  = ud_sat( diff * rate);
        disp_out = mix(disp_out * 0.35, vec3(farB, sharp, nearB), 0.85);
    }
    else if (dbg == 3)   // depth (meters / 50)
    {
        float distM = max(-getPosition(uv).z, 0.01);
        disp_out = vec3(ud_sat(distM / 50.0));
    }
    else if (dbg == 4)   // rim + coverage envelope
    {
        disp_out = vec3(rimW, m, ud_sat(4.0 * m * (1.0 - m)));
    }

    frag_color = vec4(clamp(disp_out, 0.0, 1.0), 1.0);
}
