# Props — Director Kit page contract

Page key: `props_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (1/33 setting-bound rows; 32 C++-referenced rows).

## Real owners

- `panel_prop_mover` / `panel` at `indra/newview/skins/default/xui/en/panel_prop_mover.xml:12` (33 matching rows).
- Referencing classes: `ALPanelPropMover` (53).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALPropMoverKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `props_enroll` / `hint_text` (Tip) | `text` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:21` | kit readout ← approved static copy; no runtime writer | none |
| `props_enroll` / `btn_enroll` (Enroll object) | `button` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:36` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onEnrollSelected()` (`indra/newview/alpanelpropmover.cpp:89`) | none |
| `props_enroll` / `prop_list` (Props) | `scroll_list` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:58` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onSelectProp()` (`indra/newview/alpanelpropmover.cpp:86`) | none |
| `props_enroll` / `btn_remove` (Remove) | `button` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:46` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onRemoveProp()` (`indra/newview/alpanelpropmover.cpp:90`) | none |
| `props_enroll` / `status_text` (Status) | `text` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:605` | kit readout ← approved static copy; no runtime writer | none |
| `prop_path` / `node_list` (Nodes) | `scroll_list` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:115` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onSelectNode()` (`indra/newview/alpanelpropmover.cpp:87`) | none |
| `prop_path` / `btn_node_add` (Add node) | `button` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:83` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onAddNode()` (`indra/newview/alpanelpropmover.cpp:91`) | none |
| `prop_path` / `btn_node_del` (Delete node) | `button` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:93` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onDeleteNode()` (`indra/newview/alpanelpropmover.cpp:92`) | none |
| `prop_path` / `btn_node_clear` (Clear path) | `button` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:103` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onClearNodes()` (`indra/newview/alpanelpropmover.cpp:93`) | none |
| `prop_path` / `node_skid_spin` (Skid (deg)) | `spinner` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:145` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onCommitNodeSkid()` (`indra/newview/alpanelpropmover.cpp:101`) | none |
| `prop_path` / `node_speed_spin` (Node m/s) | `spinner` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:160` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onCommitNodeSpeed()` (`indra/newview/alpanelpropmover.cpp:102`) | none |
| `drive` / `btn_drive` (Drive) | `button` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:212` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onDrive()` (`indra/newview/alpanelpropmover.cpp:94`) | none |
| `drive` / `btn_stop` (Stop) | `button` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:222` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onStop()` (`indra/newview/alpanelpropmover.cpp:95`) | none |
| `drive` / `btn_drive_all` (Drive all) | `button` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:232` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onDriveAll()` (`indra/newview/alpanelpropmover.cpp:96`) | none |
| `drive` / `btn_stop_all` (Stop all) | `button` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:242` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onStopAll()` (`indra/newview/alpanelpropmover.cpp:97`) | none |
| `drive` / `path_speed_spin` (Path m/s) | `spinner` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:177` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onCommitPathSpeed()` (`indra/newview/alpanelpropmover.cpp:99`) | none |
| `drive` / `end_mode_combo` (At last node) | `combo_box` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:186` | kit `combo` ↔ `ActorMoverEndMode` `S32`; `ALPropMoverKitAdapter` preserves/calls wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onCommitEndMode()` (`indra/newview/alpanelpropmover.cpp:100`). Contract note: page contract supplies omitted setting `ActorMoverEndMode` | page `move_tab`, `indra/newview/skins/default/xui/en/panel_actor_mover.xml:236` `end_mode_combo` |
| `orient` / `btn_capture_orient` (Capture Orient) | `button` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:268` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onCaptureOrient()` (`indra/newview/alpanelpropmover.cpp:104`) | none |
| `orient` / `rotation_mode_combo` (Orient mode) | `combo_box` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:277` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onCommitRotationMode()` (`indra/newview/alpanelpropmover.cpp:105`) | none |
| `hover` / `hover_wave_combo` (Shape) | `combo_box` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:322` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onCommitEffects()` (`indra/newview/alpanelpropmover.cpp:108`) | none |
| `hover` / `hover_amp_spin` (Amp (m)) | `spinner` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:346` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onCommitEffects()` (`indra/newview/alpanelpropmover.cpp:109`) | none |
| `hover` / `hover_freq_spin` (Speed (Hz)) | `spinner` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:361` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onCommitEffects()` (`indra/newview/alpanelpropmover.cpp:110`) | none |
| `rock` / `rock_axis_combo` (Axes) | `combo_box` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:380` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onCommitEffects()` (`indra/newview/alpanelpropmover.cpp:112`) | none |
| `rock` / `rock_amp_spin` (Amp (deg)) | `spinner` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:408` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onCommitEffects()` (`indra/newview/alpanelpropmover.cpp:113`) | none |
| `rock` / `rock_freq_spin` (Speed (Hz)) | `spinner` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:423` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onCommitEffects()` (`indra/newview/alpanelpropmover.cpp:114`) | none |
| `spin` / `spin_axis_combo` (Axis) | `combo_box` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:455` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onCommitEffects()` (`indra/newview/alpanelpropmover.cpp:116`) | none |
| `spin` / `spin_speed_spin` (Deg/s) | `spinner` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:483` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onCommitEffects()` (`indra/newview/alpanelpropmover.cpp:117`) | none |
| `shuttle` / `btn_shuttle` (Shuttle) | `button` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:595` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onClickShuttle()` (`indra/newview/alpanelpropmover.cpp:120`) | none |
| `shuttle` / `shuttle_axis_combo` (Axis) | `combo_box` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:505` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onCommitShuttleAxis()` (`indra/newview/alpanelpropmover.cpp:119`) | none |
| `shuttle` / `shuttle_sign_combo` (Direction) | `combo_box` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:531` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onClickShuttle()` (`indra/newview/alpanelpropmover.cpp:593`) | none |
| `shuttle` / `shuttle_distance_spin` (Distance (m)) | `spinner` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:570` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onClickShuttle()` (`indra/newview/alpanelpropmover.cpp:593`) | none |
| `shuttle` / `shuttle_speed_spin` (Speed (m/s)) | `spinner` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:585` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onClickShuttle()` (`indra/newview/alpanelpropmover.cpp:593`) | none |
| `shuttle` / `shuttle_slope_spin` (Slope (deg)) | `spinner` `indra/newview/skins/default/xui/en/panel_prop_mover.xml:555` | `ALPropMoverKitAdapter` command/model binding → wired in `ALPanelPropMover::postBuild()`; behavior `ALPanelPropMover::onClickShuttle()` (`indra/newview/alpanelpropmover.cpp:593`) | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALPanelPropMover::onAddNode()` — `indra/newview/alpanelpropmover.cpp:91` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPropMover::onCaptureOrient()` — `indra/newview/alpanelpropmover.cpp:104` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPropMover::onClearNodes()` — `indra/newview/alpanelpropmover.cpp:93` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPropMover::onClickShuttle()` — `indra/newview/alpanelpropmover.cpp:120, 593` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPropMover::onCommitEffects()` — `indra/newview/alpanelpropmover.cpp:108, 109, 110, 112, 113, 114, 116, 117` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPropMover::onCommitEndMode()` — `indra/newview/alpanelpropmover.cpp:100` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPropMover::onCommitNodeSkid()` — `indra/newview/alpanelpropmover.cpp:101` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPropMover::onCommitNodeSpeed()` — `indra/newview/alpanelpropmover.cpp:102` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPropMover::onCommitPathSpeed()` — `indra/newview/alpanelpropmover.cpp:99` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPropMover::onCommitRotationMode()` — `indra/newview/alpanelpropmover.cpp:105` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPropMover::onCommitShuttleAxis()` — `indra/newview/alpanelpropmover.cpp:119` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPropMover::onDeleteNode()` — `indra/newview/alpanelpropmover.cpp:92` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPropMover::onDrive()` — `indra/newview/alpanelpropmover.cpp:94` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPropMover::onDriveAll()` — `indra/newview/alpanelpropmover.cpp:96` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPropMover::onEnrollSelected()` — `indra/newview/alpanelpropmover.cpp:89` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPropMover::onRemoveProp()` — `indra/newview/alpanelpropmover.cpp:90` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPropMover::onSelectNode()` — `indra/newview/alpanelpropmover.cpp:87` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPropMover::onSelectProp()` — `indra/newview/alpanelpropmover.cpp:86` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPropMover::onStop()` — `indra/newview/alpanelpropmover.cpp:95` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPropMover::onStopAll()` — `indra/newview/alpanelpropmover.cpp:97` | Move model-facing work into `ALPropMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

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

## Ambiguous same-name candidates (never auto-bound)

None.

## Rejected out-of-scope same-name candidates

59 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

## New widget required

None. Every approved visual kind is either a kit primitive or an existing XUI control. An unresolved binding is not permission to invent a visual class.

## Unresolved and pending-decision items

None in the current evidence index.

## Risks

- A widget-name collision can select an unrelated XUI root or C++ literal; ownership must be resolved by root, class, and behavior together.
- Extracting model work without rewiring the existing owner creates divergent v1/v2 behavior; the extraction and forwarding call are one reviewed change.
- Duplicate writable settings must obey the rollout homes table; this page emits a link/read-only row when it is not the chosen home.
- Shared edit models require owner tokens, and external presets/cue playback require one global history barrier at start/stop rather than per write.
- Unresolved rows fail generation closed; they are never silently replaced with a guessed widget or command.
