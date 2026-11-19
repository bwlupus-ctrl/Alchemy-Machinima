# Probe update on demand: adversarial code review (Opus)

**Scope:** the uncommitted implementation on `fix/animesh-clone-pose-polish` @ `84545d8b841`. That is `git diff` plus
`alprobeschedule.h`, `alprobedirty.{h,cpp}` and TUT `test<72>`..`test<90>`, written from BRIEF_V6 plus 3 corrections.
The review was read-only. Nothing was edited, built or committed.

**Labels:**
- **PROVES:** read in the code.
- **INFERENCE:** reasoning about runtime behaviour or magnitude.

The user has accepted the brief §0.4 limits, so they are not flagged here.

**Verdict: FIX-THEN-BUILD.**
- P1-1 and P1-3 are small, local fixes.
- P1-2 needs a user decision: tag it, or accept it as a §0.4 limit.

Under the user's severity gate, P1-1 and P1-2 break the strict-T7 promise ("camera motion never dirties probes").
P1-3 breaks the §5.1 invariant: "secondary-scratch passes start at face 0".

---

## P1 (blockers under the user's severity gate)

### P1-1: The light-diff prefilter follows the camera, so camera motion creates light events
**Where:** `llreflectionmapmanager.cpp:1631-1640` (union), `:1682` / `:1710` (`lightLeave`), and `alprobeschedule.h`
`lightStep` (the `is_new` event).

**Mechanism (PROVES):**
- The prefilter union is built from every probe with `owner == ORDINARY || probe == Live` and a cube.
- That set includes `mDefaultProbe`. Its `mOrigin` is the camera + 64 m, rewritten on every default-probe face by
  `touch_default_probe()` (`:2619-2621`). Under ON the default probe re-captures every 16 m of camera travel.
- Each time it does, the union AABB moves. Lights at the margin then leave the prefilter, which triggers `lightLeave`
  and a discrete `C_LIGHT|R_LIGHT` event. Lights that enter it get `is_new`, which also emits a discrete event.
- The Live sphere does the same as its subject walks. That is usually the user's own avatar, so it moves with the
  camera.

**Effects:**
- (a) `nl` counts these events (`mEvLights`). Strict T7 requires `nl = l = 0`, so T7 and T28 FAIL on the counters
  alone.
- (b) The default probe takes C_LIGHT hits and `markRecountDue` from these events. INFERENCE: it re-captures every
  `RenderDefaultProbeUpdatePeriod` while you fly, instead of once per 16 m.
- (c) Over-cap and recount-due probes are hit unconditionally (`lightUnconditional`). That covers every probe with
  more than 256 lights, which is the T28/T46 scene.
- (d) Under churn, the `mLightsBySeq.erase` cost is O(n) per light.

**Fix:**
- Build the union from ORDINARY probes **excluding `mDefaultProbe`**.
- Keep tracking lights inside the Live sphere for the Live tokens. Emit add/leave events only for lights that
  intersect the ordinary-only union; add or drop Live-only entries silently, because the Live H picks them up through
  its tokens.

### P1-2: VO-cache culling kills and re-creates objects as the camera turns, which gives `g` (and `t`) on orbit and fly
**Where:**
- `llviewerregion.cpp:1755-1834` `killInvisibleObjects`, which calls `gObjectList.killObject` at `:1830`.
- The H2 hooks at `llspatialpartition.cpp:781` and `:793`.

**Mechanism (PROVES):**
- `sVOCacheCullingEnabled` = `RequestFullRegionCache && ObjectCacheEnabled`. Both default to 1 (`llworld.cpp:114`).
- Objects behind the camera beyond `sRearFarRadius` are killed. That runs `markDead`, then `unlinkDrawable`, then
  `handleRemoval`, and H2 records a discrete `R_GEOM`.
- They are re-created from the cache when they come back into view. That gives H2 insertion, an H3 `REBUILD_ALL`, and
  then first-image textures (H6 (a)) and first mesh arrivals (MS), all as real events.
- This repeats on every orbit pass, so strict T7 `g = 0` FAILS on the second pass. T23b is also at risk.

INFERENCE: the size of the effect depends on how the scene extent compares with `sRearFarRadius` (between
`SceneLoadMinRadius` and a fraction of the draw distance).

The brief does not cover this path. §9.3 lists only particles, impostors, avatar-attached statics and water.

**Options (needs a user decision):**
- (a) Put a `ScopedTag(TAG_VOCACHE)` around the kill loop in `killInvisibleObjects`, and flag cache-created objects
  (for example by `mEntry->hasVOCacheEntry()` plus a one-shot "created from cache" bit) so their first build and
  insertion are tagged. Probes then keep the culled objects, which is arguably the better image.
- (b) Accept it as a new §0.4 limit, and redefine T7 with `RequestFullRegionCache=0`.

### P1-3: The sliced cursor is not reset when Live takes the secondary scratch, which publishes a garbage cube
**Where:** the cine-step branch, `llreflectionmapmanager.cpp:582` onwards. `mRtSlice.reset()` exists only at `:713`
(Every-frame Live), `:726`, `:731` and the early returns.

**Mechanism (PROVES):**
1. A closest-dynamic sliced pass is mid-flight. For example, N = 2 gives faces 0-1 of 6. Note that the `break` at
   `:2137` happens only at a pass end, so passes do span frames.
2. A Live Probe is designated in On change, Balanced or Economy mode. Its budget and FULL passes write the **same
   secondary scratch** (`updateProbeFace`: `probe != mUpdatingProbe` gives `sourceIdx + 1`).
3. Live is undesignated and the same dynamic probe becomes realtime again. `sliceBind` sees the same id and cube, so
   it **resumes at face k**.
4. Face 5 then filters irradiance or radiance from faces 0..k-1, which hold the Live probe's capture.

This is the T22 failure ("No garbage cube").

**Fix:** `if (mOnDemandActive) mRtSlice.reset();` at the top of the cine-step branch, or whenever `realtime_probe ==
cinematicLive`.

---

## P2

- **P2-1: The recount completes at most 3 probes per frame whatever the budget.** Where: `alprobeschedule.h:1479-1500`
  (`run`: prio, reg, prio, each `advance` = at most 1 probe).
  - With P = 256 and L = 50 a full cycle takes ≥ 86 frames. §4.5.6 gives ⌈P·L/8192⌉ = 2.
  - Until a probe is recounted it is "treated as over cap", so it takes extra unconditional light hits.
  - Correction 2's fairness can be kept while looping `assign`/`advance` until the budget is used.
- **P2-2: Spot events become global in their own frame.** Where: `llreflectionmapmanager.cpp:1752`.
  - Every eligible-spot add/remove/STRUCT marks **all** ordinary probes recount-due **before** the hit test (`:1891`
    onwards). `lightUnconditional` (`alprobeschedule.h:632`) then hits every probe.
  - This contradicts §4.5.6, "spots need no extra global term". It is brief-faithful but over-dirties: a rig projector
    on/off toggle refreshes every probe.
  - Fix: for a probe whose last count ≤ cap−1, being due because of this frame's single spot event should not count
    as over cap.
- **P2-3: H2 inside a `ScopedMove` notes motion even when bounds are unchanged.** Where: `alprobedirty.cpp:387`,
  `:412`.
  - H1b means "no-op if the union did not change", but an octree re-bin (`markPartitionMove` from `updateLOD` at
    `:1777`, processed untagged) still produces a pending motion state, then a `z` settle.
  - INFERENCE: this is rare in this fork, because the camera-distance term in `getBinRadius` is commented out
    (`llvovolume.cpp:4807`).
  - Fix: skip H2 notes while `sMoveDepth > 0` and let `ProbeMoveNote` decide.
- **P2-4: Every region crossing dirties every probe.** Where: `pipeline.cpp:4483-4494` (`shiftObjects`, then
  `shiftPos`, then `markRebuild(REBUILD_ALL)`) for static non-volume drawables.
  - Water and void water are `C_TERRAIN_WATER`, which is global, so all probes re-capture on each crossing. This
    affects T14 and any T7 flight across a border.
  - Fix: a `ScopedTag` around the shift loop.
- **P2-5: The S13 golden is a placeholder that passes silently.** Where: `tests/alcinelightrigmodel_test.cpp:5482`.
  - `kGoldenH = 0`, so the golden branch is **skipped silently**. S13 passes on the implementer's own oracle.
  - I checked `podReferenceHeadEnv` statement-for-statement against the HEAD `sampleCinematicH` lines removed by the
    diff: order, tolerances, overload kinds and exact items all match.
  - Independent golden: an FNV re-implementation in Python of `StickyHash::update` over `podEnv()` in HEAD order gives
    **`kGoldenH = 0x1E859BA4513A0291ULL`** (INFERENCE until the test confirms it).
    - 60 fields and 84 floats.
    - F32 rounding emulated, including `v += 0.07f`.
    - UUIDs `{n, 0 × 15}`.
  - Set that value, and make 0 a failure instead of a skip.
- **P2-6: The ON edge does not re-mark recount.** The ON edge (`:1814`/`:1824`) calls `mRecount.reset()` but does not
  `markRecountDue` the records. Counts that went stale during OFF are trusted until the regular job reaches the probe.
- **P2-7: A sculpt-map refinement is a real event even when it should be tagged.** `llvovolume.cpp:943-948`
  (`updateTextureVirtualSize`) issues an untagged `markRebuild(REBUILD_VOLUME)`, which is H3 discrete. It defeats the
  MS tag at `postCreateTexture`. This hits only on the first pass.
- **P2-8: Two recorder sets can grow during early returns.** `sInsertedThisFrame` and `sRemovedThisFrame` are cleared
  only in `drain()`. During early-return frames with recording ON (teleport, login < PRECACHE) they grow with every
  octree insert and remove. The growth is bounded by the drawable count.

---

## Answers to the attack list
- **(1) OFF identity:** PROVES byte-identical rendering at defaults.
  - Every hook is gated on `recording()`. `ScopedTag`, `ScopedMove` and `ProbeCaptureScope` write only globals or
    statics: `sCaptureKind` is read only by tiny cull, and cull is off at px = 0.
  - The `calcNearbyLights` and `[LiveProbeFaceLights]` terms are false. `pinned_count` is equivalent, and `isTooSlow`
    is still evaluated.
  - The D filter is gated on `px > 0`. The C0 zones only add scopes.
  - `sampleCinematicH` is changed only by the env extraction. Its order, tolerances and types are verified, and
    `cine_animating` is unchanged on OFF.
  - OFF per-frame cost: a handful of calls, `w = SchedWindow()` (about 300 B memset), and zero-initialisation of
    LLVector3/LLQuaternion locals per `updateMove`. There are no allocations.
- **(2) Clean without capture:** none found.
  - The ack needs a txn in the same epoch and cube with irradiance done.
  - `clearTxn` is called only at the 3 sites.
  - `mRadiancePass` is set false only at an ON txn start, and slicing and Live save and restore it.
  - The barrier uses txn serial ≥ arm serial for ORDINARY, pass start serial ≥ arm serial for REALTIME, and start op >
    arm op for Live.
  - Faces per frame are ≤ 1 + 6 in every mode, because Live and sliced passes are exclusive.
  - The only wrong-cube path is P1-3.
- **(3) Crash/UB:** none found.
  - Recorder, debounce and light-snapshot keys are never dereferenced.
  - `mLightsBySeq` is pruned before every erase.
  - H6 iterates the live counts.
  - H2 runs while `mVObjp` is still set (`cleanupReferences`: `unlinkDrawable` comes before `mVObjp = NULL`).
  - Every hook is main-thread: `postCreateTexture` is finalised through the main-thread callback, and the DS loop and
    the material lambdas run on the main thread.
- **(4) Strict T7:** P1-1 and P1-2 (with P2-3, P2-4 and P2-7 as minor leaks). The LOD, mesh and texture tags are
  otherwise sound.
- **(5) Compile:** the build uses `/W3` + `CMAKE_COMPILE_WARNING_AS_ERROR` (00-Common.cmake:46, :179). C4189 is level 4,
  so it cannot fire.
  - Checked: `SLOT_COUNT` and `LIGHT_COUNT` are S32, `rigRimParams(S32)`, `CineRigRimTint` is F32 in settings.xml, the
    other setting types match, `getCloudScrollDelta` returns LLVector2, `mHashUs*` are F64, `GoboOverride` /
    `isProjectorNoShadow` / `RenderFarClip` are public, and `LIGHT_MAX_RADIUS` comes from llprimitive.h.
  - No risk found. The compiler remains the final arbiter.
- **(6) Tests:**
  - 19 tests (72..90) sit under a ceiling of 96, so all are registered. The ceiling is correct.
  - I traced S14 (all sub-cases), S15, S17 and S18 by hand, and they pass.
  - S13's golden check is skipped (P2-5).
- **(7) Deviations:** all sound.
  - Sky = 52 floats is correct: 6 + 27 + 7 + 9 + 2 + 1.
  - The `onTxnComplete` serial is needed for the nack hit.
  - The four H4 flags match the `lod_only` term.
  - Dropping sky, WL sky and clouds is required: they rebuild every frame and the env hash covers them. Avatars do not
    appear in non-dynamic captures.
  - Counting `rebal` is fine.
  - The 1.25× superset matches `calc_light_dist`. The only exception is selected far lights, which does no harm.
  - The default probe as a barrier member follows the brief.
  - The GLOBAL env event bumping Live coalesces with the Live H change in the same frame.
  - Logging on OFF is negligible.
