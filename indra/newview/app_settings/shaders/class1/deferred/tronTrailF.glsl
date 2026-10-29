/**
 * @file tronTrailF.glsl
 *
 * [TronT3] Light-cycle trail ribbons: fragment stage. One program serves the
 * three trail styles (0 vertical light wall, 1 ground streak, 2 camera-
 * facing ribbon); the host builds the strip geometry per style and this
 * shader only shapes the ACROSS profile:
 *   wall          translucent body brightening toward the top + a bright cap
 *                 along the top edge (jetwall look);
 *   ground/camera bright core line down the centre + soft falloff to both
 *                 edges.
 * Colour is a per-trail uniform (Tron palette, linear); brightness is
 * display-intent and made exposure-independent on screen by the SAME single
 * exposure rule the Tron World pass uses (tronWorldF.glsl ~745-757 /
 * 986-995): no-post scale, optional clamp to display range, then divide ONCE
 * by the effective exposure E, then the 65000 clamp.
 *
 * Blend state (host, see contract section 1.3): colour ADDITIVE
 * (ONE, ONE), alpha (ONE, ONE_MINUS_SRC_ALPHA) -- so frag_color.a is the
 * exposure-stable glow-tag feed (TronTrailGlow x fade x profile) and the
 * union of overlapping ribbons stays in [0,1].
 *
 * Fog: mode 0 none; mode 1 analytic exp(-density * z_view) toward black
 * (WindLight forward fog is a later phase; params3.zw are reserved for it).
 *
 * Every uniform is uploaded by ALTron::renderTrails() on EVERY draw -- this
 * program never relies on GL's default-zero uniforms; the true off path is
 * "never bound" (TronTrailEnabled off / no geometry -> the host skips the
 * whole trail block, no depth blit, no bind).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

out vec4 frag_color;

in vec2  vary_texcoord0;   // x = fade01 (1 fresh .. 0 dead), y = across01 (0 base / edge A .. 1 top / edge B)
in float vary_zc;          // view-space depth (m, >= 0)

uniform sampler2D exposureMap;      // 1x1 auto-exposure scale (bound by the host: EXPOSURE_MAP channel)
uniform float     exposure;         // signed selector: > 0 manual x auto, < 0 manual only (S-Log3 lock), 0 -> E = 1

// [TronT3] uploaded by ALTron::renderTrails(); packing per tron_t3_contract.md section 1.2
uniform vec4 tron_trail_color;      // rgb = palette colour (linear), w = glow feed 0..1 (TronTrailGlow)
uniform vec4 tron_trail_params;     // x edge/core width (fraction of across, 0.02..0.5), y edge gain, z body gain, w fade exponent
uniform vec4 tron_trail_params2;    // x intensity (display-intent linear), y style 0/1/2, z no-post scale, w clamp-display flag
uniform vec4 tron_trail_params3;    // x fog mode (0 none / 1 analytic), y fog density 1/m, z reserved, w reserved

void main()
{
    float fade01 = clamp(vary_texcoord0.x, 0.0, 1.0);
    float across = clamp(vary_texcoord0.y, 0.0, 1.0);
    float fade   = pow(fade01, max(tron_trail_params.w, 0.01));          // base in [0,1], exponent > 0
    float ew     = clamp(tron_trail_params.x, 0.02, 0.5);                // smoothstep edges: 0 < ew always

    float body;
    float edge;
    if (tron_trail_params2.y < 0.5)
    {
        // style 0: light wall -- translucent body brightening toward the top,
        // bright cap on the top edge
        body = tron_trail_params.z * mix(0.5, 1.0, across);
        edge = 1.0 - smoothstep(0.0, ew, 1.0 - across);
    }
    else
    {
        // styles 1/2: ground streak / camera ribbon -- bright core line, soft
        // falloff to both edges
        float d = abs(across - 0.5) * 2.0;                               // 0 centre .. 1 edge
        float s = 1.0 - d;
        body = tron_trail_params.z * s * s;
        edge = 1.0 - smoothstep(0.0, ew, d);
    }
    float profile = body + edge * max(tron_trail_params.y, 0.0);

    float fogf = 1.0;
    if (tron_trail_params3.x > 0.5)
    {
        fogf = exp(-max(tron_trail_params3.y, 0.0) * vary_zc);
    }

    vec3 neon = tron_trail_color.rgb * max(tron_trail_params2.x, 0.0) * fade * profile * fogf;

    // ---- ONE exposure rule (identical to tronWorldF.glsl) ------------------
    neon *= max(tron_trail_params2.z, 0.0);                              // no-post frames: scale, then clamp
    if (tron_trail_params2.w > 0.5)
    {
        neon = min(neon, vec3(1.0));
    }
    //   exposure > 0 : E = exposureMap * exposure   (manual * auto adaptation)
    //   exposure < 0 : E = -exposure                (manual iris only: S-Log3 lock)
    //   exposure == 0: E = 1                        (no-post)
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

    vec3  outc  = min(neon / E, vec3(65000.0));
    float out_a = clamp(tron_trail_color.w, 0.0, 1.0) * fade * min(profile, 1.0) * fogf;
    frag_color = vec4(outc, clamp(out_a, 0.0, 1.0));
}
