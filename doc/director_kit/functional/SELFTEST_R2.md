# Functional self-test Round 2 gates

Round 2 treats only an observable, user-reachable effect as a pass. A control is first revealed (page, mode/card, selector state, and scroll position), then its screen-centre point must resolve through the root view to the control or one of its children. Hidden, parent-disabled, or covered controls fail. Setting comparisons use an epsilon no larger than `max(1e-9, abs(expected)*1e-7, increment*1e-4)`. Commit-only rows inspect the control's pre-existing commit signal before input and require an owner slot; the runner never adds a slot. Named callbacks must also still exist in `CommitCallbackRegistry`. `UNWIRED` rows remain failing findings.

## Exact old-code failures

| 10/10 failure class | Round 2 assertion that fails on the old code |
|---|---|
| Selector `setLLSD` | Each `kc_lights` and `kc_per_light_rim` segment is first moved to a different segment, clicked through the root hit-test, then must set `DirectorV2SelectedLight` to the manifest value **and** expose the selected light's `ALCineLightRigParamBlob` EV/rim-gain value. The old selector does not change the setting, so both the setting and consumer-selection checks fail. |
| Dead switch / sticky pressed | For `ALKitSwitch`, the bound setting, `LLUICtrl::getValue()`, and `ALKitSwitch::isOn()` (the exact state used by `draw()` for knob position/palette) must agree after mouse-up. Neither the switch nor its child button may retain mouse capture, and `ALKitSwitch::isFocusRingVisible()` plus `LLButton::isFocusRingVisible()` must both be false after a pointer click. The old dead/stale switch fails value-vs-drawn-state; a sticky press/focus indication fails the capture/focus-ring checks. |
| Rim needs rig | Before exercising the Rim master, the selected Light Rig slot is enabled as a restored test prerequisite. After the click, `ALCineRigRim::isEnabled()` must equal the manifest result; this predicate includes `ALCineLightRigManager::enabledMask() != 0`. The old code that changes only `CineRigRimEnabled`, or omits the Light Rig gate, fails the renderer-active consumer assertion. Rim presets additionally require the renderer bind-gain value (finite, clamped `CineRigRimMasterGain`) to reach the manifest value. |
| Weather/rain gate | The self-test-owned CPU consumer mirrors the pipeline gate: effective intensity is zero unless both `AlchemyWeatherEnabled` and `AlchemyWeatherRainEnabled` are true; otherwise it is the finite clamped rain intensity multiplied by the same EEP cloud/coupling factor. Weather master, Rain enable, and Rain intensity rows must change this consumer and reach the expected zero/nonzero/value result. Old code that changes a UI setting without affecting the rain-pass gate fails the consumer assertion. |
| Unbound legacy page | Every click/toggle must be reachable through the root hit-test. Before input, a commit-only control must have at least one pre-existing owner slot; if it names a registry callback, that name must resolve. The runner adds no callback of its own, and a declared modal must additionally open. Therefore a legacy or V2 control with no owner handler cannot pass merely because `onCommit()` was invoked, and the old unbound `group_run_combo` fails with `no observable effect`. |

## Other gate rules

- `EXPECT_MISSING` passes only after its page opens successfully.
- A disabled control fails unless its manifest row supplies a specific `skip_reason`; `SKIP`, `DEFER_INWORLD`, and `NO_EXERCISE` likewise require an explicit reason.
- File-picker, destructive, logout, and delete actions are never clicked. Declared modal/color-picker actions must open a new floater, which the runner then closes.
- Per-entry restoration restores both the saved setting layer and any pre-existing unsaved active override.
- `scripts/perf/ui_selftest.ps1 -Functional -Both` runs v1 then v2 and writes one combined CSV, allowing `functional_report.py` to emit the in-run v1/v2 diff.
