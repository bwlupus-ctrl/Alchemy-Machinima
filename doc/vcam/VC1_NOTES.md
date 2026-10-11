# VC1 implementation notes

## Implemented

- Capture labels are UTF-8-normalized, trimmed, control-character sanitized, and truncated to 64 bytes at a codepoint boundary. Labels are exposed in snapshots, serialized as optional scene data, defaulted for old scenes, and editable through the registry (`indra/newview/llprismlens.cpp:64`, `:3889`, `:5243`, `:5811`).
- Added main-thread configuration/runtime/performance revision signals. Nested registry operations coalesce to one configuration notification; runtime and performance delivery is limited to 10 Hz (`indra/newview/llprismlens.cpp:7161`, `:7885`).
- Added the single target validation path used by selected-face and direct-target callers, including root modify permission, geometry, feedback/source, duplicate, and capacity checks. Bind and reassign use it; reassignment preserves the display record and remains available at 16/16 (`indra/newview/llprismlens.cpp:1916`, `:2150`, `:2194`).
- Added exact-identity capture/display removal records and restore APIs. Capture restore preserves UUID+generation and prefers the original free slot, while Gate restore inserts only the removed arm and conditionally restores an unchanged on-air route without rewinding the cut serial. Capture-local removal retains shared scratch while peers exist and frees it after the final capture.
- Scene apply now returns a typed report. It validates the full payload before mutation, rejects only malformed Prism/Gate data atomically, and quarantines unavailable, geometry-invalid, or permission-failed live faces while Director continues loading (`indra/newview/llprismlens.cpp`, `indra/newview/llfloaterdirector.cpp`).
- Extended `ALKitHistory` custom entries with preflight/release hooks, explicit commit/cancel, reservation-lifetime cleanup on eviction/barrier, and intact-step failure messages (`indra/newview/alkithistory.cpp:125`, `:141`, `:158`, `:298`).
- Added reusable `ALVCamPageModel` with cached snapshots, selected capture/display and monitor mapping, idle-coalesced dirty delivery, shared current-view transform, typed Add/Remove/Bind/Reassign/setting commands, and fine-grained history (`indra/newview/alvcampagemodel.cpp:125`, `:187`, `:356`). The existing manager delegates current-view math to the model (`indra/newview/llfloaterprismmanager.cpp:1928`).
- Added persistent `PrismMonitorPreviewsEnabled=true` and `PrismMonitorPreviewFPS=1.0`; the page's discrete selector will constrain the latter to 0.5/1/2/5 in VC6 (`indra/newview/app_settings/settings.xml:29483`). CMake source/header lists were updated (`indra/newview/CMakeLists.txt:214`, `:1124`).

## Verification and deviations

- Per request, no configure, build, or executable test was run. Static verification performed: `settings.xml` parsed successfully, `git diff --check` passed, and every modified/new text file was normalized to LF.
- No new runnable registry unit test was added: the current registry is inseparable from live viewer globals (selection, object list, pipeline, Gate controller). VC1 exposes typed records and target APIs so later scheduler/viewer harnesses can exercise them without scene replay.
- Undo identity reservation is represented by immutable typed records held for the lifetime of each history entry. UUIDs are random and generations are monotonic/non-reused; restore preflights identity/capacity conflicts, while history eviction/barriers release entry-owned records. No separate duplicate reservation table was added because it would not strengthen those allocator guarantees.

## Fix round

- Scene loads now quarantine transient geometry as retryable offline state and permission failures as cached permission quarantine; live-world state no longer rejects an otherwise structurally valid scene.
- Quarantine revalidation is capture-kind aware, including a Surface Lens revalidating its owned face. A Surface Lens with its sole face quarantined is not scheduled, and permission quarantine avoids repeated geometry scans until modify permission changes.
- Capture identity restore preserves unrelated Gate roles, active state, and cut serial. It restores the removed arm's own Preview/Warm roles and restores/punches its former on-air role only when no later cut occurred.
- OTS replacement uses identity-preserving rollback, defaults legacy OTS capture labels from their `OTS A`/`OTS B` arms, and wraps multi-capture swaps in one revision-signal scope.
- Capture restore prefers its original slot and falls back to another free slot with the same UUID+generation. VCam identity-history precondition failures are discarded after reporting so they cannot pin the Director undo/redo stack.
- Label controls are removed before trimming, and the final-capture teardown releases the shared Prism scratch pool.
