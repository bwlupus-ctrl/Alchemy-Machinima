# Ghosts — Director Kit page contract

Page key: `ghosts_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (4/125 setting-bound rows; 122 C++-referenced rows).

## Real owners

- `panel_ghost_studio` / `panel` at `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:10` (125 matching rows).
- Referencing classes: `ALPanelGhostStudio` (257), `ALGhostStudio` (3).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Reuse the existing preview/diagnostics controls with a 240 px maximum and visible-only, revision-driven refresh.

## Row bindings

`ALGhostStudioKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `ghost_list` / `show_all_check` (Show ghosts) | `check_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:18` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onShowAllToggle()` (`indra/newview/alpanelghoststudio.cpp:443`) | none |
| `ghost_list` / `edit_ghosts_check` (Edit ghosts) | `check_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:16` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onToggleEditMode()` (`indra/newview/alpanelghoststudio.cpp:444`) | none |
| `ghost_list` / `ghost_list` (Ghosts) | `scroll_list` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:23` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onListSelect()` (`indra/newview/alpanelghoststudio.cpp:445`), wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onListMouseUp()` (`indra/newview/alpanelghoststudio.cpp:446`) | none |
| `ghost_list` / `studio_status` (Count) | `text` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:511` | kit readout ← approved static copy; no runtime writer | none |
| `ghost_list` / `btn_ghost_dup` (Duplicate) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:42` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickDuplicate()` (`indra/newview/alpanelghoststudio.cpp:451`) | none |
| `ghost_list` / `btn_ghost_del` (Delete) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:44` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickDelete()` (`indra/newview/alpanelghoststudio.cpp:452`) | none |
| `ghost_list` / `btn_ghost_refresh` (Refresh) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:46` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickRefresh()` (`indra/newview/alpanelghoststudio.cpp:453`) | none |
| `ghost_add_copy` / `source_combo` (Copy from) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:33` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickAdd()` (`indra/newview/alpanelghoststudio.cpp:1977`) | none |
| `ghost_add_copy` / `clone_type_combo` (Type) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:35` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickAdd()` (`indra/newview/alpanelghoststudio.cpp:1977`) | none |
| `ghost_add_copy` / `btn_ghost_add` (Add) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:40` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickAdd()` (`indra/newview/alpanelghoststudio.cpp:449`) | none |
| `ghost_add_copy` / `btn_duplicate_in_place` (Duplicate here) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:104` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickDuplicateInPlace()` (`indra/newview/alpanelghoststudio.cpp:516`) | none |
| `ghost_add_copy` / `btn_copy_transform` (Copy) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:106` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickCopyTransform()` (`indra/newview/alpanelghoststudio.cpp:517`) | none |
| `ghost_add_copy` / `btn_paste_transform` (Paste) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:108` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickPasteTransform()` (`indra/newview/alpanelghoststudio.cpp:518`) | none |
| `ghost_position` / `ghost_name` (Name) | `line_editor` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:67` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onNameCommit()` (`indra/newview/alpanelghoststudio.cpp:450`) | none |
| `ghost_position` / `pos_x_spinner` (X) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:69` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onPosCommit()` (`indra/newview/alpanelghoststudio.cpp:455`) | none |
| `ghost_position` / `pos_y_spinner` (Y) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:71` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onPosCommit()` (`indra/newview/alpanelghoststudio.cpp:456`) | none |
| `ghost_position` / `pos_z_spinner` (Z) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:73` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onPosCommit()` (`indra/newview/alpanelghoststudio.cpp:457`) | none |
| `ghost_position` / `yaw_spinner` (Yaw (deg)) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:75` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onYawCommit()` (`indra/newview/alpanelghoststudio.cpp:462`) | none |
| `ghost_position` / `scale_spinner` (Scale) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:82` | kit `number` ↔ `TemporalWorldScale` `F32`; `ALGhostStudioKitAdapter` preserves/calls wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onScaleCommit()` (`indra/newview/alpanelghoststudio.cpp:463`). Contract note: page contract supplies omitted setting `TemporalWorldScale` | page `temporal_tab`, `indra/newview/skins/default/xui/en/panel_temporal_capture.xml:78` `scale_slider`, `indra/newview/skins/default/xui/en/panel_temporal_capture.xml:94` `scale_spinner` |
| `ghost_snap` / `btn_place` (Click to place) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:85` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickPlace()` (`indra/newview/alpanelghoststudio.cpp:510`) | none |
| `ghost_snap` / `btn_to_actor` (To actor) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:87` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickToActor()` (`indra/newview/alpanelghoststudio.cpp:511`) | none |
| `ghost_snap` / `btn_to_me` (To me) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:89` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickToMe()` (`indra/newview/alpanelghoststudio.cpp:512`) | none |
| `ghost_snap` / `btn_drop_ground` (Drop to ground) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:98` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickDrop()` (`indra/newview/alpanelghoststudio.cpp:513`) | none |
| `ghost_snap` / `btn_align_feet` (Align feet) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:100` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickAlignFeet()` (`indra/newview/alpanelghoststudio.cpp:514`) | none |
| `ghost_facing` / `look_target_combo` (Face target) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:92` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onLookTargetCommit()` (`indra/newview/alpanelghoststudio.cpp:519`) | none |
| `ghost_facing` / `btn_face_now` (Face now) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:94` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickFaceNow()` (`indra/newview/alpanelghoststudio.cpp:521`) | none |
| `ghost_facing` / `keep_facing_check` (Keep facing) | `check_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:96` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onKeepFacingCommit()` (`indra/newview/alpanelghoststudio.cpp:523`) | none |
| `ghost_facing` / `btn_reset_upright` (Upright) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:102` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickUpright()` (`indra/newview/alpanelghoststudio.cpp:515`) | none |
| `look` / `style_combo` (Look) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:125` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onStyleCommit()` (`indra/newview/alpanelghoststudio.cpp:526`) | none |
| `look` / `distort_combo` (Distortion) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:156` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onDistortCommit()` (`indra/newview/alpanelghoststudio.cpp:534`) | none |
| `look` / `distort_amount_slider` (Distort amt) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:188` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onDistortAmountCommit()` (`indra/newview/alpanelghoststudio.cpp:535`) | none |
| `look` / `entity_style_hint` (Note) | `text` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:120` | kit readout ← approved static copy; no runtime writer | none |
| `tint` / `actor_tint_check` (Actor hue) | `check_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:168` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onActorTintToggle()` (`indra/newview/alpanelghoststudio.cpp:527`) | none |
| `tint` / `hue_slider` (Hue) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:170` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onHueCommit()` (`indra/newview/alpanelghoststudio.cpp:528`) | none |
| `tint` / `alpha_slider` (Alpha) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:173` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onAlphaCommit()` (`indra/newview/alpanelghoststudio.cpp:529`) | none |
| `tint` / `brightness_slider` (Brightness) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:191` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onBrightnessCommit()` (`indra/newview/alpanelghoststudio.cpp:536`) | none |
| `motion_fx` / `pixel_slider` (Pixelate) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:176` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onPixelCommit()` (`indra/newview/alpanelghoststudio.cpp:532`) | none |
| `motion_fx` / `shimmer_amount_slider` (Shimmer) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:182` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onShimmerAmountCommit()` (`indra/newview/alpanelghoststudio.cpp:531`) | none |
| `motion_fx` / `shimmer_speed_slider` (Shimmer Hz) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:179` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onShimmerSpeedCommit()` (`indra/newview/alpanelghoststudio.cpp:530`) | none |
| `motion_fx` / `glitch_slider` (Glitch) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:185` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onGlitchCommit()` (`indra/newview/alpanelghoststudio.cpp:533`) | none |
| `motion_fx` / `effect_fps_spinner` (Effect fps) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:194` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onEffectFpsCommit()` (`indra/newview/alpanelghoststudio.cpp:537`) | none |
| `pose` / `btn_freeze` (Grab pose) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:209` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickFreeze()` (`indra/newview/alpanelghoststudio.cpp:540`) | none |
| `pose` / `btn_live` (Follow live) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:211` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickLive()` (`indra/newview/alpanelghoststudio.cpp:541`) | none |
| `pose` / `drive_mode_combo` (Drive) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:218` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onDriveModeCommit()` (`indra/newview/alpanelghoststudio.cpp:480`) | none |
| `pose` / `mirror_hold_check` (Hold on change) | `check_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:213` | kit `toggle` ↔ `GhostMirrorHoldOnSourceChange` `Boolean` | none |
| `pose` / `pose_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:217` | kit readout ← approved static copy; no runtime writer | none |
| `pose` / `mirror_repeat_held_check` (Repeat held) | `check_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:215` | kit `toggle` ↔ `GhostMirrorRepeatHeldAnimesh` `Boolean` | none |
| `chaos` / `chaos_slider` (Chaos amount) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:227` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onChaosCommit()` (`indra/newview/alpanelghoststudio.cpp:465`) | none |
| `chaos` / `anim_speed_spinner` (Anim speed) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:239` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onAnimSpeedCommit()` (`indra/newview/alpanelghoststudio.cpp:466`) | none |
| `entity` / `entity_look_combo` (Render look) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:230` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onEntityLookCommit()` (`indra/newview/alpanelghoststudio.cpp:538`) | none |
| `entity` / `physics_check` (Physics) | `check_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:242` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onPhysicsCommit()` (`indra/newview/alpanelghoststudio.cpp:477`) | none |
| `entity` / `anim_loop_mode_combo` (On finish) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:244` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onLoopModeCommit()` (`indra/newview/alpanelghoststudio.cpp:490`) | none |
| `playback` / `btn_anim_pause` (Pause) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:249` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickAnimPause()` (`indra/newview/alpanelghoststudio.cpp:478`) | none |
| `playback` / `btn_anim_resume` (Resume) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:251` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickAnimResume()` (`indra/newview/alpanelghoststudio.cpp:479`) | none |
| `playback` / `btn_anim_sync` (Sync) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:253` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickAnimSync()` (`indra/newview/alpanelghoststudio.cpp:491`) | none |
| `anim_asset` / `directed_anim_editor` (Anim UUID) | `line_editor` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:259` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onDirectedAnimCommit()` (`indra/newview/alpanelghoststudio.cpp:481`) | none |
| `anim_asset` / `animation_library_combo` (Inventory) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:255` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onAnimationLibraryCommit()` (`indra/newview/alpanelghoststudio.cpp:482`) | none |
| `anim_asset` / `btn_anim_library_refresh` (Refresh) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:257` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALGhostStudio::scanAnimLibrary()` (`indra/newview/alpanelghoststudio.cpp:483`) | none |
| `anim_asset` / `anim_metadata_text` (Metadata) | `text` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:261` | kit readout ← approved static copy; no runtime writer | none |
| `autoanim` / `autoanim_window_spinner` (Learn (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:265` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onAutoAnimWindow()` (`indra/newview/alpanelghoststudio.cpp:469`) | none |
| `autoanim` / `btn_autoanim_go` (Go autonomous) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:270` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onAutoAnimAction()` (`indra/newview/alpanelghoststudio.cpp:473`) | none |
| `autoanim` / `btn_autoanim_follow` (Follow source) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:271` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onAutoAnimAction()` (`indra/newview/alpanelghoststudio.cpp:474`) | none |
| `autoanim` / `btn_autoanim_stop` (Stop now) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:267` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onAutoAnimAction()` (`indra/newview/alpanelghoststudio.cpp:470`) | none |
| `autoanim` / `btn_autoanim_relearn` (Relearn) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:268` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onAutoAnimAction()` (`indra/newview/alpanelghoststudio.cpp:471`) | none |
| `autoanim` / `autoanim_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:274` | kit readout ← approved static copy; no runtime writer | none |
| `autoanim` / `btn_autoanim_reseed` (Reseed) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:269` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onAutoAnimAction()` (`indra/newview/alpanelghoststudio.cpp:472`) | none |
| `autoanim` / `btn_autoanim_save` (Save...) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:272` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onAutoAnimFile()` (`indra/newview/alpanelghoststudio.cpp:475`) | none |
| `autoanim` / `btn_autoanim_load` (Load...) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:273` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onAutoAnimFile()` (`indra/newview/alpanelghoststudio.cpp:476`) | none |
| `head_gaze` / `btn_lens_gaze_selection` (Gaze on) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:278` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickLensGazeSelection()` (`indra/newview/alpanelghoststudio.cpp:494`) | none |
| `head_gaze` / `lens_gaze_target_mode` (Target) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:283` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onLensGazeSettingsCommit()` (`indra/newview/alpanelghoststudio.cpp:498`) | none |
| `head_gaze` / `lens_gaze_cast_target` (Cast member) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:290` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onLensGazeSettingsCommit()` (`indra/newview/alpanelghoststudio.cpp:500`) | none |
| `head_gaze` / `lens_gaze_torso` (Torso) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:280` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onLensGazeTorsoCommit()` (`indra/newview/alpanelghoststudio.cpp:496`) | none |
| `head_gaze` / `lens_gaze_head_eye` (Head / eyes) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:292` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onLensGazeSettingsCommit()` (`indra/newview/alpanelghoststudio.cpp:502`) | none |
| `head_gaze` / `lens_gaze_intensity` (Intensity) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:295` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onLensGazeSettingsCommit()` (`indra/newview/alpanelghoststudio.cpp:504`) | none |
| `head_gaze` / `lens_gaze_smoothing` (Smoothing) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:298` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onLensGazeSettingsCommit()` (`indra/newview/alpanelghoststudio.cpp:506`) | none |
| `head_gaze` / `lens_gaze_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:301` | kit readout ← approved static copy; no runtime writer | none |
| `crowd_pattern` / `btn_array_line` (Line) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:325` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickArray()` (`indra/newview/alpanelghoststudio.cpp:545`) | none |
| `crowd_pattern` / `btn_array_ring` (Ring) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:327` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickArray()` (`indra/newview/alpanelghoststudio.cpp:547`) | none |
| `crowd_pattern` / `array_formation_combo` (Formation) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:329` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onFormationCommit()` (`indra/newview/alpanelghoststudio.cpp:549`), wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:3510`) | none |
| `crowd_pattern` / `array_count_spinner` (Count) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:319` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:3510`) | none |
| `crowd_pattern` / `array_spacing_spinner` (Spacing (m)) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:322` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:3510`) | none |
| `crowd_layout` / `formation_edge_gap_spinner` (Edge gap) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:375` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:3510`) | none |
| `crowd_layout` / `formation_jitter_spinner` (Jitter) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:377` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:3510`) | none |
| `crowd_layout` / `formation_seed_spinner` (Seed) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:383` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:3510`) | none |
| `crowd_layout` / `formation_terrain_check` (Snap to ground) | `check_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:372` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:3510`) | none |
| `crowd_layout` / `formation_facing_combo` (Heading) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:390` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:3510`) | none |
| `crowd_layout` / `facing_yaw_offset_spinner` (Facing offset) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:418` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALGhostStudio::getCrowdPlacementDraft()` (`indra/newview/alpanelghoststudio.cpp:592`) | none |
| `crowd_layout` / `btn_pick_facing_target` (Pick facing) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:415` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickPickFacingTarget()` (`indra/newview/alpanelghoststudio.cpp:617`) | none |
| `crowd_layout` / `preserve_source_tilt_check` (Source tilt) | `check_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:420` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:3510`) | none |
| `crowd_guide` / `formation_guide_shape_combo` (Guide) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:364` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:3510`) | none |
| `crowd_guide` / `formation_guide_size_spinner` (Guide size) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:370` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:3510`) | none |
| `crowd_guide` / `crowd_copies_as_combo` (Copies as) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:357` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:3510`) | none |
| `crowd_guide` / `crowd_create_mode_combo` (Create as) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:409` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:3510`) | none |
| `crowd_guide` / `nameplate_mode_combo` (Names) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:445` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onNameplateModeCommit()` (`indra/newview/alpanelghoststudio.cpp:637`) | none |
| `crowd_guide` / `formation_overlay_palette_combo` (Marker colour) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:431` | kit `combo` ↔ `GhostStudioOverlayPalette` `S32`; `ALGhostStudioKitAdapter` preserves/calls wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:3510`) | none |
| `crowd_preview` / `btn_array_build` (Preview) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:423` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickBuildArray()` (`indra/newview/alpanelghoststudio.cpp:607`) | none |
| `crowd_preview` / `btn_crowd_confirm` (Confirm) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:425` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickCrowdConfirm()` (`indra/newview/alpanelghoststudio.cpp:608`) | none |
| `crowd_preview` / `btn_crowd_unpin` (Unpin) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:427` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickCrowdUnpin()` (`indra/newview/alpanelghoststudio.cpp:610`) | none |
| `crowd_preview` / `btn_crowd_cancel` (Cancel) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:429` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickCrowdCancel()` (`indra/newview/alpanelghoststudio.cpp:612`) | none |
| `crowd_preview` / `crowd_placement_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:440` | kit readout ← approved static copy; no runtime writer | none |
| `crowd_preview` / `crowd_placement_instructions` (Controls) | `text` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:442` | kit readout ← approved static copy; no runtime writer | none |
| `group_members` / `group_edit_members_check` (Edit members) | `check_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:451` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onGroupEditMembersCommit()` (`indra/newview/alpanelghoststudio.cpp:631`) | none |
| `group_members` / `lock_mode_combo` (Group lock) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:453` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALGhostStudio::getCrowdPlacementDraft()` (`indra/newview/alpanelghoststudio.cpp:621`) | none |
| `group_members` / `group_member_pinned_check` (Pin offset) | `check_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:459` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onGroupMemberPinnedCommit()` (`indra/newview/alpanelghoststudio.cpp:633`) | none |
| `group_members` / `btn_group_member_reset` (Reset member) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:461` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickGroupMemberReset()` (`indra/newview/alpanelghoststudio.cpp:635`) | none |
| `crowd_brigade` / `brigade_columns_spinner` (Files) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:386` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:3510`) | none |
| `crowd_brigade` / `brigade_rows_spinner` (Ranks) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:387` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:3510`) | none |
| `crowd_brigade` / `brigade_file_spacing_spinner` (File gap) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:385` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:3510`) | none |
| `crowd_brigade` / `brigade_rank_spacing_spinner` (Rank gap) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:388` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:3510`) | none |
| `crowd_brigade` / `array_parameter_spinner` (Parameter) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:380` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::captureFormationParameter()` (`indra/newview/alpanelghoststudio.cpp:550`), wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:3510`) | none |
| `pose_strip` / `strip_count_spinner` (Frames) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:468` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickStartStrip()` (`indra/newview/alpanelghoststudio.cpp:3901`) | none |
| `pose_strip` / `strip_interval_spinner` (Interval (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:471` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickStartStrip()` (`indra/newview/alpanelghoststudio.cpp:3901`) | none |
| `pose_strip` / `btn_strip_start` (Capture) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:474` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickStartStrip()` (`indra/newview/alpanelghoststudio.cpp:614`) | none |
| `pose_strip` / `btn_strip_cancel` (Cancel) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:476` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickCancelStrip()` (`indra/newview/alpanelghoststudio.cpp:615`) | none |
| `pose_strip` / `strip_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:478` | kit readout ← approved static copy; no runtime writer | none |
| `formation_motion` / `formation_motion_combo` (Motion) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:479` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onMotionCommit()` (`indra/newview/alpanelghoststudio.cpp:3932`) | none |
| `formation_motion` / `formation_motion_speed` (Speed) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:488` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:3510`) | none |
| `formation_motion` / `formation_motion_amplitude` (Amp) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:491` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:3510`) | none |
| `formation_motion` / `btn_formation_motion_apply` (Apply) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:494` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onMotionCommit()` (`indra/newview/alpanelghoststudio.cpp:616`) | none |
| `formation_motion` / `pose_rate_spinner` (Pose fps) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:500` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onPoseRateCommit()` (`indra/newview/alpanelghoststudio.cpp:639`) | none |
| `directing_unit` / `btn_group_unit` (Group) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:496` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickGroup()` (`indra/newview/alpanelghoststudio.cpp:619`) | none |
| `directing_unit` / `btn_ungroup_unit` (Ungroup) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:498` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickUngroup()` (`indra/newview/alpanelghoststudio.cpp:620`) | none |
| `directing_unit` / `motion_precedence_help` (Note) | `text` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:502` | kit readout ← approved static copy; no runtime writer | none |
| `diagnostics` / `btn_rig_diag_refresh` (Refresh) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:304` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickRigDiagRefresh()` (`indra/newview/alpanelghoststudio.cpp:542`) | none |
| `diagnostics` / `rig_diagnostics` (Report) | `text_editor` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:306` | kit readout ← approved static copy; no runtime writer | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALGhostStudio::getCrowdPlacementDraft()` — `indra/newview/alpanelghoststudio.cpp:592, 621` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALGhostStudio::scanAnimLibrary()` — `indra/newview/alpanelghoststudio.cpp:483` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::captureFormationParameter()` — `indra/newview/alpanelghoststudio.cpp:550` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onActorTintToggle()` — `indra/newview/alpanelghoststudio.cpp:527` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onAlphaCommit()` — `indra/newview/alpanelghoststudio.cpp:529` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onAnimSpeedCommit()` — `indra/newview/alpanelghoststudio.cpp:466` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onAnimationLibraryCommit()` — `indra/newview/alpanelghoststudio.cpp:482` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onAutoAnimAction()` — `indra/newview/alpanelghoststudio.cpp:470, 471, 472, 473, 474` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onAutoAnimFile()` — `indra/newview/alpanelghoststudio.cpp:475, 476` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onAutoAnimWindow()` — `indra/newview/alpanelghoststudio.cpp:469` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onBrightnessCommit()` — `indra/newview/alpanelghoststudio.cpp:536` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onChaosCommit()` — `indra/newview/alpanelghoststudio.cpp:465` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickAdd()` — `indra/newview/alpanelghoststudio.cpp:449, 1977` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickAlignFeet()` — `indra/newview/alpanelghoststudio.cpp:514` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickAnimPause()` — `indra/newview/alpanelghoststudio.cpp:478` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickAnimResume()` — `indra/newview/alpanelghoststudio.cpp:479` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickAnimSync()` — `indra/newview/alpanelghoststudio.cpp:491` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickArray()` — `indra/newview/alpanelghoststudio.cpp:545, 547` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickBuildArray()` — `indra/newview/alpanelghoststudio.cpp:607` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickCancelStrip()` — `indra/newview/alpanelghoststudio.cpp:615` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickCopyTransform()` — `indra/newview/alpanelghoststudio.cpp:517` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickCrowdCancel()` — `indra/newview/alpanelghoststudio.cpp:612` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickCrowdConfirm()` — `indra/newview/alpanelghoststudio.cpp:608` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickCrowdUnpin()` — `indra/newview/alpanelghoststudio.cpp:610` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickDelete()` — `indra/newview/alpanelghoststudio.cpp:452` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickDrop()` — `indra/newview/alpanelghoststudio.cpp:513` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickDuplicate()` — `indra/newview/alpanelghoststudio.cpp:451` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickDuplicateInPlace()` — `indra/newview/alpanelghoststudio.cpp:516` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickFaceNow()` — `indra/newview/alpanelghoststudio.cpp:521` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickFreeze()` — `indra/newview/alpanelghoststudio.cpp:540` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickGroup()` — `indra/newview/alpanelghoststudio.cpp:619` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickGroupMemberReset()` — `indra/newview/alpanelghoststudio.cpp:635` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickLensGazeSelection()` — `indra/newview/alpanelghoststudio.cpp:494` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickLive()` — `indra/newview/alpanelghoststudio.cpp:541` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickPasteTransform()` — `indra/newview/alpanelghoststudio.cpp:518` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickPickFacingTarget()` — `indra/newview/alpanelghoststudio.cpp:617` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickPlace()` — `indra/newview/alpanelghoststudio.cpp:510` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickRefresh()` — `indra/newview/alpanelghoststudio.cpp:453` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickRigDiagRefresh()` — `indra/newview/alpanelghoststudio.cpp:542` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickStartStrip()` — `indra/newview/alpanelghoststudio.cpp:614, 3901` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickToActor()` — `indra/newview/alpanelghoststudio.cpp:511` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickToMe()` — `indra/newview/alpanelghoststudio.cpp:512` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickUngroup()` — `indra/newview/alpanelghoststudio.cpp:620` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickUpright()` — `indra/newview/alpanelghoststudio.cpp:515` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onDirectedAnimCommit()` — `indra/newview/alpanelghoststudio.cpp:481` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onDistortAmountCommit()` — `indra/newview/alpanelghoststudio.cpp:535` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onDistortCommit()` — `indra/newview/alpanelghoststudio.cpp:534` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onDriveModeCommit()` — `indra/newview/alpanelghoststudio.cpp:480` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onEffectFpsCommit()` — `indra/newview/alpanelghoststudio.cpp:537` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onEntityLookCommit()` — `indra/newview/alpanelghoststudio.cpp:538` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onFormationCommit()` — `indra/newview/alpanelghoststudio.cpp:549` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onGlitchCommit()` — `indra/newview/alpanelghoststudio.cpp:533` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onGroupEditMembersCommit()` — `indra/newview/alpanelghoststudio.cpp:631` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onGroupMemberPinnedCommit()` — `indra/newview/alpanelghoststudio.cpp:633` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onHueCommit()` — `indra/newview/alpanelghoststudio.cpp:528` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onKeepFacingCommit()` — `indra/newview/alpanelghoststudio.cpp:523` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onLensGazeSettingsCommit()` — `indra/newview/alpanelghoststudio.cpp:498, 500, 502, 504, 506` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onLensGazeTorsoCommit()` — `indra/newview/alpanelghoststudio.cpp:496` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onListMouseUp()` — `indra/newview/alpanelghoststudio.cpp:446` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onListSelect()` — `indra/newview/alpanelghoststudio.cpp:445` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onLookTargetCommit()` — `indra/newview/alpanelghoststudio.cpp:519` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onLoopModeCommit()` — `indra/newview/alpanelghoststudio.cpp:490` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onMotionCommit()` — `indra/newview/alpanelghoststudio.cpp:616, 3932` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onNameCommit()` — `indra/newview/alpanelghoststudio.cpp:450` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onNameplateModeCommit()` — `indra/newview/alpanelghoststudio.cpp:637` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onPhysicsCommit()` — `indra/newview/alpanelghoststudio.cpp:477` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onPixelCommit()` — `indra/newview/alpanelghoststudio.cpp:532` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onPosCommit()` — `indra/newview/alpanelghoststudio.cpp:455, 456, 457` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onPoseRateCommit()` — `indra/newview/alpanelghoststudio.cpp:639` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onScaleCommit()` — `indra/newview/alpanelghoststudio.cpp:463` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onShimmerAmountCommit()` — `indra/newview/alpanelghoststudio.cpp:531` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onShimmerSpeedCommit()` — `indra/newview/alpanelghoststudio.cpp:530` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onShowAllToggle()` — `indra/newview/alpanelghoststudio.cpp:443` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onStyleCommit()` — `indra/newview/alpanelghoststudio.cpp:526` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onToggleEditMode()` — `indra/newview/alpanelghoststudio.cpp:444` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onYawCommit()` — `indra/newview/alpanelghoststudio.cpp:462` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::updateOwnedCrowdPlacement()` — `indra/newview/alpanelghoststudio.cpp:3510` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **212 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-ghost_list-tallest-850` | Only `ghost_list` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-ghost_list-tallest-1160` | Only `ghost_list` is put in its tallest local state at 1160 px; all other cards stay default. |
| `show_all_check-on` | Only `show_all_check` is on; dependent visibility is measured. |
| `edit_ghosts_check-on` | Only `edit_ghosts_check` is on; dependent visibility is measured. |
| `ghost_list-empty` | `ghost_list` is empty with no selection. |
| `ghost_list-one` | `ghost_list` has one selected item. |
| `ghost_list-overflow` | `ghost_list` has 51 items and a visible scrollbar. |
| `card-ghost_add_copy-tallest-850` | Only `ghost_add_copy` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-ghost_add_copy-tallest-1160` | Only `ghost_add_copy` is put in its tallest local state at 1160 px; all other cards stay default. |
| `source_combo-option-0` | Only `source_combo` selects `Selected cast`; other selectors remain default. |
| `source_combo-option-1` | Only `source_combo` selects `Mara`; other selectors remain default. |
| `source_combo-option-2` | Only `source_combo` selects `Jun`; other selectors remain default. |
| `source_combo-option-3` | Only `source_combo` selects `Me`; other selectors remain default. |
| `clone_type_combo-option-0` | Only `clone_type_combo` selects `Overlay ghost`; other selectors remain default. |
| `clone_type_combo-option-1` | Only `clone_type_combo` selects `Entity clone`; other selectors remain default. |
| `card-ghost_position-tallest-850` | Only `ghost_position` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-ghost_position-tallest-1160` | Only `ghost_position` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-ghost_snap-tallest-850` | Only `ghost_snap` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-ghost_snap-tallest-1160` | Only `ghost_snap` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-ghost_facing-tallest-850` | Only `ghost_facing` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-ghost_facing-tallest-1160` | Only `ghost_facing` is put in its tallest local state at 1160 px; all other cards stay default. |
| `look_target_combo-option-0` | Only `look_target_combo` selects `Camera`; other selectors remain default. |
| `look_target_combo-option-1` | Only `look_target_combo` selects `Me`; other selectors remain default. |
| `look_target_combo-option-2` | Only `look_target_combo` selects `Actor: Mara`; other selectors remain default. |
| `look_target_combo-option-3` | Only `look_target_combo` selects `Ghost: Ghost 1`; other selectors remain default. |
| `keep_facing_check-on` | Only `keep_facing_check` is on; dependent visibility is measured. |
| `card-look-tallest-850` | Only `look` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-look-tallest-1160` | Only `look` is put in its tallest local state at 1160 px; all other cards stay default. |
| `style_combo-option-0` | Only `style_combo` selects `Ghost`; other selectors remain default. |
| `style_combo-option-1` | Only `style_combo` selects `Clone`; other selectors remain default. |
| `style_combo-option-2` | Only `style_combo` selects `Hologram`; other selectors remain default. |
| `style_combo-option-3` | Only `style_combo` selects `Wireframe`; other selectors remain default. |
| `style_combo-option-4` | Only `style_combo` selects `X-ray`; other selectors remain default. |
| `style_combo-option-5` | Only `style_combo` selects `Thermal`; other selectors remain default. |
| `style_combo-option-6` | Only `style_combo` selects `Neon Outline`; other selectors remain default. |
| `style_combo-option-7` | Only `style_combo` selects `Silhouette`; other selectors remain default. |
| `style_combo-option-8` | Only `style_combo` selects `Toon/Ink`; other selectors remain default. |
| `style_combo-option-9` | Only `style_combo` selects `Chrome`; other selectors remain default. |
| `style_combo-option-10` | Only `style_combo` selects `Dissolve`; other selectors remain default. |
| `style_combo-option-11` | Only `style_combo` selects `Negative`; other selectors remain default. |
| `style_combo-option-12` | Only `style_combo` selects `Gold Statue`; other selectors remain default. |
| `style_combo-option-13` | Only `style_combo` selects `Night-vision`; other selectors remain default. |
| `style_combo-option-14` | Only `style_combo` selects `Blueprint`; other selectors remain default. |
| `style_combo-option-15` | Only `style_combo` selects `Ectoplasm`; other selectors remain default. |
| `style_combo-option-16` | Only `style_combo` selects `Frost/Ice`; other selectors remain default. |
| `style_combo-option-17` | Only `style_combo` selects `Prism`; other selectors remain default. |
| `style_combo-option-18` | Only `style_combo` selects `Thermal Scope`; other selectors remain default. |
| `style_combo-option-19` | Only `style_combo` selects `Wallhack/ESP`; other selectors remain default. |
| `style_combo-option-20` | Only `style_combo` selects `Night-vision Tube`; other selectors remain default. |
| `style_combo-option-21` | Only `style_combo` selects `Damage Overlay`; other selectors remain default. |
| `style_combo-option-22` | Only `style_combo` selects `Killcam`; other selectors remain default. |
| `style_combo-option-23` | Only `style_combo` selects `Oil Slick`; other selectors remain default. |
| `style_combo-option-24` | Only `style_combo` selects `Vaporwave`; other selectors remain default. |
| `style_combo-option-25` | Only `style_combo` selects `Halftone/Comic`; other selectors remain default. |
| `style_combo-option-26` | Only `style_combo` selects `Sonar Reveal`; other selectors remain default. |
| `style_combo-option-27` | Only `style_combo` selects `Hologram Echo`; other selectors remain default. |
| `distort_combo-option-0` | Only `distort_combo` selects `Distort: None`; other selectors remain default. |
| `distort_combo-option-1` | Only `distort_combo` selects `Distort: Pixelate`; other selectors remain default. |
| `distort_combo-option-2` | Only `distort_combo` selects `Distort: Voxel`; other selectors remain default. |
| `distort_combo-option-3` | Only `distort_combo` selects `Distort: Lens/Magnify`; other selectors remain default. |
| `distort_combo-option-4` | Only `distort_combo` selects `Distort: Wave/Ripple`; other selectors remain default. |
| `distort_combo-option-5` | Only `distort_combo` selects `Distort: RGB Split`; other selectors remain default. |
| `distort_combo-option-6` | Only `distort_combo` selects `Distort: Block Glitch`; other selectors remain default. |
| `distort_combo-option-7` | Only `distort_combo` selects `Distort: Vertical Tear`; other selectors remain default. |
| `distort_combo-option-8` | Only `distort_combo` selects `Distort: VHS`; other selectors remain default. |
| `card-tint-tallest-850` | Only `tint` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-tint-tallest-1160` | Only `tint` is put in its tallest local state at 1160 px; all other cards stay default. |
| `actor_tint_check-on` | Only `actor_tint_check` is on; dependent visibility is measured. |
| `card-motion_fx-tallest-850` | Only `motion_fx` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-motion_fx-tallest-1160` | Only `motion_fx` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-pose-tallest-850` | Only `pose` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-pose-tallest-1160` | Only `pose` is put in its tallest local state at 1160 px; all other cards stay default. |
| `drive_mode_combo-option-0` | Only `drive_mode_combo` selects `Mirror`; other selectors remain default. |
| `drive_mode_combo-option-1` | Only `drive_mode_combo` selects `Directed`; other selectors remain default. |
| `drive_mode_combo-option-2` | Only `drive_mode_combo` selects `Frozen`; other selectors remain default. |
| `drive_mode_combo-option-3` | Only `drive_mode_combo` selects `True mirror`; other selectors remain default. |
| `mirror_hold_check-on` | Only `mirror_hold_check` is on; dependent visibility is measured. |
| `mirror_repeat_held_check-on` | Only `mirror_repeat_held_check` is on; dependent visibility is measured. |
| `card-chaos-tallest-850` | Only `chaos` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-chaos-tallest-1160` | Only `chaos` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-entity-tallest-850` | Only `entity` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-entity-tallest-1160` | Only `entity` is put in its tallest local state at 1160 px; all other cards stay default. |
| `entity_look_combo-option-0` | Only `entity_look_combo` selects `Normal`; other selectors remain default. |
| `entity_look_combo-option-1` | Only `entity_look_combo` selects `Apparition`; other selectors remain default. |
| `entity_look_combo-option-2` | Only `entity_look_combo` selects `Hologram*`; other selectors remain default. |
| `entity_look_combo-option-3` | Only `entity_look_combo` selects `Chrome*`; other selectors remain default. |
| `entity_look_combo-option-4` | Only `entity_look_combo` selects `Toon*`; other selectors remain default. |
| `entity_look_combo-option-5` | Only `entity_look_combo` selects `Silhouette*`; other selectors remain default. |
| `physics_check-on` | Only `physics_check` is on; dependent visibility is measured. |
| `anim_loop_mode_combo-option-0` | Only `anim_loop_mode_combo` selects `Loop (re-trigger)`; other selectors remain default. |
| `anim_loop_mode_combo-option-1` | Only `anim_loop_mode_combo` selects `Play once`; other selectors remain default. |
| `card-playback-tallest-850` | Only `playback` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-playback-tallest-1160` | Only `playback` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-anim_asset-tallest-850` | Only `anim_asset` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-anim_asset-tallest-1160` | Only `anim_asset` is put in its tallest local state at 1160 px; all other cards stay default. |
| `animation_library_combo-option-0` | Only `animation_library_combo` selects `Choose inventory animation...`; other selectors remain default. |
| `animation_library_combo-option-1` | Only `animation_library_combo` selects `Dance hip sway`; other selectors remain default. |
| `animation_library_combo-option-2` | Only `animation_library_combo` selects `Idle look around`; other selectors remain default. |
| `animation_library_combo-option-3` | Only `animation_library_combo` selects `Wave hello`; other selectors remain default. |
| `card-autoanim-tallest-850` | Only `autoanim` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-autoanim-tallest-1160` | Only `autoanim` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-head_gaze-tallest-850` | Only `head_gaze` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-head_gaze-tallest-1160` | Only `head_gaze` is put in its tallest local state at 1160 px; all other cards stay default. |
| `lens_gaze_target_mode-option-0` | Only `lens_gaze_target_mode` selects `Travel tangent`; other selectors remain default. |
| `lens_gaze_target_mode-option-1` | Only `lens_gaze_target_mode` selects `Camera`; other selectors remain default. |
| `lens_gaze_target_mode-option-2` | Only `lens_gaze_target_mode` selects `Cast target`; other selectors remain default. |
| `lens_gaze_target_mode-option-3` | Only `lens_gaze_target_mode` selects `World point`; other selectors remain default. |
| `lens_gaze_cast_target-option-0` | Only `lens_gaze_cast_target` selects `Selected cast`; other selectors remain default. |
| `lens_gaze_cast_target-option-1` | Only `lens_gaze_cast_target` selects `Mara`; other selectors remain default. |
| `lens_gaze_cast_target-option-2` | Only `lens_gaze_cast_target` selects `Jun`; other selectors remain default. |
| `lens_gaze_cast_target-option-3` | Only `lens_gaze_cast_target` selects `Me`; other selectors remain default. |
| `card-crowd_pattern-tallest-850` | Only `crowd_pattern` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-crowd_pattern-tallest-1160` | Only `crowd_pattern` is put in its tallest local state at 1160 px; all other cards stay default. |
| `array_formation_combo-option-0` | Only `array_formation_combo` selects `Line`; other selectors remain default. |
| `array_formation_combo-option-1` | Only `array_formation_combo` selects `Grid`; other selectors remain default. |
| `array_formation_combo-option-2` | Only `array_formation_combo` selects `Staggered rows`; other selectors remain default. |
| `array_formation_combo-option-3` | Only `array_formation_combo` selects `Split row`; other selectors remain default. |
| `array_formation_combo-option-4` | Only `array_formation_combo` selects `Soul Train`; other selectors remain default. |
| `array_formation_combo-option-5` | Only `array_formation_combo` selects `Ring`; other selectors remain default. |
| `array_formation_combo-option-6` | Only `array_formation_combo` selects `Arc`; other selectors remain default. |
| `array_formation_combo-option-7` | Only `array_formation_combo` selects `Concentric rings`; other selectors remain default. |
| `array_formation_combo-option-8` | Only `array_formation_combo` selects `Spiral`; other selectors remain default. |
| `array_formation_combo-option-9` | Only `array_formation_combo` selects `Chevron`; other selectors remain default. |
| `array_formation_combo-option-10` | Only `array_formation_combo` selects `Zigzag`; other selectors remain default. |
| `array_formation_combo-option-11` | Only `array_formation_combo` selects `Horseshoe`; other selectors remain default. |
| `array_formation_combo-option-12` | Only `array_formation_combo` selects `Infinity`; other selectors remain default. |
| `array_formation_combo-option-13` | Only `array_formation_combo` selects `Pentagram`; other selectors remain default. |
| `array_formation_combo-option-14` | Only `array_formation_combo` selects `Star outline`; other selectors remain default. |
| `array_formation_combo-option-15` | Only `array_formation_combo` selects `Diamond`; other selectors remain default. |
| `array_formation_combo-option-16` | Only `array_formation_combo` selects `Cross`; other selectors remain default. |
| `array_formation_combo-option-17` | Only `array_formation_combo` selects `Arrow`; other selectors remain default. |
| `array_formation_combo-option-18` | Only `array_formation_combo` selects `Heart`; other selectors remain default. |
| `array_formation_combo-option-19` | Only `array_formation_combo` selects `Sunburst`; other selectors remain default. |
| `array_formation_combo-option-20` | Only `array_formation_combo` selects `Military Brigade`; other selectors remain default. |
| `array_formation_combo-option-21` | Only `array_formation_combo` selects `Soul Train (military)`; other selectors remain default. |
| `array_formation_combo-option-22` | Only `array_formation_combo` selects `Staggered ranks (military)`; other selectors remain default. |
| `array_formation_combo-option-23` | Only `array_formation_combo` selects `Ring brigade (military)`; other selectors remain default. |
| `array_formation_combo-option-24` | Only `array_formation_combo` selects `Organic`; other selectors remain default. |
| `card-crowd_layout-tallest-850` | Only `crowd_layout` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-crowd_layout-tallest-1160` | Only `crowd_layout` is put in its tallest local state at 1160 px; all other cards stay default. |
| `formation_terrain_check-on` | Only `formation_terrain_check` is on; dependent visibility is measured. |
| `formation_facing_combo-option-0` | Only `formation_facing_combo` selects `Rotate with formation`; other selectors remain default. |
| `formation_facing_combo-option-1` | Only `formation_facing_combo` selects `Source yaw`; other selectors remain default. |
| `formation_facing_combo-option-2` | Only `formation_facing_combo` selects `Camera / stage`; other selectors remain default. |
| `formation_facing_combo-option-3` | Only `formation_facing_combo` selects `Centroid in`; other selectors remain default. |
| `formation_facing_combo-option-4` | Only `formation_facing_combo` selects `Outward`; other selectors remain default. |
| `formation_facing_combo-option-5` | Only `formation_facing_combo` selects `World point`; other selectors remain default. |
| `formation_facing_combo-option-6` | Only `formation_facing_combo` selects `Mirror source yaw`; other selectors remain default. |
| `formation_facing_combo-option-7` | Only `formation_facing_combo` selects `Look-at target actor`; other selectors remain default. |
| `formation_facing_combo-option-8` | Only `formation_facing_combo` selects `Random`; other selectors remain default. |
| `formation_facing_combo-option-9` | Only `formation_facing_combo` selects `Face the aisle`; other selectors remain default. |
| `formation_facing_combo-option-10` | Only `formation_facing_combo` selects `Watch subject, live`; other selectors remain default. |
| `formation_facing_combo-option-11` | Only `formation_facing_combo` selects `Two ranks across`; other selectors remain default. |
| `formation_facing_combo-option-12` | Only `formation_facing_combo` selects `Cluster inward`; other selectors remain default. |
| `formation_facing_combo-option-13` | Only `formation_facing_combo` selects `Loose toward focal`; other selectors remain default. |
| `formation_facing_combo-option-14` | Only `formation_facing_combo` selects `Face previous slot`; other selectors remain default. |
| `formation_facing_combo-option-15` | Only `formation_facing_combo` selects `Animated look wave`; other selectors remain default. |
| `preserve_source_tilt_check-on` | Only `preserve_source_tilt_check` is on; dependent visibility is measured. |
| `card-crowd_guide-tallest-850` | Only `crowd_guide` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-crowd_guide-tallest-1160` | Only `crowd_guide` is put in its tallest local state at 1160 px; all other cards stay default. |
| `formation_guide_shape_combo-option-0` | Only `formation_guide_shape_combo` selects `Guide: Off`; other selectors remain default. |
| `formation_guide_shape_combo-option-1` | Only `formation_guide_shape_combo` selects `Guide: Circle`; other selectors remain default. |
| `formation_guide_shape_combo-option-2` | Only `formation_guide_shape_combo` selects `Guide: Square`; other selectors remain default. |
| `crowd_copies_as_combo-option-0` | Only `crowd_copies_as_combo` selects `Copies as: Select clone`; other selectors remain default. |
| `crowd_copies_as_combo-option-1` | Only `crowd_copies_as_combo` selects `Copies as: Overlay ghost`; other selectors remain default. |
| `crowd_copies_as_combo-option-2` | Only `crowd_copies_as_combo` selects `Copies as: Entity clone`; other selectors remain default. |
| `crowd_create_mode_combo-option-0` | Only `crowd_create_mode_combo` selects `Create: Separate`; other selectors remain default. |
| `crowd_create_mode_combo-option-1` | Only `crowd_create_mode_combo` selects `Create: Rigid group`; other selectors remain default. |
| `crowd_create_mode_combo-option-2` | Only `crowd_create_mode_combo` selects `Create: Move+Face group`; other selectors remain default. |
| `nameplate_mode_combo-option-0` | Only `nameplate_mode_combo` selects `Names: Off`; other selectors remain default. |
| `nameplate_mode_combo-option-1` | Only `nameplate_mode_combo` selects `Names: Smart`; other selectors remain default. |
| `nameplate_mode_combo-option-2` | Only `nameplate_mode_combo` selects `Names: All`; other selectors remain default. |
| `formation_overlay_palette_combo-option-0` | Only `formation_overlay_palette_combo` selects `Preview: Neon green`; other selectors remain default. |
| `formation_overlay_palette_combo-option-1` | Only `formation_overlay_palette_combo` selects `Preview: Neon yellow`; other selectors remain default. |
| `formation_overlay_palette_combo-option-2` | Only `formation_overlay_palette_combo` selects `Preview: Cyan`; other selectors remain default. |
| `formation_overlay_palette_combo-option-3` | Only `formation_overlay_palette_combo` selects `Preview: White`; other selectors remain default. |
| `formation_overlay_palette_combo-option-4` | Only `formation_overlay_palette_combo` selects `Preview: Magenta`; other selectors remain default. |
| `card-crowd_preview-tallest-850` | Only `crowd_preview` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-crowd_preview-tallest-1160` | Only `crowd_preview` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-group_members-tallest-850` | Only `group_members` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-group_members-tallest-1160` | Only `group_members` is put in its tallest local state at 1160 px; all other cards stay default. |
| `group_edit_members_check-on` | Only `group_edit_members_check` is on; dependent visibility is measured. |
| `lock_mode_combo-option-0` | Only `lock_mode_combo` selects `Selected group: Off`; other selectors remain default. |
| `lock_mode_combo-option-1` | Only `lock_mode_combo` selects `Selected group: Rigid`; other selectors remain default. |
| `lock_mode_combo-option-2` | Only `lock_mode_combo` selects `Selected group: Move+Face`; other selectors remain default. |
| `group_member_pinned_check-on` | Only `group_member_pinned_check` is on; dependent visibility is measured. |
| `card-crowd_brigade-tallest-850` | Only `crowd_brigade` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-crowd_brigade-tallest-1160` | Only `crowd_brigade` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-pose_strip-tallest-850` | Only `pose_strip` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-pose_strip-tallest-1160` | Only `pose_strip` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-formation_motion-tallest-850` | Only `formation_motion` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-formation_motion-tallest-1160` | Only `formation_motion` is put in its tallest local state at 1160 px; all other cards stay default. |
| `formation_motion_combo-option-0` | Only `formation_motion_combo` selects `Off`; other selectors remain default. |
| `formation_motion_combo-option-1` | Only `formation_motion_combo` selects `Orbit`; other selectors remain default. |
| `formation_motion_combo-option-2` | Only `formation_motion_combo` selects `Spin`; other selectors remain default. |
| `formation_motion_combo-option-3` | Only `formation_motion_combo` selects `Breathe`; other selectors remain default. |
| `formation_motion_combo-option-4` | Only `formation_motion_combo` selects `Ripple`; other selectors remain default. |
| `formation_motion_combo-option-5` | Only `formation_motion_combo` selects `March actor path`; other selectors remain default. |
| `card-directing_unit-tallest-850` | Only `directing_unit` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-directing_unit-tallest-1160` | Only `directing_unit` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-diagnostics-tallest-850` | Only `diagnostics` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-diagnostics-tallest-1160` | Only `diagnostics` is put in its tallest local state at 1160 px; all other cards stay default. |
| `actor-none` | Rail has no actor; target-dependent rows are disabled with an explicit empty state. |
| `actor-selected` | One rail actor is selected and unpinned. |
| `actor-pinned-rail-changes` | Page target is pinned while the rail changes to another actor. |
| `actor-removed` | The active actor disappears while the page is visible. |
| `selection-deleted` | The selected model row is removed by another owner while visible. |
| `permission-denied` | The current target exists but cannot be modified. |
| `target-offline` | The current target/provider becomes unavailable. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## Ambiguous same-name candidates (never auto-bound)

None.

## Rejected out-of-scope same-name candidates

5 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

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
