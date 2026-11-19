# Corner-Fillet System — Adversarial Review Findings & Fix Brief (for Codex)

**Reviewed:** the uncommitted rounded-waypoint-corner (fillet) feature in `I:\alchemy-machinima` (working tree vs `6fbc0fc0efa`): `buildCornerFillet`, `compileCornerPieces`/loop-seam, brake/accel speed envelope, arc-midpoint event relocation, schema-4 persistence, the Corner-radius UI.
**Method:** 3 read-only passes (geometry+compiler; speed+events+persistence; UI) — two Opus, one Sonnet. No build/run.
**Verdict:** **SOUND — one P1, otherwise P3 only.** The math and engine are clean; the single real defect is a UI state-consistency bug.

## Codex remediation status — 2026-08-30

**A1 FIXED.** `alpanelpatheditor.cpp:469-481` now rebuilds a dirty Path before
deriving UI mode and gates `exact_corners` on the engine's compiled
`!path->mCornerPieces.empty()` state. The tension enabled-state and tooltip now
change with the route actually being evaluated, including immediately after a
node drag invalidates the last fillet. The tension commit guard was also changed
to the rebuilt compiled state (`alpanelpatheditor.cpp:1658-1677`); otherwise the
slider would have looked enabled while silently rejecting the edit.

**C1/C2 FIXED.** `cornerRadiusEligible` (`alpanelpatheditor.cpp:46-57`) is the
single enable/commit predicate and requires at least three nodes. A two-node loop
therefore no longer presents an enabled corner control that can only reject.

**B1 PARTIALLY CLOSED.** Geometry test 15
(`alpathgeometry_test.cpp:367-392`) uses a 60-degree turn and asserts effective
radius, tangent distances, and sweep, so the formerly invisible
`radius * tan(turn/2)` regression now fails. The larger Path/UI integration
fixture remains recommended P3 work; it requires a viewer-level test seam rather
than duplicating production logic in the header-only geometry test.

**C3/C4 intentionally unchanged.** Edit-marker depth remains the prior open UX
decision; open-endpoint radius persistence remains harmless per-node authored
state that can become active if the path is later looped.

Verification after remediation:

- Release `INTEGRATION_TEST_alpathgeometry` built and all 15 tests passed.
- Full Release `alchemy-bin` compiled, linked `AlchemyTest.exe`, and completed
  viewer-manifest copying successfully.
- Final scoped XUI/binding/whitespace checks are recorded in
  `doc/PATH_SYSTEM_CORNER_FILLET_REMEDIATION_FOR_CLAUDE.md`.

Confirmed sound from source (with algebra / numeric cases, not the doc): entry/exit use **normalized** legs (`alpathgeometry.h:161-162` — the flagged "missing normalization P0" does NOT exist); tangent distance `r·tan(turn/2)` and clamped `effective_radius` are correct; center/tangency/circularity hold for left, right, and tilted-3D corners; degenerate turns (straight/U-turn/short/coincident/non-finite) are rejected without mutating output; the **49% overlap clamp** guarantees a ≥2% straight gap on a shared leg (both corners clamp to 0.49·shared-leg independently); the loop seam splits node-0's fillet into two halves with `mNodeDist[0]==0`, no dup/reverse; sample budget stays bounded (≤~49k < 65,536). Speed: the two-half-arc `min` envelope is provably equal to a single full-arc interval (**no double-slowdown**), brakes *before* the entry, stays continuous at the tangent points, uses shortest-circular distance at the loop seam, is a **min-cap not an override**, and duration integrates the **same** `pathSpeedAt` as playback. Every rounded-node event consumer (dwell/anim/node-camera/speed/pose-ghost/marker) reads the arc-midpoint `mNodeDist`; the pivot stays the draggable handle with a tether. `mCornerRadius` round-trips with **no gap** across save/load/copy/undo/redo/follower/reverse/mirror; schemas 1-3 load as radius 0; malformed/missing/NaN/Inf clamp to [0,10000]; the setter guards before mutating and rebuilds immediately; render state is RAII-balanced.

---

## A. Fix directly

### A1 (P1) — Tension slider stays disabled when the route has silently reverted to the tension spline
- **Where:** `indra/newview/alpanelpatheditor.cpp:462-469` computes `exact_corners` by scanning the **authored** field `node.mCornerRadius > 0.001f`; `:480` `mTension->setEnabled(have_actor && !primitive && !walking && !exact_corners)`; the tooltip at `:481-483` asserts "exact straight/arc geometry" on the same authored-value basis.
- **Why it's wrong:** the *engine* decides routing mode from the **compiled** state `Path::mCornerPieces` (`llactormover.h:181`). `compileCornerPieces` (`llactormover.cpp:1095-1130`) populates it only when at least one fillet actually validates; if every rounded node's fillet is currently degenerate it returns early leaving `mCornerPieces` empty, and `segmentCount()`/`evalSegment()` (`:1201`, `:1223`) fall back to the historical Catmull-Rom **tension** blend. Every engine consumer checks `mCornerPieces.empty()` for "is exact mode active" (`:1201`, `:1223`, `:1335`, `:1456`) — the UI is the one place that reads the authored field instead.
- **Repro:** 3-node open path → round the interior node with a valid radius (tension correctly disabled) → drag a neighbor until the turn is degenerate (collinear, or too short for the 49% clamp). `moveWaypoint` (`llactormover.cpp:2337-2353`) writes only position, never `mCornerRadius`, so the next `rebuild()` finds no valid fillet and walks the tension route — but `exact_corners` is still `true`, so the tension slider stays disabled and the tooltip lies. The user cannot re-enable tension without zeroing the radius.
- **Fix:** gate on the compiled state, not the authored value. After ensuring the path is rebuilt (mirror the dirty-check-then-rebuild that `getPathStats()` uses at `llactormover.cpp:3199-3203` — `getPath()` at `:1695` is a plain const lookup and won't force a rebuild), set:
  ```cpp
  const bool exact_corners = path && !primitive && !path->mCornerPieces.empty();
  ```
  Keep the tooltip text but let it follow the same `exact_corners`. This makes the UI agree with what the actor is actually walking, and re-enables tension the moment the fillets go invalid.

---

## B. Recommended — close the integration test gap (P3, both Opus passes + UI pass converged on this)

The suite (`indra/newview/tests/alpathgeometry_test.cpp`, tests 13/14 at `:291-366`) exercises only the pure `buildCornerFillet` primitive. Nothing tests the engine or UI integration. Add a `Path`-level fixture asserting:
1. **The `tan` factor at a general angle** — test 13 is a 90° turn where `tan(45°)=1`, so `td`, `r·tan(turn/2)`, `r/tan(turn/2)`, and a bare `td=r` all collapse to the same number; test 14 never asserts `mRadius`. A regression dropping the `tan` factor (wrong radius at every non-90° corner) would pass today. Add `ensure_distance("effective radius", fillet.mRadius, EXPECTED, 1e-6)` at an acute or obtuse corner with a known radius.
2. **`cornerSpeedLimitAt` continuity + no double-reduce** — assert continuity at `SA`/`MA`/`EA` and that the two-half `min` equals a single-arc envelope (guards against a future measure-from-arc or double-slowdown regression).
3. **`mNodeDist` at the arc midpoint** for a rounded interior node, and `0` for a loop seam.
4. **Schema-4 round-trip + reverse/mirror** preserve `mCornerRadius`.
5. **The A1 regression** — author a valid radius, invalidate it via a neighbor move, and assert `path->mCornerPieces.empty()` matches the panel's tension-enable state. This test would have caught A1 immediately.
6. The **49% overlap clamp** on a shared leg (two consecutive corners) and the **loop-seam split** — currently verified only by hand.

---

## C. Minor (P3 — batch)

- **C1 — Duplicated eligibility predicate.** The corner-node eligibility test is hand-duplicated at `alpanelpatheditor.cpp:402-404` (`corner_node`) and `:1572-1575` (`eligible`); currently consistent, but factor into one helper to prevent future enable/commit drift.
- **C2 — 2-node loop enables the spinner but always rejects.** Eligibility (`:402-414`/`:1572-1575`) doesn't require `mNodes.size() >= 3`, but `compileCornerPieces` has an `n<3` guard (`llactormover.cpp:1099`) so any radius is rejected with the "cannot be rounded" alert. Harmless but confusing — add the `>=3` condition to eligibility.
- **C3 — In-world visual check for a render-state behavior change.** The `renderActorPathOverlay` state setup swapped `LLGLSUIDefault` (depth-test off, always-on-top) for an explicit `LLGLDepthTest(depth_aware, false, GL_LEQUAL)` (`llactormover.cpp:~13296-13299`) — this rides along with the tether work and now depth-tests the edit overlay. Plausibly intentional (matches the depth-tested-ribbon design), but it interacts with the **still-open** prior decision B2 (edit-mode markers always-on-top). Confirm in-world once linked; resolve together with B2.
- **C4 — Setter accepts endpoint radius the compiler ignores** (`llactormover.cpp:2445` vs the interior-only loop `:1106`): a radius stored on an open-path endpoint persists but is inert until the path becomes a loop. Harmless / arguably intended (radius is per-node); no route/NaN risk. Leave as-is or document.

---

## D. Not corner defects — do not touch here
- The two prior Path decisions remain open by the user's choice: live-cadence foot-pop (recommend gating the cadence slider `!walking`) and edit-mode marker depth (recommend always-on-top edit markers over depth-tested ribbons). C3 above should be resolved together with the second one.

---

## E. Verification after the fix
1. **Completed:** the full Release target linked and copied the updated viewer/runtime assets.
2. **Completed:** `INTEGRATION_TEST_alpathgeometry` built and all 15 tests passed, including the new 60-degree regression.
3. **Pending in-world QA** (from `doc/PATH_SYSTEM_CORNER_FILLET_REVIEW_FOR_CLAUDE.md` scenarios), emphasizing: **A1** — round a corner, drag a neighbor to degeneracy, confirm the tension slider re-enables, its tooltip corrects, and a tension edit commits; flat 90° L/R brake-before-entry; tilted 3-D corner; rounded loop seam forward + ping-pong (no jump/stall at `d=0≡totalLength`); two close corners share the more restrictive cap; dwell/anim/camera on a rounded pivot fire at the midpoint marker; schema-3 load + schema-4 round-trip + a hand-edited NaN/1e9 radius.
