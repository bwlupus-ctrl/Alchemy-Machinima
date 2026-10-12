# Wiring audit: Cast and Performance pages (static, read-only)

Date: 2026-10-12. Static source review only. No build, no viewer run, no commit, no fetch. The only file written is this one.

## Scope

Pages (I:/alchemy-machinima/indra/newview/skins/default/xui/en/):
panel_dpage_cast_dance.xml, panel_dpage_cast_ghosts.xml, panel_dpage_cast_move.xml, panel_dpage_cast_path.xml, panel_dpage_cast_props.xml, panel_dpage_performance_animate.xml, panel_dpage_performance_gaze.xml, panel_dpage_performance_gaze_cues.xml, panel_dpage_performance_polish.xml, panel_dpage_performance_switchboard.xml.

Hosted legacy panels (their controls are included): panel_actor_mover.xml (move page), panel_path_editor.xml (path page), panel_prop_mover.xml (props page), panel_dance_ball.xml (dance page), panel_ghost_studio.xml (ghosts page), panel_lens_gaze.xml (gaze page), panel_pose_polish.xml (polish page), panel_director_anim_switcher.xml (switchboard page), panel_anim_preview.xml (animate page).

None of the ten pages uses `kit_row` markup. The controls audited are the widgets themselves.

## Method and reachability

- Page keys: each `kit_lazy_page name=` in floater_director_v2.xml:37-99 is the key passed to `LLFloaterDirector::bindPage` (llfloaterdirector.cpp:938-979) through `ALKitLazyPage::ensureBuilt` (alkitlazypage.cpp:48). All pages in scope map to a binder: move_tab, path_tab, animate_tab, dance_tab, gaze_tab, polish_tab, ghosts_tab (bindCameraPage) and gaze_cues_tab (bindLauncherPage). Props and switchboard need no Director binder, since their controls are inside hosted panels that self-bind.
- Setting checks: each `control_name` key was checked in app_settings/settings.xml (the key was present in every case; type and default read from the file). Range checks compare default against the widget min/max. Consumers were grepped in newview *.cpp.
- Verdict legend: OK, UNBOUND, UNREACHABLE, MISSING-SETTING, TYPE-MISMATCH, RANGE, LABEL-MISMATCH, UNSURE.

## Problems (non-OK rows)

Rows from the orchestrator's own scope: none.

| page | widget | kind | label | binding | verdict | reason |
|---|---|---|---|---|---|---|
| dance (panel_dance_ball) | db_ramp_target | spinner | Target | P:258, 1948 -> ALDanceBall::rampSpeed E:150-157 | RANGE | panel_dance_ball.xml:1024-1027 has min 0.25, max 4 but no value=. LLF32UICtrl starts at 0 (llf32uictrl.cpp:40, 45) and nothing sets it, so Go unedited sends 0, which E:153 clamps to 0.25 and everything slows to 0.25x. Needs value="1" |
| dance (panel_dance_ball) | db_adv_jitter | spinner | Jitter | P:243, 371, 1898 -> setTiming E:772-779 | RANGE | XML max_val=300 (panel_dance_ball.xml:612) but the engine clamps jitter to 0.49 x interval (E:777-779). Larger entries are accepted, then the refresh at P:1053 snaps back. Cosmetic |
| dance (panel_dance_ball) | db_block_yaw_minus | button | -15 deg | P:394 -> onBlockingNudgeYaw(-1) P:1491-1497 | LABEL-MISMATCH | Label fixed "-15" in the XML (panel_dance_ball.xml:800). Applied delta is sign x the Step spinner (P:1493). Step is 1..15 (xml:809-811) and P never calls setLabel on this button |
| dance (panel_dance_ball) | db_block_yaw_plus | button | +15 deg | P:396 -> onBlockingNudgeYaw(+1) P:1491-1497 | LABEL-MISMATCH | Same as above; label fixed "+15", delta = Step spinner |

## Audit table

Columns: page | widget | kind | label | binding | verdict | reason. Settings file abbreviated as S = app_settings/settings.xml. C++ paths are under I:/alchemy-machinima/indra/newview/ unless stated.

### Page-level controls

| page | widget | kind | label | binding | verdict | reason |
|---|---|---|---|---|---|---|
| panel_dpage_cast_dance | dance_popout | button | Open Dance Ball window... | llfloaterdirector.cpp:1216-1218 (bindCameraPage, dance_tab) -> LLFloaterReg "dance_ball" | OK | llviewerfloaterreg.cpp:789; floater_dance_ball.xml present |
| panel_dpage_cast_move | group_run_combo | combo_box | Group | llfloaterdirector.cpp:983 (bindMovePage, move_tab); read on demand in onClickGroup 2992; items filled in refreshGroupControls 3109-3130 | OK | No commit callback by design; the selection is read when Start or Stop is clicked |
| panel_dpage_cast_move | btn_group_start | button | Start group | llfloaterdirector.cpp:984, 994 -> onClickGroup(true) 2990-3015 -> cast.startMovesStaggered | OK | Real action |
| panel_dpage_cast_move | btn_group_stop | button | Stop group | :985, 995 -> onClickGroup(false) -> mover.stop per member | OK | Real action |
| panel_dpage_cast_move | groups_list | scroll_list | Groups (Group / Members / Delay) | :986, 996 -> onGroupsListSelect 3026-3046 | OK | Column names match :3208-3210 |
| panel_dpage_cast_move | group_delay_spinner | spinner | Delay (s) 0-120 | :987, 997 -> onCommitGroupDelay 3048-3055 -> setGroupDelay | OK | Code-bound; range 0-120 matches the writer |
| panel_dpage_cast_move | group_rename_editor | line_editor | (rename field) | :988, 998 -> onClickGroupRename 3057-3078 -> renameGroup | OK | Commit on Enter |
| panel_dpage_cast_move | btn_group_rename | button | Rename | :989, 999 -> onClickGroupRename | OK | Same handler |
| panel_dpage_cast_move | btn_group_dissolve | button | Dissolve | :990, 1000 -> onClickGroupDissolve 3080-3093 -> dissolveGroup | OK | Untags only, per tool tip |
| panel_dpage_cast_ghosts | (none) | | | Hosts ghost_studio_panel | n/a | Controls are in the ghost-studio audit |
| panel_dpage_cast_path | (none) | | | Hosts path_editor | n/a | Controls are in the path-editor audit |
| panel_dpage_cast_props | (none) | | | Hosts prop_mover_panel | n/a | Controls are in the prop-mover audit |
| panel_dpage_cast_dance | (hosts director_dance_ball_panel) | | | | n/a | Controls are in the dance-ball audit |
| panel_dpage_performance_gaze | btn_actor_gaze | button | Open Actor Gaze window... | llfloaterdirector.cpp:1207-1210 (bindCameraPage, gaze_tab) -> onOpenActorGaze 4171-4176 -> ALFloaterActorGaze::showForSelection | OK | floater_actor_gaze.xml; llviewerfloaterreg.cpp:777 |
| panel_dpage_performance_gaze | (hosts director_gaze_panel) | | | | n/a | Controls are in the lens-gaze audit |
| panel_dpage_performance_gaze_cues | card_gaze_cues_open | button | Open Gaze Cues... | llfloaterdirector.cpp:1283-1285 (bindLauncherPage, gaze_cues_tab) -> addLauncherCard 1593-1605 (setCommitCallback) -> showInstance("gaze_cues") :1290 | OK | floater_gaze_cues.xml; llviewerfloaterreg.cpp:781 |
| panel_dpage_performance_polish | polish_popout | button | Open Pose Polish window... | llfloaterdirector.cpp:1220-1222 (bindCameraPage, polish_tab) -> showInstance("pose_polish") | OK | floater_pose_polish.xml; llviewerfloaterreg.cpp:780 |
| panel_dpage_performance_switchboard | (hosts director_anim_switcher_embedded) | | | | n/a | Controls are in the switchboard table below |
| panel_dpage_performance_animate | anim_list | scroll_list | Animation UUID / Pri / On | llfloaterdirector.cpp:1091 (bindAnimatePage); right-click 1153-1154 -> onAnimRightClick 3615; refresh 3891+ | OK | Columns prio and playing exist (:533-534) |
| panel_dpage_performance_animate | btn_anim_copy | button | Copy UUID | :1093, 1136 -> onAnimCopyUUID 3636-3646 (clipboard) | OK | Real action |
| panel_dpage_performance_animate | btn_anim_setloco | button | Set role | :1094, 1137 -> onAnimSetLoco 3691 -> applySelectedLocomotion 3666-3689 | OK | Writes the locomotion set |
| panel_dpage_performance_animate | btn_anim_play | button | Play local | :1095, 1138 -> onAnimPlayLocal(true) 3717-3731 | OK | Client-side only, per tool tip |
| panel_dpage_performance_animate | btn_anim_stop | button | Stop local | :1096, 1139 -> onAnimPlayLocal(false) | OK | |
| panel_dpage_performance_animate | paste_uuid_editor | line_editor | Paste UUID | :1097; read by pasteAnimId 3604-3613 on demand | OK | No commit callback by design |
| panel_dpage_performance_animate | btn_paste_play | button | Play local | :1098, 1140 -> onPastePlayLocal(true) 3733-3746 | OK | |
| panel_dpage_performance_animate | btn_paste_stop | button | Stop local | :1099, 1141 -> onPastePlayLocal(false) | OK | |
| panel_dpage_performance_animate | btn_paste_setloco | button | Set role | :1100, 1142 -> onPasteSetLoco 3748-3751 | OK | |
| panel_dpage_performance_animate | play_local_priority_combo | combo_box | Priority | control_name DirectorAnimPlayLocalPriority, S:7760 S32 default -1; items -1..4 match; consumer llfloaterdirector.cpp:3706 | OK | |
| panel_dpage_performance_animate | locomotion_role_combo | combo_box | Locomotion role | :1101, 1135, 1144-1145 -> onLocomotionRoleChanged 3768 | OK | Item values 1,2,3,9,10 match LOCO_IDLE, WALK_FORWARD, RUN_FORWARD, FLY, HOVER (lldirectorcast.h:186-198) |
| panel_dpage_performance_animate | locomotion_speed | spinner | m/s | :1102, 1146-1151 -> onLocomotionMetadataCommit 3774-3790 | OK | Clamp 0.05-20 matches XML |
| panel_dpage_performance_animate | locomotion_phase | spinner | Phase | :1103, 1146-1151 | OK | Clamp -1..1 matches XML |
| panel_dpage_performance_animate | locomotion_allow_ao | check_box | AO | :1104, 1146-1151 -> mAllowAO | OK | |
| panel_dpage_performance_animate | btn_clear_loco | button | Clear | :1106, 1143 -> onClickClearLoco 3753-3766 | OK | |
| panel_dpage_performance_animate | btn_open_animexp | button | Open Animation Explorer | :1155-1158 -> showInstance("animation_explorer") | OK | llviewerfloaterreg.cpp:875 |

### Hosted panel: panel_actor_mover (move page)

| page | widget | kind | label | binding | verdict | reason |
|---|---|---|---|---|---|---|
| move | scope_radio | radio_group | Selected actors / Everyone | No control_name. alpanelactormover.cpp:37-40 init from ActorMoverSync; :38 commit -> onScopeCommit 90-96 writes ActorMoverSync (value 1 = Everyone = true) | OK | Value mapping is consistent |
| move | scope_selected | radio_item (0) | Selected actors | part of scope_radio | OK | |
| move | scope_everyone | radio_item (1) | Everyone | part of scope_radio | OK | |
| move | heading_dial | ALCompassDial | Heading trim | alpanelactormover.cpp:42-43 -> onDialCommit 98-102 writes ActorMoverHeading; ALCompassDial::onCommit alcompassdial.cpp:201-275 | OK | Leaf control; handleMouseDown alcompassdial.cpp:192 |
| move | heading_spinner | spinner | Heading (deg) | control_name ActorMoverHeading, S:7159 F32 default 0, range -180..180 | OK | Consumer llactormover.cpp:4222 |
| move | speed_slider | slider | Speed (m/s) | control_name ActorMoverSpeed, S:7137 F32 default 1.0, range 0.05..150 | OK | Consumer llactormover.cpp:4228 |
| move | speed_spinner | spinner | (none) | same setting | OK | |
| move | dist_slider | slider | Distance (m) | control_name ActorMoverDistance, S:7148 F32 default 6.0, range 0.5..2048 | OK | Consumer llactormover.cpp:4229 |
| move | dist_spinner | spinner | (none) | same setting | OK | |
| move | cadence_slider | slider | Cadence (m/s) | control_name ActorMoverWalkNominal, S:7181 F32 default 3.0, range 1..150 | OK | Consumer llactormover.cpp:4231, 436 |
| move | cadence_spinner | spinner | (none) | same setting | OK | |
| move | end_mode_combo | combo_box | End behavior | control_name ActorMoverEndMode, S:7170 S32 default 0; items 0,1,2 | OK | Consumer llactormover.cpp:4230 |
| move | custom_anim_check | check_box | Custom walk anim (UUID) | control_name ActorMoverUseCustomAnim, S:7727 BOOL default 0 | OK | Consumer llactormover.cpp:149 |
| move | custom_anim_editor | line_editor | (UUID) | control_name ActorMoverCustomAnim, S:7738 String default "" | OK | Consumer llactormover.cpp:150 |
| move | custom_anim_priority_combo | combo_box | Priority | control_name ActorMoverCustomAnimPriority, S:7749 S32 default -1; items -1..4 | OK | Consumer llactormover.cpp:174 |
| move | btn_walk | button | Walk | alpanelactormover.cpp:45, 47, 104-121 -> startAll or start(id) | OK | Real action |
| move | btn_stop | button | Stop / Release | :46, 48, 123-137 -> stopAll or stop(id) | OK | Real action |

### Hosted panel: panel_pose_polish (polish page)

Page master_setting="ALPolishEnabled" (floater_director_v2.xml:49) is default on (S:8574).

| page | widget | kind | label | binding | verdict | reason |
|---|---|---|---|---|---|---|
| polish | polish_master_check | check_box | Enable Pose Polish (master) | control_name ALPolishEnabled, S:8574 BOOL default 1 | OK | Consumers llvoavatar.cpp:5595, 5680 |
| polish | polish_inertia_check | check_box | Transition inertialization | ALPolishInertiaEnabled, S:8607 BOOL default 1 | OK | llvoavatar.cpp:5625 |
| polish | polish_inertia_spine_slider | slider | Spine settle (s) | ALPolishInertiaHalfLifeSpine, S:8618 F32 default 0.13, range 0.02..0.40 | OK | llvoavatar.cpp:5213 |
| polish | polish_inertia_arm_slider | slider | Arm settle (s) | ALPolishInertiaHalfLifeArm, S:8629 F32 0.09, range 0.02..0.40 | OK | llvoavatar.cpp:5214 |
| polish | polish_inertia_leg_slider | slider | Leg settle (s) | ALPolishInertiaHalfLifeLeg, S:8640 F32 0.09, range 0.02..0.40 | OK | llvoavatar.cpp:5215 |
| polish | polish_inertia_head_slider | slider | Head settle (s) | ALPolishInertiaHalfLifeHead, S:8651 F32 0.07, range 0.02..0.40 | OK | llvoavatar.cpp:5216 |
| polish | polish_contact_check | check_box | Foot contact stabilizer (IK) | ALPolishContactEnabled, S:8585 BOOL default 1 | OK | llvoavatar.cpp:5630 |
| polish | polish_contact_weight_slider | slider | Contact strength | ALPolishContactWeight, S:8596 F32 default 0.75, range 0..1 | OK | llvoavatar.cpp:5270 |
| polish | polish_phasematch_check | check_box | Phase-matched walk/run starts | ALPolishPhaseMatchEnabled, S:8706 BOOL default 1 | OK | llvoavatar.cpp:5681 |
| polish | polish_loopseam_check | check_box | Loop-seam repair | ALPolishLoopSeamEnabled, S:8695 BOOL default 1 | OK | llvoavatar.cpp:5682 |
| polish | polish_secondary_check | check_box | Secondary motion (breath + idle sway) | ALPolishSecondaryEnabled, S:8662 BOOL default 1 | OK | llvoavatar.cpp:5635 |
| polish | polish_secondary_breath_slider | slider | Breath amount | ALPolishSecondaryBreathAmp, S:8673 F32 default 1.0, range 0..2 | OK | llvoavatar.cpp:5527 |
| polish | polish_secondary_sway_slider | slider | Sway amount | ALPolishSecondarySwayAmp, S:8684 F32 default 1.0, range 0..2 | OK | llvoavatar.cpp:5528 |
| polish | polish_diagnostic_check | check_box | Layer diagnostic overlay | ALPolishLayerDiagnostic, S:8717 BOOL default 0 | OK | llvoavatar.cpp:5643 |

### Hosted panel: panel_anim_preview (animate page)

| page | widget | kind | label | binding | verdict | reason |
|---|---|---|---|---|---|---|
| animate | capture_all_check | check_box | Capture all avatars | control_name AnimExplorerCaptureAll, S:8728 BOOL default 0 | OK | Consumer llviewermessage.cpp:4351 |
| animate | btn_anim_ex_stop | button | Stop | alpanelanimpreview.cpp:63, 66 -> onStop 292-300 (stopMotion, sendAnimationRequest) | OK | Real action |
| animate | btn_anim_ex_revoke | button | Stop and Revoke | :64, 67 -> onStopAndRevoke 302-316 (sendRevokePermissions) | OK | Real action |
| animate | btn_anim_ex_blacklist | button | Blacklist | :65, 68 -> onBlacklist 318-332 (ALAssetBlocklist addEntry) | OK | Real action |
| animate | animation_preview | view_border (preview rect) | (preview) | findChild alpanelanimpreview.cpp:56; mouse handled in handleMouseDown 168-185 with fallthrough to LLPanel for points outside the rect | OK | Fallthrough present at :184 |

### Hosted panel: panel_director_anim_switcher (switchboard page)

| page | widget | kind | label | binding | verdict | reason |
|---|---|---|---|---|---|---|
| switchboard | anim_switcher_armed | check_box | Armed | control_name DirectorAnimSwitcherArmed, S:8849 BOOL default 0, Persist 0 | OK | Consumers aldirectorhotkeys.cpp:53, alpaneldirectoranimswitcher.cpp:553 |
| switchboard | anim_switcher_auto | check_box | Auto | control_name DirectorAnimSwitcherAuto, S:8860 BOOL default 0 | OK | Consumer aldirectoranimswitcher.cpp:213 |
| switchboard | anim_slot_01 .. anim_slot_12 | button (toggle) | 1..12 | alpaneldirectoranimswitcher.cpp:56-59 -> onSlotButton 215-230 (selects slot, punches if armed) | OK | Generated name loop; 12 slots = SLOT_COUNT |
| switchboard | anim_slot_kind | combo_box | Kind (Anim / Pose) | :62, 82-83 -> onSlotKind | OK | Items 0 and 1 equal KIND_ANIM=0 and KIND_POSE=1 (aldirectoranimswitcher.h:53-54) |
| switchboard | anim_slot_reset | button | Reset slot | :77, 106-107 -> onResetSlot 397-409 | OK | Real action |
| switchboard | anim_slot_uuid | line_editor | Anim UUID | :63, 84-85 -> onAnimUUID | OK | |
| switchboard | anim_from_explorer | button | From Explorer | :65, 90-91 -> onFromExplorer | OK | Opens animation_explorer (:377) |
| switchboard | anim_pose_name | combo_box | Pose | :66, 86-87 -> onPoseName; visibility toggled :541-545 | OK | Hidden in Anim kind, shown in Pose kind |
| switchboard | anim_pose_load_method | combo_box | (load method) | :67, 88-89 -> onPoseLoadMethod 289-297 | OK | Items 1-12 match E_LoadPoseMethods (fsposeranimator.h:38-52); default 7 = ROT_POS_AND_SCALES |
| switchboard | anim_slot_label | line_editor | Label | :69, 92-93 -> onSlotLabel | OK | |
| switchboard | anim_slot_target | combo_box | Target | :70, 94-95 -> onSlotTarget | OK | Items 0,1,2 equal TARGET_CAST/SELF/GHOSTS (aldirectoranimswitcher.h:46-48) |
| switchboard | anim_slot_priority | combo_box | Priority | :71, 96-97 -> onSlotPriority | OK | Default -1, 0..7 per settings comment |
| switchboard | anim_slot_speed | spinner | Speed 0.1..5 | :73, 98-99 -> onSlotSpeed (writes mSpeed :341) | OK | Default 1.f (aldirectoranimswitcher.h:67) in range |
| switchboard | anim_slot_loop | check_box | Loop | :74, 100-101 -> onSlotLoop | OK | |
| switchboard | anim_slot_snap | check_box | Snap on cut | :75, 102-103 -> onSlotSnap | OK | |
| switchboard | anim_slot_enabled | check_box | Use in auto | :76, 104-105 -> onSlotEnabled (writes mEnabled :245) | OK | |
| switchboard | anim_interval | spinner | Every (s) | control_name DirectorAnimSwitcherIntervalSec, S:8882 F32 default 6.0, range 0.25..120 | OK | Consumer aldirectoranimswitcher.cpp:216 |
| switchboard | anim_jitter | spinner | Jitter (s) | control_name DirectorAnimSwitcherJitterSec, S:8893 F32 default 0.0, range 0..30 | OK | Consumer aldirectoranimswitcher.cpp:218 |
| switchboard | anim_sequence | check_box | Sequence | control_name DirectorAnimSwitcherSequence, S:8915 BOOL default 0 | OK | Consumer aldirectoranimswitcher.cpp:214 |

## Delegated panel audits

### Hosted panel: panel_dance_ball (dance page, panel_dpage_cast_dance.xml:5)

Code-bound: the panel has no control_name, setting, or commit_callback attributes. Every control is bound via getChild and setCommitCallback in ALPanelDanceBall::postBuild (alpaneldanceball.cpp:202-420). Verified by the auditor: all 82 getChild names exist in the XML. Buttons toggle before the callback fires (llbutton.cpp:339-345, 463-468). The only custom override is handleDragAndDrop (P:2071), which takes animation and folder drops at panel level.

| page | widget | kind | label | binding | verdict | reason |
|---|---|---|---|---|---|---|
| dance | panel_dance_ball | root | - | LLPanelInjector P:63; postBuild P:202 | OK | Injector name matches class= in panel_dpage_cast_dance.xml:5 |
| dance | db_on | button (toggle) | OFF/ON | P:334 -> onToggleOn P:1111 -> ALDanceBall::setEnabled E:114 | OK | State mirrored back at P:806 |
| dance | db_prev | button | prev | P:335 -> E:207 previous() | OK | |
| dance | db_freeze | button (toggle) | Freeze/Frozen | P:337 -> setFrozen E:134 | OK | Mirrored P:807 |
| dance | db_next | button | next | P:336 -> E:189 next() | OK | |
| dance | db_shuffle | button (toggle) | Shuffle | P:338 -> onShuffle P:1136 -> setTiming mMode E:772 | OK | Writes SHUFFLE_EACH_DANCER or SAME_FOR_ALL; mirrored P:808 |
| dance | db_stop_all | button | Stop all | P:339 -> setEnabled(false) P:1147 | OK | Releases all dancers (E:126-131) |
| dance | db_sync_label | text | Sync | static | OK | Caption |
| dance | db_header_desync | slider | Sync..Desync | P:340 -> onHeaderDesync P:1154 -> timing.mDesync | OK | Range 0..1, default 0 (E:74), clamp01 at E:780 |
| dance | db_desync_label | text | Desync | static | OK | Caption |
| dance | db_status | text | status line | refreshHeader P:864 | OK | Strings exist at panel xml:42-45 |
| dance | db_tabs | tab_container | - | P:213; ALScrollFit P:416 | OK | Container |
| dance | db_dancers_tab | panel | Dancers | static | OK | |
| dance | db_dancer_list | scroll_list | roster | P:215; rows P:867-903; read by onRemove, onExclude, onPlaySelected | OK | Columns on/name/kind/now/dist match P:893-902; multi-select |
| dance | db_add_selected | button | Add selected | P:343 -> onAddSelected P:1181 -> addDancer | OK | |
| dance | db_add_cast | button | Add cast | P:344 -> E:615 addCast | OK | |
| dance | db_add_ghosts | button | Add ghosts | P:345 -> onAddGhosts P:1239 | OK | Adds BACKING_ENTITY_CLONE only (P:1246), matches tooltip |
| dance | db_remove | button | Remove | P:347 -> onRemove P:1262 -> removeDancer | OK | |
| dance | db_exclude | button | Exclude | P:348 -> onExclude P:1281 -> E:519 | OK | Removes and blocks re-add |
| dance | db_add_nearby | button | Add nearby | P:346 -> onAddNearby P:1254 -> E:584 | OK | |
| dance | db_radius | spinner | Radius | P:350 -> onAutoJoinChanged P:1318 -> E:626 setAutoJoin | OK | Default 20 (E:320) within 1..512; XML initial 0 overwritten on show |
| dance | db_radius_unit | text | m | static | OK | |
| dance | db_focus_label | text | around | static | OK | |
| dance | db_focus | combo_box | Me / Camera focus / Subject A | P:351 -> onAutoJoinChanged, onAddNearby P:1258 | OK | Values 0/1/2 match AutoJoinFocus (aldanceball.h:45) |
| dance | db_autojoin | check_box | Auto-join | P:352 -> setAutoJoin E:626; consumed E:1931 | OK | Default off |
| dance | db_include_seated | check_box | Include seated | P:353 -> E:635; used E:859 | OK | Default off |
| dance | db_include_me | check_box | Include me | P:354 -> onIncludeMe P:1336 -> E:645 | OK | Default off |
| dance | db_local_note | text | Local only | static | OK | |
| dance | db_clear_exclusions | button | Clear exclusions | P:349 -> onClearExclusions P:1304 | OK | |
| dance | db_playlist_tab | panel | Playlist | static | OK | |
| dance | db_drop_hint | text | Drop animations | static | OK | Drops handled P:2071 |
| dance | db_playlist_list | scroll_list | playlist | P:229; commit P:365 -> onPlaylistSelection P:1759 | OK | Columns name/length/loop/status match P:982-989 |
| dance | db_from_explorer | button | From Explorer | P:357 -> onFromExplorer P:1616 | OK | getSelectedAnimId exists (animationexplorer.h:96) |
| dance | db_paste_uuid | button | Paste UUID | P:358 -> onPasteUUID P:1642 | OK | |
| dance | db_pl_remove | button | Remove | P:359 -> removeAnimation | OK | |
| dance | db_pl_clear | button | Clear | P:360 -> clearPlaylist | OK | |
| dance | db_pl_up | button | Up | P:361 -> moveAnimation(i, i-1) P:1709 | OK | |
| dance | db_pl_down | button | Down | P:362 -> moveAnimation(i, i+1) P:1724 | OK | |
| dance | db_rename_edit | line_editor | row name | P:364 -> onPlaylistRename P:1739 | OK | Enter commits; commit_on_focus_lost=false matches tooltip |
| dance | db_pl_rename | button | Rename | P:363 -> renameAnimation | OK | |
| dance | db_play_selected | button | Play on selected | P:366 -> onPlaySelected P:1815 | OK | Enables ball if off (P:1835) |
| dance | db_play_all | button | Play on all | P:367 -> onPlayAll P:1848 | OK | Switches to SAME_FOR_ALL (P:1864) |
| dance | db_mode_label | text | Mode | static | OK | |
| dance | db_mode | combo_box | Same / Shuffle / Sequence | P:368 -> onModeChanged P:1882 | OK | Values 0/1/2 match PlaylistMode (aldanceballmodel.h:24-29) |
| dance | db_auto_advance | check_box | Auto-advance | P:369 -> onAutoAdvanceChanged P:1892; E:182 | OK | Default off |
| dance | db_adv_interval | spinner | Every | P:370 -> mAdvanceSeconds; E:183 | OK | Default 8 within 0.5..600 (E:72) |
| dance | db_adv_interval_unit | text | s | static | OK | |
| dance | db_adv_jitter_unit | text | s | static | OK | |
| dance | db_timing_tab | panel | Timing | static | OK | |
| dance | db_t_desync | slider | Desync | P:374 -> onTimingChanged(false) P:1907 | OK | 0..1, default 0; consumed E:1679-1685 |
| dance | db_reshuffle | button | Reshuffle phases | P:380 -> E:159 | OK | Consumed E:1681-1685 |
| dance | db_t_speed | slider | Speed | P:375 -> onTimingChanged(true) -> mSpeed -> clock E:794-797 | OK | 0.25..4 matches E:781 clamp; default 1 |
| dance | db_t_speed_jitter | slider | Speed jitter | P:376 -> mSpeedJitter | OK | 0..1, default 0; consumed E:1681 |
| dance | db_t_crossfade | slider | Crossfade (s) | P:377 -> mCrossfadeSeconds | OK | 0..5, default 0.5 (aldanceballmodel.h:17); consumed E:1089, 1751 |
| dance | db_priority_label | text | Priority | static | OK | |
| dance | db_t_priority | combo_box | Asset / 4 / 5 / 6 | P:378 -> mPriority P:1918 -> E:1364 | OK | Values -1/4/5/6 match sanitize_priority E:63-66; default 6 |
| dance | db_t_dance_drives_face | check_box | Dance drives face | P:379 -> mDanceDrivesFace; E:792-824, 1047 | OK | Default false |
| dance | db_blocking_tab | panel | Blocking | static | OK | |
| dance | db_blocking_list | scroll_list | dancers for blocking | P:264; commit P:392; rows P:1360-1399 | OK | Columns match P:1386; Me excluded P:1368 |
| dance | db_block_toggle_rows | button | Toggle selected | P:393 -> onBlockingToggleRows P:1472 -> setDancerBlocking E:450 | OK | |
| dance | db_block_rotate_title | text | Rotate | static | OK | |
| dance | db_block_yaw | slider | yaw | P:395 -> onBlockingYaw P:1483 -> S:230 setYaw | OK | -180..180 deg converted to radians; mirrored P:1460 |
| dance | db_block_step | spinner | Step | read at P:1493 | OK | Value 15 within 1..15 |
| dance | db_block_mode | radio_group | OFFSET / LOCK | P:397 -> onBlockingMode P:1499 -> S:266 setMode | OK | Index 0 OFFSET, 1 LOCK; mirrored P:1462 |
| dance | db_block_offset | radio_item | OFFSET | child of db_block_mode | OK | Index 0 -> FacingMode::OFFSET (P:1501) |
| dance | db_block_lock | radio_item | LOCK | child of db_block_mode | OK | Index 1 -> FacingMode::LOCK (P:1502) |
| dance | db_block_face_title | text | Face | static | OK | |
| dance | db_block_face | combo_box | Camera ... Match first selected | P:398 -> onBlockingFace P:1508-1549 | OK | Values 0..5 map to branches (S:390-451, P:1530) |
| dance | db_block_keep_facing | check_box | Keep facing | read at P:1513 | OK | Applied on next Face choice |
| dance | db_block_place_title | text | Place | static | OK | |
| dance | db_block_pin | button | Pin here | P:399 -> onBlockingPin(true) -> S:569 pinHere | OK | |
| dance | db_block_release | button | Release | P:400 -> onBlockingPin(false) -> S:575 releasePin | OK | |
| dance | db_block_f | button | F | P:405 -> onBlockingNudge(+0.25,0,0) P:401 -> S:515 | OK | Dancer-local frame |
| dance | db_block_b | button | B | P:405 -> (-0.25,0,0) | OK | |
| dance | db_block_l | button | L | P:405 -> (0,+0.25,0) | OK | |
| dance | db_block_r | button | R | P:405 -> (0,-0.25,0) | OK | |
| dance | db_block_u | button | U | P:405 -> (0,0,+0.25) | OK | |
| dance | db_block_d | button | D | P:405 -> (0,0,-0.25) | OK | |
| dance | db_block_nudge_note | text | 0.25 m in dancer frame | static | OK | Matches P:401-403 |
| dance | db_block_formation_title | text | Formation | static | OK | |
| dance | db_block_shape | combo_box | Circle in/out, Arc, Line, Grid | read at P:1593 | OK | Values 0..4 match Formation enum (alavatarstagingmodel.h:51-57) |
| dance | db_block_spacing | spinner | Size | read at P:1594 -> S:586 applyFormation | OK | Value 2 within 0.25..32 |
| dance | db_block_centre | combo_box | Me / Camera focus / Subject A | read at P:1569 -> E:477 | OK | Values 0..2 match AutoJoinFocus |
| dance | db_block_set_centre | button | Set centre here | P:406 -> onBlockingSetCentre P:1566 | OK | |
| dance | db_block_clear_centre | button | Clear centre | P:407 -> E:485 | OK | |
| dance | db_block_apply_selected | button | Apply to selected | P:408 -> onBlockingFormation(false) P:1579 | OK | |
| dance | db_block_apply_all | button | Apply to all | P:409 -> onBlockingFormation(true) | OK | All non-self dancers (P:1586-1587) |
| dance | db_block_centre_status | text | centre: none | written P:1448 | OK | Matches XML default |
| dance | db_block_reset_selected | button | Reset selected | P:410 -> onBlockingReset(false) | OK | |
| dance | db_block_reset_all | button | Reset all | P:411 -> E:474 resetAllBlocking | OK | |
| dance | db_blocking_on | check_box | Blocking on | P:412 -> onBlockingEnabled P:1606 -> S:735 setEnabled | OK | Engine default true (alavatarstaging.h:187) matches XML |
| dance | db_cinema_tab | panel | Cinema | static | OK | |
| dance | db_wave_title | text | Wave / ripple | static | OK | |
| dance | db_wave_enable | check_box | Enable wave | P:383 -> onWaveChanged P:1931 -> E:185 setWave | OK | Consumed E:1501 |
| dance | db_wave_from_label | text | from | static | OK | |
| dance | db_wave_focus | combo_box | Me / Camera focus / Subject A | P:384 -> mWave.mFocus | OK | Values 0..2; consumed E:1417 |
| dance | db_wave_speed | slider | Spread (m/s) | P:385 -> mSpreadMetersPerSecond | OK | 0.5..100, default 10 (aldanceball.h:90); consumed E:1504-1506 |
| dance | db_wave_dir_label | text | Direction | static | OK | |
| dance | db_wave_dir | combo_box | Outward / Inward | P:386 -> mDirection | OK | Values 0/1 match WaveDirection |
| dance | db_wave_apply_label | text | Apply on | static | OK | |
| dance | db_wave_apply | combo_box | Start only / Switch only / Start and switch | P:387 -> mApply | OK | Values 0..2 match WaveApply; E:1502 |
| dance | db_ramp_title | text | Speed ramp | static | OK | |
| dance | db_ramp_seconds | spinner | Seconds | read at P:1949 -> rampSpeed E:155 | OK | Initial 0 within 0..120 |
| dance | db_ramp_go | button | Go | P:388 -> onRampGo P:1946 | OK | Target spinner issue is the RANGE row above |
| dance | db_session_title | text | Session | static | OK | |
| dance | db_export | button | Export | P:389 -> onExport P:1954 -> E:2063 exportSession | OK | File picker plus write |
| dance | db_import | button | Import | P:390 -> onImport P:1986 -> E:2126 importSession | OK | |
| dance | db_session_note | text | Sessions are saved only when you export | static | OK | Consistent with manual export/import |
| dance | db_message | text | message strip | setMessage P:546 | OK | |

Dance panel counts: OK 109, RANGE 2, LABEL-MISMATCH 2, all others 0 (113 named widgets including the root; combo items verified within their combo rows).

## Still pending

Three auditors are still running: panel_lens_gaze (gaze page), panel_ghost_studio (ghosts page), and panel_prop_mover plus panel_path_editor (props and path pages). Their rows are added when they report.
