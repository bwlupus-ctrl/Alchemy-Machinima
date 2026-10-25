# Weather — Director Kit page contract

Page key: `weather_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (64/71 setting-bound rows; 8 C++-referenced rows).

## Real owners

- `panel_weather_settings` / `panel` at `indra/newview/skins/default/xui/en/panel_weather_settings.xml:7` (51 matching rows).
- Referencing classes: `LLFloaterDirector` (8), `ALPanelWeatherSettings` (7).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALWeatherKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `weather_main` / `weather_deferred_note` (Requires) | `text` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:11` | kit readout ← approved static copy; no runtime writer | none |
| `weather_main` / `weather_reset_all` (Reset all) | `button` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:260` | `ALWeatherKitAdapter` command/model binding → wired in `ALPanelWeatherSettings::postBuild()`; behavior `ALPanelWeatherSettings::onClickResetAll()` (`indra/newview/alpanelweathersettings.cpp:158`) | none |
| `weather_library` / `weather_library_mode` (Weather controls) | none | kit `combo` ↔ `AlchemyWeatherControlMode` `U32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:18` `weather_library_mode` |
| `weather_library` / `weather_library_local` (Local weather) | none | kit `toggle` ↔ `AlchemyWeatherLocalEnabled` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:26` `weather_library_local` |
| `weather_library` / `weather_library_rainRate` (Rain rate (mm/h)) | none | kit `number` ↔ `AlchemyWeatherRainRate` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:29` `weather_library_rainRate` |
| `weather_library` / `weather_library_bands` (Rain bands) | none | kit `slider` ↔ `AlchemyWeatherPrecipVariation` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:32` `weather_library_bands` |
| `weather_library` / `weather_library_windSource` (Wind source) | none | kit `combo` ↔ `AlchemyWeatherWindSource` `U32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:37` `weather_library_windSource` |
| `weather_library` / `weather_library_windDirection` (From (deg)) | none | kit `number` ↔ `AlchemyWeatherWindDirection` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:45` `weather_library_windDirection` |
| `weather_library` / `weather_library_windSpeed` (Speed (m/s)) | none | kit `number` ↔ `AlchemyWeatherWindSpeed` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:48` `weather_library_windSpeed` |
| `weather_library` / `weather_library_windVariation` (Speed variation) | none | kit `slider` ↔ `AlchemyWeatherWindVariation` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:51` `weather_library_windVariation` |
| `weather_library` / `weather_library_gustiness` (Gustiness) | none | kit `slider` ↔ `AlchemyWeatherWindGustiness` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:56` `weather_library_gustiness` |
| `weather_library` / `weather_library_directionSwing` (Swing (deg)) | none | kit `number` ↔ `AlchemyWeatherWindDirVariation` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:61` `weather_library_directionSwing` |
| `weather_library` / `weather_library_transition` (Transition (s)) | none | kit `number` ↔ `AlchemyWeatherTransitionSeconds` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:64` `weather_library_transition` |
| `weather_library` / `weather_library_seed` (Weather seed) | none | kit `text` ↔ `AlchemyWeatherSeed` `U32`. Contract note: page contract supplies omitted setting `AlchemyWeatherSeed` | none |
| `weather_library` / `weather_library_storm` (Storm amount) | none | kit `slider` ↔ `AlchemyWeatherStorm` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:70` `weather_library_storm` |
| `weather_library` / `weather_library_lightningTiming` (Lightning timing) | none | kit `combo` ↔ `AlchemyWeatherLightningTiming` `U32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:75` `weather_library_lightningTiming` |
| `weather_library` / `weather_library_preset` (Rain preset) | none | `ALWeatherKitAdapter` command/model binding → wired in `LLFloaterDirector::bindWeatherPage()`; behavior `ALPanelWeatherSettings::applyLibraryPreset()` (`indra/newview/alpanelweathersettings.cpp:572`) | none |
| `weather_library` / `weather_library_apply` (Apply preset) | none | `ALWeatherKitAdapter` command/model binding → wired in `LLFloaterDirector::bindWeatherPage()`; behavior `ALPanelWeatherSettings::applyLibraryPreset()` (`indra/newview/alpanelweathersettings.cpp:572`) | none |
| `weather_library` / `weather_library_stop` (Stop rain) | none | `ALWeatherKitAdapter` command/model binding → wired in `LLFloaterDirector::bindWeatherPage()`; behavior `ALPanelWeatherSettings::stopPrecipitation()` (`indra/newview/alpanelweathersettings.cpp:582`) | none |
| `weather_library` / `weather_library_status` (Current weather) | none | kit readout ← approved static copy; no runtime writer | none |
| `rain` / `weather_rain_intensity` (Intensity) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:45` | kit `slider` ↔ `AlchemyWeatherRainIntensity` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:105` `weather_rain_intensity` |
| `rain` / `weather_rain_density` (Density) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:48` | kit `slider` ↔ `AlchemyWeatherRainDensity` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:110` `weather_rain_density` |
| `rain` / `weather_rain_speed` (Fall speed) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:51` | kit `slider` ↔ `AlchemyWeatherRainFallSpeed` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:115` `weather_rain_speed` |
| `rain` / `weather_wind_scale` (Wind response) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:63` | kit `slider` ↔ `AlchemyWeatherWindScale` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:120` `weather_wind_scale` |
| `rain` / `weather_rain_gust` (Gust strength) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:78` | kit `slider` ↔ `AlchemyWeatherRainGustStrength` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:125` `weather_rain_gust` |
| `rain` / `weather_eep_coupling` (Cloud link) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:66` | kit `slider` ↔ `AlchemyWeatherEEPCoupling` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:130` `weather_eep_coupling` |
| `rain` / `weather_rain_distance` (Draw distance) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:54` | kit `slider` ↔ `AlchemyWeatherRainMaxDistance` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:135` `weather_rain_distance` |
| `rain` / `weather_rain_samples` (Ray samples) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:57` | kit `slider` ↔ `AlchemyWeatherRainSamples` `U32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:140` `weather_rain_samples` |
| `rain` / `weather_rain_divisor` (Resolution) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:60` | kit `slider` ↔ `AlchemyWeatherRainResolutionDivisor` `U32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:145` `weather_rain_divisor` |
| `rain` / `weather_rain_layers` (Depth layers) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:69` | kit `slider` ↔ `AlchemyWeatherRainLayers` `U32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:150` `weather_rain_layers` |
| `rain` / `weather_rain_shutter` (Shutter) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:72` | kit `slider` ↔ `AlchemyWeatherRainVirtualShutter` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:155` `weather_rain_shutter` |
| `rain` / `weather_rain_near` (Near emphasis) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:75` | kit `slider` ↔ `AlchemyWeatherRainNearEmphasis` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:160` `weather_rain_near` |
| `rain` / `weather_rain_color` (Rain colour) | none | kit `color` ↔ `AlchemyWeatherRainColor` `Color3` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:165` `weather_rain_color` |
| `splashes` / `weather_splash_density` (Density) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:90` | kit `slider` ↔ `AlchemyWeatherSplashDensity` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:170` `weather_splash_density` |
| `splashes` / `weather_splash_size` (Ring radius) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:93` | kit `slider` ↔ `AlchemyWeatherSplashRingSize` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:175` `weather_splash_size` |
| `splashes` / `weather_splash_range` (Impact range) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:102` | kit `slider` ↔ `AlchemyWeatherSplashMaxDistance` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:180` `weather_splash_range` |
| `splashes` / `weather_splash_lifetime` (Lifetime) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:96` | kit `slider` ↔ `AlchemyWeatherSplashLifetime` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:185` `weather_splash_lifetime` |
| `splashes` / `weather_splash_up` (Up threshold) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:99` | kit `slider` ↔ `AlchemyWeatherSplashUpThreshold` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:190` `weather_splash_up` |
| `wet_mist_lens` / `weather_wetness_enabled` (Wet sheen) | `check_box` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:107` | kit `toggle` ↔ `AlchemyWeatherWetnessEnabled` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:197` `weather_wetness_enabled` |
| `wet_mist_lens` / `weather_wetness_strength` (Wetness) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:110` | kit `slider` ↔ `AlchemyWeatherWetnessStrength` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:200` `weather_wetness_strength` |
| `wet_mist_lens` / `weather_mist_enabled` (Ground mist) | `check_box` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:115` | kit `toggle` ↔ `AlchemyWeatherMistEnabled` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:205` `weather_mist_enabled` |
| `wet_mist_lens` / `weather_mist_strength` (Mist strength) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:118` | kit `slider` ↔ `AlchemyWeatherMistStrength` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:208` `weather_mist_strength` |
| `wet_mist_lens` / `weather_mist_height` (Mist height) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:121` | kit `slider` ↔ `AlchemyWeatherMistHeight` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:213` `weather_mist_height` |
| `wet_mist_lens` / `weather_lens_enabled` (Lens drops) | `check_box` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:126` | kit `toggle` ↔ `AlchemyWeatherLensDropsEnabled` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:218` `weather_lens_enabled` |
| `wet_mist_lens` / `weather_lens_strength` (Lens strength) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:129` | kit `slider` ↔ `AlchemyWeatherLensDropsStrength` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:221` `weather_lens_strength` |
| `rain_cover` / `weather_rain_occlusion_extent` (Map radius) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:148` | kit `slider` ↔ `AlchemyWeatherRainOcclusionExtent` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:228` `weather_rain_occlusion_extent` |
| `rain_cover` / `weather_rain_occlusion_bias` (Vertical bias) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:151` | kit `slider` ↔ `AlchemyWeatherRainOcclusionBias` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:233` `weather_rain_occlusion_bias` |
| `rain_cover` / `weather_rain_occlusion_softness` (Softness) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:154` | kit `slider` ↔ `AlchemyWeatherRainOcclusionSoftness` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:238` `weather_rain_occlusion_softness` |
| `rain_cover` / `weather_shelter_fade` (Fallback cover) | `check_box` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:163` | kit `toggle` ↔ `AlchemyWeatherShelterFade` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:243` `weather_shelter_fade` |
| `rain_cover` / `weather_rain_occlusion_resolution` (Map detail) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:145` | kit `slider` ↔ `AlchemyWeatherRainOcclusionResolution` `U32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:246` `weather_rain_occlusion_resolution` |
| `rain_cover` / `weather_shelter_height` (Cover height) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:167` | kit `slider` ↔ `AlchemyWeatherShelterHeight` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:251` `weather_shelter_height` |
| `lightning` / `weather_lightning_rate` (Strikes/min) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:180` | kit `slider` ↔ `AlchemyWeatherLightningRate` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:258` `weather_lightning_rate` |
| `lightning` / `weather_lightning_brightness` (Brightness) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:192` | kit `slider` ↔ `AlchemyWeatherLightningBrightness` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:263` `weather_lightning_brightness` |
| `lightning` / `weather_flash_duration` (Flash length) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:183` | kit `slider` ↔ `AlchemyWeatherLightningFlashDuration` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:268` `weather_flash_duration` |
| `lightning` / `weather_trigger` (Strike now) | `button` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:207` | `ALWeatherKitAdapter` command/model binding → wired in `ALPanelWeatherSettings::postBuild()`; behavior `ALPanelWeatherSettings::onTriggerStrike()` (`indra/newview/alpanelweathersettings.cpp:156`) | none |
| `lightning` / `weather_bolt_duration` (Bolt time) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:186` | kit `slider` ↔ `AlchemyWeatherLightningBoltDuration` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:276` `weather_bolt_duration` |
| `lightning` / `weather_lightning_ambient` (Ambient floor) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:189` | kit `slider` ↔ `AlchemyWeatherLightningAmbient` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:281` `weather_lightning_ambient` |
| `lightning` / `weather_lightning_width` (Bolt width) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:195` | kit `slider` ↔ `AlchemyWeatherLightningBoltWidth` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:286` `weather_lightning_width` |
| `lightning` / `weather_lightning_height` (Bolt height) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:198` | kit `slider` ↔ `AlchemyWeatherLightningBoltHeight` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:291` `weather_lightning_height` |
| `lightning` / `weather_lightning_min` (Minimum range) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:201` | kit `slider` ↔ `AlchemyWeatherLightningMinDistance` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:296` `weather_lightning_min` |
| `lightning` / `weather_lightning_max` (Maximum range) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:204` | kit `slider` ↔ `AlchemyWeatherLightningMaxDistance` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:301` `weather_lightning_max` |
| `lightning` / `weather_lightning_color` (Lightning colour) | none | kit `color` ↔ `AlchemyWeatherLightningColor` `Color3` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:306` `weather_lightning_color` |
| `lightning_hq` / `weather_lightning_quality_tier` (Quality tier) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:215` | kit `slider` ↔ `AlchemyWeatherLightningQualityTier` `U32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:311` `weather_lightning_quality_tier` |
| `lightning_hq` / `weather_lightning_sheet_enabled` (Cloud sheet) | `check_box` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:219` | kit `toggle` ↔ `AlchemyWeatherLightningSheetEnabled` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:316` `weather_lightning_sheet_enabled` |
| `lightning_hq` / `weather_lightning_sheet_strength` (Sheet strength) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:223` | kit `slider` ↔ `AlchemyWeatherLightningSheetStrength` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:319` `weather_lightning_sheet_strength` |
| `lightning_hq` / `weather_lightning_corona_strength` (Corona) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:226` | kit `slider` ↔ `AlchemyWeatherLightningCoronaStrength` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:324` `weather_lightning_corona_strength` |
| `lightning_hq` / `weather_lightning_afterglow` (Afterglow) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:240` | kit `slider` ↔ `AlchemyWeatherLightningAfterglowStrength` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:329` `weather_lightning_afterglow` |
| `lightning_hq` / `weather_lightning_distance_grading` (Distance grade) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:237` | kit `slider` ↔ `AlchemyWeatherLightningDistanceGrading` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:334` `weather_lightning_distance_grading` |
| `lightning_hq` / `weather_lightning_wet_glint_enabled` (Wet glint) | `check_box` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:230` | kit `toggle` ↔ `AlchemyWeatherLightningWetGlintEnabled` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:339` `weather_lightning_wet_glint_enabled` |
| `lightning_hq` / `weather_lightning_wet_glint_strength` (Glint strength) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:234` | kit `slider` ↔ `AlchemyWeatherLightningWetGlintStrength` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:342` `weather_lightning_wet_glint_strength` |
| `lightning_hq` / `weather_lightning_energy_ceiling` (HDR peak cap) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:243` | kit `slider` ↔ `AlchemyWeatherLightningEnergyCeiling` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_world_weather.xml:347` `weather_lightning_energy_ceiling` |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALPanelWeatherSettings::applyLibraryPreset()` — `indra/newview/alpanelweathersettings.cpp:572` | Move model-facing work into `ALWeatherKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelWeatherSettings::onClickResetAll()` — `indra/newview/alpanelweathersettings.cpp:158` | Move model-facing work into `ALWeatherKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelWeatherSettings::onTriggerStrike()` — `indra/newview/alpanelweathersettings.cpp:156` | Move model-facing work into `ALWeatherKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelWeatherSettings::stopPrecipitation()` — `indra/newview/alpanelweathersettings.cpp:582` | Move model-facing work into `ALWeatherKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **37 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-weather_main-tallest-850` | Only `weather_main` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-weather_main-tallest-1160` | Only `weather_main` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-weather_library-tallest-850` | Only `weather_library` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-weather_library-tallest-1160` | Only `weather_library` is put in its tallest local state at 1160 px; all other cards stay default. |
| `weather_library_mode-option-0` | Only `weather_library_mode` selects `Existing rain`; other selectors remain default. |
| `weather_library_mode-option-1` | Only `weather_library_mode` selects `Weather library`; other selectors remain default. |
| `weather_library_local-on` | Only `weather_library_local` is on; dependent visibility is measured. |
| `weather_library_windSource-option-0` | Only `weather_library_windSource` selects `Simulator wind`; other selectors remain default. |
| `weather_library_windSource-option-1` | Only `weather_library_windSource` selects `Weather wind`; other selectors remain default. |
| `weather_library_lightningTiming-option-0` | Only `weather_library_lightningTiming` selects `Existing`; other selectors remain default. |
| `weather_library_lightningTiming-option-1` | Only `weather_library_lightningTiming` selects `Weather storm`; other selectors remain default. |
| `weather_library_preset-option-0` | Only `weather_library_preset` selects `Drizzle`; other selectors remain default. |
| `weather_library_preset-option-1` | Only `weather_library_preset` selects `Steady rain`; other selectors remain default. |
| `weather_library_preset-option-2` | Only `weather_library_preset` selects `Heavy rain`; other selectors remain default. |
| `weather_library_preset-option-3` | Only `weather_library_preset` selects `Downpour`; other selectors remain default. |
| `weather_library_preset-option-4` | Only `weather_library_preset` selects `Custom`; other selectors remain default. |
| `card-rain-tallest-850` | Only `rain` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-rain-tallest-1160` | Only `rain` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-splashes-tallest-850` | Only `splashes` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-splashes-tallest-1160` | Only `splashes` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-wet_mist_lens-tallest-850` | Only `wet_mist_lens` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-wet_mist_lens-tallest-1160` | Only `wet_mist_lens` is put in its tallest local state at 1160 px; all other cards stay default. |
| `weather_wetness_enabled-on` | Only `weather_wetness_enabled` is on; dependent visibility is measured. |
| `weather_mist_enabled-on` | Only `weather_mist_enabled` is on; dependent visibility is measured. |
| `weather_lens_enabled-on` | Only `weather_lens_enabled` is on; dependent visibility is measured. |
| `card-rain_cover-tallest-850` | Only `rain_cover` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-rain_cover-tallest-1160` | Only `rain_cover` is put in its tallest local state at 1160 px; all other cards stay default. |
| `weather_shelter_fade-on` | Only `weather_shelter_fade` is on; dependent visibility is measured. |
| `card-lightning-tallest-850` | Only `lightning` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-lightning-tallest-1160` | Only `lightning` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-lightning_hq-tallest-850` | Only `lightning_hq` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-lightning_hq-tallest-1160` | Only `lightning_hq` is put in its tallest local state at 1160 px; all other cards stay default. |
| `weather_lightning_sheet_enabled-on` | Only `weather_lightning_sheet_enabled` is on; dependent visibility is measured. |
| `weather_lightning_wet_glint_enabled-on` | Only `weather_lightning_wet_glint_enabled` is on; dependent visibility is measured. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## Ambiguous same-name candidates (never auto-bound)

None.

## Rejected out-of-scope same-name candidates

92 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

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
