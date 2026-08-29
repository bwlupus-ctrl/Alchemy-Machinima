# Character Mover V2 — Claude Adversarial Review Handoff

**Repository:** `I:\alchemy-machinima`
**Branch:** `feature/ultimate-diopter`
**Research/backup commit:** `ab62137f1f8 docs: define Character Mover V2 implementation plan`
**Review range after the implementation commit:** `ab62137f1f8..HEAD`
**Build status:** intentionally **not built yet**; this document is the review gate before compilation.

## 1. Review objective

Review the Character Mover V2 implementation as one system. Do not limit the review to isolated style comments. Trace ownership and state across motion loading, AO replacement, gait transitions, dwell, arrival, suspension, actor replacement, path persistence, recorder seeks, and airborne paths.

Please prioritize findings that can cause:

- a compile or link failure;
- an animation/network ownership violation;
- an orphaned or incorrectly stopped motion;
- a frame-zero or stale-phase pose during asynchronous loading;
- a frozen unrelated gesture, gaze, AO, or animation;
- a non-deterministic gait/path result after a seek;
- malformed scene data poisoning runtime state;
- ground contact remaining active on an air path;
- an unbounded path rebuild or per-frame allocation;
- schema/UI promises that the runtime does not actually implement.

## 2. What this candidate implements

### Per-motion presentation control

- Owner-token-scoped presentation control on each `LLMotion` instance.
- Default-inert native clock behavior.
- Opt-in absolute external sample time for keyframe motions only.
- Independent 0..1 presentation-weight multiplier applied to every nonzero controller lifecycle branch without replacing LOD fade.
- Automatic reset of owner, clock, sample, and weight on motion deactivation.
- Specialized `LLKeyframeWalkMotion` bypass of the legacy `Walk Speed` integrator while externally sampled.
- Regression tests for ownership, native/external clocks, unsupported motion types, weight clamping, non-finite values, owner release, deactivate reset, and large-time mapping.

### Async-load and phase analysis

- Generic per-asset ankle-activity phase analysis for arbitrary loaded keyframe clips.
- Loading is not cached as a permanent analysis failure.
- Manual left-plant phase or automatic analyzed phase.
- A pending locomotion seed crosses the STATUS_HOLD/load boundary and is applied before the first activated pose.
- The pending seed is refreshed from the latest traveled distance every frame, so it cannot become stale while an asset loads.
- Loop samples are bounded with double-precision modulo before conversion to `F32`; non-loop samples clamp to duration.

### Local motion ownership

- Actor Mover locomotion uses qualified `LLCharacter::startMotion()` / `stopMotion()` and does not send its own simulator animation requests.
- Source UUID and AO-resolved actual UUID are tracked separately.
- AO transactions are balanced on failure, stop, suspension, replacement, and teardown paths.
- Existing simulator-signaled motion is not taken over.
- An already-active unsignaled local motion may be presentation-controlled but is not stopped unless Actor Mover started it.
- Procedural/non-keyframe motion assets are rejected before presentation ownership is claimed.
- Dwell animations deliberately use native time and the same local ownership handle without external sampling.

### Distance-matched gait runtime

- Actor Mover no longer calls `setAnimTimeFactor()`.
- `F64` traveled-distance odometer drives clip sampling.
- Walk/run selection uses nominal-speed midpoint hysteresis.
- Incoming gait starts at weight zero, waits for asset readiness, and crossfades with smootherstep.
- Pending incoming clips leave the outgoing stride at weight one and continue updating its sample.
- Same source UUID changes metadata in place instead of attempting a self-crossfade.
- Failed primary and secondary clips are cleaned, quarantined, and leave coherent role state.
- Arrival, dwell, suspension, replacement, Stop, and Stop All clear both gait sides.

### Path, facing, persistence, and air mode

- Fixed 24-sample path spans are replaced with bounded adaptive arc-length subdivision using quarter/mid/three-quarter error tests.
- Per-segment retained samples are structurally capped; recursion is depth-bounded.
- Shortest-arc C2 scalar yaw uses `ALTrajectory`; seeks/discontinuities snap rather than integrate stale state.
- Reverse/ping-pong facing correction and distance look-ahead are supported.
- Path scene schema 1 persists authored path settings, nodes, node animations, cameras, and air mode.
- Loader validates schema/type/count, finite positions/scalars/quaternions, clamps ranges, rebuilds derived data, and clears undo history.
- Director scene version is 6; legacy `loco_anim` migrates into the walk-forward role.
- Air paths select fly/hover roles, disable ground follow, clamp pitch, and release contact hints.

### Pose Polish integration

- Actor Mover publishes advisory, frame-stamped contact hints only.
- Pose Polish ignores stale hints and resets on mover discontinuities.
- Airborne mover state releases both contacts even if simulator `mInAir` disagrees.
- Expected plant/swing hints are published only for valid analyzed gait metadata.
- Actor Mover does not write leg joints and does not fabricate per-foot ground heights.

## 3. Adversarial findings already fixed in this pass

1. Removed all Actor Mover ownership of the avatar-global animation time factor.
2. Closed AO/local ownership leaks across stop, failure, suspension, cancel, and runtime replacement.
3. Prevented an async-loading clip from caching `NOT_LOOPING` before its curves arrive.
4. Added first-pose phase seeding across STATUS_HOLD.
5. Refreshed that seed while the actor continues moving, avoiding a stale initial distance.
6. Preserved the `-1` auto-phase sentinel instead of collapsing it to phase zero during load.
7. Rejected non-keyframe/procedural role assets before presentation control is claimed.
8. Kept the outgoing gait sampling while an incoming gait is pending.
9. Cleaned missing/failed canonical motions even after the controller removes the instance.
10. Quarantined failed primary and secondary role assets to prevent retry churn.
11. Converted dwell clips back to native time so a node animation is not frozen at sample zero.
12. Stopped both gait sides at arrival/dwell boundaries.
13. Reworked the adaptive subdivision budget so recursive branches cannot share and overrun one mutable counter.
14. Added path schema, finite-value, quaternion, node-count, and air/ground validation.
15. Removed fake root-ground values previously proposed as both foot-ground samples.
16. Cleared motion handles even when suspension/cancel occurs with a dead or unresolved avatar.
17. Corrected test-only access to protected `LLMotion::deactivate()` and corrected the seed ownership predicate.

## 4. Deliberately deferred from the research roadmap

These are not represented as complete runtime features in this commit and should not be accepted merely because schema/UI placeholders exist:

- extraction of path math into a pure shared module with geometry unit tests;
- exact parametric arc, circle, and helix span types;
- full rotation-minimizing 3D frames and authored roll transport;
- baked ground profiles, per-foot ground samples, and measured extra-ray profiling;
- explicit reversible takeoff/land timeline transition records;
- runtime selection of idle, turn-in-place, backward, and strafe roles (the role schema and UI reserve them; ground runtime currently selects walk/run, while air selects fly/hover);
- enforcement of persisted `min_natural_rate` / `max_natural_rate` bounds;
- a full async keyframe-asset first-pose fixture and scene round-trip test suite;
- broader performance instrumentation for 1/10/20 actors.

Treat these as follow-on work, not hidden acceptance claims. The implementation candidate is the production core and authoring/persistence foundation, not every item from Patches 6–10 of the research document.

## 5. High-risk code to inspect first

- `indra/llcharacter/llmotion.h/.cpp`: owner identity, finite-input rejection, reset timing.
- `indra/llcharacter/llmotioncontroller.cpp`: every nonzero pose-weight branch and the controlled-motion deprecation guard.
- `indra/llcharacter/llkeyframemotion.cpp`: effective sample resolution, phase-cache lifetime, seed-before-first-pose ordering.
- `indra/llcharacter/llkeyframewalkmotion.cpp`: absence of double time warping.
- `indra/newview/llactormover.cpp`:
  - `startLocalMotion()` / `stopLocalMotion()`;
  - `setLocalMotionSample()`;
  - gait transition and failed-load cleanup;
  - suspend/resume/replacement/dwell/arrival paths;
  - adaptive `Path::rebuild()`;
  - path scene load validation;
  - contact-hint publication and airborne traversal.
- `indra/newview/lldirectorcast.cpp`: scene migration and sparse role serialization.
- `indra/newview/llvoavatar.cpp`: stale-hint fallback and airborne contact release.

## 6. Invariants Claude should prove from code

- No Actor Mover path calls `setAnimTimeFactor()` or restores a clock it did not own.
- No Actor Mover start/stop path uses `LLVOAvatar::startMotion()` / `stopMotion()` for locomotion.
- Every AO `override(source, true)` has a reachable matching `override(source, false)`.
- A simulator-signaled motion can never be stopped by Actor Mover.
- Presentation ownership never silently moves between owner tokens.
- A non-keyframe role cannot enter external-sample mode.
- A loading incoming gait cannot reduce the outgoing gait weight.
- The first loaded pose uses the latest distance plus manual/automatic phase.
- A same-UUID transition never creates two controlled sides.
- Stop, arrival, dwell, suspend, cancel, actor loss, and replacement cannot leak either gait side.
- Default presentation weight 1.0 preserves stock controller arithmetic.
- Path rebuild has deterministic termination and a hard sample cap.
- Scene loading never commits partial invalid path data.
- Airborne paths cannot also ground-follow or retain foot locks.
- Stale contact hints preserve the prior Pose Polish behavior.

## 7. Static checks already completed

- `git diff --check`: pass; only line-ending conversion notices, no whitespace errors.
- XML parse: pass for `settings.xml`, `floater_director.xml`, and `panel_path_editor.xml`.
- `.rej` scan: none.
- Actor Mover `setAnimTimeFactor(` scan: none.
- Local-motion start/stop scan: locomotion uses qualified `LLCharacter` calls; remaining qualified calls are the pre-existing Director body-turn transaction.
- No compiler or linker was invoked.

## 8. Build gate after review

Use the configured build tree:

```powershell
cmake --build I:\alchemy-machinima\build-Windows-vs2026-os --config Release --target alchemy-bin
```

The new integration-test target is registered in CMake but is not present in the existing generated project tree yet. After the repository's normal configure step regenerates the solution, build/run:

```powershell
cmake --build I:\alchemy-machinima\build-Windows-vs2026-os --config Release --target INTEGRATION_TEST_llmotionpresentation
```

Do not treat a successful viewer link as sufficient. The integration test and runtime matrix below remain required.

## 9. Runtime matrix after a clean build

1. Self avatar, no AO: walk, run threshold crossing, dwell, stop, restart.
2. Self avatar with AO replacement: verify source/actual UUID balance and no orphan.
3. Remote avatar and ghost/control avatar: verify local-only behavior and no animation request.
4. Gesture, gaze, and unrelated animation active during movement: timing must remain unchanged.
5. Non-1 global time factor active before movement: start/stop must not change it.
6. Custom walk/run asset already loaded and loading on first use.
7. Missing, non-looping, and procedural role assets: fail soft, no ownership leak.
8. Same UUID in walk and run roles: no double instance/crossfade.
9. Stop/suspend/derez/runtime replacement during a crossfade.
10. Dwell animation: advances on native time while locomotion remains stopped.
11. Loop, ping-pong, sharp corners, coincident nodes, and very long paths.
12. Recorder play, pause, forward seek, and backward seek.
13. Scene v4 legacy load and v6 save/reload with role/path data.
14. Air path: fly/hover selection, no walking fallback, no ground follow, contacts released.
15. UI visible/hidden with 1/10/20 actors for performance comparison.

## 10. Requested review response

Return findings grouped by severity and subsystem. For every blocking issue include:

- concrete failure scenario;
- exact file/function;
- violated invariant;
- recommended architectural correction;
- whether it blocks compilation, the first build, or only runtime acceptance.

After findings, give a concise verdict: `READY TO BUILD`, `READY WITH NONBLOCKING FOLLOW-UPS`, or `NOT READY TO BUILD`.

## 11. Worktree/staging warning

The repository contains unrelated, pre-existing Cine Light, renderer, ReShade/G-buffer, and review-log changes. The implementation commit intentionally stages only Character Mover V2 files and only the Character Mover hunks from mixed files (`llfloaterdirector.cpp` and `settings.xml`). Review the committed diff, not the entire dirty working tree.
