# Rig — Director Kit page contract

Page key: `cine_light_rig_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (131/187 setting-bound rows; 95 C++-referenced rows).

## Real owners

- `panel_cine_light_rig` / `panel` at `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:8` (173 matching rows).
- Referencing classes: `ALPanelCineLightRig` (136), `ALCineLightRigManager` (5), `ALCineLightRigModel` (4), `LLFloaterReg` (1), `LLReflectionMapManager` (1).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Reuse the existing gobo control and light-map view under their existing contracts; no replacement editor is introduced here.

## Row bindings

`ALCineRigKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `easy` / `cine_easy_brightness` (Brightness) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:127` | kit `slider` ↔ `CineLightRigMasterEV` `F32`; `ALCineRigKitAdapter` preserves/calls wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onEasyBrightnessCommit()` (`indra/newview/alpanelcinelightrig.cpp:864`) | `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:205` `cine_master_ev`, `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:165` `cine_master_ev`, `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:56` `exp_rig_master_ev` |
| `easy` / `cine_easy_drama` (Drama) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:130` | kit `slider` ↔ `CineLightRigRatio` `F32`; `ALCineRigKitAdapter` preserves/calls wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onEasyDramaCommit()` (`indra/newview/alpanelcinelightrig.cpp:866`) | `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:367` `cine_ratio`, `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:329` `cine_ratio` |
| `easy` / `cine_easy_warmth` (Warmth) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:147` | kit `slider` ↔ `CineLightRigMasterTempMired` `F32`; `ALCineRigKitAdapter` preserves/calls wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onEasyWarmthCommit()` (`indra/newview/alpanelcinelightrig.cpp:872`) | `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:214` `cine_master_temp`, `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:168` `cine_master_temp` |
| `easy` / `cine_easy_rim` (Back light) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:133` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onEasyRimCommit()` (`indra/newview/alpanelcinelightrig.cpp:868`) | none |
| `easy` / `cine_easy_bg` (BG) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:140` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onEasyBgCommit()` (`indra/newview/alpanelcinelightrig.cpp:870`) | none |
| `easy` / `cine_easy_cone_width` (Cone) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:150` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onEasyConeWidthCommit()` (`indra/newview/alpanelcinelightrig.cpp:874`) | none |
| `easy` / `cine_easy_cone_feather` (Edge) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:153` | kit `slider` ↔ `BDMergeProjectorVolumetricsFeather` `F32`; `ALCineRigKitAdapter` preserves/calls wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onEasyConeFeatherCommit()` (`indra/newview/alpanelcinelightrig.cpp:876`); declared SHORTCUT of `projector_shafts_tab/ps_BDMergeProjectorVolumetricsFeather_slider` (home remains canonical for search/undo) | page `projector_shafts_tab`, `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:56` `ps_BDMergeProjectorVolumetricsFeather_slider`, `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:58` `ps_BDMergeProjectorVolumetricsFeather_slider` |
| `easy` / `cine_easy_shaft_length` (Vol. shaft) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:156` | kit `slider` ↔ `BDMergeProjectorVolumetricsMaxDistance` `F32`; declared SHORTCUT of `projector_shafts_tab/ps_BDMergeProjectorVolumetricsMaxDistance_slider` (home remains canonical for search/undo) | page `projector_shafts_tab`, `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:59` `cine_easy_shaft_length`, `indra/newview/skins/default/xui/en/panel_dpage_light_projector_shafts.xml:51` `ps_BDMergeProjectorVolumetricsMaxDistance_slider`, `indra/newview/skins/default/xui/en/panel_projector_shafts.xml:106` `ps_BDMergeProjectorVolumetricsMaxDistance_slider` |
| `easy` / `cine_easy_flare_preset` (Flare) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:159` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onFlarePresetSelected()` (`indra/newview/alpanelcinelightrig.cpp:822`) | none |
| `easy` / `cine_easy_night_mask_enable` (Night) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:180` | kit `toggle` ↔ `CineLightRigNightMaskEnabled` `Boolean` | `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:829` `cine_night_mask_enable`, `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:71` `cine_easy_night_mask_enable`, `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:102` `exp_nm_enabled` |
| `easy` / `cine_easy_night_mask_darkness` (Darkness) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:185` | kit `slider` ↔ `CineLightRigNightMaskDarkness` `F32`; `ALCineRigKitAdapter` preserves/calls wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onNightMaskDarknessCommit()` (`indra/newview/alpanelcinelightrig.cpp:960`) | none |
| `easy` / `cine_easy_night_mask_preset` (Preset) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:181` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onNightMaskPresetSelected()` (`indra/newview/alpanelcinelightrig.cpp:826`) | none |
| `easy_probe` / `cine_easy_probe_radius` (PBR radius) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:163` | kit `slider` ↔ `CineLightRigLiveProbeRadius` `F32` | `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:742` `cine_live_probe_radius`, `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:88` `cine_easy_probe_radius`, `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:831` `cine_live_probe_radius` |
| `easy_probe` / `cine_easy_probe_ambiance` (Probe fill) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:175` | kit `slider` ↔ `CineLightRigLiveProbeAmbiance` `F32` | `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:762` `cine_live_probe_ambiance`, `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:93` `cine_easy_probe_ambiance`, `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:834` `cine_live_probe_ambiance` |
| `easy_probe` / `cine_easy_probe_bounce_keep` (Keep bounce) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:178` | kit `slider` ↔ `CineLightRigLiveProbeBounceKeep` `F32` | `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:721` `cine_live_probe_bounce_keep`, `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:98` `cine_easy_probe_bounce_keep`, `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:837` `cine_live_probe_bounce_keep` |
| `easy_probe` / `cine_easy_probe_target` (Probe target) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:167` | kit `combo` ↔ `CineLightRigLiveProbeTarget` `S32` | `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:730` `cine_live_probe_target`, `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:103` `cine_easy_probe_target`, `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:820` `cine_live_probe_target` |
| `easy_probe` / `cine_easy_probe_gizmo` (Gizmo) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:166` | kit `toggle` ↔ `CineLightRigLiveProbeGizmo` `Boolean` | `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:715` `cine_live_probe_gizmo`, `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:114` `cine_easy_probe_gizmo`, `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:860` `cine_live_probe_gizmo` |
| `rig_status` / `cine_gizmo` (Gizmo) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:39` | kit `toggle` ↔ `CineLightRigGizmo` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:119` `cine_gizmo` |
| `rig_status` / `cine_anchor` (Anchor) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:43` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onAnchorSelected()` (`indra/newview/alpanelcinelightrig.cpp:805`) | none |
| `rig_status` / `cine_target_object` (Target object) | `button` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:45` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onTargetObject()` (`indra/newview/alpanelcinelightrig.cpp:807`) | none |
| `rig_status` / `cine_clear_object` (Clear) | `button` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:47` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onClearObjectTarget()` (`indra/newview/alpanelcinelightrig.cpp:809`) | none |
| `rig_status` / `cine_object_target_status` (Target) | `text` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:68` | kit readout ← approved static copy; no runtime writer | none |
| `light_group` / `cine_group_self` (You) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:53` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onGroupSlotsCommit()` (`indra/newview/alpanelcinelightrig.cpp:1601`) | none |
| `light_group` / `cine_group_a` (Subject A) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:56` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onGroupSlotsCommit()` (`indra/newview/alpanelcinelightrig.cpp:1601`) | none |
| `light_group` / `cine_group_b` (Subject B) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:59` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onGroupSlotsCommit()` (`indra/newview/alpanelcinelightrig.cpp:1601`) | none |
| `light_group` / `cine_group_c` (Subject C) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:62` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onGroupSlotsCommit()` (`indra/newview/alpanelcinelightrig.cpp:1601`) | none |
| `light_group` / `cine_group_d` (Subject D) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:65` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onGroupSlotsCommit()` (`indra/newview/alpanelcinelightrig.cpp:1601`) | none |
| `rig_main` / `cine_radius` (Radius) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:202` | kit `number` ↔ `CineLightRigRadius` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:159` `cine_radius` |
| `rig_main` / `cine_offset_z` (Offset Z) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:208` | kit `number` ↔ `CineLightRigOffsetZ` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:162` `cine_offset_z` |
| `rig_main` / `cine_master_ev` (Master EV) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:205` | kit `number` ↔ `CineLightRigMasterEV` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:165` `cine_master_ev`, `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:56` `exp_rig_master_ev` |
| `rig_main` / `cine_master_temp` (Warmth) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:214` | kit `slider` ↔ `CineLightRigMasterTempMired` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:168` `cine_master_temp` |
| `rig_main` / `cine_headroom` (Headroom) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:211` | kit `number` ↔ `CineLightRigHeadroomStops` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:173` `cine_headroom`, `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:58` `exp_rig_headroom` |
| `rig_main` / `cine_track_mode` (Tracking) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:227` | kit `combo` ↔ `CineLightRigTrackMode` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:176` `cine_track_mode` |
| `rig_main` / `cine_damping` (Damping s) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:224` | kit `number` ↔ `CineLightRigDamping` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:184` `cine_damping` |
| `rig_main` / `cine_reset_all` (Reset all) | `button` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:200` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::resetAll()` (`indra/newview/alpanelcinelightrig.cpp:849`) | none |
| `rig_setups` / `cine_random_cycle` (Random cycle) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:92` | kit `toggle` ↔ `CineLightRigRandomCycle` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:192` `cine_random_cycle` |
| `rig_setups` / `cine_random_interval` (Interval s) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:94` | kit `slider` ↔ `CineLightRigRandomInterval` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:195` `cine_random_interval` |
| `rig_setups` / `cine_easing` (Easing) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:86` | kit `combo` ↔ `CineLightRigEasing` `U32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:200` `cine_easing` |
| `rig_setups` / `cine_transition` (Transition s) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:221` | kit `number` ↔ `CineLightRigTransitionSec` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:210` `cine_transition` |
| `rig_effects` / `cine_fx_combo` (Effect) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:107` | kit `combo` ↔ `CineLightRigFX` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:215` `cine_fx_combo` |
| `rig_effects` / `cine_fx_stop` (Stop) | `button` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:108` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALCineLightRigManager::stopFX()` (`indra/newview/alpanelcinelightrig.cpp:832`) | none |
| `rig_effects` / `cine_manual` (Manual) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:109` | kit `toggle` ↔ `CineLightRigManual` `Boolean`; `ALCineRigKitAdapter` preserves/calls wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onManualCommit()` (`indra/newview/alpanelcinelightrig.cpp:897`) | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:224` `cine_manual` |
| `rig_effects` / `cine_seed` (Seed) | `line_editor` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:111` | kit `number` ↔ `CineLightRigSeed` `U32`; `ALCineRigKitAdapter` preserves/calls wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::commitSeed()` (`indra/newview/alpanelcinelightrig.cpp:856`) | none |
| `rig_effects` / `cine_seed_randomize` (Randomize) | `button` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:112` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::randomizeSeed()` (`indra/newview/alpanelcinelightrig.cpp:858`) | none |
| `lights [Key]` / `cine_key_ev` (EV) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:323` | kit `number` ↔ `CineLightRigKeyEV` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:242` `cine_key_ev` |
| `lights [Key]` / `cine_key_profile` (Colour) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:318` | kit `combo` ↔ `CineLightRigKeyProfile` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:245` `cine_key_profile` |
| `lights [Key]` / `cine_key_yaw` (Yaw) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:304` | kit `dial` ↔ `CineLightRigKeyYaw` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:251` `cine_key_yaw` |
| `lights [Key]` / `cine_key_pitch` (Pitch) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:309` | kit `arc` ↔ `CineLightRigKeyPitch` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:254` `cine_key_pitch` |
| `lights [Key]` / `cine_key_gel` (Gel) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:336` | kit `combo` ↔ `CineLightRigKeyGel` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:257` `cine_key_gel` |
| `lights [Key]` / `cine_key_gobo` (Gobo) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:331` | kit `combo` ↔ `CineLightRigKeyGobo` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:263` `cine_key_gobo` |
| `lights [Key]` / `cine_key_shaft` (Shaft) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:328` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALCineLightRigManager::setShaftEnabled()` (`indra/newview/alpanelcinelightrig.cpp:956`) | none |
| `lights [Key]` / `cine_key_offset_z` (Height) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:314` | kit `slider` ↔ `CineLightRigKeyOffsetZ` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:273` `cine_key_offset_z` |
| `lights [Key]` / `cine_key_cone_width` (Cone) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:326` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onManualConeWidthCommit()` (`indra/newview/alpanelcinelightrig.cpp:2348`) | none |
| `lights [Key]` / `cine_key_hero` (Hero) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:329` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALCineLightRigModel::setHeroEnabled()` (`indra/newview/alpanelcinelightrig.cpp:962`) | none |
| `lights [Key]` / `cine_key_flicker` (Flicker) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:339` | kit `combo` ↔ `CineLightRigKeyFlicker` `S32`; `ALCineRigKitAdapter` preserves/calls behavior `ALPanelCineLightRig::populateStaticCombos()` (`indra/newview/alpanelcinelightrig.cpp:1090`) | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:286` `cine_key_flicker` |
| `lights [Key]` / `cine_key_flicker_amount` (Flicker amt) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:342` | kit `slider` ↔ `CineLightRigKeyFlickerAmount` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:292` `cine_key_flicker_amount` |
| `lights [Key]` / `cine_key_shadow_soft` (Shadow soft) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:345` | kit `number` ↔ `CineLightRigKeyShadowSoft` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:297` `cine_key_shadow_soft` |
| `lights [Key]` / `cine_key_actor_only` (Actor only) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:347` | kit `toggle` ↔ `CineLightRigKeyActorOnly` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:300` `cine_key_actor_only` |
| `lights [Key]` / `cine_key_detail_shadows` (Detail shadows) | none | kit `toggle` ↔ `CineLightRigKeyDetailShadows` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:303` `cine_key_detail_shadows` |
| `lights [Key]` / `cine_key_world_shadow` (World shadow) | none | kit `toggle` ↔ `CineLightRigKeyWorldShadowPick` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:306` `cine_key_world_shadow` |
| `lights [Key]` / `cine_key_projector_feed` (Projector feed) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `lights [Fill]` / `cine_ratio` (Ratio) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:367` | kit `slider` ↔ `CineLightRigRatio` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:329` `cine_ratio` |
| `lights [Fill]` / `cine_ratio_lock` (Ratio lock) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:365` | kit `toggle` ↔ `CineLightRigRatioLock` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:334` `cine_ratio_lock` |
| `lights [Fill]` / `cine_fill_ev` (EV) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:394` | kit `number` ↔ `CineLightRigFillEV` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:337` `cine_fill_ev` |
| `lights [Fill]` / `cine_fill_profile` (Colour) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:389` | kit `combo` ↔ `CineLightRigFillProfile` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:340` `cine_fill_profile` |
| `lights [Fill]` / `cine_fill_yaw` (Yaw) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:375` | kit `dial` ↔ `CineLightRigFillYaw` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:346` `cine_fill_yaw` |
| `lights [Fill]` / `cine_fill_pitch` (Pitch) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:380` | kit `arc` ↔ `CineLightRigFillPitch` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:349` `cine_fill_pitch` |
| `lights [Fill]` / `cine_fill_gel` (Gel) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:407` | kit `combo` ↔ `CineLightRigFillGel` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:352` `cine_fill_gel` |
| `lights [Fill]` / `cine_fill_gobo` (Gobo) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:402` | kit `combo` ↔ `CineLightRigFillGobo` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:358` `cine_fill_gobo` |
| `lights [Fill]` / `cine_fill_shaft` (Shaft) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:399` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALCineLightRigManager::setShaftEnabled()` (`indra/newview/alpanelcinelightrig.cpp:956`) | none |
| `lights [Fill]` / `cine_fill_offset_z` (Height) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:385` | kit `slider` ↔ `CineLightRigFillOffsetZ` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:368` `cine_fill_offset_z` |
| `lights [Fill]` / `cine_fill_cone_width` (Cone) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:397` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onManualConeWidthCommit()` (`indra/newview/alpanelcinelightrig.cpp:2348`) | none |
| `lights [Fill]` / `cine_fill_hero` (Hero) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:400` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALCineLightRigModel::setHeroEnabled()` (`indra/newview/alpanelcinelightrig.cpp:962`) | none |
| `lights [Fill]` / `cine_fill_flicker` (Flicker) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:410` | kit `combo` ↔ `CineLightRigFillFlicker` `S32`; `ALCineRigKitAdapter` preserves/calls behavior `ALPanelCineLightRig::populateStaticCombos()` (`indra/newview/alpanelcinelightrig.cpp:1090`) | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:381` `cine_fill_flicker` |
| `lights [Fill]` / `cine_fill_flicker_amount` (Flicker amt) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:413` | kit `slider` ↔ `CineLightRigFillFlickerAmount` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:387` `cine_fill_flicker_amount` |
| `lights [Fill]` / `cine_fill_shadow_soft` (Shadow soft) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:416` | kit `number` ↔ `CineLightRigFillShadowSoft` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:392` `cine_fill_shadow_soft` |
| `lights [Fill]` / `cine_fill_actor_only` (Actor only) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:418` | kit `toggle` ↔ `CineLightRigFillActorOnly` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:395` `cine_fill_actor_only` |
| `lights [Fill]` / `cine_fill_detail_shadows` (Detail shadows) | none | kit `toggle` ↔ `CineLightRigFillDetailShadows` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:398` `cine_fill_detail_shadows` |
| `lights [Fill]` / `cine_fill_world_shadow` (World shadow) | none | kit `toggle` ↔ `CineLightRigFillWorldShadowPick` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:401` `cine_fill_world_shadow` |
| `lights [Fill]` / `cine_fill_projector_feed` (Projector feed) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `lights [Back]` / `cine_rim_ev` (EV) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:461` | kit `number` ↔ `CineLightRigRimEV` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:424` `cine_rim_ev` |
| `lights [Back]` / `cine_rim_profile` (Colour) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:456` | kit `combo` ↔ `CineLightRigRimProfile` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:427` `cine_rim_profile` |
| `lights [Back]` / `cine_rim_yaw` (Yaw) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:442` | kit `dial` ↔ `CineLightRigRimYaw` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:433` `cine_rim_yaw` |
| `lights [Back]` / `cine_rim_pitch` (Pitch) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:447` | kit `arc` ↔ `CineLightRigRimPitch` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:436` `cine_rim_pitch` |
| `lights [Back]` / `cine_rim_gel` (Gel) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:474` | kit `combo` ↔ `CineLightRigRimGel` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:439` `cine_rim_gel` |
| `lights [Back]` / `cine_rim_gobo` (Gobo) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:469` | kit `combo` ↔ `CineLightRigRimGobo` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:445` `cine_rim_gobo` |
| `lights [Back]` / `cine_rim_shaft` (Shaft) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:466` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALCineLightRigManager::setShaftEnabled()` (`indra/newview/alpanelcinelightrig.cpp:956`) | none |
| `lights [Back]` / `cine_rim_offset_z` (Height) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:452` | kit `slider` ↔ `CineLightRigRimOffsetZ` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:455` `cine_rim_offset_z` |
| `lights [Back]` / `cine_rim_cone_width` (Cone) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:464` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onManualConeWidthCommit()` (`indra/newview/alpanelcinelightrig.cpp:2348`) | none |
| `lights [Back]` / `cine_rim_hero` (Hero) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:467` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALCineLightRigModel::setHeroEnabled()` (`indra/newview/alpanelcinelightrig.cpp:962`) | none |
| `lights [Back]` / `cine_rim_flicker` (Flicker) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:477` | kit `combo` ↔ `CineLightRigRimFlicker` `S32`; `ALCineRigKitAdapter` preserves/calls behavior `ALPanelCineLightRig::populateStaticCombos()` (`indra/newview/alpanelcinelightrig.cpp:1090`) | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:468` `cine_rim_flicker` |
| `lights [Back]` / `cine_rim_flicker_amount` (Flicker amt) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:480` | kit `slider` ↔ `CineLightRigRimFlickerAmount` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:474` `cine_rim_flicker_amount` |
| `lights [Back]` / `cine_rim_shadow_soft` (Shadow soft) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:483` | kit `number` ↔ `CineLightRigRimShadowSoft` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:479` `cine_rim_shadow_soft` |
| `lights [Back]` / `cine_rim_actor_only` (Actor only) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:485` | kit `toggle` ↔ `CineLightRigRimActorOnly` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:482` `cine_rim_actor_only` |
| `lights [Back]` / `cine_rim_detail_shadows` (Detail shadows) | none | kit `toggle` ↔ `CineLightRigRimDetailShadows` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:485` `cine_rim_detail_shadows` |
| `lights [Back]` / `cine_rim_world_shadow` (World shadow) | none | kit `toggle` ↔ `CineLightRigRimWorldShadowPick` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:488` `cine_rim_world_shadow` |
| `lights [Back]` / `cine_rim_projector_feed` (Projector feed) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `lights [BG]` / `cine_bg_ev` (EV) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:528` | kit `number` ↔ `CineLightRigBgEV` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:511` `cine_bg_ev` |
| `lights [BG]` / `cine_bg_profile` (Colour) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:523` | kit `combo` ↔ `CineLightRigBgProfile` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:514` `cine_bg_profile` |
| `lights [BG]` / `cine_bg_yaw` (Yaw) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:509` | kit `dial` ↔ `CineLightRigBgYaw` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:520` `cine_bg_yaw` |
| `lights [BG]` / `cine_bg_pitch` (Pitch) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:514` | kit `arc` ↔ `CineLightRigBgPitch` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:523` `cine_bg_pitch` |
| `lights [BG]` / `cine_bg_gel` (Gel) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:541` | kit `combo` ↔ `CineLightRigBgGel` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:526` `cine_bg_gel` |
| `lights [BG]` / `cine_bg_gobo` (Gobo) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:536` | kit `combo` ↔ `CineLightRigBgGobo` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:532` `cine_bg_gobo` |
| `lights [BG]` / `cine_bg_shaft` (Shaft) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:533` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALCineLightRigManager::setShaftEnabled()` (`indra/newview/alpanelcinelightrig.cpp:956`) | none |
| `lights [BG]` / `cine_bg_offset_z` (Height) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:519` | kit `slider` ↔ `CineLightRigBgOffsetZ` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:542` `cine_bg_offset_z` |
| `lights [BG]` / `cine_bg_cone_width` (Cone) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:531` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onManualConeWidthCommit()` (`indra/newview/alpanelcinelightrig.cpp:2348`) | none |
| `lights [BG]` / `cine_bg_hero` (Hero) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:534` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALCineLightRigModel::setHeroEnabled()` (`indra/newview/alpanelcinelightrig.cpp:962`) | none |
| `lights [BG]` / `cine_bg_flicker` (Flicker) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:544` | kit `combo` ↔ `CineLightRigBgFlicker` `S32`; `ALCineRigKitAdapter` preserves/calls behavior `ALPanelCineLightRig::populateStaticCombos()` (`indra/newview/alpanelcinelightrig.cpp:1090`) | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:555` `cine_bg_flicker` |
| `lights [BG]` / `cine_bg_flicker_amount` (Flicker amt) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:547` | kit `slider` ↔ `CineLightRigBgFlickerAmount` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:561` `cine_bg_flicker_amount` |
| `lights [BG]` / `cine_bg_shadow_soft` (Shadow soft) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:550` | kit `number` ↔ `CineLightRigBgShadowSoft` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:566` `cine_bg_shadow_soft` |
| `lights [BG]` / `cine_bg_actor_only` (Actor only) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:552` | kit `toggle` ↔ `CineLightRigBgActorOnly` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:569` `cine_bg_actor_only` |
| `lights [BG]` / `cine_bg_detail_shadows` (Detail shadows) | none | kit `toggle` ↔ `CineLightRigBgDetailShadows` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:572` `cine_bg_detail_shadows` |
| `lights [BG]` / `cine_bg_world_shadow` (World shadow) | none | kit `toggle` ↔ `CineLightRigBgWorldShadowPick` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:575` `cine_bg_world_shadow` |
| `lights [BG]` / `cine_bg_projector_feed` (Projector feed) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `rig_aim` / `cine_mirror` (Mirror) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:265` | kit `toggle` ↔ `CineLightRigMirror` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:596` `cine_mirror` |
| `rig_aim` / `cine_aim_yaw_minus` (Yaw -) | `button` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:266` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::adjustSelectedLightAim()` (`indra/newview/alpanelcinelightrig.cpp:835`) | none |
| `rig_aim` / `cine_aim_yaw_plus` (Yaw +) | `button` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:267` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::adjustSelectedLightAim()` (`indra/newview/alpanelcinelightrig.cpp:838`) | none |
| `rig_aim` / `cine_aim_pitch_minus` (Pitch -) | `button` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:268` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::adjustSelectedLightAim()` (`indra/newview/alpanelcinelightrig.cpp:841`) | none |
| `rig_aim` / `cine_aim_pitch_plus` (Pitch +) | `button` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:269` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::adjustSelectedLightAim()` (`indra/newview/alpanelcinelightrig.cpp:844`) | none |
| `rig_aim` / `cine_aim_reset` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:270` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::resetAim()` (`indra/newview/alpanelcinelightrig.cpp:847`) | none |
| `rig_aim` / `cine_shadow_mode` (Shadow policy) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:273` | kit `combo` ↔ `CineLightRigShadowMode` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:614` `cine_shadow_mode` |
| `rig_aim` / `cine_detail_bias` (Detail shadow bias) | none | kit `toggle` ↔ `RenderSpotShadowDetailBias` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:623` `cine_detail_bias` |
| `rig_aim` / `cine_auto_shadow_slots` (Auto shadows) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:279` | kit `toggle` ↔ `CineLightRigAutoShadowSlots` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:626` `cine_auto_shadow_slots` |
| `rig_aim` / `cine_shadow_fixit` (Add shadows) | `button` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:281` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onClickShadowFixIt()` (`indra/newview/alpanelcinelightrig.cpp:860`) | none |
| `rig_bounce` / `cine_bounce` (Bounce) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:216` | kit `toggle` ↔ `CineLightRigBounceEnabled` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:637` `cine_bounce` |
| `rig_bounce` / `cine_bounce_ratio` (Ratio) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:218` | kit `number` ↔ `CineLightRigBounceRatio` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:640` `cine_bounce_ratio` |
| `rig_bounce` / `cine_bounce_lights_set` (Bounce set) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:248` | kit `toggle` ↔ `CineLightRigBounceLightsSet` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:643` `cine_bounce_lights_set` |
| `rig_bounce` / `cine_actor_only_all` (Actor only) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:246` | kit `toggle` ↔ `CineLightRigActorOnlyAll` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:646` `cine_actor_only_all` |
| `rig_catchlight` / `cine_catchlight_ev` (EV) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:238` | kit `number` ↔ `CineLightRigCatchlightEV` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:651` `cine_catchlight_ev` |
| `rig_catchlight` / `cine_catchlight_size` (Eye size) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:241` | kit `number` ↔ `CineLightRigCatchlightSize` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:654` `cine_catchlight_size` |
| `rig_catchlight` / `cine_catchlight_angle` (Angle) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:244` | kit `dial` ↔ `CineLightRigCatchlightAngle` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:657` `cine_catchlight_angle` |
| `rig_catchlight` / `cine_catchlight_actor_only` (Actor only) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:250` | kit `toggle` ↔ `CineLightRigCatchlightActorOnly` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:660` `cine_catchlight_actor_only` |
| `fixture` / `cine_fixture_role` (Light) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:570` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onFixtureRoleCommit()` (`indra/newview/alpanelcinelightrig.cpp:878`) | none |
| `fixture` / `cine_fixture_mode` (Use fixture) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:572` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onFixtureModeCommit()` (`indra/newview/alpanelcinelightrig.cpp:880`) | none |
| `fixture` / `cine_fixture_preset` (Preset) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:577` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onFixturePresetCommit()` (`indra/newview/alpanelcinelightrig.cpp:882`) | none |
| `fixture` / `cine_fixture_kelvin` (Kelvin) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:583` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onFixtureValuesCommit()` (`indra/newview/alpanelcinelightrig.cpp:884`) | none |
| `fixture` / `cine_fixture_source_size` (Source size) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:588` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onFixtureValuesCommit()` (`indra/newview/alpanelcinelightrig.cpp:886`) | none |
| `fixture` / `cine_fixture_gel_0` (Gel 1) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:593` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onFixtureValuesCommit()` (`indra/newview/alpanelcinelightrig.cpp:1325`) | none |
| `fixture` / `cine_fixture_gel_1` (Gel 2) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:595` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onFixtureValuesCommit()` (`indra/newview/alpanelcinelightrig.cpp:1325`) | none |
| `fixture` / `cine_fixture_gel_2` (Gel 3) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:597` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onFixtureValuesCommit()` (`indra/newview/alpanelcinelightrig.cpp:1325`) | none |
| `cookie` / `CineGobo01` (Gobo) | none | `ALCineRigKitAdapter` command/model binding → behavior `ALPanelCineLightRig::syncGoboLibrary()` (`indra/newview/alpanelcinelightrig.cpp:1489`) | none |
| `cookie` / `cine_gobo_library` (Library) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:614` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onGoboLibraryCommit()` (`indra/newview/alpanelcinelightrig.cpp:893`) | none |
| `cookie` / `cine_gobo_softness` (Variant) | `text` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:622` | kit readout ← approved static copy; no runtime writer | none |
| `cookie` / `cine_open_cues` (Cue stack...) | `button` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:626` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `LLFloaterReg::showInstance()` (`indra/newview/alpanelcinelightrig.cpp:895`) | none |
| `cookie` / `cine_cookie` (Cookie UUID) | `line_editor` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:233` | kit `text` ↔ `CineLightRigCookieUUID` `String` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:722` `cine_cookie` |
| `flare` / `cine_flare_preset` (Preset) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:640` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onFlarePresetSelected()` (`indra/newview/alpanelcinelightrig.cpp:820`) | none |
| `flare` / `cine_flare_master` (Master) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:644` | kit `slider` ↔ `RenderLensFlareMaster` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:734` `cine_flare_master` |
| `flare` / `cine_flare_rig_strength` (Rig flare) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:647` | kit `slider` ↔ `RenderCineLensFlareStrength` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:739` `cine_flare_rig_strength` |
| `flare` / `cine_flare_rig_intensity` (Rig intensity) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:650` | kit `slider` ↔ `RenderCineLensFlareIntensity` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:744` `cine_flare_rig_intensity` |
| `flare` / `cine_flare_release` (Release) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:692` | kit `slider` ↔ `RenderCineLensFlareRelease` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:749` `cine_flare_release` |
| `flare` / `cine_flare_threshold` (Threshold) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:653` | kit `slider` ↔ `RenderCineLensFlareThreshold` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:754` `cine_flare_threshold` |
| `flare` / `cine_flare_starburst` (Starburst) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:656` | kit `slider` ↔ `RenderLensFlareStarburst` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:759` `cine_flare_starburst` |
| `flare` / `cine_flare_starburst_spikes` (Spikes) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:659` | kit `number` ↔ `RenderLensFlareStarburstSpikes` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:764` `cine_flare_starburst_spikes` |
| `flare` / `cine_flare_starburst_sharpness` (Sharpness) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:661` | kit `number` ↔ `RenderLensFlareStarburstSharpness` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:767` `cine_flare_starburst_sharpness` |
| `flare` / `cine_flare_starburst_length` (Length) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:663` | kit `number` ↔ `RenderLensFlareStarburstLength` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:770` `cine_flare_starburst_length` |
| `flare` / `cine_flare_iris` (Iris) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:665` | kit `slider` ↔ `RenderLensFlareIris` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:773` `cine_flare_iris` |
| `flare` / `cine_flare_ring` (Ring) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:668` | kit `slider` ↔ `RenderLensFlareRing` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:778` `cine_flare_ring` |
| `flare` / `cine_flare_circle` (Circle) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:671` | kit `slider` ↔ `RenderLensFlareCircle` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:783` `cine_flare_circle` |
| `flare` / `cine_flare_arc` (Arc) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:674` | kit `slider` ↔ `RenderLensFlareArc` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:788` `cine_flare_arc` |
| `flare` / `cine_flare_warp` (Warp) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:683` | kit `slider` ↔ `RenderLensFlareWarp` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:793` `cine_flare_warp` |
| `flare` / `cine_flare_ghost_chroma` (Ghost chroma) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:677` | kit `slider` ↔ `RenderLensFlareGhostChroma` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:798` `cine_flare_ghost_chroma` |
| `flare` / `cine_flare_halo_chroma` (Halo chroma) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:680` | kit `slider` ↔ `RenderLensFlareHaloChroma` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:803` `cine_flare_halo_chroma` |
| `flare` / `cine_flare_streak_tip` (Streak tip) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:686` | kit `slider` ↔ `RenderLensFlareStreakTipAmount` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:808` `cine_flare_streak_tip` |
| `flare` / `cine_flare_src_color` (Source colour) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:689` | kit `slider` ↔ `RenderLensFlareSrcColorAmount` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:813` `cine_flare_src_color` |
| `live_probe` / `cine_live_probe_target` (Target) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:730` | kit `combo` ↔ `CineLightRigLiveProbeTarget` `S32` | `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:167` `cine_easy_probe_target`, `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:103` `cine_easy_probe_target`, `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:820` `cine_live_probe_target` |
| `live_probe` / `cine_live_probe_radius` (Radius) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:742` | kit `number` ↔ `CineLightRigLiveProbeRadius` `F32` | `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:163` `cine_easy_probe_radius`, `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:88` `cine_easy_probe_radius`, `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:831` `cine_live_probe_radius` |
| `live_probe` / `cine_live_probe_ambiance` (Ambiance) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:762` | kit `number` ↔ `CineLightRigLiveProbeAmbiance` `F32` | `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:175` `cine_easy_probe_ambiance`, `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:93` `cine_easy_probe_ambiance`, `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:834` `cine_live_probe_ambiance` |
| `live_probe` / `cine_live_probe_bounce_keep` (Keep bounce) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:721` | kit `number` ↔ `CineLightRigLiveProbeBounceKeep` `F32` | `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:178` `cine_easy_probe_bounce_keep`, `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:98` `cine_easy_probe_bounce_keep`, `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:837` `cine_live_probe_bounce_keep` |
| `live_probe` / `cine_live_probe_refresh` (Refresh) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:771` | kit `combo` ↔ `CineLightRigLiveProbeRefresh` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:840` `cine_live_probe_refresh` |
| `live_probe` / `cine_live_probe_refresh_now` (Refresh now) | `button` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:780` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::requestCinematicLiveProbeRefresh()` (`indra/newview/alpanelcinelightrig.cpp:852`), wired in `LLReflectionMapManager::postBuild()`; behavior `LLReflectionMapManager::requestCinematicLiveProbeRefresh()` (`indra/newview/alpanelcinelightrig.cpp:899`) | none |
| `live_probe` / `cine_live_probe_live_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:807` | kit readout ← approved static copy; no runtime writer | none |
| `live_probe` / `cine_live_probe_gizmo` (Gizmo) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:715` | kit `toggle` ↔ `CineLightRigLiveProbeGizmo` `Boolean` | `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:166` `cine_easy_probe_gizmo`, `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:114` `cine_easy_probe_gizmo`, `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:860` `cine_live_probe_gizmo` |
| `live_probe` / `cine_live_probe_offset` (Height) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:752` | kit `number` ↔ `CineLightRigLiveProbeOffsetZ` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:863` `cine_live_probe_offset` |
| `live_probe` / `cine_live_probe_manual_faces` (Faces) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:787` | kit `number` ↔ `CineLightRigLiveProbeManualFaces` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:866` `cine_live_probe_manual_faces` |
| `live_probe` / `cine_live_probe_manual_frames` (Every) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:798` | kit `number` ↔ `CineLightRigLiveProbeManualFrames` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:869` `cine_live_probe_manual_frames` |
| `live_probe` / `cine_live_probe_manual_frames_unit` (frames) | `text` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:805` | kit readout ← approved static copy; no runtime writer | none |
| `night_mask` / `cine_night_mask_preset` (Preset) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:868` | `ALCineRigKitAdapter` command/model binding → wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onNightMaskPresetSelected()` (`indra/newview/alpanelcinelightrig.cpp:824`) | none |
| `night_mask` / `cine_night_mask_darkness` (Darkness) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:894` | kit `slider` ↔ `CineLightRigNightMaskDarkness` `F32`; `ALCineRigKitAdapter` preserves/calls wired in `ALPanelCineLightRig::postBuild()`; behavior `ALPanelCineLightRig::onNightMaskDarknessCommit()` (`indra/newview/alpanelcinelightrig.cpp:958`) | none |
| `night_mask` / `cine_night_mask_distance` (Distance) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:874` | kit `slider` ↔ `CineLightRigNightMaskDistance` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:888` `cine_night_mask_distance` |
| `night_mask` / `cine_night_mask_feather` (Feather) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:884` | kit `slider` ↔ `CineLightRigNightMaskFeather` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:893` `cine_night_mask_feather` |
| `night_mask` / `cine_night_mask_shape` (Shape) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:849` | kit `combo` ↔ `CineLightRigNightMaskShape` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:898` `cine_night_mask_shape` |
| `night_mask` / `cine_night_mask_target` (Target) | `combo_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:838` | kit `combo` ↔ `CineLightRigNightMaskTarget` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:907` `cine_night_mask_target` |
| `night_mask` / `cine_night_mask_gizmo` (Gizmo) | `check_box` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:833` | kit `toggle` ↔ `CineLightRigNightMaskGizmo` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:921` `cine_night_mask_gizmo` |
| `night_mask` / `cine_night_mask_height` (Height) | `spinner` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:859` | kit `slider` ↔ `CineLightRigNightMaskHeightOffset` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:924` `cine_night_mask_height` |
| `night_mask` / `cine_night_mask_tint_strength` (Tint strength) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:906` | kit `slider` ↔ `CineLightRigNightMaskTintStrength` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:929` `cine_night_mask_tint_strength` |
| `night_mask` / `cine_night_mask_tint_color` (Tint colour) | `color_swatch` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:915` | kit `color` ↔ `CineLightRigNightMaskTintColor` `Color3` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:934` `cine_night_mask_tint_color` |
| `night_mask` / `cine_night_mask_desaturation` (Desaturation) | `slider` `indra/newview/skins/default/xui/en/panel_cine_light_rig.xml:925` | kit `slider` ↔ `CineLightRigNightMaskDesaturation` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_light_cine_light_rig.xml:937` `cine_night_mask_desaturation` |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALCineLightRigManager::setShaftEnabled()` — `indra/newview/alpanelcinelightrig.cpp:956` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALCineLightRigManager::stopFX()` — `indra/newview/alpanelcinelightrig.cpp:832` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALCineLightRigModel::setHeroEnabled()` — `indra/newview/alpanelcinelightrig.cpp:962` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::adjustSelectedLightAim()` — `indra/newview/alpanelcinelightrig.cpp:835, 838, 841, 844` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::commitSeed()` — `indra/newview/alpanelcinelightrig.cpp:856` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::onAnchorSelected()` — `indra/newview/alpanelcinelightrig.cpp:805` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::onClearObjectTarget()` — `indra/newview/alpanelcinelightrig.cpp:809` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::onClickShadowFixIt()` — `indra/newview/alpanelcinelightrig.cpp:860` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::onEasyBgCommit()` — `indra/newview/alpanelcinelightrig.cpp:870` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::onEasyBrightnessCommit()` — `indra/newview/alpanelcinelightrig.cpp:864` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::onEasyConeFeatherCommit()` — `indra/newview/alpanelcinelightrig.cpp:876` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::onEasyConeWidthCommit()` — `indra/newview/alpanelcinelightrig.cpp:874` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::onEasyDramaCommit()` — `indra/newview/alpanelcinelightrig.cpp:866` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::onEasyRimCommit()` — `indra/newview/alpanelcinelightrig.cpp:868` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::onEasyWarmthCommit()` — `indra/newview/alpanelcinelightrig.cpp:872` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::onFixtureModeCommit()` — `indra/newview/alpanelcinelightrig.cpp:880` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::onFixturePresetCommit()` — `indra/newview/alpanelcinelightrig.cpp:882` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::onFixtureRoleCommit()` — `indra/newview/alpanelcinelightrig.cpp:878` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::onFixtureValuesCommit()` — `indra/newview/alpanelcinelightrig.cpp:884, 886, 1325` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::onFlarePresetSelected()` — `indra/newview/alpanelcinelightrig.cpp:820, 822` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::onGoboLibraryCommit()` — `indra/newview/alpanelcinelightrig.cpp:893` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::onGroupSlotsCommit()` — `indra/newview/alpanelcinelightrig.cpp:1601` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::onManualCommit()` — `indra/newview/alpanelcinelightrig.cpp:897` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::onManualConeWidthCommit()` — `indra/newview/alpanelcinelightrig.cpp:2348` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::onNightMaskDarknessCommit()` — `indra/newview/alpanelcinelightrig.cpp:958, 960` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::onNightMaskPresetSelected()` — `indra/newview/alpanelcinelightrig.cpp:824, 826` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::onTargetObject()` — `indra/newview/alpanelcinelightrig.cpp:807` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::populateStaticCombos()` — `indra/newview/alpanelcinelightrig.cpp:1090` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::randomizeSeed()` — `indra/newview/alpanelcinelightrig.cpp:858` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::requestCinematicLiveProbeRefresh()` — `indra/newview/alpanelcinelightrig.cpp:852` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::resetAim()` — `indra/newview/alpanelcinelightrig.cpp:847` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::resetAll()` — `indra/newview/alpanelcinelightrig.cpp:849` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineLightRig::syncGoboLibrary()` — `indra/newview/alpanelcinelightrig.cpp:1489` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterReg::showInstance()` — `indra/newview/alpanelcinelightrig.cpp:895` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLReflectionMapManager::requestCinematicLiveProbeRefresh()` — `indra/newview/alpanelcinelightrig.cpp:899` | Move model-facing work into `ALCineRigKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **271 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-easy-tallest-850` | Only `easy` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-easy-tallest-1160` | Only `easy` is put in its tallest local state at 1160 px; all other cards stay default. |
| `cine_easy_rim-option-0` | Only `cine_easy_rim` selects `Off`; other selectors remain default. |
| `cine_easy_rim-option-1` | Only `cine_easy_rim` selects `Faint`; other selectors remain default. |
| `cine_easy_rim-option-2` | Only `cine_easy_rim` selects `Subtle`; other selectors remain default. |
| `cine_easy_rim-option-3` | Only `cine_easy_rim` selects `Strong`; other selectors remain default. |
| `cine_easy_bg-option-0` | Only `cine_easy_bg` selects `Off`; other selectors remain default. |
| `cine_easy_bg-option-1` | Only `cine_easy_bg` selects `Faint`; other selectors remain default. |
| `cine_easy_bg-option-2` | Only `cine_easy_bg` selects `Subtle`; other selectors remain default. |
| `cine_easy_bg-option-3` | Only `cine_easy_bg` selects `Strong`; other selectors remain default. |
| `cine_easy_cone_width-option-0` | Only `cine_easy_cone_width` selects `Snoot`; other selectors remain default. |
| `cine_easy_cone_width-option-1` | Only `cine_easy_cone_width` selects `Standard`; other selectors remain default. |
| `cine_easy_cone_width-option-2` | Only `cine_easy_cone_width` selects `Softbox`; other selectors remain default. |
| `cine_easy_flare_preset-option-0` | Only `cine_easy_flare_preset` selects `Anamorphic Blue`; other selectors remain default. |
| `cine_easy_flare_preset-option-1` | Only `cine_easy_flare_preset` selects `Anamorphic Gold`; other selectors remain default. |
| `cine_easy_flare_preset-option-2` | Only `cine_easy_flare_preset` selects `Spherical Prime`; other selectors remain default. |
| `cine_easy_flare_preset-option-3` | Only `cine_easy_flare_preset` selects `Vintage Uncoated`; other selectors remain default. |
| `cine_easy_flare_preset-option-4` | Only `cine_easy_flare_preset` selects `Master Anamorphic`; other selectors remain default. |
| `cine_easy_flare_preset-option-5` | Only `cine_easy_flare_preset` selects `Sci-Fi / Neon`; other selectors remain default. |
| `cine_easy_flare_preset-option-6` | Only `cine_easy_flare_preset` selects `Minimal Glint`; other selectors remain default. |
| `cine_easy_flare_preset-option-7` | Only `cine_easy_flare_preset` selects `Rainbow Prism`; other selectors remain default. |
| `cine_easy_flare_preset-option-8` | Only `cine_easy_flare_preset` selects `Dreamy Halo`; other selectors remain default. |
| `cine_easy_flare_preset-option-9` | Only `cine_easy_flare_preset` selects `JJ Blue Blast`; other selectors remain default. |
| `cine_easy_flare_preset-option-10` | Only `cine_easy_flare_preset` selects `Retro 70s Warm`; other selectors remain default. |
| `cine_easy_flare_preset-option-11` | Only `cine_easy_flare_preset` selects `Cyberpunk Neon`; other selectors remain default. |
| `cine_easy_flare_preset-option-12` | Only `cine_easy_flare_preset` selects `Ethereal Angelic`; other selectors remain default. |
| `cine_easy_flare_preset-option-13` | Only `cine_easy_flare_preset` selects `Golden Hour`; other selectors remain default. |
| `cine_easy_flare_preset-option-14` | Only `cine_easy_flare_preset` selects `Noir Practical`; other selectors remain default. |
| `cine_easy_flare_preset-option-15` | Only `cine_easy_flare_preset` selects `Documentary Real`; other selectors remain default. |
| `cine_easy_flare_preset-option-16` | Only `cine_easy_flare_preset` selects `Blockbuster T-O`; other selectors remain default. |
| `cine_easy_flare_preset-option-17` | Only `cine_easy_flare_preset` selects `Sodium Night`; other selectors remain default. |
| `cine_easy_flare_preset-option-18` | Only `cine_easy_flare_preset` selects `Music Video Glam`; other selectors remain default. |
| `cine_easy_night_mask_enable-on` | Only `cine_easy_night_mask_enable` is on; dependent visibility is measured. |
| `cine_easy_night_mask_preset-option-0` | Only `cine_easy_night_mask_preset` selects `Campfire Bubble`; other selectors remain default. |
| `cine_easy_night_mask_preset-option-1` | Only `cine_easy_night_mask_preset` selects `Moonlit Iris`; other selectors remain default. |
| `cine_easy_night_mask_preset-option-2` | Only `cine_easy_night_mask_preset` selects `Stage Box`; other selectors remain default. |
| `cine_easy_night_mask_preset-option-3` | Only `cine_easy_night_mask_preset` selects `Background Drop`; other selectors remain default. |
| `cine_easy_night_mask_preset-option-4` | Only `cine_easy_night_mask_preset` selects `Deep Night Wide`; other selectors remain default. |
| `cine_easy_night_mask_preset-option-5` | Only `cine_easy_night_mask_preset` selects `Pitch Black Cell`; other selectors remain default. |
| `card-easy_probe-tallest-850` | Only `easy_probe` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-easy_probe-tallest-1160` | Only `easy_probe` is put in its tallest local state at 1160 px; all other cards stay default. |
| `cine_easy_probe_target-option-0` | Only `cine_easy_probe_target` selects `Self`; other selectors remain default. |
| `cine_easy_probe_target-option-1` | Only `cine_easy_probe_target` selects `Subject A`; other selectors remain default. |
| `cine_easy_probe_target-option-2` | Only `cine_easy_probe_target` selects `Subject B`; other selectors remain default. |
| `cine_easy_probe_target-option-3` | Only `cine_easy_probe_target` selects `Subject C`; other selectors remain default. |
| `cine_easy_probe_target-option-4` | Only `cine_easy_probe_target` selects `Subject D`; other selectors remain default. |
| `cine_easy_probe_gizmo-on` | Only `cine_easy_probe_gizmo` is on; dependent visibility is measured. |
| `card-rig_status-tallest-850` | Only `rig_status` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-rig_status-tallest-1160` | Only `rig_status` is put in its tallest local state at 1160 px; all other cards stay default. |
| `cine_gizmo-on` | Only `cine_gizmo` is on; dependent visibility is measured. |
| `cine_anchor-option-0` | Only `cine_anchor` selects `Selected cast`; other selectors remain default. |
| `cine_anchor-option-1` | Only `cine_anchor` selects `Mara`; other selectors remain default. |
| `cine_anchor-option-2` | Only `cine_anchor` selects `Jun`; other selectors remain default. |
| `cine_anchor-option-3` | Only `cine_anchor` selects `Me`; other selectors remain default. |
| `card-light_group-tallest-850` | Only `light_group` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-light_group-tallest-1160` | Only `light_group` is put in its tallest local state at 1160 px; all other cards stay default. |
| `cine_group_self-on` | Only `cine_group_self` is on; dependent visibility is measured. |
| `cine_group_a-on` | Only `cine_group_a` is on; dependent visibility is measured. |
| `cine_group_b-on` | Only `cine_group_b` is on; dependent visibility is measured. |
| `cine_group_c-on` | Only `cine_group_c` is on; dependent visibility is measured. |
| `cine_group_d-on` | Only `cine_group_d` is on; dependent visibility is measured. |
| `card-rig_main-tallest-850` | Only `rig_main` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-rig_main-tallest-1160` | Only `rig_main` is put in its tallest local state at 1160 px; all other cards stay default. |
| `cine_track_mode-option-0` | Only `cine_track_mode` selects `Skeleton`; other selectors remain default. |
| `cine_track_mode-option-1` | Only `cine_track_mode` selects `Body-stable`; other selectors remain default. |
| `card-rig_setups-tallest-850` | Only `rig_setups` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-rig_setups-tallest-1160` | Only `rig_setups` is put in its tallest local state at 1160 px; all other cards stay default. |
| `cine_random_cycle-on` | Only `cine_random_cycle` is on; dependent visibility is measured. |
| `cine_easing-option-0` | Only `cine_easing` selects `Linear`; other selectors remain default. |
| `cine_easing-option-1` | Only `cine_easing` selects `Smoothstep`; other selectors remain default. |
| `cine_easing-option-2` | Only `cine_easing` selects `Ease-in-out`; other selectors remain default. |
| `cine_easing-option-3` | Only `cine_easing` selects `Ease-out`; other selectors remain default. |
| `card-rig_effects-tallest-850` | Only `rig_effects` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-rig_effects-tallest-1160` | Only `rig_effects` is put in its tallest local state at 1160 px; all other cards stay default. |
| `cine_manual-on` | Only `cine_manual` is on; dependent visibility is measured. |
| `card-lights-tallest-850` | Only `lights` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-lights-tallest-1160` | Only `lights` is put in its tallest local state at 1160 px; all other cards stay default. |
| `cine_key_actor_only-on` | Only `cine_key_actor_only` is on; dependent visibility is measured. |
| `cine_key_detail_shadows-on` | Only `cine_key_detail_shadows` is on; dependent visibility is measured. |
| `cine_key_world_shadow-on` | Only `cine_key_world_shadow` is on; dependent visibility is measured. |
| `cine_key_projector_feed-option-0` | Only `cine_key_projector_feed` selects `None`; other selectors remain default. |
| `cine_key_projector_feed-option-1` | Only `cine_key_projector_feed` selects `Cam 1`; other selectors remain default. |
| `cine_ratio_lock-on` | Only `cine_ratio_lock` is on; dependent visibility is measured. |
| `cine_fill_actor_only-on` | Only `cine_fill_actor_only` is on; dependent visibility is measured. |
| `cine_fill_detail_shadows-on` | Only `cine_fill_detail_shadows` is on; dependent visibility is measured. |
| `cine_fill_world_shadow-on` | Only `cine_fill_world_shadow` is on; dependent visibility is measured. |
| `cine_fill_projector_feed-option-0` | Only `cine_fill_projector_feed` selects `None`; other selectors remain default. |
| `cine_fill_projector_feed-option-1` | Only `cine_fill_projector_feed` selects `Cam 1`; other selectors remain default. |
| `cine_rim_actor_only-on` | Only `cine_rim_actor_only` is on; dependent visibility is measured. |
| `cine_rim_detail_shadows-on` | Only `cine_rim_detail_shadows` is on; dependent visibility is measured. |
| `cine_rim_world_shadow-on` | Only `cine_rim_world_shadow` is on; dependent visibility is measured. |
| `cine_rim_projector_feed-option-0` | Only `cine_rim_projector_feed` selects `None`; other selectors remain default. |
| `cine_rim_projector_feed-option-1` | Only `cine_rim_projector_feed` selects `Cam 1`; other selectors remain default. |
| `cine_bg_actor_only-on` | Only `cine_bg_actor_only` is on; dependent visibility is measured. |
| `cine_bg_detail_shadows-on` | Only `cine_bg_detail_shadows` is on; dependent visibility is measured. |
| `cine_bg_world_shadow-on` | Only `cine_bg_world_shadow` is on; dependent visibility is measured. |
| `cine_bg_projector_feed-option-0` | Only `cine_bg_projector_feed` selects `None`; other selectors remain default. |
| `cine_bg_projector_feed-option-1` | Only `cine_bg_projector_feed` selects `Cam 1`; other selectors remain default. |
| `card-rig_aim-tallest-850` | Only `rig_aim` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-rig_aim-tallest-1160` | Only `rig_aim` is put in its tallest local state at 1160 px; all other cards stay default. |
| `cine_mirror-on` | Only `cine_mirror` is on; dependent visibility is measured. |
| `cine_shadow_mode-option-0` | Only `cine_shadow_mode` selects `No rig shadows`; other selectors remain default. |
| `cine_shadow_mode-option-1` | Only `cine_shadow_mode` selects `KEY only`; other selectors remain default. |
| `cine_shadow_mode-option-2` | Only `cine_shadow_mode` selects `All projectors compete`; other selectors remain default. |
| `cine_detail_bias-on` | Only `cine_detail_bias` is on; dependent visibility is measured. |
| `cine_auto_shadow_slots-on` | Only `cine_auto_shadow_slots` is on; dependent visibility is measured. |
| `card-rig_bounce-tallest-850` | Only `rig_bounce` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-rig_bounce-tallest-1160` | Only `rig_bounce` is put in its tallest local state at 1160 px; all other cards stay default. |
| `cine_bounce-on` | Only `cine_bounce` is on; dependent visibility is measured. |
| `cine_bounce_lights_set-on` | Only `cine_bounce_lights_set` is on; dependent visibility is measured. |
| `cine_actor_only_all-on` | Only `cine_actor_only_all` is on; dependent visibility is measured. |
| `card-rig_catchlight-tallest-850` | Only `rig_catchlight` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-rig_catchlight-tallest-1160` | Only `rig_catchlight` is put in its tallest local state at 1160 px; all other cards stay default. |
| `cine_catchlight_actor_only-on` | Only `cine_catchlight_actor_only` is on; dependent visibility is measured. |
| `card-fixture-tallest-850` | Only `fixture` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-fixture-tallest-1160` | Only `fixture` is put in its tallest local state at 1160 px; all other cards stay default. |
| `cine_fixture_role-option-0` | Only `cine_fixture_role` selects `Key`; other selectors remain default. |
| `cine_fixture_role-option-1` | Only `cine_fixture_role` selects `Fill`; other selectors remain default. |
| `cine_fixture_role-option-2` | Only `cine_fixture_role` selects `Rim`; other selectors remain default. |
| `cine_fixture_role-option-3` | Only `cine_fixture_role` selects `Bg`; other selectors remain default. |
| `cine_fixture_mode-on` | Only `cine_fixture_mode` is on; dependent visibility is measured. |
| `cine_fixture_preset-option-0` | Only `cine_fixture_preset` selects `Tungsten Fresnel (650W)`; other selectors remain default. |
| `cine_fixture_preset-option-1` | Only `cine_fixture_preset` selects `Big Fresnel (5K)`; other selectors remain default. |
| `cine_fixture_preset-option-2` | Only `cine_fixture_preset` selects `Open-face (Redhead)`; other selectors remain default. |
| `cine_fixture_preset-option-3` | Only `cine_fixture_preset` selects `HMI (M18)`; other selectors remain default. |
| `cine_fixture_preset-option-4` | Only `cine_fixture_preset` selects `LED bi-color panel`; other selectors remain default. |
| `cine_fixture_preset-option-5` | Only `cine_fixture_preset` selects `Kino / fluoro bank`; other selectors remain default. |
| `cine_fixture_preset-option-6` | Only `cine_fixture_preset` selects `2 ft China ball`; other selectors remain default. |
| `cine_fixture_preset-option-7` | Only `cine_fixture_preset` selects `3 ft Softbox`; other selectors remain default. |
| `cine_fixture_preset-option-8` | Only `cine_fixture_preset` selects `5 ft Octabox`; other selectors remain default. |
| `cine_fixture_preset-option-9` | Only `cine_fixture_preset` selects `Book light`; other selectors remain default. |
| `cine_fixture_preset-option-10` | Only `cine_fixture_preset` selects `Practical bulb`; other selectors remain default. |
| `cine_fixture_preset-option-11` | Only `cine_fixture_preset` selects `Sodium vapor street`; other selectors remain default. |
| `cine_fixture_preset-option-12` | Only `cine_fixture_preset` selects `Moonlight (cheated)`; other selectors remain default. |
| `cine_fixture_gel_0-option-0` | Only `cine_fixture_gel_0` selects `None`; other selectors remain default. |
| `cine_fixture_gel_0-option-1` | Only `cine_fixture_gel_0` selects `Full CTO (L204)`; other selectors remain default. |
| `cine_fixture_gel_0-option-2` | Only `cine_fixture_gel_0` selects `1/2 CTO (L205)`; other selectors remain default. |
| `cine_fixture_gel_0-option-3` | Only `cine_fixture_gel_0` selects `1/4 CTO (L206)`; other selectors remain default. |
| `cine_fixture_gel_0-option-4` | Only `cine_fixture_gel_0` selects `1/8 CTO (L223)`; other selectors remain default. |
| `cine_fixture_gel_0-option-5` | Only `cine_fixture_gel_0` selects `Full CTB (L201)`; other selectors remain default. |
| `cine_fixture_gel_0-option-6` | Only `cine_fixture_gel_0` selects `1/2 CTB (L202)`; other selectors remain default. |
| `cine_fixture_gel_0-option-7` | Only `cine_fixture_gel_0` selects `1/4 CTB (L203)`; other selectors remain default. |
| `cine_fixture_gel_0-option-8` | Only `cine_fixture_gel_0` selects `1/8 CTB (L218)`; other selectors remain default. |
| `cine_fixture_gel_0-option-9` | Only `cine_fixture_gel_0` selects `Plus Green (L244)`; other selectors remain default. |
| `cine_fixture_gel_0-option-10` | Only `cine_fixture_gel_0` selects `1/2 Plus Green (L245)`; other selectors remain default. |
| `cine_fixture_gel_0-option-11` | Only `cine_fixture_gel_0` selects `Minus Green (L247)`; other selectors remain default. |
| `cine_fixture_gel_0-option-12` | Only `cine_fixture_gel_0` selects `1/2 Minus Green (L248)`; other selectors remain default. |
| `cine_fixture_gel_0-option-13` | Only `cine_fixture_gel_0` selects `Bastard Amber (R02)`; other selectors remain default. |
| `cine_fixture_gel_0-option-14` | Only `cine_fixture_gel_0` selects `Steel Blue (R64)`; other selectors remain default. |
| `cine_fixture_gel_0-option-15` | Only `cine_fixture_gel_0` selects `Congo Blue (L181)`; other selectors remain default. |
| `cine_fixture_gel_0-option-16` | Only `cine_fixture_gel_0` selects `Primary Red`; other selectors remain default. |
| `cine_fixture_gel_0-option-17` | Only `cine_fixture_gel_0` selects `Primary Green`; other selectors remain default. |
| `cine_fixture_gel_0-option-18` | Only `cine_fixture_gel_0` selects `Primary Blue`; other selectors remain default. |
| `cine_fixture_gel_1-option-0` | Only `cine_fixture_gel_1` selects `None`; other selectors remain default. |
| `cine_fixture_gel_1-option-1` | Only `cine_fixture_gel_1` selects `Full CTO (L204)`; other selectors remain default. |
| `cine_fixture_gel_1-option-2` | Only `cine_fixture_gel_1` selects `1/2 CTO (L205)`; other selectors remain default. |
| `cine_fixture_gel_1-option-3` | Only `cine_fixture_gel_1` selects `1/4 CTO (L206)`; other selectors remain default. |
| `cine_fixture_gel_1-option-4` | Only `cine_fixture_gel_1` selects `1/8 CTO (L223)`; other selectors remain default. |
| `cine_fixture_gel_1-option-5` | Only `cine_fixture_gel_1` selects `Full CTB (L201)`; other selectors remain default. |
| `cine_fixture_gel_1-option-6` | Only `cine_fixture_gel_1` selects `1/2 CTB (L202)`; other selectors remain default. |
| `cine_fixture_gel_1-option-7` | Only `cine_fixture_gel_1` selects `1/4 CTB (L203)`; other selectors remain default. |
| `cine_fixture_gel_1-option-8` | Only `cine_fixture_gel_1` selects `1/8 CTB (L218)`; other selectors remain default. |
| `cine_fixture_gel_1-option-9` | Only `cine_fixture_gel_1` selects `Plus Green (L244)`; other selectors remain default. |
| `cine_fixture_gel_1-option-10` | Only `cine_fixture_gel_1` selects `1/2 Plus Green (L245)`; other selectors remain default. |
| `cine_fixture_gel_1-option-11` | Only `cine_fixture_gel_1` selects `Minus Green (L247)`; other selectors remain default. |
| `cine_fixture_gel_1-option-12` | Only `cine_fixture_gel_1` selects `1/2 Minus Green (L248)`; other selectors remain default. |
| `cine_fixture_gel_1-option-13` | Only `cine_fixture_gel_1` selects `Bastard Amber (R02)`; other selectors remain default. |
| `cine_fixture_gel_1-option-14` | Only `cine_fixture_gel_1` selects `Steel Blue (R64)`; other selectors remain default. |
| `cine_fixture_gel_1-option-15` | Only `cine_fixture_gel_1` selects `Congo Blue (L181)`; other selectors remain default. |
| `cine_fixture_gel_1-option-16` | Only `cine_fixture_gel_1` selects `Primary Red`; other selectors remain default. |
| `cine_fixture_gel_1-option-17` | Only `cine_fixture_gel_1` selects `Primary Green`; other selectors remain default. |
| `cine_fixture_gel_1-option-18` | Only `cine_fixture_gel_1` selects `Primary Blue`; other selectors remain default. |
| `cine_fixture_gel_2-option-0` | Only `cine_fixture_gel_2` selects `None`; other selectors remain default. |
| `cine_fixture_gel_2-option-1` | Only `cine_fixture_gel_2` selects `Full CTO (L204)`; other selectors remain default. |
| `cine_fixture_gel_2-option-2` | Only `cine_fixture_gel_2` selects `1/2 CTO (L205)`; other selectors remain default. |
| `cine_fixture_gel_2-option-3` | Only `cine_fixture_gel_2` selects `1/4 CTO (L206)`; other selectors remain default. |
| `cine_fixture_gel_2-option-4` | Only `cine_fixture_gel_2` selects `1/8 CTO (L223)`; other selectors remain default. |
| `cine_fixture_gel_2-option-5` | Only `cine_fixture_gel_2` selects `Full CTB (L201)`; other selectors remain default. |
| `cine_fixture_gel_2-option-6` | Only `cine_fixture_gel_2` selects `1/2 CTB (L202)`; other selectors remain default. |
| `cine_fixture_gel_2-option-7` | Only `cine_fixture_gel_2` selects `1/4 CTB (L203)`; other selectors remain default. |
| `cine_fixture_gel_2-option-8` | Only `cine_fixture_gel_2` selects `1/8 CTB (L218)`; other selectors remain default. |
| `cine_fixture_gel_2-option-9` | Only `cine_fixture_gel_2` selects `Plus Green (L244)`; other selectors remain default. |
| `cine_fixture_gel_2-option-10` | Only `cine_fixture_gel_2` selects `1/2 Plus Green (L245)`; other selectors remain default. |
| `cine_fixture_gel_2-option-11` | Only `cine_fixture_gel_2` selects `Minus Green (L247)`; other selectors remain default. |
| `cine_fixture_gel_2-option-12` | Only `cine_fixture_gel_2` selects `1/2 Minus Green (L248)`; other selectors remain default. |
| `cine_fixture_gel_2-option-13` | Only `cine_fixture_gel_2` selects `Bastard Amber (R02)`; other selectors remain default. |
| `cine_fixture_gel_2-option-14` | Only `cine_fixture_gel_2` selects `Steel Blue (R64)`; other selectors remain default. |
| `cine_fixture_gel_2-option-15` | Only `cine_fixture_gel_2` selects `Congo Blue (L181)`; other selectors remain default. |
| `cine_fixture_gel_2-option-16` | Only `cine_fixture_gel_2` selects `Primary Red`; other selectors remain default. |
| `cine_fixture_gel_2-option-17` | Only `cine_fixture_gel_2` selects `Primary Green`; other selectors remain default. |
| `cine_fixture_gel_2-option-18` | Only `cine_fixture_gel_2` selects `Primary Blue`; other selectors remain default. |
| `card-cookie-tallest-850` | Only `cookie` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-cookie-tallest-1160` | Only `cookie` is put in its tallest local state at 1160 px; all other cards stay default. |
| `cine_gobo_library-option-0` | Only `cine_gobo_library` selects `Default`; other selectors remain default. |
| `cine_gobo_library-option-1` | Only `cine_gobo_library` selects `Venetian Blinds`; other selectors remain default. |
| `cine_gobo_library-option-2` | Only `cine_gobo_library` selects `Window Panes`; other selectors remain default. |
| `cine_gobo_library-option-3` | Only `cine_gobo_library` selects `Prison Bars`; other selectors remain default. |
| `cine_gobo_library-option-4` | Only `cine_gobo_library` selects `Slats`; other selectors remain default. |
| `cine_gobo_library-option-5` | Only `cine_gobo_library` selects `Grid`; other selectors remain default. |
| `cine_gobo_library-option-6` | Only `cine_gobo_library` selects `Soft Dapple`; other selectors remain default. |
| `cine_gobo_library-option-7` | Only `cine_gobo_library` selects `Branches`; other selectors remain default. |
| `cine_gobo_library-option-8` | Only `cine_gobo_library` selects `Arched Window`; other selectors remain default. |
| `cine_gobo_library-option-9` | Only `cine_gobo_library` selects `French Door`; other selectors remain default. |
| `cine_gobo_library-option-10` | Only `cine_gobo_library` selects `Curtain Edge`; other selectors remain default. |
| `cine_gobo_library-option-11` | Only `cine_gobo_library` selects `Stairwell Rail`; other selectors remain default. |
| `cine_gobo_library-option-12` | Only `cine_gobo_library` selects `Door Crack`; other selectors remain default. |
| `cine_gobo_library-option-13` | Only `cine_gobo_library` selects `Dense Foliage`; other selectors remain default. |
| `cine_gobo_library-option-14` | Only `cine_gobo_library` selects `Palm Dapple`; other selectors remain default. |
| `cine_gobo_library-option-15` | Only `cine_gobo_library` selects `Water Caustics`; other selectors remain default. |
| `cine_gobo_library-option-16` | Only `cine_gobo_library` selects `Classic Cucoloris`; other selectors remain default. |
| `cine_gobo_library-option-17` | Only `cine_gobo_library` selects `Fine Celo`; other selectors remain default. |
| `cine_gobo_library-option-18` | Only `cine_gobo_library` selects `Scrim Wave`; other selectors remain default. |
| `cine_gobo_library-option-19` | Only `cine_gobo_library` selects `Smoke Drift`; other selectors remain default. |
| `cine_gobo_library-option-20` | Only `cine_gobo_library` selects `Chain Link`; other selectors remain default. |
| `cine_gobo_library-option-21` | Only `cine_gobo_library` selects `Industrial Grate`; other selectors remain default. |
| `cine_gobo_library-option-22` | Only `cine_gobo_library` selects `Rotating Fan`; other selectors remain default. |
| `cine_gobo_library-option-23` | Only `cine_gobo_library` selects `Neon Sign Mask`; other selectors remain default. |
| `card-flare-tallest-850` | Only `flare` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-flare-tallest-1160` | Only `flare` is put in its tallest local state at 1160 px; all other cards stay default. |
| `cine_flare_preset-option-0` | Only `cine_flare_preset` selects `Anamorphic Blue`; other selectors remain default. |
| `cine_flare_preset-option-1` | Only `cine_flare_preset` selects `Anamorphic Gold`; other selectors remain default. |
| `cine_flare_preset-option-2` | Only `cine_flare_preset` selects `Spherical Prime`; other selectors remain default. |
| `cine_flare_preset-option-3` | Only `cine_flare_preset` selects `Vintage Uncoated`; other selectors remain default. |
| `cine_flare_preset-option-4` | Only `cine_flare_preset` selects `Master Anamorphic`; other selectors remain default. |
| `cine_flare_preset-option-5` | Only `cine_flare_preset` selects `Sci-Fi / Neon`; other selectors remain default. |
| `cine_flare_preset-option-6` | Only `cine_flare_preset` selects `Minimal Glint`; other selectors remain default. |
| `cine_flare_preset-option-7` | Only `cine_flare_preset` selects `Rainbow Prism`; other selectors remain default. |
| `cine_flare_preset-option-8` | Only `cine_flare_preset` selects `Dreamy Halo`; other selectors remain default. |
| `cine_flare_preset-option-9` | Only `cine_flare_preset` selects `JJ Blue Blast`; other selectors remain default. |
| `cine_flare_preset-option-10` | Only `cine_flare_preset` selects `Retro 70s Warm`; other selectors remain default. |
| `cine_flare_preset-option-11` | Only `cine_flare_preset` selects `Cyberpunk Neon`; other selectors remain default. |
| `cine_flare_preset-option-12` | Only `cine_flare_preset` selects `Ethereal Angelic`; other selectors remain default. |
| `cine_flare_preset-option-13` | Only `cine_flare_preset` selects `Golden Hour`; other selectors remain default. |
| `cine_flare_preset-option-14` | Only `cine_flare_preset` selects `Noir Practical`; other selectors remain default. |
| `cine_flare_preset-option-15` | Only `cine_flare_preset` selects `Documentary Real`; other selectors remain default. |
| `cine_flare_preset-option-16` | Only `cine_flare_preset` selects `Blockbuster T-O`; other selectors remain default. |
| `cine_flare_preset-option-17` | Only `cine_flare_preset` selects `Sodium Night`; other selectors remain default. |
| `cine_flare_preset-option-18` | Only `cine_flare_preset` selects `Music Video Glam`; other selectors remain default. |
| `card-live_probe-tallest-850` | Only `live_probe` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-live_probe-tallest-1160` | Only `live_probe` is put in its tallest local state at 1160 px; all other cards stay default. |
| `cine_live_probe_target-option-0` | Only `cine_live_probe_target` selects `Self`; other selectors remain default. |
| `cine_live_probe_target-option-1` | Only `cine_live_probe_target` selects `Subject A`; other selectors remain default. |
| `cine_live_probe_target-option-2` | Only `cine_live_probe_target` selects `Subject B`; other selectors remain default. |
| `cine_live_probe_target-option-3` | Only `cine_live_probe_target` selects `Subject C`; other selectors remain default. |
| `cine_live_probe_target-option-4` | Only `cine_live_probe_target` selects `Subject D`; other selectors remain default. |
| `cine_live_probe_refresh-option-0` | Only `cine_live_probe_refresh` selects `Every frame`; other selectors remain default. |
| `cine_live_probe_refresh-option-1` | Only `cine_live_probe_refresh` selects `Balanced`; other selectors remain default. |
| `cine_live_probe_refresh-option-2` | Only `cine_live_probe_refresh` selects `Economy`; other selectors remain default. |
| `cine_live_probe_refresh-option-3` | Only `cine_live_probe_refresh` selects `On change`; other selectors remain default. |
| `cine_live_probe_refresh-option-4` | Only `cine_live_probe_refresh` selects `Manual`; other selectors remain default. |
| `cine_live_probe_gizmo-on` | Only `cine_live_probe_gizmo` is on; dependent visibility is measured. |
| `card-night_mask-tallest-850` | Only `night_mask` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-night_mask-tallest-1160` | Only `night_mask` is put in its tallest local state at 1160 px; all other cards stay default. |
| `cine_night_mask_preset-option-0` | Only `cine_night_mask_preset` selects `Campfire Bubble`; other selectors remain default. |
| `cine_night_mask_preset-option-1` | Only `cine_night_mask_preset` selects `Moonlit Iris`; other selectors remain default. |
| `cine_night_mask_preset-option-2` | Only `cine_night_mask_preset` selects `Stage Box`; other selectors remain default. |
| `cine_night_mask_preset-option-3` | Only `cine_night_mask_preset` selects `Background Drop`; other selectors remain default. |
| `cine_night_mask_preset-option-4` | Only `cine_night_mask_preset` selects `Deep Night Wide`; other selectors remain default. |
| `cine_night_mask_preset-option-5` | Only `cine_night_mask_preset` selects `Pitch Black Cell`; other selectors remain default. |
| `cine_night_mask_shape-option-0` | Only `cine_night_mask_shape` selects `Plane`; other selectors remain default. |
| `cine_night_mask_shape-option-1` | Only `cine_night_mask_shape` selects `Sphere`; other selectors remain default. |
| `cine_night_mask_shape-option-2` | Only `cine_night_mask_shape` selects `Cube`; other selectors remain default. |
| `cine_night_mask_target-option-0` | Only `cine_night_mask_target` selects `Self`; other selectors remain default. |
| `cine_night_mask_target-option-1` | Only `cine_night_mask_target` selects `Subject A`; other selectors remain default. |
| `cine_night_mask_target-option-2` | Only `cine_night_mask_target` selects `Subject B`; other selectors remain default. |
| `cine_night_mask_target-option-3` | Only `cine_night_mask_target` selects `Subject C`; other selectors remain default. |
| `cine_night_mask_target-option-4` | Only `cine_night_mask_target` selects `Subject D`; other selectors remain default. |
| `cine_night_mask_gizmo-on` | Only `cine_night_mask_gizmo` is on; dependent visibility is measured. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## Ambiguous same-name candidates (never auto-bound)

None.

## Rejected out-of-scope same-name candidates

206 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

## New widget required

None. Every approved visual kind is either a kit primitive or an existing XUI control. An unresolved binding is not permission to invent a visual class.

## Unresolved and pending-decision items

- `cine_key_projector_feed` (Projector feed): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `cine_fill_projector_feed` (Projector feed): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `cine_rim_projector_feed` (Projector feed): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `cine_bg_projector_feed` (Projector feed): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.

## Risks

- A widget-name collision can select an unrelated XUI root or C++ literal; ownership must be resolved by root, class, and behavior together.
- Extracting model work without rewiring the existing owner creates divergent v1/v2 behavior; the extraction and forwarding call are one reviewed change.
- Duplicate writable settings must obey the rollout homes table; this page emits a link/read-only row when it is not the chosen home.
- Shared edit models require owner tokens, and external presets/cue playback require one global history barrier at start/stop rather than per write.
- Unresolved rows fail generation closed; they are never silently replaced with a guessed widget or command.
