# Clone workload cost + measurement plan

Research pass 2026-07-25, verified against `develop` @ `a6744226a61`. Companion to
`HARDWARE_OPTIMIZATION_FINDINGS.md` (GPU and CPU). Nothing here was profiled — magnitudes are
inferred, mechanisms are verified.

---

## P1 — `getClonedSourceLOD` is the only super-linear term

`llghostavatar.cpp:2108`, called from `LLVOVolume::calcLOD()` (`llvovolume.cpp:1622`) for **every**
local-only prim whose LOD is re-evaluated. Each call **allocates a `std::vector<LLUUID>`**, copies the
test-harness list, appends every studio instance id, then per ghost does `findObject` +
`dynamic_cast`, linearly scans that ghost's linksets and every child UUID, then a second `findObject`
+ `dynamic_cast`.

At 50 clones × ~25 prims ≈ 1250 local prims, one LOD sweep ≈ **60k hash lookups, 60k dynamic_casts,
>1M UUID compares, 1250 vector allocations.** It fires whenever `LLSpatialBridge::updateDistance`
runs, gated by `LLSpatialGroup::changeLOD()`, which returns true immediately on `OBJECT_DIRTY` — i.e.
near-constantly for animating clones. The bridge's impostor early-out can never help: ghosts are
never impostors.

**Fix:** invert it. Keep an `unordered_map<clone_prim_uuid, source_prim_uuid>` maintained by
`cloneAttachmentsFrom` / `releaseClonedAttachments`. O(1), zero allocation, identical semantics.
Small effort, low risk.

**Diagnostic shortcut:** everything else in the clone path is linear in clone count, so the *shape* of
a 1/10/25/50 sweep answers this immediately — **convex ⇒ P1 dominates; straight line ⇒ the fixed
per-clone walks do.**

## P2 — The extent throttle is blind to clones

`llvoavatar.cpp:2817` derives `upd_freq` from `gObjectList.getAvatarCount()`, but `mNumAvatars`
**explicitly excludes ghosts** (`llviewerobjectlist.cpp:971-974`). With 1 real avatar + 50 clones,
`upd_freq` stays at **4**, so ~12.5 full `calculateSpatialExtents()` run per frame instead of the
~1.25 the throttle was designed to permit — **10× the intended budget**. Each call walks all polymesh
joint render data, every attachment root, and a fixed 216-joint loop with a `dynamic_cast` per rigged
joint, plus `updateRiggingInfo()`.

**Fix:** feed ghost count into `upd_freq`, or give clones a longer dedicated period — they move slowly
or not at all, and the cheap "shift by pelvis delta" path already handles translation between full
recomputes. Risk: too long a period could pop a fast-moving clone's bounding box.

## P3 — Physics drags a 705-node tree walk

Verified chain: `MIN_REQUIRED_PIXEL_AREA_AVATAR_PHYSICS_MOTION = 0.f` (`llphysicsmotion.cpp:46`)
defeats the LOD cull (`llmotioncontroller.cpp:649`) → `onUpdate` runs 6 motions → any change calls
`updateVisualParams()` → a **705-node `std::map` pointer-chasing walk** (`llcharacter.cpp:471-488`;
exactly 705 `<param>` entries counted in `avatar_lad.xml`) → the ~8 changed driven params each run
`LLPolyMorphTarget::apply`. `LLVOAvatar::updateVisualParams` additionally opens with a *string-keyed*
`getVisualParamWeight("male")` on every call. Physics changes essentially every frame while active,
so this is per-clone per-frame.

**Fixes, cheapest first:** (1) restore a nonzero min-pixel-area for ghosts only — no visible change
for small/distant clones; (2) default physics off for static set-dressing clones (the plumbing and a
clean `neutralizeEntityPhysicsParams` already exist); (3) the real fix — have physics do a **targeted**
update over just the driven param set instead of the full walk, which benefits real avatars too.

## P4-P8 — Confirmed waste

- **Map deep-copy per mirrored clone per frame** (`llghostavatar.cpp:1278`) — full `std::map` copy,
  two erases, full compare, discarded unless changed. Fix: diff in place, materialize only on change.
- **Empty-map insertions into a process-wide global** (`llghostavatar.cpp:1344`) — `operator[]` on
  `LLObjectSignaledAnimationMap` permanently inserts empty entries per cloned prim, taxing non-clone
  code that also reads it. Fix: `find()` first; `erase` when the source empties.
- **Resident-only work not ghost-guarded** — `getVoiceEnabled()` on a synthetic UUID that can never be
  a voice participant; `idleCalcNameTagPosition` whose result is discarded (name tags early-out for
  ghosts at 3461); `idleUpdateRenderComplexity` every 20 frames, which can post a real
  `profileAvatar` render pass for complexity nobody reads; `refreshLifecycleStates` doing
  `findObject` + `dynamic_cast` + a by-value `getFullname()` per clone per frame;
  `cullAvatarsByPixelArea` sorting a `std::list` that clones inflate.
- **UI signature strings** — the panel builds ~300 temporary strings and multi-KB ropes per frame,
  three times over, purely to decide "nothing changed". Replace with a 64-bit hash over `mId.mData`
  plus the existing `mTransformRevision`. (`refreshAnimationLibrary` was already fixed this way and
  documents the microstutter that motivated it.)
- **Per-frame map clear/rebuild** in `collectGhostBatches` — destroys and reallocates every vector
  each frame; `.clear()` on the vectors retains capacity instead.

## Clone LOD tiers — principled, but not the viewer's existing ones

Impostoring must stay off (a stale 2D snapshot is exactly the artifact machinima cannot tolerate), and
clones must never be demoted on complexity, ART or visibility rank. But **true frustum visibility is a
much narrower condition than "exists"**, and `updateCharacter` already routes invisible non-self
avatars to `updateMotions(HIDDEN_UPDATE)` (`llvoavatar.cpp:5138`). What off-screen clones do *not*
skip is everything around motions.

| Tier | Condition | Behaviour |
|---|---|---|
| **T0 Hero** | on-screen AND (selected OR large OR pinned) | unchanged |
| **T1 On camera** | `isVisible()` | skip voice, name-tag position, render complexity; throttle lifecycle refresh |
| **T2 Off camera, live** | `!isVisible()`, not frozen | additionally skip gaze, head offset, world-matrix children, physics, extents, LOD lookup |
| **T3 Frozen** | `DRIVE_FROZEN` | also skip palette rebuild and extent recompute; placement change forces one-frame promotion |

**Two verified hazards.** (1) `updateMotionsMinimal` does **not** advance `mAnimTime`
(`llmotioncontroller.cpp:943-956`), so hiding some clones will **desync a chorus line** — any tier
using `HIDDEN_UPDATE` needs a per-instance "keep in sync" opt-out. (2) A clone outside the *view*
frustum can still be inside a *shadow* cascade, so T2 must keep pose updates (which cast) and drop
only non-pose work.

**Frozen clones already do well:** the held pause request keeps `LLMotionController` on the
`updateIdleActiveMotions()` branch — no pose blend, no `onUpdate`. Residual cost still paid:
`purgeExcessMotions`, two 216-byte signature memsets, the full `updateCharacter` prologue, extents,
`getClonedSourceLOD`, the whole `idleUpdate` tail — and **physics, which the pause path may not stop
(verify in-world).** The largest safe saving for frozen clones: a skeleton-generation counter so
`updateSkinInfoMatrixPalette` (which currently keys on `gFrameCount`) can skip the CPU rebuild when
the skeleton hasn't moved. That helps stationary live clones between motion updates too.

## Where the ceiling probably actually is

Palette uploads happen per (avatar, skin) **per pass** (`lldrawpool.cpp:908`; the velocity variant
uploads *two*). At 50 clones × ~8 skins × ~6 passes × 110 joints × 12 floats ≈ **12 MB of uniform
data and ~2400 `glUniformMatrix3x4fv` calls per frame** before any actual draw calls. That reads as
"driver time" and is invisible without GL timer queries. **Inferred** from verified constants — count
the real pass total in a capture.

---

# Measurement plan

## What already exists (this changes the plan substantially)

- **Tracy is ALREADY compiled into the shipped Release exe** — `USE_TRACY=ON`, **on-demand** (records
  nothing until a profiler connects), localhost-only. **No rebuild is needed to start profiling.**
- **`USE_TRACY_GPU` is OFF — but 88 `LL_PROFILE_GPU_ZONE`s already annotate the pipeline**, and the
  GPU context/collect calls are already wired into `swapBuffers` (`llwindowwin32.cpp:1893, 2106,
  3934`). **One CMake flag turns all 88 on.** Highest-leverage measurement action available, zero
  source changes.
- **Allocation churn is already fully instrumented** — the global `operator new`/`delete` overrides
  feed Tracy's Memory view (`llcommon.cpp:40-60`). The waste items above will appear by name with no
  code at all.
- **Zero `LL_PROFILE_*` zones exist in the fork's own hot files** — `llghostavatar.cpp` (90 KB),
  `alghoststudio.cpp` (55 KB), `llactormover.cpp` (240 KB) all grep to **0**. Every clone-specific
  cost is currently invisible, buried as unattributed self-time. **`LLVOAvatar::updateCharacter` has
  no zone either** (`llvoavatar.cpp:5047`) — the hottest avatar function is unattributed.
- Reusable precedent: `bdmergetexspike.{h,cpp}` is the exact log-histogram template that produced the
  781 ms → 46 ms cache win.
- Two config notes: `LL_PROFILER_CONFIGURATION=3` **stringifies zone names** (`llprofiler.h:142` uses
  `#name`) so zones render with literal quotes — cosmetic, but proof nobody has yet read a Tracy
  capture of this build; and config 3 keeps LLTrace fast timers alive alongside Tracy, so build a
  `=2` variant for clean CPU numbers. Tracy client is **0.13.1** — the GUI must match the protocol.

## Measure FIRST: two zones, one flag, one plot

Before writing any optimization code:
1. Build with **`USE_TRACY_GPU=ON`** (no source changes).
2. Add exactly two zones — `updateCharacter` (`llvoavatar.cpp:5047`) and `getClonedSourceLOD`
   (`llghostavatar.cpp:2108`) — plus a `ghost/clones` plot.
3. Capture the 0 / 10 / 50 clone points.

**Decision tree — the outcome selects which work list is even relevant:**
- **`swapBuffers` fat, GPU zones nearly fill the frame** → GPU-bound. Stop; none of the CPU findings
  will move fps. Work the GPU list instead.
- **`df Display` fat but GPU zones short** (large CPU↔GPU gap) → submission/driver-bound. Priority
  becomes palette-upload consolidation (one upload per (avatar, skin) per *frame*, or an SSBO of all
  palettes). Tiering will not help on-camera clones.
- **`df idle` fat, `updateCharacter` self-time dominant** → idle-update-bound. Execute P1, P2, P3,
  then the tier scheme.
- **`getClonedSourceLOD` visibly slices, or grows super-linearly 10→50** → P1 confirmed dominant. Fix
  it first; it changes the shape of every other number.

**Free confirmatory A/B, no code:** at 50 clones toggle `AvatarPhysics` 1 → 0 and record the frame-time
delta. That single number is the entire P3 budget, in 30 seconds.

## Zones and plots worth adding (after the first capture)

Add a `LL_PROFILE_PLOT` macro (none exists — no `TracyPlot` anywhere in the tree) and a
`LL_PROFILER_CATEGORY_ENABLE_GHOST` switch following the existing pattern in
`llprofilercategories.h`. Then zone: `LLGhostAvatar::idleUpdate` (+ linkset count), the mirror-anims
block, the object-anims loop, `updateVisualParams`, `ALGhostStudio::updatePerFrame` and its
sub-passes, `applyGaze`, `collectGhostBatches`/`renderStudioGhosts`, the panel's `draw()` refreshers,
and `idleCalcNameTagPosition`. **Also change** `llpolymorph.cpp:552` from a bare
`LL_PROFILE_ZONE_SCOPED` to the AVATAR category — at 50 clones that uncategorized zone alone can
exhaust Tracy's memory.

Plots, once per frame: clone count, visible count, frozen count, cloned prim count, plus counters for
palette rebuilds, palette uploads, morph applies, extent recalcs and LOD lookups. **Those five
counters are the entire cost hypothesis expressed as measurable quantities** — overlay them on frame
time and the correlation answers it directly.

## Capture protocol

Fixed private/empty region (a live public region injects uncontrolled load and destroys
comparability) · one fixed source avatar and outfit (the largest hidden variable) · **camera locked
via the Flycam Recorder** so runs are byte-comparable · fixed environment preset, no day cycle ·
fixed clone layout via `makeArray`. Sweep **0 / 1 / 10 / 25 / 50** — the 0-clone baseline is not
optional — 30 s capture after a 15 s settle, each point run **twice non-adjacently** to catch thermal
drift and cache warming. Record the settings snapshot, driver version, build timestamp and git SHA
with every capture. Export with `tracy-csvexport.exe` and diff CSVs across clone counts: that is what
turns "the flame graph looks different" into "zone X grew 12× while clones grew 5×".

## Detecting specific pathologies
- **Texture thrashing:** watch the ratio of the existing `"iglgt - pool miss"` to `"iglgt - reup
  pool"` zones (`llimagegl.cpp:1360, 1374`); extend `BDMergeTexSpike::recordUpload` with a per-UUID
  upload counter (it already tracks repeat *decodes*).
- **Cross-CCD migration:** Tracy's CPU-data view with context switches enabled (requires running as
  Administrator for ETW). Threads are already named — watch the "App" thread hop between core IDs
  0-15 and 16-31. On-demand mode does **not** capture context switches retroactively; connect first.
- **Main-thread stalls on a pool:** the zones already exist (`"df gMeshRepo"`, `"df getTextureCache"`,
  `"Image Decode"`…). Add `LL_PROFILE_MUTEX` to the mesh-repo and texture-cache mutexes for Tracy's
  lock-contention view.
- **Allocation churn:** already instrumented; capture 10-20 s with callstacks, not minutes, and
  compare ratios rather than absolute ms (callstack capture is heavy enough to move frame time).
