# Light Cues — Director Kit page contract

Page key: `light_cues_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (0/6 setting-bound rows; 3 C++-referenced rows).

## Real owners

- `floater_cine_light_cues` / `floater` at `indra/newview/skins/default/xui/en/floater_cine_light_cues.xml:9` (3 matching rows).
- `floater_cine_light_cues` in `indra/newview/skins/default/xui/en/floater_cine_light_cues.xml:2`; `ALFloaterCineLightCues` in `indra/newview/alfloatercinelightcues.cpp:51-403`.
- Referencing classes: `ALFloaterCineLightCues` (3).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Extract the existing cue list/editor stock controls into a shared owner-token view only if inline editing is required. Embed size: full card width, 240 px preferred, 160 px minimum, 320 px maximum, no nested page scroller.

## Row bindings

`ALLightCueKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `cues` / `cue_status` (Current cue) | `text` `indra/newview/skins/default/xui/en/floater_cine_light_cues.xml:85` | `ALLightCueKitAdapter` command/model binding → `ALFloaterCineLightCues::postBuild()` (`indra/newview/alfloatercinelightcues.cpp:70`) | none |
| `cues` / `cue_back` (Back) | `button` `indra/newview/skins/default/xui/en/floater_cine_light_cues.xml:74` | `ALLightCueKitAdapter` command/model binding → `ALFloaterCineLightCues::postBuild()` (`indra/newview/alfloatercinelightcues.cpp:98`) | none |
| `cues` / `cue_go` (Go) | `button` `indra/newview/skins/default/xui/en/floater_cine_light_cues.xml:76` | `ALLightCueKitAdapter` command/model binding → `ALFloaterCineLightCues::postBuild()` (`indra/newview/alfloatercinelightcues.cpp:97`) | none |
| `cues` / `cue_open_stack` (Cue editor) | none | **USER DECISION:** register an explicit `ALLightCueKitAdapter` command; no matching setting or C++ literal was found | none |
| `adv_cues` / `cue_timecode_check` (Timecode) | none | **USER DECISION:** register an explicit `ALLightCueKitAdapter` command; no matching setting or C++ literal was found | none |
| `adv_cues` / `cue_open_stack_adv` (Cue editor) | none | **USER DECISION:** register an explicit `ALLightCueKitAdapter` command; no matching setting or C++ literal was found | none |

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALFloaterCineLightCues::loadEditor/readEditor()` — `indra/newview/alfloatercinelightcues.cpp:241` | Move model-facing work into `ALLightCueKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterCineLightCues::onCueSelected/onCapture/onUpdate/onDeleteCue()` — `indra/newview/alfloatercinelightcues.cpp:294` | Move model-facing work into `ALLightCueKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterCineLightCues::onGo/onBack/onGoto/onRelease()` — `indra/newview/alfloatercinelightcues.cpp:362` | Move model-facing work into `ALLightCueKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterCineLightCues::onListLoad/onListSave/onListDelete()` — `indra/newview/alfloatercinelightcues.cpp:265` | Move model-facing work into `ALLightCueKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterCineLightCues::onTimecodeMode()` — `indra/newview/alfloatercinelightcues.cpp:353` | Move model-facing work into `ALLightCueKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterCineLightCues::postBuild()` — `indra/newview/alfloatercinelightcues.cpp:70, 97, 98` | Move model-facing work into `ALLightCueKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterCineLightCues::refreshCueRows()` — `indra/newview/alfloatercinelightcues.cpp:155` | Move model-facing work into `ALLightCueKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterCineLightCues::refreshListNames()` — `indra/newview/alfloatercinelightcues.cpp:126` | Move model-facing work into `ALLightCueKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterCineLightCues::refreshStatus()` — `indra/newview/alfloatercinelightcues.cpp:203` | Move model-facing work into `ALLightCueKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **12 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-cues-tallest-850` | Only `cues` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-cues-tallest-1160` | Only `cues` is put in its tallest local state at 1160 px; all other cards stay default. |
| `card-adv_cues-tallest-850` | Only `adv_cues` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-adv_cues-tallest-1160` | Only `adv_cues` is put in its tallest local state at 1160 px; all other cards stay default. |
| `cue_timecode_check-on` | Only `cue_timecode_check` is on; dependent visibility is measured. |
| `selection-deleted` | The selected model row is removed by another owner while visible. |
| `shared-owner-other-floater` | The standalone cue floater owns edit state; Director remains read-only. |
| `shared-owner-director` | Director owns edit state; the standalone floater mirrors without claiming. |
| `cue-playback` | Cue playback is active; history recording is suspended until stop. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## New widget required

None. Every approved visual kind is either a kit primitive or an existing XUI control. Missing action/model bindings below are USER DECISIONS, not permission to invent a visual class.

## Unresolved and USER DECISION items

- `cue_open_stack` (Cue editor): widget not found in XUI; no C++ reference or setting binding. **USER DECISION required.**
- `cue_timecode_check` (Timecode): widget not found in XUI; no C++ reference or setting binding. **USER DECISION required.**
- `cue_open_stack_adv` (Cue editor): widget not found in XUI; no C++ reference or setting binding. **USER DECISION required.**

## Risks

- A widget-name collision can select an unrelated XUI root or C++ literal; ownership must be resolved by root, class, and behavior together.
- Extracting model work without rewiring the existing owner creates divergent v1/v2 behavior; the extraction and forwarding call are one reviewed change.
- Duplicate writable settings must obey the rollout homes table; this page emits a link/read-only row when it is not the chosen home.
- Shared edit models require owner tokens, and external presets/cue playback require one global history barrier at start/stop rather than per write.
- Unresolved rows fail generation closed; they are never silently replaced with a guessed widget or command.
