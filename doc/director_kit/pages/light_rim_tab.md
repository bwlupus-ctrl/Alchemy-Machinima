# Rim — Director Kit page contract

Page key: `light_rim_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (24/25 setting-bound rows; 4 C++-referenced rows).

## Real owners

- `panel_cine_light_rig` / `panel` at `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:8` (25 matching rows).
- Referencing classes: `ALPanelCineLightRig` (4).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALRigRimKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `rig_rim` / `cine_rig_rim_master_gain` (Master) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:966` | kit `slider` ↔ `CineRigRimMasterGain` `F32` | none |
| `rig_rim` / `cine_rig_rim_roughness_soften` (Soften) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:972` | kit `slider` ↔ `CineRigRimRoughnessSoften` `F32` | none |
| `rig_rim` / `cine_rig_rim_back_softness` (Back softness) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:978` | kit `slider` ↔ `CineRigRimBackSoftness` `F32` | none |
| `rig_rim` / `cine_rig_rim_tint` (Tint) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:984` | kit `slider` ↔ `CineRigRimTint` `F32` | none |
| `rig_rim` / `cine_rig_rim_shadow` (Shadow) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:991` | kit `slider` ↔ `CineRigRimShadow` `F32` | none |
| `rig_rim` / `cine_rig_rim_include_alpha` (Alpha surfaces) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:995` | kit `toggle` ↔ `CineRigRimIncludeAlpha` `Boolean` | none |
| `rig_rim` / `cine_rig_rim_include_probes` (Probes) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:998` | kit `toggle` ↔ `CineRigRimIncludeProbes` `Boolean` | none |
| `rig_rim` / `cine_rig_rim_include_bounce` (Bounce fills) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1001` | kit `toggle` ↔ `CineRigRimIncludeBounce` `Boolean` | none |
| `rim_quality` / `cine_rimq_smooth` (Smooth) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1103` | kit `slider` ↔ `CineRigRimSmoothness` `F32` | none |
| `rim_quality` / `cine_rimq_steady` (Steady) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1106` | kit `slider` ↔ `CineRigRimAntiShimmer` `F32` | none |
| `rim_quality` / `cine_rimq_actors` (Actors only) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1098` | kit `toggle` ↔ `CineRigRimActorsOnly` `Boolean` | none |
| `rim_quality` / `cine_rimq_enable_tag` (Avatar tag) | `button` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1100` | `ALRigRimKitAdapter` command/model binding → `ALPanelCineLightRig::postBuild()` (`indra/newview/alpanelcinelightrig.cpp:768`) | none |
| `rim_edge` / `cine_rimq_edge` (Edge) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1109` | kit `slider` ↔ `CineRigRimSilhouette` `F32` | none |
| `rim_edge` / `cine_rimq_width` (Width) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1112` | kit `slider` ↔ `CineRigRimSilhouetteWidth` `F32` | none |
| `rim_edge` / `cine_rimq_units` (Units) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1113` | kit `combo` ↔ `CineRigRimSilhouetteUnits` `U32` | none |
| `rim_edge` / `cine_rimq_soft` (Soft) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1116` | kit `slider` ↔ `CineRigRimSilhouetteSoft` `F32` | none |
| `rim_wrap` / `cine_rimq_wrap_amount` (Amount) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1121` | kit `slider` ↔ `CineRigRimWrapStrength` `F32` | none |
| `rim_wrap` / `cine_rimq_wrap_reach` (Reach) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1124` | kit `slider` ↔ `CineRigRimWrapWidth` `F32` | none |
| `rim_wrap` / `cine_rimq_wrap_blur` (Blur) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1127` | kit `slider` ↔ `CineRigRimWrapBlur` `F32` | none |
| `rim_wrap` / `cine_rimq_wrap_colour` (Colour) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1130` | kit `slider` ↔ `CineRigRimWrapSaturation` `F32` | none |
| `rim_tron` / `cine_rig_rim_tron_mode` (Mode) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1070` | kit `combo` ↔ `CineRigRimTronMode` `S32`; `ALRigRimKitAdapter` preserves/calls `ALPanelCineLightRig::postBuild()` (`indra/newview/alpanelcinelightrig.cpp:807`) | page `tron_tab`, `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6321` `tr_rigrim_tron_mode_combo` |
| `rim_tron` / `cine_rig_rim_tron_mix` (Mix) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1081` | kit `slider` ↔ `CineRigRimTronMix` `F32`; `ALRigRimKitAdapter` preserves/calls `ALPanelCineLightRig::postBuild()` (`indra/newview/alpanelcinelightrig.cpp:808`) | page `tron_tab`, `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6327` `tr_rigrim_tron_mix_slider` |
| `rim_tron` / `cine_rig_rim_tron_color` (Colour) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1086` | kit `combo` ↔ `CineRigRimTronColorSource` `S32`; `ALRigRimKitAdapter` preserves/calls `ALPanelCineLightRig::postBuild()` (`indra/newview/alpanelcinelightrig.cpp:810`) | page `tron_tab`, `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6330` `tr_rigrim_tron_color_combo` |
| `rim_diagnostics` / `cine_rig_rim_debug` (Rim only) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:953` | kit `toggle` ↔ `CineRigRimDebugRimOnly` `Boolean` | none |
| `rim_diagnostics` / `cine_rimq_debug` (Debug view) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1133` | kit `combo` ↔ `CineRigRimDebugView` `U32` | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALPanelCineLightRig::postBuild()` — `indra/newview/alpanelcinelightrig.cpp:768, 807, 808, 810` | Move model-facing work into `ALRigRimKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **34 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-rig_rim-tallest-850` | Only `rig_rim` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-rig_rim-tallest-1160` | Only `rig_rim` is put in its tallest local state at 1160 px; all other cards stay default. |
| `cine_rig_rim_include_alpha-on` | Only `cine_rig_rim_include_alpha` is on; dependent visibility is measured. |
| `cine_rig_rim_include_probes-on` | Only `cine_rig_rim_include_probes` is on; dependent visibility is measured. |
| `cine_rig_rim_include_bounce-on` | Only `cine_rig_rim_include_bounce` is on; dependent visibility is measured. |
| `card-rim_quality-tallest-850` | Only `rim_quality` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-rim_quality-tallest-1160` | Only `rim_quality` is put in its tallest local state at 1160 px; all other cards stay default. |
| `cine_rimq_actors-on` | Only `cine_rimq_actors` is on; dependent visibility is measured. |
| `card-rim_edge-tallest-850` | Only `rim_edge` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-rim_edge-tallest-1160` | Only `rim_edge` is put in its tallest local state at 1160 px; all other cards stay default. |
| `cine_rimq_units-option-0` | Only `cine_rimq_units` selects `cm @ subject`; other selectors remain default. |
| `cine_rimq_units-option-1` | Only `cine_rimq_units` selects `px @ 1080p`; other selectors remain default. |
| `card-rim_wrap-tallest-850` | Only `rim_wrap` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-rim_wrap-tallest-1160` | Only `rim_wrap` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-rim_tron-tallest-850` | Only `rim_tron` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-rim_tron-tallest-1160` | Only `rim_tron` is put in its tallest local state at 1160 px; all other cards stay default. |
| `cine_rig_rim_tron_mode-option-0` | Only `cine_rig_rim_tron_mode` selects `Off`; other selectors remain default. |
| `cine_rig_rim_tron_mode-option-1` | Only `cine_rig_rim_tron_mode` selects `Palette tint`; other selectors remain default. |
| `cine_rig_rim_tron_mode-option-2` | Only `cine_rig_rim_tron_mode` selects `Tint + pulse`; other selectors remain default. |
| `cine_rig_rim_tron_color-option-0` | Only `cine_rig_rim_tron_color` selects `Primary`; other selectors remain default. |
| `cine_rig_rim_tron_color-option-1` | Only `cine_rig_rim_tron_color` selects `Secondary`; other selectors remain default. |
| `cine_rig_rim_tron_color-option-2` | Only `cine_rig_rim_tron_color` selects `Accent`; other selectors remain default. |
| `card-rim_diagnostics-tallest-850` | Only `rim_diagnostics` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-rim_diagnostics-tallest-1160` | Only `rim_diagnostics` is put in its tallest local state at 1160 px; all other cards stay default. |
| `cine_rig_rim_debug-on` | Only `cine_rig_rim_debug` is on; dependent visibility is measured. |
| `cine_rimq_debug-option-0` | Only `cine_rimq_debug` selects `Off`; other selectors remain default. |
| `cine_rimq_debug-option-1` | Only `cine_rimq_debug` selects `Rim only`; other selectors remain default. |
| `cine_rimq_debug-option-2` | Only `cine_rimq_debug` selects `Silhouette band`; other selectors remain default. |
| `cine_rimq_debug-option-3` | Only `cine_rimq_debug` selects `Smooth normal`; other selectors remain default. |
| `cine_rimq_debug-option-4` | Only `cine_rimq_debug` selects `Subject mask`; other selectors remain default. |
| `cine_rimq_debug-option-5` | Only `cine_rimq_debug` selects `Wrap only`; other selectors remain default. |

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
