# Triage: run_functional_20261015_165507 (build 26.2.0.63476, commit 96f3e92f713)

Read-only triage. Sources: `functional.csv` (2793 rows), `manifest_merged.json`, previous runs
`run_functional_20261014_074020` and `run_functional_20261013_170834`, and the code named in each row.
Verdicts: REAL = a Director bug a user can hit. HARNESS = stale manifest, wrong oracle, or the
login-screen test cannot put the control in a testable state. "Linked" = the failing row is only a symptom
of a REAL bug found elsewhere.

## 0. Bottom line

* 191 non-rule FAIL: 187 HARNESS, 4 symptoms of REAL bugs (two root causes below). 37 KNOWN_GAP: 0 proven
  REAL (all "no independent state oracle"; the handlers exist in code).
* Rule engine R1-R6 is new in this run (the 10/14 CSV has 0 rule rows), so every rule failure is new.
  R2 24 FAIL: 23 REAL (one root cause) + 1 minor. R3 43 FAIL: 43 REAL but cosmetic (px over listed in section 7).
  R4 3 FAIL: 3 REAL copy edits. R6 137 FAIL: 135 HARNESS, ~2 linked to REAL bugs.
* Two REAL functional bugs found:
  1. **Reset page does nothing on every page wrapped in a `*_tab_logic` panel** (23 pages): the reset popover is
     never created because `ALKitPage::mShell` is null at `postBuild`. Explains all 23 R2 `reset_popover/open`
     failures plus `actors_tab target_list` and `sld_diffuse` (multi). Same null `mShell` also disables the
     page-bar Changed-only toggle and Undo/Redo enablement on those pages (by code reading, not run-confirmed).
  2. **Actor FX (actor_style_tab) never refreshes**: `refreshActorStyleTab` early-outs when the target combo is not
     visible, but the kit hides that combo permanently. Explains `actor_style_status` empty (FAIL since 10/14).
* Biggest HARNESS levers: (a) the manifest has no `visible_when`/`enable_when` for any of 3962 entries, so every
  control hidden or disabled by login-state logic is a FAIL instead of a DEFER (vcam alone = 85 + 26 rows);
  (b) native radio buttons of kit "segmented" rows are hidden, the harness clicks them (9 rows, 0 of 11 radio rows ever pass);
  (c) text editors are no longer sent Return (3 rows); (d) regenerated manifest emits segment probes for a
  50-option selector that is a combo (50 rows).

## 1. "reveal did not settle within 6 frames" (92 FAIL) - all HARNESS, identical set to 10/14

The 6-frame budget is not the problem: every row was still invisible after 6 attempts because the control is hidden
or disabled by page state, which `settleFunctionalReveal` (aluiperfselftest.cpp:6330-6349) reports as FAIL unless the
manifest entry carries `visible_when`/DEFER_INWORLD. `manifest_merged.json` has 0 entries with `visible_when`.
`revealFunctionalControl` (aluiperfselftest.cpp:4732-4745) can only drive `LLComboBox` selectors, never radios,
card chips or camera/face/projector selection.

| Page | Rows | Verdict | Root cause (file:line) | Fix |
|---|---|---|---|---|
| vcam_tab, card `projector` | 26 (`vc_proj_look` ... `vc_proj_keep_gobo`, `vc_proj_resolution`) | HARNESS | Card hidden unless a projector is selected: `alpaneldirectorvcam.cpp:540` `setCodeVisible(selected_projector != nullptr)`; no projector exists at login | Manifest: emit `visible_when: "projector selected"` (DEFER_INWORLD) for cards with `show_if: selected_projector` in `vcam_page.json` |
| vcam_tab, card `projector_tv` | 20 (`vc_proj_tv_*`) | HARNESS | same, `alpaneldirectorvcam.cpp:540` | same |
| vcam_tab, card `tv_effects` | 23 (`vc_tv_*`) | HARNESS | Card hidden unless a face (display) exists: `alpaneldirectorvcam.cpp:898` `setCodeVisible(have_display)` | `visible_when: "face selected"` |
| vcam_tab, card `face_mapping` | 10 (`vc_face_feed`, `vc_face_fit`, `vc_anchor_x/y`, `vc_screen_height/aspect`, `vc_remove_face`, `vc_bar_r/g/b`) | HARNESS | same, `:898`; anchor/screen rows also row-hidden at `:748-752` | same |
| vcam_tab, card `placement` | 3 (`vc_eye_x/y/z`) | HARNESS | Row visible only for an object camera: `alpaneldirectorvcam.cpp:661-663` `setRowControlVisible(.., object_camera)`; no capture at login (cards hidden at `:889`) | `visible_when: "object camera selected"` |
| vcam_tab, card `optics` | 2 (`vc_fov_mode`, `vc_fov_deg`) | HARNESS | `:670-671` row hidden when skeleton / no capture | `visible_when: "camera selected"` |
| vcam_tab, card `output_frame` | 1 (`vc_output_custom`) | HARNESS | `:679` visible only after Custom aspect | reveal selector `vc_output_aspect`=Custom |
| gaze_tab | 3 (`gaze_cast_combo`, `btn_gaze_clear_object`, `gaze_composition_mix_slider`) | HARNESS | Whole legacy gaze host is disabled with no actor (`alpanellensgaze.cpp:1170-1181`); dependents are shown only for target mode Cast/Object (`:1265-1274`) or composition blend (`:1377`). The reveal selector write is overwritten by refresh | DEFER_INWORLD with `enable_when: actor selected` |
| ghosts_tab | 1 (`animation_library_combo`) | HARNESS | Visible only when drive mode = Directed (`alpanelghoststudio.cpp:1454-1456`); `drive_mode_combo` is disabled without an entity clone (`:1451`) | DEFER_INWORLD, `enable_when: entity clone selected` |
| light_rim_tab | 1 (`cine_rimq_actors_alpha`) | HARNESS (stale manifest) | Row is authored `hidden="true"` and `enabled="false"` ("Reserved for a later version"): `panel_dpage_light_rim.xml:64-65` | Generator: SKIP rows with `hidden="true"` / child `enabled="false"` (see section 9 item 4) |
| actor_style_tab | 1 (`actor_style_target`) | HARNESS here, but exposes REAL bug R2 (section 2) | Who-picker combo is deliberately hidden by the kit: `alkitpage.cpp:708-710` | Drive who pickers through the who chip/popover, or SKIP `who_picker` rows |
| stepped_tab | 1 (`sa_actor_target`) | HARNESS | same `alkitpage.cpp:708-710` | same |

NEW since 10/14: none (the set of 92 is identical).

## 2. REAL bugs

| # | Page(s) | Control | Verdict | Root cause | Fix |
|---|---|---|---|---|---|
| R1 | 23 kit pages (shot, moves, operator, frame_orbit, takes, focus, filters, diopter, cine_light_rig, light_rim, actors, image, bloom_glow, stylize, tron, stepped, actor_style, weather, atmosphere, shadows_probes, temporal, scene_options, director_settings) | page-bar **Reset page** button | **REAL** (high confidence: two independent signals) | `ALKitPage::postBuild` does `mShell = ALKitShell::find(this)` (`alkitpage.cpp:340`) and builds the reset popover only `if (mShell)` (`:453-492`). Every one of these pages has a `<panel name="*_tab_logic">` wrapper around `<kit_page>`, and `LLPanel::initPanelXML` attaches a panel to its parent only after its children are built and post-built (`indra/llui/llpanel.cpp:554-561`), so the wrapper has no parent yet and `find` returns null. `resetPage()` (`alkitpage.cpp:1557-1561`) then does nothing. Signals: R2 click lands on `*_reset_page` (handler_exact true) but no popover is visible; the actors fixture cannot find `actors_reset_confirm` (`aluiperfselftest.cpp:2912`). Pages whose root is `kit_page` (legacy pages) are unaffected but their Reset is disabled by design | Resolve the shell lazily: replace the cached `mShell` with a helper that calls `ALKitShell::find(this)` at use time and create the reset popover from `ALKitShell::pageBuilt` (`alkitshell.cpp:1454`, which already wires the page search box for the same reason). Same helper fixes Changed-only (`alkitpage.cpp:601-606`), Undo/Redo enablement (`:1742-1752`) and `performPageReset` history gesture (`:1565-1566`), all gated on `mShell` (probable, not run-verified) |
| R2 | actor_style_tab | whole page refresh (`actor_style_status` readout empty, target roster never rebuilt) | **REAL** (FAIL since 10/14) | `LLFloaterDirector::refreshActorStyleTab` returns early `if (!mActorStyleTarget->isInVisibleChain())` (`llfloaterdirector.cpp:5982`). The kit now hides that combo for good (`who_picker="actor_style_target"` in `panel_dpage_look_actor_style.xml:4`, hidden at `alkitpage.cpp:710`), so the page body never refreshes | Gate on the page root or `mActorStyleEnabled->isInVisibleChain()` instead of the hidden who-combo |
| R3 | director (cast popover) | `kit_cast_popover/content` | REAL-minor | When the roster is docked in the rail (`alkitshell.cpp:722`) the Cast chip still opens `kit_cast_popover` (`alkitshell.cpp:495-510`) with no children: a blank 300x360 box | Hide the cast chip while the roster is docked, or toggle the rail roster from it |
| R4 | director | `keyboard_tab_focus` (FAIL in 10/13, 10/14, 10/15) | UNRESOLVED (low) | Harness calls `director->handleKeyHere(KEY_TAB)` directly (`aluiperfselftest.cpp:3763`); `LLPanel::handleKeyHere` (`llpanel.cpp:294-305`) needs a focus root and a next tab stop. Could not tell from code whether kit switches/buttons form a tab chain | Needs a runtime probe: log `focusNextItem` result and the next candidate |

Derived failures of R1: `actors_tab target_list` (category A), `actors_tab sld_diffuse` multi-select row
(`page_reset_kept_store_and_preference:false` because `confirm` is null), R6 rows for both, and 23 R2 rows.

## 3. "control not found in director/shot_tab" (50 FAIL, NEW, all HARNESS)

| Page | Controls | Verdict | Root cause | Fix |
|---|---|---|---|---|
| shot_tab | `kc_move_settings:<option>` x50 (Bone Lock ... Pedro Cam, `CinematicCamMode` 1..51) | HARNESS (stale probe / regenerated manifest) | `panel_dpage_camera_shot.xml:8` card `kc_move_settings` is `kind="selector"` with 50 options. `ALKitCard` builds segment buttons `kc_<id>_seg_<i>` only for <=6 options and a combo `kc_<id>_selector` otherwise (`alkitcard.cpp:484-510`). The manifest generator still emits a `selector_segment` click per option (`director_functional_manifest.py:1089-1099`) and the harness looks up `<card>_seg_<test_value>` (`aluiperfselftest.cpp:2530-2536`), using the enum value instead of the option index (also wrong for any <=6 card whose values are not 0..n-1) | Generator: for >6 options emit one `select_item` row on `kc_<id>_selector`; harness: map by option index. The 50 rows did not exist in the 10/14 manifest |

## 4. "category A: commit fired but the external oracle did not move" (15 FAIL + 1 KNOWN_GAP)

`observable` for a control with no bound setting is "page-visible state hash changed" and "named handler ran"
(`aluiperfselftest.cpp:6886-6896`). Category A therefore means the handler ran but nothing observable moved.

| Page | Control | Verdict | Setting / handler | Root cause | Fix | New vs 10/14 |
|---|---|---|---|---|---|---|
| ghosts_tab | `brigade_columns_spinner`, `brigade_rows_spinner`, `brigade_file_spacing_spinner`, `brigade_rank_spacing_spinner` | HARNESS | none; `ALPanelGhostStudio::postBuild` -> `updateOwnedCrowdPlacement()` (`alpanelghoststudio.cpp:577-587`) | Handler acts only when a crowd placement draft is active (`:3439`, `:3597`); nothing at login | DEFER_INWORLD with `enable_when: crowd draft active` | same |
| ghosts_tab | `array_parameter_spinner` | HARNESS | same (`alpanelghoststudio.cpp:551`) | same; flaky (PASS in 10/14) | same | NEW |
| ghosts_tab | `formation_motion_combo` | HARNESS | `ALPanelGhostStudio::postBuild` | Motion preset applies only with a placed formation | same | same |
| dance_tab | `db_pl_rename`, `db_block_clear_centre`, `db_block_reset_all`; `db_radius` (KNOWN_GAP) | HARNESS | `ALPanelDanceBall::postBuild` (`alpaneldanceball.cpp:351,364,409,413`) | No playlist / dancers / centre exist at login, click changes no visible state | DEFER_INWORLD with world fixture | same |
| exposure_tab | `exp_sky_lighting_btn` | HARNESS | `Floater.ShowOrBringToFront env_adjust_snapshot` (`panel_exposure_hub.xml:65-66`) | It is a floater launcher but the manifest row has no `launcher` probe, and `expect` carries a stale "UNWIRED" flag with `known_gap:false`; the floater-opened check only runs for `probe: launcher` (`aluiperfselftest.cpp:7551`) | Mark as launcher with `launcher_floater: env_adjust_snapshot` | same |
| actors_tab | `target_list` | **Linked to R1** | `ALPanelAvatarLightResponse::postBuild` | `fixture_count==2` rows also run the Reset-page confirm (`aluiperfselftest.cpp:2905-2915`); `actors_reset_confirm` does not exist | fixed by R1 | NEW (was SKIP) |
| actors_tab | `sld_diffuse` (multi-select row, `source: synthetic:light_response_multi`) | **Linked to R1** | same | `undo_kept_store:true` but `page_reset_kept_store_and_preference:false` | fixed by R1 | NEW |
| actors_tab | `combo_sil_fill` | HARNESS | store field `sil_fill` expected [1,1,1] | Harness picked item index 1 (Charcoal, 0.149 grey = correct write) while the oracle expects item value 2 (White): index/value mix-up in the manifest (`test_value: 2`); handler works | Resolve `test_value` as item index or pick by value | NEW (oracle got stricter) |
| actor_style_tab | `actor_style_distortion_amount` | HARNESS | `bindActorStylePage`, per-actor style | No actor at login (`hasActorStyleTarget()` false), nothing stored | DEFER_INWORLD, `enable_when: actor selected` | same |
| stepped_tab | `sa_actor_rate` | HARNESS | `bindSteppedPage`, per-actor override | no actor; owner state unchanged | same | NEW |

## 5. "category C: click handled but no commit" (3 FAIL, all NEW, HARNESS)

| Page | Control | Verdict | Root cause | Fix |
|---|---|---|---|---|
| shot_tab | `switcher_slot_label` | HARNESS | Text rows are typed but Return is deliberately not delivered any more (`aluiperfselftest.cpp:5719-5731`, comment explains the safety change); line edits commit on Return/focus-loss, so `commits:0` (`keyboard.return_delivered_to_widget:false`) | Deliver `KEY_RETURN` to the focused `LLLineEditor` via `functionalDeliverKey` for text rows, or `setFocus(false)` to commit |
| vcam_tab | `vc_camera_name` | HARNESS | same (PASS in 10/14) | same |
| cine_light_rig_tab | `cine_seed` | HARNESS | same (PASS in 10/14); widget text was applied | same |

These are the only 3 text rows in the manifest; all regressed with the 10/15 input-safety commits (45593daead8, be260e4d826).

## 6. Other FAIL rows

### 6a. "observed value/state did not match manifest" (3, same as 10/14, HARNESS)

| Page | Control | Verdict | Root cause | Fix |
|---|---|---|---|---|
| cine_light_rig_tab | `cine_night_mask_darkness`, `cine_easy_night_mask_darkness` | HARNESS (oracle wrong) | Widget is percent, setting is the inverse floor: `onNightMaskDarknessCommit` writes `1 - percent/100` (`alpanelcinelightrig.cpp:1010-1022`). Test wrote 1% -> setting 0.99 (observed 0.99 is correct); manifest expects 1.0 | Manifest: transform expectation (`setting = 1 - value/100`), these rows are `bind="code"` |
| cine_light_rig_tab | `cine_easy_flare_preset` | HARNESS (oracle off by one) | Item index 2 writes `RenderCineLensFlarePreset = 2` (`alpanelcinelightrig.cpp:1983`, combo values are 1-based after the sentinel); manifest `test_value {item_index:2, resolve:runtime}` expects 1 | Resolve expected value at runtime from the selected item |

### 6b. "readout text was empty after page refresh" (8)

| Page | Control | Verdict | Root cause | Fix |
|---|---|---|---|---|
| vcam_tab | `vc_status` | HARNESS | Row hidden when empty: `alpaneldirectorvcam.cpp:843,855` | skip readouts whose row is code-hidden |
| vcam_tab | `vc_lens_aperture` | HARNESS | Surface-lens only (`:667`) | same |
| vcam_tab | `vc_bone_status`, `vc_face_shape` | HARNESS | Text set only inside `if (capture)` / face branches (`:726`, `:744`) | `visible_when` capture/face |
| actors_tab | `txt_sil_note`, `txt_budget` (NEW) | HARNESS | Created hidden and shown only when non-empty (`alpanelavatarlightresponse.cpp:122-123,654-658,876-877`) | skip hidden-by-design readouts |
| light_rim_tab | `cine_rimq_status` | HARNESS | Updated by a clocked, on-screen-gated refresh (`alrigrimcontroller.cpp:400`), not on page refresh | wait for the status clock or drop the readout oracle |
| actor_style_tab | `actor_style_status` | **Linked to REAL R2** | page refresh never runs (`llfloaterdirector.cpp:5982`) | fix R2 |

### 6c. Not reachable (16): `disabled-by-parent` 9, `target hidden or disabled` 4, other 3

| Page | Control | Verdict | Root cause | Fix | New |
|---|---|---|---|---|---|
| temporal_tab | `output_fps_spinner` | HARNESS (stale manifest) | Authored `unconnected="true"` + `enabled="false"` ("Not connected yet"): `panel_dpage_capture_temporal.xml:21-22` | Generator must SKIP `unconnected`/`enabled="false"` rows (`director_functional_manifest.py` generated-kit path ignores both; the old `DECLARED_NON_INTERACTIVE` table at line 150 only applies to the CSV path) | NEW |
| temporal_tab | `drive_camera_check` | HARNESS (same) | `enabled="false"`, "Reserved for a later version": `panel_dpage_capture_temporal.xml:70-71` | same | NEW (was SKIP) |
| bloom_glow_tab | `set_RenderBloomHDR` | HARNESS (same) | `unconnected="true"`, `enabled="false"`: `panel_dpage_look_bloom_glow.xml:9-10` | same | NEW |
| cine_light_rig_tab | `cine_clear_object` | HARNESS | Enabled only with a target object (`alpanelcinelightrig.cpp:658`) | DEFER_INWORLD | same |
| stepped_tab | `sa_group_pick`, `sa_group_offset` | HARNESS | Disabled with "No cast groups" (new group-rate rows, commit 4a2afb70e7c) | DEFER_INWORLD | NEW |
| director_settings_tab | `settings_crash_backups`, `settings_restore_backup`, `settings_delete_backup` | HARNESS | Disabled until crash backups exist (`llfloaterdirector.cpp:1975`) | seed a backup fixture or DEFER | NEW |
| stepped_tab | `sa_mode`, `sa_rate_chips`, `sa_what` | HARNESS (harness cannot click kit segmented rows) | `ALKitRow` hides the native radio items and paints the segments (`alkitrow.cpp:377-396`) and handles the click itself (`:1062-1078`); `functionalClickTarget` returns the hidden checkbox (`aluiperfselftest.cpp:4225-4253`). No radio row has ever passed (11 radio rows: 4 FAIL not-reachable, 5 vcam reveal FAIL, 2 SKIP) | Click the group rect at `left + (index+0.5)/count * width` | same |
| vcam_tab | `vc_source_type` | HARNESS (same) | same | same | same |
| stepped_tab | `sa_fast_rate`, `sa_adapt_hysteresis`, `sa_background_rate` | HARNESS | Mode-dependent rows (`refreshConditionalRows`, `alpanelsteppedanim.cpp:346-347`) shown only for Adaptive/Mixed; the reveal cannot set the `sa_mode` radio, so the rows are collapsed (observed rect for `sa_fast_rate` equals `sa_rate`'s, hit-test lands on `sa_adapt_hysteresis_reset` / the card panel / outside the floater). 10/14 had a 1880x1058 screen and passed; this run is 1463x772 | Reveal via radio selector; make the harness reveal support radios | NEW |

### 6d. Alerts/refusals (3 FAIL, HARNESS)

| Page | Control | Notification | Verdict | Fix |
|---|---|---|---|---|
| shot_tab | `btn_reset_all` | `CinematicCamConfirmResetAll` | HARNESS | Click opened the expected confirm alert (`state_changed:true`, `external_delta:true`); manifest has no `expect_notification` and the harness treats any alert as refusal. Same in 10/14 |
| cine_light_rig_tab | `cine_reset_all` | `GenericAlertYesCancel` | HARNESS | same |
| actors_tab | `la_btn_reset_all` | `AvatarLightResponseResetAll` | HARNESS | same (NEW row) |

## 7. Rule engine failures (all NEW: the 10/14 run has no rule rows)

### R2_POPOVERS (24 FAIL)

| Group | Count | Verdict | Cause | Fix |
|---|---|---|---|---|
| `reset_popover/open` on 23 pages (list in section 2, R1) | 23 | **REAL** | mShell null, popover never built | see R1 |
| `kit_cast_popover/content` | 1 | REAL-minor | empty popover when roster docked | see R3 |

### R3_LABEL_FIT (43 FAIL; need vs have in px)

| Count | Label | Over | Verdict | Cause | Fix |
|---|---|---|---|---|---|
| 37 | `☆` favorite button on hovered rows (shot 4, operator 2, takes 1, vcam 2, focus 1, filters 1, light_rim 2, actors 2, image 4, stylize 4, tron 5, actor_style 1, weather 2, atmosphere 3, shadows_probes 1, temporal 1, director_settings 1) | needs 13 has 10 (3 px) | REAL (cosmetic) | 18 px button with default 4+4 px pads, `alkitrow.cpp:418-423` (layout `:623`). Only hovered rows are visible when scanned, so the true count is every row with a star | `favorite.setLeftHPad(1); setRightHPad(1)` or use an icon overlay; one edit fixes all |
| 1 | `AB` (cast list column header, `shell/btn_ab`) | 17 vs 14 (3 px) | REAL | column `ab` width 22: `floater_director.xml:221` | widen to >=26 |
| 1 | `Open fog volumes in Lightbox` (atmosphere_tab) | 158 vs 156 (2 px) | REAL | `panel_dpage_world_atmosphere.xml:229` | shorten to "Open in Lightbox" |
| 1 | `Reset 'don't ask again' prompts` (director_settings_tab) | 168 vs 156 (12 px) | REAL | button label from `director_mockup_data.json` `settings_prompts.btn` | shorten ("Reset prompts") |
| 1 | `Star a row or use its right-click menu...` (quick_tab) | 331 vs 328 (3 px) | REAL | `alkitcard.cpp:278` text in a 328 px box, not elided | shorten text or enable ellipsis |
| 2 | `Follows the cast member you click...` who-hint: cine_light_rig_tab 394 vs 388 (6 px), actor_style_tab 394 vs 355 (39 px) | up to 39 px | REAL | hint box width = page width - chip - 24 (`alkitpage.cpp:1184`), text from `:679`, no ellipsis | shorten to "Follows the Cast selection; pick one to lock." and enable ellipses |

### R4_RAW_NAMES (3 FAIL, REAL copy edits)

| Page | Control | Leaked name | Source | Fix |
|---|---|---|---|---|
| gaze_tab | `gaze_limit_mode_combo` tooltip | `DirectorGazeCustomLimitsEnabled` | `panel_lens_gaze.xml:555` | reword to "the global custom-limits switch below" |
| exposure_tab | `exp_hdr_bloom` tooltip | `RenderBloomHDR` | `panel_exposure_hub.xml:70` | reword (also stale: HDR bloom is marked "Not connected yet" on the Bloom page) |
| vcam_tab | help for `vc_lens_zoom` | `PrismLensZoom` | `vcam_page.json:72` (and generated `panel_dpage_camera_vcam.xml:236`) | drop "(PrismLensZoom)" |

### R6_CONNECTED (137 FAIL)

| Group | Count | Verdict | Cause | Fix |
|---|---|---|---|---|
| Derived: "functional row did not pass in this run" | 117 (vcam 87, stepped 11, cine 6, others) | 115 HARNESS + 2 linked to R1/R2 (`actors_tab target_list`, `actor_style_target`) | follows the row failures above: 88 reveal, 9 disabled, 4 target hidden, 4 cat A, 3 cat C, 3 mismatch, 3 alerts, 3 not reachable | fix at row level |
| "row proves only `ui`" | 12: `cine_{bg,rim,fill,key}_{hero,cone_width,shaft}` | HARNESS (rule bug) | rows have `consumer: selected_rig_light_*` but `dirEffectClass` (`aluiperfselftest.cpp:8827-8844`) ignores `consumer` and reads only `expect`, which is `{"ui": ...}` | classify rows with a `consumer` as `consumer` |
| "row proves only `none`" | 8: `takes_tab btn_record`, `stylize_tab ri_reset_all`, `tron_tab tr_trail_clear_btn / tr_grid_origin_clear_btn / tr_reset_all`, `stepped_tab sa_override_clear`, `weather_tab weather_reset_all`, `atmosphere_tab ch_reset_all` | HARNESS (oracle spec) | the functional rows PASS (handler ran, state changed) but the manifest has no setting/consumer/reset_defaults expectation, so the rule credits nothing | add `reset_defaults` / state expectations to these 8 rows |

## 8. KNOWN_GAP (37)

All "known gap remains marked UNWIRED" (36) mean the control was exercised, committed and its owner state
changed, i.e. it works as far as the harness can see. The flag is set by `functional_report.py:2023` for any
`RUN` row with no setting/consumer ("no independent state oracle"), not by a real unwired audit. Handlers verified
in code for every vcam name (`alpaneldirectorvcam.cpp:1198-1243`).

| Page | Rows | Verdict | Fix |
|---|---|---|---|
| vcam_tab | 26: `vc_bone_{enabled,anchor,aim,roll,smooth,x,y,z,pitch,yaw,scale}`, `vc_fov_attached`, `vc_near_clip`, `vc_far_clip`, `vc_guide_{show,thirds,up,cross,clip}`, `vc_exposure`, `vc_grain`, `vc_chromatic`, `vc_camera_crt`, `vc_new_face_fit`, `vc_rate_mode`, `vc_target_fps` | HARNESS (no oracle) | give a registry oracle (`LLPrismLens` capture settings) and a login VCam fixture |
| cine_light_rig_tab | 5: `cine_aim_yaw_minus/plus`, `cine_aim_pitch_minus/plus`, `cine_fixture_source_size` | HARNESS | need a selected rig light as fixture |
| tron_tab | 2: `tr_apply_roto_preset_btn`, `tr_apply_rigrim_preset_btn` | HARNESS | presets write settings; add a preset-written oracle |
| weather_tab `weather_trigger`, ghosts_tab `nameplate_mode_combo`, actors_tab `btn_refresh` (NEW, was SKIP) | 3 | HARNESS | oracle |
| dance_tab `db_radius` (category A) | 1 | HARNESS | see section 4 |

## 9. Fixes ranked by rows covered

1. `visible_when` / `enable_when` in the manifest generator + SKIP-or-DEFER for code-hidden rows (vcam cards, gaze,
   ghosts, stepped groups, backups, Clear object): covers 92 reveal + 9 disabled + ~10 readouts + ~90 derived R6.
2. Selector card with >6 options (50 rows, section 3).
3. Fix REAL R1 (mShell null) - removes 23 R2 + 2 category A + derived R6, and restores Reset page, Changed only, Undo/Redo on 23 pages.
4. Generator: honour `hidden="true"`, `enabled="false"`, `unconnected="true"` (4 rows).
5. Harness: click kit segmented radios by group rect (4 not-reachable rows + 5 vcam radios hidden behind reveal) and support radio reveal selectors (3 stepped rows).
6. Harness: deliver Return to text editors (3 rows).
7. Harness: model confirm alerts as an expected notification (3 rows).
8. Fix REAL R2 (actor style refresh gate), plus R3-R4 copy/px edits (star pads fix covers 37 R3 rows).
9. `dirEffectClass` consumer handling (12 R6 rows) and reset-all/clear oracles (8 R6 rows).
10. Oracle corrections: night-mask percent inversion (2), flare preset index (1), sil_fill index (1).

## 10. Comparison with 10/14 and 10/13

| Run | FAIL | KNOWN_GAP | Notes |
|---|---|---|---|
| 10/13 (63440) | 41 | 16 | |
| 10/14 (63452) | 155 | 36 | 92 reveal, 26 cat A, 12 cat D |
| 10/15 (63476) | 191 (+207 rule rows) | 37 | 35 rows fixed since 10/14 (16 cat A, 12 cat D, 5 disabled, 2 not-found); 72 new FAIL rows |

NEW non-rule FAIL/KNOWN_GAP rows since 10/14 (72): 50 shot_tab segments; 3 text rows (`switcher_slot_label`,
`vc_camera_name`, `cine_seed`); `output_fps_spinner`, `drive_camera_check`, `set_RenderBloomHDR`; `sa_fast_rate`,
`sa_adapt_hysteresis`, `sa_background_rate`, `sa_actor_rate`, `sa_group_pick`, `sa_group_offset`;
`settings_crash_backups`, `settings_restore_backup`, `settings_delete_backup`; actors `target_list`, `combo_sil_fill`,
`txt_sil_note`, `txt_budget`, `la_btn_reset_all`, `btn_refresh`; ghosts `array_parameter_spinner`. All rule rows are new.
156 of the 191+37 rows are unchanged from 10/14, including all 92 reveal rows and `keyboard_tab_focus`.
