/**
 * Native Actor FX material transform.
 *
 * This module is linked only into scene material shaders compiled with
 * HAS_ACTOR_FX.  It never changes authored alpha: opaque/mask/blend coverage,
 * depth writes, and pool sorting remain owned by the original material shader.
 * The one deliberate exception is look 10 (Dissolve), whose coverage change is
 * the point of the look.
 *
 * Inherent clocked looks match actorghostF: 2, 10, 13, 15, 16, 17, 20, 21,
 * 22, 23, 24, 26, and 27. The other looks are intentionally static in Ghost
 * Studio too; the shared shimmer, glitch, and distortion controls can animate
 * any look without changing that preset contract.
 */

uniform int actorFxEnabled;
uniform int actorFxLook;
uniform float actorFxTime;
uniform vec3 actorFxTint;
// Full untiled render-target size in pixels. The uploader keeps this coherent
// with actorFxParams2.yz, whose y/z components are the current tile origin.
uniform vec2 actorFxScreenSize;
// x=mix strength, y=pixel size, z=shimmer amount, w=glitch amount
uniform vec4 actorFxParams0;
// x=distortion id, y=distortion amount, z=brightness, w=stable actor phase
uniform vec4 actorFxParams1;
// x=shimmer speed in Hz; y/z=whole-image fragment offset;
// w=render semantics supplied by the caller
//   0=Layer, 1=Cover material/backing, 2=exact topology line pass
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

// A common full-frame device field for scope/tube/recorder motifs. It is
// intentionally derived from the tile-correct whole-image fragment coordinate,
// never a material UV, so the graphic does not restart at every face or GLTF
// texture island or snapshot tile.
vec2 actorFxDeviceUv()
{
    vec2 frame_size = max(actorFxScreenSize, vec2(1.0));
    return clamp(actorFxFragCoord() / frame_size, vec2(0.0), vec2(1.0));
}

bool actorFxCoverMode()
{
    return actorFxParams2.w > 0.5;
}

bool actorFxWireLinePass()
{
    return actorFxParams2.w > 1.5;
}

bool actorFxFlatSensorLook()
{
    return actorFxLook == 0 || actorFxLook == 2 || actorFxLook == 3 ||
           actorFxLook == 4 || actorFxLook == 5 || actorFxLook == 6 ||
           actorFxLook == 7 || actorFxLook == 8 || actorFxLook == 13 ||
           actorFxLook == 14 || actorFxLook == 15 ||
           actorFxLook == 17 || actorFxLook == 18 || actorFxLook == 19 ||
           actorFxLook == 20 || actorFxLook == 22 || actorFxLook == 24 ||
           actorFxLook == 25 || actorFxLook == 26 || actorFxLook == 27;
}

// The public, cheap gate used by material shaders.  Callers use this before
// any optional UV work or color-space conversion so disabled Actor FX is a
// bit-for-bit identity path.
bool actorFxActive()
{
    return actorFxEnabled != 0 && actorFxParams0.x > 0.001;
}

float actorFxBeautyDissolveAlpha()
{
    if (!actorFxActive() || actorFxLook != 10)
    {
        return 1.0;
    }
    return smoothstep(0.0, 0.025, actorFxBeautyDissolveCoverage());
}

// Scale for authored normal/AO/specular/gloss/environment/emissive response.
// Layer and Clone preserve the original material. A material-owning Cover
// supplies its own flat/sensor/metal/ice read, but the transition still follows
// the uploaded strength continuously so partial activation cannot pop normals,
// AO, shine, or authored bloom between the binary endpoints.
float actorFxAuthoredMaterialResponse()
{
    if (!actorFxActive() || !actorFxCoverMode())
    {
        return 1.0;
    }

    bool style_owns_material = actorFxFlatSensorLook() ||
                               actorFxLook == 9 || actorFxLook == 12 ||
                               actorFxLook == 16;
    return style_owns_material
        ? 1.0 - clamp(actorFxParams0.x, 0.0, 1.0)
        : 1.0;
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
    float amount = actorFxParams1.y;
    return actorFxActive() &&
           (((mode == 5 || mode == 8) && amount > 0.001) ||
            actorFxLook == 27 || actorFxParams0.w > 0.001);
}

bool actorFxUvTransformEnabled()
{
    if (!actorFxActive())
    {
        return false;
    }

    int mode = int(actorFxParams1.x + 0.5);
    bool distortion = actorFxParams1.y > 0.001 &&
                      mode >= 1 && mode <= 8 && mode != 5;
    return actorFxParams0.y > 1.0 || distortion || actorFxParams0.w > 0.001;
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
    if (mode == 1 && amount > 0.001) // pixelate
    {
        block_pixels = max(block_pixels, mix(2.0, 64.0, amount));
    }
    else if (mode == 2 && amount > 0.001) // voxel
    {
        // Voxel owns an independent, visibly stronger 3..38 pixel grid even
        // when the creative Pixel Size control is zero.
        block_pixels = max(block_pixels, mix(3.0, 38.0, amount));
    }
    uv = actor_fx_screen_quantize(uv, block_pixels);

    if (mode == 3 && amount > 0.001) // lens / magnify
    {
        const vec2 center = vec2(0.5);
        vec2 d = uv - center;
        float radius = mix(0.28, 0.48, amount);
        float q = length(d) / radius;
        if (q < 1.0)
        {
            uv = center + d * mix(0.48, 0.88, q * q) * amount
                        + d * (1.0 - amount);
        }
    }
    else if (mode == 4 && amount > 0.001) // ripple
    {
        uv += vec2(sin(uv.y * 42.0 + actorFxTime * 4.2 + actorFxParams1.w),
                   cos(uv.x * 35.0 - actorFxTime * 3.4))
              * (0.002 + 0.025 * amount);
    }
    else if (mode == 6 && amount > 0.001) // block glitch
    {
        float beat = floor(actorFxTime * 7.0);
        float r = actor_fx_hash(floor(uv * vec2(12.0, 22.0))
                                + vec2(beat, actorFxParams1.w));
        uv.x += step(1.0 - 0.38 * amount, r) * (r - 0.5) * 0.28 * amount;
    }
    else if (mode == 7 && amount > 0.001) // vertical tear
    {
        float r = actor_fx_hash(vec2(floor(actorFxFragCoord().x / 13.0),
                                     floor(actorFxTime * 8.0) + actorFxParams1.w));
        uv.y += step(1.0 - 0.35 * amount, r) * (r - 0.5) * 0.24 * amount;
    }
    else if (mode == 8 && amount > 0.001) // VHS
    {
        float roll = fract(actorFxFragCoord().y / 180.0 - actorFxTime * 0.32);
        float tracking = 1.0 - smoothstep(0.02, 0.12, abs(roll - 0.5));
        float line_noise = actor_fx_hash(vec2(floor(actorFxFragCoord().y / 3.0),
                                               floor(actorFxTime * 12.0)));
        uv.x += ((line_noise - 0.5) * 0.035 + tracking * 0.02) * amount;
    }

    // Creative Glitch is orthogonal to the distortion selector. Match Ghost
    // Studio's held horizontal slices in the authored RGB sample while alpha
    // and mask coverage remain at the original UV in each material caller.
    float glitch = clamp(actorFxParams0.w, 0.0, 1.0);
    if (glitch > 0.001)
    {
        float band_id = floor(actorFxFragCoord().y / 14.0);
        float beat = floor(actorFxTime * 9.0);
        float r = actor_fx_hash(vec2(band_id, beat + actorFxParams1.w));
        float torn = step(1.0 - 0.35 * glitch, r);
        uv.x += torn * (r - 0.5) * 0.22 * glitch;
    }
    return uv;
}

// Return one RGB-split side tap.  Callers sample these only when
// actorFxRgbSplitEnabled(), so every other mode pays no extra texture reads.
vec2 actorFxRgbSplitUv(vec2 transformed_uv, float direction)
{
    float amount = clamp(actorFxParams1.y, 0.0, 1.0);
    int mode = int(actorFxParams1.x + 0.5);
    float pixels = 0.0;
    if (mode == 5 && amount > 0.001)
    {
        // Clone RGB Split is a steady channel separation; time enters only
        // through VHS tracking, creative glitch, or look 27's echo.
        pixels += mix(1.0, 12.0, amount) * direction;
    }
    else if (mode == 8 && amount > 0.001)
    {
        float roll = fract(actorFxFragCoord().y / 180.0 - actorFxTime * 0.32);
        float tracking = 1.0 - smoothstep(0.02, 0.12, abs(roll - 0.5));
        pixels += (2.0 + tracking * 8.0) * amount * direction;
    }
    if (actorFxLook == 27)
    {
        pixels += sin(actorFxTime * 3.0 + actorFxParams1.w)
                  * 8.0 * direction;
    }
    float glitch = clamp(actorFxParams0.w, 0.0, 1.0);
    if (glitch > 0.001)
    {
        float band_id = floor(actorFxFragCoord().y / 14.0);
        float r = actor_fx_hash(vec2(band_id,
                                     floor(actorFxTime * 9.0) + actorFxParams1.w));
        float torn = step(1.0 - 0.35 * glitch, r);
        pixels += torn * 6.0 * glitch * direction;
    }
    return transformed_uv + dFdx(transformed_uv) * pixels
                           + dFdy(transformed_uv) * pixels * 0.12;
}

// One deterministic signal envelope for both styled beauty and synthetic
// emission.  Keeping it callable from actorFxEmissive() makes the Dissolve
// boundary pulse identically on legacy/system and PBR materials.
float actorFxSignalPulse()
{
    float shimmer = clamp(actorFxParams0.z, 0.0, 1.0);
    float glitch = clamp(actorFxParams0.w, 0.0, 1.0);
    float pulse = 1.0;
    if (shimmer > 0.001)
    {
        pulse -= shimmer * 0.60
            * (0.5 + 0.5 * sin(actorFxTime * max(actorFxParams2.x, 0.0)
                                * 6.2831853 + actorFxParams1.w));
    }
    float tear = 0.0;
    if (glitch > 0.001)
    {
        vec2 frag_coord = actorFxFragCoord();
        tear = step(1.0 - 0.35 * glitch,
                    actor_fx_hash(vec2(floor(frag_coord.y / 14.0),
                                       floor(actorFxTime * 9.0) + actorFxParams1.w)));
    }
    return pulse * (1.0 + tear * 0.35 * glitch);
}

vec3 actorFxApply(vec3 source, vec3 normal_eye, vec3 position_eye, vec2 authored_uv)
{
    // This must precede UV work and the Dissolve branch: a zero-strength style
    // is a strict no-op and may never discard authored coverage.
    if (!actorFxActive())
    {
        return source;
    }

    // Preserve the material sample that entered the style stage.  Some
    // distortion cues (notably VHS monochrome) deliberately alter the working
    // source below; Layer Alpha must blend those cues against this unchanged
    // input instead of allowing them to leak through the base side of mix().
    vec3 layer_source = source;
    vec3 n = normalize(normal_eye);
    vec3 v = normalize(-position_eye);
    float facing = clamp(abs(dot(n, v)), 0.0, 1.0);
    float edge = pow(1.0 - facing, 2.0);
    vec2 frag_coord = actorFxFragCoord();
    int distort_mode = int(actorFxParams1.x + 0.5);
    float distort = clamp(actorFxParams1.y, 0.0, 1.0);
    float voxel_shade = 1.0;
    float vhs_band = 0.0;
    if (distort_mode == 2 && distort > 0.001)
    {
        float cells = mix(80.0, 10.0, distort);
        vec3 cell = floor(position_eye * cells) / cells;
        float cell_shade = 0.68 + 0.32 * actor_fx_hash(cell.xy + cell.z);
        // actorghostF applies one cell-lighting read after the 3..38 px sample
        // quantization. Keep it a single cue here as well.
        voxel_shade = cell_shade;
    }
    else if (distort_mode == 8 && distort > 0.001)
    {
        float roll = fract(frag_coord.y / 180.0 - actorFxTime * 0.32);
        vhs_band = 1.0 - smoothstep(0.02, 0.12, abs(roll - 0.5));
        float mono = dot(source, vec3(0.299, 0.587, 0.114));
        source = mix(source, vec3(mono), 0.25 * distort);
    }
    float lum = dot(max(source, vec3(0.0)), vec3(0.299, 0.587, 0.114));
    // Uniform look branches let the driver skip procedural work that a style
    // cannot use.  In particular, a static Ghost/Clone/Silhouette should not
    // pay a sine plus hash for every fragment merely because Actor FX is on.
    float band = 1.0;
    if (actorFxLook == 2 || actorFxLook == 13 || actorFxLook == 27)
    {
        band = 0.5 + 0.5
            * sin((frag_coord.y / 6.0 + actorFxTime * 1.7) * 6.2831853);
    }
    float grain = 0.0;
    if (actorFxLook == 13)
    {
        grain = actor_fx_hash(frag_coord + floor(actorFxTime * 18.0)) - 0.5;
    }
    else if (actorFxLook == 20)
    {
        grain = actor_fx_hash(frag_coord + floor(actorFxTime * 20.0)) - 0.5;
    }
    else if (actorFxLook == 22)
    {
        grain = actor_fx_hash(frag_coord + floor(actorFxTime * 24.0)) - 0.5;
    }
    vec3 fx = source;

    if (actorFxLook == 0) // Ghost
        // Ghost Studio is a flat mostly-white tint, not a dark material wash.
        fx = mix(vec3(1.0), actorFxTint, 0.25);
    else if (actorFxLook == 1) // Clone / authored material
        fx = source;
    else if (actorFxLook == 2) // Hologram
    {
        // Match Ghost Studio's projector rather than modulating by the already
        // lit/material source.  The clone starts with a flat bright body, dims
        // it with moving scanlines, then adds a silhouette rim.  Using source
        // luminance here made dark skins and shadowed faces nearly disappear.
        float scan = 0.30 + 0.70 * band;
        float signal = 1.0 - 0.30
            * (0.5 + 0.5 * sin(actorFxTime * 13.0)
                           * sin(actorFxTime * 7.3));
        vec3 holo_tint = mix(vec3(0.25, 0.85, 1.0), actorFxTint, 0.25);
        fx = holo_tint * (scan * signal + edge * 0.8);
    }
    else if (actorFxLook == 3) // Wireframe backing / native line draw
    {
        if (actorFxWireLinePass())
        {
            // Exact system/BOM topology line draw. Harvested mesh and Animesh
            // use actorghostF's equivalent live-palette GL_LINE path.
            vec3 wire_tint = mix(actorFxTint, vec3(1.0), 0.55);
            fx = wire_tint * (0.75 + edge * 1.8);
        }
        else
        {
            // Cover hidden-line backing: suppress authored colour/PBR response
            // while keeping the actor's exact alpha coverage, shadows and
            // velocity. The final topology pass supplies the bright lines.
            fx = mix(vec3(0.006), actorFxTint * 0.035, 0.45);
        }
    }
    else if (actorFxLook == 4) // X-ray
    {
        vec3 xray_tint = mix(vec3(0.55, 0.75, 1.0), actorFxTint, 0.35);
        // Native coverage remains authored, so use a faint interior instead of
        // reproducing the clone's global 0.4 alpha and breaking mask/blend PBR.
        fx = xray_tint * (0.16 + edge * 2.2) + source * 0.04;
    }
    else if (actorFxLook == 5) // Thermal
        fx = mix(actor_fx_heat(clamp(lum + edge * 0.28, 0.0, 1.0)),
                 actorFxTint, 0.12);
    else if (actorFxLook == 6) // Neon outline
        fx = source * 0.025 + actorFxTint * edge * 3.2;
    else if (actorFxLook == 7) // Silhouette
        fx = actorFxTint;
    else if (actorFxLook == 8) // Toon / ink
    {
        vec3 poster = floor(max(source, vec3(0.0)) * 4.0 + 0.5) / 4.0;
        fx = mix(poster * actorFxTint, vec3(0.015),
                 smoothstep(0.18, 0.62, edge));
    }
    else if (actorFxLook == 9) // Chrome
    {
        vec3 r = reflect(-v, n);
        float stripes = 0.5 + 0.5 * sin((r.y * 2.4 + r.x) * 3.1415927);
        fx = mix(vec3(0.05, 0.08, 0.12), vec3(0.9, 0.95, 1.0), stripes);
        fx = mix(fx, actorFxTint, 0.18) + edge * 0.35;
    }
    else if (actorFxLook == 10) // Dissolve (intentional coverage change)
    {
        float coverage = actorFxBeautyDissolveCoverage();
        if (coverage < 0.0) discard;
        // The incandescent boundary is emitted once by actorFxEmissive().
        // Keeping it out of the lit base avoids doubling the edge energy while
        // still sharing this exact coverage/discard decision in every caller.
        fx = source;
    }
    else if (actorFxLook == 11) // Negative
        fx = (vec3(1.0) - source) * mix(vec3(1.0), actorFxTint, 0.35);
    else if (actorFxLook == 12) // Gold statue
    {
        fx = mix(vec3(0.16, 0.055, 0.008), vec3(1.0, 0.72, 0.16),
                 smoothstep(0.05, 0.9, lum)) + edge * vec3(0.5, 0.3, 0.05);
        fx *= mix(vec3(1.0), actorFxTint, 0.12);
    }
    else if (actorFxLook == 13) // Night vision
    {
        fx = vec3(0.03, clamp(lum * 1.35 + grain * 0.14, 0.0, 1.0), 0.08)
             * (0.72 + 0.28 * band);
        fx *= mix(vec3(1.0), actorFxTint, 0.12);
    }
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
        float wispy = smoothstep(0.22, 0.82, flow + edge * 0.45);
        // Native materials retain authored coverage; express Ghost Studio's
        // wispy alpha variation as luminance instead of punching new holes.
        fx = mix(vec3(0.015, 0.12, 0.04), actorFxTint, 0.7)
             * (0.5 + flow * 1.4) * mix(0.28, 1.0, wispy);
    }
    else if (actorFxLook == 16) // Frost / ice
    {
        float sparkle = pow(actor_fx_hash(floor(frag_coord / 3.0)
                                           + floor(actorFxTime * 3.0)), 18.0);
        fx = mix(vec3(0.15, 0.42, 0.7), vec3(0.86, 0.97, 1.0),
                  lum * 0.45 + edge) + sparkle * 1.6;
        fx *= mix(vec3(1.0), actorFxTint, 0.15);
    }
    else if (actorFxLook == 17) // Prism
        fx = mix(actor_fx_rainbow(edge * 0.82 + actorFxTime * 0.035
                                  + actorFxParams1.w * 0.05) * (0.25 + edge * 1.7),
                 actorFxTint, 0.12);
    else if (actorFxLook == 18) // Thermal scope
    {
        vec2 p = actorFxDeviceUv();
        float vignette = 1.0 - smoothstep(0.18, 0.72, length(p - 0.5));
        float reticle = (1.0 - smoothstep(0.002, 0.012, abs(p.x - 0.5)))
                       + (1.0 - smoothstep(0.002, 0.012, abs(p.y - 0.5)));
        fx = actor_fx_heat(lum + edge * 0.32) * (0.35 + 0.65 * vignette)
             + actorFxTint * reticle * 0.22;
    }
    else if (actorFxLook == 19) // Wallhack / ESP
        fx = actorFxTint * (0.16 + edge * 3.1) + vec3(lum) * actorFxTint * 0.12;
    else if (actorFxLook == 20) // Night-vision tube
    {
        vec2 p = actorFxDeviceUv();
        float tube = 1.0 - smoothstep(0.36, 0.52,
                                      length(p - 0.5));
        float ir = clamp(lum * 1.55 + edge * 0.55 + grain * 0.16, 0.0, 1.0);
        fx = vec3(0.025, ir, 0.045) * tube;
    }
    else if (actorFxLook == 21) // Damage overlay
    {
        float wound = smoothstep(0.18, 0.68,
                                 length(fract(authored_uv) - 0.5))
                      * (0.55 + 0.45 * sin(actorFxTime * 5.5 + actorFxParams1.w));
        fx = mix(source, vec3(0.72, 0.005, 0.01), wound * 0.82);
    }
    else if (actorFxLook == 22) // Killcam
    {
        float device_y = actorFxDeviceUv().y;
        float bars = step(device_y, 0.12) + step(0.88, device_y);
        vec3 monochrome = vec3(lum + grain * 0.10);
        fx = mix(monochrome, actorFxTint * vec3(lum), 0.18)
             * (1.0 - clamp(bars, 0.0, 1.0) * 0.78);
    }
    else if (actorFxLook == 23) // Oil slick
        fx = actor_fx_rainbow(facing * 1.3 + lum * 0.22 + actorFxTime * 0.018)
             * (0.34 + edge * 1.25) + source * 0.14;
    else if (actorFxLook == 24) // Vaporwave
    {
        float horizon = fract(authored_uv.y * 12.0 + actorFxTime * 0.25);
        float grid = 1.0 - smoothstep(0.04, 0.12,
                                     min(fract(authored_uv.x * 10.0), horizon));
        fx = mix(vec3(1.0, 0.03, 0.55), vec3(0.0, 0.92, 1.0), lum)
             * (0.65 + grid * 0.55) + actorFxTint * edge;
    }
    else if (actorFxLook == 25) // Halftone / comic
    {
        vec2 cell = fract(frag_coord / 7.0) - 0.5;
        float radius = sqrt(max(lum, 0.02)) * 0.34;
        float dots = 1.0 - smoothstep(radius, radius + 0.08, length(cell));
        // Ghost Studio draws after scene lighting, so its tinted dots retain a
        // readable floor even on a dark actor.  Native Actor FX participates in
        // lighting/PBR; lift both the paper and ink without touching coverage.
        vec3 paper = actorFxTint * (0.045 + 0.10 * lum) + source * 0.06;
        vec3 ink = actorFxTint * (vec3(0.55) + source * 0.75);
        fx = mix(paper, ink, dots);
    }
    else if (actorFxLook == 26) // Sonar reveal
    {
        float sweep = fract(actorFxTime * 0.35 + actorFxParams1.w * 0.1);
        float device_y = actorFxDeviceUv().y;
        float beam = 1.0 - smoothstep(0.0, 0.075,
                                     abs(device_y - sweep));
        fx = actorFxTint * (0.08 + edge * 0.7 + beam * 2.5);
    }
    else if (actorFxLook == 27) // Hologram interference
        // Material callers have already assembled source.r/source.b from the
        // animated side taps requested by actorFxRgbSplitEnabled().
        fx = source * actorFxTint + actorFxTint * edge * 1.2;

    // Ghost Studio adds restrained cues after the style so flat-tint looks
    // still reveal the selected distortion. Native textured looks keep their
    // real UV warp, with authored alpha/mask coverage untouched by callers.
    if (distort > 0.001)
    {
        if (distort_mode == 1)
        {
            vec2 cell = fract(frag_coord / mix(2.0, 18.0, distort));
            float seam = smoothstep(0.0, 0.10, min(min(cell.x, cell.y),
                                                    min(1.0 - cell.x, 1.0 - cell.y)));
            fx *= mix(0.82, 1.0, seam);
        }
        else if (distort_mode == 2)
            fx *= voxel_shade;
        else if (distort_mode == 3)
        {
            float lens_ring = 1.0 - smoothstep(0.012, 0.035,
                abs(length(authored_uv - vec2(0.5)) - 0.38));
            fx += actorFxTint * lens_ring * 0.25 * distort;
        }
        else if (distort_mode == 4)
            fx *= 0.88 + 0.12 * sin(frag_coord.y * 0.12 + actorFxTime * 4.2) * distort;
        else if (distort_mode == 5)
            fx += vec3(edge, 0.0, -edge) * 0.35 * distort;
        else if (distort_mode == 6)
            fx *= 0.82 + 0.18 * actor_fx_hash(floor(authored_uv * vec2(12.0, 22.0))
                                               + floor(actorFxTime * 7.0)) * distort;
        else if (distort_mode == 7)
            fx *= 0.86 + 0.14 * actor_fx_hash(vec2(floor(frag_coord.x / 13.0),
                                                    floor(actorFxTime * 8.0))) * distort;
        else if (distort_mode == 8)
            fx *= 1.0 - vhs_band * 0.22 * distort;
    }

    // Ghost Studio's adjustable shimmer can dim by up to 60%, and a torn band
    // gets a 35% signal pop.  Keep the native pass on the same time law so an
    // actor and an overlay clone move together at identical settings.
    fx *= actorFxSignalPulse();
    fx *= max(actorFxParams1.z, 0.0);

    return mix(layer_source, fx, clamp(actorFxParams0.x, 0.0, 1.0));
}

vec2 actorFxPbrMaterial(vec2 roughness_metallic)
{
    if (!actorFxActive() || !actorFxCoverMode())
    {
        // Layer is a colour/effect treatment over the authored material. It
        // must not silently turn a leather/skin/metal surface into a new BRDF.
        return roughness_metallic;
    }

    vec2 styled = roughness_metallic;
    if (actorFxLook == 9)       styled = vec2(0.08, 1.0); // chrome
    else if (actorFxLook == 12) styled = vec2(0.18, 1.0); // gold
    else if (actorFxLook == 16) styled = vec2(0.24, 0.15); // ice
    else if (actorFxFlatSensorLook()) styled = vec2(0.92, 0.0);
    return mix(roughness_metallic, styled, clamp(actorFxParams0.x, 0.0, 1.0));
}

vec3 actorFxEmissiveImpl(vec3 authored_emissive, vec3 styled_color,
                         float dissolve_edge_brightness)
{
    if (!actorFxActive())
    {
        return authored_emissive;
    }
    float strength = clamp(actorFxParams0.x, 0.0, 1.0);
    bool style_owns_material = actorFxFlatSensorLook() ||
                               actorFxLook == 9 || actorFxLook == 12 ||
                               actorFxLook == 16;
    vec3 base_emissive = authored_emissive;
    if (actorFxCoverMode() && style_owns_material)
    {
        // Cover supplies its own imaging/material read. Fade out authored
        // emissive with the same strength instead of letting unrelated signs,
        // LEDs, or baked glow burn through the replacement treatment.
        base_emissive = mix(authored_emissive, vec3(0.0), strength);
    }

    float glow = 0.0;
    if (actorFxLook == 0)
    {
        glow = 0.32;
    }
    else if (actorFxLook == 2)
    {
        // Clone holograms are an unlit post-scene overlay.  A stronger native
        // emissive term preserves that projector read through deferred/PBR
        // lighting while still retaining authored depth and alpha coverage.
        glow = 0.55;
    }
    else if (actorFxLook == 3)
    {
        glow = 0.36;
    }
    else if (actorFxLook == 4)
    {
        glow = 0.38;
    }
    else if (actorFxLook == 5)
    {
        glow = 0.20;
    }
    else if (actorFxLook == 6)
    {
        glow = 0.45;
    }
    else if (actorFxLook == 7)
    {
        glow = 0.32;
    }
    else if (actorFxLook == 8 || actorFxLook == 11)
    {
        glow = 0.12;
    }
    else if (actorFxLook == 13)
    {
        glow = 0.28;
    }
    else if (actorFxLook == 25)
    {
        // Keep comic dots legible in shadow with a restrained lift.  The
        // uploader intentionally avoids an extra synthetic alpha-glow pass.
        glow = 0.24;
    }
    else if (actorFxLook == 14 || actorFxLook == 15 ||
        actorFxLook == 17 || actorFxLook == 19 ||
        actorFxLook == 26 || actorFxLook == 27)
    {
        glow = 0.22;
    }
    else if (actorFxLook == 18 || actorFxLook == 20)
    {
        // Scope/tube looks are self-lit imaging devices in Ghost Studio.
        glow = 0.24;
    }
    else if (actorFxLook == 24)
    {
        glow = 0.18;
    }
    else if (actorFxLook == 16)
    {
        glow = 0.14;
    }
    else if (actorFxLook == 21 || actorFxLook == 23)
    {
        glow = 0.10;
    }
    else if (actorFxLook == 22)
    {
        glow = 0.08;
    }
    vec3 style_emissive = styled_color * glow;
    if (actorFxLook == 10)
    {
        // Use the exact beauty/shadow coverage field for the incandescent
        // dissolve boundary. This makes the orange/tinted edge travel with the
        // disappearing silhouette instead of merely brightening the remaining
        // diffuse material. Exact progress endpoints produce no surviving edge.
        float coverage = actorFxBeautyDissolveCoverage();
        float dissolve_edge = 1.0 - smoothstep(0.0, 0.10, coverage);
        vec3 dissolve_tint = mix(vec3(1.0, 0.35, 0.02), actorFxTint, 0.4);
        style_emissive += dissolve_tint * dissolve_edge * 2.2
                        * actorFxSignalPulse()
                        * max(dissolve_edge_brightness, 0.0);
    }
    return base_emissive + style_emissive * strength;
}

vec3 actorFxEmissive(vec3 authored_emissive, vec3 styled_color)
{
    // Bloom and the native material path intentionally retain the historical
    // edge energy. actorghost reconstructs Dissolve bloom without brightness.
    return actorFxEmissiveImpl(authored_emissive, styled_color, 1.0);
}

vec3 actorFxBeautyEmissive(vec3 authored_emissive, vec3 styled_color)
{
    // World actorghost beauty scales the incandescent Dissolve edge by the
    // user brightness control after applying the signal pulse. Keep that
    // treatment local to shared PBR beauty; authored emissive and every glow
    // replay continue through actorFxEmissive() above unchanged.
    return actorFxEmissiveImpl(authored_emissive, styled_color,
                               max(actorFxParams1.z, 0.0));
}
