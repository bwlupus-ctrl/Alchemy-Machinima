# Run 6 in-world functional triage

Source: `run_functional_20261012_153529/functional.csv` at `cc1cf57fa9b` (1174 PASS / 29 FAIL / 87 KNOWN_GAP).

## PRODUCT BUGS

- `btn_node_setcam` — **PRODUCT**. Camera capture changed the waypoint but did not snapshot the prior authored path, so Undo could not restore it. The handler now snapshots before `setNodeCamera` (`alpanelpatheditor.cpp:1829-1852`).
- `node_cam_transition_combo` — **PRODUCT**. Transition changes likewise bypassed path history. The handler now snapshots before `setNodeCamTransition` (`alpanelpatheditor.cpp:1868-1876`). The adjacent clear-camera path was corrected at the same time (`alpanelpatheditor.cpp:1855-1866`).

## HARNESS AND SAFETY BUGS

| Failed row | Diagnosis and evidence |
|---|---|
| `btn_wp_add` | **HARNESS** — handler appends a real waypoint (`alpanelpatheditor.cpp:1178-1193`); strict node-count oracle is at `aluiperfselftest.cpp:5913-5921`. |
| `btn_wp_insert` | **HARNESS** — the fixture now selects node 2 through `waypoint_list::onCommit` before reachability (`aluiperfselftest.cpp:5030-5046`); handler inserts/appends at `alpanelpatheditor.cpp:1196-1225`. |
| `btn_edit_undo` | **HARNESS** — deterministic pre-edit/expected state is prepared at `aluiperfselftest.cpp:2384-2413`; path equality plus undo/redo depth is asserted at `aluiperfselftest.cpp:5922-5929`. Product swap is `llactormover.cpp:3514-3530`. |
| `btn_edit_redo` | **HARNESS** — same deterministic history fixture and exact expected-path/depth oracle (`aluiperfselftest.cpp:2384-2413,5926-5929`); product swap is `llactormover.cpp:3533-3550`. |
| `btn_edit_reverse` | **HARNESS** — product reverses the waypoint vector (`llactormover.cpp:3553-3581`); oracle requires every post node to equal the corresponding reversed pre node (`aluiperfselftest.cpp:5930-5938`). |
| `btn_edit_mirror` | **HARNESS** — the old two-node straight fixture was invariant under reflection. The asymmetric three-node fixture is at `aluiperfselftest.cpp:2395-2401`; the oracle independently recomputes every reflected point at `aluiperfselftest.cpp:5939-5971`. Product reflection is `llactormover.cpp:3584-3664`. |
| `btn_edit_loopclose` | **HARNESS** — handler/engine close the loop (`alpanelpatheditor.cpp:1391-1400`, `llactormover.cpp:3667-3708`); oracle requires changed path, loop mode, and equal seam endpoints (`aluiperfselftest.cpp:5972-5978`). |
| `btn_play_path` | **HARNESS** — handler starts through `LLActorMover` (`alpanelpatheditor.cpp:1418-1428`); oracle requires `walking false -> true` (`aluiperfselftest.cpp:5979-5980`). |
| `btn_stop_path` | **HARNESS** — fixture starts the local path and handler stops it (`aluiperfselftest.cpp:2471`, `alpanelpatheditor.cpp:1430-1438`); oracle requires `walking true -> false` (`aluiperfselftest.cpp:5981-5982`). |
| `btn_walk_here` | **HARNESS** — the click arms the one-shot path tool (`alpanelpatheditor.cpp:1440-1454`); public tool state is exposed at `altoolpathedit.h:69` and oracle requires `armed false -> true` (`aluiperfselftest.cpp:5983-5985`). |
| `btn_copy_to` | **HARNESS** — handler deep-copies to the selected cast destination (`alpanelpatheditor.cpp:1402-1416`, `llactormover.cpp:3711-3761`); oracle requires destination post-path == source and destination pre != post (`aluiperfselftest.cpp:5986-5988`). |
| `node_height_spinner` | **HARNESS** — panel selection fixture is `aluiperfselftest.cpp:5030-5046`; handler writes ground offset (`alpanelpatheditor.cpp:1906-1914`) and the node-field oracle is `aluiperfselftest.cpp:6041-6053`. |
| `node_dwell_spinner` | **HARNESS** — same production selection fixture; handler writes dwell (`alpanelpatheditor.cpp:1916-1924`) and exact field/control equality is asserted at `aluiperfselftest.cpp:6041-6053`. |
| `node_speed_spinner` | **HARNESS** — same production selection fixture; handler writes speed override (`alpanelpatheditor.cpp:1926-1934`) and exact field/control equality is asserted at `aluiperfselftest.cpp:6041-6053`. |
| `node_corner_radius_spinner` | **HARNESS** — the asymmetric interior node makes a valid corner and is selected through the panel (`aluiperfselftest.cpp:2395-2401,5030-5046`); handler/field oracle are `alpanelpatheditor.cpp:1936-1973` and `aluiperfselftest.cpp:6041-6053`. |
| `btn_node_setcam` | **PRODUCT**, fixed above; the harness also requires the node camera flag plus captured render-camera position/FOV (`aluiperfselftest.cpp:6017-6031`). |
| `node_cam_transition_combo` | **PRODUCT**, fixed above; fixture authors a node camera (`aluiperfselftest.cpp:2415-2418`) and oracle requires the authored transition to equal the combo (`aluiperfselftest.cpp:6032-6036`). |
| `btn_node_preview` | **HARNESS** — fixture authors/selects the shot (`aluiperfselftest.cpp:2415-2418,5030-5046`); handler toggles `LLPathCamera` (`alpanelpatheditor.cpp:1879-1901`) and oracle requires preview `false -> true` (`aluiperfselftest.cpp:6037-6039`). |
| `path_groundfollow_check` | **HARNESS** — handler writes `Path::mGroundFollow` (`alpanelpatheditor.cpp:2057-2064`); oracle requires pre != post and post == control (`aluiperfselftest.cpp:5989-5993`). |
| `path_pitch_check` | **HARNESS** — handler writes `Path::mPitchToSlope` (`alpanelpatheditor.cpp:2066-2073`); oracle requires pre != post and post == control (`aluiperfselftest.cpp:5994-5998`). |
| `btn_primitive_center` | **HARNESS** — product centers on the avatar foot (`alpanelpatheditor.cpp:1457-1471`, `llactormover.cpp:2309-2335`); oracle independently computes and matches that global point (`aluiperfselftest.cpp:5999-6011`). |
| `btn_fit_half_arc` | **HARNESS** — product fits and snapshots an exact circle arc (`alpanelpatheditor.cpp:2153-2166`, `llactormover.cpp:2262-2306`); oracle requires changed state and circle-arc shape (`aluiperfselftest.cpp:6012-6014`). |
| `btn_fit_sine` | **HARNESS** — same engine fit path for sine (`alpanelpatheditor.cpp:2168-2183`, `llactormover.cpp:2262-2306`); oracle requires changed state and sine shape (`aluiperfselftest.cpp:6015-6017`). |
| `btn_stop_following` | **HARNESS** — fixture establishes a follower and handler clears it (`aluiperfselftest.cpp:2445-2447`, `alpanelpatheditor.cpp:1818-1825`); oracle requires `following true -> false` (`aluiperfselftest.cpp:6018-6020`). |
| `db_add_selected` | **SAFETY/HARNESS** — handler necessarily reads `LLSelectMgr` (`alpaneldanceball.cpp:1181-1205`), so it is excluded in-world in both lists (`functional_report.py:39-44`, `aluiperfselftest.cpp:5548-5565`). No selection fixture is created. |
| `db_t_speed` | **HARNESS** — fixture aligns the owner pre-state with the displayed slider before the real drag (`aluiperfselftest.cpp:2680-2685`); oracle requires changed `ALDanceBall::Timing::mSpeed` == control (`aluiperfselftest.cpp:6061-6083`). Product commit is `alpaneldanceball.cpp:1907-1922`. |
| `db_ramp_go` | **HARNESS** — fixture supplies a different target and a positive 30 s duration (`aluiperfselftest.cpp:2686-2695`); oracle requires revision movement and an active ramp (`aluiperfselftest.cpp:6061-6112`). Product commit is `alpaneldanceball.cpp:1946-1952`. |
| `gaze_cast_combo` | **HARNESS** — one eligible member gave the combo no alternate. Two temporary viewer-local cast members are now added/restored (`aluiperfselftest.cpp:2803-2809,3432-3436`); production refresh lists them at `alpanellensgaze.cpp:1048-1090`. |
| `cine_fill_ev` | **HARNESS** — merged-manifest enrichment now drives Easy Mode off and Ratio Lock off, in addition to the rig/fill masters (`functional_report.py:1041-1071`). All settings are restored by the existing per-row snapshot. |

## Cheapest KNOWN_GAP oracles (do not chase in Run 6)

1. Reuse the new serialized path oracle for `path_cadence_slider`, `path_tension_slider`, `path_airborne_check`, `path_end_combo`, the primitive angle controls, and then the currently UNWIRED path scalar/shape/follow rows.
2. Extend `dance_owner` by one field for `db_t_crossfade`; the engine already exposes `timing().mCrossfadeSeconds` (`aldanceball.h:72-79`).
3. Reuse the prop-path scene snapshot for `path_speed_spin`, `end_mode_combo`, and `rotation_mode_combo`; these are already category-A commits with a local fixture.
4. Snapshot the edited `LLDirectorCast::GazeTarget` for the category-A gaze scalar/combo rows; the Run 5 gaze fixture already supplies the self actor.
5. Snapshot selected rig-light aim for the four `cine_aim_*` nudges and switcher slot state for `switcher_slot_mode` / `switcher_primary_subject`.

## Safety, allow-list, and validation

- No `LLSelectMgr` selection, Animation Explorer use, agent/object/chat send, or unqualified avatar motion call was added. Self path playback remains inside the existing `LLActorMover` qualified `LLCharacter` calls.
- No row became newly executable relative to Run 6; `functional/INWORLD_NEW_ROWS.md` therefore remains unchanged and fail-closed.
- Manifest scanner self-test passes. The enriched candidate reports 0 unresolved owner gates. `ui_draw_lint` is run separately; no build was run.

## Changed files

- `indra/newview/alpanelpatheditor.cpp`
- `indra/newview/altoolpathedit.h`
- `indra/newview/aluiperfselftest.cpp`
- `indra/newview/llactormover.cpp`
- `indra/newview/llactormover.h`
- `scripts/perf/functional_report.py`
- `doc/director_kit/RUN_INWORLD6_TRIAGE.md`
