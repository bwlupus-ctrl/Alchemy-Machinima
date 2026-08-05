# Prism Option B/C — fix brief for Gemini (before build)

You (Gemini) implemented Prism **Option B** (shared scratch G-buffer) + **Option C** (cinematic
optics) on top of `develop` @ `e77366a9057`. An adversarial review found **1 compile break (already
fixed by Claude)** plus **2 should-fixes + 1 nit** that the user wants resolved *before* the build.
Read **`GEMINI.md`** and **`doc/AI_AGENT_CODEBASE_ORIENTATION.md`** first. Do NOT build (Claude
builds); deliver compile-clean source. Baseline: the current working tree (your Option B/C changes,
with `llprismlens.cpp:4449` already corrected to `&gPipeline.mPrismLensSharedScratchRT`).

## Fix 1 (was should-fix #2) — kill the per-frame resize churn: fixed max-capacity shared scratch + sub-rect render
**Problem:** `mPrismLensSharedScratchRT` is still **exact-size** — `allocatePrismLensBuffer`
(`pipeline.cpp:~1357`) reallocates whenever `rt.width/height != requested`. Because the single pack
now round-robins across captures of different sizes (one aux render/frame), two active captures at
different resolutions **free+realloc ~6 GL targets every frame** (deferredScreen + attachments +
screen + deferredLight + waterDis + waterExclusion) → driver stalls / hitching. The old 3-pack design
avoided this; a single *exact-size* pack is worse than what it replaced.

**Fix — allocate the shared pack ONCE at the max bucket (1024²) and render each capture into a logical
sub-rect of it:**
- Allocate `mPrismLensSharedScratchRT` (+ the shared water-dis / water-exclusion scratch) at the
  **fixed maximum axis (1024)**, once; do **not** reallocate on per-capture size change (only
  allocate if not yet allocated / on master-off→on). Keep it until release.
- Let `setPrismLensLogicalExtent(w,h)` set a logical viewport/scissor that is a **sub-rect
  `[0,0,w,h]` of the max pack** (logical ≤ physical). It currently enforces logical == physical
  (`pipeline.cpp:~1425-1429`) — relax that so logical can be smaller, and make
  `applyPrismLensLogicalViewport` install the `[0,0,w,h]` viewport+scissor on the max-size pack.
  `getPrismLensLogicalExtent`/`getWaterDisTarget` gates (compare `mRT == &mPrismLensSharedScratchRT`)
  stay as-is.
- **CRITICAL (this is the exact risk the review watched for): the copy into the per-slot retained
  output must copy ONLY the `[0,0,render_width,render_height]` sub-rect** (llprismlens.cpp:~4669), so a
  smaller capture never picks up leftover texels from a previous larger capture that shares the pack.
  Clear the sub-rect before rendering. After this change, re-verify: capture A at 1024² then capture B
  at 256² → B shows only B's pixels, zero bleed.
- Fix the now-stale comments (`pipeline.cpp:~1347` "each of the three … owns one exact-size pack";
  `pipeline.h` "exact-size … shared serially") to describe the max-capacity + sub-rect model.

## Fix 2 (was should-fix #3) — make the Option C optics actually settable + persisted
**Problem:** `CameraSettings::mOptics` (chromatic aberration / film grain / CRT scanlines / exposure
bias) is only ever **read** (`getCompositeStates` → `mOpticsParams`, llprismlens.cpp:~4751). Nothing
writes a non-zero value — no UI, no settings, no scene persistence — so the whole Option C chain is
permanently `(0,0,0,0)` dead code. These are **per-camera** properties, so they belong on the Prism
Manager Capture editor, not a global setting.

**Fix:**
- **UI:** add four controls to the Camera-feed capture editor in
  `skins/default/xui/en/floater_prism_manager.xml` (the `camera_settings_panel`): Chromatic
  aberration, Film grain, CRT scanlines, Exposure bias (EV). Match the existing spinner style; sensible
  ranges (e.g. CA 0–1, grain 0–1, scanlines 0–1, EV −4…+4), default 0.
- **Commit:** in `llfloaterprismmanager.cpp` `onCommitCameraSettings()`, read those controls into
  `settings.mOptics.*` (use `static_cast<F32>(spinner->getValue().asReal())` — this project is `/WX`)
  before calling `LLPrismLens::setCameraSettings(...)`. Populate them in `refreshCaptureEditor()` from
  the selected capture so they round-trip in the UI.
- **Persistence (Director v3):** add `mOptics` to `CameraSettings` (de)serialization in
  `llprismlens.cpp` (`sceneData()` writes them under the capture's optics block; `applySceneData()`
  reads them with **finite + range validation**, defaulting to 0/off when absent — absent must load
  clean, never reject the scene).

## Fix 3 (was nit #4) — clamp the chromatic-aberration sample to the sub-rect
`prismLensF.glsl:~62,64`: the CA radial offset `(oriented_uv ± dist)` can push the sampled UV up to
~`dist` outside `[0,1]`; after the region transform that reads a texel or two **outside** the written
`[0,0,render_width,render_height]` sub-rect (stale/uninitialized output texels), r/b channels. Clamp
the CA-offset UV to the capture's written sub-rect (pass the sub-rect max as a uniform, or clamp in
region-transform space) so it never samples outside the fresh region. (Relevant once Fix 1 makes the
output a sub-rect of a max pack.)

Optional polish (nit #5, skip unless trivial): grain has no time term (static pattern) and scanline
frequency is hardcoded 600.0 (aliases on small captures). Artistic only.

## Invariants to keep (do not weaken)
`setViewNoBroadcast` only (never `setView`); full `ScopedPrismRenderState` save/restore on every exit
path; one aux scene render per frame; composite samples linear-HDR **before** main tonemap; Prism-OFF
frame byte-identical; default-off; don't touch clone/overlay/`actorghostF`/`LLActorMover`; add nothing
to `git add -A`. Shaders compile at RUNTIME — hand-check `#ifdef` balance. Warnings are errors.

## Deliver
The edited files + a short note of what changed at which file:line, and confirm: the shared pack no
longer reallocates per frame (fixed max size + sub-rect); the sub-rect copy prevents cross-capture
bleed; the optics are settable in the manager, committed to `CameraSettings`, and round-trip through a
v3 scene save/load; the CA sample is clamped to the sub-rect; `git diff --check` clean. Claude will
adversarially re-review (focus: sub-rect/stale-texel correctness + persistence round-trip) and build.
