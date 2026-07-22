# Scene-Lit Clone Injection — External Research Findings (condensed)

Source: OpenAI deep-research on doc/SCENE_LIT_CLONE_INJECTION_RESEARCH.md (2026-07). Terse on purpose.

## VERDICT: "Architecture A+" — a renderer-owned, FRAME-LOCAL GHOST-PROXY QUEUE
Pivot away from duplicated attachment entities is CONFIRMED correct. But do NOT do arbitrary
recursive re-entry of the whole deferred pipeline, and do NOT register clones in the world/spatial
DB. Instead: keep the source LLDrawInfo/VBs/materials/skin; give each clone a **render-only proxy**
(current/prev placement G, bounds, palette snapshots + cookies, pose epoch, batch refs, history bit,
cast flags); cull proxies independently; and **submit the harvested batches at the NORMAL pass
boundaries while those passes are already active** (a narrowly-scoped insertion, not a nested
`renderGeomDeferred`). Precedent: Unity `Graphics.RenderMesh`, Unreal `FPrimitiveSceneProxy`.

Frame order: buildGhostProxies → sun/spot shadows (augmented w/ ghost casters) → stateSort →
renderGeomDeferred(world) → **renderGhostDeferredOpaqueMasked** (BEFORE the deferred flush / Hi-Z /
SSAO / SSR / velocity / lighting) → flush → lighting → **renderGhostAlpha** → **renderGhostMotionVectors**.

## Corrections to the original "Architecture A" GIVENs
- **No recursive whole-pipeline re-entry.** No save/restore list makes it permanently safe;
  `generateImpostor()` is a containment WARNING, not a drop-in template.
- **One LLCullResult cannot encode multiple transforms** (it holds only a draw-info pointer). Pushing
  the same `LLDrawInfo*` repeatedly breaks pointer-based matrix caching. "One re-entry for all clones"
  = pass-setup-outside + clone-loop-inside (ScopedGhostTransform + ScopedGhostPalette + existing
  pushers), OR a new packet type, OR GPU instancing.
- **Face pools ignore the render map** (`LLDrawPoolAvatar` via `mDrawFace`) → render an exact
  **whitelist** of render-map-backed passes, NOT a blacklist (a future pool = accidental whole-world
  draw at G).
- **`gGLLastMatrix` must be invalidated between EACH clone placement**, not just around the pass.
- **Source-derived LOD/texture residency is wrong for the clone's distance** (far source → low-LOD
  geometry on a near clone). Studio must compute effective projected size across source+clones, force
  the LOD floor + texture priority, stamp a source generation and reject stale packets.

## Q2 Motion vectors / TAA
- A frozen STATIONARY clone must NOT emit zero velocity (camera still moves it on screen).
  `velocity = curScreen - prevScreen`, `cur = P_unjit·V·G_cur·Skin(palette_cur)`,
  `prev = P_unjit·V_prev·G_prev·Skin(palette_prev)`. Use UNJITTERED projection.
- Live pose → use the **previous palette** (G_prev alone smears limbs). New/teleport/pose-swap/source-
  swap → mark **history invalid** (stencil/reactive-mask; NOT just prev=cur). Never reuse the source's
  final velocity pixels — reuse its previous PALETTE. Defer transparent MVs until opaque+masked correct.

## Q3 Alpha sorting
- A hand-built render map does NOT enter the real blended-alpha path (`renderAlpha` reads
  `group->mDrawMap[PASS_ALPHA*]`). Use a **dedicated sorted ghost alpha queue** (GhostAlphaPacket:
  transformed depth key, pre/post-water, rigged, source render order). Never modify source `mDistance`.
- Do NOT convert BLEND→MASK wholesale (kills hair/eyelashes/lace/veil/glass/fur). Selective promotion
  only for near-binary textures (~98–99% of alpha below 0.05 or above 0.95, validated on a corpus).

## Q4 Re-entrancy safety
Scope/invariant-check a large state set: pipeline flags (sUseOcclusion, sShadowRender, sImpostorRender,
sReflectionRender, sRenderDeferred, sNoAlpha, RenderSpotLight, camera/mirror/probe mode…), all
matrices (+ gGLLastMatrix per-placement), FBO/viewport/scissor/depth-range/sRGB, depth/stencil/blend/
raster, program+VAO+buffer+texture+UBO bindings, occlusion/timer queries, avatar dedup
(lastAvatar/lastSkin/lastShader/skipLastSkin/sSkipOpaque…), resource pin/generation. Do NOT
flush/resolve/switch targets inside the insertion. CPU-frustum-cull ghosts; don't occlusion-test them;
draw AFTER world opaque traversal, BEFORE deferred flush.

## Q5 Shadows
`view[cascade]·G` is correct BUT ghost bounds MUST participate in **cascade fitting** (else clipped /
assigned to no cascade). An off-camera clone can cast into the frustum. Inject into sun cascades AND
the active spotlight/projector shadow maps (RenderSpotLight). Don't scale bias by clone scale blindly.
Reproduce mask cutoff / GLTF mask / double-sided / frozen palette / uniform scale in the caster pass.
**Receive-only is a valid intermediate phase** (only missing cue = contact/ground shadows).

## Q6 Frozen palette
Central hook = `LLRenderPass::uploadMatrixPalette` (not per-pool patching). `(avatar-id, skin-hash)` is
NOT a sufficient key — two clones of the same avatar+skin with different frozen poses need different
palettes → key MUST include a palette/pose cookie. **Disable palette-upload dedup while an override is
active** (correctness-first). Use a **scoped/thread-local resolver** (ScopedGhostPaletteContext), never
a process-wide static map. Never write into the source's `mMatrixPaletteCache`.

## Q7 Pivot vs hybrid
Abandon duplicated attachment entities (confirmed). Do NOT default to an "entity body + re-issued
attachments" hybrid. FIRST test **direct re-render of the source SYSTEM BODY** via the existing
single-avatar path: `LLDrawPoolAvatar::renderAvatars(LLVOAvatar* single_avatar)` → `renderSkinned()`
under G (no second LLVOAvatar). Body-entity hybrid only as a constrained fallback (two halves diverge:
pose/history/LOD/shadow + neck/wrist/ankle seams). Frozen body may need a body-specific palette
snapshot or a frozen pre-skinned buffer.

## Q8 Amortization
First: pass-outer/clone-inner CPU batching (begin pass once, upload same-pose palette once, loop
placements, invalidate matrix cache each). Then same-pose GPU instancing (per-instance G_cur/G_prev;
CPU→O(source batches), GPU→O(N); needs per-instance/clustered bounds + per-cascade instance lists).
Blended alpha usually can't instance (ordering). Optional pre-skin-once + rigid-instance-many after
profiling.

## Recommended build sequence
P0 diagnostics/invariants (counters, cookies, G-buffer diff, velocity viz) → P1 frame-local proxy:
live opaque+masked, whitelist passes, no shadows/alpha, frustum-cull → P2 temporal (G_prev, prev
palette, history reject) → P3 frozen palette (scoped resolver, dedup off) → P4 shadows (cascade fitting
+ spotlights, opaque/mask casters) → P5 blended alpha queue → P6 system body (single-avatar reissue) →
P7 batching/instancing.

## Top ranked risks (each has a falsifiable test in the full report)
1. Global-state contamination (catastrophic) — G-buffer diff test, Studio off vs on must match OUTSIDE
   clone bounds. 2. Temporal/velocity. 3. Palette dedup/lifetime (two frozen poses, same avatar/skin).
4. Source LOD/residency. 5. Blended alpha + water staging. 6. Shadow coverage/bias. 7. Mirror/probe
view coverage. 8. System-body seam. 9. Perf (after correctness; test 1/4/16/64 same-pose clones).
