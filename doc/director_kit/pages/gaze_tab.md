# Gaze — Director Kit page contract

Page key: `gaze_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (15/148 setting-bound rows; 130 C++-referenced rows).

## Real owners

- `panel_lens_gaze` / `panel` at `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:7` (145 matching rows).
- `floater_director` / `floater` at `indra/newview/skins/default/xui/en/floater_director.xml:15` (1 matching rows).
- Referencing classes: `ALPanelLensGaze` (198), `LLFloaterDirector` (2), `LLActorMover` (1), `LLDirectorCast` (1), `LLFloaterReg` (1), `ALGazeMath` (1).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALGazeKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `gaze_subjects` / `gaze_apply_to` (Apply to) | none | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::refreshEditActorCombo()` (`indra/newview/alpanellensgaze.cpp:452`) | none |
| `gaze_subjects` / `gaze_apply_group` (Group) | none | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::<inline callback>()` (`indra/newview/alpanellensgaze.cpp:455`) | none |
| `gaze_subjects` / `gaze_slot_you` (You) | `check_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:35` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onSlotLookAtCommit()` (`indra/newview/alpanellensgaze.cpp:545`) | none |
| `gaze_subjects` / `gaze_slot_a` (Subject A) | `check_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:38` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onSlotLookAtCommit()` (`indra/newview/alpanellensgaze.cpp:546`) | none |
| `gaze_subjects` / `gaze_slot_b` (Subject B) | `check_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:41` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onSlotLookAtCommit()` (`indra/newview/alpanellensgaze.cpp:547`) | none |
| `gaze_subjects` / `gaze_slot_c` (Subject C) | `check_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:44` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onSlotLookAtCommit()` (`indra/newview/alpanellensgaze.cpp:548`) | none |
| `gaze_subjects` / `gaze_slot_d` (Subject D) | `check_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:47` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onSlotLookAtCommit()` (`indra/newview/alpanellensgaze.cpp:549`) | none |
| `gaze_subjects` / `gaze_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:18` | kit readout ← approved static copy; no runtime writer | none |
| `gaze_subjects` / `gaze_base_hint` (Shared base) | `text` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:734` | kit readout ← approved static copy; no runtime writer | none |
| `hot_targets` / `hot_slot` (Slot) | `combo_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:830` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::buildHotControls()`; behavior `ALPanelLensGaze::asInteger()` (`indra/newview/alpanellensgaze.cpp:2421`) | none |
| `hot_targets` / `hot_who` (Who) | `combo_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:832` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `hot_targets` / `hot_target` (Target) | `combo_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:834` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `hot_targets` / `hot_actor` (Target actor) | `combo_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:836` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `hot_targets` / `hot_pick` (Pick) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:838` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::buildHotControls()`; behavior `ALPanelLensGaze::asInteger()` (`indra/newview/alpanellensgaze.cpp:2433`) | none |
| `hot_targets` / `hot_name` (Chosen target) | `text` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:840` | kit readout ← approved static copy; no runtime writer | none |
| `hot_targets` / `hot_spot` (Object aim) | `combo_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:866` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `hot_targets` / `hot_x` (Nudge X (m)) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:868` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `hot_targets` / `hot_y` (Nudge Y (m)) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:870` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `hot_targets` / `hot_z` (Nudge Z (m)) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:872` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `hot_targets` / `hot_nudge_reset` (Reset nudge) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:874` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::buildHotControls()`; behavior `ALPanelLensGaze::setHotGazeSlot()` (`indra/newview/alpanellensgaze.cpp:2428`) | none |
| `hot_targets` / `hot_clear` (Clear) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:842` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::buildHotControls()`; behavior `ALPanelLensGaze::setHotGazeSlot()` (`indra/newview/alpanellensgaze.cpp:2438`) | none |
| `hot_targets` / `hot_snap` (Snap (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:844` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `hot_targets` / `hot_head` (Head) | `slider` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:846` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `hot_targets` / `hot_eyes` (Eyes) | `slider` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:848` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `hot_targets` / `hot_torso` (Torso) | `slider` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:850` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `hot_targets` / `hot_after` (Return after dwell) | `check_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:852` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `hot_targets` / `hot_dwell` (Dwell (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:854` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `hot_targets` / `hot_return` (Return (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:856` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `hot_targets` / `hot_mode` (Key mode) | `combo_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:858` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `hot_targets` / `hot_fire` (Fire) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:860` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::buildHotControls()`; behavior `LLActorMover::hotGazeInput()` (`indra/newview/alpanellensgaze.cpp:2452`) | none |
| `hot_targets` / `hot_reset` (Reset slot) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:862` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::buildHotControls()`; behavior `LLDirectorCast::setHotGazeSlot()` (`indra/newview/alpanellensgaze.cpp:2444`) | none |
| `hot_targets` / `hot_gamepad` (Gamepad D-pad) | `check_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:864` | kit `toggle` ↔ `DirectorHotGazeGamepad` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_performance_gaze.xml:151` `hot_gamepad` |
| `targets` / `gaze_target_combo` (Head target) | `combo_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:82` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onTargetCommit()` (`indra/newview/alpanellensgaze.cpp:552`) | none |
| `targets` / `gaze_eye_target_combo` (Eyes look at) | `combo_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:97` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onEyeTargetCommit()` (`indra/newview/alpanellensgaze.cpp:553`) | none |
| `targets` / `gaze_target_detail_scope` (Edit target) | `combo_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:204` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onTargetDetailScopeCommit()` (`indra/newview/alpanellensgaze.cpp:558`) | none |
| `targets` / `gaze_cast_combo` (Cast member) | `combo_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:211` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onCastCommit()` (`indra/newview/alpanellensgaze.cpp:560`) | none |
| `targets` / `btn_gaze_setpoint` (Set point) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:215` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onSetPoint()` (`indra/newview/alpanellensgaze.cpp:561`) | none |
| `targets` / `btn_gaze_pick_object` (Pick object) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:219` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onPickObject()` (`indra/newview/alpanellensgaze.cpp:562`) | none |
| `targets` / `btn_gaze_clear_object` (Clear target) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:223` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onClearObject()` (`indra/newview/alpanellensgaze.cpp:563`) | none |
| `targets` / `reset_gaze_target` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:91` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onTargetCommit()` (`indra/newview/alpanellensgaze.cpp:624`) | none |
| `targets` / `reset_gaze_eye_target` (Reset eyes) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:109` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onEyeTargetCommit()` (`indra/newview/alpanellensgaze.cpp:630`) | none |
| `strength` / `gaze_intensity_slider` (Base strength) | `slider` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:339` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onIntensityCommit()` (`indra/newview/alpanellensgaze.cpp:566`) | none |
| `strength` / `gaze_blend_slider` (Head/eyes) | `slider` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:323` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onBlendCommit()` (`indra/newview/alpanellensgaze.cpp:564`) | none |
| `strength` / `gaze_torso_slider` (Torso) | `slider` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:331` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onTorsoCommit()` (`indra/newview/alpanellensgaze.cpp:565`) | none |
| `strength` / `gaze_smoothing_slider` (Smoothing) | `slider` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:368` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onSmoothingCommit()` (`indra/newview/alpanellensgaze.cpp:596`) | none |
| `strength` / `gaze_influence_slider` (Influence) | `slider` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:353` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onInfluenceSet()` (`indra/newview/alpanellensgaze.cpp:1758`) | none |
| `strength` / `btn_gaze_influence_set` (Set key) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:357` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onInfluenceSet()` (`indra/newview/alpanellensgaze.cpp:569`) | none |
| `strength` / `btn_gaze_influence_delete` (Delete key) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:361` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onInfluenceDelete()` (`indra/newview/alpanellensgaze.cpp:570`) | none |
| `strength` / `reset_gaze_intensity` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:343` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onIntensityCommit()` (`indra/newview/alpanellensgaze.cpp:668`) | none |
| `strength` / `reset_gaze_blend` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:326` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onBlendCommit()` (`indra/newview/alpanellensgaze.cpp:656`) | none |
| `strength` / `reset_gaze_torso` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:334` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onTorsoCommit()` (`indra/newview/alpanellensgaze.cpp:662`) | none |
| `strength` / `reset_gaze_smoothing` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:371` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onSmoothingCommit()` (`indra/newview/alpanellensgaze.cpp:674`) | none |
| `presets_blend` / `gaze_performance_combo` (Performance) | `combo_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:233` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onPerformanceCommit()` (`indra/newview/alpanellensgaze.cpp:600`) | none |
| `presets_blend` / `reset_gaze_performance` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:238` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onResetPerformance()` (`indra/newview/alpanellensgaze.cpp:636`) | none |
| `presets_blend` / `gaze_transition_slider` (Transition) | `slider` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:244` | kit `slider` ↔ `DirectorGazeTransitionSec` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_performance_gaze.xml:265` `gaze_transition_slider` |
| `presets_blend` / `gaze_easing_combo` (Easing) | `combo_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:255` | kit `combo` ↔ `DirectorGazeEasing` `U32` | `indra/newview/skins/default/xui/en/panel_dpage_performance_gaze.xml:270` `gaze_easing_combo` |
| `presets_blend` / `gaze_random_check` (Auto-cycle) | `check_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:273` | kit `toggle` ↔ `DirectorGazeRandomMode` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_performance_gaze.xml:280` `gaze_random_check` |
| `presets_blend` / `gaze_random_interval_slider` (Interval) | `slider` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:279` | kit `slider` ↔ `DirectorGazeRandomInterval` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_performance_gaze.xml:283` `gaze_random_interval_slider` |
| `gaze_windows` / `btn_actor_gaze` (Gaze window) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:897` | kit-native action `owner-command:LLFloaterDirector::onOpenActorGaze`; no duplicate widget is required | none |
| `gaze_windows` / `btn_pose_polish` (Pose Polish) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:822` | kit-native action `floater:pose_polish`; no duplicate widget is required | none |
| `gaze_windows` / `btn_gaze_cues` (Gaze Cues) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:826` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onGazeCues()` (`indra/newview/alpanellensgaze.cpp:617`) | none |
| `camera_lookat` / `look_at_camera_mode` (Mode) | `combo_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:740` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onModeCommit()` (`indra/newview/alpanellensgaze.cpp:616`) | none |
| `camera_lookat` / `look_at_camera_strength` (Strength) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:755` | kit `number` ↔ `DirectorLookAtCameraStrength` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_performance_gaze.xml:309` `look_at_camera_strength` |
| `camera_lookat` / `look_at_camera_head_eye` (Head/eyes) | `slider` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:766` | kit `slider` ↔ `DirectorLookAtCameraHeadEye` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_performance_gaze.xml:312` `look_at_camera_head_eye` |
| `camera_lookat` / `look_at_camera_torso` (Torso) | `check_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:779` | kit `toggle` ↔ `DirectorLookAtCameraTorso` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_performance_gaze.xml:317` `look_at_camera_torso` |
| `camera_lookat` / `look_at_camera_smoothing` (Smoothing) | `slider` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:797` | kit `slider` ↔ `DirectorLookAtCameraSmoothing` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_performance_gaze.xml:320` `look_at_camera_smoothing` |
| `camera_lookat` / `look_at_camera_torso_amount` (Torso amount) | `slider` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:786` | kit `slider` ↔ `DirectorLookAtCameraTorsoAmount` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_performance_gaze.xml:325` `look_at_camera_torso_amount` |
| `camera_lookat` / `look_at_camera_ease_time` (Ease master) | `slider` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:809` | kit `slider` ↔ `DirectorLookAtCameraEaseTime` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_performance_gaze.xml:330` `look_at_camera_ease_time` |
| `camera_lookat` / `reset_look_at_camera_mode` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:749` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::editGazeTargetsFor()` (`indra/newview/alpanellensgaze.cpp:764`) | none |
| `aim_eyeline` / `gaze_eyeline_yaw_slider` (Eyeline yaw) | `slider` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:376` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onEyelineCommit()` (`indra/newview/alpanellensgaze.cpp:598`) | none |
| `aim_eyeline` / `gaze_eyeline_pitch_slider` (Eyeline pitch) | `slider` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:384` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onEyelineCommit()` (`indra/newview/alpanellensgaze.cpp:599`) | none |
| `aim_eyeline` / `gaze_camera_roll_slider` (Camera roll) | `slider` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:516` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onCameraRollCommit()` (`indra/newview/alpanellensgaze.cpp:614`) | none |
| `aim_eyeline` / `gaze_breakoff_check` (Break off) | `check_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:494` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onBreakoffCommit()` (`indra/newview/alpanellensgaze.cpp:597`) | none |
| `aim_eyeline` / `gaze_breakoff_spinner` (Yaw limit) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:500` | kit `number` ↔ `BDMergeGazeBehindAngle` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_performance_gaze.xml:358` `gaze_breakoff_spinner` |
| `aim_eyeline` / `reset_gaze_eyeline_yaw` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:379` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onEyelineCommit()` (`indra/newview/alpanellensgaze.cpp:680`) | none |
| `aim_eyeline` / `reset_gaze_eyeline_pitch` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:387` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onEyelineCommit()` (`indra/newview/alpanellensgaze.cpp:686`) | none |
| `aim_eyeline` / `reset_gaze_camera_roll` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:521` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::editGazeTargetsFor()` (`indra/newview/alpanellensgaze.cpp:752`) | none |
| `lock_timing` / `gaze_ease_acquire_spinner` (Acquire (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:462` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onEaseAcquireCommit()` (`indra/newview/alpanellensgaze.cpp:611`) | none |
| `lock_timing` / `gaze_ease_release_spinner` (Release (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:471` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onEaseReleaseCommit()` (`indra/newview/alpanellensgaze.cpp:612`) | none |
| `lock_timing` / `gaze_deadzone_spinner` (Dead zone) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:485` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onDeadZoneCommit()` (`indra/newview/alpanellensgaze.cpp:613`) | none |
| `lock_timing` / `reset_gaze_ease_acquire` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:466` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::editGazeTargetsFor()` (`indra/newview/alpanellensgaze.cpp:713`) | none |
| `lock_timing` / `reset_gaze_ease_release` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:475` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::editGazeTargetsFor()` (`indra/newview/alpanellensgaze.cpp:719`) | none |
| `lock_timing` / `reset_gaze_deadzone` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:490` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::editGazeTargetsFor()` (`indra/newview/alpanellensgaze.cpp:725`) | none |
| `feel` / `gaze_priority_combo` (Movement style) | `combo_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:115` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onPriorityCommit()` (`indra/newview/alpanellensgaze.cpp:554`) | none |
| `feel` / `gaze_ik_lean_combo` (Lean preset) | `combo_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:130` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onIkLeanCommit()` (`indra/newview/alpanellensgaze.cpp:557`) | none |
| `feel` / `gaze_anim_priority_combo` (SL priority) | `combo_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:155` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onAnimPriorityCommit()` (`indra/newview/alpanellensgaze.cpp:555`) | none |
| `feel` / `gaze_composition_combo` (Composition) | `combo_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:179` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onCompositionCommit()` (`indra/newview/alpanellensgaze.cpp:567`) | none |
| `feel` / `gaze_chest_share_slider` (Chest share) | `slider` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:544` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onChestShareCommit()` (`indra/newview/alpanellensgaze.cpp:571`) | none |
| `feel` / `gaze_composition_mix_slider` (Retention) | `slider` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:193` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onCompositionMixCommit()` (`indra/newview/alpanellensgaze.cpp:568`) | none |
| `feel` / `reset_gaze_priority` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:146` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::editGazeTargetsFor()` (`indra/newview/alpanellensgaze.cpp:731`) | none |
| `feel` / `reset_gaze_ik_lean` (Reset lean) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:149` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::LLSD()` (`indra/newview/alpanellensgaze.cpp:750`) | none |
| `feel` / `reset_gaze_anim_priority` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:170` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::editGazeTargetsFor()` (`indra/newview/alpanellensgaze.cpp:737`) | none |
| `feel` / `reset_gaze_composition` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:187` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::editGazeTargetsFor()` (`indra/newview/alpanellensgaze.cpp:774`) | none |
| `feel` / `reset_gaze_composition_mix` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:198` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::editGazeTargetsFor()` (`indra/newview/alpanellensgaze.cpp:780`) | none |
| `feel` / `reset_gaze_chest_share` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:548` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::editGazeTargetsFor()` (`indra/newview/alpanellensgaze.cpp:786`) | none |
| `rotation_range` / `gaze_range_preset_combo` (Range preset) | `combo_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:63` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onRangePresetCommit()` (`indra/newview/alpanellensgaze.cpp:556`) | none |
| `rotation_range` / `gaze_limit_mode_combo` (Limits source) | `combo_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:554` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onLimitModeCommit()` (`indra/newview/alpanellensgaze.cpp:572`) | none |
| `rotation_range` / `gaze_exaggerate_slider` (Exaggerate) | `slider` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:527` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onExaggerateCommit()` (`indra/newview/alpanellensgaze.cpp:615`) | none |
| `rotation_range` / `reset_gaze_range_preset` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:76` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::LLSD()` (`indra/newview/alpanellensgaze.cpp:748`) | none |
| `rotation_range` / `reset_gaze_limit_mode` (Reset source) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:562` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::editGazeTargetsFor()` (`indra/newview/alpanellensgaze.cpp:792`) | none |
| `rotation_range` / `reset_gaze_exaggerate` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:532` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::editGazeTargetsFor()` (`indra/newview/alpanellensgaze.cpp:758`) | none |
| `lean_curve` / `gaze_lean_curve_combo` (Lean curve) | `combo_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:679` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onLeanCurveCommit()` (`indra/newview/alpanellensgaze.cpp:592`) | none |
| `lean_curve` / `gaze_lean_threshold_spinner` (Threshold) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:692` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onLeanThresholdCommit()` (`indra/newview/alpanellensgaze.cpp:593`) | none |
| `lean_curve` / `gaze_lean_softness_spinner` (Softness) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:701` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onLeanSoftnessCommit()` (`indra/newview/alpanellensgaze.cpp:594`) | none |
| `lean_curve` / `gaze_lean_max_spinner` (Lean max) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:710` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onLeanMaxCommit()` (`indra/newview/alpanellensgaze.cpp:595`) | none |
| `lean_curve` / `gaze_soft_recruit_spinner` (Soft recruit) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:720` | kit `number` ↔ `DirectorGazeSoftRecruitDeg` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_performance_gaze.xml:527` `gaze_soft_recruit_spinner` |
| `lean_curve` / `reset_gaze_lean_curve` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:686` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::editGazeTargetsFor()` (`indra/newview/alpanellensgaze.cpp:808`) | none |
| `lean_curve` / `reset_gaze_lean_threshold` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:696` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::editGazeTargetsFor()` (`indra/newview/alpanellensgaze.cpp:814`) | none |
| `lean_curve` / `reset_gaze_lean_softness` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:705` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::editGazeTargetsFor()` (`indra/newview/alpanellensgaze.cpp:820`) | none |
| `lean_curve` / `reset_gaze_lean_max` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:714` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::editGazeTargetsFor()` (`indra/newview/alpanellensgaze.cpp:826`) | none |
| `joint_limits` / `gaze_limit_eye_yaw_spinner` (Eye yaw) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:583` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onLimitProfileCommit()` (`indra/newview/alpanellensgaze.cpp:573`) | none |
| `joint_limits` / `gaze_limit_head_yaw_spinner` (Head yaw) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:598` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onLimitProfileCommit()` (`indra/newview/alpanellensgaze.cpp:576`) | none |
| `joint_limits` / `gaze_limit_neck_yaw_spinner` (Neck yaw) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:613` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onLimitProfileCommit()` (`indra/newview/alpanellensgaze.cpp:579`) | none |
| `joint_limits` / `gaze_limit_spine_yaw_spinner` (Spine yaw) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:628` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onLimitProfileCommit()` (`indra/newview/alpanellensgaze.cpp:582`) | none |
| `joint_limits` / `gaze_limit_hips_yaw_spinner` (Hips yaw) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:643` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onLimitProfileCommit()` (`indra/newview/alpanellensgaze.cpp:585`) | none |
| `joint_limits` / `gaze_limit_eye_pitch_spinner` (Eye down) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:587` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onLimitProfileCommit()` (`indra/newview/alpanellensgaze.cpp:574`) | none |
| `joint_limits` / `gaze_limit_eye_pitchup_spinner` (Eye up) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:591` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onLimitProfileCommit()` (`indra/newview/alpanellensgaze.cpp:575`) | none |
| `joint_limits` / `gaze_limit_head_pitch_spinner` (Head down) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:602` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onLimitProfileCommit()` (`indra/newview/alpanellensgaze.cpp:577`) | none |
| `joint_limits` / `gaze_limit_head_pitchup_spinner` (Head up) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:606` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onLimitProfileCommit()` (`indra/newview/alpanellensgaze.cpp:578`) | none |
| `joint_limits` / `gaze_limit_neck_pitch_spinner` (Neck down) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:617` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onLimitProfileCommit()` (`indra/newview/alpanellensgaze.cpp:580`) | none |
| `joint_limits` / `gaze_limit_neck_pitchup_spinner` (Neck up) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:621` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onLimitProfileCommit()` (`indra/newview/alpanellensgaze.cpp:581`) | none |
| `joint_limits` / `gaze_limit_spine_pitch_spinner` (Spine down) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:632` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onLimitProfileCommit()` (`indra/newview/alpanellensgaze.cpp:583`) | none |
| `joint_limits` / `gaze_limit_spine_pitchup_spinner` (Spine up) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:636` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onLimitProfileCommit()` (`indra/newview/alpanellensgaze.cpp:584`) | none |
| `joint_limits` / `gaze_limit_hips_pitch_spinner` (Hips down) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:647` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onLimitProfileCommit()` (`indra/newview/alpanellensgaze.cpp:586`) | none |
| `joint_limits` / `gaze_limit_hips_pitchup_spinner` (Hips up) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:651` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onLimitProfileCommit()` (`indra/newview/alpanellensgaze.cpp:587`) | none |
| `joint_limits` / `gaze_limit_eye_radial_spinner` (Eye radial) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:673` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onLimitProfileCommit()` (`indra/newview/alpanellensgaze.cpp:591`) | none |
| `joint_limits` / `gaze_limit_eyeapply_yaw_spinner` (Socket yaw) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:658` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onLimitProfileCommit()` (`indra/newview/alpanellensgaze.cpp:588`) | none |
| `joint_limits` / `gaze_limit_eyeapply_pitch_spinner` (Socket down) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:662` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onLimitProfileCommit()` (`indra/newview/alpanellensgaze.cpp:589`) | none |
| `joint_limits` / `gaze_limit_eyeapply_pitchup_spinner` (Socket up) | `spinner` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:666` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onLimitProfileCommit()` (`indra/newview/alpanellensgaze.cpp:590`) | none |
| `joint_limits` / `gaze_limit_grid_lbl` (Joint caps) | `text` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:566` | kit readout ← approved static copy; no runtime writer | none |
| `joint_limits` / `gaze_limit_grid_yaw_hdr` (Yaw) | `text` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:569` | kit readout ← approved static copy; no runtime writer | none |
| `joint_limits` / `gaze_limit_grid_pitch_hdr` (Pitch) | `text` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:571` | kit readout ← approved static copy; no runtime writer | none |
| `joint_limits` / `reset_gaze_limit_profile` (Reset profile) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:575` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALGazeMath::AnatomicalLimitProfile()` (`indra/newview/alpanellensgaze.cpp:798`) | none |
| `persona` / `gaze_persona_dom_slider` (Dominance) | `slider` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:291` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onPersonaCommit()` (`indra/newview/alpanellensgaze.cpp:601`) | none |
| `persona` / `gaze_persona_aff_slider` (Affection) | `slider` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:300` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onPersonaCommit()` (`indra/newview/alpanellensgaze.cpp:602`) | none |
| `persona` / `gaze_persona_anx_slider` (Anxiety) | `slider` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:309` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onPersonaCommit()` (`indra/newview/alpanellensgaze.cpp:603`) | none |
| `persona` / `reset_gaze_persona_dom` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:295` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onPersonaCommit()` (`indra/newview/alpanellensgaze.cpp:638`) | none |
| `persona` / `reset_gaze_persona_aff` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:304` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onPersonaCommit()` (`indra/newview/alpanellensgaze.cpp:644`) | none |
| `persona` / `reset_gaze_persona_anx` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:313` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onPersonaCommit()` (`indra/newview/alpanellensgaze.cpp:650`) | none |
| `life_cadence` / `gaze_microlife_slider` (Micro-life) | `slider` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:420` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onMicroLifeCommit()` (`indra/newview/alpanellensgaze.cpp:607`) | none |
| `life_cadence` / `gaze_blinks_check` (Blinks) | `check_box` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:432` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onBlinksCommit()` (`indra/newview/alpanellensgaze.cpp:608`) | none |
| `life_cadence` / `gaze_variation_slider` (Variation) | `slider` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:440` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onVariationCommit()` (`indra/newview/alpanellensgaze.cpp:609`) | none |
| `life_cadence` / `gaze_break_freq_slider` (Glance breaks) | `slider` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:452` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::onBreakFreqCommit()` (`indra/newview/alpanellensgaze.cpp:610`) | none |
| `life_cadence` / `gaze_stillness_slider` (Stillness) | `slider` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:392` | kit `slider` ↔ `DirectorGazeStillness` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_performance_gaze.xml:659` `gaze_stillness_slider` |
| `life_cadence` / `gaze_restraint_slider` (Restraint) | `slider` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:403` | kit `slider` ↔ `DirectorGazeRestraint` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_performance_gaze.xml:664` `gaze_restraint_slider` |
| `life_cadence` / `reset_gaze_microlife` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:425` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::editGazeTargetsFor()` (`indra/newview/alpanellensgaze.cpp:695`) | none |
| `life_cadence` / `reset_gaze_variation` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:445` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::editGazeTargetsFor()` (`indra/newview/alpanellensgaze.cpp:701`) | none |
| `life_cadence` / `reset_gaze_break_frequency` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_lens_gaze.xml:456` | `ALGazeKitAdapter` command/model binding → wired in `ALPanelLensGaze::postBuild()`; behavior `ALPanelLensGaze::editGazeTargetsFor()` (`indra/newview/alpanellensgaze.cpp:707`) | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALGazeMath::AnatomicalLimitProfile()` — `indra/newview/alpanellensgaze.cpp:798` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::<inline callback>()` — `indra/newview/alpanellensgaze.cpp:455` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::LLSD()` — `indra/newview/alpanellensgaze.cpp:748, 750` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::asInteger()` — `indra/newview/alpanellensgaze.cpp:2421, 2433` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::editGazeTargetsFor()` — `indra/newview/alpanellensgaze.cpp:695, 701, 707, 713, 719, 725, 731, 737, 752, 758, 764, 774, 780, 786, 792, 808, 814, 820, 826` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onAnimPriorityCommit()` — `indra/newview/alpanellensgaze.cpp:555` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onBlendCommit()` — `indra/newview/alpanellensgaze.cpp:564, 656` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onBlinksCommit()` — `indra/newview/alpanellensgaze.cpp:608` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onBreakFreqCommit()` — `indra/newview/alpanellensgaze.cpp:610` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onBreakoffCommit()` — `indra/newview/alpanellensgaze.cpp:597` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onCameraRollCommit()` — `indra/newview/alpanellensgaze.cpp:614` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onCastCommit()` — `indra/newview/alpanellensgaze.cpp:560` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onChestShareCommit()` — `indra/newview/alpanellensgaze.cpp:571` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onClearObject()` — `indra/newview/alpanellensgaze.cpp:563` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onCompositionCommit()` — `indra/newview/alpanellensgaze.cpp:567` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onCompositionMixCommit()` — `indra/newview/alpanellensgaze.cpp:568` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onDeadZoneCommit()` — `indra/newview/alpanellensgaze.cpp:613` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onEaseAcquireCommit()` — `indra/newview/alpanellensgaze.cpp:611` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onEaseReleaseCommit()` — `indra/newview/alpanellensgaze.cpp:612` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onExaggerateCommit()` — `indra/newview/alpanellensgaze.cpp:615` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onEyeTargetCommit()` — `indra/newview/alpanellensgaze.cpp:553, 630` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onEyelineCommit()` — `indra/newview/alpanellensgaze.cpp:598, 599, 680, 686` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onGazeCues()` — `indra/newview/alpanellensgaze.cpp:617` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onIkLeanCommit()` — `indra/newview/alpanellensgaze.cpp:557` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onInfluenceDelete()` — `indra/newview/alpanellensgaze.cpp:570` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onInfluenceSet()` — `indra/newview/alpanellensgaze.cpp:569, 1758` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onIntensityCommit()` — `indra/newview/alpanellensgaze.cpp:566, 668` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onLeanCurveCommit()` — `indra/newview/alpanellensgaze.cpp:592` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onLeanMaxCommit()` — `indra/newview/alpanellensgaze.cpp:595` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onLeanSoftnessCommit()` — `indra/newview/alpanellensgaze.cpp:594` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onLeanThresholdCommit()` — `indra/newview/alpanellensgaze.cpp:593` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onLimitModeCommit()` — `indra/newview/alpanellensgaze.cpp:572` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onLimitProfileCommit()` — `indra/newview/alpanellensgaze.cpp:573, 574, 575, 576, 577, 578, 579, 580, 581, 582, 583, 584, 585, 586, 587, 588, 589, 590, 591` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onMicroLifeCommit()` — `indra/newview/alpanellensgaze.cpp:607` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onModeCommit()` — `indra/newview/alpanellensgaze.cpp:616` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onPerformanceCommit()` — `indra/newview/alpanellensgaze.cpp:600` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onPersonaCommit()` — `indra/newview/alpanellensgaze.cpp:601, 602, 603, 638, 644, 650` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onPickObject()` — `indra/newview/alpanellensgaze.cpp:562` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onPriorityCommit()` — `indra/newview/alpanellensgaze.cpp:554` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onRangePresetCommit()` — `indra/newview/alpanellensgaze.cpp:556` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onResetPerformance()` — `indra/newview/alpanellensgaze.cpp:636` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onSetPoint()` — `indra/newview/alpanellensgaze.cpp:561` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onSlotLookAtCommit()` — `indra/newview/alpanellensgaze.cpp:545, 546, 547, 548, 549` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onSmoothingCommit()` — `indra/newview/alpanellensgaze.cpp:596, 674` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onTargetCommit()` — `indra/newview/alpanellensgaze.cpp:552, 624` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onTargetDetailScopeCommit()` — `indra/newview/alpanellensgaze.cpp:558` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onTorsoCommit()` — `indra/newview/alpanellensgaze.cpp:565, 662` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::onVariationCommit()` — `indra/newview/alpanellensgaze.cpp:609` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::refreshEditActorCombo()` — `indra/newview/alpanellensgaze.cpp:452` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelLensGaze::setHotGazeSlot()` — `indra/newview/alpanellensgaze.cpp:2428, 2438` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLActorMover::hotGazeInput()` — `indra/newview/alpanellensgaze.cpp:2452` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLDirectorCast::setHotGazeSlot()` — `indra/newview/alpanellensgaze.cpp:2444` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterReg::toggleInstance()` — `indra/newview/alpanellensgaze.cpp:618` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::onOpenActorGaze()` — `indra/newview/llfloaterdirector.cpp:1669` | Move model-facing work into `ALGazeKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **127 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-gaze_subjects-tallest-850` | Only `gaze_subjects` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-gaze_subjects-tallest-1160` | Only `gaze_subjects` is put in its tallest local state at 1160 px; all other cards stay default. |
| `gaze_apply_to-option-0` | Only `gaze_apply_to` selects `Actor`; other selectors remain default. |
| `gaze_apply_to-option-1` | Only `gaze_apply_to` selects `Group`; other selectors remain default. |
| `gaze_slot_you-on` | Only `gaze_slot_you` is on; dependent visibility is measured. |
| `gaze_slot_a-on` | Only `gaze_slot_a` is on; dependent visibility is measured. |
| `gaze_slot_b-on` | Only `gaze_slot_b` is on; dependent visibility is measured. |
| `gaze_slot_c-on` | Only `gaze_slot_c` is on; dependent visibility is measured. |
| `gaze_slot_d-on` | Only `gaze_slot_d` is on; dependent visibility is measured. |
| `card-hot_targets-tallest-850` | Only `hot_targets` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-hot_targets-tallest-1160` | Only `hot_targets` is put in its tallest local state at 1160 px; all other cards stay default. |
| `hot_slot-option-0` | Only `hot_slot` selects `Slot 1`; other selectors remain default. |
| `hot_slot-option-1` | Only `hot_slot` selects `Slot 2`; other selectors remain default. |
| `hot_slot-option-2` | Only `hot_slot` selects `Slot 3`; other selectors remain default. |
| `hot_slot-option-3` | Only `hot_slot` selects `Slot 4`; other selectors remain default. |
| `hot_target-option-0` | Only `hot_target` selects `Choose target`; other selectors remain default. |
| `hot_target-option-1` | Only `hot_target` selects `Camera`; other selectors remain default. |
| `hot_target-option-2` | Only `hot_target` selects `Actor`; other selectors remain default. |
| `hot_target-option-3` | Only `hot_target` selects `Point`; other selectors remain default. |
| `hot_target-option-4` | Only `hot_target` selects `Object`; other selectors remain default. |
| `hot_spot-option-0` | Only `hot_spot` selects `Centre`; other selectors remain default. |
| `hot_spot-option-1` | Only `hot_spot` selects `Picked spot`; other selectors remain default. |
| `hot_after-on` | Only `hot_after` is on; dependent visibility is measured. |
| `hot_mode-option-0` | Only `hot_mode` selects `Tap`; other selectors remain default. |
| `hot_mode-option-1` | Only `hot_mode` selects `Hold`; other selectors remain default. |
| `hot_gamepad-on` | Only `hot_gamepad` is on; dependent visibility is measured. |
| `card-targets-tallest-850` | Only `targets` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-targets-tallest-1160` | Only `targets` is put in its tallest local state at 1160 px; all other cards stay default. |
| `gaze_target_combo-option-0` | Only `gaze_target_combo` selects `Where it's going`; other selectors remain default. |
| `gaze_target_combo-option-1` | Only `gaze_target_combo` selects `Camera`; other selectors remain default. |
| `gaze_target_combo-option-2` | Only `gaze_target_combo` selects `Cast member`; other selectors remain default. |
| `gaze_target_combo-option-3` | Only `gaze_target_combo` selects `Fixed point`; other selectors remain default. |
| `gaze_target_combo-option-4` | Only `gaze_target_combo` selects `Object`; other selectors remain default. |
| `gaze_eye_target_combo-option-0` | Only `gaze_eye_target_combo` selects `Follow head`; other selectors remain default. |
| `gaze_eye_target_combo-option-1` | Only `gaze_eye_target_combo` selects `Relaxed (eyes idle)`; other selectors remain default. |
| `gaze_eye_target_combo-option-2` | Only `gaze_eye_target_combo` selects `Near-lens (just off camera)`; other selectors remain default. |
| `gaze_eye_target_combo-option-3` | Only `gaze_eye_target_combo` selects `Camera`; other selectors remain default. |
| `gaze_eye_target_combo-option-4` | Only `gaze_eye_target_combo` selects `Cast member`; other selectors remain default. |
| `gaze_eye_target_combo-option-5` | Only `gaze_eye_target_combo` selects `Fixed point`; other selectors remain default. |
| `gaze_eye_target_combo-option-6` | Only `gaze_eye_target_combo` selects `Object`; other selectors remain default. |
| `gaze_target_detail_scope-option-0` | Only `gaze_target_detail_scope` selects `Head`; other selectors remain default. |
| `gaze_target_detail_scope-option-1` | Only `gaze_target_detail_scope` selects `Eyes`; other selectors remain default. |
| `gaze_cast_combo-option-0` | Only `gaze_cast_combo` selects `Selected cast`; other selectors remain default. |
| `gaze_cast_combo-option-1` | Only `gaze_cast_combo` selects `Mara`; other selectors remain default. |
| `gaze_cast_combo-option-2` | Only `gaze_cast_combo` selects `Jun`; other selectors remain default. |
| `gaze_cast_combo-option-3` | Only `gaze_cast_combo` selects `Me`; other selectors remain default. |
| `card-strength-tallest-850` | Only `strength` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-strength-tallest-1160` | Only `strength` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-presets_blend-tallest-850` | Only `presets_blend` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-presets_blend-tallest-1160` | Only `presets_blend` is put in its tallest local state at 1160 px; all other cards stay default. |
| `gaze_performance_combo-option-0` | Only `gaze_performance_combo` selects `Custom`; other selectors remain default. |
| `gaze_easing_combo-option-0` | Only `gaze_easing_combo` selects `Linear`; other selectors remain default. |
| `gaze_easing_combo-option-1` | Only `gaze_easing_combo` selects `Smoothstep`; other selectors remain default. |
| `gaze_easing_combo-option-2` | Only `gaze_easing_combo` selects `Ease-In-Out`; other selectors remain default. |
| `gaze_easing_combo-option-3` | Only `gaze_easing_combo` selects `Ease-Out`; other selectors remain default. |
| `gaze_random_check-on` | Only `gaze_random_check` is on; dependent visibility is measured. |
| `card-gaze_windows-tallest-850` | Only `gaze_windows` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-gaze_windows-tallest-1160` | Only `gaze_windows` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-camera_lookat-tallest-850` | Only `camera_lookat` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-camera_lookat-tallest-1160` | Only `camera_lookat` is put in its tallest local state at 1160 px; all other cards stay default. |
| `look_at_camera_mode-option-0` | Only `look_at_camera_mode` selects `Gaze (head/eyes)`; other selectors remain default. |
| `look_at_camera_mode-option-1` | Only `look_at_camera_mode` selects `Turn body`; other selectors remain default. |
| `look_at_camera_torso-on` | Only `look_at_camera_torso` is on; dependent visibility is measured. |
| `card-aim_eyeline-tallest-850` | Only `aim_eyeline` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-aim_eyeline-tallest-1160` | Only `aim_eyeline` is put in its tallest local state at 1160 px; all other cards stay default. |
| `gaze_breakoff_check-on` | Only `gaze_breakoff_check` is on; dependent visibility is measured. |
| `card-lock_timing-tallest-850` | Only `lock_timing` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-lock_timing-tallest-1160` | Only `lock_timing` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-feel-tallest-850` | Only `feel` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-feel-tallest-1160` | Only `feel` is put in its tallest local state at 1160 px; all other cards stay default. |
| `gaze_priority_combo-option-0` | Only `gaze_priority_combo` selects `Legacy blend`; other selectors remain default. |
| `gaze_priority_combo-option-1` | Only `gaze_priority_combo` selects `Head+Eyes`; other selectors remain default. |
| `gaze_priority_combo-option-2` | Only `gaze_priority_combo` selects `Upper body`; other selectors remain default. |
| `gaze_priority_combo-option-3` | Only `gaze_priority_combo` selects `Planted spine (IK)`; other selectors remain default. |
| `gaze_ik_lean_combo-option-0` | Only `gaze_ik_lean_combo` selects `Choose lean...`; other selectors remain default. |
| `gaze_ik_lean_combo-option-1` | Only `gaze_ik_lean_combo` selects `Off`; other selectors remain default. |
| `gaze_ik_lean_combo-option-2` | Only `gaze_ik_lean_combo` selects `Subtle`; other selectors remain default. |
| `gaze_ik_lean_combo-option-3` | Only `gaze_ik_lean_combo` selects `Natural`; other selectors remain default. |
| `gaze_ik_lean_combo-option-4` | Only `gaze_ik_lean_combo` selects `Responsive`; other selectors remain default. |
| `gaze_ik_lean_combo-option-5` | Only `gaze_ik_lean_combo` selects `Expressive`; other selectors remain default. |
| `gaze_ik_lean_combo-option-6` | Only `gaze_ik_lean_combo` selects `Hero`; other selectors remain default. |
| `gaze_ik_lean_combo-option-7` | Only `gaze_ik_lean_combo` selects `Lean-in`; other selectors remain default. |
| `gaze_ik_lean_combo-option-8` | Only `gaze_ik_lean_combo` selects `Snap`; other selectors remain default. |
| `gaze_ik_lean_combo-option-9` | Only `gaze_ik_lean_combo` selects `Deep bow`; other selectors remain default. |
| `gaze_anim_priority_combo-option-0` | Only `gaze_anim_priority_combo` selects `Legacy final (always apply)`; other selectors remain default. |
| `gaze_anim_priority_combo-option-1` | Only `gaze_anim_priority_combo` selects `0 (Low)`; other selectors remain default. |
| `gaze_anim_priority_combo-option-2` | Only `gaze_anim_priority_combo` selects `1`; other selectors remain default. |
| `gaze_anim_priority_combo-option-3` | Only `gaze_anim_priority_combo` selects `2`; other selectors remain default. |
| `gaze_anim_priority_combo-option-4` | Only `gaze_anim_priority_combo` selects `3`; other selectors remain default. |
| `gaze_anim_priority_combo-option-5` | Only `gaze_anim_priority_combo` selects `4 (Highest named)`; other selectors remain default. |
| `gaze_anim_priority_combo-option-6` | Only `gaze_anim_priority_combo` selects `5`; other selectors remain default. |
| `gaze_anim_priority_combo-option-7` | Only `gaze_anim_priority_combo` selects `6 (Maximum normal)`; other selectors remain default. |
| `gaze_composition_combo-option-0` | Only `gaze_composition_combo` selects `Replace (current)`; other selectors remain default. |
| `gaze_composition_combo-option-1` | Only `gaze_composition_combo` selects `Additive overlay`; other selectors remain default. |
| `gaze_composition_combo-option-2` | Only `gaze_composition_combo` selects `Blend replace/overlay`; other selectors remain default. |
| `card-rotation_range-tallest-850` | Only `rotation_range` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-rotation_range-tallest-1160` | Only `rotation_range` is put in its tallest local state at 1160 px; all other cards stay default. |
| `gaze_range_preset_combo-option-0` | Only `gaze_range_preset_combo` selects `Choose range...`; other selectors remain default. |
| `gaze_range_preset_combo-option-1` | Only `gaze_range_preset_combo` selects `Stock`; other selectors remain default. |
| `gaze_range_preset_combo-option-2` | Only `gaze_range_preset_combo` selects `Tight`; other selectors remain default. |
| `gaze_range_preset_combo-option-3` | Only `gaze_range_preset_combo` selects `Natural`; other selectors remain default. |
| `gaze_range_preset_combo-option-4` | Only `gaze_range_preset_combo` selects `Wide`; other selectors remain default. |
| `gaze_range_preset_combo-option-5` | Only `gaze_range_preset_combo` selects `Extreme`; other selectors remain default. |
| `gaze_range_preset_combo-option-6` | Only `gaze_range_preset_combo` selects `Owl/Creature`; other selectors remain default. |
| `gaze_range_preset_combo-option-7` | Only `gaze_range_preset_combo` selects `Up-favored`; other selectors remain default. |
| `gaze_limit_mode_combo-option-0` | Only `gaze_limit_mode_combo` selects `Inherit`; other selectors remain default. |
| `gaze_limit_mode_combo-option-1` | Only `gaze_limit_mode_combo` selects `Legacy constants`; other selectors remain default. |
| `gaze_limit_mode_combo-option-2` | Only `gaze_limit_mode_combo` selects `Custom`; other selectors remain default. |
| `card-lean_curve-tallest-850` | Only `lean_curve` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-lean_curve-tallest-1160` | Only `lean_curve` is put in its tallest local state at 1160 px; all other cards stay default. |
| `gaze_lean_curve_combo-option-0` | Only `gaze_lean_curve_combo` selects `Legacy fill`; other selectors remain default. |
| `gaze_lean_curve_combo-option-1` | Only `gaze_lean_curve_combo` selects `Angle ease`; other selectors remain default. |
| `card-joint_limits-tallest-850` | Only `joint_limits` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-joint_limits-tallest-1160` | Only `joint_limits` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-persona-tallest-850` | Only `persona` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-persona-tallest-1160` | Only `persona` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-life_cadence-tallest-850` | Only `life_cadence` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-life_cadence-tallest-1160` | Only `life_cadence` is put in its tallest local state at 1160 px; all other cards stay default. |
| `gaze_blinks_check-on` | Only `gaze_blinks_check` is on; dependent visibility is measured. |
| `actor-none` | Rail has no actor; target-dependent rows are disabled with an explicit empty state. |
| `actor-selected` | One rail actor is selected and unpinned. |
| `actor-pinned-rail-changes` | Page target is pinned while the rail changes to another actor. |
| `actor-removed` | The active actor disappears while the page is visible. |
| `selection-deleted` | The selected model row is removed by another owner while visible. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## Ambiguous same-name candidates (never auto-bound)

None.

## Rejected out-of-scope same-name candidates

152 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

## New widget required

None. Every approved visual kind is either a kit primitive or an existing XUI control. An unresolved binding is not permission to invent a visual class.

## Unresolved and pending-decision items

- `hot_who` (Who): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `hot_target` (Target): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `hot_actor` (Target actor): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `hot_spot` (Object aim): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `hot_x` (Nudge X (m)): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `hot_y` (Nudge Y (m)): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `hot_z` (Nudge Z (m)): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `hot_snap` (Snap (s)): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `hot_head` (Head): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `hot_eyes` (Eyes): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `hot_torso` (Torso): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `hot_after` (Return after dwell): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `hot_dwell` (Dwell (s)): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `hot_return` (Return (s)): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `hot_mode` (Key mode): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.

## Risks

- A widget-name collision can select an unrelated XUI root or C++ literal; ownership must be resolved by root, class, and behavior together.
- Extracting model work without rewiring the existing owner creates divergent v1/v2 behavior; the extraction and forwarding call are one reviewed change.
- Duplicate writable settings must obey the rollout homes table; this page emits a link/read-only row when it is not the chosen home.
- Shared edit models require owner tokens, and external presets/cue playback require one global history barrier at start/stop rather than per write.
- Unresolved rows fail generation closed; they are never silently replaced with a guessed widget or command.
