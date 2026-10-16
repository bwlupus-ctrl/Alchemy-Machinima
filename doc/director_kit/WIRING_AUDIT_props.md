# Wiring audit: Cast > Props kit conversion

Static audit of the Props page after it became a generated kit page (10/15, legacy conversion batch 2). No build, no commit, no viewer run: every row is checked against the generated XML and the C++ source. Sources: mockup page `props_tab` (8 cards, 33 rows; only search keywords `kw` were added, no row added or removed), `kit_pages.json` entry `cast_group/props_tab` (hosts the v1 nodes of the shared `panel_prop_mover.xml`, root class `panel_prop_mover`), generated `panel_dpage_cast_props.xml`.

## Result

- The page is no longer legacy: classification `kit (P0)`, pin 5 -> 4, `cast_group/props_tab` removed from `KNOWN_LEGACY_PAGES`.
- 33 generated rows = the 33 mockup rows, 8 cards, 2 sections (Path, Motion). The Hover, Rock and Spin cards carry the enable switches `hover_on_check`, `rock_on_check`, `spin_on_check` (kit card switches; `ALKitSwitch` is an `LLCheckBoxCtrl`, so the `getChild<LLCheckBoxCtrl>` lookups resolve).
- The page root is `ALPanelPropMover`, the same class the standalone Prop Mover floater embeds. The panel owns all state (the object path mover roster, `LLActorMover` paths, `ALObjectPathMover::Effects`); no setting is involved, so every control is code-bound (`bind="code"`) and the panel refreshes itself on a 0.5 s throttle. No cast selection is used (props are in-world objects).
- The v1 Director tab and the standalone floater are unchanged (`floater_director.xml`, `floater_prop_mover.xml`, `panel_prop_mover.xml` untouched).
- v1 names with no row (parity_allow): `orient_header`, `osc_header`, `spin_header`, `shuttle_header` (static headings replaced by card titles).

## Renames (kit widget names are unique across Director pages)

`row_overrides` rename the hosted widgets that collided with other kit pages or the shell, with `source_widget` pointing at the v1 node, and `child_names` for two list columns:

| v1 name | page name | collided with |
|---|---|---|
| `btn_stop` | `prop_btn_stop` | Camera > Takes |
| `end_mode_combo` | `prop_end_mode_combo` | Cast > Move |
| `btn_remove` | `prop_btn_remove` | Director shell chrome |
| `status_text` | `prop_status_text` | Capture > Time |
| `prop_list` column `name` | `prop_name` | Director shell chrome |
| `prop_list` column `state` | `prop_state` | Light > Actors column |

C++ (`alpanelpropmover.cpp`): `postBuild` looks up each of the four widgets with `findChild(v1 name)` and falls back to the `prop_` name; `refresh()` picks the column names with `getColumn("prop_name") ? "prop_name" : "name"` (and `prop_state`). The standalone floater, which has the v1 names, takes the first branch.

## Controls

All handlers are in `alpanelpropmover.cpp`. "State" means the object path mover or `LLActorMover` store; read-back is `refresh()` / `refreshNodePane()` from that store.

| # | widget | kind | handler | target | type / range | status | Undo owner |
|---|---|---|---|---|---|---|---|
| 1 | hint_text | readout | none (static text) | n/a | text | OK | n/a (action/output, no widget step) |
| 2 | btn_enroll | button | `onEnrollSelected` | `ALObjectPathMover::toggleTarget` for the selected in-world object | n/a | OK | n/a (action/output, no widget step) |
| 3 | prop_list | list (columns `prop_name`, `nodes`, `prop_state`) | `onSelectProp` -> `refreshNodePane`; rows rebuilt by `refresh()`, selection and scroll kept | roster of `ALObjectPathMover` | code-driven | OK | none (widget_history=false; subject/selection state) |
| 4 | prop_btn_remove | button | `onRemoveProp` | `toggleTarget` (drops the path) | n/a | OK | n/a (action/output, no widget step) |
| 5 | prop_status_text | readout | `setStatus` | status text | text | OK | n/a (action/output, no widget step) |
| 6 | node_list | list (columns `index`, `skid`, `speed`) | `onSelectNode` -> `refreshNodePane` | selected prop's `Path::mNodes` | code-driven | OK | none (widget_history=false; subject/selection state) |
| 7 | btn_node_add | button | `onAddNode` | `appendWaypointHere` | n/a | OK | n/a (action/output, no widget step) |
| 8 | btn_node_del | button | `onDeleteNode` | `LLActorMover::deleteWaypoint` | n/a | OK | n/a (action/output, no widget step) |
| 9 | btn_node_clear | button | `onClearNodes` | `stop` + `clearPath` | n/a | OK | n/a (action/output, no widget step) |
| 10 | node_skid_spin | spinner | `onCommitNodeSkid` -> `setNodeYawOffset` (deg to rad) | selected node | -180..180 step 5, 0 dp (v1 = mockup) | OK | none (widget_history=false; subject/selection state) |
| 11 | node_speed_spin | spinner | `onCommitNodeSpeed` -> `setNodeSpeed` | selected node | 0..50 step 0.25, 2 dp | OK | none (widget_history=false; subject/selection state) |
| 12 | btn_drive | button | `onDrive` | `ALObjectPathMover::start` | n/a | OK | n/a (action/output, no widget step) |
| 13 | prop_btn_stop | button | `onStop` | `ALObjectPathMover::stop` | n/a | OK | n/a (action/output, no widget step) |
| 14 | btn_drive_all | button | `onDriveAll` | `startAll` | n/a | OK | n/a (action/output, no widget step) |
| 15 | btn_stop_all | button | `onStopAll` | `stopAll` | n/a | OK | n/a (action/output, no widget step) |
| 16 | path_speed_spin | spinner | `onCommitPathSpeed` -> `Path::mSpeed` (clamped 0.05..50) | selected prop path | 0.05..50 step 0.25, 2 dp | OK | none (widget_history=false; subject/selection state) |
| 17 | prop_end_mode_combo | combo | `onCommitEndMode` -> `Path::mEndMode = index` | selected prop path | items by index: Stop at end 0, Loop 1, Ping-pong 2 (v1 order) | OK | none (widget_history=false; subject/selection state) |
| 18 | btn_capture_orient | button | `onCaptureOrient` -> `setNodeOrient` | selected node | n/a | OK | n/a (action/output, no widget step) |
| 19 | rotation_mode_combo | combo | `onCommitRotationMode` -> `Path::mRotationMode = index` | selected prop path | Face path 0, Authored 1, Face path + offset 2 | OK | none (widget_history=false; subject/selection state) |
| 20 | hover_on_check (Hover card switch) | toggle | `onCommitEffects` -> `fx.mHover.mOn` | `editEffects(prop)` | bool | OK | none: card enable switches are not recorded by kit widget history |
| 21 | hover_wave_combo | combo | `onCommitEffects` -> `mWave = index` | effects | Full 0, Half 1 | OK | none (widget_history=false; subject/selection state) |
| 22 | hover_amp_spin | spinner | `onCommitEffects` -> `mAmpMeters` | effects | 0..10 step 0.05, 2 dp | OK | none (widget_history=false; subject/selection state) |
| 23 | hover_freq_spin | spinner | `onCommitEffects` -> `mFreqHz` (min 0) | effects | 0..10 step 0.05, 2 dp | OK | none (widget_history=false; subject/selection state) |
| 24 | rock_on_check (Rock card switch) | toggle | `onCommitEffects` -> `fx.mRock.mOn` | effects | bool | OK | none: card enable switches are not recorded by kit widget history |
| 25 | rock_axis_combo | combo | `onCommitEffects` -> `mAxis = index` | effects | X 0, X+Y 1, X+Y+Z 2 | OK | none (widget_history=false; subject/selection state) |
| 26 | rock_amp_spin | spinner | `onCommitEffects` -> `mAmpDeg` | effects | 0..90 step 1, 1 dp | OK | none (widget_history=false; subject/selection state) |
| 27 | rock_freq_spin | spinner | `onCommitEffects` -> `mFreqHz` | effects | 0..10 step 0.05, 2 dp | OK | none (widget_history=false; subject/selection state) |
| 28 | spin_on_check (Spin card switch) | toggle | `onCommitEffects` -> `fx.mSpin.mOn` | effects | bool | OK | none: card enable switches are not recorded by kit widget history |
| 29 | spin_axis_combo | combo | `onCommitEffects` -> `mAxis = index` | effects | X 0, Y 1, Z 2 | OK | none (widget_history=false; subject/selection state) |
| 30 | spin_speed_spin | spinner | `onCommitEffects` -> `mDegPerSec` | effects | -720..720 step 5, 1 dp | OK | none (widget_history=false; subject/selection state) |
| 31 | btn_shuttle | button | `onClickShuttle` (axis/sign/distance/speed/slope in) | `ALObjectPathMover` shuttle builder; enabled by `refreshNodePane` when a prop is selected | n/a | OK | n/a (action/output, no widget step) |
| 32 | shuttle_axis_combo | combo | `onCommitShuttleAxis` (also gates the slope spinner to XZ) | shuttle axis | X, Y, Z, XZ | OK | none (widget_history=false; subject/selection state) |
| 33 | shuttle_sign_combo | combo | read by `onClickShuttle` | shuttle sign | + / - | OK | none (widget_history=false; subject/selection state) |
| 34 | shuttle_distance_spin | spinner | read by `onClickShuttle` | shuttle distance | 0.1..500 step 0.5, 2 dp | OK | none (widget_history=false; subject/selection state) |
| 35 | shuttle_speed_spin | spinner | read by `onClickShuttle` | shuttle speed | 0.05..50 step 0.25, 2 dp | OK | none (widget_history=false; subject/selection state) |
| 36 | shuttle_slope_spin (Advanced) | spinner | read by `onClickShuttle`; enabled only for XZ | shuttle slope | -89..89 step 5, 0 dp | OK | none (widget_history=false; subject/selection state) |

(Rows 20, 24 and 28 are the card switches, not mockup rows: 33 mockup rows plus 3 switches = 36 listed controls.) The spinner ranges above are the hosted v1 values, which equal the mockup ranges.

Behaviour note: while a Hover, Rock or Spin card switch is off, the kit card dims its other rows (`alkitcard.cpp`); the panel still writes the whole effects struct on any commit, exactly as before.

## Checks run

`director_kit_gen.py` check and selftest, `director_page_sources.py --check`, `kit_lint.py --all`, `xui_layout_check.py` selftest and `--changed HEAD`, `director_functional_manifest.py --check` (props_tab binder `ALPanelPropMover::postBuild` registered), `director_parity.py` selftest and `--groups light_group`, `zs_check.ps1`, `prebuild_guard.ps1 -ChecksOnly`. Not run (rules): build, viewer, in-world self-test. Worth an in-world look: enroll via the button and via right-click, Drive with two nodes, the three effect switches toggling in the page, and the list column text (`prop_name`/`prop_state`).
