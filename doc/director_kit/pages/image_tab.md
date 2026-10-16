# Image — Director Kit page contract

Page key: `image_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (68/74 setting-bound rows; 17 C++-referenced rows).

## Real owners

- `floater_lightbox_settings` / `floater` at `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:8` (21 matching rows).
- Referencing classes: `ALFloaterLightBox` (17), `LLButton` (1).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALImageLookKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `image_easy` / `tonemap_type_combo` (Tonemapper) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:314` | kit `combo` ↔ `AlchemyRenderTonemapType` `S32`; declared SHORTCUT of `exposure_tab/exp_tonemap_type` (home remains canonical for search/undo) | page `exposure_tab`, `indra/newview/skins/default/xui/en/floater_preferences_graphics_advanced.xml:1186` `TonemapType`, `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:79` `exp_tonemap_type`, `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:14` `tonemap_type_combo`, `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:13` `exp_tonemap_type`, `indra/newview/skins/default/xui/en/panel_exposure_src.xml:13` `exp_tonemap_type` |
| `image_easy` / `exposure_slider` (Exposure) | `slider_bar` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:418` | kit `slider` ↔ `RenderExposure` `F32`; declared SHORTCUT of `exposure_tab/exp_exposure` (home remains canonical for search/undo) | page `exposure_tab`, `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:433` `exposure_spinner`, `indra/newview/skins/default/xui/en/floater_preferences_graphics_advanced.xml:1045` `RenderExposure`, `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:74` `exp_exposure`, `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:29` `exposure_slider`, `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:9` `exp_exposure`, `indra/newview/skins/default/xui/en/panel_exposure_src.xml:12` `exp_exposure`, `indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml:378` `RenderExposure` |
| `image_easy` / `tonemap_legacy_gamma_skies` (Legacy skies) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:379` | kit `toggle` ↔ `AlchemyTonemapLegacyGammaSkies` `Boolean`; declared SHORTCUT of `exposure_tab/exp_legacy_gamma_skies` (home remains canonical for search/undo) | page `exposure_tab`, `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:109` `exp_legacy_gamma_skies`, `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:34` `tonemap_legacy_gamma_skies`, `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:30` `exp_legacy_gamma_skies`, `indra/newview/skins/default/xui/en/panel_exposure_src.xml:27` `exp_legacy_gamma_skies` |
| `image_easy` / `lpm_toneexp_slider` (Tone exposure) | `slider_bar` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:635` | kit `slider` ↔ `AlchemyToneMapAMDExposure` `F32`; `ALImageLookKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTonemapper()` (`indra/newview/alfloaterlightbox.cpp:1565`); declared SHORTCUT of `exposure_tab/exp_amd_exposure` (home remains canonical for search/undo) | page `exposure_tab`, `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:649` `lpm_toneexp_spinner`, `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:99` `exp_amd_exposure`, `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:37` `lpm_toneexp_slider`, `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:26` `exp_amd_exposure`, `indra/newview/skins/default/xui/en/panel_exposure_src.xml:25` `exp_amd_exposure` |
| `image_easy` / `slog_ev_slider` (EV offset) | `slider_bar` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:897` | kit `slider` ↔ `AlchemyToneMapSLogEV` `F32`; `ALImageLookKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTonemapper()` (`indra/newview/alfloaterlightbox.cpp:1605`); declared SHORTCUT of `exposure_tab/exp_slog_ev` (home remains canonical for search/undo) | page `exposure_tab`, `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:911` `slog_ev_spinner`, `indra/newview/skins/default/xui/en/panel_dpage_light_exposure.xml:104` `exp_slog_ev`, `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:42` `slog_ev_slider`, `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:28` `exp_slog_ev`, `indra/newview/skins/default/xui/en/panel_exposure_src.xml:26` `exp_slog_ev` |
| `tonemap` / `tm_saturation_slider` (Saturation) | `slider_bar` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:459` | kit `slider` ↔ `AlchemyToneMapSaturation` `F32`; `ALImageLookKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTonemapper()` (`indra/newview/alfloaterlightbox.cpp:1584`) | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:473` `tm_saturation_spinner`, `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:49` `tm_saturation_slider` |
| `tonemap` / `tm_contrast_slider` (Contrast) | `slider_bar` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:495` | kit `slider` ↔ `AlchemyToneMapContrast` `F32`; `ALImageLookKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTonemapper()` (`indra/newview/alfloaterlightbox.cpp:1585`) | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:509` `tm_contrast_spinner`, `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:54` `tm_contrast_slider` |
| `tonemap` / `exposure_link` (Exposure) | none | kit-native action `link_to:light_group/exposure_tab`; no duplicate widget is required | none |
| `tonemap` / `exposure_default_btn` (Reset tone) | `button` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:355` | `ALImageLookKitAdapter` command/model binding → wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onClickResetGroupDefault()` (`indra/newview/alfloaterlightbox.cpp:80`) | none |
| `tonemap` / `exposure_hub_btn` (Exposure hub) | `button` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:391` | kit-native action `floater:exposure_hub`; no duplicate widget is required | none |
| `sharpen` / `cas_sharpness_slider` (Sharpen (CAS)) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:167` | kit `slider` ↔ `RenderCASSharpness` `F32` | `indra/newview/skins/default/xui/en/floater_preferences_graphics_advanced.xml:1160` `RenderSharpness`, `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:68` `cas_sharpness_slider` |
| `grade` / `set_RenderColorGrade` (Colour grade) | none | kit `toggle` ↔ `RenderColorGrade` `Boolean`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:75` `set_RenderColorGrade` |
| `grade` / `colorlut_combo` (LUT) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:51` | kit `combo` ↔ `RenderColorGradeLUT` `String` | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:78` `colorlut_combo` |
| `grade` / `set_RenderColorGradeLUTStrength` (LUT strength) | none | kit `slider` ↔ `RenderColorGradeLUTStrength` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:110` `set_RenderColorGradeLUTStrength` |
| `grade` / `set_RenderColorGradeWhiteBalanceCCT` (Temperature) | none | kit `slider` ↔ `RenderColorGradeWhiteBalanceCCT` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:115` `set_RenderColorGradeWhiteBalanceCCT` |
| `grade` / `set_RenderColorGradeWhiteBalanceDuv` (Tint) | none | kit `slider` ↔ `RenderColorGradeWhiteBalanceDuv` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:120` `set_RenderColorGradeWhiteBalanceDuv` |
| `grade` / `set_RenderColorGradeLift` (Lift) | none | kit `vec3` ↔ `RenderColorGradeLift` `Vector3`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | none |
| `grade` / `set_RenderColorGradeGamma` (Gamma) | none | kit `vec3` ↔ `RenderColorGradeGamma` `Vector3`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | none |
| `grade` / `set_grade_lightbox_link` (Lightbox ›) | none | kit-native action `floater:lightbox/colortone_settings`; no duplicate widget is required | none |
| `grade_levels` / `set_RenderColorGradeGain` (Gain) | none | kit `vec3` ↔ `RenderColorGradeGain` `Vector3`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | none |
| `grade_levels` / `set_RenderColorGradeBrightness` (Grade bright) | none | kit `slider` ↔ `RenderColorGradeBrightness` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:133` `set_RenderColorGradeBrightness` |
| `grade_levels` / `set_RenderColorGradeContrast` (Grade contrast) | none | kit `slider` ↔ `RenderColorGradeContrast` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:138` `set_RenderColorGradeContrast` |
| `grade_levels` / `set_RenderColorGradeSaturation` (Grade sat) | none | kit `slider` ↔ `RenderColorGradeSaturation` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:143` `set_RenderColorGradeSaturation` |
| `grade_levels` / `set_RenderColorGradeVibrance` (Vibrance) | none | kit `slider` ↔ `RenderColorGradeVibrance` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:148` `set_RenderColorGradeVibrance` |
| `grade_levels` / `set_RenderColorGradeHighlights` (Highlights) | none | kit `slider` ↔ `RenderColorGradeHighlights` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:153` `set_RenderColorGradeHighlights` |
| `grade_levels` / `set_RenderColorGradeShadows` (Shadows) | none | kit `slider` ↔ `RenderColorGradeShadows` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:158` `set_RenderColorGradeShadows` |
| `grade_levels` / `set_RenderColorGradeBlackPoint` (Black point) | none | kit `slider` ↔ `RenderColorGradeBlackPoint` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:163` `set_RenderColorGradeBlackPoint` |
| `grade_levels` / `set_RenderColorGradeWhitePoint` (White point) | none | kit `slider` ↔ `RenderColorGradeWhitePoint` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:168` `set_RenderColorGradeWhitePoint` |
| `grade_levels` / `set_RenderColorGradeHueShift` (Hue shift) | none | kit `slider` ↔ `RenderColorGradeHueShift` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:173` `set_RenderColorGradeHueShift` |
| `grade_levels` / `set_RenderColorGradeCurveToe` (Curve toe) | none | kit `vec3` ↔ `RenderColorGradeCurveToe` `Color3`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | none |
| `grade_levels` / `set_RenderColorGradeCurveShoulder` (Curve shoulder) | none | kit `vec3` ↔ `RenderColorGradeCurveShoulder` `Color3`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | none |
| `grade_levels` / `set_RenderColorGradeCurveStrength` (Curve amount) | none | kit `vec3` ↔ `RenderColorGradeCurveStrength` `Color3`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | none |
| `split` / `set_RenderSplitToneAmount` (Split tone) | none | kit `slider` ↔ `RenderSplitToneAmount` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:183` `set_RenderSplitToneAmount` |
| `split` / `set_RenderSplitToneBalance` (Balance) | none | kit `slider` ↔ `RenderSplitToneBalance` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:188` `set_RenderSplitToneBalance` |
| `split` / `set_RenderSplitToneShadowTint` (Shadow tint) | none | kit `color` ↔ `RenderSplitToneShadowTint` `Color3`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:193` `set_RenderSplitToneShadowTint` |
| `split` / `set_RenderSplitToneHighlightTint` (Highlight tint) | none | kit `color` ↔ `RenderSplitToneHighlightTint` `Color3`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:196` `set_RenderSplitToneHighlightTint` |
| `split` / `set_RenderSplitToneMidtoneAmount` (Midtone amt) | none | kit `slider` ↔ `RenderSplitToneMidtoneAmount` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:199` `set_RenderSplitToneMidtoneAmount` |
| `split` / `set_RenderSplitToneMidtoneTint` (Midtone tint) | none | kit `color` ↔ `RenderSplitToneMidtoneTint` `Color3`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:204` `set_RenderSplitToneMidtoneTint` |
| `vignette` / `set_RenderVignetteAmount` (Amount) | none | kit `slider` ↔ `RenderVignetteAmount` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:209` `set_RenderVignetteAmount` |
| `vignette` / `set_RenderVignetteRadius` (Radius) | none | kit `slider` ↔ `RenderVignetteRadius` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:214` `set_RenderVignetteRadius` |
| `vignette` / `set_RenderVignetteFeather` (Feather) | none | kit `slider` ↔ `RenderVignetteFeather` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:219` `set_RenderVignetteFeather` |
| `vignette` / `set_RenderVignetteSoft` (Edge soft) | none | kit `slider` ↔ `RenderVignetteSoft` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:224` `set_RenderVignetteSoft` |
| `vignette` / `set_RenderVignetteColor` (Edge colour) | none | kit `color` ↔ `RenderVignetteColor` `Color3`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:229` `set_RenderVignetteColor` |
| `vignette` / `set_RenderVignetteMidPoint` (Mid point) | none | kit `slider` ↔ `RenderVignetteMidPoint` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:232` `set_RenderVignetteMidPoint` |
| `vignette` / `set_RenderVignetteMidColor` (Mid colour) | none | kit `color` ↔ `RenderVignetteMidColor` `Color3`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:237` `set_RenderVignetteMidColor` |
| `vignette` / `set_RenderVignetteShape` (Shape) | none | kit `slider` ↔ `RenderVignetteShape` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:240` `set_RenderVignetteShape` |
| `vignette` / `set_RenderVignetteCenter` (Centre) | none | kit `vec2` ↔ `RenderVignetteCenter` `Vector3`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | none |
| `vignette` / `set_RenderVignetteCorrectAspect` (Fix aspect) | none | kit `toggle` ↔ `RenderVignetteCorrectAspect` `Boolean`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:246` `set_RenderVignetteCorrectAspect` |
| `grain` / `set_RenderFilmGrainAmount` (Amount) | none | kit `slider` ↔ `RenderFilmGrainAmount` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:251` `set_RenderFilmGrainAmount` |
| `grain` / `set_RenderFilmGrainSize` (Size) | none | kit `slider` ↔ `RenderFilmGrainSize` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:256` `set_RenderFilmGrainSize` |
| `grain` / `set_RenderFilmGrainRange` (Range) | none | kit `slider` ↔ `RenderFilmGrainRange` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:261` `set_RenderFilmGrainRange` |
| `grain` / `set_RenderFilmGrainStyle` (Style) | none | kit `combo` ↔ `RenderFilmGrainStyle` `S32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:266` `set_RenderFilmGrainStyle` |
| `grain` / `set_RenderFilmGrainAnimated` (Animated) | none | kit `toggle` ↔ `RenderFilmGrainAnimated` `Boolean`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:276` `set_RenderFilmGrainAnimated` |
| `grain` / `set_RenderFilmGrainTint` (Tint) | none | kit `color` ↔ `RenderFilmGrainTint` `Color3`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:279` `set_RenderFilmGrainTint` |
| `ca` / `set_RenderChromaticAberrationStrength` (Strength) | none | kit `slider` ↔ `RenderChromaticAberrationStrength` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:284` `set_RenderChromaticAberrationStrength` |
| `ca` / `set_RenderChromaticAberrationFalloff` (Falloff) | none | kit `slider` ↔ `RenderChromaticAberrationFalloff` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:289` `set_RenderChromaticAberrationFalloff` |
| `ca` / `set_RenderChromaticAberrationAngle` (Angle deg) | none | kit `dial` ↔ `RenderChromaticAberrationAngle` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:294` `set_RenderChromaticAberrationAngle` |
| `ca` / `set_RenderChromaticAberrationAnisotropy` (Stretch) | none | kit `slider` ↔ `RenderChromaticAberrationAnisotropy` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:297` `set_RenderChromaticAberrationAnisotropy` |
| `ca` / `set_RenderChromaticAberrationOffsetRX` (Red X) | none | kit `slider` ↔ `RenderChromaticAberrationOffsetRX` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:302` `set_RenderChromaticAberrationOffsetRX` |
| `ca` / `set_RenderChromaticAberrationOffsetRY` (Red Y) | none | kit `slider` ↔ `RenderChromaticAberrationOffsetRY` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:307` `set_RenderChromaticAberrationOffsetRY` |
| `ca` / `set_RenderChromaticAberrationOffsetBX` (Blue X) | none | kit `slider` ↔ `RenderChromaticAberrationOffsetBX` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:312` `set_RenderChromaticAberrationOffsetBX` |
| `ca` / `set_RenderChromaticAberrationOffsetBY` (Blue Y) | none | kit `slider` ↔ `RenderChromaticAberrationOffsetBY` `F32`. Contract note: kit-native setting row; synthetic data name does not require v1 XUI | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:317` `set_RenderChromaticAberrationOffsetBY` |
| `khronos` / `tm_khronos_desat_slider` (Desaturate) | `slider_bar` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:531` | kit `slider` ↔ `AlchemyToneMapKhronosDesaturation` `F32`; `ALImageLookKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTonemapper()` (`indra/newview/alfloaterlightbox.cpp:1586`) | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:545` `tm_khronos_desat_spinner`, `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:324` `tm_khronos_desat_slider` |
| `khronos` / `tm_khronos_knee_slider` (Knee) | `slider_bar` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:567` | kit `slider` ↔ `AlchemyToneMapKhronosStartCompression` `F32`; `ALImageLookKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTonemapper()` (`indra/newview/alfloaterlightbox.cpp:1587`) | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:581` `tm_khronos_knee_spinner`, `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:329` `tm_khronos_knee_slider` |
| `lpm` / `lpm_hdrmax_slider` (HDR max) | `slider_bar` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:601` | kit `slider` ↔ `AlchemyToneMapAMDHDRMax` `F32`; `ALImageLookKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTonemapper()` (`indra/newview/alfloaterlightbox.cpp:1564`) | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:615` `lpm_hdrmax_spinner`, `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:336` `lpm_hdrmax_slider` |
| `lpm` / `lpm_contrast_slider` (LPM contrast) | `slider_bar` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:669` | kit `slider` ↔ `AlchemyToneMapAMDContrast` `F32`; `ALImageLookKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTonemapper()` (`indra/newview/alfloaterlightbox.cpp:1566`) | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:683` `lpm_contrast_spinner`, `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:341` `lpm_contrast_slider` |
| `lpm` / `lpm_satr_slider` (Red sat) | `slider_bar` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:703` | kit `slider` ↔ `AlchemyToneMapAMDSaturationR` `F32`; `ALImageLookKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTonemapper()` (`indra/newview/alfloaterlightbox.cpp:1567`) | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:717` `lpm_satr_spinner`, `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:346` `lpm_satr_slider` |
| `lpm` / `lpm_satg_slider` (Green sat) | `slider_bar` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:737` | kit `slider` ↔ `AlchemyToneMapAMDSaturationG` `F32`; `ALImageLookKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTonemapper()` (`indra/newview/alfloaterlightbox.cpp:1568`) | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:751` `lpm_satg_spinner`, `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:351` `lpm_satg_slider` |
| `lpm` / `lpm_satb_slider` (Blue sat) | `slider_bar` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:771` | kit `slider` ↔ `AlchemyToneMapAMDSaturationB` `F32`; `ALImageLookKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTonemapper()` (`indra/newview/alfloaterlightbox.cpp:1569`) | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:785` `lpm_satb_spinner`, `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:356` `lpm_satb_slider` |
| `slog` / `slog_curve_combo` (Log curve) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:809` | kit `combo` ↔ `AlchemyToneMapSLogCurve` `S32`; `ALImageLookKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTonemapper()` (`indra/newview/alfloaterlightbox.cpp:1602`) | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:363` `slog_curve_combo` |
| `slog` / `slog_range_combo` (Range) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:836` | kit `combo` ↔ `AlchemyToneMapSLogRange` `S32`; `ALImageLookKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTonemapper()` (`indra/newview/alfloaterlightbox.cpp:1603`) | `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:373` `slog_range_combo` |
| `slog` / `slog_toe_slider` (Shadow toe) | `slider_bar` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:861` | kit `slider` ↔ `AlchemyToneMapSLogToe` `F32`; `ALImageLookKitAdapter` preserves/calls behavior `ALFloaterLightBox::updateTonemapper()` (`indra/newview/alfloaterlightbox.cpp:1604`) | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:875` `slog_toe_spinner`, `indra/newview/skins/default/xui/en/panel_dpage_look_image.xml:381` `slog_toe_slider` |
| `adv_image` / `set_lens_filters_link` (Lens filters) | none | kit-native action `link_to:lens_group/filters_tab`; no duplicate widget is required | none |
| `adv_image` / `open_lightbox_image` (Lightbox ›) | none | kit-native action `floater:lightbox/colortone_settings`; no duplicate widget is required | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `LLButton::setFloaterToggle()` — `indra/llui/llui.cpp:178` | Move model-facing work into `ALImageLookKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterLightBox::onClickResetGroupDefault()` — `indra/newview/alfloaterlightbox.cpp:80` | Move model-facing work into `ALImageLookKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterLightBox::updateTonemapper()` — `indra/newview/alfloaterlightbox.cpp:1564, 1565, 1566, 1567, 1568, 1569, 1584, 1585, 1586, 1587, 1602, 1603, 1604, 1605` | Move model-facing work into `ALImageLookKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **78 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-image_easy-tallest-850` | Only `image_easy` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-image_easy-tallest-1160` | Only `image_easy` is put in its tallest local state at 1160 px; all other cards stay default. |
| `tonemap_type_combo-option-0` | Only `tonemap_type_combo` selects `Khronos Neutral`; other selectors remain default. |
| `tonemap_type_combo-option-1` | Only `tonemap_type_combo` selects `ACES`; other selectors remain default. |
| `tonemap_type_combo-option-2` | Only `tonemap_type_combo` selects `ACES Boosted`; other selectors remain default. |
| `tonemap_type_combo-option-3` | Only `tonemap_type_combo` selects `Reinhard`; other selectors remain default. |
| `tonemap_type_combo-option-4` | Only `tonemap_type_combo` selects `Filmic`; other selectors remain default. |
| `tonemap_type_combo-option-5` | Only `tonemap_type_combo` selects `GT`; other selectors remain default. |
| `tonemap_type_combo-option-6` | Only `tonemap_type_combo` selects `AgX`; other selectors remain default. |
| `tonemap_type_combo-option-7` | Only `tonemap_type_combo` selects `AMD LPM`; other selectors remain default. |
| `tonemap_type_combo-option-8` | Only `tonemap_type_combo` selects `S-Log3 Capture`; other selectors remain default. |
| `tonemap_legacy_gamma_skies-on` | Only `tonemap_legacy_gamma_skies` is on; dependent visibility is measured. |
| `card-tonemap-tallest-850` | Only `tonemap` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-tonemap-tallest-1160` | Only `tonemap` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-sharpen-tallest-850` | Only `sharpen` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-sharpen-tallest-1160` | Only `sharpen` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-grade-tallest-850` | Only `grade` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-grade-tallest-1160` | Only `grade` is put in its tallest local state at 1160 px; all other cards stay default. |
| `set_RenderColorGrade-on` | Only `set_RenderColorGrade` is on; dependent visibility is measured. |
| `colorlut_combo-option-0` | Only `colorlut_combo` selects `None`; other selectors remain default. |
| `colorlut_combo-option-1` | Only `colorlut_combo` selects `Neutral`; other selectors remain default. |
| `colorlut_combo-option-2` | Only `colorlut_combo` selects `CrossProcess - Warm`; other selectors remain default. |
| `colorlut_combo-option-3` | Only `colorlut_combo` selects `CrossProcess - Cool`; other selectors remain default. |
| `colorlut_combo-option-4` | Only `colorlut_combo` selects `Acid Trip`; other selectors remain default. |
| `colorlut_combo-option-5` | Only `colorlut_combo` selects `Battleblue`; other selectors remain default. |
| `colorlut_combo-option-6` | Only `colorlut_combo` selects `Crunchy`; other selectors remain default. |
| `colorlut_combo-option-7` | Only `colorlut_combo` selects `Dreary`; other selectors remain default. |
| `colorlut_combo-option-8` | Only `colorlut_combo` selects `Fall Painty`; other selectors remain default. |
| `colorlut_combo-option-9` | Only `colorlut_combo` selects `Harshness`; other selectors remain default. |
| `colorlut_combo-option-10` | Only `colorlut_combo` selects `Nuked`; other selectors remain default. |
| `colorlut_combo-option-11` | Only `colorlut_combo` selects `OG Boy`; other selectors remain default. |
| `colorlut_combo-option-12` | Only `colorlut_combo` selects `Old Photo`; other selectors remain default. |
| `colorlut_combo-option-13` | Only `colorlut_combo` selects `Pastel Dreams`; other selectors remain default. |
| `colorlut_combo-option-14` | Only `colorlut_combo` selects `Purple Haze`; other selectors remain default. |
| `colorlut_combo-option-15` | Only `colorlut_combo` selects `Sickly Blue`; other selectors remain default. |
| `colorlut_combo-option-16` | Only `colorlut_combo` selects `Sinner 1`; other selectors remain default. |
| `colorlut_combo-option-17` | Only `colorlut_combo` selects `Sinner 2`; other selectors remain default. |
| `colorlut_combo-option-18` | Only `colorlut_combo` selects `Tealness`; other selectors remain default. |
| `colorlut_combo-option-19` | Only `colorlut_combo` selects `Toonland`; other selectors remain default. |
| `colorlut_combo-option-20` | Only `colorlut_combo` selects `Warm`; other selectors remain default. |
| `colorlut_combo-option-21` | Only `colorlut_combo` selects `AESTHETICS 1`; other selectors remain default. |
| `colorlut_combo-option-22` | Only `colorlut_combo` selects `AESTHETICS 2`; other selectors remain default. |
| `colorlut_combo-option-23` | Only `colorlut_combo` selects `AESTHETICS 3`; other selectors remain default. |
| `colorlut_combo-option-24` | Only `colorlut_combo` selects `AESTHETICS 4`; other selectors remain default. |
| `colorlut_combo-option-25` | Only `colorlut_combo` selects `AESTHETICS 5`; other selectors remain default. |
| `card-grade_levels-tallest-850` | Only `grade_levels` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-grade_levels-tallest-1160` | Only `grade_levels` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-split-tallest-850` | Only `split` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-split-tallest-1160` | Only `split` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-vignette-tallest-850` | Only `vignette` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-vignette-tallest-1160` | Only `vignette` is put in its tallest local state at 1160 px; all other cards stay default. |
| `set_RenderVignetteCorrectAspect-on` | Only `set_RenderVignetteCorrectAspect` is on; dependent visibility is measured. |
| `card-grain-tallest-850` | Only `grain` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-grain-tallest-1160` | Only `grain` is put in its tallest local state at 1160 px; all other cards stay default. |
| `set_RenderFilmGrainStyle-option-0` | Only `set_RenderFilmGrainStyle` selects `Mono luma`; other selectors remain default. |
| `set_RenderFilmGrainStyle-option-1` | Only `set_RenderFilmGrainStyle` selects `Colour (digital)`; other selectors remain default. |
| `set_RenderFilmGrainStyle-option-2` | Only `set_RenderFilmGrainStyle` selects `Coarse (16mm)`; other selectors remain default. |
| `set_RenderFilmGrainStyle-option-3` | Only `set_RenderFilmGrainStyle` selects `Photon shot (CCD)`; other selectors remain default. |
| `set_RenderFilmGrainAnimated-on` | Only `set_RenderFilmGrainAnimated` is on; dependent visibility is measured. |
| `card-ca-tallest-850` | Only `ca` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-ca-tallest-1160` | Only `ca` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-khronos-tallest-850` | Only `khronos` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-khronos-tallest-1160` | Only `khronos` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-lpm-tallest-850` | Only `lpm` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-lpm-tallest-1160` | Only `lpm` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-slog-tallest-850` | Only `slog` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-slog-tallest-1160` | Only `slog` is put in its tallest local state at 1160 px; all other cards stay default. |
| `slog_curve_combo-option-0` | Only `slog_curve_combo` selects `Sony S-Log3`; other selectors remain default. |
| `slog_curve_combo-option-1` | Only `slog_curve_combo` selects `Sony S-Log2`; other selectors remain default. |
| `slog_curve_combo-option-2` | Only `slog_curve_combo` selects `ARRI LogC3`; other selectors remain default. |
| `slog_curve_combo-option-3` | Only `slog_curve_combo` selects `Cineon / DPX`; other selectors remain default. |
| `slog_range_combo-option-0` | Only `slog_range_combo` selects `Full`; other selectors remain default. |
| `slog_range_combo-option-1` | Only `slog_range_combo` selects `Legal / Video`; other selectors remain default. |
| `card-adv_image-tallest-850` | Only `adv_image` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-adv_image-tallest-1160` | Only `adv_image` is put in its tallest local state at 1160 px; all other cards stay default. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## Ambiguous same-name candidates (never auto-bound)

None.

## Rejected out-of-scope same-name candidates

69 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

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
