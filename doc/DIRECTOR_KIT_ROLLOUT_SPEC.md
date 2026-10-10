# Director Kit: evidence-backed all-at-once rollout

Status: R4 user decisions applied for the work after `DIRECTOR_KIT_P0_SPEC.md` WP5.
Branch: `feature/director-kit-p0`.
Data authority: `scripts/code_tools/director_kit/director_mockup_data.json` (35 pages, 1,839 rows: 1,509 card rows plus 330 selector-option rows).

## 1. Source authority and ship boundary

Director v2 remains default-off and unavailable from the normal menu until WP12 passes. There is no user-visible partial conversion. The v1 Director, Lightbox, standalone cue/Diopter/Snapshot floaters, and shared panels remain behaviorally intact while v2 is built.

Page ownership is not inferred from launcher cards or rollout prose. Run:

```text
<python> scripts/code_tools/director_page_sources.py
<python> scripts/code_tools/director_page_sources.py --check
```

The script independently counts and then deterministically walks every card row and every `opts[*].rows` selector row. It scans them against English XUI, both element- and `*.function=` attribute-form callbacks, scoped owning C++ classes, `llformat`/printf and concatenated-name patterns, and the Global/PerAccount settings graph including `Include` files. Its checked outputs are:

- [`page_sources.json`](director_kit/page_sources.json), the machine authority;
- [`page_sources.md`](director_kit/page_sources.md), the readable evidence;
- the 35 linked page contracts in section 3.

`--check` fails on any stale output, an independent JSON/flattened row-count mismatch, a conflict/home/shortcut-set mismatch, a selector option missing `value`, a label/value pair that does not round-trip to the matching v1 combo item, or any generated placeholder. The Shot check covers all 50 `CinematicCamMode` items, including the 39→41 gap. A missing callback/model binding or settings declaration remains an explicit `UNRESOLVED` row with its reason; generation never converts it into a guessed binding. Bare-name hits outside the page's XUI-owner/class scope are recorded as rejected candidates, not bindings; genuine in-scope ambiguity has its own explicit list.

The current evidence reports 35 pages, **1,839 rows**, 31 scoped XUI owner files (including full-editor owners whose approved rows are summaries), **0 unresolved rows**, **0 ambiguous rows**, and **22 settings** written by more than one Director page. Shot is 334 rows, Rig is 174, and Rim is 41; their selector options are part of parity, audit manifests, and completeness gates.

## 2. Logic and adapter contract

The computed classification is binding: a page is `pure-setting` only when at least 90% of its rows resolve to declared settings and no row has a scoped C++ widget reference or registered commit handler. Every other page is `code-driven`. Under this rule Focus, Bloom & Glow, and Shadows & Probes are pure-setting in the current tree. Ordinary `gSavedSettings`/`LLCachedControl` string reads and out-of-owner name collisions do not make a page code-driven.

Each code-driven page owns the adapter named in its page contract. The adapter:

1. caches widget handles once in `attach()`;
2. uses direct `ALKitSettingBinding` only for rows whose declaration is proven in `page_sources.json`;
3. calls, or extracts into a shared controller, the exact existing functions listed row-by-row;
4. rewires the v1/non-Director owner to the same controller in the same reviewed change;
5. subscribes to setting commits and model revisions and forces a diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer;
6. performs no per-frame setting reads, by-name lookups, list rebuilds, signature construction, or raw widget writes in `draw()`.

Shared editors claim on reveal/focus/user action with an owner token. They release only their own token. Light Cues and Gaze Cues explicitly have two possible live editors: the standalone floater and the Director page. Cue playback places one `ALKitHistory::globalBarrier(reason)` at start and stop and suspends recording between them; preset apply places one barrier per apply.

## 3. Generated page matrix

The row count, owner, class, handlers, unresolved rows, live-state contract, bounded audit manifest, parity procedure, and risks for each page are in the linked file. The matrix is a view of `page_sources.json`, not an independent source list.

| Group | Page contract | Rows | Computed class | Primary real owner / logic class |
|---|---|---:|---|---|
| Cast | [Move](director_kit/pages/move_tab.md) | 19 | code-driven | `panel_actor_mover.xml`; `LLFloaterDirector`, `ALPanelActorMover` |
| Cast | [Path](director_kit/pages/path_tab.md) | 62 | code-driven | `panel_path_editor.xml`; `ALPanelPathEditor` |
| Cast | [Props](director_kit/pages/props_tab.md) | 33 | code-driven | `panel_prop_mover.xml`; `ALPanelPropMover` |
| Cast | [Ghosts](director_kit/pages/ghosts_tab.md) | 125 | code-driven | `panel_ghost_studio.xml`; `ALPanelGhostStudio` |
| Cast | [Dance](director_kit/pages/dance_tab.md) | 82 | code-driven | `panel_dance_ball.xml`; `ALPanelDanceBall` |
| Performance | [Animate](director_kit/pages/animate_tab.md) | 24 | code-driven | `floater_director.xml` + `panel_anim_preview.xml`; `LLFloaterDirector`, `ALPanelAnimPreview` |
| Performance | [Switchboard](director_kit/pages/switchboard_tab.md) | 32 | code-driven | `panel_director_anim_switcher.xml`; `ALPanelDirectorAnimSwitcher` |
| Performance | [Gaze](director_kit/pages/gaze_tab.md) | 123 | code-driven | `panel_lens_gaze.xml`; `ALPanelLensGaze` |
| Performance | [Polish](director_kit/pages/polish_tab.md) | 15 | code-driven | `panel_pose_polish.xml` plus Director action |
| Performance | [Gaze Cues](director_kit/pages/gaze_cues_tab.md) | 5 | code-driven | `floater_gaze_cues.xml`; `ALFloaterGazeCues` |
| Camera | [Shot](director_kit/pages/shot_tab.md) | 334 | code-driven | `panel_director_switcher.xml` + `panel_cinecam_params.xml`; switcher/camera classes |
| Camera | [Moves](director_kit/pages/moves_tab.md) | 14 | code-driven | `floater_director.xml`; Director generator handlers |
| Camera | [Operator](director_kit/pages/operator_tab.md) | 69 | code-driven | `panel_cinecam_operator.xml`; `LLCameraOperator`, `ALPanelCineCamParams` |
| Camera | [Frame & Orbit](director_kit/pages/frame_orbit_tab.md) | 31 | code-driven | `panel_cinecam_frame.xml` + `panel_flycam_orbit.xml`; `LLViewerJoystick`, `LLCinematicCamera`, Director |
| Camera | [Takes](director_kit/pages/takes_tab.md) | 20 | code-driven | `panel_flycam_recorder.xml`; recorder/joystick classes |
| Camera | [VCam](director_kit/pages/vcam_tab.md) | 3 | code-driven | frozen Director summary + Prism manager; `ALVCamKitAdapter` |
| Lens | [Focus](director_kit/pages/focus_tab.md) | 14 | pure-setting | `panel_dof.xml`; direct settings |
| Lens | [Filters](director_kit/pages/filters_tab.md) | 29 | code-driven | `panel_lens_looks.xml`; `ALPanelLensLooks` |
| Lens | [Diopter](director_kit/pages/diopter_tab.md) | 5 | code-driven | `floater_ultimate_diopter.xml`; `ALFloaterUltimateDiopter` |
| Light | [Rig](director_kit/pages/cine_light_rig_tab.md) | 174 | code-driven | `panel_cine_light_rig.xml`; `ALPanelCineLightRig` |
| Light | [Rim](director_kit/pages/light_rim_tab.md) | 41 | code-driven | `panel_cine_light_rig.xml`; `ALPanelCineLightRig` |
| Light | [Exposure](director_kit/pages/exposure_tab.md) | 25 | code-driven | `panel_exposure_hub.xml`; `ALPanelExposureHub` |
| Light | [Practicals](director_kit/pages/practicals_tab.md) | 16 | code-driven | `floater_lightbox_settings.xml`; Lightbox selected-light handlers |
| Light | [Actors](director_kit/pages/actors_tab.md) | 28 | code-driven | `panel_avatar_light_response.xml` + Lightbox; actor-response classes |
| Light | [Light Cues](director_kit/pages/light_cues_tab.md) | 5 | code-driven | `floater_cine_light_cues.xml`; `ALFloaterCineLightCues` |
| Look | [Image](director_kit/pages/image_tab.md) | 74 | code-driven | `floater_lightbox_settings.xml`; `ALFloaterLightBox` |
| Look | [Bloom & Glow](director_kit/pages/bloom_glow_tab.md) | 17 | pure-setting | Lightbox controls; direct settings |
| Look | [Roto Ink](director_kit/pages/stylize_tab.md) | 81 | code-driven | `floater_lightbox_settings.xml`; `ALFloaterLightBox` |
| Look | [Tron](director_kit/pages/tron_tab.md) | 132 | code-driven | `floater_lightbox_settings.xml`; `ALFloaterLightBox` |
| Look | [Actor FX](director_kit/pages/actor_style_tab.md) | 18 | code-driven | `floater_director.xml`; `LLFloaterDirector::refreshActorStyleTab` path |
| World | [Weather](director_kit/pages/weather_tab.md) | 51 | code-driven | `panel_weather_settings.xml`; weather handlers/settings consumers |
| World | [Atmosphere](director_kit/pages/atmosphere_tab.md) | 80 | code-driven | atmospheric-volume/projector panels + Lightbox; `ALPanelAtmosphericVolumes` |
| World | [Shadows & Probes](director_kit/pages/shadows_probes_tab.md) | 39 | pure-setting | `floater_lightbox_settings.xml`; direct settings |
| Capture | [Time](director_kit/pages/temporal_tab.md) | 16 | code-driven | `panel_temporal_capture.xml`; `ALPanelTemporalCapture` |
| Capture | [Scene](director_kit/pages/scene_options_tab.md) | 3 | code-driven | Director scene state |

Count gate: exactly 35 links and page files. A zero-row page, unresolved executable row, stale source index, or unapproved page key is a hard failure.

## 4. Existing-control and custom-widget policy

The invented `path_canvas`, `curve_editor`, `slot_grid`, `asset_picker`, and generic `preview_view` kinds are removed from the rollout. Path uses its waypoint list and existing buttons/spinners; cue stacks use their lists and stock editors; Switchboard and Shot use the existing slot buttons; Takes uses its recorder list/scrubber; Exposure uses its existing panel controls.

An existing complex control may be extracted into an `embed` only when its page contract says so. An embed is full card width, has `minHeightForWidth()`/`preferredHeightForWidth()`, is 160–320 px high, contains no page-level scroller, clips to its measured rect, enumerates its states, and uses an owner token. Existing Ghost preview/diagnostics are the only candidates in this snapshot.

There are currently **no approved new widget classes**. Light Cues and Gaze Cues use stock lists and buttons; their detailed editors remain in the existing floaters. Diopter exposes its four approved everyday settings and opens the existing 560×720 editor for everything else. Capture is deliberately absent from Director v2; Snapshot remains in its existing floater.

## 5. One-home conflict resolution

Within Director v2 one page is the canonical writable home for each setting below. Every other occurrence is a `link`/read-only row except a declared **SHORTCUT** in an Easy card or selector mode-specific block. A shortcut binds the same setting, carries `shortcut_of=<home page/row>`, names the home in help, and is generated with the home row's kind/range/step/decimals/units. This deliberately normalizes the legacy Dolly/Arc 0.5 s lower bounds to their Moves homes' 0.1 s bounds without changing the source snapshot. Search and undo treat the home as canonical; amber/reset flow through the shared setting watch. `kit_lint` permits only the explicit shortcut rows below and rejects any other second writer or emitted metadata drift. Existing non-Director XUI remains a compatibility writer during rollout and is a parity oracle; it is not a second Director home.

| Setting | Writable Director home | Non-home Director occurrence | Existing non-Director writer(s) retained for parity |
|---|---|---|---|
| `ActorMoverEndMode` | [Move](director_kit/pages/move_tab.md) | Props | `panel_actor_mover#end_mode_combo` |
| `AlchemyRenderTonemapType` | [Exposure](director_kit/pages/exposure_tab.md) | Image | Lightbox, Exposure Hub, advanced graphics prefs |
| `AlchemyToneMapAMDExposure` | [Exposure](director_kit/pages/exposure_tab.md) | Image | Lightbox, Exposure Hub |
| `AlchemyToneMapSLogEV` | [Exposure](director_kit/pages/exposure_tab.md) | Image | Lightbox, Exposure Hub |
| `AlchemyTonemapLegacyGammaSkies` | [Exposure](director_kit/pages/exposure_tab.md) | Image | Lightbox, Exposure Hub |
| `BDMergeProjectorVolumetricsFeather` | [Atmosphere](director_kit/pages/atmosphere_tab.md), row `ps_BDMergeProjectorVolumetricsFeather_slider` | **SHORTCUT:** Rig Easy `cine_easy_cone_feather` | Projector Shafts panel |
| `BDMergeProjectorVolumetricsMaxDistance` | [Atmosphere](director_kit/pages/atmosphere_tab.md), row `ps_BDMergeProjectorVolumetricsMaxDistance_slider` | **SHORTCUT:** Rig Easy `cine_easy_shaft_length` | Projector Shafts panel, Cine Rig easy shaft length |
| `CineRigRimTronColorSource` | [Rim](director_kit/pages/light_rim_tab.md) | Tron | Lightbox, Cine Rig |
| `CineRigRimTronMix` | [Rim](director_kit/pages/light_rim_tab.md) | Tron | Lightbox, Cine Rig |
| `CineRigRimTronMode` | [Rim](director_kit/pages/light_rim_tab.md) | Tron | Lightbox, Cine Rig |
| `CinematicCamArcDuration` | [Moves](director_kit/pages/moves_tab.md), row `hero_arc_duration` | **SHORTCUT:** Shot / Arc Move `CinematicCamArcDuration` | Director camera move controls |
| `CinematicCamArcGeneratorAngle` | [Moves](director_kit/pages/moves_tab.md), row `hero_arc_angle` | **SHORTCUT:** Shot / Arc Move `CinematicCamArcGeneratorAngle` | Director camera move controls |
| `CinematicCamArcHeightDrift` | [Moves](director_kit/pages/moves_tab.md), row `hero_arc_height_drift` | **SHORTCUT:** Shot / Arc Move `CinematicCamArcHeightDrift` | Director camera move controls |
| `CinematicCamFrameOffsetUp` | [Frame & Orbit](director_kit/pages/frame_orbit_tab.md) | Operator | Cine Camera Frame, Cine Camera Operator |
| `CinematicCamJoint` | [Frame & Orbit](director_kit/pages/frame_orbit_tab.md), row `CinematicCamJoint` | **SHORTCUT:** Shot / Bone Lock `CinematicCamJoint` | Cine Camera Frame/Params |
| `CinematicCamUseSelected` | [Frame & Orbit](director_kit/pages/frame_orbit_tab.md) | Shot | Cine Camera Params, Flycam Orbit |
| `CinematicCamVertigoDirection` | [Moves](director_kit/pages/moves_tab.md), row `zolly_direction` | **SHORTCUT:** Shot / Dolly Zoom `CinematicCamVertigoDirection` | Director camera move controls |
| `CinematicCamVertigoDuration` | [Moves](director_kit/pages/moves_tab.md), row `zolly_duration` | **SHORTCUT:** Shot / Dolly Zoom `CinematicCamVertigoDuration` | Director camera move controls |
| `RenderBloomStrength` | [Bloom & Glow](director_kit/pages/bloom_glow_tab.md) | Exposure | Exposure Hub |
| `RenderBloomThreshold` | [Bloom & Glow](director_kit/pages/bloom_glow_tab.md) | Exposure | Exposure Hub |
| `RenderExposure` | [Exposure](director_kit/pages/exposure_tab.md) | Image | Lightbox, Exposure Hub, Display/advanced prefs |
| `TemporalWorldScale` | [Time](director_kit/pages/temporal_tab.md) | Ghosts | Temporal Capture panel slider/spinner |

Count: **22 current homes chosen; 8 declared shortcuts**. The generator compares the exact computed duplicate-setting set with `WRITABLE_HOMES` and the exact `(page,row)->home` shortcut set with `SHORTCUTS`; either drift fails. `CinematicCamUseSelected` remains an ordinary non-home Shot occurrence, not a shortcut.

## 6. Bounded audit and parity

Each page contract contains an explicit state manifest. A manifest is capped at 256 named states and two minutes per page; the suite is capped at 72 minutes. Every state starts from the same default snapshot, changes only the named local condition, performs synchronous kit layout, waits one idle for handle-guarded refresh, audits, screenshots on failure, and restores under `ALKitHistory::ScopedSuspend`. There is no Cartesian product.

The manifests explicitly cover the three non-default shell sizes, every card alone in its tallest state at its minimum and maximum card width, every declared combo option alone, each toggle's dependent-visible state, list empty/one/51-item states, target/selection loss, owner-token transfer, applicable permission/offline state, cue playback, and VCam capture-count/preview states. The default 1160×720 state is the baseline parity snapshot. The four required sizes remain 1160×720, 850×560, 1141×720, and 915×720.

Parity uses the real owners in `page_sources.json`, not “v1 leaf plus filename embeds.” For each row it compares widget class/presentation kind, initial value, enabled/visible state, option/list population, commit result, reset/default/amber, and undo/redo. It invokes each extracted handler through both the old owner and the kit adapter against the same model snapshot. Runtime instrumentation records `(page, widget, requested class, actual class, owner root, adapter handler)` and is compared to static extraction.

This parity explicitly instantiates Lightbox for Tron, Roto, Image, Shadows, Practicals, and its Atmosphere/Actors rows; `ALFloaterGazeCues`, `ALFloaterCineLightCues`, and `ALFloaterUltimateDiopter`; and all shared panels. A launcher card is never an oracle for controls it does not own. Two simultaneously live owners must pass owner-token, external-write synchronization, hide/reveal, and close/reopen tests.

## 7. Work packages after WP5

These are review boundaries inside one default-off delivery. Every extraction receives an adversarial behavior diff for the v1/non-Director forwarding call.

### WP6 — evidence, generation, adapters, and complete data

Land the source-index/check gate, setting graph, one-home map, adapter/controller interfaces, owner tokens, history barrier, generated pages, and bounded audit runner. Resolve every executable unresolved row. The approved Diopter summary and both cue-list contracts are part of this package; Capture is excluded by generator configuration.

### WP7 — Cast

Convert the five Cast pages and their actor/path/prop/ghost/dance controllers. Review `ALPanelActorMover`, `ALPanelPathEditor`, `ALPanelPropMover`, `ALPanelGhostStudio`, `ALPanelDanceBall`, and the Move logic in `llfloaterdirector.*`. No path canvas is added.

### WP8 — Performance

Convert Animate, Switchboard, Gaze, Polish, and Gaze Cues. Review `llfloaterdirector.*`, animation preview/switcher, gaze/polish owners, and `alfloatergazecues.*`. The standalone Gaze Cues floater remains a shared owner; no curve or slot-grid widget is added.

Gaze Cues Play/Stop includes scoped engine work because the existing Director transport has `LLDirectorCast::action()`/`cut()` but no presentation-time seek (`lldirectorcast.h:415-421`), and `LLFlycamRecorder::seek(F32)` is recorder-only. WP8 adds exactly `bool LLActorMover::setGazeCuePreview(const LLUUID& actor_id, S32 cue_index);` and `void LLActorMover::clearGazeCuePreview(const LLUUID& actor_id);`, consulted before ordinary cue evaluation at `llactormover.cpp:6649-6668`. It never rewrites cue times or the global clock; invalid actor/index is a no-op/false, and Stop/removal/logout clears preview/runtime. `evaluateGazeCues()` remains a per-frame pure helper, not a command.

### WP9 — Camera, Lens, and VCam integration

Convert all six Camera and three Lens pages and review switcher, camera params/operator/frame/orbit/recorder, `llfloaterdirector.*`, Diopter, and Prism call sites. VCam packages are named **VC1–VC7**, never WP1–WP7: VC1–VC5 are engine work and may precede WP6; VC6 is the Director kit page integrated here after adapter infrastructure; VC7 is VCam convergence evidence consumed by WP12. VCam migration tests cover `DirectorLastTabName`, v1/v2 rect carry-over, and Camera-group placement.

### WP10 — Light and Look

Convert six Light and five Look pages. Review `alfloaterlightbox.*`, `alpanelcinelightrig.*`, Exposure, avatar-light response, both cue owners, `llfloaterdirector.*` Actor FX logic, and every preset/visibility callback listed in the page contracts. Lightbox parity is mandatory. No curve editor is added.

### WP11 — World, Time, and Scene

Convert Weather, Atmosphere, Shadows & Probes, Time, and Scene. Review Weather, Atmospheric Volumes, Projector Shafts, Lightbox shadow/atmosphere rows, Temporal Capture, and Director scene state. Snapshot stays in its existing floater and is not a Director v2 page.

### WP12 — convergence and ship gate

Run source generation/check, page/data lint, static/runtime parity, settings/home lint, all bounded state manifests, XUI layout self-tests, UI-draw lint, unit tests, performance gates, and default-v1 regression. Archive state manifests and failure screenshots. Only after adversarial review reaches zero findings, make one fresh build and run the 35-page in-world matrix; only then may normal v2 selection be exposed.

## 8. Review R2 resolution (rollout)

| R2 finding | Resolution in this rewrite |
|---|---|
| N2 wrong owners/classes | `page_sources.json` replaces handwritten sources; matrix and page contracts link real Lightbox, Director, panel, cue, and Diopter owners. Snapshot remains an external window, not a Director page. |
| N3 invented custom kinds | Removed path canvas, curve editor, slot grid, asset picker, and generic preview inventions; existing controls or bounded embeds only. |
| M-R6 one-home contradiction | Section 5 chooses all 22 conflicts from the complete 1,839-row walk and converts non-home Director rows to links/read-only; the generator fails on conflict/home drift. |
| M-R7 unbounded Cartesian audit | Per-page explicit isolated manifests, 256-state hard cap, two-minute page cap, 72-minute suite cap. |
| M-R9 external history barrier | Global lookup at preset apply and cue start/stop; recording suspended for playback duration, never barrier-per-write. |
| M-R10 one-line code pages | All 35 page files contain row bindings, exact handler anchors, adapter extraction, refresh, states, parity, and risks. |
| M-R11 WP collision | Rollout remains WP6–WP12; VCam is VC1–VC7 and is ordered into WP9. |
| Minor cue-owner collision | Both standalone cue floaters and Director adapters are named shared owners with owner-token states. |
| Parity blind to non-Director owners | Section 6 requires Lightbox, cue/Diopter floaters, and shared panels as parity oracles. |

## 9. Review R3 resolution

| R3 finding | Resolution |
|---|---|
| B1 selector-option rows | The walker includes every `cards[*].rows` and `cards[*].opts[*].rows`, records option label/index, and independently recounts the JSON. Totals are 1,839 overall: Shot 334, Rig 174, Rim 41. |
| M1 callbacks and dynamic names | XUI scanning accepts nested callback elements and any `*.function=` attribute. C++ scanning expands printf/`llformat` patterns and constant/array prefix concatenations; the 24 slot buttons and role-indexed Rig names resolve. Links and target-bearing launchers are kit-native actions. |
| M2 owner collisions | Each page has explicit XUI-root and owning-class scope. Settings reads are excluded from widget evidence; out-of-scope same-name hits are retained as rejected candidates, and the explicit ambiguous list is empty. |
| M3 exact behavior | Generated bindings name behavior and wiring functions with file:line. `?::?()` is forbidden; unresolved bindings render `UNRESOLVED` with a reason. Current executable unresolved count is 0. Twenty-row samples from Ghosts, Gaze, Dance, Path, and Shot were checked against their scoped owner functions before regeneration. |
| M4 object Mirror | P0 WP1 now specifies the object-root rotation facing publication, keeps Mirror enabled, uses `sigma=-1` for any mirrored live target, and requires a rotated-prop in-world test. |
| M5 writable homes | All 22 duplicate settings have one home, including the six Shot-option `CinematicCam*` conflicts; generation fails if the computed set and home table differ. |
| Minor 1 preview SSAO | VCam preview deferred lighting selects the non-SSAO permutation or binds neutral white AO; a stale-sentinel test is mandatory. |
| Minor 2 Rig parity | The regenerated Rig contract contains all 174 rows, including all four Lights options. |
| Minor 3 data hygiene | Generic `check_box` names resolve by declared `control_name`; `set_*` rows are kit-native settings; `cue_timecode_check` maps explicitly to `cue_timecode_mode` with a contract note. |
| Minor 4 wide drag | The TUT list documents periodic wrap and requires a 1,000-sample full-turn out-and-back to return within 0.01°. |

## 10. User decisions 10/10 applied

| Decision | Applied contract |
|---|---|
| Diopter | Director owns on/off, preset, strength (`CineDiopterBlend`), and focus split (`CineDiopterPower`), plus **Open full editor**. Exact defaults/ranges and real XUI/settings anchors are in `diopter_tab.md`; no pending-decision marker remains. |
| Capture | Dropped from the generated v2 page list via `director_mockup_data.py::PAGE_EXCLUSIONS`; the generated snapshot and source contracts contain 35 pages and the old Capture contract is deleted. The dropped page had zero rows, so no setting/control was orphaned; all capture/output controls remain reachable in the existing Snapshot floater. |
| Cue pages | Both pages contain the cue list plus Add, Remove, Play, and Stop. Select-to-edit opens the existing cue floater at the selected cue; detailed keyframes remain there. Refresh is revision-signal driven, and list edits integrate with `ALKitHistory`. |
| Object Mirror | The R3 object-front azimuth contract is already consistent in `DIRECTOR_KIT_P0_SPEC.md` (object root render rotation, Mirror remains enabled); no change was made. |

## 11. Final yes/no gate

The release candidate is NO-GO unless all 35 page links exist, source `--check` is clean, the home count matches generated evidence, executable unresolved rows are zero, the approved Diopter and cue contracts are complete, every explicit state passes at its required widths, non-Director parity and owner-token tests pass, v1 behavior diffs pass, VCam VC1–VC7 evidence is complete, performance budgets pass, adversarial review has zero must-fix findings, and the same fresh build passes the in-world matrix. Otherwise `DirectorUIV2` remains debug/self-test-only.

## 12. Review R4 resolution

| Finding | Resolution |
|---|---|
| M1 selector values | Every selector option has explicit `value`; Shot copies v1 values 1..39, 41..51 and `--check` round-trips labels/values. Kit reads/writes values, never indices. |
| M2 Gaze Play/Stop | No global seek exists, so WP8 owns the exact two-method `LLActorMover` preview API above. Light Cues remains `cueGoto`/`cueRelease`. |
| M3 one-home | Canonical-home policy now permits only 8 listed Easy/mode-block SHORTCUT rows; data carries `shortcut_of`, and search/undo stay home-canonical. |
| Minor 1 equality | WP0 equality is scoped to unchanged pages and strips additive `ew`/`es`, option `value`, and `shortcut_of`; decision pages/exclusion are explicit transforms. |
| Minor 2 retired launchers | P0 `parity_allow` names the eight retired Snapshot/Diopter/Gaze Cues/Light Cues launcher widgets individually; `addLauncherCard` is specified `findChild`-tolerant. |
| Minor 3 scanner | Shaft ownership is `ALCineLightRigManager`; Shot explicitly extracts `ALPanelCineCamParams::modeTable()` at line 127 despite the `//static`/templated signature. |
| Minor 4 Diopter | WP12 confirms the Focus split wording; history tests preset→Custom materialization, undo/redo, restored look/preset, and an uncorrupted Custom bank. |
