# Clone avatar physics — audit fixes

An independent static audit of the per-clone avatar-physics implementation found **no BROKEN
issues** — the implementation is structurally sound (no agent/sim dependency, params genuinely
reach the clone, no shared state between clones, no simulator traffic reachable, uniform scale
composes correctly). It found **three RISKY items** worth fixing, plus one to defer.

Do NOT redesign physics. These are targeted corrections.

---

## FIX 1 — Disabling physics leaves frozen morph residue (visible)
`LLPhysicsMotionController::onDeactivate()` is empty (`llphysicsmotion.cpp:258-260`), and the
physics-driven params live in **visual-param space, not in the pose**. So stopping the motion never
undoes its last morph: unchecking **Physics** leaves the last-written values for the driven params
applied permanently. Bounded by `behavior_maxeffect` (`llphysicsmotion.cpp:766-773`) — e.g. up to
~3 cm of frozen vertical offset for `Breast_Physics_UpDown_Driven` (range ±3, `pos = (0,0,-0.01)`
per unit, `avatar_lad.xml:7526-7545`). Visible in a close-up.

Upstream has the same latent flaw, but upstream never exposes a per-avatar toggle — this fork is the
first place it becomes reachable.

**Fix:** in `LLGhostAvatar::setEntityPhysicsEnabled(false)`, after `LLCharacter::stopMotion(...)`,
return the driven params to rest. All eight have symmetric ranges, so `getDefaultWeight()` (0) is
exactly the neutral the physics code itself targets at `normalized == 0.5`:

```cpp
// Physics writes VISUAL PARAMS, not pose rotations, so deactivating the
// motion does not undo its last morph. Return the driven params to rest.
static const S32 kPhysicsDriven[] = {1200,1201,1202,1203,1204,1205,1206,1207};
for (S32 id : kPhysicsDriven)
{
    if (LLVisualParam* p = getVisualParam(id))
    {
        setVisualParamWeight(p, p->getDefaultWeight());
    }
}
updateVisualParams();
```

One-shot, no per-frame cost. Verify the param IDs against `avatar_lad.xml` before trusting the list.

## FIX 2 — Hidden clones still run full physics (pure waste)
`set_entity_clone_visible(false)` sets `LLDrawable::FORCE_INVISIBLE` (`alghoststudio.cpp:74-79`), but
that is a **pipeline render gate** (`pipeline.cpp:3904`) and is **not** part of
`LLVOAvatar::isVisible()` (`llvoavatar.cpp:8750-8757`, which only tests octree visibility). So a
hidden-but-in-frustum clone still runs all six physics sub-motions AND the full
`updateVisualParams()` walk every frame, for geometry nobody can see.

**Fix:** when a clone is hidden, stop `ANIM_AGENT_PHYSICS_MOTION`; when re-shown, restart it if
`mEntityPhysicsEnabled`. Keep the enable flag and the running-motion state distinct so the toggle and
the visibility gate compose correctly (hidden + enabled → not running; shown again → running).

## FIX 3 — Crowd cost / default-ON is a foot-gun at scale
Physics is **never LOD-culled**: `MIN_REQUIRED_PIXEL_AREA_AVATAR_PHYSICS_MOTION = 0.f`
(`llphysicsmotion.cpp:46`) makes the controller's only cull test
(`llmotioncontroller.cpp:649`) always false. The fork compounds this two ways:
- ghosts are forced to `mUpdatePeriod = 1` (`llvoavatar.cpp:4566-4571`), so they never take the
  cheap `HIDDEN_UPDATE` early-out distant real avatars get (`llvoavatar.cpp:5078-5082`);
- `RenderAvatarPhysicsLODFactor` defaults to 1.0 (`settings.xml:12093`), which no-ops both secondary
  gates in `LLPhysicsMotion::onUpdate` (`llphysicsmotion.cpp:698-708`).

Estimated per visible clone per frame: ~18 joint world-transform queries, 6 `dynamic_cast`s, ~12-24
map lookups, plus a `LLVOAvatar::updateVisualParams()` walk over **all ~705 visual params**
(`llcharacter.cpp:471-488`). At 50 clones that is on the order of 35,000 param entries walked per
frame plus up to ~400 morph applies.

**Fix (pick the cheap, high-value subset):**
1. Implement FIX 2 (biggest easy win — hidden clones cost nothing).
2. Add a crowd guard: when enabling physics across a large selection, or when clone count exceeds a
   threshold, either default the new clones' physics OFF or warn. The tooltip already advises
   disabling for crowds (`panel_ghost_studio.xml:181`) — better to act than advise.
3. Micro-fix: hoist the `dynamic_cast<LLDriverParam*>` out of the integration step loop
   (`llphysicsmotion.cpp:666`) and cache it on the motion — it is per-step RTTI on a pointer that
   never changes after `initialize()`.

**Do NOT** advertise `RenderAvatarPhysicsLODFactor = 0` as the cheap path: at 0 every sub-motion
returns true (`llphysicsmotion.cpp:508-512`), so the integration is skipped but the expensive
705-param walk still runs every frame. The genuinely cheap global off switch is `AvatarPhysics = 0`
(`llphysicsmotion.cpp:458-462`).

## DEFERRED — velocity spike on position jumps (do NOT fix yet)
`mPosition_world` only advances inside `onUpdate` (`llphysicsmotion.cpp:724`), so any single-frame
teleport reads as one frame of travel: `ghostSlamPosition` (numeric transform / array helpers /
"move here"), clone spawn (stale `(0,0,0)` origin — this is the familiar upstream "settles on rez"
wobble), and most likely **unfreezing a frozen clone that was moved while paused**. The spike is
bounded (velocity clamps ±100 at `:627-630`, position clamps [0,1] at `:662`, NaN reset at
`:650-660`) so it is a pop, not corruption.

**Hold this until it is seen in-world.** Smooth gizmo drags and formation motion will not trigger it.
If it proves objectionable, the fix is a `resetPhysicsHistory()` on `LLPhysicsMotionController`
iterating its motions to clear `mPosition_world`/`mLastTime`, called from `ghostSlamPosition` and on
unfreeze.

---

## Constraints
- Client-only; nothing to the simulator; never mutate the SOURCE avatar. Note the audit confirmed
  the physics path calls `LLCharacter::startMotion`/`stopMotion` **explicitly qualified**
  (`llghostavatar.cpp:232,242,246`), bypassing `LLVOAvatar`'s overrides — **preserve that
  deliberately**, it is what keeps the `gAgent` branches structurally unreachable.
- Do not rename or remove any XUI control `name=`.
- Do not reintroduce local-avatar-scale code (reverted, commit `ec82e1a49f8`).
- Self-review to 0 must-fix. Report each fix applied, the param-ID list you verified for FIX 1, how
  the enable-flag vs running-motion states compose in FIX 2, and what you chose for the crowd guard.
