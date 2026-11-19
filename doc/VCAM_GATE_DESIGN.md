# VCAM GATE — Design Document (router model, implementation-ready)

**Feature:** "Vcam Gate" — a live-TV video switchboard / vision mixer for the prim-free
virtual cameras of the Prism engine. Many of the user's **existing** cameras **arm** to one Gate;
the Gate routes **one** of them to the subscribed monitors at a time (manual **TAKE** +
**auto-cycle**), like a security-camera switcher. The point: many cameras can be armed with **no FPS
cost**, because only the currently-watched source actually renders — and, crucially, **the gate adds
no captures of its own**. A 3-camera gate reads **3/8**, not 5/8.

**Status:** DESIGN ONLY — no source is modified by this document.
**Repo:** `I:\alchemy-machinima` (Alchemy-Machinima Second Life viewer fork).

> **THIS IS A REVISION.** The previously *shipped* gate materialized its **own** dedicated producers
> — a serialized "program" capture plus a transient "warm" capture (`mGateReserved` slots) — separate
> from the user's cameras. Those producers counted against the `MAX_CAPTURES` budget, so a 3-camera
> gate showed **5/8** and the Prism Manager had to print `"3 cams +2 gate = 5/8"` to explain the
> inflation. That model is **REJECTED**. This document redesigns the gate as a **pure router over the
> user's existing captures**: no program producer, no warm producer, no `mGateReserved`, no capacity
> competition, no "free a slot" starvation state. **Capture count == the user's actual cameras.**
>
> The switching *brain* — manual take + auto-cycle + cut timing — is still reused **verbatim** from
> the shipped, adversarially-tested `ALDirectorSwitcherModel::Controller`
> (`aldirectorswitchermodel.h:52-69`). Nothing in this revision touches that model.

Every claim about *current* code is cited `file:line`, re-verified against the live tree
(`MAX_CAPTURES` is now **8**). Design claims are labelled **PROVES** (verified in cited code),
**IMPLIES** (strong inference), **INFERS** (a reviewer/build must confirm). Line numbers are anchors;
re-resolve by symbol if a file has drifted.

---

## 0. One-paragraph thesis

The Prism engine **already renders only the cameras that are being watched** and nothing else — a
capture with no visible display bound to it is dropped to 0 Hz before any GPU work
(`llprismlens.cpp:3047-3054`). The Gate does **not** create captures, add a scheduler render pass, or
touch a shader. It is a thin *router* that, each frame, points the **gate-subscribed** monitor
displays at whichever of the **user's existing armed captures** is currently **on-air** (by rewriting
each subscribed display's `mCaptureSlot`/`mCaptureGeneration` to that capture). The existing
demand-driven admission engine then renders exactly the on-air camera and zeroes every other armed
camera for free. For a smooth cut, the Gate briefly injects a **bounded transient watch** on the
*next* armed camera's **own existing capture** for N frames before the cut — again with no new
producer and no reserved slot. The Gate's switching brain is the reused
`ALDirectorSwitcherModel::Controller`; the Gate reads its returned `Frame` for the current cut and
computes its own next-index/next-boundary for pre-warm, because the Controller exposes neither.

**What changed from the shipped code, in one line:** the on-air/warm cameras are no longer *copies*
the gate owns — they are the *user's own captures*, referenced by handle, routed to and demand-bumped
in place.

---

## STEP 1 — The Prism engine (substrate the Gate sits on)

### 1.1 Captures vs Displays: the two bounded arrays

- **PROVES.** `constexpr U32 MAX_CAPTURES = 8;` (`llprismlens.h:29`),
  `constexpr U32 MAX_DISPLAY_BINDINGS = 16;` (`llprismlens.h:30`), `MAX_LENSES` aliases
  `MAX_CAPTURES` (`llprismlens.h:31`). **`MAX_CAPTURES == 8` is the hard cap on simultaneous
  producers** — and in the router model it is *also* the cap on armable cameras, because an armed
  camera **is** an existing capture. This is the single most important number in this design.
- **PROVES.** Runtime capture = `struct PrismInstance` (`llprismlens.cpp:1586`); runtime display =
  `struct PrismDisplay` (`llprismlens.cpp:1625`). `mAnyDisplayVisible` (`:1601`) is the per-frame
  "is anyone watching me" flag.
- **PROVES.** The prim-free virtual-camera transform lives on `CameraSettings`: `mVirtual` /
  `mVirtualPos` / `mVirtualRot` (`llprismlens.h:242-244`). `addVirtualCamera()` allocates such a
  capture; it checks `count() >= MAX_CAPTURES` and takes a `freeCaptureSlot()`. **The user creates
  these; the gate never does.**

### 1.2 How a DISPLAY binds to a CAPTURE (the reference the Gate rewrites)

- **PROVES.** At runtime a display references its capture by **slot index**:
  `PrismDisplay::mCaptureSlot` (`llprismlens.cpp:1629`) plus a generation guard
  `PrismDisplay::mCaptureGeneration` (`llprismlens.cpp:1630`). Every gather of "the displays of slot
  *N*" filters on `display.mCaptureSlot == slot && display.mCaptureGeneration ==
  capture.mHandle.mGeneration` (e.g. `:2758-2759`).
- **PROVES.** In persistence, a display references its capture by **UUID**:
  `item["capture_id"] = mLenses[display.mCaptureSlot].mHandle.mId` (`llprismlens.cpp:4654`); on load
  a non-gate display's UUID must be non-null **and** resolve or the whole parse fails
  (`llprismlens.cpp:5133-5134`). *(This is the failure the router persistence sidesteps for
  gate-subscribed monitors — §H.)*
- **IMPLIES.** Because the runtime binding is a single mutable field (`mCaptureSlot`) re-read every
  frame, **the Gate routes a monitor by writing that field** to the *on-air armed camera's own slot*
  (and matching `mCaptureGeneration`). No new binding type, and no gate-owned producer, is needed.

### 1.3 The demand-driven admission scheduler (why "only watched cameras render" is free)

Per-frame driver: `renderAuxiliaryView()` (`llprismlens.cpp:5729`), one call per presented frame
(`llviewerdisplay.cpp` main display loop). Shape: `prepare` all slots → `chooseRenderSlot()` picks
**at most one** → render exactly that one, or return.

- **PROVES — the watched flag.** `PrismInstance::mAnyDisplayVisible` (`:1601`) is recomputed every
  frame inside `prepareCameraCapture()`: set false at entry (`:2747`), set true iff at least one
  *occupied, generation-matched* display bound to this slot passes its visibility test
  (`:2756-2768`).
- **PROVES — the admission gate (the hook the Gate reuses).** `updateCadenceEntitlements()` at
  `llprismlens.cpp:3007`: `if (!capture.mOccupied || !capture.mAnyDisplayVisible ||
  !capture.mFrame.mPrepared || now < capture.mRetryAfterTime) { mRequestedHz = 0; mEntitlementHz = 0;
  … continue; }` (`:3047-3054`). **An unwatched capture gets zero Hz and is never rendered.** The
  identical predicate re-gates selection in `chooseRenderSlot()` (`:3146-3150`).
- **PROVES — target sizing depends on being watched.** `prepareCameraCapture()` returns early with
  `false` and never sizes a render target if `!mAnyDisplayVisible` (`:2849-2852`); the canonical size
  is otherwise derived from the bound displays' visible footprints (`required_width/height`,
  `:2756-2768`). *(This is why transient pre-warm needs a seed size — §D.)*
- **PROVES — one render per frame, round-robin.** `chooseRenderSlot()` scans
  `(mNextRenderSlot + offset) % MAX_CAPTURES` (`:3142-3144`) and returns a single best slot;
  `renderAuxiliaryView` renders only that one. **Two watched cameras do not double per-frame GPU cost
  — they interleave frames**, sharing the single aux render slot. Load-bearing for the pre-warm cost
  claim (§D).
- **PROVES — fair sharing.** More than one watched capture → bounded max-min water filling splits the
  global budget (`:3062-3088`).

**Design consequence (IMPLIES).** The complete recipe for "make camera X render and nothing else"
already exists and runs every frame: *ensure exactly one visible display is bound (`mCaptureSlot`) to
X, and no visible display is bound to any other camera.* **The Gate's whole job is to keep that
invariant pointed at the on-air camera** and briefly extend it to the pre-warm camera. The Gate
layers on `mAnyDisplayVisible`; it must not touch the water-filling math or `chooseRenderSlot`.

### 1.4 Runtime-only routing writes (no config churn)

- **PROVES.** Runtime-only transform/binding writes bump `mRuntimeRevision`, not `mRevision`
  (mirroring `setVirtualCameraTransform`). **The Gate re-points subscribed monitors and injects
  transient demand through this same runtime-only discipline** — it never edits the user's persisted
  `CameraSettings` and never bumps the configuration revision for a routine cut. It bumps the
  gate-local `mGate.mGateRevision` so the UI on-air readout refreshes.

### 1.5 Persistence (the round-trip the Gate rides)

- **PROVES.** `sceneData()` (`llprismlens.cpp:4537`) emits `prism_captures`, `prism_displays`, and
  `prism_gates` arrays. It iterates `mLenses` and serializes every `mOccupied` capture. Displays
  serialize `binding_id`, `capture_id`, `gate_source`, and geometry (`:4652-4721`).
- **PROVES — load caps + cross-ref.** `applySceneData()` (`:4745`) parses/validates atomically,
  committing only after full cross-reference. A **non-gate** display whose `capture_id` is null or
  unresolved fails the whole parse (`:5133-5134`). **This is exactly why the shipped model needed a
  serialized program producer** — and exactly what the router model eliminates by making a
  gate-subscribed monitor reference *the gate*, not a capture id (§H).
- **PROVES — additive precedent, no version bump.** `bone_pov`, `screen_effects`, `virtual*` are all
  written unconditionally and read *optionally*; none bumped a scene version.
- **PROVES — the Director scene wrapper.** `LLFloaterDirector::saveScene` sets
  `scene["version"] = SCENE_VERSION;` where `SCENE_VERSION` is **4**, and lifts the Prism arrays
  (including `prism_gates`) to the top level.

### 1.6 The Prism Manager UI (where the Gate UI lives)

- **PROVES.** Floater `floater_prism_manager.xml` with a `capture_list` (`mCaptureList`), rebuilt in
  `rebuildCaptureList()`, a `displays_tab`, and a `gate_tab`. Poll/refresh model: `draw()` calls
  `pollSnapshots(false)` each frame, throttled to 0.25 s, refreshing on revision change; every
  mutation calls `invalidateRegistrySnapshot()`.
- **PROVES — the count/status text today.** `llfloaterprismmanager.cpp:942-957` counts
  `mGateReserved` captures and prints `"N cams +G gate = T/MAX"` to explain the inflation, because
  `rebuildCaptureList` hides gate producers (`:980`). **Both go away in the router model** (§G):
  there are no reserved producers to hide or explain.

### 1.7 The switcher precedent (the Gate's scheduling brain) — reused verbatim

- **PROVES.** `ALDirectorSwitcherModel::Controller` public surface is exactly six members
  (`aldirectorswitchermodel.h:52-69`): default ctor; `Frame update(F64 now, const Config&)`;
  `bool manualPunch(S32 slot, F64 now)`; `bool rebase(F64 now)`; `void reset()`;
  `S32 activeSlot() const`. **There is NO `peekNextSlot`** — selectors are private.
- **PROVES — the `Frame` result:** `bool mCut`, `S32 mSlot`, `F64 mBoundary`, `U64 mEventIndex`,
  `U64 mEventsElapsed`. **`mBoundary` is the time the JUST-FIRED cut occurred (≤ now), NOT the next
  cut** (`aldirectorswitchermodel.cpp` `eventBoundary`).
- **PROVES — sequence mode** is a deterministic round-robin over the enabled slots in bank order.
- **PROVES — already adversarially unit-tested** (`tests/aldirectorswitchermodel_test.cpp`, 15
  tests). The shipped gate already drives this correctly; **the router revision keeps that call site
  byte-identical** and changes only what happens *after* the `Frame` is returned (route instead of
  reprogram-a-producer).

**Design consequence (IMPLIES).** The Gate is the **display-router analog** of `ALDirectorSwitcher`:
where the adapter re-points the single viewer camera, the Gate re-points the prism monitors'
`mCaptureSlot` at the user's on-air capture. Reuse the `Controller` for the current cut and manual
take; compute pre-warm (next index / next boundary) in the Gate itself.

---

## STEP 2 — The router model (A–H)

### A. The Gate model — what it is and where it lives

A `PrismGate` lives inside `PrismLensRegistry` (members `mGateSettings` + `PrismGateRuntime mGate`) so
it can read/write `mLenses[]`/`mDisplays[]` directly. Its scheduling brain is the reused
`ALDirectorSwitcherModel::Controller`; its **only** new logic is: resolve armed refs → live captures,
route subscribed monitors, and inject transient pre-warm demand.

**Serializable shape (the router-model `GateSettings`).** The one structural change from the shipped
struct is that an armed entry references an **existing capture by handle id**, and the gate no longer
stores a `mProgramCaptureId` (there is no program producer):

```cpp
// llprismlens.h  (public, pure-data, pointer-free)
enum class EGateMode : U8 { MANUAL, AUTO_CYCLE };

struct GateArmedCamera            // a REFERENCE to one of the user's existing captures
{
    LLUUID       mArmId;          // stable identity for this armed row (UI + persist)
    std::string  mLabel;          // operator-facing name ("Wide", "OTS", …)
    bool         mEnabled = true; // maps 1:1 to Controller Config.mEnabled[i]
    LLUUID       mCaptureId;      // handle id of the user's EXISTING capture  <-- was: CameraSettings mCamera
};

struct GateSettings               // serializable
{
    LLUUID       mGateId;
    bool         mActive          = false;
    EGateMode    mMode            = EGateMode::MANUAL;
    F64          mIntervalSeconds = 8.0;   // = DEFAULT_INTERVAL_SECONDS
    U32          mPrewarmFrames   = 3;     // §D; 0 disables pre-warm (accept 1 stale frame)
    std::vector<GateArmedCamera> mArmed;   // ORDERED; cap = min(MAX_CAPTURES, SLOT_COUNT) = 8
    S32          mProgramArmIndex = 0;     // on-air index into mArmed (MANUAL authority)
    S32          mPreviewArmIndex = -1;    // MANUAL preview candidate for TAKE
    // REMOVED: LLUUID mProgramCaptureId; — there is no program producer to serialize.
};
```

**Runtime shape (`PrismGateRuntime`) — most of the shipped fields are DELETED:**

```cpp
struct PrismGateRuntime
{
    ALDirectorSwitcherModel::Controller mController;  // reused brain (current cut + manual take)
    S32  mOnAirArmIndex = -1;   // resolved this frame -> its mCaptureId -> a live slot
    S32  mWarmArmIndex  = -1;   // gate-computed next index being transient-warmed (-1 when idle)
    F64  mNextCutTime   = 0.0;  // gate-computed (jitter=0): lastBoundary + mIntervalSeconds
    U64  mCutSerial     = 0;    // bumps on every cut (drives UI on-air indicator)
    U64  mGateRevision  = 1;    // UI-poll signature for gate state (see §G)
    U32  mManualWarmFramesRemaining = 0; // bounded MANUAL preview-warm burst (§D)
    bool mPendingTake   = false;
    std::string mReason;
    // REMOVED: mProgramSlot, mWarmSlot, mWarmLoadedArmIndex — the gate owns NO producer slot.
};
```

**Additive engine-struct fields — reduced to the minimum.**

- `PrismInstance` (`llprismlens.cpp:1586`): the transient-watch inputs, applied to the **user's own
  capture** while it is the next-to-cut camera:
  `U32 mGateWatchFrames = 0;` (countdown; >0 means "admit me as watched this frame"),
  `U32 mGateWatchW = 0, mGateWatchH = 0;` (the §D admission seed size).
  **REMOVED from the shipped struct:** `mGateReserved`, `mGateWarm`, `mGateWarmW`, `mGateWarmH`,
  `mGateSourceRevision`, `mGatePublishedSourceRevision`. (`mGateWatchFrames/W/H` replace the
  `mGateWarm*` trio but live on a *real user capture*, not a reserved producer, and self-expire.)
- `PrismDisplay` (`llprismlens.cpp:1625`): `bool mGateSubscribed = false;` — **UNCHANGED, kept as-is**
  (`:1631`). This is the per-display "gate-subscribed" flag the router model is built on.

- **ONE global gate for v1.** A router gate needs **zero** dedicated producers, so the old
  "2 producers won't fit twice" argument is gone; the reason to keep v1 single is only UI/authority
  simplicity (one on-air bus). Model `prism_gates` as an array for forward-compat; enforce one active
  gate in v1.
- **How a MONITOR subscribes.** A monitor is an ordinary `PrismDisplay` with `mGateSubscribed =
  true`. Each frame, inside `updateGate` (specifically `routeGateDisplays`), the Gate writes every
  subscribed occupied display's `mCaptureSlot` = *the on-air armed camera's resolved slot* and
  `mCaptureGeneration` = that capture's generation. Non-subscribed displays are byte-identical to
  today.

### B. Camera count — armed == existing captures, and the exact caps

`MAX_CAPTURES = 8` (`llprismlens.h:29`) caps simultaneous producers. **In the router model an armed
camera IS a producer** (the user's own capture), so:

| Quantity | Value | Source |
|---|---|---|
| User cameras that exist as **captures** (hard, engine) | **8** = `MAX_CAPTURES` | `llprismlens.h:29` (PROVES) |
| Armed camera **references** (a subset of the above) | ≤ 8 (also ≤ `SLOT_COUNT`=12, so **8** binds) | this design (IMPLIES) |
| Gate **producer footprint** while active | **0** dedicated (references existing captures) | §A (this revision) |
| Producers actually **rendered** per frame | **1** (round-robin) | `llprismlens.cpp:3142-3144` (PROVES) |

**The count the user sees == the user's cameras.** A 3-camera gate means 3 user captures exist and 3
are armed → **`3/8`**. There is no `+G` inflation term. Arming does not allocate a capture; it merely
records `mCaptureId` of a capture the user already made. Disarming does not free a capture; it removes
the reference. **Deleting a camera** (the user removing the capture) simply drops it from the armed
set (its `mCaptureId` no longer resolves — handled best-effort, §Edge Cases).

**Why armed count no longer raises render cost, and why it is bounded by 8 not 12.** In the shipped
model armed defs were free-floating `CameraSettings` bytes, so up to 12 could be armed while only ≤2
producers existed. In the router model, arming references an existing capture, and only 8 captures can
exist at once — so the practical arm cap is **8**. This is the deliberate, user-requested trade: the
budget is spent *only* on real user cameras, and the count is honest. Render cost is still bounded by
§1.3 to **one** rendered camera per frame regardless of how many are armed.

### C. The switch / on-air routing (reuse, don't duplicate)

Each frame, `PrismLensRegistry::updateGate(now)` runs at the top of the prepare region of
`renderAuxiliaryView()` (existing call site `llprismlens.cpp:7062`), before the prepare loop.

1. **Advance the brain (current cut).** Build `ALDirectorSwitcherModel::Config` from `GateSettings`
   exactly as the shipped code already does (`llprismlens.cpp:4219-4228`): `mAuto = (mMode ==
   AUTO_CYCLE)`, `mSequence = true`, `mIntervalSeconds`, **`mJitterSeconds = 0.0` (forced)**,
   `mEnabled[i] = mArmed[i].mEnabled`. Call `Frame f = mController.update(now, cfg)`. Use `f.mSlot`
   as the on-air arm index and `f.mCut` as the cut-this-frame flag. **Do NOT read `f.mBoundary` as a
   future time.**
2. **Resolve on-air to a live slot — NO materialization.** `mOnAirArmIndex = f.mSlot` (MANUAL:
   `mProgramArmIndex`). Resolve `mArmed[mOnAirArmIndex].mCaptureId` to a live capture slot via
   `findCaptureById`. If it resolves, that slot **is** the on-air producer — the user's own camera.
   If it does not resolve (camera deleted), advance to the next enabled armed entry whose id resolves,
   or go dark (§Edge Cases). **There is no reprogram, no swap, no producer allocation.** On a cut,
   bump `mCutSerial` and `mGateRevision`.
3. **Route monitors** (`routeGateDisplays`, rewritten). For every occupied display with
   `mGateSubscribed == true`, set `display.mCaptureSlot = onAirSlot` and `display.mCaptureGeneration =
   mLenses[onAirSlot].mHandle.mGeneration`, decrementing/incrementing `mDisplayCount` on the old/new
   capture as the shipped `routeGateDisplays` already does (`:5839-5847`). The only change from the
   shipped function: the target slot is *the resolved on-air user capture*, not `mGate.mProgramSlot`.
4. **Let the engine do the rest.** The prepare loop sets `mAnyDisplayVisible = true` on the on-air
   capture (a visible subscribed display now binds it) and `false` on every other camera.
   `updateCadenceEntitlements` (`:3047`) zeroes idle cameras; `chooseRenderSlot` (`:3146`) renders
   only the watched one(s).
5. **Compute the next cut for pre-warm (gate-owned).** With `mJitterSeconds == 0`,
   `mNextCutTime = gateNextCutTime(f.mBoundary, mIntervalSeconds)` on any frame where `f.mCut`
   (reusing the shipped free helper at `:4239-4240`). The **next arm index** is the next enabled entry
   after `mOnAirArmIndex` in `mArmed` order (`gateNextEnabledIndex`, `:4300-4301`).

The Gate writes **no** Hz, **no** entitlement, and calls **nothing** in the water-filling / selection
math. It owns `mCaptureSlot`/`mCaptureGeneration` on subscribed displays and the §D transient-watch
input.

### D. Pre-warm (transient watch on the user's existing capture — no reserved producer)

**Problem (PROVES).** A cut to an unwatched camera can show a stale/black frame: an unwatched camera
is never sized (early `return false` at `:2849-2852`) and, if it was previously suppressed, released
its output. So re-pointing the monitors at the cut instant can yield **one stale frame** while the new
framing renders for the first time.

**Router-model solution — inject bounded transient demand onto the next camera's OWN capture.** The
key realization: the next armed camera **already has a capture** (it is a user camera). We do not need
a warm *producer*; we need the existing admission engine to render that already-existing capture for a
few frames even though no monitor is bound to it yet. The shipped admission hook does exactly this and
is **retained almost unchanged**, only relocated conceptually from "a reserved warm slot" to "the real
next-armed capture."

- **Window predicate:** open the warm window when
  `gateWarmActive(now, mNextCutTime, mPrewarmFrames, frameDT)` is true (reused free helper,
  `:4304-4306`), where `frameDT = 1/gFPSClamped`. On entry, set `mWarmArmIndex` = next enabled arm
  index, resolve its `mCaptureId` to a live slot, and on **that user capture** set
  `mGateWatchFrames = mPrewarmFrames`, `mGateWatchW = mGateWatchH =` the on-air capture's current
  `mOutputWidth/Height` (`:1609-1610`) — the warm camera inherits the same monitors at the cut, so
  that is the correct target size. If the next `mCaptureId` does not resolve, skip pre-warm for this
  cut (best-effort).
- **Sanctioned minimal admission hook — UNCHANGED from the shipped block, retargeted.** The shipped
  code already contains exactly the needed guarded block at the top of `prepareCameraCapture`,
  immediately before the `if (!capture.mAnyDisplayVisible) return false;` gate (`:2842-2848`):

  ```cpp
  if (!capture.mAnyDisplayVisible && capture.mGateWatchFrames > 0 &&
      capture.mGateWatchW > 0 && capture.mGateWatchH > 0)
  {
      capture.mAnyDisplayVisible = true;             // admit as if watched
      required_width  = llmax(required_width,  capture.mGateWatchW);
      required_height = llmax(required_height, capture.mGateWatchH);
  }
  ```

  This is a **rename** of the shipped `mGateWarm/mGateWarmW/mGateWarmH` block (`:2842-2848`) to
  `mGateWatchFrames/mGateWatchW/mGateWatchH` on a *real* capture. Everything downstream — target
  sizing, `mFrame.mPrepared`, the water-fill, `chooseRenderSlot` — is byte-identical. It layers an
  admission input; it does not rewrite the scheduler. It is a strict no-op when no gate is warming
  (`mGateWatchFrames` defaults 0). **The countdown is decremented once per frame** in `updateGate`
  (reusing the shipped `gateConsumePreviewWarmFrame`-style decrement, `:4287-4288`), so the transient
  demand self-expires with no explicit teardown — this is why a countdown is used instead of a bool.
- **At the cut:** `f.mCut` fires; the next camera's own capture already holds a `CURRENT`/`HELD`
  fresh frame from the warm window; `routeGateDisplays` re-points the monitors to that capture's slot;
  the old on-air camera loses its visible display next frame and is zeroed for free; its residual
  `mGateWatchFrames` (if any) is already 0. **No stall**, because the incoming output was produced
  during the window — and often the incoming capture still has a valid **HELD** output from the *last*
  time it was on-air, which is an extra safety net the shipped fresh-warm-producer model did not have.
- **MANUAL pre-warm (bounded burst).** Warm the **preview** (`mPreviewArmIndex`)'s own capture only
  for `mManualWarmFramesRemaining` frames right after the operator selects a preview, then let the
  countdown expire. Do not hold the preview warm continuously (two continuously-watched producers
  permanently share the single aux slot, halving the live feed's refresh). A manual TAKE re-arms the
  burst and cuts only when the preview capture is `CURRENT`. This mirrors the shipped MANUAL logic
  (`:4256-4294`) exactly, minus the reserved warm slot.

### E. Transitions — CUT only for v1 (crossfade deferred)

- **Default & only v1 transition: HARD CUT.** The monitor swaps its bound capture slot
  (`mCaptureSlot`/`mCaptureGeneration`, step C.3) at the cut frame. No blending path exists or is
  needed — the display composite samples whatever slot the monitor points at.
- **CROSSFADE: defer.** A genuine crossfade needs *both* feeds refreshed on the same composite frame,
  but the engine refreshes **one** auxiliary slot per frame. It is a display-composite / shader
  feature and is OFF-LIMITS here.

### F. Auto-cycle timing — verdict: WRAP the model, don't extend it

- **Reuse `ALDirectorSwitcherModel::Controller` for cut timing and manual take (verbatim, no edit).**
  Build a `Config` each frame, call `update(now, cfg)` for the current cut and `manualPunch(slot,
  now)` for TAKE. UI range 0.5–120 s, default 8 s.
- **What the Gate owns:** the **next** slot and **next** boundary for pre-warm (the model exposes
  neither). Compute from the Gate's ordered arm list + `lastBoundary + interval` (valid because the
  Gate forces `mSequence = true`, `mJitterSeconds = 0`).
- **Jitter/random are out of v1.** Ship global fixed interval, sequence order.
- **The tick runs on the existing per-frame Prism update** (`updateGate(now)` at `:7062`, clock
  `LLTimer::getTotalSeconds()`). Large frame gaps are absorbed by the model's "advance to final event,
  at most one cut" guarantee. **This call site is unchanged from the shipped code.**

### G. UI — the existing "gate" tab, minus the inflation apology

The `gate_tab` already exists (`floater_prism_manager.xml`) with an armed list, arm/disarm, TAKE,
mode combo, interval/prewarm spinners, an on-air indicator, and a per-display *Gate* source checkbox
(`mDisplayGateSourceCheck`, `cpp:425`, `:1291-1297`). The router model **simplifies** it:

- **Arming references the selected existing capture.** `gate_arm` records the selected
  `capture_list` row's **capture id** as a new `GateArmedCamera.mCaptureId` (instead of snapshotting
  its `CameraSettings` — the shipped `gateArm` copies `armed.mCamera = capture.mCamera` at
  `llprismlens.cpp:3896`; the router version stores `armed.mCaptureId = capture.mHandle.mId`). The
  "arms must be prim-free virtual cameras … cannot be snapshotted without losing source identity"
  guard (`:3880-3884`) is **no longer needed for snapshot reasons** — a reference keeps identity — but
  keep a simpler guard that an armed capture must be a `CAMERA_FEED`.
- **The armed list resolves labels/on-air live** from the referenced capture each poll; a row whose
  `mCaptureId` no longer resolves is shown greyed as "(deleted)" and is skipped by routing.
- **The capture list shows ALL captures.** Delete the `if (capture.mGateReserved) continue;` skip in
  `rebuildCaptureList` (`cpp:980`) and the `mGateReserved` checks in selection/enable logic
  (`cpp:869-884`, `:1398`) — there are no reserved captures to hide. An armed capture is an ordinary,
  fully-editable camera row (editing it live changes what the gate routes — desirable).
- **Status/count text becomes plain.** Replace the `"N cams +G gate = T/MAX"` branch
  (`cpp:942-957`) with the simple `"%u/%u captures"` form for all cases. The on-air arm still shows in
  the gate tab, driven by `mGate.mGateRevision` / `mCutSerial` through the 0.25 s poll.
- **Refresh discipline** unchanged: every Gate mutation calls `invalidateRegistrySnapshot()`.

### H. Persistence — the monitor subscribes to THE GATE, not a capture id

**The corner the shipped model dodged with a dedicated program producer, solved cleanly.** The shipped
model made the program a *real serialized capture* purely so a subscribed monitor's `capture_id`
would resolve at load (`:5133-5134`) and re-adopted it via `program_capture_id` (`:5385-5437`). The
router model removes the producer, so it must make the monitor **not depend on any capture id**:

1. **A gate-subscribed monitor serializes `gate_source: true` and does NOT require a resolvable
   `capture_id`.** In `sceneData`, a subscribed display writes `gate_source = true` and either omits
   `capture_id` or writes a null one (`:4654-4655`; today it writes the program id — change to
   null-for-gate-source). In `applySceneData`, **relax the cross-ref** at `:5133-5134`:

   ```cpp
   if (!parsed.mGateSubscribed &&
       (parsed.mCaptureId.isNull() || !capture_ids.count(parsed.mCaptureId)))
       return fail("A Prism display references an unknown capture.");
   // gate_source displays carry no fixed capture reference — the gate resolves them each frame.
   ```

   A gate_source display loads with `mCaptureSlot = MAX_CAPTURES` (the unbound sentinel) and
   `mGateSubscribed = true`; the first `updateGate` binds it to the resolved on-air slot. **No dangling
   `capture_id`, so the atomic-load failure at `:5133-5134` can no longer be triggered by the gate.**
   The old program-producer re-adoption block (`:5385-5437`) is **deleted**.
2. **The gate serializes its armed set as capture references.** Each `armed` entry writes
   `{ arm_id, label, enabled, capture_id }` (the referenced capture's id) instead of a full inlined
   `camera:{…}` block. On load, each armed `capture_id` is resolved against the just-parsed
   `prism_captures`; **an unresolved armed entry is skipped (best-effort), never fatal**
   (the `gateSettingsFromScene` helper already tolerates a malformed gate by discarding it,
   `:4791-4798`; extend it to per-arm skip). `mProgramCaptureId` is **removed** from the serialized
   form and from the re-adopt path.
3. **New top-level `prism_gates` array** — already emitted (`:4542`) and parsed optionally. One lift
   line already exists in `saveScene`. **`SCENE_VERSION` stays 4** — the change is a field swap inside
   an already-additive, already-optional block (armed entries carry `capture_id` instead of `camera`;
   `gate_source` displays carry no capture ref). No new top-level shape, no removed *required* key
   that an old viewer needs. A justification for not bumping: old viewers already ignore `prism_gates`
   wholesale and load gate_source displays as ordinary displays — see compatibility below. **INFERS:**
   a build must confirm old-viewer load does not choke on a `gate_source`-only display that lacks a
   `capture_id`; if it does (old viewers ran the strict `:5133-5134` check), then a **defensive
   `capture_id` = the on-air camera's id may be written for forward-compat** while the *new* loader
   ignores it for gate_source rows. This is the one spot where a reviewer must pick between "cleanest"
   (omit id, no bump) and "old-viewer-safe" (write a resolvable id, still no bump). Recommend writing
   the on-air camera's id as a compatibility hint (it resolves in both viewers), while the new loader
   treats `gate_source` as authoritative and re-routes each frame.
4. **On load:** parse captures/displays first (unchanged); then, if `prism_gates` present, adopt
   `mGateSettings`, resolve each armed `capture_id`, resolve `mOnAirArmIndex`'s capture,
   `mController.reset()` + `manualPunch(onAir, now)`, and let the first `updateGate` route the
   gate_source displays. **No producer to allocate, no `ensureGateWarmProducer`, no `reprogramGateSlot`.**

**Compatibility:**
- **Old scene → new binary:** no `prism_gates` → empty inactive gate; every display is a fixed
  binding. Identical to pre-gate behavior.
- **New scene → old binary:** `prism_gates` ignored; gate_source displays load as ordinary displays
  bound to the compatibility `capture_id` (the last on-air camera) → the old viewer shows a **static
  last-on-air feed** — graceful degradation, and it does not trip `:5133-5134` because the
  compatibility id resolves. *(If the "omit id" variant is chosen instead, an old viewer would reject
  the scene — hence the recommendation to write the hint id.)*

---

## §FRAME-BY-FRAME LIFECYCLE

Assume AUTO_CYCLE, the user has three captures A, B, C, all armed and enabled, `mIntervalSeconds = 8`,
`mPrewarmFrames = 3`, a monitor M subscribed. On-air = A. Times relative to the last cut at `t=0`;
`frameDT ≈ 1/60`.

| Frame / time | `updateGate` action | Watched set | Aux renders this frame |
|---|---|---|---|
| steady (0 ≤ t < 8 − 3·dt) | route M→A's slot; no watch counters set | {A} | **A only** (1 render) |
| pre-warm opens (t ≈ 8 − 3·dt) | nextIndex=B; resolve B's slot; set `mGateWatchFrames(B)=3`, seed W/H from A's `mOutputWidth/H` | {A, B} | A and B **interleave** — 1 render/frame, each ~½ cadence for 3 frames |
| pre-warm frames 2–3 | decrement `mGateWatchFrames(B)`; B accrues a `CURRENT` frame | {A, B} | 1 render/frame (round-robin) |
| **cut** (`f.mCut`, t ≈ 8) | `mOnAirArmIndex=B`; route M→B's slot; A loses its visible display; `mGateWatchFrames(B)` hits 0 | {B} | **B only** (B already fresh → no stall) |
| steady again | `mNextCutTime = f.mBoundary + 8` | {B} | B only |

**On a MANUAL TAKE** (operator selects preview = C): the Gate sets `mGateWatchFrames(C's slot) =
mPrewarmFrames`; when C's capture is `CURRENT`, `gateTake()` calls `manualPunch(Cindex, now)`, routes
M→C's slot, interval restarts. If `mPrewarmFrames == 0`, the route happens immediately and M shows one
stale/HELD frame while C renders first — the explicit "accept a stale frame" option (often masked by
C's HELD output if C was recently on-air).

**Guarantee of no FPS drop with all cameras armed:** at every row, **exactly one** aux view is
rendered per presented frame regardless of how many cameras are armed; a non-on-air, non-warming armed
camera has a capture but no watching display and is zeroed at `:3047`/`:3146`.

---

## §NO-FPS-DROP VERDICT — proven against the admission gate

**Claim:** arming N of the user's cameras costs the same peak frame time as one camera, and the gate
adds **zero** captures.

**Proof (PROVES, router model).**
1. **On-air camera renders.** M (`mGateSubscribed`) is routed to A's slot (`routeGateDisplays`), so in
   `prepareCameraCapture` A's `mAnyDisplayVisible` is set true by the visible-display loop
   (`:2756-2768`). A is admitted (`:3047` predicate passes) and eligible for selection (`:3146`).
2. **Every other armed camera is zeroed.** B, C have **no** subscribed (or fixed) visible display
   bound to them — M points only at A. So their `mAnyDisplayVisible` stays false (`:2747`, never set),
   and the admission gate forces `mRequestedHz = mEntitlementHz = 0` and `continue`s
   (`:3047-3054`); `chooseRenderSlot` skips them by the identical predicate (`:3146-3150`). **They do
   zero GPU work.** This holds for any number of armed cameras up to `MAX_CAPTURES`.
3. **One render per frame.** `chooseRenderSlot` returns at most one slot (`:3142-3144`);
   `renderAuxiliaryView` renders exactly that one. Peak per-frame aux GPU work is one camera.
4. **The gate adds no capture.** Armed entries are references to existing captures; `count()` is
   unchanged by arming. **`count()` == the user's cameras**, so the status text reads `N/8`.

**Honest transient (not an FPS drop):** during the ~`mPrewarmFrames`-frame warm window, two captures
(on-air + next) are watched, so they interleave through the single aux slot — the live feed's
*refresh rate* roughly halves for those few frames, and one transient extra scratch target of
`mGateWatchW×H` is live. **Peak per-frame GPU does not rise; only the live camera's update cadence
dips briefly.** Steady state is exactly one rendering camera.

**Where the shipped model's failure modes GO AWAY entirely:** there is no reserved warm slot to
starve, so the shipped "Gate running without pre-warm — free a capture slot" state
(`:4247-4252`) and the whole `reclaimGateWarmSlotForUser` capacity-competition machinery
(`:1841`, `:1889`, `:1926`, `:2235-2242`) are **deleted**. Pre-warm is skipped only in the trivial
case where the next camera's `mCaptureId` fails to resolve (deleted mid-run), which is a best-effort
skip, not a capacity fight.

---

## §EDGE CASES

1. **Gate with 0 armed.** `updateGate` is a no-op: `mController.update` returns no cut (empty
   `mEnabled`). Subscribed monitors keep their last valid binding, or, if none was ever established,
   remain unbound (`mCaptureSlot = MAX_CAPTURES`) — the engine tolerates an unbound display (it simply
   matches no capture). No −1/invalid slot is written.
2. **On-air camera deleted mid-run.** The user removes capture A while it is on-air.
   `mArmed[mOnAirArmIndex].mCaptureId` no longer resolves via `findCaptureById`. Resolution: advance to
   the next enabled armed entry whose `mCaptureId` resolves and route to it (a forced cut); if **none**
   resolves, **go dark** — leave subscribed monitors unbound (`mCaptureSlot = MAX_CAPTURES`) and set
   `mGate.mReason = "Gate dark: no armed camera is available."`. Never leave a display pointing at a
   freed slot. Also prune the dead arm from `mArmed` (or mark it unresolved and skip). This is the
   clean replacement for the shipped `deactivateGate`-on-missing-program-producer path
   (`:4202-4209`).
3. **Monitor subscribed while the gate has 0 armed.** The subscribe succeeds, `mGateSubscribed =
   true`, but routing binds nothing this frame (unbound sentinel) → the monitor shows nothing, cleanly
   (no crash, no stale slot). As soon as a camera is armed and on-air, the next `updateGate` binds it.
   *(Note: the shipped `setDisplayGateSubscribed` currently REQUIRES an active gate with a program
   producer before allowing subscribe — `:4111-4118`. In the router model that precondition is
   **removed**: subscribe to the gate at any time; routing is resolved live.)*
4. **Feedback loop — a monitor cannot be armed as its own source.** Cameras (`GateArmedCamera` →
   `CAMERA_FEED` capture) and monitors (`PrismDisplay`) are different objects, so a direct type loop is
   impossible. The logical guard to keep: **forbid a `gate_source` display from being the display
   object of any armed camera's source**, and forbid arming a capture whose in-world source object is
   itself a gate-routed display's face — reusing the existing "a camera source cannot also be one of
   its display objects" duplicate guard (`:5313-5317`) and enforcing it in `gateArm` /
   `setDisplayGateSubscribed` (return `INVALID_CONFIGURATION` with a reason surfaced via
   `setActionState`). In-world *optical* feedback (a virtual camera framing a virtual screen) remains
   tolerated by the existing 1-frame-lagged recursive mirror path; no new loop is introduced.
5. **Armed capture edited live.** Because arming is a *reference*, editing the armed camera in the
   capture editor immediately changes what the gate routes when that camera is on-air — this is
   correct and desirable (no snapshot drift, unlike the shipped copy model).
6. **Save/load a scene with a gate.** Round-trips per §H: armed set as `capture_id` refs; gate_source
   monitors carry no authoritative capture ref (plus an optional compat id); best-effort per-arm skip;
   old viewers degrade to a static last-on-air feed.

---

## §BONE-POV ORTHOGONALITY (confirmed, and simpler now)

`ALVCamBonePov::tick` enumerates registry captures, reads `capture.mCamera.mBonePov`, and drives any
enabled one via the runtime-only writers. In the router model the on-air camera **is** the user's own
capture, so bone-POV already drives it with **zero** gate plumbing — there is no gate-owned copy to
keep in sync (the shipped model had to copy `mBonePov` into the materialized program producer; that
copy is **gone**). The Gate must not touch `ALVCamBonePov`, `CameraSettings::mBonePov`, or
`normalizeBonePovJointSelection`.

---

## §TEST PLAN — pure, non-tautological, regression-failing

All pure (no viewer singleton, no GL), mirroring `aldirectorswitchermodel_test.cpp`. Factor the Gate's
pure helpers as free functions; extend the existing gate test file.

1. **Cycle-advance / next-index mapping** (armed `{A,B,C}`, sequence, interval T): `update(t0)`→A;
   `update(t0+T−ε)`→A no cut; `update(t0+T+ε)`→B cut; wrap. Single armed camera: never cuts. Empty
   armed set: no-op. Large jump: exactly one cut, deterministic final slot. **On-air capture deleted:**
   resolve advances to the next resolvable arm; all deleted → dark (unbound monitors, no −1 slot).
2. **Next-cut-time + transient-watch window:** with `mJitterSeconds == 0`, `gateNextCutTime(f) ==
   f.mBoundary + T`; `gateWarmActive` false before `nextCutTime − N·dt`, true within. Warm target index
   == the Gate's next enabled arm index (assert it equals the Controller's next cut slot). Watch seed
   == on-air `mOutputWidth/H`; a 0×0 seed yields **no** admission (guards the `mGateWatchW>0` block).
   **Assert the Gate never reads `f.mBoundary` as a future time.** **Assert `mGateWatchFrames`
   self-expires** after exactly N frames (regression guard against a stuck permanent watch).
3. **Manual-take state machine:** take with no preview → no cut; preview == on-air → no cut; preview ≠
   on-air → exactly one cut, on-air←preview, interval restarts. Auto→Manual freezes on current on-air.
   Bounded preview-warm: `mGateWatchFrames` true for exactly `mPrewarmFrames` frames then 0.
4. **Persistence round-trip + cross-version:** `GateSettings` → LLSD → parse → equal (armed order,
   labels, enabled, mode, interval, prewarm, program_arm_index, per-arm `capture_id`). **A gate_source
   display with null/absent `capture_id` LOADS (does not trip `:5133-5134`)** — the central router
   assertion. A non-gate display with null/dangling `capture_id` still **fails** (proves the relaxation
   is scoped to gate_source). Dangling armed `capture_id` → that arm dropped, scene still loads.
   Old scene (no `prism_gates`) → empty inactive gate.
5. **Feedback-loop guard:** arming/subscribing that would make a gate-routed display the source object
   of an armed camera returns `INVALID_CONFIGURATION`.

---

## §IMPLEMENTATION PLAN (ordered) — what to DELETE, what to add

This is a *reduction* of the shipped gate. The net line count must go **down**.

### Pass 1 — Data model (reference, not copy)
- `llprismlens.h`: change `GateArmedCamera::mCamera` (`:266`, a `CameraSettings`) to
  `GateArmedCamera::mCaptureId` (an `LLUUID`). **Remove** `GateSettings::mProgramCaptureId` (`:279`).
- Keep `EGateMode`, `GateSettings` (minus that field), `GateSnapshot`, and all six gate method decls
  (`gateArm`, `gateDisarm`, `gateTake`, `setDisplayGateSubscribed`, `gateSnapshot`, `setGateSettings`).
- Keep the pure free helpers (`gateNextEnabledIndex`, `gateNextCutTime`, `gateWarmActive`,
  `gateIsRealCut`, `gateClampEnabledIndex`, `gateSettingsToLLSD`, `gateSettingsFromScene`); adjust
  their LLSD to armed-`capture_id`. **Remove** `gateDisplayCaptureReferenceValid` (no program id to
  validate against).

### Pass 2 — Registry glue (delete the producer pool)
**DELETE from `llprismlens.cpp`:**
- `PrismInstance` fields `mGateReserved`, `mGateWarm`, `mGateWarmW`, `mGateWarmH`,
  `mGateSourceRevision`, `mGatePublishedSourceRevision` (`:1602-1607`). Replace with
  `mGateWatchFrames`, `mGateWatchW`, `mGateWatchH`.
- `PrismGateRuntime` fields `mProgramSlot`, `mWarmSlot`, `mWarmLoadedArmIndex` (`:1643-1647`).
- `hasReclaimableGateWarm` / `reclaimGateWarmSlotForUser` (`:2235-2242`) and their three call sites in
  the add paths (`:1841`, `:1889`, `:1926`) and in `cameraSelectionStatus` (`:1669-1670`).
- `reserveGateCapture` (`:5459`), `activateGate`'s producer allocation (`:5503-…`), the producer half
  of `deactivateGate` (`:5586-…`), `ensureGateWarmProducer` (`:5599`), `reprogramGateSlot` (`:5628`),
  and the warm/cut producer helpers (`performGateCut`, `activateGateWarm`, `gateSlotHasFreshOutput`,
  `clearGateWarmState`).
- In `updateGate` (`:4196-4312`): delete the program-producer availability check + `deactivateGate`
  (`:4202-4209`), `ensureGateWarmProducer`/`clearGateWarmState` (`:4216-4217`), the warm-swap
  `performGateCut(…, warm_ready)` and its "free a capture slot" reason (`:4241-4252`), and the
  reserved-slot conditions in the pre-warm blocks. Replace cut handling with "advance `mOnAirArmIndex`,
  bump `mCutSerial`"; replace warm with "set `mGateWatchFrames` on the next arm's resolved capture."
- In `setDisplayGateSubscribed` (`:4111-4180`): delete the "activate the gate first / bind to program
  capture" precondition (`:4111-4119`) and the `mGateReserved` filters in the un-subscribe fixed-slot
  search (`:4127`, `:4137`, `:4149`). Subscribing is now always allowed; unsubscribe falls back to the
  display's current or any `CAMERA_FEED` slot.

**ADD / REWRITE:**
- `routeGateDisplays` (`:5816-5849`): resolve `mArmed[mOnAirArmIndex].mCaptureId` → slot; point
  subscribed displays there (keep the `mDisplayCount` bookkeeping and the surface-lens-aperture guard
  at `:5828-5832`). If unresolved, unbind subscribed displays (dark).
- `prepareCameraCapture` warm block (`:2842-2848`): rename `mGateWarm*` → `mGateWatch*` (a countdown).
- `updateGate` decrement of `mGateWatchFrames` once per frame for the active warm capture.
- `renderAuxiliaryView` call site (`:7062`): **unchanged.**

### Pass 3 — Persistence (delete the program re-adopt)
- `sceneData` (`:4537`): delete the warm-slot skip (`:4547-4551`, no reserved captures exist) and the
  program-id canonicalization (`:4730-4736`). Serialize each armed entry as `{arm_id,label,enabled,
  capture_id}`. For gate_source displays write `gate_source=true` and the on-air camera's id as a
  compat hint (§H.3) rather than a hard fixed binding.
- `applySceneData`: relax the display cross-ref (`:5133-5134`) to exempt `gate_source` displays;
  resolve armed `capture_id`s best-effort; **delete** the program-producer re-adopt / reprogram /
  `ensureGateWarmProducer` block (`:5385-5437`) and the `warm_generation` accounting (`:5325-5329`),
  replacing it with: adopt `mGateSettings`, resolve on-air, `mController.reset()` + `manualPunch`.
- `llfloaterdirector.cpp` `saveScene`: the `prism_gates` lift line is already present; **no
  `SCENE_VERSION` bump.**

### Pass 4 — UI (delete the inflation apology)
- `llfloaterprismmanager.cpp`: delete the `mGateReserved` skip in `rebuildCaptureList` (`:980`) and the
  `mGateReserved` gates in selection/enable (`:869-884`, `:1398`); replace the
  `"N cams +G gate = T/MAX"` status branch (`:942-957`) with plain `"%u/%u captures"`. Change `gateArm`
  wiring to pass the selected capture id; show unresolved armed rows as "(deleted)".

### What is REMOVED from the current implementation (explicit list)
- The dedicated **program producer** and the **warm producer** — and every function that created,
  reprogrammed, swapped, reserved, reclaimed, or serialized them: `reserveGateCapture`,
  `ensureGateWarmProducer`, `reprogramGateSlot`, `performGateCut`, `activateGateWarm`,
  `gateSlotHasFreshOutput`, `clearGateWarmState`, `hasReclaimableGateWarm`,
  `reclaimGateWarmSlotForUser`.
- `PrismInstance::mGateReserved` and the `mGateWarm*`/`mGateSourceRevision*` fields.
- `PrismGateRuntime::mProgramSlot` / `mWarmSlot` / `mWarmLoadedArmIndex`.
- `GateSettings::mProgramCaptureId` and its serialization + re-adoption.
- `GateArmedCamera::mCamera` (the `CameraSettings` **copy**) → replaced by `mCaptureId`.
- The capacity competition (`reclaimGateWarmSlotForUser` at the add paths), the count inflation
  (`"N cams +G gate"`), the `rebuildCaptureList` hiding, and the starvation "free a capture slot for
  smooth cuts" reason.
- The "activate the gate first so this monitor can bind to its program capture" subscribe precondition.

Everything else is reuse: `ALDirectorSwitcherModel::Controller` (verbatim), the admission/selection
engine (untouched but for the one renamed watch block), the persistence round-trip, and every UI
idiom.

---

## §OFF-LIMITS (do not touch)

- The **render consume path** and **shaders**.
- The **admission/scheduler internals**: `updateCadenceEntitlements` water-fill (`:3062-3088`) and
  `chooseRenderSlot` (`:3125-…`). The Gate layers on `mAnyDisplayVisible` via the one renamed watch
  block only.
- `ALDirectorSwitcherModel` / `ALDirectorSwitcher` — reused as-is.
- The **Bone-POV / VCam-joint** code — orthogonal, and now needs *zero* gate interaction.
- **`SCENE_VERSION`** (`llfloaterdirector.cpp`, value **4**) — the gate change is additive/field-swap
  inside an already-optional block; no bump.

---

## §RISKS (reported)

1. **Can transient pre-warm get a fresh frame in time without a reserved producer? — LOW risk, and
   strictly lower than the shipped model.** The mechanism is identical (same N-frame admission-seed
   window, same `prepareCameraCapture` block), but it now targets the **user's own capture**, which
   frequently still holds a valid **HELD** output from its last time on-air — a safety net the shipped
   fresh-warm-producer never had. Worst case (a camera never yet rendered, `mPrewarmFrames` too small
   at low FPS) yields one HELD/stale frame at the cut — the same failure surface as before, minus the
   `releasePrismLensBuffers` alloc/free hitch. Mitigation: keep the default `mPrewarmFrames = 3` and
   the "TAKE only when preview is CURRENT" MANUAL guard.
2. **Does removing dedicated producers break any monitor/scene round-trip? — MEDIUM, fully mitigated
   by §H.** The shipped program producer existed *solely* to make a subscribed monitor's `capture_id`
   resolve at load (`:5133-5134`). Removing it requires the loader to exempt `gate_source` displays
   from that cross-ref (§H.1). The one genuine decision is old-viewer compatibility: a `gate_source`
   display with **no** `capture_id` would be rejected by an *old* viewer's strict check. Mitigation
   (recommended): write the on-air camera's id as a compat hint so old viewers degrade to a static
   feed while the new loader treats `gate_source` as authoritative — no `SCENE_VERSION` bump needed. A
   build must confirm the old-viewer path; this is the single INFERS to close.
3. **On-air/next capture deleted mid-run (arming is now a live reference).** In the shipped copy model,
   disarming/deleting a source could not desync the gate (it held its own copy). In the router model, a
   deleted capture makes an armed `mCaptureId` dangle. Mitigation (§Edge Cases 2): resolve on-air/next
   each frame; forced-advance to the next resolvable arm, or go dark with unbound monitors — never
   route to a freed slot. Covered by test-plan 1 and 4.

**Outcome confirmed:** capture count **== the user's actual cameras**. Arming records a reference and
allocates nothing; the gate holds **zero** dedicated producers. A 3-camera gate reads **3/8**, and the
full `MAX_CAPTURES = 8` budget is spent only on real user cameras. The design is a strict **reduction**
of the shipped code — a whole producer-pool subsystem (reserve/warm/reprogram/swap/reclaim, the
capacity competition, the count-inflation UI, the program re-adopt on load) is **deleted** and replaced
by "resolve a handle and rewrite `mCaptureSlot`," so it ends up **simpler**, not more complex.
