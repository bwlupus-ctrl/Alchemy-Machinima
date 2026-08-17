# VCAM GATE — Design Document

**Feature:** "Gate" — a live‑TV video switchboard / production gate for the prim‑free
virtual cameras of the Prism engine.
**Status:** DESIGN ONLY. No source is modified by this document. It is written to be
consumable as a Codex implementation brief → adversarial review → build.
**Repo:** `I:\alchemy-machinima` (Alchemy‑Machinima Second Life viewer fork).

Every claim about *current* code is cited `file:line`. Every *design* claim is labelled:

- **PROVES** — verified directly in the code cited.
- **IMPLIES** — strong inference from cited code, not literally spelled out.
- **INFERS** — an assumption I am making that a reviewer/build must confirm.

Line numbers are against the tree as read on 2026‑08‑17. Treat them as anchors; a Codex
run should re‑resolve by symbol if a file has drifted.

---

## 0. One‑paragraph thesis

The Prism engine **already renders only the cameras that are being watched** and nothing
else — a capture with no visible display bound to it is dropped to 0 Hz before any GPU work
(`llprismlens.cpp:2990`). The Gate does **not** add a scheduler, a render pass, or a
shader. It is a thin *orchestrator* that, each frame, (a) points the subscribed monitor
displays at whichever armed camera is currently **on‑air**, and (b) for a short pre‑warm
window, nudges the *next* camera into the "watched" set. The existing demand‑driven
admission engine then does everything else: it renders the on‑air camera (and, briefly, the
pre‑warm camera) and leaves every other armed camera suppressed at zero cost. The Gate's
switching *brain* is not new either — it reuses the already‑shipped, adversarially‑tested
`ALDirectorSwitcherModel::Controller` (`aldirectorswitchermodel.h:49`), the same
deterministic scheduler the Director camera switcher already drives
(`aldirectorswitcher.h:124`).

---

## STEP 1 — The Prism engine (substrate the Gate sits on)

### 1.1 Captures vs Displays: the two bounded arrays

- **PROVES.** Captures (sources) and Displays (surfaces) are separate, separately‑capped
  arrays. `constexpr U32 MAX_CAPTURES = 3;` (`llprismlens.h:28`) and
  `constexpr U32 MAX_DISPLAY_BINDINGS = 16;` (`llprismlens.h:29`). `MAX_LENSES` aliases
  `MAX_CAPTURES` (`llprismlens.h:30`). **`MAX_CAPTURES == 3` is the hard cap on
  simultaneous producers** and the single most important number in this design.
- **PROVES.** The runtime capture object is `struct PrismInstance` (`llprismlens.cpp:1583`).
  The runtime display object is `struct PrismDisplay` (`llprismlens.cpp:1616`). The public,
  serializable shapes are `CaptureDefinition` (`llprismlens.h:424`) and `DisplayDefinition`
  (`llprismlens.h:436`); a `RegistrySnapshot` carries fixed arrays of both
  (`llprismlens.h:452-453`).
- **PROVES.** A capture's mode is `PrismInstance::mMode`
  (`ECaptureMode::SURFACE_LENS | CAMERA_FEED`, `llprismlens.cpp:1587`,
  enum `llprismlens.h:42`). Camera source object id is `PrismInstance::mCameraObjectId`
  (`llprismlens.cpp:1592`).
- **PROVES.** The prim‑free virtual‑camera transform lives on `CameraSettings`:
  `mVirtual` / `mVirtualPos` / `mVirtualRot` (`llprismlens.h:242-244`), stored in **AGENT
  space**, forward = local −Z, up = local +Y (`llprismlens.h:236-244`). A CAMERA_FEED
  capture with `mVirtual == true` derives its eye/orientation from that stored transform and
  ignores `mCameraObjectId` (`llprismlens.cpp:2735-2744`). The additive Bone‑POV driver is
  `CameraSettings::mBonePov` (`llprismlens.h:248`, struct `llprismlens.h:188`).

### 1.2 How a DISPLAY binds to a CAPTURE (the reference the Gate rewrites)

- **PROVES.** At runtime a display references its capture by **slot index**:
  `PrismDisplay::mCaptureSlot` (`llprismlens.cpp:1620`) plus a generation guard
  `PrismDisplay::mCaptureGeneration` (`llprismlens.cpp:1621`). Every place that gathers "the
  displays of capture *slot*" filters on `display.mCaptureSlot == slot &&
  display.mCaptureGeneration == capture.mHandle.mGeneration`
  (e.g. `llprismlens.cpp:2708-2712`, `2231-2232`, `2471-2472`).
- **PROVES.** In persistence, a display references its capture by **UUID**, not slot:
  `item["capture_id"] = mLenses[display.mCaptureSlot].mHandle.mId`
  (`llprismlens.cpp:3962`); on load the UUID is validated against the parsed capture set
  (`llprismlens.cpp:4405-4406`) and resolved back to a slot at commit
  (`llprismlens.cpp:4575-4585`).
- **IMPLIES.** Because the runtime binding is a single mutable field (`mCaptureSlot`) that
  the engine re‑reads every frame, **the Gate can "route" a monitor simply by writing that
  field** to the on‑air producer's slot. No new binding type is needed for hard‑cut routing.

### 1.3 The demand‑driven admission scheduler (why "only watched cameras render" is free)

The per‑frame driver is `renderAuxiliaryView()` (`llprismlens.cpp:5729`), the sole caller of
which is the main display loop `llviewerdisplay.cpp:919` (**PROVES** — one call per
presented frame). Its shape (**PROVES**, `llprismlens.cpp:5772-5788`):

1. `for slot in [0,MAX_LENSES): registry.prepare(slot, …)` — prepare **all** slots
   (`llprismlens.cpp:5774-5781`).
2. `render_slot = registry.chooseRenderSlot()` — pick **at most one** slot
   (`llprismlens.cpp:5784`).
3. Render exactly that one slot, or return if none (`llprismlens.cpp:5785-…`).

Key mechanisms:

- **PROVES — the watched flag.** `PrismInstance::mAnyDisplayVisible`
  (`llprismlens.cpp:1598`) is **recomputed every frame** inside `prepareCameraCapture()`:
  it is set false at entry (`llprismlens.cpp:2697`), then set true iff at least one
  *occupied, generation‑matched* display bound to this slot passes its on‑screen visibility
  test (`prepareDisplayFrame` → `mAnyDisplayVisible = true`, `llprismlens.cpp:2706-2718`).
  The Surface‑Lens path sets it the same way (`llprismlens.cpp:2463`). Visibility is a real
  footprint test — a display smaller than `MIN_VISIBLE_EXTENT/AREA` returns false and does
  **not** mark its capture watched (`llprismlens.cpp:2444-2449`).
- **PROVES — the admission gate (the hook the Gate reuses).**
  `updateCadenceEntitlements()` at `llprismlens.cpp:2990`:
  `if (!capture.mOccupied || !capture.mAnyDisplayVisible || !capture.mFrame.mPrepared ||
  now < capture.mRetryAfterTime) { mRequestedHz = 0; mEntitlementHz = 0; … continue; }`.
  **A capture that is not watched gets zero requested/entitlement Hz and is never
  rendered.** The identical predicate re‑gates selection in `chooseRenderSlot()`
  (`llprismlens.cpp:3089-3093`).
- **PROVES — target sizing depends on being watched.** `prepareCameraCapture()` returns
  early with `false` and never sizes a render target if `!mAnyDisplayVisible`
  (`llprismlens.cpp:2792-2795`); the canonical target aspect/size is otherwise derived from
  the bound displays' visible footprints (`required_width/height`,
  `llprismlens.cpp:2704-2717`, `2797-2809`). *(This is why pre‑warm needs a seed size — see
  §D.)*
- **PROVES — one render per frame, round‑robin.** `chooseRenderSlot()` scans
  `(mNextRenderSlot + offset) % MAX_CAPTURES` (`llprismlens.cpp:3085-3087`) and returns a
  single best slot; `renderAuxiliaryView` renders only that one (`llprismlens.cpp:5784`). So
  **two watched cameras do not double per‑frame GPU cost — they interleave frames**, each
  taking a share of the single auxiliary render slot. This is load‑bearing for the pre‑warm
  cost claim (§D) and the crossfade push‑back (§E).
- **PROVES — fair sharing.** When more than one capture is watched, a bounded max‑min water
  filling splits the global capture budget among them (`llprismlens.cpp:3005-3031`); the
  budget itself is `prismCaptureBudgetHz()` scaled by the adaptive cadence factor
  (`llprismlens.cpp:2954-2969`).
- **PROVES — the state machine.** `EOutputState { EMPTY, CURRENT, HELD, SUPPRESSED }`
  (`llprismlens.h:91-97`). `CURRENT` vs `HELD` is decided by whether the last render was
  this frame (`llprismlens.cpp:2984-2986`). `suppressOutput(slot)`
  (`llprismlens.cpp:4805-4822`) releases the slot's GPU output and the pooled scratch
  buffers (`llprismlens.cpp:4808-4811`) and stamps `SUPPRESSED`
  (`llprismlens.cpp:4816`); a suppressed‑but‑watched capture becomes `WAITING`, otherwise
  `IDLE` (`llprismlens.cpp:4817-4819`).
- **PROVES — supporting counters.** `displayCount()` (`llprismlens.cpp:4784`),
  `mDisplayCount` per capture (`llprismlens.cpp:1597`), the runtime/config revisions bumped
  on mutation (e.g. `++mRuntimeRevision` `llprismlens.cpp:4821`, `++mRevision`
  `llprismlens.cpp:4599`). `PerformanceSnapshot::mRevision` (`llprismlens.h:281`) is the
  UI‑poll signature.

**Design consequence (IMPLIES).** The complete, sufficient recipe for "make camera X render
and nothing else" already exists and is exercised every frame: *ensure exactly one
visible display is bound (`mCaptureSlot`) to X, and no visible display is bound to any other
camera.* The Gate's whole job is to keep that invariant pointed at the on‑air camera and to
briefly extend it to the pre‑warm camera. **The Gate must layer on `mAnyDisplayVisible`; it
must not touch the water‑filling math or `chooseRenderSlot`.**

### 1.4 Where the virtual transform is consumed / where render happens

- **PROVES.** Virtual eye/orientation are consumed in `prepareCameraCapture()`
  (`llprismlens.cpp:2735-2744`) and the aux view is produced in `renderAuxiliaryView()`
  (`llprismlens.cpp:5729`, render body from `5790`). Runtime‑only transform writes exist and
  **do not churn the configuration revision**: `setVirtualCameraTransform()`
  (`llprismlens.h:561`, `llprismlens.cpp:3453`) and `setVirtualCameraPosition()`
  (`llprismlens.h:568`). `addVirtualCamera()` allocates a virtual CAMERA_FEED capture
  (`llprismlens.h:536`, `llprismlens.cpp:1838`).

### 1.5 Persistence (the round‑trip the Gate rides)

- **PROVES.** `sceneData()` (`llprismlens.cpp:3852`, free fn `5668`) emits a map with two
  top‑level arrays, `prism_captures` and `prism_displays` (`llprismlens.cpp:3855-3856`).
  `applySceneData()` (`llprismlens.cpp:4033`, free fn `5673`) parses/validates atomically
  and commits only after full cross‑reference validation (`llprismlens.cpp:4551-4557`).
- **PROVES — additive precedent, no version bump.** `bone_pov` (`llprismlens.cpp:3949`) is
  written unconditionally and read *optionally* — an absent block loads as defaults
  (comment `llprismlens.cpp:3886-3895`, `3911-3913`). `screen_effects`
  (`llprismlens.cpp:4004`, optional read `4460-4463`) and the virtual‑camera/screen fields
  (`virtual`, `virtual_pos`, `virtual_rot`, `width`, `height`) follow the same
  optional‑on‑read discipline (`llprismlens.cpp:3893-3909`, `4005-4027`). **None of these
  bumped a scene version.**
- **PROVES — the Director scene wrapper.** `LLFloaterDirector::saveScene` sets
  `scene["version"] = SCENE_VERSION;` where `constexpr S32 SCENE_VERSION = 3;`
  (`llfloaterdirector.cpp:86`, `:885`) and then **lifts** the two Prism arrays to the top
  level of the scene map: `scene["prism_captures"] = prism["prism_captures"];
  scene["prism_displays"] = prism["prism_displays"];` (`llfloaterdirector.cpp:887-888`).
  `loadScene` passes the *whole* scene map to `applySceneData`
  (`llfloaterdirector.cpp:1005`).
- **PROVES — forward/backward tolerance.** Load is deliberately allow‑listed: the Director
  floater "only apply[s] keys this floater owns" (`llfloaterdirector.cpp:1041` comment) and
  never rejects unknown top‑level keys. `applySceneData`'s guard requires only that
  `prism_captures`/`prism_displays` exist as arrays (`llprismlens.cpp:4075-4080`); extra
  keys are ignored. Version handling migrates/tolerates rather than hard‑rejecting: a version
  neither 1 nor 3 logs "unsupported … preserving live Prism configuration" and still loads
  legacy fields (`llfloaterdirector.cpp:1015-1023`). The "Version‑3" strings inside
  `llprismlens.cpp` (e.g. `:4079`) are **prose only** — `applySceneData` reads no numeric
  version.
- **PROVES — load caps.** On load, `captures_data.size() > MAX_CAPTURES ||
  displays_data.size() > MAX_DISPLAY_BINDINGS` fails the parse
  (`llprismlens.cpp:4083-4087`); capture ids must be non‑null and unique
  (`llprismlens.cpp:4105-4108`); a display referencing an unknown capture fails
  (`llprismlens.cpp:4405-4406`).

**Design consequence (IMPLIES).** A new top‑level `prism_gates` array rides the identical
round‑trip: emit it from `sceneData()`, parse it *optionally* in `applySceneData()`, and add
one lift line in `saveScene`. Old viewers ignore it; new viewers loading an old scene get an
empty gate. **No `SCENE_VERSION` bump is required** (mirrors bone_pov).

### 1.6 The Prism Manager UI (where the Gate UI lives)

- **PROVES.** Floater `floater_prism_manager.xml` — `name="prism_manager"`, `width="900"
  height="844"`, `min_width="720" min_height="520"`, `can_resize="true"`, title "Virtual Cam
  Manager" (`floater_prism_manager.xml:2-13`). Fixed `summary_panel` (top 8, h 74) and
  `captures_panel` (top 88, h 150) precede a `tab_container name="detail_tabs"` (top 244,
  h 561, w 880, `floater_prism_manager.xml:166-174`) with tabs `capture_tab`,
  `displays_tab`, `performance_tab`. Bottom status line `status_text` at top 814.
- **PROVES.** Capture list is a `scroll_list name="capture_list"` (single‑select,
  `floater_prism_manager.xml:93-113`), bound `mCaptureList`
  (`llfloaterprismmanager.cpp:333`), rebuilt in `rebuildCaptureList()`
  (`:886-934`) with each row's value = capture UUID (`:906`); selection handler
  `onCaptureSelectionChanged()` (`:1378-1399`) reads `getSelectedValue().asUUID()`.
  Displays list is `display_list` rebuilt by `rebuildDisplayList()` (`:936-1007`), each row
  value = display UUID (`:981`), filtered to the selected capture (`:951-954`).
- **PROVES — poll/refresh model.** `draw()` calls `pollSnapshots(false)` each frame
  (`:663-667`), throttled by `SNAPSHOT_POLL_SECONDS = 0.25f` (`:36`, gate `:671-674`).
  Refresh is revision‑driven: it compares config/runtime/performance revisions
  (`:678-692`) and calls `refreshConfiguration()` / `refreshRuntime()` /
  `refreshPerformance()` accordingly. Any mutation calls `invalidateRegistrySnapshot()`
  (`:2110-2114`) which forces an immediate re‑poll. **This is the pattern a Gate mutation
  must follow.**
- **PROVES — mode‑toggle + spinner precedent.** The rate controls are the exact analog of a
  Manual/Auto toggle + interval spinner: `rate_mode` combo (`…:459-462`, `mRateModeCombo`
  `cpp:399`), `target_fps` spinner (`…:463-476`), commit `onCommitRateSettings()`
  (`cpp:1738-1761`). Layout is hand‑placed top‑left pixels (`layout="topleft"`), no rect
  arithmetic. Destructive per‑row actions confirm first: `remove_capture` →
  `GenericAlertYesCancel` → `LLPrismLens::removeCapture()` (`cpp:1462-1490`). Controls are
  greyed with a reason via `setActionState(ctrl, enabled, tooltip)` (`cpp:266-273`).
- **PROVES — settings‑bound controls need no handler.** The Performance tab binds combos
  directly to debug settings via `control_name` (e.g. `PrismAdaptivePerformance`,
  `floater_prism_manager.xml:723-765`).

### 1.7 Existing switcher precedent (the Gate's scheduling brain, already shipped)

- **PROVES.** `ALDirectorSwitcherModel` is a "Deterministic, renderer‑independent Director
  camera switch scheduler" (`aldirectorswitchermodel.h:3`). It exposes `SLOT_COUNT = 12`
  (`:20`), a pure `Config { mAuto, mSequence, mIntervalSeconds, mJitterSeconds, mSeed,
  mEnabled[12] }` (`:27-35`), a `Frame { mCut, mSlot, mBoundary, mEventIndex,
  mEventsElapsed }` result (`:37-44`), `sanitizeConfig()` (`:47`), and
  `Controller::update(now, config)` / `manualPunch(slot, now)` / `rebase(now)` / `reset()` /
  `activeSlot()` (`:56-69`). A large forward time jump "advances directly to the final
  elapsed event and emits at most one cut" (`:54-55`); a manual punch works on a
  disabled slot and restarts the interval (`:58-60`).
- **PROVES.** The model is already adversarially unit‑tested:
  `indra/newview/tests/aldirectorswitchermodel_test.cpp` (header `:1-3`, "Adversarial tests
  for the deterministic Director switch scheduler").
- **PROVES.** The viewer adapter `ALDirectorSwitcher` owns this controller
  (`aldirectorswitcher.h:124`), drives the *viewer cinematic camera* among 12 rig slots,
  and calls `mController` from `tick(presentation_time)` (`:62`), with `punch(slot)` (`:70`)
  and bank persistence `loadBank()/saveBank()` (`:98-99`). **This is a shipped precedent for
  exactly the manual‑take + auto‑cycle + boundary logic the Gate needs.**

**Design consequence (IMPLIES).** The Gate's cycle/manual‑take scheduling is a *solved
problem* in this codebase. The Gate is the **prism‑producer analog** of `ALDirectorSwitcher`:
where `ALDirectorSwitcher` re‑points the single viewer camera, the Gate re‑points the prism
monitors' `mCaptureSlot`. **Reuse `ALDirectorSwitcherModel::Controller` as the Gate's brain**
rather than authoring a second scheduler.

---

## STEP 2 — Design resolutions (A–H)

### A. The Gate model — what it is and where it lives

**Recommendation.** Add a `PrismGate` that lives **inside `PrismLensRegistry`** (as a member
`mGate`), so it can read/write `mLenses[]`/`mDisplays[]` directly — the same class that owns
all capture/display mutation today. Its *scheduling brain* is a reused
`ALDirectorSwitcherModel::Controller`; its *prism glue* (producer materialization + monitor
routing + pre‑warm) is the only new logic.

Concrete shape (INFERS — proposed):

```
// llprismlens.h  (public, pure-data, pointer-free — mirrors CameraSettings)
enum class EGateMode : U8 { MANUAL, AUTO_CYCLE };

struct GateArmedCamera            // a lightweight camera DEFINITION, not a producer
{
    LLUUID       mCaptureId;      // identity of the armed source capture (see §B)
    // (v1 arms an EXISTING capture by id; see §B for the "definition" discussion)
};

struct GateSettings               // serializable, rides prism_gates
{
    bool          mActive     = false;
    EGateMode     mMode       = EGateMode::MANUAL;
    F64           mIntervalSeconds = 8.0;   // ALDirectorSwitcherModel::DEFAULT_INTERVAL_SECONDS
    F64           mJitterSeconds    = 0.0;
    U32           mPrewarmFrames     = 3;    // §D
    std::vector<GateArmedCamera> mArmed;     // ORDERED; cap = 12 (see §B)
    S32           mProgramArmIndex   = 0;    // on-air index into mArmed (MANUAL authority)
    S32           mPreviewArmIndex   = -1;   // MANUAL preview (candidate for TAKE)
};
```

Runtime (INFERS — inside the registry, not serialized):

```
struct PrismGateRuntime
{
    ALDirectorSwitcherModel::Controller mController;   // reused brain
    S32  mProgramSlot = -1;   // reserved producer holding the on-air camera
    S32  mWarmSlot    = -1;   // reserved producer holding the pre-warm camera
    S32  mOnAirArmIndex = -1; // resolved this frame
    U64  mCutSerial   = 0;
};
```

- **ONE global gate for v1 (recommendation).** **IMPLIES** from `MAX_CAPTURES == 3`
  (`llprismlens.h:28`): a gate needs 1 producer slot on‑air and a 2nd during pre‑warm; two
  independent active gates would need up to 4 producer slots and cannot fit. Model the data
  as an *array* `prism_gates` (so v2 can add gates) but **enforce exactly one `mActive`
  gate** at a time in v1. This mirrors the single `ALDirectorSwitcher` instance
  (`aldirectorswitcher.h:58`).
- **How a MONITOR subscribes.** A monitor is an ordinary `PrismDisplay` flagged
  `mGateSubscribed = true` (new bool on `PrismDisplay`, `llprismlens.cpp:1616`). Each frame,
  before the prepare loop, the Gate writes every subscribed display's `mCaptureSlot` /
  `mCaptureGeneration` to the on‑air producer slot. Non‑subscribed displays keep their fixed
  binding untouched — **the existing display→capture path is byte‑identical for non‑gate
  displays** (they are simply skipped by `mGateSubscribed == false`).

### B. Camera count — definitions vs producers, and the exact cap

The switchboard wants **many angles, one renderer**. `MAX_CAPTURES = 3`
(`llprismlens.h:28`) caps *simultaneous producers*, not *definitions*.

**Recommendation — arm lightweight definitions; materialize only on‑air (+pre‑warm).**

Two viable shapes; recommend the second for v1 clarity, note the first as the truly‑scalable
form:

1. **v1 (simplest, honest): arm EXISTING captures by id, cap = `MAX_CAPTURES = 3`.**
   `GateArmedCamera.mCaptureId` names one of the ≤3 real captures. This ships with **zero**
   new producer‑pool machinery and reuses the whole engine as‑is. Cost: the operator can arm
   at most 3 angles — which is the entire simultaneous‑render budget anyway, so the "many
   angles" ambition is unmet. **Use this only if schedule pressure demands the smallest
   possible change.**

2. **v1‑recommended: arm up to 12 lightweight definitions; keep a 2‑slot producer pool.**
   `GateArmedCamera` carries a full `CameraSettings` snapshot (pure, pointer‑free —
   `llprismlens.h:206-249`) instead of pointing at a live capture. The Gate **reserves up to
   two producer slots** while active (`mProgramSlot`, `mWarmSlot`) and *reprograms them in
   place* from the armed definition — it never allocates/frees per cut (see §C/§D). Armed
   cap = **`ALDirectorSwitcherModel::SLOT_COUNT = 12`** (`aldirectorswitchermodel.h:20`),
   which also lets the Gate map `mArmed` 1:1 onto the reused controller's 12 `mEnabled`
   slots for free. **INFERS**: 12 is chosen to match the existing switcher's slot vocabulary,
   not because the engine requires it; it is a soft, data‑only cap.

**Exact caps to state in the brief:**

| Quantity | Value | Source |
|---|---|---|
| Armed camera **definitions** (soft, data‑only) | **12** | `aldirectorswitchermodel.h:20` (reuse) |
| Simultaneous **producers** (hard, engine) | **3** = `MAX_CAPTURES` | `llprismlens.h:28` (PROVES) |
| Gate **producer footprint** while active | **≤ 2** (program + pre‑warm) | §C/§D (IMPLIES) |

**Why this doesn't raise render cost:** an armed‑but‑idle definition has **no producer** —
it is pure `CameraSettings` bytes, so it cannot be selected by `chooseRenderSlot`
(`llprismlens.cpp:3085`) and cannot be admitted (no slot, no `mAnyDisplayVisible`). Render
cost is bounded by the ≤2 materialized producers, and by §1.3 only **one** of those renders
per frame.

> **Reviewer decision to force:** pick option 1 or 2. This doc specifies option 2 downstream;
> option 1 is a strict subset (drop the definition snapshot + producer pool; arm ids
> directly). The rest of A/C/D/G/H is identical.

### C. The switch / on‑air routing (reuse, don't duplicate)

Each frame, in a new `PrismLensRegistry::updateGate(now)` called at the **top of
`renderAuxiliaryView()` before the prepare loop** (`llprismlens.cpp:5772`, i.e. before
`:5774`):

1. **Advance the brain.** Build `ALDirectorSwitcherModel::Config` from `GateSettings`
   (`mAuto = (mMode==AUTO_CYCLE)`, `mSequence = true` for the explicit cam1→cam2→cam3 order,
   `mIntervalSeconds`, `mJitterSeconds`, `mEnabled[i] = i < mArmed.size()`), then call
   `Frame f = mController.update(now, cfg)` (`aldirectorswitchermodel.h:56`). `f.mSlot` is
   the on‑air arm index; `f.mCut` marks a boundary this frame; `f.mBoundary` is the next
   boundary time (used by §D).
2. **Materialize on‑air.** Ensure `mProgramSlot` is a reserved virtual CAMERA_FEED capture
   whose `mCamera` equals `mArmed[f.mSlot].mCamera` (write directly to
   `mLenses[mProgramSlot]`, bump `mRuntimeRevision` only — no config churn, mirroring the
   runtime‑only transform writes at `llprismlens.cpp:3453`). On a hard cut, **swap roles**
   `mProgramSlot ↔ mWarmSlot` when the warm slot already holds the incoming camera (§D) so no
   reprogram/allocation happens at the cut instant.
3. **Route monitors.** For every occupied display with `mGateSubscribed == true`, set
   `display.mCaptureSlot = mProgramSlot` and `display.mCaptureGeneration =
   mLenses[mProgramSlot].mHandle.mGeneration`.
4. **Let the engine do the rest.** The prepare loop then naturally sets
   `mAnyDisplayVisible = true` on the on‑air producer (a visible subscribed display now binds
   it, `llprismlens.cpp:2706-2718`) and `false` on every other camera (no visible display
   binds it → `llprismlens.cpp:2697` stays false). `updateCadenceEntitlements`
   (`llprismlens.cpp:2990`) zeroes the idle cameras; `chooseRenderSlot`
   (`llprismlens.cpp:3089`) renders only the watched one(s).

**The Gate sets/propagates "watched" purely by owning `mCaptureSlot` on subscribed
displays.** It writes **no** Hz, **no** entitlement, and calls **nothing** in the
water‑filling / selection math. That is the "reuse, don't duplicate" contract.

### D. Pre‑warm (the one sanctioned engine touch)

**Problem (PROVES).** A cut to a suppressed camera shows blank/stale: a suppressed capture
released its output (`suppressOutput`, `llprismlens.cpp:4808-4816`), and an unwatched camera
is never even sized (early `return false` at `llprismlens.cpp:2792-2795`).

**Design.** `mPrewarmFrames` (default 3; **INFERS**, tune 2–5) before an auto‑cut, the Gate
brings the **next** camera into the watched set so it renders a fresh frame in time.

- **Next‑index for pre‑warm.** With `mSequence = true` the next on‑air index is deterministic
  (the next enabled arm index), so pre‑warm has a definite target. `f.mBoundary` gives the
  cut time; enter the warm window when `now >= f.mBoundary − mPrewarmFrames * frameDT`
  (`frameDT` from `gFPSClamped`, already read at `llprismlens.cpp:2955`). **INFERS**: for
  random mode (`mSequence=false`) pre‑warm is skipped in v1 (or the model must expose a
  `peekNextSlot` — out of scope for v1).
- **Sanctioned minimal hook.** Give `PrismInstance` two fields:
  `bool mGateWarm = false; U32 mGateWarmW = 0, mGateWarmH = 0;`. In `updateGate`, for the
  warm slot set `mGateWarm = true` and seed `mGateWarmW/H` from the **on‑air producer's
  current output size** (`mOutputWidth/Height`, `llprismlens.cpp:1600-1601`) — the warm
  camera inherits the same monitors at the cut, so that is the correct target size. Then add
  **one** guarded block at the top of `prepareCameraCapture`, immediately before the
  `if (!capture.mAnyDisplayVisible) return false;` gate (`llprismlens.cpp:2792`):

  ```
  if (!capture.mAnyDisplayVisible && capture.mGateWarm &&
      capture.mGateWarmW > 0 && capture.mGateWarmH > 0)
  {
      capture.mAnyDisplayVisible = true;             // admit as if watched
      required_width  = llmax(required_width,  capture.mGateWarmW);
      required_height = llmax(required_height, capture.mGateWarmH);
  }
  ```

  Everything downstream — target sizing (`:2797-2809`), `mFrame.mPrepared`, the water‑fill,
  `chooseRenderSlot` — is **byte‑identical**. This is *layering an admission input*, not
  rewriting the scheduler. It is the **only** edit inside the admission/prepare path this
  whole feature makes, and it is a strict no‑op when no gate is warming (`mGateWarm` defaults
  false).
- **Cost (quantified).** During the pre‑warm window, at most **2** cameras are watched
  (on‑air + warm). By §1.3 the engine still renders **one auxiliary slot per frame**
  (`llprismlens.cpp:5784`), round‑robin (`:3085`). So pre‑warm does **not** double per‑frame
  GPU; it makes the on‑air and warm cameras *share* the single aux slot for
  `mPrewarmFrames` frames — the on‑air camera's effective cadence roughly halves for that
  brief window, and one extra scratch target of `mGateWarmW×H` is transiently live. Steady
  state is exactly **1** rendering camera, identical to a single normal camera today.
- **At the cut:** monitors re‑point to the (already fresh) warm slot; roles swap; no frame
  stalls because the warm output was produced during the window (`CURRENT`/`HELD`, not
  `SUPPRESSED`).
- **MANUAL pre‑warm.** Warm the **preview** camera (`mPreviewArmIndex`) whenever it is set
  and differs from program, so an operator's TAKE is always fresh. This reuses the identical
  `mGateWarm` seed on the preview's reserved slot.

### E. Transitions

- **Default: HARD CUT.** Instant `mCaptureSlot` re‑point at the cut frame. Zero extra cost
  outside the pre‑warm window. This is the honest v1 default and the only transition that
  fits the architecture cleanly.
- **CROSSFADE: defer (recommendation).** A true crossfade needs *both* cameras' retained
  outputs fresh **on the same composite frame**. But the engine renders **one** auxiliary
  slot per frame (`llprismlens.cpp:5784`) — both feeds can be simultaneously *composited*
  (they are just retained textures sampled by the display shader) but they cannot both be
  *refreshed* on the same frame; during a fade both would be running at half cadence
  (§1.3) and the outgoing feed goes increasingly stale. A dissolve between two
  alternating‑frame feeds is achievable but is a **shader/composite‑path change**
  (`getCompositeStates`/`getAuxCompositeStates`, `llprismlens.h:616-627`, and
  `prismLensF.glsl`) — explicitly OFF‑LIMITS (§OFF‑LIMITS). **Recommend deferring crossfade
  to a follow‑up**; if pursued, it is a display‑composite feature (blend two `mCaptureSlot`
  sources with a time‑varying weight), *not* a Gate‑admission feature, and must be designed
  against the composite pass, not here.

### F. Auto‑cycle timing

- **Recommendation: global interval with optional per‑camera dwell override.** The reused
  model already carries `mIntervalSeconds` + `mJitterSeconds` (`aldirectorswitchermodel.h:31-32`)
  with sane bounds `MIN_INTERVAL_SECONDS = 0.1 … MAX_INTERVAL_SECONDS = 3600.0`
  (`:22-23`) and `sanitizeConfig` (`:47`). Use `mIntervalSeconds` as the global interval
  (recommended UI range **0.5–120 s**, default 8 s = `DEFAULT_INTERVAL_SECONDS`, `:21`).
  **INFERS**: per‑camera dwell is *not* natively supported by the fixed‑interval model, so
  for v1 either (a) ship global‑only (simplest, fully reuses the model), or (b) add a thin
  per‑arm `dwellSeconds` in `GateSettings` and, when non‑zero, drive cuts from the Gate's own
  boundary math instead of the model's uniform interval — **recommend (a) for v1**, (b) as a
  follow‑up, to keep the proven model authoritative.
- **The tick runs on the existing per‑frame Prism update.** `updateGate(now)` is called from
  the top of `renderAuxiliaryView()` (`llprismlens.cpp:5729`, before `:5774`) using
  `LLTimer::getTotalSeconds()` — the same real clock `updateCadenceEntitlements` uses
  (`llprismlens.cpp:3070`) and analogous to `ALDirectorSwitcher::tick(presentation_time)`
  (`aldirectorswitcher.h:62`). No new timer thread; large frame gaps are absorbed by the
  model's "advance to final elapsed event, at most one cut" guarantee
  (`aldirectorswitchermodel.h:54-55`).

### G. UI (Prism Manager)

Add a **Gate** surface to `floater_prism_manager.xml`. Two viable placements
(**INFERS** — pick in review):

- **Preferred:** a fourth tab `gate_tab` in `detail_tabs` (`floater_prism_manager.xml:166`),
  cloning the tab‑panel + `scroll_container`→document idiom already used by `capture_tab`
  (`…:185-203`). No floater‑rect change needed (tabs are internal).
- **Alternative:** a compact Gate strip below `captures_panel`; this needs floater‑height
  reflow (900×844 → taller) and is more disruptive — **not recommended**.

Gate‑tab contents (mirroring existing widget kinds so no new control types are introduced):

- **Armed list** — a `scroll_list` of armed cameras with order, on‑air indicator, and
  enabled checkbox; **Arm selected** / **Disarm** buttons that add/remove the
  currently‑selected `capture_list` row into `GateSettings.mArmed` (reuse `mSelectedCapture`,
  `llfloaterprismmanager.cpp:1378`). Row value = arm index or capture UUID (mirror
  `:906`/`:981`).
- **Mode toggle** — `gate_mode` two‑item combo Manual/Auto, cloned from `rate_mode`
  (`…:459-462`, `mRateModeCombo` `cpp:399`). Commit `onGateModeChanged()` → new
  `LLPrismLens::setGateSettings(...)`.
- **Interval / jitter / prewarm** — spinners cloned from `target_fps`
  (`…:463-476`) / `bone_pov_smoothing` (`…:411`). Enable only in Auto (reuse
  `setActionState`, `cpp:266-273`).
- **TAKE / PROGRAM (manual)** — a **Preview** selector (arm index) + a big **TAKE** button
  that calls `mController.manualPunch` via `LLPrismLens::gateTake()`; an **on‑air indicator**
  (label/emblem) bound from the runtime snapshot's on‑air arm index. Manual cut is
  instantaneous; preview is pre‑warmed (§D).
- **"Source: Gate" for a display** — on the Displays tab, add a checkbox/combo per display:
  *Fixed capture* (today's behavior) vs *Gate* (sets `mGateSubscribed`). Reuses the display
  editor commit path (`setDisplaySettings`, `cpp:1902`) — extend `DisplaySettings` /
  `PrismDisplay` with the flag, or carry it as a dedicated `setDisplayGateSubscribed(handle,
  bool)`.
- **Mini resets** — `image_overlay="Refresh_Off"` 18×18 buttons (interval/jitter/prewarm →
  defaults), cloning `bone_pov_offset_reset` (`…:402`, handler `cpp:524-571`).
- **Refresh discipline** — every Gate mutation calls `invalidateRegistrySnapshot()`
  (`cpp:2110`); on‑air/state readouts refresh through the existing revision poll
  (`cpp:678-692`). Add a gate revision to the snapshot (or reuse `mRuntimeRevision`) so the
  on‑air indicator updates within one poll tick (0.25 s, `cpp:36`).

### H. Persistence

- **Additive, no version bump.** Emit a new top‑level `prism_gates` array from
  `sceneData()` (`llprismlens.cpp:3856`, alongside the two existing arrays) and parse it
  *optionally* in `applySceneData()` (an absent array → empty gate, mirroring `bone_pov`
  optional‑read, `llprismlens.cpp:3886-3895`). In `LLFloaterDirector::saveScene` add one
  lift line `scene["prism_gates"] = prism["prism_gates"];` next to
  `llfloaterdirector.cpp:887-888`. **`SCENE_VERSION` stays 3** (`llfloaterdirector.cpp:86`).
- **Per‑gate keys (INFERS):** `gate_id`, `active`, `mode` ("manual"|"auto_cycle"),
  `interval_seconds`, `jitter_seconds`, `prewarm_frames`, `armed` (ordered array of
  `capture_id` [+ optional per‑arm `camera` snapshot for §B option 2], + `enabled` bool),
  `program_index`. Validate that each armed `capture_id` resolves to a parsed capture (reuse
  the display cross‑ref idiom, `llprismlens.cpp:4405-4406`); a dangling arm is dropped, not a
  parse failure (keep the gate best‑effort so a partially‑edited scene still loads).
- **Per‑display key:** `gate_source` (bool). Written on subscribed displays; optional on
  read (absent → false → today's fixed binding).
- **Old scene → new binary:** no `prism_gates` → empty inactive gate; all displays fixed.
  Identical to pre‑gate behavior.
- **New scene → old binary:** `prism_gates` and `gate_source` are ignored (allow‑listed load,
  `llfloaterdirector.cpp:1041`; unknown keys tolerated, `llprismlens.cpp:4075-4080`). A gate
  monitor persists with `capture_id` set to its **last on‑air** capture (write the resolved
  on‑air capture id, not null), so an old viewer shows a **static last‑on‑air feed** instead
  of an unbound/blank display — a graceful, well‑defined degradation.

---

## §TEST PLAN — pure, non‑tautological, regression‑failing

All of the following are **pure** (no viewer singleton, no GL), mirroring
`aldirectorswitchermodel_test.cpp` and `alvcambonepov_test.cpp` and the pure precedent
`normalizeBonePovJointSelection` (`llprismlens.h:150`). Put Gate‑specific pure helpers behind
free functions so they are testable without the registry. New test file (INFERS):
`indra/newview/tests/llprismgate_test.cpp` (registered in the newview test build the same way
`aldirectorswitchermodel_test.cpp` is).

1. **Cycle‑advance / next‑index (reuse the model, but assert the Gate's mapping).**
   Given armed set `{A,B,C}`, `mSequence=true`, interval `T`, seed fixed:
   - `update(t0)`→ on‑air A; `update(t0+T−ε)` → still A, `mCut=false`;
     `update(t0+T+ε)` → B, `mCut=true`. Wrap C→A after 3T.
   - **Single armed camera:** never cuts (`mCut` stays false across many T). Fails if a
     regression cuts to self.
   - **Empty armed set:** `updateGate` is a no‑op; no producer reserved; no monitor rerouted.
   - **Large time jump** `update(t0+100T)`: exactly one cut, on‑air = deterministic
     final‑event slot (asserts the model's `:54-55` contract is honored by the Gate wrapper).
   - **Disarm of the on‑air camera:** on‑air clamps to a still‑armed index (no −1 on‑air
     while ≥1 armed). Fails if disarm leaves a dangling program slot.

2. **Pre‑warm window computation.**
   Given `mBoundary`, `mPrewarmFrames`, `frameDT`:
   - `warmActive(now)` is false for `now < mBoundary − N·dt`, true within the window, and the
     warm target index equals the deterministic next enabled arm. Boundary case: exactly at
     `mBoundary − N·dt` → true (inclusive).
   - Warm size seed equals the on‑air producer's `mOutputWidth/Height` at window entry; a
     zero on‑air size yields **no** warm admission (guards the `mGateWarmW>0` gate,
     mirroring `llprismlens.cpp` §D block). Fails if warm admits with a 0×0 target.
   - MANUAL: `mPreviewArmIndex == mProgramArmIndex` → no warm; distinct → warm the preview.

3. **Manual‑take state machine.**
   States {PROGRAM, PREVIEW}. `take()` with:
   - no preview set → no cut, `cutSerial` unchanged.
   - preview == program → no cut.
   - preview ≠ program → exactly one cut, program←preview, interval restarts
     (assert via the model's `manualPunch` restart, `aldirectorswitchermodel.h:58-60`).
   - Auto→Manual transition freezes on the current on‑air index (no spurious cut on mode
     flip).

4. **Old↔new persistence mapping (round‑trip + cross‑version).**
   - Round‑trip: `GateSettings` → LLSD → parse → **equal** (armed order, mode, interval,
     jitter, prewarm, program_index, per‑arm enabled).
   - Old scene (no `prism_gates`) → empty inactive gate, zero subscribed displays.
   - New scene parsed by the *display* validator: a `gate_source` display whose `capture_id`
     is its last‑on‑air capture still validates (asserts the graceful‑degradation write in
     §H doesn't trip `llprismlens.cpp:4405-4406`).
   - Dangling armed `capture_id` (capture absent) → arm dropped, scene still loads
     (best‑effort), rather than a hard parse fail.

---

## §FILE / FUNCTION CHECKLIST

**Create**
- `indra/newview/tests/llprismgate_test.cpp` — pure logic tests (§TEST PLAN). *(Only if the
  Gate's pure helpers are factored out as free functions; otherwise fold into an existing
  suite.)*

**Modify**
- `indra/newview/llprismlens.h` — add `EGateMode`, `GateArmedCamera`, `GateSettings`; declare
  `setGateSettings()`, `gateTake()`, `gateArm()/gateDisarm()`,
  `setDisplayGateSubscribed()`, and a `gateSnapshot()`; declare pure helpers for §TEST PLAN.
- `indra/newview/llprismlens.cpp` —
  - `PrismInstance` (`:1583`): add `mGateWarm`, `mGateWarmW`, `mGateWarmH`.
  - `PrismDisplay` (`:1616`): add `mGateSubscribed`.
  - `PrismLensRegistry`: add `PrismGateRuntime mGate` + `GateSettings`; add
    `updateGate(now)`, producer reserve/reprogram/swap, monitor routing.
  - `prepareCameraCapture` (`:2690`): the single guarded warm block **before** `:2792`.
  - `renderAuxiliaryView` (`:5729`): call `registry.updateGate(now)` at the top, **before**
    the prepare loop (`:5774`).
  - `sceneData` (`:3852`) / `applySceneData` (`:4033`): emit/parse `prism_gates` + display
    `gate_source` (optional‑on‑read).
- `indra/newview/llfloaterdirector.cpp` — `saveScene` (near `:887-888`): lift
  `scene["prism_gates"]`. (`loadScene` needs no change — `applySceneData` already receives
  the whole scene map, `:1005`.)
- `indra/newview/llfloaterprismmanager.{h,cpp}` + `skins/default/xui/en/floater_prism_manager.xml`
  — the Gate tab/section, mutation handlers (mirror `onCommitRateSettings` `:1738`,
  `onRemoveCapture` confirm `:1462`), and `invalidateRegistrySnapshot()` on every mutation.

**Explicitly NOT touched**
- `updateCadenceEntitlements` water‑fill math (`:3005-3031`) and `chooseRenderSlot` selection
  (`:3068-3126`) — the Gate only sets/propagates `mAnyDisplayVisible` inputs.
- The render consume path: `renderAuxiliaryView` render body (`:5790+`), `getCompositeStates`
  / `getAuxCompositeStates` (`llprismlens.h:616-627`), and every `.glsl` shader.
- `ALDirectorSwitcherModel` / `ALDirectorSwitcher` internals — **reused**, not edited.
- `lldirectorcast`, Bone‑POV / VCam joint code (`ALVCamBonePov`, `CameraSettings::mBonePov`),
  `SCENE_VERSION` (`llfloaterdirector.cpp:86`).

---

## §RISKS (highest first)

1. **Breaking the demand‑driven admission / non‑gate displays.** The Gate must never write
   Hz/entitlement or alter selection. Mitigation: the *only* admission‑path edit is the
   `mGateWarm` block (§D), a strict no‑op when idle; non‑subscribed displays are skipped by
   `mGateSubscribed == false`, so their fixed binding is byte‑identical. Guard test: with the
   gate inactive, a golden capture/display scene renders identically (revision + on‑air
   invariants unchanged).
2. **Pre‑warm cost / target churn.** Warm adds a transient second scratch target and halves
   on‑air cadence for the window (§D). Mitigation: keep `mPrewarmFrames` small (2–5); reserve
   the warm slot rather than allocate/free (allocate/free calls
   `releasePrismLensBuffers`, which frees the **whole** pool, `llprismlens.cpp:4811/4841` —
   a per‑cut hitch). Risk if a reviewer instead chooses lazy warm‑slot allocation: quantify
   the pool‑churn hitch before accepting it.
3. **Definition‑vs‑producer cap decision (§B).** Choosing option 1 (arm‑existing, cap 3)
   silently defeats the switchboard's purpose; option 2 (12 definitions, 2 producers) adds a
   reserve/reprogram pool. This decision must be made explicitly in review; the rest of the
   design is stable across it.
4. **The switch stalling a frame.** A cut to a cold slot with no fresh frame would show a
   `SUPPRESSED`/blank feed. Mitigation: pre‑warm guarantees the incoming slot is
   `CURRENT`/`HELD` at the cut; role‑swap avoids allocation at the cut instant.
5. **Producer‑slot starvation.** An active gate consumes up to 2 of 3 `MAX_CAPTURES` slots,
   leaving ≥1 for non‑gate captures. Enforce single‑active‑gate (v1) and surface a clear "at
   capacity" reason (reuse `ActionStatus`, `llprismlens.h:313-318`).
6. **Generation staleness in routing.** Re‑pointing `mCaptureSlot` must also set
   `mCaptureGeneration` to the producer's current generation, or the display is filtered out
   by the generation guard (`llprismlens.cpp:2708-2712`). Test: reprogram bumps generation ⇒
   routing updates it in the same `updateGate` pass.

---

## §OFF‑LIMITS (do not touch)

- The **render consume path** and **shaders**: `renderAuxiliaryView` render body
  (`llprismlens.cpp:5790+`), `getCompositeStates`/`getAuxCompositeStates`
  (`llprismlens.h:616-627`), `prismLensF.glsl` and all Prism GLSL.
- The **admission/scheduler internals**: `updateCadenceEntitlements` water‑fill
  (`llprismlens.cpp:3005-3031`) and `chooseRenderSlot` (`:3068-3126`). The Gate **layers on
  `mAnyDisplayVisible`**; it must not rewrite the scheduler.
- The **capture/display engine internals** beyond the two additive struct fields and the one
  `mGateWarm` prepare block named in the checklist.
- `lldirectorcast`.
- The **Bone‑POV / VCam‑joint** work (`ALVCamBonePov`, `CameraSettings::mBonePov`,
  `normalizeBonePovJointSelection`).
- **`SCENE_VERSION`** (`llfloaterdirector.cpp:86`) — persistence is additive; no bump.
- `ALDirectorSwitcherModel` / `ALDirectorSwitcher` are **reused as‑is**, not modified.

---

## §Push‑back on the request

- **"FPS‑cheap / only the on‑air camera renders" is already true of the engine** — the Gate
  *preserves* an existing property (`llprismlens.cpp:2990`), it doesn't create a new saving.
  Framing that implies the Gate *makes* rendering cheaper is inaccurate; its value is
  **routing and orchestration**, and its risk is *not regressing* that cheapness. I'd state
  the goal as "keep the existing zero‑cost‑for‑idle guarantee while adding switching," not
  "make it cheaper."
- **Crossfade is not cheap here.** The architecture renders exactly one auxiliary slot per
  frame (`llprismlens.cpp:5784`); a genuine crossfade cannot refresh both feeds on one frame
  and is a composite/shader feature, which is OFF‑LIMITS. I'd **cut crossfade from v1** and
  scope it as a separate composite‑pass project, not a Gate mode.
- **Per‑camera dwell fights the proven model.** The reused, unit‑tested scheduler is
  fixed‑interval (`aldirectorswitchermodel.h:31`). I'd ship **global interval only** in v1
  and treat per‑camera dwell as a follow‑up, rather than forking cut‑timing away from the
  tested model on day one.
- **"Multiple gates" is impractical under `MAX_CAPTURES = 3`.** I'd model the data as an
  array for forward‑compat but **ship one active gate**, and say so plainly, rather than
  implying several independent switchboards can run at once.
- **Don't reinvent the switcher.** The request describes manual take + auto‑cycle + cut
  boundaries as if new; the codebase already ships and tests exactly that brain
  (`ALDirectorSwitcherModel`). I'd make "reuse `ALDirectorSwitcherModel::Controller`" a
  hard requirement of the brief, not an option.
```
