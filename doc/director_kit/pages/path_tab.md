# Path — Director Kit page contract

Page key: `path_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (4/62 setting-bound rows; 60 C++-referenced rows).

## Real owners

- `panel_path_editor` / `panel` at `indra/newview/skins/default/xui/en/panel_path_editor.xml:9` (62 matching rows).
- Referencing classes: `ALPanelPathEditor` (145).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing waypoint `scroll_list`, buttons, combos, and spinners as kit rows. There is no path-canvas class to embed.

## Row bindings

`ALPathKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `edit_path` / `hint` (Tip) | `text` `indra/newview/skins/default/xui/en/panel_path_editor.xml:41` | kit readout ← approved static copy; no runtime writer | none |
| `waypoints` / `waypoint_list` (Points) | `scroll_list` `indra/newview/skins/default/xui/en/panel_path_editor.xml:69` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onListSelect()` (`indra/newview/alpanelpatheditor.cpp:159`) | none |
| `waypoints` / `btn_wp_add` (Add at actor) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:156` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onClickAdd()` (`indra/newview/alpanelpatheditor.cpp:160`) | none |
| `waypoints` / `btn_wp_insert` (Insert after) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:157` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onClickInsert()` (`indra/newview/alpanelpatheditor.cpp:161`) | none |
| `waypoints` / `btn_wp_delete` (Delete node) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:158` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onClickDelete()` (`indra/newview/alpanelpatheditor.cpp:162`) | none |
| `waypoints` / `btn_wp_clear` (Clear path) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:159` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onClickClear()` (`indra/newview/alpanelpatheditor.cpp:163`) | none |
| `path_edits` / `btn_edit_undo` (Undo) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:161` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onClickUndo()` (`indra/newview/alpanelpatheditor.cpp:165`) | none |
| `path_edits` / `btn_edit_redo` (Redo) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:162` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onClickRedo()` (`indra/newview/alpanelpatheditor.cpp:166`) | none |
| `path_edits` / `btn_edit_reverse` (Reverse) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:163` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onClickReverse()` (`indra/newview/alpanelpatheditor.cpp:167`) | none |
| `path_edits` / `btn_edit_mirror` (Mirror) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:164` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onClickMirror()` (`indra/newview/alpanelpatheditor.cpp:168`) | none |
| `path_edits` / `btn_edit_loopclose` (Close loop) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:165` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onClickLoopClose()` (`indra/newview/alpanelpatheditor.cpp:169`) | none |
| `play_path` / `btn_play_path` (Play path) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:172` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onClickPlayPath()` (`indra/newview/alpanelpatheditor.cpp:171`) | none |
| `play_path` / `btn_stop_path` (Stop path) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:173` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onClickStopPath()` (`indra/newview/alpanelpatheditor.cpp:172`) | none |
| `play_path` / `btn_walk_here` (Walk to point) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:170` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onClickWalkHere()` (`indra/newview/alpanelpatheditor.cpp:170`) | none |
| `copy_to_cast` / `copy_to_combo` (Member) | `combo_box` `indra/newview/skins/default/xui/en/panel_path_editor.xml:168` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::refreshCopyCombo()` (`indra/newview/alpanelpatheditor.cpp:1536`) | none |
| `copy_to_cast` / `btn_copy_to` (Copy path) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:169` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onClickCopyTo()` (`indra/newview/alpanelpatheditor.cpp:173`) | none |
| `pose_ghosts` / `ghost_style_combo` (Look) | `combo_box` `indra/newview/skins/default/xui/en/panel_path_editor.xml:90` | kit `combo` ↔ `PathGhostStyle` `S32` | none |
| `pose_ghosts` / `ghost_distort_combo` (Distortion) | `combo_box` `indra/newview/skins/default/xui/en/panel_path_editor.xml:125` | kit `combo` ↔ `PathGhostDistort` `S32` | none |
| `pose_ghosts` / `ghost_distort_amount` (Amount) | `slider` `indra/newview/skins/default/xui/en/panel_path_editor.xml:143` | kit `slider` ↔ `PathGhostDistortAmount` `F32` | none |
| `pose_ghosts` / `ghost_effect_fps` (FX fps) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:150` | kit `number` ↔ `PathGhostEffectFps` `S32` | none |
| `selected_node` / `node_height_spinner` (Height (m)) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:181` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onNodeHeightCommit()` (`indra/newview/alpanelpatheditor.cpp:175`) | none |
| `selected_node` / `node_dwell_spinner` (Dwell (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:183` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onNodeDwellCommit()` (`indra/newview/alpanelpatheditor.cpp:176`) | none |
| `selected_node` / `node_speed_spinner` (Speed (m/s)) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:185` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onNodeSpeedCommit()` (`indra/newview/alpanelpatheditor.cpp:177`) | none |
| `selected_node` / `node_corner_radius_spinner` (Corner (m)) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:187` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onNodeCornerRadiusCommit()` (`indra/newview/alpanelpatheditor.cpp:178`) | none |
| `selected_node` / `node_anim_editor` (Anim UUID) | `line_editor` `indra/newview/skins/default/xui/en/panel_path_editor.xml:190` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onNodeAnimCommit()` (`indra/newview/alpanelpatheditor.cpp:180`) | none |
| `node_camera` / `btn_node_setcam` (Capture cam) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:196` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onClickSetCam()` (`indra/newview/alpanelpatheditor.cpp:204`) | none |
| `node_camera` / `btn_node_clearcam` (Clear cam) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:197` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onClickClearCam()` (`indra/newview/alpanelpatheditor.cpp:205`) | none |
| `node_camera` / `node_cam_transition_combo` (Transition) | `combo_box` `indra/newview/skins/default/xui/en/panel_path_editor.xml:198` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onCamTransitionCommit()` (`indra/newview/alpanelpatheditor.cpp:207`) | none |
| `node_camera` / `btn_node_preview` (Preview shot) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:203` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onClickPreview()` (`indra/newview/alpanelpatheditor.cpp:206`) | none |
| `node_camera` / `node_cam_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_path_editor.xml:204` | kit readout ← approved static copy; no runtime writer | none |
| `walk_timing` / `path_speed_spinner` (Travel (m/s)) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:211` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onPathSpeedCommit()` (`indra/newview/alpanelpatheditor.cpp:182`) | none |
| `walk_timing` / `path_cadence_slider` (Foot cadence) | `slider` `indra/newview/skins/default/xui/en/panel_path_editor.xml:213` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onPathCadenceCommit()` (`indra/newview/alpanelpatheditor.cpp:183`) | none |
| `walk_timing` / `path_easein_spinner` (Ease in (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:217` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onPathEaseInCommit()` (`indra/newview/alpanelpatheditor.cpp:185`) | none |
| `walk_timing` / `path_easeout_spinner` (Ease out (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:219` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onPathEaseOutCommit()` (`indra/newview/alpanelpatheditor.cpp:186`) | none |
| `walk_timing` / `path_tension_slider` (Corner smooth) | `slider` `indra/newview/skins/default/xui/en/panel_path_editor.xml:215` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onPathTensionCommit()` (`indra/newview/alpanelpatheditor.cpp:184`) | none |
| `walk_timing` / `color_lbl` (Color) | `text` `indra/newview/skins/default/xui/en/panel_path_editor.xml:234` | kit readout ← approved static copy; no runtime writer | none |
| `ground_end` / `path_groundfollow_check` (Ground-follow) | `check_box` `indra/newview/skins/default/xui/en/panel_path_editor.xml:221` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onPathGroundFollowCommit()` (`indra/newview/alpanelpatheditor.cpp:187`) | none |
| `ground_end` / `path_pitch_check` (Pitch to slope) | `check_box` `indra/newview/skins/default/xui/en/panel_path_editor.xml:223` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onPathPitchCommit()` (`indra/newview/alpanelpatheditor.cpp:188`) | none |
| `ground_end` / `path_airborne_check` (Air path) | `check_box` `indra/newview/skins/default/xui/en/panel_path_editor.xml:225` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onPathAirborneCommit()` (`indra/newview/alpanelpatheditor.cpp:189`) | none |
| `ground_end` / `path_end_combo` (At the end) | `combo_box` `indra/newview/skins/default/xui/en/panel_path_editor.xml:227` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onPathEndCommit()` (`indra/newview/alpanelpatheditor.cpp:190`) | none |
| `shape` / `path_shape_combo` (Shape) | `combo_box` `indra/newview/skins/default/xui/en/panel_path_editor.xml:243` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onPathShapeCommit()` (`indra/newview/alpanelpatheditor.cpp:191`) | none |
| `shape` / `btn_primitive_center` (Center) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:253` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onClickCenterPrimitive()` (`indra/newview/alpanelpatheditor.cpp:192`) | none |
| `shape` / `btn_fit_half_arc` (Half arc) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:256` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onClickFitHalfArc()` (`indra/newview/alpanelpatheditor.cpp:193`) | none |
| `shape` / `btn_fit_sine` (Sine fit) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:257` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onClickFitSine()` (`indra/newview/alpanelpatheditor.cpp:194`) | none |
| `shape` / `endpoint_generator_lbl` (Fit ends) | `text` `indra/newview/skins/default/xui/en/panel_path_editor.xml:255` | kit readout ← approved static copy; no runtime writer | none |
| `size_tilt` / `primitive_radius_x` (Extent X (m)) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:259` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onPrimitiveCommit()` (`indra/newview/alpanelpatheditor.cpp:195`), behavior `ALPanelPathEditor::onResetControl()` (`indra/newview/alpanelpatheditor.cpp:581`) | none |
| `size_tilt` / `primitive_radius_y` (Extent Y (m)) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:261` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onPrimitiveCommit()` (`indra/newview/alpanelpatheditor.cpp:196`), behavior `ALPanelPathEditor::onResetControl()` (`indra/newview/alpanelpatheditor.cpp:585`) | none |
| `size_tilt` / `primitive_start_deg` (Start (deg)) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:263` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onPrimitiveCommit()` (`indra/newview/alpanelpatheditor.cpp:197`) | none |
| `size_tilt` / `primitive_sweep_deg` (Sweep (deg)) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:265` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onPrimitiveCommit()` (`indra/newview/alpanelpatheditor.cpp:198`) | none |
| `size_tilt` / `primitive_rise` (Rise (m)) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:273` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onPrimitiveCommit()` (`indra/newview/alpanelpatheditor.cpp:202`), behavior `ALPanelPathEditor::onResetControl()` (`indra/newview/alpanelpatheditor.cpp:609`) | none |
| `size_tilt` / `primitive_yaw_deg` (Plane yaw) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:267` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onPrimitiveCommit()` (`indra/newview/alpanelpatheditor.cpp:199`) | none |
| `size_tilt` / `primitive_pitch_deg` (Plane pitch) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:269` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onPrimitiveCommit()` (`indra/newview/alpanelpatheditor.cpp:200`) | none |
| `size_tilt` / `primitive_roll_deg` (Plane roll) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:271` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onPrimitiveCommit()` (`indra/newview/alpanelpatheditor.cpp:201`) | none |
| `camera_sync` / `sync_offset_spinner` (Lead/trail (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:284` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onSyncOffsetCommit()` (`indra/newview/alpanelpatheditor.cpp:214`) | none |
| `camera_sync` / `sync_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_path_editor.xml:286` | kit readout ← approved static copy; no runtime writer | none |
| `follow_leader` / `follow_leader_combo` (Leader) | `combo_box` `indra/newview/skins/default/xui/en/panel_path_editor.xml:288` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onFollowCommit()` (`indra/newview/alpanelpatheditor.cpp:215`) | none |
| `follow_leader` / `follow_offset_spinner` (Behind (m)) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:291` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onFollowOffsetCommit()` (`indra/newview/alpanelpatheditor.cpp:217`) | none |
| `follow_leader` / `btn_stop_following` (Stop following) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:290` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onStopFollow()` (`indra/newview/alpanelpatheditor.cpp:216`) | none |
| `follow_leader` / `follow_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_path_editor.xml:293` | kit readout ← approved static copy; no runtime writer | none |
| `suspended` / `btn_suspend_resume` (Resume) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:304` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onClickResume()` (`indra/newview/alpanelpatheditor.cpp:209`) | none |
| `suspended` / `btn_suspend_reanchor` (Re-anchor here) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:305` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onClickReanchor()` (`indra/newview/alpanelpatheditor.cpp:210`) | none |
| `suspended` / `btn_suspend_cancel` (Cancel walk) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:306` | `ALPathKitAdapter` command/model binding → wired in `ALPanelPathEditor::postBuild()`; behavior `ALPanelPathEditor::onClickCancelSuspend()` (`indra/newview/alpanelpatheditor.cpp:211`) | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALPanelPathEditor::onCamTransitionCommit()` — `indra/newview/alpanelpatheditor.cpp:207` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onClickAdd()` — `indra/newview/alpanelpatheditor.cpp:160` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onClickCancelSuspend()` — `indra/newview/alpanelpatheditor.cpp:211` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onClickCenterPrimitive()` — `indra/newview/alpanelpatheditor.cpp:192` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onClickClear()` — `indra/newview/alpanelpatheditor.cpp:163` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onClickClearCam()` — `indra/newview/alpanelpatheditor.cpp:205` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onClickCopyTo()` — `indra/newview/alpanelpatheditor.cpp:173` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onClickDelete()` — `indra/newview/alpanelpatheditor.cpp:162` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onClickFitHalfArc()` — `indra/newview/alpanelpatheditor.cpp:193` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onClickFitSine()` — `indra/newview/alpanelpatheditor.cpp:194` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onClickInsert()` — `indra/newview/alpanelpatheditor.cpp:161` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onClickLoopClose()` — `indra/newview/alpanelpatheditor.cpp:169` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onClickMirror()` — `indra/newview/alpanelpatheditor.cpp:168` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onClickPlayPath()` — `indra/newview/alpanelpatheditor.cpp:171` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onClickPreview()` — `indra/newview/alpanelpatheditor.cpp:206` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onClickReanchor()` — `indra/newview/alpanelpatheditor.cpp:210` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onClickRedo()` — `indra/newview/alpanelpatheditor.cpp:166` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onClickResume()` — `indra/newview/alpanelpatheditor.cpp:209` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onClickReverse()` — `indra/newview/alpanelpatheditor.cpp:167` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onClickSetCam()` — `indra/newview/alpanelpatheditor.cpp:204` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onClickStopPath()` — `indra/newview/alpanelpatheditor.cpp:172` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onClickUndo()` — `indra/newview/alpanelpatheditor.cpp:165` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onClickWalkHere()` — `indra/newview/alpanelpatheditor.cpp:170` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onFollowCommit()` — `indra/newview/alpanelpatheditor.cpp:215` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onFollowOffsetCommit()` — `indra/newview/alpanelpatheditor.cpp:217` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onListSelect()` — `indra/newview/alpanelpatheditor.cpp:159` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onNodeAnimCommit()` — `indra/newview/alpanelpatheditor.cpp:180` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onNodeCornerRadiusCommit()` — `indra/newview/alpanelpatheditor.cpp:178` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onNodeDwellCommit()` — `indra/newview/alpanelpatheditor.cpp:176` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onNodeHeightCommit()` — `indra/newview/alpanelpatheditor.cpp:175` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onNodeSpeedCommit()` — `indra/newview/alpanelpatheditor.cpp:177` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onPathAirborneCommit()` — `indra/newview/alpanelpatheditor.cpp:189` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onPathCadenceCommit()` — `indra/newview/alpanelpatheditor.cpp:183` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onPathEaseInCommit()` — `indra/newview/alpanelpatheditor.cpp:185` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onPathEaseOutCommit()` — `indra/newview/alpanelpatheditor.cpp:186` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onPathEndCommit()` — `indra/newview/alpanelpatheditor.cpp:190` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onPathGroundFollowCommit()` — `indra/newview/alpanelpatheditor.cpp:187` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onPathPitchCommit()` — `indra/newview/alpanelpatheditor.cpp:188` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onPathShapeCommit()` — `indra/newview/alpanelpatheditor.cpp:191` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onPathSpeedCommit()` — `indra/newview/alpanelpatheditor.cpp:182` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onPathTensionCommit()` — `indra/newview/alpanelpatheditor.cpp:184` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onPrimitiveCommit()` — `indra/newview/alpanelpatheditor.cpp:195, 196, 197, 198, 199, 200, 201, 202` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onResetControl()` — `indra/newview/alpanelpatheditor.cpp:581, 585, 609` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onStopFollow()` — `indra/newview/alpanelpatheditor.cpp:216` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onSyncOffsetCommit()` — `indra/newview/alpanelpatheditor.cpp:214` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::refreshCopyCombo()` — `indra/newview/alpanelpatheditor.cpp:1536` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **102 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-edit_path-tallest-850` | Only `edit_path` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-edit_path-tallest-1160` | Only `edit_path` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-waypoints-tallest-850` | Only `waypoints` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-waypoints-tallest-1160` | Only `waypoints` is put in its tallest local state at 1160 px; all other cards stay default. |
| `waypoint_list-empty` | `waypoint_list` is empty with no selection. |
| `waypoint_list-one` | `waypoint_list` has one selected item. |
| `waypoint_list-overflow` | `waypoint_list` has 51 items and a visible scrollbar. |
| `card-path_edits-tallest-850` | Only `path_edits` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-path_edits-tallest-1160` | Only `path_edits` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-play_path-tallest-850` | Only `play_path` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-play_path-tallest-1160` | Only `play_path` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-copy_to_cast-tallest-850` | Only `copy_to_cast` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-copy_to_cast-tallest-1160` | Only `copy_to_cast` is put in its tallest local state at 1160 px; all other cards stay default. |
| `copy_to_combo-option-0` | Only `copy_to_combo` selects `Selected cast`; other selectors remain default. |
| `copy_to_combo-option-1` | Only `copy_to_combo` selects `Mara`; other selectors remain default. |
| `copy_to_combo-option-2` | Only `copy_to_combo` selects `Jun`; other selectors remain default. |
| `copy_to_combo-option-3` | Only `copy_to_combo` selects `Me`; other selectors remain default. |
| `card-pose_ghosts-tallest-850` | Only `pose_ghosts` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-pose_ghosts-tallest-1160` | Only `pose_ghosts` is put in its tallest local state at 1160 px; all other cards stay default. |
| `ghost_style_combo-option-0` | Only `ghost_style_combo` selects `Ghost`; other selectors remain default. |
| `ghost_style_combo-option-1` | Only `ghost_style_combo` selects `Clone`; other selectors remain default. |
| `ghost_style_combo-option-2` | Only `ghost_style_combo` selects `Hologram`; other selectors remain default. |
| `ghost_style_combo-option-3` | Only `ghost_style_combo` selects `Wireframe`; other selectors remain default. |
| `ghost_style_combo-option-4` | Only `ghost_style_combo` selects `X-ray`; other selectors remain default. |
| `ghost_style_combo-option-5` | Only `ghost_style_combo` selects `Thermal`; other selectors remain default. |
| `ghost_style_combo-option-6` | Only `ghost_style_combo` selects `Neon Outline`; other selectors remain default. |
| `ghost_style_combo-option-7` | Only `ghost_style_combo` selects `Silhouette`; other selectors remain default. |
| `ghost_style_combo-option-8` | Only `ghost_style_combo` selects `Toon/Ink`; other selectors remain default. |
| `ghost_style_combo-option-9` | Only `ghost_style_combo` selects `Chrome`; other selectors remain default. |
| `ghost_style_combo-option-10` | Only `ghost_style_combo` selects `Dissolve`; other selectors remain default. |
| `ghost_style_combo-option-11` | Only `ghost_style_combo` selects `Negative`; other selectors remain default. |
| `ghost_style_combo-option-12` | Only `ghost_style_combo` selects `Gold Statue`; other selectors remain default. |
| `ghost_style_combo-option-13` | Only `ghost_style_combo` selects `Night-vision`; other selectors remain default. |
| `ghost_style_combo-option-14` | Only `ghost_style_combo` selects `Blueprint`; other selectors remain default. |
| `ghost_style_combo-option-15` | Only `ghost_style_combo` selects `Ectoplasm`; other selectors remain default. |
| `ghost_style_combo-option-16` | Only `ghost_style_combo` selects `Frost/Ice`; other selectors remain default. |
| `ghost_style_combo-option-17` | Only `ghost_style_combo` selects `Prism`; other selectors remain default. |
| `ghost_style_combo-option-18` | Only `ghost_style_combo` selects `Thermal Scope`; other selectors remain default. |
| `ghost_style_combo-option-19` | Only `ghost_style_combo` selects `Wallhack/ESP`; other selectors remain default. |
| `ghost_style_combo-option-20` | Only `ghost_style_combo` selects `NV Tube`; other selectors remain default. |
| `ghost_style_combo-option-21` | Only `ghost_style_combo` selects `Damage`; other selectors remain default. |
| `ghost_style_combo-option-22` | Only `ghost_style_combo` selects `Killcam`; other selectors remain default. |
| `ghost_style_combo-option-23` | Only `ghost_style_combo` selects `Oil Slick`; other selectors remain default. |
| `ghost_style_combo-option-24` | Only `ghost_style_combo` selects `Vaporwave`; other selectors remain default. |
| `ghost_style_combo-option-25` | Only `ghost_style_combo` selects `Halftone`; other selectors remain default. |
| `ghost_style_combo-option-26` | Only `ghost_style_combo` selects `Sonar`; other selectors remain default. |
| `ghost_style_combo-option-27` | Only `ghost_style_combo` selects `Holo Echo`; other selectors remain default. |
| `ghost_distort_combo-option-0` | Only `ghost_distort_combo` selects `No distortion`; other selectors remain default. |
| `ghost_distort_combo-option-1` | Only `ghost_distort_combo` selects `Pixelate`; other selectors remain default. |
| `ghost_distort_combo-option-2` | Only `ghost_distort_combo` selects `Voxel`; other selectors remain default. |
| `ghost_distort_combo-option-3` | Only `ghost_distort_combo` selects `Lens`; other selectors remain default. |
| `ghost_distort_combo-option-4` | Only `ghost_distort_combo` selects `Wave`; other selectors remain default. |
| `ghost_distort_combo-option-5` | Only `ghost_distort_combo` selects `RGB Split`; other selectors remain default. |
| `ghost_distort_combo-option-6` | Only `ghost_distort_combo` selects `Block`; other selectors remain default. |
| `ghost_distort_combo-option-7` | Only `ghost_distort_combo` selects `V-Tear`; other selectors remain default. |
| `ghost_distort_combo-option-8` | Only `ghost_distort_combo` selects `VHS`; other selectors remain default. |
| `card-selected_node-tallest-850` | Only `selected_node` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-selected_node-tallest-1160` | Only `selected_node` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-node_camera-tallest-850` | Only `node_camera` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-node_camera-tallest-1160` | Only `node_camera` is put in its tallest local state at 1160 px; all other cards stay default. |
| `node_cam_transition_combo-option-0` | Only `node_cam_transition_combo` selects `Cut`; other selectors remain default. |
| `node_cam_transition_combo-option-1` | Only `node_cam_transition_combo` selects `Ease`; other selectors remain default. |
| `card-walk_timing-tallest-850` | Only `walk_timing` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-walk_timing-tallest-1160` | Only `walk_timing` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-ground_end-tallest-850` | Only `ground_end` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-ground_end-tallest-1160` | Only `ground_end` is put in its tallest local state at 1160 px; all other cards stay default. |
| `path_groundfollow_check-on` | Only `path_groundfollow_check` is on; dependent visibility is measured. |
| `path_pitch_check-on` | Only `path_pitch_check` is on; dependent visibility is measured. |
| `path_airborne_check-on` | Only `path_airborne_check` is on; dependent visibility is measured. |
| `path_end_combo-option-0` | Only `path_end_combo` selects `Stop at end`; other selectors remain default. |
| `path_end_combo-option-1` | Only `path_end_combo` selects `Stop + face target`; other selectors remain default. |
| `path_end_combo-option-2` | Only `path_end_combo` selects `Loop`; other selectors remain default. |
| `path_end_combo-option-3` | Only `path_end_combo` selects `Ping-pong`; other selectors remain default. |
| `card-shape-tallest-850` | Only `shape` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-shape-tallest-1160` | Only `shape` is put in its tallest local state at 1160 px; all other cards stay default. |
| `path_shape_combo-option-0` | Only `path_shape_combo` selects `Waypoints`; other selectors remain default. |
| `path_shape_combo-option-1` | Only `path_shape_combo` selects `Circle / Arc`; other selectors remain default. |
| `path_shape_combo-option-2` | Only `path_shape_combo` selects `Ellipse / Arc`; other selectors remain default. |
| `path_shape_combo-option-3` | Only `path_shape_combo` selects `Helix`; other selectors remain default. |
| `path_shape_combo-option-4` | Only `path_shape_combo` selects `Diamond`; other selectors remain default. |
| `path_shape_combo-option-5` | Only `path_shape_combo` selects `Figure 8`; other selectors remain default. |
| `path_shape_combo-option-6` | Only `path_shape_combo` selects `Sine Wave`; other selectors remain default. |
| `card-size_tilt-tallest-850` | Only `size_tilt` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-size_tilt-tallest-1160` | Only `size_tilt` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-camera_sync-tallest-850` | Only `camera_sync` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-camera_sync-tallest-1160` | Only `camera_sync` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-follow_leader-tallest-850` | Only `follow_leader` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-follow_leader-tallest-1160` | Only `follow_leader` is put in its tallest local state at 1160 px; all other cards stay default. |
| `follow_leader_combo-option-0` | Only `follow_leader_combo` selects `Selected cast`; other selectors remain default. |
| `follow_leader_combo-option-1` | Only `follow_leader_combo` selects `Mara`; other selectors remain default. |
| `follow_leader_combo-option-2` | Only `follow_leader_combo` selects `Jun`; other selectors remain default. |
| `follow_leader_combo-option-3` | Only `follow_leader_combo` selects `Me`; other selectors remain default. |
| `card-suspended-tallest-850` | Only `suspended` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-suspended-tallest-1160` | Only `suspended` is put in its tallest local state at 1160 px; all other cards stay default. |
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

74 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

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
