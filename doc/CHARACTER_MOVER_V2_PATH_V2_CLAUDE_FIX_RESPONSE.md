# Character Mover V2 — Path V2 Claude Fix Response

**Date:** 2026-08-29
**Base:** `46a532340cb fix: restore Character Mover V2 build gate`
**Claude review:** `CHARACTER_MOVER_V2_PATH_V2_CLAUDE_REVIEW_RESPONSE.md`
**Gate:** READY FOR RUNTIME ACCEPTANCE

## Correction-block result

Claude's three MAJOR findings were accepted and landed together in one correction block. The precisely specified low-risk hardening was included in that same block. Codex then repeated the static adversarial pass before running one canonical Release build for the viewer and integration test.

## Required findings closed

### F1 — rising helix reverse

`ALPathGeometry::reverse()` now constructs the exact reverse `P(1-u)`:

- moves the primitive center by the plane-rotated local rise vector;
- negates rise;
- reverses signed sweep;
- canonicalizes the new start angle.

`LLActorMover::reversePath()` uses this shared pure operation and copies center, start, sweep, and rise back before reversing event-anchor metadata. A new regression test samples a rotated, rising, multi-turn helix and proves the reversed geometry matches `P(1-u)` across the curve.

### F2 — reverse/save/load angle drift

The reversed start angle is reduced modulo 360 degrees. Near-cap start+sweep combinations therefore remain inside persistence limits, and double reverse remains geometrically equivalent to the original path.

### M1 — omitted primitive center

Schema-2 primitive loading now requires the `primitive_center` key before converting it. An omitted center is rejected rather than manufactured as finite `(0,0,0)`. Explicitly authored finite centers remain valid.

## Included hardening

- `clearPath()` unconditionally cancels the actor's queued Director group-delay start before erasing geometry.
- The missing-geometry runtime guard enters the normal suspend transaction, seeding the suspend anchor and stopping locomotion consistently.
- `setPathShape()` uses `find()`/`emplace()` and does not create an empty waypoint stub for a pathless actor.
- Converting away from a helix clears the airborne state that helix conversion forced.
- Add/Insert/Delete callbacks independently reject playback and generated primitive anchors before creating undo snapshots; the engine guards remain authoritative.
- Primitive Loop Close preserves the nearest integer number of turns instead of collapsing every multi-turn shape to one turn.
- The shared sweep tooltip describes multiple turns for every primitive, not only helices.

## Internal adversarial re-pass

The post-correction static gates proved:

1. exact reverse copies all four changed fields: center, start, sweep, and rise;
2. missing `primitive_center` is rejected before `ll_vector3d_from_sd()`;
3. queued Director start cancellation precedes path erase;
4. `setPathShape()` contains no `mPaths[key]` insertion;
5. missing runtime geometry uses `enterSuspend()`;
6. reverse implements the plane-rotated rise offset, rise negation, and modulo start;
7. `advancePath()` still contains no path-map `operator[]` insertion;
8. all XUI parses and scoped `git diff --check` passes.

## Build and test evidence

One canonical build invocation was run after Claude review and the complete correction block:

```text
cmake --build I:\alchemy-machinima\build-Windows-vs2026-os \
  --config Release \
  --target alchemy-bin INTEGRATION_TEST_alpathgeometry \
  --parallel 8
```

Result:

- viewer build: PASS;
- canonical executable: `build-Windows-vs2026-os/newview/Release/AlchemyTest.exe`;
- manifest/runtime deployment: PASS;
- integration-test build: PASS;
- `INTEGRATION_TEST_alpathgeometry`: 6 total, 6 passed.

The test matrix now covers:

- circle endpoints and analytic length;
- ellipse radii in a rotated plane;
- negative-sweep tangent direction;
- helix endpoint and analytic length;
- full-circle seam position/tangent continuity;
- rotated rising multi-turn helix reverse and double-reverse persistence safety.

The build emitted stale dependency-PDB symbol warnings while relinking the CEF media plugin after an earlier interrupted link. The canonical viewer and test targets both completed successfully; no Path V2 compiler or linker warning was emitted.

## Remaining acceptance work

The code/build gate is clear. Runtime acceptance should now execute the matrix in `CHARACTER_MOVER_V2_PATH_V2_CLAUDE_REVIEW.md`, with special attention to clearing during every playback phase, v1/v2 scene round trips, negative/multi-turn primitives, rotated rising helices, path-camera teardown, and both Director/standalone Path panel hosts.
