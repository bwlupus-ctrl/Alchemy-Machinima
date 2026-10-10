# Path — Director Kit page contract

Page key: `path_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (4/62 setting-bound rows; 60 C++-referenced rows).

## Real owners

- `panel_path_editor` / `panel` at `indra/newview/skins/default/xui/en/panel_path_editor.xml:9` (62 matching rows).
- `panel_cinecam_params` / `panel` at `indra/newview/skins/default/xui/en/panel_cinecam_params.xml:10` (3 matching rows).
- `hint` / `hint_popup` at `indra/newview/skins/default/xui/en/widgets/hint_popup.xml:3` (1 matching rows).
- Referencing classes: `ALPanelPathEditor` (93), `LLActorMover` (8), `LLEmojiHelper` (1), `LLFloaterEmojiPicker` (1), `LLFloaterEmojiComplete` (1).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing waypoint `scroll_list`, buttons, combos, and spinners as kit rows. There is no path-canvas class to embed.

## Row bindings

`ALPathKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `edit_path` / `hint` (Tip) | `text` `indra/newview/skins/default/xui/en/panel_cinecam_params.xml:669`, `text` `indra/newview/skins/default/xui/en/panel_cinecam_params.xml:687`, `text` `indra/newview/skins/default/xui/en/panel_cinecam_params.xml:1239`, `text` `indra/newview/skins/default/xui/en/panel_path_editor.xml:41`, `hint_popup` `indra/newview/skins/default/xui/en/widgets/hint_popup.xml:3` | `ALPathKitAdapter` command/model binding → `LLEmojiHelper::showHelper()` (`indra/llui/llemojihelper.cpp:136`), `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:140`), `LLFloaterEmojiPicker::onOpen()` (`indra/newview/llfloateremojipicker.cpp:366`), `?::?()` (`indra/newview/llnotificationhinthandler.cpp:37`), `LLFloaterEmojiComplete::onOpen()` (`indra/newview/llpanelemojicomplete.cpp:534`) | none |
| `waypoints` / `waypoint_list` (Points) | `scroll_list` `indra/newview/skins/default/xui/en/panel_path_editor.xml:69` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:81`) | none |
| `waypoints` / `btn_wp_add` (Add at actor) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:156` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:82`) | none |
| `waypoints` / `btn_wp_insert` (Insert after) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:157` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:83`) | none |
| `waypoints` / `btn_wp_delete` (Delete node) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:158` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:84`) | none |
| `waypoints` / `btn_wp_clear` (Clear path) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:159` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:85`) | none |
| `path_edits` / `btn_edit_undo` (Undo) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:161` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:88`) | none |
| `path_edits` / `btn_edit_redo` (Redo) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:162` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:89`) | none |
| `path_edits` / `btn_edit_reverse` (Reverse) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:163` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:90`) | none |
| `path_edits` / `btn_edit_mirror` (Mirror) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:164` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:91`) | none |
| `path_edits` / `btn_edit_loopclose` (Close loop) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:165` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:92`) | none |
| `play_path` / `btn_play_path` (Play path) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:172` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:94`) | none |
| `play_path` / `btn_stop_path` (Stop path) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:173` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:95`) | none |
| `play_path` / `btn_walk_here` (Walk to point) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:170` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:93`) | none |
| `copy_to_cast` / `copy_to_combo` (Member) | `combo_box` `indra/newview/skins/default/xui/en/panel_path_editor.xml:168` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:96`) | none |
| `copy_to_cast` / `btn_copy_to` (Copy path) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:169` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:97`) | none |
| `pose_ghosts` / `ghost_style_combo` (Look) | `combo_box` `indra/newview/skins/default/xui/en/panel_path_editor.xml:90` | kit `combo` ↔ `PathGhostStyle` `S32`; `ALPathKitAdapter` preserves/calls `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:226`) | none |
| `pose_ghosts` / `ghost_distort_combo` (Distortion) | `combo_box` `indra/newview/skins/default/xui/en/panel_path_editor.xml:125` | kit `combo` ↔ `PathGhostDistort` `S32`; `ALPathKitAdapter` preserves/calls `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:228`) | none |
| `pose_ghosts` / `ghost_distort_amount` (Amount) | `slider` `indra/newview/skins/default/xui/en/panel_path_editor.xml:143` | kit `slider` ↔ `PathGhostDistortAmount` `F32`; `ALPathKitAdapter` preserves/calls `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:229`) | none |
| `pose_ghosts` / `ghost_effect_fps` (FX fps) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:150` | kit `number` ↔ `PathGhostEffectFps` `S32`; `ALPathKitAdapter` preserves/calls `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:227`) | none |
| `selected_node` / `node_height_spinner` (Height (m)) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:181` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:98`), `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:230`) | none |
| `selected_node` / `node_dwell_spinner` (Dwell (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:183` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:99`), `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:231`) | none |
| `selected_node` / `node_speed_spinner` (Speed (m/s)) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:185` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:100`), `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:232`) | none |
| `selected_node` / `node_corner_radius_spinner` (Corner (m)) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:187` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:101`), `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:233`) | none |
| `selected_node` / `node_anim_editor` (Anim UUID) | `line_editor` `indra/newview/skins/default/xui/en/panel_path_editor.xml:190` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:102`), `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:234`) | none |
| `node_camera` / `btn_node_setcam` (Capture cam) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:196` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:126`) | none |
| `node_camera` / `btn_node_clearcam` (Clear cam) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:197` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:127`) | none |
| `node_camera` / `node_cam_transition_combo` (Transition) | `combo_box` `indra/newview/skins/default/xui/en/panel_path_editor.xml:198` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:129`), `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:236`) | none |
| `node_camera` / `btn_node_preview` (Preview shot) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:203` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:128`) | none |
| `node_camera` / `node_cam_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_path_editor.xml:204` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:130`) | none |
| `walk_timing` / `path_speed_spinner` (Travel (m/s)) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:211` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:103`), `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:237`) | none |
| `walk_timing` / `path_cadence_slider` (Foot cadence) | `slider` `indra/newview/skins/default/xui/en/panel_path_editor.xml:213` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:104`), `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:238`) | none |
| `walk_timing` / `path_easein_spinner` (Ease in (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:217` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:106`), `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:240`) | none |
| `walk_timing` / `path_easeout_spinner` (Ease out (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:219` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:107`), `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:241`) | none |
| `walk_timing` / `path_tension_slider` (Corner smooth) | `slider` `indra/newview/skins/default/xui/en/panel_path_editor.xml:215` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:105`), `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:239`) | none |
| `walk_timing` / `color_lbl` (Color) | `text` `indra/newview/skins/default/xui/en/panel_path_editor.xml:234` | kit readout ← approved static copy; no runtime writer | none |
| `ground_end` / `path_groundfollow_check` (Ground-follow) | `check_box` `indra/newview/skins/default/xui/en/panel_path_editor.xml:221` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:108`), `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:242`) | none |
| `ground_end` / `path_pitch_check` (Pitch to slope) | `check_box` `indra/newview/skins/default/xui/en/panel_path_editor.xml:223` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:109`), `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:243`) | none |
| `ground_end` / `path_airborne_check` (Air path) | `check_box` `indra/newview/skins/default/xui/en/panel_path_editor.xml:225` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:110`), `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:244`) | none |
| `ground_end` / `path_end_combo` (At the end) | `combo_box` `indra/newview/skins/default/xui/en/panel_path_editor.xml:227` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:111`), `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:245`) | none |
| `shape` / `path_shape_combo` (Shape) | `combo_box` `indra/newview/skins/default/xui/en/panel_path_editor.xml:243` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:113`), `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:246`) | none |
| `shape` / `btn_primitive_center` (Center) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:253` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:114`) | none |
| `shape` / `btn_fit_half_arc` (Half arc) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:256` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:115`) | none |
| `shape` / `btn_fit_sine` (Sine fit) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:257` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:116`) | none |
| `shape` / `endpoint_generator_lbl` (Fit ends) | `text` `indra/newview/skins/default/xui/en/panel_path_editor.xml:255` | kit readout ← approved static copy; no runtime writer | none |
| `size_tilt` / `primitive_radius_x` (Extent X (m)) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:259` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:117`), `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:247`), `ALPanelPathEditor::onResetControl()` (`indra/newview/alpanelpatheditor.cpp:519`), `LLActorMover::pathSceneData()` (`indra/newview/llactormover.cpp:1875`), `?::?()` (`indra/newview/llactormover.cpp:1975`) | none |
| `size_tilt` / `primitive_radius_y` (Extent Y (m)) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:261` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:118`), `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:248`), `ALPanelPathEditor::onResetControl()` (`indra/newview/alpanelpatheditor.cpp:523`), `LLActorMover::pathSceneData()` (`indra/newview/llactormover.cpp:1876`), `?::?()` (`indra/newview/llactormover.cpp:1977`) | none |
| `size_tilt` / `primitive_start_deg` (Start (deg)) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:263` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:119`), `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:249`), `LLActorMover::pathSceneData()` (`indra/newview/llactormover.cpp:1877`), `?::?()` (`indra/newview/llactormover.cpp:1979`) | none |
| `size_tilt` / `primitive_sweep_deg` (Sweep (deg)) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:265` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:120`), `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:250`), `LLActorMover::pathSceneData()` (`indra/newview/llactormover.cpp:1878`), `?::?()` (`indra/newview/llactormover.cpp:1981`) | none |
| `size_tilt` / `primitive_rise` (Rise (m)) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:273` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:124`), `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:254`), `ALPanelPathEditor::onResetControl()` (`indra/newview/alpanelpatheditor.cpp:547`), `LLActorMover::pathSceneData()` (`indra/newview/llactormover.cpp:1882`), `?::?()` (`indra/newview/llactormover.cpp:1989`) | none |
| `size_tilt` / `primitive_yaw_deg` (Plane yaw) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:267` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:121`), `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:251`), `LLActorMover::pathSceneData()` (`indra/newview/llactormover.cpp:1879`), `?::?()` (`indra/newview/llactormover.cpp:1983`) | none |
| `size_tilt` / `primitive_pitch_deg` (Plane pitch) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:269` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:122`), `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:252`), `LLActorMover::pathSceneData()` (`indra/newview/llactormover.cpp:1880`), `?::?()` (`indra/newview/llactormover.cpp:1985`) | none |
| `size_tilt` / `primitive_roll_deg` (Plane roll) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:271` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:123`), `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:253`), `LLActorMover::pathSceneData()` (`indra/newview/llactormover.cpp:1881`), `?::?()` (`indra/newview/llactormover.cpp:1987`) | none |
| `camera_sync` / `sync_offset_spinner` (Lead/trail (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:284` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:133`), `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:256`) | none |
| `camera_sync` / `sync_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_path_editor.xml:286` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:134`) | none |
| `follow_leader` / `follow_leader_combo` (Leader) | `combo_box` `indra/newview/skins/default/xui/en/panel_path_editor.xml:288` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:135`), `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:257`) | none |
| `follow_leader` / `follow_offset_spinner` (Behind (m)) | `spinner` `indra/newview/skins/default/xui/en/panel_path_editor.xml:291` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:137`), `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:258`) | none |
| `follow_leader` / `btn_stop_following` (Stop following) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:290` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:136`) | none |
| `follow_leader` / `follow_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_path_editor.xml:293` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:138`) | none |
| `suspended` / `btn_suspend_resume` (Resume) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:303` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:143`) | none |
| `suspended` / `btn_suspend_reanchor` (Re-anchor here) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:304` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:144`) | none |
| `suspended` / `btn_suspend_cancel` (Cancel walk) | `button` `indra/newview/skins/default/xui/en/panel_path_editor.xml:305` | `ALPathKitAdapter` command/model binding → `ALPanelPathEditor::postBuild()` (`indra/newview/alpanelpatheditor.cpp:145`) | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `LLEmojiHelper::showHelper()` — `indra/llui/llemojihelper.cpp:136` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::onResetControl()` — `indra/newview/alpanelpatheditor.cpp:519, 523, 547` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelPathEditor::postBuild()` — `indra/newview/alpanelpatheditor.cpp:81, 82, 83, 84, 85, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 126, 127, 128, 129, 130, 133, 134, 135, 136, 137, 138, 140, 143, 144, 145, 226, 227, 228, 229, 230, 231, 232, 233, 234, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 256, 257, 258` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `?::?()` — `indra/newview/llactormover.cpp:1975, 1977, 1979, 1981, 1983, 1985, 1987, 1989` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLActorMover::pathSceneData()` — `indra/newview/llactormover.cpp:1875, 1876, 1877, 1878, 1879, 1880, 1881, 1882` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterEmojiPicker::onOpen()` — `indra/newview/llfloateremojipicker.cpp:366` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `?::?()` — `indra/newview/llnotificationhinthandler.cpp:37` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterEmojiComplete::onOpen()` — `indra/newview/llpanelemojicomplete.cpp:534` | Move model-facing work into `ALPathKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

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

## New widget required

None. Every approved visual kind is either a kit primitive or an existing XUI control. Missing action/model bindings below are USER DECISIONS, not permission to invent a visual class.

## Unresolved and USER DECISION items

- `color_lbl` (Color): no C++ reference or setting binding.
- `endpoint_generator_lbl` (Fit ends): no C++ reference or setting binding.

## Risks

- A widget-name collision can select an unrelated XUI root or C++ literal; ownership must be resolved by root, class, and behavior together.
- Extracting model work without rewiring the existing owner creates divergent v1/v2 behavior; the extraction and forwarding call are one reviewed change.
- Duplicate writable settings must obey the rollout homes table; this page emits a link/read-only row when it is not the chosen home.
- Shared edit models require owner tokens, and external presets/cue playback require one global history barrier at start/stop rather than per write.
- Unresolved rows fail generation closed; they are never silently replaced with a guessed widget or command.
