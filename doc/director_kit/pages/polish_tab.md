# Polish — Director Kit page contract

Page key: `polish_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (13/15 setting-bound rows; 1 C++-referenced rows).

## Real owners

- `panel_pose_polish` / `panel` at `indra/newview/skins/default/xui/en/panel_pose_polish.xml:2` (14 matching rows).
- `floater_director` / `floater` at `indra/newview/skins/default/xui/en/floater_director.xml:15` (1 matching rows).
- Referencing classes: `LLFloaterReg` (1), `LLFloaterDirector` (1).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALPosePolishKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `windows` / `polish_popout` (Open window) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:920` | `ALPosePolishKitAdapter` command/model binding → wired in `LLFloaterDirector::postBuild()`; behavior `LLFloaterReg::showInstance()` (`indra/newview/llfloaterdirector.cpp:924`) | none |
| `master_inertia` / `polish_master_note` (Note) | `text` `indra/newview/skins/default/xui/en/panel_pose_polish.xml:19` | kit readout ← approved static copy; no runtime writer | none |
| `master_inertia` / `polish_inertia_check` (Inertia) | `check_box` `indra/newview/skins/default/xui/en/panel_pose_polish.xml:41` | kit `toggle` ↔ `ALPolishInertiaEnabled` `Boolean` | none |
| `master_inertia` / `polish_inertia_spine_slider` (Spine settle) | `slider` `indra/newview/skins/default/xui/en/panel_pose_polish.xml:57` | kit `slider` ↔ `ALPolishInertiaHalfLifeSpine` `F32` | none |
| `master_inertia` / `polish_inertia_arm_slider` (Arm settle) | `slider` `indra/newview/skins/default/xui/en/panel_pose_polish.xml:73` | kit `slider` ↔ `ALPolishInertiaHalfLifeArm` `F32` | none |
| `master_inertia` / `polish_inertia_leg_slider` (Leg settle) | `slider` `indra/newview/skins/default/xui/en/panel_pose_polish.xml:89` | kit `slider` ↔ `ALPolishInertiaHalfLifeLeg` `F32` | none |
| `master_inertia` / `polish_inertia_head_slider` (Head settle) | `slider` `indra/newview/skins/default/xui/en/panel_pose_polish.xml:105` | kit `slider` ↔ `ALPolishInertiaHalfLifeHead` `F32` | none |
| `foot_timing` / `polish_contact_check` (Foot contact) | `check_box` `indra/newview/skins/default/xui/en/panel_pose_polish.xml:126` | kit `toggle` ↔ `ALPolishContactEnabled` `Boolean` | none |
| `foot_timing` / `polish_contact_weight_slider` (Strength) | `slider` `indra/newview/skins/default/xui/en/panel_pose_polish.xml:142` | kit `slider` ↔ `ALPolishContactWeight` `F32` | none |
| `foot_timing` / `polish_phasematch_check` (Phase match) | `check_box` `indra/newview/skins/default/xui/en/panel_pose_polish.xml:163` | kit `toggle` ↔ `ALPolishPhaseMatchEnabled` `Boolean` | none |
| `foot_timing` / `polish_loopseam_check` (Loop seam) | `check_box` `indra/newview/skins/default/xui/en/panel_pose_polish.xml:184` | kit `toggle` ↔ `ALPolishLoopSeamEnabled` `Boolean` | none |
| `secondary` / `polish_secondary_check` (Secondary) | `check_box` `indra/newview/skins/default/xui/en/panel_pose_polish.xml:205` | kit `toggle` ↔ `ALPolishSecondaryEnabled` `Boolean` | none |
| `secondary` / `polish_secondary_breath_slider` (Breath) | `slider` `indra/newview/skins/default/xui/en/panel_pose_polish.xml:221` | kit `slider` ↔ `ALPolishSecondaryBreathAmp` `F32` | none |
| `secondary` / `polish_secondary_sway_slider` (Sway) | `slider` `indra/newview/skins/default/xui/en/panel_pose_polish.xml:237` | kit `slider` ↔ `ALPolishSecondarySwayAmp` `F32` | none |
| `diagnostics` / `polish_diagnostic_check` (Layer overlay) | `check_box` `indra/newview/skins/default/xui/en/panel_pose_polish.xml:258` | kit `toggle` ↔ `ALPolishLayerDiagnostic` `Boolean` | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `LLFloaterReg::showInstance()` — `indra/newview/llfloaterdirector.cpp:924` | Move model-facing work into `ALPosePolishKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **23 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-windows-tallest-850` | Only `windows` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-windows-tallest-1160` | Only `windows` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-master_inertia-tallest-850` | Only `master_inertia` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-master_inertia-tallest-1160` | Only `master_inertia` is put in its tallest local state at 1160 px; all other cards stay default. |
| `polish_inertia_check-on` | Only `polish_inertia_check` is on; dependent visibility is measured. |
| `card-foot_timing-tallest-850` | Only `foot_timing` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-foot_timing-tallest-1160` | Only `foot_timing` is put in its tallest local state at 1160 px; all other cards stay default. |
| `polish_contact_check-on` | Only `polish_contact_check` is on; dependent visibility is measured. |
| `polish_phasematch_check-on` | Only `polish_phasematch_check` is on; dependent visibility is measured. |
| `polish_loopseam_check-on` | Only `polish_loopseam_check` is on; dependent visibility is measured. |
| `card-secondary-tallest-850` | Only `secondary` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-secondary-tallest-1160` | Only `secondary` is put in its tallest local state at 1160 px; all other cards stay default. |
| `polish_secondary_check-on` | Only `polish_secondary_check` is on; dependent visibility is measured. |
| `card-diagnostics-tallest-850` | Only `diagnostics` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-diagnostics-tallest-1160` | Only `diagnostics` is put in its tallest local state at 1160 px; all other cards stay default. |
| `polish_diagnostic_check-on` | Only `polish_diagnostic_check` is on; dependent visibility is measured. |
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

1 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

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
