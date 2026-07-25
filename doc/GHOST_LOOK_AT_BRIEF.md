# Ghost Studio — Look-at / Face-target (easy win)

## Goal
Point selected ghost(s) at a target with one click: "face the camera", "face me", "face an
actor", or "face another ghost". Backlog item (Track D cheap win): creepy tableaux, synced
reveals, crowds all turning to look at the lens.

**Deliberately a LOW-RISK, transform-only feature.** No shader, culling, LOD, or draw-pool work.

## Existing API (already in place — just use it)
`ALGhostStudio::Instance` (alghoststudio.h ~104-151):
- `LLVector3d mFootGlobal` — ghost foot position, GLOBAL coords
- `LLQuaternion mRotation` — full orientation; `getYaw()` / `setYaw(F32 yaw_rad)` are the
  yaw-only accessors (`setYaw` builds `LLQuaternion(yaw, LLVector3::z_axis)`)
- `ALGhostStudio::applyEntityTransform(id)` — pushes the transform onto an entity clone
  (REQUIRED for `BACKING_ENTITY_CLONE`; overlay ghosts render from `mRotation` directly)

So the core math is: `yaw = atan2(target.Y - foot.Y, target.X - foot.X)` in a consistent frame,
then `setYaw(yaw)` + `applyEntityTransform()` when the instance is an entity clone. Mind the
global (`LLVector3d`) vs agent-coords conversion — `mFootGlobal` is global; convert the target to
the same frame before subtracting (`gAgent.getPosGlobalFromAgent()` / `getPosAgentFromGlobal()`).

## Targets to support
1. **Camera** — the current viewer camera position (`LLViewerCamera::getInstance()->getOrigin()`).
2. **Me** — the agent avatar's position.
3. **Actor / cast member** — a Director cast member or any avatar (resolve like the Director does;
   `LLDirectorCast` / `ALGhostStudio::resolveEntityClone` for clones).
4. **Another ghost** — a second Ghost Studio instance (pick from the instance list).

## Behavior
- **Face now** (one-shot): apply the yaw immediately to the target instance(s).
- **Keep facing** (continuous, per-instance toggle): re-aim every frame/idle tick so the ghost
  tracks a moving camera/actor. Cheap — reuse the same aim function from the existing per-frame
  update path. Must be a per-instance flag that persists with the instance and is clearly
  indicated in the UI. Turning it off leaves the ghost at its last heading (no snap-back).
- **Apply to selection**: at minimum the selected instance; if a multi-select or Director group is
  already available, support "all selected" / group-wide (nice-to-have, don't invent new selection
  infrastructure for it).
- **Optional pitch** (nice-to-have, only if clean): a checkbox to also tilt toward a target that is
  much higher/lower, writing `mRotation` directly instead of yaw-only. Default OFF — yaw-only is
  the safe, expected behavior for standing figures. Do not break the manip proxy's 3-axis writes.

## Constraints
- Client-only: never send anything to the simulator (same invariant as all Ghost Studio work).
- Works for BOTH ghost kinds: overlay ghosts and entity clones (entity needs
  `applyEntityTransform`). Verify both paths.
- Keep the yaw spinner and the live heading dial (`heading_dial`, `ALCompassDial`) in sync when a
  look-at changes the heading — the panel already syncs those two; a programmatic aim must update
  them too so the UI doesn't lie.
- **Superset rule** ([[director-console-superset-rule]]): anything added to the Ghost Studio panel
  MUST also be reachable from the Director Console. The panel was recently restructured into
  sectioned layout_stack + scroll — add the control into the natural section (Placement) without
  breaking that layout, and preserve all existing control `name=` attributes (ALPanelGhostStudio
  binds them via getChild).
- A chat command (in the style of the existing `/ghost*` commands, e.g. `/ghostlook`) is a welcome
  bonus if it fits the existing command table cleanly.

## Validation
- Self-review to 0 must-fix.
- This one IS largely verifiable by reasoning (transform math + UI wiring), unlike the recent
  render work — but the user still confirms in-world.
- Report: the aim function's signature/location, how continuous tracking is driven, both ghost
  kinds handled, UI added (panel + console + any chat command), and confirm dial/spinner sync.
