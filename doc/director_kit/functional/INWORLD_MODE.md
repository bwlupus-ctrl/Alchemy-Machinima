# Director functional self-test: in-world mode

Run the authorized v2 test with:

```powershell
scripts/perf/ui_selftest.ps1 -Functional -InWorld -Account 5kvideoeye
```

`-Pages` and `-Rows` work as in login-screen mode. In-world mode rejects v1,
`-Both`, and every account other than `5kvideoeye`.

## Login and credential boundary

The script launches the viewer with `--autologin` to enable the normal login
path, but it does not accept the last-used account. At `STATE_LOGIN_WAIT`, the
in-world bootstrap searches the viewer security API's `login_list`, normalizing
spaces, dots, underscores, case, and a trailing `Resident`. It selects the
saved `5kvideoeye` entry through `LLPanelLogin`, exactly like choosing that row
in the username combo (`loadFromCredentialMap`/`setFields`), and only then
resumes `STATE_LOGIN_CLEANUP`. If the entry is absent or has no remembered
authenticator, the run writes `FAIL: authorized account not saved` and exits
before any login attempt; it never falls back to the default credential.

The harness never reads, prints, types, stores, or passes a password.
Authenticator presence is checked inside the existing login-panel
implementation, and only the viewer-owned `LLCredential` follows the normal
login path. During this self-test only, `LLStartUp` skips both protected-store
writes that normally update the saved-users map and default/last-used
credential. The wrapper never copies, hashes or decodes `bin_conf.dat`; the
viewer re-salts that file on every exit, so a byte check would prove nothing.

After `STATE_STARTED`, the runner gets the avatar name from
`LLAvatarNameCache` and requires `5kvideoeye`, case-insensitively, also accepting
the legacy forms `5kvideoeye Resident`, `5kvideoeye.Resident`, and
`5kvideoeye_resident`. A mismatch writes the unchanged `FAIL: wrong account`,
starts the normal `requestQuit()` logout path, and makes the wrapper exit
non-zero from the fresh FAIL row. The account PASS or mismatch FAIL row's
observed JSON records `user_name` and `legacy_name` only. A 120-second login
timeout does the same; it never retries authentication.

The test waits for an agent region, a valid fully loaded self avatar, 120
consecutive frames, and at least three settled seconds before opening Director.

## Fail-closed interaction safety

Manifest preparation automatically sets `inworld_allowed` for a generated kit
row only when its XML declares `bind="setting"` and names a saved setting; that
declaration makes `gSavedSettings` the sole write sink. Code-bound rows still
require an explicit source-owner authorization and the unchanged callback
scanner. The run-1 baseline cannot deny a newly generated setting-only row, but
all scanner hazards still override its allowance. The absence of a detected
hazard never grants a code-bound row permission. Every other row is skipped as
`in-world: not on the viewer-local allow-list`.

The scanner resolves the registration for the exact control name (direct or
member `setCommitCallback`/`setClickedCallback`, `childSetAction`, callback
registry, XUI callback, or a complete balanced lambda). It scans that callback
and callees through depth 3 in the same and known owner files. If exact
resolution fails, it scans the old conservative union of every handler in the
full proximity window. Newly executable rows relative to run 1 are separately
denied unless [INWORLD_NEW_ROWS.md](INWORLD_NEW_ROWS.md) records exact
`file:line` locality evidence.

| Allowed owner family | Locality evidence |
|---|---|
| Light Rig / Rim: `ALPanelCineLightRig`, `ALRigRimController`, `ALPanelRigRim` | `alcinelightrig.cpp:688-699,1359-1381` writes viewer settings and builds client render-light state. |
| Lightbox / looks / exposure / weather / atmosphere: `ALFloaterLightBox`, `ALPanelLensLooks`, `ALPanelExposureHub`, `ALPanelWeatherSettings`, `ALPanelAtmosphericVolumes` | `alfloaterlightbox.cpp:209-250,1172-1224` binds and writes viewer controls; the listed panels expose those render settings. |
| Gaze: `ALPanelLensGaze`, `ALFloaterGazeCues` | `alpanellensgaze.cpp:532-676` routes reset buttons into the in-process cast model; `lldirectorcast.cpp:1225-1246` updates only that model. |
| Ghost clones: `ALPanelGhostStudio` | `lldirectorcast.cpp:1225-1246` shows the cast state is an in-process model; Ghost Studio consumes client clone state. |
| Dance Ball / animation-bank editing | Dance model/configuration edits remain local. Rows that can reach an unqualified avatar `startMotion`/`stopMotion` are excluded: on self, an AO override can call `gAgent.sendAnimationRequest` (`llvoavatar.cpp:7398-7413,7442-7457`; call sites `aldirectoranimswitcher.cpp:414,418,565` and `llfloaterdirector.cpp:3726,3741`). |
| Actor mover / Walk | `alpanelactormover.cpp:104-136` calls `LLActorMover`; `llactormover.cpp:701-731,4214-4341` starts only `LLCharacter::startMotion`, and `llactormover.cpp:4506-4640` applies only render-root joint position/rotation. It sends no AgentUpdate, control flags, or autopilot request, including for self. |
| Props mover / Path | `alpanelpropmover.cpp:450-453` calls `ALObjectPathMover::startAll`; `alobjectpathmover.cpp:46-58,276-282` applies only viewer-object/drawable transforms and `gPipeline.markMoved`, with no object-update send. |
| VCam page model: `ALPanelDirectorVCam`, `ALVCamPageModel`, `ALKitPage` | `alpaneldirectorvcam.cpp:860-942,990-1052` changes the local capture registry, opens a local manager, snaps a virtual camera, or creates a virtual display. `vc_lens_locate` and `vc_face_locate` remain excluded because `alpaneldirectorvcam.cpp:999-1015` selects an object. |
| One-shot/local action buttons | `alpanelcinelightrig.cpp:784-806` stops the selected local FX controller or requests a local render-probe refresh. Dance buttons use local motion (`aldanceball.cpp:1329-1336`); gaze resets use the local cast model; VCam manager/snap use the local registry. |
| Camera / frame / orbit: `ALPanelCineCamParams`, `ALPanelDirectorSwitcher`, `ALPanelFlycamRecorder`, `LLFlycamRecorder`, `ALPanelTemporalCapture` | `alpanelcinecamparams.cpp:462-477` writes camera settings. Animation-dispatch rows are excluded by the AO rule above. |
| Exact vetted owner/page pairs | `LLPipeline::renderWeather` on `weather_tab`; `LLPipeline::renderFinalize` on `exposure_tab`/`shadows_probes_tab`; `LLFloaterDirector::postBuild` only on `actors_tab`, `actor_style_tab`, `animate_tab`, `dance_tab`, `frame_orbit_tab`, `gaze_tab`, `move_tab`, `moves_tab`, and `shot_tab`; `LLFloaterDirector::sceneSettingsList` only on `switchboard_tab`. Evidence: `alfloaterlightbox.cpp:1172-1224`, `lldirectorcast.cpp:1225-1246`, and `llfloaterdirector.cpp:3726-3738`. |

Keyboard input continues to use the focus-guarded injection path. The final
`requestQuit()` is intentional normal logout after all state has been restored.

## In-world coverage and restoration

The runner executes login-deferred cast/ghost/gaze/dance/props rows when their
avatar or viewer-local prerequisites are available, using the same reveal,
real-dispatch, commit, consumer, and restore assertions as login-screen rows.
Viewer-local Director Light Rig, Rim, Weather, Gaze, client ghosts, Dance Ball,
Props mover, VCam, Lightbox, camera, and exposure controls remain eligible.
`gaze_breakoff_check` uses its actual `BDMergeGazeBehindPolicy` S32 write as the
independent setting oracle; it is not a missing per-actor-state gap.
If a required dance, ghost, prop, animation/cue, or selection fixture is absent,
the row is a failing `KNOWN_GAP`; fixture absence is never a PASS or safety SKIP.

Before VCam rows, the runner snapshots and temporarily isolates the VCam
registry, adds two virtual cameras (so the selector has a real alternate), and
adds a temporary virtual display. The runner
selects that camera before camera-card rows, selects the display before the
Face mapping/TV cards, and selects raw output for its mutually exclusive card.
It scans existing
nearby viewer objects without rezzing; when an existing spotlight/projector is
within 20 m, the page-model/projector API binds the test camera and exposes the
Projector cards. Otherwise those card rows are skipped with
`no projector light nearby`. The saved VCam scene is reapplied at teardown, so
the created camera, bindings, and all row edits are removed.

The C++ settings/rig snapshots are restored before normal quit. The PowerShell
wrapper separately backs up and byte-verifies only `settings*.xml` beneath
`user_settings` and `%APPDATA%\AlchemyMachinima\5kvideoeye_resident`; it never
touches `bin_conf.dat` and never replaces those directories, so chat/IM logs
and other files created during the run survive. The animation switcher
`saveBank()` writes `DirectorAnimSwitcherBank` through `gSavedSettings`, so its
per-account/settings XML persistence is covered by that backup and restore.
Output is the usual
`functional.csv` and `summary.md`; CSV rows include `inworld=true`.

An in-viewer wall-clock watchdog defaults to 30 minutes (the wrapper passes
`watchdog_seconds` from `-InWorldWatchdogMin`; the clock starts with the viewer, so
it covers login, the manifest rows and the rule phase). It restores test state,
writes the partial CSV, and requests normal logout. The wrapper timeout defaults
to the watchdog plus 8 minutes, then allows a further 60-second normal-exit grace
before reporting any last-resort hard kill explicitly.

## Input safety: global bindings, world hover, key routing and AFK (10/15)

The harness injects real mouse movement, clicks and keys. Four viewer behaviours sit
in front of the UI or behind it on those paths, and all are closed fail-closed for
the whole run (login, in-world, teardown and logout):

1. **Global key/mouse bindings.** `LLViewerWindow::handleAnyMouseClick` runs
   `gViewerInput.handleGlobalBindsMouse` before any UI dispatch
   (`llviewerwindow.cpp`, "Handle non-consuming global keybindings"). A saved plain
   left-click binding to `toggle_voice` would therefore open the microphone on a
   harness click. The in-world runner calls
   `LLViewerInput::setGlobalBindsSuspended(true)` from its constructor, before login;
   `handleGlobalBindsKeyDown/KeyUp/Mouse` then return without running anything.
   The flag is process-local (nothing is written to the user's bindings file, so a
   crash needs no restore). It is NOT cleared by `Runner::restore()` (see 3): the
   flags stay up through the logout and drop with the process; only
   `abortForLogin()` lifts them, because that session continues as a normal one.
   Every injected event (`functionalInputSafe`) and every pre-dispatch check refuses
   unless the suspension is in force, and the per-frame guard fails the run if it is
   ever lifted.
2. **AFK.** An injected mouse move reaches `LLAgent::clearAFK()`
   (`LLViewerWindow::handleMouseMove`), which sends `ANIM_REQUEST_STOP` while the
   avatar is away. The harness never calls `clearAFK`. Instead: (a) once the account
   is verified it disables the `AFKTimeout` setting for the run (active value only,
   restored by the settings snapshot; if it cannot, the run FAILs
   `inworld_afk_safety`); (b) every frame it resets `gAwayTriggerTimer`, the idle
   timer, directly; (c) the avatar being away (`AGENT_CONTROL_AWAY` or the away
   animation signalled, exactly the condition under which `clearAFK` would send)
   fails the run at once with an `inworld_afk` FAIL row and a normal logout, and
   `functionalInputSafe` refuses every injected move, click and key while away, so
   nothing is sent. An avatar that is already away at the start fails the same way
   before any click phase.
3. **World hover (cursor over the world at teardown).** `restore()` hides the
   Director and `requestQuit` closes the floaters, but the synthetic mouse point stays
   where the last click was, and the real OS cursor may be anywhere in the window.
   With nothing in front of it, `LLViewerWindow::updateUI` runs
   `LLToolPie::handleHover` -> `LLSelectMgr::setHoverObject` ->
   `RequestObjectPropertiesFamily` (and the hover parcel, land/object tips and media
   hover). The second flag, `LLViewerInput::setWorldInputSuspended`, is set together
   with the binds flag and gates this at four points: the two `tool->handleHover`
   calls and the `handleToolTip` call in `updateUI` (`llviewerwindow.cpp`),
   `LLToolPie::handleHover` and `handleToolTip` (`lltoolpie.cpp`), and
   `LLSelectMgr::setHoverObject` (`llselectmgr.cpp`, the function that sends the
   request). UI hover (floaters, captors) is unaffected. `restore()` first parks the
   synthetic mouse on the main menu bar (persistent, opaque chrome;
   `functionalParkMouse`, recorded as `teardown.mouse_parked`) and does not lift the
   flags: they are lifted only by process exit after the logout completes (or by
   `abortForLogin()`). The flag is process-local, set only by an in-world launch
   request, so a normal session never has it.
4. **Key routing.** `LLViewerWindow::handleKey` runs menu accelerators before focus
   dispatch (Ctrl+Z -> `Edit.Undo` -> `LLSelectMgr::undo` sends `Undo` to the sim for
   an undoable selection) and falls through to `LLGestureMgr::triggerGesture` /
   `gGestureList.trigger` (a user gesture bound to the key sends chat or an
   animation). The harness therefore never enters `handleKey`, `handleKeyUp` or
   `handleUnicodeChar`: every keystroke is delivered straight to the target widget
   (`functionalDeliverKey` -> `handleKeyHere`, `functionalDeliverChar` ->
   `handleUnicodeCharHere`, `functionalSelectAll` -> the text control's own
   `selectAll()`), each refusing unless the widget holds the keyboard focus. The
   `sld_diffuse` Ctrl+Z is delivered only to a focused non-text control inside the
   page (`light_response.undo_target_non_text_focus`); otherwise the row FAILs.
   Defence in depth: while the world flag is set, `handleKey` stops after the
   keyboard-focus dispatch (no menu navigation/accelerator, tool key, gesture,
   Director hotkey or auto-chat), `handleKeyUp` never reaches a gesture release, and
   `handleUnicodeChar` skips the Return remap and menu jump keys.

5. **Tool clicks, tool mouse capture, picks.** While the world flag is set
   `LLViewerWindow::handleAnyMouseClick` is UI dispatch only: a click the UI does not
   take, or any click while the UI is disabled, is dropped and never reaches the tool
   manager or the pie menu (and `functionalInputSafe` refuses to inject while the UI is
   disabled). Only UI views may hold the mouse capture: `dropToolMouseCaptureWhileWorldSuspended`
   removes a tool's capture WITHOUT its `onMouseCaptureLost()` callback (a grab would
   send there) at the top of click dispatch, `updateUI`, scroll wheel, before every
   synthetic mouse move, before parking, and every in-world frame; a tool therefore
   never receives captor hover or motion (`ObjectGrabUpdate`). `pickAsync` schedules
   nothing and `pickImmediate` returns an empty pick.
6. **Cancelled logout.** If the logout is cancelled (unsaved notecard/script dialog,
   `LLAppViewer::abortQuit`), `ALUIPerfSelfTest::quitAborted` lifts both flags when no
   runner is left. The harness does not answer a logout-blocking dialog; an uncancelled
   one is ended by the wrapper's timeout and hard-kill grace.

Injection routes (every one in `aluiperfselftest.cpp`):

| Injection | Route |
| --- | --- |
| mouse move (`functionalMoveMouse`) | `handleMouseMove` (AFK-guarded; hover gated) |
| click down/up (`functionalMouseButton`) | `handleAnyMouseClick` (binds suspended; target preverified) |
| slider/dial drag `updateUI()` | UI hover only (world tool hover gated) |
| slider/dial arrow nudges | `functionalNudgeKey` (focus the widget, then guarded `functionalDeliverKey`) |
| Tab, Esc | `director->handleKeyHere` / `floater->handleKeyHere` direct |
| Ctrl+Z (`sld_diffuse`) | `functionalDeliverKey` -> focused non-text control `handleKeyHere` |
| Ctrl+A in text rows | `functionalSelectAll` -> `LLLineEditor/LLTextEditor::selectAll()` |
| typed characters | `functionalDeliverChar` -> `handleUnicodeCharHere` |
| Return after typing | not delivered (the old window route consumed it without reaching the widget) |

## Rule-phase click gate (10/15)

Every click of the rule phase goes through `dirClick`, which in-world calls
`dirInWorldClickAllowed` first (same allow-list / hazard / exclusion policy as the
manifest rows; unknown means refuse):

- the explicit in-world exclusion list is one shared function
  (`functionalInWorldExcludedWidget`) used by the manifest rows and this gate;
- a control that is also a manifest row must pass that row's own `inworld_allowed`,
  skip/known-gap and hazard checks;
- `nav`: only the shell's own rail buttons, page tabs, tab arrows and the gear;
- `popover_anchor`: only `btn_arm`, `btn_scene_menu`, `btn_cast_chip`, `btn_help`,
  `btn_legend_hotkeys`, a page's Reset button (opens its confirm popover; Reset is
  never confirmed) and a Who chip only when the page has a Who combo (without one a
  click toggles the actor pin and saves page state, so it is never clicked);
- `link_row` / `link_card`: only the GENERATED navigation button (`alkitrow.cpp`
  postBuild, `alkitcard.cpp` `kc_<id>_go`). The link registry carries an `authored`
  flag; an authored widget that shares the link's name is refused, as is any button
  whose label is not the generated one;
- a popover anchor collapsed into the transport bar's overflow list is reported as
  WARN, not driven (an overflow entry forwards to controls such as Save scene).

## Director rule phase (R1-R6, 10/15)

After the manifest rows the same session runs the Director rule engine
(reachability, popovers, label fit, raw names, themed pages, connectedness; the
checks are listed in `FUNCTIONAL_MANIFEST.md`). It is viewer-local UI work only:
real clicks on shell chrome (rail, gear, tabs, popover anchors, link rows),
`ALKitShell` navigation and help calls, and widget-tree reads. It presses no
manifest control, sends nothing and never confirms a popover action, so it adds
no entry to the hazard lists. It is queued as about 4000-7500 frames (38 pages, 14
links, about 80 popover flows), i.e. roughly 3 to 12 minutes at 10-30 fps; the
estimate comes from the queued tasks. The in-world run will not start it with less
than 240 s of the watchdog left, or less than the estimate at an assumed 10 fps, and
records a FAIL row asking for a larger `-InWorldWatchdogMin` instead. It reports its
measured wall time and frame count in an `R1_REACHABILITY` `rule_phase_timing` row
(PASS only when the phase ran; when the budget or a precondition refused it the row is
a SKIP and the refusal's own FAIL row carries the reason).
The watchdog default is raised to 30 minutes and the wrapper timeout is the watchdog
plus 8 minutes (9 at the login screen).

R6 (connectedness) credits only an effect observed in this run: the control's own
functional row must have run and passed. Skipped or excluded rows (everything outside
the in-world allow-list), KNOWN_GAP rows and rows excluded by a `version=` filter are
reported as SKIP with their reason, never PASS and never FAIL. Rows for one control
are read in run order and the final result wins: a FAIL followed by a PASS (a retry)
is a PASS, a FAIL after the last PASS is a FAIL.

## Selection and Animation Explorer exclusions (10/12)

Any `LLSelectMgr` selection, even of a viewer-only (`mIsLocalOnly`) object, broadcasts a PointAt ViewerEffect and `AGENT_STATE_EDITING` in AgentUpdate (llselectmgr.cpp:679, 8049; llhudeffectpointat.cpp:254; llagent.cpp:2312-2315), so other residents see the avatar's edit pose. `btn_enroll` (which reads the selection) is excluded in-world. `db_from_explorer` is excluded because Animation Explorer sends ObjectSelect/ObjectDeselect to name non-avatar animation sources (animationexplorer.cpp:540-556). Both are excluded in `functional_report.py` INWORLD_EXCLUDED_CONTROLS and in the C++ runner.
