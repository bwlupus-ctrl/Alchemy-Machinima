# Exposure — Director Kit page contract

Page key: `exposure_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (19/25 setting-bound rows; 7 C++-referenced rows).

## Real owners

- `panel_exposure_hub` / `panel` at `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:2` (24 matching rows).
- `floater_director` / `floater` at `indra/newview/skins/default/xui/en/floater_director.xml:15` (1 matching rows).
- Referencing classes: `ALPanelExposureHub` (4), `ALExposureSettingsHistory` (2), `LLButton` (1), `LLFloaterReg` (1), `LLFloaterDirector` (1), `ALFloaterLightBox` (1).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use `panel_exposure_hub.xml` controls as kit rows; no curve editor exists.

## Row bindings

`ALExposureKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `scene_light` / `exp_sun_ev` (Sun / Moon EV) | `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:34` | kit `slider` ↔ `AlchemyEnvSunEV` `F32` | `indra/newview/skins/default/xui/en/floater_adjust_environment.xml:628` `env_sun_ev` |
| `scene_light` / `exp_gi_ev` (Sky light) | `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:39` | kit `slider` ↔ `AlchemyEnvSkyGIEV` `F32` | `indra/newview/skins/default/xui/en/floater_adjust_environment.xml:672` `env_gi_ev` |
| `scene_light` / `exp_local_ev` (Local lights) | `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:47` | kit `slider` ↔ `AlchemyEnvLocalLightEV` `F32` | `indra/newview/skins/default/xui/en/floater_adjust_environment.xml:883` `env_local_ev` |
| `scene_light` / `exp_shadow_lift` (Shadow lift) | `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:49` | kit `slider` ↔ `AlchemyEnvShadowLiftEV` `F32` | `indra/newview/skins/default/xui/en/floater_adjust_environment.xml:1063` `env_shadow_lift` |
| `scene_light` / `exp_moon_linked` (Link moon) | `check_box` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:36` | kit `toggle` ↔ `AlchemyEnvMoonLinked` `Boolean` | `indra/newview/skins/default/xui/en/floater_adjust_environment.xml:814` `env_moon_linked` |
| `scene_light` / `exp_moon_ev` (Moon EV) | `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:37` | kit `slider` ↔ `AlchemyEnvMoonEV` `F32` | `indra/newview/skins/default/xui/en/floater_adjust_environment.xml:828` `env_moon_ev` |
| `scene_light` / `exp_amb_ev` (Sky ambient) | `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:41` | kit `slider` ↔ `AlchemyEnvGIAmbientEV` `F32` | `indra/newview/skins/default/xui/en/floater_adjust_environment.xml:1018` `env_amb_ev` |
| `scene_light` / `exp_probe_diff_ev` (Probe diffuse) | `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:43` | kit `slider` ↔ `AlchemyEnvGIProbeDiffuseEV` `F32` | `indra/newview/skins/default/xui/en/floater_adjust_environment.xml:928` `env_probe_diff_ev` |
| `scene_light` / `exp_probe_spec_ev` (Probe reflect) | `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:45` | kit `slider` ↔ `AlchemyEnvGIProbeSpecEV` `F32` | `indra/newview/skins/default/xui/en/floater_adjust_environment.xml:973` `env_probe_spec_ev` |
| `scene_light` / `exp_local_include_rig` (Rig lights) | `check_box` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:51` | kit `toggle` ↔ `AlchemyEnvLocalLightIncludeRig` `Boolean` | `indra/newview/skins/default/xui/en/floater_adjust_environment.xml:869` `env_local_include_rig` |
| `sky_settings` / `exp_sky_note` (Per-sky values) | `text` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:64` | kit readout ← approved static copy; no runtime writer | none |
| `sky_settings` / `exp_sky_lighting_btn` (Per-sky look) | `button` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:65` | `ALExposureKitAdapter` command/model binding → wired in `LLUI::LLUI()`; behavior `LLButton::setFloaterToggle()` (`indra/llui/llui.cpp:178`) | none |
| `exposure_main` / `exp_auto_exposure` (Auto exposure) | `check_box` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:8` | kit `toggle` ↔ `RenderDynamicExposureEnabled` `Boolean` | `indra/newview/skins/default/xui/en/floater_adjust_environment.xml:588` `env_auto_exposure` |
| `exposure_main` / `exp_exposure` (Exposure) | `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:9` | kit `slider` ↔ `RenderExposure` `F32` | page `image_tab`, `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:418` `exposure_slider`, `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:433` `exposure_spinner`, `indra/newview/skins/default/xui/en/floater_preferences_graphics_advanced.xml:1014` `RenderExposure`, `indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml:378` `RenderExposure` |
| `exposure_main` / `exp_tonemap_type` (Tone mapper) | `combo_box` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:13` | kit `combo` ↔ `AlchemyRenderTonemapType` `S32` | page `image_tab`, `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:314` `tonemap_type_combo`, `indra/newview/skins/default/xui/en/floater_preferences_graphics_advanced.xml:1155` `TonemapType` |
| `exposure_main` / `exp_tonemap_mix` (Tone strength) | `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:24` | kit `slider` ↔ `RenderTonemapMix` `F32` | `indra/newview/skins/default/xui/en/floater_preferences_graphics_advanced.xml:1208` `TonemapMix` |
| `exposure_main` / `exp_amd_exposure` (AMD exposure) | `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:26` | kit `slider` ↔ `AlchemyToneMapAMDExposure` `F32` | page `image_tab`, `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:635` `lpm_toneexp_slider`, `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:649` `lpm_toneexp_spinner` |
| `exposure_main` / `exp_slog_ev` (S-Log EV) | `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:28` | kit `slider` ↔ `AlchemyToneMapSLogEV` `F32` | page `image_tab`, `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:897` `slog_ev_slider`, `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:911` `slog_ev_spinner` |
| `exposure_main` / `exp_legacy_gamma_skies` (Legacy gamma) | `check_box` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:30` | kit `toggle` ↔ `AlchemyTonemapLegacyGammaSkies` `Boolean` | page `image_tab`, `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:379` `tonemap_legacy_gamma_skies` |
| `exposure_hub` / `exp_undo` (Undo) | `button` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:3` | `ALExposureKitAdapter` command/model binding → wired in `ALPanelExposureHub::postBuild()`; behavior `ALExposureSettingsHistory::undo()` (`indra/newview/alpanelexposurehub.cpp:136`) | none |
| `exposure_hub` / `exp_redo` (Redo) | `button` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:4` | `ALExposureKitAdapter` command/model binding → wired in `ALPanelExposureHub::postBuild()`; behavior `ALExposureSettingsHistory::redo()` (`indra/newview/alpanelexposurehub.cpp:137`) | none |
| `exposure_hub` / `exposure_popout` (Open window) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:1399` | `ALExposureKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterReg::showInstance()` (`indra/newview/llfloaterdirector.cpp:1283`) | none |
| `glow_filters` / `exp_bloom_strength` (Bloom strength) | `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:71` | kit `slider` ↔ `RenderBloomStrength` `F32` | page `bloom_glow_tab`, `indra/newview/skins/default/xui/en/panel_dpage_look_bloom_glow.xml:13` `set_RenderBloomStrength` |
| `glow_filters` / `exp_bloom_threshold` (Threshold) | `slider` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:73` | kit `slider` ↔ `RenderBloomThreshold` `F32` | page `bloom_glow_tab`, `indra/newview/skins/default/xui/en/panel_dpage_look_bloom_glow.xml:18` `set_RenderBloomThreshold` |
| `glow_filters` / `exp_filters_btn` (All filters) | `button` `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:134` | `ALExposureKitAdapter` command/model binding → wired in `ALPanelExposureHub::postBuild()`; behavior `ALFloaterLightBox::showTab()` (`indra/newview/alpanelexposurehub.cpp:131`) | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `LLButton::setFloaterToggle()` — `indra/llui/llui.cpp:178` | Move model-facing work into `ALExposureKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALExposureSettingsHistory::redo()` — `indra/newview/alpanelexposurehub.cpp:137` | Move model-facing work into `ALExposureKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALExposureSettingsHistory::undo()` — `indra/newview/alpanelexposurehub.cpp:136` | Move model-facing work into `ALExposureKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterLightBox::showTab()` — `indra/newview/alpanelexposurehub.cpp:131` | Move model-facing work into `ALExposureKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterReg::showInstance()` — `indra/newview/llfloaterdirector.cpp:1283` | Move model-facing work into `ALExposureKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **26 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

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
