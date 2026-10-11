# Director suspects resolved (V2_SUSPECT_FLOATER_SCOPED)

Read-only static pass over `indra/` source for the 104 rows whose `v2_status` is `V2_SUSPECT_FLOATER_SCOPED` in `doc/director_kit/wiring/wiring_table.csv`. Per-row results are in `doc/director_kit/wiring/suspects_resolved.csv`. No source was modified and nothing was built or run.

## Result

**All 104 suspects are WIRED_IN_V2. UNWIRED: 0. UNKNOWN: 0.** Every control is covered by one of two v2 mechanisms:

| Mechanism in v2 | Rows | Where it is wired |
|---|---|---|
| `bindPage` binder `bindMovePage` (move_tab) | 8 | `llfloaterdirector.cpp:953` table, binder `:985-1007`, all-or-nothing guard `:995-996` |
| `bindPage` binder `bindActorStylePage` (actor_style_tab) | 17 | table `:955`, binder `:1018-1090`, guard `:1038-1043` |
| `bindPage` binder `bindAnimatePage` (animate_tab) | 16 | table `:956`, binder `:1092-1165`, guard `:1112-1115` |
| `bindPage` binder `bindCameraPage` (shot, moves, vcam, gaze, dance, polish, exposure) | 17 | table `:957-964`, binder `:1167-1275`, per-widget `findChild`, no guard |
| Injected owner `ALPanelCineCamParams` (operator_tab) | 1 | `panel_dpage_camera_operator.xml:10` class, `alpanelcinecamparams.cpp:428-433` |
| Injected owner `ALPanelCineLightRig` (cine_light_rig_tab, kit page) | 45 | `panel_dpage_light_cine_light_rig.xml:3` class, `alpanelcinelightrig.cpp:640-837` |

Pages not in the `bindPage` table (operator_tab, cine_light_rig_tab) do not need one: their owners are injected and run their own `postBuild`.

## Counts per page and verdict

| page | WIRED_IN_V2 | UNWIRED | UNKNOWN | total |
|---|---|---|---|---|
| cine_light_rig_tab | 45 | 0 | 0 | 45 |
| actor_style_tab | 17 | 0 | 0 | 17 |
| animate_tab | 16 | 0 | 0 | 16 |
| shot_tab | 8 | 0 | 0 | 8 |
| move_tab | 8 | 0 | 0 | 8 |
| moves_tab | 3 | 0 | 0 | 3 |
| vcam_tab | 2 | 0 | 0 | 2 |
| exposure_tab | 1 | 0 | 0 | 1 |
| dance_tab | 1 | 0 | 0 | 1 |
| gaze_tab | 1 | 0 | 0 | 1 |
| operator_tab | 1 | 0 | 0 | 1 |
| polish_tab | 1 | 0 | 0 | 1 |
| **total** | **104** | **0** | **0** | **104** |

Rows that are readouts or pointer-only (no commit callback) are still WIRED_IN_V2, because the binder caches the pointer that the draw-time refresh uses: `anim_hint`, `prism_summary`, `actor_style_status`, `cine_object_target_status`, `cine_gobo_softness`, `cine_live_probe_live_status`. `group_run_combo` and `paste_uuid_editor` are read on action rather than on commit.

## UNWIRED list and fix groups

None. The two example fixes in the brief are already in place:
- "add actor_style to the bindPage table": already at `llfloaterdirector.cpp:955`.
- "embed panel_cinecam_params in operator": already at `panel_dpage_camera_operator.xml:10`.

## Corrections to the wiring table

- The v2 evidence in `wiring_table.csv` for the floater-scoped rows cites v1 `postBuild` lines (for example `llfloaterdirector.cpp:588` for shot_tab), which never run in v2. `suspects_resolved.csv` cites the v2 binder or injected owner instead.
- The table has 104 suspect rows, not 96. Per-page counts differ from the brief: cine_light_rig_tab 45 (brief 42), actor_style_tab 17 (16), animate_tab 16 (14), move_tab 8 (7).
- Kit rows for the rig are all `bind="code"` with no `control_name` on the widget (verified for all 45). Their values flow only through the `ALPanelCineLightRig` callbacks. The `setting=` on a kit row is metadata for reset and history, except `cine_fixture_role`, which has `mirror_setting="DirectorV2SelectedLight"` (`panel_dpage_light_cine_light_rig.xml`, kit_row for `cine_fixture_role`).

## Residual checks (do not change any verdict)

1. Rig end-to-end: open the Rig page and commit `cine_easy_brightness`. Confirm `CineLightRigMasterEV` changes. This confirms the root-class `postBuild` path at runtime.
2. All-or-nothing guards: `bindMovePage` (`:995-996`), `bindAnimatePage` (`:1112-1115`) and `bindActorStylePage` (`:1038-1043`) silently null every pointer on one missing or mistyped widget, with no log. The XML names and types are correct today; a functional gate per D5 should assert them.
3. `bindPage` runs only on first reveal (`alkitlazypage.cpp:38-48`). Functional tests must open each page before commit tests.

## Observed outside the suspect set

- `ALPanelCineLightRig::postBuild` looks up `cine_group_enable` (`alpanelcinelightrig.cpp:646`, callback `:753-754`) and `cine_easy_mode` (`:669`, callback `:804-805`). Neither name exists in `panel_dpage_light_cine_light_rig.xml`, and neither appears in `wiring_table.csv`. The Easy mode is likely covered by the kit mode switch (`mode_setting="CineLightRigEasyMode"`). The group master is not obviously replaced; check it at runtime.
