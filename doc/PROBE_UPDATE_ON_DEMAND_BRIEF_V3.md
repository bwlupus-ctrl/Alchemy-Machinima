# Reflection probes: update on demand — implementation brief V3

**Status:** DESIGN ONLY (Opus, 2026-09-30). Nothing implemented, built or committed.
**Base:** `fix/animesh-clone-pose-polish` @ `303065bda35`. The only change since `c31f37cc43a` is the Night Mask rig-panel
UI (alpanelcinelightrig.*, panel_cine_light_rig.xml), so every probe anchor below was re-read and still holds.
Paths without a directory prefix are `indra/newview/`.
**Process (CLAUDE.md):** Sonnet implements from this brief. Opus and Codex then attack the code, looping until 0 must-fix.
Then a checkpoint commit and exe backup, ONE build, and the user runs §10.
**Supersedes:** V2 (reviewed by `doc/PROBE_UPDATE_ON_DEMAND_REVIEW_CODEX_R1.md` = CX and `..._OPUS_R1.md` = OP).
**Facts source:** `doc/PROBE_ON_DEMAND_RESEARCH.md` (R§n). **Labels:** PROVES = read in code at HEAD; INFERENCE = estimate.
**User ruling:** any finding that breaks the feature's promise is a blocker, whatever its label.

---

## 0. Resolution table (every review finding → where V3 fixes it)

| ID | Finding (short) | Resolution in V3 |
|---|---|---|
| CX P1-1 | Default probe never settles: +64 m Z offset vs `mStartOrigin` | §4.6 step 2e. Store the **camera** origin in `mStartCam` and compare camera to camera. The default probe is excluded from the generic origin rule. |
| CX P1-2 | Texture animation → `REBUILD_TCOORD` continuous dirt | §4.4 row TX. Both animation-driven rebuild sites are tagged: matrix creation llvovolume.cpp:665-681 and the camera-size toggle :876-894. PROVES: neither runs per frame. :673 fires once per face, :880 fires when on-screen size crosses the threshold. Animation *appearance* stays safety-only (§4.9). |
| CX P1-3 | `updateXform()` returns interpolation error, not displacement (lldrawable.cpp:639-683) | §4.4 H1a. Snapshot `mXform` position/rotation and `mCurrentScale` **before** `updateXform` and compare them after. Lights no longer depend on the move hooks: see the nearby-light diff (§4.5). |
| CX P1-4 | Completion contract: `mRadiancePass` stale after delete (:1238-1242), epochs, both passes, pointer/slot reuse | §4.7. Transaction contract: ON path forces `mRadiancePass=false` at txn start. Ack requires `mInTxn` + irradiance done + epoch match + slot match. Records live inside the probe object. Monotonic `mId` for cursors. The upstream OFF-path stale-pass bug is reported, not fixed (§12). |
| CX P1-5 | Live/realtime records → false STARVED; overflow/resync must bump Live | §4.8 ownership. Live- and realtime-owned records take no hits and are excluded from stats. Every overflow, resync and refresh-all bumps `mLiveSceneSerial`, even with zero events. |
| CX P1-6 | D can drop emissive PBR (llvovolume.cpp:7466ff registers PBR into `PASS_GLTF_PBR*`) | §8. **All `PASS_GLTF_PBR*` groups are kept.** The allow-list is legacy opaque/masked non-emissive passes only. |
| CX P1-7 | Acceptance gate too thin; unrealistic latency | §10. Adds T15-T22: slicing, Live convergence under camera motion, deletion, allocation, toggling mid-capture, teleport, MaxAge=0, refresh-all. Latency is queue-based (§10 preamble). The default flip requires all of T0-T22. |
| CX P2 dyn | Dynamic policy misdescribed | §1 D3/D7 rewritten. Dynamic probes are always-eligible but queued with MinInterval ≥ 1 s between starts, not a fixed cycle. Closest-dynamic slicing is separate: a 3-frame pass at N=2. |
| CX P2 compile | `auto_adjust_legacy` declared outside the extracted block (:756); inline accessor vs `.cpp` static | §4.3: the helper declares its own cached control, and the now-unused one in `sampleCinematicH` is removed. §4.2: one `extern bool` defined once. |
| CX C1 | "near-worthless" unproved | §7.2 reworded. The saving is bounded by the traversal share. C0 measures that ceiling. Profitability is unproven either way, and C1 still needs user approval. |
| OP P0-1 | Camera-driven terrain stride / tree LOD / grass blades / tex-anim dirty every probe; Live never idles | §4.4 rows LT, LV, LG, TX. Scoped `TAG_LOD_*` covers llsurfacepatch.cpp:1054-1070, llvotree.cpp:378-381, llvograss.cpp:368-392 and llvovolume.cpp:665-681 / :876-894. Real heightfield edits stay events (`dirty()` :92, `dirtyPatch` :611, composition :878-884). Per-site drop counters (§6.4). Live counts only non-LOD static geometry/texture (§5.2). Test T8b shows On change idling during an orbit. |
| OP P1-1 | LOD tag misses `genBBoxes`→`movePartition` bounds updates | §4.4 LV. The whole `LLVOVolume::updateLOD` body (:1699-1737) and the whole `updateGeometry` for LOD-only rebuilds (:2157-2256) run under `TAG_LOD_VOLUME`, so H1b/H4 inside :1967 are tagged. |
| OP P1-2 | Ordinary probes use the main-eye `mNearbyLights` (pipeline.cpp:9727-9737, :23263-23267); attachment child lights untracked | §4.5. A flush-time diff of `mNearbyLights` uses a per-light sticky signature: the same fields as the Live light signature :9680-9713, plus a fade bucket. Light toggles go into the ordinary env tail (§4.3). H5 (`parameterChanged`) is **removed**: a light outside the list lights no ordinary capture, and Live hashes lights itself. |
| OP P1-3 | `recording()` file-static → per-TU copies | §4.2. `extern bool gProbeDirtyRecording` is defined once in alprobedirty.cpp and read by an inline accessor. |
| OP P1-4 | Texture streaming churn; projector gobo users | §4.4 H6. Notify on the first real image and on coarse refinements only: `newDiscard < noted && noted > Dp`, with `Dp = max(0, ⌊log2(maxFullDim/probeRes)⌋)`. `mVolumeList[LLRender::LIGHT_TEX]` users are dirtied through the light diff (§4.5). |
| OP P1-5 | Flush cost | §4.6 step 2c. E×P cap 16 384. Sphere test first; capsules only on a sphere miss for `C_STATIC` with shadows; a per-probe union-bound early-out. |
| OP P2 probe fields | R_PROBE: ambiance, near clip, dynamic, box/sphere; resync on detail change; light before probe-drop | §4.6 step 2d, §4.3 tail (`RenderReflectionProbeDetail`, `…Level`), §4.2 classify order. |
| OP P2 sources | Gobo override, no-shadow set, Rig Rim when `CineRigRimIncludeProbes` | Gobo and no-shadow are per-light fields of the light diff (§4.5). Rig Rim global settings and per-light `rigRimParams` go into the env tail when included (§4.3). |
| OP P2 shift | `shift()` should offset queued events | §4.4 SH. Queued events, the light snapshot, record origins and the default camera are offset. No overflow. |
| OP P2 txn | `onTxnComplete` no-op without txn; clear stale `mInTxn` on ON edge | §4.7. |
| OP P2 H4 | Say union(entry, exit); H1b mostly no-op for volumes | §4.4 H1b/H4. |
| OP P2 D | Emissive PBR | Same as CX P1-6. |
| OP P2 budget | OVER BUDGET structurally unreachable; STARVED routine at P ≳ 25 | §6.4 wording; §10 T4 expectation. |
| OP P2 tests | Rcap ≈ 131 m in T2; T6/T7 predictions | §10 T2/T6/T7 rewritten. |
| OP P2 anchors | settings.xml :20703-20713; Lightbox insertion after :1537 **before** the `<!--` at :1538 | §6.1, §6.2 (re-verified). |
| OP P2 button | "Refresh all probes now" | **Added** (§6.2, §4.6 step 2a). One click before a take guarantees fresh probes regardless of hook coverage. |

Nothing was rejected. Two items are **only partly fixable**; see §4.9 and the report:
(a) continuous appearance sources (texture animation, flexi, media, animated gobos, cloud scroll in ordinary probes)
remain safety/refresh-all only; (b) main-eye light-list membership changes while the camera moves are *real* ordinary-probe
changes, so they cannot be suppressed.

---

## 1. Decisions

| # | Decision | Why |
|---|---|---|
| D1 | `RenderProbeOnDemand` ships **false**. The flip to true is a separate one-line commit after **all** of §10 T0-T22 pass. | Coverage is proven only in-world. CX P1-7. |
| D2 | Batched event push → per-probe serials (R§2). No octree counters. | R§2 table. |
| D3 | **Dynamic ordinary probes are not dirty-gated.** A complete dynamic probe is always eligible, but it competes in the same 1-face/frame queue, and its starts are at least `RenderProbeMinInterval` apart. So it refreshes at a queue-dependent rate, never faster than about 1 start/s: not a fixed cycle. DYN-class (avatar/animesh/particle) *geometry* events are dropped. **Lights of any class** reach probes only through the nearby-light diff (§4.5). | Pose changes inside unchanged extents are unhookable cheaply (R§7). Static probes and Live never draw DYN classes (llviewerwindow.cpp:6090-6108; alcinelightrigmanager.cpp:575). |
| D4 | Ordinary budget stays **structurally 1 face per `update()`** (continuation :354-358 or start :610-620, gated by `did_update`). Realtime is ≤ 6 (Live FULL/budget, or sliced N). Total ≤ 7/frame, the same as today (OP Q&A). No FaceBudget setting. | "Never above today's rate" is guaranteed by structure. |
| D5 | Occluded complete ordinary probes are **deferred**: their dirt and real age are kept, and they are never forced. | R§4. `getReflectionMaps` skips occluded probes (:1089). |
| D6 | Live Probe: **no min-interval**. | Its per-frame bound is its mode. |
| D7 | Closest-dynamic realtime slicing is **always cycling** at N faces/frame, one pass per ⌈6/N⌉ frames (3 frames at N=2). It is separate from the D3 queue policy. | It draws avatars. |
| D8 | Continuous appearance sources are safety-only (§4.9). | Hooking them would keep probes permanently dirty. |
| D9 | **C1 needs user approval** (§7.2). C0 is built. | CLAUDE.md "say so before starting". |
| D10 | Capture far = `RenderReflectionProbeDrawDistance` (llviewerdisplay.cpp:224-228). Cull reach is far·√3 (llvieweroctree.cpp:1379-1382). `Rcap = far·√3 + 20 m` ≈ **131 m** at the 64 m default. | PROVES. |

---

## 2. Reconciliation with v1 §B / 615750a40f9 (unchanged from V2, anchors re-verified)
- DONE in 615750a40f9:
  - Live slicing: `updateCinematicBudget` :686-744, `advanceFace` alcineliveproberefresh.h:570-595.
  - Readiness: :650-669, :722-730.
  - Converge-to-idle with lighting/env/origin/settings H: `sampleCinematicH` :750-903.
  - Live scratch blocking: :566-570.
  - `mRadiancePass` / `mRealtimeRadiancePass` discipline: :546-557, :702-737.
- PARTIAL: same-probe ignored/pinned list edits (:1189-1190). Out of scope (rig-panel state).
- MISSING, delivered here: Live geometry/appearance source (§5.2); per-probe minimum interval (§4); closest-dynamic
  slicing (§5.1).
- NOT WANTED: Live dynamic-column sources (Live is static).
- Keep unchanged: the `CineLightRigLiveProbe*` settings, `ALCineLiveProbeRefresh::decide`, the rig-panel card and the
  `[LiveProbe]` log.

## 3. Scope of this ONE delivery
- **A:** ordinary + default probe on-demand scheduling (§4).
- **B-rest:** sliced closest-dynamic (§5.1) and the Live scene source (§5.2).
- **C0:** timing zones (§7.1). C1 only with approval.
- **D:** conservative tiny-group cull, off by default (§8).
- **Surfaces:** settings, UI (including Refresh all), presets, debug text, `[ProbeSched]` verdicts, TUT tests.
- **Deferred:** probe LOD bias (R§6: `calcLOD`/`updateLOD` mutate shared state, llvovolume.cpp:1485, :1699-1737), shared
  probe shadow maps, supersample factor, relightable probes, and fast GGX.

---

## 4. A — on-demand scheduling

### 4.1 Pure model `alprobeschedule.h` (header-only, no GL/viewer globals; `viewer_HEADER_FILES`)
Namespace `ALProbeSched` (all `inline`, `stdtypes.h`, explicit `static_cast`):
```cpp
enum Reason : U16 { R_GEOM=1<<0, R_TEX=1<<1, R_LIGHT=1<<2, R_ENV=1<<3, R_PROBE=1<<4, R_SAFETY=1<<5,
                    R_CLOUD=1<<6, R_RESYNC=1<<7, R_WARMUP=1<<8, R_DYN=1<<9 };
enum Class  : U8  { C_STATIC=1<<0, C_TERRAIN_WATER=1<<1, C_LIGHT=1<<2 };
struct Event { F32 mMin[3]; F32 mMax[3]; U16 mReason; U8 mClass; };        // agent space, owned values
enum class Owner : U8 { ORDINARY, LIVE, REALTIME };
struct Record {
    U32 mId = 0;                                   // monotonic, assigned by the manager, never reused (§4.7)
    U64 mDirtySerial = 0, mAckSerial = 0, mTxnSerial = 0;
    U32 mTxnEpoch = 0;  bool mInTxn = false;  bool mTxnIrrDone = false;  S32 mTxnCube = -1;
    U16 mPending = 0, mTxnReasons = 0, mLastReasons = 0;
    F64 mFirstDirty = -1.0, mFirstDirtyInTxn = -1.0, mLastStart = -1.0, mLastComplete = -1.0;
    // capture state at the last start (R_PROBE comparison, §4.6 step 2d)
    F32 mStartOrigin[3] = {0,0,0}; F32 mStartRadius = 0.f; F32 mStartAmbiance = 0.f; F32 mStartNear = 0.f;
    bool mStartDynamic = false; bool mStartBox = false; S32 mStartCube = -1;
    F32 mStartCam[3] = {0,0,0}; F32 mStartCloud[2] = {0,0};             // default probe only
    Owner mOwner = Owner::ORDINARY;
};
inline bool dirty(const Record& r) { return r.mDirtySerial > r.mAckSerial; }
inline void hit(Record& r, U16 reason, U64 serial, F64 now);
inline void onTxnStart(Record& r, U64 serial, U32 epoch, F64 now, U16 reasons, S32 cube /*+ capture state*/);
inline void onTxnIrradianceDone(Record& r, U32 epoch);                 // sets mTxnIrrDone iff mInTxn && epoch match
inline bool onTxnComplete(Record& r, U32 epoch, S32 cube, F64 now);    // §4.7 contract; returns acked
inline void clearTxn(Record& r);                                       // mInTxn=false, mTxnIrrDone=false
struct Policy { F32 mMinInterval; F32 mMaxAge; };
struct View   { bool mComplete, mOccluded, mDynamic, mAllocated; };
enum class Why : U8 { NONE, WARMUP, DIRTY, SAFETY, DYNAMIC, WAIT_INTERVAL, DEFERRED };
inline Why evaluate(const Record& r, const View& v, const Policy& p, F64 now);
```
- `hit`:
  - sets `mDirtySerial = max(mDirtySerial, serial)` and `mPending |= reason`;
  - sets the first-dirty time: `mFirstDirtyInTxn` if `mInTxn`, else `mFirstDirty` (only when it is still < 0).
- `evaluate` checks in this exact order:
  1. `!mAllocated` → NONE.
  2. `!mComplete` → WARMUP.
  3. `mOccluded` → DEFERRED if dirty or safety-due, else NONE.
  4. `mLastStart >= 0 && now - mLastStart < mMinInterval` → WAIT_INTERVAL if dirty, dynamic or safety-due, else NONE.
  5. `dirty` → DIRTY.
  6. `mDynamic` → DYNAMIC.
  7. safety-due (`mMaxAge > 0 && mLastComplete >= 0 && now - mLastComplete >= mMaxAge`) → SAFETY.
  8. Otherwise NONE.
- Eligible means WARMUP, DIRTY, DYNAMIC or SAFETY.
- The header also holds: `hitsCapture` (§4.6 step 2c), `SliceCursor` (§5.1), window stats and `verdict()` (§6.4).

### 4.2 Recorder `alprobedirty.{h,cpp}` (new; main thread only)
```cpp
extern bool gProbeDirtyRecording;                      // DEFINED ONCE in alprobedirty.cpp (OP P1-3, CX P2)
namespace ALProbeDirty {
    inline bool recording() { return gProbeDirtyRecording; }
    void setRecording(bool on);  void setProbeResolution(U32 res);
    enum Tag : U8 { TAG_NONE, TAG_LOD_VOLUME, TAG_LOD_TERRAIN, TAG_LOD_TREE, TAG_LOD_GRASS, TAG_TEXANIM, TAG_COUNT };
    struct ScopedTag { explicit ScopedTag(Tag); ~ScopedTag(); };   // nests (innermost wins); restores previous
    void noteDrawable(LLDrawable* d, U16 reason);                 // tagged → dropped + counted per tag
    void noteDrawableBounds(LLDrawable* d, const LLVector4a* mn_mx_old, U16 reason);  // union(old, current)
    void noteTexture(LLViewerFetchedTexture* t, S32 new_discard);  // §4.4 H6
    void shiftQueued(const LLVector4a& offset);                    // §4.4 SH
    void markOverflow();
    struct Drain { std::vector<ALProbeSched::Event> mEvents; bool mOverflow;
                   U32 mDropped[TAG_COUNT]; U32 mDroppedDyn, mNoopMove, mRaw; };
    void drain(Drain& out);
}
```
- **Storage.** One file-scope frame vector plus a `std::unordered_map<const void*, U32>` coalescing index, both in
  alprobedirty.cpp. The key is never dereferenced after the note returns. Union AABBs; OR reasons and classes. The cap is
  4096 coalesced events; beyond that, `mOverflow = true` and nothing more is stored.
- **`classify`, in this order.**
  1. HUD attachment → drop.
  2. `getAvatar() != nullptr`, animated object, or `LL_VO_PART_GROUP` → DYN → drop, `mDroppedDyn++`. Lights are not
     classified here (§4.5), so the "test LIGHT first" concern disappears.
  3. `isReflectionProbe()` → drop.
  4. Surface patch / water / void water pcodes → `C_TERRAIN_WATER`.
  5. Otherwise → `C_STATIC`.
- **`worldBounds`.** If the drawable's partition `asBridge()` (llspatialpartition.h:544) → the bridge's world extents
  (lldrawable.cpp:1328-1370). Else `getSpatialExtents()`. If there is no group or the value is non-finite → a sphere
  around `getPositionAgent()` of radius 0.5·|scale| + 0.5.
- Hooks are read-only: no hook writes render, drawable, group or pipeline state.

### 4.3 Environment generation (reuses the Live signature)
- **Extract** `sampleCinematicH` sections 3-5 (:777-892) **verbatim** into a file-static
  `appendEnvironmentSignature(Signature& sig, F32 sun_moon_deg, bool blend_only_when_mixing)`.
  - **Edits (exactly these):**
    1. The literals `0.1f` at :786-787 become `sun_moon_deg`.
    2. :817 and :846 become `(blend_only_when_mixing && !mixing) ? 0.f : blend`. For the sky, `mixing` means any
       sun/moon/cloud-noise current ≠ next id. For water, it means normal-map or transparent current ≠ next id.
    3. The helper declares its own `static LLCachedControl<bool> auto_adjust_legacy(gSavedSettings,
       "RenderSkyAutoAdjustLegacy", false)`. That is the same control as :756, used at :810 and :892. The now-unused
       declaration at :756 is removed.
  - Field count and order are unchanged. Live calls `(0.1f, false)`, so its H stays bit-identical (the review uses a
    moved-code diff).
- **Ordinary env** (members `Signature mOrdEnvSig; StickyHash mOrdEnvSticky; U64 mOrdEnvH; bool mOrdEnvValid`):
  - `appendEnvironmentSignature(sig, RenderProbeDirtySunDeg, true)`, then a fixed-layout **tail**:
    - `addAbs(RenderReflectionProbeDrawDistance, 0.5f)`;
    - exact `RenderReflectionProbeDetail`, `RenderReflectionProbeLevel`;
    - light toggles, exact: `RenderAttachedLights`, `BDMergeLightToggles`, `BDMergeRenderOwnAttachedLights`,
      `BDMergeRenderOthersAttachedLights`, `BDMergeRenderWorldLights`, `BDMergeRenderProjectors` (names as read at
      pipeline.cpp:9561-9576), plus `addAbs(AlchemyGlobalLightScale, 1e-3f)`;
    - Rig Rim, exact `CineRigRimIncludeProbes` and `CineRigRimEnabled`. When both are true, append in this fixed order:
      `CineRigRimMasterGain`, `CineRigRimBackSoftness`, `CineRigRimRoughnessSoften`, `CineRigRimShadow`,
      `CineRigRimTronMix` (Abs 1e-3); `CineRigRimTint` (addAbs3 1/512); exact `CineRigRimDebugRimOnly`,
      `CineRigRimIncludeAlpha`, `CineRigRimTronMode`, `CineRigRimTronColorSource` (alcinerigrim.cpp:131-245). Then, for
      each of `ALCineLightRigManager::SLOT_COUNT` slots, exact `isSlotEnabled(slot)`; for enabled slots, each light
      i < `LIGHT_COUNT` gets `addAbs` ×4 of `at(slot).rigRimParams(i)` at 1e-3 (alcinelightrig.h:229, read-only). When
      Rig Rim is excluded, append the same number of placeholders so the layout is stable. Placeholder counts come from
      the constants, not from the live state.
  - Implementer: verify every setting's type in settings.xml. A wrong-type `LLCachedControl` asserts.
  - `h != mOrdEnvH && mOrdEnvValid` → R_ENV hits all **ORDINARY-owned** allocated records (default included).
- **Rate (INFERENCE, R§3):** a 4 h day cycle at 0.5° gives ≈ one R_ENV every 20 s, which is ≈ 0.6 faces/s per probe.

### 4.4 Hook sites (each wrapped in `if (ALProbeDirty::recording()) { … }`; nothing else is executed when off)
| ID | Site (HEAD) | Behaviour |
|---|---|---|
| H1a | lldrawable.cpp:806 `updateMoveUndamped` and :842 `updateMoveDamped` | Before `updateXform` (:808 / :846), snapshot `mXform.getPosition()`, `mXform.getRotation()`, `mCurrentScale` and a copy of `getSpatialExtents()`. After it, if the position moved > 1e-4 m, `1 − |dot(rot)|` > 1e-7, or the scale changed > 1e-4 → `noteDrawableBounds(this, old, R_GEOM)` (union of old and current). **Rotation-only changes are caught** (CX P1-3). |
| H1b | llspatialpartition.cpp:972 `move` | After the null check (:977-981), snapshot `worldBounds`. On all 3 exits (:991-992, :1003-1004, :1014) note union(old, new) `R_GEOM` if they differ by > 1e-4 m, else `mNoopMove++`. Use an RAII helper. For volumes this is usually a no-op, because `genBBoxes` updated the extents before `movePartition` (llvovolume.cpp:1967); H4 covers that case (OP P2). |
| H2 | llspatialpartition.cpp:774 `handleInsertion`, :781 `handleRemoval` (before `removeObject`) | Membership, `R_GEOM`. |
| H3 | pipeline.cpp:4517 `markTextured` (inside its `if`) → `R_TEX`; :4580 `markRebuild(LLDrawable*,flag)` (inside its `if`) → `R_GEOM` iff `flag & ~LLDrawable::REBUILD_POSITION` | The group overload :4566 is not hooked. Tags apply. |
| H4 | llvovolume.cpp:2157 `updateGeometry` | At entry: `lod_only = mLODChanged && !mVolumeChanged && !mFaceMappingChanged && !mSculptChanged && !mColorChanged && !mDrawable->isState(LLDrawable::REBUILD_POSITION)`; if `lod_only`, open `ScopedTag(TAG_LOD_VOLUME)` for the **whole** function (covers `genBBoxes`→`movePartition` :1967, OP P1-1). After the `mVolumeImpl` early branch (:2169-2177, flexi, un-hooked by D8), snapshot `worldBounds` and `content = !lod_only && (the four flags)`. Before `return` at :2255, if `content` or bounds changed → note the **union of entry and exit** bounds, `R_GEOM`. |
| LV | llvovolume.cpp:1699-1737 `updateLOD` | The whole body under `ScopedTag(TAG_LOD_VOLUME)`. This covers the `markRebuild` at :1721 and `markPartitionMove` at :1730. |
| LT | llsurfacepatch.cpp:1054-1070 (render-stride change block in `updateVisibility` :1008) | The whole `if` block under `ScopedTag(TAG_LOD_TERRAIN)`. `dirtyGeom` (llvosurfacepatch.cpp:781-790) runs `markRebuild` + `movePartition` inside it. Real edits are NOT tagged: `LLSurfacePatch::dirty()` :92, `dirtyPatch()` :611 (llvosurfacepatch.cpp:767-779), composition :878-884. |
| LTr | llvotree.cpp:378-381 (`trunk_LOD != mTrunkLOD` branch in `idleUpdate` :345) | `ScopedTag(TAG_LOD_TREE)` around that `markRebuild` only. First build :374-377 and move/rotate :389-403 stay events. |
| LG | llvograss.cpp:368-392 (two blade-count branches of `updateLOD` :338) | `ScopedTag(TAG_LOD_GRASS)`. The tree-rendering-stopped toggle :347-354 stays an event (a setting-driven content change). |
| TX | llvovolume.cpp:665-681 (matrix creation in `animateTextures` :616) and :876-894 (size-threshold toggle in `updateTextureVirtualSize` :771) | Each block under `ScopedTag(TAG_TEXANIM)`. The animation start `markTextured` :629 and the stop paths :432/:536 stay events (discrete edits). |
| H6 | llviewertexture.cpp:2121 `postCreateTexture`, after the sculpt loop (:2135-2143), main thread | New member `S8 mProbeNotedDiscard = -1` (llviewertexture.h, LLViewerFetchedTexture). `d = getDiscardLevel()` (llgltexture.h:143); `Dp = max(0, ⌊log2(max(getFullWidth(), getFullHeight()) / probeRes)⌋)`. Notify iff `mProbeNotedDiscard < 0` (first real image) or `d < mProbeNotedDiscard && mProbeNotedDiscard > Dp`; then store `min(noted, d)` (or `d` when first). Notification fans out over `getFaceList(ch)` for `ch < LLRender::NUM_TEXTURE_CHANNELS` → `noteDrawable(face->getDrawable(), R_TEX)`. More than 4096 faces → one "all ORDINARY static" event, not overflow. Downgrades never notify. Zone `"probe dirty tex fanout"`. `LIGHT_TEX` users are handled by §4.5. |
| SH | llreflectionmapmanager.cpp:1562 `shift()` | `ALProbeDirty::shiftQueued(offset)`. Also offset the light-diff snapshot positions (§4.5), every record's `mStartOrigin` and `mStartCam`, and `mRtSliceFrozenOrigin`. **No overflow** (OP P2). |
- **Removed from V2:** H5 (`parameterChanged` light hook) and light inflation of move events. They are superseded by
  §4.5 (OP P1-2).
- Environment is sampled (§4.3), not hooked.
- Material/PBR edits reach H3 through `setTE*` and the override paths (R§2). Uninstrumented async material completion is
  safety-covered.

### 4.5 Nearby-light diff (ordinary + default probes; OP P1-2, CX P1-3)
- **Why.** Ordinary probe captures do not rebuild the light list: pipeline.cpp:9727-9737 returns early for
  non-cinematic cube snapshots ("Normal probes deliberately reuse the main-eye list"). Deferred probe lighting then
  iterates the main-eye `mNearbyLights` (:23263-23267). That list already applies camera distance, the
  `RenderLocalLightCount` cap, attached/BDMerge toggles, muted/too-complex avatars and fade (:9926-9985). So it is
  exactly the set of local lights any ordinary capture can see.
- **Accessor.** pipeline.h adds a public
  `void getProbeNearbyLights(std::vector<std::pair<LLDrawable*, F32>>& out) const` (drawable plus fade). The list holds
  `LLPointer<LLDrawable>` (pipeline.h:1828-1850), so reading it during the flush is safe.
- **State.** Manager `std::unordered_map<const void*, LightEntry> mLightSnap`, where
  `LightEntry{ F32 mPos[3]; ALCineLiveProbeRefresh::StickyHash mSticky; U64 mH; U32 mSeen; }`.
- **Per light, each flush:**
  1. Build a small `Signature` with the **same fields and tolerances as pipeline.cpp:9680-9713**: id, position (0.05 m),
     colour × global scale × `ALEnvIntensity::localLightEVScale` (Col 0.01/1e-3), radius, falloff, scale, forward/up
     angles (0.25°), spotlight flag, spot params, light texture id, `isProjectorNoShadow`, and all `getGoboOverride`
     fields.
  2. Add a **fade bucket** `addExact(floor(4·clamp(fade/LIGHT_FADE_TIME, -1, 1)))` (`LIGHT_FADE_TIME` = 0.2 s, lllightconstants.h:31; a fade
  therefore yields at most ~8 events per light, all within 0.2 s).
  3. Take `h = mSticky.update(sig)`. A new entry, or `h != mH`, emits a `C_LIGHT` `R_LIGHT` event: a sphere of radius
     `LIGHT_MAX_RADIUS` (llprimitive.h:127) at the new position, plus one at the old position if it moved.
  4. Entries not seen this flush emit an event at their stored position and are erased.
- **What this covers:** attachment child-prim lights, projector direction, gobo override, the no-shadow set, fade in/out,
  toggles and a projector texture arriving. The texture arrives with the same id; the diff tracks it through the texture
  id plus `isLight`, and H6's fan-out does not need `LIGHT_TEX`. The coarse-refinement rule of H6 is therefore not
  applied to gobos (INFERENCE; a gobo that sharpens later is safety-only).
- **Hysteresis.** The per-light sticky hash stops float jitter from producing events.
- **Scope.** Live and REALTIME-owned records ignore these events (Live hashes its own light set, :774-775).
- **Cost:** ≤ 256 lights × ~45 floats; zone `"probe sched lights"`.
- **Camera-driven membership.** As the camera moves, the list membership changes (far clip, count cap, fade). These are
  **real** changes to what ordinary probes render, so they stay events, but local ones (radius 20 m spheres). Counter
  `nl=` in the log.
- The snapshot is cleared by `resetProbeSchedule` and on the ON edge; the first ON flush then treats every light as new,
  which is harmless because the ON edge resyncs anyway.

### 4.6 Manager integration (`llreflectionmapmanager.{h,cpp}`, `llreflectionmap.h`)
- `llreflectionmap.h`: `#include "alprobeschedule.h"` and a public member `ALProbeSched::Record mSched;` after :138.
- **Members:** `U64 mSchedSerial; U32 mSchedEpoch; U32 mSchedNextId; bool mSchedWasOn; bool mRefreshAllRequested;
  U64 mLiveSceneSerial;` plus the env members (§4.3), `mLightSnap`, `ALProbeSched::SliceCursor mRtSlice`,
  `LLVector4a mRtSliceFrozenOrigin`, `SchedWindow mSchedWin`, `SchedFrame mSchedFrame`.
  - **Public:** `void requestRefreshAllProbes();` sets `mRefreshAllRequested` and calls
    `requestCinematicLiveProbeRefresh()`.
  - **Static:** `ProbeCaptureKind sCaptureKind` (§8).
- **`update()` edits:**
  1. Top, before :212: `static LLCachedControl<bool> on_demand(gSavedSettings, "RenderProbeOnDemand", false);`
     `ALProbeDirty::setRecording(on_demand && LLPipeline::sReflectionProbesEnabled);`
     `ALProbeDirty::setProbeResolution(mProbeResolution);` `mSchedFrame = {};`
     The early returns :214, :223 and :339 also call `mRtSlice.reset()`.
  2. After :341, before :344: `flushProbeSchedule(on_demand)` (zones `"probe sched flush"`, `"probe sched env"`,
     `"probe sched lights"`). When off: drain and discard, clear `mLightSnap`, set `mSchedWasOn=false`, return. When on:
     - a. `frame_serial = ++mSchedSerial`. Assign `mSched.mId = ++mSchedNextId` to any record with id 0. Compute owners
       (§4.8).
       - An ownership change to ORDINARY → `R_RESYNC` hit.
       - The ON edge (`!mSchedWasOn`), overflow, or `mRefreshAllRequested` → `R_RESYNC` on every allocated ORDINARY
         record, `clearTxn` on all records, `++mLiveSceneSerial`, clear the request.
       - Grace-init `mLastComplete = now` for complete probes with `mLastComplete < 0`.
     - b. Drain the events and run the light diff (§4.5).
     - c. **Hit test** (`ALProbeSched::hitsCapture`):
       - If `E × P_alloc > 16384`, treat as overflow (step a semantics).
       - Else, per probe, first check the union AABB of all events against the probe sphere expanded by the sun sweep.
         On a miss, skip the probe.
       - Per event:
         - `C_TERRAIN_WATER` always hits (llvowater.cpp:284 and llvosurfacepatch.cpp:984 are infinite-far).
         - `C_LIGHT`: sphere vs sphere(O, Rcap).
         - `C_STATIC`: AABB vs sphere(O, Rcap). **Only on a miss**, and only if `RenderShadowDetail > 0`, test the event
           sphere swept 256 m along −sun and −moon (capsule) within `r + Rcap`.
       - Class masks:
         - ordinary: `C_STATIC|C_TERRAIN_WATER|C_LIGHT`;
         - default: `C_TERRAIN_WATER|C_LIGHT` (its render mask is at :1319-1321);
         - Live: `C_STATIC|C_TERRAIN_WATER`, non-light events only (§5.2).
       - `hit(rec, reason, frame_serial, now)` for ORDINARY-owned records. A Live footprint hit →
         `++mLiveSceneSerial` (at most once per frame).
     - d. **R_PROBE per ORDINARY non-default probe:** compare against the `mStart*` fields. Hit if:
       - origin moved > 0.1 m;
       - radius changed > 0.1 m;
       - `getAmbiance()` changed > 0.01;
       - `getNearClip()` changed > 0.01 m;
       - `getIsDynamic()` or box/sphere (`mViewerObject && getReflectionProbeIsBox()`) differ;
       - `mCubeIndex != mStartCube`.
     - e. **Default probe extras:**
       - `R_PROBE` if the **camera** origin (`LLViewerCamera::getOrigin()`) moved > 16 m from `mStartCam`. Camera is
         compared to camera, never to the +64 m capture origin (CX P1-1).
       - `R_CLOUD` if `LLEnvironment::getCloudScrollDelta()` (llenvironment.h:182) moved > 1e-4 from `mStartCloud` and
         `now - mLastStart >= RenderDefaultProbeUpdatePeriod`.
     - f. Ordinary env (§4.3). Set `mSchedWasOn = true`.
  3. **Selection** :471-477: add `&& (!on || (owner == ORDINARY && eligible(evaluate(...))))`. The
     `mOccluded && mComplete` branch :458-468 is unchanged (D5).
  4. **Default probe rule** :596-607. When off, verbatim. When on, with `e = eligible(default)`:
     - Level 0: `oldestProbe = (e && age >= period) ? mDefaultProbe : nullptr` (age as today, from `mLastUpdateTime`).
     - Level > 0: if `e && now - rec.mLastStart >= period`, then `oldestProbe = mDefaultProbe`; else if `!e &&
       oldestProbe == mDefaultProbe`, then `oldestProbe = nullptr`.
  5. **Txn start** :610-620, when on:
     - `mRadiancePass = false;` (CX P1-4; ON path only).
     - `onTxnStart(rec, mSchedSerial, mSchedEpoch, now, reasons, probe->mCubeIndex, …capture state…)`, where reasons =
       `mPending | WARMUP | SAFETY | DYN` as applicable. For the default probe, also record the camera and the cloud
       delta.
  6. The `oldestOccluded` block :622-627 is unchanged. Safety uses `mLastComplete` (R§4).
  7. **Counters, at call sites only:**
     - `++mSchedFrame.mOrdFaces` after `doProbeUpdate()` at :357 and :619;
     - `+6` after `updateRealtimeProbeAllFaces` at :555 and :592;
     - the `mCineStats.mFaces` delta around :573;
     - sliced faces (§5.1).
     - At the end of `update()`, fold the frame into `mSchedWin`, then call `logProbeSchedule`.
- **`doProbeUpdate` (:1256-1288).**
  - Irradiance end (:1279-1282): `if (mSchedWasOn) onTxnIrradianceDone(rec, mSchedEpoch)`.
  - Radiance end (:1273-1277), before `mUpdatingProbe = nullptr`:
    `if (mSchedWasOn) onTxnComplete(rec, mSchedEpoch, probe->mCubeIndex, now)`.
  - Debug text (:1267-1270), when on: `"%.1f %s"` with the letters of `mLastReasons`: G T L E P S C R W D. When off the
    string is byte-identical.
- **`resetProbeSchedule()`:**
  - `++mSchedEpoch`; `clearTxn` on all records; clear `mLightSnap`, `mRtSlice` and the env sticky; `mSchedWasOn = false`
    (the next ON frame resyncs).
  - Called from the `initReflectionMaps` reset block (:2012-2019) and `cleanup()` (:2078-2086).
- **`deleteProbe`** (:1227-1253): if the deleted probe is `mUpdatingProbe`, `++mSchedEpoch` is *not* needed: the record
  dies with the object. The stale `mRadiancePass` (not reset at :1238-1242) is neutralised by step 5 on the ON path.

### 4.7 Transaction contract (CX P1-4, OP P2)
- `onTxnStart` sets `mInTxn=true`, `mTxnIrrDone=false`, `mTxnSerial`, `mTxnEpoch`, `mTxnCube` and `mLastStart`, and
  copies the capture state.
- `onTxnIrradianceDone` sets `mTxnIrrDone` only if `mInTxn && mTxnEpoch == epoch`.
- `onTxnComplete` returns false and does nothing unless all of these hold: `mInTxn && mTxnIrrDone && mTxnEpoch == epoch
  && mTxnCube == cube`.
  - When they hold: `mAckSerial = max(mAckSerial, mTxnSerial)`, `mLastComplete = now`, `mLastReasons = mTxnReasons`.
    Then: if `!dirty` → `mPending = 0; mFirstDirty = -1`, else `mFirstDirty = mFirstDirtyInTxn`. Finally `clearTxn`.
  - When they fail: `clearTxn` and a `R_RESYNC` hit, so the probe is redone correctly.
- **Changes during a capture survive:** their serial is greater than `mTxnSerial`.
- **Toggle mid-capture:**
  - ON→OFF: completion is skipped (`mSchedWasOn` is false).
  - OFF→ON: `mInTxn` is false, so no ack, and the ON-edge resync redoes the probe.
- **Identity:**
  - Records live inside each `LLReflectionMap`, so a new object at a reused address gets a fresh record.
  - Cursors key on `mSched.mId`, which is monotonic, plus the cube index.
- The fake `oldestOccluded` update never touches `mLastComplete`.

### 4.8 Scheduler ownership (CX P1-5)
- Owner is **LIVE** if `probe == mCinematicLiveProbe`, **REALTIME** if its id is `mRtSlice`'s id while the sliced path
  ran last frame, else **ORDINARY**.
- Non-ORDINARY records take no hits, are never selected by the ordinary loop, and are excluded from `dirty`, `deferred`,
  `maxlag` and STARVED.
- On a transition back to ORDINARY → `R_RESYNC`.
- The Live serial is bumped by overflow, resync, refresh-all, and Live-footprint non-light events.

### 4.9 Safety-refresh (MaxAge) / Refresh-all only — stated in the tooltip and the log header
Two kinds of change are not caught by the dirty hooks:
- **Continuous** (they would keep probes permanently dirty): texture animation appearance, flexi motion, media frames,
  animated gobos (GOBO_TIME), cloud scroll in non-default probes, and water waves.
- **Discrete but unhooked:**
  - rotation inside an unchanged AABB of a drawable that never gets `updateMove` (e.g. a child moved by the parent's
    bridge);
  - non-volume rebuilds landing a frame late;
  - async PBR completion without a `setTE*`;
  - probe-to-probe bounce;
  - reverted transients;
  - sharpening beyond `Dp`.

`RenderProbeMaxAge = 0` removes this backstop entirely (tooltip warns). The **Refresh all probes now** button is the
pre-take guarantee.

---

## 5. B-remainder

### 5.1 Closest-dynamic realtime slicing (`RenderProbeOnDemand`, no Live Probe)
- **Dispatch** in the non-Live branch (:583-594): if `on && realtime_probe && realtime_probe != cinematicLive && N < 6`
  → `updateRealtimeSliced(realtime_probe, N)`, else the unchanged call. N = clamp(`RenderProbeRealtimeFacesPerFrame`,
  1, 6). If not sliced this frame → `mRtSlice.reset()`.
- **`SliceCursor`** `{U32 mId; S32 mCube; bool mActive; U8 mFace; bool mRadiance; bool mLastPassIrr;}`:
  - `begin(id, cube)`: an identity change resets, and the next pass starts at face 0;
  - `startIfIdle()`: `kind = mLastPassIrr ? radiance : irradiance`, the same rule as alcineliveproberefresh.h:483,
    :539, :556;
  - `advance()`.
  - alcineliveproberefresh.h itself is not edited.
- **`updateRealtimeSliced`** (zone `"rmmu - rt sliced"`, ZONE_NUM faces):
  - If `probe == mUpdatingProbe` → skip the frame, keep the cursor, `++rt_blocked`.
  - Freeze the origin at face 0 and restore it after.
  - Per face: `mRadiancePass = cursor.mRadiance`, then `updateProbeFace(probe, face)`. No `force_dynamic`, no cinematic
    flags, no readiness writes and no `mComplete` writes; ordinary warm-up owns `mComplete`.
  - Restore `mRadiancePass`. `updateNeighbors` after a pass end. `mRealtimeRadiancePass` is untouched.
- **Invariant:** every secondary-scratch pass starts at face 0 and writes faces 0-5 before the face-5 publication.
  Live and sliced passes are mutually exclusive per frame, and each resets whenever the other runs.

### 5.2 Live Probe scene source
- In `sampleCinematicH`, after section 5 (:892) and before :895:
  `if (ALProbeDirty::recording()) sig.addExact(mLiveSceneSerial);`. When recording is off, H is bit-identical to
  615750a40f9.
- The Live serial counts only `C_STATIC|C_TERRAIN_WATER` non-light, **non-tagged** events in the Live footprint, plus
  overflow, resync and refresh-all. Light changes are already in Live's own light signature (:774-775).
- **Why On change still idles while the camera orbits a loaded static set (OP P0-1):**
  - Terrain stride, tree/grass/volume LOD and tex-anim toggles are tagged and dropped.
  - Texture refinements beyond `Dp` are never notified. `mProbeNotedDiscard` keeps the lowest level noted, so a
    downgrade followed by a re-sharpen does not notify again.
  - Main-eye light-list changes never reach Live.
  - Remaining Live bumps under camera motion come only from first arrivals or coarse refinements of textures in the
    Live footprint. These are real content changes. Test T8b checks this.
- No min-interval (D6). The watchdog is unchanged.

---

## 6. Settings, UI, presets, debug, log, Tracy

### 6.1 Settings: `app_settings/settings.xml`, insert after `RenderDefaultProbeUpdatePeriod` (:20703-20713)
| Name | Type | Default | Clamp in code | Persist |
|---|---|---|---|---|
| RenderProbeOnDemand | Boolean | 0 | — | 1 |
| RenderProbeMinInterval | F32 | 1.0 | 0..10 s | 1 |
| RenderProbeMaxAge | F32 | 60 | 0 = off, else 5..600 s | 1 |
| RenderProbeDirtySunDeg | F32 | 0.5 | 0.1..5 | 1 |
| RenderProbeRealtimeFacesPerFrame | S32 | 2 | 1..6 | 1 |
| RenderProbeTinyCullPixels | F32 | 0 | 0..8 | 1 |
| RenderProbeSchedLog | Boolean | 0 | — | 0 |
| RenderProbeSharedCull | Boolean | 0 | only if C1 approved | 1 |
- 7 keys, or 8 with C1. Count them after editing.
- Each key has a Comment giving its meaning and range.
- Why MinInterval is 1.0 s: a 12-face refresh takes about 12 frames, so this caps a perpetually dirty probe at about 20 %
  of frames at 60 fps (INFERENCE).

### 6.2 UI
- **Lightbox → Rendering** (floater_lightbox_settings.xml):
  - **Insert immediately after :1537** (the `</button>` closing `reset_RenderHeroProbeUpdateRate`) and **before :1538**
    (`<!--Probe Draw Distance-->`). The commented-out block runs :1539-1576 and must not swallow the new XML (OP P2).
  - Use the idioms of :1481-1537.
  - Controls:
    - check "Update probes on demand" (`RenderProbeOnDemand`);
    - sliders with spinners and `LightBox.ResetControlDefault` buttons: "Safety refresh (s)" 0-600, "Min interval
      (s)" 0-10, "Sun change (deg)" 0.1-5, "Realtime faces/frame" 1-6, all with
      `enabled_control="RenderProbeOnDemand"`;
    - combo "Probe tiny cull": Off 0 / Light 1 / Strong 3;
    - check "Log probe schedule";
    - **button "Refresh all probes now"** with commit callback `LightBox.RefreshAllProbes`, registered in
      alfloaterlightbox.cpp next to :67-77, calling `gPipeline.mReflectionMapManager.requestRefreshAllProbes()`.
      Enabled only with on-demand; when off, the round-robin already refreshes everything.
  - Raise `render_settings_scroll_content` (:1320, height 566) by the rows added (count them).
  - Tooltips:
    - on-demand: one sentence on §4.9;
    - MaxAge: "0 disables the backstop";
    - realtime: "Realtime detail without a Live Probe only";
    - tiny cull: "skips whole groups of small opaque non-PBR surfaces".
- **Graphics → Advanced** (floater_preferences_graphics_advanced.xml):
  - One check "Update reflection probes on demand" after the `ProbeCount` combo (:954-983), `left="420"
    top_delta="22"`, `Pref.RenderOptionUpdate` as at :860.
  - Verify by summing the `top_delta`s that the Tonemap slider (:1178-1196) still ends above `vram_status_border` (top
    480). If not, omit the row and report it.
- The rig-panel card is unchanged.

### 6.3 Presets / reset
- llpresetsmanager.cpp `getGraphicsControlNames`: insert the 6 non-log, non-C1 names **before**
  `"RenderReflectionProbeDetail"` (:342), each with a trailing comma.
- Do not touch :343. It has the upstream missing comma, reported in §12.
- Per-control reset buttons live in the Lightbox block.

### 6.4 `[ProbeSched]` log (`RenderProbeSchedLog`, on and off paths; one line per 5 s of frames the manager ran)
```
[ProbeSched] mode=on|off probes=<alloc ordinary> faces=<ord/s> rt=<rt/s> starts=<n> done=<n> nack=<n>
 g t l e p s c r w d=<txn starts by reason> events=<coalesced> raw=<n> nl=<light-diff events>
 drop_lodvol drop_lodterr drop_lodtree drop_lodgrass drop_texanim drop_dyn noop_move overflow refresh_all
 dirty=<now> deferred=<now> wait=<now> maxlag=<s> rt_blocked=<n> env_changes=<n> flush_us=<avg/max>
 verdict=<OK|STARVED n=<k> worst=<s>|OVER BUDGET ord=<max>/1 rt=<max>/<allow>|OFF-BASELINE|PAUSED>
```
- `nack` counts completions refused by the §4.7 contract.
- All counts and verdicts cover **ORDINARY-owned** records only.
- **Verdicts:**
  - **OVER BUDGET:** a frame with ord > 1, rt > 6, or a sliced frame with rt > N. It is structurally unreachable if the
    code matches §1 D4 and §5.1, so it is an **implementation-bug detector only**; it must never fire.
  - **STARVED:** a non-occluded eligible ORDINARY probe pending > 5 s + MinInterval. **Expected, not a failure**, whenever
    a global change (R_ENV, resync) hits P probes with `12·P / fps > 5 s`, i.e. **P ≳ 25 at 60 fps**. This happens
    routinely during a running day cycle in probe-dense scenes.
  - **PAUSED:** the manager early-returned or paused for > 50 % of the window (the measurement is untrustworthy).
  - **OFF-BASELINE:** on-demand is off.
  - **OK:** otherwise.
- Priority: OVER BUDGET > PAUSED > STARVED > OK. The window restarts after a gap > 0.5 s (same rule as :974-984).

### 6.5 Tracy
- **New zones:**
  - `"probe sched flush"`, `"probe sched env"`, `"probe sched lights"`, `"probe dirty tex fanout"`;
  - `"rmmu - rt sliced"` (ZONE_NUM faces);
  - `"probe tiny cull"` (ZONE_NUM rejected);
  - the C0 zones.
- The existing zones are unchanged and remain the timing truth.
- There are no zones inside hooks H1-H4 or in the tag scopes.

---

## 7. C — shared cull
### 7.1 C0 (built)
- llviewerdisplay.cpp `display_cube_face` (:1290-1365) gets scoped zones:
  - `"cube face cull"` around :1307;
  - `"cube face shadow"` around :1312;
  - `"cube face sort"` around :1316-1330;
  - `"cube face geom"` around :1353-1357;
  - `"cube face light"` around `renderDeferredLighting`.
- These are zones only; no code moves.

### 7.2 C1 (`RenderProbeSharedCull`): needs user approval
- **Exact form.** `LLOctreeCull`'s node test is `min(frustum, sphere(origin, mFrustumCornerDist))`
  (llspatialpartition.cpp:1062-1070). It is hierarchical: llvieweroctree.cpp:1345-1370, `SKIP_FRUSTUM_CHECK` at
  :1354-1355, `checkObjects`/`visit` at :1468-1507.
- **Why it is pixel-identical.** Group extents nest. So a DFS-ordered cache of nodes that intersect the union sphere,
  re-filtered per face with the identical tests, produces identical `sCull` contents and order, provided that:
  - the parent-result rule for SKIP groups is kept;
  - `markNotCulled` stays per face (pipeline.cpp:4047-4080);
  - bridges keep their own `setVisible` (lldrawable.cpp:1537+);
  - the VO-cache and sky pushes are unchanged (pipeline.cpp:4019-4044).
- **What it can save:** only the traversal work for union-rejected subtrees, plus any retained per-node sphere
  classifications (CX C1). This is bounded by the share of `"cube face cull"` that C0 measures.
- **What stays per face:** all face-frustum tests, the pushes, the bridge and VO-cache culls, stateSort, both shadow
  cascades (pipeline.cpp:25975-25990, :26710-26714) and all rendering.
- **Profitability is unproven in either direction.** C0 gives the ceiling.
- **If approved** (a separate reviewed step after T13):
  - a probe-owned context, never `sCull` or the static result;
  - lifetime-validated handles, with no `LLDrawInfo` retention (pipeline.cpp:5080-5135);
  - a full key and epoch;
  - absent for Hero and Prism;
  - the `!gCubeSnapshot` union-shadow gate (:25988-25990) untouched.

---

## 8. D — conservative tiny-group cull (default OFF; byte-identical at 0)
- **Capture context.**
  - `enum class ProbeCaptureKind : U8 { NONE, ORDINARY, LIVE, REALTIME }`, held in the static
    `LLReflectionMapManager::sCaptureKind`, with a public static getter.
  - An RAII scope in `updateProbeFace` around the two `probe->update` calls (:1323, :1330) sets the kind: LIVE if
    `mCinematicLiveProbeCapture`, REALTIME if `probe != mUpdatingProbe`, else ORDINARY.
  - It restores NONE on exit, including through the early return in llreflectionmap.cpp:55-56.
  - Hero and Prism never enter this function.
- **Filter.** In pipeline.cpp `postSort`'s visible-group loop, after :5111-5116:
  `if (px > 0 && kind != NONE && !sShadowRender && !sPrismLensRender && rejectable && tiny) continue;`
  - **rejectable:** all of the following.
    - `PARTITION_VOLUME`, not a bridge, not dead.
    - Every `mDrawMap` key is in {PASS_SIMPLE, PASS_GRASS, PASS_SHINY, PASS_BUMP, PASS_POST_BUMP, PASS_MATERIAL,
      PASS_MATERIAL_ALPHA_MASK, PASS_SPECMAP, PASS_SPECMAP_MASK, PASS_NORMMAP, PASS_NORMMAP_MASK, PASS_NORMSPEC,
      PASS_NORMSPEC_MASK, PASS_ALPHA_MASK} (lldrawpool.h:149-213).
    - **Any `PASS_GLTF_PBR*` key → kept.** Emissive PBR registers there (llvovolume.cpp:7466ff), so pass keys cannot
      prove non-emissive (CX P1-6, OP P2).
    - Fullbright, emissive, glow, blend, alpha, invisible and rigged passes are kept, so alpha groups are never skipped.
  - **tiny:**
    - `r = |mObjectBounds[1]|`, `d = |center − camera origin|`.
    - Keep if `d − r ≤ 2·near`.
    - `p = 3·r·res/(d − r)`, where `res = mProbeResolution` and 3 is the cube-corner magnification bound. Reject iff
      `p < px`.
- Lights and shadow casters are unaffected. Outside probe captures `kind == NONE`, and `px == 0` means the branch never
  runs.

---

## 9. Review package
### 9.1 Off-path inertness
| Off | Prove |
|---|---|
| RenderProbeOnDemand=0 | Hooks and tag scopes cost one branch on `gProbeDirtyRecording`. `ScopedTag` is constructed only inside `if (recording())`, or is a no-op when not recording. The flush drains and discards. Selection, the default rule, dispatch, `doProbeUpdate` effects, debug text and `mRadiancePass` handling are identical. Live H is identical. The additions are counters and the setting-gated log. |
| RealtimeFacesPerFrame=6 | Always the unchanged `updateRealtimeProbeAllFaces`. |
| TinyCullPixels=0 | The `postSort` branch is never taken. |
| SharedCull=0 | Today's path. |

### 9.2 Isolation proofs required from reviewers
- (a) No hook mutates state.
- (b) `sCaptureKind` is NONE outside the `updateProbeFace` scope.
- (c) Secondary scratch: the Live cursor and `mRtSlice` never interleave; each pass starts at face 0; the blocked case
  holds.
- (d) `mRadiancePass` is restored after sliced faces, and set false at ON-path txn start only.
- (e) `mLastUpdateTime` semantics are unchanged.
- (f) No GL state change anywhere (CLAUDE.md rule 1).
- (g) The env extraction is a moved block with exactly the three documented edits.
- (h) The §4.7 contract cannot ack a partial transaction.
- (i) The light diff reads `mNearbyLights` only at flush time. It is main-eye state, because captures restore it:
  pipeline.cpp:10331-10342 for Live, :10090/:10259 for Prism.

### 9.3 Least-sure list: attack these first
1. **Camera-driven churn not yet tagged.** Find any other `markRebuild`/`markTextured`/`movePartition` driven by
   main-camera distance, pixel area or priority: particles, avatar impostors, water, sky, media faces, `LLVOVolume`
   virtual-size paths.
2. **Coverage holes:** `movePartition` from llviewerobject.cpp:1131 and pipeline.cpp:4553; group-only rebuild paths;
   bridge-child moves; a light whose texture arrives.
3. Rig `applyFrame` no-op writes: do the H1a/H1b epsilons silence them?
4. Serial race and flush placement: the flush runs before :354.
5. The §4.7 contract with delete / `initReflectionMaps` / teleport / toggle.
6. Ownership transitions (§4.8) and the STARVED statistics.
7. Light-diff churn from fade and membership during camera flight. Are 4 fade buckets too many or too few?
8. Env tail layout stability (placeholder counts) and setting types.
9. D: the rejectable set.
10. `Rcap`, the capsule and infinite-far classes.
11. H6 gating: `getDiscardLevel()` at `postCreateTexture` is the newly created level; `Dp` math.
12. **Is the approach wrong?**

---

## 10. In-world test plan (user runs; log on; Tracy 10 s per step; outcomes stated in advance)
- **Latency model.** After a change, the refresh of a probe starts once all of the following have passed:
  - its MinInterval, if the probe started less than 1 s ago;
  - its queue position among eligible probes × 12 frames;
  - its occlusion, if occluded.

  With nothing else dirty: ≈ 12 frames (≈ 0.2 s at 60 fps). With k probes ahead: ≈ 12(k+1) frames. `maxlag` reports it.
- Report raw lines. A contradicting counter is a FAIL; PAUSED is INCONCLUSIVE.
- The default flip needs **T0-T22 all PASS**.

**Steps and expected outcomes:**
- **T0 Off identity.** Off vs backup `AlchemyTest.pre-probeondemand.exe`: the snapshot diff is within the A1/A2 noise
  floor (LIVE_PROBE_REFRESH_DESIGN §8.0). `OFF-BASELINE`, faces ≈ fps.
- **T1 Static skybox, P probes.**
  - Off: `"probe update"` every frame.
  - On: after warm-up (≈ 12P frames), faces/s ≈ 12P/60 (only `s`). `"probe update"` appears as isolated frames. The
    converged image matches Off.
- **T2 Rez / move / rotate / retexture a prim.** `g`/`t` > 0. Probes within **≈ 131 m** (Rcap), or shadowed through
  the capsule, are dirtied. Probes further away stay clean. Refresh follows the latency model. Rotation in place must
  also show `g`.
- **T3 Drag the sun slider.** `e > 0`, ord ≤ 1; STARVED allowed.
- **T4 4 h day cycle.** `e` ≈ every 20 s. With P ≳ 25, STARVED appears after each `e` (expected).
- **T5 Lights.**
  - Recolour, move or rotate a projector, toggle a gobo, and toggle no-shadow → `nl > 0`, `l > 0`, nearby probes only.
  - Wear an attachment facelight and walk → `nl` rises near the path.
- **T6 Avatar walks through a static probe.** `drop_dyn` rises; the probe's `g` stays 0. Its own attachment lights do
  produce `nl`/`l` (expected).
- **T7 Camera orbit and fly in a loaded static set.** The `drop_lod*` and `drop_texanim` counters rise. `g ≈ 0` and `t ≈
  0` after textures settle. `nl` may be > 0 when lights cross far clip or the count cap (real). Over 30 s, faces/s stays
  near the safety rate plus `nl`-driven refreshes.
- **T8 Live On change.**
  - Move a prim near the subject → `[LiveProbe] last=changed`, a clean pair, then idle.
  - Off: no reaction until the watchdog.
- **T8b Live On change, camera orbit** around a static subject in a loaded set with on-demand on: `[LiveProbe]
  converged=y`, idle ≥ 95 % (OP P0-1).
- **T9 Realtime detail, no Live.**
  - Off: 6 faces/frame under `"rmmu - realtime"`.
  - On (N=2): 2 faces under `"rmmu - rt sliced"`, `rt_blocked ≈ 0` after warm-up, one pass per 3 frames.
- **T10 Occlusion.** Edit behind the camera → `deferred > 0`; turn around → refresh per the latency model.
- **T11 Default probe, coverage None/Sky, standing still.** No `p`. Paused clouds → only `s`. Moving clouds → `c` at ≤
  1 per `RenderDefaultProbeUpdatePeriod`. Walk 20 m → one `p`.
- **T12 D.** Strong vs Off: ZONE_NUM > 0 and lower `"probe update"` time. The main view is identical. PBR and emissive
  objects are still reflected.
- **T13 C0.** Record the five zones.
- **T14 Teleport and region crossing.** No crash, no OVER BUDGET. The crossing costs no full resync: `r` does not jump
  by P.
- **T15 Delete a manual probe while it is refreshing** (debug text yellow) → no `nack` storm, and the next probe
  completes both passes.
- **T16 Allocation.** Change the probe count / coverage and rez probes → WARMUP starts, then the scene settles to OK.
- **T17 Toggle on-demand mid-refresh**, both ways, repeated 10× → `r` on each ON edge, no stuck `dirty`, no OVER
  BUDGET.
- **T18 MaxAge=0 on a static set.** After warm-up, faces/s = 0 and `s = 0`. The backstop is off, as the tooltip says.
- **T19 Refresh all probes now.** `refresh_all = 1`, `r = P`, and the Live pair runs.
- **T20 EEP swap and water-only change.** `e` or resync; converges.
- **T21 Texture animation / flexi / media on screen** in a static set → no `g`/`t` churn (they are safety-only). The
  reflection of them updates at MaxAge.
- **T22 Slicing plus Live switch.** Designate or un-designate a Live Probe while slicing → no garbage cube.
  `[LiveProbe]` warm-up is normal, and the sliced path resumes at face 0.

---

## 11. Implementation order (single delivery; one build after review converges)
- [ ] 1. `alprobeschedule.h` + CMake.
- [ ] 2. TUT in `tests/alcinelightrigmodel_test.cpp` (12 tests; count the `template<> template<>` additions):
  - S1 an event mid-txn stays dirty;
  - S2 a same-frame pre-start event is acked;
  - S3 MinInterval;
  - S4 safety at MaxAge, never at 0;
  - S5 DEFERRED keeps dirt;
  - S6 footprint (inside/outside sphere, terrain global, capsule only on a sphere miss, class masks);
  - S7 `onTxnComplete` refuses without `mInTxn` / irradiance / epoch / cube match, and never clears newer serials;
  - S8 verdict priority;
  - S9 SliceCursor N faces, alternation, identity reset at face 0, blocked frame;
  - S10 grace-init and ON-edge `clearTxn`;
  - S11 ownership excludes Live/REALTIME records from hits and stats;
  - S12 the H6 notify rule (first image, coarse refinements, none beyond `Dp`, none on downgrade).
- [ ] 3. `alprobedirty.{h,cpp}` (the `extern` flag) + CMake.
- [ ] 4. settings.xml: 7 keys (count them).
- [ ] 5. Hooks and tags: H1a, H1b, H2, H3, H4 + LV, LT, LTr, LG, TX, H6 (+ the llviewertexture.h member), SH.
- [ ] 6. pipeline.h accessor. Manager:
  - env extraction (moved block);
  - record/ids/epoch;
  - flush (resync, events, light diff, hit test, R_PROBE, default extras, env);
  - selection, default rule, txn start/irradiance/complete;
  - debug text;
  - reset sites;
  - sliced realtime;
  - Live serial;
  - refresh-all;
  - counters and log.
- [ ] 7. D scope + filter. C0 zones.
- [ ] 8. UI: Lightbox block + button callback, Graphics row, presets.
- [ ] 9. Adversarial review (Opus + Codex) with §9 verbatim; loop to 0 must-fix (the user ruling applies).
- [ ] 10. Checkpoint commit + `AlchemyTest.pre-probeondemand.exe`; ONE build; hand §10 to the user.

## 12. OFF-LIMITS, allowed files, out-of-scope reports
- **OFF-LIMITS:**
  - shaders;
  - `llheroprobemanager.*`, `llprismlens.*`;
  - `alcineliveproberefresh.h` (reuse only);
  - the `updateRealtimeProbeAllFaces` and `updateCinematicBudget` bodies;
  - `decide` and Live policy;
  - `alcinelightrig*` and `alcinerigrim*` (read-only calls only);
  - `alpanelcinelightrig.*`, `panel_cine_light_rig.xml`;
  - LiveProbeConfig / scene LLSD / Setups;
  - the `updateProbeFace` body except the capture-kind scope;
  - `llviewerwindow.cpp`;
  - `llenvironment.*` (getters only);
  - in pipeline.cpp, everything except the H3 lines and the D filter. In particular: the union-shadow gate
    (:25988-25990), the alpha-distance skip (:5162-5172), `calcNearbyLights` (read through the accessor only) and
    `calcPixelArea`;
  - `calcLOD`;
  - llpresetsmanager.cpp:343;
  - `enve`, `vcpkg`.
- **Allowed:**
  - new files: `alprobeschedule.h`, `alprobedirty.{h,cpp}`;
  - `CMakeLists.txt`;
  - `llreflectionmap.h`, `llreflectionmapmanager.{h,cpp}`;
  - `lldrawable.cpp`, `llspatialpartition.cpp`;
  - `pipeline.h` (accessor), `pipeline.cpp` (as above);
  - `llvovolume.cpp`;
  - `llviewertexture.{h,cpp}`;
  - `llsurfacepatch.cpp`, `llvotree.cpp`, `llvograss.cpp` (tag scopes only);
  - `llviewerdisplay.cpp` (C0 zones);
  - `alfloaterlightbox.cpp` (one callback);
  - `app_settings/settings.xml`, `floater_lightbox_settings.xml`, `floater_preferences_graphics_advanced.xml`;
  - `llpresetsmanager.cpp`;
  - `tests/alcinelightrigmodel_test.cpp`.
- **Reported, not fixed (upstream):**
  - (1) llpresetsmanager.cpp:343: a missing comma merges two preset names.
  - (2) `deleteProbe` (:1238-1242) does not reset `mRadiancePass`. On the OFF path, a replacement probe can run a
    radiance pass first and be marked complete without its irradiance pass. The ON path neutralises this (§4.6 step 5).
