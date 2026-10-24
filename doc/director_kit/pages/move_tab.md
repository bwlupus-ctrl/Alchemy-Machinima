# Move — Director Kit page contract

Page key: `move_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (8/19 setting-bound rows; 11 C++-referenced rows).

## Real owners

- `panel_actor_mover` / `panel` at `indra/newview/skins/default/xui/en/panel_actor_mover.xml:19` (11 matching rows).
- `floater_director` / `floater` at `indra/newview/skins/default/xui/en/floater_director.xml:15` (8 matching rows).
- Referencing classes: `LLFloaterDirector` (31), `ALPanelActorMover` (7).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALActorMoverKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `walk_who` / `scope_radio` (Who) | `radio_group` `indra/newview/skins/default/xui/en/panel_actor_mover.xml:29` | kit `combo` ↔ `ActorMoverSync` `Boolean`; `ALActorMoverKitAdapter` preserves/calls wired in `ALPanelActorMover::postBuild()`; behavior `ALPanelActorMover::onScopeCommit()` (`indra/newview/alpanelactormover.cpp:40`) | none |
| `walk_who` / `btn_walk` (Walk) | `button` `indra/newview/skins/default/xui/en/panel_actor_mover.xml:290` | `ALActorMoverKitAdapter` command/model binding → wired in `ALPanelActorMover::postBuild()`; behavior `ALPanelActorMover::onClickWalk()` (`indra/newview/alpanelactormover.cpp:64`) | none |
| `walk_who` / `btn_stop` (Stop / Release) | `button` `indra/newview/skins/default/xui/en/panel_actor_mover.xml:299` | `ALActorMoverKitAdapter` command/model binding → wired in `ALPanelActorMover::postBuild()`; behavior `ALPanelActorMover::onClickStop()` (`indra/newview/alpanelactormover.cpp:65`) | none |
| `straight` / `speed_slider` (Speed) | `slider` `indra/newview/skins/default/xui/en/panel_actor_mover.xml:116` | kit `slider` ↔ `ActorMoverSpeed` `F32` | `indra/newview/skins/default/xui/en/panel_actor_mover.xml:132` `speed_spinner`, `indra/newview/skins/default/xui/en/panel_dpage_cast_move.xml:23` `speed_slider` |
| `straight` / `dist_slider` (Distance) | `slider` `indra/newview/skins/default/xui/en/panel_actor_mover.xml:158` | kit `slider` ↔ `ActorMoverDistance` `F32` | `indra/newview/skins/default/xui/en/panel_actor_mover.xml:174` `dist_spinner`, `indra/newview/skins/default/xui/en/panel_dpage_cast_move.xml:29` `dist_slider` |
| `straight` / `heading_spinner` (Heading) | `spinner` `indra/newview/skins/default/xui/en/panel_actor_mover.xml:78` | kit `dial` ↔ `ActorMoverHeading` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_cast_move.xml:35` `heading_spinner` |
| `straight` / `end_mode_combo` (End behavior) | `combo_box` `indra/newview/skins/default/xui/en/panel_actor_mover.xml:236` | kit `combo` ↔ `ActorMoverEndMode` `S32` | page `props_tab`, `indra/newview/skins/default/xui/en/panel_dpage_cast_move.xml:41` `end_mode_combo` |
| `straight` / `move_hint` (Note) | `text` `indra/newview/skins/default/xui/en/panel_actor_mover.xml:87` | kit readout ← approved static copy; no runtime writer | none |
| `straight` / `cadence_slider` (Cadence) | `slider` `indra/newview/skins/default/xui/en/panel_actor_mover.xml:200` | kit `slider` ↔ `ActorMoverWalkNominal` `F32` | `indra/newview/skins/default/xui/en/panel_actor_mover.xml:216` `cadence_spinner`, `indra/newview/skins/default/xui/en/panel_dpage_cast_move.xml:55` `cadence_slider` |
| `custom_anim` / `custom_anim_editor` (Anim UUID) | `line_editor` `indra/newview/skins/default/xui/en/panel_actor_mover.xml:262` | kit `text` ↔ `ActorMoverCustomAnim` `String` | `indra/newview/skins/default/xui/en/panel_dpage_cast_move.xml:63` `custom_anim_editor` |
| `custom_anim` / `custom_anim_priority_combo` (Priority) | `combo_box` `indra/newview/skins/default/xui/en/panel_actor_mover.xml:272` | kit `combo` ↔ `ActorMoverCustomAnimPriority` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_cast_move.xml:66` `custom_anim_priority_combo` |
| `group_run` / `group_run_combo` (Group) | `combo_box` `indra/newview/skins/default/xui/en/floater_director.xml:383` | `ALActorMoverKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onClickGroup()` (`indra/newview/llfloaterdirector.cpp:2107`) | none |
| `group_run` / `group_delay_spinner` (Delay (s)) | `spinner` `indra/newview/skins/default/xui/en/floater_director.xml:456` | `ALActorMoverKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onCommitGroupDelay()` (`indra/newview/llfloaterdirector.cpp:1466`), wired in `LLFloaterDirector::bindMovePage()`; behavior `LLFloaterDirector::onCommitGroupDelay()` (`indra/newview/llfloaterdirector.cpp:2398`) | none |
| `group_run` / `btn_group_start` (Start group) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:393` | `ALActorMoverKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onClickGroup()` (`indra/newview/llfloaterdirector.cpp:1455`), wired in `LLFloaterDirector::bindMovePage()`; behavior `LLFloaterDirector::onClickGroup()` (`indra/newview/llfloaterdirector.cpp:2395`) | none |
| `group_run` / `btn_group_stop` (Stop group) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:402` | `ALActorMoverKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onClickGroup()` (`indra/newview/llfloaterdirector.cpp:1456`), wired in `LLFloaterDirector::bindMovePage()`; behavior `LLFloaterDirector::onClickGroup()` (`indra/newview/llfloaterdirector.cpp:2396`) | none |
| `groups` / `groups_list` (Groups) | `scroll_list` `indra/newview/skins/default/xui/en/floater_director.xml:429` | `ALActorMoverKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onGroupsListSelect()` (`indra/newview/llfloaterdirector.cpp:1465`), wired in `LLFloaterDirector::bindMovePage()`; behavior `LLFloaterDirector::onGroupsListSelect()` (`indra/newview/llfloaterdirector.cpp:2397`) | none |
| `groups` / `group_rename_editor` (Rename to) | `line_editor` `indra/newview/skins/default/xui/en/floater_director.xml:477` | `ALActorMoverKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onClickGroupRename()` (`indra/newview/llfloaterdirector.cpp:1467`), wired in `LLFloaterDirector::bindMovePage()`; behavior `LLFloaterDirector::onClickGroupRename()` (`indra/newview/llfloaterdirector.cpp:2399`) | none |
| `groups` / `btn_group_rename` (Rename) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:488` | `ALActorMoverKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onClickGroupRename()` (`indra/newview/llfloaterdirector.cpp:1468`), wired in `LLFloaterDirector::bindMovePage()`; behavior `LLFloaterDirector::onClickGroupRename()` (`indra/newview/llfloaterdirector.cpp:2400`) | none |
| `groups` / `btn_group_dissolve` (Dissolve) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:467` | `ALActorMoverKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onClickGroupDissolve()` (`indra/newview/llfloaterdirector.cpp:1469`), wired in `LLFloaterDirector::bindMovePage()`; behavior `LLFloaterDirector::onClickGroupDissolve()` (`indra/newview/llfloaterdirector.cpp:2401`) | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALPanelActorMover::onClickStop()` — `indra/newview/alpanelactormover.cpp:65` | Move model-facing work into `ALActorMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelActorMover::onClickWalk()` — `indra/newview/alpanelactormover.cpp:64` | Move model-facing work into `ALActorMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelActorMover::onScopeCommit()` — `indra/newview/alpanelactormover.cpp:40` | Move model-facing work into `ALActorMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::onClickGroup()` — `indra/newview/llfloaterdirector.cpp:1455, 1456, 2107, 2395, 2396` | Move model-facing work into `ALActorMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::onClickGroupDissolve()` — `indra/newview/llfloaterdirector.cpp:1469, 2401` | Move model-facing work into `ALActorMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::onClickGroupRename()` — `indra/newview/llfloaterdirector.cpp:1467, 1468, 2399, 2400` | Move model-facing work into `ALActorMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::onCommitGroupDelay()` — `indra/newview/llfloaterdirector.cpp:1466, 2398` | Move model-facing work into `ALActorMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::onGroupsListSelect()` — `indra/newview/llfloaterdirector.cpp:1465, 2397` | Move model-facing work into `ALActorMoverKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **31 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

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
| `card-group_run-tallest-850` | Only `group_run` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-group_run-tallest-1160` | Only `group_run` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-groups-tallest-850` | Only `groups` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-groups-tallest-1160` | Only `groups` is put in its tallest local state at 1160 px; all other cards stay default. |
| `groups_list-empty` | `groups_list` is empty with no selection. |
| `groups_list-one` | `groups_list` has one selected item. |
| `groups_list-overflow` | `groups_list` has 51 items and a visible scrollbar. |
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

41 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

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
