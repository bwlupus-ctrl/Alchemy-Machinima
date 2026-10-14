# Takes — Director Kit page contract

Page key: `takes_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (12/20 setting-bound rows; 12 C++-referenced rows).

## Real owners

- `panel_flycam_recorder` / `panel` at `indra/newview/skins/default/xui/en/panel_flycam_recorder.xml:17` (20 matching rows).
- Referencing classes: `ALPanelFlycamRecorder` (19), `LLViewerJoystick` (3).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the recorder's existing list, buttons, combos, sliders, and scrubber; do not create a timeline/curve editor.

## Row bindings

`ALTakesKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `flycam_flight` / `FlycamFlightModel` (Model) | `combo_box` `indra/newview/skins/default/xui/en/panel_flycam_recorder.xml:22` | kit `combo` ↔ `FlycamFlightModel` `S32` | `indra/newview/skins/default/xui/en/floater_joystick.xml:694` `FlycamFlightModel` |
| `flycam_flight` / `FlycamDronePreset` (Drone preset) | `combo_box` `indra/newview/skins/default/xui/en/panel_flycam_recorder.xml:31` | kit `combo` ↔ `FlycamDronePreset` `S32`; `ALTakesKitAdapter` preserves/calls wired in `ALPanelFlycamRecorder::postBuild()`; behavior `LLViewerJoystick::getInstance()` (`indra/newview/alpanelflycamrecorder.cpp:86`) | `indra/newview/skins/default/xui/en/floater_joystick.xml:725` `FlycamDronePreset` |
| `flycam_flight` / `FlycamDroneRelease` (On release) | `combo_box` `indra/newview/skins/default/xui/en/panel_flycam_recorder.xml:41` | kit `combo` ↔ `FlycamDroneRelease` `S32`; `ALTakesKitAdapter` preserves/calls wired in `ALPanelFlycamRecorder::postBuild()`; behavior `LLViewerJoystick::getInstance()` (`indra/newview/alpanelflycamrecorder.cpp:92`) | `indra/newview/skins/default/xui/en/floater_joystick.xml:735` `FlycamDroneRelease` |
| `flycam_flight` / `FlycamWeightedPreset` (Weight preset) | `combo_box` `indra/newview/skins/default/xui/en/panel_flycam_recorder.xml:47` | kit `combo` ↔ `FlycamWeightedPreset` `S32`; `ALTakesKitAdapter` preserves/calls wired in `ALPanelFlycamRecorder::postBuild()`; behavior `LLViewerJoystick::getInstance()` (`indra/newview/alpanelflycamrecorder.cpp:96`) | `indra/newview/skins/default/xui/en/floater_joystick.xml:705` `FlycamWeightedPreset` |
| `flycam_flight` / `FlycamWeightedWeight` (Weight) | `spinner` `indra/newview/skins/default/xui/en/panel_flycam_recorder.xml:57` | kit `number` ↔ `FlycamWeightedWeight` `F32` | `indra/newview/skins/default/xui/en/floater_joystick.xml:713` `FlycamWeightedWeight` |
| `record_play` / `btn_record` (Record) | `button` `indra/newview/skins/default/xui/en/panel_flycam_recorder.xml:67` | `ALTakesKitAdapter` command/model binding → wired in `ALPanelFlycamRecorder::postBuild()`; behavior `ALPanelFlycamRecorder::onRecord()` (`indra/newview/alpanelflycamrecorder.cpp:78`) | none |
| `record_play` / `btn_play` (Play) | `button` `indra/newview/skins/default/xui/en/panel_flycam_recorder.xml:77` | `ALTakesKitAdapter` command/model binding → wired in `ALPanelFlycamRecorder::postBuild()`; behavior `ALPanelFlycamRecorder::onPlayPause()` (`indra/newview/alpanelflycamrecorder.cpp:79`) | none |
| `record_play` / `btn_stop` (Stop) | `button` `indra/newview/skins/default/xui/en/panel_flycam_recorder.xml:87` | `ALTakesKitAdapter` command/model binding → wired in `ALPanelFlycamRecorder::postBuild()`; behavior `ALPanelFlycamRecorder::onStop()` (`indra/newview/alpanelflycamrecorder.cpp:80`) | none |
| `record_play` / `btn_clear` (Clear) | `button` `indra/newview/skins/default/xui/en/panel_flycam_recorder.xml:97` | `ALTakesKitAdapter` command/model binding → wired in `ALPanelFlycamRecorder::postBuild()`; behavior `ALPanelFlycamRecorder::onClear()` (`indra/newview/alpanelflycamrecorder.cpp:81`) | none |
| `record_play` / `scrub_slider` (Scrub) | `slider` `indra/newview/skins/default/xui/en/panel_flycam_recorder.xml:113` | `ALTakesKitAdapter` command/model binding → wired in `ALPanelFlycamRecorder::postBuild()`; behavior `ALPanelFlycamRecorder::onScrub()` (`indra/newview/alpanelflycamrecorder.cpp:84`) | none |
| `record_play` / `time_lbl` (Length) | `text` `indra/newview/skins/default/xui/en/panel_flycam_recorder.xml:124` | kit readout ← approved static copy; no runtime writer | none |
| `record_play` / `btn_save` (Save file) | `button` `indra/newview/skins/default/xui/en/panel_flycam_recorder.xml:253` | `ALTakesKitAdapter` command/model binding → wired in `ALPanelFlycamRecorder::postBuild()`; behavior `ALPanelFlycamRecorder::onSave()` (`indra/newview/alpanelflycamrecorder.cpp:82`) | none |
| `record_play` / `btn_load` (Load file) | `button` `indra/newview/skins/default/xui/en/panel_flycam_recorder.xml:263` | `ALTakesKitAdapter` command/model binding → wired in `ALPanelFlycamRecorder::postBuild()`; behavior `ALPanelFlycamRecorder::onLoad()` (`indra/newview/alpanelflycamrecorder.cpp:83`) | none |
| `record_play` / `FlycamRecSampleRate` (Sample rate) | `slider` `indra/newview/skins/default/xui/en/panel_flycam_recorder.xml:239` | kit `slider` ↔ `FlycamRecSampleRate` `F32` | none |
| `playback` / `FlycamRecSpeed` (Speed) | `slider` `indra/newview/skins/default/xui/en/panel_flycam_recorder.xml:156` | kit `slider` ↔ `FlycamRecSpeed` `F32` | none |
| `playback` / `FlycamRecLoopMode` (At the end) | `combo_box` `indra/newview/skins/default/xui/en/panel_flycam_recorder.xml:166` | kit `combo` ↔ `FlycamRecLoopMode` `S32` | none |
| `playback` / `FlycamRecSmooth` (Smooth splines) | `check_box` `indra/newview/skins/default/xui/en/panel_flycam_recorder.xml:181` | kit `toggle` ↔ `FlycamRecSmooth` `Boolean` | none |
| `playback_anchor` / `FlycamRecAnchorMode` (Anchor to) | `combo_box` `indra/newview/skins/default/xui/en/panel_flycam_recorder.xml:191` | kit `combo` ↔ `FlycamRecAnchorMode` `S32` | none |
| `playback_anchor` / `FlycamRecFollowAnchor` (Follow anchor) | `check_box` `indra/newview/skins/default/xui/en/panel_flycam_recorder.xml:207` | kit `toggle` ↔ `FlycamRecFollowAnchor` `Boolean` | none |
| `playback_anchor` / `FlycamRecUseOperator` (Handheld layer) | `check_box` `indra/newview/skins/default/xui/en/panel_flycam_recorder.xml:218` | kit `toggle` ↔ `FlycamRecUseOperator` `Boolean` | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALPanelFlycamRecorder::onClear()` — `indra/newview/alpanelflycamrecorder.cpp:81` | Move model-facing work into `ALTakesKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelFlycamRecorder::onLoad()` — `indra/newview/alpanelflycamrecorder.cpp:83` | Move model-facing work into `ALTakesKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelFlycamRecorder::onPlayPause()` — `indra/newview/alpanelflycamrecorder.cpp:79` | Move model-facing work into `ALTakesKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelFlycamRecorder::onRecord()` — `indra/newview/alpanelflycamrecorder.cpp:78` | Move model-facing work into `ALTakesKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelFlycamRecorder::onSave()` — `indra/newview/alpanelflycamrecorder.cpp:82` | Move model-facing work into `ALTakesKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelFlycamRecorder::onScrub()` — `indra/newview/alpanelflycamrecorder.cpp:84` | Move model-facing work into `ALTakesKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelFlycamRecorder::onStop()` — `indra/newview/alpanelflycamrecorder.cpp:80` | Move model-facing work into `ALTakesKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLViewerJoystick::getInstance()` — `indra/newview/alpanelflycamrecorder.cpp:86, 92, 96` | Move model-facing work into `ALTakesKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **38 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-flycam_flight-tallest-850` | Only `flycam_flight` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-flycam_flight-tallest-1160` | Only `flycam_flight` is put in its tallest local state at 1160 px; all other cards stay default. |
| `FlycamFlightModel-option-0` | Only `FlycamFlightModel` selects `Classic`; other selectors remain default. |
| `FlycamFlightModel-option-1` | Only `FlycamFlightModel` selects `Weighted`; other selectors remain default. |
| `FlycamFlightModel-option-2` | Only `FlycamFlightModel` selects `Drone`; other selectors remain default. |
| `FlycamDronePreset-option-0` | Only `FlycamDronePreset` selects `Cine`; other selectors remain default. |
| `FlycamDronePreset-option-1` | Only `FlycamDronePreset` selects `Normal`; other selectors remain default. |
| `FlycamDronePreset-option-2` | Only `FlycamDronePreset` selects `Sport`; other selectors remain default. |
| `FlycamDronePreset-option-3` | Only `FlycamDronePreset` selects `Dolly`; other selectors remain default. |
| `FlycamDronePreset-option-4` | Only `FlycamDronePreset` selects `FPV`; other selectors remain default. |
| `FlycamDronePreset-option-5` | Only `FlycamDronePreset` selects `Custom`; other selectors remain default. |
| `FlycamDroneRelease-option-0` | Only `FlycamDroneRelease` selects `Hold`; other selectors remain default. |
| `FlycamDroneRelease-option-1` | Only `FlycamDroneRelease` selects `Drift`; other selectors remain default. |
| `FlycamWeightedPreset-option-0` | Only `FlycamWeightedPreset` selects `Light`; other selectors remain default. |
| `FlycamWeightedPreset-option-1` | Only `FlycamWeightedPreset` selects `Steadicam`; other selectors remain default. |
| `FlycamWeightedPreset-option-2` | Only `FlycamWeightedPreset` selects `Heavy`; other selectors remain default. |
| `FlycamWeightedPreset-option-3` | Only `FlycamWeightedPreset` selects `Crane`; other selectors remain default. |
| `FlycamWeightedPreset-option-4` | Only `FlycamWeightedPreset` selects `Custom`; other selectors remain default. |
| `card-record_play-tallest-850` | Only `record_play` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-record_play-tallest-1160` | Only `record_play` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-playback-tallest-850` | Only `playback` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-playback-tallest-1160` | Only `playback` is put in its tallest local state at 1160 px; all other cards stay default. |
| `FlycamRecLoopMode-option-0` | Only `FlycamRecLoopMode` selects `Hold last frame`; other selectors remain default. |
| `FlycamRecLoopMode-option-1` | Only `FlycamRecLoopMode` selects `Loop`; other selectors remain default. |
| `FlycamRecLoopMode-option-2` | Only `FlycamRecLoopMode` selects `Ping-pong`; other selectors remain default. |
| `FlycamRecSmooth-on` | Only `FlycamRecSmooth` is on; dependent visibility is measured. |
| `card-playback_anchor-tallest-850` | Only `playback_anchor` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-playback_anchor-tallest-1160` | Only `playback_anchor` is put in its tallest local state at 1160 px; all other cards stay default. |
| `FlycamRecAnchorMode-option-0` | Only `FlycamRecAnchorMode` selects `World (as recorded)`; other selectors remain default. |
| `FlycamRecAnchorMode-option-1` | Only `FlycamRecAnchorMode` selects `My avatar`; other selectors remain default. |
| `FlycamRecAnchorMode-option-2` | Only `FlycamRecAnchorMode` selects `Selected object/avatar`; other selectors remain default. |
| `FlycamRecAnchorMode-option-3` | Only `FlycamRecAnchorMode` selects `Camera at play start`; other selectors remain default. |
| `FlycamRecFollowAnchor-on` | Only `FlycamRecFollowAnchor` is on; dependent visibility is measured. |
| `FlycamRecUseOperator-on` | Only `FlycamRecUseOperator` is on; dependent visibility is measured. |
| `selection-deleted` | The selected model row is removed by another owner while visible. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## Ambiguous same-name candidates (never auto-bound)

None.

## Rejected out-of-scope same-name candidates

34 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

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
