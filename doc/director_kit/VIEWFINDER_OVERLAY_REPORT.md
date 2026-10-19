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
