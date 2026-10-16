# Wiring audit: Performance > Switchboard kit conversion

Static audit of the Switchboard page after it became a generated kit page (10/15, legacy conversion batch 1). No build, no commit, no viewer run: every row is checked against the generated XML, `settings.xml`, `aldirectoranimswitcher.cpp` and `alpaneldirectoranimswitcher.cpp`. Sources: mockup page `switchboard_tab`, `kit_pages.json` entry `performance_group/switchboard_tab` (hosts the v1 nodes of `panel_director_anim_switcher.xml`, page root class `panel_director_anim_switcher`), generated `panel_dpage_performance_switchboard.xml`.

## Result

- The page is no longer legacy: classification `kit (P0)`, pin 11 -> 10, `performance_group/switchboard_tab` removed from `KNOWN_LEGACY_PAGES`.
- 34 generated rows = 32 mockup rows + 2 added (`anim_selected_slot`, `anim_import_status`: the v1 status text boxes `ALPanelDirectorAnimSwitcher::postBuild` requires). 4 cards, 2 sections. Two mockup card ids were renamed (`selected_slot` -> `sw_selected_slot`, `playback` -> `sw_playback`) because kit widget names are global and the Shot and Takes pages already own those card names.
- The page root is the shared `ALPanelDirectorAnimSwitcher` (same pattern as Light > Actors): the existing `postBuild`, `draw`, bank mirroring and drag-and-drop of inventory animations keep working. Its `getChild` names all resolve (below). The v1 Director tab and the panel XML are unchanged.
- C++ changes: `alpaneldirectoranimswitcher.cpp` `refreshEditor` (the Anim/Pose switch) now hides and shows the rows' controls with `ALKitRow::setControlVisible`, which lays the card out again so the hidden kind leaves no gap (plain `setVisible` in the v1 tab). New kit row attribute `fill_width` (`alkitrow.h/.cpp`, `director_kit_gen.py`): a button row sized to its track instead of to its XML label, used by the 12 slot buttons because code rewrites their labels ("3 Intro").
- Label text boxes the panel code hides or reads (`anim_slot_uuid_text`, `anim_pose_name_text`, `anim_slot_priority_text`, plus kind, label and target) are the rows' own labels through `label_names`.

## C++ lookups (`ALPanelDirectorAnimSwitcher::postBuild`, `alpaneldirectoranimswitcher.cpp:52-78`)

All resolve in the generated tree with the same control class: `anim_slot_01..12` (button), `anim_slot_kind` / `anim_pose_load_method` / `anim_slot_target` / `anim_pose_name` / `anim_slot_priority` (combo), `anim_slot_uuid` / `anim_slot_label` (line editor), `anim_from_explorer` / `anim_slot_reset` (button), `anim_slot_speed` (spinner), `anim_slot_loop` / `anim_slot_snap` / `anim_slot_enabled` (kit switch, an LLCheckBoxCtrl), `anim_selected_slot` / `anim_program_status` / `anim_import_status` (text), and the three label text boxes above.

## Controls

Combo data values are carried over from the v1 XML by hosting (`sanitizeSlot` in `aldirectoranimswitcher.cpp:84-93` is the consumer clamp). All bank rows read back through `refreshEditor` (on slot select, bank change, `saveBank`).

| # | widget | kind | setting or handler | type, default, range / data values | status | Undo owner |
|---|---|---|---|---|---|---|
| 1 | anim_switcher_armed | toggle | `DirectorAnimSwitcherArmed` (setting binding; `onSlotButton` and `refreshProgramState` read it) | Boolean, 0 | OK | kit setting history (this page; guarded against newer edits) |
| 2 | anim_switcher_auto | toggle | `DirectorAnimSwitcherAuto` (`readConfig`) | Boolean, 0 | OK | kit setting history (this page; guarded against newer edits) |
| 3 | anim_interval | number | `DirectorAnimSwitcherIntervalSec` | F32, 6.0, 0.25..120 step 0.5 | OK | kit setting history (this page; guarded against newer edits) |
| 4 | anim_jitter | number | `DirectorAnimSwitcherJitterSec` | F32, 0.0, 0..30 step 0.1 | OK | kit setting history (this page; guarded against newer edits) |
| 5 | anim_sequence | toggle | `DirectorAnimSwitcherSequence` | Boolean, 0 | OK | kit setting history (this page; guarded against newer edits) |
| 6 | anim_program_status | readout | none; `refreshProgramState` writes it (mirrors armed / active slot) | text | OK | n/a (action/output; no widget step) |
| 7 | anim_help_hint | readout | none (static copy) | text | OK | n/a (action/output; no widget step) |
| 8-19 | anim_slot_01..12 | button (toggle, `fill_width`) | `onSlotButton(slot)`: selects the slot, punches it when armed; `refreshButtons` sets "N label" and tooltip, `refreshProgramState` sets the toggle state | slot 0..11 = `SLOT_COUNT` | OK | n/a (action/output; no widget step) |
| 20 | anim_selected_slot | readout | `refreshEditor` writes "Slot N setup" | text | OK | n/a (action/output; no widget step) |
| 21 | anim_slot_kind | combo | `onSlotKind` -> bank `mKind` | 0 Anim, 1 Pose (`KIND_ANIM`, `KIND_POSE`) | OK | none (widget_history=false; selected subject or selection) |
| 22 | anim_slot_uuid | line editor | `onAnimUUID`: trims, validates, keeps the stored id on a typo | UUID text, 36 chars | OK | none (widget_history=false; selected subject or selection) |
| 23 | anim_from_explorer | button | `onFromExplorer`: copies `AnimationExplorer::getSelectedAnimId()` | n/a | OK | n/a (action/output; no widget step) |
| 24 | anim_import_status | readout | written by `onAnimUUID`, `onFromExplorer`, `onResetSlot`, `onSlotButton`, drag-and-drop | text | OK | n/a (action/output; no widget step) |
| 25 | anim_slot_label | line editor | `onSlotLabel` -> `mLabel` (clamped to 40 characters) | text | OK | none (widget_history=false; selected subject or selection) |
| 26 | anim_slot_target | combo | `onSlotTarget` -> `mTarget` | 0 Cast, 1 Self, 2 Ghosts (`ETarget`; `sanitizeTarget`) | OK | none (widget_history=false; selected subject or selection) |
| 27 | anim_slot_reset | button | `onResetSlot`: default slot with label "Anim N" | n/a | OK | n/a (action/output; no widget step) |
| 28 | anim_pose_name | combo (Advanced) | `onPoseName` -> `mPoseName`; items rebuilt by `refreshPoseChoices` from the user poses folder | string values, "(empty)" = "" | OK | none (widget_history=false; selected subject or selection) |
| 29 | anim_pose_load_method | combo (Advanced) | `onPoseLoadMethod` -> `mPoseLoadMethod` | 7 all, 1 rotations, 2 positions, 3 scales, 4 rot+pos, 5 rot+scales, 6 pos+scales, 8 right hand, 9 left hand, 10 face, 11 selective, 12 selective rot (clamped `ROTATIONS..SELECTIVE_ROT`) | OK | none (widget_history=false; selected subject or selection) |
| 30 | anim_slot_priority | combo | `onSlotPriority` -> `mPriority` | -1 Default, 0..7 (`LL_CHARACTER_MAX_PRIORITY`) | OK | none (widget_history=false; selected subject or selection) |
| 31 | anim_slot_speed | number | `onSlotSpeed` -> `mSpeed` | F32, 1.0, 0.1..5 step 0.1 (`SPEED_MIN..SPEED_MAX`) | OK | none (widget_history=false; selected subject or selection) |
| 32 | anim_slot_loop | toggle | `onSlotLoop` -> `mLoop` | Boolean | OK | none (widget_history=false; selected subject or selection) |
| 33 | anim_slot_snap | toggle | `onSlotSnap` -> `mSnapOnCut` | Boolean | OK | none (widget_history=false; selected subject or selection) |
| 34 | anim_slot_enabled | toggle | `onSlotEnabled` -> `mEnabled` ("Use in auto") | Boolean | OK | none (widget_history=false; selected subject or selection) |

Rows 28 and the Anim-only rows (22, 23, 30-33 and the pose pair) swap in and out with the slot kind; the hidden kind's rows leave the layout.

## Checks run

`director_kit_gen.py` check and selftest, `director_page_sources.py --check`, `kit_lint.py --all`, `xui_layout_check.py` selftest and `--changed HEAD`, `director_functional_manifest.py` plus self-tests, `director_parity.py --selftest`, `ui_draw_lint.py --changed HEAD`, `zs_check.ps1` (`alkitrow.cpp`, `alpaneldirectoranimswitcher.cpp`), `prebuild_guard.ps1 -ChecksOnly`. Not run (rules): build, viewer, in-world self-test. Worth an in-world look: slot button text width, the Anim/Pose row swap, inventory animation drop onto the page.
