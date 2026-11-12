# Clones — Director Kit page contract

Page key: `ghosts_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (19/170 setting-bound rows; 160 C++-referenced rows).

## Real owners

- `panel_ghost_studio` / `panel` at `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:10` (165 matching rows).
- Referencing classes: `ALPanelGhostStudio` (333), `ALGhostStudio` (5), `ALGhostCloneActions` (2), `ALKitBindingRegistry` (1).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Reuse the existing preview/diagnostics controls with a 240 px maximum and visible-only, revision-driven refresh.

## Row bindings

`ALGhostStudioKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `ghost_list` / `studio_hint` () | `text` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:20` | kit readout ← approved static copy; no runtime writer | none |
| `ghost_list` / `show_all_check` (Show clones) | `check_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:18` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onShowAllToggle()` (`indra/newview/alpanelghoststudio.cpp:822`) | none |
| `ghost_list` / `edit_ghosts_check` (Edit clones) | `check_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:16` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onToggleEditMode()` (`indra/newview/alpanelghoststudio.cpp:823`) | none |
| `ghost_list` / `ghost_list` (Clones) | `scroll_list` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:23` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onListSelect()` (`indra/newview/alpanelghoststudio.cpp:824`), wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onListMouseUp()` (`indra/newview/alpanelghoststudio.cpp:826`) | none |
| `ghost_list` / `studio_status` (Count) | `text` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:1039` | kit readout ← approved static copy; no runtime writer | none |
| `ghost_list` / `btn_ghost_dup` (Duplicate) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:42` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickDuplicate()` (`indra/newview/alpanelghoststudio.cpp:831`) | none |
| `ghost_list` / `btn_ghost_del` (Delete) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:44` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickDelete()` (`indra/newview/alpanelghoststudio.cpp:832`) | none |
| `ghost_list` / `btn_ghost_refresh` (Refresh) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:46` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickRefresh()` (`indra/newview/alpanelghoststudio.cpp:833`) | none |
| `ghost_list` / `btn_ghost_solo` (Solo) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:48` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALGhostCloneActions::toggleSolo()` (`indra/newview/alpanelghoststudio.cpp:483`) | none |
| `ghost_add_copy` / `source_combo` (Copy from) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:33` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::<inline callback>()` (`indra/newview/alpanelghoststudio.cpp:500`), wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickAdd()` (`indra/newview/alpanelghoststudio.cpp:2694`) | none |
| `ghost_add_copy` / `clone_type_combo` (Type) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:35` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickAdd()` (`indra/newview/alpanelghoststudio.cpp:2694`) | none |
| `ghost_add_copy` / `btn_ghost_add` (Add) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:40` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickAdd()` (`indra/newview/alpanelghoststudio.cpp:829`) | none |
| `ghost_add_copy` / `btn_duplicate_in_place` (Duplicate here) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:114` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickDuplicateInPlace()` (`indra/newview/alpanelghoststudio.cpp:902`) | none |
| `ghost_add_copy` / `btn_copy_transform` (Copy) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:116` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickCopyTransform()` (`indra/newview/alpanelghoststudio.cpp:903`) | none |
| `ghost_add_copy` / `btn_paste_transform` (Paste) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:118` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickPasteTransform()` (`indra/newview/alpanelghoststudio.cpp:904`) | none |
| `ghost_add_copy` / `btn_ghost_swap` (Swap source) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:49` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALGhostCloneActions::swapSource()` (`indra/newview/alpanelghoststudio.cpp:473`) | none |
| `ghost_position` / `ghost_type` (Type) | `text` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:68` | kit readout ← approved static copy; no runtime writer | none |
| `ghost_position` / `entity_style_hint` () | `text` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:134` | kit readout ← approved static copy; no runtime writer | none |
| `ghost_position` / `ghost_name` (Name) | `line_editor` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:69` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onNameCommit()` (`indra/newview/alpanelghoststudio.cpp:830`) | none |
| `ghost_position` / `pos_x_spinner` (X) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:71` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onPosCommit()` (`indra/newview/alpanelghoststudio.cpp:835`) | none |
| `ghost_position` / `pos_y_spinner` (Y) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:73` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onPosCommit()` (`indra/newview/alpanelghoststudio.cpp:836`) | none |
| `ghost_position` / `pos_z_spinner` (Z) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:75` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onPosCommit()` (`indra/newview/alpanelghoststudio.cpp:837`) | none |
| `ghost_position` / `yaw_spinner` (Yaw (deg)) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:77` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onYawCommit()` (`indra/newview/alpanelghoststudio.cpp:842`) | none |
| `ghost_position` / `heading_dial` (Heading dial) | `compass_dial` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:80` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onHeadingDialCommit()` (`indra/newview/alpanelghoststudio.cpp:840`) | none |
| `ghost_position` / `pitch_spinner` (Pitch (deg)) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:87` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::asReal()` (`indra/newview/alpanelghoststudio.cpp:843`) | none |
| `ghost_position` / `roll_spinner` (Roll (deg)) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:90` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::asReal()` (`indra/newview/alpanelghoststudio.cpp:845`) | none |
| `ghost_position` / `scale_spinner` (Scale) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:84` | kit `number` ↔ `TemporalWorldScale` `F32`; `ALGhostStudioKitAdapter` preserves/calls wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onScaleCommit()` (`indra/newview/alpanelghoststudio.cpp:847`). Contract note: page contract supplies omitted setting `TemporalWorldScale` | page `temporal_tab`, `indra/newview/skins/default/xui/en/panel_dpage_capture_temporal.xml:18` `scale_slider`, `indra/newview/skins/default/xui/en/panel_temporal_capture.xml:78` `scale_slider`, `indra/newview/skins/default/xui/en/panel_temporal_capture.xml:94` `scale_spinner` |
| `ghost_position` / `btn_paste_position` (Paste position) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:99` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickPastePosition()` (`indra/newview/alpanelghoststudio.cpp:905`) | none |
| `ghost_snap` / `btn_place` (Click to place) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:93` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickPlace()` (`indra/newview/alpanelghoststudio.cpp:896`) | none |
| `ghost_snap` / `btn_to_actor` (To actor) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:95` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickToActor()` (`indra/newview/alpanelghoststudio.cpp:897`) | none |
| `ghost_snap` / `btn_to_me` (To me) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:97` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickToMe()` (`indra/newview/alpanelghoststudio.cpp:898`) | none |
| `ghost_snap` / `btn_drop_ground` (Drop to ground) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:108` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickDrop()` (`indra/newview/alpanelghoststudio.cpp:899`) | none |
| `ghost_snap` / `btn_align_feet` (Align feet) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:110` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickAlignFeet()` (`indra/newview/alpanelghoststudio.cpp:900`) | none |
| `ghost_facing` / `look_target_combo` (Face target) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:102` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onLookTargetCommit()` (`indra/newview/alpanelghoststudio.cpp:906`) | none |
| `ghost_facing` / `btn_face_now` (Face now) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:104` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickFaceNow()` (`indra/newview/alpanelghoststudio.cpp:908`) | none |
| `ghost_facing` / `keep_facing_check` (Keep facing) | `check_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:106` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onKeepFacingCommit()` (`indra/newview/alpanelghoststudio.cpp:910`) | none |
| `ghost_facing` / `btn_reset_upright` (Upright) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:112` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickUpright()` (`indra/newview/alpanelghoststudio.cpp:901`) | none |
| `ghost_look` / `style_combo` (Look) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:139` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onStyleCommit()` (`indra/newview/alpanelghoststudio.cpp:913`) | none |
| `ghost_look` / `distort_combo` (Distortion) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:205` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onDistortCommit()` (`indra/newview/alpanelghoststudio.cpp:927`) | none |
| `ghost_look` / `distort_amount_slider` (Distort amt) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:255` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onDistortAmountCommit()` (`indra/newview/alpanelghoststudio.cpp:928`) | none |
| `tint` / `actor_tint_check` (Actor hue) | `check_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:217` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onActorTintToggle()` (`indra/newview/alpanelghoststudio.cpp:914`) | none |
| `tint` / `hue_slider` (Hue) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:219` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onHueCommit()` (`indra/newview/alpanelghoststudio.cpp:915`) | none |
| `tint` / `alpha_slider` (Alpha) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:222` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onAlphaCommit()` (`indra/newview/alpanelghoststudio.cpp:916`) | none |
| `tint` / `brightness_slider` (Brightness) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:258` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onBrightnessCommit()` (`indra/newview/alpanelghoststudio.cpp:929`) | none |
| `motion_fx` / `pixel_slider` (Pixelate) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:225` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onPixelCommit()` (`indra/newview/alpanelghoststudio.cpp:919`) | none |
| `motion_fx` / `shimmer_amount_slider` (Shimmer) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:231` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onShimmerAmountCommit()` (`indra/newview/alpanelghoststudio.cpp:918`) | none |
| `motion_fx` / `shimmer_speed_slider` (Shimmer Hz) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:228` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onShimmerSpeedCommit()` (`indra/newview/alpanelghoststudio.cpp:917`) | none |
| `motion_fx` / `glitch_slider` (Glitch) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:234` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onGlitchCommit()` (`indra/newview/alpanelghoststudio.cpp:920`) | none |
| `motion_fx` / `slice_slider` (Slice) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:237` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onSliceCommit()` (`indra/newview/alpanelghoststudio.cpp:921`) | none |
| `motion_fx` / `dropout_slider` (Dropout) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:240` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onDropoutCommit()` (`indra/newview/alpanelghoststudio.cpp:922`) | none |
| `motion_fx` / `power_flicker_slider` (Power flicker) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:243` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onPowerFlickerCommit()` (`indra/newview/alpanelghoststudio.cpp:923`) | none |
| `motion_fx` / `wipe_slider` (Wipe) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:246` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onWipeCommit()` (`indra/newview/alpanelghoststudio.cpp:924`) | none |
| `motion_fx` / `interlace_slider` (Interlace) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:249` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onInterlaceCommit()` (`indra/newview/alpanelghoststudio.cpp:925`) | none |
| `motion_fx` / `position_jitter_slider` (Position jitter) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:252` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onPositionJitterCommit()` (`indra/newview/alpanelghoststudio.cpp:926`) | none |
| `motion_fx` / `effect_fps_spinner` (Effect fps) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:261` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onEffectFpsCommit()` (`indra/newview/alpanelghoststudio.cpp:930`) | none |
| `pose` / `btn_freeze` (Grab pose) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:276` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickFreeze()` (`indra/newview/alpanelghoststudio.cpp:933`) | none |
| `pose` / `btn_live` (Follow live) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:278` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickLive()` (`indra/newview/alpanelghoststudio.cpp:934`) | none |
| `pose` / `drive_mode_combo` (Drive) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:285` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onDriveModeCommit()` (`indra/newview/alpanelghoststudio.cpp:864`) | none |
| `pose` / `mirror_hold_check` (Hold on change) | `check_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:280` | kit `toggle` ↔ `GhostMirrorHoldOnSourceChange` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_cast_ghosts.xml:340` `mirror_hold_check` |
| `pose` / `pose_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:284` | kit readout ← approved static copy; no runtime writer | none |
| `pose` / `mirror_repeat_held_check` (Repeat held) | `check_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:282` | kit `toggle` ↔ `GhostMirrorRepeatHeldAnimesh` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_cast_ghosts.xml:346` `mirror_repeat_held_check` |
| `reflect` / `reflect_orientation_combo` (Orientation) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:298` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::applyEntityTransform()` (`indra/newview/alpanelghoststudio.cpp:566`) | none |
| `reflect` / `reflect_plane_combo` (Plane) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:303` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::invalidateReflectPlane()` (`indra/newview/alpanelghoststudio.cpp:579`) | none |
| `reflect` / `reflect_position_check` (Mirror position) | `check_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:308` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::asBoolean()` (`indra/newview/alpanelghoststudio.cpp:590`) | none |
| `reflect` / `mirror_light_rig_check` (Mirror light rig) | `check_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:314` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::removeMirror()` (`indra/newview/alpanelghoststudio.cpp:550`) | none |
| `reflect` / `btn_reflect_pair` (Face pair) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:310` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::getSelected()` (`indra/newview/alpanelghoststudio.cpp:601`) | none |
| `reflect` / `reflect_distance_spinner` (Distance (m)) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:311` | kit `number` ↔ `GhostReflectFacePairDistance` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_cast_ghosts.xml:376` `reflect_distance_spinner` |
| `reflect` / `btn_reflect_recapture` (Recapture plane) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:309` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::getSelected()` (`indra/newview/alpanelghoststudio.cpp:607`) | none |
| `reflect` / `reflect_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:313` | kit readout ← approved static copy; no runtime writer | none |
| `chaos` / `chaos_slider` (Chaos amount) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:295` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onChaosCommit()` (`indra/newview/alpanelghoststudio.cpp:849`) | none |
| `chaos` / `anim_speed_spinner` (Anim speed) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:324` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onAnimSpeedCommit()` (`indra/newview/alpanelghoststudio.cpp:850`) | none |
| `entity` / `entity_look_combo` (Render look) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:315` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onEntityLookCommit()` (`indra/newview/alpanelghoststudio.cpp:931`) | none |
| `entity` / `physics_check` (Physics) | `check_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:327` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onPhysicsCommit()` (`indra/newview/alpanelghoststudio.cpp:861`) | none |
| `entity` / `anim_loop_mode_combo` (On finish) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:329` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onLoopModeCommit()` (`indra/newview/alpanelghoststudio.cpp:876`) | none |
| `entity` / `clone_fade_in_spinner` (Fade in (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:120` | kit `number` ↔ `GhostStudioFadeIn` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_cast_ghosts.xml:420` `clone_fade_in_spinner` |
| `entity` / `clone_fade_out_spinner` (Fade out (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:122` | kit `number` ↔ `GhostStudioFadeOut` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_cast_ghosts.xml:423` `clone_fade_out_spinner` |
| `ghost_playback` / `btn_anim_pause` (Pause) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:334` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickAnimPause()` (`indra/newview/alpanelghoststudio.cpp:862`) | none |
| `ghost_playback` / `btn_anim_resume` (Resume) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:336` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickAnimResume()` (`indra/newview/alpanelghoststudio.cpp:863`) | none |
| `ghost_playback` / `btn_anim_sync` (Sync) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:338` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickAnimSync()` (`indra/newview/alpanelghoststudio.cpp:877`) | none |
| `ghost_playback` / `clone_start_offset_spinner` (Start offset (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:396` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALGhostStudio::setStartOffset()` (`indra/newview/alpanelghoststudio.cpp:488`) | none |
| `ghost_playback` / `clone_offset_status` (Offset status) | `text` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:398` | kit readout ← approved static copy; no runtime writer | none |
| `anim_asset` / `directed_anim_editor` (Anim UUID) | `line_editor` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:344` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onDirectedAnimCommit()` (`indra/newview/alpanelghoststudio.cpp:865`) | none |
| `anim_asset` / `directed_anim_mirror_check` (Flip pose L/R) | `check_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:346` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onDirectedMirrorCommit()` (`indra/newview/alpanelghoststudio.cpp:866`) | none |
| `anim_asset` / `source_animations_combo` (Source playing) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:348` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onSourceAnimationCommit()` (`indra/newview/alpanelghoststudio.cpp:867`) | none |
| `anim_asset` / `animation_library_combo` (Inventory) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:340` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onAnimationLibraryCommit()` (`indra/newview/alpanelghoststudio.cpp:868`) | none |
| `anim_asset` / `btn_anim_library_refresh` (Refresh) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:342` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALGhostStudio::scanAnimLibrary()` (`indra/newview/alpanelghoststudio.cpp:869`) | none |
| `anim_asset` / `btn_clear_animations` (Clear) | none | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickClearAnimations()` (`indra/newview/alpanelghoststudio.cpp:628`) | none |
| `anim_asset` / `anim_metadata_text` (Metadata) | `text` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:350` | kit readout ← approved static copy; no runtime writer | none |
| `autoanim` / `autoanim_window_spinner` (Learn (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:354` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onAutoAnimWindow()` (`indra/newview/alpanelghoststudio.cpp:853`) | none |
| `autoanim` / `btn_autoanim_go` (Go autonomous) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:359` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onAutoAnimAction()` (`indra/newview/alpanelghoststudio.cpp:857`) | none |
| `autoanim` / `btn_autoanim_follow` (Follow source) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:360` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onAutoAnimAction()` (`indra/newview/alpanelghoststudio.cpp:858`) | none |
| `autoanim` / `btn_autoanim_stop` (Stop now) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:356` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onAutoAnimAction()` (`indra/newview/alpanelghoststudio.cpp:854`) | none |
| `autoanim` / `btn_autoanim_relearn` (Relearn) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:357` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onAutoAnimAction()` (`indra/newview/alpanelghoststudio.cpp:855`) | none |
| `autoanim` / `autoanim_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:363` | kit readout ← approved static copy; no runtime writer | none |
| `autoanim` / `btn_autoanim_reseed` (Reseed) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:358` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onAutoAnimAction()` (`indra/newview/alpanelghoststudio.cpp:856`) | none |
| `autoanim` / `btn_autoanim_save` (Save...) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:361` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onAutoAnimFile()` (`indra/newview/alpanelghoststudio.cpp:859`) | none |
| `autoanim` / `btn_autoanim_load` (Load...) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:362` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onAutoAnimFile()` (`indra/newview/alpanelghoststudio.cpp:860`) | none |
| `head_gaze` / `btn_lens_gaze_selection` (Gaze on) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:367` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickLensGazeSelection()` (`indra/newview/alpanelghoststudio.cpp:880`) | none |
| `head_gaze` / `lens_gaze_target_mode` (Target) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:372` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onLensGazeSettingsCommit()` (`indra/newview/alpanelghoststudio.cpp:884`) | none |
| `head_gaze` / `lens_gaze_cast_target` (Cast member) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:379` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onLensGazeSettingsCommit()` (`indra/newview/alpanelghoststudio.cpp:886`) | none |
| `head_gaze` / `lens_gaze_torso` (Torso) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:369` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onLensGazeTorsoCommit()` (`indra/newview/alpanelghoststudio.cpp:882`) | none |
| `head_gaze` / `lens_gaze_head_eye` (Head / eyes) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:381` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onLensGazeSettingsCommit()` (`indra/newview/alpanelghoststudio.cpp:888`) | none |
| `head_gaze` / `lens_gaze_intensity` (Intensity) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:384` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onLensGazeSettingsCommit()` (`indra/newview/alpanelghoststudio.cpp:890`) | none |
| `head_gaze` / `lens_gaze_smoothing` (Smoothing) | `slider` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:387` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onLensGazeSettingsCommit()` (`indra/newview/alpanelghoststudio.cpp:892`) | none |
| `head_gaze` / `lens_gaze_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:390` | kit readout ← approved static copy; no runtime writer | none |
| `crowd_pattern` / `btn_array_line` (Line) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:417` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickArray()` (`indra/newview/alpanelghoststudio.cpp:938`) | none |
| `crowd_pattern` / `btn_array_ring` (Ring) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:419` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickArray()` (`indra/newview/alpanelghoststudio.cpp:940`) | none |
| `crowd_pattern` / `array_formation_combo` (Formation) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:421` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onFormationCommit()` (`indra/newview/alpanelghoststudio.cpp:942`), wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:4656`) | none |
| `crowd_pattern` / `array_count_spinner` (Count) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:411` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:4656`) | none |
| `crowd_pattern` / `array_spacing_spinner` (Spacing (m)) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:414` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:4656`) | none |
| `crowd_pattern` / `array_width_spinner` (Width (m)) | none | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:4656`) | none |
| `crowd_pattern` / `array_depth_spinner` (Depth (m)) | none | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:4656`) | none |
| `crowd_pattern` / `array_columns_spinner` (Columns) | none | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:4656`) | none |
| `crowd_pattern` / `array_rows_spinner` (Rows) | none | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:4656`) | none |
| `crowd_layout` / `formation_edge_gap_spinner` (Edge gap) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:467` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:4656`) | none |
| `crowd_layout` / `formation_jitter_spinner` (Jitter) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:469` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:4656`) | none |
| `crowd_layout` / `formation_seed_spinner` (Seed) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:475` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALGhostStudio::getInstance()` (`indra/newview/alpanelghoststudio.cpp:983`), wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:4656`) | none |
| `crowd_layout` / `formation_terrain_check` (Snap to ground) | `check_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:464` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:4656`) | none |
| `crowd_layout` / `formation_facing_combo` (Heading) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:482` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:4656`) | none |
| `crowd_layout` / `facing_yaw_offset_spinner` (Facing offset) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:510` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALGhostStudio::getCrowdPlacementDraft()` (`indra/newview/alpanelghoststudio.cpp:996`) | none |
| `crowd_layout` / `btn_pick_facing_target` (Pick facing) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:507` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickPickFacingTarget()` (`indra/newview/alpanelghoststudio.cpp:1021`) | none |
| `crowd_layout` / `preserve_source_tilt_check` (Source tilt) | `check_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:512` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:4656`) | none |
| `crowd_layout` / `crowd_start_mode_combo` (Start offset) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:556` | kit `combo` ↔ `GhostStudioStartMode` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_cast_ghosts.xml:647` `crowd_start_mode_combo` |
| `crowd_layout` / `crowd_start_spread_spinner` (Spread (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:561` | kit `number` ↔ `GhostStudioStartSpread` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_cast_ghosts.xml:656` `crowd_start_spread_spinner` |
| `crowd_layout` / `crowd_ripple_from_combo` (Ripple from) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:564` | kit `combo` ↔ `GhostStudioRippleFrom` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_cast_ghosts.xml:659` `crowd_ripple_from_combo` |
| `crowd_guide` / `formation_guide_shape_combo` (Guide) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:456` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:4656`) | none |
| `crowd_guide` / `formation_guide_size_spinner` (Guide size) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:462` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:4656`) | none |
| `crowd_guide` / `crowd_copies_as_combo` (Copies as) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:449` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:4656`) | none |
| `crowd_guide` / `crowd_create_mode_combo` (Create as) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:501` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:4656`) | none |
| `crowd_guide` / `nameplate_mode_combo` (Names) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:537` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onNameplateModeCommit()` (`indra/newview/alpanelghoststudio.cpp:1041`) | none |
| `crowd_guide` / `formation_overlay_palette_combo` (Marker colour) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:523` | kit `combo` ↔ `GhostStudioOverlayPalette` `S32`; `ALGhostStudioKitAdapter` preserves/calls wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:4656`) | `indra/newview/skins/default/xui/en/panel_dpage_cast_ghosts.xml:709` `formation_overlay_palette_combo` |
| `crowd_guide` / `crowd_looks_mode_combo` (Looks) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:570` | kit `combo` ↔ `GhostStudioLooksMode` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_cast_ghosts.xml:720` `crowd_looks_mode_combo` |
| `crowd_guide` / `crowd_look_spread_combo` (Look spread) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:575` | kit `combo` ↔ `GhostStudioLookSpread` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_cast_ghosts.xml:728` `crowd_look_spread_combo` |
| `crowd_guide` / `crowd_mix_look_1` (Look 1) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:580` | kit `combo` ↔ `GhostStudioMixLook1` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_cast_ghosts.xml:736` `crowd_mix_look_1` |
| `crowd_guide` / `crowd_mix_look_2` (Look 2) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:647` | kit `combo` ↔ `GhostStudioMixLook2` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_cast_ghosts.xml:806` `crowd_mix_look_2` |
| `crowd_guide` / `crowd_mix_look_3` (Look 3) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:714` | kit `combo` ↔ `GhostStudioMixLook3` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_cast_ghosts.xml:876` `crowd_mix_look_3` |
| `crowd_guide` / `crowd_mix_look_4` (Look 4) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:781` | kit `combo` ↔ `GhostStudioMixLook4` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_cast_ghosts.xml:946` `crowd_mix_look_4` |
| `crowd_guide` / `crowd_mix_look_5` (Look 5) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:848` | kit `combo` ↔ `GhostStudioMixLook5` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_cast_ghosts.xml:1016` `crowd_mix_look_5` |
| `crowd_guide` / `crowd_mix_look_6` (Look 6) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:915` | kit `combo` ↔ `GhostStudioMixLook6` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_cast_ghosts.xml:1086` `crowd_mix_look_6` |
| `crowd_preview` / `btn_array_build` (Preview) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:515` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickBuildArray()` (`indra/newview/alpanelghoststudio.cpp:1011`) | none |
| `crowd_preview` / `btn_crowd_confirm` (Confirm) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:517` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickCrowdConfirm()` (`indra/newview/alpanelghoststudio.cpp:1012`) | none |
| `crowd_preview` / `btn_crowd_unpin` (Unpin) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:519` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickCrowdUnpin()` (`indra/newview/alpanelghoststudio.cpp:1014`) | none |
| `crowd_preview` / `btn_crowd_cancel` (Cancel) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:521` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickCrowdCancel()` (`indra/newview/alpanelghoststudio.cpp:1016`) | none |
| `crowd_preview` / `crowd_placement_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:532` | kit readout ← approved static copy; no runtime writer | none |
| `crowd_preview` / `crowd_placement_instructions` (Controls) | `text` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:534` | kit readout ← approved static copy; no runtime writer | none |
| `group_members` / `group_edit_members_check` (Edit members) | `check_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:543` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onGroupEditMembersCommit()` (`indra/newview/alpanelghoststudio.cpp:1035`) | none |
| `group_members` / `lock_mode_combo` (Group lock) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:545` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALGhostStudio::getCrowdPlacementDraft()` (`indra/newview/alpanelghoststudio.cpp:1025`) | none |
| `group_members` / `group_member_pinned_check` (Pin offset) | `check_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:551` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onGroupMemberPinnedCommit()` (`indra/newview/alpanelghoststudio.cpp:1037`) | none |
| `group_members` / `btn_group_member_reset` (Reset member) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:553` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickGroupMemberReset()` (`indra/newview/alpanelghoststudio.cpp:1039`) | none |
| `crowd_brigade` / `brigade_columns_spinner` (Files) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:478` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:4656`) | none |
| `crowd_brigade` / `brigade_rows_spinner` (Ranks) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:479` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:4656`) | none |
| `crowd_brigade` / `brigade_file_spacing_spinner` (File gap) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:477` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:4656`) | none |
| `crowd_brigade` / `brigade_rank_spacing_spinner` (Rank gap) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:480` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:4656`) | none |
| `crowd_brigade` / `array_parameter_spinner` (Parameter) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:472` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::captureFormationParameter()` (`indra/newview/alpanelghoststudio.cpp:943`), wired in `ALPanelGhostStudio::registerKitParameterReset()`; behavior `ALKitBindingRegistry::add()` (`indra/newview/alpanelghoststudio.cpp:5386`), wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:4656`) | none |
| `pose_strip` / `strip_count_spinner` (Frames) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:986` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickStartStrip()` (`indra/newview/alpanelghoststudio.cpp:5064`) | none |
| `pose_strip` / `strip_interval_spinner` (Interval (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:989` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickStartStrip()` (`indra/newview/alpanelghoststudio.cpp:5064`) | none |
| `pose_strip` / `btn_strip_start` (Capture) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:992` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickStartStrip()` (`indra/newview/alpanelghoststudio.cpp:1018`) | none |
| `pose_strip` / `btn_strip_cancel` (Cancel) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:994` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickCancelStrip()` (`indra/newview/alpanelghoststudio.cpp:1019`) | none |
| `pose_strip` / `strip_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:996` | kit readout ← approved static copy; no runtime writer | none |
| `formation_motion` / `formation_motion_combo` (Motion) | `combo_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:997` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onMotionCommit()` (`indra/newview/alpanelghoststudio.cpp:5095`) | none |
| `formation_motion` / `formation_motion_speed` (Speed) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:1014` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:4656`) | none |
| `formation_motion` / `formation_motion_amplitude` (Amp) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:1017` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::updateOwnedCrowdPlacement()` (`indra/newview/alpanelghoststudio.cpp:4656`) | none |
| `formation_motion` / `btn_formation_motion_apply` (Apply) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:1020` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onMotionCommit()` (`indra/newview/alpanelghoststudio.cpp:1020`) | none |
| `formation_motion` / `pose_rate_spinner` (Pose fps) | `spinner` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:1026` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onPoseRateCommit()` (`indra/newview/alpanelghoststudio.cpp:1043`) | none |
| `formation_motion` / `motion_animations_check` (Motion animations) | `check_box` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:1028` | kit `toggle` ↔ `GhostMotionAnimations` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_cast_ghosts.xml:1264` `motion_animations_check` |
| `directing_unit` / `btn_group_unit` (Group) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:1022` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickGroup()` (`indra/newview/alpanelghoststudio.cpp:1023`) | none |
| `directing_unit` / `btn_ungroup_unit` (Ungroup) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:1024` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickUngroup()` (`indra/newview/alpanelghoststudio.cpp:1024`) | none |
| `directing_unit` / `motion_precedence_help` (Note) | `text` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:1030` | kit readout ← approved static copy; no runtime writer | none |
| `ghost_diagnostics` / `btn_rig_diag_refresh` (Refresh) | `button` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:393` | `ALGhostStudioKitAdapter` command/model binding → wired in `ALPanelGhostStudio::postBuild()`; behavior `ALPanelGhostStudio::onClickRigDiagRefresh()` (`indra/newview/alpanelghoststudio.cpp:935`) | none |
| `ghost_diagnostics` / `rig_diagnostics` (Report) | `text_editor` `indra/newview/skins/default/xui/en/panel_ghost_studio.xml:395` | kit readout ← approved static copy; no runtime writer | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALGhostCloneActions::swapSource()` — `indra/newview/alpanelghoststudio.cpp:473` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALGhostCloneActions::toggleSolo()` — `indra/newview/alpanelghoststudio.cpp:483` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALGhostStudio::getCrowdPlacementDraft()` — `indra/newview/alpanelghoststudio.cpp:996, 1025` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALGhostStudio::getInstance()` — `indra/newview/alpanelghoststudio.cpp:983` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALGhostStudio::scanAnimLibrary()` — `indra/newview/alpanelghoststudio.cpp:869` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALGhostStudio::setStartOffset()` — `indra/newview/alpanelghoststudio.cpp:488` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALKitBindingRegistry::add()` — `indra/newview/alpanelghoststudio.cpp:5386` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::<inline callback>()` — `indra/newview/alpanelghoststudio.cpp:500` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::applyEntityTransform()` — `indra/newview/alpanelghoststudio.cpp:566` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::asBoolean()` — `indra/newview/alpanelghoststudio.cpp:590` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::asReal()` — `indra/newview/alpanelghoststudio.cpp:843, 845` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::captureFormationParameter()` — `indra/newview/alpanelghoststudio.cpp:943` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::getSelected()` — `indra/newview/alpanelghoststudio.cpp:601, 607` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::invalidateReflectPlane()` — `indra/newview/alpanelghoststudio.cpp:579` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onActorTintToggle()` — `indra/newview/alpanelghoststudio.cpp:914` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onAlphaCommit()` — `indra/newview/alpanelghoststudio.cpp:916` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onAnimSpeedCommit()` — `indra/newview/alpanelghoststudio.cpp:850` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onAnimationLibraryCommit()` — `indra/newview/alpanelghoststudio.cpp:868` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onAutoAnimAction()` — `indra/newview/alpanelghoststudio.cpp:854, 855, 856, 857, 858` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onAutoAnimFile()` — `indra/newview/alpanelghoststudio.cpp:859, 860` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onAutoAnimWindow()` — `indra/newview/alpanelghoststudio.cpp:853` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onBrightnessCommit()` — `indra/newview/alpanelghoststudio.cpp:929` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onChaosCommit()` — `indra/newview/alpanelghoststudio.cpp:849` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickAdd()` — `indra/newview/alpanelghoststudio.cpp:829, 2694` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickAlignFeet()` — `indra/newview/alpanelghoststudio.cpp:900` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickAnimPause()` — `indra/newview/alpanelghoststudio.cpp:862` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickAnimResume()` — `indra/newview/alpanelghoststudio.cpp:863` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickAnimSync()` — `indra/newview/alpanelghoststudio.cpp:877` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickArray()` — `indra/newview/alpanelghoststudio.cpp:938, 940` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickBuildArray()` — `indra/newview/alpanelghoststudio.cpp:1011` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickCancelStrip()` — `indra/newview/alpanelghoststudio.cpp:1019` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickClearAnimations()` — `indra/newview/alpanelghoststudio.cpp:628` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickCopyTransform()` — `indra/newview/alpanelghoststudio.cpp:903` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickCrowdCancel()` — `indra/newview/alpanelghoststudio.cpp:1016` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickCrowdConfirm()` — `indra/newview/alpanelghoststudio.cpp:1012` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickCrowdUnpin()` — `indra/newview/alpanelghoststudio.cpp:1014` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickDelete()` — `indra/newview/alpanelghoststudio.cpp:832` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickDrop()` — `indra/newview/alpanelghoststudio.cpp:899` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickDuplicate()` — `indra/newview/alpanelghoststudio.cpp:831` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickDuplicateInPlace()` — `indra/newview/alpanelghoststudio.cpp:902` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickFaceNow()` — `indra/newview/alpanelghoststudio.cpp:908` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickFreeze()` — `indra/newview/alpanelghoststudio.cpp:933` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickGroup()` — `indra/newview/alpanelghoststudio.cpp:1023` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickGroupMemberReset()` — `indra/newview/alpanelghoststudio.cpp:1039` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickLensGazeSelection()` — `indra/newview/alpanelghoststudio.cpp:880` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickLive()` — `indra/newview/alpanelghoststudio.cpp:934` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickPastePosition()` — `indra/newview/alpanelghoststudio.cpp:905` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickPasteTransform()` — `indra/newview/alpanelghoststudio.cpp:904` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickPickFacingTarget()` — `indra/newview/alpanelghoststudio.cpp:1021` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickPlace()` — `indra/newview/alpanelghoststudio.cpp:896` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickRefresh()` — `indra/newview/alpanelghoststudio.cpp:833` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickRigDiagRefresh()` — `indra/newview/alpanelghoststudio.cpp:935` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickStartStrip()` — `indra/newview/alpanelghoststudio.cpp:1018, 5064` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickToActor()` — `indra/newview/alpanelghoststudio.cpp:897` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickToMe()` — `indra/newview/alpanelghoststudio.cpp:898` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickUngroup()` — `indra/newview/alpanelghoststudio.cpp:1024` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onClickUpright()` — `indra/newview/alpanelghoststudio.cpp:901` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onDirectedAnimCommit()` — `indra/newview/alpanelghoststudio.cpp:865` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onDirectedMirrorCommit()` — `indra/newview/alpanelghoststudio.cpp:866` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onDistortAmountCommit()` — `indra/newview/alpanelghoststudio.cpp:928` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onDistortCommit()` — `indra/newview/alpanelghoststudio.cpp:927` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onDriveModeCommit()` — `indra/newview/alpanelghoststudio.cpp:864` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onDropoutCommit()` — `indra/newview/alpanelghoststudio.cpp:922` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onEffectFpsCommit()` — `indra/newview/alpanelghoststudio.cpp:930` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onEntityLookCommit()` — `indra/newview/alpanelghoststudio.cpp:931` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onFormationCommit()` — `indra/newview/alpanelghoststudio.cpp:942` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onGlitchCommit()` — `indra/newview/alpanelghoststudio.cpp:920` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onGroupEditMembersCommit()` — `indra/newview/alpanelghoststudio.cpp:1035` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onGroupMemberPinnedCommit()` — `indra/newview/alpanelghoststudio.cpp:1037` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onHeadingDialCommit()` — `indra/newview/alpanelghoststudio.cpp:840` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onHueCommit()` — `indra/newview/alpanelghoststudio.cpp:915` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onInterlaceCommit()` — `indra/newview/alpanelghoststudio.cpp:925` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onKeepFacingCommit()` — `indra/newview/alpanelghoststudio.cpp:910` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onLensGazeSettingsCommit()` — `indra/newview/alpanelghoststudio.cpp:884, 886, 888, 890, 892` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onLensGazeTorsoCommit()` — `indra/newview/alpanelghoststudio.cpp:882` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onListMouseUp()` — `indra/newview/alpanelghoststudio.cpp:826` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onListSelect()` — `indra/newview/alpanelghoststudio.cpp:824` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onLookTargetCommit()` — `indra/newview/alpanelghoststudio.cpp:906` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onLoopModeCommit()` — `indra/newview/alpanelghoststudio.cpp:876` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onMotionCommit()` — `indra/newview/alpanelghoststudio.cpp:1020, 5095` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onNameCommit()` — `indra/newview/alpanelghoststudio.cpp:830` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onNameplateModeCommit()` — `indra/newview/alpanelghoststudio.cpp:1041` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onPhysicsCommit()` — `indra/newview/alpanelghoststudio.cpp:861` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onPixelCommit()` — `indra/newview/alpanelghoststudio.cpp:919` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onPosCommit()` — `indra/newview/alpanelghoststudio.cpp:835, 836, 837` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onPoseRateCommit()` — `indra/newview/alpanelghoststudio.cpp:1043` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onPositionJitterCommit()` — `indra/newview/alpanelghoststudio.cpp:926` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onPowerFlickerCommit()` — `indra/newview/alpanelghoststudio.cpp:923` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onScaleCommit()` — `indra/newview/alpanelghoststudio.cpp:847` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onShimmerAmountCommit()` — `indra/newview/alpanelghoststudio.cpp:918` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onShimmerSpeedCommit()` — `indra/newview/alpanelghoststudio.cpp:917` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onShowAllToggle()` — `indra/newview/alpanelghoststudio.cpp:822` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onSliceCommit()` — `indra/newview/alpanelghoststudio.cpp:921` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onSourceAnimationCommit()` — `indra/newview/alpanelghoststudio.cpp:867` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onStyleCommit()` — `indra/newview/alpanelghoststudio.cpp:913` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onToggleEditMode()` — `indra/newview/alpanelghoststudio.cpp:823` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onWipeCommit()` — `indra/newview/alpanelghoststudio.cpp:924` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::onYawCommit()` — `indra/newview/alpanelghoststudio.cpp:842` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::removeMirror()` — `indra/newview/alpanelghoststudio.cpp:550` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelGhostStudio::updateOwnedCrowdPlacement()` — `indra/newview/alpanelghoststudio.cpp:4656` | Move model-facing work into `ALGhostStudioKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **350 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

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
| `clone_type_combo-option-0` | Only `clone_type_combo` selects `Overlay clone`; other selectors remain default. |
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
| `look_target_combo-option-3` | Only `look_target_combo` selects `Clone: Clone 1`; other selectors remain default. |
| `keep_facing_check-on` | Only `keep_facing_check` is on; dependent visibility is measured. |
| `card-ghost_look-tallest-850` | Only `ghost_look` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-ghost_look-tallest-1160` | Only `ghost_look` is put in its tallest local state at 1160 px; all other cards stay default. |
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
| `style_combo-option-28` | Only `style_combo` selects `Glass`; other selectors remain default. |
| `style_combo-option-29` | Only `style_combo` selects `Mercury`; other selectors remain default. |
| `style_combo-option-30` | Only `style_combo` selects `Marble`; other selectors remain default. |
| `style_combo-option-31` | Only `style_combo` selects `Crystal`; other selectors remain default. |
| `style_combo-option-32` | Only `style_combo` selects `Magma`; other selectors remain default. |
| `style_combo-option-33` | Only `style_combo` selects `Void`; other selectors remain default. |
| `style_combo-option-34` | Only `style_combo` selects `Sumi-e`; other selectors remain default. |
| `style_combo-option-35` | Only `style_combo` selects `Lidar`; other selectors remain default. |
| `style_combo-option-36` | Only `style_combo` selects `Code Rain`; other selectors remain default. |
| `style_combo-option-37` | Only `style_combo` selects `Datamosh`; other selectors remain default. |
| `style_combo-option-38` | Only `style_combo` selects `Scan Build`; other selectors remain default. |
| `style_combo-option-39` | Only `style_combo` selects `Contour`; other selectors remain default. |
| `style_combo-option-40` | Only `style_combo` selects `Decode`; other selectors remain default. |
| `style_combo-option-41` | Only `style_combo` selects `Glyph Skin`; other selectors remain default. |
| `style_combo-option-42` | Only `style_combo` selects `Construct Wire`; other selectors remain default. |
| `style_combo-option-43` | Only `style_combo` selects `Deja Vu`; other selectors remain default. |
| `style_combo-option-44` | Only `style_combo` selects `Operator`; other selectors remain default. |
| `style_combo-option-45` | Only `style_combo` selects `Dial Out`; other selectors remain default. |
| `style_combo-option-46` | Only `style_combo` selects `Source`; other selectors remain default. |
| `style_combo-option-47` | Only `style_combo` selects `Aureole`; other selectors remain default. |
| `style_combo-option-48` | Only `style_combo` selects `Choir Light`; other selectors remain default. |
| `style_combo-option-49` | Only `style_combo` selects `Featherfall`; other selectors remain default. |
| `style_combo-option-50` | Only `style_combo` selects `Rapture`; other selectors remain default. |
| `style_combo-option-51` | Only `style_combo` selects `Emitter`; other selectors remain default. |
| `style_combo-option-52` | Only `style_combo` selects `Bad Reception`; other selectors remain default. |
| `style_combo-option-53` | Only `style_combo` selects `Static Veil`; other selectors remain default. |
| `style_combo-option-54` | Only `style_combo` selects `Latency`; other selectors remain default. |
| `style_combo-option-55` | Only `style_combo` selects `Facets`; other selectors remain default. |
| `style_combo-option-56` | Only `style_combo` selects `Cold Gaze`; other selectors remain default. |
| `style_combo-option-57` | Only `style_combo` selects `Intrusion`; other selectors remain default. |
| `style_combo-option-58` | Only `style_combo` selects `Overclock`; other selectors remain default. |
| `style_combo-option-59` | Only `style_combo` selects `Reform`; other selectors remain default. |
| `style_combo-option-60` | Only `style_combo` selects `Torn Husk`; other selectors remain default. |
| `style_combo-option-61` | Only `style_combo` selects `Hunter Vision`; other selectors remain default. |
| `style_combo-option-62` | Only `style_combo` selects `Ooze`; other selectors remain default. |
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
| `drive_mode_combo-option-4` | Only `drive_mode_combo` selects `Reflect`; other selectors remain default. |
| `mirror_hold_check-on` | Only `mirror_hold_check` is on; dependent visibility is measured. |
| `mirror_repeat_held_check-on` | Only `mirror_repeat_held_check` is on; dependent visibility is measured. |
| `card-reflect-tallest-850` | Only `reflect` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-reflect-tallest-1160` | Only `reflect` is put in its tallest local state at 1160 px; all other cards stay default. |
| `reflect_orientation_combo-option-0` | Only `reflect_orientation_combo` selects `Wall`; other selectors remain default. |
| `reflect_orientation_combo-option-1` | Only `reflect_orientation_combo` selects `Floor`; other selectors remain default. |
| `reflect_plane_combo-option-0` | Only `reflect_plane_combo` selects `Fixed`; other selectors remain default. |
| `reflect_plane_combo-option-1` | Only `reflect_plane_combo` selects `Live`; other selectors remain default. |
| `reflect_position_check-on` | Only `reflect_position_check` is on; dependent visibility is measured. |
| `mirror_light_rig_check-on` | Only `mirror_light_rig_check` is on; dependent visibility is measured. |
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
| `card-ghost_playback-tallest-850` | Only `ghost_playback` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-ghost_playback-tallest-1160` | Only `ghost_playback` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-anim_asset-tallest-850` | Only `anim_asset` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-anim_asset-tallest-1160` | Only `anim_asset` is put in its tallest local state at 1160 px; all other cards stay default. |
| `directed_anim_mirror_check-on` | Only `directed_anim_mirror_check` is on; dependent visibility is measured. |
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
| `crowd_start_mode_combo-option-0` | Only `crowd_start_mode_combo` selects `Off`; other selectors remain default. |
| `crowd_start_mode_combo-option-1` | Only `crowd_start_mode_combo` selects `Random`; other selectors remain default. |
| `crowd_start_mode_combo-option-2` | Only `crowd_start_mode_combo` selects `Ripple`; other selectors remain default. |
| `crowd_ripple_from_combo-option-0` | Only `crowd_ripple_from_combo` selects `Front to back`; other selectors remain default. |
| `crowd_ripple_from_combo-option-1` | Only `crowd_ripple_from_combo` selects `Left to right`; other selectors remain default. |
| `crowd_ripple_from_combo-option-2` | Only `crowd_ripple_from_combo` selects `Centre out`; other selectors remain default. |
| `card-crowd_guide-tallest-850` | Only `crowd_guide` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-crowd_guide-tallest-1160` | Only `crowd_guide` is put in its tallest local state at 1160 px; all other cards stay default. |
| `formation_guide_shape_combo-option-0` | Only `formation_guide_shape_combo` selects `Guide: Off`; other selectors remain default. |
| `formation_guide_shape_combo-option-1` | Only `formation_guide_shape_combo` selects `Guide: Circle`; other selectors remain default. |
| `formation_guide_shape_combo-option-2` | Only `formation_guide_shape_combo` selects `Guide: Square`; other selectors remain default. |
| `crowd_copies_as_combo-option-0` | Only `crowd_copies_as_combo` selects `Copies as: Select clone`; other selectors remain default. |
| `crowd_copies_as_combo-option-1` | Only `crowd_copies_as_combo` selects `Copies as: Overlay clone`; other selectors remain default. |
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
| `crowd_looks_mode_combo-option-0` | Only `crowd_looks_mode_combo` selects `Single`; other selectors remain default. |
| `crowd_looks_mode_combo-option-1` | Only `crowd_looks_mode_combo` selects `Mix`; other selectors remain default. |
| `crowd_look_spread_combo-option-0` | Only `crowd_look_spread_combo` selects `Random`; other selectors remain default. |
| `crowd_look_spread_combo-option-1` | Only `crowd_look_spread_combo` selects `By row`; other selectors remain default. |
| `crowd_mix_look_1-option-0` | Only `crowd_mix_look_1` selects `None`; other selectors remain default. |
| `crowd_mix_look_1-option-1` | Only `crowd_mix_look_1` selects `Ghost`; other selectors remain default. |
| `crowd_mix_look_1-option-2` | Only `crowd_mix_look_1` selects `Clone`; other selectors remain default. |
| `crowd_mix_look_1-option-3` | Only `crowd_mix_look_1` selects `Hologram`; other selectors remain default. |
| `crowd_mix_look_1-option-4` | Only `crowd_mix_look_1` selects `Wireframe`; other selectors remain default. |
| `crowd_mix_look_1-option-5` | Only `crowd_mix_look_1` selects `X-ray`; other selectors remain default. |
| `crowd_mix_look_1-option-6` | Only `crowd_mix_look_1` selects `Thermal`; other selectors remain default. |
| `crowd_mix_look_1-option-7` | Only `crowd_mix_look_1` selects `Neon Outline`; other selectors remain default. |
| `crowd_mix_look_1-option-8` | Only `crowd_mix_look_1` selects `Silhouette`; other selectors remain default. |
| `crowd_mix_look_1-option-9` | Only `crowd_mix_look_1` selects `Toon/Ink`; other selectors remain default. |
| `crowd_mix_look_1-option-10` | Only `crowd_mix_look_1` selects `Chrome`; other selectors remain default. |
| `crowd_mix_look_1-option-11` | Only `crowd_mix_look_1` selects `Dissolve`; other selectors remain default. |
| `crowd_mix_look_1-option-12` | Only `crowd_mix_look_1` selects `Negative`; other selectors remain default. |
| `crowd_mix_look_1-option-13` | Only `crowd_mix_look_1` selects `Gold Statue`; other selectors remain default. |
| `crowd_mix_look_1-option-14` | Only `crowd_mix_look_1` selects `Night-vision`; other selectors remain default. |
| `crowd_mix_look_1-option-15` | Only `crowd_mix_look_1` selects `Blueprint`; other selectors remain default. |
| `crowd_mix_look_1-option-16` | Only `crowd_mix_look_1` selects `Ectoplasm`; other selectors remain default. |
| `crowd_mix_look_1-option-17` | Only `crowd_mix_look_1` selects `Frost/Ice`; other selectors remain default. |
| `crowd_mix_look_1-option-18` | Only `crowd_mix_look_1` selects `Prism`; other selectors remain default. |
| `crowd_mix_look_1-option-19` | Only `crowd_mix_look_1` selects `Thermal Scope`; other selectors remain default. |
| `crowd_mix_look_1-option-20` | Only `crowd_mix_look_1` selects `Wallhack/ESP`; other selectors remain default. |
| `crowd_mix_look_1-option-21` | Only `crowd_mix_look_1` selects `Night-vision Tube`; other selectors remain default. |
| `crowd_mix_look_1-option-22` | Only `crowd_mix_look_1` selects `Damage Overlay`; other selectors remain default. |
| `crowd_mix_look_1-option-23` | Only `crowd_mix_look_1` selects `Killcam`; other selectors remain default. |
| `crowd_mix_look_1-option-24` | Only `crowd_mix_look_1` selects `Oil Slick`; other selectors remain default. |
| `crowd_mix_look_1-option-25` | Only `crowd_mix_look_1` selects `Vaporwave`; other selectors remain default. |
| `crowd_mix_look_1-option-26` | Only `crowd_mix_look_1` selects `Halftone/Comic`; other selectors remain default. |
| `crowd_mix_look_1-option-27` | Only `crowd_mix_look_1` selects `Sonar Reveal`; other selectors remain default. |
| `crowd_mix_look_1-option-28` | Only `crowd_mix_look_1` selects `Hologram Echo`; other selectors remain default. |
| `crowd_mix_look_1-option-29` | Only `crowd_mix_look_1` selects `Glass`; other selectors remain default. |
| `crowd_mix_look_1-option-30` | Only `crowd_mix_look_1` selects `Mercury`; other selectors remain default. |
| `crowd_mix_look_1-option-31` | Only `crowd_mix_look_1` selects `Marble`; other selectors remain default. |
| `crowd_mix_look_1-option-32` | Only `crowd_mix_look_1` selects `Crystal`; other selectors remain default. |
| `crowd_mix_look_1-option-33` | Only `crowd_mix_look_1` selects `Magma`; other selectors remain default. |
| `crowd_mix_look_1-option-34` | Only `crowd_mix_look_1` selects `Void`; other selectors remain default. |
| `crowd_mix_look_1-option-35` | Only `crowd_mix_look_1` selects `Sumi-e`; other selectors remain default. |
| `crowd_mix_look_1-option-36` | Only `crowd_mix_look_1` selects `Lidar`; other selectors remain default. |
| `crowd_mix_look_1-option-37` | Only `crowd_mix_look_1` selects `Code Rain`; other selectors remain default. |
| `crowd_mix_look_1-option-38` | Only `crowd_mix_look_1` selects `Datamosh`; other selectors remain default. |
| `crowd_mix_look_1-option-39` | Only `crowd_mix_look_1` selects `Scan Build`; other selectors remain default. |
| `crowd_mix_look_1-option-40` | Only `crowd_mix_look_1` selects `Contour`; other selectors remain default. |
| `crowd_mix_look_1-option-41` | Only `crowd_mix_look_1` selects `Decode`; other selectors remain default. |
| `crowd_mix_look_1-option-42` | Only `crowd_mix_look_1` selects `Glyph Skin`; other selectors remain default. |
| `crowd_mix_look_1-option-43` | Only `crowd_mix_look_1` selects `Construct Wire`; other selectors remain default. |
| `crowd_mix_look_1-option-44` | Only `crowd_mix_look_1` selects `Deja Vu`; other selectors remain default. |
| `crowd_mix_look_1-option-45` | Only `crowd_mix_look_1` selects `Operator`; other selectors remain default. |
| `crowd_mix_look_1-option-46` | Only `crowd_mix_look_1` selects `Dial Out`; other selectors remain default. |
| `crowd_mix_look_1-option-47` | Only `crowd_mix_look_1` selects `Source`; other selectors remain default. |
| `crowd_mix_look_1-option-48` | Only `crowd_mix_look_1` selects `Aureole`; other selectors remain default. |
| `crowd_mix_look_1-option-49` | Only `crowd_mix_look_1` selects `Choir Light`; other selectors remain default. |
| `crowd_mix_look_1-option-50` | Only `crowd_mix_look_1` selects `Featherfall`; other selectors remain default. |
| `crowd_mix_look_1-option-51` | Only `crowd_mix_look_1` selects `Rapture`; other selectors remain default. |
| `crowd_mix_look_1-option-52` | Only `crowd_mix_look_1` selects `Emitter`; other selectors remain default. |
| `crowd_mix_look_1-option-53` | Only `crowd_mix_look_1` selects `Bad Reception`; other selectors remain default. |
| `crowd_mix_look_1-option-54` | Only `crowd_mix_look_1` selects `Static Veil`; other selectors remain default. |
| `crowd_mix_look_1-option-55` | Only `crowd_mix_look_1` selects `Latency`; other selectors remain default. |
| `crowd_mix_look_1-option-56` | Only `crowd_mix_look_1` selects `Facets`; other selectors remain default. |
| `crowd_mix_look_1-option-57` | Only `crowd_mix_look_1` selects `Cold Gaze`; other selectors remain default. |
| `crowd_mix_look_1-option-58` | Only `crowd_mix_look_1` selects `Intrusion`; other selectors remain default. |
| `crowd_mix_look_1-option-59` | Only `crowd_mix_look_1` selects `Overclock`; other selectors remain default. |
| `crowd_mix_look_1-option-60` | Only `crowd_mix_look_1` selects `Reform`; other selectors remain default. |
| `crowd_mix_look_1-option-61` | Only `crowd_mix_look_1` selects `Torn Husk`; other selectors remain default. |
| `crowd_mix_look_1-option-62` | Only `crowd_mix_look_1` selects `Hunter Vision`; other selectors remain default. |
| `crowd_mix_look_1-option-63` | Only `crowd_mix_look_1` selects `Ooze`; other selectors remain default. |
| `crowd_mix_look_2-option-0` | Only `crowd_mix_look_2` selects `None`; other selectors remain default. |
| `crowd_mix_look_2-option-21` | Only `crowd_mix_look_2` selects `Night-vision Tube`; other selectors remain default. |
| `crowd_mix_look_3-option-0` | Only `crowd_mix_look_3` selects `None`; other selectors remain default. |
| `crowd_mix_look_3-option-21` | Only `crowd_mix_look_3` selects `Night-vision Tube`; other selectors remain default. |
| `crowd_mix_look_4-option-0` | Only `crowd_mix_look_4` selects `None`; other selectors remain default. |
| `crowd_mix_look_4-option-21` | Only `crowd_mix_look_4` selects `Night-vision Tube`; other selectors remain default. |
| `crowd_mix_look_5-option-0` | Only `crowd_mix_look_5` selects `None`; other selectors remain default. |
| `crowd_mix_look_5-option-21` | Only `crowd_mix_look_5` selects `Night-vision Tube`; other selectors remain default. |
| `crowd_mix_look_6-option-0` | Only `crowd_mix_look_6` selects `None`; other selectors remain default. |
| `crowd_mix_look_6-option-21` | Only `crowd_mix_look_6` selects `Night-vision Tube`; other selectors remain default. |
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
| `formation_motion_combo-option-6` | Only `formation_motion_combo` selects `Hover`; other selectors remain default. |
| `formation_motion_combo-option-7` | Only `formation_motion_combo` selects `Jump`; other selectors remain default. |
| `formation_motion_combo-option-8` | Only `formation_motion_combo` selects `Space walk`; other selectors remain default. |
| `formation_motion_combo-option-9` | Only `formation_motion_combo` selects `Float in space`; other selectors remain default. |
| `formation_motion_combo-option-10` | Only `formation_motion_combo` selects `Ascend & hold`; other selectors remain default. |
| `formation_motion_combo-option-11` | Only `formation_motion_combo` selects `Vortex`; other selectors remain default. |
| `formation_motion_combo-option-12` | Only `formation_motion_combo` selects `Pendulum`; other selectors remain default. |
| `formation_motion_combo-option-13` | Only `formation_motion_combo` selects `Bounce`; other selectors remain default. |
| `motion_animations_check-on` | Only `motion_animations_check` is on; dependent visibility is measured. |
| `card-directing_unit-tallest-850` | Only `directing_unit` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-directing_unit-tallest-1160` | Only `directing_unit` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-ghost_diagnostics-tallest-850` | Only `ghost_diagnostics` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-ghost_diagnostics-tallest-1160` | Only `ghost_diagnostics` is put in its tallest local state at 1160 px; all other cards stay default. |
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

195 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

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
