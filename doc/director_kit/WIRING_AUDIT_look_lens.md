# Wiring audit: Look + Lens pages (static, read-only)

Scope: the 8 pages in I:/alchemy-machinima/indra/newview/skins/default/xui/en/. The Filters page embeds panel_lens_looks.xml and the Focus page embeds panel_dof.xml, so their controls are audited here as part of those pages. Static only: no build, no run, no commit, no fetch.

Method: each control's setting was checked for presence, type and default in app_settings/settings.xml and settings_alchemy.xml, and its range checked against the default. Code-bound widgets were traced in the C++ (alkitrow.cpp, alkitcard.cpp, alpanellenslooks.cpp, alpaneldof.cpp, llfloaterdirector.cpp, allenslooks.cpp).

Counts: 88 controls + 1 page-text item. OK 86, UNBOUND 1, TYPE-MISMATCH 1, LABEL-MISMATCH 1 (page text), MISSING-SETTING 0, RANGE 0, V1-MISMATCH 0, UNSURE 0.

## Problems

| page | widget | kind | label | binding | verdict | reason |
|---|---|---|---|---|---|---|
| panel_dpage_look_bloom_glow.xml:87-88 | open_lightbox_glow | button (bind=code, action=lightbox:glow_settings) | Lightbox | none | UNBOUND | No setting and no handler. alkitrow.cpp:98 stores mMeta.action and nothing reads it. ALKitBindingRegistry::add (alkitbinding.cpp:268) is never called, so resolveBinding (alkitrow.cpp:596) finds nothing. No commit callback is attached to open_lightbox_glow anywhere in newview. Clicking does nothing. The target tab glow_settings does exist (floater_lightbox_settings.xml:4040). |
| panel_dof.xml:72-101 (on panel_dpage_lens_focus.xml) | dof_res_combo | combo_box | DoF Resolution | CameraDoFResScale (settings.xml, F32 0.7) | TYPE-MISMATCH | Combo bound to an F32 setting. Combos in this UI use String/S32 bindings. Item values are strings 0.25/0.5/0.7/1.0. The 0.7 default should select Three-Quarter through LLSD coercion, but this was not verified at runtime. Low risk. |
| panel_dpage_lens_diopter.xml:3 | kit_page summary | page text | Ultimate Diopter | none | LABEL-MISMATCH | Summary says the page has "everyday controls: on/off, preset, strength and focus split". The page contains only the launcher button card_diopter_open. The controls are in the full Diopter editor, so the summary overstates what is on the page. |

## OK rows

| page | widget | kind | label | binding (setting or handler) | verdict | reason |
|---|---|---|---|---|---|---|
| panel_dpage_look_bloom_glow.xml:9-11 | set_RenderBloomHDR | toggle / kit_switch | HDR bloom | RenderBloomHDR, Boolean, default 1 | OK | |
| panel_dpage_look_bloom_glow.xml:12-16 | set_RenderBloomStrength | slider | Strength | RenderBloomStrength, F32 0.325 in [0,2] | OK | |
| panel_dpage_look_bloom_glow.xml:17-21 | set_RenderBloomThreshold | slider | Threshold | RenderBloomThreshold, F32 2.5 in [0,4] | OK | |
| panel_dpage_look_bloom_glow.xml:22-26 | set_RenderBloomKnee | slider | Knee | RenderBloomKnee, F32 0.5 in [0,1] | OK | |
| panel_dpage_look_bloom_glow.xml:27-29 | set_RenderBloomHalation | toggle | Halation | RenderBloomHalation, Boolean 0 | OK | |
| panel_dpage_look_bloom_glow.xml:30-34 | set_RenderBloomHalationStrength | slider | Halation amt | RenderBloomHalationStrength, F32 0 in [0,1] | OK | |
| panel_dpage_look_bloom_glow.xml:35-37 | set_RenderBloomHalationTint | color_swatch | Halation tint | RenderBloomHalationTint, Color3 (1, 0.35, 0.15) | OK | default is the warm amber the help text describes (settings_alchemy.xml:1877-1889) |
| panel_dpage_look_bloom_glow.xml:38-42 | set_RenderBloomScatter | slider | Scatter | RenderBloomScatter, F32 1.0 in [0.5,2.5] | OK | |
| panel_dpage_look_bloom_glow.xml:43-45 | set_RenderBloomMipCount | spinner | Mip levels | RenderBloomMipCount, S32 5 in [3,7] | OK | |
| panel_dpage_look_bloom_glow.xml:46-56 | set_RenderBloomResolutionScale | combo | Resolution | RenderBloomResolutionScale, S32 0; items 0-4 | OK | |
| panel_dpage_look_bloom_glow.xml:57-59 | set_RenderBloomAlphaGlowBoost | spinner | Glow boost | RenderBloomAlphaGlowBoost, F32 2.5 in [0,8] | OK | |
| panel_dpage_look_bloom_glow.xml:62-64 | set_RenderGlow | toggle | Use glow | RenderGlow, Boolean 1; reload listeners llviewercontrol.cpp:1172-1173 | OK | help says turning it on reloads the renderer; listeners exist |
| panel_dpage_look_bloom_glow.xml:65-69 | glow_strength_slider | slider | Strength | RenderGlowStrength, F32 0.325 in [0,1] | OK | |
| panel_dpage_look_bloom_glow.xml:70-74 | glow_width_slider | slider | Softness | RenderGlowWidth, F32 1.3 in [0,10] | OK | label says Softness, setting is Width; help text describes width, acceptable |
| panel_dpage_look_bloom_glow.xml:75-79 | glow_iteration_slider | slider | Iterations | RenderGlowIterations, S32 2 in [1,10] | OK | |
| panel_dpage_look_bloom_glow.xml:80-84 | glow_resPow_slider | slider | Resolution | RenderGlowResolutionPow, S32 9 in [0,10] | OK | |
| panel_dpage_look_actor_style.xml:15 | actor_style_target | combo | Actor | roster; llfloaterdirector.cpp:503-504 -> onActorStyleTargetChanged (3294) | OK | model LLDirectorCast, not a setting |
| panel_dpage_look_actor_style.xml:19 | actor_style_enabled | check | Enable actor styling | ActorStyle.mEnabled; wired 523, written 3315 | OK | master switch; dependents gated at 3480-3571 |
| panel_dpage_look_actor_style.xml:23 | actor_style_mode | combo | Render mode | ActorStyle.mMode; items 0/1 = ACTOR_STYLE_LAYER/REPLACE (lldirectorcast.h:131-135); clamp 3316-3319 | OK | |
| panel_dpage_look_actor_style.xml:32 | actor_style_look | combo | Look | ActorStyle.mStyle; items 0-36 match ACTOR_LOOK_* (lldirectorcast.h:137-155); clamp 3320-3323 | OK | |
| panel_dpage_look_actor_style.xml:74 | actor_style_use_actor_hue | check | Use actor hue | ActorStyle.mUseActorHue, default true; written 3324 | OK | |
| panel_dpage_look_actor_style.xml:75 | actor_style_hue | slider | Custom hue | mHue default 200, range 0-360; written 3325 | OK | |
| panel_dpage_look_actor_style.xml:78 | actor_style_alpha | slider | Effect opacity | mAlpha default 0.6, range 0-1; written 3326 | OK | |
| panel_dpage_look_actor_style.xml:81 | actor_style_dissolve_progress | slider | Dissolve | mDissolveProgress default 0.5, range 0-1; written 3327; shown only when look==10 (3567-3569) | OK | Dissolve is look 10 in the combo |
| panel_dpage_look_actor_style.xml:84 | actor_style_pixel | slider | Pixelate | mPixelSize default 0, range 0-64; written 3329 | OK | |
| panel_dpage_look_actor_style.xml:87 | actor_style_shimmer_speed | slider | Shimmer Hz | mShimmerSpeed default 1, range 0-10; written 3330 | OK | |
| panel_dpage_look_actor_style.xml:90 | actor_style_shimmer_amount | slider | Shimmer amt | mShimmerAmount default 0, range 0-1; written 3331 | OK | |
| panel_dpage_look_actor_style.xml:93 | actor_style_glitch | slider | Glitch | mGlitch default 0, range 0-1; written 3332 | OK | |
| panel_dpage_look_actor_style.xml:99 | actor_style_distortion | combo | Distortion | mDistortion clamp 0-8 (3333-3334); items 0-8 | OK | |
| panel_dpage_look_actor_style.xml:112 | actor_style_distortion_amount | slider | Distort amt | mDistortionAmount default 0.5, range 0-1; written 3336 | OK | |
| panel_dpage_look_actor_style.xml:115 | actor_style_brightness | slider | Brightness | mBrightness default 1, range 0.05-1.5; written 3337 | OK | |
| panel_dpage_look_actor_style.xml:118 | actor_style_effect_fps | spinner | Effect fps | mEffectFps default 0, range 0-30; written 3338 | OK | |
| panel_dpage_look_actor_style.xml:119 | actor_style_reset | button | Reset actor style | onActorStyleReset (3346-3355) writes ActorStyle() defaults, mEnabled=false (lldirectorcast.h:167-184) | OK | matches "turn styling off" |
| panel_dpage_look_image.xml:9 | card_image_open | button (launcher) | Open Lightbox Image | addLauncherCard llfloaterdirector.cpp:1340-1342 -> ALFloaterLightBox::showTab("colortone_settings") (floater_lightbox_settings.xml:29) | OK | |
| panel_dpage_look_stylize.xml:9 | card_rotoink_open | button (launcher) | Open Roto Ink | llfloaterdirector.cpp:1354-1356 -> rotoink_settings (floater_lightbox_settings.xml:5380) | OK | |
| panel_dpage_look_stylize.xml:14 | card_tron_open | button (launcher) | Open Tron | llfloaterdirector.cpp:1362-1364 -> tron_settings (floater_lightbox_settings.xml:5856) | OK | |
| panel_dpage_look_tron.xml:6 | kc_open (kit_card kind=launcher) | button kc_open_launch | Open Tron in Lightbox | alkitcard.cpp:406-422, target=lightbox -> showTab(tron_settings) | OK | |
| panel_dpage_lens_diopter.xml:9 | card_diopter_open | button (launcher) | Open Ultimate Diopter | llfloaterdirector.cpp:1300-1302 -> showInstance("ultimate_diopter") (llviewerfloaterreg.cpp:769) | OK | the page summary overstates content; see Problems |
| panel_lens_looks.xml:5-14 | RenderGradNDEnabled | check | Graduated ND | Boolean 0 | OK | master for enabled_control children |
| panel_lens_looks.xml:20-34 | gradnd_preset_combo | combo | Preset | RenderGradNDPreset, U32 1; items 0-6 | OK | |
| panel_lens_looks.xml:39-40 | gradnd_density_slider / _spinner | slider + spinner | Density | RenderGradNDDensity, F32 1.5 in [0,6] | OK | |
| panel_lens_looks.xml:43-46 | gradnd_confine_slider / _spinner | slider + spinner | Sky Confine | RenderGradNDSkyConfine, F32 1.0 in [0,1] | OK | |
| panel_lens_looks.xml:49-52 | gradnd_sunw_slider / _spinner | slider + spinner | Sun Weight | RenderGradNDSunWeight, F32 0.4 in [0,2] | OK | |
| panel_lens_looks.xml:55-58 | gradnd_sunr_slider / _spinner | slider + spinner | Sun Radius | RenderGradNDSunRadius, F32 0.6 in [0.1,2] | OK | |
| panel_lens_looks.xml:61-64 | gradnd_pos_slider / _spinner | slider + spinner | Position | RenderGradNDPosition, F32 0.5 in [0,1] | OK | |
| panel_lens_looks.xml:67-70 | gradnd_soft_slider / _spinner | slider + spinner | Softness | RenderGradNDSoftness, F32 0.35 in [0.01,1] | OK | |
| panel_lens_looks.xml:73-76 | gradnd_angle_slider / _spinner | slider + spinner | Tilt (deg) | RenderGradNDAngle, F32 0 in [-45,45] | OK | |
| panel_lens_looks.xml:77-80 | RenderGradNDFlip | check | Darken bottom | Boolean 0 | OK | |
| panel_lens_looks.xml:82 | gradnd_tint_swatch | color_swatch | Tint | RenderGradNDTintColor, Color3 | OK | |
| panel_lens_looks.xml:83 | gradnd_tint_amount | slider | Tint | RenderGradNDTintAmount, F32 0 in [0,1] | OK | |
| panel_lens_looks.xml:85 | lens_filter_preset | combo | Lens Filter | RenderLensFilterPreset, U32 0; items 0-8 match the MIRED table | OK | |
| panel_lens_looks.xml:86 | lens_filter_amount | slider | Lens Filter | RenderLensFilterAmount, F32 0 in [0,1] | OK | |
| panel_lens_looks.xml:89-93 | RenderPolarizerEnabled | check | Polarizer | Boolean 0 | OK | master switch |
| panel_lens_looks.xml:96-107 | polarizer_preset_combo | combo | Preset | RenderPolarizerPreset, U32 3; items 0-4 | OK | |
| panel_lens_looks.xml:110-113 | pol_strength_slider / _spinner | slider + spinner | Strength | RenderPolarizerStrength, F32 0.7 in [0,1] | OK | |
| panel_lens_looks.xml:116-119 | pol_glare_slider / _spinner | slider + spinner | Glare Cut | RenderPolarizerGlareStrength, F32 2.0 in [0,4] | OK | |
| panel_lens_looks.xml:122-125 | pol_glarethr_slider / _spinner | slider + spinner | Glare Gate | RenderPolarizerGlareThreshold, F32 3.0 in [0.5,8] | OK | |
| panel_lens_looks.xml:128-131 | pol_glaredepth_slider / _spinner | slider + spinner | Glare Depth | RenderPolarizerGlareMaxDepth, F32 0.98 in [0.5,1] | OK | |
| panel_lens_looks.xml:134-137 | pol_skysat_slider / _spinner | slider + spinner | Sky Sat | RenderPolarizerSkySaturation, F32 1.4 in [1,2.5] | OK | |
| panel_lens_looks.xml:140-143 | pol_skydark_slider / _spinner | slider + spinner | Sky Darken | RenderPolarizerSkyDarken, F32 0.3 in [0,1.5] | OK | |
| panel_lens_looks.xml:144-147 | RenderPolarizerAutoBand | check | Auto band | Boolean 1 | OK | |
| panel_lens_looks.xml:150-153 | pol_bandrad_slider / _spinner | slider + spinner | Band Radius | RenderPolarizerBandRadius, F32 0.5 in [0.1,1.5] | OK | |
| panel_lens_looks.xml:156 | RenderInfraredEnabled | check | Infrared Film | Boolean 0 | OK | |
| panel_lens_looks.xml:158 | ir_preset_action | combo (action) | Look | alpanellenslooks.cpp:47-52 -> ALLensLooks::applyInfraredPreset (allenslooks.cpp:249-259); 7 entries in IR_PRESETS (allenslooks.cpp:58-66) cover items 1-7; the writes target existing settings | OK | |
| panel_lens_looks.xml:160 | ir_mode | combo | Mode | RenderInfraredMode, U32 0; items 0-1 | OK | |
| panel_lens_looks.xml:162 | ir_strength | slider | Strength | RenderInfraredStrength, F32 1 in [0,1] | OK | |
| panel_lens_looks.xml:164 | ir_enable_tag | button | Enable avatar tag | alpanellenslooks.cpp:59-70 -> RenderGBufferAvatarTag after EnableGBufferAvatarTagConfirm (notifications.xml:194); Boolean 0 | OK | |
| panel_lens_looks.xml:167 | RenderSoftEdgeEnabled | check | Soft Edge / Sweet Spot | Boolean 0 | OK | |
| panel_lens_looks.xml:169 | softedge_preset_action | combo (action) | Preset | alpanellenslooks.cpp:53-58 -> applySoftEdgePreset (allenslooks.cpp:261-267); 6 entries in SOFT_PRESETS (allenslooks.cpp:83-90) cover items 1-6 | OK | |
| panel_lens_looks.xml:171 | softedge_blur_type | combo | Blur type | RenderSoftEdgeBlurType, U32 0; items 0-1 | OK | |
| panel_lens_looks.xml:173 | softedge_blur | slider | Blur | RenderSoftEdgeBlur, F32 0.5 in [0,1] | OK | |
| panel_dof.xml:5-14 | RenderDepthOfField | check | Depth of Field | Boolean 0 (settings.xml) | OK | master for enabled_control children |
| panel_dof.xml:15-25 | RenderFocusPointFollowsPointer | check | Follow Mouse | Boolean 0 | OK | |
| panel_dof.xml:26-36 | RenderFocusPointLocked | check | Lock Focus | Boolean 0 | OK | |
| panel_dof.xml:37-47 | FocusCrosshair (RenderFocusPointCrosshair) | check | Show Focus Crosshair | Boolean 0 | OK | |
| panel_dof.xml:48-60 | RenderDepthOfFieldFront (RenderDepthOfFieldNearBlur) | check | Enable Foreground Blur | Boolean 0 | OK | |
| panel_dof.xml:112-125 | dof_fov_slider / _spinner | slider + spinner | Field of View | CameraFieldOfView, F32 60 in [10,175] | OK | reset dofFOV_default_btn :142-157 -> Machinima.ResetControl (registered alpanelcinecamparams.cpp:84-110, called alpaneldof.cpp:13) |
| panel_dof.xml:168-181 | dof_fstop_slider / _spinner | slider + spinner | Aperture (f) | CameraFNumber, F32 9 in [0.5,256] | OK | reset :198-213 |
| panel_dof.xml:225-238 | dof_length_slider / _spinner | slider + spinner | Focal Length (mm) | CameraFocalLength, F32 50 in [10,600] | OK | reset :255-270 |
| panel_dof.xml:283-296 | dof_coc_slider / _spinner | slider + spinner | CoC Maximum | CameraMaxCoF, F32 10 in [0,40] | OK | reset :313-328 |
| panel_dof.xml:340-353 | dof_focaltime_slider / _spinner | slider + spinner | Transition Time (s) | CameraFocusTransitionTime, F32 0.5 in [0,2] | OK | reset :370-385 |
| panel_dof.xml:397-407 | ShowGuides | check | Show Composition Guide | RenderCompositionGuide, Boolean 0 | OK | |
| panel_dof.xml:408-441 | GuideStyle | combo | Style | RenderCompositionGuideStyle, String rule_of_thirds; 6 string items match | OK | |
| panel_dof.xml:442-453 | GuideColor | color_swatch | (none) | RenderCompositionGuideColor, Color3 | OK | |
| panel_dof.xml:465-477 | GuideThickness | spinner | Thickness | RenderCompositionGuideWidth, F32 2.0 in [0,20] | OK | |
| panel_dof.xml:489-500 | GuideOpacity | slider | Opacity | RenderCompositionGuideVisibility, F32 0.5 in [0,1] | OK | Opacity is guide alpha (pipeline.cpp:8935; director_kit_index.xml:1175) |

## Notes

- v1 parity: bloom and glow keys are the same in floater_lightbox_settings.xml (RenderGlow*) and floater_preferences_graphics_advanced.xml (RenderBloomMipCount), so no V1-MISMATCH. panel_dof.xml and panel_lens_looks.xml are shared files used by v1 and v2, so they write identical keys. The Actor FX names also exist in v1 floater_director.xml (lines 1529-1645), and both init blocks (llfloaterdirector.cpp:484-526 and 1016-1084) wire the same handlers to the same model.
- Settings existence: all bound keys were found in settings.xml or settings_alchemy.xml. Types match the widgets except the DoF Resolution combo noted above. No default falls outside its min/max.
- Preset appliers write extra keys (RenderInfraredFoliageEV, RenderSoftEdgeShape, etc.); all of them exist in settings_alchemy.xml.
- Not checked: runtime behaviour, actual enablement gating, the write site of actor_style_status (read-only text), and tooltip claims about behaviour beyond what the code shows.
