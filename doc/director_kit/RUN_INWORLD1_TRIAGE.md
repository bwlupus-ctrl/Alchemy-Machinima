# In-world run 1 triage — 2026-10-12 09:33:09

Input: `run_functional_20261012_093309/functional.csv`, HEAD `6617aaa2618`, 1,933 rows (630 PASS / 228 FAIL / 1,075 SKIP), account `5kVideoEye` PASS with clean logout.

## Safety verdict

`move_tab/btn_walk` and `btn_stop` are viewer-local and remain allow-listed. `ALPanelActorMover::onClickWalk/onClickStop` only call `LLActorMover` (`alpanelactormover.cpp:104-136`). The mover starts the avatar motion through `LLCharacter::startMotion` (`llactormover.cpp:186-275,701-731`) and paints position/rotation only on the rendered root joint (`llactormover.cpp:4506-4640`). There is no AgentUpdate, agent control-flag, autopilot, or other movement send in that path; this remains true when the resolved actor is self.

`props_tab/btn_drive_all` is also viewer-local and remains allow-listed. It calls `ALObjectPathMover::startAll` (`alpanelpropmover.cpp:450-453`); the per-frame operation is `setPositionAgent`, `setRotation`, and `gPipeline.markMoved` (`alobjectpathmover.cpp:46-58,276-282`), with no simulator object-update send.

Gaze resets, Cine Light Rig actions, and VCam actions are viewer-local, but they pass only when their model/renderer oracle moves. Animation rows that can reach an unqualified avatar `startMotion`/`stopMotion` are excluded: an AO override on self may call `gAgent.sendAnimationRequest` (`llvoavatar.cpp:7398-7413,7442-7457`; call sites `aldirectoranimswitcher.cpp:414,418,565`, `llfloaterdirector.cpp:3726,3741`). Missing fixture rows are `KNOWN_GAP`, not `SKIP`.

## Classification and disposition

| Failed group | Rows | Class | Evidence and fix |
|---|---:|---|---|
| Reveal did not settle | 75 | HARNESS | `changeOpenClose(true)` closed the accordion it had just opened; corrected at `aluiperfselftest.cpp:1961-1974`. Reveal now scrolls every deferred frame (`:3161-3188`). VCam cards are intentionally conditional (`alpaneldirectorvcam.cpp:834-857`), so the fixture now creates/selects a camera and virtual display and switches raw/display prerequisites (`aluiperfselftest.cpp:4360-4386,5002-5038`). |
| Category A, no external delta | 48 | HARNESS | Commit/owner execution is never an effect oracle. In-world actions now assert the changed rig, probe scheduler, VCam registry, actor mover, gaze target, or actor-style model; absent fixtures are `KNOWN_GAP`. |
| Not on screen | 41 | HARNESS | Master settings were already declared; the missing operation was scroll during idle-deferred reveal. Fixed at `aluiperfselftest.cpp:3161-3188`, preserving the on-screen assertion. |
| Hazard needs reason | 32 | HARNESS | Local persistence exemptions are keyed by `(owner,page,widget)` with file/line evidence and suppress only `save_disk`, `rename`, `file_picker`, and `export_import`; server/agent/world hazards remain active. |
| Category D | 11 | HARNESS | Radio targeting uses the public radio payload. A swatch now selects a test colour and must change its bound setting; `Color3` compares RGB only against the stored `[r,g,b,0]`. An unbound swatch is a failing `KNOWN_GAP`, never a picker-open PASS. |
| Disabled by parent | 6 | HARNESS | Four auto-frame Bézier rows now set Enabled + Custom=100 and AMD exposure sets tonemap=7, matching the product gates. Cue delete is a failing `KNOWN_GAP` because no saved cue exists. |
| Foreign toast capture | 4 | HARNESS | GenericAlert/refusal toasts fail. Only a manifest-named confirmation is answered through the notification API and its resulting setting is asserted. |
| Observed mismatch | 3 | MIXED: 2 H / 1 P | Spinner expected values are independently computed as `clamp(snap(before +/- increment), min, max)`, and an out-of-range default fails. Glow Boost's source range lives in `director_mockup_data.json`; generated panel XML is never hand-edited. |
| Category C monitor strip | 1 | HARNESS | Failing `KNOWN_GAP`: the custom strip commits only named tile menu/drag hit zones, not its decorative centre (`alpaneldirectorvcam.cpp:900-903`). |
| Projector selection on `vc_enable` | 1 | HARNESS | Projector preselection now runs only for projector cards (`aluiperfselftest.cpp:4482-4486`). |
| Hidden `vc_source_type` target | 1 | HARNESS | Failing `KNOWN_GAP`: isolated `Virtual at my view` is already Virtual and has no safe alternate source fixture. |
| Missing monitor-tile control | 1 | HARNESS | Failing `KNOWN_GAP`: this is a manifest-only description inside the custom strip, not a standalone LLUICtrl. |
| Wrong gaze-reset default | 1 | HARNESS | Actor override resets now seed and assert the `LLActorMover::GazeTarget` field itself: `-1`, or `-2` for animation priority; the anatomical profile reset compares the full default profile. |
| Toolbar intercepted `ri_intensity` | 1 | HARNESS | Same reveal-scroll correction at `aluiperfselftest.cpp:3161-3188`; hit-testing is not relaxed. |
| Local-fog tint swatch | 1 | HARNESS | It is unbound, so picker-open alone cannot pass; the row remains a failing `KNOWN_GAP`. |
| Actor-style combo lacks item | 1 | HARNESS | Failing `KNOWN_GAP`: the isolated fixture has no populated cast subject beyond self. |

## Wrapper result accounting

PowerShell variable names are case-insensitive, so local `$rows` aliased the `-Rows` parameter. The wrapper now imports into `$functionalResultRows`, rejects a fresh empty CSV, prints counts from it, and uses the same collection for SKIP/sentinel/failure exit decisions (`ui_selftest.ps1:358-387`).

No row passes from commit/owner execution alone. A real state oracle must move; absent fixtures are failing `KNOWN_GAP`, while `SKIP` is reserved for safety exclusions with evidence.
