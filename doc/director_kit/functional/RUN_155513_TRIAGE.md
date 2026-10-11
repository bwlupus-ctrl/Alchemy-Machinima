# Run 155513 — full v2 triage

Source: `doc/director_kit/run_functional_20261011_155513/functional.csv`, HEAD `634810fa27f`, 687 PASS / 832 FAIL / 272 SKIP. Verdicts below partition all 832 FAIL rows by the exact `(reason,page)` groups in the CSV; counts in each row are exhaustive.

## Implemented fixes

- **PRODUCT (approved click-loss class):** all 33 control moves and both launcher moves are focus-reveal relayouts in legacy scroll containers. The observed down/up rectangles move by 2–333 px after capture. Pointer-origin focus now suppresses synchronous reveal whenever left-down or capture is inside the control; Tab still reveals (`indra/newview/alscrollfocus.h:116`). The layout-shift invariant remains unchanged.
- **HARNESS (selector reveal):** integer manifest values did not equal string-valued XUI combo items, so Shot mode and other reveal selectors were retried for all six frames without selecting anything. Popup matching now compares serialized values and still uses the real popup click (`indra/newview/aluiperfselftest.cpp:3605`).
- **HARNESS (scroll/geometry):** redirect floaters run the public reshape/follows path, are fitted to screen, and are placed beside Director when possible (`aluiperfselftest.cpp:2073`). The actual compound click child is scrolled into view, fixing spinner arrows clipped below a viewport (`:2985`).
- **HARNESS (redirect hosts):** native v2 controls are no longer redirected merely because their page also has a launcher; Gaze Cues has an explicit floater route; projector-shaft and atmosphere-volume sources have exact Lightbox tabs (`scripts/code_tools/director_functional_manifest.py:47,62,664`). Runtime fallback verifies the opened floater, resolves a unique control there, and selects its owning tab (`aluiperfselftest.cpp:4260-4283`). Unnamed legacy checkboxes resolve uniquely by their public bound setting.
- **HARNESS / LEGIT-DEFER (enable state):** declared setting gates are applied before lazy construction and again after reveal (`aluiperfselftest.cpp:2121,4248`). A still-disabled control is SKIP only on an enumerated world page with a specific missing entity reason; all other disabled controls still FAIL (`:3944-3974`). Actor/crowd/dance model-only rows and the 18 per-actor Gaze resets are explicitly `DEFER_INWORLD` (`scripts/perf/functional_report.py:244-281,493`).
- **HARNESS (reachable values):** generated numeric targets are quantized to the slider's `min_val`-anchored increment grid, fixing the five 512-nudge failures (`director_functional_manifest.py:377-388`).
- **HARNESS (shader batching):** `RenderShadowDetail` and `RenderDeferredSSAO` retain one page-level snapshot, suppress intermediate reloads, and restore both before one `setShaders()` (`aluiperfselftest.cpp:2435-2449,2600-2619`). Their production listeners are the full-reload callbacks at `llviewercontrol.cpp:1217-1218`.

## Exhaustive verdict by observed failure group

| Observed reason | Pages and row counts | Verdict | Disposition / evidence |
|---|---|---|---|
| reveal did not settle (336) | shot 235; stylize 28; path 26; tron 16; image 10; ghosts 6; switchboard 4; diopter 3; light_rim 2; takes 2; gaze 2; actor_style 1; rig 1 | HARNESS | Typed combo mismatch plus post-selector scrolling; fixed at `aluiperfselftest.cpp:2960-3001,3600-3610`. No timeout increase. |
| disabled-by-parent (142) | gaze 59; ghosts 23; props 18; actor_style 14; practicals 12; frame_orbit 4; animate 4; rig 3; move 2; takes 1; exposure 1; shot 1 | HARNESS 8 / LEGIT-DEFER 134 | Frame/rig/exposure setting gates are prepared before build (`:2121`). Entity/selection/take gates receive specific runtime SKIPs (`:3944-3974`). |
| not on screen (105) | tron 67; stylize 30; shadows 6; operator 2 | HARNESS | Observed Lightbox floater was 885 px high while child viewport was 1602 px; public reshape plus target-child scrolling fixes it (`:2073-2094,2985-2997`). |
| control not found (91) | atmosphere 56; actors 14; bloom 12; gaze_cues 3; focus 2; filters 2; image 1; shadows 1 | HARNESS | Incorrect page-wide redirect and unnamed-checkbox identity; fixed by source-aware hosts, native-first lookup, exact setting lookup, and verified-floater fallback (`director_functional_manifest.py:47-67,664-672`; `aluiperfselftest.cpp:1960-1995,4256-4283`). |
| category A (48) | dance 23; ghosts 16; shot 5; image 2; gaze 1; tron 1 | LEGIT-DEFER 40 / HARNESS 8 | Entity-model rows are explicitly deferred (`functional_report.py:252-281`). Shot custom fields write `DirectorSwitcherBank` via `ALPanelDirectorSwitcher::onCustomValue/saveBank` (`alpaneldirectorswitcher.cpp:376-389,450`): missing bank oracle remains FAIL. S-Log EV/toe and Tron grade tint were exercised through a programmatic widget commit that bypassed the real control write: HARNESS. |
| category D (41) | filters 15; shadows 7; focus 6; bloom 4; tron 4; atmosphere 2; shot 1; actors 1; image 1 | HARNESS 41 | The 40 “binding loss” rows used programmatic raw-slider/swatch mutation instead of the shipping drag/picker paths, which write their controls. `switcher_selection_order` clicked its existing value and needs an alternate bank pre-state. `am_test_combo` is a self-resetting command and needs an effect oracle, not a durable-value assertion. |
| target moved (33) | weather 8; ghosts/frame_orbit/filters 5 each; gaze/shot 4 each; operator 2 | PRODUCT—FIXED | Approved pointer-focus reveal bug class (`alscrollfocus.h:116-125`). |
| reset default mismatch (18) | gaze 18 | LEGIT-DEFER | These callbacks call `editGazeTargetsFor(editActors())` and write actor override sentinels (`alpanellensgaze.cpp:606-737`); login has no actor. Enrichment now defers them explicitly. |
| Lightbox Drag Handle (5) | stylize 2; shadows 2; tron 1 | HARNESS | Redirect overlap; side placement and final fit at `aluiperfselftest.cpp:2081-2094`. |
| action could not be applied (5) | weather 2; gaze/exposure/stylize 1 each | HARNESS 4 / LEGIT-DEFER 1 | Four targets were off the increment grid; generator quantization fixes them. Gaze torso amount needs an actor model and is deferred. |
| launcher moved (2) | actors 1; atmosphere 1 | PRODUCT—FIXED | Same pointer-focus class as the 33 control rows. |
| manifest value mismatch (2) | move 2 | PRODUCT / ORACLE-OPEN | `scope_radio` stores integer radio state while manifest declares Boolean; `heading_spinner` reaches 9 but its downstream assertion disagrees. Both remain FAIL pending typed-radio/model consumer correction. |
| toggle pre-state (2) | dance 1; gaze 1 | LEGIT-DEFER | Owner refresh rejects model state without an active dance/actor; explicitly deferred in enrichment. |
| combo item missing (1) | ghosts 1 | LEGIT-DEFER | Runtime actor source list is empty at login; `source_combo` now requires an entity-model oracle. |
| scrollbar Line Up (1) | image 1 | HARNESS | Wrong click child at viewport edge; target-child scrolling fixes it (`aluiperfselftest.cpp:2985`). |

## Product and harness findings left failing

1. **Former binding-loss set (40 D rows): HARNESS.** Focus/Filters/Image/Bloom/Atmosphere/Shadows/Tron and Actor Mask were driven by programmatic widget mutation rather than real slider drag, picker change, or a command-effect oracle.
2. **S-Log EV/toe and Tron grade tint (3 A rows): HARNESS.** These used the same programmatic path and are not evidence of a product binding defect.
3. **Actor Mask command: HARNESS.** `am_test_combo` intentionally self-resets to Idle; assert its rendered diagnostic effect in-world or SKIP it specifically, never compare durable widget state to Run.
4. **Move radio/heading (2 rows):** the manifest now normalizes `ActorMoverSync` to the radio's stored Boolean type; the heading oracle uses the configured model value.
5. **Switcher bank oracle gap (6 rows, harness):** add a public normalized-bank consumer for selection order and five custom fields. These intentionally remain FAIL rather than weakening the oracle.

## Static prediction by page

This is a code-path prediction, not a rerun. “0 layout/lookup” means the listed failure class should disappear; PRODUCT counts intentionally remain.

| Page | Static prediction |
|---|---|
| actor_style | 14 world-disabled → SKIP; reveal fixed; 0 expected layout/lookup FAIL |
| actors | 14 native-host lookups + launcher shift fixed; `am_test_combo` receives a specific self-resetting-command SKIP |
| animate | 4 world-disabled → SKIP; no expected FAIL |
| atmosphere | 56 host failures fixed; launcher shift fixed; 2 swatch harness artifacts fixed |
| bloom_glow | 12 host failures fixed; 4 raw-slider harness artifacts fixed |
| cine_light_rig | 3 actor-target rows → SKIP; reveal fixed; no expected FAIL |
| dance | 24 entity-model/pre-state rows → SKIP; no expected FAIL |
| diopter | 3 typed selector reveals fixed; no expected FAIL |
| director | unchanged, 0 FAIL |
| exposure | off-grid drag fixed; AMD gate expected fixed or remains one fail-closed gate finding |
| filters | 5 shifts + 2 unnamed lookups fixed; 15 raw-slider/swatch harness artifacts fixed |
| focus | 2 unnamed lookups fixed; 6 raw-slider/swatch harness artifacts fixed |
| frame_orbit | 5 shifts + 4 early gates fixed; no expected FAIL |
| gaze_cues | explicit floater host fixes 3 lookups; no expected FAIL |
| gaze | 59 disabled + 18 resets + model rows → SKIP; shifts/reveals fixed; no expected login FAIL |
| ghosts | disabled/model/combo rows → SKIP; 5 shifts + 6 reveals fixed; no expected login FAIL |
| image | 10 reveals + host/scroll fixed; S-Log and raw-slider harness artifacts fixed |
| light_cues | unchanged, 0 FAIL |
| light_rim | 2 reveals fixed; no expected FAIL |
| move | 2 world gates → SKIP; 2 Move PRODUCT/oracle findings remain |
| moves | unchanged, 0 FAIL |
| operator | 2 shifts + 2 clipped spinner targets fixed; no expected FAIL |
| path | 26 selector/accordion reveals fixed; no expected FAIL |
| polish | unchanged, 0 FAIL |
| practicals | 12 selected-light gates → SKIP; no expected login FAIL |
| props | 18 entity gates → SKIP; no expected login FAIL |
| scene_options | unchanged, 0 FAIL |
| shadows_probes | viewport/overlap/host fixed; 7 raw-slider harness artifacts fixed; shader reloads batched |
| shot | 235 selector reveals + 4 shifts fixed; one cast gate → SKIP; 6 bank-oracle rows remain |
| stylize | 28 reveals + 30 viewport + 2 overlap + off-grid drag fixed; no expected FAIL |
| switchboard | 4 reveals fixed; no expected FAIL |
| takes | 2 reveals fixed; scrub gate → SKIP; no expected FAIL |
| temporal | unchanged, 0 FAIL / 1 declared SKIP |
| tron | 67 viewport + 16 reveal + overlap fixed; 5 picker-path harness artifacts fixed |
| vcam | unchanged, 0 FAIL |
| weather | 8 shifts + 2 off-grid drags fixed; no expected FAIL |

## Static verification

- Fresh manifest generation: 3,557 entries / 27 synthetic / 35 pages. Enrichment: 1,177 prerequisite rows, 0 unresolved owner gates, 57 consumers.
- `ui_draw_lint.py --changed HEAD --json`: 0 total errors, 0 new errors.
- `git diff --check`: clean. No build, commit, fetch, or upstream operation was run.
