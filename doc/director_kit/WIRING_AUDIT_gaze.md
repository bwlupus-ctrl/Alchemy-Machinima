# Wiring audit: Performance > Gaze kit conversion

Static audit of the Gaze page after it became a generated kit page (10/15, legacy conversion batch 2). No build, no commit, no viewer run: every row is checked against the generated XML, `settings.xml` and the C++ source. Sources: mockup page `gaze_tab` (14 cards, 123 rows, plus the two rows added below), `kit_pages.json` entry `performance_group/gaze_tab` (hosts the v1 nodes of `panel_lens_gaze.xml`, root class `panel_lens_gaze`, `actor_target`), generated `panel_dpage_performance_gaze.xml`.

## Result

- The page is no longer legacy: classification `kit (P0)`, pin 3 -> 2, `performance_group/gaze_tab` removed from `KNOWN_LEGACY_PAGES`, and its two `xui_legacy_v1_findings` baseline entries removed from `gate_policy.json` (the list shrinks).
- 125 generated rows = the 123 mockup rows + 2 added, 14 cards (plus the Look at camera card switch `look_at_camera_enabled`), 4 sections, Who chip "Editing" on `gaze_edit_actor`. 108 widget names are looked up by `ALPanelLensGaze::postBuild`; all 108 resolve to a generated row or card switch (checked by script against the generated XML).
- Mockup rows added (the v1 panel had them, the mockup did not): `gaze_performance_combo` ("Performance", the personality preset combo, filled by code with Custom + `GAZE_PRESETS`) and `reset_gaze_performance` (Advanced "Reset", `onResetPerformance`), at the top of the Presets & blend card. `director_kit_gen.py` row pin 2105 -> 2107.
- Mockup edits otherwise: card ids `subjects` and `windows` became `gaze_subjects` and `gaze_windows` (kit card names are global; Camera > Shot owns `kc_subjects`, Performance > Polish owns `kc_windows`); seven help texts reworded so they no longer use the internal words "bucket" and "allocation" (kit_lint help audit).
- The page root is `ALPanelLensGaze` itself (shared with the standalone Actor Gaze floater, whose XML is untouched). `LLFloaterDirector::bindCameraPage` (`llfloaterdirector.cpp`, the `director_gaze_panel` lookup) now takes the root with `dynamic_cast<ALPanelLensGaze*>` before the v1 lookup, then feeds it `setSelectedActors` as before; the two later feeds are unchanged.
- No C++ change in `alpanellensgaze.cpp`: every lookup name is unchanged. Rows the code hides (`setVisible` on Cast member, Set point, Pick object, Clear target, Retention, subject toggles) hide their kit row too (`ALKitRow` treats an invisible control as a hidden row).
- Functional manifest: the v1/legacy builder forced `look_at_camera_mode` to SKIP (body-turn on SELF can drive the AO). The kit-entry builder did not, so the converted row would have become RUN (the hazard guard caught it). It now keeps the same forced SKIP through `KIT_FORCED_SAFE_SKIPS` (`director_functional_manifest.py`); no `hazard_skip_run_allow.json` entry was added.
- Search keywords: every Gaze row already had `kw`; the two added rows have them.
- Resets: the v1 Reset buttons for panel-managed values are mockup rows (Advanced) and stay hosted, so they keep their handlers (override back to inherit, or the default value, then the row's commit handler). The v1 reset buttons of the setting-bound rows (`reset_gaze_transition`, `_easing`, `_random_interval`, `_stillness`, `_restraint`, `_soft_recruit`, `_breakoff_angle`, `reset_look_at_camera_*`) are replaced by the kit row reset glyph on the same setting, and are in `parity_allow`; no C++ looks them up.
- v1 names with no row (all in `parity_allow` with reasons): 36 static labels, headings and hints, and the 12 reset buttons above.

## Controls

Settings come from `settings.xml`; ranges are the hosted v1 values, equal to the mockup. "Adv" rows show behind the card's More toggle. For rows reset by a lambda the effect is described from `alpanellensgaze.cpp`.

| # | widget | kind | setting / handler | range, flags | status | Undo owner |
|---|---|---|---|---|---|---|
| | **Subjects** | | | | | n/a (not a generated row) |
| 1 | `gaze_edit_actor` | combo | `onEditActorCommit()` |  | OK | kit widget history, replay guarded by undo_context=actor |
| 2 | `gaze_slot_you` | toggle | `onSlotLookAtCommit(0)` |  | OK | kit widget history, replay guarded by undo_context=actor |
| 3 | `gaze_slot_a` | toggle | `onSlotLookAtCommit(1)` |  | OK | kit widget history, replay guarded by undo_context=actor |
| 4 | `gaze_slot_b` | toggle | `onSlotLookAtCommit(2)` |  | OK | kit widget history, replay guarded by undo_context=actor |
| 5 | `gaze_slot_c` | toggle | `onSlotLookAtCommit(3)` |  | OK | kit widget history, replay guarded by undo_context=actor |
| 6 | `gaze_slot_d` | toggle | `onSlotLookAtCommit(4)` |  | OK | kit widget history, replay guarded by undo_context=actor |
| 7 | `gaze_enable_check` | toggle | `onEnableCommit()` |  | OK | kit widget history, replay guarded by undo_context=actor |
| 8 | `gaze_status` | readout | text set by the panel refresh (state of the edited actor) | Adv | OK | n/a (readout, never recorded) |
| 9 | `gaze_base_hint` | readout | text set by the panel refresh (shared base values) | Adv | OK | n/a (readout, never recorded) |
| | **Targets** | | | | | n/a (not a generated row) |
| 10 | `gaze_target_combo` | combo | `onTargetCommit()` |  | OK | kit widget history, replay guarded by undo_context=actor |
| 11 | `gaze_eye_target_combo` | combo | `onEyeTargetCommit()` |  | OK | kit widget history, replay guarded by undo_context=actor |
| 12 | `gaze_target_detail_scope` | combo | `onTargetDetailScopeCommit()` |  | OK | kit widget history, replay guarded by undo_context=actor |
| 13 | `gaze_cast_combo` | combo | `onCastCommit()` | Adv | OK | kit widget history, replay guarded by undo_context=actor |
| 14 | `btn_gaze_setpoint` | button | `onSetPoint()` | Adv | OK | n/a (button, never recorded) |
| 15 | `btn_gaze_pick_object` | button | `onPickObject()` | Adv | OK | n/a (button, never recorded) |
| 16 | `btn_gaze_clear_object` | button | `onClearObject()` | Adv | OK | n/a (button, never recorded) |
| 17 | `reset_gaze_target` | button | lambda: head target = Camera, then `onTargetCommit` | Adv | OK | n/a (button, never recorded) |
| 18 | `reset_gaze_eye_target` | button | lambda: eyes = -1 (follow head), then `onEyeTargetCommit` | Adv | OK | n/a (button, never recorded) |
| | **Gaze strength** | | | | | n/a (not a generated row) |
| 19 | `gaze_intensity_slider` | slider | `onIntensityCommit()` | 0..1 | OK | kit widget history, replay guarded by undo_context=actor |
| 20 | `gaze_blend_slider` | slider | `onBlendCommit()` | 0..1 | OK | kit widget history, replay guarded by undo_context=actor |
| 21 | `gaze_torso_slider` | slider | `onTorsoCommit()` | 0..1 | OK | kit widget history, replay guarded by undo_context=actor |
| 22 | `gaze_smoothing_slider` | slider | `onSmoothingCommit()` | 0..1 | OK | kit widget history, replay guarded by undo_context=actor |
| 23 | `gaze_influence_slider` | slider | value read by `onInfluenceSet` (Set key) at the playhead | 0..1 | OK | kit widget history, replay guarded by undo_context=actor |
| 24 | `btn_gaze_influence_set` | button | `onInfluenceSet()` |  | OK | n/a (button, never recorded) |
| 25 | `btn_gaze_influence_delete` | button | `onInfluenceDelete()` | Adv | OK | n/a (button, never recorded) |
| 26 | `reset_gaze_intensity` | button | lambda: 1.0, then `onIntensityCommit` | Adv | OK | n/a (button, never recorded) |
| 27 | `reset_gaze_blend` | button | lambda: 0.7, then `onBlendCommit` | Adv | OK | n/a (button, never recorded) |
| 28 | `reset_gaze_torso` | button | lambda: 0.25, then `onTorsoCommit` | Adv | OK | n/a (button, never recorded) |
| 29 | `reset_gaze_smoothing` | button | lambda: 0.5, then `onSmoothingCommit` | Adv | OK | n/a (button, never recorded) |
| | **Presets & blend** | | | | | n/a (not a generated row) |
| 30 | `gaze_performance_combo` | combo | `onPerformanceCommit()` |  | OK | kit widget history, replay guarded by undo_context=actor |
| 31 | `reset_gaze_performance` | button | `onResetPerformance`: clears the preset-authored overrides, combo back to Custom | Adv | OK | n/a (button, never recorded) |
| 32 | `gaze_transition_slider` | slider | setting `DirectorGazeTransitionSec` (F32, default 1.5), kit binding and reset | 0..10 | OK | kit setting history (watch_settings: DirectorGazeTransitionSec) |
| 33 | `gaze_easing_combo` | combo | setting `DirectorGazeEasing` (U32, default 2), kit binding and reset |  | OK | kit setting history (watch_settings: DirectorGazeEasing) |
| 34 | `gaze_random_check` | toggle | setting `DirectorGazeRandomMode` (Boolean, default 0), kit binding and reset |  | OK | kit setting history (watch_settings: DirectorGazeRandomMode) |
| 35 | `gaze_random_interval_slider` | slider | setting `DirectorGazeRandomInterval` (F32, default 12), kit binding and reset | 2..60 | OK | kit setting history (watch_settings: DirectorGazeRandomInterval) |
| | **Open windows** | | | | | n/a (not a generated row) |
| 36 | `btn_actor_gaze` | button | `LLFloaterDirector::onOpenActorGaze` (`bindCameraPage`, `llfloaterdirector.cpp:2631`) |  | OK | n/a (button, never recorded) |
| 37 | `btn_pose_polish` | button | lambda: `LLFloaterReg::toggleInstance("pose_polish")` |  | OK | n/a (button, never recorded) |
| 38 | `btn_gaze_cues` | button | `onGazeCues()` |  | OK | n/a (button, never recorded) |
| | *card switch* `look_at_camera_enabled` | toggle | setting `DirectorLookAtCameraEnabled` | | OK | n/a (not a generated row) |
| | **Look at camera** | | | | | n/a (not a generated row) |
| 39 | `look_at_camera_mode` | combo | `onModeCommit()` |  | OK | kit widget history, replay guarded by undo_context=actor |
| 40 | `look_at_camera_strength` | number | setting `DirectorLookAtCameraStrength` (F32, default 1), kit binding and reset | 0..1 | OK | kit setting history (watch_settings: DirectorLookAtCameraStrength) |
| 41 | `look_at_camera_head_eye` | slider | setting `DirectorLookAtCameraHeadEye` (F32, default 1), kit binding and reset | 0..1 | OK | kit setting history (watch_settings: DirectorLookAtCameraHeadEye) |
| 42 | `look_at_camera_torso` | toggle | setting `DirectorLookAtCameraTorso` (Boolean, default 0), kit binding and reset |  | OK | kit setting history (watch_settings: DirectorLookAtCameraTorso) |
| 43 | `look_at_camera_smoothing` | slider | setting `DirectorLookAtCameraSmoothing` (F32, default 0.12), kit binding and reset | 0..1 | OK | kit setting history (watch_settings: DirectorLookAtCameraSmoothing) |
| 44 | `look_at_camera_torso_amount` | slider | setting `DirectorLookAtCameraTorsoAmount` (F32, default 0.18), kit binding and reset | 0..1 Adv | OK | kit setting history (watch_settings: DirectorLookAtCameraTorsoAmount) |
| 45 | `look_at_camera_ease_time` | slider | setting `DirectorLookAtCameraEaseTime` (F32, default 0.15), kit binding and reset | 0..1 Adv | OK | kit setting history (watch_settings: DirectorLookAtCameraEaseTime) |
| 46 | `reset_look_at_camera_mode` | button | lambda: camera-mode override = -1 (inherit) on the edited actors | Adv | OK | n/a (button, never recorded) |
| | **Aim & eyeline** | | | | | n/a (not a generated row) |
| 47 | `gaze_eyeline_yaw_slider` | slider | `onEyelineCommit()` | -15..15 | OK | kit widget history, replay guarded by undo_context=actor |
| 48 | `gaze_eyeline_pitch_slider` | slider | `onEyelineCommit()` | -10..10 | OK | kit widget history, replay guarded by undo_context=actor |
| 49 | `gaze_camera_roll_slider` | slider | `onCameraRollCommit()` | 0..1 | OK | kit widget history, replay guarded by undo_context=actor |
| 50 | `gaze_breakoff_check` | toggle | `onBreakoffCommit()` |  | OK | kit widget history, replay guarded by undo_context=actor |
| 51 | `gaze_breakoff_spinner` | number | setting `BDMergeGazeBehindAngle` (F32, default 120), kit binding and reset | 90..180 | OK | kit setting history (watch_settings: BDMergeGazeBehindAngle) |
| 52 | `reset_gaze_eyeline_yaw` | button | lambda: 0, then `onEyelineCommit` | Adv | OK | n/a (button, never recorded) |
| 53 | `reset_gaze_eyeline_pitch` | button | lambda: 0, then `onEyelineCommit` | Adv | OK | n/a (button, never recorded) |
| 54 | `reset_gaze_camera_roll` | button | lambda: camera-roll override = -1 (inherit) on the edited actors | Adv | OK | n/a (button, never recorded) |
| | **Acquire & release** | | | | | n/a (not a generated row) |
| 55 | `gaze_ease_acquire_spinner` | number | `onEaseAcquireCommit()` | 0.05..2 | OK | kit widget history, replay guarded by undo_context=actor |
| 56 | `gaze_ease_release_spinner` | number | `onEaseReleaseCommit()` | 0.05..3 | OK | kit widget history, replay guarded by undo_context=actor |
| 57 | `gaze_deadzone_spinner` | number | `onDeadZoneCommit()` | 0..15 | OK | kit widget history, replay guarded by undo_context=actor |
| 58 | `reset_gaze_ease_acquire` | button | lambda: override = -1 (inherit the global) on the edited actors | Adv | OK | n/a (button, never recorded) |
| 59 | `reset_gaze_ease_release` | button | lambda: override = -1 (inherit the global) on the edited actors | Adv | OK | n/a (button, never recorded) |
| 60 | `reset_gaze_deadzone` | button | lambda: override = -1 (inherit the global) on the edited actors | Adv | OK | n/a (button, never recorded) |
| | **Movement feel** | | | | | n/a (not a generated row) |
| 61 | `gaze_priority_combo` | combo | `onPriorityCommit()` |  | OK | kit widget history, replay guarded by undo_context=actor |
| 62 | `gaze_ik_lean_combo` | combo | `onIkLeanCommit()` |  | OK | kit widget history, replay guarded by undo_context=actor |
| 63 | `gaze_anim_priority_combo` | combo | `onAnimPriorityCommit()` |  | OK | kit widget history, replay guarded by undo_context=actor |
| 64 | `gaze_composition_combo` | combo | `onCompositionCommit()` |  | OK | kit widget history, replay guarded by undo_context=actor |
| 65 | `gaze_chest_share_slider` | slider | `onChestShareCommit()` | 0..1 | OK | kit widget history, replay guarded by undo_context=actor |
| 66 | `gaze_composition_mix_slider` | slider | `onCompositionMixCommit()` | 0..1 Adv | OK | kit widget history, replay guarded by undo_context=actor |
| 67 | `reset_gaze_priority` | button | lambda: override = -1 (inherit the global) on the edited actors | Adv | OK | n/a (button, never recorded) |
| 68 | `reset_gaze_ik_lean` | button | lambda: Lean combo = -1 ("Choose lean...") | Adv | OK | n/a (button, never recorded) |
| 69 | `reset_gaze_anim_priority` | button | lambda: override = -1 (inherit the global) on the edited actors | Adv | OK | n/a (button, never recorded) |
| 70 | `reset_gaze_composition` | button | lambda: override = -1 (inherit the global) on the edited actors | Adv | OK | n/a (button, never recorded) |
| 71 | `reset_gaze_composition_mix` | button | lambda: override = -1 (inherit the global) on the edited actors | Adv | OK | n/a (button, never recorded) |
| 72 | `reset_gaze_chest_share` | button | lambda: override = -1 (inherit the global) on the edited actors | Adv | OK | n/a (button, never recorded) |
| | **Rotation range** | | | | | n/a (not a generated row) |
| 73 | `gaze_range_preset_combo` | combo | `onRangePresetCommit()` |  | OK | kit widget history, replay guarded by undo_context=actor |
| 74 | `gaze_limit_mode_combo` | combo | `onLimitModeCommit()` |  | OK | kit widget history, replay guarded by undo_context=actor |
| 75 | `gaze_exaggerate_slider` | slider | `onExaggerateCommit()` | 1..3 | OK | kit widget history, replay guarded by undo_context=actor |
| 76 | `reset_gaze_range_preset` | button | lambda: Range combo = -1 ("Choose range...") | Adv | OK | n/a (button, never recorded) |
| 77 | `reset_gaze_limit_mode` | button | lambda: override = -1 (inherit the global) on the edited actors | Adv | OK | n/a (button, never recorded) |
| 78 | `reset_gaze_exaggerate` | button | lambda: override = -1 (inherit the global) on the edited actors | Adv | OK | n/a (button, never recorded) |
| | **Lean curve** | | | | | n/a (not a generated row) |
| 79 | `gaze_lean_curve_combo` | combo | `onLeanCurveCommit()` |  | OK | kit widget history, replay guarded by undo_context=actor |
| 80 | `gaze_lean_threshold_spinner` | number | `onLeanThresholdCommit()` | 0..180 | OK | kit widget history, replay guarded by undo_context=actor |
| 81 | `gaze_lean_softness_spinner` | number | `onLeanSoftnessCommit()` | 0..90 | OK | kit widget history, replay guarded by undo_context=actor |
| 82 | `gaze_lean_max_spinner` | number | `onLeanMaxCommit()` | 0..90 | OK | kit widget history, replay guarded by undo_context=actor |
| 83 | `gaze_soft_recruit_spinner` | number | setting `DirectorGazeSoftRecruitDeg` (F32, default 0), kit binding and reset | 0..20 Adv | OK | kit setting history (watch_settings: DirectorGazeSoftRecruitDeg) |
| 84 | `reset_gaze_lean_curve` | button | lambda: override = -1 (inherit the global) on the edited actors | Adv | OK | n/a (button, never recorded) |
| 85 | `reset_gaze_lean_threshold` | button | lambda: override = -1 (inherit the global) on the edited actors | Adv | OK | n/a (button, never recorded) |
| 86 | `reset_gaze_lean_softness` | button | lambda: override = -1 (inherit the global) on the edited actors | Adv | OK | n/a (button, never recorded) |
| 87 | `reset_gaze_lean_max` | button | lambda: override = -1 (inherit the global) on the edited actors | Adv | OK | n/a (button, never recorded) |
| | **Joint limits** | | | | | n/a (not a generated row) |
| 88 | `gaze_limit_eye_yaw_spinner` | number | `onLimitProfileCommit()` | 0..180 | OK | kit widget history, replay guarded by undo_context=actor |
| 89 | `gaze_limit_head_yaw_spinner` | number | `onLimitProfileCommit()` | 0..180 | OK | kit widget history, replay guarded by undo_context=actor |
| 90 | `gaze_limit_neck_yaw_spinner` | number | `onLimitProfileCommit()` | 0..180 | OK | kit widget history, replay guarded by undo_context=actor |
| 91 | `gaze_limit_spine_yaw_spinner` | number | `onLimitProfileCommit()` | 0..180 | OK | kit widget history, replay guarded by undo_context=actor |
| 92 | `gaze_limit_hips_yaw_spinner` | number | `onLimitProfileCommit()` | 0..180 | OK | kit widget history, replay guarded by undo_context=actor |
| 93 | `gaze_limit_eye_pitch_spinner` | number | `onLimitProfileCommit()` | 0..180 Adv | OK | kit widget history, replay guarded by undo_context=actor |
| 94 | `gaze_limit_eye_pitchup_spinner` | number | `onLimitProfileCommit()` | 0..180 Adv | OK | kit widget history, replay guarded by undo_context=actor |
| 95 | `gaze_limit_head_pitch_spinner` | number | `onLimitProfileCommit()` | 0..180 Adv | OK | kit widget history, replay guarded by undo_context=actor |
| 96 | `gaze_limit_head_pitchup_spinner` | number | `onLimitProfileCommit()` | 0..180 Adv | OK | kit widget history, replay guarded by undo_context=actor |
| 97 | `gaze_limit_neck_pitch_spinner` | number | `onLimitProfileCommit()` | 0..180 Adv | OK | kit widget history, replay guarded by undo_context=actor |
| 98 | `gaze_limit_neck_pitchup_spinner` | number | `onLimitProfileCommit()` | 0..180 Adv | OK | kit widget history, replay guarded by undo_context=actor |
| 99 | `gaze_limit_spine_pitch_spinner` | number | `onLimitProfileCommit()` | 0..180 Adv | OK | kit widget history, replay guarded by undo_context=actor |
| 100 | `gaze_limit_spine_pitchup_spinner` | number | `onLimitProfileCommit()` | 0..180 Adv | OK | kit widget history, replay guarded by undo_context=actor |
| 101 | `gaze_limit_hips_pitch_spinner` | number | `onLimitProfileCommit()` | 0..180 Adv | OK | kit widget history, replay guarded by undo_context=actor |
| 102 | `gaze_limit_hips_pitchup_spinner` | number | `onLimitProfileCommit()` | 0..180 Adv | OK | kit widget history, replay guarded by undo_context=actor |
| 103 | `gaze_limit_eye_radial_spinner` | number | `onLimitProfileCommit()` | 0..90 Adv | OK | kit widget history, replay guarded by undo_context=actor |
| 104 | `gaze_limit_eyeapply_yaw_spinner` | number | `onLimitProfileCommit()` | 0..180 Adv | OK | kit widget history, replay guarded by undo_context=actor |
| 105 | `gaze_limit_eyeapply_pitch_spinner` | number | `onLimitProfileCommit()` | 0..180 Adv | OK | kit widget history, replay guarded by undo_context=actor |
| 106 | `gaze_limit_eyeapply_pitchup_spinner` | number | `onLimitProfileCommit()` | 0..180 Adv | OK | kit widget history, replay guarded by undo_context=actor |
| 107 | `gaze_limit_grid_lbl` | readout | text set by the panel refresh (profile summary) | Adv | OK | n/a (readout, never recorded) |
| 108 | `gaze_limit_grid_yaw_hdr` | readout | static column heading | Adv | OK | n/a (readout, never recorded) |
| 109 | `gaze_limit_grid_pitch_hdr` | readout | static column heading | Adv | OK | n/a (readout, never recorded) |
| 110 | `reset_gaze_limit_profile` | button | lambda: limit profile = default constants on the edited actors | Adv | OK | n/a (button, never recorded) |
| | **Persona** | | | | | n/a (not a generated row) |
| 111 | `gaze_persona_dom_slider` | slider | `onPersonaCommit()` | -1..1 | OK | kit widget history, replay guarded by undo_context=actor |
| 112 | `gaze_persona_aff_slider` | slider | `onPersonaCommit()` | -1..1 | OK | kit widget history, replay guarded by undo_context=actor |
| 113 | `gaze_persona_anx_slider` | slider | `onPersonaCommit()` | -1..1 | OK | kit widget history, replay guarded by undo_context=actor |
| 114 | `reset_gaze_persona_dom` | button | lambda: 0, then `onPersonaCommit` | Adv | OK | n/a (button, never recorded) |
| 115 | `reset_gaze_persona_aff` | button | lambda: 0, then `onPersonaCommit` | Adv | OK | n/a (button, never recorded) |
| 116 | `reset_gaze_persona_anx` | button | lambda: 0, then `onPersonaCommit` | Adv | OK | n/a (button, never recorded) |
| | **Idle life** | | | | | n/a (not a generated row) |
| 117 | `gaze_microlife_slider` | slider | `onMicroLifeCommit()` | 0..1 | OK | kit widget history, replay guarded by undo_context=actor |
| 118 | `gaze_blinks_check` | toggle | `onBlinksCommit()` |  | OK | kit widget history, replay guarded by undo_context=actor |
| 119 | `gaze_variation_slider` | slider | `onVariationCommit()` | 0..1 | OK | kit widget history, replay guarded by undo_context=actor |
| 120 | `gaze_break_freq_slider` | slider | `onBreakFreqCommit()` | 0..1 | OK | kit widget history, replay guarded by undo_context=actor |
| 121 | `gaze_stillness_slider` | slider | setting `DirectorGazeStillness` (F32, default 0), kit binding and reset | 0..1 | OK | kit setting history (watch_settings: DirectorGazeStillness) |
| 122 | `gaze_restraint_slider` | slider | setting `DirectorGazeRestraint` (F32, default 0), kit binding and reset | 0..1 | OK | kit setting history (watch_settings: DirectorGazeRestraint) |
| 123 | `reset_gaze_microlife` | button | lambda: override = -1 (inherit the global) on the edited actors | Adv | OK | n/a (button, never recorded) |
| 124 | `reset_gaze_variation` | button | lambda: override = -1 (inherit the global) on the edited actors | Adv | OK | n/a (button, never recorded) |
| 125 | `reset_gaze_break_frequency` | button | lambda: override = -1 (inherit the global) on the edited actors | Adv | OK | n/a (button, never recorded) |

(The card switch row before the Look at camera heading is `look_at_camera_enabled`, bound to `DirectorLookAtCameraEnabled`; `mMasterEnable` in the panel resolves to it.)

Notes: `look_at_camera_mode` is a code-bound combo (per-actor override via `onModeCommit`); the strength, head/eyes, torso, smoothing, torso amount and ease rows are the shared base settings `DirectorLookAtCamera*`. `gaze_edit_actor` is also the Who picker: the kit page hides the combo and offers it from the Who chip, exactly as the legacy page did.

## Checks run

`director_kit_gen.py` check and selftest, `director_page_sources.py --check`, `kit_lint.py --all`, `xui_layout_check.py` selftest and `--changed HEAD`, `director_functional_manifest.py --check`, `director_parity.py` selftest and `--groups light_group`, `zs_check.ps1`, `prebuild_guard.ps1 -ChecksOnly`. Not run (rules): build, viewer, in-world self-test. Worth an in-world look: the Who chip choosing an actor and All, the Look at camera card switch, one Advanced reset per kind (target, strength, eyeline, persona), the Performance preset applying and its reset, and the Cast member / Set point / Pick object rows appearing with the target mode.
