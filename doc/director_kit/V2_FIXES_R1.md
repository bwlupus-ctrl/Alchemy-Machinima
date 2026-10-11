# Director v2 functional fixes — round 1

Scope: `DirectorUIV2=true`. The v1 layouts and owners are unchanged. No build was run.

## 1. D1 — typed kit setting writes and selector cards

- Root cause: `ALKitCard` sent the S32 `DirectorV2SelectedLight` through `LLControlGroup::setLLSD`, which accepts only `TYPE_LLSD` controls (`indra/newview/alkitcard.cpp:438,463,847`; rejection contract in `indra/llxml/llcontrol.h:267-278`).
- Fix: `alKitSetTypedControl()` resolves the `LLControlVariable`, converts LLSD to its declared S32/U32/F32/Boolean/String/LLSD type, then calls `LLControlVariable::set()` (`indra/newview/alkitbinding.cpp:46`). `ALKitSettingBinding::set()` uses the same path (`:97`).
- Changed call sites: selector segment commit (`alkitcard.cpp:438`), selector combo commit (`:463`), and search/reveal selection (`:847`). The only remaining kit `setLLSD` writes `DirectorV2PageState`, which is declared LLSD.
- Self-test: click Key/Fill/Back/BG on both Lights and Rim; assert `DirectorV2SelectedLight` becomes 0/1/2/3, exactly the matching `ALKitOption` and segment are active, and the edited per-light setting is the renderer value consumed by the selected rig slot.

## 2. D2 / bug 1 — switch and button state colours

- Root cause: `ALKitSwitch::draw()` treated retained pointer focus as amber active focus, while kit buttons retained their strong hover tint after mouse-up (`indra/newview/alkitswitch.cpp:68`; `indra/llui/llbutton.cpp:551`). This visually combined Off/value state with On/pressed colour.
- Fix: switch geometry and palette now come from one sampled current value each draw; the focus ring is armed by keyboard focus and suppressed by pointer focus. Mouse-up clears the switch highlight. Kit-styled `LLButton`s opt into clearing transient hover/pressed tint on mouse-up/capture loss, without changing unstyled/v1 buttons (`llbutton.cpp:551,1312`; `alkitstyle.cpp:79`). Selector selection remains derived from `DirectorV2SelectedLight`.
- Self-test: mouse-click and Tab-toggle every kit switch and a selector segment; assert widget value equals its setting, knob endpoint/track palette equals that value, pointer click leaves no focus ring, Tab focus does, and only the selected option uses selected tint.

## 3. Rim master and presets — runtime break/gate

- Root cause: the master and provider chains were intact and type-correct. The renderer additionally requires at least one enabled Light Rig slot (`indra/newview/alcinerigrim.cpp:112-120`), while presets intentionally change rim parameters but not either master (`indra/newview/alrigrimcontroller.cpp:283-312`). Thus both actions could succeed but remain invisible with the rig off.
- Fix: D1 repairs the shared per-light selector; the Rim page now shows `Light Rig is off: rim has no effect` plus `Turn on` while `enabledMask()==0` (`indra/newview/alkitpage.cpp:542,956`). The action sets `CineLightRigEnabled`, the same setting captured into the selected slot.
- Self-test: with rig off, assert the banner is visible; press Turn on and assert `CineLightRigEnabled=true`, selected-slot `mEnabled=true`, and `enabledMask()!=0`. Toggle Rim and assert `CineRigRimEnabled` plus `ALCineRigRim::isEnabled()`/`gGL` rig-rim active state. Apply a preset and assert a sentinel such as `CineRigRimMasterGain` plus the matching bind-state gain.

## 4. D3 — local hidden-gate status

- Root cause: valid subordinate writes were visually suppressed by runtime gates that were not named beside the v2 controls.
- Fix: v2-only live banners were added by `ALKitPage`: Rim uses the real `enabledMask()` consumer gate; Weather reports unavailable while `LLPipeline::sRenderDeferred` is false; Atmosphere and Rig report `Light masking is on: projector volumetrics hidden` while `ALCineLightRigManager::anyActorOnly()` is true (`indra/newview/alkitpage.cpp:542-546,956-969`). Banner height is deducted before sizing the existing scroller/legacy host.
- Self-test: flip each gate and assert both its source runtime value and banner visibility; for masking, assert an actor-only light is skipped at `pipeline.cpp:20424-20428/21275-21279`; for Weather, assert deferred availability and the shared panel enabled state agree.

## 5. Bugs 2/5 — empty kit spinners

- Root cause: spinner XUI supplied kit text tokens, but `LLSpinCtrl` applies them only to its optional label; the embedded `LLLineEditor` retained the skin default foreground against the kit input background.
- Fix: `ALKitRow::styleValueEditor()` now applies kit foreground, disabled/read-only and tentative colours to every number/slider editor (`indra/newview/alkitrow.cpp:603-605`). This covers EV, Bounce Ratio and all kit spinners.
- Self-test: set `CineLightRigKeyEV` and `CineLightRigBounceRatio`; assert settings and rig-consumer values change, and the visible editor text equals the configured precision (`%.2f`) in enabled, disabled and tentative states.

## 6. Bug 3 — Eye catchlight

- Root cause: no separate static wiring break exists. The v2 card header creates `cine_catchlight` from `enable_setting="CineLightRigCatchlight"` (`panel_dpage_light_cine_light_rig.xml:562`), and `ALCineLightRig::tickSelected()` reads that exact setting (`indra/newview/alcinelightrig.cpp:2298-2305,2397-2401`). The misleading retained focus/value rendering and the Light Rig master gate made a successful toggle look dead.
- Fix: the switch state fix makes the header toggle unambiguous, and the existing page-master off banner exposes/turns on `CineLightRigEnabled`. No duplicate callback or alias setting was added.
- Self-test: with an avatar target and rig on, toggle the card switch; assert `CineLightRigCatchlight`, the `catchlight_enabled` consumer value, catchlight emitter UUID/existence, intensity from `CineLightRigCatchlightEV`, and immediate destruction on Off. Object targets remain intentionally unsupported (`alcinelightrig.cpp:2727-2729`).

## 7. Reported hard v2 Flycam losses

- Verification: both controls are present in v2 through Shot's injected `panel_cinecam_params.xml` (`panel_dpage_camera_shot.xml:39`; controls at `panel_cinecam_params.xml:180-202`). `FlycamOperatorSimulationHz` drives the fixed-step operator path (`llcameraoperator.cpp:591,689`); `FlycamOperatorModeBlendTime` is read by the blend consumer (`:864`). They are relocated, not unwired, so no code/XUI change was made.
- Self-test: reveal Camera > Shot, commit both spinners, assert the F32 settings change and the operator's simulation step/blend duration use the committed values; restore both.

## Validation

- `git diff --check`: clean.
- Required Python layout/draw checks could not start because both `python`/`py` are absent and the workspace `.venv` executable is denied by the sandbox. No checker or lint rule was relaxed.
- No build, commit, fetch or upstream operation was performed. Concurrent edits to `indra/newview/aluiperfselftest.cpp` and `scripts/perf/ui_selftest.ps1` were not touched.

## Round 2

### R2.1 — empty EV/Ratio spinners

- Root cause: R1's foreground writes were redundant. `ALKitStyle::styleTree()` already styles the embedded editor. The kit-only `LLSpinCtrl::setValueColumnLayout()` instead moved that editor with `setRect()`; unlike `LLLineEditor::reshape()`, `setRect()` does not refresh `mTextRightEdge`. A spinner created without an XUI width could therefore retain an approximately zero right edge and clip its right-aligned value.
- Fix: removed the redundant row colour writes. `setValueColumnLayout()` now clamps the editor width, calls `reshape()` (which runs `updateTextPadding()`), then sets its origin. This helper is called only by kit rows; stock spinner construction/layout and `LLLineEditor` are unchanged.
- Self-test assertion: every visible kit spinner has a positive editor width/right text edge and non-empty rendered value; explicitly cover Key EV and Bounce Ratio at their configured precision.

### R2.2 — typed resets and selector safety

- Root cause: the R1 typed helper rejected non-scalar control types, used `LLSD::asBoolean()` for strings, and allowed undefined values to coerce into scalar zero. Selector synchronization also hid every option when a stored value matched none.
- Fix: undefined values are rejected; Boolean strings use `LLStringUtil::convertToBOOL()` (so `"false"` and `"0"` are false); Color3/Color4/Vec3/Rect/Quat and other declared types fall through to `LLControlVariable::set(value)`. Invalid selector values preserve the last valid (initially first) option instead of blanking the card.
- Self-test assertion: reset both named tint rows to their defaults; write Boolean strings `false`, `0`, and `true`; prove undefined leaves a sentinel Key value unchanged; inject an invalid selector and assert exactly one option remains visible.

### R2.3 — keyboard-only switch/button focus

- Root cause: focus-received events do not identify input origin, and floater focus restoration can replay them after a pointer click. Kit selector buttons also retained the stock always-on focus border.
- Fix: kit styling opts `LLButton` into a keyboard-only border. Button key handlers and successful Tab traversal arm it; pointer down clears it. Stock buttons retain the old `hasFocus() && mDrawFocusBorder` behavior. `ALKitSwitch` uses its hidden button's same state, and both classes expose public const `isFocusRingVisible()` getters matching their draw predicates.
- Self-test assertion: pointer-click a switch/segment, leave the floater, then click its background and assert both getters stay false; Tab/arrow/space focus asserts true while focused, then false after pointer down.

### R2.4 — transient button and switch drawing

- Root cause: R1's mouse-up highlight reset was overwritten by normal hover processing on the next frame, and the switch's mouse-up callback changed a hidden child that its custom draw never rendered. The switch focus shape drew two complete circles, while disabled colours used translucent compositing over page art.
- Fix: removed both ineffective resets. Existing capture release clears pressed state, mouse leave clears hover state, and normal hover remains visible. The focus indicator is now the outer capsule outline only. Disabled track/knob use opaque kit `INPUT_BG`/`FAINT` tokens.
- Self-test assertion: pressed state clears on release, hover persists only while inside and clears on leave; focused Off has one capsule outline and one knob; disabled pixels equal the opaque tokens independent of page background.

### R2.5 — actionable Weather gate

- Root cause: deferred rendering is forced on, so R1's `!sRenderDeferred` banner was unreachable; the actual no-effect state was `AlchemyWeatherEnabled=false`.
- Fix: the Weather-page banner now says `Weather is off`, appears only for that setting, and provides `Turn on` to set it true.
- Self-test assertion: force Weather off, assert banner/text/action visibility, invoke Turn on, and assert the setting becomes true and the banner disappears with one layout change.

### R2.6 — accurate masking count

- Root cause: R1 gated on any actor-only light, although the pipeline suppresses volumetrics only for an actor-only projector whose own/root UUID is in the session volumetric set.
- Fix: `LLPipeline::actorOnlyVolumetricLightCount()` applies that same actor-only plus own/root volumetric predicate over active spotlight slots. Atmosphere/Rig show `Light masking is on: projector volumetrics hidden for N light(s)` only when `N > 0`.
- Self-test assertion: actor-only alone and volumetric-only alone produce zero/no banner; their intersection produces the exact count and text, including root-ID volumetric enablement.

### R2.7 — per-frame draw gate

- Root cause: R1 updated banner state/text and looked up the Path playback panel from code reachable via `draw()`/`reshape()` without sufficient caching/change gates.
- Fix: Path playback is cached once during build. Gate text uses `setTextIfChanged()`; visibility reports whether it changed, and only that transition triggers reshape from `draw()`.
- Self-test assertion: stable repeated draws perform no banner text/visibility write, child lookup, or reshape; one gate transition performs one layout update. `ui_draw_lint --changed d812416ca65` is the static assertion when Python is available.

Round 2 validation: `git diff --check` is clean. The Python draw-lint runner remains unavailable because the workspace virtualenv executable is access-denied. No build, commit, fetch, or upstream operation was performed, and no functional-self-test-owned file was touched by this job.
