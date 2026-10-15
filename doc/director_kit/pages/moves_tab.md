# Moves — Director Kit page contract

Page key: `moves_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (11/14 setting-bound rows; 3 C++-referenced rows).

## Real owners

- `floater_director` / `floater` at `indra/newview/skins/default/xui/en/floater_director.xml:15` (14 matching rows).
- Referencing classes: `LLFloaterDirector` (9).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALCameraMoveKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `ots` / `btn_build_ots_pair` (Build OTS) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:1151` | `ALCameraMoveKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onClickBuildOtsPair()` (`indra/newview/llfloaterdirector.cpp:1643`) | none |
| `ots` / `prism_ots_shoulder` (Shoulder) | `spinner` `indra/newview/skins/default/xui/en/floater_director.xml:1156` | kit `number` ↔ `PrismOtsShoulderOffset` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_moves.xml:12` `prism_ots_shoulder` |
| `ots` / `prism_ots_distance` (Distance) | `spinner` `indra/newview/skins/default/xui/en/floater_director.xml:1160` | kit `number` ↔ `PrismOtsCameraDistance` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_moves.xml:15` `prism_ots_distance` |
| `ots` / `prism_ots_fov` (Lens FOV) | `spinner` `indra/newview/skins/default/xui/en/floater_director.xml:1164` | kit `number` ↔ `PrismOtsVerticalFovDeg` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_moves.xml:18` `prism_ots_fov` |
| `ots` / `prism_ots_height_bias` (Height bias) | `spinner` `indra/newview/skins/default/xui/en/floater_director.xml:1168` | kit `number` ↔ `PrismOtsHeightBias` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_moves.xml:21` `prism_ots_height_bias` |
| `dolly_zoom` / `btn_start_zolly` (Start Zolly) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:1170` | `ALCameraMoveKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onClickZolly()` (`indra/newview/llfloaterdirector.cpp:1645`) | none |
| `dolly_zoom` / `zolly_duration` (Duration) | `spinner` `indra/newview/skins/default/xui/en/floater_director.xml:1175` | kit `number` ↔ `CinematicCamVertigoDuration` `F32` | page `shot_tab`, `indra/newview/skins/default/xui/en/panel_cinecam_params.xml:517` `CinematicCamVertigoDuration`, `indra/newview/skins/default/xui/en/panel_dpage_camera_moves.xml:29` `zolly_duration`, `indra/newview/skins/default/xui/en/panel_dpage_camera_shot.xml:125` `CinematicCamVertigoDuration` |
| `dolly_zoom` / `zolly_direction` (Direction) | `combo_box` `indra/newview/skins/default/xui/en/floater_director.xml:1178` | kit `combo` ↔ `CinematicCamVertigoDirection` `S32` | page `shot_tab`, `indra/newview/skins/default/xui/en/panel_dpage_camera_moves.xml:32` `zolly_direction`, `indra/newview/skins/default/xui/en/panel_dpage_camera_shot.xml:143` `CinematicCamVertigoDirection` |
| `hero_arc` / `btn_start_hero_arc` (Start Arc) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:1183` | `ALCameraMoveKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onClickHeroArc()` (`indra/newview/llfloaterdirector.cpp:1647`) | none |
| `hero_arc` / `hero_arc_angle` (Angle) | `spinner` `indra/newview/skins/default/xui/en/floater_director.xml:1188` | kit `number` ↔ `CinematicCamArcGeneratorAngle` `F32` | page `shot_tab`, `indra/newview/skins/default/xui/en/panel_dpage_camera_moves.xml:45` `hero_arc_angle`, `indra/newview/skins/default/xui/en/panel_dpage_camera_shot.xml:301` `CinematicCamArcGeneratorAngle` |
| `hero_arc` / `hero_arc_duration` (Duration) | `spinner` `indra/newview/skins/default/xui/en/floater_director.xml:1192` | kit `number` ↔ `CinematicCamArcDuration` `F32` | page `shot_tab`, `indra/newview/skins/default/xui/en/panel_cinecam_params.xml:734` `CinematicCamArcDuration`, `indra/newview/skins/default/xui/en/panel_dpage_camera_moves.xml:48` `hero_arc_duration`, `indra/newview/skins/default/xui/en/panel_dpage_camera_shot.xml:283` `CinematicCamArcDuration` |
| `hero_arc` / `hero_arc_height_drift` (Height drift) | `spinner` `indra/newview/skins/default/xui/en/floater_director.xml:1196` | kit `number` ↔ `CinematicCamArcHeightDrift` `F32` | page `shot_tab`, `indra/newview/skins/default/xui/en/panel_dpage_camera_moves.xml:51` `hero_arc_height_drift`, `indra/newview/skins/default/xui/en/panel_dpage_camera_shot.xml:304` `CinematicCamArcHeightDrift` |
| `hero_arc` / `hero_arc_speed_ramp` (Speed ramp) | `check_box` `indra/newview/skins/default/xui/en/floater_director.xml:1199` | kit `toggle` ↔ `CinematicCamSpeedRampEnabled` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_camera_moves.xml:54` `hero_arc_speed_ramp` |
| `hero_arc` / `hero_arc_ramp_strength` (Ramp strength) | `spinner` `indra/newview/skins/default/xui/en/floater_director.xml:1204` | kit `number` ↔ `CinematicCamSpeedRampStrength` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_camera_moves.xml:57` `hero_arc_ramp_strength` |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `LLFloaterDirector::onClickBuildOtsPair()` — `indra/newview/llfloaterdirector.cpp:1643` | Move model-facing work into `ALCameraMoveKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::onClickHeroArc()` — `indra/newview/llfloaterdirector.cpp:1647` | Move model-facing work into `ALCameraMoveKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::onClickZolly()` — `indra/newview/llfloaterdirector.cpp:1645` | Move model-facing work into `ALCameraMoveKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **12 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-ots-tallest-850` | Only `ots` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-ots-tallest-1160` | Only `ots` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-dolly_zoom-tallest-850` | Only `dolly_zoom` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-dolly_zoom-tallest-1160` | Only `dolly_zoom` is put in its tallest local state at 1160 px; all other cards stay default. |
| `zolly_direction-option-0` | Only `zolly_direction` selects `Dolly out`; other selectors remain default. |
| `zolly_direction-option-1` | Only `zolly_direction` selects `Dolly in`; other selectors remain default. |
| `card-hero_arc-tallest-850` | Only `hero_arc` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-hero_arc-tallest-1160` | Only `hero_arc` is put in its tallest local state at 1160 px; all other cards stay default. |
| `hero_arc_speed_ramp-on` | Only `hero_arc_speed_ramp` is on; dependent visibility is measured. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## Ambiguous same-name candidates (never auto-bound)

None.

## Rejected out-of-scope same-name candidates

14 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

## New widget required

None. Every approved visual kind is either a kit primitive or an existing XUI control. An unresolved binding is not permission to invent a visual class.

## Unresolved and pending-decision items

None in the current evidence index.

## Risks

- A widget-name collision can select an unrelated XUI root or C++ literal; ownership must be resolved by root, class, and behavior together.
- Extracting model work without rewiring the existing owner creates divergent v1/v2 behavior; the extraction and forwarding call are one reviewed change.
- Duplicate writable settings must obey the rollout homes table; this page emits a link/read-only row when it is not the chosen home.
- Shared edit models require owner tokens, and external presets/cue playback require one global history barrier at start/stop rather than per write.
- Unresolved rows fail generation closed; they are never silently replaced with a guessed widget or command.
