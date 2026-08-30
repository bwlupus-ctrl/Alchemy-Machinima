# Actor Path Exact Corner Fillets — Adversarial Re-Review for Claude

## Assignment

Perform a source-level adversarial review of the exact rounded-waypoint-corner
implementation in the uncommitted `I:\alchemy-machinima` working tree. Review
the code as it exists; do not rely on the implementation summary as proof.

This is a **review-only** pass. Do not edit files. Report every concrete issue as
P0, P1, P2, or P3 with an exact file:line, a reproducible geometry/state case,
and a specific fix. If a claimed problem cannot occur, say why from source.

The comparison base is:

```text
6fbc0fc0efaf3019d241724e9b45948e9a1e463b
```

The working tree contains unrelated VRAM, lighting, camera, and ReShade changes.
Keep this review scoped to the Path files listed below and do not attribute
unrelated dirty-tree changes to this feature.

## Files in scope

- `indra/newview/alpathgeometry.h`
- `indra/newview/llactormover.h`
- `indra/newview/llactormover.cpp`
- `indra/newview/alpanelpatheditor.h`
- `indra/newview/alpanelpatheditor.cpp`
- `indra/newview/llfloaterdirector.cpp`
- `indra/newview/skins/default/xui/en/panel_path_editor.xml`
- `indra/newview/skins/default/xui/en/floater_actor_mover.xml`
- `indra/newview/skins/default/xui/en/floater_director.xml`
- `indra/newview/tests/alpathgeometry_test.cpp`
- `doc/PATH_SYSTEM_UPGRADE_AND_CREATIVE_ROADMAP.md`

## Intended user behavior

A right-angle city-block turn is authored with three ordinary waypoints:
approach, corner pivot, and exit. Select the pivot and set **Corner radius (m)**
above zero. Zero retains the ordinary sharp/tension-mode path.

For a valid positive radius:

1. The traveled route is an exact straight segment, a tangent circular arc, and
   another exact straight segment.
2. The original pivot remains the draggable authoring handle, but is not on the
   traveled route.
3. Dwell, animation, camera, speed override, pose-ghost, and numbered playback
   marker semantics occur at the arc midpoint.
4. Oversized radii clamp so no fillet consumes more than 49% of the shorter
   adjacent leg.
5. Tighter turns reduce travel speed. Braking begins on the incoming straight
   and acceleration continues on the outgoing straight using a continuous,
   deterministic distance envelope.
6. Open paths round interior nodes only. Loop paths may round every node,
   including node 0 at the seam.
7. Any valid rounded corner changes the waypoint route to exact line/arc mode;
   the historical Catmull-Rom tension slider is disabled in that mode.

## Implementation map and required audit

### 1. Fillet geometry

Primary code:

- `alpathgeometry.h:46` — `CornerFillet`
- `alpathgeometry.h:141` — `buildCornerFillet`
- `alpathgeometry.h:208` — exact arc evaluation
- `alpathgeometry.h:217` — exact unit tangent

Verify independently:

- Incoming direction is `normalize(corner - prev)` and outgoing direction is
  `normalize(next - corner)`.
- For turn angle `theta`, tangent distance is `r * tan(theta / 2)`.
- Entry and exit lie on their respective authored legs.
- The center construction works for left turns, right turns, and arbitrary 3D
  planes; the arc ends exactly at exit and has the correct outgoing tangent.
- Radius clamping preserves a straight gap and cannot create overlap between
  neighboring rounded nodes.
- Straight, almost-straight, U-turn, almost-U-turn, coincident, very short,
  non-finite, acute, and obtuse inputs cannot produce NaN/Inf or an inverted arc.
- The thresholds at 0.001 m, 0.02 m, and 0.5 degrees have sensible boundary
  behavior and do not mutate the output on failure.

### 2. Route compiler and loop seam

Primary code:

- `llactormover.h:165-181` — compiled-piece/cache representation
- `llactormover.cpp:1095` — `compileCornerPieces`
- `llactormover.cpp:1199` — segment count dispatch
- `llactormover.cpp:1221` — exact piece evaluation
- `llactormover.cpp:1278` — rebuild entry
- `llactormover.cpp:1333-1451` — bounded arc-length table and logical node
  distances

Audit these contracts:

- With no valid rounded corner, historical waypoint evaluation remains exactly
  on the old tension-blended path.
- With at least one valid rounded corner, the entire route is represented by
  exact line/arc pieces and unrounded nodes remain sharp line junctions.
- Open path endpoints cannot be rounded.
- Every logical node receives exactly one usable `mNodeDist`; rounded nodes map
  to the split arc's midpoint rather than entry, exit, or unreachable pivot.
- Loop node 0 starts at the midpoint of its seam fillet, traverses the second
  half first, and closes through the first half without a missing or doubled arc.
- One, two, and many adjacent fillets cannot create zero-length, reversed, or
  disconnected compiled pieces after independent 49% clamps.
- The global sample budget remains bounded at the maximum node count even though
  one rounded node creates line plus two half-arc pieces.
- Reverse, mirror, copy, undo/redo, loop-close, node insertion/deletion, and node
  dragging preserve or deliberately recompute all corner geometry and metadata.

### 3. Corner speed profile

Primary code:

- `llactormover.cpp:948` — authored speed evaluation
- `llactormover.cpp:974` — corner envelope application
- `llactormover.cpp:1375` — compiled-piece distance bounds
- `llactormover.cpp:1454` — `cornerSpeedLimitAt`
- `llactormover.cpp:3170-3210` — duration integration; inspect the current
  source around the `pathSpeedAt` call rather than trusting this approximate
  range
- `llactormover.cpp:5030-5060` — live distance advancement; inspect current
  lines because this range may shift

The intended equations are:

```text
v_corner = max(0.2, sqrt(2.5 * effective_radius))
v_limit(distance) = sqrt(v_corner^2 + 2 * 2.5 * distance_to_arc)
```

The minimum envelope across all arc pieces is applied. Verify:

- The speed command is continuous at entry and exit tangent points.
- Braking happens before arc entry, not only after the actor is already turning.
- The two half-pieces for one corner do not incorrectly double-reduce speed.
- Consecutive corners select the more restrictive reachable envelope.
- Loop seam distance uses the shortest circular distance without producing a
  negative distance or discontinuity at `d == 0` / `d == totalLength`.
- Forward and ping-pong/reverse traversal are safe under the symmetric envelope.
- The duration readout uses the same speed function as normal playback.
- Node speed overrides, ease-in/out, dwell, and corner limits compose in the
  intended order.
- Sync-to-take intentionally derives distance from the camera playhead and thus
  cannot obey an independent corner-speed schedule. Flag this only if the UI or
  documentation falsely promises otherwise.

### 4. Persistence and mutation safety

Primary code:

- `llactormover.cpp:1740` — schema 4 save
- `llactormover.cpp:1770-1780` — per-node `corner_radius` save
- `llactormover.cpp:1795-1805` — schema bounds
- `llactormover.cpp:1925-1932` — schema-aware load
- `llactormover.cpp:2440` — setter and rebuild

Verify:

- Schema 4 round-trips finite radii and schemas 1-3 load exactly as radius zero.
- Missing, negative, NaN, Inf, and extreme hand-edited values are safe.
- The setter rejects primitives, active walks, invalid indices, and non-finite
  values before mutation, clamps the persisted value, and rebuilds immediately.
- Scene-load, node-drag, and structural-edit paths cannot leave stale compiled
  pieces, arc samples, node distances, duration, or guide geometry.
- Whole-`Waypoint` copies really preserve radius through every history/copy path.

### 5. UI and visualization

Primary code:

- `panel_path_editor.xml:169` — selected-node radius spinner
- `alpanelpatheditor.cpp:85,152` — binding and commit registration
- `alpanelpatheditor.cpp:1566` — validation, undo, commit
- `llactormover.cpp:13024` — playback marker/ghost/camera event location
- `llactormover.cpp:13368` — edit-pivot tether and traveled-route marker

Verify:

- The spinner is enabled only for stopped waypoint paths and only for eligible
  interior/open or all/loop nodes.
- Invalid geometry restores the old displayed value and does not create an undo
  entry or mutate the path.
- A successful edit creates one coherent undo step.
- Tension mode is clearly disabled/explained whenever exact line/arc routing is
  actually active.
- The edit pivot remains selectable/draggable while the route marker, pose ghost,
  dwell/camera event, and playback marker use the traveled midpoint.
- Render state remains balanced and the additional tether/marker does not leak
  line width, shader, matrix, depth, or blend state.

## High-value adversarial scenarios

At minimum, reason through or test:

1. Flat 90-degree left and right turns with known entry/exit coordinates.
2. Tilted 3D turn whose three points share no horizontal plane.
3. Acute and obtuse corners near both rejection thresholds.
4. Two consecutive 1 m legs with both radii authored absurdly large.
5. Alternating left/right city-block corners.
6. A closed rectangle with all four corners rounded, including node 0.
7. Node 0 rounded in a loop, then reverse twice and mirror twice.
8. A valid radius followed by dragging a neighbor until the turn becomes
   degenerate.
9. Dwell + animation + node camera on a rounded pivot.
10. High authored speed approaching one corner, two close corners, and the loop
    seam in both traversal directions.
11. Schema 3 load, schema 4 round-trip, and malformed schema 4 values.
12. Maximum-node-count path with many rounded nodes and the strict sample budget.

## Specific ambiguity/hotspot checks

Please give an explicit verdict on these rather than assuming they are harmless:

- `compileCornerPieces` falls back to the historical tension route if positive
  radii exist but all current turns are invalid. Check whether the UI disables
  tension based on authored positive radius or actual compiled mode, especially
  after a node drag or malformed scene load.
- Each fillet is stored in two half-arc pieces. Confirm the minimum speed-envelope
  calculation is equivalent to one full-arc interval on open paths and loops.
- Independent 49% per-corner clamps are intended to leave at least a 2% straight
  gap between adjacent fillets on their shared leg. Verify this construction for
  unequal leg lengths and acute/obtuse turns.
- Logical event distances for loops are not naturally ordered if node 0 is not
  distance zero. The implementation intentionally places the rounded seam's
  midpoint at distance zero. Verify every consumer is compatible with that.

## Known out-of-scope design decisions — do not reclassify as corner regressions

Two earlier review items remain deliberately unimplemented until the user chooses
a policy:

1. Live cadence editing while walking can re-phase the foot cycle. The recommended
   choice remains disabling cadence edits while walking.
2. Edit-mode node markers remain depth-tested. The recommended choice remains
   depth-tested route ribbons with always-on-top edit markers/highlights.

You may mention interactions, but do not ask Codex to silently choose either
policy as part of this corner review.

## Verification already completed

On 2026-08-30:

- `INTEGRATION_TEST_alpathgeometry` built in Release.
- `ctest -C Release -R INTEGRATION_TEST_RUNNER_alpathgeometry` passed, 1/1.
- The passing group contains 15 numbered tests. Tests 13-15 cover exact 90°
  geometry, tangency, radius clamping, arbitrary 3D orientation, circularity,
  planarity, and a 60° guard for `radius * tan(turn/2)`
  (`alpathgeometry_test.cpp:291-392`).
- All 64 `ALPanelPathEditor::getChild()` bindings resolve in
  `panel_path_editor.xml`.
- `panel_path_editor.xml`, `floater_actor_mover.xml`, and `floater_director.xml`
  parse successfully and have no duplicate `name` attributes.
- Scoped `git diff --check` returned no whitespace errors (one pre-existing CRLF
  conversion warning for `llfloaterdirector.cpp`).
- The full Release target compiled the affected viewer translation units, linked
  `AlchemyTest.exe`, and completed the viewer-manifest copy successfully.

Runtime QA remains pending; the binary link is complete.

## Required response format

Return:

1. Overall verdict: SOUND, SOUND WITH P3 NOTES, or NEEDS REMEDIATION.
2. Findings ordered P0 → P3, each with file:line, proof, user-visible impact, and
   concrete correction.
3. A separate table marking each audit area above PASS / FAIL / NOT PROVEN.
4. Any missing test that would have caught a real risk.
5. A concise runtime QA list for the newly linked client.

If no P0-P2 issue is found, state that explicitly. Do not invent stylistic
findings to fill the report.
