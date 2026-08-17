# VCAM GATE — Design Document (hardened, implementation-ready)

**Feature:** "Vcam Gate" — a live-TV video switchboard / vision mixer for the prim-free
virtual cameras of the Prism engine. Many cameras **arm** to one Gate; the Gate routes
**one** output to the subscribed monitors at a time (manual **TAKE** + **auto-cycle**), like a
security-camera switcher. The point: many cameras can be armed with **no FPS cost**, because
only the currently-watched source actually renders.

**Status:** DESIGN ONLY — no source is modified by this document. Written as a Codex
implementation brief → adversarial review → build.
**Repo:** `I:\alchemy-machinima` (Alchemy-Machinima Second Life viewer fork).

Every claim about *current* code is cited `file:line`. Design claims are labelled:

- **PROVES** — verified directly in the code cited (re-verified 2026-08-17 against the live tree).
- **IMPLIES** — strong inference from cited code, not literally spelled out.
- **INFERS** — an assumption a reviewer/build must confirm.

Line numbers are anchors; a Codex run should re-resolve by symbol if a file has drifted.

> **This revision corrects three load-bearing errors in the prior draft.** See
> **§CORRECTIONS** for the full list; the top three:
> 1. **Pre-warm relied on scheduler data that does not exist.** `ALDirectorSwitcherModel::Controller`
>    exposes **no** next-slot peek, and `Frame::mBoundary` is the boundary of the event that
>    *just fired* (in the past), **not** the next cut time. The prior §C/§D used `mBoundary` as
>    the upcoming cut and assumed a deterministic "next index" was readable from the model.
>    Both are false against the code. Pre-warm is re-specified as **gate-owned sequence math**
>    (jitter forced to 0, sequence mode, next index computed from the Gate's own ordered arm
>    list) — no model change required. See §D.
> 2. **Persistence would hard-fail the whole scene.** A gate-subscribed monitor written with a
>    `capture_id` that does not resolve to a persisted capture trips the display cross-ref at
>    `llprismlens.cpp:4405-4406`, which `return fail(...)`s the **entire** `applySceneData`.
>    The Gate's **program** producer must therefore be a *real, serialized* virtual capture, not
>    a hidden runtime-only slot. Only the transient **warm** slot stays unserialized. See §H.
> 3. **"Reuse the switcher verbatim" conflicts with pre-warm, and continuous preview-warm is not
>    free.** Verdict and cadence cost are made concrete in §F and §D.

---

## 0. One-paragraph thesis

The Prism engine **already renders only the cameras that are being watched** and nothing else —
a capture with no visible display bound to it is dropped to 0 Hz before any GPU work
(`llprismlens.cpp:2990`). The Gate does **not** add a scheduler render pass or a shader. It is a
thin *orchestrator* that, each frame, (a) points the subscribed monitor displays at whichever
armed camera is currently **on-air**, and (b) for a short pre-warm window, nudges the *next*
camera into the "watched" set. The existing demand-driven admission engine does everything
else. The Gate's switching *brain* — manual take + auto-cycle + cut boundaries — is reused from
the already-shipped, adversarially-tested `ALDirectorSwitcherModel::Controller`
(`aldirectorswitchermodel.h:49`), the same deterministic scheduler the Director camera switcher
drives (`aldirectorswitcher.h:124`) — **but it is wrapped, not extended: the Gate reads the
Controller's returned `Frame` for the current cut and computes its own next-index/next-boundary
for pre-warm, because the Controller exposes neither.**

---

## STEP 1 — The Prism engine (substrate the Gate sits on)

### 1.1 Captures vs Displays: the two bounded arrays

- **PROVES.** `constexpr U32 MAX_CAPTURES = 3;` (`llprismlens.h:28`),
  `constexpr U32 MAX_DISPLAY_BINDINGS = 16;` (`llprismlens.h:29`), `MAX_LENSES` aliases
  `MAX_CAPTURES` (`llprismlens.h:30`). **`MAX_CAPTURES == 3` is the hard cap on simultaneous
  producers** and the single most important number in this design.
- **PROVES.** Runtime capture = `struct PrismInstance` (`llprismlens.cpp:1583`); runtime display =
  `struct PrismDisplay` (`llprismlens.cpp:1616`). Serializable shapes are `CaptureDefinition`
  (`llprismlens.h:424`) and `DisplayDefinition` (`llprismlens.h:436`); `RegistrySnapshot` carries
  fixed arrays of both (`llprismlens.h:452-453`).
- **PROVES.** A capture's mode is `PrismInstance::mMode` (`llprismlens.cpp:1587`, enum
  `llprismlens.h:42`). Camera source id is `PrismInstance::mCameraObjectId` (`llprismlens.cpp:1592`).
- **PROVES.** The prim-free virtual-camera transform lives on `CameraSettings`: `mVirtual` /
  `mVirtualPos` / `mVirtualRot` (`llprismlens.h:242-244`), in **AGENT** space, forward = local −Z,
  up = local +Y (`llprismlens.h:236-244`). A CAMERA_FEED capture with `mVirtual == true` derives
  eye/orientation from that transform and ignores `mCameraObjectId` (`llprismlens.cpp:2735-2744`).
  `addVirtualCamera()` allocates such a capture (`llprismlens.h:536`, `llprismlens.cpp:1839`); it
  checks `count() >= MAX_CAPTURES` (`:1843`) and takes a `freeCaptureSlot()` (`:1858`). The additive
  Bone-POV driver is `CameraSettings::mBonePov` (`llprismlens.h:248`, struct `llprismlens.h:188`).

### 1.2 How a DISPLAY binds to a CAPTURE (the reference the Gate rewrites)

- **PROVES.** At runtime a display references its capture by **slot index**:
  `PrismDisplay::mCaptureSlot` (`llprismlens.cpp:1620`) plus a generation guard
  `PrismDisplay::mCaptureGeneration` (`llprismlens.cpp:1621`). Every gather of "the displays of
  slot *N*" filters on `display.mCaptureSlot == slot && display.mCaptureGeneration ==
  capture.mHandle.mGeneration` (`llprismlens.cpp:2708-2712`, `2231-2232`, `2471-2472`,
  `6204-6205`, `6461-6462`).
- **PROVES.** In persistence, a display references its capture by **UUID**:
  `item["capture_id"] = mLenses[display.mCaptureSlot].mHandle.mId` (`llprismlens.cpp:3962`); on
  load the UUID must be non-null **and** resolve to a parsed capture or the whole parse fails
  (`llprismlens.cpp:4405-4406`), then it is resolved back to a slot at commit
  (`llprismlens.cpp:4575-4585`).
- **IMPLIES.** Because the runtime binding is a single mutable field (`mCaptureSlot`) the engine
  re-reads every frame, **the Gate routes a monitor by writing that field** to the on-air
  producer's slot (and matching `mCaptureGeneration`). No new binding type is needed for a hard cut.

### 1.3 The demand-driven admission scheduler (why "only watched cameras render" is free)

Per-frame driver: `renderAuxiliaryView()` (`llprismlens.cpp:5729`), whose **sole** caller is the
main display loop (`llviewerdisplay.cpp:919` — **PROVES**, one call per presented frame). Shape
(**PROVES**, `llprismlens.cpp:5772-5788`):

1. `for slot in [0,MAX_LENSES): registry.prepare(slot, …)` — prepare **all** slots (`:5774-5781`).
2. `render_slot = registry.chooseRenderSlot()` — pick **at most one** slot (`:5784`).
3. Render exactly that one slot, or return if none (`:5785-5788`).

Key mechanisms:

- **PROVES — the watched flag.** `PrismInstance::mAnyDisplayVisible` (`llprismlens.cpp:1598`) is
  recomputed every frame inside `prepareCameraCapture()`: set false at entry (`:2697`), set true
  iff at least one *occupied, generation-matched* display bound to this slot passes its
  visibility test (`:2706-2718`). The Surface-Lens path sets it the same way (`:2463`).
- **PROVES — the admission gate (the hook the Gate reuses).** `updateCadenceEntitlements()` at
  `llprismlens.cpp:2990`: `if (!capture.mOccupied || !capture.mAnyDisplayVisible ||
  !capture.mFrame.mPrepared || now < capture.mRetryAfterTime) { mRequestedHz = 0; mEntitlementHz
  = 0; … continue; }` (`:2990-2997`). **An unwatched capture gets zero Hz and is never rendered.**
  The identical predicate re-gates selection in `chooseRenderSlot()` (`:3089-3093`).
- **PROVES — target sizing depends on being watched.** `prepareCameraCapture()` returns early
  with `false` and never sizes a render target if `!mAnyDisplayVisible` (`:2792-2795`); the
  canonical size is otherwise derived from the bound displays' visible footprints
  (`required_width/height`, `:2704-2717`, `:2799-2819`). *(This is why pre-warm needs a seed
  size — §D.)*
- **PROVES — one render per frame, round-robin.** `chooseRenderSlot()` scans
  `(mNextRenderSlot + offset) % MAX_CAPTURES` (`:3085-3087`) and returns a single best slot;
  `renderAuxiliaryView` renders only that one (`:5784`). **Two watched cameras do not double
  per-frame GPU cost — they interleave frames**, sharing the single aux render slot. This is
  load-bearing for the pre-warm cost claim (§D) and the crossfade push-back (§E).
- **PROVES — fair sharing.** More than one watched capture → bounded max-min water filling splits
  the global budget (`:3005-3031`); the budget is `prismCaptureBudgetHz()` scaled by the adaptive
  cadence factor (`:2954-2969`).
- **PROVES — the state machine.** `EOutputState { EMPTY, CURRENT, HELD, SUPPRESSED }`
  (`llprismlens.h:91-97`). `CURRENT` vs `HELD` is decided by whether the last render was this
  frame (`:2984-2986`). `suppressOutput(slot)` (`:4805-4822`) releases the slot's GPU output
  **and the whole pooled scratch buffer set** (`gPipeline.releasePrismLensBuffers()`, `:4811`,
  comment `:4809-4810`) and stamps `SUPPRESSED` (`:4816`).
- **PROVES — supporting counters.** `displayCount()` (`:4784`), `mDisplayCount` per capture
  (`:1597`), revisions bumped on mutation (`++mRuntimeRevision` `:4821`, `++mRevision` `:4599`).
  `PerformanceSnapshot::mRevision` (`llprismlens.h:281`) is the UI-poll signature.

**Design consequence (IMPLIES).** The complete recipe for "make camera X render and nothing else"
already exists and runs every frame: *ensure exactly one visible display is bound (`mCaptureSlot`)
to X, and no visible display is bound to any other camera.* The Gate's whole job is to keep that
invariant pointed at the on-air camera and briefly extend it to the pre-warm camera. **The Gate
layers on `mAnyDisplayVisible`; it must not touch the water-filling math or `chooseRenderSlot`.**

### 1.4 Runtime-only transform writes (no config churn)

- **PROVES.** Virtual eye/orientation consumed in `prepareCameraCapture()` (`:2735-2744`); aux view
  produced in `renderAuxiliaryView()` (`:5729`, body from `:5790`). Runtime-only writes that do
  **not** bump the configuration revision: `setVirtualCameraTransform()` (`llprismlens.h:561`) and
  `setVirtualCameraPosition()` (`llprismlens.h:568`). **The Gate reprograms its program producer's
  `mCamera` through this same runtime-only discipline** (bump `mRuntimeRevision`, not `mRevision`).

### 1.5 Persistence (the round-trip the Gate rides)

- **PROVES.** `sceneData()` (`llprismlens.cpp:3852`) emits a map with two top-level arrays,
  `prism_captures` and `prism_displays` (`:3855-3856`). It **iterates `mLenses` and serializes
  every `mOccupied` capture** (`:3857-3859`) — i.e. *any* real occupied slot is written, including
  a displayless one. `applySceneData()` (`:4033`) parses/validates atomically and commits only
  after full cross-reference validation.
- **PROVES — additive precedent, no version bump.** `bone_pov` is written unconditionally and read
  *optionally* (`:3911-3949` emit; optional-on-read); `screen_effects`, `virtual`, `virtual_pos`,
  `virtual_rot`, `width`, `height` follow the same optional-on-read discipline. None bumped a
  scene version.
- **PROVES — the Director scene wrapper.** `LLFloaterDirector::saveScene` sets
  `scene["version"] = SCENE_VERSION;` where `constexpr S32 SCENE_VERSION = 3;`
  (`llfloaterdirector.cpp:86`, `:885`) and **lifts** the two Prism arrays to the top level:
  `scene["prism_captures"] = prism["prism_captures"]; scene["prism_displays"] =
  prism["prism_displays"];` (`llfloaterdirector.cpp:887-888`). `loadScene` passes the *whole* scene
  map to `applySceneData` (`:1005`).
- **PROVES — forward/backward tolerance.** Load is allow-listed; unknown top-level keys are
  ignored. `applySceneData`'s guard requires only that `prism_captures`/`prism_displays` exist as
  arrays (`:4075-4080`). Version handling migrates/tolerates rather than rejecting (`:989-991`,
  Director `loadScene`).
- **PROVES — load caps + cross-ref.** `captures_data.size() > MAX_CAPTURES ||
  displays_data.size() > MAX_DISPLAY_BINDINGS` fails the parse (`:4083-4087`); capture ids must be
  non-null and unique (`:4105-4108`); **a display whose `capture_id` is null or unresolved fails
  the whole parse** (`:4405-4406`). *(This last is the constraint the prior §H violated — see §H.)*

**Design consequence (IMPLIES).** A new top-level `prism_gates` array rides the identical
round-trip: emit from `sceneData()`, parse *optionally* in `applySceneData()`, add one lift line
in `saveScene`. Old viewers ignore it. **No `SCENE_VERSION` bump** (mirrors `bone_pov`).

### 1.6 The Prism Manager UI (where the Gate UI lives)

- **PROVES.** Floater `floater_prism_manager.xml` — `name="prism_manager"`, `width="900"
  height="844"`, resizable (`:2-13`). Fixed `summary_panel` (`:16-71`) and `captures_panel`
  (`:73-164`, holds `capture_list`) precede `tab_container name="detail_tabs"` (`:166-174`) with
  **exactly three** tabs today: `capture_tab` (`:175-184`), `displays_tab` (`:532-541`),
  `performance_tab` (`:693-702`). A new gate tab is a 4th `<panel name="gate_tab">` sibling
  inserted before `</tab_container>` at `:820`. Bottom `status_text` at `:822-833`.
- **PROVES.** `scroll_list name="capture_list"` (single-select, `:93-113`), bound `mCaptureList`
  (`llfloaterprismmanager.h:124`), rebuilt in `rebuildCaptureList()` (`cpp:886-934`), each row's
  `value` = capture UUID (`cpp:906`); handler `onCaptureSelectionChanged()` reads
  `getSelectedValue().asUUID()` (`cpp:1380`) into `mSelectedCapture` (`h:81`). Displays list
  `display_list` rebuilt by `rebuildDisplayList()` (`cpp:936-1007`), filtered to the selected
  capture (`cpp:951-954`), row value = display UUID (`cpp:981`).
- **PROVES — poll/refresh model.** `draw()` calls `pollSnapshots(false)` each frame
  (`cpp:663-667`), throttled by `SNAPSHOT_POLL_SECONDS = 0.25f` (`cpp:36`). Refresh is
  revision-driven: compares config/runtime/performance revisions (`cpp:678-692`) and calls
  `refreshConfiguration()` / `refreshRuntime()` / `refreshPerformance()`. Any mutation calls
  `invalidateRegistrySnapshot()` (`cpp:2110-2114`) which forces an immediate re-poll. **Every
  Gate mutation must follow this pattern.**
- **PROVES — mode-toggle + spinner precedent.** `rate_mode` combo (`xml:459-462`, `mRateModeCombo`
  `cpp:399`) + `target_fps` spinner (`xml:463-476`), commit `onCommitRateSettings()`
  (`cpp:1738-1761`), enable-gating in `refreshCaptureEditor()` (`cpp:1136-1141`) greys the spinner
  unless mode is target — the exact analog of Manual/Auto + interval. Layout is hand-placed
  top-left pixels. Destructive actions confirm first: `remove_capture` →
  `GenericAlertYesCancel` → `LLPrismLens::removeCapture()` (`cpp:1462-1503`). Controls grey with a
  reason via `setActionState(ctrl, enabled, tooltip)` (`cpp:266-273`).
- **PROVES — reset-button idiom.** `bone_pov` spinners each pair with an 18×18
  `image_overlay="Refresh_Off"` reset button (e.g. `bone_pov_offset_reset` `xml:402`, handler
  lambda `cpp:524-550`); the group reset writes a default struct then `invalidateRegistrySnapshot()`
  (`cpp:551-571`).
- **PROVES — settings-bound controls need no handler.** The Performance tab binds combos directly
  to debug settings via `control_name` (`PrismAdaptivePerformance` `xml:723-733`, etc.) with **no**
  C++ wiring.

### 1.7 The switcher precedent (the Gate's scheduling brain) — **verified API**

- **PROVES.** `ALDirectorSwitcherModel` (`aldirectorswitchermodel.h`) — a "Deterministic,
  renderer-independent Director camera switch scheduler". Constants: `SLOT_COUNT = 12` (`:20`),
  `DEFAULT_INTERVAL_SECONDS = 8.0` (`:21`), `MIN_INTERVAL_SECONDS = 0.1` (`:22`),
  `MAX_INTERVAL_SECONDS = 3600.0` (`:23`), `MAX_JITTER_FRACTION = 0.45` (`:24`),
  `DEFAULT_SEED` (`:25`).
- **PROVES — the `Config` (`:27-35`):** `bool mAuto`, `bool mSequence`, `F64 mIntervalSeconds`,
  `F64 mJitterSeconds`, `U64 mSeed`, `std::array<bool, 12> mEnabled`. Free `sanitizeConfig()`
  (`:47`) clamps.
- **PROVES — the `Frame` result (`:37-44`):** `bool mCut`, `S32 mSlot`, `F64 mBoundary`,
  `U64 mEventIndex`, `U64 mEventsElapsed`.
  **`mBoundary == eventBoundary(final_event)` — the time the JUST-FIRED cut occurred (≤ now), NOT
  the next cut** (`aldirectorswitchermodel.cpp:398-407`, `eventBoundary` `:133-161`,
  no-jitter form `mAnchorTime + (event_index+1)*mIntervalSeconds` `:137-139`).
- **PROVES — the `Controller` public surface is exactly six members (`:52-69`):** default ctor;
  `Frame update(F64 now, const Config&)` (`:56`); `bool manualPunch(S32 slot, F64 now)` (`:60`);
  `bool rebase(F64 now)` (`:64`); `void reset()` (`:67`); `S32 activeSlot() const` (`:69`).
  **There is NO `peekNextSlot` / `nextSlot` / `previewSlot`** — `selectSlot`, `selectSequenceSlot`,
  `selectRandomSlot`, `eventBoundary` are all **private** (`:79-83`).
- **PROVES — behavior contracts:** large forward jump "advances directly to the final elapsed
  event and emits at most one cut" (`:54-55`, impl `findLastDueEvent` `:163-205`); a manual punch
  is valid for a disabled slot, becomes active, and restarts the interval (`:58-60`, impl
  `:145-168` test-confirmed). Slot selection is a **pure deterministic function of event index +
  seed** (no mutable RNG — `.cpp:72-80` comment), so a next-slot value *could* be computed, but
  only via the private methods and (for sequence) the private `mAnchorSlot`.
- **PROVES — sequence mode** (`selectSequenceSlot` `.cpp:221-259`): deterministic round-robin over
  the enabled slots **in bank order**, wrapping, skipping disabled, offset from `mAnchorSlot`.
- **PROVES — already adversarially unit-tested:** `tests/aldirectorswitchermodel_test.cpp`,
  group `"ALDirectorSwitcherModel"`, 15 tests incl. sequence walk (`test<4>`), manual punch on a
  disabled slot (`test<5>`), large-hitch equivalence (`test<12>`), determinism (`test<8>`).
- **PROVES — the viewer adapter** `ALDirectorSwitcher` owns the controller by value
  (`aldirectorswitcher.h:124`), drives it from `tick(F64 presentation_time)`
  (`aldirectorswitcher.cpp:731`, main drive `frame = mController.update(now, readConfig(bank))`
  `:820-821`, `if (frame.mCut) applySlot(frame.mSlot, …)` `:827`), with `punch(slot)`
  (`:831`, → `mController.manualPunch` `:700-701`) and bank persistence `loadBank()/saveBank()`
  (`:168`, `:242`). **This is a shipped precedent for exactly the manual-take + auto-cycle +
  boundary logic the Gate needs** — but it, too, never peeks the next slot (none exists to peek).

**Design consequence (IMPLIES).** The Gate is the **prism-producer analog** of `ALDirectorSwitcher`:
where the adapter re-points the single viewer camera, the Gate re-points the prism monitors'
`mCaptureSlot`. **Reuse the `Controller` for the current cut and manual take; compute pre-warm
(next index / next boundary) in the Gate itself** — see §D/§F for exactly why and how.

---

## STEP 2 — Design resolutions (A–H)

### A. The Gate model — what it is and where it lives

**Recommendation.** Add a `PrismGate` inside `PrismLensRegistry` (member `mGate`) so it can
read/write `mLenses[]`/`mDisplays[]` directly. Its *scheduling brain* is a reused
`ALDirectorSwitcherModel::Controller`; its *prism glue* (program-producer materialization + monitor
routing + gate-owned pre-warm) is the only new logic.

Serializable shape (INFERS — rides `prism_gates`):

```cpp
// llprismlens.h  (public, pure-data, pointer-free — mirrors CameraSettings)
enum class EGateMode : U8 { MANUAL, AUTO_CYCLE };

struct GateArmedCamera            // a lightweight camera DEFINITION, not a producer
{
    LLUUID          mArmId;       // stable identity for this armed row (UI + persist)
    std::string     mLabel;       // operator-facing name ("Wide", "OTS", …)
    bool            mEnabled = true; // maps 1:1 to Controller Config.mEnabled[i]
    CameraSettings  mCamera;      // FULL snapshot, incl. mBonePov (pointer-free, h:206-249)
};

struct GateSettings               // serializable
{
    LLUUID       mGateId;
    bool         mActive          = false;
    EGateMode    mMode            = EGateMode::MANUAL;
    F64          mIntervalSeconds = 8.0;   // = DEFAULT_INTERVAL_SECONDS
    U32          mPrewarmFrames   = 3;     // §D; 0 disables pre-warm (accept 1 stale frame)
    std::vector<GateArmedCamera> mArmed;   // ORDERED; cap = 12 (SLOT_COUNT)
    S32          mProgramArmIndex = 0;     // on-air index into mArmed (MANUAL authority)
    S32          mPreviewArmIndex = -1;    // MANUAL preview candidate for TAKE
    LLUUID       mProgramCaptureId;        // id of the persisted PROGRAM producer (§H)
};
```

Runtime (INFERS — inside the registry, not serialized):

```cpp
struct PrismGateRuntime
{
    ALDirectorSwitcherModel::Controller mController;   // reused brain (current cut + manual take)
    S32  mProgramSlot   = -1;   // reserved producer holding the on-air camera (SERIALIZED capture)
    S32  mWarmSlot      = -1;   // reserved producer holding the pre-warm camera (NOT serialized)
    S32  mOnAirArmIndex = -1;   // resolved this frame
    S32  mWarmArmIndex  = -1;   // gate-computed next index being warmed (-1 when idle)
    F64  mNextCutTime   = 0.0;  // gate-computed (jitter=0): lastBoundary + mIntervalSeconds
    U64  mCutSerial     = 0;    // bumps on every cut (drives UI on-air indicator)
    U64  mGateRevision  = 0;    // UI-poll signature for gate state (see §G)
};
```

Additive struct fields the Gate needs (the *only* engine-struct changes — §CHECKLIST):

- `PrismInstance` (`llprismlens.cpp:1583`): `bool mGateReserved = false;` (this slot is a gate
  producer → hidden from the capture-list UI, and if it is the warm slot, skipped by `sceneData`);
  `bool mGateWarm = false; U32 mGateWarmW = 0, mGateWarmH = 0;` (the §D admission seed).
- `PrismDisplay` (`llprismlens.cpp:1616`): `bool mGateSubscribed = false;` (a monitor routed by
  the Gate).

- **ONE global gate for v1.** **IMPLIES** from `MAX_CAPTURES == 3` (`llprismlens.h:28`): an active
  gate needs 1 program slot + (transiently) 1 warm slot = 2 producers; two independent active
  gates would need up to 4 and cannot fit. Model the data as an *array* `prism_gates` (v2 can add
  gates) but **enforce exactly one `mActive` gate** in v1 (mirrors the single `ALDirectorSwitcher`).
- **How a MONITOR subscribes — verified feasible.** A monitor is an ordinary `PrismDisplay` with
  `mGateSubscribed = true`. Each frame, *inside* `updateGate` (before the prepare loop), the Gate
  writes every subscribed occupied display's `mCaptureSlot` = `mProgramSlot` and
  `mCaptureGeneration` = `mLenses[mProgramSlot].mHandle.mGeneration`. Non-subscribed displays are
  skipped by `mGateSubscribed == false`, so the existing display→capture path is byte-identical for
  them (**PROVES** the field they read, `mCaptureSlot`, is a single mutable slot index re-read every
  frame, `:2708`). This is the `mGateSubscribed` idea the brief asked to verify — it is feasible
  and requires exactly one new bool on `PrismDisplay`.

### B. Camera count — definitions vs producers, and the exact caps

`MAX_CAPTURES = 3` (`llprismlens.h:28`) caps *simultaneous producers*, not *definitions*.

**Recommendation — arm up to 12 lightweight `CameraSettings` definitions; keep a 2-slot producer
footprint while active.** `GateArmedCamera` carries a full `CameraSettings` snapshot (pure,
pointer-free — `llprismlens.h:206-249`), **not** a live capture id. While the gate is active it
holds two reserved producer slots (`mProgramSlot`, `mWarmSlot`) and *reprograms them in place*
from the armed definition — it never allocates/frees per cut (see §C/§D). Armed cap =
`ALDirectorSwitcherModel::SLOT_COUNT = 12` (`aldirectorswitchermodel.h:20`), which also lets the
Gate map `mArmed` 1:1 onto the Controller's 12 `mEnabled` slots for free. **INFERS**: 12 is a
soft, data-only cap chosen to match the switcher's vocabulary; the engine does not require it.

| Quantity | Value | Source |
|---|---|---|
| Armed camera **definitions** (soft, data-only) | **12** | `aldirectorswitchermodel.h:20` |
| Simultaneous **producers** (hard, engine) | **3** = `MAX_CAPTURES` | `llprismlens.h:28` (PROVES) |
| Gate **producer footprint** while active | **2** reserved (program + warm) | §C/§D (IMPLIES) |
| Producers actually **rendered** per frame | **1** (round-robin) | `llprismlens.cpp:5784` (PROVES) |

**Why 12 armed does not raise render cost.** An armed-but-idle definition has **no producer** — it
is pure `CameraSettings` bytes, so it cannot be selected by `chooseRenderSlot` (no slot,
`:3085-3093`) and cannot be admitted (no `mAnyDisplayVisible`). Render cost is bounded by the ≤2
reserved producers, and by §1.3 only **one** renders per frame. The 11 idle armed defs cost only
the Gate's O(12) per-frame scan of `mEnabled` flags. **This is the "no FPS cost" guarantee, and it
holds — see §NO-FPS-DROP VERDICT.**

> **Rejected alternative (was "option 1" in the prior draft): arm existing captures by id, cap 3.**
> Cleaner persistence, but the operator can arm at most 3 angles — the entire simultaneous-render
> budget — so the "many angles" ambition is unmet. **Do not ship this;** it defeats the feature.
> Retained here only as the strict subset a schedule emergency could fall back to.

### C. The switch / on-air routing (reuse, don't duplicate)

Each frame, a new `PrismLensRegistry::updateGate(now)` runs at the **top of the prepare region of
`renderAuxiliaryView()`, i.e. after the existing early-returns (`llprismlens.cpp:5743`, `:5757`,
`:5761`) and immediately before the prepare loop at `:5774`.** Placement note (**corrected** — see
§CORRECTIONS #4): the program producer is a **real occupied capture** created at gate activation,
so `has_captures = registry.hasDesignation()` (`:5735`, checked `:5757`) is already true whenever a
gate is active; there is no first-frame dead-lock and no need to move the early-returns.

1. **Advance the brain (current cut).** Build `ALDirectorSwitcherModel::Config` from `GateSettings`:
   `mAuto = (mMode == AUTO_CYCLE)`, `mSequence = true` (explicit cam-order), `mIntervalSeconds`,
   **`mJitterSeconds = 0.0` (forced — see §D/§F)**, `mEnabled[i] = mArmed[i].mEnabled && i <
   mArmed.size()`. Call `Frame f = mController.update(now, cfg)`. Use `f.mSlot` as the on-air arm
   index and `f.mCut` as the cut-this-frame flag. **Do NOT read `f.mBoundary` as a future time** —
   it is the boundary of the event that just fired; the Gate derives the *next* cut time itself
   (step 5).
2. **Resolve + materialize on-air.** `mOnAirArmIndex = f.mSlot` (MANUAL: `mProgramArmIndex`).
   Ensure `mProgramSlot` holds a virtual CAMERA_FEED capture whose `mCamera ==
   mArmed[mOnAirArmIndex].mCamera`. On a cut, if the warm slot already holds the incoming camera
   (§D), **swap roles** `mProgramSlot ↔ mWarmSlot` (no reprogram at the cut instant); otherwise
   reprogram `mLenses[mProgramSlot].mCamera` in place via the runtime-only discipline (`++mRuntimeRevision`
   only, mirroring `setVirtualCameraTransform`, §1.4).
3. **Route monitors.** For every occupied display with `mGateSubscribed == true`, set
   `display.mCaptureSlot = mProgramSlot` and `display.mCaptureGeneration =
   mLenses[mProgramSlot].mHandle.mGeneration`. *(A role-swap changes `mProgramSlot`, so this
   re-point is required every cut, not just once — hence it lives in the per-frame `updateGate`.)*
4. **Let the engine do the rest.** The prepare loop then sets `mAnyDisplayVisible = true` on the
   on-air producer (a visible subscribed display now binds it, `:2706-2718`) and `false` on every
   other camera. `updateCadenceEntitlements` (`:2990`) zeroes idle cameras; `chooseRenderSlot`
   (`:3089`) renders only the watched one(s).
5. **Compute the next cut for pre-warm (gate-owned).** With `mJitterSeconds == 0`,
   `mNextCutTime = f.mBoundary + mIntervalSeconds` on any frame where `f.mCut` (and on the first
   armed frame, seed it from the Controller's first boundary). The **next arm index** is the next
   enabled entry after `mOnAirArmIndex` in `mArmed` order (the Gate owns this list, and it drives
   the Controller in `mSequence` mode, so the two agree). Store as `mWarmArmIndex` when the
   pre-warm window opens (§D). *(Manual punch keeps them in sync: a punch sets both the Controller's
   active slot and the Gate's `mProgramArmIndex`, and the Gate recomputes `mNextCutTime = now +
   interval` and the next enabled index from the punched index.)*

**The Gate sets/propagates "watched" purely by owning `mCaptureSlot`/`mCaptureGeneration` on
subscribed displays and by the §D `mGateWarm` admission input.** It writes **no** Hz, **no**
entitlement, and calls **nothing** in the water-filling / selection math.

### D. Pre-warm (corrected: gate-owned, sequence-only, jitter-free)

**Problem (PROVES).** A cut to a suppressed camera shows blank/stale: a suppressed capture released
its output (`suppressOutput`, `:4808-4816`), and an unwatched camera is never even sized (early
`return false` at `:2792-2795`). So a naïve reprogram-in-place at the cut instant yields **one
stale/black frame** while the new framing renders for the first time.

**What the prior draft got wrong.** It planned to open the warm window at `now >= f.mBoundary −
N·dt` and to read the "deterministic next on-air index" from the model. Against the code:
`f.mBoundary` is the *past* boundary (`aldirectorswitchermodel.cpp:398`, `eventBoundary` `:133-161`),
and the Controller exposes **no** next-slot accessor (`aldirectorswitchermodel.h:52-69`; selectors
are private `:79-83`). Reading `mBoundary` as the next cut would open the window at the wrong time
(one interval early), and there is no API to learn the next camera. **Both are fixed by computing
next-time and next-index in the Gate**, which is exact **iff** two conditions hold, both of which
the Gate controls:

- **Sequence mode only.** The Gate always drives the Controller with `mSequence = true`, and the
  next enabled index is a plain round-robin over the Gate's own ordered `mArmed`. (Random-mode
  pre-warm would need `mAnchorSlot`/permutation state that is private; **v1 does not offer random
  mode** — a switcher wants an explicit angle order anyway. If random is ever wanted, add a public
  `S32 peekSlot(U64 eventIndex) const` to the Controller; that is the *only* way to do it correctly,
  and it is out of scope for v1.)
- **Jitter forced to 0.** Then `nextCutTime = lastBoundary + mIntervalSeconds` exactly. (With
  jitter, the next boundary is a private `eventBoundary(final_event+1)` the Gate cannot read.)

**Design.** `mPrewarmFrames` (default 3; **INFERS**, tune 2–5) before the computed `mNextCutTime`,
the Gate brings the **next** camera into the watched set so it renders a fresh frame in time.

- **Window predicate:** open the warm window when `now >= mNextCutTime − mPrewarmFrames * frameDT`,
  where `frameDT = 1 / gFPSClamped` (`gFPSClamped` already read at `:2955`). On entry set
  `mWarmArmIndex` = next enabled arm index and program `mLenses[mWarmSlot].mCamera` =
  `mArmed[mWarmArmIndex].mCamera`.
- **Sanctioned minimal admission hook.** In `updateGate`, for the warm slot set `mGateWarm = true`
  and seed `mGateWarmW/H` from the **program producer's current output size**
  (`mOutputWidth/Height`, `:1600-1601`) — the warm camera inherits the same monitors at the cut, so
  that is the correct target size. Then add **one** guarded block at the top of
  `prepareCameraCapture`, immediately before the `if (!capture.mAnyDisplayVisible) return false;`
  gate (`:2792`):

  ```cpp
  if (!capture.mAnyDisplayVisible && capture.mGateWarm &&
      capture.mGateWarmW > 0 && capture.mGateWarmH > 0)
  {
      capture.mAnyDisplayVisible = true;             // admit as if watched
      required_width  = llmax(required_width,  capture.mGateWarmW);
      required_height = llmax(required_height, capture.mGateWarmH);
  }
  ```

  Everything downstream — target sizing (`:2797-2819`), `mFrame.mPrepared`, the water-fill,
  `chooseRenderSlot` — is **byte-identical**. This *layers an admission input*; it does not rewrite
  the scheduler. It is the **only** edit inside the admission/prepare path this feature makes, and
  it is a strict no-op when no gate is warming (`mGateWarm` defaults false).
- **At the cut:** `f.mCut` fires; the warm slot already holds a `CURRENT`/`HELD` fresh frame; roles
  swap (`mProgramSlot ↔ mWarmSlot`); monitors re-point (step C.3) to the now-program slot; the old
  program slot drops `mGateWarm`, loses its visible display next frame, and is suppressed for free.
  **No stall** because the incoming output was produced during the window.
- **MANUAL pre-warm (corrected — see §CORRECTIONS #5).** Warm the **preview** (`mPreviewArmIndex`)
  only for a bounded burst (e.g. `mPrewarmFrames` frames) right after the operator selects a
  preview, **then drop `mGateWarm`**. Do **not** hold the preview warm continuously: two
  continuously-watched producers permanently share the single aux slot (§1.3), which halves the
  live feed's refresh forever. On-demand warm keeps the live feed at full cadence except for the
  brief window, and a fresh TAKE is still guaranteed because a manual TAKE re-warms for
  `mPrewarmFrames` and cuts only when the warm slot is `CURRENT`. (If `mPrewarmFrames == 0` the
  operator accepts one stale frame on TAKE — an honest, explicit option.)

### E. Transitions — CUT only for v1 (crossfade deferred)

- **Default & only v1 transition: HARD CUT.** The monitor swaps its bound capture slot
  (`mCaptureSlot`/`mCaptureGeneration`, step C.3) at the cut frame. **No blending path exists or is
  needed** — the display composite samples whatever slot the monitor points at
  (`getCompositeStates` `llprismlens.h:616`, `getAuxCompositeStates` `:626`). Confirmed: the monitor
  just re-points; there is no per-frame blend, no second sampled source, no shader touch.
- **CROSSFADE: defer.** A genuine crossfade needs *both* feeds refreshed on the same composite
  frame, but the engine refreshes **one** auxiliary slot per frame (`:5784`); both feeds can be
  simultaneously *composited* (retained textures) yet cannot both be *refreshed* on one frame, so a
  fade runs both at half cadence and the outgoing feed staleness grows. It is a **display-composite
  / shader** feature (`prismLensF.glsl`, the composite packs `mScreenEffect*`), which is
  **OFF-LIMITS** here. Scope it as a separate composite-pass project, not a Gate mode.

### F. Auto-cycle timing — verdict: WRAP the model, don't extend it

- **Reuse `ALDirectorSwitcherModel::Controller` for cut timing and manual take (verbatim, no edit).**
  Build a `Config` each frame, call `update(now, cfg)` for the current cut and `manualPunch(slot,
  now)` for TAKE. The model already carries `mIntervalSeconds` (`aldirectorswitchermodel.h:31`) with
  bounds `MIN 0.1 … MAX 3600.0` (`:22-23`), `sanitizeConfig` (`:47`), the large-hitch guarantee
  (`:54-55`), and manual-punch-restarts-interval (`:58-60`). Use `mIntervalSeconds` as the global
  interval; **UI range 0.5–120 s, default 8 s** (= `DEFAULT_INTERVAL_SECONDS`, `:21`).
- **What the Gate does NOT reuse from the model, and must own:** the **next** slot and **next**
  boundary for pre-warm (the model exposes neither — §D). This is why the verdict is *wrap*, not
  *extend*: reuse the six-member public surface as-is; compute pre-warm from the Gate's ordered arm
  list + `lastBoundary + interval` (valid because the Gate forces `mSequence = true`,
  `mJitterSeconds = 0`). **No modification to `ALDirectorSwitcherModel` / `ALDirectorSwitcher`.**
- **Jitter/random are out of v1** for the Gate (the model supports both, but pre-warm cannot follow
  them without a new `peekSlot` accessor). Ship global fixed interval, sequence order.
- **The tick runs on the existing per-frame Prism update.** `updateGate(now)` uses
  `LLTimer::getTotalSeconds()` — the same clock `updateCadenceEntitlements` uses (`:3070`), analogous
  to `ALDirectorSwitcher::tick(presentation_time)` (`aldirectorswitcher.cpp:731`). No timer thread;
  large frame gaps are absorbed by the model's "advance to final event, at most one cut" guarantee.

### G. UI — a 4th "gate" tab in the Prism Manager

Add `<panel name="gate_tab" label="Gate">` as the 4th child of `detail_tabs`, inserted before
`</tab_container>` (`floater_prism_manager.xml:820`). No floater-rect change (tabs are internal).
Clone `performance_tab`'s `scroll_container`→`document` shell (`xml:703-818`) if the form is tall.
All widgets mirror existing kinds — no new control types.

Control set + bindings:

- **Armed list** — `scroll_list name="gate_armed_list"` (columns: order #, label, on-air emblem,
  enabled checkbox), row `value` = `GateArmedCamera.mArmId` (mirror `cpp:906`/`:981`). Rebuilt in a
  new `rebuildGateArmedList()` from the gate snapshot, reselect-by-value like `rebuildCaptureList`
  (`cpp:930-933`).
- **Arm selected / Disarm** — `button name="gate_arm"` adds the currently-selected `capture_list`
  row (`mSelectedCapture`, `cpp:1380`) as a new `GateArmedCamera` capturing that capture's
  `CameraSettings` snapshot; `button name="gate_disarm"` removes the selected armed row (confirm via
  `GenericAlertYesCancel` + floater-handle lambda, `cpp:1462-1503`). Both call new
  `LLPrismLens::gateArm(...)` / `gateDisarm(...)` then `invalidateRegistrySnapshot()`.
- **Manual TAKE** — `button name="gate_take"` calls `LLPrismLens::gateTake()` →
  `mController.manualPunch(previewIndex, now)`; a **Preview** selector picks the candidate arm
  (click a row → set `mPreviewArmIndex`, on-demand warm per §D). Instantaneous cut.
- **Mode toggle** — `combo_box name="gate_mode"` (Manual/Auto), cloned from `rate_mode`
  (`xml:459-462`), commit `onGateModeChanged()` → `LLPrismLens::setGateSettings(...)`.
- **Cycle interval** — `spinner name="gate_interval"` (min 0.5, max 120, default 8), cloned from
  `target_fps` (`xml:463-476`); **pre-warm frames** `spinner name="gate_prewarm"` (0–5). Enable the
  interval only in Auto via `setActionState` (`cpp:266-273`). Optional reset buttons cloned from
  `bone_pov_*_reset` (`xml:402`, lambda `cpp:524-550`).
- **Current-live indicator** — a label/emblem bound from the gate snapshot's `mOnAirArmIndex`,
  refreshed through the revision poll: add `mGateRevision` to the snapshot (or reuse
  `mRuntimeRevision`) so the on-air readout updates within one poll tick (0.25 s, `cpp:36`).
- **"Source: Gate" per display** — on the Displays tab, add a per-display checkbox *Fixed capture*
  (today) vs *Gate* (sets `mGateSubscribed`) committed through the display path
  (`setDisplaySettings`, `cpp:1902`, extended with the flag, or a dedicated
  `setDisplayGateSubscribed(handle, bool)`).
- **Refresh discipline** — every Gate mutation calls `invalidateRegistrySnapshot()` (`cpp:2110`).
- **Hide gate producers from the capture list** — `rebuildCaptureList` (`cpp:886-934`) skips
  captures with `mGateReserved == true`, so the program/warm slots never appear as user-editable
  cameras (they surface only as the gate tab's on-air readout).

### H. Persistence — corrected (program producer is a real serialized capture)

**The prior §H would hard-fail the whole scene.** Its plan ("write each gate monitor's `capture_id`
= last on-air") only works if that on-air capture is actually in `prism_captures`; otherwise the
display cross-ref at `llprismlens.cpp:4405-4406` (`parsed.mCaptureId.isNull() ||
!capture_ids.count(...)` → `return fail`) rejects the **entire** load. And `sceneData` serializes
*every* occupied slot (`:3857-3859`), so a naïve gate would also emit a phantom warm capture.

**Corrected scheme (additive, no version bump):**

1. **The PROGRAM producer is a normal, serialized virtual capture.** Created at gate activation via
   `addVirtualCamera` (`:1839`), it occupies a real slot with a stable id (`mProgramCaptureId`), so
   `hasDesignation()` is true (§C placement) and its `capture_id` **resolves** for subscribed
   monitors. Its `mCamera` is reprogrammed in place each cut; at save time it holds the *last on-air*
   framing. Marked `mGateReserved` so the UI hides it (§G) — but it **is** written by `sceneData`.
2. **The WARM producer is transient and NOT serialized.** In `sceneData`, skip the warm slot:
   `if (capture.mGateReserved && static_cast<S32>(slot) == mGate.mWarmSlot) continue;` (it has no
   subscribed displays, so nothing dangles). *(Equivalently, allocate the warm slot lazily only
   inside a pre-warm window; but reserve-once-per-active-lifetime is preferred to avoid the
   `releasePrismLensBuffers` whole-pool free hitch on alloc/free — `:4811`, `:4841`.)*
3. **Subscribed monitors serialize normally**, with `capture_id = mProgramCaptureId` (resolves) plus
   an optional `gate_source: true` flag (new viewer hands routing to the gate; old viewer ignores
   the flag and keeps the static binding to the last-on-air program capture). Optional-on-read.
4. **New top-level `prism_gates` array** emitted from `sceneData` (`:3856`) and parsed *optionally*
   in `applySceneData` (absent → no gate). One lift line in `saveScene`:
   `scene["prism_gates"] = prism["prism_gates"];` next to `llfloaterdirector.cpp:887-888`.
   **`SCENE_VERSION` stays 3** (`:86`).
   Per-gate keys (INFERS): `gate_id`, `active`, `mode` ("manual"|"auto_cycle"), `interval_seconds`,
   `prewarm_frames`, `program_arm_index`, `program_capture_id`, and `armed` — an ordered array of
   `{ arm_id, label, enabled, camera:{…full CameraSettings incl. bone_pov, reusing the existing
   camera-field serializers at `:3868-3949`} }`.
5. **On load:** parse `prism_captures`/`prism_displays` first (unchanged); then, if `prism_gates`
   present and `active`, re-adopt the persisted capture whose id == `program_capture_id` as
   `mProgramSlot`, mark it `mGateReserved`, re-subscribe every `gate_source` display, allocate the
   warm slot, and `mController.reset()` so the schedule restarts cleanly. Validate each armed
   `camera` with the existing `validCameraSettings` path; a malformed arm is **dropped, not fatal**
   (keep the gate best-effort so a partially-edited scene still loads).

**Compatibility:**
- **Old scene → new binary:** no `prism_gates` → empty inactive gate; every display is a fixed
  binding. Identical to pre-gate behavior.
- **New scene → old binary:** `prism_gates` and `gate_source` are ignored (allow-listed load,
  `llfloaterdirector.cpp` `loadScene`; unknown keys tolerated `:4075-4080`). The program producer
  loads as a **normal capture** and its `gate_source` monitors load as **normal displays bound to
  it** → the old viewer shows a **static last-on-air feed** — graceful, well-defined degradation,
  and it does **not** trip `:4405-4406` because `capture_id` resolves.

---

## §FRAME-BY-FRAME LIFECYCLE (the brief's "nail it" section)

Assume AUTO_CYCLE, `mArmed = {A,B,C}` enabled, `mIntervalSeconds = 8`, `mPrewarmFrames = 3`,
program slot = P (on-air A), warm slot = W (idle), a monitor M subscribed. Times relative to the
last cut at `t=0`; `frameDT ≈ 1/60`.

| Frame / time | `updateGate` action | Watched set | Aux renders this frame |
|---|---|---|---|
| steady (0 ≤ t < 8 − 3·dt) | route M→P; `mGateWarm(W)=false` | {P} | **P only** (1 render) |
| pre-warm opens (t ≈ 8 − 3·dt) | compute nextIndex=B; program W.mCamera=B; `mGateWarm(W)=true`, seed W size from P's `mOutputWidth/H` | {P, W} | P and W **interleave** — 1 render/frame, each ~½ cadence for 3 frames |
| pre-warm frames 2–3 | W accrues a `CURRENT` frame | {P, W} | 1 render/frame (round-robin) |
| **cut** (`f.mCut`, t ≈ 8) | swap P↔W (now P holds B, fresh); route M→P(=old W); old-P drops `mGateWarm`, loses visible display | {P} | **P only** (B is already fresh → no stall) |
| steady again | `mNextCutTime = f.mBoundary + 8` | {P} | P only |

**On a MANUAL TAKE** (operator selects preview = C at time `t`): the Gate programs W.mCamera=C,
`mGateWarm(W)=true` for `mPrewarmFrames` frames; when W is `CURRENT`, `gateTake()` calls
`manualPunch(Cindex, now)`, swap P↔W, route M→P(=C), interval restarts. If `mPrewarmFrames == 0`,
the swap happens immediately and M shows **one stale/black frame** while C renders first — the
explicit "accept a black frame" option. **The new source pre-warms before the cut; a black frame
occurs only with `mPrewarmFrames == 0` or under producer starvation (below).**

**Guarantee of no FPS drop with 12 armed:** at every row above, **exactly one** aux view is
rendered per presented frame (`chooseRenderSlot` returns one slot, `renderAuxiliaryView` renders
one, `:5784`) regardless of how many cameras are armed; the 11 non-live armed defs are pure bytes
with no slot and are gated out at `:2990`/`:3089`. See the explicit verdict below.

---

## §NO-FPS-DROP VERDICT (does the promise hold?)

**Frame-time (FPS) promise: HOLDS.** Per-frame auxiliary GPU work is bounded to **one** rendered
capture regardless of arm count (`:5784`, called once/frame `llviewerdisplay.cpp:919`). Idle armed
definitions have no producer and are zeroed by the admission gate (`:2990`), so 12 armed cost the
same peak frame time as one camera today. The demand-driven engine already guarantees this; the
Gate's job is to **not regress it**, and — provided the §D admission hook is the only prepare-path
edit and the water-fill/`chooseRenderSlot` are untouched — it does not.

**Two honest caveats (not FPS drops, but refresh/VRAM effects):**
1. **Pre-warm window (~3 frames per auto-cut):** two producers are watched, so they share the single
   aux slot (`:3085`) — the live feed's *refresh rate* roughly halves for those ~3 frames, and one
   transient extra scratch target of `mGateWarmW×H` is live. **Peak per-frame GPU does not rise; the
   live camera's frame time is unchanged; only its update cadence dips briefly.**
2. **MANUAL continuous preview-warm would make caveat 1 permanent** (a steady two-watched state at
   half live cadence). §D fixes this by warming preview **on-demand for a bounded burst**, not
   continuously. With that fix, steady state is exactly **one** rendering camera, identical to a
   single normal camera today.

**Where the promise can break (guard against these):**
- A non-gate capture occupying the 3rd `MAX_CAPTURES` slot starves the warm slot → pre-warm is
  skipped → cuts reprogram-in-place → **one stale frame per cut** (see §EDGE CASES). No FPS drop,
  but a visible hitch on the feed.
- Any reviewer who "optimizes" by allocating/freeing the warm slot per cut incurs the
  `releasePrismLensBuffers` **whole-pool free** each cut (`:4811`) — a real per-cut hitch. Reserve
  the warm slot for the gate's active lifetime instead.

---

## §EDGE CASES (each with the concrete resolution)

1. **Gate with 0 armed.** `updateGate` is a no-op: no producer reprogram, no monitor reroute,
   `mController.update` returns no cut (empty `mEnabled` → `aldirectorswitchermodel.cpp:371-380`).
   Subscribed monitors keep their last valid binding (or, at activation with 0 armed, the gate stays
   inactive and monitors remain fixed). No −1 slot is ever written to a display.
2. **Live camera disarmed mid-cycle.** On disarm of `mOnAirArmIndex`, clamp on-air to the nearest
   still-enabled arm index (never leave −1 while ≥1 armed); reprogram the program slot to that
   camera and re-point monitors the same frame. If the disarmed camera was the warm target, recompute
   `mWarmArmIndex`. Test: `test`-style assertion that disarm never leaves a dangling program slot.
3. **Monitor watching a gate whose live source is itself a monitor (feedback loop) — MUST be
   prevented.** A `GateArmedCamera` is a virtual CAMERA_FEED (`mCamera`); a monitor is a
   `PrismDisplay`. They are different objects, so a direct type-level loop is impossible. The real
   risk is a **virtual-screen display** placed in the *world* such that an armed camera frames it —
   an in-world optical feedback, which the engine already tolerates via the 1-frame-lagged recursive
   mirror path (`getAuxCompositeStates`, `llprismlens.h:619-627`); no new loop is introduced. The
   **logical** guard the Gate must add: **reject arming a camera, or subscribing a monitor, that
   would make the gate route to a display bound (directly) to a gate producer** — i.e. forbid a
   `gate_source` display from also being an armed camera's explicit source. Enforce in `gateArm` /
   `setDisplayGateSubscribed` (return `ERegistryResult::INVALID_CONFIGURATION` with a reason,
   surfaced via `setActionState`). *(This mirrors the existing "a camera source cannot also be one
   of its display objects" duplicate guard, `:1687-1690`.)*
4. **More armed cameras than the pre-warm budget.** Irrelevant to render cost: only 1 program + 1
   warm are ever materialized; the other 10 remain pure defs. Pre-warm always targets exactly the
   single next enabled index.
5. **Save/load a scene with a gate.** Round-trips per §H: program capture + `gate_source` monitors +
   `prism_gates` block; warm slot omitted; old viewers degrade to a static last-on-air feed.
   Best-effort: a malformed armed camera is dropped, not fatal.
6. **MAX_CAPTURES interaction when non-gate captures already exist.** At activation, if
   `count() > MAX_CAPTURES − 2` (i.e. fewer than 2 free slots), the gate can still take **1** slot
   for the program producer but **cannot reserve the warm slot**. Resolution: activate with program
   only; **skip pre-warm** (cuts reprogram-in-place, accepting one stale frame) and surface a clear
   "Gate running without pre-warm — free a capture slot for smooth cuts" reason via `setActionState`
   / `ActionStatus` (`llprismlens.h:313-318`). If `count() >= MAX_CAPTURES` (no free slot at all),
   refuse activation with `AT_CAPACITY` (mirror `addVirtualCamera` `:1843-1846`). Enforce
   single-active-gate so the gate never needs more than 2 slots.
7. **Generation staleness in routing.** Re-pointing `mCaptureSlot` must also set
   `mCaptureGeneration` to the producer's current generation, or the display is filtered out by the
   generation guard (`:2708-2712`). A role-swap changes the bound slot's generation, so step C.3
   re-reads `mLenses[mProgramSlot].mHandle.mGeneration` every frame.

---

## §BONE-POV ORTHOGONALITY (confirmed)

**PROVES — orthogonal, no interaction needed.** `ALVCamBonePov::tick` (`alvcambonepov.cpp:308`)
enumerates registry captures from a `registrySnapshot()` (`:312-314`), reads
`capture.mCamera.mBonePov` (`:329`), and drives any enabled one via the runtime-only writers
`setVirtualCameraPosition` (`:452`) / `setVirtualCameraTransform` (`:525`). Because a
`GateArmedCamera` carries a full `CameraSettings` snapshot **including `mBonePov`** (`llprismlens.h:248`),
the Gate simply copies it into the materialized program producer; the **existing** driver then picks
up whichever capture is live and drives it — no gate↔bone-POV plumbing. An off-air bone-POV armed
def is not a capture, so it is not driven until it materializes on-air (correct and desirable). The
Gate must **not** touch `ALVCamBonePov`, `CameraSettings::mBonePov`, or
`normalizeBonePovJointSelection` (`llprismlens.h:150`).

---

## §TEST PLAN — pure, non-tautological, regression-failing

All pure (no viewer singleton, no GL), mirroring `aldirectorswitchermodel_test.cpp` and
`alvcambonepov_test.cpp`, and the pure precedent `normalizeBonePovJointSelection`
(`llprismlens.h:150`). Factor the Gate's pure helpers as free functions. New test file (INFERS):
`indra/newview/tests/llprismgate_test.cpp` (registered like `aldirectorswitchermodel_test.cpp`).

1. **Cycle-advance / next-index mapping** (armed `{A,B,C}`, sequence, interval T):
   `update(t0)`→A; `update(t0+T−ε)`→A `mCut=false`; `update(t0+T+ε)`→B `mCut=true`; wrap C→A after
   3T. **Single armed camera:** never cuts (regression guard against cut-to-self). **Empty armed
   set:** `updateGate` no-op. **Large jump** `update(t0+100T)`: exactly one cut, on-air =
   deterministic final-event slot (honors `aldirectorswitchermodel.h:54-55`). **Disarm on-air:**
   clamps to a still-armed index; no dangling program slot.
2. **Next-cut-time + pre-warm window (the corrected math):** with `mJitterSeconds == 0`,
   `nextCutTime(f) == f.mBoundary + interval`; `warmActive(now)` false for `now < nextCutTime −
   N·dt`, true within, inclusive at the boundary. **Warm target index == the Gate's next enabled arm
   index** (assert it equals what the Controller returns on the *next* cut — the sync check). Warm
   size seed == program `mOutputWidth/H`; a 0×0 seed yields **no** warm admission (guards the
   `mGateWarmW>0` block). **Assert the Gate never reads `f.mBoundary` as a future time** (regression
   guard for the corrected bug): feed a `Frame` with `mBoundary` in the past and confirm the window
   opens at `mBoundary + interval − N·dt`, not `mBoundary − N·dt`.
3. **Manual-take state machine:** `take()` with no preview → no cut; preview == program → no cut;
   preview ≠ program → exactly one cut, program←preview, interval restarts (via the model's
   `manualPunch` restart, `aldirectorswitchermodel.h:58-60`); Auto→Manual freezes on the current
   on-air index (no spurious cut on mode flip). **Bounded preview-warm:** after selecting a preview,
   `mGateWarm` is true for exactly `mPrewarmFrames` frames then false (regression guard against
   continuous-warm half-cadence).
4. **Persistence round-trip + cross-version:** `GateSettings` → LLSD → parse → equal (armed order,
   labels, enabled, mode, interval, prewarm, program_arm_index, program_capture_id, per-arm
   `CameraSettings` incl. `mBonePov`). Old scene (no `prism_gates`) → empty inactive gate, zero
   subscribed. **A `gate_source` display whose `capture_id == program_capture_id` validates** (the
   corrected §H — assert it does **not** trip `:4405-4406`). A `gate_source` display whose
   `capture_id` is null/dangling **fails** (proves why the program must be serialized). Dangling
   armed camera → dropped, scene still loads.
5. **Feedback-loop guard:** arming/subscribing that would route a gate producer to a display bound
   to a gate producer returns `INVALID_CONFIGURATION`.

---

## §IMPLEMENTATION PLAN (ordered; files/functions each pass touches)

**Pass 1 — Pure data + scheduler wrap (no engine wiring).**
- `llprismlens.h`: add `EGateMode`, `GateArmedCamera`, `GateSettings`; declare `setGateSettings`,
  `gateArm`/`gateDisarm`, `gateTake`, `setDisplayGateSubscribed`, `gateSnapshot`; declare pure free
  helpers (`gateNextEnabledIndex`, `gateNextCutTime`, `gateWarmActive`).
- New `indra/newview/tests/llprismgate_test.cpp`: §TEST PLAN 1–3 against the pure helpers +
  a `Controller` instance. **Reuses `ALDirectorSwitcherModel` — no net-new scheduler.**
- *Net-new code:* the pure helpers only. *Flag:* pre-warm next-index/next-boundary are net-new
  (the Controller cannot supply them, §D/§F).

**Pass 2 — Registry glue (the core).**
- `llprismlens.cpp`: `PrismInstance` (`:1583`) += `mGateReserved`, `mGateWarm`, `mGateWarmW/H`;
  `PrismDisplay` (`:1616`) += `mGateSubscribed`. Add `PrismGateRuntime mGate` + `GateSettings` to
  `PrismLensRegistry`. Implement `updateGate(now)` (steps C.1–C.5), program-slot allocation via
  `addVirtualCamera` at activation, warm-slot reserve, reprogram-in-place + role-swap, monitor
  routing, feedback-loop guard.
- `prepareCameraCapture` (`:2690`): the single guarded warm block **before `:2792`**.
- `renderAuxiliaryView` (`:5729`): call `registry.updateGate(now)` after the early-returns, before
  the prepare loop (`:5774`).
- *Flag:* `updateGate`, the reserve/reprogram/swap pool, and the routing are **net-new**; the
  admission hook is the **only** edit inside the proven scheduler path and is a no-op when idle.

**Pass 3 — Persistence.**
- `sceneData` (`:3852`): emit `prism_gates` (reuse camera-field serializers `:3868-3949` for each
  arm's `CameraSettings`); skip the warm slot; write `gate_source` on subscribed displays.
- `applySceneData` (`:4033`): parse `prism_gates` optionally; re-adopt `program_capture_id`;
  re-subscribe `gate_source` displays (optional-on-read); best-effort arm validation.
- `llfloaterdirector.cpp` `saveScene` (`:887-888`): add `scene["prism_gates"] =
  prism["prism_gates"];`. `loadScene` unchanged (whole scene already passed, `:1005`).
- Tests: §TEST PLAN 4–5. *No `SCENE_VERSION` bump.*

**Pass 4 — UI (gate tab).**
- `floater_prism_manager.xml`: 4th `gate_tab` before `:820`; armed list, arm/disarm, TAKE, mode
  combo, interval/prewarm spinners, live indicator; per-display gate-source checkbox on
  `displays_tab`.
- `llfloaterprismmanager.{h,cpp}`: `rebuildGateArmedList`, `onGateArm/onGateDisarm` (confirm via
  `GenericAlertYesCancel` `cpp:1462-1503`), `onGateTake`, `onGateModeChanged`,
  `onCommitGateInterval` (clone `onCommitRateSettings` `cpp:1738-1761` + enable-gating
  `cpp:1136-1141`), gate reset lambdas (clone `cpp:524-571`); hide `mGateReserved` captures in
  `rebuildCaptureList` (`cpp:886-934`); every handler ends with `invalidateRegistrySnapshot()`
  (`cpp:2110`); add `mGateRevision` to the poll (`cpp:678-692`).

**Cannot reuse existing machinery / net-new:**
- Pre-warm **next-index and next-cut-time** (Controller exposes neither; computed in the Gate,
  valid only under sequence + jitter=0 — §D/§F).
- The **program/warm producer pool** (reserve, reprogram-in-place, role-swap) — no existing
  "reprogrammable reserved producer" concept.
- **Monitor routing** (`mGateSubscribed` rewrite of `mCaptureSlot`/`mCaptureGeneration` each frame).
- The **`prism_gates`** schema + `gate_source` display flag + program re-adoption on load.
- The **feedback-loop / starvation guards** and the gate-tab UI.

Everything else is reuse: `ALDirectorSwitcherModel::Controller` (verbatim), `addVirtualCamera`, the
runtime-only transform-write discipline, the admission/selection engine (untouched but for the one
warm block), the persistence round-trip, and every UI idiom.

---

## §OFF-LIMITS (do not touch)

- The **render consume path** and **shaders**: `renderAuxiliaryView` render body (`:5790+`),
  `getCompositeStates`/`getAuxCompositeStates` (`llprismlens.h:616-627`), all Prism `.glsl`.
- The **admission/scheduler internals**: `updateCadenceEntitlements` water-fill (`:3005-3031`) and
  `chooseRenderSlot` (`:3068-3126`). The Gate **layers on `mAnyDisplayVisible`** (the one warm block).
- The **capture/display engine internals** beyond the four additive struct fields
  (`mGateReserved`, `mGateWarm`, `mGateWarmW/H`, `mGateSubscribed`) and the one `mGateWarm` prepare
  block.
- `ALDirectorSwitcherModel` / `ALDirectorSwitcher` — **reused as-is, not modified** (a public
  `peekSlot` would only be needed for random/jitter pre-warm, explicitly out of v1 scope).
- `lldirectorcast`; the **Bone-POV / VCam-joint** code (`ALVCamBonePov`, `CameraSettings::mBonePov`,
  `normalizeBonePovJointSelection`) — orthogonal, confirmed.
- **`SCENE_VERSION`** (`llfloaterdirector.cpp:86`) — persistence is additive; no bump.

---

## §CORRECTIONS to the prior draft (what the code contradicted)

1. **`Frame::mBoundary` is the PAST boundary, not the next cut** (`aldirectorswitchermodel.cpp:398`,
   `eventBoundary` `:133-161`). Prior §C step 1 / §D used it as the upcoming cut time. Fixed: the
   Gate computes `nextCutTime = f.mBoundary + mIntervalSeconds` (valid only with jitter forced 0).
2. **The Controller exposes no next-slot peek** (`aldirectorswitchermodel.h:52-69`; selectors are
   private `:79-83`). Prior §D assumed a readable "deterministic next on-air index". Fixed: the Gate
   computes the next enabled index from its own ordered `mArmed` (valid only in sequence mode). Random
   mode is therefore **out of v1**; if wanted, a public `peekSlot` on the Controller is the only
   correct route — which contradicts the prior "never modify the switcher" absolute, so v1 sidesteps
   it by not offering random.
3. **Gate producers cannot be "runtime-only / unserialized" while monitors persist against them.**
   `sceneData` serializes every occupied slot (`:3857-3859`) and the display cross-ref
   (`:4405-4406`) fails the whole scene on an unresolved `capture_id`. Fixed: the **program**
   producer is a *real serialized* capture (its `capture_id` resolves for `gate_source` monitors);
   only the **warm** slot is skipped. This preserves the prior draft's intended graceful degradation
   while making it actually load.
4. **`renderAuxiliaryView` early-returns before the prepare loop** (`:5743`, `:5757` on
   `hasDesignation()`, `:5761`). Prior §C placed `updateGate` "before `:5774`" without noting that a
   gate whose only source was a lazily-materialized producer would be gated out on frame one. Fixed:
   the program capture is created at **activation**, so `hasDesignation()` is already true and the
   `:5774` placement is safe.
5. **Continuous MANUAL preview-warm is not free.** Prior §D warmed the preview "whenever it is set",
   which is a permanent two-watched state → the live feed runs at half cadence forever (§1.3). Fixed:
   warm the preview **on-demand for a bounded burst** only.
6. **"FPS-cheap" framing.** As the prior draft's push-back correctly said, the engine *already*
   renders only watched cameras (`:2990`); the Gate **preserves** that property, it does not create a
   new saving. Retained and sharpened in §NO-FPS-DROP VERDICT.

---

## §Push-back on the request (retained, verified)

- **"Only the on-air camera renders" is already true of the engine** (`:2990`). The Gate's value is
  *routing and orchestration* and *not regressing* the zero-cost-for-idle guarantee — not making
  rendering cheaper.
- **Crossfade is not cheap here** (one aux refresh/frame, `:5784`); it is a composite/shader feature
  and is **cut from v1**. Hard cut only.
- **Per-camera dwell / jitter / random fight the reused model** and, worse, break gate-owned
  pre-warm (§D). Ship **global fixed interval, sequence order** in v1.
- **"Multiple gates" is impractical under `MAX_CAPTURES = 3`** (a gate needs 2 producers). Model an
  array for forward-compat but **ship one active gate**.
- **Reuse the switcher's brain, but wrap it** — do not reinvent the manual-take/auto-cycle/boundary
  logic (`ALDirectorSwitcherModel`), and do not pretend it can peek the future (it cannot).
