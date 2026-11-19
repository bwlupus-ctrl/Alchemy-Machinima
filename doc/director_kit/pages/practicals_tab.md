# Practicals — Director Kit page contract

Page key: `practicals_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (0/46 setting-bound rows; 45 C++-referenced rows).

## Real owners

- `floater_lightbox_settings` / `floater` at `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:8` (15 matching rows).
- Referencing classes: `ALFloaterLightBox` (39), `ALVirtualLightsUI` (23), `ALProjectorTracker` (7), `ALPanelPracticals` (6), `ALVirtualLights` (1).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALPracticalsKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `sel` / `sl_status` (Status) | `text` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6432` | kit readout ← approved static copy; no runtime writer | none |
| `sel` / `sl_cast_shadows` (Cast shadows) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6434` | `ALPracticalsKitAdapter` command/model binding → wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onSelLightCastShadows()` (`indra/newview/alfloaterlightbox.cpp:90`) | none |
| `sel` / `sl_volumetric` (Light cone) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6438` | `ALPracticalsKitAdapter` command/model binding → wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onSelLightVolumetric()` (`indra/newview/alfloaterlightbox.cpp:91`) | none |
| `sel` / `sl_capture` (Capture shaft) | `button` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6441` | `ALPracticalsKitAdapter` command/model binding → wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onSelLightCaptureOverride()` (`indra/newview/alfloaterlightbox.cpp:92`) | none |
| `sel` / `sl_clear` (Clear) | `button` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6444` | `ALPracticalsKitAdapter` command/model binding → wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onSelLightClearOverride()` (`indra/newview/alfloaterlightbox.cpp:93`) | none |
| `track` / `sl_track` (Track) | none | `ALPracticalsKitAdapter` command/model binding → wired in `ALPanelPracticals::postBuild()`; behavior `ALVirtualLights::enableTrack()` (`indra/newview/alpanelpracticals.cpp:77`), wired in `ALPanelPracticals::postBuild()`; behavior `ALProjectorTracker::enable()` (`indra/newview/alprojectortracker.cpp:313`) | none |
| `track` / `sl_track_subject` (Subject) | none | `ALPracticalsKitAdapter` command/model binding → wired in `ALPanelPracticals::postBuild()`; behavior `ALProjectorTracker::configure()` (`indra/newview/alprojectortracker.cpp:191`) | none |
| `track` / `sl_track_joint` (Joint) | none | `ALPracticalsKitAdapter` command/model binding → wired in `ALPanelPracticals::postBuild()`; behavior `ALProjectorTracker::configure()` (`indra/newview/alprojectortracker.cpp:191`) | none |
| `track` / `sl_track_mode` (Mode) | none | `ALPracticalsKitAdapter` command/model binding → wired in `ALPanelPracticals::postBuild()`; behavior `ALProjectorTracker::configure()` (`indra/newview/alprojectortracker.cpp:191`) | none |
| `track` / `sl_track_smoothing` (Smoothing) | none | `ALPracticalsKitAdapter` command/model binding → wired in `ALPanelPracticals::postBuild()`; behavior `ALProjectorTracker::configure()` (`indra/newview/alprojectortracker.cpp:191`) | none |
| `track` / `sl_track_capture` (Offset) | none | `ALPracticalsKitAdapter` command/model binding → wired in `ALPanelPracticals::postBuild()`; behavior `ALProjectorTracker::capture()` (`indra/newview/alprojectortracker.cpp:339`) | none |
| `track` / `sl_track_status` (Track status) | none | `ALPracticalsKitAdapter` command/model binding → wired in `ALPanelPracticals::postBuild()`; behavior `ALProjectorTracker::state()` (`indra/newview/alprojectortracker.cpp:115`) | none |
| `gobo` / `sl_gobo` (Pattern) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6449` | `ALPracticalsKitAdapter` command/model binding → wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onSelGoboChanged()` (`indra/newview/alfloaterlightbox.cpp:94`) | none |
| `gobo` / `sl_gobo_animmode` (Animation) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6467` | `ALPracticalsKitAdapter` command/model binding → wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onSelGoboChanged()` (`indra/newview/alfloaterlightbox.cpp:95`) | none |
| `gobo` / `sl_gobo_speed` (Speed) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6478` | `ALPracticalsKitAdapter` command/model binding → wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onSelGoboChanged()` (`indra/newview/alfloaterlightbox.cpp:96`) | none |
| `gobo` / `sl_gobo_zoom` (Zoom) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6482` | `ALPracticalsKitAdapter` command/model binding → wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onSelGoboChanged()` (`indra/newview/alfloaterlightbox.cpp:97`) | none |
| `gobo` / `sl_gobo_tint` (Gel tint) | `color_swatch` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6490` | `ALPracticalsKitAdapter` command/model binding → wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onSelGoboChanged()` (`indra/newview/alfloaterlightbox.cpp:99`) | none |
| `gobo` / `sl_gobo_soft` (Softness) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6502` | `ALPracticalsKitAdapter` command/model binding → wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onSelGoboChanged()` (`indra/newview/alfloaterlightbox.cpp:102`) | none |
| `adv_practicals` / `sl_gobo_dispersion` (RGB dispersion) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6486` | `ALPracticalsKitAdapter` command/model binding → wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onSelGoboChanged()` (`indra/newview/alfloaterlightbox.cpp:98`) | none |
| `adv_practicals` / `sl_gobo_var1` (Detail) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6494` | `ALPracticalsKitAdapter` command/model binding → wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onSelGoboChanged()` (`indra/newview/alfloaterlightbox.cpp:100`) | none |
| `adv_practicals` / `sl_gobo_var2` (Thickness) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6498` | `ALPracticalsKitAdapter` command/model binding → wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onSelGoboChanged()` (`indra/newview/alfloaterlightbox.cpp:101`) | none |
| `adv_practicals` / `sl_gobo_invert` (Invert) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6505` | `ALPracticalsKitAdapter` command/model binding → wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onSelGoboChanged()` (`indra/newview/alfloaterlightbox.cpp:103`) | none |
| `adv_practicals` / `open_lightbox_sellight` (Lightbox ›) | none | kit-native action `floater:lightbox/sellight_settings`; no duplicate widget is required | none |
| `virtual` / `vl_status` (Status) | none | kit readout ← approved static copy; no runtime writer | none |
| `virtual` / `vl_place` (Place light) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `virtual` / `vl_edit` (Edit in world) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `virtual` / `vl_list` (Lights) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `virtual` / `vl_delete` (Delete) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `virtual` / `vl_on` (On) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `virtual` / `vl_type` (Type) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `virtual` / `vl_color` (Colour) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `virtual` / `vl_intensity` (Intensity) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `virtual` / `vl_radius` (Radius) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `virtual` / `vl_falloff` (Falloff) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `virtual` / `vl_fov` (Field of view) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `virtual` / `vl_focus` (Focus) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `virtual` / `vl_ambiance` (Ambiance) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `virtual` / `vl_texture` (Projector texture) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `virtual` / `vl_texture_name` (Texture) | none | kit readout ← approved static copy; no runtime writer | none |
| `virtual` / `vl_x` (X) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `virtual` / `vl_y` (Y) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `virtual` / `vl_z` (Z) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `virtual` / `vl_yaw` (Yaw) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `virtual` / `vl_pitch` (Pitch) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `virtual` / `vl_aim_pick` (Aim at) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `virtual` / `vl_vcam` (VCam feed) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALFloaterLightBox::onSelGoboChanged()` — `indra/newview/alfloaterlightbox.cpp:94, 95, 96, 97, 98, 99, 100, 101, 102, 103` | Move model-facing work into `ALPracticalsKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterLightBox::onSelLightCaptureOverride()` — `indra/newview/alfloaterlightbox.cpp:92` | Move model-facing work into `ALPracticalsKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterLightBox::onSelLightCastShadows()` — `indra/newview/alfloaterlightbox.cpp:90` | Move model-facing work into `ALPracticalsKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterLightBox::onSelLightClearOverride()` — `indra/newview/alfloaterlightbox.cpp:93` | Move model-facing work into `ALPracticalsKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterLightBox::onSelLightVolumetric()` — `indra/newview/alfloaterlightbox.cpp:91` | Move model-facing work into `ALPracticalsKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALVirtualLights::enableTrack()` — `indra/newview/alpanelpracticals.cpp:77` | Move model-facing work into `ALPracticalsKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALProjectorTracker::capture()` — `indra/newview/alprojectortracker.cpp:339` | Move model-facing work into `ALPracticalsKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALProjectorTracker::configure()` — `indra/newview/alprojectortracker.cpp:191` | Move model-facing work into `ALPracticalsKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALProjectorTracker::enable()` — `indra/newview/alprojectortracker.cpp:313` | Move model-facing work into `ALPracticalsKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALProjectorTracker::state()` — `indra/newview/alprojectortracker.cpp:115` | Move model-facing work into `ALPracticalsKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **57 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-sel-tallest-850` | Only `sel` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-sel-tallest-1160` | Only `sel` is put in its tallest local state at 1160 px; all other cards stay default. |
| `sl_cast_shadows-on` | Only `sl_cast_shadows` is on; dependent visibility is measured. |
| `sl_volumetric-on` | Only `sl_volumetric` is on; dependent visibility is measured. |
| `card-track-tallest-850` | Only `track` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-track-tallest-1160` | Only `track` is put in its tallest local state at 1160 px; all other cards stay default. |
| `sl_track-on` | Only `sl_track` is on; dependent visibility is measured. |
| `sl_track_joint-option-0` | Only `sl_track_joint` selects `Head`; other selectors remain default. |
| `sl_track_joint-option-1` | Only `sl_track_joint` selects `Chest`; other selectors remain default. |
| `sl_track_joint-option-2` | Only `sl_track_joint` selects `Hips`; other selectors remain default. |
| `sl_track_joint-option-3` | Only `sl_track_joint` selects `Left hand`; other selectors remain default. |
| `sl_track_joint-option-4` | Only `sl_track_joint` selects `Right hand`; other selectors remain default. |
| `sl_track_joint-option-5` | Only `sl_track_joint` selects `Left foot`; other selectors remain default. |
| `sl_track_joint-option-6` | Only `sl_track_joint` selects `Right foot`; other selectors remain default. |
| `sl_track_mode-option-0` | Only `sl_track_mode` selects `Aim`; other selectors remain default. |
| `sl_track_mode-option-1` | Only `sl_track_mode` selects `Follow`; other selectors remain default. |
| `card-gobo-tallest-850` | Only `gobo` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-gobo-tallest-1160` | Only `gobo` is put in its tallest local state at 1160 px; all other cards stay default. |
| `sl_gobo-option-0` | Only `sl_gobo` selects `Off (texture)`; other selectors remain default. |
| `sl_gobo-option-1` | Only `sl_gobo` selects `Venetian blinds`; other selectors remain default. |
| `sl_gobo-option-2` | Only `sl_gobo` selects `Window`; other selectors remain default. |
| `sl_gobo-option-3` | Only `sl_gobo` selects `Grate`; other selectors remain default. |
| `sl_gobo-option-4` | Only `sl_gobo` selects `Ceiling fan`; other selectors remain default. |
| `sl_gobo-option-5` | Only `sl_gobo` selects `Honeycomb / softbox`; other selectors remain default. |
| `sl_gobo-option-6` | Only `sl_gobo` selects `Foliage / dapple`; other selectors remain default. |
| `sl_gobo-option-7` | Only `sl_gobo` selects `Water caustics`; other selectors remain default. |
| `sl_gobo-option-8` | Only `sl_gobo` selects `Prison bars`; other selectors remain default. |
| `sl_gobo-option-9` | Only `sl_gobo` selects `Arched window`; other selectors remain default. |
| `sl_gobo-option-10` | Only `sl_gobo` selects `Iris / vignette`; other selectors remain default. |
| `sl_gobo-option-11` | Only `sl_gobo` selects `Dot grid`; other selectors remain default. |
| `sl_gobo-option-12` | Only `sl_gobo` selects `Rings / starburst`; other selectors remain default. |
| `sl_gobo-option-13` | Only `sl_gobo` selects `Clouds`; other selectors remain default. |
| `sl_gobo_animmode-option-0` | Only `sl_gobo_animmode` selects `None`; other selectors remain default. |
| `sl_gobo_animmode-option-1` | Only `sl_gobo_animmode` selects `Rotate`; other selectors remain default. |
| `sl_gobo_animmode-option-2` | Only `sl_gobo_animmode` selects `Wind sway`; other selectors remain default. |
| `sl_gobo_animmode-option-3` | Only `sl_gobo_animmode` selects `Pan / scroll`; other selectors remain default. |
| `sl_gobo_animmode-option-4` | Only `sl_gobo_animmode` selects `Pulse / breathe`; other selectors remain default. |
| `sl_gobo_animmode-option-5` | Only `sl_gobo_animmode` selects `Flicker`; other selectors remain default. |
| `sl_gobo_animmode-option-6` | Only `sl_gobo_animmode` selects `Strobe`; other selectors remain default. |
| `card-adv_practicals-tallest-850` | Only `adv_practicals` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-adv_practicals-tallest-1160` | Only `adv_practicals` is put in its tallest local state at 1160 px; all other cards stay default. |
| `sl_gobo_invert-on` | Only `sl_gobo_invert` is on; dependent visibility is measured. |
| `card-virtual-tallest-850` | Only `virtual` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-virtual-tallest-1160` | Only `virtual` is put in its tallest local state at 1160 px; all other cards stay default. |
| `vl_edit-on` | Only `vl_edit` is on; dependent visibility is measured. |
| `vl_list-empty` | `vl_list` is empty with no selection. |
| `vl_list-one` | `vl_list` has one selected item. |
| `vl_list-overflow` | `vl_list` has 51 items and a visible scrollbar. |
| `vl_on-on` | Only `vl_on` is on; dependent visibility is measured. |
| `vl_type-option-0` | Only `vl_type` selects `Point`; other selectors remain default. |
| `vl_type-option-1` | Only `vl_type` selects `Spot`; other selectors remain default. |
| `vl_vcam-option-0` | Only `vl_vcam` selects `Off`; other selectors remain default. |
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

80 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

## New widget required

None. Every approved visual kind is either a kit primitive or an existing XUI control. An unresolved binding is not permission to invent a visual class.

## Unresolved and pending-decision items

- `vl_place` (Place light): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `vl_edit` (Edit in world): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `vl_list` (Lights): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `vl_delete` (Delete): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `vl_on` (On): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `vl_type` (Type): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `vl_color` (Colour): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `vl_intensity` (Intensity): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `vl_radius` (Radius): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `vl_falloff` (Falloff): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `vl_fov` (Field of view): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `vl_focus` (Focus): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `vl_ambiance` (Ambiance): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `vl_texture` (Projector texture): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `vl_x` (X): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `vl_y` (Y): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `vl_z` (Z): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `vl_yaw` (Yaw): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `vl_pitch` (Pitch): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `vl_aim_pick` (Aim at): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `vl_vcam` (VCam feed): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.

## Risks

- A widget-name collision can select an unrelated XUI root or C++ literal; ownership must be resolved by root, class, and behavior together.
- Extracting model work without rewiring the existing owner creates divergent v1/v2 behavior; the extraction and forwarding call are one reviewed change.
- Duplicate writable settings must obey the rollout homes table; this page emits a link/read-only row when it is not the chosen home.
- Shared edit models require owner tokens, and external presets/cue playback require one global history barrier at start/stop rather than per write.
- Unresolved rows fail generation closed; they are never silently replaced with a guessed widget or command.
