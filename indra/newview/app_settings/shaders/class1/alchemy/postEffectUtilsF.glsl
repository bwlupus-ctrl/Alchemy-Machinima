/**
 * @file postEffectUtilsF.glsl
 * @brief Shared post-process effect helpers auto-linked into every program
 *        whose LLGLSLShader sets `mFeatures.hasPostEffects = true`
 *        (see llviewershadermgr.cpp).
 *
 * Three consumers currently link this file:
 *   - colorCorrectF.glsl  — calls applyChromaticAberration and computeLensFlare
 *                           in LINEAR space, before tonemap/gamma/LUT.
 *   - onLensFiltersF.glsl — calls applyGradND and applyPolarizer in LINEAR HDR,
 *                           in a pre-pass ahead of bloom/flare generation.
 *   - blitWithEffectsF.glsl — calls applyVignette, applyCVDCompensation,
 *                           applyFilmGrain, applyDither, applyPreview in
 *                           DISPLAY space, after all grading.
 *
 * Public entry points, grouped by logical pass:
 *
 *   LINEAR SPACE (colorCorrectF)
 *     vec4 applyChromaticAberration(sampler2D tex, vec2 uv)
 *     vec3 computeLensFlare       (sampler2D diff, sampler2D depth, vec2 uv)
 *
 *   LINEAR HDR PRE-PASS (onLensFiltersF)
 *     vec3 applyGradND            (vec3 color, vec2 uv, sampler2D depth)
 *     vec3 applyPolarizer         (vec3 color, vec2 uv, sampler2D depth, float exposure_scale)
 *
 *   DISPLAY SPACE (blitWithEffectsF)
 *     vec3 applyVignette          (vec3 color, vec2 uv)
 *     vec3 applyCVDCompensation   (vec3 color)
 *     vec3 applyFilmGrain         (vec3 color, vec2 fragCoord)
 *     vec3 applyDither            (vec3 color, vec2 fragCoord)
 *     vec3 applyPreview           (vec3 color)
 *
 * Conventions used throughout:
 *   - Every effect has an `amount <= 0` fast-path that returns the input
 *     unchanged, so the call site can unconditionally chain them.
 *   - Aspect correction, where needed, uses the branchless form
 *     `max(vec2(aspect, 1.0/aspect), 1.0)` — one axis stays 1.0 and the
 *     other carries the ratio, so the shorter axis's half-height is the
 *     normalization unit regardless of viewport shape.
 *   - Shared helpers (hash12, frameNoiseOffset, LUMA) live at the top.
 *   - Temporal decorrelation uses uFrameId hashed into a random UV offset,
 *     not a translation, so the noise fully reshuffles between frames
 *     instead of visibly scrolling.
 *
 * Several uniforms arrive in a CPU-baked form (see pipeline.cpp) — chromatic
 * aberration's amount/falloff/angle are the current examples. Those are
 * flagged inline next to their declarations.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2026, Rye <rye@alchemyviewer.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * $/LicenseInfo$
 */

/*[EXTRA_CODE_HERE]*/

// =============================================================================
// Shared inputs
// =============================================================================

// Framebuffer resolution in pixels. Used for aspect correction so effects
// stay geometrically correct regardless of window size.
uniform vec2 uResolution;

// Atmospheric HDR scale. Provided by LLSettingsSky::applyToShader via
// mShaderList. Effects that emulate sun-lit phenomena (glow, starburst) scale
// by this so they read at the same relative brightness as the sky itself.
uniform float sky_hdr_scale;

// Frame counter, wraps at 2^32. Used by temporally-decorrelated noise
// effects (film grain, dither). Set to 0 for deterministic static output.
uniform uint uFrameId;

// Rec.709 luma weights — used by effects that need perceptual brightness
// (film grain luma weighting, etc.).
const vec3 LUMA = vec3(0.2126, 0.7152, 0.0722);

// Cheap 2D → [0,1) hash used by noise-based effects (film grain, dither).
// Derived from Dave Hoskins' hash collection — quick and good enough for
// per-pixel decorrelated noise without needing a texture lookup.
float hash12(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

// Per-frame pixel-space offset for temporally decorrelated noise. The frame
// id is hashed into two independent axes and scaled by 1024 so the sample
// location jumps to an effectively uncorrelated region each frame — giving
// a full pattern reshuffle instead of the scrolling you'd get from just
// adding the frame id to the input. Pass `animate=false` to freeze the
// pattern (useful for stills and debugging).
vec2 frameNoiseOffset(bool animate)
{
    uint  frame = animate ? uFrameId : 0u;
    float ff    = float(frame);
    return vec2(hash12(vec2(ff, 13.7)),
                hash12(vec2(ff, 91.3))) * 1024.0;
}


// =============================================================================
// Chromatic aberration
// =============================================================================
//
// Radial per-channel UV offset. Samples R and B along a (rotated) radial
// direction, leaves G and A at the center. Offset magnitude grows with
// distance from screen center so the center stays clean and fringing
// intensifies at the edges, mimicking real lens dispersion.
//
// uCAOffsetR / uCAOffsetB are directions in the (radial, tangential) basis
// built from the rotated radial vector. The x component rides the radial
// axis, the y component rides its perpendicular — this is what lets the
// effect do both classic SLR radial fringing and glitchier axis-swap looks
// from the same code path.

// Note: uCAAmount, uCAFalloff, and uCAAngleSinCos arrive in a pre-baked form
// from the CPU (pipeline.cpp) — squared-and-scaled strength, reciprocal
// falloff, and a (sin, cos) pair — so the shader avoids redoing a per-pixel
// mul / divide / sincos. The slider ranges shown in comments are the
// *user-facing* values before baking.
uniform float uCAAmount;                           // artist range [0, 1]: 0.15 SLR, 0.35 vintage,
                                                  //   0.6 anamorphic, 0.9 VHS. Uploaded pre-squared × 0.02.
uniform float uCAFalloff;                          // artist range [0.5, 4]: 0.5 corners-only, 1.0 SLR,
                                                  //   2.0 broad, 4.0 nearly uniform. Uploaded as 1/value.
uniform vec2  uCAAngleSinCos;                      // artist range [0, 360°] rotating the fringe axis
                                                  //   (0 radial, 45 diagonal, 90 tangential).
                                                  //   Uploaded as (sin, cos) of the radian-converted angle.
uniform vec2  uCAOffsetR;                          // R channel offset in (radial, tangential). Default: inward.
uniform vec2  uCAOffsetB;                          // B channel offset in (radial, tangential). Default: outward.
                                                  // Glitch preset: vec2(1,0) / vec2(0,1) = R→B swap.
uniform float uCAAnisotropy;                       // [-1, 1] axis stretch of the falloff region.
                                                  //         -1 = horizontal band only
                                                  //         +1 = vertical band only

vec4 applyChromaticAberration(sampler2D tex, vec2 uv)
{
    // Fast path when the effect is disabled — skip the extra taps.
    if (uCAAmount <= 0.0)
        return texture(tex, uv);

    // Vector from screen center, with anisotropy squashing one axis so the
    // fringe band can be biased horizontal or vertical.
    vec2 dir = uv - 0.5;
    dir *= vec2(1.0 - max( uCAAnisotropy, 0.0),
                1.0 - max(-uCAAnisotropy, 0.0));

    // Aspect correction as a branchless per-axis scale: one component is
    // always 1.0, the other is the larger-axis ratio. Applied to both the
    // per-pixel vector and the half-frame reference, so their ratio stays
    // meaningful regardless of viewport shape.
    float aspect = uResolution.x / max(uResolution.y, 1.0);
    vec2  scale  = max(vec2(aspect, 1.0 / max(aspect, 1e-4)), 1.0);

    // Normalize distance to [0, 1] over the aspect-corrected half-diagonal,
    // then raise to `uCAFalloff` (CPU already took the reciprocal of the
    // user-facing slider) so higher slider values produce MORE CA across
    // more of the frame.
    float normDist = clamp(length(dir * scale) / (0.5 * length(scale)), 0.0, 1.0);
    float dist     = pow(normDist, uCAFalloff);

    // Build the rotated radial basis (and its perpendicular) as a 2x2
    // matrix so uCAOffset*.y addresses the tangential axis via a single
    // matrix-vector multiply. Sin/cos come pre-computed from the CPU.
    float s = uCAAngleSinCos.x;
    float c = uCAAngleSinCos.y;
    vec2  rotDir = vec2(c * dir.x - s * dir.y,
                        s * dir.x + c * dir.y);
    mat2  basis  = mat2(rotDir, vec2(-rotDir.y, rotDir.x));

    // uCAAmount is already squared and scaled by 0.02 on the CPU, giving a
    // gentle slider toe and a plausible worst-case lens-like offset at 1.0.
    float strength = uCAAmount * dist;
    vec2  offR = basis * uCAOffsetR;
    vec2  offB = basis * uCAOffsetB;

    // Three taps: R offset, center (G + A), B offset.
    vec4 diff;
    diff.r  = texture(tex, uv + offR * strength).r;
    diff.ga = texture(tex, uv).ga;
    diff.b  = texture(tex, uv + offB * strength).b;
    return diff;
}


// =============================================================================
// Lens flare — multi-source cinematic stack (sun + cine-rig projectors)
// =============================================================================
//
// Screen-space approximation of a cinematic lens flare. The per-source body
// (flareForSource) draws the full optical stack — glow, anamorphic streak,
// ghosts, halo, starburst, plus the gated "polished stack" additions (iris
// ghosts, dispersion rings, spectral circles/arc, chromatic fringing, lens
// warp) — and computeLensFlare sums it over the sun source and up to
// AL_CINE_FLARE_MAX rig-projector sources uploaded by pipeline.cpp.
//
// The sun is driven by a CPU-computed UV position and visibility (on-screen
// fade), plus a multi-tap depth occlusion check done here so geometry in
// front of the sun attenuates the flare smoothly. Rig sources arrive with
// CPU-computed visibility and skip the in-shader occlusion probe.
//
// Each sub-effect is gated by its own intensity uniform (0 = disabled, else
// acts as a brightness multiplier), so this single function backs a variety
// of looks without branching from the call site. All new elements default to
// 0 (off): with those defaults and uCineFlareCount == 0 the output is
// bit-identical to the original sun-only implementation.

// ---- Driver inputs (set by the viewer each frame) --------------------------
uniform float uLensFlareStrength;                 // master on/off + intensity
uniform vec2  uLensFlareSunPos;                   // sun position in UV space
uniform float uLensFlareSunVisibility;            // CPU-side on-screen fade
uniform vec3  uLensFlareLightColor;               // artist tint applied to whole flare

// ---- Depth occlusion -------------------------------------------------------
uniform float uLensFlareOcclusionRadius;          // Poisson disk radius in UV space
uniform int   uLensFlareOcclusionTaps;            // 1..32 — more taps = smoother partial occlusion

// ---- Anamorphic streak -----------------------------------------------------
uniform float uLensFlareStreakLength;             // horizontal extent in UV space
uniform float uLensFlareStreakFalloff;            // exponential falloff along the streak
uniform float uLensFlareStreakWidth;              // vertical half-thickness (gaussian sigma)
uniform float uLensFlareStreakIntensity;          // streak brightness multiplier
uniform vec3  uLensFlareStreakTint;               // anamorphic blue
uniform float uLensFlareChromaticSpread;          // vertical R/B offset for chromatic fringing

// ---- Central glow (intensity doubles as on/off) ----------------------------
uniform float uLensFlareGlow;                     // 0 = off, else brightness
uniform float uLensFlareGlowRadius;               // falloff radius in UV space
uniform float uLensFlareGlowFalloff;              // gaussian exponent sharpness

// ---- Ghosts: soft disks along the sun→center axis --------------------------
uniform float uLensFlareGhost;                    // 0 = off, else brightness
uniform int   uLensFlareGhostCount;               // number of ghosts to place
uniform float uLensFlareGhostSpacing;             // step size along the axis

// ---- Halo: ring opposite the sun -------------------------------------------
uniform float uLensFlareHalo;                     // 0 = off, else brightness
uniform float uLensFlareHaloRadius;               // ring radius in UV space
uniform float uLensFlareHaloWidth;                // ring thickness

// ---- Starburst: angular spikes radiating from the sun ----------------------
uniform float uLensFlareStarburst;                // 0 = off, else brightness
uniform int   uLensFlareStarburstSpikes;          // primary angular frequency — cos(θ·N) has 2N
                                                  //   lobes per full turn, so 4 here = 8 visible spikes
uniform float uLensFlareStarburstSharpness;       // pow() exponent — higher = tighter spikes
uniform float uLensFlareStarburstFalloff;         // radial decay rate from the sun

// ---- Cine rig flare sources (uploaded by pipeline.cpp each frame) ----------
// Screen-space projector sources from the cinematic light rig. When
// uCineFlareCount is 0 (the default) the rig loop is skipped entirely and
// computeLensFlare's output is bit-identical to the sun-only implementation.
#define AL_CINE_FLARE_MAX 8
uniform sampler2D uCineFlareShaft;                // isolated projector-volumetric RGB, never daylight
uniform int  uCineFlareCount;                     // active rig sources, 0..AL_CINE_FLARE_MAX
uniform vec4 uCineFlareA[AL_CINE_FLARE_MAX];      // per source: (uv.x, uv.y, visibility, deviceDepth)
uniform vec4 uCineFlareColor[AL_CINE_FLARE_MAX];  // per source: (linRGB, +live scale / -release scale)
uniform float uCineFlareThreshold;                // [0, 6] linear-HDR luma the emission-point
                                                  //   neighborhood must exceed; default 0.25.
uniform float uCineFlareProbeRadius;              // [0, 0.2] screen-height fraction for the
                                                  //   pixel-circular persistence ring; default 0.06.

// ---- Polished-stack additions ----------------------------------------------
// Every element below is gated by its own intensity/amount uniform whose
// DEFAULT IS 0 (or 1.0 for the pure multipliers), so with defaults in place
// none of this math runs and the legacy flare output is bit-unchanged.

// Ghost / halo chromatic fringing (0 = legacy monochrome discs/ring).
uniform float uLensFlareGhostChroma;              // [0, 0.1]  R/B ghost-disc shift along the axis
uniform float uLensFlareHaloChroma;               // [0, 0.05] R/B halo-ring radius split (UV units)

// N-gon iris ghosts — polygonal aperture reflections marching along the axis.
uniform float uLensFlareIris;                     // 0 = off, else brightness
uniform int   uLensFlareIrisCount;                // [1, 8]   number of iris ghosts
uniform int   uLensFlareIrisSides;                // [3, 12]  aperture blade count (polygon sides)
uniform float uLensFlareIrisSize;                 // [0.01, 0.2] base polygon radius in UV

// Chromatic dispersion rings — concentric rainbow rings around the source.
uniform float uLensFlareRing;                     // 0 = off, else brightness
uniform float uLensFlareRingRadius;               // [0.05, 0.8] first ring radius in UV
uniform float uLensFlareRingWidth;                // [0.01, 0.3] ring thickness in UV
uniform float uLensFlareRingDispersion;           // [0, 3]   spectral hue sweep across the ring
uniform int   uLensFlareRingCount;                // [1, 4]   number of rings

// Spectral lens circles — tinted copies of the source along the optical axis.
uniform float uLensFlareCircle;                   // 0 = off, else brightness
uniform float uLensFlareCircleScale;              // [0.02, 0.5] circle radius in UV
uniform float uLensFlareCircleSpacing;            // [0.02, 0.5] step along the axis per circle
uniform int   uLensFlareCircleCount;              // [1, 8]   number of circles

// Spectral arc — partial rainbow arc on the far side of frame center.
uniform float uLensFlareArc;                      // 0 = off, else brightness

// Lens warp — barrel (+) / pincushion (-) distortion of the "glass" layers
// (ghosts, halo, iris, rings, circles, arc). Streak and starburst stay straight.
uniform float uLensFlareWarp;                     // [-1, 1]  0 = off (layers use raw UV)

// Streak two-tone tip — blends the streak tint toward a tip color with
// horizontal distance from the source (core keeps uLensFlareStreakTint).
uniform float uLensFlareStreakTipAmount;          // [0, 1]   0 = off (single-tone streak)
uniform vec3  uLensFlareStreakTipTint;            // tip color the streak fades toward

// Source-color blend — pulls each element's energy tint toward the source's
// own color (luminance-preserving), instead of the raw screen sample.
uniform float uLensFlareSrcColorAmount;           // [0, 1]   0 = off (screen-sampled tint)

// Master one-knob scale over the whole composited flare (sun + rig).
uniform float uLensFlareMaster;                   // default 1.0 = off (branch not taken)

// =============================================================================
// Night Mask  (subject-anchored darkness mask; pre-tonemap on-lens filter)
// =============================================================================
//
// Fakes night by darkening (and optionally grading) everything OUTSIDE a
// shape centred on a target avatar: 0 = camera-facing plane (view-depth
// cutoff past the anchor), 1 = sphere, 2 = cube yaw-aligned to the subject.
// Evaluated FIRST in the on-lens stack (before Graduated ND / Polarizer) so
// the ReShade raw-scene capture downstream sees the masked result — Night
// Mask is a world-lighting fake, not a camera-lens artifact, and must be
// visible to RTGI-style bridges the way GradND/Polarizer deliberately are
// not (see the ordering comment in LLPipeline::applyOnLensFilters).
//
// Major1 (Codex review): deferredUtil's getPosition() reconstructs view-space
// position using the SHARED `inv_proj` uniform — but LLVertexBuffer::
// drawArrays()'s gGL.syncMatrices() call re-uploads inv_proj from the LIVE GL
// projection matrix whenever its hash has changed since this shader was last
// bound, and that happens INSIDE drawArrays(), i.e. AFTER any CPU-side
// uniform override and BEFORE the actual glDrawArrays (llrender.cpp
// syncMatrices). renderFinalize() runs after render_ui() has already touched
// the live matrix stack (and some paths, e.g. Scene Monitor, reset it
// entirely mid-frame), so an override of the MANAGED inv_proj is not
// trustworthy here — depending on it silently reconstructs from the wrong
// projection on exactly the frames this was meant to guard against.
//
// Fix: night_mask_inv_proj is a DEDICATED uniform nothing else ever
// re-syncs, uploaded from inverse(gGLLastProjection) by
// LLPipeline::applyOnLensFilters, and nightMaskGetPosition() below
// reconstructs view-space position locally with the exact same math
// deferredUtil's getPosition()/getScreenCoordinate()/getDepth() use, just
// against this dedicated matrix instead of the managed one. GradND/Polarizer
// never call getPosition() and are untouched by any of this.
//
// The smoothed subject anchor is transformed into VIEW SPACE on the CPU with
// that SAME gGLLastModelView-derived frame into night_mask_anchor_view, so
// both sides of every comparison below live in one coherent current-frame
// view space.
//
// Sphere and the camera-facing plane are rotation-invariant / view-depth
// only, so they read night_mask_anchor_view directly. The cube alone needs
// the subject's yaw: night_mask_box_basis is a CPU-composed 3x3 rotation
// (view-space rotation x inverse subject yaw) that maps a view-space delta
// straight into the box's yaw-aligned local axes — no agent-space transform
// happens in-shader.
//
// Transparency limitation: post-water alpha does not write depth, so a pixel
// behind translucent water/glass classifies using the OPAQUE surface behind
// it, not the transparent surface itself. This is a known, accepted v1
// limitation (matches how GradND/Polarizer's own sky-confine depth test
// behaves) — no in-shader fix.
//
// Packed uniforms:
//   night_mask_params  = (active [0/1], distance, feather, darkness)
//   night_mask_params2 = (tint_strength, desaturation, unused, unused)
uniform vec4  night_mask_params;
uniform vec4  night_mask_params2;
uniform vec3  night_mask_anchor_view;
uniform mat3  night_mask_box_basis;
uniform vec3  night_mask_tint;
uniform int   night_mask_shape;
uniform mat4  night_mask_inv_proj;

// Local view-space reconstruction against the DEDICATED night_mask_inv_proj
// uniform — replicates deferredUtil.glsl's getPosition() math exactly
// (getScreenCoordinate: uv*2-1; NDC z from depth*2-1) but never touches the
// managed inv_proj/getPosition(), which the outer comment block explains is
// unsafe for this pass.
vec4 nightMaskGetPosition(vec2 pos_screen, sampler2D depth)
{
    float d  = texture(depth, pos_screen).r;
    vec2  sc = pos_screen * 2.0 - vec2(1.0, 1.0);
    vec4  ndc = vec4(sc.x, sc.y, 2.0 * d - 1.0, 1.0);
    vec4  pos = night_mask_inv_proj * ndc;
    pos /= pos.w;
    pos.w = 1.0;
    return pos;
}

// Exact signed-distance box (Inigo Quilez); p and b already share the box's
// local axes, b is the half-extent (uniform per M6: vec3(distance)).
float nightMaskBoxSDF(vec3 p, vec3 b)
{
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

vec3 applyNightMask(vec3 color, vec2 uv, sampler2D depth)
{
    // "active" folds Enabled + HDR-required + valid-target-this-frame into a
    // single CPU-resolved switch (LLPipeline::updateNightMaskAnchor). Cheapest
    // possible early-out, mirroring applyGradND's density<=0.0 convention —
    // strength-0 must be exactly identity.
    if (night_mask_params.x <= 0.5)
        return color;

    float mask_distance = max(night_mask_params.y, 0.0);
    float feather        = max(night_mask_params.z, 0.0);
    float darkness        = clamp(night_mask_params.w, 0.0, 1.0);
    float tint_strength   = clamp(night_mask_params2.x, 0.0, 1.0);
    float desaturation    = clamp(night_mask_params2.y, 0.0, 1.0);

    vec3 view_pos = nightMaskGetPosition(uv, depth).xyz;
    vec3 delta    = view_pos - night_mask_anchor_view;

    float sd;
    if (night_mask_shape == 1)
    {
        // Sphere: rotation-invariant, plain view-space distance.
        sd = length(delta) - mask_distance;
    }
    else if (night_mask_shape == 2)
    {
        // Cube: rotate the view-space delta into the subject's yaw-aligned
        // local axes (CPU-composed basis), then an exact box SDF.
        vec3 local = night_mask_box_basis * delta;
        sd = nightMaskBoxSDF(local, vec3(mask_distance));
    }
    else
    {
        // Camera-facing plane: darkens beyond `distance` metres of VIEW DEPTH
        // past the anchor. View space looks down -Z, so depth grows as -z.
        sd = (-view_pos.z) - (-night_mask_anchor_view.z) - mask_distance;
    }

    float factor = feather > 1e-4 ? smoothstep(0.0, feather, sd) : step(0.0, sd);

    color *= mix(1.0, darkness, factor);

    float luma = dot(color, LUMA);
    color = mix(color, vec3(luma), desaturation * factor);
    color = mix(color, color * night_mask_tint, tint_strength * factor);

    return color;
}

// =============================================================================
// Graduated ND  (pre-tonemap exposure region)
// =============================================================================
//
// A neutral-density wedge applied in LINEAR HDR, before exposure roll-off and
// the tonemapper. Because this runs on physically-linear scene radiance, one
// "stop" of density is an exact halving of light (exp2(-stops)) — the honest
// optical behaviour a post-tonemap ReShade pass can only approximate.
//
// The wedge is a tilted straight-line gradient (position + angle + softness +
// flip). Two optional refinements make it "auto" like a DP angling a grad:
//   - sky confine : multiply the mask by the far-plane sky gate so foreground
//                   geometry rising into the darkened band is spared. Depth is
//                   ground truth here (sky == hardware depth ~1.0), so no
//                   colour/blueness heuristic is needed.
//   - sun weight  : add density toward the CPU-projected sun UV to hold the
//                   hottest sky, eased in only when the sun is actually on
//                   screen (has_sun).
//
// Packed uniforms:
//   gradnd_params  = (density_stops, angle_rad, position, softness)
//   gradnd_params2 = (sky_confine [0..1], sun_weight, sun_radius, flip [0/1])
//   gradnd_sun     = (sun_uv.x, sun_uv.y, has_sun [0/1], aspect)
uniform vec4 gradnd_params;
uniform vec4 gradnd_params2;
uniform vec4 gradnd_sun;

vec3 applyGradND(vec3 color, vec2 uv, sampler2D depth)
{
    float density = gradnd_params.x;
    // Cheapest possible early-out — density 0 (feature off) returns the input
    // untouched, bit-for-bit, before any texture fetch or transcendental.
    if (density <= 0.0)
        return color;

    float angle    = gradnd_params.y;
    float position = gradnd_params.z;
    float softness = max(gradnd_params.w, 1e-3);

    float sky_confine = clamp(gradnd_params2.x, 0.0, 1.0); // guard: mix() would invert the ND if >1
    float sun_weight  = gradnd_params2.y;
    float sun_radius  = max(gradnd_params2.z, 1e-3);
    float flip        = gradnd_params2.w;

    float aspect = max(gradnd_sun.w, 1e-3);

    // --- Straight-line wedge (aspect-corrected so tilt reads at true angle) --
    vec2 p = uv - vec2(0.5, position);
    p.x *= aspect;
    float axis = -p.x * sin(angle) + p.y * cos(angle);
    // GL framebuffer UV is y-up (0 = bottom, 1 = top), so a positive axis is
    // toward the top of frame. t must rise with axis so the unflipped wedge
    // darkens the SKY (top); flip inverts it to hold a bright foreground.
    float t = clamp(0.5 + (axis / softness) * 0.5, 0.0, 1.0);
    t = smoothstep(0.0, 1.0, t);
    if (flip > 0.5)
        t = 1.0 - t;

    // --- Sun weighting (screen-space proxy), gated on on-screen sun ----------
    if (sun_weight > 0.0)
    {
        vec2 dv = uv - gradnd_sun.xy;
        dv.x *= aspect;
        float near_sun = clamp(1.0 - length(dv) / sun_radius, 0.0, 1.0);
        near_sun *= gradnd_sun.z; // has_sun
        t = clamp(t * (1.0 + near_sun * sun_weight), 0.0, 1.0);
    }

    // --- Confine to sky (far plane) — protects foreground rising into band ---
    if (sky_confine > 0.0)
    {
        float d = texture(depth, uv).r;
        // Tight far-plane band — only true sky (matches the lens-flare sky test).
        // A looser threshold lets distant-but-solid geometry read as partial sky.
        float sky = smoothstep(0.9999, 1.0, d);
        t *= mix(1.0, sky, sky_confine);
    }

    // Linear ND: each stop halves the light. Exact because we are pre-tonemap.
    return color * exp2(-density * t);
}

// =============================================================================
// Polarizer  (pre-tonemap on-lens filter)
// =============================================================================
//
// A circular-polarizer approximation, applied in LINEAR HDR alongside the grad
// ND. Two independent jobs:
//
//   Glare cut  — the flagship reason to run early. A specular highlight is a
//                huge linear value here; knocking it down BEFORE the tonemap
//                lets the filmic curve roll the reduced value off naturally and
//                recover texture inside the reflection. Post-tonemap the same
//                cut only greys an already-clipped near-white pixel. Gated on
//                bright + low-chroma + mid-depth so it targets reflections off
//                water/glass and spares sky and the subject.
//
//   Sky deepen — darken (exposure-domain, so early is correct) plus a gentle
//                saturation push, confined to the far-plane sky. An optional
//                auto-band eases the effect off toward the sun (screen-space
//                proxy for the real 90-degrees-from-sun polarized band).
//
// Shares the on-lens sun uniform (gradnd_sun = uv.xy, has_sun, aspect). Packed:
//   polarizer_params  = (strength, sky_saturation, sky_darken_stops, band_radius)
//   polarizer_params2 = (glare_stops, glare_threshold, glare_max_depth, auto_band)
uniform vec4 polarizer_params;
uniform vec4 polarizer_params2;

// exposure_scale = the effective exposure (manual * auto) that the tonemapper
// will apply downstream. The glare gate thresholds absolute luma, which does not
// commute with exposure, so it evaluates luma*exposure_scale to stay calibrated
// even though this filter runs before exposure is applied.
vec3 applyPolarizer(vec3 color, vec2 uv, sampler2D depth, float exposure_scale)
{
    float strength = clamp(polarizer_params.x, 0.0, 1.0);
    if (strength <= 0.0)
        return color; // off — untouched, before any fetch

    float sky_sat     = polarizer_params.y;
    float sky_darken  = polarizer_params.z;
    float band_radius = max(polarizer_params.w, 1e-3);

    float glare_stops = polarizer_params2.x;
    float glare_thr   = max(polarizer_params2.y, 1e-3);
    float glare_depth = polarizer_params2.z;
    float auto_band   = polarizer_params2.w;

    float aspect = max(gradnd_sun.w, 1e-3);
    float d = texture(depth, uv).r;

    // --- Glare cut: bright, low-chroma, mid-depth specular knockdown ---------
    if (glare_stops > 0.0)
    {
        float luma = dot(color, LUMA);
        float maxc = max(color.r, max(color.g, color.b));
        float minc = min(color.r, min(color.g, color.b));
        float sat  = (maxc - minc) / max(maxc, 1e-4);

        float glare = smoothstep(glare_thr, glare_thr * 2.0, luma * exposure_scale); // exposed HDR bright
        glare *= clamp(1.0 - sat * 2.0, 0.0, 1.0);                  // low-chroma
        glare *= clamp((glare_depth - d) / 0.02, 0.0, 1.0);        // spare sky/far
        glare *= strength;

        color *= exp2(-glare_stops * glare);
    }

    // --- Sky deepen: darken + gentle saturation, confined to sky -------------
    float sky = smoothstep(0.9999, 1.0, d);
    if (sky > 0.0)
    {
        float skyW = sky * strength;

        if (auto_band > 0.5)
        {
            vec2 dv = uv - gradnd_sun.xy;
            dv.x *= aspect;
            float band = clamp(length(dv) / band_radius, 0.0, 1.0); // 0 at sun -> 1 away
            skyW *= mix(1.0, band, gradnd_sun.z);                   // only when sun on-screen
        }

        vec3 deep = mix(vec3(dot(color, LUMA)), color, sky_sat); // saturation push
        deep = max(deep, vec3(0.0)); // sat>1 extrapolates — clamp to avoid negative-channel hue shift
        deep *= exp2(-sky_darken);                               // sky darken (stops)
        color = mix(color, deep, skyW);
    }

    return color;
}

// -----------------------------------------------------------------------------
// Per-source flare body.
//
// The complete optical stack for ONE light source, parameterized by screen-UV
// position, pre-computed visibility, and source color so the same body serves
// both the sun and the cine-rig projector sources.
//
// The original sun-only elements (depth occlusion, central glow, anamorphic
// streak, ghosts, halo, starburst) are ported VERBATIM — same operations, same
// order, same constants — so the sun call is bit-identical to the previous
// single-source implementation. The "polished stack" additions are strictly
// additive and each early-outs on its own intensity uniform (default 0), so
// they add no math to the legacy path.
//
// occTaps:  >= 0 — run the Poisson depth-occlusion probe (clamped 1..32; the
//                  sun passes its uniform through unchanged).
//           <  0 — skip in-shader occlusion entirely. Rig sources use this:
//                  their visibility arrives CPU-computed per frame, and the
//                  sky-plane depth test below would read "always occluded"
//                  for an in-scene light prim anyway.

// Cosine spectrum palette — rainbow hue sweep for the dispersion elements.
vec3 flareSpectrum(float t)
{
    return 0.5 + 0.5 * cos(6.2831853 * (t + vec3(0.0, 0.33, 0.67)));
}

// N-gon "distance" via angle fold: length scaled so iso-lines are regular
// polygons with `n` sides (apothem-normalized). rot spins the polygon.
float flareNgonDist(vec2 p, float n, float rot)
{
    float seg = 6.2831853 / max(n, 3.0);
    float a   = atan(p.y, p.x) + rot;
    a = mod(a, seg) - seg * 0.5;
    return length(p) * cos(a) / cos(seg * 0.5);
}

vec3 flareForSource(sampler2D diffuse, sampler2D depth, vec2 uv,
                    vec2 srcUV, float srcDepth, float vis,
                    vec3 srcColor, int occTaps)
{
    vec2 sun_uv = srcUV;

    // Per-source HDR scale for the atmosphere-relative elements (glow,
    // starburst). The SUN scales by sky_hdr_scale so it reads at the same
    // relative brightness as the sky it is painted against; RIG sources
    // (occTaps < 0) derive brightness from the isolated shaft texture and
    // use 1.0 here so a projector's star/glow does not dim with the sky
    // indoors or at night. For the sun this is exactly sky_hdr_scale
    // (byte-parity).
    float src_hdr_scale = (occTaps >= 0) ? sky_hdr_scale : 1.0;

    // -------------------------------------------------------------------
    // Depth-based occlusion.
    //
    // CPU-side visibility only tracks whether the sun is on-screen; it
    // doesn't know about intervening geometry. Probe a Poisson disk of
    // depth taps around the sun's UV and count how many are at the far
    // plane (i.e. sky). This gives smooth partial occlusion when the sun
    // is half-behind an object.
    // -------------------------------------------------------------------
    if (occTaps >= 0 && all(greaterThanEqual(sun_uv, vec2(0.0))) && all(lessThanEqual(sun_uv, vec2(1.0))))
    {
        // Pre-baked Poisson disk samples, good spatial distribution.
        const vec2 taps[32] = vec2[32](
            vec2( 0.0,     0.0),
            vec2(-0.326,  -0.406),
            vec2(-0.840,  -0.074),
            vec2(-0.196,   0.457),
            vec2( 0.498,   0.336),
            vec2( 0.106,  -0.747),
            vec2( 0.736,  -0.290),
            vec2( 0.423,   0.767),
            vec2(-0.621,   0.572),
            vec2( 0.890,   0.156),
            vec2(-0.453,  -0.780),
            vec2( 0.215,  -0.945),
            vec2(-0.928,   0.326),
            vec2( 0.673,  -0.685),
            vec2(-0.158,   0.892),
            vec2( 0.952,   0.548),
            vec2(-0.756,  -0.518),
            vec2( 0.347,  -0.412),
            vec2(-0.089,  -0.290),
            vec2( 0.612,   0.710),
            vec2(-0.544,   0.815),
            vec2( 0.818,  -0.543),
            vec2(-0.987,  -0.321),
            vec2( 0.145,   0.623),
            vec2(-0.412,   0.178),
            vec2( 0.567,  -0.098),
            vec2(-0.278,  -0.654),
            vec2( 0.934,   0.389),
            vec2(-0.703,   0.112),
            vec2( 0.056,  -0.512),
            vec2( 0.289,   0.934),
            vec2(-0.867,   0.745)
        );
        int   num_taps = clamp(occTaps, 1, 32);
        float occluded = 0.0;
        for (int i = 0; i < num_taps; i++)
        {
            vec2  tap_uv = sun_uv + taps[i] * uLensFlareOcclusionRadius;
            float d      = texture(depth, tap_uv).r;
            // smoothstep against near-far plane — only sky counts as visible.
            occluded += smoothstep(0.9999, 1.0, d);
        }
        vis *= occluded / float(num_taps);
    }
    if (vis <= 0.0)
        return vec3(0.0);

    // -------------------------------------------------------------------
    // Source energy.
    //
    // Sun (occTaps >= 0): sample sun brightness once, convert to a
    // normalized "overbright" factor. We subtract 2.0 from luminance so
    // only HDR-bright suns drive the flare — prevents diffuse bright
    // surfaces from flaring. (Ported verbatim from the single-source
    // implementation.)
    //
    // Live rig sources (occTaps == -1): energy is read from uCineFlareShaft, the
    // isolated projector-volumetric render target passed as `diffuse` by the
    // rig call below. Daylight, the sun, and lit scene surfaces are absent.
    // The actual rendered shaft brightness at the source is gated by the
    // uCineFlareThreshold. A bright light flares hard; a dim / dimmed-down
    // / below-threshold light gets little or no flare (no star, no glow).
    // The emission point can legitimately sit behind the subject: requiring
    // the exact center pixel made the flare blink whenever the avatar crossed
    // it. Sample a pixel-circular 8-point ring around the center and keep the
    // maximum real HDR luma, so visible shaft energy around the silhouette
    // sustains the flare without replacing the threshold with rig assumptions.
    // Above the soft knee the energy keeps growing with overbright (capped
    // at 4x) so hotter sources read proportionally stronger. The gel color
    // is still applied exactly once by the final `srcColor` multiply, and
    // the CPU visibility and user per-source scale still ride in `vis`.
    // -------------------------------------------------------------------
    vec3  sun_color;
    float sun_lum;
    float sun_bright;
    if (occTaps >= 0)
    {
        sun_color  = texture(diffuse, clamp(sun_uv, vec2(0.0), vec2(1.0))).rgb;
        sun_lum    = dot(sun_color, LUMA);
        sun_bright = max(sun_lum - 2.0, 0.0) / max(sun_lum, 1e-4);
        sun_color *= sun_bright;

        if (sun_bright <= 0.0)
            return vec3(0.0);
    }
    else if (occTaps == -1)
    {
        vec2 suv = clamp(sun_uv, vec2(0.0), vec2(1.0));
        // Radius is authored as a fraction of screen HEIGHT, then converted
        // back through uResolution per axis. The resulting ring is circular
        // in pixels instead of stretching on ultrawide frames. Preserve a
        // 3-pixel minimum for sub-pixel emission points when the setting is 0.
        float probe_px = max(uCineFlareProbeRadius * uResolution.y, 3.0);
        vec2  ring     = probe_px / uResolution;
        vec2  diag     = ring * 0.70710678;

        float local_lum =          dot(texture(diffuse, suv).rgb, LUMA);
        local_lum = max(local_lum, dot(texture(diffuse, clamp(suv + vec2( ring.x, 0.0), vec2(0.0), vec2(1.0))).rgb, LUMA));
        local_lum = max(local_lum, dot(texture(diffuse, clamp(suv - vec2( ring.x, 0.0), vec2(0.0), vec2(1.0))).rgb, LUMA));
        local_lum = max(local_lum, dot(texture(diffuse, clamp(suv + vec2(0.0,  ring.y), vec2(0.0), vec2(1.0))).rgb, LUMA));
        local_lum = max(local_lum, dot(texture(diffuse, clamp(suv - vec2(0.0,  ring.y), vec2(0.0), vec2(1.0))).rgb, LUMA));
        local_lum = max(local_lum, dot(texture(diffuse, clamp(suv + vec2( diag.x,  diag.y), vec2(0.0), vec2(1.0))).rgb, LUMA));
        local_lum = max(local_lum, dot(texture(diffuse, clamp(suv + vec2(-diag.x,  diag.y), vec2(0.0), vec2(1.0))).rgb, LUMA));
        local_lum = max(local_lum, dot(texture(diffuse, clamp(suv + vec2( diag.x, -diag.y), vec2(0.0), vec2(1.0))).rgb, LUMA));
        local_lum = max(local_lum, dot(texture(diffuse, clamp(suv - vec2( diag.x,  diag.y), vec2(0.0), vec2(1.0))).rgb, LUMA));

        // Soft threshold as a DIRECT luma floor (linear-HDR luma sampled at the
        // shaft emission-face anchor). The previous fixed 2.0 diffuse-white
        // floor was calibrated for the SUN and rejected the far-dimmer projector
        // shafts entirely -- nothing flared even at threshold 0. Because the
        // anchor is now the shaft near-plane (setupSpotLightVolumetric geometry)
        // rather than an arbitrary surface, gating on the luma THERE keys on the
        // actual volumetric source: raise uCineFlareThreshold to require a
        // brighter shaft, lower toward 0 to catch faint ones. A small knee keeps
        // low thresholds responsive.
        float knee = max(uCineFlareThreshold * 0.5, 0.1);
        float gate = smoothstep(uCineFlareThreshold, uCineFlareThreshold + knee, local_lum);
        float rig_energy = gate * min(local_lum / max(uCineFlareThreshold + knee, 1e-3), 4.0);

        if (rig_energy <= 0.0)
            return vec3(0.0);

        sun_color  = vec3(rig_energy);
        sun_lum    = rig_energy;
        sun_bright = 1.0;
    }
    else
    {
        // Releasing rig source (occTaps <= -2). Its real shaft has ended, so
        // sampling the current shaft target would return black and hard-cut the
        // flare. CPU packs the last modeled source energy into the signed source
        // scale and exponentially decays `vis`; use a neutral unit response here
        // so the existing tint and element math produce a smooth optical tail.
        // This branch never samples the fallback texture, so daylight/sun pixels
        // cannot resurrect a projector flare during release.
        sun_color  = vec3(1.0);
        sun_lum    = 1.0;
        sun_bright = 1.0;
    }

    // ---- Source-color blend (new, gated) ----------------------------------
    // Pull the element energy tint toward the source's own color while
    // preserving the current energy luminance. For the sun this trades the
    // screen-sampled tint for the artist tint; for rig sources (whose
    // energy base is the neutral screen-luma factor) it pre-saturates the
    // elements toward the gel color on top of the final srcColor multiply.
    // Skipped entirely at the default 0.
    if (uLensFlareSrcColorAmount > 0.0)
    {
        float src_l = max(dot(srcColor, LUMA), 1e-4);
        sun_color = mix(sun_color, srcColor * (sun_lum * sun_bright / src_l),
                        clamp(uLensFlareSrcColorAmount, 0.0, 1.0));
    }

    // ---- Rig source-plane depth layer --------------------------------------
    // The hot core and diffraction star are attached to the projector's
    // emission plane, so foreground scene geometry must occlude them. Glass
    // artifacts (streak, ghosts, halo, iris, rings, circles, arc) live on the
    // camera lens and intentionally remain composited over the foreground.
    //
    // srcDepth is the emission point's standard GL device depth (NDC z mapped
    // to 0..1), matching the bound deferred depth texture. A positive gap means
    // this output pixel contains geometry closer than the source. Derivative-
    // scaled softness antialiases silhouettes while the small bias prevents
    // self-occlusion from projection/depth quantization. Sun math never enters
    // this branch, preserving its existing source treatment.
    float source_layer_mask = 1.0;
    if (occTaps < 0 && (uLensFlareGlow > 0.0 || uLensFlareStarburst > 0.0))
    {
        float scene_depth = texture(depth, clamp(uv, vec2(0.0), vec2(1.0))).r;
        float depth_gap = srcDepth - scene_depth;
        float softness = max(fwidth(scene_depth) * 0.25, 2.0e-5);
        source_layer_mask = 1.0 - smoothstep(5.0e-5, 5.0e-5 + softness, depth_gap);
    }

    float aspect = uResolution.x / max(uResolution.y, 1.0);
    vec2  delta  = uv - sun_uv;
    vec3  flare  = vec3(0.0);

    // ---- Lens warp (new, gated) -------------------------------------------
    // Barrel/pincushion-warped UV used by the "glass" layers only (ghosts,
    // halo, iris, rings, circles, arc). Streak and starburst stay straight.
    // At the default 0 this is a plain copy, so the legacy ghost/halo math
    // sees bit-identical coordinates.
    vec2 wuv = uv;
    if (uLensFlareWarp != 0.0)
    {
        vec2 wc = uv - 0.5;
        wc.x *= aspect;
        float wr2 = dot(wc, wc);
        wc *= 1.0 + uLensFlareWarp * wr2;
        wc.x /= aspect;
        wuv = wc + 0.5;
    }

    // ---- Central glow: soft radial falloff --------------------------------
    // Scaled by src_hdr_scale (same as starburst): sky_hdr_scale for the sun
    // so the glow reads at the same relative brightness as the atmosphere
    // it's painted against, 1.0 for rig sources (sky-independent).
    if (uLensFlareGlow > 0.0)
    {
        vec2  gd      = vec2(delta.x * aspect, delta.y);
        float radius2 = max(uLensFlareGlowRadius * uLensFlareGlowRadius, 1e-8);
        // r² directly from dot() — avoids the length() sqrt.
        float glow    = exp(-dot(gd, gd) * uLensFlareGlowFalloff / radius2);
        if (occTaps < 0)
            flare += sun_color * glow * uLensFlareGlow * src_hdr_scale * source_layer_mask;
        else
            flare += sun_color * glow * uLensFlareGlow * src_hdr_scale;
    }

    // ---- Anamorphic streak: horizontal band with tight vertical gaussian --
    // Early-out when either the streak is disabled or the fragment is far
    // enough off-axis that the vertical gaussian is numerically zero. The
    // bound uses the widened fringe sigma when chromatic spread is active.
    float streak_w = max(uLensFlareStreakWidth,  1e-5);
    float streak_l = max(uLensFlareStreakLength, 1e-5);
    float fringe_w = streak_w + max(uLensFlareChromaticSpread, 0.0) * 0.5;
    if (uLensFlareStreakIntensity > 0.0 && abs(delta.y) < fringe_w * 8.0)
    {
        float horiz = abs(delta.x) * aspect;
        float vert  = abs(delta.y);

        float vert_falloff  = exp(-vert * vert / (streak_w * streak_w));
        float horiz_falloff = exp(-horiz * uLensFlareStreakFalloff / streak_l);
        float streak        = vert_falloff * horiz_falloff;

        // Chromatic fringing — offset R and B vertically from the base streak.
        // The fringe channels use a widened gaussian so they stay visible
        // even when the spread exceeds the base streak width. `fringe_w` is
        // already computed above for the early-out bound.
        if (uLensFlareChromaticSpread > 0.0)
        {
            float spread   = uLensFlareChromaticSpread;
            float vert_r   = abs(delta.y + spread);
            float vert_b   = abs(delta.y - spread);
            float streak_r = exp(-vert_r * vert_r / (fringe_w * fringe_w)) * horiz_falloff;
            float streak_b = exp(-vert_b * vert_b / (fringe_w * fringe_w)) * horiz_falloff;

            flare.r += sun_color.r * streak_r * uLensFlareStreakTint.r * uLensFlareStreakIntensity;
            flare.g += sun_color.g * streak   * uLensFlareStreakTint.g * uLensFlareStreakIntensity;
            flare.b += sun_color.b * streak_b * uLensFlareStreakTint.b * uLensFlareStreakIntensity;
        }
        else
        {
            flare += sun_color * streak * uLensFlareStreakTint * uLensFlareStreakIntensity;
        }

        // ---- Streak two-tone tip (new, gated) -----------------------------
        // Linear blend of the streak tint toward a tip color with distance
        // from the core: adding streak·t·amt·(tipTint − tint) on top of the
        // base term is exactly streak·mix(tint, tipTint, t·amt). Uses the
        // base (green-channel) streak profile for all three channels.
        if (uLensFlareStreakTipAmount > 0.0)
        {
            float tip_t = 1.0 - horiz_falloff; // 0 at the core -> 1 at the tip
            flare += sun_color * streak * tip_t
                   * (uLensFlareStreakTipTint - uLensFlareStreakTint)
                   * uLensFlareStreakIntensity * uLensFlareStreakTipAmount;
        }
    }

    // ---- Ghosts: soft disks stepped along the sun→center axis -------------
    if (uLensFlareGhost > 0.0 && uLensFlareGhostCount > 0)
    {
        vec2 ghost_vec = vec2(0.5) - sun_uv;
        for (int i = 0; i < uLensFlareGhostCount; i++)
        {
            float t        = uLensFlareGhostSpacing * float(i + 1);
            vec2  ghost_uv = sun_uv + ghost_vec * t;
            float scale    = 1.0 / float(i + 1);        // later ghosts fade out
            float radius   = 0.04 * scale + 0.02;

            vec2 gd = wuv - ghost_uv;
            gd.x *= aspect;
            float d    = length(gd);
            float disk = 1.0 - smoothstep(radius * 0.5, radius, d);

            // ---- Ghost chromatic fringing (new, gated) --------------------
            // R and B discs shifted along the ghost axis; G keeps the base
            // disc, so the fringe reads as dispersion, not a triple image.
            if (uLensFlareGhostChroma > 0.0)
            {
                vec2 co = ghost_vec * uLensFlareGhostChroma;
                vec2 gr = wuv - ghost_uv + co;
                vec2 gb = wuv - ghost_uv - co;
                gr.x *= aspect;
                gb.x *= aspect;
                float disk_r = 1.0 - smoothstep(radius * 0.5, radius, length(gr));
                float disk_b = 1.0 - smoothstep(radius * 0.5, radius, length(gb));
                flare += sun_color * vec3(disk_r, disk, disk_b) * scale * uLensFlareGhost;
            }
            else
            {
                flare += sun_color * disk * scale * uLensFlareGhost;
            }
        }
    }

    // ---- Halo: thin ring on the opposite side of the screen from the sun -
    if (uLensFlareHalo > 0.0 && uLensFlareHaloRadius > 0.0)
    {
        vec2  halo_center = vec2(0.5) + (vec2(0.5) - sun_uv);
        float halo_dist   = length(wuv - halo_center);
        float halo_w      = max(uLensFlareHaloWidth, 0.01);
        float halo        = 1.0 - abs(halo_dist - uLensFlareHaloRadius) / halo_w;
        halo  = clamp(halo, 0.0, 1.0);
        halo *= halo;                                    // soften the edges

        // ---- Halo chromatic fringing (new, gated) -------------------------
        // R ring pulled slightly inward, B pushed outward — classic lateral
        // dispersion. G keeps the base ring.
        if (uLensFlareHaloChroma > 0.0)
        {
            float halo_r = clamp(1.0 - abs(halo_dist - (uLensFlareHaloRadius - uLensFlareHaloChroma)) / halo_w, 0.0, 1.0);
            float halo_b = clamp(1.0 - abs(halo_dist - (uLensFlareHaloRadius + uLensFlareHaloChroma)) / halo_w, 0.0, 1.0);
            halo_r *= halo_r;
            halo_b *= halo_b;
            flare += sun_color * vec3(halo_r, halo, halo_b) * 0.3 * uLensFlareHalo;
        }
        else
        {
            flare += sun_color * halo * 0.3 * uLensFlareHalo;
        }
    }

    // ---- Starburst: angular spikes radiating from the sun -----------------
    // Three stacked cos^N harmonics give a richer star pattern than one.
    //
    // The starburst pattern factors as `envelope(sun_dist) * angular(θ)`.
    // Computing the envelope first lets us skip the three pow/cos terms
    // and the atan when envelope is vanishingly small — which is the case
    // for most fragments (spikes are localized around the sun).
    if (uLensFlareStarburst > 0.0)
    {
        vec2 sd = delta;
        sd.x *= aspect;
        float sun_dist = length(sd);

        // Radial decay × core-fade. The core_fade is what prevents the
        // spikes from stacking on top of the glow's bright center — they
        // emanate from its edge instead.
        float radial    = exp(-sun_dist * max(uLensFlareStarburstFalloff, 0.01));
        float core_fade = smoothstep(0.0, max(uLensFlareGlowRadius, 1e-4), sun_dist);
        float envelope  = radial * core_fade;

        if (envelope > 1e-5)
        {
            float angle   = atan(sd.y, sd.x);
            float primary = float(max(uLensFlareStarburstSpikes, 1));
            float sharp   = max(uLensFlareStarburstSharpness, 1.0);

            float pattern = pow(abs(cos(angle * primary)),             sharp       ) * 0.5
                          + pow(abs(cos(angle * primary * 2.0 + 0.5)), sharp * 1.33) * 0.3
                          + pow(abs(cos(angle * primary * 4.0 + 1.0)), sharp * 1.66) * 0.2;

            if (occTaps < 0)
                flare += sun_color * pattern * envelope
                       * uLensFlareStarburst * src_hdr_scale * source_layer_mask;
            else
                flare += sun_color * pattern * envelope
                       * uLensFlareStarburst * src_hdr_scale;
        }
    }

    // ---- N-gon iris ghosts (new, gated) -----------------------------------
    // Polygonal aperture reflections marching along the source→center axis,
    // body + bright rim, per-element hash for spacing/size/rotation jitter.
    if (uLensFlareIris > 0.0 && uLensFlareIrisCount > 0)
    {
        vec2  iris_axis = vec2(0.5) - sun_uv;
        float sides     = float(clamp(uLensFlareIrisSides, 3, 12));
        int   iris_n    = clamp(uLensFlareIrisCount, 1, 8);
        for (int i = 0; i < iris_n; i++)
        {
            float fi = float(i);
            float h  = hash12(vec2(fi * 7.13, 4.7));
            // March from just past the source through center to the far side,
            // with a hash jitter so the chain doesn't read as a metronome.
            float t     = (fi + 0.5) / float(iris_n) * 1.6 + (h - 0.5) * 0.25;
            vec2  cpos  = sun_uv + iris_axis * t;
            vec2  pd    = wuv - cpos;
            pd.x *= aspect;
            float size = uLensFlareIrisSize * (0.7 + 0.6 * hash12(vec2(fi * 3.7, 9.1)));
            float nd   = flareNgonDist(pd, sides, h * 6.2831853);
            float body = 1.0 - smoothstep(size * 0.72, size, nd);
            float rim  = 1.0 - smoothstep(0.0, size * 0.28, abs(nd - size * 0.86));
            float fall = 1.0 / (1.0 + fi);              // later reflections fade
            flare += sun_color * (body * 0.35 + rim * 0.65) * fall * uLensFlareIris;
        }
    }

    // ---- Chromatic dispersion rings (new, gated) --------------------------
    // Concentric rainbow rings around the source; the cosine spectrum sweeps
    // across each ring's thickness, scaled by the dispersion knob.
    if (uLensFlareRing > 0.0 && uLensFlareRingCount > 0)
    {
        vec2 rd = wuv - sun_uv;
        rd.x *= aspect;
        float rl     = length(rd);
        float ring_w = max(uLensFlareRingWidth, 1e-3);
        int   ring_n = clamp(uLensFlareRingCount, 1, 4);
        for (int i = 0; i < ring_n; i++)
        {
            float fi   = float(i);
            float rr   = uLensFlareRingRadius * (1.0 + 0.55 * fi);
            float band = 1.0 - abs(rl - rr) / ring_w;
            if (band <= 0.0)
                continue;
            band  = clamp(band, 0.0, 1.0);
            band *= band;                                // soften the edges
            // 0..1 across the ring thickness drives the hue sweep.
            float t = clamp((rl - (rr - ring_w)) / (2.0 * ring_w), 0.0, 1.0);
            vec3  spec = flareSpectrum(t * uLensFlareRingDispersion + 0.13 * fi);
            flare += sun_color * spec * band * uLensFlareRing / (1.0 + fi);
        }
    }

    // ---- Spectral lens circles (new, gated) -------------------------------
    // Scaled, spectrum-tinted copies of the source ringing outward through
    // the optical axis past frame center — soft disc + brighter edge.
    if (uLensFlareCircle > 0.0 && uLensFlareCircleCount > 0)
    {
        vec2 circ_axis = vec2(0.5) - sun_uv;
        int  circ_n    = clamp(uLensFlareCircleCount, 1, 8);
        for (int i = 0; i < circ_n; i++)
        {
            float fi   = float(i);
            vec2  cpos = sun_uv + circ_axis * (1.2 + uLensFlareCircleSpacing * (fi + 1.0) * 2.0);
            vec2  cd   = wuv - cpos;
            cd.x *= aspect;
            float rad   = uLensFlareCircleScale * (0.8 + 0.35 * hash12(vec2(fi * 11.31, 2.9)));
            float dcirc = length(cd);
            float disc  = 1.0 - smoothstep(rad * 0.55, rad, dcirc);
            float edge  = 1.0 - smoothstep(0.0, rad * 0.22, abs(dcirc - rad * 0.85));
            vec3  spec  = flareSpectrum(fi / float(circ_n) + 0.15);
            flare += sun_color * spec * (disc * 0.3 + edge * 0.7) / (1.5 + fi) * uLensFlareCircle;
        }
    }

    // ---- Spectral arc (new, gated) ----------------------------------------
    // Partial rainbow arc on the far side of frame center: a ring segment at
    // ~2.2x the source→center distance, windowed to the away-facing angle,
    // with the spectrum swept across its thickness.
    if (uLensFlareArc > 0.0)
    {
        vec2 arc_axis = vec2((0.5 - sun_uv.x) * aspect, 0.5 - sun_uv.y);
        float alen = length(arc_axis);
        if (alen > 1e-3)
        {
            arc_axis /= alen;
            vec2 ad = wuv - sun_uv;
            ad.x *= aspect;
            float r     = length(ad);
            float arc_r = alen * 2.2;
            float arc_w = 0.10;
            float band  = 1.0 - abs(r - arc_r) / arc_w;
            if (band > 0.0)
            {
                band  = clamp(band, 0.0, 1.0);
                band *= band;
                float facing = dot(ad / max(r, 1e-4), arc_axis);
                float window = smoothstep(0.55, 0.95, facing);
                float t = clamp((r - (arc_r - arc_w)) / (2.0 * arc_w), 0.0, 1.0);
                flare += sun_color * flareSpectrum(t * 0.7 + 0.05) * band * window * uLensFlareArc;
            }
        }
    }

    // Final scale: 0.15 tames peak intensity to a plausible lens-response
    // range; tint and visibility factor apply equally to all sub-effects.
    return max(flare * vis * srcColor * 0.15, vec3(0.0));
}

vec3 computeLensFlare(sampler2D diffuse, sampler2D depth, vec2 uv)
{
    // Master gate: cheapest possible early-out. The extra integer compare on
    // uCineFlareCount keeps rig flares alive when the sun is off/occluded;
    // with the default count of 0 this reduces to the original sun-only gate.
    float vis = uLensFlareSunVisibility * uLensFlareStrength;
    if (vis <= 0.0 && uCineFlareCount <= 0)
        return vec3(0.0);

    vec3 total = vec3(0.0);

    // Source 0: the sun — math untouched, including its full-tap depth
    // occlusion (occTaps passes the uniform through; the max() only guards
    // against a negative debug value colliding with the skip sentinel).
    if (vis > 0.0)
    {
        total = flareForSource(diffuse, depth, uv, uLensFlareSunPos, 1.0, vis,
                               uLensFlareLightColor,
                               max(uLensFlareOcclusionTaps, 0));
    }

    // Rig projector sources — additive, each gated by its CPU-provided
    // visibility × per-source scale before any per-source work. Sentinel -1 is
    // a live isolated-shaft sample; -2 is a cached release tail which cannot
    // sample the scene. Both use the per-pixel source-depth mask for glow/star,
    // while edge fade and the temporal envelope are computed CPU-side.
    if (uCineFlareCount > 0)
    {
        int rig_count = min(uCineFlareCount, AL_CINE_FLARE_MAX);
        for (int i = 0; i < rig_count; i++)
        {
            float packed_scale = uCineFlareColor[i].w;
            bool  releasing    = packed_scale < 0.0;
            float rvis         = uCineFlareA[i].z * abs(packed_scale);
            if (rvis <= 0.0)
                continue;
            total += flareForSource(uCineFlareShaft, depth, uv,
                                    uCineFlareA[i].xy, uCineFlareA[i].w, rvis,
                                    uCineFlareColor[i].rgb, releasing ? -2 : -1);
        }
    }

    // Master one-knob scale — branch not taken at the default 1.0, so the
    // legacy output stays bit-identical.
    if (uLensFlareMaster != 1.0)
        total *= max(uLensFlareMaster, 0.0);

    return total;
}


// =============================================================================
// Vignette — frame darkening with aspect correction and optional 3-color ramp
// =============================================================================
//
// Distances are measured in units of half-image-height on the shorter axis,
// so uVignetteRadius = 1.0 reaches the top/bottom edges on any aspect.
// Values above 1.0 extend the falloff past the corners — useful on
// widescreen where only extreme edges should be touched.
//
// The shape parameter morphs between a circular falloff (distance = length)
// and a rounded-square falloff (distance = max(|x|,|y|)^1.5), so the same
// effect covers classic optical vignettes and stylized CRT/screen looks.
//
// Optional three-color ramp (image → mid → edge) activates only when
// uVignetteMidPoint lands strictly in (0, 1). Otherwise, the two-color
// path is used, which is the common case.

uniform float uVignetteAmount;                   // [0, 1]   strength
uniform float uVignetteRadius;                   // [0.25, 1.5] edge distance in half-height units
uniform float uVignetteSoft;                     // [0.05, 1]   width of the falloff band
uniform float uVignetteShape;                    // [0, 1]   0 = circular, 1 = rounded square
uniform vec3  uVignetteColor;                    // outer (corner) color
uniform vec3  uVignetteMidColor;                 // intermediate ramp color
uniform float uVignetteMidPoint;                 // [0, 1]   0/1 = two-color fallback; else three-color
uniform vec2  uVignetteCenter;                   // [-0.5, 0.5] offset from frame center
uniform vec2  uVignetteAspect;                   // image (w, h); (1,1) disables aspect correction
uniform float uVignetteFeather;                  // [0.2, 4.0] shape of the darkening curve
                                                //   <1 = spreads darkening inward (gentle haze)
                                                //    1 = linear smoothstep falloff
                                                //   >1 = concentrates darkening at the edges

vec3 applyVignette(vec3 color, vec2 uv)
{
    if (uVignetteAmount <= 0.0)
        return color;

    // Center offset and branchless aspect correction: one component of
    // `scale` is always 1.0, the other is the larger-axis ratio, so the
    // "short-axis half-height" normalization below holds for any viewport.
    vec2  d      = uv - 0.5 - uVignetteCenter;
    float aspect = max(uVignetteAspect.x, 1e-4) / max(uVignetteAspect.y, 1e-4);
    d *= max(vec2(aspect, 1.0 / aspect), 1.0);

    // Two distance metrics, blended by `uVignetteShape`:
    //   - circular  = Euclidean length (classic optical vignette)
    //   - square    = Chebyshev length raised to 1.5 (rounded-square CRT look)
    // `sq_base` is non-negative by construction, so `x * sqrt(x)` is a valid
    // faster form of pow(x, 1.5) without a domain guard.
    float circular = length(d) * 2.0;
    float sq_base  = max(abs(d.x), abs(d.y)) * 2.0;
    float square   = sq_base * sqrt(sq_base);
    float dist     = mix(circular, square, uVignetteShape);

    // `start` is where fading begins; `end` is where the edge color is reached.
    float start = uVignetteRadius - uVignetteSoft;
    float end   = uVignetteRadius;

    // Three-color ramp only if midpoint is strictly inside (0, 1).
    if (uVignetteMidPoint > 1e-4 && uVignetteMidPoint < 0.9999)
    {
        float mid      = mix(start, end, uVignetteMidPoint);
        float tMidRaw  = smoothstep(start, mid, dist);
        float tEdgeRaw = smoothstep(mid,   end, dist);
        // pow reshapes the mask before scaling by overall amount.
        float tMid  = pow(tMidRaw,  uVignetteFeather) * uVignetteAmount;
        float tEdge = pow(tEdgeRaw, uVignetteFeather) * uVignetteAmount;
        color = mix(color, uVignetteMidColor, tMid);
        color = mix(color, uVignetteColor,    tEdge);
    }
    else
    {
        float tRaw = smoothstep(start, end, dist);
        float t    = pow(tRaw, uVignetteFeather) * uVignetteAmount;
        color = mix(color, uVignetteColor, t);
    }
    return color;
}


// =============================================================================
// Film grain — additive hash-based noise with luma weighting
// =============================================================================
//
// Four styles:
//   0 mono luma    — classic film, same noise in R/G/B, midtone-weighted
//   1 color        — digital push, independent noise per channel
//   2 coarse       — 16mm feel, larger "grains" via pixel quantization
//   3 photon shot  — CCD-style, amplitude scales with sqrt(luminance)
//
// Size is multiplied by the display's resolution-relative pixel scale so
// grain particles stay perceptually constant across 1080p / 4K / HiDPI
// outputs. Amount is squared in-shader for a gentler slider toe; keeping
// it in-shader (vs. pre-baking on the CPU like the CA uniforms) lets a
// future UI slider animate without rebinding on every change.

uniform float uGrainAmount;                      // [0, 1]   strength (squared internally)
uniform int   uGrainStyle;                       // 0 mono, 1 color, 2 coarse, 3 photon shot
uniform float uGrainSize;                        // [1, 8]   grain cell size in 1080p-equivalent pixels
uniform float uGrainRange;                       // [0, 1]   luma position where grain peaks
                                                //          0 = shadows, 0.5 = midtones, 1 = highlights
uniform vec3  uGrainTint;                        // neutral gray; try (1, 0.9, 0.8) warm, (0.8, 0.9, 1) cool
uniform bool  uGrainAnimate;                     // false = frozen pattern (static frame for stills)

// Noise sample in ~[-0.5, 0.5]. Style controls color vs luma and whether
// cells are quantized for a coarser look. The caller scales by amplitude.
vec3 grainSample(vec2 fragCoord, int style, float size)
{
    float cell = max(size, 1.0);
    vec2  gp   = floor(fragCoord / cell) * cell;
    if (style == 1)
    {
        // Independent per-channel noise for a "digital" look.
        return vec3(hash12(gp),
                    hash12(gp + 17.0),
                    hash12(gp + 31.0)) - 0.5;
    }
    return vec3(hash12(gp) - 0.5);
}

vec3 applyFilmGrain(vec3 color, vec2 fragCoord)
{
    if (uGrainAmount <= 0.0)
        return color;

    // 0.1 is the empirical ceiling where grain stops reading as "film
    // texture" and starts reading as "broken image." Squaring amount
    // biases the slider toward subtle values.
    float amp   = uGrainAmount * uGrainAmount * 0.1;
    float gluma = dot(color, LUMA);

    // Scale grain cells by resolution so a "size 1" grain looks the same
    // perceptual size on 1080p and 4K displays.
    float pixelScale    = max(uResolution.x, uResolution.y) / 1080.0;
    float effectiveSize = uGrainSize * pixelScale;
    vec3  n = grainSample(fragCoord + frameNoiseOffset(uGrainAnimate),
                          uGrainStyle, effectiveSize);

    if (uGrainStyle == 3)
    {
        // Photon shot noise: amplitude proportional to sqrt(luminance),
        // matching how CCD sensor noise scales with photon count.
        color += n * uGrainTint * sqrt(max(gluma, 0.0)) * amp;
    }
    else
    {
        // Midtone-biased bell: weight peaks at uGrainRange and falls off
        // toward shadows/highlights. Matches how real film grain is most
        // visible in midtones.
        float d      = gluma - uGrainRange;
        float weight = max(1.0 - (d * d) * 4.0, 0.0);
        color += n * uGrainTint * weight * amp;
    }
    return color;
}


// =============================================================================
// Color Vision Deficiency — simulation, daltonization, accessibility preview
// =============================================================================
//
// Two separate use cases share these helpers:
//   1. Compensation (daltonization) — run in the output pipeline to make the
//      image more distinguishable to CVD viewers. Controlled by uCompensate*.
//   2. Preview/debug — optionally re-simulate the output as a CVD viewer
//      would perceive it, or as a false-color exposure map. Controlled by
//      uPreviewMode. Must be 0 for normal shipping output.
//
// CVD matrices are Machado et al. 2009 (severity = 1.0) — physiologically
// grounded and slightly more accurate than Brettel/Viénot, especially for
// tritanopia. Monochromacy/achromatomaly modes are implemented as
// desaturation toward Rec.709 luma.

// ---- Compensation (daltonization, output path) -----------------------------
uniform int   uCompensateMode;           // [0, 3] — 0 off, 1 protan, 2 deutan, 3 tritan
uniform float uCompensateAmount;         // [0, 1]   strength of the channel redistribution

// ---- Preview / debug --------------------------------------------------------
uniform int   uPreviewMode;              // MUST be 0 for final output.
                                        // 1 = protanopia (no L cones)
                                        // 2 = deuteranopia (no M cones)
                                        // 3 = tritanopia (no S cones)
                                        // 4 = achromatopsia (rod monochromacy) — luminance only
                                        // 5 = blue cone monochromacy — blue-tinted near-gray
                                        // 6 = achromatomaly — strong desaturation
                                        // 7 = false-color exposure map

// Forward-simulate how a CVD viewer perceives `c`. Used both internally
// (daltonize computes the difference between original and simulated) and
// as the preview overlay.
vec3 simulateCVD(vec3 c, int mode)
{
    if (mode == 4)
    {
        // Achromatopsia (rod monochromacy). Real rod response peaks at
        // ~507nm and biases neutral gray slightly cyan-green; standard
        // practice uses Rec.709 luma as a reasonable proxy.
        return vec3(dot(c, LUMA));
    }
    if (mode == 5)
    {
        // Blue cone monochromacy — only S-cones work, so the image
        // collapses to a blue-yellow axis. Approximated by extracting the
        // blue-yellow opponent signal and recombining as a desaturated
        // blue tint. Not physiologically exact but matches common
        // simulator output and reads correctly to normal viewers.
        float l  = dot(c, LUMA);
        float by = c.b - 0.5 * (c.r + c.g);
        return vec3(l - by * 0.15, l - by * 0.05, l + by * 0.25);
    }
    if (mode == 6)
    {
        // Achromatomaly — partial color loss. 80% blend toward gray
        // retains a hint of residual color.
        return mix(c, vec3(dot(c, LUMA)), 0.8);
    }

    mat3 m;
    if (mode == 1)
    {
        // Protanopia (Machado 2009, severity 1.0)
        m = mat3(0.152286, 1.052583, -0.204868,
                 0.114503, 0.786281,  0.099216,
                -0.003882,-0.048116,  1.051998);
    }
    else if (mode == 2)
    {
        // Deuteranopia (Machado 2009, severity 1.0)
        m = mat3(0.367322, 0.860646, -0.227968,
                 0.280085, 0.672501,  0.047413,
                -0.011820, 0.042940,  0.968881);
    }
    else
    {
        // Tritanopia (Machado 2009, severity 1.0)
        m = mat3(1.255528,-0.078441, -0.004733,
                -0.076749, 0.930809,  0.691367,
                 0.178779,-0.147632,  0.303900);
    }
    return m * c;
}

// Daltonization: compute the error between original and CVD-simulated
// image, then redistribute that error into channels the viewer can still
// see. Monochromacies (modes ≥ 4) have no perceivable axis to shift into,
// so the function is a no-op for them.
vec3 daltonize(vec3 c, int mode, float amount)
{
    if (mode >= 4) return c;

    vec3 sim = simulateCVD(c, mode);
    vec3 err = c - sim;
    vec3 shift;
    if (mode == 3)
    {
        // Tritan — shift blue error into the red/green axis.
        float be = err.b;
        shift = vec3( be*0.7 + err.g*0.3, -be*0.7 - err.g*0.3, 0.0);
    }
    else
    {
        // Protan/deutan — shift red/green error into green and blue.
        shift = vec3(0.0, err.r*0.7 + err.g, err.r*0.7);
    }
    return c + shift * amount;
}

// False-color exposure map (ARRI-style palette). 8 bands from under-exposed
// (deep blue) to clipped (white), keyed on Rec.709 luminance.
vec3 falseColor(vec3 c)
{
    float l = dot(c, LUMA);
    if (l < 0.02) return vec3(0.0, 0.0, 0.4);
    if (l < 0.10) return vec3(0.0, 0.3, 0.8);
    if (l < 0.30) return vec3(0.0, 0.7, 0.5);
    if (l < 0.50) return vec3(0.3, 0.7, 0.0);
    if (l < 0.70) return vec3(0.8, 0.8, 0.0);
    if (l < 0.90) return vec3(1.0, 0.5, 0.0);
    if (l < 0.98) return vec3(1.0, 0.1, 0.0);
    return vec3(1.0);
}

// Accessibility pipeline entry. Zero-cost when disabled.
vec3 applyCVDCompensation(vec3 color)
{
    if (uCompensateMode <= 0 || uCompensateAmount <= 0.0)
        return color;
    return clamp(daltonize(color, uCompensateMode, uCompensateAmount), 0.0, 1.0);
}

// Preview overlay entry. Mode 7 replaces the image entirely with a
// false-color map; modes 1-6 re-simulate the output as a CVD viewer
// would perceive it. Intended for debugging only — must be 0 for final
// shipping output.
vec3 applyPreview(vec3 color)
{
    // Fast path — zero preview mode is the shipping configuration and should
    // cost a single compare.
    if (uPreviewMode == 0)
        return color;
    if (uPreviewMode == 7)
        return falseColor(color);
    return simulateCVD(clamp(color, 0.0, 1.0), uPreviewMode);
}


// =============================================================================
// Dither — triangular PDF noise for banding suppression at the output stage
// =============================================================================
//
// Interleaved Gradient Noise (Jimenez, COD:AW 2014) sampled twice with a
// small offset to produce a TPDF-shaped noise in [-1, 1]. TPDF is
// perceptually superior to rectangular PDF — it fully masks quantization
// banding with no residual noise-floor shift.
//
// Temporal decorrelation is handled by frameNoiseOffset() (see top of file):
// the noise pattern fully reshuffles between frames instead of visibly
// scrolling as uFrameId increments.
//
// Amplitude scales to output bit depth: ±1/255 for 8-bit, ±1/1023 for 10-bit.

uniform float uDitherAmount;             // [0, 1]   TPDF amplitude scale. Leave at 1.0.
uniform int   uDitherBits;              // {8, 10}  output bit depth.
uniform bool  uDitherAnimate;           // false = freeze pattern. Almost always want true —
                                        // animated dither is perceptually invisible,
                                        // static dither can show as fixed-pattern grain.

// Interleaved Gradient Noise core — hashes a 2D input to a well-distributed
// [0, 1) sample. Frame decorrelation is applied at the call site via
// frameNoiseOffset(); the TPDF pair must share the same offset so keeping it
// outside IGN is essential, not just an optimization.
float ign(vec2 p)
{
    return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));
}

vec3 applyDither(vec3 color, vec2 fragCoord)
{
    if (uDitherAmount <= 0.0)
        return color;

    // Compute the frame offset *once* and reuse for both TPDF taps — the
    // IGN calls are cheap but factoring the per-frame hash out keeps the
    // pattern coherent across the pair (otherwise the two taps would
    // decorrelate from each other, not just from previous frames).
    vec2 p = fragCoord + frameNoiseOffset(uDitherAnimate);

    float tpdf = ign(p) + ign(p + vec2(1.618, 2.414)) - 1.0;

    float levels = (uDitherBits >= 10) ? 1023.0 : 255.0;
    return color + tpdf * (uDitherAmount / levels);
}
