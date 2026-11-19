# Reflection probes: update on demand — implementation brief V4

**Status:** DESIGN ONLY (Opus, 2026-09-30). Nothing implemented, built or committed.
**Base:** `fix/animesh-clone-pose-polish` @ `303065bda35`. Every anchor below was re-read at this HEAD.
Paths without a directory prefix are `indra/newview/`.
**Process (CLAUDE.md):**
1. Sonnet implements from this brief.
2. Opus and Codex attack the code, looping until 0 must-fix.
3. Checkpoint commit and exe backup.
4. ONE build.
5. The user runs §10.

**Supersedes:** V3. **Read V3 alongside this brief.** Where V4 says "as V3" or "unchanged from V3 §x", the V3 text
applies verbatim: default rule details, UI control list, D, C1, TUT S1-S12 and T0-T22 bodies. Everything else here
overrides V3.
**Reviews folded in:**
- R1: `doc/PROBE_UPDATE_ON_DEMAND_REVIEW_{CODEX,OPUS}_R1.md`.
- R2: `doc/PROBE_UPDATE_ON_DEMAND_REVIEW_CODEX_R2.md` (CX2), `doc/PROBE_UPDATE_ON_DEMAND_REVIEW_OPUS_R2.md` (OP2).

**Facts:** `doc/PROBE_ON_DEMAND_RESEARCH.md` (R§n).
**Labels:** PROVES = read in code; INFERENCE = estimate.

**Rulings:**
- **User ruling:** anything that breaks the feature's promise is a blocker, whatever its label.
- **User decision U1 (probe lights):** with on-demand ON, ordinary captures use a light list built around the probe
  itself.
  - Camera motion must never dirty ordinary probes through light membership.
  - The touch to `calcNearbyLights` is approved.
  - The OFF path stays byte-identical to stock.
  - The ON and OFF looks are allowed to differ.
- **User decision U2 (continuous motion):** freeze it until it stops. A per-key streak detector suppresses continuous
  sources and emits one settle event when the source goes quiet.
  - The 60 s safety refresh and Refresh-all remain as backstops.
  - Overflow never cancels the capture in flight.

---

## 0. Resolution tables

### 0.1 Round 2
| ID | Finding | V4 resolution |
|---|---|---|
| CX2 P0-1 | Camera independence abandoned; light-membership dirt on every orbit; events reach 151 m | **U1.** §4.5: ON-path ordinary, default and sliced captures build a **probe-centric** light list through the existing transient branch (pipeline.cpp:9735-9822), with save/restore via `begin/endCinematicProbeCapture` (:10321-10345). The light diff (§4.5.3) is camera-independent: it reads `mLights`, not `mNearbyLights`. Events are sized `getLightRadius()·1.5`. T7 is strict (§10). |
| CX2 P0-2 | Light diff ≠ rendered set; count cap applied in the render loop (:23214-23223, :23308-23312); `RenderLocalLightCount` missing | §4.5.3 mirrors the filters of the transient branch and the deferred loop. §4.3 tail adds `RenderLocalLightCount`, `min(RenderFarClip, draw distance)` and `RenderReflectionProbeMaxLocalLightAmbiance`. With more than `RenderLocalLightCount` eligible lights, light events become global (§4.5.3, conservative). Fade bucket removed. |
| CX2 P0-3 / OP2 N4 | Projector gobo first-image arrival missed | §4.4 H6: fan-out over `mVolumeList[LLRender::LIGHT_TEX]` (llviewertexture.h:215; llrender.h:313) emits light-sized `C_LIGHT|R_TEX` events. These also bump Live (§5.2). |
| CX2 P0-4 | Realtime ownership blocks warm-up; N=6 classified ORDINARY | §5.1: with ON, the closest-dynamic probe **always** takes the sliced path (N = 1..6, so N=6 is REALTIME too), but only when `mComplete`. An incomplete probe stays ORDINARY and gets ordinary warm-up (WARMUP). REALTIME→ORDINARY does not resync (§4.8). |
| CX2 P0-5 | Broad-phase rejects "always hit" classes | §4.6 step 2c: **global events first** (terrain/water, overflow, refresh-all, large-texture, over-cap lights), then the spatial broad-phase. |
| CX2 P0-6 | Discrete omissions intentional; MaxAge=0 makes misses permanent; Refresh-all is not a barrier | Async PBR completion is now an event (§4.4 H7). **MaxAge=0 is forbidden**: clamp 10..600 (§6.1). Refresh-all is a real **capture barrier** with completion tracking, a readout, forced occluded probes and defined mid-refresh semantics (§4.11). |
| CX2 P1 | Tests insufficient | §10 T29-T36: count-limit change, late gobo, distant terrain, overflow beyond both caps, incomplete realtime, edit during LOD, txn rejection through teleport/reset/realloc, Live OFF-hash equality via TUT S13. |
| CX2 P2 | Shift sticky anchors; placeholders for disabled slots; discard/float guards | §4.4 SH shifts the anchors (the per-light position is fields 0-2 by contract). §4.3 emits placeholders for every slot. §4.4 H6 uses integer `Dp` and a `d < 0` guard. |
| OP2 N1 | Continuous movers/lights never settle; overflow `clearTxn`s the in-flight probe | **U2.** §4.10 streak detector in the recorder and the light diff. Overflow and refresh-all only hit; they never `clearTxn` (§4.6, §4.7). |
| OP2 N2 | Mesh/sculpt LOD arrival camera-driven | §4.4 MS: `mProbeBuilt` / `mProbeLodArrival` on LLVOVolume. `notifyMeshLoaded` (llvovolume.cpp:1284-1287) and the sculpt refinement (llviewertexture.cpp:2135-2143) are tagged once built. `updateGeometry` treats them as LOD-only. |
| OP2 N3 | Fade bucket spurious; cap rank | Fade dropped. The probe-centric list removes camera rank; the cap goes in the env tail plus the global rule (§4.5). |
| OP2 N5 | Re-sharpen after downgrade missed | §4.4 H6/DS: a downscale hook (llviewertexturelist.cpp:1174-1177) stamps `mProbeBlurStamp` with the ordinary face counter. A re-sharpen notifies iff a probe face was rendered after the blur. |
| OP2 P1-a | Agent-space sticky positions on region crossing | §4.4 SH: offset the light sticky accumulators (fields 0-2), queued events, streak accumulators, record origins and cameras. |
| OP2 P1-b | `mLightScale`; missing `RenderReflectionProbeMaxLocalLightAmbiance` | §4.3 tail. `mLightScale` = f(ambiance, that setting) (llreflectionmapmanager.cpp:1306-1311, pipeline.cpp:22925-22928). Ambiance is already an R_PROBE field. |
| OP2 P1-c | `CineRigRimTint` is F32; Slot casts | §4.3: `addAbs(tint, 1e-3f)`; `static_cast<ALCineLightRigManager::Slot>(i)` (alcinelightrigmanager.h:1089-1091). |
| OP2 P1-d | Texture on more than 4096 faces never bumps Live | §4.4 H6 large-texture event bumps Live. |
| OP2 E | Ownership-flip resync inflates `r`; `mProbeNotedDiscard` write only while recording; integer `Dp`; count H2 rebalancing | §4.8 (no resync on REALTIME→ORDINARY); §4.4 H6; §4.2 `rebal` counter. |
| OP2 F | T23-T28, `nl` count in T14, UNSETTLED verdict, strict T7 | §10 and §6.4. |

### 0.2 Round 1 (V3 fixes, carried forward; section numbers are V4)
- **CX P1-1** default camera origin: §4.6 step 2e.
- **CX P1-2 / OP P0-1** camera-driven rebuilds: §4.4 tag rows.
- **CX P1-3** transform comparison: §4.4 H1a.
- **CX P1-4** transaction contract: §4.7.
- **CX P1-5** ownership: §4.8.
- **CX P1-6** PBR kept in D: §8.
- **CX P1-7** tests: §10.
- **OP P1-1** LOD scope: §4.4 LV/H4.
- **OP P1-2** light list: superseded by U1, §4.5.
- **OP P1-3** extern flag: §4.2.
- **OP P1-4** texture churn: §4.4 H6.
- **OP P1-5** flush cost: §4.6.
- **R1 P2s:**
  - R_PROBE fields: §4.6 step 2d.
  - Gobo/no-shadow/Rig Rim: §4.5, §4.3.
  - Shift: §4.4 SH.
  - Txn no-op: §4.7.
  - H4 union: §4.4.
  - Budget wording: §6.4.
  - Anchors: §6.
  - Refresh-all button: §4.11.
  - C1 wording: §7.2.

### 0.3 Rejected / only partly fixable (see report)
- **Flexi and media are frozen without a settle event.** Their per-frame updates are throttled by main-camera visibility
  and distance (flexi idle updates; `mVisible` gate at llviewermedia.cpp:3102). Hooking them would bring back
  camera-driven dirt when the camera turns away, which is the T7 promise. They stay safety/Refresh-all only.
- **Water waves** are shader-time and emit nothing. Water is inherently "frozen" at capture time.
- **Over-cap spot lights far from a probe:** handled by the conservative global rule (§4.5.3), not per-probe exactly.
- No finding was rejected outright.

---

## 1. Decisions

| # | Decision | Why |
|---|---|---|
| D1 | `RenderProbeOnDemand` ships **false**. The flip to true is a separate commit after **T0-T36 all PASS**. | Coverage is proven only in-world. |
| D2 | Batched event push → per-probe serials. | R§2. |
| D3 | Complete dynamic ordinary probes are always eligible. They are queued, with MinInterval ≥ 1 s between starts. DYN-class geometry events are dropped. | R§7. |
| D4 | Budget: ordinary 1 face per `update()`; realtime ≤ 6 (Live FULL/budget or sliced N). Total ≤ 7. | Structural. |
| D5 | Occluded complete probes are deferred, **except** under a Refresh-all barrier (§4.11). | R§4 + CX2 P0-6. |
| D6 | Live Probe: no min-interval. | Bounded per frame by its mode. |
| D7 | ON: the closest-dynamic probe always goes through the sliced path (N = 1..6), and only once `mComplete`. | CX2 P0-4. |
| D8 | Continuous sources: streak-suppressed with a settle event (§4.10). Flexi, media and water are frozen and backstop-only (§0.3). | U2. |
| D9 | C1 needs user approval; C0 is built. | CLAUDE.md. |
| D10 | Capture far = `RenderReflectionProbeDrawDistance` (llviewerdisplay.cpp:224-228). `Rcap = far·√3 + 20 m` ≈ 131 m at the default. Light events use their own radius·1.5 sphere against `Rcap`. | PROVES. |
| D11 | **ON-path captures light from a probe-centric list** (U1). Ordinary, default and sliced captures all do. Live keeps its own (already probe-centric). | U1. |

---

## 2. Reconciliation with 615750a40f9
The same as V3 §2. 615750a40f9 already delivered:
- Live slicing (:686-744);
- readiness (:650-669, :722-730);
- converge-to-idle H (:750-903);
- Live scratch blocking (:566-570).

Still to deliver: the Live geometry source (§5.2), the per-probe minimum interval (§4), and closest-dynamic slicing
(§5.1). The `CineLightRigLiveProbe*` settings, `decide`, the rig-panel card and the `[LiveProbe]` log are kept.

## 3. Scope (one delivery)
- **A:** ordinary + default scheduling, including probe-centric lights, the streak detector and the Refresh-all barrier.
- **B-rest:** §5.
- **C0:** timing zones. C1 only with approval.
- **D:** tiny-group cull, off by default.
- **Surfaces:** settings, UI, presets, debug text, log verdicts, TUT tests.
- **Deferred:** probe LOD bias (R§6), shared probe shadows, supersample factor, relightable probes, fast GGX.

---

## 4. A — on-demand scheduling

### 4.1 Pure model `alprobeschedule.h` (header-only; `viewer_HEADER_FILES`)
Namespace `ALProbeSched`. Everything is `inline`, uses `stdtypes.h`, and casts explicitly.
```cpp
enum Reason : U16 { R_GEOM=1<<0, R_TEX=1<<1, R_LIGHT=1<<2, R_ENV=1<<3, R_PROBE=1<<4, R_SAFETY=1<<5,
                    R_CLOUD=1<<6, R_RESYNC=1<<7, R_WARMUP=1<<8, R_DYN=1<<9, R_SETTLE=1<<10 };
enum Class  : U8  { C_STATIC=1<<0, C_TERRAIN_WATER=1<<1, C_LIGHT=1<<2, C_GLOBAL=1<<3 };
struct Event { F32 mMin[3]; F32 mMax[3]; U16 mReason; U8 mClass; const void* mKey; };
enum class Owner : U8 { ORDINARY, LIVE, REALTIME };
struct Record {
    U32 mId = 0;
    U64 mDirtySerial = 0, mAckSerial = 0, mTxnSerial = 0;
    U32 mTxnEpoch = 0;  bool mInTxn = false, mTxnIrrDone = false;  S32 mTxnCube = -1;
    U16 mPending = 0, mTxnReasons = 0, mLastReasons = 0;
    F64 mFirstDirty = -1.0, mFirstDirtyInTxn = -1.0, mLastStart = -1.0, mLastComplete = -1.0;
    bool mBarrier = false;                                  // pending Refresh-all (§4.11)
    U32 mStartWindows = 0; U32 mLastStartWindow = 0;        // UNSETTLED (§6.4)
    F32 mStartOrigin[3] = {0,0,0}; F32 mStartRadius = 0.f, mStartAmbiance = 0.f, mStartNear = 0.f;
    bool mStartDynamic = false, mStartBox = false; S32 mStartCube = -1;
    F32 mStartCam[3] = {0,0,0}; F32 mStartCloud[2] = {0,0};
    Owner mOwner = Owner::ORDINARY;
};
struct View { bool mComplete, mOccluded, mDynamic, mAllocated; };
enum class Why : U8 { NONE, WARMUP, BARRIER, DIRTY, SAFETY, DYNAMIC, WAIT_INTERVAL, DEFERRED };
inline Why evaluate(const Record&, const View&, const Policy&, F64 now);
// + hit, onTxnStart, onTxnIrradianceDone, onTxnComplete (§4.7), clearTxn, hitsCapture (§4.6),
//   Streak (§4.10), SliceCursor (§5.1), EnvSample + appendEnvironmentFields (§4.3), window stats + verdict (§6.4)
```
**`evaluate`, in this order:**
1. `!mAllocated` → NONE.
2. `!mComplete` → WARMUP.
3. `mBarrier` → BARRIER. This ignores occlusion and the interval.
4. `mOccluded` → DEFERRED if dirty or safety-due, else NONE.
5. Inside the interval → WAIT_INTERVAL if dirty, dynamic or safety-due, else NONE.
6. `dirty` → DIRTY.
7. `mDynamic` → DYNAMIC.
8. Safety-due → SAFETY.
9. Otherwise → NONE.

Eligible = WARMUP, BARRIER, DIRTY, DYNAMIC or SAFETY.

### 4.2 Recorder `alprobedirty.{h,cpp}` (main thread only)
```cpp
extern bool gProbeDirtyRecording;                          // defined ONCE in alprobedirty.cpp
namespace ALProbeDirty {
    inline bool recording() { return gProbeDirtyRecording; }
    void setRecording(bool); void setProbeResolution(U32);
    U64  probeFaceCounter();          // ordinary+default+sliced faces rendered while ON (H6 re-sharpen rule)
    void bumpProbeFaceCounter();
    enum Tag : U8 { TAG_NONE, TAG_LOD_VOLUME, TAG_LOD_MESH, TAG_LOD_TERRAIN, TAG_LOD_TREE, TAG_LOD_GRASS,
                    TAG_TEXANIM_TOGGLE, TAG_COUNT };
    struct ScopedTag { explicit ScopedTag(Tag); ~ScopedTag(); };   // nests; restores previous
    void noteDrawable(LLDrawable*, U16 reason);
    void noteDrawableBounds(LLDrawable*, const LLVector4a* old_mn_mx, U16 reason);
    void noteLightVolume(LLVOVolume*, U16 reason);                 // sphere(pos, radius·1.5), C_LIGHT
    void noteGlobal(U16 reason, U8 cls);                            // C_GLOBAL (all ORDINARY + Live)
    void noteTexture(LLViewerFetchedTexture*);                      // H6 fan-out
    void noteTextureDownscale(LLViewerFetchedTexture*);             // DS
    void shiftQueued(const LLVector4a& offset);
    void markOverflow();
    struct Drain { std::vector<ALProbeSched::Event> mEvents; bool mOverflow;
                   U32 mDropped[TAG_COUNT], mDroppedDyn, mNoopMove, mRebal, mRaw; };
    void drain(Drain&);
}
```
There is deliberately no media or flexi note function (§0.3).
- **Storage:** the per-frame vector and a coalescing map keyed by drawable/volume pointer (never dereferenced after the
  note returns). Cap 4096 → overflow.
- **`mRebal`:** an insert and a remove of the same key in one frame (octree rebalancing, OP2 E).
- **`classify`, in this order:**
  1. HUD → drop.
  2. Attachment / animated object / `LL_VO_PART_GROUP` → DYN → drop (lights come only from §4.5).
  3. `isReflectionProbe()` → drop.
  4. Surface patch / water / void water → `C_TERRAIN_WATER`.
  5. Otherwise → `C_STATIC`.
- **`worldBounds`:** the bridge's world extents for bridge-local drawables (llspatialpartition.h:544;
  lldrawable.cpp:1328-1370). Otherwise the drawable's extents, or a position sphere as the fallback.

### 4.3 Environment generation
- **Shape of the extraction (CX2 P1 "direct Live OFF-hash test").** Sections 3-5 of `sampleCinematicH` (:777-892) are
  split into:
  - a gatherer in the manager: `ALProbeSched::EnvSample gatherEnvSample()`. It calls exactly the getters of :778-891,
    in order, into a POD with named fields;
  - a pure builder in alprobeschedule.h: `appendEnvironmentFields(Signature&, const EnvSample&, F32 sun_moon_deg,
    bool blend_only_when_mixing)`. It appends the fields in HEAD order with HEAD tolerances.
- **Documented differences:**
  - sun/moon use `sun_moon_deg` (HEAD literal 0.1);
  - the two blend factors are replaced by 0 when `blend_only_when_mixing` and the textures are not mixing (the V3
    rule);
  - `auto_adjust_legacy` becomes a sample field;
  - the unused :756 declaration is removed.
- Live calls `(0.1f, false)`.
- **TUT S13** asserts that the builder's layout equals a table transcribed from HEAD :786-892: tolerance kind, a, b and
  count per field (sky 50 floats, water 19, settings), plus the exact-mix sequence. It also asserts that
  `(0.1f, false)` reproduces the HEAD H for a synthetic sample. This is the direct Live OFF-hash test.
- **Ordinary env** = builder with `(RenderProbeDirtySunDeg, true)`, then a fixed-layout **tail**:
  - `addAbs(RenderReflectionProbeDrawDistance, 0.5f)`.
  - `addAbs(min(RenderFarClip, RenderReflectionProbeDrawDistance), 0.5f)` (U1 list range, pipeline.cpp:9754-9757).
  - Exact `RenderReflectionProbeDetail`, `RenderReflectionProbeLevel` and `RenderLocalLightCount`.
  - `addAbs(RenderReflectionProbeMaxLocalLightAmbiance, 1e-3f)` (it drives `mLightScale`, :1306-1311).
  - Light toggles: exact `RenderAttachedLights`, `BDMergeLightToggles`, `BDMergeRenderOwnAttachedLights`,
    `BDMergeRenderOthersAttachedLights`, `BDMergeRenderWorldLights`, `BDMergeRenderProjectors` (pipeline.cpp:9561-9576),
    plus `addAbs(AlchemyGlobalLightScale, 1e-3f)`.
  - Rig Rim: exact `CineRigRimIncludeProbes` and `CineRigRimEnabled`. Then **always** the rim block, whose values are
    zeroed when not both true:
    - `addAbs` 1e-3 for `CineRigRimMasterGain`, `CineRigRimBackSoftness`, `CineRigRimRoughnessSoften`,
      `CineRigRimShadow`, `CineRigRimTronMix` and **`CineRigRimTint` (F32**, alcinerigrim.cpp:217);
    - exact `CineRigRimDebugRimOnly`, `CineRigRimIncludeAlpha`, `CineRigRimTronMode`, `CineRigRimTronColorSource`;
    - for every slot index `i < SLOT_COUNT` (the constant used at alcinelightrigmanager.cpp:441), with
      `const auto slot = static_cast<ALCineLightRigManager::Slot>(i)`: exact `isSlotEnabled(slot)`, then for every light
      `j < ALCineLightRigModel::LIGHT_COUNT` four `addAbs` of `at(slot).rigRimParams(j)`, or 0 if the slot is disabled
      or rim is excluded. The layout never depends on live state (CX2 P2).
  - Verify every setting's type in settings.xml. A wrong-type `LLCachedControl` asserts.
- `h != mOrdEnvH` → R_ENV **global** event (every ORDINARY record, including the default probe).

### 4.4 Hook sites (each inside `if (ALProbeDirty::recording())`; no other off-path code)
| ID | Site | Behaviour |
|---|---|---|
| H1a | lldrawable.cpp:806 / :842 (`updateMoveUndamped/Damped`), around `updateXform` :808 / :846 | Snapshot `mXform` position/rotation, `mCurrentScale` and the old extents before the call. Compare after it: position > 1e-4 m, `1 − |dot|` > 1e-7, or scale > 1e-4 → `noteDrawableBounds(old)`. Spinners (llviewerobject.cpp:7337 `applyAngularVelocity`) land here every frame and are streak-suppressed (§4.10). |
| H1b | llspatialpartition.cpp:972 `move` | Snapshot after :977-981. On the exits :991-992, :1003-1004 and :1014: note the union if it changed by > 1e-4 m, else `mNoopMove++`. Usually a no-op for volumes (H4 covers them). |
| H2 | llspatialpartition.cpp:774 / :781 | Membership, `R_GEOM`. Count `mRebal`. |
| H3 | pipeline.cpp:4517 `markTextured` → `R_TEX`; :4580 `markRebuild(drawable, flag)` → `R_GEOM` iff `flag & ~REBUILD_POSITION` | Tags apply. The group overload is not hooked. |
| H4 | llvovolume.cpp:2157 `updateGeometry` | Entry: `lod_only = (mLODChanged || (mSculptChanged && mProbeLodArrival)) && !mVolumeChanged && !mFaceMappingChanged && !mColorChanged && !(mSculptChanged && !mProbeLodArrival) && !isState(REBUILD_POSITION)`. If `lod_only`, a `ScopedTag(TAG_LOD_VOLUME)` covers the whole function (covering :1967). The flexi branch :2169-2177 is not hooked (§0.3). Snapshot bounds; `content` = !lod_only && a content flag. Before :2255: note the union of entry and exit bounds if content or bounds changed. On a successful build with `getNumFaces() > 0` (recording only): `mProbeBuilt = true; mProbeLodArrival = false`. |
| MS | llvovolume.cpp:1284-1287 `notifyMeshLoaded`; llviewertexture.cpp:2135-2143 sculpt refinement loop | New LLVOVolume members `bool mProbeBuilt = false, mProbeLodArrival = false` (llvovolume.h). If `mProbeBuilt`: set `mProbeLodArrival = true` and wrap the `markRebuild` in `ScopedTag(TAG_LOD_MESH)`. The first-ever arrival stays an event (OP2 N2). |
| LV | llvovolume.cpp:1699-1737 `updateLOD` | Whole body under `TAG_LOD_VOLUME`. |
| LT | llsurfacepatch.cpp:1054-1070 | `TAG_LOD_TERRAIN`. Real edits stay events: `dirty()` :92, `dirtyPatch()` :611, composition :878-884. |
| LTr | llvotree.cpp:378-381 | `TAG_LOD_TREE`. |
| LG | llvograss.cpp:368-392 | `TAG_LOD_GRASS`. |
| TXt | llvovolume.cpp:665-681, :876-894 | `TAG_TEXANIM_TOGGLE` (camera-size and matrix-creation rebuilds). |
| TXa | llvovolume.cpp:616 `animateTextures`, when `result != 0` (after :622) | `noteDrawable(mDrawable, R_TEX)`. Runs every frame for every anim instance (llviewertextureanim.cpp:80-84), independent of the camera. Streak-suppressed; settles when the animation stops. The start :629 and stop :432/:536 edits stay events. |
| H6 | llviewertexture.cpp:2121 `postCreateTexture`, after :2135-2143 | New LLViewerFetchedTexture members (llviewertexture.h): `S8 mProbeNotedDiscard = -1; U64 mProbeBlurStamp = 0; bool mProbeBlurred = false`, written **only while recording**. See the H6 rule below this table. |
| DS | llviewertexturelist.cpp:1174-1177, after `img->scaleDown(...)` | `noteTextureDownscale(image)`: if the new discard > `Dp` and `mProbeNotedDiscard ≤ Dp` → `mProbeBlurred = true; mProbeBlurStamp = probeFaceCounter()`. A downgrade itself never notifies. |
| H7 | llviewerobject.cpp:5447-5454, :7792-7802, :7816-7823 (`onMaterialComplete` lambdas) | After `findObject` succeeds, add `if (recording() && obj->mDrawable) noteDrawable(obj->mDrawable, R_TEX)`. This makes async PBR completion an event (CX2 P0-6). |
| SH | llreflectionmapmanager.cpp:1562 `shift()` | Offset all of: queued events (`shiftQueued`), streak accumulators, every light entry's `mPos` **and sticky accumulator fields 0-2** (position is first by contract, §4.5.3), every record's `mStartOrigin`/`mStartCam`, and `mRtSliceFrozenOrigin`. No overflow and no resync (OP2 P1-a). |

**H6 rule.**
- **Probe-relevant level `Dp`.** Let `dim = max(getFullWidth(), getFullHeight())` and `pr = probe resolution`. If
  `dim <= 0` or `pr == 0`, then `Dp = 0`. Otherwise `Dp` is the largest k with `(dim >> k) >= pr`, computed with integer
  shifts (no float division). Let `d = getDiscardLevel()` (llgltexture.h:143). If `d < 0`, return.
- **When to notify:**
  - (a) the first real image (`mProbeNotedDiscard < 0`);
  - (b) a coarse refinement: `d < mProbeNotedDiscard && mProbeNotedDiscard > Dp`;
  - (c) a re-sharpen (OP2 N5): `mProbeBlurred && d <= Dp`, and `probeFaceCounter() > mProbeBlurStamp`. When
    `d <= Dp`, clear `mProbeBlurred` whether or not this notified.
- **After notifying,** set `mProbeNotedDiscard = min(noted, d)`, or `d` when it was unset.
- **Fan-out:**
  - `getFaceList(ch)` for `ch < LLRender::NUM_TEXTURE_CHANNELS` → `noteDrawable(face drawable, R_TEX)`;
  - **`getVolumeList(LLRender::LIGHT_TEX)`** (llviewertexture.h:181) → `noteLightVolume(vol, R_TEX)` (CX2 P0-3,
    OP2 N4);
  - more than 4096 total → `noteGlobal(R_TEX, C_GLOBAL)`, which also bumps Live (OP2 P1-d).
- Zone `"probe dirty tex fanout"`.

**Removed:** H5 (parameterChanged) stays removed. Lights come from §4.5.3.

### 4.5 Probe-centric lights (U1)

**4.5.1 Capture scope.**
- New manager members: `bool mOnDemandActive` (set at the top of `update()`); `bool mProbeCentricLights`; public
  `bool isProbeCentricLightCapture() const`.
- In `updateProbeFace`, around each `probe->update(...)` (:1323, :1330), **only if `mOnDemandActive &&
  !mCinematicLiveProbeCapture`**:
  - set `mProbeCentricLights = true`, then `const bool saved = gPipeline.beginCinematicProbeCapture();` — the generic
    save+clear of `mNearbyLights`, :10321-10334;
  - after the call: `if (saved) gPipeline.endCinematicProbeCapture();` then `mProbeCentricLights = false`.
  - Use RAII. It must restore on the early return inside `LLReflectionMap::update` (llreflectionmap.cpp:55-56).
- The scope covers ordinary, default and sliced captures. Live already brackets its own loops (:648-649, :704), so it is
  skipped.
- OFF: no scope → byte-identical.

**4.5.2 `calcNearbyLights` edits (approved).**
- pipeline.cpp:9726-9727 becomes
  `gCubeSnapshot && (mReflectionMapManager.isCinematicLiveProbeCapture() ||
  mReflectionMapManager.isProbeCentricLightCapture())`.
- :9746-9747: `pinned_count = isCinematicLiveProbeCapture() ? getCinematicLiveProbePinnedLightCount() : 0`.
- The ignored and pinned predicates already return false outside Live (llreflectionmapmanager.cpp:1197-1212).
- The deferred loop's effective count (:23216-23221) and its ignored-light check (:23280-23285) are already Live-gated,
  so ordinary captures get `RenderLocalLightCount`.
- **Effects:**
  - The list is built per face around the probe origin, with face frustum and `max_dist = min(RenderFarClip, far)`.
  - NEARBY_LIGHT bits and fade clocks are untouched.
  - The main-eye list is restored after each face.
  - With OFF, the added term is always false.
- The code comment at :9737-9740 is updated to say that ON-path ordinary probes use it too.

**4.5.3 Light diff (camera-independent, replaces V3 §4.5).**
- **Source.** At flush, iterate `gPipeline.mLights` (pipeline.h:1853) through a new public const accessor.
- **Eligibility (a signature field).** A light is eligible when it passes the **filters of the transient branch**
  (:9761-9799, minus the Live-only ignored/pinned and the probe-dependent frustum/distance):
  - LIGHT state, not HUD;
  - attachment: `sRenderAttachedLights`, `bdmerge_should_render_light(true, own)`, and the avatar is not too complex,
    muted or too slow;
  - non-attachment: a rig emitter, or `bdmerge_should_render_light(false, false)`;
  - `radius·1.5 > 0.001`;
  - `colour·scale·EV` squared > 0.001.

  The deferred loop's filters (:23287-23306) are the same subset. `bdmerge_should_render_light` is file-static in
  pipeline.cpp; expose a public static wrapper.
- **Per-light signature**, fields in this order (**position first**, fields 0-2; SH depends on it):
  1. `addAbs3(getPositionAgent, 0.05f)`;
  2. eligible (exact);
  3. then, as pipeline.cpp:9680-9713: colour × scale × EV, radius, falloff, scale, forward/up (0.25°), spotlight,
     spot params, texture id, no-shadow, gobo override fields.
- **No fade.**
- **Events.**
  - `LightEntry{ F32 mPos[3]; F32 mRadius; StickyHash mSticky; U64 mH; U32 mSeen; Streak mStreak; }`.
  - A new entry, `h != mH`, or a vanished entry → a `C_LIGHT|R_LIGHT` event: sphere(pos, max(old, new radius)·1.5) at
    the new position, plus the old position if it moved.
  - Events go through the streak detector (§4.10), keyed by light pointer.
- **Hit rule:** the light sphere vs sphere(O, Rcap).
- **Cap rule (conservative).** When the number of eligible lights is greater than `RenderLocalLightCount`, every light
  event is **global** (it hits all ORDINARY probes). A probe's in-cap set can then change through distance-order shifts
  the per-probe sphere test cannot see. Rare, over-dirty only.
- **Scope.** Live and REALTIME records ignore light events; Live hashes its own lights, :774-775.
- **Cost.** All lights × ~45 floats; zone `"probe sched lights"`; `flush_us` is reported.
- **Why this is camera-independent.** No camera input exists: the list is centred on the probe, and the diff reads
  `mLights`. Main-eye `mNearbyLights` is never read by the scheduler.

### 4.6 Manager integration
- `llreflectionmap.h`: include `alprobeschedule.h`; add `ALProbeSched::Record mSched;` after :138.
- **Members:**
  - `U64 mSchedSerial`, `U32 mSchedEpoch`, `U32 mSchedNextId`, `U32 mSchedWindow`;
  - `bool mSchedWasOn`, `bool mOnDemandActive`, `bool mProbeCentricLights`;
  - `U64 mLiveSceneSerial`;
  - env members; `mLightSnap`; `mRecStreaks`;
  - `SliceCursor mRtSlice`, `LLVector4a mRtSliceFrozenOrigin`;
  - `RefreshAllState mRefreshAll` (§4.11);
  - `SchedWindow`, `SchedFrame`.
- **Public:** `requestRefreshAllProbes()`, `RefreshAllStatus getRefreshAllStatus() const`,
  `isProbeCentricLightCapture()`.
- **Static:** `sCaptureKind` (§8).

**`update()`:**
1. **Top, before :212.**
   - Read `RenderProbeOnDemand` → `mOnDemandActive = on && LLPipeline::sReflectionProbesEnabled`.
   - `ALProbeDirty::setRecording(mOnDemandActive)` and `setProbeResolution(mProbeResolution)`.
   - `mSchedFrame = {}`.
   - The early returns at :214, :223 and :339 also call `mRtSlice.reset()`.
2. **After :341, before :344: `flushProbeSchedule()`.**
   - **OFF:** drain and discard; clear `mLightSnap` and the streaks; `mSchedWasOn = false`.
   - **ON:**
     - **a. Housekeeping.**
       - `frame_serial = ++mSchedSerial`; assign ids; compute owners (§4.8).
       - ON edge: `clearTxn` on every record (this is the only other `clearTxn` site besides `resetProbeSchedule`),
         R_RESYNC on all allocated ORDINARY records, `++mLiveSceneSerial`.
       - Overflow (recorder cap, streak cap, or E×P > 16384): R_RESYNC hits and `++mLiveSceneSerial` **only; never
         `clearTxn`** (OP2 N1).
       - A pending Refresh-all request is armed (§4.11).
       - Grace-init `mLastComplete`.
     - **b. Collect.** Drain → streak filter (§4.10) → the light diff with its own streaks (§4.5.3).
     - **c. Hit test.**
       - **Global events first**: `C_GLOBAL`, `C_TERRAIN_WATER`, R_ENV, and over-cap lights. They hit every
         ORDINARY-owned allocated record whose class mask accepts them, and bump Live per §5.2.
       - Then, per ORDINARY probe, the union-bound early-out.
       - Then per event: `C_LIGHT` sphere vs sphere(O, Rcap); `C_STATIC` AABB vs sphere(O, Rcap), and **only on a
         miss** with `RenderShadowDetail > 0` the 256 m sun/moon capsules.
       - Masks: ordinary `C_STATIC|C_TERRAIN_WATER|C_LIGHT|C_GLOBAL`; default `C_TERRAIN_WATER|C_LIGHT|C_GLOBAL`.
       - The Live footprint for §5.2 events → `++mLiveSceneSerial`, at most once per frame.
     - **d. R_PROBE** for ORDINARY non-default probes: origin > 0.1 m, radius > 0.1 m, ambiance > 0.01, near clip >
       0.01 m, a dynamic or box/sphere flip, or a cube change vs `mStart*`.
     - **e. Default probe.**
       - R_PROBE if the **camera** moved > 16 m from `mStartCam`.
       - R_CLOUD if the cloud scroll delta moved > 1e-4 and at least `RenderDefaultProbeUpdatePeriod` has passed since
         the last start.
     - **f. Ordinary env (§4.3).** Then `mSchedWasOn = true`.
3. **Selection** (:471-477): add `&& (!mOnDemandActive || (owner == ORDINARY && eligible(evaluate(...))))`.
   - The occluded branch :458-468 is unchanged, **except** under the barrier: `evaluate` returns BARRIER. The condition
     at :458 becomes `probe->mOccluded && probe->mComplete && !(mOnDemandActive && probe->mSched.mBarrier)`, so
     barrier probes reach selection.
4. **Default rule** (:596-607). OFF: verbatim. ON: as in V3, with BARRIER counted as eligible.
5. **Txn start** (:610-620), ON only: `mRadiancePass = false; onTxnStart(...)`, and `ALProbeDirty::bumpProbeFaceCounter()`
   per face (at the :357 and :619 call sites and in the sliced path).
6. `oldestOccluded` (:622-627) is unchanged.
7. **Counters at call sites, and the Live pass counters (§4.11):**
   - FULL :555 (kind = `full_radiance`);
   - budget :573 (`mCineStats.mIrrPub`/`mRadPub` delta);
   - Every-frame :592 when `realtime_probe == cinematicLive` (kind = `mRealtimeRadiancePass` before the call).
   - At the end of `update()`: fold into the window, then `logProbeSchedule`.

**`doProbeUpdate` (:1256-1288):**
- Irradiance end (:1279-1282) → `onTxnIrradianceDone`.
- Radiance end (:1273-1277) → `onTxnComplete`, then clear the barrier (§4.11).
- Debug letters: G T L E P S C R W D, plus `B` (barrier) and `Z` (settle).

**`resetProbeSchedule()`:** `++mSchedEpoch`; `clearTxn` on all records; clear the snapshot, streaks, `mRtSlice`, env and
the barrier; `mSchedWasOn = false`. Call sites: :2012-2019 and :2078-2086.

### 4.7 Transaction contract
- `onTxnComplete` acks only when all of these hold: `mInTxn && mTxnIrrDone && epoch == mTxnEpoch && cube ==
  mTxnCube`.
- **On ack:**
  - `mAckSerial = max(mAckSerial, mTxnSerial)`;
  - set `mLastComplete` and `mLastReasons`;
  - if `mBarrier && mTxnSerial >= mRefreshAll.mSerial`, clear `mBarrier`;
  - pending/first-dirty bookkeeping as in V3.
- **On failure:** `clearTxn` plus an R_RESYNC hit.
- **`clearTxn` is called only by:**
  - `onTxnComplete`;
  - the ON edge;
  - `resetProbeSchedule`.

  **Never by overflow or Refresh-all** (OP2 N1).
- **Toggling:**
  - ON→OFF: the completion is skipped.
  - OFF→ON: no ack (not `mInTxn`), and the ON-edge resync handles it.
- Records live in the probe object. Cursors use `mId`.

### 4.8 Scheduler ownership
- **LIVE:** `probe == mCinematicLiveProbe`.
- **REALTIME:** the sliced path ran for this `mId` last frame. This is only possible when `mComplete` (§5.1), N = 1..6.
- **ORDINARY:** everything else, including an incomplete closest-dynamic probe (CX2 P0-4).
- Non-ORDINARY records take no hits, are not selected, and are excluded from statistics.
- **Transitions back to ORDINARY:**
  - LIVE→ORDINARY: R_RESYNC.
  - REALTIME→ORDINARY: **no resync**. The probe is dynamic, so already always eligible (OP2 E).

### 4.9 What remains backstop-only (tooltip + log header)
- **Frozen without a settle event** (§0.3):
  - flexi, media, water waves;
  - cloud scroll in non-default probes;
  - probe-to-probe bounce;
  - reverted transients;
  - sharpening beyond `Dp` without an intervening blur;
  - rotation inside an unchanged AABB of a drawable that never receives `updateMove`.
- **Frozen until settled** (§4.10): anything the streak detector suppresses.
- **Backstops:**
  - the safety refresh `RenderProbeMaxAge` (10..600 s; **0 is not allowed**);
  - **Refresh all probes now** (§4.11).

### 4.10 Streak detector (U2, OP2 N1)
`ALProbeSched::Streak { U16 mCount; F64 mLastSeen; bool mSuppressed; F32 mMin[3], mMax[3]; U16 mReasons;
U8 mClass; }`, with `Streak* get(key)` held in `std::unordered_map<const void*, Streak>`. There is one map in the manager
for recorder events and one inside `mLightSnap` entries for lights.

**Per coalesced event with key k (after drain, before the hit test):**
- If `now − mLastSeen ≤ 0.25 s` → `mCount++`, else `mCount = 1`. Then set `mLastSeen = now`. The gap tolerance lets
  sources updating every 2-3 frames still form a streak.
- If `mCount >= 8` → `mSuppressed = true`. Union the event's bounds, reasons and class into the streak, drop the event,
  and `drop_cont++`.
- Otherwise pass the event through.

**Each flush:**
- For every suppressed streak with `now − mLastSeen > 0.5 s`: emit **one settle event** (the union bounds, reasons plus
  `R_SETTLE`), then erase it.
- Non-suppressed streaks are erased after 0.25 s unseen.

**Limits and effects:**
- Map cap: 8192 keys → overflow.
- `shift()` offsets the streak bounds.
- Suppression covers llTargetOmega spinners (H1a, llviewerobject.cpp:7337-7361), Prop Mover, physical movers and
  vehicles, dragging with the edit tool, texture animation (TXa), rig flicker (`applyFlickerModulation`,
  alcinelightrig.cpp:303, via the light diff) and animated facelights.
- The first 7 events of a streak still dirty their probes: one refresh near motion onset. Then the probes settle, and
  refresh once more on the settle event.
- Live is bumped only by non-suppressed or settle events (§5.2), so Live idles beside a spinner.

### 4.11 Refresh-all capture barrier (CX2 P0-6)
- **`requestRefreshAllProbes()`** (Lightbox button; ON only) sets `mRefreshAll.mRequested`. It also calls
  `requestCinematicLiveProbeRefresh()`, which takes effect in On change mode.
- **Arming (flush step a):**
  - `mRefreshAll = { mSerial = frame_serial, mStart = now, mTotal = n, mLivePending = (Live designated) }`;
  - every allocated ORDINARY record (default, occluded and dynamic probes included) gets `mBarrier = true` plus an
    R_RESYNC hit;
  - `++mLiveSceneSerial`.
- **Progress:** pending = the allocated ORDINARY records with `mBarrier`.
  - A probe that is deleted or loses its slot leaves the count.
  - Probes allocated after arming are not part of the barrier; they warm up normally.
- **Occluded probes are captured.** `evaluate` returns BARRIER, and selection lets it through (§4.6 step 3).
- **Changes arriving mid-refresh** are normal dirt. They do not extend the barrier. The barrier guarantees that every
  probe has completed a capture **started after arming**.
- **Live** is done after one irradiance pass followed by one radiance pass, both completed after arming (the counters of
  §4.6 step 7). With no Live Probe, it is done immediately.
- **Done:** pending == 0 and Live done. Log `[ProbeSched] REFRESH-ALL DONE n=<n> t=<s>s`.
- **Readout:** the Lightbox text `probe_refresh_status`, updated in `ALFloaterLightBox::draw()`
  (alfloaterlightbox.cpp:240) at ≤ 4 Hz:
  - `Refreshing probes: k/n, Live: pending|done`;
  - `All probes refreshed (n probes, 3.4 s)`, for 10 s;
  - empty otherwise.
- `getRefreshAllStatus()` returns `{active, done, total, pending, livePending, seconds}`.
- A new press while active re-arms.
- `resetProbeSchedule` cancels the barrier, and the readout shows `Refresh cancelled (reset)`.

---

## 5. B-remainder
### 5.1 Closest-dynamic slicing
- **Dispatch** in the non-Live branch (:583-594), ON:
  - if `realtime_probe && realtime_probe != cinematicLive && realtime_probe->mComplete` →
    `updateRealtimeSliced(realtime_probe, N)`, with N = clamp(`RenderProbeRealtimeFacesPerFrame`, 1, 6). **N=6 is
    included.**
  - If the probe is incomplete, **no realtime capture this frame**. It stays ORDINARY and gets WARMUP through the
    ordinary queue. `mRtSlice.reset()`.
  - OFF: the unchanged `updateRealtimeProbeAllFaces` path.
- **Cursor (SliceCursor):** keyed by `(mId, cube)`; a pass starts at face 0; the pass kind alternates as in
  alcineliveproberefresh.h:483/:539/:556 (that header is not edited).
- **Per-frame behaviour:**
  - If `probe == mUpdatingProbe`, skip the frame (`rt_blocked`).
  - Freeze the origin per pass.
  - Set and restore `mRadiancePass`.
  - Each face goes through `updateProbeFace`, so it gets the probe-centric light scope (§4.5.1) and `bumpProbeFaceCounter`.
  - `updateNeighbors` at the end of a pass.
  - Never write `mComplete`, readiness or `mRealtimeRadiancePass`.
  - Zone `"rmmu - rt sliced"`.
- **Invariant:** a pass starts at face 0, and Live and sliced passes are mutually exclusive per frame.

### 5.2 Live Probe scene source
- `sampleCinematicH`, after :892: `if (ALProbeDirty::recording()) sig.addExact(mLiveSceneSerial);`. With recording
  OFF, H is bit-identical (TUT S13 plus the moved-code diff).
- `mLiveSceneSerial` counts, within the Live footprint:
  - non-tagged, non-suppressed `C_STATIC|C_TERRAIN_WATER` events;
  - settle events;
  - `C_LIGHT` events **with R_TEX** (gobo arrival);
  - globally: `C_GLOBAL` (large textures), overflow, the ON edge and Refresh-all.
- Pure light-parameter events (R_LIGHT) are excluded, because Live hashes its own lights.
- **Why Live idles under camera motion:**
  - LOD, mesh, terrain, tree, grass and texture-animation toggles are tagged;
  - texture refinements beyond `Dp` do not notify;
  - light membership is probe-centric;
  - continuous movers are suppressed.

---

## 6. Settings, UI, presets, debug, log, Tracy
### 6.1 Settings (`app_settings/settings.xml`, after `RenderDefaultProbeUpdatePeriod` :20703-20713)
| Name | Type | Default | Clamp | Persist |
|---|---|---|---|---|
| RenderProbeOnDemand | Boolean | 0 | — | 1 |
| RenderProbeMinInterval | F32 | 1.0 | 0..10 s | 1 |
| RenderProbeMaxAge | F32 | 60 | **10..600 s (0 not allowed)** | 1 |
| RenderProbeDirtySunDeg | F32 | 0.5 | 0.1..5 | 1 |
| RenderProbeRealtimeFacesPerFrame | S32 | 2 | 1..6 | 1 |
| RenderProbeTinyCullPixels | F32 | 0 | 0..8 | 1 |
| RenderProbeSchedLog | Boolean | 0 | — | 0 |
| RenderProbeSharedCull | Boolean | 0 | only if C1 approved | 1 |

There are 7 keys (8 with C1). The streak constants (8 events, 0.25 s gap, 0.5 s quiet) are `constexpr` in
alprobeschedule.h, not settings.

### 6.2 UI
- **Lightbox → Rendering** (floater_lightbox_settings.xml): insert immediately after :1537 (`</button>`) and before :1538
  (`<!--Probe Draw Distance-->`).
  - Controls as in V3; the MaxAge slider is 10-600.
  - Then the **"Refresh all probes now"** button (`LightBox.RefreshAllProbes`, registered next to
    alfloaterlightbox.cpp:67-77).
  - Then the text `probe_refresh_status` (§4.11).
  - Raise the height at :1320 by the rows added.
  - Tooltips give the §4.9 list in one sentence.
- **Graphics → Advanced:** the same one-row rule as V3 (after :954-983, collision check vs top 480).

### 6.3 Presets
The 6 names go before `"RenderReflectionProbeDetail"` (llpresetsmanager.cpp:342). Do not touch :343.

### 6.4 `[ProbeSched]` log (every 5 s of manager frames; on and off paths)
```
[ProbeSched] mode=on|off probes=<n> faces=<ord/s> rt=<rt/s> starts done nack
 g t l e p s c r w d b z=<txn starts by reason>  events raw nl drop_cont settles
 drop_lodvol drop_lodmesh drop_lodterr drop_lodtree drop_lodgrass drop_texanim drop_dyn noop_move rebal overflow
 dirty deferred wait maxlag rt_blocked env_changes flush_us=<avg/max> barrier=<pending>/<total>
 verdict=<OK|UNSETTLED id=<k> reasons=<letters>|STARVED n worst|OVER BUDGET ...|OFF-BASELINE|PAUSED>
```
- ORDINARY only.
- **UNSETTLED (OP2 F):** the same ORDINARY probe has started in **3 consecutive windows** with reasons other than
  S/W/R/B (safety, warm-up, resync, barrier). It turns "never settles" into a stated FAIL whenever the tester made no
  edits in those windows.
- **OVER BUDGET:** an implementation-bug detector (structurally unreachable).
- **STARVED:** expected when `12·P/fps > 5 s` after a global event (P ≳ 25 at 60 fps).
- **Priority:** OVER BUDGET > PAUSED > UNSETTLED > STARVED > OK.

### 6.5 Tracy
Zones:
- `"probe sched flush"`, `"probe sched env"`, `"probe sched lights"`, `"probe sched streak"`;
- `"probe dirty tex fanout"`, `"rmmu - rt sliced"`, `"probe tiny cull"`;
- the C0 zones.

There are no zones inside the hooks.

## 7. C — shared cull
- **7.1 C0 (built):** zones in `display_cube_face` (llviewerdisplay.cpp:1290-1365): cull :1307, shadow :1312, sort
  :1316-1330, geom :1353-1357, light (`renderDeferredLighting`).
- **7.2 C1:** unchanged from V3 §7.2. Exact form, pixel-identity argument, saving bounded by the traversal share that C0
  measures, profitability unproven. User approval required.

## 8. D — tiny-group cull (default OFF)
Unchanged from V3 §8:
- The `sCaptureKind` RAII sits in `updateProbeFace`; the filter sits in `postSort` after :5111-5116.
- `rejectable` = `PARTITION_VOLUME`, not a bridge, and all pass keys are legacy opaque/masked non-emissive. **Every
  `PASS_GLTF_PBR*` group is kept.**
- The `tiny` bound: `p = 3·r·res/(d−r)`.
- Excluded: `!sShadowRender`, `!sPrismLensRender`.

---

## 9. Review package
### 9.1 Off-path inertness
| Off | Prove |
|---|---|
| RenderProbeOnDemand=0 | Hooks cost one branch on `gProbeDirtyRecording`, and none of the new members is written. `isProbeCentricLightCapture()` is false, so `calcNearbyLights` behaves as HEAD and the pinned count is unchanged for Live. No light scope in `updateProbeFace`. The flush drains and discards. Selection, default, dispatch, `doProbeUpdate`, `mRadiancePass` and debug text are identical. Live H is identical (TUT S13). |
| Others | As V3. |

### 9.2 Isolation proofs
The V3 list (a)-(i), plus:
- (j) the light scope restores `mNearbyLights` after every ON-path face, and never nests with Live
  (`mCinematicLiveProbeCapture` check plus the nested-rejection guard at :10323-10329);
- (k) the main view, Prism and Hero never see the probe-centric list;
- (l) `clearTxn` is called only at the three documented sites.

### 9.3 Least-sure list
1. Is llTargetOmega / `applyAngularVelocity` (llviewerobject.cpp:7337) throttled by visibility? If it is, turning the
   camera stops a spinner, and the settle becomes camera-driven dirt. T23b checks.
2. Other camera-driven rebuilds not yet tagged (particles, impostors, the media `mVisible` path, which is not hooked).
3. Streak thresholds (8 events / 0.25 s / 0.5 s) versus sources updating below 4 Hz, which never form a streak and dirty
   each time.
4. Probe-centric list correctness: the per-face frustum and `max_dist`; the forward `setupHWLights` fade value (all
   LIGHT_FADE_TIME, as for Live).
5. The over-cap global rule, and far spot lights.
6. The H6 re-sharpen stamp semantics and the `Dp` math.
7. The barrier with occluded probes and deletions.
8. Mesh LOD `mProbeBuilt` for objects built before recording turned ON (conservative: one extra event).
9. **Is the approach wrong?**

---

## 10. In-world test plan (log on; Tracy 10 s per step; outcomes stated in advance)
- **Latency model:**
  - no one ahead in the queue: ≈ 12 frames;
  - k probes ahead: ≈ 12(k+1) frames;
  - plus MinInterval if the probe started within the last second;
  - occluded probes wait unless under the barrier.
- A contradicting counter is a FAIL; PAUSED is INCONCLUSIVE; UNSETTLED is a FAIL unless the tester was editing.
- **The default flip needs T0-T36 all PASS.**

**T0-T22 (V3, with these changes):**
- **T2:** probes within Rcap ≈ 131 m (or reached by a shadow capsule) are dirtied.
- **T5:** light edits → `nl`, `l`. Continuous flicker → `drop_cont`, then one settle.
- **T6:** attachment-light walking → streak-suppressed, `drop_cont` rises, one settle after stopping.
- **T7 STRICT.** A loaded static set; orbit and fly **twice**. Measured on the second pass: `g = t = l = e = nl = 0`,
  `p = 0` for non-default probes, default `p` only per 16 m of travel, faces/s = the safety rate, **no verdict other
  than OK**.
- **T14:** a region crossing adds `nl = 0` and `r = 0` (REALTIME flips excluded).
- **T18:** MaxAge cannot be set to 0. At 10 s, `s` = P per 10 s.
- **T19 Refresh-all:** the readout counts down to "All probes refreshed". Occluded probes are included, and so is the
  Live pair.

**New tests:**
- **T23 Spinner + Prop Mover prop** in a static set → `drop_cont` rises, probes settle (UNSETTLED absent), Live idles
  (`[LiveProbe] converged=y`).
  - **T23b:** orbit the camera around the spinner → no settle events while it keeps spinning.
- **T24 Rig with flicker, and a crowd wearing facelights** → settle, no UNSETTLED.
- **T25 Mesh-heavy dolly** → `drop_lodmesh > 0`, `g ≈ 0` once meshes have loaded once.
- **T26 VRAM-pressure turn-away / turn-back** → one `t` for the re-sharpened textures only if a probe rendered in
  between, and the reflection re-sharpens within the latency model.
- **T27 Projector gobo first load** (clear the cache) → an `l`/`t` light-texture event, and the probes and Live re-render
  with the gobo.
- **T28 More than 256 lights plus an orbit** → no light dirt (camera-independent). A light edit then produces global
  dirt (cap rule).
- **T29 Change `RenderLocalLightCount`** → one `e`, all probes.
- **T30 Late gobo upload while a probe is mid-capture** → a follow-up refresh (serial > txn).
- **T31 Distant terrain edit (> 131 m)** → hits every probe (global class).
- **T32 Overflow beyond both caps** (a region load storm) → `overflow > 0`, `nack = 0` caused by overflow, and every
  probe completes.
- **T33 Incomplete realtime probe** (Realtime detail, fresh dynamic probe) → WARMUP through ordinary, then
  `"rmmu - rt sliced"`.
- **T34 Edit a prim while its LOD is changing** (dolly while recolouring) → `t`/`g` recorded (edits are never tagged).
- **T35 Transaction rejection:** teleport, reset (EEP swap) and a probe-count change mid-capture → `nack` increments
  and those probes are redone. No stale ack: debug letters show `R`.
- **T36 Live OFF-hash equality:** TUT S13 PASS, plus the moved-code diff reviewed. In-world, with on-demand OFF, the
  `[LiveProbe]` idle/converge pattern matches the V3 T8 baseline.

---

## 11. Implementation order (single delivery; one build after review converges)
- [ ] 1. `alprobeschedule.h` (+ Streak, EnvSample builder) + CMake.
- [ ] 2. TUT (14 tests; count them):
  - S1-S12 (V3), with S7 extended: overflow never clears a txn;
  - S13 env builder layout/hash equality against HEAD;
  - S14 Streak: 8 events within 0.25 s suppress, a quiet 0.5 s gives one settle with the union bounds, a sub-streak
    rate passes through, and shift offsets the accumulators.
- [ ] 3. `alprobedirty.{h,cpp}`.
- [ ] 4. settings.xml: 7 keys.
- [ ] 5. **Hooks:** H1a, H1b, H2, H3, H4, MS (+ llvovolume.h members), LV, LT, LTr, LG, TXt, TXa, H6 + DS
  (+ llviewertexture.h members), H7, SH.
- [ ] 6. **pipeline:** the `mLights` accessor, the `bdmerge` wrapper, and the `calcNearbyLights` edits (§4.5.2).
- [ ] 7. **Manager:**
  - gatherer + extraction;
  - records, ids, epoch;
  - the flush (ON edge, overflow, barrier arming, drain, streaks, light diff, global-first hits, R_PROBE, default, env);
  - selection with the barrier;
  - the default rule;
  - txn start/irradiance/complete;
  - the light scope in `updateProbeFace`;
  - sliced realtime;
  - the Live serial;
  - Live pass counters;
  - Refresh-all status;
  - debug text;
  - reset sites;
  - log.
- [ ] 8. **D + C0.**
- [ ] 9. **UI:** Lightbox block + button + status text + the draw() update; the Graphics row; presets.
- [ ] 10. Adversarial review (Opus + Codex) with §9 verbatim, looping to 0 must-fix.
- [ ] 11. Checkpoint commit + `AlchemyTest.pre-probeondemand.exe`. ONE build. The user runs §10.

## 12. OFF-LIMITS, allowed files, reports
**OFF-LIMITS:**
- shaders;
- `llheroprobemanager.*`, `llprismlens.*`;
- `alcineliveproberefresh.h`;
- the `updateRealtimeProbeAllFaces` / `updateCinematicBudget` bodies, and `decide`;
- `alcinelightrig*`, `alcinerigrim*` (read-only calls);
- `alpanelcinelightrig.*`, `panel_cine_light_rig.xml`;
- LiveProbeConfig;
- the `updateProbeFace` body except the two scopes (capture kind, light list);
- `llviewerwindow.cpp`;
- `llenvironment.*` (getters only);
- `llviewermedia.cpp`, `llflexibleobject.cpp` (not hooked, §0.3);
- in pipeline.cpp: everything except the H3 lines, the D filter, the two `calcNearbyLights` edits and their comment
  (:9726-9747), and the new public wrapper/accessor. In particular, keep :25988-25990, :5162-5172, the deferred loop
  :23214-23345 and `calcPixelArea` untouched;
- `calcLOD`;
- llpresetsmanager.cpp:343;
- `enve`, `vcpkg`.

**Allowed:**
- new files: `alprobeschedule.h`, `alprobedirty.{h,cpp}`;
- `CMakeLists.txt`;
- `llreflectionmap.h`, `llreflectionmapmanager.{h,cpp}`;
- `lldrawable.cpp`, `llspatialpartition.cpp`;
- `pipeline.{h,cpp}` (as above);
- `llvovolume.{h,cpp}`;
- `llviewertexture.{h,cpp}`, `llviewertexturelist.cpp` (DS only);
- `llviewerobject.cpp` (H7 only);
- `llsurfacepatch.cpp`, `llvotree.cpp`, `llvograss.cpp` (tags only);
- `llviewerdisplay.cpp` (C0);
- `alfloaterlightbox.{h,cpp}` (callback + status text);
- `app_settings/settings.xml`, `floater_lightbox_settings.xml`, `floater_preferences_graphics_advanced.xml`;
- `llpresetsmanager.cpp`;
- `tests/alcinelightrigmodel_test.cpp`.

**Reported, not fixed (upstream):**
1. llpresetsmanager.cpp:343 has a missing comma.
2. `deleteProbe` (:1238-1242) does not reset `mRadiancePass`. The OFF path can mark a probe complete without its
   irradiance pass. The ON path neutralises this (§4.6 step 5).
