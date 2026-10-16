# Wiring audit: Cast > Move kit conversion

Static audit of the Move page after it became a generated kit page (10/15, legacy conversion batch 2). No build, no commit, no viewer run: every row is checked against the generated XML, `settings.xml` and the C++ source. Sources: mockup page `move_tab` (5 cards, 19 rows; only search keywords `kw` were added, no row added or removed), `kit_pages.json` entry `cast_group/move_tab` (hosts the v1 nodes of the shared `panel_actor_mover.xml` plus the new generator input `panel_director_move_src.xml` for the console-only group widgets), generated `panel_dpage_cast_move.xml`.

## Result

- The page is no longer legacy: classification `kit (P0)`, pin 6 -> 5, `cast_group/move_tab` removed from `KNOWN_LEGACY_PAGES`.
- 19 generated rows = the 19 mockup rows, 5 cards, 2 sections (Walk, Groups), Who chip `scope_radio`. Every v1 control the code looks up still resolves (see the lookup table).
- The page root is now `ALPanelActorMover` (class `panel_actor_mover`, `root_name` `move_tab_logic`), the same class the standalone Actor Mover floater embeds, so Walk/Stop/scope/heading keep one implementation. Cast selection still reaches it through `LLFloaterDirector::refreshMoveTab` -> `setSelectedActors`.
- The v1 Director tab and the standalone floater are unchanged (`floater_director.xml`, `floater_actor_mover.xml`, `panel_actor_mover.xml` untouched).
- v1 names with no row (parity_allow, with reasons): `speed_lbl`, `dist_lbl`, `cadence_lbl`, `end_lbl`, `group_run_lbl`, `groups_lbl` (static labels replaced by row labels and the card title), `speed_spinner`, `dist_spinner`, `cadence_spinner` (duplicate numeric boxes of the sliders; kit sliders show their value).

## C++ lookups

| lookup | where | change |
|---|---|---|
| `heading_dial` (`ALCompassDial`) | `alpanelactormover.cpp` `postBuild` | `findChild("heading_dial")`, else the page's `heading_spinner` row (a `compass_dial` upgrade of the v1 spinner, bound to `ActorMoverHeading`). The standalone floater still finds `heading_dial`. |
| `btn_stop` | `alpanelactormover.cpp` `postBuild` | `findChild("btn_stop")`, else `move_btn_stop`. Kit widget names are page-unique and Camera > Takes owns `btn_stop` (kit_lint widget-name), so the row is renamed through `row_overrides` (`source_widget: btn_stop`). |
| `actor_mover_panel` | `llfloaterdirector.cpp` `bindMovePage` | `dynamic_cast<ALPanelActorMover*>(root)` first (the page root is the transport), else the v1 `actor_mover_panel` child. |
| `group_run_combo`, `btn_group_start`, `btn_group_stop`, `groups_list`, `group_delay_spinner`, `group_rename_editor`, `btn_group_rename`, `btn_group_dissolve` | `llfloaterdirector.cpp` `bindMovePage` | unchanged; resolve to the hosted nodes of `panel_director_move_src.xml`. |
| `scope_radio`, `btn_walk` | `alpanelactormover.cpp` | unchanged names. |

## Controls

Defaults from `settings.xml`; ranges from the hosted v1 node (equal to the mockup row).

| # | widget | kind | setting / handler | type, default, range | consumer / read-back | status |
|---|---|---|---|---|---|---|
| 1 | scope_radio | radio (Who) | `ALPanelActorMover::onScopeCommit` writes `ActorMoverSync`; `draw()` reads it back | Boolean, 1; items Selected actors = 0, Everyone = 1 | `onClickWalk/Stop` (`LLActorMover::startAll/stopAll` or the selection) | OK |
| 2 | btn_walk | button | `onClickWalk` | n/a | `LLActorMover::start(id)` per selected actor, or `startAll` | OK |
| 3 | move_btn_stop | button | `onClickStop` | n/a | `LLActorMover::stop(id)` / `stopAll` | OK |
| 4 | speed_slider | slider | `ActorMoverSpeed` | F32, 1.0, 0.05..150 step 0.05, 2 dp | `LLActorMover::start` | OK |
| 5 | dist_slider | slider | `ActorMoverDistance` | F32, 6.0, 0.5..2048 step 0.5, 1 dp | `LLActorMover::start` | OK |
| 6 | heading_spinner | compass dial | `ActorMoverHeading` (control_name, plus `onDialCommit` live write) | F32, 0, -180..180 step 5 | `LLActorMover::start`; `draw()` reads the setting back to the needle | OK |
| 7 | end_mode_combo | combo | `ActorMoverEndMode` | S32, 0; Stop at end = 0, Loop = 1, Ping-pong = 2 (item values equal v1 and `llactormover.cpp:4247`) | walk end handling | OK |
| 8 | move_hint | readout | none (authored static text) | text | n/a | OK |
| 9 | cadence_slider (Advanced) | slider | `ActorMoverWalkNominal` | F32, 3.0, 1..150 step 0.1, 1 dp | walk animation speed scaling | OK |
| 10 | custom_anim_check (card enable) | toggle | `ActorMoverUseCustomAnim` | Boolean, 0 | `llactormover.cpp:149`; also set by the Animation Explorer (`animationexplorer.cpp:286`) | OK |
| 11 | custom_anim_editor | text | `ActorMoverCustomAnim` | String, empty; `max_length_chars` 36 | `llactormover.cpp:150`; filled by Animation Explorer (`:285`) | OK |
| 12 | custom_anim_priority_combo | combo | `ActorMoverCustomAnimPriority` | S32, -1; Baked = -1, Pri 0..4 = 0..4 (v1 values, `llactormover.cpp:174`) | custom walk priority | OK |
| 13 | group_run_combo | combo | filled by `refreshGroupControls` (`mLastGroupNames`); read by `onClickGroup` | code-driven | Start/Stop group | OK |
| 14 | group_delay_spinner | spinner | `onCommitGroupDelay`; shown for the selected group by `refreshGroupControls` | 0..120 step 0.5, 1 dp, saved with the scene | staggered starts | OK |
| 15 | btn_group_start | button | `onClickGroup(true)` | n/a | `LLActorMover::start` per member | OK |
| 16 | btn_group_stop | button | `onClickGroup(false)` | n/a | `LLActorMover::stop` per member | OK |
| 17 | groups_list | list (columns `group`, `members`, `delay`) | `onGroupsListSelect`; rebuilt by `refreshGroupControls` | code-driven | highlights members in the cast list | OK |
| 18 | group_rename_editor | text | `onClickGroupRename` (Enter) | `max_length_chars` 32 | retags members | OK |
| 19 | btn_group_rename | button | `onClickGroupRename` | n/a | retags members | OK |
| 20 | btn_group_dissolve | button | `onClickGroupDissolve` | n/a | untags members | OK |

(Row 10 is the card switch of Custom walk anim, not a mockup row: 19 mockup rows plus 1 card enable.)

## Checks run

`director_kit_gen.py` check and selftest, `director_page_sources.py --check`, `kit_lint.py --all`, `xui_layout_check.py` selftest and `--changed HEAD`, `director_functional_manifest.py --check`, `director_parity.py` selftest and `--groups light_group`, `zs_check.ps1` (`alpanelactormover.cpp`, `llfloaterdirector.cpp`), `prebuild_guard.ps1 -ChecksOnly`. Not run (rules): build, viewer, in-world self-test. Worth an in-world look: Walk/Stop from the page with Selected and Everyone scope, the Heading needle, and group Start/Stop after tagging groups before the page was first opened.
