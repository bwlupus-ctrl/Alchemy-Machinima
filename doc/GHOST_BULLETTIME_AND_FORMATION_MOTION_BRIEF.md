# Ghost Studio — Bullet-time freeze-strip + Procedural formation motion

Two signature machinima features, both **compositions of primitives that already exist**.
Transform/state only — no shader, culling, LOD, or draw-pool work.

## Primitives already available (reuse, don't reinvent)
- `ALGhostStudio::freezeInstance(id)` / `unfreezeInstance(id)` — snapshots the source's CURRENT
  matrix palettes (alghoststudio.h ~268-275); frozen state lives in `mFrozenFootAgent`,
  `mFrozenPalettes`, `mFrozenAttachMats`.
- `duplicateInstance(id)` / `duplicateInstanceInPlace(id)` (~238-239).
- `makeArray(id, count, spacing, EFormation, F32 parameter)` — Line/Ring/Arc/Grid/V/Spiral/
  Staircase/Tunnel/Scatter (~280).
- **Per-frame hook already wired:** `ALGhostStudio::instance().updateLookAt()` called from
  `llappviewer.cpp:5323` — extend it or add a sibling update for per-frame work.
- **Base-transform modulation pattern:** Chaos already stores an authored baseline
  (`mChaosHasBase`, `mChaosBaseFoot`, `mChaosBaseRotation`, `mChaosBaseScale`) and modulates from
  it. **Procedural motion should follow this exact pattern** so the operator's authored transform is
  never destroyed.

---

## Part 1 — Bullet-time freeze-strip
Capture the source's pose at successive moments and lay those frozen snapshots out in space, so one
continuous action becomes a frozen Matrix / comic-book strip.

Behavior:
- Operator sets **count** and **interval** (seconds between captures) and a **formation** (reuse
  `EFormation` — Line and Arc are the classic choices; Ring/Spiral should work too).
- On trigger: capture `count` snapshots spaced `interval` apart in time. Each capture =
  duplicate the source instance, `freezeInstance()` it (so it holds that exact pose), and place it
  at the next slot of the chosen formation.
- Captures happen over real time, driven from the existing per-frame hook — not all in one frame
  (the whole point is successive poses).
- Provide clear feedback while a strip is capturing (e.g. "capturing 3/8") and a way to cancel.
- Each snapshot is an ordinary Ghost Studio instance afterwards: selectable, movable, deletable,
  nameable. Name them so a strip is identifiable (e.g. "Strip 1 · 3/8").
- Optional if cheap: an "age" ramp across the strip (older captures slightly more transparent or
  smaller) — only for OVERLAY ghosts, since entity clones ignore the overlay style params.

Edge cases: source disappears/derezzes mid-capture (stop cleanly, keep what was captured);
overlapping strips; capture while the source is paused or frozen.

## Part 2 — Procedural formation motion
Continuously animate a GROUP of clones' transforms per frame — orbit, spin, breathe, ripple —
**without touching their poses or the source**. This makes a static crowd read as alive.

Modes (at least these):
- **Orbit** — the selection revolves about a shared centre.
- **Spin** — each clone rotates in place (yaw).
- **Breathe** — the formation expands/contracts radially about its centre.
- **Ripple** — a wave of vertical (or radial) displacement travels through the formation by slot
  order or distance from centre.

Requirements:
- **Modulate from an authored baseline**, exactly like Chaos: snapshot each instance's authored
  foot/rotation/scale on enable, animate from that, and restore it cleanly on disable. The
  operator's placement must survive turning the effect on and off.
- Per-effect **speed** and **amplitude** controls; sensible defaults; off by default.
- Applies to the current multi-selection (or a formation/group); each affected instance keeps its
  own identity.
- Entity clones need `applyEntityTransform(id)` per tick so the live clone follows; overlay ghosts
  consume `mRotation`/`mFootGlobal` directly.
- Composes sanely with **Chaos** (which also modulates transforms) and with **look-at / keep
  facing** (which drives yaw) — decide and document the precedence rather than letting them fight;
  e.g. look-at may own yaw while procedural motion owns position, unless the mode is Spin.
- Deterministic where practical (phase derived from instance id / slot index, like the seeded
  chaos + Scatter formation), so a shot is repeatable.

---

## Constraints
- Client-only: nothing to the simulator. Never mutate the SOURCE avatar.
- Both ghost kinds supported (entity clones require `applyEntityTransform`).
- Per-frame work must be cheap and skipped entirely when no instance has an effect enabled.
- **Do NOT rename or remove any existing control `name=`** — `ALPanelGhostStudio` binds via
  `getChild<T>(name)`.
- Superset rule: the Director Console embeds the same panel; verify layout still holds (it was just
  reorganized — respect the new section grouping).
- Do NOT reintroduce local-avatar-scale code (reverted deliberately, commit `ec82e1a49f8`).
- Self-review to 0 must-fix. Report: the API for each feature, how captures are timed, how the
  authored baseline is snapshotted/restored, the documented precedence vs Chaos and look-at, UI
  added, and confirm no control renames.
