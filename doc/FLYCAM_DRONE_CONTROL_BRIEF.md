# Flycam Drone Control — weight & mass for the flycam (Xbox-first) — design paper (for adversarial review)

**Date:** 2026-10-07 (rev 3 — full design paper; supersedes rev 1/2)
**Status:** DESIGN ONLY. Nothing implemented, nothing built.
**Process:** heavy engine work → **Fable / Codex** sub-agents; UI/settings/docs → **Sonnet / Opus**.
Every work package gets an **adversarial Opus review, looped to 0 must-fix BEFORE the single build**.
Claude builds; Codex/Fable never build; Codex only via `--prompt-file`. See §17.

> **BASE BRANCH.** Line numbers are from `origin/fix/animesh-clone-pose-polish` (`f9ad494`). The user has
> unpushed local work — re-anchor every reference before implementation.

---

## 1. The ask

> "A dynamic clutch that can ramp flycam speed for more dramatic shots." — "The analog triggers roll the
> camera, so a toggle between roll or clutch is required." — "The triggers should impact the movement
> mid-flight." — "Right now it just controls the direction, but has no impact on the momentum." — "Make it
> feel natural, triple-A game input." — "Primary focus is an Xbox controller; Alchemy supports non-Xbox
> controllers too." — **"I'm not trying to change DirectInput. Mainly just trying to add weight and mass to
> flycam, making it feel drone and natural."**

**Core goal: weight and mass.** The input path (DirectInput / libndof device code and mappings) is **not**
modified. Everything here operates on the axis and button values `moveFlycam` already reads.

---

## 2. Scope (ship whole)

**In this delivery**
1. `LLFlycamDrone` — a self-contained flight model (pure maths, unit-testable) called from `moveFlycam`.
2. Input shaping applied *after* the existing reads: radial dead zone, saturation, expo curves, trigger split.
3. Drone frames: world-up yaw, horizon-locked translation, separate gimbal tilt, optional cosmetic bank.
4. Mass model: target-velocity tracking with acceleration and jerk limits, power-curve taper, drag,
   progressive brake, grip, speed-dependent turn weight, Hold vs Drift, Assist vs Pure throttle.
5. Roll ↔ Clutch trigger toggle (latched + momentary) with continuous hand-over.
6. Fixed-step simulation with render interpolation (the operator's proven pattern).
7. Presets and tuning **in human terms** (time to top speed, stopping distance, coast time, turn weight),
   compiled to coefficients by closed-form derivations (§9).
8. Speed-coupled extras, all default OFF: speed-scaled look, look acceleration, bank, FOV kick, look-ahead.
9. Settings, UI (Joystick floater section + Flycam/Director panel), reset/preset registration, telemetry
   graph, log verdicts, TUT unit tests.

**Not changing:** DirectInput/libndof device code, `JoystickAxis*`/`JoystickButton*` semantics, the Classic
flight model (`FlycamFlightModel = Classic` is byte-identical to today), Flycam Orbit, the handheld operator's
internals, the flycam recorder's file format. No rumble (needs a different input API — out of scope).

---

## 3. What the code PROVES today (base-branch file:line)

| # | Fact | Where |
|---|---|---|
| F1 | `moveFlycam` has **two** integration paths: a BlackDragon path (`if (blackdragon)`) and a default path. `BlackDragonControls` defaults to **0**, so the **default path is the non-BD `else` branch** | `llviewerjoystick.cpp:1620-1760` (BD), `~:1762-1840` (default); `settings_alchemy.xml` `BlackDragonControls` = 0 |
| F2 | Per axis: `cur_delta = −axis` → per-axis dead zone → `× FlycamAxisScale[i]` → `× time` → `sDelta += (cur_delta − sDelta) × time × FlycamFeathering`. `sDelta` is a **per-frame displacement** chasing the stick; **no velocity state** — onset and release are the same exponential, nothing carries | default path `~:1790-1823` |
| F3 | Xbox defaults set `FlycamFeathering = 1.0` (≈1 s chase) | `setXboxDefaults`, `:2641` |
| F4 | Translation uses the **full** camera orientation: `sFlycamPosition += LLVector3(sDelta) * sFlycamRotation` → look down + forward = dive (6-DOF) | `:1731` (BD), `:1833` (default) |
| F5 | Rotation is incremental in the **camera-local** frame (`LLMatrix3 rot_mat(sDelta[3],[4],[5])`); yaw while pitched skews the horizon; `AutoLeveling` (default **1**) nlerps roll back at `feather × time` | `:1732-1750`, `:1835-1852` |
| F6 | The BD path clamps `time` to **[0.016, 0.033] s**, so above 60 fps it moves faster per second than below (frame-rate dependent). Default path clamps `time ≤ 0.2` | `:1628-1631`; `~:1768-1772` |
| F7 | Dead zones are per axis (`mFlycamAxisDeadZone[i]`) → notchy diagonals | default path `~:1793-1800` |
| F8 | Xbox defaults: roll = joystick axis 2 (the trigger axis under DirectInput); roll buttons unmapped (−1); Jump/Crouch add to Z in flycam; bumpers = zoom | `setXboxDefaults` `:2568-2590`; `:1664-1672` |
| F9 | Reset: `moveFlycam(true)` / `mResetFlag` re-seeds pose from the camera, clears deltas, resets the operator, re-seeds Orbit | `:1606-1619` |
| F10 | **Flycam Orbit** consumes the feathered `sDelta` as orbit controls and **overwrites** `sFlycamPosition`/`sFlycamRotation` when its anchor resolves | `:1859-2000` (write at `:1997-1998`) |
| F11 | Handheld operator is fed `opin.mLinearVel = sDelta[0..2]/dt` and `mAngularVel = sDelta[3..5]/dt` (camera-local), applied to the rendered camera only — `sFlycamPosition/Rotation` never mutated by it | `:2026-2058` |
| F12 | The operator already implements **fixed-step + accumulator + interpolation**: `mSimAccumulator += dt; while (acc ≥ h && steps < MAX_FIXED_STEPS_PER_FRAME) { prev = cur; cur = step(); acc −= h; }`, then `lerp(prev, cur, acc/h)`; `MAX_FIXED_STEPS_PER_FRAME = 512`, `FIXED_STEP_EPSILON = 1e-9`; `FlycamOperatorSimulationHz` default **120** | `llcameraoperator.cpp:30-31`, `:512-518`, `:600-625` |
| F13 | The **flycam recorder samples the final render camera** (pos, full rotation, FOV) and replays via splines — it is agnostic of what drives the camera | `llflycamrecorder.h:5-17`, `Keyframe` `:65-70` |
| F14 | No rumble/haptics in joystick or window code | `git grep rumble|haptic` → none |
| F15 | Shared easing evaluator exists | `indra/newview/alcameracurve.h` |

**Consequence of F13:** recorded takes already replay exactly; the drone model needs **no** recorder format
change. (Rev 1/2 said the recorder should store shaped input — withdrawn.) What fixed-step buys is
*live-performance* consistency: the same hands produce the same move at 60 or 144 fps.

**Pre-existing defect surfaced, not fixed here:** F6 (BD path frame-rate dependence). Separate one-liner
ticket; out of scope so this delivery stays reviewable.

---

## 4. Architecture

```
moveFlycam()
  reset? ─────────────────────────────► seed Classic + LLFlycamDrone::reset(pose)
  read axes/buttons (UNCHANGED code)    ─► raw[7], buttons
  model == Classic ─► existing BD / default path (byte-identical)
  model == Drone   ─► shape(raw) ─► LLFlycamDrone::advance(dt, input) ─► pose (interpolated)
                      write sFlycamPosition / sFlycamRotation / sFlycamZoom(Δ via existing W axis)
                      synthesize sDelta[] from the interpolated pose delta (see §11.3)
  Orbit / operator / camera write tail (UNCHANGED)
```

- **`LLFlycamDrone`** (`indra/newview/llflycamdrone.{h,cpp}`): owns state, parameters, fixed-step loop and
  interpolation. No GL, no globals, no settings reads inside `step()` — parameters arrive as a struct compiled
  once per settings change (§9). This is what makes it unit-testable (§16).
- `moveFlycam` keeps device reading, zoom, Orbit, operator and the camera write. Only the "integrate pose"
  block is replaced when `FlycamFlightModel == Drone`.

---

## 5. Input shaping (post-read; Drone only)

All inputs normalised to [−1, 1] (sticks) or [0, 1] (throttle, brake) after the existing `getJoystickAxis`.

**5.1 Radial dead zone + saturation (per stick pair).** For stick vector `s = (x, y)`, `r = |s|`,
dead zone `d`, outer saturation `o`:
```
r' = clamp((r − d) / (1 − d − o), 0, 1)
s' = (r > 0) ? s · (r' / r) : 0
```
Continuous at the edge (no jump), uniform in every direction, full output before physical full travel.
Defaults: move `d = 0.10, o = 0.05`; look `d = 0.12, o = 0.05`.

**5.2 Expo response.** `e(x) = sign(x)·((1 − k)|x| + k|x|³)`, `k ∈ [0, 1]`. Applied to the shaped magnitude
`r'` (not per axis) so curves never re-introduce notches. Defaults: move `k = 0.35`, yaw `k = 0.45`,
gimbal `k = 0.45`, throttle `k = 0.25`, brake `k = 0.0`.

**5.3 Trigger split (Clutch mode).** With the triggers on one DirectInput axis `t ∈ [−1, 1]` (verify sign
and rest in the Joystick floater):
```
throttle = shape(max( t·σ, 0)),  brake = shape(max(−t·σ, 0))      σ = ±1 ("Swap throttle/brake")
shape(u) = clamp((u − dz) / (1 − dz − o), 0, 1) then expo
```
`dz = 0.08`. Pads exposing two trigger axes map throttle/brake separately (two new flight-model axis
assignments in the existing mapping UI — no device code). Pressing both triggers on a combined axis cancels;
the tooltip says so.

**5.4 Vertical.** Existing Jump/Crouch buttons (A/B) remain the vertical input `u ∈ {−1, 0, 1}`, eased by the
vertical model (§6.6) so digital buttons still feel analog.

---

## 6. The mass model

### 6.1 State (world/agent space)
`p` position, `v` velocity (3D), `ψ` body yaw, `ω` yaw rate, `θ` gimbal pitch, `θ̇` gimbal rate, `a` last
commanded horizontal acceleration (for jerk limiting), `φ` cosmetic bank.

### 6.2 Frames (what makes it a drone)
- Yaw about **world up**: `q_body = R_z(ψ)`. The horizon never skews.
- Desired horizontal direction: `d = R_z(ψ) · (s'_fwd, s'_left, 0)`, `|d| ∈ [0, 1]`.
- Camera orientation: `q_cam = R_z(ψ) · R_y(θ) · R_x(φ)`; `θ ∈ [−89°, +89°]`.
- **Free-fly** sub-mode (FPV preset): `d` uses the full `q_cam` instead of `R_z(ψ)` (today's 6-DOF direction,
  now with mass).

### 6.3 Target velocity (horizontal)
```
Assist:  s* = |d| · lerp(V_cruise, V_top, throttle)
Pure:    s* = |d| · throttle · V_top
v*_h    = normalize(d) · s*                      (v*_h = 0 when |d| = 0)
```

### 6.4 Tracking with mass (horizontal)
Split the velocity error into along-track and cross-track parts relative to `v*_h` direction `n`:
```
e      = v*_h − v_h
e_par  = (e·n) n          e_perp = e − e_par
A_lim  = (e_par·v_h ≥ 0) ? A_accel(|v_h|) : A_decel          accelerate vs slow down
A_accel(s) = A_max · (1 − (s / V_top)^κ)                     power-curve taper, κ = 2
a_par  = clamp_len(e_par · K, A_lim)
a_perp = clamp_len(e_perp · K, G · A_max)                    G = grip ∈ [0, 1]
a_cmd  = a_par + a_perp
```
`K` (s⁻¹) is the tracking gain; with the limits it behaves like an acceleration-limited critically damped
servo: snappy when far from target, soft on arrival, no overshoot.

**Brake:** `a_brake = −normalize(v_h) · brake · D_brake` (added; can't reverse direction — clamped so one
step never flips `v_h`).

**Release (no stick, no throttle):**
- **Hold:** `v*_h = 0` with `A_decel = V_cruise / t_hold` → eased stop to hover.
- **Drift:** tracking off; only drag (§6.5) acts → the glide.

**Jerk limit:** `a = a_prev + clamp_len(a_cmd − a_prev, J_max · h)`. Default `J_max` from "onset time"
`t_j = 0.08 s`: `J_max = A_max / t_j`. This is the single largest "cinematic, not gamey" term.

### 6.5 Drag
`a_drag = −v · (c₁ + c₂|v|)` applied always in Drift; in Hold/Assist the tracking dominates, drag only shapes
high speed. `c₁` sets coast length, `c₂` removes high-speed floatiness.

### 6.6 Vertical (separate, lighter)
Same structure 1-D: `v*_z = u · V_climb`, accel limit `A_climb`, decel `A_climb_dec`, jerk `t_j`, drag
`c₁z`. Hold keeps altitude (target 0 when released) — equivalent to a drone's altitude hold.

### 6.7 Turn weight (yaw)
```
ω_max(s) = ω₀ / (1 + s / s_ref)                 tight at low speed, wide arcs at speed
ω*       = e_yaw(s'_yaw) · ω_max(|v_h|)
ω       += clamp(ω* − ω, ±α_yaw · h)            yaw-rate acceleration limit
ψ       += ω · h
```
Optional **look acceleration**: after `t_la = 0.25 s` at |s'_yaw| > 0.9, `ω₀` ramps to `ω₀ · m_la` over 0.4 s.

### 6.8 Gimbal
`θ̇* = e_g(s'_pitch) · Θ_max`, rate-limited by `α_g`, `θ` clamped ±89°. Optional speed-scaled look:
`Θ_max_eff = Θ_max / (1 + λ·|v_h|/V_top)`, same factor on `ω₀` (default λ = 0, i.e. OFF).

### 6.9 Cosmetic bank (optional, default OFF)
`φ* = clamp(β · (ω · |v_h|) / g, ±φ_max)` (centripetal-proportional), eased with τ = 0.25 s. Applied to
`q_cam` only — never to direction of travel. Disabled while D-pad roll is in use.

### 6.10 Settle
If `|v| < v_ε (0.02 m/s)` and no input for 0.2 s → `v = 0`, `a = 0`. Prevents endless creep.

### 6.11 Integration
Semi-implicit Euler at fixed `h = 1 / FlycamDroneSimulationHz` (default 120):
`v += (a + a_brake + a_drag) h;  p += v h`. Non-finite state → reset to last finite pose, `v = 0`, log
`DRONE NONFINITE` (verdict, §15).

---

## 7. Fixed step + interpolation (copy F12 exactly)

```
acc += frame_dt
steps = 0
while (acc + 1e-9 >= h && steps < 512):
    prev = cur;  cur = step(cur, input, h);  acc -= h;  ++steps
alpha = clamp(acc / h, 0, 1)
pose  = { lerp(prev.p, cur.p, alpha), nlerp(prev.q_cam, cur.q_cam, alpha) }
```
Input is sampled once per frame and held across that frame's steps (same as the operator). Backlog beyond
512 steps stays in `acc` (time is never discarded). `frame_dt` uses the **unclamped** frame interval (unlike
F6) — fixed stepping is what makes it frame-rate independent.

---

## 8. Roll ↔ Clutch (trigger ownership)

States: `ROLL`, `CLUTCH`, plus momentary `CLUTCH_HELD` (R3 held).
- **Latched toggle:** new mapped button `JoystickButtonTriggerMode` (default: View/Back is taken by mode
  cycling → default **unmapped**, user assigns; UI checkbox + Director hotkey always available).
- **Momentary:** R3 (`JoystickButtonClutchHold`, default R3 index) → CLUTCH while held.
- **ROLL → CLUTCH:** trigger contribution to roll is zeroed; existing roll rate decays with τ = 0.2 s; roll
  angle kept unless AutoLeveling.
- **CLUTCH → ROLL:** throttle/brake inputs ramp to 0 over `t_j·3`; the mass model carries velocity on
  naturally (no snap).
- **D-pad roll in CLUTCH:** the existing `JoystickButtonRollLeft/Right` assignments; Drone default layout maps
  them to D-pad ←/→.
- Toast on every change: "Triggers: Clutch" / "Triggers: Roll" (hidden with UI).

---

## 9. Human-terms tuning → coefficients (compiled once per change)

The UI never shows `A_max`, `c₁` etc. It shows the left column; `LLFlycamDrone::compile()` derives the right.

| User sees | Meaning | Derivation |
|---|---|---|
| **Cruise speed** `V_cruise` (m/s) | stick-only speed (Assist) | direct |
| **Top speed** `V_top` (m/s) | full throttle | direct (`≥ V_cruise`) |
| **Time to top speed** `t_top` (s) | 0 → 90 % of `V_top` at full stick + throttle | with taper κ = 2: `dv/dt = A(1−(v/V)²)` ⇒ `v(t) = V·tanh(A t / V)`; 90 % at `tanh⁻¹(0.9) = 1.472` ⇒ **`A_max = 1.472 · V_top / t_top`** |
| **Stopping distance** `d_stop` (m) | full brake from `V_top` | constant decel ⇒ **`D_brake = V_top² / (2 d_stop)`** |
| **Hold stop time** `t_hold` (s) | release at cruise → hover | **`A_decel = V_cruise / t_hold`** |
| **Coast time** `t_coast` (s) | Drift: cruise → 10 % (linear drag dominant) | `v = V e^{−c₁ t}` ⇒ **`c₁ = ln 10 / t_coast`** |
| **High-speed float** (0–1) | how long fast coasts last relative to slow | **`c₂ = (1 − float) · 4 c₁ / V_top`** (0 = strong quadratic drag; 1 = linear only) |
| **Onset softness** `t_j` (s) | how gently acceleration starts | **`J_max = A_max / t_j`** |
| **Grip** (0–1) | how fast momentum turns to the new heading | `G` direct |
| **Turn weight** (0–1) | how much speed widens turns | **`s_ref = V_top · (1.05 − weight)`**, `ω₀` from **Turn rate** (°/s) |
| **Climb speed / feel** | vertical | `V_climb` direct; `A_climb = 1.472·V_climb/t_top_z` |
| **Tracking stiffness** | (advanced, hidden by default) | `K = 4 / t_top` default |

Compile also clamps every derived value to sane bounds and rejects NaN (verdict on reject).

---

## 10. Presets (starting points — tune in-world, numbers are proposals)

| Preset | Cruise | Top | t_top | d_stop | t_hold | t_coast | float | t_j | Grip | Turn rate | Turn wt | Climb | Release | Frames |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| **Cine** | 2.0 | 6 | 3.0 | 8 | 1.2 | 6 | 0.7 | 0.15 | 0.35 | 45°/s | 0.7 | 1.0 | Hold | Drone |
| **Normal** | 4.0 | 12 | 2.0 | 10 | 0.8 | 4 | 0.5 | 0.10 | 0.6 | 70°/s | 0.5 | 2.0 | Hold | Drone |
| **Sport** | 8.0 | 30 | 1.6 | 18 | 0.6 | 3 | 0.3 | 0.07 | 0.8 | 110°/s | 0.4 | 4.0 | Drift | Drone (+bank) |
| **Dolly** | 1.0 | 3 | 2.5 | 3 | 1.0 | 5 | 0.8 | 0.20 | 0.9 | 25°/s | 0.2 | 0 (locked) | Hold | Drone |
| **FPV** | 10.0 | 40 | 1.0 | 25 | 0.5 | 2 | 0.2 | 0.05 | 0.5 | 200°/s | 0.3 | 6.0 | Drift | Free-fly |

Preset switching eases every parameter over 0.5 s (no mid-shot jerk). User edits create "Custom (from X)".

---

## 11. Integration details

### 11.1 Hook point
Inside `moveFlycam`, after the reset early-out (F9) and before the Orbit block (F10):
```
if (FlycamFlightModel == Drone && !orbitWillOwnPose):
    raw = read axes/buttons exactly as the default path does (same calls, same mapping)
    in  = shape(raw, triggerMode)
    pose = gDrone.advance(frame_dt_unclamped, in)
    prevPos = sFlycamPosition; prevRot = sFlycamRotation
    sFlycamPosition = pose.p;  sFlycamRotation = pose.q_cam
    synthesize sDelta (11.3);  zoom: existing CAM_W handling unchanged
else:
    existing BD/default code, untouched
```

### 11.2 Seeding & resets
- `moveFlycam(true)` / `mResetFlag` → `gDrone.reset(camera pose)`: `p` from origin, `ψ`/`θ` extracted from the
  camera quaternion (yaw about world up, pitch about the resulting right axis; any roll dropped if
  AutoLeveling, else kept as `φ` bias), `v = 0`.
- **Classic → Drone mid-flight:** seed `v` from the current camera-local `sDelta[0..2]/dt` rotated to world
  → no pop. **Drone → Classic:** seed `sDelta` from the current interpolated velocity × dt.
- Teleport / Director cut / Cinematic Camera taking the camera → existing reset path fires → `v = 0`.

### 11.3 Operator and Orbit compatibility
- The operator expects camera-local per-frame displacement in `sDelta[0..2]` and (roll, pitch, yaw) rates in
  `sDelta[3..5]` (F11). Drone writes them from the **interpolated** frame-to-frame pose delta, transformed into
  camera-local axes. The operator therefore sees real drone velocity (boost ⇒ harder gait/shake) with **no
  operator change**.
- **Orbit:** when Orbit is enabled and its anchor resolves, Orbit owns the pose (as today) and consumes
  `sDelta` as orbit controls. In that state Drone is bypassed for the frame (Classic deltas are produced so
  Orbit's controls behave exactly as now) and Drone `v` is zeroed; on Orbit exit, Drone re-seeds from the
  camera pose. Rationale: orbit controls are semantic (azimuth/elevation/radius), not velocities.

### 11.4 Build mode
`FlycamBuildModeScale` multiplies `V_cruise`, `V_top`, `V_climb` while in build mode (same intent as today).

---

## 12. Controls — Xbox Drone layout (defaults; all through the existing mapping UI)

| Control | Drone |
|---|---|
| Left stick | Move (forward/back, strafe), horizon plane |
| Right stick X / Y | Yaw / gimbal tilt |
| RT / LT | Throttle / brake (CLUTCH) — roll R/L (ROLL) |
| A / B | Ascend / descend (existing Jump/Crouch) |
| LB / RB | Zoom out / in (unchanged) |
| D-pad ← / → | Roll L/R (CLUTCH only) |
| D-pad ↑ | Hold ↔ Drift |
| D-pad ↓ | Recentre gimbal (θ → 0 over 0.4 s) |
| View/Back | Cycle preset |
| R3 (hold) | Momentary CLUTCH |
| (unassigned) | Latched ROLL ↔ CLUTCH |

Optional **Mode 2 (RC)** layout: left stick = climb + yaw, right stick = forward + strafe.
Non-Xbox pads: same model, user maps axes in the existing floater.

---

## 13. Settings (`settings_alchemy.xml`)

| Key | Type | Default | Range |
|---|---|---|---|
| `FlycamFlightModel` | S32 | 0 (Classic) | 0 Classic / 1 Drone |
| `FlycamDronePreset` | S32 | 1 (Normal) | 0..4, 5 = Custom |
| `FlycamDroneCruise`, `…Top`, `…TimeToTop`, `…StopDistance`, `…HoldTime`, `…CoastTime`, `…HighSpeedFloat`, `…Onset`, `…Grip`, `…TurnRate`, `…TurnWeight`, `…Climb` | F32 | per Normal preset | §9 bounds |
| `FlycamDroneRelease` | S32 | 0 Hold | 0 Hold / 1 Drift |
| `FlycamDroneThrottleMode` | S32 | 0 Assist | 0 Assist / 1 Pure |
| `FlycamDroneFreeFly` | BOOL | false | |
| `FlycamDroneSimulationHz` | F32 | 120 | 30..480 |
| `FlycamDroneTriggerMode` | S32 | 0 Roll | 0 Roll / 1 Clutch |
| `FlycamDroneSwapTriggers` | BOOL | false | |
| `FlycamDroneThrottleAxis`, `FlycamDroneBrakeAxis` | S32 | −1 (use combined roll axis) | axis index |
| `JoystickButtonTriggerMode`, `JoystickButtonClutchHold`, `JoystickButtonReleaseToggle`, `JoystickButtonGimbalRecentre`, `JoystickButtonPresetCycle` | S32 | see §12 | button index |
| `FlycamDroneMoveDeadzone`, `…LookDeadzone`, `…TriggerDeadzone`, `…Saturation` | F32 | 0.10 / 0.12 / 0.08 / 0.05 | 0..0.5 |
| `FlycamDroneExpoMove`, `…ExpoYaw`, `…ExpoGimbal`, `…ExpoThrottle` | F32 | 0.35 / 0.45 / 0.45 / 0.25 | 0..1 |
| Extras: `FlycamDroneBank`, `…FovKick`, `…LookAccel`, `…SpeedLook`, `…LookAhead` | F32 | 0 (off) | 0..1 |
| `FlycamDroneTelemetry` | BOOL | false | |

All registered with flycam reset/preset save. **CLAUDE.md rule 6:** any positional preset table in code is
verified by counting rows/fields after edit and cross-checking against §10.

---

## 14. UI
- **Joystick floater → "Flight model" section:** Classic/Drone, preset combo, the §9 human-terms sliders,
  Release (Hold/Drift), Throttle (Assist/Pure), Free-fly, trigger mode + swap, new button assignments,
  shaping sliders under "Advanced".
- **Flycam / Director panel:** preset combo, Release, Trigger mode (reachability rule).
- **Toasts:** trigger mode, release mode, preset — hidden with UI.
- **Telemetry graph** (`FlycamDroneTelemetry`): scrolling 5 s plot of |v_h|, throttle, brake, |a|, yaw rate;
  numbers for current preset's derived coefficients.

---

## 15. Instrumentation — log states a verdict
Once per 5 s while Drone is active (`LL_INFOS("FlycamDrone")`):
`preset=… hz=… steps/s=… backlog=… vmax=… a_max_seen=… jerk_max_seen=… clamp_hits=…`
- `DRONE OK`
- `DRONE NONFINITE` — state reset (must never happen; it's a bug report)
- `DRONE BACKLOG <s>` — accumulator above 0.1 s (frame hitch; informational)
- `DRONE JERK-EXCEEDED` — measured |Δa|/h > J_max·1.01 (limiter broken)
- `DRONE TRIGGER-REST-DRIFT` — throttle or brake > 0 for 2 s with the raw axis at rest value (split broken /
  sign wrong)

---

## 16. Verification

**Unit tests (TUT, `indra/newview/tests/llflycamdrone_test.cpp` — pure class, no viewer):**
1. Fixed-step determinism: identical input sequences at frame dts {1/30, 1/60, 1/144, jittered} → final pose
   equal within 1e-4 m.
2. `t_top`: full throttle from rest reaches 0.9·V_top within ±5 % of `t_top`.
3. `d_stop`: full brake from V_top stops within ±5 % of `d_stop`.
4. `t_coast`: Drift from cruise reaches 10 % within ±10 % of `t_coast` (c₂ = 0 case exact; c₂ > 0 shorter).
5. Jerk: max |Δa|/h ≤ J_max·1.001 across a stick-slam sequence.
6. Hold: release at cruise → |v| < v_ε within `t_hold`·1.1, no overshoot (no sign change of along-track v).
7. Horizon lock: θ = −80°, full forward → Δz = 0 (±1e-6) in Drone; non-zero in Free-fly.
8. Seeding: reset from an arbitrary quaternion → extracted ψ/θ reproduce the camera forward within 0.1°.
9. NaN injection → `NONFINITE` path, finite pose retained.

**In-world test (outcomes stated in advance):**
1. **Trigger split.** Clutch, Joystick floater open: RT alone → throttle only; LT alone → brake only;
   released → neither, no `TRIGGER-REST-DRIFT`.
2. **Horizon lock.** Gimbal straight down, push forward → level flight (Classic: dive, as today).
3. **Mass.** Normal preset, Drift: reach cruise, release → long glide ≈ `t_coast`. Hold: eased hover stop.
4. **Clutch mid-flight.** Cruise, squeeze RT half → full: smooth, increasing pull, tapering near top speed;
   LT: progressive slow-down.
5. **Turn weight.** Sport at high speed, swing stick 90° → wide carve; same at low speed → tight.
6. **Toggle.** Roll↔Clutch mid-move → no jolt; toast shows mode.
7. **Frame-rate independence.** Same stick+trigger performance with fps capped 30 vs uncapped → same feel and
   distance (telemetry graph comparable).
8. **Operator.** Operator ON, boost → gait/shake intensifies with speed; OFF → clean.
9. **Orbit.** Enable Orbit mid-flight → Orbit behaves exactly as today; exit → Drone resumes without pop.
10. **Classic off-path.** `FlycamFlightModel = 0` → indistinguishable from today.

---

## 17. Off-path inert
`FlycamFlightModel = Classic` (default for the first in-world A/B): both existing paths run unchanged, no
Drone state touched, no new reads. No render state touched anywhere in this feature.

---

## 18. Reviewer — attack these first
1. **Trigger axis reality.** Confirm how the DirectInput path reports an Xbox pad's triggers (one axis or two,
   rest value, sign) and that the split has no rest drift. 3D mice (`m3DCursor` absolute mode) must stay Classic
   or be explicitly handled — which?
2. **sDelta synthesis (§11.3).** Prove the operator receives the same units/axes as today (camera-local,
   per-frame displacement; (roll, pitch, yaw) order) and that Orbit's consumption is unaffected when Orbit owns
   the pose.
3. **Interpolation vs camera write.** The interpolated pose is written to `sFlycamPosition/Rotation`; the
   simulated state lives in `LLFlycamDrone`. Check nothing else reads `sFlycamPosition` expecting the
   integrated (not interpolated) value.
4. **Seeding maths** for ψ/θ extraction near ±90° pitch (gimbal lock) and with roll present.
5. **The derivations in §9** — especially `t_top` with taper + jerk limit (jerk adds ≈ t_j/2 delay; state
   whether compile should compensate).
6. **Is the approach wrong?** Argue target-velocity tracking (this paper) vs pure force/thrust integration;
   argue whether Drone should also replace the BD path or only the default path.

---

## 19. Work packages

| WP | Content | Owner | Review |
|---|---|---|---|
| 1 | `LLFlycamDrone` core: state, §6 maths, §7 fixed step + interpolation, §9 compile, NaN guards | **Fable** | Opus adversarial |
| 2 | TUT unit tests §16 (written against the WP1 interface, by a different agent than WP1) | **Codex** | Opus adversarial |
| 3 | `moveFlycam` hook: input shaping §5, trigger split, Roll/Clutch §8, seeding/resets §11.2, sDelta synthesis + Orbit/operator §11.3, build mode | **Codex** | Opus adversarial |
| 4 | Settings §13, presets §10 (rule-6 counting), reset/preset registration | **Sonnet** | Opus adversarial |
| 5 | UI §14 (floater section, panel, toasts, telemetry graph) | **Sonnet** | Opus adversarial |
| 6 | Instrumentation §15 + `MACHINIMA_USER_GUIDE.md` section | **Opus** | independent Opus adversarial |

Each implementer gets an explicit OFF-LIMITS file list. Fixes batched and re-deferred until 0 must-fix across
all packages; then one build; then the in-world test.
