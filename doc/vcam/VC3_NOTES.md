# VC3 implementation notes

## Implemented

- Moved the complete mapping/optics/orientation/TV-effect body into `prismSignalSample`; both the original world entry point and the preview entry point link that shader object (`indra/newview/app_settings/shaders/class1/deferred/prismSignalF.glsl:59`, `prismLensF.glsl:13`, `prismPreviewF.glsl:9`). Default-world equivalence is guaranteed by the original body executing unchanged through the same uniforms and operation order; the world entry point only forwards its original varyings.
- Added the eight main-post permutation peers at shader load, with all-or-nothing preview availability and fail-open world Prism (`indra/newview/llviewershadermgr.cpp:1877`, `indra/newview/llviewershadermgr.h:276`).
- Centralized signal-state packing for world, auxiliary, raw, and selected-face previews. Raw has capture optics, neutral centered aspect bars, and no display effects; face previews use the selected display's Fit/Fill/Stretch, anchor, bars, orientation, sheen, and six effect packs (`indra/newview/llprismlens.cpp:8814`, `:9140`, `:9393`, `:9409`). A real face without a resolved basis returns OFFLINE.
- Added the fixed 1024x288, 4x2 RGBA8 atlas. It allocates lazily once per GL lifetime, services one publication per call in deterministic round-robin order, draws directly without readback, and publishes an opaque generation plus cell UV, LIVE/PREVIEW/PAUSED/OFFLINE/THROTTLED state, serial, and timestamp (`indra/newview/llprismpreview.h:121`, `:134`, `indra/newview/llprismpreview.cpp:368`).
- Atlas work runs after the auxiliary scene scope under a fresh `ScopedPrismRenderState` and a GL guard covering target stack, viewport/scissor, blend/depth/color mask, framebuffer-sRGB, shader, texture unit 0, camera, and pipeline pack. Disabled/no-queue returns before GL access (`indra/newview/llprismlens.cpp:8369`, `:8803`, `indra/newview/llprismpreview.cpp:267`, `:370`).
- HDR conversion samples the still-previous `mExposureMap` (the atlas runs before the main view), uses the same eight-way selector and effective tonemap/color-grade uniform upload as `colorCorrect`, performs explicit linear-to-sRGB, and never compiles or binds `BLOOM_COMPOSITE` (`indra/newview/pipeline.cpp:12892`, `:12909`, `:13772`, `:13918`, `:31699`, `indra/newview/llprismpreview.cpp:412`, `:445`).

## Verification and deviations

- Per request, no configure, build, executable test, commit, fetch, or upstream operation was run. Static verification covered `git diff --check`, symbol/call-site review, shader-stage linkage review, and LF checks.
- No intentional VC3 behavior deviation. VC4's GPU/CPU budget controller and measured self-test remain outside this package; VC3 exposes timestamps/state and preserves the required one-cell-per-frame ceiling.

## Fix round

- Preview-only camera and Surface Lens renders now always use the full pinned 192x108 scratch/output extent. Producer and selected-face aspect are centered only by the atlas composite UV mapping; no aspect-sized deferred pack is requested.
- `ScopedPreviewAtlasGL` now snapshots and restores the active unit and unit-0 binding exclusively through `LLRender`. Render-target release rebases lens/publication/atlas serials together, invalidates atlas handles, marks retained tiles stale, and makes the first new publication eligible.
- Generation reuse clears monitor selection and tile state. Tile queries derive PAUSED/THROTTLED from current runtime flags and expose EMPTY, READY, HELD, ERROR, and SHADER_UNAVAILABLE states; `connectChanged` reports service revisions. Visibility overflow now warns and ignores the extra owner.
- The AO regression runs the production target/readiness decision before checking the real neutral clear color. The main color-correct pass retains its original `pow` AgX expression and no longer repeats the preview helper's tonemap/color-grade uploads.
- Per request, the fix round did not configure, build, commit, fetch, or contact an upstream. Static verification covers scoped diffs, exact render-size call sites, absence of raw texture-unit calls in the atlas guard, `git diff --check`, and LF checks. `prismSignalF.glsl` and `prismLensF.glsl` were not changed.

## Fix round 2

- RAW Surface Lens previews now derive their logical aspect from the prepared surface basis instead of the fixed 192x108 render target. Selected FILL faces keep the face boundary as the atlas letterbox boundary so capture pixels do not leak into its bars.
- Turning Prism off now rebases the preview service together with the lens resource release. Successful atlas composites publish READY, terminal FAILED/SHADER_UNAVAILABLE states win over warming/throttling overlays, and a target release publishes EMPTY when the atlas no longer exists.
- Per request, this round was reviewed statically only: no configure, build, executable test, commit, fetch, or upstream operation was run.
