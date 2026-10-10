# WP5 status

## Scope and compatibility

- **Done — default self-test compatibility:** the no-`DirectorV2`, empty-`UILayoutAuditSizes` path retains the existing two audit passes (`default`, `minimum`), CSV columns/names, target order, AB/inventory flow, `ready_wait_frames`, incomplete-inventory exit 5, compare behavior, and screenshot directory layout. New behavior is gated by `DirectorUIV2` or the new PowerShell switch.
- **Done — protected files:** WP5 changed only `indra/newview/aluiperfselftest.cpp`, `scripts/perf/ui_selftest.ps1`, and this status file. No WP2 file or UI-draw baseline was edited.

## Section 6.4

- **Done — audit sizes:** parses `default`, `min`, and explicit `WxH`; empty input remains `default,min`; `min` still emits `minimum`; explicit sizes below the floater minimum are logged and skipped.
- **Done — lazy rescan and first reveal:** a newly built `ALKitLazyPage` is rescanned after tab selection; scroller ranges are repaired, nested tabs are inserted after the current target, and the selection time is emitted as `kit_build`.
- **Done — P0 bounded states:** Director v2 audits isolated mode, chip, every card expanded/closed/More, every selector option, master on/off, and light-map view/side states. Each state starts from setting defaults, is synchronously laid out, included in finding/screenshot names, and recorded in `ui_layout_audit_states.csv` even when it has zero findings; the pre-run values are restored afterward.
- **Done — measures:** emits `kit_search` for all fixed queries (maximum), `kit_amber_storm`, `kit_index`, per-page `kit_build`, ordinary per-page `tab` UI timing, and a Rig `hover_sweep` row. Size/state settling and capture use bounded frame caps.
- **Done — `-DirectorV2`:** forces Director-only layout mode and passes `DirectorUIV2=TRUE` plus `UILayoutAuditSizes="1160x720,min,1141x720,915x720"`. Screenshots use `ui_audit/director/audit-<size>/light_group/...` for Rig/Rim.
- **Done — budgets:** prints all seven section 6.4 budget lines and `DIRECTOR V2 BUDGET: PASS|FAIL`; missing measures, missing required state coverage, or a missing v1 layout sidecar fail the gate. `-Compare ui_perf_<stamp>.csv` resolves the matching archived v1 layout CSV.
- **Partial — future rollout states:** P0 Rig/Rim states are covered. Validation/error, popover, inline-rename, list cardinality, permission/offline, cue, and VCam-specific state manifests remain WP6-WP12 work because those converted pages/controllers do not exist in the P0 scope.
- **Partial — runtime requested/actual type ledger item:** existing static parity covers the P0 dynamic lookup patterns. A requested-type/actual-type runtime stream needs the WP6 adapter/source instrumentation and is not fabricated in this self-test-only package.

## Sections 6.5 and 6.7 / verification

- **Done — UI draw lint:** baseline untouched; targeted lint reports `0 errors, 0 new, 0 warnings, 0 baseline additions`.
- **Done — screenshot hook:** Director state captures and the HTML archive reader support the section 6.7 Director directory layout. Mockup rendering and Opus screenshot judgment remain the specified post-build orchestrator step.
- **Done — script gates:** `xui_layout_check.py --selftest`, `ui_draw_lint.py --selftest`, `kit_lint.py --all`, `director_parity.py --groups light_group`, and `director_kit_gen.py --check` passed. Kit lint reported only the three pre-existing pending-WP warnings.
- **Compile risks:** no build or compiler was run by request. The orchestrator should check the new ALKit header dependencies, `LLHandle<LLView>` conversions, aggregate initialization of audit records, and `LLUICtrl::onCommit()` visibility with `cl /Zs`; PowerShell parsing passed.
