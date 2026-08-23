# Pose Polish integration plan (continuity, grounding, layering)

Status: research + integration plan only. No source implementation. Scope covers
every ranked improvement **except motion matching / ML** (explicitly out of scope
here; see the closing note). Baseline: `feature/cine-light-rig`. Every `file:line`
anchor below was verified against the working tree.

## Executive decision

Add one new **post-blend Pose Polish stage** — a thin per-avatar filter that runs
*after* SL's normal animation blend and *before* the procedural gaze layer — never
a replacement animation engine. It repairs discontinuities, preserves foot contact,
and otherwise reproduces the authored motion exactly. It must:

- never modify simulator position, animation IDs, priorities, easing semantics, or
  any network message;
- be a strict no-op when disabled and when there is nothing to repair (an animation
  playing steadily with no motion/priority change must come out bit-identical);
- honor SL joint priority and the existing NORMAL/ADDITIVE pass results;
- degrade gracefully by distance/visibility (quality ladder) and under frame pressure.

The target render pipeline becomes:

```text
Simulator movement
  -> SL motions, priorities, easing        (llmotioncontroller: NORMAL then ADDITIVE)
  -> final pose blend                       (LLPoseBlender::blendAndApply)
  -> [NEW] transition inertialization       (ALPoseContinuityFilter)   <-- Milestone 1
  -> [NEW] contact / terrain correction     (ALContactStabilizer)      <-- Milestone 2
  -> gaze + secondary motion                (LLActorMover gaze; procedural secondary) 
  -> skinning
```

The gaze/IK-spine/additive-overlay layer we already ship is the "gaze + secondary
motion" step; Pose Polish sits *upstream* of it, so gaze continues to solve against
an already-continuous, already-grounded pose. Nothing in the gaze layer needs to
change.

## Insertion point and reusable machinery (verified)

- **Post-motion hook.** `updateMotions(NORMAL_UPDATE)` runs the full blend at
  `indra/newview/llvoavatar.cpp:5223`; the gaze arbitration runs immediately after
  at `indra/newview/llvoavatar.cpp:5229`. **Milestones 1–2 insert between line 5224
  and 5229**, in that exact order (inertialize, then contact), so gaze paints on the
  polished pose. Root position/rotation is already resolved just above at
  `indra/newview/llvoavatar.cpp:5203` (`updateRootPositionAndRotation`), which the
  contact stage reads but never writes.
- **Blend result to filter.** `LLPoseBlender::blendAndApply()`
  (`indra/llcharacter/llpose.cpp:543`) applies per-joint blended local transforms via
  `LLJointStateBlender::blendJointStates()` (`indra/llcharacter/llpose.cpp:246`). At
  the hook, `joint->getRotation()/getPosition()` is the final authored pose — the
  Pose Polish input.
- **Two passes exist.** `updateMotionsByType(NORMAL_BLEND)` then
  `updateMotionsByType(ADDITIVE_BLEND)` (`indra/llcharacter/llmotioncontroller.cpp:496`
  and `:504`); `updateMotionsByType()` at `:574`. Polish runs after both.
- **Reusable analytic leg IK.** `LLKeyframeStandMotion` already owns a rotation-plane
  two-bone solver per leg: `LLJointSolverRP3 mIKLeft, mIKRight`
  (`indra/llcharacter/llkeyframestandmotion.h:99`) driven from internal hip/knee/ankle
  joint copies (`:76`, `:92`) with ground projection in `onUpdate()`
  (`indra/llcharacter/llkeyframestandmotion.cpp:161`, ankle tracking/locking logic at
  `:180-198`). **Milestone 2 reuses `LLJointSolverRP3`** rather than authoring a new
  solver — this is the single biggest de-risk for foot IK.
- **Existing cadence control.** `LLKeyframeWalkMotion` warps playback time by avatar
  speed (`mAdjTimeLast`, `SPEED_ADJUST_TIME_CONSTANT`, `SPEED_ADJUST_MAX_SEC`,
  `indra/llcharacter/llkeyframewalkmotion.cpp:41-49`, applied `:115-129`).
  **Milestone 3 generalizes this**, it does not replace it.

## Cross-cutting architecture

### Ownership and lifetime

Introduce `ALPosePolish` owned per avatar (a member on `LLVOAvatar`, or a session map
keyed by avatar id mirroring `LLActorMover`'s gaze map so a non-participating avatar
is a pure map-miss no-op). It holds all per-joint continuity/contact state.

- **Allocate state once per avatar**, never per frame. Fixed memory budget ~20–30 KB
  for a fully tracked 216-joint avatar; less for reduced tiers.
- **Process only joints participating in the current blend** (walk the blend's active
  joint-state set, not all 216).
- The filter is a value-added *read-then-rewrite* of `LLJoint` local transforms at the
  hook; it stores its own shadow state and never mutates motion/controller internals.

### Quality ladder (LOD)

Reuse the avatar's existing pixel-area / visibility / impostor decisions (same inputs
`updateCharacter` already computes). Tiers:

| Tier | Who | Inertialization | Contact | Phase |
|---|---|---|---|---|
| Full | self + selected Director actors | all blended joints | foot IK + pelvis nudge | yes |
| Near | nearby visible avatars | all blended joints | basic foot lock | no |
| Mid | mid-distance | major body joints only | off | no |
| Far / impostor / hidden | rest | off (existing path) | off | off |

Under sustained frame pressure, **shed contact solving first, keep inertialization** —
transition continuity is cheaper and higher-value than IK.

### Hard compatibility rules (apply to every milestone)

1. Disabled or nothing-to-repair -> bit-identical to the current pose. Use the same
   "execution-path, not just equal values" discipline as the gaze work: gate every new
   branch; do not reorganize the default float path.
2. Never write simulator position, `mRoot` world position/rotation, animation IDs,
   priorities, or any network/message state.
3. Never change SL priority semantics. Polish operates on the *already-priority-blended*
   pose; it does not re-arbitrate ownership.
4. Reset all continuity/contact state on: teleport, region cross, skeleton rebuild
   (Bento/attachment change), animation pause / scrub / time reversal, and abnormally
   large frame dt. (Mirror the gaze layer's reseed-on-discontinuity pattern.)
5. Bipeds only for contact; sitting / flying / swimming / ground-sit / non-biped Bento
   creatures / arbitrary dances skip the contact stage entirely.

## Milestone 1 — Transition inertialization (`ALPoseContinuityFilter`)

**First prototype. Highest value, lowest risk.** SL easing is a timed weight blend that
does not preserve outgoing joint *velocity*, which is the root of AO snapping / pops.

### Algorithm (Gears-of-War-style inertialization, post-process)

Per tracked joint, keep shadow state: last displayed local rotation (quat) and position,
plus their angular/linear velocities (finite-differenced across frames).

1. Each frame, read the freshly blended local transform.
2. Detect a discontinuity: the active motion set changed, a per-joint priority winner
   changed, or the blended value jumped beyond a velocity-consistent threshold.
3. On a detected transition, snapshot the *currently displayed* pose+velocity as the
   inertialization source and compute the offset from the new animation.
4. Decay that offset toward zero with a **critically damped** curve (per-group
   half-life), evaluated in **quaternion log / angle-axis space** for rotation.
5. Add the decaying offset to the new blended pose and write the result back to the
   joint. When the offset reaches ~0, stop contributing — the authored animation is then
   reproduced exactly.

This repairs discontinuities only; it is **not** a global per-frame smoother (which would
make motion floaty/laggy).

### Prototype half-lives (settings, tunable)

Pelvis/spine 100–160 ms; arms/legs 70–120 ms; head 50–90 ms; fingers/face disabled
initially (or 30–60 ms). Expose as `ALPolishInertiaHalfLife*` per joint group +
`ALPolishInertiaEnabled` (default off until validated).

### Compatibility / no-op proof

With no transition and steady playback, the offset is exactly zero, so the write equals
the blended input bit-for-bit. Disabled -> the stage early-returns before reading joints.
Reset conditions (rule 4) zero the shadow state so a teleport/scrub never inertializes
across a discontinuity.

### Tests

AO swap, gesture interruption mid-play, sit/stand, teleport, region cross, low FPS
(large dt), Director pause/scrub/time-reversal, Bento avatar, distant-avatar LOD drop.
Add a debug overlay: raw blended pose vs polished pose vs per-joint correction magnitude.

## Milestone 2 — Contact stabilizer (`ALContactStabilizer`)

Generalize the existing stand-motion grounding to arbitrary grounded locomotion, using
the existing `LLJointSolverRP3`.

### Algorithm

1. **Contact inference** (per foot): primarily low toe/ankle *world-space velocity*;
   height only as supporting evidence. Add hysteresis (enter/exit thresholds + min dwell)
   so contact does not flicker. Reuse the walk motion's down-foot heuristic
   (`indra/llcharacter/llkeyframewalkmotion.cpp` velocity/height cues) as a starting point.
2. **Lock** the contact point briefly in world space when contact begins.
3. **Solve** hip/knee/ankle with `LLJointSolverRP3` (one per leg, analytic — no iterative
   whole-body IK) to hold the ankle at the locked point; align the ankle gently to the
   ground normal.
4. **Bounded pelvis correction** only when necessary (small, clamped), never a full-body
   solve; never touches `mRoot`/sim position.
5. **Blend corrections in and out** — do NOT toggle the IK constraint on/off hard
   (that is the classic footskate knee-pop). The correction weight itself ramps via the
   Milestone-1 inertializer, and releases immediately if the required correction exceeds a
   safety bound (reach clamp / heel lift).

### Guards

Grounded biped locomotion only (rule 5). Off for sit/fly/swim/non-biped/dances. Contact
never fabricates reach beyond the leg; on overextension it releases rather than hyperextend.

### Tests

Walk/run over flat and sloped terrain, AO locomotion variety, strafe/turn-in-place,
start/stop, stairs/ramp, foot-slide before/after, knee-pop check, self vs others LOD.
Cross-reference the footskate-cleanup literature (Kovar et al.) — corrections must be
blended, not switched.

## Milestone 3 — Phase-aware locomotion

Extend the existing speed-based playback warp with cached per-clip phase data.

- At clip load, detect alternating low-velocity foot-contact windows -> record L/R plant
  phase and estimate natural stride speed. Cache on the asset (load-time, once).
- On walk/run clip change, **start near a compatible plant phase** so the switch does not
  double-plant or skate.
- Apply only **limited** time-warping (keep the existing `SPEED_ADJUST_MAX_SEC` clamp
  spirit) so it never looks sped-up.
- If reliable phase cannot be detected, **fall back to Milestone-1 inertialization** — no
  creator re-export required.

Builds on `LLKeyframeWalkMotion`'s existing adjuster (`:41-49`, `:115-129`); do not
rewrite it, wrap it.

## Milestone 4 — Loop-seam repair

At asset-load time, compare loop-start vs loop-end pose and velocity; synthesize a small
derived correction window straddling the seam (data only, cached — the asset is never
modified). Removes the common single-frame loop hitch. Cheap: analysis once per clip,
correction reuses the Milestone-1 decay. Purely load-time cost.

## Milestone 5 — NLA-like masks and explicit layers (Director-facing)

Do **not** rewrite the priority blender. Add a Director-facing layer model that
*translates into* the existing motion + priority machinery:

- Explicit per-joint masks and named layers (locomotion, upper-body action, face, gaze,
  hands, correction).
- Override vs additive semantics that continue to honor SL joint priority.
- Mute/solo + weight visualization; a per-joint "current owner" diagnostic (pairs with the
  gaze yield-gate diagnostics already in the tree).

This is tooling/authoring over the existing passes, not a new runtime blend.

## Milestone 6 — Procedural secondary motion

Small, bounded, opt-in overlays layered with the gaze secondary-motion pass (same post-
blend location, same additive-over-animation discipline as the gaze additive overlay):
breath, weight-shift idle, sway, settle. Each is a clamped additive delta gated to zero by
default; reuses the gaze layer's `additiveOverlayLocal` composition order so it never
fights the animation. (The "Gravitate" postural-lean slider discussed separately is a
natural first member of this layer.)

## Sequencing and milestones

1. **M1 Inertialization** — build first, in isolation, behind `ALPolishInertiaEnabled`.
   Ship only after it is bit-identical when idle and clean across the full test matrix.
2. **M2 Contact** — separate feature flag, only after M1 is solid (contact reuses M1's
   ramp for blend-in/out).
3. **M3 Phase** — after M2 (shares contact windows).
4. **M4 Loop-seam** — independent, can slot any time after M1 (reuses M1 decay).
5. **M5 Layering** — parallelizable (tooling, not runtime-critical).
6. **M6 Secondary motion** — after M1 (rides the same additive discipline as gaze).

## Risks and mitigations

| Risk | Consequence | Mitigation |
|---|---|---|
| Global smoothing creeps in | Floaty, laggy motion | Inertialize discontinuities only; zero offset when steady |
| Hard IK on/off | Knee pop / footskate | Blend correction weight via M1; release on over-reach |
| Priority semantics altered | AO/gesture ownership breaks | Operate strictly post-blend; never re-arbitrate priority |
| Per-frame allocation | GC/heap churn on large scenes | Allocate per-avatar state once; process active joints only |
| State survives a discontinuity | Snap/warp after TP/scrub | Reset on TP, region cross, rebuild, pause/scrub/reverse, big dt |
| Non-biped / sit / fly | Broken contact solve | Contact = grounded biped only; explicit guards |
| Network/sim divergence | Desync, replication issues | Never touch sim pos / anim IDs / messages; render-only |
| Cost on low-end | Frame loss | Quality ladder; shed contact before inertialization under pressure |

## Test plan (shared)

Per milestone, plus an integration matrix: AO swaps, gesture interrupts, sit/stand/ground-
sit, fly/swim, teleport, region cross, Bento rebuild, low FPS, Director pause/scrub/reverse,
distant-avatar LOD transitions, and self vs others. Every stage ships with a raw-vs-polished
debug overlay and a per-joint correction-magnitude readout. Acceptance for the idle/no-op
case is recorded per-frame local-transform bit-parity, exactly as the gaze work uses.

## Out of scope (by request)

Motion matching / data-driven pose search / ML retargeting. Very high compatibility risk
and cost; excluded from this plan. If revisited, it would sit beside (not inside) the Pose
Polish stage and feed it, never replace SL priority.
