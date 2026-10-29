# Live Probe Refresh — face budget + converge-to-idle design (R3)

Status: DESIGN R3 (Opus, 2026-09-29; Codex R3 core-verified). Implementer: Sonnet. Review: adversarial (Codex/Opus) to 0 must-fix, THEN one build.
Anchors are at HEAD `c221cda3732` (branch `fix/animesh-clone-pose-polish`). Paths are relative to `indra/newview/`.
User decisions: default **On change**; refresh mode is **per-machine** (not scene, not Setups); **no crossfade** (a pop
is accepted).

## R3 — Codex round 3 (core verified: reset sites, scratch, mComplete/mFadeIn, updateNeighbors, extraction)
| Finding | R3 resolution |
|---|---|
| P1-A: continuous change starved radiance (the radiance pass needed a clean irradiance on the current H). | The pass kind **always alternates** irradiance/radiance (§4.1 step 6). Clean-H bookkeeping only decides IDLE. Test M14 bounds the radiance publication gap at ≤ 6 frames; in-world §8.12. |
| P1-B: a single REL tolerance with a 1e-4 floor hid density-multiplier changes; relative water height allowed ~20 cm; 0.01 direction ≈ 0.5°. | Per-field tolerances in field units (`Tol::ABS/REL/ANGLE/COLOR` with per-field values, §4.1 and §4.4): densities relative with tiny floors (density multiplier `Rel(1%, 1e-7)`), heights in absolute metres (water 0.01 m), directions in degrees (sun 0.1°, emitters 0.25°), colours per channel. Test M15 covers visible changes vs bounded noise. |
| P2: projector geometry missing. | Emitter scale added, `Abs(0.005 m)` (§4.2). |
| P2: water float count. | Corrected to **19** (sky 50 = 33 + 17). |
| P2: hashing cost unprofiled. | `hash_us=avg/max` in the `[LiveProbe]` log; all-rigs profile step §8.11. |
| P2: counters, Tracy and the moved-code diff are structural only. | Image comparison with a same-build noise floor (§8.0 Pixel): PASS/FAIL/INCONCLUSIVE stated in advance. |

## R2 — coordinator simplification after Codex round 2 (supersedes the R1 scheduler)
| Codex R2 finding | R2 resolution |
|---|---|
| P1-1: a generation counter does not prove frame order; snapshot/360 `display()` calls skip the manager (llviewerwindow.cpp:5771, 6004); early returns (:209-220, :333) bypass abort. | **No producer/consumer.** The signature is computed **inside the manager's live-probe step** (§4.3). That step runs after `display()`, so after the environment update (llviewerdisplay.cpp:772) and the rig tick. Extra `display()` calls never advance anything. Every early return and the non-budget branch call `resetCinematicRefresh()` (§4.3 steps 1 and 3). |
| P1-3: tolerant compare ≠ identity; convergence declared unconditionally; A→B→A mid-pass. | **One hash H**, checked on **every frame** a pass spans (including blocked frames). A pass counts only if H never changed. Converged = the last irradiance pass and the last radiance pass were both clean with the same H, and that H equals the current H (§4.1). |
| P1-4: motion state machine / hysteresis / stale `mRefreshRig` / mode changes bypass `decide`. | **Removed.** No motion state, no adjacent-frame deltas, no cached rig sample (the rig is read on demand, §4.2). Animation = the rig's own flags → Every-frame path, plus `SettleSec`. A mode change resets state in `decide` step 1, and every frame outside the budget branch resets it too. |
| P1-5: 57 floats vs 40; missing moisture/droplet/ice; water transitions. | Exact schema mirroring the uniform uploads (§4.4), in dynamically sized vectors (no cap). Water blend factor and next-texture IDs are included, so water-only transitions change H. |
| P2: test overclaims. | Model tests cover only the model (§7). Frame order, scratch isolation and pixel identity are verified in-world via the `[LiveProbe]` counters, Tracy face counts and the moved-code diff (§8). |

Kept from R1:
- verified facts (§2);
- per-pass publication accepted;
- catchlight and no-shadow flags in the signature;
- EEP pause/reset policy;
- 68 px status box;
- GI sampling EVs excluded (alenvintensity.cpp:297).

## 1. Problem and measured cost
The Cine Light Rig Live Probe is the designated realtime probe (`mCinematicLiveProbe`). In
`LLReflectionMapManager::update()` (llreflectionmapmanager.cpp:495-541, zone `"rmmu - realtime"`) it captures **all 6
faces every frame** (:512-515), alternating irradiance/radiance via `mRealtimeRadiancePass` (:508, :537). Each face goes
`updateProbeFace` → `LLReflectionMap::update` (llreflectionmap.cpp:73) → `display_cube_face` (llviewerdisplay.cpp:1256):
`updateCull` + `generateSunShadow` per face (:1312). Tracy on the RTX 5090:
- probe ON: 49 ms;
- probe OFF: 30 ms;
- so ≈19 ms per frame, ≈3.2 ms per face (INFERENCE).

## 2. Verified facts (code PROVES unless labelled)
1. **Scratch.** Faces go to scratch `sourceIdx = mReflectionProbeCount (+1 if probe != mUpdatingProbe)` (:898-903). The
   array has `count+2` cubes (:1545-1566). The hero manager has its own texture (llheroprobemanager.cpp:557). Only the
   realtime path writes the secondary scratch.
2. **Publication.** Only `face == 5` (:1005) convolves scratch into the live slot:
   - radiance → `mTexture[mCubeIndex]` (:1046);
   - irradiance → `mIrradianceMaps[mCubeIndex]` (:1097).

   Publication is **atomic per pass, not per pair (accepted)**: new irradiance can show with old radiance for ≤6/B frames
   while `mComplete/mFadeIn` stay live. The Every-frame path already does this for 1 frame.
3. **Shadows scale with faces.** `generateSunShadow` runs once per captured face in `display_cube_face` (:1312). The main
   view has its own at :841. Idle frames pay nothing; probe-OFF is the shipping precedent for frames without a capture.
4. **Avatars are excluded.** The probe is non-dynamic (alcinelightrigmanager.cpp:545), and `cubeSnapshot` hides
   AVATAR/CONTROL_AV/PARTICLES (llviewerwindow.cpp:6090-6106). Attachments hidden with their avatar = INFERENCE
   (lldrawable.cpp:1835); in-world check §8.9.
5. **Origin.** Reloaded from the probe object every frame (:431-434) and by `autoAdjustOrigin()` (llreflectionmap.cpp:76).
6. **Frame order in `doFrame`.**
   - idle: rig tick (llappviewer.cpp:5560), which applies emitter objects (`applyFrame`, alcinelightrig.cpp:2738);
   - `display()`: `gFrameCount++` (llviewerdisplay.cpp:602), then `LLEnvironment::update` (:772);
   - `mReflectionMapManager.update()` (llappviewer.cpp:1566).

   Snapshot tiles / 360 call `display()` again without the manager (llviewerwindow.cpp:5771, 6004).
7. **Early returns in `update()`:** :209-212 (probes disabled / teleport / pre-PRECACHE), :217-220 (logout), :333-336
   (`mProbes.empty()`).
8. Round-robin never picks the cinematic probe (:466). Readiness flags are cleared at designation change (:743-745), map
   re-init (:1576-1578) and cleanup (:1643-1644).
9. **Fallback.** With no cinematic probe the realtime slot goes to `closestDynamic` (:495-496). Untouched.
10. **EEP sky transition.** It calls `pause()` + `reset()` (llenvironment.cpp:2943-2944). Non-default probes are skipped
    while paused (:422), and `reset()` re-inits maps and clears readiness. Water-only transitions (llenvironment.cpp:3576)
    do not pause: the water blend factor/IDs in H (§4.4) catch them.

## 3. User-facing behaviour — `CineLightRigLiveProbeRefresh` (S32), combo "Refresh" in the Live Probe card
| Value | Label | Behaviour | Extra cost (INFERENCE) |
|---|---|---|---|
| 0 | Every frame | Existing path, unchanged (moved function body). | ≈19 ms |
| 1 | Balanced | Always cycling at 2 faces/frame (3 frames per pass, alternating irradiance/radiance). | ≈6.5 ms |
| 2 | Economy | Always cycling at 1 face/frame (6 frames per pass). | ≈3.2 ms |
| 3 **(default)** | On change | Cycles at `ChangeFaces` (2) until converged, then **idle**. A watchdog pair every 5 s. Animated rig looks use the Every-frame path, plus a settle. A moving subject keeps it cycling at 2 faces/frame (≈6-frame lag; not promoted). | idle ≈0 |

All three budget modes use the Every-frame path for the 2-frame warm-up while the probe is not yet ready.

## 4. Architecture

### 4.1 Pure model — new header `alcineliveproberefresh.h`
Hygiene:
- all functions `inline`;
- includes `stdtypes.h`, `lluuid.h`, `<vector>`, `<cmath>`, `<cstring>`, `<cstdint>`, `<algorithm>`;
- no GL or viewer globals;
- explicit `static_cast` on every narrowing.

Add it to `viewer_HEADER_FILES` (CMakeLists.txt, near :1153). Namespace `ALCineLiveProbeRefresh`:
```cpp
enum class Mode : S32 { EVERY_FRAME = 0, BALANCED = 1, ECONOMY = 2, ON_CHANGE = 3 };
inline Mode sanitizeMode(S32 v);                         // out of range -> ON_CHANGE
enum class Reason : U8 { NONE, WARMUP, CONTINUOUS, CHANGED, WATCHDOG, MANUAL, ANIMATED, SETTLING, CONVERGED };
inline const char* reasonName(Reason r);                 // "none","warm-up","continuous","changed","watchdog",
                                                         // "manual","animated","settling","converged"
enum class Path : U8 { FULL, BUDGET, IDLE };

// ---- signature: per-field sticky quantiser -> one 64-bit hash H (R3: per-field tolerances in field units) ----
enum class Tol : U8 {
    ABS,     // scalar:   |v - acc| > a                          (heights m, EVs stops, 0..1 factors, Kelvin)
    REL,     // scalar:   |v - acc| > max(b, a * max(|v|,|acc|)) (densities, multipliers; b = tiny floor in field units)
    ANGLE,   // 3-vector: angle(v, acc) > a degrees              (sun/moon/emitter directions); if |v| or |acc| < 1e-6 -> ABS 1e-6 per axis
    COLOR    // 3-vector: any channel |v_c - acc_c| > max(b, a * max(|v_c|,|acc_c|))  (per-channel, HDR-safe)
};
struct Field { Tol mTol; U8 mCount; F32 mA; F32 mB; };   // mCount 1 (ABS/REL) or 3 (ANGLE/COLOR)
struct Signature {                       // rebuilt every sampled frame in a fixed append order
    std::vector<F32> mVal; std::vector<Field> mField;    // mVal holds sum(mCount) floats in field order
    U64 mExact = 1469598103934665603ULL;                 // FNV-1a over exact items
    inline void clear();
    inline void addAbs(F32 v, F32 tol);  inline void addRel(F32 v, F32 rel, F32 floor);
    inline void addAngle(const F32* v3, F32 deg);  inline void addColor(const F32* v3, F32 rel, F32 floor);
    inline void addAbs3(const F32* v3, F32 tol);   // three ABS fields (positions, scales, wave dirs)
    inline void addExact(U64 bits); inline void addExact(const LLUUID& id); inline void addExact(bool b);
};
struct StickyHash {                      // a field's accepted value(s) move, as a group, only when that field's test fires
    std::vector<F32> mAcc; std::vector<Field> mLayout;
    inline U64 update(const Signature& s);   // layout (tols/counts) or size change -> mAcc = s.mVal, mLayout = s.mField;
                                             // returns FNV-1a(size, layout, mAcc bits, mExact)
};

// ---- scheduler ----
struct Params { Mode mMode = Mode::ON_CHANGE; S32 mChangeFaces = 2; F32 mWatchdogSec = 5.f; F32 mSettleSec = 0.5f; };
struct State {
    const void* mProbe = nullptr; S32 mCubeIndex = -1; Mode mMode = Mode::EVERY_FRAME;
    bool mActive = false; U8 mFace = 0; bool mRadiance = false; S32 mFaces = 1;   // pass cursor
    U64 mPassH = 0; bool mPassClean = true;
    bool mIrrOk = false; U64 mIrrH = 0; bool mRadOk = false; U64 mRadH = 0;       // last pass of each kind
    bool mLastPassIrr = false;                                                    // kind of the last completed pass
    F64 mLastConverged = -1.0; F64 mLastAnim = -1.0; bool mManual = false;
    Reason mReason = Reason::NONE; F64 mReasonTime = -1.0; };
struct Decision { Path mPath = Path::IDLE; S32 mFaces = 0; Reason mReason = Reason::NONE; };
enum class PassEnd : U8 { NONE, IRRADIANCE, RADIANCE };

inline bool     converged(const State& s, U64 h);        // mIrrOk && mRadOk && mIrrH == h && mRadH == h
inline Decision decide(State& s, const Params& p, U64 h, bool animating, bool ready, F64 now,
                       const void* probe, S32 cube_index);
inline void     noteFrameH(State& s, U64 h);             // every BUDGET frame (blocked or not): if (mActive && h != mPassH) mPassClean = false
inline PassEnd  advanceFace(State& s, F64 now);          // after each captured budget face
inline void     onFullPass(State& s, bool radiance, U64 h, F64 now);   // after each FULL frame (a complete pass)
inline void     reset(State& s);                          // everything
```
**`decide`, exact order:**
1. If `probe != s.mProbe || cube_index != s.mCubeIndex || p.mMode != s.mMode` → `reset(s)`, then store all three.
2. `!ready` → `mActive = false`, return FULL/WARMUP.
3. **BALANCED / ECONOMY:** if `!mActive`, start a pass of kind `!mLastPassIrr ? irradiance : radiance` with faces 2 / 1,
   reason CONTINUOUS. Return BUDGET. H is unused (the caller passes 0 and does not sample).
4. **ON_CHANGE animation.** If `animating` → `mLastAnim = now`. If `animating || (mLastAnim >= 0 && now - mLastAnim <
   p.mSettleSec)` → `mActive = false`, return FULL with reason ANIMATED (if `animating`) or SETTLING. `animating` is
   tested first, so `SettleSec = 0` is exact.
5. If `mActive` → return BUDGET with `mFaces`. A started pass always completes; changes only make it dirty.
6. **Pick a reason:**
   - `MANUAL` if `mManual` (clear it; set `mIrrOk = mRadOk = false`);
   - else `WATCHDOG` if `converged(s,h) && p.mWatchdogSec > 0 && now - mLastConverged >= p.mWatchdogSec` (set
     `mIrrOk = mRadOk = false`);
   - else `CHANGED` (or WARMUP if `mLastConverged < 0`) if `!converged(s,h)`;
   - else return IDLE/CONVERGED.

   Start a pass with faces `clamp(p.mChangeFaces, 1, 6)`. **R3: the kind ALWAYS alternates**, as in step 3:
   `!mLastPassIrr ? irradiance : radiance`, whether or not the previous pass was clean. Cleanliness bookkeeping decides
   only IDLE (via `converged`), never the pass kind. So under continuous change (e.g. walking) both maps still publish
   every `2 × ceil(6 / faces)` frames, i.e. every 6 frames at 2 faces. Convergence still needs a clean irradiance
   followed by a clean radiance on the same H (see `advanceFace`). Return BUDGET.

**Start a pass:** `mActive = true`, `mFace = 0`, `mRadiance = kind`, `mFaces = n`, `mPassH = h`, `mPassClean = true`;
record the reason and time.

**`advanceFace(s, now)`:**
- `++mFace`. If `mFace < 6` → NONE.
- Otherwise the pass ends: `mActive = false`, and `ok = mPassClean`.
  - irradiance → `mIrrOk = ok; mIrrH = mPassH; mRadOk = false; mLastPassIrr = true`; return IRRADIANCE.
  - radiance → `mRadOk = ok && mIrrOk && mIrrH == mPassH; mRadH = mPassH; mLastPassIrr = false`; if `mRadOk` →
    `mLastConverged = now`; return RADIANCE.

**`onFullPass(s, radiance, h, now)`:** a FULL frame is one complete pass sampled at one instant, so it is clean:
- irradiance → `mIrrOk = true; mIrrH = h; mRadOk = false; mLastPassIrr = true`;
- radiance → `mRadOk = mIrrOk && mIrrH == h; mRadH = h; mLastPassIrr = false`; if ok → `mLastConverged = now`.

After settling, step 6 idles immediately if the last FULL pair matched the current h; otherwise it cycles.

**A→B→A:** because `noteFrameH` runs on every frame the pass spans, a transient H change dirties the pass even if H
returns to its start value. The pass still publishes (seam accepted) but does not count.

### 4.2 Rig side — read on demand (no cached sample)
`alcinelightrig.{h,cpp}`, new public method `void ALCineLightRig::appendLiveProbeSignature(ALCineLiveProbeRefresh::Signature& sig) const`,
declared near alcinelightrig.h:231.

For i in `LIGHT_COUNT`, take `mProjectors[i]` then `mOmnis[i]`, then `mCatchlight` (h:378-382). For each:
- **Null or dead:** `sig.addExact(false)`.
- **Otherwise:**
  - `addExact(true)`;
  - `addAbs3(getPositionAgent().mV, move_m)` (the caller passes `move_m` in; the signature becomes
    `appendLiveProbeSignature(Signature&, F32 move_m)` on both rig and manager);
  - `addAbs3(getScale().mV, 0.005f)` (emitter box, i.e. projector geometry; R3);
  - `addAngle((LLVector3(0,0,-1) * getRotation()).mV, 0.25f)`;
  - `addColor(getLightSRGBColor().mV, 0.01f, 1.f/512.f)`;
  - `addRel(getLightIntensity(), 0.01f, 1e-3f)`;
  - `addAbs(getLightRadius(), 0.02f)` (m);
  - `addAbs(getLightFalloff(), 0.01f)`;
  - `addAbs(getSpotLightParams().mV[0], 0.00436f)` (FOV in radians; 0.25°);
  - `addExact(getIsLight())`;
  - `addExact(LLPipeline::isProjectorNoShadow(getID()))`;
  - `addExact(getLightTextureID())`.

New method `bool ALCineLightRig::liveProbeAnimating() const` returns true if any of these holds:
- `mActiveFX >= 0`;
- `mTransitionActive`;
- a projector with `mLastFrame.mProj[i].mOn && goboIsAnimated(mLastFrame.mProj[i].mGobo)`;
- a light (projector or omni on) with `mCurrentLive[i].mFlickerProgram != FLICKER_NONE && mCurrentLive[i].mFlickerAmount > 0`.

`ALCineLightRigManager` (alcinelightrigmanager.{h,cpp}) gets two new public methods:
- `void appendLiveProbeSignature(Signature&, F32 move_m) const`: for each slot, `addExact(isSlotEnabled(slot))`, and for enabled
  slots `at(slot).appendLiveProbeSignature(sig, move_m)`. All rigs are included: their emitters light the capture.
- `bool liveProbeAnimating() const`: OR over enabled slots.

Both compute from live state at call time. Nothing is cached, so nothing goes stale.

### 4.3 Reflection manager — `llreflectionmapmanager.{h,cpp}`
Header:
- `#include "alcineliveproberefresh.h"`.
- Public:
  - `void requestCinematicLiveProbeRefresh()` → `mCineRefresh.mManual = true`;
  - `CinematicRefreshStatus getCinematicRefreshStatus() const`, returning `{mode, path, reason, secondsSinceReason,
    passFace (0-5 or -1), passIsRadiance, converged}`.
- Private members near :267-272: `ALCineLiveProbeRefresh::State mCineRefresh; ALCineLiveProbeRefresh::StickyHash
  mCineSticky; ALCineLiveProbeRefresh::Signature mCineSig; LLVector4a mCineFrozenOrigin; U64 mCineLastH = 0;
  ALCineLiveProbeRefresh::Path mCineLastPath;` and stats `{steps, faces, fullFrames, budgetFrames, idleFrames, irrPub,
  radPub, cleanPasses, dirtyPasses, blocked, resets; F64 windowStart}`.
- Private helpers:
  - `void resetCinematicRefresh()` → `reset(mCineRefresh)`, `mCineSticky.mAcc.clear()`, `++stats.resets`;
  - `void updateRealtimeProbeAllFaces(LLReflectionMap* realtime_probe, LLReflectionMap* cinematicLive)`;
  - `void updateCinematicBudget(LLReflectionMap* probe, S32 faces)`;
  - `U64 sampleCinematicH(LLReflectionMap* probe)`.

`llreflectionmapmanager.cpp` adds `#include "alcinelightrigmanager.h"` (precedent: pipeline.cpp includes it) and
`"llenvironment.h"` (it already uses `LLEnvironment` at :1206).

Steps:
1. **Resets.** Call `resetCinematicRefresh()` immediately before each `return` at :211, :219 and :335. Also call it in
   `setCinematicLiveProbe` when the probe changes (:740 block), in the `initReflectionMaps` reset block (:1572), and in
   `cleanup()` (:1637).
2. **Extract.** Move the body of `if (realtime_probe != nullptr) {...}` (:497-541) **verbatim** into
   `updateRealtimeProbeAllFaces`. Zero token changes.
3. **Dispatch**, replacing :497-541 (the :495-496 `realtime_probe` line is unchanged):
```cpp
static LLCachedControl<S32> refresh_mode(gSavedSettings, "CineLightRigLiveProbeRefresh", 3);
const ALCineLiveProbeRefresh::Mode mode = ALCineLiveProbeRefresh::sanitizeMode(refresh_mode);
if (realtime_probe != nullptr && realtime_probe == cinematicLive &&
    mode != ALCineLiveProbeRefresh::Mode::EVERY_FRAME) {
    LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("rmmu - cine step");
    realtime_probe->autoAdjustOrigin();
    const bool on_change = mode == ALCineLiveProbeRefresh::Mode::ON_CHANGE;
    const U64 h = on_change ? sampleCinematicH(realtime_probe) : 0;
    const bool animating = on_change && ALCineLightRigManager::instance().liveProbeAnimating();
    const bool ready = mCinematicIrradianceReady && mCinematicRadianceReady && realtime_probe->mComplete;
    const Decision d = decide(mCineRefresh, params, h, animating, ready, gFrameTimeSeconds,
                              realtime_probe, realtime_probe->mCubeIndex);
    ++stats.steps; mCineLastPath = d.mPath;
    if (d.mPath == Path::FULL) {
        const bool rad = mRealtimeRadiancePass;              // the pass the moved body will run
        updateRealtimeProbeAllFaces(realtime_probe, cinematicLive);
        onFullPass(mCineRefresh, rad, h, gFrameTimeSeconds);
        ++(rad ? stats.radPub : stats.irrPub); ++stats.fullFrames;
    } else if (d.mPath == Path::BUDGET) {
        noteFrameH(mCineRefresh, h);
        if (realtime_probe == mUpdatingProbe) ++stats.blocked;   // would share the primary scratch: skip, keep the cursor
        else { updateCinematicBudget(realtime_probe, d.mFaces); ++stats.budgetFrames; }
    } else ++stats.idleFrames;
} else {
    if (mCineRefresh.mProbe != nullptr) resetCinematicRefresh();   // Every frame / fallback / paused / no probe
    if (realtime_probe != nullptr) updateRealtimeProbeAllFaces(realtime_probe, cinematicLive);
}
```
   - In Every-frame mode this adds one cached read, one compare, and one pointer test in front of the unchanged body.
   - `params` comes from `LLCachedControl` reads, clamped: ChangeFaces 1..6, WatchdogSec 0..60, SettleSec 0..5.
4. **`updateCinematicBudget(probe, faces)`:**
```cpp
LL_PROFILE_ZONE_NAMED_CATEGORY_DISPLAY("rmmu - cine budget"); LL_PROFILE_ZONE_NUM(faces);
if (!mCineRefresh.mActive) return;
if (mCineRefresh.mFace == 0) mCineFrozenOrigin = probe->mOrigin;   // FREEZE the origin for this pass
const LLVector4a live_origin = probe->mOrigin;
probe->mOrigin = mCineFrozenOrigin;
const bool radiance_pass = isRadiancePass();
mCinematicLiveProbeCapture = true;
const bool saved = gPipeline.beginCinematicProbeCapture();
for (S32 n = 0; n < faces && mCineRefresh.mActive; ++n) {  // stops at the pass end; a pass never starts mid-frame
    mRadiancePass = mCineRefresh.mRadiance;
    updateProbeFace(probe, mCineRefresh.mFace);
    ++stats.faces;
    const PassEnd e = advanceFace(mCineRefresh, gFrameTimeSeconds);
    if (e == PassEnd::IRRADIANCE) { ++stats.irrPub; ++(mCineRefresh.mIrrOk ? stats.cleanPasses : stats.dirtyPasses); }
    if (e == PassEnd::RADIANCE) {
        ++stats.radPub; ++(mCineRefresh.mRadOk ? stats.cleanPasses : stats.dirtyPasses);
        mCinematicIrradianceReady = mCinematicRadianceReady = true; probe->mComplete = true;
    }
}
if (saved) gPipeline.endCinematicProbeCapture();
mCinematicLiveProbeCapture = false;
mRadiancePass = radiance_pass;
probe->mOrigin = live_origin;                               // the influence volume keeps following the subject
if (!mCineRefresh.mActive) updateNeighbors(probe);          // after any pass end
```
   - `mRealtimeRadiancePass` is untouched by this path.
   - `mComplete` and `mFadeIn` are never cleared by it, so the last cube stays displayed while the next one builds.
   - Balanced does exactly 3 frames per pass, Economy 6.
5. **Instrument.** When `CineLightRigLiveProbeRefreshLog` is on, the log prints once per second:
   `LL_INFOS("LiveProbe") "[LiveProbe] mode=… steps faces full budget idle(%) irr_pub rad_pub clean dirty blocked resets hash_us=avg/max converged=y/n last=<reason> <s>s"`.
   Tracy zones: `"rmmu - realtime"` (unchanged), `"rmmu - cine step"`, `"rmmu - cine budget"` (ZONE_NUM = faces), and
   `"cine probe H"` (inside `sampleCinematicH`).

### 4.4 Signature schema — `sampleCinematicH(probe)`, fixed append order
`mCineSig.clear()` first. `move_m = CineLightRigLiveProbeMoveTolerance` (default 0.05, clamp 0.001..1). Every getter
below was verified to exist at HEAD (llsettingssky.h, llsettingswater.h, llsettingsbase.h:265 `getBlendFactor`). Then
append:
**R3 per-field tolerances** are in each field's own units. Notation:
- `Abs(t)`: absolute `t`;
- `Rel(r, f)`: relative `r` with an absolute floor `f` in field units;
- `Ang(d)`: `d` degrees;
- `Col(r, f)`: per channel, relative `r` with floor `f`.

Principle: each tolerance is below the smallest change visible in a capture, and above float jitter.
1. **Probe:**
   - origin `addAbs3(move_m)`;
   - radius `Abs(0.02 m)`;
   - `getAmbiance()` `Abs(0.01)` (feeds `mLightScale` in `updateProbeFace`);
   - `addExact(U64(probe->mCubeIndex))`.
2. **Rigs:** `ALCineLightRigManager::instance().appendLiveProbeSignature(mCineSig, move_m)` (§4.2).
3. **Sky:** `LLEnvironment::instance().getCurrentSky()`; `addExact(false)` if null. The set mirrors
   `LLSettingsVOSky::applyToUniforms` (llsettingsvo.cpp:846-866) and `applySpecial` (:870+). Getters return raw stored
   values (e.g. `getDensityMultiplier`, llsettingssky.cpp:1449), which the atmospheric shaders use directly.
   - **Directions:** `getSunDirection` and `getMoonDirection`, `Ang(0.1°)`. A 4 h day cycle crosses this about every 4 s,
     which gives an organic refresh rate.
   - **Colours** (HDR-safe), `Col(0.005, 1e-4)`: `getSunlightColor`, `getMoonlightColor`, `getCloudColor`,
     `getAmbientColor`, `getBlueDensity`, `getBlueHorizon`, `getGlow`.
   - **Cloud position/density:** `getCloudPosDensity1` and `getCloudPosDensity2`, `Col(0.005, 1e-3)`.
   - **Scalar densities and multipliers:**
     - `getHazeDensity` `Rel(0.005, 1e-4)`;
     - `getDensityMultiplier` `Rel(0.01, 1e-7)` (typical 1e-4: a change 0.0001 → 0.00011 is visible and must fire);
     - `getDistanceMultiplier` `Rel(0.005, 1e-3)`;
     - `getCloudScale` `Rel(0.005, 1e-4)`;
     - `getStarBrightness` `Rel(0.005, 1e-3)`;
     - `getSkyDropletRadius` `Rel(0.005, 0.01)`;
     - `getMaxY` `Rel(0.005, 1.0)` (m).
   - **0..1-range scalars**, `Abs(1e-3)`: `getHazeHorizon`, `getCloudShadow`, `getCloudVariance`, `getMoonBrightness`,
     `getSkyMoistureLevel`, `getSkyIceLevel`, `getReflectionProbeAmbiance(RenderSkyAutoAdjustLegacy)`, `getGamma`,
     `getSunMoonGlowFactor`, `static_cast<F32>(getBlendFactor())`.
   - **Exact:** `getIsSunUp`, and the textures `getSunTextureId`, `getMoonTextureId`, `getCloudNoiseTextureId`,
     `getBloomTextureId`, `getRainbowTextureId`, `getHaloTextureId`, `getNextSunTextureId`, `getNextMoonTextureId`,
     `getNextCloudNoiseTextureId`.
   - Total: 50 floats (33 in eleven 3-vectors, plus 17 scalars), 1 bool and 9 ids.
4. **Water:** `getCurrentWater()`; `addExact(false)` if null. The set mirrors `LLSettingsVOWater::applySpecial` (:1239+).
   - `getWaterFogColor` `Col(0.005, 1e-4)`;
   - `getModifiedWaterFogDensity(false)` `Rel(0.005, 1e-4)`;
   - `Abs(1e-3)`: `getFogMod`, `getFresnelScale`, `getFresnelOffset`, `getScaleAbove`, `getScaleBelow`,
     `static_cast<F32>(getBlendFactor())`;
   - `getBlurMultiplier` `Abs(1e-4)`;
   - `getWave1Dir` and `getWave2Dir`, 2 ABS fields each at `Abs(1e-3)`;
   - `getNormalScale` `addAbs3(1e-3)`;
   - `gPipeline.getRenderWaterHeight()` `Abs(0.01 m)` (absolute metres at any height);
   - `addExact` for `getNormalMapID`, `getNextNormalMapID`, `getTransparentTextureID`, `getNextTransparentTextureID`.
   - Total: **19 floats** (3 + 1 + 6 + 1 + 4 + 3 + 1) and 4 ids.
5. **Capture-relevant settings:**
   - EVs `Abs(0.01 stop)`: `AlchemyEnvSunEV`, `AlchemyEnvMoonEV`, `AlchemyEnvLocalLightEV`, `AlchemyEnvShadowLiftEV`;
   - `AlchemyEnvSunKelvin` `Abs(10 K)`;
   - tint colours per channel `Abs(1/512)`: `AlchemyEnvSunTintColor`, `AlchemyEnvMoonTintColor` (rgb);
   - strengths `Abs(1e-3)`: `AlchemyEnvSunTintStrength`, `AlchemyEnvMoonTintStrength`;
   - exact: `AlchemyEnvMoonLinked`, `AlchemyEnvLocalLightIncludeRig`, `RenderShadowDetail`, `RenderLocalLightCount`,
     `RenderSkyAutoAdjustLegacy`.

   GI/ambient sampling EVs are identity during captures (alenvintensity.cpp:297) and are excluded.
6. `const U64 h = mCineSticky.update(mCineSig); mCineLastH = h; return h;`

**Sampling cost (R3).** Wrap `sampleCinematicH` in an `LLTimer`. The `[LiveProbe]` line reports `hash_us=avg/max` per
second. Expected: well under 50 µs with all 5 rigs enabled (≈45 emitters × ~20 floats + ~100 env values; INFERENCE). It
must be profiled in-world with all rigs on (§8.11).

Sizes are dynamic, so there is no overflow. An emitter or null-object change alters the size or the exact hash, and
therefore H. Sticky quantisation means float noise below tolerance never moves an accepted value, so H cannot flicker.
Slow drift accumulates until it crosses tolerance, which gives exactly one change. Deliberately not sampled: cloud scroll
position (continuous; covered by the watchdog) and camera-dependent uniforms.

## 5. Settings (`app_settings/settings.xml`, after `CineLightRigLiveProbeGizmo`, :2169-2175)
| Name | Type | Default | Persist |
|---|---|---|---|
| CineLightRigLiveProbeRefresh | S32 | 3 | 1 |
| CineLightRigLiveProbeChangeFaces | S32 | 2 | 1 |
| CineLightRigLiveProbeWatchdogSec | F32 | 5.0 | 1 |
| CineLightRigLiveProbeSettleSec | F32 | 0.5 | 1 |
| CineLightRigLiveProbeMoveTolerance | F32 | 0.05 | 1 |
| CineLightRigLiveProbeRefreshLog | Boolean | 0 | 0 |

Each key gets a Comment giving its meaning and range. The refresh mode is per-machine: not in `LiveProbeConfig`, scene
LLSD or Setups. Add `"CineLightRigLiveProbeRefresh"` to the panel reset list after alpanelcinelightrig.cpp:509.

## 6. UI — `panel_cine_light_rig.xml`, `cine_probe_card` (:679-755)
- Card height 172 → **214**.
- New row at top 112:
  - text "Refresh" (left 8, width 52);
  - `<combo_box control_name="CineLightRigLiveProbeRefresh" name="cine_live_probe_refresh" left="62" top="108"
    width="170" height="20">`, items 0..3: "Every frame", "Balanced", "Economy", "On change";
  - `<button name="cine_live_probe_refresh_now" label="Refresh now" left="240" top="108" width="92" height="20">`.
- Combo tooltip: "How often the live probe re-renders its six cube faces. Every frame: full quality, highest cost.
  Balanced: 2 faces per frame (~1/3 cost). Economy: 1 face per frame (~1/6 cost); both lag and can seam with
  flickering lights. On change: refreshes at 2 faces per frame until the probe matches the scene, then stops
  (re-checks every 5 s); animated rig looks switch to Every frame."
- `cine_live_probe_status`: top 112 → 138, height 52 → 68.
- Refresh line kept to ≤ 45 characters:
  - `Refresh: Every frame`
  - `Refresh: Balanced, irradiance 3/6`
  - `Refresh: On change, idle (changed 4s ago)`
  - `Refresh: On change, live (animated)`
  - `Refresh: On change, refreshing 3/6`

  Wrap is verified in-world (§8.8).
- Tooltips on `cine_live_probe_enable` (:695) and `cine_easy_probe_enable` (:164): "Cost depends on Live Probe >
  Refresh (Every frame = six cubemap views per frame)."
- `postBuild` (near :597): the button calls `gPipeline.mReflectionMapManager.requestCinematicLiveProbeRefresh()`.
- The status line is built in `updateDerivedStatus` (:2244-2286) for WARMING/LIVE.

## 7. Model tests (TUT, `tests/alcinelightrigmodel_test.cpp`, include `../alcineliveproberefresh.h`)
The model proves only scheduling and hashing logic, using synthetic `h` values at 60 Hz timestamps:
- M1 `sanitizeMode` bounds.
- M2 not ready → FULL/WARMUP in all budget modes.
- M3 Balanced: pass ends on frames 3 (irradiance), 6 (radiance), 9 (irradiance)…; never IDLE.
- M4 Economy: 6 frames per pass, alternating, never IDLE.
- M5 On change, constant h: one clean irradiance pass, one clean radiance pass, then IDLE; `converged` true.
- M6 h changes on one frame inside an irradiance pass → dirty, not counted, cycling continues; once h is stable, two
  clean passes, then IDLE.
- M7 A→B→A inside a radiance pass → dirty; not converged; another pass runs.
- M8 a clean irradiance at h1, then h2 before the radiance → the next pass is still radiance (alternation). It is
  not counted (`mIrrH != h2`); the irradiance → radiance pair that follows on h2 converges.
- M9 watchdog: IDLE until 5.0 s after convergence, then exactly one pair, then IDLE; `WatchdogSec = 0` → never.
- M10 manual request while IDLE → exactly one pair.
- M11 `animating` → FULL/ANIMATED; after it clears, FULL/SETTLING for 0.5 s, then IDLE when the last FULL pair matched
  the current h. SettleSec = 0 → FULL exactly while animating.
- M12 a probe pointer, cube index or mode change resets the state.
- M13 StickyHash: noise below tolerance leaves H constant over 1000 frames; drift above tolerance changes H by single
  transitions with no oscillation; a size change or exact-id change changes H.
- M14 (R3, P1-A) H changes on **every** frame for 600 frames at ChangeFaces 2: irradiance and radiance publications
  strictly alternate; the gap between consecutive radiance publications is ≤ 6 frames; never IDLE; never converged.
  Once H is stable: converges within ≤ 12 frames.
- M15 (R3, P1-B) per-field tolerance cases: a visible change fires H and bounded noise does not.

  | Field | Visible change (must change H) | Noise over 1000 frames (must not) |
  |---|---|---|
  | `densityMultiplier` `Rel(0.01, 1e-7)` | 1.0e-4 → 1.1e-4 | ±0.4 % |
  | water height `Abs(0.01)` at 20 m | +0.02 m | ±0.004 m |
  | sun dir `Ang(0.1°)` | rotated 0.2° | ±0.04° |
  | colour `Col(0.005, 1e-4)` | +1 % in one channel at value 1.0 | ±0.2 % |
  | colour `Col(0.005, 1e-4)` | 0 → 3e-4 | ±5e-5 |
  | emitter position `Abs(0.05)` | +0.06 m | ±0.02 m |
  | emitter scale `Abs(0.005)` | +0.01 m | ±0.002 m |
  | haze horizon `Abs(1e-3)` | +0.003 | ±0.0004 |

Count: 15 tests (verify by counting `template<> template<>` additions).
**Not provable by model tests** (stated explicitly):
- real frame order;
- scratch isolation;
- the extracted body being pixel-identical.

These are covered in §8.

## 8. In-world validation (user runs; `CineLightRigLiveProbeRefreshLog` on; Tracy 10 s per step; one rig; probe + shadows on)
0. **Every-frame identity.**
   - **Structural:** before the build, `git diff --color-moved=zebra` shows :497-541 as moved and unedited. In world,
     mode 0: no `[LiveProbe]` line, no `"rmmu - cine step"` zone, six "Render Cube Face" zones per frame under
     `"rmmu - realtime"`.
   - **Pixel (R3).** Structural checks do not prove pixels, so do an image comparison:
     - Setup: a static scene with no water in view; a fixed EEP with cloud scroll 0; a frozen Director camera; mode 0.
       Wait 10 s after login so textures settle.
     - With the **backup exe** (`AlchemyTest.pre-liveproberefresh.exe`, HEAD), take two lossless PNG snapshots, A1 and A2,
       a few seconds apart. Their difference is the noise floor.
     - With the **new build**, same place, camera and settings, take B.
     - Compare with any image-diff tool (e.g. ImageMagick `magick compare -metric AE`).
     - PASS: diff(A1, B) ≤ diff(A1, A2). FAIL: diff(A1, B) > diff(A1, A2). INCONCLUSIVE: the A1/A2 floor is large
       (animated content in view); retake in a more static spot.
1. **Balanced:** `faces = 2 × budget`, `irr_pub + rad_pub ≈ steps/3`, idle 0 %, blocked 0. Tracy: two cube faces under
   `"rmmu - cine budget"` per frame.
2. **Economy:** `faces = budget`, publications ≈ steps/6.
3. **On change, untouched scene:** `converged=y`, idle ≥ 95 %, one pair every 5 s (faces 12, clean 2). Frame time ≈
   probe-OFF + <0.3 ms.
4. **Edit the Key EV once:** `last=changed`, then clean passes, then idle.
5. **Flicker or FX look:** `last=animated`, `full = steps`, and probe flicker in sync with direct light.
6. **Walk the subject:** dirty passes rise while walking; clean pair + idle within ≈ 12 frames of stopping. Standing AO
   idle must stay idle; if not, report the log (MoveTolerance).
7. **Edit sky/water in the Environment floater; EEP swap; water-only change:** `last=changed`, then idle. During the EEP
   pause, steps stop and `resets` increments.
8. **UI:** the status line does not clip at 68 px in any state.
9. **Attachment check:** a large worn attachment is absent from the probe. If it is present, report it (watchdog latency
   only).
10. **Snapshot / 360, mode switches, target switch, teleport:** `steps` equals the frames the manager ran; no garbage
    probe; `blocked` stays 0. Fallback: probe off + realtime detail → closest-dynamic still updates every frame.
11. **Hash cost (R3):** enable all 5 rig slots with all lights on, On change mode. Report `hash_us`. Expected avg <
    50 µs; a max above 200 µs is a FAIL to report (INFERENCE threshold).
12. **Walk continuously for 10 s (P1-A):** irr_pub ≈ rad_pub, both > 0 every second, and reflections visibly follow
    the subject with ~6 frames of lag.

A counter that contradicts the predicted value for its step is a FAIL. Report the raw log line; do not interpret it.

## 9. OFF-LIMITS
- Shaders.
- `llheroprobemanager.*`.
- `pipeline.cpp`.
- `llviewerdisplay.cpp`, `llviewerwindow.cpp`, `llreflectionmap.*`, `llenvironment.*`, `llsettingsvo.*`.
- In the reflection manager: the `updateProbeFace` body, `doProbeUpdate`, round-robin, the closest-dynamic fallback, and
  the Every-frame body (move only).
- `LiveProbeConfig` / scene LLSD / Setups.
- The `enve` tree.

## 10. Implementation order (single delivery; one build after the review converges)
- [ ] 1. `alcineliveproberefresh.h` (§4.1) + CMake header entry.
- [ ] 2. TUT M1-M15.
- [ ] 3. settings.xml: 6 keys (count them).
- [ ] 4. Rig and rig manager: `appendLiveProbeSignature`, `liveProbeAnimating`.
- [ ] 5. Reflection manager, in this order:
  - resets at the 3 early returns and 3 lifecycle sites;
  - verbatim extraction;
  - dispatch;
  - `updateCinematicBudget`;
  - `sampleCinematicH` (§4.4 order);
  - status;
  - log.
- [ ] 6. XUI + panel: combo, Refresh-now button, status 68 px, tooltips, reset list.
- [ ] 7. Adversarial review. Ask specifically:
  - (a) is the Every-frame body verbatim;
  - (b) can anything else write the secondary scratch mid-pass;
  - (c) the frozen-origin save/restore;
  - (d) `mComplete` is never cleared;
  - (e) resets at every early return and on the else branch;
  - (f) the pass-kind rule keeps irradiance → radiance;
  - (g) every §4.4 getter compiles with its exact name;
  - (h) every §4.4 field uses the tolerance kind and value listed (count fields against the schema totals: sky 50
    floats, water 19);
  - (i) the pass kind always alternates.

  Loop to 0 must-fix.
- [ ] 8. Checkpoint commit + exe backup (`AlchemyTest.pre-liveproberefresh.exe`, needed for §8.0), then ONE build, then hand §8 to the user.
