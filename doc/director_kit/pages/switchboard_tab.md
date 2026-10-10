# Switchboard — Director Kit page contract

Page key: `switchboard_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (5/32 setting-bound rows; 26 C++-referenced rows).

## Real owners

- `panel_director_anim_switcher` / `panel` at `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:11` (32 matching rows).
- Referencing classes: `ALPanelDirectorAnimSwitcher` (51).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Keep the twelve existing `anim_slot_01`…`anim_slot_12` buttons as measured kit button rows/cells; do not create a slot-grid widget.

## Row bindings

`ALAnimSwitcherKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `arm_timing` / `anim_switcher_armed` (Armed) | `check_box` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:31` | kit `toggle` ↔ `DirectorAnimSwitcherArmed` `Boolean` | none |
| `arm_timing` / `anim_switcher_auto` (Auto) | `check_box` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:42` | kit `toggle` ↔ `DirectorAnimSwitcherAuto` `Boolean` | none |
| `arm_timing` / `anim_interval` (Every (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:351` | kit `number` ↔ `DirectorAnimSwitcherIntervalSec` `F32` | none |
| `arm_timing` / `anim_jitter` (Jitter (s)) | `spinner` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:367` | kit `number` ↔ `DirectorAnimSwitcherJitterSec` `F32` | none |
| `arm_timing` / `anim_sequence` (Sequence) | `check_box` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:378` | kit `toggle` ↔ `DirectorAnimSwitcherSequence` `Boolean` | none |
| `arm_timing` / `anim_program_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:52` | kit readout ← approved static copy; no runtime writer | none |
| `slots` / `anim_help_hint` (How to fill) | `text` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:388` | kit readout ← approved static copy; no runtime writer | none |
| `slots` / `anim_slot_01` (Slot 1) | `button` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:61` | `ALAnimSwitcherKitAdapter` command/model binding → wired in `ALPanelDirectorAnimSwitcher::postBuild()`; behavior `ALPanelDirectorAnimSwitcher::onSlotButton()` (`indra/newview/alpaneldirectoranimswitcher.cpp:59`) | none |
| `slots` / `anim_slot_02` (Slot 2) | `button` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:62` | `ALAnimSwitcherKitAdapter` command/model binding → wired in `ALPanelDirectorAnimSwitcher::postBuild()`; behavior `ALPanelDirectorAnimSwitcher::onSlotButton()` (`indra/newview/alpaneldirectoranimswitcher.cpp:59`) | none |
| `slots` / `anim_slot_03` (Slot 3) | `button` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:63` | `ALAnimSwitcherKitAdapter` command/model binding → wired in `ALPanelDirectorAnimSwitcher::postBuild()`; behavior `ALPanelDirectorAnimSwitcher::onSlotButton()` (`indra/newview/alpaneldirectoranimswitcher.cpp:59`) | none |
| `slots` / `anim_slot_04` (Slot 4) | `button` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:64` | `ALAnimSwitcherKitAdapter` command/model binding → wired in `ALPanelDirectorAnimSwitcher::postBuild()`; behavior `ALPanelDirectorAnimSwitcher::onSlotButton()` (`indra/newview/alpaneldirectoranimswitcher.cpp:59`) | none |
| `slots` / `anim_slot_05` (Slot 5) | `button` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:65` | `ALAnimSwitcherKitAdapter` command/model binding → wired in `ALPanelDirectorAnimSwitcher::postBuild()`; behavior `ALPanelDirectorAnimSwitcher::onSlotButton()` (`indra/newview/alpaneldirectoranimswitcher.cpp:59`) | none |
| `slots` / `anim_slot_06` (Slot 6) | `button` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:66` | `ALAnimSwitcherKitAdapter` command/model binding → wired in `ALPanelDirectorAnimSwitcher::postBuild()`; behavior `ALPanelDirectorAnimSwitcher::onSlotButton()` (`indra/newview/alpaneldirectoranimswitcher.cpp:59`) | none |
| `slots` / `anim_slot_07` (Slot 7) | `button` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:67` | `ALAnimSwitcherKitAdapter` command/model binding → wired in `ALPanelDirectorAnimSwitcher::postBuild()`; behavior `ALPanelDirectorAnimSwitcher::onSlotButton()` (`indra/newview/alpaneldirectoranimswitcher.cpp:59`) | none |
| `slots` / `anim_slot_08` (Slot 8) | `button` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:68` | `ALAnimSwitcherKitAdapter` command/model binding → wired in `ALPanelDirectorAnimSwitcher::postBuild()`; behavior `ALPanelDirectorAnimSwitcher::onSlotButton()` (`indra/newview/alpaneldirectoranimswitcher.cpp:59`) | none |
| `slots` / `anim_slot_09` (Slot 9) | `button` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:69` | `ALAnimSwitcherKitAdapter` command/model binding → wired in `ALPanelDirectorAnimSwitcher::postBuild()`; behavior `ALPanelDirectorAnimSwitcher::onSlotButton()` (`indra/newview/alpaneldirectoranimswitcher.cpp:59`) | none |
| `slots` / `anim_slot_10` (Slot 10) | `button` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:70` | `ALAnimSwitcherKitAdapter` command/model binding → wired in `ALPanelDirectorAnimSwitcher::postBuild()`; behavior `ALPanelDirectorAnimSwitcher::onSlotButton()` (`indra/newview/alpaneldirectoranimswitcher.cpp:59`) | none |
| `slots` / `anim_slot_11` (Slot 11) | `button` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:71` | `ALAnimSwitcherKitAdapter` command/model binding → wired in `ALPanelDirectorAnimSwitcher::postBuild()`; behavior `ALPanelDirectorAnimSwitcher::onSlotButton()` (`indra/newview/alpaneldirectoranimswitcher.cpp:59`) | none |
| `slots` / `anim_slot_12` (Slot 12) | `button` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:72` | `ALAnimSwitcherKitAdapter` command/model binding → wired in `ALPanelDirectorAnimSwitcher::postBuild()`; behavior `ALPanelDirectorAnimSwitcher::onSlotButton()` (`indra/newview/alpaneldirectoranimswitcher.cpp:59`) | none |
| `selected_slot` / `anim_slot_kind` (Kind) | `combo_box` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:100` | `ALAnimSwitcherKitAdapter` command/model binding → wired in `ALPanelDirectorAnimSwitcher::postBuild()`; behavior `ALPanelDirectorAnimSwitcher::onSlotKind()` (`indra/newview/alpaneldirectoranimswitcher.cpp:82`) | none |
| `selected_slot` / `anim_slot_uuid` (Anim UUID) | `line_editor` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:135` | `ALAnimSwitcherKitAdapter` command/model binding → wired in `ALPanelDirectorAnimSwitcher::postBuild()`; behavior `ALPanelDirectorAnimSwitcher::onAnimUUID()` (`indra/newview/alpaneldirectoranimswitcher.cpp:84`) | none |
| `selected_slot` / `anim_from_explorer` (From Explorer) | `button` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:145` | `ALAnimSwitcherKitAdapter` command/model binding → wired in `ALPanelDirectorAnimSwitcher::postBuild()`; behavior `ALPanelDirectorAnimSwitcher::onFromExplorer()` (`indra/newview/alpaneldirectoranimswitcher.cpp:90`) | none |
| `selected_slot` / `anim_slot_label` (Label) | `line_editor` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:214` | `ALAnimSwitcherKitAdapter` command/model binding → wired in `ALPanelDirectorAnimSwitcher::postBuild()`; behavior `ALPanelDirectorAnimSwitcher::onSlotLabel()` (`indra/newview/alpaneldirectoranimswitcher.cpp:92`) | none |
| `selected_slot` / `anim_slot_target` (Target) | `combo_box` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:234` | `ALAnimSwitcherKitAdapter` command/model binding → wired in `ALPanelDirectorAnimSwitcher::postBuild()`; behavior `ALPanelDirectorAnimSwitcher::onSlotTarget()` (`indra/newview/alpaneldirectoranimswitcher.cpp:94`) | none |
| `selected_slot` / `anim_slot_reset` (Reset slot) | `button` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:113` | `ALAnimSwitcherKitAdapter` command/model binding → wired in `ALPanelDirectorAnimSwitcher::postBuild()`; behavior `ALPanelDirectorAnimSwitcher::onResetSlot()` (`indra/newview/alpaneldirectoranimswitcher.cpp:106`) | none |
| `selected_slot` / `anim_pose_name` (Pose file) | `combo_box` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:167` | `ALAnimSwitcherKitAdapter` command/model binding → wired in `ALPanelDirectorAnimSwitcher::postBuild()`; behavior `ALPanelDirectorAnimSwitcher::onPoseName()` (`indra/newview/alpaneldirectoranimswitcher.cpp:86`) | none |
| `selected_slot` / `anim_pose_load_method` (Pose apply) | `combo_box` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:178` | `ALAnimSwitcherKitAdapter` command/model binding → wired in `ALPanelDirectorAnimSwitcher::postBuild()`; behavior `ALPanelDirectorAnimSwitcher::onPoseLoadMethod()` (`indra/newview/alpaneldirectoranimswitcher.cpp:88`) | none |
| `playback` / `anim_slot_priority` (Priority) | `combo_box` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:257` | `ALAnimSwitcherKitAdapter` command/model binding → wired in `ALPanelDirectorAnimSwitcher::postBuild()`; behavior `ALPanelDirectorAnimSwitcher::onSlotPriority()` (`indra/newview/alpaneldirectoranimswitcher.cpp:96`) | none |
| `playback` / `anim_slot_speed` (Speed) | `spinner` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:283` | `ALAnimSwitcherKitAdapter` command/model binding → wired in `ALPanelDirectorAnimSwitcher::postBuild()`; behavior `ALPanelDirectorAnimSwitcher::onSlotSpeed()` (`indra/newview/alpaneldirectoranimswitcher.cpp:98`) | none |
| `playback` / `anim_slot_loop` (Loop) | `check_box` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:293` | `ALAnimSwitcherKitAdapter` command/model binding → wired in `ALPanelDirectorAnimSwitcher::postBuild()`; behavior `ALPanelDirectorAnimSwitcher::onSlotLoop()` (`indra/newview/alpaneldirectoranimswitcher.cpp:100`) | none |
| `playback` / `anim_slot_snap` (Snap on cut) | `check_box` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:303` | `ALAnimSwitcherKitAdapter` command/model binding → wired in `ALPanelDirectorAnimSwitcher::postBuild()`; behavior `ALPanelDirectorAnimSwitcher::onSlotSnap()` (`indra/newview/alpaneldirectoranimswitcher.cpp:102`) | none |
| `playback` / `anim_slot_enabled` (Use in auto) | `check_box` `indra/newview/skins/default/xui/en/panel_director_anim_switcher.xml:314` | `ALAnimSwitcherKitAdapter` command/model binding → wired in `ALPanelDirectorAnimSwitcher::postBuild()`; behavior `ALPanelDirectorAnimSwitcher::onSlotEnabled()` (`indra/newview/alpaneldirectoranimswitcher.cpp:104`) | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALPanelDirectorAnimSwitcher::onAnimUUID()` — `indra/newview/alpaneldirectoranimswitcher.cpp:84` | Move model-facing work into `ALAnimSwitcherKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDirectorAnimSwitcher::onFromExplorer()` — `indra/newview/alpaneldirectoranimswitcher.cpp:90` | Move model-facing work into `ALAnimSwitcherKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDirectorAnimSwitcher::onPoseLoadMethod()` — `indra/newview/alpaneldirectoranimswitcher.cpp:88` | Move model-facing work into `ALAnimSwitcherKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDirectorAnimSwitcher::onPoseName()` — `indra/newview/alpaneldirectoranimswitcher.cpp:86` | Move model-facing work into `ALAnimSwitcherKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDirectorAnimSwitcher::onResetSlot()` — `indra/newview/alpaneldirectoranimswitcher.cpp:106` | Move model-facing work into `ALAnimSwitcherKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDirectorAnimSwitcher::onSlotButton()` — `indra/newview/alpaneldirectoranimswitcher.cpp:59` | Move model-facing work into `ALAnimSwitcherKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDirectorAnimSwitcher::onSlotEnabled()` — `indra/newview/alpaneldirectoranimswitcher.cpp:104` | Move model-facing work into `ALAnimSwitcherKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDirectorAnimSwitcher::onSlotKind()` — `indra/newview/alpaneldirectoranimswitcher.cpp:82` | Move model-facing work into `ALAnimSwitcherKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDirectorAnimSwitcher::onSlotLabel()` — `indra/newview/alpaneldirectoranimswitcher.cpp:92` | Move model-facing work into `ALAnimSwitcherKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDirectorAnimSwitcher::onSlotLoop()` — `indra/newview/alpaneldirectoranimswitcher.cpp:100` | Move model-facing work into `ALAnimSwitcherKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDirectorAnimSwitcher::onSlotPriority()` — `indra/newview/alpaneldirectoranimswitcher.cpp:96` | Move model-facing work into `ALAnimSwitcherKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDirectorAnimSwitcher::onSlotSnap()` — `indra/newview/alpaneldirectoranimswitcher.cpp:102` | Move model-facing work into `ALAnimSwitcherKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDirectorAnimSwitcher::onSlotSpeed()` — `indra/newview/alpaneldirectoranimswitcher.cpp:98` | Move model-facing work into `ALAnimSwitcherKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelDirectorAnimSwitcher::onSlotTarget()` — `indra/newview/alpaneldirectoranimswitcher.cpp:94` | Move model-facing work into `ALAnimSwitcherKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **45 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-arm_timing-tallest-850` | Only `arm_timing` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-arm_timing-tallest-1160` | Only `arm_timing` is put in its tallest local state at 1160 px; all other cards stay default. |
| `anim_switcher_armed-on` | Only `anim_switcher_armed` is on; dependent visibility is measured. |
| `anim_switcher_auto-on` | Only `anim_switcher_auto` is on; dependent visibility is measured. |
| `anim_sequence-on` | Only `anim_sequence` is on; dependent visibility is measured. |
| `card-slots-tallest-850` | Only `slots` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-slots-tallest-1160` | Only `slots` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-selected_slot-tallest-850` | Only `selected_slot` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-selected_slot-tallest-1160` | Only `selected_slot` is put in its tallest local state at 1160 px; all other cards stay default. |
| `anim_slot_kind-option-0` | Only `anim_slot_kind` selects `Anim`; other selectors remain default. |
| `anim_slot_kind-option-1` | Only `anim_slot_kind` selects `Pose`; other selectors remain default. |
| `anim_slot_target-option-0` | Only `anim_slot_target` selects `Cast`; other selectors remain default. |
| `anim_slot_target-option-1` | Only `anim_slot_target` selects `Self`; other selectors remain default. |
| `anim_slot_target-option-2` | Only `anim_slot_target` selects `Ghosts`; other selectors remain default. |
| `anim_pose_name-option-0` | Only `anim_pose_name` selects `(empty)`; other selectors remain default. |
| `anim_pose_load_method-option-0` | Only `anim_pose_load_method` selects `Rot + pos + scales`; other selectors remain default. |
| `anim_pose_load_method-option-1` | Only `anim_pose_load_method` selects `Rotations only`; other selectors remain default. |
| `anim_pose_load_method-option-2` | Only `anim_pose_load_method` selects `Positions only`; other selectors remain default. |
| `anim_pose_load_method-option-3` | Only `anim_pose_load_method` selects `Scales only`; other selectors remain default. |
| `anim_pose_load_method-option-4` | Only `anim_pose_load_method` selects `Rot + positions`; other selectors remain default. |
| `anim_pose_load_method-option-5` | Only `anim_pose_load_method` selects `Rot + scales`; other selectors remain default. |
| `anim_pose_load_method-option-6` | Only `anim_pose_load_method` selects `Pos + scales`; other selectors remain default. |
| `anim_pose_load_method-option-7` | Only `anim_pose_load_method` selects `Right hand`; other selectors remain default. |
| `anim_pose_load_method-option-8` | Only `anim_pose_load_method` selects `Left hand`; other selectors remain default. |
| `anim_pose_load_method-option-9` | Only `anim_pose_load_method` selects `Face only`; other selectors remain default. |
| `anim_pose_load_method-option-10` | Only `anim_pose_load_method` selects `Selective`; other selectors remain default. |
| `anim_pose_load_method-option-11` | Only `anim_pose_load_method` selects `Selective rot`; other selectors remain default. |
| `card-playback-tallest-850` | Only `playback` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-playback-tallest-1160` | Only `playback` is put in its tallest local state at 1160 px; all other cards stay default. |
| `anim_slot_priority-option-0` | Only `anim_slot_priority` selects `Default`; other selectors remain default. |
| `anim_slot_priority-option-1` | Only `anim_slot_priority` selects `Low (0)`; other selectors remain default. |
| `anim_slot_priority-option-2` | Only `anim_slot_priority` selects `Medium (1)`; other selectors remain default. |
| `anim_slot_priority-option-3` | Only `anim_slot_priority` selects `High (2)`; other selectors remain default. |
| `anim_slot_priority-option-4` | Only `anim_slot_priority` selects `Higher (3)`; other selectors remain default. |
| `anim_slot_priority-option-5` | Only `anim_slot_priority` selects `Highest (4)`; other selectors remain default. |
| `anim_slot_priority-option-6` | Only `anim_slot_priority` selects `Priority 5`; other selectors remain default. |
| `anim_slot_priority-option-7` | Only `anim_slot_priority` selects `Priority 6`; other selectors remain default. |
| `anim_slot_priority-option-8` | Only `anim_slot_priority` selects `Additive (7)`; other selectors remain default. |
| `anim_slot_loop-on` | Only `anim_slot_loop` is on; dependent visibility is measured. |
| `anim_slot_snap-on` | Only `anim_slot_snap` is on; dependent visibility is measured. |
| `anim_slot_enabled-on` | Only `anim_slot_enabled` is on; dependent visibility is measured. |
| `selection-deleted` | The selected model row is removed by another owner while visible. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## Ambiguous same-name candidates (never auto-bound)

None.

## Rejected out-of-scope same-name candidates

None.

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
