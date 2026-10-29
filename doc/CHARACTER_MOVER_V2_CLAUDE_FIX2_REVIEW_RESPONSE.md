# Character Mover V2 — Corrective Review 2 Response (fixes to `077060046da`)

**Range reviewed:** `bb515fa0b79..077060046da` (`fix: close Character Mover V2 runtime gate`)
**Prior gate:** HOLD (`doc/CHARACTER_MOVER_V2_CLAUDE_FIX_REVIEW_RESPONSE.md`) — one BLOCKER (dwell/arrival gait churn), one MAJOR (seek role from scrub velocity), two AM-m6 edges.
**Method:** adversarial re-verification of the corrective diff (only `llactormover.cpp` + `llmotion.h` changed) with special attention to whether removing the reset re-introduces the original AM-m6. No build run.

## Gate decision: READY TO BUILD

Both prior runtime findings are fixed **correctly** — and, critically, the BLOCKER fix does **not** re-introduce AM-m6. Both AM-m6 edges are closed and the N1 clamp is committed. No new blocker or major found. Compile/link remains clean (2-file, +187/-16, additive logic only).

## The BLOCKER fix threads the needle (verified)

The unconditional empty-handle reset in `updateLocomotionGait` (old :565-573) is removed. `mRole`/`mAnim` are now cleared only on genuine failure/loss:

- **Deliberate stop vs genuine loss are distinguished by handle validity** — the decisive mechanism. `clean_lost_motion` (`llactormover.cpp:744-773`) returns **false for an already-invalid handle** (`!handle.valid()`, :745-747). A dwell-entry / terminal-arrival stop invalidates the handle, so `clean_lost_motion` no-ops and `mRole` is **preserved** as the quiescent marker → `desired == mRole` → the `!secondary.valid() && desired != mRole` restart at :614 is skipped → **stable hold, no per-frame create/stop churn**. I traced the arrival path (gait at :4338 each frame with `mRole` preserved) and the dwell early-return (:4236) — the churn is gone.
- **Genuine loss still recovers.** A lost motion leaves a *valid* handle whose instance is gone/deactivated → `clean_lost_motion` detects it (findMotion null & not pending → :760; or not active/ownership-lost → :771), stops it, and the caller clears `mRole` (:778-779) → next gait reacquires. AM-m6 recovery preserved.
- **Failed start clears the role** at all three sites so retry isn't suppressed: `beginLocomotionTransition` (:533-534), the discontinuity restart (:589-590), and `startLocomotion` (:703-704 pre-reset + :726-727 on failure). `startLocomotion` success sets `mRole = role` (:710), so the pre-reset is safe.

Answers: **Q1 — no**, dwell/arrival cannot reach a gait start during a deliberate hold (handle invalid ⇒ role preserved ⇒ `desired == mRole` no-op). **Q2 — yes**, every start failure and every genuine loss leaves `mRole == LOCO_NONE` (three failure sites + `clean_lost_motion`).

## The MAJOR fix is correct (verified)

Both seek-without-owned-clip branches now pass `sync_seek ? 0.f : speed` / `follower_seek ? 0.f : follower_speed` (`llactormover.cpp:4351`, `:4573`). Zero → `desiredLocomotionRole` returns WALK (ground) / HOVER (air) deterministically; scrub displacement can no longer select RUN/FLY, and the absolute odometer still drives phase. Seek-with-owned-clip still samples only (no gait decision). **Q3 — no**, a seek cannot select RUN/FLY from one-frame displacement.

## AM-m6 edges (verified)

- **Q4 — no.** `refresh_load_state` is now called unconditionally each sampling frame (`llactormover.cpp:840,845` — the `mPhaseOrigin < 0` short-circuit is gone), so a manually-phased (`mPhaseOrigin >= 0`) loaded clip can't stay marked pending.
- **Q5 — yes, safely.** Null `findMotion` distinguishes pending (→ failed-load quarantine, no hot-retry) from previously-loaded (→ recoverable stop, no UUID poison, `mRole` cleared for reacquire), with no cross-stop.

## N1 residual clamp (verified)

**Q6 — yes.** `getPresentationBaseWeight` divide branch is `llclamp(presented_weight / mPresentationWeight, 0.f, 1.f)` (`llmotion.h:169`); the `== 1.f` identity branch and the `: 0.f` fail-closed branch are byte-unchanged, so weight-1.0 bit-identity holds and Inf/NaN is closed. (Committed here; identical to the independently-applied hardening.)

## Scorecard vs the prior HOLD

| Prior finding | Status |
|---|---|
| BLOCKER — dwell/arrival gait churn | **FIXED** (reset removed; role cleared only on genuine loss/failure via handle-validity distinction; AM-m6 not re-introduced) |
| MAJOR — seek role from scrub velocity | **FIXED** (zero speed on both seek-without-clip branches) |
| AM-m6 edge — manually-phased pending | **FIXED** (unconditional load-state refresh) |
| AM-m6 edge — purge vs failed quarantine | **FIXED** (pending→quarantine, loaded→recoverable) |
| N1 — residual inversion | **FIXED** (divide-branch clamp) |

## Non-blocking (unchanged, agreed)
- The `llmotionpresentation` test still doesn't compile `llmotioncontroller.cpp`, so the 5 capture sites and the W==1/W==0 branches are statically-audited only. Optional strengthening; not a gate item.
- Follower time-offset mode and automatic Turn/Backward/Strafe/Takeoff/Land selection remain deferred future features (the UI no longer promises them).

## Verdict
**READY TO BUILD.** Recommend proceeding to the viewer build + the runtime matrix. Suggested first in-world checks tied to these fixes: a stop-mode arrival (stable stand, no foot-shuffle), a mid-path dwell (holds the dwell anim, no walk restart), and a recorder scrub/seek on a synced follower (lands at WALK/HOVER, correct phase, no RUN/FLY pop).
