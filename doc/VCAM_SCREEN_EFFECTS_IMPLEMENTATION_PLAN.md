# VCam Per-Screen TV Effects — Complete Implementation Architecture & Code Specification

This document expands the conceptual guide in `doc/VCAM_SCREEN_EFFECTS_GUIDE.md` into a complete, production-ready technical specification and code design for per-display TV/CRT screen effects in **Alchemy-Machinima** (Virtual Cam / VCam subsystem).

> **Safety Invariants:**
> 1. **Zero Source Modification during drafting:** No `.cpp`, `.h`, `.glsl`, or `.xml` codebase files are touched.
> 2. **No Build Execution:** No build tasks are initiated.
> 3. **Composite-Time Only:** All effects run purely in `prismLensF.glsl` on existing captured feeds. No extra render passes, frame buffers, or dynamic memory allocations.
> 4. **Bit-Identical Fallback:** When all effect sliders are set to 0.0, the shader execution and output are bit-identical to the baseline feed.
> 5. **Strict C++ `/WX` Compliance:** All casts explicit (`static_cast<F32>`), `const LLStringExplicit&` string passing, enum handling complete.

---

## 0. MANDATORY CORRECTIONS (adversarial review 2026-08-05 — these OVERRIDE any conflicting draft in §3–§7)

The sections below are a correct *foundation* (the data model, per-display composite path, and packing all target real API: `DisplaySettings`/`DisplayDefinition.mSettings`/`setDisplaySettings`/`DisplayHandle` verified in `llprismlens.h`). But the following were verified WRONG against the source tree and MUST be applied. Where a later section conflicts with §0, §0 wins.

**C1 — Persistence goes in `llprismlens.cpp` (`PrismLensRegistry::sceneData/applySceneData`), NOT `lldirectorcast.cpp`.**
`lldirectorcast.cpp` exists but is the Director *actor/cast* scene layer (marks, flycam), not prism displays. The prism scene (de)serialization is `PrismLensRegistry::sceneData()` (`llprismlens.cpp:3474`, writes `prism_captures` + `prism_displays`) and `applySceneData()` (`:3543`). Add the `ScreenEffects` fields to the **`prism_displays` item block** — the per-display block (near `:3538` write / the displays loop in `applySceneData`), NOT the per-capture block where the optics live (`chromatic_aberration` etc. are per-capture at `:3504`). Read under a `has(...)` + `is_numeric(...)` guard (mirror the existing display fields), default each field to its struct default when absent (an absent block must load clean, never reject the scene), then `clampAndValidate()`. The LLSD key/value snippets in §7 are fine; only the file/function/block change.

**C2 — EXTEND `prismLensF.glsl`; do NOT wholesale-rewrite it (the §4 rewrite is a regression).**
The committed optics blocks (CA, exposure, CRT scanlines `sin(oriented_uv.y*600.0)*0.5+0.5 → mix(1.0, scanline*0.4+0.6, optics.z)`, film grain) MUST stay **byte-identical when the new effects are 0** — a shipped capture using them must look exactly as today. The §4 draft changes the scanline formula (parametric ~2199 density at the 0.5 default, `*0.45+0.55`) → forbidden. Instead, start from the ACTUAL current `prismLensF.glsl` and only:
  1. Add the 5 uniforms (`screenEffect0..3`, `screenEffectTime`).
  2. INSERT the UV-space distortions (pixelate, vertical roll+hole, VHS tracking) immediately after `oriented_uv` is computed and BEFORE `texture_uv` / the committed CA block, modifying `oriented_uv` in place. With all three at 0, `oriented_uv` is unchanged → committed sampling is identical.
  3. Per-display **chroma bleed**: widen the existing CA guard to `if (prismLensOptics.x > 0.001 || screenEffect2.z > 0.001)` and add `dist.x += screenEffect2.z * 0.012;` — so `screenEffect2.z == 0` leaves the committed CA math byte-identical.
  4. Keep the committed exposure / scanline(optics.z, 600) / grain blocks VERBATIM. Do NOT merge the per-display scanline into the optics scanline — add per-display scanline as its OWN appended block.
  5. APPEND the post-sample effects (grayscale, sepia, per-display scanlines, interlace, static, flicker, dropout, vignette, and the vertical-hole darkening from C4) AFTER the committed grain block, operating on `col`, each gated `>0.001` → no-op at 0.
The §4 GLSL below is a reference for the effect MATH only — re-home each block into the real shader per the above; the result with all sliders 0 must diff-clean against today's shader output.

**C3 — Time source is `fmodf(gFrameTimeSeconds, 3600.f)`, NOT `LLFrameTimer::getElapsedSeconds()`.**
Upload `gPrismLensProgram.uniform1f(sScreenEffectTime, fmodf(gFrameTimeSeconds, 3600.f));` — the exact pattern froxel/projvol use (`pipeline.cpp:13585`, `:14313`). `gFrameTimeSeconds` is the global frame clock; the `fmodf` wrap keeps shader time in a small range so animated effects don't lose float precision over a long session.

**C4 — Implement the real "unstable vertical hole" (the §4 roll only scrolls the image).**
Roll the V in the UV stage AND darken a thin band at the moving seam so it reads as a V-sync bar, e.g.:
```glsl
// UV stage: compute the seam and roll; remember the seam for the darken step
float roll_seam = 0.0;
if (screenEffect1.z > 0.001) {
    float roll_rate = mix(0.1, 1.5, screenEffect1.w);
    roll_seam = fract(screenEffectTime * roll_rate * screenEffect1.z);
    oriented_uv.y = fract(oriented_uv.y + roll_seam);
}
// overlay stage (after sampling): dark band centred on the seam
if (screenEffect1.z > 0.001) {
    float band = smoothstep(0.0, 0.06, abs(fract(oriented_uv.y - roll_seam + 0.5) - 0.5));
    col.rgb *= mix(1.0 - 0.85 * screenEffect1.z, 1.0, band);
}
```

**C5 — Use the REAL manager API (the §6 accessor names are invented).**
The selected display is `selectedDisplay()` → `const LLPrismLens::DisplayDefinition*` (may be null → bail); its handle is the member `mSelectedDisplay`. Read current effects from `selectedDisplay()->mSettings.mEffects`; commit via `LLPrismLens::setDisplaySettings(mSelectedDisplay, settings)` (signature `llprismlens.h:313`). Populate the sliders from the selected display inside the existing display-selection refresh path (not only `postBuild`). `LLSlider::getValue().asReal()` → wrap in `static_cast<F32>` (/WX).

**C6 — Housekeeping + sequencing.** Delete the stray `implementation_plan.md` Gemini left at the repo root; do NOT commit it. Implement this feature ONLY AFTER the mesh-display fix lands — it edits the same files (`llprismlens.cpp/.h`, `llfloaterprismmanager.cpp`, `prismLensF.glsl`) and starting concurrently will collide. HARD invariant, verified in review: **all sliders 0 ⇒ output bit-identical to today; VCam OFF ⇒ frame byte-identical.**

**C7 — ADD a per-display BRIGHTNESS effect (user request 08-05).** Add `F32 mBrightness = 0.f;` to `ScreenEffects` (range **−1..+1**, 0 = unchanged), include it in `clampAndValidate` (`mBrightness = llclamp(mBrightness, -1.f, 1.f);`) and `isZero` (`&& mBrightness == 0.f`). Pack it into the first reserved slot **`mScreenEffect3[2]`** (retitle that field's comment "Reserved"→"Brightness"; getCompositeStates fills it from `mEffects.mBrightness`). Shader (appended post-sample stage): `if (abs(screenEffect3.z) > 0.001) { col.rgb *= (1.0 + screenEffect3.z); }` → 0 is identity. Add a "Brightness" slider (**min_val="-1" max_val="1"**) to the Displays effects panel and a `brightness` LLSD key to the persistence block (default 0 when absent). NOTE it is distinct from the per-camera **exposure** optic (that is EV bias, per-capture, `prismLensOptics.w`); brightness here is a simple per-display linear gain — both may coexist.

---

## 1. Architecture Overview & Data Model

Each VCam display face (`PrismDisplay`) maintains its own independent `ScreenEffects` struct within `DisplaySettings`. As the compositor renders each display face in `pipeline.cpp`, per-display `ScreenEffects` floats are packed into 4 `vec4` uniforms (`screenEffect0` .. `screenEffect3`) alongside a monotonic viewer clock float uniform (`screenEffectTime`).

```
+-----------------------------------------------------------------------------------+
| LLPrismLens Registry                                                              |
|                                                                                   |
|  Display 1 -> DisplaySettings.mEffects (Scanlines=0.8, Static=0.2)                |
|  Display 2 -> DisplaySettings.mEffects (Grayscale=1.0, Vignette=0.5)               |
+-----------------------------------------------------------------------------------+
                                       |
                                       v
                    LLPrismLens::getCompositeStates()
                                       | (Packs 14 floats into 4x vec4)
                                       v
                     CompositeState::mScreenEffect0..3
                                       |
                                       v
                pipeline.cpp::renderDeferredLighting() Loop
                                       | (Uploads uniforms per display face)
                                       v
                   prismLensF.glsl Fragment Shader
                                       |
  [ 1. UV Distortion ] -> [ 2. Feed Fetch ] -> [ 3. Color Grade ] -> [ 4. Overlays ]
```

---

## 2. Data Structure Definitions (`indra/newview/llprismlens.h`)

### 2.1 `ScreenEffects` Struct
Added to `indra/newview/llprismlens.h` above `DisplaySettings`:

```cpp
struct ScreenEffects
{
    F32 mScanlines      = 0.f;  // 0..1 scanline blend strength
    F32 mScanlineCount  = 0.5f; // 0..1 line density (0.5 = 600 lines)
    F32 mPixelate       = 0.f;  // 0..1 block quantization strength
    F32 mGrayscale      = 0.f;  // 0..1 luma desaturation
    F32 mSepia          = 0.f;  // 0..1 sepia tone blend
    F32 mStatic         = 0.f;  // 0..1 analog hash noise strength
    F32 mVerticalRoll   = 0.f;  // 0..1 V-sync roll band strength
    F32 mRollSpeed      = 0.2f; // 0..1 roll speed rate
    F32 mTracking       = 0.f;  // 0..1 VHS horizontal tear/jitter
    F32 mFlicker        = 0.f;  // 0..1 brightness flicker rate
    F32 mChromaBleed    = 0.f;  // 0..1 horizontal color fringe
    F32 mVignette       = 0.f;  // 0..1 CRT edge darkening & curvature
    F32 mInterlace      = 0.f;  // 0..1 field line shimmer
    F32 mDropout        = 0.f;  // 0..1 signal dropout/ghosting

    void clampAndValidate()
    {
        mScanlines     = llclamp(mScanlines, 0.f, 1.f);
        mScanlineCount = llclamp(mScanlineCount, 0.f, 1.f);
        mPixelate      = llclamp(mPixelate, 0.f, 1.f);
        mGrayscale     = llclamp(mGrayscale, 0.f, 1.f);
        mSepia         = llclamp(mSepia, 0.f, 1.f);
        mStatic        = llclamp(mStatic, 0.f, 1.f);
        mVerticalRoll  = llclamp(mVerticalRoll, 0.f, 1.f);
        mRollSpeed     = llclamp(mRollSpeed, 0.f, 1.f);
        mTracking      = llclamp(mTracking, 0.f, 1.f);
        mFlicker       = llclamp(mFlicker, 0.f, 1.f);
        mChromaBleed   = llclamp(mChromaBleed, 0.f, 1.f);
        mVignette      = llclamp(mVignette, 0.f, 1.f);
        mInterlace     = llclamp(mInterlace, 0.f, 1.f);
        mDropout       = llclamp(mDropout, 0.f, 1.f);
    }

    bool isZero() const
    {
        return mScanlines == 0.f && mPixelate == 0.f && mGrayscale == 0.f &&
               mSepia == 0.f && mStatic == 0.f && mVerticalRoll == 0.f &&
               mTracking == 0.f && mFlicker == 0.f && mChromaBleed == 0.f &&
               mVignette == 0.f && mInterlace == 0.f && mDropout == 0.f;
    }
};
```

### 2.2 Updating `DisplaySettings` & `CompositeState`
In `indra/newview/llprismlens.h`:

```cpp
struct DisplaySettings
{
    EFitMode mFitMode = EFitMode::FIT;
    F32 mAnchor[2] = { 0.5f, 0.5f };
    F32 mBarColorLinear[3] = { 0.f, 0.f, 0.f };
    ScreenEffects mEffects; // Per-display TV screen effects
};

struct CompositeState
{
    // ... existing fields ...
    S32 mScissor[4] = { 0, 0, 0, 0 };
    F32 mEdgeFeather = 0.f;

    // Option C: Cinematic optics parameters passed to compositor
    F32 mOpticsParams[4] = { 0.f, 0.f, 0.f, 0.f };

    // Per-display TV/CRT screen effect uniform packs (4x vec4)
    F32 mScreenEffect0[4] = { 0.f, 0.f, 0.f, 0.f }; // Scanlines, ScanlineCount, Pixelate, Grayscale
    F32 mScreenEffect1[4] = { 0.f, 0.f, 0.f, 0.f }; // Sepia, Static, VerticalRoll, RollSpeed
    F32 mScreenEffect2[4] = { 0.f, 0.f, 0.f, 0.f }; // Tracking, Flicker, ChromaBleed, Vignette
    F32 mScreenEffect3[4] = { 0.f, 0.f, 0.f, 0.f }; // Interlace, Dropout, Reserved, Reserved
};
```

---

## 3. C++ Registry & Uniform Uploading Logic

### 3.1 Packing Effects in `LLPrismLens::getCompositeStates()` (`indra/newview/llprismlens.cpp`)

```cpp
// Inside getCompositeStates(...) when populating state for display binding:
state.mScreenEffect0[0] = display.mSettings.mEffects.mScanlines;
state.mScreenEffect0[1] = display.mSettings.mEffects.mScanlineCount;
state.mScreenEffect0[2] = display.mSettings.mEffects.mPixelate;
state.mScreenEffect0[3] = display.mSettings.mEffects.mGrayscale;

state.mScreenEffect1[0] = display.mSettings.mEffects.mSepia;
state.mScreenEffect1[1] = display.mSettings.mEffects.mStatic;
state.mScreenEffect1[2] = display.mSettings.mEffects.mVerticalRoll;
state.mScreenEffect1[3] = display.mSettings.mEffects.mRollSpeed;

state.mScreenEffect2[0] = display.mSettings.mEffects.mTracking;
state.mScreenEffect2[1] = display.mSettings.mEffects.mFlicker;
state.mScreenEffect2[2] = display.mSettings.mEffects.mChromaBleed;
state.mScreenEffect2[3] = display.mSettings.mEffects.mVignette;

state.mScreenEffect3[0] = display.mSettings.mEffects.mInterlace;
state.mScreenEffect3[1] = display.mSettings.mEffects.mDropout;
state.mScreenEffect3[2] = 0.f; // Reserved
state.mScreenEffect3[3] = 0.f; // Reserved
```

### 3.2 Uniform Binding in `Pipeline::renderDeferredLighting()` (`indra/newview/pipeline.cpp`)

```cpp
// Static hashed uniform name definitions:
static const LLStaticHashedString sScreenEffect0("screenEffect0");
static const LLStaticHashedString sScreenEffect1("screenEffect1");
static const LLStaticHashedString sScreenEffect2("screenEffect2");
static const LLStaticHashedString sScreenEffect3("screenEffect3");
static const LLStaticHashedString sScreenEffectTime("screenEffectTime");

// Inside composite iteration loop:
gPrismLensProgram.uniform4fv(sScreenEffect0, 1, prism_state.mScreenEffect0);
gPrismLensProgram.uniform4fv(sScreenEffect1, 1, prism_state.mScreenEffect1);
gPrismLensProgram.uniform4fv(sScreenEffect2, 1, prism_state.mScreenEffect2);
gPrismLensProgram.uniform4fv(sScreenEffect3, 1, prism_state.mScreenEffect3);
gPrismLensProgram.uniform1f(sScreenEffectTime, fmodf(gFrameTimeSeconds, 3600.f)); // per §0.C3 — NOT LLFrameTimer::getElapsedSeconds()
```

---

## 4. Fragment Shader Implementation (`app_settings/shaders/class1/deferred/prismLensF.glsl`)

> ⚠️ **Per §0.C2 this is a MATH REFERENCE ONLY, not a drop-in rewrite.** Start from the ACTUAL current `prismLensF.glsl`, keep its committed optics blocks (CA, exposure, `sin(y*600)` scanline, grain) verbatim, and INSERT/APPEND the new per-display blocks. Use the C4 vertical-hole code and the C7 brightness block. With all sliders 0 the output must diff-clean against today.

```glsl
/**
 * @file prismLensF.glsl
 * @brief Composites VCam auxiliary beauty feed with cinematic optics & per-display screen TV effects.
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

// Option C: Camera-level Optics Parameters
// x: Chromatic Aberration, y: Film Grain, z: CRT Scanlines, w: Exposure Bias EV
uniform vec4 prismLensOptics;

// Per-Display TV Screen Effect Uniform Packs
uniform vec4 screenEffect0; // x: scanlines, y: scanlineCount, z: pixelate, w: grayscale
uniform vec4 screenEffect1; // x: sepia, y: static, z: verticalRoll, w: rollSpeed
uniform vec4 screenEffect2; // x: tracking, y: flicker, z: chromaBleed, w: vignette
uniform vec4 screenEffect3; // x: interlace, y: dropout, z: reserved, w: reserved
uniform float screenEffectTime;

in vec2 prism_uv;
out vec4 frag_color;

// Helper hash pseudo-random noise generator
float tv_hash(vec2 p)
{
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453123);
}

void main()
{
    vec2 logical_uv = prism_uv * displayToCaptureScale + displayToCaptureOffset;

    if (edgeFeather < 0.0)
    {
        discard;
    }

    if (letterbox != 0 &&
        (any(lessThan(logical_uv, vec2(0.0))) ||
         any(greaterThan(logical_uv, vec2(1.0)))))
    {
        frag_color = vec4(barColorLinear, 0.0);
    }
    else
    {
        vec2 oriented_uv = logical_uv * retainedOrientationScale + retainedOrientationOffset;

        // ---------------------------------------------------------------------
        // 1. SAMPLING SPACE DISTORTIONS (Modify UV before feed fetch)
        // ---------------------------------------------------------------------
        
        // Pixelation Block Quantization
        if (screenEffect0.z > 0.001)
        {
            float blocks = mix(512.0, 16.0, screenEffect0.z);
            oriented_uv = floor(oriented_uv * blocks) / blocks;
        }

        // Vertical V-Sync Roll & Unstable Hole
        if (screenEffect1.z > 0.001)
        {
            float roll_rate = mix(0.1, 1.5, screenEffect1.w);
            float v_shift = fract(oriented_uv.y + screenEffectTime * roll_rate * screenEffect1.z);
            oriented_uv.y = v_shift;
        }

        // VHS Horizontal Tracking Jitter / Tear
        if (screenEffect2.x > 0.001)
        {
            float row = floor(oriented_uv.y * 180.0);
            float tear_noise = tv_hash(vec2(row, floor(screenEffectTime * 18.0)));
            float tear_gate = step(0.92 - screenEffect2.x * 0.25, tear_noise);
            float jitter = (tear_noise - 0.5) * 0.06 * screenEffect2.x * tear_gate;
            oriented_uv.x = clamp(oriented_uv.x + jitter, 0.0, 1.0);
        }

        // Compute final texture sampling coordinate safely clamped within retained region
        vec2 texture_uv = clamp(oriented_uv, vec2(0.0), vec2(1.0)) * textureRegionScale + textureRegionOffset;

        // ---------------------------------------------------------------------
        // 2. TEXTURE SAMPLE & COLOR GRADING
        // ---------------------------------------------------------------------
        vec4 col;

        // Per-Display Chroma Bleed OR Camera Chromatic Aberration
        float c_bleed = screenEffect2.z;
        float c_optics = prismLensOptics.x;
        if (c_bleed > 0.001 || c_optics > 0.001)
        {
            vec2 offset = vec2(0.0);
            if (c_bleed > 0.001)
            {
                offset += vec2(c_bleed * 0.012, 0.0);
            }
            if (c_optics > 0.001)
            {
                offset += (oriented_uv - vec2(0.5)) * c_optics * 0.015;
            }

            vec2 r_uv = clamp(oriented_uv + offset, vec2(0.0), vec2(1.0)) * textureRegionScale + textureRegionOffset;
            vec2 b_uv = clamp(oriented_uv - offset, vec2(0.0), vec2(1.0)) * textureRegionScale + textureRegionOffset;
            
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

        // Camera Exposure EV Bias
        if (abs(prismLensOptics.w) > 0.001)
        {
            col.rgb *= exp2(prismLensOptics.w);
        }

        // Grayscale Desaturation
        if (screenEffect0.w > 0.001)
        {
            float luma = dot(col.rgb, vec3(0.2126, 0.7152, 0.0722));
            col.rgb = mix(col.rgb, vec3(luma), screenEffect0.w);
        }

        // Sepia Tone Blend
        if (screenEffect1.x > 0.001)
        {
            float luma = dot(col.rgb, vec3(0.2126, 0.7152, 0.0722));
            vec3 sepia_color = vec3(luma * 1.2, luma * 0.95, luma * 0.65);
            col.rgb = mix(col.rgb, sepia_color, screenEffect1.x);
        }

        // ---------------------------------------------------------------------
        // 3. OVERLAYS, ARTIFACTS & SIGNAL NOISE
        // ---------------------------------------------------------------------

        // Parametric Scanlines (Combined Optics & Screen Effect)
        float sl_intensity = max(prismLensOptics.z, screenEffect0.x);
        if (sl_intensity > 0.001)
        {
            float density = mix(200.0, 1200.0, screenEffect0.y);
            float sl_wave = sin(oriented_uv.y * density * 3.14159265) * 0.5 + 0.5;
            col.rgb *= mix(1.0, sl_wave * 0.45 + 0.55, sl_intensity);
        }

        // Interlace Field Line Shimmer
        if (screenEffect3.x > 0.001)
        {
            float field = step(0.5, fract(screenEffectTime * 30.0));
            float line_even = step(0.5, fract(oriented_uv.y * 240.0));
            float interlace_dim = abs(field - line_even);
            col.rgb *= mix(1.0, 0.75 + 0.25 * interlace_dim, screenEffect3.x);
        }

        // Analog Hash Static Noise
        if (screenEffect1.y > 0.001)
        {
            float static_noise = tv_hash(oriented_uv * 100.0 + vec2(screenEffectTime * 23.1, screenEffectTime * 47.3));
            col.rgb = mix(col.rgb, vec3(static_noise), screenEffect1.y * 0.45);
        }

        // Film Grain Noise (Optics)
        if (prismLensOptics.y > 0.001)
        {
            float grain = (tv_hash(oriented_uv + vec2(screenEffectTime)) - 0.5) * prismLensOptics.y * 0.15;
            col.rgb += vec3(grain);
        }

        // Brightness Flicker
        if (screenEffect2.y > 0.001)
        {
            float flick = tv_hash(vec2(floor(screenEffectTime * 24.0), 1.0));
            col.rgb *= (1.0 - screenEffect2.y * 0.3 * flick);
        }

        // Signal Dropout & Glitch Darkening
        if (screenEffect3.y > 0.001)
        {
            float drop_time = floor(screenEffectTime * 6.0);
            float drop_occ = step(0.93 - screenEffect3.y * 0.15, tv_hash(vec2(drop_time, 7.0)));
            col.rgb *= (1.0 - drop_occ * screenEffect3.y * 0.7);
        }

        // Vignette CRT Edge Darkening
        if (screenEffect2.w > 0.001)
        {
            vec2 v_coord = (oriented_uv - vec2(0.5)) * 2.0;
            float v_dist = dot(v_coord, v_coord);
            float vig_mask = clamp(1.0 - v_dist * 0.45 * screenEffect2.w, 0.0, 1.0);
            col.rgb *= vig_mask;
        }

        frag_color = col;
    }
}
```

---

## 5. UI Layout Sub-Panel (`indra/newview/skins/default/xui/en/floater_prism_manager.xml`)

Inside `displays_tab` -> `displays_layout_stack`, below display anchor controls:

```xml
<layout_panel
 auto_resize="true"
 follows="left|top|right"
 height="240"
 layout="topleft"
 name="effects_settings_panel">
    <text
     follows="left|top"
     font="SansSerifBold"
     height="18"
     layout="topleft"
     left="0"
     name="effects_heading"
     top="4"
     width="220">
        Per-Screen TV Effects (Selected Display)
    </text>

    <!-- Presets Bar -->
    <button
     follows="left|top"
     height="20"
     label="Reset (Clean)"
     layout="topleft"
     left="0"
     name="preset_clean"
     top="26"
     width="90" />
    <button
     follows="left|top"
     height="20"
     label="Old CRT"
     layout="topleft"
     left="96"
     name="preset_crt"
     top="26"
     width="75" />
    <button
     follows="left|top"
     height="20"
     label="Broken TV"
     layout="topleft"
     left="176"
     name="preset_broken"
     top="26"
     width="85" />
    <button
     follows="left|top"
     height="20"
     label="VHS Tape"
     layout="topleft"
     left="266"
     name="preset_vhs"
     top="26"
     width="75" />
    <button
     follows="left|top"
     height="20"
     label="Security Feed"
     layout="topleft"
     left="346"
     name="preset_cctv"
     top="26"
     width="95" />

    <!-- Effects Controls Grid -->
    <scroll_container
     follows="all"
     height="180"
     layout="topleft"
     left="0"
     name="effects_scroll"
     top="52"
     width="840">
        <panel
         follows="left|top|right"
         height="280"
         layout="topleft"
         name="effects_panel_doc"
         width="820">

            <!-- Row 1: CRT & Lines -->
            <slider
             decimal_digits="2"
             follows="left|top"
             height="18"
             increment="0.05"
             label="Scanlines"
             label_width="90"
             left="10"
             max_val="1"
             min_val="0"
             name="effect_scanlines"
             top="6"
             width="250" />
            <slider
             decimal_digits="2"
             follows="left|top"
             height="18"
             increment="0.05"
             label="Line Density"
             label_width="90"
             left="270"
             max_val="1"
             min_val="0"
             name="effect_scanline_count"
             top="6"
             width="250" />
            <slider
             decimal_digits="2"
             follows="left|top"
             height="18"
             increment="0.05"
             label="Pixelate"
             label_width="90"
             left="530"
             max_val="1"
             min_val="0"
             name="effect_pixelate"
             top="6"
             width="250" />

            <!-- Row 2: Color Tints -->
            <slider
             decimal_digits="2"
             follows="left|top"
             height="18"
             increment="0.05"
             label="Grayscale"
             label_width="90"
             left="10"
             max_val="1"
             min_val="0"
             name="effect_grayscale"
             top="32"
             width="250" />
            <slider
             decimal_digits="2"
             follows="left|top"
             height="18"
             increment="0.05"
             label="Sepia Tint"
             label_width="90"
             left="270"
             max_val="1"
             min_val="0"
             name="effect_sepia"
             top="32"
             width="250" />
            <slider
             decimal_digits="2"
             follows="left|top"
             height="18"
             increment="0.05"
             label="Chroma Bleed"
             label_width="90"
             left="530"
             max_val="1"
             min_val="0"
             name="effect_chroma_bleed"
             top="32"
             width="250" />

            <!-- Row 3: Sync & Roll -->
            <slider
             decimal_digits="2"
             follows="left|top"
             height="18"
             increment="0.05"
             label="Vertical Roll"
             label_width="90"
             left="10"
             max_val="1"
             min_val="0"
             name="effect_vertical_roll"
             top="58"
             width="250" />
            <slider
             decimal_digits="2"
             follows="left|top"
             height="18"
             increment="0.05"
             label="Roll Speed"
             label_width="90"
             left="270"
             max_val="1"
             min_val="0"
             name="effect_roll_speed"
             top="58"
             width="250" />
            <slider
             decimal_digits="2"
             follows="left|top"
             height="18"
             increment="0.05"
             label="VHS Tracking"
             label_width="90"
             left="530"
             max_val="1"
             min_val="0"
             name="effect_tracking"
             top="58"
             width="250" />

            <!-- Row 4: Noise & Glitch -->
            <slider
             decimal_digits="2"
             follows="left|top"
             height="18"
             increment="0.05"
             label="Static Noise"
             label_width="90"
             left="10"
             max_val="1"
             min_val="0"
             name="effect_static"
             top="84"
             width="250" />
            <slider
             decimal_digits="2"
             follows="left|top"
             height="18"
             increment="0.05"
             label="Flicker"
             label_width="90"
             left="270"
             max_val="1"
             min_val="0"
             name="effect_flicker"
             top="84"
             width="250" />
            <slider
             decimal_digits="2"
             follows="left|top"
             height="18"
             increment="0.05"
             label="Interlace"
             label_width="90"
             left="530"
             max_val="1"
             min_val="0"
             name="effect_interlace"
             top="84"
             width="250" />

            <!-- Row 5: Framing & Dropout -->
            <slider
             decimal_digits="2"
             follows="left|top"
             height="18"
             increment="0.05"
             label="Vignette"
             label_width="90"
             left="10"
             max_val="1"
             min_val="0"
             name="effect_vignette"
             top="110"
             width="250" />
            <slider
             decimal_digits="2"
             follows="left|top"
             height="18"
             increment="0.05"
             label="Dropout"
             label_width="90"
             left="270"
             max_val="1"
             min_val="0"
             name="effect_dropout"
             top="110"
             width="250" />
        </panel>
    </scroll_container>
</layout_panel>
```

---

## 6. Floater Controller Logic (`indra/newview/llfloaterprismmanager.cpp`)

> ⚠️ **Per §0.C5 the accessor names below are illustrative.** Use the REAL API: `selectedDisplay()` (→ `const LLPrismLens::DisplayDefinition*`, null-check), the member `mSelectedDisplay`, `selectedDisplay()->mSettings.mEffects`, and `LLPrismLens::setDisplaySettings(mSelectedDisplay, settings)`. Populate sliders in the existing display-selection refresh. Add the C7 Brightness slider to the getter/setter helpers.

### 6.1 Wiring Sliders & Presets in `postBuild()`

```cpp
// Helper function to read UI sliders into ScreenEffects
ScreenEffects getEffectsFromUI()
{
    ScreenEffects fx;
    fx.mScanlines     = static_cast<F32>(getChild<LLSlider>("effect_scanlines")->getValue().asReal());
    fx.mScanlineCount = static_cast<F32>(getChild<LLSlider>("effect_scanline_count")->getValue().asReal());
    fx.mPixelate      = static_cast<F32>(getChild<LLSlider>("effect_pixelate")->getValue().asReal());
    fx.mGrayscale     = static_cast<F32>(getChild<LLSlider>("effect_grayscale")->getValue().asReal());
    fx.mSepia         = static_cast<F32>(getChild<LLSlider>("effect_sepia")->getValue().asReal());
    fx.mStatic        = static_cast<F32>(getChild<LLSlider>("effect_static")->getValue().asReal());
    fx.mVerticalRoll  = static_cast<F32>(getChild<LLSlider>("effect_vertical_roll")->getValue().asReal());
    fx.mRollSpeed     = static_cast<F32>(getChild<LLSlider>("effect_roll_speed")->getValue().asReal());
    fx.mTracking      = static_cast<F32>(getChild<LLSlider>("effect_tracking")->getValue().asReal());
    fx.mFlicker       = static_cast<F32>(getChild<LLSlider>("effect_flicker")->getValue().asReal());
    fx.mChromaBleed   = static_cast<F32>(getChild<LLSlider>("effect_chroma_bleed")->getValue().asReal());
    fx.mVignette      = static_cast<F32>(getChild<LLSlider>("effect_vignette")->getValue().asReal());
    fx.mInterlace     = static_cast<F32>(getChild<LLSlider>("effect_interlace")->getValue().asReal());
    fx.mDropout       = static_cast<F32>(getChild<LLSlider>("effect_dropout")->getValue().asReal());
    fx.clampAndValidate();
    return fx;
}

// Helper function to populate UI sliders from ScreenEffects
void setUIFromEffects(const ScreenEffects& fx)
{
    getChild<LLSlider>("effect_scanlines")->setValue(fx.mScanlines);
    getChild<LLSlider>("effect_scanline_count")->setValue(fx.mScanlineCount);
    getChild<LLSlider>("effect_pixelate")->setValue(fx.mPixelate);
    getChild<LLSlider>("effect_grayscale")->setValue(fx.mGrayscale);
    getChild<LLSlider>("effect_sepia")->setValue(fx.mSepia);
    getChild<LLSlider>("effect_static")->setValue(fx.mStatic);
    getChild<LLSlider>("effect_vertical_roll")->setValue(fx.mVerticalRoll);
    getChild<LLSlider>("effect_roll_speed")->setValue(fx.mRollSpeed);
    getChild<LLSlider>("effect_tracking")->setValue(fx.mTracking);
    getChild<LLSlider>("effect_flicker")->setValue(fx.mFlicker);
    getChild<LLSlider>("effect_chroma_bleed")->setValue(fx.mChromaBleed);
    getChild<LLSlider>("effect_vignette")->setValue(fx.mVignette);
    getChild<LLSlider>("effect_interlace")->setValue(fx.mInterlace);
    getChild<LLSlider>("effect_dropout")->setValue(fx.mDropout);
}

// Commit callback handler when any effect slider changes
void onEffectSliderChanged()
{
    DisplayHandle selected_display = getSelectedDisplayHandle();
    if (selected_display.mId.isNull()) return;

    DisplaySettings settings = getSelectedDisplaySettings();
    settings.mEffects = getEffectsFromUI();
    LLPrismLens::setDisplaySettings(selected_display, settings);
}

// Preset Handlers
void applyPreset(const ScreenEffects& preset)
{
    setUIFromEffects(preset);
    onEffectSliderChanged();
}
```

### 6.2 Named Presets Data Map

```cpp
ScreenEffects presetClean; // All zero

ScreenEffects presetCRT;
presetCRT.mScanlines = 0.6f;
presetCRT.mScanlineCount = 0.5f;
presetCRT.mVignette = 0.4f;
presetCRT.mFlicker = 0.15f;

ScreenEffects presetBroken;
presetBroken.mGrayscale = 1.0f;
presetBroken.mStatic = 0.35f;
presetBroken.mVerticalRoll = 0.5f;
presetBroken.mRollSpeed = 0.3f;
presetBroken.mScanlines = 0.8f;
presetBroken.mDropout = 0.4f;

ScreenEffects presetVHS;
presetVHS.mTracking = 0.45f;
presetVHS.mChromaBleed = 0.6f;
presetVHS.mInterlace = 0.3f;
presetVHS.mFlicker = 0.2f;

ScreenEffects presetCCTV;
presetCCTV.mGrayscale = 1.0f;
presetCCTV.mScanlines = 0.4f;
presetCCTV.mStatic = 0.15f;
presetCCTV.mVignette = 0.5f;
presetCCTV.mPixelate = 0.2f;
```

---

## 7. Director Scene Serialization (`indra/newview/llprismlens.cpp` — `sceneData`/`applySceneData`, per §0.C1)

> ⚠️ Per §0.C1: put this in `PrismLensRegistry::sceneData()`/`applySceneData()` in **`llprismlens.cpp`** (~:3474/:3543), in the **`prism_displays`** item block (per-display; NOT the per-capture optics block, NOT `lldirectorcast.cpp` which is the separate actor/cast layer). Add a `brightness` key (C7). The key/value snippets below are correct — only the file/function/block changes.

When persisting scenes to LLSD maps, `ScreenEffects` fields are stored inside each display map:

```cpp
// Writing Display LLSD:
LLSD effects_sd;
effects_sd["scanlines"]      = fx.mScanlines;
effects_sd["scanline_count"] = fx.mScanlineCount;
effects_sd["pixelate"]       = fx.mPixelate;
effects_sd["grayscale"]      = fx.mGrayscale;
effects_sd["sepia"]          = fx.mSepia;
effects_sd["static"]         = fx.mStatic;
effects_sd["vertical_roll"]  = fx.mVerticalRoll;
effects_sd["roll_speed"]     = fx.mRollSpeed;
effects_sd["tracking"]       = fx.mTracking;
effects_sd["flicker"]        = fx.mFlicker;
effects_sd["chroma_bleed"]   = fx.mChromaBleed;
effects_sd["vignette"]       = fx.mVignette;
effects_sd["interlace"]      = fx.mInterlace;
effects_sd["dropout"]        = fx.mDropout;
display_sd["screen_effects"] = effects_sd;

// Reading Display LLSD (Safe with defaults):
if (display_sd.has("screen_effects"))
{
    LLSD fx_sd = display_sd["screen_effects"];
    fx.mScanlines     = static_cast<F32>(fx_sd["scanlines"].asReal());
    fx.mScanlineCount = static_cast<F32>(fx_sd["scanline_count"].asReal());
    fx.mPixelate      = static_cast<F32>(fx_sd["pixelate"].asReal());
    fx.mGrayscale     = static_cast<F32>(fx_sd["grayscale"].asReal());
    fx.mSepia         = static_cast<F32>(fx_sd["sepia"].asReal());
    fx.mStatic        = static_cast<F32>(fx_sd["static"].asReal());
    fx.mVerticalRoll  = static_cast<F32>(fx_sd["vertical_roll"].asReal());
    fx.mRollSpeed     = static_cast<F32>(fx_sd["roll_speed"].asReal());
    fx.mTracking      = static_cast<F32>(fx_sd["tracking"].asReal());
    fx.mFlicker       = static_cast<F32>(fx_sd["flicker"].asReal());
    fx.mChromaBleed   = static_cast<F32>(fx_sd["chroma_bleed"].asReal());
    fx.mVignette      = static_cast<F32>(fx_sd["vignette"].asReal());
    fx.mInterlace     = static_cast<F32>(fx_sd["interlace"].asReal());
    fx.mDropout       = static_cast<F32>(fx_sd["dropout"].asReal());
    fx.clampAndValidate();
}
```

---

## 8. Verification & Review Invariants

1. **Bit-Identical Behavior:** If all 14 effect values are set to `0.0`, all GLSL conditional branches (`if (screenEffectX > 0.001)`) evaluate to false, rendering an identical texture sample as prior code.
2. **Compilation Cleanliness:** Zero unused variables, explicit numeric conversions (`static_cast<F32>`), clean `#ifdef` matching in GLSL.
3. **No Allocation Overhead:** `CompositeState` carries plain array members (`F32 mScreenEffect0[4]`), avoiding `std::vector` or dynamic allocations during composition.
