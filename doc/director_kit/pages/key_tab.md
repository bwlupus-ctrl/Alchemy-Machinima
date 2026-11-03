# Key — Director Kit page contract

Page key: `key_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (23/33 setting-bound rows; 21 C++-referenced rows).

## Real owners

- `key_tab_logic` / `panel` at `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:3` (33 matching rows).
- Referencing classes: `ALPanelActorKey` (24), `LLReShadeBridge` (1).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALPanelActorKey` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `ak_chroma` / `ak_enable` (Key actors) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:13` | kit `toggle` ↔ `ActorKeyEnabled` `Boolean` | none |
| `ak_chroma` / `ak_status` (Status) | `text` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:16` | kit readout ← approved static copy; no runtime writer | none |
| `ak_chroma` / `ak_reshade` (ReShade) | `text` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:19` | kit readout ← approved static copy; no runtime writer | none |
| `ak_chroma` / `ak_guarantee` (Guarantee) | `text` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:22` | kit readout ← approved static copy; no runtime writer | none |
| `ak_chroma` / `ak_retry` (Retry) | `button` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:25` | `ALPanelActorKey` command/model binding → wired in `ALPanelActorKey::postBuild()`; behavior `LLReShadeBridge::retryKeyGuard()` (`indra/newview/alpanelactorkey.cpp:158`) | none |
| `ak_chroma` / `ak_mode` (Mode) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:28` | kit `combo` ↔ `ActorKeyMode` `U32` | none |
| `ak_chroma` / `ak_preset` (Key colour) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:37` | kit `combo` ↔ `ActorKeyColorPreset` `U32` | none |
| `ak_chroma` / `ak_color` (Custom colour) | `color_swatch` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:52` | kit `color` ↔ `ActorKeyColor` `Color3` | none |
| `ak_chroma` / `ak_codes` (Codes) | `text` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:55` | kit readout ← approved static copy; no runtime writer | none |
| `ak_chroma` / `ak_invert` (Invert matte) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:58` | kit `toggle` ↔ `ActorKeyMatteInvert` `Boolean` | none |
| `ak_subjects` / `ak_who` (Who) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:63` | kit `combo` ↔ `ActorKeySubjects` `U32` | none |
| `ak_subjects` / `ak_clones` (Include clones) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:73` | kit `toggle` ↔ `ActorKeyIncludeClones` `Boolean` | none |
| `ak_subjects` / `ak_glass` (Glass refracts) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:76` | kit `combo` ↔ `ActorKeyGlassSource` `U32` | none |
| `ak_subjects` / `ak_animesh` (Include animesh) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:84` | kit `toggle` ↔ `ActorKeyIncludeAnimesh` `Boolean` | none |
| `ak_subjects` / `ak_count` (Keyed) | `text` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:87` | kit readout ← approved static copy; no runtime writer | none |
| `ak_edges` / `ak_feather` (Feather) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:92` | kit `slider` ↔ `ActorKeyFeather` `F32` | none |
| `ak_edges` / `ak_choke` (Choke) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:97` | kit `slider` ↔ `ActorKeyChoke` `F32` | none |
| `ak_edges` / `ak_edge` (Edge fill) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:102` | kit `combo` ↔ `ActorKeyEdgeMode` `U32` | none |
| `ak_edges` / `ak_alpha` (Hair and alpha coverage) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:110` | kit `toggle` ↔ `ActorKeyAlphaCoverage` `Boolean` | none |
| `ak_shadows` / `ak_shadow` (Keep actor shadows) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:115` | kit `toggle` ↔ `ActorKeyShadow` `Boolean` | none |
| `ak_shadows` / `ak_iso` (Isolation) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:118` | kit `combo` ↔ `ActorKeyShadowIsolation` `U32` | none |
| `ak_shadows` / `ak_sstr` (Strength) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:127` | kit `slider` ↔ `ActorKeyShadowStrength` `F32` | none |
| `ak_shadows` / `ak_scasc` (Cascades) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:132` | kit `combo` ↔ `ActorKeyShadowCascades` `U32` | none |
| `ak_shadows` / `ak_ssoft` (Softness) | `slider` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:142` | kit `slider` ↔ `ActorKeyShadowSoftness` `F32` | none |
| `ak_shadows` / `ak_sstatus` (Status) | `text` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:147` | kit readout ← approved static copy; no runtime writer | none |
| `ak_shadows` / `ak_mshadow` (Grey shadow in matte) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:150` | kit `toggle` ↔ `ActorKeyMatteShadow` `Boolean` | none |
| `ak_output` / `ak_hud` (Hide HUD attachments) | `kit_switch` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:155` | kit `toggle` ↔ `ActorKeyHideHUD` `Boolean` | none |
| `ak_output` / `ak_warn` (Capture warnings) | `text` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:158` | kit readout ← approved static copy; no runtime writer | none |
| `ak_output` / `ak_limits` (Known limits) | `text` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:161` | kit readout ← approved static copy; no runtime writer | none |
| `ak_diagnostics` / `ak_debug` (Debug view) | `combo_box` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:166` | kit `combo` ↔ `ActorKeyDebugView` `U32` | none |
| `ak_diagnostics` / `ak_selftest` (Self-test) | `button` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:177` | kit `button` ↔ `ActorKeySelfTest` `U32`; `ALPanelActorKey` preserves/calls wired in `ALPanelActorKey::postBuild()`; behavior `ALPanelActorKey::setU32()` (`indra/newview/alpanelactorkey.cpp:164`) | none |
| `ak_diagnostics` / `ak_shadowtest` (Exact shadow test) | `button` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:180` | kit `button` ↔ `ActorKeyShadowSelfTest` `U32`; `ALPanelActorKey` preserves/calls wired in `ALPanelActorKey::postBuild()`; behavior `ALPanelActorKey::setU32()` (`indra/newview/alpanelactorkey.cpp:166`) | none |
| `ak_diagnostics` / `ak_result` (Last result) | `text` `indra/newview/skins/default/xui/en/panel_dpage_capture_key.xml:183` | kit readout ← approved static copy; no runtime writer | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALPanelActorKey::setU32()` — `indra/newview/alpanelactorkey.cpp:164, 166` | Move model-facing work into `ALPanelActorKey` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLReShadeBridge::retryKeyGuard()` — `indra/newview/alpanelactorkey.cpp:158` | Move model-facing work into `ALPanelActorKey` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **55 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-ak_chroma-tallest-850` | Only `ak_chroma` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-ak_chroma-tallest-1160` | Only `ak_chroma` is put in its tallest local state at 1160 px; all other cards stay default. |
| `ak_enable-on` | Only `ak_enable` is on; dependent visibility is measured. |
| `ak_mode-option-0` | Only `ak_mode` selects `Actors on key`; other selectors remain default. |
| `ak_mode-option-1` | Only `ak_mode` selects `Set only`; other selectors remain default. |
| `ak_mode-option-2` | Only `ak_mode` selects `Alpha matte`; other selectors remain default. |
| `ak_preset-option-0` | Only `ak_preset` selects `Chroma Green #00B140`; other selectors remain default. |
| `ak_preset-option-1` | Only `ak_preset` selects `Pure Green #00FF00`; other selectors remain default. |
| `ak_preset-option-2` | Only `ak_preset` selects `Chroma Blue #0047BB`; other selectors remain default. |
| `ak_preset-option-3` | Only `ak_preset` selects `Pure Blue #0000FF`; other selectors remain default. |
| `ak_preset-option-4` | Only `ak_preset` selects `Magenta #FF00FF`; other selectors remain default. |
| `ak_preset-option-5` | Only `ak_preset` selects `Pure Black #000000`; other selectors remain default. |
| `ak_preset-option-6` | Only `ak_preset` selects `Pure White #FFFFFF`; other selectors remain default. |
| `ak_preset-option-7` | Only `ak_preset` selects `Mid Grey #808080`; other selectors remain default. |
| `ak_preset-option-8` | Only `ak_preset` selects `Custom`; other selectors remain default. |
| `ak_invert-on` | Only `ak_invert` is on; dependent visibility is measured. |
| `card-ak_subjects-tallest-850` | Only `ak_subjects` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-ak_subjects-tallest-1160` | Only `ak_subjects` is put in its tallest local state at 1160 px; all other cards stay default. |
| `ak_who-option-0` | Only `ak_who` selects `All avatars`; other selectors remain default. |
| `ak_who-option-1` | Only `ak_who` selects `Director cast`; other selectors remain default. |
| `ak_who-option-2` | Only `ak_who` selects `Actor-only lit`; other selectors remain default. |
| `ak_who-option-3` | Only `ak_who` selects `Me only`; other selectors remain default. |
| `ak_clones-on` | Only `ak_clones` is on; dependent visibility is measured. |
| `ak_glass-option-0` | Only `ak_glass` selects `Scene`; other selectors remain default. |
| `ak_glass-option-1` | Only `ak_glass` selects `Key colour`; other selectors remain default. |
| `ak_animesh-on` | Only `ak_animesh` is on; dependent visibility is measured. |
| `card-ak_edges-tallest-850` | Only `ak_edges` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-ak_edges-tallest-1160` | Only `ak_edges` is put in its tallest local state at 1160 px; all other cards stay default. |
| `ak_edge-option-0` | Only `ak_edge` selects `Blend`; other selectors remain default. |
| `ak_edge-option-1` | Only `ak_edge` selects `Hard`; other selectors remain default. |
| `ak_alpha-on` | Only `ak_alpha` is on; dependent visibility is measured. |
| `card-ak_shadows-tallest-850` | Only `ak_shadows` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-ak_shadows-tallest-1160` | Only `ak_shadows` is put in its tallest local state at 1160 px; all other cards stay default. |
| `ak_shadow-on` | Only `ak_shadow` is on; dependent visibility is measured. |
| `ak_iso-option-0` | Only `ak_iso` selects `Fast`; other selectors remain default. |
| `ak_iso-option-1` | Only `ak_iso` selects `Exact`; other selectors remain default. |
| `ak_iso-option-2` | Only `ak_iso` selects `All sun shadows`; other selectors remain default. |
| `ak_scasc-option-0` | Only `ak_scasc` selects `1`; other selectors remain default. |
| `ak_scasc-option-1` | Only `ak_scasc` selects `2`; other selectors remain default. |
| `ak_scasc-option-2` | Only `ak_scasc` selects `3`; other selectors remain default. |
| `ak_scasc-option-3` | Only `ak_scasc` selects `4`; other selectors remain default. |
| `ak_mshadow-on` | Only `ak_mshadow` is on; dependent visibility is measured. |
| `card-ak_output-tallest-850` | Only `ak_output` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-ak_output-tallest-1160` | Only `ak_output` is put in its tallest local state at 1160 px; all other cards stay default. |
| `ak_hud-on` | Only `ak_hud` is on; dependent visibility is measured. |
| `card-ak_diagnostics-tallest-850` | Only `ak_diagnostics` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-ak_diagnostics-tallest-1160` | Only `ak_diagnostics` is put in its tallest local state at 1160 px; all other cards stay default. |
| `ak_debug-option-0` | Only `ak_debug` selects `Off`; other selectors remain default. |
| `ak_debug-option-1` | Only `ak_debug` selects `Coverage`; other selectors remain default. |
| `ak_debug-option-2` | Only `ak_debug` selects `Matte`; other selectors remain default. |
| `ak_debug-option-3` | Only `ak_debug` selects `Actor shadow`; other selectors remain default. |
| `ak_debug-option-4` | Only `ak_debug` selects `Lens coverage`; other selectors remain default. |

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
