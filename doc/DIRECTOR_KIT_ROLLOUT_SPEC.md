# Director Kit: evidence-backed all-at-once rollout

Status: R2 rewrite for the work after `DIRECTOR_KIT_P0_SPEC.md` WP5.
Branch: `feature/director-kit-p0`.
Data authority: `scripts/code_tools/director_kit/director_mockup_data.json` (36 pages, 1,509 rows in the current snapshot).

## 1. Source authority and ship boundary

Director v2 remains default-off and unavailable from the normal menu until WP12 passes. There is no user-visible partial conversion. The v1 Director, Lightbox, standalone cue/Diopter/Snapshot floaters, and shared panels remain behaviorally intact while v2 is built.

Page ownership is not inferred from launcher cards or rollout prose. Run:

```text
<python> scripts/code_tools/director_page_sources.py
<python> scripts/code_tools/director_page_sources.py --check
```

The script deterministically scans every approved row against all English XUI, C++ string references and commit-callback registrations, and the Global/PerAccount settings graph including `Include` files. Its checked outputs are:

- [`page_sources.json`](director_kit/page_sources.json), the machine authority;
- [`page_sources.md`](director_kit/page_sources.md), the readable evidence;
- the 36 linked page contracts in section 3.

`--check` fails on any stale output. A missing XUI widget, callback/model binding, or settings declaration remains visible as an unresolved row; generation never converts it into a guessed binding.

The current evidence reports 36 pages, 1,509 rows, 51 distinct XUI owner files (including full-editor owners whose approved rows are only summaries), 157 unresolved rows, and 16 settings written by more than one Director page. R2 cited 20 conflicts from an earlier 334-row Shot snapshot. The current approved Shot page has 78 rows and no longer contains the four additional conflicts; section 5 records this drift rather than inventing absent rows.

## 2. Logic and adapter contract

The computed classification is binding: a page is `pure-setting` only when at least 90% of its rows resolve to declared settings and no row has a C++ widget reference or registered commit handler. Every other page is `code-driven`. Under this rule only Bloom & Glow and Shadows & Probes are pure-setting in the current tree. This deliberately classifies Operator, Frame & Orbit, Animate, Actor FX, Move, Lightbox tools, and settings whose widgets participate in C++ refresh logic as code-driven.

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
| Performance | [Gaze Cues](director_kit/pages/gaze_cues_tab.md) | 3 | code-driven | `floater_gaze_cues.xml`; `ALFloaterGazeCues` |
| Camera | [Shot](director_kit/pages/shot_tab.md) | 78 | code-driven | `panel_director_switcher.xml` + `panel_cinecam_params.xml`; switcher/camera classes |
| Camera | [Moves](director_kit/pages/moves_tab.md) | 14 | code-driven | `floater_director.xml`; Director generator handlers |
| Camera | [Operator](director_kit/pages/operator_tab.md) | 69 | code-driven | `panel_cinecam_operator.xml`; `LLCameraOperator`, `ALPanelCineCamParams` |
| Camera | [Frame & Orbit](director_kit/pages/frame_orbit_tab.md) | 31 | code-driven | `panel_cinecam_frame.xml` + `panel_flycam_orbit.xml`; `LLViewerJoystick`, `LLCinematicCamera`, Director |
| Camera | [Takes](director_kit/pages/takes_tab.md) | 20 | code-driven | `panel_flycam_recorder.xml`; recorder/joystick classes |
| Camera | [VCam](director_kit/pages/vcam_tab.md) | 3 | code-driven | frozen Director summary + Prism manager; `ALVCamKitAdapter` |
| Lens | [Focus](director_kit/pages/focus_tab.md) | 14 | code-driven | `panel_dof.xml` plus focus actions |
| Lens | [Filters](director_kit/pages/filters_tab.md) | 29 | code-driven | `panel_lens_looks.xml`; `ALPanelLensLooks` |
| Lens | [Diopter](director_kit/pages/diopter_tab.md) | 6 | code-driven | `floater_ultimate_diopter.xml`; `ALFloaterUltimateDiopter` |
| Light | [Rig](director_kit/pages/cine_light_rig_tab.md) | 116 | code-driven | `panel_cine_light_rig.xml`; `ALPanelCineLightRig` |
| Light | [Rim](director_kit/pages/light_rim_tab.md) | 25 | code-driven | `panel_cine_light_rig.xml`; `ALPanelCineLightRig` |
| Light | [Exposure](director_kit/pages/exposure_tab.md) | 25 | code-driven | `panel_exposure_hub.xml`; `ALPanelExposureHub` |
| Light | [Practicals](director_kit/pages/practicals_tab.md) | 16 | code-driven | `floater_lightbox_settings.xml`; Lightbox selected-light handlers |
| Light | [Actors](director_kit/pages/actors_tab.md) | 28 | code-driven | `panel_avatar_light_response.xml` + Lightbox; actor-response classes |
| Light | [Light Cues](director_kit/pages/light_cues_tab.md) | 6 | code-driven | `floater_cine_light_cues.xml`; `ALFloaterCineLightCues` |
| Look | [Image](director_kit/pages/image_tab.md) | 74 | code-driven | `floater_lightbox_settings.xml`; `ALFloaterLightBox` |
| Look | [Bloom & Glow](director_kit/pages/bloom_glow_tab.md) | 17 | pure-setting | Lightbox controls; direct settings |
| Look | [Roto Ink](director_kit/pages/stylize_tab.md) | 81 | code-driven | `floater_lightbox_settings.xml`; `ALFloaterLightBox` |
| Look | [Tron](director_kit/pages/tron_tab.md) | 132 | code-driven | `floater_lightbox_settings.xml`; `ALFloaterLightBox` |
| Look | [Actor FX](director_kit/pages/actor_style_tab.md) | 18 | code-driven | `floater_director.xml`; `LLFloaterDirector::refreshActorStyleTab` path |
| World | [Weather](director_kit/pages/weather_tab.md) | 51 | code-driven | `panel_weather_settings.xml`; weather handlers/settings consumers |
| World | [Atmosphere](director_kit/pages/atmosphere_tab.md) | 80 | code-driven | atmospheric-volume/projector panels + Lightbox; `ALPanelAtmosphericVolumes` |
| World | [Shadows & Probes](director_kit/pages/shadows_probes_tab.md) | 39 | pure-setting | `floater_lightbox_settings.xml`; direct settings |
| Capture | [Time](director_kit/pages/temporal_tab.md) | 16 | code-driven | `panel_temporal_capture.xml`; `ALPanelTemporalCapture` |
| Capture | [Capture](director_kit/pages/capture_settings_tab.md) | 0 | code-driven / blocked | Snapshot floater/panels; approved data is empty |
| Capture | [Scene](director_kit/pages/scene_options_tab.md) | 3 | code-driven / unresolved | Director scene state |

Count gate: exactly 36 links and page files. A zero-row page, unresolved executable row, stale source index, or unapproved page key is a hard failure.

## 4. Existing-control and custom-widget policy

The invented `path_canvas`, `curve_editor`, `slot_grid`, `asset_picker`, and generic `preview_view` kinds are removed from the rollout. Path uses its waypoint list and existing buttons/spinners; cue stacks use their lists and stock editors; Switchboard and Shot use the existing slot buttons; Takes uses its recorder list/scrubber; Exposure uses its existing panel controls.

An existing complex control may be extracted into an `embed` only when its page contract says so. An embed is full card width, has `minHeightForWidth()`/`preferredHeightForWidth()`, is 160–320 px high, contains no page-level scroller, clips to its measured rect, enumerates its states, and uses an owner token. Existing Ghost preview/diagnostics and an extracted cue editor are the only candidates in this snapshot.

There are currently **no approved new widget classes**. The Diopter six-row launcher/summary cannot represent its 560×720 nine-tab editor, and Capture has zero rows. Both are explicit **USER DECISION** data/model gaps; implementation must not invent a compact editor. Unresolved standard buttons/readouts are binding decisions, not new-widget authorization.

## 5. One-home conflict resolution

Within Director v2 exactly one page is writable for each setting below. Every occurrence on another Director page is generated as `link` or read-only state text naming the home. Existing non-Director XUI remains a compatibility writer during rollout and is a parity oracle; it is not a second Director home.

| Setting | Writable Director home | Non-home Director occurrence | Existing non-Director writer(s) retained for parity |
|---|---|---|---|
| `ActorMoverEndMode` | [Move](director_kit/pages/move_tab.md) | Props | `panel_actor_mover#end_mode_combo` |
| `AlchemyRenderTonemapType` | [Exposure](director_kit/pages/exposure_tab.md) | Image | Lightbox, Exposure Hub, advanced graphics prefs |
| `AlchemyToneMapAMDExposure` | [Exposure](director_kit/pages/exposure_tab.md) | Image | Lightbox, Exposure Hub |
| `AlchemyToneMapSLogEV` | [Exposure](director_kit/pages/exposure_tab.md) | Image | Lightbox, Exposure Hub |
| `AlchemyTonemapLegacyGammaSkies` | [Exposure](director_kit/pages/exposure_tab.md) | Image | Lightbox, Exposure Hub |
| `BDMergeProjectorVolumetricsFeather` | [Atmosphere](director_kit/pages/atmosphere_tab.md) | Rig | Projector Shafts panel |
| `BDMergeProjectorVolumetricsMaxDistance` | [Atmosphere](director_kit/pages/atmosphere_tab.md) | Rig | Projector Shafts panel, Cine Rig easy shaft length |
| `CineRigRimTronColorSource` | [Rim](director_kit/pages/light_rim_tab.md) | Tron | Lightbox, Cine Rig |
| `CineRigRimTronMix` | [Rim](director_kit/pages/light_rim_tab.md) | Tron | Lightbox, Cine Rig |
| `CineRigRimTronMode` | [Rim](director_kit/pages/light_rim_tab.md) | Tron | Lightbox, Cine Rig |
| `CinematicCamFrameOffsetUp` | [Frame & Orbit](director_kit/pages/frame_orbit_tab.md) | Operator | Cine Camera Frame, Cine Camera Operator |
| `CinematicCamUseSelected` | [Frame & Orbit](director_kit/pages/frame_orbit_tab.md) | Shot | Cine Camera Params, Flycam Orbit |
| `RenderBloomStrength` | [Bloom & Glow](director_kit/pages/bloom_glow_tab.md) | Exposure | Exposure Hub |
| `RenderBloomThreshold` | [Bloom & Glow](director_kit/pages/bloom_glow_tab.md) | Exposure | Exposure Hub |
| `RenderExposure` | [Exposure](director_kit/pages/exposure_tab.md) | Image | Lightbox, Exposure Hub, Display/advanced prefs |
| `TemporalWorldScale` | [Time](director_kit/pages/temporal_tab.md) | Ghosts | Temporal Capture panel slider/spinner |

Count: **16 current homes chosen**. R2's expected count of 20 is not reproducible from the approved current JSON/XUI. If those four Shot conflicts return, source generation must expose them and this table must choose homes before `--check` can pass; they may not be restored from prose memory.

## 6. Bounded audit and parity

Each page contract contains an explicit state manifest. A manifest is capped at 256 named states and two minutes per page; the suite is capped at 72 minutes. Every state starts from the same default snapshot, changes only the named local condition, performs synchronous kit layout, waits one idle for handle-guarded refresh, audits, screenshots on failure, and restores under `ALKitHistory::ScopedSuspend`. There is no Cartesian product.

The manifests explicitly cover the three non-default shell sizes, every card alone in its tallest state at its minimum and maximum card width, every declared combo option alone, each toggle's dependent-visible state, list empty/one/51-item states, target/selection loss, owner-token transfer, applicable permission/offline state, cue playback, and VCam capture-count/preview states. The default 1160×720 state is the baseline parity snapshot. The four required sizes remain 1160×720, 850×560, 1141×720, and 915×720.

Parity uses the real owners in `page_sources.json`, not “v1 leaf plus filename embeds.” For each row it compares widget class/presentation kind, initial value, enabled/visible state, option/list population, commit result, reset/default/amber, and undo/redo. It invokes each extracted handler through both the old owner and the kit adapter against the same model snapshot. Runtime instrumentation records `(page, widget, requested class, actual class, owner root, adapter handler)` and is compared to static extraction.

This parity explicitly instantiates Lightbox for Tron, Roto, Image, Shadows, Practicals, and its Atmosphere/Actors rows; `ALFloaterGazeCues`, `ALFloaterCineLightCues`, and `ALFloaterUltimateDiopter`; Snapshot owners for Capture; and all shared panels. A launcher card is never an oracle for controls it does not own. Two simultaneously live owners must pass owner-token, external-write synchronization, hide/reveal, and close/reopen tests.

## 7. Work packages after WP5

These are review boundaries inside one default-off delivery. Every extraction receives an adversarial behavior diff for the v1/non-Director forwarding call.

### WP6 — evidence, generation, adapters, and complete data

Land the source-index/check gate, setting graph, one-home map, adapter/controller interfaces, owner tokens, history barrier, generated pages, and bounded audit runner. Resolve every executable unresolved row. Diopter and Capture require approved row data before this package can pass.

### WP7 — Cast

Convert the five Cast pages and their actor/path/prop/ghost/dance controllers. Review `ALPanelActorMover`, `ALPanelPathEditor`, `ALPanelPropMover`, `ALPanelGhostStudio`, `ALPanelDanceBall`, and the Move logic in `llfloaterdirector.*`. No path canvas is added.

### WP8 — Performance

Convert Animate, Switchboard, Gaze, Polish, and Gaze Cues. Review `llfloaterdirector.*`, animation preview/switcher, gaze/polish owners, and `alfloatergazecues.*`. The standalone Gaze Cues floater remains a shared owner; no curve or slot-grid widget is added.

### WP9 — Camera, Lens, and VCam integration

Convert all six Camera and three Lens pages and review switcher, camera params/operator/frame/orbit/recorder, `llfloaterdirector.*`, Diopter, and Prism call sites. VCam packages are named **VC1–VC7**, never WP1–WP7: VC1–VC5 are engine work and may precede WP6; VC6 is the Director kit page integrated here after adapter infrastructure; VC7 is VCam convergence evidence consumed by WP12. VCam migration tests cover `DirectorLastTabName`, v1/v2 rect carry-over, and Camera-group placement.

### WP10 — Light and Look

Convert six Light and five Look pages. Review `alfloaterlightbox.*`, `alpanelcinelightrig.*`, Exposure, avatar-light response, both cue owners, `llfloaterdirector.*` Actor FX logic, and every preset/visibility callback listed in the page contracts. Lightbox parity is mandatory. No curve editor is added.

### WP11 — World and Capture

Convert Weather, Atmosphere, Shadows & Probes, Time, Capture, and Scene. Review Weather, Atmospheric Volumes, Projector Shafts, Lightbox shadow/atmosphere rows, Temporal Capture, Snapshot owners, and Director scene state. Capture cannot pass with zero approved rows.

### WP12 — convergence and ship gate

Run source generation/check, page/data lint, static/runtime parity, settings/home lint, all bounded state manifests, XUI layout self-tests, UI-draw lint, unit tests, performance gates, and default-v1 regression. Archive state manifests and failure screenshots. Only after adversarial review reaches zero findings, make one fresh build and run the 36-page in-world matrix; only then may normal v2 selection be exposed.

## 8. Review R2 resolution (rollout)

| R2 finding | Resolution in this rewrite |
|---|---|
| N2 wrong owners/classes | `page_sources.json` replaces handwritten sources; matrix and page contracts link real Lightbox, Director, panel, cue, Diopter, and Snapshot owners. |
| N3 invented custom kinds | Removed path canvas, curve editor, slot grid, asset picker, and generic preview inventions; existing controls or bounded embeds only. |
| M-R6 one-home contradiction | Section 5 chooses all 16 conflicts reproducible from the current source and converts non-home Director rows to links/read-only. The 20→16 snapshot drift is explicit. |
| M-R7 unbounded Cartesian audit | Per-page explicit isolated manifests, 256-state hard cap, two-minute page cap, 72-minute suite cap. |
| M-R9 external history barrier | Global lookup at preset apply and cue start/stop; recording suspended for playback duration, never barrier-per-write. |
| M-R10 one-line code pages | All 36 page files contain row bindings, exact handler anchors, adapter extraction, refresh, states, parity, and risks. |
| M-R11 WP collision | Rollout remains WP6–WP12; VCam is VC1–VC7 and is ordered into WP9. |
| Minor cue-owner collision | Both standalone cue floaters and Director adapters are named shared owners with owner-token states. |
| Parity blind to non-Director owners | Section 6 requires Lightbox, cue/Diopter/Snapshot floaters, and shared panels as parity oracles. |

## 9. Final yes/no gate

The release candidate is NO-GO unless all 36 page links exist, source `--check` is clean, the home count matches generated evidence, executable unresolved rows are zero, Capture is non-empty, Diopter has approved full-page data, every explicit state passes at its required widths, non-Director parity and owner-token tests pass, v1 behavior diffs pass, VCam VC1–VC7 evidence is complete, performance budgets pass, adversarial review has zero must-fix findings, and the same fresh build passes the in-world matrix. Otherwise `DirectorUIV2` remains debug/self-test-only.
