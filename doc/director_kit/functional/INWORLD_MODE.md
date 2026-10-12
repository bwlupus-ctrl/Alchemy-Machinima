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
writes `FAIL: wrong account`, sends the viewer logout request, and exits with
status 1 immediately. A 120-second login timeout does the same; it never retries
authentication.

The test waits for an agent region, a valid fully loaded self avatar, 120
consecutive frames, and at least three settled seconds before opening Director.

## Fail-closed interaction safety

Manifest preparation resolves installed handlers and scans them before the
viewer can click a row. An unresolved in-world handler is skipped as
`in-world: hazard unknown`. Besides the existing file, delete, rename,
import/export, save, and logout hazards, the in-world deny-list covers:

- `gMessageSystem` and reliable/message sends;
- `LLAgent` send methods;
- `LLViewerObject::sendXxx` and `LLSelectMgr::sendXxx`;
- parcel update/buy/claim/deed/release sends;
- chat, IM, voice, rez/create/duplicate/delete/take/return, give/pay/buy,
  teleport/sit/stand, attach/detach/wear/take-off, uploads, group/friend actions,
  and comparable world-action handlers.

Rows tagged by that scan are skipped as
`in-world: server-side action denied`. Keyboard input continues to use the
existing focus-guarded injection path. The final `requestQuit()` is intentional
normal logout after all state has been restored.

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
wrapper separately backs up and byte-verifies restoration of both
`user_settings` and `%APPDATA%\AlchemyMachinima\5kvideoeye_resident`. Output is
the usual `functional.csv` and `summary.md`; CSV rows include `inworld=true`.
