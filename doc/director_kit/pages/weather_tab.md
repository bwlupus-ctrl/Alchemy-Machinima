# Weather — Director Kit page contract

Page key: `weather_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (48/51 setting-bound rows; 3 C++-referenced rows).

## Real owners

- `panel_weather_settings` / `panel` at `indra/newview/skins/default/xui/en/panel_weather_settings.xml:7` (51 matching rows).
- Referencing classes: `ALPanelWeatherSettings` (4).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALWeatherKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `weather_main` / `weather_deferred_note` (Requires) | `text` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:11` | kit readout ← approved static copy; no runtime writer | none |
| `weather_main` / `weather_reset_all` (Reset all) | `button` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:260` | `ALWeatherKitAdapter` command/model binding → wired in `ALPanelWeatherSettings::postBuild()`; behavior `ALPanelWeatherSettings::onClickResetAll()` (`indra/newview/alpanelweathersettings.cpp:137`) | none |
| `rain` / `weather_rain_intensity` (Intensity) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:45` | kit `slider` ↔ `AlchemyWeatherRainIntensity` `F32` | none |
| `rain` / `weather_rain_density` (Density) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:48` | kit `slider` ↔ `AlchemyWeatherRainDensity` `F32` | none |
| `rain` / `weather_rain_speed` (Fall speed) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:51` | kit `slider` ↔ `AlchemyWeatherRainFallSpeed` `F32` | none |
| `rain` / `weather_wind_scale` (Wind response) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:63` | kit `slider` ↔ `AlchemyWeatherWindScale` `F32` | none |
| `rain` / `weather_rain_gust` (Gust strength) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:78` | kit `slider` ↔ `AlchemyWeatherRainGustStrength` `F32` | none |
| `rain` / `weather_eep_coupling` (Cloud link) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:66` | kit `slider` ↔ `AlchemyWeatherEEPCoupling` `F32` | none |
| `rain` / `weather_rain_distance` (Draw distance) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:54` | kit `slider` ↔ `AlchemyWeatherRainMaxDistance` `F32` | none |
| `rain` / `weather_rain_samples` (Ray samples) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:57` | kit `slider` ↔ `AlchemyWeatherRainSamples` `U32` | none |
| `rain` / `weather_rain_divisor` (Resolution) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:60` | kit `slider` ↔ `AlchemyWeatherRainResolutionDivisor` `U32` | none |
| `rain` / `weather_rain_layers` (Depth layers) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:69` | kit `slider` ↔ `AlchemyWeatherRainLayers` `U32` | none |
| `rain` / `weather_rain_shutter` (Shutter) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:72` | kit `slider` ↔ `AlchemyWeatherRainVirtualShutter` `F32` | none |
| `rain` / `weather_rain_near` (Near emphasis) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:75` | kit `slider` ↔ `AlchemyWeatherRainNearEmphasis` `F32` | none |
| `splashes` / `weather_splash_density` (Density) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:90` | kit `slider` ↔ `AlchemyWeatherSplashDensity` `F32` | none |
| `splashes` / `weather_splash_size` (Ring radius) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:93` | kit `slider` ↔ `AlchemyWeatherSplashRingSize` `F32` | none |
| `splashes` / `weather_splash_range` (Impact range) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:102` | kit `slider` ↔ `AlchemyWeatherSplashMaxDistance` `F32` | none |
| `splashes` / `weather_splash_lifetime` (Lifetime) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:96` | kit `slider` ↔ `AlchemyWeatherSplashLifetime` `F32` | none |
| `splashes` / `weather_splash_up` (Up threshold) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:99` | kit `slider` ↔ `AlchemyWeatherSplashUpThreshold` `F32` | none |
| `wet_mist_lens` / `weather_wetness_enabled` (Wet sheen) | `check_box` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:107` | kit `toggle` ↔ `AlchemyWeatherWetnessEnabled` `Boolean` | none |
| `wet_mist_lens` / `weather_wetness_strength` (Wetness) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:110` | kit `slider` ↔ `AlchemyWeatherWetnessStrength` `F32` | none |
| `wet_mist_lens` / `weather_mist_enabled` (Ground mist) | `check_box` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:115` | kit `toggle` ↔ `AlchemyWeatherMistEnabled` `Boolean` | none |
| `wet_mist_lens` / `weather_mist_strength` (Mist strength) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:118` | kit `slider` ↔ `AlchemyWeatherMistStrength` `F32` | none |
| `wet_mist_lens` / `weather_mist_height` (Mist height) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:121` | kit `slider` ↔ `AlchemyWeatherMistHeight` `F32` | none |
| `wet_mist_lens` / `weather_lens_enabled` (Lens drops) | `check_box` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:126` | kit `toggle` ↔ `AlchemyWeatherLensDropsEnabled` `Boolean` | none |
| `wet_mist_lens` / `weather_lens_strength` (Lens strength) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:129` | kit `slider` ↔ `AlchemyWeatherLensDropsStrength` `F32` | none |
| `rain_cover` / `weather_rain_occlusion_extent` (Map radius) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:148` | kit `slider` ↔ `AlchemyWeatherRainOcclusionExtent` `F32` | none |
| `rain_cover` / `weather_rain_occlusion_bias` (Vertical bias) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:151` | kit `slider` ↔ `AlchemyWeatherRainOcclusionBias` `F32` | none |
| `rain_cover` / `weather_rain_occlusion_softness` (Softness) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:154` | kit `slider` ↔ `AlchemyWeatherRainOcclusionSoftness` `F32` | none |
| `rain_cover` / `weather_shelter_fade` (Fallback cover) | `check_box` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:163` | kit `toggle` ↔ `AlchemyWeatherShelterFade` `Boolean` | none |
| `rain_cover` / `weather_rain_occlusion_resolution` (Map detail) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:145` | kit `slider` ↔ `AlchemyWeatherRainOcclusionResolution` `U32` | none |
| `rain_cover` / `weather_shelter_height` (Cover height) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:167` | kit `slider` ↔ `AlchemyWeatherShelterHeight` `F32` | none |
| `lightning` / `weather_lightning_rate` (Strikes/min) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:180` | kit `slider` ↔ `AlchemyWeatherLightningRate` `F32` | none |
| `lightning` / `weather_lightning_brightness` (Brightness) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:192` | kit `slider` ↔ `AlchemyWeatherLightningBrightness` `F32` | none |
| `lightning` / `weather_flash_duration` (Flash length) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:183` | kit `slider` ↔ `AlchemyWeatherLightningFlashDuration` `F32` | none |
| `lightning` / `weather_trigger` (Strike now) | `button` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:207` | `ALWeatherKitAdapter` command/model binding → wired in `ALPanelWeatherSettings::postBuild()`; behavior `ALPanelWeatherSettings::onTriggerStrike()` (`indra/newview/alpanelweathersettings.cpp:135`) | none |
| `lightning` / `weather_bolt_duration` (Bolt time) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:186` | kit `slider` ↔ `AlchemyWeatherLightningBoltDuration` `F32` | none |
| `lightning` / `weather_lightning_ambient` (Ambient floor) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:189` | kit `slider` ↔ `AlchemyWeatherLightningAmbient` `F32` | none |
| `lightning` / `weather_lightning_width` (Bolt width) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:195` | kit `slider` ↔ `AlchemyWeatherLightningBoltWidth` `F32` | none |
| `lightning` / `weather_lightning_height` (Bolt height) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:198` | kit `slider` ↔ `AlchemyWeatherLightningBoltHeight` `F32` | none |
| `lightning` / `weather_lightning_min` (Minimum range) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:201` | kit `slider` ↔ `AlchemyWeatherLightningMinDistance` `F32` | none |
| `lightning` / `weather_lightning_max` (Maximum range) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:204` | kit `slider` ↔ `AlchemyWeatherLightningMaxDistance` `F32` | none |
| `lightning_hq` / `weather_lightning_quality_tier` (Quality tier) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:215` | kit `slider` ↔ `AlchemyWeatherLightningQualityTier` `U32` | none |
| `lightning_hq` / `weather_lightning_sheet_enabled` (Cloud sheet) | `check_box` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:219` | kit `toggle` ↔ `AlchemyWeatherLightningSheetEnabled` `Boolean` | none |
| `lightning_hq` / `weather_lightning_sheet_strength` (Sheet strength) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:223` | kit `slider` ↔ `AlchemyWeatherLightningSheetStrength` `F32` | none |
| `lightning_hq` / `weather_lightning_corona_strength` (Corona) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:226` | kit `slider` ↔ `AlchemyWeatherLightningCoronaStrength` `F32` | none |
| `lightning_hq` / `weather_lightning_afterglow` (Afterglow) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:240` | kit `slider` ↔ `AlchemyWeatherLightningAfterglowStrength` `F32` | none |
| `lightning_hq` / `weather_lightning_distance_grading` (Distance grade) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:237` | kit `slider` ↔ `AlchemyWeatherLightningDistanceGrading` `F32` | none |
| `lightning_hq` / `weather_lightning_wet_glint_enabled` (Wet glint) | `check_box` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:230` | kit `toggle` ↔ `AlchemyWeatherLightningWetGlintEnabled` `Boolean` | none |
| `lightning_hq` / `weather_lightning_wet_glint_strength` (Glint strength) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:234` | kit `slider` ↔ `AlchemyWeatherLightningWetGlintStrength` `F32` | none |
| `lightning_hq` / `weather_lightning_energy_ceiling` (HDR peak cap) | `slider` `indra/newview/skins/default/xui/en/panel_weather_settings.xml:243` | kit `slider` ↔ `AlchemyWeatherLightningEnergyCeiling` `F32` | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALPanelWeatherSettings::onClickResetAll()` — `indra/newview/alpanelweathersettings.cpp:137` | Move model-facing work into `ALWeatherKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelWeatherSettings::onTriggerStrike()` — `indra/newview/alpanelweathersettings.cpp:135` | Move model-facing work into `ALWeatherKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **23 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-weather_main-tallest-850` | Only `weather_main` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-weather_main-tallest-1160` | Only `weather_main` is put in its tallest local state at 1160 px; all other cards stay default. |
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

19 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

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
