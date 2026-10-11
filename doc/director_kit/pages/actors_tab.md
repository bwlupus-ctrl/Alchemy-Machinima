# Actors — Director Kit page contract

Page key: `actors_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (5/28 setting-bound rows; 22 C++-referenced rows).

## Real owners

- `panel_avatar_light_response` / `panel` at `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:2` (21 matching rows).
- `floater_lightbox_settings` / `floater` at `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:8` (5 matching rows).
- `floater_director` / `floater` at `indra/newview/skins/default/xui/en/floater_director.xml:15` (1 matching rows).
- Referencing classes: `ALPanelAvatarLightResponse` (36), `LLFloaterReg` (2), `ALFloaterLightBox` (1).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALActorLightKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `actor_mask` / `am_disable_check` (Disable) | `check_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5358` | kit `toggle` ↔ `CineLightRigActorMaskDisable` `Boolean` | none |
| `actor_mask` / `am_debug_combo` (Debug view) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5361` | kit `combo` ↔ `CineLightRigActorMaskDebug` `U32` | none |
| `actor_mask` / `am_bias_slider` (Mask bias) | `slider` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5367` | kit `slider` ↔ `CineLightRigActorMaskBias` `F32` | none |
| `actor_mask` / `am_reset_CineLightRigActorMaskBias` (Reset bias) | `button` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5368` | `ALActorLightKitAdapter` command/model binding → wired in `ALFloaterLightBox::ALFloaterLightBox()`; behavior `ALFloaterLightBox::onClickResetControlDefault()` (`indra/newview/alfloaterlightbox.cpp:76`) | none |
| `actor_mask` / `am_test_combo` (Isolation test) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5370` | kit `combo` ↔ `CineLightRigActorMaskTest` `U32` | none |
| `actor_mask` / `open_lightbox_actormask` (Lightbox ›) | none | kit-native action `floater:lightbox/actor_mask_settings`; no duplicate widget is required | none |
| `avatar_list` / `target_list` (Avatars) | `scroll_list` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:32` | `ALActorLightKitAdapter` command/model binding → wired in `ALPanelAvatarLightResponse::postBuild()`; behavior `ALPanelAvatarLightResponse::onSelectionChanged()` (`indra/newview/alpanelavatarlightresponse.cpp:113`) | none |
| `avatar_list` / `chk_only_adjusted` (Only adjusted) | `check_box` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:55` | `ALActorLightKitAdapter` command/model binding → wired in `ALPanelAvatarLightResponse::postBuild()`; behavior `ALPanelAvatarLightResponse::refreshRows()` (`indra/newview/alpanelavatarlightresponse.cpp:115`) | none |
| `avatar_list` / `btn_refresh` (Refresh) | `button` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:20` | `ALActorLightKitAdapter` command/model binding → wired in `ALPanelAvatarLightResponse::postBuild()`; behavior `ALPanelAvatarLightResponse::<inline callback>()` (`indra/newview/alpanelavatarlightresponse.cpp:120`) | none |
| `avatar_list` / `light_response_popout` (Open window) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:1418` | kit-native action `floater:avatar_light_response:selected-actor`; no duplicate widget is required | none |
| `light_response` / `sld_diffuse` (Diffuse) | `slider` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:70` | `ALActorLightKitAdapter` command/model binding → wired in `ALPanelAvatarLightResponse::postBuild()`; behavior `ALPanelAvatarLightResponse::onSliderCommit()` (`indra/newview/alpanelavatarlightresponse.cpp:128`) | none |
| `light_response` / `sld_tame` (Reflections) | `slider` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:86` | `ALActorLightKitAdapter` command/model binding → wired in `ALPanelAvatarLightResponse::postBuild()`; behavior `ALPanelAvatarLightResponse::onSliderCommit()` (`indra/newview/alpanelavatarlightresponse.cpp:129`) | none |
| `light_response` / `sld_brightness` (Brightness) | `slider` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:102` | `ALActorLightKitAdapter` command/model binding → wired in `ALPanelAvatarLightResponse::postBuild()`; behavior `ALPanelAvatarLightResponse::onSliderCommit()` (`indra/newview/alpanelavatarlightresponse.cpp:130`) | none |
| `light_response` / `sld_glow` (Glow) | `slider` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:128` | `ALActorLightKitAdapter` command/model binding → wired in `ALPanelAvatarLightResponse::postBuild()`; behavior `ALPanelAvatarLightResponse::onSliderCommit()` (`indra/newview/alpanelavatarlightresponse.cpp:131`) | none |
| `light_response` / `chk_bypass` (Bypass) | `check_box` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:138` | `ALActorLightKitAdapter` command/model binding → wired in `ALPanelAvatarLightResponse::postBuild()`; behavior `ALPanelAvatarLightResponse::onBypass()` (`indra/newview/alpanelavatarlightresponse.cpp:145`) | none |
| `light_response` / `txt_brightness_ev` (Brightness EV) | `text` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:111` | kit readout ← approved static copy; no runtime writer | none |
| `silhouette` / `sld_sil_amount` (Amount) | `slider` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:164` | `ALActorLightKitAdapter` command/model binding → wired in `ALPanelAvatarLightResponse::postBuild()`; behavior `ALPanelAvatarLightResponse::onSliderCommit()` (`indra/newview/alpanelavatarlightresponse.cpp:132`) | none |
| `silhouette` / `combo_sil_fill` (Fill) | `combo_box` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:193` | `ALActorLightKitAdapter` command/model binding → wired in `ALPanelAvatarLightResponse::postBuild()`; behavior `ALPanelAvatarLightResponse::onSilFillPreset()` (`indra/newview/alpanelavatarlightresponse.cpp:149`) | none |
| `silhouette` / `swatch_sil_fill` (Custom colour) | `color_swatch` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:185` | `ALActorLightKitAdapter` command/model binding → wired in `ALPanelAvatarLightResponse::postBuild()`; behavior `ALPanelAvatarLightResponse::onSilFill()` (`indra/newview/alpanelavatarlightresponse.cpp:146`) | none |
| `silhouette` / `chk_sil_haze` (Haze) | `check_box` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:210` | `ALActorLightKitAdapter` command/model binding → wired in `ALPanelAvatarLightResponse::postBuild()`; behavior `ALPanelAvatarLightResponse::onSilCheckbox()` (`indra/newview/alpanelavatarlightresponse.cpp:150`) | none |
| `silhouette` / `chk_sil_keep_rim` (Keep rim) | `check_box` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:220` | `ALActorLightKitAdapter` command/model binding → wired in `ALPanelAvatarLightResponse::postBuild()`; behavior `ALPanelAvatarLightResponse::onSilCheckbox()` (`indra/newview/alpanelavatarlightresponse.cpp:151`) | none |
| `light_presets` / `btn_reset_selected` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:294` | `ALActorLightKitAdapter` command/model binding → wired in `ALPanelAvatarLightResponse::postBuild()`; behavior `ALPanelAvatarLightResponse::onResetSelected()` (`indra/newview/alpanelavatarlightresponse.cpp:153`) | none |
| `light_presets` / `btn_inherit_selected` (Inherit) | `button` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:304` | `ALActorLightKitAdapter` command/model binding → wired in `ALPanelAvatarLightResponse::postBuild()`; behavior `ALPanelAvatarLightResponse::onInheritSelected()` (`indra/newview/alpanelavatarlightresponse.cpp:154`) | none |
| `light_presets` / `btn_copy` (Copy) | `button` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:314` | `ALActorLightKitAdapter` command/model binding → wired in `ALPanelAvatarLightResponse::postBuild()`; behavior `ALPanelAvatarLightResponse::onCopy()` (`indra/newview/alpanelavatarlightresponse.cpp:156`) | none |
| `light_presets` / `btn_paste` (Paste) | `button` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:323` | `ALActorLightKitAdapter` command/model binding → wired in `ALPanelAvatarLightResponse::postBuild()`; behavior `ALPanelAvatarLightResponse::onPaste()` (`indra/newview/alpanelavatarlightresponse.cpp:157`) | none |
| `light_presets` / `btn_reset_all` (Reset all...) | `button` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:332` | `ALActorLightKitAdapter` command/model binding → wired in `ALPanelAvatarLightResponse::postBuild()`; behavior `ALPanelAvatarLightResponse::onResetAll()` (`indra/newview/alpanelavatarlightresponse.cpp:155`) | none |
| `actors_diagnostics` / `chk_debug_tint` (Debug tint) | `check_box` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:343` | kit `toggle` ↔ `AvatarLightResponseDebugTint` `Boolean` | none |
| `actors_diagnostics` / `txt_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:352` | kit readout ← approved static copy; no runtime writer | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALFloaterLightBox::onClickResetControlDefault()` — `indra/newview/alfloaterlightbox.cpp:76` | Move model-facing work into `ALActorLightKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelAvatarLightResponse::<inline callback>()` — `indra/newview/alpanelavatarlightresponse.cpp:120` | Move model-facing work into `ALActorLightKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelAvatarLightResponse::onBypass()` — `indra/newview/alpanelavatarlightresponse.cpp:145` | Move model-facing work into `ALActorLightKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelAvatarLightResponse::onCopy()` — `indra/newview/alpanelavatarlightresponse.cpp:156` | Move model-facing work into `ALActorLightKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelAvatarLightResponse::onInheritSelected()` — `indra/newview/alpanelavatarlightresponse.cpp:154` | Move model-facing work into `ALActorLightKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelAvatarLightResponse::onPaste()` — `indra/newview/alpanelavatarlightresponse.cpp:157` | Move model-facing work into `ALActorLightKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelAvatarLightResponse::onResetAll()` — `indra/newview/alpanelavatarlightresponse.cpp:155` | Move model-facing work into `ALActorLightKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelAvatarLightResponse::onResetSelected()` — `indra/newview/alpanelavatarlightresponse.cpp:153` | Move model-facing work into `ALActorLightKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelAvatarLightResponse::onSelectionChanged()` — `indra/newview/alpanelavatarlightresponse.cpp:113` | Move model-facing work into `ALActorLightKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelAvatarLightResponse::onSilCheckbox()` — `indra/newview/alpanelavatarlightresponse.cpp:150, 151` | Move model-facing work into `ALActorLightKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelAvatarLightResponse::onSilFill()` — `indra/newview/alpanelavatarlightresponse.cpp:146` | Move model-facing work into `ALActorLightKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelAvatarLightResponse::onSilFillPreset()` — `indra/newview/alpanelavatarlightresponse.cpp:149` | Move model-facing work into `ALActorLightKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelAvatarLightResponse::onSliderCommit()` — `indra/newview/alpanelavatarlightresponse.cpp:128, 129, 130, 131, 132` | Move model-facing work into `ALActorLightKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelAvatarLightResponse::refreshRows()` — `indra/newview/alpanelavatarlightresponse.cpp:115` | Move model-facing work into `ALActorLightKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterReg::showInstance()` — `indra/newview/llfloaterdirector.cpp:613, 1232` | Move model-facing work into `ALActorLightKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **42 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-actor_mask-tallest-850` | Only `actor_mask` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-actor_mask-tallest-1160` | Only `actor_mask` is put in its tallest local state at 1160 px; all other cards stay default. |
| `am_disable_check-on` | Only `am_disable_check` is on; dependent visibility is measured. |
| `am_debug_combo-option-0` | Only `am_debug_combo` selects `Off`; other selectors remain default. |
| `am_debug_combo-option-1` | Only `am_debug_combo` selects `Overlay`; other selectors remain default. |
| `am_debug_combo-option-2` | Only `am_debug_combo` selects `Mask only`; other selectors remain default. |
| `am_test_combo-option-0` | Only `am_test_combo` selects `Idle`; other selectors remain default. |
| `am_test_combo-option-1` | Only `am_test_combo` selects `Run test`; other selectors remain default. |
| `am_test_combo-option-2` | Only `am_test_combo` selects `Cancel`; other selectors remain default. |
| `card-avatar_list-tallest-850` | Only `avatar_list` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-avatar_list-tallest-1160` | Only `avatar_list` is put in its tallest local state at 1160 px; all other cards stay default. |
| `target_list-empty` | `target_list` is empty with no selection. |
| `target_list-one` | `target_list` has one selected item. |
| `target_list-overflow` | `target_list` has 51 items and a visible scrollbar. |
| `chk_only_adjusted-on` | Only `chk_only_adjusted` is on; dependent visibility is measured. |
| `card-light_response-tallest-850` | Only `light_response` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-light_response-tallest-1160` | Only `light_response` is put in its tallest local state at 1160 px; all other cards stay default. |
| `chk_bypass-on` | Only `chk_bypass` is on; dependent visibility is measured. |
| `card-silhouette-tallest-850` | Only `silhouette` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-silhouette-tallest-1160` | Only `silhouette` is put in its tallest local state at 1160 px; all other cards stay default. |
| `combo_sil_fill-option-0` | Only `combo_sil_fill` selects `Black`; other selectors remain default. |
| `combo_sil_fill-option-1` | Only `combo_sil_fill` selects `Charcoal`; other selectors remain default. |
| `combo_sil_fill-option-2` | Only `combo_sil_fill` selects `White`; other selectors remain default. |
| `combo_sil_fill-option-3` | Only `combo_sil_fill` selects `Chroma Green`; other selectors remain default. |
| `combo_sil_fill-option-4` | Only `combo_sil_fill` selects `Chroma Blue`; other selectors remain default. |
| `combo_sil_fill-option-5` | Only `combo_sil_fill` selects `Custom`; other selectors remain default. |
| `chk_sil_haze-on` | Only `chk_sil_haze` is on; dependent visibility is measured. |
| `chk_sil_keep_rim-on` | Only `chk_sil_keep_rim` is on; dependent visibility is measured. |
| `card-light_presets-tallest-850` | Only `light_presets` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-light_presets-tallest-1160` | Only `light_presets` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-actors_diagnostics-tallest-850` | Only `actors_diagnostics` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-actors_diagnostics-tallest-1160` | Only `actors_diagnostics` is put in its tallest local state at 1160 px; all other cards stay default. |
| `chk_debug_tint-on` | Only `chk_debug_tint` is on; dependent visibility is measured. |
| `actor-none` | Rail has no actor; target-dependent rows are disabled with an explicit empty state. |
| `actor-selected` | One rail actor is selected and unpinned. |
| `actor-pinned-rail-changes` | Page target is pinned while the rail changes to another actor. |
| `actor-removed` | The active actor disappears while the page is visible. |
| `permission-denied` | The current target exists but cannot be modified. |
| `target-offline` | The current target/provider becomes unavailable. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## Ambiguous same-name candidates (never auto-bound)

None.

## Rejected out-of-scope same-name candidates

12 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

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
