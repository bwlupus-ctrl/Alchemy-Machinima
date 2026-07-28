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
 *   ghostFx     -- [GhostStudio] per-instance creative FX (xyz all 0 = off):
 *                  x shimmer speed Hz, y shimmer intensity 0..1 (brightness/
 *                  alpha wobble), z glitch amount 0..1 (slice offset + chroma
 *                  split), w OUTPUT BRIGHTNESS multiplier [R2-1] (1 = as-is;
 *                  the draw always uploads it -- the ghost is unlit in the
 *                  post-tonemap overlay, so this is how a clone sits into a
 *                  night scene instead of glowing fullbright).
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
uniform int ghostLook;
// Independent, orthogonal distortion layer. x=strength, yz=lens center.
uniform int ghostDistort;
uniform vec4 ghostDistortParams;
// [R2-2] indexed-batch slot filter: >= 0 draws ONLY fragments whose
// per-vertex material slot matches (the clone re-draws a multi-material
// batch once per slot with that slot's texture bound); -1 = no filtering.
uniform int ghostSlot;

in vec2 vary_texcoord0;
in vec3 vary_position;
in vec3 vary_normal;
flat in int vary_texture_index;
in vec4 vary_vertex_color;

// cheap stable hash for the glitch bands (classic one-liner)
float ghost_hash(vec2 p)
{
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

float ghost_noise(vec2 p)
{
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(ghost_hash(i), ghost_hash(i + vec2(1.0, 0.0)), f.x),
               mix(ghost_hash(i + vec2(0.0, 1.0)),
                   ghost_hash(i + vec2(1.0, 1.0)), f.x), f.y);
}

float ghost_fbm(vec2 p)
{
    float v = 0.0;
    v += 0.500 * ghost_noise(p); p = p * 2.03 + 17.1;
    v += 0.250 * ghost_noise(p); p = p * 2.01 + 11.7;
    v += 0.125 * ghost_noise(p);
    return v / 0.875;
}

vec3 heat_lut(float x)
{
    x = clamp(x, 0.0, 1.0);
    vec3 c = mix(vec3(0.02, 0.05, 0.55), vec3(0.0, 0.85, 0.35),
                 smoothstep(0.0, 0.45, x));
    c = mix(c, vec3(1.0, 0.9, 0.05), smoothstep(0.42, 0.72, x));
    return mix(c, vec3(1.0, 0.03, 0.0), smoothstep(0.72, 1.0, x));
}

vec3 ghost_rainbow(float h)
{
    vec3 p = abs(fract(h + vec3(0.0, 0.6667, 0.3333)) * 6.0 - 3.0);
    return clamp(p - 1.0, 0.0, 1.0);
}

void main()
{
    // [R2-2] indexed-batch slot filter (see ghostSlot above)
    if (ghostSlot >= 0 && vary_texture_index != ghostSlot)
    {
        discard;
    }

    vec2 uv = vary_texcoord0.xy;
    float distort = clamp(ghostDistortParams.x, 0.0, 1.0);
    float voxelShade = 1.0;
    float vhsBand = 0.0;

    // Distortions alter the common sampling coordinates before any look is
    // evaluated. Thus every look consumes the same distorted source sample.
    if (ghostDistort == 1 && distort > 0.001) // Pixelate
    {
        vec2 px = max(fwidth(uv) * mix(2.0, 64.0, distort), vec2(1e-6));
        uv = (floor(uv / px) + 0.5) * px;
    }
    else if (ghostDistort == 2 && distort > 0.001) // Voxel
    {
        float cells = mix(80.0, 10.0, distort);
        vec3 cell = floor(vary_position * cells) / cells;
        vec2 px = max(fwidth(uv) * mix(3.0, 38.0, distort), vec2(1e-6));
        uv = (floor(uv / px) + 0.5) * px;
        voxelShade = 0.68 + 0.32 * ghost_hash(cell.xy + cell.z);
    }
    else if (ghostDistort == 3 && distort > 0.001) // Lens / magnify
    {
        vec2 center = ghostDistortParams.yz;
        vec2 d = uv - center;
        float radius = mix(0.28, 0.48, distort);
        float q = length(d) / radius;
        if (q < 1.0)
            uv = center + d * mix(0.48, 0.88, q * q) * distort
                        + d * (1.0 - distort);
    }
    else if (ghostDistort == 4 && distort > 0.001) // Wave / ripple
    {
        uv += vec2(sin(uv.y * 42.0 + ghostTime * 4.2 + ghostAux.w),
                   cos(uv.x * 35.0 - ghostTime * 3.4)) * (0.002 + 0.025 * distort);
    }
    else if (ghostDistort == 6 && distort > 0.001) // Block glitch
    {
        float beat = floor(ghostTime * 7.0);
        vec2 block = floor(uv * vec2(12.0, 22.0));
        float r = ghost_hash(block + vec2(beat, ghostAux.w));
        uv.x += step(1.0 - 0.38 * distort, r) * (r - 0.5) * 0.28 * distort;
    }
    else if (ghostDistort == 7 && distort > 0.001) // Vertical tear
    {
        float slice = floor(gl_FragCoord.x / 13.0);
        float beat = floor(ghostTime * 8.0);
        float r = ghost_hash(vec2(slice, beat + ghostAux.w));
        uv.y += step(1.0 - 0.35 * distort, r) * (r - 0.5) * 0.24 * distort;
    }
    else if (ghostDistort == 8 && distort > 0.001) // VHS / tracking
    {
        float roll = fract(gl_FragCoord.y / 180.0 - ghostTime * 0.32);
        vhsBand = 1.0 - smoothstep(0.02, 0.12, abs(roll - 0.5));
        float lineNoise = ghost_hash(vec2(floor(gl_FragCoord.y / 3.0),
                                           floor(ghostTime * 12.0)));
        uv.x += (lineNoise - 0.5) * 0.035 * distort + vhsBand * 0.02 * distort;
    }
    // Clamp ONLY when a distortion actually offset the UV. Applying it to the
    // undistorted base sample corrupts legitimately tiled/wrapped textures
    // (fishnet, repeating patterns): it collapses their repeats onto the edge
    // texel, so those parts pick up the edge's alpha and drop out. The Clone and
    // any no-distortion look must wrap exactly as authored.
    // No UV clamp here. The texture's own wrap mode (repeat/clamp) handles any
    // out-of-[0,1] UVs the distortions produce -- exactly as the real render does,
    // and as this shader did before the distortion layer was added. The layer's
    // original unconditional clamp collapsed tiled/wrapped textures (fishnet,
    // repeating patterns) onto the edge texel, dropping those parts to the edge's
    // alpha (the Clone regression), and did not even bound the final UV because the
    // legacy pixelate/glitch below still move it. Wrapped sampling is the authored,
    // correct behavior for every look and distortion.

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
    if (ghostDistort == 5 && distort > 0.001) // RGB split
    {
        vec2 split = vec2(0.003 + 0.018 * distort, 0.0);
        tex.r = texture(diffuseMap, uv + split).r;
        tex.b = texture(diffuseMap, uv - split).b;
    }
    else if (ghostDistort == 8 && distort > 0.001)
    {
        vec2 split = vec2(0.002 + vhsBand * 0.008, 0.0) * distort;
        tex.r = texture(diffuseMap, uv + split).r;
        tex.b = texture(diffuseMap, uv - split).b;
        float mono = dot(tex.rgb, vec3(0.299, 0.587, 0.114));
        tex.rgb = mix(tex.rgb, vec3(mono), 0.25 * distort);
    }
    tex.rgb *= voxelShade;
    if (torn > 0.0)
    {
        // chroma split on the torn slice: R and B sampled a hair apart
        vec2 split = vec2(0.006 * glitch, 0.0);
        tex.r = texture(diffuseMap, uv + split).r;
        tex.b = texture(diffuseMap, uv - split).b;
    }

    // [R2-4] per-vertex colour: the editor's TE tint (rgb) and transparency
    // (a) on legacy faces, folded into the sample so every consumer below --
    // clone RGB, the flat-tint styles' alpha, and the mask discard -- sees it
    // exactly like the real render does. White (the parked generic) for PBR
    // and untinted faces = byte-identical.
    tex *= vary_vertex_color;

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
    // alpha, so an x-ray body is faint inside with bright silhouette edges).
    // [R2-1] the per-instance brightness scales the WHOLE output colour (rim
    // included) but never the alpha -- a dimmed clone stays as opaque.
    vec3  rgb   = base * scan * flicker + color.rgb * rim;
    float alpha = clamp(color.a * tex.a * (1.0 - scan_amt * (0.4 - 0.4 * band)) * flicker
                        + rim * 0.5, 0.0, 1.0);

    float lum = dot(tex.rgb, vec3(0.299, 0.587, 0.114));
    float facing = clamp(abs(dot(n, v)), 0.0, 1.0);
    float edge = pow(1.0 - facing, 2.0);
    if (ghostLook == 5) // Thermal
    {
        rgb = mix(heat_lut(clamp(lum + edge * 0.28, 0.0, 1.0)), color.rgb, 0.12);
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 6) // Neon outline
    {
        rgb = color.rgb * edge * 3.0;
        alpha = color.a * smoothstep(0.12, 0.75, edge);
    }
    else if (ghostLook == 7) // Silhouette
    {
        rgb = color.rgb;
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 8) // Toon / ink
    {
        float bands = 4.0;
        vec3 poster = floor(tex.rgb * bands + 0.5) / bands;
        float ink = smoothstep(0.18, 0.62, edge);
        rgb = mix(poster * color.rgb, vec3(0.015), ink);
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 9) // Chrome
    {
        vec3 r = reflect(-v, n);
        float stripes = 0.5 + 0.5 * sin((r.y * 2.4 + r.x) * 3.1415927);
        rgb = mix(vec3(0.05, 0.08, 0.12), vec3(0.9, 0.95, 1.0), stripes);
        rgb = mix(rgb, color.rgb, 0.18) + edge * 0.35;
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 10) // Dissolve
    {
        float d = ghost_fbm(vary_position.xy * 3.2 + vec2(0.0, ghostTime * 0.22));
        float threshold = 0.40 + 0.16 * sin(ghostTime * 0.55 + ghostAux.w);
        if (d < threshold) discard;
        float glow = 1.0 - smoothstep(threshold, threshold + 0.10, d);
        rgb = tex.rgb * color.rgb + glow * mix(vec3(1.0, 0.35, 0.02), color.rgb, 0.4) * 2.2;
        alpha = color.a * tex.a * smoothstep(threshold, threshold + 0.025, d);
    }
    else if (ghostLook == 11) // Negative
    {
        rgb = (vec3(1.0) - tex.rgb) * mix(vec3(1.0), color.rgb, 0.35);
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 12) // Gold statue
    {
        rgb = mix(vec3(0.16, 0.055, 0.008), vec3(1.0, 0.72, 0.16),
                  smoothstep(0.05, 0.9, lum)) + edge * vec3(0.5, 0.3, 0.05);
        rgb *= mix(vec3(1.0), color.rgb, 0.12);
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 13) // Night vision
    {
        float grain = ghost_hash(gl_FragCoord.xy + floor(ghostTime * 18.0)) - 0.5;
        float nv = clamp(lum * 1.35 + grain * 0.14, 0.0, 1.0);
        rgb = vec3(0.03, nv, 0.08) * (0.72 + 0.28 * band);
        rgb *= mix(vec3(1.0), color.rgb, 0.12);
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 14) // Blueprint
    {
        vec2 grid_uv = abs(fract(gl_FragCoord.xy / 18.0) - 0.5);
        float grid = 1.0 - smoothstep(0.43, 0.49, max(grid_uv.x, grid_uv.y));
        rgb = vec3(0.005, 0.035, 0.09) + color.rgb * (edge * 1.8 + grid * 0.11);
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 15) // Ectoplasm
    {
        float flow = ghost_fbm(vary_position.xy * 2.8
                               + vec2(sin(ghostTime * 0.45), -ghostTime * 0.38));
        float wispy = smoothstep(0.22, 0.82, flow + edge * 0.45);
        rgb = mix(vec3(0.015, 0.12, 0.04), color.rgb, 0.7) * (0.5 + flow * 1.4);
        alpha = color.a * tex.a * wispy;
    }
    else if (ghostLook == 16) // Frost / ice
    {
        float sparkle = pow(ghost_hash(floor(gl_FragCoord.xy / 3.0)
                                          + floor(ghostTime * 3.0)), 18.0);
        rgb = mix(vec3(0.15, 0.42, 0.7), vec3(0.86, 0.97, 1.0), lum * 0.45 + edge)
              + sparkle * 1.6;
        rgb *= mix(vec3(1.0), color.rgb, 0.15);
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 17) // Prism
    {
        rgb = ghost_rainbow(edge * 0.82 + ghostTime * 0.035 + ghostAux.w * 0.05)
              * (0.25 + edge * 1.7);
        rgb = mix(rgb, color.rgb, 0.12);
        alpha = color.a * tex.a * (0.2 + edge * 0.8);
    }
    else if (ghostLook == 18) // Thermal scope
    {
        vec2 p = fract(vary_texcoord0);
        float vignette = smoothstep(0.72, 0.18, length(p - 0.5));
        float reticle = (1.0 - smoothstep(0.002, 0.012, abs(p.x - 0.5)))
                      + (1.0 - smoothstep(0.002, 0.012, abs(p.y - 0.5)));
        rgb = heat_lut(lum + edge * 0.32) * (0.35 + 0.65 * vignette)
              + color.rgb * reticle * 0.22;
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 19) // Wallhack / ESP
    {
        rgb = color.rgb * (0.16 + edge * 3.1) + vec3(lum) * color.rgb * 0.12;
        alpha = color.a * tex.a * (0.28 + edge * 0.72);
    }
    else if (ghostLook == 20) // Night-vision tube
    {
        vec2 p = fract(vary_texcoord0) - 0.5;
        float tube = 1.0 - smoothstep(0.36, 0.52, length(p));
        float grain = ghost_hash(gl_FragCoord.xy + floor(ghostTime * 20.0)) - 0.5;
        float ir = clamp(lum * 1.55 + edge * 0.55 + grain * 0.16, 0.0, 1.0);
        rgb = vec3(0.025, ir, 0.045) * tube;
        alpha = color.a * tex.a * tube;
    }
    else if (ghostLook == 21) // Damage overlay
    {
        vec2 p = fract(vary_texcoord0) - 0.5;
        float wound = smoothstep(0.18, 0.68, length(p))
                    * (0.55 + 0.45 * sin(ghostTime * 5.5 + ghostAux.w));
        rgb = mix(tex.rgb * color.rgb, vec3(0.72, 0.005, 0.01), wound * 0.82);
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 22) // Killcam
    {
        float grain = ghost_hash(gl_FragCoord.xy + floor(ghostTime * 24.0)) - 0.5;
        float bars = step(fract(vary_texcoord0.y), 0.12)
                   + step(0.88, fract(vary_texcoord0.y));
        rgb = mix(vec3(lum + grain * 0.10), color.rgb * vec3(lum), 0.18);
        rgb *= 1.0 - clamp(bars, 0.0, 1.0) * 0.78;
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 23) // Oil slick / iridescent
    {
        rgb = ghost_rainbow(facing * 1.3 + lum * 0.22 + ghostTime * 0.018)
              * (0.34 + edge * 1.25) + tex.rgb * 0.14;
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 24) // Vaporwave
    {
        float horizon = fract(vary_texcoord0.y * 12.0 + ghostTime * 0.25);
        float grid = 1.0 - smoothstep(0.04, 0.12, min(fract(vary_texcoord0.x * 10.0), horizon));
        vec3 candy = mix(vec3(1.0, 0.03, 0.55), vec3(0.0, 0.92, 1.0), lum);
        rgb = candy * (0.65 + grid * 0.55) + color.rgb * edge;
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 25) // Halftone / comic
    {
        vec2 cell = fract(gl_FragCoord.xy / 7.0) - 0.5;
        float dotMask = 1.0 - smoothstep(sqrt(max(lum, 0.02)) * 0.34,
                                         sqrt(max(lum, 0.02)) * 0.34 + 0.08,
                                         length(cell));
        rgb = mix(vec3(0.015), color.rgb * (0.35 + tex.rgb), dotMask);
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 26) // Sonar reveal
    {
        float sweep = fract(ghostTime * 0.35 + ghostAux.w * 0.1);
        float beam = 1.0 - smoothstep(0.0, 0.075,
                                     abs(fract(vary_texcoord0.y) - sweep));
        rgb = color.rgb * (0.08 + edge * 0.7 + beam * 2.5);
        alpha = color.a * tex.a * (0.22 + edge * 0.35 + beam * 0.55);
    }
    else if (ghostLook == 27) // Hologram interference / double image
    {
        vec2 echo = vec2(0.012 * sin(ghostTime * 3.0 + ghostAux.w), 0.0);
        vec3 a = texture(diffuseMap, uv + echo).rgb;
        vec3 b = texture(diffuseMap, uv - echo).rgb;
        rgb = vec3(a.r, tex.g, b.b) * color.rgb + color.rgb * edge * 1.2;
        alpha = color.a * tex.a * (0.55 + edge * 0.45);
    }
    if (ghostLook >= 5)
    {
        rgb *= flicker;
        alpha *= flicker;
    }
    rgb *= max(ghostFx.w, 0.0);
    // Flat-tint looks have no authored texture detail to reveal a UV warp.
    // Add a restrained signal cue so every distortion remains readable on
    // Ghost/X-ray/etc. while textured looks still show the actual sample warp.
    if (distort > 0.001)
    {
        if (ghostDistort == 1)
        {
            vec2 cell = fract(gl_FragCoord.xy / mix(2.0, 18.0, distort));
            float seam = smoothstep(0.0, 0.10, min(min(cell.x, cell.y),
                                                    min(1.0 - cell.x, 1.0 - cell.y)));
            rgb *= mix(0.82, 1.0, seam);
        }
        else if (ghostDistort == 2)
            rgb *= voxelShade;
        else if (ghostDistort == 3)
        {
            float lensRing = 1.0 - smoothstep(0.012, 0.035,
                abs(length(vary_texcoord0 - ghostDistortParams.yz) - 0.38));
            rgb += color.rgb * lensRing * 0.25 * distort;
        }
        else if (ghostDistort == 4)
            rgb *= 0.88 + 0.12 * sin(gl_FragCoord.y * 0.12 + ghostTime * 4.2) * distort;
        else if (ghostDistort == 5)
            rgb += vec3(edge, 0.0, -edge) * 0.35 * distort;
        else if (ghostDistort == 6)
            rgb *= 0.82 + 0.18 * ghost_hash(floor(vary_texcoord0 * vec2(12.0, 22.0))
                                             + floor(ghostTime * 7.0)) * distort;
        else if (ghostDistort == 7)
            rgb *= 0.86 + 0.14 * ghost_hash(vec2(floor(gl_FragCoord.x / 13.0),
                                                  floor(ghostTime * 8.0))) * distort;
    }
    if (ghostDistort == 8)
        rgb *= 1.0 - vhsBand * 0.22 * distort;
    frag_color = max(vec4(rgb, alpha), vec4(0));
}
