# Stepped — Director Kit page contract

Page key: `stepped_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (46/70 setting-bound rows; 27 C++-referenced rows).

## Real owners

- `stepped_tab_logic` / `panel` at `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:3` (66 matching rows).
- Referencing classes: `ALPanelSteppedAnim` (31).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALSteppedAnimKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `stepped` / `sa_scope` (Applies to) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:12` | kit `combo` ↔ `StepAnimScope` `S32` | none |
| `stepped` / `sa_nearby_radius` (Radius) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:23` | kit `slider` ↔ `StepAnimNearbyRadius` `F32`; `ALSteppedAnimKitAdapter` preserves/calls behavior `ALPanelSteppedAnim::bindSteppedPage()` (`indra/newview/alpanelsteppedanim.cpp:92`) | none |
| `stepped` / `sa_limit_distance` (Limit distance) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:28` | kit `toggle` ↔ `StepAnimLimitDistance` `Boolean` | none |
| `stepped` / `sa_include_self` (Include me) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:31` | kit `toggle` ↔ `StepAnimIncludeSelf` `Boolean` | none |
| `stepped` / `sa_include_ghosts` (Ghost clones) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:34` | kit `toggle` ↔ `StepAnimIncludeGhosts` `Boolean` | none |
| `stepped` / `sa_actor_target` (Actor) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:37` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `stepped` / `sa_override_list` (Overrides) | `scroll_list` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:47` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `stepped` / `sa_actor_override` (This actor) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:50` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `stepped` / `sa_actor_offset` (Rate offset) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:59` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `stepped` / `sa_actor_part` (Part) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:69` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `stepped` / `sa_actor_part_rate` (Part rate) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:82` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `stepped` / `sa_actor_effective` (Effective) | `text` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:93` | kit readout ← approved static copy; no runtime writer | none |
| `stepped` / `sa_actor_rate` (Own rate) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:96` | `ALSteppedAnimKitAdapter` command/model binding → behavior `ALPanelSteppedAnim::bindSteppedPage()` (`indra/newview/alpanelsteppedanim.cpp:93`) | none |
| `stepped` / `sa_actor_steps` (Own steps) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:101` | `ALSteppedAnimKitAdapter` command/model binding → behavior `ALPanelSteppedAnim::bindSteppedPage()` (`indra/newview/alpanelsteppedanim.cpp:94`) | none |
| `stepped` / `sa_actor_role` (Mixed role) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:110` | `ALSteppedAnimKitAdapter` command/model binding → behavior `ALPanelSteppedAnim::bindSteppedPage()` (`indra/newview/alpanelsteppedanim.cpp:90`) | none |
| `stepped` / `sa_override_clear` (Clear) | `button` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:119` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `stepped` / `sa_exclude_mounted` (Skip camera rigs) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:122` | kit `toggle` ↔ `StepAnimExcludeCameraMounted` `Boolean` | none |
| `stepped` / `sa_max_avatars` (Max actors) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:125` | kit `number` ↔ `StepAnimMaxAvatars` `S32` | none |
| `stepped` / `sa_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:128` | kit readout ← approved static copy; no runtime writer | none |
| `rate` / `sa_mode` (Mode) | `radio_group` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:133` | kit `segmented` ↔ `StepAnimMode` `S32` | none |
| `rate` / `sa_rate_chips` (On) | `radio_group` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:140` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `rate` / `sa_rate` (Rate) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:148` | kit `slider` ↔ `StepAnimRate` `F32` | none |
| `rate` / `sa_rate_readout` (Reads as) | `text` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:153` | kit readout ← approved static copy; no runtime writer | none |
| `rate` / `sa_fast_rate` (Fast rate) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:156` | kit `slider` ↔ `StepAnimFastRate` `F32`; `ALSteppedAnimKitAdapter` preserves/calls behavior `ALPanelSteppedAnim::bindSteppedPage()` (`indra/newview/alpanelsteppedanim.cpp:84`) | none |
| `rate` / `sa_adapt_threshold` (Threshold) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:161` | kit `slider` ↔ `StepAnimAdaptThreshold` `F32`; `ALSteppedAnimKitAdapter` preserves/calls behavior `ALPanelSteppedAnim::bindSteppedPage()` (`indra/newview/alpanelsteppedanim.cpp:85`) | none |
| `rate` / `sa_adapt_hysteresis` (Hysteresis) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:166` | kit `slider` ↔ `StepAnimAdaptHysteresis` `F32`; `ALSteppedAnimKitAdapter` preserves/calls behavior `ALPanelSteppedAnim::bindSteppedPage()` (`indra/newview/alpanelsteppedanim.cpp:86`) | none |
| `rate` / `sa_adapt_dwell` (Min hold) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:171` | kit `slider` ↔ `StepAnimAdaptMinDwell` `F32`; `ALSteppedAnimKitAdapter` preserves/calls behavior `ALPanelSteppedAnim::bindSteppedPage()` (`indra/newview/alpanelsteppedanim.cpp:87`) | none |
| `rate` / `sa_speed_meter` (Limb speed) | `text` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:176` | `ALSteppedAnimKitAdapter` command/model binding → behavior `ALPanelSteppedAnim::bindSteppedPage()` (`indra/newview/alpanelsteppedanim.cpp:88`) | none |
| `rate` / `sa_background_rate` (Background) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:179` | kit `slider` ↔ `StepAnimBackgroundRate` `F32`; `ALSteppedAnimKitAdapter` preserves/calls behavior `ALPanelSteppedAnim::bindSteppedPage()` (`indra/newview/alpanelsteppedanim.cpp:89`) | none |
| `what_steps` / `sa_what` (Steps) | `radio_group` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:186` | kit `segmented` ↔ `StepAnimWhatSteps` `S32` | none |
| `what_steps` / `sa_gaze` (Gaze & look-at) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:192` | kit `toggle` ↔ `StepAnimGaze` `Boolean` | none |
| `what_steps` / `sa_flexi` (Hair, cloth & flexi) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:195` | kit `toggle` ↔ `StepAnimFlexi` `Boolean` | none |
| `what_steps` / `sa_attach_fx` (Attachment FX) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:198` | kit `toggle` ↔ `StepAnimAttachFX` `Boolean` | none |
| `what_steps` / `sa_particles` (Step particles) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:201` | kit `toggle` ↔ `StepAnimParticles` `Boolean` | none |
| `what_steps` / `sa_texture_anim` (Step textures) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:204` | kit `toggle` ↔ `StepAnimTextureAnim` `Boolean` | none |
| `step_timing` / `sa_clock` (Clock) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:209` | kit `combo` ↔ `StepAnimClock` `S32` | none |
| `step_timing` / `sa_offset` (Actor offset) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:217` | kit `combo` ↔ `StepAnimOffsetMode` `S32` | none |
| `step_timing` / `sa_phase` (Phase) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:226` | kit `slider` ↔ `StepAnimPhase` `F32`; `ALSteppedAnimKitAdapter` preserves/calls behavior `ALPanelSteppedAnim::bindSteppedPage()` (`indra/newview/alpanelsteppedanim.cpp:91`) | none |
| `step_timing` / `sa_base_fps` (Base fps) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:231` | kit `combo` ↔ `StepAnimBaseFPS` `S32` | none |
| `variation` / `sa_seed` (Seed) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:242` | kit `number` ↔ `StepAnimSeed` `U32` | none |
| `variation` / `sa_seed_reroll` (New seed) | `button` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:245` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `variation` / `sa_phase_spread` (Phase spread) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:248` | kit `slider` ↔ `StepAnimPhaseSpread` `F32` | none |
| `variation` / `sa_rate_variation` (Rate variation) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:253` | kit `slider` ↔ `StepAnimRateVariation` `F32` | none |
| `variation` / `sa_timing_jitter` (Imperfect timing) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:258` | kit `slider` ↔ `StepAnimTimingJitter` `F32` | none |
| `group_rates` / `sa_group_pick` (Group) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:265` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `group_rates` / `sa_group_offset` (Group rate) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:271` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `group_rates` / `sa_group_list` (All groups) | `scroll_list` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:281` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `character` / `sa_boil` (Pose boil) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:286` | kit `toggle` ↔ `StepAnimBoil` `Boolean` | none |
| `character` / `sa_boil_amount` (Amount) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:289` | kit `slider` ↔ `StepAnimBoilAmount` `F32` | none |
| `character` / `sa_boil_ticks` (Boil every) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:294` | kit `number` ↔ `StepAnimBoilTicks` `S32` | none |
| `character` / `sa_impact` (Hold on impact) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:297` | kit `toggle` ↔ `StepAnimImpactHold` `Boolean` | none |
| `character` / `sa_impact_ticks` (Frames) | `spinner` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:300` | kit `number` ↔ `StepAnimImpactTicks` `S32` | none |
| `character` / `sa_impact_threshold` (Threshold) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:303` | kit `slider` ↔ `StepAnimImpactThreshold` `F32` | none |
| `character` / `sa_smear` (Smear frames) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:308` | kit `toggle` ↔ `StepAnimSmear` `Boolean` | none |
| `character` / `sa_smear_strength` (Strength) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:311` | kit `slider` ↔ `StepAnimSmearStrength` `F32` | none |
| `character` / `sa_smear_threshold` (Smear speed) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:316` | kit `slider` ↔ `StepAnimSmearThreshold` `F32` | none |
| `character` / `sa_ease` (Ease spacing) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:321` | kit `toggle` ↔ `StepAnimEase` `Boolean` | none |
| `character` / `sa_ease_amount` (Amount) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:324` | kit `slider` ↔ `StepAnimEaseAmount` `F32` | none |
| `pairs` / `sa_match_roto` (Match Roto Ink) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:331` | kit `toggle` ↔ `StepAnimMatchRotoInk` `Boolean` | none |
| `pairs` / `sa_roto_link` (Roto Ink) | none | kit-native action `link_to:look_group/stylize_tab`; no duplicate widget is required | none |
| `pairs` / `sa_tron_link` (Tron) | none | kit-native action `link_to:look_group/tron_tab`; no duplicate widget is required | none |
| `pairs` / `sa_actorfx_link` (Actor FX) | none | kit-native action `link_to:look_group/actor_style_tab`; no duplicate widget is required | none |
| `pairs` / `sa_time_link` (World clock) | none | kit-native action `link_to:capture_group/temporal_tab`; no duplicate widget is required | none |
| `part_rates` / `sa_part_head` (Head) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:340` | kit `combo` ↔ `StepAnimPartHead` `S32` | none |
| `part_rates` / `sa_part_face` (Face) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:350` | **UNRESOLVED:** UNRESOLVED: no scoped owner callback, setting, model binding, or kit action | none |
| `part_rates` / `sa_part_arms` (Arms) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:360` | kit `combo` ↔ `StepAnimPartArms` `S32` | none |
| `part_rates` / `sa_part_hands` (Hands) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:370` | kit `combo` ↔ `StepAnimPartHands` `S32` | none |
| `part_rates` / `sa_part_legs` (Legs) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:380` | kit `combo` ↔ `StepAnimPartLegs` `S32` | none |
| `part_rates` / `sa_part_extras` (Extras) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:390` | kit `combo` ↔ `StepAnimPartExtras` `S32` | none |
| `part_rates` / `sa_part_attach` (Attachments) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_look_stepped.xml:400` | kit `combo` ↔ `StepAnimPartAttach` `S32` | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALPanelSteppedAnim::bindSteppedPage()` — `indra/newview/alpanelsteppedanim.cpp:84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94` | Move model-facing work into `ALSteppedAnimKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **117 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

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
| `sa_scope-option-3` | Only `sa_scope` selects `Friends`; other selectors remain default. |
| `sa_scope-option-4` | Only `sa_scope` selects `Nearby`; other selectors remain default. |
| `sa_limit_distance-on` | Only `sa_limit_distance` is on; dependent visibility is measured. |
| `sa_include_self-on` | Only `sa_include_self` is on; dependent visibility is measured. |
| `sa_include_ghosts-on` | Only `sa_include_ghosts` is on; dependent visibility is measured. |
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
| `sa_actor_offset-option-0` | Only `sa_actor_offset` selects `Inherit`; other selectors remain default. |
| `sa_actor_offset-option-1` | Only `sa_actor_offset` selects `Same`; other selectors remain default. |
| `sa_actor_offset-option-2` | Only `sa_actor_offset` selects `+1 step`; other selectors remain default. |
| `sa_actor_offset-option-3` | Only `sa_actor_offset` selects `+2 steps`; other selectors remain default. |
| `sa_actor_part-option-0` | Only `sa_actor_part` selects `Head`; other selectors remain default. |
| `sa_actor_part-option-1` | Only `sa_actor_part` selects `Face`; other selectors remain default. |
| `sa_actor_part-option-2` | Only `sa_actor_part` selects `Arms`; other selectors remain default. |
| `sa_actor_part-option-3` | Only `sa_actor_part` selects `Hands`; other selectors remain default. |
| `sa_actor_part-option-4` | Only `sa_actor_part` selects `Legs`; other selectors remain default. |
| `sa_actor_part-option-5` | Only `sa_actor_part` selects `Extras`; other selectors remain default. |
| `sa_actor_part-option-6` | Only `sa_actor_part` selects `Attachments`; other selectors remain default. |
| `sa_actor_part_rate-option-0` | Only `sa_actor_part_rate` selects `Inherit`; other selectors remain default. |
| `sa_actor_part_rate-option-1` | Only `sa_actor_part_rate` selects `Same as torso`; other selectors remain default. |
| `sa_actor_part_rate-option-2` | Only `sa_actor_part_rate` selects `+1 step`; other selectors remain default. |
| `sa_actor_part_rate-option-3` | Only `sa_actor_part_rate` selects `+2 steps`; other selectors remain default. |
| `sa_actor_part_rate-option-4` | Only `sa_actor_part_rate` selects `Smooth`; other selectors remain default. |
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
| `card-variation-tallest-850` | Only `variation` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-variation-tallest-1160` | Only `variation` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-group_rates-tallest-850` | Only `group_rates` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-group_rates-tallest-1160` | Only `group_rates` is put in its tallest local state at 1160 px; all other cards stay default. |
| `sa_group_pick-option-0` | Only `sa_group_pick` selects `Hunters`; other selectors remain default. |
| `sa_group_pick-option-1` | Only `sa_group_pick` selects `Demons`; other selectors remain default. |
| `sa_group_offset-option-0` | Only `sa_group_offset` selects `Same`; other selectors remain default. |
| `sa_group_offset-option-1` | Only `sa_group_offset` selects `+1 step`; other selectors remain default. |
| `sa_group_offset-option-2` | Only `sa_group_offset` selects `+2 steps`; other selectors remain default. |
| `sa_group_offset-option-3` | Only `sa_group_offset` selects `Smooth`; other selectors remain default. |
| `sa_group_list-empty` | `sa_group_list` is empty with no selection. |
| `sa_group_list-one` | `sa_group_list` has one selected item. |
| `sa_group_list-overflow` | `sa_group_list` has 51 items and a visible scrollbar. |
| `card-character-tallest-850` | Only `character` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-character-tallest-1160` | Only `character` is put in its tallest local state at 1160 px; all other cards stay default. |
| `sa_boil-on` | Only `sa_boil` is on; dependent visibility is measured. |
| `sa_impact-on` | Only `sa_impact` is on; dependent visibility is measured. |
| `sa_smear-on` | Only `sa_smear` is on; dependent visibility is measured. |
| `sa_ease-on` | Only `sa_ease` is on; dependent visibility is measured. |
| `card-pairs-tallest-850` | Only `pairs` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-pairs-tallest-1160` | Only `pairs` is put in its tallest local state at 1160 px; all other cards stay default. |
| `sa_match_roto-on` | Only `sa_match_roto` is on; dependent visibility is measured. |
| `card-part_rates-tallest-850` | Only `part_rates` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-part_rates-tallest-1160` | Only `part_rates` is put in its tallest local state at 1160 px; all other cards stay default. |
| `sa_part_head-option-0` | Only `sa_part_head` selects `Same as torso`; other selectors remain default. |
| `sa_part_head-option-1` | Only `sa_part_head` selects `+1 step`; other selectors remain default. |
| `sa_part_head-option-2` | Only `sa_part_head` selects `+2 steps`; other selectors remain default. |
| `sa_part_head-option-3` | Only `sa_part_head` selects `Smooth`; other selectors remain default. |
| `sa_part_face-option-0` | Only `sa_part_face` selects `Same as torso`; other selectors remain default. |
| `sa_part_face-option-1` | Only `sa_part_face` selects `+1 step`; other selectors remain default. |
| `sa_part_face-option-2` | Only `sa_part_face` selects `+2 steps`; other selectors remain default. |
| `sa_part_face-option-3` | Only `sa_part_face` selects `Smooth`; other selectors remain default. |
| `sa_part_arms-option-0` | Only `sa_part_arms` selects `Same as torso`; other selectors remain default. |
| `sa_part_arms-option-1` | Only `sa_part_arms` selects `+1 step`; other selectors remain default. |
| `sa_part_arms-option-2` | Only `sa_part_arms` selects `+2 steps`; other selectors remain default. |
| `sa_part_arms-option-3` | Only `sa_part_arms` selects `Smooth`; other selectors remain default. |
| `sa_part_hands-option-0` | Only `sa_part_hands` selects `Same as torso`; other selectors remain default. |
| `sa_part_hands-option-1` | Only `sa_part_hands` selects `+1 step`; other selectors remain default. |
| `sa_part_hands-option-2` | Only `sa_part_hands` selects `+2 steps`; other selectors remain default. |
| `sa_part_hands-option-3` | Only `sa_part_hands` selects `Smooth`; other selectors remain default. |
| `sa_part_legs-option-0` | Only `sa_part_legs` selects `Same as torso`; other selectors remain default. |
| `sa_part_legs-option-1` | Only `sa_part_legs` selects `+1 step`; other selectors remain default. |
| `sa_part_legs-option-2` | Only `sa_part_legs` selects `+2 steps`; other selectors remain default. |
| `sa_part_legs-option-3` | Only `sa_part_legs` selects `Smooth`; other selectors remain default. |
| `sa_part_extras-option-0` | Only `sa_part_extras` selects `Same as torso`; other selectors remain default. |
| `sa_part_extras-option-1` | Only `sa_part_extras` selects `+1 step`; other selectors remain default. |
| `sa_part_extras-option-2` | Only `sa_part_extras` selects `+2 steps`; other selectors remain default. |
| `sa_part_extras-option-3` | Only `sa_part_extras` selects `Smooth`; other selectors remain default. |
| `sa_part_attach-option-0` | Only `sa_part_attach` selects `Same as torso`; other selectors remain default. |
| `sa_part_attach-option-1` | Only `sa_part_attach` selects `+1 step`; other selectors remain default. |
| `sa_part_attach-option-2` | Only `sa_part_attach` selects `+2 steps`; other selectors remain default. |
| `sa_part_attach-option-3` | Only `sa_part_attach` selects `Smooth`; other selectors remain default. |

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
- `sa_actor_offset` (Rate offset): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `sa_actor_part` (Part): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `sa_actor_part_rate` (Part rate): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `sa_override_clear` (Clear): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `sa_rate_chips` (On): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `sa_seed_reroll` (New seed): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `sa_group_pick` (Group): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `sa_group_offset` (Group rate): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `sa_group_list` (All groups): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.
- `sa_part_face` (Face): UNRESOLVED: no scoped owner callback, setting, model binding, or kit action.

## Risks

- A widget-name collision can select an unrelated XUI root or C++ literal; ownership must be resolved by root, class, and behavior together.
- Extracting model work without rewiring the existing owner creates divergent v1/v2 behavior; the extraction and forwarding call are one reviewed change.
- Duplicate writable settings must obey the rollout homes table; this page emits a link/read-only row when it is not the chosen home.
- Shared edit models require owner tokens, and external presets/cue playback require one global history barrier at start/stop rather than per write.
- Unresolved rows fail generation closed; they are never silently replaced with a guessed widget or command.
