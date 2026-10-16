# Wiring audit: Light > Practicals kit conversion

Static audit of the Practicals page after it became a generated kit page (10/15, legacy conversion batch 1). No build, no commit, no viewer run: every row is checked against the generated XML and the C++ source. Before this, the page was a single launcher card that opened the Lightbox "Selected Light" tab. Sources: mockup page `practicals_tab` (3 cards, 16 rows, unchanged), `kit_pages.json` entry `light_group/practicals_tab` (hosts the nodes of `panel_practicals_src.xml`, root class `panel_practicals`), generated `panel_dpage_light_practicals.xml`.

## Result

- The page is no longer legacy: classification `kit (P0)`, pin 9 -> 8, `light_group/practicals_tab` removed from `KNOWN_LEGACY_PAGES`.
- 16 generated rows = the 16 mockup rows (3 cards, 3 sections). The v1 launcher widgets (`card_practicals*`, `practicals_flow_*`) are retired (`parity_allow`); the `card_practicals_open` branch in `LLFloaterDirector::bindLauncherPage` now finds nothing.
- New page logic class `ALPanelPracticals` (`alpanelpracticals.h/.cpp`, registered as `panel_practicals`, added to `CMakeLists.txt`). It mirrors the Lightbox "Selected Light" tab (`ALFloaterLightBox::updateSelectedLightPanel`, `onSelLight*`, `onSelGoboChanged`): same selection source, same `LLPipeline` entry points, same session-only semantics. The Lightbox is unchanged and keeps its own controls, so the two surfaces can be open together (both read the same pipeline state).
- Everything is session-only: no setting is written by this page.
- `director_parity.py` `root_owner` gained `panel_practicals` (see the Light Cues audit).

## Behavior shared with the Lightbox tab

- Follows the primary selection each frame (`LLSelectMgr`); the per-projector controls are enabled only when the primary object is a spotlight projector (`LLVOVolume::isLightSpotlight`). All writes are diff-guarded; on a change of selected object or projector state every mirror write bypasses the keyboard-focus guard so a newly selected light never shows stale values.
- With no projector selected the mirrors show neutral values (pattern off, speed and zoom 1.0) and the status reads "Select a spotlight projector in-world to edit it here."

## Controls

| # | widget | kind | handler / pipeline call | type, default, range / data values | status |
|---|---|---|---|---|---|
| 1 | sl_status | readout | written by `refreshFromSelection`: "Editing: <object name>" or the hint | text | OK |
| 2 | sl_cast_shadows | toggle | `onCastShadows` -> `LLPipeline::toggleProjectorCastShadows`; mirror `!isProjectorNoShadow` | Boolean, checked = casts shadows | OK |
| 3 | sl_volumetric | toggle | `onVolumetric` -> `toggleVolumetricShaft`; mirror `isVolumetricShaftEnabled` | Boolean, off | OK |
| 4 | sl_capture | button | `onCaptureOverride`: snapshots the 11 `BDMergeProjectorVolumetrics*` settings (same list as the Lightbox) into `setVolumetricShaftOverride` | n/a | OK |
| 5 | sl_clear | button | `onClearOverride` -> `clearVolumetricShaftOverride` and `clearGoboOverride`; enabled by `hasVolumetricShaftOverride || hasGoboOverride` | n/a | OK |
| 6 | sl_gobo | combo | `onGoboChanged` -> `GoboOverride::mPattern` | -1 Off (texture), 0 Venetian blinds, 1 Window, 2 Grate, 3 Ceiling fan, 4 Honeycomb, 5 Foliage, 6 Water caustics, 7 Prison bars, 8 Arched window, 9 Iris, 10 Dot grid, 11 Rings, 12 Clouds (same values as the Lightbox combo) | OK |
| 7 | sl_gobo_animmode | combo | `mAnimMode` | 0 None, 1 Rotate, 2 Wind sway, 3 Pan, 4 Pulse, 5 Flicker, 6 Strobe | OK |
| 8 | sl_gobo_speed | slider | `mSpeed` | F32, -8..8 step 0.05, 2 dp, mirror default 1.0 | OK |
| 9 | sl_gobo_zoom | slider | `mZoom` | F32, 0.1..8 step 0.05, 2 dp, mirror default 1.0 | OK |
| 10 | sl_gobo_tint | color | `mTint` (RGB of the swatch; applies while the picker is dragged) | white = no tint | OK |
| 11 | sl_gobo_soft | slider (Advanced section) | `mPatternParams.z` | F32, 0..1 step 0.05, 2 dp | OK |
| 12 | sl_gobo_dispersion | slider | `mDispersion` | F32, 0..0.1 step 0.001, 3 dp | OK |
| 13 | sl_gobo_var1 | slider | `mPatternParams.x` (Detail) | F32, -1..1 step 0.05, 2 dp | OK |
| 14 | sl_gobo_var2 | slider | `mPatternParams.y` (Thickness) | F32, -1..1 step 0.05, 2 dp | OK |
| 15 | sl_gobo_invert | toggle | `mPatternParams.w` (>= 0.5 is inverted) | Boolean | OK |
| 16 | open_lightbox_sellight | button | kit action `lightbox:sellight_settings` (opens the Lightbox tab) | n/a | OK |

The ranges equal the Lightbox controls and the `GoboOverride` consumer (the pipeline clamps on use). Rows 4 and 5 use the kit's code-bound button; nothing in the page watches a setting.

## Checks run

`director_kit_gen.py` check and selftest, `director_page_sources.py --check`, `kit_lint.py --all`, `xui_layout_check.py` selftest and `--changed HEAD`, `director_functional_manifest.py` plus self-tests, `director_parity.py` `--selftest` and `--groups light_group`, `ui_draw_lint.py --changed HEAD` (0 new), `zs_check.ps1` (`alpanelpracticals.cpp`), `prebuild_guard.ps1 -ChecksOnly`. Not run (rules): build, viewer, in-world self-test. Worth an in-world look: select a spotlight projector and edit each gobo row; open the Lightbox tab beside the page to confirm both mirror the same state.
