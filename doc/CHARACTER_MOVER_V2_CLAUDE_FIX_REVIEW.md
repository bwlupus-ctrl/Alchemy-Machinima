# Character Mover V2 — Corrective Adversarial Review Brief

## Review target

- Baseline implementation: `35cd2ef0d13` (`feat: implement Character Mover V2 runtime`)
- Review the committed corrective range: `35cd2ef0d13..HEAD`
- Primary finding source: `doc/CHARACTER_MOVER_V2_CLAUDE_REVIEW_RESPONSE.md`
- Review committed files only. The working tree contains unrelated Cine Light,
  renderer, ReShade, menu, shader, and local log work that is not part of this
  patch.

This is the pre-build adversarial gate requested by the project owner. No viewer
build or runtime acceptance pass has been run for these corrections. Please do
not infer build success from the static checks listed below.

## Corrective scope

### Test target blockers

1. Replaced all nine nonexistent `ensure_approximately` calls with TUT's
   `ensure_distance` helper.
2. Expanded the `llmotionpresentation` integration-test source closure from
   `llmotion.cpp` to `llmotion.cpp;llpose.cpp;lljoint.cpp`.
3. Added a residual-weight regression assertion and renamed the large native
   clock assertion so it claims passthrough, not external-clock mapping.

### AO and local-motion safety

1. Added `AOEngine::resolveAnimation()`, a const/read-only lookup. It returns the
   state asset already active or the selected, already-resolved asset UUID. It
   never cycles/randomizes the AO, starts/stops timers, edits state, performs
   cleanup, or sends a simulator animation request. Null means use stock.
2. `LLActorMover::startLocalMotion()` no longer calls the transactional
   `AOEngine::override()` path to discover an animation and no longer tracks a
   fake AO transaction in `LocalMotionHandle`.
3. Local-motion stop now releases presentation control but never stops an
   animation UUID that the simulator currently signals, including on self.
4. The activation-offset loop was replaced with bounded F64 closed-form cycle
   arithmetic and a bounded F32 conversion.
5. Inactive or presentation-ownership-lost loaded motions clear their role and
   recover on the next gait update. Pending loads remain valid; deleted loading
   instances still flow through the bad-asset quarantine.

### Presentation clock and crossfade math

1. Added `LLMotion::getPresentationBaseWeight()` to recover the controller's
   presentation-neutral residual domain.
2. Every `mResidualWeight` capture in `LLMotionController` now neutralizes the
   presentation multiplier before the multiplier is applied on subsequent
   frames. Identity weight 1.0 keeps the exact original arithmetic branch.
3. `applyExternalLocomotionSeed()` now uses positive modulo for negative loop
   base times.
4. The API comment explicitly states that claiming control is type-agnostic for
   weight-only use while external clock sampling remains capability-gated.

### Deterministic path and follower behavior

1. Recorder seeks larger than `SYNC_SEEK_SNAP_M` reset follower odometer state
   from absolute follower arc distance instead of integrating fake travel.
2. Leader and follower seeks publish contact/facing discontinuities and snap
   existing owned clip clocks without feeding scrub velocity into gait choice.
   A seek with no owned clip still starts a safe fallback role.
3. Removed duplicate post-gait sample writes in straight, path, and follower
   traversal.
4. Replaced the per-segment-only resource bound with both per-segment and
   whole-path limits. Live authoring and scene loading share a 4096-node cap;
   the arc table is capped below 65,536 samples. Leaf budgets are distributed
   fairly across remaining segments and unused budget rolls forward.

### Lifecycle corrections

1. Actor migration preserves the active dwell animation before
   `stopLocomotion()` clears it, then restarts it on the replacement body.
2. Dwell entry stops both gait sides based on handle ownership rather than a
   possibly stale `mAnim` marker; dwell exit always asks the role resolver to
   restart locomotion.
3. A zero-distance `placeAt()` hold no longer starts and immediately stops a
   gait. Arrival also cleans a secondary-only transition.
4. Air Hover/Fly selection now has separate 0.10/0.20 m/s hysteresis thresholds.

### Authoring honesty and scene schema

1. The Director role combo now exposes only runtime-reachable authoring roles:
   Idle, Walk Forward, Run Forward, Fly, and Hover. Idle remains visible because
   it is the deterministic fallback for an unset Hover role. Deferred roles
   remain readable in the internal enum/scene parser for compatibility but are
   no longer presented as working authoring choices.
2. Speed, phase, and AO changes made before `Set role` are retained as a
   metadata-only draft; the status reports that draft. Drafts with no asset are
   not serialized as configured roles.
3. Removed unused `min_natural_rate` and `max_natural_rate` fields from new scene
   output and runtime storage. Old scene keys remain harmless and are ignored.
   Clamping an externally sampled clock to those values would violate distance
   matching, so retaining dead schema was more misleading than removing it.
4. Ground-path pitch is disabled while Air Path is active, and its tooltip now
   states that air paths always use flight-tangent pitch.

## Static gates already run

- `git diff --check` on every corrective file: pass (line-ending warnings only).
- Both edited XUI documents parsed through the .NET XML parser: pass.
- Test source-closure files `llmotion.cpp`, `llpose.cpp`, and `lljoint.cpp` exist.
- No `mAOActive`, locomotion AO lookup via `override()`, unbounded activation
  loop, stale test assertion, per-scene node constant, or removed role item
  remains in the corrective paths.
- All five controller residual captures use the presentation-neutral helper.
- Corrective file staging is isolated from unrelated working-tree changes;
  `llfloaterdirector.cpp` requires hunk staging because it also contains
  unrelated Cine Light Z-offset edits.

## Required adversarial proof obligations

Please report BLOCKER / MAJOR / MINOR with an exact file and reachable scenario.
At minimum, attempt to refute each of these claims:

1. The integration test compiles and links with the revised source closure.
2. `AOEngine::resolveAnimation()` is observably read-only in enabled, disabled,
   underwater, empty-set, unresolved-inventory, and currently-active states.
3. Crafted loop/ease metadata cannot hang or overflow activation-offset work.
4. Presentation weight is applied exactly once through activation, ease-in,
   active, ease-out, zero-weight, release, and deactivation paths; weight 1.0 is
   still bit-identical to stock.
5. Pending loads are not mistaken for lost motions, failed assets remain
   quarantined, and a legitimately stopped owned clip can recover.
6. For every legal authored path, recursion terminates and the whole arc table
   remains below its hard cap without starving later segments nondeterministically.
7. Seeking a leader to the same playhead produces the same follower phase,
   contact discontinuity, facing, and role regardless of scrub history or map
   update order; scrub velocity cannot cause a transient Run/Fly choice.
8. Dwell, arrival, hold, suspend, replacement, and simulator-signaled ownership
   paths cannot leak or cross-stop a motion side.
9. The role UI cannot author a role the runtime never selects, metadata drafts
   survive refresh, old scenes remain readable, and Air Path pitch controls do
   not promise ignored behavior.

## Explicitly non-blocking / not expanded here

- Motion instances are still re-found by UUID every frame by design; no raw
  `LLMotion*` is cached across controller purge.
- Owner restart during the controller's narrow ease/deprecation window and the
  one-frame native/external clock-domain handoff remain framework-level cosmetic
  edges. The new lost-ownership recovery narrows their visible consequence.
- Follow time-offset mode and the deferred turn/backward/strafe/takeoff/land
  selectors are still future work. This patch removes their false UI promise; it
  does not claim to implement them.

## Gate decision requested

Return one of:

- `READY TO BUILD` — no compile/link blocker and no reachable runtime major.
- `HOLD` — list exact blockers/majors and a minimal corrective sequence.

Do not perform the viewer build as part of this review. The owner requested the
adversarial review document before the build gate.
