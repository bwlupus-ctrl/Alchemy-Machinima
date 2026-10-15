# Wiring audit: Light > Exposure (Personal Lighting exposure) kit conversion

Static, read-only audit, 2026-10-15. No build, no commit, no edits except this file.

- Repo: `I:/alchemy-machinima`, HEAD `02c2587781f`. The working tree has uncommitted Director Settings work in `scripts/code_tools/director_kit/kit_pages.json`, `scripts/code_tools/director_kit_gen.py`, `scripts/code_tools/director_page_sources.py`, `scripts/code_tools/director_kit/director_mockup_data.json`, `indra/newview/llfloaterdirector.{cpp,h}`, `indra/newview/alkitshell.{cpp,h}`. Line numbers in those files are the working-tree lines at read time.
- Curated source `C:\Users\xianw\cxs\content_curated` is outside the repo and was read only.
- `unknown` marks anything not verified in this pass.
- Out of scope but a second writer: the standalone Personal Lighting floater (`env_adjust_snapshot`, kept) and the Exposure hub floater (`exposure_hub`).

## 0. Result

- Personal Lighting (PL) Light Intensity strip: **18 settings** (auto exposure included), **1 UI-state key** (`AlchemyEnvIntensityAdvanced`), **36 interactive controls** (3 checks, 12 sliders, 3 colour swatches, 1 combo, 17 buttons), 3 readouts.
- Undo is already one shared stack (19 tracked keys, including `RenderExposure`). It is a singleton listening on `gSavedSettings`, so any writer, including the Director, already records into it.
- Today's Director page (`panel_dpage_light_exposure.xml`, a legacy wrapper around `panel_exposure_hub.xml`) carries **11 of the 18 settings**. It misses Sun temperature (Kelvin) and the three tint pairs (7 settings), the Quick Presets combo and "Presets...", the Advanced panel and "Reset advanced", the legacy/classic note, and the Sun/Moon label flip and Moon enable rule.
- The kit adds neither edit finalizers nor batching. Without new code, Director drags split into several undo steps, page resets make one undo step per key, and tint pairs reset only half.
- Presets: 14 built-ins (C++ table private to `llfloaterenvironmentadjust.cpp`) plus the user file `light_intensity_presets.xml`. No `env_intensity` kit provider is registered; the curated `presets.json` spec exists. The kit's existing preset store is a different file and must not be used for these presets.
- **Blocking mockup defect:** `exp_nm_darkness` binds `CineLightRigNightMaskDarkness` (a 0..1 floor, default 0.15) as a 0..100 percent slider. The hub converts `percent = (1 - floor) * 100`. A raw kit row writes the wrong value (risk R1).
- The conversion also moves the pinned generator counts and the five approved `image_tab` shortcuts (`kit_pages.json:195-196`).

## 1. Personal Lighting exposure controls

### 1.1 Strip widgets

Widget lines: `indra/newview/skins/default/xui/en/floater_adjust_environment.xml`. Setting lines: `indra/newview/app_settings/settings_alchemy.xml` unless marked. "Undo" = listed in `TRACKED_SETTINGS`, `indra/newview/alsettingsundohistory.cpp:26-47`.

Basic strip (`lp_env_intensity`, XML 564):

| # | widget (XML line) | type | label | setting: type, default (line) | UI range (XML) | consumer clamp | handler / side effects | read-back |
|---|---|---|---|---|---|---|---|---|
| 1 | `env_auto_exposure` (581) | check | "Auto exposure" (586) | `RenderDynamicExposureEnabled`: Boolean, 1 (4347) | n/a | `pipeline.cpp:13051` (coefficient and speed at 13052-13054; section 1.2) | control binding; Undo (`alsettingsundohistory.cpp:45`) | control |
| 2 | `env_exposure_undo` (591) | button | "Undo" | none | n/a | `ALExposureSettingsHistory::undo()`, `llfloaterenvironmentadjust.cpp:564-570` | tooltip "Undo: <label>" / "Nothing to undo", `:1265-1283` | revision signal, connected `:578-580` |
| 3 | `env_exposure_redo` (600) | button | "Redo" | none | n/a | `redo()`, `:571-577` | same | same |
| 4 | `env_sun_ev` (617) | slider | label `env_sun_ev_label` (609): "Sun / Moon (EV):" (XML 26); flips to "Sun (EV):" (XML 27) when unlinked, `:1184-1199` | `AlchemyEnvSunEV`: F32, 0 (4129) | -4..6, step 0.05, 2 dp | ±6: `alenvintensity.cpp:38,44-47`, applied at 257 | reset `env_sun_ev_reset` (647) → `llfloaterenvironmentadjust.cpp:422-428` (batch "Reset Sun EV"); watcher `:232-250`, connected `:530-536`; edit finalizers `:514-517` | control |
| 5 | `env_gi_ev` (661) | slider | "Sky light (GI) (EV):" (658) | `AlchemyEnvSkyGIEV`: F32, 0 (4140) | -4..4, 0.05, 2 dp | ±6 per sum: `alenvintensity.cpp:258-259,333` | reset `env_gi_ev_reset` (691) → `:430-436` (batch) | control |
| 6 | `env_legacy_sun_note` (703, hidden) | readout | classic note (XML 17-18), legacy note (XML 28-29) | none | n/a | none | `updateGammaLabel()` `:1085-1131`; signals `:444-454`; refresh `:753` | predicate: classic = `canAutoAdjust() && !RenderSkyAutoAdjustLegacy`; legacy = probe ambiance == 0 |
| 7 | `env_intensity_hint` (713) | readout | static text | none | n/a | none | none | none |
| 8 | `env_intensity_preset` (731; items 734-748) | combo | "Quick Presets" (tooltip only) | none (derived) | n/a | none | `onEnvIntensityPresetSelected()` `:1204-1235`; `applyEnvIntensityPreset()` `:1244-1263`; `refreshEnvIntensityPresetCombo()` `:1330-1366`; user section `:1291-1321`; commit `:524-528` | `refreshEnvIntensityPresetCombo()`; dirty flag set by watchers, refreshed in `draw()` `:1368-1376` |
| 9 | `env_intensity_presets_btn` (762) | button | "Presets..." | none | n/a | none | `LLFloaterReg::showInstance("env_intensity_presets")` `:554-558` (floater registered `llviewerfloaterreg.cpp:438`) | none |
| 10 | `env_adv_toggle` (773) | button | "Advanced +" / "Advanced -" (XML 21-22) | `AlchemyEnvIntensityAdvanced`: Boolean, 0 (4334), persist 1 | n/a | none | toggles the setting `:492-498`; `applyEnvIntensityAdvanced()` `:1138-1178`; label `:1147-1150`; signal `:501-505` | setting |

Advanced panel (`lp_env_intensity_adv`, XML 790, hidden by default):

| # | widget (XML line) | type | label | setting: type, default (line) | UI range | consumer clamp | handler / side effects | read-back |
|---|---|---|---|---|---|---|---|---|
| 11 | `env_moon_linked` (807) | check | "Link to Sun" (810) | `AlchemyEnvMoonLinked`: Boolean, 1 (4154) | n/a | `alenvintensity.cpp:255-256` | `updateEnvIntensityMoonLink()` `:1184-1199`; signal `:507-511`; reset-advanced sets true `:489` | setting |
| 12 | `env_moon_ev` (828) | slider | "Moon (EV):" (804) | `AlchemyEnvMoonEV`: F32, 0 (4165) | -4..6, 0.05, 2 dp | ±6 (`:255-257`) | disabled while linked (`:1191-1197`); reset `env_moon_ev_reset` (847) → table `:159` | control |
| 13 | `env_local_include_rig` (869) | check | "incl. rig lights" (866) | `AlchemyEnvLocalLightIncludeRig`: Boolean, 0 (4220) | n/a | Cine Rig emitters excluded unless on: `alenvintensity.cpp:317` | excluded from presets `llfloaterenvironmentadjust.cpp:182-185`; in Reset advanced `:132-147` | control |
| 14 | `env_local_ev` (883) | slider | "Local lights:" (859) | `AlchemyEnvLocalLightEV`: F32, 0 (4209) | -4..4, 0.05, 2 dp | ±6 (`:321`) | reset `:902` → table `:160` | control |
| 15 | `env_probe_diff_ev` (928) | slider | "Probe diffuse (EV):" (914) | `AlchemyEnvGIProbeDiffuseEV`: F32, 0 (4187) | -3..3, 0.05, 2 dp | ±6 per sum (`:259`; SSAO ceiling `:333`) | reset `:947` → `:161` | control |
| 16 | `env_probe_spec_ev` (973) | slider | "Probe reflections (EV):" (959) | `AlchemyEnvGIProbeSpecEV`: F32, 0 (4198) | -3..3, 0.05, 2 dp | ±6 (`:260`) | reset `:992` → `:162` | control |
| 17 | `env_amb_ev` (1018) | slider | "Sky ambient (EV):" (1004) | `AlchemyEnvGIAmbientEV`: F32, 0 (4176) | -3..3, 0.05, 2 dp | ±6 per sum (`:258`) | reset `:1037` → `:163` | control |
| 18 | `env_shadow_lift` (1063) | slider | "Shadow lift (EV):" (1049) | `AlchemyEnvShadowLiftEV`: F32, 0 (4323) | 0..2, 0.05, 2 dp | 0..2 (`:265`; `LIFT_EV_CLAMP` `:39`) | reset `:1082` → `:164` | control |
| 19 | `env_sun_kelvin` (1108) | slider | "Sun temperature (K):" (1094) | `AlchemyEnvSunKelvin`: F32, 6500 (4231) | 2000..12000, 50, 0 dp | 1000..15000 (`alenvintensity.cpp:82`); identity at 6500 (`:161-163`) | reset `:1127` → `:165` | control |
| 20 | `env_sun_tint_swatch` (1142-1150) + `env_sun_tint_strength` (1153-1169) | colour + slider | "Sun tint:" (1139) | `AlchemyEnvSunTintColor`: Color4, (1,1,1,1) (4242); `AlchemyEnvSunTintStrength`: F32, 1 (4258) | strength 0..1, 0.01, 3 dp | channels clamped 0..1 (`:49-53`); strength 0..1 (`:185`); max channel 4 (`:40,202-204`); legacy-gamma normalises max to 1 (`:284-288`); stays active in probe captures (`:297-303` does not touch it) | reset `env_sun_tint_reset` (1171-1180) → `:166` (swatch and strength together) | control |
| 21 | `env_moon_tint_swatch` (1191-1199) + `env_moon_tint_strength` (1202-1216) | colour + slider | "Moon tint:" (1188) | `AlchemyEnvMoonTintColor` (4269); `AlchemyEnvMoonTintStrength` (4285) | 0..1, 0.01, 3 dp | as row 20; applied only when `!sun_up` (`:262-263`) | reset `:1220-1229` → `:167` | control |
| 22 | `env_amb_tint_swatch` (1240-1248) + `env_amb_tint_strength` (1251-1265) | colour + slider | "Ambient tint:" (1237) | `AlchemyEnvAmbientTintColor` (4296); `AlchemyEnvAmbientTintStrength` (4312) | 0..1, 0.01, 3 dp | as row 20; zeroed in probe captures (`:297-303`); `:264` | reset `:1269-1278` → `:168` | control |
| 23 | `env_adv_reset_all` (1287) | button | "Reset advanced" (label 1283) | 14 keys (`:132-147`) + `AlchemyEnvMoonLinked` true (`:489`) | n/a | n/a | one batch "Reset All" `:479-491`; excludes both masters and the advanced flag | none |
| 24 | `env_adv_note` (1297) | readout | static text | none | n/a | none | none | none |

Containers: `lp_env_intensity` (564), `lp_env_intensity_adv` (790). Column notes in the XML comment at 550-562.

### 1.2 Auto exposure consumers (row 1)

- `RenderDynamicExposureEnabled`: read at `pipeline.cpp:13051`.
- `RenderDynamicExposureCoefficient`: `settings.xml:21899`, persist 0, default 0.5; not in any PL UI. Read at `pipeline.cpp:13052`.
- `RenderDynamicExposureSpeedError`: `settings_alchemy.xml:4358`, persist 0, default 0.1. Read at `:13053`.
- `RenderDynamicExposureSpeedTarget`: `settings_alchemy.xml:4369`, persist 0, default 2.0. Read at `:13054`.
- The tooltip claim "S-Log3 ignores it" is `unknown` in this pass.

### 1.3 Camera exposure (not PL, but in the PL undo stack)

- `RenderExposure`: F32, 1.0, persist 1 (`settings.xml:21523`). Clamp 0.5..4 at `pipeline.cpp:13246`, `:14817`, `:18139`. Hub slider 0.5..4, 0.1 step, 1 dp (`panel_exposure_hub.xml:9`).
- Not on the strip. Tracked by the PL undo as "Camera exposure" (`alsettingsundohistory.cpp:46`); see R13.

### 1.4 Consumer clamp summary

| setting | UI (XML) | consumer | verdict |
|---|---|---|---|
| Sun / Moon / Local EV | -4..6 / -4..6 / -4..4 | ±6 (`alenvintensity.cpp:38,44-47`) | fine |
| Sky GI + sky ambient, or GI + probe diffuse | GI -4..4; ambient and diffuse -3..3 | each sum clamped to ±6 (`:258-259`) | UI can reach ±7 and saturates silently (R12) |
| Probe reflections | -3..3 | ±6 (`:260`) | fine |
| Shadow lift | 0..2 | 0..2 (`:265`) | fine |
| Kelvin | 2000..12000 | 1000..15000 (`:82`) | UI hides consumer-accepted values; preset sanitize also clamps to 2000..12000 (`alenvintensitypresets.cpp:64-76`, `sanitize` `:181-204`), so Debug values outside are rewritten on save (R12) |
| Tint strengths | 0..1 | 0..1 (`:185`) | fine |
| Tint channels | sRGB picker | 0..1 (`:49-53`); max channel 4 (`:40`) | fine |
| RenderExposure | 0.5..4 | 0.5..4 (`pipeline.cpp:13246`) | fine |

## 2. Current Director Exposure page

### 2.1 Structure and routing

- `floater_director_v2.xml:81`: `kit_lazy_page name="exposure_tab"` → `panel_dpage_light_exposure.xml`. That file is generated (header hash), `legacy="true"` (line 3), and holds `exposure_popout` (button) and `director_exposure_panel` = `panel_exposure_hub.xml`.
- Hub XML: `panel_exposure_hub.xml` (137 lines). Buttons `exp_undo` / `exp_redo` at 3-4. Tabs: `exp_tab_camera` (6-31), `exp_tab_light` (32-54 for PL, 55-62 Cine Light Rig, 63-67 Sky), `exp_tab_post` (69-134).
- Hub C++: `indra/newview/alpanelexposurehub.cpp`. `RESET_SETTINGS` at 30-52 (no Kelvin or tints). Reset buttons batched at 89-95. Undo/redo at 136-137. History connection at 138-140. `installEditFinalizers` on the camera and light tabs only (141-142; the Post tab is not finalized). Night Mask conversion in `onNightMaskDarknessCommit` and `syncNightMaskDarkness` (called at 113, 118, 145). Lens filter preset combos commit at 121 (`exp_ir_preset`) and 126 (`exp_softedge_preset`). Filters button callback at 131 opens `colortone_settings`.
- Director binder: `indra/newview/llfloaterdirector.cpp`. Route `exposure_tab` → `bindCameraPage` at 1391 (`bindCameraPage` at 1919). Popout at 2015. `director_exposure_panel` fill at 1118-1123, 2063-2067, 2346. `exposure_tab` is in the unwrapped-page list at 1114. `llfloaterdirector.cpp` has no reference to `ALExposureSettingsHistory` or `ALEnvIntensityPresets` (grep: 0).
- Generator: `scripts/code_tools/director_kit_gen.py:174-184` classifies `exposure_tab` as legacy (leaf in `floater_director.xml:1398`, no `p0_pages` entry). Pinned classification counts at `:1212` (working tree: kit 23, pure 1, legacy 13).
- Repo mockup: `scripts/code_tools/director_kit/director_mockup_data.json`, `light_group/exposure_tab`: `pre: env_intensity`, 8 cards, 14 preset names, `preset_user: true`. Generator strip map `director_mockup_data.py:382` maps it to `("env_intensity", [])`.
- Curated (outside repo): `C:\Users\xianw\cxs\content_curated\light_group.json` exposure_tab has `preset_provider: env_intensity`. Its `scene_light` card has 10 rows (no Kelvin, tints, presets, Reset advanced, note). `presets.json` `providers` has an `env_intensity` spec (builtin true, 14 names, user file). Edits belong in the curated files, then regeneration (`director_mockup_data.py:21`).

### 2.2 PL coverage on the Director page today

| setting | on Director page now | where (`panel_exposure_hub.xml`) | gap |
|---|---|---|---|
| RenderDynamicExposureEnabled | yes | Camera tab, line 8 | none |
| AlchemyEnvSunEV | yes | 34 | no label flip |
| AlchemyEnvSkyGIEV | yes | 39 | none |
| AlchemyEnvMoonLinked | yes | 36 | none |
| AlchemyEnvMoonEV | yes | 37-38 (`disabled_control`) | generator cannot emit `disabled_control` (R10) |
| AlchemyEnvGIAmbientEV | yes | 41 | none |
| AlchemyEnvGIProbeDiffuseEV | yes | 43 | none |
| AlchemyEnvGIProbeSpecEV | yes | 45 | none |
| AlchemyEnvLocalLightEV | yes | 47 | none |
| AlchemyEnvLocalLightIncludeRig | yes | 51 | none |
| AlchemyEnvShadowLiftEV | yes | 49 | none |
| AlchemyEnvSunKelvin | **no** | — | add |
| Sun tint (colour + strength) | **no** | — | add (pair reset, R4) |
| Moon tint (colour + strength) | **no** | — | add |
| Ambient tint (colour + strength) | **no** | — | add |
| AlchemyEnvIntensityAdvanced (UI state) | **no** | — | decide (R11) |

Not on the Director page: Quick Presets combo, "Presets...", Advanced toggle, Reset advanced, legacy/classic note, Sun/Moon label flip, Moon enable rule. Undo and Redo are present at the hub top (3-4).

Mockup and coverage audit: `doc/director_kit/COVERAGE_AUDIT_2026-10-15.md:59` (row 26: 39 of 63 hub controls unindexed) and `:60` (row 27: Kelvin, tints and presets unindexed; `exp_personal_lighting_btn` the only path).

### 2.3 Non-PL controls that must be kept

| group | widgets (hub line) | settings (default, line) | notes |
|---|---|---|---|
| Camera exposure | `exp_exposure` (9), `exp_tonemap_type` (13; 9 items 14-22), `exp_tonemap_mix` (24), `exp_amd_exposure` (26), `exp_slog_ev` (28), `exp_legacy_gamma_skies` (30) | `RenderExposure` (1.0, `settings.xml:21523`); `AlchemyRenderTonemapType` (S32 7, `settings_alchemy.xml:1275`); `RenderTonemapMix` (0.7, `settings.xml:22543`); `AlchemyToneMapAMDExposure` (7.4, `settings_alchemy.xml:1353`); `AlchemyToneMapSLogEV` (0, `:1331`); `AlchemyTonemapLegacyGammaSkies` (Boolean 0, `:1287`) | AMD enabled only on tonemap type 7, S-Log EV only on type 8 (`ALPanelExposureHub::updateTonemapGating`). Hub Camera tab: `exp_tab_camera` lines 6-31 |
| Post: bloom | `exp_bloom_strength` (71), `exp_bloom_threshold` (73) | `RenderBloomStrength` (0.325), `RenderBloomThreshold` (2.5), `settings_alchemy.xml` | enabled by `RenderBloomHDR` |
| Post: filters button | `exp_filters_btn` (134) | none | opens lightbox `colortone_settings` |
| Post: lens filters | GND `exp_gnd_*` (75-88), Polarizer `exp_pol_*` (89-100), Infrared `exp_ir_*` (105-119), Soft edge `exp_softedge_*` (120-133) | `RenderGradNDEnabled`, `RenderPolarizerEnabled`, `RenderInfraredEnabled`, `RenderSoftEdgeEnabled` and their strength and preset settings | preset combos apply `ALLensLooks` presets on commit and reset to "0" (commit lambdas `alpanelexposurehub.cpp:121-130`). Keep these writers |
| Night Mask | `exp_nm_enabled` (102), `exp_nm_darkness` (103-104) | `CineLightRigNightMaskEnabled` (Boolean 0); `CineLightRigNightMaskDarkness` (F32 0.15 floor, `settings.xml:2390-2395`) | darkness UI is percent; conversion in the hub (R1) |
| Cine Light Rig | `exp_rig_master_ev` (56), `exp_rig_headroom` (58), `exp_light_rig_btn` (60) | `CineLightRigMasterEV` (0, `settings.xml:1991`), `CineLightRigHeadroomStops` (2, `settings.xml:2012`) | master -16..16 step 0.25; headroom 0..16 |
| Sky | `exp_sky_note` (64), `exp_sky_lighting_btn` (65) | none | opens `env_adjust_snapshot`. Scene gamma and probe ambiance are per sky, in PL's basic section |
| Window | `exposure_popout` (`panel_dpage_light_exposure.xml:4`) | none | opens `exposure_hub` (`llfloaterdirector.cpp:2015`) |

### 2.4 Gaps (summary)

- Missing PL settings: 7 (Kelvin, three tint pairs).
- Missing PL UI: Quick Presets combo, "Presets...", Advanced toggle, Reset advanced, legacy/classic note, Sun/Moon label flip, Moon enable rule.
- Present but duplicated: auto exposure (also in the hub Camera tab), Light tab rows (hub copies the PL writers).
- Present but incorrect in the mockup: Night Mask darkness (R1).

## 3. Non-trivial parts

### 3.1 Quick Presets

- Built-in table: `llfloaterenvironmentadjust.cpp:186-206` (struct `EnvIntensityPreset`, 16 fields) and `:209-225` (14 rows). Values are the 16 settings in this order: SunEV, SkyGIEV, MoonLinked, MoonEV, GIAmbientEV, GIProbeDiffuseEV, GIProbeSpecEV, LocalLightEV, SunKelvin, SunTint (colour + strength), MoonTint (colour + strength), AmbientTint (colour + strength), ShadowLiftEV.
- Names (XUI items 734-747 and the table keys): Stock, Golden Hour, Blue Hour, Overcast Soft, Noon Punch, High-Key Studio, Low-Key Noir, Moonlit Night, Neon Night, Candlelit Interior, Desert Heat, Arctic Cold, Soft Fill, Reflections Pop.
- Placeholder "Custom" (XUI 748, value `custom`). Selected when no preset matches (`:1330-1366`). Match tolerance 1e-4 (`alenvintensitypresets.cpp:50`).
- Ties: built-in beats user preset with the same values (comment `:1324-1326`).
- Apply writes exactly the 16 settings through `ALEnvIntensityPresets::apply()` (`alenvintensitypresets.cpp:1074-1097`; called at `llfloaterenvironmentadjust.cpp:1262`). It never writes `AlchemyEnvIntensityAdvanced`, `AlchemyEnvLocalLightIncludeRig` or `RenderDynamicExposureEnabled` (comment `:181-186`).
- Undo label: the floater's apply batch is "Preset '<name>'" (`:1221-1223`, `:1260-1261`); the module's `apply()` batch is "Preset" (`alenvintensitypresets.cpp:1076`).
- User section: "-- My presets --" header string (XML 25). Rebuilt by `rebuildEnvIntensityUserPresets()` (`:1291-1321`), driven by `connectListChanged` and `connectApplied` (`:540-552`). User item values are `user:<name>`.

### 3.2 My presets (store and Save / Load / Delete dialog)

- Store (`alenvintensitypresets.h/.cpp`): file `light_intensity_presets.xml` in the user settings directory (`alenvintensitypresets.cpp:44`, `:174-177`, `LL_PATH_USER_SETTINGS`). Version 1. Limits: 200 presets (`.h:90`), 64-character names (`.h:89`), reserved names = built-in names, case-insensitive (`isReservedName`, `.cpp:125-131`).
- Ranges: `FLOAT_FIELDS` at `.cpp:64-76` clamp the 12 float fields to the slider ranges; `COLOR_FIELDS` at `:85-89` clamp channels to 0..1; `sanitize()` at `:181-204`.
- Write path: `save()` at `:1179-1244` (name check, cross-process lock `PresetsLock` at `:506`, reload-if-changed, sanitize, atomic write, rollback on failure). Damaged files are renamed to `.bad[.N].xml`, never deleted (header comment in `.h:1-27`).
- Other API: `applyNamed` `:1297`, `remove` `:1262`, `listNames` `:1132`, `findMatchingName` `:1166`, `isApplying` `:1099`.
- Dialog (`alfloaterenvintensitypresets.cpp`): `postBuild` `:70-96` (connects list changes); `onOpen` `:97-107` (pre-selects the matching user preset); `refreshList` `:108-131`; `onSave` `:154-190` (read-only check, name check, overwrite confirm `EnvIntensityPresetOverwrite`, captures the name not `this`); `onLoad` `:191-204` (`applyNamed`, refresh if gone); `onDelete` `:205-232` (confirm `EnvIntensityPresetConfirmDelete`, busy/read-only/failed notices). XUI: `floater_env_intensity_presets.xml` (`preset_list`, `preset_name`, `btn_save`, `btn_load`, `btn_delete`, `btn_close`). Notifications: `notifications.xml:13781-13893`.
- Menu: "Light Intensity Presets..." at `menu_viewer.xml:414-418` (no RLV gate). "Personal Lighting..." at `:404-410` is gated by `RLV.EnableIfNot setenv` (R18).

### 3.3 Undo / redo: scope

- Service: `alsettingsundohistory.cpp`. `service()` singleton at `:237`. Listeners on the 19 tracked keys in the constructor (`:56-80`), skipped while applying (`:68`). Tick every 0.25 s (`:58`, `:83-94`) ends an edit when no mouse capture is held and closes idle steps. Apply (`:166-185`) reverts or re-applies in order and suppresses re-recording.
- Model: `alsettingsundohistorymodel.cpp`. `record()` `:13-52` merges a same-key change into the pending step only within 500 ms and only while one change is pending. Batch `:54-75`, edit `:77-93`, idle close `:95-102`, undo/redo `:114-144`, merge `:205-219`. Constants: `COALESCE_MILLISECONDS = 500` and `MAX_DEPTH = 100` (`alsettingsundohistorymodel.h:33-34`).
- Clear: `llstartup.cpp:2660` (start of session), `llappviewer.cpp:6248`. Initialize: `llviewercontrol.cpp:1123`.
- Consumers: floater (`llfloaterenvironmentadjust.cpp:562-580`, `:1265-1283`); hub (`alpanelexposurehub.cpp:136-142`); kit: **none** (grep: no `ALExposureSettingsHistory` in `alkit*.cpp`).
- Edit finalizers: `installEditFinalizers()` at `alsettingsundohistory.cpp:261` handles `LLSliderCtrl` (mouse down/up, editor commit) and `LLSpinCtrl` (arrow buttons, line editor). It does not handle colour swatches, checkboxes or combos.
- Answer to "must the Director share the floater's history": it already does by construction, because every writer goes through `gSavedSettings`. The Director must not add a second stack for these rows. It must add: `installEditFinalizers(root)` on its page, batches around multi-key actions (page reset, Reset advanced, tint-pair reset), and nothing else. Kit history (`alkithistory.cpp`) is widget-level; setting-bound rows are not recorded there (`alkitrow.cpp:297-304` records only code-bound rows).
- Scope caveat: `RenderExposure` is in the same stack (R13).

### 3.4 Moon-linked logic

- Consumer: `body_ev = sun_up ? sun : (linked ? sun : moon)` (`alenvintensity.cpp:255-256`). Moon tint only when `!sun_up` (`:262-263`).
- Floater: label flip and Moon widget enable in `updateEnvIntensityMoonLink()` (`llfloaterenvironmentadjust.cpp:1184-1199`; strings XML 26-27; widgets `env_moon_ev`, `env_moon_ev_unit`, `env_moon_ev_reset`).
- Reset advanced re-links the moon (`:489`). Presets carry `mMoonLinked` (table column; `moonlit_night` sets false).
- Director: no `disabled_control` and no dynamic label in the generator (`director_kit_gen.py:509-510`, `:549-550` emit `enabled_control` only; kit `disabled` is static at `:507`, `:547`). Code is needed for the flip and the enable rule (R10).

### 3.5 Advanced toggle

- Floater: `env_adv_toggle` (XML 773) toggles `AlchemyEnvIntensityAdvanced` (persist 1, default 0, `settings_alchemy.xml:4334-4339`). The flag is not undo-tracked, not in presets, and not in Reset advanced (`:129-131`).
- Kit: advanced rows are hidden behind the kit "More" control (`alkitrow.cpp`, `isEffectivelyVisible` at 645; `WIRING_AUDIT_light_actors.md` section 2). Whether the kit's "More" state persists is `unknown`. Binding the PL flag needs a code-bound toggle (R11).

### 3.6 Colour swatches (Color4)

- Settings are Color4 (RGBA). The consumer uses only the RGB channels (`alenvintensity.cpp:176-212` reads `mV[0..2]`; alpha ignored). Defaults are white `(1,1,1,1)` (`settings_alchemy.xml:4242-4256`, `:4269-4283`, `:4296-4310`).
- Floater: `LLColorSwatchCtrl` with `control_name` and `can_apply_immediately="true"` (XML 1142-1150, 1191-1199, 1240-1248).
- Kit: `color` kind maps to `color_swatch` (`director_kit_gen.py:54-63`). Binding handles `TYPE_COL4` (`alkitbinding.cpp:40-52`). Swatch size 40x18 (`alkitrow.cpp:515-518`).
- Undo: swatches are not finalized (`installEditFinalizers` covers sliders and spinners only), so picks coalesce by the 500 ms idle rule (R16). Picker-cancel behaviour: `unknown`.
- Tint rules (`alenvintensity.cpp:176-212`): Kelvin ratio times swatch, luminance-normalised, max channel limited to 4, strength 0..1 (`:185`).

## 4. Proposed kit layout

Page: `light_group/exposure_tab`, generated kit page (no legacy embed). Preset strip: `preset: {mode: provider, provider: env_intensity}` (needs the provider, R5). No hosted floater panel: `LLFloaterEnvironmentAdjust` members are not reusable, so every PL row is setting-bound except the rows marked code.

Column key: **bind** = setting (`s`) or code. **Range** = legacy UI range unless noted. **dp/step** = decimals / increment. Help text is one line and proposed.

### 4.1 Cards in order

| # | card id (title) | row widget | kind | label | bind | range | dp / step | default | help (one line, proposed) | show_if / enable |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | `exp_history` ("Exposure history") | `exp_undo` | button | Undo | code: `ALExposureSettingsHistory::undo()` | n/a | n/a | n/a | "Undoes the last Personal Lighting exposure change." | tooltip "Undo: <label>"; enabled by `canUndo()` |
| | | `exp_redo` | button | Redo | code: `redo()` | n/a | n/a | n/a | "Redoes the last undone exposure change." | enabled by `canRedo()` |
| 2 | `scene_light` ("Light intensity") | `exp_auto_exposure` | toggle | Auto exposure | `RenderDynamicExposureEnabled` | Boolean | n/a | on | "Partly adjusts for brightness changes so the light sliders feel softer in dim scenes. Off gives fixed exposure." | none |
| | | `exp_sun_ev` | slider | Sun / Moon EV | `AlchemyEnvSunEV` | -4..6 | 2 / 0.05 | 0 | "Sun brightness in stops; 0 is the sky's stock level. Also drives the moon while Link moon is on." | label is static (decision D2) |
| | | `exp_gi_ev` | slider | Sky light (GI) | `AlchemyEnvSkyGIEV` | -4..4 | 2 / 0.05 | 0 | "Master for sky ambient and reflection-probe light together. 0 is stock." | none |
| | | `exp_local_ev` | slider | Local lights | `AlchemyEnvLocalLightEV` | -4..4 | 2 / 0.05 | 0 | "Brightness offset for every point, spot and projector light. Rig lights need the option below." | none |
| | | `exp_shadow_lift` | slider | Shadow lift | `AlchemyEnvShadowLiftEV` | 0..2 | 2 / 0.05 | 0 | "Lifts ambient light where the sun or moon does not reach. 0 is off." | none |
| | | `exp_sun_note` | readout | (none) | code (adapter text) | n/a | n/a | n/a | "Legacy or classic sky: Sun and Moon boosts are capped or inactive. Set Reflection Probe Ambiance above 0 to enable boosts." | show_if `sun_cap_note` (adapter, R14) |
| | | `exp_hint` | readout | (none) | none | n/a | n/a | n/a | "Auto exposure softens changes; off = exact control. Viewer-side, not saved to sky." | none |
| 3 | `light_advanced` ("Advanced light") | `exp_moon_linked` | toggle | Link to Sun | `AlchemyEnvMoonLinked` | Boolean | n/a | on | "On: the Sun / Moon slider drives the moon too. Off: Moon EV takes over at night." | advanced |
| | | `exp_moon_ev` | slider | Moon EV | `AlchemyEnvMoonEV` | -4..6 | 2 / 0.05 | 0 | "Moon brightness in stops, used when Link moon is off and the moon is the main light." | advanced; enabled only when unlinked (code, R10) |
| | | `exp_amb_ev` | slider | Sky ambient | `AlchemyEnvGIAmbientEV` | -3..3 | 2 / 0.05 | 0 | "Sky ambient brightness in stops, added to the Sky light master." | advanced |
| | | `exp_probe_diff_ev` | slider | Probe diffuse | `AlchemyEnvGIProbeDiffuseEV` | -3..3 | 2 / 0.05 | 0 | "Indirect light from reflection probes in stops, added to the Sky light master." | advanced |
| | | `exp_probe_spec_ev` | slider | Probe reflections | `AlchemyEnvGIProbeSpecEV` | -3..3 | 2 / 0.05 | 0 | "Brightness of glossy and metal reflections from probes. Independent of the Sky light master." | advanced |
| | | `exp_local_include_rig` | toggle | Rig lights | `AlchemyEnvLocalLightIncludeRig` | Boolean | n/a | off | "Also applies Local lights to Cine Light Rig lights." | advanced |
| | | `exp_sun_kelvin` | slider | Sun temperature (K) | `AlchemyEnvSunKelvin` | 1000..15000 (D1; legacy UI 2000..12000) | 0 / 50 | 6500 | "Colour temperature of direct sun light on surfaces. 6500 K = no change." | advanced |
| | | `exp_sun_tint` | colour | Sun tint | `AlchemyEnvSunTintColor` (Color4) | sRGB | n/a | white | "Tint on direct sun light. White or strength 0 = no tint; Kelvin still applies." | advanced; reset pairs with the strength row (code, R4) |
| | | `exp_sun_tint_strength` | slider | Sun tint strength | `AlchemyEnvSunTintStrength` | 0..1 | 3 / 0.01 | 1 | "Strength of the sun tint, 0 to 1. 0 = no tint." | advanced |
| | | `exp_moon_tint` | colour | Moon tint | `AlchemyEnvMoonTintColor` | sRGB | n/a | white | "Tint on direct moon light at night. White or strength 0 = no tint." | advanced; reset pairs (code) |
| | | `exp_moon_tint_strength` | slider | Moon tint strength | `AlchemyEnvMoonTintStrength` | 0..1 | 3 / 0.01 | 1 | "Strength of the moon tint, 0 to 1. 0 = no tint." | advanced |
| | | `exp_amb_tint` | colour | Ambient tint | `AlchemyEnvAmbientTintColor` | sRGB | n/a | white | "Tint on sky ambient and probe irradiance: shadow side, interiors, corners." | advanced; reset pairs (code) |
| | | `exp_amb_tint_strength` | slider | Ambient tint strength | `AlchemyEnvAmbientTintStrength` | 0..1 | 3 / 0.01 | 1 | "Strength of the ambient tint, 0 to 1. 0 = no tint." | advanced |
| | | `exp_adv_reset_all` | button | Reset advanced | code: batch of the 14 keys (`:132-147`) + `AlchemyEnvMoonLinked` true | n/a | n/a | n/a | "Resets every advanced control above to its default. The Sun / Moon and Sky light sliders are not changed." | advanced |
| 4 | `exposure_main` ("Camera exposure", kept) | `exp_exposure` | slider | Exposure | `RenderExposure` | 0.5..4 | 1 / 0.1 | 1 | "Overall image brightness sent to the tonemapper, from 0.5 to 4." | none |
| | | `exp_tonemap_type` | combo | Tone mapper | `AlchemyRenderTonemapType` | 9 items (0..8) | n/a | 7 | "Algorithm that maps HDR light into the display range." | none |
| | | `exp_tonemap_mix` | slider | Tone strength | `RenderTonemapMix` | 0..1 | 1 / 0.1 | 0.7 | "Blend between linear and tone-mapped colour: 0 is linear, 1 is fully tone-mapped." | none |
| | | `exp_amd_exposure` | slider | AMD exposure | `AlchemyToneMapAMDExposure` | 1..16 (mockup) | 2 / 0.1 | 7.4 | "Exposure for the AMD LPM tonemapper only (type 7); other tonemappers ignore it." | advanced; enabled only on type 7 |
| | | `exp_slog_ev` | slider | S-Log EV | `AlchemyToneMapSLogEV` | -8..8 (mockup) | 1 / 0.1 | 0 | "Exposure offset in stops before the log curve. S-Log3 Capture only (type 8)." | advanced; enabled only on type 8 |
| | | `exp_legacy_gamma_skies` | toggle | Legacy gamma | `AlchemyTonemapLegacyGammaSkies` | Boolean | n/a | off | "Keeps tonemapping active on skies with probe ambiance 0, using their old gamma curve." | advanced |
| 5 | `sky_settings` ("Per-sky settings", kept) | `exp_sky_note` | readout | Per-sky values | none | n/a | n/a | n/a | as now | none |
| | | `exp_sky_lighting_btn` | button | Per-sky look | target `floater:env_adjust_snapshot` | n/a | n/a | n/a | "Opens Personal Lighting, where Scene Gamma and Reflection Probe Ambiance are set for each sky." | none |
| 6 | `rig_exposure` (link, kept) | `exp_rig_master_ev` | number | Master EV | `CineLightRigMasterEV` | -16..16 | 2 / 0.25 | 0 | "Master exposure in stops, applied to every rig light." | none |
| | | `exp_rig_headroom` | number | Headroom | `CineLightRigHeadroomStops` | 0..16 | 2 / 0.25 | 2 | "Stops kept free below the viewer's light-intensity ceiling." | none |
| 7 | `exp_night_mask` (link, kept, FIXED) | `exp_nm_enabled` | toggle | Enable | `CineLightRigNightMaskEnabled` | Boolean | n/a | off | "Darkens everything outside a shape around the target subject. Shape, distance and tint are on the Rig page." | none |
| | | `exp_nm_darkness` | slider | Darkness | **code**: percent 0..100; write `1 - p/100` to `CineLightRigNightMaskDarkness`; read `(1 - floor) * 100` (as hub) | 0..100 | 0 / 1 | 85 (= floor 0.15) | "Same control as the Rig page Night Mask darkness, in percent." | none. Not a raw setting row (R1) |
| 8 | `lens_filters` (link, decision D4) | `exp_gnd_enabled`, `exp_pol_enabled`, `exp_ir_enabled`, `exp_softedge_enabled` | toggle x4 | Enable | `RenderGradNDEnabled`, `RenderPolarizerEnabled`, `RenderInfraredEnabled`, `RenderSoftEdgeEnabled` | Boolean | n/a | off | "Neutral-density gradient before tonemapping, to hold a blown-out sky. Off costs nothing." (one line per filter) | none; or link to `lens_group/filters_tab` |
| 9 | `glow_filters` (kept) | `exp_bloom_strength` | slider | Bloom strength | `RenderBloomStrength` | 0..2 | 3 / 0.01 | 0.325 | "HDR bloom strength, multiplied before bloom is added to the scene." | enabled by `RenderBloomHDR` |
| | | `exp_bloom_threshold` | slider | Threshold | `RenderBloomThreshold` | 0..4 | 2 / 0.05 | 2.5 | "Linear brightness above which light blooms. 1 is roughly brighter than white." | enabled by `RenderBloomHDR` |
| | | `exp_filters_btn` | button | All filters | target `lightbox:colortone_settings` | n/a | n/a | n/a | "Opens every Lens Looks control in the Lightbox Image tab." | none |
| 10 | `exposure_window` (kept) | `exposure_popout` | button | Open window | target `floater:exposure_hub` | n/a | n/a | n/a | "Opens the Exposure window." | none |

### 4.2 Decisions the layout depends on

- **D1** Kelvin range: keep legacy 2000..12000 (matches preset sanitize) or widen to the consumer's 1000..15000. The legacy range hides consumer-accepted values, so the layout proposes 1000..15000. Widening also means changing `alenvintensitypresets.cpp:73`.
- **D2** Sun/Moon label: static "Sun / Moon EV" (mockup) or dynamic "Sun EV" when unlinked (needs code).
- **D3** Preset Save/Delete: use the strip's Save/Delete (needs provider checks, R8) or set `canSaveUser` false and open the PL Presets floater from a button.
- **D4** Lens filters: four inline cards (as curated `light_group.json`) or one link card (as repo mockup).
- **D5** Advanced state: kit "More" only, or bind `AlchemyEnvIntensityAdvanced` (code-bound toggle).
- **D6** Auto exposure: keep in the Light card only, or also in Camera.
- **D7** RLV: gate the strip under `setenv` like the PL menu item, or not (R18).

## 5. Risks

Severity: H = breaks a stated contract or writes wrong values; M = behaviour divergence or extra work; L = cosmetic or moot after regeneration.

- **R1 (H) Night Mask darkness semantics.** The repo mockup row `exp_nm_darkness` (exposure_tab / exp_night_mask_link) binds `CineLightRigNightMaskDarkness` as a 0..100 percent slider. The setting is the floor 0..1 with default 0.15 (`settings.xml:2390-2395`; comment "0 black, 1 no darkening"). The hub converts (`alpanelexposurehub.cpp` `onNightMaskDarknessCommit`, `syncNightMaskDarkness`). A raw row would show 0.15 as "0.15 %" and write 85 into the floor. Fix: code-bound row with the same conversion (section 4.1, row 7).
- **R2 (H) No edit finalizers on kit sliders.** No `alkit*.cpp` or `llfloaterdirector.cpp` call installs `ALExposureSettingsHistory::installEditFinalizers` (grep: 0). A drag with a pause longer than 500 ms (`alsettingsundohistorymodel.h:33`, idle close `.cpp:95-102`) becomes several undo steps. Fix: call `installEditFinalizers(root)` in the binder.
- **R3 (H) Unbatched resets.** `ALKitPage::performPageReset()` (`alkitpage.cpp:1493`) and per-row reset (`alkitrow.h:74-78`, `ALKitSettingBinding::reset()` at `alkitbinding.cpp:81-85`) call `resetToDefault` per key with no `ScopedBatch`. A page reset creates one undo step per changed key. The floater and hub batch their resets (`llfloaterenvironmentadjust.cpp:422-491`; `alpanelexposurehub.cpp:89-95`). The kit's own history cannot undo setting rows (`alkithistory.cpp` is widget-level), so the exposure undo is the only undo.
- **R4 (H) Tint pairs reset half.** A setting-bound row resets only its own setting. Binding priority is setting first, then the reset-settings list (`alkitrow.cpp:764-767`); the multi-setting reset applies only to code-bound rows (`:714`). Swatch reset without strength (or the reverse) leaves a partial tint. Fix: code-bound batched reset of both (floater pattern `llfloaterenvironmentadjust.cpp:157-168`, `:460-476`).
- **R5 (H) Preset store split.** The kit strip uses `ALKitSnapshotStore` (`alkitpresets.h:40`; path `director_presets/<key>.xml`, `alkitpresets.cpp:202-203`). PL presets live in `light_intensity_presets.xml` (`alenvintensitypresets.cpp:44,174-177`). No `env_intensity` provider is registered (provider registration block `llfloaterdirector.cpp:611-627` has none). A snapshot-store provider would create a second list. Fix: provider wrapping `ALEnvIntensityPresets` (interface `alkitpresets.h:10-24`; model on `ALWeatherPresetProvider`, `llfloaterdirector.cpp:459-489`).
- **R6 (H) Strip never re-reads provider state.** `ALKitPresetStrip` refreshes only on its own actions and on tracked-setting modified flips (`alkitpresets.cpp:265`, `:340`, `:369`, `:403`, `:432`). `ALKitPresetProvider::revision()` is declared (`alkitpresets.h:26`) but never called in `alkitpresets.cpp` (grep). A preset saved in the PL floater will not appear in the Director strip until the next refresh.
- **R7 (M) Modified-state baseline differs.** The strip's "(modified)" and "Revert to X" state uses its own `captureApplied` snapshot (`alkitpresets.cpp:409-441`). A preset applied from the PL floater (`llfloaterenvironmentadjust.cpp:1244-1263`) is not an apply event for the strip, so the strip can show a stale label. Custom detection differs: floater exact match with 1e-4 (`:1330-1366`), strip snapshot compare (`alkitpresets.cpp:436-449`).
- **R8 (M) Save/Delete UX differs.** Strip `save()` (`alkitpresets.cpp:355-371`) has no overwrite confirmation, reserved-name check, limit, or read-only/busy notice. The PL floater has all of them (`alfloaterenvintensitypresets.cpp:154-232`; result codes `alenvintensitypresets.h:72-88`). Strip delete uses the generic yes/cancel (`alkitpresets.cpp:373-407`). Fix per D3.
- **R9 (M) Two sources of truth for built-ins.** The C++ table is in an anonymous namespace (`llfloaterenvironmentadjust.cpp:186-225`). The XUI combo items are separate (`floater_adjust_environment.xml:734-748`). The provider needs the table; move it into a shared header next to the 16-value struct (`alenvintensitypresets.h`) and generate or check the XUI list against it.
- **R10 (M) Moon rules not expressible.** `enabled_control` is the only enable attribute the generator emits (`director_kit_gen.py:509-510`, `:549-550`). No `disabled_control`, no dynamic label. Without code the Moon EV stays enabled while linked, and the Sun row label does not flip (`llfloaterenvironmentadjust.cpp:1184-1199`).
- **R11 (M) Two Advanced states.** Kit "More" versus the persisted `AlchemyEnvIntensityAdvanced` (`settings_alchemy.xml:4334`). Decide D5.
- **R12 (M) Clamp mismatches.** GI master plus ambient or probe diffuse can reach ±7; the consumer clamps each sum to ±6 (`alenvintensity.cpp:258-259`, `:333`), silently. Kelvin: UI 2000..12000 hides 1000..2000 and 12000..15000 that the consumer accepts (`:82`); preset `sanitize` clamps Debug values to the UI range on save (`alenvintensitypresets.cpp:181-204`).
- **R13 (M) Undo scope includes camera exposure.** `RenderExposure` is tracked by the PL undo (`alsettingsundohistory.cpp:46`). A PL Undo can revert a Lightbox or Preferences exposure change. Decide: keep the single stack, or remove `RenderExposure` from it and give camera exposure its own history.
- **R14 (M) Live-sky gating.** The floater disables the whole strip when there is no live sky (`llfloaterenvironmentadjust.cpp:691-697`), and computes the legacy/classic note from the captured sky (`:757-800`, `:1102-1128`). A Director adapter for `exp_sun_note` needs the same capture, or the note can be wrong. Which sky the Director should read is `unknown`.
- **R15 (M) Three writers of the same controls.** PL floater, hub Light tab (`panel_exposure_hub.xml:32-54`, no Kelvin or tints), and the Director kit. All write through settings, so no write is lost (`alsettingsundohistory.cpp:68` guards only the history). The hub and the floater will drift from the Director unless one is retired (D-level decision; out of scope here).
- **R16 (L) Colour swatch history.** `installEditFinalizers` covers only sliders and spinners (`alsettingsundohistory.cpp:261-290`). Colour picks coalesce by idle. Picker-cancel behaviour: `unknown`.
- **R17 (L) Per-frame cost.** The floater refreshes the combo only on dirty frames (`llfloaterenvironmentadjust.cpp:1368-1376`; dirty set by watchers `:232-250`). During a drag that is one refresh per frame per open floater (16 reads, up to about 15 compares). The Director must not add `draw()` work; use signals and the 10 Hz cap from `pages/exposure_tab.md` (Live state).
- **R18 (M) RLV.** "Personal Lighting..." is gated by `RLV.EnableIfNot setenv` (`menu_viewer.xml:404-410`). "Light Intensity Presets..." is not (`:414-418`). No RLV check was found in `llfloaterenvironmentadjust.cpp`. Whether the Director strip must respect `setenv` is `unknown` (D7).
- **R19 (M) Generator gates and pinned counts.** `EXPECTED_SHORTCUTS` (`director_kit_gen.py:37-52`, checked at `:241`) must gain the five `image_tab` → `exposure_tab` shortcuts (approved 10/13; `kit_pages.json:195-196` omit_rows and note; comment `director_kit_gen.py:46-47`). Classification pins (`:1191-1215`, counts at `:1212` and `:1279`) move kit 23 → 24 and legacy 13 → 12. The row-count pin `:230-231` (37 pages / 2072 rows) changes. The working tree already has uncommitted Director Settings edits to these pins; coordinate with that work.
- **R20 (L) Binder cleanup.** `exposure_tab` routes to `bindCameraPage` (`llfloaterdirector.cpp:1391`). `director_exposure_panel` code at `:1118-1123`, `:2063-2067`, `:2346` must be retired. `exposure_tab` is in the unwrapped list (`:1114`). `director_page_sources.py:299` names `ALExposureKitAdapter`, which does not exist under `indra/` (grep). Same pattern as the Actors precedent (`WIRING_AUDIT_light_actors.md`, risk 9).
- **R21 (L) Legacy watch list.** `panel_dpage_light_exposure.xml:3` `watch_settings` lacks `AlchemyEnvSunKelvin` and the six tint keys. Moot after regeneration.
- **R22 (L) `has_reset` is inert.** The curated `light_group.json` rows carry `has_reset: true`. Neither `director_kit_gen.py` nor `director_mockup_data.py` reads that key (grep). Resets come from `reset_setting` (`director_kit_gen.py:446-454`) and the setting binding. Do not rely on it.

## 6. Work list for the conversion

1. Curated source (`C:\Users\xianw\cxs\content_curated`): add Kelvin, the three tint pairs, the Advanced card, the Presets provider and Reset advanced to `light_group.json` exposure_tab; add the legacy/classic note; fix `exp_nm_darkness` (R1). Regenerate `director_mockup_data.json` via `director_mockup_data.py`.
2. `scripts/code_tools/director_kit/kit_pages.json`: add a `light_group/exposure_tab` `p0_pages` entry with `preset: {mode: provider, provider: env_intensity}`, row overrides (bulk_reset for resets that must not run on page reset is decision D8, see section 7), and `add_rows` if needed. Remove the `image_tab` `omit_rows` (`:195`) and its note (`:196`) once the shortcuts exist.
3. `scripts/code_tools/director_kit_gen.py`: add the five shortcuts to `EXPECTED_SHORTCUTS` (`:37-52`); update the classification table and counts (`:1191-1215`, `:1212`, `:1279`); if `disabled_control` or a code-bound enable is needed, extend `row_element` (`:593-660`).
4. `scripts/code_tools/director_page_sources.py`: add `floater_adjust_environment.xml` as an owner for `exposure_tab` (`:62`, `:102`); the adapter name `ALExposureKitAdapter` (`:299`) must exist or be renamed.
5. C++ provider: implement `env_intensity` over `ALEnvIntensityPresets` (builtin table moved out of `llfloaterenvironmentadjust.cpp:186-225`); register it next to `registerLookPresetProviders()` (`llfloaterdirector.cpp:611-627`); bump `revision()` on `connectListChanged` / `connectApplied` (R6).
6. C++ strip: refresh when the provider revision changes (R6); align the modified-state baseline with the provider (R7).
7. C++ binder: new `bindExposurePage` replacing `bindCameraPage` for `exposure_tab` (`llfloaterdirector.cpp:1391`); `installEditFinalizers(root)`; batched page/card/tint/advanced resets; Night Mask percent adapter (R1); legacy/classic note adapter using the floater's predicate (R14); Sun/Moon label and Moon enable (R10, D2); retire `director_exposure_panel` code (R20); keep the popout.
8. Regenerate the index `indra/newview/skins/default/xui/en/director_kit_index.xml` (generator-owned; INDEX_FIELDS 22, `director_kit_gen.py:36`).
9. Doc: update `doc/director_kit/pages/exposure_tab.md` (page contract) after the conversion.

## 7. Open decisions and unknowns

- D1 Kelvin range (legacy 2000..12000 vs consumer 1000..15000). See R12.
- D2 Sun/Moon label: static or dynamic.
- D3 Preset Save/Delete in the strip or in the PL Presets floater.
- D4 Lens filters: four inline cards or one link card.
- D5 Advanced state: kit "More" or the persisted PL flag.
- D6 Auto exposure: Light card only or also Camera.
- D7 RLV `setenv` gating of the Director strip (`unknown` whether it should).
- D8 Which PL rows are bulk-resettable by "Reset page" (default true, `alkitpage.cpp:1493` path; setting-bound rows reset). Recommendation: exclude PL rows from page reset, as the actors page does for its persistence toggle (`bulk_reset: false`, `kit_pages.json` actors_tab entry), and keep the explicit Reset advanced.
- D9 `RenderExposure` in the PL undo stack (R13).
- Unknown: whether kit "More" persists; whether the kit slider accepts `installEditFinalizers` without side effects on its own callbacks (no kit callbacks found by grep, so likely none); whether the picker-cancel writes a colour step (R16); whether the Director needs `ALLensLooks` commit semantics for the filter combos; the tooltip claim that S-Log3 ignores auto exposure.
- Not run: `xui_layout_check`, the generator, any build, any viewer run.
