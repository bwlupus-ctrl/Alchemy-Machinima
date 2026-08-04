# Next-Patch Bug List

Running list of bugs to address in the next patch after 2026-08-02b.

## 1. Advanced Graphics Preferences panel — fixed-height, clips settings / not scrollable
- **Reported:** 2026-08-02 (user).
- **Symptom:** the Advanced Graphics floater (`indra/newview/skins/default/xui/en/floater_preferences_graphics_advanced.xml`) is a fixed-height two-column layout with a bottom button bar; the lower rows of the left column (Flexiprims and anything below) clip behind the button bar, and there is no scroll. Adding the new "Machinima High LOD" checkbox pushed the left column past the bar.
- **Workaround shipped in 20260802b:** floater height 452->492, `vert_border` 377->417, `horiz_border` top/top_delta +40. Current settings fit again.
- **Proper fix (next patch):** wrap the two-column content in a `scroll_container` (or accordion) so the panel is robust to added settings and to small screens; verify on 1366x768 and 1280x720. Re-audit that every left+right column row is visible after any future addition. The +40 height bump is a stopgap, not a real fix.

## 2. Overlay ghost stylization — secondary findings (deferred from the 2026-08-03 overlay fix cae1629eedc)
- **PBR overlay-clone double-tint by baseColor.** PBR VBs carry `MAP_COLOR` (llvovolume.cpp:6442) with `mBaseColor` baked in (llface.cpp:1441); the clone `clone_color` path multiplies the base-color factor AGAIN as a uniform (llactormover.cpp:4703/4731; static 4908) → RGB (and blend alpha) can be effectively squared. Overlay-clone approximation debt, NOT the full LLGhostAvatar clone.
- **Non-clone stylizations omit real alpha-blend batches.** Ghost / Hologram / X-ray / toolkit looks use solid-only sweeps (llactormover.cpp:5215 / 5156 / 5195), so true alpha-pool hair/sheer layers are absent from those styles (independent of the shininess fix). Expanding stylized looks to alpha-pool geometry is a separate behavioral change.
- **Hologram-Echo bypasses vertex RGB** for its two offset samples (actorghostF.glsl:456) → the R/B echo channels omit authored vertex tint/base-color → color mismatch.

## 3. Clone walk / runtime — low-severity (deferred from the 2026-08-03 round-3 review)
- Follower leader-ref not rekeyed when the leader has zero own records (provably inert per advanceFollower — no observable impact).
- A ghost that dies within the 6-frame debounce BEFORE any anchor seed suspends with a (0,0,0) anchor → auto-resume fails (manual Resume still works); pre-existing, graceful.
- 1-frame cosmetic flicker on live-refresh of a walking clone (new body placed at authored foot until applyOverride repaints it that frame).
- `refreshLifecycleStates` skips `STATE_LOCKED` instances (alghoststudio.cpp:2135) → a LOCKED clone's runtime death isn't detected/parked (locked clones are meant to be persistent; pre-existing).
