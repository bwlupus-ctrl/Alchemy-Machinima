# Wiring audit: Cast > Dance kit conversion

Static audit of the Dance page after it became a generated kit page (10/15, legacy conversion batch 3). No build, no commit, no viewer run: every row is checked against the generated XML (`panel_dpage_cast_dance.xml`) and the C++ source. Sources: mockup page `dance_tab` (15 cards, 82 rows in the mockup; 84 now, see below), `kit_pages.json` entry `cast_group/dance_tab` (hosts the v1 nodes of `panel_dance_ball.xml`; page root class `panel_dance_ball`, root name `dance_tab_logic`).

## Result

- The page is no longer legacy: classification `kit (P0)`, pin 2 -> 1, `cast_group/dance_tab` removed from `KNOWN_LEGACY_PAGES` (`KNOWN_LEGACY_MAX` 2 -> 1; Ghosts follows in the same batch).
- 84 generated rows = the 82 mockup rows + 2 added (below), 15 cards, 4 sections (Setup, Playlist, Blocking, Waves). Three cards carry a card switch that is the v1 checkbox: `db_auto_advance` (Auto-advance), `db_blocking_on` (Blocking on), `db_wave_enable` (Wave). `ALKitSwitch` is an `LLCheckBoxCtrl`, so the `getChild<LLCheckBoxCtrl>` lookups resolve.
- The page root is `ALPanelDanceBall`, the same class the standalone Dance Ball floater embeds (`class="panel_dance_ball"` on the generated logic root). The panel owns all state (`ALDanceBall`, session-local; nothing is sent to the simulator), so every control is code-bound (`bind="code"`); the panel refreshes itself from revision gates plus a 2 Hz clock. The v1 `<string>` table (state, kind, message formats) is copied onto the generated root by the new `copy_strings` option of `director_kit_gen.py`, because the class reads its text with `getString`.
- The v1 Director tab (`floater_director.xml`) and the standalone floater (`floater_dance_ball.xml`, `panel_dance_ball.xml`) are untouched.
- Whole-page drag and drop of animations and folders (`ALPanelDanceBall::handleDragAndDrop`) is on the page root and works unchanged.

## Mockup rows added (the v1 panel had them, the mockup did not)

- `db_message` (Message, readout, Dance Ball card, after Status): the v1 message line at the foot of the panel; `setMessage()` writes every action result into it (added/removed/excluded dancers, playlist errors, import/export results).
- `db_local_note` (Note, readout, Blocking on card): the v1 "Local only: only you see this." note.
- Mockup row pin in `director_kit_gen.py`: 2107 -> 2109. Every Dance row also gained search keywords (`kw`).
- Card id `timing` renamed `dance_timing` in the mockup (generated card widget names must be unique; `kc_timing` collided with Look > Stylize).

## Renames and C++ changes

- Scroll-list columns are renamed through `child_names` so no column name repeats across the Director: dancer list `on/name/kind/now/dist` -> `dancer_on/dancer_name/dancer_kind/dancer_now/dancer_dist`; playlist `name/length/loop/status` -> `item_name/item_length/item_loop/item_status`; blocking list `on/name/yaw/mode/pin/status` -> `block_on/block_name/block_yaw/block_mode/block_pin/block_status`.
- `alpaneldanceball.cpp`: new helper `column_name(list, index, v1_name)` (:108) returns the name of column `index`; `rebuildDancerList` (:959-967), `rebuildPlaylistList` (:1049-1055) and `rebuildBlockingList` (:1463) address columns by position, so the same code fills the floater (v1 names) and the kit page (renamed). Cell updates were already positional.
- `alpaneldanceball.cpp`: `db_tabs` (the floater's tab container) is found by type with the new `find_tab_container` (:121, used at :249) instead of `getChild("db_tabs")`; the kit page has no tab container, so `installForTabs` is guarded by `if (mTabs)` (:455). The lazy page adoption (`ALScrollFit::adoptPage`) fits the kit page and enables slider wheels.
- `aluiperfselftest.cpp:3359-3360`: the functional probe that injects a "You" row into `db_dancer_list` takes the name column by position (`getColumn(1)->mName`) instead of the literal `"name"`. No fixture or expectation was loosened.
- `director_page_sources.py`: five `alpaneldanceball.cpp` binding line numbers refreshed (they were already stale).
- No change in `llfloaterdirector.cpp`: `dance_popout` is wired by the existing `bindCameraPage` (`root->findChild<LLButton>("dance_popout")`), which now finds the kit row's button. The legacy `director_dance_ball_panel` lookups return null on the kit page and are skipped.

## parity_allow additions (explicit names, no patterns)

All are static v1 labels or containers with a kit replacement: `db_tabs` (tab container; kit page is one card page), `db_sync_label` (Sync row label), `db_desync_label` (Desync row label), `db_priority_label` (Priority row label), `db_block_rotate_title` (Turn & pin card), `db_block_face_title` (Face row label), `db_block_place_title` (Turn & pin card, Pin here / Release), `db_block_formation_title` (Formation card title), `db_wave_title` (Wave card title), `db_wave_from_label` (From row label), `db_ramp_title` (Speed ramp card title), `db_session_title` (Save & load card title).

## Undo ownership (batch-2 review part 1)

Code-bound kit rows record widget history by default and replay `setValue` plus `onCommit` on whatever is selected at undo time. Rules applied here, one owner per control:

- Buttons and readouts are never recorded.
- Rows whose value belongs to the selected dancer or playlist row (`db_block_yaw`, `db_block_mode`, `db_block_face`, `db_block_keep_facing`, `db_rename_edit`) and the three lists (selection is not data) are `widget_history="false"` (`no_widget_history` in `kit_pages.json`). The page has no history of its own for them.
- Rows that write global Dance Ball state (timing, wave, auto-join, include flags, playlist mode, auto-advance, header sync, blocking switch) keep kit widget history: the value is not per-subject, so a replay is correct whatever is selected. Local UI-only values (`db_block_step`, `db_block_shape`, `db_block_spacing`, `db_block_centre`, `db_ramp_target`, `db_ramp_seconds`) also keep it; they are not subject data.

## Controls

All handlers are in `alpaneldanceball.cpp` (postBuild wiring :380-460). Combo data values are the v1 `value` attributes, hosted unchanged. Read-back is the gated `refresh*` function listed.

| Card | Widget | Type / range | Combo data values | Handler / target | Read-back | Undo owner | Status |
|---|---|---|---|---|---|---|---|
| Dance Ball | `db_on` | button | - | onToggleOn -> ALDanceBall::setEnabled | refreshHeader: setValueIfChanged(mOnBtn, ball.enabled()) | n/a (button, never recorded) | OK |
| Dance Ball | `db_status` | readout | - | none (output) | refreshHeader: setTextIfChanged(mStatusText) from roster size, mode, sync, label, frozen | n/a (readout) | OK |
| Dance Ball | `db_message` | readout | - | none (output) | setMessage() at event time (add/remove/exclude/rename/play/import/export results) | n/a (readout) | OK |
| Dance Ball | `db_header_desync` | slider 0..1 | - | onHeaderDesync -> timing.mDesync | refreshHeader + refreshTimingControls: setValueIfChanged(mHeaderDesync, timing.mDesync) | kit widget history (global Dance Ball state, no subject) | OK |
| Dance Ball | `dance_popout` | button | - | llfloaterdirector.cpp bindCameraPage: LLFloaterReg::showInstance("dance_ball") | none (opens the floater) | n/a (button, never recorded) | OK |
| Dancers | `db_dancer_list` | list (5 cols) | - | no commit handler (selection read by onRemove/onExclude/...) | rebuildDancerList (roster revision) + updateDancerCells (2 Hz); columns addressed by position | none (widget_history=false): list selection is not data | OK |
| Dancers | `db_add_selected` | button | - | onAddSelected -> addAvatarToBall | rebuildDancerList on roster revision; message with count | n/a (button, never recorded) | OK |
| Dancers | `db_add_cast` | button | - | onAddCast | roster revision; message | n/a (button, never recorded) | OK |
| Dancers | `db_add_ghosts` | button | - | onAddGhosts | roster revision; message | n/a (button, never recorded) | OK |
| Dancers | `db_remove` | button | - | onRemove (each selected row) | roster revision; message | n/a (button, never recorded) | OK |
| Dancers | `db_exclude` | button | - | onExclude (each selected row) | roster revision; message | n/a (button, never recorded) | OK |
| Add nearby | `db_add_nearby` | button | - | onAddNearby | roster revision; message | n/a (button, never recorded) | OK |
| Add nearby | `db_radius` | number 1..512 | - | onAutoJoinChanged(false) -> ball.setAutoJoin(radius) | refreshDancerControls: setValueIfChanged(mRadius, ball.autoJoinRadius()) | kit widget history (global Dance Ball state, no subject) | OK |
| Add nearby | `db_focus` | combo | Me=0; Camera focus=1; Subject A=2 | onAutoJoinChanged(true) -> ball.setAutoJoin(focus) | refreshDancerControls: showStoredValue(mFocus, autoJoinFocus); subject combo merged on the actor-choice revision | kit widget history (global Dance Ball state, no subject) | OK |
| Add nearby | `db_autojoin` | toggle | - | onAutoJoinChanged(false) -> ball.setAutoJoin(on) | refreshDancerControls: setValueIfChanged(mAutoJoin, ball.autoJoin()) | kit widget history (global Dance Ball state, no subject) | OK |
| Add nearby | `db_include_seated` | toggle | - | onIncludeSeated -> setIncludeSeated | refreshDancerControls: setValueIfChanged(mIncludeSeated, ball.includeSeated()) | kit widget history (global Dance Ball state, no subject) | OK |
| Add nearby | `db_include_me` | toggle | - | onIncludeMe -> setIncludeSelf | refreshDancerControls: setValueIfChanged(mIncludeMe, ball.includeSelf()) | kit widget history (global Dance Ball state, no subject) | OK |
| Add nearby | `db_clear_exclusions` | button | - | onClearExclusions | message | n/a (button, never recorded) | OK |
| Playlist | `db_drop_hint` | readout | - | none (static text) | XML text | n/a (readout) | OK |
| Playlist | `db_playlist_list` | list (4 cols) | - | onPlaylistSelection (fills the rename box) | rebuildPlaylistList (playlist revision) + updatePlaylistCells; columns by position | none (widget_history=false): list selection is not data | OK |
| Playlist | `db_from_explorer` | button | - | onFromExplorer -> addAnimationAsset | playlist revision; message | n/a (button, never recorded) | OK |
| Playlist | `db_paste_uuid` | button | - | onPasteUUID -> addAnimationAsset per UUID | playlist revision; message | n/a (button, never recorded) | OK |
| Playlist | `db_pl_remove` | button | - | onPlaylistRemove | playlist revision | n/a (button, never recorded) | OK |
| Playlist | `db_pl_clear` | button | - | onPlaylistClear | playlist revision | n/a (button, never recorded) | OK |
| Playlist | `db_pl_up` | button | - | onPlaylistUp | playlist revision | n/a (button, never recorded) | OK |
| Playlist | `db_pl_down` | button | - | onPlaylistDown | playlist revision | n/a (button, never recorded) | OK |
| Playlist | `db_rename_edit` | text | - | commit -> onPlaylistRename (renames the selected playlist row) | onPlaylistSelection fills it with the selected row name | none (widget_history=false): text of the selected playlist row; a replay would rename whichever row is selected now | OK |
| Playlist | `db_pl_rename` | button | - | onPlaylistRename | playlist revision | n/a (button, never recorded) | OK |
| Play | `db_play_selected` | button | - | onPlaySelected -> playRowOnDancers | status revision; message | n/a (button, never recorded) | OK |
| Play | `db_play_all` | button | - | onPlayAll -> mode SAME_FOR_ALL + play | refreshPlaylistControls (mode) | n/a (button, never recorded) | OK |
| Play | `db_mode` | combo | Same for all dancers=0; Shuffle each dancer=1; Sequence each dancer=2 | onModeChanged -> timing.mMode | refreshPlaylistControls: setValueIfChanged(mMode, "%d" of timing.mMode) | kit widget history (global Dance Ball state, no subject) | OK |
| Play | `db_prev` | button | - | onPrevious | status revision | n/a (button, never recorded) | OK |
| Play | `db_next` | button | - | onNext | status revision | n/a (button, never recorded) | OK |
| Play | `db_freeze` | button | - | onFreeze -> setFrozen | refreshHeader: setValueIfChanged(mFreezeBtn, ball.frozen()) | n/a (button, never recorded) | OK |
| Play | `db_shuffle` | button | - | onShuffle -> mode SHUFFLE_EACH_DANCER | refreshHeader: setValueIfChanged(mShuffleBtn, mode == SHUFFLE) | n/a (button, never recorded) | OK |
| Play | `db_stop_all` | button | - | onStopAll -> setEnabled(false) | refreshHeader | n/a (button, never recorded) | OK |
| Auto-advance | `db_adv_interval` | number 0.5..600 | - | onAutoAdvanceChanged -> timing.mAdvanceSeconds | refreshPlaylistControls: setValueIfChanged(mAdvInterval, ...) | kit widget history (global Dance Ball state, no subject) | OK |
| Auto-advance | `db_adv_jitter` | number 0..300 | - | onAutoAdvanceChanged -> timing.mAdvanceJitterSeconds | refreshPlaylistControls: setValueIfChanged(mAdvJitter, ...) | kit widget history (global Dance Ball state, no subject) | OK |
| Speed & sync | `db_t_speed` | slider 0.25..4 | - | onTimingChanged(true) -> timing.mSpeed | refreshTimingControls + refreshLiveSpeed (shows the live value during a ramp) | kit widget history (global Dance Ball state, no subject) | OK |
| Speed & sync | `db_t_desync` | slider 0..1 | - | onTimingChanged(false) -> timing.mDesync | refreshTimingControls: setValueIfChanged(mTDesync, timing.mDesync) | kit widget history (global Dance Ball state, no subject) | OK |
| Speed & sync | `db_reshuffle` | button | - | onReshuffle | none (new random phases) | n/a (button, never recorded) | OK |
| Speed & sync | `db_t_crossfade` | slider 0..5 | - | onTimingChanged(false) -> timing.mCrossfadeSeconds | refreshTimingControls | kit widget history (global Dance Ball state, no subject) | OK |
| Speed & sync | `db_t_speed_jitter` | slider 0..1 | - | onTimingChanged(false) -> timing.mSpeedJitter | refreshTimingControls | kit widget history (global Dance Ball state, no subject) | OK |
| Speed & sync | `db_t_priority` | combo | Asset (as authored)=-1; 4=4; 5=5; 6 (default)=6 | onTimingChanged(false) -> timing.mPriority | refreshTimingControls: setValueIfChanged(mTPriority, "%d" of priority) | kit widget history (global Dance Ball state, no subject) | OK |
| Speed & sync | `db_t_dance_drives_face` | toggle | - | onTimingChanged(false) -> timing.mDanceDrivesFace | refreshTimingControls | kit widget history (global Dance Ball state, no subject) | OK |
| Blocking on | `db_blocking_list` | list (6 cols) | - | onBlockingSelection -> refreshBlockingControls | rebuildBlockingList (roster/staging revision) + updateBlockingCells (2 Hz); columns by position | none (widget_history=false): list selection is not data | OK |
| Blocking on | `db_local_note` | readout | - | none (static text) | XML text | n/a (readout) | OK |
| Blocking on | `db_block_toggle_rows` | button | - | onBlockingToggleRows -> setDancerBlocking per selected row | updateBlockingCells | n/a (button, never recorded) | OK |
| Turn & pin | `db_block_yaw_minus` | button | - | onBlockingNudgeYaw(-1); step = db_block_step | updateBlockingCells | n/a (button, never recorded) | OK |
| Turn & pin | `db_block_yaw` | slider -180..180 | - | onBlockingYaw -> setBlockingYaw for every selected dancer | refreshBlockingControls: shows the first selected dancer yaw | none (widget_history=false): per-selected-dancer value; replaying onto a later selection would write the wrong dancer | OK |
| Turn & pin | `db_block_yaw_plus` | button | - | onBlockingNudgeYaw(+1); step = db_block_step | updateBlockingCells | n/a (button, never recorded) | OK |
| Turn & pin | `db_block_step` | number 1..15 | - | read at click time by onBlockingNudgeYaw (no commit handler) | none (local step size, not stored) | kit widget history (local UI value, not per-subject) | OK |
| Turn & pin | `db_block_face` | combo | Camera=0; Subject A=1; Ball centre=2; Away from centre=3; Each other=4; Match first selected=5 | onBlockingFace -> face target of every selected dancer | applied, not read back per dancer | none (widget_history=false): per-selected-dancer value; replaying onto a later selection would write the wrong dancer | OK |
| Turn & pin | `db_block_pin` | button | - | onBlockingPin(true) | updateBlockingCells (Pin and Status columns) | n/a (button, never recorded) | OK |
| Turn & pin | `db_block_release` | button | - | onBlockingPin(false) | updateBlockingCells | n/a (button, never recorded) | OK |
| Turn & pin | `db_block_keep_facing` | toggle | - | onBlockingFace -> keep-facing flag of every selected dancer | applied, not read back per dancer | none (widget_history=false): per-selected-dancer value; replaying onto a later selection would write the wrong dancer | OK |
| Nudge | `db_block_f` | button | - | onBlockingNudge(+0.25 x) | none (moves the dancers) | n/a (button, never recorded) | OK |
| Nudge | `db_block_b` | button | - | onBlockingNudge(-0.25 x) | none | n/a (button, never recorded) | OK |
| Nudge | `db_block_l` | button | - | onBlockingNudge(+0.25 y) | none | n/a (button, never recorded) | OK |
| Nudge | `db_block_r` | button | - | onBlockingNudge(-0.25 y) | none | n/a (button, never recorded) | OK |
| Nudge | `db_block_u` | button | - | onBlockingNudge(+0.25 z) | none | n/a (button, never recorded) | OK |
| Nudge | `db_block_d` | button | - | onBlockingNudge(-0.25 z) | none | n/a (button, never recorded) | OK |
| Nudge | `db_block_nudge_note` | readout | - | none (static text) | XML text | n/a (readout) | OK |
| Nudge | `db_block_mode` | custom | OFFSET=0; LOCK=1 | onBlockingMode -> setBlockingMode (OFFSET index 0 / LOCK) for every selected dancer | refreshBlockingControls: setValueIfChanged(mBlockMode, mode of the first selected dancer) | none (widget_history=false): per-selected-dancer value; replaying onto a later selection would write the wrong dancer | OK |
| Formation | `db_block_shape` | combo | Circle in=0; Circle out=1; Arc=2; Line=3; Grid=4 | read by onBlockingFormation (no commit handler) | none (local choice) | kit widget history (local UI value, not per-subject) | OK |
| Formation | `db_block_spacing` | number 0.25..32 | - | read by onBlockingFormation (no commit handler) | none (local size) | kit widget history (local UI value, not per-subject) | OK |
| Formation | `db_block_centre` | combo | Me=0; Camera focus=1; Subject A=2 | read by onBlockingSetCentre (no commit handler) | refreshBlockingControls: showStoredValue(mBlockCentre, blockingCentreFocus) | kit widget history (local UI value, not per-subject) | OK |
| Formation | `db_block_set_centre` | button | - | onBlockingSetCentre | refreshBlockingControls centre status | n/a (button, never recorded) | OK |
| Formation | `db_block_clear_centre` | button | - | onBlockingClearCentre | refreshBlockingControls centre status | n/a (button, never recorded) | OK |
| Formation | `db_block_centre_status` | readout | - | none (output) | refreshBlockingControls: setValueIfChanged(mBlockCentreStatus) | n/a (readout) | OK |
| Apply blocking | `db_block_apply_selected` | button | - | onBlockingFormation(false) | updateBlockingCells | n/a (button, never recorded) | OK |
| Apply blocking | `db_block_apply_all` | button | - | onBlockingFormation(true) | updateBlockingCells | n/a (button, never recorded) | OK |
| Apply blocking | `db_block_reset_selected` | button | - | onBlockingReset(false) | updateBlockingCells | n/a (button, never recorded) | OK |
| Apply blocking | `db_block_reset_all` | button | - | onBlockingReset(true) | updateBlockingCells | n/a (button, never recorded) | OK |
| Wave | `db_wave_focus` | combo | Me=0; Camera focus=1; Subject A=2 | onWaveChanged(true) -> wave.mFocus | refreshCinemaControls: showStoredValue(mWaveFocus, wave.mFocus) | kit widget history (global Dance Ball state, no subject) | OK |
| Wave | `db_wave_speed` | slider 0.5..100 | - | onWaveChanged(false) -> wave.mSpreadMetersPerSecond | refreshCinemaControls | kit widget history (global Dance Ball state, no subject) | OK |
| Wave | `db_wave_dir` | combo | Outward=0; Inward=1 | onWaveChanged(false) -> wave.mDirection | refreshCinemaControls: "%d" of direction | kit widget history (global Dance Ball state, no subject) | OK |
| Wave | `db_wave_apply` | combo | Start only=0; Switch only=1; Start and switch=2 | onWaveChanged(false) -> wave.mApply | refreshCinemaControls: "%d" of apply | kit widget history (global Dance Ball state, no subject) | OK |
| Speed ramp | `db_ramp_target` | number 0.25..4 | - | read by onRampGo (no commit handler) | none (local value) | kit widget history (local UI value, not per-subject) | OK |
| Speed ramp | `db_ramp_seconds` | number 0..120 | - | read by onRampGo (no commit handler) | none (local value) | kit widget history (local UI value, not per-subject) | OK |
| Speed ramp | `db_ramp_go` | button | - | onRampGo -> ALDanceBall::startSpeedRamp | refreshLiveSpeed | n/a (button, never recorded) | OK |
| Save & load | `db_export` | button | - | onExport -> file picker, exportSession | message | n/a (button, never recorded) | OK |
| Save & load | `db_import` | button | - | onImport -> file picker, importSession | all gates reset; message | n/a (button, never recorded) | OK |
| Save & load | `db_session_note` | readout | - | none (static text) | XML text | n/a (readout) | OK |
| Auto-advance (card switch) | `db_auto_advance` | switch | on/off | onAutoAdvanceChanged -> timing.mAutoAdvance | refreshPlaylistControls: setValueIfChanged(mAutoAdvance, timing.mAutoAdvance) | kit widget history (global Dance Ball state, no subject) | OK |
| Blocking on (card switch) | `db_blocking_on` | switch | on/off | onBlockingEnabled -> setBlockingEnabled | refreshBlockingControls: setValueIfChanged(mBlockingOn, ball.blockingEnabled()) | kit widget history (global Dance Ball state, no subject) | OK |
| Wave (card switch) | `db_wave_enable` | switch | on/off | onWaveChanged(false) -> wave.mEnabled | refreshCinemaControls: setValueIfChanged(mWaveEnable, wave.mEnabled) | kit widget history (global Dance Ball state, no subject) | OK |

## Notes

- `db_header_desync` (header Sync) and `db_t_desync` (Timing Desync) are two sliders over the same `timing.mDesync`; each is refreshed from the model, so editing one updates the other on the next gate.
- `db_on`, `db_freeze`, `db_shuffle` are v1 toggle buttons; the button kind is hosted unchanged (`is_toggle`), and their pressed state is read back from the model every refresh.
- Every row above is OK; none needed a fix beyond the renames and optional lookups listed.
