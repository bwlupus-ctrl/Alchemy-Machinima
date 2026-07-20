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
 *                  y texture-RGB mix 0..1, zw reserved.
 * Hologram = scan+rim+flicker on; x-ray = rim only; ghost/clone/wireframe =
 * all zero (plain colour * texture with the alpha stage).
 */

out vec4 frag_color;

uniform vec4 color;
uniform sampler2D diffuseMap;
uniform float ghostTime;
uniform vec4 ghostParams;
uniform vec4 ghostAux;

in vec2 vary_texcoord0;
in vec3 vary_position;
in vec3 vary_normal;

void main()
{
    vec4 tex = texture(diffuseMap, vary_texcoord0.xy);

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

    // texture RGB shows per ghostAux.y (clone = 1); the tint always multiplies
    vec3 base = mix(vec3(1.0), tex.rgb, clamp(ghostAux.y, 0.0, 1.0)) * color.rgb;

    // scanlines dim the body colour; the rim ADDS glow on top (and lifts the
    // alpha, so an x-ray body is faint inside with bright silhouette edges)
    vec3  rgb   = base * scan * flicker + color.rgb * rim;
    float alpha = clamp(color.a * tex.a * (1.0 - scan_amt * (0.4 - 0.4 * band)) * flicker
                        + rim * 0.5, 0.0, 1.0);
    frag_color = max(vec4(rgb, alpha), vec4(0));
}
