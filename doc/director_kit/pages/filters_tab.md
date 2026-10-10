# Filters — Director Kit page contract

Page key: `filters_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (26/29 setting-bound rows; 3 C++-referenced rows).

## Real owners

- `panel_lens_looks` / `panel` at `indra/newview/skins/default/xui/en/panel_lens_looks.xml:2` (27 matching rows).
- Referencing classes: `ALLensLooks` (2), `ALPanelLensLooks` (1), `LLNotificationsUtil` (1).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALLensFilterKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `gradnd` / `gradnd_preset_combo` (Preset) | `combo_box` `indra/newview/skins/default/xui/en/panel_lens_looks.xml:26` | kit `combo` ↔ `RenderGradNDPreset` `U32` | `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:78` `exp_gnd_preset` |
| `gradnd` / `gradnd_density_slider` (Strength) | `slider_bar` `indra/newview/skins/default/xui/en/panel_lens_looks.xml:38` | kit `slider` ↔ `RenderGradNDDensity` `F32` | `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:87` `exp_gnd_density`, `indra/newview/skins/default/xui/en/panel_lens_looks.xml:40` `gradnd_density_spinner` |
| `gradnd` / `gradnd_pos_slider` (Position) | `slider_bar` `indra/newview/skins/default/xui/en/panel_lens_looks.xml:62` | kit `slider` ↔ `RenderGradNDPosition` `F32` | `indra/newview/skins/default/xui/en/panel_lens_looks.xml:64` `gradnd_pos_spinner` |
| `gradnd` / `gradnd_soft_slider` (Softness) | `slider_bar` `indra/newview/skins/default/xui/en/panel_lens_looks.xml:68` | kit `slider` ↔ `RenderGradNDSoftness` `F32` | `indra/newview/skins/default/xui/en/panel_lens_looks.xml:70` `gradnd_soft_spinner` |
| `gradnd` / `gradnd_angle_slider` (Angle) | `slider_bar` `indra/newview/skins/default/xui/en/panel_lens_looks.xml:74` | kit `slider` ↔ `RenderGradNDAngle` `F32` | `indra/newview/skins/default/xui/en/panel_lens_looks.xml:76` `gradnd_angle_spinner` |
| `gradnd` / `gradnd_confine_slider` (Sky only) | `slider_bar` `indra/newview/skins/default/xui/en/panel_lens_looks.xml:44` | kit `slider` ↔ `RenderGradNDSkyConfine` `F32` | `indra/newview/skins/default/xui/en/panel_lens_looks.xml:46` `gradnd_confine_spinner` |
| `gradnd` / `gradnd_sunw_slider` (Sun weight) | `slider_bar` `indra/newview/skins/default/xui/en/panel_lens_looks.xml:50` | kit `slider` ↔ `RenderGradNDSunWeight` `F32` | `indra/newview/skins/default/xui/en/panel_lens_looks.xml:52` `gradnd_sunw_spinner` |
| `gradnd` / `gradnd_sunr_slider` (Sun radius) | `slider_bar` `indra/newview/skins/default/xui/en/panel_lens_looks.xml:56` | kit `slider` ↔ `RenderGradNDSunRadius` `F32` | `indra/newview/skins/default/xui/en/panel_lens_looks.xml:58` `gradnd_sunr_spinner` |
| `gradnd` / `check_box` (Darken bottom) | none | kit `toggle` ↔ `RenderGradNDFlip` `Boolean`. Contract note: generic data name resolved by control_name `RenderGradNDFlip` | none |
| `gradnd` / `gradnd_tint_swatch` (Tint) | `color_swatch` `indra/newview/skins/default/xui/en/panel_lens_looks.xml:82` | kit `color` ↔ `RenderGradNDTintColor` `Color3` | none |
| `gradnd` / `gradnd_tint_amount` (Tint amount) | `slider` `indra/newview/skins/default/xui/en/panel_lens_looks.xml:83` | kit `slider` ↔ `RenderGradNDTintAmount` `F32` | none |
| `polarizer` / `polarizer_preset_combo` (Preset) | `combo_box` `indra/newview/skins/default/xui/en/panel_lens_looks.xml:101` | kit `combo` ↔ `RenderPolarizerPreset` `U32` | `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:92` `exp_pol_preset` |
| `polarizer` / `pol_strength_slider` (Strength) | `slider_bar` `indra/newview/skins/default/xui/en/panel_lens_looks.xml:111` | kit `slider` ↔ `RenderPolarizerStrength` `F32` | `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:99` `exp_pol_strength`, `indra/newview/skins/default/xui/en/panel_lens_looks.xml:113` `pol_strength_spinner` |
| `polarizer` / `pol_glare_slider` (Glare cut) | `slider_bar` `indra/newview/skins/default/xui/en/panel_lens_looks.xml:117` | kit `slider` ↔ `RenderPolarizerGlareStrength` `F32` | `indra/newview/skins/default/xui/en/panel_lens_looks.xml:119` `pol_glare_spinner` |
| `polarizer` / `pol_skydark_slider` (Sky darken) | `slider_bar` `indra/newview/skins/default/xui/en/panel_lens_looks.xml:141` | kit `slider` ↔ `RenderPolarizerSkyDarken` `F32` | `indra/newview/skins/default/xui/en/panel_lens_looks.xml:143` `pol_skydark_spinner` |
| `polarizer` / `pol_skysat_slider` (Sky saturation) | `slider_bar` `indra/newview/skins/default/xui/en/panel_lens_looks.xml:135` | kit `slider` ↔ `RenderPolarizerSkySaturation` `F32` | `indra/newview/skins/default/xui/en/panel_lens_looks.xml:137` `pol_skysat_spinner` |
| `polarizer` / `pol_glarethr_slider` (Glare level) | `slider_bar` `indra/newview/skins/default/xui/en/panel_lens_looks.xml:123` | kit `slider` ↔ `RenderPolarizerGlareThreshold` `F32` | `indra/newview/skins/default/xui/en/panel_lens_looks.xml:125` `pol_glarethr_spinner` |
| `polarizer` / `pol_glaredepth_slider` (Glare depth) | `slider_bar` `indra/newview/skins/default/xui/en/panel_lens_looks.xml:129` | kit `slider` ↔ `RenderPolarizerGlareMaxDepth` `F32` | `indra/newview/skins/default/xui/en/panel_lens_looks.xml:131` `pol_glaredepth_spinner` |
| `polarizer` / `check_box` (Ease by sun) | none | kit `toggle` ↔ `RenderPolarizerAutoBand` `Boolean`. Contract note: generic data name resolved by control_name `RenderPolarizerAutoBand` | none |
| `polarizer` / `pol_bandrad_slider` (Band size) | `slider_bar` `indra/newview/skins/default/xui/en/panel_lens_looks.xml:151` | kit `slider` ↔ `RenderPolarizerBandRadius` `F32` | `indra/newview/skins/default/xui/en/panel_lens_looks.xml:153` `pol_bandrad_spinner` |
| `lens_filter` / `lens_filter_preset` (Filter) | `combo_box` `indra/newview/skins/default/xui/en/panel_lens_looks.xml:85` | kit `combo` ↔ `RenderLensFilterPreset` `U32` | none |
| `lens_filter` / `lens_filter_amount` (Amount) | `slider` `indra/newview/skins/default/xui/en/panel_lens_looks.xml:86` | kit `slider` ↔ `RenderLensFilterAmount` `F32` | none |
| `infrared` / `ir_preset_action` (Preset) | `combo_box` `indra/newview/skins/default/xui/en/panel_lens_looks.xml:158` | `ALLensFilterKitAdapter` command/model binding → wired in `ALPanelLensLooks::postBuild()`; behavior `ALLensLooks::applyInfraredPreset()` (`indra/newview/alpanellenslooks.cpp:47`) | none |
| `infrared` / `ir_mode` (Mode) | `combo_box` `indra/newview/skins/default/xui/en/panel_lens_looks.xml:160` | kit `combo` ↔ `RenderInfraredMode` `U32` | none |
| `infrared` / `ir_strength` (Strength) | `slider` `indra/newview/skins/default/xui/en/panel_lens_looks.xml:162` | kit `slider` ↔ `RenderInfraredStrength` `F32` | `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:118` `exp_ir_strength` |
| `infrared` / `ir_enable_tag` (Avatar tag) | `button` `indra/newview/skins/default/xui/en/panel_lens_looks.xml:164` | `ALLensFilterKitAdapter` command/model binding → wired in `ALPanelLensLooks::postBuild()`; behavior `LLNotificationsUtil::add()` (`indra/newview/alpanellenslooks.cpp:59`) | none |
| `softedge` / `softedge_preset_action` (Preset) | `combo_box` `indra/newview/skins/default/xui/en/panel_lens_looks.xml:169` | `ALLensFilterKitAdapter` command/model binding → wired in `ALPanelLensLooks::postBuild()`; behavior `ALLensLooks::applySoftEdgePreset()` (`indra/newview/alpanellenslooks.cpp:53`) | none |
| `softedge` / `softedge_blur_type` (Type) | `combo_box` `indra/newview/skins/default/xui/en/panel_lens_looks.xml:171` | kit `combo` ↔ `RenderSoftEdgeBlurType` `U32` | none |
| `softedge` / `softedge_blur` (Amount) | `slider` `indra/newview/skins/default/xui/en/panel_lens_looks.xml:173` | kit `slider` ↔ `RenderSoftEdgeBlur` `F32` | `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:132` `exp_softedge_blur` |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALLensLooks::applyInfraredPreset()` — `indra/newview/alpanellenslooks.cpp:47` | Move model-facing work into `ALLensFilterKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALLensLooks::applySoftEdgePreset()` — `indra/newview/alpanellenslooks.cpp:53` | Move model-facing work into `ALLensFilterKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLNotificationsUtil::add()` — `indra/newview/alpanellenslooks.cpp:59` | Move model-facing work into `ALLensFilterKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **52 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-gradnd-tallest-850` | Only `gradnd` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-gradnd-tallest-1160` | Only `gradnd` is put in its tallest local state at 1160 px; all other cards stay default. |
| `gradnd_preset_combo-option-0` | Only `gradnd_preset_combo` selects `Custom (use sliders)`; other selectors remain default. |
| `gradnd_preset_combo-option-1` | Only `gradnd_preset_combo` selects `Sky Hold - Light (1 stop)`; other selectors remain default. |
| `gradnd_preset_combo-option-2` | Only `gradnd_preset_combo` selects `Sky Hold - Strong (2.5)`; other selectors remain default. |
| `gradnd_preset_combo-option-3` | Only `gradnd_preset_combo` selects `Harsh Noon (3 stops)`; other selectors remain default. |
| `gradnd_preset_combo-option-4` | Only `gradnd_preset_combo` selects `Golden Hour (sun-weighted)`; other selectors remain default. |
| `gradnd_preset_combo-option-5` | Only `gradnd_preset_combo` selects `Optical Grad (no sky mask)`; other selectors remain default. |
| `gradnd_preset_combo-option-6` | Only `gradnd_preset_combo` selects `Low / Reflection Hold`; other selectors remain default. |
| `check_box-on` | Only `check_box` is on; dependent visibility is measured. |
| `card-polarizer-tallest-850` | Only `polarizer` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-polarizer-tallest-1160` | Only `polarizer` is put in its tallest local state at 1160 px; all other cards stay default. |
| `polarizer_preset_combo-option-0` | Only `polarizer_preset_combo` selects `Custom (use sliders)`; other selectors remain default. |
| `polarizer_preset_combo-option-1` | Only `polarizer_preset_combo` selects `Deep Blue Sky`; other selectors remain default. |
| `polarizer_preset_combo-option-2` | Only `polarizer_preset_combo` selects `Glare Killer (water/glass)`; other selectors remain default. |
| `polarizer_preset_combo-option-3` | Only `polarizer_preset_combo` selects `Landscape (sky + glare)`; other selectors remain default. |
| `polarizer_preset_combo-option-4` | Only `polarizer_preset_combo` selects `Subtle`; other selectors remain default. |
| `card-lens_filter-tallest-850` | Only `lens_filter` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-lens_filter-tallest-1160` | Only `lens_filter` is put in its tallest local state at 1160 px; all other cards stay default. |
| `lens_filter_preset-option-0` | Only `lens_filter_preset` selects `Custom`; other selectors remain default. |
| `lens_filter_preset-option-1` | Only `lens_filter_preset` selects `85`; other selectors remain default. |
| `lens_filter_preset-option-2` | Only `lens_filter_preset` selects `85B`; other selectors remain default. |
| `lens_filter_preset-option-3` | Only `lens_filter_preset` selects `85C`; other selectors remain default. |
| `lens_filter_preset-option-4` | Only `lens_filter_preset` selects `81A`; other selectors remain default. |
| `lens_filter_preset-option-5` | Only `lens_filter_preset` selects `80A`; other selectors remain default. |
| `lens_filter_preset-option-6` | Only `lens_filter_preset` selects `80B`; other selectors remain default. |
| `lens_filter_preset-option-7` | Only `lens_filter_preset` selects `80C`; other selectors remain default. |
| `lens_filter_preset-option-8` | Only `lens_filter_preset` selects `82A`; other selectors remain default. |
| `card-infrared-tallest-850` | Only `infrared` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-infrared-tallest-1160` | Only `infrared` is put in its tallest local state at 1160 px; all other cards stay default. |
| `ir_preset_action-option-0` | Only `ir_preset_action` selects `Wood 720nm Classic`; other selectors remain default. |
| `ir_preset_action-option-1` | Only `ir_preset_action` selects `Kodak HIE`; other selectors remain default. |
| `ir_preset_action-option-2` | Only `ir_preset_action` selects `Subtle 665nm`; other selectors remain default. |
| `ir_preset_action-option-3` | Only `ir_preset_action` selects `Deep 850nm`; other selectors remain default. |
| `ir_preset_action-option-4` | Only `ir_preset_action` selects `Aerochrome #12`; other selectors remain default. |
| `ir_preset_action-option-5` | Only `ir_preset_action` selects `Aerochrome Hot Pink #21`; other selectors remain default. |
| `ir_preset_action-option-6` | Only `ir_preset_action` selects `Aerochrome Red #25`; other selectors remain default. |
| `ir_mode-option-0` | Only `ir_mode` selects `Black & white`; other selectors remain default. |
| `ir_mode-option-1` | Only `ir_mode` selects `Aerochrome`; other selectors remain default. |
| `card-softedge-tallest-850` | Only `softedge` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-softedge-tallest-1160` | Only `softedge` is put in its tallest local state at 1160 px; all other cards stay default. |
| `softedge_preset_action-option-0` | Only `softedge_preset_action` selects `Vaseline Oval`; other selectors remain default. |
| `softedge_preset_action-option-1` | Only `softedge_preset_action` selects `Lensbaby Sweet`; other selectors remain default. |
| `softedge_preset_action-option-2` | Only `softedge_preset_action` selects `Dream Sequence`; other selectors remain default. |
| `softedge_preset_action-option-3` | Only `softedge_preset_action` selects `Faux Tilt Band`; other selectors remain default. |
| `softedge_preset_action-option-4` | Only `softedge_preset_action` selects `Vintage Portrait`; other selectors remain default. |
| `softedge_preset_action-option-5` | Only `softedge_preset_action` selects `Nightmare Edge`; other selectors remain default. |
| `softedge_blur_type-option-0` | Only `softedge_blur_type` selects `Diffusion`; other selectors remain default. |
| `softedge_blur_type-option-1` | Only `softedge_blur_type` selects `Defocus (needs DoF)`; other selectors remain default. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## Ambiguous same-name candidates (never auto-bound)

None.

## Rejected out-of-scope same-name candidates

2 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

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
