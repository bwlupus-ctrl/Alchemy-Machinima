# Viewfinder overlay

Built 2026-10-19. Roll line added 2026-10-19 (follow-up). Not built into a viewer, not run in-world.

## Where the user clicks

Director > **Camera** (rail group `camera_group`) > **Frame & Orbit** (`frame_orbit_tab`) > section **Frame** > card **Frame format**:

| Row | Widget | Setting | Notes |
| --- | --- | --- | --- |
| Viewfinder | `viewfinder_enabled` (switch) | `CinematicViewfinderEnabled` (Boolean, default off) | Help: "Corner brackets and a centre crosshair with live FOV, view angle, focal length, camera roll and FPS. Stays visible when the UI is hidden; never in snapshots." |
| Size | `viewfinder_scale` (slider 0.5 to 4.0, step 0.05, 2 decimals) | `CinematicViewfinderScale` (F32, default 1.0) | Enabled only while Viewfinder is on (`enabled_control` in `kit_pages.json`). Help: "Scales the viewfinder text, lines and crosshair for your screen resolution." |

Both rows sit under "Bar opacity", above "Lens reframe". Searchable with: viewfinder crosshair fov view angle focal length roll fps hud overlay.

## What it draws

Thin corner brackets, a small centre "+", and a five-line labelled block just below-right of the crosshair. Labels and values are two aligned columns in the same small font with a drop shadow:

```
FOV   33.7°
View  0.588
Lens  40 mm
Roll  -12.5°
FPS   30
```

- FOV: vertical FOV in degrees, 1 decimal.
- View: raw view angle (zoom) in radians, 3 decimals (the value the View angle slider and Ctrl+0/8/9 change). Labelled "View" so it is not confused with roll.
- Lens: 35 mm-equivalent focal length, `f = 12 / tan(fov / 2)` (24 mm sensor height), whole mm. Worked example: 0.588 rad = 33.69 deg, f = 39.63 mm, shown as "40 mm" (rounded, not truncated).
- Roll: camera roll from `LLAgentCamera::getRollAngle()` (the Shift+Q / Shift+E roll; new const accessor on `mRollAngle`, radians, accumulates unwrapped). Shown in degrees, 1 decimal, negative values with a minus sign and no plus sign for positive, wrapped to -180..180 (190 deg reads -170.0, -190 reads 170.0, 180 stays 180.0). A tiny negative never prints "-0.0".
- FPS: same source as the status-bar FPS (`FPS_SAMPLE` last-recording mean), sampled every 0.1 s, whole number.

Brackets sit inside the frame-guide rect when a target aspect is active (same computation as the guide, now shared as `cinematicTargetFrameRect`), otherwise inside the world view. Margin 12 UI units at Size 1.0.

## Size

One multiplier scales everything together, on top of the UI scale: line thickness, bracket arm length and margin, crosshair, text offset from the crosshair, column gap, and the font. Lines are thin quads (wide GL lines are not portable). The font comes from a ladder of 13 named point sizes (`Viewfinder0` to `Viewfinder12` in `fonts.xml`, each a 2^(1/4) step, 4.5 pt at 0.5x to 36 pt at 4x, `Viewfinder4` = 9 pt = the Small UI font), so glyphs are rasterised at real size, not stretched. Only the glyph size snaps to the nearest rung (at most about 9%); layout uses the exact scale and measures the real font.

## Visibility

Drawn from `LLViewerWindow::drawDebugText()`, which `render_ui()` runs after both UI-hiding mechanisms (HideUI and the Ctrl+Alt+F1 UI feature mask), so it stays visible with the UI hidden. Skipped when `gSnapshot` is set, so it never reaches snapshots. Main view only; `render_ui()` is not called for probes, mirrors or cube faces. Cost when off: one cached-control test.

## Files

- `indra/newview/alviewfinder.h` (new): pure formatting, roll wrap, layout and font-step helpers.
- `indra/newview/tests/alviewfinder_test.cpp` (new) plus `CMakeLists.txt` registration: readout, roll wrap and tests at Size 0.5 / 1 / 4.
- `indra/newview/llviewerwindow.cpp`: shared frame rect, `drawCinematicViewfinder`, call in `drawDebugText`.
- `indra/newview/llagentcamera.h`: `getRollAngle() const`.
- `indra/newview/app_settings/settings.xml`, `skins/default/xui/en/fonts.xml`.
- `scripts/code_tools/director_kit/director_mockup_data.json`, `kit_pages.json`, `scripts/code_tools/director_kit_gen.py` (row pin 2133) and the regenerated kit outputs.

## Not verified

No viewer build or in-world run: the overlay look, the font-rung creation hitch the first time a new Size rung is used, the on-top-of-floaters draw order with UI visible, and that the roll sign matches what the user sees when pressing Shift+E.

## Hotkey

Added 2026-10-20. Default: **Ctrl+Alt+V** (`toggle_viewfinder`). Rebind
**Toggle Viewfinder** in **Preferences > Controls**. The row is in the shared
movement controls table so it remains available in first-person Controls as
well as the other modes (the Camera group is hidden in first person).
`LLKeyConflictHandler` loads the command from `key_bindings.xml` into its normal
assignable command map; no special conflict-handler exception is needed.

The regular `REGISTER_KEYBOARD_ACTION` handler flips
`CinematicViewfinderEnabled` on key-down only. It uses the same setting as the
Director switch, so the switch and Size enable state follow external changes.
The normal keyboard action path suppresses repeats and has no UI-visible or
flycam restriction. Defaults cover all four binding modes: first person, third
person, sitting and edit avatar. Flycam uses the current one of these modes.
This also lets the overlay be switched with the UI hidden.

Conflict check: before adding the binding, no mode had any V binding with
NONE, CTL, ALT or CTL_ALT (including modifier-subset matches in the keyboard
scanner). All shipped skin XUI shortcuts were scanned: the only V shortcuts
were Paste (Ctrl+V), with no Ctrl+Alt+V accelerator. The final XML has exactly
one Ctrl+Alt+V binding per mode, exclusively `toggle_viewfinder`.

The Director help now says: "Toggle with Ctrl+Alt+V (change in Preferences >
Controls)." Generated output headers changed with the source-data hash; no
Director rows or pins changed. Page-source and functional-manifest regeneration
produced no tracked content changes.

Gate results:

- `zs_check.ps1 -Files indra/newview/llviewerinput.cpp`: PASS (1 TU, 0 failures;
  checked in its own call; migration validation below also checks llkeyconflict.cpp).
- `director_kit_gen.py --check`, `director_page_sources.py --check` and
  `director_functional_manifest.py --check`: PASS after the C++ edit and
  regeneration.
- `prebuild_guard.ps1 -ChecksOnly`: PASS, all UI/layout, draw, compile, maths,
  kit generation/lint/parity, page-source and theme gates. Run with
  `GIT_CONFIG_COUNT=1`, `GIT_CONFIG_KEY_0=core.safecrlf`,
  `GIT_CONFIG_VALUE_0=false`; a process-local Git global-config path retained
  the sandbox's safe-directory access, and the syntax-cache path was redirected
  inside the workspace.
- `git ls-files --eol scripts/code_tools/director_kit/director_mockup_data.json`:
  `i/lf w/lf attr/text eol=lf`.
- `git diff --check`: PASS.

No viewer build or in-world input test was performed. No commit, push, stash,
pipeline or shader changes.

Hotkey migration P1 fixes, 2026-10-20 (review
`.tmp/review_codex_vfkey_r4.md`):

Runtime decides shipped-default fallback from the original user file's binding
count, before migration. Zero-binding files receive no Viewfinder migration and
still return zero. Preferences > Controls uses the exact runtime binding-count
rules on a local input instance before applying the same migration. Parse or
validation failures receive no migration; omitted modes remain omitted.

`KeyBindingsMigrationVersion` is a new persisted S32 setting, default 0, in
`app_settings/settings.xml` / `gSavedSettings`. Its global user scope matches
`LL_PATH_USER_SETTINGS/key_bindings.xml` (the key file is not per-account).
While the marker is below 2 and the original file yielded at least one binding,
only `toggle_viewfinder` is eligible for the shipped Ctrl+Alt+V default in each
provided mode. Any existing V binding blocks insertion, regardless of mask,
including SHIFT and invalid masks. Existing custom or explicitly empty
Viewfinder entries also block insertion. No unrelated command is restored.
Marker values 2 or newer receive no additions, independently of the XML version.

The marker becomes 2 only after a permanent Controls key-file save opens and
writes the file. Temporary preview saves and loads do not set it. Clearing
Viewfinder and saving therefore keeps it absent on subsequent loads; a user who
never saves continues to receive the eligible in-memory default each session.
Migration itself does not write bindings or alter the key-file format/version.

HEAD ce591b11f94's version semantics are restored exactly: the shipped
`xml_version` and `keybindings_xml_version` remain 1; unversioned input defaults
to 0. The entire original runtime loader, including its legacy default-LMB
handling, missing-LMB additions and version-1 rewrite, is unchanged before the
new migration tail. Runtime loads only the newly added Viewfinder bindings after
that legacy upgrade, keeping them out of its file write and preserving the
original fallback count. This retains HEAD's version-0 script left-click behavior,
including after a Controls save. `LLKeyConflictHandler::saveToSettings` is exactly
HEAD's implementation apart from setting the persisted marker after a permanent
write. It uses the original parser and `keys.xml_version.set(keybindings_xml_version,
true)` with no migration during save.

`scripts/code_tools/test_key_binding_migration.py` extracts and executes the
production pure rule with parameter/parser doubles. Coverage includes zero-binding
files, markers 0/1/2/3, a cleared command with marker >=2, free defaults, every V
modifier combination and invalid masks, custom and empty Viewfinder entries,
unrelated omissions, omitted defaults, repeated migration, and collecting only
new runtime additions. Source regression checks use `git show HEAD:` to require
the entire original runtime loader before the migration tail and the entire
Controls save function apart from the marker. They also verify unchanged version
defaults/constants, shipped format attributes and a persisted global marker.
These tests do not execute the viewer's XML parser or in-world Controls UI.

Validation: pure migration test PASS. Separate `zs_check.ps1 -Files` calls for
`llviewerinput.cpp` and `llkeyconflict.cpp` PASS (1 TU each, 0 failures).
`prebuild_guard.ps1 -ChecksOnly` PASS: all UI/layout, draw, compile, maths, kit,
page-source and theme checks. Run with `GIT_CONFIG_COUNT=1`,
`GIT_CONFIG_KEY_0=core.safecrlf`, `GIT_CONFIG_VALUE_0=false`, the existing
workspace Git config and syntax cache. The new setting is appended so existing
setting declaration references remain unchanged. `git diff --check` PASS.
No viewer build, commit, push or stash.
