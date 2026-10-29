# Corner-Fillet Remediation — Claude Verification Brief

## Assignment

Re-review the remediation of the findings in
`doc/PATH_SYSTEM_CORNER_FILLET_FIXES_FOR_CODEX.md` against the current
uncommitted `I:\alchemy-machinima` source. This is a read-only review: do not
edit files. The comparison base remains
`6fbc0fc0efaf3019d241724e9b45948e9a1e463b`, but keep the review scoped to the
Path files because the working tree contains unrelated systems.

Return **SOUND**, **SOUND WITH P3 NOTES**, or **NEEDS REMEDIATION**. For every
remaining defect, provide severity, exact file:line, source proof, a concrete
reproduction, and a specific correction.

## Remediation claims to verify

### 1. A1/P1 — compiled route state now owns the tension UI

Inspect `indra/newview/alpanelpatheditor.cpp`:

- `:469-477` checks `path->mDirty`, obtains the existing editable Path, rebuilds
  it, and refreshes the pointer before reading compiled state.
- `:480-481` derives `exact_corners` from
  `!path->mCornerPieces.empty()`, not from authored `mCornerRadius` values.
- `:492-497` uses that same result for both tension enabled-state and tooltip.
- `:1658-1677` rebuilds if needed and gates the tension **commit handler** on
  `!p.mCornerPieces.empty()` as well. It no longer silently rejects an enabled
  tension control merely because an inert authored radius remains positive.

Required source trace:

1. Start with a valid rounded three-node open path.
2. Move a neighbor so the only fillet becomes collinear, almost straight,
   U-turn-like, or too short.
3. `moveWaypoint` marks the path dirty without deleting the authored radius.
4. The next panel refresh rebuilds; `compileCornerPieces` produces no valid
   pieces and the engine reverts to the tension route.
5. The tension slider must be enabled, its tooltip must describe tension, and a
   committed tension value must mutate `mTension` plus mark the path dirty,
   despite the inert authored radius remaining visible.
6. Move the node back to valid geometry; rebuilding must repopulate compiled
   pieces and disable tension again.

Check for stale-pointer use, accidental path creation, rebuild-every-frame cost,
or disagreement between tooltip, UI enablement, preview, duration, and playback.

### 2. C1/C2 — one eligibility rule and a real minimum node count

Inspect `alpanelpatheditor.cpp:46-57`, `:413-414`, and `:1583-1584`.

Verify `cornerRadiusEligible` is the only predicate used by both enablement and
commit, and requires:

- a real non-primitive path;
- stopped playback;
- at least three nodes;
- an in-range selected node;
- any node for a loop, but only interior nodes for an open path.

Specifically verify a two-node loop keeps the spinner disabled and cannot enter
the commit path, while a three-node loop permits all three nodes.

### 3. B1/P3 — non-right-angle geometry regression added

Inspect `indra/newview/tests/alpathgeometry_test.cpp:367-392`.

The new 60-degree case must independently prove:

- requested effective radius remains 2 m;
- entry and exit tangent distances equal `2 * tan(30 degrees)`;
- sweep equals 60 degrees.

Confirm this test would fail if the production code replaced
`radius * tan(turn/2)` with a bare radius or inverted the tangent factor. Confirm
it calls the production `buildCornerFillet` rather than reimplementing the math.

## Items deliberately not claimed fixed

Do not treat these as hidden remediation claims:

- Path-level automated fixtures for `cornerSpeedLimitAt`, midpoint `mNodeDist`,
  schema-4 round-trip, loop seam compilation, and UI enablement remain a P3 test
  gap. The current integration test target is intentionally header-only; do not
  accept a duplicated test-only compiler as equivalent production coverage.
- Edit-overlay node depth remains the earlier open UX decision. Do not silently
  change it during this review.
- An authored corner radius on an open endpoint remains inert but persisted, so
  it can become active if the path is later looped. This is intentional and safe.
- Live cadence editing remains the separate prior UX decision.

## Verification evidence to audit

On 2026-08-30:

- Release target `INTEGRATION_TEST_alpathgeometry` built successfully.
- `ctest --test-dir build-Windows-vs2026-os/newview -C Release -R
  INTEGRATION_TEST_RUNNER_alpathgeometry --output-on-failure` passed 1/1. The
  passing TUT group contains 15 numbered tests.
- Full Release target `alchemy-bin` compiled the remediated
  `alpanelpatheditor.cpp`, completed whole-program optimization, linked
  `build-Windows-vs2026-os/newview/Release/AlchemyTest.exe`, and completed the
  viewer-manifest copy with exit code 0.
- The final static checks should show all three Path XUI files parse, no duplicate
  XUI names, all 64 panel `getChild()` bindings resolve, and scoped
  `git diff --check` has no whitespace errors.

## Required response

1. Mark A1, C1, C2, and the 60-degree regression **FIXED**, **PARTIAL**, or
   **NOT FIXED**, with file:line proof.
2. State explicitly whether any P0-P2 regression was introduced by rebuilding
   the path during UI refresh.
3. List only real remaining findings; keep the acknowledged Path-level test gap
   at P3 unless source analysis exposes an actual correctness defect.
4. Provide a short in-world QA list emphasizing valid → degenerate → valid corner
   transitions and three-node loop eligibility.
