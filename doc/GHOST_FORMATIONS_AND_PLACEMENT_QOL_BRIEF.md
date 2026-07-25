# Ghost Studio — Formation presets + Placement QOL (easy-win batch)

Two independent, LOW-RISK features batched into one pass because they touch the same files
(`alghoststudio.{h,cpp}`, `alpanelghoststudio.{h,cpp}`, `panel_ghost_studio.xml`, chat commands).
**Transform/UI logic only — no shader, culling, LOD, or draw-pool work.**

---

## Part 1 — Formation presets (extend `makeArray`)

### Current state
`S32 ALGhostStudio::makeArray(const LLUUID& id, S32 count, F32 spacing, bool ring)` supports exactly
two shapes via a bool. Conventions to PRESERVE:
- The **prototype occupies slot 0 and never moves**; `count` is the TOTAL including it, so `count-1`
  copies are created.
- Each copy: `copy.mId.generate()`, `copy.mName = makeDefaultName()`, offset applied to
  `copy.mFootGlobal` (an `LLVector3d`, GLOBAL coords), facing set with `copy.setYaw()`.
- Ring copies face outward; line copies inherit the prototype's facing.
- Returns the number of ghosts created.

### Wanted
Replace the `bool ring` with a formation enum (keep a compatible overload or update all call sites)
supporting at least:
- **Line** (existing behavior — must stay byte-identical)
- **Ring** (existing behavior — must stay byte-identical)
- **Arc** — partial ring; needs an arc-sweep parameter (degrees), default e.g. 120
- **Grid** — rows x columns derived from `count` (roughly square), `spacing` between neighbours
- **V / wedge** — two arms angled back from the prototype; needs a spread angle
- **Spiral** — expanding spiral outward from the prototype
- **Staircase** — line that also steps up in Z each slot (needs a rise-per-step; a step of
  `spacing * 0.5` is a reasonable default, make it tunable if cheap)
- **Tunnel** — two parallel facing rows forming a corridor the camera can fly down
- **Scatter** — random-ish placement within a radius, but **deterministic**: seed from the
  prototype's instance id (same pattern as the existing seeded "chaos" feature) so a formation is
  reproducible and does not reshuffle every frame or on reload.

Facing rules: pick the natural one per shape (ring/arc = face outward; grid/line/staircase = inherit
prototype facing; V = each arm angled; tunnel = the two rows face each other; scatter = inherit or
jittered deterministically). Document the choice in comments.

### UI
The array section currently has `array_count_spinner`, `array_spacing_spinner`, `btn_array_line`,
`btn_array_ring`. Add a **formation combo** and a general **"Build" / apply** button. **Do NOT
remove or rename** `btn_array_line` / `btn_array_ring` or any other existing control `name=` —
`ALPanelGhostStudio` binds every control via `getChild<T>(name)`, so a rename breaks the panel. If
the two legacy buttons become redundant, keep them working as shortcuts for their formations.
Shape-specific parameters (arc sweep, V spread, staircase rise, scatter radius) should be exposed
sensibly without cluttering — a single context-sensitive spinner that relabels per formation is
acceptable and preferred over seven new always-visible controls.

---

## Part 2 — Placement QOL

Small operations on the selected instance (and, where it makes sense, all selected / the whole
array). Each should bump the transform revision via the existing setters
(`setFootGlobal` / `setYaw` / `setTransform`, which already do `++mTransformRevision`) and call
`applyEntityTransform(id)` for `BACKING_ENTITY_CLONE` instances so the live clone follows.

1. **Drop to ground** — snap the ghost's feet to the land surface beneath it. Use the standard
   viewer land query (e.g. `LLWorld::getInstance()->resolveLandHeightGlobal(...)`); if an object
   surface query is readily available and cheap, prefer whatever the existing "Place (click ground)"
   path already uses for consistency. Must work at any distance/region the ghost occupies.
2. **Align feet** — set every selected ghost (or the whole array) to the SAME ground/Z level as the
   currently selected one, so a formation stands on one plane.
3. **Reset upright** — clear any authored pitch/roll, keeping yaw: effectively
   `setYaw(getYaw())`. Fixes a ghost tipped over by the 3-axis manip proxy.
4. **Duplicate in place** — copy the selected ghost at the SAME transform (note: the existing
   `duplicateInstance()` offsets the copy "one step to its left" per its tooltip — this variant must
   not offset). New id + `makeDefaultName()` like the array path.
5. **Copy / paste transform** — copy the selected ghost's foot + rotation (+ scale) to a small
   session-only clipboard, paste onto another ghost. Paste should apply to all selected if a
   multi-selection exists.

---

## Constraints (both parts)
- **Client-only invariant:** nothing is ever sent to the simulator.
- Works for BOTH ghost kinds — overlay ghosts render from `mRotation`/`mFootGlobal` directly;
  entity clones additionally require `applyEntityTransform(id)`.
- Keep the `yaw_spinner` / `heading_dial` in sync (the panel already mirrors `mRotation` every draw
  unless actively edited — just don't bypass it).
- **Superset rule** ([[director-console-superset-rule]]): the Director Console embeds this same
  panel, so panel additions satisfy it — verify the console still lays out correctly given the
  recent sectioned layout_stack + scroll redesign, and preserve ALL existing control names.
- Chat commands in the style of the existing `/ghost*` family are a welcome bonus if they fit the
  command table cleanly (e.g. `/ghostform <shape> <count> <spacing>`, `/ghostdrop`).
- Self-review to 0 must-fix. Report: the new `makeArray` signature + how legacy Line/Ring stayed
  identical, each formation's facing rule, the scatter seed source, each QOL op and where it lives,
  UI/controls added, any control renamed (should be none), and confirm both ghost kinds work.
