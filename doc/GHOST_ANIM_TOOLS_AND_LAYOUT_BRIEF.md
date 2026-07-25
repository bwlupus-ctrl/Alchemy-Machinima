# Ghost Studio — animation tools (sync / library / loop) + panel layout pass

Three animation features plus a usability-driven layout pass. Per the Director research blueprint
(`doc/DIRECTOR_ANIMATION_CONTROL_DEEP_RESEARCH.md`): **drive `LLMotionController`, never replace it.**

Existing per-clone animation state already landed: `Instance::mAnimSpeed`, `setInstanceAnimSpeed()`,
`setInstancePaused()` (pause reuses `DRIVE_FROZEN` / `requestPause()`), drive modes
Mirror / Directed / Frozen, and a Directed animation UUID field.

---

## Part 1 — Sync / restart animation on selected clones
One action that restarts animation on **all selected clones together**, so a formation locks into
step on cue. This is the counterpart to per-clone speed: speed spreads them out, sync snaps them
back.

- For `DIRECTED` clones: restart the directed asset from its start (offset 0).
- For `MIRROR` clones: re-trigger the mirrored set so they realign with the source's current state.
- Honor multi-selection; a single click should resync a whole crowd.
- Follow the research doc's discipline (§7.2/§7.4): only stop/restart motions this system owns, and
  use explicit restart semantics for a same-UUID asset rather than assuming a clean re-entry.
- Restarting must NOT disturb per-clone speed, pause state, or drive mode.

## Part 2 — Animation library + metadata  (research doc §11)
Today Directed mode requires pasting a raw animation UUID — unusable in practice. Add a picker.

- List the user's **inventory animation items** by name (searchable/filterable if cheap).
- Show per-asset metadata where available: **duration, loop flag, loop in/out, ease in/out, base
  priority, animated joint list/count, hand pose, emote**. Read it through the same path
  `LLKeyframeMotion` uses to deserialize the asset; cache a derived index keyed by asset UUID.
- **Async + graceful degradation:** metadata requires the asset to be fetched and deserialized.
  Show a clear pending/unavailable state rather than blocking the UI or displaying zeros as if they
  were real. Never embed asset binary data anywhere persistent.
- Selecting an entry sets the selected clone's Directed animation (replacing manual UUID paste). The
  existing UUID field must keep working for anyone pasting directly.
- Respect permissions: only the user's own inventory; this is a local convenience, not an asset
  extraction tool. Nothing is written to disk beyond a lightweight metadata index.
- Scope guard: this is a **picker with metadata**, not the full Director asset-library/tagging
  system. Keep it proportionate.

## Part 3 — Per-clone loop / play-once
Let a clone loop a clip or play it a single time, independent of how the asset was uploaded.
- Per-instance setting alongside `mAnimSpeed`, applied to that clone's motion controller only.
- Interacts with Part 1 (a play-once clip should be restartable via Sync) and with pause.
- If the asset's own loop metadata cannot be overridden cleanly at runtime, say so and implement the
  closest honest behavior (e.g. re-trigger on completion for forced loop; stop-at-end for
  play-once) rather than pretending the asset was changed.

---

## Part 4 — Panel layout pass (usability, not cosmetics)
From an in-world screenshot of the current panel, the real problems are:

1. **The entire `Look` section is dead for entity clones.** It shows a disabled combo, an "Actor
   hue" checkbox and **seven disabled sliders** (Hue, Alpha, Pixelate, Shimmer Hz, Amount, Glitch,
   Brightness) with the note *"Scene-lit entity; overlay style controls unavailable"* — roughly
   250px of unusable UI in the **common** case, since entity clones are the canonical ghost type.
   **Fix: collapse/hide the inapplicable Look controls when the selected instance is an entity
   clone** (keep the explanatory line, or a collapsed section header). Show them normally for
   overlay ghosts.
2. **~100px of dead vertical space** between the `Copy transform` / `Paste transform` row and the
   `Look` header. Reclaim it.
3. **`Anim Speed` / `Pause` / `Resume` are stranded** on the last visible row, far from the drive
   mode they belong with, and the **`Array` section is pushed entirely off-screen**. Regroup
   `Pose & Animation` so drive mode, anim speed, pause/resume, sync, and loop/play-once read as one
   coherent block; show the Directed UUID/library row only when Directed mode is active.
4. Net goal: at the default window size an operator should see Placement, Pose & Animation, and at
   least the start of Array without scrolling.

Collapsible sections are welcome if they fit the codebase idiom cleanly.

---

## Constraints
- **Do NOT rename or remove any existing control `name=`** — `ALPanelGhostStudio` binds every
  control via `getChild<T>(name)`; hiding/reparenting is fine, renaming breaks the panel. New
  controls may be added.
- The Director Console embeds this same panel — verify it still lays out correctly there (fixed
  height, so content must scroll rather than clip), per the superset rule.
- Client-only: nothing to the simulator. Clone-only: never touch the source avatar's or agent's
  motion controller, or the global time factor.
- Do NOT reintroduce local-avatar-scale code (reverted deliberately, commit `ec82e1a49f8`).
- Defaults must leave existing clones behaving exactly as they do today.
- Self-review to 0 must-fix. Report: each feature's API + UI, how sync avoids disturbing
  speed/pause/mode, the library's async/permission handling, the honest loop/play-once semantics,
  what the layout pass hid vs moved, and confirm no control renames.
