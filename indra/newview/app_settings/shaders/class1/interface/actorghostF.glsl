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
 *                TEXTURE alpha always participates; per-VERTEX alpha joins it
 *                only when ghostUseVertexAlpha != 0 (alpha-pool / PBR draws)
 *                -- legacy non-alpha-pool faces bake SHININESS, not opacity,
 *                into vertex alpha (shiny "None" == 0), so honoring it there
 *                wiped every face whose material lacked a spec/normal map.
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

#ifdef GHOST_SYSTEM_AVATAR
// LLViewerJointMesh updates the stock `color` uniform while it binds each BOM
// texture.  Keep the cinematic style tint independent so those per-mesh setup
// writes cannot erase it during the replay sweep.
uniform vec4 ghostSystemColor;
#define color ghostSystemColor
#else
uniform vec4 color;
#endif
#ifdef GHOST_INDEXED_WORLD
// Authoritative shared-world indexed replay. The engine injects its native
// tex0..texN sampler family, vary_texture_index, and diffuseLookup() at the
// full legacy indexed width. Ghost Studio never compiles this permutation.

// Exact per-slot material contract. RGB factor is one for ordinary legacy/PBR
// beauty because the batcher bakes TE/base-colour factors into diffuse_color;
// it carries an auxiliary authored-map factor when a replay source requires
// one. A is the non-vertex authored alpha factor. Texture-alpha participation,
// cutoff law and vertex-alpha participation remain independent so OPAQUE,
// MASK and BLEND cannot leak semantics across merged material slots.
uniform vec4  ghostIndexedMaterialFactor[GHOST_INDEXED_CHANNELS];
uniform float ghostIndexedTextureAlpha[GHOST_INDEXED_CHANNELS];
uniform float ghostIndexedMinimumAlpha[GHOST_INDEXED_CHANNELS];
uniform int   ghostIndexedUseVertexAlpha[GHOST_INDEXED_CHANNELS];
uniform int   ghostIndexedAlphaCutoffMode[GHOST_INDEXED_CHANNELS];
#define GHOST_DIFFUSE_SAMPLE(uv_) diffuseLookup(uv_)
#else
flat in int vary_texture_index;
uniform sampler2D diffuseMap;
#define GHOST_DIFFUSE_SAMPLE(uv_) texture(diffuseMap, uv_)
#endif
uniform float ghostTime;
uniform vec4 ghostParams;
uniform vec4 ghostAux;
uniform vec4 ghostFx;
uniform int ghostLook;
// 1 = vertex-colour ALPHA is real opacity (alpha-pool / PBR draws) and
// multiplies the texture alpha; 0 = legacy non-alpha-pool face whose vertex
// alpha bakes SHININESS, not opacity (RGB tint still always applies).
uniform int ghostUseVertexAlpha;
// Shared world MASK replay follows the exact native base-pass discard law:
// 0 = no cutoff, 1 = texture alpha, 2 = texture * vertex alpha, 3 = texture
// alpha with the legacy-material half-UNORM8 (1/512) cutoff bias. Ghost Studio keeps its
// historical coverage branch and never compiles this world-only uniform.
#ifdef GHOST_WORLD_PASS
uniform int ghostAlphaCutoffMode;
#endif
#if defined(GHOST_SHARED_DISSOLVE) && !defined(GHOST_WORLD_PASS)
#error GHOST_SHARED_DISSOLVE requires GHOST_WORLD_PASS
#endif
// x > .5 means the material alpha mode actually uses sampled texture alpha;
// y is the PBR base-colour factor alpha. OPAQUE materials receive (0, 1), so
// arbitrary/packed data in their base-colour A channel cannot punch holes.
uniform vec2 ghostAlpha;
// Independent, orthogonal distortion layer. x=strength, yz=lens center.
uniform int ghostDistort;
uniform vec4 ghostDistortParams;
// Native live Actor wire is submitted to the linear HDR world buffer. Ghost
// Studio overlays remain post-tonemap/display-space and leave this disabled.
uniform int ghostWorldLinear;
#ifdef GHOST_WORLD_PASS
// Shared live Actor FX reuses the exact animated beauty program for its bloom
// replay.  This mode preserves every alpha/mask/dissolve decision above, emits
// no RGB, and publishes only bloom energy in HDR alpha.  Ghost Studio's late
// interface permutation never compiles or observes this uniform.
uniform int ghostGlowOnly;
#endif
// Whole-image pixel origin of the current tiled snapshot subregion.
uniform vec2 ghostFragOffset;
#ifdef GHOST_WORLD_PASS
// Full untiled render-target size. Shared Actor FX device motifs use this with
// ghostFragOffset so their scope/tube/recorder field spans the whole frame
// instead of restarting at every face, material UV island, or snapshot tile.
uniform vec2 ghostScreenSize;
// Layer+Dissolve owns authored coverage while its UI alpha remains treatment
// strength. Negative means Cover/full treatment. This is deliberately separate
// from ghostParams.x, whose historical meaning is scanline amount.
uniform float ghostCoverageLayerStrength;
#endif
#ifdef GHOST_SHARED_DISSOLVE
// Classic Actor FX uses the same authored progress and the same raw/rest-space
// field as native beauty + shadow. Ghost Studio keeps its historical automatic
// dissolve when this world-only permutation is absent.
uniform float ghostDissolveProgress;
in vec3 vary_object_position;
#endif
// [R2-2] indexed-batch slot filter: >= 0 draws ONLY fragments whose
// per-vertex material slot matches (the clone re-draws a multi-material
// batch once per slot with that slot's texture bound); -1 = no filtering.
uniform int ghostSlot;

in vec2 vary_texcoord0;
in vec3 vary_position;
in vec3 vary_normal;

float ghostSrgbChannelToLinear(float channel)
{
    float c = max(channel, 0.0);
    return c <= 0.04045
        ? c / 12.92
        : pow((c + 0.055) / 1.055, 2.4);
}
#ifdef GHOST_WORLD_PASS
// Localized world radiance is authored as a display-space palette plus an HDR
// intensity. Decode the bounded palette first and multiply the intensity in
// linear space; decoding palette*intensity made values above one grow
// superlinearly. The peak extraction is defensive for composite colours such
// as Halftone ink that can already exceed display white before this helper.
vec3 ghostWorldRadiance(vec3 display_color, float intensity)
{
    vec3 positive = max(display_color, vec3(0.0));
    float palette_scale = max(max(max(positive.r, positive.g), positive.b), 1.0);
    vec3 palette = clamp(positive / palette_scale, vec3(0.0), vec3(1.0));
    vec3 decoded = vec3(ghostSrgbChannelToLinear(palette.r),
                        ghostSrgbChannelToLinear(palette.g),
                        ghostSrgbChannelToLinear(palette.b));
    vec3 working = ghostWorldLinear != 0 ? decoded : palette;
    return working * palette_scale * max(intensity, 0.0);
}
#endif
in vec4 vary_vertex_color;

vec2 ghostDeviceUv()
{
#ifdef GHOST_WORLD_PASS
    return clamp((gl_FragCoord.xy + ghostFragOffset)
                 / max(ghostScreenSize, vec2(1.0)),
                 vec2(0.0), vec2(1.0));
#else
    // Ghost Studio is a historical late overlay and retains its per-material
    // motif coordinates. Only authoritative shared-world activation uses the
    // whole-frame device field above.
    return fract(vary_texcoord0);
#endif
}

#ifdef GHOST_WORLD_PASS
vec3 getAdditiveColor();
vec3 getAtmosAttenuation();
vec4 applySkyAndWaterFog(vec3 pos, vec3 additive, vec3 atten, vec4 color);
#endif

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
    vec2 fragCoord = gl_FragCoord.xy + ghostFragOffset;
    float ghostDissolveEdge = 0.0;
#ifdef GHOST_INDEXED_WORLD
    if (vary_texture_index < 0 || vary_texture_index >= GHOST_INDEXED_CHANNELS)
    {
        discard;
    }
    int activeAlphaCutoffMode =
        ghostIndexedAlphaCutoffMode[vary_texture_index];
    int activeUseVertexAlpha =
        ghostIndexedUseVertexAlpha[vary_texture_index];
    vec4 activeMaterialFactor =
        ghostIndexedMaterialFactor[vary_texture_index];
    vec2 activeGhostAlpha = vec2(
        ghostIndexedTextureAlpha[vary_texture_index],
        activeMaterialFactor.a);
    float activeMinimumAlpha =
        ghostIndexedMinimumAlpha[vary_texture_index];
#else
    int activeAlphaCutoffMode = 0;
#ifdef GHOST_WORLD_PASS
    activeAlphaCutoffMode = ghostAlphaCutoffMode;
#endif
    int activeUseVertexAlpha = ghostUseVertexAlpha;
    vec4 activeMaterialFactor = vec4(1.0);
    vec2 activeGhostAlpha = ghostAlpha;
    float activeMinimumAlpha = ghostAux.x;
#endif
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
        float slice = floor(fragCoord.x / 13.0);
        float beat = floor(ghostTime * 8.0);
        float r = ghost_hash(vec2(slice, beat + ghostAux.w));
        uv.y += step(1.0 - 0.35 * distort, r) * (r - 0.5) * 0.24 * distort;
    }
    else if (ghostDistort == 8 && distort > 0.001) // VHS / tracking
    {
        float roll = fract(fragCoord.y / 180.0 - ghostTime * 0.32);
        vhsBand = 1.0 - smoothstep(0.02, 0.12, abs(roll - 0.5));
        float lineNoise = ghost_hash(vec2(floor(fragCoord.y / 3.0),
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
        float band_id = floor(fragCoord.y / 14.0);
        float beat    = floor(ghostTime * 9.0);
        float r       = ghost_hash(vec2(band_id, beat + ghostAux.w));
        torn = step(1.0 - 0.35 * glitch, r);            // a minority of bands tear
        uv.x += torn * (r - 0.5) * 0.22 * glitch;       // sideways slice offset
    }

    vec4 tex = GHOST_DIFFUSE_SAMPLE(uv);
#ifdef GHOST_WORLD_PASS
    // Creative RGB distortions must never move authored coverage.  Keep a
    // separate sample at the original material UV for BLEND ramps and MASK
    // discard; Ghost Studio intentionally retains its historical distorted-A
    // behaviour in the non-world permutation.
    float worldAuthoredTextureAlpha = activeGhostAlpha.x > 0.5
        ? GHOST_DIFFUSE_SAMPLE(vary_texcoord0.xy).a : 1.0;
#endif
    if (ghostDistort == 5 && distort > 0.001) // RGB split
    {
        vec2 split = vec2(0.003 + 0.018 * distort, 0.0);
        tex.r = GHOST_DIFFUSE_SAMPLE(uv + split).r;
        tex.b = GHOST_DIFFUSE_SAMPLE(uv - split).b;
    }
    else if (ghostDistort == 8 && distort > 0.001)
    {
        vec2 split = vec2(0.002 + vhsBand * 0.008, 0.0) * distort;
        tex.r = GHOST_DIFFUSE_SAMPLE(uv + split).r;
        tex.b = GHOST_DIFFUSE_SAMPLE(uv - split).b;
        float mono = dot(tex.rgb, vec3(0.299, 0.587, 0.114));
        tex.rgb = mix(tex.rgb, vec3(mono), 0.25 * distort);
    }
#ifndef GHOST_WORLD_PASS
    // Shared-world voxel shading is applied once after EOTF decoding with the
    // other scalar distortion cues. Historical clone overlays retain their
    // display-space multiplication here.
    tex.rgb *= voxelShade;
#endif
    if (torn > 0.0)
    {
        // chroma split on the torn slice: R and B sampled a hair apart
        vec2 split = vec2(0.006 * glitch, 0.0);
        tex.r = GHOST_DIFFUSE_SAMPLE(uv + split).r;
        tex.b = GHOST_DIFFUSE_SAMPLE(uv - split).b;
    }

    // [R2-4] per-vertex colour, split by channel semantics:
    // RGB is always authored tint/base-colour and must always be honored.
    // Legacy non-alpha-pool vertex alpha holds shininess, not opacity.
    tex.rgb *= activeMaterialFactor.rgb * vary_vertex_color.rgb;
    float sampledAuthoredAlpha = tex.a;
#ifdef GHOST_WORLD_PASS
    sampledAuthoredAlpha = worldAuthoredTextureAlpha;
#endif
    float authoredAlpha = activeGhostAlpha.x > 0.5 ? sampledAuthoredAlpha : 1.0;
    if (activeUseVertexAlpha != 0)
    {
        authoredAlpha *= vary_vertex_color.a;
    }
    authoredAlpha *= clamp(activeGhostAlpha.y, 0.0, 1.0);

    // alpha-mask cutoff, exactly like the real render's masked passes. With
    // ghostAux.x == 0 no texel can be below the cutoff, so the branch is free
    // for opaque/blend batches.
#ifdef GHOST_WORLD_PASS
    float cutoffAlpha = activeGhostAlpha.x > 0.5 ? sampledAuthoredAlpha : 1.0;
    if (activeAlphaCutoffMode == 2)
    {
        cutoffAlpha *= vary_vertex_color.a;
    }
    cutoffAlpha *= clamp(activeGhostAlpha.y, 0.0, 1.0);
    float cutoffBias = activeAlphaCutoffMode == 3 ? (1.0 / 512.0) : 0.0;
    if (activeAlphaCutoffMode != 0 && cutoffAlpha < activeMinimumAlpha - cutoffBias)
#else
    if (authoredAlpha < activeMinimumAlpha)
#endif
    {
        discard;
    }
    // All look branches below consume tex.a as the authored coverage. Replace
    // it once here so OPAQUE, MASK, and BLEND semantics stay centralized.
    tex.a = authoredAlpha;
#ifdef GHOST_WORLD_PASS
    // Shared live activation changes surface colour, not material coverage.
    // Keep the exact centralized authored texture/vertex ramp for every look;
    // Dissolve is the sole look allowed to reshape/discard it below. This
    // branch is absent from Ghost Studio's historical interface permutation.
    // Native MASK/PBR MASK uses alpha only for discard; a surviving fragment
    // is material-opaque.  BLEND alone retains its continuous authored ramp.
    float worldAuthoredCoverage = activeAlphaCutoffMode != 0
        ? clamp(color.a, 0.0, 1.0)
        : clamp(color.a * tex.a, 0.0, 1.0);
    // Coverage-changing Layer+Dissolve cannot leave the native actor below its
    // holes. Preserve the authored replay as the zero-strength endpoint and
    // blend only the creative treatment; negative is Cover/full treatment.
    vec3 worldDissolveSource = tex.rgb;
    float worldDissolveTreatmentStrength = 1.0;
#endif

    // animated screen-space scanlines, scrolling slowly upward
    float scan_amt = clamp(ghostParams.x, 0.0, 1.0);
    float period   = max(ghostParams.w, 2.0);
    float band     = 0.5 + 0.5 * sin((fragCoord.y / period + ghostTime * 1.7) * 6.2831853);
    float scan     = mix(1.0, 0.30 + 0.70 * band, scan_amt);

    // Fresnel-ish rim from the eye-space normal / view direction. Both the
    // historical clone program and shared-world replay must survive degenerate
    // geometry and eye-plane vertices without publishing NaNs.
    float normal_len2 = dot(vary_normal, vary_normal);
    vec3 n = normal_len2 > 1e-12
        ? vary_normal * inversesqrt(normal_len2) : vec3(0.0, 0.0, 1.0);
    vec3 view_vector = -vary_position;
    float view_len2 = dot(view_vector, view_vector);
    vec3 v = view_len2 > 1e-12
        ? view_vector * inversesqrt(view_len2) : vec3(0.0, 0.0, 1.0);
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
    vec3 rgb;
#ifdef GHOST_WORLD_PASS
    // Shared live replay is converted to scene linear before its animated
    // signal envelope. Ghost Studio retains the historical display-space law.
    rgb = base * scan + color.rgb * rim;
#else
    rgb = base * scan * flicker + color.rgb * rim;
#endif
#ifdef GHOST_WORLD_PASS
    // Shared world replay gates the rim as well as the body.  Without tex.a on
    // the additive rim term, a fully transparent texel on a BLEND card still
    // produced an opaque Hologram/X-ray rectangle around hair, lace, and eyes.
    float syntheticCoverage = clamp(
        color.a * tex.a * (1.0 - scan_amt * (0.4 - 0.4 * band))
        + rim * 0.5 * tex.a, 0.0, 1.0);
    float alpha = clamp(color.a * tex.a * (1.0 - scan_amt * (0.4 - 0.4 * band)) * flicker
                        + rim * 0.5 * tex.a, 0.0, 1.0);
#else
    // Preserve Ghost Studio's historical post-tonemap overlay equation.
    float alpha = clamp(color.a * tex.a * (1.0 - scan_amt * (0.4 - 0.4 * band)) * flicker
                        + rim * 0.5, 0.0, 1.0);
#endif

    float lum = dot(tex.rgb, vec3(0.299, 0.587, 0.114));
    float facing = clamp(abs(dot(n, v)), 0.0, 1.0);
    float edge = pow(1.0 - facing, 2.0);
#ifdef GHOST_WORLD_PASS
    // Ghost Studio owns final display RGB, while this permutation is authored
    // into the pre-exposure HDR world.  Keep a separate, localized radiance
    // channel so graphic looks retain a readable geometry-normal silhouette
    // without turning their complete surface into fullbright bloom. Each
    // palette is decoded before its HDR intensity, then added after the common
    // display-colour conversion.
    vec3 worldRadiance = vec3(0.0);
    vec3 worldBloomRadiance = vec3(0.0);
    float worldRimWide = smoothstep(0.05, 0.72, edge);
    float worldRimCore = smoothstep(0.48, 0.92, edge);

    if (ghostLook == 0) // Ghost: pale body with a soft apparition rim.
    {
        worldRadiance = ghostWorldRadiance(
            color.rgb, 0.10 + worldRimWide * 0.18);
    }
    else if (ghostLook == 2) // Hologram: scan body plus projector rim.
    {
        worldRadiance = ghostWorldRadiance(
            color.rgb, scan * 0.24 + worldRimWide * 0.42
                       + worldRimCore * 0.30);
        // Bloom is the bright scan crest and rim subset, not the complete
        // projector body/readability floor.
        worldBloomRadiance = ghostWorldRadiance(
            color.rgb, pow(band, 4.0) * 0.12 + worldRimWide * 0.24
                       + worldRimCore * 0.24);
    }
    else if (ghostLook == 3) // Wire: exact GL_LINE topology, never a UV grid.
    {
        worldRadiance = ghostWorldRadiance(color.rgb, 0.42);
    }
    else if (ghostLook == 4) // X-ray: faint interior, strong continuous rim.
    {
        rgb = color.rgb * (0.16 + edge * 2.2) + tex.rgb * 0.04;
        worldRadiance = ghostWorldRadiance(
            color.rgb, 0.08 + worldRimWide * 0.42
                       + worldRimCore * 0.34);
    }
#endif
    if (ghostLook == 5) // Thermal
    {
        rgb = mix(heat_lut(clamp(lum + edge * 0.28, 0.0, 1.0)), color.rgb, 0.12);
#ifdef GHOST_WORLD_PASS
        // Preserve the heat palette through HDR exposure without making the
        // complete actor a bloom source.
        worldRadiance = ghostWorldRadiance(
            rgb, 0.08 + worldRimWide * 0.10);
#endif
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 6) // Neon outline
    {
        rgb = color.rgb * edge * 3.0;
#ifdef GHOST_WORLD_PASS
        float edgeMask = smoothstep(0.12, 0.75, edge);
        // A dark covered interior anchors the luminous silhouette on both
        // bright and dark backgrounds; radiance remains edge-localized.
        rgb += color.rgb * 0.035;
        worldRadiance = ghostWorldRadiance(
            color.rgb, worldRimWide * 0.38 + worldRimCore * 0.82);
        worldBloomRadiance = worldRadiance * 0.82;
        // Edge intensity must remain inside the material's authored coverage;
        // otherwise transparent portions of BLEND cards become neon quads.
        alpha = color.a * tex.a * edgeMask;
#else
        // Preserve Ghost Studio's historical post-tonemap overlay equation.
        alpha = color.a * smoothstep(0.12, 0.75, edge);
#endif
    }
    else if (ghostLook == 7) // Silhouette
    {
        rgb = color.rgb;
#ifdef GHOST_WORLD_PASS
        // A restrained body floor and paired rim keep the flat treatment
        // legible without classifying Silhouette as an explicit bloom look.
        rgb *= 0.82;
        worldRadiance = ghostWorldRadiance(
            color.rgb, 0.08 + worldRimWide * 0.14);
#endif
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 8) // Toon / ink
    {
        float bands = 4.0;
        vec3 poster = floor(tex.rgb * bands + 0.5) / bands;
        float ink = smoothstep(0.18, 0.62, edge);
        rgb = mix(poster * color.rgb, vec3(0.015), ink);
#ifdef GHOST_WORLD_PASS
        // Restore a small paper/readability floor after display-to-linear
        // conversion.  Ink itself remains dark and non-emissive.
        worldRadiance = ghostWorldRadiance(
            poster * color.rgb, 0.035 + (1.0 - ink) * 0.045);
#endif
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 9) // Chrome
    {
        vec3 r = reflect(-v, n);
        float stripes = 0.5 + 0.5 * sin((r.y * 2.4 + r.x) * 3.1415927);
#ifdef GHOST_WORLD_PASS
        // Legacy/BOM has no reflection-probe BRDF in this replay. Approximate
        // the PBR Chrome contract with a dark metal base, a broad environment
        // band, and a view-dependent white glint instead of a flat grey wash.
        float glint = smoothstep(0.72, 0.98, stripes);
        rgb = mix(vec3(0.025, 0.04, 0.07),
                  vec3(0.78, 0.88, 1.0), stripes);
        rgb = mix(rgb, color.rgb, 0.12)
            + vec3(0.35, 0.42, 0.50)
              * (glint * 0.32 + worldRimCore * 0.28);
        worldRadiance = ghostWorldRadiance(
            vec3(0.55, 0.68, 0.82),
            glint * 0.10 + worldRimCore * 0.08);
#else
        rgb = mix(vec3(0.05, 0.08, 0.12), vec3(0.9, 0.95, 1.0), stripes);
        rgb = mix(rgb, color.rgb, 0.18) + edge * 0.35;
#endif
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 10) // Dissolve
    {
#ifdef GHOST_SHARED_DISSOLVE
        worldDissolveTreatmentStrength = ghostCoverageLayerStrength < 0.0
            ? 1.0 : clamp(ghostCoverageLayerStrength, 0.0, 1.0);
        float progress = clamp(ghostDissolveProgress, 0.0, 1.0);
        if (progress >= 1.0)
        {
            discard;
        }
        vec2 actor_offset = vec2(cos(ghostAux.w), sin(ghostAux.w)) * 17.0;
        vec2 domain = vary_object_position.xy
                    + vec2(vary_object_position.z * 0.73,
                           vary_object_position.z * 1.17);
        // Exact actorFxDissolveCoverage() field. Shared legacy/PBR beauty,
        // synthetic bloom and native shadow must agree on the same rest-space
        // domain, advection and unpulsed manual threshold.
        float threshold = progress;
        float d = progress <= 0.0
            ? 1.0
            : ghost_fbm(domain * 3.2 + actor_offset
                        + vec2(0.0, ghostTime * 0.22)) - threshold;
        if (d < 0.0) discard;
        float glow = 1.0 - smoothstep(0.0, 0.10, d);
        ghostDissolveEdge = glow * worldDissolveTreatmentStrength;
        rgb = tex.rgb * color.rgb;
#ifdef GHOST_WORLD_PASS
        // Keep the incandescent edge in the same localized-radiance channel as
        // every other signature look. Beauty and bloom now share one colour,
        // one treatment strength, and the one common flicker/brightness law.
        worldRadiance = ghostWorldRadiance(
            mix(vec3(1.0, 0.35, 0.02), color.rgb, 0.4),
            ghostDissolveEdge * 2.2);
        worldBloomRadiance = worldRadiance;
#else
        rgb += glow * mix(vec3(1.0, 0.35, 0.02), color.rgb, 0.4) * 2.2;
#endif
        // Match shared PBR output coverage exactly: surviving MASK fragments
        // remain material-opaque, while BLEND retains its authored ramp.
        alpha = worldAuthoredCoverage * smoothstep(0.0, 0.025, d);
#else
        float d = ghost_fbm(vary_position.xy * 3.2 + vec2(0.0, ghostTime * 0.22));
        float threshold = 0.40 + 0.16 * sin(ghostTime * 0.55 + ghostAux.w);
        if (d < threshold) discard;
        float glow = 1.0 - smoothstep(threshold, threshold + 0.10, d);
        rgb = tex.rgb * color.rgb + glow * mix(vec3(1.0, 0.35, 0.02), color.rgb, 0.4) * 2.2;
        alpha = color.a * tex.a * smoothstep(threshold, threshold + 0.025, d);
#endif
    }
    else if (ghostLook == 11) // Negative
    {
        rgb = (vec3(1.0) - tex.rgb) * mix(vec3(1.0), color.rgb, 0.35);
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 12) // Gold statue
    {
#ifdef GHOST_WORLD_PASS
        // A broad view lobe stands in for the reflection response available on
        // PBR attachments. Keep the dark ochre body so the highlight reads as
        // metal rather than as blanket yellow emission.
        float gold_lobe = clamp(smoothstep(0.04, 0.92, lum)
                                + worldRimWide * 0.30, 0.0, 1.0);
        rgb = mix(vec3(0.105, 0.028, 0.003),
                  vec3(1.0, 0.72, 0.16), gold_lobe);
        rgb *= mix(vec3(1.0), color.rgb, 0.10);
        worldRadiance = ghostWorldRadiance(
            vec3(1.0, 0.58, 0.10), worldRimCore * 0.10);
#else
        rgb = mix(vec3(0.16, 0.055, 0.008), vec3(1.0, 0.72, 0.16),
                  smoothstep(0.05, 0.9, lum)) + edge * vec3(0.5, 0.3, 0.05);
        rgb *= mix(vec3(1.0), color.rgb, 0.12);
#endif
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 13) // Night vision
    {
        float grain = ghost_hash(fragCoord + floor(ghostTime * 18.0)) - 0.5;
        float nv = clamp(lum * 1.35 + grain * 0.14, 0.0, 1.0);
        rgb = vec3(0.03, nv, 0.08) * (0.72 + 0.28 * band);
        rgb *= mix(vec3(1.0), color.rgb, 0.12);
#ifdef GHOST_WORLD_PASS
        worldRadiance = ghostWorldRadiance(
            vec3(0.015, 0.10 + nv * 0.10, 0.025),
            0.72 + 0.28 * band);
#endif
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 14) // Blueprint
    {
        vec2 grid_uv = abs(fract(fragCoord / 18.0) - 0.5);
        float grid = 1.0 - smoothstep(0.43, 0.49, max(grid_uv.x, grid_uv.y));
        rgb = vec3(0.005, 0.035, 0.09) + color.rgb * (edge * 1.8 + grid * 0.11);
#ifdef GHOST_WORLD_PASS
        // Emit the drafted grid and silhouette, never the navy backing.
        worldRadiance = ghostWorldRadiance(
            color.rgb, grid * 0.22 + worldRimWide * 0.30
                       + worldRimCore * 0.38);
        worldBloomRadiance = worldRadiance * 0.84;
#endif
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 15) // Ectoplasm
    {
        float flow = ghost_fbm(vary_position.xy * 2.8
                               + vec2(sin(ghostTime * 0.45), -ghostTime * 0.38));
        float wispy = smoothstep(0.22, 0.82, flow + edge * 0.45);
        rgb = mix(vec3(0.015, 0.12, 0.04), color.rgb, 0.7) * (0.5 + flow * 1.4);
#ifdef GHOST_WORLD_PASS
        // Shared material replay preserves authored surface coverage. Match
        // actorFxF by expressing the ectoplasm wisps as luminance so legacy,
        // system/BOM, and PBR faces do not split at material boundaries.
        rgb *= mix(0.28, 1.0, wispy);
        worldRadiance = ghostWorldRadiance(
            mix(vec3(0.015, 0.12, 0.04), color.rgb, 0.7),
            wispy * (0.22 + flow * 0.34) + worldRimCore * 0.24);
        worldBloomRadiance = worldRadiance
            * clamp(wispy * 0.68 + worldRimCore * 0.32, 0.0, 1.0);
        alpha = color.a * tex.a;
#else
        // Ghost Studio keeps its historical wispy transparency.
        alpha = color.a * tex.a * wispy;
#endif
    }
    else if (ghostLook == 16) // Frost / ice
    {
        float sparkle = pow(ghost_hash(floor(fragCoord / 3.0)
                                          + floor(ghostTime * 3.0)), 18.0);
#ifdef GHOST_WORLD_PASS
        // Dielectric ice approximation: blue body, milky grazing response and
        // sparse crystalline glints. Avoid a broad metallic-looking glow.
        float ice_lobe = clamp(lum * 0.34 + worldRimWide * 0.76, 0.0, 1.0);
        rgb = mix(vec3(0.11, 0.34, 0.62), vec3(0.88, 0.98, 1.0), ice_lobe)
            + sparkle * 1.35;
        rgb *= mix(vec3(1.0), color.rgb, 0.12);
        worldRadiance = ghostWorldRadiance(
            vec3(0.62, 0.88, 1.0),
            sparkle * 0.85 + worldRimCore * 0.10);
#else
        rgb = mix(vec3(0.15, 0.42, 0.7), vec3(0.86, 0.97, 1.0), lum * 0.45 + edge)
              + sparkle * 1.6;
        rgb *= mix(vec3(1.0), color.rgb, 0.15);
#endif
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 17) // Prism
    {
#ifdef GHOST_WORLD_PASS
        vec3 spectrum = ghost_rainbow(edge * 0.82 + ghostTime * 0.035
                                      + ghostAux.w * 0.05);
        // Thin-film colour belongs mainly at grazing angles. A restrained
        // neutral/tinted body prevents the legacy portion from reading as an
        // unrelated unlit rainbow beside PBR surfaces.
        rgb = mix(vec3(0.025, 0.035, 0.055), color.rgb * 0.16, 0.35)
            + spectrum * (0.16 + edge * 1.55);
        worldRadiance = ghostWorldRadiance(
            spectrum, worldRimWide * 0.34 + worldRimCore * 0.46);
        worldBloomRadiance = worldRadiance * 0.80;
#else
        rgb = ghost_rainbow(edge * 0.82 + ghostTime * 0.035 + ghostAux.w * 0.05)
              * (0.25 + edge * 1.7);
        rgb = mix(rgb, color.rgb, 0.12);
#endif
        alpha = color.a * tex.a * (0.2 + edge * 0.8);
    }
    else if (ghostLook == 18) // Thermal scope
    {
        vec2 p = ghostDeviceUv();
        float vignette = 1.0 - smoothstep(0.18, 0.72, length(p - 0.5));
        float reticle = (1.0 - smoothstep(0.002, 0.012, abs(p.x - 0.5)))
                      + (1.0 - smoothstep(0.002, 0.012, abs(p.y - 0.5)));
        rgb = heat_lut(lum + edge * 0.32) * (0.35 + 0.65 * vignette)
              + color.rgb * reticle * 0.22;
#ifdef GHOST_WORLD_PASS
        worldRadiance = ghostWorldRadiance(
            heat_lut(lum + edge * 0.32),
            0.06 + worldRimWide * 0.08)
            + ghostWorldRadiance(color.rgb, reticle * 0.16);
#endif
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 19) // Wallhack / ESP
    {
        rgb = color.rgb * (0.16 + edge * 3.1) + vec3(lum) * color.rgb * 0.12;
#ifdef GHOST_WORLD_PASS
        worldRadiance = ghostWorldRadiance(
            color.rgb, worldRimWide * 0.42 + worldRimCore * 0.64);
        worldBloomRadiance = worldRadiance * 0.82;
#endif
        alpha = color.a * tex.a * (0.28 + edge * 0.72);
    }
    else if (ghostLook == 20) // Night-vision tube
    {
        vec2 p = ghostDeviceUv() - 0.5;
        float tube = 1.0 - smoothstep(0.36, 0.52, length(p));
        float grain = ghost_hash(fragCoord + floor(ghostTime * 20.0)) - 0.5;
        float ir = clamp(lum * 1.55 + edge * 0.55 + grain * 0.16, 0.0, 1.0);
        rgb = vec3(0.025, ir, 0.045) * tube;
#ifdef GHOST_WORLD_PASS
        worldRadiance = ghostWorldRadiance(
            vec3(0.01, 0.10 + ir * 0.10, 0.018), tube);
#endif
        alpha = color.a * tex.a * tube;
    }
    else if (ghostLook == 21) // Damage overlay
    {
#ifdef GHOST_WORLD_PASS
        // Shared scalar/indexed/system world permutations all export the same
        // raw/rest-space position used by Dissolve. Build the wound field there
        // so it cannot restart at every material UV island. The stable actor
        // phase offsets repeated meshes without introducing time swimming.
        vec2 damage_domain = vec2(
            vary_object_position.x + vary_object_position.z * 0.73,
            vary_object_position.y + vary_object_position.z * 1.17);
        damage_domain = damage_domain * 1.35
            + vec2(cos(ghostAux.w), sin(ghostAux.w)) * 0.31;
        vec2 p = fract(damage_domain) - 0.5;
#else
        vec2 p = fract(vary_texcoord0) - 0.5;
#endif
        float wound = smoothstep(0.18, 0.68, length(p))
                    * (0.55 + 0.45 * sin(ghostTime * 5.5 + ghostAux.w));
        rgb = mix(tex.rgb * color.rgb, vec3(0.72, 0.005, 0.01), wound * 0.82);
#ifdef GHOST_WORLD_PASS
        worldRadiance = ghostWorldRadiance(
            vec3(0.52, 0.003, 0.006), wound * 0.12);
#endif
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 22) // Killcam
    {
        float grain = ghost_hash(fragCoord + floor(ghostTime * 24.0)) - 0.5;
        float device_y = ghostDeviceUv().y;
        float bars = step(device_y, 0.12) + step(0.88, device_y);
        rgb = mix(vec3(lum + grain * 0.10), color.rgb * vec3(lum), 0.18);
        rgb *= 1.0 - clamp(bars, 0.0, 1.0) * 0.78;
#ifdef GHOST_WORLD_PASS
        worldRadiance = ghostWorldRadiance(
            color.rgb * vec3(lum),
            (1.0 - clamp(bars, 0.0, 1.0)) * 0.055);
#endif
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 23) // Oil slick / iridescent
    {
#ifdef GHOST_WORLD_PASS
        float film = facing * 1.3 + lum * 0.22 + ghostTime * 0.018;
        vec3 spectrum = ghost_rainbow(film);
        // Approximate a dielectric thin-film coat: authored colour remains in
        // the body while spectral response rises continuously toward grazing.
        rgb = tex.rgb * 0.22
            + spectrum * (0.20 + worldRimWide * 0.92
                          + worldRimCore * 0.28);
        worldRadiance = ghostWorldRadiance(
            spectrum, worldRimCore * 0.07);
#else
        rgb = ghost_rainbow(facing * 1.3 + lum * 0.22 + ghostTime * 0.018)
              * (0.34 + edge * 1.25) + tex.rgb * 0.14;
#endif
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 24) // Vaporwave
    {
#ifdef GHOST_WORLD_PASS
        // Match the native PBR treatment's tile-safe full-frame field. One
        // device-space grid now spans BOM, legacy, rigged, static and PBR
        // components without restarting at material or snapshot-tile edges.
        vec2 vapor_uv = ghostDeviceUv();
        float horizon = fract(vapor_uv.y * 12.0 + ghostTime * 0.25);
        float grid = 1.0 - smoothstep(
            0.04, 0.12,
            min(fract(vapor_uv.x * 10.0), horizon));
#else
        float horizon = fract(vary_texcoord0.y * 12.0 + ghostTime * 0.25);
        float grid = 1.0 - smoothstep(0.04, 0.12, min(fract(vary_texcoord0.x * 10.0), horizon));
#endif
        vec3 candy = mix(vec3(1.0, 0.03, 0.55), vec3(0.0, 0.92, 1.0), lum);
        rgb = candy * (0.65 + grid * 0.55) + color.rgb * edge;
#ifdef GHOST_WORLD_PASS
        worldRadiance = ghostWorldRadiance(candy, grid * 0.16)
            + ghostWorldRadiance(
                color.rgb, worldRimWide * 0.12 + worldRimCore * 0.12);
#endif
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 25) // Halftone / comic
    {
        vec2 cell = fract(fragCoord / 7.0) - 0.5;
        float dotMask = 1.0 - smoothstep(sqrt(max(lum, 0.02)) * 0.34,
                                         sqrt(max(lum, 0.02)) * 0.34 + 0.08,
                                         length(cell));
#ifdef GHOST_WORLD_PASS
        // Match the native comic treatment's readable paper/ink floor while
        // retaining the same seven-pixel screen-space dot law.
        vec3 paper = color.rgb * (0.045 + 0.10 * lum) + tex.rgb * 0.06;
        vec3 ink = color.rgb * (vec3(0.55) + tex.rgb * 0.75);
        rgb = mix(paper, ink, dotMask);
        worldRadiance = ghostWorldRadiance(
            mix(paper, ink, dotMask), 0.11);
#else
        rgb = mix(vec3(0.015), color.rgb * (0.35 + tex.rgb), dotMask);
#endif
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 26) // Sonar reveal
    {
        float sweep = fract(ghostTime * 0.35 + ghostAux.w * 0.1);
        float beam = 1.0 - smoothstep(0.0, 0.075,
                                     abs(ghostDeviceUv().y - sweep));
        rgb = color.rgb * (0.08 + edge * 0.7 + beam * 2.5);
#ifdef GHOST_WORLD_PASS
        worldRadiance = ghostWorldRadiance(
            color.rgb, beam * 0.78 + worldRimWide * 0.22
                       + worldRimCore * 0.30);
        worldBloomRadiance = ghostWorldRadiance(
            color.rgb, beam * 0.74 + worldRimWide * 0.16
                       + worldRimCore * 0.24);
#endif
        alpha = color.a * tex.a * (0.22 + edge * 0.35 + beam * 0.55);
    }
    else if (ghostLook == 27) // Hologram interference / double image
    {
        vec2 echo = vec2(0.012 * sin(ghostTime * 3.0 + ghostAux.w), 0.0);
        vec3 a = GHOST_DIFFUSE_SAMPLE(uv + echo).rgb;
        vec3 b = GHOST_DIFFUSE_SAMPLE(uv - echo).rgb;
        rgb = vec3(a.r, tex.g, b.b) * color.rgb + color.rgb * edge * 1.2;
#ifdef GHOST_WORLD_PASS
        worldRadiance = ghostWorldRadiance(
            color.rgb, 0.08 * (0.5 + 0.5 * band)
                       + worldRimWide * 0.26 + worldRimCore * 0.34);
        worldBloomRadiance = ghostWorldRadiance(
            color.rgb, pow(band, 4.0) * 0.045
                       + worldRimWide * 0.18 + worldRimCore * 0.28);
#endif
        alpha = color.a * tex.a * (0.55 + edge * 0.45);
    }
#ifdef GHOST_WORLD_PASS
    else if (ghostLook == 28) // Live Actor: Rim Noir
    {
        float tonal = 0.34 + 0.66 * smoothstep(0.035, 0.65, lum);
        rgb = mix(vec3(lum), tex.rgb, 0.28) * tonal;
        worldRadiance = ghostWorldRadiance(
            color.rgb, 0.28 * worldRimWide + 1.10 * worldRimCore);
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 29) // Live Actor: Gel Split
    {
        float side = smoothstep(-0.35, 0.35, n.x);
        vec3 cool = mix(vec3(0.02, 0.75, 1.00), color.rgb, 0.22);
        vec3 warm = mix(vec3(1.00, 0.02, 0.38), color.rgb.zyx, 0.18);
        vec3 gel = mix(cool, warm, side);
        rgb = tex.rgb * 0.78;
        worldRadiance = ghostWorldRadiance(
            gel, 0.30 * worldRimWide + 1.05 * worldRimCore);
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 30) // Live Actor: Bass Sweep
    {
        float tempo = max(ghostFx.x, 0.20);
        float unit_phase = fract(ghostAux.w * 0.15915494);
        float sweep_phase = fract(ghostDeviceUv().y * 2.5
                                  - ghostTime * (0.22 + 0.16 * tempo)
                                  + unit_phase);
        float sweep_band = 1.0 - smoothstep(
            0.025, 0.10, abs(sweep_phase - 0.5));
        float beat_wave = 0.5 + 0.5 * sin(
            6.2831853 * (ghostTime * tempo + unit_phase));
        float beat = pow(beat_wave, 8.0);
        vec3 club = mix(vec3(0.05, 0.85, 1.00), color.rgb, 0.50);
        rgb = tex.rgb * (0.68 + 0.16 * beat);
        worldRadiance = ghostWorldRadiance(
            club, sweep_band * (0.35 + 1.05 * beat)
                  + 0.30 * worldRimWide + 0.25 * worldRimCore);
        worldBloomRadiance = ghostWorldRadiance(
            club, sweep_band * (0.20 + 0.12 * beat)
                  + 0.10 * worldRimCore);
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 31) // Live Actor: Moonlit
    {
        vec3 moon = mix(vec3(0.16, 0.30, 0.72), color.rgb, 0.18);
        rgb = mix(vec3(lum) * vec3(0.22, 0.30, 0.50),
                  tex.rgb, 0.24) * 0.58;
        worldRadiance = ghostWorldRadiance(
            moon, 0.20 * worldRimWide + 0.72 * worldRimCore);
        alpha = color.a * tex.a;
    }
    else if (ghostLook == 32) // Live Actor: Possessed
    {
        float unit_phase = fract(ghostAux.w * 0.15915494);
        float heart_phase = fract(
            ghostTime * max(ghostFx.x, 0.25) + unit_phase);
        float first = 1.0 - smoothstep(
            0.0, 0.055, abs(heart_phase - 0.10));
        float second = 0.62 * (1.0 - smoothstep(
            0.0, 0.045, abs(heart_phase - 0.26)));
        float heartbeat = max(first, second);
        float under = pow(max(dot(n, vec3(0.0, -0.816, 0.578)), 0.0), 2.0);
        float crawl = 0.5 + 0.5 * sin(vary_position.y * 7.5
                    + vary_position.x * 2.2 - ghostTime * 0.9 + ghostAux.w);
        vec3 blood = mix(vec3(0.75, 0.003, 0.015), color.rgb, 0.20);
        rgb = tex.rgb * (0.32 + 0.38 * (1.0 - under));
        worldRadiance = ghostWorldRadiance(
            blood, under * (0.26 + 1.25 * heartbeat)
                 + worldRimCore * (0.22 + 0.28 * heartbeat)
                 + 0.08 * crawl * worldRimWide);
        alpha = color.a * tex.a;
    }
#endif
    if (ghostLook >= 5)
    {
#ifdef GHOST_WORLD_PASS
        // Capture Dissolve's edge-shaped coverage before the common flicker.
        // World beauty restores this value only for Dissolve; every other live
        // look restores authoritative authored material coverage below.
        syntheticCoverage = clamp(alpha, 0.0, 1.0);
#else
        rgb *= flicker;
        alpha *= flicker;
#endif
    }
#ifdef GHOST_WORLD_PASS
    // The common creative flicker is RGB animation, not surface coverage.
    // Dissolve restores its pre-flicker edge-shaped alpha; all other looks
    // restore the untouched material coverage captured above.
    alpha = ghostLook == 10 ? syntheticCoverage : worldAuthoredCoverage;
    // The localized world radiance is kept outside the historical display RGB
    // equation above. Apply the one common shimmer/glitch envelope and the one
    // user brightness multiplier here. The palette was already decoded before
    // these HDR intensity terms, so neither value receives an sRGB transfer a
    // second time. This keeps beauty and explicit bloom phase-locked without
    // changing Ghost Studio's non-world permutation.
    worldRadiance *= flicker * max(ghostFx.w, 0.0);
    worldBloomRadiance *= flicker * max(ghostFx.w, 0.0);
#endif
#ifndef GHOST_WORLD_PASS
    rgb *= max(ghostFx.w, 0.0);
#endif
    // Flat-tint looks have no authored texture detail to reveal a UV warp.
    // Add a restrained signal cue so every distortion remains readable on
    // Ghost/X-ray/etc. while textured looks still show the actual sample warp.
#ifdef GHOST_WORLD_PASS
    float worldDistortionCue = 1.0;
    vec3 worldDistortionAdditive = vec3(0.0);
#endif
    if (distort > 0.001)
    {
        if (ghostDistort == 1)
        {
            vec2 cell = fract(fragCoord / mix(2.0, 18.0, distort));
            float seam = smoothstep(0.0, 0.10, min(min(cell.x, cell.y),
                                                    min(1.0 - cell.x, 1.0 - cell.y)));
            float cue = mix(0.82, 1.0, seam);
#ifdef GHOST_WORLD_PASS
            worldDistortionCue *= cue;
#else
            rgb *= cue;
#endif
        }
        else if (ghostDistort == 2)
        {
#ifdef GHOST_WORLD_PASS
            worldDistortionCue *= voxelShade;
#else
            rgb *= voxelShade;
#endif
        }
        else if (ghostDistort == 3)
        {
            float lensRing = 1.0 - smoothstep(0.012, 0.035,
                abs(length(vary_texcoord0 - ghostDistortParams.yz) - 0.38));
#ifdef GHOST_WORLD_PASS
            worldDistortionAdditive += ghostWorldRadiance(
                color.rgb, lensRing * 0.25 * distort);
#else
            rgb += color.rgb * lensRing * 0.25 * distort;
#endif
        }
        else if (ghostDistort == 4)
        {
            float cue = 0.88 + 0.12
                * sin(fragCoord.y * 0.12 + ghostTime * 4.2) * distort;
#ifdef GHOST_WORLD_PASS
            worldDistortionCue *= cue;
#else
            rgb *= cue;
#endif
        }
        else if (ghostDistort == 5)
        {
#ifdef GHOST_WORLD_PASS
            worldDistortionAdditive += vec3(edge, 0.0, -edge)
                * 0.35 * distort;
#else
            rgb += vec3(edge, 0.0, -edge) * 0.35 * distort;
#endif
        }
        else if (ghostDistort == 6)
        {
            float cue = 0.82 + 0.18
                * ghost_hash(floor(vary_texcoord0 * vec2(12.0, 22.0))
                             + floor(ghostTime * 7.0)) * distort;
#ifdef GHOST_WORLD_PASS
            worldDistortionCue *= cue;
#else
            rgb *= cue;
#endif
        }
        else if (ghostDistort == 7)
        {
            float cue = 0.86 + 0.14
                * ghost_hash(vec2(floor(fragCoord.x / 13.0),
                                  floor(ghostTime * 8.0))) * distort;
#ifdef GHOST_WORLD_PASS
            worldDistortionCue *= cue;
#else
            rgb *= cue;
#endif
        }
    }
    if (ghostDistort == 8)
    {
        float cue = 1.0 - vhsBand * 0.22 * distort;
#ifdef GHOST_WORLD_PASS
        worldDistortionCue *= cue;
#else
        rgb *= cue;
#endif
    }
#ifdef GHOST_WORLD_PASS
    // Scalar distortion treatments dim/shape the same localized radiance in
    // beauty and bloom. Lens and chromatic cues remain beauty-only additions.
    worldRadiance *= worldDistortionCue;
    worldBloomRadiance *= worldDistortionCue;
#endif
#ifdef GHOST_WORLD_PASS
    vec3 worldLinearDissolveSource = worldDissolveSource;
#endif
    if (ghostWorldLinear != 0)
    {
        // actorghostF authors its looks in display/sRGB space.  Convert only
        // the native pre-tonemap live-wire call; late Ghost Studio overlays are
        // intentionally unchanged.
        rgb = vec3(ghostSrgbChannelToLinear(rgb.r),
                   ghostSrgbChannelToLinear(rgb.g),
                   ghostSrgbChannelToLinear(rgb.b));
#ifdef GHOST_WORLD_PASS
        worldLinearDissolveSource = vec3(
            ghostSrgbChannelToLinear(worldDissolveSource.r),
            ghostSrgbChannelToLinear(worldDissolveSource.g),
            ghostSrgbChannelToLinear(worldDissolveSource.b));
#endif
    }
#ifdef GHOST_WORLD_PASS
    // Apply animation, user brightness, and scalar distortion once in scene
    // linear space. Additive lens/chromatic cues participate in the same
    // terminal signal envelope. Dissolve then interpolates two linear endpoints
    // so its Layer source never inherits treatment animation or brightness.
    float worldSignal = flicker * max(ghostFx.w, 0.0);
    rgb = (rgb * worldDistortionCue + worldDistortionAdditive) * worldSignal;
    if (ghostLook == 10 && ghostCoverageLayerStrength >= 0.0)
    {
        rgb = mix(worldLinearDissolveSource, rgb,
                  worldDissolveTreatmentStrength);
    }
    // Beauty receives precisely the same localized radiance that the optional
    // glow replay publishes below. Fog/atmospherics then attenuate the combined
    // world result through the existing path.
    rgb += worldRadiance;
    if (ghostGlowOnly != 0)
    {
        // The caller only schedules this pass for the frozen signature list.
        // Publish the localized scan/rim/grid/wisp/beam radiance itself, never
        // the complete styled body. That keeps bloom spatially aligned with
        // beauty and prevents a second broad fullbright silhouette.
        vec3 emitted = worldBloomRadiance;
        // Extract the complete sky/underwater transmittance from the affine fog
        // function. Additive haze is background radiance and must never become
        // actor emission.
        vec3 foggedEmission = applySkyAndWaterFog(
            vary_position, getAdditiveColor(), getAtmosAttenuation(),
            vec4(emitted, 1.0)).rgb;
        vec3 foggedZero = applySkyAndWaterFog(
            vary_position, getAdditiveColor(), getAtmosAttenuation(),
            vec4(vec3(0.0), 1.0)).rgb;
        emitted = max(foggedEmission - foggedZero, vec3(0.0));
        // Gate treatment bloom by authoritative material coverage. Surviving
        // MASK texels stay binary and BLEND retains its authored ramp.
        float bloomCoverage = ghostLook == 10
            ? syntheticCoverage : worldAuthoredCoverage;
        float glow = max(max(emitted.r, emitted.g), emitted.b)
                   * bloomCoverage;
        frag_color = vec4(0.0, 0.0, 0.0, max(glow, 0.0));
        return;
    }
    // This dedicated replay executes before the main HDR target is resolved.
    // Match ordinary forward world alpha, including both sky atmosphere and
    // the underwater fog law. Ghost Studio's late display-space program never
    // compiles this branch.
    rgb = applySkyAndWaterFog(vary_position, getAdditiveColor(),
                              getAtmosAttenuation(), vec4(rgb, alpha)).rgb;
#endif
    frag_color = max(vec4(rgb, alpha), vec4(0));
}
