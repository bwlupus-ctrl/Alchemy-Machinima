# Ghost Studio — per-clone animation speed / pause / chorus offset

Machinima ask: **slow (or speed) animation PER CLONE** — one hero in slow-motion while the crowd
moves at normal speed, staggered crowd tempos, frozen-mid-action figures.

This follows the Director research blueprint's core directive
(`doc/DIRECTOR_ANIMATION_CONTROL_DEEP_RESEARCH.md`): **do NOT replace or reimplement
`LLMotionController` / `LLKeyframeMotion` — drive them.** Everything here is a thin control layer
over existing per-character API.

## The lever (already exists, currently unused per-avatar)
- `LLCharacter::setAnimTimeFactor(F32 factor)` → `mMotionController.setTimeFactor(factor)`
  (llcharacter.h:169, llmotioncontroller.h:164) — **per-character** animation speed.
- `LLMotionController::getTimeFactor()` (llmotioncontroller.h:165)
- `LLMotionController::pauseAllMotions()` / `unpauseAllMotions()` (llmotioncontroller.h:156-157)

Every `LLGhostAvatar` is an `LLVOAvatar`/`LLCharacter` with its **own** motion controller, so these
affect only that clone. Nothing in `indra/newview` currently calls `setTimeFactor` per avatar.

## Features to add

### 1. Per-clone animation speed  ⭐ (the ask)
- New `F32 mAnimSpeed = 1.f` on `ALGhostStudio::Instance` — same shape as the existing `mScale` /
  `mChaosAmount` fields (alghoststudio.h ~111-129).
- Setter `ALGhostStudio::setInstanceAnimSpeed(const LLUUID&, F32)` mirroring the existing
  `setInstanceScale()` / `setInstanceChaos()` pattern: clamp, store, and for
  `BACKING_ENTITY_CLONE` resolve the clone and apply.
- Apply via the clone's own `setAnimTimeFactor()`. Suggested range **0.05 – 4.0** (slow-mo through
  fast-forward), default 1.0.
- **Persistence:** the clone re-mirrors source animations every frame in `MIRROR` drive mode
  (`LLGhostAvatar::idleUpdate` → `processAnimationStateChanges()`), which starts/stops motions.
  Verify the time factor survives that (it is controller state, not per-motion) and re-apply if any
  path resets it — including after `refreshEntityClone()` replaces the runtime clone, which is
  exactly the class of bug that broke the camera earlier today (subject/state lost on refresh).
- **Interaction with the existing GLOBAL slow-mo:** `LLMotionController::sGlobalTimeFactor` (the
  B3a/Freeze-World slow-motion factor) is separate and global. Determine how the two combine and
  make it sensible — a per-clone factor should read as a multiplier *relative to* the scene tempo,
  not fight it. State the resolved semantics in the report.

### 2. Per-clone pause / resume
- `pauseAllMotions()` / `unpauseAllMotions()` on that clone only — a figure frozen mid-action while
  everything else keeps moving.
- **Clarify vs the existing `DRIVE_FROZEN` mode:** frozen drive mode stops *mirroring the source*;
  a true pause holds this clone's own motion controller at its current time. If they overlap
  meaningfully, say so and pick the non-redundant behavior rather than shipping two names for one
  thing.
- Note: speed 0 and pause are different mechanisms — do not fake pause with speed 0 if
  `pauseAllMotions()` is the cleaner path (and beware a 0 factor dividing anywhere).

### 3. Seeded speed variation via the existing Chaos system  ⭐ (high payoff, cheap)
The instance already carries `mChaosEnabled` / `mChaosAmount` with a deterministic UUID-seeded
helper (the same FNV seed used by the new Scatter formation). Extend chaos to also vary **animation
speed** subtly per clone (e.g. ±15% at full chaos, scaled by `mChaosAmount`).

This is the single most "alive crowd" upgrade available: today a crowd of clones varies in
yaw/scale but every body moves in **perfect lockstep**, which is the main tell that they are copies.
Small deterministic tempo variation breaks that instantly.

### 4. Chorus / canon phase offset (stretch — only if it stays clean)
Start the same animation at different time offsets across clones so one clip becomes a wave/canon
(the backlog's "pose-clip chorus"). `LLMotionController::startMotion(id, start_offset)` supports a
start offset. This is more delicate than speed — it requires restarting motions, and the research
doc warns about same-UUID instance semantics and restart policy (§7.4, §10). **If it cannot be done
cleanly, skip it and say so** — features 1–3 are the valuable core.

## UI
- Per-clone **Anim Speed** control in the Ghost Studio panel, near the existing drive-mode /
  chaos controls (the panel is now a sectioned layout_stack + scroll).
- Pause/resume control for the selected clone(s); honor the new multi-selection where sensible so a
  whole formation can be slowed at once.
- **Superset rule** ([[director-console-superset-rule]]): the Director Console embeds this same
  panel, so panel additions satisfy it — just verify layout still holds.
- **Do NOT rename or remove any existing control `name=`** — `ALPanelGhostStudio` binds every
  control via `getChild<T>(name)` (54 bindings currently resolve; keep it that way).
- A `/ghostspeed <value|pause|resume> [all]` chat command in the existing `/ghost*` style is a
  welcome bonus if it fits the command table cleanly.

## Constraints
- **Clone-only.** Never touch the SOURCE avatar's motion controller, the agent avatar's, or the
  global time factor as a side effect. A clone's speed must not leak to the avatar it copies.
- Client-only: nothing to the simulator.
- Default 1.0 / unpaused must be byte-identical to today for every existing clone.
- Self-review to 0 must-fix. Report: the field + setter signatures, how speed survives
  mirror-refresh and clone replacement, the resolved global-vs-per-clone semantics, pause vs
  DRIVE_FROZEN distinction, whether chorus offset was included or skipped and why, and the UI added.
