# Flycam "Weighted Classic" — Classic feel + mass, momentum and coast (design R3)

Status: DESIGN ONLY (no code). Branch `fix/animesh-clone-pose-polish`. Verified against HEAD `8aa3c922f7d` on
2026-10-06. Every `file:line` below was re-read for this revision. Unresolved items are marked **OPEN** with a
recommendation.

Implementation: Codex implements (`--prompt-file`, under the watchdog). A fresh Codex review follows each work
package, plus the adversarial Opus review that `CLAUDE.md:43` requires. Then ONE build at the end
(`CLAUDE.md:197-213`).

---

## R3 changes (Codex verify R3, orchestrator decision: least regression risk to Classic and Drone)

| # | Change | Sections |
|---|---|---|
| R3-1 | **The `ALFlycamFrame::run`/`Host` orchestrator is dropped.** `alflycamframe.{h,cpp}` and test O1 are removed. No existing `moveFlycam` block is moved, reordered or wrapped. | §3, §5.5, §11, §13 |
| R3-2 | **Minimal additive integration: ONE call site.** It sits just before `if (drone_frame)` at `llviewerjoystick.cpp:2452`, the same position in the frame order as the Drone overwrite: after the Classic shadow and discard (`:2422-2448`), before Orbit and the operator/camera tail. It is guarded by the frame owner and the Weighted edge from `alResolveFlycamFrame`. The glue is spelled out line by line in §5.6. | §3, §5.6 |
| R3-3 | **Every Weighted decision lives in the pure `ALFlycamWeighted::Coordinator`.** That covers invocation-time checks, edges, bypass, rests and reseeds, proven-route guards, the telemetry pose guard and focus-loss rest. It has an explicit `FrameIn` (every field enumerated) and `FrameOut`. | §5.6 |
| R3-4 | **The decision table stays legacy-compatible.** `alResolveFlycamFrame` treats WEIGHTED as Classic-family for **all existing fields**, so existing `moveFlycam` code sees the same edges and flags it handles today. A new field `weightedEdge` tells the Coordinator the precise transition. Two new legacy edge values exist only to drive the existing owner bookkeeping at `:2757-2798`. As a result:<br>— the existing seed and hand-off code is reused unchanged (W→D uses the C→D seed; D→W uses the D→C `classicSeed`);<br>— `seedWeighted` and `deactivateWeighted` are removed (the Coordinator derives them from `weightedEdge`). | §6 |
| R3-5 | **No Classic code change.** The `applyLeveling` extraction is dropped. The Coordinator runs Classic's own `ALFlycamClassic::finishIntegration` on a **scratch** state with a zero motion delta, which applies Classic's exact leveling to the weighted pose. The identity rotation is bit-exact (§7.1). | §4.3, §7.1 |
| R3-6 | **No changes to the operator or camera tail.** In Weighted, the handheld operator keeps Classic's `sDelta` feed (`:2850-2860`); a weighted feed is OPEN-11. The presented-pose telemetry check compares against the Coordinator's own last write, and only while the operator and shake are off (the camera then equals `sFlycamPosition`, `:2880-2886`). So no post-write hook is needed. | §7.4, §9.3 |
| R3-7 | **The D→W history seed moves to the first Weighted frame.** On the D→W edge frame the Coordinator records a pending seed. The next frame seeds from `sDrone.previousPose/currentPose`, which then hold the Drone edge frame (`:2750-2755`). | §6.3 |
| R3-8 | **Tests:** the Coordinator, the decision tables and I1-I13 are rewritten as `FrameIn` series replays. **Gate property:** for models 0 and 1 with `previousOwner ≠ WEIGHTED`, `alResolveFlycamFrame` equals a frozen copy of today's function field for field, and the glue condition is false. Classic golden 4/4 and Drone 49/49 are regression gates. | §11 |
| R3-9 | `mWeightedCameraOwner` and the Release/Recentre hint changes are dropped. The existing "Drone not active" hints are accurate in Weighted. The D-pad pair toggle and the Reset Drone change (user decisions a and c) remain the only edits outside the glue and the decision function. | §9.4, §13 |

## R2 changes (Codex design review R2) — retained where not superseded by R3

| # | Change | Status in R3 |
|---|---|---|
| R2-1 | The presented-pose check is telemetry-only by default, with a hidden arm and auto-disarm on POSECUT-STORM. Proven routes rest. | Kept (§7.4). |
| R2-2 | Same-region teleport re-derived from code (§2.3). | Kept. |
| R2-3 | Leveling bound: contraction plus injection; crossings ≤ 1 + the sign changes of the input; yaw–pitch coupling `asin(sin δψ·sin φ)` (W31). | Kept (§4.3). |
| R2-4 | Shared production orchestrator. | **Superseded by R3-1/R3-2.** |
| R2-5 | Distinct D-pad A/B at settings level; `toggleFlightModel` normalises. | Kept (§8, §9.4). |

## R1 changes (Codex design review R1, user decisions 2026-10-06) — summary

| # | Change |
|---|---|
| R1-1 | The D-pad toggles a user-selectable pair (`FlycamDpadModelA`/`B`, default Classic ↔ Weighted). |
| R1-2 | World-frame momentum (OPEN-1 closed). |
| R1-3 | Reset Drone never touches `FlycamFlightModel` (OPEN-8 closed). |
| R1-4 | Every W→C transition is an immediate Classic frame: no advance, no hand-off, no `sDelta` write. |
| R1-5 | G1 reworded: the filter is exact; the complete model inherits Classic's fps dependence. |
| R1-6 | Shadow-loop soundness stated explicitly (W27). |
| R1-7 | Leveling treated as pose feedback (W21, W28-W30). |
| R1-8 | `isExtremeInput` rename; pose guard added. |
| R1-9 | Focus loss: Weighted always rests and ignores `FlycamDroneResumeMomentum`. |
| R1-10 | Testable seam (now the R3 Coordinator). |
| R1-11 | UI gates: `--selftest`, then `--changed`, then first open. |

---

## 0. The ask (user, 2026-10-06)

> "I'm not happy with how the drone feels, I want it to feel more like the current flycam settings but with
> weight, mass and momentum."

| # | Decision (final) |
|---|---|
| U1 | Classic's exact feel is the **target**. All of Classic is unchanged: axis mapping, scales (including negative-scale inversion), dead zones, the BD and non-BD integrators, feathering, the XInput trigger ROLL/THROTTLE modes and throttle on all axes, zoom/FOV, Level horizon, auto-leveling and build mode. |
| U2 | A **mass layer** sits between Classic's per-frame motion and the camera: it eases in, carries momentum, and **coasts to a stop**. The coast length is set by the weight. |
| U3 | Weight applies to **movement and rotation**. |
| U4 | **One** "Weight" slider, 0-100 % (0 % = bit-exact Classic), plus presets. |
| U5 | `FlycamFlightModel`: 0 Classic, 1 Drone, **2 Weighted Classic**. Drone stays. |
| U6 | The D-pad toggles a **user-selectable pair**. The default is Classic ↔ Weighted. |
| U7 | Momentum keeps the old direction (world frame). |
| U8 | Reset Drone and Reset Weighted reset only their own tuning, never the flight model. |
| U9 (R3) | **Least regression risk to the just-shipped Classic and Drone.** Additive integration only. |

---

## 1. Summary

- **Integration (R3).** Classic and Drone code paths are untouched. Weighted adds:
  - **one guarded call site** in `moveFlycam`, at the Drone-overwrite position (§5.6);
  - extended rows in `alResolveFlycamFrame`;
  - the D-pad pair toggle and the Reset Drone fix the user asked for.

  The call site runs only on Weighted frames and Weighted edges. Classic always runs as the shadow, starting from
  the last presented pose. Its per-frame increment (`sDelta[0..5]`) is the target, and the Coordinator's pose
  overwrites the shadow pose.
- **Filter.** Each axis has a critically damped, two-pole velocity follower (two equal first-order lags in
  cascade).
  - Translation is filtered in the world frame. Rotation is a body-frame angular velocity integrated on the
    quaternion.
  - The filter is advanced with its exact closed form under a zero-order hold. It is stable for any `dt`, has no
    overshoot, and has continuous acceleration.
  - **For a given target signal the filter adds no frame-rate dependence.** The complete model inherits Classic's
    own fps dependence (F1).
- **Leveling** runs after the mass step through **Classic's own `finishIntegration`** on a scratch state (no Classic
  code change). It is pose feedback that never writes filter state. It is bounded, with crossings ≤ 1 + the sign
  changes of the input (§4.3).
- **Zoom/FOV is unweighted**, and so is the operator feed for now (OPEN-11).
- **0 % is exact Classic:** the effective model is Classic, and every W→C transition is an immediate Classic frame.
- **Mapping and presets:**
  - `tau_t = 1.0 s·(20^w − 1)/19` with `w = Weight/100`; `tau_r = 0.6·tau_t`;
  - Light 25 %, Steadicam 50 % (default), Heavy 70 %, Crane 90 %.
- **UI:**
  - the Drone tab becomes "Flight model", with a Weighted section;
  - the Xbox Pad tab gains the pair combos;
  - the Director panel lists Weighted and swaps in the Weighted controls.

---

## 2. What the code proves today (verified, HEAD `8aa3c922f7d`)

### 2.1 Classic — `indra/newview/alflycamclassic.{h,cpp}`

| Ref | Fact |
|---|---|
| `alflycamclassic.h:20-75` | `Input`; `State` (references to `sLastDelta`, `sDelta`, pose, zoom); `Intermediate {blackDragon, isZero, time, feather}`; `integrateDeltas`, then `finishIntegration`. |
| `alflycamclassic.cpp:30` | BD clamps `time` to [0.016, 0.033]. |
| `alflycamclassic.cpp:182-185` | Non-BD clamps `time` to ≤ 0.2. |
| `alflycamclassic.cpp:47-51` | BD feather 3.0 at the zoom caps. It reads zoom only. |
| `alflycamclassic.cpp:56-124, 192-243` | The delta pipeline reads only `in.*`, `lastDelta` and `delta`. **`sDelta`/`sLastDelta` evolve independently of pose.** |
| `alflycamclassic.cpp:100-102, 217-219` | The throttle scales axes 0..5. |
| `alflycamclassic.cpp:107-113, 224-230` | Build scale applies to translation only. |
| `alflycamclassic.cpp:122, 239` | Frame-stepped feather. `sDelta[i]` is a per-frame increment. |
| `alflycamclassic.cpp:140-142, 255-258` | `pos += LLVector3(sDelta[0..2]) * rot` (pre-rotation `rot`); then `rot = LLQuaternion(LLMatrix3(sDelta[3],sDelta[4],sDelta[5])) * rot`. |
| `alflycamclassic.cpp:144-161, 260-276` | Leveling. BD snaps on `levelHorizon`/`rollDefault==1`; non-BD snaps on `levelHorizon`; both nlerp `min(feather·time,1)` when `autoLeveling`. |
| `alflycamclassic.cpp:163-172, 278-287` | Zoom. |
| `alflycamclassic.cpp:312-316` | `levelHorizon` zeroes `sDelta[3]` and re-bases `sLastDelta[3]`. **This is the only mutation of `sDelta` in `finishIntegration`.** |

**F1.**
- BD's `time ≥ 0.016` clamp makes Classic's per-second speed rise with fps: at 144 fps it is about 2.3× the speed
  at 60 fps.
- Both feathers are frame-stepped.

Weighted inherits both (U1).

### 2.2 Ownership — `indra/newview/alcameradispatch.h`

| Ref | Fact |
|---|---|
| `:27-43` | `ALFlycamPoseOwner {CLASSIC, DRONE, ORBIT}`; six edges. |
| `:45-55` | `ALFlycamFrameDecision`. |
| `:60-124` | `alResolveFlycamFrame` (model 2 resolves to CLASSIC today, `:66`). |
| `:158-175` | `alClassifyFlycamDispatchGap`. |
| `:212-225` | `alCameraAngularVelocity` (body-frame left factor). |

### 2.3 `moveFlycam` — `indra/newview/llviewerjoystick.cpp`

| Ref | Fact |
|---|---|
| `:78-79` | Static members `LLViewerJoystick::sLastDelta`, `sDelta` (`std::array<F32,7>`). |
| `:91-97` | `FlycamPoseSample {positionGlobal, rotation, dt, valid}` (anonymous namespace). |
| `:99-147` | `FlycamDroneRuntime`: `owner` `:123`, `previousPose`/`currentPose` `:124-125`, `resetPending` `:128`, `lastWrittenSerial` `:129`. |
| `:182-186` | `flycamFlightModel()`. |
| `:2115-2117` | **Function-local statics** `sFlycamRotation`, `sFlycamPosition`, `sFlycamZoom`. Only `moveFlycam` can write them. |
| `:2141-2174` | Reset branch: re-reads the camera, `resetDeltas`; `sDrone.owner = drone_selected ? DRONE : CLASSIC` (`:2164`). Callers: toggle `:2932`; build mode `lltoolmgr.cpp:360,:391` and `llviewermenu.cpp:3443,:3461`; app activate/deactivate `llviewerwindow.cpp:2046`; startup `llappviewer.cpp:1346`. |
| `:2210-2221` | `tagged_dispatch`, `dispatch_foreign`, the decision, `desired_owner`, `frame_owner`, `owner_edge`, `drone_frame`, `orbit_frame`. |
| `:2234-2239` | `resetPending` re-base of `sDrone.lastWrittenSerial` and the foreign flag. |
| `:2241-2385` | Drone block, entered only if `drone_frame` or `desired_owner == DRONE`. |
| `:2387-2418` | `classic_input`. |
| `:2420-2421` | `shadow_entry_position`/`shadow_entry_rotation`. |
| `:2422-2440` | Classic shadow (`classic_intermediate` `:2430`). |
| `:2444-2448` | `discardClassicShadowPose`. |
| **`:2450-2452`** | **The Drone overwrite comment and `if (drone_frame)`.** The Weighted call site is inserted immediately before `:2452`. |
| `:2616-2620` | `else if (level_horizon)` consumes Level on all non-Drone frames, so it covers Weighted unchanged. |
| `:2638-2747` | Orbit. |
| `:2750-2755` | Pose history, recorded for every owner. |
| `:2757-2798` | Edge block: C→D seed (`edge == CLASSIC_TO_DRONE`, `:2760-2784`); D→C `classicSeed` (`edge == DRONE_TO_CLASSIC`, `:2785-2791`); `sDrone.owner = desired_owner` (`:2792`). |
| `:2800-2807, :2895-2897` | Operator resets, keyed on `resetOperator` and the Drone↔Orbit edges. |
| `:2811-2814` | `FlycamOperatorEnabled`, `CameraShakeApplyActive`. |
| `:2850-2860` | Classic operator feed from `sDelta`. |
| `:2880-2886` | With both off, the camera is written exactly as `sFlycamPosition` and the `sFlycamRotation` axes. |
| `:2888-2893` | Serial bookkeeping. |
| `:3516-3527` | D-pad FLIGHT_MODEL toggle. |
| `:4064-4077` | `resetDroneSettings` (`:4066` sets the model to 0). |

Same-region teleport (R2-2): `process_teleport_local` (`llviewermessage.cpp:6625-6691`).
- `slamLookAt` only resets the agent axes (`llagentcamera.cpp:423-429`).
- Without KeepsLookAt, `resetView(true,true)` turns the flycam off (`llagentcamera.cpp:341-348`, through
  `handle_toggle_flycam`, `llviewermenu.cpp:3364-3367`). Re-entry is a reset.
- With KeepsLookAt, `updateCamera` → `updateCameraLocation` (`llagentcamera.cpp:1659`) may write `LLViewerCamera`
  (gated by `getCameraNeedsUpdate`, `llviewercamera.cpp:90+`). The flycam statics are untouched and the next
  dispatch rewrites the camera.

Region change: `LLAgent::setRegion` (Drone brief §11.9, `doc/FLYCAM_DRONE_CONTROL_BRIEF.md:1002-1017`). Classic
does not re-express its pose (F25).

### 2.4 UI and tests

| Ref | Fact |
|---|---|
| `floater_joystick.xml:628-678` | Xbox Pad tab: D-pad combos `:641-670` (value-4 labels at `:647/:657/:667`); D-pad Down `:671-672` (top 290); Rumble `:673-675` (tops 330 and 355). |
| `floater_joystick.xml:679-750` | Drone tab (`joystick_drone_tab`): model combo `:682-686`; cursor reason `:687-689`; `drone_settings_panel` `:690` (`top="40"`). |
| `llfloaterjoystick.cpp:113-148` | Empty `draw()`; 10 Hz `tick()`. |
| `llfloaterjoystick.cpp:273-285` | `refreshDroneEnabled()`. |
| `llfloaterjoystick.cpp:355-356` | `installForTabs`. |
| `llfloaterjoystick.cpp:385-395, :396-408` | Snapshots. |
| `llfloaterjoystick.cpp:566-574` | `cancel()`. |
| `panel_flycam_recorder.xml:12-43` | Director/Recorder shared row: model 84 px, Drone preset 112, Release 92, in 320 px. |
| `alpanelflycamrecorder.cpp:62-175`; `.h:55-65` | Panel code. |
| `app_settings/settings.xml:13124-13130` | `FlycamFlightModel`. |
| `xui_layout_check.py` | `--selftest` `:929`; `--changed` `:977`; hidden widgets excluded from overlap `:543-544`; alias `->setVisible(` dynamic `:687-703`; `installForTabs` runtime fit `:708-709`. |
| `CMakeLists.txt:192, :1066-1067, :2725-2727` | Sources, headers, tests. |
| `tests/llflycamdrone_test.cpp` | 49 tests (T26-T29 decision rows at `:623-676`). |
| `tests/alflycamclassic_test.cpp` | Classic golden tests (4/4). |

---

## 3. Architecture — additive, one call site (R3-2)

```
moveFlycam()                                             EXISTING CODE — UNCHANGED except where marked
  :2141 reset branch                                     unchanged (model 2 → owner CLASSIC → next frame C→W)
  :2214 alResolveFlycamFrame(EFFECTIVE model, ...)       ◄ CHANGED ARGUMENT ONLY (§6.1)
  :2241 Drone block                                      unchanged
  :2387-2448 Classic shadow + discard                    unchanged
  ► NEW: Weighted call site (≤ 25 lines, §5.6)           ◄ only if frame_owner==WEIGHTED || weightedEdge!=NONE
  :2452 if (drone_frame) ... else if (level_horizon)     unchanged
  :2638 Orbit ; :2750 history ; :2757 edge block         unchanged (sees legacy-compatible edges, §6)
  :2809-2897 FOV, operator, camera write, serials        unchanged
```

**Shadow-loop soundness.**
1. `integrateDeltas` never reads pose (`alflycamclassic.cpp:56-124, 192-243`). The `sDelta`/`sLastDelta` series
   in Weighted is therefore identical to Classic's for the same inputs.
2. The target's translation uses the **presented** `q0` (`shadow_entry_rotation`).
3. The target contains no leveling, origin shift, teleport offset or foreign write.

**Why the existing tail is safe for Weighted frames.**
- For Weighted frames, the tail (`:2452-2897`) does exactly what it does for Classic frames: `frame_owner` is
  neither DRONE nor ORBIT, so `drone_frame` and `orbit_frame` are false.
- Edges reach the edge block as Classic-family edges (§6).

---

## 4. The mass layer — normative definition

Pure module `indra/newview/alflycamweighted.{h,cpp}` (`ALFlycamWeighted`). It reads no settings and no globals.
Arithmetic is F64; poses are narrowed to F32 at the boundary.

### 4.1 Target from Classic

Inputs:
- `d[0..5]`: `sDelta[0..5]` **as read at the call site**, after `finishIntegration`. The only mutation there is
  Level's `sDelta[3] = 0` (`alflycamclassic.cpp:312-316`). On Level frames the roll target is therefore 0, which
  agrees with the same-frame `zeroRoll` (§4.3).
- `q0`: `shadow_entry_rotation`.
- `h`: the step (§4.4).

```
v*  = LLVector3d( LLVector3(d0,d1,d2) * q0 ) / h
L   = LLQuaternion( LLMatrix3(d3,d4,d5) ) ; if L.w < 0: L = -L
θL  = 2·atan2(|L.xyz|, L.w) · L.xyz/|L.xyz|   (0 if |L.xyz| < 1e-12)
ω*  = θL / h                                             // body frame
inputZero = classic_intermediate.isZero
```

### 4.2 Filter: critically damped cascade, exact discretisation

There is one filter per group: translation (`τ_t`, world `LLVector3d`; momentum keeps its old direction, U7) and
rotation (`τ_r`, body `LLVector3d`).

```
continuous:  u' = (r − u)/τ ,  v' = (u − v)/τ          (r held constant over the step: ZOH)
step(h):     x = h/τ ; a = e^{−x} ; b = x·a ; eu = u − r ; ev = v − r
             u ← r + a·eu ;  v ← r + a·ev + b·eu
             Δ = r·h + τ·((1−a)·ev + (1−a−b)·eu)          // exact ∫v dt
```

Numerics, in F64:
- `1−a = −expm1(−x)`;
- `1−a−b = −expm1(−x) − x·a`, using the series `x²/2 − x³/3 + x⁴/8` when `x < 1e-3`.

Guarantees:
- **G1 Filter exactness.** For a target held constant over a step, the update is the exact flow of the LTI ODE.
  - The semigroup `step(h1)∘step(h2) ≡ step(h1+h2)` holds **only while the target is unchanged**.
  - A piecewise-constant target signal with changes on common frame boundaries gives the same output at any fps.

  This is the **filter only**. The complete model inherits F1, because the target signal itself depends on fps.
  W4 proves the filter; W27 shows that the complete model's fps behaviour equals Classic's.
- **G2 Stability.** For all `h, τ > 0`, `a, b ∈ [0, 1)`.
- **G3 No overshoot.**
  - Step response `v = r(1 − (1+t/τ)e^{−t/τ})`.
  - Release `v = r(1+t/τ)e^{−t/τ}`, with no sign change.
  - Each body-rate component changes sign at most once during a coast.
  - Remaining coast is exactly `τ(u+v)`.
- **G4 Unit DC gain.** Cruise speed is exactly Classic's.
- **G5 Homogeneity.** The feel is the same at any throttle or axis scale.

### 4.3 Rotation and leveling (pose feedback; bounds from R2-3)

```
R = quat(Δθ/|Δθ|, |Δθ|) ; q = R * q0          // same side as Classic's LLQuaternion(rot_mat)*q (W0)
q = classicLevel(q)                            // §7.1: Classic's own finishIntegration on a scratch state
```

- **Integration.** No Euler angles are integrated, and cumulative 180° and 360° turns are sums of small
  increments. Single-axis rotation is exact; the multi-axis composition error is about `½|Δθ₁×Δθ₂|` (W14).
- **(i) Contraction.** For `0 < k ≤ 1` and `|θ| < π`, `nlerp(k, q, q_level)` gives
  `|e'| = atan((1−k)sin|θ| / ((1−k)cos|θ| + k)) ≤ |θ|`, with `ρ = |e'|/|θ| < 1` and `ρ → 1−k` as `θ → 0`. So
  `|e_n| ≤ ρ_max^n|e_0| + Σρ_max^{n−j}|ι_j|`, and `e → 0` because `ι` decays (G3).
- **(ii) Zero crossings ≤ 1 + the number of sign changes of the injected roll `ι`.** A fixed-sign `ι` can still
  cause one crossing when `e_0` has the opposite sign. Within one release coast this means at most 2 crossings for
  a single source.
- **(iii) Yaw–pitch coupling.**
  - At pitch `φ`, a body yaw `δψ` rotates the left axis about the local up: `y' = y cos δψ − x sin δψ`.
  - Per-frame roll injection is `ι_yaw = asin(sin δψ·sin φ)`, with `|ι_yaw| ≤ |δψ||sin φ|`.
  - Total over a coast: `≤ |sin φ|_max·2τ_r|ω_z0|`.
  - Classic has the same coupling for the same total yaw. Pitch drift is `O(δψ²)`.
  - Classic leveling is degenerate near 90° (`alflycamclassic.cpp:152-153, :268-269`), which is pre-existing; W31
    stops at 85°.
- **Instant Level and BD roll-default.** `classicLevel` snaps on the same frame, then `zeroRoll()` runs on that
  same frame (W29, W30).

### 4.4 Step size and degenerate frames

```
h_raw = in.frameIntervalSeconds
if !finite(h_raw) or h_raw < 1e-4: DEGENERATE → no mass step; pose = entry pose, then classicLevel; state kept
h = min(h_raw, 0.25)   // the same h forms v* and integrates (Drone uses the same cap, :2496)
```

### 4.5 Coast settle-to-exact-stop

```
ε_v = 1e-3 m/s, ε_ω = 1e-3 rad/s, ε_d = 2e-4 m, ε_θ = 1e-4 rad
targetRest = inputZero && |v*| < ε_v && |ω*| < ε_ω
after step: targetRest && τ_t|u_t+v_t| < ε_d && τ_r|u_r+v_r| < ε_θ → zero state, settled
settled: wake = !inputZero || |v*| ≥ 2ε_v || |ω*| ≥ 2ε_ω ; !wake → hold entry pose (+ classicLevel)
```

The snap drops at most 0.2 mm and 0.0057°. After it, the pose is bitwise constant.

### 4.6 Filter API

```cpp
namespace ALFlycamWeighted {
struct Tuning { F64 tauT = 0.0; F64 tauR = 0.0; };          // τ ≤ 0 ⇒ exact pass-through
struct Target { LLVector3d v, w; bool inputZero = true; };
struct State  { LLVector3d uT, vT, uR, vR; bool active = false; bool settled = true; };
struct Step   { LLVector3d dp, dtheta; bool settledNow=false, woke=false, held=false, nonFinite=false; };
Tuning tuningForWeight(F32 weightPercent, F32 tauMax, F32 curveK, F32 rotationShare);
Target targetFromClassic(const F32 d[6], const LLQuaternion& q0, F64 h, bool inputZero);
bool   isExtremeInput(const Target&, F64 h);                // |v*| > 2000 m/s or |θL| > π/2
Step   advance(State&, const Target&, const Tuning&, F64 h);
void   seedAtTarget(State&, const Target&); void seedVelocity(State&, const LLVector3d&, const LLVector3d&);
void   rest(State&); void deactivate(State&); void zeroRoll(State&);
LLQuaternion rotationFromVector(const LLVector3d&);
F32 presetWeight(S32); S32 presetForWeight(F32); const char* presetName(S32);
}
```

---

## 5. Weight, presets, Coordinator, glue

### 5.1 Weight → time constants

```
w = clamp(Weight,0,100)/100 (NaN → 0) ; tau_t = TauMax·(K^w − 1)/(K − 1) (K ≤ 1.0001 → TauMax·w) ; tau_r = Share·tau_t
defaults: TauMax 1.0 s, K 20, Share 0.6
```

| Weight | τ_t | τ_r | Ease-in 90 % (3.89τ) | Stop 2 % (5.83τ) | Coast per m/s (2τ) | Pan stop |
|---|---|---|---|---|---|---|
| 10 % | 0.018 | 0.011 | 0.07 s | 0.11 s | 0.04 m | 0.06 s |
| **25 % Light** | 0.059 | 0.035 | 0.23 s | 0.34 s | 0.12 m | 0.21 s |
| **50 % Steadicam** | 0.183 | 0.110 | 0.71 s | 1.07 s | 0.37 m | 0.64 s |
| **70 % Heavy** | 0.376 | 0.226 | 1.46 s | 2.19 s | 0.75 m | 1.32 s |
| **90 % Crane** | 0.728 | 0.437 | 2.83 s | 4.24 s | 1.46 m | 2.55 s |
| 100 % | 1.000 | 0.600 | 3.89 s | 5.83 s | 2.00 m | 3.50 s |

### 5.2 Presets

| Id | Name | Weight |
|---|---|---|
| 0 | Light | 25 |
| 1 | **Steadicam** (default) | 50 |
| 2 | Heavy | 70 |
| 3 | Crane | 90 |
| 4 | Custom | slider; shown when no preset matches within ±0.05 |

### 5.5 (removed in R3)

The `ALFlycamFrame::run`/`Host` orchestrator (R2-4) is withdrawn; see R3-1.

### 5.6 Coordinator — all Weighted decisions, explicit I/O (R3-3)

```cpp
namespace ALFlycamWeighted {
struct PoseSample { LLVector3d positionGlobal; LLQuaternion rotation; F64 dt = 0.0; bool valid = false; };
struct Settings   { F32 weight = 50.f, tauMax = 1.f, curve = 20.f, rotationShare = .6f; bool guardArmed = false; };

struct FrameIn {
    // ownership (from alResolveFlycamFrame, :2214)
    ALFlycamPoseOwner frameOwner;              // frame_owner
    ALFlycamWeightedEdge weightedEdge;         // frame_decision.weightedEdge
    bool foreignDroneRegain;                   // frame_decision.foreignDroneRegain
    // dispatch (all read before :2888 updates them)
    bool tagged;                               // tagged_dispatch
    U64  dispatchSerial;                       // gCameraDispatchSerial
    U64  lastWrittenSerial;                    // sDrone.lastWrittenSerial
    bool foreignSinceWrite;                    // gCameraDispatchForeignSinceFlycamWrite
    LLVector3d originGlobal;                   // gAgent.getPosGlobalFromAgent(LLVector3::zero)
    // poses (agent space)
    LLVector3 entryPos;  LLQuaternion entryRot;     // shadow_entry_position / _rotation (= last presented)
    LLVector3 classicPos; LLQuaternion classicRot;  // sFlycamPosition / sFlycamRotation after Classic
    F32 zoom;                                       // sFlycamZoom (scratch leveling only; never output)
    std::array<F32,7> delta, lastDelta;             // sDelta, sLastDelta after finishIntegration
    ALFlycamClassic::Input classicIn;               // classic_input
    ALFlycamClassic::Intermediate classicIm;        // classic_intermediate
    F64 frameIntervalSeconds;                       // gFrameIntervalSeconds.value()
    PoseSample prevPose, curPose;                   // sDrone.previousPose / currentPose (D→W seed)
    LLVector3 presentedPos; LLQuaternion presentedRot;  // LLViewerCamera origin / quaternion (pre-write)
    bool presentedComparable;                       // !FlycamOperatorEnabled && !CameraShakeApplyActive
    Settings settings;
};
struct FrameOut {
    bool writePose = false; LLVector3 pos; LLQuaternion rot;   // assign to sFlycamPosition/Rotation
    std::array<F32,6> operatorDelta{};                         // telemetry only (OPEN-11)
    std::string log;                                           // rate-limited verdict / one-shot lines
};
class Coordinator { public: FrameOut frame(const FrameIn&); /* state, tuning cache, last write, report */ };
}
```

**`Coordinator::frame` logic (normative):**
1. **Weighted edge actions** (`weightedEdge`):
   - `C2W` / `O2W`: seed AT_TARGET this frame and clear the last-write memory.
   - `D2W` (the frame owner is DRONE): set `pendingSeed = (tagged && foreignSinceWrite) ? REST : FROM_HISTORY`;
     write nothing; return.
   - `W2C` / `W2O`, and `W2D` with `foreignDroneRegain`: deactivate; write nothing; return. A W→C frame is
     therefore pure Classic (R1-4).
   - `W2D` without foreign: run this frame as a normal Weighted frame (outgoing), then deactivate after writing.
     The existing `:2760-2784` seed then sees the weighted pose history.
2. **Pending seed** (first W frame after D2W):
   - FROM_HISTORY: `v = (cur.posGlobal − prev.posGlobal)/cur.dt`, `ω = alCameraAngularVelocity(prev.rot,
     cur.rot, cur.dt)`. Invalid samples, `dt ∉ (0.0005, 0.25]`, or extreme input give REST.
   - REST: rest.
3. **Inactive state on a W frame** (defensive): seed AT_TARGET.
4. **Proven-route rests** (only when `weightedEdge == NONE`):
   - origin tag change → rest (`reseeds:origin`);
   - when tagged, `alClassifyFlycamDispatchGap(dispatchSerial, lastWrittenSerial, foreignSinceWrite)`: FOREIGN or
     SELF → rest;
   - internal check: `entryPos/entryRot` against the Coordinator's last written pose, beyond 1e-4 m / 1e-5 rad →
     rest (`posecut:internal`).
5. **Telemetry pose check** (§7.4): only when `presentedComparable` held on the previous and the current frame;
   `presentedPos/Rot` against the last write. Counted; rests only if armed and not storm-disarmed.
6. Target, extreme-input check, advance, settle/hold, `q = R*q0`, `classicLevel(q)`, then `zeroRoll` on Level or
   BD roll-default frames. Output pose; remember it as the last write.
7. Report accumulation. Every 5 s of Weighted time it produces `out.log` (the §12 verdict). A one-shot
   `WEIGHTED INACTIVE 3d-cursor` is produced on `W2C` when `classicIn.cursor3D`.

**Focus loss and alt-tab (R1-9).**
- `llviewerwindow.cpp:2046` → the reset branch → `sDrone.owner = CLASSIC` (`:2164`). The next frame is a C2W
  edge, which seeds AT_TARGET; `sDelta` was zeroed by `resetDeltas`, so this is a rest.
- `FlycamDroneResumeMomentum` is never read by the Coordinator.

**The glue (the only new code in `moveFlycam`):** inserted between `:2448` and `:2450`. It reads and writes only
the named identifiers.

```cpp
    // [Weighted Classic] Additive call site. Entered only on WEIGHTED-owned frames or WEIGHTED edges;
    // never for FlycamFlightModel 0/1 unless leaving WEIGHTED (one W->* edge frame).
    if (frame_owner == ALFlycamPoseOwner::WEIGHTED ||
        frame_decision.weightedEdge != ALFlycamWeightedEdge::NONE)
    {
        static ALFlycamWeighted::Coordinator sWeighted;
        ALFlycamWeighted::FrameIn win;
        win.frameOwner = frame_owner;  win.weightedEdge = frame_decision.weightedEdge;
        win.foreignDroneRegain = frame_decision.foreignDroneRegain;
        win.tagged = tagged_dispatch;  win.dispatchSerial = gCameraDispatchSerial;
        win.lastWrittenSerial = sDrone.lastWrittenSerial;
        win.foreignSinceWrite = gCameraDispatchForeignSinceFlycamWrite;
        win.originGlobal = gAgent.getPosGlobalFromAgent(LLVector3::zero);
        win.entryPos = shadow_entry_position;  win.entryRot = shadow_entry_rotation;
        win.classicPos = sFlycamPosition;  win.classicRot = sFlycamRotation;  win.zoom = sFlycamZoom;
        win.delta = sDelta;  win.lastDelta = sLastDelta;
        win.classicIn = classic_input;  win.classicIm = classic_intermediate;
        win.frameIntervalSeconds = gFrameIntervalSeconds.value();
        win.prevPose = toWeightedSample(sDrone.previousPose);  win.curPose = toWeightedSample(sDrone.currentPose);
        win.presentedPos = LLViewerCamera::getInstance()->getOrigin();
        win.presentedRot = LLViewerCamera::getInstance()->getQuaternion();
        win.presentedComparable = !weightedSettings().operatorEnabled && !weightedSettings().shakeApplyActive;
        win.settings = weightedSettings().snapshot();
        const ALFlycamWeighted::FrameOut wout = sWeighted.frame(win);
        if (wout.writePose) { sFlycamPosition = wout.pos;  sFlycamRotation = wout.rot; }
        if (!wout.log.empty()) LL_INFOS("FlycamWeighted") << wout.log << LL_ENDL;
    }
```

The glue writes only `sFlycamPosition` and `sFlycamRotation`, and only on Weighted frames. It never writes
`sDelta`, `sLastDelta`, `sFlycamZoom`, `sDrone`, the operator, the camera, the serials or any `mXI*` flag.

Two support helpers are new, in the anonymous namespace near `:182-199`:
- `toWeightedSample(const FlycamPoseSample&)`, a four-field copy;
- `weightedSettings()`, a struct of `LLCachedControl`s: `FlycamWeighted*`, `FlycamOperatorEnabled`,
  `CameraShakeApplyActive`, plus `snapshot()`.

The only other edit inside `moveFlycam` is the decision **argument** at `:2215` (§6.1).

---

## 6. Ownership — `alResolveFlycamFrame` (legacy-compatible extension, R3-4)

### 6.1 Effective model (exact bypass)

```cpp
inline S32 alEffectiveFlycamModel(S32 model, F32 weightPercent)
{ if (model == 1) return 1; if (model == 2 && std::isfinite(weightPercent) && weightPercent > 0.f) return 2; return 0; }
```

`:2215` passes `alEffectiveFlycamModel(flycamFlightModel(), weightedSettings().weight)` in place of
`(S32)flycamFlightModel()`. For models 0 and 1 the value is identical (W2). The reset branch (`:2154/:2164`) is
**unchanged**: with model 2 it sets owner CLASSIC, and the next frame is a C2W edge.

### 6.2 Enum and field additions (append only)

```cpp
enum class ALFlycamPoseOwner { CLASSIC, DRONE, ORBIT, WEIGHTED };
enum class ALFlycamOwnerEdge { /* existing six */, CLASSIC_TO_WEIGHTED, WEIGHTED_TO_CLASSIC };
enum class ALFlycamWeightedEdge { NONE, C2W, W2C, W2D, D2W, W2O, O2W };
// ALFlycamFrameDecision += ALFlycamWeightedEdge weightedEdge = ALFlycamWeightedEdge::NONE;
```

### 6.3 Algorithm

1. Compute the **legacy result** with today's unchanged algorithm on `legacyModel = (model == 2 ? 0 : model)` and
   `legacyPrev = (previousOwner == WEIGHTED ? CLASSIC : previousOwner)`.
2. Overlay Weighted, where `weightedSelected = model == 2 && !cursor3D && !orbitResolved`.

| previous → selected | Legacy `edge` (seen by existing code) | `frameOwner` | `weightedEdge` | What existing code does (unchanged) |
|---|---|---|---|---|
| C → W | `CLASSIC_TO_WEIGHTED` (new value) | W | C2W | `:2757` block: owner := WEIGHTED; nothing else fires. |
| W → W | NONE | W | NONE | — |
| W → C | `WEIGHTED_TO_CLASSIC` (new value) | C | W2C | Owner := CLASSIC. The frame is pure Classic. |
| W → D | `CLASSIC_TO_DRONE` | W, or D if `foreignDroneRegain` | W2D | `:2760-2784` seeds Drone from the history of weighted poses (same as C→D); resets the operator. |
| D → W | `DRONE_TO_CLASSIC` | D | D2W | `:2785-2791` `classicSeed` into `sDelta` (Classic's target carries the Drone momentum); resets the operator. |
| W → O | `CLASSIC_TO_ORBIT` | O | W2O | As C→O: Orbit engages from the shadow pose, which started at the last presented weighted pose. |
| O → W | `ORBIT_TO_CLASSIC` | W | O2W | As O→C. |

- All other legacy fields (`resetOperator`, `discardDroneAccumulator`, `discardClassicShadowPose`, `seedDrone`,
  `foreignDroneRegain`) are the legacy result's.
- `selectedOwner` is WEIGHTED when Weighted is selected. Existing code compares `desired_owner` only with DRONE,
  and `:2792` stores it, so the next frame's `previousOwner` is WEIGHTED.
- `frame_owner == WEIGHTED` is neither DRONE nor ORBIT, so `drone_frame` and `orbit_frame` stay false.
- The two new legacy edge values make `owner_edge` true. The only effect is the owner bookkeeping at `:2792`
  (proof: `:2760` needs `CLASSIC_TO_DRONE`, `:2785` needs `DRONE_TO_CLASSIC`, and `:2800-2807` needs the Drone↔Orbit
  edges).

**Gate property (R3-8):** for `model ∈ {0,1}` and `previousOwner ∈ {C,D,O}`, the result equals the frozen
pre-edit function field for field, `weightedEdge == NONE` and `frameOwner ≠ WEIGHTED`. The glue is therefore
never entered.

---

## 7. Discontinuities — must NOT become a long glide

| Event | Route | Weighted |
|---|---|---|
| Flycam toggle, build mode, startup, **focus loss / alt-tab** | Reset branch → owner CLASSIC → C2W | Rest via the AT_TARGET seed with zeroed `sDelta`. |
| FOREIGN regain (PILOT/RECORDER/PATH/CINE) | Gap classifier inside the Coordinator | Rest. Classic's stale-pose behaviour is unchanged. |
| SELF gap | Same | Rest. |
| Region change | Origin tag | Rest. The pose is not re-expressed (F25, OPEN-4). |
| Same-region teleport without KeepsLookAt | `resetView` turns the flycam off (§2.3) | The reset path rests. |
| Same-region teleport with KeepsLookAt | The flycam statics are untouched (§2.3) | Momentum continues (parity). If comparable, the transient camera write is logged as `posecut:presented` (telemetry). |
| Shadow-pose rewrite inside `moveFlycam` | Internal check | Rest. |
| Level horizon, BD roll-default | `classicIn` flags | Same-frame snap plus `zeroRoll`. |
| Auto-level | `classicLevel` | Bounded (§4.3). |
| Extreme or non-finite input | `isExtremeInput`, `nonFinite` | `writePose = false`, so Classic's shadow pose stands; rest. |
| ZOOM_DEFAULT | — | Unweighted. |

### 7.1 `classicLevel` — Classic's own code, no Classic change (R3-5)

```
scratch: delta' = {0,0,0,0,0,0, in.delta[6]}, lastDelta' = in.lastDelta, pos' = 0, rot' = q, zoom' = in.zoom
ALFlycamClassic::finishIntegration(in.classicIn, in.classicIm, State{lastDelta', delta', pos', rot', zoom'})
q = rot'          // scratch delta/lastDelta/pos/zoom discarded
```

- **Bit-exact identity motion.** `LLMatrix3(0,0,0)` is the exact identity (cos 0 = 1, sin 0 = 0), so
  `LLQuaternion(identity) = (0,0,0,1)` exactly, and `identity * q` reproduces `q` exactly. Each component is a
  sum of products with exact 0 and 1. `rot'` is therefore Classic's leveling applied to `q` and nothing else
  (W26).
- The BD/non-BD branch, `feather`, `time`, `levelHorizon`, `autoLeveling` and `rollDefault` all come from
  `classicIn`/`classicIm`, so the leveling is exactly Classic's for this frame.

### 7.4 Presented-pose telemetry (R2-1, R3-6)

- The comparison is against the Coordinator's **own last written pose**, and only when the operator and the shake
  were off on both frames. Then the camera write at `:2880-2886` equals `sFlycamPosition`/`sFlycamRotation`
  exactly. Otherwise the check reports `n/a(operator)`.
- **Telemetry only by default:** it is counted as `posecut:presented`, with `posecut_last` details.
- A hidden `FlycamWeightedPoseGuardArmed` lets it rest. POSECUT-STORM (more than 2 per second over 5 s) gives an
  INCONCLUSIVE verdict and auto-disarms for the session.
- Known producer: the KeepsLookAt same-region teleport.

---

## 8. Settings (`app_settings/settings.xml`)

| Name | Type | Default | Range | UI |
|---|---|---|---|---|
| `FlycamFlightModel` (`:13124-13130`) | S32 | 0 | {0,1,2}, other → 0 | Comment "0 Classic, 1 Drone, 2 Weighted Classic." |
| `FlycamWeightedWeight` | F32 | 50 | [0,100] | slider |
| `FlycamWeightedPreset` | S32 | 1 | 0..4 | combo |
| `FlycamWeightedRotationShare` | F32 | 0.6 | [0,2] | debug |
| `FlycamWeightedTauMax` | F32 | 1.0 | [0.1,3] | debug |
| `FlycamWeightedCurve` | F32 | 20 | [1,1000] | debug |
| `FlycamWeightedPoseGuardArmed` | Boolean | 0 | — | debug |
| `FlycamDpadModelA` | S32 | 0 | {0,1,2}, other → 0 | Xbox Pad combo |
| `FlycamDpadModelB` | S32 | 2 | {0,1,2}, other → 2 | Xbox Pad combo |

**Preset sync and pair validation** use `LLViewerJoystick` commit signals, added beside `:959-963`. These are new
connections; no existing connection changes.
- **Preset ↔ weight:** a preset of 0..3 sets the weight; a weight commit sets `presetForWeight` (Custom = 4).
- **Pair:** if A == B after a commit, the other side becomes `partner(changed)` (0→2, 2→0, 1→0). This is a pure
  helper in `alxinputmath.h`.

New API: `applyWeightedPreset(S32)`, `resetWeightedSettings()` (preset 1 and weight 50, share 0.6, TauMax 1.0,
Curve 20; the model is never touched), `getWeightedTuning()`.

**User decision c:** delete `llviewerjoystick.cpp:4066` (`setS32("FlycamFlightModel", 0)` in
`resetDroneSettings`).

---

## 9. Interactions

### 9.1 Triggers

- THROTTLE and ROLL act through Classic (`alflycamclassic.cpp:98-102/215-219`). G5 keeps the feel the same at any
  throttle.
- The D-pad Down hint is unchanged.

### 9.2 Zoom

Unweighted:
- Classic's feather is already cap-aware (`:44-51`);
- ZoomDirect is absolute;
- ZOOM_DEFAULT must snap.

`FrameOut` has no zoom channel.

### 9.3 Handheld operator (R3-6)

Unchanged. Weighted frames take the existing non-Drone path, `update(sDelta/dt)` (`:2850-2860`), so the operator
reacts to Classic's (stick) motion rather than to the coasting camera.
- `FrameOut.operatorDelta` reports the weighted motion as telemetry.
- **OPEN-11:** switch the feed to `operatorDelta` with a one-line conditional at `:2857-2858` if in-world shake
  looks wrong during coasts. That would be a separately reviewed edit.

### 9.4 D-pad pair

`:3516-3527` (the only D-pad edit) becomes:

```
ModelToggle r; next = ALXInput::toggleFlightModel(current, A, B, m3DCursor, r)
// normalise: invalid a→0, b→2; a==b → (0,2), result NORMALISED_PAIR (still switches)
// target = (current==a) ? b : a   (current outside the pair → a); cursor3D && target!=0 → BLOCKED_3D_CURSOR
```

| Situation | Hint |
|---|---|
| Classic | "Flight: Classic" |
| Weighted | "Flight: Weighted (Steadicam 50%)" (or "Custom 37%", or "0% = Classic") |
| Drone | "Flight: Drone" |
| NORMALISED_PAIR | "D-pad pair invalid: using Classic / Weighted" |
| BLOCKED_3D_CURSOR | "Weighted / Drone unavailable: 3D cursor" |

- Rumble is the existing `RUMBLE_MODE_TICK`, on a change.
- Release and Recentre are unchanged. Their existing "Drone not active" hint (`:3532-3536`, `:3546-3550`) is
  accurate in Weighted, where they are inert.

---

## 10. UI (CLAUDE.md rules 1-10; zero checker findings)

### 10.1 Xbox Pad tab

- The value-4 labels at `:647/:657/:667` become "Flight model: toggle pair".
- New row at top 312:

  | Name | Type | left, top, w, h | Notes |
  |---|---|---|---|
  | `xinput_label_Pair` | text, SansSerifSmall, right-aligned | 20, 312, 150, 18 | "Flight model pair" |
  | `FlycamDpadModelA` | combo | 180, 312, 170, 18 | Classic 0 / Weighted Classic 2 / Drone 1 |
  | `FlycamDpadModelB` | combo | 370, 312, 170, 18 | Same items |

  The tooltip says "A and B must differ; picking the same model moves the other side".
- `xbox_rumble_title` `:673` moves to top 350, and the rumble row `:674-675` to 375.
- The scroll-document height is not hand-edited (runtime-fitted).
- Cancel snapshot: both names go into `:385-395`.

### 10.2 Drone tab → "Flight model"

- Tab label "Flight model"; the name is unchanged.
- Model combo items: Classic 0, Weighted Classic 2, Drone 1.
- Cursor-reason text: "Weighted / Drone unavailable while 3D Cursor is on".
- New `weighted_settings_panel` (0, 40, 740 × 80, disabled by default):
  - `weighted_title`;
  - `weighted_preset_label`;
  - `FlycamWeightedPreset` combo (170, 28, 180 × 20);
  - `reset_weighted` button (370, 28, 140 × 20);
  - `FlycamWeightedWeight` slider (20, 55, 340 × 18, 0..100);
  - `weighted_readout` (385, 55, 340 × 18).
- New `drone_section_title` (20, 124).
- `drone_settings_panel` `top` changes from 40 to 146.

C++:
- `refreshDroneEnabled()` (`:273-285`) gains the Weighted panel enable and a key-guarded readout (10 Hz tick,
  never `draw()`);
- the snapshot list `:396-408` gains the five `FlycamWeighted*` names.

### 10.2b Director panel (`panel_flycam_recorder.xml`)

- Model combo items: Classic 0, Weighted 2, Drone 1.
- Hidden `FlycamWeightedPreset` (left 102, 112 px) and `FlycamWeightedWeight` spinner (left 220, 92 px), with
  allow-overlap pragmas.
- `alpanelflycamrecorder` swaps visibility with alias `->setVisible()`, in `postBuild` and the
  `FlycamFlightModel` signal (`scoped_connection`), never in `draw()`.

### 10.3 Gates, in order

1. `xui_layout_check.py --selftest`.
2. `--changed <last-built-commit>`: zero findings.
3. First open of the Joystick floater (all tabs) and the Director Takes tab with `UIFitDiagnostics`: no warning.

---

## 11. Tests

Registration: `LL_ADD_INTEGRATION_TEST(alflycamweighted "alflycamweighted.cpp;alflycamclassic.cpp" ...)`, near
`CMakeLists.txt:2726`.

### 11.1 Filter, in the new `tests/alflycamweighted_test.cpp`

| # | Test |
|---|---|
| W0 | LL multiplication convention; `rotationFromVector` equals `setAngleAxis`. |
| W1 | Mapping, table, clamps, presets. |
| W2 | `alEffectiveFlycamModel` (0 and 1 map to themselves). |
| W4 | Filter fps exactness for piecewise-constant targets, including jittered `dt`. |
| W5 | Semigroup under a constant target. |
| W6 | Step response, no overshoot. |
| W7 | Coast `2τr`, no reversal, bitwise hold after settle. |
| W8 | Settle time. |
| W9 | Ramp lag `2τ·slope`. |
| W10 | Homogeneity (throttle). |
| W11 | Isotropy. |
| W12 | Single-axis rotation exactness. |
| W13 | 180° and 360°; extreme per-frame rotation. |
| W14 | Multi-axis bound. |
| W15 | `targetFromClassic`. |
| W16 | τ → 0 and τ ≤ 0. |
| W17 | AT_TARGET seed exact. |
| W18 | History seed and invalid inputs. |
| W19 | Extreme and non-finite. |
| W20 | `zeroRoll`. |
| W21 | Crossing bound cases: same sign → 0; opposite sign with fixed-sign `ι` → 1; one sign change of `ι` → ≤ 2; contraction bound. |
| W22 | Sign parity through the real Classic integrator. |
| W23 | Settle hysteresis. |
| W24 | `dt` robustness. |
| W25 | Long-run precision. |
| **W26 (R3-5)** | `classicLevel(q)` (scratch `finishIntegration`, zero motion delta) is **bitwise equal** to the rotation produced by Classic's own `finishIntegration` on the real state with zero motion delta and the same `q`. Also: with leveling off, `classicLevel(q) == q` bitwise. Cases BD/non-BD × levelHorizon/autoLeveling/rollDefault. |
| W27 | Multi-frame real-Classic shadow: identical `sDelta` series; τ→0 equals Classic; cruise ratio 1 at 60 and 144 Hz. |
| W28 | Auto-level plus yaw coast while pitched; crossing and contraction bounds. |
| W29 | Instant Level while roll-coasting. |
| W30 | BD roll-default held. |
| W31 | Yaw–pitch coupling `asin(sin δψ·sin φ)` at φ = 0°, 30°, 60°, 85°; total ≤ `|sin φ|·|Δψ_coast|`; equals Classic's coupling for the same yaw. |

### 11.2 Decision table

| # | Test |
|---|---|
| D0 (R3-8) | **Gate property:** a frozen copy of today's `alResolveFlycamFrame` (pasted into the test as `legacyResolve`) is compared over every input with `model ∈ {0,1}` and `previousOwner ∈ {C,D,O}` (× cursor3D × orbit × tagged × foreign). Every field is equal, `weightedEdge == NONE` and `frameOwner ≠ WEIGHTED`, so the glue condition is false. |
| D1 | The §6.3 rows, exhaustively, including `previousOwner = WEIGHTED` with every selection. |
| D2 | For `previousOwner = WEIGHTED` and `model ∈ {0,1}`: exactly one W→* edge frame; afterwards the stored owner is not WEIGHTED, and the glue is never entered again. |
| D3 | 3D cursor with model 2 gives W2C. |
| D4 | D2W with tagged and foreign gives a pending REST (Coordinator). |
| D5 | Effective model `(2, 0)` gives the same result as model 0, including from `prev = WEIGHTED` (W→C, then pure Classic). |

### 11.3 XInput

| # | Test |
|---|---|
| X1 | `toggleFlightModel`: pairs; current outside the pair → A; A == B → (0,2) with NORMALISED_PAIR; clamps; cursor3D. |
| X2 | `DPAD_FLIGHT_MODEL == 4`. |
| X3 | `partner` and validation: setting either side equal moves the other; re-entry is a no-op; Cancel-style restores end valid. |

### 11.4 UI

§10.3.

### 11.5 Coordinator scenarios — `FrameIn` series replays (R3-8)

Each test builds a sequence of `FrameIn` values the way the glue would fill them. Where real Classic data is
needed, `delta`/`lastDelta`/`classicPos` come from running the real `ALFlycamClassic` integrator on scripted
input. The test then asserts the `FrameOut` series. There is no orchestrator.

| # | Test |
|---|---|
| I1 | Weight → 0 mid-coast: the W2C frame and later frames have `writePose == false`, so the pose equals Classic's. Combined with D5, this proves bit-exact Classic. |
| I2 | Gate (with D0): for models 0 and 1, no `FrameIn` is ever produced. The test asserts the glue predicate on the decision outputs over random frame sequences. |
| I3 | Shadow overwrite: `FrameOut.pos` becomes the next frame's `entryPos`; the internal check stays at 0. |
| I4 | Edge sequence C2W, W2D (outgoing write, then deactivate), D2W (pending), first W (history seed), W2O, O2W: seeds as specified; positions continuous. |
| I5 | Leveling: auto-level, Level, BD roll-default (via `classicIn` flags). |
| I6 | FOREIGN (`foreignSinceWrite = true`, serial jump): rest; no glide. |
| I7 | SELF gap (serial jump): rest. |
| I8 | Focus loss: the replay of reset gives a C2W `FrameIn` with zero `delta`, so rest, with `FlycamDroneResumeMomentum` irrelevant (never an input). |
| I9 | Origin change: rest. |
| I10 | Internal mismatch gives rest. Presented mismatch: disarmed gives a count and no state change (bitwise); armed rests; storm disarms; with `presentedComparable = false`, `n/a`. |
| I11 | 3D cursor gives W2C and the one-shot log. Orbit: W2O, then O2W. |
| I12 | Zoom: `FrameOut` has no zoom. `operatorDelta` equals the weighted local deltas (telemetry). |
| I13 | Same-region teleport: (a) the reset replay rests; (b) KeepsLookAt: a presented mismatch only, with momentum continuing bitwise. |

### 11.6 Regression gates

- `alflycamclassic_test` 4/4 and `llflycamdrone_test` 49/49, unchanged.
- D0 and I2, the gate property.

### 11.7 In-world (user; outcomes stated in advance)

1. **0 %:** identical to Classic (`WEIGHTED BYPASS`).
2. **Steadicam:** cruise equals Classic; visible stop at about 1.07 s; no creep (`TRACK` otherwise).
3. **Heavy:** turns swing wide.
4. **D-pad pair** toggles Classic ↔ Weighted.
5. **Cine hand-back:** no glide.
6. **Telemetry:** read `posecut` counts (they do not affect feel).
7. **Classic and Drone quick checks:** they behave exactly as in the previous build (`DRONE OK`).

---

## 12. Instrumentation — one rate-limited verdict line

The Coordinator emits one line every 5 s of Weighted time:

```
WEIGHTED <VERDICT> w=50% preset=Steadicam tau_t=0.183 tau_r=0.110 fps=143.8 vmax=4.21/4.30 wmax=1.10/1.22
  track_err=0.2% settles=3 coast_last=1.62s(8.9tau) reseeds=foreign:1 self:0 origin:0 c2w:2
  posecut=internal:0 presented:2|n/a guard=disarmed posecut_last=presented:d=0.412m,a=3.1deg,serial+1
  edges=C2W:1 W2C:0 D2W:0 W2D:0 W2O:0 O2W:0 extreme=0 degenerate=0
```

Verdicts, in precedence order:

| Verdict | Meaning |
|---|---|
| NONFINITE | |
| POSECUT-STORM | INCONCLUSIVE; auto-disarms. |
| EXTREME | INCONCLUSIVE. |
| DRIFT | FAIL. |
| TRACK | FAIL. |
| BYPASS | |
| OK | |

The only other line is the one-shot `WEIGHTED INACTIVE 3d-cursor`.

---

## 13. Work packages (Codex implements; fresh Codex review per WP; Opus adversarial review; ONE build)

### WP-W1 — pure core and tests

| | |
|---|---|
| **Files** | `alflycamweighted.{h,cpp}` (new: filter, Coordinator, `FrameIn`/`FrameOut`, presets, `classicLevel`)<br>`alcameradispatch.h` (enum and field additions; legacy-compatible overlay in `alResolveFlycamFrame`; `alEffectiveFlycamModel`)<br>`alxinputmath.h` (`toggleFlightModel`, `partner`)<br>`tests/alflycamweighted_test.cpp` (new: W0-W31, D0-D5, I1-I13)<br>`tests/alxinputmath_test.cpp` (+X1, X3)<br>`CMakeLists.txt` (source near `:192`, header near `:1066`, test near `:2726`) |
| **Off-limits** | `alflycamclassic.*` (**no change**, R3-5), `llflycamdrone.*`, `llviewerjoystick.*`, XUI, `settings.xml`, `tests/llflycamdrone_test.cpp`, `tests/alflycamclassic_test.cpp` |
| **Review focus** | D0: the legacy rows are byte-identical; the overlay touches only `selectedOwner`/`frameOwner`, the two new edge values and `weightedEdge`. W26 bit-exactness. The closed form. |

### WP-W2 — glue, settings, D-pad

| | |
|---|---|
| **Files** | `llviewerjoystick.cpp` and `.h` (if API declarations are needed); `app_settings/settings.xml` |
| **Edits, exhaustive** | (1) the `:2215` argument → `alEffectiveFlycamModel(...)`;<br>(2) the glue block between `:2448` and `:2450`, verbatim as §5.6;<br>(3) `toWeightedSample` and `weightedSettings()` in the anonymous namespace near `:182-199`;<br>(4) the D-pad FLIGHT_MODEL action `:3516-3527` → pair toggle;<br>(5) new commit-signal connections near `:959-963` (preset sync, pair validation);<br>(6) `applyWeightedPreset`, `resetWeightedSettings`, `getWeightedTuning`;<br>(7) delete `:4066`;<br>(8) the nine settings. |
| **Not edited** | Every other line of `moveFlycam`, including the reset branch, Drone block, Classic shadow, edge block, operator and camera tail. The reviewer checks this with `git diff -U0` on `moveFlycam`: only edits (1) and (2) may appear. |
| **Off-limits** | `alflycamclassic.*`, `llflycamdrone.*`, `alcameradispatch.h`, `alflycamweighted.*`, UI files |

### WP-W3 — UI

| | |
|---|---|
| **Files** | `floater_joystick.xml`; `llfloaterjoystick.{h,cpp}`; `panel_flycam_recorder.xml`; `alpanelflycamrecorder.{h,cpp}` |
| **Gates** | `--selftest`; `--changed` with zero findings; first-open `UIFitDiagnostics` (in the build report as a required user check); rules 1 and 4; Cancel restores. |
| **Off-limits** | All engine files. |

Then: a whole-feature Codex review plus an Opus adversarial review, a checkpoint commit and exe backup, and **one
build**. Run `alflycamweighted`, `alflycamclassic` (4/4), `llflycamdrone` (49/49) and `alxinputmath`.

---

## 14. Reviewer — attack these first

1. D0 and D5: is the legacy result truly unchanged for models 0 and 1 and for the bypass?
2. §6.3: do the two new edge values trigger anything in `:2757-2807, :2895` beyond `:2792`?
3. §5.6 glue:
   - does it read every `FrameIn` field **before** anything later in the frame changes it? (serials `:2888`, history
     `:2750`)
   - is the D2W pending seed read from the right history samples?
4. W26: is the identity rotation in the scratch `finishIntegration` truly bit-exact under the compiler's FP model
   (`/fp:` flags, FMA contraction)? If not provable, fall back to a verbatim **copy** of the leveling block in
   `alflycamweighted.cpp`, guarded by the W26 cross-check, rather than changing Classic.
5. §4.3 bounds; W31 coupling.
6. §7.4: is `presentedComparable` (operator and shake off) a sufficient condition for the camera to equal the
   statics at `:2880-2886`?

---

## 15. OPEN items

| # | Item | Recommendation |
|---|---|---|
| ~~OPEN-1~~ | World-frame momentum. | CLOSED (user b). |
| OPEN-2 | `RotationShare`, `TauMax`, `K`, preset weights. | In-world tuning; not a blocker. |
| OPEN-3 | Classic's own fps dependence (F1), inherited. | Leave it (U1). |
| OPEN-4 | Classic does not re-express its pose on an origin shift (F25). | Parity: rest only. |
| ~~OPEN-5~~ | D-pad A/B. | CLOSED (user a). |
| OPEN-6 | Zoom unweighted. | Keep. |
| OPEN-7 | Orbit controls are not weighted. | Out of scope. |
| ~~OPEN-8~~ | Reset Drone and the model. | CLOSED (user c). |
| OPEN-9 | Settle thresholds. | In-world; not a blocker. |
| OPEN-10 | Should the presented check ever be armed by default? | Probably never; decide from telemetry. |
| OPEN-11 (R3-6) | The operator feed in Weighted is Classic's `sDelta` (stick motion), not the coasting camera. | Keep it for zero tail changes. If shake looks wrong during coasts, make a one-line separately reviewed switch to `FrameOut.operatorDelta` at `:2857-2858`. |
| OPEN-12 (R3-5) | W26 depends on FP-model bit-exactness of the identity rotation. | If W26 fails on the build's FP flags, use a verbatim copy of the leveling block in `alflycamweighted.cpp`, cross-checked by tests. Classic stays untouched either way. |
