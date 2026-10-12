# Triage: run_functional_20261012_122652 (in-world, v2)

Source: `doc/director_kit/run_functional_20261012_122652/functional.csv` (+ `manifest_merged.json`).
Harness: `indra/newview/aluiperfselftest.cpp`. Method: every FAIL / KNOWN_GAP row was grouped by reason, the `observed` JSON
(rects, `visible_break` / `enabled_break`, `setting.before/after`, `effect`) was read for representative rows (and
scripted over all rows of a group), then matched to the panel code.

Headline: **no row reports category C ("click handled but no commit") or D ("owner unresolved")**. Every control the
harness could drive committed. The real defects found are layout / step-rounding problems, not dead controls.
Everything else in the 295 FAIL + 85 KNOWN_GAP is harness / manifest.

---------------------------------------------------------------------------------------------------

## 1. PRODUCT BUGS

### P1 (HIGH) Lightbox hand-scrolled tabs: scroll viewport is ~796 px taller than the visible tab area
Affects what the v2 Director "Stylize" (Roto Ink) and "Tron" launcher cards open (`floater_lightbox_settings.xml`
tabs `rotoink_settings`, `tron_settings`; same viewport also seen on actors / atmosphere(haze) / shadows_probes lightbox tabs).

- Symptom: the scroller rect extends below the floater, under the viewer's bottom toolbar. A human cannot see, click or
  scroll-bar-drag anything in that strip; the scrollbar track / down arrow is off-screen too.
- Evidence (observed JSON, all 82 rows share identical rects):
  - `rect.viewport = [534,-760,1010,1092]` (scroller, 1852 px tall) vs `rect.floater = [533,5,1009,1140]`.
  - The auto-wrapped Image tab in the same floater is correct: `viewport = [534,36,1008,1092]` (1056 px tall). So the
    correct tab content band is y 36..1092; the named scrollers overshoot it by 796 px.
  - Controls sit at `rect.ctrl` y = -26 ... -760 (below the floater bottom y=5) -> reason `not on screen` (78 rows:
    stylize 27, tron 51). The harness check is correct (`aluiperfselftest.cpp:3292-3309`).
  - Clicks that land inside the overshoot strip are intercepted by other views: `ri_intensity_slider`,
    `tr_grid_major_intensity_slider`, `tr_suit_seam_gain_slider` resolve to
    `.../bottom_toolbar_panel/.../speak` (ctrl y 1..21), `tr_suit_seam_density_slider` resolves to
    `Floater View/floater_lightbox_settings/Drag Handle` (ctrl y 25..45). 4 rows `not reachable`.
  - Successful (PASS) lightbox clicks all have point y >= 38 (e.g. `tr_grid_major_width_slider` y=38), i.e. inside the
    real band.
- Cause (statically unproven, 2 facts): XML scrollers are `follows="all" height="806"` in a `follows="all"` panel
  (`floater_lightbox_settings.xml:5381-5384`, `5857-5860`) and `ALScrollFit::installForTabs` names them
  (`alfloaterlightbox.cpp:160-167`); the auto-wrapped tabs (`alscrollfit.cpp:179-234`, `rect = tab->getLocalRect()`) are
  right. The named scrollers did not follow the floater reshape (floater is 476x1135 from saved rect vs 420x860 XUI).
  Width followed (476), height did not.
- Impact: Roto Ink doc is hand-typed 2520 px (scroll range only 668 px with an 1852 px viewport), so roughly the last
  ~800 px of the page (the Motion / pattern rows at ctrl y = -671..-760: `ri_pattern_seed`, `ri_motion_*`, `ri_motion2_*`,
  `ri_motion_step`) cannot ever be brought into the visible band. Tron (4100 px) has the same last-796 px loss.
- Suggested fix: make the named scroller track the tab panel (after `installForTabs` call
  `scroller->reshape(panel.w, panel.h)` for each `NamedScroller`, or let `wrapTabPanel`-style `rect = local` apply
  to them), and re-fit on floater `reshape`. Confirm first with one screenshot of Lightbox -> Tron at the saved size
  (scrollbar bottom should be visible).
- Caveat: because the harness only scrolls the control into the (oversized) viewport, some of the 78 rows are
  rescuable by scrolling further; see H8. Roto Ink's trailing rows are not.

### P2 (LOW, 10 spinners in the CSV) Spinner step 0.25 with `decimal_digits="1"` produces uneven steps and stores a rounded value
- Evidence: shot_tab rows `CinematicCamHoverDistance` 2.5 -> 2.8, `CinematicCamWhipDistance` 3.0 -> 3.3,
  `CinematicCamTwoShotMinDist` 2.5 -> 2.8, `CinematicCamFisheyeDuration` 2.5 -> 2.8, `CinematicCamSkimmerDistance`
  3.0 -> 3.3, `CinematicCamCableMissDistance` 2.0 -> 2.3, `CinematicCamCableDuration` 5.0 -> 5.3,
  `CinematicCamTiltWhipPeriod` 2.0 -> 2.3, `CinematicCamArcDistance` 2.6 -> 2.8, `CinematicCamSpiralEndRadius` 1.4 -> 1.6.
  XUI increment is 0.25 but the setting after one arrow click moves by 0.3 / 0.2 (LLSpinCtrl rounds to the display precision).
- Where: `skins/default/xui/en/panel_cinecam_params.xml` lines 423, 709, 736, 824, 908, 962, 998, 1099, 1111, 1134, 1165,
  1299, 1311, 1347, 1374, 1376 (16 spinners with `increment="0.25" decimal_digits="1"`).
- Suggested fix: `decimal_digits="2"` on those spinners, or `increment="0.1"`/`0.5`.

### P3 (UX, by design but poor) Hidden rows leave blank gaps in the Roto Ink / Tron scrollers
`alfloaterlightbox.cpp:893-896` comment: "hidden rows leave blank space in the scroll area rather than reflowing". With P1 this
makes the visible rows even harder to reach. Consider reflow, or collapsing the hidden group heights.

### Not product bugs (checked, to avoid chasing)
- category A rows: all had `owner.instrumented_runs = 1` and widget state changing (e.g. `db_wave_speed` 0.5 -> 1.0,
  `gaze_priority_combo` 0 -> 1): the handlers run; they are bound to engine state, not a setting, so the oracle is blind.
- `cine_fill_ev` disabled-by-parent: `alpanelcinelightrig.cpp:2617` intended (easy mode / ratio lock).
- Path editor panels disabled: intended (`alpanelpatheditor.cpp:842-875,979-984,1567-1576`), needs an actor.

---------------------------------------------------------------------------------------------------

## 2. HARNESS GAPS (grouped, with concrete fix)

### H1. "reveal did not settle within 6 frames" (64) - conditional visibility, control itself is `setVisible(false)`
`visible_break.path` is the control itself in every row (`functionalChainBreak`, `aluiperfselftest.cpp:2919-2933`, returns the
first hidden view, which is the control). The manifest has no `visible_when` / `reveal` driver for these.
- stylize (28): visibility from `ALFloaterLightBox::updateRotoInkAnimVisibility` (`alfloaterlightbox.cpp:683-817`):
  `ri_match_reach` needs ink mode 1; `ri_color2_swatch` needs Color2Mode in {1,3,4}; `ri_color2_speed/length` need {1,2};
  `ri_subject_*` need CineOutlineSubjectMode (3 / 3-9 / 4 / 7-8); `ri_pattern_*` need PatternType != 0 (drift 1|2, hatch 4);
  `ri_motion_shape/angle` + `ri_motion2_*` per style; `ri_motion_phase` needs MotionTempo > 0; `ri_metric_thr` needs DepthMode 1.
- tron (16): `updateTronVisibility` (`alfloaterlightbox.cpp:897-1090`): `tr_trace_*` need TronTraceEnabled (pulse rows also
  TracePulseAmount > 0); `tr_rim_scan_*` need TronRimEnabled && ScanAmount > 0; `tr_trail_width` / `tr_subject_target_combo` per
  trail style / single subject.
- image (10): `updateTonemapper` (`alfloaterlightbox.cpp:1358-1396`): `tm_*` only when AlchemyRenderTonemapType == 0, `lpm_*` only == 7.
- ghosts (2): `animation_library_combo` needs a directed ghost (`alpanelghoststudio.cpp:1443`); `array_parameter_spinner` needs a
  formation with a parameter (`:3281-3283,3681`).
- switchboard (2): `anim_pose_name/anim_pose_load_method` hidden for animation-type slots (`alpaneldirectoranimswitcher.cpp:542-545`).
- gaze (2): `gaze_cast_combo` only in cast mode (`alpanellensgaze.cpp:1247`); `gaze_composition_mix_slider` only for composition blend (`:1355`).
- takes (2): `FlycamWeightedPreset/Weight` only in weighted mode (`alpanelflycamrecorder.cpp:120-121`).
- diopter (2): `diopter_blend/power` gated by `aldiopterrelevance.cpp:59,278`.
- Fix: add per-entry `reveal_settings: {Setting: value}` (set-then-restore, same mechanism as the combo `reveal` selectors in
  `revealFunctionalControl`, `aluiperfselftest.cpp:3345-3360`) driving the setting named above, then re-run
  `updateRotoInkAnimVisibility()` / `updateTronVisibility()` / `updateTonemapper()` (they fire from setting signals, so setting
  the value is enough). Do not downgrade to SKIP without the driver.

### H2. "not reachable: disabled-by-parent" (44) - control itself disabled by panel state
`enabled_break` is the control, not a parent (same helper). Needs a prerequisite, not a product fix.
- path_tab (43): all gated by `have_actor` / `waypoint_edit` / `walkable` etc. (`alpanelpatheditor.cpp:774-784,842-875,979-984,
  1521-1576,1616-1624,1644-1652,1727-1737`). Prerequisite: select an actor with a path, turn Edit mode on, add 2 waypoints.
  Add `prereq: select_actor+edit_mode+2_waypoints` to the manifest and run `prepareFunctionalPrerequisite` for it.
- cine_light_rig `cine_fill_ev` (1): enabled only when `!mEasyModeActive && !CineLightRigRatioLock` (`alpanelcinelightrig.cpp:2617`).
  Prerequisite: Easy mode off, ratio lock off.

### H3. Spinner "observed value/state did not match manifest" (45 FAIL + 1 KNOWN_GAP) - expected-value snapping is wrong
`aluiperfselftest.cpp:4873-4885` computes `expected = min + round((before +/- inc - min)/inc) * inc`. LLSpinCtrl simply does
`before +/- inc` (rounded to display digits), it never snaps to a `min`-anchored grid. Every one of these rows shows
`after - before == XUI increment` (e.g. `switcher_interval` 8.0 -> 8.5 inc 0.5, `zolly_duration` 8.0 -> 8.5 but harness expects 8.6
because min=0.1; `FlycamOperatorReactionLag` 0.18 -> 0.13, harness expects 0.15 because it snaps to the 0.05 grid). The settings
did change and handlers fired (`owner.instrumented_runs=1`).
- Fix: `expected = clamp(before + direction*inc)`, then round only to the spinner's decimal digits (read `LLSpinCtrl::getPrecision()`),
  no `min` anchor. (The 10 rows listed in P2 will still fail until P2 is fixed; make them report P2 explicitly:
  `|after-before| != increment`.)
- `heading_spinner` (3.576 -> 9): same cause (+5 then rounded to 0 digits).

### H4. Color swatches (12 FAIL) - the generic equality ignores the Color3/Color4 normalisation done later
Observed: `effect.picker_opened=true`, `effect.setting_oracle=true` (the swatch-specific block at `:5138-5165` passes), but the earlier
generic compare `expected_ok = ... functionalEqual(actual, expected, type, ...)` at `:4905-4906` already set `expected_ok=false`
because the setting holds `[0.2,0.6,1.0,0.0]` vs expected `[0.2,0.6,1.0]`. Fix: skip the `:4905` equality for `LLColorSwatchCtrl`
(or compare 3 channels for `type == "Color3"` there). Rows: focus GuideColor, filters gradnd_tint, bloom halation tint, ri_color_swatch,
tr_color_primary/secondary/accent/pulse, tr_grade_tint, ch_color/ch_sun_tint, ps_tint.

### H5. "category A: commit fired but the external oracle did not move" (26 FAIL + 24 KNOWN_GAP)
Each row has `owner.instrumented_runs=1`, so the handler ran. They have no bound setting (`setting.name=""`) and the visible-page-state
diff did not change, so `observable=false` (`:4903-4918`).
- Selected value already current (manifest picked the default): `move_tab/scope_radio` (test item_index 0 = already selected,
  `ActorMoverSync` false -> false). Fix: pick `item_index != current`.
- Empty lists (nothing to select): `groups_list`, `waypoint_list`, `prop_list`, `node_list`, `db_playlist_list`,
  `localfog_volume_list` (`value=` empty, `programmatic_commit_path: generic fallback`). Fix: seed one entry via the world prerequisite.
- Engine-state controls with no setting (dance ball 19, gaze 14, ghosts 2, takes `btn_stop`, switcher 2, `cine_aim_*`, softedge/ir preset):
  add a consumer oracle per control family: `ALDanceBall` model getters, `ALGazeMotor` slot values (the manifest already has
  `gaze_oracle_field/value`), `ALGhostStudio::getShowAll()`, recorder state for `btn_stop`, `ALCineLightRig` aim yaw/pitch.
  `takes/btn_stop` needs a recording first.

### H6. "no independent state oracle" (20 FAIL) - FAIL by construction, not a measurement
`aluiperfselftest.cpp:5166-5170` forces `expected_ok=false` when the manifest says `no_state_oracle`. Rows: props `btn_enroll`,
`btn_node_add`, `btn_stop_all`; ghosts `edit_ghosts_check`; dance 15 (`db_on`, `db_add_*`, `db_play_*`, `db_freeze`, `db_shuffle`, `db_ramp_go` ...);
gaze `gaze_slot_you`. Fix: write the oracle (same as H5) or move to KNOWN_GAP/SKIP with an explicit reason; they must not be
FAIL counted as product failures.

### H7. KNOWN_GAP "known gap remains marked UNWIRED" (25) - manifest flag is stale
Those rows passed every check (`reason` is only the leftover `known_gap` flag, `:5284-5285`). Examples: dance `db_mode`,
`db_t_speed`, `db_t_desync`; gaze `gaze_enable_check`, `gaze_deadzone_spinner`, `gaze_blinks_check`, `gaze_break_freq_slider`,
`gaze_composition_combo`, `gaze_range_preset_combo`; 13 cine_light_rig (`cine_easy_*`, `cine_aim_reset`, `cine_fixture_*`, flares,
`cine_open_cues`); `shake_preset_combo`, `ir_preset_action`, `switcher_custom_enabled`. Fix: clear `known_gap` in the
wiring table for rows that now pass (all are `WIRED_CPP` / `V2_SUSPECT_FLOATER_SCOPED: run and confirm in-world`, and the
in-world confirm just happened).

### H8. Lightbox rows "not on screen" - scroll strategy (part of the 78; do after P1 is fixed or to isolate it)
`functionalReachable` (`aluiperfselftest.cpp:3219-3251`) only does `scrollToShowRect` (minimal scroll into the oversized scroller).
Fix: after that, if the click point is not inside `host_rect AND visible tab content`, scroll so the control is at the TOP of the
scroller (`LLScrollContainer` scroll position), then re-test. Rows still failing after that are true P1 casualties.

### H9. Combo popup list click point "outside Director/redirect floater" (2: `shot_tab/switcher_slot_mode` KG, `cine_light_rig_tab/cine_fx_combo`)
`item_point [1795,1339]` / `[1515,1322]` with `list_rect [..,647,..,1350]`: the drop list is a popup that legitimately extends above
the Director floater (top y=950). `functionalPreDispatch` (`:3149-3153`) applies the host-rect test to popup list items. Fix: skip the host
containment test when the target is a popup list item (`combo.open_click.list_item_visible == true`). (Side note: both lists are
~700-1180 px tall, i.e. very long combos; worth a look, but not a bug.)

### H10. "spinner default is outside the manifest range" (2: `switchboard_tab/anim_slot_speed`, `gaze_tab/gaze_ease_release_spinner`)
Entries have `setting: null`; `mFunctionalBefore` is empty so `asReal()==0`, below `min` 0.1 / 0.05 (`:4893-4902`).
Fix: when no setting, take `before` from the spinner widget value.

### H11. "in-world design: ..." KNOWN_GAP (33) - correct, needs world state
dance transport/blocking 15 (needs populated dance ball), ghost/crowd edits 9, prop stop / drive all 2, animate list 1, gaze cue list+delete 2,
actor-style target 1, vcam monitor strip / tile / source type 3. Harness fix is an in-world fixture (spawn 2 ghosts, enrol a prop path,
save one gaze cue, a populated inventory animation); nothing to fix in product.

---------------------------------------------------------------------------------------------------

## 3. COUNTS (295 FAIL + 85 KNOWN_GAP = 380 rows)

| Group (reason) | Rows | Verdict | Where |
|---|---:|---|---|
| not on screen (stylize 27, tron 51) | 78 | PRODUCT P1 (+ H8 for some) | 1/P1, 2/H8 |
| not reachable: bottom toolbar / lightbox Drag Handle | 4 | PRODUCT P1 | 1/P1 |
| reveal did not settle (stylize 28, tron 16, image 10, ghosts 2, switchboard 2, gaze 2, takes 2, diopter 2) | 64 | HARNESS H1 | 2/H1 |
| disabled-by-parent (path 43, cine_fill_ev 1) | 44 | HARNESS H2 | 2/H2 |
| value mismatch: spinners | 45 FAIL + 1 KG | HARNESS H3 (10 of them also PRODUCT P2) | 2/H3, 1/P2 |
| value mismatch: color swatches | 12 | HARNESS H4 | 2/H4 |
| category A (FAIL 26 + KG 24) | 50 | HARNESS H5 (oracle blind / empty list / already-selected) | 2/H5 |
| no independent state oracle | 20 | HARNESS H6 (forced FAIL) | 2/H6 |
| known gap remains marked UNWIRED | 25 | HARNESS H7 (stale manifest flag; controls pass) | 2/H7 |
| in-world design KG (needs world fixture) | 33 | HARNESS H11 | 2/H11 |
| outside Director/redirect floater (combo popup) | 2 | HARNESS H9 | 2/H9 |
| spinner default outside manifest range | 2 | HARNESS H10 | 2/H10 |
| **Total** | **380** | | |

Product-bug rows: 82 (P1, partly H8) + 10 (P2, overlapping the 45 H3 rows). Everything else, ~290 rows, is harness / manifest.
Dead controls (category C/D) found: 0.
