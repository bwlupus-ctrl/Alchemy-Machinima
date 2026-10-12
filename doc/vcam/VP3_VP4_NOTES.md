# VCam Projector VP3 + VP4 implementation notes

## Feed preparation

- Each projector binding owns a lazily allocated square `GL_RGBA8` render target with `TMG_AUTO` at its 256/512/1024 setting; projector aspect is handled by the logical FIT/FILL mapping. Binding/settings changes, unbind, target invalidation, capture deletion, scene replacement, and render-target/context release discard the prepared target.
- A conversion pass runs only after that capture publishes a new full-quality retained frame. The Tracy CPU/GPU zone is named `VCam projector feed prep`; unchanged publications are skipped by publication serial.
- `prismProjectorFeedF.glsl` reuses `prismSignalF.glsl` plus the same eight tonemap/color-grade/legacy permutations and uniform upload path as monitor previews. Capture exposure bias and feed EV are combined, optional main exposure is honored, output is clamped display-sRGB with alpha 1, and `flush()` generates the full mip chain.
- Projector FIT/FILL/STRETCH, anchor, frozen capture orientation, retained logical UV region, and screen effects are resolved before conversion. FIT bars are hard black; the retained glow channel never becomes cookie opacity.

## Pipeline binding

- `bindProjectorCookie()` is shared by deferred spot and multi-spot surface lighting, BDMerge projector shafts, and froxel injection through the two existing setup functions. Cine rig emitter volumes therefore use the same seam whenever they have an object binding.
- A valid prepared feed replaces only `projectionMap`; focus/ambient mip behavior and shadow-map selection remain unchanged. Feed cookies use trilinear or anisotropic mip filtering and `proj_lod` derives from the prepared width.
- The feed texture always beats a procedural gobo. `mKeepGobo` optionally retains texture-path tint, animation/flicker, zoom, and dispersion while forcing `gobo_pattern = -1`; otherwise those controls are neutral. Before a prepared frame, OWN_COOKIE executes the legacy path, while BLACK/WHITE use the configured fallback.
- With no registry binding, the helper performs the former bind/filter/uniform sequence unchanged. No additional texture bind or shader uniform is issued.
- Both cinematic probe signatures include capture UUID plus generation when a binding exists. Publication serial/content is intentionally excluded, so bind/unbind/reassign dirties probes without video-frame churn.

## VP1/VP2 review follow-ups

- Scene load now quarantines a loaded object that stopped being a projector instead of rejecting the full Prism payload.
- Display reassignment suppresses the old capture only when both its display and projector counts reach zero.
- Deleting a capture detaches Gate-subscribed projector rows for rerouting instead of deleting them with the currently routed capture.
- The feature-off consumption path checks `prismEnabled()` before constructing/accessing the registry singleton.

## Verification boundary

- Per request, no configure, build, executable test, commit, fetch, or upstream operation was performed. Static verification covered every `projectionMap` producer/bind site, declaration/definition parity, `git diff --check`, LF-only files, shader-family registration/unload, and ownership exclusions.
- Runtime acceptance still requires the VP8 visual/readback/performance gates: mip0/last-mip/alpha/FIT-bar readback, ALM/context-loss reallocation, orientation and monitor color match, wall/focus/shaft behavior, no-binding image diff, and the <= 0.1 ms 512-class Tracy measurement.
