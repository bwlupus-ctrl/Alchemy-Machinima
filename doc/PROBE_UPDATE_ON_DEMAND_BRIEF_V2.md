# Reflection probes: update on demand — implementation brief V2

**Status:** DESIGN ONLY (Opus, 2026-09-30). Nothing implemented, built or committed.
**Base:** `fix/animesh-clone-pose-polish` @ `c31f37cc43a` (contains Live Probe refresh `615750a40f9`).
All anchors are at that HEAD. Paths without a directory prefix are `indra/newview/`.
**Process (CLAUDE.md):** Sonnet implements from this brief → Opus + Codex attack the code (adversarial, loop to
0 must-fix) → checkpoint commit + exe backup → ONE build → user tests §9.
**Supersedes:** `doc/PROBE_UPDATE_ON_DEMAND_BRIEF.md` (v1, stale anchors). **Facts source:**
`doc/PROBE_ON_DEMAND_RESEARCH.md` (Codex, at HEAD) — cited as R§n.
**Labels:** PROVES = read in code at HEAD; INFERENCE = reasoning/estimate, not measured.

---

## 0. Decisions made in this brief (read first)

| # | Decision | Why |
|---|---|---|
| D1 | `RenderProbeOnDemand` ships **default false**. Flip to true in a separate one-line commit only after §9 T1–T7 pass. | Every scene is affected; hook coverage is unproven in-world; the built-but-untested backlog is already long (CLAUDE.md rule 5). The switch makes the A/B one click in one session. |
| D2 | Dirty model = **batched event push → per-probe serials** (R§2 recommendation). No octree counters. | R§2 table: GEOM_DIRTY polling misses in-node motion and texture/light-only changes; counters need tombstones/bridge transforms. |
| D3 | **Dynamic probes (avatar/animesh/particle classes) are not dirty-gated**: they stay always-eligible (subject to MinInterval). DYN-class geometry events are **dropped at record time**. | Pose changes inside unchanged extents are unhookable cheaply (R§7). Static probes/Live never draw those classes (llviewerwindow.cpp:6090-6108; Live is static, alcinelightrigmanager.cpp:575). Lights on attachments are NOT dropped (R§7, pipeline.cpp:9626-9640). |
| D4 | Ordinary-probe face budget stays **structurally 1 face/frame** (today's `doProbeUpdate` once per `update()`). No `RenderProbeFaceBudget` setting (a 0..1 range is meaningless). | "Never above today's rate" is guaranteed by structure, and verified by the OVER BUDGET verdict. |
| D5 | Occluded complete probes: **deferred** (dirt and real age preserved; never forced). | R§4 recommended policy; an occluded probe is not bound for rendering (`getReflectionMaps` skips it, llreflectionmapmanager.cpp:1089). |
| D6 | Live Probe gets **no min-interval**. | Its cost is already bounded per frame by its mode (≤ ChangeFaces, or 6 on FULL). A min-interval would only add subject-tracking lag. |
| D7 | Closest-dynamic realtime slicing is **always-cycling** (no dirty gating). | It draws avatars (D3); it is "realtime" by definition. Slicing alone cuts 6 → N faces/frame. |
| D8 | Flexi, texture animation, media frames, cloud scroll (ordinary probes) are **safety-refresh only** in this delivery. | Continuous sources would keep probes permanently dirty; R§2 rank 3. Listed explicitly in §3.8. |
| D9 | **C1 (candidate cull cache) is NOT built unless the user approves** — §6 proves its exact form saves only octree node tests. C0 (per-stage Tracy split of a cube face) IS built. | CLAUDE.md "say so BEFORE starting". This is the one open user decision. |
| D10 | Capture far is **`RenderReflectionProbeDrawDistance`**, not the camera far: `display_update_camera` sets it while `gCubeSnapshot` (llviewerdisplay.cpp:224-228). Corrects R§2 ("retains camera far"). Cull reach = far·√3 (`mFrustumCornerDist` of a 90° square frustum; llvieweroctree.cpp:1379-1382). | PROVES (read). Reviewer must re-confirm (§8 item 10). |

---

## 1. Reconciliation with v1 §B and 615750a40f9

| v1 §B item | Status at HEAD |
|---|---|
| Live Probe face slicing (N faces/frame, pass flips at face 6) | **DONE** — `updateCinematicBudget` llreflectionmapmanager.cpp:686-744; `advanceFace` alcineliveproberefresh.h:570-595; Balanced 2 / Economy 1 / On change 1-6 (header :477-486, :554-557). |
| Live readiness under slicing (flags at 6th face; `mComplete`/`updateNeighbors`) | **DONE** — manager :722-730, :740-743; FULL warm-up :650-669. |
| Live dirty gating, stop when unchanged | **DONE** for lighting/env/origin/settings — `sampleCinematicH` :750-903, converge-to-idle header :409-411, :512-552; watchdog 5 s. |
| Live dirty on rig projector/ignored/pinned list changes | **PARTIAL** — rigs hashed (:768), lights hashed (:774-775); same-probe list edits only assign (:1189-1190). Out of scope here (lists are rig-panel state; R§1 notes it). |
| Live dirty on arbitrary prim geometry/appearance | **MISSING → §4.2 (this delivery).** |
| Live dynamic-column (avatar/particle) sources | **NOT WANTED** — Live is static (alcinelightrigmanager.cpp:575); v1 was wrong (R§1). |
| Per-probe minimum interval | **MISSING → §3 (ordinary only; D6).** |
| Closest-dynamic realtime slicing without a Live Probe | **MISSING → §4.1.** Today it runs `updateRealtimeProbeAllFaces` (6/frame) :590-593. |
| Scratch-slot safety under slicing | **DONE for Live** (block when `realtime_probe == mUpdatingProbe`, :566-570); must be replicated for §4.1. |
| Pass alternation / `isRadiancePass` consumers | **DONE for Live** (`mRadiancePass` set per face, restored, :702-737; `mRealtimeRadiancePass` resync :546-557). §4.1 copies the pattern. |

Keep, do not redesign: `CineLightRigLiveProbe*` settings, `ALCineLiveProbeRefresh::decide` and its modes, the rig-panel
Live Probe card, `[LiveProbe]` log.

---

## 2. Scope of this ONE delivery

- **A.** On-demand scheduling for ordinary probes incl. the default (sky) probe (§3).
- **B-rest.** Closest-dynamic realtime slicing (§4.1); Live Probe scene-change source (§4.2).
- **C0.** Per-stage Tracy zones inside a cube face (§6.1). **C1 only on user approval (§6.2).**
- **D.** Conservative tiny-group cull inside manager-owned captures, default OFF (§7). Probe LOD bias **deferred** (R§6:
  `calcLOD`/`updateLOD` mutate shared `mLOD` and rebuild queues, llvovolume.cpp:1485, :1699-1723).
- Settings, Lightbox + Graphics UI, presets, debug text, `ProbeSched` verdict log, TUT model tests (§5, §8).

Explicitly deferred: shared probe shadow maps, supersample factor, relightable G-buffer probes, fast GGX, probe LOD bias,
hooks for flexi/texture-anim/media (D8).

---

## 3. A — on-demand scheduling for ordinary probes

### 3.1 New pure model `alprobeschedule.h` (header-only, no GL/viewer globals; add to `viewer_HEADER_FILES`)
Hygiene identical to alcineliveproberefresh.h (all `inline`, `stdtypes.h` only, explicit `static_cast`). Namespace
`ALProbeSched`:
```cpp
enum Reason : U16 { R_GEOM=1<<0, R_TEX=1<<1, R_LIGHT=1<<2, R_ENV=1<<3, R_PROBE=1<<4, R_SAFETY=1<<5,
                    R_CLOUD=1<<6, R_RESYNC=1<<7, R_WARMUP=1<<8, R_DYN=1<<9 };
enum Class  : U8  { C_STATIC=1<<0, C_TERRAIN_WATER=1<<1, C_LIGHT=1<<2 };   // DYN is dropped before this point
struct Event  { F32 mMin[3]; F32 mMax[3]; U16 mReason; U8 mClass; };       // agent-space AABB, owned values
struct Record {                                  // one per LLReflectionMap (member, §3.6)
    U64 mDirtySerial = 0;  U64 mAckSerial = 0;  U64 mTxnSerial = 0;  bool mInTxn = false;
    U16 mPending = 0;  U16 mTxnReasons = 0;  U16 mLastReasons = 0;
    F64 mFirstDirty = -1.0;  F64 mFirstDirtyInTxn = -1.0;
    F64 mLastStart = -1.0;   F64 mLastComplete = -1.0;         // REAL start / REAL radiance-face-6 completion
    F32 mStartOrigin[3] = {0,0,0};  F32 mStartRadius = 0.f;  S32 mStartCube = -1;
    F32 mStartCloud[2] = {0,0};                                // default probe only
};
inline bool dirty(const Record& r) { return r.mDirtySerial > r.mAckSerial; }
inline void hit(Record& r, U16 reason, U64 serial, F64 now);   // mDirtySerial=max(.,serial); mPending|=reason;
                                                                // first-dirty time (mFirstDirtyInTxn if mInTxn)
inline void onTxnStart(Record& r, U64 serial, F64 now, U16 reasons, const F32* origin, F32 radius, S32 cube);
inline void onTxnComplete(Record& r, F64 now);   // mAckSerial=max(mAckSerial,mTxnSerial); mInTxn=false;
    // mLastComplete=now; mLastReasons=mTxnReasons; if(!dirty(r)){mPending=0;mFirstDirty=-1;}
    // else {mFirstDirty=mFirstDirtyInTxn;}  mFirstDirtyInTxn=-1.   NEVER a bool clear (R§4 table row 4).
struct Policy { F32 mMinInterval; F32 mMaxAge; };
struct View   { bool mComplete, mOccluded, mDynamic, mAllocated; };
enum class Why : U8 { NONE, WARMUP, DIRTY, SAFETY, DYNAMIC, WAIT_INTERVAL, DEFERRED };
inline Why evaluate(const Record& r, const View& v, const Policy& p, F64 now);
```
`evaluate`, exact order: `!mAllocated`→NONE; `!mComplete`→WARMUP (today's warm-up, no interval);
`mOccluded`→DEFERRED if (dirty or safety-due) else NONE; interval gate: `mLastStart>=0 && now-mLastStart < mMinInterval`
→WAIT_INTERVAL if (dirty||dynamic||safety-due) else NONE; `dirty`→DIRTY; `mDynamic`→DYNAMIC;
safety-due (`mMaxAge>0 && mLastComplete>=0 && now-mLastComplete>=mMaxAge`)→SAFETY; else NONE.
Eligible = WARMUP/DIRTY/DYNAMIC/SAFETY.
Also in the header: footprint math (§3.3), `SliceCursor` (§4.1), window stats + `verdict()` (§5.4). Pure functions only.

### 3.2 Recorder `alprobedirty.{h,cpp}` (new; main thread only)
```cpp
namespace ALProbeDirty {
    bool recording();                        // inline, reads a file-static bool: the ONLY cost on the off path
    void setRecording(bool on);              // manager, top of update()
    enum Tag : U8 { TAG_NONE, TAG_LOD };
    struct ScopedTag { ScopedTag(Tag); ~ScopedTag(); };   // nests; LOD-tagged notes are dropped (counted)
    void noteDrawable(LLDrawable* d, U16 reason);          // world bounds + class, coalesced by pointer
    void noteBounds(const LLVector4a& mn, const LLVector4a& mx, U16 reason, U8 cls, const void* key);
    void noteLight(LLVOVolume* v);                         // sphere(positionAgent, LIGHT_MAX_RADIUS)
    void noteTexture(LLViewerFetchedTexture* t);           // fan-out over face lists
    void markOverflow();                                    // "dirty everything" (shift, cap)
    struct Drain { std::vector<ALProbeSched::Event> mEvents; bool mOverflow; U32 mDroppedLod, mDroppedDyn,
                   mNoopMove, mRaw; };
    void drain(Drain& out);                                 // swap out + clear map; manager only
}
```
- **Coalescing:** `std::unordered_map<const void*, U32>` → index into the frame vector; union AABBs, OR reasons/classes.
  The pointer is a hash key only and is never dereferenced after the note returns (R§2 lifetime rule).
- **Cap:** 4096 coalesced events; beyond → `mOverflow=true`, stop storing. Overflow = dirty all allocated probes (R_RESYNC).
- **Classification `classify(LLDrawable*)`** from its `LLViewerObject` (implementer verifies names with the compiler):
  HUD attachment → drop; `getAvatar()!=nullptr` or animated object, or `LL_VO_PART_GROUP` → DYN; surface patch /
  water / void water pcodes → `C_TERRAIN_WATER`; `isReflectionProbe()` → drop (probe volumes are not captured); else
  `C_STATIC`. If `d->isState(LLDrawable::LIGHT)` → OR `C_LIGHT`, reason OR `R_LIGHT`, AABB inflated by
  `LIGHT_MAX_RADIUS` (llprimitive.h:127, radius clamp :162) — a light is never dropped as DYN (attachment lights).
  DYN without LIGHT → dropped, `mDroppedDyn++` (D3).
- **World bounds `worldBounds(LLDrawable*)`:** if `d->getSpatialGroup()` and its partition `asBridge()` (llspatialpartition.h:544)
  → the bridge's own extents (world; `LLSpatialBridge::updateSpatialExtents`, lldrawable.cpp:1328-1370); else
  `d->getSpatialExtents()`; if no group or non-finite → sphere(`getPositionAgent()`, 0.5·|scale|+0.5).
- Debug-build `llassert` main thread (use the in-tree main-thread helper). No hook ever mutates render/drawable state.

### 3.3 Capture footprint (pure, `ALProbeSched::hitsCapture`)
Inputs: probe origin O, `Rcap = RenderReflectionProbeDrawDistance·√3 + 20 m` (D10 + max projector reach), event AABB,
class, sun and moon unit directions (toward the light), `shadows = RenderShadowDetail>0`, accepted class mask.
- `C_TERRAIN_WATER` → **always hits** (those partitions are infinite-far: llvowater.cpp:284, llvosurfacepatch.cpp:984).
- `C_LIGHT` → AABB (already inflated) vs sphere(O, Rcap).
- `C_STATIC` → AABB vs sphere(O, Rcap) **or**, if `shadows`, the event bounding sphere (c, r) swept along each of
  −sunDir, −moonDir for 256 m (capsule) within `r + Rcap` of O (off-volume casters, R§5 "do not reuse color sphere").
- Class masks: ordinary static probe `C_STATIC|C_TERRAIN_WATER|C_LIGHT`; default probe `C_TERRAIN_WATER|C_LIGHT`
  (its mask is sky/water/terrain/clouds, llreflectionmapmanager.cpp:1319-1321); Live same as ordinary static.
Conservative by construction (over-dirtying allowed; missing a dependency is not — R§8 risks).

### 3.4 Hook sites (each: `if (ALProbeDirty::recording()) { ... }`, nothing else)
| ID | Site (HEAD) | Note |
|---|---|---|
| H1a | lldrawable.cpp:808 (`updateMoveUndamped`) and :846 (`updateMoveDamped`), right after `updateXform` | if `dist_squared > 0` → `noteDrawable(this, R_GEOM)`; extents are still the OLD ones here (partition move comes later via `moveUpdatePipeline`, :751-778). |
| H1b | llspatialpartition.cpp:972 `LLSpatialPartition::move` | after the null check (:977-981) snapshot `worldBounds`; on each of the 3 exits (:991-992, :1003-1004, :1014-1015) note union(old,new) with `R_GEOM` **only if** min/max differ by >1e-4 m (else `mNoopMove++`). Use an RAII helper so no exit is missed. |
| H2 | llspatialpartition.cpp:774 `handleInsertion`, :781 `handleRemoval` (before `removeObject`) | membership; `R_GEOM`. Removal bounds are copied before release (R§2). |
| H3 | pipeline.cpp:4517 `markTextured` (inside the existing `if`) → `R_TEX`; :4580 `markRebuild(LLDrawable*,flag)` (inside the `if`) → `R_GEOM` only if `flag & ~LLDrawable::REBUILD_POSITION` | Position-only requests are covered by H1/H4 (avoids rig no-op churn). Group-level `markRebuild(LLSpatialGroup*)` :4566 is **not** hooked (batching noise; §8 item 1). |
| H4 | llvovolume.cpp:2157 `LLVOVolume::updateGeometry` | at entry (after the `mVolumeImpl` branch :2169-2177, which stays un-hooked = flexi, D8) snapshot `worldBounds` and `content = mVolumeChanged||mFaceMappingChanged||mSculptChanged||mColorChanged`; before `return` at :2255 note `R_GEOM` if `content || bounds changed`. Catches `genBBoxes` moves where extents are updated before the partition move (llvovolume.cpp:1967). LOD-only rebuilds: `content` false and bounds usually equal → no event. |
| LOD | llvovolume.cpp:1719-1723 | wrap the `markRebuild(mDrawable, REBUILD_VOLUME)` in `ALProbeDirty::ScopedTag lod(TAG_LOD)` → dropped, `mDroppedLod++`. Main-camera LOD churn must not dirty probes (R§8 "repeated work"). |
| H5 | llvovolume.cpp:4564 `parameterChanged(U16, LLNetworkData*, bool, bool)` after the base call | if `param_type` is `PARAMS_LIGHT` or `PARAMS_LIGHT_IMAGE` and `mDrawable.notNull()` → `noteLight(this)`. Setters route here (llvovolume.cpp:3222, :3237, :3250, …); implementer confirms `parameterChanged(U16,bool)` :4559 dispatches to this overload and that `setIsLight`→`setParameterEntryInUse` reaches it. Covers rig emitters (alcinelightrig.cpp:1976-1994). |
| H6 | llviewertexture.cpp:2121 `postCreateTexture`, after the sculpt loop (:2135-2143) | main-thread publication (R§2: never in `createTexture`'s worker path). For each channel `< LLRender::NUM_TEXTURE_CHANNELS`, each face in `getFaceList(ch)` (llviewertexture.h:176) → `noteDrawable(face->getDrawable(), R_TEX)`. >4096 faces for one texture → `markOverflow()`. Own Tracy zone `"probe dirty tex fanout"`. |
| SH | llreflectionmapmanager.cpp:1562 `shift()` | `ALProbeDirty::markOverflow()` (queued bounds are in the old frame) + shift the slice cursor's frozen origin (§4.1). |

Environment is **sampled, not hooked** (§3.5). Material/PBR edits reach H3 via `setTE*`/override paths
(llvovolume.cpp:2355-2392, llviewerobject.cpp:5892-5954, 7709-7712 — R§2); uninstrumented async material completion is
safety-covered.

### 3.5 Environment generation (reuse of the Live signature)
- **Extract** `sampleCinematicH` sections 3-5 (llreflectionmapmanager.cpp:777-892) **verbatim** into a file-static
  `appendEnvironmentSignature(ALCineLiveProbeRefresh::Signature& sig, F32 sun_moon_deg, bool blend_only_when_mixing)`.
  Only edits: the two literals `0.1f` at :786-787 become `sun_moon_deg`; the two `getBlendFactor()` lines (:817, :846) become
  `(blend_only_when_mixing && !mixing) ? 0.f : blend` where sky `mixing` = any of sun/moon/cloud-noise current≠next id,
  water `mixing` = normal-map or transparent current≠next id. **Field count and order unchanged** (StickyHash layout
  stable). Live calls `(0.1f, false)` → its H is bit-identical to today (reviewer: moved-code diff).
- Ordinary env: manager members `ALCineLiveProbeRefresh::Signature mOrdEnvSig; StickyHash mOrdEnvSticky; U64 mOrdEnvH`.
  Once per `update()` (inside the flush, §3.6) build `appendEnvironmentSignature(mOrdEnvSig, sunDeg, true)` + tail
  `addAbs(RenderReflectionProbeDrawDistance, 0.5f)`; `h = mOrdEnvSticky.update(sig)`; if `h != mOrdEnvH` (and not the first
  sample) → `R_ENV` hit on **all** allocated probes incl. default. Independent sticky baseline from Live (R§3).
- Rate (INFERENCE, R§3): default 4 h day cycle, 0.5° → ≈ one ENV hit / 20 s → ≈ 0.6 faces/s per probe. Blend factor no
  longer drives cadence unless textures actually mix.
- Default probe extras (in the flush): **R_CLOUD** if `LLEnvironment::getCloudScrollDelta()` (llenvironment.h:182) moved
  > 1e-4 from `mStartCloud` and `now - rec.mLastStart >= RenderDefaultProbeUpdatePeriod`; **R_PROBE** if the camera origin
  moved > 16 m from the default's `mStartOrigin` (its capture origin is camera + 64 m, `touch_default_probe` :138-147).

### 3.6 Manager integration (`llreflectionmapmanager.{h,cpp}`, `llreflectionmap.h`)
- `llreflectionmap.h`: `#include "alprobeschedule.h"`; public member `ALProbeSched::Record mSched;` (after :138).
- Header members: `U64 mSchedSerial = 0; bool mSchedWasOn = false; U64 mLiveSceneSerial = 0; ALProbeSched::SliceCursor
  mRtSlice; LLVector4a mRtSliceFrozenOrigin; SchedWindow mSchedWin; SchedFrame mSchedFrame;` + the env members;
  private helpers `void flushProbeSchedule(bool on)`, `void resetProbeSchedule()`, `void logProbeSchedule(F64 now, bool on)`,
  `void updateRealtimeSliced(LLReflectionMap*, S32 faces)`; static `ProbeCaptureKind sCaptureKind` (§7).
- **`update()` edits, in order:**
  1. Top (before the :212 early return): `static LLCachedControl<bool> on_demand(…"RenderProbeOnDemand", false);`
     `ALProbeDirty::setRecording(on_demand && LLPipeline::sReflectionProbesEnabled);` `mSchedFrame = {};`
     Early returns at :214/:223/:339 also `mRtSlice.reset()`.
  2. After :341 (probes non-empty), before :344: `flushProbeSchedule(on)` where `on = on_demand`. It: drains the recorder;
     `frame_serial = ++mSchedSerial` (every frame); on `on && !mSchedWasOn` or overflow → `R_RESYNC` hit on every
     allocated complete probe (changes while off were not recorded); grace-init `mLastComplete = now` for complete probes
     with `mLastComplete < 0`; if `E × allocated > 200 000` → treat as overflow; else per event × allocated probe →
     `hitsCapture` → `hit(rec, reason, frame_serial, now)`; Live footprint hit → `++mLiveSceneSerial` (once per frame);
     env + default extras (§3.5); per-probe **R_PROBE** when origin moved >0.1 m, radius changed >0.1 m, or
     `mCubeIndex != mStartCube` vs the record; stores `mSchedWasOn = on`. Off: returns after `drain` (discard) and
     `mSchedWasOn=false`. Zones `"probe sched flush"`, `"probe sched env"`.
  3. Selection :471-477 — add `&& (!on || eligibleOrdinary(probe))` where eligibility = `evaluate(...)` ∈
     {WARMUP, DIRTY, DYNAMIC, SAFETY} with `View{mComplete, mOccluded, getIsDynamic(), mCubeIndex!=-1}`, **and**
     `probe != mRtSlice.mProbe || !probe->mComplete` (§4.1 owner excluded once complete). The `mOccluded&&mComplete`
     branch :458-468 is unchanged (D5; `evaluate` returns DEFERRED there only for stats).
  4. Default probe :596-607 — `if (!on)` keep verbatim. `else`: `dw = evaluate(default)` eligible? Level 0: `oldestProbe =
     (eligible && age >= period) ? mDefaultProbe : nullptr` (age as today, `mLastUpdateTime`). Level > 0: if eligible and
     `now - rec.mLastStart >= period` → `oldestProbe = mDefaultProbe` (same override, but only with a reason); if not
     eligible and `oldestProbe == mDefaultProbe` → `oldestProbe = nullptr`.
  5. Txn start :610-620: when `on`, `onTxnStart(rec, mSchedSerial, now, rec.mPending | (incomplete?R_WARMUP) |
     (safety?R_SAFETY) | (dynamic?R_DYN), origin, radius, cube)` (+ cloud delta for default).
  6. `oldestOccluded` block :622-627 **unchanged** (legacy `mLastUpdateTime`; safety uses `mLastComplete`, R§4).
  7. Counters at call sites only: `++mSchedFrame.mOrdFaces` after `doProbeUpdate()` :357 and :619; realtime faces
     `+= 6` after `updateRealtimeProbeAllFaces` :555/:592; Live budget faces via `mCineStats.mFaces` delta around
     :573; sliced faces in §4.1. End of `update()`: fold frame into `mSchedWin`; `logProbeSchedule`.
- `doProbeUpdate` :1273-1277 radiance completion: `if (mSchedWasOn) onTxnComplete(rec, now)` before `mUpdatingProbe =
  nullptr`. Debug text :1267-1270 (mask on, viewer-object probes only): when on, `llformat("%.1f %s", t, letters)` from
  `mLastReasons` — G T L E P S C R W D (geom, tex, light, env, probe, safety, cloud, resync, warm-up, dynamic). Off:
  byte-identical string.
- `resetProbeSchedule()` (clears every `mSched`, `mRtSlice`, env sticky, `mSchedWasOn=false` ⇒ next on-frame resyncs):
  call in `initReflectionMaps` reset block (:2012-2019), `cleanup()` (:2078-2086). `deleteProbe` needs nothing (record dies).

### 3.7 Guarantees and what they are not
- Faces: ordinary ≤ 1/frame (structural); realtime ≤ 6/frame; sliced ≤ N. Verified per frame (§5.4).
- A hard "MaxAge + MinInterval" freshness bound is impossible when demand exceeds 1 face/frame (R§3); STARVED reports it.
- Changes during a capture survive (serial > txn serial). Fake occluded updates never set `mLastComplete` (R§4).

### 3.8 Covered only by the safety refresh (MaxAge, default 60 s) — state in UI tooltip and log header
Flexi motion; texture animation (llvovolume.cpp:616-691); media frames (llviewermedia.cpp:3202-3237); cloud scroll in
non-default probes; water wave animation; rotation of an object whose world AABB is unchanged; non-volume rebuilds
deferred past the frame (trees/grass/terrain completion); async PBR material completion without a `setTE*`; indirect
probe-to-probe bounce (R§2); transient changes that revert before capture. Occluded probes are deferred, not refreshed.

---

## 4. B-remainder

### 4.1 Closest-dynamic realtime slicing (only when `RenderProbeOnDemand` and no Live Probe)
- Dispatch at :583-594 (the non-Live branch). Replace `if (realtime_probe != nullptr) updateRealtimeProbeAllFaces(...)` by:
  `if (on && realtime_probe && realtime_probe != cinematicLive && faces < 6)` → `updateRealtimeSliced(realtime_probe, faces)`,
  else the existing call **unchanged**. `faces = clamp(RenderProbeRealtimeFacesPerFrame, 1, 6)`; 6 ⇒ today's path.
  Whenever the sliced path is not taken this frame → `mRtSlice.reset()`.
- `ALProbeSched::SliceCursor { const void* mProbe; S32 mCube; bool mActive; U8 mFace; bool mRadiance; bool mLastPassIrr; }`
  with `bool begin(probe, cube)` (identity change → reset, start at face 0), `startIfIdle()` (kind = `mLastPassIrr ?
  radiance : irradiance`, same alternation rule as Live header :483, :539, :556), `PassEnd advance()`. Concept reuse of
  alcineliveproberefresh.h per R§1; **that header is not edited**.
- `updateRealtimeSliced(probe, faces)`: zone `"rmmu - rt sliced"` + `LL_PROFILE_ZONE_NUM(faces)`. If `probe ==
  mUpdatingProbe` → skip frame, keep cursor, `++mSchedWin.mRtBlocked` (primary scratch would be shared; :1335-1340).
  Freeze origin at face 0 (as :695-701), restore after. Save `isRadiancePass()`, set `mRadiancePass = cursor.mRadiance`
  per face, `updateProbeFace(probe, face)` (no `force_dynamic`; dynamic via `getIsDynamic()`, identical to today),
  restore. **No** `mCinematicLiveProbeCapture`, no `beginCinematicProbeCapture`, no readiness flags, no `mComplete`
  writes (ordinary warm-up owns `mComplete`). `updateNeighbors(probe)` after a pass end. `mRealtimeRadiancePass` untouched.
- Invariant for reviewers: every secondary-scratch pass starts at face 0 and writes faces 0..5 in order before the face-5
  publication (:1424 copy target, publication at face 5); Live and sliced passes are mutually exclusive per frame and the
  other's cursor is reset whenever it does not run.

### 4.2 Live Probe gains geometry/appearance/light sources
- In `sampleCinematicH`, after section 5 (:892) and before the sticky update (:895):
  `if (ALProbeDirty::recording()) sig.addExact(mLiveSceneSerial);` — the Live footprint (§3.3, static mask, current
  origin) counts every non-DYN geometry/texture/light event. Recording off ⇒ H identical to 615750a40f9 (layout of
  `mExact` unchanged because nothing is appended).
- Effect: a prim edit near the subject → H changes → On change runs a pass pair and re-converges; a moving prop keeps it
  cycling at ChangeFaces/frame (same bound as a walking subject). Balanced/Economy/Every frame ignore H (unchanged).
- No min-interval (D6). Watchdog unchanged. Rig-emitter moves while the subject walks also bump the serial — they
  already change H through the rig signature, so no new cost.

---

## 5. Settings, UI, presets, debug, log, Tracy

### 5.1 Settings — `app_settings/settings.xml`, inserted after `RenderDefaultProbeUpdatePeriod` (:703-713)
| Name | Type | Default | Clamp in code | Persist |
|---|---|---|---|---|
| RenderProbeOnDemand | Boolean | 0 (D1) | — | 1 |
| RenderProbeMinInterval | F32 | 1.0 s | 0..10 | 1 |
| RenderProbeMaxAge | F32 | 60 s | 0 = off, else 5..600 | 1 |
| RenderProbeDirtySunDeg | F32 | 0.5 | 0.1..5 | 1 |
| RenderProbeRealtimeFacesPerFrame | S32 | 2 | 1..6 | 1 |
| RenderProbeTinyCullPixels | F32 | 0 (off) | 0..8 | 1 |
| RenderProbeSchedLog | Boolean | 0 | — | 0 |
| RenderProbeSharedCull | Boolean | 0 | **only if C1 approved** | 1 |
Each gets a Comment with meaning and range. Count the keys after editing (7, or 8 with C1).
MinInterval 1.0 (not v1's 0.25): one 12-face refresh takes ≈12 frames, so 0.25 s would not limit a perpetually dirty
probe; 1.0 s caps it at ≈20 % of frames at 60 fps (INFERENCE).

### 5.2 UI
- **Lightbox → Rendering** (floater_lightbox_settings.xml): new block after the mirror update-rate row (:1537), before
  `cost_text_border` (:1577), same idioms as :1481-1537 (text + slider + spinner + `LightBox.ResetControlDefault` button):
  check "Update probes on demand" (`RenderProbeOnDemand`); sliders "Safety refresh (s)" 0-600, "Min interval (s)" 0-10,
  "Sun change (deg)" 0.1-5, "Realtime faces/frame" 1-6 (all `enabled_control="RenderProbeOnDemand"`); combo "Probe tiny
  cull" Off 0 / Light 1 / Strong 3 (`RenderProbeTinyCullPixels`); check "Log probe schedule" (`RenderProbeSchedLog`).
  Raise `render_settings_scroll_content` height (:1320, 566) by the added rows (count them). Tooltips: on-demand one states
  the §3.8 safety-only list in one sentence; realtime one says "Realtime detail without a Live Probe only".
- **Graphics → Advanced** (floater_preferences_graphics_advanced.xml): one check "Update reflection probes on demand"
  after the `ProbeCount` combo (:954-983), `left="420" top_delta="22"`, `Pref.RenderOptionUpdate` commit like :860.
  Verify by summing `top_delta`s that the Tonemap slider (:1178-1196) still ends above `vram_status_border` (top 480). If
  it would collide, omit this row (Lightbox only) and say so in the implementation report — do not move other controls.
- Rig panel Live Probe card: **unchanged**.

### 5.3 Presets / reset
- llpresetsmanager.cpp `getGraphicsControlNames` (:314+): insert the 6 non-log, non-C1 names **before**
  `"RenderReflectionProbeDetail"` (:342), each with a trailing comma. Do NOT touch :343 (pre-existing upstream missing
  comma between `"RenderReflectionProbeLevel"` and `"RenderCASSharpness"` — out of scope, report only).
- Per-control reset buttons in the Lightbox block (above). No panel reset-list changes.

### 5.4 `[ProbeSched]` log (when `RenderProbeSchedLog`, both on and off paths) — every 5 s of frames the manager ran
```
[ProbeSched] mode=on|off probes=<alloc> faces=<ord/s> rt=<rt/s> starts=<n> done=<n>
 g=<n> t=<n> l=<n> e=<n> p=<n> s=<n> c=<n> r=<n> w=<n> d=<n> events=<coalesced> raw=<notes> lod_drop=<n> dyn_drop=<n>
 noop_move=<n> overflow=<n> dirty=<now> deferred=<now> wait=<now> maxlag=<s> rt_blocked=<n> env_changes=<n>
 verdict=<OK|STARVED n=<k> worst=<s>|OVER BUDGET ord=<max>/1 rt=<max>/<allow>|OFF-BASELINE|PAUSED>
```
Reason counters count txn starts by reason bit. `maxlag` = max pending age of non-occluded dirty/safety-due probes.
- **OVER BUDGET** — any frame in the window with ord > 1, rt > 6, or a sliced frame with rt > N. It is a bug; must never fire.
- **STARVED** — some non-occluded eligible probe pending > 5 s + MinInterval (budget too small for demand; not a bug).
- **PAUSED** — the manager early-returned or was paused for > 50 % of the window (measurement untrustworthy).
- **OFF-BASELINE** — on-demand off (faces/s ≈ fps is expected). **OK** otherwise.
Priority: OVER BUDGET > PAUSED > STARVED > OK. Window restarts after gaps > 0.5 s (same rule as :974-984).

### 5.5 Tracy zones
New: `"probe sched flush"`, `"probe sched env"`, `"probe dirty tex fanout"`, `"rmmu - rt sliced"` (ZONE_NUM faces),
`"probe tiny cull"` (ZONE_NUM rejected groups) and C0's five zones (§6.1). Existing timing truth unchanged:
`"reflection manager update"`, `"probe update"`, `"rmmu - realtime"`, `"rmmu - cine step"`, `"rmmu - cine budget"`,
`"cubeSnapshot"`. No zones inside H1-H5 (hot paths).

---

## 6. C — shared cull

### 6.1 C0 (built): measure what a face costs
llviewerdisplay.cpp `display_cube_face` (:1290-1365): add `LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY` scopes
`"cube face cull"` (around :1307), `"cube face shadow"` (:1312), `"cube face sort"` (:1316-1330),
`"cube face geom"` (:1353-1357), `"cube face light"` (renderDeferredLighting). Zones only; no statement moves.

### 6.2 C1 (candidate cache, `RenderProbeSharedCull`) — **needs user approval; recommendation: do not build**
**Pixel-identity argument (exact form).** Per face, `LLSpatialPartition::cull` (llspatialpartition.cpp:1429-1459) runs
`LLOctreeCull`, whose node test is `min(frustum, sphere(origin, mFrustumCornerDist))` (:1062-1070), hierarchical: a
rejected node prunes its subtree, a fully-inside node accepts descendants untested (llvieweroctree.cpp:1345-1370),
`SKIP_FRUSTUM_CHECK` groups accept under a non-zero parent (:1354-1355). Group extents nest (rebound unions children), so a
node failing the union sphere fails every face and a node fully inside a face is fully inside at every descendant. Hence a
cache of "nodes intersecting the union sphere, in DFS order with parent index", re-filtered per face with the identical
test, the parent-result rule for `SKIP_FRUSTUM_CHECK`, the same `checkObjects`/`processGroup` (:1468-1507, pipeline.cpp:
4047-4080), bridges through their own `setVisible` (lldrawable.cpp:1537+), VO-cache cull and sky pushes (pipeline.cpp:
4019-4044) left per face, yields identical `sCull` contents and order ⇒ identical pixels. Invalidate on probe origin/pass
change and any membership/bounds event (§3.4 H1-H2), rebuild rather than restart the pass (R§5).
**What that saves:** only the tests at union-rejected subtree roots — one sphere test each, which the per-face
traversal already pays once per root. Every per-face frustum test, every `markNotCulled` push, bridge culls, VO-cache
culls, stateSort, both shadow-cascade culls (retained per R§5; pipeline.cpp:25975-25990, 26710-26714) and all rendering
remain per face. Expected saving ≈ 0 plus cache overhead (INFERENCE; C0 will show `"cube face cull"`'s share). A cheaper
non-exact union cull is not pixel-identical and is out of scope. If the user nevertheless approves C1: probe-owned context
object on the manager transaction (never `sCull`/the display_cube_face static result), lifetime-validated group handles
(no `LLDrawInfo` retention, pipeline.cpp:5080-5135), key = probe, cube epoch, pass, origin, draw distance, render mask,
water clip, coordinate epoch, spatial epoch; absent for Hero (llheroprobemanager.cpp:295-305) and Prism
(llprismlens.cpp:6284-6327); the `!gCubeSnapshot` union-shadow gate (pipeline.cpp:25988-25990) untouched.

---

## 7. D — conservative tiny-group cull (default OFF, byte-identical at 0)
- **Capture context:** `enum class ProbeCaptureKind : U8 { NONE, ORDINARY, LIVE, REALTIME }`, static
  `LLReflectionMapManager::sCaptureKind` + public static getter. Set by an RAII scope in `updateProbeFace` around the two
  `probe->update(...)` calls (:1323, :1330): LIVE if `mCinematicLiveProbeCapture`, REALTIME if `probe != mUpdatingProbe`,
  else ORDINARY; restored to NONE on scope exit (covers `LLReflectionMap::update`'s early return, llreflectionmap.cpp:55-56).
  Hero never enters this function; Prism auxiliary renders never run inside it.
- **Filter:** pipeline.cpp `postSort`, visible-group loop, after the occlusion/auto-hide `continue` (:5111-5116):
  `if (px > 0 && kind != NONE && !sShadowRender && !sPrismLensRender && group is rejectable && tiny) continue;`
  - rejectable: partition `PARTITION_VOLUME`, `asBridge()==nullptr`, not dead, and **every** `mDrawMap` key ∈ {PASS_SIMPLE,
    PASS_GRASS, PASS_SHINY, PASS_BUMP, PASS_POST_BUMP, PASS_MATERIAL, PASS_MATERIAL_ALPHA_MASK, PASS_SPECMAP,
    PASS_SPECMAP_MASK, PASS_NORMMAP, PASS_NORMMAP_MASK, PASS_NORMSPEC, PASS_NORMSPEC_MASK, PASS_ALPHA_MASK, PASS_GLTF_PBR,
    PASS_GLTF_PBR_ALPHA_MASK} (lldrawpool.h:149-213). Anything fullbright, emissive, glow, blend, alpha, invisible,
    rigged → kept (alpha groups are therefore never skipped, so the alpha push :5143-5179 is consistent).
  - tiny: bounding radius `r = |mObjectBounds[1]|`, `d = |center − camera origin|`; keep if `d − r <= 2·near`; pixel diameter
    upper bound `p = 3 · r · res / (d − r)` with `res = mProbeResolution` (final, not the 4× target, :295 vs :1358) and the
    factor 3 = cube-corner magnification bound (1/cos²θ). Reject iff `p < px`.
- Lights are unaffected (selected from the light set, not draw infos); shadow casters unaffected (`!sShadowRender`).
  Main view, main shadows, Hero, VCam/Prism: `kind == NONE`. `px == 0` ⇒ the branch is never taken (identity).
- UI promises "small objects" only as whole groups (R§6); tooltip says so.

---

## 8. Review package

### 8.1 Off-path inertness to prove (per switch)
| Switch off | Must be proved |
|---|---|
| RenderProbeOnDemand=0 | Hooks cost one branch on a static bool, no writes. `flushProbeSchedule` only drains/discards. Selection :471-477, default rule :596-607, dispatch :583-594, debug text, `doProbeUpdate` identical in effect. Live H identical (§4.2). Only additions: call-site counters + log (setting-gated). |
| RealtimeFacesPerFrame=6 | Always the unchanged `updateRealtimeProbeAllFaces` body. |
| TinyCullPixels=0 | `postSort` branch never taken. |
| SharedCull=0 (if C1) | Today's `updateCull` path, untouched. |

### 8.2 Shared-state isolation proofs required from reviewers
(a) No hook mutates drawable/group/pipeline state or calls pipeline functions. (b) `sCaptureKind` is NONE outside
`updateProbeFace`'s scope, including Hero/Prism/main/shadow passes. (c) Secondary scratch: no interleaving between the
Live budget cursor and `mRtSlice`; every pass starts at face 0; blocked when the probe is `mUpdatingProbe`. (d)
`mRadiancePass` restored after sliced faces; `mRealtimeRadiancePass` untouched by the sliced path. (e) `mLastUpdateTime`
semantics unchanged on both paths. (f) No GL state, blend, colour-mask, draw-buffer change anywhere (CLAUDE.md rule 1).
(g) `appendEnvironmentSignature` extraction is a moved block with exactly the two documented edits.

### 8.3 Least-sure list — attack these first
1. **Coverage holes:** find any geometry/appearance change that emits no event (movePartition from llviewerobject.cpp:1131,
   pipeline.cpp:4553; group-only rebuild paths; bridge-child moves). Is excluding `markRebuild(group)` wrong?
2. **Perpetual dirt:** do Cine rig `applyFrame` writes (alcinelightrig.cpp ~1976-1994, positions) emit events on a
   static rig? H1b's 1e-4 no-op filter and H3's position-only exclusion are meant to stop it.
3. **Serial race:** events noted during this frame's capture must get the next serial (flush is before :354).
4. **Default-probe policy** vs today's priority override and the level-0 max-frequency rule.
5. **Occlusion deferral** and `oldestOccluded` (:622-627): safety must never be satisfied by it.
6. **Slice cursor/scratch** (§4.1) against re-designation, `initReflectionMaps`, `reset()`, teleport, `shift()`.
7. **Live H gating:** recording off ⇒ H bit-identical; recording on ⇒ no self-dirt from the Live probe prim (dropped via
   `isReflectionProbe()`).
8. **Env extraction:** Live schema unchanged; the ordinary blend-factor rule keeps layout stable.
9. **D context scope** and group-rejection criteria; any rejectable group that can emit light or be the only surface
   visible near a seam.
10. **Footprint (D10):** `Rcap = drawDist·√3 + 20 m`, sun/moon capsule 256 m, infinite-far classes — too small anywhere?
11. Recorder cap / E×P overflow / OFF→ON resync correctness and cost (loading storms).
12. H6 fan-out: main thread only; cost with thousands of faces.
13. **Is the approach wrong?** Argue for counters or for dropping any hook.

---

## 9. In-world test plan (user runs; `RenderProbeSchedLog` on; Tracy 10 s per step; outcomes stated in advance)
Report raw log lines. A counter contradicting its predicted value is a FAIL; `PAUSED` = INCONCLUSIVE, retake.
- **T0 Off identity.** On-demand off vs backup exe (`AlchemyTest.pre-probeondemand.exe`): lossless snapshot diff within
  the A1/A2 noise floor (procedure of LIVE_PROBE_REFRESH_DESIGN §8.0). `verdict=OFF-BASELINE`, faces ≈ fps.
- **T1 Static skybox, no Live, detail Static+Dynamic, P allocated probes.** Off: `"probe update"` every frame (≈16 ms
  inclusive per the user's Tracy). On: after warm-up (≈12P frames) faces/s ≈ 12P/60 (safety only; `s>0`, all else ≈0),
  `verdict=OK`; `"probe update"` appears as isolated single frames; frame time ≈ probes-off baseline between them
  (INFERENCE). Converged image identical to Off (snapshot diff).
- **T2 Rez / move / retexture a prim.** `g`/`t` > 0; nearby probes refresh within ≈ MinInterval + 12 frames (debug text
  `G`/`T`); distant probes stay clean.
- **T3 Drag the sun slider.** `e > 0`, ord ≤ 1 face/frame, never OVER BUDGET; STARVED allowed while dragging.
- **T4 Running day cycle (4 h).** `e` ≈ every ~20 s; faces/s ≈ 0.6P + safety (INFERENCE, R§3).
- **T5 Toggle / recolour a light; animated rig look.** `l > 0`; with flicker, nearby probes cycle but each ≤ 1 start/s.
- **T6 Avatar walks through a static probe.** `dyn_drop` rises, no `g` hits for that probe, it stays clean.
- **T7 Camera orbit in a static set.** `lod_drop` rises; `g`/`t` should stay near 0 once textures settle. High `t` = texture
  streaming noise (efficiency finding, not a correctness failure) — report it.
- **T8 Live Probe, On change, move a prim near the subject.** On: `[LiveProbe] last=changed`, clean pair, idle. Off: no
  reaction until the 5 s watchdog. (Discriminating A/B for §4.2.)
- **T9 Realtime detail, no Live, dynamic probe near camera.** Off: 6 faces/frame under `"rmmu - realtime"`. On (N=2):
  2 under `"rmmu - rt sliced"`, `rt_blocked` ≈ 0 after warm-up, own reflection lags ≤ 3 frames/pass.
- **T10 Occlusion.** Edit a prim behind the camera: `deferred > 0`, no refresh; turn around → refresh within ≈1 s.
- **T11 Default probe, coverage None/Sky.** On: updates only on env change, camera travel > 16 m, or cloud movement at
  ≤ `RenderDefaultProbeUpdatePeriod`; static sky with paused clouds → only safety.
- **T12 D.** Tiny cull Strong vs Off in a dense set: `"probe tiny cull"` ZONE_NUM > 0 and lower `"probe update"` time;
  main view snapshot identical with probes frozen; reflections differ only by missing small opaque groups.
- **T13 C0.** Record the five cube-face zones for one face at the skybox; report `"cube face cull"` share (input to D9).
- **T14 Teleport, region crossing, EEP swap, probe-count change.** No crash, `r`/`overflow` increments, no OVER BUDGET.

---

## 10. Implementation order (single delivery; one build after review converges)
- [ ] 1. `alprobeschedule.h` (Record, evaluate, hitsCapture, SliceCursor, verdict) + CMake header entry.
- [ ] 2. TUT in `tests/alcinelightrigmodel_test.cpp` (include `../alprobeschedule.h`): S1 event mid-txn stays dirty; S2
      same-frame pre-start event acked; S3 MinInterval gate keeps pending; S4 safety at MaxAge, never at 0; S5 occluded →
      DEFERRED, dirt kept, eligible when unoccluded; S6 footprint (inside/outside sphere, terrain global, light inflation,
      sun capsule, default class mask); S7 `onTxnComplete` never clears newer serials; S8 verdict priority incl. OVER BUDGET;
      S9 SliceCursor N faces, alternation, identity reset at face 0, blocked frame keeps cursor; S10 grace-init + resync.
      10 tests — verify by counting `template<> template<>` additions.
- [ ] 3. `alprobedirty.{h,cpp}` + CMake source entry.
- [ ] 4. settings.xml: 7 keys (count them).
- [ ] 5. Hooks H1a, H1b, H2, H3, H4, LOD tag, H5, H6, SH (§3.4) — each behind `recording()`.
- [ ] 6. Manager: env extraction (moved block) → members/record → flush → selection/default/txn/completion → debug text →
      `resetProbeSchedule` sites → sliced realtime → Live serial append → counters/log.
- [ ] 7. D: capture-kind scope + postSort filter. C0 zones.
- [ ] 8. UI (Lightbox block, Graphics row per §5.2 rule), presets list.
- [ ] 9. Adversarial review (Opus + Codex) with §8.1-8.3 verbatim; loop to 0 must-fix.
- [ ] 10. Checkpoint commit + backup `AlchemyTest.pre-probeondemand.exe`; ONE build; hand §9 to the user.
- [ ] (C1 only if the user approves D9, as a separate reviewed step after §9 T13.)

## 11. OFF-LIMITS
Shaders; `llheroprobemanager.*`; `llprismlens.*`; `alcineliveproberefresh.h` (reuse only, no edits);
`updateRealtimeProbeAllFaces` and `updateCinematicBudget` bodies; `ALCineLiveProbeRefresh::decide`/Live policy;
`alcinelightrig*`, `alpanelcinelightrig.cpp`, `panel_cine_light_rig.xml`; `LiveProbeConfig`/scene LLSD/Setups;
`updateProbeFace` body except the capture-kind scope lines; `llviewerwindow.cpp`; `llenvironment.*` (getters only);
`pipeline.cpp` except H3 lines and the D filter (and C1 only if approved) — in particular the `!gCubeSnapshot`
union-shadow gate (:25988-25990), alpha-distance skip (:5162-5172) and `calcPixelArea`; `LLVOVolume::calcLOD` and
`updateLOD` beyond the one scope line; llpresetsmanager.cpp:343; the `enve` tree; `vcpkg`.
Files allowed: the new `alprobeschedule.h`, `alprobedirty.{h,cpp}`; `CMakeLists.txt`; `llreflectionmap.h`;
`llreflectionmapmanager.{h,cpp}`; `lldrawable.cpp`; `llspatialpartition.cpp`; `pipeline.cpp` (as above);
`llvovolume.cpp`; `llviewertexture.cpp`; `llviewerdisplay.cpp` (C0 zones only); `app_settings/settings.xml`;
`floater_lightbox_settings.xml`; `floater_preferences_graphics_advanced.xml`; `llpresetsmanager.cpp`;
`tests/alcinelightrigmodel_test.cpp`.
