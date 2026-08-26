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

// Ghost Studio authors its palettes in display/sRGB space.  Shared PBR runs in
// scene-linear HDR, so fixed creative colours must be decoded before they are
// mixed with a lit material.  Keep these helpers private to Actor FX rather
// than relying on a pipeline module that is not present in every permutation.
float actor_fx_pbr_srgb_channel(float c)
{
    c = clamp(c, 0.0, 1.0);
    return c <= 0.04045 ? c / 12.92
                        : pow((c + 0.055) / 1.055, 2.4);
}

vec3 actor_fx_pbr_palette(vec3 c)
{
    return vec3(actor_fx_pbr_srgb_channel(c.r),
                actor_fx_pbr_srgb_channel(c.g),
                actor_fx_pbr_srgb_channel(c.b));
}

// Exact (extended above 1.0) linear-to-sRGB conversion for classifying an
// authored GLTF base colour.  Cover graphics must not let scene lighting,
// normal maps, or metallic response move a texel between creative palette
// bands.  Unlike actor_fx_pbr_perceptual(), this is not a display tonemap.
float actor_fx_pbr_linear_to_srgb_channel(float c)
{
    c = max(c, 0.0);
    return c <= 0.0031308 ? c * 12.92
                          : 1.055 * pow(c, 1.0 / 2.4) - 0.055;
}

vec3 actor_fx_pbr_linear_to_srgb(vec3 c)
{
    return vec3(actor_fx_pbr_linear_to_srgb_channel(c.r),
                actor_fx_pbr_linear_to_srgb_channel(c.g),
                actor_fx_pbr_linear_to_srgb_channel(c.b));
}

// Bounded perceptual working colour for post-light classifiers.  It preserves
// HDR ordering without attempting to duplicate the viewer's later tonemapper.
// The returned value is used only for luma/threshold decisions, never written
// directly to the HDR target.
vec3 actor_fx_pbr_perceptual(vec3 hdr)
{
    vec3 positive = max(hdr, vec3(0.0));
    vec3 mapped = positive / (vec3(1.0) + positive);
    return pow(mapped, vec3(1.0 / 2.2));
}

// Palette-driven looks need a stable floor in shadow without becoming a flat
// LDR decal in a bright scene.  This restrained scale follows scene luminance
// and leaves the actual exposure/tonemap operation to the normal pipeline.
float actor_fx_pbr_hdr_scale(vec3 hdr)
{
    float luma = dot(max(hdr, vec3(0.0)),
                     vec3(0.2126, 0.7152, 0.0722));
    return clamp(0.72 + 0.42 * sqrt(luma + 0.02), 0.72, 2.25);
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

// Live-actor-exclusive clocks share the deterministic Actor FX time and UUID
// phase. They therefore remain phase-locked across material families, tiled
// snapshots, shared beauty, and the optional synthetic-emission replay.
float actorFxStableUnitPhase()
{
    return fract(actorFxParams1.w * 0.15915494);
}

float actorFxBassSweepBand()
{
    float tempo = max(actorFxParams2.x, 0.20);
    float phase = fract(actorFxDeviceUv().y * 2.5
                      - actorFxTime * (0.22 + 0.16 * tempo)
                      + actorFxStableUnitPhase());
    return 1.0 - smoothstep(0.025, 0.10, abs(phase - 0.5));
}

float actorFxBassSweepBeat()
{
    float tempo = max(actorFxParams2.x, 0.20);
    float wave = 0.5 + 0.5 * sin(6.2831853
        * (actorFxTime * tempo + actorFxStableUnitPhase()));
    return pow(wave, 8.0);
}

float actorFxPossessedHeartbeat()
{
    float phase = fract(actorFxTime * max(actorFxParams2.x, 0.25)
                        + actorFxStableUnitPhase());
    float first = 1.0 - smoothstep(0.0, 0.055, abs(phase - 0.10));
    float second = 0.62
        * (1.0 - smoothstep(0.0, 0.045, abs(phase - 0.26)));
    return max(first, second);
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
           actorFxLook == 18 || actorFxLook == 19 ||
           actorFxLook == 20 || actorFxLook == 22 || actorFxLook == 24 ||
           actorFxLook == 25 || actorFxLook == 26 || actorFxLook == 27;
}

bool actorFxPbrPhysicalLook()
{
    return actorFxLook == 9 || actorFxLook == 12 || actorFxLook == 16 ||
           actorFxLook == 17 || actorFxLook == 23;
}

float actorFxPbrStrength()
{
    return clamp(actorFxParams0.x, 0.0, 1.0);
}

// The public, cheap gate used by material shaders.  Callers use this before
// any optional UV work or color-space conversion so disabled Actor FX is a
// bit-for-bit identity path.
bool actorFxActive()
{
    return actorFxEnabled != 0 && actorFxParams0.x > 0.001;
}

// Physical Layer is an input-space transition through the one normal PBR
// evaluation.  It therefore reaches the identical Cover material at strength
// one without evaluating a second BRDF.
float actorFxPbrPhysicalWeight()
{
    return actorFxActive() && actorFxPbrPhysicalLook()
        ? actorFxPbrStrength() : 0.0;
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
    float normal_len2 = dot(normal_eye, normal_eye);
    vec3 n = normal_len2 > 1e-12
        ? normal_eye * inversesqrt(normal_len2) : vec3(0.0, 0.0, 1.0);
    vec3 view_eye = -position_eye;
    float view_len2 = dot(view_eye, view_eye);
    vec3 v = view_len2 > 1e-12
        ? view_eye * inversesqrt(view_len2) : vec3(0.0, 0.0, 1.0);
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
    else if (actorFxLook == 28) // Live: Rim Noir
    {
        float rim_wide = smoothstep(0.05, 0.72, edge);
        float rim_core = smoothstep(0.48, 0.92, edge);
        vec3 noir = mix(vec3(lum), source, 0.28)
                  * (0.34 + 0.66 * smoothstep(0.035, 0.65, lum));
        fx = noir + actorFxTint * (0.28 * rim_wide + 1.10 * rim_core);
    }
    else if (actorFxLook == 29) // Live: Gel Split
    {
        float rim_wide = smoothstep(0.05, 0.72, edge);
        float rim_core = smoothstep(0.48, 0.92, edge);
        float side = smoothstep(-0.35, 0.35, n.x);
        vec3 cool = mix(vec3(0.02, 0.75, 1.00), actorFxTint, 0.22);
        vec3 warm = mix(vec3(1.00, 0.02, 0.38), actorFxTint.zyx, 0.18);
        vec3 gel = mix(cool, warm, side);
        fx = source * 0.78 + gel * (0.30 * rim_wide + 1.05 * rim_core);
    }
    else if (actorFxLook == 30) // Live: Bass Sweep
    {
        float rim_wide = smoothstep(0.05, 0.72, edge);
        float rim_core = smoothstep(0.48, 0.92, edge);
        float sweep_band = actorFxBassSweepBand();
        float beat = actorFxBassSweepBeat();
        vec3 club = mix(vec3(0.05, 0.85, 1.00), actorFxTint, 0.50);
        fx = source * (0.68 + 0.16 * beat)
           + club * (sweep_band * (0.35 + 1.05 * beat)
                     + 0.30 * rim_wide + 0.25 * rim_core);
    }
    else if (actorFxLook == 31) // Live: Moonlit
    {
        float rim_wide = smoothstep(0.05, 0.72, edge);
        float rim_core = smoothstep(0.48, 0.92, edge);
        vec3 moon = mix(vec3(0.16, 0.30, 0.72), actorFxTint, 0.18);
        vec3 night = mix(vec3(lum) * vec3(0.22, 0.30, 0.50),
                         source, 0.24) * 0.58;
        fx = night + moon * (0.20 * rim_wide + 0.72 * rim_core);
    }
    else if (actorFxLook == 32) // Live: Possessed
    {
        float rim_wide = smoothstep(0.05, 0.72, edge);
        float rim_core = smoothstep(0.48, 0.92, edge);
        float heartbeat = actorFxPossessedHeartbeat();
        float under = pow(max(dot(n, vec3(0.0, -0.816, 0.578)), 0.0), 2.0);
        float crawl = 0.5 + 0.5 * sin(position_eye.y * 7.5
                    + position_eye.x * 2.2 - actorFxTime * 0.9
                    + actorFxParams1.w);
        vec3 blood = mix(vec3(0.75, 0.003, 0.015), actorFxTint, 0.20);
        fx = source * (0.32 + 0.38 * (1.0 - under))
           + blood * (under * (0.26 + 1.25 * heartbeat)
                      + rim_core * (0.22 + 0.28 * heartbeat)
                      + 0.08 * crawl * rim_wide);
    }
    else if (actorFxLook == 33) // Live: Seraph
    {
        float rim_wide = smoothstep(0.05, 0.72, edge);
        float rim_core = smoothstep(0.40, 0.95, edge);
        float top = smoothstep(-0.10, 0.80, n.y);
        vec3 halo = mix(vec3(1.00, 0.86, 0.55), actorFxTint, 0.20);
        fx = source * (0.72 + 0.10 * top)
           + halo * (0.35 * rim_wide + 1.20 * rim_core * (0.5 + 0.5 * top));
    }
    else if (actorFxLook == 34) // Live: Interrogation
    {
        float rim_core = smoothstep(0.48, 0.92, edge);
        float key = smoothstep(-0.15, 0.60, n.x);
        float shadow = 1.0 - key;
        vec3 lit = mix(vec3(lum), source, 0.5) * (0.40 + 1.20 * key);
        fx = lit * (1.0 - 0.85 * shadow) + actorFxTint * (0.60 * rim_core);
    }
    else if (actorFxLook == 35) // Live: Wraith
    {
        float rim_wide = smoothstep(0.05, 0.72, edge);
        float rim_core = smoothstep(0.45, 0.95, edge);
        float flick = 0.70 + 0.30 * sin(actorFxTime * 6.0 + position_eye.y * 3.0);
        float crawl = 0.5 + 0.5 * sin(position_eye.y * 9.0 - actorFxTime * 1.6);
        vec3 spectral = mix(vec3(0.15, 1.00, 0.55), actorFxTint, 0.15);
        vec3 body = mix(vec3(lum) * vec3(0.20, 0.42, 0.28), source, 0.22) * 0.5;
        fx = body + spectral * flick
           * (0.22 * rim_wide + 0.95 * rim_core + 0.10 * crawl * rim_wide);
    }

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

// -------------------------------------------------------------------------
// Deferred-native PBR treatment
// -------------------------------------------------------------------------
// The legacy entry point above deliberately remains intact for classic/BOM and
// material-buffer programs.  Shared forward PBR needs a stricter split:
// physical replacement materials are authored before the one BRDF evaluation,
// while graphic/sensor looks operate on the resulting lit HDR colour.  This
// avoids lighting an already posterized signal and avoids evaluating PBR twice.

float actorFxPbrDissolveCoverage()
{
    if (!actorFxActive() || actorFxLook != 10)
    {
        return 1.0;
    }
    return actorFxBeautyDissolveCoverage();
}

float actorFxPbrDissolveAlpha(float coverage)
{
    if (!actorFxActive() || actorFxLook != 10)
    {
        return 1.0;
    }
    return smoothstep(0.0, 0.025, coverage);
}

// PBR separates geometric/material detail from authored emission.  A physical
// Cover must not erase normal/AO detail merely because it suppresses unrelated
// LEDs or baked emissive.  Flat sensor Covers intentionally use the geometric
// normal and neutral AO; Frost softens rather than removes authored relief.
float actorFxPbrNormalAoResponse()
{
    if (!actorFxActive())
    {
        return 1.0;
    }

    float strength = actorFxPbrStrength();
    // Frost's physical endpoint is identical in Layer and Cover.  Other
    // physical looks deliberately retain authored relief/AO.
    if (actorFxLook == 16)
    {
        return 1.0 - 0.55 * actorFxPbrPhysicalWeight();
    }
    // Flattening graphic/sensor material detail is replacement semantics only;
    // Layer continues to classify the normally lit HDR material underneath.
    if (actorFxCoverMode() && actorFxFlatSensorLook())
    {
        return 1.0 - strength;
    }
    return 1.0;
}

float actorFxPbrAuthoredEmissiveResponse()
{
    if (!actorFxActive() || actorFxLook == 1 || actorFxLook == 10 ||
        actorFxLook == 21 || (actorFxLook >= 28 && actorFxLook <= 32))
    {
        return 1.0;
    }

    // Every replacement treatment owns its emission signal.  Apply the same
    // gain in beauty and glow, and in both Layer and Cover, so LEDs and emissive
    // maps cannot be recoloured by the style or bloom with a different weight.
    return 1.0 - actorFxPbrStrength();
}

// VHS is an imaging transform, so every authored colour channel which can
// remain visible (base colour and material emission) must receive the same
// monochrome mix. Keeping this helper in scene-linear space makes deferred
// beauty and glow agree without re-encoding either signal.
vec3 actorFxPbrVhsColor(vec3 source)
{
    if (!actorFxActive() || int(actorFxParams1.x + 0.5) != 8)
    {
        return source;
    }

    float distort = clamp(actorFxParams1.y, 0.0, 1.0);
    float mono = dot(source, vec3(0.2126, 0.7152, 0.0722));
    return mix(source, vec3(mono), 0.25 * distort);
}

vec3 actorFxPbrPreLight(vec3 source)
{
    if (!actorFxActive())
    {
        return source;
    }

    // Clone and Dissolve retain the authored base exactly.  Chrome, Gold, and
    // Frost are the three physical replacement materials; their base colour is
    // established here so probes, direct lights, and point lights all see the
    // same material in the single normal PBR evaluation.
    float physical_weight = actorFxPbrPhysicalWeight();
    if (physical_weight <= 0.0)
    {
        return source;
    }

    vec3 styled = source;
    vec3 tint = actor_fx_pbr_palette(actorFxTint);
    if (actorFxLook == 9) // neutral chrome
    {
        styled = mix(vec3(0.91, 0.92, 0.94), tint, 0.08);
    }
    else if (actorFxLook == 12) // conductor gold
    {
        styled = vec3(1.000, 0.766, 0.336)
               * mix(vec3(1.0), tint, 0.10);
    }
    else if (actorFxLook == 16) // dielectric frost / ice
    {
        // The creative ice swatch is authored in sRGB; decode it before mixing
        // with the scene-linear GLTF base colour.
        vec3 ice = mix(actor_fx_pbr_palette(vec3(0.46, 0.73, 0.95)),
                       tint, 0.12);
        styled = mix(source, ice, 0.78);
    }

    return mix(source, styled, physical_weight);
}

// Lightweight, emission-only evaluator shared by beauty and both glow
// permutations. It intentionally contains only deliberate signature-bloom
// looks, but uses the exact beauty masks, clocks, distortion envelope, signal,
// brightness, and strength laws.
vec3 actorFxPbrSyntheticEmission(vec3 authored_source,
                                 vec3 geometry_normal_eye,
                                 vec3 position_eye, vec2 authored_uv,
                                 float dissolve_coverage)
{
    if (!actorFxActive() ||
        !(actorFxLook == 2 || actorFxLook == 6 || actorFxLook == 10 ||
          actorFxLook == 14 || actorFxLook == 15 || actorFxLook == 17 ||
          actorFxLook == 19 || actorFxLook == 26 || actorFxLook == 27 ||
          actorFxLook == 30))
    {
        return vec3(0.0);
    }

    vec3 n = geometry_normal_eye;
    float n_len2 = dot(n, n);
    n = n_len2 > 1e-12
        ? n * inversesqrt(n_len2) : vec3(0.0, 0.0, 1.0);
    vec3 view_eye = -position_eye;
    float view_len2 = dot(view_eye, view_eye);
    vec3 v = view_len2 > 1e-12
        ? view_eye * inversesqrt(view_len2) : vec3(0.0, 0.0, 1.0);
    float facing = clamp(abs(dot(n, v)), 0.0, 1.0);
    float edge = pow(1.0 - facing, 2.0);
    vec2 frag_coord = actorFxFragCoord();
    vec3 authored = actorFxPbrVhsColor(max(authored_source, vec3(0.0)));
    float emission_scale = actor_fx_pbr_hdr_scale(authored);
    vec3 tint = actor_fx_pbr_palette(actorFxTint);

    float band = 1.0;
    if (actorFxLook == 2 || actorFxLook == 27)
    {
        band = 0.5 + 0.5
             * sin((frag_coord.y / 6.0 + actorFxTime * 1.7) * 6.2831853);
    }

    vec3 emission = vec3(0.0);
    if (actorFxLook == 2)
    {
        float signal = 1.0 - 0.30
            * (0.5 + 0.5 * sin(actorFxTime * 13.0)
                           * sin(actorFxTime * 7.3));
        vec3 holo_tint = actor_fx_pbr_palette(
            mix(vec3(0.25, 0.85, 1.0), actorFxTint, 0.25));
        float scan_peak = smoothstep(0.72, 1.0, band);
        emission = holo_tint
            * (edge * 0.80 + scan_peak * 0.16) * signal
            * 0.55 * emission_scale;
    }
    else if (actorFxLook == 6)
    {
        emission = tint * edge * 1.44 * emission_scale;
    }
    else if (actorFxLook == 10)
    {
        float dissolve_edge = 1.0 - smoothstep(0.0, 0.10,
                                               dissolve_coverage);
        vec3 dissolve_tint = actor_fx_pbr_palette(
            mix(vec3(1.0, 0.35, 0.02), actorFxTint, 0.4));
        emission = dissolve_tint * dissolve_edge * 2.2;
    }
    else if (actorFxLook == 14)
    {
        vec2 guv = abs(fract(frag_coord / 18.0) - 0.5);
        float grid = 1.0 - smoothstep(0.43, 0.49, max(guv.x, guv.y));
        emission = tint * (edge * 1.8 + grid * 0.11)
                 * 0.22 * emission_scale;
    }
    else if (actorFxLook == 15)
    {
        float flow = actor_fx_fbm(position_eye.xy * 2.8
                                  + vec2(sin(actorFxTime * 0.45),
                                         -actorFxTime * 0.38));
        float wispy = smoothstep(0.22, 0.82, flow + edge * 0.45);
        vec3 ecto = mix(actor_fx_pbr_palette(vec3(0.015, 0.12, 0.04)),
                        tint, 0.7);
        emission = ecto * (edge * 0.55 + wispy * flow * 0.35)
                 * 0.22 * emission_scale;
    }
    else if (actorFxLook == 17)
    {
        vec3 spectrum = actor_fx_pbr_palette(actor_fx_rainbow(
            edge * 0.82 + actorFxTime * 0.035 + actorFxParams1.w * 0.05));
        emission = spectrum * edge * 0.374 * emission_scale;
    }
    else if (actorFxLook == 19)
    {
        emission = tint * edge * 0.682 * emission_scale;
    }
    else if (actorFxLook == 26)
    {
        float sweep = fract(actorFxTime * 0.35 + actorFxParams1.w * 0.1);
        float beam = 1.0 - smoothstep(0.0, 0.075,
                                     abs(actorFxDeviceUv().y - sweep));
        emission = tint * (edge * 0.7 + beam * 2.5)
                 * 0.22 * emission_scale;
    }
    else if (actorFxLook == 27) // Hologram interference
    {
        float scan_peak = smoothstep(0.72, 1.0, band);
        emission = tint * (edge * 1.2 + scan_peak * 0.14)
                 * 0.22 * emission_scale;
    }
    else if (actorFxLook == 33) // Live: Seraph
    {
        float top = smoothstep(-0.10, 0.80, n.y);
        vec3 halo = actor_fx_pbr_palette(mix(vec3(1.00, 0.86, 0.55), actorFxTint, 0.20));
        emission = halo * (edge * 0.90 + 0.35) * top * 0.22 * emission_scale;
    }
    else if (actorFxLook == 34) // Live: Interrogation (no synthetic emission)
    {
        emission = vec3(0.0);
    }
    else if (actorFxLook == 35) // Live: Wraith
    {
        float flick = 0.70 + 0.30 * sin(actorFxTime * 6.0 + position_eye.y * 3.0);
        vec3 spectral = actor_fx_pbr_palette(mix(vec3(0.15, 1.00, 0.55), actorFxTint, 0.15));
        emission = spectral * edge * 0.40 * flick * emission_scale;
    }
    else // 30: Live Bass Sweep
    {
        float rim_core = smoothstep(0.48, 0.92, edge);
        float sweep_band = actorFxBassSweepBand();
        float beat = actorFxBassSweepBeat();
        vec3 club = actor_fx_pbr_palette(
            mix(vec3(0.05, 0.85, 1.00), actorFxTint, 0.50));
        emission = club * (sweep_band * (0.20 + 0.12 * beat)
                         + 0.10 * rim_core) * emission_scale;
    }

    float distort = clamp(actorFxParams1.y, 0.0, 1.0);
    int distort_mode = int(actorFxParams1.x + 0.5);
    float emission_cue = 1.0;
    if (distort > 0.001)
    {
        if (distort_mode == 1)
        {
            vec2 cell = fract(frag_coord / mix(2.0, 18.0, distort));
            float seam = smoothstep(0.0, 0.10,
                min(min(cell.x, cell.y), min(1.0 - cell.x, 1.0 - cell.y)));
            emission_cue *= mix(0.82, 1.0, seam);
        }
        else if (distort_mode == 2)
        {
            float cells = mix(80.0, 10.0, distort);
            vec3 cell = floor(position_eye * cells) / cells;
            emission_cue *= 0.68 + 0.32
                * actor_fx_hash(cell.xy + cell.z);
        }
        else if (distort_mode == 4)
        {
            emission_cue *= 0.88 + 0.12
                * sin(frag_coord.y * 0.12 + actorFxTime * 4.2) * distort;
        }
        else if (distort_mode == 6)
        {
            emission_cue *= 0.82 + 0.18
                * actor_fx_hash(floor(authored_uv * vec2(12.0, 22.0))
                                + floor(actorFxTime * 7.0)) * distort;
        }
        else if (distort_mode == 7)
        {
            emission_cue *= 0.86 + 0.14
                * actor_fx_hash(vec2(floor(frag_coord.x / 13.0),
                                     floor(actorFxTime * 8.0))) * distort;
        }
        else if (distort_mode == 8)
        {
            float roll = fract(frag_coord.y / 180.0 - actorFxTime * 0.32);
            float vhs_band = 1.0 - smoothstep(0.02, 0.12,
                                              abs(roll - 0.5));
            emission_cue *= 1.0 - vhs_band * 0.22 * distort;
        }
    }

    return emission * actorFxSignalPulse()
           * max(actorFxParams1.z, 0.0) * emission_cue
           * actorFxPbrStrength();
}

// Apply the post-light creative treatment and return its deliberately
// synthetic emission in one evaluation.  authored_source is the distorted,
// scene-linear base-colour sample; it keeps bloom identical in beauty and glow
// programs even though only beauty has access to the fully lit HDR result.
vec3 actorFxPbrPostLight(vec3 lit_color, vec3 authored_source,
                         vec3 geometry_normal_eye, vec3 position_eye,
                         vec2 authored_uv, float dissolve_coverage,
                         out vec3 synthetic_emission)
{
    synthetic_emission = vec3(0.0);
    if (!actorFxActive())
    {
        return lit_color;
    }

    float strength = actorFxPbrStrength();
    vec3 layer_source = lit_color;
    vec3 source = max(lit_color, vec3(0.0));
    vec3 authored = max(authored_source, vec3(0.0));
    vec3 n = geometry_normal_eye;
    float n_len2 = dot(n, n);
    n = n_len2 > 1e-12
        ? n * inversesqrt(n_len2) : vec3(0.0, 0.0, 1.0);
    vec3 view_eye = -position_eye;
    float view_len2 = dot(view_eye, view_eye);
    vec3 v = view_len2 > 1e-12
        ? view_eye * inversesqrt(view_len2) : vec3(0.0, 0.0, 1.0);
    float facing = clamp(abs(dot(n, v)), 0.0, 1.0);
    float edge = pow(1.0 - facing, 2.0);
    float rim_wide = smoothstep(0.05, 0.72, edge);
    float rim_core = smoothstep(0.48, 0.92, edge);
    vec2 frag_coord = actorFxFragCoord();
    int distort_mode = int(actorFxParams1.x + 0.5);
    float distort = clamp(actorFxParams1.y, 0.0, 1.0);
    float voxel_shade = 1.0;
    float vhs_band = 0.0;

    if (distort_mode == 2 && distort > 0.001)
    {
        float cells = mix(80.0, 10.0, distort);
        vec3 cell = floor(position_eye * cells) / cells;
        voxel_shade = 0.68 + 0.32 * actor_fx_hash(cell.xy + cell.z);
    }
    else if (distort_mode == 8 && distort > 0.001)
    {
        float roll = fract(frag_coord.y / 180.0 - actorFxTime * 0.32);
        vhs_band = 1.0 - smoothstep(0.02, 0.12, abs(roll - 0.5));
        source = actorFxPbrVhsColor(source);
        authored = actorFxPbrVhsColor(authored);
    }

    // Graphic Cover is a stable authored-colour imaging treatment.  Layer is a
    // treatment of the normally lit HDR material.  Physical/hybrid looks never
    // enter this authored classifier and retain their lit source.
    bool graphic_cover = actorFxCoverMode()
        && (actorFxFlatSensorLook() || actorFxLook == 11);
    vec3 graphic_source = graphic_cover ? authored : source;
    // Clone, Chrome, Dissolve, and Gold are source-only post-light paths. Do
    // not pay their former per-fragment EOTF/OETF and HDR-scale cost unless a
    // selected distortion actually needs the tint palette.
    bool source_only = actorFxLook == 1 || actorFxLook == 9 ||
                       actorFxLook == 10 || actorFxLook == 12;
    vec3 perceptual = vec3(0.0);
    float lum = 0.0;
    float hdr_scale = 1.0;
    if (!source_only)
    {
        perceptual = graphic_cover
            ? actor_fx_pbr_linear_to_srgb(graphic_source)
            : actor_fx_pbr_perceptual(graphic_source);
        lum = dot(perceptual, vec3(0.299, 0.587, 0.114));
        hdr_scale = actor_fx_pbr_hdr_scale(graphic_source);
    }
    bool needs_tint = !source_only ||
        (distort > 0.001 && distort_mode == 3);
    vec3 tint = needs_tint
        ? actor_fx_pbr_palette(actorFxTint) : vec3(0.0);

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
    {
        fx = actor_fx_pbr_palette(mix(vec3(1.0), actorFxTint, 0.25))
           * (0.92 + rim_wide * 0.14 + rim_core * 0.06) * hdr_scale;
    }
    else if (actorFxLook == 1) // Clone: exact authored PBR
    {
        fx = source;
    }
    else if (actorFxLook == 2) // Hologram
    {
        float scan = 0.30 + 0.70 * band;
        float signal = 1.0 - 0.30
            * (0.5 + 0.5 * sin(actorFxTime * 13.0)
                           * sin(actorFxTime * 7.3));
        vec3 holo_tint = actor_fx_pbr_palette(
            mix(vec3(0.25, 0.85, 1.0), actorFxTint, 0.25));
        fx = holo_tint * (scan * signal + edge * 0.8) * hdr_scale;
    }
    else if (actorFxLook == 3) // Wireframe backing / native line draw
    {
        if (actorFxWireLinePass())
        {
            vec3 wire_tint = mix(tint, vec3(1.0), 0.55);
            fx = wire_tint * (0.75 + edge * 1.8) * hdr_scale;
        }
        else
        {
            fx = mix(actor_fx_pbr_palette(vec3(0.006)), tint * 0.035,
                     0.45) * hdr_scale;
        }
    }
    else if (actorFxLook == 4) // X-ray
    {
        vec3 xray_tint = actor_fx_pbr_palette(
            mix(vec3(0.55, 0.75, 1.0), actorFxTint, 0.35));
        fx = xray_tint * (0.16 + edge * 2.2) * hdr_scale
           + graphic_source * 0.04;
    }
    else if (actorFxLook == 5) // Thermal
    {
        fx = mix(actor_fx_pbr_palette(
                     actor_fx_heat(clamp(lum + edge * 0.28, 0.0, 1.0))),
                 tint, 0.12) * hdr_scale;
    }
    else if (actorFxLook == 6) // Neon outline
    {
        fx = graphic_source * 0.025 + tint * edge * 3.2 * hdr_scale;
    }
    else if (actorFxLook == 7) // Silhouette
    {
        fx = tint * (0.90 + rim_wide * 0.10 + rim_core * 0.04)
           * hdr_scale;
    }
    else if (actorFxLook == 8) // Toon / ink
    {
        vec3 poster_srgb = floor(perceptual * 4.0 + 0.5) / 4.0;
        vec3 poster = actor_fx_pbr_palette(poster_srgb);
        fx = mix(poster * tint, actor_fx_pbr_palette(vec3(0.015)),
                 smoothstep(0.18, 0.62, edge)) * hdr_scale;
    }
    else if (actorFxLook == 9) // Chrome: BRDF treatment is pre-light
    {
        fx = source;
    }
    else if (actorFxLook == 10) // Dissolve: coverage plus localized edge
    {
        fx = source;
    }
    else if (actorFxLook == 11) // Negative
    {
        vec3 negative = actor_fx_pbr_palette(vec3(1.0) - perceptual);
        fx = negative * mix(vec3(1.0), tint, 0.35) * hdr_scale;
    }
    else if (actorFxLook == 12) // Gold: BRDF treatment is pre-light
    {
        fx = source;
    }
    else if (actorFxLook == 13) // Night vision
    {
        float nv = clamp(lum * 1.35 + grain * 0.14, 0.0, 1.0);
        fx = actor_fx_pbr_palette(vec3(0.03, nv, 0.08))
           * (0.72 + 0.28 * band) * mix(vec3(1.0), tint, 0.12)
           * hdr_scale;
    }
    else if (actorFxLook == 14) // Blueprint
    {
        vec2 guv = abs(fract(frag_coord / 18.0) - 0.5);
        float grid = 1.0 - smoothstep(0.43, 0.49, max(guv.x, guv.y));
        vec3 blue = actor_fx_pbr_palette(vec3(0.005, 0.035, 0.09));
        fx = (blue + tint * (edge * 1.8 + grid * 0.11)) * hdr_scale;
    }
    else if (actorFxLook == 15) // Ectoplasm
    {
        float flow = actor_fx_fbm(position_eye.xy * 2.8
                                  + vec2(sin(actorFxTime * 0.45),
                                         -actorFxTime * 0.38));
        float wispy = smoothstep(0.22, 0.82, flow + edge * 0.45);
        vec3 ecto = mix(actor_fx_pbr_palette(vec3(0.015, 0.12, 0.04)),
                        tint, 0.7);
        fx = ecto * (0.5 + flow * 1.4) * mix(0.28, 1.0, wispy)
           * hdr_scale;
    }
    else if (actorFxLook == 16) // Frost: physical BRDF plus crystal sparkle
    {
        float sparkle = pow(actor_fx_hash(floor(frag_coord / 3.0)
                                           + floor(actorFxTime * 3.0)),
                            18.0);
        vec3 ice_glint = actor_fx_pbr_palette(vec3(0.86, 0.97, 1.0));
        fx = source + ice_glint * sparkle * 1.6 * hdr_scale;
    }
    else if (actorFxLook == 17) // Prism: retain dielectric reflections
    {
        vec3 spectrum = actor_fx_pbr_palette(actor_fx_rainbow(
            edge * 0.82 + actorFxTime * 0.035 + actorFxParams1.w * 0.05));
        fx = source * (vec3(0.42) + spectrum * 0.38)
           + spectrum * (0.10 + edge * 1.35) * hdr_scale;
    }
    else if (actorFxLook == 18) // Thermal scope
    {
        vec2 p = actorFxDeviceUv();
        float vignette = 1.0 - smoothstep(0.18, 0.72, length(p - 0.5));
        float reticle = (1.0 - smoothstep(0.002, 0.012, abs(p.x - 0.5)))
                      + (1.0 - smoothstep(0.002, 0.012, abs(p.y - 0.5)));
        fx = actor_fx_pbr_palette(actor_fx_heat(lum + edge * 0.32))
           * (0.35 + 0.65 * vignette) * hdr_scale
           + tint * reticle * 0.22 * hdr_scale;
    }
    else if (actorFxLook == 19) // Wallhack / ESP
    {
        fx = tint * (0.16 + edge * 3.1 + lum * 0.12) * hdr_scale;
    }
    else if (actorFxLook == 20) // Night-vision tube
    {
        vec2 p = actorFxDeviceUv();
        float tube = 1.0 - smoothstep(0.36, 0.52, length(p - 0.5));
        float ir = clamp(lum * 1.55 + edge * 0.55 + grain * 0.16,
                         0.0, 1.0);
        fx = actor_fx_pbr_palette(vec3(0.025, ir, 0.045))
           * tube * hdr_scale;
    }
    else if (actorFxLook == 21) // Damage overlay
    {
        // Rest-space placement is continuous across material/UV islands and
        // does not swim when the camera moves. Fold Z into the 2-D field so the
        // motif also remains coherent around the sides of the actor.
        vec2 damage_domain = vec2(
            vary_actor_fx_position.x + vary_actor_fx_position.z * 0.73,
            vary_actor_fx_position.y + vary_actor_fx_position.z * 1.17)
            * 1.35
            + vec2(cos(actorFxParams1.w), sin(actorFxParams1.w)) * 0.31;
        float wound = smoothstep(0.18, 0.68,
                                 length(fract(damage_domain) - 0.5))
                    * (0.55 + 0.45
                       * sin(actorFxTime * 5.5 + actorFxParams1.w));
        fx = mix(source, actor_fx_pbr_palette(vec3(0.72, 0.005, 0.01))
                         * hdr_scale,
                 wound * 0.82);
    }
    else if (actorFxLook == 22) // Killcam
    {
        float device_y = actorFxDeviceUv().y;
        float bars = step(device_y, 0.12) + step(0.88, device_y);
        vec3 monochrome = vec3(lum + grain * 0.10);
        vec3 kill_srgb = mix(monochrome, actorFxTint * vec3(lum), 0.18)
                       * (1.0 - clamp(bars, 0.0, 1.0) * 0.78);
        fx = actor_fx_pbr_palette(clamp(kill_srgb, 0.0, 1.0)) * hdr_scale;
    }
    else if (actorFxLook == 23) // Oil slick: retain dielectric reflections
    {
        vec3 spectrum = actor_fx_pbr_palette(actor_fx_rainbow(
            facing * 1.3 + lum * 0.22 + actorFxTime * 0.018));
        fx = source * (vec3(0.48) + spectrum * 0.52)
           + spectrum * edge * 0.55 * hdr_scale;
    }
    else if (actorFxLook == 24) // Vaporwave
    {
        // Full-frame device coordinates keep the animated grid continuous
        // across every PBR material and texture island, including tiled capture.
        vec2 vapor_uv = actorFxDeviceUv();
        float horizon = fract(vapor_uv.y * 12.0 + actorFxTime * 0.25);
        float grid = 1.0 - smoothstep(0.04, 0.12,
                                     min(fract(vapor_uv.x * 10.0),
                                         horizon));
        vec3 candy = actor_fx_pbr_palette(
            mix(vec3(1.0, 0.03, 0.55), vec3(0.0, 0.92, 1.0), lum));
        fx = candy * (0.65 + grid * 0.55) * hdr_scale
           + tint * edge * hdr_scale;
    }
    else if (actorFxLook == 25) // Halftone / comic
    {
        vec2 cell = fract(frag_coord / 7.0) - 0.5;
        float radius = sqrt(max(lum, 0.02)) * 0.34;
        float dots = 1.0 - smoothstep(radius, radius + 0.08,
                                     length(cell));
        vec3 paper = tint * (0.045 + 0.10 * lum)
                   + graphic_source * 0.06;
        vec3 ink = tint * (vec3(0.55) + actor_fx_pbr_palette(perceptual)
                           * 0.75);
        fx = mix(paper, ink, dots) * hdr_scale;
    }
    else if (actorFxLook == 26) // Sonar reveal
    {
        float sweep = fract(actorFxTime * 0.35 + actorFxParams1.w * 0.1);
        float beam = 1.0 - smoothstep(0.0, 0.075,
                                     abs(actorFxDeviceUv().y - sweep));
        fx = tint * (0.08 + edge * 0.7 + beam * 2.5) * hdr_scale;
    }
    else if (actorFxLook == 27) // Hologram interference
    {
        fx = graphic_source * tint + tint * edge * 1.2 * hdr_scale;
    }
    else if (actorFxLook == 28) // Live: Rim Noir
    {
        vec3 neutral = actor_fx_pbr_palette(vec3(lum)) * hdr_scale;
        vec3 noir = mix(neutral, source, 0.28)
                  * (0.34 + 0.66 * smoothstep(0.035, 0.65, lum));
        fx = noir + tint * hdr_scale
                  * (0.28 * rim_wide + 1.10 * rim_core);
    }
    else if (actorFxLook == 29) // Live: Gel Split
    {
        float side = smoothstep(-0.35, 0.35, n.x);
        vec3 cool = actor_fx_pbr_palette(
            mix(vec3(0.02, 0.75, 1.00), actorFxTint, 0.22));
        vec3 warm = actor_fx_pbr_palette(
            mix(vec3(1.00, 0.02, 0.38), actorFxTint.zyx, 0.18));
        vec3 gel = mix(cool, warm, side);
        fx = source * 0.78 + gel * hdr_scale
           * (0.30 * rim_wide + 1.05 * rim_core);
    }
    else if (actorFxLook == 30) // Live: Bass Sweep
    {
        float sweep_band = actorFxBassSweepBand();
        float beat = actorFxBassSweepBeat();
        vec3 club = actor_fx_pbr_palette(
            mix(vec3(0.05, 0.85, 1.00), actorFxTint, 0.50));
        fx = source * (0.68 + 0.16 * beat)
           + club * hdr_scale
             * (sweep_band * (0.35 + 1.05 * beat)
                + 0.30 * rim_wide + 0.25 * rim_core);
    }
    else if (actorFxLook == 31) // Live: Moonlit
    {
        vec3 moon = actor_fx_pbr_palette(
            mix(vec3(0.16, 0.30, 0.72), actorFxTint, 0.18));
        vec3 cool_luma = actor_fx_pbr_palette(
            clamp(vec3(lum) * vec3(0.22, 0.30, 0.50), 0.0, 1.0))
            * hdr_scale;
        vec3 night = mix(cool_luma, source, 0.24) * 0.58;
        fx = night + moon * hdr_scale
                   * (0.20 * rim_wide + 0.72 * rim_core);
    }
    else if (actorFxLook == 32) // Live: Possessed
    {
        float heartbeat = actorFxPossessedHeartbeat();
        float under = pow(max(dot(n, vec3(0.0, -0.816, 0.578)), 0.0), 2.0);
        float crawl = 0.5 + 0.5 * sin(position_eye.y * 7.5
                    + position_eye.x * 2.2 - actorFxTime * 0.9
                    + actorFxParams1.w);
        vec3 blood = actor_fx_pbr_palette(
            mix(vec3(0.75, 0.003, 0.015), actorFxTint, 0.20));
        fx = source * (0.32 + 0.38 * (1.0 - under))
           + blood * hdr_scale
             * (under * (0.26 + 1.25 * heartbeat)
                + rim_core * (0.22 + 0.28 * heartbeat)
                + 0.08 * crawl * rim_wide);
    }
    else if (actorFxLook == 33) // Live: Seraph
    {
        float top = smoothstep(-0.10, 0.80, n.y);
        vec3 halo = actor_fx_pbr_palette(mix(vec3(1.00, 0.86, 0.55), actorFxTint, 0.20));
        fx = source * (0.72 + 0.10 * top)
           + halo * hdr_scale
             * (0.35 * rim_wide + 1.20 * rim_core * (0.5 + 0.5 * top));
    }
    else if (actorFxLook == 34) // Live: Interrogation
    {
        float key = smoothstep(-0.15, 0.60, n.x);
        float shadow = 1.0 - key;
        vec3 lit = mix(actor_fx_pbr_palette(vec3(lum)) * hdr_scale, source, 0.5)
                 * (0.40 + 1.20 * key);
        fx = lit * (1.0 - 0.85 * shadow) + actorFxTint * hdr_scale * (0.60 * rim_core);
    }
    else if (actorFxLook == 35) // Live: Wraith
    {
        float flick = 0.70 + 0.30 * sin(actorFxTime * 6.0 + position_eye.y * 3.0);
        float crawl = 0.5 + 0.5 * sin(position_eye.y * 9.0 - actorFxTime * 1.6);
        vec3 spectral = actor_fx_pbr_palette(mix(vec3(0.15, 1.00, 0.55), actorFxTint, 0.15));
        vec3 body = mix(actor_fx_pbr_palette(
            clamp(vec3(lum) * vec3(0.20, 0.42, 0.28), 0.0, 1.0)) * hdr_scale,
            source, 0.22) * 0.5;
        fx = body + spectral * hdr_scale * flick
           * (0.22 * rim_wide + 0.95 * rim_core + 0.10 * crawl * rim_wide);
    }

    // UV displacement and RGB side taps were already applied to the authored
    // source samples. These restrained post cues keep flat graphic looks from
    // hiding the selected distortion. Synthetic emission applies the same
    // scalar cues in its dedicated lightweight evaluator.
    if (distort > 0.001)
    {
        if (distort_mode == 1)
        {
            vec2 cell = fract(frag_coord / mix(2.0, 18.0, distort));
            float seam = smoothstep(0.0, 0.10,
                min(min(cell.x, cell.y), min(1.0 - cell.x, 1.0 - cell.y)));
            float cue = mix(0.82, 1.0, seam);
            fx *= cue;
        }
        else if (distort_mode == 2)
        {
            fx *= voxel_shade;
        }
        else if (distort_mode == 3)
        {
            float lens_ring = 1.0 - smoothstep(0.012, 0.035,
                abs(length(authored_uv - vec2(0.5)) - 0.38));
            fx += tint * lens_ring * 0.25 * distort;
        }
        else if (distort_mode == 4)
        {
            float cue = 0.88 + 0.12
                * sin(frag_coord.y * 0.12 + actorFxTime * 4.2) * distort;
            fx *= cue;
        }
        else if (distort_mode == 5)
        {
            fx += vec3(edge, 0.0, -edge) * 0.35 * distort;
        }
        else if (distort_mode == 6)
        {
            float cue = 0.82 + 0.18
                * actor_fx_hash(floor(authored_uv * vec2(12.0, 22.0))
                                + floor(actorFxTime * 7.0)) * distort;
            fx *= cue;
        }
        else if (distort_mode == 7)
        {
            float cue = 0.86 + 0.14
                * actor_fx_hash(vec2(floor(frag_coord.x / 13.0),
                                     floor(actorFxTime * 8.0))) * distort;
            fx *= cue;
        }
        else if (distort_mode == 8)
        {
            float cue = 1.0 - vhs_band * 0.22 * distort;
            fx *= cue;
        }
    }

    // Brightness and the shared shimmer/glitch envelope are a single terminal
    // modulation of the styled branch.  Synthetic emission receives that same
    // envelope exactly once, and authored emission remains governed solely by
    // the material policy in the caller.
    float signal = actorFxSignalPulse();
    float brightness = max(actorFxParams1.z, 0.0);
    fx *= signal * brightness;
    synthetic_emission = actorFxPbrSyntheticEmission(
        authored_source, geometry_normal_eye, position_eye,
        authored_uv, dissolve_coverage);

    return mix(layer_source, fx, strength);
}

vec2 actorFxPbrMaterial(vec2 roughness_metallic)
{
    if (!actorFxActive())
    {
        return roughness_metallic;
    }

    vec2 styled = roughness_metallic;
    float weight = actorFxPbrPhysicalWeight();
    if (actorFxLook == 9)       styled = vec2(0.08, 1.0); // chrome
    else if (actorFxLook == 12) styled = vec2(0.18, 1.0); // gold
    else if (actorFxLook == 16) styled = vec2(0.18, 0.0); // dielectric ice
    else if (actorFxLook == 17) styled = vec2(0.10, 0.0); // dielectric prism
    else if (actorFxLook == 23) styled = vec2(0.22, 0.0); // dielectric oil film
    else if (actorFxCoverMode() && actorFxFlatSensorLook())
    {
        styled = vec2(0.92, 0.0);
        weight = actorFxPbrStrength();
    }
    return mix(roughness_metallic, styled, weight);
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
