# Stepped — Director Kit page contract

Page key: `stepped_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (33/48 setting-bound rows; 17 C++-referenced rows).

## Real owners

- `stepped_tab_logic` / `panel` at `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:3` (44 matching rows).
- Referencing classes: `ALPanelSteppedAnim` (21).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALSteppedAnimKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `stepped` / `sa_scope` (Applies to) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:11` | kit `combo` ↔ `StepAnimScope` `S32` | none |
| `stepped` / `sa_actor_target` (Actor) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:20` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `stepped` / `sa_override_list` (Overrides) | `scroll_list` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:30` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `stepped` / `sa_actor_override` (This actor) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:33` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `stepped` / `sa_actor_rate` (Own rate) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:42` | `ALSteppedAnimKitAdapter` command/model binding → behavior `ALPanelSteppedAnim::bindSteppedPage()` (`indra/newview/alpanelsteppedanim.cpp:66`) | none |
| `stepped` / `sa_actor_steps` (Own steps) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:47` | `ALSteppedAnimKitAdapter` command/model binding → behavior `ALPanelSteppedAnim::bindSteppedPage()` (`indra/newview/alpanelsteppedanim.cpp:67`) | none |
| `stepped` / `sa_actor_role` (Mixed role) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:56` | `ALSteppedAnimKitAdapter` command/model binding → behavior `ALPanelSteppedAnim::bindSteppedPage()` (`indra/newview/alpanelsteppedanim.cpp:64`) | none |
| `stepped` / `sa_override_clear` (Clear) | `button` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:65` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `stepped` / `sa_exclude_mounted` (Skip camera rigs) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:68` | kit `toggle` ↔ `StepAnimExcludeCameraMounted` `Boolean` | none |
| `stepped` / `sa_max_avatars` (Max actors) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:71` | kit `number` ↔ `StepAnimMaxAvatars` `S32` | none |
| `stepped` / `sa_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:74` | kit readout ← approved static copy; no runtime writer | none |
| `rate` / `sa_mode` (Mode) | `radio_group` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:79` | kit `segmented` ↔ `StepAnimMode` `S32` | none |
| `rate` / `sa_rate_chips` (On) | `radio_group` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:86` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `rate` / `sa_rate` (Rate) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:94` | kit `slider` ↔ `StepAnimRate` `F32` | none |
| `rate` / `sa_rate_readout` (Reads as) | `text` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:99` | kit readout ← approved static copy; no runtime writer | none |
| `rate` / `sa_fast_rate` (Fast rate) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:102` | kit `slider` ↔ `StepAnimFastRate` `F32`; `ALSteppedAnimKitAdapter` preserves/calls behavior `ALPanelSteppedAnim::bindSteppedPage()` (`indra/newview/alpanelsteppedanim.cpp:58`) | none |
| `rate` / `sa_adapt_threshold` (Threshold) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:107` | kit `slider` ↔ `StepAnimAdaptThreshold` `F32`; `ALSteppedAnimKitAdapter` preserves/calls behavior `ALPanelSteppedAnim::bindSteppedPage()` (`indra/newview/alpanelsteppedanim.cpp:59`) | none |
| `rate` / `sa_adapt_hysteresis` (Hysteresis) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:112` | kit `slider` ↔ `StepAnimAdaptHysteresis` `F32`; `ALSteppedAnimKitAdapter` preserves/calls behavior `ALPanelSteppedAnim::bindSteppedPage()` (`indra/newview/alpanelsteppedanim.cpp:60`) | none |
| `rate` / `sa_adapt_dwell` (Min hold) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:117` | kit `slider` ↔ `StepAnimAdaptMinDwell` `F32`; `ALSteppedAnimKitAdapter` preserves/calls behavior `ALPanelSteppedAnim::bindSteppedPage()` (`indra/newview/alpanelsteppedanim.cpp:61`) | none |
| `rate` / `sa_speed_meter` (Limb speed) | `text` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:122` | `ALSteppedAnimKitAdapter` command/model binding → behavior `ALPanelSteppedAnim::bindSteppedPage()` (`indra/newview/alpanelsteppedanim.cpp:62`) | none |
| `rate` / `sa_background_rate` (Background) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:125` | kit `slider` ↔ `StepAnimBackgroundRate` `F32`; `ALSteppedAnimKitAdapter` preserves/calls behavior `ALPanelSteppedAnim::bindSteppedPage()` (`indra/newview/alpanelsteppedanim.cpp:63`) | none |
| `what_steps` / `sa_what` (Steps) | `radio_group` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:132` | kit `segmented` ↔ `StepAnimWhatSteps` `S32` | none |
| `what_steps` / `sa_face` (Face & lip-sync) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:138` | kit `toggle` ↔ `StepAnimFace` `Boolean` | none |
| `what_steps` / `sa_gaze` (Gaze & look-at) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:141` | kit `toggle` ↔ `StepAnimGaze` `Boolean` | none |
| `what_steps` / `sa_flexi` (Hair, cloth & flexi) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:144` | kit `toggle` ↔ `StepAnimFlexi` `Boolean` | none |
| `what_steps` / `sa_attach_fx` (Attachment FX) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:147` | kit `toggle` ↔ `StepAnimAttachFX` `Boolean` | none |
| `what_steps` / `sa_particles` (Step particles) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:150` | kit `toggle` ↔ `StepAnimParticles` `Boolean` | none |
| `what_steps` / `sa_texture_anim` (Step texture animation) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:153` | kit `toggle` ↔ `StepAnimTextureAnim` `Boolean` | none |
| `step_timing` / `sa_clock` (Clock) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:158` | kit `combo` ↔ `StepAnimClock` `S32` | none |
| `step_timing` / `sa_offset` (Actor offset) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:166` | kit `combo` ↔ `StepAnimOffsetMode` `S32` | none |
| `step_timing` / `sa_phase` (Phase) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:175` | kit `slider` ↔ `StepAnimPhase` `F32`; `ALSteppedAnimKitAdapter` preserves/calls behavior `ALPanelSteppedAnim::bindSteppedPage()` (`indra/newview/alpanelsteppedanim.cpp:65`) | none |
| `step_timing` / `sa_base_fps` (Base fps) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:180` | kit `combo` ↔ `StepAnimBaseFPS` `S32` | none |
| `character` / `sa_boil` (Pose boil) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:191` | kit `toggle` ↔ `StepAnimBoil` `Boolean` | none |
| `character` / `sa_boil_amount` (Amount) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:194` | kit `slider` ↔ `StepAnimBoilAmount` `F32` | none |
| `character` / `sa_boil_ticks` (Boil every) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:199` | kit `number` ↔ `StepAnimBoilTicks` `S32` | none |
| `character` / `sa_impact` (Hold on impact) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:202` | kit `toggle` ↔ `StepAnimImpactHold` `Boolean` | none |
| `character` / `sa_impact_ticks` (Frames) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:205` | kit `number` ↔ `StepAnimImpactTicks` `S32` | none |
| `character` / `sa_impact_threshold` (Threshold) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:208` | kit `slider` ↔ `StepAnimImpactThreshold` `F32` | none |
| `character` / `sa_smear` (Smear frames) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:213` | kit `toggle` ↔ `StepAnimSmear` `Boolean` | none |
| `character` / `sa_smear_strength` (Strength) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:216` | kit `slider` ↔ `StepAnimSmearStrength` `F32` | none |
| `character` / `sa_smear_threshold` (Smear speed) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:221` | kit `slider` ↔ `StepAnimSmearThreshold` `F32` | none |
| `character` / `sa_ease` (Ease spacing) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:226` | kit `toggle` ↔ `StepAnimEase` `Boolean` | none |
| `character` / `sa_ease_amount` (Amount) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:229` | kit `slider` ↔ `StepAnimEaseAmount` `F32` | none |
| `pairs` / `sa_match_roto` (Match Roto Ink) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:236` | kit `toggle` ↔ `StepAnimMatchRotoInk` `Boolean` | none |
| `pairs` / `sa_roto_link` (Roto Ink) | none | kit-native action `link_to:look_group/stylize_tab`; no duplicate widget is required | none |
| `pairs` / `sa_tron_link` (Tron) | none | kit-native action `link_to:look_group/tron_tab`; no duplicate widget is required | none |
| `pairs` / `sa_actorfx_link` (Actor FX) | none | kit-native action `link_to:look_group/actor_style_tab`; no duplicate widget is required | none |
| `pairs` / `sa_time_link` (World clock) | none | kit-native action `link_to:capture_group/temporal_tab`; no duplicate widget is required | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALPanelSteppedAnim::bindSteppedPage()` — `indra/newview/alpanelsteppedanim.cpp:58, 59, 60, 61, 62, 63, 64, 65, 66, 67` | Move model-facing work into `ALSteppedAnimKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **54 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-stepped-tallest-850` | Only `stepped` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-stepped-tallest-1160` | Only `stepped` is put in its tallest local state at 1160 px; all other cards stay default. |
| `sa_scope-option-0` | Only `sa_scope` selects `Selected actor`; other selectors remain default. |
| `sa_scope-option-1` | Only `sa_scope` selects `Whole cast`; other selectors remain default. |
| `sa_scope-option-2` | Only `sa_scope` selects `Everyone in view`; other selectors remain default. |
| `sa_actor_target-option-0` | Only `sa_actor_target` selects `Selected cast`; other selectors remain default. |
| `sa_actor_target-option-1` | Only `sa_actor_target` selects `Mara`; other selectors remain default. |
| `sa_actor_target-option-2` | Only `sa_actor_target` selects `Jun`; other selectors remain default. |
| `sa_actor_target-option-3` | Only `sa_actor_target` selects `Me`; other selectors remain default. |
| `sa_override_list-empty` | `sa_override_list` is empty with no selection. |
| `sa_override_list-one` | `sa_override_list` has one selected item. |
| `sa_override_list-overflow` | `sa_override_list` has 51 items and a visible scrollbar. |
| `sa_actor_override-option-0` | Only `sa_actor_override` selects `Inherit`; other selectors remain default. |
| `sa_actor_override-option-1` | Only `sa_actor_override` selects `Never stepped`; other selectors remain default. |
| `sa_actor_override-option-2` | Only `sa_actor_override` selects `Own settings`; other selectors remain default. |
| `sa_actor_steps-option-0` | Only `sa_actor_steps` selects `Inherit`; other selectors remain default. |
| `sa_actor_steps-option-1` | Only `sa_actor_steps` selects `Pose`; other selectors remain default. |
| `sa_actor_steps-option-2` | Only `sa_actor_steps` selects `Pose + position`; other selectors remain default. |
| `sa_actor_role-option-0` | Only `sa_actor_role` selects `Auto`; other selectors remain default. |
| `sa_actor_role-option-1` | Only `sa_actor_role` selects `Target`; other selectors remain default. |
| `sa_actor_role-option-2` | Only `sa_actor_role` selects `Background`; other selectors remain default. |
| `sa_exclude_mounted-on` | Only `sa_exclude_mounted` is on; dependent visibility is measured. |
| `card-rate-tallest-850` | Only `rate` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-rate-tallest-1160` | Only `rate` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-what_steps-tallest-850` | Only `what_steps` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-what_steps-tallest-1160` | Only `what_steps` is put in its tallest local state at 1160 px; all other cards stay default. |
| `sa_face-on` | Only `sa_face` is on; dependent visibility is measured. |
| `sa_gaze-on` | Only `sa_gaze` is on; dependent visibility is measured. |
| `sa_flexi-on` | Only `sa_flexi` is on; dependent visibility is measured. |
| `sa_attach_fx-on` | Only `sa_attach_fx` is on; dependent visibility is measured. |
| `sa_particles-on` | Only `sa_particles` is on; dependent visibility is measured. |
| `sa_texture_anim-on` | Only `sa_texture_anim` is on; dependent visibility is measured. |
| `card-step_timing-tallest-850` | Only `step_timing` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-step_timing-tallest-1160` | Only `step_timing` is put in its tallest local state at 1160 px; all other cards stay default. |
| `sa_clock-option-0` | Only `sa_clock` selects `Capture time`; other selectors remain default. |
| `sa_clock-option-1` | Only `sa_clock` selects `Real time`; other selectors remain default. |
| `sa_offset-option-0` | Only `sa_offset` selects `Random`; other selectors remain default. |
| `sa_offset-option-1` | Only `sa_offset` selects `Locked together`; other selectors remain default. |
| `sa_offset-option-2` | Only `sa_offset` selects `Manual phase`; other selectors remain default. |
| `sa_base_fps-option-0` | Only `sa_base_fps` selects `24`; other selectors remain default. |
| `sa_base_fps-option-1` | Only `sa_base_fps` selects `25`; other selectors remain default. |
| `sa_base_fps-option-2` | Only `sa_base_fps` selects `30`; other selectors remain default. |
| `card-character-tallest-850` | Only `character` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-character-tallest-1160` | Only `character` is put in its tallest local state at 1160 px; all other cards stay default. |
| `sa_boil-on` | Only `sa_boil` is on; dependent visibility is measured. |
| `sa_impact-on` | Only `sa_impact` is on; dependent visibility is measured. |
| `sa_smear-on` | Only `sa_smear` is on; dependent visibility is measured. |
| `sa_ease-on` | Only `sa_ease` is on; dependent visibility is measured. |
| `card-pairs-tallest-850` | Only `pairs` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-pairs-tallest-1160` | Only `pairs` is put in its tallest local state at 1160 px; all other cards stay default. |
| `sa_match_roto-on` | Only `sa_match_roto` is on; dependent visibility is measured. |

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

- `sa_actor_target` (Actor): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `sa_override_list` (Overrides): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `sa_actor_override` (This actor): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `sa_override_clear` (Clear): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `sa_rate_chips` (On): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.

## Risks

- A widget-name collision can select an unrelated XUI root or C++ literal; ownership must be resolved by root, class, and behavior together.
- Extracting model work without rewiring the existing owner creates divergent v1/v2 behavior; the extraction and forwarding call are one reviewed change.
- Duplicate writable settings must obey the rollout homes table; this page emits a link/read-only row when it is not the chosen home.
- Shared edit models require owner tokens, and external presets/cue playback require one global history barrier at start/stop rather than per write.
- Unresolved rows fail generation closed; they are never silently replaced with a guessed widget or command.
