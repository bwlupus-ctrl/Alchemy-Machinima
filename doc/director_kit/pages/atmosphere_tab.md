# Atmosphere — Director Kit page contract

Page key: `atmosphere_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (56/84 setting-bound rows; 25 C++-referenced rows).

## Real owners

- `panel_projector_shafts` / `panel` at `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:2` (29 matching rows).
- `panel_atmospheric_volumes` / `panel` at `indra/newview/skins/default/xui/en/panel_atmospheric_volumes.xml:6` (25 matching rows).
- `floater_lightbox_settings` / `floater` at `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:8` (15 matching rows).
- Referencing classes: `ALPanelAtmosphericVolumes` (55), `ALFloaterLightBox` (2).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALAtmosphereKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `haze_main` / `set_CineHazeEnable` (Enable haze) | none | kit `toggle` ↔ `CineHazeEnable` `Boolean`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:10` `set_CineHazeEnable` |
| `haze_main` / `ch_CineHazeStrength_slider` (Strength) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5290` | kit `slider` ↔ `CineHazeStrength` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:13` `ch_CineHazeStrength_slider` |
| `haze_main` / `ch_CineHazeDensity_slider` (Density /m) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5293` | kit `slider` ↔ `CineHazeDensity` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:18` `ch_CineHazeDensity_slider` |
| `haze_main` / `ch_CineHazeStartDistance_slider` (Start m) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5296` | kit `slider` ↔ `CineHazeStartDistance` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:23` `ch_CineHazeStartDistance_slider` |
| `haze_main` / `ch_CineHazeMaxOpacity_slider` (Max opacity) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5299` | kit `slider` ↔ `CineHazeMaxOpacity` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:28` `ch_CineHazeMaxOpacity_slider` |
| `haze_main` / `ch_color_mode_combo` (Colour from) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5303` | kit `combo` ↔ `CineHazeColorMode` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:33` `ch_color_mode_combo` |
| `haze_colour` / `ch_color_swatch` (Manual colour) | `color_swatch` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5308` | kit `color` ↔ `CineHazeColor` `Color3` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:43` `ch_color_swatch` |
| `haze_colour` / `ch_CineHazeColorIntensity_slider` (Intensity) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5311` | kit `slider` ↔ `CineHazeColorIntensity` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:46` `ch_CineHazeColorIntensity_slider` |
| `haze_colour` / `ch_CineHazeSunStrength_slider` (Sun strength) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5327` | kit `slider` ↔ `CineHazeSunStrength` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:51` `ch_CineHazeSunStrength_slider` |
| `haze_colour` / `ch_sun_tint_swatch` (Sun tint) | `color_swatch` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5324` | kit `color` ↔ `CineHazeSunTint` `Color3` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:56` `ch_sun_tint_swatch` |
| `haze_colour` / `ch_CineHazeSunLobeWidth_slider` (Lobe width) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5330` | kit `slider` ↔ `CineHazeSunLobeWidth` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:59` `ch_CineHazeSunLobeWidth_slider` |
| `haze_height` / `set_CineHazeHeightEnable` (Denser low) | none | kit `toggle` ↔ `CineHazeHeightEnable` `Boolean`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:66` `set_CineHazeHeightEnable` |
| `haze_height` / `ch_CineHazeReferenceHeight_slider` (Base height m) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5316` | kit `slider` ↔ `CineHazeReferenceHeight` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:69` `ch_CineHazeReferenceHeight_slider` |
| `haze_height` / `ch_ref_from_camera` (From camera) | `button` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5318` | `ALAtmosphereKitAdapter` command/model binding → wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onCineHazeRefFromCamera()` (`indra/newview/alfloaterlightbox.cpp:103`) | none |
| `haze_height` / `ch_CineHazeHeightFalloff_slider` (Falloff /m) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5320` | kit `slider` ↔ `CineHazeHeightFalloff` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:77` `ch_CineHazeHeightFalloff_slider` |
| `haze_adv` / `ch_debug_combo` (Debug view) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5334` | kit `combo` ↔ `CineHazeDebugView` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:84` `ch_debug_combo` |
| `haze_adv` / `ch_reset_all` (Reset haze) | `button` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5288` | `ALAtmosphereKitAdapter` command/model binding → wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onCineHazePreset()` (`indra/newview/alfloaterlightbox.cpp:102`) | none |
| `haze_adv` / `open_lightbox_haze` (Lightbox ›) | none | kit-native action `floater:lightbox/cinehaze_settings`; no duplicate widget is required | none |
| `projector_cones` / `ps_BDMergeProjectorVolumetricsMultiplier_slider` (Brightness) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:18` | kit `slider` ↔ `BDMergeProjectorVolumetricsMultiplier` `F32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:4487` `projvol_multiplier_slider`, `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:105` `ps_BDMergeProjectorVolumetricsMultiplier_slider` |
| `projector_cones` / `ps_BDMergeProjectorVolumetricsDensity_slider` (Haziness) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:67` | kit `slider` ↔ `BDMergeProjectorVolumetricsDensity` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:110` `ps_BDMergeProjectorVolumetricsDensity_slider` |
| `projector_cones` / `ps_BDMergeProjectorVolumetricsAnisotropy_slider` (Forward glow) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:24` | kit `slider` ↔ `BDMergeProjectorVolumetricsAnisotropy` `F32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:4569` `projvol_anisotropy_slider`, `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:115` `ps_BDMergeProjectorVolumetricsAnisotropy_slider` |
| `projector_cones` / `ps_BDMergeProjectorVolumetricsFeather_slider` (Edge softness) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:53` | kit `slider` ↔ `BDMergeProjectorVolumetricsFeather` `F32` | page `cine_light_rig_tab`, `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:120` `ps_BDMergeProjectorVolumetricsFeather_slider` |
| `projector_cones` / `ps_tint_swatch` (Tint) | `color_swatch` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:60` | kit `color` ↔ `BDMergeProjectorVolumetricsTint` `Color3` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:125` `ps_tint_swatch` |
| `projector_cones` / `ps_BDMergeProjectorVolumetricsTintStrength_slider` (Tint amount) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:63` | kit `slider` ↔ `BDMergeProjectorVolumetricsTintStrength` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:128` `ps_BDMergeProjectorVolumetricsTintStrength_slider` |
| `fog_glow` / `ps_BDMergeProjectorVolumetricsFogStrength_slider` (Ground fog) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:86` | kit `slider` ↔ `BDMergeProjectorVolumetricsFogStrength` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:135` `ps_BDMergeProjectorVolumetricsFogStrength_slider` |
| `fog_glow` / `ps_BDMergeProjectorVolumetricsFogGroundDensity_slider` (Fog density) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:89` | kit `slider` ↔ `BDMergeProjectorVolumetricsFogGroundDensity` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:140` `ps_BDMergeProjectorVolumetricsFogGroundDensity_slider` |
| `fog_glow` / `ps_BDMergeProjectorVolumetricsFogFalloff_slider` (Fog height) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:92` | kit `slider` ↔ `BDMergeProjectorVolumetricsFogFalloff` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:145` `ps_BDMergeProjectorVolumetricsFogFalloff_slider` |
| `fog_glow` / `ps_BDMergeProjectorVolumetricsBloomFeed_slider` (Bloom) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:98` | kit `slider` ↔ `BDMergeProjectorVolumetricsBloomFeed` `F32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5248` `fx_BDMergeProjectorVolumetricsBloomFeed_slider`, `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:150` `ps_BDMergeProjectorVolumetricsBloomFeed_slider` |
| `fog_glow` / `ps_BDMergeProjectorVolumetricsMaxDistance_slider` (Beam length) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:101` | kit `slider` ↔ `BDMergeProjectorVolumetricsMaxDistance` `F32` | page `cine_light_rig_tab`, `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:156` `cine_easy_shaft_length`, `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:59` `cine_easy_shaft_length`, `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:155` `ps_BDMergeProjectorVolumetricsMaxDistance_slider` |
| `fog_glow` / `ps_BDMergeProjectorVolumetricsFogBase_slider` (Fog base) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:95` | kit `slider` ↔ `BDMergeProjectorVolumetricsFogBase` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:160` `ps_BDMergeProjectorVolumetricsFogBase_slider` |
| `dust_wind` / `ps_BDMergeProjectorVolumetricsNoiseStrength_slider` (Dust amount) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:70` | kit `slider` ↔ `BDMergeProjectorVolumetricsNoiseStrength` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:167` `ps_BDMergeProjectorVolumetricsNoiseStrength_slider` |
| `dust_wind` / `ps_BDMergeProjectorVolumetricsNoiseScale_slider` (Dust scale) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:73` | kit `slider` ↔ `BDMergeProjectorVolumetricsNoiseScale` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:172` `ps_BDMergeProjectorVolumetricsNoiseScale_slider` |
| `dust_wind` / `ps_BDMergeProjectorVolumetricsNoiseSpeed_slider` (Dust speed) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:76` | kit `slider` ↔ `BDMergeProjectorVolumetricsNoiseSpeed` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:177` `ps_BDMergeProjectorVolumetricsNoiseSpeed_slider` |
| `dust_wind` / `ps_BDMergeFroxelWindAzimuth_slider` (Wind bearing) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:79` | kit `dial` ↔ `BDMergeFroxelWindAzimuth` `F32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5221` `fx_BDMergeFroxelWindAzimuth_slider`, `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:182` `ps_BDMergeFroxelWindAzimuth_slider` |
| `dust_wind` / `ps_BDMergeFroxelWindElevation_slider` (Wind tilt) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:82` | kit `arc` ↔ `BDMergeFroxelWindElevation` `F32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5224` `fx_BDMergeFroxelWindElevation_slider`, `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:185` `ps_BDMergeFroxelWindElevation_slider` |
| `dust_wind` / `ps_BDMergeFroxelWindInverted_check` (Invert wind) | `check_box` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:84` | kit `toggle` ↔ `BDMergeFroxelWindInverted` `Boolean` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5226` `fx_BDMergeFroxelWindInverted_check`, `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:188` `ps_BDMergeFroxelWindInverted_check` |
| `soft_shadows` / `ps_BDMergeSoftShadowSoftness_slider` (Softness) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:106` | kit `slider` ↔ `BDMergeSoftShadowSoftness` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:193` `ps_BDMergeSoftShadowSoftness_slider` |
| `soft_shadows` / `ps_BDMergeSoftShadowMaxPenumbra_slider` (Max blur) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:109` | kit `slider` ↔ `BDMergeSoftShadowMaxPenumbra` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:198` `ps_BDMergeSoftShadowMaxPenumbra_slider` |
| `soft_shadows` / `ps_BDMergeSoftShadowFill_slider` (Shadow fill) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:112` | kit `slider` ↔ `BDMergeSoftShadowFill` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:203` `ps_BDMergeSoftShadowFill_slider` |
| `soft_shadows` / `soft_shadow_sun_check` (Sun too) | none | kit `toggle` ↔ `BDMergeSoftShadowSun` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:208` `soft_shadow_sun_check` |
| `soft_shadows` / `ps_BDMergeProjectorVolumetricsShadowTint_slider` (Shadow tint) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:56` | kit `slider` ↔ `BDMergeProjectorVolumetricsShadowTint` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:211` `ps_BDMergeProjectorVolumetricsShadowTint_slider` |
| `soft_shadows` / `ps_BDMergeProjectorVolumetricsShadowSamples_slider` (Shadow taps) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:50` | kit `slider` ↔ `BDMergeProjectorVolumetricsShadowSamples` `U32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:216` `ps_BDMergeProjectorVolumetricsShadowSamples_slider` |
| `soft_shadows` / `gobo_anisotropic_check` (Gobo smoothing) | none | kit `toggle` ↔ `BDMergeGoboAnisotropic` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:221` `gobo_anisotropic_check` |
| `local_fog_volumes` / `localfog_volume_list` (Volumes) | `scroll_list` `indra/newview/skins/default/xui/en/panel_atmospheric_volumes.xml:37` | `ALAtmosphereKitAdapter` command/model binding → wired in `ALPanelAtmosphericVolumes::postBuild()`; behavior `ALPanelAtmosphericVolumes::onSelectionChanged()` (`indra/newview/alpanelatmosphericvolumes.cpp:70`) | none |
| `local_fog_volumes` / `localfog_add` (+ Add) | `button` `indra/newview/skins/default/xui/en/panel_atmospheric_volumes.xml:44` | `ALAtmosphereKitAdapter` command/model binding → wired in `ALPanelAtmosphericVolumes::postBuild()`; behavior `ALPanelAtmosphericVolumes::onAdd()` (`indra/newview/alpanelatmosphericvolumes.cpp:72`) | none |
| `local_fog_volumes` / `localfog_duplicate` (Duplicate) | `button` `indra/newview/skins/default/xui/en/panel_atmospheric_volumes.xml:45` | `ALAtmosphereKitAdapter` command/model binding → wired in `ALPanelAtmosphericVolumes::postBuild()`; behavior `ALPanelAtmosphericVolumes::onDuplicate()` (`indra/newview/alpanelatmosphericvolumes.cpp:74`) | none |
| `local_fog_volumes` / `localfog_delete` (Delete) | `button` `indra/newview/skins/default/xui/en/panel_atmospheric_volumes.xml:46` | `ALAtmosphereKitAdapter` command/model binding → wired in `ALPanelAtmosphericVolumes::postBuild()`; behavior `ALPanelAtmosphericVolumes::onDelete()` (`indra/newview/alpanelatmosphericvolumes.cpp:76`) | none |
| `local_fog_volumes` / `localfog_enabled` (Enabled) | `check_box` `indra/newview/skins/default/xui/en/panel_atmospheric_volumes.xml:47` | `ALAtmosphereKitAdapter` command/model binding → wired in `ALPanelAtmosphericVolumes::postBuild()`; behavior `ALPanelAtmosphericVolumes::onEditorCommit()` (`indra/newview/alpanelatmosphericvolumes.cpp:251`) | none |
| `local_fog_volumes` / `localfog_froxel_requirement` (Requires) | `text` `indra/newview/skins/default/xui/en/panel_atmospheric_volumes.xml:24` | kit readout ← approved static copy; no runtime writer | none |
| `local_fog_volumes` / `localfog_label` (Name) | `line_editor` `indra/newview/skins/default/xui/en/panel_atmospheric_volumes.xml:49` | `ALAtmosphereKitAdapter` command/model binding → wired in `ALPanelAtmosphericVolumes::postBuild()`; behavior `ALPanelAtmosphericVolumes::onEditorCommit()` (`indra/newview/alpanelatmosphericvolumes.cpp:251`) | none |
| `fog_position` / `localfog_shape` (Shape) | `combo_box` `indra/newview/skins/default/xui/en/panel_atmospheric_volumes.xml:65` | `ALAtmosphereKitAdapter` command/model binding → wired in `ALPanelAtmosphericVolumes::postBuild()`; behavior `ALPanelAtmosphericVolumes::onEditorCommit()` (`indra/newview/alpanelatmosphericvolumes.cpp:251`) | none |
| `fog_position` / `localfog_move_camera` (Move to camera) | `button` `indra/newview/skins/default/xui/en/panel_atmospheric_volumes.xml:69` | `ALAtmosphereKitAdapter` command/model binding → wired in `ALPanelAtmosphericVolumes::postBuild()`; behavior `ALPanelAtmosphericVolumes::onMoveToCamera()` (`indra/newview/alpanelatmosphericvolumes.cpp:78`) | none |
| `fog_position` / `localfog_center_x` (Center X) | `spinner` `indra/newview/skins/default/xui/en/panel_atmospheric_volumes.xml:53` | `ALAtmosphereKitAdapter` command/model binding → wired in `ALPanelAtmosphericVolumes::postBuild()`; behavior `ALPanelAtmosphericVolumes::onEditorCommit()` (`indra/newview/alpanelatmosphericvolumes.cpp:251`) | none |
| `fog_position` / `localfog_center_y` (Center Y) | `spinner` `indra/newview/skins/default/xui/en/panel_atmospheric_volumes.xml:54` | `ALAtmosphereKitAdapter` command/model binding → wired in `ALPanelAtmosphericVolumes::postBuild()`; behavior `ALPanelAtmosphericVolumes::onEditorCommit()` (`indra/newview/alpanelatmosphericvolumes.cpp:251`) | none |
| `fog_position` / `localfog_center_z` (Center Z) | `spinner` `indra/newview/skins/default/xui/en/panel_atmospheric_volumes.xml:55` | `ALAtmosphereKitAdapter` command/model binding → wired in `ALPanelAtmosphericVolumes::postBuild()`; behavior `ALPanelAtmosphericVolumes::onEditorCommit()` (`indra/newview/alpanelatmosphericvolumes.cpp:251`) | none |
| `fog_position` / `localfog_hint` (Note) | `text` `indra/newview/skins/default/xui/en/panel_atmospheric_volumes.xml:84` | kit readout ← approved static copy; no runtime writer | none |
| `fog_size_turn` / `localfog_size_x` (Radius X) | `spinner` `indra/newview/skins/default/xui/en/panel_atmospheric_volumes.xml:57` | `ALAtmosphereKitAdapter` command/model binding → wired in `ALPanelAtmosphericVolumes::postBuild()`; behavior `ALPanelAtmosphericVolumes::onEditorCommit()` (`indra/newview/alpanelatmosphericvolumes.cpp:251`) | none |
| `fog_size_turn` / `localfog_size_y` (Radius Y) | `spinner` `indra/newview/skins/default/xui/en/panel_atmospheric_volumes.xml:58` | `ALAtmosphereKitAdapter` command/model binding → wired in `ALPanelAtmosphericVolumes::postBuild()`; behavior `ALPanelAtmosphericVolumes::onEditorCommit()` (`indra/newview/alpanelatmosphericvolumes.cpp:251`) | none |
| `fog_size_turn` / `localfog_size_z` (Radius Z) | `spinner` `indra/newview/skins/default/xui/en/panel_atmospheric_volumes.xml:59` | `ALAtmosphereKitAdapter` command/model binding → wired in `ALPanelAtmosphericVolumes::postBuild()`; behavior `ALPanelAtmosphericVolumes::onEditorCommit()` (`indra/newview/alpanelatmosphericvolumes.cpp:251`) | none |
| `fog_size_turn` / `localfog_rotation_x` (Turn X) | `spinner` `indra/newview/skins/default/xui/en/panel_atmospheric_volumes.xml:61` | `ALAtmosphereKitAdapter` command/model binding → wired in `ALPanelAtmosphericVolumes::postBuild()`; behavior `ALPanelAtmosphericVolumes::onEditorCommit()` (`indra/newview/alpanelatmosphericvolumes.cpp:251`) | none |
| `fog_size_turn` / `localfog_rotation_y` (Turn Y) | `spinner` `indra/newview/skins/default/xui/en/panel_atmospheric_volumes.xml:62` | `ALAtmosphereKitAdapter` command/model binding → wired in `ALPanelAtmosphericVolumes::postBuild()`; behavior `ALPanelAtmosphericVolumes::onEditorCommit()` (`indra/newview/alpanelatmosphericvolumes.cpp:251`) | none |
| `fog_size_turn` / `localfog_rotation_z` (Turn Z) | `spinner` `indra/newview/skins/default/xui/en/panel_atmospheric_volumes.xml:63` | `ALAtmosphereKitAdapter` command/model binding → wired in `ALPanelAtmosphericVolumes::postBuild()`; behavior `ALPanelAtmosphericVolumes::onEditorCommit()` (`indra/newview/alpanelatmosphericvolumes.cpp:251`) | none |
| `fog_look` / `localfog_density` (Density) | `slider` `indra/newview/skins/default/xui/en/panel_atmospheric_volumes.xml:73` | `ALAtmosphereKitAdapter` command/model binding → wired in `ALPanelAtmosphericVolumes::postBuild()`; behavior `ALPanelAtmosphericVolumes::onEditorCommit()` (`indra/newview/alpanelatmosphericvolumes.cpp:251`) | none |
| `fog_look` / `localfog_feather` (Edge softness) | `slider` `indra/newview/skins/default/xui/en/panel_atmospheric_volumes.xml:75` | `ALAtmosphereKitAdapter` command/model binding → wired in `ALPanelAtmosphericVolumes::postBuild()`; behavior `ALPanelAtmosphericVolumes::onEditorCommit()` (`indra/newview/alpanelatmosphericvolumes.cpp:251`) | none |
| `fog_look` / `localfog_height_falloff` (Height falloff) | `slider` `indra/newview/skins/default/xui/en/panel_atmospheric_volumes.xml:77` | `ALAtmosphereKitAdapter` command/model binding → wired in `ALPanelAtmosphericVolumes::postBuild()`; behavior `ALPanelAtmosphericVolumes::onEditorCommit()` (`indra/newview/alpanelatmosphericvolumes.cpp:251`) | none |
| `fog_look` / `localfog_noise_scale` (Noise scale) | `slider` `indra/newview/skins/default/xui/en/panel_atmospheric_volumes.xml:79` | `ALAtmosphereKitAdapter` command/model binding → wired in `ALPanelAtmosphericVolumes::postBuild()`; behavior `ALPanelAtmosphericVolumes::onEditorCommit()` (`indra/newview/alpanelatmosphericvolumes.cpp:251`) | none |
| `fog_look` / `localfog_noise_speed` (Noise speed) | `slider` `indra/newview/skins/default/xui/en/panel_atmospheric_volumes.xml:81` | `ALAtmosphereKitAdapter` command/model binding → wired in `ALPanelAtmosphericVolumes::postBuild()`; behavior `ALPanelAtmosphericVolumes::onEditorCommit()` (`indra/newview/alpanelatmosphericvolumes.cpp:251`) | none |
| `fog_look` / `localfog_tint` (Tint) | `color_swatch` `indra/newview/skins/default/xui/en/panel_atmospheric_volumes.xml:83` | `ALAtmosphereKitAdapter` command/model binding → wired in `ALPanelAtmosphericVolumes::postBuild()`; behavior `ALPanelAtmosphericVolumes::onEditorCommit()` (`indra/newview/alpanelatmosphericvolumes.cpp:251`) | none |
| `shaft_quality` / `ps_BDMergeProjectorVolumetricsResolution_slider` (Samples) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:21` | kit `slider` ↔ `BDMergeProjectorVolumetricsResolution` `U32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:4528` `projvol_resolution_slider`, `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:239` `ps_BDMergeProjectorVolumetricsResolution_slider` |
| `shaft_quality` / `projector_volumetrics_half_res_check` (Half-res) | none | kit `toggle` ↔ `BDMergeProjectorVolumetricsHalfRes` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:244` `projector_volumetrics_half_res_check` |
| `shaft_quality` / `projector_volumetrics_temporal_check` (Smoothing) | none | kit `toggle` ↔ `BDMergeProjectorVolumetricsTemporal` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:247` `projector_volumetrics_temporal_check` |
| `shaft_quality` / `ps_BDMergeProjectorVolumetricsTemporalBlend_slider` (History) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:30` | kit `slider` ↔ `BDMergeProjectorVolumetricsTemporalBlend` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:250` `ps_BDMergeProjectorVolumetricsTemporalBlend_slider` |
| `shaft_quality` / `projector_volumetrics_adaptive_check` (Adaptive) | none | kit `toggle` ↔ `BDMergeProjectorVolumetricsAdaptive` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:255` `projector_volumetrics_adaptive_check` |
| `shaft_quality` / `ps_BDMergeProjectorVolumetricsTemporalReject_slider` (Anti-ghost) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:33` | kit `slider` ↔ `BDMergeProjectorVolumetricsTemporalReject` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:258` `ps_BDMergeProjectorVolumetricsTemporalReject_slider` |
| `shaft_quality` / `projector_volumetrics_temporal_beam_depth_check` (Track beam) | none | kit `toggle` ↔ `BDMergeProjectorVolumetricsTemporalBeamDepth` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:263` `projector_volumetrics_temporal_beam_depth_check` |
| `shaft_quality` / `projector_volumetrics_scissor_check` (Scissor cull) | none | kit `toggle` ↔ `BDMergeProjectorVolumetricsScissor` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:266` `projector_volumetrics_scissor_check` |
| `shaft_quality` / `projector_volumetrics_pyramid_scissor_check` (Pyramid cull) | none | kit `toggle` ↔ `BDMergeProjectorVolumetricsPyramidScissor` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:269` `projector_volumetrics_pyramid_scissor_check` |
| `shaft_quality` / `ps_BDMergeProjectorVolumetricsMinResolution_slider` (Min samples) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:39` | kit `slider` ↔ `BDMergeProjectorVolumetricsMinResolution` `U32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:272` `ps_BDMergeProjectorVolumetricsMinResolution_slider` |
| `shaft_quality` / `ps_BDMergeProjectorVolumetricsMaxLuminance_slider` (Peak limit) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:42` | kit `slider` ↔ `BDMergeProjectorVolumetricsMaxLuminance` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:277` `ps_BDMergeProjectorVolumetricsMaxLuminance_slider` |
| `shaft_quality` / `ps_BDMergeProjectorVolumetricsDither_slider` (Dither) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:45` | kit `slider` ↔ `BDMergeProjectorVolumetricsDither` `U32` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:282` `ps_BDMergeProjectorVolumetricsDither_slider` |
| `shaft_quality` / `projector_volumetrics_temporal_dither_check` (Anti-banding) | none | kit `toggle` ↔ `BDMergeProjectorVolumetricsTemporalDither` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:287` `projector_volumetrics_temporal_dither_check` |
| `shaft_quality` / `projector_volumetrics_equiangular_check` (Equiangular steps) | none | kit `toggle` ↔ `BDMergeProjectorVolumetricsEquiangular` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:290` `projector_volumetrics_equiangular_check` |
| `shaft_quality` / `projector_volumetrics_airborne_tap_check` (Airborne tap) | none | kit `toggle` ↔ `BDMergeProjectorVolumetricsAirborneTap` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:293` `projector_volumetrics_airborne_tap_check` |
| `shaft_quality` / `projector_volumetrics_cookie_footprint_check` (Cookie footprint) | none | kit `toggle` ↔ `BDMergeProjectorVolumetricsCookieFootprint` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_world_atmosphere.xml:296` `projector_volumetrics_cookie_footprint_check` |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALFloaterLightBox::onCineHazePreset()` — `indra/newview/alfloaterlightbox.cpp:102` | Move model-facing work into `ALAtmosphereKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterLightBox::onCineHazeRefFromCamera()` — `indra/newview/alfloaterlightbox.cpp:103` | Move model-facing work into `ALAtmosphereKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelAtmosphericVolumes::onAdd()` — `indra/newview/alpanelatmosphericvolumes.cpp:72` | Move model-facing work into `ALAtmosphereKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelAtmosphericVolumes::onDelete()` — `indra/newview/alpanelatmosphericvolumes.cpp:76` | Move model-facing work into `ALAtmosphereKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelAtmosphericVolumes::onDuplicate()` — `indra/newview/alpanelatmosphericvolumes.cpp:74` | Move model-facing work into `ALAtmosphereKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelAtmosphericVolumes::onEditorCommit()` — `indra/newview/alpanelatmosphericvolumes.cpp:251` | Move model-facing work into `ALAtmosphereKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelAtmosphericVolumes::onMoveToCamera()` — `indra/newview/alpanelatmosphericvolumes.cpp:78` | Move model-facing work into `ALAtmosphereKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelAtmosphericVolumes::onSelectionChanged()` — `indra/newview/alpanelatmosphericvolumes.cpp:70` | Move model-facing work into `ALAtmosphereKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **62 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-haze_main-tallest-850` | Only `haze_main` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-haze_main-tallest-1160` | Only `haze_main` is put in its tallest local state at 1160 px; all other cards stay default. |
| `set_CineHazeEnable-on` | Only `set_CineHazeEnable` is on; dependent visibility is measured. |
| `ch_color_mode_combo-option-0` | Only `ch_color_mode_combo` selects `Sky-linked (WindLight)`; other selectors remain default. |
| `ch_color_mode_combo-option-1` | Only `ch_color_mode_combo` selects `Manual colour`; other selectors remain default. |
| `card-haze_colour-tallest-850` | Only `haze_colour` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-haze_colour-tallest-1160` | Only `haze_colour` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-haze_height-tallest-850` | Only `haze_height` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-haze_height-tallest-1160` | Only `haze_height` is put in its tallest local state at 1160 px; all other cards stay default. |
| `set_CineHazeHeightEnable-on` | Only `set_CineHazeHeightEnable` is on; dependent visibility is measured. |
| `card-haze_adv-tallest-850` | Only `haze_adv` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-haze_adv-tallest-1160` | Only `haze_adv` is put in its tallest local state at 1160 px; all other cards stay default. |
| `ch_debug_combo-option-0` | Only `ch_debug_combo` selects `Off`; other selectors remain default. |
| `ch_debug_combo-option-1` | Only `ch_debug_combo` selects `Distance (0-512 m)`; other selectors remain default. |
| `ch_debug_combo-option-2` | Only `ch_debug_combo` selects `Density at surface / base`; other selectors remain default. |
| `ch_debug_combo-option-3` | Only `ch_debug_combo` selects `Optical depth`; other selectors remain default. |
| `ch_debug_combo-option-4` | Only `ch_debug_combo` selects `Transmittance`; other selectors remain default. |
| `ch_debug_combo-option-5` | Only `ch_debug_combo` selects `Fog amount`; other selectors remain default. |
| `ch_debug_combo-option-6` | Only `ch_debug_combo` selects `Sky exclusion`; other selectors remain default. |
| `card-projector_cones-tallest-850` | Only `projector_cones` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-projector_cones-tallest-1160` | Only `projector_cones` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-fog_glow-tallest-850` | Only `fog_glow` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-fog_glow-tallest-1160` | Only `fog_glow` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-dust_wind-tallest-850` | Only `dust_wind` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-dust_wind-tallest-1160` | Only `dust_wind` is put in its tallest local state at 1160 px; all other cards stay default. |
| `ps_BDMergeFroxelWindInverted_check-on` | Only `ps_BDMergeFroxelWindInverted_check` is on; dependent visibility is measured. |
| `card-soft_shadows-tallest-850` | Only `soft_shadows` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-soft_shadows-tallest-1160` | Only `soft_shadows` is put in its tallest local state at 1160 px; all other cards stay default. |
| `soft_shadow_sun_check-on` | Only `soft_shadow_sun_check` is on; dependent visibility is measured. |
| `gobo_anisotropic_check-on` | Only `gobo_anisotropic_check` is on; dependent visibility is measured. |
| `card-local_fog_volumes-tallest-850` | Only `local_fog_volumes` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-local_fog_volumes-tallest-1160` | Only `local_fog_volumes` is put in its tallest local state at 1160 px; all other cards stay default. |
| `localfog_volume_list-empty` | `localfog_volume_list` is empty with no selection. |
| `localfog_volume_list-one` | `localfog_volume_list` has one selected item. |
| `localfog_volume_list-overflow` | `localfog_volume_list` has 51 items and a visible scrollbar. |
| `localfog_enabled-on` | Only `localfog_enabled` is on; dependent visibility is measured. |
| `card-fog_position-tallest-850` | Only `fog_position` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-fog_position-tallest-1160` | Only `fog_position` is put in its tallest local state at 1160 px; all other cards stay default. |
| `localfog_shape-option-0` | Only `localfog_shape` selects `Box`; other selectors remain default. |
| `localfog_shape-option-1` | Only `localfog_shape` selects `Ellipsoid`; other selectors remain default. |
| `card-fog_size_turn-tallest-850` | Only `fog_size_turn` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-fog_size_turn-tallest-1160` | Only `fog_size_turn` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-fog_look-tallest-850` | Only `fog_look` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-fog_look-tallest-1160` | Only `fog_look` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-shaft_quality-tallest-850` | Only `shaft_quality` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-shaft_quality-tallest-1160` | Only `shaft_quality` is put in its tallest local state at 1160 px; all other cards stay default. |
| `projector_volumetrics_half_res_check-on` | Only `projector_volumetrics_half_res_check` is on; dependent visibility is measured. |
| `projector_volumetrics_temporal_check-on` | Only `projector_volumetrics_temporal_check` is on; dependent visibility is measured. |
| `projector_volumetrics_adaptive_check-on` | Only `projector_volumetrics_adaptive_check` is on; dependent visibility is measured. |
| `projector_volumetrics_temporal_beam_depth_check-on` | Only `projector_volumetrics_temporal_beam_depth_check` is on; dependent visibility is measured. |
| `projector_volumetrics_scissor_check-on` | Only `projector_volumetrics_scissor_check` is on; dependent visibility is measured. |
| `projector_volumetrics_pyramid_scissor_check-on` | Only `projector_volumetrics_pyramid_scissor_check` is on; dependent visibility is measured. |
| `projector_volumetrics_temporal_dither_check-on` | Only `projector_volumetrics_temporal_dither_check` is on; dependent visibility is measured. |
| `projector_volumetrics_equiangular_check-on` | Only `projector_volumetrics_equiangular_check` is on; dependent visibility is measured. |
| `projector_volumetrics_airborne_tap_check-on` | Only `projector_volumetrics_airborne_tap_check` is on; dependent visibility is measured. |
| `projector_volumetrics_cookie_footprint_check-on` | Only `projector_volumetrics_cookie_footprint_check` is on; dependent visibility is measured. |
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

67 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

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
