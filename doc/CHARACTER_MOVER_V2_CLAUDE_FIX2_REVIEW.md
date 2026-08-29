# Character Mover V2 — Corrective Review 2

**Review range:** `bb515fa0b79..HEAD`
**Prior review:** `doc/CHARACTER_MOVER_V2_CLAUDE_FIX_REVIEW_RESPONSE.md`
**Requested gate:** confirm **READY TO BUILD** or report only reproducible remaining blockers/majors.

No build has been run after this correction. This is intentionally another pre-build adversarial gate.

## Scope of this correction

This block addresses both runtime findings in the prior HOLD response, both AM-m6 recovery edges, and the N1 residual clamp already called out as fixed.

### 1. Dwell and arrival no longer restart gait every frame

The unconditional empty-handle reset in `updateLocomotionGait()` was removed. Empty handles are not sufficient evidence of failure: dwell entry and terminal arrival deliberately stop both gait sides while preserving the selected role as the quiescent-state marker.

Role/animation state is now cleared only by failure or genuine lifecycle loss:

- failed primary start in `beginLocomotionTransition()`;
- failed discontinuity restart in `updateLocomotionGait()`;
- every explicit `startLocomotion()` transaction before resolution/start, including initial start, dwell resume, suspension resume, and migration;
- external stop, ownership loss, controller purge, or failed asset cleanup in `updateLocomotionSample()`.

Expected invariant:

- deliberate dwell/arrival stop + matching desired role = no transition;
- failed start/lost ownership + role NONE = deterministic reacquisition, subject to failed-asset quarantine.

### 2. Seek role selection no longer consumes scrub velocity

Both seek-without-owned-clip branches now pass zero speed to `updateLocomotionGait()`:

- recorder-synced authored paths;
- followers seeking to the leader arc.

Zero selects the deterministic safe fallback, WALK on ground or HOVER in air. The absolute odometer remains the target arc, so phase is still deterministic. Existing owned clips continue to be sampled directly without a gait decision.

### 3. Pending-load and purge recovery are separated

`mPendingLoad` is refreshed every sampling frame, independent of whether automatic phase discovery is enabled. A manually configured nonnegative phase therefore cannot leave a loaded clip permanently marked pending.

When `findMotion()` returns null:

- pending handles retain failed-load quarantine behavior;
- previously loaded handles are treated as recoverable controller purge/lifecycle loss, cleared without poisoning the source UUID, and allowed to reacquire.

### 4. Residual recovery is range-safe

`LLMotion::getPresentationBaseWeight()` clamps the divided base weight to `[0,1]`. The exact `mPresentationWeight == 1.f` identity branch and zero-weight fail-closed branch are unchanged.

## Adversarial state audit

| Scenario | Required result | Source result |
|---|---|---|
| Dwell entry, same frame | both gait handles stay stopped | role remains selected; `desired == mRole`; no transition |
| Repeated dwell frames | no gait work/churn | dwell branch returns before gait update |
| Dwell exit | one explicit restart | `startLocomotion()` starts once, then clears dwell |
| Stop-mode arrival, subsequent frames | stable stand, no per-frame create/stop | handles empty but role preserved; gait is a no-op |
| Initial/resume/migration start failure | retry state is not suppressed | explicit restart transaction leaves role NONE; bad UUID quarantined |
| External stop/ownership loss | recover without cross-stop | cleanup clears owned token/role; simulator-signaled motion remains alive |
| Healthy loaded instance purged | reacquire, no false quarantine | refreshed pending=false selects recoverable-loss path |
| Failed pending asset removed | quarantine, no hot retry loop | pending=true selects failed-load cleanup |
| Large synced seek with existing clip | sample absolute phase only | no gait selection |
| Large synced seek without clip | deterministic WALK/HOVER fallback | gait receives zero speed |
| Normal playback | speed-dependent WALK/RUN or HOVER/FLY | non-seek speed remains unchanged |

## Static verification completed

- `git diff --check`: passed; only unrelated existing CRLF warnings were printed.
- Reviewed every gait-side `stopLocalMotion()`, role clear, and animation clear call site.
- Confirmed no unconditional “both handles empty => role NONE” logic remains.
- Confirmed neither seek-without-handle branch passes scrub-derived velocity.
- Confirmed primary and secondary pending-load refreshes are unconditional.
- Confirmed the change is isolated to:
  - `indra/newview/llactormover.cpp`
  - `indra/llcharacter/llmotion.h`
  - the prior review response and this review brief.

## Review questions

1. Can dwell entry or stop-mode arrival still reach a branch that starts a gait while the deliberate hold is active?
2. Does every start failure that previously relied on the removed unconditional reset now leave `mRole == LOCO_NONE`?
3. Can a recorder/follower seek still select RUN or FLY solely from one-frame arc displacement?
4. Can a loaded, manually phased clip remain incorrectly marked pending?
5. Is a null canonical motion safely distinguishable using the last refreshed pending state without creating a cross-stop or retry loop?
6. Does the residual clamp preserve exact identity at presentation weight 1 and fail closed at 0?

## Known non-blocking boundary

The existing `llmotionpresentation_test` exercises the residual helper but does not compile the full `llmotioncontroller.cpp` capture pipeline. The five controller call sites were statically re-audited, but that integration-test strengthening remains optional because pulling the controller into the narrow llcharacter test target adds a large dependency surface unrelated to these runtime blockers.

Follower time-offset mode and automatic Turn/Backward/Strafe/Takeoff/Land role selection remain explicit future features, not regressions in this correction.
