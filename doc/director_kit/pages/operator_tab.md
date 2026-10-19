# Operator — Director Kit page contract

Page key: `operator_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (68/69 setting-bound rows; 1 C++-referenced rows).

## Real owners

- `panel_cinecam_operator` / `panel` at `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:9` (67 matching rows).
- `panel_cinecam_params` / `panel` at `indra/newview/skins/default/xui/en/panel_cinecam_params.xml:10` (2 matching rows).
- Referencing classes: `ALPanelCineCamParams` (2).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALCameraOperatorKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `shake_look` / `shake_preset_combo` (Look) | `combo_box` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:34` | `ALCameraOperatorKitAdapter` command/model binding → wired in `ALPanelCineCamParams::postBuild()`; behavior `ALPanelCineCamParams::onShakePresetSelected()` (`indra/newview/alpanelcinecamparams.cpp:495`) | none |
| `shake_look` / `shake_style` (Rig) | `combo_box` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:68` | kit `combo` ↔ `FlycamOperatorStyle` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:27` `shake_style` |
| `shake_look` / `shake_locomotion_mode` (Movement) | `combo_box` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:50` | kit `combo` ↔ `FlycamOperatorLocomotionMode` `S32` | `indra/newview/skins/default/xui/en/panel_cinecam_params.xml:155` `FlycamOperatorLocomotionMode`, `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:42` `shake_locomotion_mode` |
| `shake_look` / `shake_profile` (Feel) | `combo_box` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:86` | kit `combo` ↔ `FlycamOperatorProfile` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:56` `shake_profile` |
| `shake_look` / `FlycamOperatorEnabled` (Live flycam) | `check_box` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:108` | kit `toggle` ↔ `FlycamOperatorEnabled` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:70` `FlycamOperatorEnabled` |
| `shake_look` / `FlycamOperatorForceWalk` (Force gait) | `check_box` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:116` | kit `toggle` ↔ `FlycamOperatorForceWalk` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:73` `FlycamOperatorForceWalk` |
| `shake_look` / `FlycamOperatorModeBlendTime` (Blend (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_params.xml:192` | kit `number` ↔ `FlycamOperatorModeBlendTime` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:76` `FlycamOperatorModeBlendTime` |
| `shake_look` / `FlycamOperatorSimulationHz` (Update Hz) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_params.xml:199` | kit `number` ↔ `FlycamOperatorSimulationHz` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:79` `FlycamOperatorSimulationHz` |
| `motion_amounts` / `FlycamOperatorMaster` (Master) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:224` | kit `number` ↔ `FlycamOperatorMaster` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:85` `FlycamOperatorMaster` |
| `motion_amounts` / `FlycamOperatorGainSurge` (Forward/back) | `slider` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:128` | kit `slider` ↔ `FlycamOperatorGainSurge` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:88` `FlycamOperatorGainSurge` |
| `motion_amounts` / `FlycamOperatorGainSway` (Left/right) | `slider` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:136` | kit `slider` ↔ `FlycamOperatorGainSway` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:93` `FlycamOperatorGainSway` |
| `motion_amounts` / `FlycamOperatorGainHeave` (Up/down) | `slider` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:144` | kit `slider` ↔ `FlycamOperatorGainHeave` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:98` `FlycamOperatorGainHeave` |
| `motion_amounts` / `FlycamOperatorGainRoll` (Horizon tilt) | `slider` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:155` | kit `slider` ↔ `FlycamOperatorGainRoll` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:103` `FlycamOperatorGainRoll` |
| `motion_amounts` / `FlycamOperatorGainPitch` (Look up/down) | `slider` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:163` | kit `slider` ↔ `FlycamOperatorGainPitch` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:108` `FlycamOperatorGainPitch` |
| `motion_amounts` / `FlycamOperatorGainYaw` (Pan left/right) | `slider` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:171` | kit `slider` ↔ `FlycamOperatorGainYaw` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:113` `FlycamOperatorGainYaw` |
| `motion_amounts` / `FlycamOperatorGainFOV` (Lens breathing) | `slider` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:182` | kit `slider` ↔ `FlycamOperatorGainFOV` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:118` `FlycamOperatorGainFOV` |
| `response` / `CinematicCamSmoothing` (Glide) | `slider` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:193` | kit `slider` ↔ `CinematicCamSmoothing` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:125` `CinematicCamSmoothing` |
| `response` / `FlycamOperatorReactivity` (Reactivity) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:229` | kit `number` ↔ `FlycamOperatorReactivity` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:130` `FlycamOperatorReactivity` |
| `response` / `FlycamOperatorReactionLag` (Reaction lag) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:285` | kit `number` ↔ `FlycamOperatorReactionLag` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:133` `FlycamOperatorReactionLag` |
| `response` / `CinematicCamFrameOffsetUp` (Frame up) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:201` | kit `number` ↔ `CinematicCamFrameOffsetUp` `F32` | page `frame_orbit_tab`, `indra/newview/skins/default/xui/en/panel_cinecam_frame.xml:136` `auto_frame_vertical_trim`, `indra/newview/skins/default/xui/en/panel_dpage_camera_frame_orbit.xml:74` `auto_frame_vertical_trim` |
| `motion_response` / `FlycamOperatorMotionPan` (Motion pan) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:277` | kit `number` ↔ `FlycamOperatorMotionPan` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:139` `FlycamOperatorMotionPan` |
| `motion_response` / `FlycamOperatorMotionTilt` (Motion tilt) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:279` | kit `number` ↔ `FlycamOperatorMotionTilt` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:142` `FlycamOperatorMotionTilt` |
| `motion_response` / `FlycamOperatorMotionRoll` (Motion roll) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:281` | kit `number` ↔ `FlycamOperatorMotionRoll` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:145` `FlycamOperatorMotionRoll` |
| `motion_response` / `FlycamOperatorMotionCalm` (Motion calm) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:275` | kit `number` ↔ `FlycamOperatorMotionCalm` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:148` `FlycamOperatorMotionCalm` |
| `motion_response` / `FlycamOperatorProfileInfluence` (Profile mix) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:234` | kit `number` ↔ `FlycamOperatorProfileInfluence` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:151` `FlycamOperatorProfileInfluence` |
| `motion_response` / `FlycamOperatorEnergyTrim` (Energy) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:254` | kit `number` ↔ `FlycamOperatorEnergyTrim` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:154` `FlycamOperatorEnergyTrim` |
| `settle` / `FlycamOperatorSettle` (Settle) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:292` | kit `number` ↔ `FlycamOperatorSettle` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:159` `FlycamOperatorSettle` |
| `settle` / `FlycamOperatorSettleFreq` (Bounce Hz) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:294` | kit `number` ↔ `FlycamOperatorSettleFreq` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:162` `FlycamOperatorSettleFreq` |
| `settle` / `FlycamOperatorOnset` (Onset) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:259` | kit `number` ↔ `FlycamOperatorOnset` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:165` `FlycamOperatorOnset` |
| `settle` / `FlycamOperatorSettleDecay` (Settle decay) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:296` | kit `number` ↔ `FlycamOperatorSettleDecay` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:168` `FlycamOperatorSettleDecay` |
| `reframe` / `FlycamOperatorRecomposeAmount` (Recompose) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:383` | kit `number` ↔ `FlycamOperatorRecomposeAmount` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:173` `FlycamOperatorRecomposeAmount` |
| `reframe` / `FlycamOperatorRecomposeInterval` (Recomp sec) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:385` | kit `number` ↔ `FlycamOperatorRecomposeInterval` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:176` `FlycamOperatorRecomposeInterval` |
| `reframe` / `FlycamOperatorDrag` (Drag) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:264` | kit `number` ↔ `FlycamOperatorDrag` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:179` `FlycamOperatorDrag` |
| `hand` / `FlycamOperatorTremorDamping` (Steady) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:290` | kit `number` ↔ `FlycamOperatorTremorDamping` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:184` `FlycamOperatorTremorDamping` |
| `hand` / `FlycamOperatorIdleIntensity` (Idle wander) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:249` | kit `number` ↔ `FlycamOperatorIdleIntensity` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:187` `FlycamOperatorIdleIntensity` |
| `hand` / `FlycamOperatorTimeSpeed` (Time speed) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:239` | kit `number` ↔ `FlycamOperatorTimeSpeed` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:190` `FlycamOperatorTimeSpeed` |
| `hand` / `FlycamOperatorSmoothing` (Smoothing) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:244` | kit `number` ↔ `FlycamOperatorSmoothing` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:193` `FlycamOperatorSmoothing` |
| `hand` / `FlycamOperatorSeed` (Seed) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:269` | kit `number` ↔ `FlycamOperatorSeed` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:196` `FlycamOperatorSeed` |
| `pan_wander` / `FlycamOperatorPanMode` (Pan style) | `combo_box` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:301` | kit `combo` ↔ `FlycamOperatorPanMode` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:201` `FlycamOperatorPanMode` |
| `pan_wander` / `FlycamOperatorPanAmount` (Pan amount) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:307` | kit `number` ↔ `FlycamOperatorPanAmount` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:210` `FlycamOperatorPanAmount` |
| `pan_wander` / `FlycamOperatorPanSmoothness` (Pan smooth) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:309` | kit `number` ↔ `FlycamOperatorPanSmoothness` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:213` `FlycamOperatorPanSmoothness` |
| `pan_wander` / `FlycamOperatorPanReactSmoothing` (Pan react) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:311` | kit `number` ↔ `FlycamOperatorPanReactSmoothing` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:216` `FlycamOperatorPanReactSmoothing` |
| `pan_wander` / `FlycamOperatorPanTiltFreq` (Pan/tilt Hz) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:313` | kit `number` ↔ `FlycamOperatorPanTiltFreq` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:219` `FlycamOperatorPanTiltFreq` |
| `tilt_roll` / `FlycamOperatorTiltAmount` (Tilt amount) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:315` | kit `number` ↔ `FlycamOperatorTiltAmount` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:224` `FlycamOperatorTiltAmount` |
| `tilt_roll` / `FlycamOperatorRollAmount` (Roll amount) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:320` | kit `number` ↔ `FlycamOperatorRollAmount` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:227` `FlycamOperatorRollAmount` |
| `tilt_roll` / `FlycamOperatorStepRoll` (Step roll) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:326` | kit `number` ↔ `FlycamOperatorStepRoll` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:230` `FlycamOperatorStepRoll` |
| `tilt_roll` / `FlycamOperatorRollFreq` (Roll Hz) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:322` | kit `number` ↔ `FlycamOperatorRollFreq` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:233` `FlycamOperatorRollFreq` |
| `tilt_roll` / `FlycamOperatorRollDamping` (Roll damp) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:324` | kit `number` ↔ `FlycamOperatorRollDamping` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:236` `FlycamOperatorRollDamping` |
| `breathing` / `FlycamOperatorBreathAmount` (Breath %) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:338` | kit `number` ↔ `FlycamOperatorBreathAmount` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:241` `FlycamOperatorBreathAmount` |
| `breathing` / `FlycamOperatorBreathFreq` (Breath Hz) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:340` | kit `number` ↔ `FlycamOperatorBreathFreq` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:244` `FlycamOperatorBreathFreq` |
| `breathing` / `FlycamOperatorBreathLift` (Breath lift) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:342` | kit `number` ↔ `FlycamOperatorBreathLift` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:247` `FlycamOperatorBreathLift` |
| `breathing` / `FlycamOperatorMotionBreath` (Motion breath) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:283` | kit `number` ↔ `FlycamOperatorMotionBreath` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:250` `FlycamOperatorMotionBreath` |
| `walk_gait` / `FlycamOperatorWalkCadence` (Cadence) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:347` | kit `number` ↔ `FlycamOperatorWalkCadence` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:255` `FlycamOperatorWalkCadence` |
| `walk_gait` / `FlycamOperatorStepBob` (Step bounce) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:359` | kit `number` ↔ `FlycamOperatorStepBob` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:258` `FlycamOperatorStepBob` |
| `walk_gait` / `FlycamOperatorGaitCoupling` (Gait strength) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:357` | kit `number` ↔ `FlycamOperatorGaitCoupling` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:261` `FlycamOperatorGaitCoupling` |
| `walk_gait` / `FlycamOperatorLateralStep` (Step sway) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:355` | kit `number` ↔ `FlycamOperatorLateralStep` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:264` `FlycamOperatorLateralStep` |
| `walk_gait` / `FlycamOperatorForwardWalkBias` (Forward gait) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:351` | kit `number` ↔ `FlycamOperatorForwardWalkBias` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:267` `FlycamOperatorForwardWalkBias` |
| `walk_gait` / `FlycamOperatorLateralWalkBias` (Lateral gait) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:353` | kit `number` ↔ `FlycamOperatorLateralWalkBias` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:270` `FlycamOperatorLateralWalkBias` |
| `walk_gait` / `FlycamOperatorCadenceDrive` (Speed drive) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:349` | kit `number` ↔ `FlycamOperatorCadenceDrive` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:273` `FlycamOperatorCadenceDrive` |
| `auto_walk` / `FlycamOperatorAutoWalkEnter` (Walk above) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:364` | kit `number` ↔ `FlycamOperatorAutoWalkEnter` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:278` `FlycamOperatorAutoWalkEnter` |
| `auto_walk` / `FlycamOperatorAutoRunEnter` (Run above) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:372` | kit `number` ↔ `FlycamOperatorAutoRunEnter` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:281` `FlycamOperatorAutoRunEnter` |
| `auto_walk` / `FlycamOperatorAutoWalkExit` (Walk below) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:366` | kit `number` ↔ `FlycamOperatorAutoWalkExit` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:284` `FlycamOperatorAutoWalkExit` |
| `auto_walk` / `FlycamOperatorAutoRunExit` (Run below) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:374` | kit `number` ↔ `FlycamOperatorAutoRunExit` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:287` `FlycamOperatorAutoRunExit` |
| `auto_walk` / `FlycamOperatorRefLinearSpeed` (Ref linear) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:331` | kit `number` ↔ `FlycamOperatorRefLinearSpeed` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:290` `FlycamOperatorRefLinearSpeed` |
| `auto_walk` / `FlycamOperatorRefAngularSpeed` (Ref angular) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:333` | kit `number` ↔ `FlycamOperatorRefAngularSpeed` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:293` `FlycamOperatorRefAngularSpeed` |
| `auto_walk` / `FlycamOperatorAutoWalkEnterDwell` (Walk in sec) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:368` | kit `number` ↔ `FlycamOperatorAutoWalkEnterDwell` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:296` `FlycamOperatorAutoWalkEnterDwell` |
| `auto_walk` / `FlycamOperatorAutoWalkExitDwell` (Walk out sec) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:370` | kit `number` ↔ `FlycamOperatorAutoWalkExitDwell` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:299` `FlycamOperatorAutoWalkExitDwell` |
| `auto_walk` / `FlycamOperatorAutoRunEnterDwell` (Run in sec) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:376` | kit `number` ↔ `FlycamOperatorAutoRunEnterDwell` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:302` `FlycamOperatorAutoRunEnterDwell` |
| `auto_walk` / `FlycamOperatorAutoRunExitDwell` (Run out sec) | `spinner` `indra/newview/skins/default/xui/en/panel_cinecam_operator.xml:378` | kit `number` ↔ `FlycamOperatorAutoRunExitDwell` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_operator.xml:305` `FlycamOperatorAutoRunExitDwell` |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALPanelCineCamParams::onShakePresetSelected()` — `indra/newview/alpanelcinecamparams.cpp:495` | Move model-facing work into `ALCameraOperatorKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **66 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-shake_look-tallest-850` | Only `shake_look` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-shake_look-tallest-1160` | Only `shake_look` is put in its tallest local state at 1160 px; all other cards stay default. |
| `shake_preset_combo-option-0` | Only `shake_preset_combo` selects `Locked-Off`; other selectors remain default. |
| `shake_preset_combo-option-1` | Only `shake_preset_combo` selects `Tripod Drift`; other selectors remain default. |
| `shake_preset_combo-option-2` | Only `shake_preset_combo` selects `Doc Handheld`; other selectors remain default. |
| `shake_preset_combo-option-3` | Only `shake_preset_combo` selects `Run-and-Gun`; other selectors remain default. |
| `shake_preset_combo-option-4` | Only `shake_preset_combo` selects `Steadicam Float`; other selectors remain default. |
| `shake_preset_combo-option-5` | Only `shake_preset_combo` selects `Shoulder Rig`; other selectors remain default. |
| `shake_preset_combo-option-6` | Only `shake_preset_combo` selects `Vehicle / Drive`; other selectors remain default. |
| `shake_preset_combo-option-7` | Only `shake_preset_combo` selects `Verite`; other selectors remain default. |
| `shake_preset_combo-option-8` | Only `shake_preset_combo` selects `Heartbeat`; other selectors remain default. |
| `shake_style-option-0` | Only `shake_style` selects `Rig: Custom`; other selectors remain default. |
| `shake_style-option-1` | Only `shake_style` selects `Rig: Tripod / sticks`; other selectors remain default. |
| `shake_style-option-2` | Only `shake_style` selects `Rig: Subtle handheld`; other selectors remain default. |
| `shake_style-option-3` | Only `shake_style` selects `Rig: Documentary`; other selectors remain default. |
| `shake_style-option-4` | Only `shake_style` selects `Rig: Run-and-gun`; other selectors remain default. |
| `shake_style-option-5` | Only `shake_style` selects `Rig: Shoulder`; other selectors remain default. |
| `shake_style-option-6` | Only `shake_style` selects `Rig: Gimbal float`; other selectors remain default. |
| `shake_style-option-7` | Only `shake_style` selects `Rig: Verite`; other selectors remain default. |
| `shake_style-option-8` | Only `shake_style` selects `Rig: Breathing only`; other selectors remain default. |
| `shake_locomotion_mode-option-0` | Only `shake_locomotion_mode` selects `Motion: Legacy / authored`; other selectors remain default. |
| `shake_locomotion_mode-option-1` | Only `shake_locomotion_mode` selects `Motion: Creep`; other selectors remain default. |
| `shake_locomotion_mode-option-2` | Only `shake_locomotion_mode` selects `Motion: Walk`; other selectors remain default. |
| `shake_locomotion_mode-option-3` | Only `shake_locomotion_mode` selects `Motion: Run`; other selectors remain default. |
| `shake_locomotion_mode-option-4` | Only `shake_locomotion_mode` selects `Motion: Drive`; other selectors remain default. |
| `shake_locomotion_mode-option-5` | Only `shake_locomotion_mode` selects `Motion: Float`; other selectors remain default. |
| `shake_locomotion_mode-option-6` | Only `shake_locomotion_mode` selects `Motion: Unsteady`; other selectors remain default. |
| `shake_locomotion_mode-option-7` | Only `shake_locomotion_mode` selects `Motion: Auto from speed`; other selectors remain default. |
| `shake_profile-option-0` | Only `shake_profile` selects `Feel: Natural`; other selectors remain default. |
| `shake_profile-option-1` | Only `shake_profile` selects `Feel: Locked`; other selectors remain default. |
| `shake_profile-option-2` | Only `shake_profile` selects `Feel: Follow`; other selectors remain default. |
| `shake_profile-option-3` | Only `shake_profile` selects `Feel: Restless`; other selectors remain default. |
| `shake_profile-option-4` | Only `shake_profile` selects `Feel: Composed`; other selectors remain default. |
| `shake_profile-option-5` | Only `shake_profile` selects `Feel: Drift`; other selectors remain default. |
| `shake_profile-option-6` | Only `shake_profile` selects `Feel: Snap`; other selectors remain default. |
| `shake_profile-option-7` | Only `shake_profile` selects `Feel: March`; other selectors remain default. |
| `FlycamOperatorEnabled-on` | Only `FlycamOperatorEnabled` is on; dependent visibility is measured. |
| `FlycamOperatorForceWalk-on` | Only `FlycamOperatorForceWalk` is on; dependent visibility is measured. |
| `card-motion_amounts-tallest-850` | Only `motion_amounts` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-motion_amounts-tallest-1160` | Only `motion_amounts` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-response-tallest-850` | Only `response` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-response-tallest-1160` | Only `response` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-motion_response-tallest-850` | Only `motion_response` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-motion_response-tallest-1160` | Only `motion_response` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-settle-tallest-850` | Only `settle` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-settle-tallest-1160` | Only `settle` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-reframe-tallest-850` | Only `reframe` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-reframe-tallest-1160` | Only `reframe` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-hand-tallest-850` | Only `hand` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-hand-tallest-1160` | Only `hand` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-pan_wander-tallest-850` | Only `pan_wander` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-pan_wander-tallest-1160` | Only `pan_wander` is put in its tallest local state at 1160 px; all other cards stay default. |
| `FlycamOperatorPanMode-option-0` | Only `FlycamOperatorPanMode` selects `Pan: Smooth fBm`; other selectors remain default. |
| `FlycamOperatorPanMode-option-1` | Only `FlycamOperatorPanMode` selects `Pan: Weave`; other selectors remain default. |
| `FlycamOperatorPanMode-option-2` | Only `FlycamOperatorPanMode` selects `Pan: Raw fBm`; other selectors remain default. |
| `card-tilt_roll-tallest-850` | Only `tilt_roll` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-tilt_roll-tallest-1160` | Only `tilt_roll` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-breathing-tallest-850` | Only `breathing` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-breathing-tallest-1160` | Only `breathing` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-walk_gait-tallest-850` | Only `walk_gait` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-walk_gait-tallest-1160` | Only `walk_gait` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-auto_walk-tallest-850` | Only `auto_walk` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-auto_walk-tallest-1160` | Only `auto_walk` is put in its tallest local state at 1160 px; all other cards stay default. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## Ambiguous same-name candidates (never auto-bound)

None.

## Rejected out-of-scope same-name candidates

140 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

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
