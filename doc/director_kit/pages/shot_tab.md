# Shot — Director Kit page contract

Page key: `shot_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (36/78 setting-bound rows; 53 C++-referenced rows).

## Real owners

- `panel_director_switcher` / `panel` at `indra/newview/skins/default/xui/en/panel_director_switcher.xml:10` (52 matching rows).
- `panel_cinecam_params` / `panel` at `indra/newview/skins/default/xui/en/panel_cinecam_params.xml:10` (11 matching rows).
- `floater_director` / `floater` at `indra/newview/skins/default/xui/en/floater_director.xml:15` (8 matching rows).
- `panel_avatar_light_response` / `panel` at `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:2` (1 matching rows).
- `panel_flycam_orbit` / `panel` at `indra/newview/skins/default/xui/en/panel_flycam_orbit.xml:13` (1 matching rows).
- Referencing classes: `ALPanelDirectorSwitcher` (27), `LLFloaterDirector` (9), `LLCinematicCamera` (8), `LLPipeline` (7), `ALPanelCineCamParams` (2), `ALPanelAvatarLightResponse` (1).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Keep the existing switcher buttons and `panel_cinecam_params.xml` controls; no slot-grid or curve editor is authorized.

## Row bindings

`ALShotKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `cinematic_cam` / `CinematicCamUseSelected` (Use selected) | `check_box` `indra/newview/skins/default/xui/en/panel_cinecam_params.xml:99`, `check_box` `indra/newview/skins/default/xui/en/panel_flycam_orbit.xml:75` | kit `toggle` ↔ `CinematicCamUseSelected` `Boolean`; `ALShotKitAdapter` preserves/calls `?::?()` (`indra/newview/alpanelcinecamparams.cpp:283`), `?::?()` (`indra/newview/llcinematiccamera.cpp:698`), `LLFloaterDirector::refreshStatusStrip()` (`indra/newview/llfloaterdirector.cpp:3567`) | page `frame_orbit_tab` |
| `cinematic_cam` / `CinematicCamLookAtHead` (Frame head) | `check_box` `indra/newview/skins/default/xui/en/panel_cinecam_params.xml:111` | kit `toggle` ↔ `CinematicCamLookAtHead` `Boolean`; `ALShotKitAdapter` preserves/calls `?::?()` (`indra/newview/alpanelcinecamparams.cpp:284`), `LLCinematicCamera::isActiveHeadFramingTarget()` (`indra/newview/llcinematiccamera.cpp:610`), `LLCinematicCamera::updateCamera()` (`indra/newview/llcinematiccamera.cpp:2600`) | none |
| `cinematic_cam` / `CinematicCamDutchAngle` (Dutch angle) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_params.xml:140` | kit `arc` ↔ `CinematicCamDutchAngle` `F32`; `ALShotKitAdapter` preserves/calls `?::?()` (`indra/newview/alpanelcinecamparams.cpp:375`), `LLCinematicCamera::updateCamera()` (`indra/newview/llcinematiccamera.cpp:2890`) | none |
| `cinematic_cam` / `CineFisheyeEnabled` (Fisheye) | `check_box` `indra/newview/skins/default/xui/en/panel_cinecam_params.xml:176` | kit `toggle` ↔ `CineFisheyeEnabled` `Boolean`; `ALShotKitAdapter` preserves/calls `?::?()` (`indra/newview/pipeline.cpp:23089`) | none |
| `cinematic_cam` / `CinematicCamUseOperator` (Handheld) | `check_box` `indra/newview/skins/default/xui/en/panel_cinecam_params.xml:123` | kit `toggle` ↔ `CinematicCamUseOperator` `Boolean`; `ALShotKitAdapter` preserves/calls `?::?()` (`indra/newview/alpanelcinecamparams.cpp:285`), `LLCinematicCamera::updateCamera()` (`indra/newview/llcinematiccamera.cpp:2601`) | none |
| `cinematic_cam` / `CineFisheyeStrength` (Fisheye bend) | none | kit `slider` ↔ `CineFisheyeStrength` `F32`; `ALShotKitAdapter` preserves/calls `LLPipeline::renderCineFisheye()` (`indra/newview/pipeline.cpp:14836`), `?::?()` (`indra/newview/pipeline.cpp:23240`) | none |
| `cinematic_cam` / `CineFisheyeStrength2` (Edge squeeze) | none | kit `slider` ↔ `CineFisheyeStrength2` `F32`; `ALShotKitAdapter` preserves/calls `LLPipeline::renderCineFisheye()` (`indra/newview/pipeline.cpp:14838`), `?::?()` (`indra/newview/pipeline.cpp:23241`) | none |
| `cinematic_cam` / `CineFisheyeZoom` (Lens zoom) | none | kit `slider` ↔ `CineFisheyeZoom` `F32`; `ALShotKitAdapter` preserves/calls `LLPipeline::renderCineFisheye()` (`indra/newview/pipeline.cpp:14840`), `?::?()` (`indra/newview/pipeline.cpp:23242`) | none |
| `cinematic_cam` / `CineFisheyeVignetteRadius` (Peephole size) | none | kit `slider` ↔ `CineFisheyeVignetteRadius` `F32`; `ALShotKitAdapter` preserves/calls `LLPipeline::renderCineFisheye()` (`indra/newview/pipeline.cpp:14842`), `?::?()` (`indra/newview/pipeline.cpp:23243`) | none |
| `cinematic_cam` / `CineFisheyeVignetteSoft` (Peephole soft) | none | kit `slider` ↔ `CineFisheyeVignetteSoft` `F32`; `ALShotKitAdapter` preserves/calls `LLPipeline::renderCineFisheye()` (`indra/newview/pipeline.cpp:14844`), `?::?()` (`indra/newview/pipeline.cpp:23244`) | none |
| `cinematic_cam` / `CineFisheyeCenterX` (Centre X) | none | kit `slider` ↔ `CineFisheyeCenterX` `F32`; `ALShotKitAdapter` preserves/calls `LLPipeline::renderCineFisheye()` (`indra/newview/pipeline.cpp:14846`), `?::?()` (`indra/newview/pipeline.cpp:23245`) | none |
| `cinematic_cam` / `CineFisheyeCenterY` (Centre Y) | none | kit `slider` ↔ `CineFisheyeCenterY` `F32`; `ALShotKitAdapter` preserves/calls `LLPipeline::renderCineFisheye()` (`indra/newview/pipeline.cpp:14848`), `?::?()` (`indra/newview/pipeline.cpp:23246`) | none |
| `motion_start` / `CinematicCamMotionStartMode` (Start from) | `combo_box` `indra/newview/skins/default/xui/en/panel_cinecam_params.xml:246` | kit `combo` ↔ `CinematicCamMotionStartMode` `S32`; `ALShotKitAdapter` preserves/calls `?::?()` (`indra/newview/alpanelcinecamparams.cpp:286`), `LLCinematicCamera::captureMotionStart()` (`indra/newview/llcinematiccamera.cpp:978`) | none |
| `motion_start` / `CinematicCamMotionStartOffsetDeg` (Offset) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_params.xml:273` | kit `dial` ↔ `CinematicCamMotionStartOffsetDeg` `F32`; `ALShotKitAdapter` preserves/calls `?::?()` (`indra/newview/alpanelcinecamparams.cpp:287`), `LLCinematicCamera::captureMotionStart()` (`indra/newview/llcinematiccamera.cpp:980`) | none |
| `motion_start` / `CinematicCamMotionDirection` (Direction) | `combo_box` `indra/newview/skins/default/xui/en/panel_cinecam_params.xml:290` | kit `combo` ↔ `CinematicCamMotionDirection` `S32`; `ALShotKitAdapter` preserves/calls `?::?()` (`indra/newview/alpanelcinecamparams.cpp:288`), `LLCinematicCamera::captureMotionStart()` (`indra/newview/llcinematiccamera.cpp:982`) | none |
| `motion_start` / `CinematicCamMotionSeed` (Seed) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_params.xml:316` | kit `number` ↔ `CinematicCamMotionSeed` `S32`; `ALShotKitAdapter` preserves/calls `?::?()` (`indra/newview/alpanelcinecamparams.cpp:289`), `LLCinematicCamera::captureMotionStart()` (`indra/newview/llcinematiccamera.cpp:984`) | none |
| `reset_controls` / `btn_reset_mode` (Reset mode) | `button` `indra/newview/skins/default/xui/en/panel_cinecam_params.xml:210` | `ALShotKitAdapter` command/model binding → `ALPanelCineCamParams::postBuild()` (`indra/newview/alpanelcinecamparams.cpp:399`) | none |
| `reset_controls` / `btn_reset_all` (Reset all) | `button` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:332`, `button` `indra/newview/skins/default/xui/en/panel_cinecam_params.xml:220` | `ALShotKitAdapter` command/model binding → `ALPanelAvatarLightResponse::postBuild()` (`indra/newview/alpanelavatarlightresponse.cpp:155`), `ALPanelCineCamParams::postBuild()` (`indra/newview/alpanelcinecamparams.cpp:404`) | none |
| `camera_switcher` / `switcher_auto` (Auto cut) | `check_box` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:41` | kit `toggle` ↔ `DirectorSwitcherAuto` `Boolean` | none |
| `camera_switcher` / `switcher_overrides_flycam` (Take flycam) | `check_box` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:562` | kit `toggle` ↔ `DirectorSwitcherOverridesFlycam` `Boolean` | none |
| `camera_switcher` / `switcher_program_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:51` | `ALShotKitAdapter` command/model binding → `ALPanelDirectorSwitcher::postBuild()` (`indra/newview/alpaneldirectorswitcher.cpp:90`) | none |
| `subjects` / `btn_set_a` (Set A) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:962` | `ALShotKitAdapter` command/model binding → `LLFloaterDirector::postBuild()` (`indra/newview/llfloaterdirector.cpp:567`) | none |
| `subjects` / `btn_clear_a` (Clear A) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:971` | `ALShotKitAdapter` command/model binding → `LLFloaterDirector::postBuild()` (`indra/newview/llfloaterdirector.cpp:571`) | none |
| `subjects` / `btn_set_b` (Set B) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:992` | `ALShotKitAdapter` command/model binding → `LLFloaterDirector::postBuild()` (`indra/newview/llfloaterdirector.cpp:568`) | none |
| `subjects` / `btn_clear_b` (Clear B) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:1001` | `ALShotKitAdapter` command/model binding → `LLFloaterDirector::postBuild()` (`indra/newview/llfloaterdirector.cpp:572`) | none |
| `subjects` / `btn_set_c` (Set C) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:1022` | `ALShotKitAdapter` command/model binding → `LLFloaterDirector::postBuild()` (`indra/newview/llfloaterdirector.cpp:569`) | none |
| `subjects` / `btn_clear_c` (Clear C) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:1031` | `ALShotKitAdapter` command/model binding → `LLFloaterDirector::postBuild()` (`indra/newview/llfloaterdirector.cpp:573`) | none |
| `subjects` / `btn_set_d` (Set D) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:1052` | `ALShotKitAdapter` command/model binding → `LLFloaterDirector::postBuild()` (`indra/newview/llfloaterdirector.cpp:570`) | none |
| `subjects` / `btn_clear_d` (Clear D) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:1061` | `ALShotKitAdapter` command/model binding → `LLFloaterDirector::postBuild()` (`indra/newview/llfloaterdirector.cpp:574`) | none |
| `switcher_slots` / `switcher_slot_01` (1) | `button` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:60` | **USER DECISION:** register an explicit `ALShotKitAdapter` command; no matching setting or C++ literal was found | none |
| `switcher_slots` / `switcher_slot_02` (2) | `button` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:61` | **USER DECISION:** register an explicit `ALShotKitAdapter` command; no matching setting or C++ literal was found | none |
| `switcher_slots` / `switcher_slot_03` (3) | `button` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:62` | **USER DECISION:** register an explicit `ALShotKitAdapter` command; no matching setting or C++ literal was found | none |
| `switcher_slots` / `switcher_slot_04` (4) | `button` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:63` | **USER DECISION:** register an explicit `ALShotKitAdapter` command; no matching setting or C++ literal was found | none |
| `switcher_slots` / `switcher_slot_05` (5) | `button` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:64` | **USER DECISION:** register an explicit `ALShotKitAdapter` command; no matching setting or C++ literal was found | none |
| `switcher_slots` / `switcher_slot_06` (6) | `button` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:65` | **USER DECISION:** register an explicit `ALShotKitAdapter` command; no matching setting or C++ literal was found | none |
| `switcher_slots` / `switcher_slot_07` (7) | `button` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:66` | **USER DECISION:** register an explicit `ALShotKitAdapter` command; no matching setting or C++ literal was found | none |
| `switcher_slots` / `switcher_slot_08` (8) | `button` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:67` | **USER DECISION:** register an explicit `ALShotKitAdapter` command; no matching setting or C++ literal was found | none |
| `switcher_slots` / `switcher_slot_09` (9) | `button` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:68` | **USER DECISION:** register an explicit `ALShotKitAdapter` command; no matching setting or C++ literal was found | none |
| `switcher_slots` / `switcher_slot_10` (10) | `button` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:69` | **USER DECISION:** register an explicit `ALShotKitAdapter` command; no matching setting or C++ literal was found | none |
| `switcher_slots` / `switcher_slot_11` (11) | `button` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:70` | **USER DECISION:** register an explicit `ALShotKitAdapter` command; no matching setting or C++ literal was found | none |
| `switcher_slots` / `switcher_slot_12` (12) | `button` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:71` | **USER DECISION:** register an explicit `ALShotKitAdapter` command; no matching setting or C++ literal was found | none |
| `switcher_slots` / `switcher_reset_slots` (Reset slots) | `button` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:90` | `ALShotKitAdapter` command/model binding → `ALPanelDirectorSwitcher::postBuild()` (`indra/newview/alpaneldirectorswitcher.cpp:91`) | none |
| `selected_slot` / `switcher_slot_mode` (Mode) | `combo_box` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:109` | `ALShotKitAdapter` command/model binding → `ALPanelDirectorSwitcher::postBuild()` (`indra/newview/alpaneldirectorswitcher.cpp:72`) | none |
| `selected_slot` / `switcher_slot_enabled` (Use in auto) | `check_box` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:100` | `ALShotKitAdapter` command/model binding → `ALPanelDirectorSwitcher::postBuild()` (`indra/newview/alpaneldirectorswitcher.cpp:71`) | none |
| `selected_slot` / `switcher_slot_label` (Label) | `line_editor` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:130` | `ALShotKitAdapter` command/model binding → `ALPanelDirectorSwitcher::postBuild()` (`indra/newview/alpaneldirectorswitcher.cpp:77`) | none |
| `selected_slot` / `switcher_primary_subject` (Subject) | `combo_box` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:150` | `ALShotKitAdapter` command/model binding → `ALPanelDirectorSwitcher::postBuild()` (`indra/newview/alpaneldirectorswitcher.cpp:74`) | none |
| `selected_slot` / `switcher_secondary_subject` (Second) | `combo_box` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:169` | `ALShotKitAdapter` command/model binding → `ALPanelDirectorSwitcher::postBuild()` (`indra/newview/alpaneldirectorswitcher.cpp:76`) | none |
| `selected_slot` / `switcher_custom_enabled` (Custom angle) | `check_box` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:180` | `ALShotKitAdapter` command/model binding → `ALPanelDirectorSwitcher::postBuild()` (`indra/newview/alpaneldirectorswitcher.cpp:79`) | none |
| `selected_slot` / `switcher_capture_current` (Capture view) | `button` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:190` | `ALShotKitAdapter` command/model binding → `ALPanelDirectorSwitcher::postBuild()` (`indra/newview/alpaneldirectorswitcher.cpp:87`) | none |
| `custom_angle` / `switcher_custom_help` (Fine-tune) | `text` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:199` | kit readout ← approved static copy; no runtime writer | none |
| `custom_angle` / `switcher_custom_yaw` (Yaw) | `spinner` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:215` | `ALShotKitAdapter` command/model binding → `ALPanelDirectorSwitcher::postBuild()` (`indra/newview/alpaneldirectorswitcher.cpp:80`) | none |
| `custom_angle` / `switcher_custom_pitch` (Pitch) | `spinner` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:233` | `ALShotKitAdapter` command/model binding → `ALPanelDirectorSwitcher::postBuild()` (`indra/newview/alpaneldirectorswitcher.cpp:81`) | none |
| `custom_angle` / `switcher_custom_distance` (Distance) | `spinner` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:251` | `ALShotKitAdapter` command/model binding → `ALPanelDirectorSwitcher::postBuild()` (`indra/newview/alpaneldirectorswitcher.cpp:83`) | none |
| `custom_angle` / `switcher_custom_height` (Height) | `spinner` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:269` | `ALShotKitAdapter` command/model binding → `ALPanelDirectorSwitcher::postBuild()` (`indra/newview/alpaneldirectorswitcher.cpp:84`) | none |
| `custom_angle` / `switcher_custom_fov` (FOV) | `spinner` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:287` | `ALShotKitAdapter` command/model binding → `ALPanelDirectorSwitcher::postBuild()` (`indra/newview/alpaneldirectorswitcher.cpp:85`) | none |
| `custom_angle` / `reset_switcher_custom_yaw` (Reset yaw) | `button` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:221` | `ALShotKitAdapter` command/model binding → `ALPanelDirectorSwitcher::postBuild()` (`indra/newview/alpaneldirectorswitcher.cpp:159`) | none |
| `custom_angle` / `reset_switcher_custom_pitch` (Reset pitch) | `button` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:239` | `ALShotKitAdapter` command/model binding → `ALPanelDirectorSwitcher::postBuild()` (`indra/newview/alpaneldirectorswitcher.cpp:160`) | none |
| `custom_angle` / `reset_switcher_custom_distance` (Reset distance) | `button` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:257` | `ALShotKitAdapter` command/model binding → `ALPanelDirectorSwitcher::postBuild()` (`indra/newview/alpaneldirectorswitcher.cpp:161`) | none |
| `custom_angle` / `reset_switcher_custom_height` (Reset height) | `button` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:275` | `ALShotKitAdapter` command/model binding → `ALPanelDirectorSwitcher::postBuild()` (`indra/newview/alpaneldirectorswitcher.cpp:162`) | none |
| `custom_angle` / `reset_switcher_custom_fov` (Reset FOV) | `button` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:293` | `ALShotKitAdapter` command/model binding → `ALPanelDirectorSwitcher::postBuild()` (`indra/newview/alpaneldirectorswitcher.cpp:163`) | none |
| `auto_timing` / `switcher_interval` (Every (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:327` | kit `number` ↔ `DirectorSwitcherIntervalSec` `F32` | none |
| `auto_timing` / `switcher_jitter` (Jitter (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:355` | kit `number` ↔ `DirectorSwitcherJitterSec` `F32` | none |
| `auto_timing` / `switcher_selection_order` (Order) | `radio_group` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:378` | kit `combo` ↔ `DirectorSwitcherSequence` `Boolean` | none |
| `auto_timing` / `switcher_seed` (Seed) | `spinner` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:397` | kit `number` ↔ `DirectorSwitcherSeed` `U32` | none |
| `cut_ease` / `switcher_ease_duration` (Duration) | `slider` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:437` | kit `slider` ↔ `DirectorSwitcherEaseSec` `F32` | none |
| `cut_ease` / `switcher_ease_curve` (Curve) | `combo_box` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:457` | kit `combo` ↔ `DirectorSwitcherEaseCurve` `S32` | none |
| `cut_ease` / `EaseFeather` (Feather) | `spinner` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:503` | kit `number` ↔ `DirectorSwitcherEaseFeather` `F32` | none |
| `cut_ease` / `switcher_ease_bezier_x1` (Bezier X1) | `spinner` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:514` | kit `number` ↔ `DirectorSwitcherEaseBezierX1` `F32`; `ALShotKitAdapter` preserves/calls `ALPanelDirectorSwitcher::updateCurveControlsEnabled()` (`indra/newview/alpaneldirectorswitcher.cpp:199`) | none |
| `cut_ease` / `switcher_ease_bezier_y1` (Bezier Y1) | `spinner` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:522` | kit `number` ↔ `DirectorSwitcherEaseBezierY1` `F32`; `ALShotKitAdapter` preserves/calls `ALPanelDirectorSwitcher::updateCurveControlsEnabled()` (`indra/newview/alpaneldirectorswitcher.cpp:199`) | none |
| `cut_ease` / `switcher_ease_bezier_x2` (Bezier X2) | `spinner` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:530` | kit `number` ↔ `DirectorSwitcherEaseBezierX2` `F32`; `ALShotKitAdapter` preserves/calls `ALPanelDirectorSwitcher::updateCurveControlsEnabled()` (`indra/newview/alpaneldirectorswitcher.cpp:200`) | none |
| `cut_ease` / `switcher_ease_bezier_y2` (Bezier Y2) | `spinner` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:538` | kit `number` ↔ `DirectorSwitcherEaseBezierY2` `F32`; `ALShotKitAdapter` preserves/calls `ALPanelDirectorSwitcher::updateCurveControlsEnabled()` (`indra/newview/alpaneldirectorswitcher.cpp:200`) | none |
| `slow_world` / `switcher_ease_time_factor` (World speed) | `slider` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:590` | kit `slider` ↔ `DirectorSwitcherEaseTimeFactor` `F32` | none |
| `slow_world` / `switcher_freeze_curve` (Curve) | `combo_box` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:610` | kit `combo` ↔ `DirectorSwitcherFreezeCurve` `S32` | none |
| `slow_world` / `FreezeFeather` (Feather) | `spinner` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:656` | kit `number` ↔ `DirectorSwitcherFreezeFeather` `F32` | none |
| `slow_world` / `switcher_freeze_bezier_x1` (Bezier X1) | `spinner` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:667` | kit `number` ↔ `DirectorSwitcherFreezeBezierX1` `F32`; `ALShotKitAdapter` preserves/calls `ALPanelDirectorSwitcher::updateCurveControlsEnabled()` (`indra/newview/alpaneldirectorswitcher.cpp:203`) | none |
| `slow_world` / `switcher_freeze_bezier_y1` (Bezier Y1) | `spinner` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:675` | kit `number` ↔ `DirectorSwitcherFreezeBezierY1` `F32`; `ALShotKitAdapter` preserves/calls `ALPanelDirectorSwitcher::updateCurveControlsEnabled()` (`indra/newview/alpaneldirectorswitcher.cpp:203`) | none |
| `slow_world` / `switcher_freeze_bezier_x2` (Bezier X2) | `spinner` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:683` | kit `number` ↔ `DirectorSwitcherFreezeBezierX2` `F32`; `ALShotKitAdapter` preserves/calls `ALPanelDirectorSwitcher::updateCurveControlsEnabled()` (`indra/newview/alpaneldirectorswitcher.cpp:204`) | none |
| `slow_world` / `switcher_freeze_bezier_y2` (Bezier Y2) | `spinner` `indra/newview/skins/default/xui/en/panel_director_switcher.xml:691` | kit `number` ↔ `DirectorSwitcherFreezeBezierY2` `F32`; `ALShotKitAdapter` preserves/calls `ALPanelDirectorSwitcher::updateCurveControlsEnabled()` (`indra/newview/alpaneldirectorswitcher.cpp:204`) | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALPanelAvatarLightResponse::postBuild()` — `indra/newview/alpanelavatarlightresponse.cpp:155` | Move model-facing work into `ALShotKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `?::?()` — `indra/newview/alpanelcinecamparams.cpp:283, 284, 285, 286, 287, 288, 289, 375` | Move model-facing work into `ALShotKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineCamParams::postBuild()` — `indra/newview/alpanelcinecamparams.cpp:399, 404` | Move model-facing work into `ALShotKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDirectorSwitcher::postBuild()` — `indra/newview/alpaneldirectorswitcher.cpp:71, 72, 74, 76, 77, 79, 80, 81, 83, 84, 85, 87, 90, 91, 159, 160, 161, 162, 163` | Move model-facing work into `ALShotKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDirectorSwitcher::updateCurveControlsEnabled()` — `indra/newview/alpaneldirectorswitcher.cpp:199, 200, 203, 204` | Move model-facing work into `ALShotKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `?::?()` — `indra/newview/llcinematiccamera.cpp:698` | Move model-facing work into `ALShotKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLCinematicCamera::captureMotionStart()` — `indra/newview/llcinematiccamera.cpp:978, 980, 982, 984` | Move model-facing work into `ALShotKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLCinematicCamera::isActiveHeadFramingTarget()` — `indra/newview/llcinematiccamera.cpp:610` | Move model-facing work into `ALShotKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLCinematicCamera::updateCamera()` — `indra/newview/llcinematiccamera.cpp:2600, 2601, 2890` | Move model-facing work into `ALShotKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::postBuild()` — `indra/newview/llfloaterdirector.cpp:567, 568, 569, 570, 571, 572, 573, 574` | Move model-facing work into `ALShotKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::refreshStatusStrip()` — `indra/newview/llfloaterdirector.cpp:3567` | Move model-facing work into `ALShotKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `?::?()` — `indra/newview/pipeline.cpp:23089, 23240, 23241, 23242, 23243, 23244, 23245, 23246` | Move model-facing work into `ALShotKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLPipeline::renderCineFisheye()` — `indra/newview/pipeline.cpp:14836, 14838, 14840, 14842, 14844, 14846, 14848` | Move model-facing work into `ALShotKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **171 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-cinematic_cam-tallest-850` | Only `cinematic_cam` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-cinematic_cam-tallest-1160` | Only `cinematic_cam` is put in its tallest local state at 1160 px; all other cards stay default. |
| `CinematicCamUseSelected-on` | Only `CinematicCamUseSelected` is on; dependent visibility is measured. |
| `CinematicCamLookAtHead-on` | Only `CinematicCamLookAtHead` is on; dependent visibility is measured. |
| `CineFisheyeEnabled-on` | Only `CineFisheyeEnabled` is on; dependent visibility is measured. |
| `CinematicCamUseOperator-on` | Only `CinematicCamUseOperator` is on; dependent visibility is measured. |
| `card-motion_start-tallest-850` | Only `motion_start` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-motion_start-tallest-1160` | Only `motion_start` is put in its tallest local state at 1160 px; all other cards stay default. |
| `CinematicCamMotionStartMode-option-0` | Only `CinematicCamMotionStartMode` selects `Classic`; other selectors remain default. |
| `CinematicCamMotionStartMode-option-1` | Only `CinematicCamMotionStartMode` selects `Facing`; other selectors remain default. |
| `CinematicCamMotionStartMode-option-2` | Only `CinematicCamMotionStartMode` selects `Camera`; other selectors remain default. |
| `CinematicCamMotionStartMode-option-3` | Only `CinematicCamMotionStartMode` selects `Explicit`; other selectors remain default. |
| `CinematicCamMotionStartMode-option-4` | Only `CinematicCamMotionStartMode` selects `Random`; other selectors remain default. |
| `CinematicCamMotionDirection-option-0` | Only `CinematicCamMotionDirection` selects `Auto`; other selectors remain default. |
| `CinematicCamMotionDirection-option-1` | Only `CinematicCamMotionDirection` selects `CW`; other selectors remain default. |
| `CinematicCamMotionDirection-option-2` | Only `CinematicCamMotionDirection` selects `CCW`; other selectors remain default. |
| `CinematicCamMotionDirection-option-3` | Only `CinematicCamMotionDirection` selects `Random`; other selectors remain default. |
| `card-reset_controls-tallest-850` | Only `reset_controls` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-reset_controls-tallest-1160` | Only `reset_controls` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-camera_switcher-tallest-850` | Only `camera_switcher` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-camera_switcher-tallest-1160` | Only `camera_switcher` is put in its tallest local state at 1160 px; all other cards stay default. |
| `switcher_auto-on` | Only `switcher_auto` is on; dependent visibility is measured. |
| `switcher_overrides_flycam-on` | Only `switcher_overrides_flycam` is on; dependent visibility is measured. |
| `card-subjects-tallest-850` | Only `subjects` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-subjects-tallest-1160` | Only `subjects` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-switcher_slots-tallest-850` | Only `switcher_slots` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-switcher_slots-tallest-1160` | Only `switcher_slots` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-selected_slot-tallest-850` | Only `selected_slot` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-selected_slot-tallest-1160` | Only `selected_slot` is put in its tallest local state at 1160 px; all other cards stay default. |
| `switcher_slot_mode-option-0` | Only `switcher_slot_mode` selects `Bone Lock`; other selectors remain default. |
| `switcher_slot_mode-option-1` | Only `switcher_slot_mode` selects `Orbit`; other selectors remain default. |
| `switcher_slot_mode-option-2` | Only `switcher_slot_mode` selects `Fly Hover`; other selectors remain default. |
| `switcher_slot_mode-option-3` | Only `switcher_slot_mode` selects `Sweep`; other selectors remain default. |
| `switcher_slot_mode-option-4` | Only `switcher_slot_mode` selects `Crane`; other selectors remain default. |
| `switcher_slot_mode-option-5` | Only `switcher_slot_mode` selects `Dolly Zoom`; other selectors remain default. |
| `switcher_slot_mode-option-6` | Only `switcher_slot_mode` selects `Push-In`; other selectors remain default. |
| `switcher_slot_mode-option-7` | Only `switcher_slot_mode` selects `Low Hero`; other selectors remain default. |
| `switcher_slot_mode-option-8` | Only `switcher_slot_mode` selects `Overhead`; other selectors remain default. |
| `switcher_slot_mode-option-9` | Only `switcher_slot_mode` selects `Over-the-Shoulder`; other selectors remain default. |
| `switcher_slot_mode-option-10` | Only `switcher_slot_mode` selects `Crash Zoom`; other selectors remain default. |
| `switcher_slot_mode-option-11` | Only `switcher_slot_mode` selects `Slow Zoom`; other selectors remain default. |
| `switcher_slot_mode-option-12` | Only `switcher_slot_mode` selects `Whip Arc`; other selectors remain default. |
| `switcher_slot_mode-option-13` | Only `switcher_slot_mode` selects `Arc Move`; other selectors remain default. |
| `switcher_slot_mode-option-14` | Only `switcher_slot_mode` selects `Reveal Rise`; other selectors remain default. |
| `switcher_slot_mode-option-15` | Only `switcher_slot_mode` selects `Pull-Back`; other selectors remain default. |
| `switcher_slot_mode-option-16` | Only `switcher_slot_mode` selects `Two-Shot`; other selectors remain default. |
| `switcher_slot_mode-option-17` | Only `switcher_slot_mode` selects `Lead Follow`; other selectors remain default. |
| `switcher_slot_mode-option-18` | Only `switcher_slot_mode` selects `ECU Eyes`; other selectors remain default. |
| `switcher_slot_mode-option-19` | Only `switcher_slot_mode` selects `Long Lens`; other selectors remain default. |
| `switcher_slot_mode-option-20` | Only `switcher_slot_mode` selects `Spiral`; other selectors remain default. |
| `switcher_slot_mode-option-21` | Only `switcher_slot_mode` selects `Pedestal Rise`; other selectors remain default. |
| `switcher_slot_mode-option-22` | Only `switcher_slot_mode` selects `Barrel Roll`; other selectors remain default. |
| `switcher_slot_mode-option-23` | Only `switcher_slot_mode` selects `Corkscrew`; other selectors remain default. |
| `switcher_slot_mode-option-24` | Only `switcher_slot_mode` selects `Pendulum`; other selectors remain default. |
| `switcher_slot_mode-option-25` | Only `switcher_slot_mode` selects `Contra-Orbit`; other selectors remain default. |
| `switcher_slot_mode-option-26` | Only `switcher_slot_mode` selects `Fisheye Lunge`; other selectors remain default. |
| `switcher_slot_mode-option-27` | Only `switcher_slot_mode` selects `Floor Skimmer`; other selectors remain default. |
| `switcher_slot_mode-option-28` | Only `switcher_slot_mode` selects `Boost Rise`; other selectors remain default. |
| `switcher_slot_mode-option-29` | Only `switcher_slot_mode` selects `Boom Over`; other selectors remain default. |
| `switcher_slot_mode-option-30` | Only `switcher_slot_mode` selects `Top Spin`; other selectors remain default. |
| `switcher_slot_mode-option-31` | Only `switcher_slot_mode` selects `Turntable Crane`; other selectors remain default. |
| `switcher_slot_mode-option-32` | Only `switcher_slot_mode` selects `Floating Close-Up`; other selectors remain default. |
| `switcher_slot_mode-option-33` | Only `switcher_slot_mode` selects `Tilt Whip`; other selectors remain default. |
| `switcher_slot_mode-option-34` | Only `switcher_slot_mode` selects `Body Helix Reveal`; other selectors remain default. |
| `switcher_slot_mode-option-35` | Only `switcher_slot_mode` selects `Descent Pedestal`; other selectors remain default. |
| `switcher_slot_mode-option-36` | Only `switcher_slot_mode` selects `Parallax Slide`; other selectors remain default. |
| `switcher_slot_mode-option-37` | Only `switcher_slot_mode` selects `Figure-8`; other selectors remain default. |
| `switcher_slot_mode-option-38` | Only `switcher_slot_mode` selects `Detail Sweep`; other selectors remain default. |
| `switcher_slot_mode-option-39` | Only `switcher_slot_mode` selects `Cable Cam Fly-by`; other selectors remain default. |
| `switcher_slot_mode-option-40` | Only `switcher_slot_mode` selects `Breathing Hold`; other selectors remain default. |
| `switcher_slot_mode-option-41` | Only `switcher_slot_mode` selects `Static Wide`; other selectors remain default. |
| `switcher_slot_mode-option-42` | Only `switcher_slot_mode` selects `Static Medium`; other selectors remain default. |
| `switcher_slot_mode-option-43` | Only `switcher_slot_mode` selects `Static Close`; other selectors remain default. |
| `switcher_slot_mode-option-44` | Only `switcher_slot_mode` selects `Static Profile Left`; other selectors remain default. |
| `switcher_slot_mode-option-45` | Only `switcher_slot_mode` selects `Static Profile Right`; other selectors remain default. |
| `switcher_slot_mode-option-46` | Only `switcher_slot_mode` selects `Static Low Hero`; other selectors remain default. |
| `switcher_slot_mode-option-47` | Only `switcher_slot_mode` selects `Static High Angle`; other selectors remain default. |
| `switcher_slot_mode-option-48` | Only `switcher_slot_mode` selects `Static Full Body`; other selectors remain default. |
| `switcher_slot_enabled-on` | Only `switcher_slot_enabled` is on; dependent visibility is measured. |
| `switcher_primary_subject-option-0` | Only `switcher_primary_subject` selects `Default`; other selectors remain default. |
| `switcher_primary_subject-option-1` | Only `switcher_primary_subject` selects `A`; other selectors remain default. |
| `switcher_primary_subject-option-2` | Only `switcher_primary_subject` selects `B`; other selectors remain default. |
| `switcher_primary_subject-option-3` | Only `switcher_primary_subject` selects `C`; other selectors remain default. |
| `switcher_primary_subject-option-4` | Only `switcher_primary_subject` selects `D`; other selectors remain default. |
| `switcher_secondary_subject-option-0` | Only `switcher_secondary_subject` selects `A`; other selectors remain default. |
| `switcher_secondary_subject-option-1` | Only `switcher_secondary_subject` selects `B`; other selectors remain default. |
| `switcher_secondary_subject-option-2` | Only `switcher_secondary_subject` selects `C`; other selectors remain default. |
| `switcher_secondary_subject-option-3` | Only `switcher_secondary_subject` selects `D`; other selectors remain default. |
| `switcher_custom_enabled-on` | Only `switcher_custom_enabled` is on; dependent visibility is measured. |
| `card-custom_angle-tallest-850` | Only `custom_angle` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-custom_angle-tallest-1160` | Only `custom_angle` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-auto_timing-tallest-850` | Only `auto_timing` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-auto_timing-tallest-1160` | Only `auto_timing` is put in its tallest local state at 1160 px; all other cards stay default. |
| `switcher_selection_order-option-0` | Only `switcher_selection_order` selects `Random`; other selectors remain default. |
| `switcher_selection_order-option-1` | Only `switcher_selection_order` selects `Sequence`; other selectors remain default. |
| `card-cut_ease-tallest-850` | Only `cut_ease` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-cut_ease-tallest-1160` | Only `cut_ease` is put in its tallest local state at 1160 px; all other cards stay default. |
| `switcher_ease_curve-option-0` | Only `switcher_ease_curve` selects `Smootherstep`; other selectors remain default. |
| `switcher_ease_curve-option-1` | Only `switcher_ease_curve` selects `Linear`; other selectors remain default. |
| `switcher_ease_curve-option-2` | Only `switcher_ease_curve` selects `Ease (CSS)`; other selectors remain default. |
| `switcher_ease_curve-option-3` | Only `switcher_ease_curve` selects `Ease In (CSS)`; other selectors remain default. |
| `switcher_ease_curve-option-4` | Only `switcher_ease_curve` selects `Ease Out (CSS)`; other selectors remain default. |
| `switcher_ease_curve-option-5` | Only `switcher_ease_curve` selects `Ease In-Out (CSS)`; other selectors remain default. |
| `switcher_ease_curve-option-6` | Only `switcher_ease_curve` selects `Quad In`; other selectors remain default. |
| `switcher_ease_curve-option-7` | Only `switcher_ease_curve` selects `Quad Out`; other selectors remain default. |
| `switcher_ease_curve-option-8` | Only `switcher_ease_curve` selects `Quad In-Out`; other selectors remain default. |
| `switcher_ease_curve-option-9` | Only `switcher_ease_curve` selects `Cubic In`; other selectors remain default. |
| `switcher_ease_curve-option-10` | Only `switcher_ease_curve` selects `Cubic Out`; other selectors remain default. |
| `switcher_ease_curve-option-11` | Only `switcher_ease_curve` selects `Cubic In-Out`; other selectors remain default. |
| `switcher_ease_curve-option-12` | Only `switcher_ease_curve` selects `Quart In`; other selectors remain default. |
| `switcher_ease_curve-option-13` | Only `switcher_ease_curve` selects `Quart Out`; other selectors remain default. |
| `switcher_ease_curve-option-14` | Only `switcher_ease_curve` selects `Quart In-Out`; other selectors remain default. |
| `switcher_ease_curve-option-15` | Only `switcher_ease_curve` selects `Quint In`; other selectors remain default. |
| `switcher_ease_curve-option-16` | Only `switcher_ease_curve` selects `Quint Out`; other selectors remain default. |
| `switcher_ease_curve-option-17` | Only `switcher_ease_curve` selects `Quint In-Out`; other selectors remain default. |
| `switcher_ease_curve-option-18` | Only `switcher_ease_curve` selects `Expo In`; other selectors remain default. |
| `switcher_ease_curve-option-19` | Only `switcher_ease_curve` selects `Expo Out`; other selectors remain default. |
| `switcher_ease_curve-option-20` | Only `switcher_ease_curve` selects `Expo In-Out`; other selectors remain default. |
| `switcher_ease_curve-option-21` | Only `switcher_ease_curve` selects `Circ In`; other selectors remain default. |
| `switcher_ease_curve-option-22` | Only `switcher_ease_curve` selects `Circ Out`; other selectors remain default. |
| `switcher_ease_curve-option-23` | Only `switcher_ease_curve` selects `Circ In-Out`; other selectors remain default. |
| `switcher_ease_curve-option-24` | Only `switcher_ease_curve` selects `Back In`; other selectors remain default. |
| `switcher_ease_curve-option-25` | Only `switcher_ease_curve` selects `Back Out`; other selectors remain default. |
| `switcher_ease_curve-option-26` | Only `switcher_ease_curve` selects `Back In-Out`; other selectors remain default. |
| `switcher_ease_curve-option-27` | Only `switcher_ease_curve` selects `Elastic In`; other selectors remain default. |
| `switcher_ease_curve-option-28` | Only `switcher_ease_curve` selects `Elastic Out`; other selectors remain default. |
| `switcher_ease_curve-option-29` | Only `switcher_ease_curve` selects `Elastic In-Out`; other selectors remain default. |
| `switcher_ease_curve-option-30` | Only `switcher_ease_curve` selects `Bounce In`; other selectors remain default. |
| `switcher_ease_curve-option-31` | Only `switcher_ease_curve` selects `Bounce Out`; other selectors remain default. |
| `switcher_ease_curve-option-32` | Only `switcher_ease_curve` selects `Bounce In-Out`; other selectors remain default. |
| `switcher_ease_curve-option-33` | Only `switcher_ease_curve` selects `Custom (bezier)`; other selectors remain default. |
| `card-slow_world-tallest-850` | Only `slow_world` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-slow_world-tallest-1160` | Only `slow_world` is put in its tallest local state at 1160 px; all other cards stay default. |
| `switcher_freeze_curve-option-0` | Only `switcher_freeze_curve` selects `Smootherstep`; other selectors remain default. |
| `switcher_freeze_curve-option-1` | Only `switcher_freeze_curve` selects `Linear`; other selectors remain default. |
| `switcher_freeze_curve-option-2` | Only `switcher_freeze_curve` selects `Ease (CSS)`; other selectors remain default. |
| `switcher_freeze_curve-option-3` | Only `switcher_freeze_curve` selects `Ease In (CSS)`; other selectors remain default. |
| `switcher_freeze_curve-option-4` | Only `switcher_freeze_curve` selects `Ease Out (CSS)`; other selectors remain default. |
| `switcher_freeze_curve-option-5` | Only `switcher_freeze_curve` selects `Ease In-Out (CSS)`; other selectors remain default. |
| `switcher_freeze_curve-option-6` | Only `switcher_freeze_curve` selects `Quad In`; other selectors remain default. |
| `switcher_freeze_curve-option-7` | Only `switcher_freeze_curve` selects `Quad Out`; other selectors remain default. |
| `switcher_freeze_curve-option-8` | Only `switcher_freeze_curve` selects `Quad In-Out`; other selectors remain default. |
| `switcher_freeze_curve-option-9` | Only `switcher_freeze_curve` selects `Cubic In`; other selectors remain default. |
| `switcher_freeze_curve-option-10` | Only `switcher_freeze_curve` selects `Cubic Out`; other selectors remain default. |
| `switcher_freeze_curve-option-11` | Only `switcher_freeze_curve` selects `Cubic In-Out`; other selectors remain default. |
| `switcher_freeze_curve-option-12` | Only `switcher_freeze_curve` selects `Quart In`; other selectors remain default. |
| `switcher_freeze_curve-option-13` | Only `switcher_freeze_curve` selects `Quart Out`; other selectors remain default. |
| `switcher_freeze_curve-option-14` | Only `switcher_freeze_curve` selects `Quart In-Out`; other selectors remain default. |
| `switcher_freeze_curve-option-15` | Only `switcher_freeze_curve` selects `Quint In`; other selectors remain default. |
| `switcher_freeze_curve-option-16` | Only `switcher_freeze_curve` selects `Quint Out`; other selectors remain default. |
| `switcher_freeze_curve-option-17` | Only `switcher_freeze_curve` selects `Quint In-Out`; other selectors remain default. |
| `switcher_freeze_curve-option-18` | Only `switcher_freeze_curve` selects `Expo In`; other selectors remain default. |
| `switcher_freeze_curve-option-19` | Only `switcher_freeze_curve` selects `Expo Out`; other selectors remain default. |
| `switcher_freeze_curve-option-20` | Only `switcher_freeze_curve` selects `Expo In-Out`; other selectors remain default. |
| `switcher_freeze_curve-option-21` | Only `switcher_freeze_curve` selects `Circ In`; other selectors remain default. |
| `switcher_freeze_curve-option-22` | Only `switcher_freeze_curve` selects `Circ Out`; other selectors remain default. |
| `switcher_freeze_curve-option-23` | Only `switcher_freeze_curve` selects `Circ In-Out`; other selectors remain default. |
| `switcher_freeze_curve-option-24` | Only `switcher_freeze_curve` selects `Back In`; other selectors remain default. |
| `switcher_freeze_curve-option-25` | Only `switcher_freeze_curve` selects `Back Out`; other selectors remain default. |
| `switcher_freeze_curve-option-26` | Only `switcher_freeze_curve` selects `Back In-Out`; other selectors remain default. |
| `switcher_freeze_curve-option-27` | Only `switcher_freeze_curve` selects `Elastic In`; other selectors remain default. |
| `switcher_freeze_curve-option-28` | Only `switcher_freeze_curve` selects `Elastic Out`; other selectors remain default. |
| `switcher_freeze_curve-option-29` | Only `switcher_freeze_curve` selects `Elastic In-Out`; other selectors remain default. |
| `switcher_freeze_curve-option-30` | Only `switcher_freeze_curve` selects `Bounce In`; other selectors remain default. |
| `switcher_freeze_curve-option-31` | Only `switcher_freeze_curve` selects `Bounce Out`; other selectors remain default. |
| `switcher_freeze_curve-option-32` | Only `switcher_freeze_curve` selects `Bounce In-Out`; other selectors remain default. |
| `switcher_freeze_curve-option-33` | Only `switcher_freeze_curve` selects `Custom (bezier)`; other selectors remain default. |
| `selection-deleted` | The selected model row is removed by another owner while visible. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## New widget required

None. Every approved visual kind is either a kit primitive or an existing XUI control. Missing action/model bindings below are USER DECISIONS, not permission to invent a visual class.

## Unresolved and USER DECISION items

- `CineFisheyeStrength` (Fisheye bend): widget not found in XUI.
- `CineFisheyeStrength2` (Edge squeeze): widget not found in XUI.
- `CineFisheyeZoom` (Lens zoom): widget not found in XUI.
- `CineFisheyeVignetteRadius` (Peephole size): widget not found in XUI.
- `CineFisheyeVignetteSoft` (Peephole soft): widget not found in XUI.
- `CineFisheyeCenterX` (Centre X): widget not found in XUI.
- `CineFisheyeCenterY` (Centre Y): widget not found in XUI.
- `switcher_slot_01` (1): no C++ reference or setting binding. **USER DECISION required.**
- `switcher_slot_02` (2): no C++ reference or setting binding. **USER DECISION required.**
- `switcher_slot_03` (3): no C++ reference or setting binding. **USER DECISION required.**
- `switcher_slot_04` (4): no C++ reference or setting binding. **USER DECISION required.**
- `switcher_slot_05` (5): no C++ reference or setting binding. **USER DECISION required.**
- `switcher_slot_06` (6): no C++ reference or setting binding. **USER DECISION required.**
- `switcher_slot_07` (7): no C++ reference or setting binding. **USER DECISION required.**
- `switcher_slot_08` (8): no C++ reference or setting binding. **USER DECISION required.**
- `switcher_slot_09` (9): no C++ reference or setting binding. **USER DECISION required.**
- `switcher_slot_10` (10): no C++ reference or setting binding. **USER DECISION required.**
- `switcher_slot_11` (11): no C++ reference or setting binding. **USER DECISION required.**
- `switcher_slot_12` (12): no C++ reference or setting binding. **USER DECISION required.**
- `switcher_custom_help` (Fine-tune): no C++ reference or setting binding.

## Risks

- A widget-name collision can select an unrelated XUI root or C++ literal; ownership must be resolved by root, class, and behavior together.
- Extracting model work without rewiring the existing owner creates divergent v1/v2 behavior; the extraction and forwarding call are one reviewed change.
- Duplicate writable settings must obey the rollout homes table; this page emits a link/read-only row when it is not the chosen home.
- Shared edit models require owner tokens, and external presets/cue playback require one global history barrier at start/stop rather than per write.
- Unresolved rows fail generation closed; they are never silently replaced with a guessed widget or command.
