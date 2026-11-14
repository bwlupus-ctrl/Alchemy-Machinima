# Groups — Director Kit page contract

Page key: `groups_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (0/14 setting-bound rows; 14 C++-referenced rows).

## Real owners

- `panel_director_move_src` / `panel` at `indra/newview/skins/default/xui/en/panel_director_move_src.xml:14` (8 matching rows).
- Referencing classes: `LLFloaterDirector` (38).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`LLFloaterDirector::bindGroupsPage` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `groups` / `groups_list` (Groups) | `scroll_list` `indra/newview/skins/default/xui/en/panel_director_move_src.xml:52` | `LLFloaterDirector::bindGroupsPage` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onGroupsListSelect()` (`indra/newview/llfloaterdirector.cpp:1644`), wired in `LLFloaterDirector::bindGroupsPage()`; behavior `LLFloaterDirector::onGroupsListSelect()` (`indra/newview/llfloaterdirector.cpp:2950`) | none |
| `groups` / `btn_group_new` (New) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `groups` / `group_rename_editor` (Rename to) | `line_editor` `indra/newview/skins/default/xui/en/panel_director_move_src.xml:101` | `LLFloaterDirector::bindGroupsPage` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onClickGroupRename()` (`indra/newview/llfloaterdirector.cpp:1646`), wired in `LLFloaterDirector::bindGroupsPage()`; behavior `LLFloaterDirector::onClickGroupRename()` (`indra/newview/llfloaterdirector.cpp:2952`) | none |
| `groups` / `btn_group_rename` (Rename) | `button` `indra/newview/skins/default/xui/en/panel_director_move_src.xml:112` | `LLFloaterDirector::bindGroupsPage` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onClickGroupRename()` (`indra/newview/llfloaterdirector.cpp:1649`), wired in `LLFloaterDirector::bindGroupsPage()`; behavior `LLFloaterDirector::onClickGroupRename()` (`indra/newview/llfloaterdirector.cpp:2955`) | none |
| `groups` / `btn_group_dissolve` (Dissolve) | `button` `indra/newview/skins/default/xui/en/panel_director_move_src.xml:91` | `LLFloaterDirector::bindGroupsPage` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onClickGroupDissolve()` (`indra/newview/llfloaterdirector.cpp:1650`), wired in `LLFloaterDirector::bindGroupsPage()`; behavior `LLFloaterDirector::onClickGroupDissolve()` (`indra/newview/llfloaterdirector.cpp:2956`) | none |
| `cast_group_members` / `group_members_list` (Members) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `cast_group_members` / `btn_group_add_selected` (Add selected cast) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `cast_group_members` / `btn_group_add` (Add...) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `cast_group_members` / `btn_group_remove` (Remove) | none | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `group_used_by` / `group_used_by` (Used by) | none | kit readout ← approved static copy; no runtime writer | none |
| `group_run` / `group_run_combo` (Group) | `combo_box` `indra/newview/skins/default/xui/en/panel_director_move_src.xml:23` | `LLFloaterDirector::bindGroupsPage` command/model binding → wired in `LLFloaterDirector::bindGroupsPage()`; behavior `LLFloaterDirector::onGroupsListSelect()` (`indra/newview/llfloaterdirector.cpp:2958`), wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onClickGroup()` (`indra/newview/llfloaterdirector.cpp:7093`) | none |
| `group_run` / `group_delay_spinner` (Delay (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_director_move_src.xml:80` | `LLFloaterDirector::bindGroupsPage` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onCommitGroupDelay()` (`indra/newview/llfloaterdirector.cpp:1645`), wired in `LLFloaterDirector::bindGroupsPage()`; behavior `LLFloaterDirector::onCommitGroupDelay()` (`indra/newview/llfloaterdirector.cpp:2951`) | none |
| `group_run` / `btn_group_start` (Start group) | `button` `indra/newview/skins/default/xui/en/panel_director_move_src.xml:33` | `LLFloaterDirector::bindGroupsPage` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onClickGroup()` (`indra/newview/llfloaterdirector.cpp:1634`), wired in `LLFloaterDirector::bindGroupsPage()`; behavior `LLFloaterDirector::onClickGroup()` (`indra/newview/llfloaterdirector.cpp:2948`) | none |
| `group_run` / `btn_group_stop` (Stop group) | `button` `indra/newview/skins/default/xui/en/panel_director_move_src.xml:42` | `LLFloaterDirector::bindGroupsPage` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onClickGroup()` (`indra/newview/llfloaterdirector.cpp:1635`), wired in `LLFloaterDirector::bindGroupsPage()`; behavior `LLFloaterDirector::onClickGroup()` (`indra/newview/llfloaterdirector.cpp:2949`) | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `LLFloaterDirector::onClickGroup()` — `indra/newview/llfloaterdirector.cpp:1634, 1635, 2948, 2949, 7093` | Move model-facing work into `LLFloaterDirector::bindGroupsPage` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::onClickGroupDissolve()` — `indra/newview/llfloaterdirector.cpp:1650, 2956` | Move model-facing work into `LLFloaterDirector::bindGroupsPage` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::onClickGroupRename()` — `indra/newview/llfloaterdirector.cpp:1646, 1649, 2952, 2955` | Move model-facing work into `LLFloaterDirector::bindGroupsPage` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::onCommitGroupDelay()` — `indra/newview/llfloaterdirector.cpp:1645, 2951` | Move model-facing work into `LLFloaterDirector::bindGroupsPage` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::onGroupsListSelect()` — `indra/newview/llfloaterdirector.cpp:1644, 2950, 2958` | Move model-facing work into `LLFloaterDirector::bindGroupsPage` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **17 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-groups-tallest-850` | Only `groups` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-groups-tallest-1160` | Only `groups` is put in its tallest local state at 1160 px; all other cards stay default. |
| `groups_list-empty` | `groups_list` is empty with no selection. |
| `groups_list-one` | `groups_list` has one selected item. |
| `groups_list-overflow` | `groups_list` has 51 items and a visible scrollbar. |
| `card-cast_group_members-tallest-850` | Only `cast_group_members` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-cast_group_members-tallest-1160` | Only `cast_group_members` is put in its tallest local state at 1160 px; all other cards stay default. |
| `group_members_list-empty` | `group_members_list` is empty with no selection. |
| `group_members_list-one` | `group_members_list` has one selected item. |
| `group_members_list-overflow` | `group_members_list` has 51 items and a visible scrollbar. |
| `card-group_used_by-tallest-850` | Only `group_used_by` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-group_used_by-tallest-1160` | Only `group_used_by` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-group_run-tallest-850` | Only `group_run` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-group_run-tallest-1160` | Only `group_run` is put in its tallest local state at 1160 px; all other cards stay default. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## Ambiguous same-name candidates (never auto-bound)

None.

## Rejected out-of-scope same-name candidates

22 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

## New widget required

None. Every approved visual kind is either a kit primitive or an existing XUI control. An unresolved binding is not permission to invent a visual class.

## Unresolved and pending-decision items

- `btn_group_new` (New): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `group_members_list` (Members): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `btn_group_add_selected` (Add selected cast): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `btn_group_add` (Add...): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `btn_group_remove` (Remove): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.

## Risks

- A widget-name collision can select an unrelated XUI root or C++ literal; ownership must be resolved by root, class, and behavior together.
- Extracting model work without rewiring the existing owner creates divergent v1/v2 behavior; the extraction and forwarding call are one reviewed change.
- Duplicate writable settings must obey the rollout homes table; this page emits a link/read-only row when it is not the chosen home.
- Shared edit models require owner tokens, and external presets/cue playback require one global history barrier at start/stop rather than per write.
- Unresolved rows fail generation closed; they are never silently replaced with a guessed widget or command.
