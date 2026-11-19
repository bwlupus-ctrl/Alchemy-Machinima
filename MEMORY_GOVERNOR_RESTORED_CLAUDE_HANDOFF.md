# Alchemy Memory Governor Restored — Claude Handoff

Date: 2026-08-22  
Repository: `I:\alchemy-machinima`  
Branch: `feature/cine-light-rig`  
Direct-run build: `I:\alchemy-machinima\build-Windows-vs2026-os\newview\Release\AlchemyTest.exe`

## Outcome

The working ReShade installation was checkpointed first, then the pressure-aware memory governor was restored and the Release viewer rebuilt successfully. The post-build viewer is running and responsive from the `I:` Release directory. ReShade 6.8 remained byte-for-byte unchanged and initialized/compiled effects successfully after the rebuild.

The previous ReShade overlay failure was resolved by reinstalling ReShade before this patch was restored. The speculative `STATE_STARTED`/startup-deferral workaround from the abandoned attempt is **not** included.

## Known-good checkpoint

Git checkpoint commit created before restoring the governor:

```text
5c87c46a669  Checkpoint: working ReShade 6.8.0 before memory governor
```

This is an empty marker commit. It did not include or modify the repository's unrelated untracked files.

Working ReShade recovery archive:

```text
Path: I:\alchemy-machinima\build-Windows-vs2026-os\newview\checkpoints\reshade-working-6.8.0-5c87c46a669.zip
Size: 466,265,177 bytes
SHA-256: CF512CD5604EB819BADEA7B5995086F36FF67208E3449E0D5BEEADD8C717926D
```

The archive contains the active injector/config/log plus `reshade-shaders`, `PRESETS`, `Addons`, and `_shader-backups` from the working Release directory.

## ReShade regression baseline

Active injector after the memory build:

```text
Path: I:\alchemy-machinima\build-Windows-vs2026-os\newview\Release\opengl32.dll
Version: 6.8.0.2155
Size: 5,592,064 bytes
SHA-256: 0CEE63F9C9F13F3AC909C5B4903F4DBB4B719A7AB3B4F13B0DEAF83C814B94F7
```

Active configuration after the memory build:

```text
ReShade.ini SHA-256: EC66B7FBBD8A97A2E7D47B56B3112855F5C7227E04213A7D463AADF0B6AEA292
```

Both hashes exactly match the pre-patch checkpoint. The post-build `ReShade.log` records `Initialized`, runtime recreation, and successful compilation of the SL G-buffer provider, iMMERSE effects, and Virtual Cinema effects.

## Restored memory work

The governor addresses the dense-region runaway caused by texture and mesh decoded caches independently taking large fractions of the same RAM pool.

Key behavior:

- Windows accounting distinguishes resident memory, process private commit, physical availability, commit availability, and a process-private ceiling.
- Process-cap subtraction is saturated, eliminating the old unsigned underflow.
- Texture and mesh decoded caches share one live budget instead of double-counting RAM.
- The live cache ceiling accounts for current cache occupancy, physical availability, commit availability, process private commit, and a protected reserve.
- New cache insertions stop under soft/critical pressure; existing entries are evicted to the live budget.
- Critical pressure temporarily overrides capture-mode full-resolution pinning and restores it after recovery.
- The reserve scales with installed RAM, so small machines can fall back to zero optional decoded-cache budget instead of being starved.
- Image-decode concurrency is capped at one worker per 2 GiB of installed RAM before CPU/configured limits.
- The emergency texture threshold scales to 5% of installed RAM, clamped to 1–8 GiB.

New setting:

```text
BDMergePoolReserveFraction = 0.25
```

The Release `app_settings\settings.xml` matches the patched source file:

```text
SHA-256: B0248F8857306F8D72CDBB6C17531FDBDB44DCBA8CFD98AD737D20E3CD09547E
```

## Source state

Tracked patch scope: 217 insertions and 107 deletions across 11 files:

- `indra/llcommon/llmemory.cpp`
- `indra/llcommon/llmemory.h`
- `indra/newview/CMakeLists.txt`
- `indra/newview/app_settings/settings.xml`
- `indra/newview/bdmergemeshpool.cpp`
- `indra/newview/bdmergemeshpool.h`
- `indra/newview/bdmergetexpool.cpp`
- `indra/newview/bdmergetexpool.h`
- `indra/newview/bdmergetexspike.cpp`
- `indra/newview/llappviewer.cpp`
- `indra/newview/llviewertexture.cpp`

New source files:

- `indra/newview/bdmergememorybudget.cpp`
- `indra/newview/bdmergememorybudget.h`

The restored memory patch is intentionally uncommitted after checkpoint `5c87c46a669`. Do not clean/reset the worktree because it contains unrelated user files.

## Build verification

Build command:

```powershell
cmake --build I:\alchemy-machinima\build-Windows-vs2026-os --config Release --target alchemy-bin --parallel 4
```

Completed checks:

- CMake regeneration succeeded.
- A compile-only pass succeeded before linking.
- Full Release compilation, link-time optimization, manifest copy, and deployment succeeded.
- `git diff --check` passes apart from existing ignore/line-ending warnings.
- Source and Release settings XML parse and match.
- ReShade DLL and INI hashes match the checkpoint after deployment.
- Post-build Release process is alive and responsive.

Final executable:

```text
Size: 81,362,944 bytes
Timestamp: 2026-08-22 15:29:33 local time
SHA-256: 474D28C1738829D57D334F78F58DEA9C4ED37FF068CB3E40C635E780B605B5A1
```

## Live validation still required

1. Confirm `End` opens/closes the ReShade overlay in the post-build session.
2. Confirm the active preset and expected depth/G-buffer effects render.
3. Exercise an ordinary region change before the original dense-region case.
4. Monitor private commit and working set during a controlled dense-region teleport.
5. Confirm `BDMergeMemory` reports a bounded combined cache budget and pressure transitions when appropriate.
6. Stop the test if private commit continues climbing without a plateau; the checkpoint is the rollback boundary.
