# Director v2 on/off wiring audit (static)

Date: 2026-10-12. Read-only audit of source. No build, no run, no commit, no fetch. Only this file was written.

Scope: every on/off control in the Director v2 UI: panel_dpage_*.xml, floater_director_v2.xml, and the sub-panels they embed.

## Result

| Verdict | Count | Meaning |
|---|---|---|
| MISMATCH | 0 | writes a different setting than the legacy checkbox for the same feature |
| UNBOUND | 0 | no setting and no handler |
| MISSING-SETTING | 0 | bound to a name not defined in any settings file |
| OK | 144 | bound to an existing settings key; legacy twin (if any) writes the same key |
| OK-HANDLER | 77 | no settings key; bound by a C++ commit or read handler to model state (not UNBOUND) |
| Total | 221 | |

## Problems (MISMATCH / UNBOUND / MISSING-SETTING)

None found.

Checks behind this: all 42 settings names referenced by v2 rows, and all 130 control_name values across v2 pages and embedded sub-panels, exist in settings.xml, settings_alchemy.xml, settings_per_account.xml or settings_per_account_alchemy.xml. No v2 control shares a widget name with a legacy checkbox that writes a different setting. The 56 same-label pairs with different settings were reviewed and are distinct features that share a generic label (for example "Gizmo", "Actor only", "Armed", "Level horizon", "Enabled").

## Notes

- Kit runtime: kit_row bind="setting" binds the row setting attribute (alkitrow.cpp:583). Card and option enable switches bind via alkitcard.cpp:270-271 and :287. Page masters bind via alkitpage.cpp:499. bind="none" and bind="code" rows resolve through ALKitBindingRegistry, but no add() call exists in newview, so C++ commit or read handlers by widget name do the binding. Each of those was traced.
- bind="none" VCam rows: alpaneldirectorvcam.cpp onControl (:878-896) and dispatch to commitCamera (:1135, writes capture->mCamera), commitDisplay (:1199 region, effect struct) and commitProjector (:1282 region). vc_protect (:896) writes PrismAdaptivePerformance, which matches the legacy "Protect the main view" checkbox (floater_prism_manager.xml:729-733). The legacy guide and effect checkboxes (llfloaterprismmanager.cpp:398-456) serialize the same capture.mCamera fields (llprismlens.cpp:6267-6270, 6740-6747).
- vc_bone_enabled is a no-op unless the camera source is Virtual (alpaneldirectorvcam.cpp:1180 gates on settings.mVirtual). This is a behaviour gate, not a wiring fault.
- db_block_keep_facing (alpaneldanceball.cpp:272) has no commit callback. It is read only when the Face action runs (alpaneldanceball.cpp:1513). This is intended, so it is OK-HANDLER.
- cine_fixture_mode writes CineLightRig{Key,Fill,Rim,Bg}FixtureMode, chosen by the selected role (alpanelcinelightrig.cpp:64-66, 1281-1286, 1359-1365). All four keys exist.
- cine_group_*, cine_*_shaft and cine_*_hero write model state on ALCineLightRig (alcinelightrig.cpp:1048, 1055, 1326), not settings keys.
- RenderDepthOfFieldFront (panel_dof.xml) has control RenderDepthOfFieldNearBlur, labelled "Enable Foreground Blur". The widget name does not match its control, but the label does. This is a naming note only; it is marked OK.
- show_paths_global_check (floater_director_v2.xml:123, kit_bar) was missing from the first pass. It binds ActorMoverShowHeading, the same key as the legacy floater_director.xml:184. OK.
- Bloom rows set_RenderBloomHDR and set_RenderBloomHalation have no legacy XUI checkbox. They bind their own child kit_switch control_name, which matches the row setting.

## Table 1: kit_row toggles (60 rows, panel_dpage_*.xml and floater_director_v2.xml)

| Page | Widget | Label | Bind | Setting or state written | Key exists | Legacy twin | Verdict |
|---|---|---|---|---|---|---|---|
| camera_vcam | vc_bone_enabled | Bone POV | none | alpaneldirectorvcam.cpp:1180 commitCamera, capture->mCamera.mBonePov.mEnabled (gated by mVirtual) | n/a | none found | OK-HANDLER |
| camera_vcam | vc_bone_scale | Clone-scale aware | none | alpaneldirectorvcam.cpp:1179, mCamera.mBonePov.mScaleAware | n/a | floater_prism_manager.xml bone_pov_scale_aware (no control_name, llfloaterprismmanager.cpp:398) | OK-HANDLER |
| camera_vcam | vc_guide_show | Show guide | none | alpaneldirectorvcam.cpp:1151, mCamera.mShowGuide | n/a | none found | OK-HANDLER |
| camera_vcam | vc_guide_thirds | Rule of thirds | none | alpaneldirectorvcam.cpp:1152, mCamera.mGuideThirds | n/a | floater_prism_manager.xml guide_thirds (llfloaterprismmanager.cpp:406, same field, llprismlens.cpp:6267) | OK-HANDLER |
| camera_vcam | vc_guide_up | Up / roll | none | alpaneldirectorvcam.cpp:1153, mCamera.mGuideUpRoll | n/a | floater_prism_manager.xml guide_uproll (llfloaterprismmanager.cpp:407) | OK-HANDLER |
| camera_vcam | vc_guide_cross | Center crosshair | none | alpaneldirectorvcam.cpp:1154, mCamera.mGuideCrosshair | n/a | floater_prism_manager.xml guide_crosshair (llfloaterprismmanager.cpp:408) | OK-HANDLER |
| camera_vcam | vc_guide_clip | Clip range markers | none | alpaneldirectorvcam.cpp:1155, mCamera.mGuideClipMarkers | n/a | floater_prism_manager.xml guide_clipmarkers (llfloaterprismmanager.cpp:409) | OK-HANDLER |
| camera_vcam | vc_proj_flip_h | Flip H | none | alpaneldirectorvcam.cpp:1307 commitProjector, projector mFlipH | n/a | none found | OK-HANDLER |
| camera_vcam | vc_proj_flip_v | Flip V | none | alpaneldirectorvcam.cpp:1308, projector mFlipV | n/a | none found | OK-HANDLER |
| camera_vcam | vc_proj_follow_exposure | Follow main exposure | none | alpaneldirectorvcam.cpp:1315, projector mFollowMainExposure | n/a | none found | OK-HANDLER |
| camera_vcam | vc_proj_keep_gobo | Keep the light's gobo | none | alpaneldirectorvcam.cpp:1330, projector mKeepGobo | n/a | none found | OK-HANDLER |
| camera_vcam | vc_tv_flip_h | Flip H (TV) | none | alpaneldirectorvcam.cpp:1217 commitDisplay, effect mFlipH | n/a | floater_prism_manager.xml effect_flip_h (llfloaterprismmanager.cpp:454) | OK-HANDLER |
| camera_vcam | vc_tv_flip_v | Flip V (TV) | none | alpaneldirectorvcam.cpp:1218, effect mFlipV | n/a | none found | OK-HANDLER |
| camera_vcam | vc_tv_rotate | Rotate 90 | none | alpaneldirectorvcam.cpp:1219, effect mRotate90 | n/a | floater_prism_manager.xml effect_rotate90 (llfloaterprismmanager.cpp:456) | OK-HANDLER |
| camera_vcam | vc_protect | Protect main view | none | alpaneldirectorvcam.cpp:896 onControl, PrismAdaptivePerformance | Y | floater_prism_manager.xml:729-733 control_name=PrismAdaptivePerformance ("Protect the main view") | OK |
| light_cine_light_rig | cine_easy_night_mask_enable | Night | setting | CineLightRigNightMaskEnabled (row and child control agree) | Y | panel_cine_light_rig.xml cine_easy_night_mask_enable, same key | OK |
| light_cine_light_rig | cine_easy_probe_gizmo | Gizmo | setting | CineLightRigLiveProbeGizmo | Y | panel_cine_light_rig.xml cine_easy_probe_gizmo, same key | OK |
| light_cine_light_rig | cine_gizmo | Gizmo | setting | CineLightRigGizmo | Y | panel_cine_light_rig.xml cine_gizmo, same key | OK |
| light_cine_light_rig | cine_group_self | You | code | alpanelcinelightrig.cpp:661 and onGroupSlotsCommit (:1669), group slot bit 0, ALCineLightRig::setGroupSlots (alcinelightrig.cpp:1055) | n/a | panel_cine_light_rig.xml cine_group_self (no control_name, same handler) | OK-HANDLER |
| light_cine_light_rig | cine_group_a | Subject A | code | same, group slot bit 1 | n/a | panel_cine_light_rig.xml cine_group_a (no control_name) | OK-HANDLER |
| light_cine_light_rig | cine_group_b | Subject B | code | same, group slot bit 2 | n/a | panel_cine_light_rig.xml cine_group_b (no control_name) | OK-HANDLER |
| light_cine_light_rig | cine_group_c | Subject C | code | same, group slot bit 3 | n/a | panel_cine_light_rig.xml cine_group_c (no control_name) | OK-HANDLER |
| light_cine_light_rig | cine_group_d | Subject D | code | same, group slot bit 4 | n/a | panel_cine_light_rig.xml cine_group_d (no control_name) | OK-HANDLER |
| light_cine_light_rig | cine_random_cycle | Random cycle | setting | CineLightRigRandomCycle | Y | panel_cine_light_rig.xml cine_random_cycle, same key | OK |
| light_cine_light_rig | cine_manual | Manual | setting | CineLightRigManual | Y | panel_cine_light_rig.xml cine_manual, same key | OK |
| light_cine_light_rig | cine_key_shaft | Shaft | code | alpanelcinelightrig.cpp:872, 877-882, ALCineLightRig::setShaftEnabled (alcinelightrig.cpp:1326), role Key | n/a | panel_cine_light_rig.xml cine_key_shaft (no control_name) | OK-HANDLER |
| light_cine_light_rig | cine_key_hero | Hero | code | alpanelcinelightrig.cpp:873, 883-888, setHeroEnabled, role Key | n/a | panel_cine_light_rig.xml cine_key_hero (no control_name) | OK-HANDLER |
| light_cine_light_rig | cine_key_actor_only | Actor only | setting | CineLightRigKeyActorOnly | Y | panel_cine_light_rig.xml cine_key_actor_only, same key | OK |
| light_cine_light_rig | cine_ratio_lock | Ratio lock | setting | CineLightRigRatioLock | Y | panel_cine_light_rig.xml cine_ratio_lock, same key | OK |
| light_cine_light_rig | cine_fill_shaft | Shaft | code | setShaftEnabled, role Fill | n/a | panel_cine_light_rig.xml cine_fill_shaft (no control_name) | OK-HANDLER |
| light_cine_light_rig | cine_fill_hero | Hero | code | setHeroEnabled, role Fill | n/a | panel_cine_light_rig.xml cine_fill_hero (no control_name) | OK-HANDLER |
| light_cine_light_rig | cine_fill_actor_only | Actor only | setting | CineLightRigFillActorOnly | Y | panel_cine_light_rig.xml cine_fill_actor_only, same key | OK |
| light_cine_light_rig | cine_rim_shaft | Shaft | code | setShaftEnabled, role Rim | n/a | panel_cine_light_rig.xml cine_rim_shaft (no control_name) | OK-HANDLER |
| light_cine_light_rig | cine_rim_hero | Hero | code | setHeroEnabled, role Rim | n/a | panel_cine_light_rig.xml cine_rim_hero (no control_name) | OK-HANDLER |
| light_cine_light_rig | cine_rim_actor_only | Actor only | setting | CineLightRigRimActorOnly | Y | panel_cine_light_rig.xml cine_rim_actor_only, same key | OK |
| light_cine_light_rig | cine_bg_shaft | Shaft | code | setShaftEnabled, role Bg | n/a | panel_cine_light_rig.xml cine_bg_shaft (no control_name) | OK-HANDLER |
| light_cine_light_rig | cine_bg_hero | Hero | code | setHeroEnabled, role Bg | n/a | panel_cine_light_rig.xml cine_bg_hero (no control_name) | OK-HANDLER |
| light_cine_light_rig | cine_bg_actor_only | Actor only | setting | CineLightRigBgActorOnly | Y | panel_cine_light_rig.xml cine_bg_actor_only, same key | OK |
| light_cine_light_rig | cine_mirror | Mirror | setting | CineLightRigMirror | Y | panel_cine_light_rig.xml cine_mirror, same key | OK |
| light_cine_light_rig | cine_auto_shadow_slots | Auto shadows | setting | CineLightRigAutoShadowSlots | Y | panel_cine_light_rig.xml cine_auto_shadow_slots, same key | OK |
| light_cine_light_rig | cine_bounce | Bounce | setting | CineLightRigBounceEnabled | Y | panel_cine_light_rig.xml cine_bounce, same key | OK |
| light_cine_light_rig | cine_bounce_lights_set | Bounce set | setting | CineLightRigBounceLightsSet | Y | panel_cine_light_rig.xml cine_bounce_lights_set, same key | OK |
| light_cine_light_rig | cine_actor_only_all | Actor only | setting | CineLightRigActorOnlyAll | Y | panel_cine_light_rig.xml cine_actor_only_all, same key | OK |
| light_cine_light_rig | cine_catchlight_actor_only | Actor only | setting | CineLightRigCatchlightActorOnly | Y | panel_cine_light_rig.xml cine_catchlight_actor_only, same key | OK |
| light_cine_light_rig | cine_fixture_mode | Use fixture | code | alpanelcinelightrig.cpp:683, 848-849, onFixtureModeCommit (:1359), writes CineLightRig{role}FixtureMode | Y (all 4 roles) | panel_cine_light_rig.xml cine_fixture_mode (no control_name, same handler) | OK |
| light_cine_light_rig | cine_live_probe_gizmo | Gizmo | setting | CineLightRigLiveProbeGizmo | Y | panel_cine_light_rig.xml cine_live_probe_gizmo, same key | OK |
| light_cine_light_rig | cine_night_mask_gizmo | Gizmo | setting | CineLightRigNightMaskGizmo | Y | panel_cine_light_rig.xml cine_night_mask_gizmo, same key | OK |
| light_rim | cine_rig_rim_include_alpha | Alpha surfaces | setting | CineRigRimIncludeAlpha | Y | panel_cine_light_rig.xml cine_rig_rim_include_alpha, same key | OK |
| light_rim | cine_rig_rim_include_probes | Probes | setting | CineRigRimIncludeProbes | Y | panel_cine_light_rig.xml, same key | OK |
| light_rim | cine_rig_rim_include_bounce | Bounce fills | setting | CineRigRimIncludeBounce | Y | panel_cine_light_rig.xml, same key | OK |
| light_rim | cine_rimq_actors | Actors only | setting | CineRigRimActorsOnly | Y | panel_cine_light_rig.xml cine_rimq_actors, same key | OK |
| light_rim | cine_rimq_actors_alpha | Alpha actors | setting | CineRigRimActorsOnlyAlpha | Y | panel_cine_light_rig.xml cine_rimq_actors_alpha, same key (label differs: "Alpha (Phase 2)") | OK |
| light_rim | cine_rig_rim_debug | Rim only | setting | CineRigRimDebugRimOnly | Y | panel_cine_light_rig.xml cine_rig_rim_debug, same key | OK |
| look_bloom_glow | set_RenderBloomHDR | HDR bloom | setting | RenderBloomHDR (child kit_switch control_name agrees) | Y | none found in v1 XUI | OK |
| look_bloom_glow | set_RenderBloomHalation | Halation | setting | RenderBloomHalation (child control_name agrees) | Y | none found in v1 XUI | OK |
| look_bloom_glow | set_RenderGlow | Use glow | setting | RenderGlow (child control_name agrees) | Y | floater_lightbox_settings.xml "Use Glow Shader" control_name=RenderGlow | OK |
| director_arm (floater_director_v2) | arm_moves_check | Actor moves | setting | DirectorArmMoves | Y | floater_director.xml:133 control_name=DirectorArmMoves | OK |
| director_arm (floater_director_v2) | arm_camera_check | Camera | setting | DirectorArmCamera | Y | floater_director.xml:144 | OK |
| director_arm (floater_director_v2) | arm_play_check | Recorder play | setting | DirectorArmRecorderPlay | Y | floater_director.xml:155 | OK |
| director_arm (floater_director_v2) | arm_capture_check | Recorder capture | setting | DirectorArmRecorderCapture | Y | floater_director.xml:166 | OK |

## Table 2: kit card, option and page master switches (13)

| Page | Widget | Label | Binding | Setting or state written | Key exists | Legacy twin | Verdict |
|---|---|---|---|---|---|---|---|
| light_cine_light_rig | cine_key_on | Key (option) | enable_setting=CineLightRigKeyOn (alkitcard.cpp:287) | CineLightRigKeyOn | Y | panel_cine_light_rig.xml cine_key_on, same key | OK |
| light_cine_light_rig | cine_fill_on | Fill (option) | enable_setting=CineLightRigFillOn | CineLightRigFillOn | Y | panel_cine_light_rig.xml cine_fill_on, same key | OK |
| light_cine_light_rig | cine_rim_on | Back (option) | enable_setting=CineLightRigRimOn | CineLightRigRimOn | Y | panel_cine_light_rig.xml cine_rim_on, same key | OK |
| light_cine_light_rig | cine_bg_on | BG (option) | enable_setting=CineLightRigBgOn | CineLightRigBgOn | Y | panel_cine_light_rig.xml cine_bg_on, same key | OK |
| light_cine_light_rig | cine_easy_probe_enable | Easy probe (card) | enable_setting (alkitcard.cpp:270-271) | CineLightRigLiveProbeEnabled | Y | panel_cine_light_rig.xml cine_easy_probe_enable, same key | OK |
| light_cine_light_rig | cine_group_enable | Who's lit (card) | enable_widget only; runtime switch with no key | rig group enable: onGroupEnabledCommit (alpanelcinelightrig.cpp:1662) to ALCineLightRig::setGroupEnabled (alcinelightrig.cpp:1048) | n/a | panel_cine_light_rig.xml cine_group_enable (no control_name) | OK-HANDLER |
| light_cine_light_rig | cine_catchlight | Eye catchlight (card) | enable_setting | CineLightRigCatchlight | Y | panel_cine_light_rig.xml cine_catchlight, same key | OK |
| light_cine_light_rig | cine_live_probe_enable | Live probe (card) | enable_setting | CineLightRigLiveProbeEnabled | Y | panel_cine_light_rig.xml cine_live_probe_enable, same key | OK |
| light_cine_light_rig | cine_night_mask_enable | Night mask (card) | enable_setting | CineLightRigNightMaskEnabled | Y | panel_cine_light_rig.xml cine_night_mask_enable, same key | OK |
| light_rim | cine_rimq_wrap_enable | Light wrap (card) | enable_setting | CineRigRimWrapEnabled | Y | panel_cine_light_rig.xml cine_rimq_wrap_enable, same key | OK |
| light_cine_light_rig | cine_enabled | Light rig (page master) | master_setting (alkitpage.cpp:499) | CineLightRigEnabled | Y | panel_cine_light_rig.xml cine_enabled, same key | OK |
| light_rim | cine_rig_rim_enable | Rim light (page master) | master_setting | CineRigRimEnabled | Y | panel_cine_light_rig.xml cine_rig_rim_enable, same key | OK |
| camera_vcam | vc_enable | Virtual Cam (page master) | master_setting; also onControl alpaneldirectorvcam.cpp:878 | PrismLensEnabled | Y | floater_director.xml prism_lens_enabled; floater_prism_manager.xml prism_enabled (both control_name=PrismLensEnabled) | OK |

## Table 3: v2 page check boxes and kit bar switch (6)

| Page | Widget | Label | Binding | Setting or state written | Key exists | Legacy twin | Verdict |
|---|---|---|---|---|---|---|---|
| camera_moves (panel_dpage_camera_moves.xml:27) | hero_arc_speed_ramp | Speed ramp | check_box control_name | CinematicCamSpeedRampEnabled | Y | floater_director.xml:1197, same key | OK |
| capture_scene_options (panel_dpage_capture_scene_options.xml:5) | scene_hotkeys_enabled | Enable Director hotkeys | check_box control_name | DirectorHotkeysEnabled | Y | floater_director.xml:1758, same key | OK |
| floater_director_v2 kit_transport (floater_director_v2.xml:123) | show_paths_global_check | (none) | kit_switch control_name | ActorMoverShowHeading | Y | floater_director.xml:184 (same key) | OK |
| look_actor_style (panel_dpage_look_actor_style.xml:19) | actor_style_enabled | Enable actor styling | check_box, no control_name; commit loop llfloaterdirector.cpp:505-524 | style.mEnabled (llfloaterdirector.cpp:3315, 3533) | n/a | floater_director.xml actor_style_enabled (no control_name, same handler) | OK-HANDLER |
| look_actor_style (panel_dpage_look_actor_style.xml:74) | actor_style_use_actor_hue | Use actor hue | check_box, no control_name; commit loop llfloaterdirector.cpp:509, 522-523 | style.mUseActorHue (llfloaterdirector.cpp:3324, 3536) | n/a | floater_director.xml actor_style_use_actor_hue (no control_name) | OK-HANDLER |
| performance_animate (panel_dpage_performance_animate.xml:53) | locomotion_allow_ao | AO | check_box, no control_name; commit llfloaterdirector.cpp:560-563 | locomotion entry mAllowAO (llfloaterdirector.cpp:3683, 3788) | n/a | floater_director.xml locomotion_allow_ao (no control_name) | OK-HANDLER |

## Table 4: sub-panel switches bound to a settings key (96 rows, embedded v1 panels)

Each row is the same element as the embedded legacy panel. Key existence was checked per row.

| Page | Sub-panel | Widget | Label | Setting | Key exists | Legacy twin | Verdict |
|---|---|---|---|---|---|---|---|
| camera_frame_orbit | panel_cinecam_frame.xml | frame_lens_enabled | Reframe as a lens / gate change | CinematicFrameLensEnabled | Y | same element in panel_cinecam_frame.xml (embedded v1) | OK |
| camera_frame_orbit | panel_cinecam_frame.xml | auto_frame_enabled | Fit and place from avatar joints | CinematicAutoFrameEnabled | Y | same element in panel_cinecam_frame.xml (embedded v1) | OK |
| camera_frame_orbit | panel_cinecam_frame.xml | frame_guide_enabled | Show format guide | CinematicFrameGuideEnabled | Y | same element in panel_cinecam_frame.xml (embedded v1) | OK |
| camera_frame_orbit | panel_flycam_orbit.xml | FlycamOrbitEnabled | Enable Flycam Orbit | FlycamOrbitEnabled | Y | same element in panel_flycam_orbit.xml (embedded v1) | OK |
| camera_frame_orbit | panel_flycam_orbit.xml | CinematicCamUseSelected | Target selected avatar | CinematicCamUseSelected | Y | same element in panel_flycam_orbit.xml (embedded v1) | OK |
| camera_frame_orbit | panel_flycam_orbit.xml | FlycamOrbitLevel | Level horizon | FlycamOrbitLevel | Y | same element in panel_flycam_orbit.xml (embedded v1) | OK |
| camera_operator | panel_cinecam_operator.xml | CameraShakeApplyActive | Apply to active camera | CameraShakeApplyActive | Y | same element in panel_cinecam_operator.xml (embedded v1) | OK |
| camera_operator | panel_cinecam_operator.xml | FlycamOperatorEnabled | Use on live flycam | FlycamOperatorEnabled | Y | same element in panel_cinecam_operator.xml (embedded v1) | OK |
| camera_operator | panel_cinecam_operator.xml | FlycamOperatorForceWalk | Force gait | FlycamOperatorForceWalk | Y | same element in panel_cinecam_operator.xml (embedded v1) | OK |
| camera_shot | panel_director_switcher.xml | switcher_armed | Armed | DirectorSwitcherArmed | Y | same element in panel_director_switcher.xml (embedded v1) | OK |
| camera_shot | panel_director_switcher.xml | switcher_auto | Auto | DirectorSwitcherAuto | Y | same element in panel_director_switcher.xml (embedded v1) | OK |
| camera_shot | panel_director_switcher.xml | switcher_ease | Ease cuts | DirectorSwitcherEaseCuts | Y | same element in panel_director_switcher.xml (embedded v1) | OK |
| camera_shot | panel_director_switcher.xml | switcher_ease_freeze | Slow / freeze world | DirectorSwitcherEaseFreeze | Y | same element in panel_director_switcher.xml (embedded v1) | OK |
| camera_shot | panel_director_switcher.xml | switcher_overrides_flycam | Override flycam | DirectorSwitcherOverridesFlycam | Y | same element in panel_director_switcher.xml (embedded v1) | OK |
| camera_shot | panel_cinecam_params.xml | CinematicCamEnabled | Enable Cinematic Camera | CinematicCamEnabled | Y | same element in panel_cinecam_params.xml (embedded v1) | OK |
| camera_shot | panel_cinecam_params.xml | CinematicCamUseSelected | Target selected avatar | CinematicCamUseSelected | Y | same element in panel_cinecam_params.xml (embedded v1) | OK |
| camera_shot | panel_cinecam_params.xml | CinematicCamLookAtHead | Frame head | CinematicCamLookAtHead | Y | same element in panel_cinecam_params.xml (embedded v1) | OK |
| camera_shot | panel_cinecam_params.xml | CinematicCamUseOperator | Handheld operator | CinematicCamUseOperator | Y | same element in panel_cinecam_params.xml (embedded v1) | OK |
| camera_shot | panel_cinecam_params.xml | CineFisheyeEnabled | Fisheye | CineFisheyeEnabled | Y | same element in panel_cinecam_params.xml (embedded v1) | OK |
| camera_shot | panel_cinecam_params.xml | CinematicCamBoneHorizonLock | Level horizon | CinematicCamBoneHorizonLock | Y | same element in panel_cinecam_params.xml (embedded v1) | OK |
| camera_shot | panel_cinecam_params.xml | CinematicCamSweepPingPong | Ping-pong | CinematicCamSweepPingPong | Y | same element in panel_cinecam_params.xml (embedded v1) | OK |
| camera_shot | panel_cinecam_params.xml | CinematicCamBarrelOscillate | Oscillate | CinematicCamBarrelOscillate | Y | same element in panel_cinecam_params.xml (embedded v1) | OK |
| camera_takes | panel_flycam_recorder.xml | FlycamRecSmooth | Smooth splines | FlycamRecSmooth | Y | same element in panel_flycam_recorder.xml (embedded v1) | OK |
| camera_takes | panel_flycam_recorder.xml | FlycamRecFollowAnchor | Follow anchor | FlycamRecFollowAnchor | Y | same element in panel_flycam_recorder.xml (embedded v1) | OK |
| camera_takes | panel_flycam_recorder.xml | FlycamRecUseOperator | Handheld operator layer on playback | FlycamRecUseOperator | Y | same element in panel_flycam_recorder.xml (embedded v1) | OK |
| capture_temporal | panel_temporal_capture.xml | drive_anim_check | Animation & actor motion | TemporalDriveAnimation | Y | same element in panel_temporal_capture.xml (embedded v1) | OK |
| capture_temporal | panel_temporal_capture.xml | drive_objects_check | Object motion (spin, paths) | TemporalDriveObjects | Y | same element in panel_temporal_capture.xml (embedded v1) | OK |
| capture_temporal | panel_temporal_capture.xml | drive_texanim_check | Texture animation | TemporalDriveTextureAnim | Y | same element in panel_temporal_capture.xml (embedded v1) | OK |
| capture_temporal | panel_temporal_capture.xml | drive_particles_check | Particles | TemporalDriveParticles | Y | same element in panel_temporal_capture.xml (embedded v1) | OK |
| capture_temporal | panel_temporal_capture.xml | drive_camera_check | Camera (stays real-time in this version) | TemporalDriveCamera | Y | same element in panel_temporal_capture.xml (embedded v1) | OK |
| cast_ghosts | panel_ghost_studio.xml | mirror_hold_check | Hold on src change | GhostMirrorHoldOnSourceChange | Y | same element in panel_ghost_studio.xml (embedded v1) | OK |
| cast_ghosts | panel_ghost_studio.xml | mirror_repeat_held_check | Repeat held | GhostMirrorRepeatHeldAnimesh | Y | same element in panel_ghost_studio.xml (embedded v1) | OK |
| cast_move | panel_actor_mover.xml | custom_anim_check | Custom walk anim (UUID) | ActorMoverUseCustomAnim | Y | same element in panel_actor_mover.xml (embedded v1) | OK |
| cast_path | panel_path_editor.xml | onion_skin_check | Pose ghosts | PathShowOnionSkin | Y | same element in panel_path_editor.xml (embedded v1) | OK |
| cast_path | panel_path_editor.xml | path_camera_enable_check | Enable authored path camera for this session | PathCameraEnabled | Y | same element in panel_path_editor.xml (embedded v1) | OK |
| lens_filters | panel_lens_looks.xml | (unnamed) | Graduated ND (pre-tonemap) | RenderGradNDEnabled | Y | same element in panel_lens_looks.xml (embedded v1) | OK |
| lens_filters | panel_lens_looks.xml | (unnamed) | Darken bottom instead (Custom) | RenderGradNDFlip | Y | same element in panel_lens_looks.xml (embedded v1) | OK |
| lens_filters | panel_lens_looks.xml | (unnamed) | Polarizer (pre-tonemap) | RenderPolarizerEnabled | Y | same element in panel_lens_looks.xml (embedded v1) | OK |
| lens_filters | panel_lens_looks.xml | (unnamed) | Auto band (ease off near sun) | RenderPolarizerAutoBand | Y | same element in panel_lens_looks.xml (embedded v1) | OK |
| lens_filters | panel_lens_looks.xml | ir_enabled | Infrared Film (pre-tonemap) | RenderInfraredEnabled | Y | same element in panel_lens_looks.xml (embedded v1) | OK |
| lens_filters | panel_lens_looks.xml | softedge_enabled | Soft Edge / Sweet Spot (pre-tonemap) | RenderSoftEdgeEnabled | Y | same element in panel_lens_looks.xml (embedded v1) | OK |
| lens_focus | panel_dof.xml | (unnamed) | Depth of Field | RenderDepthOfField | Y | same element in panel_dof.xml (embedded v1) | OK |
| lens_focus | panel_dof.xml | (unnamed) | Follow Mouse | RenderFocusPointFollowsPointer | Y | same element in panel_dof.xml (embedded v1) | OK |
| lens_focus | panel_dof.xml | (unnamed) | Lock Focus (alt+shift+X) | RenderFocusPointLocked | Y | same element in panel_dof.xml (embedded v1) | OK |
| lens_focus | panel_dof.xml | FocusCrosshair | Show Focus Crosshair | RenderFocusPointCrosshair | Y | same element in panel_dof.xml (embedded v1) | OK |
| lens_focus | panel_dof.xml | RenderDepthOfFieldFront | Enable Foreground Blur | RenderDepthOfFieldNearBlur | Y | same element in panel_dof.xml (embedded v1) | OK |
| lens_focus | panel_dof.xml | ShowGuides | Show Composition Guide | RenderCompositionGuide | Y | same element in panel_dof.xml (embedded v1) | OK |
| light_actors | panel_avatar_light_response.xml | chk_enabled | Enable Avatar Light Response | AvatarLightResponseEnabled | Y | same element in panel_avatar_light_response.xml (embedded v1) | OK |
| light_actors | panel_avatar_light_response.xml | chk_debug_tint | Debug: magenta tint | AvatarLightResponseDebugTint | Y | same element in panel_avatar_light_response.xml (embedded v1) | OK |
| light_exposure | panel_exposure_hub.xml | exp_auto_exposure | Auto exposure | RenderDynamicExposureEnabled | Y | same element in panel_exposure_hub.xml (embedded v1) | OK |
| light_exposure | panel_exposure_hub.xml | exp_legacy_gamma_skies | Tonemap legacy-gamma skies | AlchemyTonemapLegacyGammaSkies | Y | same element in panel_exposure_hub.xml (embedded v1) | OK |
| light_exposure | panel_exposure_hub.xml | exp_moon_linked | Link Moon to Sun | AlchemyEnvMoonLinked | Y | same element in panel_exposure_hub.xml (embedded v1) | OK |
| light_exposure | panel_exposure_hub.xml | exp_local_include_rig | Local lights EV includes rig lights | AlchemyEnvLocalLightIncludeRig | Y | same element in panel_exposure_hub.xml (embedded v1) | OK |
| light_exposure | panel_exposure_hub.xml | exp_gnd_enabled | Graduated ND (pre-tonemap) | RenderGradNDEnabled | Y | same element in panel_exposure_hub.xml (embedded v1) | OK |
| light_exposure | panel_exposure_hub.xml | exp_pol_enabled | Polarizer (pre-tonemap) | RenderPolarizerEnabled | Y | same element in panel_exposure_hub.xml (embedded v1) | OK |
| light_exposure | panel_exposure_hub.xml | exp_nm_enabled | Enable Night Mask | CineLightRigNightMaskEnabled | Y | same element in panel_exposure_hub.xml (embedded v1) | OK |
| light_exposure | panel_exposure_hub.xml | exp_ir_enabled | Enable infrared | RenderInfraredEnabled | Y | same element in panel_exposure_hub.xml (embedded v1) | OK |
| light_exposure | panel_exposure_hub.xml | exp_softedge_enabled | Enable soft edge | RenderSoftEdgeEnabled | Y | same element in panel_exposure_hub.xml (embedded v1) | OK |
| performance_animate | panel_anim_preview.xml | capture_all_check | Capture all avatars | AnimExplorerCaptureAll | Y | same element in panel_anim_preview.xml (embedded v1) | OK |
| performance_gaze | panel_lens_gaze.xml | look_at_camera_enabled | Look at camera (global master) | DirectorLookAtCameraEnabled | Y | same element in panel_lens_gaze.xml (embedded v1) | OK |
| performance_gaze | panel_lens_gaze.xml | gaze_random_check | Auto-cycle presets | DirectorGazeRandomMode | Y | same element in panel_lens_gaze.xml (embedded v1) | OK |
| performance_gaze | panel_lens_gaze.xml | look_at_camera_torso | Enabled | DirectorLookAtCameraTorso | Y | same element in panel_lens_gaze.xml (embedded v1) | OK |
| performance_polish | panel_pose_polish.xml | polish_master_check | Enable Pose Polish (master) | ALPolishEnabled | Y | same element in panel_pose_polish.xml (embedded v1) | OK |
| performance_polish | panel_pose_polish.xml | polish_inertia_check | Transition inertialization (stops AO/gesture snapping) | ALPolishInertiaEnabled | Y | same element in panel_pose_polish.xml (embedded v1) | OK |
| performance_polish | panel_pose_polish.xml | polish_contact_check | Foot contact stabilizer (IK) | ALPolishContactEnabled | Y | same element in panel_pose_polish.xml (embedded v1) | OK |
| performance_polish | panel_pose_polish.xml | polish_phasematch_check | Phase-matched walk/run starts | ALPolishPhaseMatchEnabled | Y | same element in panel_pose_polish.xml (embedded v1) | OK |
| performance_polish | panel_pose_polish.xml | polish_loopseam_check | Loop-seam repair | ALPolishLoopSeamEnabled | Y | same element in panel_pose_polish.xml (embedded v1) | OK |
| performance_polish | panel_pose_polish.xml | polish_secondary_check | Secondary motion (breath + idle sway) | ALPolishSecondaryEnabled | Y | same element in panel_pose_polish.xml (embedded v1) | OK |
| performance_polish | panel_pose_polish.xml | polish_diagnostic_check | Layer diagnostic overlay | ALPolishLayerDiagnostic | Y | same element in panel_pose_polish.xml (embedded v1) | OK |
| performance_switchboard | panel_director_anim_switcher.xml | anim_switcher_armed | Armed | DirectorAnimSwitcherArmed | Y | same element in panel_director_anim_switcher.xml (embedded v1) | OK |
| performance_switchboard | panel_director_anim_switcher.xml | anim_switcher_auto | Auto | DirectorAnimSwitcherAuto | Y | same element in panel_director_anim_switcher.xml (embedded v1) | OK |
| performance_switchboard | panel_director_anim_switcher.xml | anim_sequence | Sequence | DirectorAnimSwitcherSequence | Y | same element in panel_director_anim_switcher.xml (embedded v1) | OK |
| world_atmosphere | panel_atmospheric_volumes.xml | localfog_master | Enable local atmospheric volumes | BDMergeLocalFogVolumes | Y | same element in panel_atmospheric_volumes.xml (embedded v1) | OK |
| world_atmosphere | panel_projector_shafts.xml | (unnamed) | Enable volumetric projector cones | BDMergeProjectorVolumetrics | Y | same element in panel_projector_shafts.xml (embedded v1) | OK |
| world_atmosphere | panel_projector_shafts.xml | (unnamed) | Half-res march (faster) | BDMergeProjectorVolumetricsHalfRes | Y | same element in panel_projector_shafts.xml (embedded v1) | OK |
| world_atmosphere | panel_projector_shafts.xml | (unnamed) | Temporal accumulation (needs half-res) | BDMergeProjectorVolumetricsTemporal | Y | same element in panel_projector_shafts.xml (embedded v1) | OK |
| world_atmosphere | panel_projector_shafts.xml | (unnamed) | Track airborne beam depth | BDMergeProjectorVolumetricsTemporalBeamDepth | Y | same element in panel_projector_shafts.xml (embedded v1) | OK |
| world_atmosphere | panel_projector_shafts.xml | (unnamed) | Scissor cull | BDMergeProjectorVolumetricsScissor | Y | same element in panel_projector_shafts.xml (embedded v1) | OK |
| world_atmosphere | panel_projector_shafts.xml | (unnamed) | Adaptive samples | BDMergeProjectorVolumetricsAdaptive | Y | same element in panel_projector_shafts.xml (embedded v1) | OK |
| world_atmosphere | panel_projector_shafts.xml | (unnamed) | Temporal anti-banding (no extra samples) | BDMergeProjectorVolumetricsTemporalDither | Y | same element in panel_projector_shafts.xml (embedded v1) | OK |
| world_atmosphere | panel_projector_shafts.xml | ps_BDMergeFroxelWindInverted_check | Invert wind direction | BDMergeFroxelWindInverted | Y | same element in panel_projector_shafts.xml (embedded v1) | OK |
| world_atmosphere | panel_projector_shafts.xml | (unnamed) | Soft projector shadows (contact-hardening + fill) | BDMergeSoftProjectorShadows | Y | same element in panel_projector_shafts.xml (embedded v1) | OK |
| world_atmosphere | panel_projector_shafts.xml | (unnamed) | Also soften sun/moon shadows | BDMergeSoftShadowSun | Y | same element in panel_projector_shafts.xml (embedded v1) | OK |
| world_atmosphere | panel_projector_shafts.xml | (unnamed) | Gobo anisotropic/mip filtering (anti-alias) | BDMergeGoboAnisotropic | Y | same element in panel_projector_shafts.xml (embedded v1) | OK |
| world_weather | panel_weather_settings.xml | weather_enabled | Cinematic weather (viewer local) | AlchemyWeatherEnabled | Y | same element in panel_weather_settings.xml (embedded v1) | OK |
| world_weather | panel_weather_settings.xml | weather_rain_enabled | Depth-aware volumetric rain | AlchemyWeatherRainEnabled | Y | same element in panel_weather_settings.xml (embedded v1) | OK |
| world_weather | panel_weather_settings.xml | weather_splash_enabled | Procedural surface splashes | AlchemyWeatherSplashEnabled | Y | same element in panel_weather_settings.xml (embedded v1) | OK |
| world_weather | panel_weather_settings.xml | weather_wetness_enabled | Wet-surface Fresnel sheen | AlchemyWeatherWetnessEnabled | Y | same element in panel_weather_settings.xml (embedded v1) | OK |
| world_weather | panel_weather_settings.xml | weather_mist_enabled | Near-ground rain mist | AlchemyWeatherMistEnabled | Y | same element in panel_weather_settings.xml (embedded v1) | OK |
| world_weather | panel_weather_settings.xml | weather_lens_enabled | Camera-lens drop caustics (POV) | AlchemyWeatherLensDropsEnabled | Y | same element in panel_weather_settings.xml (embedded v1) | OK |
| world_weather | panel_weather_settings.xml | weather_rain_occlusion | Per-pixel vertical rain cover | AlchemyWeatherRainOcclusion | Y | same element in panel_weather_settings.xml (embedded v1) | OK |
| world_weather | panel_weather_settings.xml | weather_shelter_fade | Camera-ray fallback / lens cover | AlchemyWeatherShelterFade | Y | same element in panel_weather_settings.xml (embedded v1) | OK |
| world_weather | panel_weather_settings.xml | weather_lightning_enabled | Automatic and manual lightning | AlchemyWeatherLightningEnabled | Y | same element in panel_weather_settings.xml (embedded v1) | OK |
| world_weather | panel_weather_settings.xml | weather_lightning_quality_enabled | Cinematic HDR lightning quality | AlchemyWeatherLightningQualityEnabled | Y | same element in panel_weather_settings.xml (embedded v1) | OK |
| world_weather | panel_weather_settings.xml | weather_lightning_sheet_enabled | Cloud-shaped sheet illumination | AlchemyWeatherLightningSheetEnabled | Y | same element in panel_weather_settings.xml (embedded v1) | OK |
| world_weather | panel_weather_settings.xml | weather_lightning_wet_glint_enabled | Rain-wet surface lightning glint | AlchemyWeatherLightningWetGlintEnabled | Y | same element in panel_weather_settings.xml (embedded v1) | OK |

## Table 5: sub-panel switches bound to C++ handlers, no settings key (46 rows, OK-HANDLER)

Each was checked for a getChild lookup, a commit callback (or an action-time read), and a model write. The write target is named where it was traced; otherwise the table says so.

| Page | Sub-panel | Widget | Label | Handler (file:line) | State written | Verdict |
|---|---|---|---|---|---|---|
| camera_shot | panel_director_switcher.xml | switcher_slot_enabled | Use in auto | alpaneldirectorswitcher.cpp:72; commit :138; read :318 | camera switcher bank slot mEnabled (model) | OK-HANDLER |
| camera_shot | panel_director_switcher.xml | switcher_custom_enabled | Custom angle | alpaneldirectorswitcher.cpp:80; commit :148; read :372 | switcher custom-angle flag (model) | OK-HANDLER |
| performance_switchboard | panel_director_anim_switcher.xml | anim_slot_loop | Loop | alpaneldirectoranimswitcher.cpp:74; commit and read present | anim switcher slot (model; target not traced) | OK-HANDLER |
| performance_switchboard | panel_director_anim_switcher.xml | anim_slot_snap | Snap on cut | alpaneldirectoranimswitcher.cpp:75; commit and read present | anim switcher slot (model) | OK-HANDLER |
| performance_switchboard | panel_director_anim_switcher.xml | anim_slot_enabled | Use in auto | alpaneldirectoranimswitcher.cpp:76; commit and read present | anim switcher slot (model) | OK-HANDLER |
| cast_ghosts | panel_ghost_studio.xml | show_all_check | Show ghosts | alpanelghoststudio.cpp:276; commit and read present | ghost show-all state (model) | OK-HANDLER |
| cast_ghosts | panel_ghost_studio.xml | edit_ghosts_check | Edit ghosts | alpanelghoststudio.cpp:277; commit and read present | ghost edit mode (model) | OK-HANDLER |
| cast_ghosts | panel_ghost_studio.xml | chaos_check | Chaos | alpanelghoststudio.cpp:295; commit and read present | ghost spec (model) | OK-HANDLER |
| cast_ghosts | panel_ghost_studio.xml | autoanim_check | AutoAnimate | alpanelghoststudio.cpp:299; commit and read present | ghost spec (model) | OK-HANDLER |
| cast_ghosts | panel_ghost_studio.xml | physics_check | Physics | alpanelghoststudio.cpp:311; commit and read present | ghost spec (model) | OK-HANDLER |
| cast_ghosts | panel_ghost_studio.xml | lens_gaze_check | This actor | alpanelghoststudio.cpp:320; commit and read present | ghost lens-gaze flag (model) | OK-HANDLER |
| cast_ghosts | panel_ghost_studio.xml | keep_facing_check | Keep facing | alpanelghoststudio.cpp:343; commit and reads present | ghost spec (model) | OK-HANDLER |
| cast_ghosts | panel_ghost_studio.xml | actor_tint_check | Actor hue | alpanelghoststudio.cpp:346; commit and reads present | ghost spec (model) | OK-HANDLER |
| cast_ghosts | panel_ghost_studio.xml | formation_terrain_check | Snap each to terrain | alpanelghoststudio.cpp:386; commit via loop :581-585; read :3478 | ghost formation spec mTerrainConform (model) | OK-HANDLER |
| cast_ghosts | panel_ghost_studio.xml | preserve_source_tilt_check | Preserve source tilt | alpanelghoststudio.cpp:389; commit via loop :582-585; read :3481 | ghost spec mPreserveSourceTilt (model) | OK-HANDLER |
| cast_ghosts | panel_ghost_studio.xml | group_edit_members_check | Edit group members | alpanelghoststudio.cpp:402; commit and read present | ghost group state (model) | OK-HANDLER |
| cast_ghosts | panel_ghost_studio.xml | group_member_pinned_check | Pin member offset | alpanelghoststudio.cpp:404; commit and read present | ghost group state (model) | OK-HANDLER |
| cast_dance | panel_dance_ball.xml | db_autojoin | Auto-join | alpaneldanceball.cpp:225; commit and read present | dance ball autoJoin (model) | OK-HANDLER |
| cast_dance | panel_dance_ball.xml | db_include_seated | Include seated | alpaneldanceball.cpp:226; commit and read present | dance ball includeSeated (model) | OK-HANDLER |
| cast_dance | panel_dance_ball.xml | db_include_me | Include me | alpaneldanceball.cpp:227; commit and read present | dance ball includeSelf (model) | OK-HANDLER |
| cast_dance | panel_dance_ball.xml | db_auto_advance | Auto-advance | alpaneldanceball.cpp:241; commit and read present | dance ball (model) | OK-HANDLER |
| cast_dance | panel_dance_ball.xml | db_t_dance_drives_face | Dance drives face | alpaneldanceball.cpp:251; commit and read present | dance ball (model) | OK-HANDLER |
| cast_dance | panel_dance_ball.xml | db_wave_enable | Enable wave | alpaneldanceball.cpp:253; commit and read present | dance ball wave (model) | OK-HANDLER |
| cast_dance | panel_dance_ball.xml | db_block_keep_facing | Keep facing | alpaneldanceball.cpp:272; read only at onBlockingFace :1513, no commit | blocking-face action argument (action-time read) | OK-HANDLER |
| cast_dance | panel_dance_ball.xml | db_blocking_on | Blocking on | alpaneldanceball.cpp:289; commit and read present | dance ball blocking (model) | OK-HANDLER |
| performance_gaze | panel_lens_gaze.xml | gaze_slot_you | You | alpanellensgaze.cpp:367; commit and read present | gaze slot mask (model) | OK-HANDLER |
| performance_gaze | panel_lens_gaze.xml | gaze_slot_a | A | alpanellensgaze.cpp:368; commit and read present | gaze slot mask (model) | OK-HANDLER |
| performance_gaze | panel_lens_gaze.xml | gaze_slot_b | B | alpanellensgaze.cpp:369; commit and read present | gaze slot mask (model) | OK-HANDLER |
| performance_gaze | panel_lens_gaze.xml | gaze_slot_c | C | alpanellensgaze.cpp:370; commit and read present | gaze slot mask (model) | OK-HANDLER |
| performance_gaze | panel_lens_gaze.xml | gaze_slot_d | D | alpanellensgaze.cpp:371; commit and read present | gaze slot mask (model) | OK-HANDLER |
| performance_gaze | panel_lens_gaze.xml | gaze_enable_check | Enabled for selection | alpanellensgaze.cpp:373; commit and reads present | gaze enable for selection (model) | OK-HANDLER |
| performance_gaze | panel_lens_gaze.xml | gaze_blinks_check | Enabled | alpanellensgaze.cpp:420; commit and reads present | gaze blink option (model) | OK-HANDLER |
| performance_gaze | panel_lens_gaze.xml | gaze_breakoff_check | Break off past | alpanellensgaze.cpp:426; commit and reads present | gaze breakoff option (model) | OK-HANDLER |
| cast_path | panel_path_editor.xml | edit_mode_check | Edit in world | alpanelpatheditor.cpp:81; commit and read present | path edit mode (model) | OK-HANDLER |
| cast_path | panel_path_editor.xml | path_groundfollow_check | Ground-follow | alpanelpatheditor.cpp:109; commit and reads present | path node flag (model) | OK-HANDLER |
| cast_path | panel_path_editor.xml | path_pitch_check | Pitch to slope | alpanelpatheditor.cpp:110; commit and reads present | path node flag (model) | OK-HANDLER |
| cast_path | panel_path_editor.xml | path_airborne_check | Air path | alpanelpatheditor.cpp:111; commit and reads present | path node flag (model) | OK-HANDLER |
| cast_path | panel_path_editor.xml | sync_take_check | Sync to camera take | alpanelpatheditor.cpp:133; commit and reads present | path sync flag (model) | OK-HANDLER |
| cast_props | panel_prop_mover.xml | hover_on_check | Hover | alpanelpropmover.cpp:67; commit and read present | prop motion option (model) | OK-HANDLER |
| cast_props | panel_prop_mover.xml | rock_on_check | Rock | alpanelpropmover.cpp:71; commit and read present | prop motion option (model) | OK-HANDLER |
| cast_props | panel_prop_mover.xml | spin_on_check | Spin | alpanelpropmover.cpp:76; commit and read present | prop motion option (model) | OK-HANDLER |
| light_actors | panel_avatar_light_response.xml | chk_only_adjusted | Only adjusted | alpanelavatarlightresponse.cpp:91; commit and reads present | avatar light response view (model) | OK-HANDLER |
| light_actors | panel_avatar_light_response.xml | chk_bypass | Bypass (A/B) | alpanelavatarlightresponse.cpp:96; commit and read present | avatar light bypass (model) | OK-HANDLER |
| light_actors | panel_avatar_light_response.xml | chk_sil_haze | Haze over silhouette | alpanelavatarlightresponse.cpp:100; commit and read present | silhouette option (model) | OK-HANDLER |
| light_actors | panel_avatar_light_response.xml | chk_sil_keep_rim | Keep Rim | alpanelavatarlightresponse.cpp:101; commit and read present | silhouette option (model) | OK-HANDLER |
| world_atmosphere | panel_atmospheric_volumes.xml | localfog_enabled | Enabled | alpanelatmosphericvolumes.cpp:51; commit and read present | local fog volume enable (model) | OK-HANDLER |
## Label-level conflicts reviewed (not mismatches)

56 pairs share a label but bind different keys. Each pair is a distinct feature: rig Gizmo variants, rig Actor-only variants, the camera switcher against the animation switcher ("Armed", "Auto"), cinecam against flycam "Level horizon", and Lens Gaze "Enabled" against audio preferences "Enabled". None is a mismatch.
