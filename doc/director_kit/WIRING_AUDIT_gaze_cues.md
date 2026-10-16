# Wiring audit: Performance > Gaze Cues kit conversion

Static audit of the Gaze Cues page after it became a generated kit page (10/15, legacy conversion batch 1). No build, no commit, no viewer run: every row is checked against the generated XML and the C++ source. Before this, the page was a single launcher card that opened the Gaze Cues floater. Sources: mockup page `gaze_cues_tab` (1 card, 5 rows; the card id `cues` was renamed `gaze_cue_track` because kit card names are global and Light Cues owns `cues`), `kit_pages.json` entry `performance_group/gaze_cues_tab` (hosts the cue list node of `panel_gaze_cues_src.xml`, root class `panel_gaze_cues`), generated `panel_dpage_performance_gaze_cues.xml`.

## Result

- The page is no longer legacy: classification `kit (P0)`, pin 8 -> 7, `performance_group/gaze_cues_tab` removed from `KNOWN_LEGACY_PAGES`.
- 5 generated rows = the 5 mockup rows. The v1 launcher widgets (`card_gaze_cues*`, `gaze_cues_flow_*`) are retired (`parity_allow`); the `card_gaze_cues_open` branch in `LLFloaterDirector::bindLauncherPage` now finds nothing.
- New page logic class `ALPanelGazeCues` (`alpanelgazecues.h/.cpp`, registered as `panel_gaze_cues`, added to `CMakeLists.txt`). The actor is the first Cast selection, or the agent when nothing is selected (what the retired launcher chose). It edits the actor's cue list through `LLDirectorCast::getGazeCues` / `setGazeCues`, the same calls the floater makes.
- New engine API (the Play and Stop rows had no engine support): `LLActorMover::setGazeCuePreview(actor, index)`, `clearGazeCuePreview(actor)`, `hasGazeCuePreview(actor)` (`llactormover.h/.cpp`). `applyDirectorLookAt` swaps the actor's cue list for a one-cue copy (start moved to 0) evaluated on preview-local time. When no preview exists the added code is one `map::empty()` test, so the normal track path is unchanged. Like the cue track itself, the preview drives actors that have Look at camera on (the Play tooltip says so). The preview ends on Stop, on any list edit from this page, when the cue index goes stale, and on `clearAllDirectorLookAtRuntime` (logout).
- Floater changes (`alfloatergazecues.h/.cpp`): public statics `makeDefaultCue` (the former `defaultCue` body, now shared), `targetLabel`, `macroLabel`, and `selectCueIndex(index)`. The floater behaves as before.
- `director_parity.py` `root_owner` gained `panel_gaze_cues` (see the Light Cues audit).

## Controls

| # | widget | kind | handler / consumer | notes | status | Undo owner |
|---|---|---|---|---|---|---|
| 1 | cue_list | list (`scroll_list`, columns `gc_time`, `gc_target`, `gc_macro`) | rows built by `refreshRows` (same text as the floater: start time `%.3f`, target label, macro label); selection commit -> `onSelection` shows `gaze_cues` for the actor (key = actor id string) and `selectCueIndex` | rebuild is gated on `getGazeCueRevision()` and the actor id; selection survives a rebuild for the same actor | OK | none (widget_history=false): cue list selection is not data; cue edits are page-recorded (ALKitHistory::recordCustom) |
| 2 | btn_cue_add | button | `onAdd`: `makeDefaultCue(actor)` appended, `setGazeCues` (sanitizes and sorts), then selects the new cue | one undo step ("Add gaze cue") via `ALKitHistory::recordCustom` with the stored before/after lists | OK | n/a (button, never recorded) |
| 3 | btn_cue_delete | button | `onRemove`: erase the selected cue, `setGazeCues` | one undo step ("Remove gaze cue"); disabled with no selection | OK | n/a (button, never recorded) |
| 4 | gaze_cue_play | button | `onPlay` -> `LLActorMover::setGazeCuePreview(actor, selected)` | enabled with a selection and the actor in the cast; returns false (no change) otherwise | OK | n/a (button, never recorded) |
| 5 | gaze_cue_stop | button | `onStop` -> `clearGazeCuePreview(actor)` | enabled only while a preview runs (`hasGazeCuePreview`) | OK | n/a (button, never recorded) |

The page contract (`pages/gaze_cues_tab.md`) suggests an `ALKitHistory::globalBarrier` around Play and Stop; it is not implemented: the preview never edits the cue list or any setting, so there is nothing to undo around it, and a barrier would clear the user's undo stack for no reason.

## Checks run

`director_kit_gen.py` check and selftest, `director_page_sources.py --check`, `kit_lint.py --all`, `xui_layout_check.py` selftest and `--changed HEAD`, `director_functional_manifest.py` plus self-tests, `director_parity.py` `--selftest` and `--groups light_group`, `ui_draw_lint.py --changed HEAD` (0 new), `zs_check.ps1` (`alpanelgazecues.cpp`, `alfloatergazecues.cpp`, `llactormover.cpp`), `prebuild_guard.ps1 -ChecksOnly`. Not run (rules): build, viewer, in-world self-test. Worth an in-world look: Play on an actor with Look at camera on (the cue should acquire from its authored target and the track resume after Stop), row click opening the floater at the right cue, undo of Add and Remove.
