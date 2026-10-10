# Bloom & Glow — Director Kit page contract

Page key: `bloom_glow_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **pure-setting** (16/17 setting-bound rows; 0 C++-referenced rows).

## Real owners

- `floater_lightbox_settings` / `floater` at `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:8` (4 matching rows).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALBloomGlowKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `bloom` / `set_RenderBloomHDR` (HDR bloom) | none | kit `toggle` ↔ `RenderBloomHDR` `Boolean` | none |
| `bloom` / `set_RenderBloomStrength` (Strength) | none | kit `slider` ↔ `RenderBloomStrength` `F32` | page `exposure_tab`, `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:71` `exp_bloom_strength` |
| `bloom` / `set_RenderBloomThreshold` (Threshold) | none | kit `slider` ↔ `RenderBloomThreshold` `F32` | page `exposure_tab`, `indra/newview/skins/default/xui/en/panel_exposure_hub.xml:73` `exp_bloom_threshold` |
| `bloom` / `set_RenderBloomKnee` (Knee) | none | kit `slider` ↔ `RenderBloomKnee` `F32` | none |
| `bloom` / `set_RenderBloomHalation` (Halation) | none | kit `toggle` ↔ `RenderBloomHalation` `Boolean` | none |
| `bloom` / `set_RenderBloomHalationStrength` (Halation amt) | none | kit `slider` ↔ `RenderBloomHalationStrength` `F32` | none |
| `bloom` / `set_RenderBloomHalationTint` (Halation tint) | none | kit `color` ↔ `RenderBloomHalationTint` `Color3` | none |
| `bloom` / `set_RenderBloomScatter` (Scatter) | none | kit `slider` ↔ `RenderBloomScatter` `F32` | none |
| `bloom` / `set_RenderBloomMipCount` (Mip levels) | none | kit `number` ↔ `RenderBloomMipCount` `S32` | `indra/newview/skins/default/xui/en/floater_preferences_graphics_advanced.xml:738` `RenderPostProcess` |
| `bloom` / `set_RenderBloomResolutionScale` (Resolution) | none | kit `combo` ↔ `RenderBloomResolutionScale` `S32` | none |
| `bloom` / `set_RenderBloomAlphaGlowBoost` (Glow boost) | none | kit `number` ↔ `RenderBloomAlphaGlowBoost` `F32` | none |
| `prim_glow` / `set_RenderGlow` (Use glow) | none | kit `toggle` ↔ `RenderGlow` `Boolean` | none |
| `prim_glow` / `glow_strength_slider` (Strength) | `slider_bar` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:4189` | kit `slider` ↔ `RenderGlowStrength` `F32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:4205` `glow_strength_spinner` |
| `prim_glow` / `glow_width_slider` (Softness) | `slider_bar` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:4246` | kit `slider` ↔ `RenderGlowWidth` `F32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:4262` `glow_width_spinner` |
| `prim_glow` / `glow_iteration_slider` (Iterations) | `slider_bar` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:4075` | kit `slider` ↔ `RenderGlowIterations` `S32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:4091` `glow_iteration_spinner` |
| `prim_glow` / `glow_resPow_slider` (Resolution) | `slider_bar` `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:4132` | kit `slider` ↔ `RenderGlowResolutionPow` `S32` | `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml:4148` `glow_resPow_spinner`, `indra/newview/skins/default/xui/en/floater_preferences_graphics_advanced.xml:85` `GlowResolution` |
| `adv_bloom` / `open_lightbox_glow` (Lightbox ›) | none | **USER DECISION:** register an explicit `ALBloomGlowKitAdapter` command; no matching setting or C++ literal was found | none |

## Adapter extraction

No approved widget has a C++ literal reference. `ALBloomGlowKitAdapter` owns only explicit actions and setting signals; it must not synthesize a polling mirror.

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
| `card-bloom-tallest-850` | Only `bloom` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-bloom-tallest-1160` | Only `bloom` is put in its tallest local state at 1160 px; all other cards stay default. |
| `set_RenderBloomHDR-on` | Only `set_RenderBloomHDR` is on; dependent visibility is measured. |
| `set_RenderBloomHalation-on` | Only `set_RenderBloomHalation` is on; dependent visibility is measured. |
| `set_RenderBloomResolutionScale-option-0` | Only `set_RenderBloomResolutionScale` selects `Full`; other selectors remain default. |
| `set_RenderBloomResolutionScale-option-1` | Only `set_RenderBloomResolutionScale` selects `3/4`; other selectors remain default. |
| `set_RenderBloomResolutionScale-option-2` | Only `set_RenderBloomResolutionScale` selects `Half`; other selectors remain default. |
| `set_RenderBloomResolutionScale-option-3` | Only `set_RenderBloomResolutionScale` selects `Quarter`; other selectors remain default. |
| `set_RenderBloomResolutionScale-option-4` | Only `set_RenderBloomResolutionScale` selects `Eighth`; other selectors remain default. |
| `card-prim_glow-tallest-850` | Only `prim_glow` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-prim_glow-tallest-1160` | Only `prim_glow` is put in its tallest local state at 1160 px; all other cards stay default. |
| `set_RenderGlow-on` | Only `set_RenderGlow` is on; dependent visibility is measured. |
| `card-adv_bloom-tallest-850` | Only `adv_bloom` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-adv_bloom-tallest-1160` | Only `adv_bloom` is put in its tallest local state at 1160 px; all other cards stay default. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## New widget required

None. Every approved visual kind is either a kit primitive or an existing XUI control. Missing action/model bindings below are USER DECISIONS, not permission to invent a visual class.

## Unresolved and USER DECISION items

- `set_RenderBloomHDR` (HDR bloom): widget not found in XUI.
- `set_RenderBloomStrength` (Strength): widget not found in XUI.
- `set_RenderBloomThreshold` (Threshold): widget not found in XUI.
- `set_RenderBloomKnee` (Knee): widget not found in XUI.
- `set_RenderBloomHalation` (Halation): widget not found in XUI.
- `set_RenderBloomHalationStrength` (Halation amt): widget not found in XUI.
- `set_RenderBloomHalationTint` (Halation tint): widget not found in XUI.
- `set_RenderBloomScatter` (Scatter): widget not found in XUI.
- `set_RenderBloomMipCount` (Mip levels): widget not found in XUI.
- `set_RenderBloomResolutionScale` (Resolution): widget not found in XUI.
- `set_RenderBloomAlphaGlowBoost` (Glow boost): widget not found in XUI.
- `set_RenderGlow` (Use glow): widget not found in XUI.
- `open_lightbox_glow` (Lightbox ›): widget not found in XUI; no C++ reference or setting binding. **USER DECISION required.**

## Risks

- A widget-name collision can select an unrelated XUI root or C++ literal; ownership must be resolved by root, class, and behavior together.
- Extracting model work without rewiring the existing owner creates divergent v1/v2 behavior; the extraction and forwarding call are one reviewed change.
- Duplicate writable settings must obey the rollout homes table; this page emits a link/read-only row when it is not the chosen home.
- Shared edit models require owner tokens, and external presets/cue playback require one global history barrier at start/stop rather than per write.
- Unresolved rows fail generation closed; they are never silently replaced with a guessed widget or command.
