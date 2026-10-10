# Animate — Director Kit page contract

Page key: `animate_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (2/24 setting-bound rows; 20 C++-referenced rows).

## Real owners

- `floater_director` / `floater` at `indra/newview/skins/default/xui/en/floater_director.xml:15` (19 matching rows).
- `panel_anim_preview` / `panel` at `indra/newview/skins/default/xui/en/panel_anim_preview.xml:17` (5 matching rows).
- Referencing classes: `LLFloaterDirector` (16), `ALPanelAnimPreview` (4).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALAnimateKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `selected_member` / `anim_list` (Playing now) | `scroll_list` `indra/newview/skins/default/xui/en/floater_director.xml:622` | `ALAnimateKitAdapter` command/model binding → `LLFloaterDirector::postBuild()` (`indra/newview/llfloaterdirector.cpp:512`) | none |
| `selected_member` / `btn_anim_copy` (Copy UUID) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:661` | `ALAnimateKitAdapter` command/model binding → `LLFloaterDirector::postBuild()` (`indra/newview/llfloaterdirector.cpp:517`) | none |
| `selected_member` / `btn_anim_setloco` (Set role) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:670` | `ALAnimateKitAdapter` command/model binding → `LLFloaterDirector::postBuild()` (`indra/newview/llfloaterdirector.cpp:518`) | none |
| `selected_member` / `btn_anim_play` (Play local) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:679` | `ALAnimateKitAdapter` command/model binding → `LLFloaterDirector::postBuild()` (`indra/newview/llfloaterdirector.cpp:519`) | none |
| `selected_member` / `btn_anim_stop` (Stop local) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:688` | `ALAnimateKitAdapter` command/model binding → `LLFloaterDirector::postBuild()` (`indra/newview/llfloaterdirector.cpp:520`) | none |
| `selected_member` / `anim_hint` (Hint) | `text` `indra/newview/skins/default/xui/en/floater_director.xml:648` | `ALAnimateKitAdapter` command/model binding → `LLFloaterDirector::postBuild()` (`indra/newview/llfloaterdirector.cpp:513`) | none |
| `paste_uuid` / `paste_lbl` (Paste UUID) | `text` `indra/newview/skins/default/xui/en/floater_director.xml:696` | kit readout ← approved static copy; no runtime writer | none |
| `paste_uuid` / `paste_uuid_editor` (Asset UUID) | `line_editor` `indra/newview/skins/default/xui/en/floater_director.xml:707` | `ALAnimateKitAdapter` command/model binding → `LLFloaterDirector::postBuild()` (`indra/newview/llfloaterdirector.cpp:521`) | none |
| `paste_uuid` / `btn_paste_play` (Play local) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:717` | `ALAnimateKitAdapter` command/model binding → `LLFloaterDirector::postBuild()` (`indra/newview/llfloaterdirector.cpp:522`) | none |
| `paste_uuid` / `btn_paste_stop` (Stop local) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:726` | `ALAnimateKitAdapter` command/model binding → `LLFloaterDirector::postBuild()` (`indra/newview/llfloaterdirector.cpp:523`) | none |
| `paste_uuid` / `btn_paste_setloco` (Set role) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:735` | `ALAnimateKitAdapter` command/model binding → `LLFloaterDirector::postBuild()` (`indra/newview/llfloaterdirector.cpp:524`) | none |
| `paste_uuid` / `play_local_priority_combo` (Priority) | `combo_box` `indra/newview/skins/default/xui/en/floater_director.xml:744` | kit `combo` ↔ `DirectorAnimPlayLocalPriority` `S32` | none |
| `locomotion` / `locomotion_role_combo` (Role) | `combo_box` `indra/newview/skins/default/xui/en/floater_director.xml:770` | `ALAnimateKitAdapter` command/model binding → `LLFloaterDirector::postBuild()` (`indra/newview/llfloaterdirector.cpp:525`) | none |
| `locomotion` / `locomotion_speed` (Travel m/s) | `spinner` `indra/newview/skins/default/xui/en/floater_director.xml:789` | `ALAnimateKitAdapter` command/model binding → `LLFloaterDirector::postBuild()` (`indra/newview/llfloaterdirector.cpp:526`) | none |
| `locomotion` / `locomotion_phase` (Foot phase) | `spinner` `indra/newview/skins/default/xui/en/floater_director.xml:803` | `ALAnimateKitAdapter` command/model binding → `LLFloaterDirector::postBuild()` (`indra/newview/llfloaterdirector.cpp:527`) | none |
| `locomotion` / `locomotion_allow_ao` (Allow AO) | `check_box` `indra/newview/skins/default/xui/en/floater_director.xml:813` | `ALAnimateKitAdapter` command/model binding → `LLFloaterDirector::postBuild()` (`indra/newview/llfloaterdirector.cpp:528`) | none |
| `locomotion` / `btn_clear_loco` (Clear) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:836` | `ALAnimateKitAdapter` command/model binding → `LLFloaterDirector::postBuild()` (`indra/newview/llfloaterdirector.cpp:531`) | none |
| `locomotion` / `btn_open_animexp` (Open Explorer) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:873` | `ALAnimateKitAdapter` command/model binding → `LLFloaterDirector::postBuild()` (`indra/newview/llfloaterdirector.cpp:548`) | none |
| `locomotion` / `locomotion_editor_label` (Speed & phase) | `text` `indra/newview/skins/default/xui/en/floater_director.xml:760` | kit readout ← approved static copy; no runtime writer | none |
| `anim_preview` / `animation_preview` (Preview) | `view_border` `indra/newview/skins/default/xui/en/panel_anim_preview.xml:26` | `ALAnimateKitAdapter` command/model binding → `ALPanelAnimPreview::postBuild()` (`indra/newview/alpanelanimpreview.cpp:56`) | none |
| `anim_preview` / `capture_all_check` (Capture all) | `check_box` `indra/newview/skins/default/xui/en/panel_anim_preview.xml:49` | kit `toggle` ↔ `AnimExplorerCaptureAll` `Boolean` | none |
| `anim_preview` / `btn_anim_ex_stop` (Stop) | `button` `indra/newview/skins/default/xui/en/panel_anim_preview.xml:59` | `ALAnimateKitAdapter` command/model binding → `ALPanelAnimPreview::postBuild()` (`indra/newview/alpanelanimpreview.cpp:63`) | none |
| `anim_preview` / `btn_anim_ex_revoke` (Stop+revoke) | `button` `indra/newview/skins/default/xui/en/panel_anim_preview.xml:68` | `ALAnimateKitAdapter` command/model binding → `ALPanelAnimPreview::postBuild()` (`indra/newview/alpanelanimpreview.cpp:64`) | none |
| `anim_preview` / `btn_anim_ex_blacklist` (Blacklist) | `button` `indra/newview/skins/default/xui/en/panel_anim_preview.xml:77` | `ALAnimateKitAdapter` command/model binding → `ALPanelAnimPreview::postBuild()` (`indra/newview/alpanelanimpreview.cpp:65`) | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALPanelAnimPreview::postBuild()` — `indra/newview/alpanelanimpreview.cpp:56, 63, 64, 65` | Move model-facing work into `ALAnimateKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::postBuild()` — `indra/newview/llfloaterdirector.cpp:512, 513, 517, 518, 519, 520, 521, 522, 523, 524, 525, 526, 527, 528, 531, 548` | Move model-facing work into `ALAnimateKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **32 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-selected_member-tallest-850` | Only `selected_member` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-selected_member-tallest-1160` | Only `selected_member` is put in its tallest local state at 1160 px; all other cards stay default. |
| `anim_list-empty` | `anim_list` is empty with no selection. |
| `anim_list-one` | `anim_list` has one selected item. |
| `anim_list-overflow` | `anim_list` has 51 items and a visible scrollbar. |
| `card-paste_uuid-tallest-850` | Only `paste_uuid` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-paste_uuid-tallest-1160` | Only `paste_uuid` is put in its tallest local state at 1160 px; all other cards stay default. |
| `play_local_priority_combo-option-0` | Only `play_local_priority_combo` selects `Baked`; other selectors remain default. |
| `play_local_priority_combo-option-1` | Only `play_local_priority_combo` selects `Pri 0 - Low`; other selectors remain default. |
| `play_local_priority_combo-option-2` | Only `play_local_priority_combo` selects `Pri 1 - Medium`; other selectors remain default. |
| `play_local_priority_combo-option-3` | Only `play_local_priority_combo` selects `Pri 2 - High`; other selectors remain default. |
| `play_local_priority_combo-option-4` | Only `play_local_priority_combo` selects `Pri 3 - Higher`; other selectors remain default. |
| `play_local_priority_combo-option-5` | Only `play_local_priority_combo` selects `Pri 4 - Highest`; other selectors remain default. |
| `card-locomotion-tallest-850` | Only `locomotion` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-locomotion-tallest-1160` | Only `locomotion` is put in its tallest local state at 1160 px; all other cards stay default. |
| `locomotion_role_combo-option-0` | Only `locomotion_role_combo` selects `Idle`; other selectors remain default. |
| `locomotion_role_combo-option-1` | Only `locomotion_role_combo` selects `Walk forward`; other selectors remain default. |
| `locomotion_role_combo-option-2` | Only `locomotion_role_combo` selects `Run forward`; other selectors remain default. |
| `locomotion_role_combo-option-3` | Only `locomotion_role_combo` selects `Fly`; other selectors remain default. |
| `locomotion_role_combo-option-4` | Only `locomotion_role_combo` selects `Hover`; other selectors remain default. |
| `locomotion_allow_ao-on` | Only `locomotion_allow_ao` is on; dependent visibility is measured. |
| `card-anim_preview-tallest-850` | Only `anim_preview` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-anim_preview-tallest-1160` | Only `anim_preview` is put in its tallest local state at 1160 px; all other cards stay default. |
| `capture_all_check-on` | Only `capture_all_check` is on; dependent visibility is measured. |
| `actor-none` | Rail has no actor; target-dependent rows are disabled with an explicit empty state. |
| `actor-selected` | One rail actor is selected and unpinned. |
| `actor-pinned-rail-changes` | Page target is pinned while the rail changes to another actor. |
| `actor-removed` | The active actor disappears while the page is visible. |
| `selection-deleted` | The selected model row is removed by another owner while visible. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## New widget required

None. Every approved visual kind is either a kit primitive or an existing XUI control. Missing action/model bindings below are USER DECISIONS, not permission to invent a visual class.

## Unresolved and USER DECISION items

- `paste_lbl` (Paste UUID): no C++ reference or setting binding.
- `locomotion_editor_label` (Speed & phase): no C++ reference or setting binding.

## Risks

- A widget-name collision can select an unrelated XUI root or C++ literal; ownership must be resolved by root, class, and behavior together.
- Extracting model work without rewiring the existing owner creates divergent v1/v2 behavior; the extraction and forwarding call are one reviewed change.
- Duplicate writable settings must obey the rollout homes table; this page emits a link/read-only row when it is not the chosen home.
- Shared edit models require owner tokens, and external presets/cue playback require one global history barrier at start/stop rather than per write.
- Unresolved rows fail generation closed; they are never silently replaced with a guessed widget or command.
