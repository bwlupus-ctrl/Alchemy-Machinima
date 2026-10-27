# Exposure — Director Kit page contract

Page key: `exposure_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (41/49 setting-bound rows; 11 C++-referenced rows).

## Real owners

- `exposure_tab_logic` / `panel` at `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:3` (49 matching rows).
- `panel_exposure_hub` / `panel` at `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:2` (25 matching rows).
- `floater_director` / `floater` at `indra/newview/skins/default/xui/en/floater_director.xml:15` (1 matching rows).
- Referencing classes: `ALPanelExposurePage` (12), `ALPanelExposureHub` (4), `ALExposureSettingsHistory` (2), `ALFloaterLightBox` (2), `ALKitBindingRegistry` (2), `LLButton` (1), `LLFloaterReg` (1), `LLFloaterDirector` (1).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use `panel_exposure_hub.xml` controls as kit rows; no curve editor exists.

## Row bindings

`ALExposureKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `scene_light` / `exp_sun_ev` (Sun / Moon EV) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:13`, `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:34` | kit `slider` ↔ `AlchemyEnvSunEV` `F32` | `indra/newview/skins/default/xui/en/floater_adjust_environment.xml:629` `env_sun_ev`, `indra/newview/skins/default/xui/en/panel_exposure_src.xml:28` `exp_sun_ev` |
| `scene_light` / `exp_gi_ev` (Sky light) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:18`, `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:39` | kit `slider` ↔ `AlchemyEnvSkyGIEV` `F32` | `indra/newview/skins/default/xui/en/floater_adjust_environment.xml:673` `env_gi_ev`, `indra/newview/skins/default/xui/en/panel_exposure_src.xml:31` `exp_gi_ev` |
| `scene_light` / `exp_local_ev` (Local lights) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:23`, `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:47` | kit `slider` ↔ `AlchemyEnvLocalLightEV` `F32` | `indra/newview/skins/default/xui/en/floater_adjust_environment.xml:894` `env_local_ev`, `indra/newview/skins/default/xui/en/panel_exposure_src.xml:35` `exp_local_ev` |
| `scene_light` / `exp_local_size` (Local light size) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:28`, `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:68` | kit `slider` ↔ `AlchemyEnvLocalLightSize` `F32` | `indra/newview/skins/default/xui/en/floater_adjust_environment.xml:1317` `env_local_size`, `indra/newview/skins/default/xui/en/panel_exposure_src.xml:36` `exp_local_size` |
| `scene_light` / `exp_shadow_lift` (Shadow lift) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:33`, `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:49` | kit `slider` ↔ `AlchemyEnvShadowLiftEV` `F32` | `indra/newview/skins/default/xui/en/floater_adjust_environment.xml:1074` `env_shadow_lift`, `indra/newview/skins/default/xui/en/panel_exposure_src.xml:37` `exp_shadow_lift` |
| `scene_light` / `exp_moon_linked` (Link moon) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:38`, `check_box` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:36` | kit `toggle` ↔ `AlchemyEnvMoonLinked` `Boolean` | `indra/newview/skins/default/xui/en/floater_adjust_environment.xml:825` `env_moon_linked`, `indra/newview/skins/default/xui/en/panel_exposure_src.xml:29` `exp_moon_linked` |
| `scene_light` / `exp_moon_ev` (Moon EV) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:41`, `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:37` | kit `slider` ↔ `AlchemyEnvMoonEV` `F32` | `indra/newview/skins/default/xui/en/floater_adjust_environment.xml:839` `env_moon_ev`, `indra/newview/skins/default/xui/en/panel_exposure_src.xml:30` `exp_moon_ev` |
| `scene_light` / `exp_amb_ev` (Sky ambient) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:46`, `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:41` | kit `slider` ↔ `AlchemyEnvGIAmbientEV` `F32` | `indra/newview/skins/default/xui/en/floater_adjust_environment.xml:1029` `env_amb_ev`, `indra/newview/skins/default/xui/en/panel_exposure_src.xml:32` `exp_amb_ev` |
| `scene_light` / `exp_probe_diff_ev` (Probe diffuse) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:51`, `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:43` | kit `slider` ↔ `AlchemyEnvGIProbeDiffuseEV` `F32` | `indra/newview/skins/default/xui/en/floater_adjust_environment.xml:939` `env_probe_diff_ev`, `indra/newview/skins/default/xui/en/panel_exposure_src.xml:33` `exp_probe_diff_ev` |
| `scene_light` / `exp_probe_spec_ev` (Probe reflect) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:56`, `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:45` | kit `slider` ↔ `AlchemyEnvGIProbeSpecEV` `F32` | `indra/newview/skins/default/xui/en/floater_adjust_environment.xml:984` `env_probe_spec_ev`, `indra/newview/skins/default/xui/en/panel_exposure_src.xml:34` `exp_probe_spec_ev` |
| `scene_light` / `exp_local_include_rig` (Rig lights) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:61`, `check_box` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:51` | kit `toggle` ↔ `AlchemyEnvLocalLightIncludeRig` `Boolean` | `indra/newview/skins/default/xui/en/floater_adjust_environment.xml:880` `env_local_include_rig`, `indra/newview/skins/default/xui/en/panel_exposure_src.xml:38` `exp_local_include_rig` |
| `sky_settings` / `exp_sky_note` (Per-sky values) | `text` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:66`, `text` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:64` | kit readout ← approved static copy; no runtime writer | none |
| `sky_settings` / `exp_sky_lighting_btn` (Per-sky look) | `button` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:69`, `button` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:65` | `ALExposureKitAdapter` command/model binding → wired in `LLUI::LLUI()`; behavior `LLButton::setFloaterToggle()` (`indra/llui/llui.cpp:178`) | none |
| `exposure_main` / `exp_auto_exposure` (Auto exposure) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:77`, `check_box` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:8` | kit `toggle` ↔ `RenderDynamicExposureEnabled` `Boolean` | `indra/newview/skins/default/xui/en/floater_adjust_environment.xml:589` `env_auto_exposure`, `indra/newview/skins/default/xui/en/panel_exposure_src.xml:11` `exp_auto_exposure` |
| `exposure_main` / `exp_exposure` (Exposure) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:80`, `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:9` | kit `slider` ↔ `RenderExposure` `F32` | page `image_tab`, `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:418` `exposure_slider`, `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:433` `exposure_spinner`, `indra/newview/skins/default/xui/en/floater_preferences_graphics_advanced.xml:1045` `RenderExposure`, `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:29` `exposure_slider`, `indra/newview/skins/default/xui/en/panel_exposure_src.xml:12` `exp_exposure`, `indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml:378` `RenderExposure` |
| `exposure_main` / `exp_tonemap_type` (Tone mapper) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:85`, `combo_box` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:13` | kit `combo` ↔ `AlchemyRenderTonemapType` `S32` | page `image_tab`, `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:314` `tonemap_type_combo`, `indra/newview/skins/default/xui/en/floater_preferences_graphics_advanced.xml:1186` `TonemapType`, `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:14` `tonemap_type_combo`, `indra/newview/skins/default/xui/en/panel_exposure_src.xml:13` `exp_tonemap_type` |
| `exposure_main` / `exp_tonemap_mix` (Tone strength) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:100`, `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:24` | kit `slider` ↔ `RenderTonemapMix` `F32` | `indra/newview/skins/default/xui/en/floater_preferences_graphics_advanced.xml:1239` `TonemapMix`, `indra/newview/skins/default/xui/en/panel_exposure_src.xml:24` `exp_tonemap_mix` |
| `exposure_main` / `exp_amd_exposure` (AMD exposure) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:105`, `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:26` | kit `slider` ↔ `AlchemyToneMapAMDExposure` `F32` | page `image_tab`, `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:635` `lpm_toneexp_slider`, `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:649` `lpm_toneexp_spinner`, `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:37` `lpm_toneexp_slider`, `indra/newview/skins/default/xui/en/panel_exposure_src.xml:25` `exp_amd_exposure` |
| `exposure_main` / `exp_slog_ev` (S-Log EV) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:110`, `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:28` | kit `slider` ↔ `AlchemyToneMapSLogEV` `F32` | page `image_tab`, `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:897` `slog_ev_slider`, `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:911` `slog_ev_spinner`, `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:42` `slog_ev_slider`, `indra/newview/skins/default/xui/en/panel_exposure_src.xml:26` `exp_slog_ev` |
| `exposure_main` / `exp_legacy_gamma_skies` (Legacy gamma) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:115`, `check_box` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:30` | kit `toggle` ↔ `AlchemyTonemapLegacyGammaSkies` `Boolean` | page `image_tab`, `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:379` `tonemap_legacy_gamma_skies`, `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:34` `tonemap_legacy_gamma_skies`, `indra/newview/skins/default/xui/en/panel_exposure_src.xml:27` `exp_legacy_gamma_skies` |
| `exposure_hub` / `exp_undo` (Undo) | `button` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:121`, `button` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:3` | `ALExposureKitAdapter` command/model binding → wired in `ALPanelExposureHub::postBuild()`; behavior `ALExposureSettingsHistory::undo()` (`indra/newview/alpanelexposurehub.cpp:137`), wired in `ALPanelExposurePage::postBuild()`; behavior `ALPanelExposurePage::<inline callback>()` (`indra/newview/alpanelexposurepage.cpp:209`) | none |
| `exposure_hub` / `exp_redo` (Redo) | `button` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:124`, `button` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:4` | `ALExposureKitAdapter` command/model binding → wired in `ALPanelExposureHub::postBuild()`; behavior `ALExposureSettingsHistory::redo()` (`indra/newview/alpanelexposurehub.cpp:138`), wired in `ALPanelExposurePage::postBuild()`; behavior `ALPanelExposurePage::<inline callback>()` (`indra/newview/alpanelexposurepage.cpp:211`) | none |
| `exposure_hub` / `exposure_popout` (Open window) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:1401`, `button` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:127` | `ALExposureKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterReg::showInstance()` (`indra/newview/llfloaterdirector.cpp:1641`) | none |
| `glow_filters` / `exp_bloom_strength` (Bloom strength) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:133`, `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:73` | kit `slider` ↔ `RenderBloomStrength` `F32`; declared SHORTCUT of `bloom_glow_tab/set_RenderBloomStrength` (home remains canonical for search/undo) | page `bloom_glow_tab`, `indra/newview/skins/default/xui/en/panel_dpage_look_bloom_glow.xml:13` `set_RenderBloomStrength`, `indra/newview/skins/default/xui/en/panel_exposure_src.xml:43` `exp_bloom_strength` |
| `glow_filters` / `exp_bloom_threshold` (Threshold) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:138`, `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:75` | kit `slider` ↔ `RenderBloomThreshold` `F32`; declared SHORTCUT of `bloom_glow_tab/set_RenderBloomThreshold` (home remains canonical for search/undo) | page `bloom_glow_tab`, `indra/newview/skins/default/xui/en/panel_dpage_look_bloom_glow.xml:18` `set_RenderBloomThreshold`, `indra/newview/skins/default/xui/en/panel_exposure_src.xml:44` `exp_bloom_threshold` |
| `glow_filters` / `exp_filters_btn` (All filters) | `button` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:143`, `button` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:136` | `ALExposureKitAdapter` command/model binding → wired in `ALPanelExposureHub::postBuild()`; behavior `ALFloaterLightBox::showTab()` (`indra/newview/alpanelexposurehub.cpp:132`), wired in `ALPanelExposurePage::postBuild()`; behavior `ALFloaterLightBox::showTab()` (`indra/newview/alpanelexposurepage.cpp:207`) | none |
| `sensor` / `sensor_enabled` (Simulate sensor) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:148` | kit `toggle` ↔ `CineSensorEnabled` `Boolean` | none |
| `sensor` / `sensor_iso` (ISO) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:151` | kit `combo` ↔ `CineSensorISO` `F32`; `ALExposureKitAdapter` preserves/calls wired in `ALPanelExposurePage::postBuild()`; behavior `ALKitBindingRegistry::add()` (`indra/newview/alpanelexposurepage.cpp:229`), wired in `ALPanelExposurePage::postBuild()`; behavior `ALPanelExposurePage::alKitSetTypedControl()` (`indra/newview/alpanelexposurepage.cpp:231`) | none |
| `sensor` / `sensor_preset` (Camera preset) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:185` | kit `combo` ↔ `CineSensorPreset` `U32` | none |
| `sensor` / `sensor_brightness` (Brightness mode) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:195` | kit `combo` ↔ `CineSensorBrightnessMode` `U32` | none |
| `sensor` / `sensor_amount` (Noise amount) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:203` | kit `slider` ↔ `CineSensorNoiseAmount` `F32` | none |
| `sensor` / `sensor_chroma` (Chroma noise) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:208` | kit `slider` ↔ `CineSensorChroma` `F32` | none |
| `sensor` / `sensor_seed_fixed` (Fixed seed) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:213` | kit `toggle` ↔ `CineSensorSeedFixed` `Boolean` | none |
| `sensor` / `sensor_seed` (Seed) | `line_editor` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:216` | kit `text` ↔ `CineSensorSeed` `U32`; `ALExposureKitAdapter` preserves/calls wired in `ALPanelExposurePage::postBuild()`; behavior `ALKitBindingRegistry::add()` (`indra/newview/alpanelexposurepage.cpp:239`), wired in `ALPanelExposurePage::postBuild()`; behavior `ALPanelExposurePage::getU32()` (`indra/newview/alpanelexposurepage.cpp:244`) | none |
| `sensor` / `sensor_readout` (Sensor readout) | `text` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:219` | kit readout ← approved static copy; no runtime writer | none |
| `sensor` / `sensor_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:222` | kit readout ← approved static copy; no runtime writer | none |
| `custom_sensor` / `sensor_custom_base` (Base ISO) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:227` | kit `number` ↔ `CineSensorCustomBaseISO` `F32` | none |
| `custom_sensor` / `sensor_custom_switch` (HCG switch ISO) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:230` | kit `number` ↔ `CineSensorCustomSwitchISO` `F32` | none |
| `custom_sensor` / `sensor_custom_headroom` (Base headroom) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:233` | kit `number` ↔ `CineSensorCustomHeadroom` `F32` | none |
| `custom_sensor` / `sensor_custom_lcg_fullwell` (LCG capacity) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:236` | kit `number` ↔ `CineSensorCustomLCGFullWell` `F32` | none |
| `custom_sensor` / `sensor_custom_lcg_readnoise` (LCG read noise) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:239` | kit `number` ↔ `CineSensorCustomLCGReadNoise` `F32` | none |
| `custom_sensor` / `sensor_custom_lcg_analogbase` (LCG base gain) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:242` | kit `number` ↔ `CineSensorCustomLCGAnalogBase` `F32` | none |
| `custom_sensor` / `sensor_custom_lcg_pedestal` (LCG black level) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:245` | kit `number` ↔ `CineSensorCustomLCGPedestal` `F32` | none |
| `custom_sensor` / `sensor_custom_lcg_white` (LCG white level) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:248` | kit `number` ↔ `CineSensorCustomLCGWhite` `F32` | none |
| `custom_sensor` / `sensor_custom_hcg_fullwell` (HCG capacity) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:251` | kit `number` ↔ `CineSensorCustomHCGFullWell` `F32` | none |
| `custom_sensor` / `sensor_custom_hcg_readnoise` (HCG read noise) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:254` | kit `number` ↔ `CineSensorCustomHCGReadNoise` `F32` | none |
| `custom_sensor` / `sensor_custom_hcg_analogbase` (HCG base gain) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:257` | kit `number` ↔ `CineSensorCustomHCGAnalogBase` `F32` | none |
| `custom_sensor` / `sensor_custom_hcg_pedestal` (HCG black level) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:260` | kit `number` ↔ `CineSensorCustomHCGPedestal` `F32` | none |
| `custom_sensor` / `sensor_custom_hcg_white` (HCG white level) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:263` | kit `number` ↔ `CineSensorCustomHCGWhite` `F32` | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `LLButton::setFloaterToggle()` — `indra/llui/llui.cpp:178` | Move model-facing work into `ALExposureKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALExposureSettingsHistory::redo()` — `indra/newview/alpanelexposurehub.cpp:138` | Move model-facing work into `ALExposureKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALExposureSettingsHistory::undo()` — `indra/newview/alpanelexposurehub.cpp:137` | Move model-facing work into `ALExposureKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterLightBox::showTab()` — `indra/newview/alpanelexposurehub.cpp:132` | Move model-facing work into `ALExposureKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterLightBox::showTab()` — `indra/newview/alpanelexposurepage.cpp:207` | Move model-facing work into `ALExposureKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALKitBindingRegistry::add()` — `indra/newview/alpanelexposurepage.cpp:229, 239` | Move model-facing work into `ALExposureKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelExposurePage::<inline callback>()` — `indra/newview/alpanelexposurepage.cpp:209, 211` | Move model-facing work into `ALExposureKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelExposurePage::alKitSetTypedControl()` — `indra/newview/alpanelexposurepage.cpp:231` | Move model-facing work into `ALExposureKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelExposurePage::getU32()` — `indra/newview/alpanelexposurepage.cpp:244` | Move model-facing work into `ALExposureKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterReg::showInstance()` — `indra/newview/llfloaterdirector.cpp:1641` | Move model-facing work into `ALExposureKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **66 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-scene_light-tallest-850` | Only `scene_light` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-scene_light-tallest-1160` | Only `scene_light` is put in its tallest local state at 1160 px; all other cards stay default. |
| `exp_moon_linked-on` | Only `exp_moon_linked` is on; dependent visibility is measured. |
| `exp_local_include_rig-on` | Only `exp_local_include_rig` is on; dependent visibility is measured. |
| `card-sky_settings-tallest-850` | Only `sky_settings` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-sky_settings-tallest-1160` | Only `sky_settings` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-exposure_main-tallest-850` | Only `exposure_main` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-exposure_main-tallest-1160` | Only `exposure_main` is put in its tallest local state at 1160 px; all other cards stay default. |
| `exp_auto_exposure-on` | Only `exp_auto_exposure` is on; dependent visibility is measured. |
| `exp_tonemap_type-option-0` | Only `exp_tonemap_type` selects `Khronos Neutral`; other selectors remain default. |
| `exp_tonemap_type-option-1` | Only `exp_tonemap_type` selects `ACES`; other selectors remain default. |
| `exp_tonemap_type-option-2` | Only `exp_tonemap_type` selects `ACES Boosted`; other selectors remain default. |
| `exp_tonemap_type-option-3` | Only `exp_tonemap_type` selects `Reinhard`; other selectors remain default. |
| `exp_tonemap_type-option-4` | Only `exp_tonemap_type` selects `Filmic`; other selectors remain default. |
| `exp_tonemap_type-option-5` | Only `exp_tonemap_type` selects `GT`; other selectors remain default. |
| `exp_tonemap_type-option-6` | Only `exp_tonemap_type` selects `AgX`; other selectors remain default. |
| `exp_tonemap_type-option-7` | Only `exp_tonemap_type` selects `AMD LPM`; other selectors remain default. |
| `exp_tonemap_type-option-8` | Only `exp_tonemap_type` selects `S-Log3 Capture`; other selectors remain default. |
| `exp_legacy_gamma_skies-on` | Only `exp_legacy_gamma_skies` is on; dependent visibility is measured. |
| `card-exposure_hub-tallest-850` | Only `exposure_hub` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-exposure_hub-tallest-1160` | Only `exposure_hub` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-glow_filters-tallest-850` | Only `glow_filters` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-glow_filters-tallest-1160` | Only `glow_filters` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-sensor-tallest-850` | Only `sensor` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-sensor-tallest-1160` | Only `sensor` is put in its tallest local state at 1160 px; all other cards stay default. |
| `sensor_enabled-on` | Only `sensor_enabled` is on; dependent visibility is measured. |
| `sensor_iso-option-0` | Only `sensor_iso` selects `50`; other selectors remain default. |
| `sensor_iso-option-1` | Only `sensor_iso` selects `64`; other selectors remain default. |
| `sensor_iso-option-2` | Only `sensor_iso` selects `80`; other selectors remain default. |
| `sensor_iso-option-3` | Only `sensor_iso` selects `100`; other selectors remain default. |
| `sensor_iso-option-4` | Only `sensor_iso` selects `125`; other selectors remain default. |
| `sensor_iso-option-5` | Only `sensor_iso` selects `160`; other selectors remain default. |
| `sensor_iso-option-6` | Only `sensor_iso` selects `200`; other selectors remain default. |
| `sensor_iso-option-7` | Only `sensor_iso` selects `250`; other selectors remain default. |
| `sensor_iso-option-8` | Only `sensor_iso` selects `320`; other selectors remain default. |
| `sensor_iso-option-9` | Only `sensor_iso` selects `400`; other selectors remain default. |
| `sensor_iso-option-10` | Only `sensor_iso` selects `500`; other selectors remain default. |
| `sensor_iso-option-11` | Only `sensor_iso` selects `640`; other selectors remain default. |
| `sensor_iso-option-12` | Only `sensor_iso` selects `800`; other selectors remain default. |
| `sensor_iso-option-13` | Only `sensor_iso` selects `1000`; other selectors remain default. |
| `sensor_iso-option-14` | Only `sensor_iso` selects `1250`; other selectors remain default. |
| `sensor_iso-option-15` | Only `sensor_iso` selects `1600`; other selectors remain default. |
| `sensor_iso-option-16` | Only `sensor_iso` selects `2000`; other selectors remain default. |
| `sensor_iso-option-17` | Only `sensor_iso` selects `2500`; other selectors remain default. |
| `sensor_iso-option-18` | Only `sensor_iso` selects `3200`; other selectors remain default. |
| `sensor_iso-option-19` | Only `sensor_iso` selects `4000`; other selectors remain default. |
| `sensor_iso-option-20` | Only `sensor_iso` selects `5000`; other selectors remain default. |
| `sensor_iso-option-21` | Only `sensor_iso` selects `6400`; other selectors remain default. |
| `sensor_iso-option-22` | Only `sensor_iso` selects `8000`; other selectors remain default. |
| `sensor_iso-option-23` | Only `sensor_iso` selects `10000`; other selectors remain default. |
| `sensor_iso-option-24` | Only `sensor_iso` selects `12800`; other selectors remain default. |
| `sensor_iso-option-25` | Only `sensor_iso` selects `16000`; other selectors remain default. |
| `sensor_iso-option-26` | Only `sensor_iso` selects `20000`; other selectors remain default. |
| `sensor_iso-option-27` | Only `sensor_iso` selects `25600`; other selectors remain default. |
| `sensor_preset-option-0` | Only `sensor_preset` selects `Clean full-frame (base 100)`; other selectors remain default. |
| `sensor_preset-option-1` | Only `sensor_preset` selects `Dual-native cinema (800/3200)`; other selectors remain default. |
| `sensor_preset-option-2` | Only `sensor_preset` selects `Small/older sensor`; other selectors remain default. |
| `sensor_preset-option-3` | Only `sensor_preset` selects `Custom`; other selectors remain default. |
| `sensor_brightness-option-0` | Only `sensor_brightness` selects `Neutral`; other selectors remain default. |
| `sensor_brightness-option-1` | Only `sensor_brightness` selects `Brightens`; other selectors remain default. |
| `sensor_seed_fixed-on` | Only `sensor_seed_fixed` is on; dependent visibility is measured. |
| `card-custom_sensor-tallest-850` | Only `custom_sensor` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-custom_sensor-tallest-1160` | Only `custom_sensor` is put in its tallest local state at 1160 px; all other cards stay default. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## Ambiguous same-name candidates (never auto-bound)

None.

## Rejected out-of-scope same-name candidates

28 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

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
