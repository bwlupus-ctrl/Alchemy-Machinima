# Character Mover V2 — Path V2 Adversarial Review Handoff

**Date:** 2026-08-29
**Candidate base:** `46a532340cb fix: restore Character Mover V2 build gate`
**Review scope:** the cohesive, currently uncommitted Path V2 block listed below
**Gate status:** **HOLD — CLAUDE FINAL REVIEW REQUIRED BEFORE BUILD OR COMMIT**

## 0. Required gate order

1. Review the complete cohesive Path V2 diff and this handoff.
2. Return all findings together, ordered by severity.
3. Codex applies accepted findings as one cohesive correction block and repeats its internal adversarial pass.
4. Only after Claude clears the candidate: run one canonical Release build and the validation suite.
5. Stage only the scoped Path V2 files and create one implementation commit.

Do not build, commit, or split this candidate into incremental micro-patches during Claude review.

## 1. Review objective

Attack this as a production camera/path-authoring change, not as a cosmetic UI patch. The candidate is intended to close the user-visible Path V2 gap that the earlier handoff explicitly deferred:

- play or stop the selected actor's authored path without firing the Director `ACTION` macro;
- delete a playing path without recreating empty geometry, moving the actor toward global origin, or leaving the path camera latched;
- author exact circle/arcs, ellipse/arcs, and circular helices from the Path panel;
- persist those controls with an explicit schema migration;
- preserve timing, dwell, animation, and camera events through exact-primitive conversion;
- keep edits undoable and reject structurally unsafe edits during playback.

Do not accept the candidate based only on compilation. Prove the lifecycle, seam, migration, coordinate-frame, and camera finiteness invariants below.

## 2. Files in this review block

- `indra/newview/alpathgeometry.h`
- `indra/newview/tests/alpathgeometry_test.cpp`
- `indra/newview/CMakeLists.txt`
- `indra/newview/llactormover.h`
- `indra/newview/llactormover.cpp`
- `indra/newview/llpathcamera.cpp`
- `indra/newview/alpanelpatheditor.h`
- `indra/newview/alpanelpatheditor.cpp`
- `indra/newview/skins/default/xui/en/panel_path_editor.xml`
- `indra/newview/skins/default/xui/en/floater_actor_mover.xml`
- `indra/newview/skins/default/xui/en/floater_director.xml`

The worktree contains unrelated Cine Light, render, shader, ReShade, settings, and menu changes. They are outside this review and must not be included in the Path V2 commit.

## 3. Root cause and deletion fix

The old `clearPath()` erased `mPaths[key]` while an active `Move` still had `mIsPath == true`. On the next frame, `advancePath()` used `mPaths[avatar_id]`, which silently manufactured a default empty path. Degenerate evaluation could then produce global origin, while the path-camera source still believed it owned the camera. This explains the reported zoom-to-space state and inability to escape it.

The candidate changes both sides of the invariant:

1. `clearPath()` stops the active path transaction before erasing geometry.
2. `advancePath()` uses `find()`, never `operator[]`; missing/invalid geometry suspends at the live rendered root and releases locomotion/camera ownership.
3. scene replacement and undo/redo replacement also stop an active path before swapping geometry.
4. the confirmation callback stores the actor UUID in its notification payload, so changing the selected actor while the dialog is open cannot clear the wrong path.

Adversarial cases: clear during ordinary playback, dwell, ping-pong reversal, loop seam, path-camera interpolation, camera preview, follower playback, and after the selected cast member changes while the confirmation is open.

## 4. Exact geometry model

`ALPathGeometry::Primitive` is a pure header-level evaluator shared by runtime and tests. It stores:

- type: waypoints, circle/arc, ellipse/arc, or helix;
- global center;
- radius X and radius Y;
- start angle and signed sweep;
- plane yaw, pitch, and roll;
- helix rise.

Circle/arc and circular helix use analytic length and direct distance-to-parameter inversion. Ellipse/arc uses the existing bounded adaptive arc table because ellipse arc length has no elementary closed form. Negative sweeps reverse travel. Plane rotation is applied to both evaluated points and tangents.

A primitive spans the whole authored path. Existing `mNodes` remain evenly parameterized event anchors for root height, dwell, speed override, animation, yaw offset, and per-node camera metadata; their positions are regenerated from the exact primitive. Conversion back to waypoints freezes those generated positions.

### Deliberate boundary

This is a complete user-visible whole-path primitive workflow. It is **not** a heterogeneous span graph that mixes Catmull–Rom, line, circle, ellipse, and helix spans inside one path, and it does not implement automatic least-squares shape recognition. Those are not claimed here. Claude should flag any code or UI text that implies mixed-span composition.

## 5. Persistence and migration

Path scene data now writes schema `2` and includes all ten primitive fields. Loading accepts schemas `1` and `2`; schema `1` remains a waypoint path. Schema `2` validates finite center/camera data, clamps scalar ranges, normalizes camera rotations, rebuilds generated anchors, and clears edit history after replacement.

Loaded loop semantics are sanitized:

- looped circle/ellipse/zero-rise helix sweeps are rounded to a nonzero integer number of turns so the seam closes;
- rising helices cannot loop and degrade to ping-pong;
- helices force airborne movement and disable ground-follow;
- circle and helix force equal radii.

Attack malformed LLSD: missing/unknown schema, too many nodes, NaN/Inf values, invalid shape/end-mode integers, zero/negative radii, huge sweeps, nonfinite camera quaternion/FOV, a rising looped helix, and a partial arc marked as Loop.

## 6. Editor and transport behavior

The Path panel now exposes `Play Path`, `Stop Path`, shape selection, `Center on actor`, radii, start/sweep, plane yaw/tilt/roll, and rise. `Walk to Point...` remains a separate destructive two-point workflow and its tooltip says that it replaces the authored path.

Playback and primitive geometry are mutually exclusive with waypoint edit mode. Runtime methods also reject append/insert/move/delete during playback or on generated primitive anchors, so stale/programmatic UI callbacks cannot bypass the visual disable state. Shape, primitive, node, and path-wide edits participate in undo/redo.

Loop safety is enforced both when End Mode changes and when primitive controls change later. Editing a looped sweep retains an integer-turn seam; giving a looped helix nonzero rise changes it to ping-pong.

Attack rapid actor switching, double-clicks, keyboard/programmatic commits on disabled controls, preview-to-play transitions, playback-to-clear transitions, undo after conversion, redo after conversion, copy-to overwrite, and conversion among every pair of shape types.

## 7. Camera safety

Per-node camera authoring rejects nonfinite position, rotation, and FOV, normalizes rotation, and clamps FOV. `LLPathCamera::updateCamera()` revalidates the global pose and the converted agent position/basis before writing `LLViewerCamera`. Invalid data keeps the last valid frame instead of poisoning render-camera state.

The path camera is not a separate lifetime: it is active only while preview owns it or the selected subject has an active path camera. Clear, stop, scene replacement, undo/redo replacement, actor loss, and flycam override must all release/yield ownership.

## 8. Invariants to prove from code

1. No active path `Move` can outlive erased or replaced path geometry.
2. `advancePath()` cannot insert into `mPaths`.
3. A missing or degenerate path cannot place an actor or camera at global origin.
4. Primitive endpoint evaluation reaches exactly `u == 1` when distance reaches the stored `mTotalLength`.
5. Full-turn circle/ellipse/zero-rise helix loops are position-continuous; a rising helix is never allowed to wrap.
6. Negative sweep produces the opposite tangent and remains correct through Reverse.
7. All authored primitive fields survive scene save/load, undo/redo, and copy-to.
8. Primitive camera event metadata follows the correct anchor through Reverse.
9. Generated primitive anchors cannot be dragged or structurally edited.
10. Every adaptive table remains within `MAX_ARC_SAMPLES_PER_SEGMENT` and `MAX_ARC_SAMPLES_PER_PATH`.
11. A finite global camera input cannot overflow into a nonfinite agent-space camera write.
12. Direct Play Path starts only the selected path and does not fire unrelated Director action rows.

## 9. Automated evidence already obtained

The following is preliminary evidence gathered before the final-review gate was corrected. It is not permission to build or a substitute for Claude's complete-diff review.

- `INTEGRATION_TEST_alpathgeometry` builds in Release.
- Test result: 5 total, 5 passed.
- Covered: circle endpoints/analytic length, ellipse radii with rotated plane, negative sweep tangent, helix endpoint/analytic length, and full-circle seam position/tangent continuity.
- `alpanelpatheditor.cpp`, `llactormover.cpp`, and `llpathcamera.cpp` compile cleanly in the Release viewer target.
- `panel_path_editor.xml`, `floater_actor_mover.xml`, and `floater_director.xml` parse successfully.
- All 12 new Path V2 control names occur exactly once in the panel XML and are referenced by the panel implementation.
- Scoped `git diff --check` passes.
- Static gate confirms `advancePath()` contains no `mPaths[` expression.
- Static gate confirms `clearPath()` calls `stop(key)` before `mPaths.erase(key)`.

No further build should run until Claude returns the final review and the complete correction block, if any, has passed Codex's internal adversarial review.

## 10. Required runtime matrix after link

- Waypoint: Stop, Loop, Ping-pong, Reverse, Mirror, Loop-close, camera preview, path camera, save/load, undo/redo.
- Circle: partial positive/negative arc; 1, 2, and 10 turns; Stop, Loop, Ping-pong; rotated plane.
- Ellipse: high eccentricity, tiny radii, partial/full/multi-turn sweep, negative sweep, rotated plane, constant-speed visual check.
- Helix: positive/negative rise, zero rise, positive/negative/multi-turn sweep, Stop/Ping-pong, Loop rejection/sanitization, 3D facing.
- Lifecycle: clear at each playback phase; replace scene data while playing; undo/redo replacement; derez/region loss; switch Subject A; engage flycam.
- Persistence: load v1 waypoint data unchanged; v2 round trip for every primitive field and every node camera/event field.
- UI: verify both Director Console and standalone Character Mover hosts scroll to every control with no clipping.

## 11. Requested Claude response

Return findings ordered `BLOCKER`, `MAJOR`, `MINOR`, each with exact file/function, failure sequence, and smallest architectural correction. Explicitly answer:

1. Can deletion or replacement still leave camera/movement ownership alive?
2. Is primitive distance mapping actually exact where claimed and bounded where numeric?
3. Can any editor order create a discontinuous Loop seam?
4. Does schema v1/v2 migration preserve default waypoint behavior?
5. Can undo/redo, copy, reverse, or actor switching lose/misattach event or camera metadata?
6. Is the whole-path primitive boundary represented honestly?
7. Is there any reason this block should not proceed to runtime acceptance after the clean Release link?
