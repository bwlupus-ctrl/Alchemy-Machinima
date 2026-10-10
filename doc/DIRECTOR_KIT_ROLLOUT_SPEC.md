# Director Kit: all-at-once rollout specification

Status: binding implementation specification for the work after `DIRECTOR_KIT_P0_SPEC.md` WP5.  
Branch: `feature/director-kit-p0`.  
Source snapshot: `scripts/code_tools/director_kit/director_mockup_data.json` (36 pages).

## 1. Ship contract and rollout boundary

Director v2 is one product delivery, not a sequence of user-visible partial conversions. WP0-WP5 build the kit foundation and Light > Rig/Rim exemplars; WP6 and later convert every other page in the same tree. `DirectorUIV2` remains default-off and has no normal menu entry until the final gate in section 8 passes. Automated tests may enable it only through `ui_selftest.ps1 --set DirectorUIV2 TRUE` or a Debug Settings path.

The shippable state has all of these properties at once:

- all 36 data pages are full kit pages; no `legacy="true"` page and no launcher-only substitute remains;
- Look tools are inline pages. Image, Bloom & Glow, Roto Ink, Tron, and Actor FX expose their real controls; links may navigate between pages but cannot replace a page;
- there is one home for each control. A duplicate concept elsewhere is a `link` row or read-only summary, never a second writer;
- the rail cast selection drives actor-aware pages through the adapters in section 2; ACTION/CUT remains the primary transport action;
- VCam is the Camera > VCam kit page inside `floater_director_v2.xml`; the frozen `floater_director.xml` remains byte-identical;
- the final runtime layout audit has an absolute zero findings on every page at 1160x720, 850x560, 1141x720, and 915x720, including every mode, chip, selector option, expanded card/More state, and popover/alternate state described in section 6;
- parity, lint, generated-data completeness, keyboard, undo, performance, and in-world checks all pass. There is no waiver based on a v1 baseline at the final gate.

The current tree justifies the boundary: the v1 VCam leaf is only the three-control summary at `indra/newview/skins/default/xui/en/floater_director.xml:1342-1349`, while shared logic panels are embedded for Dance (`:567-569`), Gaze (`:915-917`), Exposure (`:1398-1400`), Actors (`:1413-1419`), and Atmosphere (`:1692-1697`). The conversion therefore preserves their C++ behavior through adapters rather than treating the XML snapshot as the feature.

## 2. Logic ownership and adapter contract

Every page declares exactly one logic class:

- **pure-setting**: every mutable value is an `LLControlVariable` or a kit-native action. `ALKitPage` owns the bindings; a small action provider may expose buttons, but it does not mirror values.
- **code-driven**: ordinary kit rows remain the presentation, while an existing model/panel owns values and commands. A named `ALKitPageLogicAdapter` is the only bridge.
- **custom-widget**: the page uses the same adapter plus one or more bounded `embed` rows for controls that cannot be represented by scalar/list kit kinds.

The named pattern is:

```cpp
class ALKitPageLogicAdapter
{
public:
    virtual ~ALKitPageLogicAdapter() = default;
    virtual void attach(ALKitPage&, ALKitBindingRegistry&) = 0;
    virtual void onReveal() = 0;
    virtual void onHide() = 0;
    virtual void refresh(U32 dirty_bits) = 0;
    virtual std::string undoContext() const = 0;
};
```

`ALKitPageAdapterRegistry` creates an adapter by the generated `logic_adapter` key. The registry factory is stateless and idempotent; it never captures a panel `this`. The created adapter is owned by the page and captures only `LLHandle`/manager identity. It caches every widget pointer once in `attach()`. Deferred work captures the page handle and exits when it is dead. This is required because the Director floater has no `reuse_instance` attribute in `indra/newview/skins/default/xui/en/floater_director.xml:8-18` and is destroyed on close.

For code-driven rows, the existing panel implementation is not copied into kit callbacks. Its model-facing operations move into a named controller or adapter (`ALActorMoverKitAdapter`, `ALPathKitAdapter`, `ALGhostStudioKitAdapter`, and so on), and both the existing standalone/shared panel and the kit page call that controller. Each row registers an `ALKitBinding` with `get`, `set`, default/reset, changed-signal, and description functions; buttons register named commands. This preserves the existing engine path while putting the presentation behind `kit_row`. Direct settings continue to use `ALKitSettingBinding`. Polling from `draw()` is forbidden.

Shared embedded panels are not embedded wholesale as a page. Their v2 page XML is new kit XML; their existing C++ class/controller is reused through the adapter. The v1 XML and other floaters remain untouched. Path, Ghost Studio, Actor Mover, Weather, Exposure, Actor Style, Practicals, Gaze, and the other named layout exceptions therefore receive an independent Director-v2 conversion and in-world check, as required by `CLAUDE.md`. A shared controller cannot claim edit ownership merely because both views exist: it uses an owner token, claims on reveal/focus/user action, and releases only its own token.

All user-visible mutations go to the Director-wide `ALKitHistory`. An adapter may add a typed custom entry with `undo()`/`redo()` lambdas and a scene-generation/context token; it may not install another key handler or history stack. Bulk external changes use the barrier contract in the kit spec. A page's settings watch excludes values already represented by typed custom entries, preventing double journal entries.

## 3. Kit kinds for non-scalar controls

The generator accepts these additional row kinds. They are layout contracts, not permission to paste a legacy panel into a card.

| Kind | Child contract | Layout and audit contract |
|---|---|---|
| `path_canvas` | one `ALPathCanvas`/path-editor canvas view | full-span card; `heightForWidth()` clamps to 180-300 px; no internal page scroller; toolbar actions remain separate kit rows |
| `slot_grid` | one switcher/program grid with a fixed maximum of 12 slots | full-span; 3/4/6 columns selected from width; cell minimum 72x48; every selection/armed/disabled state is audited |
| `asset_picker` | one animation/asset results view | full-span; bounded list height 160-240 px; filter and actions are separate kit rows; `ALPanelAnimPreview` is the preview child, not a nested legacy panel |
| `curve_editor` | one curve/keyframe view | full-span; 180-260 px; scrubber has `wheel_adjust="false"`; empty, one-key, selected-key, and overflow-key states are audited |
| `monitor_strip` | one `ALVCamMonitorStrip` | full-span; fixed eight tiles; audits 0/1/4/5/8 captures, raw and every display selector state |
| `preview_view` | one render/pose preview LLView | full-span; 160-240 px; generation/handle checked; no model refresh in `draw()` |
| `diagnostics` | one bounded, read-only text/list view | full-span; maximum 240 px; refresh on demand or at most 2 Hz while visible |
| `embed` | exactly one registered LLView implementing `ALKitEmbeddedView` | exceptional fallback described below |

`ALKitEmbeddedView` exposes `minHeightForWidth`, `preferredHeightForWidth`, `enumerateAuditStates`, `applyAuditState`, `claim(owner_token)`, and `release(owner_token)`. An `embed` row is full-span, has no hand-authored child geometry, reserves the scrollbar width, and clips its child to the calculated content rect. It cannot contain another page-level scroll container, cannot read settings or look up children from `draw()`, and cannot exceed 320 px without an explicit generator error and a page-specific audit fixture. The generator report names every embed and its reason. `kit_lint` rejects an unregistered child, missing audit-state enumerator, fixed document height, or an embed used only to avoid converting ordinary rows.

The path editor canvas, switcher/animation slot grids, animation picker/`ALPanelAnimPreview`, exposure/camera curves, VCam monitor strip, and large diagnostics use the named kinds above. Existing complex panels may contribute the child view/controller, but their surrounding controls become kit rows. This makes all children participate in the same card measurement and the absolute-zero audit instead of preserving legacy coordinates.

## 4. Per-page conversion matrix (all 36 pages)

Every output is `panel_dpage_<group>_<page>.xml`, with a logic root when an adapter is named and a `kit_page` beneath it. “Widgets” lists only non-scalar kinds; ordinary scalar, list, text, link, and action rows use the standard kit kinds. “History” is the page context within the one `ALKitHistory`. Presets are `widgets`, `provider:<key>`, or `none`; an existing preset combo is retained through its adapter until a provider replaces it without changing behavior.

### Cast (5)

| Page; data rows | Logic class | v1 source / logic adapter | Binding, widgets, history, presets |
|---|---|---|---|
| Move `move_tab`; 19 | code-driven | `floater_director.xml:323`, `panel_actor_mover.xml`; `ALActorMoverKitAdapter` over `ALPanelActorMover` | eleven code rows register manager bindings/commands; rail cast is the scope source; `actor-mover:<selection>`; none |
| Path `path_tab`; 62 | custom-widget | `floater_director.xml:498`, `panel_path_editor.xml`; `ALPathKitAdapter` over `ALPanelPathEditor` | all 58 code rows bind through the adapter; `path_canvas`; preserve `mEditOwner`; `path:<actor-id>`; provider:`path` |
| Props `props_tab`; 33 | code-driven | `floater_director.xml:535`, `panel_prop_mover.xml`; `ALPropMoverKitAdapter` over `ALPanelPropMover` | all 33 rows bind to selected-prop/path commands; `prop:<object-id>`; provider:`prop_path` |
| Ghosts `ghosts_tab`; 125 | custom-widget | `floater_director.xml:554`, `panel_ghost_studio.xml`; `ALGhostStudioKitAdapter` over `ALPanelGhostStudio` | 122 code rows; bounded `diagnostics` and `preview_view` where the studio previews placement; preserve placement owner token; `ghost:<uuid>`; provider:`ghost_look` |
| Dance `dance_tab`; 82 | code-driven | `floater_director.xml:567-569`, `panel_dance_ball.xml`; `ALDanceBallKitAdapter` over `ALPanelDanceBall` | all rows use dance-session bindings and list commands; `dance:<session-generation>`; provider:`dance_session` |

### Performance (5)

| Page; data rows | Logic class | v1 source / logic adapter | Binding, widgets, history, presets |
|---|---|---|---|
| Animate `animate_tab`; 24 | custom-widget | `floater_director.xml:581`, `panel_anim_preview.xml`; `ALAnimateKitAdapter` over `ALPanelAnimPreview` | animation list/actions use adapter; `asset_picker` + `preview_view`; `animation:<actor-id>`; provider:`locomotion` |
| Switchboard `switchboard_tab`; 32 | custom-widget | `floater_director.xml:879-881`, `panel_director_anim_switcher.xml`; `ALAnimSwitcherKitAdapter` | 12 program slots become one `slot_grid`; editor rows remain kit rows; `anim-switchboard:<actor-id>`; provider:`anim_program` |
| Gaze `gaze_tab`; 123 | code-driven | `floater_director.xml:915-917`, `panel_lens_gaze.xml`; `ALGazeKitAdapter` over `ALPanelLensGaze` | 109 code rows; rail actor maps through `gaze_actor`; `gaze:<actor-id>`; provider:`gaze_profile` |
| Polish `polish_tab`; 15 | pure-setting | `floater_director.xml:919-921`, `panel_pose_polish.xml`; `ALPosePolishActions` only | 13 setting rows bind directly; open/reset/status actions are named; `polish:<actor-id>`; none |
| Gaze Cues `gaze_cues_tab`; 3 snapshot rows expanded from the real cue model | custom-widget | `floater_director.xml:923-929`; `ALGazeCueKitAdapter` | generator must snapshot the complete cue editor, not the three-row summary; `curve_editor`; `gaze-cues:<actor-id>:<cue-generation>`; provider:`gaze_cues` |

### Camera (6)

| Page; data rows | Logic class | v1 source / logic adapter | Binding, widgets, history, presets |
|---|---|---|---|
| Shot `shot_tab`; 334 | custom-widget | `floater_director.xml:938-940`, `panel_director_switcher.xml`, `panel_cinecam_params.xml`; `ALShotKitAdapter` | settings bind directly; switcher commands and dynamic subjects bind through adapter; `slot_grid` and camera `curve_editor`; `shot:<scene-generation>`; provider:`shot` |
| Moves `moves_tab`; 14 | pure-setting | `floater_director.xml:1127-1129`; `ALCameraMoveActions` | eleven settings direct, three generator buttons named actions; `camera-moves`; none |
| Operator `operator_tab`; 69 | pure-setting | `floater_director.xml:1209-1211`, `panel_cinecam_operator.xml`; `ALCameraOperatorActions` | 68 setting rows direct; preset action delegates to existing controller; `camera-operator`; provider:`operator` |
| Frame & Orbit `frame_orbit_tab`; 31 | pure-setting | `floater_director.xml:1245-1247`, `panel_cinecam_frame.xml`, `panel_flycam_orbit.xml`; `ALFrameOrbitActions` | 29 setting rows direct; resolve/status actions adapter-backed; `camera-frame`; provider:`frame_orbit` |
| Takes `takes_tab`; 20 | custom-widget | `floater_director.xml:1317`, `panel_flycam_recorder.xml`; `ALTakesKitAdapter` | recorder commands and scrubber use typed entries; `curve_editor`; `take:<recording-generation>`; provider:`takes` |
| VCam `vcam_tab`; monitor design replaces the 3-row summary | custom-widget | v1 summary stays frozen at `floater_director.xml:1342-1349`; new `ALPanelDirectorVCam` + `ALVCamKitAdapter` | `monitor_strip`; typed Prism commands enter `ALKitHistory`; `vcam:<registry-generation>`; provider:`vcam`; details are binding in `VCAM_MONITORS_DESIGN.md` |

### Lens (3)

| Page; data rows | Logic class | v1 source / logic adapter | Binding, widgets, history, presets |
|---|---|---|---|
| Focus `focus_tab`; 14 | pure-setting | `floater_director.xml:1334-1336`, `panel_dof.xml`; `ALFocusActions` | all mutable rows direct; `lens-focus`; provider:`dof` |
| Filters `filters_tab`; 29 | pure-setting | `floater_director.xml:1338-1340`, `panel_lens_looks.xml`; `ALLensFilterActions` | 26 settings direct; three preset/action rows delegate; `lens-filters`; provider:`lens_filters` |
| Diopter `diopter_tab`; 6 snapshot rows expanded from the real inline editor | pure-setting | `floater_director.xml:1354-1360`; `ALDiopterActions` | replace the launcher summary with every Diopter setting plus the two editor actions; `lens-diopter`; provider:`diopter` |

### Light (6)

| Page; data rows | Logic class | v1 source / logic adapter | Binding, widgets, history, presets |
|---|---|---|---|
| Rig `cine_light_rig_tab`; 174 | custom-widget | `floater_director.xml:1375`, `panel_cine_light_rig.xml`; `ALCineRigKitAdapter`/existing controller | standard/code rows as P0; `light_rig_map`; `cine-rig-slot:<n>`; widgets |
| Rim `light_rim_tab`; 41 | code-driven | new data page; `ALRigRimController` + `ALPanelRigRim` | settings direct and controller status/actions; `cine-rig-slot:<n>`; provider:`rig_rim` |
| Exposure `exposure_tab`; 25 | custom-widget | `floater_director.xml:1398-1400`, `panel_exposure_hub.xml`; `ALExposureKitAdapter` | settings direct; sky/action/undo routes adapter-backed; `curve_editor`; `exposure:<environment-generation>`; provider:`exposure` |
| Practicals `practicals_tab`; 16 | code-driven | `floater_director.xml:1402-1408`; `ALPracticalsKitAdapter` over selected-light logic | all rows bind to the selected light; rail does not change object selection; `practical:<object-face>`; provider:`practical` |
| Actors `actors_tab`; 28 | code-driven | `floater_director.xml:1413-1424`, `panel_avatar_light_response.xml`; `ALActorLightKitAdapter` | target list and 23 code rows use selected actor; rail cast drives target; `actor-light:<actor-id>`; provider:`actor_light` |
| Light Cues `light_cues_tab`; 6 snapshot rows expanded from cue stack | custom-widget | `floater_director.xml:1429-1435`; `ALLightCueKitAdapter` | complete cue stack inline, not launcher; `curve_editor`; `light-cues:<slot>:<cue-generation>`; provider:`light_cues` |

### Look (5)

| Page; data rows | Logic class | v1 source / logic adapter | Binding, widgets, history, presets |
|---|---|---|---|
| Image `image_tab`; 74 | pure-setting | `floater_director.xml:1444-1450`; `ALImageLookActions` | settings direct; cross-tool concepts are link rows; `look-image`; provider:`image` |
| Bloom & Glow `bloom_glow_tab`; 17 | pure-setting | new data page | all settings direct; the open-Lightbox row is removed from v2; `look-bloom`; provider:`bloom_glow` |
| Roto Ink `stylize_tab`; 81 | pure-setting | `floater_director.xml:1455-1461`; `ALRotoActions` | settings direct; Tron is a link row; `look-roto`; provider:`roto` |
| Tron `tron_tab`; 132 | pure-setting | new data page; `ALTronActions` | full inline page, never a launcher; settings direct and seven named actions; `look-tron`; provider:`tron` |
| Actor FX `actor_style_tab`; 18 | code-driven | `floater_director.xml:1477`; `ALActorStyleKitAdapter` | all 18 rows bind to per-actor style model; rail cast drives actor; `actor-style:<actor-id>`; provider:`actor_style` |

### World (3)

| Page; data rows | Logic class | v1 source / logic adapter | Binding, widgets, history, presets |
|---|---|---|---|
| Weather `weather_tab`; 51 | pure-setting | `floater_director.xml:1661`, `panel_weather_settings.xml`; `ALWeatherActions` | 48 settings direct; trigger/reset/status named actions; `weather:<environment-generation>`; provider:`weather` |
| Atmosphere `atmosphere_tab`; 80 | code-driven | `floater_director.xml:1692-1697`, `panel_atmospheric_volumes.xml`, `panel_projector_shafts.xml`; `ALAtmosphereKitAdapter` | settings direct; 28 volume/list/action rows adapter-backed; `atmosphere:<volume-id>`; provider:`atmosphere` |
| Shadows `shadows_probes_tab`; 39 | pure-setting | `floater_director.xml:1711`; `ALShadowProbeActions` | 37 settings direct; projector link and action row named; `world-shadows`; provider:`shadows_probes` |

### Capture (3)

| Page; data rows | Logic class | v1 source / logic adapter | Binding, widgets, history, presets |
|---|---|---|---|
| Time `temporal_tab`; 16 | code-driven | `floater_director.xml:1732`, `panel_temporal_capture.xml`; `ALTemporalCaptureKitAdapter` | eight settings direct and eight runtime/action rows adapter-backed; `temporal-capture`; provider:`temporal` |
| Capture `capture_settings_tab`; currently 0, must be populated | pure-setting | empty v1 leaf at `floater_director.xml:1745`; capture settings registry | generator completeness fails until all normal capture/output settings, labels, help, keywords, scopes, and defaults are present; `capture-settings`; provider:`capture` |
| Scene `scene_options_tab`; 3 | pure-setting | `floater_director.xml:1756`; `ALSceneOptionsActions` | scene toggle direct; hotkey/category readouts revision-driven; `scene-options`; none |

Count gate: the tables above contain exactly 36 page keys (5 + 5 + 6 + 3 + 6 + 5 + 3 + 3). The generator fails if its data set or config contains a missing/extra key, if a summary/launcher substitutes for a page marked “expanded,” or if any row lacks a label, help text, keywords, scope, binding/action, and reset policy.

## 5. Generation, parity, and completeness gates

`kit_pages.json` changes from `p0_pages` to a required `pages` map with exactly the 36 keys in section 4. Each entry declares `logic_class`, `logic_adapter`, `v1_sources`, `undo_context`, `preset`, `who_adapter`, custom kinds, and every code-row binding/action. `director_kit_gen.py` emits only kit pages. `legacy=true`, `launcher`, an unresolved code row, an unregistered embed, and a page with zero indexed rows are hard errors.

`director_parity.py --groups all` is the mandatory gate after WP6 begins. For every v1 leaf and recursively expanded `filename=` source it verifies:

1. every C++-referenced widget name exists in the v2 page or has a narrow, reasoned replacement entry;
2. every v1 `control_name` has one v2 binding, and one control is the writable home;
3. every `getChild<T>`/`findChild<T>` reference matches a v2 control compatible with `T`, not merely the same name;
4. literal, prefix/suffix-concatenated, indexed, and generated names are expanded. In particular the `cine_` + role + `_{profile,gobo,gel,flicker}` lookups at `indra/newview/alpanelcinelightrig.cpp:1064-1074` are mandatory patterns and cannot be presentation-upgraded to an incompatible class;
5. runtime self-test instrumentation logs each resolved `(page, name, requested C++ type, actual type)` and parity compares the log to the static extraction. A missing runtime path is a failure, not an allowlist entry.

Settings lint follows the viewer's real configuration graph. It reads every Global and PerAccount base named by `indra/newview/app_settings/settings_files.xml:1-68`, then recursively follows `Include` arrays such as `settings.xml:6327-6330` to load `settings_alchemy.xml`. Runtime resolution uses the same setting-group lookup semantics as `LLView::findControl` (`indra/llui/llview.cpp:2405-2427`). `vec2` requires a Vector3/Vector3D control and leaves z unchanged; the control enum has no Vector2 (`indra/llxml/llcontrol.h:51-66`), and `RenderVignetteCenter` is a Vector3 with unused z (`settings_alchemy.xml:3435-3447`).

The generated search index is version 2 with a header declaring `fields="15"`; every data line has exactly the 15 fields defined in the P0 spec. The loader rejects another version, field count, malformed escape, missing page, or duplicate writable home. VCam, Capture, Gaze Cues, Light Cues, Diopter, and every formerly summary/launcher page must have complete control rows before generation passes.

`kit_lint` measures label text with the actual configured SansSerif/IBM Plex metrics at every supported UI scale. Text wider than `LABEL_W` is an error unless the row explicitly opts into the defined two-line label form, which increases that row's measured height. The former “more than 16 characters” warning is removed. Empty help/keywords, a hidden-only home control, and a setting without a loaded declaration are errors.

## 6. Exhaustive layout and behavioral audit

The Director-v2 audit does not visit one default snapshot. `ALKitPage::enumerateAuditStates()` returns a deterministic Cartesian set pruned only for states the page declares mutually exclusive:

- every page mode (including Easy and Advanced);
- every section chip in filter mode and every anchor destination in anchor mode;
- every selector option;
- every advanced card both collapsed and expanded, with “More options” closed and open;
- every registered custom-widget/embed state;
- master on/off, populated/empty lists, validation/error banners, and applicable permission/offline states;
- page popovers and inline rename editors, including the transport scene-name editor;
- VCam 0/1/4/5/8 captures, raw and each display choice, previews running/paused/throttled/offline;
- rail label/icon modes and help panel/strip modes implied by the four widths.

The runner applies a state, forces synchronous kit layout, waits one idle for handle-guarded refresh, and calls `auditGeometry`; it then restores the page snapshot under `ALKitHistory::ScopedSuspend`. It reports the state key in every finding and screenshot name. Custom widgets must supply states through section 3; no opaque child is considered audited merely because its default rect passed.

Required sizes are exactly 1160x720, 850x560, 1141x720, and 915x720. The command-line token `min` maps to the existing output name `minimum`; `auditSizeName()` currently returns `minimum` at `indra/newview/aluiperfselftest.cpp:1320-1323`, so this preserves baseline keys. Final acceptance is **zero absolute findings for shell, chrome, and all 36 pages in every enumerated state**. The intermediate v1 “0 new” comparison ends when the last legacy page is removed.

The audit also asserts that effectively hidden rows are actually `setVisible(false)`. The current overflow walk skips only invisible children (`indra/llui/lluifitdiag.cpp:45-61`), so alpha-zero, clipped, or merely omitted-from-height rows do not qualify. Visibility tracking is an allocation-free vector/bitset sized to all rows, not a 64-row mask. `ALKitDoc` permanently reserves scrollbar width to prevent appear/disappear relayout oscillation.

Behavioral passes run after geometry for each page: writable-home uniqueness, reset/default/amber, undo/redo, rail who-follow and pin, search reveal, keyboard focus protection, and page hide/reveal refresh. Search reveal uses the page's mode adapter and must leave all revealed controls operative. The audit records zero by-name lookups per steady frame and verifies that no hidden page performs widget writes.

## 7. Work packages after WP5

These packages are review boundaries inside one all-at-once delivery. No package enables v2 for normal users. Each package gets adversarial review to zero must-fix; named `CLAUDE.md` layout exceptions receive their own in-world check even when implemented in the same package.

### WP6 — conversion infrastructure and complete data

Files:

- `alkitpage.*`, `alkitrow.*`, `alkitbinding.*`, `alkithistory.*`, new `alkitpageadapter.*`, and the custom view classes from section 3;
- `director_kit_gen.py`, `kit_pages.json`, `cpp_name_patterns.json`, `kit_lint.py`, `director_parity.py`, and `director_mockup_data.json`;
- generated page XML/index/report files and `aluiperfselftest.*` audit-state support.

Acceptance:

- config contains exactly 36 pages and generation contains no legacy/launcher classification;
- all incomplete summary pages have real rows and all 15 search fields;
- adapter lifetime, owner-token, typed-history, dynamic-name/class parity, setting-include, label-metric, embed, and audit-state tests pass;
- `director_parity.py --groups all` can run incrementally and names remaining unconverted pages as failures.

### WP7 — Cast pages

Files: the five Cast `panel_dpage_cast_*.xml` files; `alpanelactormover.*`, `alpanelpatheditor.*`, `alpanelpropmover.*`, `alpanelghoststudio.*`, `alpaneldanceball.*`; their new adapter/controller files; generator config/data.

Acceptance: all five section-4 row contracts pass parity; Path canvas and all list/placement states pass the four-size audit; Actor Mover, Path, and Ghost Studio each receive a separate in-world layout/ownership check; rail selection, pinning, undo contexts, presets, path editing, ghost placement, and dance session import/export remain behaviorally equivalent.

### WP8 — Performance pages

Files: the five Performance page XML files; `alpanelanimpreview.*`, `alpaneldirectoranimswitcher.*`, `alpanellensgaze.*`, pose-polish and gaze-cue owners; new adapters; config/data.

Acceptance: animation picker/preview and slot grid pass every custom state; Gaze’s 109 code rows bind through one adapter; rail actor changes update Gaze/Polish/Cues without claiming another view's edit state; Gaze Cues is a full inline editor; parity and the absolute audit pass for all five pages.

### WP9 — Camera, Lens, and VCam

Files: all six Camera and three Lens page XML files; camera switcher/operator/frame/orbit/recorder owners and new adapters; `llprismlens.*`, `pipeline.*`, preview/model/monitor/panel files, shaders, drag/drop files, settings, tests, and generated data listed by `VCAM_MONITORS_DESIGN.md`.

Acceptance: Shot slot grid/curves and Takes timeline pass; Diopter is not a launcher; VCam satisfies every package and gate in its design, is a `kit_page` under Camera, uses the sole `ALKitHistory`, and leaves `floater_director.xml` byte-identical; visible-display cadence precedes previews; CPU/GPU/scratch budget and drag-source tests pass; all nine pages pass parity/audit.

### WP10 — Light and Look

Files: six Light and five Look page XML files; cine-rig/Rim/Exposure/Practical/Actor/Cue controllers and adapters; Look action/provider files; generator data/config.

Acceptance: Rig/Rim retain P0 behavior; Exposure and Light Cues are inline; selected Practical and Actor targets have stable contexts; Image/Bloom/Roto/Tron/Actor FX are all inline with one writable home per concept; Tron is never a launcher; Light map live/off math and every Side-view state pass; all eleven pages pass parity/audit.

### WP11 — World and Capture

Files: three World and three Capture page XML files; weather/atmosphere/shadow/temporal controllers and adapters; capture-settings data; generator config.

Acceptance: Weather and Atmosphere shared panels are independently in-world checked; atmospheric volume identity/context survives undo; Capture has a complete non-empty row set; Temporal runtime commands use typed history or an explicit non-undoable status; all six pages pass parity/audit.

### WP12 — convergence and ship gate

Files: `llfloaterdirector.*`, generated v2 XML/index/report, lint/parity/layout/self-test tooling, test registrations, and review evidence under `doc/director_kit/final_review/`. The v1 floater and shared v1 XML remain byte-identical.

Acceptance:

- generator check, `kit_lint --all`, `director_parity.py --groups all`, XUI self-tests, UI-draw lint, unit tests, and default-v1 regression pass;
- the section-6 matrix produces zero findings and screenshots at all four sizes;
- all performance budgets, including VCam CPU/GPU p95 and frame-delta assertions, pass on the target machine;
- adversarial review reports zero must-fix; then one fresh build is made and the full in-world checklist passes;
- only after all preceding evidence exists may the normal v2 selection be exposed and made the shipped Director.

## 8. Final ship gate

Shipping is a single yes/no decision. The release candidate is **NO-GO** unless every item below is evidenced in the same build:

1. `gen_report.txt` says 36 kit / 0 legacy / 0 launcher / 0 incomplete, and `director_kit_gen.py --check` is clean.
2. `kit_lint.py --all`, `director_parity.py --groups all`, `xui_layout_check.py --selftest`, `ui_draw_lint.py --changed <base>`, and all registered TUT/scheduler tests pass with no waiver.
3. The exhaustive runtime audit in section 6 reports absolute zero findings at all four sizes and in all non-default states. Its state manifest and screenshots are archived in `doc/director_kit/final_review/`.
4. Director open, steady page, first reveal, search, amber, and VCam budgets all pass. VCam additionally has reliable CPU and GPU coverage and a passing p95 frame-delta assertion.
5. Undo/redo has one owner; bulk barriers, mode reveal, text-focus keyboard behavior, Prism typed operations, and scene-generation contexts pass their tests.
6. The v1 Director and its embedded XML are byte-identical; the default-v1 regression run is unchanged.
7. The adversarial review has zero blocker/major/minor must-fix findings, the fresh build succeeds, and the in-world matrix covers all 36 pages, ACTION/CUT, rail cast, every selector option, expanded cards, Advanced/Easy, shared-panel ownership, VCam drag-to-surface, Previews off, and context loss.
8. Only then is the normal menu/default rollout change allowed. If any item regresses, `DirectorUIV2` remains debug/self-test-only; there is no partially converted fallback release.
