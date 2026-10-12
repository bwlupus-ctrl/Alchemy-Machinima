# Director functional self-test: in-world mode

Run the authorized v2 test with:

```powershell
scripts/perf/ui_selftest.ps1 -Functional -InWorld -Account 5kvideoeye
```

`-Pages` and `-Rows` work as in login-screen mode. In-world mode rejects v1,
`-Both`, and every account other than `5kvideoeye`.

## Login and credential boundary

The script launches the viewer with `--autologin`. That option sets
`AutoLogin`; `LLStartUp` loads the last remembered `LLCredential` through the
viewer's security API. The harness neither opens `bin_conf.dat` nor reads,
prints, types, stores, or passes a password. The existing login-screen
self-test interlock initially shows the login UI; the in-world runner resumes
`STATE_LOGIN_CLEANUP` only when `AutoLogin` is set, using the credential already
held by `LLStartUp`.

After `STATE_STARTED`, the runner gets the avatar name from
`LLAvatarNameCache` and requires `5kvideoeye`, case-insensitively, also accepting
the legacy forms `5kvideoeye Resident` and `5kvideoeye.Resident`. A mismatch
writes `FAIL: wrong account`, starts the normal `requestQuit()` logout path, and
makes the wrapper exit non-zero from the fresh FAIL row. A 120-second login
timeout does the same; it never retries authentication.

The test waits for an agent region, a valid fully loaded self avatar, 120
consecutive frames, and at least three settled seconds before opening Director.

## Fail-closed interaction safety

Manifest preparation sets `inworld_allowed` only for a pure setting binding
with no owner/callback, or for one of the explicit source-owner families below.
Handler scanning remains defense in depth, but the absence of a detected hazard
never grants permission. Every other executable row is skipped as
`in-world: not on the viewer-local allow-list`.

| Allowed owner family | Locality evidence |
|---|---|
| Light Rig / Rim: `ALPanelCineLightRig`, `ALRigRimController`, `ALPanelRigRim` | `alcinelightrig.cpp:688-699,1359-1381` writes viewer settings and builds client render-light state. |
| Lightbox / looks / exposure / weather / atmosphere: `ALFloaterLightBox`, `ALPanelLensLooks`, `ALPanelExposureHub`, `ALPanelWeatherSettings`, `ALPanelAtmosphericVolumes` | `alfloaterlightbox.cpp:209-250,1172-1224` binds and writes viewer controls; the listed panels expose those render settings. |
| Gaze: `ALPanelLensGaze`, `ALFloaterGazeCues` | `lldirectorcast.cpp:1225-1246` updates only the Director cast gaze model. |
| Ghost clones: `ALPanelGhostStudio` | `lldirectorcast.cpp:1225-1246` shows the cast state is an in-process model; Ghost Studio consumes client clone state. |
| Dance Ball / local animation: `ALPanelDanceBall`, `ALPanelDirectorAnimSwitcher`, `ALPanelAnimPreview` | `aldanceball.cpp:1329-1336` and `aldirectoranimswitcher.cpp:418-425` call local avatar motion APIs. |
| Props mover / Path: `ALPanelPropMover`, `ALPanelActorMover`, `ALPanelPathEditor`, `ALObjectPathMover` | `alobjectpathmover.cpp:46-58` applies drawable transforms and marks them moved locally. |
| VCam page model: `ALPanelDirectorVCam`, `ALVCamPageModel`, `ALKitPage` | `alpaneldirectorvcam.cpp:990-998` changes the local capture model; `vc_lens_locate` and `vc_face_locate` are explicitly excluded because `alpaneldirectorvcam.cpp:999-1015` selects an object. |
| Camera / frame / orbit: `ALPanelCineCamParams`, `ALPanelDirectorSwitcher`, `ALPanelFlycamRecorder`, `LLFlycamRecorder`, `ALPanelTemporalCapture` | `alpanelcinecamparams.cpp:462-477` writes camera settings; local animation dispatch is independently evidenced at `llfloaterdirector.cpp:3726-3738`. |
| Exact vetted owner/page pairs | `LLPipeline::renderWeather` on `weather_tab`; `LLPipeline::renderFinalize` on `exposure_tab`/`shadows_probes_tab`; `LLFloaterDirector::postBuild` only on `actors_tab`, `actor_style_tab`, `animate_tab`, `dance_tab`, `frame_orbit_tab`, `gaze_tab`, `move_tab`, `moves_tab`, and `shot_tab`; `LLFloaterDirector::sceneSettingsList` only on `switchboard_tab`. Evidence: `alfloaterlightbox.cpp:1172-1224`, `lldirectorcast.cpp:1225-1246`, and `llfloaterdirector.cpp:3726-3738`. |

Keyboard input continues to use the focus-guarded injection path. The final
`requestQuit()` is intentional normal logout after all state has been restored.

## In-world coverage and restoration

The runner executes login-deferred cast/ghost/gaze/dance/props rows when their
avatar or viewer-local prerequisites are available, using the same reveal,
real-dispatch, commit, consumer, and restore assertions as login-screen rows.
Viewer-local Director Light Rig, Rim, Weather, Gaze, client ghosts, Dance Ball,
Props mover, VCam, Lightbox, camera, and exposure controls remain eligible.

Before VCam rows, the runner snapshots and temporarily isolates the VCam
registry so the test camera is CAM 1, then adds a virtual camera at the current
view labelled `Virtual at my view`. It scans existing
nearby viewer objects without rezzing; when an existing spotlight/projector is
within 20 m, the page-model/projector API binds the test camera and exposes the
Projector cards. Otherwise those card rows are skipped with
`no projector light nearby`. The saved VCam scene is reapplied at teardown, so
the created camera, bindings, and all row edits are removed.

The C++ settings/rig snapshots are restored before normal quit. The PowerShell
wrapper separately backs up and byte-verifies only `settings*.xml` beneath
`user_settings` and `%APPDATA%\AlchemyMachinima\5kvideoeye_resident`; it never
copies `bin_conf.dat` and never replaces those directories, so chat/IM logs and
other files created during the run survive. Output is the usual
`functional.csv` and `summary.md`; CSV rows include `inworld=true`.

An in-viewer wall-clock watchdog defaults to 12 minutes (the wrapper passes
`watchdog_seconds` from `-InWorldWatchdogMin`). It restores test state, writes
the partial CSV, and requests normal logout. The wrapper timeout defaults to 18
minutes, then allows a further 60-second normal-exit grace before reporting any
last-resort hard kill explicitly.
