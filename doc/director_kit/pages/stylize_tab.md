# Roto Ink — Director Kit page contract

Page key: `stylize_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (78/81 setting-bound rows; 32 C++-referenced rows).

## Real owners

- `floater_lightbox_settings` / `floater` at `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:8` (79 matching rows).
- Referencing classes: `ALFloaterLightBox` (32).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALRotoKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `ink` / `ri_reset_all` (Reset all) | `button` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5460` | `ALRotoKitAdapter` command/model binding → wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onRotoInkPreset()` (`indra/newview/alfloaterlightbox.cpp:104`) | none |
| `ink` / `ri_enable` (Enable ink) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5386` | kit `toggle` ↔ `CineOutlineEnabled` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:16` `ri_enable` |
| `ink` / `ri_color_swatch` (Ink colour) | `color_swatch` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5610` | kit `color` ↔ `CineOutlineColor` `Color3`; `ALRotoKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateRotoInkAnimVisibility()` (`indra/newview/alfloaterlightbox.cpp:950`) | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:19` `ri_color_swatch` |
| `ink` / `ri_intensity_slider` (HDR intensity) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5616` | kit `slider` ↔ `CineOutlineIntensity` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:22` `ri_intensity_slider` |
| `ink` / `ri_opacity_slider` (Opacity) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5619` | kit `slider` ↔ `CineOutlineOpacity` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:27` `ri_opacity_slider` |
| `ink` / `ri_blend_mode_combo` (Blend) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5622` | kit `combo` ↔ `CineOutlineBlendMode` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:32` `ri_blend_mode_combo` |
| `ink` / `ri_ink_layer_combo` (Layer) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5463` | kit `combo` ↔ `CineOutlineLayer` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:41` `ri_ink_layer_combo` |
| `ink` / `ri_ink_mode_combo` (Colour from) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5597` | kit `combo` ↔ `CineOutlineInkColorMode` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:51` `ri_ink_mode_combo` |
| `ink` / `ri_glow_slider` (Bloom feed) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5628` | kit `slider` ↔ `CineOutlineGlow` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:59` `ri_glow_slider` |
| `ink` / `ri_match_reach_slider` (Match px) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5613` | kit `slider` ↔ `CineOutlineMatchReach` `F32`; `ALRotoKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateRotoInkAnimVisibility()` (`indra/newview/alfloaterlightbox.cpp:955`) | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:64` `ri_match_reach_slider` |
| `line` / `ri_sil_enable` (Silhouette) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5469` | kit `toggle` ↔ `CineOutlineSilhouetteEnabled` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:71` `ri_sil_enable` |
| `line` / `ri_sil_weight_slider` (Weight) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5485` | kit `slider` ↔ `CineOutlineSilhouetteWeight` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:74` `ri_sil_weight_slider` |
| `line` / `ri_width_slider` (Width px) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5500` | kit `slider` ↔ `CineOutlineWidth` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:79` `ri_width_slider` |
| `line` / `ri_softness_slider` (Softness) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5503` | kit `slider` ↔ `CineOutlineSoftness` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:84` `ri_softness_slider` |
| `line` / `ri_far_cutoff_slider` (Far cutoff m) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5506` | kit `slider` ↔ `CineOutlineFarCutoff` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:89` `ri_far_cutoff_slider` |
| `line` / `ri_sil_thr_slider` (Threshold) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5471` | kit `slider` ↔ `CineOutlineSilhouetteThreshold` `F32`; `ALRotoKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateRotoInkDepthMode()` (`indra/newview/alfloaterlightbox.cpp:838`) | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:94` `ri_sil_thr_slider` |
| `line` / `ri_metric_thr_slider` (Metric m) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5482` | kit `slider` ↔ `CineOutlineMetricThreshold` `F32`; `ALRotoKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateRotoInkDepthMode()` (`indra/newview/alfloaterlightbox.cpp:841`) | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:99` `ri_metric_thr_slider` |
| `line` / `ri_depth_mode_combo` (Depth mode) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5495` | kit `combo` ↔ `CineOutlineDepthMode` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:104` `ri_depth_mode_combo` |
| `creases` / `ri_crease_enable` (Creases) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5487` | kit `toggle` ↔ `CineOutlineCreaseEnabled` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:114` `ri_crease_enable` |
| `creases` / `ri_crease_weight_slider` (Crease amt) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5492` | kit `slider` ↔ `CineOutlineCreaseWeight` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:117` `ri_crease_weight_slider` |
| `creases` / `ri_crease_angle_slider` (Angle deg) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5489` | kit `slider` ↔ `CineOutlineCreaseAngle` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:122` `ri_crease_angle_slider` |
| `colour2` / `ri_color2_mode_combo` (Mode) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5632` | kit `combo` ↔ `CineOutlineColor2Mode` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:129` `ri_color2_mode_combo` |
| `colour2` / `ri_color2_swatch` (Second colour) | `color_swatch` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5647` | kit `color` ↔ `CineOutlineColor2` `Color3`; `ALRotoKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateRotoInkAnimVisibility()` (`indra/newview/alfloaterlightbox.cpp:964`) | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:140` `ri_color2_swatch` |
| `colour2` / `ri_color2_speed_slider` (Cycle speed) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5650` | kit `slider` ↔ `CineOutlineColor2Speed` `F32`; `ALRotoKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateRotoInkAnimVisibility()` (`indra/newview/alfloaterlightbox.cpp:967`) | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:143` `ri_color2_speed_slider` |
| `colour2` / `ri_color2_length_slider` (Cycle px) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5653` | kit `slider` ↔ `CineOutlineColor2Length` `F32`; `ALRotoKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateRotoInkAnimVisibility()` (`indra/newview/alfloaterlightbox.cpp:968`) | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:148` `ri_color2_length_slider` |
| `isolation` / `ri_subject_mode_combo` (Isolate) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5510` | kit `combo` ↔ `CineOutlineSubjectMode` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:155` `ri_subject_mode_combo` |
| `isolation` / `ri_subject_target_combo` (Target) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5552` | kit `combo` ↔ `CineOutlineSubjectTarget` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:171` `ri_subject_target_combo` |
| `isolation` / `ri_subject_invert_check` (Invert) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5550` | kit `toggle` ↔ `CineOutlineSubjectInvert` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:182` `ri_subject_invert_check` |
| `isolation` / `ri_subject_range_slider` (Depth range m) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5563` | kit `slider` ↔ `CineOutlineSubjectDepthRange` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:185` `ri_subject_range_slider` |
| `isolation` / `ri_subject_feather_slider` (Depth feather) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5566` | kit `slider` ↔ `CineOutlineSubjectDepthFeather` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:190` `ri_subject_feather_slider` |
| `isolation` / `ri_subject_manual_depth_slider` (Fallback m) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5560` | kit `slider` ↔ `CineOutlineSubjectManualDepth` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:195` `ri_subject_manual_depth_slider` |
| `isolation` / `ri_subject_source_combo` (Mask source) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5523` | kit `combo` ↔ `CineOutlineSubjectSource` `S32`; `ALRotoKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateRotoInkAnimVisibility()` (`indra/newview/alfloaterlightbox.cpp:976`) | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:200` `ri_subject_source_combo` |
| `isolation` / `ri_subject_targetset_combo` (Target set) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5530` | kit `combo` ↔ `CineOutlineSubjectTargetSet` `S32`; `ALRotoKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateRotoInkAnimVisibility()` (`indra/newview/alfloaterlightbox.cpp:977`) | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:210` `ri_subject_targetset_combo` |
| `isolation` / `ri_subject_max_spinner` (Max targets) | `spinner` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5541` | kit `number` ↔ `CineOutlineSubjectMaxTargets` `S32`; `ALRotoKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateRotoInkAnimVisibility()` (`indra/newview/alfloaterlightbox.cpp:978`) | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:224` `ri_subject_max_spinner` |
| `isolation` / `ri_subject_shape_combo` (Target shape) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5544` | kit `combo` ↔ `CineOutlineSubjectShape` `S32`; `ALRotoKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateRotoInkAnimVisibility()` (`indra/newview/alfloaterlightbox.cpp:979`) | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:227` `ri_subject_shape_combo` |
| `isolation` / `ri_subject_target_color_check` (Per target) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5549` | kit `toggle` ↔ `CineOutlineSubjectTargetColor` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:236` `ri_subject_target_color_check` |
| `band` / `ri_subject_near_slider` (Band near m) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5569` | kit `slider` ↔ `CineOutlineSubjectDepthNear` `F32`; `ALRotoKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateRotoInkAnimVisibility()` (`indra/newview/alfloaterlightbox.cpp:988`) | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:241` `ri_subject_near_slider` |
| `band` / `ri_subject_far_slider` (Band far m) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5572` | kit `slider` ↔ `CineOutlineSubjectDepthFar` `F32`; `ALRotoKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateRotoInkAnimVisibility()` (`indra/newview/alfloaterlightbox.cpp:989`) | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:246` `ri_subject_far_slider` |
| `ellipse` / `ri_subject_ellipse_slider` (Ellipse mix) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5575` | kit `slider` ↔ `CineOutlineSubjectEllipse` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:253` `ri_subject_ellipse_slider` |
| `ellipse` / `ri_subject_ellipse_scale_slider` (Ellipse size) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5578` | kit `slider` ↔ `CineOutlineSubjectEllipseScale` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:258` `ri_subject_ellipse_scale_slider` |
| `ellipse` / `ri_subject_ellipse_feather_slider` (Ellipse edge) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5581` | kit `slider` ↔ `CineOutlineSubjectEllipseFeather` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:263` `ri_subject_ellipse_feather_slider` |
| `ellipse` / `ri_subject_screen_cx_slider` (Shape X) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5584` | kit `slider` ↔ `CineOutlineSubjectScreenCX` `F32`; `ALRotoKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateRotoInkAnimVisibility()` (`indra/newview/alfloaterlightbox.cpp:992`) | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:268` `ri_subject_screen_cx_slider` |
| `ellipse` / `ri_subject_screen_cy_slider` (Shape Y) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5587` | kit `slider` ↔ `CineOutlineSubjectScreenCY` `F32`; `ALRotoKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateRotoInkAnimVisibility()` (`indra/newview/alfloaterlightbox.cpp:993`) | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:273` `ri_subject_screen_cy_slider` |
| `ellipse` / `ri_subject_screen_rx_slider` (Half width) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5590` | kit `slider` ↔ `CineOutlineSubjectScreenRX` `F32`; `ALRotoKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateRotoInkAnimVisibility()` (`indra/newview/alfloaterlightbox.cpp:994`) | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:278` `ri_subject_screen_rx_slider` |
| `ellipse` / `ri_subject_screen_ry_slider` (Half height) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5593` | kit `slider` ↔ `CineOutlineSubjectScreenRY` `F32`; `ALRotoKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateRotoInkAnimVisibility()` (`indra/newview/alfloaterlightbox.cpp:995`) | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:283` `ri_subject_screen_ry_slider` |
| `sketch` / `ri_sketch_enable` (Hand-drawn) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5656` | kit `toggle` ↔ `CineOutlineSketchEnabled` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:290` `ri_sketch_enable` |
| `sketch` / `ri_sketch_amount_slider` (Boil px) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5658` | kit `slider` ↔ `CineOutlineSketchAmount` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:293` `ri_sketch_amount_slider` |
| `sketch` / `ri_sketch_fps_slider` (Redraw fps) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5664` | kit `slider` ↔ `CineOutlineSketchFPS` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:298` `ri_sketch_fps_slider` |
| `sketch` / `ri_sketch_detail_slider` (Wobble size) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5661` | kit `slider` ↔ `CineOutlineSketchDetail` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:303` `ri_sketch_detail_slider` |
| `sketch` / `ri_sketch_strokes_spinner` (Strokes) | `spinner` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5667` | kit `number` ↔ `CineOutlineSketchStrokes` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:308` `ri_sketch_strokes_spinner` |
| `sketch` / `ri_sketch_roughness_slider` (Roughness) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5670` | kit `slider` ↔ `CineOutlineSketchRoughness` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:311` `ri_sketch_roughness_slider` |
| `sketch` / `ri_sketch_seed_slider` (Seed) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5673` | kit `slider` ↔ `CineOutlineSketchSeed` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:316` `ri_sketch_seed_slider` |
| `pattern` / `ri_pattern_type_combo` (Pattern) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5677` | kit `combo` ↔ `CineOutlinePatternType` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:323` `ri_pattern_type_combo` |
| `pattern` / `ri_pattern_size_slider` (Size px) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5686` | kit `slider` ↔ `CineOutlinePatternSize` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:335` `ri_pattern_size_slider` |
| `pattern` / `ri_pattern_ratio_slider` (Ratio) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5689` | kit `slider` ↔ `CineOutlinePatternRatio` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:340` `ri_pattern_ratio_slider` |
| `pattern` / `ri_pattern_drift_slider` (Drift px/s) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5692` | kit `slider` ↔ `CineOutlinePatternDrift` `F32`; `ALRotoKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateRotoInkAnimVisibility()` (`indra/newview/alfloaterlightbox.cpp:936`) | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:345` `ri_pattern_drift_slider` |
| `pattern` / `ri_hatch_angle_slider` (Hatch angle) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5695` | kit `slider` ↔ `CineOutlineHatchAngle` `F32`; `ALRotoKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateRotoInkAnimVisibility()` (`indra/newview/alfloaterlightbox.cpp:938`) | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:350` `ri_hatch_angle_slider` |
| `pattern` / `ri_hatch_reach_slider` (Shade px) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5698` | kit `slider` ↔ `CineOutlineHatchReach` `F32`; `ALRotoKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateRotoInkAnimVisibility()` (`indra/newview/alfloaterlightbox.cpp:939`) | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:355` `ri_hatch_reach_slider` |
| `pattern` / `ri_hatch_cross_check` (Cross-hatch) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5700` | kit `toggle` ↔ `CineOutlineHatchCross` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:360` `ri_hatch_cross_check` |
| `pattern` / `ri_pattern_seed_slider` (Seed) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5702` | kit `slider` ↔ `CineOutlinePatternSeed` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:363` `ri_pattern_seed_slider` |
| `motion1` / `ri_motion_style_combo` (Style) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5706` | kit `combo` ↔ `CineOutlineMotionStyle` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:370` `ri_motion_style_combo` |
| `motion1` / `ri_motion_speed_slider` (Speed) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5749` | kit `slider` ↔ `CineOutlineMotionSpeed` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:412` `ri_motion_speed_slider` |
| `motion1` / `ri_motion_amount_slider` (Amount) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5752` | kit `slider` ↔ `CineOutlineMotionAmount` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:417` `ri_motion_amount_slider` |
| `motion1` / `ri_motion_scale_slider` (Scale) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5755` | kit `slider` ↔ `CineOutlineMotionScale` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:422` `ri_motion_scale_slider` |
| `motion1` / `ri_motion_shape_slider` (Shape) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5745` | kit `slider` ↔ `CineOutlineMotionShape` `F32`; `ALRotoKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateRotoInkAnimVisibility()` (`indra/newview/alfloaterlightbox.cpp:904`) | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:427` `ri_motion_shape_slider` |
| `motion1` / `ri_motion_angle_slider` (Angle deg) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5758` | kit `dial` ↔ `CineOutlineMotionAngle` `F32`; `ALRotoKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateRotoInkAnimVisibility()` (`indra/newview/alfloaterlightbox.cpp:905`) | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:432` `ri_motion_angle_slider` |
| `motion1` / `ri_motion_seed_slider` (Seed) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5845` | kit `slider` ↔ `CineOutlineMotionSeed` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:435` `ri_motion_seed_slider` |
| `motion2` / `ri_motion2_style_combo` (Style) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5762` | kit `combo` ↔ `CineOutlineMotion2Style` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:442` `ri_motion2_style_combo` |
| `motion2` / `ri_motion2_speed_slider` (Speed) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5801` | kit `slider` ↔ `CineOutlineMotion2Speed` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:484` `ri_motion2_speed_slider` |
| `motion2` / `ri_motion2_amount_slider` (Amount) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5804` | kit `slider` ↔ `CineOutlineMotion2Amount` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:489` `ri_motion2_amount_slider` |
| `motion2` / `ri_motion2_scale_slider` (Scale) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5807` | kit `slider` ↔ `CineOutlineMotion2Scale` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:494` `ri_motion2_scale_slider` |
| `motion2` / `ri_motion2_shape_slider` (Shape) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5810` | kit `slider` ↔ `CineOutlineMotion2Shape` `F32`; `ALRotoKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateRotoInkAnimVisibility()` (`indra/newview/alfloaterlightbox.cpp:911`) | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:499` `ri_motion2_shape_slider` |
| `motion2` / `ri_motion2_angle_slider` (Angle deg) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5813` | kit `dial` ↔ `CineOutlineMotion2Angle` `F32`; `ALRotoKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateRotoInkAnimVisibility()` (`indra/newview/alfloaterlightbox.cpp:912`) | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:504` `ri_motion2_angle_slider` |
| `timing` / `ri_anim_speed_slider` (Anim speed) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5825` | kit `slider` ↔ `CineOutlineAnimSpeed` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:509` `ri_anim_speed_slider` |
| `timing` / `ri_anim_pause_check` (Pause) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5827` | kit `toggle` ↔ `CineOutlineAnimPause` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:514` `ri_anim_pause_check` |
| `timing` / `ri_anim_follow_director_check` (Director time) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5828` | kit `toggle` ↔ `CineOutlineAnimFollowDirector` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:517` `ri_anim_follow_director_check` |
| `timing` / `ri_motion_tempo_slider` (Tempo BPM) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5832` | kit `slider` ↔ `CineOutlineMotionTempo` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:520` `ri_motion_tempo_slider` |
| `timing` / `ri_motion_phase_slider` (Phase beats) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5835` | kit `slider` ↔ `CineOutlineMotionPhase` `F32`; `ALRotoKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateRotoInkAnimVisibility()` (`indra/newview/alfloaterlightbox.cpp:917`) | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:525` `ri_motion_phase_slider` |
| `timing` / `ri_motion_step_combo` (Hold frames) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5838` | kit `combo` ↔ `CineOutlineMotionStep` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_look_stylize.xml:530` `ri_motion_step_combo` |
| `timing` / `ri_sync_tron_link` (Tron clock) | none | kit-native action `link_to:look_group/tron_tab`; no duplicate widget is required | none |
| `adv_roto` / `open_lightbox_roto` (Lightbox ›) | none | kit-native action `floater:lightbox/rotoink_settings`; no duplicate widget is required | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALFloaterLightBox::onRotoInkPreset()` — `indra/newview/alfloaterlightbox.cpp:104` | Move model-facing work into `ALRotoKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterLightBox::updateRotoInkAnimVisibility()` — `indra/newview/alfloaterlightbox.cpp:904, 905, 911, 912, 917, 936, 938, 939, 950, 955, 964, 967, 968, 976, 977, 978, 979, 988, 989, 992, 993, 994, 995` | Move model-facing work into `ALRotoKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterLightBox::updateRotoInkDepthMode()` — `indra/newview/alfloaterlightbox.cpp:838, 841` | Move model-facing work into `ALRotoKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **166 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-ink-tallest-850` | Only `ink` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-ink-tallest-1160` | Only `ink` is put in its tallest local state at 1160 px; all other cards stay default. |
| `ri_enable-on` | Only `ri_enable` is on; dependent visibility is measured. |
| `ri_blend_mode_combo-option-0` | Only `ri_blend_mode_combo` selects `Normal (ink over)`; other selectors remain default. |
| `ri_blend_mode_combo-option-1` | Only `ri_blend_mode_combo` selects `Multiply (darken)`; other selectors remain default. |
| `ri_blend_mode_combo-option-2` | Only `ri_blend_mode_combo` selects `Add (glow)`; other selectors remain default. |
| `ri_ink_layer_combo-option-0` | Only `ri_ink_layer_combo` selects `Scene`; other selectors remain default. |
| `ri_ink_layer_combo-option-1` | Only `ri_ink_layer_combo` selects `Camera (default)`; other selectors remain default. |
| `ri_ink_layer_combo-option-2` | Only `ri_ink_layer_combo` selects `Over atmosphere`; other selectors remain default. |
| `ri_ink_layer_combo-option-3` | Only `ri_ink_layer_combo` selects `Overlay (flat)`; other selectors remain default. |
| `ri_ink_mode_combo-option-0` | Only `ri_ink_mode_combo` selects `Fixed colour`; other selectors remain default. |
| `ri_ink_mode_combo-option-1` | Only `ri_ink_mode_combo` selects `Match nearby light`; other selectors remain default. |
| `card-line-tallest-850` | Only `line` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-line-tallest-1160` | Only `line` is put in its tallest local state at 1160 px; all other cards stay default. |
| `ri_sil_enable-on` | Only `ri_sil_enable` is on; dependent visibility is measured. |
| `ri_depth_mode_combo-option-0` | Only `ri_depth_mode_combo` selects `Relative (scale-free)`; other selectors remain default. |
| `ri_depth_mode_combo-option-1` | Only `ri_depth_mode_combo` selects `Metric (metres)`; other selectors remain default. |
| `card-creases-tallest-850` | Only `creases` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-creases-tallest-1160` | Only `creases` is put in its tallest local state at 1160 px; all other cards stay default. |
| `ri_crease_enable-on` | Only `ri_crease_enable` is on; dependent visibility is measured. |
| `card-colour2-tallest-850` | Only `colour2` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-colour2-tallest-1160` | Only `colour2` is put in its tallest local state at 1160 px; all other cards stay default. |
| `ri_color2_mode_combo-option-0` | Only `ri_color2_mode_combo` selects `Off`; other selectors remain default. |
| `ri_color2_mode_combo-option-1` | Only `ri_color2_mode_combo` selects `Two-tone along edge`; other selectors remain default. |
| `ri_color2_mode_combo-option-2` | Only `ri_color2_mode_combo` selects `Rainbow cycle`; other selectors remain default. |
| `ri_color2_mode_combo-option-3` | Only `ri_color2_mode_combo` selects `Highlight (motion peaks)`; other selectors remain default. |
| `ri_color2_mode_combo-option-4` | Only `ri_color2_mode_combo` selects `Heat (motion field)`; other selectors remain default. |
| `card-isolation-tallest-850` | Only `isolation` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-isolation-tallest-1160` | Only `isolation` is put in its tallest local state at 1160 px; all other cards stay default. |
| `ri_subject_mode_combo-option-0` | Only `ri_subject_mode_combo` selects `Everything`; other selectors remain default. |
| `ri_subject_mode_combo-option-1` | Only `ri_subject_mode_combo` selects `Subject only`; other selectors remain default. |
| `ri_subject_mode_combo-option-2` | Only `ri_subject_mode_combo` selects `Everything except subject`; other selectors remain default. |
| `ri_subject_mode_combo-option-3` | Only `ri_subject_mode_combo` selects `Target set`; other selectors remain default. |
| `ri_subject_mode_combo-option-4` | Only `ri_subject_mode_combo` selects `Depth band`; other selectors remain default. |
| `ri_subject_mode_combo-option-5` | Only `ri_subject_mode_combo` selects `Foreground of subject`; other selectors remain default. |
| `ri_subject_mode_combo-option-6` | Only `ri_subject_mode_combo` selects `Background of subject`; other selectors remain default. |
| `ri_subject_mode_combo-option-7` | Only `ri_subject_mode_combo` selects `Screen rectangle`; other selectors remain default. |
| `ri_subject_mode_combo-option-8` | Only `ri_subject_mode_combo` selects `Screen ellipse`; other selectors remain default. |
| `ri_subject_mode_combo-option-9` | Only `ri_subject_mode_combo` selects `Focus point (DoF)`; other selectors remain default. |
| `ri_subject_target_combo-option-0` | Only `ri_subject_target_combo` selects `Self`; other selectors remain default. |
| `ri_subject_target_combo-option-1` | Only `ri_subject_target_combo` selects `Subject A`; other selectors remain default. |
| `ri_subject_target_combo-option-2` | Only `ri_subject_target_combo` selects `Subject B`; other selectors remain default. |
| `ri_subject_target_combo-option-3` | Only `ri_subject_target_combo` selects `Subject C`; other selectors remain default. |
| `ri_subject_target_combo-option-4` | Only `ri_subject_target_combo` selects `Subject D`; other selectors remain default. |
| `ri_subject_invert_check-on` | Only `ri_subject_invert_check` is on; dependent visibility is measured. |
| `ri_subject_source_combo-option-0` | Only `ri_subject_source_combo` selects `Boxes (depth slab / shapes)`; other selectors remain default. |
| `ri_subject_source_combo-option-1` | Only `ri_subject_source_combo` selects `Avatar tag (HDR only)`; other selectors remain default. |
| `ri_subject_source_combo-option-2` | Only `ri_subject_source_combo` selects `Tag AND boxes (HDR only)`; other selectors remain default. |
| `ri_subject_source_combo-option-3` | Only `ri_subject_source_combo` selects `Tag OR boxes (HDR only)`; other selectors remain default. |
| `ri_subject_targetset_combo-option-0` | Only `ri_subject_targetset_combo` selects `Director subject`; other selectors remain default. |
| `ri_subject_targetset_combo-option-1` | Only `ri_subject_targetset_combo` selects `All cast`; other selectors remain default. |
| `ri_subject_targetset_combo-option-2` | Only `ri_subject_targetset_combo` selects `All avatars in view`; other selectors remain default. |
| `ri_subject_targetset_combo-option-3` | Only `ri_subject_targetset_combo` selects `Only me`; other selectors remain default. |
| `ri_subject_targetset_combo-option-4` | Only `ri_subject_targetset_combo` selects `All except me`; other selectors remain default. |
| `ri_subject_targetset_combo-option-5` | Only `ri_subject_targetset_combo` selects `Nearest N avatars`; other selectors remain default. |
| `ri_subject_targetset_combo-option-6` | Only `ri_subject_targetset_combo` selects `Selected objects`; other selectors remain default. |
| `ri_subject_targetset_combo-option-7` | Only `ri_subject_targetset_combo` selects `Animesh only`; other selectors remain default. |
| `ri_subject_shape_combo-option-0` | Only `ri_subject_shape_combo` selects `Ellipse`; other selectors remain default. |
| `ri_subject_shape_combo-option-1` | Only `ri_subject_shape_combo` selects `Box`; other selectors remain default. |
| `ri_subject_shape_combo-option-2` | Only `ri_subject_shape_combo` selects `Capsule`; other selectors remain default. |
| `ri_subject_target_color_check-on` | Only `ri_subject_target_color_check` is on; dependent visibility is measured. |
| `card-band-tallest-850` | Only `band` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-band-tallest-1160` | Only `band` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-ellipse-tallest-850` | Only `ellipse` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-ellipse-tallest-1160` | Only `ellipse` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-sketch-tallest-850` | Only `sketch` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-sketch-tallest-1160` | Only `sketch` is put in its tallest local state at 1160 px; all other cards stay default. |
| `ri_sketch_enable-on` | Only `ri_sketch_enable` is on; dependent visibility is measured. |
| `card-pattern-tallest-850` | Only `pattern` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-pattern-tallest-1160` | Only `pattern` is put in its tallest local state at 1160 px; all other cards stay default. |
| `ri_pattern_type_combo-option-0` | Only `ri_pattern_type_combo` selects `None`; other selectors remain default. |
| `ri_pattern_type_combo-option-1` | Only `ri_pattern_type_combo` selects `Dashed`; other selectors remain default. |
| `ri_pattern_type_combo-option-2` | Only `ri_pattern_type_combo` selects `Dotted`; other selectors remain default. |
| `ri_pattern_type_combo-option-3` | Only `ri_pattern_type_combo` selects `Double line`; other selectors remain default. |
| `ri_pattern_type_combo-option-4` | Only `ri_pattern_type_combo` selects `Hatch`; other selectors remain default. |
| `ri_pattern_type_combo-option-5` | Only `ri_pattern_type_combo` selects `Taper`; other selectors remain default. |
| `ri_hatch_cross_check-on` | Only `ri_hatch_cross_check` is on; dependent visibility is measured. |
| `card-motion1-tallest-850` | Only `motion1` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-motion1-tallest-1160` | Only `motion1` is put in its tallest local state at 1160 px; all other cards stay default. |
| `ri_motion_style_combo-option-0` | Only `ri_motion_style_combo` selects `None`; other selectors remain default. |
| `ri_motion_style_combo-option-1` | Only `ri_motion_style_combo` selects `Pulse`; other selectors remain default. |
| `ri_motion_style_combo-option-2` | Only `ri_motion_style_combo` selects `Chase (draw-around)`; other selectors remain default. |
| `ri_motion_style_combo-option-3` | Only `ri_motion_style_combo` selects `Scan (sweep bar)`; other selectors remain default. |
| `ri_motion_style_combo-option-4` | Only `ri_motion_style_combo` selects `Wave (segments)`; other selectors remain default. |
| `ri_motion_style_combo-option-5` | Only `ri_motion_style_combo` selects `Shimmer (twinkle)`; other selectors remain default. |
| `ri_motion_style_combo-option-6` | Only `ri_motion_style_combo` selects `Marching Ants`; other selectors remain default. |
| `ri_motion_style_combo-option-7` | Only `ri_motion_style_combo` selects `Heartbeat`; other selectors remain default. |
| `ri_motion_style_combo-option-8` | Only `ri_motion_style_combo` selects `Strobe`; other selectors remain default. |
| `ri_motion_style_combo-option-9` | Only `ri_motion_style_combo` selects `Ripple`; other selectors remain default. |
| `ri_motion_style_combo-option-10` | Only `ri_motion_style_combo` selects `Write-On (angular)`; other selectors remain default. |
| `ri_motion_style_combo-option-11` | Only `ri_motion_style_combo` selects `Dissolve`; other selectors remain default. |
| `ri_motion_style_combo-option-12` | Only `ri_motion_style_combo` selects `Electric`; other selectors remain default. |
| `ri_motion_style_combo-option-13` | Only `ri_motion_style_combo` selects `Glitch`; other selectors remain default. |
| `ri_motion_style_combo-option-14` | Only `ri_motion_style_combo` selects `Breathe`; other selectors remain default. |
| `ri_motion_style_combo-option-15` | Only `ri_motion_style_combo` selects `Sparkle Comets`; other selectors remain default. |
| `ri_motion_style_combo-option-16` | Only `ri_motion_style_combo` selects `Travelling Wave`; other selectors remain default. |
| `ri_motion_style_combo-option-17` | Only `ri_motion_style_combo` selects `Barber Pole`; other selectors remain default. |
| `ri_motion_style_combo-option-18` | Only `ri_motion_style_combo` selects `Neon Buzz`; other selectors remain default. |
| `ri_motion_style_combo-option-19` | Only `ri_motion_style_combo` selects `Morse Blink`; other selectors remain default. |
| `ri_motion_style_combo-option-20` | Only `ri_motion_style_combo` selects `Film Jitter`; other selectors remain default. |
| `ri_motion_style_combo-option-21` | Only `ri_motion_style_combo` selects `8-bit Stepped`; other selectors remain default. |
| `ri_motion_style_combo-option-22` | Only `ri_motion_style_combo` selects `Echo Aura`; other selectors remain default. |
| `ri_motion_style_combo-option-23` | Only `ri_motion_style_combo` selects `Shockwave`; other selectors remain default. |
| `ri_motion_style_combo-option-24` | Only `ri_motion_style_combo` selects `Orbit Dots`; other selectors remain default. |
| `ri_motion_style_combo-option-25` | Only `ri_motion_style_combo` selects `Constellation`; other selectors remain default. |
| `ri_motion_style_combo-option-26` | Only `ri_motion_style_combo` selects `Fire Lick`; other selectors remain default. |
| `ri_motion_style_combo-option-27` | Only `ri_motion_style_combo` selects `Smoke Wisps`; other selectors remain default. |
| `ri_motion_style_combo-option-28` | Only `ri_motion_style_combo` selects `Ink Drip`; other selectors remain default. |
| `ri_motion_style_combo-option-29` | Only `ri_motion_style_combo` selects `Splatter`; other selectors remain default. |
| `ri_motion_style_combo-option-30` | Only `ri_motion_style_combo` selects `Lightning Arcs`; other selectors remain default. |
| `ri_motion_style_combo-option-31` | Only `ri_motion_style_combo` selects `Laser Scan`; other selectors remain default. |
| `ri_motion_style_combo-option-32` | Only `ri_motion_style_combo` selects `Heat Shimmer`; other selectors remain default. |
| `ri_motion_style_combo-option-33` | Only `ri_motion_style_combo` selects `Jelly Wobble`; other selectors remain default. |
| `ri_motion_style_combo-option-34` | Only `ri_motion_style_combo` selects `Paint-On (directional)`; other selectors remain default. |
| `ri_motion_style_combo-option-35` | Only `ri_motion_style_combo` selects `Radial Reveal`; other selectors remain default. |
| `card-motion2-tallest-850` | Only `motion2` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-motion2-tallest-1160` | Only `motion2` is put in its tallest local state at 1160 px; all other cards stay default. |
| `ri_motion2_style_combo-option-0` | Only `ri_motion2_style_combo` selects `None`; other selectors remain default. |
| `ri_motion2_style_combo-option-1` | Only `ri_motion2_style_combo` selects `Pulse`; other selectors remain default. |
| `ri_motion2_style_combo-option-2` | Only `ri_motion2_style_combo` selects `Chase (draw-around)`; other selectors remain default. |
| `ri_motion2_style_combo-option-3` | Only `ri_motion2_style_combo` selects `Scan (sweep bar)`; other selectors remain default. |
| `ri_motion2_style_combo-option-4` | Only `ri_motion2_style_combo` selects `Wave (segments)`; other selectors remain default. |
| `ri_motion2_style_combo-option-5` | Only `ri_motion2_style_combo` selects `Shimmer (twinkle)`; other selectors remain default. |
| `ri_motion2_style_combo-option-6` | Only `ri_motion2_style_combo` selects `Marching Ants`; other selectors remain default. |
| `ri_motion2_style_combo-option-7` | Only `ri_motion2_style_combo` selects `Heartbeat`; other selectors remain default. |
| `ri_motion2_style_combo-option-8` | Only `ri_motion2_style_combo` selects `Strobe`; other selectors remain default. |
| `ri_motion2_style_combo-option-9` | Only `ri_motion2_style_combo` selects `Ripple`; other selectors remain default. |
| `ri_motion2_style_combo-option-10` | Only `ri_motion2_style_combo` selects `Write-On (angular)`; other selectors remain default. |
| `ri_motion2_style_combo-option-11` | Only `ri_motion2_style_combo` selects `Dissolve`; other selectors remain default. |
| `ri_motion2_style_combo-option-12` | Only `ri_motion2_style_combo` selects `Electric`; other selectors remain default. |
| `ri_motion2_style_combo-option-13` | Only `ri_motion2_style_combo` selects `Glitch`; other selectors remain default. |
| `ri_motion2_style_combo-option-14` | Only `ri_motion2_style_combo` selects `Breathe`; other selectors remain default. |
| `ri_motion2_style_combo-option-15` | Only `ri_motion2_style_combo` selects `Sparkle Comets`; other selectors remain default. |
| `ri_motion2_style_combo-option-16` | Only `ri_motion2_style_combo` selects `Travelling Wave`; other selectors remain default. |
| `ri_motion2_style_combo-option-17` | Only `ri_motion2_style_combo` selects `Barber Pole`; other selectors remain default. |
| `ri_motion2_style_combo-option-18` | Only `ri_motion2_style_combo` selects `Neon Buzz`; other selectors remain default. |
| `ri_motion2_style_combo-option-19` | Only `ri_motion2_style_combo` selects `Morse Blink`; other selectors remain default. |
| `ri_motion2_style_combo-option-20` | Only `ri_motion2_style_combo` selects `Film Jitter`; other selectors remain default. |
| `ri_motion2_style_combo-option-21` | Only `ri_motion2_style_combo` selects `8-bit Stepped`; other selectors remain default. |
| `ri_motion2_style_combo-option-22` | Only `ri_motion2_style_combo` selects `Echo Aura`; other selectors remain default. |
| `ri_motion2_style_combo-option-23` | Only `ri_motion2_style_combo` selects `Shockwave`; other selectors remain default. |
| `ri_motion2_style_combo-option-24` | Only `ri_motion2_style_combo` selects `Orbit Dots`; other selectors remain default. |
| `ri_motion2_style_combo-option-25` | Only `ri_motion2_style_combo` selects `Constellation`; other selectors remain default. |
| `ri_motion2_style_combo-option-26` | Only `ri_motion2_style_combo` selects `Fire Lick`; other selectors remain default. |
| `ri_motion2_style_combo-option-27` | Only `ri_motion2_style_combo` selects `Smoke Wisps`; other selectors remain default. |
| `ri_motion2_style_combo-option-28` | Only `ri_motion2_style_combo` selects `Ink Drip`; other selectors remain default. |
| `ri_motion2_style_combo-option-29` | Only `ri_motion2_style_combo` selects `Splatter`; other selectors remain default. |
| `ri_motion2_style_combo-option-30` | Only `ri_motion2_style_combo` selects `Lightning Arcs`; other selectors remain default. |
| `ri_motion2_style_combo-option-31` | Only `ri_motion2_style_combo` selects `Laser Scan`; other selectors remain default. |
| `ri_motion2_style_combo-option-32` | Only `ri_motion2_style_combo` selects `Heat Shimmer`; other selectors remain default. |
| `ri_motion2_style_combo-option-33` | Only `ri_motion2_style_combo` selects `Jelly Wobble`; other selectors remain default. |
| `ri_motion2_style_combo-option-34` | Only `ri_motion2_style_combo` selects `Paint-On (directional)`; other selectors remain default. |
| `ri_motion2_style_combo-option-35` | Only `ri_motion2_style_combo` selects `Radial Reveal`; other selectors remain default. |
| `card-timing-tallest-850` | Only `timing` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-timing-tallest-1160` | Only `timing` is put in its tallest local state at 1160 px; all other cards stay default. |
| `ri_anim_pause_check-on` | Only `ri_anim_pause_check` is on; dependent visibility is measured. |
| `ri_anim_follow_director_check-on` | Only `ri_anim_follow_director_check` is on; dependent visibility is measured. |
| `ri_motion_step_combo-option-0` | Only `ri_motion_step_combo` selects `Smooth`; other selectors remain default. |
| `ri_motion_step_combo-option-1` | Only `ri_motion_step_combo` selects `On twos (12 fps)`; other selectors remain default. |
| `ri_motion_step_combo-option-2` | Only `ri_motion_step_combo` selects `On threes (8 fps)`; other selectors remain default. |
| `ri_motion_step_combo-option-3` | Only `ri_motion_step_combo` selects `On fours (6 fps)`; other selectors remain default. |
| `card-adv_roto-tallest-850` | Only `adv_roto` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-adv_roto-tallest-1160` | Only `adv_roto` is put in its tallest local state at 1160 px; all other cards stay default. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## Ambiguous same-name candidates (never auto-bound)

None.

## Rejected out-of-scope same-name candidates

82 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

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
