# Wiring audit: Light > Light Cues kit conversion

Static audit of the Light Cues page after it became a generated kit page (10/15, legacy conversion batch 1). No build, no commit, no viewer run: every row is checked against the generated XML and the C++ source. Before this, the page was a single launcher card ("Open ..." button plus a one-line summary) that opened the Lighting Cue Stack floater. Sources: mockup page `light_cues_tab` (1 card, 5 rows, unchanged), `kit_pages.json` entry `light_group/light_cues_tab` (hosts the cue list node of `panel_light_cues_src.xml`, root class `panel_light_cues`), generated `panel_dpage_light_cues.xml`.

## Result

- The page is no longer legacy: classification `kit (P0)`, pin 10 -> 9, `light_group/light_cues_tab` removed from `KNOWN_LEGACY_PAGES`.
- 5 generated rows = the 5 mockup rows. The v1 launcher widgets (`card_light_cues*`, `light_cues_flow_*`) are retired; the card names are in `parity_allow`. `LLFloaterDirector::bindLauncherPage` still contains the branch for `card_light_cues_open`; it finds nothing and does nothing.
- New page logic class `ALPanelLightCues` (`alpanellightcues.h/.cpp`, registered as `panel_light_cues`, added to `CMakeLists.txt`). It reads and edits the same model the floater does (`ALCineLightRig` cue list, `captureCue`, `setCueList`, `cueGoto`, `cueRelease`) and does nothing else.
- Floater change: `ALFloaterCineLightCues::onOpen(const LLSD& key)` (`alfloatercinelightcues.h/.cpp`): an integer key selects that cue and loads the editor. The floater is `single_instance`, so a row click on the page re-uses the open floater. No existing caller passes an integer key.
- `director_parity.py` `root_owner` gained `panel_light_cues` and `panel_practicals` (same mechanism as `panel_avatar_light_response`): the page's `getChild` lookups are the ones in its own class, not the floater's. Without it the gate counted the floater's editor widgets (`cue_list_load`, `cue_label`, ...) as missing from the page.

## Controls

| # | widget | kind | handler / consumer | notes | status | Undo owner |
|---|---|---|---|---|---|---|
| 1 | cue_rows | list (`scroll_list`, columns `lc_number`, `lc_label`, `lc_at`, `lc_fade`, `lc_delay`, `lc_curve`, `lc_follow`) | rows built by `refreshRows` from `ALCineLightRig::cueList()` (same fields and formats as the floater); selection commit -> `onSelection` opens `cine_light_cues` at that index | refresh is gated on `cueRevision()` and the selected rig slot, so `draw` costs two compares; selection is preserved across rebuilds. The columns are renamed `lc_*` because widget names are global in the kit | OK | none (widget_history=false; selected subject or selection) |
| 2 | cue_capture (Add) | button | `onAdd`: `captureCue("Cue N")`, `sanitizeCue`, append, `setCueList`; selects the new row | complete live-rig snapshot with the default cue timing; the floater's editor timing is not used from here. One undo step ("Add light cue") via `ALKitHistory::recordCustom` (before/after cue list, rig slot) | OK | custom cue-list snapshot on this Light Cues page; captures rig slot |
| 3 | cue_delete (Remove) | button | `onRemove`: erase the selected cue, `setCueList` | one undo step ("Remove light cue"); disabled with no selection | OK | custom cue-list snapshot on this Light Cues page; captures rig slot |
| 4 | cue_goto (Play) | button | `onPlay`: `cueGoto(selected, presentation time, false)` (the floater's Goto without snap) | disabled with no selection, and in timecode mode (as the floater's transport) | OK | n/a (action/output; no widget step) |
| 5 | cue_release (Stop) | button | `onStop`: `cueRelease(presentation time)` | disabled in timecode mode | OK | n/a (action/output; no widget step) |

## Checks run

`director_kit_gen.py` check and selftest, `director_page_sources.py --check`, `kit_lint.py --all`, `xui_layout_check.py` selftest and `--changed HEAD`, `director_functional_manifest.py` plus self-tests, `director_parity.py` `--selftest` and `--groups light_group`, `ui_draw_lint.py --changed HEAD` (0 new), `zs_check.ps1` (`alpanellightcues.cpp`, `alfloatercinelightcues.cpp`), `prebuild_guard.ps1 -ChecksOnly`. Not run (rules): build, viewer, in-world self-test. Worth an in-world look: row click opening the floater at the right cue, undo of Add and Remove, Play and Stop on the live rig.
