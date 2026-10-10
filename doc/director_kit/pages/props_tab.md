# Props — Director Kit page contract

Page key: `props_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (1/33 setting-bound rows; 33 C++-referenced rows).

## Real owners

- `panel_prop_mover` / `panel` at `indra/newview/skins/default/xui/en/panel_prop_mover.xml:12` (33 matching rows).
- `floater_actor_mover` / `floater` at `indra/newview/skins/default/xui/en/floater_actor_mover.xml:9` (2 matching rows).
- `panel_actor_mover` / `panel` at `indra/newview/skins/default/xui/en/panel_actor_mover.xml:19` (2 matching rows).
- `floaterbulkperms` / `floater` at `indra/newview/skins/default/xui/en/floater_bulk_perms.xml:7` (1 matching rows).
- `floater_director` / `floater` at `indra/newview/skins/default/xui/en/floater_director.xml:15` (1 matching rows).
- `Event` / `floater` at `indra/newview/skins/default/xui/en/floater_event.xml:8` (1 matching rows).
- `floater_help_browser` / `floater` at `indra/newview/skins/default/xui/en/floater_help_browser.xml:10` (1 matching rows).
- `asset_hex_editor` / `floater` at `indra/newview/skins/default/xui/en/floater_hex_editor.xml:4` (1 matching rows).
- `linkreplace` / `floater` at `indra/newview/skins/default/xui/en/floater_linkreplace.xml:3` (1 matching rows).
- `prism_manager` / `floater` at `indra/newview/skins/default/xui/en/floater_prism_manager.xml:10` (1 matching rows).
- `scene_explorer` / `floater` at `indra/newview/skins/default/xui/en/floater_scene_explorer.xml:9` (1 matching rows).
- `experince_list_editor` / `panel` at `indra/newview/skins/default/xui/en/panel_experience_list_editor.xml:11` (1 matching rows).
- `panel_flycam_recorder` / `panel` at `indra/newview/skins/default/xui/en/panel_flycam_recorder.xml:17` (1 matching rows).
- `panel` / `panel` at `indra/newview/skins/default/xui/en/panel_hint.xml:1` (1 matching rows).
- `panel` / `panel` at `indra/newview/skins/default/xui/en/panel_hint_image.xml:1` (1 matching rows).
- `panel_profile_web` / `panel` at `indra/newview/skins/default/xui/en/panel_profile_web.xml:3` (1 matching rows).
- `panel_temporal_capture` / `panel` at `indra/newview/skins/default/xui/en/panel_temporal_capture.xml:17` (1 matching rows).
- Referencing classes: `ALPanelPropMover` (32), `LLFloaterHexEditor` (7), `LLPanelExperienceListEditor` (2), `LLFloaterEvent` (2), `LLFloaterHelpBrowser` (2), `LLPanelProfileWeb` (2), `LLHintPopup` (1), `LLFloaterDirector` (1), `ALFloaterSceneExplorer` (1), `ALPanelTemporalCapture` (1), `LLFloaterActorMover` (1), `LLFloaterBulkPermission` (1), `LLFloaterLinkReplace` (1), `LLFloaterPrismManager` (1), `ALPanelActorMover` (1), `ALPanelFlycamRecorder` (1).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALPropMoverKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `props_enroll` / `hint_text` (Tip) | `text` `indra/newview/skins/default/xui/en/floater_actor_mover.xml:51`, `text` `indra/newview/skins/default/xui/en/panel_hint.xml:13`, `text` `indra/newview/skins/default/xui/en/panel_hint_image.xml:31`, `text` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:21` | `ALPropMoverKitAdapter` command/model binding → `LLHintPopup::postBuild()` (`indra/newview/llhints.cpp:185`) | none |
| `props_enroll` / `btn_enroll` (Enroll object) | `button` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:36` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:89`) | none |
| `props_enroll` / `prop_list` (Props) | `scroll_list` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:58` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:56`) | none |
| `props_enroll` / `btn_remove` (Remove) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:290`, `button` `indra/newview/skins/default/xui/en/panel_experience_list_editor.xml:75`, `button` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:46` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:90`), `LLFloaterDirector::postBuild()` (`indra/newview/llfloaterdirector.cpp:354`), `LLPanelExperienceListEditor::postBuild()` (`indra/newview/llpanelexperiencelisteditor.cpp:61`), `LLPanelExperienceListEditor::postBuild()` (`indra/newview/llpanelexperiencelisteditor.cpp:65`) | none |
| `props_enroll` / `status_text` (Status) | `text` `indra/newview/skins/default/xui/en/floater_actor_mover.xml:41`, `floater.string` `indra/newview/skins/default/xui/en/floater_bulk_perms.xml:16`, `text` `indra/newview/skins/default/xui/en/floater_event.xml:35`, `text` `indra/newview/skins/default/xui/en/floater_help_browser.xml:54`, `text` `indra/newview/skins/default/xui/en/floater_hex_editor.xml:11`, `text` `indra/newview/skins/default/xui/en/floater_linkreplace.xml:82`, `text` `indra/newview/skins/default/xui/en/floater_prism_manager.xml:888`, `text` `indra/newview/skins/default/xui/en/floater_scene_explorer.xml:687`, `text` `indra/newview/skins/default/xui/en/panel_profile_web.xml:27`, `text` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:605`, `text` `indra/newview/skins/default/xui/en/panel_temporal_capture.xml:259` | `ALPropMoverKitAdapter` command/model binding → `ALFloaterSceneExplorer::updateStatusText()` (`indra/newview/alfloatersceneexplorer.cpp:2601`), `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:62`), `ALPanelTemporalCapture::postBuild()` (`indra/newview/alpaneltemporalcapture.cpp:31`), `LLFloaterActorMover::draw()` (`indra/newview/llfloateractormover.cpp:237`), `LLFloaterBulkPermission::handleInventory()` (`indra/newview/llfloaterbulkpermission.cpp:336`), `LLFloaterEvent::handleMediaEvent()` (`indra/newview/llfloaterevent.cpp:90`), `LLFloaterEvent::handleMediaEvent()` (`indra/newview/llfloaterevent.cpp:94`), `LLFloaterHelpBrowser::handleMediaEvent()` (`indra/newview/llfloaterhelpbrowser.cpp:103`), `LLFloaterHelpBrowser::handleMediaEvent()` (`indra/newview/llfloaterhelpbrowser.cpp:107`), `LLFloaterHexEditor::onOpen()` (`indra/newview/llfloaterhexeditor.cpp:86`), `LLFloaterHexEditor::imageCallback()` (`indra/newview/llfloaterhexeditor.cpp:166`), `LLFloaterHexEditor::imageCallback()` (`indra/newview/llfloaterhexeditor.cpp:181`), `LLFloaterHexEditor::assetCallback()` (`indra/newview/llfloaterhexeditor.cpp:205`), `LLFloaterHexEditor::assetCallback()` (`indra/newview/llfloaterhexeditor.cpp:230`), `LLFloaterHexEditor::onClickSave()` (`indra/newview/llfloaterhexeditor.cpp:418`), `LLFloaterHexEditor::onSaveComplete()` (`indra/newview/llfloaterhexeditor.cpp:436`), `LLFloaterLinkReplace::postBuild()` (`indra/newview/llfloaterlinkreplace.cpp:69`), `LLFloaterPrismManager::postBuild()` (`indra/newview/llfloaterprismmanager.cpp:333`), `LLPanelProfileWeb::handleMediaEvent()` (`indra/newview/llpanelprofile.cpp:1960`), `LLPanelProfileWeb::handleMediaEvent()` (`indra/newview/llpanelprofile.cpp:1980`) | none |
| `prop_path` / `node_list` (Nodes) | `scroll_list` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:115` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:57`) | none |
| `prop_path` / `btn_node_add` (Add node) | `button` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:83` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:91`) | none |
| `prop_path` / `btn_node_del` (Delete node) | `button` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:93` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:92`) | none |
| `prop_path` / `btn_node_clear` (Clear path) | `button` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:103` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:93`) | none |
| `prop_path` / `node_skid_spin` (Skid (deg)) | `spinner` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:145` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:60`) | none |
| `prop_path` / `node_speed_spin` (Node m/s) | `spinner` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:160` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:61`) | none |
| `drive` / `btn_drive` (Drive) | `button` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:212` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:94`) | none |
| `drive` / `btn_stop` (Stop) | `button` `indra/newview/skins/default/xui/en/panel_actor_mover.xml:310`, `button` `indra/newview/skins/default/xui/en/panel_flycam_recorder.xml:87`, `button` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:222` | `ALPropMoverKitAdapter` command/model binding → `ALPanelActorMover::postBuild()` (`indra/newview/alpanelactormover.cpp:46`), `ALPanelFlycamRecorder::postBuild()` (`indra/newview/alpanelflycamrecorder.cpp:70`), `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:95`) | none |
| `drive` / `btn_drive_all` (Drive all) | `button` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:232` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:96`) | none |
| `drive` / `btn_stop_all` (Stop all) | `button` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:242` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:97`) | none |
| `drive` / `path_speed_spin` (Path m/s) | `spinner` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:177` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:58`) | none |
| `drive` / `end_mode_combo` (At last node) | `combo_box` `indra/newview/skins/default/xui/en/panel_actor_mover.xml:246`, `combo_box` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:186` | kit `combo` ↔ `ActorMoverEndMode` `S32`; `ALPropMoverKitAdapter` preserves/calls `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:59`) | page `move_tab` |
| `orient` / `btn_capture_orient` (Capture Orient) | `button` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:268` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:64`) | none |
| `orient` / `rotation_mode_combo` (Orient mode) | `combo_box` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:277` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:65`) | none |
| `hover` / `hover_wave_combo` (Shape) | `combo_box` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:322` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:68`) | none |
| `hover` / `hover_amp_spin` (Amp (m)) | `spinner` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:346` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:69`) | none |
| `hover` / `hover_freq_spin` (Speed (Hz)) | `spinner` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:361` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:70`) | none |
| `rock` / `rock_axis_combo` (Axes) | `combo_box` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:380` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:72`) | none |
| `rock` / `rock_amp_spin` (Amp (deg)) | `spinner` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:408` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:73`) | none |
| `rock` / `rock_freq_spin` (Speed (Hz)) | `spinner` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:423` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:74`) | none |
| `spin` / `spin_axis_combo` (Axis) | `combo_box` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:455` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:77`) | none |
| `spin` / `spin_speed_spin` (Deg/s) | `spinner` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:483` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:78`) | none |
| `shuttle` / `btn_shuttle` (Shuttle) | `button` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:595` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:120`), `?::?()` (`indra/newview/alpanelpropmover.cpp:320`) | none |
| `shuttle` / `shuttle_axis_combo` (Axis) | `combo_box` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:505` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:80`) | none |
| `shuttle` / `shuttle_sign_combo` (Direction) | `combo_box` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:531` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:81`) | none |
| `shuttle` / `shuttle_distance_spin` (Distance (m)) | `spinner` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:570` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:82`) | none |
| `shuttle` / `shuttle_speed_spin` (Speed (m/s)) | `spinner` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:585` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:83`) | none |
| `shuttle` / `shuttle_slope_spin` (Slope (deg)) | `spinner` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:555` | `ALPropMoverKitAdapter` command/model binding → `ALPanelPropMover::postBuild()` (`indra/newview/alpanelpropmover.cpp:84`) | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALFloaterSceneExplorer::updateStatusText()` — `indra/newview/alfloatersceneexplorer.cpp:2601` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelActorMover::postBuild()` — `indra/newview/alpanelactormover.cpp:46` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelFlycamRecorder::postBuild()` — `indra/newview/alpanelflycamrecorder.cpp:70` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `?::?()` — `indra/newview/alpanelpropmover.cpp:320` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPropMover::postBuild()` — `indra/newview/alpanelpropmover.cpp:56, 57, 58, 59, 60, 61, 62, 64, 65, 68, 69, 70, 72, 73, 74, 77, 78, 80, 81, 82, 83, 84, 89, 90, 91, 92, 93, 94, 95, 96, 97, 120` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelTemporalCapture::postBuild()` — `indra/newview/alpaneltemporalcapture.cpp:31` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterActorMover::draw()` — `indra/newview/llfloateractormover.cpp:237` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterBulkPermission::handleInventory()` — `indra/newview/llfloaterbulkpermission.cpp:336` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::postBuild()` — `indra/newview/llfloaterdirector.cpp:354` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterEvent::handleMediaEvent()` — `indra/newview/llfloaterevent.cpp:90, 94` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterHelpBrowser::handleMediaEvent()` — `indra/newview/llfloaterhelpbrowser.cpp:103, 107` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterHexEditor::assetCallback()` — `indra/newview/llfloaterhexeditor.cpp:205, 230` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterHexEditor::imageCallback()` — `indra/newview/llfloaterhexeditor.cpp:166, 181` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterHexEditor::onClickSave()` — `indra/newview/llfloaterhexeditor.cpp:418` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterHexEditor::onOpen()` — `indra/newview/llfloaterhexeditor.cpp:86` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterHexEditor::onSaveComplete()` — `indra/newview/llfloaterhexeditor.cpp:436` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterLinkReplace::postBuild()` — `indra/newview/llfloaterlinkreplace.cpp:69` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterPrismManager::postBuild()` — `indra/newview/llfloaterprismmanager.cpp:333` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLHintPopup::postBuild()` — `indra/newview/llhints.cpp:185` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLPanelExperienceListEditor::postBuild()` — `indra/newview/llpanelexperiencelisteditor.cpp:61, 65` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLPanelProfileWeb::handleMediaEvent()` — `indra/newview/llpanelprofile.cpp:1960, 1980` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **48 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-props_enroll-tallest-850` | Only `props_enroll` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-props_enroll-tallest-1160` | Only `props_enroll` is put in its tallest local state at 1160 px; all other cards stay default. |
| `prop_list-empty` | `prop_list` is empty with no selection. |
| `prop_list-one` | `prop_list` has one selected item. |
| `prop_list-overflow` | `prop_list` has 51 items and a visible scrollbar. |
| `card-prop_path-tallest-850` | Only `prop_path` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-prop_path-tallest-1160` | Only `prop_path` is put in its tallest local state at 1160 px; all other cards stay default. |
| `node_list-empty` | `node_list` is empty with no selection. |
| `node_list-one` | `node_list` has one selected item. |
| `node_list-overflow` | `node_list` has 51 items and a visible scrollbar. |
| `card-drive-tallest-850` | Only `drive` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-drive-tallest-1160` | Only `drive` is put in its tallest local state at 1160 px; all other cards stay default. |
| `end_mode_combo-option-0` | Only `end_mode_combo` selects `Stop at end`; other selectors remain default. |
| `end_mode_combo-option-1` | Only `end_mode_combo` selects `Loop`; other selectors remain default. |
| `end_mode_combo-option-2` | Only `end_mode_combo` selects `Ping-pong`; other selectors remain default. |
| `card-orient-tallest-850` | Only `orient` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-orient-tallest-1160` | Only `orient` is put in its tallest local state at 1160 px; all other cards stay default. |
| `rotation_mode_combo-option-0` | Only `rotation_mode_combo` selects `Face path`; other selectors remain default. |
| `rotation_mode_combo-option-1` | Only `rotation_mode_combo` selects `Authored`; other selectors remain default. |
| `rotation_mode_combo-option-2` | Only `rotation_mode_combo` selects `Face path + offset`; other selectors remain default. |
| `card-hover-tallest-850` | Only `hover` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-hover-tallest-1160` | Only `hover` is put in its tallest local state at 1160 px; all other cards stay default. |
| `hover_wave_combo-option-0` | Only `hover_wave_combo` selects `Full`; other selectors remain default. |
| `hover_wave_combo-option-1` | Only `hover_wave_combo` selects `Half`; other selectors remain default. |
| `card-rock-tallest-850` | Only `rock` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-rock-tallest-1160` | Only `rock` is put in its tallest local state at 1160 px; all other cards stay default. |
| `rock_axis_combo-option-0` | Only `rock_axis_combo` selects `X`; other selectors remain default. |
| `rock_axis_combo-option-1` | Only `rock_axis_combo` selects `X+Y`; other selectors remain default. |
| `rock_axis_combo-option-2` | Only `rock_axis_combo` selects `X+Y+Z`; other selectors remain default. |
| `card-spin-tallest-850` | Only `spin` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-spin-tallest-1160` | Only `spin` is put in its tallest local state at 1160 px; all other cards stay default. |
| `spin_axis_combo-option-0` | Only `spin_axis_combo` selects `X`; other selectors remain default. |
| `spin_axis_combo-option-1` | Only `spin_axis_combo` selects `Y`; other selectors remain default. |
| `spin_axis_combo-option-2` | Only `spin_axis_combo` selects `Z`; other selectors remain default. |
| `card-shuttle-tallest-850` | Only `shuttle` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-shuttle-tallest-1160` | Only `shuttle` is put in its tallest local state at 1160 px; all other cards stay default. |
| `shuttle_axis_combo-option-0` | Only `shuttle_axis_combo` selects `X`; other selectors remain default. |
| `shuttle_axis_combo-option-1` | Only `shuttle_axis_combo` selects `Y`; other selectors remain default. |
| `shuttle_axis_combo-option-2` | Only `shuttle_axis_combo` selects `Z`; other selectors remain default. |
| `shuttle_axis_combo-option-3` | Only `shuttle_axis_combo` selects `XZ`; other selectors remain default. |
| `shuttle_sign_combo-option-0` | Only `shuttle_sign_combo` selects `+`; other selectors remain default. |
| `shuttle_sign_combo-option-1` | Only `shuttle_sign_combo` selects `-`; other selectors remain default. |
| `selection-deleted` | The selected model row is removed by another owner while visible. |
| `permission-denied` | The current target exists but cannot be modified. |
| `target-offline` | The current target/provider becomes unavailable. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## New widget required

None. Every approved visual kind is either a kit primitive or an existing XUI control. Missing action/model bindings below are USER DECISIONS, not permission to invent a visual class.

## Unresolved and USER DECISION items

None in the current evidence index.

## Risks

- A widget-name collision can select an unrelated XUI root or C++ literal; ownership must be resolved by root, class, and behavior together.
- Extracting model work without rewiring the existing owner creates divergent v1/v2 behavior; the extraction and forwarding call are one reviewed change.
- Duplicate writable settings must obey the rollout homes table; this page emits a link/read-only row when it is not the chosen home.
- Shared edit models require owner tokens, and external presets/cue playback require one global history barrier at start/stop rather than per write.
- Unresolved rows fail generation closed; they are never silently replaced with a guessed widget or command.
