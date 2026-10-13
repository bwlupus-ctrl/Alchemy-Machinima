# Diopter — Director Kit page contract

Page key: `diopter_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (4/5 setting-bound rows; 1 C++-referenced rows).

## Real owners

- `ultimate_diopter` / `floater` at `indra/newview/skins/default/xui/en/floater_ultimate_diopter.xml:15` (4 matching rows).
- `ultimate_diopter` in `indra/newview/skins/default/xui/en/floater_ultimate_diopter.xml:8`; `ALFloaterUltimateDiopter` in `indra/newview/alfloaterultimatediopter.cpp:266-1565`.
- Referencing classes: `ALFloaterUltimateDiopter` (1).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Keep only the everyday controls (on/off, preset, strength, focus split) plus Open full editor. The existing 560×720 nine-tab floater remains the detailed editor.

## Row bindings

`ALDiopterKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `diopter_summary` / `diopter_enabled` (On / off) | `check_box` `indra/newview/skins/default/xui/en/floater_ultimate_diopter.xml:67` | kit `toggle` ↔ `CineDiopterEnabled` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_lens_diopter.xml:8` `diopter_enabled` |
| `diopter_summary` / `diopter_preset` (Preset) | `combo_box` `indra/newview/skins/default/xui/en/floater_ultimate_diopter.xml:73` | kit `combo` ↔ `CineDiopterPreset` `U32` | `indra/newview/skins/default/xui/en/panel_dpage_lens_diopter.xml:11` `diopter_preset` |
| `diopter_summary` / `diopter_blend` (Strength) | `slider` `indra/newview/skins/default/xui/en/floater_ultimate_diopter.xml:106` | kit `slider` ↔ `CineDiopterBlend` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_lens_diopter.xml:38` `diopter_blend` |
| `diopter_summary` / `diopter_power` (Focus split) | `slider` `indra/newview/skins/default/xui/en/floater_ultimate_diopter.xml:290` | kit `slider` ↔ `CineDiopterPower` `F32` | `indra/newview/skins/default/xui/en/panel_dpage_lens_diopter.xml:43` `diopter_power` |
| `diopter_summary` / `diopter_open_editor` (Full editor) | none | kit-native action `floater:ultimate_diopter`; no duplicate widget is required | none |

## User-decision control contract

| Director label | Real widget / setting | Exact values | Default |
|---|---|---|---|
| On / off | `diopter_enabled` / `CineDiopterEnabled` (`floater_ultimate_diopter.xml:66`; read by `alfloaterultimatediopter.cpp:556`) | Boolean | off (`settings.xml:608`) |
| Preset | `diopter_preset` / `CineDiopterPreset` (`floater_ultimate_diopter.xml:72`) | U32 0-20; the 21 labels are the XUI items in order | 0, Custom (`settings.xml:615`) |
| Strength | `diopter_blend` / `CineDiopterBlend` (`floater_ultimate_diopter.xml:105`) | 0.00-1.00, step 0.01, 2 decimals | 1.00 (`settings.xml:629`) |
| Focus split | `diopter_power` / `CineDiopterPower` (`floater_ultimate_diopter.xml:287`; read by `alfloaterultimatediopter.cpp:604`) | -10.00 to +10.00 D, step 0.25, 2 decimals | +2.00 D (`settings.xml:867`) |
| Open full editor | existing `ultimate_diopter` floater registration (`llviewerfloaterreg.cpp:769`) | action only | n/a |

The Director labels Strength and Focus split are presentation labels only; they bind directly to the existing Master Blend and Power (+D) controls/settings above. Confirm that 'Focus split' reads naturally in the mandatory in-world WP12 check. The full editor is opened, never embedded or recreated.

History test: start from any non-Custom preset, edit Focus split/`CineDiopterPower` so `alDiopterHandlePresetTransition()` (`llviewerfloaterreg.cpp:597-626`) materializes Custom, then undo and verify both the prior preset and rendered look return and the saved Custom bank is byte-for-byte unchanged; redo must reproduce the edit.

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALFloaterUltimateDiopter::applyRowState()` — `indra/newview/alfloaterultimatediopter.cpp:697` | Move model-facing work into `ALDiopterKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterUltimateDiopter::armFocusPick/onFocusPickCommitted()` — `indra/newview/alfloaterultimatediopter.cpp:996` | Move model-facing work into `ALDiopterKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterUltimateDiopter::initRangeCombos/syncRangesToCurrentValues()` — `indra/newview/alfloaterultimatediopter.cpp:1265` | Move model-facing work into `ALDiopterKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterUltimateDiopter::onFilterChanged()` — `indra/newview/alfloaterultimatediopter.cpp:1531` | Move model-facing work into `ALDiopterKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterUltimateDiopter::onRangeComboCommit()` — `indra/newview/alfloaterultimatediopter.cpp:1360` | Move model-facing work into `ALDiopterKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterUltimateDiopter::onSetFromCameraFocusClicked()` — `indra/newview/alfloaterultimatediopter.cpp:1141` | Move model-facing work into `ALDiopterKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterUltimateDiopter::onShowDepthToggled()` — `indra/newview/alfloaterultimatediopter.cpp:1485` | Move model-facing work into `ALDiopterKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterUltimateDiopter::populateState/buildSigKey()` — `indra/newview/alfloaterultimatediopter.cpp:552` | Move model-facing work into `ALDiopterKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterUltimateDiopter::refreshRelevance()` — `indra/newview/alfloaterultimatediopter.cpp:844` | Move model-facing work into `ALDiopterKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterUltimateDiopter::updateBandPanels()` — `indra/newview/alfloaterultimatediopter.cpp:807` | Move model-facing work into `ALDiopterKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **27 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-diopter_summary-tallest-850` | Only `diopter_summary` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-diopter_summary-tallest-1160` | Only `diopter_summary` is put in its tallest local state at 1160 px; all other cards stay default. |
| `diopter_enabled-on` | Only `diopter_enabled` is on; dependent visibility is measured. |
| `diopter_preset-option-0` | Only `diopter_preset` selects `Custom (start from scratch)`; other selectors remain default. |
| `diopter_preset-option-1` | Only `diopter_preset` selects `Split Diopter (classic)`; other selectors remain default. |
| `diopter_preset-option-2` | Only `diopter_preset` selects `Dream Spot`; other selectors remain default. |
| `diopter_preset-option-3` | Only `diopter_preset` selects `Anamorphic Cinema`; other selectors remain default. |
| `diopter_preset-option-4` | Only `diopter_preset` selects `Vintage Swirl`; other selectors remain default. |
| `diopter_preset-option-5` | Only `diopter_preset` selects `Hand Lens (free glass)`; other selectors remain default. |
| `diopter_preset-option-6` | Only `diopter_preset` selects `Halo FX - Subtle`; other selectors remain default. |
| `diopter_preset-option-7` | Only `diopter_preset` selects `Halo FX - Full`; other selectors remain default. |
| `diopter_preset-option-8` | Only `diopter_preset` selects `Halo FX - Half Moon`; other selectors remain default. |
| `diopter_preset-option-9` | Only `diopter_preset` selects `Halo FX - Split Moon`; other selectors remain default. |
| `diopter_preset-option-10` | Only `diopter_preset` selects `Halo FX - Crescent`; other selectors remain default. |
| `diopter_preset-option-11` | Only `diopter_preset` selects `Halo FX - Broken Ring`; other selectors remain default. |
| `diopter_preset-option-12` | Only `diopter_preset` selects `Halo FX - Handheld`; other selectors remain default. |
| `diopter_preset-option-13` | Only `diopter_preset` selects `Faceted - Radial Mirror`; other selectors remain default. |
| `diopter_preset-option-14` | Only `diopter_preset` selects `Concentric Echo`; other selectors remain default. |
| `diopter_preset-option-15` | Only `diopter_preset` selects `Prism Split`; other selectors remain default. |
| `diopter_preset-option-16` | Only `diopter_preset` selects `Heart Bokeh`; other selectors remain default. |
| `diopter_preset-option-17` | Only `diopter_preset` selects `Star Dream`; other selectors remain default. |
| `diopter_preset-option-18` | Only `diopter_preset` selects `Moon Bite (crescent)`; other selectors remain default. |
| `diopter_preset-option-19` | Only `diopter_preset` selects `Petal Dream (flower window)`; other selectors remain default. |
| `diopter_preset-option-20` | Only `diopter_preset` selects `Macro Pulse`; other selectors remain default. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## Ambiguous same-name candidates (never auto-bound)

None.

## Rejected out-of-scope same-name candidates

10 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

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
