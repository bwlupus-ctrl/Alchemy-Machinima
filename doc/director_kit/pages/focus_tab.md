# Focus — Director Kit page contract

Page key: `focus_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (14/14 setting-bound rows; 2 C++-referenced rows).

## Real owners

- `panel_dof` / `panel` at `indra/newview/skins/default/xui/en/panel_dof.xml:2` (12 matching rows).
- `floater_phototools` / `floater` at `indra/newview/skins/default/xui/en/floater_phototools.xml:25` (5 matching rows).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALFocusKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `dof` / `dof_fstop_slider` (Aperture) | `slider_bar` `indra/newview/skins/default/xui/en/panel_dof.xml:179` | kit `slider` ↔ `CameraFNumber` `F32` | `indra/newview/skins/default/xui/en/panel_dof.xml:195` `dof_fstop_spinner` |
| `dof` / `dof_length_slider` (Focal length) | `slider_bar` `indra/newview/skins/default/xui/en/panel_dof.xml:236` | kit `slider` ↔ `CameraFocalLength` `F32` | `indra/newview/skins/default/xui/en/panel_dof.xml:252` `dof_length_spinner` |
| `dof` / `dof_coc_slider` (Max blur) | `slider_bar` `indra/newview/skins/default/xui/en/panel_dof.xml:294` | kit `slider` ↔ `CameraMaxCoF` `F32` | `indra/newview/skins/default/xui/en/panel_dof.xml:310` `dof_coc_spinner` |
| `dof` / `dof_res_combo` (Quality) | `combo_box` `indra/newview/skins/default/xui/en/panel_dof.xml:82` | kit `combo` ↔ `CameraDoFResScale` `F32` | none |
| `dof` / `RenderDepthOfFieldFront` (Near blur) | `check_box` `indra/newview/skins/default/xui/en/panel_dof.xml:53` | kit `toggle` ↔ `RenderDepthOfFieldNearBlur` `Boolean` | none |
| `dof` / `dof_focaltime_slider` (Focus speed) | `slider_bar` `indra/newview/skins/default/xui/en/panel_dof.xml:351` | kit `slider` ↔ `CameraFocusTransitionTime` `F32` | `indra/newview/skins/default/xui/en/panel_dof.xml:367` `dof_focaltime_spinner` |
| `dof` / `dof_fov_slider` (Field of view) | `slider_bar` `indra/newview/skins/default/xui/en/panel_dof.xml:123` | kit `slider` ↔ `CameraFieldOfView` `F32` | `indra/newview/skins/default/xui/en/panel_dof.xml:139` `dof_fov_spinner` |
| `focus_point` / `check_box` (Lock focus) | none | kit `toggle` ↔ `RenderFocusPointLocked` `Boolean`; `ALFocusKitAdapter` preserves/calls `?::?()` (`indra/llui/llcheckboxctrl.cpp:44`), `?::?()` (`indra/newview/tests/aldiopterrelevance_test.cpp:152`), `?::?()` (`indra/newview/tests/aldiopterrelevance_test.cpp:315`) | `indra/newview/skins/default/xui/en/floater_phototools.xml:196` `FocusLock`, `indra/newview/skins/default/xui/en/floater_ultimate_diopter.xml:279` `diopter_focus_locked` |
| `focus_point` / `check_box` (Follow mouse) | none | kit `toggle` ↔ `RenderFocusPointFollowsPointer` `Boolean`; `ALFocusKitAdapter` preserves/calls `?::?()` (`indra/llui/llcheckboxctrl.cpp:44`), `?::?()` (`indra/newview/tests/aldiopterrelevance_test.cpp:152`), `?::?()` (`indra/newview/tests/aldiopterrelevance_test.cpp:315`) | `indra/newview/skins/default/xui/en/floater_phototools.xml:207` `FocusFollowsPointer` |
| `focus_point` / `FocusCrosshair` (Show crosshair) | `check_box` `indra/newview/skins/default/xui/en/floater_phototools.xml:218`, `check_box` `indra/newview/skins/default/xui/en/panel_dof.xml:38` | kit `toggle` ↔ `RenderFocusPointCrosshair` `Boolean` | `indra/newview/skins/default/xui/en/floater_ultimate_diopter.xml:277` `diopter_focus_crosshair` |
| `guide` / `GuideStyle` (Style) | `combo_box` `indra/newview/skins/default/xui/en/floater_phototools.xml:252`, `combo_box` `indra/newview/skins/default/xui/en/panel_dof.xml:409` | kit `combo` ↔ `RenderCompositionGuideStyle` `String` | none |
| `guide` / `GuideColor` (Colour) | `color_swatch` `indra/newview/skins/default/xui/en/floater_phototools.xml:286`, `color_swatch` `indra/newview/skins/default/xui/en/panel_dof.xml:443` | kit `color` ↔ `RenderCompositionGuideColor` `Color3` | none |
| `guide` / `GuideThickness` (Thickness) | `spinner` `indra/newview/skins/default/xui/en/floater_phototools.xml:308`, `spinner` `indra/newview/skins/default/xui/en/panel_dof.xml:465` | kit `number` ↔ `RenderCompositionGuideWidth` `F32` | none |
| `guide` / `GuideOpacity` (Opacity) | `slider` `indra/newview/skins/default/xui/en/floater_phototools.xml:332`, `slider` `indra/newview/skins/default/xui/en/panel_dof.xml:489` | kit `slider` ↔ `RenderCompositionGuideVisibility` `F32` | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `?::?()` — `indra/llui/llcheckboxctrl.cpp:44` | Move model-facing work into `ALFocusKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `?::?()` — `indra/newview/tests/aldiopterrelevance_test.cpp:152, 315` | Move model-facing work into `ALFocusKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **22 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-dof-tallest-850` | Only `dof` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-dof-tallest-1160` | Only `dof` is put in its tallest local state at 1160 px; all other cards stay default. |
| `dof_res_combo-option-0` | Only `dof_res_combo` selects `Quarter`; other selectors remain default. |
| `dof_res_combo-option-1` | Only `dof_res_combo` selects `Half`; other selectors remain default. |
| `dof_res_combo-option-2` | Only `dof_res_combo` selects `Three-Quarter`; other selectors remain default. |
| `dof_res_combo-option-3` | Only `dof_res_combo` selects `Full`; other selectors remain default. |
| `RenderDepthOfFieldFront-on` | Only `RenderDepthOfFieldFront` is on; dependent visibility is measured. |
| `card-focus_point-tallest-850` | Only `focus_point` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-focus_point-tallest-1160` | Only `focus_point` is put in its tallest local state at 1160 px; all other cards stay default. |
| `check_box-on` | Only `check_box` is on; dependent visibility is measured. |
| `FocusCrosshair-on` | Only `FocusCrosshair` is on; dependent visibility is measured. |
| `card-guide-tallest-850` | Only `guide` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-guide-tallest-1160` | Only `guide` is put in its tallest local state at 1160 px; all other cards stay default. |
| `GuideStyle-option-0` | Only `GuideStyle` selects `Rule of Thirds`; other selectors remain default. |
| `GuideStyle-option-1` | Only `GuideStyle` selects `Golden Ratio (Top Left)`; other selectors remain default. |
| `GuideStyle-option-2` | Only `GuideStyle` selects `Golden Ratio (Top Right)`; other selectors remain default. |
| `GuideStyle-option-3` | Only `GuideStyle` selects `Golden Ratio (Bottom Left)`; other selectors remain default. |
| `GuideStyle-option-4` | Only `GuideStyle` selects `Golden Ratio (Bottom Right)`; other selectors remain default. |
| `GuideStyle-option-5` | Only `GuideStyle` selects `Diagonal`; other selectors remain default. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## New widget required

None. Every approved visual kind is either a kit primitive or an existing XUI control. Missing action/model bindings below are USER DECISIONS, not permission to invent a visual class.

## Unresolved and USER DECISION items

- `check_box` (Lock focus): widget not found in XUI.
- `check_box` (Follow mouse): widget not found in XUI.

## Risks

- A widget-name collision can select an unrelated XUI root or C++ literal; ownership must be resolved by root, class, and behavior together.
- Extracting model work without rewiring the existing owner creates divergent v1/v2 behavior; the extraction and forwarding call are one reviewed change.
- Duplicate writable settings must obey the rollout homes table; this page emits a link/read-only row when it is not the chosen home.
- Shared edit models require owner tokens, and external presets/cue playback require one global history barrier at start/stop rather than per write.
- Unresolved rows fail generation closed; they are never silently replaced with a guessed widget or command.
