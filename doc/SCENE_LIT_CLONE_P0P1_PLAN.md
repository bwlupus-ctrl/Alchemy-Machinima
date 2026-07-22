# Scene-Lit Clone Proxy — P0 + P1 Implementation Plan (Codex, 2026-07-22)

Design-first plan for the first two phases of the scene-lit clone project (deferred-injection
render proxy). Builds on `doc/SCENE_LIT_CLONE_RESEARCH_FINDINGS.md` ("Architecture A+"). Source of
this plan: Codex design consult 2026-07-22. **No code written yet — an implementation-design round
comes before edits (mandatory Codex loop).**

## The clean boundary: preserve DATA + TRANSFORM, replace RENDERING
- **Reuse:** the harvest (`mGhostBatches` incl. `GhostBatch::mPass`, `mGhostStaticFaces`), source/
  instance resolution, the `T(foot)*R(mRotation)*S(mScale)*T(-pivot)` placement math, live
  `LLRenderPass::uploadMatrixPalette()` primitives, cutoff/material lookup helpers where semantics
  match, non-rigged attachment transform composition.
- **Do NOT reuse:** `renderStudioGhosts()` orchestration, the Ghost FX/UI shaders, its depth-prime +
  colour sweeps, its far→near sort, frozen-palette override behaviour, alpha/glow handling, UI GL
  state. **The deferred path needs its own pass-faithful draw routine** — do not retrofit the
  post-tonemap overlay into the G-buffer.

## Two constraints to lock BEFORE implementation
1. The new renderer entry point **takes an explicit camera/view context + transformed bounds** — no
   hard-coded `CAMERA_WORLD` / singleton camera — so the hero-probe mirror pass can call the same
   build/cull/submit path later. (Mirror coverage is why the scene-lit proxy exists per the mirrors
   brief; this is the hook.)
2. Deferred submission has its **own** pass-faithful draw path (see boundary above).

## Ordering fix — MOVE the harvest (don't split, don't re-harvest)
`collectGhostBatches()` runs today at [llviewerdisplay.cpp:1086](../indra/newview/llviewerdisplay.cpp) —
AFTER the deferred pass. Move the single call to **immediately after the world `stateSort()` /
optional `rebuildPools()` block** (stateSort at [llviewerdisplay.cpp:919](../indra/newview/llviewerdisplay.cpp),
block ends ~932); remove the late call at 1086.
- Safe there: the collector reads attachment `LLSpatialGroup::mDrawMap` directly (geometry-scoped,
  not the `LLCullResult` render maps) — see its own note at
  [llactormover.cpp:5054](../indra/newview/llactormover.cpp). Running after `rebuildPools()` avoids
  harvesting right before a pool rebuild reorganises geometry.
- Raw `LLDrawInfo*`/`LLFace*` stay same-frame-only, consumed before `gPipeline.clearReferences()`.
- **P0 stamps the queue with the current frame number and rejects cross-frame consumption.** (A
  source-generation cookie handles rebuild/staleness more rigorously later.)

## P1 — frame-local proxy: LIVE opaque + masked only
### Insertion point
Directly after `gPipeline.renderGeomDeferred(*LLViewerCamera::getInstance(), true)` at
[llviewerdisplay.cpp:1046](../indra/newview/llviewerdisplay.cpp), still inside `deferredScreen`,
BEFORE texture-unit cleanup / `rt.flush()` / Hi-Z/SSAO/SSR/velocity / `renderDeferredLighting()` (~1068):
```
gPipeline.renderGeomDeferred(camera, true);
LLActorMover::instance().renderGhostDeferredOpaqueMasked(camera);   // round 1: mover builds queue
```
Target end state: **pipeline-owned** `LLPipeline::renderGhostDeferredOpaqueMasked(...)` consuming a
frame-local queue. Never recursively call `renderGeomDeferred()`.

### Frame-local record (build every view/frame; NEVER retain LLDrawInfo*)
```
GhostProxy { instance/source id; placement G; world-space bounds;
             refs to wearer's GhostBatch + GhostStaticFace vectors;
             live/frozen mode (P1 = LIVE only); frame stamp / source cookie }
```
Frustum-cull the transformed proxy bounds against the **passed** camera. No world occlusion, no
spatial-DB insertion. Frozen palettes = P3.

### Pass whitelist (accept — explicit, not a blacklist)
`PASS_SIMPLE_RIGGED`, `PASS_FULLBRIGHT_RIGGED`, `PASS_FULLBRIGHT_SHINY_RIGGED`, `PASS_SHINY_RIGGED`,
`PASS_BUMP_RIGGED`, `PASS_MATERIAL_RIGGED`, `PASS_MATERIAL_ALPHA_MASK_RIGGED`,
`PASS_MATERIAL_ALPHA_EMISSIVE_RIGGED`, `PASS_SPECMAP_RIGGED`, `PASS_SPECMAP_MASK_RIGGED`,
`PASS_SPECMAP_EMISSIVE_RIGGED`, `PASS_NORMMAP_RIGGED`, `PASS_NORMMAP_MASK_RIGGED`,
`PASS_NORMMAP_EMISSIVE_RIGGED`, `PASS_NORMSPEC_RIGGED`, `PASS_NORMSPEC_MASK_RIGGED`,
`PASS_NORMSPEC_EMISSIVE_RIGGED`, `PASS_ALPHA_MASK_RIGGED`, `PASS_FULLBRIGHT_ALPHA_MASK_RIGGED`,
`PASS_GLTF_PBR_RIGGED`, `PASS_GLTF_PBR_ALPHA_MASK_RIGGED`. (`*_EMISSIVE_RIGGED` = base geometry
passes, keep them.)

**Reject in P1:** `PASS_ALPHA_RIGGED`, `PASS_MATERIAL_ALPHA_RIGGED`, `PASS_SPECMAP_BLEND_RIGGED`,
`PASS_NORMMAP_BLEND_RIGGED`, `PASS_NORMSPEC_BLEND_RIGGED`, `PASS_GLTF_GLOW_RIGGED`, `PASS_GLOW_RIGGED`,
invisible/water-exclusion, post-bump/cosmetic, anything not listed.

`GhostStaticFace`: accept `mAlphaKind` 0/1, reject 2 — BUT choosing the right deferred program needs
the face's real GLTF/legacy material, not just `mAlphaKind`. If that means reproducing pool logic,
**rigged-only is the safer first internal milestone**; opaque/masked non-rigged attachments remain
the P1 deliverable target. **Never** call `LLDrawPoolAvatar::renderDeferred()` wholesale (`mDrawFace`
can redraw unrelated geometry) — submit whitelisted render-map batches directly.

### Minimal state safety (`ScopedGhostTransform` per placement)
save modelview → compose `view*T(foot)*R*S*T(-pivot)` → `gGL.syncMatrices()` → `gGLLastMatrix =
nullptr` → draw only whitelisted batches → restore modelview → sync + invalidate `gGLLastMatrix`
again. **`gGLLastMatrix` invalidation between EVERY clone**, incl. two adjacent same-source clones.
Scope: modelview/texture matrices, shader/program (via each deferred pass begin/end), VBO binding,
active texture unit + bound textures, depth test/write/func, blend enable/func (finish OFF),
cull enable/mode (+ GLTF double-sided), colour mask, `gGLLastMatrix`, palette-dedup locals
(`lastAvatar`/`lastMeshId`/`skipLastSkin`) scoped per submission, + assert FBO/viewport unchanged.
Prefer existing RAII GL wrappers + normal `beginDeferredPass`/draw/`endDeferredPass`. **Do NOT mutate
pipeline globals** (`sUseOcclusion`, `sShadowRender`, `sImpostorRender`, `sReflectionRender`,
`sRenderDeferred`, `sNoAlpha`, `RenderSpotLight`, camera/mirror/probe mode) — ASSERT unchanged
(saving/restoring would hide contamination). No flush/resolve/target-switch/viewport-change/query/
nested render inside submission.

Deferred to later phases: prev view/proj + velocity + history (P2); frozen palette resolver + dedup
suppression (P3); shadow flags/cascade/spotlight (P4); alpha sort + water staging (P5); mirror
clip-plane/probe state (mirror integration, same API); occlusion queries — never for P1 proxies.

## P0 — smallest diagnostics/invariants slice that de-risks P1 (LOAD-BEARING)
- Frame/view stamp on the queue, asserted at consumption.
- Counters: LIVE instances considered; proxies built / frustum-culled / submitted; rigged batches
  harvested; accepted opaque / accepted masked / rejected blended-glow-unknown; static faces
  accepted/rejected; stale/null skips; actual draw calls.
- **Reusable** scoped before/after invariant checker around `renderGhostDeferredOpaqueMasked()`: FBO,
  viewport/scissor, proj/modelview/texture matrices, colour mask, depth/blend/cull/stencil enable+func,
  active shader, active texture unit, the listed pipeline flags + camera/view identity. (Later phases
  ADD fields, not a new framework.)
- Unknown-pass diagnostics: loud fail in debug, count/drop in release.
- Debug render/log of transformed proxy bounds + cull decisions.
- **G-buffer contamination test** (the #1-risk falsifiable check): identical deterministic frames
  Studio OFF vs ON; compare each deferred attachment (albedo/base, normals, material/spec, emissive,
  depth) **before lighting**; mask the union of clone screen-space bounds (slightly expanded); require
  ZERO diff outside. Run with 1 clone, then **2 placements of the same source at separated transforms**
  — the second is the best detector of stale matrix/palette caches.

Nice-but-not-load-bearing for P1: velocity viz (no MVs yet), GPU timers, LOD/residency telemetry,
shadow/mirror/alpha counters, baseline-image infra beyond the focused G-buffer diff.

## Decisive P1 in-viewer test
LIVE clone half in sun / half in shadow, intersecting/behind world geometry; masked hair + a PBR
attachment with clear normal/roughness; move sun+camera, animate the source. Verify: correct depth,
occluded by world geometry, receives deferred lighting + world shadows, cutoff HOLES (not solid hair
cards), follows live pose; blended clothing + glow-only redraws ABSENT by design; casts NO shadow (P1).
**Biggest risk = global-state / matrix-cache contamination, not missing material coverage.**
Falsifiable: Studio OFF vs ON from a fixed camera, raw G-buffer diff outside expanded clone bounds =
zero; repeat with two same-source clones at separated transforms — if world pixels change, or clone 2
renders at clone 1's transform, P1 FAILS.

## Verdict
P0 + P1 is the correct first slice; nothing architectural must precede it. Lock the two constraints
above, then do an implementation-design round (exact function signatures, the `ScopedGhostTransform`
shape, the whitelist switch, the invariant-checker API) before editing.
