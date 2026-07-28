# Ghost Clone — Unified Spawn Engine: Deep-Research & Code-Generation Brief

**Audience:** GPT / Codex (research + code generation).
**Repo:** `I:\alchemy-machinima`, branch `develop`.
**Deliverable of THIS task:** (1) a deep design analysis and (2) *generated, reviewable code* for a unified clone spawn engine.
**HARD SCOPE LIMIT:** **DO NOT BUILD, LINK, OR DEPLOY.** Produce code + design as artifacts/patches for later human+Claude review and build. No `msbuild`, no viewer launch, no in-world test. Claude owns the build/link step separately.

> Terminology note: "GPT" and "Codex" mean the researching agent. "Claude" is the build/verify owner. "Overlay clone" and "entity clone" are the two existing clone mechanisms defined below.

---

## 0. One-paragraph problem statement

Ghost Studio currently has **two structurally opposite clone mechanisms** — an *overlay* clone (a live mirror of the source's GPU render data) and an *entity* clone (owned client-only objects) — plus an orthogonal *deferred* render mode. The overlay inherits the source's fidelity for free but cannot persist or be independently controlled; the entity clone can be a first-class Director actor but must **re-derive** skin, materials, attachments, animesh, and baked textures from scratch — and that re-derivation is where essentially **every clone bug** lives (this session alone: HUD attachment leakage, animesh control-avatar ordering, Bakes-on-Mesh invisibility). The goal is to research and generate code for a **unified spawn engine**: one front-end + one shared-subsystem layer, dispatching to the right backend by capability, so the two paths stop diverging and the bug class collapses. The two *backends* are NOT being merged away — they are endpoints of a spectrum and both are load-bearing. What is being merged is the **entry point, the shared resolution logic, and the lifecycle**.

---

## 1. The two orthogonal axes (do not conflate them)

Design conversations keep collapsing these; keep them separate throughout:

- **Axis A — Spawn / data model:** *Overlay* (reference the source's live render artifacts) vs *Entity* (own independent client-only objects).
- **Axis B — Render path:** *Forward* (default) vs *Deferred* (`GhostDeferredEnable`, G-buffer / scene-lit).

Axis B is a **rendering flag** that should compose on top of *either* backend. It is not a third kind of clone. (Known, pre-documented limitation: the deferred debug path does not apply to the overlay clone for PBR content — treat as a constraint, not a bug.)

---

## 2. Current architecture (verify exact signatures/lines against the tree)

### 2.1 Overlay clone — `LLActorMover` (fidelity-by-reference; a live mirror)
- `GhostBatch { LLDrawInfo* mInfo; U32 mPass; }` — frame-lifetime pointers into the pipeline's live `PASS_*_RIGGED` draw maps.
- Harvest: iterate those maps, keep entries with non-null `di->mAvatar` and `di->mSkinInfo` (both stamped by the **source's** own successful rebuild). A separate `GhostStaticFace` path captures legitimately-static worn faces that never enter the rigged pass maps.
- Skinning: per batch, `LLRenderPass::uploadMatrixPalette(di->mAvatar, di->mSkinInfo, ...)` using the source avatar's **live palette** (already baked to world space). Placement is a world-space transform `T(foot)·R·S·T(-pivot)` premultiplied into the modelview — no second skeleton, no per-vertex work.
- **Consequence:** inherits source fidelity for free (skin, materials, BoM textures, HUD exclusion, animesh via the source's control avatar). BUT it is chained to a live, currently-drawn source; it cannot persist, be posed independently, or outlive the source, and it breaks when the source is not drawn as live rigged batches (impostor/cull/not-rendered).

### 2.2 Entity clone — `LLGhostAvatar` (independence-by-copy; an owned actor)
- Built by `copy_prim_state` (per prim) + `cloneAttachmentsFrom` (per linkset), orchestrated by `ALGhostStudio`.
- Must **re-derive** everything on new client-only `LLVOVolume`s: sculpt/mesh params (installed before `setVolume` so `isSculpted()`/skin acquisition fires), skin (installed via `notifySkinInfoLoaded` with a mesh-UUID coherence check), TE/material/GLTF copy, baked-texture ("magic id") bindings (`changeTEImage`, later re-resolved by `refreshBakeTexture`), animated-object identity + control-avatar (`finalize_control_avatar`), the outer render transform (`LLClientOuterTransform`, `stampEntityOuterTransform`) for uniform scale, and foot-lock placement (`setGhostFootPosition` / `applyDesiredGhostFootPosition`).
- **Transactional spawn:** `cloneAttachmentsFrom` counts every live *world* root into `mCloneExpectedRoots` **before** the type check; `clonedAttachmentsComplete()` compares expected vs recorded; a shortfall makes `createEntityRuntime` roll the whole ghost back. (This is why filters like HUD-exclusion must happen at the attachment-point level *before* the count — see §4.)
- **Consequence:** persistent, manipulable, poseable, can outlive the source, can be a Director actor. Price: it reconstructs source state and every reconstruction gap is a bug.

### 2.3 Orchestration — `ALGhostStudio`
- Owns the instance model (`kind == BACKING_ENTITY_CLONE`, per-instance state), `resolveEntityClone`, `spawnEntityClone`, crowd/duplicate/refresh flows (`commitCrowdPlacement`, `duplicateGroup`, `refreshEntityClone` → `createEntityRuntime`), pose cadence + matrix-palette bookkeeping for the overlay, and source resolution via `LLDirectorCast::resolve` (which upcasts `resolveEntityClone` results — a clone-of-clone hazard worth designing out).
- This is the natural home for the unified front-end.

### 2.4 Render path — `GhostDeferredEnable`
- Routes clone geometry through the deferred G-buffer (scene-lit) pass; default off, forward otherwise. Orthogonal render flag (Axis B).

### 2.5 Director / actor system
- Treats ghosts as **actors** (waypoint pathing, look-at, pose, animation speed, formations). This REQUIRES the owned/entity model. Any unification must preserve this path unchanged in behavior.

### 2.6 Instrumentation already in the tree (reuse it, do not reinvent)
`GHOSTRIG` (per-prim clone-vs-source rig/mesh/skin/animesh/cav + mesh UUIDs), `GHOSTCAV` (control-avatar finalization), `GHOSTTEX` (per-face texture/BoM/alpha/pool/draw-info/visibility), `GHOSTVERIFY` + `RECHECK` (structural + rigged-face acceptance harness; strict attachment-point decode; `structure_degraded`), `GHOSTMATCOPY`, `GHOSTLOD`.

---

## 3. Evidence: the divergence-bug taxonomy (why unification pays off)

Every entity-clone defect this session was the entity path **re-deriving** something the overlay/source already had correct:

| Symptom | Overlay | Entity | Root cause (entity re-derivation) | Status |
|---|---|---|---|---|
| Rigged skin transfer | correct (inherits) | **correct** | `copy_prim_state` skin install — proven `clone_hasSkin == src_hasSkin` byte-for-byte | Proven OK |
| "Floating" pieces (Molly) | fine | broken | Self-clone iterated **HUD attachment points**; ghost has no HUD joints → fell back to chest joint → HUD prims float as junk | **Fixed** (skip HUD points before root count) |
| Animesh detach | fine (source cav) | broken | Control-avatar finalized in wrong order (attach before animated param → wrong bridge) | **Fixed** (`finalize_control_avatar`) |
| Invisible body (Roddra) | fine | broken | **Bakes-on-Mesh / alpha** re-resolution on the clone (forward path); `refreshBakeTexture` re-resolves baked "magic ids" against the ghost's own bakes and can clobber the copied source binding; deferred path renders her correctly → binding is fine, forward render/material handling is not | **Open** |

**Thesis:** the overlay is right because it *reuses the source's already-resolved state*. The entity path is wrong wherever it *re-resolves* that state independently. **Sharing the resolution is the fix for the whole class.** (Roddra's invisible body is the live proof: it is literally "entity re-derives baked textures and gets it wrong; overlay uses the source's resolved textures and gets it right.")

**Regression provenance (bisect baseline):** the crowd recode (diff `backup-pre-crowd-recode..backup-pre-crowd-complete`, both committed snapshot branches) changed **only `alghoststudio.cpp`** orchestration — the attach/skin/material code in `llghostavatar.cpp` is bit-identical across it. The weekend "regression" was orchestration exposing latent entity-path input-domain gaps (e.g. self-cloning became the default, exercising HUD iteration). This strongly supports "unify the front-end + share the resolution helpers; the backends themselves are sound."

---

## 4. Hard invariants the unified engine MUST preserve

1. **Transactional spawn semantics:** expected-root counting happens BEFORE per-object type/capability filtering; any capability filter (HUD exclusion, etc.) must be applied at the source-enumeration level so `clonedAttachmentsComplete()` stays meaningful. A filter placed after the count silently rolls back whole clones.
2. **Proven skin path untouched:** do not "improve" `copy_prim_state`'s skin install; it is verified correct.
3. **Director actor path unchanged:** owned entity clones must remain fully poseable/path-animatable.
4. **Client-only / no sim sends:** every clone operation is local; never emit `ObjectUpdate`/`ObjectExtraParams` (`local_origin=false`, `mIsLocalOnly`).
5. **HUDs never cloned into world geometry** (already true after the recent fix — keep it centralized in the shared enumeration filter).
6. **Existing acceptance harness stays valid:** `GHOSTVERIFY`/`RECHECK` must still pass; `expect_rigged` derives from the **source** prim's skin.

---

## 5. Proposed unified design (research + refine this; then generate code)

### 5.1 Shape
```
                       ┌─────────────────────────────┐
   GhostCloneRequest → │   Unified Spawn Engine      │  (front-end; lives in ALGhostStudio)
   (capability spec)   │   - resolve source          │
                       │   - choose backend          │
                       │   - drive lifecycle         │
                       └───────────────┬─────────────┘
                                       │ uses
                       ┌───────────────▼─────────────┐
                       │   Shared Services layer      │  (the anti-divergence core)
                       │   - SourceResolver           │  (LLDirectorCast, reject clone-of-clone)
                       │   - AttachmentEnumerator      │  (world-only; HUD/temp filter; the count)
                       │   - MaterialResolver          │  (TE/GLTF/BoM/alpha; reuse SOURCE-resolved)
                       │   - SkinTransfer              │  (proven path; shared)
                       │   - Placement (foot-lock,     │
                       │     outer transform)          │
                       │   - GhostViz representation    │  (see doc §12 GhostViz)
                       └───────┬───────────────┬───────┘
                               │               │
                 ┌─────────────▼───┐   ┌───────▼──────────────┐
                 │ OverlayBackend   │   │ EntityBackend         │
                 │ (LLActorMover)   │   │ (LLGhostAvatar)       │
                 │ live mirror       │   │ owned actor           │
                 └──────────────────┘   └───────────────────────┘
                          (Axis B: forward | deferred render flag on either)
```

### 5.2 `GhostCloneRequest` — capability spec that drives backend choice
Fields the engine reasons over (research the exact set):
- `source` (avatar id / director cast), `must_reject_clone_of_clone` (default true).
- `persistence`: `Ephemeral` (dies with source visibility) | `Persistent` (owns data, outlives source).
- `controllable`: none | move/scale | full Director actor (pose, path, animation-speed, formations).
- `fidelity`: `MirrorExact` (accept source-coupling) | `IndependentSnapshot`.
- `render`: `Forward` | `Deferred` (Axis B).
- `count`/formation params (crowd).

### 5.3 Backend selection matrix (starting point — refine)
| Use case | persistence | controllable | → Backend | Render |
|---|---|---|---|---|
| Quick "show me a copy" / live preview | Ephemeral | none | **Overlay** | either |
| Crowd fill that just needs to look right, tracks source | Ephemeral | move | **Overlay** (cheap, high-fidelity) | forward |
| Director actor (posed, path-animated, outlives source) | Persistent | full | **Entity** | either |
| "Lock this pose / bake this look" | Persistent | move+ | **Entity via promote** (§5.4) | either |
| Scene-lit hero clone | Persistent | full | **Entity** | **Deferred** |

### 5.4 The "promote / lock" path (key unification idea)
Default to the **overlay** (higher fidelity, far fewer edge cases). When the user needs persistence or manipulation, **promote the overlay into an entity clone by snapshotting the source's already-resolved state** (owned VB copy + material *values* + palette + copied mutable BoM textures — refs alone fail because VB/texture identity != immutability; see `doc/` scene-lit-clone notes). This directly reuses the overlay's correct resolution as the entity's input, instead of the entity re-deriving from raw source state. Research whether promote-from-live-overlay is cleaner than today's from-scratch entity build.

### 5.5 Shared `MaterialResolver` (the highest-value extraction)
Centralize TE + GLTF + **Bakes-on-Mesh magic-id + alpha** resolution so BOTH backends and the acceptance harness use one implementation, seeded from the **source's resolved bindings** (what the overlay proves correct) rather than re-resolving against the ghost's own (often-undefined) bakes. This is the concrete fix path for the open Roddra invisibility. Research: can the entity path bind the source's resolved textures and *suppress* `refreshBakeTexture` re-resolution (or make it unable to clobber a good binding with `IMG_DEFAULT`)? Does the forward vs deferred difference for BoM come from a pool/pass classification the resolver can normalize?

---

## 6. Research questions (the deep dive — answer these with file:line evidence)

1. **Ownership seams:** exactly which state does the overlay reference vs the entity own? Enumerate every field the entity re-derives and classify each as (a) safely shareable-from-source, (b) must-be-copied-for-independence, (c) must-be-recomputed. This table is the spec for the Shared Services layer.
2. **Promote path feasibility:** can an overlay be snapshotted into an entity in-place (freeze current palette + owned geometry + resolved materials) without the from-scratch re-derivation? What breaks (animesh, BoM, LOD, impostor sources)?
3. **Director binding:** what exactly does `LLActorMover` (pathing/pose) require of a clone object? Confirm the overlay genuinely cannot be a Director actor and the entity backend is mandatory for that use case.
4. **Source resolution hazards:** `LLDirectorCast::resolve` can return a ghost (clone-of-clone). Where should the engine reject/deduplicate that, and how should crowd/duplicate/refresh flows resolve prototypes safely?
5. **Transactional generalization:** how does the expected-root count + rollback generalize when the engine may choose different backends, filter capabilities (HUD/temp attachments), and support crowds? Preserve invariant §4.1.
6. **Render-flag orthogonality:** verify `GhostDeferredEnable` can be applied per-clone on either backend; capture the PBR/overlay limitation as an explicit capability gate, not a surprise.
7. **BoM/alpha forward-vs-deferred:** why does deferred render Roddra correctly while forward does not, given identical bindings (`GHOSTTEX`: `clone_face_tex==src_face_tex`, `gl=1`, `baked_default=0`)? Locate the pool/pass/alpha-mode divergence.
8. **Migration & regression surface:** the smallest sequence of refactors that lands the shared front-end + `MaterialResolver` without regressing the fixed HUD/animesh paths or the Director actor path. Identify what can be done behind a debug setting first.
9. **GhostViz alignment:** reconcile this engine with the `GHOST_STUDIO_TRANSFORM_ARCHITECTURE_DEEP_RESEARCH.md §12` unified "GhostViz" representation layer so they are one design, not two.

---

## 7. Code-generation scope (GENERATE, DO NOT BUILD)

Produce reviewable code (new files + patches) for:
1. `GhostCloneRequest` + a `GhostSpawnEngine` front-end in `ALGhostStudio` that resolves source, selects backend, and drives a common lifecycle (spawn / update / lock / despawn).
2. Backend adapter interfaces wrapping the **existing** `LLActorMover` (overlay) and `LLGhostAvatar` (entity) with minimal behavior change.
3. The **Shared Services** interfaces + first real extraction: pull the entity path's material/BoM/alpha resolution into a `MaterialResolver` seeded from source-resolved bindings; wire both backends + the `GHOSTVERIFY` harness to it.
4. The `AttachmentEnumerator` that centralizes world-only enumeration (HUD/temp filter) + the transactional count (preserving §4.1).
5. (Optional, if research supports it) a prototype `promote(overlay) → entity` snapshot path.

**For each generated unit:** cite the source functions it replaces/wraps, keep diffs minimal and reversible, guard risky behavior behind a debug setting, and add/extend `GHOST*` instrumentation rather than removing it. Mark everything clearly as **UNBUILT — pending Claude compile/link/in-world verification.**

---

## 8. Deliverable format

1. **Design report** answering §6 with file:line evidence, the ownership table (§6.1), the refined selection matrix, and the migration plan (§6.8).
2. **Generated code** per §7, as new files and unified diffs, each labeled UNBUILT and cross-referenced to the design report.
3. **Risk register:** what could regress (Director actor path, transactional spawn, HUD/animesh fixes, crowd flows) and the guard for each.
4. **Explicit statement** of what was NOT done: no build, no link, no deploy, no in-world test.

---

## 9. Guardrails (repeat — important)
- **NO BUILD / LINK / DEPLOY / IN-WORLD.** Code + analysis only.
- Do not modify the proven skin-copy path or the just-fixed HUD/animesh paths except to route them through shared helpers with identical behavior.
- Client-only; no sim sends.
- Preserve the transactional expected-root count invariant.
- Keep the Director actor (entity) path behaviorally intact.
- Prefer additive, debug-gated introduction over a big-bang rewrite.

---

## Appendix A — session evidence pointers
- Skin parity proof: `GHOSTRIG` — `clone_hasSkin==src_hasSkin` (584 skinned / 299 not; zero drops), `clone_mesh_uuid==src_mesh_uuid`, `src_is_ghost=0`.
- HUD root cause: log `"attachment point invalid: 31/32/33/34/36 … falling back to 1 (chest)"` on the degraded root UUIDs; HUD joints only built for self (`llvoavatar.cpp` HUD-attachment guard).
- Animesh fix: `finalize_control_avatar` (synchronous cav reclassification to `LLControlAVBridge` + `matchVolumeTransform`).
- Roddra (open): `GHOSTTEX` shows faithful bindings (`gl=1`, `baked_default=0`); `GhostDeferredEnable` renders her correctly → forward-path BoM/alpha handling, not a binding failure.
- Milestone branches: `backup-pre-crowd-recode`, `backup-pre-crowd-complete`. Recode touched only `alghoststudio.cpp`.
