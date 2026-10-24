# Projector Shafts — Director Kit page contract

Page key: `projector_shafts_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **pure-setting** (51/51 setting-bound rows; 0 C++-referenced rows).

## Real owners

- `panel_projector_shafts` / `panel` at `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:2` (29 matching rows).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALProjectorShaftsKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `shaft_look` / `ps_BDMergeProjectorVolumetricsMultiplier_slider` (Brightness) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:23` | kit `slider` ↔ `BDMergeProjectorVolumetricsMultiplier` `F32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:4487` `projvol_multiplier_slider`, `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:11` `ps_BDMergeProjectorVolumetricsMultiplier_slider` |
| `shaft_look` / `ps_BDMergeProjectorVolumetricsDensity_slider` (Haziness) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:72` | kit `slider` ↔ `BDMergeProjectorVolumetricsDensity` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:16` `ps_BDMergeProjectorVolumetricsDensity_slider` |
| `shaft_look` / `ps_BDMergeProjectorVolumetricsAnisotropy_slider` (Forward glow) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:29` | kit `slider` ↔ `BDMergeProjectorVolumetricsAnisotropy` `F32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:4569` `projvol_anisotropy_slider`, `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:21` `ps_BDMergeProjectorVolumetricsAnisotropy_slider` |
| `shaft_look` / `ps_tint_swatch` (Tint) | `color_swatch` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:65` | kit `color` ↔ `BDMergeProjectorVolumetricsTint` `Color3` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:26` `ps_tint_swatch` |
| `shaft_look` / `ps_BDMergeProjectorVolumetricsTintStrength_slider` (Tint amount) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:68` | kit `slider` ↔ `BDMergeProjectorVolumetricsTintStrength` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:29` `ps_BDMergeProjectorVolumetricsTintStrength_slider` |
| `shaft_look` / `ps_BDMergeProjectorVolumetricsBloomFeed_slider` (Glow halo) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:103` | kit `slider` ↔ `BDMergeProjectorVolumetricsBloomFeed` `F32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5248` `fx_BDMergeProjectorVolumetricsBloomFeed_slider`, `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:34` `ps_BDMergeProjectorVolumetricsBloomFeed_slider` |
| `shaft_look` / `ps_BDMergeProjectorVolumetricsBloomFeedAnamorphic_slider` (Halo stretch) | none | kit `slider` ↔ `BDMergeProjectorVolumetricsBloomFeedAnamorphic` `F32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5251` `fx_BDMergeProjectorVolumetricsBloomFeedAnamorphic_slider`, `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:39` `ps_BDMergeProjectorVolumetricsBloomFeedAnamorphic_slider` |
| `shaft_look` / `ps_BDMergeProjectorVolumetricsMaxLuminance_slider` (Peak limit) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:47` | kit `slider` ↔ `BDMergeProjectorVolumetricsMaxLuminance` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:44` `ps_BDMergeProjectorVolumetricsMaxLuminance_slider` |
| `shaft_beam` / `ps_BDMergeProjectorVolumetricsMaxDistance_slider` (Beam length) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:106` | kit `slider` ↔ `BDMergeProjectorVolumetricsMaxDistance` `F32` | page `cine_light_rig_tab`, `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:156` `cine_easy_shaft_length`, `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:59` `cine_easy_shaft_length`, `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:51` `ps_BDMergeProjectorVolumetricsMaxDistance_slider` |
| `shaft_beam` / `ps_BDMergeProjectorVolumetricsFeather_slider` (Edge softness) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:58` | kit `slider` ↔ `BDMergeProjectorVolumetricsFeather` `F32` | page `cine_light_rig_tab`, `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:56` `ps_BDMergeProjectorVolumetricsFeather_slider` |
| `shaft_beam` / `ps_BDMergeProjectorVolumetricsRimStrength_slider` (Rim glow) | none | kit `slider` ↔ `BDMergeProjectorVolumetricsRimStrength` `F32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5233` `fx_BDMergeProjectorVolumetricsRimStrength_slider`, `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:61` `ps_BDMergeProjectorVolumetricsRimStrength_slider` |
| `shaft_beam` / `ps_BDMergeProjectorVolumetricsRimPower_slider` (Rim tightness) | none | kit `slider` ↔ `BDMergeProjectorVolumetricsRimPower` `F32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5236` `fx_BDMergeProjectorVolumetricsRimPower_slider`, `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:66` `ps_BDMergeProjectorVolumetricsRimPower_slider` |
| `shaft_beam` / `ps_BDMergeProjectorVolumetricsRimWrap_slider` (Rim wrap) | none | kit `slider` ↔ `BDMergeProjectorVolumetricsRimWrap` `F32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5239` `fx_BDMergeProjectorVolumetricsRimWrap_slider`, `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:71` `ps_BDMergeProjectorVolumetricsRimWrap_slider` |
| `shaft_beam` / `ps_BDMergeProjectorVolumetricsRimSoftness_slider` (Rim skin softness) | none | kit `slider` ↔ `BDMergeProjectorVolumetricsRimSoftness` `F32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5242` `fx_BDMergeProjectorVolumetricsRimSoftness_slider`, `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:76` `ps_BDMergeProjectorVolumetricsRimSoftness_slider` |
| `shaft_beam` / `ps_BDMergeProjectorVolumetricsRimThreshold_slider` (Rim threshold) | none | kit `slider` ↔ `BDMergeProjectorVolumetricsRimThreshold` `F32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5245` `fx_BDMergeProjectorVolumetricsRimThreshold_slider`, `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:81` `ps_BDMergeProjectorVolumetricsRimThreshold_slider` |
| `shaft_dust` / `ps_BDMergeProjectorVolumetricsNoiseStrength_slider` (Dust amount) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:75` | kit `slider` ↔ `BDMergeProjectorVolumetricsNoiseStrength` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:88` `ps_BDMergeProjectorVolumetricsNoiseStrength_slider` |
| `shaft_dust` / `ps_BDMergeProjectorVolumetricsNoiseScale_slider` (Dust scale) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:78` | kit `slider` ↔ `BDMergeProjectorVolumetricsNoiseScale` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:93` `ps_BDMergeProjectorVolumetricsNoiseScale_slider` |
| `shaft_dust` / `ps_BDMergeProjectorVolumetricsNoiseSpeed_slider` (Dust speed) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:81` | kit `slider` ↔ `BDMergeProjectorVolumetricsNoiseSpeed` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:98` `ps_BDMergeProjectorVolumetricsNoiseSpeed_slider` |
| `shaft_dust` / `ps_BDMergeProjectorVolumetricsDust_check` (Particles) | none | kit `toggle` ↔ `BDMergeProjectorVolumetricsDust` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:103` `ps_BDMergeProjectorVolumetricsDust_check` |
| `shaft_dust` / `ps_BDMergeProjectorVolumetricsDustIntensity_slider` (Particle amount) | none | kit `slider` ↔ `BDMergeProjectorVolumetricsDustIntensity` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:106` `ps_BDMergeProjectorVolumetricsDustIntensity_slider` |
| `shaft_dust` / `ps_BDMergeProjectorVolumetricsDustScale_slider` (Particle size) | none | kit `slider` ↔ `BDMergeProjectorVolumetricsDustScale` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:111` `ps_BDMergeProjectorVolumetricsDustScale_slider` |
| `shaft_dust` / `ps_BDMergeProjectorVolumetricsDustDrift_slider` (Particle drift) | none | kit `slider` ↔ `BDMergeProjectorVolumetricsDustDrift` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:116` `ps_BDMergeProjectorVolumetricsDustDrift_slider` |
| `shaft_fog` / `ps_BDMergeProjectorVolumetricsFogStrength_slider` (Ground fog) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:91` | kit `slider` ↔ `BDMergeProjectorVolumetricsFogStrength` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:123` `ps_BDMergeProjectorVolumetricsFogStrength_slider` |
| `shaft_fog` / `ps_BDMergeProjectorVolumetricsFogGroundDensity_slider` (Fog density) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:94` | kit `slider` ↔ `BDMergeProjectorVolumetricsFogGroundDensity` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:128` `ps_BDMergeProjectorVolumetricsFogGroundDensity_slider` |
| `shaft_fog` / `ps_BDMergeProjectorVolumetricsFogFalloff_slider` (Fog height) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:97` | kit `slider` ↔ `BDMergeProjectorVolumetricsFogFalloff` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:133` `ps_BDMergeProjectorVolumetricsFogFalloff_slider` |
| `shaft_fog` / `ps_BDMergeProjectorVolumetricsFogBase_slider` (Fog base) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:100` | kit `slider` ↔ `BDMergeProjectorVolumetricsFogBase` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:138` `ps_BDMergeProjectorVolumetricsFogBase_slider` |
| `shaft_wind` / `ps_BDMergeFroxelWindAzimuth_slider` (Wind bearing) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:84` | kit `dial` ↔ `BDMergeFroxelWindAzimuth` `F32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5221` `fx_BDMergeFroxelWindAzimuth_slider`, `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:145` `ps_BDMergeFroxelWindAzimuth_slider` |
| `shaft_wind` / `ps_BDMergeFroxelWindElevation_slider` (Wind tilt) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:87` | kit `arc` ↔ `BDMergeFroxelWindElevation` `F32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5224` `fx_BDMergeFroxelWindElevation_slider`, `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:148` `ps_BDMergeFroxelWindElevation_slider` |
| `shaft_wind` / `ps_BDMergeFroxelWindInverted_check` (Invert wind) | `check_box` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:89` | kit `toggle` ↔ `BDMergeFroxelWindInverted` `Boolean` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5226` `fx_BDMergeFroxelWindInverted_check`, `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:151` `ps_BDMergeFroxelWindInverted_check` |
| `shaft_shadows` / `ps_BDMergeProjectorVolumetricsShadowSamples_slider` (Shadow taps) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:55` | kit `slider` ↔ `BDMergeProjectorVolumetricsShadowSamples` `U32` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:156` `ps_BDMergeProjectorVolumetricsShadowSamples_slider` |
| `shaft_shadows` / `ps_BDMergeProjectorVolumetricsShadowTint_slider` (Shadow tint) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:61` | kit `slider` ↔ `BDMergeProjectorVolumetricsShadowTint` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:161` `ps_BDMergeProjectorVolumetricsShadowTint_slider` |
| `soft_shadows` / `ps_BDMergeSoftShadowSoftness_slider` (Softness) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:111` | kit `slider` ↔ `BDMergeSoftShadowSoftness` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:168` `ps_BDMergeSoftShadowSoftness_slider` |
| `soft_shadows` / `ps_BDMergeSoftShadowMaxPenumbra_slider` (Max blur) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:114` | kit `slider` ↔ `BDMergeSoftShadowMaxPenumbra` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:173` `ps_BDMergeSoftShadowMaxPenumbra_slider` |
| `soft_shadows` / `ps_BDMergeSoftShadowFill_slider` (Shadow fill) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:117` | kit `slider` ↔ `BDMergeSoftShadowFill` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:178` `ps_BDMergeSoftShadowFill_slider` |
| `soft_shadows` / `soft_shadow_sun_check` (Sun too) | none | kit `toggle` ↔ `BDMergeSoftShadowSun` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:183` `soft_shadow_sun_check` |
| `shaft_quality` / `ps_BDMergeProjectorVolumetricsResolution_slider` (Samples) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:26` | kit `slider` ↔ `BDMergeProjectorVolumetricsResolution` `U32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:4528` `projvol_resolution_slider`, `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:188` `ps_BDMergeProjectorVolumetricsResolution_slider` |
| `shaft_quality` / `projector_volumetrics_half_res_check` (Half-res) | none | kit `toggle` ↔ `BDMergeProjectorVolumetricsHalfRes` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:193` `projector_volumetrics_half_res_check` |
| `shaft_quality` / `projector_volumetrics_temporal_check` (Smoothing) | none | kit `toggle` ↔ `BDMergeProjectorVolumetricsTemporal` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:196` `projector_volumetrics_temporal_check` |
| `shaft_quality` / `ps_BDMergeProjectorVolumetricsTemporalBlend_slider` (History) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:35` | kit `slider` ↔ `BDMergeProjectorVolumetricsTemporalBlend` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:199` `ps_BDMergeProjectorVolumetricsTemporalBlend_slider` |
| `shaft_quality` / `projector_volumetrics_adaptive_check` (Adaptive) | none | kit `toggle` ↔ `BDMergeProjectorVolumetricsAdaptive` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:204` `projector_volumetrics_adaptive_check` |
| `shaft_quality` / `ps_BDMergeProjectorVolumetricsTemporalReject_slider` (Anti-ghost) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:38` | kit `slider` ↔ `BDMergeProjectorVolumetricsTemporalReject` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:207` `ps_BDMergeProjectorVolumetricsTemporalReject_slider` |
| `shaft_quality` / `projector_volumetrics_temporal_beam_depth_check` (Track beam) | none | kit `toggle` ↔ `BDMergeProjectorVolumetricsTemporalBeamDepth` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:212` `projector_volumetrics_temporal_beam_depth_check` |
| `shaft_quality` / `projector_volumetrics_scissor_check` (Scissor cull) | none | kit `toggle` ↔ `BDMergeProjectorVolumetricsScissor` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:215` `projector_volumetrics_scissor_check` |
| `shaft_quality` / `ps_BDMergeProjectorVolumetricsMinResolution_slider` (Min samples) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:44` | kit `slider` ↔ `BDMergeProjectorVolumetricsMinResolution` `U32` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:218` `ps_BDMergeProjectorVolumetricsMinResolution_slider` |
| `shaft_quality` / `ps_BDMergeProjectorVolumetricsDither_slider` (Dither) | `slider` `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:50` | kit `slider` ↔ `BDMergeProjectorVolumetricsDither` `U32` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:223` `ps_BDMergeProjectorVolumetricsDither_slider` |
| `shaft_quality` / `projector_volumetrics_temporal_dither_check` (Anti-banding) | none | kit `toggle` ↔ `BDMergeProjectorVolumetricsTemporalDither` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:228` `projector_volumetrics_temporal_dither_check` |
| `shaft_quality` / `gobo_anisotropic_check` (Gobo smoothing) | none | kit `toggle` ↔ `BDMergeGoboAnisotropic` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:231` `gobo_anisotropic_check` |
| `shaft_ab` / `projector_volumetrics_equiangular_check` (Equiangular) | none | kit `toggle` ↔ `BDMergeProjectorVolumetricsEquiangular` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:236` `projector_volumetrics_equiangular_check` |
| `shaft_ab` / `projector_volumetrics_airborne_tap_check` (Airborne tap) | none | kit `toggle` ↔ `BDMergeProjectorVolumetricsAirborneTap` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:239` `projector_volumetrics_airborne_tap_check` |
| `shaft_ab` / `projector_volumetrics_cookie_footprint_check` (Gobo footprint) | none | kit `toggle` ↔ `BDMergeProjectorVolumetricsCookieFootprint` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:242` `projector_volumetrics_cookie_footprint_check` |
| `shaft_ab` / `projector_volumetrics_pyramid_scissor_check` (Pyramid scissor) | none | kit `toggle` ↔ `BDMergeProjectorVolumetricsPyramidScissor` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:245` `projector_volumetrics_pyramid_scissor_check` |

## Adapter extraction

No approved widget has a C++ literal reference. `ALProjectorShaftsKitAdapter` owns only explicit actions and setting signals; it must not synthesize a polling mirror.

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **35 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-shaft_look-tallest-850` | Only `shaft_look` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-shaft_look-tallest-1160` | Only `shaft_look` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-shaft_beam-tallest-850` | Only `shaft_beam` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-shaft_beam-tallest-1160` | Only `shaft_beam` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-shaft_dust-tallest-850` | Only `shaft_dust` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-shaft_dust-tallest-1160` | Only `shaft_dust` is put in its tallest local state at 1160 px; all other cards stay default. |
| `ps_BDMergeProjectorVolumetricsDust_check-on` | Only `ps_BDMergeProjectorVolumetricsDust_check` is on; dependent visibility is measured. |
| `card-shaft_fog-tallest-850` | Only `shaft_fog` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-shaft_fog-tallest-1160` | Only `shaft_fog` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-shaft_wind-tallest-850` | Only `shaft_wind` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-shaft_wind-tallest-1160` | Only `shaft_wind` is put in its tallest local state at 1160 px; all other cards stay default. |
| `ps_BDMergeFroxelWindInverted_check-on` | Only `ps_BDMergeFroxelWindInverted_check` is on; dependent visibility is measured. |
| `card-shaft_shadows-tallest-850` | Only `shaft_shadows` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-shaft_shadows-tallest-1160` | Only `shaft_shadows` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-soft_shadows-tallest-850` | Only `soft_shadows` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-soft_shadows-tallest-1160` | Only `soft_shadows` is put in its tallest local state at 1160 px; all other cards stay default. |
| `soft_shadow_sun_check-on` | Only `soft_shadow_sun_check` is on; dependent visibility is measured. |
| `card-shaft_quality-tallest-850` | Only `shaft_quality` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-shaft_quality-tallest-1160` | Only `shaft_quality` is put in its tallest local state at 1160 px; all other cards stay default. |
| `projector_volumetrics_half_res_check-on` | Only `projector_volumetrics_half_res_check` is on; dependent visibility is measured. |
| `projector_volumetrics_temporal_check-on` | Only `projector_volumetrics_temporal_check` is on; dependent visibility is measured. |
| `projector_volumetrics_adaptive_check-on` | Only `projector_volumetrics_adaptive_check` is on; dependent visibility is measured. |
| `projector_volumetrics_temporal_beam_depth_check-on` | Only `projector_volumetrics_temporal_beam_depth_check` is on; dependent visibility is measured. |
| `projector_volumetrics_scissor_check-on` | Only `projector_volumetrics_scissor_check` is on; dependent visibility is measured. |
| `projector_volumetrics_temporal_dither_check-on` | Only `projector_volumetrics_temporal_dither_check` is on; dependent visibility is measured. |
| `gobo_anisotropic_check-on` | Only `gobo_anisotropic_check` is on; dependent visibility is measured. |
| `card-shaft_ab-tallest-850` | Only `shaft_ab` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-shaft_ab-tallest-1160` | Only `shaft_ab` is put in its tallest local state at 1160 px; all other cards stay default. |
| `projector_volumetrics_equiangular_check-on` | Only `projector_volumetrics_equiangular_check` is on; dependent visibility is measured. |
| `projector_volumetrics_airborne_tap_check-on` | Only `projector_volumetrics_airborne_tap_check` is on; dependent visibility is measured. |
| `projector_volumetrics_cookie_footprint_check-on` | Only `projector_volumetrics_cookie_footprint_check` is on; dependent visibility is measured. |
| `projector_volumetrics_pyramid_scissor_check-on` | Only `projector_volumetrics_pyramid_scissor_check` is on; dependent visibility is measured. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## Ambiguous same-name candidates (never auto-bound)

None.

## Rejected out-of-scope same-name candidates

51 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

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
