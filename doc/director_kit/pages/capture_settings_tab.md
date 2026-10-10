# Capture — Director Kit page contract

Page key: `capture_settings_tab`. Evidence: [`page_sources.json`](../page_sources.json) and [`page_sources.md`](../page_sources.md).
Computed class: **code-driven** (0/0 setting-bound rows; 0 C++-referenced rows).
**PENDING USER DECISION**


## Real owners

- `Snapshot` / `floater` at `indra/newview/skins/default/xui/en/floater_snapshot.xml:9` (0 matching rows).
- Snapshot owns capture/output behavior: `LLFloaterSnapshot`/`LLFloaterSnapshotBase` in `indra/newview/llfloatersnapshot.cpp:79-489` and the `panel_snapshot_*.xml` panels; the approved Director page currently has zero rows.
- Referencing classes: `LLFloaterSnapshot` (0).

Name collisions in secondary XUI files are evidence candidates, not automatic ownership. The implementation must retain only occurrences whose root and class agree with the row's behavior.

## Presentation rule

The approved page has zero rows. Populate it from the Snapshot owner before conversion; control selection and scope are PENDING USER DECISION.

## Row bindings

`ALCaptureSettingsKitAdapter` is the page bridge. A direct setting row remains an `ALKitSettingBinding`; a C++-referenced row routes through the named existing function below or through a controller extraction of that same function. The v1 owner is rewired to that controller in the same work package.

| Card / row | Existing control evidence | Required binding | Other writable homes |
|---|---|---|---|

## Adapter extraction

| Existing function | Extraction contract |
|---|---|
| `LLFloaterSnapshot::Impl::getActivePanel/getImageFormat()` — `indra/newview/llfloatersnapshot.cpp:79` | Move model-facing work into `ALCaptureSettingsKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterSnapshot::Impl::getWidthSpinner/getHeightSpinner()` — `indra/newview/llfloatersnapshot.cpp:116` | Move model-facing work into `ALCaptureSettingsKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterSnapshot::Impl::updateControls()` — `indra/newview/llfloatersnapshot.cpp:292` | Move model-facing work into `ALCaptureSettingsKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterSnapshotBase::ImplBase::setStatus/setNeedRefresh()` — `indra/newview/llfloatersnapshot.cpp:448` | Move model-facing work into `ALCaptureSettingsKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |
| `LLFloaterSnapshotBase::ImplBase::updateLayout()` — `indra/newview/llfloatersnapshot.cpp:180` | Move model-facing work into `ALCaptureSettingsKitAdapter` or its controller; keep a v1 forwarding call and diff its behavior. Presentation-only refresh stays in the owner. |

## Live state and refresh

- Cache all widget handles in `attach()`. Subscribe once to every setting's commit signal and to the existing model/controller revision or callback named above.
- Force one diff-guarded refresh on reveal, target change, undo/redo, preset completion, and owner-token transfer. Lists refresh from a monotonic model revision; live numeric readouts are capped at 10 Hz.
- Hidden pages unsubscribe or return before presentation writes. No per-frame setting reads, name lookups, list rebuilds, signature strings, or raw writes are permitted in `draw()`.
- If an existing owner currently refreshes from `draw()`, the extraction adds an explicit dirty/revision signal; the adapter never calls that `draw()` path.

## Bounded non-default audit states

Manifest size: **6 states**, hard cap 256, page runtime budget 2 minutes. Each row below is applied independently from the default snapshot; there is no Cartesian product.

| State key | Exact scenario |
|---|---|
| `size-850x560` | Default data at the minimum supported shell size. |
| `size-915x720` | Default data at the narrow 16:9 shell size. |
| `size-1141x720` | Default data at the wide non-round shell size. |
| `permission-denied` | The current target exists but cannot be modified. |
| `target-offline` | The current target/provider becomes unavailable. |
| `data-missing` | Approved page remains empty; audit must fail closed until rows are approved. |

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

- **PENDING USER DECISION:** The approved page has zero rows. Populate it from the Snapshot owner before conversion; control selection and scope are PENDING USER DECISION.

## Risks

- A widget-name collision can select an unrelated XUI root or C++ literal; ownership must be resolved by root, class, and behavior together.
- Extracting model work without rewiring the existing owner creates divergent v1/v2 behavior; the extraction and forwarding call are one reviewed change.
- Duplicate writable settings must obey the rollout homes table; this page emits a link/read-only row when it is not the chosen home.
- Shared edit models require owner tokens, and external presets/cue playback require one global history barrier at start/stop rather than per write.
- Unresolved rows fail generation closed; they are never silently replaced with a guessed widget or command.
