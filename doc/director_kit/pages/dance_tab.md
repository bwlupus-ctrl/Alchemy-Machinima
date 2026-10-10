# Dance — Director Kit page contract

Page key: `dance_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (0/82 setting-bound rows; 79 C++-referenced rows).

## Real owners

- `panel_dance_ball` / `panel` at `indra/newview/skins/default/xui/en/panel_dance_ball.xml:12` (81 matching rows).
- `floater_director` / `floater` at `indra/newview/skins/default/xui/en/floater_director.xml:15` (1 matching rows).
- Referencing classes: `ALPanelDanceBall` (154), `LLFloaterReg` (1), `LLFloaterDirector` (1).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALDanceBallKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `dance_ball` / `db_on` (Power) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:94` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onToggleOn()` (`indra/newview/alpaneldanceball.cpp:334`) | none |
| `dance_ball` / `db_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:195` | kit readout ← approved static copy; no runtime writer | none |
| `dance_ball` / `db_header_desync` (Sync) | `slider` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:172` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onHeaderDesync()` (`indra/newview/alpaneldanceball.cpp:340`) | none |
| `dance_ball` / `dance_popout` (Open window) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:568` | kit-native action `floater:dance_ball`; no duplicate widget is required | none |
| `dancers` / `db_dancer_list` (Dancers) | `scroll_list` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:230` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::rebuildDancerList()` (`indra/newview/alpaneldanceball.cpp:867`) | none |
| `dancers` / `db_add_selected` (Add selected) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:247` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onAddSelected()` (`indra/newview/alpaneldanceball.cpp:343`) | none |
| `dancers` / `db_add_cast` (Add cast) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:257` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onAddCast()` (`indra/newview/alpaneldanceball.cpp:344`) | none |
| `dancers` / `db_add_ghosts` (Add ghosts) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:267` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onAddGhosts()` (`indra/newview/alpaneldanceball.cpp:345`) | none |
| `dancers` / `db_remove` (Remove) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:277` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onRemove()` (`indra/newview/alpaneldanceball.cpp:347`) | none |
| `dancers` / `db_exclude` (Exclude) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:287` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onExclude()` (`indra/newview/alpaneldanceball.cpp:348`) | none |
| `add_nearby` / `db_add_nearby` (Add nearby) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:297` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onAddNearby()` (`indra/newview/alpaneldanceball.cpp:346`) | none |
| `add_nearby` / `db_radius` (Radius (m)) | `spinner` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:312` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onAutoJoinChanged()` (`indra/newview/alpaneldanceball.cpp:350`) | none |
| `add_nearby` / `db_focus` (Around) | `combo_box` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:343` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onAutoJoinChanged()` (`indra/newview/alpaneldanceball.cpp:351`) | none |
| `add_nearby` / `db_autojoin` (Auto-join) | `check_box` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:357` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onAutoJoinChanged()` (`indra/newview/alpaneldanceball.cpp:352`) | none |
| `add_nearby` / `db_include_seated` (Include seated) | `check_box` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:367` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onIncludeSeated()` (`indra/newview/alpaneldanceball.cpp:353`) | none |
| `add_nearby` / `db_include_me` (Include me) | `check_box` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:377` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onIncludeMe()` (`indra/newview/alpaneldanceball.cpp:354`) | none |
| `add_nearby` / `db_clear_exclusions` (Clear exclude) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:398` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onClearExclusions()` (`indra/newview/alpaneldanceball.cpp:349`) | none |
| `playlist_edit` / `db_drop_hint` (Tip) | `text` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:419` | kit readout ← approved static copy; no runtime writer | none |
| `playlist_edit` / `db_playlist_list` (Animations) | `scroll_list` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:432` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onPlaylistSelection()` (`indra/newview/alpaneldanceball.cpp:365`) | none |
| `playlist_edit` / `db_from_explorer` (From Explorer) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:448` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onFromExplorer()` (`indra/newview/alpaneldanceball.cpp:357`) | none |
| `playlist_edit` / `db_paste_uuid` (Paste UUID) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:458` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onPasteUUID()` (`indra/newview/alpaneldanceball.cpp:358`) | none |
| `playlist_edit` / `db_pl_remove` (Remove) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:468` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onPlaylistRemove()` (`indra/newview/alpaneldanceball.cpp:359`) | none |
| `playlist_edit` / `db_pl_clear` (Clear) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:478` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onPlaylistClear()` (`indra/newview/alpaneldanceball.cpp:360`) | none |
| `playlist_edit` / `db_pl_up` (Up) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:488` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onPlaylistUp()` (`indra/newview/alpaneldanceball.cpp:361`) | none |
| `playlist_edit` / `db_pl_down` (Down) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:498` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onPlaylistDown()` (`indra/newview/alpaneldanceball.cpp:362`) | none |
| `playlist_edit` / `db_rename_edit` (Name) | `line_editor` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:509` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onPlaylistRename()` (`indra/newview/alpaneldanceball.cpp:364`) | none |
| `playlist_edit` / `db_pl_rename` (Rename) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:519` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onPlaylistRename()` (`indra/newview/alpaneldanceball.cpp:363`) | none |
| `play_controls` / `db_play_selected` (Play selected) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:529` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onPlaySelected()` (`indra/newview/alpaneldanceball.cpp:366`) | none |
| `play_controls` / `db_play_all` (Play all) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:539` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onPlayAll()` (`indra/newview/alpaneldanceball.cpp:367`) | none |
| `play_controls` / `db_mode` (Mode) | `combo_box` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:559` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onModeChanged()` (`indra/newview/alpaneldanceball.cpp:368`) | none |
| `play_controls` / `db_prev` (Previous) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:104` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onPrevious()` (`indra/newview/alpaneldanceball.cpp:335`) | none |
| `play_controls` / `db_next` (Next) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:126` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onNext()` (`indra/newview/alpaneldanceball.cpp:336`) | none |
| `play_controls` / `db_freeze` (Freeze) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:116` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onFreeze()` (`indra/newview/alpaneldanceball.cpp:337`) | none |
| `play_controls` / `db_shuffle` (Shuffle) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:137` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onShuffle()` (`indra/newview/alpaneldanceball.cpp:338`) | none |
| `play_controls` / `db_stop_all` (Stop all) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:147` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onStopAll()` (`indra/newview/alpaneldanceball.cpp:339`) | none |
| `auto_advance` / `db_adv_interval` (Every (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:588` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onAutoAdvanceChanged()` (`indra/newview/alpaneldanceball.cpp:370`) | none |
| `auto_advance` / `db_adv_jitter` (Jitter (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:614` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onAutoAdvanceChanged()` (`indra/newview/alpaneldanceball.cpp:371`) | none |
| `timing` / `db_t_speed` (Speed) | `slider` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:680` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onTimingChanged()` (`indra/newview/alpaneldanceball.cpp:375`) | none |
| `timing` / `db_t_desync` (Desync) | `slider` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:652` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onTimingChanged()` (`indra/newview/alpaneldanceball.cpp:374`) | none |
| `timing` / `db_reshuffle` (Reshuffle) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:664` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onReshuffle()` (`indra/newview/alpaneldanceball.cpp:380`) | none |
| `timing` / `db_t_crossfade` (Crossfade (s)) | `slider` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:716` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onTimingChanged()` (`indra/newview/alpaneldanceball.cpp:377`) | none |
| `timing` / `db_t_speed_jitter` (Speed jitter) | `slider` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:698` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onTimingChanged()` (`indra/newview/alpaneldanceball.cpp:376`) | none |
| `timing` / `db_t_priority` (Priority) | `combo_box` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:738` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onTimingChanged()` (`indra/newview/alpaneldanceball.cpp:378`) | none |
| `timing` / `db_t_dance_drives_face` (Drive face) | `check_box` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:753` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onTimingChanged()` (`indra/newview/alpaneldanceball.cpp:379`) | none |
| `local_blocking` / `db_blocking_list` (Dancers) | `scroll_list` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:777` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onBlockingSelection()` (`indra/newview/alpaneldanceball.cpp:392`) | none |
| `local_blocking` / `db_block_toggle_rows` (Toggle) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:794` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onBlockingToggleRows()` (`indra/newview/alpaneldanceball.cpp:393`) | none |
| `turn_pin` / `db_block_yaw_minus` (-15 deg) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:801` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onBlockingNudgeYaw()` (`indra/newview/alpaneldanceball.cpp:394`) | none |
| `turn_pin` / `db_block_yaw` (Yaw) | `slider` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:804` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onBlockingYaw()` (`indra/newview/alpaneldanceball.cpp:395`) | none |
| `turn_pin` / `db_block_yaw_plus` (+15 deg) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:808` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onBlockingNudgeYaw()` (`indra/newview/alpaneldanceball.cpp:396`) | none |
| `turn_pin` / `db_block_step` (Step) | `spinner` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:811` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onBlockingNudgeYaw()` (`indra/newview/alpaneldanceball.cpp:1491`) | none |
| `turn_pin` / `db_block_face` (Face) | `combo_box` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:822` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onBlockingFace()` (`indra/newview/alpaneldanceball.cpp:398`) | none |
| `turn_pin` / `db_block_pin` (Pin here) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:835` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onBlockingPin()` (`indra/newview/alpaneldanceball.cpp:399`) | none |
| `turn_pin` / `db_block_release` (Release) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:837` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onBlockingPin()` (`indra/newview/alpaneldanceball.cpp:400`) | none |
| `turn_pin` / `db_block_keep_facing` (Keep facing) | `check_box` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:831` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onBlockingNudgeYaw()` (`indra/newview/alpaneldanceball.cpp:1491`) | none |
| `nudge` / `db_block_f` (Forward) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:838` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onBlockingNudge()` (`indra/newview/alpaneldanceball.cpp:1559`) | none |
| `nudge` / `db_block_b` (Back) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:839` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onBlockingNudge()` (`indra/newview/alpaneldanceball.cpp:1559`) | none |
| `nudge` / `db_block_l` (Left) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:840` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onBlockingNudge()` (`indra/newview/alpaneldanceball.cpp:1559`) | none |
| `nudge` / `db_block_r` (Right) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:841` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onBlockingNudge()` (`indra/newview/alpaneldanceball.cpp:1559`) | none |
| `nudge` / `db_block_u` (Up) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:842` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onBlockingNudge()` (`indra/newview/alpaneldanceball.cpp:1559`) | none |
| `nudge` / `db_block_d` (Down) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:843` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onBlockingNudge()` (`indra/newview/alpaneldanceball.cpp:1559`) | none |
| `nudge` / `db_block_nudge_note` (Nudge) | `text` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:845` | kit readout ← approved static copy; no runtime writer | none |
| `nudge` / `db_block_mode` (Mode) | `radio_group` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:813` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onBlockingMode()` (`indra/newview/alpaneldanceball.cpp:397`) | none |
| `formation` / `db_block_shape` (Formation) | `combo_box` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:849` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onBlockingFormation()` (`indra/newview/alpaneldanceball.cpp:1579`) | none |
| `formation` / `db_block_spacing` (Size (m)) | `spinner` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:858` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onBlockingFormation()` (`indra/newview/alpaneldanceball.cpp:1579`) | none |
| `formation` / `db_block_centre` (Centre on) | `combo_box` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:860` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onBlockingFormation()` (`indra/newview/alpaneldanceball.cpp:1579`) | none |
| `formation` / `db_block_set_centre` (Set centre) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:866` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onBlockingSetCentre()` (`indra/newview/alpaneldanceball.cpp:406`) | none |
| `formation` / `db_block_clear_centre` (Clear centre) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:868` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onBlockingClearCentre()` (`indra/newview/alpaneldanceball.cpp:407`) | none |
| `formation` / `db_block_centre_status` (Centre) | `text` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:874` | kit readout ← approved static copy; no runtime writer | none |
| `apply_blocking` / `db_block_apply_selected` (Apply selected) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:870` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onBlockingFormation()` (`indra/newview/alpaneldanceball.cpp:408`) | none |
| `apply_blocking` / `db_block_apply_all` (Apply to all) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:872` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onBlockingFormation()` (`indra/newview/alpaneldanceball.cpp:409`) | none |
| `apply_blocking` / `db_block_reset_selected` (Reset selected) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:876` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onBlockingReset()` (`indra/newview/alpaneldanceball.cpp:410`) | none |
| `apply_blocking` / `db_block_reset_all` (Reset all) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:878` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onBlockingReset()` (`indra/newview/alpaneldanceball.cpp:411`) | none |
| `wave` / `db_wave_focus` (From) | `combo_box` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:929` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onWaveChanged()` (`indra/newview/alpaneldanceball.cpp:384`) | none |
| `wave` / `db_wave_speed` (Spread (m/s)) | `slider` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:949` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onWaveChanged()` (`indra/newview/alpaneldanceball.cpp:385`) | none |
| `wave` / `db_wave_dir` (Direction) | `combo_box` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:971` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onWaveChanged()` (`indra/newview/alpaneldanceball.cpp:386`) | none |
| `wave` / `db_wave_apply` (Apply on) | `combo_box` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:994` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onWaveChanged()` (`indra/newview/alpaneldanceball.cpp:387`) | none |
| `speed_ramp` / `db_ramp_target` (Target) | `spinner` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:1024` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onRampGo()` (`indra/newview/alpaneldanceball.cpp:1946`) | none |
| `speed_ramp` / `db_ramp_seconds` (Seconds) | `spinner` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:1039` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onRampGo()` (`indra/newview/alpaneldanceball.cpp:1946`) | none |
| `speed_ramp` / `db_ramp_go` (Go) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:1049` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onRampGo()` (`indra/newview/alpaneldanceball.cpp:388`) | none |
| `session` / `db_export` (Export...) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:1070` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onExport()` (`indra/newview/alpaneldanceball.cpp:389`) | none |
| `session` / `db_import` (Import...) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:1080` | `ALDanceBallKitAdapter` command/model binding → wired in `ALPanelDanceBall::postBuild()`; behavior `ALPanelDanceBall::onImport()` (`indra/newview/alpaneldanceball.cpp:390`) | none |
| `session` / `db_session_note` (Note) | `text` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:1090` | kit readout ← approved static copy; no runtime writer | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALPanelDanceBall::onAddCast()` — `indra/newview/alpaneldanceball.cpp:344` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onAddGhosts()` — `indra/newview/alpaneldanceball.cpp:345` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onAddNearby()` — `indra/newview/alpaneldanceball.cpp:346` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onAddSelected()` — `indra/newview/alpaneldanceball.cpp:343` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onAutoAdvanceChanged()` — `indra/newview/alpaneldanceball.cpp:370, 371` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onAutoJoinChanged()` — `indra/newview/alpaneldanceball.cpp:350, 351, 352` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onBlockingClearCentre()` — `indra/newview/alpaneldanceball.cpp:407` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onBlockingFace()` — `indra/newview/alpaneldanceball.cpp:398` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onBlockingFormation()` — `indra/newview/alpaneldanceball.cpp:408, 409, 1579` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onBlockingMode()` — `indra/newview/alpaneldanceball.cpp:397` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onBlockingNudge()` — `indra/newview/alpaneldanceball.cpp:1559` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onBlockingNudgeYaw()` — `indra/newview/alpaneldanceball.cpp:394, 396, 1491` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onBlockingPin()` — `indra/newview/alpaneldanceball.cpp:399, 400` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onBlockingReset()` — `indra/newview/alpaneldanceball.cpp:410, 411` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onBlockingSelection()` — `indra/newview/alpaneldanceball.cpp:392` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onBlockingSetCentre()` — `indra/newview/alpaneldanceball.cpp:406` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onBlockingToggleRows()` — `indra/newview/alpaneldanceball.cpp:393` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onBlockingYaw()` — `indra/newview/alpaneldanceball.cpp:395` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onClearExclusions()` — `indra/newview/alpaneldanceball.cpp:349` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onExclude()` — `indra/newview/alpaneldanceball.cpp:348` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onExport()` — `indra/newview/alpaneldanceball.cpp:389` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onFreeze()` — `indra/newview/alpaneldanceball.cpp:337` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onFromExplorer()` — `indra/newview/alpaneldanceball.cpp:357` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onHeaderDesync()` — `indra/newview/alpaneldanceball.cpp:340` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onImport()` — `indra/newview/alpaneldanceball.cpp:390` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onIncludeMe()` — `indra/newview/alpaneldanceball.cpp:354` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onIncludeSeated()` — `indra/newview/alpaneldanceball.cpp:353` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onModeChanged()` — `indra/newview/alpaneldanceball.cpp:368` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onNext()` — `indra/newview/alpaneldanceball.cpp:336` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onPasteUUID()` — `indra/newview/alpaneldanceball.cpp:358` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onPlayAll()` — `indra/newview/alpaneldanceball.cpp:367` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onPlaySelected()` — `indra/newview/alpaneldanceball.cpp:366` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onPlaylistClear()` — `indra/newview/alpaneldanceball.cpp:360` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onPlaylistDown()` — `indra/newview/alpaneldanceball.cpp:362` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onPlaylistRemove()` — `indra/newview/alpaneldanceball.cpp:359` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onPlaylistRename()` — `indra/newview/alpaneldanceball.cpp:363, 364` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onPlaylistSelection()` — `indra/newview/alpaneldanceball.cpp:365` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onPlaylistUp()` — `indra/newview/alpaneldanceball.cpp:361` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onPrevious()` — `indra/newview/alpaneldanceball.cpp:335` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onRampGo()` — `indra/newview/alpaneldanceball.cpp:388, 1946` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onRemove()` — `indra/newview/alpaneldanceball.cpp:347` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onReshuffle()` — `indra/newview/alpaneldanceball.cpp:380` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onShuffle()` — `indra/newview/alpaneldanceball.cpp:338` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onStopAll()` — `indra/newview/alpaneldanceball.cpp:339` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onTimingChanged()` — `indra/newview/alpaneldanceball.cpp:374, 375, 376, 377, 378, 379` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onToggleOn()` — `indra/newview/alpaneldanceball.cpp:334` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::onWaveChanged()` — `indra/newview/alpaneldanceball.cpp:384, 385, 386, 387` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDanceBall::rebuildDancerList()` — `indra/newview/alpaneldanceball.cpp:867` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterReg::showInstance()` — `indra/newview/llfloaterdirector.cpp:607` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **82 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-dance_ball-tallest-850` | Only `dance_ball` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-dance_ball-tallest-1160` | Only `dance_ball` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-dancers-tallest-850` | Only `dancers` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-dancers-tallest-1160` | Only `dancers` is put in its tallest local state at 1160 px; all other cards stay default. |
| `db_dancer_list-empty` | `db_dancer_list` is empty with no selection. |
| `db_dancer_list-one` | `db_dancer_list` has one selected item. |
| `db_dancer_list-overflow` | `db_dancer_list` has 51 items and a visible scrollbar. |
| `card-add_nearby-tallest-850` | Only `add_nearby` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-add_nearby-tallest-1160` | Only `add_nearby` is put in its tallest local state at 1160 px; all other cards stay default. |
| `db_focus-option-0` | Only `db_focus` selects `Me`; other selectors remain default. |
| `db_focus-option-1` | Only `db_focus` selects `Camera focus`; other selectors remain default. |
| `db_focus-option-2` | Only `db_focus` selects `Subject A`; other selectors remain default. |
| `db_autojoin-on` | Only `db_autojoin` is on; dependent visibility is measured. |
| `db_include_seated-on` | Only `db_include_seated` is on; dependent visibility is measured. |
| `db_include_me-on` | Only `db_include_me` is on; dependent visibility is measured. |
| `card-playlist_edit-tallest-850` | Only `playlist_edit` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-playlist_edit-tallest-1160` | Only `playlist_edit` is put in its tallest local state at 1160 px; all other cards stay default. |
| `db_playlist_list-empty` | `db_playlist_list` is empty with no selection. |
| `db_playlist_list-one` | `db_playlist_list` has one selected item. |
| `db_playlist_list-overflow` | `db_playlist_list` has 51 items and a visible scrollbar. |
| `card-play_controls-tallest-850` | Only `play_controls` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-play_controls-tallest-1160` | Only `play_controls` is put in its tallest local state at 1160 px; all other cards stay default. |
| `db_mode-option-0` | Only `db_mode` selects `Same for all dancers`; other selectors remain default. |
| `db_mode-option-1` | Only `db_mode` selects `Shuffle each dancer`; other selectors remain default. |
| `db_mode-option-2` | Only `db_mode` selects `Sequence each dancer`; other selectors remain default. |
| `card-auto_advance-tallest-850` | Only `auto_advance` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-auto_advance-tallest-1160` | Only `auto_advance` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-timing-tallest-850` | Only `timing` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-timing-tallest-1160` | Only `timing` is put in its tallest local state at 1160 px; all other cards stay default. |
| `db_t_priority-option-0` | Only `db_t_priority` selects `Asset (as authored)`; other selectors remain default. |
| `db_t_priority-option-1` | Only `db_t_priority` selects `4`; other selectors remain default. |
| `db_t_priority-option-2` | Only `db_t_priority` selects `5`; other selectors remain default. |
| `db_t_priority-option-3` | Only `db_t_priority` selects `6 (default)`; other selectors remain default. |
| `db_t_dance_drives_face-on` | Only `db_t_dance_drives_face` is on; dependent visibility is measured. |
| `card-local_blocking-tallest-850` | Only `local_blocking` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-local_blocking-tallest-1160` | Only `local_blocking` is put in its tallest local state at 1160 px; all other cards stay default. |
| `db_blocking_list-empty` | `db_blocking_list` is empty with no selection. |
| `db_blocking_list-one` | `db_blocking_list` has one selected item. |
| `db_blocking_list-overflow` | `db_blocking_list` has 51 items and a visible scrollbar. |
| `card-turn_pin-tallest-850` | Only `turn_pin` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-turn_pin-tallest-1160` | Only `turn_pin` is put in its tallest local state at 1160 px; all other cards stay default. |
| `db_block_face-option-0` | Only `db_block_face` selects `Camera`; other selectors remain default. |
| `db_block_face-option-1` | Only `db_block_face` selects `Subject A`; other selectors remain default. |
| `db_block_face-option-2` | Only `db_block_face` selects `Ball centre`; other selectors remain default. |
| `db_block_face-option-3` | Only `db_block_face` selects `Away from centre`; other selectors remain default. |
| `db_block_face-option-4` | Only `db_block_face` selects `Each other`; other selectors remain default. |
| `db_block_face-option-5` | Only `db_block_face` selects `Match first selected`; other selectors remain default. |
| `db_block_keep_facing-on` | Only `db_block_keep_facing` is on; dependent visibility is measured. |
| `card-nudge-tallest-850` | Only `nudge` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-nudge-tallest-1160` | Only `nudge` is put in its tallest local state at 1160 px; all other cards stay default. |
| `db_block_mode-option-0` | Only `db_block_mode` selects `OFFSET`; other selectors remain default. |
| `db_block_mode-option-1` | Only `db_block_mode` selects `LOCK`; other selectors remain default. |
| `card-formation-tallest-850` | Only `formation` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-formation-tallest-1160` | Only `formation` is put in its tallest local state at 1160 px; all other cards stay default. |
| `db_block_shape-option-0` | Only `db_block_shape` selects `Circle in`; other selectors remain default. |
| `db_block_shape-option-1` | Only `db_block_shape` selects `Circle out`; other selectors remain default. |
| `db_block_shape-option-2` | Only `db_block_shape` selects `Arc`; other selectors remain default. |
| `db_block_shape-option-3` | Only `db_block_shape` selects `Line`; other selectors remain default. |
| `db_block_shape-option-4` | Only `db_block_shape` selects `Grid`; other selectors remain default. |
| `db_block_centre-option-0` | Only `db_block_centre` selects `Me`; other selectors remain default. |
| `db_block_centre-option-1` | Only `db_block_centre` selects `Camera focus`; other selectors remain default. |
| `db_block_centre-option-2` | Only `db_block_centre` selects `Subject A`; other selectors remain default. |
| `card-apply_blocking-tallest-850` | Only `apply_blocking` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-apply_blocking-tallest-1160` | Only `apply_blocking` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-wave-tallest-850` | Only `wave` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-wave-tallest-1160` | Only `wave` is put in its tallest local state at 1160 px; all other cards stay default. |
| `db_wave_focus-option-0` | Only `db_wave_focus` selects `Me`; other selectors remain default. |
| `db_wave_focus-option-1` | Only `db_wave_focus` selects `Camera focus`; other selectors remain default. |
| `db_wave_focus-option-2` | Only `db_wave_focus` selects `Subject A`; other selectors remain default. |
| `db_wave_dir-option-0` | Only `db_wave_dir` selects `Outward`; other selectors remain default. |
| `db_wave_dir-option-1` | Only `db_wave_dir` selects `Inward`; other selectors remain default. |
| `db_wave_apply-option-0` | Only `db_wave_apply` selects `Start only`; other selectors remain default. |
| `db_wave_apply-option-1` | Only `db_wave_apply` selects `Switch only`; other selectors remain default. |
| `db_wave_apply-option-2` | Only `db_wave_apply` selects `Start and switch`; other selectors remain default. |
| `card-speed_ramp-tallest-850` | Only `speed_ramp` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-speed_ramp-tallest-1160` | Only `speed_ramp` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-session-tallest-850` | Only `session` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-session-tallest-1160` | Only `session` is put in its tallest local state at 1160 px; all other cards stay default. |
| `selection-deleted` | The selected model row is removed by another owner while visible. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## Ambiguous same-name candidates (never auto-bound)

None.

## Rejected out-of-scope same-name candidates

1 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

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
