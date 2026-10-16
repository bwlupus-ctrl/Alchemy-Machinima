# Wiring audit: Light > Exposure kit conversion

Static audit of the Exposure page after it became a generated kit page (10/15, legacy conversion batch 1). No build, no commit, no viewer run: every row is checked against the generated XML, `settings.xml` and the C++ source. Before this, the page embedded the shared Exposure hub (`panel_exposure_hub.xml`, three tabs Camera / Light / Post, with its own Undo and Redo). Sources: mockup page `exposure_tab` (8 cards, 25 rows), `kit_pages.json` entry `light_group/exposure_tab` (hosts copies of the hub controls from `panel_exposure_src.xml`, root class `panel_exposure_page`, provider presets `env_intensity`), generated `panel_dpage_light_exposure.xml`.

## Result

- The page is no longer legacy: classification `kit (P0)`, pin 7 -> 6, `light_group/exposure_tab` removed from `KNOWN_LEGACY_PAGES`.
- 24 generated rows: the 25 mockup rows minus the two Bloom rows (below) plus one link row. 8 cards (3 of them link cards), 3 sections, a preset strip.
- **Two mockup rows are not generated:** `exp_bloom_strength` and `exp_bloom_threshold`. `kit_lint` fails them as "unlisted second writer" of `RenderBloomStrength` / `RenderBloomThreshold`, whose home is Look > Bloom & glow, and the pinned `EXPECTED_SHORTCUTS` contract (user approval needed to change) does not list them. They are replaced by a link row `exp_bloom_link` to that home, the same device the Operator page uses (`omit_rows_note` in `kit_pages.json`). Declaring two more shortcuts would put the rows back; that needs your approval.
- **The five approved Image > Exposure shortcut rows are NOT re-added.** `kit_lint` only accepts `shortcut_of` rows inside an Easy card or a selector option, and the Image page has neither (its five rows sit in ordinary cards). Re-adding them needs a structure decision for the Image page (an Easy mode or selector blocks); the `omit_rows_note` on `look_group/image_tab` records this. Until then the Image page keeps its Exposure link row.
- Controls the hub had that the mockup moved elsewhere (so nothing is lost, they live on kit pages): Night Mask enable and darkness (Light > Rig), Graduated ND, Polarizer, Infrared and Soft Edge with their presets (Lens > Filters), `Light Rig...` (Rig link card), `Personal Lighting...` in the Light tab (`exp_sky_lighting_btn` opens the same floater). The three link cards show their title and link only; the generator does not emit the mockup's summary rows.
- New page logic class `ALPanelExposurePage` (`alpanelexposurepage.h/.cpp`, registered as `panel_exposure_page`, added to `CMakeLists.txt`): tone-mapper gating of AMD exposure and S-Log EV (event-driven off `AlchemyRenderTonemapType`, reset glyphs included), Undo / Redo (`ALExposureSettingsHistory`, button state off its revision signal), `installEditFinalizers` so a slider drag is one history step, the Filters button (`ALFloaterLightBox::showTab("colortone_settings")`), and the `env_intensity` preset provider (the 14 built-in Light Intensity presets and saved user presets, save and delete included).
- `llfloaterenvironmentadjust.h/.cpp`: new `static bool applyEnvIntensityBuiltin(key)` applying a built-in preset exactly as the Personal Lighting combo does (same table, same `ALEnvIntensityPresets::apply`, one history batch). Nothing else in that floater changed.
- Generator: new `own_history` option (`director_kit_gen.py`): the page's rows stay setting-bound but the kit history neither watches them nor records widget edits, because this page's undo is `ALExposureSettingsHistory` (Undo / Redo buttons); otherwise one change would be two undo steps. `director_parity.py` `root_owner` gained `panel_exposure_page`.

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
| 23 | exp_bloom_link | link row | navigates to Look > Bloom & glow | replaces rows `exp_bloom_strength`, `exp_bloom_threshold` | see Result |
| 24 | exp_filters_btn | button | `ALFloaterLightBox::showTab("colortone_settings")` (same call as the hub) | n/a | OK |

Preset strip (`env_intensity`): ids `b:<key>` for the 14 built-ins (Stock, Golden Hour, Blue Hour, Overcast Soft, Noon Punch, High-Key Studio, Low-Key Noir, Moonlit Night, Neon Night, Candlelit Interior, Desert Heat, Arctic Cold, Soft Fill, Reflections Pop) applied through `applyEnvIntensityBuiltin`; `u:<name>` for saved presets applied through `ALEnvIntensityPresets::applyNamed` in one history batch; Save as saves the live 16 settings (`saveCurrent`, name rules and limits as in Personal Lighting), Delete removes a saved preset. The strip's modified marker watches the same 16 settings the presets own.

## Checks run

`director_kit_gen.py` check and selftest, `director_page_sources.py --check`, `kit_lint.py --all`, `xui_layout_check.py` selftest and `--changed HEAD`, `director_functional_manifest.py` plus self-tests, `director_parity.py` `--selftest` and `--groups light_group`, `ui_draw_lint.py --changed HEAD` (0 new), `zs_check.ps1` (`alpanelexposurepage.cpp`, `llfloaterenvironmentadjust.cpp`), `prebuild_guard.ps1 -ChecksOnly`. Not run (rules): build, viewer, in-world self-test. Worth an in-world look: the preset strip (apply, Save as, Delete, the modified marker), Undo / Redo after a slider drag and after a preset, AMD exposure and S-Log EV dimming while switching the tone mapper.
