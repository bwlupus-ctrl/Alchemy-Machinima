# VC4 implementation notes

## Implemented

- Removed all VCam `ALKitHistory` integration. `ALVCamPageModel` now mutates the registry directly, and the VC1-only preflight/release/discard history extensions were removed from `ALKitHistory`.
- Removed the public capture/display identity-record and restore API. OTS replacement keeps only private, stack-lifetime capture/display rollback records plus one exact Gate snapshot restored synchronously on failure; transient/offline display geometry is quarantined during rollback rather than making rollback depend on rez timing.
- Scene-load reporting now counts permission failures separately from offline/loading faces. Invalid-geometry quarantine stores the rejected source fingerprint and reason, avoiding repeat geometry scans until the volume/face source changes.
- Added an allocation-free `PrismPreviewBudgetController`: two-second presented-frame reservations, independent CPU/GPU samples charged by `max(cpu,gpu)`, cold predictions, rolling p95 prediction, hard tail throttling, 120-clean-sample recovery, suspension, and scratch-reacquisition accounting.
- Added fixed timestamp-query rings using non-blocking `GL_TIMESTAMP` pairs. Results are first polled three frames later and only read when available. Preview-only auxiliary timing begins before scratch acquisition and ends after scoped renderer restoration; atlas timing likewise encloses compositor setup through GL restoration. Tracy CPU/GPU zones name both operations.
- Admission remains below visible/Gate scheduling: only a slot already classified preview-only consults the preview budget. Rejected reservations advance the existing deadline once and never create catch-up work. Atlas admission retains the previous cell on deferral.
- The budget-limited effective shared rate steps 5 -> 2 -> 1 -> 0.5 fps and then suspends. Tile state exposes `THROTTLED`; service state separately exposes throttled/suspended. Feature-off returns before query polling, allocation, or issuance.
- Added the non-persistent `PrismPreviewSelfTest` trigger and `LLPrismPreview::runSelfTest()`. The eight-capture/1-fps test runs baseline, warming, and enabled phases and emits one machine-readable PASS/FAIL/INCONCLUSIVE line with CPU, GPU, charged, scratch, p95, maximum, coverage, deferral, frame, and entitlement metrics.
- Extended `LLPrismPreviewPolicy` tests with synthetic independent CPU/GPU charging, reservation, tail suspension/recovery, and scratch-reacquisition checks; existing tests continue to cover visible-first water filling and no catch-up deadlines.

## Verification

- Per request, no configure, build, executable test, commit, fetch, or upstream operation was run.
- Static verification: settings XML parse, symbol/call-site sweeps for removed identity/history APIs, `git diff --check`, and LF/trailing-whitespace checks.

## Fix round

- Tail enforcement now waits for eight calibration samples, evaluates frame-tail violations only when a new preview frame sample arrives, ignores cold warm-up for enforcement, and recovers suspension through one timed, budgeted probe at 0.5 fps. Visibility and preview-setting changes reset the controller.
- Warm-up admits at most one 0.5 fps preview scheduling grant per frame, uses CPU samples when timestamp queries are unavailable, exposes `WARMING` tiles, and has an explicit 30-second timeout instead of hanging.
- Self-test output identifies missing prerequisites, reports per-capture requested/admitted/produced counts and achieved fps, measures real CPU/GPU coverage, and gates PASS on the CPU-only, GPU-only, charged, tail, scratch, servicing, and visible-entitlement requirements.
- Deferred atlas cells advance their deadlines without a duplicate revision notification. Atlas timing includes its compositor and `ScopedPreviewAtlasGL` restoration; the caller's outer `ScopedPrismRenderState` restoration is intentionally outside that operation measurement.
- Timestamp-query names are deleted while the GL context is live and are only forgotten after true context loss. Visible and preview-only auxiliary renders now have distinct, accurately named Tracy CPU/GPU scopes.
- Per request, this fix round did not configure, build, run executable tests, commit, fetch, or contact an upstream. Verification was static only.
