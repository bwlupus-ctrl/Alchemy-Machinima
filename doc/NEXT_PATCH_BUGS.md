# Next-Patch Bug List

Running list of bugs to address in the next patch after 2026-08-02b.

## 1. Advanced Graphics Preferences panel — fixed-height, clips settings / not scrollable
- **Reported:** 2026-08-02 (user).
- **Symptom:** the Advanced Graphics floater (`indra/newview/skins/default/xui/en/floater_preferences_graphics_advanced.xml`) is a fixed-height two-column layout with a bottom button bar; the lower rows of the left column (Flexiprims and anything below) clip behind the button bar, and there is no scroll. Adding the new "Machinima High LOD" checkbox pushed the left column past the bar.
- **Workaround shipped in 20260802b:** floater height 452->492, `vert_border` 377->417, `horiz_border` top/top_delta +40. Current settings fit again.
- **Proper fix (next patch):** wrap the two-column content in a `scroll_container` (or accordion) so the panel is robust to added settings and to small screens; verify on 1366x768 and 1280x720. Re-audit that every left+right column row is visible after any future addition. The +40 height bump is a stopgap, not a real fix.
