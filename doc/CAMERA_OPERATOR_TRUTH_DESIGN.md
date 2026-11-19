# TRUTH Operator — Design Document
## A state-derived camera operator and motion model for Alchemy-Machinima

**Replaces the internals of:** `indra/newview/llcameraoperator.{h,cpp}` (`LLCameraOperator`)
**Integration site (unchanged):** `LLCinematicCamera::updateCamera()` — operator output composes final rotation, camera-local position offset, and `fov_mul` immediately before `LLViewerCamera` is written.
**Output contract (unchanged):** `LLCameraOperatorOutput { mRoll, mPitch, mYaw, mPosOffset (camera-local), mFovMul }`.

> Verified against the code: the operator already receives `LLCameraOperatorInput` with `mDeltaTime`, camera-local `mLinearVel`, `mAngularVel` differentiated from the **base** (pre-operator) pose (`mPrevPos`/`mPrevRot` store base pose, not final) — so a no-feedback seam already exists. `updateFromPose()` provides an alternate fixed-tick path gated by `FlycamOperatorLocomotionMode`.

---

## 1. Design philosophy

The current operator is a **noise generator wearing a costume**. It synthesizes a walk cycle whether or not anyone is walking, because it has no idea whether anyone is walking. The fix is not better noise — it is inverting the architecture:

> **The operator never *invents* motion. It *responds* to motion that actually happened.**

A real camera operator is a physical system excited by real inputs: their own gait when they walk, their breathing when they stand, their arm inertia when the camera accelerates, their reaction time when the subject moves. Every one of those inputs is readable in the viewer every frame.

```
REAL STATE (camera kinematics + avatar locomotion)
        │  sample & filter
        ▼
MOTION STATE (velocities, accelerations, gait phase, locomotion class)
        │  excites
        ▼
OPERATOR BEHAVIORS (breath, tremor, gait, inertia, reframe)  ← presets scale these
        │  sum, clamp
        ▼
LLCameraOperatorOutput (unchanged struct)
```

Noise still exists — but only as **texture inside truth-derived envelopes**. When the envelope is zero (standing still ⇒ gait envelope = 0), the layer is *silent by construction*, not by tuning. "Standing still looks like standing still" is the resting state of the model, not a special case.

---

## 2. Truth signals (what we read, exactly)

### 2.1 Camera kinematics — `CameraMotionSampler`
Read the **base pose** (post-cinematic-solve, **pre-operator**) each update, fed by `LLCinematicCamera`, never by `LLViewerCamera::getInstance()` (which contains last frame's operator output — see §6).

| Signal | Derivation | Filtering |
|---|---|---|
| Linear velocity `v` (world → camera-local) | `(base_pos − prev_base_pos)/dt` | EMA low-pass, τ = 60 ms |
| Linear acceleration `a` | differentiate **filtered** `v` | second EMA, τ = 120 ms |
| Angular velocity `ω` | `dq = base_rot * ~prev_rot` → axis-angle / dt | EMA, τ = 60 ms |
| Angular acceleration `α` | differentiate filtered `ω` | EMA, τ = 120 ms |
| Ground speed `s` | `|v.xy|` | dead-zone: `s < 0.02 m/s ⇒ 0` |
| Jerk proxy `J` | `|Δa|/dt`, low-passed τ = 250 ms | drives tremor gain |

**Dead-zone rationale:** SL positions are quantized/network-interpolated; a parked camera shows sub-cm velocity noise. Dead-zone + low-pass guarantee a parked camera reads as exactly stationary.
**Discontinuity guard:** `|Δpos| > 5 m` in a tick, switcher cut serial change, or `dt ≤ 0` ⇒ treat as a cut: reset filters/springs to new pose, zero derivatives. (Hook: existing `reset()` on `mLastSwitcherCutSerial` change.)

### 2.2 Avatar locomotion — `AvatarLocomotionProbe`
From `LLVOAvatarSelf`/`gAgentAvatarp` (or the cinematic subject):

| Signal | Source | Used for |
|---|---|---|
| Locomotion class {sit,stand,walk,run,fly,hover,air} | `isSitting()`, `gAgent.getFlying()/getRunning()`, signaled anims `ANIM_AGENT_WALK/RUN/FLY/HOVER/SIT/STAND` | gate gait, breath posture |
| Avatar velocity | `avatar->getVelocity()` | cross-check gait gate, cadence |
| Walk-anim playback time (if retrievable) | `avatar->findMotion(ANIM_AGENT_WALK)` → runtime | footfall phase hint (optional; PLL survives without it) |

**Locomotion gate (hysteresis):** engage at 0.5 m/s, release at 0.3 m/s, 200 ms debounce both ways; class ∈ {walk,run}.

### 2.3 Who drives the gait? (coupling)
1. **Primary: camera ground speed `s`.** `gait_env = smoothstep(0.4,1.0,s)·clamp(s/3.2,0,2)`. Static camera filming a walker ⇒ **zero** bob (the operator is standing).
2. **Phase/cadence hint: coupled avatar.** When camera is mounted-on/following an avatar (`|v_cam − v_avatar| < 0.5 m/s` sustained 0.5 s), lock gait *phase* to that avatar's anim and AND-gate on `loco_active` (kills the "smooth dolly at 1.4 m/s while avatar stands" phantom). Uncoupled free-flycam uses rule 1 alone.
3. **Flying/sitting always closes the gait gate.**

"Standing still ⇒ no walk bob" falls out: `s=0` ⇒ `gait_env=0` ⇒ gait layer contributes exactly 0. This is Phase 1 because it is one multiplication.

---

## 3. Operator behaviors (signal → output)
All camera-local, summed, clamped (<5° total; small-angle euler safe).

### 3.1 Breath & postural sway (idle truth, always on, preset-scaled)
- **Breathing:** pitch osc at **0.25 Hz**, amp 0.03–0.12°, + vertical `mPosOffset.z` 1–3 mm, phase-locked; period ±10 % irregular (seeded) so it's not a metronome.
- **Postural drift:** **Ornstein–Uhlenbeck** on yaw/pitch (`dθ = −θ·rate·dt + σ·dW`) — the classic "drift off frame and ease back"; mean-reversion is the operator correcting. σ≈0.15°/√s, rate≈0.8/s (Doc); 0 for Tripod/Locked.
- Breath amp ×1.5–2 when `recent_exertion` high (low-passed integral of gait_env+|a|, τ=8 s) — winded after a run.

### 3.2 Micro-tremor (physiological hand shake)
6–12 Hz band-limited rotational noise, amp 0.005–0.03°. Keep the existing seeded value-noise (deterministic), band-passed. Gain = `base·(1 + k_J·J)` — grows under hard accel. Tripod: gain keyed to `|ω|` (fluid-head buzz only while panning, silent parked).

### 3.3 Gait bob (footstep-synced, only when real)
**Phase-locked loop, not a free LFO:**
```
step_freq(s) = clamp(1.35·sqrt(s/stride_len), 1.4, 3.2)  # Hz; stride≈0.75 m walk, 1.1 m run
φ           += 2π·step_freq(s)·dt·gait_gate               # integrates ONLY while gated
if anim_phase_hint: φ += k_pll·wrap(φ_anim − φ)·dt        # k_pll≈4/s soft lock to real footfalls
```
Phase **stops advancing** the instant locomotion stops. **Footfall-impulse synthesis:** on `φ` crossing 0 or π, kick a **critically damped vertical spring** (ω_n≈18, ζ=1) with impulse `A_v·gait_env` — asymmetric thump-and-recover; stopping mid-stride just decays.
- Vertical `A_v`: 4 mm@walk → ~18 mm@run, ×preset **transmissibility T** (Steadicam arm ≈ 0.1).
- Lateral `A_l·sin(φ/2)` (stride = half step freq), 3–8 mm. Roll `0.25°·T·sin(φ/2)`. Forward surge ~2 mm, 90° lead.

### 3.4 Inertia: lag/overshoot/settle (driven by real acceleration)
Operator aim = spring-damper follower of base orientation (quaternion domain):
```
aim_err  = log(base_rot * ~operator_aim)
aim_vel += (ω_n²·aim_err − 2·ζ·ω_n·aim_vel)·dt
operator_aim = operator_aim * exp(aim_vel·dt)
output_rot_offset = log(operator_aim * ~base_rot)
```
ζ=1.0 Tripod/Steadicam (pure lag-settle); ζ=0.6–0.7 Run-and-Gun/Doc (overshoot on whips). ω_n: Tripod 9, Steadicam 4.5, Doc 6, RunGun 7. **Zero when base isn't rotating** — a whip auto-produces lag→overshoot→settle because the camera actually moved. Replaces the synthetic reaction-lag term with the physical thing.
**Translational inertia:** `pos_lag_target = −k_a·a_local` (≤4 cm) through a critically damped spring (ω_n≈10). k_a: 0 (Tripod) … 0.012 s² (RunGun).

### 3.5 Reframe lag ("operator notices, then corrects")
VOR-style self-stabilization is near-perfect (so we don't add error from base rotation beyond §3.4), but external subject tracking has latency: push subject-bearing into a **delay line** (τ=120–300 ms), chase `(delayed − current)` through a **±1–2° dead-zone** + underdamped spring. Subject still ⇒ zero contribution.

### 3.6 FOV breathing (Doc only, optional v1)
`mFovMul` ±0.5 % OU + a small zoom-hunt kick when reframe fires a big correction. Other presets: exactly 1.0.

---

## 4. Composition & clamps
```
out.rot = inertia + reframe + breath + tremor + gait_roll
out.pos = gait_pos + inertia_pos + breath_pos
out.fov = fov_layer or 1.0
clamp |rot|≤5°, |pos|≤0.15 m ; slew ≤ (30°/s, 0.5 m/s) anomaly firewall
```

---

## 5. Determinism vs truth-reactivity — the two-feed stance
Live camera velocity is a function of this session's frames, not `presentation_time`. Don't pretend otherwise — split modes.

**One pure core, two feeds.** Operator core = pure fixed-step `(state,input,seed)→(state',output)`, no wall clock, no `LLViewerCamera` reads. Inputs via `MotionFeed`:
- **LiveFeed** (real-time): sampled from live base camera + avatar each render frame. Reactive, truthful, not scrub-safe — fine, nobody scrubs live operation. `dt` from `LLPresentationTime` deltas (paused ⇒ dt=0 ⇒ freezes).
- **RecordedFeed** (playback/scrub): the base path in a take is already a deterministic function of `presentation_time`; finite-difference *that* at fixed ticks ⇒ reproducible inputs ⇒ identical output every scrub.

**Fixed ticks + snapshots.** Keep `mFixedStep = 1/120` + `RenderPoseSegment`. Snapshot full operator state every 240 ticks (2 s); scrub restores nearest ≤ T and fast-forwards ≤240 ticks. Per-build/per-machine bitwise identity (cross-machine float identity out of scope — render is on one box).

**Avatar-truth hole plug:** record a per-take **locomotion track** `{class:u8, speed:f16, gait_phase:f16}` ≈5 B/tick, RLE-compressible. Old takes without it degrade to camera-speed gating. **Recording stance:** takes store base path + seed + preset + locomotion track; operator re-synthesizes on playback (shake stays editable — swap Doc→Steadicam after the fact); provide explicit **"Bake Operator"** export.

---

## 6. Numerical stability & feedback
1. **No self-reading** — input is base pose differentiated before the operator; make it an architectural rule (`LLCameraOperator` never calls `LLViewerCamera::getInstance()`). Positive-feedback howl becomes impossible, not merely damped.
2. **Everything is springs and low-passes** — every raw derivative low-passed; every output through a spring ζ≥0.6 (≥0.85 positional). No layer emits a step.
3. **Hysteresis + debounce on all gates** (gait engage 0.5 / release 0.3 m/s, 200 ms).
4. **Fixed-step 1/120** ⇒ unconditionally stable springs, frame-rate independent.
5. **Cut/teleport reset** + output clamps + slew limits as firewall.

---

## 7. Presets re-founded on truth (physiology + rig physics)

| Parameter | Locked-Off | Tripod | Steadicam | Doc Handheld | Run-and-Gun |
|---|---|---|---|---|---|
| Breath amp (°/mm) | 0 | 0 | 0.03/1 | 0.10/3 | 0.12/3 |
| OU σ (°/√s) / revert (1/s) | 0 | 0 | 0.06/0.5 | 0.15/0.8 | 0.20/1.2 |
| Tremor amp (°) / accel-gain | 0 | 0.004, \|ω\|-keyed | 0.005/0.3 | 0.015/1.0 | 0.03/2.0 |
| Gait transmissibility T | 0 | 0 | 0.10v/0.25l | 0.7 | 1.0 |
| Aim spring ω_n / ζ | ∞ (bypass) | 9/1.0 | 4.5/1.0 | 6/0.75 | 7/0.6 |
| Pos-lag k_a (s²) | 0 | 0 | 0.010 | 0.006 | 0.012 |
| Reaction τ / dead-zone | — | 150ms/0.5° | 200ms/1.0° | 280ms/1.8° | 120ms/1.2° |
| FOV breathing | off | off | off | ±0.5 % | off |

**Locked-Off** = true bypass (identity, zero cost). **Tripod** = nothing at rest, only lag/friction-buzz/settle during pans. **Steadicam** floats (gait isolated by T=0.1, soft aim lag, gentle drift). **Doc** breathes/drifts/corrects/hunts. **Run-and-Gun** transmits everything, overshoots whips (ζ=0.6), fast-but-sloppy. The recently-added reaction-lag knob survives as the τ/dead-zone column, now driven by measured subject bearing. Persona variation = ±15 % table randomization from take seed (deterministic).

---

## 8. Motion-blur foundation (stretch — feasible, clean, no shake compromise)
**Verdict: shared foundation is clean.** Blur needs only "publish final camera pose per tick" — produced anyway.

**Shared kernel: pose history ring buffer**
```cpp
struct LLCameraPoseSample { F64 t; LLVector3 pos; LLQuaternion rot; F32 fov; };
// Ring of FINAL (post-operator) poses at ticks, depth 32 (~266 ms), written by LLCinematicCamera after composition.
```
Operator reads *base* poses; buffer stores *final* poses — strictly downstream, no feedback.

**Shutter angle → blur:** exposure `E = (θ/360)·(1/f)` (24 fps @180° ⇒ 1/48 s = 2.5 ticks @120 Hz). Velocity-buffer/reprojection: `VP_open` from pose-history at `t−E`, `VP_close` current; per-pixel motion vector = clip-space delta of depth-reconstructed world pos; directional blur (8–12 taps, depth-aware).
**Consistency guarantee:** blur streaks include operator motion (Run-and-Gun footfall thump blurs vertically; Locked-Off has zero camera blur; whip blurs exactly as far as the lagged/overshooting final camera swept). Shake and blur can't disagree — two readouts of one pose sequence.
**Interchange:** blur is a pure consumer (enable/disable freely; operator off ⇒ base poses ⇒ still correct for dolly/pan). Scrub-safe by inheritance. Object motion blur out of scope for v1 but reserve the target format. ReShade present but guesses velocity from color/depth — a native pass with true camera velocity is strictly better; optionally expose the velocity target to ShaderToggler/ReShade addons.

---

## 9. Phased implementation plan
- **Phase 0 — Instrument (2–3 d):** `CameraMotionSampler` + `AvatarLocomotionProbe` + debug HUD (v, a, ω, gait gate, loco class, phase). No behavior change. Becomes the tuning tool for every later phase.
- **Phase 1 — The truth gate (1–2 d). Biggest win, smallest diff:** multiply existing synthetic walk-bob by `gait_env × loco_gate`; route idle/breath as the only idle contributor. **Phantom walk-bob dies here.** Risk: gate flicker → hysteresis/debounce, verify on HUD.
- **Phase 2 — Inertia on truth (1 wk):** aim spring + translational pos-lag; retire synthetic reaction-lag (map UI knob → τ_react). Risk: quaternion spring log/exp correctness — unit-test scripted whips. *(Do the pose-history buffer here too — 40 lines.)*
- **Phase 3 — Real gait (1–1.5 wk):** PLL phase + footfall-impulse bob replaces synthetic walk cycle; anim-phase hint if available, PLL-only otherwise; coupling detection. Risk: anim API may not expose phase — design survives on PLL alone.
- **Phase 4 — Determinism hardening (1 wk):** `MotionFeed` split, 240-tick snapshots, locomotion track in takes, scrub-identity test (render frame N twice from different scrub directions, assert bitwise-equal). 
- **Phase 5 — Motion blur (1–2 wk, independent):** velocity/reprojection pass + shutter-angle UI. Motion-model side already done.

**Cross-cutting:** network jitter aliasing (dead-zone+low-pass, watch HUD); perf non-issue (<0.05 ms/frame); per-machine-only float determinism (accepted, documented).

---

## Appendix A — Per-tick pseudocode
```
tick(state, feed, seed, tick_index):            # dt = 1/120 fixed
  in = feed.sample(tick_index)                  # base pose + avatar truth (Live or Recorded)
  if in.is_cut: state.reset_to(in.base_pose); return IDENTITY
  v = lowpass(state.v, (in.base_pos-state.prev_pos)/dt, 60ms)
  a = lowpass(state.a, (v-state.v_prev)/dt, 120ms)
  ω = lowpass(state.ω, axis_angle(in.base_rot*~state.prev_rot)/dt, 60ms)
  s = deadzone(|v.xy|, 0.02)
  gate = hysteresis_debounce(state.gate, s>0.5 && in.loco_ok, s<0.3, 200ms)
  genv = smoothstep(0.4,1.0,s)*gate*preset.T
  φ += 2π*step_freq(s)*dt*gate; if hint: φ += k_pll*wrap(in.anim_phase-φ)*dt
  if footfall_crossing(φ): state.gait_spring.kick(A_v(s)*genv)
  breath  = breath_osc(seed,t)*preset.breath*exertion_scale(state)
  drift   = ou_step(state.ou, preset.σ, preset.revert, noise(seed,t), dt)
  tremor  = bandpass_noise(seed,t,6..12Hz)*preset.tremor*(1+k_J*state.J)
  gait    = { z:state.gait_spring.step(dt), y:A_l*sin(φ/2)*genv, roll:0.25°*sin(φ/2)*genv }
  inertia = quat_spring_step(state.aim, in.base_rot, preset.ω_n, preset.ζ, dt)
  poslag  = spring_step(state.plag, -preset.k_a*a_local, 10, 1, dt)
  reframe = spring_step(state.rfrm, deadzone(delayline(in.subject_bearing,preset.τ)-in.subject_bearing, preset.dz), preset.ζ, dt)
  out.rot = clamp(inertia+reframe+breath+drift+tremor+gait.roll, 5°)
  out.pos = clamp(gait.pos+poslag+breath.pos, 0.15m)
  out.fov = preset.fov_breathing ? fov_ou(state) : 1.0
  pose_history.push(compose(in.base_pose, out))   # motion-blur feed
  return out                                       # LLCameraOperatorOutput, unchanged
```

## Appendix B — File plan
- `llcameraoperator.{h,cpp}` — rewritten internals; `LLCameraOperatorOutput` unchanged; `LLCameraOperatorInput` extended with locomotion fields (keep existing `mDeltaTime/mLinearVel/mAngularVel`).
- `llcameramotionfeed.{h,cpp}` — `MotionFeed` interface + `LiveFeed`/`RecordedFeed`, sampler, probe.
- `llcameraposehistory.h` — shared ring buffer (blur foundation).
- `llcinematiccamera.cpp` — near-zero diff; operator call site keeps shape; `reset()`-on-cut hook already exists.
