# Wiring audit: Ghost Studio, Prop Mover, Path Editor

Static, read-only audit. No build, no viewer run, no commit, no fetch. Only this file was written.

Sources: panel_ghost_studio.xml, panel_prop_mover.xml, panel_path_editor.xml (skins/default/xui/en); alpanelghoststudio.cpp, alpanelpropmover.cpp, alpanelpatheditor.cpp, alghoststudio.h/.cpp, alobjectpathmover.h, llactormover.h/.cpp, llghostavatar.cpp, llpathcamera.cpp, settings.xml (app_settings).

Method: each named control was matched to its XUI attributes (kind, label, control_name, min/max) and to its getChild, commit/clicked callback, reset-table entry or read-on-click site. Settings were checked for existence, type and default. Enum values were checked against header definitions. Line numbers are C++ unless marked otherwise.

Not covered: in-world tool gestures (drag, shift-drag, ground pick) inside ALToolGhostPlace, ALToolPathEdit and ALToolCrowdPlace; the renderer behind the starred Entity look items; the Director console host wrappers (panel_dpage_cast_*.xml), which only embed these panels.

Non-control items excluded from the table: static texts, status lines and the hidden text rows (anim_metadata_text, autoanim_status, lens_gaze_status), plus the Path Editor color_swatch panel, which is display-only (alpanelpatheditor.cpp:968-970).

## Counts

Controls checked: 272 (Ghost Studio 144, Prop Mover 34, Path Editor 94).

- OK: 265
- UNREACHABLE: 1
- LABEL-MISMATCH: 3
- RANGE: 1
- UNSURE: 2
- UNBOUND: 0
- MISSING-SETTING: 0
- TYPE-MISMATCH: 0

## Table (problems first, then OK)

| Panel | Widget | Kind | Label / tip | Binding | Verdict | Reason |
|---|---|---|---|---|---|---|
| Ghost Studio | crowd_copies_as_combo | combo_box | Copies as: Select clone / Copies as: Overlay ghost / Copies as: Entity clone | XML enabled=false; getChild :390; written from draft :1426, :3594; setEnabled(false) :1430, :3684 | UNREACHABLE | display-only by design; no user path can change it |
| Path Editor | path_end_combo | combo_box | Stop at end / Stop + face target / Loop / Ping-pong | cb :190 -> onPathEndCommit:2088 (END_STOP_FACE :2121-2130) | LABEL-MISMATCH | Stop + face target faces Director Subject A only; with no Subject A it silently falls back to plain stop :2126-2128 with no UI cue |
| Path Editor | path_camera_enable_check | check_box | Enable authored path camera | control_name PathCameraEnabled; settings.xml:7214-7224 Boolean default 0 with Persist=1; consumer llpathcamera.cpp:47 | LABEL-MISMATCH | label says for this session but the value persists across restarts |
| Ghost Studio | btn_lens_gaze_selection | button | Enable selected | cb :494 -> onClickLensGazeSelection:2621 (setGazeTargetMode from combo; setGazeEnabled true) | LABEL-MISMATCH | tip says camera gaze; target is the combo value (:2633-2635), so camera only when combo = Camera |
| Ghost Studio | pose_rate_spinner | spinner | Pose fps | cb :639 -> onPoseRateCommit:4149 -> setPoseRate | RANGE | XML min 0 accepts 1; alghoststudio.cpp:1280 clamps 0<hz<2 up to 2; tip says 2-29 Hz, so 1 silently becomes 2 |
| Ghost Studio | distort_combo | combo_box | Distort: None / Distort: Pixelate / Distort: Voxel / Distort: Lens/Magnify / Distort: Wave/Ripple / Distort... | cb :534 -> onDistortCommit:3021 (inst->mDistort) | UNSURE | labels 0-8; EGhostDistortion enum not found by grep (alghoststudio.h:520); consumer not traced |
| Ghost Studio | entity_look_combo | combo_box | Normal / Apparition / Hologram* / Chrome* / Toon* / Silhouette* | cb :538 -> onEntityLookCommit:3070 -> setInstanceLook | UNSURE | ids 0-5 = EGhostLook alghoststudio.h:119-123; tip says Apparition scaffolded and starred looks deferred; renderer not traced |
| Path Editor | reset_node_speed | button | tip: Reset speed override | bind_reset :243 | OK |  |
| Path Editor | node_speed_spinner | spinner | Speed override (m/s) | cb :177 -> onNodeSpeedCommit:1926 | OK |  |
| Path Editor | reset_node_dwell | button | tip: Reset dwell | bind_reset :242 | OK |  |
| Path Editor | btn_stop_path | button | Stop Path | cb :172 -> onClickStopPath:1430 | OK |  |
| Path Editor | btn_play_path | button | Play Path | cb :171 -> onClickPlayPath:1418 | OK |  |
| Path Editor | node_height_spinner | spinner | Height nudge (m) | cb :175 -> onNodeHeightCommit:1906 | OK |  |
| Path Editor | node_dwell_spinner | spinner | Dwell (s) | cb :176 -> onNodeDwellCommit:1916 | OK |  |
| Path Editor | reset_node_height | button | tip: Reset height nudge | bind_reset :241 -> onResetControl sets 0 :517-519 | OK |  |
| Path Editor | node_corner_radius_spinner | spinner | Corner radius (m) | cb :178 -> onNodeCornerRadiusCommit:1936 | OK |  |
| Path Editor | node_cam_transition_combo | combo_box | Cut / Ease | cb :207 -> onCamTransitionCommit:1868 | OK | 0 cut, 1 ease |
| Path Editor | btn_node_clearcam | button | Clear camera | cb :205 -> onClickClearCam:1855 | OK |  |
| Path Editor | reset_node_cam_transition | button | tip: Reset camera transition to Ease | bind_reset :247 sets Ease :539 | OK |  |
| Path Editor | path_speed_spinner | spinner | Travel speed (m/s) | cb :182 -> onPathSpeedCommit:1999 | OK |  |
| Path Editor | btn_node_preview | button | Preview shot | cb :206 -> onClickPreview:1879 | OK | label toggles Exit preview :1047 |
| Path Editor | btn_node_setcam | button | Capture camera here | cb :204 -> onClickSetCam:1829 | OK | enabled :1021 |
| Path Editor | node_anim_editor | line_editor | - | cb :180 -> onNodeAnimCommit:1974 | OK |  |
| Path Editor | reset_node_corner_radius | button | tip: Reset corner radius | bind_reset :244 | OK |  |
| Path Editor | reset_node_anim | button | tip: Clear node animation | bind_reset :245 | OK |  |
| Path Editor | reset_path_camera_enable | button | tip: Reset authored camera enable | bind_reset :246 | OK |  |
| Path Editor | btn_suspend_cancel | button | Cancel walk | cb :211 -> onClickCancelSuspend:1158 | OK | always enabled :1121 |
| Path Editor | btn_walk_here | button | Walk to Point... | cb :170 -> onClickWalkHere:1440 (armWalkTo) | OK |  |
| Path Editor | reset_ghost_distort | button | tip: Reset ghost distortion to default | bind_reset :239 | OK |  |
| Path Editor | ghost_distort_combo | combo_box | No distortion / Pixelate / Voxel / Lens / Wave / RGB Split / Block / V-Tear / VHS | control_name PathGhostDistort; settings.xml:7694-7704 S32 default 0; consumer llactormover.cpp:13462 | OK | 0-8 match settings comment |
| Path Editor | ghost_distort_amount | slider | Amount | control_name PathGhostDistortAmount; settings.xml:7705-7715 F32 default 0.5; consumer llactormover.cpp:13463 | OK | reset uses setting default |
| Path Editor | ghost_effect_fps | spinner | FX cadence (fps) | control_name PathGhostEffectFps; settings.xml:7683-7693 S32 default 0; consumer llactormover.cpp:13779 | OK |  |
| Path Editor | reset_ghost_distort_amount | button | tip: Reset distortion amount to default | bind_reset :240 | OK |  |
| Path Editor | reset_ghost_style | button | tip: Reset ghost style to default | bind_reset :237 | OK |  |
| Path Editor | waypoint_list | scroll_list | tip: Waypoints in order. Select a row to inspect or highlight tha | cb :159 -> onListSelect:1169 | OK |  |
| Path Editor | edit_mode_check | check_box | Edit in world | cb :158 -> onToggleEditMode:1294 (ALToolPathEdit) | OK | tool gestures not traced |
| Path Editor | onion_skin_check | check_box | Pose ghosts | control_name PathShowOnionSkin; settings.xml:7639-7649 Boolean default 0; consumer llactormover.cpp:13458; hotkey aldirectorhotkeys.cpp:142 | OK |  |
| Path Editor | ghost_style_combo | combo_box | Ghost / Clone / Hologram / Wireframe / X-ray / Thermal / Neon Outline / Silhouette / Toon/Ink / Chrome / Di... | control_name PathGhostStyle; settings.xml:7672-7682 S32 default 0; consumer llactormover.cpp:13461 | OK | combo and EGhostStyle use 0-27 (llactormover.cpp:11326-11355); settings comment lists 0-17 only (doc drift) |
| Path Editor | reset_onion_skin | button | tip: Reset pose ghosts to default | bind_reset :236 -> onResetControl setting branch :496-507 | OK |  |
| Path Editor | reset_ghost_effect_fps | button | tip: Reset FX cadence to default | bind_reset :238 | OK |  |
| Path Editor | btn_edit_mirror | button | Mirror | cb :168 -> onClickMirror:1380 -> mirrorPath | OK |  |
| Path Editor | btn_edit_reverse | button | Reverse | cb :167 -> onClickReverse:1369 -> reversePath | OK |  |
| Path Editor | btn_edit_loopclose | button | Close loop | cb :169 -> onClickLoopClose:1391 | OK |  |
| Path Editor | btn_copy_to | button | Copy path to | cb :173 -> onClickCopyTo:1402 -> copyPathTo:1415 | OK |  |
| Path Editor | copy_to_combo | combo_box | tip: Cast member to copy this path onto | no commit; read on copy :1409; populated :1600-1610 | OK | read-on-click by design |
| Path Editor | btn_edit_redo | button | Redo | cb :166 -> onClickRedo:1361 -> redoPath | OK |  |
| Path Editor | btn_wp_insert | button | Insert after | cb :161 -> onClickInsert:1196 | OK | after selected node; appends if last |
| Path Editor | btn_wp_add | button | Add at actor | cb :160 -> onClickAdd:1178 | OK | enabled :979 |
| Path Editor | btn_wp_delete | button | Delete node | cb :162 -> onClickDelete:1228 | OK |  |
| Path Editor | btn_edit_undo | button | Undo | cb :165 -> onClickUndo:1353 -> undoPath | OK |  |
| Path Editor | btn_wp_clear | button | Clear path | cb :163 -> onClickClear:1248 (confirm) -> clearPath:1283 | OK |  |
| Path Editor | reset_primitive_pitch | button | tip: Reset plane pitch | bind_reset :263 | OK |  |
| Path Editor | primitive_pitch_deg | spinner | Plane pitch | cb :200 -> onPrimitiveCommit:2184 | OK |  |
| Path Editor | reset_primitive_yaw | button | tip: Reset plane yaw | bind_reset :262 | OK |  |
| Path Editor | primitive_rise | spinner | Rise / end ΔZ (m) | cb :202 -> onPrimitiveCommit:2184 | OK |  |
| Path Editor | reset_primitive_roll | button | tip: Reset plane roll | bind_reset :264 | OK |  |
| Path Editor | primitive_roll_deg | spinner | Plane roll | cb :201 -> onPrimitiveCommit:2184 | OK |  |
| Path Editor | reset_primitive_start | button | tip: Reset start / phase | bind_reset :260 | OK |  |
| Path Editor | primitive_start_deg | spinner | Start / phase (deg) | cb :197 -> onPrimitiveCommit:2184 | OK |  |
| Path Editor | reset_primitive_radius_y | button | tip: Reset Extent Y | bind_reset :259 | OK |  |
| Path Editor | primitive_yaw_deg | spinner | Plane yaw | cb :199 -> onPrimitiveCommit:2184 | OK |  |
| Path Editor | reset_primitive_sweep | button | tip: Reset sweep / cycles | bind_reset :261 | OK |  |
| Path Editor | primitive_sweep_deg | spinner | Sweep / cycles (deg) | cb :198 -> onPrimitiveCommit:2184 | OK |  |
| Path Editor | follow_offset_spinner | spinner | Behind (m) | cb :217 -> onFollowOffsetCommit:1804 | OK |  |
| Path Editor | btn_stop_following | button | Stop following | cb :216 -> onStopFollow:1818 | OK |  |
| Path Editor | reset_follow_leader | button | tip: Reset follow leader | bind_reset :268 | OK |  |
| Path Editor | btn_suspend_reanchor | button | Re-anchor here | cb :210 -> onClickReanchor:1150 | OK |  |
| Path Editor | btn_suspend_resume | button | Resume | cb :209 -> onClickResume:1142 (resumeWalk) | OK |  |
| Path Editor | reset_follow_offset | button | tip: Reset following distance | bind_reset :269 | OK |  |
| Path Editor | reset_sync_take | button | tip: Reset camera take sync | bind_reset :266 | OK |  |
| Path Editor | sync_take_check | check_box | Sync to camera take | cb :213 -> onSyncToggle:1766 | OK | enabled :1644 |
| Path Editor | reset_primitive_rise | button | tip: Reset rise | bind_reset :265 | OK |  |
| Path Editor | follow_leader_combo | combo_box | - | cb :215 -> onFollowCommit:1787 | OK | items from cast :1702-1708 |
| Path Editor | reset_sync_offset | button | tip: Reset lead / trail | bind_reset :267 | OK |  |
| Path Editor | sync_offset_spinner | spinner | Lead/trail (s) | cb :214 -> onSyncOffsetCommit:1777 | OK | enabled when sync on :1652 |
| Path Editor | reset_path_easeout | button | tip: Reset ease out | bind_reset :252 | OK |  |
| Path Editor | path_easeout_spinner | spinner | Ease out (s) | cb :186 -> onPathEaseOutCommit:2048 | OK |  |
| Path Editor | reset_path_easein | button | tip: Reset ease in | bind_reset :251 | OK |  |
| Path Editor | path_pitch_check | check_box | Pitch to slope | cb :188 -> onPathPitchCommit:2066 | OK |  |
| Path Editor | reset_path_groundfollow | button | tip: Reset ground-follow | bind_reset :253 | OK |  |
| Path Editor | path_groundfollow_check | check_box | Ground-follow | cb :187 -> onPathGroundFollowCommit:2057 | OK |  |
| Path Editor | reset_path_cadence | button | tip: Reset foot cadence reference | bind_reset :249 | OK |  |
| Path Editor | path_cadence_slider | slider | Foot cadence ref | cb :183 -> onPathCadenceCommit:2008 (clamp 0.5..12) | OK |  |
| Path Editor | reset_path_speed | button | tip: Reset travel speed | bind_reset :248 sets 1.0 :543 | OK |  |
| Path Editor | path_easein_spinner | spinner | Ease in (s) | cb :185 -> onPathEaseInCommit:2039 | OK |  |
| Path Editor | reset_path_tension | button | tip: Reset corner smoothing | bind_reset :250 | OK |  |
| Path Editor | path_tension_slider | slider | Corner smoothing | cb :184 -> onPathTensionCommit:2016 | OK | enabled only without corner pieces or primitive :850 |
| Path Editor | btn_fit_sine | button | Fit Sine Wave to Endpoints | cb :194 -> onClickFitSine:2168 (SINE_WAVE) | OK | enabled n>=2 :984 |
| Path Editor | btn_fit_half_arc | button | Fit Half Arc to Endpoints | cb :193 -> onClickFitHalfArc:2153 (CIRCLE_ARC) | OK | enabled n>=2 :983 |
| Path Editor | btn_primitive_center | button | Center on actor | cb :192 -> onClickCenterPrimitive:1457 | OK |  |
| Path Editor | primitive_radius_y | spinner | Extent Y (m) | cb :196 -> onPrimitiveCommit:2184 | OK | enabled when shape uses Y :869 |
| Path Editor | reset_primitive_radius_x | button | tip: Reset Extent X | bind_reset :258 | OK |  |
| Path Editor | primitive_radius_x | spinner | Extent X (m) | cb :195 -> onPrimitiveCommit:2184 (clamp 0.01..10000) | OK | enabled for primitives :864 |
| Path Editor | reset_path_airborne | button | tip: Reset air path | bind_reset :255 | OK |  |
| Path Editor | path_airborne_check | check_box | Air path | cb :189 -> onPathAirborneCommit:2075 | OK |  |
| Path Editor | reset_path_pitch | button | tip: Reset pitch to slope | bind_reset :254 | OK |  |
| Path Editor | reset_path_shape | button | tip: Reset to Waypoints | bind_reset :257 sets Waypoints :579 | OK |  |
| Path Editor | path_shape_combo | combo_box | Waypoints / Circle / Arc / Ellipse / Arc / Helix / Diamond / Figure 8 / Sine Wave | cb :191 -> onPathShapeCommit:2140 | OK | enabled :862 |
| Path Editor | reset_path_end | button | tip: Reset end behavior | bind_reset :256 sets END_STOP :575 | OK |  |
| Ghost Studio | btn_group_member_reset | button | Reset member | cb :635 -> onClickGroupMemberReset:4128 | OK | enabled :1506 |
| Ghost Studio | strip_count_spinner | spinner | Frames | read on Capture :3926 (onClickStartStrip:3914) | OK | read-on-click by design |
| Ghost Studio | reset_strip_count | button | ↺ | reset table :653 | OK |  |
| Ghost Studio | reset_strip_interval | button | ↺ | reset table :654 | OK |  |
| Ghost Studio | strip_interval_spinner | spinner | Interval | read on Capture :3927 | OK | read-on-click by design |
| Ghost Studio | group_member_pinned_check | check_box | Pin member offset | cb :633 -> onGroupMemberPinnedCommit:4113 | OK | enabled in member edit :1505 |
| Ghost Studio | formation_overlay_palette_combo | combo_box | Preview: Neon green / Preview: Neon yellow / Preview: Cyan / Preview: White / Preview: Magenta | control_name GhostStudioOverlayPalette; settings.xml:7595-7604 S32 default 0 = alghoststudio.cpp:4910 | OK |  |
| Ghost Studio | btn_crowd_cancel | button | Cancel | cb :612 -> onClickCrowdCancel:3828 | OK |  |
| Ghost Studio | nameplate_mode_combo | combo_box | Names: Off / Names: Smart / Names: All | cb :637 -> onNameplateModeCommit:4142 writes GhostStudioNameplateMode; read :680 | OK | settings.xml:7415-7424 S32 default 1 = alghoststudio.cpp:1879 |
| Ghost Studio | lock_mode_combo | combo_box | Selected group: Off / Selected group: Rigid / Selected group: Move+Face | cb :621 -> onLockModeCommit:4079 | OK | enabled only with a group :1476 |
| Ghost Studio | group_edit_members_check | check_box | Edit group members | cb :631 -> onGroupEditMembersCommit:4097 | OK | enabled when a group is selected :1502 |
| Ghost Studio | reset_motion_amplitude | button | ↺ | reset table :656 | OK |  |
| Ghost Studio | edit_ghosts_check | check_box | Edit ghosts | cb :444 -> onToggleEditMode:2862 | OK | in-world gestures (drag, shift-drag) not traced |
| Ghost Studio | btn_formation_motion_apply | button | Apply selection | cb :616 -> onMotionCommit:3945 | OK |  |
| Ghost Studio | btn_ungroup_unit | button | Ungroup | cb :620 -> onClickUngroup:4074 | OK |  |
| Ghost Studio | btn_group_unit | button | Group | cb :619 -> onClickGroup:4068 (lockGroup) | OK |  |
| Ghost Studio | formation_motion_amplitude | spinner | Amp | read on Apply :3960 | OK |  |
| Ghost Studio | btn_strip_cancel | button | Cancel | cb :615 -> onClickCancelStrip:3935 | OK |  |
| Ghost Studio | btn_strip_start | button | Capture | cb :614 -> onClickStartStrip:3914 | OK |  |
| Ghost Studio | formation_motion_combo | combo_box | Off / Orbit / Spin / Breathe / Ripple / March actor path | read on Apply :3958 (onMotionCommit:3945), enum 0-5 EMotion alghoststudio.h:338-341 | OK | read-on-apply by design |
| Ghost Studio | reset_motion_speed | button | ↺ | reset table :655 | OK |  |
| Ghost Studio | formation_motion_speed | spinner | Speed | read on Apply :3959 | OK | reset edits the field only; applied on Apply |
| Ghost Studio | btn_freeze | button | Grab pose now | cb :540 -> onClickFreeze:3086 -> freezeInstance | OK | enabled overlay only :1477 |
| Ghost Studio | btn_live | button | Follow live | cb :541 -> onClickLive:3104 -> unfreezeInstance | OK |  |
| Ghost Studio | effect_fps_spinner | spinner | Effect fps | cb :537 -> onEffectFpsCommit:3063 -> setEffectFps | OK |  |
| Ghost Studio | reset_effect_fps | button | ↺ | reset table :648 | OK |  |
| Ghost Studio | mirror_hold_check | check_box | Hold on src change | control_name GhostMirrorHoldOnSourceChange; settings.xml:7247-7256 Boolean default 1 | OK | consumer llghostavatar.cpp:3265, :3445 |
| Ghost Studio | chaos_check | check_box | Chaos | cb :464 -> onChaosCommit:2320 | OK |  |
| Ghost Studio | chaos_slider | slider | tip: Chaos amount (0..1); stable for this clone's instance id | cb :465 -> onChaosCommit:2320 | OK | no label; tip gives meaning |
| Ghost Studio | mirror_repeat_held_check | check_box | Repeat held | control_name GhostMirrorRepeatHeldAnimesh; settings.xml:7258-7267 Boolean default 0 | OK | consumer llghostavatar.cpp:3263 |
| Ghost Studio | drive_mode_combo | combo_box | Mirror / Directed / Frozen / True mirror | cb :480 -> onDriveModeCommit:2397 -> setInstanceDriveMode | OK | 0-3 = EDriveMode alghoststudio.h:108-111 |
| Ghost Studio | reset_shimmer_amount | button | ↺ | reset table :661 | OK |  |
| Ghost Studio | glitch_slider | slider | Glitch | cb :533 -> onGlitchCommit:3011 | OK |  |
| Ghost Studio | reset_shimmer_speed | button | ↺ | reset table :660 | OK |  |
| Ghost Studio | shimmer_amount_slider | slider | Amount | cb :531 -> onShimmerAmountCommit:2991 | OK |  |
| Ghost Studio | reset_glitch | button | ↺ | reset table :662 | OK |  |
| Ghost Studio | brightness_slider | slider | Brightness | cb :536 -> onBrightnessCommit:3052 (clamp 0.05..1.5) | OK | matches XML range |
| Ghost Studio | reset_brightness | button | ↺ | reset table :664 | OK |  |
| Ghost Studio | distort_amount_slider | slider | Distort amt | cb :535 -> onDistortAmountCommit:3034 (clamp 0..1) | OK | enabled when distort not none :3030, :1472 |
| Ghost Studio | reset_distort_amount | button | ↺ | reset table :663 | OK |  |
| Ghost Studio | reset_chaos | button | ↺ | reset table :665 | OK |  |
| Ghost Studio | autoanim_window_spinner | spinner | Learn s | cb :469 -> onAutoAnimWindow:2483 | OK | XML initial 120 matches reset text |
| Ghost Studio | reset_autoanim_window | button | ↺ | reset table :650 | OK |  |
| Ghost Studio | directed_anim_editor | line_editor | Animation UUID | cb :481 -> onDirectedAnimCommit:2550 | OK | visible when Directed :1441 |
| Ghost Studio | autoanim_check | check_box | AutoAnimate | cb :468 -> onAutoAnimToggle:2479 | OK | enabled for entity clones :2499 |
| Ghost Studio | btn_autoanim_stop | button | Stop now | cb :470 -> onAutoAnimAction(StopLearning):2487 | OK |  |
| Ghost Studio | btn_autoanim_go | button | Go autonomous | cb :473 -> onAutoAnimAction(GoAutonomous):2487 | OK | enabled per :2513 |
| Ghost Studio | btn_autoanim_follow | button | Follow source | cb :474 -> onAutoAnimAction(FollowSource):2487 | OK |  |
| Ghost Studio | btn_autoanim_relearn | button | Relearn | cb :471 -> onAutoAnimAction(Relearn):2487 | OK |  |
| Ghost Studio | btn_autoanim_reseed | button | Reseed | cb :472 -> onAutoAnimAction(Reseed):2487 | OK | enabled when AutoAnimate on :2506 |
| Ghost Studio | physics_check | check_box | Physics | cb :477 -> onPhysicsCommit:2369 | OK |  |
| Ghost Studio | anim_loop_mode_combo | combo_box | Loop (re-trigger) / Play once | cb :490 -> onLoopModeCommit:2584 | OK | 0/1 = ELoopMode alghoststudio.h:112 |
| Ghost Studio | anim_speed_spinner | spinner | Anim Speed | cb :466 -> onAnimSpeedCommit:2358 (clamp 0.05..4) | OK |  |
| Ghost Studio | reset_anim_speed | button | ↺ | reset table :649 | OK |  |
| Ghost Studio | btn_anim_pause | button | Pause | cb :478 -> onClickAnimPause:2379 -> setInstancePaused(true) | OK | resume drive mode stored alghoststudio.h:378 |
| Ghost Studio | animation_library_combo | combo_box | tip: Animations in your own inventory only. Metadata loads asynch | cb :482 -> onAnimationLibraryCommit:2571 | OK | visible only when Directed :1443 |
| Ghost Studio | btn_anim_library_refresh | button | Refresh | cb :483 -> scanAnimLibrary + populate :485-488 | OK | visible when Directed :1444 |
| Ghost Studio | btn_anim_resume | button | Resume | cb :479 -> onClickAnimResume:2388 | OK |  |
| Ghost Studio | btn_anim_sync | button | Sync | cb :491 -> onClickAnimSync:2595 -> restartInstanceAnimation | OK |  |
| Ghost Studio | shimmer_speed_slider | slider | Shimmer Hz | cb :530 -> onShimmerSpeedCommit:2981 | OK |  |
| Ghost Studio | pos_z_spinner | spinner | Z | cb :457 -> onPosCommit:2216 | OK |  |
| Ghost Studio | yaw_spinner | spinner | Yaw (deg) | cb :462 -> onYawCommit:2233 | OK | range -180..180 matches XML |
| Ghost Studio | pos_x_spinner | spinner | X | cb :455 -> onPosCommit:2216 | OK |  |
| Ghost Studio | pos_y_spinner | spinner | Y | cb :456 -> onPosCommit:2216 | OK |  |
| Ghost Studio | reset_yaw | button | ↺ | reset table :646 -> onResetNumeric:4165 | OK |  |
| Ghost Studio | reset_scale | button | ↺ | reset table :647 | OK |  |
| Ghost Studio | btn_place | button | Place (click ground) | cb :510 -> onClickPlace:2688 (ALToolGhostPlace) | OK | enabled overlay only :1459 |
| Ghost Studio | heading_dial | compass_dial | tip: Ghost heading: drag the needle to rotate the selected ghost  | cb :460 -> onHeadingDialCommit:2251 | OK | tag registered llui/alcompassdial.cpp:26 |
| Ghost Studio | scale_spinner | spinner | Scale | cb :463 -> onScaleCommit:2337 | OK | XML min/max = GHOST_SCALE_MIN/MAX alghoststudio.h:64-65 |
| Ghost Studio | source_combo | combo_box | tip: Whose body the new ghost copies: you, or any cast member | no commit; read on click :1992 (onClickAdd); populated :1118-1127 | OK | read-on-click by design |
| Ghost Studio | clone_type_combo | combo_box | Overlay ghost / Entity clone | no commit; read :1994; 0/1 = BACKING_OVERLAY/ENTITY_CLONE alghoststudio.h:84 | OK | read-on-click by design |
| Ghost Studio | show_all_check | check_box | Show ghosts | cb alpanelghoststudio.cpp:443 -> onShowAllToggle:4155; init true :679 | OK |  |
| Ghost Studio | ghost_list | scroll_list | tip: Every ghost and crowd group. Double-click a clone to toggle  | cb :445; mouseup :446; dblclick :448 -> onListDoubleClick:1970 (toggle instance enable or group edit) | OK |  |
| Ghost Studio | btn_ghost_add | button | Add | cb :449 -> onClickAdd:1990 (addInstance / spawnEntityClone) | OK |  |
| Ghost Studio | btn_ghost_refresh | button | Refresh | cb :453 -> onClickRefresh:2208 (refreshEntityClone) | OK | visible+enabled for entity clones :1414-1415 |
| Ghost Studio | ghost_name | line_editor | Name | cb :450 -> onNameCommit:2014 (renameInstance/renameGroup) | OK |  |
| Ghost Studio | btn_ghost_dup | button | Duplicate | cb :451 -> onClickDuplicate:2031 -> duplicateInstance; copy offset to left alghoststudio.cpp:962-965 | OK | enabled overlay or group header only :1396 |
| Ghost Studio | btn_ghost_del | button | Delete | cb :452 -> onClickDelete:2165 | OK |  |
| Ghost Studio | btn_to_actor | button | To actor | cb :511 -> onClickToActor:2707 | OK | enabled overlay only :1461 |
| Ghost Studio | actor_tint_check | check_box | Actor hue | cb :527 -> onActorTintToggle:2951 | OK |  |
| Ghost Studio | hue_slider | slider | Hue | cb :528 -> onHueCommit:2961 | OK | enabled overlay and not tint :1465 |
| Ghost Studio | btn_paste_transform | button | Paste transform | cb :518 -> onClickPasteTransform:2839 | OK | enabled with clipboard :1489 |
| Ghost Studio | style_combo | combo_box | Ghost / Clone / Hologram / Wireframe / X-ray / Thermal / Neon Outline / Silhouette / Toon/Ink / Chrome / Di... | cb :526 -> onStyleCommit:2938 (inst->mStyle) | OK | ids 0-27 = EGhostStyle llactormover.cpp:11326-11355; labels match |
| Ghost Studio | reset_hue | button | ↺ | reset table :657 | OK |  |
| Ghost Studio | pixel_slider | slider | Pixelate | cb :532 -> onPixelCommit:3001 | OK |  |
| Ghost Studio | reset_pixel | button | ↺ | reset table :659 | OK |  |
| Ghost Studio | alpha_slider | slider | Alpha | cb :529 -> onAlphaCommit:2971 (clamp 0..1) | OK |  |
| Ghost Studio | reset_alpha | button | ↺ | reset table :658 | OK |  |
| Ghost Studio | btn_face_now | button | Face now | cb :521 -> onClickFaceNow:2298 | OK |  |
| Ghost Studio | keep_facing_check | check_box | Keep facing | cb :523 -> onKeepFacingCommit:2311 | OK |  |
| Ghost Studio | btn_to_me | button | To me | cb :512 -> onClickToMe:2744 | OK | enabled overlay only :1462 |
| Ghost Studio | look_target_combo | combo_box | tip: Aim at the camera, you, a Director cast member, or another G | cb :519 -> onLookTargetCommit:2269 | OK | items Camera/Me/Actor:/Ghost: :1166-1177 |
| Ghost Studio | btn_drop_ground | button | Drop to ground | cb :513 -> onClickDrop:2777 | OK |  |
| Ghost Studio | btn_duplicate_in_place | button | Duplicate here | cb :516 -> onClickDuplicateInPlace:2098 | OK |  |
| Ghost Studio | btn_copy_transform | button | Copy transform | cb :517 -> onClickCopyTransform:2829 | OK |  |
| Ghost Studio | btn_align_feet | button | Align feet | cb :514 -> onClickAlignFeet:2791 | OK | anchor = selectedUnitId |
| Ghost Studio | btn_reset_upright | button | Upright | cb :515 -> onClickUpright:2812 | OK | keeps yaw |
| Ghost Studio | formation_jitter_spinner | spinner | Jitter | loop cb :572 | OK | hidden for grid formations :3674 |
| Ghost Studio | formation_edge_gap_spinner | spinner | Edge gap | loop cb :575 | OK | hidden for grid formations :3673 |
| Ghost Studio | array_parameter_spinner | spinner | Parameter | cb :550 -> captureFormationParameter:3218 | OK | visible per formation :3282 |
| Ghost Studio | formation_seed_spinner | spinner | Seed | loop cb :571 -> updateOwnedCrowdPlacement | OK | read :3477 |
| Ghost Studio | reset_array_parameter | button | ↺ | cb :555 -> descriptor default | OK | visible per formation :3283 |
| Ghost Studio | formation_guide_size_spinner | spinner | Size | loop cb :574 | OK | enabled when guide shown :3679 |
| Ghost Studio | btn_array_line | button | Line | cb :545 -> onClickArray(LINE):3113 | OK |  |
| Ghost Studio | btn_crowd_unpin | button | Unpin | cb :610 -> onClickCrowdUnpin:3806 | OK |  |
| Ghost Studio | btn_array_ring | button | Ring | cb :547 -> onClickArray(RING):3113 | OK |  |
| Ghost Studio | formation_guide_shape_combo | combo_box | Guide: Off / Guide: Circle / Guide: Square | loop cb :573 -> updateOwnedCrowdPlacement | OK | 0-2 = EGuideShape alghoststudio.h:174-179 |
| Ghost Studio | array_formation_combo | combo_box | Line / Grid / Staggered rows / Split row / Soul Train / Ring / Arc / Concentric rings / Spiral / Chevron / ... | cb :549 -> onFormationCommit:3872 | OK | values = EFormation alghoststudio.h:150-172; Organic = 8 = FORMATION_SCATTER (alghoststudio.cpp:3846) |
| Ghost Studio | facing_yaw_offset_spinner | spinner | Facing offset | cb :592 -> setCrowdPlacementFacingOffset:3603 | OK |  |
| Ghost Studio | btn_pick_facing_target | button | Pick facing point | cb :617 -> onClickPickFacingTarget:3963 | OK |  |
| Ghost Studio | preserve_source_tilt_check | check_box | Preserve source tilt | loop cb :582 -> spec read :3481 | OK |  |
| Ghost Studio | btn_crowd_confirm | button | Confirm | cb :608 -> onClickCrowdConfirm:3777 | OK |  |
| Ghost Studio | formation_terrain_check | check_box | Snap each to terrain | loop cb alpanelghoststudio.cpp:581 -> updateOwnedCrowdPlacement; read :3478 | OK | |
| Ghost Studio | btn_array_build | button | Preview in world | cb :607 -> onClickBuildArray:3121 | OK |  |
| Ghost Studio | crowd_create_mode_combo | combo_box | Create: Separate / Create: Rigid group / Create: Move+Face group | loop cb :583 -> updateOwnedCrowdPlacement | OK | 0-2 = ELockMode alghoststudio.h:113-118; read :3483 |
| Ghost Studio | brigade_columns_spinner | spinner | Files | loop cb :576 | OK | visible for grid formations :3668 |
| Ghost Studio | brigade_file_spacing_spinner | spinner | File gap | loop cb :578 | OK | visible for grid formations :3670 |
| Ghost Studio | brigade_rows_spinner | spinner | Ranks | loop cb :577 | OK | visible for grid formations :3669 |
| Ghost Studio | formation_facing_combo | combo_box | Rotate with formation / Source yaw / Camera / stage / Centroid in / Outward / World point / Mirror source y... | loop cb :580 -> updateOwnedCrowdPlacement | OK | values = EFormationFacing alghoststudio.h:180-187 |
| Ghost Studio | brigade_rank_spacing_spinner | spinner | Rank gap | loop cb :579 | OK | visible for grid formations :3671 |
| Ghost Studio | reset_lens_gaze_smoothing | button | ↺ | reset table :669 | OK |  |
| Ghost Studio | btn_rig_diag_refresh | button | Refresh | cb :542 -> onClickRigDiagRefresh:1909 | OK |  |
| Ghost Studio | lens_gaze_intensity | slider | Intensity | cb :504 -> onLensGazeSettingsCommit:2639 | OK |  |
| Ghost Studio | reset_lens_gaze_intensity | button | ↺ | reset table :668 | OK |  |
| Ghost Studio | rig_diagnostics | text_editor | - | written by refreshRigDiagnostics:1837 | OK | read-only text editor |
| Ghost Studio | array_spacing_spinner | spinner | Spacing | loop cb :570 -> updateOwnedCrowdPlacement | OK |  |
| Ghost Studio | reset_array_spacing | button | ↺ | reset table :652 | OK |  |
| Ghost Studio | array_count_spinner | spinner | Count | loop cb :569-586 -> updateOwnedCrowdPlacement | OK |  |
| Ghost Studio | reset_array_count | button | ↺ | reset table :651 | OK |  |
| Ghost Studio | reset_lens_gaze_head_eye | button | ↺ | reset table :667 | OK |  |
| Ghost Studio | lens_gaze_check | check_box | This actor | cb :492 -> onLensGazeToggle:2604 | OK |  |
| Ghost Studio | lens_gaze_torso | slider | Torso | cb :496 -> onLensGazeTorsoCommit:2671 | OK |  |
| Ghost Studio | btn_autoanim_load | button | Load... | cb :476 -> onAutoAnimFile(false):2471 | OK |  |
| Ghost Studio | lens_gaze_smoothing | slider | Smoothing | cb :506 -> onLensGazeSettingsCommit:2639 | OK |  |
| Ghost Studio | btn_autoanim_save | button | Save... | cb :475 -> onAutoAnimFile(true):2471 | OK |  |
| Ghost Studio | lens_gaze_cast_target | combo_box | tip: Director cast member used by Cast target mode | cb :500 -> onLensGazeSettingsCommit:2639 | OK | enabled in Cast mode :1454 |
| Ghost Studio | lens_gaze_head_eye | slider | Head / eyes | cb :502 -> onLensGazeSettingsCommit:2639 | OK |  |
| Ghost Studio | lens_gaze_target_mode | combo_box | Travel tangent / Camera / Cast target / World point | cb :498 -> onLensGazeSettingsCommit:2639 | OK | 0-3 = GAZE_* llactormover.h:459 |
| Ghost Studio | reset_lens_gaze_torso | button | ↺ | reset table :666 | OK |  |
| Prop Mover | btn_remove | button | Remove | cb :90 -> onRemoveProp:353 | OK |  |
| Prop Mover | prop_list | scroll_list | tip: Enrolled props. The name fills in from the object's properti | cb :86 -> onSelectProp:463 | OK |  |
| Prop Mover | btn_node_add | button | Add node here | cb :91 -> onAddNode:367 -> appendWaypointHere | OK | appends at current prop position (matches tip) |
| Prop Mover | btn_enroll | button | Enroll selected object | cb :89 -> onEnrollSelected:326 (toggleTarget) | OK |  |
| Prop Mover | btn_capture_orient | button | Capture Orient | cb :104 -> onCaptureOrient:523 | OK | enabled with a node :270 |
| Prop Mover | btn_shuttle | button | Shuttle | cb :120 -> onClickShuttle:593 -> buildAxisShuttle:602 | OK |  |
| Prop Mover | shuttle_speed_spin | spinner | m/s | read on click :607 | OK |  |
| Prop Mover | btn_node_del | button | Delete node | cb :92 -> onDeleteNode:385 | OK | selected node, else last (matches tip) |
| Prop Mover | rock_on_check | check_box | Rock | cb :111 -> onCommitEffects:562 | OK |  |
| Prop Mover | rock_axis_combo | combo_box | X / X+Y / X+Y+Z | cb :112 -> onCommitEffects:562 | OK | 0-2 = ROCK_AXIS alobjectpathmover.h:92 |
| Prop Mover | rock_amp_spin | spinner | Amp deg | cb :113 -> onCommitEffects:562 | OK |  |
| Prop Mover | hover_wave_combo | combo_box | Full / Half | cb :108 -> onCommitEffects:562 | OK | 0/1 = HOVER_WAVE alobjectpathmover.h:91 |
| Prop Mover | hover_amp_spin | spinner | Amp m | cb :109 -> onCommitEffects:562 | OK |  |
| Prop Mover | hover_freq_spin | spinner | Hz | cb :110 -> onCommitEffects:562 | OK |  |
| Prop Mover | rock_freq_spin | spinner | Hz | cb :114 -> onCommitEffects:562 | OK |  |
| Prop Mover | shuttle_axis_combo | combo_box | X / Y / Z / XZ | cb :119 -> onCommitShuttleAxis:586 -> refreshNodePane | OK | 0-3 = EShuttleAxis alobjectpathmover.h:94-95 |
| Prop Mover | shuttle_sign_combo | combo_box | + / - | read on click :601 (onClickShuttle:593) | OK | 0 plus, 1 minus; read-on-click by design |
| Prop Mover | shuttle_slope_spin | spinner | Slope deg | read on click :608; enabled only for XZ :318-319 | OK |  |
| Prop Mover | spin_on_check | check_box | Spin | cb :115 -> onCommitEffects:562 | OK |  |
| Prop Mover | spin_axis_combo | combo_box | X / Y / Z | cb :116 -> onCommitEffects:562 | OK | 0-2 = SPIN_AXIS alobjectpathmover.h:93 |
| Prop Mover | spin_speed_spin | spinner | deg/s | cb :117 -> onCommitEffects:562 | OK | range -720..720 |
| Prop Mover | node_speed_spin | spinner | m/s | cb :102 -> onCommitNodeSpeed:508 -> setNodeSpeed | OK | range 0..50 |
| Prop Mover | path_speed_spin | spinner | Path m/s | cb :99 -> onCommitPathSpeed:474 (clamp 0.05..50) | OK | matches XML |
| Prop Mover | end_mode_combo | combo_box | Stop at end / Loop / Ping-pong | cb :100 -> onCommitEndMode:485 | OK | 0/1/2 = mEndMode stop/loop/ping-pong llactormover.h:137 |
| Prop Mover | btn_node_clear | button | Clear path | cb :93 -> onClearNodes:404 (stop + clearPath) | OK |  |
| Prop Mover | node_list | scroll_list | tip: The selected prop's path nodes. Select one to tune its skid  | cb :87 -> onSelectNode:469 | OK |  |
| Prop Mover | node_skid_spin | spinner | Skid (deg) | cb :101 -> onCommitNodeSkid:496 -> setNodeYawOffset | OK | deg to rad; range -180..180 |
| Prop Mover | btn_drive | button | Drive | cb :94 -> onDrive:417 | OK |  |
| Prop Mover | shuttle_distance_spin | spinner | Dist m | read on click :606 | OK |  |
| Prop Mover | rotation_mode_combo | combo_box | Face path / Authored / Face path + offset | cb :105 -> onCommitRotationMode:546 | OK | 0-2 = OBJPATH_ROT_* llactormover.h:158-160 |
| Prop Mover | hover_on_check | check_box | Hover | cb :107 -> onCommitEffects:562 | OK |  |
| Prop Mover | btn_stop | button | Stop | cb :95 -> onStop:438 | OK |  |
| Prop Mover | btn_drive_all | button | Drive all | cb :96 -> onDriveAll:450 | OK |  |
| Prop Mover | btn_stop_all | button | Stop all | cb :97 -> onStopAll:456 | OK |  |

