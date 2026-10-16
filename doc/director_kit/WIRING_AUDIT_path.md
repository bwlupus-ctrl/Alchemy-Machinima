# Wiring audit: Cast > Path kit conversion

Static audit of the Path page after it became a generated kit page (10/15, legacy conversion batch 2). No build, no commit, no viewer run: every row is checked against the generated XML, `settings.xml` and the C++ source. Sources: mockup page `path_tab` (15 cards, 62 rows, plus the two rows added below), `kit_pages.json` entry `cast_group/path_tab` (hosts the v1 nodes of `panel_path_editor.xml`, root class `panel_path_editor`), generated `panel_dpage_cast_path.xml`.

## Result

- The page is no longer legacy: classification `kit (P0)`, pin 4 -> 3, `cast_group/path_tab` removed from `KNOWN_LEGACY_PAGES`, and its `xui_legacy_v1_findings` baseline entry removed from `gate_policy.json` (the list shrinks).
- 64 generated rows = the 62 mockup rows + 2 added (below), 15 cards, 5 sections, Who chip "Path of" (actor from the Cast selection, `actor_target_synthetic`). The Edit-in-world, Pose ghosts, Node camera and Camera sync cards carry the card switches `edit_mode_check`, `onion_skin_check` (`PathShowOnionSkin`), `path_camera_enable_check` (`PathCameraEnabled`) and `sync_take_check`; they are `ALKitSwitch` (an `LLCheckBoxCtrl`), so the C++ lookups resolve.
- Mockup rows added (the v1 panel had them, the mockup did not): `path_readout` ("Summary", the length and duration line, Waypoints card) and `suspend_status` ("Status", Paused walk card). `director_kit_gen.py` row pin 2103 -> 2105.
- Mockup flags changed: the three Paused walk buttons and `node_cam_status` lost "Advanced" (v1 showed them; hiding the resume/cancel controls of a paused walk behind Advanced would strand the user). The Paused walk card is shown only while a walk is suspended (below).
- Search keywords (`kw`) added to every Path row (the mockup had none).
- The page root is `ALPanelPathEditor` itself (shared with the standalone Actor Mover floater, whose XML is untouched). Cast selection still reaches it through `LLFloaterDirector::refreshPathTab` -> `setTargetActor`; the edit-tool ownership (`claimEdit`, focus hooks, `onVisibilityChange`) is unchanged.
- Dial and arc rows (Plane yaw, pitch, roll) stay numeric boxes: the generator only upgrades a spinner to a compass dial when no C++ names the widget, and `ALPanelPathEditor` looks them up as `LLSpinCtrl`.

## C++ changes

| where | change |
|---|---|
| `alpanelpatheditor.cpp` `postBuild` | `path_sections` (accordion) and `path_header` are looked up with `findChild` and null-guarded (kit page has neither; the Who chip names the actor). `suspend_banner` likewise; `kc_suspended` (the card) is looked up and hidden with `ALKitCard::setCodeVisible(false)`. The accordion layout callbacks are skipped without an accordion. |
| `alpanelpatheditor.cpp` `bind_reset` | `findChild` for the v1 icon reset buttons; skipped when absent (kit rows draw the reset glyph). |
| `alpanelpatheditor.cpp` `registerKitResets` (new) | On the kit page, registers an `ALKitBinding` (`PathResetBinding`) per code-bound row through `ALKitBindingRegistry`: value read/apply through the widget's own commit, default for the amber state and glyph, and reset routed to `onResetControl(key)` (the same function the v1 reset icons call, same defaults). |
| `alpanelpatheditor.cpp` `refreshHeader`, `refreshSuspendBanner` | null-guard `mHeader`; show/hide `mSuspendBanner` (v1) or the Paused walk card (kit). The placement hint is hidden under the banner only on v1. |
| `llfloaterdirector.cpp` `bindPathPage` | `dynamic_cast<ALPanelPathEditor*>(root)` first, else the v1 `path_editor` child. |

## Controls

All handlers are in `alpanelpatheditor.cpp`. Ranges equal the hosted v1 nodes (checked by script: no min/max/step/decimals difference between `panel_path_editor.xml` and the generated page).

| # | widget | handler / setting | notes | status | Undo owner |
|---|---|---|---|---|---|
| 1 | edit_mode_check (card switch) | `onToggleEditMode` (arms `ALToolPathEdit`) | enabled by `refreshHeader` when an actor is targeted | OK | none: card enable switches are not recorded by kit widget history |
| 2 | hint | static text | hint stays visible (no banner overlap on the kit page) | OK | n/a (action/output; no widget step) |
| 3 | waypoint_list | `onListSelect` -> `setEditNode`; rows by `refreshList` (columns `num`, `role`, `detail`) | | OK | none (widget_history=false; selected subject or selection) |
| 4 | path_readout | `refreshReadout` text | new row | OK | n/a (action/output; no widget step) |
| 5-8 | btn_wp_add, btn_wp_insert, btn_wp_delete, btn_wp_clear | `onClickAdd`, `onClickInsert`, `onClickDelete`, `onClickClear` | | OK | n/a (action/output; no widget step) |
| 9-13 | btn_edit_undo, btn_edit_redo, btn_edit_reverse, btn_edit_mirror, btn_edit_loopclose | `undoPath`, `redoPath`, reverse, mirror, close loop (disabled while walking) | | OK | latest step on this page across its owners; n/a (action/output; no widget step) |
| 14-16 | btn_play_path, btn_stop_path, btn_walk_here | `onClickPlayPath`, `onClickStopPath`, `onClickWalkHere` | | OK | n/a (action/output; no widget step) |
| 17-18 | copy_to_combo, btn_copy_to | `onClickCopyTo` copies to the combo's member | combo filled by code | OK | Path snapshots (this page instance; no kit widget step); n/a (action/output; no widget step) |
| 19 | onion_skin_check (card switch) | setting `PathShowOnionSkin` | Boolean, default per `settings.xml`; kit setting binding and reset | OK | none: card enable switches are not recorded by kit widget history |
| 20-23 | ghost_style_combo, ghost_distort_combo, ghost_distort_amount, ghost_effect_fps | settings `PathGhostStyle`, `PathGhostDistort`, `PathGhostDistortAmount` (0..1 step 0.05), `PathGhostEffectFps` (0..30) | combo values 0..27 and 0..8 equal v1 | OK | kit setting history (this page; guarded against newer edits) |
| 24-28 | node_height_spinner (-5..5), node_dwell_spinner (0..120), node_speed_spinner (0..10), node_corner_radius_spinner (0..10000), node_anim_editor (36 chars) | `onNodeHeightCommit`, `onNodeDwellCommit`, `onNodeSpeedCommit`, `onNodeCornerRadiusCommit`, `onNodeAnimCommit` (undo snapshot, then the selected node) | reset default 0 (anim: empty) via `PathResetBinding` | OK | Path snapshots (this page instance; no kit widget step) |
| 29 | path_camera_enable_check (card switch) | setting `PathCameraEnabled` | | OK | none: card enable switches are not recorded by kit widget history |
| 30-33 | btn_node_setcam, btn_node_clearcam, node_cam_transition_combo, btn_node_preview | `onClickSetCam`, `onClickClearCam`, `onCamTransitionCommit` (Cut 0 / Ease 1, reset Ease), `onClickPreview` | | OK | n/a (action/output; no widget step); Path snapshots (this page instance; no kit widget step) |
| 34 | node_cam_status | status text from `refreshCameraControls` | now basic | OK | n/a (action/output; no widget step) |
| 35-39 | path_speed_spinner, path_cadence_slider, path_easein_spinner, path_easeout_spinner, path_tension_slider | `onPathSpeedCommit`, `onPathCadenceCommit`, `onPathEaseInCommit`, `onPathEaseOutCommit`, `onPathTensionCommit` | reset defaults 1, 3, 0, 0, 0.5 | OK | Path snapshots (this page instance; no kit widget step) |
| 40 | color_swatch (row "Color", Advanced) | per-actor world color painted by `refreshPathControls` on the hosted `panel` | mockup row `color_lbl` renamed to the C++ name through `row_overrides` | OK | Path snapshots (this page instance; no kit widget step) |
| 41-44 | path_groundfollow_check, path_pitch_check, path_airborne_check, path_end_combo | `onPathGroundFollowCommit`, `onPathPitchCommit`, `onPathAirborneCommit`, `onPathEndCommit` (Stop 0, Stop + face target 1, Loop 2, Ping-pong 3) | reset default false / Stop | OK | Path snapshots (this page instance; no kit widget step) |
| 45-49 | path_shape_combo, btn_primitive_center, btn_fit_half_arc, btn_fit_sine, endpoint_generator_lbl | `onPathShapeCommit` (Waypoints 0 ... Sine Wave 6), `onClickCenterPrimitive`, `onClickFitHalfArc`, `onClickFitSine`, static help | reset default Waypoints | OK | Path snapshots (this page instance; no kit widget step); n/a (action/output; no widget step) |
| 50-57 | primitive_radius_x/y, primitive_start_deg, primitive_sweep_deg, primitive_rise, primitive_yaw_deg, primitive_pitch_deg, primitive_roll_deg | `onPrimitiveCommit` | reset defaults 3, 2, 0, 360, 3, 0, 0, 0 | OK | Path snapshots (this page instance; no kit widget step) |
| 58-60 | sync_take_check (card switch), sync_offset_spinner (-60..60), sync_status | `onSyncToggle`, `onSyncOffsetCommit`, `refreshChoreography` text | switch off is its own reset | OK | Path snapshots (this page instance; no kit widget step); n/a (action/output; no widget step) |
| 61-64 | follow_leader_combo, follow_offset_spinner (0..64), btn_stop_following, follow_status | `onFollowCommit`, `onFollowOffsetCommit` (reset 3), `onStopFollow`, status text | | OK | Path snapshots (this page instance; no kit widget step); n/a (action/output; no widget step) |
| 65-68 | suspend_status, btn_suspend_resume, btn_suspend_reanchor, btn_suspend_cancel | `refreshSuspendBanner` text and enable; `onClickResume`, `onClickReanchor`, `onClickCancelSuspend` | card shown only while `isWalkSuspended`; `suspend_status` is the new row | OK | n/a (action/output; no widget step) |

(Rows 1, 19, 29 and 58 are card switches, not mockup rows.)

Resets that changed shape: the v1 `reset_follow_leader` (clear following) has no kit glyph because the leader combo has no comparable default; **Stop following** does the same. The `sync_take` reset is the Camera sync card switch itself.

## Checks run

`director_kit_gen.py` check and selftest, `director_page_sources.py --check`, `kit_lint.py --all`, `xui_layout_check.py` selftest and `--changed HEAD`, `director_functional_manifest.py --check`, `director_parity.py` selftest and `--groups light_group`, `zs_check.ps1`, `prebuild_guard.ps1 -ChecksOnly`. Not run (rules): build, viewer, in-world self-test. Worth an in-world look: the Edit in world switch arming the tool, the kit reset glyphs (they call `onResetControl`), the Paused walk card appearing and disappearing, and the color swatch.

`xui_layout_check.py --selftest` change: its "lazy-page adoptPage wiring" fixture used `panel_dpage_cast_path.xml` as the probe (a legacy page with a `scroll_container`). No Director page has a scroll_container any more, so the probe is now the fixture `scripts/code_tools/xui_fixtures/lazy_scroll_probe.xml` (the same shape, registered as a lazily hosted page); the assertion is unchanged.
