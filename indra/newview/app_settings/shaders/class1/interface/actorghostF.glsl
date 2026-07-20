/**
 * @file actorghostF.glsl
 * @brief Pose-ghost FX fragment shader (hologram / x-ray ghost styles).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 *
 * [ActorMover] Constant colour * diffuse (the ghost draw binds white), shaped
 * by three tunable terms so ONE program covers both FX ghost styles:
 *
 *   scanlines -- animated horizontal bands in SCREEN space (gl_FragCoord.y),
 *                so they stay stable across skinning/pose and read instantly
 *                as a "hologram projection";
 *   rim       -- fresnel-ish silhouette boost from the eye-space normal
 *                (brightest where the surface turns away from the eye);
 *   flicker   -- subtle whole-body shimmer from two incommensurate sines
 *                (never visibly loops, gentle enough to not strobe).
 *
 * Uniforms the ghost draw feeds per style (see drawGeometryGhost):
 *   ghostTime   -- seconds, the frame clock; drives the scroll + flicker.
 *   ghostParams -- x scanline strength 0..1, y rim strength, z flicker
 *                  strength 0..1, w scanline period in pixels.
 * Hologram = all three on; x-ray = rim only (scan/flicker zero).
 */

out vec4 frag_color;

uniform vec4 color;
uniform sampler2D diffuseMap;
uniform float ghostTime;
uniform vec4 ghostParams;

in vec2 vary_texcoord0;
in vec3 vary_position;
in vec3 vary_normal;

void main()
{
    vec4 tex = texture(diffuseMap, vary_texcoord0.xy);

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

    // scanlines dim the body colour; the rim ADDS glow on top (and lifts the
    // alpha, so an x-ray body is faint inside with bright silhouette edges)
    vec3  rgb   = (color.rgb * tex.rgb) * scan * flicker + color.rgb * rim;
    float alpha = clamp(color.a * (1.0 - scan_amt * (0.4 - 0.4 * band)) * flicker
                        + rim * 0.5, 0.0, 1.0);
    frag_color = max(vec4(rgb, alpha), vec4(0));
}
