# Queued for the next Codex round: shell / C++ / generator items found by the Haiku legacy sweep (10/10 evening)

1. **Ghosts double scrollbar.** ALScrollFit's legacy-host path (alscrollfit.cpp:~497) wraps ghosts_legacy_host in an outer auto-scroller, while the Ghost Studio tabs have their own tab-local scrollers (wired by ALScrollFocus in alpanelghoststudio.cpp:437-440, on purpose). Fixes:
   - (a) add "ghosts_legacy_host" to ALKitShell::unwrappedPages() (alkitshell.cpp ~829) and fill it like dance/exposure;
   - (b) panel_ghost_studio.xml: the root is a fixed height=760 and ghost_studio_tabs a fixed 520. The tabs must follow the page height.
   - The checker's 4 "scroller-wiring" findings on panel_ghost_studio.xml may be false positives given the ALScrollFocus design. Verify; don't add a second scroll mechanism.
   - Also check filters_legacy_host_auto_scroll and props_legacy_host_auto_scroll for the same double scroll.
2. **Exposure exp_undo / exp_redo.** The XML places them at top=4 above the tabs, but at runtime they render at the page bottom (OVERLAP + CLIPPED_V). This is likely an ALScrollFit placement issue.
3. **Shell spills on EVERY page:**
   - ROSTER_SPILL cast_hint (147 at 1160 px; floater_director_v2 roster hint text);
   - DIRECTOR_SPILL kit_transport / btn_cast_chip / btn_hotkeys;
   - CAST_UNREACHABLE;
   - OFF_FLOATER.
4. **Generated kit-page issues (generator / kit):**
   - Bloom & Glow kc_bloom "more" card: a SpinCtrl Down button overlaps the Drop Down Button vs the MipCount spinner; open_lightbox_glow is 105 px wide but has 73 px of space.
   - Light â€º Actors: a floating "Open in window..." button sits over the Actors card title (panel_dpage_light_actors.xml).
5. **switcher_slot_N.** The dynamic label ("%d %s", alpaneldirectorswitcher.cpp:510) can still truncate. Elide it with a tooltip.
6. **STYLE_COMBO_CARET** findings at y<0 / minimum size: these look like scroll-position artifacts. Make the probe ignore widgets scrolled out of the viewport.
7. **Path editor trade-off (Haiku change, review it):** the accordion no longer scrolls itself, so the host path_scroll scrolls the 1160 px editor. Collapsed tabs leave blank space, and the suspend banner reserves a 52 px gap. Prefer a layout where collapsed sections shrink the document.

8. Page header description is ellipsised on every tab (e.g. 'Viewer-only rain, splashes...'): wrap to 2 lines or give it the width / tooltip per mockup.
9. Transport status line ('0 moving · CineCam: ...') clips at 900 px: elide with tooltip, keep timecode visible.

10. VCam page: stats text 'Prism 0/8 captures...' crammed on the checkbox row (generated panel_dpage_camera_vcam / generator).
11. Bottom-left rail cast hint cut off ('...Add to') - part of item 3 roster.


12. xui_layout_check.py does not model accordion_tab header (25) + padding (2+2) in fit_panel (llaccordionctrltab.cpp:960) -> missed the Path Play/Stop clip. Teach the checker; add a selftest fixture.

