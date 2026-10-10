# WP5 status

## Scope and compatibility

- **Done — default self-test compatibility:** the no-`DirectorV2`, empty-`UILayoutAuditSizes` path retains the existing two audit passes, CSV names/schema, target order, AB/inventory flow, exit codes, compare behavior, screenshot layout, and original `UIFolderViewCull` restore path. V2 behavior is keyed only by session-only `UIPerfDirectorV2`, not saved `DirectorUIV2`.
- **Done — setting safety:** runner `--set` values remain unsaved command-line overrides. V2 restores only runner overrides, audit settings, and P0 page settings on finish/abort, so login-screen changes outside the test survive; a crash cannot persist test mutations.

## Section 6.4

- **Done — audit sizes:** parses `default`, `min`, and explicit `WxH`; empty input remains `default,min`; `min` emits `minimum`; sizes below the floater minimum are logged and skipped.
- **Done — lazy build measurement:** lazy rescan uses the undecorated raw path and restores newly discovered nested tabs. `ALKitLazyPage::buildMs()` supplies page-only construction time. Kit pages emit `kit_build`; legacy embeds emit `kit_build_legacy` and are excluded from the 50 ms gate; floater-open time is not reused.
- **Done — isolated bounded states:** every state resets mode, chip, card body/More, selector, master, and map defaults; page defaults are captured once and reused across sizes. Selector/map states reveal their owning card. Application is history-suspended; dead, refused, or hidden targets record `applied=0` and never count as covered.
- **Done — measures and caps:** steady and hover subtract same-pass baseline; page-zero `steady` participates; synthetic hover is cleared and settled before audits; only sampled hit tests are timed. State settle/capture caps, 256 states/page, two minutes/page, and a 75-minute V2 suite timeout are enforced.
- **Done — runner and budgets:** `-DirectorV2` passes session markers and four sizes. Budget output includes target values and exits 0 on a budget pass despite legacy generic Director thresholds. Legacy-vs-v1 normalizes host prefixes, coverage markers, width-dependent detail, and baseline size names.
- **Partial — future rollout states:** validation/error, popover, inline-rename, list-cardinality, permission/offline, cue, and VCam manifests remain WP6–WP12 work outside P0.
- **Partial — runtime type stream:** `director_parity.py --groups light_group` previously passed with 0 findings across 61 requests and 0 unresolved dynamic lookups, so P0 has no pattern requiring runtime proof. The full requested/actual stream remains WP6 adapter/source work.

## Verification and handoff

- **Done — local static checks:** PowerShell 5 parsing passed, `git diff --check` found no whitespace errors, and `ui_draw_lint.py --changed HEAD --json` reports `new_errors: 0` with no baseline additions.
- **Not run by request:** no build, `/Zs`, commit, or full Python gate suite; those remain for the orchestrator.
- **Post-build checklist:** exercise master/map states pre-login, confirm abort-by-login restores saved `DirectorUIV2` before settings saving resumes, and inspect Rig/Rim screenshots plus the legacy comparison report.
