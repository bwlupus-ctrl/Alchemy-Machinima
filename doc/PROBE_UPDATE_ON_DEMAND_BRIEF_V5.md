# Reflection probes: update on demand — implementation brief V5 (self-contained)

**Status:** DESIGN ONLY (Opus, 2026-09-30). Nothing implemented, built or committed. This is the **single implementation
source**; V1-V4 are superseded and need not be read.

**Base:** `fix/animesh-clone-pose-polish` @ `303065bda35`, **plus** the separate, pending `[LiveProbeFaceLights]` fix.
That fix is in the working tree at llviewerdisplay.cpp:1292-1305: it calls `calcNearbyLights` after
`display_update_camera()` for Live captures. V5 assumes it exists and extends it (§4.5.2). Every anchor was re-read at
HEAD. Anchors in llviewerdisplay.cpp after line 1290 are quoted by statement, because that fix shifts them.

Paths without a directory prefix are `indra/newview/`.

**Process (CLAUDE.md):**
1. Sonnet implements.
2. Opus + Codex adversarial review, looping to 0 must-fix.
3. Checkpoint commit + exe backup.
4. ONE build.
5. The user runs §10.

**Labels:** PROVES = read in code; INFERENCE = estimate.
**Rulings:**
- **User rule:** any finding that breaks the promise is a blocker.
- **U1 (probe lights):** with on-demand ON, captures light from a list built around the probe, and camera motion never
  dirties probes through lights. The OFF path is byte-identical. A different ON look is accepted.
- **U2 (continuous motion):** freeze continuous motion until it stops, then refresh once. The safety refresh and
  Refresh-all are the backstops.

**Open user decisions:**
- **UD1:** C1 (§7.2).
- **UD2:** flexi/media stop signal (§4.12). No camera-independent signal exists. An opt-in cadence backstop is designed
  and defaults OFF.

---

## 0. Resolution tables

### 0.1 Round 3 (CX3 = Codex R3, OP3 = Opus R3)
| ID | Finding | V5 resolution |
|---|---|---|
| OP3 P0-1 | Probe-centric list empty in the deferred pass (loop at pipeline.cpp:23263 runs before `calcNearbyLights` :7518) | The Live part is fixed separately (`[LiveProbeFaceLights]`). §4.5.2 extends that same call site to `isProbeCentricLightCapture()`. T37 checks it. |
| CX3 P0-1 | Live bypasses suppression: moving attachment lights hashed at :774; flicker → `liveProbeAnimating` (alcinelightrig.cpp:1206-1230) → FULL (alcineliveproberefresh.h:489-504) | §5.2: an ON-path Live H (`sampleCinematicHOnDemand`) takes lights from the debounced light diff (published = frozen values) and passes `animating=false`. The OFF path calls the unchanged `sampleCinematicH`. |
| CX3 P0-2 / OP3 P1-2 | `isTooSlow` (llvoavatar.cpp:9954, AutoTune-driven) makes eligibility camera-dependent | §4.5.4: ON drops the `isTooSlow` term from both the probe-centric capture filter (pipeline.cpp:9778-9780) and the diff. AutoTune's *setting* changes remain real ENV changes; T7 requires AutoTune off (§10). |
| CX3 P0-2 | Shifted accumulators leave a stale cached `mH` (hash over accumulator bits, alcineliveproberefresh.h:339-354) | §4.5.5: `ALProbeSched::stickyDigest()` recomputes every stored hash after `shift()`. TUT S16. |
| CX3 P0-3 / OP3 P1-1 | Cap transition; sim-wide count; cost | §4.5.6: per-probe local eligible counts, previous and current; the rule applies if either is over cap. §4.5.3: prefilter to the union bound, reused buffers, benchmark T52. |
| CX3 P0-4 / OP3 P0-4/5/6 | Streak lifecycle, slow sources, low fps, real edits hidden, overflow before filtering | §4.10: per-(key, motion class) **debounce** with an adaptive quiet (0.5 s … 30 s, low-fps aware). Discrete edits bypass it. Counts saturate. Keys carry an identity tag. Deletion keeps its bounds. Motion never takes recorder slots, and there is a bulk entry beyond the map cap. E×P is computed after filtering. |
| CX3 P0-5 / OP3 P0-3 | Refresh-all hangs or lies | §4.11: a barrier over **relevant allocated** probes (ORDINARY and REALTIME) with stable identity. Completion requires a transaction/pass **started after arming**, and flags clear only on a real ack. Live is `unavailable` without a cube/relevance. Terminal states: `paused` (EEP), `incomplete k/n` after 30 s, `cancelled`, `removed`. |
| CX3 P0-6 / OP3 P0-8 | Blur stamp overwritten; Live exposure; global face counter makes it camera-driven | §4.4 H6/DS: keep the **first** outstanding blur stamp. The event carries `mMinFaceSerial` and hits only probes, or Live, that rendered a face after the blur (`mLastFaceSerial`). |
| CX3 P0-6 / OP3 P0-7 | Mesh/sculpt swap hidden by the LOD tag | §4.4 MS: built-**asset identity** (sculpt id + type + ready). An arrival is tagged only when the identity matches a ready build. |
| OP3 P0-2 | H6 use-after-free (swap-and-decrement lists, llviewertexture.cpp:1447, :1502) | §4.4 H6 iterates `i < getNumFaces(ch)` (:1480) and `i < getNumVolumes(LIGHT_TEX)` (:1521). |
| CX3 residue | Flexi/media stopping without a settle; slow periodic sources | Slow sources: §4.10 adaptive quiet up to 30 s. Flexi/media: **no camera-independent signal exists** (PROVES, §4.12). Opt-in cadence backstop RenderProbeAnimCadence (default 0) — **UD2**. |
| CX3 P1 | Cost; counterexample tests; T1/T21/S4; S13 wiring | T47-T53 counterexamples, T52 CPU benchmark; T1/T21/S4 updated. S13 = moved-code line-diff review + an **independent golden H** (§10 S13). |
| OP3 P1-3 | S13 not independent | Same as above. The golden literal is produced by the reviewer, not the implementer. |
| CX3/OP3 P2 | H4 `isState` qualification; friend/public access; narrowing/unused; MaxAge wording; first-frame slicing; H7 redundancy | §4.4 H4 (`mDrawable->isState(LLDrawable::REBUILD_POSITION)`); §4.6 all new pipeline-called manager methods are public; §9.4 compile rules; §6.1 MaxAge is "due", not a deadline; §5.1 first sliced frame; H7 kept (harmless). |

### 0.2 Earlier rounds (all carried into this document)
- **R1 Codex:**
  - P1-1 → §4.6 step 2e.
  - P1-2 → §4.4 tags.
  - P1-3 → H1a.
  - P1-4 → §4.7.
  - P1-5 → §4.8.
  - P1-6 → §8.
  - P1-7 → §10.
  - P2 → §1 D3, §4.2, §4.3, §7.2.
- **R1 Opus:**
  - P0-1 → §4.4 LT/LTr/LG/TXt, §5.2.
  - P1-1 → LV/H4.
  - P1-2 → §4.5.
  - P1-3 → §4.2.
  - P1-4 → H6.
  - P1-5 → §4.6.
  - P2 → §4.3, §4.4 SH, §4.6 2d, §4.7, §6.
- **R2 Codex:**
  - P0-1..6 → §4.5, §4.5.6, H6, §5.1, §4.6 2c, H7 / §4.11 / MaxAge clamp.
  - P1 → §10.
  - P2 → SH, §4.3, H6 guards.
- **R2 Opus:**
  - N1 → §4.10.
  - N2 → MS.
  - N3 → §4.5.
  - N4 → H6.
  - N5 → DS.
  - P1-a..d → SH, §4.3, §4.3, H6.
  - E → §4.8, H6, `rebal`.

### 0.3 Rejected / partly fixable / needs the user
- **Rejected:** none.
- **Partly fixable:**
  - (a) Flexi and media have no camera-independent "stopped" signal (§4.12, PROVES). They stay frozen, backed by
    MaxAge, Refresh-all and the UD2 option.
  - (b) A colour/intensity edit on a light whose colour is itself continuously animated (flicker) joins the same
    motion class and shows when the animation stops. The same holds for an XFORM edit of a continuously moving light.
    Other classes are unaffected.
  - (c) With AutoTune active, its automatic setting changes are real ENV changes. T7 is specified with AutoTune off.
  - (d) The Live Probe keeps following its subject. Its own origin motion is its purpose and is not frozen.
- **Needs the user:** UD1 (C1) and UD2 (the flexi/media backstop). Also: the ON-look difference now includes lights of
  "too slow" avatars (§4.5.4).

---

## 1. Decisions
| # | Decision | Why |
|---|---|---|
| D1 | `RenderProbeOnDemand` ships **false**. The flip is a separate commit after **T0-T53 all PASS**. | Coverage is proven only in-world. |
| D2 | Batched event push → per-probe serials. No octree counters. | R§2. |
| D3 | Complete dynamic ordinary probes are always eligible and queued. MinInterval (≥ 1 s) separates their starts, so their cadence depends on the queue. DYN-class geometry is dropped; lights come from §4.5. | R§7. |
| D4 | Ordinary: 1 face per `update()`. Realtime: ≤ 6 (Live FULL/budget or sliced N). Total ≤ 7/frame, the same as today. | Structural. |
| D5 | Occluded complete probes are deferred, except under the Refresh-all barrier. | R§4. |
| D6 | Live: no min-interval. Its origin is not frozen. Scene motion is frozen (§5.2). | Live's contract. |
| D7 | ON: a complete closest-dynamic probe always takes the sliced path (N = 1..6). An incomplete one warms up through the ordinary queue. | CX2 P0-4. |
| D8 | Continuous motion is debounced (§4.10). Flexi, media and water are frozen, with backstops (§4.12). | U2. |
| D9 | C1 needs user approval. C0 is built. | CLAUDE.md. |
| D10 | Capture far = `RenderReflectionProbeDrawDistance` (llviewerdisplay.cpp:224-228). Cull reach far·√3 (llvieweroctree.cpp:1379-1382). `Rcap = far·√3 + 20 m` ≈ 131 m. | PROVES. |
| D11 | ON: ordinary, default and sliced captures use a probe-centric light list (U1). | U1. |

## 2. Reconciliation with 615750a40f9
- **DONE:**
  - Live slicing: `updateCinematicBudget` :686-744, `advanceFace` alcineliveproberefresh.h:570-595.
  - Readiness: :650-669, :722-730.
  - Converge-to-idle H: `sampleCinematicH` :750-903.
  - Live scratch blocking: :566-570.
  - Radiance-pass discipline: :546-557, :702-737.
- **PARTIAL:** same-probe ignored/pinned list edits (:1189-1190). Out of scope.
- **This delivery:** the Live scene/lights ON-path (§5.2), the per-probe interval (§4), and closest-dynamic slicing
  (§5.1).
- **Kept unchanged:** the `CineLightRigLiveProbe*` settings, `decide`, the rig-panel card and the `[LiveProbe]` log.

## 3. Scope (one delivery)
- **A:** ordinary + default on-demand, with probe-centric lights, the debounce and the Refresh-all barrier (§4).
- **B-rest:** §5.
- **C0:** §7.1. C1 only if approved.
- **D:** §8 (default off).
- **Surfaces:** settings, UI, presets, debug text, `[ProbeSched]` verdicts, TUT.
- **Deferred:** probe LOD bias (R§6: `calcLOD`/`updateLOD` mutate shared state), shared probe shadows, supersample,
  relightable probes, fast GGX.

---

## 4. A — on-demand scheduling

### 4.1 Pure model `alprobeschedule.h` (header-only; add to `viewer_HEADER_FILES`)
Namespace `ALProbeSched`. All functions `inline`. Include `stdtypes.h`, `lluuid.h`, `<vector>`, `<cmath>`,
`<cstring>`, `<algorithm>`, `<unordered_map>` and `"alcineliveproberefresh.h"` (for `Signature`/`StickyHash`). Use
explicit `static_cast` on every narrowing. No GL or viewer globals.
```cpp
enum Reason : U16 { R_GEOM=1<<0, R_TEX=1<<1, R_LIGHT=1<<2, R_ENV=1<<3, R_PROBE=1<<4, R_SAFETY=1<<5, R_CLOUD=1<<6,
                    R_RESYNC=1<<7, R_WARMUP=1<<8, R_DYN=1<<9, R_SETTLE=1<<10, R_BARRIER=1<<11, R_CADENCE=1<<12 };
enum Class  : U8  { C_STATIC=1<<0, C_TERRAIN_WATER=1<<1, C_LIGHT=1<<2, C_GLOBAL=1<<3 };
enum class Motion : U8 { NONE, XFORM, TEXANIM, LIGHT_XFORM, LIGHT_PHOTO };
struct Event { F32 mMin[3]; F32 mMax[3]; U16 mReason; U8 mClass; U64 mMinFaceSerial; };  // 0 = unconditional
enum class Owner : U8 { ORDINARY, LIVE, REALTIME };
struct Record {
    U32 mId = 0;                                               // monotonic, never reused
    U64 mDirtySerial = 0, mAckSerial = 0, mTxnSerial = 0, mLastFaceSerial = 0;
    U32 mTxnEpoch = 0;  bool mInTxn = false, mTxnIrrDone = false;  S32 mTxnCube = -1;
    U16 mPending = 0, mTxnReasons = 0, mLastReasons = 0;
    F64 mFirstDirty = -1.0, mFirstDirtyInTxn = -1.0, mLastStart = -1.0, mLastComplete = -1.0;
    U32 mStartStreakWindows = 0, mLastStartWindow = 0;          // UNSETTLED (§6.4)
    U16 mLocalLights = 0, mLocalLightsPrev = 0;                  // §4.5.6
    F32 mStartOrigin[3] = {0,0,0}; F32 mStartRadius = 0.f, mStartAmbiance = 0.f, mStartNear = 0.f;
    bool mStartDynamic = false, mStartBox = false; S32 mStartCube = -1;
    F32 mStartCam[3] = {0,0,0}; F32 mStartCloud[2] = {0,0};    // default probe only
    Owner mOwner = Owner::ORDINARY;
};
struct Policy { F32 mMinInterval; F32 mMaxAge; };
struct View   { bool mComplete, mOccluded, mDynamic, mAllocated, mBarrier; };
enum class Why : U8 { NONE, WARMUP, BARRIER, DIRTY, SAFETY, DYNAMIC, WAIT_INTERVAL, DEFERRED };
inline bool dirty(const Record& r) { return r.mDirtySerial > r.mAckSerial; }
inline void hit(Record&, U16 reason, U64 serial, F64 now);
inline void onTxnStart(Record&, U64 serial, U32 epoch, F64 now, U16 reasons, S32 cube /*, capture state */);
inline void onTxnIrradianceDone(Record&, U32 epoch);
inline bool onTxnComplete(Record&, U32 epoch, S32 cube, F64 now);   // §4.7
inline void clearTxn(Record&);
inline Why  evaluate(const Record&, const View&, const Policy&, F64 now);
// hitsCapture (§4.6), Debounce (§4.10), SliceCursor (§5.1), Barrier (§4.11), EnvSample +
// appendEnvironmentFields (§4.3), stickyDigest (§4.5.5), window stats + verdict (§6.4)
```
- **`hit`:** `mDirtySerial = max(., serial)`, `mPending |= reason`. It also records the first-dirty time: in
  `mFirstDirtyInTxn` if `mInTxn`, else in `mFirstDirty` if that is still < 0.
- **`evaluate`, in this exact order:**
  1. `!mAllocated` → NONE.
  2. `!mComplete` → WARMUP.
  3. `v.mBarrier` → BARRIER (ignores occlusion and interval).
  4. `mOccluded` → DEFERRED if dirty or safety-due, else NONE.
  5. `mLastStart >= 0 && now − mLastStart < mMinInterval` → WAIT_INTERVAL if dirty, dynamic or safety-due, else NONE.
  6. dirty → DIRTY.
  7. dynamic → DYNAMIC.
  8. Safety-due (`now − mLastComplete >= mMaxAge`, with `mLastComplete >= 0`) → SAFETY.
  9. Otherwise NONE.
- **Eligible** = WARMUP, BARRIER, DIRTY, DYNAMIC or SAFETY.

### 4.2 Recorder `alprobedirty.{h,cpp}` (new; main thread only; add to CMake)
```cpp
extern bool gProbeDirtyRecording;                               // defined ONCE in alprobedirty.cpp
namespace ALProbeDirty {
    inline bool recording() { return gProbeDirtyRecording; }
    void setRecording(bool); void setProbeResolution(U32); void setFrame(U64 serial, F64 now, F32 frame_dt);
    U64  currentSerial();                                       // last flush serial (texture blur stamps)
    enum Tag : U8 { TAG_NONE, TAG_LOD_VOLUME, TAG_LOD_MESH, TAG_LOD_TERRAIN, TAG_LOD_TREE, TAG_LOD_GRASS,
                    TAG_TEXANIM_TOGGLE, TAG_COUNT };
    struct ScopedTag  { explicit ScopedTag(Tag); ~ScopedTag(); };    // nests, restores previous
    struct ScopedMove { explicit ScopedMove(bool active); ~ScopedMove(); };  // H1b: H2 notes inside = motion
    void noteDrawable(LLDrawable*, U16 reason, ALProbeSched::Motion m);
    void noteDrawableBounds(LLDrawable*, const LLVector4a* old_mn_mx, U16 reason, ALProbeSched::Motion m);
    void noteLightVolume(LLVOVolume*, U16 reason, U64 min_face_serial);   // sphere(pos, radius·1.5), discrete
    void noteGlobal(U16 reason, U8 cls);
    void noteTexture(LLViewerFetchedTexture*, U64 min_face_serial);       // H6 fan-out
    void noteTextureDownscale(LLViewerFetchedTexture*);                   // DS
    void shiftQueued(const LLVector4a& offset);                           // queued events + debounces
    void markOverflow();
    struct Drain { std::vector<ALProbeSched::Event> mEvents; bool mOverflow;
                   U32 mDropped[TAG_COUNT], mDroppedDyn, mNoopMove, mRebal, mRaw, mMotionNotes, mSettles,
                       mBulk; };
    void drain(Drain&, F64 now);                                          // appends due settle events
}
```
- **Discrete notes** (`Motion::NONE`) are coalesced into a per-frame vector plus an `unordered_map<const void*, U32>`
  index: union the bounds, OR the reasons and classes. The cap is 4096, beyond which `mOverflow` is set. The key is
  never dereferenced after the note returns.
- **Motion notes** go **only** to the debounce (§4.10). They never take a vector slot (OP3 P0-6).
- **`mRebal`** counts an insert and a remove of the same key in one frame. **`mMotionNotes`** counts motion notes.
- **`classify`, in this order:**
  1. HUD → drop.
  2. Attachment, animated object, or `LL_VO_PART_GROUP` → DYN → drop (`mDroppedDyn`).
  3. `isReflectionProbe()` → drop.
  4. Surface patch / water / void water → `C_TERRAIN_WATER`.
  5. Otherwise → `C_STATIC`.
- **`worldBounds`:**
  - A drawable in a bridge partition (`asBridge()`, llspatialpartition.h:544) → the bridge's world extents
    (lldrawable.cpp:1328-1370).
  - Otherwise `getSpatialExtents()`.
  - No group or non-finite → sphere(`getPositionAgent()`, 0.5·|scale| + 0.5).
- **Identity tag:** `LLViewerObject::getID()` read at note time.
- **Hooks are read-only:** they never write render, drawable, group or pipeline state.

### 4.3 Environment generation
- **Extraction (Live-safe).**
  - A gatherer in the manager, `ALProbeSched::EnvSample gatherEnvSample()`, calls **exactly** the getters of
    llreflectionmapmanager.cpp:778-891, in order, into named POD fields. This includes `auto_adjust_legacy` (read from
    `RenderSkyAutoAdjustLegacy`, as :756).
  - The pure builder `appendEnvironmentFields(Signature&, const EnvSample&, F32 sun_moon_deg, bool
    blend_only_when_mixing)` appends in HEAD order with HEAD tolerances.
  - The only differences are:
    1. `sun_moon_deg` replaces the literal 0.1 at :786-787.
    2. When `blend_only_when_mixing` is set and the textures do not mix, the blend factors at :817 and :846 become 0. For
       the sky, "mix" means any sun/moon/cloud-noise current ≠ next id; for water, normal/transparent current ≠ next.
  - `sampleCinematicH` calls gather + builder with `(0.1f, false)`. The now-unused :756 declaration is removed.
- **Ordinary env** = builder(`RenderProbeDirtySunDeg`, true) followed by a fixed-layout **tail**:
  - `addAbs(RenderReflectionProbeDrawDistance, 0.5f)`;
  - `addAbs(min(RenderFarClip, RenderReflectionProbeDrawDistance), 0.5f)` (the list range, pipeline.cpp:9754-9757);
  - exact `RenderReflectionProbeDetail`, `RenderReflectionProbeLevel`, `RenderLocalLightCount`;
  - `addAbs(RenderReflectionProbeMaxLocalLightAmbiance, 1e-3f)` (it drives `mLightScale`, :1306-1311;
    pipeline.cpp:22925-22928);
  - exact `RenderAttachedLights`, `BDMergeLightToggles`, `BDMergeRenderOwnAttachedLights`,
    `BDMergeRenderOthersAttachedLights`, `BDMergeRenderWorldLights`, `BDMergeRenderProjectors` (pipeline.cpp:9561-9576),
    and `addAbs(AlchemyGlobalLightScale, 1e-3f)`;
  - Rig Rim: exact `CineRigRimIncludeProbes` and `CineRigRimEnabled`, then **always** the rim block, with values zeroed
    unless both are true:
    - `addAbs` 1e-3 for `CineRigRimMasterGain`, `CineRigRimBackSoftness`, `CineRigRimRoughnessSoften`,
      `CineRigRimShadow`, `CineRigRimTronMix` and **`CineRigRimTint` (F32)** (alcinerigrim.cpp:214-217);
    - exact `CineRigRimDebugRimOnly`, `CineRigRimIncludeAlpha`, `CineRigRimTronMode`, `CineRigRimTronColorSource`;
    - for every `i < SLOT_COUNT` (the constant at alcinelightrigmanager.cpp:441), with `const auto slot =
      static_cast<ALCineLightRigManager::Slot>(i)`: exact `isSlotEnabled(slot)` (alcinelightrigmanager.h:1089-1091),
      then for every `j < ALCineLightRigModel::LIGHT_COUNT`, four `addAbs(…, 1e-3f)` of `at(slot).rigRimParams(j)`
      (alcinelightrig.h:229), **or four zeros** when the slot is disabled or rim is excluded.
  - Verify every setting's type in settings.xml.
- `h != mOrdEnvH` (after the first sample) → an R_ENV **global** event.
- **Rate (INFERENCE, R§3):** a 4 h day at 0.5° gives ≈ 1 event / 20 s, ≈ 0.6 faces/s per probe.

### 4.4 Hook sites (each wrapped in `if (ALProbeDirty::recording())`; nothing else runs when off)
| ID | Site | Behaviour |
|---|---|---|
| H1a | lldrawable.cpp:806 `updateMoveUndamped`, :842 `updateMoveDamped` | Before `updateXform` (:808 / :846), snapshot `mXform.getPosition()`, `mXform.getRotation()`, `mCurrentScale` and a copy of `getSpatialExtents()`. After it: position > 1e-4 m, `1 − \|dot\|` > 1e-7, or scale > 1e-4 → `noteDrawableBounds(this, old, R_GEOM, XFORM)`. The return value of `updateXform` is interpolation error, not displacement (lldrawable.cpp:639-683). llTargetOmega runs for every active object with no visibility gate (llviewerobjectlist.cpp:956-1006; llviewerobject.cpp:2520-2538). |
| H1b | llspatialpartition.cpp:972 `move` | After :977-981, snapshot `worldBounds` and open `ScopedMove(true)`. On the exits :991-992, :1003-1004, :1014: if the union changed by > 1e-4 → note `(R_GEOM, XFORM)`, else `mNoopMove++`. Usually a no-op for volumes: `genBBoxes` updates extents before `movePartition` (llvovolume.cpp:1967), and H4 covers that. |
| H2 | llspatialpartition.cpp:774 `handleInsertion`, :781 `handleRemoval` (before `removeObject`) | `R_GEOM`. **Inside a ScopedMove → XFORM (motion); otherwise discrete** (creation/deletion). Count `mRebal`. |
| H3 | pipeline.cpp:4517 `markTextured` (inside its `if`) → discrete `R_TEX`; :4580 `markRebuild(drawable, flag)` (inside its `if`) → discrete `R_GEOM` iff `flag & ~LLDrawable::REBUILD_POSITION` | Tags apply. The group overload :4566 is not hooked. |
| H4 | llvovolume.cpp:2157 `updateGeometry` | See the H4 rule below this table. |
| MS | llvovolume.cpp:1284-1287 `notifyMeshLoaded`; llviewertexture.cpp:2135-2143 sculpt refinement | New LLVOVolume members (llvovolume.h, written only while recording): `LLUUID mProbeAssetId; U8 mProbeAssetType = 0; bool mProbeAssetReady = false, mProbeLodArrival = false`. If `mProbeAssetReady` and the current (sculpt id, type) equals the stored pair → `mProbeLodArrival = true` and `ScopedTag(TAG_LOD_MESH)` around that `markRebuild`. Otherwise (first arrival, or a **swapped asset**) → untagged, a real event (OP3 P0-7). |
| LV | llvovolume.cpp:1699-1737 `updateLOD` | Whole body under `TAG_LOD_VOLUME`. |
| LT | llsurfacepatch.cpp:1054-1070 (stride block in `updateVisibility` :1008) | `TAG_LOD_TERRAIN`. Real edits stay events: `dirty()` :92, `dirtyPatch()` :611 (llvosurfacepatch.cpp:767-779), composition :878-884. |
| LTr | llvotree.cpp:378-381 (in `idleUpdate` :345) | `TAG_LOD_TREE`. The first build :374-377 and move/rotate :389-403 stay events. |
| LG | llvograss.cpp:368-392 (in `updateLOD` :338) | `TAG_LOD_GRASS`. The render-stopped toggle :347-354 stays an event. |
| TXt | llvovolume.cpp:665-681 (matrix creation), :876-894 (size toggle, `updateTextureVirtualSize` :771) | `TAG_TEXANIM_TOGGLE`. |
| TXa | llvovolume.cpp:616 `animateTextures`, when `result != 0` (after :622) | `noteDrawable(mDrawable, R_TEX, TEXANIM)`. Called for every instance every frame, independent of the camera (llviewertextureanim.cpp:80-85). Start :629 and stop :432/:536 remain discrete (`markTextured`). |
| H6 | llviewertexture.cpp:2121 `postCreateTexture`, after :2135-2143 | See the H6 rule below this table. |
| DS | llviewertexturelist.cpp:1174-1177, after `img->scaleDown(...)` | `noteTextureDownscale(image)`: if the new discard > `Dp` and `mProbeNotedDiscard ≤ Dp` and **not already** `mProbeBlurred` → `mProbeBlurred = true; mProbeBlurStamp = currentSerial()`. The **first outstanding** stamp is kept (CX3). A downgrade never notifies. |
| H7 | llviewerobject.cpp:5447-5454, :7792-7802, :7816-7823 (`onMaterialComplete` lambdas) | After `findObject` succeeds: `if (recording() && obj->mDrawable) noteDrawable(obj->mDrawable, R_TEX, NONE)` (async PBR completion; harmless overlap with H3). |
| SH | llreflectionmapmanager.cpp:1562 `shift()` | Offset queued events, all debounce bounds, every light entry's `mPos`/`mPubPos` and its sticky accumulator fields 0-2 (position first by contract), then **recompute the stored hashes with `stickyDigest`** (§4.5.5). Also offset `mStartOrigin`, `mStartCam`, the cadence registry (§4.12) and `mRtSliceFrozenOrigin`. No overflow, no resync. |

**H4 rule** (llvovolume.cpp:2157 `updateGeometry`).
- **At entry,** compute
  `lod_only = (mLODChanged || (mSculptChanged && mProbeLodArrival)) && !mVolumeChanged && !mFaceMappingChanged &&
  !mColorChanged && !(mSculptChanged && !mProbeLodArrival) &&
  !mDrawable->isState(LLDrawable::REBUILD_POSITION)`.
- If `lod_only`, `ScopedTag(TAG_LOD_VOLUME)` covers the **whole** function, including `genBBoxes`→`movePartition` at
  :1967.
- If `mVolumeChanged`, set `mProbeAssetReady = false` (params changed).
- The flexi branch :2169-2177 is not hooked (§4.12).
- **After that branch,** snapshot `worldBounds`. `content = !lod_only && (any of the four flags)`.
- **Before `return` at :2255:**
  - `content` → a discrete `R_GEOM` over the union of the entry and exit bounds.
  - Bounds changed without content → `(R_GEOM, XFORM)`.
  - On success with `getNumFaces() > 0`, store the asset identity. `mProbeAssetReady` becomes true only when the asset
    is loaded: for mesh, `isMesh() && getVolume()->isMeshAssetLoaded()` (llvolume.h:1123); for a sculpt, the sculpt
    texture has produced geometry (implementer: confirm the predicate around `getSculptLevel()`, llvolume.h:1054).
    Then `mProbeLodArrival = false`.

**H6 rule** (llviewertexture.cpp:2121 `postCreateTexture`, after :2135-2143).
- **New LLViewerFetchedTexture members** (llviewertexture.h), written **only while recording**: `S8 mProbeNotedDiscard
  = -1; bool mProbeBlurred = false; U64 mProbeBlurStamp = 0`.
- **`Dp`:** `dim = max(getFullWidth(), getFullHeight())`, `pr = probe res`. If `dim <= 0 || pr == 0`, then `Dp = 0`.
  Otherwise `Dp` is the largest k with `(dim >> k) >= pr`, using integer shifts.
- **`d = getDiscardLevel()`** (llgltexture.h:143). If `d < 0`, return.
- **Notify on:**
  - (a) the first image, `mProbeNotedDiscard < 0`;
  - (b) a coarse refinement: `d < noted && noted > Dp`;
  - (c) a re-sharpen: `mProbeBlurred && d <= Dp`. The events carry `mMinFaceSerial = mProbeBlurStamp` (OP3 P0-8).
    Then clear `mProbeBlurred`.
- **Then** set `noted = (noted < 0) ? d : min(noted, d)`.
- **Fan-out** (OP3 P0-2):
  - `for (S32 i = 0; i < getNumFaces(ch); ++i)` over `(*getFaceList(ch))[i]`, for `ch < LLRender::NUM_TEXTURE_CHANNELS`
    → face drawable, discrete `R_TEX`;
  - `for (S32 i = 0; i < getNumVolumes(LLRender::LIGHT_TEX); ++i)` over `(*getVolumeList(LLRender::LIGHT_TEX))[i]` →
    `noteLightVolume(vol, R_TEX, minFace)` (gobo arrival);
  - more than 4096 total → one `C_GLOBAL|R_TEX` event carrying the same `minFace`.
- Zone `"probe dirty tex fanout"`.

### 4.5 Probe-centric lights (U1)
**4.5.1 Capture scope.**
- Manager members:
  - `bool mOnDemandActive` (set at the top of `update()`);
  - `bool mProbeCentricLights`;
  - **public** `isOnDemandActive()` and `isProbeCentricLightCapture()`.
- In `updateProbeFace`, around each `probe->update(...)` (:1323, :1330), **only if `mOnDemandActive &&
  !mCinematicLiveProbeCapture`**, open an RAII scope:
  - on entry: `mProbeCentricLights = true; saved = gPipeline.beginCinematicProbeCapture()` (the generic save + clear,
    pipeline.cpp:10321-10334);
  - on exit: `if (saved) gPipeline.endCinematicProbeCapture(); mProbeCentricLights = false`.
  - The scope must restore across the early return in `LLReflectionMap::update` (llreflectionmap.cpp:55-56).
- The scope covers ordinary, default and sliced faces. Live brackets its own loops (:648-649, :704).
- OFF: no scope.

**4.5.2 Building the list before the deferred loop (OP3 P0-1).**
- **In the `[LiveProbeFaceLights]` block** (llviewerdisplay.cpp, directly after `display_update_camera()` in
  `display_cube_face`), extend the gate to:
  ```cpp
  gCubeSnapshot && (gPipeline.mReflectionMapManager.isCinematicLiveProbeCapture() ||
                    gPipeline.mReflectionMapManager.isProbeCentricLightCapture())
  ```
  The comment gains "…and ON-path probe-centric captures".
- **In `calcNearbyLights`:**
  - pipeline.cpp:9726-9727 gets the same `||` term.
  - :9746-9747: `pinned_count = isCinematicLiveProbeCapture() ? getCinematicLiveProbePinnedLightCount() : 0`.
  - The comment at :9737-9740 is updated.
- **Already correct:**
  - The ignored/pinned predicates are capture-gated (llreflectionmapmanager.cpp:1197-1212).
  - The deferred loop's effective count and ignored check are Live-gated (pipeline.cpp:23216-23221, :23280-23285).
  - The list keeps `LIGHT_FADE_TIME` fades (:9817-9818). NEARBY_LIGHT bits and fade clocks are untouched. Spot-shadow
    targets are `!gCubeSnapshot`-gated.
- **OFF:** both added terms are false.

**4.5.3 Light diff (camera-independent).**
- **Source:** `gPipeline.mLights` (pipeline.h:1853), via a new public const accessor.
- **Prefilter:** a light is processed fully if its sphere(pos, 1.5·r) intersects the union AABB of the capture spheres
  of all ORDINARY probes and Live, each sphere(O, `Rcap`).
  - Spot lights outside it are tracked only by (id, eligible) for §4.5.6.
  - Lights outside that are not spots are skipped. An entry that leaves the prefilter emits a discrete event at its
    stored position and is erased.
- **Eligibility** is a signature field, a mirror of the transient branch's filters (:9761-9799) minus the Live
  ignored/pinned lists and the probe-dependent frustum/distance:
  - LIGHT state, not HUD;
  - attachment: `sRenderAttachedLights`, `bdmerge_should_render_light(true, own)`, the avatar is not
    `isTooComplex`/`isInMuteList`, and **(`isTooSlow` only when OFF — §4.5.4)**;
  - non-attachment: a rig emitter, or `bdmerge_should_render_light(false, false)`;
  - `radius·1.5 > 0.001`;
  - colour·scale·EV squared > 0.001.

  `bdmerge_should_render_light` is file-static (:9470), so add a public static wrapper `LLPipeline::probeShouldRenderLight`.
- **Per light, three signatures** (reused buffers, one `Signature` object per kind) with their own StickyHash each:
  - **XFORM** (fields 0-2 = position by contract): `addAbs3(getPositionAgent, 0.05f)`, forward/up angles 0.25°.
  - **PHOTO:** colour × `AlchemyGlobalLightScale` × `localLightEVScale` (Col 0.01/1e-3).
  - **STRUCT:** eligible (exact), radius 0.05, falloff 0.01, scale 0.005, spotlight flag, spot params 0.00436, light
    texture id, `isProjectorNoShadow`, and every `getGoboOverride` field (as pipeline.cpp:9680-9713).
- **Events:**
  - A new entry, removal, or STRUCT change → a **discrete** `C_LIGHT|R_LIGHT` event with bounds sphere(pos,
    max(radius old, new)·1.5), at the new position and the old one. For a removal, also include any pending debounce
    bounds for that light.
  - An XFORM change → debounce `(light, LIGHT_XFORM)`.
  - A PHOTO change → debounce `(light, LIGHT_PHOTO)`.
- **Published values:** `mPubPos` and the published XFORM/PHOTO hashes update only at that class's settle. Live
  (§5.2) reads only these.
- **Scope:** Live and REALTIME records ignore light events. Zone `"probe sched lights"`.

**4.5.4 `isTooSlow` (CX3 P0-2).** With ON, pipeline.cpp:9778-9780 evaluates `avatar->isTooSlow()` only when
`!mReflectionMapManager.isOnDemandActive()`. This applies to the probe-centric **and** Live transient lists. The diff
excludes `isTooSlow` likewise. OFF is unchanged (Live's OFF signature at :9637 is unchanged).

**4.5.5 `stickyDigest` (CX3 P0-2).**
- `inline U64 stickyDigest(const StickyHash& s, U64 exact)` reproduces the tail of `StickyHash::update`
  (alcineliveproberefresh.h:339-354) bit-for-bit.
- Each light entry stores the last `mExact` per signature.
- After SH offsets the fields 0-2 accumulators, every stored hash is recomputed with `stickyDigest`. A crossing then
  produces **no** light event (TUT S16, T14).

**4.5.6 Cap rule (CX3 P0-3, OP3 P1-1).**
- Per ORDINARY probe, keep `mLocalLights` = eligible lights with `|pos − O| − 1.5·r < max_dist`, plus every eligible
  spot (spots are list candidates at any distance, :9815). `max_dist = min(RenderFarClip, draw distance)`.
- **Recount** at ≤ 1 Hz and whenever a light add/remove/eligibility/STRUCT event occurs. Keep the previous value in
  `mLocalLightsPrev`.
- A light event hits a probe spatially (sphere vs sphere(O, Rcap)) **or** unconditionally if `mLocalLightsPrev > cap ||
  mLocalLights > cap`, with `cap = RenderLocalLightCount`.
- With this rule, a cap+1 → cap transition still hits the affected probes.

### 4.6 Manager integration (`llreflectionmapmanager.{h,cpp}`, `llreflectionmap.h`)
- `llreflectionmap.h`: include `alprobeschedule.h`; add a public `ALProbeSched::Record mSched;` after :138.
- **Members:**
  - `U64 mSchedSerial`, `U32 mSchedEpoch`, `U32 mSchedNextId`, `U32 mSchedWindow`;
  - `bool mSchedWasOn`, `bool mOnDemandActive`, `bool mProbeCentricLights`;
  - `U64 mLiveSceneSerial`, `U64 mLiveLastFaceSerial`;
  - env: `Signature mOrdEnvSig; StickyHash mOrdEnvSticky; U64 mOrdEnvH; bool mOrdEnvValid`;
  - `mLightSnap` (`unordered_map<const void*, LightEntry>`), debounce maps, the cadence registry;
  - `SliceCursor mRtSlice; LLVector4a mRtSliceFrozenOrigin`;
  - `Barrier mBarrier`;
  - Live ON-path: `Signature mCineSigOn; StickyHash mCineStickyOn`;
  - `SchedWindow`, `SchedFrame`.
- **Public:** `requestRefreshAllProbes()`, `getRefreshAllStatus()`, `isOnDemandActive()`,
  `isProbeCentricLightCapture()`. Pipeline and floater code call these, so they must be public, not friend-only.
- **Static:** `ProbeCaptureKind sCaptureKind` (§8).

**`update()` edits:**
1. **Top, before :212.**
   - `static LLCachedControl<bool> on_demand(gSavedSettings, "RenderProbeOnDemand", false);`
   - `mOnDemandActive = on_demand && LLPipeline::sReflectionProbesEnabled;`
   - `ALProbeDirty::setRecording(mOnDemandActive); setProbeResolution(mProbeResolution); mSchedFrame = {};`
   - The early returns :214, :223 and :339 also do `mRtSlice.reset()`. If on-demand is ON and the barrier is active,
     they apply its terminal rules (§4.11).
2. **After :341, before :344: `flushProbeSchedule()`.**
   - **OFF:** drain and discard; clear `mLightSnap`, the debounces and the cadence registry; `mSchedWasOn = false`;
     cancel an active barrier ("cancelled (disabled)"); return.
   - **ON:**
     - **a. Housekeeping.**
       - `frame_serial = ++mSchedSerial`, then `ALProbeDirty::setFrame(frame_serial, now, gFrameIntervalSeconds)`.
       - Assign ids; compute owners (§4.8).
       - **ON edge:** `clearTxn` on every record, R_RESYNC on all allocated ORDINARY records, `++mLiveSceneSerial`.
       - **Recorder overflow:** R_RESYNC plus `++mLiveSceneSerial`, **never `clearTxn`**.
       - Arm a pending barrier (§4.11). Grace-init `mLastComplete` for complete probes with −1.
     - **b. Collect.**
       - Drain; this includes due settle events and the bulk entry.
       - Run the light diff (§4.5.3-4.5.6); its due light settles are added here too.
       - Cadence hits (§4.12, only if enabled).
       - Then compute **E×P after filtering**. More than 16384 → treat as overflow.
     - **c. Hit test.**
       - **Global first:** `C_GLOBAL`, `C_TERRAIN_WATER`, R_ENV, and light events that the cap rule makes
         unconditional for a given probe.
       - Then, per ORDINARY probe, the union-bound early-out; then per event:
         - `C_LIGHT`: sphere vs sphere(O, Rcap);
         - `C_STATIC`: AABB vs sphere(O, Rcap); **only on a miss** and with `RenderShadowDetail > 0`, the 256 m
           capsules along −sun and −moon within `r + Rcap`.
       - Skip a probe when `ev.mMinFaceSerial > rec.mLastFaceSerial`.
       - Masks: ordinary `C_STATIC|C_TERRAIN_WATER|C_LIGHT|C_GLOBAL`; default `C_TERRAIN_WATER|C_LIGHT|C_GLOBAL`
         (render mask :1319-1321).
       - `hit(rec, reason, frame_serial, now)` for ORDINARY-owned records.
       - Live bump per §5.2, at most once per frame.
     - **d. R_PROBE** (ORDINARY, non-default): origin > 0.1 m, radius > 0.1 m, `getAmbiance()` > 0.01, `getNearClip()`
       > 0.01 m, a flip of `getIsDynamic()` or box/sphere (`mViewerObject && getReflectionProbeIsBox()`), or
       `mCubeIndex ≠ mStartCube`, all vs the `mStart*` fields.
     - **e. Default probe:**
       - R_PROBE if the **camera** origin (`LLViewerCamera::getOrigin()`) moved > 16 m from `mStartCam` (never compared
         with the +64 m capture origin, :138-147);
       - R_CLOUD if `getCloudScrollDelta()` (llenvironment.h:182) moved > 1e-4 from `mStartCloud` and `now − mLastStart
         ≥ RenderDefaultProbeUpdatePeriod`.
     - **f. Ordinary env** (§4.3). Barrier progress (§4.11). Then `mSchedWasOn = true`.
3. **Selection** (:471-477): add `&& (!mOnDemandActive || (owner == ORDINARY && eligible(evaluate(...))))`.
   - The occluded branch at :458 becomes `probe->mOccluded && probe->mComplete && !(mOnDemandActive &&
     barrierMember(probe))`.
   - The irrelevant-skip at :426-430 is unchanged. Irrelevant probes are not barrier members (§4.11).
4. **Default rule** (:596-607). OFF: verbatim. ON: let `e = eligible(evaluate(default))`.
   - Level 0: `oldestProbe = (e && age ≥ period) ? mDefaultProbe : nullptr` (age from `mLastUpdateTime`, as today).
   - Level > 0: if `e && now − rec.mLastStart ≥ period` → `oldestProbe = mDefaultProbe`; else if `!e && oldestProbe ==
     mDefaultProbe` → `oldestProbe = nullptr`.
5. **Txn start** (:610-620), ON only:
   - `mRadiancePass = false;` (neutralises the stale pass after delete, :1238-1242);
   - `onTxnStart(rec, mSchedSerial, mSchedEpoch, now, rec.mPending | WARMUP | SAFETY | DYN | BARRIER as applicable,
     mCubeIndex, capture state incl. camera / cloud for default)`.
6. **Face bookkeeping:**
   - After each `doProbeUpdate()` (:357, :619): `rec.mLastFaceSerial = mSchedSerial` and `++mSchedFrame.mOrdFaces`.
   - Sliced faces: the same, on the realtime probe's record.
   - Live faces: `mLiveLastFaceSerial = mSchedSerial`, set at the FULL (:555), budget (:573) and every-frame (:592)
     call sites.
7. `oldestOccluded` (:622-627) is unchanged. Safety uses `mLastComplete`.
8. **Counters at call sites only:**
   - `+6` at :555/:592;
   - the `mCineStats.mFaces` delta around :573;
   - Live pass tracking for the barrier (§4.11).
   - At the end: fold into the window, then `logProbeSchedule`.

**`doProbeUpdate` (:1256-1288):**
- Irradiance end (:1279-1282) → `onTxnIrradianceDone`.
- Radiance end (:1273-1277) → `onTxnComplete`. **On ack**, a barrier member whose `mTxnSerial ≥ barrier arm serial` is
  marked done.
- Debug text (:1267-1270) when ON: `"%.1f %s"` with the letters of `mLastReasons`: G T L E P S C R W D Z B A (geom,
  tex, light, env, probe, safety, cloud, resync, warm-up, dynamic, settle, barrier, cadence). OFF: byte-identical.

**`resetProbeSchedule()`:**
- `++mSchedEpoch`; `clearTxn` on all records; clear the snapshot, debounces, registry, `mRtSlice`, env and the ON-path
  Live sticky;
- the barrier → "cancelled (reset)";
- `mSchedWasOn = false`.
- Called from the `initReflectionMaps` reset block (:2012-2019) and `cleanup()` (:2078-2086).

### 4.7 Transaction contract
- `onTxnStart` sets `mInTxn`, clears `mTxnIrrDone`, and sets `mTxnSerial`, `mTxnEpoch`, `mTxnCube` and `mLastStart`.
  It also copies the capture state and the window counters for UNSETTLED.
- `onTxnIrradianceDone` sets `mTxnIrrDone` iff `mInTxn && epoch match`.
- **`onTxnComplete` acks iff** `mInTxn && mTxnIrrDone && epoch == mTxnEpoch && cube == mTxnCube`. On ack:
  - `mAckSerial = max(mAckSerial, mTxnSerial)`;
  - set `mLastComplete` and `mLastReasons`;
  - if not dirty → `mPending = 0; mFirstDirty = −1`, else `mFirstDirty = mFirstDirtyInTxn`;
  - `clearTxn`; return true.
- **Otherwise:** `clearTxn`, an R_RESYNC hit, return false (`nack`).
- **`clearTxn` is called only in:** `onTxnComplete`, the ON edge, and `resetProbeSchedule`. **Never** on overflow, never
  on Refresh-all.
- Changes during a capture survive, because their serial is greater than `mTxnSerial`.
- **Toggling:**
  - ON→OFF: the completion is skipped (`mSchedWasOn` is false).
  - OFF→ON: no ack; the ON-edge resync takes over.
- Records live in the probe object. Cursors and barrier membership use `mId`.

### 4.8 Ownership
- **LIVE:** `probe == mCinematicLiveProbe`.
- **REALTIME:** the sliced path ran for this `mId` last frame (only for complete probes).
- **ORDINARY:** everything else.
- Non-ORDINARY records take no hits, are not selected, and are excluded from dirty/deferred/maxlag/STARVED/UNSETTLED.
- **Transitions:** LIVE→ORDINARY → R_RESYNC. REALTIME→ORDINARY → no resync (dynamic probes are always eligible).
- The first frame of slicing may coincide with an ordinary selection of the same probe. The ordinary transaction
  proceeds and slicing is `rt_blocked` meanwhile; this converges (OP3 P2).

### 4.9 Backstop-only content (tooltip + log header)
- **Frozen, no settle event (§4.12):**
  - flexi motion;
  - media frames;
  - water waves;
  - cloud scroll in non-default probes;
  - probe-to-probe bounce;
  - reverted transients;
  - sharpening beyond `Dp` without an intervening blur;
  - rotation inside an unchanged AABB of a drawable that never receives `updateMove`.
- **Frozen until settle (§4.10):**
  - transform motion (spinners, Prop Mover, physics, vehicles, edit-tool drags, timer `llSetPos`/`llSetRot`);
  - texture animation;
  - light XFORM/PHOTO animation (rig flicker, facelights, moving lights).
- **Backstops:**
  - `RenderProbeMaxAge`, 10-600 s. It sets when a probe becomes **due**; the actual refresh depends on the queue and on
    occlusion, so it is not a deadline.
  - **Refresh all probes now** (§4.11).
  - The optional cadence (§4.12, UD2).

### 4.10 Motion debounce (U2; replaces the V4 streak)
- **State:** `ALProbeSched::Debounce { U64 mIdTag; F64 mLast, mSettleAt; F32 mGapEma; U16 mCount /*saturating*/;
  bool mPending; F32 mMin[3], mMax[3]; U16 mReasons; U8 mClass; }`.
- **Key:** `(pointer, Motion)`. `mIdTag` = a hash of the object UUID. A mismatch means pointer reuse → reset the state.
- **Maps:** one in the recorder (drawable motion) and one per light entry (LIGHT_XFORM, LIGHT_PHOTO).
- **Constants** (`constexpr`):
  - `kQmin(dt) = max(0.5 s, 3 × dt)`, where `dt` is the smoothed frame interval (below 4 fps the quiet window stretches
    with the frame time);
  - `kQmax = 30 s`;
  - `kForget = 120 s`;
  - map cap 16384.
- **`onMotion(key, bounds, reasons, class, now, dt)`:**
  1. If there is no state or the id tag differs → a fresh state (`mGapEma = −1`, `mCount = 0`).
  2. Else `gap = now − mLast`:
     - if `gap > 2·kQmax` → `mGapEma = −1` (a long-idle key is treated as fresh);
     - else `mGapEma = (mGapEma < 0) ? gap : 0.5·mGapEma + 0.5·gap`.
  3. `mCount = min(mCount+1, 0xFFFF)`, `mLast = now`, union the bounds/reasons/class, `mPending = true`.
  4. `Q = clamp((mGapEma < 0) ? 0 : 2·mGapEma, kQmin(dt), kQmax)`, `mSettleAt = now + Q`.
- **At drain:**
  - Every pending state with `now ≥ mSettleAt` emits **one settle event** (the union bounds, reasons |
    `R_SETTLE`). Then `mPending = false` and its bounds reset. The gap estimate is kept.
  - States unseen for `kForget` are erased.
  - More than 1024 settles in one frame are merged into one union event.
- **Map full** → new keys fold into one **bulk** state (union bounds, a single timer), never overflow (OP3 P0-6).
- **Properties:**
  - Continuous motion emits nothing until it stops, then exactly one refresh.
  - Alternating keys debounce independently.
  - Periodic sources with a period up to ≈ 15 s (`2·gap ≤ kQmax`) are frozen from their second cycle. Longer periods
    are discrete state changes and refresh once per change, below the UNSETTLED threshold.
  - A one-off move refreshes about 0.5 s after it ends.
  - **Discrete notes never enter the debounce.** Texture, content, membership outside motion, light STRUCT and
    add/remove are never delayed by a moving key (OP3 P0-5).
  - Settle events are ordinary events, so the serial ordering keeps them if they land mid-capture.
- Light deletion while pending: the discrete removal event includes the pending bounds, and the state is erased.

### 4.11 Refresh-all capture barrier
- **`Barrier`** (pure model in alprobeschedule.h, state-machine tested by S17):
  `{ bool mActive; U64 mArmSerial; F64 mArmTime, mPausedTime; std::vector<Member> mMembers;
  enum LiveState { NONE, UNAVAILABLE, PENDING, IRR_DONE, DONE, REMOVED } mLive; U64 mLiveIrrStartSerial;
  Terminal mTerminal; }`, where `Member { U32 mId; enum { PENDING, DONE, DROPPED } mState; }`.
- **Request** (the Lightbox button, ON only): `requestRefreshAllProbes()` sets a flag, and also calls
  `requestCinematicLiveProbeRefresh()`.
- **Arming** (flush step a):
  - Members = every probe that is **allocated (`mCubeIndex != −1`), `isRelevant()` (llreflectionmap.cpp:292-312), and
    ORDINARY or REALTIME**. The default probe is included. Occluded and dynamic probes are included.
  - Each member gets R_RESYNC plus R_BARRIER.
  - Live: `PENDING` if it is designated, has a cube and is relevant; `UNAVAILABLE` if it is designated without those;
    `NONE` if it is not designated.
  - `++mLiveSceneSerial`.
- **Progress** (every flush), per member found by `mId`:
  - deleted, lost its slot, or became irrelevant → `DROPPED`;
  - **ORDINARY:** DONE only via an **acked** transaction with `mTxnSerial ≥ mArmSerial` (§4.6 `doProbeUpdate`);
  - **REALTIME:** DONE when the slice cursor completes an irradiance pass then a radiance pass whose face 0 ran after
    arming;
  - an ownership change does not change membership.
  - Members are always eligible (BARRIER) regardless of occlusion or interval.
- **Live:**
  - A pass counts only if its **first face ran after arming**. FULL passes are single-frame, so they count if run after
    the arm frame. For budget passes, record `mLiveIrrStartSerial` at the call site when `mCineRefresh.mActive &&
    mFace == 0` before :573. The pass end is detected from the `mIrrPub`/`mRadPub` delta.
  - An irradiance pass that started post-arm → IRR_DONE; then a radiance pass that started post-arm → DONE.
  - Live undesignated mid-barrier → REMOVED.
- **Terminal states:**
  - **DONE:** no member PENDING and Live ∈ {NONE, UNAVAILABLE, DONE, REMOVED}. Readout `All probes refreshed (n)`, or
    `All remaining probes refreshed (k of n, m dropped)`, plus `Live: …` when not NONE. Log `[ProbeSched] REFRESH-ALL
    DONE`.
  - **Paused:** while `mPaused` (EEP transitions, llenvironment.cpp:2943, :3546), the readout shows `paused` and the
    timeout clock stops.
  - **Timeout:** 30 s of unpaused time → `Incomplete: k/n refreshed`. Never "All".
  - **Cancelled:** on-demand turned off, probes disabled, or an early return while active → `cancelled (disabled)`.
    `resetProbeSchedule` → `cancelled (reset)`.
  - Pressing again re-arms.
- The readout `probe_refresh_status` is updated in `ALFloaterLightBox::draw()` (alfloaterlightbox.cpp:240) at ≤ 4 Hz,
  and holds a terminal text for 10 s.
- Changes after arming are ordinary dirt and never extend the barrier. The guarantee is: **every member probe completed a
  capture that started after arming, or is reported as dropped/incomplete.**

### 4.12 Flexi / media: no camera-independent stop signal (UD2)
**PROVES that neither has a camera-independent signal:**
- **Flexi.** `LLVolumeImplFlexible::doIdleUpdate` simulates **only when visible**, with a period derived from the
  camera's pixel area (llflexibleobject.cpp:344-411). Its shape is a camera-gated client simulation that never "stops"
  under wind.
- **Media.** Frames are uploaded only when `!mSuspendUpdates && mVisible` (llviewermedia.cpp:3090, :3102). So the media
  texture a probe samples is itself frozen whenever the main camera does not see it. Plugin status transitions exist,
  but observing them is gated by the same visibility/priority.

**The backstop, designed and default OFF** — build it, with the user choosing whether to enable it:
- Setting `RenderProbeAnimCadence` (F32, 0 = off, else 5-120 s).
- A registry of "animated content" keys: a flexi object is registered on first sight at the flexi branch of H4
  (llvovolume.cpp:2169-2177); a media face is registered from `LLVOVolume` media entries when H4/H3 fire for it.
  - Each entry holds the key, id tag and world bounds.
  - Entries are removed by H2 removal (discrete), and refreshed by motion.
- Every `cadence` seconds, ORDINARY probes whose footprint contains an entry get `R_CADENCE`. Cadence starts are
  excluded from UNSETTLED.
- Registration happens on first sighting, which is camera-gated once. Afterwards the cadence is camera-independent.
- **UD2 options for the user:**
  - (a) frozen + MaxAge + Refresh-all (the default);
  - (b) enable the cadence, at a cost of about 12 faces per affected probe per cadence period.

---

## 5. B-remainder

### 5.1 Closest-dynamic slicing
- **Dispatch,** non-Live branch (:583-594), ON:
  - if `realtime_probe && realtime_probe != cinematicLive && realtime_probe->mComplete` → `updateRealtimeSliced(probe,
    N)`, with N = clamp(`RenderProbeRealtimeFacesPerFrame`, 1, 6). **N=6 is included.**
  - If the probe is incomplete: no realtime capture. It stays ORDINARY and gets WARMUP. `mRtSlice.reset()`.
  - OFF: the unchanged `updateRealtimeProbeAllFaces`.
- **`SliceCursor { U32 mId; S32 mCube; bool mActive; U8 mFace; bool mRadiance; bool mLastPassIrr; U64 mPassStartSerial; }`:**
  - an identity change resets it, and the next pass starts at face 0;
  - the pass kind is `mLastPassIrr ? radiance : irradiance` (as alcineliveproberefresh.h:483/:539/:556; that header is
    not edited).
- **`updateRealtimeSliced`** (zone `"rmmu - rt sliced"`, ZONE_NUM faces):
  - `probe == mUpdatingProbe` → skip the frame and keep the cursor (`rt_blocked`).
  - Freeze the origin at face 0, restore it after.
  - Per face: set `mRadiancePass = cursor.mRadiance`, then `updateProbeFace(probe, face)`. This applies the light
    scope. Update `mLastFaceSerial`.
  - Restore `mRadiancePass`. `updateNeighbors` at a pass end.
  - Never write `mComplete`, readiness or `mRealtimeRadiancePass`.
- **Invariant:** secondary-scratch passes start at face 0. Live and sliced passes are mutually exclusive per frame.

### 5.2 Live Probe ON-path (CX3 P0-1)
- **Dispatch** at :524-528:
  ```cpp
  const U64 cine_h = on_change ? (mOnDemandActive ? sampleCinematicHOnDemand(realtime_probe)
                                                  : sampleCinematicH(realtime_probe)) : 0;
  const bool cine_animating = on_change && !mOnDemandActive &&
      (ALCineLightRigManager::instance().liveProbeAnimating() || gPipeline.isCinematicProbeLightAnimating());
  ```
  OFF is identical to HEAD, apart from the extraction proven by S13.
- **`sampleCinematicHOnDemand(probe)`** uses `mCineSigOn` / `mCineStickyOn`. The layout is fixed and the zone is
  `"cine probe H on"`.
  1. The probe section, verbatim as :761-765 (the origin stays raw: Live follows its subject, D6).
  2. **Light tokens,** taken from `mLightSnap` entries that pass the Live selection:
     - eligible;
     - `id ∉ mCinematicIgnoredLightIds`, read directly, because the predicate is capture-gated;
     - `pinned || spot || calc-dist(mPubPos, origin, max_dist) < max_dist`, with the same `max_dist` rule as
       pipeline.cpp:9611-9614. Use **published** positions.
     - Sorted by id. Per light: `addExact(id)`, `addExact(STRUCT hash)`, `addExact(published XFORM hash)`,
       `addExact(published PHOTO hash)`.
     - Then `addExact(count)`, plus the light toggle/scale fields of pipeline.cpp:9578-9585.
     - Rig emitters are ordinary entries here (their scale, the projector geometry, sits in STRUCT). Rig slot
       enable/disable reaches the tokens through eligibility and colour.
  3. Env: gather + builder `(0.1f, false)`.
  4. `addExact(mLiveSceneSerial)`.
- **Effect:**
  - Flicker, moving facelights, animated gobos (GOBO_TIME, never in STRUCT) and rig FX/transitions no longer force FULL
    or change H while they run. They settle once, then the Live Probe refreshes one pair.
  - The subject walking still cycles Live, which is its contract.
  - Every-frame, Balanced and Economy modes ignore H, so they are unchanged.
- **`mLiveSceneSerial`** counts, within the Live footprint (sphere(Live O, Rcap)):
  - discrete `C_STATIC|C_TERRAIN_WATER` events and settle events;
  - `C_LIGHT` events with `R_TEX` (gobo arrival), subject to `mMinFaceSerial ≤ mLiveLastFaceSerial`;
  - `C_GLOBAL`;
  - overflow, the ON edge and Refresh-all.

  Pure light events (R_LIGHT) are excluded, because the light tokens carry them.
- **Why Live idles under camera motion:**
  - the LOD/mesh/terrain/tree/grass/tex-anim toggles are tagged;
  - refinements beyond `Dp` do not notify, and re-sharpens hit only if Live rendered after the blur;
  - lights are probe-centric and debounced;
  - motion is debounced;
  - `isTooSlow` is excluded under ON.

---

## 6. Settings, UI, presets, debug, log, Tracy

### 6.1 Settings (`app_settings/settings.xml`, inserted after `RenderDefaultProbeUpdatePeriod` :20703-20713)
| Name | Type | Default | Clamp in code | Persist |
|---|---|---|---|---|
| RenderProbeOnDemand | Boolean | 0 | — | 1 |
| RenderProbeMinInterval | F32 | 1.0 | 0..10 s | 1 |
| RenderProbeMaxAge | F32 | 60 | 10..600 s (0 not allowed; "due" time, not a deadline) | 1 |
| RenderProbeDirtySunDeg | F32 | 0.5 | 0.1..5 | 1 |
| RenderProbeRealtimeFacesPerFrame | S32 | 2 | 1..6 | 1 |
| RenderProbeTinyCullPixels | F32 | 0 | 0..8 | 1 |
| RenderProbeAnimCadence | F32 | 0 | 0 = off, else 5..120 s (UD2) | 1 |
| RenderProbeSchedLog | Boolean | 0 | — | 0 |
| RenderProbeSharedCull | Boolean | 0 | only if C1 is approved | 1 |
- 8 keys, or 9 with C1. Count them.
- Every key has a Comment giving its meaning and range.
- MinInterval 1.0 caps a perpetually dirty probe at about 20 % of frames at 60 fps (INFERENCE).

### 6.2 UI
**Lightbox → Rendering** (floater_lightbox_settings.xml):
- Insert **immediately after :1537** (the `</button>` of `reset_RenderHeroProbeUpdateRate`) and **before :1538**
  (`<!--Probe Draw Distance-->`). The comment spans :1539-1576.
- Use the idioms of :1481-1537: text + slider + spinner + a reset button (`LightBox.ResetControlDefault`).
- Controls:
  - check "Update probes on demand" (`RenderProbeOnDemand`);
  - sliders:
    - "Safety refresh (s)", 10-600;
    - "Min interval (s)", 0-10;
    - "Sun change (deg)", 0.1-5;
    - "Realtime faces/frame", 1-6;
    - "Animated content refresh (s)", 0-120 (UD2; shown even if the user keeps it at 0).

    All carry `enabled_control="RenderProbeOnDemand"`.
  - combo "Probe tiny cull": Off 0 / Light 1 / Strong 3;
  - check "Log probe schedule";
  - **button "Refresh all probes now"**: callback `LightBox.RefreshAllProbes`, registered next to
    alfloaterlightbox.cpp:67-77, enabled with on-demand;
  - text `probe_refresh_status`.
- Raise `render_settings_scroll_content` (:1320, height 566) by the rows added.
- Tooltips:
  - on-demand: the §4.9 list in one sentence;
  - MaxAge: "a probe becomes due after this long; refresh follows the queue";
  - realtime: "Realtime detail without a Live Probe only";
  - tiny cull: "skips whole groups of small opaque non-PBR surfaces";
  - cadence: "periodically refreshes probes near flexi/media (camera-independent after first sighting)".

**Graphics → Advanced** (floater_preferences_graphics_advanced.xml):
- One check, "Update reflection probes on demand", after the `ProbeCount` combo (:954-983), with `left="420"
  top_delta="22"` and `Pref.RenderOptionUpdate` as at :860.
- Verify by summing the `top_delta`s that the Tonemap slider (:1178-1196) still ends above `vram_status_border` (top
  480). If it does not, omit the row and report it.

The rig-panel card is unchanged.

### 6.3 Presets / reset
- llpresetsmanager.cpp `getGraphicsControlNames`: insert, before `"RenderReflectionProbeDetail"` (:342) and each with a
  trailing comma, `RenderProbeOnDemand`, `RenderProbeMinInterval`, `RenderProbeMaxAge`, `RenderProbeDirtySunDeg`,
  `RenderProbeRealtimeFacesPerFrame`, `RenderProbeTinyCullPixels`, `RenderProbeAnimCadence`.
- Do not touch :343.
- Per-control resets live in the Lightbox.

### 6.4 `[ProbeSched]` log (`RenderProbeSchedLog`; one line per 5 s of manager frames; on and off paths)
```
[ProbeSched] mode=on|off probes=<n> faces=<ord/s> rt=<rt/s> starts done nack
 g t l e p s c r w d z b a=<txn starts by reason>  events raw nl motion settles bulk deb=<pending>
 drop_lodvol drop_lodmesh drop_lodterr drop_lodtree drop_lodgrass drop_texanim drop_dyn noop_move rebal overflow
 dirty deferred wait maxlag rt_blocked env_changes flush_us=<avg/max> lights=<processed>/<total> barrier=<state k/n>
 verdict=<OK|UNSETTLED id=<k> reasons=<letters>|STARVED n worst|OVER BUDGET ...|OFF-BASELINE|PAUSED>
```
- ORDINARY only.
- **UNSETTLED:** one probe started in 3 consecutive windows with reasons outside S/W/R/B/A. This is a FAIL unless the
  tester was editing.
- **STARVED:** expected after a global event when `12·P/fps > 5 s` (P ≳ 25 at 60 fps).
- **OVER BUDGET:** a frame with ord > 1, rt > 6, or sliced rt > N. It is an implementation-bug detector and is
  structurally unreachable.
- **PAUSED:** the window was more than 50 % early-return or paused.
- **Priority:** OVER BUDGET > PAUSED > UNSETTLED > STARVED > OK.
- The window restarts after a gap > 0.5 s (as :974-984).

### 6.5 Tracy
- Zones:
  - `"probe sched flush"`, `"probe sched env"`, `"probe sched lights"`, `"probe sched debounce"`;
  - `"probe dirty tex fanout"`, `"cine probe H on"`, `"rmmu - rt sliced"`, `"probe tiny cull"`;
  - the C0 zones.
- No zones inside the hooks.
- The existing zones (`"reflection manager update"`, `"probe update"`, `"rmmu - realtime"`, `"rmmu - cine step"`,
  `"rmmu - cine budget"`, `"cubeSnapshot"`) stay the timing truth.

---

## 7. C — shared cull

### 7.1 C0 (built)
In `display_cube_face` (llviewerdisplay.cpp), add scoped zones only:
- `"cube face cull"` around `gPipeline.updateCull(...)`;
- `"cube face shadow"` around `generateSunShadow`;
- `"cube face sort"` around the `stateSort` block;
- `"cube face geom"` around `renderGeomDeferred`;
- `"cube face light"` around `renderDeferredLighting`.

No statements move.

### 7.2 C1 (`RenderProbeSharedCull`) — UD1
- **Pixel-identity argument.**
  - `LLOctreeCull`'s node test is `min(frustum, sphere(origin, mFrustumCornerDist))` (llspatialpartition.cpp:1062-1070).
  - The traversal is hierarchical: llvieweroctree.cpp:1345-1370, `SKIP_FRUSTUM_CHECK` at :1354-1355,
    `checkObjects`/`visit` at :1468-1507.
  - Group extents nest. So a DFS-ordered cache of union-sphere-intersecting nodes, re-filtered per face with identical
    tests, yields identical `sCull` contents and order, provided that:
    - the SKIP parent-result rule is kept;
    - `markNotCulled` stays per face (pipeline.cpp:4047-4080);
    - bridges keep their own `setVisible` (lldrawable.cpp:1537+);
    - the VO-cache and sky pushes are unchanged (pipeline.cpp:4019-4044).
- **Saving:** only the traversal of union-rejected subtrees and retained sphere classifications, bounded by C0's `"cube
  face cull"` share.
- **Remaining per face:** the face tests, pushes, bridge and VO-cache culls, stateSort, both shadow cascades
  (pipeline.cpp:25975-25990, :26710-26714) and rendering.
- **Profitability is unproven.**
- **If approved:**
  - a probe-owned context, never `sCull` or the static result;
  - lifetime-validated handles, with no `LLDrawInfo` retention (:5080-5135);
  - a full key and epoch;
  - absent for Hero and Prism;
  - the union-shadow gate (:25988-25990) untouched.

## 8. D — conservative tiny-group cull (default OFF; byte-identical at 0)
- **Capture context:** `enum class ProbeCaptureKind : U8 { NONE, ORDINARY, LIVE, REALTIME }`, static
  `LLReflectionMapManager::sCaptureKind` plus a public static getter.
  - An RAII scope in `updateProbeFace` around the two `probe->update` calls sets LIVE if `mCinematicLiveProbeCapture`,
    REALTIME if `probe != mUpdatingProbe`, else ORDINARY.
  - It restores NONE on exit, including the early return.
  - Hero and Prism never enter `updateProbeFace`.
- **Filter** in pipeline.cpp `postSort`, visible-group loop, after :5111-5116:
  `if (px > 0 && kind != NONE && !sShadowRender && !sPrismLensRender && rejectable && tiny) continue;`
  - **rejectable:**
    - `PARTITION_VOLUME`, `asBridge() == nullptr`, not dead;
    - **every** `mDrawMap` key is in {PASS_SIMPLE, PASS_GRASS, PASS_SHINY, PASS_BUMP, PASS_POST_BUMP, PASS_MATERIAL,
      PASS_MATERIAL_ALPHA_MASK, PASS_SPECMAP, PASS_SPECMAP_MASK, PASS_NORMMAP, PASS_NORMMAP_MASK, PASS_NORMSPEC,
      PASS_NORMSPEC_MASK, PASS_ALPHA_MASK} (lldrawpool.h:149-213);
    - any `PASS_GLTF_PBR*` key → **kept**, because emissive PBR registers there (llvovolume.cpp:7466ff);
    - fullbright, emissive, glow, blend, alpha, invisible and rigged passes → kept.
  - **tiny:**
    - `r = |mObjectBounds[1]|`, `d = |centre − camera origin|`;
    - keep if `d − r ≤ 2·near`;
    - `p = 3·r·res/(d − r)`, with `res = mProbeResolution` (the final resolution, :295 vs :1358) and 3 = the cube-corner
      magnification bound. Reject iff `p < px`.
- Lights and shadow casters are unaffected. `px == 0` means the branch never runs.

---

## 9. Review package

### 9.1 Off-path inertness
| Off | Prove |
|---|---|
| RenderProbeOnDemand=0 | Hooks cost one branch on `gProbeDirtyRecording`, and no new member is written. `isOnDemandActive()` and `isProbeCentricLightCapture()` are false, so `calcNearbyLights`, `[LiveProbeFaceLights]` and `isTooSlow` behave as HEAD. No light scope. The flush drains and discards. Selection, the default rule, dispatch, `doProbeUpdate` effects, `mRadiancePass` and debug text are identical. Live runs `sampleCinematicH` plus the real `cine_animating`, and H is identical (S13). |
| RealtimeFacesPerFrame | Irrelevant while OFF. |
| TinyCullPixels=0 | The branch is never taken. |
| SharedCull=0 | Today's path. |
| AnimCadence=0 | No registry hits. |

### 9.2 Isolation proofs required
- (a) No hook mutates state.
- (b) `sCaptureKind` is NONE outside its scope.
- (c) The secondary scratch never interleaves between Live and slicing; passes start at face 0; the blocked case holds.
- (d) `mRadiancePass` is restored, and set false only at ON txn start.
- (e) `mLastUpdateTime` semantics are unchanged.
- (f) No GL state change (CLAUDE.md rule 1).
- (g) The env extraction is a line-diffed move with the two documented edits.
- (h) The §4.7 contract never acks a partial transaction.
- (i) The main view, Prism and Hero never see the probe-centric list. It is restored after every face, and never nests
  with Live (`mCinematicLiveProbeCapture` check plus the guard at :10323-10329).
- (j) `clearTxn` is called only at the three documented sites.
- (k) Barrier members change state only on ack, drop or terminal.

### 9.3 Least-sure list
1. Camera-driven rebuild paths not yet tagged: particles, impostors, avatar-attached statics, water.
2. The debounce constants versus real sources (a 0.3 s timer, 20 s slideshows).
3. Per-face list correctness and the per-face cost with thousands of lights (T52).
4. The cap rule's local counts, including spots anywhere.
5. `stickyDigest` equals `StickyHash::update` bit-for-bit (S16).
6. The Live ON H: published-value selection and the removal of the `liveProbeAnimating` FULL path.
7. Barrier Live pass-start detection at the :573 call site.
8. The MS asset-readiness predicate for sculpts.
9. The DS stamp semantics with `mMinFaceSerial`.
10. **Is the approach wrong?**

### 9.4 Compile rules (P2)
- Qualify `LLDrawable::` state names.
- The pipeline→manager methods listed in §4.6 are public.
- Use `static_cast` on every narrowing (U16 counts, S8 discard).
- No unused locals (MSVC /WX, C4189), in particular after removing :756.
- The Slot enum cast (§4.3).
- `getNumFaces` / `getNumVolumes` loops use S32.

---

## 10. Test plan

### 10.1 In-world (user runs; log on; Tracy 10 s per step; outcomes stated in advance)
- **Latency model:** with k eligible probes ahead, ≈ 12(k+1) frames; plus MinInterval if the probe started less than
  1 s ago; plus the debounce quiet time for motion sources; occluded probes wait unless under the barrier.
- **Verdict rules:** a contradicting counter is a FAIL; PAUSED is INCONCLUSIVE; UNSETTLED is a FAIL unless the tester
  was editing.
- **AutoTune off** for T7, T23, T25 and T44.
- **The default flip needs T0-T53 all PASS.**

**Baseline and core behaviour**
- **T0 Off identity.** Off vs the backup `AlchemyTest.pre-probeondemand.exe`: a lossless snapshot diff within the A1/A2
  noise floor. Procedure: static scene, fixed EEP, cloud scroll 0, a frozen camera; two backup snapshots set the noise
  floor; PASS if the diff of the new build ≤ that floor. Log shows `OFF-BASELINE`, faces ≈ fps.
- **T1 Static skybox with P probes** (updated).
  - Off: `"probe update"` every frame.
  - On: after warm-up (≈ 12P frames), faces/s ≈ 12P/MaxAge (≈ 12P/60 at the default), only `s`, verdict OK. The zone
    shows as isolated frames.
  - The converged image differs from Off only by local lighting (U1 probe-centric): accepted.
- **T2 Rez, move, rotate, retexture a prim.** Probes within Rcap ≈ 131 m (or reached by a capsule) are hit.
  - A move/rotate refreshes about 0.5 s after the motion ends (`z`).
  - A retexture refreshes immediately (`t`).
  - Probes further away stay clean.
- **T3 Drag the sun slider.** `e > 0`, ord ≤ 1; STARVED allowed.
- **T4 Running 4 h day cycle.** `e` ≈ every 20 s. STARVED is expected with P ≳ 25.
- **T5 Light edits.**
  - Recolour, move or rotate a projector, toggle its gobo or no-shadow → `nl` plus `l` or `z`.
  - STRUCT edits are immediate. XFORM/PHOTO edits refresh after they settle.
- **T6 Avatar with facelights walks through a static probe.** `drop_dyn` rises; the light shows under `motion`; one
  `z` after it stops.
- **T7 STRICT** (AutoTune off, loaded static set). Orbit and fly twice. On the second pass:
  `g = t = l = e = nl = z = 0`; non-default `p = 0`; default `p` only per 16 m; faces/s = the safety rate; verdict OK.
- **T8 Live On change.** Move a prim near the subject → `[LiveProbe] last=changed`, a clean pair, idle.
  - **T8b:** a camera orbit → `converged=y`, idle ≥ 95 %.
- **T9 Realtime detail, no Live.**
  - Off: 6 faces/frame under `"rmmu - realtime"`.
  - On, N=2: `"rmmu - rt sliced"`, `rt_blocked ≈ 0`.
  - On, N=6: also sliced (REALTIME).
- **T10 Occlusion.** Edit behind the camera → `deferred > 0`; turn around → refresh.
- **T11 Default probe, coverage None/Sky, standing still.** No `p`. Paused clouds → only `s`. Moving clouds → `c` at
  most once per period. Walking 20 m → one `p`.
- **T12 D.** Strong vs Off: ZONE_NUM > 0 and less `"probe update"` time. The main view is identical; PBR and emissive
  objects are still reflected.
- **T13 C0.** Record the five zones.
- **T14 Region crossing and teleport.** A crossing gives `nl = 0`, `r = 0` (REALTIME flips excluded) and no light
  events (S16). A teleport resets cleanly.
- **T15 Delete a manual probe while it is refreshing.** No `nack` storm; the next probe completes both passes.
- **T16 Allocation.** Change the probe count or coverage, rez probes → WARMUP → OK.
- **T17 Toggle on-demand mid-refresh**, both ways, 10×. `r` on each ON edge, no stuck dirty, no OVER BUDGET, and a
  barrier in progress is `cancelled`.
- **T18 MaxAge.** It cannot be set below 10 s. At 10 s, `s` ≈ P per 10 s.
- **T19 Refresh-all.** The readout counts down to `All probes refreshed (n)`. Occluded probes are included, and so is
  the Live pair started after arming.
- **T20 EEP swap and water-only change.** `e` or a resync; it converges.
- **T21 Texture animation, flexi and media on screen** (updated).
  - Texture animation: `motion` counts rise, no `t`/`g` churn, and one `z` when the animation stops.
  - Flexi and media: no events (frozen), refreshed only at MaxAge / Refresh-all / cadence (if enabled).
- **T22 Slicing with a Live switch.** No garbage cube; the sliced path resumes at face 0.

**Motion, lights and textures**
- **T23 Spinner and Prop Mover prop.** `motion` rises and no probe starts while they move; Live idles.
  - **T23b:** an orbit around the spinner → no `z`.
  - Stop the spinner → one `z`.
- **T24 A rig with flicker, and a facelight crowd.** No starts while animating; Live idles (no FULL). Stop → one
  refresh.
- **T25 Mesh-heavy dolly.** `drop_lodmesh > 0`, `g ≈ 0` once meshes have loaded.
- **T26 VRAM pressure: turn away, then back.** A re-sharpen `t` hits only probes that rendered a face after the blur.
- **T27 Projector gobo first load** (cache cleared). A light-texture event; probes and Live re-render.
- **T28 More than 256 lights plus an orbit.** No light dirt.
- **T29 Change `RenderLocalLightCount`.** One `e` for all probes.
- **T30 Late gobo upload mid-capture.** A follow-up refresh.
- **T31 Distant terrain edit (> 131 m).** Hits every probe.
- **T32 Load storm beyond both caps.** `overflow > 0`, `nack = 0` caused by overflow, every probe completes.
- **T33 Incomplete realtime probe.** WARMUP through ordinary, then sliced.
- **T34 Edit a prim while its LOD changes.** `t`/`g` recorded.
- **T35 Teleport, EEP reset and probe-count change mid-capture.** `nack` plus a redo; debug letter `R`.
- **T36 Live OFF-hash.** S13 PASS plus the reviewed line-diff. OFF `[LiveProbe]` behaviour matches T8.

**Round-3 additions**
- **T37** A bright point light next to a probe (ON) → the light appears in the probe on **every** face, including
  Economy/1-face budgets (OP3 P0-1).
- **T38** Live Economy near a rig vs Every frame → identical local lighting in the probe (validates the separate
  `[LiveProbeFaceLights]` fix).
- **T39** Swap the mesh asset of a built object → the new mesh appears in the probes (discrete event).
- **T40** A 1 s texture slideshow and a 0.5 s `llSetRot` timer → frozen from the second cycle, no UNSETTLED. Stop →
  one `z`.
- **T41** A spinner at ≤ 3 fps capture (high-res offline) → still frozen; `z` only after it stops.
- **T42** Recolour a spinning prim → an immediate `t`. Change the radius of a flickering light → an immediate `l`
  (STRUCT). Move a flickering light → it settles on the XFORM class.
- **T43** Refresh-all at coverage 1/2 (irrelevant probes are not members); with a Live probe without a cube (`Live:
  unavailable`); during an EEP transition (`paused`). No hang, no false "All".
- **T44** T7-strict under VRAM pressure → `t = 0` unless a probe rendered during the blur.
- **T45** 5000+ texture-anim and omega keys in view distance → no overflow storm; `bulk` may be > 0; probes settle.
- **T46** More than 256 lights sim-wide with a facelight crowd stopping and starting → only the probes near the crowd
  refresh.

**Codex counterexamples and cost**
- **T47** Cap+1 → cap by removing a distant spot → the over-cap probes are hit.
- **T48** Toggle AutoTune "too slow" on an avatar with a light (ON) → no light event.
- **T49** An alternating pair of blinking prims (each at 2 Hz, out of phase) → both frozen.
- **T50** Delete a light while its motion is pending → one discrete event covering its old path.
- **T51** A Refresh-all while a Live budget pass is mid-flight → Live is DONE only after a post-arm irr + rad pair.
- **T52 CPU benchmark.** 2000+ lights and 500 probes-in-range scenarios. Report `flush_us` avg/max and the per-face
  `"probe update"` delta ON vs OFF.
  - Target (INFERENCE): flush avg ≤ 0.5 ms, max ≤ 2 ms. Exceeding it is a FAIL to report.
- **T53** Re-arm Refresh-all during an active barrier; probes deleted mid-barrier → `m dropped`, never a hang.

### 10.2 TUT (`tests/alcinelightrigmodel_test.cpp`; count the `template<> template<>` additions: 17)
- **S1** An event mid-txn stays dirty after completion.
- **S2** A same-frame pre-start event is acked.
- **S3** MinInterval keeps pending.
- **S4** Safety fires at MaxAge (≥ 10). A policy with 0 is clamped by the caller to 10, so there is no permanent
  disable (updated).
- **S5** DEFERRED keeps dirt; BARRIER overrides occlusion and interval.
- **S6** Footprint: inside/outside the sphere, terrain global, capsule only on a sphere miss, class masks,
  `mMinFaceSerial` skip.
- **S7** Transaction contract: the refusals (no txn / no irradiance / epoch / cube); newer serials are never cleared;
  overflow never clears a transaction.
- **S8** Verdict priority, including UNSETTLED.
- **S9** SliceCursor: N faces, alternation, identity reset at face 0, blocked frame.
- **S10** Grace-init and the ON-edge `clearTxn`.
- **S11** Ownership excludes LIVE/REALTIME records.
- **S12** H6 rule: first image; coarse refinement; nothing beyond `Dp`; no notify on downgrade; the first blur stamp is
  kept; a re-sharpen carries `mMinFaceSerial`; the `d < 0` guard; integer `Dp`.
- **S13** Env builder:
  - (i) the layout (kind, a, b, count per field; sky 50 floats, water 19, settings) equals a table;
  - (ii) `(0.1f, false)` on a fixed synthetic `EnvSample` equals a **golden H literal**. The **reviewer** (Codex)
    produces the literal independently: extract HEAD :786-892 with `git show c31f37cc43a:indra/newview/
    llreflectionmapmanager.cpp`, substitute getter → sample value by script, and run it against
    alcineliveproberefresh.h;
  - (iii) plus a mandatory review of `git diff --color-moved=dimmed-zebra` showing the getters moved unchanged. This
    covers the gatherer wiring.
- **S14** Debounce:
  - continuous at 60 fps → no event until 0.5 s quiet, then one settle with the union bounds;
  - a 2 s periodic source → frozen from the second cycle;
  - a gap > 60 s resets the estimate;
  - 3 fps → the quiet stretches;
  - alternating keys are independent;
  - the count saturates;
  - an id-tag mismatch resets;
  - bulk beyond the cap;
  - discrete notes bypass;
  - a shift offsets the bounds.
- **S15** Cap rule: previous/current local counts; cap+1 → cap hits.
- **S16** `stickyDigest` equals `StickyHash::update` for an unchanged signature. After shifting the accumulators and
  the sample by the same offset, the digest equals the next `update()` result (no event).
- **S17** The Barrier state machine: membership by id; ORDINARY done only via a post-arm ack; REALTIME via a post-arm
  pair; Live pre-arm passes are excluded; UNAVAILABLE / REMOVED; paused freezes the timeout; timeout → incomplete;
  cancel; re-arm.

---

## 11. Implementation order (single delivery; one build after the review converges)
- [ ] 0. Confirm `[LiveProbeFaceLights]` is committed on the base. If not, stop and report.
- [ ] 1. `alprobeschedule.h` + CMake.
- [ ] 2. TUT S1-S17 (count 17). The S13 golden literal comes from the reviewer before build.
- [ ] 3. `alprobedirty.{h,cpp}` (extern flag, debounce, bulk) + CMake.
- [ ] 4. settings.xml: 8 keys.
- [ ] 5. **Hooks:** H1a, H1b (+ ScopedMove), H2, H3, H4, MS (+ llvovolume.h), LV, LT, LTr, LG, TXt, TXa, H6 + DS
  (+ llviewertexture.h), H7, SH.
- [ ] 6. **pipeline:** the `mLights` accessor, `probeShouldRenderLight`, the `calcNearbyLights` edits (§4.5.2, §4.5.4),
  the H3 lines, the D filter.
- [ ] 7. **llviewerdisplay.cpp:** extend the `[LiveProbeFaceLights]` gate; C0 zones.
- [ ] 8. **Manager:**
  - gatherer + extraction;
  - records, ids, epoch;
  - the flush (ON edge, overflow, barrier, drain + settles, light diff + debounce, cap counts, cadence, E×P after
    filtering, global-first hits, R_PROBE, default, env);
  - selection with the barrier; the default rule; txn start/irradiance/complete; face serials;
  - the light scope; sliced realtime;
  - the Live ON H and dispatch;
  - barrier progress; debug text; reset sites; log.
- [ ] 9. **UI:** the Lightbox block, button, status and `draw()` update; the Graphics row; presets.
- [ ] 10. Adversarial review (Opus + Codex) with §9 verbatim, looping to 0 must-fix.
- [ ] 11. Checkpoint commit + `AlchemyTest.pre-probeondemand.exe`. ONE build. The user runs §10.

## 12. OFF-LIMITS, allowed files, reports
**OFF-LIMITS:**
- shaders;
- `llheroprobemanager.*`, `llprismlens.*`;
- `alcineliveproberefresh.h` (included and reused, never edited);
- the `updateRealtimeProbeAllFaces` and `updateCinematicBudget` bodies, and `decide`;
- `alcinelightrig*`, `alcinerigrim*` (read-only calls only);
- `alpanelcinelightrig.*`, `panel_cine_light_rig.xml`;
- LiveProbeConfig and scene LLSD;
- the `updateProbeFace` body except the two RAII scopes;
- `llviewerwindow.cpp`;
- `llenvironment.*` (getters only);
- `llviewermedia.cpp`, `llflexibleobject.cpp`;
- in pipeline.cpp, everything except: H3, the D filter, the `calcNearbyLights` edits (:9726-9747 and the `isTooSlow`
  term at :9778-9780), and the new public accessor/wrapper. In particular the union-shadow gate :25988-25990, :5162-5172,
  the deferred loop :23214-23345 and `calcPixelArea` stay untouched;
- `calcLOD`;
- llpresetsmanager.cpp:343;
- `enve`, `vcpkg`.

**Allowed:**
- new: `alprobeschedule.h`, `alprobedirty.{h,cpp}`;
- `CMakeLists.txt`;
- `llreflectionmap.h`, `llreflectionmapmanager.{h,cpp}`;
- `lldrawable.cpp`, `llspatialpartition.cpp`;
- `pipeline.{h,cpp}` (as above);
- `llvovolume.{h,cpp}`;
- `llviewertexture.{h,cpp}`, `llviewertexturelist.cpp` (DS only);
- `llviewerobject.cpp` (H7 only);
- `llsurfacepatch.cpp`, `llvotree.cpp`, `llvograss.cpp` (tags only);
- `llviewerdisplay.cpp` (the gate extension and C0);
- `alfloaterlightbox.{h,cpp}`;
- `app_settings/settings.xml`, `floater_lightbox_settings.xml`, `floater_preferences_graphics_advanced.xml`;
- `llpresetsmanager.cpp`;
- `tests/alcinelightrigmodel_test.cpp`.

**Reported, not fixed (upstream/shipped):**
1. llpresetsmanager.cpp:343: a missing comma merges two preset names.
2. `deleteProbe` (:1238-1242) does not reset `mRadiancePass`. The OFF path can mark a probe complete without an
   irradiance pass. The ON path neutralises this.
3. The Live face-0 empty light list: being fixed separately as `[LiveProbeFaceLights]`.
