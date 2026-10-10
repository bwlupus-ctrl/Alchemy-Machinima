# Actors — Director Kit page contract

Page key: `actors_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (5/28 setting-bound rows; 22 C++-referenced rows).

## Real owners

- `panel_avatar_light_response` / `panel` at `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:2` (21 matching rows).
- `floater_lightbox_settings` / `floater` at `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:8` (5 matching rows).
- `floater_director` / `floater` at `indra/newview/skins/default/xui/en/floater_director.xml:15` (1 matching rows).
- `HoverHeight` / `floater` at `indra/newview/skins/default/xui/en/floater_edit_hover_height.xml:14` (1 matching rows).
- `linkreplace` / `floater` at `indra/newview/skins/default/xui/en/floater_linkreplace.xml:3` (1 matching rows).
- `panel_cinecam_params` / `panel` at `indra/newview/skins/default/xui/en/panel_cinecam_params.xml:10` (1 matching rows).
- `GroupInfo` / `panel` at `indra/newview/skins/default/xui/en/panel_group_info_sidetray.xml:11` (1 matching rows).
- Referencing classes: `ALPanelAvatarLightResponse` (20), `ALFloaterLightBox` (1), `LLFloaterLinkReplace` (1), `LLPanelGroup` (1), `LLFloaterDirector` (1), `ALPanelCineCamParams` (1), `LLFloaterHoverHeight` (1).

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
| `actor_mask` / `am_reset_CineLightRigActorMaskBias` (Reset bias) | `button` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5368` | `ALActorLightKitAdapter` command/model binding → `ALFloaterLightBox::onClickResetControlDefault()` (`indra/newview/alfloaterlightbox.cpp:76`) | none |
| `actor_mask` / `am_test_combo` (Isolation test) | `combo_box` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:5370` | kit `combo` ↔ `CineLightRigActorMaskTest` `U32` | none |
| `actor_mask` / `open_lightbox_actormask` (Lightbox ›) | none | **USER DECISION:** register an explicit `ALActorLightKitAdapter` command; no matching setting or C++ literal was found | none |
| `avatar_list` / `target_list` (Avatars) | `scroll_list` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:32` | `ALActorLightKitAdapter` command/model binding → `ALPanelAvatarLightResponse::postBuild()` (`indra/newview/alpanelavatarlightresponse.cpp:90`) | none |
| `avatar_list` / `chk_only_adjusted` (Only adjusted) | `check_box` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:55` | `ALActorLightKitAdapter` command/model binding → `ALPanelAvatarLightResponse::postBuild()` (`indra/newview/alpanelavatarlightresponse.cpp:91`) | none |
| `avatar_list` / `btn_refresh` (Refresh) | `button` `indra/newview/skins/default/xui/en/floater_linkreplace.xml:94`, `button` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:20`, `button` `indra/newview/skins/default/xui/en/panel_group_info_sidetray.xml:261` | `ALActorLightKitAdapter` command/model binding → `ALPanelAvatarLightResponse::postBuild()` (`indra/newview/alpanelavatarlightresponse.cpp:120`), `LLFloaterLinkReplace::postBuild()` (`indra/newview/llfloaterlinkreplace.cpp:60`), `LLPanelGroup::postBuild()` (`indra/newview/llpanelgroup.cpp:175`) | none |
| `avatar_list` / `light_response_popout` (Open window) | `button` `indra/newview/skins/default/xui/en/floater_director.xml:1418` | `ALActorLightKitAdapter` command/model binding → `LLFloaterDirector::postBuild()` (`indra/newview/llfloaterdirector.cpp:592`) | none |
| `light_response` / `sld_diffuse` (Diffuse) | `slider` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:70` | `ALActorLightKitAdapter` command/model binding → `ALPanelAvatarLightResponse::postBuild()` (`indra/newview/alpanelavatarlightresponse.cpp:92`) | none |
| `light_response` / `sld_tame` (Reflections) | `slider` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:86` | `ALActorLightKitAdapter` command/model binding → `ALPanelAvatarLightResponse::postBuild()` (`indra/newview/alpanelavatarlightresponse.cpp:93`) | none |
| `light_response` / `sld_brightness` (Brightness) | `slider` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:102` | `ALActorLightKitAdapter` command/model binding → `ALPanelAvatarLightResponse::postBuild()` (`indra/newview/alpanelavatarlightresponse.cpp:94`) | none |
| `light_response` / `sld_glow` (Glow) | `slider` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:128` | `ALActorLightKitAdapter` command/model binding → `ALPanelAvatarLightResponse::postBuild()` (`indra/newview/alpanelavatarlightresponse.cpp:95`) | none |
| `light_response` / `chk_bypass` (Bypass) | `check_box` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:138` | `ALActorLightKitAdapter` command/model binding → `ALPanelAvatarLightResponse::postBuild()` (`indra/newview/alpanelavatarlightresponse.cpp:96`) | none |
| `light_response` / `txt_brightness_ev` (Brightness EV) | `text` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:111` | `ALActorLightKitAdapter` command/model binding → `ALPanelAvatarLightResponse::postBuild()` (`indra/newview/alpanelavatarlightresponse.cpp:109`) | none |
| `silhouette` / `sld_sil_amount` (Amount) | `slider` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:164` | `ALActorLightKitAdapter` command/model binding → `ALPanelAvatarLightResponse::postBuild()` (`indra/newview/alpanelavatarlightresponse.cpp:97`) | none |
| `silhouette` / `combo_sil_fill` (Fill) | `combo_box` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:193` | `ALActorLightKitAdapter` command/model binding → `ALPanelAvatarLightResponse::postBuild()` (`indra/newview/alpanelavatarlightresponse.cpp:99`) | none |
| `silhouette` / `swatch_sil_fill` (Custom colour) | `color_swatch` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:185` | `ALActorLightKitAdapter` command/model binding → `ALPanelAvatarLightResponse::postBuild()` (`indra/newview/alpanelavatarlightresponse.cpp:98`) | none |
| `silhouette` / `chk_sil_haze` (Haze) | `check_box` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:210` | `ALActorLightKitAdapter` command/model binding → `ALPanelAvatarLightResponse::postBuild()` (`indra/newview/alpanelavatarlightresponse.cpp:100`) | none |
| `silhouette` / `chk_sil_keep_rim` (Keep rim) | `check_box` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:220` | `ALActorLightKitAdapter` command/model binding → `ALPanelAvatarLightResponse::postBuild()` (`indra/newview/alpanelavatarlightresponse.cpp:101`) | none |
| `light_presets` / `btn_reset_selected` (Reset) | `button` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:294` | `ALActorLightKitAdapter` command/model binding → `ALPanelAvatarLightResponse::postBuild()` (`indra/newview/alpanelavatarlightresponse.cpp:105`) | none |
| `light_presets` / `btn_inherit_selected` (Inherit) | `button` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:304` | `ALActorLightKitAdapter` command/model binding → `ALPanelAvatarLightResponse::postBuild()` (`indra/newview/alpanelavatarlightresponse.cpp:106`) | none |
| `light_presets` / `btn_copy` (Copy) | `button` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:314` | `ALActorLightKitAdapter` command/model binding → `ALPanelAvatarLightResponse::postBuild()` (`indra/newview/alpanelavatarlightresponse.cpp:107`) | none |
| `light_presets` / `btn_paste` (Paste) | `button` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:323` | `ALActorLightKitAdapter` command/model binding → `ALPanelAvatarLightResponse::postBuild()` (`indra/newview/alpanelavatarlightresponse.cpp:108`) | none |
| `light_presets` / `btn_reset_all` (Reset all...) | `button` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:332`, `button` `indra/newview/skins/default/xui/en/panel_cinecam_params.xml:220` | `ALActorLightKitAdapter` command/model binding → `ALPanelAvatarLightResponse::postBuild()` (`indra/newview/alpanelavatarlightresponse.cpp:155`), `ALPanelCineCamParams::postBuild()` (`indra/newview/alpanelcinecamparams.cpp:404`) | none |
| `actors_diagnostics` / `chk_debug_tint` (Debug tint) | `check_box` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:343` | kit `toggle` ↔ `AvatarLightResponseDebugTint` `Boolean` | none |
| `actors_diagnostics` / `txt_status` (Status) | `text` `indra/newview/skins/default/xui/en/floater_edit_hover_height.xml:127`, `text` `indra/newview/skins/default/xui/en/panel_avatar_light_response.xml:352` | `ALActorLightKitAdapter` command/model binding → `ALPanelAvatarLightResponse::postBuild()` (`indra/newview/alpanelavatarlightresponse.cpp:110`), `LLFloaterHoverHeight::postBuild()` (`indra/newview/llfloaterhoverheight.cpp:109`) | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALFloaterLightBox::onClickResetControlDefault()` — `indra/newview/alfloaterlightbox.cpp:76` | Move model-facing work into `ALActorLightKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelAvatarLightResponse::postBuild()` — `indra/newview/alpanelavatarlightresponse.cpp:90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 105, 106, 107, 108, 109, 110, 120, 155` | Move model-facing work into `ALActorLightKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALPanelCineCamParams::postBuild()` — `indra/newview/alpanelcinecamparams.cpp:404` | Move model-facing work into `ALActorLightKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterDirector::postBuild()` — `indra/newview/llfloaterdirector.cpp:592` | Move model-facing work into `ALActorLightKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterHoverHeight::postBuild()` — `indra/newview/llfloaterhoverheight.cpp:109` | Move model-facing work into `ALActorLightKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterLinkReplace::postBuild()` — `indra/newview/llfloaterlinkreplace.cpp:60` | Move model-facing work into `ALActorLightKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLPanelGroup::postBuild()` — `indra/newview/llpanelgroup.cpp:175` | Move model-facing work into `ALActorLightKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

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

## New widget required

None. Every approved visual kind is either a kit primitive or an existing XUI control. Missing action/model bindings below are USER DECISIONS, not permission to invent a visual class.

## Unresolved and USER DECISION items

- `open_lightbox_actormask` (Lightbox ›): widget not found in XUI; no C++ reference or setting binding. **USER DECISION required.**

## Risks

- A widget-name collision can select an unrelated XUI root or C++ literal; ownership must be resolved by root, class, and behavior together.
- Extracting model work without rewiring the existing owner creates divergent v1/v2 behavior; the extraction and forwarding call are one reviewed change.
- Duplicate writable settings must obey the rollout homes table; this page emits a link/read-only row when it is not the chosen home.
- Shared edit models require owner tokens, and external presets/cue playback require one global history barrier at start/stop rather than per write.
- Unresolved rows fail generation closed; they are never silently replaced with a guessed widget or command.
