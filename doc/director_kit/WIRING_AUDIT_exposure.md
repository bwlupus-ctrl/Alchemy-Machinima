# Wiring audit: Light > Exposure kit conversion

Static audit of the Exposure page after it became a generated kit page (10/15, legacy conversion batch 1). No build, no commit, no viewer run: every row is checked against the generated XML, `settings.xml` and the C++ source. Before this, the page embedded the shared Exposure hub (`panel_exposure_hub.xml`, three tabs Camera / Light / Post, with its own Undo and Redo). Sources: mockup page `exposure_tab` (8 cards, 25 rows), `kit_pages.json` entry `light_group/exposure_tab` (hosts copies of the hub controls from `panel_exposure_src.xml`, root class `panel_exposure_page`, provider presets `env_intensity`), generated `panel_dpage_light_exposure.xml`.

## Result

- The page is no longer legacy: classification `kit (P0)`, pin 7 -> 6, `light_group/exposure_tab` removed from `KNOWN_LEGACY_PAGES`.
- 25 generated rows = the 25 mockup rows, 8 cards (3 of them link cards), 3 sections, a preset strip. Revised 10/15 after review: the two Bloom rows (`exp_bloom_strength`, `exp_bloom_threshold`) are back, as `shortcut_of` rows of their home on Look > Bloom & glow (`bloom_glow_tab/set_RenderBloomStrength`, `set_RenderBloomThreshold`). `kit_lint` only accepts a shortcut inside an Easy block, so the page now has a **Basics / Everything** page mode (setting `DirectorExposureEasyMode`, a new Boolean UI preference, default off = Everything): Basics shows Camera exposure and Bloom & filters, Everything shows every card. The two shortcuts are in the `EXPECTED_SHORTCUTS` contract (`director_kit_gen.py`, `director_page_sources.py`) on the strength of the user's standing mockup-wins rule. The temporary `exp_bloom_link` row and the two `parity_allow` entries for the bloom rows are removed.
- **The five approved Image > Exposure shortcut rows are now generated** on Look > Image, in an Easy card at the top of that page (see "Look > Image Easy card" below).
- Controls the hub had that the mockup moved elsewhere (so nothing is lost, they live on kit pages): Night Mask enable and darkness (Light > Rig), Graduated ND, Polarizer, Infrared and Soft Edge with their presets (Lens > Filters), `Light Rig...` (Rig link card), `Personal Lighting...` in the Light tab (`exp_sky_lighting_btn` opens the same floater). The three link cards show their title and link only; the generator does not emit the mockup's summary rows.
- New page logic class `ALPanelExposurePage` (`alpanelexposurepage.h/.cpp`, registered as `panel_exposure_page`, added to `CMakeLists.txt`): tone-mapper gating of AMD exposure and S-Log EV (event-driven off `AlchemyRenderTonemapType`, reset glyphs included), Undo / Redo (`ALExposureSettingsHistory`, button state off its revision signal), `installEditFinalizers` so a slider drag is one history step, the Filters button (`ALFloaterLightBox::showTab("colortone_settings")`), and the `env_intensity` preset provider (the 14 built-in Light Intensity presets and saved user presets, save and delete included).
- `llfloaterenvironmentadjust.h/.cpp`: new `static bool applyEnvIntensityBuiltin(key)` applying a built-in preset exactly as the Personal Lighting combo does (same table, same `ALEnvIntensityPresets::apply`, one history batch). Nothing else in that floater changed.
- Generator: `own_history` option (`director_kit_gen.py`), revised 10/15 after review: it is now a **list of settings** whose undo another owner records. Only those leave the kit watch list and widget history; every other control keeps kit watch and undo. `director_parity.py` `root_owner` gained `panel_exposure_page`.

## Controls (defaults and ranges equal `settings.xml`, the hub controls and the mockup)

| # | widget | kind | setting or handler | type, default, range / data values | status |
|---|---|---|---|---|---|
| 1 | exp_sun_ev | slider | `AlchemyEnvSunEV` | F32, 0, -4..6 step 0.05 | OK |
| 2 | exp_gi_ev | slider | `AlchemyEnvSkyGIEV` | F32, 0, -4..4 | OK |
| 3 | exp_local_ev | slider | `AlchemyEnvLocalLightEV` | F32, 0, -4..4 | OK |
| 4 | exp_shadow_lift | slider | `AlchemyEnvShadowLiftEV` | F32, 0, 0..2 | OK |
| 5 | exp_moon_linked (Advanced) | toggle | `AlchemyEnvMoonLinked` | Boolean, 1 | OK |
| 6 | exp_moon_ev (Advanced) | slider | `AlchemyEnvMoonEV`; `disabled_control` = `AlchemyEnvMoonLinked` | F32, 0, -4..6 | OK |
| 7 | exp_amb_ev (Advanced) | slider | `AlchemyEnvGIAmbientEV` | F32, 0, -3..3 | OK |
| 8 | exp_probe_diff_ev (Advanced) | slider | `AlchemyEnvGIProbeDiffuseEV` | F32, 0, -3..3 | OK |
| 9 | exp_probe_spec_ev (Advanced) | slider | `AlchemyEnvGIProbeSpecEV` | F32, 0, -3..3 | OK |
| 10 | exp_local_include_rig (Advanced) | toggle | `AlchemyEnvLocalLightIncludeRig` | Boolean, 0 | OK |
| 11 | exp_sky_note | readout | none (static copy) | text | OK |
| 12 | exp_sky_lighting_btn | button | hosted `Floater.ShowOrBringToFront env_adjust_snapshot` (the hub's own callback) | n/a | OK |
| 13 | exp_auto_exposure | toggle | `RenderDynamicExposureEnabled` | Boolean, 1 | OK |
| 14 | exp_exposure | slider | `RenderExposure` | F32, 1.0, 0.5..4 step 0.1 | OK |
| 15 | exp_tonemap_type | combo | `AlchemyRenderTonemapType` | S32, 7; 0 Khronos Neutral, 1 ACES, 2 ACES Boosted, 3 Reinhard, 4 Filmic, 5 GT, 6 AgX, 7 AMD LPM, 8 S-Log3 Capture | OK |
| 16 | exp_tonemap_mix | slider | `RenderTonemapMix` | F32, 0.7, 0..1 | OK |
| 17 | exp_amd_exposure (Advanced) | slider | `AlchemyToneMapAMDExposure`; enabled only for type 7 (`updateTonemapGating`) | F32, 7.4, 1..16 | OK |
| 18 | exp_slog_ev (Advanced) | slider | `AlchemyToneMapSLogEV`; enabled only for type 8 | F32, 0, -8..8 | OK |
| 19 | exp_legacy_gamma_skies (Advanced) | toggle | `AlchemyTonemapLegacyGammaSkies` | Boolean, 0 | OK |
| 20 | exp_undo | button | `ALExposureSettingsHistory::undo()`; enabled and tooltip from `canUndo` / `undoLabel` on the revision signal | n/a | OK |
| 21 | exp_redo | button | `ALExposureSettingsHistory::redo()` | n/a | OK |
| 22 | exposure_popout | button | `LLFloaterReg::showInstance("exposure_hub")` (`LLFloaterDirector::bindCameraPage`, unchanged) | n/a | OK |
| 23 | exp_bloom_strength, exp_bloom_threshold | slider x2 | settings `RenderBloomStrength` (F32, 0.325, 0..2 step 0.01, 3 dp) and `RenderBloomThreshold` (F32, 2.5, 0..4 step 0.05, 2 dp); `shortcut_of` Look > Bloom & glow | OK |
| 24 | exp_filters_btn | button | `ALFloaterLightBox::showTab("colortone_settings")` (same call as the hub) | n/a | OK |

Preset strip (`env_intensity`): ids `b:<key>` for the 14 built-ins (Stock, Golden Hour, Blue Hour, Overcast Soft, Noon Punch, High-Key Studio, Low-Key Noir, Moonlit Night, Neon Night, Candlelit Interior, Desert Heat, Arctic Cold, Soft Fill, Reflections Pop) applied through `applyEnvIntensityBuiltin`; `u:<name>` for saved presets applied through `ALEnvIntensityPresets::applyNamed` in one history batch; Save as saves the live 16 settings (`saveCurrent`, name rules and limits as in Personal Lighting), Delete removes a saved preset. The strip's modified marker watches the same 16 settings the presets own.

## Undo owners (revised 10/15)

Each setting-bound control has exactly one undo owner. `ALExposureSettingsHistory` (`alsettingsundohistory.cpp`, `TRACKED_SETTINGS`) records the env EVs, tint settings, `RenderDynamicExposureEnabled` and `RenderExposure`; the page's Undo / Redo buttons drive it. Those settings are listed in `own_history` in `kit_pages.json`, so the kit history does not watch them and their rows have `widget_history="false"`. Every other setting on the page is not tracked there and keeps kit watch and widget undo (the shell's undo).

| rows | settings | undo owner |
|---|---|---|
| 1-10 | `AlchemyEnvSunEV`, `AlchemyEnvSkyGIEV`, `AlchemyEnvLocalLightEV`, `AlchemyEnvShadowLiftEV`, `AlchemyEnvMoonLinked`, `AlchemyEnvMoonEV`, `AlchemyEnvGIAmbientEV`, `AlchemyEnvGIProbeDiffuseEV`, `AlchemyEnvGIProbeSpecEV`, `AlchemyEnvLocalLightIncludeRig` | `ALExposureSettingsHistory` only |
| 13, 14 | `RenderDynamicExposureEnabled`, `RenderExposure` | `ALExposureSettingsHistory` only |
| 15 | `AlchemyRenderTonemapType` | kit history only |
| 16 | `RenderTonemapMix` | kit history only |
| 17 | `AlchemyToneMapAMDExposure` | kit history only |
| 18 | `AlchemyToneMapSLogEV` | kit history only |
| 19 | `AlchemyTonemapLegacyGammaSkies` | kit history only |
| 23 | `RenderBloomStrength`, `RenderBloomThreshold` | kit history only |
| preset strip | the 16 settings the `env_intensity` presets own (all in `TRACKED_SETTINGS`) | `ALExposureSettingsHistory` (one batch per preset) |

The generated page lists the five kit-owned settings plus the two bloom settings in `watch_settings` and none of the tracked ones (checked in `panel_dpage_light_exposure.xml`). Before this revision `own_history` dropped every setting, so Tone mapper, Tone strength, AMD exposure, S-Log EV and Legacy gamma had no undo at all.

## Look > Image Easy card (added 10/15)

Look > Image has no wiring audit of its own, so the shortcut rows are recorded here. The page gained an Easy card at the top (first card of the Tone section) holding the five approved Light > Exposure shortcuts, each a `shortcut_of` row with the same widget name it had in the mockup (moved from the Tone mapper, AMD LPM and S-Log3 cards; the rows were omitted from the page before). `kit_lint` accepts shortcuts only in an Easy block, so the page also has an **Easy view / Full view** page mode (setting `DirectorImageEasyMode`, new Boolean UI preference, default off = Full view): Easy view shows only the Easy card, Full view shows every card including it. Both pages' shortcut lists are in the `EXPECTED_SHORTCUTS` contract (`director_kit_gen.py`, `director_page_sources.py`); `omit_rows` on `look_group/image_tab` is now empty and its note says so.

| widget | setting | home | type, range | undo owner | status |
|---|---|---|---|---|---|
| tonemap_type_combo | `AlchemyRenderTonemapType` | `exposure_tab/exp_tonemap_type` | S32, 7; same nine items as the home | kit history | OK |
| exposure_slider | `RenderExposure` | `exposure_tab/exp_exposure` | F32, 1.0, 0.5..4 step 0.1 | `ALExposureSettingsHistory` (`own_history: ["RenderExposure"]` on the Image page, so one change is one step) | OK |
| tonemap_legacy_gamma_skies | `AlchemyTonemapLegacyGammaSkies` | `exposure_tab/exp_legacy_gamma_skies` | Boolean, 0 (was Advanced on Image, basic in the Easy card) | kit history | OK |
| lpm_toneexp_slider | `AlchemyToneMapAMDExposure` | `exposure_tab/exp_amd_exposure` | F32, 7.4, 1..16 step 0.1 | kit history | OK |
| slog_ev_slider | `AlchemyToneMapSLogEV` | `exposure_tab/exp_slog_ev` | F32, 0, -8..8 step 0.1 | kit history | OK |

The generator copies each shortcut's kind, range, step, decimals and unit from its home (`normalize_shortcuts`) and appends "Home: Exposure > ..." to its help; `kit_lint` checks the same fields against the generated home row. The Image page keeps its Exposure link row.

## Checks run

`director_kit_gen.py` check and selftest, `director_page_sources.py --check`, `kit_lint.py --all`, `xui_layout_check.py` selftest and `--changed HEAD`, `director_functional_manifest.py` plus self-tests, `director_parity.py` `--selftest` and `--groups light_group`, `ui_draw_lint.py --changed HEAD` (0 new), `zs_check.ps1` (`alpanelexposurepage.cpp`, `llfloaterenvironmentadjust.cpp`), `prebuild_guard.ps1 -ChecksOnly`. Not run (rules): build, viewer, in-world self-test. Worth an in-world look: the preset strip (apply, Save as, Delete, the modified marker), Undo / Redo after a slider drag and after a preset, AMD exposure and S-Log EV dimming while switching the tone mapper.
