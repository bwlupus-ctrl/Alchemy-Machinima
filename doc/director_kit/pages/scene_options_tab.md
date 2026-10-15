# Scene — Director Kit page contract

Page key: `scene_options_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (2/4 setting-bound rows; 0 C++-referenced rows).

## Real owners

- `floater_director` / `floater` at `indra/newview/skins/default/xui/en/floater_director.xml:15` (3 matching rows).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Use the existing XUI control types as ordinary measured kit rows. No new canvas, curve, timeline, slot-grid, or picker class is authorized.

## Row bindings

`ALSceneOptionsKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `scene_options` / `scene_hotkeys_enabled` (Hotkeys) | `check_box` `indra/newview/skins/default/xui/en/floater_director.xml:1758` | kit `toggle` ↔ `DirectorHotkeysEnabled` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_capture_scene_options.xml:8` `scene_hotkeys_enabled` |
| `scene_options` / `scene_add_self_at_login` (Add me at login) | none | kit `toggle` ↔ `DirectorAddSelfAtLogin` `Boolean` | `indra/newview/skins/default/xui/en/panel_dpage_capture_scene_options.xml:11` `scene_add_self_at_login` |
| `scene_options` / `scene_hotkeys_list` (Shortcuts) | `text` `indra/newview/skins/default/xui/en/floater_director.xml:1759` | kit readout ← approved static copy; no runtime writer | none |
| `scene_options` / `scene_categories_text` (Saved in file) | `text` `indra/newview/skins/default/xui/en/floater_director.xml:1761` | kit readout ← approved static copy; no runtime writer | none |

## Adapter extraction

No approved widget has a C++ literal reference. `ALSceneOptionsKitAdapter` owns only explicit actions and setting signals; it must not synthesize a polling mirror.

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **7 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-scene_options-tallest-850` | Only `scene_options` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-scene_options-tallest-1160` | Only `scene_options` is put in its tallest local state at 1160 px; all other cards stay default. |
| `scene_hotkeys_enabled-on` | Only `scene_hotkeys_enabled` is on; dependent visibility is measured. |
| `scene_add_self_at_login-on` | Only `scene_add_self_at_login` is on; dependent visibility is measured. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## Ambiguous same-name candidates (never auto-bound)

None.

## Rejected out-of-scope same-name candidates

4 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

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
