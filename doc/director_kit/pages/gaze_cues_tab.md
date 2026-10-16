# Gaze Cues — Director Kit page contract

Page key: `gaze_cues_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (0/5 setting-bound rows; 3 C++-referenced rows).

## Real owners

- `floater_gaze_cues` / `floater` at `indra/newview/skins/default/xui/en/floater_gaze_cues.xml:9` (3 matching rows).
- `floater_gaze_cues` in `indra/newview/skins/default/xui/en/floater_gaze_cues.xml:2`; `ALFloaterGazeCues` in `indra/newview/alfloatergazecues.cpp:101-663`.
- Referencing classes: `ALFloaterGazeCues` (6).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

Keep the cue list plus Add, Remove, Play, and Stop in Director. Selecting a row opens the existing Gaze Cues window at that cue; detailed keyframe editing remains there.

## Row bindings

`ALGazeCueKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|
| `gaze_cue_track` / `cue_list` (Cues) | `scroll_list` `indra/newview/skins/default/xui/en/floater_gaze_cues.xml:31` | `ALGazeCueKitAdapter` command/model binding → wired in `ALFloaterGazeCues::postBuild()`; behavior `ALFloaterGazeCues::onCueSelected()` (`indra/newview/alfloatergazecues.cpp:138`) | none |
| `gaze_cue_track` / `btn_cue_add` (Add) | `button` `indra/newview/skins/default/xui/en/floater_gaze_cues.xml:39` | `ALGazeCueKitAdapter` command/model binding → wired in `ALFloaterGazeCues::postBuild()`; behavior `ALFloaterGazeCues::onAdd()` (`indra/newview/alfloatergazecues.cpp:140`) | none |
| `gaze_cue_track` / `btn_cue_delete` (Remove) | `button` `indra/newview/skins/default/xui/en/floater_gaze_cues.xml:43` | `ALGazeCueKitAdapter` command/model binding → wired in `ALFloaterGazeCues::postBuild()`; behavior `ALFloaterGazeCues::onDelete()` (`indra/newview/alfloatergazecues.cpp:142`) | none |
| `gaze_cue_track` / `gaze_cue_play` (Play) | none | kit-native action `engine-work:WP8:LLActorMover::setGazeCuePreview`; no duplicate widget is required | none |
| `gaze_cue_track` / `gaze_cue_stop` (Stop) | none | kit-native action `engine-work:WP8:LLActorMover::clearGazeCuePreview`; no duplicate widget is required | none |

## Cue-list behavior decided 2026-10-10

- Row format is `Time | Target | Macro`, exactly the fields built by `ALFloaterGazeCues::refreshCueList()` (`alfloatergazecues.cpp:235-262`).
- Add reuses `onAdd()` (`alfloatergazecues.cpp:409-417`); Remove reuses `onDelete()` (`:436-452`). Both mutate through `LLDirectorCast::setGazeCues()` (`lldirectorcast.cpp:1364-1397`), which sanitizes, sorts, bumps `getGazeCueRevision()`, and clears gaze runtime.
- Selecting a Director row opens `gaze_cues` for that actor and selects the same cue, then follows `onCueSelected()` (`alfloatergazecues.cpp:395-406`) to load the detailed editor. Detailed targets, macros, transitions, and influence keyframes remain only in that window.
- Existing transport cannot seek gaze time: `LLDirectorCast` exposes only `action()`/`cut()` (`lldirectorcast.h:415-421`, implemented at `lldirectorcast.cpp:2498-2660`), `LLPresentationTime` has no seek/setter, and `LLFlycamRecorder::seek(F32)` (`llflycamrecorder.cpp:284`) affects only recorder playback. Therefore this is explicit **WP8 engine work**, not a call to the pure `evaluateGazeCues()` helper.
- Add exact public APIs `bool LLActorMover::setGazeCuePreview(const LLUUID& actor_id, S32 cue_index);` and `void LLActorMover::clearGazeCuePreview(const LLUUID& actor_id);`. Play calls the first; Stop calls the second. Invalid/unresolved actor or out-of-range index returns false without mutation. One preview per actor stores the cue index and preview-start presentation time; `applyDirectorLookAt()` consults it before normal track evaluation at `llactormover.cpp:6649-6668`, evaluates a one-cue copy on preview-local elapsed time, and never edits the cue list or global presentation clock. Clear/actor removal/logout restores the normal authored/track path and clears Director gaze runtime.
- Refresh is event-driven: add a controller revision signal at `setGazeCues()`'s existing revision bump and subscribe while visible. Refresh once on attach/reveal, actor change, and that signal; do not call the floater's per-frame `draw()` refresh path (`alfloatergazecues.cpp:187-193`).
- Add and Remove are one `ALKitHistory` command each, storing the actor ID and complete before/after `GazeCueList`; Remove is therefore undoable and needs no confirmation. Play and Stop each place one `ALKitHistory::globalBarrier(reason)` and history recording is suspended between them.

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `ALFloaterGazeCues::loadEditor()` — `indra/newview/alfloatergazecues.cpp:320` | Move model-facing work into `ALGazeCueKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterGazeCues::loadInfluenceEditor/readInfluenceEditor()` — `indra/newview/alfloatergazecues.cpp:584` | Move model-facing work into `ALGazeCueKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterGazeCues::onAdd()` — `indra/newview/alfloatergazecues.cpp:140` | Move model-facing work into `ALGazeCueKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterGazeCues::onAdd/onEdit/onDelete()` — `indra/newview/alfloatergazecues.cpp:409` | Move model-facing work into `ALGazeCueKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterGazeCues::onCueSelected()` — `indra/newview/alfloatergazecues.cpp:138, 395` | Move model-facing work into `ALGazeCueKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterGazeCues::onDelete()` — `indra/newview/alfloatergazecues.cpp:142` | Move model-facing work into `ALGazeCueKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterGazeCues::onInfluenceAdd/onInfluenceUpdate/onInfluenceDelete()` — `indra/newview/alfloatergazecues.cpp:634` | Move model-facing work into `ALGazeCueKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterGazeCues::onQuickMacro()` — `indra/newview/alfloatergazecues.cpp:486` | Move model-facing work into `ALGazeCueKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterGazeCues::onSetPoint/onPickObject/onClearObject()` — `indra/newview/alfloatergazecues.cpp:460` | Move model-facing work into `ALGazeCueKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterGazeCues::readEditor()` — `indra/newview/alfloatergazecues.cpp:342` | Move model-facing work into `ALGazeCueKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterGazeCues::refreshCastCombo()` — `indra/newview/alfloatergazecues.cpp:274` | Move model-facing work into `ALGazeCueKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterGazeCues::refreshCueList()` — `indra/newview/alfloatergazecues.cpp:235` | Move model-facing work into `ALGazeCueKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterGazeCues::refreshEditorVisibility()` — `indra/newview/alfloatergazecues.cpp:303` | Move model-facing work into `ALGazeCueKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `ALFloaterGazeCues::refreshInfluenceList()` — `indra/newview/alfloatergazecues.cpp:545` | Move model-facing work into `ALGazeCueKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **16 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `card-gaze_cue_track-tallest-850` | Only `gaze_cue_track` is put in its tallest local state at 850 px; all other cards stay default. |
| `card-gaze_cue_track-tallest-1160` | Only `gaze_cue_track` is put in its tallest local state at 1160 px; all other cards stay default. |
| `cue_list-empty` | `cue_list` is empty with no selection. |
| `cue_list-one` | `cue_list` has one selected item. |
| `cue_list-overflow` | `cue_list` has 51 items and a visible scrollbar. |
| `actor-none` | Rail has no actor; target-dependent rows are disabled with an explicit empty state. |
| `actor-selected` | One rail actor is selected and unpinned. |
| `actor-pinned-rail-changes` | Page target is pinned while the rail changes to another actor. |
| `actor-removed` | The active actor disappears while the page is visible. |
| `selection-deleted` | The selected model row is removed by another owner while visible. |
| `shared-owner-other-floater` | The standalone cue floater owns edit state; Director remains read-only. |
| `shared-owner-director` | Director owns edit state; the standalone floater mirrors without claiming. |
| `cue-playback` | Cue playback is active; history recording is suspended until stop. |

## Parity check

1. Instantiate the non-Director owner root(s) above and the kit page against the same settings/model snapshot.
2. For every table row, compare initial value, enabled/visible state, options/list contents, commit result, reset/default, and post-undo value. Exercise every listed C++ handler through both call sites.
3. Run the bounded state manifest at 1160×720, 850×560, 1141×720, and 915×720. Non-Director owners (Lightbox, cue/Diopter/Snapshot floaters, and shared panels) are mandatory parity oracles even when no v1 Director leaf embeds them.
4. Review-diff every controller extraction and prove the v1 forwarding call preserves callbacks, owner tokens, selection, history barriers, and visibility gates.

## Ambiguous same-name candidates (never auto-bound)

None.

## Rejected out-of-scope same-name candidates

23 candidates were rejected by the XUI-owner/class scope; exact records and reasons are in `page_sources.json`.

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
