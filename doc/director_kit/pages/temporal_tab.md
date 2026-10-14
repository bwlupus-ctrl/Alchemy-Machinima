# Time — Director Kit page contract

Page key: `temporal_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (8/18 setting-bound rows; 9 C++-referenced rows).

## Real owners

- `panel_temporal_capture` / `panel` at `indra/newview/skins/default/xui/en/panel_temporal_capture.xml:17` (18 matching rows).
- Referencing classes: `ALPanelTemporalCapture` (10).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALTemporalCaptureKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `world_clock` / `mode_combo` (Mode) | `combo_box` `indra/newview/skins/default/xui/en/panel_temporal_capture.xml:48` | kit `combo` ↔ `TemporalMode` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_capture_temporal.xml:9` `mode_combo` |
| `world_clock` / `scale_slider` (Speed) | `slider` `indra/newview/skins/default/xui/en/panel_temporal_capture.xml:78` | kit `slider` ↔ `TemporalWorldScale` `F32`; `ALTemporalCaptureKitAdapter` preserves/calls wired in `ALPanelTemporalCapture::postBuild()`; behavior `ALPanelTemporalCapture::onScaleCommit()` (`indra/newview/alpaneltemporalcapture.cpp:49`) | page `ghosts_tab`, `indra/newview/skins/default/xui/en/panel_dpage_capture_temporal.xml:17` `scale_slider`, `indra/newview/skins/default/xui/en/panel_temporal_capture.xml:94` `scale_spinner` |
| `world_clock` / `output_fps_spinner` (Out FPS) | `spinner` `indra/newview/skins/default/xui/en/panel_temporal_capture.xml:182` | kit `number` ↔ `TemporalOutputFPS` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_capture_temporal.xml:22` `output_fps_spinner` |
| `world_clock` / `status_text` (Status) | `text` `indra/newview/skins/default/xui/en/panel_temporal_capture.xml:259` | kit readout ← approved static copy; no runtime writer | none |
| `world_clock` / `effective_text` (Effective) | `text` `indra/newview/skins/default/xui/en/panel_temporal_capture.xml:154` | kit readout ← approved static copy; no runtime writer | none |
| `world_clock` / `postspeed_text` (Post speed) | `text` `indra/newview/skins/default/xui/en/panel_temporal_capture.xml:164` | kit readout ← approved static copy; no runtime writer | none |
| `world_clock` / `temporal_hint` (Note) | `text` `indra/newview/skins/default/xui/en/panel_temporal_capture.xml:271` | kit readout ← approved static copy; no runtime writer | none |
| `speed_presets` / `btn_freeze` (Freeze) | `button` `indra/newview/skins/default/xui/en/panel_temporal_capture.xml:106` | `ALTemporalCaptureKitAdapter` command/model binding → wired in `ALPanelTemporalCapture::postBuild()`; behavior `ALPanelTemporalCapture::onClickScalePreset()` (`indra/newview/alpaneltemporalcapture.cpp:33`) | none |
| `speed_presets` / `btn_quarter` (0.25x) | `button` `indra/newview/skins/default/xui/en/panel_temporal_capture.xml:116` | `ALTemporalCaptureKitAdapter` command/model binding → wired in `ALPanelTemporalCapture::postBuild()`; behavior `ALPanelTemporalCapture::onClickScalePreset()` (`indra/newview/alpaneltemporalcapture.cpp:35`) | none |
| `speed_presets` / `btn_half` (0.5x) | `button` `indra/newview/skins/default/xui/en/panel_temporal_capture.xml:125` | `ALTemporalCaptureKitAdapter` command/model binding → wired in `ALPanelTemporalCapture::postBuild()`; behavior `ALPanelTemporalCapture::onClickScalePreset()` (`indra/newview/alpaneltemporalcapture.cpp:37`) | none |
| `speed_presets` / `btn_realtime` (1x) | `button` `indra/newview/skins/default/xui/en/panel_temporal_capture.xml:134` | `ALTemporalCaptureKitAdapter` command/model binding → wired in `ALPanelTemporalCapture::postBuild()`; behavior `ALPanelTemporalCapture::onClickScalePreset()` (`indra/newview/alpaneltemporalcapture.cpp:39`) | none |
| `speed_presets` / `btn_double` (2x) | `button` `indra/newview/skins/default/xui/en/panel_temporal_capture.xml:144` | `ALTemporalCaptureKitAdapter` command/model binding → wired in `ALPanelTemporalCapture::postBuild()`; behavior `ALPanelTemporalCapture::onClickScalePreset()` (`indra/newview/alpaneltemporalcapture.cpp:41`) | none |
| `drive_on_clock` / `drives_lbl` (Applies to) | `text` `indra/newview/skins/default/xui/en/panel_temporal_capture.xml:192` | kit readout ← approved static copy; no runtime writer | none |
| `drive_on_clock` / `drive_anim_check` (Animation) | `check_box` `indra/newview/skins/default/xui/en/panel_temporal_capture.xml:204` | kit `toggle` ↔ `TemporalDriveAnimation` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_capture_temporal.xml:59` `drive_anim_check` |
| `drive_on_clock` / `drive_objects_check` (Objects) | `check_box` `indra/newview/skins/default/xui/en/panel_temporal_capture.xml:215` | kit `toggle` ↔ `TemporalDriveObjects` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_capture_temporal.xml:62` `drive_objects_check` |
| `drive_on_clock` / `drive_texanim_check` (Textures) | `check_box` `indra/newview/skins/default/xui/en/panel_temporal_capture.xml:226` | kit `toggle` ↔ `TemporalDriveTextureAnim` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_capture_temporal.xml:65` `drive_texanim_check` |
| `drive_on_clock` / `drive_particles_check` (Particles) | `check_box` `indra/newview/skins/default/xui/en/panel_temporal_capture.xml:237` | kit `toggle` ↔ `TemporalDriveParticles` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_capture_temporal.xml:68` `drive_particles_check` |
| `drive_on_clock` / `drive_camera_check` (Camera) | `check_box` `indra/newview/skins/default/xui/en/panel_temporal_capture.xml:249` | kit `toggle` ↔ `TemporalDriveCamera` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_capture_temporal.xml:71` `drive_camera_check` |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALPanelTemporalCapture::onClickScalePreset()` — `indra/newview/alpaneltemporalcapture.cpp:33, 35, 37, 39, 41` | Move model-facing work into `ALTemporalCaptureKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelTemporalCapture::onScaleCommit()` — `indra/newview/alpaneltemporalcapture.cpp:49` | Move model-facing work into `ALTemporalCaptureKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **16 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-world_clock-tallest-850` | Only `world_clock` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-world_clock-tallest-1160` | Only `world_clock` is put in its tallest local state at 1160 px; all other cards stay default. |
| `mode_combo-option-0` | Only `mode_combo` selects `Live (realtime)`; other selectors remain default. |
| `mode_combo-option-1` | Only `mode_combo` selects `Manual scale`; other selectors remain default. |
| `card-speed_presets-tallest-850` | Only `speed_presets` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-speed_presets-tallest-1160` | Only `speed_presets` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-drive_on_clock-tallest-850` | Only `drive_on_clock` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-drive_on_clock-tallest-1160` | Only `drive_on_clock` is put in its tallest local state at 1160 px; all other cards stay default. |
| `drive_anim_check-on` | Only `drive_anim_check` is on; dependent visibility is measured. |
| `drive_objects_check-on` | Only `drive_objects_check` is on; dependent visibility is measured. |
| `drive_texanim_check-on` | Only `drive_texanim_check` is on; dependent visibility is measured. |
| `drive_particles_check-on` | Only `drive_particles_check` is on; dependent visibility is measured. |
| `drive_camera_check-on` | Only `drive_camera_check` is on; dependent visibility is measured. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## Ambiguous same-name candidates (never auto-bound)

None.

## Rejected out-of-scope same-name candidates

50 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

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
