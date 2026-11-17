# Move — Director Kit page contract

Page key: `move_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (8/19 setting-bound rows; 10 C++-referenced rows).

## Real owners

- `panel_actor_mover` / `panel` at `indra/newview/skins/default/xui/en/panel_actor_mover.xml:19` (11 matching rows).
- `avatar_anim_speed` / `panel` at `indra/newview/skins/default/xui/en/panel_avatar_anim_speed.xml:3` (6 matching rows).
- Referencing classes: `LLFloaterDirector` (11), `ALPanelActorMover` (9).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALActorMoverKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `walk_who` / `scope_radio` (Who) | `radio_group` `indra/newview/skins/default/xui/en/panel_actor_mover.xml:29` | kit `combo` ↔ `ActorMoverSync` `Boolean`; `ALActorMoverKitAdapter` preserves/calls wired in `ALPanelActorMover::postBuild()`; behavior `ALPanelActorMover::onScopeCommit()` (`indra/newview/alpanelactormover.cpp:45`) | none |
| `walk_who` / `btn_walk` (Walk) | `button` `indra/newview/skins/default/xui/en/panel_actor_mover.xml:290` | `ALActorMoverKitAdapter` command/model binding → wired in `ALPanelActorMover::postBuild()`; behavior `ALPanelActorMover::onClickWalk()` (`indra/newview/alpanelactormover.cpp:75`) | none |
| `walk_who` / `btn_stop` (Stop / Release) | `button` `indra/newview/skins/default/xui/en/panel_actor_mover.xml:299` | `ALActorMoverKitAdapter` command/model binding → wired in `ALPanelActorMover::postBuild()`; behavior `ALPanelActorMover::onClickStop()` (`indra/newview/alpanelactormover.cpp:76`) | none |
| `straight` / `speed_slider` (Speed) | `slider` `indra/newview/skins/default/xui/en/panel_actor_mover.xml:116` | kit `slider` ↔ `ActorMoverSpeed` `F32` | `indra/newview/skins/default/xui/en/panel_actor_mover.xml:132` `speed_spinner`, `indra/newview/skins/default/xui/en/panel_dpage_cast_move.xml:24` `speed_slider` |
| `straight` / `dist_slider` (Distance) | `slider` `indra/newview/skins/default/xui/en/panel_actor_mover.xml:158` | kit `slider` ↔ `ActorMoverDistance` `F32` | `indra/newview/skins/default/xui/en/panel_actor_mover.xml:174` `dist_spinner`, `indra/newview/skins/default/xui/en/panel_dpage_cast_move.xml:30` `dist_slider` |
| `straight` / `heading_spinner` (Heading) | `spinner` `indra/newview/skins/default/xui/en/panel_actor_mover.xml:78` | kit `dial` ↔ `ActorMoverHeading` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_cast_move.xml:36` `heading_spinner` |
| `straight` / `end_mode_combo` (End behavior) | `combo_box` `indra/newview/skins/default/xui/en/panel_actor_mover.xml:236` | kit `combo` ↔ `ActorMoverEndMode` `S32` | page `props_tab`, `indra/newview/skins/default/xui/en/panel_dpage_cast_move.xml:42` `end_mode_combo` |
| `straight` / `move_hint` (Note) | `text` `indra/newview/skins/default/xui/en/panel_actor_mover.xml:87` | kit readout ← approved static copy; no runtime writer | none |
| `straight` / `cadence_slider` (Cadence) | `slider` `indra/newview/skins/default/xui/en/panel_actor_mover.xml:200` | kit `slider` ↔ `ActorMoverWalkNominal` `F32` | `indra/newview/skins/default/xui/en/panel_actor_mover.xml:216` `cadence_spinner`, `indra/newview/skins/default/xui/en/panel_dpage_cast_move.xml:56` `cadence_slider` |
| `custom_anim` / `custom_anim_editor` (Anim UUID) | `line_editor` `indra/newview/skins/default/xui/en/panel_actor_mover.xml:262` | kit `text` ↔ `ActorMoverCustomAnim` `String` | `indra/newview/skins/default/xui/en/panel_dpage_cast_move.xml:64` `custom_anim_editor` |
| `custom_anim` / `custom_anim_priority_combo` (Priority) | `combo_box` `indra/newview/skins/default/xui/en/panel_actor_mover.xml:272` | kit `combo` ↔ `ActorMoverCustomAnimPriority` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_cast_move.xml:67` `custom_anim_priority_combo` |
| `my_avatar` / `my_heading_dial` (My heading) | none | `ALActorMoverKitAdapter` command/model binding → wired in `ALPanelActorMover::postBuild()`; behavior `ALPanelActorMover::onMyHeadingCommit()` (`indra/newview/alpanelactormover.cpp:63`) | none |
| `anim_speed` / `anim_speed_slider` (Speed) | `slider` `indra/newview/skins/default/xui/en/panel_avatar_anim_speed.xml:7` | `ALActorMoverKitAdapter` command/model binding → behavior `LLFloaterDirector::bindAnimSpeedPage()` (`indra/newview/llfloaterdirector.cpp:2877`) | none |
| `anim_speed` / `anim_speed_reset` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_avatar_anim_speed.xml:11` | `ALActorMoverKitAdapter` command/model binding → behavior `LLFloaterDirector::bindAnimSpeedPage()` (`indra/newview/llfloaterdirector.cpp:2877`) | none |
| `anim_speed` / `anim_speed_spread` (Vary spread (%)) | `slider` `indra/newview/skins/default/xui/en/panel_avatar_anim_speed.xml:13` | `ALActorMoverKitAdapter` command/model binding → behavior `LLFloaterDirector::bindAnimSpeedPage()` (`indra/newview/llfloaterdirector.cpp:2877`) | none |
| `anim_speed` / `anim_speed_vary_all` (Vary all) | `button` `indra/newview/skins/default/xui/en/panel_avatar_anim_speed.xml:17` | `ALActorMoverKitAdapter` command/model binding → behavior `LLFloaterDirector::bindAnimSpeedPage()` (`indra/newview/llfloaterdirector.cpp:2877`) | none |
| `anim_speed` / `anim_speed_reset_all` (Reset all) | `button` `indra/newview/skins/default/xui/en/panel_avatar_anim_speed.xml:19` | `ALActorMoverKitAdapter` command/model binding → behavior `LLFloaterDirector::bindAnimSpeedPage()` (`indra/newview/llfloaterdirector.cpp:2877`) | none |
| `anim_speed` / `anim_speed_status` (Changed) | `text` `indra/newview/skins/default/xui/en/panel_avatar_anim_speed.xml:21` | kit readout ← approved static copy; no runtime writer | none |
| `manage_groups` / `manage_groups_link` (Manage groups...) | none | kit-native action `link_to:cast_group/groups_tab`; no duplicate widget is required | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALPanelActorMover::onClickStop()` — `indra/newview/alpanelactormover.cpp:76` | Move model-facing work into `ALActorMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelActorMover::onClickWalk()` — `indra/newview/alpanelactormover.cpp:75` | Move model-facing work into `ALActorMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelActorMover::onMyHeadingCommit()` — `indra/newview/alpanelactormover.cpp:63` | Move model-facing work into `ALActorMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelActorMover::onScopeCommit()` — `indra/newview/alpanelactormover.cpp:45` | Move model-facing work into `ALActorMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::bindAnimSpeedPage()` — `indra/newview/llfloaterdirector.cpp:2877` | Move model-facing work into `ALActorMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **30 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-walk_who-tallest-850` | Only `walk_who` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-walk_who-tallest-1160` | Only `walk_who` is put in its tallest local state at 1160 px; all other cards stay default. |
| `scope_radio-option-0` | Only `scope_radio` selects `Selected actors`; other selectors remain default. |
| `scope_radio-option-1` | Only `scope_radio` selects `Everyone`; other selectors remain default. |
| `card-straight-tallest-850` | Only `straight` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-straight-tallest-1160` | Only `straight` is put in its tallest local state at 1160 px; all other cards stay default. |
| `end_mode_combo-option-0` | Only `end_mode_combo` selects `Stop at end`; other selectors remain default. |
| `end_mode_combo-option-1` | Only `end_mode_combo` selects `Loop`; other selectors remain default. |
| `end_mode_combo-option-2` | Only `end_mode_combo` selects `Ping-pong`; other selectors remain default. |
| `card-custom_anim-tallest-850` | Only `custom_anim` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-custom_anim-tallest-1160` | Only `custom_anim` is put in its tallest local state at 1160 px; all other cards stay default. |
| `custom_anim_priority_combo-option-0` | Only `custom_anim_priority_combo` selects `Baked`; other selectors remain default. |
| `custom_anim_priority_combo-option-1` | Only `custom_anim_priority_combo` selects `Pri 0 - Low`; other selectors remain default. |
| `custom_anim_priority_combo-option-2` | Only `custom_anim_priority_combo` selects `Pri 1 - Medium`; other selectors remain default. |
| `custom_anim_priority_combo-option-3` | Only `custom_anim_priority_combo` selects `Pri 2 - High`; other selectors remain default. |
| `custom_anim_priority_combo-option-4` | Only `custom_anim_priority_combo` selects `Pri 3 - Higher`; other selectors remain default. |
| `custom_anim_priority_combo-option-5` | Only `custom_anim_priority_combo` selects `Pri 4 - Highest`; other selectors remain default. |
| `card-my_avatar-tallest-850` | Only `my_avatar` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-my_avatar-tallest-1160` | Only `my_avatar` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-anim_speed-tallest-850` | Only `anim_speed` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-anim_speed-tallest-1160` | Only `anim_speed` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-manage_groups-tallest-850` | Only `manage_groups` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-manage_groups-tallest-1160` | Only `manage_groups` is put in its tallest local state at 1160 px; all other cards stay default. |
| `actor-none` | Rail has no actor; target-dependent rows are disabled with an explicit empty state. |
| `actor-selected` | One rail actor is selected and unpinned. |
| `actor-pinned-rail-changes` | Page target is pinned while the rail changes to another actor. |
| `actor-removed` | The active actor disappears while the page is visible. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## Ambiguous same-name candidates (never auto-bound)

None.

## Rejected out-of-scope same-name candidates

32 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

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
