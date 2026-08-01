# Machinima Feature Backlog

Deferred feature requests for the Alchemy-Machinima fork's director / camera / environment tooling. Captured 2026-07-30; **not yet implemented.** Each entry: what it does, why, a rough implementation path (reusing systems that already exist in the fork), and a rough effort.

---

## 1. WindLight / EEP hotkey switcher ("environment bank")
**What:** A bank where you drop the environment presets you want (EEP skies, day cycles, water), then switch between them live with hotkeys — like the Camera Switcher, but for lighting / atmosphere.

**Why:** Change the mood and lighting of a shot on the fly while filming, without opening the environment editor. Cut instantly from golden-hour to storm to night.

**Rough path:** Mirror the Director Camera Switcher design — a slot bank of environment presets (each slot = an EEP asset / saved sky or day-cycle), number-key hotkeys, instant apply through `LLEnvironment::setEnvironment(...)` with an optional per-cut crossfade duration. Home it in the Director Console (there's already a **Time** tab — add an Environment / WindLight section there) and route hotkeys through the existing director hotkey path (`aldirectorhotkeys`). Keep the apply presentation-time-clean so a recorded take reproduces the same cuts.

**Effort:** Medium — reuses the switcher slot-bank + hotkey pattern; the new part is EEP apply + crossfade.

---

## 2. Avatar offset + rotate like an object (Move-Avatar modifier)
**What:** Under **Move Avatar**, a modifier to translate and rotate an avatar (yourself or a cast member) as if it were an object — a transform gizmo plus numeric offsets — for fast staging / blocking.

**Why:** Position and angle performers precisely for a shot without them having to walk into place; fast blocking for filming.

**Rough path:** A **client-side render transform** — the avatar isn't moved server-side; it's offset/rotated in *your* rendered view for the shot. Reuse the fork's existing avatar outer-render-transform / clone-transform machinery (the same mechanism behind ghost-clone move/rotate and avatar uniform scale — see doc/AVATAR_UNIFORM_SCALE_DEEP_RESEARCH.md and the entity-clone transform work). Apply a per-cast-member render offset (position + rotation) as a Move-Avatar modifier, wired to the Director cast system. **Caveat:** client-side only (others don't see it) — which is exactly right for machinima, since you film your own view.

**Effort:** Medium — the render-transform infrastructure already exists for clones/scale; the new part is applying it to a live cast avatar + a gizmo / numeric UI + the modifier toggle.

---

## 3. Camera Switcher "Reset all to defaults"
**What:** A one-click button to reset ALL camera-switcher settings — the 12 slots, every per-slot custom angle / subject assignment, and the auto-director — back to defaults.

**Why:** Quick clean slate after experimenting with slots.

**Rough path:** A "Reset all" button on the Switcher sub-tab that resets every `DirectorSwitcher*` control and the per-slot bank settings to their defaults (disarmed; slots to their default framings; `custom_enable` off; subject = Default) via the fork's existing `resetToDefault` pattern (the same idiom the weather panel's per-slider resets use).

**Effort:** Small / self-contained.

---

## Related open follow-ups (in-flight, from the 2026-07-30 session)
Not new requests — tracked here so the whole backlog lives in one place:
- **Projector-shaft drifting dust under denoising** — static dust works now (`NoiseSpeed = 0`); preserving *drifting* dust needs a high-frequency re-inject on top of the temporal-denoised base (projvol slice-2).
- **Stable-projector-shadow fix** — decouple the projector-volumetric / hero-beam render list from the 2-slot shadow auction (`BDMergeStableSpotShadows`); confirmed a pre-existing BD-merge bug, weather-wave-independent.
- **Switcher preset-row dedup** — the camera preset row currently shows on both the Cinematic and Operator sub-pages (harmless duplication from the panel split); trim to one.
- **Compatibility tiers** — has its own full brief: doc/COMPATIBILITY_PRESET_DEEP_RESEARCH.md.
