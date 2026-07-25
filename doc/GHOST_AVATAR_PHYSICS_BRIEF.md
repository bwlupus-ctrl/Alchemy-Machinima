# Ghost Studio — avatar physics on entity clones

## Goal
Give entity clones the same **avatar physics** (breast/belly/butt jiggle) their source has, so a
clone reads as a living body instead of a mannequin. Machinima-critical for crowds and dance shots.

## Why clones have no physics today (confirmed)
- Avatar physics is a motion: `ANIM_AGENT_PHYSICS_MOTION`
  (`LLPhysicsMotionController`, registered in `llvoavatar.cpp:1309`).
- It is started **only** from `LLVOAvatar::startDefaultMotions()` (llvoavatar.cpp:2157-2168),
  bundled with `ANIM_AGENT_HEAD_ROT`, `ANIM_AGENT_EYE`, `ANIM_AGENT_BODY_NOISE`,
  `ANIM_AGENT_BREATHE_ROT`, `ANIM_AGENT_HAND_MOTION`, `ANIM_AGENT_PELVIS_FIX`.
- `LLGhostAvatar` sets `mEnableDefaultMotions = false` (llghostavatar.cpp:76), deliberately — those
  default motions previously fought the mirrored animation. So the clone opts out of the whole
  bundle and loses physics with it.

## Why it will actually look right (confirmed)
`LLPhysicsMotion::calculateVelocity_local()` derives motion from **`joint->getWorldPosition()`**
deltas (llphysicsmotion.cpp:433-438) → velocity → acceleration. It is driven by JOINT movement, not
by the avatar travelling through the world. Therefore:
- A clone standing in place playing an animation **will** show physics (its joints move).
- A frozen/paused clone correctly shows none.
- A clone walked by `LLActorMover` shows physics too.

It also drives joint states (llphysicsmotion.cpp:222), i.e. the physics collision-volume joints, so
**fitted-mesh bodies respond**, not just the legacy system mesh.

## Implement
1. **Start physics on the clone WITHOUT re-enabling the other default motions.** Do NOT flip
   `mEnableDefaultMotions` back on and do NOT call `startDefaultMotions()` — head-rot / eye /
   body-noise / breathe are disabled on purpose and would fight the mirrored animation (this was a
   previously fixed bug class). Start only `ANIM_AGENT_PHYSICS_MOTION` on the clone's own motion
   controller.
2. **Verify the physics visual params reach the clone.** The physics wearable's params
   (breast/belly/butt physics mass, gravity, drag, max effect, spring, gain, damping, etc.) are
   visual params. `LLGhostAvatar` already calls `copyAppearanceFrom(source, true)`
   (llghostavatar.cpp:369) — confirm the physics params are included and land with the same values
   as the source. If they do not come across, copy them explicitly. A clone with the motion running
   but zeroed params will show nothing, which would look like the feature failing.
3. **Per-instance toggle.** Add `mPhysicsEnabled` on `ALGhostStudio::Instance`, following the exact
   shape of the existing `mAnimSpeed` / `mChaosAmount` fields, with a setter mirroring
   `setInstanceAnimSpeed()`. Applies to the current multi-selection.
   - **Default:** ON, so a clone matches its source's appearance out of the box.
   - **But note the cost:** physics runs per clone, so a 20-clone crowd pays it 20x. Make the
     toggle easy to reach and mention the crowd cost in the tooltip. If enabling it for a large
     selection is likely to hurt, say so in your report rather than silently shipping a perf trap.
4. **Re-apply after clone replacement.** `refreshEntityClone()` swaps the runtime clone, and
   duplication / formations / freeze-strip create new ones — physics state must follow, exactly
   like `mAnimSpeed` already does (that pattern is already in `setInstanceAnimSpeed`'s call sites).
5. **Interaction with existing state:** pause (`DRIVE_FROZEN` / `requestPause()`) should stop
   physics along with everything else; per-clone anim speed should scale it naturally since physics
   is derived from joint movement over time. Verify the frozen case does not leave physics
   twitching on a held pose.

## UI
- A per-instance **Physics** checkbox in the Pose & Animation section (near drive mode / anim
  speed / loop), honoring multi-selection.
- Director Console inherits it via the shared panel (superset rule).
- **Do NOT rename or remove any existing control `name=`** — `ALPanelGhostStudio` binds every
  control via `getChild<T>(name)`.

## Constraints
- Client-only: nothing to the simulator; never touch the SOURCE avatar's motions or params.
- Entity clones only (overlay ghosts are a different render path and out of scope here).
- Default behavior for existing clones must not otherwise change.
- Do NOT reintroduce local-avatar-scale code (reverted, commit `ec82e1a49f8`).
- Self-review to 0 must-fix. Report: how physics is started without the other default motions,
  whether the physics visual params were already copied or needed explicit handling, the toggle's
  API/default, how state survives clone replacement, the pause/speed interaction, and the crowd
  perf implication.
