# Animate — Director Kit page contract

Page key: `animate_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (2/26 setting-bound rows; 22 C++-referenced rows).

## Real owners

- `floater_director` / `floater` at `indra/newview/skins/default/xui/en/floater_director.xml:15` (21 matching rows).
- `panel_anim_preview` / `panel` at `indra/newview/skins/default/xui/en/panel_anim_preview.xml:17` (5 matching rows).
- Referencing classes: `LLFloaterDirector` (59), `ALPanelAnimPreview` (11), `LLFloaterReg` (1).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALAnimateKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `selected_member` / `animate_header` (Animating) | `text` `indra/newview/skins/default/xui/en/floater_director.xml:611` | kit readout ← approved static copy; no runtime writer | none |
| `selected_member` / `anim_list` (Playing now) | `scroll_list` `indra/newview/skins/default/xui/en/floater_director.xml:624` | `ALAnimateKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::refreshAnimateTab()` (`indra/newview/llfloaterdirector.cpp:2992`) | none |
| `selected_member` / `btn_anim_copy` (Copy UUID) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:663` | `ALAnimateKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onAnimCopyUUID()` (`indra/newview/llfloaterdirector.cpp:1548`), wired in `LLFloaterDirector::bindAnimatePage()`; behavior `LLFloaterDirector::onAnimCopyUUID()` (`indra/newview/llfloaterdirector.cpp:2532`) | none |
| `selected_member` / `btn_anim_setloco` (Set role) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:672` | `ALAnimateKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onAnimSetLoco()` (`indra/newview/llfloaterdirector.cpp:1549`), wired in `LLFloaterDirector::bindAnimatePage()`; behavior `LLFloaterDirector::onAnimSetLoco()` (`indra/newview/llfloaterdirector.cpp:2533`) | none |
| `selected_member` / `btn_anim_play` (Play local) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:681` | `ALAnimateKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onAnimPlayLocal()` (`indra/newview/llfloaterdirector.cpp:1550`), wired in `LLFloaterDirector::bindAnimatePage()`; behavior `LLFloaterDirector::onAnimPlayLocal()` (`indra/newview/llfloaterdirector.cpp:2534`) | none |
| `selected_member` / `btn_anim_stop` (Stop local) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:690` | `ALAnimateKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onAnimPlayLocal()` (`indra/newview/llfloaterdirector.cpp:1551`), wired in `LLFloaterDirector::bindAnimatePage()`; behavior `LLFloaterDirector::onAnimPlayLocal()` (`indra/newview/llfloaterdirector.cpp:2535`) | none |
| `selected_member` / `anim_hint` (Hint) | `text` `indra/newview/skins/default/xui/en/floater_director.xml:650` | kit readout ← approved static copy; no runtime writer | none |
| `paste_uuid` / `paste_lbl` (Paste UUID) | `text` `indra/newview/skins/default/xui/en/floater_director.xml:698` | kit readout ← approved static copy; no runtime writer | none |
| `paste_uuid` / `paste_uuid_editor` (Asset UUID) | `line_editor` `indra/newview/skins/default/xui/en/floater_director.xml:709` | `ALAnimateKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::pasteAnimId()` (`indra/newview/llfloaterdirector.cpp:2705`) | none |
| `paste_uuid` / `btn_paste_play` (Play local) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:719` | `ALAnimateKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onPastePlayLocal()` (`indra/newview/llfloaterdirector.cpp:1552`), wired in `LLFloaterDirector::bindAnimatePage()`; behavior `LLFloaterDirector::onPastePlayLocal()` (`indra/newview/llfloaterdirector.cpp:2536`) | none |
| `paste_uuid` / `btn_paste_stop` (Stop local) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:728` | `ALAnimateKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onPastePlayLocal()` (`indra/newview/llfloaterdirector.cpp:1553`), wired in `LLFloaterDirector::bindAnimatePage()`; behavior `LLFloaterDirector::onPastePlayLocal()` (`indra/newview/llfloaterdirector.cpp:2537`) | none |
| `paste_uuid` / `btn_paste_setloco` (Set role) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:737` | `ALAnimateKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onPasteSetLoco()` (`indra/newview/llfloaterdirector.cpp:1554`), wired in `LLFloaterDirector::bindAnimatePage()`; behavior `LLFloaterDirector::onPasteSetLoco()` (`indra/newview/llfloaterdirector.cpp:2538`) | none |
| `paste_uuid` / `play_local_priority_combo` (Priority) | `combo_box` `indra/newview/skins/default/xui/en/floater_director.xml:746` | kit `combo` ↔ `DirectorAnimPlayLocalPriority` `S32` | `indra/newview/skins/default/xui/en/panel_dpage_performance_animate.xml:51` `play_local_priority_combo` |
| `locomotion` / `locomotion_role_combo` (Role) | `combo_box` `indra/newview/skins/default/xui/en/floater_director.xml:772` | `ALAnimateKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onLocomotionRoleChanged()` (`indra/newview/llfloaterdirector.cpp:1556`), wired in `LLFloaterDirector::bindAnimatePage()`; behavior `LLFloaterDirector::onLocomotionRoleChanged()` (`indra/newview/llfloaterdirector.cpp:2540`) | none |
| `locomotion` / `locomotion_speed` (Travel m/s) | `spinner` `indra/newview/skins/default/xui/en/floater_director.xml:791` | `ALAnimateKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onLocomotionMetadataCommit()` (`indra/newview/llfloaterdirector.cpp:2875`) | none |
| `locomotion` / `locomotion_phase` (Foot phase) | `spinner` `indra/newview/skins/default/xui/en/floater_director.xml:805` | `ALAnimateKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onLocomotionMetadataCommit()` (`indra/newview/llfloaterdirector.cpp:2875`) | none |
| `locomotion` / `locomotion_allow_ao` (Allow AO) | `check_box` `indra/newview/skins/default/xui/en/floater_director.xml:815` | `ALAnimateKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onLocomotionMetadataCommit()` (`indra/newview/llfloaterdirector.cpp:2875`) | none |
| `locomotion` / `loco_text` (Status) | `text` `indra/newview/skins/default/xui/en/floater_director.xml:825` | kit readout ← approved static copy; no runtime writer | none |
| `locomotion` / `btn_clear_loco` (Clear) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:838` | `ALAnimateKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterDirector::onClickClearLoco()` (`indra/newview/llfloaterdirector.cpp:1555`), wired in `LLFloaterDirector::bindAnimatePage()`; behavior `LLFloaterDirector::onClickClearLoco()` (`indra/newview/llfloaterdirector.cpp:2539`) | none |
| `locomotion` / `btn_open_animexp` (Open Explorer) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:875` | `ALAnimateKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterReg::showInstance()` (`indra/newview/llfloaterdirector.cpp:1564`) | none |
| `locomotion` / `locomotion_editor_label` (Speed & phase) | `text` `indra/newview/skins/default/xui/en/floater_director.xml:762` | kit readout ← approved static copy; no runtime writer | none |
| `anim_preview` / `animation_preview` (Preview) | `view_border` `indra/newview/skins/default/xui/en/panel_anim_preview.xml:26` | `ALAnimateKitAdapter` command/model binding → wired in `ALPanelAnimPreview::postBuild()`; behavior `ALPanelAnimPreview::previewAnim()` (`indra/newview/alpanelanimpreview.cpp:75`) | none |
| `anim_preview` / `capture_all_check` (Capture all) | `check_box` `indra/newview/skins/default/xui/en/panel_anim_preview.xml:49` | kit `toggle` ↔ `AnimExplorerCaptureAll` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_performance_animate.xml:102` `capture_all_check` |
| `anim_preview` / `btn_anim_ex_stop` (Stop) | `button` `indra/newview/skins/default/xui/en/panel_anim_preview.xml:59` | `ALAnimateKitAdapter` command/model binding → wired in `ALPanelAnimPreview::wireActionButtons()`; behavior `ALPanelAnimPreview::onStop()` (`indra/newview/alpanelanimpreview.cpp:108`) | none |
| `anim_preview` / `btn_anim_ex_revoke` (Stop+revoke) | `button` `indra/newview/skins/default/xui/en/panel_anim_preview.xml:68` | `ALAnimateKitAdapter` command/model binding → wired in `ALPanelAnimPreview::wireActionButtons()`; behavior `ALPanelAnimPreview::onStopAndRevoke()` (`indra/newview/alpanelanimpreview.cpp:112`) | none |
| `anim_preview` / `btn_anim_ex_blacklist` (Blacklist) | `button` `indra/newview/skins/default/xui/en/panel_anim_preview.xml:77` | `ALAnimateKitAdapter` command/model binding → wired in `ALPanelAnimPreview::wireActionButtons()`; behavior `ALPanelAnimPreview::onBlacklist()` (`indra/newview/alpanelanimpreview.cpp:116`) | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALPanelAnimPreview::onBlacklist()` — `indra/newview/alpanelanimpreview.cpp:116` | Move model-facing work into `ALAnimateKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelAnimPreview::onStop()` — `indra/newview/alpanelanimpreview.cpp:108` | Move model-facing work into `ALAnimateKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelAnimPreview::onStopAndRevoke()` — `indra/newview/alpanelanimpreview.cpp:112` | Move model-facing work into `ALAnimateKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelAnimPreview::previewAnim()` — `indra/newview/alpanelanimpreview.cpp:75` | Move model-facing work into `ALAnimateKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::onAnimCopyUUID()` — `indra/newview/llfloaterdirector.cpp:1548, 2532` | Move model-facing work into `ALAnimateKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::onAnimPlayLocal()` — `indra/newview/llfloaterdirector.cpp:1550, 1551, 2534, 2535` | Move model-facing work into `ALAnimateKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::onAnimSetLoco()` — `indra/newview/llfloaterdirector.cpp:1549, 2533` | Move model-facing work into `ALAnimateKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::onClickClearLoco()` — `indra/newview/llfloaterdirector.cpp:1555, 2539` | Move model-facing work into `ALAnimateKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::onLocomotionMetadataCommit()` — `indra/newview/llfloaterdirector.cpp:2875` | Move model-facing work into `ALAnimateKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::onLocomotionRoleChanged()` — `indra/newview/llfloaterdirector.cpp:1556, 2540` | Move model-facing work into `ALAnimateKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::onPastePlayLocal()` — `indra/newview/llfloaterdirector.cpp:1552, 1553, 2536, 2537` | Move model-facing work into `ALAnimateKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::onPasteSetLoco()` — `indra/newview/llfloaterdirector.cpp:1554, 2538` | Move model-facing work into `ALAnimateKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::pasteAnimId()` — `indra/newview/llfloaterdirector.cpp:2705` | Move model-facing work into `ALAnimateKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::refreshAnimateTab()` — `indra/newview/llfloaterdirector.cpp:2992` | Move model-facing work into `ALAnimateKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterReg::showInstance()` — `indra/newview/llfloaterdirector.cpp:1564` | Move model-facing work into `ALAnimateKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

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
