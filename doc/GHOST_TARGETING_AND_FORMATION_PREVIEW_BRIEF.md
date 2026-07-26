# Ghost Studio — turn-to-face targeting + formation placement preview

**Design AND implement. Two features, one brief, because they share a root cause and a solution
surface.** Repo `I:\alchemy-machinima`, branch `develop`, HEAD `22d744247d7`.

⛔ **Project rules that bind this work** (`CLAUDE.md`, read them):
- **SHIP FEATURES WHOLE.** Engine + settings + UI + preset/reset wiring, in one delivery. A feature
  the user cannot reach is not delivered. Do not propose "engine now, UI later".
- **ALL Claude-written code gets an adversarial Codex review** before build and before commit.
- The **Director Console is a superset** of all machinima floaters — anything editable anywhere must
  also be editable there. Prefer extending a shared `LLPanel` over duplicating.

---

# FEATURE 1 — Turn to face an actor, and far easier world-point targeting

**User:** *"Turn to face an actor is a must build feature for clones and making it FAR easier to set
a world point. not only to look at but to turn to."*

Look-at (gaze/head) exists. **Turning the BODY does not.** And there is **no way to target a bare
world POINT at all** — only objects/avatars via right-click.

## What already exists — VERIFIED, do not re-derive
- **`Move::mHeading`** (radians) is ALREADY computed from a direction vector:
  `llactormover.cpp:1763` → `mv.mHeading = atan2f(at.mV[VY], at.mV[VX])`.
  **Turn-to-face is that call with `at` replaced by `(target - actor)` normalised.**
- The same block sets `mSpeed = 0, mDistance = 0, mEndMode = 0` — an established **"pinned hold"**.
  So *a Move that only sets facing, without walking, is already expressible.* Turn-to-face is a hold
  with a computed heading, not new machinery.
- `llactormover.cpp:1654` sets `mv.mHeading = yaw`; `:1783-1799` reuses heading and offsets it by PI.
- **`ActorMoverHeading`** (F32, degrees, "rel facing", `:1565`) — a STATIC offset today.
- Ghost placement carries **`mRotation`** (quaternion), composed into the modelview at `:3898-3906`:
  `modelview = view * T(foot) * R(mRotation) * S(s) * T(-pivot)`.
- **`renderHeadingPreview()`** (`:4624`, gated by `ActorMoverShowHeading`) already draws a heading
  beacon. **This is the precedent to extend for BOTH features** — it is a UI-pass draw
  (`gUIProgram`/`LLGLSUIDefault`/no depth, per the comment at `:3108`).
- Target picking exists on all three context menus (`menu_avatar_other.xml`,
  `menu_attachment_other.xml`, `menu_object.xml`) under **Director**: Add to Cast, Set Subject A/B,
  Cinematic Cam Follow, Actor Mover Target, Set Mark Here, Reset to Mark.
- **An in-world point picker ALREADY EXISTS**: `altoolpathedit.cpp` + `alpanelpatheditor.cpp`
  (`ALToolPathEdit`, the path node editor). **Base "set a world point" on this. Do not build a
  second picker.**

## Decide and implement
1. **Continuous vs one-shot.** Keep facing a moving actor, or turn once and hold? Both are real
   shots (a locked stare vs a single turn) — if both, say how they are exposed without confusing them.
2. **Turn rate and easing.** An instant snap reads as a bug. Degrees/sec limit, plus a settle.
   Reuse the camera operator's easing vocabulary rather than inventing a second one.
3. **Legal targets:** another cast member, Subject A/B, an object/animesh, or a bare **world point**.
4. **KEEP LOOK-AT AND TURN-TO SEPARATE.** A clone must be able to face one way and glance another —
   that is an over-the-shoulder shot. Do not collapse them into one target.
5. **World-point UX.** Extend the `ALToolPathEdit` click-to-place pattern; add "use current camera
   position/focus"; add numeric entry. **`Set Mark Here` already stores world positions — decide
   whether a Mark IS the world-point primitive** rather than adding a parallel concept.
6. **Conflict with pathing.** While an actor walks a path, heading comes from the path tangent
   (`:1763`). Define who wins, and what happens when the path ends.

---

# FEATURE 2 — Formation placement PERIMETER preview

**User:** *"the crowd should show a perimeter around the clone about where it will place them because
it's currently erratic how it works."*

The complaint is **predictability**: you commit a formation and clones land somewhere you did not
expect. Nothing shows the footprint first.

## What already exists — VERIFIED
- **`ALGhostStudio::EFormation`** (`alghoststudio.h:87`): `LINE, RING, ARC, GRID, V, SPIRAL,
  STAIRCASE, TUNNEL, SCATTER`.
- **`makeArray(id, count, spacing, formation, parameter)`** (`:318`).
- **`formationSlot(proto, slot, count, spacing, formation, parameter)`** (`:352`) — **returns the
  `LLVector3d` for ONE slot. This is the whole preview: call it for every slot and draw the result.**
  It is currently `private`.
- `parameter` means: **degrees** for Arc/V, **rise metres** for Staircase, **radius metres** for
  Scatter. Zero = the shape's natural default. (Per the header comment at `:315-317`.)
- UI: `mFormationCombo`, `mFormationParam`, `onClickArray()`, `onFormationCommit()` in
  `alpanelghoststudio.h:133-135, 231-232`.
- **There is NO preview render of any kind for formations.** The only preview machinery in the
  project is `renderHeadingPreview()`.

## Decide and implement
1. **What to draw.** Per-slot footprint markers (a disc/ring at each `formationSlot`) AND an outer
   perimeter enclosing them? Numbered slots so the director knows fill ORDER? Say what actually
   answers "where will they go".
2. **When it shows.** Live while the formation combo/param/count/spacing change (a true preview,
   before committing), or only while a Ghost Studio tab is open, or behind a toggle? It must not
   cost anything when the studio is idle — follow the `ActorMoverShowHeading` precedent
   (`renderHeadingPreview` is "zero cost unless the setting is on AND ghosts exist").
3. **Where to hook the draw.** `renderHeadingPreview()` is the model: UI pass, `gUIProgram`,
   `LLGLSUIDefault`, no depth test. Confirm the call site and whether the formation preview belongs
   beside it or needs its own.
4. **`formationSlot` is private** — decide the minimal exposure (a public `formationPreviewSlots()`
   returning the vector is probably cleaner than making the private helper public).
5. **Is placement actually erratic, or only unpredictable?** Read every branch of `formationSlot`.
   If a shape genuinely misbehaves (SCATTER's randomness not seeded/stable, STAIRCASE rise sign,
   ARC/V degrees vs radians, GRID aspect), **say so and fix it** — a preview that faithfully
   previews a bug is not the goal. Report anything you find as a defect, separately from the preview.
6. **Orientation.** Does the formation build relative to the proto's facing, to world axes, or to the
   camera? That alone could explain "erratic". State it, and expose it if it should be a choice.
7. **UI.** Both features need controls in the Ghost Studio panel AND mirrored into the Director
   Console per the superset rule.

---

## Output
Complete, mechanically applicable code for both features: `alghoststudio.h/.cpp`,
`llactormover.h/.cpp`, `alpanelghoststudio.h/.cpp`, `settings.xml`, XUI, and the Director Console
mirror. State the implementation order and what can ship independently. If the workspace is
read-only, put everything in your FINAL MESSAGE as text.

Be opinionated. The user is a director; "technically correct but unpredictable to use" is the exact
failure being reported.
