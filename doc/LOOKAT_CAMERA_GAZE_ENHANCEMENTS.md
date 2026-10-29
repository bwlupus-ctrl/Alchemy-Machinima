# Look-at-Camera / Lens Gaze — Enhancements (implementation spec)

**For the implementer (Gemini):** this is a self-contained design/implementation spec. Build it in the
Alchemy-Machinima viewer fork at **I:\alchemy-machinima**. Do NOT change unrelated systems. Keep all gaze
math PURE and deterministic (see §Determinism). After you implement, a reviewer (Claude) will adversarially
review the diff, so leave the code clean and cite the symbols you touched.

## 0. Goal
Make the "look at camera" gaze believable and directable for multi-avatar machinima:
- **Targeting**: pick exactly which cast members gaze, give each its own target, and (headline) have flagged
  subjects track whichever camera is **live on the Vcam Gate**, re-aiming smoothly on a cut.
- **Animation naturalism**: kill the dead "uncanny stare" with micro-life, reaction latency, natural breaks,
  an anatomical neck→body chain, asymmetric ease, and per-subject variation.

## 1. Current architecture (what exists — extend this, don't rebuild)
- **Per-frame application**: `LLVOAvatar` calls `LLActorMover::applyGaze(this)`
  (`indra/newview/llvoavatar.cpp:5231`) every frame. This is a RENDER-ONLY override of the avatar's
  head/neck/eye (and optional torso) rotation — it does not touch the server or the avatar's real animation
  state.
- **Pure math**: `indra/newview/algazemath.h` holds the pure gaze math (direction blend, smoothing, ease,
  dead-zone, break-off). Add new naturalism functions HERE so they are unit-testable and deterministic.
- **Per-cast flag**: `LLDirectorCast::setLookAtCamera(const LLUUID& id, bool)` / `isLookAtCamera(id)`
  (`lldirectorcast.cpp:120/137`) already stores, per cast member, whether it looks at the camera; it is
  serialized into the scene (`e["look_at_camera"]`, `:529`).
- **UI**: the Lens Gaze panel `indra/newview/alpanellensgaze.cpp` + `skins/default/xui/en/panel_lens_gaze.xml`
  already exposes: Enable; a **target combo** (0 "Where it's going", 1 **Camera**, 2 **Cast member**,
  3 **Fixed point**); Head/eyes blend; Torso; Intensity; Smoothing; Dead zone (deg); **Break off past**.
- **Settings**: `DirectorLookAtCamera{Enabled,Mode,Strength,HeadEye,Torso,TorsoAmount,Smoothing,EaseTime}`
  in `app_settings/settings.xml` (~4822+).
- **Vcam Gate on-air**: the Prism Vcam Gate exposes the currently on-air camera; the live-camera feature
  reads that (see §A3 for the exact accessor to add/use).

## 2. Determinism (REQUIRED for all "random-looking" motion)
Machinima must scrub/replay identically. Every "random" element (saccade timing, break cadence, reaction
delay, per-subject jitter) MUST be a PURE closed-form function of `(cast_seed, presentation_time)`, exactly
like the Cine FX system (`alcinelightrigmodel.cpp` evalFX). Use hashed value noise / phase-sin helpers keyed
on a per-cast seed (derive `cast_seed` from the avatar UUID's low bits, stable per session). NO
`Math.random()`, NO wall-clock, NO frame-count accumulation. Same `presentation_time` → same pose. Put these
in `algazemath.h` and unit-test them.

---

## PART A — Targeting

### A1. Per-cast "who looks" toggle row
Add a **You / A / B / C / D** checkbox row (mirror the Cine Light Rig group row layout) to the Lens Gaze
panel, each wired to `LLDirectorCast::setLookAtCamera(slotId, checked)` for that slot's resolved avatar.
- Resolve slot→UUID via the existing `LLDirectorCast` subject accessors (self + A..D).
- Reflect current state from `isLookAtCamera(id)`; grey a slot whose subject is unassigned.
- This replaces the effective "global on/off" with explicit per-subject selection.

### A2. Per-subject target (each cast member gets its own target)
Today the target combo is effectively one setting. Make the **target per cast slot**:
- Store, per cast slot, a `GazeTarget { EMode mode; LLUUID castRef; LLVector3 fixedPoint; LLUUID objectRef; }`
  where `EMode ∈ { MOTION ("where it's going"), CAMERA, CAST_MEMBER, FIXED_POINT, OBJECT }`.
  (OBJECT is new — reuse the object-resolution approach from the Cine object-targeting feature:
  `gObjectList.findObject(objectRef)` → its bounding-box centre.)
- UI: when a slot is selected in the You/A/B/C/D row, the target combo + its sub-field (cast picker / "Set
  point from camera" / object-from-selection) edits THAT slot's target.
- `applyGaze` resolves each avatar's target from its slot's `GazeTarget` and aims there.
- Example the user wants supported: A→CAMERA, B→CAST_MEMBER(A) (eyeline), C→FIXED_POINT.

### A3. ⭐ Live-camera re-aim (Vcam Gate integration) — the headline
When a cast member's target mode is CAMERA and the **Vcam Gate is active**, aim at the **on-air** camera, and
**re-aim smoothly when the gate cuts**.
- Add/So use a read accessor for the gate's current on-air camera pose. The Prism gate routes monitors to the
  on-air capture; expose the on-air capture's world eye position (a virtual camera has a stored transform;
  an object-backed one resolves via its object). Add `LLPrismLens::gateOnAirCameraEye(LLVector3& out_agent)`
  returning false when the gate is inactive (then fall back to the actual render camera
  `LLViewerCamera::getInstance()->getOrigin()`).
- On a gate CUT (the on-air capture changes — detectable via the gate's cut serial / on-air arm index), the
  gaze target position jumps; the EXISTING smoothing + ease must carry the avatars from the old camera to the
  new one over `EaseTime` (do NOT snap). Optionally a per-cast tiny stagger so a group doesn't turn in perfect
  unison (reuse §B6 jitter).
- Gate-inactive or "look at render camera" stays the current behaviour.

---

## PART B — Animation naturalism (all in `algazemath.h`, pure + tested)

All of these MODULATE the final aim/pose that `applyGaze` already computes. Add a single
`GazeLifeParams` struct (new settings, §Settings) and pure functions that take `(base_aim, cast_seed, t,
params)` and return the modulated aim + secondary pose channels.

### B1. Micro-life while locked (biggest win)
Even when locked on target, add subtle life so it isn't a dead stare:
- **Eye saccades**: tiny (<~1.5°) hashed micro-offsets to the eye aim on an irregular cadence (value-noise
  keyed on cast_seed) — quick flicks, brief holds.
- **Blinks**: periodic eyelid blink if the rig supports an eyelid/morph channel; otherwise a 1–2 frame
  eye-aim dip. Cadence ~ every 3–6 s, hashed per cast so subjects don't blink in sync.
- **Head drift**: <~1° slow Lissajous drift on the head so the neck isn't frozen.
- Amplitudes scale with a `MicroLife` intensity (0 = perfectly still, 1 = lively). Default ~0.4.

### B2. Reaction latency + catch
When a subject ACQUIRES a target (target set, or camera comes into view past dead-zone), don't start turning
instantly:
- A per-subject randomized **reaction delay** (hashed on cast_seed, ~0.1–0.6 s) before the ease begins
  ("notices the camera"), then a slightly **over-damped catch** into the lock.
- This staggers a group so they don't snap in unison.

### B3. Naturalized breaks
The current "Break off past X°" only fires past an angle. Add **periodic natural breaks within range**:
- On a hashed cadence (~every 4–9 s per subject), briefly glance away by a small angle for a short hold, then
  return — a natural "look-away". Gated by a `BreakFrequency` (0 = never; 1 = fidgety).
- Must be deterministic (hashed on cast_seed, t) and never break during the first `ReactionDelay` of a fresh
  lock.

### B4. Anatomical chain with limits
Replace the flat Torso slider contribution with a proper chain as the target exceeds comfortable range:
- Distribute the required yaw/pitch across **eyes → head → neck → chest → hips**, each with a limit; only
  recruit the next joint once the previous saturates. Comfortable head+neck yaw ~ up to ~70°; beyond that,
  bring in chest, then hips.
- Past a `BodyTurnThreshold` (default ~90°), trigger a **foot re-plant / body turn** (this is the existing
  Mode=1 "Turn body"); below it, upper-body only. Keep it render-only and smoothed.
- Keep the existing Head/eyes + Torso sliders as the low-angle weighting; the chain governs the high-angle
  recruitment.

### B5. Ease asymmetry
Split the single `EaseTime` into **acquire** vs **release**:
- `EaseAcquireSec` (default ~0.25) and `EaseReleaseSec` (default ~0.6). Snappy lock, slower release reads as
  intent. Keep a single "Ease" slider that scales both if you want minimal UI, plus an advanced pair.

### B6. Per-subject variation
Auto-jitter (hashed on cast_seed, small) the effective Intensity, Smoothing, and the §B1–B3 cadences per
cast member (±~10–15%) so a crowd never looks hive-minded. One `Variation` slider (0 = identical,
1 = characterful). Default ~0.3.

---

## Settings (add to app_settings/settings.xml, follow the DirectorLookAtCamera* style)
`DirectorGazeMicroLife` (0..1), `DirectorGazeBlinks` (bool), `DirectorGazeReactionMin/Max` (sec),
`DirectorGazeBreakFrequency` (0..1), `DirectorGazeBodyTurnThresholdDeg`, `DirectorGazeEaseAcquireSec`,
`DirectorGazeEaseReleaseSec`, `DirectorGazeVariation` (0..1). Per-cast target lives in the scene/cast data,
not global settings.

## UI (panel_lens_gaze.xml + alpanellensgaze.cpp)
- Add the You/A/B/C/D toggle row (A1) at the top.
- Make the target combo + sub-field edit the SELECTED slot's target (A2).
- Add a small "Naturalism" group: Micro-life, Break frequency, Variation sliders; Blinks checkbox; the
  ease-acquire/release pair (advanced). Keep it within the panel rect. XML comments must not contain `--`.

## Persistence
Per-cast `look_at_camera` already serializes. Extend the cast scene entry with the per-slot `GazeTarget`
(mode + refs + fixed point), additively; a scene lacking it defaults to the global/legacy behaviour. Do not
break old scenes.

## Determinism / performance / safety
- Render-only (never sends anything to the server); no change to the avatar's real animation or to other
  avatars' viewers.
- All motion PURE closed-form of `(cast_seed, presentation_time)` — scrub/replay identical (§2).
- `applyGaze` runs per avatar per frame: keep the added math O(1) per avatar, no allocations in the hot path.
- Gate integration must no-op cleanly when the Vcam Gate is inactive or absent.

## Tests
Pure unit tests in the gaze-math test file (mirror the cine model tests): saccade/break/blink cadence
determinism (same t → same value; different cast_seed → decorrelated); ease-asymmetry curve; anatomical
chain recruitment order + limits; reaction-delay gating; body-turn threshold. No avatar/viewer singletons in
the pure tests.

## Build/verify note for the implementer
Incremental `cmake --build build-Windows-vs2026-os --config Release` does NOT re-copy skins/app_settings —
after editing XML/settings, copy the changed files into
`build-Windows-vs2026-os/newview/Release/{skins,app_settings}/`. The exe must be closed to relink.
