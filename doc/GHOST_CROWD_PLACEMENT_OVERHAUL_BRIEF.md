# Ghost Studio — Crowd / Formation Placement Overhaul (Codex brief)

**Goal:** (1) make the placement PREVIEW highly visible, (2) expand FACING-on-spawn,
(3) fix SCATTER dispersion with seeded even distribution, (4) add new formations + QoL.
Produce an **adversarial design first** (hazards below), then implement. Client-only,
deterministic given (id, seed, params). Placement/render correctness is **in-world only**.

## Anchors (read before designing)
- Formations: `EFormation` `alghoststudio.h:108` (LINE/RING/ARC/GRID/V/SPIRAL/STAIRCASE/
  TUNNEL/SCATTER). Slot math: `formationSlotAt` `alghoststudio.cpp:1475-1591` → returns
  `FormationSlot{mFoot, mYaw, mAuthorYaw}`. Preview slots: `formationPreviewSlots` :1600.
  Build/spawn: :1649-1756.
- Preview render: `renderFormationPreview` `alghoststudio.cpp:1769-~1850`. Gated by
  `GhostStudioShowFormationPreview` + an operator floater open. Draws via `gUIProgram`,
  `LINES`, no depth write. Colors: `col_slot(.25,.75,1,.85)`, `col_proto` amber,
  `col_perim` faint. Markers = flat ground rings R=0.28m, facing TICK 0.55m, LIFT 0.05m.
  **This is why it's "hard to see": thin low-alpha blue lines, flat on the ground, over grass.**
- Scatter: `FORMATION_SCATTER` :1573 = pure-random disc (seeded angle + sqrt radius) → CLUMPS.
- Existing facing (EXPAND, don't duplicate): `updateLookAt()` :926 (continuous look-at);
  "Face target" combo + "Face now" btn + tracking checkbox (`panel_ghost_studio.xml:87-93`);
  `compass_dial` heading dial :76; `renderHeadingPreview()` in `LLActorMover`; `mAuthorYaw`/
  `mYaw` per slot (formation defaults noted at :1484 "Ring/Arc/Spiral face outward").
- UI: Crowd tab in `panel_ghost_studio.xml` + Director console `floater_director.xml`
  (superset). Controls to add: `seed`, `facing-mode`, new formation entries.

## 1. Preview visibility (the "hard to see" complaint)
Redesign markers to read over any terrain/lighting at a glance:
- **Vertical STALKS** — a bright pole rising ~1.0-1.5m from each slot. Biggest single win
  vs flat ground rings.
- Filled ground **DISC** (semi-transparent) + bright ring with a **DARK outline** (dual-tone
  so it reads over grass, sand, or dark ground).
- Per-slot **NUMBER labels** (1..N; anchor/slot-0 visually distinct).
- Clear **facing ARROW** (not just a tick) at each slot showing spawn heading.
- Optional gentle **PULSE** (animate alpha/scale), setting-gated.
- Draw a faint **through-terrain** version + a solid on-top version so it's never hidden.
- Keep the enclosing perimeter, but dashed/brighter. New settings for colors/scale/height.

## 2. Facing on spawn (expand `updateLookAt`, don't duplicate)
A per-formation **FACING MODE** applied at spawn AND re-appliable: **Source** / Camera /
Centroid-in / Outward / World-point / Path-heading / Mirror-source-yaw / Look-at-target-actor
/ Author-yaw (current per-formation default) / Random. Drive `mYaw`/`mAuthorYaw` from the mode.
UI: a "Facing" combo in the Crowd tab (mirror to Director). Sensible default per formation
(Amphitheater → faces stage/camera; Ring → in/out toggle). Define precedence vs the existing
continuous look-at tracking.

## 3. Seed scatter (better dispersion)
Replace pure-random with a seeded low-discrepancy distribution — **SUNFLOWER / VOGEL
golden-angle**: `a = slot * 2.399963f (≈137.5°)`, `rad = radius * sqrt((slot+0.5)/count)` —
even area-filling, no clumps. Add a **SEED** control (re-roll pattern) + a **JITTER** amount
(organic wobble layered on the even base) + optional **min-distance** guarantee + optional
density falloff (uniform vs center-weighted). Keep determinism (seed from prototype id + seed
control). Vogel is idiomatic here (`RenderShadowSoftVogel` already uses it).

## 4. New formations (each = `formationSlotAt` case + preview + facing default + UI entry,
## mirrored to Director)
**Amphitheater** (tiered arc as an audience facing a stage/camera point — high machinima
value) · Wedge/Chevron · Concentric-rings · Checkerboard · Crescent · Perimeter-line ·
Distribute-along-path (reuse the actor path system) · Cluster-groups (N clumps).

## 5. Quality
- **Terrain-conform** — snap each slot's foot Z to ground height (region/land height query)
  so scatter/large formations sit on hills instead of floating. CRITICAL on uneven land.
- Overlap/collision avoidance — nudge slots off existing avatars/objects.
- Per-spawn variety — small random yaw/scale/pose offset per instance (tie into the existing
  Chaos param).
- Save/recall formation presets.

## Hazards to call out in the design
Terrain height-query cost/availability + async land data; look-at-tracking vs formation-yaw
precedence; preview draw-state and z-fighting/ordering with the world; seed stability across
rebuilds; preview cost when many slots; Director/panel control mirroring drift.

## Acceptance
Preview is obvious over grass at a glance (stalks+discs+numbers+arrows); a facing mode makes
all clones face the chosen target on spawn; Scatter spreads evenly and re-seeds; new formations
place + face correctly; terrain-conform keeps clones grounded; Director mirrors all controls;
builds clean.
