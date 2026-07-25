# Deep-review brief — Alchemy-Machinima Ghost Studio session (2026-07-24)

**Repo:** `bwlupus-ctrl/Alchemy-Machinima`, branch `develop` (fork of Alchemy Viewer / LL Second Life
viewer). C++/OpenGL desktop app.
**Review target:** everything from commit `82317ba7028` (last in-world VERIFIED state) to HEAD, plus
the uncommitted avatar-physics change. ~2,650 insertions across 21 files.

## What this subsystem is
"Ghost Studio" creates **client-only clones** of avatars for machinima. `LLGhostAvatar` derives from
`LLVOAvatar` but is synthetic: the simulator knows nothing about it. `ALGhostStudio` is the data
model (per-instance transform/pose/animation state); `ALPanelGhostStudio` is the shared UI panel,
embedded by BOTH a standalone floater and the Director Console's Ghosts tab.

## Hard invariants (violations are the most serious possible finding)
1. **Client-only.** Nothing about a clone may ever be sent to the simulator. Every extra-param copy
   uses `local_origin=false`; the clone is `mIsLocalOnly`.
2. **Never mutate the SOURCE avatar** (or the agent avatar) — not its motions, visual params, motion
   controller, AO, or the global animation time factor. A clone reads from its source, never writes.
3. **Defaults must be inert.** Scale 1.0, speed 1.0, effects Off, etc. must behave byte-identically
   to before for existing clones and for ordinary avatars.
4. **Control names are load-bearing.** `ALPanelGhostStudio` binds every widget via
   `getChild<T>("name")`. Renaming/removing an XUI control silently breaks the panel at runtime.

## Ranked risk areas — please focus here

### 1. Per-frame work (perf + lifetime)  ⚠️ highest concern
Several features now run every frame from `ALGhostStudio::updateLookAt()`
(called at `llappviewer.cpp:5323`) and from `ALPanelGhostStudio::draw()`:
- continuous "keep facing" look-at re-aiming
- procedural formation motion (Orbit/Spin/Breathe/Ripple)
- bullet-time freeze-strip capture timing
- avatar physics per clone
- the panel's `draw()` runs ~7 refresh passes, three of which build change-detection **signature
  strings by concatenating UUIDs every frame**

We ALREADY shipped and fixed one perf regression here: the animation library called a full
recursive inventory walk (`collectDescendentsIf` from the root) from `draw()` every frame, because
its throttle was gated on a non-empty signature. It caused a visible microstutter. **Please look for
any remaining per-frame cost of that character**, and for whether the signature-string churn is
acceptable with, say, 20-50 clones.

Also: **object lifetime**. Clones are resolved via `gObjectList.findObject(...)` /
`resolveEntityClone()` and can derez or be replaced by `refreshEntityClone()` (which changes the
runtime UUID). Look for stale pointers, use-after-free, or state that fails to follow a replaced
clone. A real bug of exactly this class already occurred: the Director camera's Subject A silently
fell back to the agent avatar when a refresh changed the clone's runtime id.

### 2. Avatar physics on clones (uncommitted; NOT externally reviewed)
Starts only `ANIM_AGENT_PHYSICS_MOTION` on each clone's own motion controller, deliberately WITHOUT
re-enabling `mEnableDefaultMotions` (head-rot/eye/body-noise/breathe are disabled on purpose — they
previously fought the mirrored animation). Physics params are claimed to already arrive via
`copyAppearanceFrom(source, true)`. Default is ON per clone.
Review: motion start/stop lifecycle across clone replacement, whether per-clone physics is a
performance trap for crowds, and whether anything here can touch the source avatar.

### 3. Ground-sit animation filtering
`ANIM_AGENT_SIT_GROUND` and `ANIM_AGENT_SIT_GROUND_CONSTRAINED` are now **filtered out of the
mirrored animation set** before reaching the clone. Rationale: mirroring them caused
`processAnimationStateChanges()` to call `sitDown(true)`, giving an unparented client-only clone
structural ground-sit state and pelvis geometry — the clone visually vanished. Prim sits (which
parent the avatar and play ordinary sit animations) were unaffected and must stay working.
Review: is filtering by animation ID robust? Any other animation that implies structural state?
Does the filter leak into non-clone paths?

### 4. Animation asset library (new file `alghostanimassetindex.{h,cpp}`)
Indexes the agent's own inventory animations and reads metadata via
`LLKeyframeMotion::deserialize()`. Async, excludes trash/library, nothing persisted to disk.
Review: permissions correctness (must be the user's own inventory only, and must not become an
asset-extraction path), async/lifetime safety, and that no asset binary is retained.

### 5. Shared runtime touch (`indra/llcharacter/llkeyframemotion.{h,cpp}`)
Adds three const accessors (`getJointMotionNames`, `getEmoteID`, `getEmoteName`) used by the
library. Believed behaviorally inert, but this file is shared by EVERY avatar — please confirm
nothing here can affect normal animation playback.

### 6. Loop / play-once semantics
Per-clone forced loop re-triggers completed non-looping motions; "play once" stops after one asset
duration. It is claimed this never mutates shared keyframe metadata (which is shared across all
avatars using that asset). **Please verify that claim specifically.**

### 7. UI/XUI restructure
The panel was reorganized into a `layout_stack` of sections inside a scroll container, and the
"Look" section is collapsed for entity clones. It must also lay out correctly inside the Director
Console's FIXED-HEIGHT tab (content must scroll, not clip). ~103 control names must all still
resolve via `getChild`.

### 8. Diagnostic code that must not ship
A `GhostSit` probe is still present (in `llghostavatar.cpp`, `lldrawpoolavatar.cpp`,
`lldrawpool.cpp`) with **per-frame draw-batch counters**. It is retained only until the ground-sit
fix is confirmed in-world, then must be removed. Flag anything else diagnostic left behind.

## Context that may save you time
- Codex (GPT-5 class) authored most of this and self-reviewed each batch to "0 must-fix". Its
  reviews have repeatedly passed code that **failed in-world** — it cannot build the viewer or see
  rendered output. Treat "self-reviewed clean" as unverified.
- Six real defects tonight were found only by running the viewer: camera framing on scaled clones,
  three distinct local-avatar-scale defects (since reverted), the ground-sit vanishing, and the
  inventory-scan microstutter.
- A local-avatar-scale feature was implemented and then **deliberately reverted** (commit
  `ec82e1a49f8`). Do not suggest re-landing it; its three root causes are documented in
  `doc/AVATAR_LOCAL_SCALE_PHASE1_BRIEF.md`, `SCALED_AVATAR_CULLING_EXTENTS_BRIEF.md`, and
  `SCALED_AVATAR_STALE_EXTENTS_FIX_BRIEF.md`.

## What would be most useful back
Concrete, file/line-anchored findings ranked by severity, especially: crashes/use-after-free,
invariant violations (sim traffic, source mutation), per-frame cost, and anything that only
misbehaves in a state we have not tested (many clones, derez mid-operation, region change, clone
replacement during an active effect). Style/naming feedback is low value here.
