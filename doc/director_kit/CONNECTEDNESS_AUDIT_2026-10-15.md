# Director v2 connectedness audit (2026-10-15)

Read-only audit. Scope (revised by the user mid-task): the Director v2 console only. That means every group and page, the shell chrome, the bottom transport bar, the popovers, the help column, Quick, Settings, and the legacy panels embedded inside Director pages. Standalone floaters the Director opens are out of scope; only their launcher buttons are checked, to see that each is connected and opens something.

Nothing in the repo was changed except this file. The tree was at HEAD `3d050fefebb` and `git status` was clean for `indra/newview` when the audit started. Another agent is editing label-sizing code, so the R4 numbers describe the tree before that work.

Paths are relative to the repo root. XUI files are under `indra/newview/skins/default/xui/en/`. Anything that could not be proven from source is marked **unverified**. No viewer was run.

## 0. Files actually read, and method

**Read in full or nearly in full**
- Shell and kit core: `alkitshell.cpp` and `.h`, `alkitpopover.cpp`, `alkitlazypage.cpp`, `alkitbar.cpp`, `alkithelp.cpp`, `alkitbinding.cpp`, `alkitrow.cpp`, `alkitpage.cpp`, `alkitstyle.h`, `alkitsearch.cpp` (lines 1-370), `aldirectorhotkeys.cpp`.
- Partly read: `alkitcard.cpp` (205-360, 455-480, 750-1000, 1120-1214; the rest by grep), `alkitqol.h` (paste and favourite helpers), `alkitstyle.cpp` (head only).
- `floater_director_v2.xml` in full.
- `llfloaterdirector.cpp`: 1215-3010, 3035-3500, 3744-4060, 4430-4445, 5722-5740, 5875-5910, 6874-7283. The rest was grepped.
- `alpaneldirectorvcam.cpp` 100-440 and 903-1010, `aldirectorcastui.cpp` (`mergeSubjectCombo`), and `alpanellensgaze.cpp`, `alpanelexposurehub.cpp`, `alpanelrigrim.cpp` (excerpts).
- `doc/director_kit/COVERAGE_AUDIT_2026-10-15.md` (headings, sections 2-6) and `doc/director_kit/HIDDEN_GATES.md` (table).

**Parsed by script, not read line by line**
- All 38 `panel_dpage_*.xml` pages, expanded through every `filename=` include. The 10 embedded legacy panels are `panel_actor_mover`, `panel_path_editor`, `panel_prop_mover`, `panel_ghost_studio`, `panel_dance_ball`, `panel_exposure_hub`, `panel_anim_preview`, `panel_lens_gaze`, `panel_pose_polish` and `panel_director_anim_switcher`.
- `director_kit_index.xml` (4,261 entries), `settings*.xml` (3,632 settings), `textures.xml`, `llviewerfloaterreg.cpp`.
- A C++ name scan over every `indra/newview/*.cpp` and `*.h` file.

**Not read**
- `alkithistory.cpp`, `alkitpresets.cpp` (grep only), `alkitmenu.cpp`, `alkitmath.h`, `director_kit_gen.py`.
- The `.cpp` of the embedded legacy panels (Ghost Studio, Dance Ball and similar). Their controls were matched by name only, which is weaker than reading their binders.

**Text-width method (R4).** IBM Plex Sans variable font (`indra/newview/fonts/IBMPlexSansVar-Roman.ttf`) measured with PIL. Kit12 is 9 pt at 96 dpi, so 12 px; Kit13 bold is 13 px; Kit15 bold is 15 px. Box padding is ignored. Figures are about ±5%. Layout constants come from `alkitstyle.h:16-31`. The label width available to a row label is `LABEL_W - 18 = 82 px` (`alkitrow.cpp:477`).

**Scripts.** They live in the session scratchpad, not in the repo: the widget inventory with include expansion, the setting-type and range check, the index-versus-tree check, the text-width scan and the transport-bar collapse simulation. Section 4 turns them into the planned static gate.

## 1. Summary

| Rule | Findings | HIGH | MED | LOW | Verified-clean notes |
|---|---:|---:|---:|---:|---|
| R1 Connected | 14 | 2 | 3 | 9 | 1,081 setting-bound rows: 0 missing settings; 24 link/action targets: 0 broken targets (but see R1-1: the click path is broken) |
| R2 Reachable | 8 | 1 | 3 | 4 | 38 pages, 10 groups (9 rail + gear): all reachable; 0 orphan cards; 0 duplicate widget names |
| R3 Popovers | 5 | 0 | 2 | 3 | Popover child lookups: all recursive, so none broke when `ensureScroller` re-parented children |
| R4 Fit | 11 | 0 | 7 | 4 | 1,492 labels measured |
| R5 Raw names | 7 | 1 | 2 | 4 | Authored help/tooltips in the 38 pages: 1 raw setting name in ~1,500 strings |
| R6 Value wiring | 5 | 0 | 1 | 4 | 0 type, default-range, combo-value or widget/row-range mismatches across all 1,081 rows |
| R7 Help/hotkey text | 5 | 0 | 2 | 3 | |
| R8 Searchable | 12 gaps checked | | | | 7 still open, 1 partly closed, 4 not re-checked |

**Top findings, in the order a user hits them**
1. **Every link row, card link, Quick "Open on its page" row and "Show in search" menu item does nothing.** `navigateTo(string)` splits at the first `/`, so `capture_group/scene_options_tab` becomes page `capture_group`, which is not a page (R1-1).
2. **The page-bar Reset, Changed-only and in-page filter do nothing on all 13 legacy pages** (R1-2). The trash and undo icons are real controls with no target.
3. **The inline scene-name box probably appears at the wrong place** when Save, Save as or Rename is pressed. The bar never relays out after the swap (R3-2).
4. **Clicking a popover's anchor while it is open closes it and immediately reopens it** (R3-1).
5. **Below about 933 px of window width, Save, the scene menu (Save as, Rename, Delete) and Reset marks are collapsed out of the transport bar** and have no other route (R2-2).
6. **The Moves page's OTS, Zolly and Hero Arc buttons never get their enable gating or tooltips**, because `refreshPrismDependents()` returns early on widgets that only exist in v1 (R1-3).
7. **Scene Save and Load silently drop the Shot preset in v2**, because `mCineCamPanel` is v1-only (R1-4).
8. **Two controls have no consumer:** the HDR bloom toggle (`RenderBloomHDR`) and Out FPS (`TemporalOutputFPS`) (R1-5, R1-6).
9. **The help column prints "Setting <InternalName>" for every setting row**, and search results show setting names and widget-name labels (R5-1 to R5-3).
10. **48 row labels, 2 page-master labels, 6 section chips, the arm-popover label "Recorder capture" and the cast-list name column are truncated** (R4).

## 2. Findings by rule

Severity: HIGH = a core promise breaks or a visible control does nothing; MED = degraded, or breaks under a plausible condition; LOW = polish or latent.

### R1 Connected: every interactive control has a binding, an attached handler, or a declared link

Coverage: 1,497 kit rows. Of these, 1,081 carry a `setting` (every one exists in `settings*.xml`), 24 carry `action` or `link_to`, and 355 `bind="code"` rows name a widget that C++ references. The 51 `bind="code"` or `bind="none"` rows with no literal C++ reference were all explained: 12 static-text readouts; the 12 `switcher_slot_NN` buttons built from `llformat("switcher_slot_%02d")` at `alpaneldirectorswitcher.cpp:67`; 20 `cine_<role>_*` rows built from `prefix + "_shaft"` at `alpanelcinelightrig.cpp:890-892`; and 7 vec rows bound through `setting`. Of the 649 controls in embedded legacy panels with no row, 599 have a `control_name` or a name in C++, 3 have an XML `commit_callback`, and 47 are explained: radio items inside a radio group, `Machinima.ResetControl` buttons (`panel_lens_gaze.xml:247`), `"rst_" + setting` buttons (`alpanelexposurehub.cpp:83`) and the `anim_slot_NN` loop.

| ID | Page / widget | File:line | What is wrong | Sev | Suggested fix |
|---|---|---|---|---|---|
| R1-1 | Settings `settings_add_self_link`; Shot `shot_use_selected_link`, `handheld_operator_link` (card link); Operator `frame_offset_up_link`; Image `exposure_link`, `set_lens_filters_link`; Stepped `sa_roto_link`, `sa_tron_link`, `sa_actorfx_link`, `sa_time_link`; Roto Ink `ri_sync_tron_link`; Tron `tr_rigrim_link`, `tr_roto_link`; Shadows `shadows_soft_projector_link`; every Quick "Open on its page" proxy; the "Show in search" item in every row's right-click menu | `alkitshell.cpp:1042-1054` (parse), callers `alkitrow.cpp:210`, `alkitcard.cpp:473`, `alkitshell.cpp:1888` | `navigateTo(std::string)` splits at the first `/`. `camera_group/frame_orbit_tab` becomes `page_key="camera_group"`, `widget="frame_orbit_tab"`. `ensurePageBuilt` then calls `selectPage("camera_group")`, which needs an `ALKitLazyPage` of that name; the group is a plain `<panel>`, so it returns false and the click is silent. `showRowInSearch` builds `"group/page/widget"` and fails the same way. The 24 targets all resolve; only the string parser is wrong. Search results are fine because they pass an `ALKitTarget` with `page_key = "group/page"` (`:1653-1659`). | HIGH | Parse `group/page[/widget]` (split at the second slash), or take `page_key` as the longest prefix that names a lazy page. Move the parser into `alkitqol.h` and add a unit test with all 17 link strings. |
| R1-2 | All 13 legacy pages: Move, Path, Props, Ghosts, Dance, Animate, Switchboard, Gaze, Gaze Cues, Polish, Exposure, Practicals, Light Cues | `alkitpage.cpp:416-424` (Reset button), `:568-580` (Changed only), `:749-779` (legacy branch returns before cards exist), `:1498-1506` (`performPageReset` loops `mCards`) | The page bar always builds Reset page, Undo, Redo, "Find a control..." and Changed only. On a legacy page there are no cards and no rows, so Reset page opens a confirmation popover and then resets nothing. Changed only filters nothing. In-page search filters nothing (results popup works). Undo and Redo are connected only through `watch_settings`. | HIGH | For `legacy` pages, hide or disable Reset page and Changed only, or drive them from the page's `watch_settings` list (reset each watched setting that has a default). Hide the controls that cannot act. |
| R1-3 | Cast roster / Camera > Moves: `btn_build_ots_pair`, `btn_start_zolly`, `btn_start_hero_arc`, `prism_summary`, `btn_prism_manage` | `llfloaterdirector.cpp:7017-7023`; binders `:1604-1608` (v1 only), `:2629-2652` | `refreshPrismDependents()` returns unless all of `mOtsPairBtn`, `mPrismSummaryText`, `mZollyBtn`, `mHeroArcBtn` are set. `prism_summary` and `btn_prism_manage` are in no v2 page (`grep` of all `panel_dpage_*.xml`: 0 hits), so the function never runs. The three buttons still work, because `onClickBuildOtsPair`, `onClickZolly` and `onClickHeroArc` re-check and raise a `GenericAlert`. But the enable state and the explanatory tooltips (`:7040-7056`, `:7096-7106`) are dead. | MED | Split the guard: gate each button on its own pointer. Drop `prism_summary` and `mPrismManageBtn` for v2 or add the rows. |
| R1-4 | Scene Save / Load (transport) | `llfloaterdirector.cpp:4045-4052` (save), `:4435-4443` (load), `mCineCamPanel` set at `:1619`, `:2663`; widget `cinecam_params_embedded` only in `floater_director.xml:1120` | In v2 `mCineCamPanel` is always null. Scenes silently omit `cinecam_preset` on save and ignore it on load; the Shot preset is not part of a v2 scene. | MED | Read and apply the Shot preset through `ALPanelCineCamParams`'s static helpers (the Shot page already uses them: `ALPanelCineCamParams::applyShakePreset`, `resetModeToDefaults`). |
| R1-5 | Look > Bloom & Glow `set_RenderBloomHDR` (and Exposure hub bloom sliders, `enabled_control="RenderBloomHDR"`) | `panel_dpage_look_bloom_glow.xml:9-10`; `panel_exposure_hub.xml:70-74`; no C++ or GLSL reader | The toggle has no consumer. Its help says "Off falls back to the legacy glow", which is false. Already recorded as HIDDEN_GATES row 8. | MED | Wire it in `pipeline.cpp` (the HDR branch near 22941) or remove the row and the `enabled_control` gating. |
| R1-6 | Capture > Time `output_fps_spinner` ("Out FPS") | `panel_dpage_capture_temporal.xml:21-22`; no reader of `TemporalOutputFPS` in C++ | The setting is never read. `alpaneltemporalcapture.cpp:90-102` computes the post speed-up from the speed alone. The help says the value "is used to show the recommended post speed-up". | LOW | Use it in the readout or drop the row. |
| R1-7 | Arm popover rows (`arm_moves_check`, `arm_camera_check`, `arm_play_check`, `arm_capture_check`, `delay_spinner`) | `alkitrow.cpp:414-431` (star), `:1035-1039` ("Show in search"), `alkitqol.h:73-77` | Popover rows are `ALKitRow`s with no `ALKitPage`, so `meta.page_key` is empty. The hover star, "Add to Quick" and "Show in search" in the right-click menu are no-ops (`isValidFavorite` rejects an empty page). | LOW | Make those three items opt-out for rows outside a page (`kit_row favorite="false"`), or hide them when `page_key` is empty. |
| R1-8 | Move page `groups_list`, `group_run_combo`, `btn_group_*` | bound in `bindMovePage` `llfloaterdirector.cpp:2346-2368`, only after the page is built; the cast roster's `group_assign_combo` is bound in `bindChrome` `:1873` | Works, but `refreshGroupControls()` (`:5722`) keeps its early return on `mGroupAssignCombo`, so run-combo repopulation depends on the Move page having been built. If Move was never opened, the run combo is not populated until it is. Cosmetic. | LOW | None needed; note for the functional run. |
| R1-9 | `kit_ui_ms` / `KitShowUIMs` | `llfloaterdirector.cpp:1917-1941`; no `kit_ui_ms` in any v2 XML | The "UI x ms" readout and its setting watch have no widget in v2. | LOW | Remove, or add the readout. |
| R1-10 | `DirectorV2CastRosterOpen` | `alkitshell.cpp:343`; nothing writes it | Persisted setting with no control. The cast roster always docks in the rail when there is room and otherwise lives in the Cast chip popover. | LOW | Add a toggle or drop the setting. |
| R1-11 | Launcher cards `card_diopter_open`, `card_haze_open`, `card_shadows_probes_open`, `card_snapshot_open`, `card_actor_mask_open`, and `btn_prism_manage`, `scene_metadata_text` | `llfloaterdirector.cpp:2822-2881`, `:1784` | Guarded by `findChild`, so harmless dead code. Those ids do not exist in v2 pages. | LOW | Delete. |
| R1-12 | VCam page | `alpaneldirectorvcam.cpp:216-225`, `onControl` `:903` onward | 138 `vc_` controls; 110 are neither bound by `control_name` nor named in the `onControl` name chain, so they depend on the table-driven handlers further down the file. Ten were spot-checked (`vc_eye_x`, `vc_exposure`, `vc_rate_mode`, `vc_target_fps`, `vc_fov_mode`, `vc_near_clip`, `vc_guide_show`, `vc_bone_enabled`, `vc_camera_name`, `vc_grain`) and each has a handler at lines 1182-1242. | LOW | **Unverified in full.** Add a gate: every `vc_` control must be named in `onControl` or the table. |
| R1-13 | Quick favourites whose source row is missing | `alkitshell.cpp:1870-1873` | `createProxy` returns null and the favourite is silently dropped from view while staying persisted. | LOW | Show a "missing control" placeholder or prune. |
| R1-14 | Rig `cine_seed` (text row on a U32 setting) | `alkitbinding.cpp:24` | `static_cast<U32>(value.asInteger())` on an S32 LLSD; seeds above 2^31 wrap. Bad text gives 0. | LOW | Add a numeric validator or make it a number row. |

### R2 Reachable: every page, card and popover is reachable through the real navigation code

Verified: the 38 lazy pages sit in 10 `director_tabs` groups. Nine are on the rail (`floater_director_v2.xml:5-13`) and `settings_group` is reached by the gear. The gear commit calls `selectPage("director_settings_tab")` (`alkitshell.cpp:440-444`). `selectPage` walks up from the inner tab container to the child whose parent is `mDirectorTabs` (`:1117-1124`), and `unwrappedPages()` keeps group panels unwrapped (`:1218-1237`), which is what fixed the earlier Settings failure. All 24 `link_to` and `action` targets resolve to existing pages, widgets, registered floaters or Lightbox tabs. No card is missing from a section, no card or widget name is duplicated on a page.

| ID | Page / widget | File:line | What is wrong | Sev | Suggested fix |
|---|---|---|---|---|---|
| R2-1 | All link rows | see R1-1 | Link rows are reachable controls with unreachable targets. | HIGH | R1-1 |
| R2-2 | Transport bar: Reset marks (`btn_reset_marks`), Save, `...` scene menu | `floater_director_v2.xml:116` (`collapse_order`), `alkitbar.cpp:165-175` | The bar collapses children in `collapse_order` until it fits. Required width is about 932 px with nothing optional visible. At 850 px (the floater minimum) `btn_reset_marks`, `btn_scene_menu` and `btn_scene_save` collapse. At 900 px `btn_reset_marks` collapses. When the "ACTION has nothing armed" strip is visible (about 1,130 px required), at 1,000 px reset marks, scene menu and Save collapse too, and at 850 px so do Load, Set marks, paths and the legend button. Save, Save as, Rename and Delete have no shortcut. F5 and F8 cover marks only. | MED | Move Save/Save as/Rename/Delete into the shell as a persistent menu, or reorder `collapse_order` so scene controls collapse last, or raise `min_width` to the budget. |
| R2-3 | Search index entries pointing at missing widgets: Practicals 16, Gaze Cues 5, Light Cues 5, VCam 1 | `director_kit_index.xml` lines 522, 1006, 1475 and others; see section 6 | 27 non-external entries name widgets that are not in the page tree, for example `sl_cast_shadows`, `cue_goto`, `gaze_cue_play`, `vc_monitor_tile_manifest`. The hit opens the launcher page, finds nothing and highlights nothing. Same as COVERAGE G2. | MED | Re-emit as external rows (the floater rows already exist, for example index line 2989) or drop (A6). |
| R2-4 | Legacy search jump | `alkitshell.cpp:1072-1100` | Opens accordions and scrolls, but never selects inner `tab_container` tabs. Ghosts, Dance and Exposure hide controls in inner tabs. COVERAGE G11. | MED | Call `selectTabRecursive` on the legacy host first (A8). |
| R2-5 | Cast roster Group box | `alkitshell.cpp:874-886` | Hidden when fewer than 3 list rows would remain. At the floater's minimum height the rail leaves about 130 px for the roster and the box is hidden, so tagging a cast member into a group is unreachable. The help text still points users at it. | LOW | Move group tagging to the cast right-click menu, or keep the box and shrink the list. |
| R2-6 | `/` search shortcut | `llfloaterdirector.cpp:3236-3250`, `alkitpage.cpp:1183-1215` | At narrow page widths the page-bar search editor is hidden (`search_fits` false: preset-strip pages fall below 120 px at a page width under about 790 px) but `/` still focuses it, so typing is invisible. | LOW | Check `getVisible()` before focusing. |
| R2-7 | Alt+PgUp/PgDn group cycling | `alkitshell.cpp:1713-1725` | Cycles into Settings (no rail button, no rail selection); the floater title then reads "Director > Settings > Settings" (`:758`). | LOW | Skip `settings_group` in the cycle; title shows the page once. |
| R2-8 | External search hit with missing target | `alkitshell.cpp:1662-1692` | Opens the floater, then returns false when the target is not found; the user sees an open floater with no highlight and no message. | LOW | Toast "Opened <floater>; control not found". |

### R3 Popovers and overlays

Popovers checked: arm, legend, hotkeys, scene menu (XML, `ALKitPopover`); search results, cast roster, per-page Reset confirm and per-page Who chooser (code-built).

- **Anchors.** The three XML anchors (`btn_arm`, `btn_help`, `btn_scene_menu`) are resolved in `ALKitPopover::postBuild` (`alkitpopover.cpp:109-118`). The code-built popovers call `bindAnchor` directly (`alkitshell.cpp:502`, `:1326`; `alkitpage.cpp:434`).
- **Sizing.** XML popovers declared with no height are laid out by content (`alkitpopover.cpp:123-133`, `184-232`), then re-capped to the shell height on `show()` (`:258-261`). Search (430x390), cast (300x360), Reset (320x96) and Who (360 x items) are fixed or item-derived by design.
- **Front.** `show()` calls `sendChildToFront` (`:278-279`).
- **Child lookups after `ensureScroller`.** Every lookup of a popover child is recursive. The calls are `getChild("btn_legend_hotkeys")` (`llfloaterdirector.cpp:1838`), `findChild("btn_legend_close")` (`alkitpopover.cpp:134`), `getChild("btn_scene_save_as")`, `"btn_scene_rename"`, `"btn_scene_delete"` (`:1787-1796`), `getChild("kit_hotkeys_popover")`, `getChild("legend_panel")` (`:1834-1835`). `LLView::findChild`/`getChild` default to `recurse=true` and none passes `false`, so moving non-button children into the scroll document broke nothing. The arm-popover rows and the legend body moved into `*_content`; their controls (`arm_*_check`, `delay_spinner`) are found only through their rows' own `findChild`.

| ID | Popover / widget | File:line | What is wrong | Sev | Suggested fix |
|---|---|---|---|---|---|
| R3-1 | Arm, scene menu, legend (`btn_help`), cast chip, Who chip, page Reset | `alkitshell.cpp:1995-2011`, `alkitpopover.cpp:299-309` | `ALKitShell::handleMouseDown` closes an open popover for any click outside it, including a click on its own anchor. The button's commit then runs `toggle()`, sees the popover hidden and reopens it. The anchor cannot close its own popover. **Unverified in-world; follows from the code order.** | MED | Do not close when the click lands on the open popover's anchor (compare against `mAnchor`), or record a "closed this click" flag that `toggle` honours. |
| R3-2 | Transport scene-name editor (`scene_name_editor`) | `llfloaterdirector.cpp:3849-3850`, `:3867-3868`, `:3883-3884`; `alkitbar.cpp:123-210`; `floater_director_v2.xml:130` | Save, Save as and Rename hide `scene_combo` and call `scene_name_editor->setVisible(true)` directly. `ALKitBar` lays out only visible children and only on `reshape` or `setCollapsibleVisible`. The editor was hidden at the last layout, so it has no position (the XML gives no left/bottom) and appears at the bar's origin, over the ACTION button. REVIEW_SPECS_OPUS_R1.md:205-206 already flagged that it is not in the bar's width budget. **Unverified in-world.** | MED | Call `mTransportBar->setCollapsibleVisible(editor, true)` and `(combo, false)` (or add a `swap` API) so the bar relays out; assert placement in the layout gate. |
| R3-3 | Who chooser popover | `alkitshell.cpp:1318-1404` | The popover is built once from the combo's items. On each open it relabels buttons by index over the currently enabled items, but each button's commit still carries the value captured at build time. If an item's enabled state changes (a subject slot becoming unset), label and value shift apart. The combo values are fixed slot numbers (`aldirectorcastui.cpp:136-141`), so this is latent. **Unverified.** | LOW | Rebuild the choice buttons on open, or capture the item index and read its value at click time. |
| R3-4 | Popovers anchored to a collapsed bar child | `alkitpopover.cpp:249-282` | `show()` positions from the anchor's stale rect when the anchor was collapsed (hidden). The legend/hotkeys popover then opens at an old position. | LOW | Refuse or re-anchor to the bar when `!anchor->getVisible()`. |
| R3-5 | Fixed-size popovers (search 430x390, cast 300x360, Reset 320x96) | `alkitshell.cpp:449`, `:497`; `alkitpage.cpp:430` | Not content-derived. The cast popover's roster child is also a fixed 280x340. Fine at the default size; **unverified** at the 560 px minimum height (popover cap logic does not apply). | LOW | Apply the same cap in `show()`. |
| R3-6 | Escape ordering | `llfloaterdirector.cpp:3163-3179` | Esc closes a popover first, then clears the search, then defocuses a search box, then CUT. Correct. Noted as verified. | | |

### R4 Fit: truncated labels and values

Method: section 0. Row-label limit 82 px (dial and arc rows 128 px). 1,492 labels measured; **48 exceed the limit**. The label box clips; there is no ellipsis (`alkitrow.cpp:253-263`). The reported "Include ghost c" is `sa_include_ghosts` ("Include ghost clones", 114 px against 82).

| ID | Page / widget | File:line | What is wrong | Sev | Suggested fix |
|---|---|---|---|---|---|
| R4-1 | Row labels, worst first | `alkitrow.cpp:477`, `alkitstyle.h:16` | Settings: "When an unsaved scene is found" (180 px; 98 over), "Reset 'don't ask again' prompts" (169), "Restore the selected backup" (156), "Delete the selected backup" (149), "Add me to the cast at login" (144, also on Capture > Scene `scene_add_self_at_login`). Light > Actors: "Remember between sessions" (162). Stepped: "Step texture animation" (123), "Include ghost clones" (114), "Hair, cloth & flexi" (91), "Skip camera rigs" (87). Shot: "Play slots in sequence" (121). VCam: 28 labels (122 down to 83), for example "Virtual placement note", "Reposition to my view", "Follow main exposure", "Mapping for new face", "Keep the light's gobo", "Brightness (feed EV)". Weather/Atmosphere/Operator/Takes: 5 more by 1-7 px. By page: VCam 28, Settings 6, Stepped 6, Weather 2, 6 pages with 1. | MED | Widen the label column to the longest label in the card (capped), wrap to two lines, or use `use_ellipses` plus a tooltip. The measured-label gate (section 4, C7) should fail the build. |
| R4-2 | Page-bar master labels | `alkitpage.cpp:513` (72 px box), `:1193` (150 px master box) | "Stepped animation" 102 px, "Light response" 80 px in a 72 px box; the switch sits at x=80, so the clipped text meets the switch. Other masters fit (≤60 px). | MED | Size the box from the text or shorten ("Stepped", "Light resp."). |
| R4-3 | Section chips | `alkitpage.cpp:1092`, `:1154` (100 px chips) | "Rate & performance 5" (VCam, about 128 px), "Everyday controls" (Diopter, about 106), "Mask & silhouette" (Actors, about 107), "Sketch & pattern" (Roto Ink, about 100), "Trails & subject" (Tron, about 93), "Haze & beams" (Atmosphere, about 87). (The VCam section labels carry an authored number prefix, "1 Source" to "5 Rate & performance", which makes that chip the widest.) | MED | Size chips to text, scroll the strip, or shorten. |
| R4-4 | Arm popover row label | `floater_director_v2.xml:152` (kit row "Recorder capture"), `alkitrow.cpp:477` | "Recorder capture" is 94 px against 82 ("Recorder play" 75 fits). | MED | Same fix as R4-1; or relabel "Capture". |
| R4-5 | Cast roster name column | `floater_director_v2.xml:19` | The "Actor" column is 72 px. Display names and the group suffix clip to about 11 characters. | MED | Make the column flex-width. |
| R4-6 | Card header presets | `alkitcard.cpp:800-806` (160 px each), `:1162` | Camera > Takes "Flycam flight" has two header presets (Drone preset, and the advanced Weight preset, which is forced visible). With the reset icon, 362 px of header controls sit in a 340 px card, so the second preset is placed at x=-48 and overlaps the title. **Unverified at runtime**; the card may be stretched wider. | MED | Cap header presets at one per card, move the second into the body, or make the preset width flexible. |
| R4-7 | Card title | `alkitcard.cpp:1162` (title width = card width/2 - 12 = 158 px) | VCam "Showing the raw camera output" is 194 px at Kit13 bold; the others fit. | LOW | Shorten or widen the title area when there are no header controls. |
| R4-8 | Tab labels with pips | `alkitshell.cpp:626-658`, `:787-789` | Tab width is measured from the label only (`+24`, clamped 56-112), but the pips append "  •" and "  ○". With both pips, Polish, VCam, Rim, Actors, Stepped and Weather exceed the button by up to 12 px. | LOW | Measure with the pip suffixes. |
| R4-9 | Page-bar Who hint | `alkitpage.cpp:1175` | The hint "Follows the cast member you click..." (about 470 px) is cut to whatever is left. At a 450 px page width it is cut to about 200 px. | LOW | Ellipsize with a tooltip, or drop at narrow widths. |
| R4-10 | Combo items | `panel_dpage_look_tron.xml` (`tr_grid_water_mode_combo`, 198 px), VCam `vc_capture_budget` (182), Settings `settings_recovery_mode` (172), Diopter `diopter_preset` (155), Filters `gradnd_preset_combo` (154) | Longest item exceeds the roughly 152 px usable in a 340 px card (control span 180). 143 combos checked; 5 overflow. | LOW | Shorten the items. |
| R4-11 | Rail without labels | `alkitshell.cpp:534-562`, `:640-646` | Below a 980 px shell width the rail shows icons only. The rail buttons have no tooltip and Quick uses the Camera icon (`floater_director_v2.xml:5`, `KitIcon_rail_camera_16`), so two icons look identical and none can be identified. | MED | Set `setToolTip(label)` on every rail button; add a Quick icon. |

Not truncated (checked): help-column titles (4 of 1,497 rows are within 28 px of the 204 px limit), help kicker (max 164 px), toast and status strip (use ellipses), transport button labels.

### R5 No raw names in user-visible text

| ID | Where | File:line | What is wrong | Sev | Suggested fix |
|---|---|---|---|---|---|
| R5-1 | Help column | `alkithelp.cpp:132-133` | Every setting row appends `"Setting " + meta.setting` (for example "Setting CineLightRigMasterEV"). The widget-name line was removed (comment at `:134-135`), but the setting name remains. Affects 1,081 rows. | HIGH | Delete the line. Developers get the setting name from the tooltip on the row's reset button or a debug flag. |
| R5-2 | Search results | `alkitshell.cpp:1540` | For Director hits the second line of each result is `searchDisplayText(hit.setting)`, the raw setting name. | MED | Show the card path (already the group header) or the help's first clause. |
| R5-3 | Search results, external rows | `director_kit_index.xml` (external rows) | 1,405 of 2,197 external rows fall back to the widget name as label; 513 read like "fx reset BDMergeFroxelWindAzimuth", "tr reset TronGridWidth", "rst CineDiopterRingPhase". COVERAGE G9. | MED | Generator: derive labels from the adjacent text or tooltip, skip reset buttons (A7). |
| R5-4 | VCam Surface Lens zoom help | `panel_dpage_camera_vcam.xml:236` | "...(PrismLensZoom)." The only raw setting name in the authored kit help. | LOW | Remove the parenthetical. |
| R5-5 | Legacy-panel tooltips | `panel_exposure_hub.xml` (`exp_hdr_bloom`: "RenderBloomHDR"), `panel_ghost_studio.xml` (`rig_diagnostics_label`: "[GhostPanelPerf]"), `panel_lens_gaze.xml` (`gaze_limit_mode_combo`: "DirectorGazeCustomLimitsEnabled") | Three tooltips quote setting names. | LOW | Reword. |
| R5-6 | Path label fallback and boilerplate | `alkitshell.cpp:1198-1216` (`return key`), `alkithelp.cpp:187` | `pagePathLabel` falls back to the raw page key when a tab is missing. Every help panel also ends with "Right-click a cast member for subject and mark actions." even on Settings rows. | LOW | Return the leaf label; show the boilerplate only where it applies. |
| R5-7 | Search entry text | `director_kit_index.xml:522` | Help text "...preview owned by WP8." (internal work-package name) on a phantom entry. | LOW | Goes away with R2-3. |

### R6 Value wiring

Static check over all 1,081 setting-bound kit rows (`rows_setting`): setting exists in `settings*.xml` (0 missing); kind versus declared type; default value inside the row's min/max (0 outside); row min/max equal to the embedded widget's (0 differ); combo item values numeric for S32/U32/F32 settings (0 non-numeric) and default present in the items (0 absent); decimals on integer settings (0).

Exceptions: `RenderVignetteCenter` is a `vec2` row on a Vector3 setting ("z unused" per the setting comment; `ALKitVecBinding::commit` preserves z); `cine_seed` text on U32 (R1-14).

**Spot check of 40 rows** (seed 1015, 28 numeric, 12 other) read-back path: every one binds through `ALKitSettingBinding` (`alkitbinding.cpp:63-90`, change signal at `:87`) and a C++ reader exists, except `RenderBloomHDR` (R1-5). Sample: CinematicCamSweepSpeed [0.01,10], AlchemyWeatherSplashUpThreshold [0,0.98], AlchemyWeatherSplashMaxDistance [4,128], AlchemyWeatherLightningBoltHeight [16,512], BDMergeProjectorVolumetricsMaxLuminance [1,64], DirectorSwitcherIntervalSec [0.1,120], RenderShadowSlopeScale [0,3], RenderShadowBlurSize [0,5], CineLightRigRatio [0,5], RenderLensFlareArc [0,1], TronTrailHeight [0.1,8], TronGridWall [0,1], CineOutlineHatchAngle [0,180], CinematicCamStaticMediumHeading [-180,180] dial, AlchemyToneMapContrast [0,2], RenderColorGradeBrightness [-0.5,0.5], RenderChromaticAberrationAnisotropy [-1,1], CinematicAutoFrameSettleFeather [0,1], plus combos CinematicCamOTSSide, CineLightRigLiveProbeRefresh, CineOutlineMotionStep, CinematicCamMotionStartMode, CineRigRimTronColorSource, TronSubjectTargetSet, and toggles CineLightRigLiveProbeGizmo, CineDiopterEnabled, TronRimRejectFloors, CineRigRimDebugRimOnly, StepAnimWhatSteps (segmented), RenderVignetteMidColor (colour). For 31 of 35 inspected settings the consumer reads the raw value with no clamp, so the row range is the only guard. A clamp in the shader or a derived uniform is **unverified**.

| ID | Where | File:line | What is wrong | Sev | Suggested fix |
|---|---|---|---|---|---|
| R6-1 | Row context menu "Paste value", card paste | `alkitrow.cpp:820-836`, `alkitqol.h:57-66`, `alkitbinding.cpp:12-58` | Paste coerces type only. A value copied from a wider row (for example Max luminance 64) is written to a narrower setting (0..1) with no clamp to `min_val`/`max_val`. Consumers read it raw. | MED | Clamp to the row's min/max in `setBoundValue`. |
| R6-2 | Quick proxies | `alkitrow.cpp:895-902` | A proxy for a row without `has_min`/`has_max` gets ±1,000,000 limits. | LOW | Copy source limits only; refuse proxies for unbounded numerics, or use the setting's own bounds. |
| R6-3 | `RenderBloomHDR`, `TemporalOutputFPS` | see R1-5, R1-6 | Controls with no reader. | MED/LOW | See R1. |
| R6-4 | `cine_seed` U32 | see R1-14 | | LOW | |
| R6-5 | Consumer-side clamps | `pipeline.cpp`, `altron.cpp`, `alrotoink.cpp` | Row ranges were authored from mockups; no automated check ties a row range to a consumer clamp or shader limit. | LOW | Add a table of consumer ranges to the gate (section 4, C9). |

### R7 Help and hotkey text versus actual handlers

| ID | Text | Handler | What is wrong | Sev | Suggested fix |
|---|---|---|---|---|---|
| R7-1 | Hotkeys popover (`floater_director_v2.xml:187-199`) | `aldirectorhotkeys.cpp:102-153` | The list omits real keys: F2 (toggle console), F3 (ACTION/CUT), F4 (CUT), F6 (pose ghosts on/off), F7 (take play/pause). It lists F5 and F8 correctly. | MED | Add the five keys. |
| R7-2 | "1-9: camera slots / Alt+1-9: animation slots" | `aldirectorhotkeys.cpp:44-96` | Both act only when the matching switcher is armed (`DirectorSwitcherArmed`, `DirectorAnimSwitcherArmed`), the console is open and nothing has keyboard focus. The text does not say so. | LOW | "(when armed)". |
| R7-3 | Legend "L column ... (Camera > Shot)" | `floater_director_v2.xml:171`; the flag UI is Performance > Gaze ("Look at camera (global master)", `panel_lens_gaze.xml:27`) and the Actor Gaze floater | Points at the wrong page. A/B subjects are correctly on Camera > Shot. | MED | "Performance > Gaze". |
| R7-4 | ACTION tooltip "Nothing armed: check at least one 'Action fires' box" | `llfloaterdirector.cpp:3366` | The arm popover has no "Action fires" heading; the rows are "Actor moves / Camera / Recorder play / Recorder capture". | LOW | Reword to "Open the arrow beside ACTION and turn something on". |
| R7-5 | Scene popover button labels vs enable state | `llfloaterdirector.cpp:3488-3492` | Rename and Delete grey out with no scene selected (correct); Save as is always enabled. Tooltips on the popover buttons are absent. | LOW | Add tooltips. |
| R7-6 | `?`/`F1` toggles Hotkeys but the legend button label is "Hotkeys (F1 / ?)" | `llfloaterdirector.cpp:3180-3184`, `:3231-3235` | Consistent; noted as verified. | | |

### R8 Searchable: status of COVERAGE_AUDIT gaps (not redone)

| Gap | Status in the current tree |
|---|---|
| G1 legacy controls unindexed (128) | Open. No generator change; index still built from mockup rows for legacy pages. |
| G2 dead rows on LINK pages | Open, 27 entries confirmed (R2-3). |
| G3 card master switches not indexed | **Unverified** (not re-checked). |
| G4 page/rail masters not indexed | Partly closed: `AlchemyWeatherEnabled`, `PrismLensEnabled`, `CineLightRigEnabled`, `CineRigRimEnabled`, `AvatarLightResponseEnabled`, `ALPolishEnabled` each have one row; `StepAnimEnabled` has none. |
| G5 shell controls not indexed | Open: `DirectorArmMoves`, `DirectorActionDelay`, `ActorMoverShowHeading` have no rows. |
| G6 page/group names not searchable | Not re-checked in code (`alkitsearch.cpp:145-153` unchanged). |
| G7 combo items not indexed | Open (`INDEX_FIELDS` still 22, `alkitsearch.cpp:17`). |
| G8 launcher discovery misses floaters | Open: no `actor_gaze` or `env_adjust_snapshot` among the 55 external targets. |
| G9 external-row noise | Open and slightly worse: 1,405 widget-name labels (audit said 1,392). |
| G10 thin synonyms | Not re-checked. |
| G11 legacy jump does not switch inner tabs | Open (R2-4). |
| G12 missing / settings-only features | Not re-checked. |

## 3. Prioritised fix list

**Batch 1: dead controls and broken navigation (small, mechanical, highest value)**
1. R1-1: fix `navigateTo(std::string)` parsing; unit test with every `link_to` string; confirm Settings > "Add me to the cast at login" lands on Capture > Scene.
2. R1-2: legacy pages get no Reset page and no Changed only (or a real implementation over `watch_settings`).
3. R3-2: relayout the transport bar when swapping `scene_combo` and `scene_name_editor`; add a placement assertion.
4. R3-1: anchor click closes its own popover.
5. R1-3: split the `refreshPrismDependents` guard.
6. R1-4: scene Save/Load carry the Shot preset in v2.
7. R1-5 / R1-6: wire or remove `RenderBloomHDR` and `TemporalOutputFPS`.

**Batch 2: reachability at small sizes**
8. R2-2: scene file controls reachable at 850 px; reorder or relocate.
9. R2-5, R2-6, R4-11: Group box, hidden search editor, rail tooltips and a Quick icon.

**Batch 3: text**
10. R5-1 to R5-3, R5-4 to R5-7: remove "Setting <name>", raw detail lines, widget-name labels, raw tooltips.
11. R4-1 to R4-10: label column policy (measured width, wrap or ellipsis plus tooltip), master box, section chips, roster column, card header presets, tab pips.
12. R7-1 to R7-5: hotkey list and wording.

**Batch 4: search and values**
13. R2-3 / R2-4 / G8: index regeneration (the audit's batch A).
14. R6-1: clamp paste to the row range.
15. R1-7 to R1-14: cleanup.

Each batch ends with the layout gate, the new connectedness gate and the click-every-control functional run, then a single fresh build per the repo rules.

## 4. What the planned static connectedness gate must check

Each class below is a finding from this audit that would have failed at code time. Wire them into `kit_lint.py` (XUI side) and a new `kit_cpp_scan.py` (C++ side), plus C++ unit tests where a pure function can be extracted.

| Check | Fails on | Catches |
|---|---|---|
| **C1 Control has a sink.** Every interactive tag in the Director tree (`floater_director_v2.xml`, all `panel_dpage_*`, every `filename=` include, popovers, bar) must have `control_name`, an XML `commit_callback` naming a registered function, a `kit_row` with a valid `setting`/`action`/`link_to`, or a name found in a generated handler table (a scan of `setCommitCallback`/binder `findChild("name")` calls, extended with `cpp_name_patterns.json` for dynamic names). | A new unbound control | R1-3, R1-8, R1-13 class |
| **C2 Binder coverage.** Every page with `bind="code"` non-readout rows or code-set controls must have a `bindings[]` entry in `bindPage` or a registered root `class=`. Every `findChild/getChild("x")` inside a binder must name a widget present in that page's XML (or its includes). | `prism_summary` / `btn_prism_manage` (R1-3); `cinecam_params_embedded` (R1-4) | R1-3, R1-4 |
| **C3 Early-return sets.** For member-pointer guards of the form `if (!mA || !mB ...) return;` in refresh functions, every named member's widget must exist in some v2 page. | R1-3 | R1-3 |
| **C4 Navigation strings.** Every `link_to` (row and card), `navigateTo` literal and favourite key must parse as `group/page[/widget]`, the page must exist, the widget must exist on it. A C++ unit test calls the real parser on every `link_to`. | R1-1 | R1-1, R2-1 |
| **C5 Reachability.** Every lazy page has a group in the rail or the gear path; every card is in a section; every popover's `anchor` exists, is not in the bar's `collapse_order` unless an alternate route is declared (`sole_path.json`), and every popover button has a handler. | R2 class | R2-2 |
| **C6 Bar width budget.** Simulate `ALKitBar::layoutChildren` from the XML widths at 850, 933, 1000 and 1160 px, with and without `status_strip` and `transport_toast`. Every function without an alternate path must remain visible. Also assert that swapping children (combo/editor) goes through the bar API. | R2-2, R3-2 | R2-2, R3-2 |
| **C7 Measured fit.** Implement the "measured label-width" rule that `kit_lint.py:32` lists as not done, using the repo font and the constants in `alkitstyle.h`: row label ≤ `LABEL_W-18` (dial rows ≤ cell width), master label ≤ box width, section label plus count ≤ chip width, card title ≤ `w/2-12` and header presets ≤ 1, tab label plus both pips ≤ button width, combo longest item ≤ span minus 28, popover row labels, cast-list column minimums. Exceptions only through `kit_lint_allow.json` with a reason. | R4 class | R4-1 to R4-10 |
| **C8 No raw names.** (a) Authored `help`, `tool_tip`, `label`, `title`, `summary`, `<text>` content must not contain a name found in `settings*.xml`, a camelCase token of 3 or more humps, a `snake_case` token, or `WPn`. (b) The C++ scan must find no `+ meta.setting`, `hit.setting`, `meta.widget` or `page_key` passed to a text sink in `alkithelp.cpp` or `alkitshell.cpp` result rows. (c) Index rows: no label equals or contains its widget name; no label matches `(fx|rst|tr|ps|exp) (reset )?<Setting>`. | R5 class | R5-1 to R5-7 |
| **C9 Setting has a reader and a range.** Every setting bound by a Director control must have at least one reader in C++ or GLSL outside the UI binders (`RenderBloomHDR`, `TemporalOutputFPS` fail today). For numeric rows, the default must be inside the row range, the widget range must equal the row range, and the row range must lie inside a consumer range recorded in `consumer_ranges.json` (seeded from the 40-row sample). | R1-5, R1-6, R6 class | R1-5, R1-6, R6-5 |
| **C10 Paste/proxy clamp.** Unit test: `setBoundValue` with an out-of-range value must clamp to `min_val`/`max_val`. | R6-1 | R6-1 |
| **C11 Hotkey parity.** Parse the keys in `aldirectorhotkeys.cpp` and `LLFloaterDirector::handleKeyHere/handleUnicodeCharHere` and diff against `kit_hotkeys_text`. Both directions must be empty. Also diff legend and tooltip phrases against page labels ("Camera > Shot" must name a page that holds the control). | R7 class | R7-1, R7-3, R7-4 |
| **C12 Legacy chrome.** For `legacy="true"` pages the generator must emit `chrome="legacy"`; `ALKitPage` must not build Reset page or Changed only unless `mCards` is non-empty. A runtime self-test clicks Reset on every legacy page and asserts a non-zero reset count or a hidden button. | R1-2 | R1-2 |
| **C13 Phantom index rows.** Every non-external index entry must resolve to a named node in its page tree (COVERAGE gate G-dead). 27 fail today. | R2-3 | R2-3, R5-7 |
| **C14 Popover self-test (runtime, in `ui_selftest`).** For each popover: show, assert height ≥ content height (up to the cap), it is the front child, a click on its anchor closes it and it stays closed, Esc closes it, and each child resolves by a recursive lookup. | R3 class | R3-1, R3-5 |
| **C15 Adversarial fixtures.** The reviewer plants: an unbound button in a popover, a `link_to` with 2 and 3 parts, a binder that looks up a widget the page lacks, a label 90 px wide, a help string with a setting name, a hotkey in code but not in the text, and a setting with no reader. Each must fail its check (per the Codex relaxes-gates rule). | all | all |

## 5. Unverified items

- R3-1 (anchor re-open) and R3-2 (scene-name editor placement) are derived from event order and layout code, not from running the viewer. Both would be visible in one minute in-world.
- R4-6 (Flycam flight header overlap) depends on the card width the flow grid actually assigns.
- R1-12: the VCam page's 110 routed controls were spot-checked at 10, not exhaustively.
- Embedded legacy panels (Ghost Studio, Dance Ball, Path, Props, Exposure hub, Switchboard) were matched by name against C++ and `control_name`; their own binders were not read.
- Consumer clamps for shader-side values (R6).
- Plex Sans width estimates are ±5%; LLTextBox padding and the user's UI scale factor are not modelled.
- The other agent's label-sizing change may already fix part of R4-1 to R4-4.

## 6. Reference: phantom index entries (R2-3)

27 non-external entries with no matching widget on the page. Practicals: `sl_status`, `sl_cast_shadows`, `sl_volumetric`, `sl_capture`, `sl_clear`, `sl_gobo`, `sl_gobo_animmode`, `sl_gobo_speed`, `sl_gobo_zoom`, `sl_gobo_tint`, `sl_gobo_soft`, `sl_gobo_dispersion`, `sl_gobo_var1`, `sl_gobo_var2`, `sl_gobo_invert`, `open_lightbox_sellight`. Gaze Cues: `cue_list`, `btn_cue_add`, `btn_cue_delete`, `gaze_cue_play`, `gaze_cue_stop`. Light Cues: `cue_rows`, `cue_capture`, `cue_delete`, `cue_goto`, `cue_release`. VCam: `vc_monitor_tile_manifest`. The matching floater rows already exist as external entries (for example `sl_cast_shadows` at index line 2989, `cue_goto` at line 3663), so the Director rows can be dropped.

## 7. Needs a user decision (wire or remove)

Both rows are kept, now disabled with the tooltip "Not connected yet" and honest help text (fix batch 10/15, item B9). Neither has a consumer in C++ or GLSL, so neither can be wired without new renderer or readout work.

| Setting | Where | State now | Decision needed |
|---|---|---|---|
| `RenderBloomHDR` | Look > Bloom & Glow, "HDR bloom" (`set_RenderBloomHDR`); also the Exposure hub bloom controls | Row disabled, tooltip "Not connected yet". No reader exists, so bloom does not change with it. | Wire it in `pipeline.cpp` (HDR bloom branch) or remove the row and the `enabled_control` gating in the Exposure hub. |
| `TemporalOutputFPS` | Capture > Time, "Out FPS" (`output_fps_spinner`) | Row disabled, tooltip "Not connected yet". `alpaneltemporalcapture.cpp` computes the post speed-up from the speed alone. | Use it in the speed-up readout or remove the row. |
