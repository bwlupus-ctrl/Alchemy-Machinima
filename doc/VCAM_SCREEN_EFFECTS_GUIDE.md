# VCam per-screen TV effects — deep implementation guide

Give each VCam **display face** ("screen") its own **mix-and-match** set of adjustable TV/CRT effects
(scanlines, pixelation, B&W/sepia, analog static, vertical roll, VHS tracking, flicker, chroma bleed,
vignette, interlace, signal dropout, …). Confirmed design (user, 08-05): **per-DISPLAY** (each screen
independent) and **mix-and-match** (a panel of independent effects, each on/off + intensity, freely
combinable — not a single-look dropdown). Model it on the clone stylizations for spirit, but implement
as independent composable sliders like the existing camera "optics."

> **Read `GEMINI.md` + `doc/AI_AGENT_CODEBASE_ORIENTATION.md` first. Do NOT build/commit/`git add`.**
> Deliver compile-clean `/WX` source; Claude builds; an Opus sub-agent adversarially reviews to zero
> must-fix. **SEQUENCING: do not start until the in-flight mesh-display fix
> (`doc/PRISM_MESH_DISPLAY_FIX_BRIEF.md`) has landed** — it edits `llprismlens.cpp` +
> `llfloaterprismmanager.cpp`, which this feature also touches; starting concurrently will collide.

## The foundation you're extending (read these to copy the pattern)
The VCam feed is composited per display face; each face is drawn with **per-display uniforms**. Trace:
- `LLPrismLens::CompositeState` (`llprismlens.h` ~283) already carries per-display params incl.
  `mOpticsParams[4]` (chromatic aberration / film grain / CRT scanlines / exposure — Option C).
- `getCompositeStates(...)` (`llprismlens.cpp` ~4722) fills a `CompositeState` per display, copying
  the capture's `mOptics` into `mOpticsParams`.
- The composite draw loop in `pipeline.cpp` `renderDeferredLighting` (~16852+, the `gPrismLensProgram`
  block) uploads per-display uniforms (`sPrismLensOptics`, letterbox `sBarColorLinear`, `sEdgeFeather`,
  the retained-orientation transform) and draws each display face.
- The composite fragment shader `app_settings/shaders/class1/deferred/prismLensF.glsl` samples the
  retained feed and applies `prismLensOptics`.

**Key consequence:** per-screen effects are *the same kind of thing already flowing per display* — you
are adding more per-display params to an existing, proven, allocation-free path. That is what makes it
robust in realtime.

## Data model (per-display)
Today `mOptics` is on the **capture** (`CameraSettings`, per camera). The new effects are per
**display**. Add a `ScreenEffects` struct and attach it to the display binding (`PrismDisplay`):
```cpp
struct ScreenEffects            // all 0 = off; ranges noted; validated on load
{
    F32 mScanlines      = 0.f;  // 0..1 strength   (may reuse/supersede optics CRT)
    F32 mScanlineCount  = 0.f;  // 0..1 -> mapped to a line density (avoid hardcoded 600)
    F32 mPixelate       = 0.f;  // 0..1 -> block size
    F32 mGrayscale      = 0.f;  // 0..1 desaturate
    F32 mSepia          = 0.f;  // 0..1 sepia tint (mutually blends after grayscale)
    F32 mStatic         = 0.f;  // 0..1 analog noise (time-animated)
    F32 mVerticalRoll   = 0.f;  // 0..1 V-sync roll band strength ("unstable vertical hole")
    F32 mRollSpeed      = 0.f;  // 0..1 -> roll rate
    F32 mTracking       = 0.f;  // 0..1 horizontal VHS tear/jitter (time-animated)
    F32 mFlicker        = 0.f;  // 0..1 brightness flicker (time-animated)
    F32 mChromaBleed    = 0.f;  // 0..1 horizontal color-fringe (distinct from optics CA)
    F32 mVignette       = 0.f;  // 0..1 edge darkening / CRT curvature feel
    F32 mInterlace      = 0.f;  // 0..1 interlace shimmer (time-animated)
    F32 mDropout        = 0.f;  // 0..1 signal dropout / ghosting
};
```
Keep the count reasonable; every field is an independent, bounded knob. Decide (and note) whether the
existing 4 per-capture optics stay as a camera-level grade **on top of** per-display effects
(recommended — additive, no migration) or get folded in; do NOT silently break the committed optics.

Pass to the shader by **packing into a small fixed set of `vec4` uniforms** (e.g.
`screenEffect0..screenEffect3`) set per display in the composite loop — mirror exactly how
`sPrismLensOptics` is uploaded. Add the packed values to `CompositeState` (like `mOpticsParams`) and
fill them in `getCompositeStates` from the selected display's `ScreenEffects`.

## Shader (`prismLensF.glsl`) — composable, ordered, time-aware, bounded
Add a **frame-time uniform** (`screenEffectTime`, seconds, from a monotonic viewer clock — set it in
the composite draw; this also fixes the known "grain has no time term" nit). Apply effects in a fixed,
composable order so any mix reads correctly:
1. **Sampling-space (modify the UV before the feed fetch):** pixelation (quantize UV to blocks),
   vertical roll (offset V by `fract(time*rollSpeed)`, with a dark band across the wrap seam = the
   "vertical hole"), VHS tracking (per-scanline horizontal jitter via a time+row noise), mild
   vignette/curvature warp. Clamp the resulting UV to the feed's valid sub-rect (respect the retained
   region transform already used for the optics/CA — do not sample outside it).
2. **Fetch** the feed (single sample; for chroma bleed take up to 2 extra horizontally-offset samples).
3. **Color:** grayscale (luma), then sepia blend, then chroma bleed recombine, then existing exposure.
4. **Overlay/attenuate:** scanlines (parameterized density, not hardcoded), interlace (even/odd row
   dim by `time`), static (hash noise by `uv*res + time`), flicker (brightness *`(1 - flicker*noise(time))`),
   dropout/ghosting, final vignette darkening.
Every step gated so `param==0` is a no-op and the un-effected result is **bit-identical to today**
(a screen with all sliders at 0 must look exactly like the current feed). Keep `#ifdef` balance;
shaders compile at runtime, and a new/edited `.glsl` is not auto-copied to the Release tree.

## UI (`floater_prism_manager.xml` + `llfloaterprismmanager.cpp`)
In the **Displays** tab, when a display row is selected, show a per-screen "Effects" sub-panel: one
compact slider (0..1) per effect, grouped (CRT / color / signal), in a scroll container so it fits.
On commit, read the sliders into the selected display's `ScreenEffects` and call the registry setter;
in the display-refresh, populate the sliders from the selected display. Every `getChild<LLSpinCtrl>`/
`<slider>` name must match the XUI. `setValue`/`getValue().asReal()` → `static_cast<F32>` (/WX). This
is per-display state, so the panel must reflect the *selected display*, not the capture.

## Persistence (Director scene, bump to v4 or extend v3 display block)
Serialize each display's `ScreenEffects` in `sceneData()` (under the display's block) and read back in
`applySceneData()` with **finite + range validation**, defaulting each field to 0 when absent (absent
must load clean, never reject the scene). Mirror exactly how the capture `mOptics` round-trips.

## Robustness invariants (the user's explicit requirement)
- **Composite-time only.** No extra scene render, no per-frame allocation, no growable state — pure
  bounded per-pixel math on the already-captured feed. This is what keeps it realtime-safe.
- **All params clamped** to their range in code before upload; the shader must not divide by zero
  (guard pixel-block size, line count, etc.).
- **Time from a monotonic viewer clock** passed as a uniform; no per-frame CPU work per display beyond
  copying floats.
- **All-zero == today.** A display with no effects is bit-identical to the current feed; **Prism/VCam
  off is byte-identical** to before. Default every field 0/off.
- Don't touch clone/overlay/`actorghostF`/`LLActorMover`; internal symbols/settings keys unchanged.

## Phased plan (each phase reviewable + independently shippable)
- **P1 — plumbing + 2 effects end-to-end:** add `ScreenEffects` to `PrismDisplay`, the packed uniforms
  through `CompositeState`/`getCompositeStates`/the composite draw, the `screenEffectTime` uniform, and
  implement **grayscale + scanlines** in the shader + two sliders in the Displays tab + v-scene
  persistence. Proves the per-display path (two screens on one camera, one B&W one clean).
- **P2 — the rest of the palette:** pixelation, static, vertical roll, tracking, flicker, chroma bleed,
  vignette, interlace, dropout — each an independent composable slider.
- **P3 — polish:** grouping/scroll UI, a few named presets ("Broken TV" = B&W+static+roll, "Old CRT" =
  scanlines+vignette+flicker) that just set slider bundles, and the effect ordering tuned.

## Deliver
Per-phase, per-file changelog; the all-zero==today proof; `git diff --check` clean; list every changed
runtime asset (the `.glsl` + XUI must be hand-deployed). Claude + Opus review (focus: all-zero
byte-identical, clamped/no-NaN shader, per-display state correctness, /WX) then build.
