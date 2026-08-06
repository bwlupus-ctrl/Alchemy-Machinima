/**
 * @file prismLensF.glsl
 * @brief Samples the Prism Lens linear-HDR auxiliary beauty.
 */

uniform sampler2D prismLensMap;
uniform vec2 displayToCaptureScale;
uniform vec2 displayToCaptureOffset;
uniform vec2 retainedOrientationScale;
uniform vec2 retainedOrientationOffset;
uniform vec2 textureRegionScale;
uniform vec2 textureRegionOffset;
uniform int letterbox;
uniform vec3 barColorLinear;
uniform float edgeFeather;

// Option C: Cinematic lens & post-processing parameters
// x: Chromatic Aberration (0.0 to 1.0)
// y: Film Grain (0.0 to 1.0)
// z: CRT Scanlines (0.0 to 1.0)
// w: Exposure Bias EV (-4.0 to +4.0)
uniform vec4 prismLensOptics;

// Per-display TV/CRT screen effect packs. Every effect block below gates on
// its strength being > 0.001, so an all-zero pack (the default) leaves this
// shader's output bit-identical to the pre-effects composite.
// screenEffect0: x scanlines, y scanline density, z pixelate, w grayscale
// screenEffect1: x sepia, y static noise, z vertical roll, w roll speed
// screenEffect2: x tracking, y flicker, z chroma bleed, w vignette
// screenEffect3: x interlace, y dropout, z brightness, w reserved
// screenEffect4: x flip-H, y flip-V, z rotate-90, w environmental sheen (0..1)
uniform vec4 screenEffect0;
uniform vec4 screenEffect1;
uniform vec4 screenEffect2;
uniform vec4 screenEffect3;
uniform vec4 screenEffect4;
uniform float screenEffectTime;

// Environmental sheen palette (Feature B). Uploaded once per composite batch
// from the live sky/ambient; a Fresnel-weighted mix of these two colors is
// added over the feed only while screenEffect4.w > 0, so both are inert at the
// sheen-0 default.
uniform vec3 prismEnvSheenSky;    // reflected color toward the up hemisphere
uniform vec3 prismEnvSheenGround; // reflected color toward the down hemisphere

in vec2 prism_uv;
in vec3 prism_eye_pos;
in vec3 prism_eye_normal;

out vec4 frag_color;

// Shared hash for the animated screen effects. Deliberately a separate helper
// so the committed film-grain block below stays byte-for-byte untouched.
float tv_hash(vec2 p)
{
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453123);
}

void main()
{
    vec2 logical_uv = prism_uv * displayToCaptureScale +
                      displayToCaptureOffset;

    // Slice 1 is opaque-correct only. Edge feathering and transparent-pool
    // ordering are intentionally deferred; retain the uniform/API now.
    // Host-side Slice 1 clamps this reserved value non-negative. The branch
    // keeps the uniform live without adding feather math to this slice.
    if (edgeFeather < 0.0)
    {
        discard;
    }
    // FIT deliberately maps bars outside canonical capture UV. Test before
    // orientation and sampling so an edge texel is never smeared into a bar.
    if (letterbox != 0 &&
        (any(lessThan(logical_uv, vec2(0.0))) ||
         any(greaterThan(logical_uv, vec2(1.0)))))
    {
        // Alpha is the viewer's pre-tonemap Prism glow channel, not opacity.
        frag_color = vec4(barColorLinear, 0.0);
    }
    else
    {
        vec2 oriented_uv = logical_uv * retainedOrientationScale +
                           retainedOrientationOffset;

        // -----------------------------------------------------------------
        // Feature A: per-display screen-axis orientation. Rewrites the
        // sampling UV BEFORE every distortion block and the fetch, so it
        // composes cleanly with the effects below. Each branch gates on its
        // flag, so all-off leaves oriented_uv untouched (bit-identical).
        // Order is rotate, then horizontal, then vertical mirror.
        // -----------------------------------------------------------------
        if (screenEffect4.z > 0.5)
        {
            // Quarter-turn about the UV center. Non-square displays show the
            // turned feed stretched to the face, which is the intended
            // "portrait monitor" behavior.
            vec2 centered = oriented_uv - vec2(0.5);
            oriented_uv = vec2(0.5) + vec2(-centered.y, centered.x);
        }
        if (screenEffect4.x > 0.5)
        {
            oriented_uv.x = 1.0 - oriented_uv.x;
        }
        if (screenEffect4.y > 0.5)
        {
            oriented_uv.y = 1.0 - oriented_uv.y;
        }

        // -----------------------------------------------------------------
        // Per-display UV-space distortions. Each block rewrites oriented_uv
        // in place BEFORE the committed sampling below and is a strict no-op
        // at strength 0, so untouched displays sample identically.
        // -----------------------------------------------------------------

        // Pixelation: quantize the sampling grid into ever larger blocks.
        if (screenEffect0.z > 0.001)
        {
            float blocks = mix(512.0, 16.0, screenEffect0.z);
            oriented_uv = floor(oriented_uv * blocks) / blocks;
        }

        // Unstable V-sync vertical roll. The moving seam position is kept so
        // the post-sample stage can darken the classic sync band at the tear
        // instead of merely scrolling the picture.
        float roll_seam = 0.0;
        if (screenEffect1.z > 0.001)
        {
            float roll_rate = mix(0.1, 1.5, screenEffect1.w);
            roll_seam = fract(screenEffectTime * roll_rate * screenEffect1.z);
            oriented_uv.y = fract(oriented_uv.y + roll_seam);
        }

        // VHS tracking: horizontal tear/jitter on randomly gated scan rows.
        if (screenEffect2.x > 0.001)
        {
            float row = floor(oriented_uv.y * 180.0);
            float tear_noise = tv_hash(vec2(row, floor(screenEffectTime * 18.0)));
            float tear_gate = step(0.92 - screenEffect2.x * 0.25, tear_noise);
            float jitter = (tear_noise - 0.5) * 0.06 * screenEffect2.x * tear_gate;
            oriented_uv.x = clamp(oriented_uv.x + jitter, 0.0, 1.0);
        }

        vec2 texture_uv = oriented_uv * textureRegionScale +
                          textureRegionOffset;

        vec4 col;
        // Chromatic Aberration (Radial dispersion offset). The guard is
        // widened for the per-display chroma bleed, which rides the same
        // three-tap fetch as a horizontal fringe; at screenEffect2.z == 0 the
        // committed radial-only math runs byte-identical.
        if (prismLensOptics.x > 0.001 || screenEffect2.z > 0.001)
        {
            vec2 dist = (oriented_uv - vec2(0.5)) * prismLensOptics.x * 0.015;
            if (screenEffect2.z > 0.001)
            {
                dist.x += screenEffect2.z * 0.012;
            }
            vec2 r_uv = clamp(oriented_uv + dist, vec2(0.0), vec2(1.0)) * textureRegionScale + textureRegionOffset;
            vec2 b_uv = clamp(oriented_uv - dist, vec2(0.0), vec2(1.0)) * textureRegionScale + textureRegionOffset;
            float r = texture(prismLensMap, r_uv).r;
            float g = texture(prismLensMap, texture_uv).g;
            float b = texture(prismLensMap, b_uv).b;
            float a = texture(prismLensMap, texture_uv).a;
            col = vec4(r, g, b, a);
        }
        else
        {
            col = texture(prismLensMap, texture_uv);
        }

        // Exposure EV Bias Adjustment
        if (abs(prismLensOptics.w) > 0.001)
        {
            col.rgb *= exp2(prismLensOptics.w);
        }

        // CRT Scanlines
        if (prismLensOptics.z > 0.001)
        {
            float scanline = sin(oriented_uv.y * 600.0) * 0.5 + 0.5;
            col.rgb *= mix(1.0, scanline * 0.4 + 0.6, prismLensOptics.z);
        }

        // Film Grain Noise
        if (prismLensOptics.y > 0.001)
        {
            float grain = (fract(sin(dot(oriented_uv, vec2(12.9898, 78.233))) * 43758.5453) - 0.5) * prismLensOptics.y * 0.15;
            col.rgb += vec3(grain);
        }

        // -----------------------------------------------------------------
        // Per-display post-sample screen effects. Applied AFTER the committed
        // per-camera optics above; every block gates at > 0.001 so a zeroed
        // ScreenEffects pack leaves col untouched. Alpha (the pre-tonemap
        // Prism glow channel) is deliberately never modified.
        // -----------------------------------------------------------------

        // Grayscale desaturation (Rec. 709 luma).
        if (screenEffect0.w > 0.001)
        {
            float luma = dot(col.rgb, vec3(0.2126, 0.7152, 0.0722));
            col.rgb = mix(col.rgb, vec3(luma), screenEffect0.w);
        }

        // Sepia tone blend.
        if (screenEffect1.x > 0.001)
        {
            float sepia_luma = dot(col.rgb, vec3(0.2126, 0.7152, 0.0722));
            vec3 sepia_color = vec3(sepia_luma * 1.2, sepia_luma * 0.95,
                                    sepia_luma * 0.65);
            col.rgb = mix(col.rgb, sepia_color, screenEffect1.x);
        }

        // Per-display parametric scanlines. Deliberately its own block: the
        // fixed-density per-camera optics scanline above stays untouched and
        // both may layer.
        if (screenEffect0.x > 0.001)
        {
            float sl_density = mix(200.0, 1200.0, screenEffect0.y);
            float sl_wave = sin(oriented_uv.y * sl_density * 3.14159265) * 0.5 + 0.5;
            col.rgb *= mix(1.0, sl_wave * 0.45 + 0.55, screenEffect0.x);
        }

        // Interlace field line shimmer: alternate fields dim on a 30 Hz beat.
        if (screenEffect3.x > 0.001)
        {
            float field = step(0.5, fract(screenEffectTime * 30.0));
            float line_even = step(0.5, fract(oriented_uv.y * 240.0));
            float interlace_dim = abs(field - line_even);
            col.rgb *= mix(1.0, 0.75 + 0.25 * interlace_dim, screenEffect3.x);
        }

        // Analog hash static noise.
        if (screenEffect1.y > 0.001)
        {
            float static_noise = tv_hash(oriented_uv * 100.0 +
                vec2(screenEffectTime * 23.1, screenEffectTime * 47.3));
            col.rgb = mix(col.rgb, vec3(static_noise), screenEffect1.y * 0.45);
        }

        // Brightness flicker: whole-picture pulse re-rolled 24 times a second.
        if (screenEffect2.y > 0.001)
        {
            float flick = tv_hash(vec2(floor(screenEffectTime * 24.0), 1.0));
            col.rgb *= (1.0 - screenEffect2.y * 0.3 * flick);
        }

        // Momentary signal dropout darkening.
        if (screenEffect3.y > 0.001)
        {
            float drop_time = floor(screenEffectTime * 6.0);
            float drop_occ = step(0.93 - screenEffect3.y * 0.15,
                                  tv_hash(vec2(drop_time, 7.0)));
            col.rgb *= (1.0 - drop_occ * screenEffect3.y * 0.7);
        }

        // Dark V-sync band centred on the vertical-roll seam computed in the
        // UV stage, so the roll reads as a sync loss instead of a scroll.
        if (screenEffect1.z > 0.001)
        {
            float band = smoothstep(0.0, 0.06,
                abs(fract(oriented_uv.y - roll_seam + 0.5) - 0.5));
            col.rgb *= mix(1.0 - 0.85 * screenEffect1.z, 1.0, band);
        }

        // Vignette CRT edge darkening.
        if (screenEffect2.w > 0.001)
        {
            vec2 v_coord = (oriented_uv - vec2(0.5)) * 2.0;
            float v_dist = dot(v_coord, v_coord);
            col.rgb *= clamp(1.0 - v_dist * 0.45 * screenEffect2.w, 0.0, 1.0);
        }

        // Per-display linear brightness gain (-1..+1); 0 is exact identity.
        if (abs(screenEffect3.z) > 0.001)
        {
            col.rgb *= (1.0 + screenEffect3.z);
        }

        // -----------------------------------------------------------------
        // Feature B: environmental sheen. A Fresnel-weighted environment
        // reflection is added OVER the finished picture for a glossy-panel
        // look. The environment is approximated by a two-color hemisphere
        // (sky/ground) sampled along the reflected view vector, tinted by the
        // live scene lighting on the CPU side - a deliberately cheap stand-in
        // for a full reflection-probe cube fetch (see the .cpp note). Gated on
        // screenEffect4.w, and the final mix(col, col + X, 0.0) is an exact
        // identity, so sheen 0 is bit-identical to the pre-sheen composite.
        // -----------------------------------------------------------------
        if (screenEffect4.w > 0.001)
        {
            vec3 N = normalize(prism_eye_normal);
            vec3 Vd = normalize(-prism_eye_pos); // fragment -> camera, eye space
            // Displays are two-sided; face the normal toward the viewer.
            if (dot(N, Vd) < 0.0)
            {
                N = -N;
            }
            float fresnel = pow(1.0 - max(dot(N, Vd), 0.0), 5.0);
            vec3 refl = reflect(-Vd, N);
            float hemi = clamp(refl.y * 0.5 + 0.5, 0.0, 1.0);
            vec3 env = mix(prismEnvSheenGround, prismEnvSheenSky, hemi);
            col.rgb = mix(col.rgb, col.rgb + env * fresnel, screenEffect4.w);
        }

        // Preserve the auxiliary beauty alpha as well as RGB.
        frag_color = col;
    }
}
