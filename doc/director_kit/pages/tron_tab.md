# Tron — Director Kit page contract

Page key: `tron_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (124/132 setting-bound rows; 103 C++-referenced rows).

## Real owners

- `floater_lightbox_settings` / `floater` at `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:8` (130 matching rows).
- Referencing classes: `ALFloaterLightBox` (106), `ALTron` (1).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALTronKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `tron_master` / `tr_enable` (Enable Tron) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5862` | kit `toggle` ↔ `TronEnabled` `Boolean` | none |
| `tron_master` / `tr_layer_combo` (Layer) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5864` | kit `combo` ↔ `TronLayer` `S32` | none |
| `tron_master` / `tr_palette_combo` (Palette) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5891` | kit `combo` ↔ `TronPalette` `S32`; `ALTronKitAdapter` preserves/calls wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onTronPaletteSelected()` (`indra/newview/alfloaterlightbox.cpp:104`) | none |
| `tron_master` / `tr_color_primary` (Primary) | `color_swatch` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5906` | kit `color` ↔ `TronColorPrimary` `Color3`; `ALTronKitAdapter` preserves/calls wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onTronColorEdited()` (`indra/newview/alfloaterlightbox.cpp:105`) | none |
| `tron_master` / `tr_color_secondary` (Secondary) | `color_swatch` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5909` | kit `color` ↔ `TronColorSecondary` `Color3`; `ALTronKitAdapter` preserves/calls wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onTronColorEdited()` (`indra/newview/alfloaterlightbox.cpp:105`) | none |
| `tron_master` / `tr_color_accent` (Accent) | `color_swatch` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5912` | kit `color` ↔ `TronColorAccent` `Color3`; `ALTronKitAdapter` preserves/calls wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onTronColorEdited()` (`indra/newview/alfloaterlightbox.cpp:105`) | none |
| `tron_master` / `tr_color_pulse` (Pulse) | `color_swatch` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5915` | kit `color` ↔ `TronColorPulse` `Color3`; `ALTronKitAdapter` preserves/calls wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onTronColorEdited()` (`indra/newview/alfloaterlightbox.cpp:105`) | none |
| `tron_master` / `tr_also_roto` (+ Roto Ink) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5888` | kit `toggle` ↔ `TronPresetAlsoRoto` `Boolean` | none |
| `tron_master` / `tr_also_rigrim` (+ Rig Rim) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5889` | kit `toggle` ↔ `TronPresetAlsoRigRim` `Boolean` | none |
| `tron_master` / `tr_apply_roto_preset_btn` (Roto preset) | `button` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6335` | `ALTronKitAdapter` command/model binding → wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onTronApplyRotoPreset()` (`indra/newview/alfloaterlightbox.cpp:108`) | none |
| `tron_master` / `tr_apply_rigrim_preset_btn` (Rim preset) | `button` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6336` | `ALTronKitAdapter` command/model binding → wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onTronApplyRigRimPreset()` (`indra/newview/alfloaterlightbox.cpp:109`) | none |
| `tron_master` / `tr_reset_all` (Reset all) | `button` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5938` | `ALTronKitAdapter` command/model binding → wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onTronPreset()` (`indra/newview/alfloaterlightbox.cpp:103`) | none |
| `clock` / `tr_anim_speed_slider` (Speed) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5920` | kit `slider` ↔ `TronAnimSpeed` `F32` | none |
| `clock` / `tr_anim_pause_check` (Pause) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5922` | kit `toggle` ↔ `TronAnimPause` `Boolean` | none |
| `clock` / `tr_pulse_rate_slider` (Pulse Hz) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5925` | kit `slider` ↔ `TronPulseRate` `F32` | none |
| `clock` / `tr_pulse_shape_combo` (Pulse shape) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5928` | kit `combo` ↔ `TronPulseShape` `S32` | none |
| `clock` / `tr_pulse_amount_slider` (Pulse amount) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5935` | kit `slider` ↔ `TronPulseAmount` `F32` | none |
| `clock` / `tr_sync_roto_check` (Sync Roto Ink) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5937` | kit `toggle` ↔ `CineOutlineAnimUseTronClock` `Boolean` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5830` `ri_sync_tron_check` |
| `clock` / `tr_anim_follow_check` (Director time) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5923` | kit `toggle` ↔ `TronAnimFollowDirector` `Boolean` | none |
| `grade` / `tr_grade_strength_slider` (Strength) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5942` | kit `slider` ↔ `TronGradeStrength` `F32` | none |
| `grade` / `tr_grade_darken_ev_slider` (Darken EV) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5945` | kit `slider` ↔ `TronGradeDarkenEV` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:919`) | none |
| `grade` / `tr_grade_desaturate_slider` (Desaturate) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5948` | kit `slider` ↔ `TronGradeDesaturate` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:920`) | none |
| `grade` / `tr_grade_crush_slider` (Crush) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5951` | kit `slider` ↔ `TronGradeCrush` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:921`) | none |
| `grade` / `tr_grade_sky_darken_slider` (Sky darken) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5969` | kit `slider` ↔ `TronGradeSkyDarken` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:927`) | none |
| `grade` / `tr_grade_tint_swatch` (Grade tint) | `color_swatch` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5954` | kit `color` ↔ `TronGradeTint` `Color3`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:922`) | none |
| `grade` / `tr_grade_tint_amount_slider` (Tint amount) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5957` | kit `slider` ↔ `TronGradeTintAmount` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:923`) | none |
| `grade` / `tr_grade_keep_bright_lo_slider` (Keep bright lo) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5960` | kit `slider` ↔ `TronGradeKeepBrightLo` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:924`) | none |
| `grade` / `tr_grade_keep_bright_hi_slider` (Keep bright hi) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5963` | kit `slider` ↔ `TronGradeKeepBrightHi` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:925`) | none |
| `grade` / `tr_grade_keep_subject_slider` (Keep subject) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5966` | kit `slider` ↔ `TronGradeKeepSubject` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:926`) | none |
| `grid` / `tr_grid_enable_check` (Enable grid) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5973` | kit `toggle` ↔ `TronGridEnabled` `Boolean` | none |
| `grid` / `tr_grid_intensity_slider` (Intensity) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5975` | kit `slider` ↔ `TronGridIntensity` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:935`) | none |
| `grid` / `tr_grid_spacing_slider` (Spacing m) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5978` | kit `slider` ↔ `TronGridSpacing` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:936`) | none |
| `grid` / `tr_grid_width_slider` (Width m) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5981` | kit `slider` ↔ `TronGridWidth` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:937`) | none |
| `grid` / `tr_grid_glow_slider` (Glow feed) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6024` | kit `slider` ↔ `TronGridGlow` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:950`) | none |
| `grid` / `tr_grid_min_width_px_slider` (Min width px) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5984` | kit `slider` ↔ `TronGridMinWidthPx` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:938`) | none |
| `grid` / `tr_grid_major_every_spinner` (Major every) | `spinner` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5987` | kit `number` ↔ `TronGridMajorEvery` `S32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:939`) | none |
| `grid` / `tr_grid_major_width_slider` (Major width x) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5989` | kit `slider` ↔ `TronGridMajorWidth` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:940`) | none |
| `grid` / `tr_grid_major_intensity_slider` (Major int x) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5992` | kit `slider` ↔ `TronGridMajorIntensity` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:941`) | none |
| `grid` / `tr_grid_floor_slider` (Floor) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5995` | kit `slider` ↔ `TronGridFloor` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:942`) | none |
| `grid` / `tr_grid_wall_slider` (Wall) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5998` | kit `slider` ↔ `TronGridWall` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:943`) | none |
| `grid` / `tr_grid_ceiling_slider` (Ceiling) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6001` | kit `slider` ↔ `TronGridCeiling` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:944`) | none |
| `grid` / `tr_grid_sharpness_slider` (Sharpness) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6004` | kit `slider` ↔ `TronGridSharpness` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:945`) | none |
| `grid` / `tr_grid_normal_source_combo` (Normal source) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6007` | kit `combo` ↔ `TronGridNormalSource` `S32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:946`) | none |
| `grid` / `tr_grid_far_fade_slider` (Far fade m) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6012` | kit `slider` ↔ `TronGridFarFade` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:947`) | none |
| `grid` / `tr_grid_water_mode_combo` (Water mode) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6015` | kit `combo` ↔ `TronGridWaterMode` `S32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:948`) | none |
| `grid` / `tr_grid_water_tolerance_slider` (Water tol m) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6021` | kit `slider` ↔ `TronGridWaterTolerance` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:949`) | none |
| `grid` / `tr_grid_subject_exclude_slider` (Subject excl) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6302` | kit `slider` ↔ `TronGridSubjectExclude` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1049`) | none |
| `grid` / `tr_grid_subject_radius_slider` (Subject radius) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6305` | kit `slider` ↔ `TronGridSubjectRadius` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1050`) | none |
| `grid` / `tr_grid_subject_radius_feather_slider` (Radius feather) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6308` | kit `slider` ↔ `TronGridSubjectRadiusFeather` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1051`) | none |
| `grid` / `tr_grid_origin_snap_btn` (Origin) | `button` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6027` | `ALTronKitAdapter` command/model binding → behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:951`), wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onTronGridOriginSnap()` (`indra/newview/alfloaterlightbox.cpp:106`) | none |
| `grid` / `tr_grid_origin_clear_btn` (Origin clear) | `button` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6028` | `ALTronKitAdapter` command/model binding → behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:951`), wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onTronGridOriginClear()` (`indra/newview/alfloaterlightbox.cpp:107`) | none |
| `pulses` / `tr_pulse_grid_amount_slider` (Amount) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6032` | kit `slider` ↔ `TronPulseGridAmount` `F32` | none |
| `pulses` / `tr_pulse_grid_speed_slider` (Speed m/s) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6035` | kit `slider` ↔ `TronPulseGridSpeed` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:960`) | none |
| `pulses` / `tr_pulse_grid_length_slider` (Length) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6038` | kit `slider` ↔ `TronPulseGridLength` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:961`) | none |
| `pulses` / `tr_pulse_grid_wavelength_slider` (Wavelength m) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6041` | kit `slider` ↔ `TronPulseGridWavelength` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:962`) | none |
| `pulses` / `tr_pulse_grid_density_slider` (Density) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6044` | kit `slider` ↔ `TronPulseGridDensity` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:963`) | none |
| `pulses` / `tr_pulse_grid_direction_combo` (Direction) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6047` | kit `combo` ↔ `TronPulseGridDirection` `S32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:964`) | none |
| `pulses` / `tr_pulse_grid_color_mode_combo` (Pulse colour) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6054` | kit `combo` ↔ `TronPulseGridColorMode` `S32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:965`) | none |
| `pulses` / `tr_pulse_seed_slider` (Seed) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6060` | kit `slider` ↔ `TronPulseSeed` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:966`) | none |
| `traces` / `tr_trace_enable` (Enable traces) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6064` | kit `toggle` ↔ `TronTraceEnabled` `Boolean` | none |
| `traces` / `tr_trace_intensity_slider` (Intensity) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6066` | kit `slider` ↔ `TronTraceIntensity` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:983`) | none |
| `traces` / `tr_trace_density_slider` (Density) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6075` | kit `slider` ↔ `TronTraceDensity` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:986`) | none |
| `traces` / `tr_trace_cell_slider` (Cell m) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6069` | kit `slider` ↔ `TronTraceCell` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:984`) | none |
| `traces` / `tr_trace_width_slider` (Width m) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6072` | kit `slider` ↔ `TronTraceWidth` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:985`) | none |
| `traces` / `tr_trace_pad_radius_slider` (Pad radius m) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6078` | kit `slider` ↔ `TronTracePadRadius` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:987`) | none |
| `traces` / `tr_trace_diagonal_slider` (Diagonal) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6081` | kit `slider` ↔ `TronTraceDiagonal` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:988`) | none |
| `traces` / `tr_trace_walls_slider` (Walls) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6084` | kit `slider` ↔ `TronTraceWalls` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:989`) | none |
| `traces` / `tr_trace_floors_slider` (Floors) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6087` | kit `slider` ↔ `TronTraceFloors` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:990`) | none |
| `traces` / `tr_trace_pulse_amount_slider` (Pulse amt) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6090` | kit `slider` ↔ `TronTracePulseAmount` `F32` | none |
| `traces` / `tr_trace_pulse_speed_slider` (Pulse cells/s) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6093` | kit `slider` ↔ `TronTracePulseSpeed` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:997`) | none |
| `traces` / `tr_trace_pulse_length_slider` (Pulse length) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6096` | kit `slider` ↔ `TronTracePulseLength` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:998`) | none |
| `traces` / `tr_trace_seed_slider` (Seed) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6099` | kit `slider` ↔ `TronTraceSeed` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:999`) | none |
| `rim` / `tr_rim_enable` (Enable rim) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6103` | kit `toggle` ↔ `TronRimEnabled` `Boolean` | none |
| `rim` / `tr_rim_gain_slider` (Gain) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6105` | kit `slider` ↔ `TronRimGain` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1014`) | none |
| `rim` / `tr_rim_exponent_slider` (Exponent) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6108` | kit `slider` ↔ `TronRimExponent` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1015`) | none |
| `rim` / `tr_rim_scan_amount_slider` (Scan amt) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6120` | kit `slider` ↔ `TronRimScanAmount` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1019`) | none |
| `rim` / `tr_rim_scan_speed_slider` (Scan /s) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6123` | kit `slider` ↔ `TronRimScanSpeed` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1028`) | none |
| `rim` / `tr_rim_scan_width_slider` (Scan width) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6126` | kit `slider` ↔ `TronRimScanWidth` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1029`) | none |
| `rim` / `tr_rim_silhouette_gain_slider` (Silhouette) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6111` | kit `slider` ↔ `TronRimSilhouetteGain` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1016`) | none |
| `rim` / `tr_rim_silhouette_threshold_slider` (Sil threshold) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6114` | kit `slider` ↔ `TronRimSilhouetteThreshold` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1017`) | none |
| `rim` / `tr_rim_pulse_amount_slider` (Rim pulse) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6117` | kit `slider` ↔ `TronRimPulseAmount` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1018`) | none |
| `rim` / `tr_rim_color_mode_combo` (Colour mode) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6129` | kit `combo` ↔ `TronRimColorMode` `S32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1020`) | none |
| `rim` / `tr_rim_reject_floors_check` (Reject floors) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6134` | kit `toggle` ↔ `TronRimRejectFloors` `Boolean`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1021`) | none |
| `suit` / `tr_suit_use_palette_check` (Use palette) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6138` | kit `toggle` ↔ `TronSuitUsePalette` `Boolean` | none |
| `suit` / `tr_suit_seam_gain_slider` (Seam gain) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6149` | kit `slider` ↔ `TronSuitSeamGain` `F32` | none |
| `suit` / `tr_suit_rim_gain_slider` (Suit rim) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6152` | kit `slider` ↔ `TronSuitRimGain` `F32` | none |
| `suit` / `tr_suit_seam_cell_slider` (Seam cell m) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6140` | kit `slider` ↔ `TronSuitSeamCell` `F32` | none |
| `suit` / `tr_suit_seam_width_slider` (Seam width m) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6143` | kit `slider` ↔ `TronSuitSeamWidth` `F32` | none |
| `suit` / `tr_suit_seam_density_slider` (Seam density) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6146` | kit `slider` ↔ `TronSuitSeamDensity` `F32` | none |
| `trails` / `tr_trail_enable` (Enable trails) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6156` | kit `toggle` ↔ `TronTrailEnabled` `Boolean` | none |
| `trails` / `tr_trail_style_combo` (Style) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6158` | kit `combo` ↔ `TronTrailStyle` `S32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1069`) | none |
| `trails` / `tr_trail_target_set_combo` (Target set) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6164` | kit `combo` ↔ `TronTrailTargetSet` `S32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1070`) | none |
| `trails` / `tr_trail_intensity_slider` (Intensity) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6185` | kit `slider` ↔ `TronTrailIntensity` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1072`) | none |
| `trails` / `tr_trail_height_slider` (Height m) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6194` | kit `slider` ↔ `TronTrailHeight` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1097`) | none |
| `trails` / `tr_trail_width_slider` (Width m) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6197` | kit `slider` ↔ `TronTrailWidth` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1105`) | none |
| `trails` / `tr_trail_fade_time_slider` (Fade time s) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6204` | kit `slider` ↔ `TronTrailFadeTime` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1076`) | none |
| `trails` / `tr_trail_color_mode_combo` (Colour mode) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6188` | kit `combo` ↔ `TronTrailColorMode` `S32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1073`) | none |
| `trails` / `tr_trail_max_actors_spinner` (Max actors) | `spinner` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6183` | kit `number` ↔ `TronTrailMaxActors` `S32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1071`) | none |
| `trails` / `tr_trail_scale_check` (Scale avatar) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6199` | kit `toggle` ↔ `TronTrailScaleToAvatar` `Boolean`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1074`) | none |
| `trails` / `tr_trail_lift_slider` (Lift m) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6201` | kit `slider` ↔ `TronTrailLift` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1075`) | none |
| `trails` / `tr_trail_fade_curve_slider` (Fade curve) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6207` | kit `slider` ↔ `TronTrailFadeCurve` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1077`) | none |
| `trails` / `tr_trail_edge_slider` (Edge / core) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6210` | kit `slider` ↔ `TronTrailEdge` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1078`) | none |
| `trails` / `tr_trail_edge_gain_slider` (Edge gain) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6213` | kit `slider` ↔ `TronTrailEdgeGain` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1079`) | none |
| `trails` / `tr_trail_body_gain_slider` (Body gain) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6216` | kit `slider` ↔ `TronTrailBodyGain` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1080`) | none |
| `trails` / `tr_trail_glow_slider` (Glow) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6219` | kit `slider` ↔ `TronTrailGlow` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1081`) | none |
| `trails` / `tr_trail_fog_combo` (Fog) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6222` | kit `combo` ↔ `TronTrailFog` `S32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1082`) | none |
| `trails` / `tr_trail_fog_density_slider` (Fog density) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6227` | kit `slider` ↔ `TronTrailFogDensity` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1113`) | none |
| `trails` / `tr_trail_spacing_slider` (Spacing m) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6230` | kit `slider` ↔ `TronTrailSpacing` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1083`) | none |
| `trails` / `tr_trail_min_speed_slider` (Min speed m/s) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6233` | kit `slider` ↔ `TronTrailMinSpeed` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1084`) | none |
| `trails` / `tr_trail_idle_break_slider` (Idle break s) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6236` | kit `slider` ↔ `TronTrailIdleBreak` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1085`) | none |
| `trails` / `tr_trail_break_distance_slider` (Break dist m) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6239` | kit `slider` ↔ `TronTrailBreakDistance` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1086`) | none |
| `trails` / `tr_trail_max_points_spinner` (Max points) | `spinner` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6242` | kit `number` ↔ `TronTrailMaxPoints` `S32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1087`) | none |
| `trails` / `tr_trail_max_segments_spinner` (Max segments) | `spinner` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6244` | kit `number` ↔ `TronTrailMaxSegments` `S32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1088`) | none |
| `trails` / `tr_trail_clear_on_cut_check` (Clear on cut) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6245` | kit `toggle` ↔ `TronTrailClearOnCut` `Boolean`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1089`) | none |
| `trails` / `tr_trail_clear_btn` (Clear trails) | `button` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6246` | `ALTronKitAdapter` command/model binding → behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1090`), wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALTron::clearTrails()` (`indra/newview/alfloaterlightbox.cpp:112`) | none |
| `subject` / `tr_subject_target_set_combo` (Target set) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6250` | kit `combo` ↔ `TronSubjectTargetSet` `S32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1039`) | none |
| `subject` / `tr_subject_target_combo` (Single subj) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6261` | kit `combo` ↔ `TronSubjectTarget` `S32` | none |
| `subject` / `tr_subject_max_targets_spinner` (Max targets) | `spinner` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6269` | kit `number` ↔ `TronSubjectMaxTargets` `S32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1040`) | none |
| `subject` / `tr_subject_shape_combo` (Shape) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6271` | kit `combo` ↔ `TronSubjectShape` `S32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1041`) | none |
| `subject` / `tr_subject_feather_slider` (Feather) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6277` | kit `slider` ↔ `TronSubjectFeather` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1042`) | none |
| `subject` / `tr_subject_depth_range_slider` (Depth range m) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6280` | kit `slider` ↔ `TronSubjectDepthRange` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1043`) | none |
| `subject` / `tr_subject_ellipse_scale_slider` (Box scale) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6283` | kit `slider` ↔ `TronSubjectEllipseScale` `F32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1044`) | none |
| `subject` / `tr_subject_source_grid_combo` (Mask (grid)) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6286` | kit `combo` ↔ `TronSubjectSourceGrid` `S32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1045`) | none |
| `subject` / `tr_subject_source_rim_combo` (Mask (rim)) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6293` | kit `combo` ↔ `TronSubjectSourceRim` `S32`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1046`) | none |
| `subject` / `tr_subject_invert_check` (Invert) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6300` | kit `toggle` ↔ `TronSubjectInvert` `Boolean`; `ALTronKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTronVisibility()` (`indra/newview/alfloaterlightbox.cpp:1048`) | none |
| `integration` / `tr_rigrim_tron_mode_combo` (Rig Rim tint) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6321` | kit `combo` ↔ `CineRigRimTronMode` `S32` | page `light_rim_tab`, `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1070` `cine_rig_rim_tron_mode` |
| `integration` / `tr_rigrim_tron_mix_slider` (Tint mix) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6327` | kit `slider` ↔ `CineRigRimTronMix` `F32` | page `light_rim_tab`, `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1081` `cine_rig_rim_tron_mix` |
| `integration` / `tr_rigrim_tron_color_combo` (Tint colour) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6330` | kit `combo` ↔ `CineRigRimTronColorSource` `S32` | page `light_rim_tab`, `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:1086` `cine_rig_rim_tron_color` |
| `integration` / `tr_no_post_scale_slider` (No-post scale) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6313` | kit `slider` ↔ `TronNoPostScale` `F32` | none |
| `integration` / `tr_bloom_meter_scale_slider` (Bloom meter) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:6316` | kit `slider` ↔ `TronBloomMeterScale` `F32` | none |
| `adv_tron` / `tr_roto_link` (Roto Ink) | none | kit-native action `link_to:look_group/stylize_tab`; no duplicate widget is required | none |
| `adv_tron` / `open_lightbox_tron` (Lightbox ›) | none | kit-native action `floater:lightbox/tron_settings`; no duplicate widget is required | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALFloaterLightBox::onTronApplyRigRimPreset()` — `indra/newview/alfloaterlightbox.cpp:109` | Move model-facing work into `ALTronKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterLightBox::onTronApplyRotoPreset()` — `indra/newview/alfloaterlightbox.cpp:108` | Move model-facing work into `ALTronKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterLightBox::onTronColorEdited()` — `indra/newview/alfloaterlightbox.cpp:105` | Move model-facing work into `ALTronKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterLightBox::onTronGridOriginClear()` — `indra/newview/alfloaterlightbox.cpp:107` | Move model-facing work into `ALTronKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterLightBox::onTronGridOriginSnap()` — `indra/newview/alfloaterlightbox.cpp:106` | Move model-facing work into `ALTronKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterLightBox::onTronPaletteSelected()` — `indra/newview/alfloaterlightbox.cpp:104` | Move model-facing work into `ALTronKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterLightBox::onTronPreset()` — `indra/newview/alfloaterlightbox.cpp:103` | Move model-facing work into `ALTronKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterLightBox::updateTronVisibility()` — `indra/newview/alfloaterlightbox.cpp:919, 920, 921, 922, 923, 924, 925, 926, 927, 935, 936, 937, 938, 939, 940, 941, 942, 943, 944, 945, 946, 947, 948, 949, 950, 951, 960, 961, 962, 963, 964, 965, 966, 983, 984, 985, 986, 987, 988, 989, 990, 997, 998, 999, 1014, 1015, 1016, 1017, 1018, 1019, 1020, 1021, 1028, 1029, 1039, 1040, 1041, 1042, 1043, 1044, 1045, 1046, 1048, 1049, 1050, 1051, 1069, 1070, 1071, 1072, 1073, 1074, 1075, 1076, 1077, 1078, 1079, 1080, 1081, 1082, 1083, 1084, 1085, 1086, 1087, 1088, 1089, 1090, 1097, 1105, 1113` | Move model-facing work into `ALTronKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALTron::clearTrails()` — `indra/newview/alfloaterlightbox.cpp:112` | Move model-facing work into `ALTronKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **120 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-tron_master-tallest-850` | Only `tron_master` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-tron_master-tallest-1160` | Only `tron_master` is put in its tallest local state at 1160 px; all other cards stay default. |
| `tr_enable-on` | Only `tr_enable` is on; dependent visibility is measured. |
| `tr_layer_combo-option-0` | Only `tr_layer_combo` selects `Scene`; other selectors remain default. |
| `tr_layer_combo-option-1` | Only `tr_layer_combo` selects `Camera (default)`; other selectors remain default. |
| `tr_palette_combo-option-0` | Only `tr_palette_combo` selects `Custom`; other selectors remain default. |
| `tr_palette_combo-option-1` | Only `tr_palette_combo` selects `Legacy Cyan`; other selectors remain default. |
| `tr_palette_combo-option-2` | Only `tr_palette_combo` selects `Ares Red`; other selectors remain default. |
| `tr_palette_combo-option-3` | Only `tr_palette_combo` selects `Classic 1982`; other selectors remain default. |
| `tr_palette_combo-option-4` | Only `tr_palette_combo` selects `Recognizer Orange`; other selectors remain default. |
| `tr_palette_combo-option-5` | Only `tr_palette_combo` selects `Uprising White`; other selectors remain default. |
| `tr_palette_combo-option-6` | Only `tr_palette_combo` selects `Rinzler`; other selectors remain default. |
| `tr_palette_combo-option-7` | Only `tr_palette_combo` selects `Siren White`; other selectors remain default. |
| `tr_palette_combo-option-8` | Only `tr_palette_combo` selects `Quorra`; other selectors remain default. |
| `tr_palette_combo-option-9` | Only `tr_palette_combo` selects `Clu Gold`; other selectors remain default. |
| `tr_palette_combo-option-10` | Only `tr_palette_combo` selects `Grid Blue`; other selectors remain default. |
| `tr_also_roto-on` | Only `tr_also_roto` is on; dependent visibility is measured. |
| `tr_also_rigrim-on` | Only `tr_also_rigrim` is on; dependent visibility is measured. |
| `card-clock-tallest-850` | Only `clock` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-clock-tallest-1160` | Only `clock` is put in its tallest local state at 1160 px; all other cards stay default. |
| `tr_anim_pause_check-on` | Only `tr_anim_pause_check` is on; dependent visibility is measured. |
| `tr_pulse_shape_combo-option-0` | Only `tr_pulse_shape_combo` selects `Sine`; other selectors remain default. |
| `tr_pulse_shape_combo-option-1` | Only `tr_pulse_shape_combo` selects `Saw`; other selectors remain default. |
| `tr_pulse_shape_combo-option-2` | Only `tr_pulse_shape_combo` selects `Heartbeat`; other selectors remain default. |
| `tr_pulse_shape_combo-option-3` | Only `tr_pulse_shape_combo` selects `Square`; other selectors remain default. |
| `tr_sync_roto_check-on` | Only `tr_sync_roto_check` is on; dependent visibility is measured. |
| `tr_anim_follow_check-on` | Only `tr_anim_follow_check` is on; dependent visibility is measured. |
| `card-grade-tallest-850` | Only `grade` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-grade-tallest-1160` | Only `grade` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-grid-tallest-850` | Only `grid` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-grid-tallest-1160` | Only `grid` is put in its tallest local state at 1160 px; all other cards stay default. |
| `tr_grid_enable_check-on` | Only `tr_grid_enable_check` is on; dependent visibility is measured. |
| `tr_grid_normal_source_combo-option-0` | Only `tr_grid_normal_source_combo` selects `G-buffer`; other selectors remain default. |
| `tr_grid_normal_source_combo-option-1` | Only `tr_grid_normal_source_combo` selects `Geometric (default)`; other selectors remain default. |
| `tr_grid_water_mode_combo-option-0` | Only `tr_grid_water_mode_combo` selects `Skip`; other selectors remain default. |
| `tr_grid_water_mode_combo-option-1` | Only `tr_grid_water_mode_combo` selects `Sea of Simulation (secondary colour)`; other selectors remain default. |
| `tr_grid_water_mode_combo-option-2` | Only `tr_grid_water_mode_combo` selects `Floor`; other selectors remain default. |
| `card-pulses-tallest-850` | Only `pulses` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-pulses-tallest-1160` | Only `pulses` is put in its tallest local state at 1160 px; all other cards stay default. |
| `tr_pulse_grid_direction_combo-option-0` | Only `tr_pulse_grid_direction_combo` selects `Both ways`; other selectors remain default. |
| `tr_pulse_grid_direction_combo-option-1` | Only `tr_pulse_grid_direction_combo` selects `+ axis`; other selectors remain default. |
| `tr_pulse_grid_direction_combo-option-2` | Only `tr_pulse_grid_direction_combo` selects `- axis`; other selectors remain default. |
| `tr_pulse_grid_direction_combo-option-3` | Only `tr_pulse_grid_direction_combo` selects `Hashed per line`; other selectors remain default. |
| `tr_pulse_grid_color_mode_combo-option-0` | Only `tr_pulse_grid_color_mode_combo` selects `Palette pulse`; other selectors remain default. |
| `tr_pulse_grid_color_mode_combo-option-1` | Only `tr_pulse_grid_color_mode_combo` selects `Secondary`; other selectors remain default. |
| `tr_pulse_grid_color_mode_combo-option-2` | Only `tr_pulse_grid_color_mode_combo` selects `White`; other selectors remain default. |
| `card-traces-tallest-850` | Only `traces` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-traces-tallest-1160` | Only `traces` is put in its tallest local state at 1160 px; all other cards stay default. |
| `tr_trace_enable-on` | Only `tr_trace_enable` is on; dependent visibility is measured. |
| `card-rim-tallest-850` | Only `rim` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-rim-tallest-1160` | Only `rim` is put in its tallest local state at 1160 px; all other cards stay default. |
| `tr_rim_enable-on` | Only `tr_rim_enable` is on; dependent visibility is measured. |
| `tr_rim_color_mode_combo-option-0` | Only `tr_rim_color_mode_combo` selects `Primary`; other selectors remain default. |
| `tr_rim_color_mode_combo-option-1` | Only `tr_rim_color_mode_combo` selects `Per-target`; other selectors remain default. |
| `tr_rim_color_mode_combo-option-2` | Only `tr_rim_color_mode_combo` selects `Secondary`; other selectors remain default. |
| `tr_rim_reject_floors_check-on` | Only `tr_rim_reject_floors_check` is on; dependent visibility is measured. |
| `card-suit-tallest-850` | Only `suit` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-suit-tallest-1160` | Only `suit` is put in its tallest local state at 1160 px; all other cards stay default. |
| `tr_suit_use_palette_check-on` | Only `tr_suit_use_palette_check` is on; dependent visibility is measured. |
| `card-trails-tallest-850` | Only `trails` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-trails-tallest-1160` | Only `trails` is put in its tallest local state at 1160 px; all other cards stay default. |
| `tr_trail_enable-on` | Only `tr_trail_enable` is on; dependent visibility is measured. |
| `tr_trail_style_combo-option-0` | Only `tr_trail_style_combo` selects `Light wall`; other selectors remain default. |
| `tr_trail_style_combo-option-1` | Only `tr_trail_style_combo` selects `Ground streak`; other selectors remain default. |
| `tr_trail_style_combo-option-2` | Only `tr_trail_style_combo` selects `Camera ribbon`; other selectors remain default. |
| `tr_trail_target_set_combo-option-0` | Only `tr_trail_target_set_combo` selects `Single Director subject`; other selectors remain default. |
| `tr_trail_target_set_combo-option-1` | Only `tr_trail_target_set_combo` selects `All Director cast`; other selectors remain default. |
| `tr_trail_target_set_combo-option-2` | Only `tr_trail_target_set_combo` selects `All avatars in view`; other selectors remain default. |
| `tr_trail_target_set_combo-option-3` | Only `tr_trail_target_set_combo` selects `Only my avatar`; other selectors remain default. |
| `tr_trail_target_set_combo-option-4` | Only `tr_trail_target_set_combo` selects `All avatars except me`; other selectors remain default. |
| `tr_trail_target_set_combo-option-5` | Only `tr_trail_target_set_combo` selects `Nearest N avatars`; other selectors remain default. |
| `tr_trail_target_set_combo-option-6` | Only `tr_trail_target_set_combo` selects `Selected objects`; other selectors remain default. |
| `tr_trail_target_set_combo-option-7` | Only `tr_trail_target_set_combo` selects `Animesh only`; other selectors remain default. |
| `tr_trail_color_mode_combo-option-0` | Only `tr_trail_color_mode_combo` selects `Primary`; other selectors remain default. |
| `tr_trail_color_mode_combo-option-1` | Only `tr_trail_color_mode_combo` selects `Per-actor palette`; other selectors remain default. |
| `tr_trail_color_mode_combo-option-2` | Only `tr_trail_color_mode_combo` selects `Secondary`; other selectors remain default. |
| `tr_trail_scale_check-on` | Only `tr_trail_scale_check` is on; dependent visibility is measured. |
| `tr_trail_fog_combo-option-0` | Only `tr_trail_fog_combo` selects `Off`; other selectors remain default. |
| `tr_trail_fog_combo-option-1` | Only `tr_trail_fog_combo` selects `Analytic`; other selectors remain default. |
| `tr_trail_clear_on_cut_check-on` | Only `tr_trail_clear_on_cut_check` is on; dependent visibility is measured. |
| `card-subject-tallest-850` | Only `subject` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-subject-tallest-1160` | Only `subject` is put in its tallest local state at 1160 px; all other cards stay default. |
| `tr_subject_target_set_combo-option-0` | Only `tr_subject_target_set_combo` selects `Single Director subject`; other selectors remain default. |
| `tr_subject_target_set_combo-option-1` | Only `tr_subject_target_set_combo` selects `All Director cast`; other selectors remain default. |
| `tr_subject_target_set_combo-option-2` | Only `tr_subject_target_set_combo` selects `All avatars in view`; other selectors remain default. |
| `tr_subject_target_set_combo-option-3` | Only `tr_subject_target_set_combo` selects `Only my avatar`; other selectors remain default. |
| `tr_subject_target_set_combo-option-4` | Only `tr_subject_target_set_combo` selects `All avatars except me`; other selectors remain default. |
| `tr_subject_target_set_combo-option-5` | Only `tr_subject_target_set_combo` selects `Nearest N avatars`; other selectors remain default. |
| `tr_subject_target_set_combo-option-6` | Only `tr_subject_target_set_combo` selects `Selected objects`; other selectors remain default. |
| `tr_subject_target_set_combo-option-7` | Only `tr_subject_target_set_combo` selects `Animesh only`; other selectors remain default. |
| `tr_subject_target_combo-option-0` | Only `tr_subject_target_combo` selects `Self`; other selectors remain default. |
| `tr_subject_target_combo-option-1` | Only `tr_subject_target_combo` selects `Subject A`; other selectors remain default. |
| `tr_subject_target_combo-option-2` | Only `tr_subject_target_combo` selects `Subject B`; other selectors remain default. |
| `tr_subject_target_combo-option-3` | Only `tr_subject_target_combo` selects `Subject C`; other selectors remain default. |
| `tr_subject_target_combo-option-4` | Only `tr_subject_target_combo` selects `Subject D`; other selectors remain default. |
| `tr_subject_shape_combo-option-0` | Only `tr_subject_shape_combo` selects `Ellipse`; other selectors remain default. |
| `tr_subject_shape_combo-option-1` | Only `tr_subject_shape_combo` selects `Box`; other selectors remain default. |
| `tr_subject_shape_combo-option-2` | Only `tr_subject_shape_combo` selects `Capsule`; other selectors remain default. |
| `tr_subject_source_grid_combo-option-0` | Only `tr_subject_source_grid_combo` selects `Boxes`; other selectors remain default. |
| `tr_subject_source_grid_combo-option-1` | Only `tr_subject_source_grid_combo` selects `Avatar tag`; other selectors remain default. |
| `tr_subject_source_grid_combo-option-2` | Only `tr_subject_source_grid_combo` selects `Tag AND boxes`; other selectors remain default. |
| `tr_subject_source_grid_combo-option-3` | Only `tr_subject_source_grid_combo` selects `Tag OR boxes`; other selectors remain default. |
| `tr_subject_source_rim_combo-option-0` | Only `tr_subject_source_rim_combo` selects `Boxes`; other selectors remain default. |
| `tr_subject_source_rim_combo-option-1` | Only `tr_subject_source_rim_combo` selects `Avatar tag`; other selectors remain default. |
| `tr_subject_source_rim_combo-option-2` | Only `tr_subject_source_rim_combo` selects `Tag AND boxes`; other selectors remain default. |
| `tr_subject_source_rim_combo-option-3` | Only `tr_subject_source_rim_combo` selects `Tag OR boxes`; other selectors remain default. |
| `tr_subject_invert_check-on` | Only `tr_subject_invert_check` is on; dependent visibility is measured. |
| `card-integration-tallest-850` | Only `integration` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-integration-tallest-1160` | Only `integration` is put in its tallest local state at 1160 px; all other cards stay default. |
| `tr_rigrim_tron_mode_combo-option-0` | Only `tr_rigrim_tron_mode_combo` selects `Off`; other selectors remain default. |
| `tr_rigrim_tron_mode_combo-option-1` | Only `tr_rigrim_tron_mode_combo` selects `Palette tint`; other selectors remain default. |
| `tr_rigrim_tron_mode_combo-option-2` | Only `tr_rigrim_tron_mode_combo` selects `Tint + pulse`; other selectors remain default. |
| `tr_rigrim_tron_color_combo-option-0` | Only `tr_rigrim_tron_color_combo` selects `Primary`; other selectors remain default. |
| `tr_rigrim_tron_color_combo-option-1` | Only `tr_rigrim_tron_color_combo` selects `Secondary`; other selectors remain default. |
| `tr_rigrim_tron_color_combo-option-2` | Only `tr_rigrim_tron_color_combo` selects `Accent`; other selectors remain default. |
| `card-adv_tron-tallest-850` | Only `adv_tron` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-adv_tron-tallest-1160` | Only `adv_tron` is put in its tallest local state at 1160 px; all other cards stay default. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## Ambiguous same-name candidates (never auto-bound)

None.

## Rejected out-of-scope same-name candidates

None.

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
