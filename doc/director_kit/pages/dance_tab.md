# Dance — Director Kit page contract

Page key: `dance_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (0/82 setting-bound rows; 79 C++-referenced rows).

## Real owners

- `panel_dance_ball` / `panel` at `indra/newview/skins/default/xui/en/panel_dance_ball.xml:12` (81 matching rows).
- `floater_director` / `floater` at `indra/newview/skins/default/xui/en/floater_director.xml:15` (1 matching rows).
- Referencing classes: `ALPanelDanceBall` (78), `LLFloaterDirector` (1).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALDanceBallKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `dance_ball` / `db_on` (Power) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:94` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:204`) | none |
| `dance_ball` / `db_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:195` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:211`) | none |
| `dance_ball` / `db_header_desync` (Sync) | `slider` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:172` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:210`) | none |
| `dance_ball` / `dance_popout` (Open window) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:568` | `ALDanceBallKitAdapter` command/model binding → `LLFloaterDirector::postBuild()` (`indra/newview/llfloaterdirector.cpp:586`) | none |
| `dancers` / `db_dancer_list` (Dancers) | `scroll_list` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:230` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:215`) | none |
| `dancers` / `db_add_selected` (Add selected) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:247` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:216`) | none |
| `dancers` / `db_add_cast` (Add cast) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:257` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:217`) | none |
| `dancers` / `db_add_ghosts` (Add ghosts) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:267` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:218`) | none |
| `dancers` / `db_remove` (Remove) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:277` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:219`) | none |
| `dancers` / `db_exclude` (Exclude) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:287` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:220`) | none |
| `add_nearby` / `db_add_nearby` (Add nearby) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:297` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:221`) | none |
| `add_nearby` / `db_radius` (Radius (m)) | `spinner` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:312` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:223`) | none |
| `add_nearby` / `db_focus` (Around) | `combo_box` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:343` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:224`) | none |
| `add_nearby` / `db_autojoin` (Auto-join) | `check_box` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:357` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:225`) | none |
| `add_nearby` / `db_include_seated` (Include seated) | `check_box` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:367` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:226`) | none |
| `add_nearby` / `db_include_me` (Include me) | `check_box` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:377` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:227`) | none |
| `add_nearby` / `db_clear_exclusions` (Clear exclude) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:398` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:222`) | none |
| `playlist_edit` / `db_drop_hint` (Tip) | `text` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:419` | kit readout ← approved static copy; no runtime writer | none |
| `playlist_edit` / `db_playlist_list` (Animations) | `scroll_list` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:432` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:229`) | none |
| `playlist_edit` / `db_from_explorer` (From Explorer) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:448` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:230`) | none |
| `playlist_edit` / `db_paste_uuid` (Paste UUID) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:458` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:231`) | none |
| `playlist_edit` / `db_pl_remove` (Remove) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:468` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:232`) | none |
| `playlist_edit` / `db_pl_clear` (Clear) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:478` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:233`) | none |
| `playlist_edit` / `db_pl_up` (Up) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:488` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:234`) | none |
| `playlist_edit` / `db_pl_down` (Down) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:498` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:235`) | none |
| `playlist_edit` / `db_rename_edit` (Name) | `line_editor` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:509` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:237`) | none |
| `playlist_edit` / `db_pl_rename` (Rename) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:519` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:236`) | none |
| `play_controls` / `db_play_selected` (Play selected) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:529` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:238`) | none |
| `play_controls` / `db_play_all` (Play all) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:539` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:239`) | none |
| `play_controls` / `db_mode` (Mode) | `combo_box` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:559` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:240`) | none |
| `play_controls` / `db_prev` (Previous) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:104` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:205`) | none |
| `play_controls` / `db_next` (Next) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:126` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:206`) | none |
| `play_controls` / `db_freeze` (Freeze) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:116` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:207`) | none |
| `play_controls` / `db_shuffle` (Shuffle) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:137` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:208`) | none |
| `play_controls` / `db_stop_all` (Stop all) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:147` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:209`) | none |
| `auto_advance` / `db_adv_interval` (Every (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:588` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:242`) | none |
| `auto_advance` / `db_adv_jitter` (Jitter (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:614` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:243`) | none |
| `timing` / `db_t_speed` (Speed) | `slider` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:680` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:247`) | none |
| `timing` / `db_t_desync` (Desync) | `slider` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:652` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:245`) | none |
| `timing` / `db_reshuffle` (Reshuffle) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:664` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:246`) | none |
| `timing` / `db_t_crossfade` (Crossfade (s)) | `slider` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:716` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:249`) | none |
| `timing` / `db_t_speed_jitter` (Speed jitter) | `slider` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:698` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:248`) | none |
| `timing` / `db_t_priority` (Priority) | `combo_box` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:738` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:250`) | none |
| `timing` / `db_t_dance_drives_face` (Drive face) | `check_box` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:753` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:251`) | none |
| `local_blocking` / `db_blocking_list` (Dancers) | `scroll_list` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:777` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:264`) | none |
| `local_blocking` / `db_block_toggle_rows` (Toggle) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:794` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:265`) | none |
| `turn_pin` / `db_block_yaw_minus` (-15 deg) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:801` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:266`) | none |
| `turn_pin` / `db_block_yaw` (Yaw) | `slider` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:804` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:267`) | none |
| `turn_pin` / `db_block_yaw_plus` (+15 deg) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:808` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:268`) | none |
| `turn_pin` / `db_block_step` (Step) | `spinner` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:811` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:269`) | none |
| `turn_pin` / `db_block_face` (Face) | `combo_box` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:822` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:271`) | none |
| `turn_pin` / `db_block_pin` (Pin here) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:835` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:273`) | none |
| `turn_pin` / `db_block_release` (Release) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:837` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:274`) | none |
| `turn_pin` / `db_block_keep_facing` (Keep facing) | `check_box` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:831` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:272`) | none |
| `nudge` / `db_block_f` (Forward) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:838` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:275`) | none |
| `nudge` / `db_block_b` (Back) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:839` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:275`) | none |
| `nudge` / `db_block_l` (Left) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:840` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:275`) | none |
| `nudge` / `db_block_r` (Right) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:841` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:276`) | none |
| `nudge` / `db_block_u` (Up) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:842` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:276`) | none |
| `nudge` / `db_block_d` (Down) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:843` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:276`) | none |
| `nudge` / `db_block_nudge_note` (Nudge) | `text` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:845` | kit readout ← approved static copy; no runtime writer | none |
| `nudge` / `db_block_mode` (Mode) | `radio_group` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:813` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:270`) | none |
| `formation` / `db_block_shape` (Formation) | `combo_box` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:849` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:279`) | none |
| `formation` / `db_block_spacing` (Size (m)) | `spinner` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:858` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:280`) | none |
| `formation` / `db_block_centre` (Centre on) | `combo_box` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:860` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:281`) | none |
| `formation` / `db_block_set_centre` (Set centre) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:866` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:282`) | none |
| `formation` / `db_block_clear_centre` (Clear centre) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:868` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:283`) | none |
| `formation` / `db_block_centre_status` (Centre) | `text` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:874` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:284`) | none |
| `apply_blocking` / `db_block_apply_selected` (Apply selected) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:870` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:285`) | none |
| `apply_blocking` / `db_block_apply_all` (Apply to all) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:872` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:286`) | none |
| `apply_blocking` / `db_block_reset_selected` (Reset selected) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:876` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:287`) | none |
| `apply_blocking` / `db_block_reset_all` (Reset all) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:878` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:288`) | none |
| `wave` / `db_wave_focus` (From) | `combo_box` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:929` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:254`) | none |
| `wave` / `db_wave_speed` (Spread (m/s)) | `slider` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:949` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:255`) | none |
| `wave` / `db_wave_dir` (Direction) | `combo_box` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:971` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:256`) | none |
| `wave` / `db_wave_apply` (Apply on) | `combo_box` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:994` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:257`) | none |
| `speed_ramp` / `db_ramp_target` (Target) | `spinner` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:1024` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:258`) | none |
| `speed_ramp` / `db_ramp_seconds` (Seconds) | `spinner` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:1039` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:259`) | none |
| `speed_ramp` / `db_ramp_go` (Go) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:1049` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:260`) | none |
| `session` / `db_export` (Export...) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:1070` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:261`) | none |
| `session` / `db_import` (Import...) | `button` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:1080` | `ALDanceBallKitAdapter` command/model binding → `ALPanelDanceBall::postBuild()` (`indra/newview/alpaneldanceball.cpp:262`) | none |
| `session` / `db_session_note` (Note) | `text` `indra/newview/skins/default/xui/en/panel_dance_ball.xml:1090` | kit readout ← approved static copy; no runtime writer | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALPanelDanceBall::postBuild()` — `indra/newview/alpaneldanceball.cpp:204, 205, 206, 207, 208, 209, 210, 211, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224, 225, 226, 227, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 242, 243, 245, 246, 247, 248, 249, 250, 251, 254, 255, 256, 257, 258, 259, 260, 261, 262, 264, 265, 266, 267, 268, 269, 270, 271, 272, 273, 274, 275, 276, 279, 280, 281, 282, 283, 284, 285, 286, 287, 288` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::postBuild()` — `indra/newview/llfloaterdirector.cpp:586` | Move model-facing work into `ALDanceBallKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

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

## New widget required

None. Every approved visual kind is either a kit primitive or an existing XUI control. Missing action/model bindings below are USER DECISIONS, not permission to invent a visual class.

## Unresolved and USER DECISION items

- `db_drop_hint` (Tip): no C++ reference or setting binding.
- `db_block_nudge_note` (Nudge): no C++ reference or setting binding.
- `db_session_note` (Note): no C++ reference or setting binding.

## Risks

- A widget-name collision can select an unrelated XUI root or C++ literal; ownership must be resolved by root, class, and behavior together.
- Extracting model work without rewiring the existing owner creates divergent v1/v2 behavior; the extraction and forwarding call are one reviewed change.
- Duplicate writable settings must obey the rollout homes table; this page emits a link/read-only row when it is not the chosen home.
- Shared edit models require owner tokens, and external presets/cue playback require one global history barrier at start/stop rather than per write.
- Unresolved rows fail generation closed; they are never silently replaced with a guessed widget or command.
