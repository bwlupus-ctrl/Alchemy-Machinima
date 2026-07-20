/**
 * @file actorghostF.glsl
 * @brief Pose-ghost FX fragment shader (hologram / x-ray ghost styles).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * [ActorMover] Constant colour * diffuse, shaped by three tunable terms plus a
 * batch-alpha stage so ONE program covers EVERY ghost style (with the params
 * zeroed it reduces exactly to highlightF.glsl's color * texture):
 *
 *   scanlines -- animated horizontal bands in SCREEN space (gl_FragCoord.y),
 *                so they stay stable across skinning/pose and read instantly
 *                as a "hologram projection";
 *   rim       -- fresnel-ish silhouette boost from the eye-space normal
 *                (brightest where the surface turns away from the eye);
 *   flicker   -- subtle whole-body shimmer from two incommensurate sines
 *                (never visibly loops, gentle enough to not strobe);
 *   alpha     -- ghostAux.x is a minimum-alpha DISCARD (the ghost draw feeds
 *                each alpha-MASKED batch its authored cutoff, so masked hair
 *                sheets / lace punch real holes instead of drawing solid) and
 *                ghostAux.y picks whether the texture's RGB shows (1, the
 *                clone) or only the flat tint does (0, every other style --
 *                masked/blended batches still bind their real texture for its
 *                ALPHA channel). tex.a always scales the output alpha; opaque
 *                batches are bound to plain white (tex.a == 1), so this only
 *                bites where the real render also honors the alpha channel.
 *
 * Uniforms the ghost draw feeds per style (see drawGeometryGhost):
 *   ghostTime   -- seconds, the frame clock; drives the scroll + flicker.
 *   ghostParams -- x scanline strength 0..1, y rim strength, z flicker
 *                  strength 0..1, w scanline period in pixels.
 *   ghostAux    -- x minimum-alpha discard cutoff (0 keeps every texel),
 *                  y texture-RGB mix 0..1, z pixelation block size in screen
 *                  pixels (0 = off), w per-instance FX phase (radians).
 *   ghostFx     -- [GhostStudio] per-instance creative FX, all 0 = off:
 *                  x shimmer speed Hz, y shimmer intensity 0..1 (brightness/
 *                  alpha wobble), z glitch amount 0..1 (slice offset + chroma
 *                  split), w reserved.
 * Hologram = scan+rim+flicker on; x-ray = rim only; ghost/clone/wireframe =
 * all zero (plain colour * texture with the alpha stage).
 */

out vec4 frag_color;

uniform vec4 color;
uniform sampler2D diffuseMap;
uniform float ghostTime;
uniform vec4 ghostParams;
uniform vec4 ghostAux;
uniform vec4 ghostFx;

in vec2 vary_texcoord0;
in vec3 vary_position;
in vec3 vary_normal;

// cheap stable hash for the glitch bands (classic one-liner)
float ghost_hash(vec2 p)
{
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
    vec2 uv = vary_texcoord0.xy;

    // [GhostStudio] pixelation: quantize the sampling UV in ~screen-pixel
    // blocks. fwidth(uv) is the UV footprint of ONE screen pixel here, so
    // multiplying by the block size snaps the texture into blocks that stay
    // screen-sized regardless of zoom. Reads strongest on the textured clone;
    // flat-tint styles only show it through the alpha/mask stage.
    if (ghostAux.z > 0.5)
    {
        vec2 px = max(fwidth(uv) * ghostAux.z, vec2(1e-6));
        uv = (floor(uv / px) + 0.5) * px;
    }

    // [GhostStudio] glitch: a few horizontal screen bands tear sideways for a
    // frame-ish beat (time-quantized so slices hold, not swim), scaled by the
    // amount. The active band also chroma-splits below.
    float glitch = clamp(ghostFx.z, 0.0, 1.0);
    float torn = 0.0;
    if (glitch > 0.001)
    {
        float band_id = floor(gl_FragCoord.y / 14.0);
        float beat    = floor(ghostTime * 9.0);
        float r       = ghost_hash(vec2(band_id, beat + ghostAux.w));
        torn = step(1.0 - 0.35 * glitch, r);            // a minority of bands tear
        uv.x += torn * (r - 0.5) * 0.22 * glitch;       // sideways slice offset
    }

    vec4 tex = texture(diffuseMap, uv);
    if (torn > 0.0)
    {
        // chroma split on the torn slice: R and B sampled a hair apart
        vec2 split = vec2(0.006 * glitch, 0.0);
        tex.r = texture(diffuseMap, uv + split).r;
        tex.b = texture(diffuseMap, uv - split).b;
    }

    // alpha-mask cutoff, exactly like the real render's masked passes. With
    // ghostAux.x == 0 no texel can be below the cutoff, so the branch is free
    // for opaque/blend batches.
    if (tex.a < ghostAux.x)
    {
        discard;
    }

    // animated screen-space scanlines, scrolling slowly upward
    float scan_amt = clamp(ghostParams.x, 0.0, 1.0);
    float period   = max(ghostParams.w, 2.0);
    float band     = 0.5 + 0.5 * sin((gl_FragCoord.y / period + ghostTime * 1.7) * 6.2831853);
    float scan     = mix(1.0, 0.30 + 0.70 * band, scan_amt);

    // fresnel-ish rim from the eye-space normal / view direction
    vec3  n   = normalize(vary_normal);
    vec3  v   = normalize(-vary_position);
    float rim = pow(1.0 - clamp(abs(dot(n, v)), 0.0, 1.0), 2.0) * ghostParams.y;

    // subtle whole-body flicker (13 Hz-ish beat against 7.3, capped at -30%)
    float flicker = 1.0 - 0.30 * clamp(ghostParams.z, 0.0, 1.0)
                        * (0.5 + 0.5 * sin(ghostTime * 13.0) * sin(ghostTime * 7.3));

    // [GhostStudio] shimmer: adjustable brightness/alpha wobble -- the
    // parameterized cousin of the hologram flicker, phase-offset per instance
    // so a crowd of ghosts never strobes in lockstep. Intensity 0 = exactly 1.
    flicker *= 1.0 - clamp(ghostFx.y, 0.0, 1.0)
                   * (0.5 + 0.5 * sin(ghostTime * max(ghostFx.x, 0.0) * 6.2831853
                                      + ghostAux.w)) * 0.6;
    // a torn glitch slice also pops brighter for that broken-signal read
    flicker *= 1.0 + torn * 0.35 * glitch;

    // texture RGB shows per ghostAux.y (clone = 1); the tint always multiplies
    vec3 base = mix(vec3(1.0), tex.rgb, clamp(ghostAux.y, 0.0, 1.0)) * color.rgb;

    // scanlines dim the body colour; the rim ADDS glow on top (and lifts the
    // alpha, so an x-ray body is faint inside with bright silhouette edges)
    vec3  rgb   = base * scan * flicker + color.rgb * rim;
    float alpha = clamp(color.a * tex.a * (1.0 - scan_amt * (0.4 - 0.4 * band)) * flicker
                        + rim * 0.5, 0.0, 1.0);
    frag_color = max(vec4(rgb, alpha), vec4(0));
}
