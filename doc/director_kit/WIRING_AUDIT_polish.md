# Wiring audit: Performance > Polish kit conversion

Static audit of the Polish page after it became a generated kit page (10/15, legacy conversion batch 1). No build, no commit, no viewer run: every row is checked against the generated XML, `settings.xml` and the C++ source. Sources: mockup page `polish_tab` (5 cards, 15 rows, unchanged), `kit_pages.json` entry `performance_group/polish_tab` (hosts the v1 nodes of `panel_pose_polish.xml`), generated `panel_dpage_performance_polish.xml`.

## Result

- The page is no longer legacy: classification `kit (P0)`, pin 12 -> 11, `performance_group/polish_tab` removed from `KNOWN_LEGACY_PAGES`.
- 15 generated rows = the 15 mockup rows, 5 cards, 2 sections, master toggle `polish_master_check` on `ALPolishEnabled`. No mockup row added or removed.
- Every control but the pop-out button is setting-bound with no C++ owner (the Pose Polish stage reads the settings directly), so there are no lookups to remap. The one C++ lookup, `polish_popout`, is bound in `LLFloaterDirector::bindCameraPage` (`llfloaterdirector.cpp:2611`, unchanged) and resolves to the generated button.
- v1 names without a row: `polish_section_*` (six static headings, replaced by card titles) and the v1 wrapper `panel_pose_polish`. The panel stays in use by `floater_pose_polish.xml` and the v1 Director; both are unchanged.

## Controls

Defaults come from `settings.xml`, ranges from the hosted v1 node (equal to the mockup row). Read-back is the kit setting binding (watch on `watch_settings`) for every setting row.

| # | widget | kind | setting / handler | type, default, range | consumer | status | Undo owner |
|---|---|---|---|---|---|---|---|
| 0 | polish_master_check (page master) | toggle | `ALPolishEnabled` | Boolean, 1 | Pose Polish stage master gate (`alposepolish.h`) | OK | n/a (not a generated row) |
| 1 | polish_popout | button | `LLFloaterReg::showInstance("pose_polish")` (`bindCameraPage`, `:2611`) | n/a | opens the plain `pose_polish` floater | OK | n/a (action/output; no widget step) |
| 2 | polish_master_note | readout | none (authored static text) | text | n/a | OK | n/a (action/output; no widget step) |
| 3 | polish_inertia_check | toggle | `ALPolishInertiaEnabled` | Boolean, 1 | inertialization (M1) | OK | kit setting history (this page; guarded against newer edits) |
| 4 | polish_inertia_spine_slider | slider | `ALPolishInertiaHalfLifeSpine` | F32, 0.13, 0.02..0.40 step 0.01, 2 dp | M1 half-life | OK | kit setting history (this page; guarded against newer edits) |
| 5 | polish_inertia_arm_slider | slider (Advanced) | `ALPolishInertiaHalfLifeArm` | F32, 0.09, 0.02..0.40 | M1 | OK | kit setting history (this page; guarded against newer edits) |
| 6 | polish_inertia_leg_slider | slider (Advanced) | `ALPolishInertiaHalfLifeLeg` | F32, 0.09, 0.02..0.40 | M1 | OK | kit setting history (this page; guarded against newer edits) |
| 7 | polish_inertia_head_slider | slider (Advanced) | `ALPolishInertiaHalfLifeHead` | F32, 0.07, 0.02..0.40 | M1 | OK | kit setting history (this page; guarded against newer edits) |
| 8 | polish_contact_check | toggle | `ALPolishContactEnabled` | Boolean, 1 | contact stabilizer (M2) | OK | kit setting history (this page; guarded against newer edits) |
| 9 | polish_contact_weight_slider | slider | `ALPolishContactWeight` | F32, 0.75, 0..1 step 0.05 | M2 blend cap | OK | kit setting history (this page; guarded against newer edits) |
| 10 | polish_phasematch_check | toggle | `ALPolishPhaseMatchEnabled` | Boolean, 1 | phase-aware locomotion | OK | kit setting history (this page; guarded against newer edits) |
| 11 | polish_loopseam_check | toggle | `ALPolishLoopSeamEnabled` | Boolean, 1 | loop-seam repair | OK | kit setting history (this page; guarded against newer edits) |
| 12 | polish_secondary_check | toggle | `ALPolishSecondaryEnabled` | Boolean, 1 | secondary motion (`alposesecondary.h`) | OK | kit setting history (this page; guarded against newer edits) |
| 13 | polish_secondary_breath_slider | slider | `ALPolishSecondaryBreathAmp` | F32, 1.0, 0..2 step 0.05 | breath | OK | kit setting history (this page; guarded against newer edits) |
| 14 | polish_secondary_sway_slider | slider | `ALPolishSecondarySwayAmp` | F32, 1.0, 0..2 step 0.05 | sway | OK | kit setting history (this page; guarded against newer edits) |
| 15 | polish_diagnostic_check | toggle (Advanced) | `ALPolishLayerDiagnostic` | Boolean, 0 | per-joint layer overlay | OK | kit setting history (this page; guarded against newer edits) |

## Checks run

`director_kit_gen.py --check` and `--selftest`, `director_page_sources.py --check`, `kit_lint.py --all`, `xui_layout_check.py --selftest` and `--changed HEAD`, `director_functional_manifest.py` plus self-tests, `director_parity.py --selftest`, `ui_draw_lint.py --changed HEAD`, `zs_check.ps1` (no C++ file changed for this page), `prebuild_guard.ps1 -ChecksOnly`. Not run (rules): build, viewer, in-world self-test.
