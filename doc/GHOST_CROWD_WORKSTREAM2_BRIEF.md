# Ghost Studio — Crowd Workstream 2 (Codex brief)

Follow-up crowd/clone fixes + features. Run AFTER the creative-effects job lands — they share
`panel_ghost_studio.xml` + `settings.xml`, so do NOT run in parallel. Adversarial design first,
then implement as one `-Wswitch`-clean change. Placement/render correctness is IN-WORLD only.
⚠️ After building, the human must copy changed skins (+ any shaders) to build/Release (incremental
build copies neither).

## Anchors
- Formations: `formationSlotAt` `alghoststudio.cpp:1475-1745` (a case per `EFormation`).
  `FormationOptions` (seed/jitter/facing/terrain) threaded through preview + spawn.
- Group motion: `setFormationMotion` / `updateFormationMotion` (`alghoststudio.h:387,489`) already
  snapshots selected ghosts into a shared `EMotion` group; OFF restores authored transforms.
- Seed UI: `formation_seed_spinner` (`panel_ghost_studio.xml:311`), `mFormationSeed`
  (`alpanelghoststudio.cpp:185`), read into `options.mSeed` (~1726/1757).
- Facing: `EFormationFacing` (`alghoststudio.h:116`), switch in the slot builder ~1524-1557; existing
  `FACING_WORLD_POINT` / `FACING_TARGET_ACTOR` + `updateLookAt()` `:926` + "Face target" UI.
- Clone pose cadence (for adjustable frame rate): harvest + palette upload run per frame in
  `collectGhostBatches` / `drawGeometryGhost` (`llactormover.cpp`). Codex: locate the exact per-clone
  pose refresh to throttle.

## 1. Perimeter Line — even true square (BUG)
`alghoststudio.cpp:1705` splits count into `ceil(count/4)` slots/side → uneven when count is not a
multiple of 4 (last side under-filled, e.g. 10 → 3/3/3/1) and corners double-covered/gapped. FIX:
distribute all N slots evenly around the FULL perimeter by arc-length (perimeter = 4L; slot i at
t=i/N around the loop → mapped to the square edge). Even for ANY count, a true square, no corner
overlap; closed loop (last ≠ first). Keep a size/spacing param.

## 2. Group spawn / marching army (locked unit)
Spawn a batch as a LOCKED GROUP that moves/rotates/animates as ONE unit for easy directing:
- A group id/flag on instances; a spawn-as-group path that creates them pre-grouped.
- Move/rotate the group as a rigid body (apply the group transform to all members, preserving the
  formation). A group path-walk marches the whole formation along a path (reuse the actor path system).
- "Can't be broken down": while grouped, per-instance drag/edit moves the WHOLE group (or is disabled);
  an explicit Ungroup unlocks. Group select/move/delete act on the unit.
- UI: "Lock as unit" / Group + Ungroup in the Crowd tab (mirror to Director). Build on
  `setFormationMotion`'s existing group concept rather than a parallel system.

## 3. Seed → gate to Scatter
Seed only affects Scatter (Vogel) + jitter. Gate its visibility/enable to formations where it matters
(Scatter; jitter>0). Otherwise hide/disable so it doesn't read as a global no-op.

## 4. Facing → "point at a target" as the primary flow
Make "pick a target (world point OR an avatar/object) and every clone points at it" the headline facing
option, with a clear target picker (click a spot / pick an actor), building on `FACING_WORLD_POINT` /
`FACING_TARGET_ACTOR` + `updateLookAt`. Keep the other modes; sensible default per formation.

## 5. Adjustable clone frame rate (stop-motion stutter)
Per-clone (and per-group) adjustable POSE UPDATE RATE: refresh the clone's harvested pose/palette at a
chosen Hz (e.g. 2-30 fps) instead of every frame, for a choppy stop-motion / robotic look that sells the
stylized effects. Slider in fps (max/0 = smooth every-frame). Hold the last pose between updates. Cheap
(fewer palette uploads). UI in the Look or Crowd tab (mirror to Director), per-instance + per-group.

## Constraints
- Client-only; deterministic; Director Console superset for every new control; per-instance/per-group.
- Do NOT regress existing formations/facing/scatter/motion.

## Acceptance
Perimeter makes an even true square at any count; a locked group spawns + marches as one unit + can't be
accidentally broken; seed only shows where it matters; a target picker aims all clones; clone frame rate
visibly stutters at low fps and is smooth at max; Director mirrors all controls; builds clean.
