# Flycam Drone Control — weight & mass for the flycam (Xbox-first) — design brief (for adversarial review)

**Date:** 2026-10-06
**Status:** DESIGN ONLY. Nothing implemented, nothing built.
**Process:** heavy engine work → **Fable / Codex** sub-agents; UI/settings/docs → **Sonnet/Opus**. Every
work package gets an **adversarial Opus review looped to 0 must-fix BEFORE the single build** (Claude
builds; Codex/Fable never build; Codex only via `--prompt-file`). See §11.

> **BASE BRANCH.** Line numbers are from `origin/fix/animesh-clone-pose-polish` (`f9ad494`). Re-anchor
> against the user's local tree before implementation.

---

## 1. The ask

> "A dynamic clutch that can ramp flycam speed for more dramatic shots." — "Currently the analog
> triggers roll the camera, so a toggle between roll or clutch would be required." — "The triggers
> should impact the movement mid-flight." — "Right now it just controls the direction, but has no
> impact on the momentum." — "Make it feel natural, triple-A game input." — "Primary focus is an Xbox
> controller, but Alchemy supports non-Xbox controllers. Focus on making the Xbox input feel like a
> drone." — **"I'm not trying to change DirectInput. Mainly just trying to add weight and mass to
> flycam, making it feel drone and natural."**

**Core goal: weight and mass.** Everything here serves that. The input path (DirectInput / libndof)
is **not** modified; the feature works entirely on the axis/button values it already delivers.

---

## 2. Scope (ship whole)

**In this delivery:**
1. **Input shaping (post-read only):** radial stick dead zone, response curves, trigger
   dead/saturation zones — applied to the values the existing DirectInput path already returns. No
   change to device enumeration, polling or mapping code.
2. **Drone flight model:** world-up yaw, horizon-locked translation, separate gimbal tilt, a real
   velocity state with throttle/brake/drag/grip, Hold vs Drift release behaviour, flight modes.
3. **Roll ↔ Clutch trigger toggle** (latched + momentary), D-pad roll in Clutch mode.
4. **Fixed-step simulation + render interpolation**; recorder stores shaped input.
5. **Speed-coupled camera extras** (all default OFF): speed-scaled look, look acceleration, bank, FOV
   kick, look-ahead.
7. **Tuning UI in human terms**, presets (Cine / Normal / Sport / Dolly / FPV), debug telemetry graph.
8. Settings, Joystick floater + Flycam/Director UI, reset/preset registration, log verdicts.

**Not changing:** the input API (DirectInput/libndof stays exactly as is); the Classic flight model. `FlycamFlightModel = Classic` must be byte-identical to
today (§9).

---

## 3. What the code PROVES today (`llviewerjoystick.cpp`, base branch)

| Fact | Where |
|---|---|
| Speed = stick × `FlycamAxisScale[i]` × frame time; then exponential chase: `sDelta += (cur_delta − sDelta) × time × FlycamFeathering`. **No velocity state** — release decays at the same rate as onset; nothing carries | `moveFlycam`, ~`:1705-1717` |
| Xbox defaults set `FlycamFeathering = 1.0` (≈1 s lag) | `setXboxDefaults`, `:2641` |
| Translation is rotated by the **full** camera orientation: `sFlycamPosition += sDelta * sFlycamRotation` → looking down + forward = dive into the ground (6-DOF spaceship, not drone) | `:1731` (and `:1833` second path) |
| Rotation is applied incrementally in the **camera-local** frame (`LLMatrix3 rot_mat(sDelta[X],[Y],[Z])`), so yaw while pitched tilts the horizon; `mAutoLeveling` then pulls roll back over time | `:1732-1750` |
| Dead zones are **per axis** (`mFlycamAxisDeadZone[i]`) → notchy diagonals | `:1676+` |
| Input via DirectInput (`<dinput.h>`, libndof). DirectInput normally exposes an Xbox pad's LT/RT as **one combined axis** (INFERENCE from DirectInput's standard Xbox behaviour — verify in the Joystick floater axis readout) | `:54`, `:80-115` |
| Xbox defaults: roll on joystick axis 2 (the trigger axis); roll buttons unmapped (`-1`); Jump/Crouch buttons add to the Z (vertical) axis in flycam; bumpers = zoom in/out | `setXboxDefaults`, `:2568-2590`; `:1664-1672` |
| No rumble / haptics anywhere in joystick or window code (and none is added — out of scope) | `git grep rumble|haptic|XInputSetState` → none |
| The handheld operator consumes flycam velocity: `opin.mLinearVel = sDelta[0..2] / dt`, `mAngularVel = sDelta[3..5] / dt` | `:2041-2046` (`LLCameraOperator::update`) |
| Operator has a fixed simulation quantum setting | `FlycamOperatorSimulationHz` |
| Flycam Orbit writes `sFlycamPosition` from its own anchor maths when active | `:1997` |
| Easing curve library exists (shared Bézier evaluator) | `indra/newview/alcameracurve.h` |
| There are **two** translation/rotation code paths in `moveFlycam` (a BD-derived path and a fallback path) | `~:1620-1760`, `~:1762-1840` |

---

## 4. Input layer

### 4.1 Input source — unchanged
- The flight model reads the **same** axis and button values `moveFlycam` reads today
  (`getJoystickAxis` / `getJoystickButton` through the existing mapping). No XInput, no new device
  code, no change to `JoystickAxis*` / `JoystickButton*` semantics.
- **Triggers on one combined axis** (the usual DirectInput Xbox layout — verify in the Joystick floater
  axis readout): in Clutch mode the axis is split by sign — one direction = **throttle**, the other =
  **brake**. Pressing both cancels, so throttle and brake can't overlap; documented in the tooltip.
  A "Swap throttle/brake" checkbox covers pads that report the opposite sign.
- **Pads that expose triggers as two axes** work too: map throttle and brake to separate axes in the
  existing mapping UI (two new flight-model axis assignments, not new device code).

### 4.2 Shaping (post-read, Drone model only)
- **Radial dead zone** for each stick (magnitude-based, then rescaled 0..1 so there's no jump at the
  edge) replacing per-axis trimming in the drone model. Classic keeps per-axis.
- **Outer saturation** (default 5%): last bit of travel = full.
- **Response curves (expo)** per control: move stick, look stick, throttle, brake, gimbal.
- **Trigger dead zone** (default 8%) and **engage threshold**.
- Shaped values feed the flight model **and** are what the recorder stores (§7).

---

## 5. Drone flight model (`FlycamFlightModel = Drone`)

### 5.1 Frames — what makes it a drone
- **Body yaw about world up.** Yaw never tilts the horizon.
- **Horizon-locked translation.** Forward/strafe move in the **yaw-only** plane; vertical is world Z.
  Looking down and pushing forward flies level over the subject, it does not dive.
- **Gimbal tilt separate from the body.** Right-stick vertical tilts the camera (pitch) without
  changing the direction of travel. Gimbal has its own speed, curve and smoothing.
- **Roll** is never stick-driven in Drone (except D-pad in Clutch mode, §6); optional cosmetic **bank**
  into turns (§5.6).
- Optional **"Free-fly"** sub-mode keeps today's 6-DOF view-relative translation with the new dynamics,
  for users who want spaceship flight with momentum.

### 5.2 Velocity state
World-space velocity `V` (and separate yaw rate and gimbal rate), integrated at a fixed step.

- **Move stick → desired direction** in the horizon plane (magnitude = shaped deflection).
- **Throttle (RT) → thrust** along the desired direction, shaped by a **power curve**: strong pickup at
  low speed tapering to zero at top speed (no wall at top speed).
- **Brake (LT) → progressive deceleration**; light = feather, full = firm stop.
- **Drag:** constant term (coast length) + speed-proportional term (no floaty high speed).
- **Jerk limiting:** acceleration itself ramps in over a short window (default 0.08 s) — the
  single biggest "cinematic, not gamey" factor.
- **Separate accel / decel** characteristics.
- **Vertical** has its own thrust/drag/top speed (lighter, slower climbs than cruise).
- **Settle:** below a crawl speed with no input, ease to an exact stop (no endless creep).

### 5.3 Release behaviour (drone "position hold" vs "attitude")
- **Hold (default):** sticks released → active braking to a hover over `HoldStopTime`. Like a GPS
  drone. Safe, predictable framing.
- **Drift:** sticks released → coast under drag only. Like ATTI mode. The dramatic glide.
- Toggle on a button; both shown in the HUD readout.

### 5.4 Clutch semantics
- **Assist (default):** stick alone flies at the mode's cruise speed; RT adds thrust on top up to the
  boost top speed; LT brakes. Never touching the triggers = familiar flight.
- **Pure throttle:** stick = direction only; all speed comes from RT. Fully performed speed.

### 5.5 Turning weight
- **Grip** (0..1): how fast existing velocity rotates toward a new stick direction.
- **Speed-dependent turn rate:** tight at low speed, wide carving arcs at high speed (the "weight").

### 5.6 Speed-coupled camera extras (all default OFF)
- **Speed-scaled look sensitivity** (steadier at speed).
- **Look acceleration** (hold full deflection → yaw rate ramps up after a delay).
- **Bank into turns** (few degrees, eased; disabled when Clutch-mode D-pad roll is in use).
- **FOV kick** (small, eased, tied to speed).
- **Look-ahead** (gimbal yaw drifts slightly toward the velocity direction).

### 5.7 Flight modes (presets, tuned in human terms)
| Preset | Feel |
|---|---|
| **Cine** | slow, heavy, long coast, gentle turns, Hold on release |
| **Normal** | balanced drone |
| **Sport** | punchy throttle, high top speed, banks |
| **Dolly** | no vertical, very smooth, low top speed |
| **FPV** | Free-fly frames, fast, Drift on release |

Cycle with a button (View/Back by default); the mode change eases parameters over 0.5 s so it never
jerks mid-shot.

---

## 6. Controls (Xbox default layout — all remappable; non-Xbox via existing mapping UI)

| Control | Drone layout |
|---|---|
| Left stick | Move (forward/back, strafe) in the horizon plane |
| Right stick X | Yaw |
| Right stick Y | Gimbal tilt |
| RT | Throttle (Clutch) / roll right (Roll mode) |
| LT | Brake (Clutch) / roll left (Roll mode) |
| A / B | Ascend / descend (existing Jump/Crouch → Z) |
| LB / RB | Zoom out / in (unchanged) |
| D-pad ←/→ | Roll left/right while triggers are in Clutch |
| D-pad ↑ | Hold ↔ Drift |
| D-pad ↓ | Recentre gimbal |
| View/Back | Cycle flight mode |
| Stick click (R3) | Momentary Clutch (hold) |
| New mapped button | Latched Roll ↔ Clutch toggle |

Optional **"Mode 2 (RC transmitter)"** layout for drone pilots: left stick = altitude + yaw, right
stick = forward + strafe.

Mode switches show a small toast ("Triggers: Clutch", "Drift", "Sport") that hides with the UI.

**Toggle handover:** Roll→Clutch lets roll rate decay through smoothing (angle kept unless
auto-level); Clutch→Roll eases throttle contribution to zero over the decel time.

---

## 7. Simulation, interpolation, recorder, other systems

- **Fixed step** at `FlycamOperatorSimulationHz` with **render interpolation** between the last two
  states (otherwise a fixed step judders when fps doesn't divide it).
- **Recorder** stores shaped inputs + mode/preset per sample (format version bump; old takes play as
  Classic). Replay at any fps reproduces the move.
- **Handheld operator** keeps consuming velocity — feed it the simulated `V` (and yaw/gimbal rates), not
  `sDelta`, so its gait/shake reacts to real drone speed.
- **Flycam Orbit** (`:1997`) takes precedence when active; Drone velocity is zeroed on Orbit entry and
  re-seeded from the current camera on exit.
- **Cinematic Camera / Director cuts:** on any external camera teleport, Drone velocity resets (no
  carried momentum across a cut).
- **Entering Drone** from Classic seeds `V` from the current `sDelta` so switching mid-flight has no
  pop.

---

## 8. Feedback
- No rumble (would need a different input API — out of scope by user decision). Feedback is visual
  only: mode toasts and the optional speed readout, both hidden with the UI.

## 9. Off-path inert
- `FlycamFlightModel = Classic` (default for the first in-world A/B): both existing `moveFlycam` paths
  run unchanged.
- Non-Xbox devices in Classic: identical to today.
- No render-state changes anywhere in this feature.

---

## 10. Reviewer: attack these first (least-sure list)

1. **Combined trigger axis.** Confirm how the existing DirectInput path reports an Xbox pad's
   triggers (one axis or two) and its rest value; prove the throttle/brake split has no drift at rest
   and no sign flip across pads. 3D mice (absolute `m3DCursor` mode) must keep Classic behaviour or be
   explicitly handled.
2. **Frame change without regressions.** Horizon-locked translation + world-up yaw replace the
   camera-local maths only in Drone; prove Classic, Orbit and the operator are untouched.
3. **Fixed step + interpolation + operator.** The operator already has its own quantum — is there a
   double-integration or phase-mismatch between the drone sim and the operator sim?
4. **Two `moveFlycam` paths** (`~:1620` and `~:1762`): which one runs for an Xbox pad, and does Drone
   need to hook both?
5. **Recorder compatibility** with existing takes and the operator's determinism guarantees.
6. **Is the approach wrong?** Argue a separate `LLFlycamDrone` controller class that `moveFlycam`
   delegates to (preferred by this brief) versus extending `moveFlycam` in place.

---

## 11. Work packages

| WP | Content | Owner | Review |
|---|---|---|---|
| 1 | Shaping layer on existing axis values (radial DZ, curves, saturation, trigger split/zones), input-layout log | **Codex** | Opus adversarial |
| 3 | `LLFlycamDrone` flight model: frames, velocity, thrust curve, drag, brake, jerk limit, grip, Hold/Drift, modes, Free-fly | **Fable** | Opus adversarial |
| 4 | Fixed step + interpolation, operator feed, Orbit/cut/Classic handover, recorder format | **Fable** | Opus adversarial |
| 5 | Controls/mapping (existing mapping UI only), Roll↔Clutch toggle, toasts | **Sonnet** | Opus adversarial |
| 6 | Tuning UI in human terms + presets + debug telemetry graph | **Sonnet** | Opus adversarial |
| 7 | Settings, reset/preset registration, docs (`MACHINIMA_USER_GUIDE.md`) | **Opus** | independent Opus adversarial |

Each implementer gets an explicit OFF-LIMITS file list. Fixes batched and re-deferred until 0 must-fix
across all packages; then one build.

---

## 12. In-world test (outcomes stated in advance)

1. **Trigger split.** Clutch mode, Joystick floater open: RT alone → throttle only; LT alone → brake
   only; both released → neither (no drift). Log `DRONEINPUT combined-triggers` or `separate-triggers`.
2. **Horizon lock.** Drone/Normal: pitch the gimbal straight down, push forward. *Expect:* level flight
   over the ground. Classic: *expect* today's dive.
3. **Momentum.** Drift mode: reach speed, release. *Expect:* glide with smooth decay. Hold mode:
   *expect* eased stop within `HoldStopTime`.
4. **Clutch mid-flight.** Cruise forward, squeeze RT halfway then fully. *Expect:* smooth, increasing
   pull, tapering near top speed; no step. LT: progressive slow-down.
5. **Turn weight.** At high speed swing the stick 90°. *Expect:* wide carving arc; at low speed, tight.
6. **Toggle.** Switch Roll↔Clutch mid-move. *Expect:* no jolt; toast shows mode.
7. **Determinism.** Record a take, replay at 30 and 144 fps. *Expect:* same path.
8. **Classic off-path.** `FlycamFlightModel = Classic`. *Expect:* indistinguishable from today.
9. **Non-Xbox pad.** Generic pad in Drone via the existing mapping. *Expect:* same weight and coast feel.
