# Overlay↔Entity Parity — Implementation Design (Codex, adversarial)

Design of record for making the Overlay ghost CLONE render identically to the source (=
Entity), reusing the live render path. Produced by Codex against the current code +
the scene-lit-clone history. Companion to
`GHOST_OVERLAY_ENTITY_PARITY_DEEP_RESEARCH_BRIEF.md`. Implement top-down; each slice
goes through the build → adversarial Codex code-review → in-world-test loop.

---

## 1. Recommended approach

**Choose (A): replay harvested batches through the REAL material shaders, split across
their proper render phases** — NOT a bespoke shader, and NOT literal (B) "all categories
through deferred."

- Opaque + alpha-masked → G-buffer (already: `renderGhostDeferredOpaqueMasked`).
- Fullbright opaque/masked/shiny → existing post-deferred solid stages (already).
- **Alpha-blended → real forward-alpha shaders AFTER lighting** (the gap / first slice).
- Emissive/glow → proper additive/bloom stage (later).
- Bespoke `gActorGhostProgram` remains ONLY for stylized ghost/hologram/x-ray/wireframe.

"Use the real path" = match its shader-selection + binding semantics, **not** call
`LLRenderPass::pushBatch()` unchanged (that applies the source model matrix and would
overwrite the ghost placement; the ghost helpers intentionally omit it —
`pipeline.cpp:4629,4674`; `ScopedGhostTransform` composes the clone modelview —
`pipeline.cpp:4923,4934,4948`).

Foundation already present:
- Legacy deferred material batches bind real diffuse/normal/spec + uniforms (`pipeline.cpp:4751,4760,4770`).
- Scalar PBR calls `LLFetchedGLTFMaterial::bind()` + respects `mDoubleSided` (`pipeline.cpp:4698,4704,4710`).
- Indexed PBR uploads per-slot factors/cutoffs/maps/KHR transforms (`pipeline.cpp:4811,4852,4855,4857,4875`).
- Real rigged palette uploaded per batch before draw (`pipeline.cpp:5388`).

Reject literal (B): alpha blend can't be widened into `renderGhostDeferredOpaqueMasked`
(needs depth-tested, non-depth-writing forward compositing AFTER deferred light apply;
`pipeline.cpp:4967,4974,14074,14144`). Keep (B)'s staged pipeline hooks as the execution
framework for (A). Prior hazards: skipping blend/static while suppressing the whole
overlay → render-nowhere holes (`SCENE_LIT_CLONE_COVERAGE_FIX_PLAN.md:11,17`); forward
alpha needs opaque static in world depth first or the UI fallback overwrites composited
hair (`SCENE_LIT_CLONE_BLEND_SLICE_BLUEPRINT.md:6`).

Preserve the cost distinction from `LLGhostAvatar` (a real `LLVOAvatar` with its own
drawable/geometry/appearance/attachments/idle-motion — `llghostavatar.cpp:77,110,482,937,998,1296`):
overlay adds only material draw submissions + small per-frame bookkeeping. No second
skeleton, attachment objects, geometry builds, animation controller, shadows, or LOD.

## 2. First slice — rigged forward-alpha parity (for otherwise fully-covered live clones)

Smallest slice that visibly fixes rigged head alpha (eyelashes/brows/hair, `PASS_ALPHA_RIGGED`),
rigged body alpha layers, and rigged PBR `ALPHA_MODE_BLEND` — using the real PBR/material
alpha shaders instead of `actorghostF.glsl` (which samples only `diffuseMap` × flat color,
`actorghostF.glsl:48,108,156`). Current bespoke sweep: `llactormover.cpp:4484,4488`.

1. **Extend the phase dispatcher, not `drawGeometryGhost()`.** Add a rigged-blend forward
   stage alongside `renderGhostPostDeferred()`, consuming the same validated
   `GhostProxyQueue`; keep its world/probe/HUD/frame/view guards (`pipeline.cpp:5566,5574,5581`).
2. **Select shaders from each `LLDrawInfo`, not just `mPass`:** GLTF `ALPHA_MODE_BLEND` →
   rigged `gDeferredPBRAlphaProgram`; legacy material → stored `mShaderMask` permutation of
   `gDeferredMaterialProgram`; plain lit alpha → rigged `gDeferredAlphaProgram`; fullbright
   alpha → rigged fullbright-alpha (`SCENE_LIT_CLONE_BLEND_SLICE_BLUEPRINT.md:21,26`;
   collector already includes legacy blend variants + `PASS_ALPHA_RIGGED` at
   `llactormover.cpp:5179,5182,5194`).
3. **Reuse stock alpha preparation** (don't duplicate gamma/water/min-alpha/deferred-env/
   sun-shadow/probe setup). Per batch: upload palette; apply `ScopedGhostTransform`; bind
   the real material AFTER binding its shader; reproduce texture-matrix setup WITHOUT
   `applyModelMatrix()`; use the batch's RGB blend factors + stock alpha factors; draw with
   depth test on, depth writes off; restore captured blend + GL state
   (`SCENE_LIT_CLONE_BLEND_SLICE_BLUEPRINT.md:33,37,43`).
4. **Gate conservatively:** attempt rigged blend only when `RIGGED_SOLID` is covered AND
   (no `STATIC_SOLID` present OR it is already covered) — overlay derives present mask
   per-category (`llactormover.cpp:5830,5851`); avoids UI-phase static fallback burying
   forward-alpha (`SCENE_LIT_CLONE_BLEND_SLICE_BLUEPRINT.md:61`).
5. **Coverage commits only after complete success:** add `RIGGED_BLEND` progress
   (required/drawn/failed); set `GHOST_COVERAGE_RIGGED_BLEND` only if EVERY eligible blend
   batch classified+validated+palette-uploaded+drawn; else leave clear and let the existing
   overlay draw the whole blend category (monotonic all-success, like solid at
   `pipeline.cpp:5219,5271,5482`). Category-granular invariant: `llghostcoverage.h:6,15`;
   overlay already computes `present & ~covered` (`llactormover.cpp:5885,5896`).

**Acceptance gate:** head/body/PBR-blend match an Entity reference in color/alpha/lighting;
palette/material/shader failure → `RIGGED_BLEND` uncovered + exactly one fallback draw;
success suppresses exactly the bespoke rigged-blend sweep (no double-alpha); existing
G-buffer contamination test unchanged (`llghostdeferreddiagnostics.h:61`); the forward
stage gets its own `LLScopedGhostRenderInvariant` (`llghostdeferreddiagnostics.h:9`);
gate-off = GL no-op (`pipeline.cpp:5142,5147`).

**Defer in slice 1:** static attachments, cross-water clones, global alpha ordering,
legacy glow, PBR glow, frozen-pose replay (bits stay unset → fallback stays hole-safe).

## 3. Hazards (all MUST-GET-RIGHT)

- **Alpha blend is forward-only:** never in the G-buffer; after light apply, depth test on
  / writes off / authored per-batch blend factors (`pipeline.cpp:14074`). Sort instances
  far-to-near by transformed clone bounds, source order within an instance
  (`llactormover.cpp:5873`). Global interleave with world glass/particles stays approximate.
- **Emissive/glow separate ownership:** PBR emissive = base pass + duplicate additive glow;
  classifier puts only `PASS_GLTF_GLOW_RIGGED` in `RIGGED_GLOW` (`llactormover.cpp:4620,4635`).
  Blend parity must not also submit glow; glow must not suppress the base; set `RIGGED_GLOW`
  only after all additive draws succeed; restore `BT_ALPHA` after additive
  (`llactormover.cpp:4498`). Legacy glow not yet harvested (`llactormover.cpp:4624,4630`) →
  separate later slice, not silently "covered."
- **Indexed batches stay indexed:** don't reuse the bespoke per-slot `ghostSlot` redraw
  (`llactormover.cpp:3620`, `actorghostF.glsl:56`); use the real indexed shader/material
  arrays (`pipeline.cpp:4852,4863,4875,4890`), bounded by `sIndexedGLTFChannels`
  (`pipeline.cpp:4824`). Over-limit/null-gap/shader-unavailable/unsupported indexed-alpha →
  fail that category's coverage, don't fall through to scalar.
- **Color-space/factors/UV:** keep `LLFetchedGLTFMaterial::bind()` (`pipeline.cpp:4698`);
  scope `GL_FRAMEBUFFER_SRGB` only where the stock path does (`pipeline.cpp:5429`); do NOT
  CPU-gamma `mBaseColor`, reconstruct transforms, or hand-pick emissive/base maps (the
  bespoke failure mode). Legacy batches keep `mTextureMatrix` (`pipeline.cpp:4798`).
- **Alpha mode + double-sided from resolved material:** pass classifies phase; validate
  actual GLTF alpha mode + material presence before drawing. Scalar PBR disables culling for
  `mDoubleSided` (`pipeline.cpp:4710`); indexed disables for the whole draw if any slot is
  double-sided (`pipeline.cpp:4839,4915`) — preserve stock behavior. Mask cutoff is
  material/slot-specific (`pipeline.cpp:4855`).
- **Cost budgeted by draws, not scene objects:** allowed = +1 draw per harvested
  batch/category, material/texture bind, palette upload, sort + coverage counters. Rejected
  = new `LLVOAvatar`, duplicated attachments/drawables, independent geometry/LOD/motion/
  texanim/physics, shadow replay, per-slot scalar redraw, rebuilding/deep-copying
  `LLDrawInfo`. Collector walks existing spatial-group draw maps, retains frame-local
  pointers (`llactormover.cpp:5405,5415`).

## 4. Phased plan

1. **Phase 1 — rigged forward-alpha vertical slice** (the first slice above).
2. **Phase 2 — static opaque/masked world submission** (real non-rigged shader + per-face
   render matrix, before forward alpha; bind from live face/draw info, not just
   `face->getTexture()` — `CLONE_FIDELITY_AUDIT_BLUEPRINT.md:67`).
3. **Phase 3 — broaden rigged blend** (drop the "no uncovered static solid" gate once P2
   supplies world depth; all legacy blend permutations, plain/fullbright alpha, scalar PBR
   alpha, proven indexed-alpha).
4. **Phase 4 — static blend** (same forward-alpha framework, non-rigged shaders; commit
   `STATIC_BLEND` independently).
5. **Phase 5 — PBR emissive + GLTF glow** (base in solid/blend phase, then replay
   `PASS_GLTF_GLOW_RIGGED` via real emissive/glow; `pipeline.cpp:4821,4863,4895`).
6. **Phase 6 — remaining solid/static + perf gate** (profile Overlay vs Entity at crowd
   counts; require materially lower CPU + frame time; no category uses `actorghostF.glsl`
   for clone color).

**Defer:** legacy `PASS_GLOW_RIGGED` bloom, unified world-alpha sort, intersecting
translucent-clone ordering, water-straddle dual-clip, shadow/probe participation,
frozen-pose scene-lit replay (`llactormover.cpp:5573`), system-avatar body
(`llactormover.cpp:3830`), stylized shaders.

## 5. Starting seams
- `pipeline.cpp:4967` shared batch-phase classification · `:5142` opaque/masked G-buffer
  replay · `:5566` staged post-deferred dispatcher · `:14074` post-light forward timing ·
  `:4629` ghost-safe submission (no source model matrix) · `:4690` scalar PBR bind ·
  `:4811` indexed PBR bind.
- `llactormover.cpp:5172` harvested rigged-pass domain · `:5881` per-instance coverage
  suppression · `llghostcoverage.h:30` five-category ownership.
