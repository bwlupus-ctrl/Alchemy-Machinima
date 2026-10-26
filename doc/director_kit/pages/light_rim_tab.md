# Rim — Director Kit page contract

Page key: `light_rim_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (71/81 setting-bound rows; 7 C++-referenced rows).

## Real owners

- `panel_cine_light_rig` / `panel` at `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:8` (43 matching rows).
- Referencing classes: `ALRigRimController` (5), `LLPipeline` (3).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALRigRimKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `rig_rim` / `cine_rig_rim_master_gain` (Master) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:966` | kit `slider` ↔ `CineRigRimMasterGain` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:10` `cine_rig_rim_master_gain` |
| `rig_rim` / `cine_rig_rim_roughness_soften` (Soften) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:972` | kit `slider` ↔ `CineRigRimRoughnessSoften` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:15` `cine_rig_rim_roughness_soften` |
| `rig_rim` / `cine_rig_rim_back_softness` (Back softness) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:978` | kit `slider` ↔ `CineRigRimBackSoftness` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:20` `cine_rig_rim_back_softness` |
| `rig_rim` / `cine_rig_rim_tint` (Tint) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:984` | kit `slider` ↔ `CineRigRimTint` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:25` `cine_rig_rim_tint` |
| `rig_rim` / `cine_rig_rim_shadow` (Shadow) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:991` | kit `slider` ↔ `CineRigRimShadow` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:30` `cine_rig_rim_shadow` |
| `rig_rim` / `cine_rig_rim_include_alpha` (Alpha surfaces) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:995` | kit `toggle` ↔ `CineRigRimIncludeAlpha` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:35` `cine_rig_rim_include_alpha` |
| `rig_rim` / `cine_rig_rim_include_probes` (Probes) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:998` | kit `toggle` ↔ `CineRigRimIncludeProbes` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:38` `cine_rig_rim_include_probes` |
| `rig_rim` / `cine_rig_rim_include_bounce` (Bounce fills) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1001` | kit `toggle` ↔ `CineRigRimIncludeBounce` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:41` `cine_rig_rim_include_bounce` |
| `rim_quality` / `cine_rimq_smooth` (Smoothness) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1103` | kit `slider` ↔ `CineRigRimSmoothness` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:46` `cine_rimq_smooth` |
| `rim_quality` / `cine_rimq_smooth_radius` (Smooth radius) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1106` | kit `slider` ↔ `CineRigRimSmoothRadius` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:51` `cine_rimq_smooth_radius` |
| `rim_quality` / `cine_rimq_texture_normals` (Texture normals) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1109` | kit `slider` ↔ `CineRigRimTextureNormals` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:56` `cine_rimq_texture_normals` |
| `rim_quality` / `cine_rimq_steady` (Steady) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1112` | kit `slider` ↔ `CineRigRimAntiShimmer` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:61` `cine_rimq_steady` |
| `rim_quality` / `cine_rimq_actors` (Actors only) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1098` | kit `toggle` ↔ `CineRigRimActorsOnly` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:66` `cine_rimq_actors` |
| `rim_quality` / `cine_rimq_enable_tag` (Avatar tag) | `button` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1100` | `ALRigRimKitAdapter` command/model binding → behavior `ALRigRimController::attach()` (`indra/newview/alrigrimcontroller.cpp:124`) | none |
| `per_light_rim [Key]` / `cine_rig_rim_key_gain` (Strength) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1011` | kit `number` ↔ `CineRigRimKeyGain` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:81` `cine_rig_rim_key_gain` |
| `per_light_rim [Key]` / `cine_rig_rim_key_sharpness` (Sharpness) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1014` | kit `number` ↔ `CineRigRimKeySharpness` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:84` `cine_rig_rim_key_sharpness` |
| `per_light_rim [Key]` / `cine_rig_rim_key_wrap` (Wrap) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1017` | kit `number` ↔ `CineRigRimKeyWrap` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:87` `cine_rig_rim_key_wrap` |
| `per_light_rim [Key]` / `cine_rig_rim_key_back_bias` (Back bias) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1020` | kit `number` ↔ `CineRigRimKeyBackBias` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:90` `cine_rig_rim_key_back_bias` |
| `per_light_rim [Fill]` / `cine_rig_rim_fill_gain` (Strength) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1025` | kit `number` ↔ `CineRigRimFillGain` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:95` `cine_rig_rim_fill_gain` |
| `per_light_rim [Fill]` / `cine_rig_rim_fill_sharpness` (Sharpness) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1028` | kit `number` ↔ `CineRigRimFillSharpness` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:98` `cine_rig_rim_fill_sharpness` |
| `per_light_rim [Fill]` / `cine_rig_rim_fill_wrap` (Wrap) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1031` | kit `number` ↔ `CineRigRimFillWrap` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:101` `cine_rig_rim_fill_wrap` |
| `per_light_rim [Fill]` / `cine_rig_rim_fill_back_bias` (Back bias) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1034` | kit `number` ↔ `CineRigRimFillBackBias` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:104` `cine_rig_rim_fill_back_bias` |
| `per_light_rim [Back]` / `cine_rig_rim_rim_gain` (Strength) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1039` | kit `number` ↔ `CineRigRimRimGain` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:109` `cine_rig_rim_rim_gain` |
| `per_light_rim [Back]` / `cine_rig_rim_rim_sharpness` (Sharpness) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1042` | kit `number` ↔ `CineRigRimRimSharpness` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:112` `cine_rig_rim_rim_sharpness` |
| `per_light_rim [Back]` / `cine_rig_rim_rim_wrap` (Wrap) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1045` | kit `number` ↔ `CineRigRimRimWrap` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:115` `cine_rig_rim_rim_wrap` |
| `per_light_rim [Back]` / `cine_rig_rim_rim_back_bias` (Back bias) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1048` | kit `number` ↔ `CineRigRimRimBackBias` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:118` `cine_rig_rim_rim_back_bias` |
| `per_light_rim [BG]` / `cine_rig_rim_bg_gain` (Strength) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1053` | kit `number` ↔ `CineRigRimBgGain` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:123` `cine_rig_rim_bg_gain` |
| `per_light_rim [BG]` / `cine_rig_rim_bg_sharpness` (Sharpness) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1056` | kit `number` ↔ `CineRigRimBgSharpness` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:126` `cine_rig_rim_bg_sharpness` |
| `per_light_rim [BG]` / `cine_rig_rim_bg_wrap` (Wrap) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1059` | kit `number` ↔ `CineRigRimBgWrap` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:129` `cine_rig_rim_bg_wrap` |
| `per_light_rim [BG]` / `cine_rig_rim_bg_back_bias` (Back bias) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1062` | kit `number` ↔ `CineRigRimBgBackBias` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:132` `cine_rig_rim_bg_back_bias` |
| `rim_edge` / `cine_rimq_edge` (Edge) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1115` | kit `slider` ↔ `CineRigRimSilhouette` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:138` `cine_rimq_edge` |
| `rim_edge` / `cine_rimq_width` (Width) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1118` | kit `slider` ↔ `CineRigRimSilhouetteWidth` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:143` `cine_rimq_width` |
| `rim_edge` / `cine_rimq_units` (Units) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1119` | kit `combo` ↔ `CineRigRimSilhouetteUnits` `U32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:148` `cine_rimq_units` |
| `rim_edge` / `cine_rimq_soft` (Soft) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1122` | kit `slider` ↔ `CineRigRimSilhouetteSoft` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:156` `cine_rimq_soft` |
| `rim_wrap` / `cine_rimq_wrap_amount` (Amount) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1127` | kit `slider` ↔ `CineRigRimWrapStrength` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:163` `cine_rimq_wrap_amount` |
| `rim_wrap` / `cine_rimq_wrap_reach` (Reach) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1130` | kit `slider` ↔ `CineRigRimWrapWidth` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:168` `cine_rimq_wrap_reach` |
| `rim_wrap` / `cine_rimq_wrap_blur` (Blur) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1133` | kit `slider` ↔ `CineRigRimWrapBlur` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:173` `cine_rimq_wrap_blur` |
| `rim_wrap` / `cine_rimq_wrap_colour` (Colour) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1136` | kit `slider` ↔ `CineRigRimWrapSaturation` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:178` `cine_rimq_wrap_colour` |
| `rim_tron` / `cine_rig_rim_tron_mode` (Mode) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1070` | kit `combo` ↔ `CineRigRimTronMode` `S32`; `ALRigRimKitAdapter` preserves/calls behavior `ALRigRimController::attach()` (`indra/newview/alrigrimcontroller.cpp:1402`) | page `tron_tab`, `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6321` `tr_rigrim_tron_mode_combo`, `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:185` `cine_rig_rim_tron_mode` |
| `rim_tron` / `cine_rig_rim_tron_mix` (Mix) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1081` | kit `slider` ↔ `CineRigRimTronMix` `F32`; `ALRigRimKitAdapter` preserves/calls behavior `ALRigRimController::attach()` (`indra/newview/alrigrimcontroller.cpp:1403`) | page `tron_tab`, `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6327` `tr_rigrim_tron_mix_slider`, `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:194` `cine_rig_rim_tron_mix` |
| `rim_tron` / `cine_rig_rim_tron_color` (Colour) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1086` | kit `combo` ↔ `CineRigRimTronColorSource` `S32`; `ALRigRimKitAdapter` preserves/calls behavior `ALRigRimController::attach()` (`indra/newview/alrigrimcontroller.cpp:1404`) | page `tron_tab`, `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6330` `tr_rigrim_tron_color_combo`, `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:199` `cine_rig_rim_tron_color` |
| `rim_diagnostics` / `cine_rig_rim_debug` (Rim only) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:953` | kit `toggle` ↔ `CineRigRimDebugRimOnly` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:210` `cine_rig_rim_debug` |
| `rim_diagnostics` / `cine_rimq_debug` (Debug view) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1139` | kit `combo` ↔ `CineRigRimDebugView` `U32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:213` `cine_rimq_debug` |
| `light_shape` / `relight_physical` (Physical falloff) | none | kit `toggle` ↔ `RenderLocalLightPhysicalFalloff` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:227` `relight_physical` |
| `light_shape` / `relight_world_size` (World light size (m)) | none | kit `slider` ↔ `RenderLocalLightDefaultRadius` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:230` `relight_world_size` |
| `light_shape` / `relight_size_from_prim` (Size from prim) | none | kit `toggle` ↔ `RenderLocalLightSizeFromPrim` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:235` `relight_size_from_prim` |
| `light_shape` / `relight_highlight_softness` (Highlight softness) | none | kit `slider` ↔ `RenderLocalLightHighlightScale` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:238` `relight_highlight_softness` |
| `light_shape` / `relight_shape_preset` (Size preset) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `light_shape` / `relight_near_floor` (Near floor (m)) | none | kit `number` ↔ `RenderLocalLightNearFloor` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:254` `relight_near_floor` |
| `light_shape` / `relight_max_boost` (Max boost) | none | kit `number` ↔ `RenderLocalLightPhysicalMaxBoost` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:257` `relight_max_boost` |
| `area_soft_shadows` / `area_shadow_thin_objects` (Thin objects) | none | kit `toggle` ↔ `RenderAreaShadowThinObjects` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:262` `area_shadow_thin_objects` |
| `area_soft_shadows` / `area_shadow_world` (World shadows) | none | kit `toggle` ↔ `RenderAreaShadowCubeMaps` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:265` `area_shadow_world` |
| `area_soft_shadows` / `area_shadow_world_lights` (World lights) | none | kit `number` ↔ `RenderAreaShadowCubeLights` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:268` `area_shadow_world_lights` |
| `area_soft_shadows` / `area_shadow_world_status` (World shadows) | none | kit readout ← approved static copy; no runtime writer | none |
| `area_soft_shadows` / `area_shadow_world_picks` (World picks) | none | kit readout ← approved static copy; no runtime writer | none |
| `area_soft_shadows` / `area_shadow_world_list_picks` (List picks) | none | `ALRigRimKitAdapter` command/model binding → wired in `LLFloaterDirector::bindPage()`; behavior `LLPipeline::worldShadowPickChatLines()` (`indra/newview/pipeline.cpp:29334`) | none |
| `area_soft_shadows` / `area_shadow_world_clear_picks` (Clear picks) | none | `ALRigRimKitAdapter` command/model binding → wired in `LLFloaterDirector::bindPage()`; behavior `LLPipeline::clearWorldShadowPicks()` (`indra/newview/pipeline.cpp:24702`) | none |
| `area_soft_shadows` / `area_shadow_world_clear_away_picks` (Clear away picks) | none | `ALRigRimKitAdapter` command/model binding → wired in `LLFloaterDirector::bindPage()`; behavior `LLPipeline::clearAwayWorldShadowPicks()` (`indra/newview/pipeline.cpp:24714`) | none |
| `area_soft_shadows` / `area_shadow_vcam` (Also in VCam feeds) | none | kit `toggle` ↔ `RenderAreaShadowsVCam` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:286` `area_shadow_vcam` |
| `area_soft_shadows` / `area_shadow_sl_status` (SL status) | none | kit readout ← approved static copy; no runtime writer | none |
| `area_soft_shadows` / `area_shadow_coverage` (Coverage) | none | kit readout ← approved static copy; no runtime writer | none |
| `area_soft_shadows` / `area_shadow_strength` (Shadow strength) | none | kit `slider` ↔ `RenderAreaShadowStrength` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:295` `area_shadow_strength` |
| `area_soft_shadows` / `area_shadow_contrast` (Shadow contrast) | none | kit `slider` ↔ `RenderAreaShadowContrast` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:300` `area_shadow_contrast` |
| `area_soft_shadows` / `area_shadow_screen_reach` (Screen-space detail (m)) | none | kit `slider` ↔ `RenderAreaShadowWorldScreenReach` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:305` `area_shadow_screen_reach` |
| `area_soft_shadows` / `area_shadow_world_amount` (World shadow amount) | none | kit `slider` ↔ `RenderAreaShadowWorldAmount` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:310` `area_shadow_world_amount` |
| `area_soft_shadows` / `area_shadow_screen_amount` (Screen-space amount) | none | kit `slider` ↔ `RenderAreaShadowScreenAmount` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:315` `area_shadow_screen_amount` |
| `area_soft_shadows` / `area_shadow_lights` (Shadowed lights) | none | kit `number` ↔ `RenderAreaShadowLights` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:320` `area_shadow_lights` |
| `area_soft_shadows` / `area_shadow_quality` (Quality preset) | none | kit `combo` ↔ `RenderAreaShadowQuality` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:323` `area_shadow_quality` |
| `area_soft_shadows` / `area_shadow_samples` (Rays per light) | none | kit `number` ↔ `RenderAreaShadowSamples` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:334` `area_shadow_samples` |
| `area_soft_shadows` / `area_shadow_steps` (March steps) | none | kit `number` ↔ `RenderAreaShadowSteps` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:337` `area_shadow_steps` |
| `area_soft_shadows` / `area_shadow_distance` (Trace distance (m)) | none | kit `number` ↔ `RenderAreaShadowMaxDist` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:340` `area_shadow_distance` |
| `area_soft_shadows` / `area_shadow_thickness` (Thickness (m)) | none | kit `number` ↔ `RenderAreaShadowThickness` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:343` `area_shadow_thickness` |
| `area_soft_shadows` / `area_shadow_half` (Half resolution) | none | kit `toggle` ↔ `RenderAreaShadowHalfRes` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:346` `area_shadow_half` |
| `area_soft_shadows` / `area_shadow_denoise` (Denoise radius) | none | kit `number` ↔ `RenderAreaShadowDenoise` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:349` `area_shadow_denoise` |
| `area_soft_shadows` / `area_shadow_light_bias` (Light end bias (m)) | none | kit `number` ↔ `RenderAreaShadowLightBias` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:352` `area_shadow_light_bias` |
| `area_soft_shadows` / `area_shadow_world_near` (Near clip (m)) | none | kit `number` ↔ `RenderAreaShadowWorldNear` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:355` `area_shadow_world_near` |
| `area_soft_shadows` / `area_shadow_mapped_contact` (Contact (m)) | none | kit `number` ↔ `RenderAreaShadowMappedContact` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:358` `area_shadow_mapped_contact` |
| `area_soft_shadows` / `area_shadow_debug` (Visibility view) | none | kit `combo` ↔ `RenderAreaShadowDebugView` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:361` `area_shadow_debug` |
| `area_soft_shadows` / `area_shadow_world_debug` (World view) | none | kit `combo` ↔ `RenderAreaShadowWorldDebug` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:376` `area_shadow_world_debug` |
| `area_soft_shadows` / `area_shadow_budget` (Work budget (M)) | none | kit `number` ↔ `RenderAreaShadowBudget` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_rim.xml:386` `area_shadow_budget` |
| `area_soft_shadows` / `area_shadow_sl_help` (SL shadows) | none | kit readout ← approved static copy; no runtime writer | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALRigRimController::attach()` — `indra/newview/alrigrimcontroller.cpp:124, 1402, 1403, 1404` | Move model-facing work into `ALRigRimKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLPipeline::clearAwayWorldShadowPicks()` — `indra/newview/pipeline.cpp:24714` | Move model-facing work into `ALRigRimKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLPipeline::clearWorldShadowPicks()` — `indra/newview/pipeline.cpp:24702` | Move model-facing work into `ALRigRimKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLPipeline::worldShadowPickChatLines()` — `indra/newview/pipeline.cpp:29334` | Move model-facing work into `ALRigRimKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **69 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

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
| `card-per_light_rim-tallest-850` | Only `per_light_rim` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-per_light_rim-tallest-1160` | Only `per_light_rim` is put in its tallest local state at 1160 px; all other cards stay default. |
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
| `card-light_shape-tallest-850` | Only `light_shape` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-light_shape-tallest-1160` | Only `light_shape` is put in its tallest local state at 1160 px; all other cards stay default. |
| `relight_physical-on` | Only `relight_physical` is on; dependent visibility is measured. |
| `relight_size_from_prim-on` | Only `relight_size_from_prim` is on; dependent visibility is measured. |
| `relight_shape_preset-option-0` | Only `relight_shape_preset` selects `Choose size...`; other selectors remain default. |
| `relight_shape_preset-option-1` | Only `relight_shape_preset` selects `Bulb (3 cm)`; other selectors remain default. |
| `relight_shape_preset-option-2` | Only `relight_shape_preset` selects `Practical (12 cm)`; other selectors remain default. |
| `relight_shape_preset-option-3` | Only `relight_shape_preset` selects `Softbox (45 cm)`; other selectors remain default. |
| `relight_shape_preset-option-4` | Only `relight_shape_preset` selects `Window (90 cm)`; other selectors remain default. |
| `card-area_soft_shadows-tallest-850` | Only `area_soft_shadows` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-area_soft_shadows-tallest-1160` | Only `area_soft_shadows` is put in its tallest local state at 1160 px; all other cards stay default. |
| `area_shadow_thin_objects-on` | Only `area_shadow_thin_objects` is on; dependent visibility is measured. |
| `area_shadow_world-on` | Only `area_shadow_world` is on; dependent visibility is measured. |
| `area_shadow_vcam-on` | Only `area_shadow_vcam` is on; dependent visibility is measured. |
| `area_shadow_quality-option-0` | Only `area_shadow_quality` selects `Low`; other selectors remain default. |
| `area_shadow_quality-option-1` | Only `area_shadow_quality` selects `Medium`; other selectors remain default. |
| `area_shadow_quality-option-2` | Only `area_shadow_quality` selects `High`; other selectors remain default. |
| `area_shadow_quality-option-3` | Only `area_shadow_quality` selects `Cinematic`; other selectors remain default. |
| `area_shadow_quality-option-4` | Only `area_shadow_quality` selects `Custom`; other selectors remain default. |
| `area_shadow_half-on` | Only `area_shadow_half` is on; dependent visibility is measured. |
| `area_shadow_debug-option-0` | Only `area_shadow_debug` selects `Off`; other selectors remain default. |
| `area_shadow_debug-option-1` | Only `area_shadow_debug` selects `Light 1`; other selectors remain default. |
| `area_shadow_debug-option-2` | Only `area_shadow_debug` selects `Light 2`; other selectors remain default. |
| `area_shadow_debug-option-3` | Only `area_shadow_debug` selects `Light 3`; other selectors remain default. |
| `area_shadow_debug-option-4` | Only `area_shadow_debug` selects `Light 4`; other selectors remain default. |
| `area_shadow_debug-option-5` | Only `area_shadow_debug` selects `Light 5`; other selectors remain default. |
| `area_shadow_debug-option-6` | Only `area_shadow_debug` selects `Light 6`; other selectors remain default. |
| `area_shadow_debug-option-7` | Only `area_shadow_debug` selects `Light 7`; other selectors remain default. |
| `area_shadow_debug-option-8` | Only `area_shadow_debug` selects `Light 8`; other selectors remain default. |
| `area_shadow_world_debug-option-0` | Only `area_shadow_world_debug` selects `Off`; other selectors remain default. |
| `area_shadow_world_debug-option-1` | Only `area_shadow_world_debug` selects `World visibility only`; other selectors remain default. |
| `area_shadow_world_debug-option-2` | Only `area_shadow_world_debug` selects `Face (grey = face / 5)`; other selectors remain default. |
| `area_shadow_world_debug-option-3` | Only `area_shadow_world_debug` selects `Depth difference`; other selectors remain default. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## Ambiguous same-name candidates (never auto-bound)

None.

## Rejected out-of-scope same-name candidates

90 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

## New widget required

None. Every approved visual kind is either a kit primitive or an existing XUI control. An unresolved binding is not permission to invent a visual class.

## Unresolved and pending-decision items

- `relight_shape_preset` (Size preset): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.

## Risks

- A widget-name collision can select an unrelated XUI root or C++ literal; ownership must be resolved by root, class, and behavior together.
- Extracting model work without rewiring the existing owner creates divergent v1/v2 behavior; the extraction and forwarding call are one reviewed change.
- Duplicate writable settings must obey the rollout homes table; this page emits a link/read-only row when it is not the chosen home.
- Shared edit models require owner tokens, and external presets/cue playback require one global history barrier at start/stop rather than per write.
- Unresolved rows fail generation closed; they are never silently replaced with a guessed widget or command.
