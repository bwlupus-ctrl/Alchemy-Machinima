# Character Mover V2 — Corrective Review Response (fixes to `bb515fa0b79`)

**Range reviewed:** `35cd2ef0d13..bb515fa0b79` (`fix: harden Character Mover V2 review findings`)
**Method:** three adversarial passes (motion engine, actor-mover+AO, director/UI), each verifying the fixes against the prior findings *and* hunting for regressions the fixes introduce. Every BLOCKER/MAJOR re-verified by hand against the committed code. No build run.

## Gate decision: HOLD

The corrective commit **genuinely fixes** the original findings — but it **introduced one new BLOCKER** (a dwell/arrival gait regression) and left **one MAJOR** (seek role from scrub velocity). Compile/link is clean; the viewer would build. HOLD is for runtime correctness, not the build.

### Confirmed FIXED (verified)
- **AM-M1 (AO cross-kill):** `AOEngine::resolveAnimation()` is provably read-only — every callee (`getStateForMotion`/`getStateByRemapID`/`mapSwimming`/`AOSet::getState`) is a pure read; no cycle/timer/cleanup/`sendAnimationRequest`; `const` honest (no `const_cast`/`mutable`); it even avoids `override()`'s disabled-AO `setLastMotion` side effect. The fake AO transaction is gone from `LocalMotionHandle` with zero dangling refs; the only remaining `override()` users (Director turn-anim) self-balance. **Solid.**
- **AM-M2 (hang):** closed-form `floor`-based offset, O(1), divisor-guarded, F64→F32 bounded via `llmin(x, FLT_MAX)`; hostile Inf/NaN metadata fails closed to finite. **Solid.**
- **AM-m9 (arc budget):** whole-path cap proven by induction ≤ **65533** samples (<65536), per-segment ≤4096, deterministic, fair rollforward, no starvation/div-by-zero; node cap enforced at all four `mNodes` growth sites. **Solid.**
- **AM-m4/m5/m7/m8:** dwell-anim carry-over preserved across migrate; zero-distance holds no longer touch gait; the `isSelf()` stop bypass removed (sim-signaled UUID never locally stopped, self included) with no orphaned AO; hover/fly hysteresis 0.20/0.10. **Fixed individually** (but see BLOCKER).
- **B1/B2 (test blockers):** all `ensure_approximately`→`ensure_distance`; source closure `llmotion.cpp;llpose.cpp;lljoint.cpp` resolves the LLPose link with **no new unresolved external** (symbol-audited). **Fixed** (static-verified, not yet BUILD_TESTING-built).
- **ME-m2 (weight double-apply) + weight-1.0 bit-identity:** neutralize-then-apply-once at all 5 capture sites; identity branch proven zero-float-op bit-identical. The one new edge this introduced (N1, below) is now hardened.
- **DIR-1 / DIR-m1 / DIR-m2 / DIR-m3:** role combo restricted to the 5 runtime-reachable roles (enum values preserved for old-scene compat) with honest status; air-path pitch greys out with honest tooltip; dead rate fields removed cleanly (no dangling refs, old scenes ignore the keys — also moots DIR-m4); metadata drafts retained and **cannot leak** into the runtime (`resolveLocomotionRole` rejects null-`mAnim`). **All solid.**
- **N1 (new edge from the ME-m2 fix): FIXED** — `getPresentationBaseWeight()` divide branch now `llclamp(…,0,1)` (identity/zero branches untouched, bit-identity intact, Inf/NaN closed, no new include).

---

## BLOCKER — new dwell/arrival gait regression (runtime, default config)

- **File/function:** `LLActorMover::updateLocomotionGait`, `llactormover.cpp:570-573` (the reset) → restart at `llactormover.cpp:614-617`.
- **Mechanism (verified):** the corrective commit added
  ```cpp
  if (!mv.mLocomotion.valid() && !mv.mSecondaryLocomotion.valid())
      mv.mRole = Cast::LOCO_NONE;   // :570-573
  ```
  which cannot distinguish "handles empty because lost/failed" from "handles empty because this frame stopped them on purpose while the Move stays live." Forcing `mRole = NONE` defeats the `desired != mv.mRole` guard at :614, so
  ```cpp
  else if (!mv.mSecondaryLocomotion.valid() && desired != mv.mRole)
      beginLocomotionTransition(av, mv, desired);   // :616 — fires EVERY frame
  ```
  restarts the gait every frame (desired resolves to WALK/HOVER via the `locomotion_anim` fallback). `updateLocomotionGait` is called every frame in `advancePath` (:4338) and the legacy path.
- **Two reachable manifestations:**
  1. **Dwell holds broken.** Dwell-entry stops both sides, then the same frame reaches :4338 → reset → :616 restarts the walk it just stopped; subsequent dwell frames early-return before sampling, so a frozen walk plays (or fights the node dwell anim) for the whole hold instead of the intended stand. **Regresses the core dwell feature.**
  2. **Arrival-hold churn.** After a stop-mode arrival clears the handles (legacy :3543-3550; path :4459-4464), every frame runs gait→reset→:616 start→arrival stop again — a full create/claim/startMotion + stop cycle **every frame, forever**, while held. Pre-fix this was a stable `desired == mRole` no-op.
- **Violated invariant:** "Stop, arrival, dwell … cannot leak or cross-stop a motion side" (the fix's own claim 8) and the original AM-m5 fix.
- **Fix (minimal):** the reset at :570-573 is largely redundant — both genuine loss paths already clear `mRole` inside `updateLocomotionSample` (:765-771 lost, :803-808 failed). The only case it uniquely covers is a *failed* `startLocalMotion` that left `mRole` set at :590. So: **roll back the `mRole`/`mAnim` assignment on the `startLocalMotion` failure branch** (:592-597) instead of resetting unconditionally in the gait — or clear `mRole` only at the deliberate-stop sites (arrival + dwell entry) and gate the gait update while arrived/dwelling.
- **Blocks:** RUNTIME (first-run acceptance; hits the default path-with-dwell/stop-end flow).

## MAJOR — seek with no owned clip still derives role from scrub velocity

- **File:** `advanceFollower` `llactormover.cpp:4559-4562` and `advancePath` `llactormover.cpp:4336-4339`.
- **Failure:** on a seek frame with `!mLocomotion.valid() && !mSecondaryLocomotion.valid()`, `updateLocomotionGait(av, mv, follower_speed, dt, true)` is called with `follower_speed = traveled_step/dt`. A 20 m scrub at 60 fps = 1250 m/s → `desiredLocomotionRole` picks **RUN** on ground; on an air path any seek >0.2 m/s → **FLY**, even when the leader at that playhead is hovering. Role becomes scrub-history-dependent for one frame + a 0.25 s crossfade — contradicting the fix's own "a seek with no owned clip starts a safe fallback role / scrub velocity cannot feed gait choice."
- **Fix:** in the seek-with-no-handles case pass `0` (or the leader's authored speed at that arc) as the speed, not the scrub-derived `follower_speed`.
- **Blocks:** RUNTIME (recorder-seek determinism).

---

## MINOR (non-blocking)
- **AM-m6 edge (primary pending-load):** primary `mPendingLoad` is refreshed only when `mPhaseOrigin < 0` (:831-836); with an operator manual phase ≥0 + uncached asset, a later externally-deactivated primary is never recovered (silent slide). Add an unconditional load-state refresh.
- **AM-m6 edge (purge vs fail):** `findMotion==null` is treated as "failed load" (:750-752), but `purgeExcessMotions` can delete a *healthy* deactivated instance → wrongly quarantined; recoverable via any discontinuity/dwell/arrival restart.
- **N2 (test quality):** the "residual regression assertion" tests the helper in isolation; `llmotioncontroller.cpp` (where ME-m2 lived) isn't compiled into the test, so the 5 capture sites and the W==1 identity/W==0 branches are unguarded. Not vacuous, but weaker than billed.
- **Sub-threshold follower scrub** (<1 m/frame) is still integrated as travel and feeds `d_arc/dt` into role choice on non-seek sync frames — a documented determinism boundary for slow frame-stepping (leader phase is immune). Worth stating, not a bug.
- **N3:** ME-m1 positive-mod range is `[0, cycle]` inclusive under extreme rounding — harmless (wrap + F32 cast collapse it).

## Minimal corrective sequence to reach READY TO BUILD
1. **BLOCKER:** stop resetting `mRole` unconditionally in `updateLocomotionGait` (:570-573) — roll back role on the `startLocalMotion` failure branch, or clear only at deliberate-stop sites and gate the gait while arrived/dwelling. Re-verify dwell holds and stop-end arrivals produce a single stable stand, no per-frame start/stop.
2. **MAJOR:** pass 0 / authored speed (not scrub velocity) in the two seek-with-no-handles branches (:4338, :4561).
3. Optional: the two AM-m6 edge refreshes; strengthen the test (N2) to compile `llmotioncontroller.cpp` and assert the ease-path double-apply + the W==1 identity.

Compile/link is clean; once (1) and (2) land the gate is READY TO BUILD.
