/**
 * Native Actor FX material transform.
 *
 * This module is linked only into scene material shaders compiled with
 * HAS_ACTOR_FX.  It never changes authored alpha: opaque/mask/blend coverage,
 * depth writes, and pool sorting remain owned by the original material shader.
 * The one deliberate exception is look 10 (Dissolve), whose coverage change is
 * the point of the look.
 */

uniform int actorFxEnabled;
uniform int actorFxLook;
uniform float actorFxTime;
uniform vec3 actorFxTint;
// x=mix strength, y=pixel size, z=shimmer amount, w=glitch amount
uniform vec4 actorFxParams0;
// x=distortion id, y=distortion amount, z=brightness, w=stable actor phase
uniform vec4 actorFxParams1;
// x=shimmer speed in Hz; y/z=whole-image fragment offset; w reserved
uniform vec4 actorFxParams2;

// Shared with shadow and glow programs; implemented by actorFxDissolveF.glsl.
float actorFxDissolveCoverage(vec3 object_position);
in vec3 vary_actor_fx_position;

float actorFxBeautyDissolveCoverage()
{
    return actorFxDissolveCoverage(vary_actor_fx_position);
}

float actor_fx_hash(vec2 p)
{
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

float actor_fx_noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(actor_fx_hash(i), actor_fx_hash(i + vec2(1.0, 0.0)), f.x),
               mix(actor_fx_hash(i + vec2(0.0, 1.0)),
                   actor_fx_hash(i + vec2(1.0, 1.0)), f.x), f.y);
}

float actor_fx_fbm(vec2 p)
{
    float v = 0.0;
    v += 0.500 * actor_fx_noise(p); p = p * 2.03 + 17.1;
    v += 0.250 * actor_fx_noise(p); p = p * 2.01 + 11.7;
    v += 0.125 * actor_fx_noise(p);
    return v / 0.875;
}

vec3 actor_fx_heat(float x)
{
    x = clamp(x, 0.0, 1.0);
    vec3 c = mix(vec3(0.02, 0.05, 0.55), vec3(0.0, 0.85, 0.35),
                 smoothstep(0.0, 0.45, x));
    c = mix(c, vec3(1.0, 0.9, 0.05), smoothstep(0.42, 0.72, x));
    return mix(c, vec3(1.0, 0.03, 0.0), smoothstep(0.72, 1.0, x));
}

vec3 actor_fx_rainbow(float h)
{
    vec3 p = abs(fract(h + vec3(0.0, 0.6667, 0.3333)) * 6.0 - 3.0);
    return clamp(p - 1.0, 0.0, 1.0);
}

// gl_FragCoord is relative to the current render tile.  Adding the uploader's
// tile origin keeps pixel blocks, scan lines, and random bands continuous when
// the same frame is rendered as multiple tiles.
vec2 actorFxFragCoord()
{
    return gl_FragCoord.xy + actorFxParams2.yz;
}

// The public, cheap gate used by material shaders.  Callers use this before
// any optional UV work or color-space conversion so disabled Actor FX is a
// bit-for-bit identity path.
bool actorFxActive()
{
    return actorFxEnabled != 0 && actorFxParams0.x > 0.001;
}

vec2 actor_fx_screen_quantize(vec2 uv, float block_pixels)
{
    if (block_pixels <= 1.0)
    {
        return uv;
    }

    vec2 frag = actorFxFragCoord();
    vec2 block_center = (floor(frag / block_pixels) + 0.5) * block_pixels;
    vec2 delta_pixels = block_center - frag;
    return uv + dFdx(uv) * delta_pixels.x + dFdy(uv) * delta_pixels.y;
}

bool actorFxRgbSplitEnabled()
{
    int mode = int(actorFxParams1.x + 0.5);
    return actorFxActive() &&
           mode == 5 && actorFxParams1.y > 0.001;
}

bool actorFxUvTransformEnabled()
{
    if (!actorFxActive())
    {
        return false;
    }

    int mode = int(actorFxParams1.x + 0.5);
    bool distortion = actorFxParams1.y > 0.001 && mode >= 1 && mode <= 8 && mode != 5;
    return actorFxParams0.y > 1.0 || distortion;
}

// Transform a material's authored UV before RGB sampling.  Alpha and mask
// coverage deliberately continue sampling the original UV in the callers.
vec2 actorFxUv(vec2 uv, vec3 position_eye)
{
    float amount = clamp(actorFxParams1.y, 0.0, 1.0);
    int mode = int(actorFxParams1.x + 0.5);
    if (!actorFxActive())
    {
        return uv;
    }

    // Pixel Size is an independent, literal screen-pixel block size.  Mode 1
    // can request a larger block while retaining the same tile-safe grid.
    float block_pixels = max(actorFxParams0.y, 0.0);
    if (mode == 1) // pixelate
    {
        block_pixels = max(block_pixels, mix(2.0, 64.0, amount));
    }
    else if (mode == 2 && amount > 0.001) // voxel
    {
        block_pixels = max(block_pixels, mix(5.0, 28.0, amount));
    }
    uv = actor_fx_screen_quantize(uv, block_pixels);

    if (amount <= 0.001)
    {
        return uv;
    }

    if (mode == 2) // voxel: block the surface and stagger neighbouring cells
    {
        vec2 frag_cell = floor(actorFxFragCoord() / max(block_pixels, 1.0));
        vec3 volume_cell = floor(position_eye / mix(0.28, 0.06, amount));
        float cell_phase = actor_fx_hash(frag_cell + volume_cell.xy
                                         + vec2(volume_cell.z, actorFxParams1.w));
        vec2 texel = abs(dFdx(uv)) + abs(dFdy(uv));
        uv += (vec2(cell_phase,
                   actor_fx_hash(frag_cell.yx + volume_cell.yz + 19.7)) - 0.5)
              * texel * mix(1.0, 6.0, amount);
    }
    else if (mode == 3) // lens
    {
        vec2 d = uv - vec2(0.5);
        float q = clamp(length(d) / 0.48, 0.0, 1.0);
        return vec2(0.5) + d * mix(1.0 - 0.48 * amount, 1.0, q * q);
    }
    if (mode == 4) // ripple
    {
        return uv + vec2(sin(uv.y * 42.0 + actorFxTime * 4.2 + actorFxParams1.w),
                         cos(uv.x * 35.0 - actorFxTime * 3.4))
                    * (0.002 + 0.025 * amount);
    }
    if (mode == 6) // block glitch
    {
        float beat = floor(actorFxTime * 7.0);
        float r = actor_fx_hash(floor(uv * vec2(12.0, 22.0))
                                + vec2(beat, actorFxParams1.w));
        uv.x += step(1.0 - 0.38 * amount, r) * (r - 0.5) * 0.28 * amount;
    }
    else if (mode == 7) // vertical tear
    {
        float r = actor_fx_hash(vec2(floor(actorFxFragCoord().x / 13.0),
                                     floor(actorFxTime * 8.0) + actorFxParams1.w));
        uv.y += step(1.0 - 0.35 * amount, r) * (r - 0.5) * 0.24 * amount;
    }
    else if (mode == 8) // VHS
    {
        float line_noise = actor_fx_hash(vec2(floor(actorFxFragCoord().y / 3.0),
                                              floor(actorFxTime * 12.0)));
        uv.x += (line_noise - 0.5) * 0.035 * amount;
    }
    return uv;
}

// Return one RGB-split side tap.  Callers sample these only when
// actorFxRgbSplitEnabled(), so every other mode pays no extra texture reads.
vec2 actorFxRgbSplitUv(vec2 transformed_uv, float direction)
{
    float amount = clamp(actorFxParams1.y, 0.0, 1.0);
    float pixels = mix(1.0, 12.0, amount) * direction;
    float band = actor_fx_hash(vec2(floor(actorFxFragCoord().y / 18.0),
                                     floor(actorFxTime * 10.0) + actorFxParams1.w));
    pixels *= mix(0.65, 1.35, band);
    return transformed_uv + dFdx(transformed_uv) * pixels
                          + dFdy(transformed_uv) * pixels * 0.12;
}

vec3 actorFxApply(vec3 source, vec3 normal_eye, vec3 position_eye, vec2 authored_uv)
{
    // This must precede UV work and the Dissolve branch: a zero-strength style
    // is a strict no-op and may never discard authored coverage.
    if (!actorFxActive())
    {
        return source;
    }

    vec2 uv = authored_uv;
    if (actorFxUvTransformEnabled())
    {
        uv = actorFxUv(uv, position_eye);
    }
    vec3 n = normalize(normal_eye);
    vec3 v = normalize(-position_eye);
    float facing = clamp(abs(dot(n, v)), 0.0, 1.0);
    float edge = pow(1.0 - facing, 2.0);
    float lum = dot(max(source, vec3(0.0)), vec3(0.299, 0.587, 0.114));
    vec2 frag_coord = actorFxFragCoord();
    float band = 0.5 + 0.5 * sin((frag_coord.y / 6.0 + actorFxTime * 1.7) * 6.2831853);
    float grain = actor_fx_hash(frag_coord + floor(actorFxTime * 18.0)) - 0.5;
    vec3 fx = source;

    if (actorFxLook == 0) // Ghost
        fx = mix(source * actorFxTint, actorFxTint * (0.25 + lum * 0.75), 0.72) + edge * actorFxTint;
    else if (actorFxLook == 1) // Clone / authored material
        fx = source;
    else if (actorFxLook == 2) // Hologram
        fx = actorFxTint * (lum * (0.45 + 0.55 * band) + edge * 2.2);
    else if (actorFxLook == 3) // Wireframe
    {
        vec2 cell = abs(fract(uv * 28.0) - 0.5);
        float line = 1.0 - smoothstep(0.42, 0.49, max(cell.x, cell.y));
        fx = mix(source * 0.08, actorFxTint * (0.35 + edge * 2.2), line);
    }
    else if (actorFxLook == 4) // X-ray
        fx = actorFxTint * (0.12 + edge * 3.0) + source * 0.08;
    else if (actorFxLook == 5) // Thermal
        fx = actor_fx_heat(clamp(lum + edge * 0.28, 0.0, 1.0));
    else if (actorFxLook == 6) // Neon outline
        fx = source * 0.025 + actorFxTint * edge * 3.2;
    else if (actorFxLook == 7) // Silhouette
        fx = actorFxTint;
    else if (actorFxLook == 8) // Toon / ink
        fx = mix(floor(max(source, vec3(0.0)) * 4.0 + 0.5) / 4.0,
                 vec3(0.012), smoothstep(0.18, 0.62, edge));
    else if (actorFxLook == 9) // Chrome
    {
        vec3 r = reflect(-v, n);
        float stripes = 0.5 + 0.5 * sin((r.y * 2.4 + r.x) * 3.1415927);
        fx = mix(vec3(0.025, 0.045, 0.08), vec3(0.95, 0.98, 1.0), stripes)
             + edge * 0.35;
    }
    else if (actorFxLook == 10) // Dissolve (intentional coverage change)
    {
        float coverage = actorFxBeautyDissolveCoverage();
        if (coverage < 0.0) discard;
        float glow = 1.0 - smoothstep(0.0, 0.10, coverage);
        fx = source + glow * mix(vec3(1.0, 0.35, 0.02), actorFxTint, 0.4) * 2.2;
    }
    else if (actorFxLook == 11) // Negative
        fx = (vec3(1.0) - source) * mix(vec3(1.0), actorFxTint, 0.35);
    else if (actorFxLook == 12) // Gold statue
        fx = mix(vec3(0.16, 0.055, 0.008), vec3(1.0, 0.72, 0.16),
                 smoothstep(0.05, 0.9, lum)) + edge * vec3(0.5, 0.3, 0.05);
    else if (actorFxLook == 13) // Night vision
        fx = vec3(0.03, clamp(lum * 1.35 + grain * 0.14, 0.0, 1.0), 0.08)
             * (0.72 + 0.28 * band);
    else if (actorFxLook == 14) // Blueprint
    {
        vec2 guv = abs(fract(frag_coord / 18.0) - 0.5);
        float grid = 1.0 - smoothstep(0.43, 0.49, max(guv.x, guv.y));
        fx = vec3(0.005, 0.035, 0.09) + actorFxTint * (edge * 1.8 + grid * 0.11);
    }
    else if (actorFxLook == 15) // Ectoplasm
    {
        float flow = actor_fx_fbm(position_eye.xy * 2.8
                                  + vec2(sin(actorFxTime * 0.45), -actorFxTime * 0.38));
        fx = mix(vec3(0.015, 0.12, 0.04), actorFxTint, 0.7) * (0.5 + flow * 1.4);
    }
    else if (actorFxLook == 16) // Frost / ice
    {
        float sparkle = pow(actor_fx_hash(floor(frag_coord / 3.0)
                                           + floor(actorFxTime * 3.0)), 18.0);
        fx = mix(vec3(0.15, 0.42, 0.7), vec3(0.86, 0.97, 1.0),
                 lum * 0.45 + edge) + sparkle * 1.6;
    }
    else if (actorFxLook == 17) // Prism
        fx = actor_fx_rainbow(edge * 0.82 + actorFxTime * 0.035
                              + actorFxParams1.w * 0.05) * (0.25 + edge * 1.7);
    else if (actorFxLook == 18) // Thermal scope
    {
        vec2 p = fract(uv);
        float vignette = smoothstep(0.72, 0.18, length(p - 0.5));
        float reticle = 1.0 - smoothstep(0.002, 0.012, min(abs(p.x - 0.5), abs(p.y - 0.5)));
        fx = actor_fx_heat(lum + edge * 0.32) * (0.35 + 0.65 * vignette)
             + actorFxTint * reticle * 0.22;
    }
    else if (actorFxLook == 19) // Wallhack / ESP
        fx = actorFxTint * (0.16 + edge * 3.1) + vec3(lum) * actorFxTint * 0.12;
    else if (actorFxLook == 20) // Night-vision tube
    {
        float tube = 1.0 - smoothstep(0.36, 0.52, length(fract(uv) - 0.5));
        float ir = clamp(lum * 1.55 + edge * 0.55 + grain * 0.16, 0.0, 1.0);
        fx = vec3(0.025, ir, 0.045) * tube;
    }
    else if (actorFxLook == 21) // Damage overlay
    {
        float wound = smoothstep(0.18, 0.68, length(fract(uv) - 0.5))
                      * (0.55 + 0.45 * sin(actorFxTime * 5.5 + actorFxParams1.w));
        fx = mix(source, vec3(0.72, 0.005, 0.01), wound * 0.82);
    }
    else if (actorFxLook == 22) // Killcam
    {
        float bars = step(fract(uv.y), 0.12) + step(0.88, fract(uv.y));
        fx = vec3(lum + grain * 0.10) * (1.0 - clamp(bars, 0.0, 1.0) * 0.78);
    }
    else if (actorFxLook == 23) // Oil slick
        fx = actor_fx_rainbow(facing * 1.3 + lum * 0.22 + actorFxTime * 0.018)
             * (0.34 + edge * 1.25) + source * 0.14;
    else if (actorFxLook == 24) // Vaporwave
    {
        float horizon = fract(uv.y * 12.0 + actorFxTime * 0.25);
        float grid = 1.0 - smoothstep(0.04, 0.12, min(fract(uv.x * 10.0), horizon));
        fx = mix(vec3(1.0, 0.03, 0.55), vec3(0.0, 0.92, 1.0), lum)
             * (0.65 + grid * 0.55) + actorFxTint * edge;
    }
    else if (actorFxLook == 25) // Halftone / comic
    {
        vec2 cell = fract(frag_coord / 7.0) - 0.5;
        float radius = sqrt(max(lum, 0.02)) * 0.34;
        float dots = 1.0 - smoothstep(radius, radius + 0.08, length(cell));
        fx = mix(vec3(0.015), actorFxTint * (0.35 + source), dots);
    }
    else if (actorFxLook == 26) // Sonar reveal
    {
        float sweep = fract(actorFxTime * 0.35 + actorFxParams1.w * 0.1);
        float beam = 1.0 - smoothstep(0.0, 0.075, abs(fract(uv.y) - sweep));
        fx = actorFxTint * (0.08 + edge * 0.7 + beam * 2.5);
    }
    else if (actorFxLook == 27) // Hologram interference
        fx = source * vec3(1.08, 0.82 + 0.18 * band, 1.18)
             + actorFxTint * edge * 1.2;

    float shimmer = clamp(actorFxParams0.z, 0.0, 1.0);
    float glitch = clamp(actorFxParams0.w, 0.0, 1.0);
    float pulse = 1.0 - shimmer * 0.30
                  * (0.5 + 0.5 * sin(actorFxTime * max(actorFxParams2.x, 0.0)
                                      * 6.2831853 + actorFxParams1.w));
    float tear = step(1.0 - 0.35 * glitch,
                      actor_fx_hash(vec2(floor(frag_coord.y / 14.0),
                                         floor(actorFxTime * 9.0) + actorFxParams1.w)));
    fx *= pulse * (1.0 + tear * 0.25 * glitch);
    fx *= max(actorFxParams1.z, 0.0);

    return mix(source, fx, clamp(actorFxParams0.x, 0.0, 1.0));
}

vec2 actorFxPbrMaterial(vec2 roughness_metallic)
{
    if (!actorFxActive())
    {
        return roughness_metallic;
    }

    vec2 styled = roughness_metallic;
    if (actorFxLook == 9)       styled = vec2(0.08, 1.0); // chrome
    else if (actorFxLook == 12) styled = vec2(0.18, 1.0); // gold
    else if (actorFxLook == 16) styled = vec2(0.24, 0.15); // ice
    return mix(roughness_metallic, styled, clamp(actorFxParams0.x, 0.0, 1.0));
}

vec3 actorFxEmissive(vec3 authored_emissive, vec3 styled_color)
{
    if (!actorFxActive())
    {
        return authored_emissive;
    }
    float glow = 0.0;
    if (actorFxLook == 2 || actorFxLook == 6 || actorFxLook == 14 ||
        actorFxLook == 15 || actorFxLook == 17 || actorFxLook == 19 ||
        actorFxLook == 26 || actorFxLook == 27)
    {
        glow = 0.22;
    }
    return authored_emissive + styled_color * glow * clamp(actorFxParams0.x, 0.0, 1.0);
}
