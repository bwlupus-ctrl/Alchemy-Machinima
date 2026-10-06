# Flycam Drone Control — weight & mass for the flycam (Xbox-first) — design paper (for adversarial review)

**Date:** 2026-10-06 (rev 10 — resolves Codex round 7 on rev 9; keeps the rev 4–9 resolutions and the user's decisions of 2026-10-06; supersedes rev 1–9)
**Status:** DESIGN ONLY. Nothing implemented, nothing built, nothing committed.
**Process (§19):** Opus designs (this paper) → **Codex implements every work package** (`--prompt-file`,
OFF-LIMITS list, watchdog) → **Codex adversarial review** in a separate session, looped to 0 must-fix →
Claude builds ONCE → user tests in-world. Fable is retired for this work.

> **BASE.** Every file:line below was re-verified on the LOCAL tree `I:/alchemy-machinima`,
> branch `fix/animesh-clone-pose-polish` @ `4bb6dc61eddc` (2026-10-06). Re-anchor again if the tree moves.

**Goal, in one line (user):** *"add a mode to flycam so it feels like it has mass."* Everything in this paper
serves that one mode. Anything that does not is listed as deferred (§2.2).

---

## User decisions (2026-10-06) — rev 4.1

These are the user's decisions on the six questions rev 4 raised. They are binding on every WP and applied everywhere they are relevant.

| # | Decision | Where applied |
|---|---|---|
| U1 | **D-pad:** Up = Classic↔Drone, Left = Hold↔Drift, Right = Recentre gimbal. Down stays the existing ROLL/THROTTLE toggle. Cue/level actions remain assignable through the same action enum. The Drone actions are **appended** to the enum as values 4/5/6. A migration defaults a slot to its Drone action **only if that slot is None (0)**; slots already holding a cue or level action are never touched. The migration is versioned in rev 5: `JoystickXInputDpadMigrationVersion`. | §12.1, §13, §16.3, §19 WP3/WP4 |
| U2 | **Presets:** keep all five: Cine, Normal, Sport, Dolly, FPV. Sport has **no bank** in v1 (bank is deferred). | §10 |
| U3 | **Camera hand-back:** after the Recorder, Path Camera or Director, Drone continues **from where that owner left the camera** (the rendered pose). | §11.2 |
| U4 | **Momentum after alt-tab or a hand-back: an option for both.** New setting `FlycamDroneResumeMomentum`: 0 Stop (default) / 1 Keep. Keep is defined precisely in §11.2.1: whose velocity is kept, in which frame, how it is validated, and why it cannot jump. Tests T30–T38 and T42–T44. Revised by rev 5 (three presented samples, owner tags, quaternion rates). | §11.2, §13, §14, §15, §16 |
| U5 | **Classic golden tests:** WP0 moves the Classic integrators **verbatim** into a testable unit. The move is **verified by counting** (§16.2.1), not by compiling. | §16.2, §19 |
| U6 | **Reviewer of record:** `CLAUDE.md` is **left alone**. Per the user's current directive, **Codex** does the adversarial review for this feature. | §19 |

---

## rev 5 changes (Codex R2 resolution)

Codex R2 verdict on rev 4.1: **NOT APPROVED**.

**Verified closed by R2:**
- unified trigger ownership;
- settings location;
- Cursor3D exclusion;
- single Orbit resolution;
- the five presets, with Sport unbanked;
- appended D-pad values;
- no double integration by the Classic shadow. This holds **provided** the post-shadow overwrite and the edge-frame ordering are implemented exactly as §4 and §11.4 state. Both are now WP3 review checklist items (§19).

The facts R2 cited were re-verified locally:
- `gFrameCount++` happens only in `display()` (`llviewerdisplay.cpp:604`).
- `display()` returns early when hidden or minimized (`llviewerdisplay.cpp:473-479`).
- Multi-pass snapshots call `display()` per tile (`llviewerwindow.cpp:5794`).
- Camera arbitration runs every `idle()` (`llappviewer.cpp:5594-5619`).

| # | Codex R2 finding | rev 5 fix | § |
|---|---|---|---|
| R2-P0-1 | Gap detection on `gFrameCount` is the wrong clock: minimized/hidden frames give false negatives, multi-pass snapshots give false gaps | New **idle camera-dispatch serial** `gCameraDispatchSerial`, plus a per-dispatch **owner tag** (`gCameraDispatchOwner`), both set once per `idle()` at the arbitration block. The Drone writer and the final-camera sampler share the serial. A SELF or FOREIGN gap is decided from the **owner tags** of the intervening dispatches, not by comparing poses. `gFrameCount` is used only to mark samples as *presented* (R2-P1-5). | §11.2 |
| R2-P0-2 | B2 is enforced before the drag coefficients exist | **[Superseded in rev 6: B2 and the fixed point are deleted; `compile()` is closed-form, §9.]** `compile()` is now a **dependency-ordered fixed point**. Each bound is enforced as soon as every coefficient it reads exists, and again after every solve it depends on. Any clamp re-runs exactly the calibrations downstream of the clamped coefficient. It terminates because every clamp only lowers a coefficient. | §9.1 |
| R2-P0-3 | The jerk/no-reversal proof is invalid: the real ramp time is L/J, near-zero crossing is possible, and the guard's `a_new` rewrite breaks G1 | **[Superseded in rev 6: the a_cap controller is replaced by the provable two-stage model, §6.3–6.5.]** Three changes:<br>1. B3 is **deleted**.<br>2. A **jerk-safe stopping invariant**: every component of the total command that reduces either the velocity error or \|v\| is capped by the discrete jerk-limited stopping bound `a ≤ a_cap(e) = −J·h + √(J²h² + 2J·|e|)` (the normative rev 5 form; this summary previously misquoted it, R3-P1-6), applied against the target **and** against zero (the "zero waypoint"). This is the standard `v ≤ a²/(2J)` condition, with a one-tick discrete correction.<br>3. The guard **never rewrites `a`**, so **G1 holds unconditionally**.<br>G2 is proven for 1-D channels (vertical, and straight-line horizontal). It is **explicitly relaxed** in exactly one place: horizontal motion that is turning, where the guard clamps only velocity, counts the hit, and tests require 0 hits. Settle now snaps only below `J·h²/2`, which also keeps the *effective* acceleration within G1. `t_j` now means "ramp time of the largest calibrated limit" (`J = L_max/t_j`), solved by fixed point. | §6.3, §6.4, §6.5, §9 |
| R2-P1-4 | A 1 % gate is impossible for tick-quantised times | Threshold crossings are **interpolated within the tick**, using the same linear interpolation as rendering. The 1 % gate applies to the interpolated time. The raw tick-quantised crossing is separately asserted within one tick `h`. | §9, §16.1 |
| R2-P1-5 | Foreign Keep uses a two-frame secant | Foreign Keep now requires **≥ 3 consecutive samples from the same foreign owner, all PRESENTED** (`gFrameCount` advanced). It also requires an **acceleration-consistency check** between the two secants. Any failure → Stop. The C¹ claim is restated with its tolerance. | §11.2.1 |
| R2-P1-6 | SELF Keep compared the post-operator camera but resumed unshaken and reset the operator, which can pop | SELF gaps (Stop **and** Keep) **do not reset the operator**: its state and rendered offset continue, and Drone resumes its own unshaken pose. FOREIGN gaps seed the **final rendered (post-operator) pose** and then reset the operator, so the offset is 0 on the resume frame. There is no pop either way. | §11.2, §11.7 |
| R2-P1-7 | Euler rates jump 180° across ±90° | Every seeded rate (Classic/Orbit history, foreign Keep) comes from the **quaternion delta**: shortest-arc, then an angular-velocity vector, then a projection onto Drone's yaw, gimbal and roll axes. Within 5° of vertical, the shared up/forward component goes to yaw only. | §11.5 |
| R2-P1-8 | Action 6 recentre needs a latch | A recentre **latch inside `step()`**, with its target, ease, completion rule, stick cancellation and lifetime (runtime-only) all specified. | §6.3 step 9, §12.1 |
| R2-P1-9 | The counting scope omits the shared Level-horizon cleanup (`ljc:2023-2027`) | That cleanup is **moved and counted** as the tail of the extracted unit, and the golden tests cover it. | §16.2.1 |
| R2-P2-10 | A lost or restored migration flag re-runs the fill | The BOOL is replaced by a **never-decreasing integer** `JoystickXInputDpadMigrationVersion`. The re-run behaviour is documented and is harmless, because the fill is None-only. | §12.1, §13 |

---

## rev 11 changes (Codex R8 resolution)

Codex R8 verdict on rev 10: **R7-P0-1 closed**. R7-P0-2 needed one precedence rule, and T10 had a stale bound.

| # | Codex R8 finding | rev 11 fix | § |
|---|---|---|---|
| R8-1 | Settle / re-baseline and a pending pole switch can be eligible on the same tick; `p_after` was "pending only for a switch-over", with no precedence | **One atomic snap per tick.** When steady-near holds, a single snap `y1 = v = v_r(n)` applies **every** eligible change at once: it adopts the pending pole if one exists, resets `L_tot^max` to the current `L_tot`, and is the settle when `v_r(n) = 0`. All snap kinds share the target `v_r(n)`, because settle requires `v_r(n) = 0` exactly. Eligibility always uses `p_after = pending p` whenever one is present, and `L_tot^after = current L_tot`. | §6.3 step 6, §6.4, T15, T49, T52 |
| R8-2 | T10 asserted `\|Δv\|/h ≤ L_tot` every tick, which contradicts §6.4/G1/T49 after a limit drop | T10 now asserts `\|Δv\|/h ≤ L_tot^max`, with `J_eff` computed from `L_tot^max`. | T10 |

---

## rev 10 changes (Codex R7 resolution)

Codex R7 verdict on rev 9: **NOT APPROVED**, with the last precise details.

| # | Codex R7 finding | rev 10 fix | § |
|---|---|---|---|
| R7-P0-1 | Steady-near gives `\|y1\| ≤ ε_q` but `\|v\| ≤ 2ε_q`, so the tail is `≤ 1.5·ε_q·t_j`, not `ε_q·t_j` | **The correct bound is stated:** `0 ≤ ε_trunc ≤ 1.5·ε_q·t_j ≤ 0.001875·t_j` m, ≤ 0.375 mm shipped. §6.4, §9 and T5–T9 are fixed, and T5–T9 add an adversarial settle at `\|y1\| = ε_q`, `\|v\| = 2ε_q`. The `d_stop` tolerance is `max(1 mm, 1.5·ε_q·t_j)`. | §6.4, §9, T5–T9 |
| R7-P0-2 | `ε_q` must hold against the POST-snap `J_eff` (a re-baseline resets `L_tot^max`; a switch-over changes `g_max`) | Snap eligibility uses **`J_⋆ = min(J_eff^before, J_eff^after)`**, where the after value uses the **pending** pole and the **current** `L_tot`. The snap tick is bounded by `3/16·J_⋆` (both sides), and the next tick by `5/8·J_eff^after`, with `g_0^after ≤ g_max(p_after)`. T15, T49 and T52 exercise large **downward** changes: an `L_tot` drop ≥ 30×, `t_j` 0.05 → 1.0 s, and both at once. | §6.3 step 6, §6.4, T15, T49, T52 |

---

## rev 9 changes (Codex R6 resolution)

Codex R6 verdict on rev 8: **NOT APPROVED**, with two snap-bound items left.

**Closed by R6:**
- the 5 mm/s cap preserving the settle jerk bound;
- the T22 floor;
- indefinite `max(old, new)` retention;
- per-channel G2/T51.

| # | Codex R6 finding | rev 9 fix | § |
|---|---|---|---|
| R6-P0-1 | Settle may fire with `\|v_r\| ≤ ε_snap`, so the omitted distance includes reference input, and `ε_trunc ≤ ε_snap·t_j` is unproven | **Settle requires `v_r = 0` exactly** (stage 1 reaches it through the clamp at 0 or the Drift snap). The omitted distance is then only the zero-input smoother tail, so `ε_trunc ≤ ε_q·t_j`, proven in §6.4. T5–T9 assert it, plus `v_r = 0` on every settle tick. | §6.3 step 6, §6.4, §9, T5–T9 |
| R6-P0-2 | Per-condition `ε_snap` thresholds allow `\|v − v_r\| ≤ 2ε_snap`, so snap jerk can reach `4ε_snap = J_eff·h²`; the `J_eff/2` claim and T49 were false | Every snap condition is tightened to **`ε_q = ε_snap/4`**. **One proof** covers settle, re-baseline and the pole switch-over: jerk ≤ `(3/16)·J_eff` on the snap tick (`|Δv_snap| ≤ 2ε_q`, plus the previous `|Δv| ≤ ε_q`), and ≤ `(5/8)·J_eff` on the next tick (`g_0·L_tot·h ≤ J_eff·h²/2`, plus `2ε_q`). T15, T49 and T52 now assert these exact bounds, including an adversarial case with every condition held exactly at `ε_q`. | §6.3 step 6, §6.4, T15, T49, T52 |

---

## rev 8 changes (Codex R5 resolution)

Codex R5 verdict on rev 7: **NOT APPROVED**.

**Closed by R5:**
- the Free-fly combined clamp;
- saturated drag: it is continuous at `V_cap`, keeps the exact coast flow below it, and has no snap.

| # | Codex R5 finding | rev 8 fix | § |
|---|---|---|---|
| R5-P0-1 | The stop-distance solve omits the settle truncation, and 1 mm tests can fail | **Guarantee weakened, as R5 allowed.** The camera distance is `d_stop − ε_trunc`, with `0 ≤ ε_trunc ≤ ε_snap·t_j` (proven from the smoother's remaining sum). The test asserts `d_stop − max(1 mm, ε_snap·t_j) ≤ d_cam ≤ d_stop`. All snaps now use `ε_snap = min(J_eff·h²/4, 0.005 m/s)`. The fixed 5 mm/s cap also prevents a visible settle snap when quadratic drag at `V_cap` makes `J_eff` large (FPV at 30 Hz), and keeps `ε_trunc ≤ 1 mm` for every shipped preset. | §9, §6.4, §6.3 step 6, T5–T9 |
| R5-P0-2 | `L_tot^max` cannot retire the old bound by tail weight; the kernel has infinite support | **Never retired by tail weight.** `L_tot^max` = the max of every `L_tot` since the last **exact steady point** (`y1 = v = v_r`), where the earlier history provably contributes zero. Steady points are seeds, settle, the pole switch-over, and a new **re-baseline snap** (same conditions and jerk proof as settle). Continuous maneuvering keeps `max(old, new)` indefinitely, and T49 asserts exactly that. | §6.4, T49 |
| R5-P0-3 | Lemma 5's single `λ` is false for mixed H/Z motion (different limits) | G2 is **restated and proven per channel**: H (planar) and Z (1-D) when horizon-locked, F (3-D) in Free-fly, the Free-fly Drift flow now acting on the full 3-D speed. No channel reverses. The 3-D direction may bend and is explicitly allowed to. T51 now includes mixed-Z stops in both modes. | §6.4 Lemma 5, §6.5, §6.3 step 2, T51 |
| R5-P1-4 | T22 still had the obsolete `d_stop ≥ 2·V_t·h` | T22 now asserts `d_stop ≥ V_t·(t_j + 2h)`. | T22 |

---

## rev 7 changes (Codex R4 resolution)

Codex R4 verdict on rev 6: **NOT APPROVED, converging**.

**Closed by R4:**
- R3's `a_cmd`/`a_new` counter-example;
- zero passage;
- the old fixed point;
- agent-origin;
- the toggle-time call;
- the summary mismatch.

**Confirmed sound by R4:**
- the summation-by-parts identity;
- `TV(g) = 2·g_max`;
- large-h stability (`0 < p < 1`);
- the settle-snap jerk bound from a steady seed;
- the smoother as a defensible feel trade-off. In-world preset tests are still pending.

| # | Codex R4 finding | rev 7 fix | § |
|---|---|---|---|
| R4-P0-1 | `D_brk` calibrates the mass stage, not the camera distance: from a steady nonzero state, a unity-DC smoother adds exactly `V_t·t_j` before settle truncation | `D_brk` now solves `d(D) = d_stop − V_t·t_j`. The camera distance is `d(D) + V_t·t_j − ε_trunc`, with the settle truncation bounded explicitly (microscopic). Validation enforces `d_stop ≥ V_t·(t_j + 2h)`, and the UI shows that floor. The preset table is recomputed. T5–T9 now assert the **camera** distance and the `V_t·t_j` identity. | §9, §10, §16.1 |
| R4-P0-2 | Lemma 1 under-bounds Free-fly: the stick can carry Z while `A_climb` also acts on Z | Free-fly uses **one combined 3-D clamp** (the target includes the climb term, then `clamp_len` to `V_t`) and does **not** run the vertical step. Lemma 1 is restated per mode: horizon-locked has three truly orthogonal parts; Free-fly has along ⟂ cross only, valid under any alignment. T50 added. | §6.2, §6.4 |
| R4-P0-3 | A preset or build-scale reduction can leave `\|v_r\| > V_cap`; a later Drift step breaks the drag bound | **Drag deceleration is saturated** at `L_drag = c1·V_cap + c2·V_cap²`. Lemma 1 therefore holds for **any** inherited speed, and the exact flow is unchanged below `V_cap`, so `t_coast` stays exact. **No velocity is ever snapped.** Coefficient changes: stage-1 limits apply immediately, Lemma 3 uses `L_tot^max` over the smoother memory, and a changed onset pole waits for the first steady tick (same jerk bound as settle). T49 and T52 added. | §6.3, §6.4, §10, §11.8 |
| R4-P1-4 | Lemma 5 assumed the stop basis stays put; braking with yaw or stick turn rotates `n` | A **zero target always uses `n = v̂_r`**, the motion's own direction, regardless of stick or yaw. Every stop step is then `v_r ← λ·v_r`, `λ ∈ [0,1]` (**per channel since rev 8**: H/Z or F, R5-P0-3), and G2 is proven during stops **with** steering input. A *stop* is defined precisely; partial brake or steering (`s* > 0`) is explicitly outside G2. T51 added. | §6.3 step 1, §6.4 Lemma 5, §6.5 |

---

## rev 6 changes (Codex R3 resolution)

Codex R3 verdict on rev 5: **NOT APPROVED**.

**Verified closed by R3:**
- interpolated gates;
- three presented foreign samples;
- quaternion-derived rates;
- the recentre latch;
- Level-horizon counting;
- the versioned migration.

R3's agent-origin finding was re-verified locally. `LLAgent::setRegion` shifts the agent frame and calls
`LLViewerCamera::setOrigin(camera − delta)` directly (`llagent.cpp:1087-1111`). The second `moveFlycam` caller
was also re-verified: `toggleFlycam` → `moveFlycam(true)` (`llviewerjoystick.cpp:2266`).

**Design choice (R3 invited "simpler and provable beats clever").** rev 6 **replaces** the rev 4/5 controller with
a two-stage model whose guarantees follow from two textbook facts:
1. **Stage 1, "mass":** every update is either a clamp toward the target or the *exact* solution of an ODE over one tick (taper or drag). Each tick's velocity change is therefore bounded, and the human-terms times have **closed forms**.
2. **Stage 2, "onset softness":** a critically damped two-pole smoother whose discrete kernel `g_k ≥ 0`, `Σg = 1`. The camera velocity is then a **convex combination** of past reference velocities, and acceleration and jerk bounds follow from the kernel's sum and total variation. This is the summation-by-parts argument in §6.4.

This deletes, together with every iteration and every bound the model can violate:
- the tracking gain K and bounds B1, B2 and B4;
- the a_cap cap and the guard;
- zero passage;
- feed-forward;
- the compile fixed point.

The preset feel (taper, cruise/top, Hold/Drift, brake, grip, turn weight, onset) is preserved.

| # | Codex R3 finding | rev 6 fix | § |
|---|---|---|---|
| R3-P0-1 | The stopping invariant constrains `a_cmd`, not the jerk-limited `a_new`; counter-example (J = h = 1, v = 1, a = 2, v* = 1.1) | **Controller replaced.** There is no `a_cmd`/`a_new` pair any more. Stage 1 can never overshoot its target, because every step is a clamp onto it or an exact flow bounded by it. Stage 2 output is a convex combination of stage-1 values, so it never exceeds the range of its inputs (the counter-example cannot occur). Acceleration and jerk bounds (`L_tot`, `J_eff`) are **proven on the camera velocity itself**, per vector, for the planar case too. T10/T11 rewritten. | §6.3–6.5, §16.1 |
| R3-P0-2 | The zero-passage snap breaks the effective-jerk bound | **Zero passage deleted.** A reversal passes through zero continuously inside the smoother. The only remaining snap is **settle**. It fires only when all three velocity states and the last Δv are ≤ `J_eff·h²/4`, and the proof that its jerk is ≤ `J_eff/2` is in §6.4. | §6.3 step 6, §6.4 |
| R3-P0-3 | `compile()` is not a proven fixed point; B2 cannot be repaired via c2/K when `h·c1 > 1` | **No iteration.** All coefficients are **closed-form**:<br>• `A_max = V_t·atanh(0.9)/t_top`, `A_dec = 0.9·V_c/t_hold`, `A_climb = 0.9·V_climb/t_climb`;<br>• `c1 = −ln E / t_coast`, with E independent of c1;<br>• filter pole `p = t_j/(t_j + 2h)`.<br>The one exception, `D_brk`, is a monotone scalar inversion of an **explicit** discrete-sum formula (bisection, no simulation). B2 no longer exists, because drag is integrated by its exact flow, which can never reverse or overshoot. | §9 |
| R3-P0-4 | Agent-origin shifts move the live camera behind dispatch ownership; Drone state, history and sampler stay in the old frame | Two changes. Sampler and pose-history positions are stored in **global** coordinates (`LLVector3d` via `gAgent.getPosGlobalFromAgent`), so secants are shift-invariant. Drone keeps an **origin tag** (the agent origin, in global coordinates); on any change it **translates** `p`, `prev`, `cur` and the agent-space history by the same delta before doing anything else. | §11.9 |
| R3-P1-5 | `moveFlycam(true)` from `toggleFlycam` (outside `idle()`) can clear foreign history | Serial and foreign bookkeeping happen **only in the tagged arbitration call**. A toggle-time call is **reset-only**: it reseeds Drone with Stop and sets `resetPending`. The first tagged FLYCAM dispatch consumes it, re-baselines `lastWrittenSerial` and the foreign flag, and never applies Keep across a toggle. | §11.2 |
| R3-P1-6 | The rev 5 summary's `a_cap` differed from the normative form | The rev 5 summary row is corrected to the normative `−J·h + √(J²h² + 2J·x)` and marked superseded. No test or section uses `a_cap` any more. | rev 5 table |

---

## rev 4 changes (Codex R1 resolution)

Codex R1 verdict: **NOT APPROVED** (review of rev 3 at `4bb6dc61eddc`). Every item is mapped to its fix below.
*rev 6 note:* the rev 4 fixes for P0-2 to P0-5 (the servo, bounds B1–B5, the single jerk limiter and the simulated
calibration) are **superseded** by rev 6's two-stage model (§6) and closed-form compile (§9). The R1 requirements
they answered still hold:
- jerk limited on the total acceleration;
- one consistent discrete model;
- an honest `t_top`;
- stability.

### P0

| # | Codex R1 finding | rev 4 fix | § |
|---|---|---|---|
| P0-1 | Parallel `FlycamDroneTriggerMode` / Roll-Clutch state machine conflicts with the existing `JoystickXInputTriggerMode` (D-pad Down, toast, rumble) | **Deleted.** There is ONE persisted trigger mode: `JoystickXInputTriggerMode`. Drone "Clutch" **is** the existing THROTTLE mode. While Drone is active, RT = throttle and LT is **reinterpreted as brake**. Classic THROTTLE semantics (`Max^RT·Min^LT`, which scales translation AND rotation) are unchanged. D-pad Down, `xInputHint` and `RUMBLE_MODE_TICK` are reused as they are. No new trigger settings, buttons or state. | §8 |
| P0-2 | §6/§9 derived from different systems than the integrator that runs | §6 is now **one discrete tick function** `step()`. It is the only definition of the model. §9 calibrates every human-terms value by **simulating that same `step()`**. There are no closed-form derivations left in the runtime path. | §6, §9 |
| P0-3 | No stability bounds; semi-implicit Euler alone proves nothing | Explicit bounds B1–B5: **0 < K·h ≤ 1**, a linearised total-gain bound, K·t_j ≤ 1, a seed-speed cap, and a no-reversal guard. `compile()` enforces them *before* calibration, so the calibrated numbers always describe the coefficients that actually run. | §6.4 |
| P0-4 | Jerk limit skipped brake and drag; brake was instantaneous | Brake is no longer a separate impulse. It lowers the servo target and raises the decel limit. **One jerk limiter acts on the TOTAL commanded acceleration**: servo + brake + drag + feed-forward. | §6.3 step 5 |
| P0-5 | `t_top = tanh` ignores jerk, gain limit and drag | **Calibrated.** `A_max` is solved by bisection on the actual discrete model (jerk, drag, feed-forward, taper and gain all included) at the configured sim Hz. If a request is infeasible, the UI shows the **achieved** value marked "≈" in amber. | §9 |
| P0-6 | Recorder, Path Camera and Director stop `moveFlycam` from running; no reseed when the camera comes back | **Ownership-gap detection**: a frame-counter gap since Drone last wrote. On regain, Drone reseeds from the **rendered camera** if another owner moved it, or else from its own last pose, and resets the operator. Velocity follows `FlycamDroneResumeMomentum` (U4): Stop gives v = 0; Keep gives a validated, capped velocity. | §11.2 |
| P0-7 | Orbit ownership cannot be known at the hook; it currently resolves after integration | **One owner, decided once per frame.** The `resolveAnchor` call is hoisted to the top of `moveFlycam`. It is `const` and pure-read (`llcinematiccamera.cpp:743`), so Classic behaviour is equivalent. Orbit-owner frames run Classic exactly as today. Drone reseeds on **both** edges. | §11.3 |
| P0-8 | `m3DCursor` absolute semantics undefined | **Excluded.** Drone is inactive when `Cursor3D` is on: Classic runs and the UI greys out Drone with a reason. This matches `ALXInput::compatible()` (`alxinputmath.h:37-43`). | §11.6 |

### P1

| # | Finding | rev 4 fix | § |
|---|---|---|---|
| P1-1 | Classic→Drone velocity ignores the BD-clamped timestep; Classic feather distorts the Drone→Classic seed | Classic→Drone seeds v from the **observed pose history** over the actual (unclamped) frame dt, so the BD [0.016, 0.033] clamp is accounted for automatically. Drone→Classic writes the `sDelta` seed **after** the edge frame's feather update, so Classic's first feather step acts on the seed and never pre-distorts it. Transition tests T26–T30. | §11.4, §16 |
| P1-2 | Quaternion seeding singularity | Explicit policy: horizon projection; keep the prior yaw when the projection is below 1e-4; pitch stored in [−90°, 90°] with an 89° input clamp that never snaps a seeded value; roll absorbs the remainder (exact reconstruction); canonical quaternion sign. Continuity sweep tests T19–T21. | §11.5 |
| P1-3 | Synthesised `sDelta` duplicates weaker logic | Drone feeds the operator through its existing **`LLCameraOperator::updateFromPose(dt, pos, rot)`** (`llcameraoperator.cpp:630`). There is no `sDelta` synthesis. | §11.7 |
| P1-4 | Settings belong in `settings.xml` | All new keys go in `settings.xml`, next to the Flycam/XInput block. Nothing goes in `settings_alchemy.xml`. | §13 |
| P1-5 | "Byte-identical" is not a claim the design can make; golden tests are needed | The claim is now **behavioural equivalence**. The Classic integrators are extracted verbatim into a pure, testable unit (WP0) with golden tests across BD/non-BD × XInput off/ROLL/THROTTLE × build mode × dt. Orbit and operator-off are covered by an unchanged-hunk structural guarantee plus in-world A/B. | §16.2, §17 |
| P1-6 | Fixed step is deterministic only for inputs on the same timeline | Caveat stated. Exact invariance is guaranteed **only for identical per-tick input**. Frame-held live input has a bounded divergence. Tests cover timestamped input, interpolation latency, jitter and 512-step backlog recovery. The dt clamp policy is now explicit. | §7, §16 |
| P1-7 | Delegation conflicts with `CLAUDE.md` (Fable retired) | §19 rewritten. Codex implements everything, including the core model. Codex reviews adversarially (current user directive). Claude builds. | §19 |

### §3 re-verification (Codex R1) → table corrected

- **F1 partial:** `BlackDragonControls` defaults to 0, **but Xbox Defaults set it TRUE**, so Xbox users run the BD path.
- **F2 false:** `sDelta` *does* carry a decaying displacement. Classic has a symmetric first-order lag. It does not have mass.
- **F5 partial:** `AutoLeveling` defaults to 1, but Xbox Defaults set it 0.
- **F8 partial:** Jump/Crouch, roll buttons and bumper zoom are added in the BD loop only.
- **F12:** the operator clamps input dt to [0, 0.25] s and sim Hz to 30–240. Legacy locomotion at zero lag bypasses fixed stepping.
- **F14 false:** rumble exists.

The full corrected table is in §3.

### XInput reconciliation (Codex R1) → §8, §12

- **One mode.** Drone Clutch = THROTTLE. No parallel setting.
- **LT = brake only while Drone is active.** Classic keeps `Max^RT·Min^LT` scaling of translation **and** rotation (`llviewerjoystick.cpp:1844-1846`, `:1954-1956`). Drone's speed-only meaning, which leaves rotation unscaled, is mode-gated.
- **Trigger samples.** Drone uses the independent XInput LT/RT samples with the existing freshness, association, takeover and release-pending logic. It never splits DirectInput axis 2.
- **D-pad.** D-pad Down stays the trigger-mode toggle. Gimbal recentre moved off it. Drone actions are **new values in the existing D-pad action enum** (Left/Right/Up), not mapped joystick buttons.
- **No new mapped buttons.** R3 (button 9 = Mouselook) and View (button 6 = Run toggle) get no new defaults. Collisions are impossible by construction.
- **Feedback.** Reuse `xInputHint` and `RUMBLE_MODE_TICK`. The "no rumble" premise is deleted.

### P2 → §2.2 (deferred) and §16 (tests)

- **Deferred until the core mass mode passes its tests:** cosmetic bank, FOV kick, look-ahead, look acceleration, speed-scaled gimbal, RC (Mode 2) layout, telemetry graph, preset/parameter cross-fades, separate non-XInput throttle/brake axes, momentary clutch.
- **Tests added:** overspeed, reversal, simultaneous brake/turn/drag, zero grip, settle, NaN settings, sim-Hz extremes, ownership regain, ±90° seeding, XInput focus/reconnect, D-pad conflicts, and both Classic integrators.

---

## 1. The ask

> "A dynamic clutch that can ramp flycam speed for more dramatic shots." — "The triggers should impact the
> movement mid-flight." — "Right now it just controls the direction, but has no impact on the momentum." —
> "Make it feel natural, triple-A game input." — "Primary focus is an Xbox controller; Alchemy supports
> non-Xbox controllers too." — **"I'm not trying to change DirectInput. Mainly just trying to add weight and
> mass to flycam, making it feel drone and natural."** — **"add a mode to flycam so it feels like it has mass."**

The DirectInput/libndof device code and the axis/button mapping semantics are **not** modified. Drone reads
the same values `moveFlycam` already reads, plus the XInput trigger samples the existing XInput layer already
polls.

---

## 2. Scope (ship whole)

### 2.1 In this delivery
1. **`LLFlycamDrone`** (`indra/newview/llflycamdrone.{h,cpp}`) is a pure, unit-testable flight model (§6). It also contains the fixed-step loop and interpolation (§7), calibration (§9), and the seeding/transition helpers (§11).
2. **Flight model switch** `FlycamFlightModel` = Classic | Drone. Classic is the default.
3. **Mass:**
   - stage 1 (rev 6): a target-velocity reference with acceleration limits and an exact power-curve taper;
   - exact-flow drag for the Drift glide;
   - progressive brake;
   - grip;
   - speed-dependent turn weight;
   - Hold vs Drift release;
   - Assist vs Pure throttle;
   - altitude hold;
   - stage 2: a critically damped onset smoother, with **proven** acceleration and jerk bounds on the camera velocity covering every source (§6.4).
4. **Drone frames:** world-up yaw, horizon-locked travel, separate gimbal tilt and a roll state. A Free-fly sub-mode is available for FPV.
5. **Input shaping** after the existing reads: radial dead zone, saturation and expo on the sticks. Triggers use the existing XInput shaping.
6. **Unified triggers** on `JoystickXInputTriggerMode` (§8).
7. **Correct handling of every ownership edge:** Classic↔Drone, Orbit, higher-priority camera owners, reset/teleport, and 3D cursor (§11).
8. **Settings, presets, UI, D-pad actions, log verdicts, and TUT tests.** This includes WP0, the Classic extraction with golden tests.

### 2.2 Deferred (R1 P2: only after the core mass mode passes its tests)
- Cosmetic bank, FOV kick, look-ahead, look acceleration and speed-scaled gimbal.
- RC "Mode 2" stick layout.
- Telemetry graph. Log verdicts (§15) remain in scope.
- Preset/parameter cross-fades. A preset switch applies immediately, under the §6.4 coefficient-change rules (immediate stage-1 limits, saturated drag, deferred pole change). The proven bounds hold throughout.
- Separate throttle/brake axes for non-XInput pads. Until then, a non-XInput pad gets Drone with Assist cruise and no brake (§8.4).
- Momentary clutch on a held button.

### 2.3 Not changing
- DirectInput/libndof device code, and the semantics of `JoystickAxis*` and `JoystickButton*`.
- The Classic flight model. Its behaviour is equivalent (§17). Its code is moved by WP0, verbatim.
- Flycam Orbit maths.
- The handheld operator's internals.
- The flycam recorder and its file format.
- XInput association, polling and rumble machinery. The only XInput change is the D-pad enum extension and Drone-aware hint text.

---

## 3. What the LOCAL code proves today (re-verified @ `4bb6dc61eddc`)

`ljc` = `indra/newview/llviewerjoystick.cpp`.

| # | Fact (rev 4) | Where | R1 |
|---|---|---|---|
| F1 | `moveFlycam` has two integrators. The **BD branch comes first** (`if (blackdragon)`, `ljc:1769-1913`); the non-BD `else` follows (`ljc:1914-2021`). `BlackDragonControls` defaults to 0 (`settings_alchemy.xml:5-15`), **but `setXboxDefaults` sets it TRUE (`ljc:3174`)**, so Xbox-defaulted users run the **BD** path. Both paths must be covered. | as cited | partial → fixed |
| F2 | Per axis, `cur_delta = −axis` → dead zone → (XInput roll/throttle) → build scale → `× FlycamAxisScale[i]` → `× time` → `sDelta += (cur_delta − sDelta)·time·feather`. `sDelta` is a per-frame displacement that **does carry** a decaying displacement after release. Classic therefore has a symmetric first-order lag (onset time = release time), measured per frame. It has **no velocity state in m/s**, no acceleration or jerk limits, no speed-dependent turning and no brake. *That* is the missing "mass". | BD `ljc:1797-1868`; non-BD `ljc:1929-1980` | false → fixed |
| F3 | Xbox Defaults set `FlycamFeathering = 1.0`. The stock default is 16. | `ljc:3249`; `settings.xml` `FlycamFeathering` | true |
| F4 | Translation uses the full camera orientation, `sFlycamPosition += LLVector3(sDelta) * sFlycamRotation`, so looking down and pushing forward dives. | `ljc:1880` (BD), `ljc:1988` | true |
| F5 | Rotation is incremental in the camera-local frame (`LLMatrix3 rot_mat(sDelta[3..5])`). Leveling runs on `AutoLeveling` / Level-horizon / `ROLL_DEFAULT` (BD). `AutoLeveling` defaults to **1** in `settings.xml`, but **Xbox Defaults set it 0** (`ljc:3201`). | `ljc:1881-1901`, `ljc:1990-2009` | partial → fixed |
| F6 | BD clamps `time` to [0.016, 0.033] s, which makes it frame-rate dependent. Non-BD clamps `time ≤ 0.2`. | `ljc:1771-1774`; `ljc:1916-1922` | true |
| F7 | Dead zones are per axis, which makes diagonals notchy. | `ljc:1819-1840`, `ljc:1943-1950` | true |
| F8 | Xbox Defaults map roll = DirectInput axis 2 (`JoystickAxis3 = 2`, `ljc:3179`), roll buttons −1, Jump = 1 (B), Crouch = 0 (A), ZoomIn 4 / ZoomOut 5 (`ljc:3187-3198`). **Jump/Crouch→Z, roll buttons→roll and bumper zoom are added only in the BD loop** (`ljc:1802-1816`). The non-BD loop adds no buttons. | as cited | partial → fixed |
| F9 | Reset (`moveFlycam(true)` / `mResetFlag`) reseeds the pose from the camera, calls `resetDeltas`, reseeds XInput takeover, resets the operator and drops Orbit. | `ljc:1734-1748` | true |
| F10 | Flycam Orbit runs **after** integration. It resolves the anchor at `ljc:2054`, consumes the feathered `sDelta` as orbit controls (`ljc:2106-2143`), and **overwrites** the pose (`ljc:2159-2160`). | `ljc:2039-2163` | true |
| F11 | Classic operator feed: `opin.mLinearVel = sDelta[0..2]/dt`, `mAngularVel = sDelta[3..5]/dt`. It is applied to the rendered camera only. | `ljc:2188-2227` | true |
| F12 | The operator's fixed-step pattern: `MAX_FIXED_STEPS_PER_FRAME = 512`, `FIXED_STEP_EPSILON = 1e-9`, accumulator + interpolation, with backlog kept in the accumulator. **Corrections:** input dt is clamped to **[0, 0.25] s**, sim Hz to **30–240**, and Legacy locomotion at zero reaction lag **bypasses** fixed stepping. | `llcameraoperator.cpp:30-31`, `:512-518`, `:571-628` (clamps `:593`, `:596-598`; bypass `:582-586`) | true + corrected |
| F13 | The recorder samples the final render camera (pos, rot, FOV) regardless of what drives it, so it needs no format change. | `llflycamrecorder.h:5-11`, `Keyframe :65-71` | true |
| F14 | **Rumble exists.** Patterns are `RUMBLE_CONNECT / FLYCAM_ON / FLYCAM_OFF / MODE_TICK` (`alxinputmath.h:62-124`), played by `playRumble` (`ljc:2510`). D-pad Down plays `RUMBLE_MODE_TICK` (`ljc:2833`). | as cited | **false → fixed** |
| F15 | A shared easing evaluator exists. | `indra/newview/alcameracurve.h` | true |
| F16 | **XInput trigger authority.** `JoystickXInputTriggerMode` (0 ROLL, 1 THROTTLE; `settings.xml:13659`).<br>• **D-pad Down** toggles it, calls `xInputHint("Triggers: …")` and plays `RUMBLE_MODE_TICK` (`ljc:2828-2834`).<br>• **Takeover** = associated ∧ compatible ∧ flycam ∧ (THROTTLE ∨ RollShaped) (`ljc:2774-2776`).<br>• **Axis 2 zeroed** under takeover/release-pending: `getFlycamAxis` returns 0 for DirectInput axis 2 (`ljc:2789-2792`).<br>• **Gated output:** `computeTriggerOutputs` gates on freshness (`ljc:2794-2809`). THROTTLE = `Max^RT·Min^LT` (`alxinputmath.h:29-35`), with shaping via `ALXInput::shape` (dead zone 0.06, curve 2; `alxinputmath.h:16-22`, `settings.xml:13687-13700`).<br>• **Classic scaling:** THROTTLE scales translation AND rotation; zoom is excluded (`ljc:1844-1846`, `ljc:1954-1956`). | as cited | new |
| F17 | D-pad Left/Right/Up are **configurable actions**, not mapped buttons. The enum is 0 None, 1 Cue Go, 2 Cue Back, 3 Level horizon (`settings.xml:13701-13721`; handler `ljc:2835-2857`). All three default to 0. | as cited | new |
| F18 | Xbox button use (`ljc:3187-3198`): View(6) = Run toggle, Menu(7) = ZoomDefault + Flycam toggle, **R3(9) = Mouselook**, B/A = Jump/Crouch. | as cited | new |
| F19 | `ALXInput::compatible()` returns false when `Cursor3D` is on, so XInput is excluded and status reads "legacy: 3D cursor". | `alxinputmath.h:37-43`; `ljc:2866` | new |
| F20 | **Camera dispatch priority:** AgentPilot > Recorder playback > Path Camera > Cinematic Camera > flycam. The Director Switcher takes the camera via `CinematicCamEnabled` (`aldirectorswitcher.cpp:321-327`, `:342`). **Nothing resets the flycam when it regains ownership.** | `llappviewer.cpp:5594-5619` | new |
| F21 | `LLCinematicCamera::resolveAnchor` is `const` and only reads the avatar/joint, so it can be hoisted within the frame. | `llcinematiccamera.cpp:743-776` | new |
| F22 | **Operator pose API** `updateFromPose(frame_dt, pos, rot)` derives camera-local linear and angular velocity itself: frame-to-frame for Legacy, fixed-tick pose segments otherwise. It is already used by the recorder, cinematic camera and agent camera. | `llcameraoperator.cpp:630-786` | new |
| F23 | Pre-existing defect, **not fixed here**: in BD, `mInvertPitch` inverts `cur_delta[CAM_Y]` **after** the loop has consumed it, so it is dead (`ljc:1870-1872`). Xbox inverts via a negative `FlycamAxisScale4` (`ljc:3221`). | as cited | new |
| F24 | Flycam/XInput settings live in `settings.xml` (e.g. `:12902`, `:13659`). `settings_alchemy.xml` is a small override table. | as cited | new |
| F25 | `LLAgent::setRegion` re-bases the agent frame and moves the live camera by `−delta` directly via `LLViewerCamera::setOrigin`, outside camera dispatch. Classic's `sFlycamPosition` is **not** re-based: pre-existing, untouched. | `llagent.cpp:1087-1111` | new (R3) |

**Consequences**
- **Recorder (F13):** recorded takes replay exactly. Drone needs no recorder change.
- **Fixed stepping:** it buys live-performance consistency across frame rates, within the limits stated in §7.
- **Ownership (F20):** Drone must detect regaining the camera itself (§11.2).
- **Pre-existing defects F6 and F23:** surfaced, separate tickets, out of scope.

---

## 4. Architecture — "Classic shadow + Drone pose override"

```
moveFlycam()
  updateXInputTakeover / focus early-outs / reset (UNCHANGED, F9)        ── Drone::reset(camera pose) added
  trigger_roll/throttle, level_horizon (UNCHANGED)
  orbit_owner = FlycamOrbitEnabled && resolveAnchor(...)    ◄── HOISTED (once per frame, F21); result reused below
  drone_on    = FlycamFlightModel==Drone && !m3DCursor && !orbit_owner
  edge handling (§11.4): seed on Classic→Drone / Orbit→Drone, hand off on Drone→Classic
  Classic BD / non-BD integrator (UNCHANGED behaviour; WP0 calls the extracted pure function)
        └─ always runs: computes sDelta / sLastDelta / zoom / AFK exactly as today ("shadow")
  if drone_on:
        input  = read+shape (§5) ;  trig = droneTriggers (§8)
        pose   = drone.advance(dt, input)                      ── fixed step + interpolation (§7)
        sFlycamPosition/Rotation = pose                        ── OVERWRITES the shadow pose (same idiom as Orbit)
  Orbit block (UNCHANGED maths; uses the hoisted anchor result)
  tail: FOV/lens write (UNCHANGED); operator:
        drone_on ? updateFromPose(dt, sFlycamPosition, sFlycamRotation) : update(sDelta) (UNCHANGED)
  pose-history bookkeeping (§11.4) ; lastWrittenSerial = gCameraDispatchSerial ; clear foreignSinceFlycamWrite (§11.2)
llappviewer.cpp idle(): ++gCameraDispatchSerial; owner tag per branch; after dispatch → final-camera sampler (§11.2)
```

Why a shadow:
- Classic still owns `sDelta`, `sLastDelta`, zoom, the bumpers, `ZOOM_DEFAULT` and AFK. Zoom therefore behaves exactly as today in both models.
- Orbit always sees the feathered `sDelta` it expects.
- The shadow costs a few dozen flops per frame.

`LLFlycamDrone` reads no settings and no globals inside `step()`. Parameters arrive in a struct produced by `compile()` (§9).

---

## 5. Input (Drone only; after the existing reads)

### 5.1 Raw read and direction parity
For each logical channel `i`:

```
raw_i = −getFlycamAxis(mJoystickAxis[i])
        (non-BD only: flip i==4 unless mInvertPitch)      ← exactly as the active Classic path
        × sign(FlycamAxisScale[i])
```

Classic also flips on `mInvertPitch`, but only in non-BD; F23 makes the BD flip dead. Taking only the **sign** of
`FlycamAxisScale[i]` gives Drone the same stick→direction sense as the active Classic path. This includes the
Xbox negative pitch scale. The *magnitudes* come from Drone's own parameters. Test T24.

Vertical input `u`:
- the Z channel, clamped to [−1, 1];
- plus `JUMP − CROUCH` buttons, **in both BD and non-BD** while Drone is active. This is mode-gated and needed
  because the Xbox map has no Z axis (F8).

### 5.2 Radial dead zone and saturation (per logical stick pair: move = (0, 1), look = (4, 5))
`r = |s|`, `r' = clamp((r − d)/(1 − d − o), 0, 1)`, `s' = r>0 ? s·r'/r : 0`.
- Defaults: move `d = 0.10`, look `d = 0.12`, `o = 0.05`.
- These replace `FlycamAxisDeadZone[i]` for Drone channels only.

### 5.3 Expo
`e(x) = sign(x)·((1−k)|x| + k|x|³)`, applied to the shaped magnitude `r'`.
- Defaults: move 0.35, yaw 0.45, gimbal 0.45.
- Roll (ROLL mode) uses the same per-axis dead zone as Classic.

### 5.4 Triggers
Triggers are covered entirely by §8. The existing `ALXInput::shape` shapes them, with the existing
`JoystickXInputTriggerDeadZone` and `JoystickXInputTriggerCurve`. There are no new trigger shaping settings.

---

## 6. The mass model — ONE discrete system, two stages (rev 6)

### 6.1 Notation
- `h = 1/Hz`: the fixed tick, with `Hz = FlycamDroneSimulationHz` clamped to 30–240 (the same range as the operator, F12).
- Agent space is metres and seconds, with Z up. `R_z(ψ)` is yaw about world up.
- `clamp_len(x, L)` scales vector x down to length L. `wrap` maps to (−π, π].
- **Parameters** (all closed-form, §9):
  - mass: `V_c, V_t, A_max, A_dec, D_brk, c1, c2, G, V_climb, A_climb`;
  - onset smoother: `p`;
  - rotation: `ω0, s_ref, α_yaw, Θ_rate, α_g, Φ_rate, α_roll`;
  - modes: `release ∈ {HOLD, DRIFT}, assist, freefly`;
  - derived bounds: `V_cap = 2·V_t`, `L_tot`, `J_eff` (§6.4).
- **Input** (sampled per frame, held across that frame's ticks):
  - `d` = (fwd, left), |d| ≤ 1, after §5;
  - `thr, brk ∈ [0,1]`;
  - `u, yaw, tilt, roll ∈ [−1,1]`;
  - `level` and `recentre` (edges).
- **State:**
  - `v_r` — stage-1 reference velocity, the "mass";
  - `y1` — smoother intermediate;
  - `v` — camera velocity;
  - `p` — position;
  - `ψ, ω, θ, θ̇, φ, φ̇`;
  - recentre latch `R`.

  All velocities are 3-D world (agent-frame) vectors.

### 6.2 Frames
- Camera orientation: `q_cam = R_z(ψ)·R_y(θ)·R_x(φ)`.
- Travel uses `R_z(ψ)` only, so it is horizon-locked: the gimbal tilt and roll never steer the motion.
- **Free-fly** (FPV) instead uses the full `q_cam` for the stick direction. It runs **one combined 3-D clamp** (R4-P0-2):
  - the target is `v* = clamp_len(s*·n3 + (0, 0, u_in·V_climb), V_t)`;
  - step 2 runs on the full 3-D `v_r`, with `n = v*/|v*|`;
  - **step 3 is not run.**

  So no axis ever receives two stage-1 accelerations in the same tick.

### 6.3 `step(S, I, h)` — the normative definition

```
STAGE 1 — the mass. Every update is a CLAMP toward the target or the EXACT flow of an ODE over one tick.
 1. target (horizontal)
    S_lim  = assist ? lerp(V_c, V_t, thr) : thr·V_t          (Pure falls back to Assist if triggers ≠ Drone-THROTTLE, §8)
    s*     = |d| · S_lim · (1 − brk)                          (brake lowers the target …)
    n      = s*>0 ? R_z(ψ)·d/|d| : (|v_r,h|>0 ? v_r,h/|v_r,h| : 0)
                                                               (R4-P1-4: a ZERO target — release, full brake — always uses the
                                                                current motion direction, so a stop is direction-preserving
                                                                whatever the stick or yaw does)
    engaged = |d|>0 ∨ brk>0 ∨ release==HOLD
 2. horizontal reference v_r,h
    if engaged:
       u = v_r,h·n ;  w = v_r,h − u·n                          (along / cross parts)
       L = lerp(A_dec, D_brk, brk)                             (… and raises the decel limit)
       if u < 0:          u = min(0, u + L·h)                  (moving against n: slow to 0, clamp AT 0)
       elif u > s*:       u = max(s*, u − L·h)                 (too fast: linear decel, clamp AT target)
       else:              u = min(s*, V_t·tanh(atanh(u/V_t) + A_max·h/V_t))
                                                               (exact flow of u' = A_max(1 − (u/V_t)²): power-curve taper)
       w = w − clamp_len(w, G·A_max·h)                         (grip: sideways momentum slews to 0, clamp AT 0)
       v_r,h = u·n + w
    else (Drift, released, no brake):  s = |v_r,h|   (Free-fly: s = |v_r|, the full 3-D reference — one channel, §6.4 Lemma 5)
       s = max( c1·s·E / (c1 + c2·s·(1 − E)),  s − L_drag·h ),  E = e^{−c1·h}
                                                               (exact flow of s' = −s(c1 + c2·s), with its deceleration SATURATED
                                                                at L_drag = c1·V_cap + c2·V_cap² (R4-P0-3); direction kept)
       if s ≤ ε_s: s = 0,  ε_s = min(L_tot·h, 0.01 m/s)           (exact-zero; the change is within Lemma 1's bound, and far below every 0.1·V_c threshold)
       v_r,h = s·v̂_r,h
 3. vertical reference (altitude hold, always; Drift affects horizontal only)
    v_r,z = v_r,z + clamp(u_in·V_climb − v_r,z, ±A_climb·h)    (u_in = vertical input; clamp AT target)

STAGE 2 — onset softness: critically damped two-pole smoother, identical for every component
 4. y1 = p·y1 + (1−p)·v_r ;   v = p·v + (1−p)·y1                p = t_j/(t_j + 2h)   (§9)
 5. position (semi-implicit):  p_pos += v·h
 6. snaps (rev 9). Each REPLACES this tick's step-4 smoother update. Conditions use this tick's stage-1 result
    v_r(n) and the previous smoother states. ε_q = ε_snap/4, ε_snap = min(J_⋆·h²/4, 0.005 m/s),
    J_⋆ = min(J_eff^before, J_eff^after) (after = pending pole and current L_tot; §6.4, R7-P0-2):
      steady-near:  |y1_{n−1} − v_r(n)| ≤ ε_q  ∧  |v_{n−1} − y1_{n−1}| ≤ ε_q  ∧  |Δv_{n−1}| ≤ ε_q
      settle:       no input ∧ v_r(n) = 0 EXACTLY (R6-P0-1) ∧ steady-near  →  y1 = v = 0
      re-baseline / onset-pole switch-over (§6.4):  steady-near  →  y1 = v = v_r(n)
                                                               (jerk ≤ 3/16·J_⋆ on the snap tick, ≤ 5/8·J_eff^after on the next, §6.4)
      ATOMIC (R8-1): at most ONE snap per tick. If steady-near holds and any snap is eligible (settle, re-baseline,
      pending pole), the single snap y1 = v = v_r(n) applies ALL of them together: adopt the pending pole if present,
      L_tot^max := L_tot, and (v_r(n) = 0) it is the settle. J_⋆ always uses p_after = pending p when one exists.
 7. rotation
    ω*  = e_yaw(yaw)·ω0 / (1 + |v_h|/s_ref) ;  ω += clamp(ω* − ω, ±α_yaw·h) ;  ψ = wrap(ψ + ω·h)
    recentre latch (D-pad action 6, §12.1): input edge `recentre` sets R = true
          R ∧ tilt == 0 (post dead zone)  →  θ̇* = clamp(−θ·Θ_rate/10°, ±Θ_rate)     // full rate far out, eases over the last 10°
          R ∧ tilt ≠ 0                     →  R = false (stick authority; θ̇ carries on through α_g, no snap)
          R ∧ |θ| < 0.1° ∧ |θ̇| < 0.5°/s    →  θ = 0, θ̇ = 0, R = false (complete)
          R is runtime-only state: cleared by reset, any reseed, ownership regain and a model switch; kept across a
          preset switch; a new edge while R is set restarts nothing (idempotent)
    θ̇*  = R ? (as above) : e_g(tilt)·Θ_rate ; θ̇ += clamp(θ̇* − θ̇, ±α_g·h) ; θ += θ̇·h ;
          θ clamped to ±θ_lim, θ_lim = max(89°, |θ_seed|) shrinking to 89° as soon as |θ| ≤ 89° (§11.5); clamp hit → θ̇ = 0
    φ:  level edge → φ = 0, φ̇ = 0 (snap, as Classic's Level horizon)
        else φ̇* = roll·Φ_rate (ROLL mode only) ; φ̇ += clamp(φ̇* − φ̇, ±α_roll·h) ; φ = wrap(φ + φ̇·h)
        AutoLeveling ∧ roll == 0 → φ −= φ·min(1, h/0.5 s)
 8. finite check: any non-finite → restore the last finite state, v_r = y1 = v = 0, log DRONE NONFINITE
```

**Notes**
- `|v_h|` in turn weight is the camera's horizontal speed.
- Rotation is rate-limited, not jerk-limited. The jerk guarantee is a translational guarantee.
- **Seeds** set `v_r = y1 = v = v_seed`, so the smoother starts at steady state with acceleration 0.
  - Stop: `v_seed = 0`.
  - History or Keep: `v_seed` from §11.2.1 / §11.4.
  - SELF Keep keeps all three states unchanged.
- **Feel of the parts:**
  - Stage 1 gives the power-curve pull-away, the cruise/top split, linear braking and Hold stops, grip, and the exact-drag Drift glide.
  - Stage 2 turns every corner of those profiles into an S-curve: the "weight" at onset and arrival.

### 6.4 Why it is safe — proofs (replace rev 4/5 B1–B5, V1, guard, zero passage)

Let `g_k = (1−p)²·(k+1)·p^k`, k ≥ 0, be the discrete impulse response of stage 2: two identical one-pole stages.
- `g_k ≥ 0` and `Σ g_k = 1`.
- From a steady seed, `v_n = Σ_k g_k·v_r(n−k)` for every component. Equivalently, for the vector, a convex combination of past references.
- `g` is unimodal: it rises then falls. Its total variation (counting the step up from `g_{−1} = 0`) is therefore `TV(g) = 2·g_max`, where `g_max = max_k g_k`. `compile()` finds it by scanning to the first decrease.

**Lemma 1 (bounded reference steps).** Every stage-1 tick satisfies `|v_r(n) − v_r(n−1)| ≤ L_tot·h`, where

```
L_h   = √( max(A_max, A_dec, D_brk)² + (G·A_max)² )               horizontal along ⟂ cross
L_tot = max( √( L_h² + A_climb² ),   L_drag ),    L_drag = c1·V_cap + c2·V_cap²
```

- **Clamps** move each component by at most its limit × h.
- **The taper flow's** derivative is `A_max·(1 − (u/V_t)²) ≤ A_max`.
- **The drag step** is saturated at `L_drag·h` by construction, so it is bounded **whatever the inherited speed** (R4-P0-3). That covers seeds, Keep, preset switches and build-scale reductions that leave `|v_r|` above the new `V_cap`. Below `V_cap`, the saturation is inactive and the exact flow runs unmodified, so the `t_coast` closed form stays exact for every `V_c ≤ V_cap`.
- **The Drift zero snap** changes s by at most `ε_s ≤ L_tot·h`, by its own threshold.
- **Horizon-locked:**
  - the along and cross parts lie in the horizontal plane and are orthogonal to each other (`w ⟂ n`);
  - the vertical clamp acts only on z;
  - the three are mutually orthogonal, so their root-sum-square bounds the step.
- **Free-fly** (R4-P0-2):
  - one combined 3-D clamp (§6.2), with no separate vertical step;
  - along ⟂ cross gives `≤ L_h·h ≤ L_tot·h`;
  - the bound allows any alignment of the stick with Z.
- **Switching Free-fly ↔ horizon-locked** mid-flight changes only which of these two step rules runs. Each is bounded by `L_tot·h`.
- **Input changes of any kind** (stick, throttle, brake, mode switch, takeover loss, stale samples) enter **only** through the targets of these same clamps and flows. The bound is therefore unconditional.
- **Coefficient changes** (preset switch, slider edit, build-scale ease; R4-P0-3):
  - **Stage-1 limits** change immediately. Each tick is bounded by *that tick's* limits.
  - **`L_tot^max` (R5-P0-2).** Lemmas 2–3 use `L_tot^max`: the maximum of every `L_tot` in force since the last **exact steady point**. It is never retired by tail weight; the kernel has infinite support.
    - **What a steady point is:** a tick where `y1 = v = v_r` holds exactly. From there, the future output equals the convolution of an input that was constant for all earlier ticks, so earlier reference steps contribute exactly zero, and `L_tot^max` may reset to the current `L_tot`.
    - **Where steady points occur:** seeds, the settle snap, the onset-pole switch-over, and a **re-baseline snap**. The re-baseline snap sets `y1 = v = v_r`, and fires only while `L_tot^max > L_tot` and the "steady-near" conditions of §6.3 step 6 hold (each ≤ `ε_q = ε_snap/4`).
    - **Snap bound:** its jerk is bounded exactly like settle (below).
  - **The smoother pole `p`** (onset `t_j`) is **not** changed mid-motion. A pending `p` is applied at the first tick where the "steady-near" conditions hold (§6.3 step 6). There it sets `y1 = v = v_r`, a snap with the same proven jerk bound as settle. Until then the old `p` runs.
  - The convolution identity therefore always holds, with one constant kernel per run segment, and every segment starts from a steady state.

**Lemma 2 (acceleration).** `Δv_n = Σ_k g_k·Δv_r(n−k)`, so `|Δv_n|/h ≤ L_tot^max` (triangle inequality, `Σg = 1`). Without a recent coefficient change, `L_tot^max = L_tot`.

**Lemma 3 (jerk — proven on the camera velocity, planar included).** Summation by parts gives
`Δ²v_n = Σ_k (g_k − g_{k−1})·Δv_r(n−k)`. Therefore:

```
|Δ²v_n| ≤ TV(g)·L_tot^max·h = 2·g_max·L_tot^max·h      ⇒      jerk ≤ J_eff = 2·g_max·L_tot^max / h   (= L_tot without a recent coefficient change)
```

- Norms make this hold for vectors exactly as for scalars, so the planar case is covered.
- For small `1 − p`, `g_max ≈ (1−p)/e` and `1 − p ≈ 2h/t_j`. So `J_eff ≈ 4·L_tot/(e·t_j) ≈ 1.47·L_tot/t_j`, essentially independent of Hz.
- **R3's counter-example cannot be built.** There is no separately commanded acceleration that can lag; the camera's acceleration *is* a kernel-weighted average of bounded reference steps.

**Lemma 4 (no overshoot of a held target).**
- **Stage 1** never passes its target. Every along, cross and vertical update clamps *onto* the target, and the taper flow is capped by `min(s*, ·)`.
- **Stage 2** output is a convex combination of the stage-1 history. If that history lies on one side of a value (e.g. all `≤ s*` while accelerating from below), so does the camera velocity.

**Lemma 5 (dissipation never reverses velocity) — G2, stated precisely and COMPONENTWISE (R5-P0-3).**
- **A *stop*** is any run of ticks from n0 on in which the horizontal target is zero (`s* = 0`: Hold release, full brake `brk = 1`, or Drift coast) and the vertical target is zero. Yaw, tilt and stick direction may change freely during it (R4-P1-4).
- **Channels.**
  - **Horizon-locked:** two channels, decelerated with different limits: **H**, the horizontal plane (2-D), and **Z**, vertical (1-D).
  - **Free-fly:** one channel, **F**, the full 3-D vector. During a Free-fly stop the target is zero, so `n = v̂_r` in 3-D and the Drift flow runs on `|v_r|` in 3-D.
  - A mixed horizontal/Z motion in horizon-locked mode **does** change 3-D direction during a stop, because H and Z stop at different rates. rev 7 wrongly claimed a single `λ`. G2 is therefore stated **per channel**.
- For each channel c, write `r̂_c = v_r,c(n0)/|v_r,c(n0)|`, skipping channels already at rest.
- **Why a channel's basis cannot rotate.** With a zero target, step 1 sets `n = v̂_r,c`. That depends only on the reference itself, not on ψ or the stick. The along part is all of `v_r,c`, and the cross part is `w = 0`.
1. **Stage 1 preserves each channel's direction under a stop.**
   - Within each channel, every step has the form `v_r,c ← λ_c·v_r,c` with `λ_c ∈ [0, 1]`. The `λ_c` may differ between channels.
     - **H or F:** along decel clamped *at* 0, the saturated drag flow, the Drift snap.
     - **Z:** the clamp to 0, a 1-D sign-preserving step.
   - Yaw input turns the camera, but cannot steer the stopping motion. That is the intended "momentum" feel.
   - So `v_r,c(n)·r̂_c ≥ 0` for all n ≥ n0 and every channel: **no channel's reference ever reverses.** The overall 3-D direction may bend, because H and Z finish at different times, but nothing goes backwards.
2. **The camera velocity, per channel:** `v_c(n)·r̂_c = Σ_k g_k·v_r,c(n−k)·r̂_c`. The smoother is linear and acts identically on every component, so it acts on each channel separately.
   - Every term with `n−k ≥ n0` is ≥ 0.
   - **If the channel's reference history in the smoother's memory also has `v_r,c·r̂_c ≥ 0`**, that channel of the camera velocity never reverses. That is the normal case: braking or releasing while moving in one general direction, with no commanded reversal just before.
   - Otherwise, the only possible negative part is the pre-stop tail: `v_c(n)·r̂_c ≥ −W_{n−n0}·max_{k<n0}|v_r,c(k)|`, where `W_m = Σ_{k>m} g_k` decays geometrically.
   - That residue can only come from a reversal or a sharp turn that the user *commanded* within the last few `t_j`. It is not created by brake, drag or Hold.
3. **This is the one relaxation of G2, stated exactly.** It replaces rev 5's turning-guard relaxation. There is no guard.
4. **Not a stop, and not covered by G2:** a *partial* brake or a reduced stick with `s* > 0`.
   - The reference then tracks a nonzero target along the stick, which may turn. That is steering, not dissipation.
   - The velocity cannot cross zero along the target direction, because the along clamp stops at `s* ≥ 0` (Lemma 4).
   - G2 makes no claim about the cross part while turning.

**Snap threshold (R7-P0-2: uses the bound in force on BOTH sides of the snap).**
`ε_snap = min(J_⋆·h²/4, 0.005 m/s)` and `ε_q = ε_snap/4`, where `J_⋆ = min(J_eff^before, J_eff^after)`:
- `J_eff^before = 2·g_max(p_current)·L_tot^max / h` is the bound of the segment that ends at the snap.
- `J_eff^after = 2·g_max(p_after)·L_tot / h` is the bound of the segment that starts at the snap.
  - `p_after` is the **pending** pole **whenever one is pending**, whatever kind of snap fires; otherwise the current pole.
  - `L_tot` is the **current** one; every snap resets `L_tot^max` to it.
- **Atomic snap (R8-1).** At most one snap fires per tick, and it applies every eligible change together: the pending pole (if any) is adopted, `L_tot^max` resets to the current `L_tot`, and if `v_r(n) = 0` it is also the settle. Every snap kind has the same target `v_r(n)`, so combining them changes nothing in the proof below; it only guarantees that the after-segment really is `(p_after, L_tot)`, with no second snap on the next tick.
- A **pure** settle (no pending `p` and `L_tot^max = L_tot`) changes neither, so `J_⋆ = J_eff`. A settle that also adopts a pending `p` and/or resets `L_tot^max` (Atomic snap, R8-1) uses the general before/after formulas above (R9).
- The fixed 5 mm/s cap matters (R5). With strong quadratic drag, `L_drag` at `V_cap` can make `J_eff·h²/4` large; at FPV and 30 Hz it is several m/s, which would be a visible snap.
- Since `ε_q ≤ J_⋆·h²/16`, every inequality below holds against **both** bounds.

**One snap proof, covering settle, re-baseline and the pole switch-over (R6-P0-2).**
- **Snap tick.** The snap replaces the smoother update with `y1 = v = v_r(n)` (settle: `v_r(n) = 0`). So
  `|Δv_snap| = |v_r(n) − v_{n−1}| ≤ |v_r(n) − y1_{n−1}| + |y1_{n−1} − v_{n−1}| ≤ 2·ε_q`, and
  `|Δ²v| ≤ |Δv_snap| + |Δv_{n−1}| ≤ 3·ε_q ≤ (3/16)·J_⋆·h²`, within both the before and the after bound.
- **Next tick.** The smoother starts from the exact steady state with the **after** pole, so
  `Δv_{n+1} = g_0^after·Δv_r(n+1)`, with `g_0^after = (1−p_after)² ≤ g_max(p_after)`, and `|Δv_r(n+1)| ≤ L_tot·h` (current limits).
  Therefore `|Δv_{n+1}| ≤ J_eff^after·h²/2`, and `|Δ²v| ≤ J_eff^after·h²/2 + 2·ε_q ≤ (5/8)·J_eff^after·h²`.
- **Later ticks** are covered by Lemma 3 from the new steady point.
- **Result:** every snap stays within `J_eff`, with no exemption. rev 8's "≤ J_eff/2" claim used per-condition `ε_snap` thresholds, which actually allow `4·ε_snap = J_eff·h²`; R6 caught this, and the thresholds above fix it.

**Settle needs an exactly zero reference (R6-P0-1).** Stage 1 reaches `v_r = 0` exactly, through the clamp at 0 or
the Drift snap, before settle may fire. The omitted distance is then only the zero-input tail of `(y1_{n−1}, v_{n−1})`:
`|Σ_{m≥n} v_m·h| ≤ h·(|v_{n−1}| + |y1_{n−1}|)·p/(1−p) = (|v_{n−1}| + |y1_{n−1}|)·t_j/2`.
- Steady-near with `v_r = 0` gives `|y1| ≤ ε_q`, and therefore `|v| ≤ |v − y1| + |y1| ≤ 2·ε_q`.
- So **`0 ≤ ε_trunc ≤ 1.5·ε_q·t_j ≤ 0.001875·t_j` m**. That is ≤ 0.375 mm for every shipped preset (t_j ≤ 0.2 s).
- This corrects rev 9's `ε_q·t_j`, which wrongly assumed `|v| ≤ ε_q` (R7-P0-1).

Both are within `J_eff`, so no exemption is needed. The rev 5 zero-passage snap is deleted: reversals pass through
zero inside the smoother.

**B5 (kept as a sanity cap only).** A seeded speed is capped at `V_cap`. Lemma 1 no longer depends on it, because drag saturation bounds the drag step for any speed (R4-P0-3).

**Deleted:** K, B1, B2, B4 and V1/a_cap, the guard, zero passage, feed-forward, and every compile iteration.
Nothing in the model can violate a bound, so `compile()` needs no clamps beyond input validation (§9).

### 6.5 What the model guarantees (each one is a test)
- **G1** On every tick of a continuous run: acceleration `|Δv|/h ≤ L_tot^max` and jerk `|Δ²v|/h² ≤ J_eff`. Both reduce to the current `L_tot` without a recent coefficient change. They are measured on the **camera velocity**, every source included. A *seed* is a re-initialisation at the observed velocity with acceleration 0, and is the only point where the run restarts.
- **G2** Lemma 5, **per channel** (H and Z when horizon-locked; F in Free-fly). During a *stop* (zero target), no channel's reference reverses, whatever the yaw or stick does (proven). The 3-D direction may bend between channels. The camera never reverses when the pre-stop history is one-directional (proven); otherwise any reversal is bounded by the geometric tail `W_m`. Partial brakes and steering (`s* > 0`) are outside G2.
- **G3** The state is always finite.
- **G4** Frame-grouping invariance (§7).
- **G5** No overshoot of a held target (Lemma 4).

---

## 7. Fixed step + interpolation — and what "deterministic" means

```
advance(frame_dt, input):                        // public: frame_dt clamped to [0, 0.25] s (policy below)
    acc += clamp(frame_dt, 0, 0.25)
    steps = 0
    while acc + 1e-9 ≥ h ∧ steps < 512:  prev = cur; cur = step(cur, input, h); acc −= h; ++steps
    α = clamp(acc/h, 0, 1)
    render: p = lerp(prev.p, cur.p, α); ψ = prev.ψ + wrap(cur.ψ − prev.ψ)·α; θ, φ lerped; q = R_z R_y R_x
advanceRaw(dt, input)                            // test-only entry: no dt clamp (backlog tests)
```

- **Interpolation.** The Euler state is interpolated, not the quaternion. The horizon therefore stays exactly level between ticks, and no sign-flip handling is needed. Render latency is ≤ one tick `h` (8.3 ms at 120 Hz).
- **Hitch policy.** This is an explicit change from rev 3. Frame dt is clamped to 0.25 s, matching the operator (F12). Time beyond that is **discarded** on purpose, so a 3-second load stall cannot fling a free-flying camera.
- **Backlog cap.** With Hz ≤ 240, a clamped frame is at most 60 ticks, so the 512-step cap is purely defensive. If the cap is ever hit (only via `advanceRaw` or a future change), the backlog stays in `acc` and drains on later frames, as the operator does.
- **Determinism — the honest claim (P1-6):**
  - **Exact:** for an identical **per-tick input sequence**, the final state is bit-identical however the ticks are grouped into frames (T1).
  - **Bounded, not exact:** live input is sampled once per frame and held, so the same hands at 30 vs 144 fps produce different tick-level input at stick transitions. The divergence is bounded by the input change × one frame duration. That is what makes it feel the same, not *be* identical (T2).
  - **Tested:** timestamped input, jitter, interpolation latency and backlog recovery (T1–T4).

---

## 8. Triggers — unified with the existing XInput system (P0-1)

### 8.1 One mode
`JoystickXInputTriggerMode` remains the only persisted trigger mode. D-pad Down toggles it with the existing code
(`ljc:2828-2834`). Only the **hint text** becomes model-aware.

| Mode | Classic (unchanged) | Drone |
|---|---|---|
| 0 ROLL | Legacy DirectInput-axis-2 roll, or shaped XInput roll if `RollShaped` | Same roll source, mapped to a roll **rate** `φ̇* = roll·Φ_rate`. No throttle and no brake: Assist cruises at `V_c`, and Pure falls back to Assist |
| 1 THROTTLE ("Clutch") | `Max^RT·Min^LT` multiplier on translation **and** rotation (F16) | **RT = throttle** `thr = shape(RT)`, **LT = brake** `brk = shape(LT)` (speed only; rotation not scaled) |

### 8.2 Sampling
Drone uses a new pure helper, `ALXInput::droneTriggers(lt, rt, dz, curve, thr&, brk&)`, in `alxinputmath.h`. It
calls the existing `shape()` on each trigger independently.

The runtime call sits beside `computeTriggerOutputs` with the **same gate**: `mXITakeover ∧ xInputAssociated() ∧
fresh(...)`. A stale, unfocused or unassociated sample returns `thr = brk = 0`.

DirectInput axis 2 is never split. Under takeover it already reads 0 through `getFlycamAxis`. Release-pending works
unchanged: a THROTTLE→ROLL switch keeps axis 2 suppressed until it returns to neutral (`ljc:2765-2772`).

### 8.3 Switching mid-flight
- No extra ramps are needed. A mode switch, a takeover loss or a pad disconnect changes `thr`/`brk` in one step. That only moves stage-1 targets and limits, so the camera's acceleration and jerk stay within `L_tot` and `J_eff` (§6.4).
- Roll input switching off decays `φ̇` through `α_roll`. φ is kept.
- Tests T10 and T25.

### 8.4 Feedback and non-XInput pads
- **Hint.** `xInputHint` reads "Triggers: THROTTLE + BRAKE (Drone)" or "Triggers: ROLL (Drone)" when Drone is active. Classic text is unchanged. `RUMBLE_MODE_TICK` is unchanged.
- **Status.** `getXInputStatus()` reports the Drone meaning in the same style.
- **Non-XInput pads, or XInput disabled.** Drone works with the sticks, and the triggers do whatever the DirectInput mapping does today (roll if mapped). Drone then runs in Assist at cruise with no brake, and the floater says so. Separate DirectInput trigger axes are deferred (§2.2).

---

## 9. Human terms → coefficients: CLOSED FORM (rev 6; P0-2, P0-5, R3-P0-3)

The UI shows only the left column. `compile()` runs as follows:
1. Validate the inputs. A non-finite or out-of-range value keeps the previous valid value and the clamp is logged. Every time is clamped to `≥ 4·h` and every speed to `> 0`; Dolly's `V_climb = 0` is allowed and skips the climb terms.
2. Evaluate the formulas below **once**.

There is **no iteration, no simulation and no stability bound to repair**. Every stage-1 step is a clamp or an exact
flow (§6.4), so no coefficient combination can be unstable.

| User sees | Exact meaning on the **mass stage** (stage 1, sampled at ticks) | Coefficient |
|---|---|---|
| **Cruise / Top speed** `V_c`, `V_t` (m/s) | direct (`V_t ≥ V_c`) | — |
| **Time to top speed** `t_top` | From rest, \|d\| = 1, thr = 1: the reference follows `V_t·tanh(A_max·t/V_t)` exactly, so it reaches 0.9·V_t at `t_top` | **`A_max = V_t·atanh(0.9)/t_top`** (= 1.472·V_t/t_top) |
| **Hold stop time** `t_hold` | From V_c, release, Hold: linear decel, 0.1·V_c at `t_hold` | **`A_dec = 0.9·V_c/t_hold`** |
| **Stopping distance** `d_stop` | From steady V_t, thr = 1, brk = 1: the **camera's** total distance until settle. A unity-DC smoother started from a steady nonzero state adds **exactly `V_t·t_j`** (its mean delay × the velocity drop; R4-P0-1): camera distance = `d(D) + V_t·t_j − ε_trunc`, where `ε_trunc` is the tail the settle snap cuts off. Settle requires `v_r = 0` exactly (R6-P0-1), so the omitted distance is only the zero-input tail of `(y1, v)`: **`0 ≤ ε_trunc ≤ 1.5·ε_q·t_j ≤ 0.001875·t_j` m** (§6.4, R7-P0-1). The guarantee is **weakened** accordingly: the camera distance is `d_stop − ε_trunc`, i.e. `d_stop` within `max(1 mm, 1.5·ε_q·t_j)`, which is 1 mm for every shipped preset (t_j ≤ 0.2 s) | **`D_brk`** = the unique root of `d(D) = d_stop − V_t·t_j`, with `d(D) = h·(m·V_t − D·h·m(m+1)/2)`, `m = ⌊V_t/(D·h)⌋`. `d` is explicit and monotone in D; solved by bisection (≤ 60 steps, no simulation). Seed `D₀ = V_t²/(2(d_stop − V_t·t_j))` |
| **Coast time** `t_coast` | From V_c, release, Drift: the exact drag flow reaches 0.1·V_c at `t_coast` | With `r = (1 − float)·4/V_t`: `E = 0.1·(1 + r·V_c)/(1 + 0.1·r·V_c)` (independent of c1) ⇒ **`c1 = −ln E / t_coast`**, **`c2 = r·c1`** |
| **High-speed float** (0–1) | sets r above (1 = linear drag only) | — |
| **Climb speed / time** `V_climb`, `t_climb` | From rest, u = 1: linear, 0.9·V_climb at `t_climb` | **`A_climb = 0.9·V_climb/t_climb`** |
| **Onset softness** `t_j` | Stage 2's mean delay: `Σ k·g_k·h = 2p·h/(1−p) = t_j` | **`p = t_j/(t_j + 2h)`** |
| **Grip** (0–1, min 0.05) | `G`, direct; the floor keeps sideways momentum from persisting forever | — |
| **Turn rate** (°/s), **Turn weight** (0–1) | `ω0` direct; `s_ref = V_t·(1.05 − weight)`; `α_yaw = ω0/0.15 s` | — |
| **Tilt rate** (°/s) | `Θ_rate` direct; `α_g = Θ_rate/0.15 s` | — |
| Roll rate | `Φ_rate = |FlycamAxisScale3|` rad/s (Classic's roll magnitude); `α_roll = Φ_rate/0.15 s` | — |
| *(readout)* max acceleration / max jerk | §6.4 Lemmas 2–3 | **`L_tot`, `J_eff = 2·g_max·L_tot/h`**, shown in "Advanced" |

**What the UI times mean (honest labelling).**
- The times above are **exact for the mass stage**.
- The camera additionally carries the onset smoothing, which delays any long linear segment by exactly `t_j`, its mean delay. The UI therefore shows, for example, "Time to top speed 2.0 s (+0.10 s onset)".
- Tests check two things (T5–T9):
  - the mass-stage crossing (sub-tick interpolated, §7) is within **0.5 %**;
  - the camera crossing equals `t + t_j` within `2 % · t + h`. The residual is the smoother's transient at the start of a segment.
- The rev 5 sub-tick interpolation rule for threshold crossings is kept: crossing times are interpolated linearly between the bracketing ticks.

**Cost and threading.**
- `compile()` is O(1) plus one ≤ 60-step scalar bisection, in microseconds.
- It runs off the settings commit signal, debounced 150 ms in idle, never in `draw()` (CLAUDE.md UI rule).
- **`D_brk` bracket, proven.** `d(D)` is a right-endpoint Riemann sum of a decreasing line, so `V_t²/(2D) − V_t·h ≤ d(D) ≤ V_t²/(2D)`. With `d* = d_stop − V_t·t_j`, the root lies in `[V_t²/(2(d* + V_t·h)), V_t²/(2·d*)]`.
  - Validation clamps `d_stop ≥ V_t·(t_j + 2h)`, so `d* ≥ 2·V_t·h > 0`. The UI shows the floor ("at least V_t·t_j: the onset softness itself needs that much room").
  - Bisection on that bracket is monotone and terminates (T22).

---

## 10. Presets (proposals — tune in-world)

| Preset | Cruise | Top | t_top | d_stop | t_hold | t_coast | float | t_j | Grip | Turn rate | Turn wt | Tilt rate | Climb | t_climb | Release | Frames |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| **Cine** | 2.0 | 6 | 3.0 | 8 | 1.2 | 6 | 0.7 | 0.15 | 0.35 | 45 | 0.7 | 30 | 1.0 | 1.0 | Hold | Drone |
| **Normal** | 4.0 | 12 | 2.0 | 10 | 0.8 | 4 | 0.5 | 0.10 | 0.6 | 70 | 0.5 | 50 | 2.0 | 0.8 | Hold | Drone |
| **Sport** | 8.0 | 30 | 1.6 | 18 | 0.6 | 3 | 0.3 | 0.07 | 0.8 | 110 | 0.4 | 80 | 4.0 | 0.6 | Drift | Drone |
| **Dolly** | 1.0 | 3 | 2.5 | 3 | 1.0 | 5 | 0.8 | 0.20 | 0.9 | 25 | 0.2 | 20 | 0 (locked) | — | Hold | Drone |
| **FPV** | 10.0 | 40 | 1.0 | 25 | 0.5 | 2 | 0.2 | 0.05 | 0.5 | 200 | 0.3 | 150 | 6.0 | 0.4 | Drift | Free-fly |

**Derived coefficients** (closed form, §9; at 120 Hz). Every preset is feasible by construction: no bound exists that a combination could violate.

| | Cine | Normal | Sport | Dolly | FPV |
|---|---|---|---|---|---|
| `A_max` (m/s²) | 2.94 | 8.83 | 27.6 | 1.77 | 58.9 |
| `A_dec` (m/s²) | 1.50 | 4.50 | 12.0 | 0.90 | 18.0 |
| `D_brk` ≈ V_t²/(2(d_stop − V_t·t_j)) (m/s²) | 2.54 | 8.18 | 28.3 | 1.88 | 34.8 |
| `V_t·t_j` share of `d_stop` (m) | 0.90 of 8 | 1.20 of 10 | 2.10 of 18 | 0.60 of 3 | 2.00 of 25 |
| `A_climb` (m/s²) | 0.90 | 2.25 | 6.00 | — (locked) | 13.5 |
| `p` at 120 Hz | 0.900 | 0.857 | 0.808 | 0.923 | 0.750 |

- Dolly's climb is locked (`V_climb = 0`), so its climb terms are skipped.
- The `D_brk` values shown are the continuous seeds; the exact discrete root is within the §9 bracket.

**All five presets ship (U2).** Sport has **no bank in v1**; bank is deferred (§2.2).

**Behaviour on edit and switch.**
- Any user edit creates "Custom (from X)".
- A preset switch applies stage-1 coefficients immediately. Cross-fade is deferred. Acceleration and jerk stay bounded by `L_tot^max` and `J_eff` over the smoother memory (§6.4). Drag saturation keeps Lemma 1 true even when the new preset's `V_cap` is below the current speed. A changed onset `t_j` waits for the first steady tick (§6.4). Velocity is **never** snapped down.

**Code tables.** Any positional preset table in code is verified by **counting rows and fields** against this table
after every edit (CLAUDE.md rule 6).

---

## 11. Integration details

### 11.1 Hook
See §4. The edits to `moveFlycam` are limited to:
- the hoisted anchor resolve;
- the edge block;
- the Drone override block after the Classic integrator;
- the operator-feed branch;
- end-of-frame bookkeeping.

The Classic integrator, Orbit maths and camera/FOV write are unchanged hunks, apart from WP0's mechanical extraction.

### 11.2 Camera ownership (P0-6, R2-P0-1)

**The clock: an idle camera-dispatch serial, not `gFrameCount`.**
- Camera arbitration runs once per `idle()` (`llappviewer.cpp:5594-5619`). `gFrameCount` advances only on a successful `display()` (`llviewerdisplay.cpp:604`).
  - Minimized or hidden rendering returns early (`llviewerdisplay.cpp:473-479`), which gives false negatives.
  - Multi-pass snapshots call `display()` once per tile (`llviewerwindow.cpp:5794`), which gives false gaps.
- rev 5 therefore adds, in a tiny header `alcameradispatch.h`:
  - `U64 gCameraDispatchSerial`, incremented **once per `idle()`** immediately before the arbitration chain;
  - `ECameraOwner gCameraDispatchOwner`, set by the branch taken: `PILOT`, `RECORDER`, `PATH`, `CINE`, `FLYCAM` or `AGENT`.

  These are two lines in the existing chain. `PILOT`, `RECORDER`, `PATH` and `CINE` cover the Director Switcher and LLDirectorCast, since both drive `CINE`.
- **Writer — tagged calls only (R3-P1-5).** The serial and foreign bookkeeping runs **only** inside the tagged arbitration call, i.e. when `gCameraDispatchOwner == FLYCAM` and this serial has not been consumed yet. When that call completes its camera write (any model, including Orbit-owner frames), it records `lastWrittenSerial = gCameraDispatchSerial` and clears `foreignSinceFlycamWrite`.
- **Toggle-time call is reset-only.** `toggleFlycam → moveFlycam(true)` (`ljc:2266`) runs outside `idle()`. It reseeds Drone (Stop) and the operator as today, and sets `resetPending`. It does **not** touch the serial, the foreign flag or the sampler.
- **Consuming the reset.** The next tagged FLYCAM dispatch sees `resetPending`. It treats the frame as a fresh start: `lastWrittenSerial = S − 1`, foreign cleared, momentum **Stop**. A flycam toggle therefore never inherits the agent camera's motion through Keep, and never misclassifies the agent-camera frames before it as a regain.
- **Sampler.** After the chain, immediately before `LLFlycamRecorder::instance().onIdleFrame()` (`llappviewer.cpp:5631`), it records the final camera for this dispatch: `{serial, owner, dt = gFrameIntervalSeconds, origin_global (LLVector3d, via gAgent.getPosGlobalFromAgent), axes, flycam_wrote, frameCountAtSample = gFrameCount}`. Positions are global, so agent-origin shifts cannot corrupt secants (§11.9).
  - It keeps a ring of the last 4 samples.
  - It keeps a sticky `foreignSinceFlycamWrite` flag: set when `owner ≠ FLYCAM`, cleared by the writer.
  - **Presented.** A sample is PRESENTED when `gFrameCount` has advanced by the time the *next* sample (or the regain check) is taken, i.e. at least one `display()` completed with that camera. This is the only remaining use of `gFrameCount`.

**Gap classification** at Drone frame start, for serial S. It uses owner tags, not pose comparison, so there are no false positives:

```
gap     = (S != lastWrittenSerial + 1)
foreign = foreignSinceFlycamWrite                 // some other owner dispatched since our last write
if gap ∧ ¬foreign  → SELF gap    (moveFlycam was dispatched but returned early: focus loss / alt-tab / minimize)
if foreign         → FOREIGN gap (Recorder / Path Camera / Director-Cine / Pilot / agent camera had the camera)
else               → no gap
```

A foreign owner cannot dispatch without moving the serial past `lastWrittenSerial + 1`, so `foreign` implies `gap`.
Snapshots never call `idle()`, so they create no gap.

**Regain:**

| | SELF gap | FOREIGN gap |
|---|---|---|
| Pose | Drone's own last rendered **unshaken** pose; nobody else touched the camera | The **final rendered** camera, i.e. the post-operator `LLViewerCamera` (= sample S−1) — U3: continue where the owner left it |
| Operator | **Not reset** (R2-P1-6). Its state and its rendered offset carry on, so the post-operator image is continuous | **Reset**, so the offset is 0 on the resume frame and the rendered image equals the seeded pose |
| Momentum | per §11.2.1 | per §11.2.1 |
| Common | `sFlycamPosition/Rotation = pose; acc = 0`; log `DRONE OWNER-REGAIN <self\|foreign> momentum=…` | same |

The seeded pose always reconstructs the source pose exactly (§11.5). Frame S therefore starts on the
pixel-identical rendered image, in **either** momentum mode.

**Reset and teleport** keep the existing reset path (F9). That path also calls `drone.reset(camera pose)` and
`LLCameraOperator::reset()`, and it is **always Stop**.

**Classic** keeps today's regain behaviour: it snaps back to the stale `sFlycamPosition`. That is not changed (§17).

The existing `CameraShakeApplyActive` operator reset (`ljc:2190-2197`) still keys on `gFrameCount`. That is
pre-existing Classic behaviour and is untouched.

#### 11.2.1 `FlycamDroneResumeMomentum` (U4): Stop (0, default) / Keep (1)

**Stop.** `v_r = y1 = v = 0`, `ω = θ̇ = φ̇ = 0`. In Hold, Drone then sits still until the stick moves.
- **SELF gap:** the operator is not reset. It sees the velocity drop to 0 and settles through its own dynamics, with no pose pop (T42).

**Keep after a SELF gap** (alt-tab, focus loss, minimize):
- **Whose velocity:** Drone's own.
- **Which state:** the state Drone was *rendering* at its last write. That is `prev`/`cur` interpolated at the last α, for position, `v_r`, `y1`, `v`, every angle and rate, and the latch.
- **Effect:** it becomes `prev = cur`, a true pause:
  - The gap's wall time is **not** integrated (`acc = 0`).
  - All stage states are kept, so the camera continues as one run and G1/G2 hold across the gap (§6.5).
  - The operator is not reset, so the rendered image resumes exactly.

**Keep after a FOREIGN hand-back** (R2-P1-5: no two-frame secant):
- **Whose velocity:** the **other owner's** on-screen camera motion.
- **Samples used:** the last three samples, `S−3`, `S−2` and `S−1`.
- **Estimate:**

  ```
  v_a = (C_{S−2} − C_{S−3}) / dt_{S−2} ;  v_b = (C_{S−1} − C_{S−2}) / dt_{S−1}        global positions → m/s
  a_est = (v_b − v_a) / ((dt_{S−1} + dt_{S−2})/2)
  v_keep = v_b + a_est·dt_{S−1}/2                     // secant midpoint → end of the owner's last presented frame
  Ω_keep from the quaternion delta C_{S−2}→C_{S−1} (§11.5.1) → ω, θ̇, φ̇ (each clamped to its rate limit)
  v_r = y1 = v = v_keep                               // smoother starts at steady state: acceleration restarts at 0 (§6.3 seeds)
  ```

- **Mapping:** horizon-locked frames split `v_keep` into `v_h` and `v_z`; Free-fly keeps it as 3-D.
- **Validation:** every check must pass. **Any failure falls back to Stop**, with the reason logged.

  | Check | Rejects | Reason |
  |---|---|---|
  | `S−3`, `S−2`, `S−1` exist with consecutive serials ending at `S−1` | stale history | `stale` |
  | All three have the **same** owner, and that owner is not `FLYCAM` | a window straddling an owner change | `straddle` |
  | All three are **PRESENTED** | frames the user never saw (minimized or hidden) | `unpresented` |
  | Each `dt` is in `(0.0005, 0.25]`, and every value is finite | hitch / garbage | `dt` / `nonfinite` |
  | `|v_b| ≤ V_cap` (build-scaled) and `|Ω_b| ≤ 2·ω0` | **camera cuts** (rejected, never clamped) | `cut` |
  | **Acceleration consistency:** `|a_est| ≤ A_keep = 1.5·L_tot` (build-scaled), and the angular equivalent `≤ 2·α_yaw` | non-smooth motion: a cut spread over frames, jitter, or an origin shift | `inconsistent` |

- **What Keep guarantees (restated with its tolerance):**
  - **Position and orientation:** exact. The resume frame starts on the owner's last presented image.
  - **Velocity:** continuous to within `ε_v = A_keep·dt_{S−1}`. This is the worst-case end-point error of the second-order estimate under the consistency bound. At 60 fps with Normal it is ≈ 0.25 m/s.
  - **Higher order:** acceleration is **not** continuous. It restarts at 0 with the smoother at steady state, and is bounded from there by G1. That restart is the price of a provable start.
  - Without three consistent, presented samples there is no C¹ claim, and the result is Stop.
  - Explicit cut metadata from the Recorder, Path Camera or Director would be stronger, but those owners do not publish it today. Adding it is future work.
- **After resume:** Hold with no stick eases to rest through `A_dec` (no reversal: Lemma 5, one-directional history since the smoother starts at steady state). Drift glides on drag. A held stick blends to its target. Speeds between V_t and V_cap decay along the overspeed path.

**Not affected:** Orbit edges and Classic↔Drone edges are not gaps. They use the continuous history seed (§11.3, §11.4).

**Tests:** T30–T38 and T42–T44 (§16.1).

### 11.3 Orbit — one owner per frame (P0-7)
- **Who decides.** `orbit_owner` is computed **once** at the top, from the hoisted `resolveAnchor`. The Orbit block consumes that result and does not resolve again. `resolveAnchor` is `const` and pure-read (F21), and its inputs do not change inside `moveFlycam`, so Classic behaviour is equivalent.
- **Orbit-owner frames.** Drone does not advance: its accumulator is zeroed and its state frozen. The Classic shadow produces `sDelta` exactly as today, and the operator uses the Classic `update(sDelta)` feed. **Orbit behaves exactly as today.**
- **Rising edge (Drone → Orbit).** The pose entering the Orbit block is Drone's last rendered pose; the shadow increment for that frame is discarded. Orbit's own engage logic (`ljc:2083-2103`) recovers its state from that pose. Operator reset.
- **Falling edge (Orbit → Drone).** Drone seeds from the continuous pose history (§11.4 E1), so a camera riding a walking avatar hands over with its momentum, capped at V_cap. Operator reset.

### 11.4 Classic ↔ Drone transitions (P1-1)
**Rule:** the frame that observes an edge is rendered by the **outgoing** owner. The incoming owner seeds at the end
of that frame from its final pre-operator pose.

Every frame, in both models, read-only for Classic, `moveFlycam` records pose history: `P_{k−1}, q_{k−1}, P_k, q_k`
and `dt_k = gFrameIntervalSeconds` (the actual, unclamped value).

- **E1 Classic→Drone** (setting flip, Cursor3D turned off, Orbit falling edge):
  `drone.seedFromHistory(P_{k−1}, q_{k−1}, P_k, q_k, dt_k, prior)` sets:
  - `p = P_k`;
  - ψ/θ/φ from `q_k` (§11.5);
  - `v = (P_k − P_{k−1})/dt_k`, capped at V_cap;
  - ω, θ̇, φ̇ from the **quaternion delta** `q_{k−1}→q_k` (§11.5.1), never from Euler differences, each clamped to its rate limit;
  - `v_r = y1 = v = v_seed` (steady smoother, acceleration 0), `acc = 0`, `prev = cur`.

  Because v is measured from what was actually *rendered* over the actual dt, the BD [0.016, 0.033] clamp
  (and the per-frame-displacement nature of Classic) is accounted for automatically. Operator reset.
- **E2 Drone→Classic** (setting flip, or Cursor3D turned on; *not* the Orbit rising edge, see §11.3):
  1. Drone renders the edge frame.
  2. Then, **after** that frame's Classic feather update, `sDelta[0..2] = camera-local(v)·dt_k` and `sDelta[3..5] = (φ̇, θ̇, ω)` mapped to Classic's (roll, pitch, yaw) camera-local increments × dt_k.
  3. Classic's first feather step therefore acts on the seed on frame k+1. That step is Classic's own momentum decay, which is the intended behaviour. The feather never pre-distorts the seed.

  Classic moves by a per-frame displacement, so the inherited speed holds only while dt stays close to dt_k.
  Drone state is then marked stale. Operator reset.
- **Tests:**
  - T26–T28: BD and non-BD, at 30, 60 and 144 fps; at 144 fps the BD clamp is active.
  - Velocity continuous ±2 %, no position pop beyond v·dt, rotation rate continuous.

### 11.5 Seeding policy (P1-2)
From `q` (normalised): `f, l, u` = X/Y/Z rows of `LLMatrix3(q)`; `c = |(f_x, f_y)|`.

```
ψ = c ≥ 1e-4 ? atan2(f_y, f_x)
  : (prior valid ? ψ_prior : atan2(l_y, l_x) − 90°)            // retain the prior yaw at the singularity
θ = atan2(f_z, c)  ∈ [−90°, 90°]                               // never clamped at seed (no snap)
φ = signed angle about f from left(R_z(ψ)R_y(θ)) to l          // absorbs whatever ψ/θ did not → EXACT reconstruction
q_seed = R_z(ψ)R_y(θ)R_x(φ); canonical sign: dot(q_seed, q) ≥ 0 (else negate)
θ_lim = max(89°, |θ|): stick input may not increase |θ| beyond θ_lim; θ_lim falls to 89° once |θ| ≤ 89°
```

- ZYX can represent any rotation, so reconstruction is exact for every input, including straight up/down and over-the-top cameras. In the over-the-top case φ ≈ 180°, which is kept and not auto-righted unless AutoLeveling is on.
- **Tests:**
  - T19: random quaternions, with and without roll, reconstruct to < 0.05°.
  - T20: a ±90° continuity sweep from 85° through 90° to 95° (over the top), seeded each frame. The rendered pose equals the camera pose at every step, and 1 s of zero input afterwards leaves it unchanged.
  - T21: θ_lim behaviour.
- The units and sign of LL pitch are mapped by the implementer; the test pins the convention.

#### 11.5.1 Seeded rates from the quaternion delta (R2-P1-7)
Near ±90°, the Euler ψ and φ can each jump by 180° between two nearly identical orientations, so angle differences
are not rates. **Every** seeded rate comes from the rotation itself:
- the Classic/Orbit history seed (E1);
- the foreign Keep (§11.2.1).

```
dq = q_k · conj(q_{k−1});  if dq.w < 0: dq = −dq             // shortest arc (LL multiply order pinned by T43)
Ω  = axis(dq) · angle(dq) / dt                               // world-frame angular velocity, chart-free
ZYX rates satisfy Ω = ψ̇·ẑ + θ̇·ĝ + φ̇·f̂, with ĝ = R_z(ψ_k)·ŷ (gimbal axis) and f̂ = camera forward:
θ̇ = Ω·ĝ
if |θ_k| ≤ 85°:  φ̇ = (Ω·ĥ) / cos θ_k   (ĥ = horizontal forward);   ψ̇ = Ω·ẑ − φ̇·sin θ_k
else (near vertical: ẑ and f̂ nearly coincide):  φ̇ = 0,  ψ̇ = Ω·ẑ      // shared component goes to yaw only
ω = ψ̇; clamp ω, θ̇, φ̇ to ω0, Θ_rate, Φ_rate
```

- **No 180° jump.** These rates depend only on `dq` and the seeded frame, so they cannot jump by 180°.
- **Continuity at the 85° switch.** The near-vertical rule is continuous to within the `φ̇·sin θ` term at 85°. That term is bounded by `Φ_rate`, and T43 measures it.
- **Tests:** T43 (constant Ω through ±90°, rates continuous) and T44 (rates match Ω's projections away from vertical).

### 11.6 3D cursor (P0-8)
`drone_on` requires `!m3DCursor`. With `Cursor3D` on:
- Classic runs, using the E2 hand-off if Drone was active.
- The Flight-model control is disabled with the tooltip "Drone needs relative input; 3D-cursor (absolute) mode is on".
- Log `DRONE INACTIVE 3d-cursor` is written once.

### 11.7 Handheld operator (P1-3)
| Frame | Feed |
|---|---|
| Drone frames | `LLCameraOperator::instance().updateFromPose(dt, sFlycamPosition, sFlycamRotation)`, using the **interpolated, unshaken** pose |
| Classic frames and Orbit-owner frames | Unchanged `update(sDelta)` |

`updateFromPose` handles both Legacy (frame-to-frame) and fixed-tick locomotion (F22).

These events call `LLCameraOperator::reset()`:
- every Drone seed;
- every model edge;
- every Orbit edge;
- every **FOREIGN** regain.

The operator therefore never mixes `update()` and `updateFromPose()` histories, and never sees a seed jump as velocity.

**The one exception is a SELF gap** (R2-P1-6). There:
- the pose and history are unchanged;
- the operator is **not** reset;
- its rendered offset continues, so the image does not pop;
- the first `updateFromPose` after the gap sees the same pose as before, so it gets zero velocity under Stop, or Drone's continuing motion under Keep.

### 11.8 Build mode
`FlycamBuildModeScale = s` is applied by **dimensional scaling**:
- speeds and accelerations are multiplied by s, so `L_tot`, `L_drag`, `V_cap` and `J_eff` scale by s. While `s_eff` eases, Lemma 3 uses `L_tot^max` over the memory (§6.4). A shrinking `V_cap` is harmless because drag is saturated (R4-P0-3);
- `c2` is divided by s;
- `c1` and the smoother pole `p` are unchanged.

The step equations are homogeneous, so every calibrated *time* is preserved exactly and distances scale by s
(T23). Entering or leaving build mode eases `s_eff` with τ = 0.3 s. Velocity is not rescaled; stage 1 carries it.

### 11.9 Agent-origin shifts (R3-P0-4)
`LLAgent::setRegion` changes the agent coordinate frame when the avatar changes region (`llagent.cpp:1087-1111`). It translates the agent and calls `LLViewerCamera::setOrigin(camera − delta)` **directly**, outside the dispatch ownership. Drone handles it so that no stored position is ever expressed in a stale frame:

- **Origin tag.** Drone stores `originTag = gAgent.getPosGlobalFromAgent(LLVector3::zero)` (`LLVector3d`) alongside every agent-space state it owns: `p`, `prev.p`, `cur.p`, the last rendered pose, and `sFlycamPosition` while Drone owns it.
- **Re-express before anything else.** At the start of every Drone frame, and at every regain or seed, before gap classification:

  ```n  o = gAgent.getPosGlobalFromAgent(LLVector3::zero)
  if o != originTag:  Δ = LLVector3(originTag − o)       // computed in F64, then narrowed
                      p += Δ; prev.p += Δ; cur.p += Δ; lastRenderedPose.p += Δ; sFlycamPosition += Δ
                      originTag = o;  log DRONE ORIGIN-SHIFT |Δ|
  ```n
  Velocities and filter states are translation-invariant and untouched. The shift is exact: the camera stays at the same **global** point, as `setRegion` intends.
- **Global history and samples.** The pose history (§11.4) and the final-camera sampler (§11.2) store **global** positions (`LLVector3d`). Secants are therefore immune to shifts between samples, so a shift can never fake a Keep velocity. Positions are converted back with `gAgent.getPosAgentFromGlobal` only when used as a pose.
- **During a gap.** A shift during a SELF gap is applied by the same check before the SELF reseed. A FOREIGN regain seeds from `LLViewerCamera`, which `setRegion` already moved into the new frame.
- **Classic is unchanged.** Its `sFlycamPosition` is not re-expressed on a region change; that is pre-existing behaviour (F25), out of scope, and recorded here.
- **Tests:** T47.

---

## 12. Controls — Xbox (Drone)

| Control | Drone | Note |
|---|---|---|
| Left stick | Move (forward/back, strafe), horizon-locked | Sign parity with the active Classic path (§5.1) |
| Right stick X / Y | Yaw / gimbal tilt | |
| RT / LT | **THROTTLE mode:** throttle / brake. **ROLL mode:** roll (existing source) | Mode via the existing `JoystickXInputTriggerMode` |
| **D-pad Down** | Toggle ROLL ↔ THROTTLE (**existing**; toast + `RUMBLE_MODE_TICK`) | Unchanged; Drone-aware hint text only |
| B / A | Ascend / descend (existing Jump = 1 / Crouch = 0) | Read in Drone on both BD and non-BD |
| LB / RB | Zoom (existing; Classic shadow) | Unchanged |
| Menu | Flycam toggle / zoom default (existing) | Unchanged |
| **D-pad Up** | **Classic ↔ Drone** (action 4) | U1 default, applied only if the slot was None (§12.1) |
| **D-pad Left** | **Hold ↔ Drift** (action 5) | U1 default, applied only if the slot was None |
| **D-pad Right** | **Recentre gimbal** (action 6) | U1 default, applied only if the slot was None |

### 12.1 D-pad action enum — extension and None-only defaulting (U1)

**Appending, never renumbering.** The persisted values of `JoystickXInputDpadLeft/Right/Up` are numbers, so the new
actions are **appended**. Values 0–3 keep their meaning exactly. The floater combos carry explicit `value`s, not
indices. Each of the three combos then has 7 entries, which is verified by counting (rule 6).

| Value | Action | Drone active | Classic |
|---|---|---|---|
| 0 | None | unchanged | unchanged |
| 1 / 2 | Light cue Go / Back | unchanged | unchanged |
| 3 | Level horizon | Drone: φ = 0 (§6.3 step 9) | unchanged |
| **4** | **Flight model Classic ↔ Drone** | → Classic; hint "Flight: Classic" + `RUMBLE_MODE_TICK` | → Drone; hint "Flight: Drone" + `RUMBLE_MODE_TICK`. With Cursor3D on: hint "Drone unavailable: 3D cursor" and no change |
| **5** | **Hold ↔ Drift** (`FlycamDroneRelease`) | hint "Release: Hold/Drift" | hint "Drone not active", no change |
| **6** | **Recentre gimbal**: sets the `recentre` edge, which arms latch `R` in `step()` (§6.3 step 9). θ eases to 0 at up to `Θ_rate`, slowing over the last 10°. It completes at \|θ\| < 0.1°, and any tilt-stick input cancels it. `R` is runtime-only and is cleared by reset, reseed, regain and a model switch | hint "Gimbal recentring"; a repeat press while active is a no-op | hint "Drone not active", no change |
| other | (out of range) | no action, as today | no action, as today |

**None-only defaulting (versioned migration, R2-P2-10).**
- **Settings defaults.** The `settings.xml` defaults of the three D-pad keys **stay 0**.
- **New key.** `JoystickXInputDpadMigrationVersion` (S32, default 0, `Persist 1`). It replaces rev 4.1's BOOL flag.
- **Migration.** Runs after settings load (in `LLViewerJoystick` init):

  ```
  const S32 DPAD_MIGRATION_DRONE = 1
  v = JoystickXInputDpadMigrationVersion
  if v < DPAD_MIGRATION_DRONE:
      for (slot, action) in { (Up, 4), (Left, 5), (Right, 6) }:
          if JoystickXInputDpad<slot> == 0:  set it to action     // None → Drone default
          // any other value (1–3 cue/level, or anything else) is NEVER touched
  JoystickXInputDpadMigrationVersion = max(v, DPAD_MIGRATION_DRONE)   // never decreases; later migrations append 2, 3, …
  ```

- **Effect.**
  - A user who has already assigned cue Go/Back or Level horizon to a slot keeps it.
  - A user who later sets a slot back to None keeps None, because the version is ≥ 1 and the step never runs again.
  - **Known edge:** a user who had *deliberately* chosen None before this build gets the Drone default once. One combo change undoes it permanently.
- **Lost or restored version (documented behaviour).**
  - **Lost:** if `settings.xml` is deleted or reset, the D-pad keys also return to 0, so filling them is the correct fresh-profile behaviour.
  - **Restored from an older backup** (version 0 together with the old D-pad values): the fill re-runs on that restored profile. It is harmless, because it is still None-only and can never overwrite a cue or level assignment.
  - A version **higher** than this build knows (a newer build's profile) is left untouched, and nothing is filled.
- **"Xbox Defaults" button** (`setXboxDefaults`, `ljc:3171`): it applies the **same None-only fill** for the three slots, so it never clobbers cue or level assignments. It still does not touch `FlycamFlightModel`.
- **Tests:** T39–T41 (§16.3).

No new `JoystickButton*` settings exist. R3 (Mouselook) and View (Run toggle) are untouched.

Non-Xbox pads use the same model; the user maps axes in the existing floater (§8.4).

---

## 13. Settings — all in `settings.xml` (P1-4)

New keys go next to the Flycam/XInput block. All are `Persist 1`.

| Key | Type | Default | Range |
|---|---|---|---|
| `FlycamFlightModel` | S32 | 0 Classic | 0 Classic / 1 Drone |
| `FlycamDronePreset` | S32 | 1 Normal | 0 Cine, 1 Normal, 2 Sport, 3 Dolly, 4 FPV, 5 Custom |
| `FlycamDroneCruise`, `…Top`, `…TimeToTop`, `…StopDistance`, `…HoldTime`, `…CoastTime`, `…HighSpeedFloat`, `…Onset`, `…Grip`, `…TurnRate`, `…TurnWeight`, `…TiltRate`, `…Climb`, `…ClimbTime` | F32 | Normal preset (§10) | validated in `compile()` |
| `FlycamDroneRelease` | S32 | 0 Hold | 0 Hold / 1 Drift |
| `FlycamDroneThrottleMode` | S32 | 0 Assist | 0 Assist / 1 Pure |
| `FlycamDroneFreeFly` | BOOL | 0 | |
| `FlycamDroneSimulationHz` | F32 | 120 | 30–240 |
| `FlycamDroneMoveDeadzone`, `…LookDeadzone`, `…Saturation` | F32 | 0.10 / 0.12 / 0.05 | 0–0.5 |
| `FlycamDroneExpoMove`, `…ExpoYaw`, `…ExpoGimbal` | F32 | 0.35 / 0.45 / 0.45 | 0–1 |
| `FlycamDroneResumeMomentum` (U4) | S32 | 0 Stop | 0 Stop / 1 Keep (§11.2.1) |
| `JoystickXInputDpadMigrationVersion` (U1, R2-P2-10) | S32 | 0 | never-decreasing migration version (§12.1) |

**Reused, not duplicated:**
- `JoystickXInputTriggerMode`, `…TriggerDeadZone`, `…TriggerCurve`
- `JoystickXInputDpadLeft/Right/Up`: defaults stay 0; the comment lists 0–6; None-only migration (§12.1)
- `JoystickXInputRumble*`
- `FlycamBuildModeScale`, `AutoLeveling`, `Cursor3D`
- the sign of `FlycamAxisScale*`, and `|FlycamAxisScale3|` for the roll rate
- `FlycamOperator*`

**Removed versus rev 3:**
- `FlycamDroneTriggerMode`, `…SwapTriggers`, `…ThrottleAxis`, `…BrakeAxis`
- `…TriggerDeadzone`, `…ExpoThrottle`
- all five new `JoystickButton*` keys
- the extras and `…Telemetry`

**Defaults and presets.**
- `setXboxDefaults` / `setSNDefaults` **do not** touch `FlycamFlightModel`, so Classic stays the default.
- A "Reset Drone" button restores the Normal preset values.
- Preset table: rule-6 count (§10).

---

## 14. UI

- **Joystick floater → "Flight model" section** (the same floater that hosts the XInput controls):
  - Classic/Drone radio, disabled with a reason under Cursor3D;
  - preset combo;
  - the §9 human-terms sliders. Each time shows its exact mass-stage value plus "(+t_j onset)", and validation clamps are shown in amber;
  - read-only `L_tot` / `J_eff` in "Advanced";
  - Release (Hold/Drift), Throttle (Assist/Pure), Free-fly;
  - **Resume momentum: Stop / Keep** (U4), with the tooltip "After alt-tab or when another camera hands back:
    Stop = start still; Keep = carry on at the camera's last on-screen speed";
  - "Advanced": dead zones, expo, sim Hz;
  - a one-line trigger-mode readout reusing `getXInputStatus()`, plus the non-XInput note (§8.4);
  - the D-pad combos gain entries 4–6.
- **Flycam / Director panel:** Flight model, preset and Release (reachability rule).
- **Toasts:** existing `xInputHint` only. Hidden with UI, as today.
- **No work in `draw()`.** Slider commits feed the debounced idle `compile()`, and the panel reads cached compiled values (CLAUDE.md 2026-10-05 rule).

---

## 15. Instrumentation — the log states a verdict

Once per 5 s while Drone is active (`LL_INFOS("FlycamDrone")`):
`preset=… hz=… steps/s=… backlog=… vmax=… acc_max_seen=… (L_tot=…) jerk_max_seen=… (J_eff=…) clamps=…`

| Verdict | Meaning |
|---|---|
| `DRONE OK` | Normal running |
| `DRONE NONFINITE` | State restored. This must never happen; treat it as a bug report |
| `DRONE BOUND-EXCEEDED acc\|jerk` | Measured camera acceleration > `L_tot`·1.001 or jerk > `J_eff`·1.001 within a continuous run. §6.4 proves this impossible, so it means an implementation bug |
| `DRONE ORIGIN-SHIFT \|Δ\|` | The agent origin changed; Drone state was re-expressed (§11.9) |
| `DRONE BACKLOG <s>` | The accumulator is above 0.1 s (informational) |
| `DRONE CLAMP <param> req→applied` | Input validation clamped a setting (§9). There are no stability bounds any more |
| `DRONE OWNER-REGAIN self\|foreign momentum=…` | Reseed on ownership regain (§11.2), with the momentum outcome: `stop`, `keep v=…`, or `keep-rejected:<stale\|straddle\|unpresented\|dt\|nonfinite\|cut\|inconsistent>` |
| `DRONE INACTIVE 3d-cursor` | Drone requested, but excluded (§11.6) |
| `DRONE TRIGGERS-STALE` | THROTTLE mode with no fresh XInput sample for > 2 s while focused (informational) |

---

## 16. Verification

### 16.1 TUT — `indra/newview/tests/llflycamdrone_test.cpp` (pure class, no viewer)
Unless stated otherwise, each test runs for every preset × Hz ∈ {30, 60, 120, 240}.

**Fixed step and determinism (§7)**
1. **Frame-grouping invariance:** an identical per-tick input sequence through frame-dt sequences {1/30, 1/60, 1/144, jittered ±40 %} gives **bit-identical** state.
2. **Live input, bounded:** frame-held input at 30/60/144 fps versus tick-aligned timestamped input. Position divergence ≤ Δinput × frame_dt × V_t at each transition, and no unbounded drift.
3. **Interpolation latency:** the render pose always lies between prev and cur, with lag ≤ h.
4. **Backlog:**
   - `advanceRaw` with 2000 ticks' worth: ≤ 512 steps per call, backlog retained and drained over later calls, total simulated time = total input time (±1e-9 s), no pose discontinuity.
   - `advance()` clamps to 0.25 s.

**Calibration (§9, closed form)**
5–9. **Calibrated times** for `t_top`, `d_stop`, `t_hold`, `t_coast` and `t_climb`, per preset × Hz:
   - **Mass stage:** the stage-1 crossing (sub-tick interpolated, §7/§9) is within **0.5 %** of the request. The raw tick-quantised crossing is within ±h.
   - **Camera output:** the crossing equals `t + t_j` within `2 %·t + h`.
   - **`d_stop`:** the **camera's** total distance until settle satisfies `d_stop − max(1 mm, 1.5·ε_q·t_j) − 1e-6 ≤ d_cam ≤ d_stop + 1e-6` (R4-P0-1, R5-P0-1, R6-P0-1, R7-P0-1). That is `d(D_brk) + V_t·t_j − ε_trunc`, with `0 ≤ ε_trunc ≤ 1.5·ε_q·t_j` measured and asserted, including an adversarial settle with `|y1| = ε_q` and `|v| = 2·ε_q`. Also assert `v_r = 0` exactly on every settle tick. Also checked separately: the identity "extra distance = `V_t·t_j`" from a steady state, for every preset × Hz.
   - **Determinism:** `compile()` evaluated twice gives bit-identical coefficients. It has no iteration.

**Guarantees (§6.4–6.5) — the proofs, checked numerically**
10. **G1 (acceleration and jerk on the camera velocity):**
    - **Every tick:** across a 10 000-run fuzz, `|Δv|/h ≤ L_tot^max` and `|Δ²v|/h² ≤ J_eff` computed from `L_tot^max` (×1.000001 for FP; R8-2 — after a limit drop the bound is `max(old, new)` until a steady point, §6.4).
    - **Fuzz inputs:** random stick/throttle/brake slams, ROLL↔THROTTLE switches, takeover loss, stale samples, preset switches mid-motion, Drift↔Hold toggles, recentre, overspeed seeds ≤ V_cap, and the settle snap.
    - **Proof coverage:** the fuzz includes **R3's counter-example class**: high acceleration toward a target that is then raised slightly. Stage-1 target overshoot must be 0, and G5 must hold.
    - **Planar:** fuzzed with turning.
11. **G2 / Lemma 5:**
    - **One-directional history:** every run whose reference stayed within a 90° cone for ≥ 8·t_j before a stop (Hold release, full brake, Drift) has `v·r̂ ≥ 0` on every tick after the stop.
    - **Mixed history:** a reversal or a sharp turn just before the stop gives `v·r̂ ≥ −W_m·max|v_r|` on every tick. Assert the bound, and that the residue decays geometrically.
    - **Stage 1 alone:** `v_r(n)·r̂ ≥ 0` always.
12. **Overspeed:** from \|v₀\| = 2·V_t with full stick, the camera speed decays monotonically to target, G1 holds, and there is no overshoot below the target (G5).
13. **Reversal:** full stick back from full forward.
    - The camera velocity crosses zero exactly once, continuously.
    - There is no snap tick, and no stall at zero beyond the smoother's natural passage.
    - G1 holds throughout.
14. **Grip floor:** at `G = 0.05`, sideways momentum decays linearly at `G·A_max`, stays finite and has no NaN. Validation lifts `G < 0.05` to 0.05.
15. **Settle and all snaps (R6):** a snap fires only under its §6.3 step 6 conditions, and settle only with `v_r = 0` exactly.
    - **Measured jerk:** ≤ `(3/16)·J_⋆` on the snap tick and ≤ `(5/8)·J_eff^after` on the next tick.
    - **Large downward changes (R7-P0-2):** run each snap kind after
      - an `L_tot` drop of ≥ 30× (FPV → Dolly limits, then re-baseline),
      - a `t_j` rise from 0.05 s to 1.0 s (a large `g_max` drop at switch-over),
      - and both at once,
      - and **combined events (R8-1)**: a pending pole while a re-baseline is eligible, and a pending pole at the settle tick. Assert exactly one snap fires, the pending pole is adopted on that tick, `L_tot^max` resets, and no snap fires on the next tick.

      Each must meet both bounds measured against the **after** values. Eligibility must be refused while the conditions meet only the pre-snap bound.
    - **Adversarial:** each condition held exactly at `ε_q`, with the next stage-1 step at `L_tot·h`. Both bounds must still hold.
16. **Bad values:** NaN/Inf **input** and NaN/out-of-range **settings**. `compile()` rejects or clamps them and logs, the previous valid parameters stay live, the state stays finite, and the NONFINITE path is exercised.
17. **Hz extremes:** at 30 and 240, every G holds and every T5–T9 tolerance holds. `J_eff` at 30 vs 240 Hz differs by < 25 % (§6.4 near-independence).

**Frames, seeding and parity**
18. **Horizon lock:** θ = −80°, full forward gives Δz = 0 (±1e-6). Free-fly gives non-zero Δz.
19. **Seeding:** exact reconstruction (< 0.05°) for 10 000 random quaternions, with and without roll.
20. **±90° continuity sweep** (85° → 90° → 95°). The prior yaw is used below c = 1e-4, the canonical sign holds, the rendered pose is continuous, and 1 s at zero input causes no drift or pop.
21. **θ_lim:** a seeded 89.7° is kept, input cannot increase \|θ\|, and the limit drops to 89° after it returns.
22. **Adversarial settings** (t_top = 0.05, t_j = 1, d_stop = 0.01, float = 0, Hz = 30):
    - validation clamps (times ≥ 4h, **`d_stop ≥ V_t·(t_j + 2h)`** (R5-P1-4), grip ≥ 0.05) and logs;
    - the `D_brk` bisection stays inside its proven bracket and terminates;
    - every G still holds in a fuzz run with the result;
    - `compile()` takes < 1 ms.
23. **Build-mode scaling:** calibrated times are identical and distances scale by s.
24. **Direction parity:** for each axis, under BD and non-BD, with `mInvertPitch` on/off and a ± `FlycamAxisScale` sign, `sign(Drone response) == sign(Classic response)`.
25. **Triggers:** `ALXInput::droneTriggers` shaping, a stale/unfocused sample gives (0, 0), and Classic `throttle()` / `roll()` golden values are unchanged (extends `alxinputmath_test.cpp`).

**Transitions and ownership** (using WP0's extracted Classic integrator as the Classic side)
26. **Classic(BD)→Drone** at 30/60/144 fps (clamp active at 144): v continuous ±2 % and no pop.
27. **Classic(non-BD)→Drone:** same checks.
28. **Drone→Classic** for BD and non-BD: the first Classic frame's displacement = v·dt ±2 %, and rotation increments are continuous.
29. **Orbit edges:** on the rising edge the pose held equals Drone's last; on the falling edge, history seed with a momentum cap.
30. **Ownership regain (Stop), driven by dispatch serials and owner tags (not `gFrameCount`):**
    - SELF gap (FLYCAM dispatches that returned early) → own-pose reseed, v = 0, operator **not** reset;
    - FOREIGN gap (any non-FLYCAM owner in between) → rendered-pose reseed, v = 0, operator reset requested;
    - no gap → no reseed;
    - **minimized:** `display()` skipped while idle dispatches continue → classified correctly (no false negative);
    - **multi-pass snapshot:** `display()` called 4× in one idle → no gap (no false positive).

**Resume momentum (U4; `FlycamDroneResumeMomentum`, §11.2.1)**

The foreign cases use a synthetic other-owner camera path fed through the final-camera sampler helper.

31. **Keep, self gap** (2 s gap, mid-glide):
    - the resume pose equals the last rendered pose (≤ 1e-6);
    - v equals the last rendered (interpolated) v;
    - the gap's wall time is not integrated;
    - G1 holds across the gap (one continuous run), because all stage states are kept.
32. **Keep, foreign hand-back:**
    - **Constant u:** seeded `v == u` ±1e-4, and the frame-S start pose equals `C_{S−1}` exactly.
    - **Constant acceleration A ≤ A_keep:** `|v_keep − v_true(end of S−1)| ≤ ε_v`.
    - the smoother starts at steady state (acceleration 0), and G1 holds from there.
33. **Cut rejection:**
    - A jump with \|v_b\| > V_cap gives Stop, reason `cut`, and **no clamped velocity**.
    - A rotation-only cut (\|Ω_b\| > 2·ω0) is also `cut`.
    - A cut smeared over two frames is `inconsistent`.
34. **Straddle, stale and unpresented:**
    - mixed owners in S−3..S−1 give `straddle`;
    - non-consecutive serials give `stale`;
    - any sample not PRESENTED (minimized, so no `display()` between samples) gives `unpresented`.
    - Each is Stop.
35. **Bad samples:** `dt ≤ 0.0005` or `dt > 0.25`, or a non-finite sample, gives Stop with the reason logged.
36. **After Keep:**
    - Hold with no stick eases to rest with `v·r̂ ≥ 0` on every tick (Lemma 5, steady-state start);
    - Drift glides on drag;
    - an overspeed resume (V_t < \|v\| ≤ V_cap) decays monotonically.
37. **Stop and reset:**
    - Stop reproduces T30 exactly;
    - reset/teleport is Stop even with Keep set.
38. **Keep rates:** ω, θ̇ and φ̇ are seeded from the quaternion delta (§11.5.1) and clamped to their limits.

**rev 5 additions**

42. **SELF gap with operator ON** (Legacy and fixed-tick locomotion; Stop and Keep):
    - The post-operator rendered pose is continuous across the gap (≤ 1e-4 m / 0.01°).
    - The operator is not reset.
    - Uses the existing `llcameraoperator_test.cpp` harness.
43. **±90° rate continuity:**
    - A constant world Ω carries the camera through θ = ±90° and over the top.
    - Rates seeded every frame from §11.5.1 show no 180° jumps.
    - At the 85° switch, rate continuity is within `Φ_rate`.
    - The LL quaternion multiply order is pinned.
44. **Rate projection:** away from vertical, the seeded (ψ̇, θ̇, φ̇) reproduce Ω to < 1e-4 rad/s.
45. **Recentre latch:**
    - **Completes:** θ → 0 with no overshoot, then R clears.
    - **Cancels:** any tilt input cancels with no rate snap.
    - **Clears:** reset, regain and model switch all clear R.
    - **Survives:** a preset switch keeps R.
    - **Idempotent:** a repeat edge while active is a no-op.
    - **Never saved:** R is not persisted.
46. **Closed-form compile:**
    - every coefficient matches its §9 formula to 1e-9 relative;
    - the explicit `d(D)` formula equals a brute-force discrete sum;
    - the `D_brk` bracket bounds hold for 10 000 random (V_t, d_stop, h).
47. **Agent-origin shift (§11.9):** a synthetic origin change of 256 m is applied in four situations: mid-flight, during a SELF gap, during a FOREIGN gap, and between Keep samples S−3..S−1.
    - The rendered global camera position is continuous (≤ 1e-4 m).
    - Keep velocity is unaffected, with no `inconsistent` or `cut` rejection caused by the shift.
    - History seeds are unaffected.
48. **Toggle-time reset (R3-P1-5):** `moveFlycam(true)` called outside a tagged dispatch.
    - It leaves `lastWrittenSerial` and the foreign flag untouched, and sets `resetPending`.
    - The next tagged FLYCAM dispatch consumes it with Stop: no Keep from agent-camera frames, and no regain misclassification.
49. **Coefficient change while moving (R4-P0-3):** for every pair of presets, and for build-scale ease-out, switch at full speed so the old speed is above the new `V_cap`. Then Drift, Hold-release and full-brake.
    - Every tick satisfies `|Δv_r| ≤ L_tot·h` (drag saturation active above `V_cap`).
    - Every tick satisfies `|Δv|/h ≤ L_tot^max` and `|Δ²v|/h² ≤ J_eff`, with `L_tot^max` **held at max(old, new)** until an exact steady point (R5-P0-2). A re-baseline snap fires at cruise and resets it.
    - **Adversarial case:** switch to a lower-limit preset while maneuvering continuously, so no steady point ever occurs. Then G1 holds against `max(old, new)` indefinitely, and **never** against the new `L_tot` alone.
    - The re-baseline snap's jerk is ≤ `(3/16)·J_⋆` on its tick and ≤ `(5/8)·J_eff^after` on the next (R6-P0-2, R7-P0-2). This is exercised with the largest downward preset change (FPV → Dolly), where `J_eff^after ≪ J_eff^before`, and again with a `t_j` edit pending at the same time (one atomic snap, R8-1).
    - Velocity is never snapped.
    - Below `V_cap`, the saturation is inactive and `t_coast` stays exact.
50. **Free-fly alignment (R4-P0-2):** stick pitched straight up or down plus full climb, and every mix in between.
    - The combined 3-D clamp keeps `|Δv_r| ≤ L_h·h`.
    - No axis receives two stage-1 accelerations.
    - Toggling Free-fly ↔ horizon-locked mid-flight keeps G1.
51. **Stops with steering, per channel (R4-P1-4, R5-P0-3):** full brake and Hold-release while yawing at full rate, swinging the stick through 360° under full brake, and Drift with yaw. **Each is also run with mixed horizontal + vertical motion** (climbing or descending at the stop), in horizon-locked mode and in Free-fly.
    - Per channel: `v_r,c(n)·r̂_c ≥ 0` on every tick.
    - H stays parallel to `r̂_H`; Z never changes sign; Free-fly's F stays parallel to `r̂_F`.
    - In horizon-locked mode, the overall 3-D direction is allowed to bend (H and Z finish at different times). Assert that nothing reverses.
    - The camera satisfies Lemma 5 per channel.
    - Partial brake with steering is excluded from G2 by definition, and is checked only for G1 and G5.
52. **Deferred onset change:** edit `t_j` mid-maneuver.
    - The old `p` runs until the first steady tick.
    - The switch-over snap satisfies the §6.4 snap bounds: `3/16·J_⋆` on the snap tick and `5/8·J_eff^after` on the next, with `J_eff^after` computed from the **pending** pole.
    - Exercised with `t_j` 0.05 → 1.0 s (downward `J_eff`) and 1.0 → 0.05 s.
    - **Combined (R8-1):** the edit pending when a preset drop makes a re-baseline eligible, and when the settle fires. One snap adopts the pole and resets `L_tot^max`; `J_⋆` uses the pending pole.
    - From then on, `J_eff` follows the new `p`.

### 16.2 Golden Classic tests — `indra/newview/tests/alflycamclassic_test.cpp` (P1-5)
**WP0** extracts the two Classic per-frame integrators **verbatim** into the pure `alflycamclassic.{h,cpp}`. Inputs
are the raw axes, buttons, dt, flags and scales; the state is `sDelta`, `sLastDelta`, pose and zoom. `moveFlycam`
calls the extracted function. The Codex review checks for a move-only diff (`git diff --color-moved`).

#### 16.2.1 Counting-verified move (U5; CLAUDE.md rule 6)
The move is verified **by counting, not by compiling**. The WP0 implementer and the reviewer each produce the
counts independently, and they must agree.

1. **Before and after line counts** of each moved block, as re-anchored at implementation time:
   - BD `ljc:1769-1913`;
   - non-BD `ljc:1914-2021`;
   - **the shared Level-horizon cleanup `ljc:2023-2027`** (R2-P1-9: `if (level_horizon) { sDelta[CAM_X_AXIS] = 0; sLastDelta[CAM_X_AXIS] = −getFlycamAxis(mJoystickAxis[CAM_X_AXIS]); }`). It moves as the **tail** of the extracted unit, so the golden Level-horizon tests cover it.

   The cleanup's `getFlycamAxis(...)` re-read is plumbed as the same frame's raw CAM_X input. The two reads are identical because `getFlycamAxis` is a pure getter with no state change between them, and the reviewer confirms this.

   Excluding the plumbing lines listed in step 3, the moved body's non-blank line count must equal the original's.
2. **Token-level identity.** Each block is normalised by stripping whitespace and comments, and the plumbing substitutions in step 3 are applied mechanically. The result must equal the original byte for byte. Count and report:
   - arithmetic operators (`+ - * / += -=`);
   - `llclamp` / `llmin` / `llmax` / `llabs` calls;
   - `if` statements;
   - array subscripts.

   Each count must match the original exactly.
3. **Plumbing table.** List every identifier that changed from a global or member to a struct field, for example:
   - `getJoystickButton(mMappedButtons[X])` → `in.button[X]`
   - `LLToolMgr::…inBuildMode()` → `in.buildMode`
   - `getMin/MaxView()` → `in.minView/maxView`
   - `mXITakeover` / `mXIMode` → `in.*`
   - statics → `state.*`

   Give the **occurrence count** of each identifier in the original and in the extracted code; each pair must be equal. Side effects that cannot move, such as `gAgent.clearAFK()`, stay in `moveFlycam`, driven by the returned `is_zero` flag. They are listed explicitly.
4. **`git diff --color-moved=zebra --color-moved-ws=ignore-all-space`** shows only moved lines plus the step-3 plumbing lines. Any other changed line is a must-fix.
5. **Call order and float evaluation order are unchanged.** There is no algebraic "tidying" and no reordering of the feather or clamp operations, because a different FP order is a behaviour change.

The golden expected values come from an **independent reference** written from the §3 facts **by a different Codex
session** than the extractor. They are compared at ≤ 1e-6.

**Matrix**
- BD and non-BD
- XInput off / ROLL (shaped takeover) / THROTTLE takeover. THROTTLE must scale translation **and** rotation; zoom is excluded.
- Build mode on and off
- dt ∈ {1/30, 1/60, 1/144, 0.5}, which exercises both clamps
- Per-axis dead zones
- Jump/Crouch/roll/zoom buttons, BD only
- `ROLL_DEFAULT` and Level horizon, including the moved post-integrator cleanup (`sDelta[CAM_X] = 0`, `sLastDelta[CAM_X]` reseeded) for BD and non-BD
- AutoLeveling on and off

**What the extracted function does not cover**
- **Orbit** and **operator-off** sit outside the extracted function.
- They are covered by the **structural guarantee**: their hunks are unchanged except the hoisted resolve, which is checked by Codex against F21.
- They are also covered by in-world A/B (§16.4, items 9–10).

### 16.3 XInput / D-pad (pure helpers + `alxinputmath_test.cpp`)
- **D-pad Down:**
  - toggles the mode under both models;
  - the hint text is model-aware;
  - `RUMBLE_MODE_TICK` is requested.
- **Actions 0–3:** unchanged.
- **Actions 4–6:**
  - action 4 toggles the flight model in both models (it is refused under Cursor3D);
  - actions 5–6 are hint only, with no state change, in Classic;
  - actions 5–6 change state in Drone;
  - out-of-range values do nothing.
- **T39 — None-only migration:** for every combination of the three slots ∈ {0, 1, 2, 3, 7}, run the migration once:
  - slots at 0 become 4 / 5 / 6 (Up / Left / Right);
  - every non-zero slot is unchanged;
  - `JoystickXInputDpadMigrationVersion` becomes 1.
- **T40 — versioned migration:**
  - after migrating, set a slot back to 0 and re-run init: it stays 0;
  - with the version at 5 (a newer build), nothing is filled and the version stays 5 (it never decreases);
  - with the version reset to 0 and cue values present, only the 0 slots fill (the documented restored-backup case).
- **T41 — "Xbox Defaults":** applies the same None-only fill (cue and level slots preserved) and leaves `FlycamFlightModel` unchanged. Each D-pad combo has exactly 7 entries with values 0–6 (rule-6 count).
- **Focus loss:** produces no edges (existing).
- **Reconnect / invalidate:** a stale sample gives `thr = brk = 0`, and the jerk-bounded result is smooth.
- **Release-pending:** still suppresses axis 2 after THROTTLE→ROLL.

### 16.4 In-world (outcomes stated in advance)

**Triggers**
1. **Triggers (Drone, THROTTLE):** RT alone gives throttle, LT alone gives brake; released gives neither. D-pad Down then gives ROLL, with the hint "Triggers: ROLL (Drone)" and a tick rumble.

**Feel**
2. **Horizon lock:** gimbal straight down plus forward gives level flight (Classic: dive, as today).
3. **Mass:** Normal preset. Drift reach-cruise-release glides ≈ `t_coast`; Hold eases to a hover.
4. **Clutch mid-flight:** cruising, squeezing RT gives a smooth pull that tapers near top speed; LT gives a progressive slow-down that never jerks to zero.
5. **Turn weight:** in Sport, a 90° stick swing carves wide at speed and tight when slow.

**Robustness**
6. **Mode toggle mid-move:** no jolt.
7. **Frame-rate:** the same performance capped at 30 fps vs uncapped feels the same, and covers distance within the §7 bound.

**Integration**
8. **Operator:** ON, with boost, the gait and shake intensify with speed; OFF is clean.
9. **Orbit:** enabled mid-flight, Orbit behaves exactly as today; on exit Drone resumes with momentum and no pop.
10. **Classic A/B:** `FlycamFlightModel = 0` on BD and non-BD, in ROLL and THROTTLE, with build mode and the operator off, is indistinguishable from the pre-change exe.
11. **Ownership (Stop):** mid-Drone flight, start a Path Camera preview, recorder playback, or a Director arm. When it ends, the flycam continues from the rendered camera with no jump, starts still, and the log shows `OWNER-REGAIN foreign momentum=stop`.
12. **Ownership (Keep):**
    - When a recorder dolly or Path Camera move ends, the flycam carries on in the same direction at the same on-screen speed, with no jump, then eases out (Hold) or glides (Drift).
    - A Director *cut* hand-back starts still, and the log shows `keep-rejected:cut`.
    - Alt-tab mid-glide, then return: the glide resumes from the same spot.
13. **Teleport:** resets with v = 0, even with Keep set.
14. **Cursor3D on:** Drone is greyed out with its reason and Classic runs.
15. **D-pad (U1):**
    - Up toggles Classic↔Drone with a hint and a tick.
    - Left toggles Hold↔Drift.
    - Right recentres the gimbal.
    - A profile that already had cue Go/Back on Left/Right still has them after the update.

---

## 17. Off-path: behavioural equivalence (not "byte-identical")

With `FlycamFlightModel = Classic` (the default):
- The Classic integrators produce the same outputs for the same inputs. WP0 golden tests (§16.2) prove this.
- Orbit and the camera/operator tail are unchanged hunks, apart from the const-hoisted `resolveAnchor`.
- The added pose-history bookkeeping only writes Drone-private statics.
- The XInput trigger predicate, shaping and THROTTLE scaling are untouched.
- D-pad values 0–3 are untouched. The None-only migration (§12.1) can only *fill* empty D-pad slots.
- The final-camera sampler (§11.2.1) only reads the camera and writes Drone-private state.

No Drone state is touched, and no render state is touched anywhere in this feature.

---

## 18. Reviewer — attack these first (round 8)

1. **Is the approach wrong?** Argue the Classic-shadow + pose-override pipeline against a Drone branch that replaces the Classic block. Is the shadow's `sDelta`/zoom coupling sound in every edge?
2. **WP0 extraction.** Prove it is move-only. List every global the Classic block touches (`getJoystickButton`, `LLToolMgr`, view limits, `gAgent.clearAFK`, `mXI*`) and how it is plumbed.
3. **The §6.4 proofs.** Attack these points:
   - **Lemma 1:** is every stage-1 step really bounded by `L_tot·h`? Check the taper flow at u ≈ s*, the drag flow at seeds of `V_cap`, the Drift snap, and Free-fly 3-D.
   - **Summation by parts:** `Δ²v = Σ(g_k − g_{k−1})Δv_r`, with the `TV(g) = 2g_max` claim. Check it with a seed mid-run.
   - **Lemma 5:** the exact relaxation `W_m`.
   - **Feel:** is a convex-combination smoother adequate drone feel, versus a true S-curve tracker?
4. **Calibration.** Check monotonicity of each 1-D problem, bracket adequacy, and cost at 240 Hz with the slowest preset.
5. **Ownership detector.** Is there any path where `moveFlycam` runs on consecutive frames while another system moved `LLViewerCamera` in between, for example agent-origin shift on a region crossing? Drone `p` shares the agent space of `sFlycamPosition`.
6. **Operator.** Is mixing `update()` (Classic/Orbit frames) with `updateFromPose()` (Drone frames) safe given the reset-on-every-edge rule?
7. **E2 seed.** Is `sDelta = v·dt_k` correct against Classic's per-frame-displacement semantics under BD's clamped `time`?
8. **D-pad enum 4–6 and the versioned None-only migration.** Look for persistence issues: settings saved by an older build, a saved value outside 0–6, and a lost, restored or newer `JoystickXInputDpadMigrationVersion`.
10. **Dispatch serial.** Is there any path that calls `moveFlycam` outside the `idle()` arbitration, or more than once per serial? Candidates: `toggleFlycam → moveFlycam(true)` (`ljc:2266`), and snapshots. Does any owner write `LLViewerCamera` outside its dispatch branch? Either would break the owner-tag classification.
9. **Keep momentum.** Find a hand-back that passes every validation check yet produces a visible jump or a wrong-direction glide. Examples: a slow cross-fade that a Director treats as a cut, an operator shake baked into the owner's samples (does it trip `inconsistent` too often?), or an agent-origin shift within S−3..S−1.

---

## 19. Work packages & delegation (P1-7; user's current directive)

**Roles:**
- **Opus** designs (this paper and revisions).
- **Codex implements every WP**, including the core model. It works via `--prompt-file` with an explicit OFF-LIMITS list and a watchdog that cancels on a log stall, and it never builds.
- **Codex adversarially reviews** each WP in a **separate, fresh session** from the one that implemented it. This follows the user's current directive (U6). `CLAUDE.md` is deliberately left unchanged.
- **Claude builds once.**

**Tests:** every test WP is written by a different Codex session than the code it tests.

| WP | Content | Implements | Adversarial review |
|---|---|---|---|
| 0 | Extract the Classic integrators verbatim into `alflycamclassic.{h,cpp}`; `moveFlycam` calls it. Deliver the §16.2.1 counting report. | Codex A | Codex (fresh): independent recount + move-only diff |
| 1 | `LLFlycamDrone`: §6 two-stage `step()` (§6.4 lemmas are the spec), §7 fixed step + interpolation, §9 closed-form compile, §11.9 origin tag, §11.5 seeding, §11.4 helpers, NaN guards; `ALXInput::droneTriggers` | Codex B | Codex (fresh) |
| 2 | Tests §16.1–16.3, including the golden reference (written against the WP0/WP1 interfaces only) | Codex C | Codex (fresh) |
| 3 | `moveFlycam` integration: anchor hoist, edges, ownership via dispatch serial + owner tags + resume momentum (§11.2), Drone override, operator feed (SELF-gap exception), build-mode easing, Cursor3D gate, D-pad enum 4–6 with the recentre edge, model-aware hints and status; `alcameradispatch.h`; in `llappviewer.cpp`, the serial increment, the owner tag per branch and the final-camera sampler call (before `onIdleFrame`). **Review checklist (R2):** the post-shadow overwrite and the edge-frame ordering exactly as §4 and §11.4 | Codex D | Codex (fresh) |
| 4 | `settings.xml` keys §13, all five presets §10 (rule-6 count), Reset Drone, the versioned D-pad None-only migration + `setXboxDefaults` fill (§12.1) | Codex E | Codex (fresh) |
| 5 | UI §14: floater section, panel, D-pad combos; no work in `draw()` | Codex F | Codex (fresh) |
| 6 | Instrumentation §15 and a `MACHINIMA_USER_GUIDE.md` section | Codex G | Codex (fresh) |

**Order (CLAUDE.md + memory):**
1. Implement all WPs.
2. Batch the review findings, then re-defer the fixes as one set until **0 must-fix across all WPs**. Severity gate: anything that breaks "feels like it has mass" or Classic equivalence is a blocker, whatever its label.
3. Make a checkpoint commit and back up the exe.
4. Run the pre-build stale-obj guard.
5. **Build once.**
6. Run the TUT tests.
7. The user runs the in-world test.

The feature ships whole: simulation, settings, UI and wiring together.

---

## 20. User decisions — RESOLVED (2026-10-06)

All six rev-4 questions were answered by the user. See **User decisions (2026-10-06)** at the top (U1–U6). No user decision is open in rev 4.1.
