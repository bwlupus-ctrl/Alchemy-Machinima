# Shadows — Director Kit page contract

Page key: `shadows_probes_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **pure-setting** (37/39 setting-bound rows; 0 C++-referenced rows).

## Real owners

- `floater_lightbox_settings` / `floater` at `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:8` (35 matching rows).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALShadowProbeKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `shadow_type` / `shadowtype_combo` (Shadows) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:2567` | kit `combo` ↔ `RenderShadowDetail` `S32` | `indra/newview/skins/default/xui/en/floater_preferences_graphics_advanced.xml:865` `ShadowDetail`, `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:13` `shadowtype_combo`, `indra/newview/skins/default/xui/en/panel_performance_preferences.xml:309` `ShadowDetail` |
| `shadow_type` / `set_RenderDeferredSSAO` (SSAO) | none | kit `toggle` ↔ `RenderDeferredSSAO` `Boolean`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/floater_preferences_graphics_advanced.xml:799` `UseSSAO`, `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:22` `set_RenderDeferredSSAO` |
| `sharp` / `sd_near_split_slider` (Sharp range m) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:3470` | kit `slider` ↔ `RenderShadowNearSplitMeters` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:27` `sd_near_split_slider` |
| `sharp` / `sd_split_blend_slider` (Cascade blend) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:3510` | kit `slider` ↔ `RenderShadowSplitBlend` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:32` `sd_split_blend_slider` |
| `soft` / `sd_soften_mode_combo` (Softening) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:3347` | kit `combo` ↔ `RenderShadowSoftenMode` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:39` `sd_soften_mode_combo` |
| `soft` / `sd_soft_world_mm_slider` (Penumbra mm) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:3712` | kit `slider` ↔ `RenderShadowSoftWorldMM` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:49` `sd_soft_world_mm_slider` |
| `soft` / `sd_soften_px_slider` (Radius px) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:3389` | kit `slider` ↔ `RenderShadowSoftenPx` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:54` `sd_soften_px_slider` |
| `soft` / `sd_soften_mm_slider` (Radius mm) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:3429` | kit `slider` ↔ `RenderShadowSoftenMM` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:59` `sd_soften_mm_slider` |
| `shadow_subject` / `sd_subject_cascade_check` (Follow subject) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:3780` | kit `toggle` ↔ `RenderShadowSubjectCascade` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:66` `sd_subject_cascade_check` |
| `shadow_subject` / `sd_subject_margin` (Margin m) | `spinner` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:3851` | kit `number` ↔ `RenderShadowSubjectMargin` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:69` `sd_subject_margin` |
| `shadow_subject` / `sd_subject_reach` (Reach m) | `spinner` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:3876` | kit `number` ↔ `RenderShadowSubjectReach` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:72` `sd_subject_reach` |
| `shadow_subject` / `sd_subject_reach_max` (Reach cap m) | `spinner` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:3892` | kit `number` ↔ `RenderShadowSubjectReachMax` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:75` `sd_subject_reach_max` |
| `shadow_subject` / `sd_subject_max_distance` (Max dist m) | `spinner` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:3917` | kit `number` ↔ `RenderShadowSubjectMaxDistance` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:78` `sd_subject_max_distance` |
| `shadow_subject` / `sd_subject_feather` (Feather) | `spinner` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:3933` | kit `number` ↔ `RenderShadowSubjectFeather` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:81` `sd_subject_feather` |
| `shadow_subject` / `sd_subject_snap_check` (Snap texels) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:3945` | kit `toggle` ↔ `RenderShadowSubjectSnap` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:84` `sd_subject_snap_check` |
| `shadow_projectors` / `bd_max_spot_shadows` (Max count) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:4076` | kit `slider` ↔ `BDMergeMaxSpotShadows` `U32` | `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:89` `bd_max_spot_shadows` |
| `shadow_projectors` / `sd_alpha_cap_combo` (Cutout range) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:4031` | kit `combo` ↔ `RenderShadowAlphaNearCascades` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:94` `sd_alpha_cap_combo` |
| `shadow_projectors` / `sd_alpha_mode_combo` (Alpha policy) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:3976` | kit `combo` ↔ `RenderShadowAlphaMode` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:104` `sd_alpha_mode_combo` |
| `shadow_projectors` / `sd_alpha_cutoff` (Alpha cutoff) | `spinner` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:4010` | kit `number` ↔ `RenderShadowAlphaCutoff` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:112` `sd_alpha_cutoff` |
| `shadow_projectors` / `shadows_soft_projector_link` (Soft shadows) | none | kit-native action `link_to:light_group/projector_shafts_tab`; no duplicate widget is required | none |
| `tune` / `sd_units_mode_combo` (Units) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:3557` | kit `combo` ↔ `RenderShadowUnitsMode` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:118` `sd_units_mode_combo` |
| `tune` / `sd_units_scope_combo` (Units scope) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:3748` | kit `combo` ↔ `RenderShadowUnitsScope` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:126` `sd_units_scope_combo` |
| `tune` / `sd_bias_mm_slider` (Bias mm) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:3591` | kit `slider` ↔ `RenderShadowBiasMM` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:134` `sd_bias_mm_slider` |
| `tune` / `sd_offset_texels_slider` (Offset texels) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:3631` | kit `slider` ↔ `RenderShadowOffsetTexels` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:139` `sd_offset_texels_slider` |
| `tune` / `sd_slope_scale_slider` (Slope scale) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:3671` | kit `slider` ↔ `RenderShadowSlopeScale` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:144` `sd_slope_scale_slider` |
| `tune` / `shadow_bias_slider` (Shadow bias) | `slider_bar` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:2657` | kit `slider` ↔ `RenderShadowBias` `F32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:2672` `shadow_bias_spinner`, `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:149` `shadow_bias_slider` |
| `tune` / `shadow_offset_slider` (Shadow offset) | `slider_bar` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:2710` | kit `slider` ↔ `RenderShadowOffset` `F32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:2725` `shadow_offset_spinner`, `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:154` `shadow_offset_slider` |
| `tune` / `shadow_spotbias_slider` (Spot bias) | `slider_bar` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:2763` | kit `slider` ↔ `RenderSpotShadowBias` `F32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:2778` `shadow_spotbias_spinner`, `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:159` `shadow_spotbias_slider` |
| `tune` / `shadow_spotoffset_slider` (Spot offset) | `slider_bar` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:2816` | kit `slider` ↔ `RenderSpotShadowOffset` `F32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:2831` `shadow_spotoffset_spinner`, `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:164` `shadow_spotoffset_slider` |
| `tune` / `shadowres_slider` (Resolution x) | `slider_bar` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:2604` | kit `slider` ↔ `RenderShadowResolutionScale` `F32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:2619` `shadowres_spinner`, `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:169` `shadowres_slider` |
| `tune` / `bd_shadow_res0` (Cascade 0 res) | `spinner` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:3285` | kit `number` ↔ `BDMergeShadowResolution0` `U32` | `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:174` `bd_shadow_res0` |
| `tune` / `bd_shadow_res1` (Cascade 1 res) | `spinner` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:3299` | kit `number` ↔ `BDMergeShadowResolution1` `U32` | `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:177` `bd_shadow_res1` |
| `tune` / `bd_shadow_res2` (Cascade 2 res) | `spinner` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:3313` | kit `number` ↔ `BDMergeShadowResolution2` `U32` | `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:180` `bd_shadow_res2` |
| `tune` / `bd_shadow_res3` (Cascade 3 res) | `spinner` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:3327` | kit `number` ↔ `BDMergeShadowResolution3` `U32` | `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:183` `bd_shadow_res3` |
| `tune` / `bd_shadow_res_proj` (Projector res) | `spinner` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:4116` | kit `number` ↔ `BDMergeProjectorShadowResolution` `U32` | `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:186` `bd_shadow_res_proj` |
| `tune` / `shadow_blur_slider` (Blur size) | `slider_bar` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:2869` | kit `slider` ↔ `RenderShadowBlurSize` `F32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:2884` `shadow_blur_spinner`, `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:189` `shadow_blur_slider` |
| `tune` / `shadow_blurdist_slider` (Blur distance) | `slider_bar` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:2922` | kit `slider` ↔ `RenderShadowBlurDistFactor` `F32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:2937` `shadow_blurdist_spinner`, `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:194` `shadow_blurdist_slider` |
| `tune` / `open_lightbox_shadows` (Lightbox ›) | none | kit-native action `floater:lightbox/shadow_settings`; no duplicate widget is required | none |
| `probe_reach` / `set_RenderReflectionProbeReachCull` (Skip far probes) | none | kit `toggle` ↔ `RenderReflectionProbeReachCull` `Boolean`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_world_shadows_probes.xml:204` `set_RenderReflectionProbeReachCull` |

## Adapter extraction

No approved widget has a C++ literal reference. `ALShadowProbeKitAdapter` owns only explicit actions and setting signals; it must not synthesize a polling mirror.

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
| `card-shadow_type-tallest-850` | Only `shadow_type` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-shadow_type-tallest-1160` | Only `shadow_type` is put in its tallest local state at 1160 px; all other cards stay default. |
| `shadowtype_combo-option-0` | Only `shadowtype_combo` selects `None`; other selectors remain default. |
| `shadowtype_combo-option-1` | Only `shadowtype_combo` selects `Sun/Moon`; other selectors remain default. |
| `shadowtype_combo-option-2` | Only `shadowtype_combo` selects `Sun/Moon + projectors`; other selectors remain default. |
| `set_RenderDeferredSSAO-on` | Only `set_RenderDeferredSSAO` is on; dependent visibility is measured. |
| `card-sharp-tallest-850` | Only `sharp` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-sharp-tallest-1160` | Only `sharp` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-soft-tallest-850` | Only `soft` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-soft-tallest-1160` | Only `soft` is put in its tallest local state at 1160 px; all other cards stay default. |
| `sd_soften_mode_combo-option-0` | Only `sd_soften_mode_combo` selects `Stock (shares SSAO blur)`; other selectors remain default. |
| `sd_soften_mode_combo-option-1` | Only `sd_soften_mode_combo` selects `Pixels`; other selectors remain default. |
| `sd_soften_mode_combo-option-2` | Only `sd_soften_mode_combo` selects `World mm`; other selectors remain default. |
| `sd_soften_mode_combo-option-3` | Only `sd_soften_mode_combo` selects `None`; other selectors remain default. |
| `card-shadow_subject-tallest-850` | Only `shadow_subject` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-shadow_subject-tallest-1160` | Only `shadow_subject` is put in its tallest local state at 1160 px; all other cards stay default. |
| `sd_subject_cascade_check-on` | Only `sd_subject_cascade_check` is on; dependent visibility is measured. |
| `sd_subject_snap_check-on` | Only `sd_subject_snap_check` is on; dependent visibility is measured. |
| `card-shadow_projectors-tallest-850` | Only `shadow_projectors` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-shadow_projectors-tallest-1160` | Only `shadow_projectors` is put in its tallest local state at 1160 px; all other cards stay default. |
| `sd_alpha_cap_combo-option-0` | Only `sd_alpha_cap_combo` selects `All distances (stock)`; other selectors remain default. |
| `sd_alpha_cap_combo-option-1` | Only `sd_alpha_cap_combo` selects `Near only`; other selectors remain default. |
| `sd_alpha_cap_combo-option-2` | Only `sd_alpha_cap_combo` selects `Near + mid`; other selectors remain default. |
| `sd_alpha_cap_combo-option-3` | Only `sd_alpha_cap_combo` selects `All but far`; other selectors remain default. |
| `sd_alpha_mode_combo-option-0` | Only `sd_alpha_mode_combo` selects `Stock (dither below 0.88)`; other selectors remain default. |
| `sd_alpha_mode_combo-option-1` | Only `sd_alpha_mode_combo` selects `Low cutoff, no dither`; other selectors remain default. |
| `card-tune-tallest-850` | Only `tune` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-tune-tallest-1160` | Only `tune` is put in its tallest local state at 1160 px; all other cards stay default. |
| `sd_units_mode_combo-option-0` | Only `sd_units_mode_combo` selects `Legacy (fractions)`; other selectors remain default. |
| `sd_units_mode_combo-option-1` | Only `sd_units_mode_combo` selects `World (mm / texels)`; other selectors remain default. |
| `sd_units_scope_combo-option-0` | Only `sd_units_scope_combo` selects `All cascades`; other selectors remain default. |
| `sd_units_scope_combo-option-1` | Only `sd_units_scope_combo` selects `Cascade 0 only`; other selectors remain default. |
| `card-probe_reach-tallest-850` | Only `probe_reach` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-probe_reach-tallest-1160` | Only `probe_reach` is put in its tallest local state at 1160 px; all other cards stay default. |
| `set_RenderReflectionProbeReachCull-on` | Only `set_RenderReflectionProbeReachCull` is on; dependent visibility is measured. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## Ambiguous same-name candidates (never auto-bound)

None.

## Rejected out-of-scope same-name candidates

39 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

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
