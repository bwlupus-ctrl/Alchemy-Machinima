# Shared hook map

Local base `7c11d3f38bd` only; current upstream may have drifted. HEAD line ranges include three context lines.

Grouped by shared file, ordered by earliest candidate landing slice. Full patches and evidence: [hooks.json](hooks.json).
Guards are unknown unless manually proven. Conditional evidence is not a proof that the whole hunk is guarded.

## Slice 1: `indra/llui/CMakeLists.txt`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 4–10 (n=7) | unknown/non-function (add_library (llui STATIC)) | common-ui | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 71–78 (n=8) | unknown/non-function (target_sources(llui) | common-ui | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 92–98 (n=7) | unknown/non-function (target_sources(llui) | common-ui | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 117–123 (n=7) | unknown/non-function (target_sources(llui) | common-ui | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 193–200 (n=8) | unknown/non-function (target_sources(llui) | common-ui | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 222–228 (n=7) | unknown/non-function (target_sources(llui) | common-ui | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 306–312 (n=7) | unknown/non-function (if(BUILD_TESTING)) | common-ui | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 1: `indra/llui/llfloater.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 58–65 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | common-ui | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 300–306 (n=7) | unknown/non-function (LLFloater::LLFloater(const LLSD& key, const LLFloater::Params& p)) | common-ui | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 720–735 (n=16) | LLFloater::openFloater | common-ui | creative-candidate | unknown; conditional evidence present | Edits LLViewerEventRecorder |
| 769–791 (n=23) | LLFloater::closeFloater, LLFloater::openFloater | common-ui | creative-candidate | unknown; conditional evidence present | Comment evidence: [UIFitNoise] Retry on a later open if layout animation made this pass incomplete. |
| 857–889 (n=33) | LLFloater::closeFloater | common-ui | creative-candidate | unknown; conditional evidence present | Comment evidence: [SliderWheel] design v7 4.2/4.2a: flush pending slider wheel settles (the single final mouse-up of a wheel gesture) while the whole floater, derived class included, is still alive: canClose() passed, dependents are closed, onClose has NOT run, nothing is hidden or destroyed. The  |
| 2180–2209 (n=30) | LLFloater::draw, LLFloater::profileZoneName | unknown | unknown-review | unknown; conditional evidence present | Edits LLFloater, LL_PROFILE_ZONE_TEXT, LL_PROFILER_CATEGORY_ENABLE_UI, LL_PROFILE_ZONE_TRANSIENT |
| 2311–2328 (n=18) | LLFloater::updateTransparency | unknown | unknown-review | unknown; conditional evidence present | Edits LLUICtrl, LLView |
| 3257–3263 (n=7) | LLFloaterView::draw | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |

## Slice 1: `indra/llui/llfloater.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 374–381 (n=8) | getLifecycleSerial | common-ui | creative-candidate | unknown; no local conditional evidence | Comment evidence: [SliderWheel] see mLifecycleSerial |
| 499–508 (n=10) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 557–563 (n=7) | unknown/non-function (private:) | common-ui | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 567–574 (n=8) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 578–589 (n=12) | unknown/non-function (private:) | common-ui | creative-candidate | unknown; no local conditional evidence | Comment evidence: [SliderWheel] design v7 4.2a: close-flush re-entrancy guard. mLifecycleSerial is bumped by openFloater and by every closeFloater that reaches the flush point, so a flush callback that reopens or re-closes this floater is detected by the outer closeFloater. |
| 696–698 (n=3) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 1: `indra/llui/llscrollbar.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 35–41 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | common-ui | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 135–155 (n=21) | LLScrollbar::setStyle | unknown | unknown-review | unknown; no local conditional evidence | Edits LLScrollbar, LLUIImage, LLUIColor |
| 433–441 (n=9) | LLScrollbar::scrollByWheel | common-ui | creative-candidate | unknown; no local conditional evidence | Comment evidence: [SliderWheel] design 1.1: a real line change arms the slider-wheel scroll-through guard for the owning scroller. The sub-notch "handled, no line" returns below do not stamp. |

## Slice 1: `indra/llui/llscrollcontainer.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 32–43 (n=12) | unknown/non-function (declaration, XML, build list or global scope) | common-ui | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 272–290 (n=19) | LLScrollContainer::handleScrollWheel | common-ui | creative-candidate | unknown; conditional evidence present | Comment evidence: [SliderWheel] design 1.3 disabled-slider hook. Dispatch skips disabled children, so a DISABLED participating slider under the cursor never saw the event, but the retired ALScrollFit::WheelGuard did (childFromPoint checks visibility only). Under an armed guard, keep it alive and s |
| 322–340 (n=19) | LLScrollContainer::handleScrollHWheel | common-ui | creative-candidate | unknown; conditional evidence present | Comment evidence: [SliderWheel] design 1.3 disabled-slider hook, horizontal wheel: a guarded H-wheel over a participating (necessarily disabled, an enabled one already handled it) slider is SWALLOWED and keeps the guard alive, exactly what WheelGuard did. |
| 512–528 (n=17) | LLScrollContainer::profileZoneName | unknown | unknown-review | unknown; conditional evidence present | Edits LLScrollContainer, LLFloater |
| 567–577 (n=11) | LLScrollContainer::draw | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Tracy value for the existing LLScrollContainer::draw zone: this makes dense scroll documents stand out without changing culling. |
| 587–595 (n=9) | LLScrollContainer::draw | unknown | unknown-review | unknown; conditional evidence present | Edits LL_PROFILER_CATEGORY_ENABLE_UI, LL_PROFILE_ZONE_TRANSIENT |
| 866–877 (n=12) | LLScrollContainer::setDocPosVertical | unknown | unknown-review | unknown; no local conditional evidence | Edits LLScrollContainer |

## Slice 1: `indra/llui/llslider.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 24–41 (n=18) | unknown/non-function (declaration, XML, build list or global scope) | common-ui, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge C8] See llslider.h for the slider precision override note. donor: Black Dragon (NiranV Dean) 94c47d42de. |
| 50–57 (n=8) | unknown/non-function (LLSlider::Params::Params()) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLColor4 |
| 60–87 (n=28) | unknown/non-function (LLSlider::Params::Params()) | common-ui, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: [SliderWheel] provided true/false = explicit ON/OFF, not provided = UNSET (design 2.1) |
| 96–111 (n=16) | LLSlider::LLSlider | sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: [EnvIntensity v2] fine-step sliders keep a saved fine value (e.g. 0.01 EV): the coarse snap below would otherwise round it and write it back. |
| 119–157 (n=39) | LLSlider::setValue, LLSlider::~LLSlider | common-ui | creative-candidate | unknown; conditional evidence present | Comment evidence: [SliderWheel] destruction only CANCELS a pending wheel settle (never fires from a dtor); the registry's dead handle drops the entry. Round to nearest increment (bias towards rounding down) [BDMerge C8] Allow callers (typed text-field commits) to bypass increment rounding. Drag/ke |
| 161–167 (n=7) | LLSlider::updateThumbRect | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 191–262 (n=72) | LLSlider::setValue, LLSlider::snapToFineGrid, LLSlider::steppedValue, LLSlider::valueFromFraction, LLSlider::valueToFraction | sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: [EnvIntensity v2] Fine-grid snap for wheel_fine_div sliders: clamp, snap to mMinValue + n * (increment / div), and pin the ends exactly (a 0.002 grid would otherwise land on 0.99999994 instead of the 1.0 maximum). Only the Shift-wheel path and incoming LLSD values (control-variab |
| 283–289 (n=7) | LLSlider::handleHover | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 295–301 (n=7) | LLSlider::handleHover | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 314–323 (n=10) | LLSlider::handleMouseUp | common-ui | creative-candidate | unknown; no local conditional evidence | Comment evidence: [SliderWheel] a real mouse-up reports for this gesture: cancel any pending wheel settle BEFORE the signal runs (design 4.2). The uncaptured branch does not invalidate. |
| 363–371 (n=9) | LLSlider::handleMouseDown | common-ui | creative-candidate | unknown; no local conditional evidence | Comment evidence: [SliderWheel] a drag starts and its real mouse-up will report: cancel a pending wheel settle. (The Ctrl+click branch above takes no capture and leaves a pending settle alone.) |
| 382–393 (n=12) | LLSlider::handleKeyHere | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 396–625 (n=230) | LLSlider::applyWheelValue, LLSlider::fireWheelSettle, LLSlider::handleScrollWheel, LLSlider::handleScrollWheelStock, LLSlider::hwheelEnabled, LLSlider::participates, LLSlider::scheduleWheelSettle, LLSlider::wheelEnabled, attachedSlider | common-ui, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: [SliderWheel] design v7 2.2/2.3: resolved wheel state. [SliderWheel] design 4.1. Everything after a foreign call goes through alive(h), never 'this'. review P1-2: the slider re-resolved through its handle AND still attached where it was when the wheel notch started: same live par |
| 628–671 (n=44) | LLSlider::handleScrollHWheel | common-ui | creative-candidate | unknown; conditional evidence present | Comment evidence: [SliderWheel] explicit OFF declines the horizontal wheel too (design 2.2). Not participating (UNSET, switch off) or a vertical slider: today's code verbatim. [SliderWheel] participating horizontal slider. design 1.3: guarded H-wheel is SWALLOWED (as WheelGuard did) and keeps the  |
| 709–717 (n=9) | LLSlider::draw | unknown | unknown-review | unknown; no local conditional evidence | Edits LLColor4 |

## Slice 1: `indra/llui/llslider.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 24–44 (n=21) | unknown/non-function (declaration, XML, build list or global scope) | common-ui | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge C8] Slider precision override: allow LLSlider::setValue() to skip increment-clamping for a caller-supplied value, so a value typed into the owning LLSliderCtrl's text editor can land between increments instead of being snapped. donor: Black Dragon (NiranV Dean) 94c47d42d |
| 50–58 (n=9) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 65–93 (n=29) | unknown/non-function (public:) | common-ui, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: [AL] Opt-in: let the ordinary (vertical) scroll wheel step a HORIZONTAL slider by its increment while hovered, for precision adjustment. Off by default so sliders inside scroll containers keep routing the wheel to the container. Vertical sliders already take the vertical wheel. [ |
| 96–107 (n=12) | unknown/non-function (protected:) | sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge C8] precision_override skips increment-rounding below; see donor note above. [EnvIntensity v2] out of line: wheel_fine_div sliders keep incoming fine values (control sync); others are exactly setValue((F32)v, true). |
| 119–163 (n=45) | cancelWheelSettle, getThumbRect, getWheelAdjustState, getWheelFineDiv, getWheelSettleGen, isWheelSettlePending, setWheelAdjust | common-ui, looks, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: Exposed for input-test drivers that must begin a synthetic drag on the same thumb pixels a user would grab. [EnvIntensity v2] accessor for LLSliderCtrl's gated wheel forwarding [SliderWheel] Runtime override of the wheel_adjust param = explicit ON / OFF (replaces the [LightboxWhe |
| 172–190 (n=19) | unknown/non-function (private:) | common-ui, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: [AL] hovered vertical wheel steps a horizontal slider. [SliderWheel] tri-state: UNSET (param not provided), ON, OFF (design 2.1). Not const: setWheelAdjust() makes it explicit ON/OFF. |

## Slice 1: `indra/llui/llsliderctrl.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 24–32 (n=9) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge C8] See llsliderctrl.h / llslider.h for the slider precision override note. donor: Black Dragon (NiranV Dean) 94c47d42de. |
| 37–43 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | common-ui | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 57–64 (n=8) | unknown/non-function (LLSliderCtrl::LLSliderCtrl(const LLSliderCtrl::Params& p)) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 107–115 (n=9) | LLSliderCtrl::LLSliderCtrl | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 151–175 (n=25) | LLSliderCtrl::LLSliderCtrl | common-ui, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: [SliderWheel] design 2.1: copy ONLY a wheel_adjust the ctrl was actually given. Copying the ctrl's default (false) would mark the child provided = explicit OFF and defeat the global default; an unprovided ctrl leaves the child UNSET. |
| 238–319 (n=82) | LLSliderCtrl::handleScrollWheel, LLSliderCtrl::liveSlider, LLSliderCtrl::setValue, cancelWheelSettleOf | common-ui, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: [SliderWheel] design v7 4.2b. [SliderWheel] RS(): after every foreign call where execution continues, re-resolve the ctrl AND the child; return before any further access if either failed. Needs locals h, sh, self, s. review P1-2: when ctrl/child re-resolution fails, the ORIGINAL  |
| 358–364 (n=7) | LLSliderCtrl::updateText | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_NAMED_CATEGORY_UI, LLSliderCtrl |
| 368–376 (n=9) | LLSliderCtrl::updateText | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 431–443 (n=13) | LLSliderCtrl::onEditorCommit | common-ui | creative-candidate | unknown; no local conditional evidence | Comment evidence: [SliderWheel] design v7 4.2b: handles for the ctrl AND its child BEFORE the first foreign call (setValue's control write, the validate signal, onCommit, the editor signal). After each one SLIDERWHEEL_RS() re-resolves both and returns if either is gone. |
| 447–461 (n=15) | LLSliderCtrl::onEditorCommit | common-ui | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge C8] Typed values may land between increments when gated on. |
| 464–492 (n=29) | LLSliderCtrl::onEditorCommit | common-ui | creative-candidate | unknown; conditional evidence present | Comment evidence: [SliderWheel] the typed value is the final commit: cancel a pending wheel settle FIRST, before any callback (s is fresh from the RS() above). |
| 496–537 (n=42) | LLSliderCtrl::onSliderCommit | common-ui | creative-candidate | unknown; no local conditional evidence | Comment evidence: [SliderWheel] design v7 4.2b (see onEditorCommit): handles first, RS() after every foreign call. |
| 631–633 (n=3) | unknown/non-function (void LLSliderCtrl::reportInvalidData()) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 1: `indra/llui/llsliderctrl.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 24–35 (n=12) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge C8] setValue() gains a precision_override parameter threaded down to LLSlider::setValue(); onEditorCommit() (llsliderctrl.cpp) passes the live BDMergeSliderPrecision setting so a typed text-field value can land between increments. donor: Black Dragon (NiranV Dean) 94c47d |
| 51–69 (n=19) | unknown/non-function (public:) | common-ui, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: [AL] passthrough to LLSlider::Params::wheel_adjust (hover wheel steps a horizontal slider by its increment); off by default. [EnvIntensity v2] passthrough to LLSlider::Params::wheel_fine_div (Shift = increment / div; fine snapping grid); 0 = off. [SliderWheelStep] passthrough to  |
| 81–94 (n=14) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 107–123 (n=17) | isWheelSettlePending | sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge C8] precision_override forwarded to LLSlider::setValue(); see donor note above. virtual*/ void    setValue(const LLSD& value) { setValue((F32)value.asReal(), true); } [EnvIntensity v2] out of line: routes through LLSlider::setValue(const LLSD&) so wheel_fine_div sliders  |
| 155–171 (n=17) | unknown/non-function (public:) | common-ui, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: [EnvIntensity v2] Only for sliders with wheel_adjust + wheel_fine_div: forwards the wheel from anywhere over the control (label / value text) to the slider bar. Every other slider takes the base-class path. virtual*/ bool    handleScrollWheel(S32 x, S32 y, LLScrollDelta delta); [ |
| 193–200 (n=8) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 212–214 (n=3) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 1: `indra/llui/llspinctrl.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 152–173 (n=22) | LLSpinCtrl::setValueColumnLayout | unknown | unknown-review | unknown; no local conditional evidence | Edits LLSpinCtrl, LLRect |
| 185–213 (n=29) | LLSpinCtrl::constrainValue, LLSpinCtrl::onUpBtn | common-ui | creative-candidate | unknown; conditional evidence present | Comment evidence: Keep either endpoint when entered exactly; crossing it wraps with the overshoot intact (180 + 1 -> -179, -180 - 1 -> 179). |
| 228–234 (n=7) | LLSpinCtrl::onUpBtn | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 250–256 (n=7) | LLSpinCtrl::onDownBtn | common-ui | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 270–276 (n=7) | LLSpinCtrl::onDownBtn | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 294–300 (n=7) | LLSpinCtrl::onEditorGainFocus | common-ui | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 305–315 (n=11) | LLSpinCtrl::onEditorLostFocus | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 374–385 (n=12) | LLSpinCtrl::updateEditor | unknown | unknown-review | unknown; conditional evidence present | Edits LL_PROFILE_ZONE_NAMED_CATEGORY_UI, LLSpinCtrl, LLStringExplicit |
| 406–412 (n=7) | LLSpinCtrl::onEditorCommit | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 455–474 (n=20) | LLSpinCtrl::setMixedValue | common-ui | creative-candidate | unknown; conditional evidence present | Comment evidence: A detail refresh must not replace text being edited, or make the next Up/Down discard that text when it clears mixed state. Mixed is never entered while focused (returned above), so a focused editor clearing mixed still shows only the placeholder: replace it with the number so th |
| 485–491 (n=7) | LLSpinCtrl::onCommit | common-ui | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 580–582 (n=3) | unknown/non-function (bool LLSpinCtrl::handleKeyHere(KEY key, MASK mask)) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 1: `indra/llui/llspinctrl.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 65–73 (n=9) | hasBeenSet | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: False until the first setValue()/forceSetValue(), and again after clear(): an unset spinner writes even an equal value (see setValue()). |
| 78–97 (n=20) | getPrecision, getWrap, isMixedValue, setWrap | common-ui | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 111–117 (n=7) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 128–135 (n=8) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 1: `indra/newview/CMakeLists.txt`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 118–145 (n=28) | unknown/non-function (execute_process() | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 170–182 (n=13) | unknown/non-function (endif ()) | actor-path, environment | mixed/perf-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 186–237 (n=52) | unknown/non-function (set(viewer_SOURCE_FILES) | actor-bar, actor-path, camera, common-ui, director-shell, rig, sensor, shafts-volumetrics, vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 240–250 (n=11) | unknown/non-function (set(viewer_SOURCE_FILES) | camera, stepped-animation | mixed/perf-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 272–295 (n=24) | unknown/non-function (set(viewer_SOURCE_FILES) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 332–446 (n=115) | unknown/non-function (set(viewer_SOURCE_FILES) | actor-bar, actor-path, camera, capture-sidecar, clones, common-ui, composition, crowd, director-shell, gaze, looks, rig, roto-tron, sensor, shafts-volumetrics, stepped-animation, tracking, vcam, weather | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 522–528 (n=7) | unknown/non-function (set(viewer_SOURCE_FILES) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 551–557 (n=7) | unknown/non-function (set(viewer_SOURCE_FILES) | environment | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 649–655 (n=7) | unknown/non-function (set(viewer_SOURCE_FILES) | environment | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 660–666 (n=7) | unknown/non-function (set(viewer_SOURCE_FILES) | clones | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 816–823 (n=8) | unknown/non-function (set(viewer_SOURCE_FILES) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 892–902 (n=11) | unknown/non-function (set(viewer_SOURCE_FILES) | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 906–912 (n=7) | unknown/non-function (set(viewer_SOURCE_FILES) | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1099–1105 (n=7) | unknown/non-function (set(viewer_SOURCE_FILES) | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1118–1130 (n=13) | unknown/non-function (set(viewer_SOURCE_FILES) | actor-path, environment | mixed/perf-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1133–1186 (n=54) | unknown/non-function (set(viewer_HEADER_FILES) | actor-bar, actor-path, camera, common-ui, director-shell, rig, sensor, shafts-volumetrics, vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1189–1201 (n=13) | unknown/non-function (set(viewer_HEADER_FILES) | camera, stepped-animation | mixed/perf-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1226–1245 (n=20) | unknown/non-function (set(viewer_HEADER_FILES) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1283–1444 (n=162) | unknown/non-function (set(viewer_HEADER_FILES) | actor-bar, actor-path, camera, capture-sidecar, clones, common-ui, composition, crowd, director-shell, gaze, looks, rig, roto-tron, sensor, shafts-volumetrics, stepped-animation, tracking, vcam, weather | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1522–1528 (n=7) | unknown/non-function (set(viewer_HEADER_FILES) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1551–1557 (n=7) | unknown/non-function (set(viewer_HEADER_FILES) | environment | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1653–1659 (n=7) | unknown/non-function (set(viewer_HEADER_FILES) | environment | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1664–1670 (n=7) | unknown/non-function (set(viewer_HEADER_FILES) | clones | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1809–1816 (n=8) | unknown/non-function (set(viewer_HEADER_FILES) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1880–1892 (n=13) | unknown/non-function (set(viewer_HEADER_FILES) | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1896–1903 (n=8) | unknown/non-function (set(viewer_HEADER_FILES) | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 2095–2101 (n=7) | unknown/non-function (set(viewer_HEADER_FILES) | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 2335–2341 (n=7) | unknown/non-function (set_source_files_properties(${viewer_CHARACTER_FILES}) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 2615–2652 (n=38) | unknown/non-function (elseif(LINUX)) | unknown | unknown-review | unknown; conditional evidence present | Edits LL_GENERATOR_IS_MULTI_CONFIG |
| 2710–2716 (n=7) | unknown/non-function (if(IS_DIRECTORY ${LOCAL_DIST_DIR})) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 2750–2756 (n=7) | unknown/non-function (if (PACKAGE)) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 2839–2869 (n=31) | unknown/non-function (if (BUILD_TESTING)) | actor-bar, actor-path, camera, clones, common-ui, crowd, director-shell, looks, rig, sensor, shafts-volumetrics, vcam, weather | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 2876–2895 (n=20) | unknown/non-function (if (BUILD_TESTING)) | camera, vcam, weather | creative-candidate | unknown; no local conditional evidence | Edits LL_TEST_ADDITIONAL_SOURCE_FILES |
| 2944–3128 (n=185) | unknown/non-function (if (BUILD_TESTING)) | actor-bar, actor-path, camera, clones, composition, director-shell, gaze, rig, shafts-volumetrics, stepped-animation, tracking, vcam | mixed/perf-review | unknown; no local conditional evidence | Edits LL_ADD_INTEGRATION_TEST, ALGazeMath, ALTrajectory, ALGazeMotor, LLControlVariable, LLUI, gSavedSettings |
| 3190–3229 (n=40) | unknown/non-function (if (BUILD_TESTING)) | camera | creative-candidate | unknown; no local conditional evidence | Edits LL_ADD_INTEGRATION_TEST, LLControlGroup, LLDir, gDirUtilp, LLNotificationsUtil, LLNotifications |

## Slice 1: `indra/newview/app_settings/settings.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 2–3459 (n=3458) | unknown/non-function (declaration, XML, build list or global scope) | actor-path, camera, clones, crowd, director-shell, environment, gaze, looks, rig, roto-tron, sensor, stepped-animation, weather | creative-candidate | unknown; no local conditional evidence | Comment evidence: AvatarBakedTextureUploadTimeout is in use by QA--> deprecated (see SL-12429) --> |
| 3461–4504 (n=1044) | unknown/non-function (declaration, XML, build list or global scope) | camera, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: deprecated (see MAINT-8091) --> deprecated (see MAINT-8091) --> *HACK: On first run, set this to 0 for new users, |
| 4506–5344 (n=839) | unknown/non-function (declaration, XML, build list or global scope) | camera, director-shell | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 5346–5520 (n=175) | unknown/non-function (declaration, XML, build list or global scope) | camera, director-shell, roto-tron | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 5522–5553 (n=32) | unknown/non-function (declaration, XML, build list or global scope) | composition | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 5555–5575 (n=21) | unknown/non-function (declaration, XML, build list or global scope) | composition | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 5577–5762 (n=186) | unknown/non-function (declaration, XML, build list or global scope) | camera | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 5764–5784 (n=21) | unknown/non-function (declaration, XML, build list or global scope) | camera | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 5786–5795 (n=10) | unknown/non-function (declaration, XML, build list or global scope) | camera | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 5797–5817 (n=21) | unknown/non-function (declaration, XML, build list or global scope) | camera | creative-candidate | unknown; no local conditional evidence | Edits ALL |
| 5819–5883 (n=65) | unknown/non-function (declaration, XML, build list or global scope) | camera | creative-candidate | unknown; no local conditional evidence | Edits LL_ERRS |
| 5885–5905 (n=21) | unknown/non-function (declaration, XML, build list or global scope) | camera | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 5907–6554 (n=648) | unknown/non-function (declaration, XML, build list or global scope) | camera, roto-tron | creative-candidate | unknown; no local conditional evidence | Edits RenderAnisotropic, RenderAnisotropicLevel, RenderAppleUseMultGL, RenderAttachedLights, RenderAttachedParticles, RenderAvatarCloth, RenderCanUseGLTFPBROpaqueShaders, RenderCanUseTerrainBakeShaders, RenderClass1MemoryBandwidth, RenderCPUBasis |
| 6556–20682 (n=14127) | unknown/non-function (declaration, XML, build list or global scope) | actor-path, camera, capture-sidecar, clones, common-ui, composition, crowd, director-shell, environment, gaze, looks, rig, roto-tron, sensor, shafts-volumetrics, stepped-animation, vcam, weather | mixed/perf-review | unknown; no local conditional evidence | Comment evidence: [AutoAnimate] Learned performance settings. --> [TrueMirror] begin: DRIVE_TRUE_MIRROR settings --> [TrueMirror] end --> AvatarBakedTextureUploadTimeout is in use by QA--> deprecated (see SL-12429) --> deprecated (see MAINT-8091) --> deprecated (see MAINT-8091) --> *HACK: On first |
| 20782–20788 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 20862–20930 (n=69) | unknown/non-function (declaration, XML, build list or global scope) | rig | creative-candidate | unknown; no local conditional evidence | Edits RenderSpotShadowDetailBias |
| 21177–21193 (n=17) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderShadowPerspectiveWarp |
| 21226–21740 (n=515) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderShadowBlurSize, RenderShadowBlurSamples, RenderShadowBlurDistFactor, RenderShadowSoftenMode, RenderDeferredSSAO, RenderShadowSoftenPx, RenderShadowSoftenMM, RenderDynamicLOD, RenderFSAASamples, RenderFSAAType |
| 21742–21762 (n=21) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFarClip, RenderAutoMaskAlphaNonDeferred |
| 21764–21927 (n=164) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderAutoMaskAlphaDeferred, RenderInitError, RenderFlexTimeFactor, RenderLightRadius, RenderFogRatio, RenderHeroProbeResolution, RenderHeroProbeClearShaderCache, RenderHeroProbeDistance, RenderHeroProbeUpdateRate, RenderHeroProbeConservativeUpdateMultiplier |
| 21929–22765 (n=837) | unknown/non-function (declaration, XML, build list or global scope) | director-shell, looks, rig, roto-tron, sensor, vcam | creative-candidate | unknown; no local conditional evidence | Edits RenderGlowIterations, RenderNameShowTime, RenderGlowLumWeights, RenderObjectBump, RenderGlowMaxExtractAlpha, RenderQualityPerformance, RenderGlowMinLuminance, RenderGlowResolutionPow, RenderReflectionDetail, RenderTransparentWater |
| 22767–23277 (n=511) | unknown/non-function (declaration, XML, build list or global scope) | actor-path, crowd, looks, sensor | creative-candidate | unknown; no local conditional evidence | Edits RenderNameShowSelf, RenderTerrainScale, RenderTerrainPBREnabled, RenderTerrainPBRDetail, RenderNameShowTime, RenderTerrainPBRScale, RenderObjectBump, RenderTerrainPBRPlanarSampleCount, RenderQualityPerformance, RenderTerrainPBRTriplanarBlendFactor |
| 23279–23299 (n=21) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderTerrainPBRNormalsEnabled, RenderWaterMaterials, RenderTrackerBeacon, RenderWaterMipNormal |
| 23301–23387 (n=87) | unknown/non-function (declaration, XML, build list or global scope) | looks | creative-candidate | unknown; no local conditional evidence | Edits RenderTransparentWater, RenderWaterRefResolution, RenderTreeLODFactor, RenderParcelSelection, RenderUIInSnapshot, RenderShaderCacheEnabled, RenderBalanceInSnapshot, RenderShaderCacheVersion, RenderCASSharpness, RenderTonemapMix |
| 23389–23563 (n=175) | unknown/non-function (declaration, XML, build list or global scope) | actor-path, crowd | creative-candidate | unknown; no local conditional evidence | Edits RenderUIBuffer, RenderUnloadedAvatar, RenderUseAdvancedAtmospherics, RenderUseTriStrips, RenderUseFarClip, RenderAvatarFriendsOnly, RenderAvatarComplexityMode, RenderAvatarMaxNonImpostors, RenderAutoMuteRenderWeightLimit, RenderAvatarMaxComplexity |
| 23565–23574 (n=10) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderUseVAO |
| 23576–23585 (n=10) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderVBOMappingDisable |
| 23587–23717 (n=131) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderUseStreamVBO, RenderPreferStreamDraw, RenderVolumeLODFactor, RenderWater, LLMapDestination |
| 23719–23871 (n=153) | unknown/non-function (declaration, XML, build list or global scope) | looks | creative-candidate | unknown; no local conditional evidence | Edits RenderWaterMaterials, RenderWaterMipNormal, RenderWaterRefResolution, RenderParcelSelection, RenderShaderCacheEnabled, RenderShaderCacheVersion, RenderCASSharpness, RenderTonemapMix, RenderTonemapType, RenderHighPrecisionPostProcess |
| 23873–23904 (n=32) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 23906–23926 (n=21) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 23928–24014 (n=87) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 24016–24036 (n=21) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLMapDestination |
| 24038–24069 (n=32) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 24071–24091 (n=21) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 24093–24355 (n=263) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 24357–24546 (n=190) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 24548–24656 (n=109) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 24658–24954 (n=297) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 24956–25009 (n=54) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 25011–25152 (n=142) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 25154–25344 (n=191) | unknown/non-function (declaration, XML, build list or global scope) | unknown | mixed/perf-review | unknown; no local conditional evidence | Edits LLSD |
| 25346–25795 (n=450) | unknown/non-function (declaration, XML, build list or global scope) | unknown | mixed/perf-review | unknown; no local conditional evidence | Edits LLSD |
| 25797–26731 (n=935) | unknown/non-function (declaration, XML, build list or global scope) | common-ui, roto-tron, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: deprecated (see SL-12871) --> |
| 26733–26742 (n=10) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 26744–26753 (n=10) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 26755–26918 (n=164) | unknown/non-function (declaration, XML, build list or global scope) | sensor | creative-candidate | unknown; no local conditional evidence | Edits RenderOcclusionTimeout, RenderDelayVBUpdate |
| 26920–26951 (n=32) | unknown/non-function (declaration, XML, build list or global scope) | roto-tron | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 26953–26962 (n=10) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 26964–27072 (n=109) | unknown/non-function (declaration, XML, build list or global scope) | environment | creative-candidate | unknown; no local conditional evidence | Comment evidence: deprecated (see SL-12871) --> |
| 27074–27347 (n=274) | unknown/non-function (declaration, XML, build list or global scope) | camera | mixed/perf-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 27349–27358 (n=10) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 27360–27490 (n=131) | unknown/non-function (declaration, XML, build list or global scope) | environment | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 27492–27576 (n=85) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Settings below are for back compatibility only. 1.23 settings --> |
| 27578–27770 (n=193) | unknown/non-function (declaration, XML, build list or global scope) | camera | creative-candidate | unknown; no local conditional evidence | Comment evidence: End of back compatibility settings --> |
| 27772–27792 (n=21) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 27794–27891 (n=98) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 27893–27924 (n=32) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Settings below are for back compatibility only. 1.23 settings --> |
| 27926–28066 (n=141) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: End of back compatibility settings --> |
| 28068–28088 (n=21) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 28090–28187 (n=98) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 28189–28198 (n=10) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 28200–28231 (n=32) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 28233–28253 (n=21) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 28255–28286 (n=32) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLMediaControl |
| 28288–28341 (n=54) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 28343–28385 (n=43) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 28387–28396 (n=10) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 28398–28407 (n=10) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 28409–28429 (n=21) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 28431–28440 (n=10) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 28442–28451 (n=10) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 28453–30574 (n=2122) | unknown/non-function (declaration, XML, build list or global scope) | actor-path, capture-sidecar, crowd, director-shell, gaze, looks, roto-tron, sensor, vcam, weather | creative-candidate | unknown; no local conditional evidence | Comment evidence: <FSPoser> --> [WebIdle] built-in web page idle cost --> Viewer-native cinematic weather. Master gate defaults off. --> |
| 30576–31679 (n=1104) | unknown/non-function (declaration, XML, build list or global scope) | actor-path, camera, capture-sidecar, crowd, director-shell, environment, looks, rig, roto-tron, sensor, vcam | creative-candidate | unknown; no local conditional evidence | Comment evidence: </FSPoser> --> [TronT0] Tron effects package foundation: master enable, shared palette, [TronT1] Tron World: dark grade + world-space neon grid with travelling [TronT1] Rig Rim "Tron" palette tint (deferredUtil.glsl rigRimTerm() / [TronT2] Circuit traces: per-plane world-lattice  |
| 31681–31889 (n=209) | unknown/non-function (declaration, XML, build list or global scope) | clones, director-shell, weather | creative-candidate | unknown; no local conditional evidence | Edits RenderDisableVintageMode |
| 31891–31988 (n=98) | unknown/non-function (declaration, XML, build list or global scope) | looks, weather | creative-candidate | unknown; no local conditional evidence | Comment evidence: <FSPoser> --> </FSPoser> --> |

## Slice 1: `indra/newview/llappviewer.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–43 (n=19) | unknown/non-function (declaration, XML, build list or global scope) | camera, common-ui, composition, director-shell, looks, sensor, vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 72–104 (n=33) | unknown/non-function (declaration, XML, build list or global scope) | actor-bar, actor-path, camera, clones, common-ui, gaze, looks, rig, roto-tron, shafts-volumetrics, stepped-animation, tracking, vcam, weather | mixed/perf-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 154–160 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | director-shell | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 273–280 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 379–438 (n=60) | alCameraDispatchFinalizePresented, alCameraDispatchSampleFinal | camera | creative-candidate | unknown; conditional evidence present | Edits gCameraDispatchSerial, gCameraDispatchOwner, gCameraDispatchFlycamWrote, gCameraDispatchForeignSinceFlycamWrite, ALCameraDispatchSample, gCameraDispatchSamples, gCameraDispatchSampleStart, gCameraDispatchSampleCount, gFrameCount, LLViewerCamera |
| 558–573 (n=16) | idle_afk_check | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Functional self-test (in-world): the session is never allowed to go Away on its own -- not during the run, not during teardown, and not if the logout is blocked by a dialog. Independent of the AFKTimeout setting (and of an RLV allowidle restriction), which the run restores or can |
| 667–678 (n=12) | settings_to_globals | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge Batch4] Machinima High-LOD: honor a persisted-ON master toggle at startup (raised object-LOD ceiling, force-LOD-3, clamped-positive distance factor). |
| 774–780 (n=7) | LLAppViewer::LLAppViewer | unknown | unknown-review | unknown; no local conditional evidence | Edits gDirUtilp |
| 829–836 (n=8) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 872–879 (n=8) | LLAppViewer::init | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Unconditional: must precede error handling, configuration and all TLS use. |
| 913–926 (n=14) | LLAppViewer::init, getControl | stepped-animation | creative-candidate | unknown; conditional evidence present | Edits ALSteppedAnimRuntime, gSavedSettings, LLControlVariable, LLSD |
| 1417–1438 (n=22) | LLAppViewer::initMaxHeapSize | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge G5.0] 0 = auto-size from installed physical RAM. A fixed cap makes updateMemoryInfo() report available memory as min(true available, cap - private commit), so a machine with far more RAM than the cap can misread as "low system memory" and trigger the emergency texture pu |
| 1480–1528 (n=49) | LLAppViewer::doFrame, frameLimiterEnabled, frameLimiterFps, frameLimiterVSyncLock, frameLimiterVSyncSetting | unknown | mixed/perf-review | unknown; conditional evidence present | Comment evidence: [FrameLimiter] One cached bool read per frame; everything else is skipped when the cap is off. [FrameLimiter] Opt-in driver pacing (wglSwapIntervalEXT); off = CPU pacing only. [FrameLimiter] The CACHED viewer VSync setting (never a WGL query). With VSync on and the cap at or abov |
| 1621–1628 (n=8) | LLAppViewer::doFrame | camera | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1677–1685 (n=9) | LLAppViewer::doFrame | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [ProbeCost] Never reuse a previous iteration, including skipped displays. |
| 1694–1707 (n=14) | LLAppViewer::doFrame | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [ProbeBeforePresent] FALSE preserves the original post-display update site for live A/B captures. |
| 1714–1724 (n=11) | LLAppViewer::doFrame | stepped-animation | creative-candidate | unknown; conditional evidence present | Edits ALSteppedAnimRuntime |
| 1754–1768 (n=15) | LLAppViewer::doFrame | unknown | mixed/perf-review | unknown; no local conditional evidence | Comment evidence: [FrameLimiter] When the cap is on, an unfocused-but-visible window (the user clicked over to OBS) is paced by the cap, not by the background yield: the yield sleeps on top of the render time and would drop a 30 fps cap to ~18 fps. A hidden or MINIMISED window keeps today's yield  |
| 1775–1786 (n=12) | LLAppViewer::doFrame | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Skip the sleep, keep the worker-thread pause. |
| 1842–1864 (n=23) | LLAppViewer::doFrame | unknown | mixed/perf-review | unknown; conditional evidence present | Comment evidence: [FrameLimiter] Fallback pacing: a frame that presented was already paced just before SwapBuffers (this returns 0 then). A frame that did not present (minimised or hidden window, skipped display) is paced here, so VSync's swap wait and the background/minimised yield above already  |
| 1939–1962 (n=24) | LLAppViewer::cleanup | camera, director-shell, looks, tracking, vcam | mixed/perf-review | unknown; conditional evidence present | Comment evidence: [XInputRumble] The main loop no longer advances patterns; stop the pad now, not when the joystick is terminated much later in cleanup. [AvatarLightResponse] Quit-time flush: the Director floater skips its close-save while quitting, and a slider debounce may still be pending. flus |
| 2107–2115 (n=9) | LLAppViewer::cleanup | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: decoded sounds now live in the dedicated "sounds" subfolder -- clear it too |
| 2182–2198 (n=17) | LLAppViewer::cleanup | sensor | creative-candidate | unknown; conditional evidence present | Edits gSavedSettings, ALUIPerfSelfTest, ALEnvOnlyDiagnostics, LL_INFOS, LL_ENDL |
| 2288–2295 (n=8) | LLAppViewer::cleanup | rig | creative-candidate | unknown; no local conditional evidence | Edits LLPipeline |
| 2432–2456 (n=25) | LLAppViewer::cleanup | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge FastExit] Everything durable is now persisted (settings, per-account settings, mute list, conversation/transaction logs, inventory cache) and all child processes (SLVoice, media/CEF plugins) and worker threads have already been shut down above - the only thing left in th |
| 2495–2519 (n=25) | LLAppViewer::initThreads | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge G5.1 4a-i] the stock 16-thread ceiling starves wide CPUs (measured: ~45 threads' worth of queued decode demand on 32 threads); 0 = stock ceiling of 16 A decoder can transiently hold a full raw image plus conversion and cache copies. Limit concurrency to one worker per 2  |
| 2610–2662 (n=53) | applyMachinimaDebugLogging | actor-bar, actor-path, camera, capture-sidecar, clones, director-shell, looks, rig, roto-tron, weather | mixed/perf-review | unknown; conditional evidence present | Comment evidence: [DebugLogging] additional fork tags |
| 2756–2762 (n=7) | LLAppViewer::loadSettingsFromDirectory | unknown | unknown-review | unknown; no local conditional evidence | Edits ALScopedPresetMaterializing |
| 2862–2868 (n=7) | LLAppViewer::loadColorSettings | director-shell | creative-candidate | unknown; no local conditional evidence | Edits ALDirectorTheme |
| 3606–3612 (n=7) | LLAppViewer::initWindow | sensor | creative-candidate | unknown; no local conditional evidence | Edits ALEnvOnlyDiagnostics |
| 3616–3622 (n=7) | LLAppViewer::initWindow | sensor | creative-candidate | unknown; no local conditional evidence | Edits ALEnvOnlyDiagnostics |
| 4574–4590 (n=17) | LLAppViewer::fastQuit, LLAppViewer::forceQuit | camera, director-shell | creative-candidate | unknown; conditional evidence present | Comment evidence: [XInputRumble] _exit below skips LLViewerJoystick::terminate. |
| 4603–4612 (n=10) | LLAppViewer::requestQuit | director-shell | creative-candidate | unknown; no local conditional evidence | Comment evidence: Final recovery capture belongs before closeAllChildren() and the logout teardown, while Director's model/UI dependencies are known to be alive. |
| 4644–4653 (n=10) | LLAppViewer::requestQuit | common-ui | creative-candidate | unknown; no local conditional evidence | Comment evidence: [SliderWheel] design v7 4.2: drop every pending slider wheel settle WITHOUT firing and latch the quit flag, so no settle can send a hover / region-environment update during logout. Covers sliders outside floaters (Quick Settings pulldown) too. |
| 4714–4724 (n=11) | LLAppViewer::abortQuit | director-shell | creative-candidate | unknown; no local conditional evidence | Edits LLFloaterDirector, ALUIPerfSelfTest |
| 4826–4837 (n=12) | LLAppViewer::initCache | vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: The general one-shot is consumed below even if another viewer owns the media cache. Only the media lock owner's completed sweep acknowledges this. |
| 4955–4966 (n=12) | LLAppViewer::initCache | vcam | creative-candidate | unknown; no local conditional evidence | Comment evidence: Retain the Preferences request after its persisted flag was consumed above, also in a read-only viewer that wins the separate media lock. Media cleanup acquires its own exclusive lock off-main, including when either VCam enable is off. purgeCache() stays non-recursive. |
| 5062–5070 (n=9) | LLAppViewer::purgeCache | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: deleteFilesInDir is non-recursive, so the decoded-sound subfolder needs its own sweep to be cleared along with the rest of the cache. |
| 5334–5357 (n=24) | LLAppViewer::idle | capture-sidecar, clones, roto-tron, stepped-animation | creative-candidate | unknown; conditional evidence present | Comment evidence: [Temporal Capture] Advance the presentation clock and freeze this iteration's immutable time context BEFORE any cinematic consumer (object path/ghost updates, gObjectList.update, particles, camera) runs later in idle() or in the following display(). Wall/network time is untouched |
| 5368–5404 (n=37) | LLAppViewer::idle | gaze | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge A0.1] no-op sample patch proving the per-item runtime gate (doc/BD_MERGE_PATCHLOG.md) [BDMerge B4] configurable max head/eye rotation angle for lookAt (donor: Black Dragon 525e398af5). Pushes the per-frame setting into llcharacter's static constraint members, since llcha |
| 5465–5479 (n=15) | LLAppViewer::idle | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Login-screen tests are ticked by idle_startup(). Once an authorized in-world test reaches STATE_STARTED, idle_startup() is no longer called. Keep the active-runner check as the zero-cost gate for normal sessions. |
| 5612–5620 (n=9) | LLAppViewer::idle | tracking | creative-candidate | unknown; conditional evidence present | Edits gDisconnected, gTeleportDisplay, ALProjectorTracker |
| 5656–5703 (n=48) | LLAppViewer::idle | actor-bar, actor-path, clones, gaze, looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ObjectPath] client-side object drives (props on splines): advance and re-assert each enrolled object's rendered transform BEFORE gObjectList.update(), so the driven mount's new pose is consumed THIS frame by object interpolation, the seated-avatar root slave, the skeleton world- |
| 5793–5911 (n=119) | LLAppViewer::idle | actor-path, camera, clones, director-shell, rig, shafts-volumetrics, vcam, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge B3] Freeze World: hold the particle simulation while the world is frozen so existing particles hang in place instead of aging out (donor: Black Dragon b4cfc2cb83; BD leaves particles running, but the merge spec for this item requires the full scene — including particles  |
| 5914–5957 (n=44) | LLAppViewer::idle | camera, looks, vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: Follow the final VCam/flycam camera; hidden costs one cached bool test. The ownership sampler and recorder must observe the same final, post-operator camera.  Keep this immediately before onIdleFrame(). |
| 5967–5987 (n=21) | LLAppViewer::idle | stepped-animation, tracking | creative-candidate | unknown; conditional evidence present | Comment evidence: applyHeld above is the final render-only skeleton writer (including boil/smear and held root yaw). Sample/capture and place projectors here, then finish their drawable/bounds/child mesh work before display() can enter its earliest hero, mirror or cube-probe render pass. |
| 6011–6018 (n=8) | LLAppViewer::idleShutdown | common-ui | creative-candidate | unknown; no local conditional evidence | Comment evidence: [SliderWheel] second quit site: cancel (never fire) pending slider wheel settles |
| 6094–6100 (n=7) | LLAppViewer::sendLogoutRequest | tracking | creative-candidate | unknown; no local conditional evidence | Edits ALProjectorTracker |
| 6419–6433 (n=15) | LLAppViewer::disconnectViewer | clones, director-shell, looks, vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: Write/clean the Director recovery BEFORE Prism/rig/world teardown below (a forced disconnect would otherwise save a stripped scene). Latched. |
| 6439–6446 (n=8) | LLAppViewer::disconnectViewer | sensor | creative-candidate | unknown; no local conditional evidence | Edits ALExposureSettingsHistory |
| 6457–6501 (n=45) | LLAppViewer::disconnectViewer | actor-bar, actor-path, clones, composition, director-shell, rig, shafts-volumetrics, vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: Local light emitters must leave mLights before the region/object list is torn down. This is idempotent on logout, relog, and final shutdown. [DirFix] The path editor's shared edit session (edit owner/actor/node, the transient path-edit tool, a held path-camera preview) is per-ses |

## Slice 1: `indra/newview/llfloaterenvironmentadjust.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 27–65 (n=39) | unknown/non-function (declaration, XML, build list or global scope) | common-ui, environment, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge B13] BD - Windlight Stuff |
| 88–113 (n=26) | bdmerge_env_gate | environment | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge B13] BD - Windlight Stuff [BDMerge B13] gate; off = stock behavior |
| 115–296 (n=182) | env_intensity_builtin_values, env_intensity_reset_setting | rig, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: [EnvIntensity v2] Advanced exposure panel (lp_env_intensity_adv). [EnvIntensity v2] every Advanced-panel key: "Reset advanced" restores these (never the two masters AlchemyEnvSunEV / AlchemyEnvSkyGIEV, never the panel-state key) and then explicitly re-links the moon. [EnvIntensit |
| 299–398 (n=100) | LLFloaterEnvironmentAdjust::fitFloaterToEnvironmentDocument, LLFloaterEnvironmentAdjust::postBuild, LLFloaterEnvironmentAdjust::reshapeOuterStackToVisiblePanels, LLFloaterEnvironmentAdjust::~LLFloaterEnvironmentAdjust | common-ui | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge B13] Match LLLayoutStack::updateLayout exactly: spacing is charged once for every visible panel, and refunded only when the literal last panel is visible. lp_bdmerge is that last panel even while hidden. The stack is laid out inside the tab panel at the tab container's f |
| 428–658 (n=231) | ALEnvIntensityPresets::connectApplied, ALEnvIntensityPresets::connectListChanged, ALExposureSettingsHistory::connectRevisionChanged, LLFloaterEnvironmentAdjust::onOpen, LLFloaterEnvironmentAdjust::postBuild, emplace_back, getControl, getSignal, setAction, setCommitCallback | environment, looks, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: [EnvIntensity] Light Intensity strip (lp_env_intensity). The sliders and the Auto exposure checkbox bind to viewer settings via control_name in the XML; only the reset-to-0-EV buttons need code. Viewer-side multipliers, never written into the sky asset (mLiveSky is untouched). [E |
| 666–679 (n=14) | LLFloaterEnvironmentAdjust::onOpen | environment | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge B13] BD - Windlight Stuff: populate the sky preset combo |
| 682–713 (n=32) | LLFloaterEnvironmentAdjust::onClose | unknown | unknown-review | unknown; conditional evidence present | Edits LLEnvironment |
| 767–780 (n=14) | LLFloaterEnvironmentAdjust::refresh | environment | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge B13] BD - Windlight Stuff: reflect the env-wide lock state |
| 788–806 (n=19) | LLFloaterEnvironmentAdjust::captureCurrentEnvironment | clones | creative-candidate | unknown; conditional evidence present | Edits LLEnvironment, LLSettingsSky, LLSettingsWater |
| 810–842 (n=33) | LLFloaterEnvironmentAdjust::captureCurrentEnvironment | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: Preserve the old selection side effect for an existing fixed local environment even when one half is temporarily unavailable. |
| 1123–1418 (n=296) | LLFloaterEnvironmentAdjust::applyEnvIntensityAdvanced, LLFloaterEnvironmentAdjust::applyEnvIntensityBuiltin, LLFloaterEnvironmentAdjust::applyEnvIntensityPreset, LLFloaterEnvironmentAdjust::draw, LLFloaterEnvironmentAdjust::onEnvIntensityPresetSelected, LLFloaterEnvironmentAdjust::rebuildEnvIntensityUserPresets, LLFloaterEnvironmentAdjust::refreshEnvIntensityPresetCombo, LLFloaterEnvironmentAdjust::updateEnvIntensityMoonLink, LLFloaterEnvironmentAdjust::updateExposureHistoryButtons, LLFloaterEnvironmentAdjust::updateGammaLabel | common-ui, looks, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: [EnvIntensity] Two sky states make the Light Intensity sliders (partly) inert, mirroring LLSettingsVOSky::applySpecial: - classic sky (pre-PBR shading: a legacy sky with RenderSkyAutoAdjustLegacy off): both EV factors are inactive (1.0); - legacy-gamma sky (probe ambiance == 0):  |
| 1426–1598 (n=173) | LLFloaterEnvironmentAdjust::doApplyCreateNewInventory, LLFloaterEnvironmentAdjust::loadSkySettingFromFile, LLFloaterEnvironmentAdjust::onButtonApply, LLFloaterEnvironmentAdjust::onButtonDelete, LLFloaterEnvironmentAdjust::onButtonImport, LLFloaterEnvironmentAdjust::onButtonSave, LLFloaterEnvironmentAdjust::onCloudScrollXLocked, LLFloaterEnvironmentAdjust::onCloudScrollYLocked, LLFloaterEnvironmentAdjust::onInventoryCreated, LLFloaterEnvironmentAdjust::onSaveAsCommit, LLFloaterEnvironmentAdjust::onSelectPreset, LLFloaterEnvironmentAdjust::updateEnvOnlyAvailability, LLSettingsVOBase::createInventoryItem, confirmDeletePreset | clones, environment, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge B13] BD - Windlight Stuff (donor: BD llfloaterenvironmentadjust.cpp lines 753-937). Only reachable when BDMergeEnvLocalPresets is enabled (controls hidden and unwired otherwise). ============================================================================================ |

## Slice 1: `indra/newview/llfloaterenvironmentadjust.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 31–49 (n=19) | unknown/non-function (declaration, XML, build list or global scope) | common-ui | creative-candidate | unknown; no local conditional evidence | Comment evidence: BD - [BDMerge B13] |
| 60–78 (n=19) | unknown/non-function (public:) | common-ui, director-shell, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: The built-in Light Intensity quick presets, by key ("golden_hour"), for the Director Exposure page's preset strip. Applies exactly what the Personal Lighting combo applies, as one undo step in the exposure settings history. False for an unknown key. |
| 103–201 (n=99) | unknown/non-function (private:) | environment, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: [EnvIntensity v2] Advanced exposure panel show/hide (AlchemyEnvIntensityAdvanced) and the Sun/Moon link label + Moon slider enable (AlchemyEnvMoonLinked). [EnvIntensity presets] Quick Preset combo (env_intensity_preset) on the Light Intensity strip. Applying a preset writes the f |

## Slice 1: `indra/newview/llfloaterhoverheight.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–7 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | composition | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 29–78 (n=50) | LLFloaterHoverHeight::~LLFloaterHoverHeight, clientHeightShortId | clones, common-ui, composition | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ClientHeight] the lambdas capture this |
| 93–133 (n=41) | ALClientHeight::instance, LLFloaterHoverHeight::postBuild, setCommitCallback, setPrearrangeCallback | composition, director-shell | creative-candidate | unknown; conditional evidence present | Comment evidence: Stock behaviour, unchanged: the server send happens only on mouse-up / editor commit. [ClientHeight] target selector, local offset slider, resets, status. The drop-down opening is one of the few places the list is rebuilt (never from draw()). Quick-reset buttons at the end of eac |
| 149–179 (n=31) | LLFloaterHoverHeight::onOpen, LLFloaterHoverHeight::postBuild | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: virtual A UUID key (menu "Local Height...") selects that avatar; otherwise keep the selection (default Me). onClose() drops the region callback; take it back up on re-open. |
| 228–443 (n=216) | LLFloaterHoverHeight::isMeSelected, LLFloaterHoverHeight::onClientHeightMoved, LLFloaterHoverHeight::onNameResolved, LLFloaterHoverHeight::onResetAll, LLFloaterHoverHeight::onResetTarget, LLFloaterHoverHeight::onStoreChanged, LLFloaterHoverHeight::onTargetChanged, LLFloaterHoverHeight::rebuildTargets, LLFloaterHoverHeight::refreshStatus, LLFloaterHoverHeight::refreshTargetWidgets, LLFloaterHoverHeight::targetKey, LLFloaterHoverHeight::updateEditEnabled, push_back, std::sort | clones, composition | creative-candidate | unknown; conditional evidence present | Comment evidence: [ClientHeight] The stock (server) hover slider is region-gated AND only meaningful for Me: it never moves when another avatar is selected, so nobody's server hover is changed by accident. --------------------------------------------------------------------------- [ClientHeight] l |

## Slice 1: `indra/newview/llfloaterjoystick.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 36–42 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | camera, common-ui | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 45–54 (n=10) | unknown/non-function (declaration, XML, build list or global scope) | camera | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 83–89 (n=7) | di8_list_devices_callback | camera | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 98–106 (n=9) | unknown/non-function (BOOL CALLBACK di8_list_devices_callback(LPCDIDEVICEINSTANCE device_instance_ptr,) | camera | creative-candidate | unknown; no local conditional evidence | Edits LLFloater, LLEventTimer |
| 112–315 (n=204) | LLFloaterJoystick::draw, LLFloaterJoystick::postBuild, LLFloaterJoystick::refreshDroneEnabled, LLFloaterJoystick::refreshDronePresetLabel, LLFloaterJoystick::refreshDroneReadouts, LLFloaterJoystick::refreshLiveControls, LLFloaterJoystick::refreshWeightedReadout, LLFloaterJoystick::tick | camera, common-ui | creative-candidate | unknown; conditional evidence present | Edits LLViewerJoystick, LL_PROFILE_ZONE_SCOPED_CATEGORY_UI, LLFloater, LLFloaterJoystick, gFrameIntervalSeconds, LLStringExplicit, LLFlycamDrone, LLSliderCtrl, LLRect, gSavedSettings |
| 323–401 (n=79) | LLFloaterJoystick::onOpen, LLFloaterJoystick::postBuild | camera, common-ui | creative-candidate | unknown; no local conditional evidence | Edits LLButton, LLTabContainer, LLTextBox, LLUICtrl, LLPanel, LLComboBox, LLSliderCtrl, LLFloaterJoystick, LLSD, LLFloater |
| 410–453 (n=44) | LLFloaterJoystick::initFromSettings | camera | creative-candidate | unknown; conditional evidence present | Edits gSavedSettings, LLControlVariable |
| 562–576 (n=15) | LLFloaterJoystick::refreshListOfDevices | camera | creative-candidate | unknown; conditional evidence present | Edits LLViewerJoystick |
| 583–593 (n=11) | LLFloaterJoystick::refreshListOfDevices | camera | creative-candidate | unknown; no local conditional evidence | Edits LLSD, LLViewerJoystick |
| 595–613 (n=19) | LLFloaterJoystick::cancel, LLFloaterJoystick::refreshListOfDevices | camera | creative-candidate | unknown; conditional evidence present | Comment evidence: Update tracking |
| 696–702 (n=7) | LLFloaterJoystick::onCommitJoystickEnabled | camera | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 716–787 (n=72) | LLFloaterJoystick::onClickResetDrone, LLFloaterJoystick::onClickResetWeighted, LLFloaterJoystick::onClickRestoreXboxDefaults, LLFloaterJoystick::onCommitDronePreset, LLFloaterJoystick::onCommitDroneTuning, LLFloaterJoystick::onCommitFlightModel, LLFloaterJoystick::onCommitWeightedPreset | camera | creative-candidate | unknown; conditional evidence present | Comment evidence: WP4 owns the preset/reset implementation and exposes this entry point. |
| 820–833 (n=14) | LLFloaterJoystick::onClose, LLFloaterJoystick::setXboxDefaults | camera | creative-candidate | unknown; no local conditional evidence | Edits LLFloaterJoystick, LLViewerJoystick |

## Slice 1: `indra/newview/llfloaterjoystick.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 27–60 (n=34) | unknown/non-function (declaration, XML, build list or global scope) | camera, common-ui | creative-candidate | unknown; no local conditional evidence | Edits LLPanel, LLTabContainer, LLTextBox, LLSliderCtrl, LLUICtrl, LLFloaterJoystick, LLFloater, LLEventTimer, LLSD |
| 71–99 (n=29) | unknown/non-function (private:) | camera | creative-candidate | unknown; no local conditional evidence | Edits LLUICtrl, LLSD |
| 117–146 (n=30) | unknown/non-function (private:) | camera, common-ui | creative-candidate | unknown; no local conditional evidence | Edits LLTabContainer, LLTextBox, LLUICtrl, LLPanel, LLComboBox, LLSliderCtrl, LLUUID, ALScrollFit, LLStatBar, LLButton |

## Slice 1: `indra/newview/llpanelmaininventory.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 26–33 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | common-ui | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 145–152 (n=8) | LLPanelMainInventory::LLPanelMainInventory | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 327–337 (n=11) | LLPanelMainInventory::postBuild | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: The master observer is the only custom-tabs connection retained while off. |
| 341–349 (n=9) | LLPanelMainInventory::~LLPanelMainInventory | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Custom tabs call into the parent on selection changes during teardown. |
| 611–653 (n=43) | LLPanelMainInventory::handleMouseDown, LLPanelMainInventory::handleRightMouseDown, LLPanelMainInventory::refreshFinderFromFilter, LLPanelMainInventory::updateCustomTabsEnabled | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Complete the custom-to-stock transition once while the old panel is known. Label-only refresh: do not disturb an in-progress Finder edit. |
| 658–664 (n=7) | LLPanelMainInventory::resetFilters | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 675–681 (n=7) | LLPanelMainInventory::findLinks | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 753–759 (n=7) | LLPanelMainInventory::onSelectSearchType | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 842–848 (n=7) | LLPanelMainInventory::onClearSearch | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 879–888 (n=10) | LLPanelMainInventory::onFilterEdit | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 895–902 (n=8) | LLPanelMainInventory::onFilterEdit | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: set new filter string Custom tabs own their substring; fixed tabs retain the shared search. |
| 947–954 (n=8) | LLPanelMainInventory::onFilterSelected | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 957–970 (n=14) | LLPanelMainInventory::onFilterSelected | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 981–1000 (n=20) | LLPanelMainInventory::onFilterSelected | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: A user tab switch replaces text; external sync must respect typing. |
| 1013–1030 (n=18) | LLPanelMainInventory::handleDragAndDrop | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: FIRE-35598 / c0846c53d7: never allow a drag to activate the + pseudo-panel. |
| 1053–1069 (n=17) | LLPanelMainInventory::draw | unknown | unknown-review | unknown; conditional evidence present | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |
| 1076–1082 (n=7) | LLPanelMainInventory::draw | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1359–1366 (n=8) | LLFloaterInventoryFinder::updateElementsFromFilter | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1388–1395 (n=8) | LLFloaterInventoryFinder::draw | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |
| 1539–1549 (n=11) | LLFloaterInventoryFinder::draw | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1562–1568 (n=7) | LLFloaterInventoryFinder::onCreatorSelfFilterCommit | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1583–1589 (n=7) | LLFloaterInventoryFinder::onCreatorOtherFilterCommit | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 1: `indra/newview/llstartup.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–32 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | clones, sensor | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 44–52 (n=9) | unknown/non-function (declaration, XML, build list or global scope) | common-ui, looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 201–209 (n=9) | unknown/non-function (declaration, XML, build list or global scope) | actor-bar, director-shell, gaze | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 459–465 (n=7) | idle_startup | unknown | unknown-review | unknown; no local conditional evidence | Edits ALUIPerfSelfTest |
| 930–945 (n=16) | idle_startup | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: The UI performance harness is deliberately login-screen-only. It must never consume saved AutoLogin credentials or command-line UserLoginInfo, even when those would normally bypass the login UI. |
| 1029–1035 (n=7) | idle_startup | unknown | unknown-review | unknown; no local conditional evidence | Edits ALUIPerfSelfTest |
| 1130–1136 (n=7) | idle_startup | unknown | unknown-review | unknown; conditional evidence present | Edits gRememberUser, ALUIPerfSelfTest |
| 1490–1499 (n=10) | idle_startup | looks, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: [AvatarLightResponse] per-account light-response entries. Loaded here, not in STATE_LOGIN_CLEANUP: the saved "self" entry resolves through gAgentID, which process_login_success_response() has only just set. |
| 1530–1536 (n=7) | idle_startup | sensor | creative-candidate | unknown; no local conditional evidence | Edits ALEnvOnlyDiagnostics |
| 2264–2274 (n=11) | idle_startup | director-shell, gaze | creative-candidate | unknown; no local conditional evidence | Comment evidence: [Machinima] Drive Actor Gaze random-performance cycling from the idle list for the whole session (guarded), so persisted DirectorGazeRandomMode runs without needing the gaze floater opened first. |
| 2648–2666 (n=19) | doOnIdleOneTime, idle_startup | actor-bar, director-shell, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: List the logged-in avatar in the Director cast by default (no subject/actor role). This initialization is explicitly non-dirty because the floater may already have established its baseline. Startup feature recommendations and command-line graphics overrides are initialization, no |
| 2754–2760 (n=7) | login_callback | sensor | creative-candidate | unknown; no local conditional evidence | Edits ALEnvOnlyDiagnostics |
| 3381–3391 (n=11) | LLStartUp::setStartupState | clones | creative-candidate | unknown; conditional evidence present | Edits LLPresentationTime |
| 4292–4294 (n=3) | unknown/non-function (void handleLoadChatAlertSounds()) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 1: `indra/newview/llviewercontrol.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 28–47 (n=20) | unknown/non-function (declaration, XML, build list or global scope) | director-shell, rig, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge B3] Freeze World: pause every avatar's motion controller and set the stock FreezeTime freeze-frame flag while the camera stays free, gated by BDMergeFreezeWorld. donor: Black Dragon (NiranV Dean) b4cfc2cb83 "STORM-2118: Changed: Rewrote how Freeze World works and renamed |
| 53–62 (n=10) | unknown/non-function (declaration, XML, build list or global scope) | weather | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 93–99 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 101–107 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | common-ui | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 114–120 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 219–237 (n=19) | handleSetShaderChanged, settings_shader_change_count | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 296–327 (n=32) | handleCameraMaxRollChanged, handleCameraMaxRollSittingChanged, handleCinematicCameraChanged, handleFollowJointChanged | camera | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge B6] BD Bone Camera (donor wiring: BD handleFollowJoint) [BDMerge B8] BD Cinematic Head Tracking (donor wiring) |
| 413–447 (n=35) | handleMachinimaHighLODChanged, handleVolumeLODChanged, machinimaEffectiveMaxLODFactor | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge Batch4] Machinima High-LOD: effective object-LOD-factor ceiling. Stock is MAX_LOD_FACTOR (4.0); while BDMergeMachinimaHighLOD is on the ceiling is raised so RenderVolumeLODFactor can push mesh-repo fetch / detail well past stock. [BDMerge Batch4] sDistanceFactor = 1 - lo |
| 1014–1108 (n=95) | handleRenderWaterLevelChanged, handleSetPoseStandLock, handleSnapshotResolutionUnlockChanged, handleUseFreezeWorldChanged | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge B3] Freeze World: freeze the whole scene (avatar animation, object movement, particles) while the camera remains free. donor: Black Dragon b4cfc2cb83. Runtime-only state: pause handles + FreezeTime (both Persist 0), nothing saved settings-wise is mutated. The gate check  |
| 1133–1198 (n=66) | handleRigRimWrapAmount, setting_setup_signal_listener, settings_setup_listeners, setupWeatherMaterialListeners | director-shell, looks, rig, sensor, shafts-volumetrics, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: Material permutations are resident; every bind uploads the runtime gate. No weather listener may reload the viewer's shader inventory. Settings behavior must exist before the lazy Exposure page is opened. Restores own both preset and ISO, in either write order. Clear the old engi |
| 1207–1217 (n=11) | settings_setup_listeners | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge A5.4-1a] toggling the velocity buffer allocates/frees mVelocityMap [Ultimate Diopter] toggling the effect allocates/frees mDiopterMap |
| 1223–1241 (n=19) | settings_setup_listeners | camera | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge B6] BD Bone Camera [BDMerge B8] BD Cinematic Head Tracking [BDMerge G4.1] per-cascade + projector shadow resolution |
| 1244–1261 (n=18) | settings_setup_listeners | roto-tron | creative-candidate | unknown; no local conditional evidence | Comment evidence: T21 changes shader permutations and the main-view debug attachment. [TronA0] G-buffer avatar tag is a hashed global define -> shader reload. Avatar tags compile with HDR. This setting is now a runtime source preference. [BDMerge Batch4] Machinima High-LOD master toggle |
| 1279–1311 (n=33) | settings_setup_listeners | capture-sidecar, shafts-volumetrics, weather | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge G3.3 Dust / review fix] the dust sampler + sampling code are a compile-time permutation of projectorVolumetricF.glsl (PROJVOL_DUST_ENABLE, added in llviewershadermgr.cpp) so Dust off costs no fragment texture unit; toggling the lever must therefore rebuild shaders. handl |
| 1460–1471 (n=12) | settings_setup_listeners | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge B3] Freeze World (donor: Black Dragon b4cfc2cb83) [BDMerge C6] high-res snapshot unlock warning [BDMerge F6] Pose Stand ground lock |
| 1505–1507 (n=3) | unknown/non-function (void test_cached_control()) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 1: `indra/newview/skins/default/xui/en/floater_adjust_environment.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 4–47 (n=44) | unknown/non-function (declaration, XML, build list or global scope) | looks, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: [EnvIntensity] Light Intensity note texts, selected in updateGammaLabel(). --> [EnvIntensity v2] Advanced toggle labels and the Sun/Moon link label swap [EnvIntensity userpresets] header row of the user-preset section of the xui-check: allow-overlap save btn_flyout --> Initial st |
| 49–62 (n=14) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 90–97 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 107–114 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 127–140 (n=14) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 150–156 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 166–172 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 177–185 (n=9) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 201–207 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 221–227 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 282–288 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 303–309 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 436–442 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 456–462 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 501–507 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 521–527 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 547–1418 (n=872) | unknown/non-function (declaration, XML, build list or global scope) | common-ui, environment, looks, rig, sensor, shafts-volumetrics, vcam | creative-candidate | unknown; no local conditional evidence | Comment evidence: [EnvIntensity] Light Intensity strip. [EnvIntensity presets] Quick Presets combo. Geometry: this [EnvIntensity userpresets] "Presets..." opens the Save / Load / [EnvIntensity v2] Advanced panel toggle; label set from [EnvIntensity v2] Advanced exposure panel. Hidden by default Ro |

## Slice 2: `indra/llcommon/llmemory.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 55–64 (n=10) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: NOTE: this number MAY be less than the actual available memory on systems with more than MaxHeapSize64 GB of physical memory (default 16GB) In that case, should report min(available, sMaxHeapSizeInKB-sAllocateMemInKB) Raw immediately reusable physical memory. The effective alloca |
| 71–78 (n=8) | unknown/non-function (U32Kilobytes LLMemory::sMaxHeapSizeInKB(U32_MAX);) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLMemory |
| 107–139 (n=33) | LLMemory::updateMemoryInfo | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: This is the maximum additional commit available to this process; unlike physical availability it also catches commit-limit exhaustion. |
| 141–160 (n=20) | LLMemory::updateMemoryInfo | director-shell | creative-candidate | unknown; no local conditional evidence | Comment evidence: Our Windows definition of PagefileUsage is documented by Microsoft as "the total amount of memory that the memory manager has committed for a running process", which is rss. Use resident size as the closest portable approximation of this process's committed footprint on Darwin. |
| 166–209 (n=44) | LLMemory::updateMemoryInfo | director-shell | creative-candidate | unknown; conditional evidence present | Comment evidence: The heap ceiling is a commit ceiling, not a resident-working-set ceiling. Use private commit and saturate the subtraction: the prior unsigned underflow reported ~4 TB available once usage crossed the cap. |
| 242–252 (n=11) | LLMemory::logMemoryInfo | director-shell | creative-candidate | unknown; no local conditional evidence | Edits LL_INFOS, LL_ENDL |
| 255–272 (n=18) | LLMemory::getAvailableCommitKB, LLMemory::getAvailablePhysicalMemKB | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: static static |
| 279–296 (n=18) | LLMemory::getAllocatedPrivateMemKB, LLMemory::isSystemMemoryLow | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: static static |

## Slice 2: `indra/llui/lluicolortable.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 198–205 (n=8) | LLUIColorTable::setColor | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: An explicit edit: the colour no longer follows the skin / colour theme. |
| 223–263 (n=41) | LLUIColorTable::isDefault, LLUIColorTable::setLoadedColor | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Same node, same address: widgets holding this colour pick up the new value on their next draw. A colour the user reset still has a user node (it is never erased, see resetToDefault()); it mirrors the loaded value, so it follows the theme again. "Default" is the skin value, even w |
| 271–295 (n=25) | LLUIColorTable::resetToDefault | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: INVARIANT: no node of mLoadedColors or mUserSetColors is ever erased or moved here. LLUIColor holds a raw pointer to a node (getColor() hands out the user node while one exists, otherwise the loaded node), and widgets keep those pointers across Advanced > Reload Color Settings an |
| 304–338 (n=35) | LLUIColorTable::loadFromSettings | director-shell | creative-candidate | unknown; conditional evidence present | Comment evidence: The user file is read into a scratch table and merged by value, so no existing node moves. A colour the file sets is a user edit again (it stops following the loaded layer); every other reset colour is re-synced to the freshly reloaded loaded value. Listeners (the Director colour |
| 343–355 (n=13) | LLUIColorTable::saveUserSettings | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Compare user color value with the default value, skip if equal A reset colour only mirrors the skin / theme value: never saved. Compare user color value with the skin default, skip if equal |

## Slice 2: `indra/newview/alchatcommand.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 30–40 (n=11) | unknown/non-function (declaration, XML, build list or global scope) | actor-path, clones | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 47–53 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 57–87 (n=31) | ALChatCommand::parseCommand, objpath_report | actor-path, roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: [ObjectPath] /objpath* feedback straight to nearby chat, so a command is never a silent no-op the operator has to dig out of the log (the mistake the prototype shipped with). Same local stand-in idiom as fsradar.cpp's radar_report_to_nearby_chat. lowercased string - it does not m |
| 372–1118 (n=747) | ALChatCommand::parseCommand | actor-path, clones, crowd, director-shell, gaze, roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: ------------------------------------------------------------------- [Pathing] Actor pathing P1 test harness (until the P2 in-world editor exists). All four operate on MY avatar's session path, so a solo operator can seed, walk, and clear a 3D Catmull-Rom path and A/B the engine ( |

## Slice 2: `indra/newview/alviewermenu.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 18–53 (n=36) | unknown/non-function (declaration, XML, build list or global scope) | actor-bar, actor-path, camera, clones, composition, director-shell, looks, stepped-animation, tracking | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 62–70 (n=9) | unknown/non-function (declaration, XML, build list or global scope) | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 294–300 (n=7) | object_destroy | tracking | creative-candidate | unknown; no local conditional evidence | Edits ALProjectorTracker |
| 304–311 (n=8) | object_destroy | tracking | creative-candidate | unknown; conditional evidence present | Comment evidence: Reject an unresolved restore before temp-on-rez or any mutation. |
| 604–1433 (n=830) | alpha_mode_avatar_id, check_avatar_actor_mover_target, check_avatar_add_to_cast, check_avatar_add_to_dance_ball, check_avatar_alpha_mode, check_avatar_anim_speed, check_avatar_cinecam_follow, check_avatar_mask_cutoff, check_avatar_set_subject_a, check_avatar_set_subject_b, check_avatar_set_subject_c, check_avatar_set_subject_d, check_object_alpha_mode, check_object_cast_shadows, check_object_hero_beam, check_object_mask_cutoff, check_object_path_target, check_object_volumetric_shaft, check_object_world_shadow, dance_ball_target_from_selection, enable_avatar_alpha_mode, enable_avatar_derender, enable_avatar_mask_cutoff, enable_avatar_reset_to_mark, enable_object_alpha_mode, enable_object_cast_shadows, enable_object_hero_beam, enable_object_mask_cutoff, enable_object_shaft_clear_override, enable_object_volumetric_shaft, enable_object_world_shadow, get_selected_projector_volume, handle_avatar_actor_mover_target, handle_avatar_add_to_cast, handle_avatar_add_to_dance_ball, handle_avatar_alpha_mode, handle_avatar_anim_speed, handle_avatar_anim_speed_director, handle_avatar_cinecam_follow, handle_avatar_copy_position, handle_avatar_derender, handle_avatar_light_response, handle_avatar_local_height, handle_avatar_mask_cutoff, handle_avatar_reset_to_mark, handle_avatar_set_mark, handle_avatar_set_subject_a, handle_avatar_set_subject_b, handle_avatar_set_subject_c, handle_avatar_set_subject_d, handle_object_alpha_mode, handle_object_cast_shadows, handle_object_hero_beam, handle_object_mask_cutoff, handle_object_path_target, handle_object_shaft_capture, handle_object_shaft_clear_override, handle_object_volumetric_shaft, handle_object_world_shadow, object_path_root_from_selection, selected_primary_avatar, selected_world_shadow_lights | actor-bar, actor-path, camera, clones, composition, director-shell, looks, rig, shafts-volumetrics, stepped-animation, vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: The avatar of the primary selected object (the avatar itself, or the wearer of a selected attachment), or null. The selection handle can be null during menu teardown. Derender another avatar (or the wearer of an attachment) from the right-click menus. "persistent" is saved, anyth |
| 1471–1553 (n=83) | ALViewerMenu::initialize_menus | actor-path, camera, clones, composition, director-shell, looks, rig, shafts-volumetrics, stepped-animation | creative-candidate | unknown; no local conditional evidence | Comment evidence: The radar menu registers "Avatar.Derender" in its own scoped registrar, so these use distinct names to never collide with it. [BDMerge G3.3 Phase 2] session-only per-projector volumetric shaft toggle [World Shadow pick] beside Volumetric Shaft: session-only per-light opt-in for t |

## Slice 2: `indra/newview/app_settings/commands.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 421–463 (n=43) | unknown/non-function (declaration, XML, build list or global scope) | environment | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge B13] BD environment editor toolbar buttons --> |
| 489–502 (n=14) | unknown/non-function (declaration, XML, build list or global scope) | director-shell | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 2: `indra/newview/app_settings/key_bindings.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–12 (n=12) | unknown/non-function (declaration, XML, build list or global scope) | composition, director-shell, gaze | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 46–57 (n=12) | unknown/non-function (declaration, XML, build list or global scope) | composition, director-shell, gaze | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 134–145 (n=12) | unknown/non-function (declaration, XML, build list or global scope) | composition, director-shell, gaze | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 229–240 (n=12) | unknown/non-function (declaration, XML, build list or global scope) | composition, director-shell, gaze | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 2: `indra/newview/app_settings/settings_alchemy.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 2–183 (n=182) | unknown/non-function (declaration, XML, build list or global scope) | camera | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1308–1718 (n=411) | unknown/non-function (declaration, XML, build list or global scope) | looks, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: [TonemapLegacySky] Viewer-side opt-in; preserve the sky's legacy gamma. --> |
| 1789–1801 (n=13) | unknown/non-function (declaration, XML, build list or global scope) | crowd, looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1944–2025 (n=82) | unknown/non-function (declaration, XML, build list or global scope) | looks, rig, roto-tron, sensor | creative-candidate | unknown; no local conditional evidence | Edits RenderChromaticAberrationStrength, RenderInfraredEnabled, RenderInfraredMode, RenderInfraredStrength, RenderInfraredExposureEV, RenderInfraredFoliageEV, RenderInfraredFoliageSensitivity, RenderInfraredDryVegetation, RenderInfraredSkyDarken, RenderInfraredWaterDarken |
| 2027–2091 (n=65) | unknown/non-function (declaration, XML, build list or global scope) | looks | creative-candidate | unknown; no local conditional evidence | Edits RenderChromaticAberrationAnisotropy, RenderGradNDPosition, RenderChromaticAberrationOffsetRX, RenderGradNDSoftness, RenderChromaticAberrationOffsetRY, RenderGradNDSkyConfine, RenderChromaticAberrationOffsetBX, RenderGradNDSunWeight, RenderChromaticAberrationOffsetBY, RenderGradNDSunRadius |
| 2093–2328 (n=236) | unknown/non-function (declaration, XML, build list or global scope) | looks, weather | creative-candidate | unknown; no local conditional evidence | Edits RenderColorGradeLUT, RenderPolarizerEnabled, RenderColorGradeLUTStrength, RenderPolarizerPreset, RenderPolarizerStrength, RenderPolarizerSkySaturation, RenderPolarizerSkyDarken, RenderPolarizerAutoBand, RenderPolarizerBandRadius, RenderPolarizerGlareStrength |
| 2760–2769 (n=10) | unknown/non-function (declaration, XML, build list or global scope) | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Edits RenderGLContext10bitSDR, RenderFocusPointCrosshair, RenderFocusPointLocked |
| 2771–3003 (n=233) | unknown/non-function (declaration, XML, build list or global scope) | capture-sidecar, clones, composition, roto-tron | creative-candidate | unknown; no local conditional evidence | Edits RenderLensFlareStrength, RenderCompositionGuide, RenderLensFlareStreakLength, RenderCompositionGuideStyle, RenderLensFlareStreakFalloff, RenderCompositionGuideColor, RenderLensFlareStreakThickness, RenderCompositionGuideWidth, RenderLensFlareStreakIntensity, RenderCompositionGuideVisibility |
| 3131–3305 (n=175) | unknown/non-function (declaration, XML, build list or global scope) | clones, looks | creative-candidate | unknown; no local conditional evidence | Edits RenderParcelSelectionToMaxHeight, RenderLensFlareGhostChroma, RenderReflectionProbeIrradianceResolution, RenderLensFlareHaloChroma, RenderTonemapACESWhite, RenderLensFlareIris, RenderTonemapAgxContrast, RenderLensFlareIrisCount, RenderTonemapAgxWhite, RenderLensFlareIrisSides |
| 3307–3441 (n=135) | unknown/non-function (declaration, XML, build list or global scope) | clones, sensor, shafts-volumetrics | creative-candidate | unknown; no local conditional evidence | Edits RenderVignetteSoft, RenderLensFlareWarp, RenderLensFlareStreakTipAmount, RenderLensFlareStreakTipTint, RenderLensFlareSrcColorAmount, RenderLensFlareMaster, RenderCineLensFlareStrength, RenderCineLensFlareIntensity, RenderCineLensFlareThreshold, RenderCineLensFlareProbeRadius |
| 3443–3559 (n=117) | unknown/non-function (declaration, XML, build list or global scope) | looks | creative-candidate | unknown; no local conditional evidence | Edits RenderReflectionProbeIrradianceResolution, LLSD, RenderTonemapACESWhite, ALInspectColumnConfig, RenderTonemapAgxContrast, RenderTonemapAgxWhite, RenderDepthOfFieldNearBlur, RenderTonemapFilmicWhite, RenderTonemapReinhardWhite, RenderVignetteAmount |
| 3561–3651 (n=91) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderVignetteFeather, RenderVignetteMidColor, RenderVignetteMidPoint, RenderVignetteRadius, RenderVignetteShape, RenderVignetteSoft |
| 3653–4771 (n=1119) | unknown/non-function (declaration, XML, build list or global scope) | director-shell, environment, looks, rig, sensor, shafts-volumetrics, vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: [Cine Haze] Cinematic Depth Atmosphere: distance / height haze that [EnvIntensity] viewer-side Light Intensity (Personal Lighting floater). [EnvIntensity v2] exposure strip upgrade: moon split, GI split, probe [EnvIntensity] previously read via LLCachedControl with hard-coded |

## Slice 2: `indra/newview/app_settings/toolbars.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 9–15 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | director-shell | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 2: `indra/newview/fsfloaterposer.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 33–52 (n=20) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: <summary> Describes how to load a pose file. </summary> |
| 67–74 (n=8) | unknown/non-function (class FSFloaterPoser : public LLFloater, public LLEditMenuHandler) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLVOAvatar |
| 246–266 (n=21) | unknown/non-function (public:) | director-shell | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge] Local animation playback tab: play/stop a nearby-seen animation LOCALLY on the selected target (avatar or animesh). Client-side only; nothing is sent to the sim. Motions started via LLCharacter::startMotion survive the server animation-state reconciliation (they are not |
| 528–545 (n=18) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge] target avatar/animesh id -> set of animation ids WE started locally, so Stop / Stop All can reliably stop exactly what this floater played. |

## Slice 2: `indra/newview/llcontrolavatar.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–35 (n=11) | unknown/non-function (declaration, XML, build list or global scope) | actor-bar, actor-path, clones, director-shell, stepped-animation | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 193–201 (n=9) | LLControlAvatar::matchVolumeTransform | composition | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ClientHeight] Own term only: the attach joint already carries the wearer's offset (it rides the wearer's root), so the two STACK. |
| 247–267 (n=21) | LLControlAvatar::matchVolumeTransform | actor-path, clones, composition | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ClientHeight] local, render-only offset (world Z); setPositionAgent above is untouched. [ActorMover] local ghost locomotion for animesh: this function is the linkset-driven root placement and runs from BOTH the avatar update (llvoavatar.cpp) and LLDrawable::updateXform(), so it  |
| 360–366 (n=7) | LLControlAvatar::createControlAvatar | stepped-animation | creative-candidate | unknown; no local conditional evidence | Edits ALAvatarAnimSpeed |
| 384–403 (n=20) | LLControlAvatar::idleUpdate | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: stamp the animesh bridge like LLVOAvatar::idleUpdateMisc stamps attachments. Worn animesh is skipped (the wearer stamps this bridge). Bridge fetched from the drawable -- mControlAVBridge can go stale. |
| 411–436 (n=26) | LLControlAvatar::computeNeedsUpdate | actor-bar, director-shell, looks | creative-candidate | unknown; conditional evidence present | Comment evidence: A cast animated attachment with its own effective Actor FX is an independent presentation actor.  Do not let a jellied or throttled wearer overwrite the per-frame cadence requested by LLVOAvatar::computeUpdatePeriod().  hasEffectiveActorFx() retains the wearer's hard-mute veto. |
| 620–637 (n=18) | LLControlAvatar::updateAnimations | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: No entry and an empty entry both contribute no animations; avoid manufacturing empty process-wide state while merely reading it. |

## Slice 2: `indra/newview/lldrawpool.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–47 (n=23) | unknown/non-function (declaration, XML, build list or global scope) | looks, roto-tron | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 60–66 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 68–149 (n=82) | LLScopedVelocityPolygonOffset::LLScopedVelocityPolygonOffset, LLScopedVelocityPolygonOffset::beginPass, LLScopedVelocityPolygonOffset::endPass, LLScopedVelocityPolygonOffset::set, LLScopedVelocityPolygonOffset::~LLScopedVelocityPolygonOffset, getVelocityRiggedBias | actor-bar, actor-path, capture-sidecar, director-shell | creative-candidate | unknown; conditional evidence present | Comment evidence: [VelHistory] Keep drawable history live throughout the velocity pass, then apply each requested current transform at pass exit. Capacity survives clears. Every guard restores these exact values, so they stay current all pass. A positive bias would push velocity away from the came |
| 330–361 (n=32) | LLDrawPool::beginVelocityPass, LLDrawPool::endVelocityPass, LLDrawPool::getNumVelocityPasses, LLDrawPool::renderVelocity | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge A5.4-1a] Velocity-pass base hooks. Default: this pool contributes no velocity (0 passes). Rigid draw pools override these; skinned support is 1b. virtual virtual virtual virtual |
| 496–1868 (n=1373) | LLRenderPass::LLRenderPass, LLRenderPass::actorFxFragOffset, LLRenderPass::actorFxFrameTime, LLRenderPass::actorFxLookNeedsSyntheticBloom, LLRenderPass::actorFxScreenSize, LLRenderPass::applyModelMatrix, LLRenderPass::beginVelocityModelMatrixUpdates, LLRenderPass::bindVelocityUniforms, LLRenderPass::flushVelocityModelMatrixUpdates, LLRenderPass::invalidateModelMatrixCache, LLRenderPass::pushBatches, LLRenderPass::pushEmissiveBatchesIndexed, LLRenderPass::pushEmissiveBatchesScalar, LLRenderPass::pushMaskBatches, LLRenderPass::pushMaskBatchesIndexed, LLRenderPass::pushRiggedBatches, LLRenderPass::pushRiggedMaskBatches, LLRenderPass::pushRiggedVelocityBatches, LLRenderPass::pushUntexturedBatches, LLRenderPass::pushUntexturedRiggedBatches, LLRenderPass::pushVelocityBatches, LLRenderPass::pushVelocityBatchesTextured, LLRenderPass::renderGroup, LLRenderPass::renderRiggedGroup, LLRenderPass::shouldSuppressSharedActorFx, LLRenderPass::shouldSuppressSharedActorFxPBR, LLRenderPass::updateLastVelocityModelMatrix, LLRenderPass::uploadActorFx, LLRenderPass::uploadActorFxDisabled, LLRenderPass::uploadActorFxStyleOnly, LLRenderPass::uploadAvatarLightResponse, LLRenderPass::uploadAvatarLightResponseDisabled, LLRenderPass::uploadAvatarLightResponseTo, LLRenderPass::uploadVelocityMatrixPalettes, LLRenderPass::~LLRenderPass, actor_mask_native_changes_coverage, get_actor_fx_shader, shared_actor_fx_beauty_context, shared_actor_fx_replay_active, shared_actor_fx_style_suppresses, shared_actor_fx_zero_cover, shared_actor_fx_zero_cover_context, skip_shadow_draw, upload_actor_fx_style, upload_alr | actor-bar, actor-path, capture-sidecar, clones, director-shell, looks, roto-tron, sensor, vcam, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: [AlphaShadowCap] Fold both policies into the existing per-draw rain filter. [TronT2] Look 36 "Tron Suit" -- actorFxF.glsl. [AvatarLightResponse] per-avatar light response uniforms -- avatarLightResponseF.glsl / actorghostF.glsl Uploads one draw's response to 'shader', which MUST  |
| 1870–2005 (n=136) | LLRenderPass::pushRiggedVelocityBatches, LLRenderPass::pushRiggedVelocityBatchesTextured, LLRenderPass::pushVelocityAlphaBatchesIndexed | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: Slot count is capped at N (<= 8) by genDrawInfo; clamp defensively so a stale/over-long list can never overrun the 8-slot array or N sampler units. [VelZFight] See pushRiggedVelocityBatches: masked rigged velocity has the same unavoidable cross-program depth-expression mismatch.  |
| 2008–2099 (n=92) | LLRenderPass::pushVelocityAlphaBatchesIndexed | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: Slot count is capped at N (<= 8) by genDrawInfo; clamp defensively so a stale/over-long list can never bind past the N diffuse sampler units. |
| 2102–2111 (n=10) | LLRenderPass::pushBatch | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 2142–2148 (n=7) | LLRenderPass::pushBatch | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 2158–2170 (n=13) | LLRenderPass::pushUntexturedBatch | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 2181–2192 (n=12) | LLRenderPass::uploadMatrixPalette | unknown | unknown-review | unknown; conditional evidence present | Edits ALDerenderList |
| 2210–2216 (n=7) | LLRenderPass::uploadMatrixPalette | unknown | unknown-review | unknown; conditional evidence present | Edits ALDerenderList |
| 2247–2253 (n=7) | LLRenderPass::uploadMatrixPalette | unknown | unknown-review | unknown; conditional evidence present | Edits ALDerenderList |
| 2276–2297 (n=22) | LLRenderPass::setupGLTFTextureMatrix, LLRenderPass::teardownGLTFTextureMatrix | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: static static |
| 2299–2314 (n=16) | setup_texture_matrix, teardown_texture_matrix | unknown | unknown-review | unknown; no local conditional evidence | Edits LLDrawInfo, LLRenderPass |
| 2334–2343 (n=10) | LLRenderPass::pushGLTFBatches | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 2353–2362 (n=10) | LLRenderPass::pushUntexturedGLTFBatches | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 2380–2390 (n=11) | LLRenderPass::pushGLTFBatchesScalar | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [AlphaShadowCap] Count only the sweep that owns this slot count. |
| 2406–2416 (n=11) | LLRenderPass::pushGLTFBatchesIndexed | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [AlphaShadowCap] Count only the sweep that owns this slot count. |
| 2419–2447 (n=29) | LLRenderPass::pushGLTFBatchIndexed | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: sIndexedGLTFChannels) and contributes one element to the per-slot scalar/transform active shader material stride) and contributes one element to the per-slot scalar/transform |
| 2473–2492 (n=20) | LLRenderPass::pushGLTFBatchIndexed | unknown | unknown-review | unknown; conditional evidence present | Edits LLGLTFMaterial, ALPHA_MODE_MASK |
| 2532–2542 (n=11) | LLRenderPass::pushGLTFBatchIndexed | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 2564–2576 (n=13) | LLRenderPass::pushGLTFBatchIndexed | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: Shared replay uploads the proxy's authoritative style immediately before entering this helper (including the topology-wire bit).  Do not resolve and overwrite it from the source DrawInfo a second time. |
| 2578–2588 (n=11) | LLRenderPass::pushGLTFBatch | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 2593–2619 (n=27) | LLRenderPass::pushGLTFBatch | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: Material/texture caching must not cache the per-draw cutoff. Two objects may share one PBR asset while carrying different client overrides. |
| 2623–2646 (n=24) | LLRenderPass::pushUntexturedGLTFBatch | looks | creative-candidate | unknown; conditional evidence present | Edits LLPipeline, LLGLSLShader |
| 2775–2777 (n=3) | unknown/non-function (void LLRenderPass::pushRiggedGLTFBatchIndexed(LLDrawInfo& params, const LLVOAvat) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 2: `indra/newview/lldrawpool.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 42–72 (n=31) | unknown/non-function (class LLGLSLShader;) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [VelZFight] Preserve polygon-offset parameters around scoped velocity bias. GL_POLYGON_OFFSET_FILL enablement remains owned by an adjacent LLGLEnable. [VelZFight] Read the GL offset state ONCE per velocity pass (glGet can force a driver-thread sync under threaded optimisation). G |
| 136–150 (n=15) | unknown/non-function (public:) | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge A5.4-1a] Velocity / motion-vector pass. Base returns 0 passes so a pool that does not override these is silently skipped by renderGeomVelocity. Implemented (rigid + camera) by the opaque/alpha-mask draw pools; skinned batches are the Phase 1b seam (see pool .cpp files). |
| 388–497 (n=110) | unknown/non-function (public:) | director-shell, looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: Upload native per-actor cinematic styling to the currently bound shader. The UUID overload follows Director identity rules (null explicitly means You); LLDrawInfo's null mActorFxOwner instead means disabled and is routed through the draw-info overload/disabled helper. Returns tru |
| 504–510 (n=7) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 516–524 (n=9) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLDrawInfo |

## Slice 2: `indra/newview/lldrawpoolavatar.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–37 (n=13) | unknown/non-function (declaration, XML, build list or global scope) | actor-bar, actor-path, clones, director-shell, sensor | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 46–62 (n=17) | unknown/non-function (declaration, XML, build list or global scope) | rig, roto-tron | creative-candidate | unknown; no local conditional evidence | Comment evidence: The World-shadow routing in beginShadowPass checks one camera ID per resident World-shadow light; keep that range in step with the light count. |
| 72–79 (n=8) | unknown/non-function (LLGLSLShader* LLDrawPoolAvatar::sVertexProgram = NULL;) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLDrawPoolAvatar |
| 82–178 (n=97) | get_actor_fx_id, get_actor_fx_wearer, has_actor_fx_wireframe, has_enabled_actor_fx, is_hidden_entity_clone, use_system_actor_ghost_replay | actor-bar, actor-path, clones, director-shell, looks | creative-candidate | unknown; conditional evidence present | Comment evidence: Entity clones deliberately use the normal avatar/rigged-mesh pools, whose faces can remain enrolled after their object drawables are FORCE_INVISIBLE. Honor the viewer-local state at the pool entry point as well.  An animated attachment renders through its LLControlAvatar, so inhe |
| 247–270 (n=24) | LLDrawPoolAvatar::getModelView, avatar_modelview_overrides | unknown | unknown-review | unknown; conditional evidence present | Edits LLMatrix4 |
| 274–293 (n=20) | LLDrawPoolAvatar::popModelViewOverride, LLDrawPoolAvatar::pushModelViewOverride | unknown | unknown-review | unknown; conditional evidence present | Edits LLDrawPoolAvatar, LLMatrix4 |
| 416–422 (n=7) | LLDrawPoolAvatar::getNumShadowPasses | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: avatars opaque, avatar alpha, avatar alpha mask, alpha attachments, alpha mask attachments, opaque attachments... Solid bodies, bake-cut bodies, alpha blend, alpha mask. |
| 427–438 (n=12) | LLDrawPoolAvatar::beginShadowPass | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: Cube-only routing gate: ALL bypasses (and never writes) the per-mesh frame snapshot, so sun/spot maps in this frame retain bake-cut routing. |
| 444–450 (n=7) | LLDrawPoolAvatar::beginShadowPass | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 459–473 (n=15) | LLDrawPoolAvatar::beginShadowPass | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 481–487 (n=7) | LLDrawPoolAvatar::beginShadowPass | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 498–504 (n=7) | LLDrawPoolAvatar::endShadowPass | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 506–521 (n=16) | LLDrawPoolAvatar::renderShadow | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 528–535 (n=8) | LLDrawPoolAvatar::renderShadow | clones | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 538–659 (n=122) | LLDrawPoolAvatar::renderShadow, LLDrawPoolAvatar::shadowDrawsBake | clones, looks, rig, roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: A styled cinematic actor is already forced out of the beauty impostor path; keep its live shadow in lockstep. Mute/invisible remain stronger privacy policy and are never bypassed by Actor FX. Optionally skip the costlier avatar shadow passes (alpha blend is the most expensive and |
| 884–1041 (n=158) | LLDrawPoolAvatar::beginVelocityPass, LLDrawPoolAvatar::endVelocityPass, LLDrawPoolAvatar::getNumVelocityPasses, LLDrawPoolAvatar::renderVelocity, LLDrawPoolAvatar::renderVelocityAlpha | capture-sidecar, clones, looks | creative-candidate | unknown; conditional evidence present | Comment evidence: ============================================================================ [BDMerge A5.4-1b] Classic (system) avatar velocity pass. Donor: Black Dragon lldrawpoolavatar.cpp:500-566. Draws the system-avatar meshes with gAvatarVelocityProgram; llviewerjointmesh::uploadJointMatric |
| 1103–1110 (n=8) | LLDrawPoolAvatar::renderAvatars | clones | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1140–1168 (n=29) | LLDrawPoolAvatar::renderAvatars | clones, looks | creative-candidate | unknown; conditional evidence present | Comment evidence: Cinematic Actor FX must follow the live skeleton and authored materials; a cached impostor would freeze both. Existing mute/invisible gates below remain authoritative even when the style is enabled. |
| 1174–1195 (n=22) | LLDrawPoolAvatar::renderAvatars | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: Animesh attachment of a jellydolled or invisible parent - don't show Actor FX may keep a performance-jellied wearer and its animated attachment live, but never revives an invisible/muted wearer or bypasses the avatar render-type switch. |
| 1200–1206 (n=7) | LLDrawPoolAvatar::renderAvatars | looks | creative-candidate | unknown; conditional evidence present | Edits LLVOAvatar |
| 1221–1304 (n=84) | LLDrawPoolAvatar::renderAvatars, ~RestoreForwardWorld | clones, looks, rig, roto-tron, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: Upload on every avatar, including unstyled actors, so a shared bound shader cannot inherit the previous avatar's Actor FX state. Replace replays the rigid eyes through the dedicated world actorghost shader. The helper is main-world/replay-ready gated; every auxiliary view and lin |
| 1328–1354 (n=27) | LLDrawPoolAvatar::renderAvatars | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: The classic head/upper/lower/skirt meshes are not part of the harvested attachment batches. Draw their exact live triangles a second time as topology lines, depth-tested against the fill. |
| 1379–1381 (n=3) | unknown/non-function (LLColor3 LLDrawPoolAvatar::getDebugColor() const) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 2: `indra/newview/llfloaterpreference.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 31–37 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | sensor | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1155–1161 (n=7) | LLFloaterPreference::draw | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |
| 1652–1658 (n=7) | LLFloaterPreference::onBtnOK | sensor | creative-candidate | unknown; no local conditional evidence | Edits ALEnvOnlyDiagnostics |
| 2027–2038 (n=12) | LLFloaterPreference::onClickEnablePopup | director-shell | creative-candidate | unknown; conditional evidence present | Comment evidence: Director's crash-backup prompt: re-enabling it also forgets any remembered answer, so a later disable can never replay a stale one. |
| 2070–2077 (n=8) | LLFloaterPreference::resetAllIgnored | director-shell | creative-candidate | unknown; no local conditional evidence | Comment evidence: Resetting the prompts also forgets Director's remembered crash-backup answer. |
| 3310–3316 (n=7) | LLPanelPreferenceGraphics::draw | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |
| 3676–3685 (n=10) | LLPanelPreferenceControls::populateControlTable | gaze | creative-candidate | unknown; no local conditional evidence | Comment evidence: [Machinima] Actor Gaze look-at-camera toggles (shipped unbound). |

## Slice 2: `indra/newview/llpopupview.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 26–32 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | director-shell | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 119–126 (n=8) | LLPopupView::handleMouseEvent | director-shell | creative-candidate | unknown; no local conditional evidence | Comment evidence: Popup dispatch bypasses the row and Director event handlers. |

## Slice 2: `indra/newview/llreflectionmapmanager.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 30–53 (n=24) | unknown/non-function (declaration, XML, build list or global scope) | clones, rig, sensor, vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 143–191 (n=49) | LLReflectionMapManager::ProbeCaptureScope::ProbeCaptureScope, LLReflectionMapManager::ProbeCaptureScope::~ProbeCaptureScope | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [ProbeOnDemand] Who is capturing right now (tiny-group cull context); NONE outside LLReflectionMapManager::updateProbeFace. [ProbeOnDemand] Capture-context scope around each probe->update(): sets the capture kind and, when probe_centric, saves + clears the nearby-light list so th |
| 261–366 (n=106) | LLReflectionMapManager::update, LLReflectionMapManager::updateProbeReach | director-shell, rig, vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: A mirror's reflected eye can reach local probes outside the main sphere. Its existing roster is bounded; retain the union conservatively without another scene traversal or changing the hero manager's own relevance. Global illumination for every eye; preserve its scheduling distan |
| 369–385 (n=17) | LLReflectionMapManager::update | rig | creative-candidate | unknown; conditional evidence present | Edits LLCachedControl, gSavedSettings, RenderReflectionProbeReachCull |
| 389–446 (n=58) | LLReflectionMapManager::update | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: Round mDynamicProbeCount to the nearest increment of 16 Round mDynamicProbeCount to the nearest increment of 16 The default probe owns slot zero. While the cinematic Live Probe is designated, reserve one additional effective slot even when manual dynamic-allocation rounding would |
| 456–462 (n=7) | LLReflectionMapManager::update | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 500–558 (n=59) | LLReflectionMapManager::update | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: An ordinary pass may have started while the probe was still in reach. Discard its partial scratch capture instead of spending more far passes. [ProbeOnDemand] Collect the frame's dirty events, hit the probes they reach, run the environment / light diff and the refresh-all barrier |
| 561–588 (n=28) | LLReflectionMapManager::update | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: Keep the explicitly selected cinematic probe inside the finite cubemap allocation even in probe-dense scenes. Index zero remains the default. |
| 593–2656 (n=2064) | LLReflectionMapManager::findProbeById, LLReflectionMapManager::flushLightDiff, LLReflectionMapManager::flushProbeSchedule, LLReflectionMapManager::gatherEnvSample, LLReflectionMapManager::getCinematicRefreshStatus, LLReflectionMapManager::getRefreshAllStatus, LLReflectionMapManager::logCinematicRefresh, LLReflectionMapManager::logProbeSchedule, LLReflectionMapManager::noteLivePassForBarrier, LLReflectionMapManager::noteScheduleEarlyReturn, LLReflectionMapManager::requestCinematicLiveProbeRefresh, LLReflectionMapManager::requestRefreshAllProbes, LLReflectionMapManager::resetCinematicRefresh, LLReflectionMapManager::resetProbeSchedule, LLReflectionMapManager::sampleCinematicH, LLReflectionMapManager::sampleCinematicHOnDemand, LLReflectionMapManager::sampleOrdinaryEnvH, LLReflectionMapManager::schedId, LLReflectionMapManager::update, LLReflectionMapManager::updateCinematicBudget, LLReflectionMapManager::updateRealtimeProbeAllFaces, LLReflectionMapManager::updateRealtimeSliced, check_priority, probe_box_overlap, probe_copy3, probe_gather_light, std::lower_bound | clones, looks, rig, roto-tron, sensor, vcam, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: next distribute the free indices next distribute the free indices find the closest probe that needs a cube index make default probe have a distance of 64m for the purposes of prioritization (if it's already been generated once) [ProbeOnDemand] Barrier members are captured regardl |
| 2810–2865 (n=56) | LLReflectionMapManager::isCinematicLiveProbeIgnoredLight, LLReflectionMapManager::isCinematicLiveProbePinnedLight, LLReflectionMapManager::setCinematicLiveProbe | rig, roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: [ActorMask] Global membership, not just helpers of the Live target rig. [ActorMask] A stale pin must never override capture exclusion. |
| 2907–2921 (n=15) | LLReflectionMapManager::doProbeUpdate | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: The face has been rendered: a later texture downscale gets a larger op. |
| 2923–2974 (n=52) | LLReflectionMapManager::doProbeUpdate | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [ProbeOnDemand] time + the reasons of this refresh (G T L E P S C R W D Z B) [ProbeOnDemand] Radiance end: ack the transaction only when its irradiance pass completed in the same epoch / cube; a nack keeps the probe dirty (R_RESYNC). A barrier member is done on an ack of a transa |
| 2986–3060 (n=75) | LLReflectionMapManager::updateProbeFace, ProbeSunModeScope, ~ProbeSunModeScope | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [ProbeOnDemand] the capture-context scopes below only matter while on-demand is active or the tiny-group cull is enabled [ProbeCost] Preserve the selected pack for filtering after mRT returns to main. only render sky, water, terrain, and clouds only render sky, water, terrain, an |
| 3087–3093 (n=7) | LLReflectionMapManager::updateProbeFace | unknown | unknown-review | unknown; no local conditional evidence | Edits LLRenderTarget, gPipeline |
| 3266–3275 (n=10) | LLReflectionMapManager::updateProbeFace | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [ProbeBeforePresent] All face rendering, blur, mip generation and cube copies are now recorded. update() performs at most one optional glFlush. |
| 3294–3327 (n=34) | LLReflectionMapManager::shift | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [LiveProbeRefresh] A budget pass in flight captures about the frozen origin; keep it in the same coordinate frame as the shifted probe. [ProbeOnDemand] SH: a region crossing moves the world, not the scene. Queued events, debounce bounds, every light entry (position, accumulators  |
| 3714–3738 (n=25) | LLReflectionMapManager::getProbeCaptureScale, LLReflectionMapManager::initReflectionMaps | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [ProbeCost] One clamp shared by allocation and capture selection. |
| 3742–3748 (n=7) | LLReflectionMapManager::initReflectionMaps | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 3778–3787 (n=10) | LLReflectionMapManager::initReflectionMaps | rig | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 3842–3855 (n=14) | LLReflectionMapManager::cleanup | rig | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 2: `indra/newview/llspatialpartition.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 43–50 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 102–122 (n=21) | unknown/non-function (public:) | capture-sidecar, director-shell, looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge A5.4-1a] Points at the owning drawable's mLastVelocityMatrix. Used only by the velocity pass: read as the previous object matrix, then the current mModelMatrix is queued for pass-end write-back. Null (default) -> identity fallback (zero object velocity). Wired in LLVolum |
| 126–139 (n=14) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge] True when this draw belongs to a worn avatar attachment (rigged or not). Discriminator for the gated 3-pass alpha ordering (SIM non-rigged -> rigged -> attachment non-rigged) that keeps worn attachment alpha prims (eyelashes etc.) from being over-blended by rigged hair. |
| 147–159 (n=13) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Effective per-slot PBR alpha cutoffs, parallel to mGLTFMaterialList. MASK slots contain [0..1]; OPAQUE slots contain -1. BLEND is never indexed. Keeping this state on the draw avoids mutating shared material assets when two objects use the same PBR material with different client  |
| 178–184 (n=7) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLPipeline |
| 296–387 (n=92) | operator, worldAlphaDepth | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: interleaved world-alpha sort key: a stamped attachment's groups sort at the wearer's avatar depth, everything else at bounds depth at an exact depth tie, drain stamped ensembles before plain world groups so a null head cannot split every ensemble std::less: raw < on unrelated poi |
| 484–492 (n=9) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: avatar-ensemble sort key, fanned out from the bridge in LLPipeline::postSort; mDepth stays bounds-derived |
| 594–607 (n=14) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: alpha draw-order stamp, set by LLVOAvatar::idleUpdateMisc / LLControlAvatar::idleUpdate and fanned out to this bridge's alpha groups in LLPipeline::postSort. mAvatarp is only ever compared, never dereferenced (avatar may die first). |
| 904–906 (n=3) | unknown/non-function (extern const F32 SG_MAX_OBJ_RAD;) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 2: `indra/newview/llviewerdisplay.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 29–48 (n=20) | unknown/non-function (declaration, XML, build list or global scope) | actor-path, clones, rig, shafts-volumetrics | mixed/perf-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 64–74 (n=11) | unknown/non-function (declaration, XML, build list or global scope) | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 99–109 (n=11) | unknown/non-function (declaration, XML, build list or global scope) | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 204–241 (n=38) | display_startup | unknown | mixed/perf-review | unknown; conditional evidence present | Comment evidence: Before STATE_PRECACHE the login screen bypasses render_ui(); time its equivalent root UI draw so the login-screen harness advances. [FrameLimiter] The login screen presents here, outside swap(): it goes through the same pacedPresent() as every other present (CPU wait or driver pr |
| 466–474 (n=9) | display | unknown | mixed/perf-review | unknown; no local conditional evidence | Comment evidence: [FrameLimiter] This resize frame presents here, outside swap(): pace it the same way (each present has its own deadline) so it cannot burst after the previous present. |
| 557–567 (n=11) | display | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: RenderVBOPoolCleanAtFrameEnd: keep expiring pooled VBOs while minimized, so buffers freed by scene churn are not retained until the window is restored. |
| 711–723 (n=13) | display | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge G2.1] configurable near-clip for extreme close-ups and mouselook; 0 = stock (MIN_NEAR_PLANE). Lower values cost z-buffer precision across the scene - per-shot tool, verify with the depth sanity test when changing. |
| 782–797 (n=16) | display | weather | creative-candidate | unknown; conditional evidence present | Comment evidence: Hero initialization can reload shaders and release the surface map. Publish once after that invalidation, before mirrors or the main view. |
| 884–899 (n=16) | display | rig, vcam, weather | creative-candidate | unknown; no local conditional evidence | Comment evidence: [P1-lite] World cube shadow maps (main path only, before the Prism aux pass). Returns before any GL call while the setting is off. generateSunShadow leaves sCurCameraID on a SUN/SPOT shadow ID and the reset to CAMERA_WORLD comes later; the cube pass requires CAMERA_WORLD, so set  |
| 906–919 (n=14) | display | actor-path, clones, director-shell | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ActorMover] refresh translucent pose-ghost snapshots in the SAME dedicated impostor context (viewport + matrices are save-restored around this block). No-op with zero cost unless the ghost toggle + impostor mode are on and a mover/Director floater is open. The billboards themsel |
| 965–974 (n=10) | display | vcam | creative-candidate | unknown; no local conditional evidence | Comment evidence: The Prism Lens spike owns an isolated cull/sort/render and restores the main camera and global renderer state before the main stateSort. |
| 994–1010 (n=17) | display | actor-path, clones | creative-candidate | unknown; no local conditional evidence | Comment evidence: [GhostDeferred/P0] Harvest the ghost draw batches HERE -- right after the world stateSort + final pool rebuild -- so the frame-local references are ready BEFORE the deferred pass (the scene-lit ghost proxy submission needs them then). Moved up from its old home just before render |
| 1119–1154 (n=36) | display | actor-path, clones | creative-candidate | unknown; no local conditional evidence | Comment evidence: [GhostDeferred] Build the frame-local ghost proxy queue (enumerate + placement bounds + frustum cull + counters; NO draw calls) BEFORE the world deferred pass, then submit the clones into the G-buffer AFTER it (before the flush/lighting). The submission is wrapped in a render- st |
| 1170–1185 (n=16) | display | actor-path, clones, roto-tron, vcam | creative-candidate | unknown; no local conditional evidence | Comment evidence: [GhostDeferred] Per-frame counter log (gated on GhostDeferredDebugLog), AFTER lighting so the post-deferred forward-solid draws + FINALIZE coverage are counted. Cheap no-op when the log is off. |
| 1188–1269 (n=82) | display | capture-sidecar, clones | creative-candidate | unknown; conditional evidence present | Comment evidence: [GhostDeferred/P0] collectGhostBatches() moved earlier -- to just after the world stateSort/rebuildPools block above -- so the harvest is ready before the deferred pass. (Was here, right before render_ui().) [CloneFidelity] run an armed source-vs-clone data audit HERE -- world +  |
| 1352–1453 (n=102) | ScopedProbeShadowState, invalidateFastBinds, useBilinearReuseMaps, ~ScopedProbeShadowState | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [ProbeCost] Allocated only for opt-in faces; includes a Reuse -> Full fallback. The default face never snapshots/restores state or invalidates fast binds. [ProbeCost] Bound the hardware footprint as well as the explicit PCF taps. Restore the known stock filter policy without quer |
| 1485–1509 (n=25) | display_cube_face | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [LiveProbeFaceLights] The cinematic live probe empties mNearbyLights in beginCinematicProbeCapture(), and the only other calcNearbyLights() call (renderGeomPostDeferred) runs at the TAIL of renderDeferredLighting(), after the deferred local-light loop has already consumed the lis |
| 1519–1558 (n=40) | display_cube_face | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [ProbeCost] Keep restoration alive through deferred lighting and forward passes. Compute Full's probe clip/fade distances without generating maps. [PerfB1] Shadow setup does not make FB 0 capture-owned; deferredScreen is cleared after it is bound below. |
| 1576–1611 (n=36) | display_cube_face | roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: [ProbeBeforePresent] With the A/B switch off, labels the CPU wait that lands on the first probe GL command after present. With it on, this work is recorded before the enclosing display's present. |
| 1824–1834 (n=11) | render_ui | unknown | unknown-review | unknown; conditional evidence present | Edits LLTimer, ALUIPerfSelfTest |
| 1855–1864 (n=10) | render_ui | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Comment evidence: Scene is finalized and the G-buffer is still valid, but the UI has not yet been composited -- capture camera + G-buffer state for the ReShade bridge here. |
| 1882–1903 (n=22) | render_ui | actor-path, clones, director-shell, looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ActorStyle/GhostStudio] Actor FX is applied by native material shaders. Topology-accurate live Wireframe is submitted earlier from renderDeferredLighting(), while the main world depth target is still bound, so foreground geometry occludes its mesh/Animesh lines. [GhostStudio] st |
| 1936–1966 (n=31) | render_ui, swap | unknown | mixed/perf-review | unknown; conditional evidence present | Comment evidence: [FrameLimiter] Pace the present itself, so the present interval is the cap interval whatever the work before the swap cost. Cap off: just the swap (no wait; the swap is recorded exactly as before); each present in a frame is paced. The wait runs before the swap timer starts. Driv |
| 2067–2094 (n=28) | render_ui_3d | actor-path, clones, crowd, rig, shafts-volumetrics, vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge HideUI] Beacon lines are indicators too: require the interface to be visible under BOTH hide mechanisms (the mask gate below already covers Ctrl+Alt+F1; getUIVisibility covers HideUIControls / mouselook). Actor path guides moved to the pre-finalize scene-depth pass in di |

## Slice 2: `indra/newview/llviewerfloaterreg.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 30–75 (n=46) | unknown/non-function (declaration, XML, build list or global scope) | actor-path, camera, gaze, looks, rig, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: [Ultimate Diopter] Diopter.ResetControl commit-callback registration and preset materialization (LLPipeline::materializeDiopterPreset) |
| 87–93 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 115–131 (n=17) | unknown/non-function (declaration, XML, build list or global scope) | actor-path, capture-sidecar, director-shell, environment, vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 375–385 (n=11) | LLViewerFloaterReg::registerFloaters | camera, director-shell | creative-candidate | unknown; no local conditional evidence | Comment evidence: Machinima.ResetControl / Machinima.AutoFrameResolve must exist before ANY panel's XUI builds: XUI-only panels (e.g. panel_cinecam_frame) have no constructor to register them, and a lazily built Director page can be the first to load them. |
| 400–407 (n=8) | LLViewerFloaterReg::registerFloaters | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge C6] detachable snapshot preview (donor: Black Dragon 4d0faf18c4) |
| 434–444 (n=11) | LLViewerFloaterReg::registerFloaters | environment, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: [EnvIntensity userpresets] Save / Load / Delete floater for the Light Intensity strip presets [BDMerge B13] BD environment editors (donor: BD llviewerfloaterreg.cpp:380-381) |
| 538–803 (n=266) | LLUICtrl::CommitCallbackRegistry::defaultRegistrar, LLViewerFloaterReg::registerFloaters, getSignal | actor-path, camera, capture-sidecar, clones, director-shell, gaze, looks, rig, sensor, vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: cinematic_camera is a thin shell around panel_cinecam_params.xml (ALPanelCineCamParams), shared with the Director Console [Ultimate Diopter] pure-XML shell; all controls bind to CineDiopter* settings. Its mini reset buttons need a commit callback that is registered BEFORE the flo |
| 878–884 (n=7) | LLViewerFloaterReg::registerFloaters | actor-path | creative-candidate | unknown; no local conditional evidence | Edits LLFloaterReg, LLFloaterBuildFunc |
| 886–892 (n=7) | LLViewerFloaterReg::registerFloaters | unknown | unknown-review | unknown; no local conditional evidence | Edits LLFloaterReg, LLFloaterBuildFunc |
| 900–906 (n=7) | LLViewerFloaterReg::registerFloaters | unknown | unknown-review | unknown; no local conditional evidence | Edits LLFloaterReg, LLFloaterBuildFunc, ALFloaterPhototools |
| 912–918 (n=7) | LLViewerFloaterReg::registerFloaters | unknown | unknown-review | unknown; no local conditional evidence | Edits LLFloaterReg, LLFloaterBuildFunc |

## Slice 2: `indra/newview/llviewerinput.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 36–53 (n=18) | unknown/non-function (declaration, XML, build list or global scope) | actor-bar, clones, director-shell, gaze | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 505–619 (n=115) | actor_look_at_camera_1, actor_look_at_camera_2, actor_look_at_camera_3, actor_look_at_camera_4, actor_look_at_camera_5, cine_light_rig_enable, director_hot_gaze_1, director_hot_gaze_2, director_hot_gaze_3, director_hot_gaze_4, director_look_at_camera, releaseHotGazeKeys, toggle_actor_look_at_camera, toggle_colour_chart, world_wind_down | actor-bar, clones, director-shell, gaze, looks, rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [Machinima] World clock and Actor Gaze toggles share their panels' actions. Regular (non-global) actions debounce OS auto-repeat upstream: fire once on key down, and ignore the KEYSTATE_UP/level edges so a hold cannot toggle. [Machinima] Toggle the selected Cinematic Light Rig's  |
| 1024–1037 (n=14) | toggle_viewfinder | composition | creative-candidate | unknown; conditional evidence present | Edits gSavedSettings |
| 1208–1233 (n=26) | unknown/non-function (REGISTER_KEYBOARD_ACTION(script_mouse_handler_name, script_trigger_lbutton);) | composition, director-shell, gaze, rig | creative-candidate | unknown; no local conditional evidence | Comment evidence: [Machinima] Actor Gaze look-at-camera toggles. Regular (NOT global) actions: the regular key path debounces OS auto-repeat via gKeyboard->getKeyRepeated so a held key toggles exactly once, whereas the global-binds path fires KEYSTATE_DOWN on every repeat and would chatter-toggle. |
| 1332–1338 (n=7) | LLViewerInput::handleKey | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1371–1389 (n=19) | LLViewerInput::handleGlobalBindsKeyDown, LLViewerInput::handleKeyUp | gaze | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1396–1405 (n=10) | LLViewerInput::handleGlobalBindsKeyUp | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1413–1422 (n=10) | LLViewerInput::handleGlobalBindsMouse | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1640–1723 (n=84) | LLViewerInput::loadKeysXML, LLViewerInput::migrateKeysXML, migrateBindingMode, std::any_of | composition, director-shell, gaze | creative-candidate | unknown; conditional evidence present | Comment evidence: The marker is a viewer setting, independent of the key file's format version. Includes explicitly cleared or custom bindings for this command. Any binding on this key blocks insertion, regardless of mask. Viewfinder and Hot Gaze additions; omitted modes and all file-version seman |
| 1793–1806 (n=14) | LLViewerInput::loadBindingsXML | composition | creative-candidate | unknown; no local conditional evidence | Comment evidence: HEAD's fallback decision and legacy LMB upgrade above use only the original file. Add Viewfinder afterward so the migration is never written by that upgrade. |
| 1927–1938 (n=12) | LLViewerInput::scanKey | director-shell, gaze | creative-candidate | unknown; conditional evidence present | Edits gIdleCallbacks |
| 1963–1975 (n=13) | LLViewerInput::scanKey | gaze | creative-candidate | unknown; conditional evidence present | Comment evidence: A release belongs to the action chosen on DOWN, even if modifiers, keyboard mode or UI focus changed while held. |
| 1989–1995 (n=7) | LLViewerInput::handleMouse | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 2: `indra/newview/llviewerinput.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 75–94 (n=20) | LLSyntheticInputScope, active, ~LLSyntheticInputScope | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: RAII marker: the functional self-test wraps each synthetic event it delivers in one of these. Process-local depth counter; it cannot stick (destructor).  Not copyable. |
| 131–160 (n=30) | globalBindsSuspended, setGlobalBindsSuspended, setWorldInputSuspended, worldInputSuspended | director-shell | creative-candidate | unknown; no local conditional evidence | Comment evidence: Functional self-test safety (aluiperfselftest.cpp, in-world run): while set, the three handleGlobalBinds* entry points above run NOTHING, so a synthetic click or key can never reach a user binding such as toggle_voice (an open microphone is a server-visible effect). In-process fl |
| 201–207 (n=7) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 228–242 (n=15) | LLViewerInput::dropsNativeInput | unknown | unknown-review | unknown; no local conditional evidence | Edits LLViewerInput, LLSyntheticInputScope |

## Slice 2: `indra/newview/llviewerjoystick.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 27–48 (n=22) | unknown/non-function (declaration, XML, build list or global scope) | actor-bar, actor-path, camera, clones, director-shell, looks, rig | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 51–71 (n=21) | unknown/non-function (declaration, XML, build list or global scope) | camera | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 79–86 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | camera | creative-candidate | unknown; no local conditional evidence | Edits LLViewerJoystick |
| 90–502 (n=413) | FlycamDroneRuntime, baseParams, buildScaledParams, cachedDroneCompiledValues, configureDrone, doAfterInterval, droneCompilePreviewCache, droneCompilePreviewGeneration, dronePresetName, dronePresetProvenance, droneSettings, flycamFlightModel, logDroneClamp, logDroneValidation, refreshDroneCompilePreviewCache, requestedDroneHumanParams, sameHumanParams, sameSetting, scheduleDroneCompilePreview, shapeRadial, signedExpo, snapshot, syncDronePresetProvenance, toWeightedSample, weightedSettings | camera | creative-candidate | unknown; conditional evidence present | Edits LLVector3d, LLQuaternion, LLFlycamDrone, ALFlycamPoseOwner, RenderPose, LL_INFOS, LL_ENDL, LLCachedControl, gSavedSettings, ALFlycamWeighted |
| 537–680 (n=144) | di8_xbox_probe_callback, di8_xinput_probe_callback, isXboxProductName, loadXInput, sameWindowsDeviceId, std::transform | camera | creative-candidate | unknown; conditional evidence present | Comment evidence: [XInputPad] Types only from Xinput.h: no import library or application DLL search. |
| 708–714 (n=7) | di8_devices_callback | camera | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 756–766 (n=11) | di8_devices_callback | camera | creative-candidate | unknown; no local conditional evidence | Edits LLSD, LLViewerJoystick |
| 776–782 (n=7) | guid_from_string | camera | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 786–792 (n=7) | string_from_guid | camera | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 809–843 (n=35) | LLViewerJoystick::setOverrideCamera, LLViewerJoystick::updateEnabled | camera | creative-candidate | unknown; conditional evidence present | Edits gSavedSettings |
| 858–864 (n=7) | LLViewerJoystick::HotPlugAddCallback | camera | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 876–882 (n=7) | LLViewerJoystick::HotPlugRemovalCallback | camera | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 888–896 (n=9) | unknown/non-function (LLViewerJoystick::LLViewerJoystick()) | camera | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 900–1068 (n=169) | LLViewerJoystick::LLViewerJoystick, getControl | camera, looks | creative-candidate | unknown; conditional evidence present | Comment evidence: Undo restores every owned setting itself. Reapplying the selector here could overwrite the weight restored earlier. |
| 1075–1099 (n=25) | LLViewerJoystick::init | camera | creative-candidate | unknown; conditional evidence present | Edits LLViewerJoystick, ALXInput, gSavedSettings |
| 1154–1163 (n=10) | LLViewerJoystick::init | camera | creative-candidate | unknown; no local conditional evidence | Edits LLSD |
| 1175–1181 (n=7) | LLViewerJoystick::init | camera | creative-candidate | unknown; conditional evidence present | Edits gSavedSettings |
| 1185–1197 (n=13) | LLViewerJoystick::init | camera | creative-candidate | unknown; conditional evidence present | Edits gSavedSettings |
| 1209–1303 (n=95) | LLViewerJoystick::initDevice, LLViewerJoystick::refreshFromSettings | camera | creative-candidate | unknown; conditional evidence present | Edits LLViewerJoystick, gSavedSettings |
| 1338–1347 (n=10) | LLViewerJoystick::initDevice | camera | creative-candidate | unknown; no local conditional evidence | Edits LLSD |
| 1352–1361 (n=10) | LLViewerJoystick::initDevice | camera | creative-candidate | unknown; no local conditional evidence | Edits LLViewerJoystick, LLSD |
| 1373–1382 (n=10) | LLViewerJoystick::initDevice | camera | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1388–1394 (n=7) | LLViewerJoystick::initDevice | camera | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1417–1423 (n=7) | LLViewerJoystick::initDevice | camera | creative-candidate | unknown; no local conditional evidence | Edits LLSD |
| 1427–1440 (n=14) | LLViewerJoystick::terminate | camera | creative-candidate | unknown; conditional evidence present | Edits LL_WINDOWS, LL_MESA_HEADLESS |
| 1449–1481 (n=33) | LLViewerJoystick::updateStatus | camera | creative-candidate | unknown; conditional evidence present | Comment evidence: Functional self-test (in-world): joystick / gamepad input is ignored entirely while world input is suspended (it reaches avatar controls, object moves and the flycam). [XInputPad] Failed DI reads retain cached axes; suppress only the lost pad, and only while the feature is on (tu |
| 1512–1519 (n=8) | LLViewerJoystick::handleRun | camera | creative-candidate | unknown; conditional evidence present | Edits gSavedSettings, LLCachedControl |
| 1582–1595 (n=14) | LLViewerJoystick::agentFly | camera | creative-candidate | unknown; conditional evidence present | Comment evidence: [fork] no ALControlCache in this base; cached control instead |
| 1621–1628 (n=8) | LLViewerJoystick::agentYaw | camera | creative-candidate | unknown; conditional evidence present | Edits gAgentCamera, gSavedSettings, LLCachedControl |
| 1646–1652 (n=7) | LLViewerJoystick::resetDeltas | camera | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1657–1789 (n=133) | LLViewerJoystick::moveObjects | camera | creative-candidate | unknown; conditional evidence present | Comment evidence: Functional self-test (in-world): joystick / gamepad input is ignored entirely while world input is suspended (it reaches avatar controls, object moves and the flycam). avoid making ridicously big movements if there's a big drop in fps BD - Avoid making ridicously big movements if |
| 1796–1801 (n=6) | LLViewerJoystick::moveObjects | camera | creative-candidate | unknown; no local conditional evidence | Edits gAwayTriggerTimer |
| 1824–1841 (n=18) | LLViewerJoystick::moveAvatar | camera | creative-candidate | unknown; conditional evidence present | Comment evidence: Functional self-test (in-world): joystick / gamepad input is ignored entirely while world input is suspended (it reaches avatar controls, object moves and the flycam). [1 0 2 4  3  5] [Z X Y RZ RX RY] |
| 1845–3831 (n=1987) | LLViewerJoystick::beginMainLoop, LLViewerJoystick::computeTriggerOutputs, LLViewerJoystick::getFlycamAxis, LLViewerJoystick::getXInputStatus, LLViewerJoystick::handleXInputDpad, LLViewerJoystick::invalidateXInput, LLViewerJoystick::moveAvatar, LLViewerJoystick::moveFlycam, LLViewerJoystick::playRumble, LLViewerJoystick::pollForXboxController, LLViewerJoystick::pollXInputState, LLViewerJoystick::scanJoystick, LLViewerJoystick::scanXInputAssociation, LLViewerJoystick::seedTriggerHistory, LLViewerJoystick::setDeviceListDirty, LLViewerJoystick::setRumbleMotors, LLViewerJoystick::stopRumble, LLViewerJoystick::toggleFlycam, LLViewerJoystick::updateXInputRumble, LLViewerJoystick::updateXInputTakeover, LLViewerJoystick::xInputAssociated, LLViewerJoystick::xInputHint, xInputSettings | actor-path, camera, capture-sidecar, clones, director-shell, gaze, rig, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: If AutomaticFly is enabled, then button1 merely causes a jump (as the up/down axis already controls flying) if on the ground, or cease flight if already flying. If AutomaticFly is disabled, then button1 toggles flying. BD //BD - Remappable Joystick Controls time interval in secon |
| 3840–3880 (n=41) | LLViewerJoystick::scanJoystick | camera | creative-candidate | unknown; conditional evidence present | Edits LLCachedControl, gSavedSettings, LLToolMgr |
| 4009–4015 (n=7) | LLViewerJoystick::isLikeSpaceNavigator | camera | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 4024–4030 (n=7) | LLViewerJoystick::setSNDefaults | camera | creative-candidate | unknown; no local conditional evidence | Comment evidence: The SpaceNavigator doesn't act as a 3D cursor on macOS / Linux. The SpaceNavigator doesn't act as a 3D cursor on OS X / Linux. |
| 4034–4040 (n=7) | LLViewerJoystick::setSNDefaults | camera | creative-candidate | unknown; no local conditional evidence | Edits gSavedSettings |
| 4044–4066 (n=23) | LLViewerJoystick::setSNDefaults | camera | creative-candidate | unknown; no local conditional evidence | Edits gSavedSettings |
| 4106–4347 (n=242) | LLViewerJoystick::applyDronePreset, LLViewerJoystick::applyWeightedPreset, LLViewerJoystick::getDroneAcceptedValues, LLViewerJoystick::getDroneCompiledTick, LLViewerJoystick::getDroneCompiledValues, LLViewerJoystick::getDronePresetProvenance, LLViewerJoystick::getDronePresetProvenanceRevision, LLViewerJoystick::getWeightedTuning, LLViewerJoystick::handleHotGazeInput, LLViewerJoystick::isDronePresetCustom, LLViewerJoystick::markDronePresetCustom, LLViewerJoystick::resetDroneSettings, LLViewerJoystick::resetWeightedSettings, LLViewerJoystick::setXboxDefaults | actor-bar, camera, director-shell, gaze, looks | creative-candidate | unknown; conditional evidence present | Comment evidence: BD - Xbox360 Controller Support [fork] Personalized defaults: values below marked [user] come from the owner's proven AlchemyBeta 7.1.9.2516 config (Desktop settings.xml snapshot, 2026-07-16) so "Xbox Defaults" restores THAT setup, not stock. [user] flycam scales: slow, machinima |

## Slice 2: `indra/newview/llviewermenu.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–32 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | director-shell, tracking, vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 50–56 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | camera | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 62–68 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 76–82 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 404–410 (n=7) | unknown/non-function (void initialize_menus();) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLUICtrl |
| 654–670 (n=17) | init_menus | unknown | unknown-review | unknown; conditional evidence present | Edits LLUICtrl, gMenuHolder, LLMenuItemCallGL, LL_WARNS, LL_ENDL |
| 2098–2110 (n=13) | set_all_animation_time_factors | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Utility function to set all AV time factors to the same global value Utility function to set the global animation speed. This drives a single global multiplier that every motion controller reads live each frame -- self, other avatars, and animated objects -- so it uniformly affec |
| 2112–2118 (n=7) | handleEvent | unknown | unknown-review | unknown; no local conditional evidence | Edits LLMotionController |
| 2124–2130 (n=7) | handleEvent | unknown | unknown-review | unknown; no local conditional evidence | Edits LLMotionController |
| 4723–4732 (n=10) | handleEvent | composition | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ClientHeight] The Set Hover Height floater now also hosts the local (client-side) avatar height, which needs no region support. Keep the menu always enabled; only the stock server hover slider is gated (LLFloaterHoverHeight::updateEditEnabled()). |
| 4766–4776 (n=11) | handle_object_sit | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 5124–5133 (n=10) | get_derezzable_objects | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 5239–5256 (n=18) | derez_objects | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Explicit vectors can bypass selection policy. Synthetic local IDs must never be marshalled into a simulator DeRezObject request. |
| 6031–6037 (n=7) | handleEvent | tracking | creative-candidate | unknown; no local conditional evidence | Edits ALProjectorTracker |
| 6678–6696 (n=19) | show_debug_menus | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge DevMenus] Show the Debug/Develop menus whenever the Advanced menu is on (UseDebugMenus), not ONLY under QAMode. QAMode has intrusive side effects in release builds (it turns missing-UI-string warnings into fatal LL_ERRS crashes and echoes the log into chat - see llpanel. |
| 7044–7076 (n=33) | find_reset_skeleton_target, handleEvent | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [Alchemy] Reset Skeleton on an animesh must target the animesh's OWN control avatar. find_avatar_from_object() walks an ATTACHMENT up to its wearer, which is correct for every other caller (touch/inspect/cast handlers on a worn object), but for a WORN animesh it resets the wearer |
| 7104–7110 (n=7) | handleEvent | unknown | unknown-review | unknown; conditional evidence present | Edits LLVOAvatar, LLSelectMgr |
| 7179–7208 (n=30) | handleEvent | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge F6] Undeform Avatar (FSPose-driven) - ported from Firestorm fork (I:\enve\indra\newview\llviewermenu.cpp:10911, class FSToolsUndeform, FIRE-4345), BD/FS -> Alchemy merge campaign item F6. Distinct from Alchemy's existing "Undeform Me" (Tools.UndeformSelf / avatar_undefor |
| 7785–7791 (n=7) | handleEvent | unknown | unknown-review | unknown; conditional evidence present | Edits LLLocalMeshMgr |
| 8075–8081 (n=7) | handleEvent | unknown | unknown-review | unknown; conditional evidence present | Edits LLLocalMeshMgr |
| 10135–10159 (n=25) | handleEvent | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Flat, grey-balanced, PBR-aware neutral sky for machinima capture / color grading. Unlike the sunrise/noon/sunset/midnight entries above, this isn't a server asset UUID -- it's built in code (see LLSettingsVOSky::buildStudioNeutralDay()) and applied via the fixed-sky-pointer overl |
| 10225–10240 (n=16) | handleEvent | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: No fixed server asset UUID exists for a code-built sky, so match on name instead (set in LLSettingsVOSky::buildStudioNeutralDay()). |
| 10355–10397 (n=43) | bind_membership_label_item, handleEvent, setItem | unknown | unknown-review | unknown; conditional evidence present | Edits LLUICtrl, LLHandle, gMenuHolder, LLUpdateMembershipLabel, gMembershipLabelUpdater |
| 10539–10546 (n=8) | initialize_menus | unknown | unknown-review | unknown; no local conditional evidence | Edits LLUpdateMembershipLabel, gMembershipLabelUpdater |
| 10925–10931 (n=7) | initialize_menus | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 10937–10976 (n=40) | add, initialize_menus | camera, director-shell, vcam | creative-candidate | unknown; no local conditional evidence | Edits LLUICtrl, LLSD, LLCinematicCamera, LLToolPie, LLViewerObject, ALPanelDirectorVCam |

## Slice 2: `indra/newview/llviewertexture.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 29–37 (n=9) | unknown/non-function (declaration, XML, build list or global scope) | unknown | mixed/perf-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 50–56 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 63–73 (n=11) | unknown/non-function (declaration, XML, build list or global scope) | actor-bar, camera, director-shell, rig | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge] capture-mode auto-arming reads these "am I filming?" signals |
| 95–101 (n=7) | unknown/non-function (S32 LLViewerTexture::sImageCount = 0;) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLViewerTexture |
| 112–131 (n=20) | unknown/non-function (F32 LLViewerTexture::sCurrentTime = 0.0f;) | unknown | mixed/perf-review | unknown; no local conditional evidence | Edits LLViewerTexture |
| 493–1121 (n=629) | LLViewerTexture::getCaptureQualityRevision, LLViewerTexture::getVRAMPressureStatus, LLViewerTexture::isCaptureModeActive, LLViewerTexture::isCaptureQualityPinActive, LLViewerTexture::isCaptureQualitySuspended, LLViewerTexture::isVRAMPressureActive, LLViewerTexture::updateClass, update_capture_mode | actor-bar, camera, director-shell, roto-tron | mixed/perf-review | unknown; conditional evidence present | Comment evidence: [BDMerge] Capture mode: "am I filming?" resolved ONCE per frame.  Evaluated here rather than at the per-texture sites because those run for every texture every frame -- polling four subsystems per texture would be pure waste. Memory-heavy effects use isCaptureQualityPinActive(),  |
| 1152–1220 (n=69) | LLViewerTexture::updateClass | unknown | mixed/perf-review | unknown; conditional evidence present | Comment evidence: Background restoration must not defeat an active pressure response. The cap-only path starts with off-screen textures and lets the paced attack above cross the on-screen threshold gradually. All capture-quality overrides share this frame's hold/suspend permission. The underlying  |
| 1251–1262 (n=12) | get_render_free_main_memory_treshold | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: 512 MB was far too late on a machine whose cache could grow by roughly 1 GB/s. Scale the emergency texture valve with installed RAM while keeping reasonable bounds for both small and very large systems. |
| 1280–1287 (n=8) | LLViewerTexture::getSystemMemoryBudgetFactor | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 2184–2192 (n=9) | LLViewerFetchedTexture::createTexture | unknown | mixed/perf-review | unknown; no local conditional evidence | Edits LLTimer, BDMergeTexSpike |
| 2211–2233 (n=23) | LLViewerFetchedTexture::postCreateTexture | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [ProbeOnDemand] MS: a refinement of an already-built sculpt is LOD churn (tagged, dropped); a first arrival or a swapped asset stays a real geometry event. [ProbeOnDemand] H6: a new / refined / re-sharpened texture dirties the probes that show it (quantised by what a probe can re |
| 2376–2386 (n=11) | LLViewerFetchedTexture::processTextureStats | unknown | mixed/perf-review | unknown; no local conditional evidence | Comment evidence: Full-resolution overrides share the per-frame recovery permission. |
| 2392–2398 (n=7) | LLViewerFetchedTexture::processTextureStats | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 3688–3699 (n=12) | LLViewerLODTexture::processTextureStats | unknown | mixed/perf-review | unknown; no local conditional evidence | Comment evidence: Capture and TextureLoadFullRes share the same per-frame permission; ordinary screen-space quality can recover while the override is suspended. |
| 3705–3711 (n=7) | LLViewerLODTexture::processTextureStats | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 2: `indra/newview/llviewertexture.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 90–102 (n=13) | unknown/non-function (class LLTextureBar;) | unknown | mixed/perf-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 118–139 (n=22) | unknown/non-function (protected:) | camera, director-shell | mixed/perf-review | unknown; no local conditional evidence | Comment evidence: [BDMerge] True while the viewer believes a take is in progress (manual capture pin, or auto-armed by hide-UI / flycam / cinematic cam / Director ACTION). Resolved once per frame in updateClass(); callers must not re-derive it. Capture quality is allowed to pin bias/full-resolutio |
| 238–260 (n=23) | unknown/non-function (public:) | unknown | mixed/perf-review | unknown; no local conditional evidence | Comment evidence: estimated free memory for textures, by bias calculation Runtime VRAM-governor telemetry. Render-target bytes are already part of texture bytes and are exposed separately for diagnostics only. |
| 442–453 (n=12) | unknown/non-function (public:) | rig | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ProbeOnDemand] H6/DS bookkeeping for the reflection probe dirty recorder (alprobedirty.cpp). Written ONLY while RenderProbeOnDemand is ON. |

## Slice 2: `indra/newview/llviewerwindow.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 24–42 (n=19) | unknown/non-function (declaration, XML, build list or global scope) | actor-path, director-shell | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge C6] Snapshot conveniences: gated high-res snapshot limit override (bdmerge_max_snapshot_image_size) and toggleable date/time in local snapshot filenames. donor: Black Dragon (NiranV Dean) 16143bb98b "Added: Option to toggle adding date and time to snapshot names." + BD's |
| 77–83 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 96–104 (n=9) | unknown/non-function (declaration, XML, build list or global scope) | director-shell | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 178–185 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | clones, crowd | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 201–207 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | composition | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 252–258 (n=7) | unknown/non-function (extern bool gDisplaySwapBuffers;) | unknown | unknown-review | unknown; no local conditional evidence | Edits gSnapshot |
| 262–533 (n=272) | cinematicTargetFrameRect, drawCinematicFrameGuide, drawCinematicViewfinder | composition, director-shell | creative-candidate | unknown; conditional evidence present | Comment evidence: Target-frame rectangle shared by the frame guide, the viewfinder and the composition guide (pipeline.cpp). Returns false when no aspect is active (Off / Native, bad values) or the frame is as-near-native as the window, so there is nothing to mark. Native/as-near-native formats pr |
| 1303–1357 (n=55) | LLViewerWindow::dropToolMouseCaptureWhileWorldSuspended, LLViewerWindow::handleAnyMouseClick | director-shell | creative-candidate | unknown; conditional evidence present | Comment evidence: Functional self-test (in-world): while world input is suspended, only UI views may hold the mouse capture.  A tool (grab, select, pie ...) holding it would receive hover, clicks and wheel and could send ObjectGrabUpdate / ObjectDeGrab / ObjectSelect.  Drop it WITHOUT the onMouseC |
| 1436–1452 (n=17) | LLViewerWindow::handleAnyMouseClick | director-shell | creative-candidate | unknown; conditional evidence present | Comment evidence: Captor dispatch bypasses LLView's child traversal, so mirror its optional diagnostic hook for input-test attribution. |
| 1470–1480 (n=11) | LLViewerWindow::handleAnyMouseClick | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_NAMED_CATEGORY_UI |
| 1503–1524 (n=22) | LLViewerWindow::handleAnyMouseClick | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Functional self-test (in-world): UI dispatch only.  A click the UI did not take (or any click while the UI is disabled) is DROPPED: it never reaches the tool manager (grab, sit, select ...) or the pie menu.  Up-clicks stay "handled" as for the OS. |
| 1539–1546 (n=8) | LLViewerWindow::handleMouseDown | unknown | unknown-review | unknown; conditional evidence present | Edits gViewerInput, LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |
| 1557–1563 (n=7) | LLViewerWindow::handleDoubleClick | unknown | unknown-review | unknown; conditional evidence present | Edits gViewerInput |
| 1570–1577 (n=8) | LLViewerWindow::handleMouseUp | unknown | unknown-review | unknown; conditional evidence present | Edits gViewerInput, LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |
| 1581–1601 (n=21) | LLViewerWindow::handleMiddleMouseDown, LLViewerWindow::handleRightMouseDown, LLViewerWindow::handleRightMouseUp | unknown | unknown-review | unknown; conditional evidence present | Edits gViewerInput |
| 1608–1616 (n=9) | LLViewerWindow::handleDragNDrop | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Functional self-test (in-world): no drag-and-drop at all (SLURL dispatch, file upload, media drop), even for marked events -- the harness never needs one. |
| 1995–2001 (n=7) | LLViewerWindow::handleMiddleMouseUp | unknown | unknown-review | unknown; conditional evidence present | Edits gViewerInput |
| 2005–2011 (n=7) | LLViewerWindow::handleOtherMouse | unknown | unknown-review | unknown; conditional evidence present | Edits gViewerInput |
| 2035–2041 (n=7) | LLViewerWindow::handleMouseMove | unknown | unknown-review | unknown; conditional evidence present | Edits gViewerInput |
| 2072–2078 (n=7) | LLViewerWindow::handleMouseDragged | unknown | unknown-review | unknown; conditional evidence present | Edits gViewerInput |
| 2089–2095 (n=7) | LLViewerWindow::handleMouseLeave | unknown | unknown-review | unknown; conditional evidence present | Edits gViewerInput |
| 2161–2167 (n=7) | LLViewerWindow::handleFocus | unknown | unknown-review | unknown; conditional evidence present | Edits gViewerInput |
| 2182–2188 (n=7) | LLViewerWindow::handleFocusLost | unknown | unknown-review | unknown; conditional evidence present | Edits gViewerInput |
| 2213–2219 (n=7) | LLViewerWindow::handleTranslatedKeyDown | unknown | unknown-review | unknown; conditional evidence present | Edits gViewerInput |
| 2246–2252 (n=7) | LLViewerWindow::handleTranslatedKeyUp | unknown | unknown-review | unknown; conditional evidence present | Edits gViewerInput |
| 2263–2269 (n=7) | LLViewerWindow::handleScanKey | unknown | unknown-review | unknown; conditional evidence present | Edits gViewerInput |
| 2274–2280 (n=7) | LLViewerWindow::handleActivate | unknown | unknown-review | unknown; conditional evidence present | Edits gViewerInput |
| 2311–2317 (n=7) | LLViewerWindow::handleActivateApp | unknown | unknown-review | unknown; conditional evidence present | Edits gViewerInput |
| 2370–2400 (n=31) | LLViewerWindow::handleDataCopy, LLViewerWindow::handleScrollHWheel, LLViewerWindow::handleScrollWheel, LLViewerWindow::handleWindowBlock, LLViewerWindow::handleWindowUnblock | unknown | unknown-review | unknown; conditional evidence present | Edits gViewerInput |
| 2426–2436 (n=11) | LLViewerWindow::handleDeviceChange | camera | creative-candidate | unknown; no local conditional evidence | Comment evidence: The Xbox auto-detect enumerates DirectInput ONLY when this flag is set (event-driven; the old periodic main-thread EnumDevices walk caused a micro-stutter every few seconds while filming). |
| 2985–3021 (n=37) | LLViewerWindow::initWorldUI | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [WebIdle] AlchemyWebPreloadToolbarPages (default off): the Destination Guide, Avatar Welcome Pack and Marketplace floaters are otherwise built (and their web pages navigated + rendered) at login without ever being opened. When off they are created the first time the user opens th |
| 3366–3379 (n=14) | LLViewerWindow::drawDebugText | composition | creative-candidate | unknown; conditional evidence present | Comment evidence: Cinematic Viewfinder: drawn here, not in draw(), so it survives both UI-hiding mechanisms. gSnapshot keeps it out of every capture (render_ui() also runs for snapshots). |
| 3461–3488 (n=28) | LLViewerWindow::draw | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: UI-only, live-composite framing aid. getIndicatorsVisible() covers both mUIVisible and the RENDER_DEBUG_FEATURE_UI mask; gSnapshot keeps the guide out of every capture buffer even when "Render UI" is on. |
| 3528–3534 (n=7) | LLViewerWindow::handleKeyUp | unknown | unknown-review | unknown; conditional evidence present | Edits gViewerInput |
| 3567–3578 (n=12) | LLViewerWindow::handleKeyUp | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Functional self-test (in-world): a key release never reaches a gesture. |
| 3591–3597 (n=7) | LLViewerWindow::handleKey | unknown | unknown-review | unknown; conditional evidence present | Edits gViewerInput |
| 3604–3613 (n=10) | LLViewerWindow::handleKey | director-shell | creative-candidate | unknown; no local conditional evidence | Edits ALKitHistory, LLView |
| 3699–3718 (n=20) | LLViewerWindow::handleKey | director-shell | creative-candidate | unknown; conditional evidence present | Comment evidence: Functional self-test (in-world): after the keyboard-focus dispatch NOTHING below may run -- no menu navigation or accelerator (Edit.Undo would send 'Undo' to the sim), no tool key, no gesture trigger (a user gesture bound to a key sends chat or an animation), no Director hotkey,  |
| 3829–3856 (n=28) | LLViewerWindow::handleKey | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: [GhostStudio] Esc leaves ghost edit-mode even after the build-mode handoff. Once a ghost is selected the transient ALToolGhostEdit is no longer current (a stock Translate/Rotate/Scale composite is), so that tool's own handleKey() can never see Esc. Handle it here -- AFTER focused |
| 3875–3892 (n=18) | LLViewerWindow::handleKey | director-shell | creative-candidate | unknown; conditional evidence present | Comment evidence: [Director] Transport plus switcher number punches. Gestures and focused UI/tools above retain precedence. This must run before printable-key auto-chat so an armed, visible switcher can reserve 1..9; its fast gate returns false everywhere else and ordinary typing remains untouched |
| 3951–3974 (n=24) | LLViewerWindow::handleUnicodeChar | director-shell | creative-candidate | unknown; conditional evidence present | Comment evidence: Functional self-test (in-world): no Return remap into the user key bindings, and no menu jump-key handling; the character goes only to the keyboard-focus control. |
| 4010–4023 (n=14) | LLViewerWindow::handleScrollWheel | director-shell | creative-candidate | unknown; no local conditional evidence | Edits ALKitHistory, LLView |
| 4048–4066 (n=19) | LLViewerWindow::handleScrollWheel | clones, crowd | creative-candidate | unknown; conditional evidence present | Comment evidence: Ghost Studio's staged-crowd tool owns world-space wheel rotation after all UI layers decline it. Keep this narrow so existing media, pie, gun, mouselook, and camera wheel behavior is unchanged. A mouse-capturing tool was already dispatched above and cannot receive the event twice |
| 4280–4300 (n=21) | LLViewerWindow::updateUI | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Functional self-test (in-world): a tool holding the mouse capture must not receive the captor hover below (ObjectGrabUpdate while the synthetic mouse is parked). Functional self-test (in-world): the world gets NO hover at all -- no tool hover pick (LLToolPie::handleHover -> setHo |
| 4561–4567 (n=7) | LLViewerWindow::updateUI | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 4652–4658 (n=7) | LLViewerWindow::updateUI | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 4664–4670 (n=7) | LLViewerWindow::updateUI | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 4732–4748 (n=17) | LLViewerWindow::updateLayout | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Arm once per frame after tools choose their cursor. Win32 checks live client bounds/focus on its window thread, including captured motion; cached viewer mouse/focus events could otherwise flash on client re-entry. |
| 4984–4998 (n=15) | LLViewerWindow::renderSelections | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge HideUI] Selection indicators (silhouettes + manipulator tools) are interface: skip them entirely while the interface is hidden so a shot never carries "something is selected" tells. GL picking must keep working (it renders into the pick buffer, not the screen), so that p |
| 5208–5218 (n=11) | LLViewerWindow::pickAsync | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Functional self-test (in-world): no world pick is scheduled (tool dispatch is dropped too). |
| 5278–5288 (n=11) | LLViewerWindow::pickImmediate | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Functional self-test (in-world): world picks return nothing. |
| 5738–5752 (n=15) | bdmerge_max_snapshot_image_size | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge C6] runtime max snapshot side length (see llviewerwindow.h); 12288 with the gated high-res override on, stock 7680 otherwise |
| 5813–5833 (n=21) | LLViewerWindow::saveImageLocal | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge C6] toggleable date/time in snapshot filenames (donor: Black Dragon 16143bb98b). Stock (gate off or toggle on) keeps the timestamp; with the gate on and the toggle off, fall back to plain sequential numbering like BD. |
| 6320–6326 (n=7) | LLViewerWindow::rawSnapshot | actor-path | creative-candidate | unknown; no local conditional evidence | Edits LLActorMover |
| 6428–6434 (n=7) | LLViewerWindow::simpleSnapshot | actor-path | creative-candidate | unknown; no local conditional evidence | Edits LLActorMover |
| 6475–6481 (n=7) | LLViewerWindow::cubeSnapshot | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [PerfB1] No default-framebuffer clear: cube rendering clears its capture targets in display_cube_face(). |
| 7199–7212 (n=14) | LLViewerWindow::getIndicatorsVisible | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge HideUI] Indicator overlays follow the interface: hidden under either hide-UI mechanism (see header). Used by renderSelections, the selection-beam effects, beacon lines and face highlights. |

## Slice 2: `indra/newview/llvoavatar.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 24–46 (n=23) | unknown/non-function (declaration, XML, build list or global scope) | stepped-animation | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge B3] Freeze World: stop avatar pelvis rotation while the world is frozen, gated by BDMergeFreezeWorld + UseFreezeWorld. donor: Black Dragon (NiranV Dean) b4cfc2cb83. See doc/BD_MERGE_PATCHLOG.md. |
| 49–71 (n=23) | unknown/non-function (declaration, XML, build list or global scope) | actor-bar, actor-path, clones, composition, director-shell, looks, rig, roto-tron | creative-candidate | unknown; no local conditional evidence | Edits gAgentID |
| 743–749 (n=7) | unknown/non-function (LLVOAvatar::LLVOAvatar(const LLUUID& id,) | clones | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 861–867 (n=7) | LLVOAvatar::~LLVOAvatar | actor-path | creative-candidate | unknown; conditional evidence present | Edits LLActorMover |
| 898–934 (n=37) | LLVOAvatar::getClientOuterTransformHandle, LLVOAvatar::getClientOuterTransformInverse, LLVOAvatar::getClientOuterTransformMatrix, LLVOAvatar::getClientOuterTransformRevision, LLVOAvatar::hasClientOuterTransform, LLVOAvatar::markDead | actor-path, capture-sidecar | creative-candidate | unknown; conditional evidence present | Edits LLVOAvatar, LLClientOuterTransform, LLMatrix4, LLActorMover |
| 1039–1045 (n=7) | LLVOAvatar::areAllNearbyInstancesBaked | clones | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1061–1067 (n=7) | LLVOAvatar::getNearbyRezzedStats | clones | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1124–1130 (n=7) | LLVOAvatar::dumpBakedStatus | clones | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1348–1354 (n=7) | LLVOAvatar::initInstance | stepped-animation | creative-candidate | unknown; no local conditional evidence | Edits ALAvatarAnimSpeed |
| 1631–1651 (n=21) | LLVOAvatar::calculateSpatialExtents | capture-sidecar | creative-candidate | unknown; conditional evidence present | Comment evidence: Minimal Phase-1 culling support: enlarge/contract the complete native avatar-plus-attachments bounds by the same positive foot-pivoted scale. |
| 2016–2023 (n=8) | LLVOAvatar::lineSegmentIntersect | unknown | unknown-review | unknown; conditional evidence present | Edits gAgent, LLPipeline, ALDerenderList |
| 2124–2131 (n=8) | LLVOAvatar::lineSegmentIntersectRiggedAttachments | unknown | unknown-review | unknown; conditional evidence present | Edits gAgent, ALDerenderList |
| 2177–2205 (n=29) | LLVOAvatar::startDefaultMotions | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: ------------------------------------------------------------------------- start default motions ------------------------------------------------------------------------- [Alchemy] Honor mEnableDefaultMotions HERE, not only at the buildCharacter() call site. LLControlAvatar (llcon |
| 2339–2345 (n=7) | LLVOAvatar::resetSkeleton | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 2439–2452 (n=14) | LLVOAvatar::resetSkeleton | actor-path, clones | creative-candidate | unknown; conditional evidence present | Comment evidence: [Machinima] The skeleton was rebuilt: joint pointers and bone lengths changed. A world/joint pause retains only the ambient phase; stale rig repair state is reset before a zero-step paint of the new skeleton. |
| 2807–2814 (n=8) | LLVOAvatar::idleUpdate | clones | creative-candidate | unknown; no local conditional evidence | Edits ALDerenderList |
| 2828–2836 (n=9) | LLVOAvatar::idleUpdate | clones | creative-candidate | unknown; conditional evidence present | Edits ALDerenderList |
| 2888–2906 (n=19) | LLVOAvatar::idleUpdate | actor-path | creative-candidate | unknown; conditional evidence present | Comment evidence: [ObjectPath] A path-driven object is placed UNDAMPED by ALObjectPathMover (exact analytic position each frame). This seated-avatar early-move would otherwise re-issue it DAMPED -- updateMoveDampedAsync clears MOVE_UNDAMPED and sets EARLY_MOVE, so the damped move WINS and the obje |
| 2947–2979 (n=33) | LLVOAvatar::idleUpdate | clones, composition | creative-candidate | unknown; conditional evidence present | Comment evidence: [ClientHeight] mLastRootPos stays VISUAL (name tag, voice); the BASE twin feeds only getOffObject's standoff fallback. [BDMerge B12] (EXPERIMENTAL) Mouselook head-bone scaling: apply/restore every frame, after the character's own motion/appearance-driven joint update above, so ne |
| 3015–3022 (n=8) | LLVOAvatar::idleUpdate | clones | creative-candidate | unknown; conditional evidence present | Edits LLFrameTimer |
| 3029–3035 (n=7) | LLVOAvatar::idleUpdateVoiceVisualizer | unknown | unknown-review | unknown; no local conditional evidence | Edits ALDerenderList |
| 3128–3154 (n=27) | LLVOAvatar::calcRiggedAlphaDepth | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: camera-axis depth of this avatar, stamped onto every rigged alpha bridge (attachments below, animesh in LLControlAvatar::idleUpdate) so its alpha interleaves with world alpha (LLDrawPoolAlpha::renderAlpha). From the animated extents, so avatars sharing a sit target still order de |
| 3166–3172 (n=7) | LLVOAvatar::idleUpdateMisc | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 3227–3244 (n=18) | LLVOAvatar::idleUpdateMisc | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: stamp the attachment's draw order onto its bridge; LLPipeline::postSort fans it out to the bridge's alpha groups, sorting the wearer's whole ensemble at one avatar depth. HUD alpha sorts in HUD space, so unrigged HUDs stay unstamped. |
| 3379–3390 (n=12) | LLVOAvatar::idleUpdateLipSync | stepped-animation | creative-candidate | unknown; conditional evidence present | Comment evidence: Lip-sync holds with the face bones: the actor's Face part (StepAnimFace off or Face part Smooth never holds; a coarser Face part releases on its own refresh frames). |
| 3546–3557 (n=12) | LLVOAvatar::idleUpdateNameTag | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: A client-only ghost clone has no resident identity -- no name tag. |
| 4197–4210 (n=14) | LLVOAvatar::isVisuallyMuted, LLVOAvatar::slamPosition | actor-path | creative-candidate | unknown; conditional evidence present | Comment evidence: [Machinima] A position slam (teleport / region cross) is a discontinuity: drop Pose Polish shadow state so it never smooths across the jump. |
| 4212–4219 (n=8) | LLVOAvatar::isVisuallyMuted | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: * a client-only Ghost Studio clone is never visually muted |
| 4232–4244 (n=13) | LLVOAvatar::isVisuallyMuted | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: Actor FX needs the authored live avatar, not a complexity/ART stand-in. Explicit mute-list and Never Render policy above remain authoritative; only automatic performance muting is bypassed. |
| 4608–4614 (n=7) | LLVOAvatar::updateFootstepSounds | clones | creative-candidate | unknown; conditional evidence present | Edits gAudiop |
| 4663–4689 (n=27) | LLVOAvatar::computeUpdatePeriod | actor-bar, clones, looks, rig, roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: Ghost clones and styled cinematic actors always update their authored skeleton every frame. Actor FX cannot use temporally throttled jelly or impostor state without visibly freezing animation and attachments. [AvatarLightResponse] An adjusted avatar is kept live: full geometry an |
| 4747–4756 (n=10) | LLVOAvatar::updateOrientation | clones | creative-candidate | unknown; conditional evidence present | Edits ALAvatarAnimationTime |
| 4823–4842 (n=20) | LLVOAvatar::updateOrientation | actor-path | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge B3] Freeze World: stop avatars from rotating while the world is frozen (donor: Black Dragon b4cfc2cb83) |
| 4845–4860 (n=16) | LLVOAvatar::updateOrientation | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 4863–4870 (n=8) | LLVOAvatar::updateOrientation | clones | creative-candidate | unknown; no local conditional evidence | Edits LLFrameTimer, ALAvatarAnimationTime |
| 4893–4910 (n=18) | LLVOAvatar::updateOrientation | unknown | unknown-review | unknown; conditional evidence present | Edits gAgent |
| 4942–4948 (n=7) | LLVOAvatar::updateTimeStep | clones | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 5000–5018 (n=19) | LLVOAvatar::updateRootPositionAndRotation | clones | creative-candidate | unknown; conditional evidence present | Edits LLMotionController, ALAvatarAnimationTime |
| 5026–5043 (n=18) | LLVOAvatar::updateRootPositionAndRotation | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: [GhostClone] A ghost clone's object position is ALREADY its intended render root (LLGhostAvatar::ghostSlamPosition stores foot + pelvisToFoot). The stock bbox-center + hover adjustments below move the root to a point that the per-frame ghost foot-lock then re-slams back down -- b |
| 5059–5079 (n=21) | LLVOAvatar::updateRootPositionAndRotation | clones, composition | creative-candidate | unknown; conditional evidence present | Comment evidence: of the agent's physical representation of the agent's physical representation ([GhostClone] skip for ghosts: their object position is already the intended root -- see note above) [ClientHeight] local, render-only offset; after mInAir above, so footsteps / in-air state keep using  |
| 5142–5166 (n=25) | LLVOAvatar::updateRootPositionAndRotation | actor-path, clones, composition | creative-candidate | unknown; conditional evidence present | Comment evidence: [ClientHeight] WORLD-vertical lift; mRoot is seat-local here, so express it in the seat's frame (the camera's world-Z subtraction then matches exactly). [ActorMover] local ghost locomotion: when this avatar is being moved by the Actor Mover, override the rendered root pose AFTER  |
| 5216–5811 (n=596) | ALPosePolish::invalidateSkeleton, ALPosePolish::reset, ALPosePolish::run, ALPosePolish::runContact, ALPosePolish::runInertialization, ALPosePolish::runSecondary, LLVOAvatar::updateCharacter | actor-path, clones, director-shell, gaze, stepped-animation | creative-candidate | unknown; conditional evidence present | Comment evidence: ------------------------------------------------------------------------ [Machinima] Post-blend Pose Polish stage (alposepolish.h). Defined here because it operates on the full LLVOAvatar. Master gate ALPolishEnabled defaults false -> pure no-op, displayed pose bit-identical to t |
| 5820–5832 (n=13) | LLVOAvatar::updateCharacter | composition | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ClientHeight] Resolve the local height offset BEFORE computeNeedsUpdate(): a changed offset must force one update on a LOD-throttled / impostored avatar, and LLControlAvatar::computeNeedsUpdate() overwrites mNeedsImpostorUpdate with the wearer's value, so the flag is re-asserted |
| 5837–5848 (n=12) | LLVOAvatar::updateCharacter | clones, director-shell, gaze | creative-candidate | unknown; conditional evidence present | Comment evidence: Director still has to process release gates on LOD-skipped frames; otherwise a deselection/safety change can leave its root and local turn motion owned until the next detailed impostor update. |
| 5914–5961 (n=48) | LLVOAvatar::updateCharacter | actor-path, clones, composition, director-shell, gaze | creative-candidate | unknown; conditional evidence present | Comment evidence: [Machinima] Pose Polish: on the freshly blended pose, repair transition discontinuities and stabilize foot contact BEFORE the gaze layer paints on top, so gaze solves against a continuous, grounded pose. No-op unless ALPolishEnabled. See docs/pose_polish_integration_plan.md.  [Al |
| 5985–6092 (n=108) | LLVOAvatar::updateHeadOffset, LLVOAvatar::updateMouselookHeadBoneScale | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: updateHeadOffset() updateMouselookHeadBoneScale() [BDMerge B12] (EXPERIMENTAL) Scale down own head bone in mouselook. Donor: Black Dragon indra/newview/bdanimator.cpp (BDAnimator::update(), commit 605f6a7416 "Changed: (EXPERIMENTAL) Scale down head bones while in mouselook to pre |
| 6167–6179 (n=13) | LLVOAvatar::postPelvisSetRecalc | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge B12] Skip while our own head bone is force-scaled to zero for mouselook -- mHead's scale feeds directly into computeBodySize()'s height math and would otherwise shrink mBodySize for the duration of mouselook (the "feet clipping into the floor" regression BD fixed in 518b |
| 6280–6288 (n=9) | LLVOAvatar::updateVisibility | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Render suppression must not release the source mesh or pause fetch callbacks. |
| 6327–6375 (n=49) | LLScopedAvatarOuterModelView, LLVOAvatar::renderSkinned, ~LLScopedAvatarOuterModelView | capture-sidecar, clones | creative-candidate | unknown; conditional evidence present | Edits LLScopedAvatarOuterModelView, LLVOAvatar, gGL, LLRender, gGLModelView, LLMatrix4, LLDrawPoolAvatar, LLRenderPass |
| 6548–6556 (n=9) | LLVOAvatar::renderSkinned | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Comment evidence: [AlphaVel] The velocity target is replace-only; do not let classic skirt/hair/eyelash rendering override the stage's blend disable. |
| 6588–6595 (n=8) | LLVOAvatar::renderTransparent | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 6612–6618 (n=7) | LLVOAvatar::renderRigid | clones | creative-candidate | unknown; no local conditional evidence | Edits LLScopedAvatarOuterModelView |
| 6915–6921 (n=7) | LLVOAvatar::updateTextures | unknown | unknown-review | unknown; no local conditional evidence | Edits ALDerenderList |
| 6974–6982 (n=9) | LLVOAvatar::updateTextures | unknown | unknown-review | unknown; no local conditional evidence | Edits ALDerenderList |
| 7319–7330 (n=12) | LLVOAvatar::processSingleAnimationStateChange | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge B12] See postPelvisSetRecalc() above for why this is skipped while mouselook head-bone scaling is active. |
| 7332–7338 (n=7) | LLVOAvatar::processSingleAnimationStateChange | clones | creative-candidate | unknown; conditional evidence present | Edits gAudiop, gSavedSettings |
| 7366–7379 (n=14) | LLVOAvatar::processSingleAnimationStateChange | stepped-animation | creative-candidate | unknown; conditional evidence present | Comment evidence: Reflect consumes only live intent records. An expired STOP or no intent falls through to the stock original start in this same call. |
| 7394–7402 (n=9) | LLVOAvatar::processSingleAnimationStateChange | stepped-animation | creative-candidate | unknown; conditional evidence present | Edits ALReflectAnim |
| 7424–7461 (n=38) | LLVOAvatar::resetAnimations | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Stock: rebuild every active motion instance and restart it at its current time offset. Unchanged for normal (non-self) avatars. [Alchemy] Animesh: flushAllMotions() RESTARTS whatever is active, so a stranded local driver (poser default motions, an FSPosingMotion, a switchboard or |
| 7814–7820 (n=7) | LLVOAvatar::rebuildAttachmentOverrides | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 7988–7994 (n=7) | LLVOAvatar::addAttachmentOverridesForObject | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 8243–8249 (n=7) | LLVOAvatar::removeAttachmentOverridesForObject | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 8272–8278 (n=7) | LLVOAvatar::removeAttachmentOverridesForObject | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 8541–8552 (n=12) | LLVOAvatar::updateVisualParams | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge B12] See postPelvisSetRecalc() above for why this is skipped while mouselook head-bone scaling is active. |
| 8739–8844 (n=106) | LLVOAvatar::isActorGhostSystemReplayReady, LLVOAvatar::uploadActorGhostPalette | actor-path, clones, looks | creative-candidate | unknown; conditional evidence present | Comment evidence: Every weighted classic body mesh uses the same reference palette.  Pick the first built core mesh; doing the private lookup here avoids exposing mesh-LOD internals to ActorMover while keeping an interleaved alpha replay independent of whichever actor last used the shader program. |
| 9016–9022 (n=7) | LLVOAvatar::attachObject | stepped-animation | creative-candidate | unknown; no local conditional evidence | Edits ALAvatarAnimSpeed |
| 9417–9426 (n=10) | LLVOAvatar::sitOnObject | composition | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ClientHeight] sitOnObject runs outside the avatar update (and a LOD-skipped avatar never reaches the root writer that frame), so it carries the world-vertical term itself. |
| 9461–9475 (n=15) | LLVOAvatar::getOffObject | composition | creative-candidate | unknown; conditional evidence present | Comment evidence: [ClientHeight] The fallback becomes the drawable's position, so it must be the BASE (un-offset) cache; the root write below re-adds the local height offset. |
| 9482–9493 (n=12) | LLVOAvatar::getOffObject | composition | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ClientHeight] world write (the parent is null now): re-add the local height offset. |
| 9708–9721 (n=14) | LLVOAvatar::isVisible | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: This is update visibility. Actual draws/picks are gated independently. |
| 9874–9880 (n=7) | LLVOAvatar::logPendingPhasesAllAvatars | clones | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 9966–9973 (n=8) | LLVOAvatar::updateRuthTimer | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: A ghost's id is synthetic -- never emit a server avatar-textures request for it. |
| 10081–10092 (n=12) | LLVOAvatar::isTooComplex | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: A client-only ghost clone is never too-complex (it must render through the real path). Checked FIRST, before the isRlvSilhouette branch below. |
| 10096–10102 (n=7) | LLVOAvatar::isTooComplex | clones | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 10108–10125 (n=18) | LLVOAvatar::isTooComplex | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge Batch4] Machinima High-LOD: while filming, disable the complexity / surface-area auto-mute path (equivalent to RenderAvatarMaxComplexity=0 and RenderAutoMuteSurfaceAreaLimit=0) so distant/complex avatars stay full 3D. Manual mutes (AV_DO_NOT_RENDER / mute list, handled a |
| 10130–10136 (n=7) | LLVOAvatar::isTooComplex | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 10143–10154 (n=12) | LLVOAvatar::isTooSlow | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: A client-only ghost clone is never throttled as too-slow. |
| 10174–10184 (n=11) | LLVOAvatar::updateTooSlow | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: A client-only ghost clone never participates in ART / too-slow accounting. |
| 11314–11414 (n=101) | LLVOAvatar::copyAppearanceFrom | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: [GhostStudio] Copy a source avatar's appearance onto this one, BY VALUE.  The hazard this exists to avoid: applyParsedAppearanceMessage() below calls param->setWeight() / setAnimationTarget() on contents.mParams[i]. Those are pointers into whichever avatar PARSED the message. Han |
| 11516–11525 (n=10) | LLVOAvatar::applyParsedAppearanceMessage | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: (never for a client-only ghost -- its id is synthetic, no server object) |
| 11602–11628 (n=27) | LLVOAvatar::updateSkinInfoMatrixPalette | stepped-animation | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge A5.4-1b] Stash the outgoing palette as the previous frame's before rebuilding (velocity pass). swap() instead of copy: mGLMp is resized+rewritten just below, so this is allocation-free steady-state. |
| 11661–11942 (n=282) | LLVOAvatar::captureStepAnimRenderState, LLVOAvatar::clearStepAnimRenderState, LLVOAvatar::invalidateSkinPalettesAfterOverlay, LLVOAvatar::invalidateSkinPalettesForOverlay, LLVOAvatar::markStepAnimPoseChanged, LLVOAvatar::restoreStepAnimLabels, LLVOAvatar::shiftStepAnimLabels, LLVOAvatar::syncStepAnimAttachmentMatrices, LLVOAvatar::verifyStepAnimRenderState, void | stepped-animation | creative-candidate | unknown; conditional evidence present | Comment evidence: Retain normal spatial processing once per changed linkset. Child matrices are already current and must not enter updateMove(), where spinning children would dirty their spatial groups. |
| 12506–12512 (n=7) | LLVOAvatar::getUnbakedPixelAreaRank | clones | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 12529–12536 (n=8) | LLVOAvatar::cullAvatarsByPixelArea | clones | creative-candidate | unknown; no local conditional evidence | Edits ALDerenderList |
| 12543–12552 (n=10) | LLVOAvatar::cullAvatarsByPixelArea | clones | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 12830–12992 (n=163) | LLVOAvatar::getActorFxOwnerId, LLVOAvatar::getBaseRootPosition, LLVOAvatar::getClientHeightZ, LLVOAvatar::getEffectiveClientHeightZ, LLVOAvatar::hasEffectiveActorFx, LLVOAvatar::isHardVisualMute, LLVOAvatar::isImpostor, LLVOAvatar::refreshClientHeight, LLVOAvatar::resolveClientHeight, LLVOAvatar::shouldImpostor, refresh | actor-bar, actor-path, clones, composition, director-shell, looks | creative-candidate | unknown; conditional evidence present | Comment evidence: [ClientHeight] --------------------------------------------------------------- Cheap live approximation of the key; the real resolve runs only when the stamp says so (ALClientHeightModel::needsResolve). [/ClientHeight] ------------------------------------------------------------- |
| 12994–13005 (n=12) | LLVOAvatar::shouldImpostor | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge Batch4] Machinima High-LOD: never rank-impostor while filming. |
| 13089–13095 (n=7) | LLVOAvatar::idleUpdateRenderComplexity | clones | creative-candidate | unknown; no local conditional evidence | Edits LLPerfStats |
| 13128–13134 (n=7) | LLVOAvatar::updateNearbyAvatarCount | clones | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 13528–13545 (n=18) | LLVOAvatar::setOverallAppearanceNormal | looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: Control avatars do not rebuild the system-avatar skeleton, but they still need alternate-bind position/scale overrides from their live animated linkset when Actor FX lifts a performance-jelly state. Animation messages received while performance-jellied retain their signaled state |
| 13671–13678 (n=8) | LLVOAvatar::getOverallAppearance | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Keep the data path normal; the separate render gate hides every surface. |
| 13681–13687 (n=7) | LLVOAvatar::getOverallAppearance | clones | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 13691–13707 (n=17) | LLVOAvatar::getOverallAppearance | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: A styled cinematic actor needs its authored skeleton, animation, attachments, mesh visibility/extents and root placement. Treat it as normal only after the hard visibility policy above. |
| 13940–13946 (n=7) | LLVOAvatar::getTotalGPURenderTime | clones | creative-candidate | unknown; conditional evidence present | Edits LLVOAvatar |
| 13955–13961 (n=7) | LLVOAvatar::getMaxGPURenderTime | clones | creative-candidate | unknown; conditional evidence present | Edits LLVOAvatar |
| 13973–13981 (n=9) | LLVOAvatar::getAverageGPURenderTime | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: isTooSlow() now returns false for ghosts, so exclude them explicitly or a client-only clone would skew the average avatar GPU time. |
| 14008–14010 (n=3) | unknown/non-function (bool LLVOAvatar::isBuddy() const) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 2: `indra/newview/llvoavatar.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 31–37 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 56–63 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | actor-path, composition | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 213–245 (n=33) | getCollisionVolumeJoint, getCollisionVolumeJointCount, getCreationSerial, getJointOverrideEpoch, getSteppedSkeletonSerial | stepped-animation | creative-candidate | unknown; no local conditional evidence | Edits LLJoint, LLVector3 |
| 287–311 (n=25) | getUniformScale, isGhostAvatar | capture-sidecar, clones | creative-candidate | unknown; no local conditional evidence | Comment evidence: [GhostStudio] True if this is a scene-lit clone (no associated user). Unlike control/UI avatars this one has mIsDummy == false, so it takes the REAL render path; every site that assumes a server-backed identity (world enumeration, name cache, mute state, autotune, sounds, sim req |
| 326–332 (n=7) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 525–541 (n=17) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge B12] Experimental mouselook head-bone scaling. Donor: Black Dragon bdanimator.cpp (BDAnimator::update) + llagentcamera.cpp (commit 605f6a7416 and follow-ups). Reimplemented here without BD's Poser subsystem: applied/restored per-frame from idleUpdate() instead of on came |
| 550–594 (n=45) | unknown/non-function (public:) | actor-bar, actor-path, clones, composition, director-shell, looks, roto-tron | creative-candidate | unknown; no local conditional evidence | Comment evidence: Hard user/privacy suppression is stronger than cinematic Actor FX. The effective-style helper deliberately excludes performance jelly/impostor state so a styled actor can remain live without reviving hidden actors. Canonical resident/linkset key used by render hot paths. Unlike t |
| 614–624 (n=11) | setVisualMuteSettingsLocal | clones | creative-candidate | unknown; no local conditional evidence | Comment evidence: Set the local visual-mute state WITHOUT persisting it to LLRenderMuteList (unlike setVisualMuteSettings()). A client-only avatar with a synthetic id (LLGhostAvatar) uses this to undo the base ctor's saved-mute lookup -- persisting would write, and later read back, a STRANGER's sa |
| 647–659 (n=13) | unknown/non-function (public:) | clones, looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: Upload the classic 45-matrix system-avatar palette to the currently bound replay shader.  Shared Actor FX alpha can be interleaved long after the opaque sweep, so it cannot rely on that sweep's program uniforms. Conservative same-frame proof that every classic/system mesh the nat |
| 713–719 (n=7) | unknown/non-function (private:) | clones | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 948–968 (n=21) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge A5.4-1b] Previous frame's GL palette for the velocity pass (donor: Black Dragon llvoavatar.h MatrixPaletteCache). mLastFrame is the gFrameCount mLastGLMp was CURRENT for; consumers must treat it as valid only when mLastFrame == gFrameCount - 1 (contiguous), else the avat |
| 977–997 (n=21) | unknown/non-function (public:) | stepped-animation | creative-candidate | unknown; no local conditional evidence | Edits LLPointer, LLViewerObject, LLMatrix4, LLMatrix4a |
| 1024–1035 (n=12) | unknown/non-function (public:) | clones | creative-candidate | unknown; no local conditional evidence | Comment evidence: [GhostStudio] Copy shape + baked textures from another avatar BY VALUE. Never share the source's LLAppearanceMessageContents directly: applyParsedAppearanceMessage calls setWeight() on contents.mParams[i] (see llvoavatar.cpp), so a shared struct would mutate the SOURCE's LLVisual |
| 1170–1177 (n=8) | isTyping | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge F3] FS radar shows a typing indicator; mTyping is private (FS parity getter) |
| 1189–1198 (n=10) | setMoverContactHints | actor-path | creative-candidate | unknown; no local conditional evidence | Edits ALPosePolish |
| 1207–1217 (n=11) | unknown/non-function (private:) | actor-path | creative-candidate | unknown; no local conditional evidence | Comment evidence: [Machinima] Post-blend Pose Polish stage (continuity/grounding/secondary). Session-only, disabled by default; see alposepolish.h. |
| 1259–1275 (n=17) | unknown/non-function (private:) | actor-path, composition | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ClientHeight] mLastRootPos minus the effective client offset, captured on the same line. mLastRootPos stays VISUAL (name tag, voice); only getOffObject's standoff fallback (which becomes the drawable's position) needs the BASE value. [ClientHeight] cached own / effective offset  |
| 1440–1442 (n=3) | unknown/non-function (void dump_sequential_xml(const std::string outprefix, const LLSD& content);) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 2: `indra/newview/llvovolume.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 27–37 (n=11) | unknown/non-function (declaration, XML, build list or global scope) | stepped-animation, weather | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 69–75 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | rig | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 83–90 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | actor-bar, clones, director-shell | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 104–129 (n=26) | machinimaForceCullArea, machinimaForceSimpleRenderArea | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge Batch4] Machinima High-LOD: when BDMergeMachinimaHighLOD is on, relax the small-on-screen face degradation constants to 0 so small/distant faces keep full materials and aren't dropped from batching. Off = stock constants (byte-identical). |
| 622–647 (n=26) | LLVOVolume::animateTextures | rig, stepped-animation, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: Coverage changes even after mTexAnimMode is established. The shared region/tile scheduler coalesces these per-frame events. [ProbeOnDemand] TXa: texture animation is running (called for every instance every frame, independent of the camera) -> debounced motion. |
| 686–694 (n=9) | LLVOVolume::animateTextures | rig | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ProbeOnDemand] TXt: an animated-texture batch toggle is LOD-like churn, not a content change. |
| 800–816 (n=17) | LLVOVolume::updateTextureVirtualSize | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Hidden source faces never enter a draw batch, but all material channels must retain fetch demand for appearance/attachment cloning. |
| 910–918 (n=9) | LLVOVolume::updateTextureVirtualSize | rig | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ProbeOnDemand] TXt: size-driven texture-animation toggle (camera distance), not a content change. |
| 967–980 (n=14) | LLVOVolume::updateTextureVirtualSize | rig | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ProbeOnDemand] MS: a refinement of an already-built sculpt is LOD churn; a first build (current_discard < 0) stays an event. |
| 1324–1364 (n=41) | LLVOVolume::notifyMeshLoaded, LLVOVolume::probeNoteAssetArrival | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [ProbeOnDemand] MS: is this rebuild a pure refinement / LOD arrival of an asset this volume already built (same sculpt id + type, ready)? A first arrival or a SWAPPED asset returns false and stays a real geometry event. [ProbeOnDemand] MS: mesh LOD / refinement of an already-buil |
| 1569–1586 (n=18) | LLVOVolume::calcLOD | capture-sidecar, clones | creative-candidate | unknown; no local conditional evidence | Comment evidence: Ghost entity clones apply a render-only outer scale (LLClientOuterTransform, up to GHOST_SCALE_MAX) at draw time, but LOD is otherwise computed from unscaled native geometry -- so a large clone is driven at its 1x LOD and pops. Resolve the clone's own scale here (a scaled clone p |
| 1613–1638 (n=26) | LLVOVolume::calcLOD | capture-sidecar, clones | creative-candidate | unknown; conditional evidence present | Comment evidence: getLastAnimExtents() is already outer-scaled in LLVOAvatar::calculateSpatialExtents, so 'radius' above already reflects the clone scale -- do NOT scale it again (that would apply the scale squared). Just record the avatar's scale for the source-LOD gate below. e.g. a cloned anime |
| 1642–1655 (n=14) | LLVOVolume::calcLOD | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: Non-rigged clone prim: extents are native, so scale the LOD radius by the clone's own outer scale (guarded; own_scale == 1 for normal prims -> no-op, byte-identical). |
| 1705–1719 (n=15) | LLVOVolume::calcLOD | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge Batch4] BDMergeMachinimaHighLOD: force every volume to highest detail regardless of distance/zoom. This kills wide-shot LOD loss AND the LOD-change popping (cur_detail stops changing -> updateLOD's markRebuild stops firing). Reuses the proven HUD/selection forced-LOD pat |
| 1724–1761 (n=38) | LLVOVolume::calcLOD | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: RenderRiggedLODHysteresis: rigged radius/distance come from the avatar's animated extents, which are recomputed every few frames while it animates, so cur_detail can ping-pong across a threshold and rebuild/re-upload the whole rigged group each time. Increases in detail stay imme |
| 1794–1802 (n=9) | LLVOVolume::updateLOD | rig | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ProbeOnDemand] LV: a camera-distance LOD switch is not a content change. |
| 2250–2272 (n=23) | LLVOVolume::updateGeometry | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [ProbeOnDemand] H4: a pure LOD rebuild (no content flag, no REBUILD_POSITION) is camera-driven churn: tag the WHOLE function (including genBBoxes -> movePartition) so its notes are dropped. A changed volume invalidates the built-asset identity. Nothing below runs unless RenderPro |
| 2298–2313 (n=16) | LLVOVolume::updateGeometry | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [ProbeOnDemand] H4: entry bounds + whether this rebuild changes content. |
| 2371–2406 (n=36) | LLVOVolume::updateGeometry | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: Content changed: one discrete event over the entry + exit bounds. No content change but the bounds moved: motion. Built-asset identity (MS): ready only once the asset itself is loaded. |
| 2673–2685 (n=13) | LLVOVolume::setTEMaterialID | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: A client-only object's params are set on it directly; no sim ever gives it a material id, and resolving the null one it keeps would replace them with no material when its faces are deselected. |
| 3532–3565 (n=34) | LLVOVolume::updateSpotLightPriority | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge SpotStable] Stable projector shadows: the viewer has only two projector-shadow slots, awarded by on-screen pixel size - so panning the camera away reshuffles which lights hold shadows mid-shot. When enabled, priority is the light's WORLD size instead (radius cubed, so bi |
| 4870–4895 (n=26) | LLVOVolume::getManipPivotPositionAgent | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: Raise the edit-manip pivot to just above the invisible proxy box top so the constant-screen-size translate/rotate gizmo sits clearly OVER the ghost, not buried mid-body at large clone scales. Only the manipulator ORIGIN moves -- the proxy transform, bbox, and stretch-scale handle |
| 4975–4986 (n=12) | LLVOVolume::lineSegmentIntersect | weather | creative-candidate | unknown; conditional evidence present | Comment evidence: Shelter rays count only visible surfaces: client-only FORCE_INVISIBLE helpers (rig emitters, catchlight, live probe, manipulator proxies) never shelter rain. |
| 4994–5001 (n=8) | LLVOVolume::lineSegmentIntersect | weather | creative-candidate | unknown; conditional evidence present | Edits LLFloater, gFloaterTools, ALWeatherShelter |
| 5008–5015 (n=8) | LLVOVolume::lineSegmentIntersect | weather | creative-candidate | unknown; conditional evidence present | Edits LLToolMgr, ALWeatherShelter |
| 5076–5096 (n=21) | LLVOVolume::lineSegmentIntersect | weather | creative-candidate | unknown; conditional evidence present | Edits ALWeatherShelter |
| 5535–5542 (n=8) | can_batch_gltf_material | unknown | unknown-review | unknown; conditional evidence present | Edits LLGLTFMaterial, ALPHA_MODE_BLEND, LLPipeline |
| 5581–5588 (n=8) | can_batch_legacy_material | unknown | unknown-review | unknown; conditional evidence present | Edits LLMaterial, LLPipeline, ALPHA_MODE_BLEND |
| 5690–5776 (n=87) | resolve_actor_fx_owners, resolve_outer_transform | actor-bar, capture-sidecar, director-shell, looks | creative-candidate | unknown; conditional evidence present | Comment evidence: Native Actor FX is authored for the visible Director actor, not necessarily the avatar that skins this one face. Worn animated objects are skinned by an LLControlAvatar. Keep that exact runtime identity in LLDrawInfo even before the object joins the Director cast: cast membership |
| 5835–5844 (n=10) | LLVolumeGeometryManager::registerFace | capture-sidecar, looks | creative-candidate | unknown; no local conditional evidence | Edits LLClientOuterTransform |
| 5861–5867 (n=7) | LLVolumeGeometryManager::registerFace | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 5892–5931 (n=40) | LLVolumeGeometryManager::registerFace | unknown | unknown-review | unknown; conditional evidence present | Edits LLPipeline, LLRenderPass, ALPHA_MODE_MASK, ALPHA_MODE_BLEND, ALPHA_MODE_OPAQUE |
| 5941–5964 (n=24) | LLVolumeGeometryManager::registerFace | unknown | unknown-review | unknown; conditional evidence present | Edits LLDrawPool, LLRenderPass, LLPipeline, ALPHA_MODE_BLEND, LLMaterial, ALPHA_OVERRIDE_NONE, ALPHA_MODE_MASK |
| 5970–5979 (n=10) | LLVolumeGeometryManager::registerFace | unknown | unknown-review | unknown; no local conditional evidence | Edits LLPipeline, ALPHA_MODE_MASK |
| 5993–6000 (n=8) | LLVolumeGeometryManager::registerFace | capture-sidecar | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 6004–6020 (n=17) | LLVolumeGeometryManager::registerFace | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 6033–6051 (n=19) | LLVolumeGeometryManager::registerFace | sensor | creative-candidate | unknown; conditional evidence present | Edits LLDrawInfo |
| 6090–6113 (n=24) | LLVolumeGeometryManager::registerFace | capture-sidecar, looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: mVertexBuffer's TYPE_EMISSIVE allocation is group-wide. Preserve exact per-draw authored-glow ownership so shared Actor FX (and any other DrawInfo consumer) never classifies a neighbouring face's emissive stream as this batch's glow. |
| 6120–6130 (n=11) | LLVolumeGeometryManager::registerFace | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 6155–6169 (n=15) | LLVolumeGeometryManager::registerFace | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge A5.4-1a] velocity pass prev-object-matrix source (mirror BD llvovolume.cpp:5574). Rigid only; rigged faces (model_mat == null) get their previous transform from the palette cache in Phase 1b. |
| 6181–6198 (n=18) | LLVolumeGeometryManager::registerFace | looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge] Mark draws that belong to a genuinely worn attachment (rigged or not). getAvatarAncestor() excludes standalone animesh even though getAvatar() returns its control avatar. Drives the gated 3-pass alpha ordering in LLDrawPoolAlpha. |
| 6218–6233 (n=16) | LLVolumeGeometryManager::registerFace | unknown | unknown-review | unknown; no local conditional evidence | Edits LLPipeline, ALPHA_OVERRIDE_NONE, ALPHA_MODE_MASK, LLMaterial, ALPHA_MODE_BLEND |
| 6239–6255 (n=17) | LLVolumeGeometryManager::registerFace | looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: The pass is authoritative. Store the same effective policy for scalar legacy/PBR rendering, alpha-pool shader selection, shadows, velocity, and Actor FX replay. Shared material assets remain untouched. |
| 6259–6267 (n=9) | LLVolumeGeometryManager::registerFace | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 6400–6406 (n=7) | LLVolumeGeometryManager::rebuildGeom | unknown | unknown-review | unknown; conditional evidence present | Edits LLDrawable |
| 6411–6417 (n=7) | LLVolumeGeometryManager::rebuildGeom | unknown | unknown-review | unknown; no local conditional evidence | Edits ALDerenderList |
| 6566–6574 (n=9) | LLVolumeGeometryManager::rebuildGeom | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Preserve material fetches, animesh controllers, attachment joint overrides and face setup above; omit only draw geometry. |
| 6593–6618 (n=26) | LLVolumeGeometryManager::rebuildGeom | unknown | unknown-review | unknown; conditional evidence present | Edits LLGLTFMaterial, ALPHA_MODE_BLEND, LLPipeline, ALPHA_OVERRIDE_NONE, LLDrawPool, ALPHA_MODE_MASK |
| 6707–6714 (n=8) | LLVolumeGeometryManager::rebuildGeom | unknown | unknown-review | unknown; conditional evidence present | Edits LLGLTFMaterial, ALPHA_MODE_BLEND, LLPipeline |
| 6883–6936 (n=54) | apply_target_mask_vertex_coverage | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: TARGET_MASK coverage is texture alpha alone.  Bake that policy only into the affected face's vertex range: RGB remains untouched, while alpha becomes one. Legacy glow has no per-DrawInfo cutoff upload, so its otherwise-unused emissive RGB carries a 16-bit cutoff plus a marker for |
| 6988–6998 (n=11) | LLVolumeGeometryManager::rebuildMesh | unknown | unknown-review | unknown; no local conditional evidence | Edits LLPipeline |
| 7168–7201 (n=34) | LLVolumeGeometryManager::genDrawInfo | looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: sIndexedGLTFChannels distinct materials into one batch, assigning each face a material slot via setTextureIndex. The shader selects per-vertex by that slot. See LLRenderPass::pushGLTFBatchIndexed. Static/native GBuffer PBR retains the full indexed material width. Rigged PBR is al |
| 7208–7227 (n=20) | LLVolumeGeometryManager::genDrawInfo | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: splitting after slot assignment would leave sparse indexed material lists that shared replay must reject |
| 7236–7243 (n=8) | LLVolumeGeometryManager::genDrawInfo | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 7251–7257 (n=7) | LLVolumeGeometryManager::genDrawInfo | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 7292–7298 (n=7) | LLVolumeGeometryManager::genDrawInfo | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 7305–7334 (n=30) | LLVolumeGeometryManager::genDrawInfo | unknown | unknown-review | unknown; conditional evidence present | Edits LLPipeline, ALPHA_OVERRIDE_NONE, LLMaterial, ALPHA_MODE_MASK, ALPHA_MODE_BLEND |
| 7336–7342 (n=7) | LLVolumeGeometryManager::genDrawInfo | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 7350–7363 (n=14) | LLVolumeGeometryManager::genDrawInfo | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: per-DrawInfo authored-glow batch split |
| 7372–7380 (n=9) | LLVolumeGeometryManager::genDrawInfo | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 7390–7396 (n=7) | LLVolumeGeometryManager::genDrawInfo | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 7621–7627 (n=7) | LLVolumeGeometryManager::genDrawInfo | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 7630–7645 (n=16) | LLVolumeGeometryManager::genDrawInfo | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge G2.3 fix] Only an explicit per-target MASK policy owns pass routing independently of TE alpha. getGeometryVolume() baked TE/base-colour alpha into this range. Remove it only for TARGET_MASK so every mask shader tests texture alpha alone; all other faces incur no vertex r |
| 7648–7660 (n=13) | LLVolumeGeometryManager::genDrawInfo | unknown | unknown-review | unknown; no local conditional evidence | Edits LLDrawPool, LLPipeline, ALPHA_OVERRIDE_NONE, ALPHA_MODE_BLEND, LLMaterial |
| 7664–7682 (n=19) | LLVolumeGeometryManager::genDrawInfo | unknown | unknown-review | unknown; conditional evidence present | Edits LLMaterial, LLPipeline, ALPHA_OVERRIDE_NONE, ALPHA_MODE_MASK, ALPHA_MODE_BLEND |
| 7685–7691 (n=7) | LLVolumeGeometryManager::genDrawInfo | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 7696–7707 (n=12) | LLVolumeGeometryManager::genDrawInfo | unknown | unknown-review | unknown; conditional evidence present | Edits LLGLTFMaterial, ALPHA_MODE_BLEND, LLPipeline, ALPHA_MODE_MASK |
| 7717–7725 (n=9) | LLVolumeGeometryManager::genDrawInfo | unknown | unknown-review | unknown; conditional evidence present | Edits LLMaterial |
| 7751–7762 (n=12) | LLVolumeGeometryManager::genDrawInfo | roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge G2.3 fix] Forced MASK material faces must reach the material-mask table even with TE alpha or legacy bump set. |
| 7794–7800 (n=7) | LLVolumeGeometryManager::genDrawInfo | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 7821–7827 (n=7) | LLVolumeGeometryManager::genDrawInfo | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 7849–7863 (n=15) | LLVolumeGeometryManager::genDrawInfo | unknown | unknown-review | unknown; conditional evidence present | Edits LLPipeline, ALPHA_OVERRIDE_NONE, ALPHA_MODE_MASK, LLRenderPass |
| 7934–7940 (n=7) | LLVolumeGeometryManager::genDrawInfo | unknown | unknown-review | unknown; conditional evidence present | Edits LLMaterial |
| 7957–7964 (n=8) | LLVolumeGeometryManager::genDrawInfo | unknown | unknown-review | unknown; conditional evidence present | Edits LLMaterial |
| 8055–8061 (n=7) | LLGeometryManager::addGeometryCount | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 2: `indra/newview/pipeline.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 7–13 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 28–67 (n=40) | unknown/non-function (declaration, XML, build list or global scope) | clones, looks, rig, roto-tron, sensor, shafts-volumetrics, vcam, weather | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 84–100 (n=17) | unknown/non-function (declaration, XML, build list or global scope) | composition, rig | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 102–109 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | actor-bar, camera, director-shell, roto-tron | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 120–130 (n=11) | unknown/non-function (declaration, XML, build list or global scope) | capture-sidecar, clones, shafts-volumetrics, vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 136–149 (n=14) | unknown/non-function (declaration, XML, build list or global scope) | clones, roto-tron | creative-candidate | unknown; no local conditional evidence | Edits LLCharacter |
| 156–167 (n=12) | unknown/non-function (declaration, XML, build list or global scope) | actor-path, clones | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 179–185 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 217–232 (n=16) | unknown/non-function (declaration, XML, build list or global scope) | sensor, weather | creative-candidate | unknown; no local conditional evidence | Edits ALWeatherModel, ALWeatherShelter |
| 294–325 (n=32) | unknown/non-function (bool LLPipeline::RenderDeferredAtmospheric;) | vcam | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ShadowDist P1] [ShadowDist P2] [ShadowDist P3] |
| 327–432 (n=106) | unknown/non-function (F32 LLPipeline::CameraMaxCoF;) | capture-sidecar, rig, shafts-volumetrics | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge G3.2] volumetric lighting (donor: Black Dragon) [BDMerge G3.3] per-projector volumetric light cones (visible spotlight shafts) [BDMerge G3.3 ConservativeShadow] airborne-march shadow sampler A/B gate (default on). [BDMerge G3.3 Dust] baked 64^3 dust-volume particulate br |
| 453–459 (n=7) | unknown/non-function (extern bool gDisplaySwapBuffers;) | unknown | unknown-review | unknown; no local conditional evidence | Edits gAvatarSilhouetteRimParityCompiled |
| 462–471 (n=10) | unknown/non-function (LLPipeline gPipeline;) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLStaticHashedString |
| 476–535 (n=60) | shadow_dist_ortho_coef, shadow_dist_persp_coef | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [ShadowDist P1] shadowUtil.glsl / blurLightF.glsl uniforms (hashed-string uploads; every one carries a GLSL initialiser equal to its stock value so a program that never receives it stays stock). [ShadowDist P2] Control C uniforms (shadowUtil.glsl) [ShadowDist P3] Control B column |
| 555–563 (n=9) | unknown/non-function (bool    LLPipeline::sRenderSoundBeacons = false;) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [F4] see pipeline.h; donor: Firestorm pipeline.cpp LLPipeline::sLastFocusPoint / sDoFEnabled |
| 567–637 (n=71) | LLPipeline::countShadowAlphaCapDraw, ShadowDrawFilterScope, alphaCapDebug, ~AlphaCapSunLeakCheck, ~ShadowDrawFilterScope | rig, vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: [AlphaShadowCap] No per-draw diagnostic work unless the cap filter is active. Check all exits, including the early-return paths of generateSunShadow. |
| 661–683 (n=23) | addDeferredAttachments | unknown | unknown-review | unknown; conditional evidence present | Edits LLCachedControl, gSavedSettings |
| 712–727 (n=16) | LLPipeline::LLPipeline | shafts-volumetrics | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge G3.3 Batch 1 A] identity so the first temporal-resolve upload is well-defined (it is ignored that frame anyway - history is invalid). [BDMerge Froxel F3] same: identity prev-modelview so the first froxel-temporal upload is well-defined (ignored that frame - history is in |
| 737–751 (n=15) | bdmergeMaxSpotShadows | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge NSpot] runtime projector-shadow slot count (2 = stock, up to LLPipeline::MAX_SPOT_SHADOWS). Changing reallocates via handleShadowsResized. |
| 819–825 (n=7) | LLPipeline::init | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 918–949 (n=32) | LLPipeline::init | vcam | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ShadowDist P1] [ShadowDist P2] [ShadowDist P3] |
| 951–1038 (n=88) | LLPipeline::init | capture-sidecar, shafts-volumetrics | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge G3.2] [BDMerge G3.3] [BDMerge G3.3 ConservativeShadow + Dust] [BDMerge Froxel F0] hybrid froxel volumetrics grid |
| 1058–1064 (n=7) | LLPipeline::init | roto-tron | creative-candidate | unknown; no local conditional evidence | Edits ALActorMaskDiagnostics |
| 1204–1219 (n=16) | LLPipeline::resizeScreenTexture | rig, weather | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1223–1233 (n=11) | LLPipeline::allocateScreenBuffer | rig | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1297–1312 (n=16) | LLPipeline::allocateScreenBufferInternal | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [ProbeCost] Keep the stock aux pack for previews. |
| 1341–1359 (n=19) | LLPipeline::allocateScreenBufferInternal | rig, roto-tron | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ActorMask] Detach before deferredScreen.allocate destroys its depth. [RelightQ] Invalidate with this pack, including resolution divisor changes. |
| 1362–1422 (n=61) | LLPipeline::allocateScreenBufferInternal | capture-sidecar, looks, rig, roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: Visible-diffuse sidecar: a second colour attachment capturing forward surfaces' diffuse in the same draw calls as the beauty pass.  MAIN VIEW ONLY. mRT follows the reflection-probe/hero swap, so without this check the aux and hero packs also get the attachment -- and they then pa |
| 1427–1441 (n=15) | LLPipeline::allocateScreenBufferInternal | capture-sidecar | creative-candidate | unknown; conditional evidence present | Comment evidence: Post ping-pong precision. Default 10-bit (RGB10_A2); RenderHDR16BitColor promotes the whole post chain (postPing/Pong + FXAA/SMAA below) to fp16 (RGBA16F) to kill banding in heavy grades, at a real per-pass bandwidth cost that is why it is opt-in rather than the default. Read her |
| 1475–1522 (n=48) | LLPipeline::allocateScreenBufferInternal | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Propagate allocation failure (like the FXAA/SMAA path below) so the caller's lower-resolution retry can run. Matters more now that the fp16 post option doubles these to 8 bytes/pixel: a VRAM-tight 4K+ user must fall back rather than hit an FBO-less bindTarget assert. [ActorPath]  |
| 1533–1545 (n=13) | LLPipeline::allocateScreenBufferInternal | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: FXAA packs luma into ALPHA (glowcombineFXAAF -> fxaaF with FXAA_GREEN_AS_LUMA 0 reads rgba.w). RGB10_A2 has a 2-bit alpha, quantising luma to 4 levels and crippling edge detection, so the FXAA prep target alone gets 16-bit unorm (same 0..1 clamp, finer RGB, 16-bit luma). SMAA, fp |
| 1555–1660 (n=106) | LLPipeline::allocateScreenBufferInternal | capture-sidecar | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge A5.4-1a] Velocity / motion-vector buffer (GL_RG16F). Shares the deferred screen's depth buffer so the velocity geometry pass depth-tests against the already-rendered opaque scene (mirror BD pipeline.cpp:1046). Default OFF -> released -> zero extra cost/behavior. [A5.4-3] |
| 1692–30816 (n=29125) | ALWorldShadow::cachedNear, ALWorldShadow::updateResidencyPicked, BlurHappySize, IR, LLOctreeDirtyTexture, LLPipeline::FinalizeGuides::operator, LLPipeline::acquirePrismLensScratch, LLPipeline::acquireVCamVolumetrics, LLPipeline::activatePrismAuxiliaryProbeState, LLPipeline::actorMaskParityPathInconclusiveReason, LLPipeline::actorMaskViewStamp, LLPipeline::actorOnlyVolumetricLightCount, LLPipeline::addObject, LLPipeline::addPool, LLPipeline::addToQuickLookup, LLPipeline::addTrianglesDrawn, LLPipeline::allocDrawable, LLPipeline::allocatePrismLensOutput, LLPipeline::allocatePrismPreviewScratch, LLPipeline::allocateShadowBuffer, LLPipeline::appendCinematicProbeLightSignature, LLPipeline::applyCAS, LLPipeline::applyEnvOnlyDraw, LLPipeline::applyFXAA, LLPipeline::applyLensSoftEdge, LLPipeline::applyOnLensFilters, LLPipeline::applySMAA, LLPipeline::applyShadowUnitsFiltering, LLPipeline::areaShadowsActive, LLPipeline::areaShadowsAllowedThisView, LLPipeline::assertInitializedDoError, LLPipeline::beginCinematicProbeCapture, LLPipeline::beginPrismAuxiliaryState, LLPipeline::bindActorMask, LLPipeline::bindDeferredShader, LLPipeline::bindDeferredShaderFast, LLPipeline::bindLightFunc, LLPipeline::bindPrismLensTarget, LLPipeline::bindPrismPreviewScratch, LLPipeline::bindRigRimAux, LLPipeline::bindScreenToTexture, LLPipeline::bindShadowMaps, LLPipeline::calcNearbyLights, LLPipeline::calcPixelArea, LLPipeline::canUseAntiAliasing, LLPipeline::canUseInterleavedAlpha, LLPipeline::canUseWindLightShaders, LLPipeline::checkReferences, LLPipeline::clearAwayWorldShadowPicks, LLPipeline::clearGoboOverride, LLPipeline::clearPrismLensDirtyScreenShaderTracking, LLPipeline::clearProjectorShadowSoftness, LLPipeline::clearRebuildDrawables, LLPipeline::clearRebuildGroups, LLPipeline::clearReferences, LLPipeline::clearShadowUniformStamps, LLPipeline::clearVolumetricShaftOverride, LLPipeline::clearVolumetricShafts, LLPipeline::clearWorldShadowPicks, LLPipeline::collectRotoInkCandidates, LLPipeline::colorCorrect, LLPipeline::colorCorrectPermutation, LLPipeline::colorCorrectProgram, LLPipeline::colorCorrectWillApplyExposure, LLPipeline::combineGlow, LLPipeline::compositeBloomHDR, LLPipeline::compositeSensorOverlay, LLPipeline::copyRenderTarget, LLPipeline::copyScreenSpaceReflections, LLPipeline::createGLBuffers, LLPipeline::createLUTBuffers, LLPipeline::createObject, LLPipeline::createObjects, LLPipeline::deferredLightShape, LLPipeline::dirtyPoolObjectTextures, LLPipeline::dirtyPrismProjectorFeed, LLPipeline::disableLights, LLPipeline::doOcclusion, LLPipeline::effectiveShadowSplitBlend, LLPipeline::enableLights, LLPipeline::enableLightsAvatar, LLPipeline::enableLightsAvatarEdit, LLPipeline::enableLightsDynamic, LLPipeline::enableLightsFullbright, LLPipeline::enableLightsPreview, LLPipeline::enableShadows, LLPipeline::endCinematicProbeCapture, LLPipeline::endPrismAuxiliaryState, LLPipeline::ensureActorMask, LLPipeline::ensureLensPyramid, LLPipeline::ensurePrismProjectorFeed, LLPipeline::ensureRigRimAux, LLPipeline::ensureRigRimSmooth, LLPipeline::ensureRigRimWrap, LLPipeline::ensureSensorTargets, LLPipeline::ensureSilCover, LLPipeline::feedProjectorVolumetricBloom, LLPipeline::feedProjectorVolumetricBloomClassic, LLPipeline::finalizeGhostRiggedSolidCoverage, LLPipeline::findPool, LLPipeline::findReferences, LLPipeline::flushRelightTakes, LLPipeline::forAllVisibleDrawables, LLPipeline::gatherRotoInkTargets, LLPipeline::generateBloomHDR, LLPipeline::generateExposure, LLPipeline::generateGlow, LLPipeline::generateLuminance, LLPipeline::generateSMAABuffers, LLPipeline::getAlphaMaskCutoffOverride, LLPipeline::getAlphaModeOverride, LLPipeline::getGhostDeferredCoverageThisFrame, LLPipeline::getGoboOverride, LLPipeline::getPool, LLPipeline::getPoolFromTE, LLPipeline::getPoolTypeFromTE, LLPipeline::getProjectorShadowSoftness, LLPipeline::getRenderBeacons, LLPipeline::getRenderHighlights, LLPipeline::getRenderMOAPBeacons, LLPipeline::getRenderParticleBeacons, LLPipeline::getRenderPhysicalBeacons, LLPipeline::getRenderScriptedBeacons, LLPipeline::getRenderScriptedTouchBeacons, LLPipeline::getRenderSoundBeacons, LLPipeline::getRenderWaterHeight, LLPipeline::getSpatialPartition, LLPipeline::getVisibleExtents, LLPipeline::getVolumetricShaftOverride, LLPipeline::getWaterDisTarget, LLPipeline::getWaterExclusionMaskTarget, LLPipeline::ghostPostDeferredBlendPending, LLPipeline::ghostPostDeferredSolidsPending, LLPipeline::grabReferences, LLPipeline::hasAlphaOverrides, LLPipeline::hasGoboOverride, LLPipeline::hasRenderTypeControl, LLPipeline::hasVCamVolumetricOverride, LLPipeline::hasVolumetricShaftOverride, LLPipeline::inkExposureSelector, LLPipeline::invalidateWeatherSpatial, LLPipeline::isHeroProjector, LLPipeline::isLiveMainAreaShadowPass, LLPipeline::isProjectorNoShadow, LLPipeline::isProjectorShadowSuppressed, LLPipeline::isVCamVolumetricAutoEnabled, LLPipeline::isVCamVolumetricDeclined, LLPipeline::isVolumetricShaftEnabled, LLPipeline::isWaterClip, LLPipeline::isWorldShadowPicked, LLPipeline::lineSegmentIntersectInHUD, LLPipeline::lineSegmentIntersectInWorld, LLPipeline::lineSegmentIntersectParticle, LLPipeline::loadProjVolDustMap, LLPipeline::logRelightTake, LLPipeline::manualWorldShadowPickList, LLPipeline::markGLRebuild, LLPipeline::markMeshDirty, LLPipeline::markMoved, LLPipeline::markNotCulled, LLPipeline::markOccluder, LLPipeline::markPartitionMove, LLPipeline::markRebuild, LLPipeline::markShift, LLPipeline::markTextured, LLPipeline::markVisible, LLPipeline::materializeDiopterPreset, LLPipeline::materializeKaleidoPreset, LLPipeline::nativeSpotShadowClip, LLPipeline::noteAreaDepthLight, LLPipeline::noteVolumetricShaftEdit, LLPipeline::popRenderDebugFeatureMask, LLPipeline::postSort, LLPipeline::prepareDirtyPrismProjectorFeeds, LLPipeline::prepareGatePrismProjectorFeeds, LLPipeline::prepareMediaPrismProjectorFeeds, LLPipeline::preparePrismProjectorFeeds, LLPipeline::preserveReShadeGBuffer, LLPipeline::probeShouldRenderLight, LLPipeline::processPartitionQ, LLPipeline::projectorDetailShadows, LLPipeline::projectorShaftCostReadout, LLPipeline::pushRenderDebugFeatureMask, LLPipeline::rebaseWeather, LLPipeline::rebuildPools, LLPipeline::rebuildPriorityGroups, LLPipeline::recordTrianglesDrawn, LLPipeline::refreshCachedSettings, LLPipeline::releaseActorMasks, LLPipeline::releaseGLBuffers, LLPipeline::releaseLUTBuffers, LLPipeline::releaseLensPyramid, LLPipeline::releasePrismLensBuffers, LLPipeline::releasePrismLensLargeScratch, LLPipeline::releasePrismLensOutput, LLPipeline::releasePrismLensOutputs, LLPipeline::releasePrismPreviewScratch, LLPipeline::releasePrismProjectorFeed, LLPipeline::releasePrismProjectorFeeds, LLPipeline::releaseRigRimAux, LLPipeline::releaseScreenBuffers, LLPipeline::releaseSensorTargets, LLPipeline::releaseShadowBuffers, LLPipeline::releaseSilCovers, LLPipeline::releaseSpotShadowTargets, LLPipeline::releaseSunShadowTarget, LLPipeline::releaseSunShadowTargets, LLPipeline::releaseUnusedPrismAreaTargets, LLPipeline::releaseVCamVolumetrics, LLPipeline::removeFromQuickLookup, LLPipeline::removeMutedAVsLights, LLPipeline::removePool, LLPipeline::renderActorMask, LLPipeline::renderActorPathGuides, LLPipeline::renderAlphaObjects, LLPipeline::renderAreaShadows, LLPipeline::renderCineFisheye, LLPipeline::renderCineOutline, LLPipeline::renderCompositionGuideOverlay, LLPipeline::renderDebug, LLPipeline::renderDeferredLighting, LLPipeline::renderDoF, LLPipeline::renderFinalize, LLPipeline::renderFinalizeLegacy, LLPipeline::renderFinalizePresent, LLPipeline::renderFinalizeScene, LLPipeline::renderFocusPoint, LLPipeline::renderFroxelVolumetrics, LLPipeline::renderFullbrightMaskedObjects, LLPipeline::renderGLTFObjects, LLPipeline::renderGeomDeferred, LLPipeline::renderGeomPostDeferred, LLPipeline::renderGeomShadow, LLPipeline::renderGeomVelocity, LLPipeline::renderGhostDeferredOpaqueMasked, LLPipeline::renderGhostPostDeferred, LLPipeline::renderGhostRiggedBlend, LLPipeline::renderGhostSilCover, LLPipeline::renderHighlights, LLPipeline::renderMaskedObjects, LLPipeline::renderMotionBlurComposite, LLPipeline::renderObjects, LLPipeline::renderPhysicsDisplay, LLPipeline::renderProjectorVolumetric, LLPipeline::renderProjectorVolumetricClassic, LLPipeline::renderProjectorVolumetricNew, LLPipeline::renderRetainedPrismProjectorFeed, LLPipeline::renderRigRimWrap, LLPipeline::renderSelectedFaces, LLPipeline::renderSensor, LLPipeline::renderSilCover, LLPipeline::renderTronWorld, LLPipeline::renderUltimateDiopter, LLPipeline::renderUltimateKaleidoscope, LLPipeline::renderVelocityDebug, LLPipeline::renderVolumetric, LLPipeline::renderWeather, LLPipeline::renderWeatherParticles, LLPipeline::resetDrawOrders, LLPipeline::resetFrameStats, LLPipeline::resetShadowSlotHistory, LLPipeline::resetShadowSlotTimers, LLPipeline::resetVertexBuffers, LLPipeline::resolveAlphaMaskCutoff, LLPipeline::resolveAlphaMode, LLPipeline::resolveAlphaPolicy, LLPipeline::resolveRotoInkLayer, LLPipeline::resolveShadowSubject, LLPipeline::restoreGL, LLPipeline::selectForwardLightSet, LLPipeline::sensorAAEncode, LLPipeline::sensorDiagnosticView, LLPipeline::sensorUnavailableReason, LLPipeline::sensorVignette, LLPipeline::sensorWarpUniforms, LLPipeline::setAlphaMaskCutoffOverride, LLPipeline::setAlphaModeOverride, LLPipeline::setGoboOverride, LLPipeline::setLight, LLPipeline::setPrismAuxiliaryWaterHeight, LLPipeline::setProjectorDetailShadows, LLPipeline::setProjectorShadowSoftness, LLPipeline::setRenderBeacons, LLPipeline::setRenderDebugFeatureControl, LLPipeline::setRenderHighlightTextureChannel, LLPipeline::setRenderHighlights, LLPipeline::setRenderMOAPBeacons, LLPipeline::setRenderParticleBeacons, LLPipeline::setRenderPhysicalBeacons, LLPipeline::setRenderScriptedBeacons, LLPipeline::setRenderScriptedTouchBeacons, LLPipeline::setRenderSoundBeacons, LLPipeline::setRigWorldShadowPick, LLPipeline::setVolumetricShaftOverride, LLPipeline::setWorldShadowPick, LLPipeline::setWorldShadowPicks, LLPipeline::setupAvatarLights, LLPipeline::setupGradingLUT, LLPipeline::setupHWLights, LLPipeline::setupSpotLightVolumetricClassic, LLPipeline::shadersLoaded, LLPipeline::shadowUnitsFilterWanted, LLPipeline::shiftObjects, LLPipeline::shouldRunRotoInkAt, LLPipeline::sortAlphaGroupsForInterleaving, LLPipeline::spotShadowMapSampled, LLPipeline::stateSort, LLPipeline::suspendWeather, LLPipeline::toggleHeroProjector, LLPipeline::toggleProjectorCastShadows, LLPipeline::toggleRenderBeacons, LLPipeline::toggleRenderDebug, LLPipeline::toggleRenderDebugControl, LLPipeline::toggleRenderDebugFeature, LLPipeline::toggleRenderDebugFeatureControl, LLPipeline::toggleRenderHighlights, LLPipeline::toggleRenderMOAPBeacons, LLPipeline::toggleRenderParticleBeacons, LLPipeline::toggleRenderPhysicalBeacons, LLPipeline::toggleRenderScriptedBeacons, LLPipeline::toggleRenderScriptedTouchBeacons, LLPipeline::toggleRenderSoundBeacons, LLPipeline::toggleRenderType, LLPipeline::toggleRenderTypeControl, LLPipeline::toggleRenderTypeControlNegated, LLPipeline::toggleVolumetricShaft, LLPipeline::toggleWorldShadowPick, LLPipeline::unlinkDrawable, LLPipeline::unloadShaders, LLPipeline::updateCineFlareEnergy, LLPipeline::updateCull, LLPipeline::updateDrawableGeom, LLPipeline::updateGL, LLPipeline::updateGeom, LLPipeline::updateMove, LLPipeline::updateMoveDampedAsync, LLPipeline::updateMoveNormalAsync, LLPipeline::updateMovedList, LLPipeline::updateNightMaskAnchor, LLPipeline::updateRenderTransparentWater, LLPipeline::updateShadowEngaged, LLPipeline::uploadEnvOnlyView, LLPipeline::uploadPrismPreviewColorCorrectUniforms, LLPipeline::uploadShadowAlphaMode, LLPipeline::uploadShadowUniforms, LLPipeline::verify, LLPipeline::visibleObjectsInFrustum, LLPipeline::visualizeBuffers, LLPipeline::worldChannelOwnsPickedLight, LLPipeline::worldHDREnabled, LLPipeline::worldPresentationPermutation, LLPipeline::worldShadowOwned, LLPipeline::worldShadowPickChatLines, LLPipeline::worldShadowPickList, LLPipeline::worldShadowPickReport, LLPipeline::worldShadowPickRevision, LLPipeline::worldShadowReadout, LLPipeline::worldShadowResidentEligible, LLPrismLens::ensureProjectorOutputCapacity, ProjVolAuxStateGuard, ProjectionStackGuard, ScopedActorMaskState, ScopedGhostColorMask, ScopedGhostTransform, ScopedSilCoverInvariant, ScopedSilCoverState, State, VelocityModelMatrixUpdateGuard, alDiopterResolveBaseFocus, alDiopterResolveLook, alKaleidoFocusUV, alKaleidoResolveLook, alKaleidoResolveMotion, alKaleidoSpinDeg, apply, applyParityMaskBlend, apply_cube_face_rotation, area_source_radius, bdmerge_alpha_owner_avatar, bdmerge_dust_wind_dir, bdmerge_rebuild_for_alpha_target, bdmerge_should_render_light, bdmerge_should_render_projector, bdmerge_snapshot_autoscale_multiplier, bindHighlightProgram, calc_light_dist, capture, cg_apply_duv, cg_cct_to_xy, cg_compute_white_balance_gain, check_references, classifyGhostBatchPhase, debugLightingTerm, diopter_drift, diopter_hash, diopter_noise1, drawActorMaskBatch, drawSilCoverBatch, forAllDrawables, froxelShaftInputsDiffer, getControl, ghostMaterialShaderIndex, globalFroxelShaftInputs, kalFract, kalHashCell, kalNoise1, lerpf, light_shape_enabled, light_shape_for_volume, light_shape_master_enabled, local_light_selection_color, log_world_shadow_impl, parityMaskBlendSupported, pow3f, pow4fsrgb, probe_tiny_cull_pass_ok, projvolAdaptiveResolution, projvolCameraMargin, projvolClipPyramidBounds, projvolConeDefinition, projvolCoverage, projvolDrawFullResRim, projvolDrawRimTexture, projvolEffectiveDefinition, projvolFullCostIsUnclamped, projvolPyramidBounds, projvolPyramidGeometry, projvolSoftBudgetAllowed, projvolSphereBounds, projvolStepBudget, projvolUploadRecordingUniforms, projvolUploadStepBudget, projvolVariantReady, pushGhostBatch, pushGhostBumpBatch, pushGhostFullbrightShinyBatch, pushGhostGLTFBatch, pushGhostGLTFBatchIndexed, pushGhostMaterialBatch, refresh, release_prism_scratch_entry, renderMOAPBeacons, renderParticleBeacons, renderPhysicalBeacons, renderScriptedBeacons, renderScriptedTouchBeacons, renderSoundHighlights, render_hud_elements, saveShaderTextures, saveTexture, setVCamVolumetricMaster, setup_ghost_texture_matrix, silCoverBlendCacheMatches, silCoverBlendFactor, silCoverFor, silCoverPrivate, std::max, std::sort, std::stable_sort, teardown_ghost_texture_matrix, unbindHighlightProgram, update, uploadDebugLightingTerm, uploadSLogCaptureUniforms, uploadSilhouetteFrame, validate_framebuffer_object, visit, worldShadowLightNear, worldShadowLightNearUncached, world_shadow_eligible, world_shadow_extents, world_shadow_label, world_shadow_pick_labels, ~FlushCover, ~FlushMask, ~ProjVolAuxStateGuard, ~ProjectionStackGuard, ~ResetActorMaskView, ~ScopedActorMaskState, ~ScopedGhostColorMask, ~ScopedGhostTransform, ~ScopedSilCoverInvariant, ~ScopedSilCoverState, ~SensorScope, ~State, ~VelocityModelMatrixUpdateGuard | actor-bar, actor-path, camera, capture-sidecar, clones, composition, director-shell, environment, looks, rig, roto-tron, sensor, shafts-volumetrics, vcam, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: must be even to avoid a stripe in the horizontal shadow blur Release every GL target owned by one Prism scratch pool entry and zero its size key so acquirePrismLensScratch treats it as empty. File-local so no caller outside the acquire/release pair can partially tear down an entr |
| 30857–30886 (n=30) | LLPipeline::renderDeferredLighting, LLPrismPreviewPolicy::clearPreviewAO | vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: The pinned preview pack is reused. Seed its AO/direct-light map to neutral white on every preview before the SSAO/shadow dispatch gates, so a sentinel or a previous preview can never darken this capture. |
| 30891–30901 (n=11) | LLPipeline::renderDeferredLighting | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 30908–30923 (n=16) | LLPipeline::renderDeferredLighting | vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: Cascade colours and raw R/G are producer diagnostics; preserve the sun pass exactly instead of running the normal two-pass light blur. |
| 30929–30941 (n=13) | LLPipeline::renderDeferredLighting | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge A1.2] scale the shared shadow+SSAO soften blur kernel so a high-res snapshot keeps the same blur weighting it had at window resolution; this pass softens both the sun shadow and (when RenderDeferredSSAO is on, as gated above) SSAO together, matching BD's donor which scal |
| 30948–31377 (n=430) | LLPipeline::renderDeferredLighting, std::isfinite | actor-bar, capture-sidecar, director-shell, rig, roto-tron, sensor, vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: [ShadowDist P1] Control D: sun-shadow channel softening split from the SSAO blur (design v2 s5, v3 s6, v4 s4, v5 s4). Mode 0 = stock (the shader's original path runs untouched). Uniforms persist in the program across the second (vertical) bind below. reference-pixel sigma: scaled |
| 31392–31429 (n=38) | LLPipeline::renderDeferredLighting | rig, roto-tron, vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: The diagnostic replaces opaque lighting; do not add punctual lights over its display-space result after the soften pass. [RigRim] Round-2 P2 fix: read once per call so the point-light push and the multi-light batch pack/upload below can both skip their rim-specific work when Rig  |
| 31450–31455 (n=6) | LLPipeline::renderDeferredLighting | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 31457–31509 (n=53) | LLPipeline::renderDeferredLighting | looks, rig, roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: [ActorMask] Before EVERY count/toggle/selection side effect. Removing these entries is exactly the light-off world list, including which world entry triggers the count-limit break. A cinematic live probe captures the four authored rig projectors, but not the target rig's syntheti |
| 31512–31524 (n=13) | LLPipeline::renderDeferredLighting | sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: [EnvIntensity v2] per-emitter Local lights EV folded in once here; light_colors.push_back below carries this already-scaled colour and the fullscreen packing copy (col[count] = light_colors[i]) is untouched. |
| 31541–31577 (n=37) | LLPipeline::renderDeferredLighting | rig, roto-tron, sensor, vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: Rig projectors retain their cone/cookie when the world projector toggle is off. Priority is persistent LLVOVolume state used by next-frame main-view shadow-slot assignment. [ActorMask] Record only accepted points actually drawn by F3. [RigRim] |
| 31579–31595 (n=17) | LLPipeline::renderDeferredLighting | rig, vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: Rig projectors retain their cone/cookie when the world projector toggle is off. |
| 31597–31616 (n=20) | LLPipeline::renderDeferredLighting | rig, roto-tron, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: [ActorMask] This accepted F3 point will use the stock batch below. [RigRim] Only staged when active; every consumer below checks the same function-constant rig_rim_active, so indices stay parallel. |
| 31642–31659 (n=18) | LLPipeline::renderDeferredLighting | rig, sensor | creative-candidate | unknown; no local conditional evidence | Edits gDeferredSpotLightProgram, LLColor3, ALEnvIntensity, LLShaderMgr |
| 31677–31703 (n=27) | LLPipeline::renderDeferredLighting | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [RigRim] Round-2 P2 fix: skip packing the rim slot when off; the upload below is skipped too, so a stale/uninitialized entry here is never read. |
| 31706–31735 (n=30) | LLPipeline::renderDeferredLighting | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [RigRim] P2-5: the GLSL name 'rig_rim_lights' is reused for two differently-shaped arrays: this program's multiPointLightF.glsl declares vec4[LIGHT_COUNT] parallel to MULTI_LIGHT/MULTI_LIGHT_COL (uploaded here, manually, once per batch), while the forward alpha programs declare v |
| 31758–31775 (n=18) | LLPipeline::renderDeferredLighting | rig, sensor | creative-candidate | unknown; no local conditional evidence | Edits gDeferredMultiSpotLightProgram, LLColor3, ALEnvIntensity, LLShaderMgr |
| 31781–32475 (n=695) | LLPipeline::renderDeferredLighting, LLPipeline::renderPrismDisplays, ScopedPrismCompositeRestore, popFaceMatrix, pushFaceMatrix, pushWorldMatrix, ~ScopedPrismCompositeRestore | actor-path, capture-sidecar, clones, looks, rig, roto-tron, sensor, shafts-volumetrics, vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: Clear does not free internal vector storage, so this is more efficient than creating new vectors each frame [ActorMask] Independent uncapped scan: the world collector's break cannot hide actors. All actor draws follow BOTH world spot blocks, so tier-0 projectors only auction afte |
| 32488–32502 (n=15) | LLPipeline::doAtmospherics | vcam | creative-candidate | unknown; no local conditional evidence | Edits LLRenderTarget, gPipeline |
| 32507–32513 (n=7) | LLPipeline::doAtmospherics | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 32535–32547 (n=13) | LLPipeline::doWaterHaze | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [PerfB1] Above water, an empty current draw list means the haze geometry covers no pixels. |
| 32549–32563 (n=15) | LLPipeline::doWaterHaze | vcam | creative-candidate | unknown; no local conditional evidence | Edits LLRenderTarget, gPipeline |
| 32569–32583 (n=15) | LLPipeline::doWaterHaze | unknown | unknown-review | unknown; no local conditional evidence | Edits LLShaderMgr |
| 32611–32832 (n=222) | LLPipeline::bindProjectorCookie, LLPipeline::doWaterExclusionMask, LLPipeline::projectorCookieRevision, LLPipeline::setupSpotLight | clones, vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: flush() rebinds the preceding FBO, and bindTarget() installs that target's full-size viewport -- with exact-size Prism scratch packs the restored viewport is already correct, so no reassert is needed. Media samples the settings accepted with these pixels. OWN_COOKIE is deliberate |
| 32893–32899 (n=7) | LLPipeline::setupSpotLight | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 32901–32911 (n=11) | LLPipeline::setupSpotLight | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [World Shadow pick] A picked projector whose soft-shadow channel is world-owned this frame is shadowed by its cube alone: the lighting pass does not also sample its native SL map (index -1, the "no map" value the shader already handles), so there is one shadow edge, not two. The  |
| 32919–33219 (n=301) | LLPipeline::retainSpotShadowOwners, LLPipeline::setupSpotLight, LLPipeline::setupSpotLightVolumetric, LLPipeline::snapshotSpotShadowAuction, LLPipeline::spotShadowTier | rig, roto-tron, shafts-volumetrics, vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge G3.3 Batch 3] Per-projector "cast shadows" opt-out: a projector in the no-shadow set still lights the scene (all the projection uniforms above are still set) but is excluded from the shadow-slot priority assignment below, so it never occupies an mTargetShadowSpotLight[]  |
| 33222–33232 (n=11) | LLPipeline::unbindDeferredShader | rig | creative-candidate | unknown; conditional evidence present | Edits LLShaderMgr |
| 33289–34342 (n=1054) | LLPipeline::bindReflectionProbes, LLPipeline::bindWeatherMaterial, LLPipeline::dirtyWeatherDrawable, LLPipeline::dirtyWeatherRegion, LLPipeline::drawWeatherCover, LLPipeline::generateWeatherAdvancedShelter, LLPipeline::generateWeatherRainOcclusion, LLPipeline::generateWeatherSurfaceCover, LLPipeline::renderShadow, LLPipeline::unbindReflectionProbes, look, sgn, std::any_of | clones, vcam, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: On an auxiliary no-UBO allocation failure, never let setUniforms() lazily run updateUniforms() from the source camera. The explicit Prism shader guard still suppresses SSR/hero; probe lighting simply degrades for this failed capture instead of contaminating main state. The shared |
| 34361–34369 (n=9) | LLPipeline::renderShadow | unknown | unknown-review | unknown; no local conditional evidence | Edits LLGLEnable, LLGLState |
| 34380–34427 (n=48) | LLPipeline::renderShadow | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [AlphaShadowCap] Independent expected count from the sorted maps, not from filter invocations. Scalar legacy sweeps omit multi-slot draws; only the four material-mask types below have an optional indexed shadow sweep. PBR has either scalar + indexed sweeps or one unsplit fallback |
| 34446–34454 (n=9) | LLPipeline::renderShadow | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 34485–34503 (n=19) | LLPipeline::renderShadow | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [AlphaShadowCap] Before binds; restored at iteration end, before rigged. |
| 34505–34515 (n=11) | LLPipeline::renderShadow | unknown | unknown-review | unknown; conditional evidence present | Edits LLGLSLShader, LLShaderMgr |
| 34521–34527 (n=7) | LLPipeline::renderShadow | unknown | unknown-review | unknown; no local conditional evidence | Edits LLGLSLShader, LLShaderMgr |
| 34533–34547 (n=15) | LLPipeline::renderShadow | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [AlphaShadowCap] Grass cannot be a worn attachment. |
| 34556–34562 (n=7) | LLPipeline::renderShadow | unknown | unknown-review | unknown; no local conditional evidence | Edits LLGLSLShader, LLShaderMgr |
| 34567–34581 (n=15) | LLPipeline::renderShadow | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [AlphaShadowCap] Before binds; restored at iteration end, before rigged. |
| 34594–34600 (n=7) | LLPipeline::renderShadow | unknown | unknown-review | unknown; no local conditional evidence | Edits LLGLSLShader, LLShaderMgr |
| 34610–34616 (n=7) | LLPipeline::renderShadow | unknown | unknown-review | unknown; no local conditional evidence | Edits LLGLSLShader, LLShaderMgr |
| 34639–34645 (n=7) | LLPipeline::renderShadow | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 34839–34885 (n=47) | LLPipeline::getSpotShadowTarget, LLPipeline::getSunShadowTarget, LLPipeline::mainSunShadowsPublishedThisIteration, LLPipeline::reexpressProbeSpotShadows | vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: [ProbeCost] Allocation metadata and this iteration's completed generation are both required. [Prism spot shadows Stage 2] the aux capture samples its own dedicated maps; the main-view maps must survive untouched for the main deferred lighting that runs after the aux pass (paralle |
| 34943–34953 (n=11) | LLPipeline::generateSunShadow | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [ProbeCost] A failed/early-returning main generation cannot publish stale maps. |
| 35042–35048 (n=7) | LLPipeline::generateSunShadow | unknown | unknown-review | unknown; conditional evidence present | Edits gGL |
| 35053–35060 (n=8) | LLPipeline::generateSunShadow | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 35069–35109 (n=41) | LLPipeline::generateSunShadow | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [ShadowDist P1] Control A: cascade blend width this generation's frusta (and, through the meta, the shader) use. Probe renders always keep the stock 0.25. At default 1.f - 0.25f == 0.75f and 1.f + 0.25f == 1.25f exactly, so the stock constants below are reproduced exactly (<= 1 u |
| 35195–35248 (n=54) | LLPipeline::generateSunShadow | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [ShadowDist P1] Control A: "Sharp shadow range (m)" (design v2 s2.2). The user value is the camera distance covered EXCLUSIVELY by cascade 0, i.e. (1 - blend) * clip0; cascades 1..3 then split the remainder geometrically so their texel-density ratio is uniform. Bypasses the split |
| 35253–35330 (n=78) | LLPipeline::generateSunShadow | vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: [ProbeCost] Reuse/Off need Full's probe projector clip/fade distances, but must leave all shadow maps and their metadata untouched. CPU zones enclose the entire RT call, including Tracy GPU timestamps. Pass zones carry the sun/projector index; bind values pack old/new FBOs. [Shad |
| 35365–35373 (n=9) | LLPipeline::generateSunShadow | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [ShadowDist P1] (1 -/+ blend) == 0.75f / 1.25f exactly at default |
| 35430–35694 (n=265) | LLPipeline::generateSunShadow | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: restore render matrices [ShadowDist P3] Control B: cascade 0 = light-space orthographic COLUMN around the subject (design v2 s3.4, v3 s4, v4 s5). Self-contained: fits, records, culls, renders, writes ShadowMeta site 3 and 'continue's, so the legacy slice fit below stays byte-iden |
| 35715–35723 (n=9) | LLPipeline::generateSunShadow | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [ShadowDist P1] (1 -/+ blend) == 0.75f / 1.25f exactly at default |
| 35742–35767 (n=26) | LLPipeline::generateSunShadow | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [ShadowDist P1] ShadowMeta site 4: a cleared map samples as lit whatever its matrix says, so sunVP[j] is left as is (exactly as the legacy code leaves mSunShadowMatrix[j]). |
| 35780–35790 (n=11) | LLPipeline::generateSunShadow | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [ShadowDist P2] Control C coefficients for THIS cascade's projection, written by whichever of the three proj[j] branches below runs and stored in the meta at site 2. |
| 35853–35859 (n=7) | LLPipeline::generateSunShadow | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE |
| 35891–35908 (n=18) | LLPipeline::generateSunShadow | unknown | unknown-review | unknown; conditional evidence present | Edits RenderShadowErrorCutoff, LLPipeline, gCubeSnapshot |
| 35936–35943 (n=8) | LLPipeline::generateSunShadow | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 35967–35974 (n=8) | LLPipeline::generateSunShadow | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 35987–36004 (n=18) | LLPipeline::generateSunShadow | unknown | unknown-review | unknown; conditional evidence present | Edits LLPipeline, gCubeSnapshot |
| 36008–36370 (n=363) | LLPipeline::generateSunShadow | rig, roto-tron, vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: translate view to origin translate view to origin llassert(eye.isFinite()); shadow_cam.setFar(128.f); shadow_cam.ignoreAgentFrustumPlane(LLCamera::AGENT_PLANE_NEAR); translate and scale to from [-1, 1] to [0, 1] [ShadowDist P1] ShadowMeta site 2: this cascade's view-independent t |
| 36372–37427 (n=1056) | LLPipeline::generatePrismSpotShadows, LLPipeline::generateSunShadow, LLPipeline::generateWorldShadowCubes, LLPipeline::releaseWorldShadowTargets, WorldShadowDrawScope, WorldShadowStateGuard, attachment_visibility, capture, compare, countWorldShadowRiggedMaps, std::sort, ~WorldShadowDrawScope, ~WorldShadowStateGuard | looks, rig, roto-tron, sensor, vcam | creative-candidate | unknown; conditional evidence present | Comment evidence:  hack to disable projector shadows Restore the entry parent (or FB0 + GL_BACK) before the beauty-pass state setup. Keep gGLViewport exactly as stock, including empty-slice behavior. should never happen update shadow targets [ProbeCost] Only completed main generations publish; ear |
| 38223–38237 (n=15) | LLPipeline::skipRenderingShadows | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [ShadowDist P1] ShadowMeta site 5: all four maps cleared -> lit; the matrices are irrelevant, only the (future) subject flag must drop. |
| 38297–38449 (n=153) | LLPipeline::getRotoAnimClockOffset, LLPipeline::uploadPrismPreviewColorGradeUniforms | clones, looks, roto-tron, vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: --- Linear-space grading (pre-tonemap) --- White balance: resolve (CCT offset, Duv) into a linear-sRGB gain on the CPU. Duv is exposed to artists on a friendly [-1, 1] scale and scaled into CIE 1960 uv units (Ã‚Â±0.02) here. Lift / Gamma / Gain; gamma is inverted on the CPU. ---  |

## Slice 2: `indra/newview/pipeline.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 39–67 (n=29) | unknown/non-function (declaration, XML, build list or global scope) | clones, rig, roto-tron, vcam, weather | creative-candidate | unknown; no local conditional evidence | Edits ALDiopterBankKind, ALDiopterFocusProvenance |
| 85–130 (n=46) | unknown/non-function (bool LLRayAABB(const LLVector3 &center, const LLVector3 &size, const LLVector3&) | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ReShadeBridge] Extra colour attachments on the MAIN-VIEW mRT->screen target, added only when RenderVisibleDiffuseSidecar is on. Attachment 0 is the beauty buffer. Named because the indices appear in GLSL layout qualifiers, in the LLRender indexed-guard calls, and in the bridge's |
| 146–157 (n=12) | unknown/non-function (public:) | shafts-volumetrics | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge G3.3 Dust] lazily load the baked 64^3 RGBA8 dust volume (app_settings/dust/dust_volume_64_rgba8.ktx) into mProjVolDustMap as a GL_TEXTURE_3D (REPEAT/LINEAR). Cheap to call every frame: it returns immediately once loaded, and mProjVolDustLoadAttempted limits a missing/ co |
| 171–209 (n=39) | unknown/non-function (public:) | vcam | creative-candidate | unknown; no local conditional evidence | Comment evidence: Prism Option B (reworked): acquire an EXACT-SIZE auxiliary deferred scratch pack for one capture render. Selects (or allocates) the pool entry whose physical size equals the requested render size and points mActivePrismLensScratch at it. Exact-size is mandatory: the shared deferr |
| 225–569 (n=345) | physicalCameraOrder, projectorShaftCost, projectorVolumetricShaderLoaded | director-shell, looks, rig, roto-tron, sensor, shafts-volumetrics, vcam, weather | creative-candidate | unknown; no local conditional evidence | Comment evidence: On-lens filters (Graduated ND + Polarizer) applied to the linear HDR scene before bloom/flare generation, so those optics respect the filtered scene. [Ultimate Diopter] two-pass lens-front diopter/halo: MRT gather into mDiopterMap, then composite src -> dst (caller swaps either w |
| 708–797 (n=90) | unknown/non-function (public:) | capture-sidecar, clones, rig, shafts-volumetrics, vcam | creative-candidate | unknown; no local conditional evidence | Comment evidence: [GhostDeferred] submit Ghost Studio clones into the deferred G-buffer (scene- lit clones). Explicit camera so the hero-probe mirror pass can reuse it. No-op unless GhostDeferredEnable is set; wrap in LLScopedGhostRenderInvariant. [GhostDeferred] POST-deferred forward stages for t |
| 801–868 (n=68) | rigRimStatus, rigRimStatusRevision | capture-sidecar, clones, rig, roto-tron, shafts-volumetrics, vcam | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ActorMask] Unknown callers fail closed; explicit entry points are verified against the active pack/flags, never trusted as permission by themselves. [AvatarSilhouette] View-local RG8 coverage replay. Phase 0 is opaque / masked (before atmospherics); phase 1 adds authored-alpha c |
| 878–944 (n=67) | unknown/non-function (public:) | composition, rig, vcam, weather | creative-candidate | unknown; no local conditional evidence | Comment evidence: [P1-lite] World cube shadow maps for the top 1-4 resident lights (point or projector) (alworldshadowmath.h). Main view only; returns before any GL call while RenderAreaShadowCubeMaps is off. Never touches the native sun/projector slots, fades or matrices. [Prism spot shadows Stag |
| 949–990 (n=42) | getLights, isCinematicProbeLightAnimating | rig, roto-tron, sensor, vcam | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ActorMask] Raw content selected only around eligible forward draws. Snapshot the main-view lighting/probe state before an auxiliary Prism render and restore it afterward. These calls must be paired by RAII; they intentionally remain separate from sPrismLensRender so the main cam |
| 1005–1011 (n=7) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1021–1027 (n=7) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1108–1174 (n=67) | unknown/non-function (private:) | actor-path, clones | creative-candidate | unknown; no local conditional evidence | Comment evidence: [GhostDeferred] per-instance coverage: which render CATEGORIES drew into the deferred G-buffer this frame (overlay suppression is per-category, never all-or-nothing -- see llghostcoverage.h). Rebuilt each frame; an instance with no successful category is absent (full overlay fall |
| 1353–1383 (n=31) | unknown/non-function (public:) | capture-sidecar, rig, vcam, weather | creative-candidate | unknown; no local conditional evidence | Comment evidence: Non-null only inside one world-cube light's six caster passes. Also selects the separate rigged BLEND map; native sun/spot keep their route. [AlphaShadowCap] 0 = none, 1 = rain (reject attachments), 2 = attachments only, 3 = attachments only + debug accounting (chosen once per pa |
| 1406–1425 (n=20) | unknown/non-function (public:) | rig, roto-tron | creative-candidate | unknown; no local conditional evidence | Comment evidence: [PerfB1] Pack-local water scratch keeps auxiliary passes at capture resolution. |
| 1428–1489 (n=62) | reset | rig | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ShadowDist P1] Interpretation of the four sun maps above (design v6 s1.1). INVARIANT: every statement that writes, clears, fits or (re)allocates any shadow[j] of this pack writes this meta in the same block and bumps 'serial'. uploadShadowUniforms() derives everything the sun-sh |
| 1491–1899 (n=409) | PrismLensOutput, allocate, clampSpotShadowCount, getActivePrismLensScratchPack, maxSpotShadowsForTextureUnits, spotShadowMapIndex, swapOutput | capture-sidecar, director-shell, rig, shafts-volumetrics, vcam, weather | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ProbeCost] Ordinary probe captures only; previews and heroes retain stock packs. Option B (reworked): bounded pool of EXACT-SIZE auxiliary deferred scratch packs, reused across frames keyed by (width,height). Exact-size => physical == render, so the shared deferred fullscreen sh |
| 1926–2195 (n=270) | getNightMaskFrameState | camera, capture-sidecar, director-shell, looks, rig, roto-tron, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: WP-B scratch is owned by the main presentation, independently of mRT. RAW (pre-on-lens-filter) HDR scene snapshot for the ReShade bridge. applyOnLensFilters blits mRT->screen here BEFORE the Graduated ND / Polarizer modify it (gated on RenderReShadeDecoupleOnLens), so RTGI-style  |
| 2216–2249 (n=34) | unknown/non-function (public:) | looks, rig, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: Per-source temporal visibility smoothing for the cine-rig lens flare, mirroring mLensFlareSunVisibility above but one scalar per rig projector SLOT (not per packed upload index — a packed index can be reused by a different light next frame when a source drops out, which would oth |
| 2268–2285 (n=18) | unknown/non-function (protected:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Non-null only for the opt-in, stateless feed collector. Main-view ties keep their existing pointer order and native auction inputs. |
| 2288–2295 (n=8) | operator | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 2299–2356 (n=58) | unknown/non-function (protected:) | roto-tron, vcam | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ActorMask] Zero = world; other keys = WP0 exact owner/wearer bytes. Main-view state retained while the one scheduled Prism capture owns the shared renderer. The nearby set owns LLPointer references, which keeps every drawable alive until its NEARBY_LIGHT bit can be restored safe |
| 2457–2470 (n=14) | unknown/non-function (protected:) | shafts-volumetrics | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge G3.3 Dust] 64^3 tileable dust volume (GL_TEXTURE_3D, RGBA8), lazily loaded from app_settings/dust the first frame beam dust is enabled. Released in releaseGLBuffers; the attempted flag stops a missing/corrupt file from re-hitting the disk every frame (reset with the GL b |
| 2503–2516 (n=14) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [F4] DoF focus target for the current frame, promoted from a local static in renderDoF() so renderFocusPoint() can draw it in the UI pass. (donor: Firestorm pipeline.h LLPipeline::sLastFocusPoint, FIRE-16728) [F4] whether DoF was actually composited this frame; gates the focus po |
| 2582–2613 (n=32) | unknown/non-function (public:) | vcam | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ShadowDist P1] Control A (sharp-shadow range) + Control D (soften split) [ShadowDist P2] Control C (world-unit bias / offset / soft kernel) [ShadowDist P3] Control B (Follow subject) + alpha fix + Prism sun re-expression |
| 2615–2808 (n=194) | unknown/non-function (public:) | capture-sidecar, rig, shafts-volumetrics | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge G3.2] volumetric lighting (donor: Black Dragon) [BDMerge G3.3] per-projector volumetric light cones (visible spotlight shafts) [BDMerge G3.3 Phase 1] cinema levers [BDMerge G3.3 Phase 1 item 3] march the cones into a half-res target and depth-aware bilateral-upsample the |
| 2819–2842 (n=24) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [Ultimate Diopter] §6.3 piece 2f's write-back revert failure path -- alDiopterAbortPresetSelection -- and the guard/reason state machine it runs under now live in aldiopterpresetbank.h (Codex review finding F6), not here; this header only includes that one for ALDiopterBankKind.  |

## Slice 2: `indra/newview/skins/default/colors.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 2–154 (n=153) | unknown/non-function (declaration, XML, build list or global scope) | clones, director-shell, roto-tron | creative-candidate | unknown; no local conditional evidence | Comment evidence: Director kit palette. Keep these names as the single C++/XUI colour authority. --> One-to-one Director.dc.html tokens; aliases above remain for compact C++ lookup. --> |

## Slice 2: `indra/newview/skins/default/textures/textures.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 134–164 (n=31) | unknown/non-function (with the same filename but different name) | rig | creative-candidate | unknown; no local conditional evidence | Comment evidence: Cinematic Lighting 2.0 cookie thumbnails.  The same sharp luma masks |
| 184–193 (n=10) | unknown/non-function (with the same filename but different name) | environment | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge B13] BD environment editor toolbar icons --> |
| 909–934 (n=26) | unknown/non-function (with the same filename but different name) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1049–1141 (n=93) | unknown/non-function (with the same filename but different name) | director-shell | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 2: `indra/newview/skins/default/xui/en/floater_fs_poser.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1145–1263 (n=119) | unknown/non-function (declaration, XML, build list or global scope) | director-shell | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1974–1980 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1998–2058 (n=61) | unknown/non-function (declaration, XML, build list or global scope) | director-shell | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 2: `indra/newview/skins/default/xui/en/floater_lightbox_settings.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–27 (n=27) | unknown/non-function (declaration, XML, build list or global scope) | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 34–40 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 156–172 (n=17) | unknown/non-function (declaration, XML, build list or global scope) | looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge] CAS sharpen strength. The one implemented sharpener (AMD |
| 342–355 (n=14) | unknown/non-function (declaration, XML, build list or global scope) | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 368–407 (n=40) | unknown/non-function (declaration, XML, build list or global scope) | capture-sidecar, looks, roto-tron, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: [TonemapLegacySky] Always available, independent of tonemap type. --> [ExposureHub] Opens the one-stop Exposure floater (same settings). Placed before the |
| 413–420 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 433–465 (n=33) | unknown/non-function (declaration, XML, build list or global scope) | looks, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: <text [Grade] Existing type-0 controls, gated in updateTonemapper(). |
| 467–502 (n=36) | unknown/non-function (declaration, XML, build list or global scope) | looks, sensor | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 504–538 (n=35) | unknown/non-function (declaration, XML, build list or global scope) | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 543–574 (n=32) | unknown/non-function (declaration, XML, build list or global scope) | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 577–642 (n=66) | unknown/non-function (declaration, XML, build list or global scope) | looks, sensor | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 644–676 (n=33) | unknown/non-function (declaration, XML, build list or global scope) | looks, sensor | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 678–1297 (n=620) | unknown/non-function (declaration, XML, build list or global scope) | looks, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: Rendering Panel--> Probe Detail--> Probe Coverage--> [S-Log3] Capture controls (tonemap type 8 only). Gated <text Probe Draw Distance-->  Anti-Aliasing--> <check_box Rendering Panel--> Probe Detail--> |
| 1299–4187 (n=2889) | unknown/non-function (declaration, XML, build list or global scope) | director-shell, looks, rig, roto-tron, sensor, shafts-volumetrics | creative-candidate | unknown; no local conditional evidence | Comment evidence: Anisotropic Filtering--> Probe Coverage--> Resolution Scale--> TODO: Disable if AA is also disabled. This option causes broken rendering otherwise! Bug maybe?--> [ProbeManualRate] Region probe update rate--> [ProbeCost] Capture cost controls; heroes retain stock quality. --> Upda |
| 4189–4244 (n=56) | unknown/non-function (declaration, XML, build list or global scope) | looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: SSAO Factor--> Glow Resolution--> |
| 4246–4301 (n=56) | unknown/non-function (declaration, XML, build list or global scope) | looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: SSAO Scale--> Glow Strength--> |
| 4303–4358 (n=56) | unknown/non-function (declaration, XML, build list or global scope) | looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: SSAO Max Scale--> Glow Softness--> |
| 4360–6510 (n=2151) | unknown/non-function (declaration, XML, build list or global scope) | camera, capture-sidecar, crowd, director-shell, environment, looks, rig, roto-tron, sensor, shafts-volumetrics, weather | mixed/perf-review | unknown; no local conditional evidence | Comment evidence: Glow Panel--> DOF Panel--> Glow Toggle--> [BDMerge G3.2] Godrays Panel - thin UI, all widgets bind debug settings--> Glow Iterations--> Glow Resolution--> [BDMerge G3.3] per-projector volumetric light cones--> [BDMerge B-2b.3] Machinima Panel - every runtime-adjustable BDMerge Gl |

## Slice 2: `indra/newview/skins/default/xui/en/floater_preferences_graphics_advanced.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–13 (n=13) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 154–180 (n=27) | unknown/non-function (declaration, XML, build list or global scope) | unknown | mixed/perf-review | unknown; no local conditional evidence | Edits RenderMaxVRAMBudget |
| 182–222 (n=41) | unknown/non-function (declaration, XML, build list or global scope) | director-shell | mixed/perf-review | unknown; no local conditional evidence | Comment evidence: [FrameLimiter] Frame rate cap; also on Director > Capture > Time. --> |
| 444–450 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 452–458 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 517–528 (n=12) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 531–537 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 663–679 (n=17) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits BDMergeMachinimaHighLOD |
| 717–723 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 830–848 (n=19) | unknown/non-function (declaration, XML, build list or global scope) | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Edits RenderHDR16BitColor, Renders |
| 938–944 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 978–984 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1013–1034 (n=22) | unknown/non-function (declaration, XML, build list or global scope) | looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ProbeOnDemand] --> |
| 1213–1226 (n=14) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1243–1305 (n=63) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderVRAMGovernorEnabled |

## Slice 2: `indra/newview/skins/default/xui/en/fonts.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 322–335 (n=14) | unknown/non-function (declaration, XML, build list or global scope) | director-shell | creative-candidate | unknown; no local conditional evidence | Comment evidence: Kit chip selection uses the mockup's 600 weight without changing the |
| 663–688 (n=26) | unknown/non-function (declaration, XML, build list or global scope) | composition, director-shell | creative-candidate | unknown; no local conditional evidence | Comment evidence: Director kit CSS-pixel equivalents at the viewer's 96 dpi baseline. --> Cinematic Viewfinder readout ladder: Viewfinder4 is the 9 pt Small UI |

## Slice 2: `indra/newview/skins/default/xui/en/menu_attachment_other.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 3–205 (n=203) | unknown/non-function (declaration, XML, build list or global scope) | actor-path, camera, director-shell, looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: [Director] Machinima tools, consolidated. The cast / subject / mover / |
| 392–415 (n=24) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 507–551 (n=45) | unknown/non-function (declaration, XML, build list or global scope) | director-shell | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 2: `indra/newview/skins/default/xui/en/menu_attachment_self.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 2–194 (n=193) | unknown/non-function (declaration, XML, build list or global scope) | actor-path, camera, director-shell, looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: [Director] Machinima tools, consolidated. The cast / subject / mover / |
| 609–653 (n=45) | unknown/non-function (declaration, XML, build list or global scope) | director-shell | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 2: `indra/newview/skins/default/xui/en/menu_avatar_other.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 3–11 (n=9) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 114–322 (n=209) | unknown/non-function (declaration, XML, build list or global scope) | actor-path, camera, director-shell, looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: [Director] Machinima tools, consolidated. Every item resolves the clicked |
| 403–426 (n=24) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 495–539 (n=45) | unknown/non-function (declaration, XML, build list or global scope) | director-shell | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 2: `indra/newview/skins/default/xui/en/menu_avatar_self.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 2–10 (n=9) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 298–490 (n=193) | unknown/non-function (declaration, XML, build list or global scope) | actor-path, camera, director-shell, looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: [Director] Machinima tools for my own avatar, consolidated. Every item |
| 511–555 (n=45) | unknown/non-function (declaration, XML, build list or global scope) | director-shell | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 2: `indra/newview/skins/default/xui/en/menu_object.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 22–270 (n=249) | unknown/non-function (declaration, XML, build list or global scope) | actor-path, camera, director-shell, looks, vcam | creative-candidate | unknown; no local conditional evidence | Comment evidence: [Director] Machinima tools for objects, consolidated. Play/Stop Local [ObjectPath] enroll this linkset's ROOT in the client-side object |
| 281–361 (n=81) | unknown/non-function (declaration, XML, build list or global scope) | director-shell, rig, shafts-volumetrics | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge G3.3] Machinima: purely client-side volumetric light-shaft opt-in. [World Shadow pick] Machinima: purely client-side opt-in to the world (cube) shadow slots, for any [BDMerge F4] per-projector Hero Beam. When the froxel volumetrics master is on, [BDMerge G3.3 Batch 3] pe |
| 589–595 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 598–604 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 2: `indra/newview/skins/default/xui/en/menu_viewer.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 200–442 (n=243) | unknown/non-function (declaration, XML, build list or global scope) | camera, capture-sidecar, clones, director-shell, gaze, looks, rig, sensor, vcam | creative-candidate | unknown; no local conditional evidence | Edits RenderFocusPointLocked, RenderFocusPointCrosshair |
| 767–785 (n=19) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 920–935 (n=16) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 997–1013 (n=17) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1249–1267 (n=19) | unknown/non-function (declaration, XML, build list or global scope) | clones | creative-candidate | unknown; no local conditional evidence | Comment evidence: [FriendsOnly] same setting as Develop > Avatar > Character Tests > Render Only Friends. |
| 1431–1462 (n=32) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1493–1519 (n=27) | unknown/non-function (declaration, XML, build list or global scope) | environment | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge B13] BD environment editors --> |

## Slice 2: `indra/newview/skins/default/xui/en/notifications.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 189–205 (n=17) | unknown/non-function (No tutorial is currently available.) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 6490–6520 (n=31) | unknown/non-function (You cannot undo this action.) | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 9277–9300 (n=24) | unknown/non-function (Multiple SLurls were received from an untrusted browser within a short period.) | camera | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 9340–9359 (n=20) | unknown/non-function (Failed to save snapshot to [PATH]: Disk is full. [NEED_MEMORY]KB is required but) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge C6] high-res snapshot unlock warning (donor: Black Dragon |
| 9395–9432 (n=38) | unknown/non-function (this preset or choose another name.) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge B2] Camera preset rename (donor: Black Dragon "Unlimited Camera |
| 13574–13934 (n=361) | unknown/non-function (Incoming instant messages from anyone who is not your friend will now be answere) | camera, director-shell, environment, looks, sensor, weather | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge B13] BD local Windlight preset notifications (donor: BD [FileSafety] --> [FileSafety] --> [F4] Phototools environment quick-switcher (donor: Firestorm I:\enve Shared cinematic Weather panel: named complete-state presets and reset. --> Ultimate Diopter Custom-bank state m |

## Slice 2: `indra/newview/skins/default/xui/en/panel_preferences_advanced.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 234–272 (n=39) | unknown/non-function (declaration, XML, build list or global scope) | clones, director-shell, roto-tron | creative-candidate | unknown; no local conditional evidence | Comment evidence: [DebugLogging] master switch for fork/machinima diagnostic log lines --> [WebIdle] hidden/never-opened built-in web pages cost CPU+GPU for nothing --> |

## Slice 2: `indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 383–401 (n=19) | unknown/non-function (declaration, XML, build list or global scope) | camera, director-shell | creative-candidate | unknown; no local conditional evidence | Edits BDMergeCineModeAuto, ALL |

## Slice 2: `indra/newview/skins/default/xui/en/strings.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 21–29 (n=9) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 4233–4242 (n=10) | unknown/non-function (name="Command_360_Capture_Label">360 snapshot</string>) | environment | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge B13] BD environment editor toolbar buttons --> |
| 4283–4292 (n=10) | unknown/non-function (name="Command_360_Capture_Label">360 snapshot</string>) | environment | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge B13] BD environment editor toolbar buttons --> |
| 4324–4332 (n=9) | unknown/non-function (name="Command_360_Capture_Label">360 snapshot</string>) | director-shell | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 4543–4570 (n=28) | unknown/non-function (and report the problem.) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [F4] Phototools environment quick-switcher combo placeholder entries [BDMerge F6] Pose Stand animation display names |

## Slice 2: `indra/newview/viewer_manifest.py`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 76–87 (n=12) | unknown/non-function (class ViewerManifest(LLManifest):) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 91–96 (n=6) | unknown/non-function (class ViewerManifest(LLManifest):) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 108–119 (n=12) | unknown/non-function (class ViewerManifest(LLManifest):) | director-shell, shafts-volumetrics | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 3: `indra/llcharacter/CMakeLists.txt`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 30–38 (n=9) | unknown/non-function (target_sources(llcharacter) | clones, stepped-animation | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 79–85 (n=7) | unknown/non-function (if (BUILD_TESTING)) | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_ADD_INTEGRATION_TEST |

## Slice 3: `indra/llcharacter/llheadrotmotion.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 24–35 (n=12) | unknown/non-function (declaration, XML, build list or global scope) | gaze | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge B4] Configurable max head/eye rotation angle for lookAt, gated by BDMergeHeadEyeLimits. donor: Black Dragon (NiranV Dean) 525e398af5 "Modify head tracking options to allow customizing max head and eye rotation in degrees", defaults per follow-up 637c2c6f7d/9b824a0b50. Ga |
| 69–90 (n=22) | LLEyeMotion::resetEyeRotationConstraint, LLHeadRotMotion::resetHeadRotationConstraint | gaze | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge B4] runtime-configurable copies of the constraint constants above. Default to the stock constants so behavior is unchanged until newview pushes a custom value (only while BDMergeHeadEyeLimits is enabled). |
| 198–218 (n=21) | LLHeadRotMotion::onUpdate | clones, gaze | creative-candidate | unknown; conditional evidence present | Edits LLFrameTimer, ALAvatarAnimationTime |
| 262–268 (n=7) | LLHeadRotMotion::onUpdate | gaze | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 395–401 (n=7) | LLEyeMotion::onActivate | gaze | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 431–437 (n=7) | LLEyeMotion::adjustEyeTarget | gaze | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 492–536 (n=45) | LLEyeMotion::onUpdate | clones, gaze | creative-candidate | unknown; conditional evidence present | Edits LLFrameTimer, ALAvatarAnimationTime, LLVector3 |
| 547–555 (n=9) | LLEyeMotion::onUpdate | gaze | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 563–577 (n=15) | LLEyeMotion::onUpdate | gaze | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 585–596 (n=12) | LLEyeMotion::onUpdate | gaze | creative-candidate | unknown; no local conditional evidence | Edits LLVector3 |

## Slice 3: `indra/llcharacter/llheadrotmotion.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 31–37 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | clones, gaze | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 102–113 (n=12) | setHeadRotationConstraint | gaze | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge B4] configurable max head rotation angle, gated by BDMergeHeadEyeLimits (donor: Black Dragon 525e398af5). Pushed per-frame from newview via LLCachedControl; defaults to HEAD_ROTATION_CONSTRAINT so gate-off behavior is unchanged from stock. |
| 124–132 (n=9) | unknown/non-function (public:) | gaze | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge B4] |
| 196–207 (n=12) | setEyeRotationConstraint | gaze | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge B4] configurable max eye rotation angle, gated by BDMergeHeadEyeLimits (donor: Black Dragon 525e398af5). Pushed per-frame from newview via LLCachedControl; defaults to EYE_ROT_LIMIT_ANGLE so gate-off behavior is unchanged from stock. |
| 214–224 (n=11) | unknown/non-function (public:) | clones, gaze | creative-candidate | unknown; no local conditional evidence | Edits LLVector3, ALAvatarAnimationTime |
| 228–239 (n=12) | unknown/non-function (public:) | clones, gaze | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge B4] |

## Slice 3: `indra/llcharacter/llkeyframemotion.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 35–42 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | clones | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 50–87 (n=38) | unknown/non-function (declaration, XML, build list or global scope) | actor-path, stepped-animation | creative-candidate | unknown; no local conditional evidence | Comment evidence: [PosePolish M4] Loop-seam repair statics. docs/pose_polish_integration_plan.md "Milestone 4". Cache is keyed by clip asset id, mirroring LLKeyframeWalkMotion::sPhaseInfoCache's per-asset caching for Milestone 3. -------------------------------------------------------------------- |
| 457–473 (n=17) | unknown/non-function (LLKeyframeMotion::LLKeyframeMotion(const LLUUID &id)) | actor-path | creative-candidate | unknown; no local conditional evidence | Comment evidence: [PosePolish M4] loop-seam repair runtime state; see llkeyframemotion.h. |
| 491–521 (n=31) | LLKeyframeMotion::getEmoteID, LLKeyframeMotion::getEmoteName, LLKeyframeMotion::getJointMotionNames | unknown | unknown-review | unknown; conditional evidence present | Edits LLKeyframeMotion, LLUUID |
| 543–549 (n=7) | LLKeyframeMotion::onInitialize | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 681–687 (n=7) | LLKeyframeMotion::setupPose | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 721–729 (n=9) | LLKeyframeMotion::onActivate | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 736–768 (n=33) | LLKeyframeMotion::onActivate | actor-path, stepped-animation | creative-candidate | unknown; conditional evidence present | Comment evidence: [PosePolish M4] Loop-seam repair: a fresh activation must never carry over a mid-window blend (or a previous-time reference for wrap detection) from a prior play of this same motion instance. Also force the (cached, once-per- asset) seam analysis and this instance's captured-pose |
| 772–791 (n=20) | LLKeyframeMotion::onUpdate | actor-path, stepped-animation | creative-candidate | unknown; conditional evidence present | Comment evidence: [PosePolish M4] Loop-seam repair: capture this clip's in-cycle time from *before* the stock computation below overwrites it, so a wrap edge can be detected afterward. No-op unless sLoopSeamRepairEnabled is true (see detectSeamWrap()). |
| 820–855 (n=36) | LLKeyframeMotion::onUpdate | stepped-animation | creative-candidate | unknown; conditional evidence present | Comment evidence: Carry the authored seam gap plus any previous in-flight correction. Measuring the residual at the previous time preserves this frame's motion. Use the actual wrap overshoot on the first window frame. |
| 886–1353 (n=468) | LLKeyframeMotion::applyExternalLocomotionSeed, LLKeyframeMotion::applySeamEase, LLKeyframeMotion::captureSeamPose, LLKeyframeMotion::detectSeamWrap, LLKeyframeMotion::getGenericLocomotionPhaseInfo, LLKeyframeMotion::getLoopSeamInfo, LLKeyframeMotion::setExternalLocomotionSeed, find_generic_plant_window, wrap_generic_phase | actor-path, stepped-animation | creative-candidate | unknown; conditional evidence present | Comment evidence: Loading is not a stable analysis result.  In particular, do not poison the asset-wide cache with NOT_LOOPING before the async asset callback has populated mJointMotionList; the next mover frame must be able to analyze the newly available curves. ---------------------------------- |
| 1359–1413 (n=55) | LLKeyframeMotion::applyConstraints | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: apply constraints Activation/rebuild while frozen gets one full spatial solve without advancing temporal clocks; subsequent frames replay the constrained pose. Only constraint chains can be modified here; cache their state pointers once rather than copying the full skeleton every |
| 1416–1425 (n=10) | LLKeyframeMotion::onDeactivate | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1550–1564 (n=15) | LLKeyframeMotion::applyConstraint | clones | creative-candidate | unknown; no local conditional evidence | Comment evidence: Bound the explicit spatial solver step; animation phase and damping retain the full clock rate even above unit speed. |
| 1677–1695 (n=19) | LLKeyframeMotion::applyConstraint | clones | creative-candidate | unknown; no local conditional evidence | Edits LLSmoothInterpolation, ALAvatarAnimationTime, ALAvatarConstraint |
| 1738–1750 (n=13) | LLKeyframeMotion::applyConstraint | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: constraint to child constraint to parent LL_INFOS() << iteration_count << " iterations on " << mCharacter->findCollisionVolume(shared_data->mSourceConstraintVolume)->getName() << LL_ENDL; |
| 2071–2077 (n=7) | LLKeyframeMotion::deserialize | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 2950–2956 (n=7) | LLKeyframeMotion::flushKeyframeCache | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 2964–2980 (n=17) | LLKeyframeMotion::setLoop | actor-path, stepped-animation | creative-candidate | unknown; conditional evidence present | Comment evidence: [PosePolish M4] the cached seam analysis (getLoopSeamInfo()) was computed against the *previous* loop window; a preview/edit-tool mutation of it must invalidate the shared per-asset cache entry so a stale seam is never applied. No-op unless sLoopSeamRepairEnabled is true (the cac |
| 2987–2993 (n=7) | LLKeyframeMotion::setLoopIn | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 3006–3018 (n=13) | LLKeyframeMotion::setLoopIn | actor-path, stepped-animation | creative-candidate | unknown; conditional evidence present | Comment evidence: [PosePolish M4] see setLoop() -- invalidate the stale cached seam analysis for this asset. No-op unless sLoopSeamRepairEnabled is true. |
| 3024–3030 (n=7) | LLKeyframeMotion::setLoopOut | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 3043–3055 (n=13) | LLKeyframeMotion::setLoopOut | actor-path, stepped-animation | creative-candidate | unknown; conditional evidence present | Comment evidence: [PosePolish M4] see setLoop() -- invalidate the stale cached seam analysis for this asset. No-op unless sLoopSeamRepairEnabled is true. |
| 3245–3247 (n=3) | unknown/non-function (LLKeyframeMotion::JointConstraint::~JointConstraint()) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 3: `indra/llcharacter/llkeyframemotion.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 32–41 (n=10) | unknown/non-function (declaration, XML, build list or global scope) | stepped-animation | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 114–122 (n=9) | getPriority | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: client-side per-instance override wins WITHOUT touching the shared, per-asset mJointMotionList->mBasePriority (see LLMotion::setPriorityOverride) |
| 130–139 (n=10) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLUUID |
| 157–164 (n=8) | supportsExternalSampling | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 170–176 (n=7) | hasLoadFailed | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 189–223 (n=35) | valid | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Per-asset phase metadata for every loaded keyframe clip, including arbitrary user UUIDs instantiated as generic LLKeyframeMotion objects. Seed the externally controlled clip clock before activation. If the asset is still loading, onActivate() resolves this seed after the keyframe |
| 226–246 (n=21) | resetLoopSeamRepairEnabled, setLoopSeamRepairEnabled | actor-path, gaze, stepped-animation | creative-candidate | unknown; no local conditional evidence | Comment evidence: [PosePolish M4] Loop-seam repair master gate. docs/pose_polish_integration_plan.md "Milestone 4 -- Loop-seam repair".  llcharacter has no access to gSavedSettings/LLControlGroup (verified: no such usage anywhere under indra/llcharacter). Mirrors the static-setter "push" pattern u |
| 318–366 (n=49) | LoopSeamInfo, LoopSeamJointDelta | actor-path, stepped-animation | creative-candidate | unknown; no local conditional evidence | Comment evidence: ------------------------------------------------------------------------- [PosePolish M4] LoopSeamJointDelta / LoopSeamInfo docs/pose_polish_integration_plan.md "Milestone 4 -- Loop-seam repair".  Per-asset endpoint deltas and pose-mismatch gates. Velocity mismatch alone cannot b |
| 369–375 (n=7) | unknown/non-function (protected:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 530–569 (n=40) | unknown/non-function (protected:) | actor-path, stepped-animation | creative-candidate | unknown; no local conditional evidence | Comment evidence: [PosePolish M4] Loop-seam repair per-instance runtime state (the post-wrap blend window). Only ever written/read when sLoopSeamRepairEnabled is true -- see detectSeamWrap()/captureSeamPose()/applySeamEase() in llkeyframemotion.cpp. Harmless, inert data when the gate is off; never |
| 591–593 (n=3) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 3: `indra/llcharacter/llkeyframewalkmotion.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 28–43 (n=16) | unknown/non-function (declaration, XML, build list or global scope) | clones | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 52–103 (n=52) | circularDistanceUnit, wrapUnit | actor-path | creative-candidate | unknown; conditional evidence present | Comment evidence: ----------------------------------------------------------------------------- [PosePolish M3] Phase-aware locomotion constants. docs/pose_polish_integration_plan.md "Milestone 3 -- Phase-aware locomotion". All of this is only ever touched when sPhaseAwareLocomotionEnabled is true |
| 138–156 (n=19) | LLKeyframeWalkMotion::onActivate | actor-path | creative-candidate | unknown; conditional evidence present | Comment evidence: [PosePolish M3] Phase-aware locomotion start. See docs/pose_polish_integration_plan.md "Milestone 3". No-op unless sPhaseAwareLocomotionEnabled is true (pushed from newview; see the setter's doc comment in llkeyframewalkmotion.h) -- when false this whole block is skipped and mAdj |
| 160–185 (n=26) | LLKeyframeWalkMotion::onDeactivate | actor-path | creative-candidate | unknown; conditional evidence present | Comment evidence: [PosePolish M3] Only erase the shared per-character handoff record if we are still its most recent writer. Freshness (see computeLocomotionStartOffset) alone cannot tell a genuine clip-to-clip handoff (e.g. walk -> run, where the new clip is already active and writing its own fre |
| 189–212 (n=24) | LLKeyframeWalkMotion::onUpdate | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Character Mover's absolute sample already encodes distance/cadence. Bypass the legacy "Walk Speed" integrator or the built-in walk/run subclasses would warp the requested sample a second time. Keep the phase handoff fed from the effective clip time so stock and custom gait transi |
| 226–536 (n=311) | LLKeyframeWalkMotion::computeLocomotionStartOffset, LLKeyframeWalkMotion::getLocomotionPhaseInfo, LLKeyframeWalkMotion::onUpdate, LLKeyframeWalkMotion::updateLocomotionHandoff, findPlantWindow | actor-path | creative-candidate | unknown; conditional evidence present | Comment evidence: [PosePolish M3] record this clip's current gait phase so a sibling walk/run motion activating on this same character can start near a compatible plant phase. No-op unless sPhaseAwareLocomotionEnabled is true. ----------------------------------------------------------------------- |
| 600–609 (n=10) | LLWalkAdjustMotion::onUpdate | clones | creative-candidate | unknown; conditional evidence present | Edits LLFrameTimer, ALAvatarAnimationTime |
| 690–699 (n=10) | LLWalkAdjustMotion::onUpdate | clones | creative-candidate | unknown; no local conditional evidence | Edits LLSmoothInterpolation, ALAvatarAnimationTime |
| 711–720 (n=10) | LLWalkAdjustMotion::onUpdate | clones | creative-candidate | unknown; no local conditional evidence | Edits LLSmoothInterpolation, ALAvatarAnimationTime |
| 786–795 (n=10) | LLFlyAdjustMotion::onUpdate | clones | creative-candidate | unknown; conditional evidence present | Edits LLFrameTimer, ALAvatarAnimationTime |
| 797–806 (n=10) | LLFlyAdjustMotion::onUpdate | clones | creative-candidate | unknown; no local conditional evidence | Edits LLSmoothInterpolation, ALAvatarAnimationTime |

## Slice 3: `indra/llcharacter/llkeyframewalkmotion.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 33–39 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 70–90 (n=21) | resetPhaseAwareLocomotionEnabled, setPhaseAwareLocomotionEnabled | actor-path, gaze | creative-candidate | unknown; no local conditional evidence | Comment evidence: [PosePolish M3] Phase-aware locomotion master+feature gate. docs/pose_polish_integration_plan.md "Milestone 3 -- Phase-aware locomotion".  llcharacter has no access to gSavedSettings/LLControlGroup (verified: no such usage anywhere under indra/llcharacter). The one precedent for  |
| 94–156 (n=63) | LLLocomotionHandoff, LLLocomotionPhaseInfo | actor-path | creative-candidate | unknown; no local conditional evidence | Comment evidence: ------------------------------------------------------------------------- [PosePolish M3] Phase-aware locomotion start offset.  Generalizes the cadence adjuster above: rather than replacing the per-frame time warp (mAdjTimeLast / SPEED_ADJUST_TIME_CONSTANT / SPEED_ADJUST_MAX_SEC) |

## Slice 3: `indra/llcharacter/llmotioncontroller.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 145–157 (n=13) | getLastAppliedRegularRotationPriority | gaze | creative-candidate | unknown; no local conditional evidence | Comment evidence: [Machinima] Narrow gaze-yield observer: true only when 'joint' received a non-additive rotation contribution in the most recent real blend pass. Never reports the additive channel. False => caller must ALLOW (the safe compatibility default). Reads only mPoseBlender; exposes no mo |
| 173–186 (n=14) | usesNativeLifetime | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Single predicate used by every native non-loop stop/deactivate path. Public only so the owner-lifetime contract can be regression-tested without a wall-clock-dependent controller fixture. |
| 197–209 (n=13) | getGlobalTimeFactor, setGlobalTimeFactor | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Global animation speed multiplier applied to EVERY motion controller (self, other avatars, and animated objects) live each frame. Composes on top of the per-controller time factor, so it uniformly scales all skeletal animation playback in the scene. |
| 225–231 (n=7) | unknown/non-function (protected:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 269–271 (n=3) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 3: `indra/llcharacter/llpose.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 235–249 (n=15) | unknown/non-function (bool LLJointStateBlender::addJointState(const LLPointer<LLJointState>& joint_sta) | gaze | creative-candidate | unknown; no local conditional evidence | Comment evidence: [Machinima] Minimum contributing weight for a rotation state to count toward the gaze-yield observation. The blender itself skips only exact 0.f (below); this floor matches the gaze code's own activity thresholds (wBody > 0.001f) so an easing-out sliver-weight motion does not str |
| 253–263 (n=11) | LLJointStateBlender::blendJointStates | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [Machinima] Highest non-additive ROT priority reaching this joint this blend (see mLastRegularRotPriority). Stamped after the blend below. |
| 294–310 (n=17) | LLJointStateBlender::blendJointStates | gaze | creative-candidate | unknown; conditional evidence present | Comment evidence: [Machinima] Record the highest NON-additive rotation priority that actually contributes (weight above the floor) for the gaze-yield gate. mPriorities[i] is the resolved effective priority addMotion passed in. |
| 413–427 (n=15) | LLJointStateBlender::blendJointStates | gaze | creative-candidate | unknown; no local conditional evidence | Comment evidence: [Machinima] Stamp the gaze-yield observation for this joint. Stamped regardless of found_regular_rot: a joint blended with only POS/additive contributions must read "no regular ROT contributor -> allow", not resurrect an older observation. The early-out above (no states) skips th |
| 508–518 (n=11) | LLPoseBlender::addMotion, getNextJointState | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Precomputed per joint state by the owning motion: a single flag read. |
| 526–537 (n=12) | LLPoseBlender::addMotion, getNextJointState | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: getJointPriority() applies the stock rule (USE_MOTION_PRIORITY -> the motion's base priority, else the joint's own priority) and folds in any per-instance client-side priority override. It never mutates the shared per-asset data, so other avatars playing the same asset blend at t |
| 547–560 (n=14) | LLPoseBlender::blendAndApply | gaze | creative-candidate | unknown; no local conditional evidence | Comment evidence: [Machinima] Advance the blend serial once per pass so gaze-yield observations stamped this frame are distinguishable from stale ones. |
| 566–574 (n=9) | LLPoseBlender::blendAndCache | gaze | creative-candidate | unknown; no local conditional evidence | Comment evidence: [Machinima] Advance the serial here too so the gaze-yield observation stays correct if the disabled quantum path (SL-763) is ever re-enabled. |
| 577–614 (n=38) | LLPoseBlender::blendAndCache, LLPoseBlender::getLastRegularRotationPriority | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: ----------------------------------------------------------------------------- getLastRegularRotationPriority() ----------------------------------------------------------------------------- The pool is persistent across frames and keyed by LLJoint*, so a miss means the joint was n |
| 636–638 (n=3) | unknown/non-function (void LLPoseBlender::clearBlenders()) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 3: `indra/llcharacter/llpose.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 88–105 (n=18) | unknown/non-function (protected:) | gaze | creative-candidate | unknown; no local conditional evidence | Comment evidence: [Machinima] Gaze-yield observation of the last real blend: the highest NON-ADDITIVE rotation priority that actually contributed to this joint, stamped with the pose blender's blend serial so a superseded observation (the joint is no longer animated) reads as invalid instead of bl |
| 121–132 (n=12) | unknown/non-function (protected:) | gaze | creative-candidate | unknown; no local conditional evidence | Comment evidence: [Machinima] Increments once per real blend pass (blendAndApply / blendAndCache). A per-joint observation is only current if its stamped serial equals this; a stale stamp means the joint was not re-blended (its motion stopped), so gaze must be allowed on it again. |
| 148–160 (n=13) | unknown/non-function (public:) | gaze | creative-candidate | unknown; no local conditional evidence | Comment evidence: [Machinima] Gaze-yield query. Returns true and sets priority_out only when 'joint' received at least one non-additive rotation contribution in the MOST RECENT real blend pass (stamped serial == mBlendSerial). False means no data / stale / never animated -> the caller must ALLOW t |

## Slice 3: `indra/llcharacter/lltargetingmotion.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 28–34 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | clones, gaze | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 105–117 (n=13) | LLTargetingMotion::onUpdate | clones, gaze | creative-candidate | unknown; conditional evidence present | Edits LLFrameTimer, ALAvatarAnimationTime |

## Slice 3: `indra/llcommon/llstring.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 828–834 (n=7) | wstring_find_emoji_clusters | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_STRING |
| 858–988 (n=131) | find_emoji_cluster_containing, is_regional_indicator, may_be_cluster_interior, wstring_set_grapheme_lookup_step_sink | clones, composition | creative-candidate | unknown; conditional evidence present | Comment evidence: [TextLinear] Local (bounded) cluster lookup.  The single-position helpers below (align / step / range_at without a precomputed cluster list) used to call wstring_find_emoji_clusters on the whole string, i.e. O(N) per cursor move. LLTextBase::setCursorPos runs one of them on every |
| 1003–1017 (n=15) | wstring_step_grapheme_forward | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [TextLinear] bounded; identical result to the cluster-list overload. |
| 1032–1045 (n=14) | wstring_step_grapheme_backward | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [TextLinear] bounded; identical result to the cluster-list overload. |
| 1059–1071 (n=13) | wstring_grapheme_align_backward | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [TextLinear] bounded; identical result to the cluster-list overload. |
| 1085–1097 (n=13) | wstring_grapheme_align_forward | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [TextLinear] bounded; identical result to the cluster-list overload. |
| 1119–1133 (n=15) | wstring_emoji_range_at | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [TextLinear] bounded; identical result to the cluster-list overload. |

## Slice 3: `indra/llplugin/llpluginclassmedia.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 198–204 (n=7) | LLPluginClassMedia::idle | stepped-animation | creative-candidate | unknown; no local conditional evidence | Edits LLTimer |
| 255–265 (n=11) | LLPluginClassMedia::idle | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 345–382 (n=38) | LLPluginClassMedia::enableMediaFrames, LLPluginClassMedia::retainBits, LLPluginClassMedia::retryDirty | vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: Does not navigate, stop, seek or resize the current player. The plugin tags its retained current-load pixels before activating our new path. |
| 832–896 (n=65) | LLPluginClassMedia::configurePrivateFile, LLPluginClassMedia::loadURI, LLPluginClassMedia::logLoopSeamSummary, LLPluginClassMedia::privateFileFailureReason, LLPluginClassMedia::privateFileVerdict | stepped-animation, vcam | creative-candidate | unknown; conditional evidence present | Edits LLPluginLoopSeam, LLTimer, LL_INFOS, LL_ENDL, LLUUID, LLPluginClassMedia, LLPluginMessage, LLPLUGIN_MESSAGE_CLASS_MEDIA |
| 1196–1271 (n=76) | LLPluginClassMedia::receivePluginMessage | stepped-animation, vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: Only a previously admitted load may hold through its explicit Loop edit/reprepare. Pending on an arbitrary new load never qualifies. |
| 1401–1407 (n=7) | LLPluginClassMedia::receivePluginMessage | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 3: `indra/llplugin/llpluginclassmedia.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 30–39 (n=10) | unknown/non-function (declaration, XML, build list or global scope) | stepped-animation, vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 74–91 (n=18) | getDirtyLoadToken, getLayoutEpoch, getLoadToken, getVideoHeight, getVideoSarDen, getVideoSarNum, getVideoWidth, tracksMediaFrames | vcam | creative-candidate | unknown; no local conditional evidence | Edits LLPluginSharedMemory, LLRect, LLMediaFrame |
| 170–193 (n=24) | privateFileCloseFence, privateFileRetaining, privateFileSeekable | stepped-animation, vcam | creative-candidate | unknown; no local conditional evidence | Edits LLPluginFileClose, LLPluginLoopSeam, LLPluginFileAdmission |
| 517–529 (n=13) | unknown/non-function (protected:) | vcam | creative-candidate | unknown; no local conditional evidence | Edits LLMediaFrame |

## Slice 3: `indra/newview/alfloaterexploresounds.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 9–20 (n=12) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits gDirUtilp, LLFile, LLMenuGL, LLContextMenu, gMenuHolder, LLViewerMenuHolderGL |
| 74–85 (n=12) | ALFloaterExploreSounds::~ALFloaterExploreSounds | unknown | unknown-review | unknown; conditional evidence present | Edits LLContextMenu |
| 97–116 (n=20) | ALFloaterExploreSounds::postBuild | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: right-click the sound list -> "Show in Cache Folder" (reveal the decoded cache/sounds/<asset>.dsf in the OS file manager). Same LLContextMenu idiom as the Animation Explorer's animation_list menu. |
| 546–600 (n=55) | ALFloaterExploreSounds::onScrollListRightClicked, ALFloaterExploreSounds::showSelectedInCacheFolder | actor-path | creative-candidate | unknown; conditional evidence present | Comment evidence: right-click on the sound list: select the row under the cursor (so the menu acts on what was clicked) and pop the context menu. Mirrors AnimationExplorer::onScrollListRightClicked. Reveal the selected sound's decoded cache file in the OS file manager. Decoded sounds live in the d |

## Slice 3: `indra/newview/app_settings/shaders/class1/deferred/shadowUtil.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 30–92 (n=63) | unknown/non-function (uniform sampler2DShadow shadowMap0;) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [ProbeCost] Dedicated opt-in uniforms; stock matrices and selection stay untouched. [BDMerge NSpot] extended projector shadow slots [ShadowDist P1] Control A: cascade blend width as a fraction of each split (stock 0.25 -> bands 0.75x .. 1.25x). Uploaded from the sampled pack's Sh |
| 95–248 (n=154) | estimateSpotBlockerGap, pcfShadow | capture-sidecar, clones | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge Batch 2] Feature 1 - soft (contact-hardening + filled) shadows. All gated: soft_shadow_enable == 0 -> the classic hard 5-tap paths run byte-identically, so default behavior is unchanged. receiver depth (light source-size term) instead of crushing to pure black (0 = no fi |
| 256–751 (n=496) | defined, detailSpotTap, estimateDetailSpotBlockerGap, pcfShadowC, pcfSpotShadow, probeReuseBand, sampleDirectionalShadow, sampleDirectionalShadowReuse, sunBandC, sunUnitsCascade | capture-sidecar, composition, shafts-volumetrics, vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: [ShadowDist P2] Control C sun kernel: same tap patterns as pcfShadow (classic 5-tap / Poisson / Vogel, same fill guard) but - depth bias in world metres, converted per cascade and (perspective) per receiver: stc.z -= bias * k [/ w], slope-scaled by the kernel radius actually samp |
| 763–770 (n=8) | sampleDirectionalShadow | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 839–1131 (n=293) | detailSpotReceiver, directionalShadowCascade, sampleDirectionalShadow, sampleSpotShadow | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: ===== [ShadowDist P2] UNITS (and [P3] SUBJECT) path ===== [ShadowDist P2 fix, Codex P2] selection coordinates: world-unit cascades select and weight bands with the UN-offset receiver; cascades left on the legacy kernel (RenderShadowUnitsScope == 1 -> cascades 1-3) keep the legacy |
| 1133–1189 (n=57) | sampleSpotShadow | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: KEEP IN SYNC: this 10-case GLSL dispatch MUST mirror LLPipeline::spotShadowMapIndex(): spot slot s -> shadowMap[4 + s]. |
| 1201–1425 (n=225) | defined, nonpcfShadow, nonpcfShadowAtPos, nonpcfShadowC, pcfSpotShadowConservative, sampleSpotShadowConservative | roto-tron, shafts-volumetrics | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge G3.3 ConservativeShadow] Volume-march spot-shadow sampler for the AIRBORNE raymarch samples of projectorVolumetricF. The surface pipeline above (sampleSpotShadow -> pcfSpotShadow) is built for lit RECEIVER SURFACES and has four separate mechanisms that each read "lit" fo |

## Slice 3: `indra/newview/app_settings/shaders/class2/deferred/alphaF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–4 (n=4) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 24–65 (n=42) | unknown/non-function (declaration, XML, build list or global scope) | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: Particle ribbons supply a camera-facing normal independently of winding. |
| 84–132 (n=49) | unknown/non-function (uniform vec4 light_position[8];) | looks, rig, roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: [RigRim] shared rim helper (deferredUtil.glsl) [RigRimQ] per-pixel quality inputs (deferredUtil.glsl; struct must match there) |
| 137–195 (n=59) | calcPointLightOrSpotLight | looks, rig, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: [RigRim] 'rim' = this light unit's rig rim params (rig_rim_lights[i]); zero for non-rig lights.  NOTE: 'v' is the eye-space POSITION here (legacy naming). Invert setupHWLights' attenuation encoding to recover shader range R. |
| 255–261 (n=7) | calcPointLightOrSpotLight | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 267–302 (n=36) | calcPointLightOrSpotLight | looks, rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [RigRim] additive rim (cone + attenuation weighted, no shadow).  No glossiness on this path, so roughness = 1 (softest).  Independent of 'da' so wrap can reach past the terminator.  Mode 0 never enters. |
| 305–322 (n=18) | main | looks, roto-tron, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: WP2 material contract: after Actor FX, before G-buffer packing/forward lighting. |
| 326–338 (n=13) | main | rig | creative-candidate | unknown; no local conditional evidence | Comment evidence: As in pbralphaF: culling controls whether a back face can reach lighting. [RigRimQ] Toksvig input, unconditional at main scope and BEFORE the alpha discards below (review P2-5); unused unless rig_rim_mode != 0. |
| 348–380 (n=33) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 423–447 (n=25) | defined, main | looks, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: Transparent legacy surfaces have no GBuffer emissive attachment.  Add the styled emission to their forward beauty result; the separate glow sub-pass only publishes bloom metadata and never changes coverage. |
| 457–463 (n=7) | main | sensor | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 485–514 (n=30) | main | actor-path, crowd, looks, rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [RigRimQ] forward-alpha local pixel: no normal map on this path, so the vertex normal IS the shading normal; only the Toksvig length carries information.  rim_fw hoisted to main scope; mode 0 keeps neutral. [RigRim] trailing rig_rim_lights[i] argument (+ [RigRimQ] rim_px) |
| 520–545 (n=26) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 551–573 (n=23) | main | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: R = 0 is written but masked off by the pool; G claims forward coverage proportional to how much of this pixel this layer actually covers. |

## Slice 3: `indra/newview/fsfloaterposer.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–38 (n=14) | unknown/non-function (declaration, XML, build list or global scope) | actor-path | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 45–51 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 54–61 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 78–96 (n=19) | switchboardSlotOccupancy | actor-path | creative-candidate | unknown; conditional evidence present | Edits ALDirectorAnimSwitcher |
| 99–112 (n=14) | FSFloaterPoser::FSFloaterPoser | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 146–165 (n=20) | FSFloaterPoser::isEditingAvatar, FSFloaterPoser::~FSFloaterPoser | unknown | unknown-review | unknown; conditional evidence present | Edits LLVOAvatar |
| 202–213 (n=12) | FSFloaterPoser::postBuild, setCommitCallback | unknown | unknown-review | unknown; no local conditional evidence | Edits LLComboBox, LLButton, LLTextBox, LLUICtrl, LLSD |
| 219–236 (n=18) | FSFloaterPoser::postBuild, setCommitCallback | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge] Local animation playback tab |
| 324–330 (n=7) | FSFloaterPoser::onOpen | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1121–1129 (n=9) | FSFloaterPoser::onPoseMenuAction | unknown | unknown-review | unknown; conditional evidence present | Edits LLSD |
| 1329–1335 (n=7) | FSFloaterPoser::loadPoseFromXml | unknown | unknown-review | unknown; conditional evidence present | Edits gDirUtilp, LL_PATH_USER_SETTINGS, LLSD, LLVector3, LLQuaternion, LLSDSerialize, LLSDParser, LL_WARNS, LL_ENDL |
| 1432–1445 (n=14) | FSFloaterPoser::havePermissionToAnimateOtherAvatar | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge] Posing is applied as a LOCAL motion on our own copy of the target's skeleton (FSPosingMotion via LLCharacter::startMotion) and is never transmitted to the sim, so posing a nearby avatar only affects how they render in THIS viewer. When enabled, allow posing any same-reg |
| 1592–1601 (n=10) | FSFloaterPoser::onToggleLoadSavePanel | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 2447–2699 (n=253) | FSFloaterPoser::onAnimPlay, FSFloaterPoser::onAnimSendToSwitchboard, FSFloaterPoser::onAnimStop, FSFloaterPoser::onAnimStopAll, FSFloaterPoser::onPoseSendToSwitchboard, FSFloaterPoser::refreshAnimationList, FSFloaterPoser::refreshSwitchboardSlotCombo | actor-path | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge] Local animation playback: gather every animation asset UUID currently signaled on nearby avatars (per-avatar mSignaledAnimations) and animesh (the separate LLObjectSignaledAnimationMap), and list them for local playback. animation asset id -> number of nearby characters |

## Slice 3: `indra/newview/llagent.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 26–37 (n=12) | unknown/non-function (declaration, XML, build list or global scope) | stepped-animation, tracking, weather | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 70–76 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 293–299 (n=7) | handleSlowMotionAnimation | stepped-animation | creative-candidate | unknown; conditional evidence present | Edits gAgentAvatarp, ALAvatarAnimSpeed |
| 815–838 (n=24) | LLAgent::moveUp | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge C2] Nimble: skip the local pre-jump animation wind-up on jump input, same as the always-play case above already does at completion. Donor: Catznip CATZ-400 (ba7dcf52e1, "Add option to turn off pre-jumping"), ported as a jump-input-time flag instead of a standing PlayPrej |
| 1087–1103 (n=17) | LLAgent::setRegion | rig, tracking, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: Restore before the agent coordinate frame / region origin changes. |
| 2770–2785 (n=16) | LLAgent::requestStopMotion | stepped-animation | creative-candidate | unknown; conditional evidence present | Comment evidence: Includes a failed-build original fallback. Intent changes now; local cleanup and STOP(original), for Inworld only, are deferred until idle. |
| 3413–3423 (n=11) | LLAgent::sendAnimationRequests | clones, stepped-animation | creative-candidate | unknown; conditional evidence present | Edits ALReflectAnim, LL_WARNS, LL_ENDL |
| 3437–3447 (n=11) | LLAgent::sendAnimationRequest | clones, stepped-animation | creative-candidate | unknown; conditional evidence present | Edits ALReflectAnim, LL_WARNS, LL_ENDL |
| 4329–4339 (n=11) | LLAgent::handleTeleportFinished | capture-sidecar, rig | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ReShadeReset] Covers local and cross-region teleports without treating seamless region crossings as teleports. |
| 4783–4791 (n=9) | LLAgent::setTeleportState | tracking, weather | creative-candidate | unknown; conditional evidence present | Edits ALPanelWeatherSettings, ALProjectorTracker |

## Slice 3: `indra/newview/llagentcamera.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 24–41 (n=18) | unknown/non-function (declaration, XML, build list or global scope) | camera | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge C3] Camera QoL: X/Y/Z mouselook offset. Donor: novel (no BD source - I:\black-dragon has no mouselook offset feature at merge time); implemented fresh in calcCameraPositionTargetGlobal()'s CAMERA_MODE_MOUSELOOK branch, additive over the stock head_offset assembly. Gated  |
| 44–51 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | camera | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 222–233 (n=12) | LLAgentCamera::init | camera | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge B6] BD Bone Camera initial value (live updates via listener) [BDMerge B8] BD Cinematic Head Tracking |
| 1294–1325 (n=32) | LLAgentCamera::updateCamera | camera | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge B8s] BD camera straight-lift (donor: Black Dragon, verbatim llagentcamera.cpp updateCamera). Master gate for the verbatim-BD camera fidelity corrections in this function: BD's roll-reset + bone/cinematic if/else-if nesting, the un-normalized cinematic up-vector, and the  |
| 1573–1657 (n=85) | LLAgentCamera::updateCamera | camera | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge B8s] BD camera straight-lift (donor: Black Dragon, verbatim llagentcamera.cpp updateCamera). Bone camera and cinematic head tracking nest under ONE third-person / focus-on-avatar guard that first zeroes any accumulated camera roll (BD reverts key-roll here so it cannot f |
| 1776–1868 (n=93) | LLAgentCamera::updateCamera | camera | creative-candidate | unknown; conditional evidence present | Comment evidence: Camera Shake is applied only to the final render-camera pose. The agent camera's resolved position, focus, collision, movement, and simulator state remain untouched. Only step/apply on frames where the agent camera pose was actually re-resolved this frame. After flycam exit the v |
| 2105–2133 (n=29) | LLAgentCamera::calcCameraPositionTargetGlobal | camera | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge C3] X/Y/Z mouselook camera offset. Donor: novel - no BD source exists for this (I:\black-dragon has no "mouselook offset" / "CameraOffsetMouselook" setting or code at merge time); implemented fresh following this merge's BDMerge*/LLCachedControl conventions. Purely addit |
| 2509–2523 (n=15) | LLAgentCamera::getAvatarRootPosition | composition | creative-candidate | unknown; conditional evidence present | Comment evidence: [ClientHeight] "Bind Camera" off excludes BOTH the stock hover and the local (client-side) offset (a world-vertical lift, so a plain world-Z subtraction). |
| 3526–3528 (n=3) | unknown/non-function (void LLAgentCamera::loadCameraPosition()) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 3: `indra/newview/llagentcamera.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 127–141 (n=15) | unknown/non-function (public:) | camera | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge B6] BD Bone Camera: character joint index the camera focus follows; -1 = off. Written by the CameraFollowJoint settings listener. [BDMerge B8] BD Cinematic Head Tracking: camera aim/up-vector subtly follow the head, roll clamped by the lerp factors (1 = fully upright). |
| 287–293 (n=7) | getRollAngle | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 3: `indra/newview/lldrawpoolalpha.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–36 (n=12) | unknown/non-function (declaration, XML, build list or global scope) | rig, sensor, shafts-volumetrics | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 49–62 (n=14) | unknown/non-function (declaration, XML, build list or global scope) | actor-path, rig, roto-tron | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 64–74 (n=11) | unknown/non-function (bool LLDrawPoolAlpha::sShowDebugAlpha = false;) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [AlphaVel] Shared velocity texture-matrix helpers; unlike TexSetup these do not depend on beauty-pass shader pointers having been initialized. |
| 120–136 (n=17) | prepare_alpha_shader | looks, rig, shafts-volumetrics | creative-candidate | unknown; no local conditional evidence | Comment evidence: [Cine Haze] prime every alpha-pool program once per pass, including the glow-only programs (deferredEnvironment == false) that never go through bindDeferredShader() but still call applySkyAndWaterFog() for its transmittance (their fogged(c) - fogged(0) difference keeps only that) |
| 161–169 (n=9) | unknown/non-function (static void prepare_alpha_shader(LLGLSLShader* shader, bool deferredEnvironment,) | looks | creative-candidate | unknown; no local conditional evidence | Edits LLStaticHashedString |
| 188–228 (n=41) | LLDrawPoolAlpha::renderPostDeferred | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: postSort left the shared lists in legacy order; switch them to the interleaved order only for the consumer that performs the merge. Shared PBR is optional and published only as one complete family. Prime every scalar/slot base program here; prepare_alpha_shader also visits its sk |
| 253–297 (n=45) | LLDrawPoolAlpha::renderPostDeferred | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge] Gated 3-pass alpha ordering (port of AYAstorm double-alpha fix, mayatonton PR #122). When on, POST_WATER non-rigged BLEND is split around the rigged pass: SIM non-rigged (background windows/foliage) -> rigged (hair/ clothing) -> worn attachment non-rigged (eyelash prims |
| 314–752 (n=439) | LLDrawPoolAlpha::beginVelocityPass, LLDrawPoolAlpha::endVelocityPass, LLDrawPoolAlpha::forwardRender, LLDrawPoolAlpha::renderVelocity, LLDrawPoolAlpha::renderVelocityGroups | capture-sidecar, looks, rig, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: [AlphaVel] Programs are selected per DrawInfo because one sorted alpha stream can interleave legacy/PBR, scalar/indexed, and rigid/rigged faces. [AlphaVel] Leave the same unbound/program-neutral contract as other velocity pools before renderGeomVelocity restores its modelview. Ma |
| 762–769 (n=8) | LLDrawPoolAlpha::forwardRender | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: in interleaved mode depth writes are decided per group inside renderAlpha |
| 773–787 (n=15) | LLDrawPoolAlpha::forwardRender | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge] In the gated 3-pass path the non-rigged pass runs twice (SIM then attachment); fire the debug highlight only on the attachment pass so it isn't drawn twice. ATTACHMENT_ALL is the stock/gate-off single non-rigged pass. |
| 901–912 (n=12) | Draw | looks | creative-candidate | unknown; conditional evidence present | Edits LLRenderPass |
| 1001–1013 (n=13) | LLDrawPoolAlpha::drawEmissive | looks | creative-candidate | unknown; conditional evidence present | Edits LLRenderPass |
| 1015–1063 (n=49) | LLDrawPoolAlpha::renderEmissives | looks | creative-candidate | unknown; conditional evidence present | Edits LLShaderMgr, LLGLSLShader, LLRenderPass, gDeferredEmissiveIndexedProgram, LLViewerTexture, LLViewerFetchedTexture, gGL |
| 1066–1085 (n=20) | LLDrawPoolAlpha::renderPbrEmissives | looks | creative-candidate | unknown; conditional evidence present | Edits LLRenderPass |
| 1088–1094 (n=7) | LLDrawPoolAlpha::renderRiggedEmissives | unknown | unknown-review | unknown; no local conditional evidence | Edits LLGLSLShader, LLShaderMgr |
| 1098–1151 (n=54) | LLDrawPoolAlpha::renderRiggedEmissives | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: Matrix-palette uniforms are program state. A same-skin batch on a newly selected scalar/indexed program must upload again. |
| 1156–1162 (n=7) | LLDrawPoolAlpha::renderRiggedPbrEmissives | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1164–1320 (n=157) | LLDrawPoolAlpha::renderActorFxEmissives, LLDrawPoolAlpha::renderAlpha, LLDrawPoolAlpha::renderRiggedPbrEmissives, ~RestoreForwardWorld | actor-path, capture-sidecar, looks, rig, roto-tron, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: Match the beauty alpha draw's custom-blend cutoff override. Ordinary GLTF BLEND keeps the -1 supplied by material bind. Mirror the beauty shader's cutout policy. Legacy material BLEND has no cutoff; fullbright tests texture alpha; ordinary lit alpha tests the complete texture * f |
| 1323–1346 (n=24) | LLDrawPoolAlpha::renderAlpha | unknown | unknown-review | unknown; conditional evidence present | Edits LLCullResult, gPipeline, LLEnvironment |
| 1349–1464 (n=116) | LLDrawPoolAlpha::renderAlpha | capture-sidecar, clones, looks, rig, roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: depth-write conditions that apply to every group in this pass; in merged mode, stamped rigged groups additionally write depth (see below) merged mode: per-group depth-write guard, re-emplaced only when the write state changes (one glDepthMask per run, not per group) share one ava |
| 1487–1524 (n=38) | LLDrawPoolAlpha::renderAlpha | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: merged mode: stamped rigged groups write depth so attachment-order layering holds; an unstamped group has no defined position in the rigged order and must not depth-reject geometry behind it (it still blends). Non-merged passes keep the caller's depth state (set in forwardRender) |
| 1536–1556 (n=21) | LLDrawPoolAlpha::renderAlpha | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge] 3-pass attachment filter (non-rigged path only). Gate off / rigged path use ATTACHMENT_ALL, which skips nothing, so this is byte-identical to stock when disabled. |
| 1560–1567 (n=8) | LLDrawPoolAlpha::renderAlpha | unknown | unknown-review | unknown; conditional evidence present | Edits LLGLTFMaterial, ALPHA_MODE_BLEND, LLPipeline |
| 1575–1582 (n=8) | LLDrawPoolAlpha::renderAlpha | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1662–1726 (n=65) | LLDrawPoolAlpha::renderAlpha | capture-sidecar, looks, rig, roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: Replace/Cover replays the actor through the frame-ready shared proxy. Preserve native DoF depth for every nonzero Cover, but suppress it at opacity zero so an intentionally invisible actor cannot occlude/blur the background. [ActorMask] Eligibility was evaluated under the real sh |
| 1732–1758 (n=27) | LLDrawPoolAlpha::renderAlpha | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: [Cine Haze] additive-blended draws (dst factor ONE: additive particles / glow faces) must not accumulate the atmosphere colour once per layer -- tell the shader to apply transmittance only.  Value-cached uniform; only touched around additive draws. |
| 1786–1812 (n=27) | LLDrawPoolAlpha::renderAlpha | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: Zero-authored-glow faces have no TYPE_EMISSIVE stream. Queue only effective glow looks into the lightweight colour-stream shader, avoiding blanket duplicate draws. |
| 1817–1843 (n=27) | LLDrawPoolAlpha::renderAlpha | capture-sidecar, roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: [ActorMask] Emissive replays and following passes start with world. M3: the emissive sub-passes below run single-output shaders (renderEmissives / renderPbrEmissives / renderRiggedEmissives / renderRiggedPbrEmissives all declare only frag_color). With attachment 1 still write-ena |
| 1872–1905 (n=34) | LLDrawPoolAlpha::renderAlpha | capture-sidecar, looks | creative-candidate | unknown; conditional evidence present | Comment evidence: reopen the sidecar for any subsequent diffuse-bearing draws |

## Slice 3: `indra/newview/llflexibleobject.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–31 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | stepped-animation | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 768–778 (n=11) | LLVolumeImplFlexible::doUpdateGeometry | stepped-animation | creative-candidate | unknown; conditional evidence present | Edits ALSteppedAnimRuntime |

## Slice 3: `indra/newview/llfloater360capture.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–36 (n=12) | unknown/non-function (declaration, XML, build list or global scope) | actor-path | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge B3] Freeze World interaction guard: don't clear FreezeTime while the Freeze World toggle (UseFreezeWorld) is holding the scene. donor: Black Dragon (NiranV Dean) b4cfc2cb83. See doc/BD_MERGE_PATCHLOG.md. |
| 606–613 (n=8) | LLFloater360Capture::capture360Images | actor-path | creative-candidate | unknown; no local conditional evidence | Edits LLActorMover |
| 841–852 (n=12) | LLFloater360Capture::freezeWorld | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge B3] unless Freeze World is holding the scene: its own toggle owns FreezeTime for the duration of the freeze |

## Slice 3: `indra/newview/llfloaterhoverheight.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–7 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | composition | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 29–49 (n=21) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLButton, LLComboBox, LLSliderCtrl, LLTextBox, LLFloaterHoverHeight |
| 56–92 (n=37) | unknown/non-function (public:) | composition | creative-candidate | unknown; no local conditional evidence | Comment evidence: virtual*/ void onOpen(const LLSD& key); [ClientHeight] Local (client-side) avatar height. No draw() override: every refresh below is event-driven (open, drop-down opening, Reset all, name-cache callback, store change). |

## Slice 3: `indra/newview/llpreviewanim.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 27–37 (n=11) | unknown/non-function (declaration, XML, build list or global scope) | stepped-animation | creative-candidate | unknown; no local conditional evidence | Edits LLMotion |
| 58–70 (n=13) | LLPreviewAnim::postBuild | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Default the client-side priority selector to "Baked (default)" (first item, value -1) so an untouched floater is byte-identical to stock. |
| 73–90 (n=18) | LLPreviewAnim::postBuild, setCommitCallback | stepped-animation | creative-candidate | unknown; conditional evidence present | Edits LLUICtrl, LLSD, LLInventoryItem, ALReflectAnim, ALReflect |
| 128–172 (n=45) | LLPreviewAnim::play | stepped-animation | creative-candidate | unknown; conditional evidence present | Comment evidence: Client-side, per-instance priority for this playback. "Baked (default)" is -1 == no override == byte-identical to stock. This never mutates the shared per-asset data; a "Play in World" still shows other viewers the baked priority. See LLMotion::setPriorityOverride. Apply immediat |
| 180–193 (n=14) | LLPreviewAnim::draw | stepped-animation | creative-candidate | unknown; conditional evidence present | Edits ALReflectAnim, LLUUID |
| 204–212 (n=9) | LLPreviewAnim::draw | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: (re)apply the chosen client-side priority now that the motion instance actually exists (async-load / in-world case) |
| 223–238 (n=16) | LLPreviewAnim::cleanup, LLPreviewAnim::refreshFromItem | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Preload motion Reflect obtains bytes without creating/loading the original on self. |
| 248–263 (n=16) | LLPreviewAnim::onClose | stepped-animation | creative-candidate | unknown; conditional evidence present | Edits gAgentAvatarp, gAgent, LLUUID, ALReflectAnim, LLUICtrl |

## Slice 3: `indra/newview/llselectmgr.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 29–36 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | camera, tracking | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 58–64 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 106–112 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | clones | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 130–139 (n=10) | selectionAllLocalPreview | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: A null node means the selection is NOT proven all-local -- fall back to a normal simulator send rather than silently suppressing it (matches the stricter selectionAllLocalMeshPreview()). |
| 141–208 (n=68) | selectionAllLocalMeshPreview, selectionHasGhostManipProxy, synthesizeLocalPreviewNode | clones, looks, rig | creative-candidate | unknown; conditional evidence present | Comment evidence: True only if the selection is non-empty and consists entirely of local MESH previews (LLLocalMeshMgr). Narrower than selectionAllLocalPreview(): a Ghost Studio manipulation proxy is client-only (isLocalOnly) but is NOT a mesh preview, so it must never be routed to the Local Mesh  |
| 266–278 (n=13) | deleteLocalPreviewSelection | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: Defence in depth: only actual mesh previews belong to LLLocalMeshMgr. The dispatch already filters to all-mesh-preview selections, but a ghost manip proxy (client-only, not a mesh preview) must never be despawned here. |
| 1288–1301 (n=14) | LLSelectMgr::setHoverObject | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Functional self-test (in-world): hovering must never request object properties from the simulator (requestObjectPropertiesFamily below). |
| 4376–4410 (n=35) | LLSelectMgr::selectDelete | clones, crowd | creative-candidate | unknown; conditional evidence present | Comment evidence: Client-only local mesh previews have no sim counterpart; delete them locally instead of sending a DeRez the sim would ignore. [GhostStudio] Delete on the manip proxy removes the SELECTED GHOST instance (the proxy is only an editing handle). After the stock-tool handoff, this norm |
| 4559–4586 (n=28) | LLSelectMgr::selectForceDelete | clones, crowd | creative-candidate | unknown; conditional evidence present | Edits ALGhostStudio, LLUUID, ALGhostGroupModel |
| 4803–4810 (n=8) | LLSelectMgr::selectDuplicate | tracking | creative-candidate | unknown; no local conditional evidence | Comment evidence: Duplicate baselines are reused by later transform sends; never cache a driven pose. |
| 4826–4836 (n=11) | LLSelectMgr::selectDuplicate | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: Client-only local previews: duplicate locally. The ObjectDuplicate send below is swallowed for an all-local selection (sendListToRegions gate), which would otherwise make Ctrl+D / shift-drag-copy a silent no-op. Local MESH previews: duplicate locally. The ObjectDuplicate send bel |
| 4899–4906 (n=8) | LLSelectMgr::repeatDuplicate | tracking | creative-candidate | unknown; no local conditional evidence | Comment evidence: Duplicate baselines are reused by later transform sends; never cache a driven pose. |
| 4929–4935 (n=7) | LLSelectMgr::repeatDuplicate | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 5071–5078 (n=8) | LLSelectMgr::sendMultipleUpdate | tracking | creative-candidate | unknown; conditional evidence present | Comment evidence: Last-resort barrier: an unhooked editor must never send a tracked pose. |
| 5527–5539 (n=13) | LLSelectMgr::sendAttach | tracking | creative-candidate | unknown; conditional evidence present | Comment evidence: Also protect attachment requests using an alternate selection handle. |
| 5803–5812 (n=10) | LLSelectMgr::saveSelectedObjectTransform | tracking | creative-candidate | unknown; conditional evidence present | Comment evidence: Restore before even the PICK baseline of a click that starts a drag. |
| 6029–6036 (n=8) | LLSelectMgr::packObjectIDAndRotation | tracking | creative-candidate | unknown; conditional evidence present | Edits ALProjectorTracker |
| 6193–6199 (n=7) | LLSelectMgr::sendListToRegions, apply | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 6207–6213 (n=7) | LLSelectMgr::sendListToRegions, apply | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 6617–6623 (n=7) | LLSelectMgr::processObjectPropertiesFamily | camera | creative-candidate | unknown; no local conditional evidence | Edits LLCinematicCamera |
| 7239–7248 (n=10) | LLSelectMgr::generateSilhouette | unknown | unknown-review | unknown; no local conditional evidence | Edits LLMatrix4 |
| 7305–7314 (n=10) | LLSelectNode::LLSelectNode | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 7719–7739 (n=21) | LLSelectNode::renderOneSilhouette | tracking | creative-candidate | unknown; conditional evidence present | Comment evidence: Reuse the captured contour, carrying it with the current rigid pose. This also handles an independently driven child outside Build tools. |
| 7929–7935 (n=7) | LLSelectMgr::updateSelectionCenter | tracking | creative-candidate | unknown; no local conditional evidence | Edits ALProjectorTracker |
| 8272–8278 (n=7) | LLSelectMgr::canSelectObject | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 9149–9155 (n=7) | LLSelectMgr::selectionMove | tracking | creative-candidate | unknown; no local conditional evidence | Edits ALProjectorTracker |
| 9274–9280 (n=7) | LLSelectMgr::sendSelectionMove | tracking | creative-candidate | unknown; conditional evidence present | Edits ALProjectorTracker |
| 9381–9383 (n=3) | unknown/non-function (bool LLCheckIdenticalFunctor<class LLFace *>::same(class LLFace* const & a, clas) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 3: `indra/newview/llsurfacepatch.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 32–38 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | rig | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1057–1065 (n=9) | LLSurfacePatch::updateVisibility | composition, rig | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ProbeOnDemand] LT: a camera-distance terrain stride change is LOD churn; real edits (dirty(), dirtyPatch(), composition) stay events. |

## Slice 3: `indra/newview/llviewerjoystick.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 28–38 (n=11) | unknown/non-function (declaration, XML, build list or global scope) | camera | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 51–128 (n=78) | unknown/non-function (typedef enum e_joystick_driver_state) | camera | creative-candidate | unknown; no local conditional evidence | Edits LLViewerJoystick, LLSingleton, LLSD |
| 141–159 (n=19) | unknown/non-function (public:) | camera | creative-candidate | unknown; no local conditional evidence | Edits ALFlycamWeighted, LLFlycamDrone |
| 170–252 (n=83) | unknown/non-function (protected:) | camera, gaze | creative-candidate | unknown; no local conditional evidence | Comment evidence: WM_DEVICECHANGE (via LLViewerWindow::handleDeviceChange) marks the device list dirty; pollForXboxController only enumerates DirectInput when this is set. Eliminates the periodic main-thread DI8 EnumDevices walk (a filming micro-stutter every few seconds) — enumeration now happens |
| 262–307 (n=46) | unknown/non-function (private:) | camera | creative-candidate | unknown; no local conditional evidence | Comment evidence: SETTINGS [1 0 2 4  3  5] [Z X Y RZ RX RY] Device prefs Modes prefs |

## Slice 3: `indra/newview/llviewermessage.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–35 (n=11) | unknown/non-function (declaration, XML, build list or global scope) | rig, weather | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 45–51 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 56–62 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | actor-path | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 395–405 (n=11) | give_money | unknown | unknown-review | unknown; conditional evidence present | Edits LLViewerObject, gObjectList |
| 3349–3355 (n=7) | process_agent_movement_complete | weather | creative-candidate | unknown; no local conditional evidence | Edits ALWeatherBridge |
| 3994–4003 (n=10) | process_kill_object | rig | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ProbeOnDemand] An authoritative kill clears cache provenance even when the object is currently culled (no drawable to note). |
| 4351–4369 (n=19) | process_avatar_animation | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge F7] Animation Explorer "Capture all avatars": each AvatarAnimation message re-lists EVERYTHING currently playing on that avatar, so snapshot the previous signaled set before it's cleared to detect genuinely NEW anims in the non-self branch below. Default off = stock beha |
| 4371–4428 (n=58) | process_avatar_animation | actor-path | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge C2] Nimble (landing half): skip pre-jump/landing/stand-up waits. Donor: Catznip CATZ-400 (ba7dcf52e1, "Add option to turn off pre-jumping"), carried by Black Dragon. When the server signals one of these transitional animations for OUR OWN avatar, don't record it in mSign |
| 4465–4478 (n=14) | process_avatar_animation | actor-path | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge F7] Animation Explorer: record newly-started animations sourced by an object/avatar onto OUR OWN avatar (this branch is already gated on avatarp->isSelf() above). Donor: animationexplorer.cpp / RecentAnimationList::addAnimation(), called from the equivalent spot in FS's  |
| 4493–4509 (n=17) | process_avatar_animation | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge F7] Animation Explorer "Capture all avatars": record anims newly signaled on OTHER avatars (machinima actors). This branch carries no AnimationSourceList data, so the animated avatar itself is passed as the "played by" source. MAX_ANIMATIONS trimming happens inside addAn |
| 5049–5090 (n=42) | log_transaction_durable | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge Transactions] Durable, always-on money-transaction log. The in-world money notifications are transient toasts, and the Transaction Log floater only records while it is open+visible and keeps nothing on disk - so a missed toast or a crash loses all record of what you paid |
| 5144–5154 (n=11) | process_money_balance_reply_extended | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge Transactions] Durable record BEFORE the Notify* popup gates below, so it is logged even when the popups are disabled, missed, or a crash follows. |
| 6679–6685 (n=7) | process_teleport_local | weather | creative-candidate | unknown; no local conditional evidence | Edits ALWeatherBridge |

## Slice 3: `indra/newview/llviewerobject.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–36 (n=12) | unknown/non-function (declaration, XML, build list or global scope) | clones, stepped-animation, tracking | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 58–64 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | rig | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 69–75 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 78–84 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | clones | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 103–109 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | rig | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 123–149 (n=27) | unknown/non-function (declaration, XML, build list or global scope) | clones | creative-candidate | unknown; no local conditional evidence | Edits LLViewerObject, LLSD, LLMaterialID, LLUUID, LLPointer, LLViewerTexture |
| 240–251 (n=12) | LLViewerObject::createObject | clones | creative-candidate | unknown; conditional evidence present | Edits LLGhostAvatar |
| 960–994 (n=35) | LLViewerObject::invalidateWorldShadowBounds, LLViewerObject::nextWorldShadowRevision, LLViewerObject::setParent | rig, stepped-animation | creative-candidate | unknown; conditional evidence present | Comment evidence: O(1): one process-unique tag (atomic counter) on this object and on its edit root, found by a parent walk (no recursion, no allocation). The tags are unique, so a recreated object with a recycled UUID never matches a stale cache entry. The revision FIELDS themselves are only writ |
| 1014–1020 (n=7) | LLViewerObject::addChild | rig | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1044–1050 (n=7) | LLViewerObject::removeChild | rig | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1348–1354 (n=7) | LLViewerObject::processUpdateMessage | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 2300–2306 (n=7) | LLViewerObject::processUpdateMessage | unknown | unknown-review | unknown; no local conditional evidence | Edits LLVector3 |
| 2339–2371 (n=33) | LLViewerObject::processUpdateMessage | tracking | creative-candidate | unknown; conditional evidence present | Comment evidence: Track only accepted packets, before ping extrapolation / target omega. Keep the packet frame, including unresolved parents, independently of recursive setRegion propagation and later orphan adoption. Terse packets have no scale: never cache a local manip preview as a server scale |
| 2577–2599 (n=23) | LLViewerObject::idleUpdate | clones, stepped-animation | creative-candidate | unknown; conditional evidence present | Comment evidence: [Temporal Capture] Objects drive: scale ONLY the local target-omega integrator so prim spin slows/speeds with the world clock. The shared dt (linear extrapolation below) stays on wall time -- simulator-driven linear/physical motion is a later pillar. Capped so a hitch at fast sca |
| 4597–4623 (n=27) | LLViewerObject::getLastServerTransform | tracking | creative-candidate | unknown; conditional evidence present | Edits LLViewerObject, LLVector3, LLQuaternion, ALProjectorTrackMath |
| 4670–4685 (n=16) | LLViewerObject::getManipPivotPositionAgent | clones | creative-candidate | unknown; conditional evidence present | Edits LLVector3, LLViewerObject, LLGhostAvatar |
| 5209–5419 (n=211) | LLViewerObject::getGhostResolvedBakedTexture, LLViewerObject::getGhostResolvedMaterialBindingCount, LLViewerObject::ghostResolvedMaterialBindingMatches, LLViewerObject::ghostResolvedMaterialSemanticMatches, LLViewerObject::hasGhostResolvedMaterialBinding, LLViewerObject::isGhostResolvedMaterialBindingPending, LLViewerObject::restoreGhostResolvedMaterialBinding, LLViewerObject::setGhostResolvedBakedTexture, LLViewerObject::setGhostResolvedMaterialBinding | clones | creative-candidate | unknown; conditional evidence present | Edits LLViewerObject, LLUUID, LLViewerTexture, LLAvatarAppearanceDefines, LLAvatarAppearanceDictionary, LLCachedControl, gSavedSettings, LLTextureEntry, LLFetchedGLTFMaterial, LLViewerFetchedTexture |
| 5421–5431 (n=11) | LLViewerObject::getBakedTextureForMagicId | clones | creative-candidate | unknown; conditional evidence present | Edits LLViewerTexture |
| 5482–5495 (n=14) | LLViewerObject::setTE | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: A TE with a registered source-resolved binding must get a clone-private render material before updateTEMaterialTextures writes fetched PBR channels. initRenderMaterial is a no-op when uniqueness is unnecessary. |
| 5505–5522 (n=18) | LLViewerObject::updateTEMaterialTextures | clones | creative-candidate | unknown; no local conditional evidence | Edits LLViewerTextureManager, LLGLTexture, LLViewerTexture |
| 5533–5543 (n=11) | LLViewerObject::updateTEMaterialTextures | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [ProbeOnDemand] H7: an async PBR material finished loading. |
| 5581–5587 (n=7) | LLViewerObject::updateTEMaterialTextures | clones | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 5951–5960 (n=10) | LLViewerObject::initRenderMaterial | clones | creative-candidate | unknown; no local conditional evidence | Comment evidence: A managed ghost binding must never write pinned channels back into a fetched base shared by source/world faces. |
| 5972–5981 (n=10) | LLViewerObject::initRenderMaterial | clones | creative-candidate | unknown; no local conditional evidence | Comment evidence: Also covers asynchronous material-completion callbacks that re-init. |
| 6210–6221 (n=12) | LLViewerObject::getBoundingBoxAgent | clones | creative-candidate | unknown; conditional evidence present | Edits LLGhostAvatar, LLVector3, LLBBox, LLQuaternion |
| 7888–7907 (n=20) | LLViewerObject::setRenderMaterialID, onMaterialComplete | clones, rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [ProbeOnDemand] H7: an async PBR material finished loading. |
| 7918–7928 (n=11) | LLViewerObject::setRenderMaterialID, onMaterialComplete | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [ProbeOnDemand] H7: an async PBR material finished loading. |
| 8168–8170 (n=3) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 3: `indra/newview/llviewerpartsim.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–32 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | clones, stepped-animation | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 45–60 (n=16) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLViewerPartSim |
| 78–107 (n=30) | bounce_off_source_plane, land_on_source_plane | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Bounce rule against the source's height plane; shared by the per-substep test and the ballistic tail so a long hold can never leave a particle below the plane. Long-tail variant: a mirror would reflect the whole fall back above the source. Land the particle on the plane instead,  |
| 277–330 (n=54) | LLViewerPartGroup::hasSteppedParticles, LLViewerPartGroup::removeParticleSource, LLViewerPartSource::particleStepDecision | stepped-animation | creative-candidate | unknown; conditional evidence present | Edits ALSteppedAnim, LLViewerPartSource, LLViewerObject, LLVOAvatar, ALSteppedAnimRuntime, LLViewerPartGroup, LLViewerPartSim |
| 344–357 (n=14) | LLViewerPartGroup::addPart, std::find_if | unknown | unknown-review | unknown; conditional evidence present | Edits LLViewerPartSource |
| 359–365 (n=7) | unknown/non-function (bool LLViewerPartGroup::addPart(LLViewerPart* part, F32 desired_size)) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLViewerPartGroup |
| 375–547 (n=173) | LLViewerPartGroup::updateParticles | stepped-animation | creative-candidate | unknown; conditional evidence present | Comment evidence: Offscreen groups normally update only every eighth frame. Consume real elapsed time every frame for stepped particles; mSkipOffset prevents those extra passes being counted again. A capture may transfer this particle to a later group in this pass. Claim the frame before changing  |
| 571–581 (n=11) | LLViewerPartGroup::updateParticles | stepped-animation | creative-candidate | unknown; conditional evidence present | Edits LLViewerPart, LL_PART_DEAD_MASK, ALSteppedAnim |
| 584–590 (n=7) | LLViewerPartGroup::updateParticles | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 671–677 (n=7) | LLViewerPartSim::enable | stepped-animation | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 699–705 (n=7) | LLViewerPartSim::destroyClass | stepped-animation | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 840–874 (n=35) | LLViewerPartSim::updateSimulation | capture-sidecar, clones, stepped-animation | creative-candidate | unknown; conditional evidence present | Comment evidence: [Temporal Capture] Particles drive: advance the ENTIRE particle time domain (emission, source age, particle birth/age/max-age, velocity, wind, target, bounce, color/scale/glow fades AND the skip accounting below) on the presentation clock, so slow-mo and fast are coherent. Scale  |
| 897–967 (n=71) | LLViewerPartSim::updateSimulation | stepped-animation | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge FilmClean] Viewer EFFECT particles are UI, not scene content: the sparkle spiral around an avatar on a script touch or an inventory offer, and the selection beam. They are spawned by LLHUDEffectSpiral (llhudeffecttrail.cpp) which registers a real LLViewerPartSourceSpiral |
| 995–1031 (n=37) | LLViewerPartSim::updateSimulation | stepped-animation | creative-candidate | unknown; conditional evidence present | Edits ALSteppedAnimRuntime, LLVOAvatar, ALSteppedAnim |
| 1063–1069 (n=7) | LLViewerPartSim::updateSimulation | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1075–1092 (n=18) | LLViewerPartSim::updateSimulation | stepped-animation | creative-candidate | unknown; conditional evidence present | Edits ALSteppedAnimRuntime |
| 1113–1165 (n=53) | LLViewerPartSim::clearStepAnimDeferredSources, LLViewerPartSim::clearStepAnimParticleDebt, LLViewerPartSim::refreshStepAnimAttachmentSourcePositions | stepped-animation | creative-candidate | unknown; conditional evidence present | Comment evidence: Particle groups ran before the held attachment matrices were applied. Re-anchor follow-source particles now so both newly emitted particles and existing followers are drawn from the held source in this render window. Lazy invalidation also covers particles retained by a dead sour |

## Slice 3: `indra/newview/llviewerpartsim.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 27–33 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | stepped-animation | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 61–67 (n=7) | unknown/non-function (public:) | stepped-animation | creative-candidate | unknown; no local conditional evidence | Edits ALSteppedAnim |
| 99–107 (n=9) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 128–139 (n=12) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Raw pointers stay alive through mParticles; counts change only on birth, death or transfer. Offscreen checks walk sources, never the particle pool. |
| 157–167 (n=11) | unknown/non-function (public:) | stepped-animation | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 207–218 (n=12) | unknown/non-function (protected:) | stepped-animation | creative-candidate | unknown; no local conditional evidence | Edits LLPointer, LLViewerPartSource |

## Slice 3: `indra/newview/llviewerpartsource.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 27–33 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | stepped-animation | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 79–91 (n=13) | unknown/non-function (public:) | stepped-animation | creative-candidate | unknown; no local conditional evidence | Edits ALSteppedAnim |

## Slice 3: `indra/newview/llviewershadermgr.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 30–48 (n=19) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 52–58 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | roto-tron | creative-candidate | unknown; no local conditional evidence | Edits gActorMaskDebugProgram |
| 76–85 (n=10) | unknown/non-function (using std::string;) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Runtime T21 readback is legal only after a successful load of the matching compile-time permutation. pipeline.cpp consumes this load-result latch. |
| 134–157 (n=24) | actorMaskTextureChannels | actor-path, clones, roto-tron | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ActorMover] pose-ghost FX (hologram / x-ray ghost styles) [ActorMask] One sampler per slot; native material auxiliary maps are unnecessary. |
| 175–185 (n=11) | unknown/non-function (LLGLSLShader            gPostScreenSpaceReflectionProgram;) | vcam | creative-candidate | unknown; no local conditional evidence | Edits LLGLSLShader, gPrismLensProgram, gPrismMediaCopyProgram, gPrismProjectorFeedProgram, gPrismProjectorLookProgram, gPrismPreviewProgram |
| 194–215 (n=22) | unknown/non-function (LLGLSLShader            gDeferredTreeShadowProgram;) | rig, roto-tron | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ActorMask] |
| 244–275 (n=32) | unknown/non-function (LLGLSLShader            gDeferredEmissiveProgram;) | looks, roto-tron, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: WP-B: main-presentation physical camera variants, never used by legacy feeds. |
| 280–325 (n=46) | unknown/non-function (LLGLSLShader            gSMAAEdgeDetectProgram[4];) | rig, roto-tron, shafts-volumetrics, weather | creative-candidate | unknown; no local conditional evidence | Comment evidence: [Ultimate Diopter] pass 1 gather (quality tap tiers) + pass 2 composite [Ultimate Kaleidoscope] tool mode 1 of the Ultimate Diopter post pass [BDMerge G3.2] volumetric lighting (donor: Black Dragon) [Cine Outline Phase 1] deferred normal/depth outline post pass [TronT1] Tron Worl |
| 338–369 (n=32) | unknown/non-function (LLGLSLShader            gDeferredSkinnedFullbrightAlphaMaskAlphaProgram;) | capture-sidecar, shafts-volumetrics | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge A5.4-1a] velocity / motion-vector pass programs (rigid + camera). The skinned/rigged and avatar variants are Phase 1b. [BDMerge Froxel F0] hybrid froxel volumetrics: P1 media pass + debug visualizer. [BDMerge Froxel F1] P4 integrate + P5 apply passes. [BDMerge Froxel F2] |
| 373–392 (n=20) | unknown/non-function (LLGLSLShader            gCGColorgradeGammaProgram;) | capture-sidecar, looks, sensor | creative-candidate | unknown; no local conditional evidence | Edits LLGLSLShader, gOnLensFiltersProgram, gCineFlareEnergyProgram, gLensSoftEdgeProgram, gColorChartProgram, gColorChartExposureProgram, gColorChartCameraProgram, gOnLensNoIR, gVelocityMaterialProgram, LLMaterial |
| 400–421 (n=22) | unknown/non-function (LLGLSLShader            gDeferredSkinnedPBROpaqueProgram;) | looks | creative-candidate | unknown; no local conditional evidence | Edits LLGLSLShader, gSharedActorFxPBRProgram, gSharedActorFxSkinnedPBRProgram, gSharedActorFxPBRSlotProgram, gSharedActorFxSkinnedPBRSlotProgram, gSharedActorFxPBRGlowProgram, gSharedActorFxSkinnedPBRGlowProgram, gSharedActorFxPBRGlowSlotProgram, gSharedActorFxSkinnedPBRGlowSlotProgram, gSharedActorFxPBRSyntheticGlowProgram |
| 435–577 (n=143) | LLViewerShaderMgr::hasPrimaryActorFxModules, LLViewerShaderMgr::hasSharedActorFxPBRShaders, add_common_permutations, alr_emissive_buffer_enabled, alr_silhouette_only_emissive, init_shared_actor_fx_pbr_replay_uniforms, shared_actor_fx_pbr_family_loaded, sl_visible_diffuse_sidecar, unload_shared_actor_fx_pbr_family | capture-sidecar, looks | creative-candidate | unknown; conditional evidence present | Comment evidence: The shared replay is an optional cinematic accelerator.  Publish it only as one complete family: partial availability could suppress a native component before its matching alpha-mode or glow command exists. Safe program defaults. The authored-glow program can add synthetic bloom  |
| 616–650 (n=35) | disable_indexed_gltf_batching | actor-path, crowd | creative-candidate | unknown; no local conditional evidence | Comment evidence: Indexed GLTF geometry is formed before rendering and cannot be safely drawn through scalar programs one material at a time. If any required indexed PBR beauty/glow/shadow program fails, disable indexed material formation before the pipeline rebuilds geometry and consistently use  |
| 689–720 (n=32) | LLViewerShaderMgr::finalizeShaderList | clones, roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: [ActorMask] mirror_flag/clipPlane dirtied here, uploaded only at bind. Actorghost world programs are optional/fail-open. Never publish an unloaded optional program to the environment-uniform list (debug builds assert that every entry is linked). |
| 724–803 (n=80) | LLViewerShaderMgr::finalizeShaderList | looks, rig, roto-tron, sensor, shafts-volumetrics, vcam, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge G3.2] atmospherics uniforms (blue_density etc.) are pushed to registered shaders only - without this the volumetric shader computes haze_density/(blue_density+haze_density) = 0/0 = NaN and blacks the frame [Ultimate Diopter] [Ultimate Kaleidoscope] [BDMerge G3.3] project |
| 810–817 (n=8) | LLViewerShaderMgr::finalizeShaderList | looks | creative-candidate | unknown; no local conditional evidence | Edits gActorFxGlowProgram, gActorFxPBRGlowProgram |
| 824–852 (n=29) | LLViewerShaderMgr::finalizeShaderList | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: Optional shared Actor FX PBR programs must never enter environment propagation partially loaded: debug builds assert every registered base and rigged variant is linked.  The renderer therefore also gets a simple all-or-nothing readiness signal from this same family invariant. |
| 871–878 (n=8) | LLViewerShaderMgr::finalizeShaderList | unknown | unknown-review | unknown; no local conditional evidence | Edits gOnLensFiltersProgram, gCineFlareEnergyProgram |
| 904–923 (n=20) | LLViewerShaderMgr::initAttribsAndUniforms, silCoverMapUniform | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [AvatarSilhouette] WP4's exhaustive file list excludes llshadermgr.*; register the independent coverage sampler here before any programs are linked so every LLGLSLShader sizes/maps its uniform vectors with it. |
| 927–954 (n=28) | LLViewerShaderMgr::gbufferAvatarTagActive, LLViewerShaderMgr::gbufferAvatarTagPreferred, LLViewerShaderMgr::gbufferAvatarTagWanted | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Avatar identity is compiled at startup whenever the normal target is RGBA16. RenderGBufferAvatarTag is a runtime source preference, never a compile key. static static |
| 963–970 (n=8) | LLViewerShaderMgr::setShaders | unknown | unknown-review | unknown; no local conditional evidence | Edits LLGLSLShader |
| 982–1063 (n=82) | LLViewerShaderMgr::setShaders, increment | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Fold a fingerprint of the on-disk shader SOURCE into the cache key so ANY shader edit invalidates the GL program-binary cache on next launch — automatically, independent of the viewer version/build number. Without this, a shader-only change (or two builds that share a version) le |
| 1080–1110 (n=31) | LLViewerShaderMgr::setShaders | looks, weather | creative-candidate | unknown; no local conditional evidence | Comment evidence: Indexed GLTF PBR batches one material per four texture units (base color, normal, ORM, emissive). The PBR opaque GBuffer-write pass binds no shadow/reflection maps, so the full fragment texture-unit budget is available here -- unlike sIndexedTextureChannels above, no units are re |
| 1119–1127 (n=9) | LLViewerShaderMgr::setShaders | looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: [AvatarLightResponse] count the programs that attach avatarLightResponseF.glsl during this load |
| 1273–1324 (n=52) | LLViewerShaderMgr::setShaders, LLViewerShaderMgr::unloadShaders | looks, sensor, vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: The cover sampler can narrow indexed material batches. Retire old draw maps before any draw uses the newly linked, smaller sampler inventory. [AvatarLightResponse] A completed auxiliary Prism pass can retain shader addresses solely to restore main-view screen uniforms on their ne |
| 1371–1384 (n=14) | LLViewerShaderMgr::loadBasicShaders | unknown | unknown-review | unknown; conditional evidence present | Edits LLCachedControl, gSavedSettings, RenderEnableEmissiveBuffer |
| 1426–1448 (n=23) | LLViewerShaderMgr::loadBasicShaders | roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: [TronA0] Per-pixel avatar tag in the G-buffer normal .w. HASH-TOKEN global define: LLShaderMgr::loadShaderFile keys the flag-decoder preamble text on it (and emits the real GBUFFER_AVATAR_TAG there), the binary-cache hash covers it (llglslshader.cpp), and the utility files' defin |
| 1474–1499 (n=26) | LLViewerShaderMgr::loadBasicShaders | environment, looks, shafts-volumetrics, weather | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1507–1519 (n=13) | LLViewerShaderMgr::loadBasicShaders | looks | creative-candidate | unknown; conditional evidence present | Edits LL_WARNS, LL_ENDL |
| 1562–1579 (n=18) | LLViewerShaderMgr::loadShadersWater | capture-sidecar | creative-candidate | unknown; conditional evidence present | Comment evidence: S3: without this the water shader declares only frag_color, so the sidecar attachment kept the seed pass's answer for the SEABED behind the water and published it as the water surface's albedo at K=1. Only the visible-diffuse permutation -- water has no emissive output, so add_co |
| 1596–1609 (n=14) | LLViewerShaderMgr::loadShadersWater | capture-sidecar | creative-candidate | unknown; conditional evidence present | Comment evidence: MUST be after clearPermutations(). The underwater program shares POOL_WATER, which opens the sidecar and coverage attachments; without the extra output declarations this shader would write UNDEFINED values to them (hazard H1) -- the precise failure this exists to prevent. |
| 1756–2014 (n=259) | LLViewerShaderMgr::loadColorChartShaders, LLViewerShaderMgr::loadSensorShaders, LLViewerShaderMgr::loadShadersDeferred, LLViewerShaderMgr::unloadColorChartShaders, LLViewerShaderMgr::unloadSensorShaders, isComplete | clones, looks, rig, roto-tron, sensor, shafts-volumetrics | creative-candidate | unknown; conditional evidence present | Comment evidence: Only the two camera stages needed by the atlas. These private programs cannot invalidate the physical sensor's program suite. Clone source recipes, not program handles or legacy permutations. Compile the pure WP-A utility as a separate production fragment unit. |
| 2024–2031 (n=8) | LLViewerShaderMgr::loadShadersDeferred | rig | creative-candidate | unknown; no local conditional evidence | Edits gAreaShadowProgram, gAreaShadowFilterProgram |
| 2033–2044 (n=12) | LLViewerShaderMgr::loadShadersDeferred | roto-tron | creative-candidate | unknown; no local conditional evidence | Edits gDeferredMultiLightActorMaskProgram, gDeferredMultiSpotLightActorMaskProgram, gVisibleDiffuseSeedProgram |
| 2055–2063 (n=9) | LLViewerShaderMgr::loadShadersDeferred | roto-tron | creative-candidate | unknown; no local conditional evidence | Edits gActorMaskProgram, gSilCoverProgram, gActorMaskDebugProgram |
| 2073–2082 (n=10) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gActorFxGlowProgram, gActorFxSkinnedGlowProgram, gActorFxPBRGlowProgram, gActorFxPBRSkinnedGlowProgram |
| 2093–2137 (n=45) | LLViewerShaderMgr::loadShadersDeferred | looks, rig, roto-tron, shafts-volumetrics, weather | creative-candidate | unknown; no local conditional evidence | Comment evidence: [Ultimate Diopter] |
| 2148–2187 (n=40) | LLViewerShaderMgr::loadShadersDeferred | capture-sidecar, shafts-volumetrics, vcam | creative-candidate | unknown; no local conditional evidence | Edits gPrismLensProgram, gPrismMediaCopyProgram, LLGLSLShader, gPrismProjectorFeedProgram, gPrismProjectorLookProgram, gPrismPreviewProgram, gVelocityProgram, gVelocityAlphaProgram, gVelocityPBRAlphaProgram, gVelocityAlphaIndexedProgram |
| 2196–2202 (n=7) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 2211–2327 (n=117) | LLViewerShaderMgr::loadShadersDeferred | looks, vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: The projector conversion uses the same display transform and colour pipeline as monitor previews, but writes a dedicated mipmapped cookie. Keep the family aligned with colorCorrectPermutation() so legacy gamma, tonemap and colour-grade choices match the visible monitor signal. Pr |
| 2337–2348 (n=12) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gDeferredDiffuseProgram |
| 2352–2364 (n=13) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gDeferredDiffuseAlphaMaskProgram |
| 2380–2392 (n=13) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gDeferredNonIndexedDiffuseAlphaMaskNoColorProgram |
| 2395–2407 (n=13) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gDeferredBumpProgram |
| 2439–2448 (n=10) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gDeferredMaterialProgram |
| 2491–2518 (n=28) | LLViewerShaderMgr::loadShadersDeferred | capture-sidecar | creative-candidate | unknown; conditional evidence present | Comment evidence: A material's velocity coverage must evaluate the same glare as beauty. Share its sources, features and all permutations. Same post-link lighting flag as the forward beauty program. |
| 2556–2566 (n=11) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits LLGLSLShader |
| 2602–2615 (n=14) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gDeferredPBROpaqueProgram |
| 2627–2641 (n=15) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gDeferredPBROpaqueIndexedProgram, LLGLSLShader |
| 2658–2664 (n=7) | LLViewerShaderMgr::loadShadersDeferred | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: The legacy material indexed programs were built earlier (above) with the now-stale channel count and share sIndexedGLTFChannels. Tear them down so the invariant sIndexedLegacyMaterials => sIndexedGLTFChannels >= 2 holds. |
| 2670–2681 (n=12) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gPBRGlowProgram |
| 2691–2708 (n=18) | LLViewerShaderMgr::loadShadersDeferred | actor-path, crowd, looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: pushGLTFBatchIndexed drives it directly. Optional: failure leaves the program incomplete and the pool falls back to scalar glow. Kept out of the 'success' chain. pushGLTFBatchIndexed drives it directly. A failure disables indexed material formation so rebuilt geometry uses the sc |
| 2719–2726 (n=8) | LLViewerShaderMgr::loadShadersDeferred | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_WARNS, LL_ENDL, gPBRGlowIndexedProgram, gPBRGlowSkinnedIndexedProgram |
| 2757–2774 (n=18) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; conditional evidence present | Edits gSavedSettings, ALR_RIMSPLIT_PARITY |
| 2803–3114 (n=312) | LLViewerShaderMgr::loadShadersDeferred | capture-sidecar, looks | creative-candidate | unknown; conditional evidence present | Comment evidence: [ActorStyle/Shared PBR Phase 1] Optional, isolated forward-PBR replay family.  It intentionally does not participate in the main 'success' chain: a failed cinematic replay program must leave native GLTF rendering available.  The family is unloaded as a unit below. Deliberately om |
| 3161–3167 (n=7) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gDeferredTreeShadowProgram |
| 3174–3180 (n=7) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gDeferredSkinnedTreeShadowProgram |
| 3215–3237 (n=23) | LLViewerShaderMgr::loadShadersDeferred | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: A failed compile decrements mShaderLevel; restore it for the retry. |
| 3253–3275 (n=23) | LLViewerShaderMgr::loadShadersDeferred | rig | creative-candidate | unknown; conditional evidence present | Edits gDeferredMultiLightProgram, gGLManager, LL_WARNS, LL_ENDL, gRigRimAuxCompiled |
| 3289–3310 (n=22) | LLViewerShaderMgr::loadShadersDeferred | rig | creative-candidate | unknown; conditional evidence present | Edits gDeferredSpotLightProgram, gGLManager, LL_WARNS, LL_ENDL, gRigRimAuxCompiled |
| 3324–3408 (n=85) | LLViewerShaderMgr::loadShadersDeferred | rig, roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: [ActorMask] Optional programs cannot change stock success/fallback. No environment uniforms: bindDeferredShader supplies globals, as stock. |
| 3499–3515 (n=17) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 3610–3625 (n=16) | LLViewerShaderMgr::loadShadersDeferred | looks, roto-tron | creative-candidate | unknown; no local conditional evidence | Comment evidence: [TronA0] Eyes link diffuseF.glsl (a world shader) without HAS_SKIN; this permutation lets diffuseF tag the pixel as avatar geometry. |
| 3635–3651 (n=17) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gDeferredFullbrightProgram |
| 3678–3690 (n=13) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gDeferredFullbrightAlphaMaskProgram |
| 3725–3738 (n=14) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gDeferredFullbrightAlphaMaskAlphaProgram |
| 3774–3785 (n=12) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gDeferredFullbrightShinyProgram |
| 3818–3829 (n=12) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gDeferredEmissiveProgram |
| 3832–3880 (n=49) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: A dedicated zero-authored-glow path uses the ordinary colour stream, so Actor FX does not force every actor VBO to carry duplicate emissive vertex data. It is only submitted for active glowing looks. |
| 3883–3896 (n=14) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gDeferredEmissiveIndexedProgram, LLGLSLShader |
| 3915–3981 (n=67) | LLViewerShaderMgr::loadShadersDeferred | capture-sidecar, rig | creative-candidate | unknown; conditional evidence present | Comment evidence: NOT folded into 'success', and deliberately not asserted. This program is OPTIONAL: it exists only when the visible-diffuse sidecar is enabled, and the whole feature is designed to degrade to feature-off. Gating 'success' on it would abort the rest of the load chain -- gDeferredS |
| 4063–4069 (n=7) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gDeferredShadowProgram |
| 4076–4082 (n=7) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gDeferredSkinnedShadowProgram |
| 4095–4101 (n=7) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gDeferredShadowCubeProgram |
| 4110–4116 (n=7) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gDeferredShadowFullbrightAlphaMaskProgram |
| 4132–4138 (n=7) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gDeferredShadowAlphaMaskProgram |
| 4148–4154 (n=7) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gDeferredShadowGLTFAlphaMaskProgram |
| 4165–4175 (n=11) | LLViewerShaderMgr::loadShadersDeferred | actor-path, crowd, looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: per-slot in the shadow map. Optional: if it fails to load the shadow pass falls back to the scalar program (slightly wrong per-face cutouts, no crash), so this is kept out of the 'success' chain. per-slot in the shadow map. A failure disables indexed material formation; the subse |
| 4192–4199 (n=8) | LLViewerShaderMgr::loadShadersDeferred | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_WARNS, LL_ENDL, gDeferredShadowGLTFAlphaMaskIndexedProgram, gDeferredSkinnedShadowGLTFAlphaMaskIndexedProgram |
| 4205–4212 (n=8) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gDeferredShadowMaterialIndexedProgram, LLGLSLShader |
| 4238–4244 (n=7) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gDeferredShadowGLTFAlphaBlendProgram |
| 4256–4262 (n=7) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gDeferredAvatarShadowProgram |
| 4270–4276 (n=7) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gDeferredAvatarAlphaShadowProgram |
| 4282–4288 (n=7) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gDeferredAvatarAlphaMaskShadowProgram |
| 4315–4329 (n=15) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gDeferredAvatarProgram |
| 4348–4361 (n=14) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gDeferredAvatarAlphaProgram |
| 4590–5209 (n=620) | LLViewerShaderMgr::loadShadersDeferred | clones, rig, roto-tron, sensor, shafts-volumetrics, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: Rig Rim Quality programs are optional. A failure disables only the affected quality stage; the stock lighting chain remains available. [TronT1] Tron World post pass. Same shape as the Cine Outline program: isDeferred attaches deferredUtil.glsl (getDepth/getNorm/getNormRaw and the |
| 5480–5778 (n=299) | LLViewerShaderMgr::loadShadersDeferred | capture-sidecar, looks, shafts-volumetrics | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge A5.4-1a/1b] Velocity / motion-vector pass programs. Phase 1b adds the make_rigged_variant skinned pair (HAS_SKIN -> objectSkinV.glsl current + last palettes) and the classic-avatar velocity program. These programs are cheap to keep resident and only run when BDMergeVeloc |
| 6035–6114 (n=80) | LLViewerShaderMgr::loadShadersDeferred | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: Optional history pass; failure leaves the established unsmoothed path. On-lens filters (Graduated ND + Polarizer) pre-pass. Runs on the linear HDR scene before bloom/flare. hasPostEffects attaches postEffectUtilsF, which defines applyGradND/applyPolarizer and the gradnd_*/polariz |
| 6121–6235 (n=115) | LLViewerShaderMgr::loadShadersDeferred | looks, rig, roto-tron, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: [ActorMask] Optional programs never affect stock shader success/fallback. All permutations follow clearPermutations (CLAUDE.md). [AvatarSilhouette] Coverage replay programs deliberately share the Actor Mask vertex contract, but write continuous RG coverage. [ActorMask] WP7 debug  |
| 6269–6280 (n=12) | LLViewerShaderMgr::loadShadersObject | looks | creative-candidate | unknown; no local conditional evidence | Edits gObjectAlphaMaskNoColorProgram |
| 6337–6344 (n=8) | LLViewerShaderMgr::loadShadersAvatar | clones | creative-candidate | unknown; no local conditional evidence | Edits gAvatarActorGhostProgram, gAvatarEyeballActorGhostProgram |
| 6352–6363 (n=12) | LLViewerShaderMgr::loadShadersAvatar | looks | creative-candidate | unknown; no local conditional evidence | Edits gAvatarProgram |
| 6377–6446 (n=70) | LLViewerShaderMgr::loadShadersAvatar | clones, looks | creative-candidate | unknown; conditional evidence present | Comment evidence: [ActorStyle] Dedicated WORLD/HDR actorghostF programs for the classic system avatar.  They deliberately remain separate from the interface Ghost Studio pair: classic body vertices use the avatar palette (not objectSkinV), rigid eyes use their joint model matrix, and both need wor |
| 6467–6584 (n=118) | LLViewerShaderMgr::loadShadersInterface | actor-path, clones, looks | creative-candidate | unknown; conditional evidence present | Comment evidence: [ActorMover] pose-ghost FX shader (ALL model-ghost styles): the highlight pair's skinned constant-colour transform plus animated screen-space scanlines, a fresnel-ish rim boost, a subtle time flicker (ghostTime / ghostParams) and the per-batch alpha stage (ghostAux: mask-cutoff d |
| 6955–6984 (n=30) | LLViewerShaderMgr::updateWeatherUniforms, LLViewerShaderMgr::weatherMaterialsEnabled, LLViewerShaderMgr::weatherMaterialsSupported | weather | creative-candidate | unknown; conditional evidence present | Comment evidence: Four maps per terrain layer plus paint/alpha and cover; forward paths need up to 16. |
| 6993–6995 (n=3) | unknown/non-function (LLViewerShaderMgr::shader_iter LLViewerShaderMgr::endShaders() const) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 3: `indra/newview/llviewershadermgr.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 35–45 (n=11) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Cine-rig lens flare: max rig projector sources uploaded per frame to uCineFlareA/uCineFlareColor (postEffectUtilsF.glsl AL_CINE_FLARE_MAX must match — LIGHT_COUNT(4) rig lights per rig, cap sized for ~2 subjects). |
| 62–100 (n=39) | colorChartShaderGeneration | looks, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: HDR startup availability, loaded availability, and the runtime source preference. The preference never controls shader compilation. Optional, lazy chart programs. Failed attempts retry only after unload (chart re-enable or a global shader reload), never on each frame. True only w |
| 160–176 (n=17) | unknown/non-function (public:) | looks, weather | creative-candidate | unknown; no local conditional evidence | Edits LLGLSLShader |
| 187–199 (n=13) | unknown/non-function (inline bool operator != (LLViewerShaderMgr::shader_iter const & a, LLViewerShade) | roto-tron | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ActorMask] Independent programs; no stock object is mutated. |
| 247–269 (n=23) | unknown/non-function (extern LLGLSLShader         gBloomCompositeProgram;) | actor-path, clones, looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ActorMover] pose-ghost FX (hologram / x-ray styles); the rigged variant is what drawGeometryGhost binds. Optional: draw code falls back to the classic ghost when this failed to compile. Main-world/HDR variants used only by shared live Actor FX. Ghost Studio keeps gActorGhostProg |
| 280–293 (n=14) | unknown/non-function (extern LLGLSLShader         gPostScreenSpaceReflectionProgram;) | looks, vcam | creative-candidate | unknown; no local conditional evidence | Comment evidence: Same eight legacy/color-grade/tonemap permutations selected by colorCorrect. Preview variants intentionally omit HAS_POST_EFFECTS/BLOOM_COMPOSITE. |
| 297–311 (n=15) | unknown/non-function (extern LLGLSLShader         gDeferredBumpProgram;) | rig, roto-tron | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ActorMask] Fullscreen-only wrappers of the unchanged stock light bodies. |
| 313–319 (n=7) | unknown/non-function (extern LLGLSLShader         gHazeWaterProgram;) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLGLSLShader, gVisibleDiffuseSeedProgram |
| 324–349 (n=26) | unknown/non-function (extern LLGLSLShader         gDeferredShadowGLTFAlphaBlendProgram;) | roto-tron, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: WP-B: main-presentation physical camera variants, never used by legacy feeds. |
| 351–407 (n=57) | unknown/non-function (extern LLGLSLShader         gSMAAEdgeDetectProgram[4];) | looks, rig, roto-tron, sensor, shafts-volumetrics, weather | creative-candidate | unknown; no local conditional evidence | Comment evidence: [Ultimate Diopter] pass 1 gather at four tap-count quality tiers + pass 2 composite [Ultimate Kaleidoscope] tool mode 1 of the Ultimate Diopter post pass: single MRT pass, no permutations [BDMerge G3.2] volumetric lighting (donor: Black Dragon) [Cine Outline Phase 1] deferred nor |
| 417–424 (n=8) | unknown/non-function (extern LLGLSLShader         gDeferredFullbrightAlphaMaskAlphaProgram;) | looks | creative-candidate | unknown; no local conditional evidence | Edits LLGLSLShader, gActorFxGlowProgram, gActorFxPBRGlowProgram |
| 434–467 (n=34) | unknown/non-function (extern LLGLSLShader         gHUDFullbrightShinyProgram;) | capture-sidecar, shafts-volumetrics | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge A5.4-1a] velocity / motion-vector pass programs (rigid + camera). Skinned/rigged + avatar variants are Phase 1b. [BDMerge A5.4-1c] fullscreen camera-motion fallback (fills avatar/sky/uncovered pixels with camera-induced motion before the geometry velocity stamps). [BDMer |
| 477–483 (n=7) | unknown/non-function (extern LLGLSLShader         gRlvSphereProgram;) | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Edits LLGLSLShader, gVelocityMaterialProgram, LLMaterial |
| 488–528 (n=41) | unknown/non-function (extern LLGLSLShader         gDeferredPBROpaqueIndexedProgram; // multi-material) | looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: Optional, fail-open forward PBR family for the shared live Actor FX replay. The alpha-mode index is a replay API contract rather than LLMaterial's enum ordering (LLMaterial orders BLEND before MASK). Slot-named variants advertise TYPE_TEXTURE_INDEX and evaluate the complete index |

## Slice 3: `indra/newview/skins/default/xui/en/control_table_contents_movement.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 205–218 (n=14) | unknown/non-function (declaration, XML, build list or global scope) | composition | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 3: `indra/newview/skins/default/xui/en/floater_edit_hover_height.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–14 (n=14) | unknown/non-function (declaration, XML, build list or global scope) | composition | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ClientHeight] Set Hover Height also hosts the local (client-side) avatar height: a render-only |
| 17–143 (n=127) | unknown/non-function (declaration, XML, build list or global scope) | clones, composition | creative-candidate | unknown; no local conditional evidence | Comment evidence: Local offset: 5 mm increment for drag and text entry. The mouse wheel follows the GLOBAL |

## Slice 3: `indra/newview/skins/default/xui/en/floater_joystick.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–787 (n=787) | unknown/non-function (declaration, XML, build list or global scope) | camera, roto-tron | creative-candidate | unknown; no local conditional evidence | Comment evidence: xui-check: allow-overlap joystick_general_tab joystick_buttons_tab --> xui-check: allow-overlap joystick_general_tab joystick_xbox_tab --> xui-check: allow-overlap joystick_general_tab joystick_drone_tab --> xui-check: allow-overlap joystick_buttons_tab joystick_xbox_tab --> xui- |

## Slice 3: `indra/newview/skins/default/xui/en/panel_preferences_move_mlook.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 222–297 (n=76) | unknown/non-function (declaration, XML, build list or global scope) | camera | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge C3] Mouselook camera offset. Donor: novel, no BD source (see |

## Slice 4: `indra/llcharacter/lleditingmotion.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 28–34 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | clones | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 165–174 (n=10) | LLEditingMotion::onUpdate | clones | creative-candidate | unknown; conditional evidence present | Edits LLFrameTimer, ALAvatarAnimationTime |
| 241–251 (n=11) | LLEditingMotion::onUpdate | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: The first endpoint is solved IK, the second is the old pose. |

## Slice 4: `indra/llrender/llrender.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 38–44 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 641–655 (n=15) | LLLightState::setLocalLightSize | rig | creative-candidate | unknown; conditional evidence present | Edits LLLightState, gGL |
| 659–683 (n=25) | LLLightState::setLightShape, LLLightState::setRigRim | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [RigRim] |
| 903–939 (n=37) | LLRender::setLightShapeActive, LLRender::setLocalLightSpecExponent, LLRender::setRigRimActive | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [RigRim] Keep the fast-bind and light-state caches coherent with the rim globals that are not otherwise part of LLRender's light records. Enabling must repopulate even a program bound/cached while disabled. Disabling neither clears per-light shape state nor changes its hash. |
| 954–970 (n=17) | LLRender::syncLightState | rig | creative-candidate | unknown; conditional evidence present | Edits LLVector4, LL_NUM_LIGHT_UNITS |
| 972–1008 (n=37) | LLRender::syncLightState | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [RigRim] Round-2 P2 fix: skip even reading mRigRim while off; the upload below is skipped too, so a stale zero here is fine. [RigRim] P2-5: this uploads the forward-lit vec4[8] interpretation of 'rig_rim_lights' (parallel to light_position[8], sourced from the GL light-unit state |
| 1310–1324 (n=15) | LLRender::getMatrix | clones | creative-candidate | unknown; no local conditional evidence | Comment evidence: [GhostDeferred] read any cached matrix (incl. texture matrices) for state diagnostics -- core profile does not expose GL_TEXTURE_MATRIX, so the CPU cache is the source of truth. |
| 1425–1597 (n=173) | LLRender::clearIndexedDrawBufferGuard, LLRender::clearIndexedDrawBufferGuardBlend, LLRender::findIndexedGuardState, LLRender::indexedDrawBufferGuardSupported, LLRender::reassertIndexedDrawBuffer, LLRender::setColorMask, LLRender::setIndexedDrawBufferGuard, LLRender::setIndexedDrawBufferGuardBlend, LLRender::setIndexedDrawBufferGuardMask | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: that call just reset the mask on EVERY draw buffer static Callers must gate on indexedDrawBufferGuardSupported() before they create the attachments at all; arming here would silently promise containment we cannot deliver. Single-owner by design: there is no guard stack, and the R |
| 1639–1647 (n=9) | LLRender::blendFunc | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: global blend -> every draw buffer, including any guarded one |
| 1664–1672 (n=9) | LLRender::blendFunc | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: global blend -> every draw buffer, including any guarded one |
| 1693–1733 (n=41) | LLRender::getLightStateSnapshot, LLRender::restoreLightStateSnapshot, LLRender::restoreLightUnitContent | rig, roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: [ActorMask] Copy all content including view-space position/direction and attenuation/Rig Rim, without applying the current modelview. |

## Slice 4: `indra/llrender/llrender.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 224–230 (n=7) | hasMipMaps | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 262–270 (n=9) | unknown/non-function (public:) | rig | creative-candidate | unknown; no local conditional evidence | Edits LLVector4 |
| 287–295 (n=9) | unknown/non-function (protected:) | rig | creative-candidate | unknown; no local conditional evidence | Edits LLVector4 |
| 411–427 (n=17) | unknown/non-function (public:) | clones, rig | creative-candidate | unknown; no local conditional evidence | Comment evidence: [GhostDeferred] read any cached matrix by mode (for render-state diagnostics) [RigRim] Cheap per-frame gate plus the settings/validity signature used by the fast-bind invalidation. Returns true when either changes and also bumps the light-state hash so every cached program refres |
| 479–549 (n=71) | getBlendFunc, indexedDrawBufferGuardActive | capture-sidecar, rig, roto-tron | creative-candidate | unknown; no local conditional evidence | Comment evidence: ---- indexed draw-buffer guard -------------------------------------- A GLOBAL glColorMask/glBlendFunc resets the per-attachment (indexed) state of EVERY draw buffer. A feature that needs an auxiliary colour attachment to hold its own mask/blend across a multi-pool pass cannot de |
| 588–599 (n=12) | unknown/non-function (private:) | rig | creative-candidate | unknown; no local conditional evidence | Edits LLVector4 |
| 616–637 (n=22) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: see setIndexedDrawBufferGuard() Returns nullptr when the guard is inactive or 'index' is not guarded. |
| 655–709 (n=55) | LLScopedIndexedDrawBufferGuard, active, ~LLScopedIndexedDrawBufferGuard | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: RAII wrapper for LLRender's indexed draw-buffer guard. Arms the guard CLOSED (attachment write-disabled) for the lifetime of the scope and guarantees it is disarmed on every exit path, so an early return or a throw cannot leave a stray indexed override on the context for the next |

## Slice 4: `indra/llrender/llrendertarget.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 137–164 (n=28) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Opt-in sequence for depth-only sibling passes. Drains each pass without returning to the parent between targets; finish()/destruction restores the original parent once. Disabled sequences use stock bindTarget/flush. The caller owns gGLViewport, including its stock last-shadow exi |
| 173–193 (n=21) | getFBO, getInternalFormat | clones | creative-candidate | unknown; no local conditional evidence | Comment evidence: [GhostDeferred] Raw framebuffer object name. Diagnostics/readback only; callers must preserve and restore framebuffer bindings. GL internal format of a color attachment (0 if no such attachment) |
| 205–239 (n=35) | unknown/non-function (public:) | capture-sidecar, roto-tron | creative-candidate | unknown; no local conditional evidence | Comment evidence: Like copyContents(), but reads from a specific color attachment of source rather than whatever GL_READ_BUFFER source's FBO currently has set (copyContents() implicitly reads attachment 0, since that is the read buffer bindTarget() leaves in place). Color-only (mask must not be GL |

## Slice 4: `indra/llrender/llshadermgr.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 34–40 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 47–53 (n=7) | unknown/non-function (using std::make_pair;) | looks | creative-candidate | unknown; no local conditional evidence | Edits LLShaderMgr |
| 69–163 (n=95) | LLShaderMgr::clearShaderObjects, LLShaderMgr::computeShadersSupported, LLShaderMgr::ownsShaderObject, LLShaderMgr::prepareWeatherFeatures, LLShaderMgr::shaderLanguageVersion | looks, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: Compile the supported inventory once. Weather settings only change uniforms. Native material inventory. Fullbright, water, HUD beauty and avatar helpers are excluded. diffuseAlphaMaskNoColorF is rigid avatar eyes and has no skinning tag. |
| 283–339 (n=57) | LLShaderMgr::attachShaderFeatures | looks, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: Beauty and dissolve/shadow coverage form one Actor FX contract. If either primary module failed to load, use identity fallbacks for both so the color and shadow passes cannot disagree about coverage. [AvatarLightResponse] per-avatar response trim; identity while alrEnabled == 0. |
| 358–367 (n=10) | LLShaderMgr::attachShaderFeatures | shafts-volumetrics | creative-candidate | unknown; conditional evidence present | Comment evidence: Classic projector programs supply their frozen utility in mShaderFiles. Use a permutation marker so unrelated programs retain their feature hash. |
| 385–393 (n=9) | LLShaderMgr::attachShaderFeatures | shafts-volumetrics | creative-candidate | unknown; conditional evidence present | Comment evidence: Classic supplies its pre-Shadow-Sculpt sampler in mShaderFiles too: its rim passes a receiver normal and must never enter the detail path. |
| 429–443 (n=15) | LLShaderMgr::attachShaderFeatures | environment, shafts-volumetrics | creative-candidate | unknown; conditional evidence present | Comment evidence: [Cine Haze] shared transmittance / atmosphere-colour library.  Pure functions + uniforms (all uniform-gated), attached to exactly the set of programs that can call it: the deferred haze pass and every forward shader that links environment/waterFogF.glsl (hasAtmospherics). |
| 454–465 (n=12) | LLShaderMgr::attachShaderFeatures | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: AMD FidelityFX LPM (tonemap type 7) lives in its own object so that RunLPMFilter() (called from tonemapUtilF.glsl) resolves at link time. |
| 634–649 (n=16) | LLShaderMgr::loadShaderFile | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: A failed replacement must not leave a filename alias to an older permutation. Successfully cached objects republish their alias below. |
| 743–753 (n=11) | LLShaderMgr::loadShaderFile | unknown | unknown-review | unknown; conditional evidence present | Edits LLGLSLShader |
| 804–813 (n=10) | LLShaderMgr::loadShaderFile | unknown | unknown-review | unknown; conditional evidence present | Edits LLGLSLShader |
| 819–854 (n=36) | LLShaderMgr::loadShaderFile | roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: [TronA0] Per-pixel avatar tag in the flag channel. The preamble TEXT is chosen here from the HASH-TOKEN global define GBUFFER_AVATAR_TAG_ENABLED (set by LLViewerShaderMgr::setShaders under exactly the RGBA16-normals predicate, and part of the binary-cache key) so that every compi |
| 1064–1089 (n=26) | LLShaderMgr::loadShaderFile | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Include the assembled bytes: version, stage, permutations, indexed channels, source edits and the resolved class all distinguish objects. |
| 1128–1134 (n=7) | LLShaderMgr::loadShaderFile | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1162–1168 (n=7) | LLShaderMgr::loadShaderFile | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1346–1361 (n=16) | LLShaderMgr::loadCachedProgramBinary | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: Fallback Actor FX programs share the primary program feature/hash key. Ignore a cached primary binary while either primary module is missing, otherwise cache loading can defeat the coupled degradation above. |
| 1403–1417 (n=15) | LLShaderMgr::saveCachedProgramBinary | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: Do not cache a fallback program under the primary key. A later run with working modules must relink the real Actor FX implementation. |
| 1468–1480 (n=13) | LLShaderMgr::initAttribsAndUniforms | weather | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1502–1510 (n=9) | LLShaderMgr::initAttribsAndUniforms | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1553–1560 (n=8) | LLShaderMgr::initAttribsAndUniforms | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1567–1574 (n=8) | LLShaderMgr::initAttribsAndUniforms | sensor | creative-candidate | unknown; no local conditional evidence | Edits LLShaderMgr |
| 1686–1701 (n=16) | LLShaderMgr::initAttribsAndUniforms | unknown | unknown-review | unknown; no local conditional evidence | Edits LLShaderMgr |
| 1832–1843 (n=12) | LLShaderMgr::initAttribsAndUniforms | looks, sensor | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1878–1937 (n=60) | LLShaderMgr::initAttribsAndUniforms | clones, looks, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: Cine rig flare sources (Group 1 — rig source arrays) Lens flare polished-stack additions (Group 2 — element intensities) Lens flare polished-stack shape params (Group 3) Lens flare master (Group 4) Graduated ND (pre-tonemap exposure region) Polarizer (pre-tonemap on-lens filter;  |
| 2001–2363 (n=363) | LLShaderMgr::initAttribsAndUniforms | capture-sidecar, clones, rig, roto-tron, sensor, shafts-volumetrics, weather | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge G3.2] volumetric lighting (donor: Black Dragon) [BDMerge G3.3] per-projector volumetric light cones [BDMerge G3.3 Phase 1] projector-volumetric cinema levers [BDMerge G3.3 Phase 3] atmosphere levers [BDMerge G3.3 Batch 1] temporal reprojection (A) + gobo-colored shadows  |
| 2371–2373 (n=3) | unknown/non-function (void LLShaderMgr::initAttribsAndUniforms()) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 4: `indra/llrender/llshadermgr.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 39–47 (n=9) | unknown/non-function (public:) | looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: [AvatarLightResponse] programs that attached alchemy/avatarLightResponseF.glsl since the last reset. |
| 51–63 (n=13) | unknown/non-function (public:) | weather | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 82–90 (n=9) | unknown/non-function (public:) | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 125–132 (n=8) | unknown/non-function (public:) | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 139–146 (n=8) | unknown/non-function (public:) | sensor | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 245–258 (n=14) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 391–402 (n=12) | unknown/non-function (public:) | looks, sensor | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 437–525 (n=89) | unknown/non-function (public:) | clones, looks, sensor, shafts-volumetrics | creative-candidate | unknown; no local conditional evidence | Comment evidence: Cine rig flare sources (Group 1 — rig source arrays, uploaded per frame like MULTI_LIGHT). AL_CINE_FLARE_MAX cap defined alongside LL_DEFERRED_MULTI_LIGHT_COUNT in llviewershadermgr.h. Lens flare polished-stack additions (Group 2 — element intensities, default 0 = feature-off byt |
| 589–984 (n=396) | unknown/non-function (public:) | capture-sidecar, clones, environment, rig, roto-tron, sensor, shafts-volumetrics, weather | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge G3.2] volumetric lighting (donor: Black Dragon) [BDMerge G3.3] per-projector volumetric light cones [BDMerge G3.3 Phase 1] projector-volumetric cinema levers [BDMerge G3.3 Phase 3] atmosphere levers (all default to a no-op) [BDMerge G3.3 Batch 1] temporal reprojection (A |
| 994–1014 (n=21) | updateWeatherUniforms, weatherMaterialsEnabled, weatherMaterialsSupported | weather | creative-candidate | unknown; no local conditional evidence | Comment evidence: Shader teardown accompanies GL-context teardown/reload. |
| 1022–1030 (n=9) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Owns compiled objects. The filename maps above are aliases for preloaded graphics feature modules, not a permutation cache. |

## Slice 4: `indra/llwindow/llwindowwin32.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 205–247 (n=43) | getBorderlessPrimaryRect | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge Borderless] Fullscreen is a borderless windowed cover of the target monitor at its CURRENT desktop resolution, NOT an exclusive display-mode change. This means: no ChangeDisplaySettings mode switch, no forced resolution drop, no alt-tab minimize/mode-restore churn, and i |
| 718–738 (n=21) | LLWindowWin32::LLWindowWin32 | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge QoL] upstream sanitizes only position, not size: a saved oversized rect (e.g. from a crash-exit) creates a window larger than every attached display, leaving UI unreachable off screen. Clamp to the virtual desktop (union of all monitors) before sanitizing position. |
| 805–815 (n=11) | LLWindowWin32::LLWindowWin32 | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge Borderless] Skipped entirely for borderless fullscreen - there is no exclusive display-mode change; switchContext() sizes the WS_POPUP window to the target monitor's current desktop resolution instead. |
| 1144–1157 (n=14) | LLWindowWin32::maximize | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge Borderless] A borderless-fullscreen window already covers the primary monitor; re-maximizing it (SW_MAXIMIZE) resizes the WS_POPUP - triggering a GL realloc black-flash and re-covering the taskbar. No-op in that mode. |
| 1295–1344 (n=50) | LLWindowWin32::switchContext | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge Borderless] Borderless fullscreen windowed on the PRIMARY monitor: a plain WS_POPUP window, NOT topmost, with NO display-mode change - so it behaves like an ordinary window (click off = just loses focus).  CRITICAL anti-"exclusive" measure: Windows "Fullscreen Optimizati |
| 1624–1655 (n=32) | LLWindowWin32::switchContext | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge 10bit] Ask for the STANDARD R10G10B10A2 (2 alpha bits), not a bare R10G10B10. Without an alpha constraint the driver was handing back an R10G10B10A0 format (0 alpha), which ReShade (and anything mapping WGL->DXGI) does not recognize as 10-bit and so falls back to R8G8B8A |
| 1734–1757 (n=24) | LLWindowWin32::switchContext | capture-sidecar | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge 10bit] Report the ACTUAL per-channel bit depth of the format we selected, queried via the ARB path. DescribePixelFormat/pfd.cColorBits (logged later) can't distinguish R10G10B10A2 from R8G8B8A8 - it reports both as "Color Bits 32". These are the real numbers: R10 G10 B10 |
| 1845–1860 (n=16) | LLWindowWin32::switchContext | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge 10bit] Per-channel PFD widths as reported by DescribePixelFormat - this is the exact data ReShade's convert_pixel_format() keys off. ReShade detects 10-bit only when cColorBits==30; NVIDIA reports cColorBits=32 for R10G10B10A2, so ReShade must instead check cRedBits==10  |
| 2114–2339 (n=226) | LLWindowWin32::getFrameRefresh, LLWindowWin32::isFrameVSyncActive, LLWindowWin32::isFrameWindowMeasurable, LLWindowWin32::setFrameSwapInterval, LLWindowWin32::toggleVSync, queryMonitorRefresh | clones | mixed/perf-review | unknown; conditional evidence present | Comment evidence: [FrameLimiter] Refresh rate of one monitor. First choice: the active display path whose source matches the monitor's GDI device, whose target refresh rate is an exact fraction (60000/1001). Fallback: the integer EnumDisplaySettings reports (inexact: 59.94 Hz reads as 59 or 60). C |
| 2569–2626 (n=58) | LLWindowWin32::refreshUIHiddenCursor, LLWindowWin32::setHideCursorWithUI, post | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Never use ShowCursor here: tools own its counter and permanent-hide state. Captured moves do not send WM_SETCURSOR. Restore the OS nonclient shape ourselves when leaving the client with a button held down. |
| 2631–2647 (n=17) | LLWindowWin32::updateCursor, post | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 2742–2748 (n=7) | LLWindowWin32::gatherInput | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_NAMED_CATEGORY_WIN32 |
| 2842–2855 (n=14) | LLWindowWin32::mainWindowProc | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 2869–2887 (n=19) | LLWindowWin32::mainWindowProc, post | unknown | mixed/perf-review | unknown; conditional evidence present | Comment evidence: [BDMerge Borderless] Only EXCLUSIVE fullscreen needs the minimize-on-focus-loss / restore-mode-on-focus-gain dance (its display-mode change must be released so other apps and the desktop are usable). Borderless fullscreen is an ordinary window - it must stay put on alt-tab like a |
| 2896–2905 (n=10) | LLWindowWin32::mainWindowProc, post | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge Borderless] Borderless fullscreen is an ordinary window: NO activate-time handling at all - no minimize, no display-mode restore, no topmost toggle. Clicking off just loses focus like any window (that focus-time churn was the "acts exclusive" behavior). |
| 2913–2919 (n=7) | LLWindowWin32::mainWindowProc | unknown | mixed/perf-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 3432–3438 (n=7) | LLWindowWin32::mainWindowProc | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 3463–3475 (n=13) | LLWindowWin32::mainWindowProc | unknown | mixed/perf-review | unknown; conditional evidence present | Comment evidence: [FrameLimiter] Only a minimise, or a restore / maximise out of the minimised state, is a visibility change that re-arms driver pacing. Not every WM_SIZE: an interactive resize sends one per mouse move and would keep wiping the safety net's measurement. |
| 3535–3542 (n=8) | LLWindowWin32::mainWindowProc | unknown | mixed/perf-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 3544–3551 (n=8) | LLWindowWin32::mainWindowProc | unknown | mixed/perf-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 3553–3560 (n=8) | LLWindowWin32::mainWindowProc | unknown | mixed/perf-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 4174–4185 (n=12) | LLWindowWin32::setFullscreenResolution | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge Borderless] No exclusive display mode was ever set, so there is nothing to (re)apply - the desktop resolution is already correct. |
| 4193–4207 (n=15) | LLWindowWin32::resetDisplayResolution | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge Borderless] Borderless fullscreen never changes the display mode, so there is nothing to reset. Skip the ChangeDisplaySettings call entirely - calling it on focus loss / context rebuild is what produced the "changing resolutions" pause and desktop flash even though no ex |

## Slice 4: `indra/newview/alderenderlist.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 13–35 (n=23) | unknown/non-function (declaration, XML, build list or global scope) | clones | creative-candidate | unknown; no local conditional evidence | Edits ALDerenderList |
| 156–227 (n=72) | ALDerenderList::ALDerenderList, ALDerenderList::isHiddenObject, ALDerenderList::keepsAvatarLive, ALDerenderList::rebuildHiddenSource, ALDerenderList::~ALDerenderList, LLViewerParcelMgr::getInstance, getParent | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: Walk only this object's ancestry, never the clone list. Seated avatars stop at the wearer; their seat and its other occupants stay visible. Reuse the FORCE_INVISIBLE volume rebuild gate, with a separate policy predicate so simulator updates cannot clear it and Restore cannot clea |
| 300–308 (n=9) | ALDerenderList::removeObjects | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: One set of local IDs per region (TYPE_OBJECT and TYPE_AVATAR): every entry removed in this call goes out as one batch per region, with no duplicate IDs. |
| 316–360 (n=45) | ALDerenderList::removeObjects | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: An entry loaded from disk and not seen yet this session has no region: it is still removed below, there is just nothing to re-request. Same as objects: ask the simulator to send the avatar and all of its attachment prims again so they return without waiting for a relog. idRegion  |
| 363–369 (n=7) | ALDerenderList::removeObjects | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: TYPE_OBJECT TYPE_OBJECT and TYPE_AVATAR |
| 377–396 (n=20) | ALDerenderList::purgeSessionEntries, remove_if | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Session entries are never written to the file, so this does not save. |
| 411–426 (n=16) | ALDerenderList::addSelection | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: The generic selection action can also receive an avatar. Funnel live clone sources through the same TYPE_AVATAR policy as radar/context UI. |
| 454–549 (n=96) | ALDerenderList::addAvatar, ALDerenderList::processAvatarUpdate | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: Remember where the avatar and ALL of its descendant prims are (attachment roots and the child prims of multi-prim attachments) before they are killed, so removing the entry can ask the simulator to send them again. Drop hidden attachments too, so selection outlines cannot reveal  |

## Slice 4: `indra/newview/alderenderlist.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 13–35 (n=23) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits ALDerenderAvatar, ALDerenderList |
| 38–44 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLVOAvatar |
| 137–165 (n=29) | ALDerenderAvatar | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Added for the radar port: the only constructor that previously existed besides fromLLSD() left m_idEntry/m_strEntryName unset, so nothing could ever actually add a live avatar to the list. Where the avatar (and its attachments) lived when it was derendered, so that removing the e |
| 183–215 (n=33) | isDerendered, isHiddenSource | clones | creative-candidate | unknown; no local conditional evidence | Comment evidence: Generic lookup across entry types (radar port: TYPE_AVATAR has no dedicated accessor like getObjectEntry()). Drop every session (non-persistent) entry without touching the saved file. Called when the session ends so a relog starts clean. Add a live avatar to the derender list. Li |
| 238–246 (n=9) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLUUID |

## Slice 4: `indra/newview/alfloatersceneexplorer.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 552–558 (n=7) | ALFloaterSceneExplorer::draw | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |
| 1250–1268 (n=19) | ALFloaterSceneExplorer::reconcile | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: Control avatars (animesh) and UI/preview avatars have no associated user and aren't real scene avatars. The animesh object itself still appears via the normal object / attachment path below. Control avatars (animesh), UI/preview avatars, and client-only ghost clones have no assoc |
| 1500–1506 (n=7) | ALFloaterSceneExplorer::drainPropsQueue | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 4: `indra/newview/app_settings/shaders/class1/alchemy/postEffectUtilsF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 4–14 (n=11) | unknown/non-function (declaration, XML, build list or global scope) | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 19–28 (n=10) | unknown/non-function (declaration, XML, build list or global scope) | looks, sensor | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 200–225 (n=26) | unknown/non-function (vec4 applyChromaticAberration(sampler2D tex, vec2 uv)) | clones | creative-candidate | unknown; no local conditional evidence | Comment evidence: Lens flare — anamorphic streak with optional glow, ghosts, halo, starburst Lens flare — multi-source cinematic stack (sun + cine-rig projectors) Screen-space approximation of an anamorphic lens flare. Driven by a CPU-computed sun UV position and visibility (on-screen fade), plus  |
| 261–685 (n=425) | applyGradND, applyNightMask, applyPolarizer, flareForSource, flareNgonDist, flareSpectrum, nightMaskBoxSDF, nightMaskGetPosition | capture-sidecar, clones, looks, sensor, shafts-volumetrics | creative-candidate | unknown; conditional evidence present | Comment evidence: ---- Cine rig flare sources (uploaded by pipeline.cpp each frame) ---------- Screen-space projector sources from the cinematic light rig. When uCineFlareCount is 0 (the default) the rig loop is skipped entirely and computeLensFlare's output is bit-identical to the sun-only implem |
| 690–696 (n=7) | flareForSource | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 727–733 (n=7) | flareForSource | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 742–916 (n=175) | flareForSource | clones, roto-tron, shafts-volumetrics | creative-candidate | unknown; conditional evidence present | Comment evidence: Sample sun brightness once, convert to a normalized "overbright" factor. We subtract 2.0 from luminance so only HDR-bright suns drive the flare — prevents diffuse bright surfaces from flaring. Source energy.  Sun (occTaps >= 0): sample sun brightness once, convert to a normalized |
| 949–967 (n=19) | flareForSource | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: ---- Streak two-tone tip (new, gated) ----------------------------- Linear blend of the streak tint toward a tip color with distance from the core: adding streak·t·amt·(tipTint − tint) on top of the base term is exactly streak·mix(tint, tipTint, t·amt). Uses the base (green-chann |
| 975–1003 (n=29) | flareForSource | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: ---- Ghost chromatic fringing (new, gated) -------------------- R and B discs shifted along the ghost axis; G keeps the base disc, so the fringe reads as dispersion, not a triple image. |
| 1005–1031 (n=27) | flareForSource | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: ---- Halo chromatic fringing (new, gated) ------------------------- R ring pulled slightly inward, B pushed outward — classic lateral dispersion. G keeps the base ring. |
| 1058–1229 (n=172) | computeLensFlare, flareForSource | clones, rig, roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: ---- N-gon iris ghosts (new, gated) ----------------------------------- Polygonal aperture reflections marching along the source→center axis, body + bright rim, per-element hash for spacing/size/rotation jitter. March from just past the source through center to the far side, with |

## Slice 4: `indra/newview/app_settings/shaders/class1/deferred/blurLightF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 36–51 (n=16) | unknown/non-function (uniform vec2 screen_res;) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [ShadowDist P1] Control D: sun-shadow (.r) softening decoupled from the SSAO (.g) blur. 0 = stock (the shadow channel shares the SSAO kernel; the original code below runs untouched), 1 = Gaussian sigma in pixels, 2 = Gaussian sigma in world metres (converted per pixel from depth, |
| 118–188 (n=71) | main | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: [ShadowDist P1] Control D. The stock loops above have already produced .g (SSAO) -- and .b/.a, which softenLight never reads -- exactly as before; only the shadow channel is replaced here. Dedicated taps sit on the UNPERTURBED pixel grid at whole-pixel offsets (the light map is p |

## Slice 4: `indra/newview/app_settings/shaders/class1/deferred/emissiveF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 28–117 (n=90) | main | capture-sidecar, clones, looks, roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: Shared Cover replays authored legacy glow separately from its treatment bloom.  Keep dissolve coverage active, but omit actorFxEmissive so the actorghost glow-only pass remains the single synthetic contributor. 0: no cutoff, 1: texture alpha, 2: texture * vertex alpha, 3: texture |

## Slice 4: `indra/newview/app_settings/shaders/class1/deferred/pbropaqueF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–4 (n=4) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 62–156 (n=95) | main | clones, looks, roto-tron, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: WP2 material contract: after Actor FX, before G-buffer packing/forward lighting. Material-owning Cover looks keep the geometric surface but do not inherit authored tangent-space bumps. Layer and Clone return exactly 1 here. |
| 158–265 (n=108) | defined, main | looks, roto-tron, sensor, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: Neutral occlusion prevents the old material's cavities/noise from modulating a flat/sensor/owned-material Cover treatment. Evaluate derivatives for the entire quad, then exclude emitting pixels. [TronA0] rigged mesh: avatar geometry -> tagged flag |
| 271–281 (n=11) | unknown/non-function (uniform sampler2D diffuseMap;  //always in sRGB space) | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 312–314 (n=3) | unknown/non-function (void main()) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 4: `indra/newview/llagentlistener.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 254–265 (n=12) | LLAgentListener::requestSit | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 323–329 (n=7) | LLAgentListener::findObjectClosestTo | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 487–498 (n=12) | LLAgentListener::getAutoPilot | clones | creative-candidate | unknown; no local conditional evidence | Edits LLUUID, LLViewerObject, gObjectList |
| 501–507 (n=7) | LLAgentListener::getAutoPilot | clones | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 544–550 (n=7) | LLAgentListener::startFollowPilot | clones | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 564–579 (n=16) | LLAgentListener::startFollowPilot | clones | creative-candidate | unknown; conditional evidence present | Edits gObjectList, LLViewerObject |
| 613–624 (n=12) | LLAgentListener::lookAt | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: Never look at a client-only ghost clone (via obj_uuid or nearest-position). |
| 794–800 (n=7) | LLAgentListener::getNearbyAvatarsList | clones | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 817–823 (n=7) | LLAgentListener::getNearbyObjectsList | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 838–844 (n=7) | LLAgentListener::getAgentScreenPos | clones | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 4: `indra/newview/llavataractions.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 39–45 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | clones | creative-candidate | unknown; no local conditional evidence | Edits LLGhostAvatar |
| 114–123 (n=10) | LLAvatarActions::requestFriendshipDialog | clones | creative-candidate | unknown; conditional evidence present | Edits LLGhostAvatar |
| 143–152 (n=10) | LLAvatarActions::requestFriendshipDialog | clones | creative-candidate | unknown; conditional evidence present | Edits LLGhostAvatar |
| 201–208 (n=8) | LLAvatarActions::offerTeleport | clones | creative-candidate | unknown; conditional evidence present | Edits LLGhostAvatar |
| 235–242 (n=8) | LLAvatarActions::startIM | clones | creative-candidate | unknown; conditional evidence present | Edits LLGhostAvatar |
| 277–284 (n=8) | LLAvatarActions::startCall | clones | creative-candidate | unknown; conditional evidence present | Edits LLGhostAvatar |
| 393–400 (n=8) | LLAvatarActions::showProfile | clones | creative-candidate | unknown; conditional evidence present | Edits LLGhostAvatar |
| 635–642 (n=8) | LLAvatarActions::showOnMap | clones | creative-candidate | unknown; conditional evidence present | Edits LLGhostAvatar |
| 651–658 (n=8) | LLAvatarActions::pay | clones | creative-candidate | unknown; conditional evidence present | Edits LLGhostAvatar |
| 731–738 (n=8) | LLAvatarActions::teleportRequest | clones | creative-candidate | unknown; conditional evidence present | Edits LLGhostAvatar |
| 854–861 (n=8) | LLAvatarActions::share | clones | creative-candidate | unknown; conditional evidence present | Edits LLGhostAvatar |
| 1378–1385 (n=8) | LLAvatarActions::toggleBlock | clones | creative-candidate | unknown; conditional evidence present | Edits LLGhostAvatar |
| 1400–1407 (n=8) | LLAvatarActions::toggleMute | clones | creative-candidate | unknown; conditional evidence present | Edits LLGhostAvatar |
| 1461–1468 (n=8) | LLAvatarActions::inviteToGroup | clones | creative-candidate | unknown; conditional evidence present | Edits LLGhostAvatar |

## Slice 4: `indra/newview/llavatarrenderinfoaccountant.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 230–236 (n=7) | LLAvatarRenderInfoAccountant::avatarRenderInfoReportCoro | clones | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 4: `indra/newview/llcallingcard.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 109–121 (n=13) | LLAvatarTracker::track | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: Never track a client-only ghost clone: its id is synthetic, so TrackAgent would send a bogus PreyID to the simulator. |

## Slice 4: `indra/newview/lldrawable.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 48–55 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | rig, tracking | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 63–82 (n=20) | markPositionRebuild | tracking | creative-candidate | unknown; conditional evidence present | Comment evidence: Only the rigid-motion rebuild sites use this helper. Scale changes and REBUILD_VOLUME/ALL retain their genuine silhouette invalidations. |
| 627–633 (n=7) | LLDrawable::updateXform | rig | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 729–735 (n=7) | LLDrawable::updateXform | unknown | unknown-review | unknown; no local conditional evidence | Edits gPipeline, LLDrawable |
| 822–857 (n=36) | LLDrawable::updateMoveUndamped | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [ProbeOnDemand] H1a: snapshot the transform + bounds, note a real displacement afterwards (updateXform's return value is interpolation error, not displacement). Read-only; nothing runs unless recording. |
| 880–886 (n=7) | LLDrawable::updatePartition | unknown | unknown-review | unknown; no local conditional evidence | Edits gPipeline, LLDrawable |
| 888–921 (n=34) | LLDrawable::updateMoveDamped | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [ProbeOnDemand] H1a: see updateMoveUndamped(). |
| 1057–1063 (n=7) | LLDrawable::shiftPos | rig | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1125–1135 (n=11) | LLDrawable::onSpatialExtentsChanged | rig | creative-candidate | unknown; conditional evidence present | Edits LLDrawable |
| 1461–1502 (n=42) | LLSpatialBridge::updateSpatialExtents | capture-sidecar, clones | creative-candidate | unknown; conditional evidence present | Comment evidence: Ghost entity clones apply a render-only outer scale about a world-space foot pivot (LLClientOuterTransform). This bridge's octree bounds come from unscaled native geometry, so a large clone's attachment is frustum/occlusion-culled by its tiny native box (whole attachments blink o |
| 1639–1652 (n=14) | LLSpatialBridge::setVisible | looks | creative-candidate | unknown; no local conditional evidence | Edits LLControlAvatar, LLPipeline, LLVOAvatar |
| 1738–1749 (n=12) | LLSpatialBridge::updateDistance | looks | creative-candidate | unknown; conditional evidence present | Edits LLControlAvatar |
| 1938–1940 (n=3) | unknown/non-function (void LLHUDBridge::shiftPos(const LLVector4a& vec)) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 4: `indra/newview/lldrawpoolterrain.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 720–774 (n=55) | LLDrawPoolTerrain::beginVelocityPass, LLDrawPoolTerrain::endVelocityPass, LLDrawPoolTerrain::renderVelocity | capture-sidecar, clones | creative-candidate | unknown; conditional evidence present | Comment evidence: ============================================================================ [BDMerge A5.4-1a] Velocity / motion-vector pass. Donor: Black Dragon lldrawpoolterrain.cpp:208-257. Terrain is a face pool and never moves, so we feed the region render matrix as BOTH current (via applyM |

## Slice 4: `indra/newview/llface.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 54–60 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1191–1213 (n=23) | LLFace::canRenderAsMask | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: One policy owns routing for legacy and PBR. PBR is handled by its own pool/pass selection below LLVOVolume, while this helper applies the same resolved decision to legacy auto-mask routing. |
| 2283–2318 (n=36) | face_resolve_outer_transform | capture-sidecar, clones | creative-candidate | unknown; conditional evidence present | Comment evidence: Ghost entity clones share a post-skin outer transform (LLClientOuterTransform) via their object and/or skinning avatar. Mirrors resolve_outer_transform() in llvovolume.cpp so face pixel-area culling can account for the rendered (scaled) footprint. Returns null for normal content, |
| 2413–2437 (n=25) | LLFace::calcPixelArea | capture-sidecar, clones | creative-candidate | unknown; conditional evidence present | Comment evidence: Ghost entity clones render at an outer scale about a world-space foot pivot, but the extents above (native mExtents / joint-derived mRiggedExtents) are unscaled -- so faces on a large clone evaluate their 1x pixel area and cross the ~8px force-cull, blinking. Scale the size and r |

## Slice 4: `indra/newview/llfloaterpay.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 46–52 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | clones | creative-candidate | unknown; no local conditional evidence | Edits LLGhostAvatar |
| 362–368 (n=7) | LLFloaterPay::payViaObject | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 404–418 (n=15) | LLFloaterPay::payDirectly | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: A client-only ghost clone has a synthetic id -- never open a pay dialog for it. |
| 558–568 (n=11) | LLFloaterPay::give | unknown | unknown-review | unknown; conditional evidence present | Edits LLViewerObject, gObjectList |

## Slice 4: `indra/newview/llmanip.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 27–33 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | clones | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 351–375 (n=25) | LLManip::getSavedPivotPoint | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: [GhostStudio] mirror getPivotPoint()'s raised manip pivot for the ghost proxy so translate drag/snap deltas stay consistent -- a raised live pivot against a bbox-centre saved pivot would jump the drag at start. Translation doesn't change the proxy's rotation/scale, so the raise o |
| 378–389 (n=12) | LLManip::getPivotPoint | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: Only the owned proxy of a displaced ENTITY gets a body pivot. Other proxies keep the historical raised manipulator origin unchanged. |

## Slice 4: `indra/newview/llnearbyvoicemoderation.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 55–66 (n=12) | LLNearbyVoiceModeration::getVOAvatarFromId | clones | creative-candidate | unknown; conditional evidence present | Edits LLVOAvatar |

## Slice 4: `indra/newview/llspatialpartition.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 44–57 (n=14) | unknown/non-function (declaration, XML, build list or global scope) | clones, rig | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 776–798 (n=23) | LLSpatialGroup::handleInsertion, LLSpatialGroup::handleRemoval | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [ProbeOnDemand] H2: membership (creation outside a move, motion inside one). [ProbeOnDemand] H2: membership (deletion outside a move, motion inside one); before removeObject so the drawable's group / bounds are still valid. |
| 982–1026 (n=45) | ProbeMoveNote, ~ProbeMoveNote | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [ProbeOnDemand] H1b: a partition move is motion, not creation/deletion. While it is open, octree membership notes (H2) are motion; on every exit the bounds change is noted as debounced motion (usually a no-op for volumes: genBBoxes updates extents before movePartition and H4 cove |
| 1032–1039 (n=8) | LLSpatialPartition::move | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 3294–3303 (n=10) | visit | clones | creative-candidate | unknown; conditional evidence present | Edits ALDerenderList |
| 3920–3927 (n=8) | LLDrawInfo::validate | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 4229–4231 (n=3) | unknown/non-function (void LLCullResult::assertDrawMapsEmpty()) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 4: `indra/newview/lltoolcomp.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 54–89 (n=36) | LLToolComposite::isManipulating, LLToolComposite::releaseManip, alGhostStockPickConsumed | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: [GhostEditFix] Shared by the Translate/Rotate/Scale pickCallbacks: while a Ghost Studio edit session has handed off to the stock gizmos, only the ghost tool can hit-test ghosts. Returns true when the pick was consumed (ghost switch, or a confirmed empty-ground click flagged for t |
| 304–315 (n=12) | LLToolCompTranslate::pickCallback | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: [GhostEditFix] ghost pick / empty-ground exit before any stock selection change |
| 436–447 (n=12) | LLToolCompScale::pickCallback | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: [GhostEditFix] ghost pick / empty-ground exit before any stock selection change |
| 653–664 (n=12) | LLToolCompRotate::pickCallback | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: [GhostEditFix] ghost pick / empty-ground exit before any stock selection change |
| 755–766 (n=12) | LLToolCompGun::resetGrabStateWithoutSending | unknown | unknown-review | unknown; conditional evidence present | Edits LLToolCompGun |

## Slice 4: `indra/newview/lltoolcomp.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 81–96 (n=16) | unknown/non-function (public:) | clones | creative-candidate | unknown; no local conditional evidence | Comment evidence: [GhostStudio] True while an internal LLManip is actively dragging (holds mouse capture). The composite / getCurrentTool() do NOT hold the capture -- the internal manip does -- so an external driver must ask THIS, not hasMouseCapture(), to detect a gizmo drag. Out-of-line: LLManip |
| 252–261 (n=10) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Functional self-test only: forget this composite's OWN grab instance state without sending anything (its mouse capture was dropped without the tool callback). |

## Slice 4: `indra/newview/llviewercamera.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 377–391 (n=15) | LLViewerCamera::setPerspective | clones | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge A5.4-1a] Capture the (un-jittered) projection for the velocity pass. The velocity shaders combine this with the (jitter-free) modelview to produce clean motion vectors. (The SMAA T2x jitter that used to be applied after this capture was reverted with A5.8 - it ghosted on |

## Slice 4: `indra/newview/llviewerjointmesh.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 171–209 (n=39) | LLViewerJointMesh::uploadJointMatrices | capture-sidecar | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge A5.4-1b] Maintain the previous frame's palette for the velocity pass (donor: BD llviewerjointmesh.cpp:190-207, with a U32-underflow-safe staleness test). After a frame gap the previous palette is reseeded from the current one -> zero limb velocity. [BDMerge A5.4-1b] velo |
| 217–263 (n=47) | LLViewerJointMesh::isActorGhostPaletteReady, LLViewerJointMesh::isActorGhostReplayReady | clones | creative-candidate | unknown; conditional evidence present | Edits LLViewerJointMesh, LLPolyMesh, LL_CHARACTER_MAX_JOINTS_PER_MESH, LLVertexBuffer |
| 291–303 (n=13) | LLViewerJointMesh::drawShape | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Body meshes share a palette. Even a first mesh routed to the other shadow pass must upload it for the following meshes in this pass. |
| 320–330 (n=11) | LLViewerJointMesh::drawShape | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: A raw GL test image has no analysed alpha; keep its body shadow solid. |
| 336–361 (n=26) | LLViewerJointMesh::drawShape | unknown | unknown-review | unknown; conditional evidence present | Edits LLGLTexture, gGL, LLViewerTextureManager, LLDrawPoolAvatar |
| 367–372 (n=6) | LLViewerJointMesh::drawShape | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 4: `indra/newview/llviewerjointmesh.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 54–69 (n=16) | unknown/non-function (public:) | clones, looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ActorStyle/SharedActivation] Read-only readiness proof used before a Cover actor suppresses its native system-avatar beauty.  A visible mesh is ready only when drawShape() can consume its complete VB/index range; an invisible mesh is not a required source descriptor for this fra |
| 80–100 (n=21) | unknown/non-function (public:) | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Comment evidence: One routing decision per frame, shared by every map's solid/cut passes. [BDMerge A5.4-1b] Previous frame's joint palette for the classic-avatar velocity pass (donor: Black Dragon llviewerjointmesh). Uploaded to AVATAR_LAST_MATRIX when gAvatarVelocityProgram is bound; reset to the |

## Slice 4: `indra/newview/llviewerobject.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 28–34 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 46–52 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 154–168 (n=15) | getClientOuterTransform, setClientOuterTransform | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Edits LLClientOuterTransform |
| 315–325 (n=11) | worldShadowBoundsRevision | rig | creative-candidate | unknown; no local conditional evidence | Comment evidence: Changes to link membership, drawable bounds or transforms invalidate the resident lights' enclosure cache. Stored on the edit root. |
| 353–362 (n=10) | unknown/non-function (public:) | clones | creative-candidate | unknown; no local conditional evidence | Comment evidence: [GhostStudio] position used for the EDIT MANIPULATOR pivot only. Defaults to getPivotPositionAgent(); the ghost manip proxy raises it above its (large, invisible) body so the constant-screen-size gizmo isn't buried in the clone. |
| 421–450 (n=30) | unknown/non-function (public:) | clones | creative-candidate | unknown; no local conditional evidence | Comment evidence: [GhostStudio] Debug-gated, viewer-local resolved material bindings. These never change semantic TE ids and never emit simulator updates. |
| 488–505 (n=18) | getLastServerScale, hasLastServerTransform | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Latest accepted ObjectUpdate, before ping/omega extrapolation. A local render override must restore these values, including when it starts on an already moving prim. This cache is never serialized or sent. |
| 842–852 (n=11) | unknown/non-function (public:) | clones | creative-candidate | unknown; no local conditional evidence | Comment evidence: Lazily allocated only for debug-gated local ghost volumes. Keeping one null pointer here avoids adding vector/map storage to every world object. |
| 859–890 (n=32) | isCineRigEmitter, isCineRigProbe, isColorChart, isGhostManipProxy, isLocalMeshPreview | capture-sidecar, clones, looks, rig | creative-candidate | unknown; no local conditional evidence | Comment evidence: Viewer-only render ownership. Never serialized or sent to the simulator. [GhostStudio] WHICH KIND of client-only object this is. isLocalOnly() only says "no sim counterpart" -- that is true for BOTH a local mesh preview and a Ghost Studio manipulation proxy, which must NOT share  |
| 955–961 (n=7) | unknown/non-function (public:) | clones | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1061–1075 (n=15) | unknown/non-function (protected:) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLVector3d, LLUUID, LLVector3, LLQuaternion |
| 1150–1157 (n=8) | unknown/non-function (public:) | rig | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1215–1217 (n=3) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 4: `indra/newview/llviewerobjectlist.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 27–33 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | tracking | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 43–49 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | rig | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 244–264 (n=21) | LLViewerObjectList::processUpdateCore | rig, tracking | creative-candidate | unknown; conditional evidence present | Comment evidence: [ProbeOnDemand] An object re-created from the object cache after a cache cull is not a scene change; a real server update makes it authoritative again. |
| 369–383 (n=15) | LLViewerObjectList::processObjectUpdateFromCache | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: BD/FS -> Alchemy merge campaign, item F3: also gate derendered avatars reconstructed from the region's VOCache (see updateRegion() call for LLVOAvatar just above -- this path does serve avatars, not just prims). processAvatarUpdate() runs FIRST and unconditionally: it also record |
| 531–537 (n=7) | LLViewerObjectList::processObjectUpdate | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 663–678 (n=16) | LLViewerObjectList::processObjectUpdate | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: BD/FS -> Alchemy merge campaign, item F3: also gate derendered avatars. processObjectUpdate() below only tracks TYPE_OBJECT entries (root/child local-id bookkeeping doesn't apply to avatars), so TYPE_AVATAR entries need their own check here to keep FSRadar's "Derender" / "Show Fr |
| 685–695 (n=11) | LLViewerObjectList::processObjectUpdate | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 993–999 (n=7) | LLViewerObjectList::update | clones | creative-candidate | unknown; conditional evidence present | Edits LLVOAvatar |
| 1402–1409 (n=8) | LLViewerObjectList::cleanupReferences | tracking | creative-candidate | unknown; no local conditional evidence | Comment evidence: Includes recursive child markDead calls, which bypass killObject. |

## Slice 4: `indra/newview/llviewertextureanim.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 28–34 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | clones | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 65–79 (n=15) | LLViewerTextureAnim::reset | clones | creative-candidate | unknown; conditional evidence present | Comment evidence: [Temporal Capture] When temporal is driving this animation, a reset must also clear the accumulated presentation phase so the driven path restarts in step with the stock (wall) path, which reset() zeroes via the timer above. Guarded on the drive gate so the stock (not-driven) pat |
| 133–139 (n=7) | LLViewerTextureAnim::animateTextures | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 237–288 (n=52) | LLViewerTextureAnim::advanceSteppedTime, LLViewerTextureAnim::advanceTime | capture-sidecar, clones | creative-candidate | unknown; conditional evidence present | Comment evidence: Advance phase only; leave the displayed frame/offset/rotation untouched. This also consumes every presentation delta when temporal capture drives us. [Temporal Capture] Texture-anim drive: advance this instance's phase on the presentation clock when driven, else exactly the stock |

## Slice 4: `indra/newview/llworld.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 93–101 (n=9) | unknown/non-function (LLWorld::LLWorld() :) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 684–690 (n=7) | LLWorld::updateRegions | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 752–792 (n=41) | LLWorld::applyPendingVisualWaterHeight, LLWorld::requestVisualWaterHeightUpdate | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Region planes first, then exactly one hole/edge-water rebuild. |
| 1392–1398 (n=7) | LLWorld::getAvatars | clones | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1452–1458 (n=7) | LLWorld::getAvatars | clones | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1498–1504 (n=7) | LLWorld::getAvatars | clones | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1538–1544 (n=7) | LLWorld::getNearbyAvatarsAndMaxGPUTime | clones | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1562–1568 (n=7) | LLWorld::getAvatar | clones | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 4: `indra/newview/rlvfloaters.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 81–88 (n=8) | rlvGetItemNameFromObjID | clones | creative-candidate | unknown; conditional evidence present | Edits LLVOAvatar |

## Slice 5: `indra/llrender/llrendertarget.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 34–75 (n=42) | ScopedRTGpuZone, ~ScopedRTGpuZone | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Keep the existing GPU timestamps, but measure their CPU submission cost separately from the GL operations inside bind/clear/flush. No extra queries. |
| 201–209 (n=9) | LLRenderTarget::resize | unknown | unknown-review | unknown; no local conditional evidence | Edits LLImageGL, LLTexUnit |
| 360–375 (n=16) | LLRenderTarget::addColorAttachment | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: deleteTextures defers the GL delete by several frames; remove the just-created accounting record now so a failed allocation cannot transiently drive the governor. |
| 440–456 (n=17) | LLRenderTarget::allocateDepth | unknown | unknown-review | unknown; no local conditional evidence | Edits LLImageGLMemory, LLImageGL |
| 569–586 (n=18) | LLRenderTarget::bindTarget | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Enclose GPU timestamp entry AND exit in the CPU zone. |
| 588–614 (n=27) | LLRenderTarget::bindTarget | unknown | unknown-review | unknown; conditional evidence present | Edits LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE |
| 616–623 (n=8) | LLRenderTarget::clear | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_GPU_ZONE, LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE |
| 632–641 (n=10) | LLRenderTarget::clear | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE |
| 643–652 (n=10) | LLRenderTarget::clear | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE |
| 674–693 (n=20) | LLRenderTarget::flush | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_GPU_ZONE, gGL, LL_PROFILE_ZONE_NAMED_CATEGORY_PIPELINE, LL_PROFILE_ZONE_NUM |
| 703–800 (n=98) | LLRenderTarget::ScopedDepthTargetSequence::ScopedDepthTargetSequence, LLRenderTarget::ScopedDepthTargetSequence::bindTarget, LLRenderTarget::ScopedDepthTargetSequence::finish, LLRenderTarget::ScopedDepthTargetSequence::flush, LLRenderTarget::ScopedDepthTargetSequence::~ScopedDepthTargetSequence, LLRenderTarget::flush | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Drain BEFORE changing either the logical stack or the GL target. Pop only the logical sibling. bindTarget binds A -> B directly and updates FBO/resolution/viewport together, reselecting this FBO's NONE buffers. B inherits the entry parent, never A (no RT-stack cycle). Keep the st |
| 838–961 (n=124) | LLRenderTarget::copyColorToAttachment0, LLRenderTarget::copyContents, LLRenderTarget::copyContentsFromAttachment | roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: Restore the convention bindTarget() establishes (GL_COLOR_ATTACHMENT0) so other callers -- including this class's own copyContents(), which reads whatever the source's current read buffer happens to be -- keep finding it in place. [RotoInk] See header comment: narrows the blit's  |

## Slice 5: `indra/newview/app_settings/shaders/class1/deferred/avatarF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 37–65 (n=29) | main | looks, roto-tron | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 69–113 (n=45) | main | looks, roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: [TronA0] system avatar mesh: always avatar geometry -> tagged flag |

## Slice 5: `indra/newview/app_settings/shaders/class1/deferred/bumpF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–4 (n=4) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 41–81 (n=41) | main | looks, roto-tron, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: WP2 material contract: after Actor FX, before G-buffer packing/forward lighting. |
| 84–162 (n=79) | defined, main | looks, roto-tron, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: [TronA0] rigged mesh: avatar geometry -> tagged flag |

## Slice 5: `indra/newview/app_settings/shaders/class1/deferred/deferredUtil.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–4 (n=4) | unknown/non-function (declaration, XML, build list or global scope) | rig | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 23–33 (n=11) | unknown/non-function (declaration, XML, build list or global scope) | rig | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 70–365 (n=296) | animatedGoboUV, envOnlyProjectorColor, goboAnimIntensity, goboHash, goboLod, goboValueNoise, proceduralGobo, sampleGobo | sensor, shafts-volumetrics | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge Batch 2] Feature 2 - gobo/cookie mip + anisotropic filtering. When non-zero, the cookie LOD samplers clamp their focus-based LOD up to the screen-space (derivative) minification LOD, so a projected gobo viewed at a grazing angle or from far away samples a coarser mip ins |
| 461–524 (n=64) | alrSilDecode, getNormRaw | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 539–545 (n=7) | getTexture2DLodAmbient | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 551–557 (n=7) | getTexture2DLodDiffuse | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 576–582 (n=7) | getProjectedLightAmbiance | sensor | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 588–610 (n=23) | getProjectedLightDiffuseColor, texture2DLodSpecular | sensor, shafts-volumetrics | creative-candidate | unknown; conditional evidence present | Comment evidence: Surface-only chromatic split. Volumetric consumers call the single-tap getTexture2DLodDiffuse funnel directly and never enter this branch. |
| 646–652 (n=7) | getProjectedLightSpecularColor | sensor | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 869–2008 (n=1140) | applyShadowLift, calcDiffuseSpecular, localLightAlpha, localLightGGXRatio, localLightLegacy, localLightPbr, localLightSpecular, pbrBaseLightKeep, pbrCalcPointLightOrSpotLight, pbrCalcPointLightOrSpotLightRim, pbrCalcPointLightOrSpotLightRimKeep, pbrCalcPointLightOrSpotLightRimKeepRef, pbrCalcPointLightOrSpotLightRimKeepRefKernel, pbrCalcPointLightOrSpotLightRimSplit, pbrCalcPointLightOrSpotLightRimSplitKernel, relightAtten, relightLegacy, relightLegacyNormalization, relightLegacySized, relightNormalization, relightPbr, relightPbrSized, relightPunctual, relightRatio, relightRepresentative, relightSpecularRadius, relightSphere, relightUnit, rigRimForwardPixel, rigRimLoadPixel, rigRimMode, rigRimNeutralPixel, rigRimOctDecode, rigRimOctEncode, rigRimSignNotZero, rigRimSilBandWidth, rigRimSilProx, rigRimTerm, rigRimTermEx, tronRimTint | looks, rig, roto-tron, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: [LocalLightSize] Roughness-widening form of Karis 2013 sphere lights. Only D changes: centre-direction Fresnel, visibility, diffuse, cone, shadow and attenuation remain exactly as before. The normalized lobe has peak (alpha/alphaPrime)^2 of the original; shoulders may rise as it  |
| 2022–2033 (n=12) | pbrBaseLight | looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: [AvatarLightResponse] old name: forwards spec_keep = 1.0. |
| 2052–2172 (n=121) | pbrCalcPointLightOrSpotLightRim, pbrCalcPointLightOrSpotLightRimKeep, pbrCalcPointLightOrSpotLightRimKeepRef, pbrCalcPointLightOrSpotLightRimSplit | rig | creative-candidate | unknown; no local conditional evidence | Comment evidence: Compatibility overloads: consumers without an emitter radius retain point-light shading. |

## Slice 5: `indra/newview/app_settings/shaders/class1/deferred/diffuseAlphaMaskF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–4 (n=4) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 40–56 (n=17) | unknown/non-function (in vec2 vary_texcoord0;) | looks, roto-tron, weather | creative-candidate | unknown; no local conditional evidence | Comment evidence: WP2 material contract: after Actor FX, before G-buffer packing/forward lighting. |
| 63–86 (n=24) | defined, main | roto-tron, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: Match forward alpha when a card is rendered with culling disabled. [TronA0] rigged mesh: avatar geometry -> tagged flag |

## Slice 5: `indra/newview/app_settings/shaders/class1/deferred/diffuseAlphaMaskIndexedF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–4 (n=4) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 37–75 (n=39) | main | looks, roto-tron, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: WP2 material contract: after Actor FX, before G-buffer packing/forward lighting. |
| 79–134 (n=56) | defined, main | looks, roto-tron, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: [TronA0] rigged mesh: avatar geometry -> tagged flag |

## Slice 5: `indra/newview/app_settings/shaders/class1/deferred/diffuseAlphaMaskNoColorF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–4 (n=4) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 33–73 (n=41) | main | looks, roto-tron, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: WP2 material contract: after Actor FX, before G-buffer packing/forward lighting. |
| 75–130 (n=56) | defined, main | looks, roto-tron, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: [TronA0] rigged mesh: avatar geometry -> tagged flag |

## Slice 5: `indra/newview/app_settings/shaders/class1/deferred/diffuseF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–4 (n=4) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 36–142 (n=107) | defined, main | looks, roto-tron, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: WP2 material contract: after Actor FX, before G-buffer packing/forward lighting. [TronA0] rigged mesh / avatar eyes: avatar geometry -> tagged flag |

## Slice 5: `indra/newview/app_settings/shaders/class1/deferred/diffuseIndexedF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–4 (n=4) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 34–145 (n=112) | defined, main | looks, roto-tron, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: WP2 material contract: after Actor FX, before G-buffer packing/forward lighting. [TronA0] rigged mesh: avatar geometry -> tagged flag |

## Slice 5: `indra/newview/app_settings/shaders/class1/deferred/emissiveIndexedF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 33–61 (n=29) | unknown/non-function (declaration, XML, build list or global scope) | looks, roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: Common base-pass policy for every material slot in this DrawInfo. 0: no cutoff, 1: texture alpha, 2: texture * vertex alpha, 3: texture alpha with the legacy-material 1/512 cutoff bias. |
| 105–197 (n=93) | main, shared_minimum_alpha | looks, roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: TARGET_MASK legacy glow encodes its 16-bit cutoff in emissive RG and marks it in B. Ordinary glow keeps emissive RGB at zero. See the scalar variant: only shared authored replay of a BLEND base range needs the extra diffuse vertex-alpha coverage term. |

## Slice 5: `indra/newview/app_settings/shaders/class1/deferred/fullbrightF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–44 (n=20) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 50–73 (n=24) | unknown/non-function (in vec2 vary_texcoord0;) | looks, roto-tron | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 75–83 (n=9) | unknown/non-function (uniform float minimum_alpha;) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 88–96 (n=9) | main | looks, roto-tron | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 111–265 (n=155) | main | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: Fullbright batches intentionally do not carry a normal attribute. Derive one only for active Actor FX; disabled draws stay identity and avoid derivatives plus all style/emissive work. This pool is unblended: destination alpha is the renderer's glow tag, not material transparency. |

## Slice 5: `indra/newview/app_settings/shaders/class1/deferred/globalF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 32–57 (n=26) | envOnlyEnabled, envOnlyIdentity, envOnlySetFlag, gbufferTagPreferred | sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: Shared by every fragment link, including probe-only programs. Deferred G-buffer reads set the recipient; forward draws use mode 2. Mode 0 executes the existing Personal Lighting arithmetic without identity interpolation. |
| 80–102 (n=23) | gbufferAvatarFlag | roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: [TronA0] Per-pixel avatar tag: avatar-family G-buffer writers route their flag through this before encodeNormal(). With GBUFFER_AVATAR_TAG emitted by the preamble (llshadermgr.cpp, HDR/RGBA16 normals only) an even 1/6 code becomes the odd code just below it ("avatar + this flag") |

## Slice 5: `indra/newview/app_settings/shaders/class1/deferred/impostorF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 39–45 (n=7) | unknown/non-function (in vec2 vary_texcoord0;) | roto-tron | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 55–62 (n=8) | main | roto-tron | creative-candidate | unknown; no local conditional evidence | Comment evidence: [TronA0] avatar impostors: always avatar geometry -> tagged flag |

## Slice 5: `indra/newview/app_settings/shaders/class1/deferred/materialIndexedF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–4 (n=4) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 217–249 (n=33) | getNormal | looks, roto-tron | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 257–273 (n=17) | getSpecular | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 284–299 (n=16) | main | looks, roto-tron, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: WP2: decoded material response precedes packing and lighting. |
| 309–395 (n=87) | defined, main | looks, roto-tron, weather | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 397–409 (n=13) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 5: `indra/newview/app_settings/shaders/class1/deferred/pbrglowF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 36–42 (n=7) | unknown/non-function (out vec4 frag_color;) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 45–162 (n=118) | main | looks, roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: Opaque/MASK beauty applies the base-colour factor before its cutoff. BLEND beauty intentionally cuts on texture alpha alone, then uses the combined alpha as coverage. actorFxUseCoverageAlpha identifies that latter alpha-pool use of this shared glow shader. Re-evaluate authored gl |

## Slice 5: `indra/newview/app_settings/shaders/class1/deferred/pbrglowIndexedF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 62–90 (n=29) | unknown/non-function (uniform sampler2D basecolor7; uniform sampler2D emissivemap7;) | looks, roto-tron | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 142–226 (n=85) | main | looks, roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: Match PBR beauty's Layer/Cover handling of authored emissive before adding the style's independent synthetic bloom contribution. |

## Slice 5: `indra/newview/app_settings/shaders/class1/deferred/pbropaqueIndexedF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–4 (n=4) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 83–110 (n=28) | unknown/non-function (vec3 linear_to_srgb(vec3 c);) | looks, roto-tron | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 214–376 (n=163) | defined, main | looks, roto-tron, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: WP2 material contract: after Actor FX, before G-buffer packing/forward lighting. Evaluate derivatives for the entire quad, then exclude emitting pixels. [TronA0] rigged mesh: avatar geometry -> tagged flag |

## Slice 5: `indra/newview/app_settings/shaders/class1/lighting/lightAlphaMaskF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 29–59 (n=31) | default_lighting | looks, roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: [TronT2 fix] first statement of the calling main() (avatar/eyeball/ simple F are 'void main() { default_lighting(); }'), before the cutout. |
| 61–111 (n=51) | default_lighting | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: This forward path has no emissive GBuffer.  Add the same linear Actor FX lift used by deferred/PBR after scene lighting, then return to the legacy shader's sRGB soft-clip domain. |

## Slice 5: `indra/newview/app_settings/shaders/class1/lighting/lightAlphaMaskNonIndexedF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 31–61 (n=31) | default_lighting | looks, roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: [TronT2 fix] first statement of the calling main() (avatar/eyeball/ simple F are 'void main() { default_lighting(); }'), before the cutout. |
| 63–111 (n=49) | default_lighting | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 5: `indra/newview/app_settings/shaders/class2/deferred/pbralphaF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–4 (n=4) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 24–35 (n=12) | unknown/non-function (declaration, XML, build list or global scope) | rig | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 51–77 (n=27) | unknown/non-function (uniform vec3 sun_dir;) | unknown | unknown-review | unknown; conditional evidence present | Edits ALR_RIMSPLIT_PARITY |
| 99–134 (n=36) | unknown/non-function (uniform float minimum_alpha; // PBR alphaMode: MASK, See: mAlphaCutoff, setAlpha) | looks, rig, roto-tron, sensor | creative-candidate | unknown; conditional evidence present | Edits LLRender |
| 164–247 (n=84) | main | looks, rig, roto-tron, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: [RigRim] same as above plus the per-light rim params (deferredUtil.glsl) [AvatarLightResponse] spec_keep variants (deferredUtil.glsl); alr* from avatarLightResponseF.glsl [RigRimQ] per-pixel quality inputs (deferredUtil.glsl; struct must match there) WP2 material contract: after  |
| 252–257 (n=6) | main | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 259–313 (n=55) | main | looks, rig, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: [RigRimQ] Toksvig input, evaluated unconditionally at main scope so the derivative never sits under a branch or behind a later discard (review P2-5).  Not used unless rig_rim_mode != 0: colour math unchanged. |
| 324–421 (n=98) | defined, main | looks, sensor, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: Evaluate derivatives for the entire quad, then exclude emitting pixels. |
| 423–518 (n=96) | LIGHT_LOOP, main | looks, rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [RigRimQ] forward-alpha local pixel (Phase 1: smooth normal + anti-shimmer only; silhouette / subject stay neutral).  rim_fw was hoisted to main scope right after 'norm'; mode 0 keeps the neutral pixel. [RigRim] ...Rim variant carries rig_rim_lights[i]; identical result when the  |
| 521–570 (n=50) | main | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: Dedicated R8 diagnostic output; pipeline GL_MAX blending makes this a frame-wide mismatch OR, so a later zero cannot erase an earlier one. |
| 574–584 (n=11) | unknown/non-function (uniform sampler2D emissiveMap;) | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 5: `indra/newview/app_settings/shaders/class3/deferred/fullbrightShinyF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 52–112 (n=61) | main | looks, roto-tron, sensor | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 122–187 (n=66) | main | looks, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: Capture the styled, unlit surface colour before reflection-probe energy is added. Actor FX emission is a material property, not reflected light. This is an unblended world pass, so destination alpha is glow metadata rather than material coverage.  Publish synthetic Actor FX emiss |

## Slice 5: `indra/newview/app_settings/shaders/class3/deferred/materialF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–4 (n=4) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 24–38 (n=15) | unknown/non-function (declaration, XML, build list or global scope) | rig | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 60–139 (n=80) | unknown/non-function (uniform mat3 normal_matrix;) | capture-sidecar, looks, roto-tron, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: [ReShadeBridge] Visible-diffuse sidecar MRT outputs (mRT->screen attachments 1 and 2). The alpha pool renders legacy-material BLEND faces with both attachments write-enabled (lldrawpoolalpha.cpp renderAlpha opens the indexed guard for every draw in its loop, and this program is b |
| 152–269 (n=118) | calcPointLightOrSpotLightKernel | looks, rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [RigRim] shared rim helper (deferredUtil.glsl) [RigRimQ] per-pixel quality inputs (deferredUtil.glsl; struct must match there) [RigRim] 'rim' = this light unit's rig rim params (rig_rim_lights[i]); zero for non-rig lights.  NOTE: 'v' is the eye-space POSITION and 'npos' the norma |
| 310–316 (n=7) | calcPointLightOrSpotLightKernel | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 336–344 (n=9) | calcPointLightOrSpotLightKernel | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 349–427 (n=79) | calcPointLightOrSpotLight, calcPointLightOrSpotLightKernel | looks, rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [RigRim] additive rim (cone + attenuation weighted, no shadow); roughness from legacy glossiness (spec.a).  Independent of 'da' so wrap can reach past the terminator.  Mode 0 never enters. Preserve the no-radius evaluator used by existing standalone regression gates. |
| 458–474 (n=17) | getNormal | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 484–500 (n=17) | getSpecular | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 534–540 (n=7) | getShadow | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 543–567 (n=25) | unknown/non-function (float getShadow(vec3 pos, vec3 norm)) | looks, roto-tron, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: WP2 material contract: after Actor FX, before G-buffer packing/forward lighting. Legacy forward glare contributes coverage. Preserve its authored value exactly. |
| 570–657 (n=88) | defined | looks, weather | creative-candidate | unknown; conditional evidence present | Comment evidence: Match PBR and the masked path; single-sided draws retain their cull state. Legacy material textures/G-buffer values are sRGB. Actor FX has one linear-light contract across legacy, PBR, and fullbright surfaces. Emission can vary within a quad: keep derivative evaluation unconditio |
| 683–690 (n=8) | unknown/non-function (void main()) | sensor | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 727–736 (n=10) | unknown/non-function (void main()) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Let a sun glint remain visible on clear blended material. |
| 741–788 (n=48) | unknown/non-function (void main()) | looks, rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [RigRimQ] forward-alpha local pixel (Phase 1: smooth normal + anti-shimmer only).  vary_normal is the unperturbed vertex normal.  The derivative is evaluated unconditionally at main scope (review P2-5; the BLEND permutation has no discards) and is unused unless rig_rim_mode != 0. |
| 794–827 (n=34) | unknown/non-function (void main()) | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: Legacy material BLEND is a forward path and has no emissive GBuffer attachment. Publish the same synthetic Actor FX emission used by the opaque/mask permutations directly into beauty before atmospheric fog. |
| 829–879 (n=51) | unknown/non-function (void main()) | capture-sidecar, looks, roto-tron | creative-candidate | unknown; conditional evidence present | Comment evidence: Reuse effective beauty coverage, including sun/local/environment glare, emissive suppression, Actor FX and vertex alpha. Texture alpha alone leaves background motion on otherwise dominant reflective glass. Same contract as alphaF.glsl: RGB = LINEAR diffuse reflectance of THIS lay |
| 881–914 (n=34) | main | looks, weather | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 6: `indra/newview/app_settings/shaders/class1/alchemy/blitWithEffectsF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 48–60 (n=13) | unknown/non-function (out vec4 frag_color;) | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Comment evidence: [Ultimate Diopter / Cine Fisheye] warped present depth for ReShade. |
| 79–90 (n=12) | main | sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: Transmission was collected by the sensor. Only artistic colour remains. |
| 95–128 (n=34) | main | capture-sidecar | creative-candidate | unknown; conditional evidence present | Comment evidence: Depth must ride through the same lens warps as color, or ReShade's depth-driven effects outline the unwarped scene. Order: fisheye mapping FIRST (present pixel -> fisheye source = diopter output space), then the diopter warp map (diopter output -> scene space). exact forward sour |

## Slice 6: `indra/newview/app_settings/shaders/class1/deferred/tonemapUtilF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 110–128 (n=19) | PBRNeutralToneMapping | looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: tonemap_khronos_params: .x = desaturation, .y = startCompression (peak value where highlight compression begins). Defaults (0.15, 0.76) reproduce the original Khronos sample implementation's hardcoded constants exactly. |
| 131–137 (n=7) | PBRNeutralToneMapping | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 142–149 (n=8) | unknown/non-function (vec3 PBRNeutralToneMapping( vec3 color )) | looks, sensor | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 336–529 (n=194) | applyExposure, decode_slog_family, encode_slog_family, slf_apply_purity, slf_gamut_matrix, slf_inverse_purity, slf_log10, slf_oetf_cineon, slf_oetf_logc3, slf_oetf_slog2, slf_oetf_slog3 | capture-sidecar, looks, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: tonemap_grade: .x = saturation, .y = contrast. Shared post-tonemap grade stage applied to every operator's output. Identity at (1.0, 1.0). AMD FidelityFX LPM (tonemap type 7). Defined in deferred/LPMUtil.glsl, which reads its own tonemap_amd[24] + tonemap_amd_shoulder uniforms. D |
| 556–582 (n=27) | applyToneMap | looks, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: Cine log capture. Return immediately: the shared saturation/contrast grade and the linear<->tonemap mix below would corrupt log codes. Shared saturation/contrast grade, applied to every tonemapper's output. Mathematically an identity when tonemap_grade == (1.0, 1.0): mix(x, y, 1. |

## Slice 6: `indra/newview/app_settings/shaders/class1/environment/waterF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–61 (n=37) | main | capture-sidecar | creative-candidate | unknown; conditional evidence present | Comment evidence: S3: water was previously OUTSIDE the sidecar's modified set, so the attachment kept whatever the seed pass wrote for the surface BEHIND the water -- i.e. it published SEABED ALBEDO at K=1. Confidently wrong is worse than absent.  This renderer models water as specular/reflective  |

## Slice 6: `indra/newview/app_settings/shaders/class3/environment/waterF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 27–46 (n=20) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 354–372 (n=19) | main | capture-sidecar | creative-candidate | unknown; conditional evidence present | Comment evidence: S3: water was previously OUTSIDE the sidecar's modified set, so the attachment kept whatever the seed pass wrote for the surface BEHIND the water -- i.e. it published SEABED ALBEDO at K=1. Confidently wrong is worse than absent.  This renderer models water as specular/reflective  |

## Slice 6: `indra/newview/lldrawpoolalpha.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 58–97 (n=40) | unknown/non-function (public:) | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Comment evidence: [AlphaVel] Alpha pools are deferred to a final velocity stage so dominant translucent surfaces overwrite the opaque/camera fallback. virtual*/ void beginVelocityPass(S32 pass); virtual*/ void endVelocityPass(S32 pass); virtual*/ void renderVelocity(S32 pass = 0); [BDMerge] Per-dr |
| 111–121 (n=11) | unknown/non-function (private:) | capture-sidecar, looks | creative-candidate | unknown; no local conditional evidence | Edits LLDrawInfo |

## Slice 6: `indra/newview/lldrawpoolavatar.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 35–41 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLGLTexture |
| 60–66 (n=7) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 73–80 (n=8) | unknown/non-function (typedef enum) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLMatrix4 |
| 97–110 (n=14) | unknown/non-function (typedef enum) | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge A5.4-1b] classic (system) avatar velocity pass. Donor: Black Dragon lldrawpoolavatar.cpp:500-566 (motion-blur hooks). virtual*/ S32  getNumVelocityPasses(); virtual*/ void beginVelocityPass(S32 pass); virtual*/ void endVelocityPass(S32 pass); virtual*/ void renderVelocit |
| 129–153 (n=25) | unknown/non-function (typedef enum) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Split body meshes by bake coverage; other rendering keeps ALL. The opaque pass left a body mesh for the cut pass in this shadow map. |

## Slice 6: `indra/newview/lldrawpoolbump.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 410–423 (n=14) | LLDrawPoolBump::renderGroup | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 621–656 (n=36) | LLDrawPoolBump::beginVelocityPass, LLDrawPoolBump::endVelocityPass, LLDrawPoolBump::renderVelocity | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge A5.4-1a] Velocity pass (rigid + camera). Donor: Black Dragon lldrawpoolbump.cpp:595-624. Covers bump plus shiny surfaces owned by this pool so metal/reflective geometry also produces motion vectors. Rigged is Phase 1b. [BDMerge A5.4-1b] rigged |
| 963–969 (n=7) | LLBumpImageList::onSourceUpdated | unknown | unknown-review | unknown; no local conditional evidence | Edits LLImageGL |
| 1065–1075 (n=11) | LLRenderPass::pushBumpBatch | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1120–1126 (n=7) | LLRenderPass::pushBumpBatch | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1139–1141 (n=3) | unknown/non-function (void LLRenderPass::pushBumpBatch(LLDrawInfo& params, bool texture, bool batch_te) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 6: `indra/newview/lldrawpoolbump.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 76–87 (n=12) | getNumVelocityPasses | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge A5.4-1a] velocity pass (rigid + camera) |

## Slice 6: `indra/newview/lldrawpoolmaterials.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 117–133 (n=17) | pushMaterialBatchIndexed | looks | creative-candidate | unknown; conditional evidence present | Edits LLRenderPass |
| 180–186 (n=7) | pushMaterialBatchIndexed | looks | creative-candidate | unknown; no local conditional evidence | Edits LLRenderPass |
| 320–329 (n=10) | LLDrawPoolMaterials::renderDeferred | looks | creative-candidate | unknown; conditional evidence present | Edits LLRenderPass |
| 407–413 (n=7) | LLDrawPoolMaterials::renderDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits LLRenderPass |
| 431–531 (n=101) | LLDrawPoolMaterials::beginVelocityPass, LLDrawPoolMaterials::endVelocityPass, LLDrawPoolMaterials::renderVelocity | capture-sidecar | creative-candidate | unknown; conditional evidence present | Comment evidence: ============================================================================ [BDMerge A5.4-1a] Velocity / motion-vector pass (rigid + camera). Donor: Black Dragon lldrawpoolmaterials.cpp:292-348. Opaque/emissive passes use the plain velocity program; alpha-mask passes use the aut |

## Slice 6: `indra/newview/lldrawpoolmaterials.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 70–81 (n=12) | getNumVelocityPasses | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge A5.4-1a] velocity pass (rigid + camera) |

## Slice 6: `indra/newview/lldrawpoolpbropaque.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 29–36 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | looks | creative-candidate | unknown; no local conditional evidence | Edits LLStaticHashedString |
| 126–132 (n=7) | LLDrawPoolGLTFPBR::renderPostDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gPBRGlowProgram |
| 137–143 (n=7) | LLDrawPoolGLTFPBR::renderPostDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits gPBRGlowProgram |
| 160–232 (n=73) | LLDrawPoolGLTFPBR::beginVelocityPass, LLDrawPoolGLTFPBR::endVelocityPass, LLDrawPoolGLTFPBR::renderVelocity | capture-sidecar | creative-candidate | unknown; conditional evidence present | Comment evidence: ============================================================================ [BDMerge A5.4-1a] Velocity / motion-vector pass (rigid + camera). Donor: Black Dragon lldrawpoolpbropaque.cpp:100-140. This pool owns both the opaque and alpha-mask GLTF passes (mRenderType). Opaque uses |

## Slice 6: `indra/newview/lldrawpoolpbropaque.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 41–52 (n=12) | getNumVelocityPasses | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge A5.4-1a] velocity pass (rigid + camera) |

## Slice 6: `indra/newview/lldrawpoolsimple.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 37–44 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | looks | creative-candidate | unknown; no local conditional evidence | Edits LLStaticHashedString |
| 65–71 (n=7) | LLDrawPoolGlow::renderPostDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 78–84 (n=7) | LLDrawPoolGlow::renderPostDeferred | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 194–231 (n=38) | LLDrawPoolFullbright::renderPostDeferred | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Comment evidence: ⛔ DO NOT add LLGLEnable(GL_BLEND) here. It was tried as a fix for S2 and caused a VISIBLE RENDERING REGRESSION: opaque fullbright surfaces whose textures carry an alpha channel started blending, so geometry that was never transparent became see-through. Reported in-world within m |
| 268–397 (n=130) | LLDrawPoolAlphaMask::beginVelocityPass, LLDrawPoolAlphaMask::endVelocityPass, LLDrawPoolAlphaMask::renderVelocity, LLDrawPoolFullbright::beginVelocityPass, LLDrawPoolFullbright::endVelocityPass, LLDrawPoolFullbright::renderVelocity, LLDrawPoolFullbrightAlphaMask::beginVelocityPass, LLDrawPoolFullbrightAlphaMask::endVelocityPass, LLDrawPoolFullbrightAlphaMask::renderVelocity, LLDrawPoolGrass::beginVelocityPass, LLDrawPoolGrass::endVelocityPass, LLDrawPoolGrass::renderVelocity, LLDrawPoolSimple::beginVelocityPass, LLDrawPoolSimple::endVelocityPass, LLDrawPoolSimple::renderVelocity | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Comment evidence: ============================================================================ [BDMerge A5.4-1a/1b] Velocity / motion-vector passes. Donor: Black Dragon lldrawpoolsimple.cpp:160-322. Phase 1a: rigid + camera. Phase 1b: each pool also binds the rigged variant (HAS_SKIN) and pushes t |

## Slice 6: `indra/newview/lldrawpoolsimple.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 47–58 (n=12) | getNumVelocityPasses | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge A5.4-1a] velocity pass (rigid + camera) |
| 71–82 (n=12) | getNumVelocityPasses | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge A5.4-1a] velocity pass (rigid + camera) |
| 95–106 (n=12) | getNumVelocityPasses | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge A5.4-1a] velocity pass (rigid + camera, alpha-mask cutout) |
| 118–129 (n=12) | getNumVelocityPasses | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge A5.4-1a] velocity pass (rigid + camera, alpha-mask cutout) |
| 142–153 (n=12) | getNumVelocityPasses | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge A5.4-1a] velocity pass (rigid + camera) |

## Slice 6: `indra/newview/lldrawpoolterrain.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 56–69 (n=14) | getNumVelocityPasses | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge A5.4-1a] velocity pass (camera only -- terrain is static, so its previous object matrix == current -> zero object velocity, camera velocity still applies) virtual*/ S32  getNumVelocityPasses() override { return 1; } virtual*/ void beginVelocityPass(S32 pass) override; vi |

## Slice 6: `indra/newview/lldrawpooltree.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 104–160 (n=57) | LLDrawPoolTree::beginVelocityPass, LLDrawPoolTree::endVelocityPass, LLDrawPoolTree::renderVelocity | capture-sidecar | creative-candidate | unknown; conditional evidence present | Comment evidence: ============================================================================ [BDMerge A5.4-1a] Velocity / motion-vector pass. Donor: Black Dragon lldrawpooltree.cpp:140-175. Trees are static-position, so previous object matrix == current (region render matrix) and only camera mot |

## Slice 6: `indra/newview/lldrawpooltree.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 55–67 (n=13) | getNumVelocityPasses | capture-sidecar | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge A5.4-1a] velocity pass (camera only -- tree position is static; wind sway is a vertex effect not captured in Phase 1a) virtual*/ S32  getNumVelocityPasses() override { return 1; } virtual*/ void beginVelocityPass(S32 pass) override; virtual*/ void endVelocityPass(S32 pas |

## Slice 7: `indra/newview/altoolalign.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 22–28 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | tracking | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 443–454 (n=12) | ALToolAlign::align | tracking | creative-candidate | unknown; no local conditional evidence | Edits ALProjectorTracker |

## Slice 7: `indra/newview/app_settings/shaders/class1/environment/waterFogF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 35–44 (n=10) | unknown/non-function (vec3 linear_to_srgb(vec3 col);) | environment, shafts-volumetrics | creative-candidate | unknown; no local conditional evidence | Comment evidence: [Cine Haze] Cinematic Depth Atmosphere layer (windlight/cineHazeF.glsl). Returns 'color' untouched when cine_haze_active == 0. |
| 130–138 (n=9) | applySkyAndWaterFog | environment, shafts-volumetrics | creative-candidate | unknown; no local conditional evidence | Comment evidence: [Cine Haze] after the WindLight aerial perspective, above-water only (the deferred haze pass does the same for opaque / water-surface pixels) |
| 140–146 (n=7) | applySkyAndWaterFog | shafts-volumetrics | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 7: `indra/newview/app_settings/shaders/class3/deferred/hazeF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 45–59 (n=15) | unknown/non-function (uniform vec4 waterPlane;) | environment, shafts-volumetrics | creative-candidate | unknown; no local conditional evidence | Comment evidence: [Cine Haze] Cinematic Depth Atmosphere layer (windlight/cineHazeF.glsl). Uniform-gated: with cine_haze_active == 0 this pass is byte-identical to before. |
| 88–99 (n=12) | main | shafts-volumetrics | creative-candidate | unknown; conditional evidence present | Comment evidence: developer view: show the sky exclusion instead of discarding |
| 104–133 (n=30) | main | environment, shafts-volumetrics | creative-candidate | unknown; conditional evidence present | Comment evidence: [Cine Haze] compose the artist layer on top of the WindLight aerial perspective inside the same blend (see cineHazeDeferred for the algebra). alpha 0 => dst * 0 + debug colour: replaces the pixel |

## Slice 7: `indra/newview/app_settings/shaders/class3/deferred/multiPointLightF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–45 (n=21) | areaShadowVisibility | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [RelightQ] The existing master upload carries shape bit 0 and visibility bit 1. Explicit LOD is required in the divergent per-light loops. |
| 49–74 (n=26) | unknown/non-function (uniform float sun_wash;) | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [RigRim] per-light rim params, parallel to light[] / light_col[] (x gain, y k, z wrap, w back bias).  Zero for every non-rig light.  Uploaded with the same count as MULTI_LIGHT / MULTI_LIGHT_COL from the fullscreen batch. [RigRimQ] per-pixel quality inputs (deferredUtil.glsl; str |
| 99–142 (n=44) | localLightMain | looks, rig, sensor | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 145–169 (n=25) | localLightMain | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [RigRimQ] quality inputs (see spotLightF.glsl); mode 0 never samples. |
| 186–214 (n=29) | localLightMain | rig, sensor | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 225–244 (n=20) | localLightMain | looks, rig, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: In the active kernel, zero per-emitter radius gives the original D. Avoid a second uniform branch for every light in this batch. [RigRim] PBR branch (omni: no cone, no per-light shadow) |
| 249–276 (n=28) | localLightMain | rig, sensor | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 282–293 (n=12) | localLightMain | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 299–345 (n=47) | localLightMain | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [RigRim] legacy branch: evaluated outside the nl > 0 test so wrap can reach just past the terminator.  Own normalize / attenuation because the existing block only computes them for front-facing pixels; roughness from legacy glossiness. [RigRim] additive on top of the batch's own  |
| 353–374 (n=22) | localLightMain, main | rig, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: One runtime-uniform dispatch, with a literal-zero specialization of the original light math. With no visible specular, use the literal-zero kernel as well. This avoids both extra work and compiler rounding changes to diffuse-only pixels. |

## Slice 7: `indra/newview/app_settings/shaders/class3/deferred/pointLightF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–45 (n=21) | areaShadowVisibility | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [RelightQ] The existing master upload carries shape bit 0 and visibility bit 1. Explicit LOD is required in the divergent per-light loops. |
| 49–63 (n=15) | envOnlyLightColor | sensor | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 90–206 (n=117) | localLightMain | looks, rig, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: [RigRim] per-light rim params (x gain, y k, z wrap, w back bias); zero for every non-rig light.  Uploaded next to LIGHT_CENTER/LIGHT_SIZE in the deferred box-light loop.  Globals + helper live in deferredUtil.glsl. [RigRimQ] per-pixel quality inputs (deferredUtil.glsl; struct mus |
| 228–265 (n=38) | localLightMain | looks, rig, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: [RigRim] PBR branch (omni: no cone, no per-light shadow); additive on top of the light's own contribution (mode 0 never enters). |
| 269–285 (n=17) | localLightMain | rig, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: [RigRim] legacy branch; roughness from legacy glossiness.  (nl here is the eps-clamped calcHalfVectors value, so the discard above never fires; the helper recomputes n.l itself for the wrap term.) |
| 292–317 (n=26) | localLightMain | rig, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: [RigRim] fold the rim in BEFORE the all-black discard so rim-only pixels survive (mode 0 never enters; the discard is unchanged). |
| 320–349 (n=30) | localLightMain, main | rig, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: With no visible specular, use the literal-zero kernel as well. This avoids both extra work and compiler rounding changes to diffuse-only pixels. |

## Slice 7: `indra/newview/app_settings/shaders/class3/deferred/spotLightF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–45 (n=21) | areaShadowVisibility | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [RelightQ] The existing master upload carries shape bit 0 and visibility bit 1. Explicit LOD is required in the divergent per-light loops. |
| 72–86 (n=15) | envOnlyLightColor | sensor | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 113–235 (n=123) | localLightMain, relightProjectorImage | looks, rig, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge NSpot] direct per-projector shadow sampling (shadowUtil.glsl) [RigRim] per-projector rim params (x gain, y k, z wrap, w back bias); the globals + helper live in deferredUtil.glsl.  Uploaded by setupSpotLight(); zero for every non-rig projector, so only rig fixtures ever  |
| 252–310 (n=59) | localLightMain | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge NSpot] Sample THIS projector's own shadow map directly via shadowUtil's indexed dispatch (shadowMap4-13 / shadow_matrix[4+idx]), instead of reading a pre-resolved channel of the packed screen-space shadow buffer. The old packed path (shd.b / shd.a) had only 2 free channe |
| 314–374 (n=61) | localLightMain | rig, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: Cookie/cone and the existing projector shadow still gate this light. Match the projector's original per-light specular clamp. |
| 401–435 (n=35) | localLightMain | looks, rig, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: [RigRim] PBR branch: rim weighted by this projector's cookie/cone (getProjectedLightDiffuseColor == colour x gobo x edge fade), distance attenuation and its own shadow.  Sampled OUTSIDE the nl > 0 test so wrap can reach just past the terminator; the 3.25 balance magic is delibera |
| 460–474 (n=15) | localLightMain | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [RigRim] legacy (Blinn-Phong) branch: same weighting as the PBR branch; roughness derived from legacy glossiness (spec.a). |
| 482–497 (n=16) | localLightMain | rig, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: The disabled feature retains the original projector reflection. |
| 514–552 (n=39) | localLightMain | rig, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: [RigRim] additive on top of the projector's own lighting; the classic_mode final_scale below applies to both so the balance is preserved.  Mode 0 never reaches this block (exact no-op). |
| 554–570 (n=17) | main | sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: With no visible specular, use the literal-zero kernel as well. This avoids both extra work and compiler rounding changes to diffuse-only pixels. |

## Slice 7: `indra/newview/llfloatertools.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–31 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | tracking | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 660–666 (n=7) | LLFloaterTools::draw | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |
| 987–995 (n=9) | LLFloaterTools::onOpen | tracking | creative-candidate | unknown; no local conditional evidence | Edits ALProjectorTracker, LLSelectMgr |

## Slice 7: `indra/newview/llmaniprotate.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–31 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | tracking | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 387–399 (n=13) | LLManipRotate::handleMouseDownOnPart | tracking | creative-candidate | unknown; conditional evidence present | Comment evidence: The manipulator was hit against the preview pose. Reacquire it on the restored server pose instead of applying that stale drag anchor. |

## Slice 7: `indra/newview/llmanipscale.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–31 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | tracking | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 343–355 (n=13) | LLManipScale::handleMouseDownOnPart | tracking | creative-candidate | unknown; conditional evidence present | Comment evidence: The manipulator was hit against the preview pose. Reacquire it on the restored server pose instead of applying that stale drag anchor. |

## Slice 7: `indra/newview/llmaniptranslate.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 29–35 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | tracking | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 271–278 (n=8) | LLManipTranslate::restoreGL | unknown | unknown-review | unknown; no local conditional evidence | Edits LLImageGL |
| 322–334 (n=13) | LLManipTranslate::handleMouseDownOnPart | tracking | creative-candidate | unknown; conditional evidence present | Comment evidence: The manipulator was hit against the preview pose. Reacquire it on the restored server pose instead of applying that stale drag anchor. |

## Slice 7: `indra/newview/llpanelobject.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 28–34 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | tracking | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 379–385 (n=7) | LLPanelObject::getState | tracking | creative-candidate | unknown; conditional evidence present | Edits ALProjectorTracker |
| 425–436 (n=12) | LLPanelObject::getState | tracking | creative-candidate | unknown; no local conditional evidence | Edits ALProjectorTrackMath, ALProjectorTracker |
| 484–490 (n=7) | LLPanelObject::getState | unknown | unknown-review | unknown; no local conditional evidence | Edits LLQuaternion |
| 1788–1810 (n=23) | LLPanelObject::sendRotation | tracking | creative-candidate | unknown; conditional evidence present | Comment evidence: Backstop only: normal Build selection already restored the pose. Discard cached controls only if an override actually survived to here. set() preserves focused editor text; force a fresh value as well so a later focus-loss commit cannot replay the discarded preview text. |
| 1862–1884 (n=23) | LLPanelObject::sendScale | tracking | creative-candidate | unknown; conditional evidence present | Comment evidence: Backstop only: normal Build selection already restored the pose. Discard cached controls only if an override actually survived to here. set() preserves focused editor text; force a fresh value as well so a later focus-loss commit cannot replay the discarded preview text. |
| 1916–1938 (n=23) | LLPanelObject::sendPosition | tracking | creative-candidate | unknown; conditional evidence present | Comment evidence: Backstop only: normal Build selection already restored the pose. Discard cached controls only if an override actually survived to here. set() preserves focused editor text; force a fresh value as well so a later focus-loss commit cannot replay the discarded preview text. |
| 2133–2139 (n=7) | LLPanelObject::draw | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |
| 2471–2480 (n=10) | LLPanelObject::onCopyPos | tracking | creative-candidate | unknown; conditional evidence present | Edits LLVector3, ALProjectorTracker, ALProjectorTrackMath |
| 2503–2516 (n=14) | LLPanelObject::onCopyRot | tracking | creative-candidate | unknown; conditional evidence present | Edits ALProjectorTracker, ALProjectorTrackMath |
| 2728–2739 (n=12) | LLPanelObject::onPosSelect | tracking | creative-candidate | unknown; conditional evidence present | Edits ALProjectorTracker, LLVector3, ALProjectorTrackMath |
| 2780–2791 (n=12) | LLPanelObject::onRotSelect | tracking | creative-candidate | unknown; conditional evidence present | Edits ALProjectorTracker, LLQuaternion, ALProjectorTrackMath |

## Slice 7: `indra/newview/llpanelvolume.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–37 (n=13) | unknown/non-function (declaration, XML, build list or global scope) | tracking, vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 86–92 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 155–165 (n=11) | LLPanelVolume::postBuild, setVCamTarget | vcam | creative-candidate | unknown; no local conditional evidence | Edits LLTextureCtrl, LLUUID |
| 424–431 (n=8) | LLPanelVolume::getState | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 777–783 (n=7) | LLPanelVolume::draw | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |
| 1043–1060 (n=18) | LLPanelVolume::applyLightSwatchColor, LLPanelVolume::onLightSelectColor | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Leaves an open picker where it is, so a drag through colors the wire cannot tell apart stays smooth. |
| 1069–1186 (n=118) | LLPanelVolume::onVCamProjector, LLPanelVolume::refreshVCamProjectorLabel, LLPanelVolume::snapshotVCamProjector | vcam | creative-candidate | unknown; conditional evidence present | Edits LLPanelVolume, LLUUID, LLPrismLens, LLViewerObject, gObjectList, ALVCamPageModel, LLSD, LLNotificationsUtil |
| 1357–1363 (n=7) | LLPanelVolume::onCopyLight | unknown | unknown-review | unknown; no local conditional evidence | Edits LLColor3 |
| 1545–1551 (n=7) | LLPanelVolume::onLightSelect | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1749–1755 (n=7) | LLPanelVolume::onCommitLight | unknown | unknown-review | unknown; no local conditional evidence | Edits LLColor4, LLColor3 |
| 1833–1839 (n=7) | LLPanelVolume::onCommitProbe | tracking | creative-candidate | unknown; no local conditional evidence | Edits ALProjectorTracker |

## Slice 7: `indra/newview/llpresetsmanager.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 27–39 (n=13) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge B2] Camera presets: auto-local-copy-on-edit + rename, layered on this file's existing LL preset storage — donor: Black Dragon "Unlimited Camera Presets" (llagentcamera.cpp / llfloaterpreference.cpp), commit 152762d400 (2018-12-03), moved in 4ff6498a7e (2020-07-06). Gated |
| 289–312 (n=24) | LLPresetsManager::cameraSettingChanged | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge B2] Auto-local-copy on editing a system preset: instead of just deselecting the active preset (stock behavior below), persist the live edit straight back into the active preset's file, same as BD's onCameraArray()/onFocusArray() -> onAddCameraPreset(false, ...). Works un |
| 333–339 (n=7) | LLPresetsManager::getGraphicsControlNames | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderShadowAlphaNearCascades |
| 341–358 (n=18) | LLPresetsManager::getGraphicsControlNames | rig | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ProbeOnDemand] |
| 577–583 (n=7) | LLPresetsManager::loadPreset | unknown | unknown-review | unknown; no local conditional evidence | Edits ALScopedPresetMaterializing |
| 591–603 (n=13) | LLPresetsManager::loadPreset | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [AlphaShadowCap] Older graphics presets omit this key: load those as stock, even when the previous preset enabled the cap. Restore on a failed load. |
| 630–639 (n=10) | LLPresetsManager::loadPreset | unknown | unknown-review | unknown; conditional evidence present | Edits gSavedSettings, RenderShadowAlphaNearCascades |
| 721–796 (n=76) | LLPresetsManager::renameCameraPreset | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge B2] True in-place rename for camera presets. Stock Alchemy/LL only offers save-as-new + delete-old (no identity-preserving rename exists for either camera or graphics presets). No-ops (returns false) when BDMergeCameraPresets is off. Donor: Black Dragon camera preset UX  |

## Slice 7: `indra/newview/llreflectionmap.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 291–308 (n=18) | LLReflectionMap::isRelevant | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: Relevance is also queried by culling, neighbors and auxiliary renders, before the normal end-of-frame manager update. Refresh before reading. |

## Slice 7: `indra/newview/llreflectionmap.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 26–32 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | rig | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 111–120 (n=10) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Manager's once-per-frame reach result. Hero/mirror probes are managed separately and retain this default; newly registered probes fail open. |
| 142–151 (n=10) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [ProbeOnDemand] On-demand update schedule record (dirty serials, transaction state, recount state). Owned and written only by LLReflectionMapManager. |

## Slice 7: `indra/newview/llreflectionmapmanager.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 26–40 (n=15) | unknown/non-function (declaration, XML, build list or global scope) | rig, weather | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 112–120 (n=9) | unknown/non-function (public:) | rig | creative-candidate | unknown; no local conditional evidence | Comment evidence: Called at the relevance-query boundary; at most one bounded pass per frame. |
| 135–180 (n=46) | getCinematicLiveProbePinnedLightCount, isCinematicLiveProbeCapture | rig | creative-candidate | unknown; no local conditional evidence | Comment evidence: Designate the one client-only cinematic probe that receives the six-face realtime update slot. Its capture remains environment-only; ignored_light_ids prevents synthetic rig fill/catchlight from being baked back into the probe. [LiveProbeRefresh] Snapshot of the live probe refres |
| 209–240 (n=32) | getCaptureKind, isOnDemandActive, isProbeCentricLightCapture | looks, vcam | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ProbeOnDemand] On-demand scheduling (RenderProbeOnDemand). Pipeline, display and floater code call these, so they are public. True while on-demand updating is active (setting ON and probes enabled). True while an ordinary / default / sliced probe face is being captured with the  |
| 283–344 (n=62) | unknown/non-function (private:) | rig | creative-candidate | unknown; no local conditional evidence | Comment evidence: [LiveProbeRefresh] The shipped six-faces-every-frame realtime capture (body moved verbatim out of update()) and the budgeted variants. ---- [ProbeOnDemand] ------------------------------------------------- Per-frame schedule flush (collect events, hit probes, env / light diff, ba |
| 357–372 (n=16) | unknown/non-function (private:) | weather | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ProbeBeforePresent] Coalesces RenderProbeFlushAfterFace to one kick at the end of update(), after all faces and their filtering/copies. [ProbeManualRate] frame counter for RenderProbeUpdateEveryNFrames (only advanced while N > 1; an ordinary-probe tick is counter % N == 0) [Prob |
| 379–526 (n=148) | unknown/non-function (private:) | rig | creative-candidate | unknown; no local conditional evidence | Comment evidence: The cinematic probe owns the realtime slot while present. It is updated live without enabling the dynamic-content mask, so the followed avatar and particles cannot photograph themselves from inside the probe. [LiveProbeRefresh] Scheduler state for the Balanced / Economy / On chan |
| 530–537 (n=8) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 554–556 (n=3) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 7: `indra/newview/llsettingsvo.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 29–35 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | sensor | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 39–46 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 528–547 (n=20) | LLSettingsVOSky::buildFromPresetFile | environment | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge B13] BD - Local Windlights (donor: BD llsettingsvo.cpp) |
| 566–675 (n=110) | LLSettingsVOSky::buildStudioNeutralDay, LLSettingsVOSky::buildStudioNeutralNight | environment, looks | creative-candidate | unknown; conditional evidence present | Comment evidence: Defined in indra/llinventory/llsettingssky.cpp with external linkage (used the same way by indra/newview/rlvenvironment.cpp); there is no header declaration. "Studio Neutral (Day)" -- flat, grey-balanced, PBR-aware neutral EEP sky meant for machinima capture / color grading, not  |
| 915–973 (n=59) | LLSettingsVOSky::applySpecial | looks, sensor, shafts-volumetrics | creative-candidate | unknown; conditional evidence present | Comment evidence: [EnvIntensity v2] Personal Lighting exposure strip. The effective factors come from ALEnvIntensity::compute() (alenvintensity.cpp) and are uploaded as their OWN uniforms, applied in linear light by atmosphericsFuncs.glsl (calcAtmosphericVarsLinear), reflectionProbeF.glsl and defe |
| 1021–1050 (n=30) | LLSettingsVOSky::illumination | sensor | creative-candidate | unknown; conditional evidence present | Edits LLSettingsVOSky, LLSettingsSky, LLCachedControl, gSavedSettings, RenderHDREnabled, RenderSkyAutoAdjustLegacy, RenderSkySunlightScale, RenderHDRSkySunlightScale, RenderSkyAmbientScale, ALEnvIntensity |
| 1167–1186 (n=20) | LLSettingsVOWater::buildFromPresetFile | environment | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge B13] BD - Local Windlights (donor: BD llsettingsvo.cpp) |
| 1261–1275 (n=15) | LLSettingsVOWater::applySpecial | vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: A Prism camera feed returns its scoped source-region height regardless of whether the eye is above or below water, so plane and fog depth use one value. Preserve the stock main-eye neighboring-region correction. |
| 1299–1333 (n=35) | LLSettingsVOWater::applySpecial | vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: Prism Slice 1 reuses the mirror fragment predicate for the mandatory behind-lens clip. Its plane is agent-space and keeps the non-negative half-space pointing away from the eye. The source cache is built once on auxiliary entry; the scoped renderer swaps the exact main cache and  |
| 1524–1543 (n=20) | LLSettingsVODay::buildFromPresetFile | environment | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge B13] BD - Local Windlights (donor: BD llsettingsvo.cpp) |

## Slice 7: `indra/newview/lltoolgrab.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–31 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | tracking | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 204–258 (n=55) | restoreGrabPick, transportTouchPick | tracking | creative-candidate | unknown; conditional evidence present | Comment evidence: Map a real picked material point to the authoritative edit frame without disturbing a touch-only tracked light. Synthetic requestTouch picks have no object ID/real intersection and keep their supplied surface data unchanged. Menu Touch can outlive the pie pick. No surface is pref |
| 279–285 (n=7) | LLToolGrabBase::handleObjectHit | tracking | creative-candidate | unknown; conditional evidence present | Edits ALProjectorTracker |
| 395–402 (n=8) | LLToolGrabBase::startSpin | tracking | creative-candidate | unknown; conditional evidence present | Edits ALProjectorTracker |
| 458–492 (n=35) | LLToolGrabBase::startGrab | tracking | creative-candidate | unknown; conditional evidence present | Comment evidence: drag from center Keep the existing grab frame for ordinary objects and real drags. Where the grab starts, relative to the center of the root object of the set. JC - This code looks wonky, but I believe it does the right thing. Otherwise, when you grab a linked object set, it "pop |
| 574–580 (n=7) | LLToolGrabBase::handleHoverActive | tracking | creative-candidate | unknown; conditional evidence present | Edits ALProjectorTracker |
| 860–866 (n=7) | LLToolGrabBase::handleHoverNonPhysical | tracking | creative-candidate | unknown; conditional evidence present | Edits ALProjectorTracker |
| 969–976 (n=8) | LLToolGrabBase::handleHoverNonPhysical | unknown | unknown-review | unknown; conditional evidence present | Edits LLPickInfo |
| 982–993 (n=12) | LLToolGrabBase::handleHoverNonPhysical | unknown | unknown-review | unknown; no local conditional evidence | Edits LLVector3 |
| 1120–1126 (n=7) | LLToolGrabBase::onMouseCaptureLost | tracking | creative-candidate | unknown; no local conditional evidence | Edits ALProjectorTracker |
| 1163–1169 (n=7) | LLToolGrabBase::onMouseCaptureLost | tracking | creative-candidate | unknown; no local conditional evidence | Edits ALProjectorTracker |
| 1173–1188 (n=16) | LLToolGrabBase::resetStateWithoutSending | tracking | creative-candidate | unknown; no local conditional evidence | Edits LLToolGrabBase, ALProjectorTracker |
| 1193–1204 (n=12) | LLToolGrabBase::stopGrab | tracking | creative-candidate | unknown; conditional evidence present | Comment evidence: For touch-only tracked lights, repick the current driven surface; the mouse-down pick may now be in a previous frame's position. |
| 1267–1278 (n=12) | send_ObjectGrab_message | unknown | unknown-review | unknown; conditional evidence present | Edits LLViewerObject, LLPickInfo, LLVector3 |
| 1284–1295 (n=12) | send_ObjectGrab_message | unknown | unknown-review | unknown; no local conditional evidence | Edits LLVector3 |
| 1310–1317 (n=8) | send_ObjectDeGrab_message | unknown | unknown-review | unknown; conditional evidence present | Edits LLPickInfo |
| 1322–1333 (n=12) | send_ObjectDeGrab_message | unknown | unknown-review | unknown; no local conditional evidence | Edits LLVector3 |

## Slice 7: `indra/newview/llviewerregion.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 62–68 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | rig | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 650–664 (n=15) | LLViewerRegion::LLViewerRegion | unknown | unknown-review | unknown; conditional evidence present | Edits gSavedSettings, RenderWaterLevelOverride, RenderWaterLevel |
| 856–888 (n=33) | LLViewerRegion::applyVisualWaterHeight, LLViewerRegion::getWaterHeight, LLViewerRegion::sanitizeVisualWaterHeight, LLViewerRegion::setWaterHeight | unknown | unknown-review | unknown; conditional evidence present | Edits LLViewerRegion, gSavedSettings, RenderWaterLevelOverride, RenderWaterLevel |
| 1852–1859 (n=8) | LLViewerRegion::killInvisibleObjects | rig | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ProbeOnDemand] cache culling (the camera turned) is not a scene change |
| 2824–2830 (n=7) | unknown/non-function (void LLViewerRegion::decodeBoundingInfo(LLVOCacheEntry* entry)) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLViewerRegion, LLDataPackerBinaryBuffer, LLUUID |
| 2841–2852 (n=12) | LLViewerRegion::cacheFullUpdate | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [ProbeOnDemand] changed / replaced / revalidated cache entry: the data is authoritative, so a culled-then-re-created object is not "unchanged". |
| 2881–2890 (n=10) | LLViewerRegion::cacheFullUpdate | rig | creative-candidate | unknown; conditional evidence present | Edits ALProbeDirty |
| 4039–4041 (n=3) | unknown/non-function (U32 LLViewerRegion::getWhisperRange() const) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 7: `indra/newview/llviewertexturelist.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 62–68 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | rig | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 293–334 (n=42) | LLViewerTextureList::continueEmergencyPurge, LLViewerTextureList::requestEmergencyPurge, LLViewerTextureList::shutdown, request | unknown | mixed/perf-review | unknown; conditional evidence present | Comment evidence: Hold only the visited image: retaining the snapshot's images would artificially keep every texture referenced during the entire sweep. |
| 952–960 (n=9) | LLViewerTextureList::updateImageDecodePriority | unknown | unknown-review | unknown; conditional evidence present | Edits LLCachedControl, gSavedSettings, RenderVRAMGovernorEnabled, LLViewerTexture |
| 1085–1095 (n=11) | LLViewerTextureList::updateImageDecodePriority | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: The emergency visit must leave an unused texture's lazy-flush wait alone. |
| 1211–1222 (n=12) | LLViewerTextureList::updateImagesCreateTextures | rig | creative-candidate | unknown; conditional evidence present | Comment evidence: [ProbeOnDemand] DS: records a blur stamp (an op) for the probe dirty recorder; a downgrade itself never dirties a probe. |
| 1285–1295 (n=11) | LLViewerTextureList::updateImagesFetchTextures | unknown | unknown-review | unknown; conditional evidence present | Edits LLCachedControl, gSavedSettings, RenderVRAMGovernorEnabled, LLTimer |
| 1303–1328 (n=26) | LLViewerTextureList::updateImagesFetchTextures | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: existing textures agresively to free memory faster. This isn't particularly precise and can overshoot, but it doesn't need to be, just making sure it did a full circle and doesn't get stuck updating at bias = 4 with 4 times the rate permanently. existing textures faster. With the |
| 1347–1364 (n=18) | LLViewerTextureList::updateImagesFetchTextures | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Preserve the legacy time budget, which historically excluded the candidate-copy phase when the governor is disabled. |
| 1368–1397 (n=30) | LLViewerTextureList::updateImagesFetchTextures | unknown | unknown-review | unknown; conditional evidence present | Edits LLViewerTexture, LL_DEBUGS, LL_ENDL |
| 2007–2009 (n=3) | unknown/non-function (bool LLUIImageList::initFromFile()) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 7: `indra/newview/llvograss.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 42–48 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | rig | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 368–376 (n=9) | LLVOGrass::updateLOD | rig | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ProbeOnDemand] LG: a camera-distance blade-count change is LOD churn (the render-stopped toggle above stays an event). |
| 383–390 (n=8) | LLVOGrass::updateLOD | rig | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ProbeOnDemand] LG: see above. |

## Slice 7: `indra/newview/llvotree.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 50–56 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | rig | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 378–386 (n=9) | LLVOTree::idleUpdate | rig | creative-candidate | unknown; no local conditional evidence | Comment evidence: [ProbeOnDemand] LTr: a camera-distance trunk LOD switch is churn; the first build and move/rotate below stay events. |

## Slice 8: `indra/llrender/llglslshader.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 29–36 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 57–63 (n=7) | unknown/non-function (GLuint LLGLSLShader::sCurBoundShader = 0;) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLGLSLShader |
| 95–107 (n=13) | is_sampler_type | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 359–375 (n=17) | LLGLSLShader::dumpStats | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Preserve stage classification until a currently bound program is unbound. |
| 390–396 (n=7) | LLGLSLShader::dumpStats | unknown | unknown-review | unknown; conditional evidence present | Edits LLShaderMgr |
| 422–442 (n=21) | LLGLSLShader::isCompute | unknown | unknown-review | unknown; conditional evidence present | Edits LLGLSLShader |
| 454–469 (n=16) | LLGLSLShader::createShader | weather | creative-candidate | unknown; conditional evidence present | Edits LLShaderMgr, LLShaderFeatures |
| 504–510 (n=7) | LLGLSLShader::createShader | unknown | unknown-review | unknown; conditional evidence present | Edits LLShaderMgr |
| 512–518 (n=7) | LLGLSLShader::createShader | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 523–529 (n=7) | LLGLSLShader::createShader | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 540–559 (n=20) | LLGLSLShader::createShader | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge NSpot diag] silent sampler overflow is catastrophic (samplers past the GPU unit limit read unit 0) - always report near/over budget |
| 784–815 (n=32) | LLGLSLShader::mapUniform | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Multi-material shaders bind several sampler families with a fixed per-slot layout. These names are not reserved uniforms and must not fall through to GL's default sampler value of texture unit zero. |
| 820–854 (n=35) | LLGLSLShader::mapUniform | weather | creative-candidate | unknown; conditional evidence present | Comment evidence: found it found it |
| 876–882 (n=7) | LLGLSLShader::mapUniformTextureChannel | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 936–945 (n=10) | LLGLSLShader::mapUniforms | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 968–975 (n=8) | LLGLSLShader::mapUniforms | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 980–1018 (n=39) | LLGLSLShader::mapUniforms | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1037–1044 (n=8) | LLGLSLShader::mapUniforms | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1072–1088 (n=17) | LLGLSLShader::link | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: GLSL 1.50 does not support explicit fragment-output locations without an extension. Bind the named forward-rendering outputs before linking; on newer GLSL these agree with (and do not override) the layout qualifiers. |
| 1112–1130 (n=19) | LLGLSLShader::bind | weather | creative-candidate | unknown; conditional evidence present | Edits LLVertexBuffer, LLShaderMgr |
| 1158–1164 (n=7) | LLGLSLShader::unbind | unknown | unknown-review | unknown; conditional evidence present | Edits LLVertexBuffer |
| 1170–1175 (n=6) | unknown/non-function (void LLGLSLShader::unbind(void)) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLGLSLShader, LLTexture, LLTexUnit, LL_PROFILE_ZONE_SCOPED_CATEGORY_SHADER |
| 1220–1225 (n=6) | unknown/non-function (S32 LLGLSLShader::bindTexture(S32 uniform, LLRenderTarget* texture, bool depth,) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLGLSLShader, LLRenderTarget, LLTexUnit, LL_PROFILE_ZONE_SCOPED_CATEGORY_SHADER |
| 2113–2120 (n=8) | LLGLSLShader::hash | unknown | unknown-review | unknown; no local conditional evidence | Edits LLShaderMgr |
| 2132–2177 (n=46) | LLGLSLShader::hash | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge ShaderCacheFix] include shader SOURCE CONTENTS in the cache key. Stock hashes only file names/defines/features/GL strings, so editing a .glsl file silently kept serving the stale cached binary (observed: NSpot spot-shadow chain changes in shadowUtil.glsl ignored until th |

## Slice 8: `indra/llrender/llglslshader.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 37–48 (n=12) | unknown/non-function (class LLShaderFeatures) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Multi-material programs declare explicit sampler families such as basecolor0/normalmap0/... or diffuse0/bump0/spec0.  The value is the material-slot stride used to bind those samplers to fixed texture units. It is separate from mIndexedTextureChannels, which injects tex0..texN an |
| 68–81 (n=14) | unknown/non-function (public:) | looks, weather | creative-candidate | unknown; no local conditional evidence | Comment evidence: Native per-draw cinematic Actor FX. The fragment utility transforms the actor's original material result while leaving authored coverage intact. Lightweight shadow-only Actor FX coverage. This deliberately does not link the beauty utility: shadow programs only need Dissolve's cov |
| 146–154 (n=9) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Numeric spelling keeps the GL 4.1/macOS headers usable. |
| 184–190 (n=7) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 205–216 (n=12) | unknown/non-function (public:) | looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: Shared Actor FX replays indexed rigged PBR through a forward-lighting shader, which needs additional shadow/reflection samplers beyond the native GBuffer pass.  Keep its material stride separate so static/native indexed PBR and indexed legacy materials retain their full batch wid |
| 314–321 (n=8) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLTexture, LLTexUnit, LLRenderTarget |

## Slice 8: `indra/newview/app_settings/shaders/class1/deferred/pbrterrainF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–4 (n=4) | unknown/non-function (declaration, XML, build list or global scope) | weather | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 182–194 (n=13) | unknown/non-function (vec3 mikktspace(vec3 vNt, vec3 vT, float sign)) | looks, weather | creative-candidate | unknown; no local conditional evidence | Comment evidence: WP2 material contract: after Actor FX, before G-buffer packing/forward lighting. |
| 435–464 (n=30) | defined, main | weather | creative-candidate | unknown; conditional evidence present | Comment evidence: Evaluate derivatives for the entire quad, then exclude emitting pixels. |

## Slice 8: `indra/newview/app_settings/shaders/class1/deferred/pbrterrainV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 35–41 (n=7) | unknown/non-function (uniform mat3 normal_matrix;) | weather | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 8: `indra/newview/app_settings/shaders/class1/deferred/terrainF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–4 (n=4) | unknown/non-function (declaration, XML, build list or global scope) | weather | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 42–54 (n=13) | unknown/non-function (in vec4 vary_texcoord1;) | looks, weather | creative-candidate | unknown; no local conditional evidence | Comment evidence: WP2 material contract: after Actor FX, before G-buffer packing/forward lighting. |
| 66–81 (n=16) | defined, main | weather | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 8: `indra/newview/app_settings/shaders/class1/deferred/terrainV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 27–33 (n=7) | unknown/non-function (uniform mat3 normal_matrix;) | weather | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 8: `indra/newview/app_settings/shaders/class1/deferred/treeF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 38–44 (n=7) | unknown/non-function (uniform float minimum_alpha;) | weather | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 52–60 (n=9) | main | weather | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 8: `indra/newview/app_settings/shaders/class1/windlight/atmosphericsFuncs.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 42–68 (n=27) | envOnlyIdentity | environment, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: [EnvIntensity] viewer-side Light Intensity EV factors (2^EV), kept separate from the pre-PBR compatibility scales above so those keep their stock semantics. All exactly 1.0 at 0 EV and 1.0 in classic mode. [EnvIntensity v2] sky_amb_scale (sky ambient) replaces sky_gi_scale here ( |
| 151–197 (n=47) | calcAtmosphericFarHazeColor | environment | creative-candidate | unknown; no local conditional evidence | Comment evidence: [Cine Haze] WindLight far-field haze colour: the 'additive' term of calcAtmosphericVars() in the limit rel_pos_len -> infinity (combined_haze -> 0, so its (1 - combined_haze) factor is exactly 1) for a unit eye-space view direction.  This is the colour the WindLight aerial perspe |
| 224–250 (n=27) | calcAtmosphericVarsLinear | environment, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: [EnvIntensity] Light Intensity EV factors act on LINEAR light, which is what sunlit/amblit are at this point on the PBR path. Classic mode keeps them sRGB-encoded and its consumers sum sun and ambient in encoded space before decoding, so no linear factor can be applied correctly  |

## Slice 8: `indra/newview/llenvironment.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 29–35 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 855–864 (n=10) | unknown/non-function (std::string env_selection_to_string(LLEnvironment::EnvSelection_t sel)) | environment | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge B13] BD per-axis cloud scroll locks + local Windlights |
| 937–954 (n=18) | LLEnvironment::getCurrentSky | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Null before the environment is set up (e.g. a floater opened at the login screen). |
| 960–976 (n=17) | LLEnvironment::getCurrentWater | unknown | unknown-review | unknown; conditional evidence present | Edits LLSettingsWater |
| 1552–1559 (n=8) | LLEnvironment::getLightDirection | unknown | unknown-review | unknown; no local conditional evidence | Edits LLSettingsSky |
| 1563–1570 (n=8) | LLEnvironment::getSunDirection | unknown | unknown-review | unknown; no local conditional evidence | Edits LLSettingsSky |
| 1574–1581 (n=8) | LLEnvironment::getMoonDirection | unknown | unknown-review | unknown; no local conditional evidence | Edits LLSettingsSky |
| 1647–1668 (n=22) | LLEnvironment::update | vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: Prism's synchronous uniform transaction infers which forced dirty bits were cleared by shader binds. A frame-wide environment sweep must not run inside that transaction or overwrite both the inference and source maps. |
| 1720–1731 (n=12) | LLEnvironment::updateCloudScroll | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge B13] BD - Lock Cloud Scroll |
| 1781–1798 (n=18) | LLEnvironment::swapShaderUniformState | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: LLShaderUniforms owns vectors, so std::swap moves their backing storage without rebuilding or copying the cached uniform values. |
| 2429–2480 (n=52) | LLEnvironment::createDayCycleFromPreset, LLEnvironment::createSkyFromPreset, LLEnvironment::createWaterFromPreset | environment | creative-candidate | unknown; conditional evidence present | Comment evidence: ====================================================================== [BDMerge B13] BD - Local Windlights (donor: BD llenvironment.cpp) ====================================================================== |
| 3707–3709 (n=3) | unknown/non-function (namespace) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 8: `indra/newview/llenvironment.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 116–130 (n=15) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Movable backing store for the cached sky/water uniform maps. Auxiliary camera renders can swap the main-view maps out, build source-eye maps in their place, and restore the exact main maps without rebuilding them. |
| 134–144 (n=11) | getCurrentDay | unknown | unknown-review | unknown; no local conditional evidence | Edits LLSettingsDay |
| 151–157 (n=7) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 187–197 (n=11) | isCloudScrollXLocked, isCloudScrollYLocked, pauseCloudScrollX, pauseCloudScrollY | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge B13] BD per-axis cloud scroll locks (donor: BD llenvironment.h) |
| 228–244 (n=17) | isLocalPreset, setLocalPreset | environment | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge B13] BD - Local Windlights: load EEP-format ("version"-keyed) preset files saved to disk by BDMergeEnvLibrary::savePreset. [BDMerge B13] BD - Local Windlights: whether the current local environment came from a local disk preset (enables local re-saving). |
| 406–416 (n=11) | unknown/non-function (private:) | environment | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge B13] BD per-axis cloud scroll locks [BDMerge B13] BD - Local Windlights |
| 537–539 (n=3) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 8: `indra/newview/llfloatereditenvironmentbase.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 47–55 (n=9) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge B13] |
| 127–144 (n=18) | LLFloaterEditEnvironmentBase::loadInventoryItem | environment | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge B13] BD - local Windlight presets (donor: BD llfloaterfixedenvironment.cpp loadInventoryItem, lines 318-331): presets stay editable, but copy/trans (and thus Save As) only when a local disk preset is loaded. Gated; off = stock (all true). |

## Slice 8: `indra/newview/llfloaterfixedenvironment.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 55–68 (n=14) | unknown/non-function (declaration, XML, build list or global scope) | environment | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge B13] BD - Windlight Stuff |
| 85–102 (n=18) | bdmerge_env_gate | environment | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge B13] BD - Windlight Stuff [BDMerge B13] gate; off = stock behavior |
| 128–157 (n=30) | LLFloaterFixedEnvironment::postBuild, setClickedCallback, setCommitCallback | environment | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge B13] BD - Windlight Stuff: local preset combo + delete button. Both widgets are visible="false" in the XML; the flyout's SAVELOCAL item comes from menu_save_settings.xml and is hidden here when the gate is off, so stock appearance/behavior is unchanged. BD - the combo re |
| 167–181 (n=15) | LLFloaterFixedEnvironment::onOpen | environment | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge B13] BD - Windlight Stuff: track whether this edit session was entered without an inventory item (local edit) and populate the preset combo from disk + inventory. |
| 216–251 (n=36) | LLFloaterFixedEnvironment::refresh | environment | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge B13] BD - Windlight Stuff: local-preset permission surface (donor: BD llfloaterfixedenvironment.cpp refresh(), lines 217-255). BD - If we enter through an item we will proceed as normal, checking for its permissions to determine what we can and can't do. If we enter thro |
| 393–405 (n=13) | LLFloaterFixedEnvironment::onButtonApply | environment | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge B13] BD - Windlight Stuff: save preset to disk (donor: BD llfloaterfixedenvironment.cpp onButtonApply/onButtonSave, line 1014). Unreachable when the gate is off (menu item hidden). |
| 500–697 (n=198) | LLFloaterFixedEnvironment::loadItem, LLFloaterFixedEnvironment::loadPreset, LLFloaterFixedEnvironment::onButtonDelete, LLFloaterFixedEnvironment::onButtonSaveLocal, LLFloaterFixedEnvironment::onSelectPreset, LLFloaterFixedEnvironment::populatePresetsList, LLSettingsVOBase::getSettingsAsset, confirmDeletePreset | environment | creative-candidate | unknown; conditional evidence present | Comment evidence: [BDMerge B13] BD - Windlight Stuff (donor: BD llfloaterfixedenvironment.cpp lines 986-1158). All entry points are gated: the preset combo, delete button, and SAVELOCAL flyout item are only visible/wired when BDMergeEnvLocalPresets is enabled. ===================================== |
| 741–755 (n=15) | LLFloaterFixedEnvironmentWater::loadWaterSettingFromFile | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge B13] BD import path also understands EEP-format disk presets and flags the result as a local preset (savable back to disk). |
| 835–849 (n=15) | LLFloaterFixedEnvironmentSky::loadSkySettingFromFile | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge B13] BD import path also understands EEP-format disk presets and flags the result as a local preset (savable back to disk). |

## Slice 8: `indra/newview/llfloaterfixedenvironment.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 39–46 (n=8) | unknown/non-function (class LLButton;) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: BD - [BDMerge B13] |
| 75–94 (n=20) | unknown/non-function (protected:) | environment | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge B13] BD - Windlight Stuff (gated by BDMergeEnvLocalPresets; donor: BD llfloaterfixedenvironment.cpp/h). BD replaced the name line editor with a preset combo; here the combo is a separate, hidden-by- default widget so stock layout is untouched when the gate is off. |

## Slice 8: `indra/newview/llsettingsvo.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 32–38 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | sensor | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 89–122 (n=34) | unknown/non-function (private:) | environment, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: Same evaluated source, compatibility scales and linear EV/tint factors for atmospheric uniform uploads and precipitation lighting. Flat, grey-balanced, PBR-aware neutral EEP skies for machinima capture / color grading (not stylistic looks). See llsettingsvo.cpp for the exact star |
| 152–159 (n=8) | unknown/non-function (public:) | environment | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge B13] BD - Local Windlights: EEP-format disk preset |
| 187–194 (n=8) | unknown/non-function (public:) | environment | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge B13] BD - Local Windlights: EEP-format disk preset |

## Slice 8: `indra/newview/llviewercamera.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 53–74 (n=22) | unknown/non-function (public:) | vcam, weather | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 8: `indra/newview/skins/default/xui/en/floater_fixedenvironment.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 17–26 (n=10) | unknown/non-function (declaration, XML, build list or global scope) | environment | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge B13] BD local Windlight presets: title strings used only |
| 59–65 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 69–104 (n=36) | unknown/non-function (declaration, XML, build list or global scope) | environment | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge B13] BD local Windlight presets: hidden by default, |

## Slice 8: `indra/newview/skins/default/xui/en/menu_save_settings.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 15–34 (n=20) | unknown/non-function (declaration, XML, build list or global scope) | environment | creative-candidate | unknown; no local conditional evidence | Comment evidence: [BDMerge B13] local Windlight preset save; hidden at runtime unless |

## Slice 9: `indra/llplugin/llpluginprocessparent.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 44–52 (n=9) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; conditional evidence present | Edits LL_WINDOWS |
| 122–135 (n=14) | LLPluginProcessParent::~LLPluginProcessParent | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Destroy any remaining shared memory regions Pending CPU uploads keep only their mapping, never this parent/process. destroy the shared memory region and remove it from our map |
| 187–202 (n=16) | LLPluginProcessParent::privateFileCloseFence | vcam | creative-candidate | unknown; conditional evidence present | Edits LLPluginFileClose, LLPluginProcessParent, LL_WINDOWS |
| 226–232 (n=7) | LLPluginProcessParent::pollTick | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 343–367 (n=25) | LLPluginProcessParent::idle, observeProcess | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: A fence may have been requested while process creation was pending. Maintenance owns the timeout. Main only issues the non-waiting kill and observes exit; a kill request alone never releases the file. observeProcess checked ownership and running state before this signal. SIGTERM  |
| 1188–1199 (n=12) | LLPluginProcessParent::receiveMessage | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: A retired media owner is already detached. Receive its close ack here, after VLC stop/release, rather than discarding it with the old owner. |
| 1260–1268 (n=9) | LLPluginProcessParent::receiveMessage | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: destroy the shared memory region and remove it from our map |
| 1289–1299 (n=11) | LLPluginProcessParent::addSharedMemory, emplace | unknown | unknown-review | unknown; no local conditional evidence | Edits LLPluginSharedMemory |
| 1313–1319 (n=7) | LLPluginProcessParent::removeSharedMemory | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1328–1349 (n=22) | LLPluginProcessParent::getSharedMemoryAddress, LLPluginProcessParent::getSharedMemorySize, LLPluginProcessParent::retainSharedMemory | unknown | unknown-review | unknown; conditional evidence present | Edits LLPluginProcessParent, LLPluginSharedMemory |

## Slice 9: `indra/llplugin/llpluginprocessparent.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 30–44 (n=15) | unknown/non-function (declaration, XML, build list or global scope) | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 99–106 (n=8) | getLaunchTimeout | vcam | creative-candidate | unknown; no local conditional evidence | Edits LLPluginFileClose |
| 115–123 (n=9) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Register before handing pixels to a worker. Erasure/destruction retires the map's reference; the one pending upload owns the mapping until done. |
| 218–226 (n=9) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLPluginSharedMemory |
| 237–244 (n=8) | unknown/non-function (private:) | vcam | creative-candidate | unknown; no local conditional evidence | Edits LLPluginFileClose |

## Slice 9: `indra/llui/llui.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 77–91 (n=15) | unknown/non-function (enum EDragAndDropType) | vcam | creative-candidate | unknown; no local conditional evidence | Comment evidence: Reserved for viewer-local VCam UUID cargo. Never renumber on re-port. |

## Slice 9: `indra/media_plugins/libvlc/media_plugin_libvlc.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 40–53 (n=14) | unknown/non-function (typedef SSIZE_T ssize_t;) | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 70–84 (n=15) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 86–96 (n=11) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 102–344 (n=243) | MediaPluginLibVLC::privateAdmissionMessage, MediaPluginLibVLC::privateEvents, MediaPluginLibVLC::repeatLog, MediaPluginLibVLC::setLoop, MediaPluginLibVLC::tickPrivateFile, latchWrap, mLibVLCContext, privateNow, publishAdmission | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Message-thread only. A setting transition can subscribe to this same player's retained pixels; decoder callbacks never read tokens. Observe VLC's exact native repeat boundary, without querying LibVLC from vout callbacks or relying on its 250ms TimeChanged clock samples. Only this |
| 403–440 (n=38) | MediaPluginLibVLC::displayFrame | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: The native marker follows decoder/vout drain and precedes seek. Also measures the explicitly labelled duration-less recovery. Switch notification policy without replacing live decoder callbacks or their context. Untagged/disabled players use the original sender. |
| 463–533 (n=71) | MediaPluginLibVLC::reportVideoSize, MediaPluginLibVLC::resetVLC, MediaPluginLibVLC::stopPlayer | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: libvlc 3 stop/release quiesces callbacks before reusing pixels/context. |
| 548–571 (n=24) | MediaPluginLibVLC::setFrameDirty | unknown | unknown-review | unknown; conditional evidence present | Edits LLPluginMessage, LLPLUGIN_MESSAGE_CLASS_MEDIA |
| 651–659 (n=9) | MediaPluginLibVLC::playMedia | unknown | unknown-review | unknown; conditional evidence present | Edits ALMediaFile |
| 666–776 (n=111) | MediaPluginLibVLC::playMedia | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Every replacement must prove video again before gaining audio/frames. verifiedLocalFile's pread leaves offset alone. Recheck the exact descriptor, then rewind before EVERY new fd-backed input. External references were conservatively rejected on the exact pinned file by the viewer |
| 781–798 (n=18) | MediaPluginLibVLC::playMedia | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 812–827 (n=16) | MediaPluginLibVLC::playMedia | unknown | unknown-review | unknown; conditional evidence present | Edits ALMediaFile |
| 855–862 (n=8) | MediaPluginLibVLC::setVolumeVLC | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 881–888 (n=8) | MediaPluginLibVLC::setVolumeVLC | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 940–953 (n=14) | MediaPluginLibVLC::receiveMessage | unknown | unknown-review | unknown; conditional evidence present | Edits ALMediaFile |
| 976–982 (n=7) | MediaPluginLibVLC::receiveMessage | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1061–1147 (n=87) | MediaPluginLibVLC::receiveMessage | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Renewing tracking must work for paused/static players. Only this current player's completed pixels may be tagged. |
| 1149–1155 (n=7) | MediaPluginLibVLC::receiveMessage | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1180–1186 (n=7) | MediaPluginLibVLC::receiveMessage | unknown | unknown-review | unknown; conditional evidence present | Edits ALMediaFile |
| 1206–1212 (n=7) | MediaPluginLibVLC::receiveMessage | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 9: `indra/newview/app_settings/shaders/class3/deferred/reflectionProbeF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 34–57 (n=24) | unknown/non-function (uniform samplerCubeArray   irradianceProbes;) | sensor, vcam | creative-candidate | unknown; no local conditional evidence | Comment evidence: Explicit auxiliary-pass guard. SSR iterationCount==0 is not sufficient: callers still enter the trace path, and hero sampling ignores heroProbeCount. [EnvIntensity] viewer-side Sky/GI intensity (2^AlchemyEnvSkyGIEV). Scales the probe IRRADIANCE sample only; the amblit/probe blend |
| 588–604 (n=17) | tapIrradianceMap | sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: [EnvIntensity] sky_gi_scale multiplies the irradiance term only; the mix weight below stays min(refParams.x, 1) so the ambiance blend is unchanged. |
| 780–792 (n=13) | doProbeSample | sensor, vcam | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 804–813 (n=10) | doProbeSample | vcam | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 898–920 (n=23) | sampleReflectionProbesLegacy | sensor, vcam | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 933–943 (n=11) | sampleReflectionProbesLegacy | vcam | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 953–962 (n=10) | unknown/non-function (void applyGlossEnv(inout vec3 color, vec3 glossenv, vec4 spec, vec3 pos, vec3 no) | looks, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: [AvatarLightResponse] keep scales ONLY the evaluated reflection addend; the mixing weight (1 - envIntensity) on the existing colour and the Fresnel are authored and untouched. keep == 1.0 is bit-exact (x * 1.0). The old name forwards 1.0. |
| 964–973 (n=10) | applyLegacyEnv, applyLegacyEnvKeep | sensor | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 9: `indra/newview/llpanelface.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 29–36 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 95–101 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 445–454 (n=10) | LLPanelFace::postBuild, setVCamTarget | vcam | creative-candidate | unknown; no local conditional evidence | Edits LLTextureCtrl, LLUUID |
| 579–585 (n=7) | LLPanelFace::draw | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |
| 1456–1463 (n=8) | LLPanelFace::updateUI | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 3387–3550 (n=164) | BindingReader, Collector, LLPanelFace::onVCamTexture, LLPanelFace::refreshVCamTextureLabel, LLPanelFace::snapshotVCamFaces, apply | vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: Restore is driven only by the per-face snapshot: an originally unbound face goes back to unbound, never to the picker's id. |

## Slice 9: `indra/newview/llpanelface.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 32–43 (n=12) | unknown/non-function (declaration, XML, build list or global scope) | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 170–178 (n=9) | unknown/non-function (protected:) | vcam | creative-candidate | unknown; no local conditional evidence | Edits LLUUID |
| 283–299 (n=17) | unknown/non-function (protected:) | vcam | creative-candidate | unknown; no local conditional evidence | Edits LLUUID, LLPrismLens |
| 741–743 (n=3) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 9: `indra/newview/llpanelvolume.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 30–36 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 87–95 (n=9) | unknown/non-function (public:) | vcam | creative-candidate | unknown; no local conditional evidence | Edits LLUUID, LLViewerObject |
| 109–117 (n=9) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Gives the light the swatch's color and shows in the swatch the color the light holds, which the wire can carry, rather than the one picked. |
| 157–166 (n=10) | unknown/non-function (protected:) | vcam | creative-candidate | unknown; no local conditional evidence | Edits LLUUID, LLPrismLens |

## Slice 9: `indra/newview/lltexturectrl.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 29–36 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 59–65 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 79–86 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 150–160 (n=11) | unknown/non-function (bool get_can_copy_texture(LLUUID asset_id)) | vcam | creative-candidate | unknown; no local conditional evidence | Edits LLPrismPreviewService |
| 211–218 (n=8) | LLFloaterTexturePicker::~LLFloaterTexturePicker | vcam | creative-candidate | unknown; no local conditional evidence | Edits LLPrismPreviewService |
| 544–579 (n=36) | LLFloaterTexturePicker::onClose, LLFloaterTexturePicker::onOpen | vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: mPickerFinalized/mVCamLiveApplied are per picker session: they are reset in onClose, not here, because openFloater() re-runs onOpen when the swatch is clicked while the picker is already open. |
| 686–711 (n=26) | LLFloaterTexturePicker::postBuild, LLPrismLens::connectConfigurationChanged, LLPrismPreview::connectChanged, setCommitCallback | vcam | creative-candidate | unknown; no local conditional evidence | Edits LLUICtrl, LLSD, LLSliderCtrl, LLPrismLens, LLPrismPreview |
| 744–756 (n=13) | LLFloaterTexturePicker::draw | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 848–902 (n=55) | LLFloaterTexturePicker::draw | vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: "None / Remove VCam feed" reveals the real light cookie. A point light has none, so show an empty frame rather than the texture control's generic white fallback image. |
| 906–916 (n=11) | LLFloaterTexturePicker::draw | vcam | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1110–1119 (n=10) | LLFloaterTexturePicker::commitCallback | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1183–1201 (n=19) | LLFloaterTexturePicker::onBtnCancel | vcam | creative-candidate | unknown; conditional evidence present | Edits LLTextureCtrl, LLUUID |
| 1205–1211 (n=7) | LLFloaterTexturePicker::onBtnSelect | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1431–1605 (n=175) | LLFloaterTexturePicker::onVCamCommit, LLFloaterTexturePicker::onVCamProjectorSettingsCommit, LLFloaterTexturePicker::refreshVCamList, LLFloaterTexturePicker::refreshVCamProjectorControls, LLFloaterTexturePicker::setVCamSelection, LLFloaterTexturePicker::setVCamTarget, LLFloaterTexturePicker::updateVCamDemand | vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: The atlas has already fitted the accepted source aspect into its 16:9 cell. Preserve that cell here (avoid fitting it twice). |
| 1758–1767 (n=10) | LLFloaterTexturePicker::changeMode | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1821–1828 (n=8) | LLFloaterTexturePicker::changeMode | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1929–1935 (n=7) | LLFloaterTexturePicker::setLocalTextureEnabled | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1937–1970 (n=34) | LLFloaterTexturePicker::setBakeTextureEnabled, LLFloaterTexturePicker::setStandardSourcesEnabled, LLFloaterTexturePicker::setVCamEnabled | vcam | creative-candidate | unknown; conditional evidence present | Edits LLFloaterTexturePicker |
| 2250–2262 (n=13) | LLTextureCtrl::setEnabled | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 2264–2270 (n=7) | LLTextureCtrl::setEnabled | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 2272–2280 (n=9) | LLTextureCtrl::setEnabled | unknown | unknown-review | unknown; no local conditional evidence | Edits LLView |
| 2316–2327 (n=12) | LLTextureCtrl::showPicker | vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: Latch the VCam original once per picker session: clicking the swatch again while the picker is open must not re-snapshot live-applied state. |
| 2330–2343 (n=14) | LLTextureCtrl::showPicker | vcam | creative-candidate | unknown; conditional evidence present | Edits LLFloaterTexturePicker, LLUUID |
| 2374–2386 (n=13) | LLTextureCtrl::showPicker | vcam | creative-candidate | unknown; no local conditional evidence | Edits LLUUID |
| 2495–2508 (n=14) | LLTextureCtrl::onFloaterCommit | vcam | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 2532–2539 (n=8) | LLTextureCtrl::onFloaterCommit | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 2586–2623 (n=38) | LLTextureCtrl::setVCamOverride, LLTextureCtrl::setVCamTarget | vcam | creative-candidate | unknown; conditional evidence present | Edits LLTextureCtrl, LLFloaterTexturePicker, LLUUID, LLStringExplicit |
| 2699–2707 (n=9) | LLTextureCtrl::handleDragAndDrop | vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: mStandardEnabled, not getEnabled(): the swatch stays enabled for the viewer-local VCam source even when the slot itself isn't editable; inventory drops must still be refused then. |
| 2835–2851 (n=17) | LLTextureCtrl::draw | vcam | creative-candidate | unknown; conditional evidence present | Edits LLRect, LLColor4, LLFontGL |
| 2988–2990 (n=3) | unknown/non-function (namespace LLInitParam) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 9: `indra/newview/lltexturectrl.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 36–42 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 49–55 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLSliderCtrl |
| 90–106 (n=17) | unknown/non-function (enum LLPickerSource) | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 198–211 (n=14) | unknown/non-function (public:) | vcam | creative-candidate | unknown; no local conditional evidence | Edits LLUUID |
| 306–318 (n=13) | unknown/non-function (private:) | vcam | creative-candidate | unknown; no local conditional evidence | Edits LLUUID |
| 416–434 (n=19) | unknown/non-function (public:) | vcam | creative-candidate | unknown; no local conditional evidence | Edits LLUUID, LLUICtrl |
| 479–487 (n=9) | unknown/non-function (protected:) | vcam | creative-candidate | unknown; no local conditional evidence | Edits LLUICtrl, LLSliderCtrl |
| 509–528 (n=20) | unknown/non-function (private:) | vcam | creative-candidate | unknown; no local conditional evidence | Edits LLUUID, LLPrismPreviewService |

## Slice 9: `indra/newview/lltoolbarview.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 884–892 (n=9) | LLToolBarView::handleDropTool | vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: DAD_PERSON means that cargo_data contains an uuid, not an LLInventoryObject These cargo types carry a UUID, not an LLInventoryObject. |

## Slice 9: `indra/newview/lltooldraganddrop.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 26–35 (n=10) | unknown/non-function (declaration, XML, build list or global scope) | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 62–68 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 78–83 (n=6) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; conditional evidence present | Edits LLNoPreferredType, LLInventoryCollectFunctor, LLInventoryCategory, LLInventoryItem, LLFolderType, LLNoPreferredTypeOrItem |
| 143–148 (n=6) | unknown/non-function (bool LLDropCopyableItems::operator()() | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Starts a fetch on folders and items.  This is really not used as an observer in the traditional sense; we're just using it to request a fetch and we don't care about when/if the response arrives. no-op: it's fire n forget right? */ |
| 235–243 (n=9) | LLToolDragAndDrop::LLDragAndDropDictionary::LLDragAndDropDictionary | vcam | creative-candidate | unknown; no local conditional evidence | Comment evidence: DAD_VCAM_FEED (22) is viewer-local UUID cargo. Only world object faces accept it; it is never interpreted as inventory. |
| 277–293 (n=17) | LLToolDragAndDrop::beginDrag | vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: VCam cargo is a registry UUID, never an inventory UUID. In particular, do not let a coincidentally matching inventory item trigger a fetch. |
| 295–305 (n=11) | LLToolDragAndDrop::beginDrag | unknown | unknown-review | unknown; conditional evidence present | Edits gInventory, LL_PROFILE_ZONE_NAMED_CATEGORY_UI, LLInventoryModelBackgroundFetch |
| 312–321 (n=10) | LLToolDragAndDrop::beginDrag | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: go ahead and fire & forget the descendents if we are not dragging a protected folder. The root category is queued above (and folder-view sources already did so in LLInvFVBridge::startDrag()). Enumerating the entire subtree here only built lists for a fire-and-forget fetch and was |
| 324–331 (n=8) | LLToolDragAndDrop::beginMultiDrag | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |
| 346–359 (n=14) | LLToolDragAndDrop::beginMultiDrag, LLToolDragAndDrop::endDrag | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: find categories (i.e. inventory folders) in the cargo. The removed subtree walk was consumed solely by a fire-and-forget observer which was never started, never by the drag cargo itself. Preserve that original no-fetch behavior here. |
| 369–384 (n=16) | LLToolDragAndDrop::handleMouseUp, LLToolDragAndDrop::onMouseCaptureLost | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: The drop starts with a drop=false acceptance pass. It must recompute state that is not represented by the inventory revision (wear state, Marketplace state, RLV locks, settings, and the active panel). |
| 460–466 (n=7) | LLToolDragAndDrop::handleHover | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |
| 509–515 (n=7) | LLToolDragAndDrop::dragOrDrop | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |
| 526–544 (n=19) | LLToolDragAndDrop::dragOrDrop | vcam | creative-candidate | unknown; no local conditional evidence | Comment evidence: For people drag and drop we don't need an actual inventory object, instead we need the current cargo id, which should be a person id. People and VCam drags carry UUIDs rather than inventory objects. VCam is kept as an explicit source/type pair so no other viewer cargo can inherit |
| 554–562 (n=9) | LLToolDragAndDrop::dragOrDrop | vcam | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 570–581 (n=12) | LLToolDragAndDrop::dragOrDrop | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_NAMED_CATEGORY_UI |
| 599–611 (n=13) | LLToolDragAndDrop::dragOrDrop | vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: Only VCam cargo gets the real drop pass; people cargo keeps its old behaviour. |
| 621–633 (n=13) | LLToolDragAndDrop::dragOrDrop | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_NAMED_CATEGORY_UI |
| 643–651 (n=9) | LLToolDragAndDrop::dragOrDrop | vcam | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 659–669 (n=11) | LLToolDragAndDrop::dragOrDrop | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_NAMED_CATEGORY_UI |
| 685–693 (n=9) | LLToolDragAndDrop::dragOrDrop | vcam | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 727–743 (n=17) | LLToolDragAndDrop::dragOrDrop3D | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI, LL_PROFILE_ZONE_NAMED_CATEGORY_UI |
| 767–792 (n=26) | LLToolDragAndDrop::pick | vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: VCam bindings are viewer-local face identities. Preserve a picked attachment object so its static, non-rigged face can pass through the same geometry validator as any other visible object face. |
| 1797–1807 (n=11) | LLToolDragAndDrop::dropObject | unknown | unknown-review | unknown; conditional evidence present | Edits LLViewerObject, gObjectList |
| 2316–2419 (n=104) | LLToolDragAndDrop::dad3dVCamAvatar, LLToolDragAndDrop::dad3dVCamFeed, LLToolDragAndDrop::dad3dVCamLand, LLToolDragAndDrop::dad3dVCamNoTarget | vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: Real projectors take the projector path. A plain (point) light only does with Shift held, so a glowing TV/screen prim still accepts a normal face/display drop. |
| 3470–3477 (n=8) | LLToolDragAndDrop::locateInventory | vcam | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 9: `indra/newview/lltooldraganddrop.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 65–71 (n=7) | unknown/non-function (public:) | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 173–183 (n=11) | unknown/non-function (protected:) | vcam | creative-candidate | unknown; no local conditional evidence | Edits LLViewerObject |

## Slice 9: `indra/newview/llviewermedia.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 27–35 (n=9) | unknown/non-function (declaration, XML, build list or global scope) | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 686–692 (n=7) | LLViewerMedia::updateMedia | vcam | creative-candidate | unknown; no local conditional evidence | Edits LLPrismLens |
| 700–707 (n=8) | LLViewerMedia::updateMedia | vcam | creative-candidate | unknown; conditional evidence present | Edits LLPrismLens |
| 775–781 (n=7) | LLViewerMedia::updateMedia | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 811–818 (n=8) | LLViewerMedia::updateMedia | vcam | creative-candidate | unknown; conditional evidence present | Edits LLPrismLens |
| 860–866 (n=7) | LLViewerMedia::updateMedia | unknown | unknown-review | unknown; conditional evidence present | Edits gViewerWindow, ALMediaFile |
| 871–877 (n=7) | LLViewerMedia::updateMedia | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 981–987 (n=7) | LLViewerMedia::setAllMediaEnabled | vcam | creative-candidate | unknown; conditional evidence present | Edits LLPrismLens |
| 1037–1043 (n=7) | LLViewerMedia::setAllMediaPaused | vcam | creative-candidate | unknown; conditional evidence present | Edits LLPrismLens |
| 1714–1723 (n=10) | LLViewerMediaImpl::LLViewerMediaImpl | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [WebIdle] grace period before an undrawn UI page is paused |
| 1777–1783 (n=7) | LLViewerMediaImpl::createMediaSource | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1802–1817 (n=16) | LLViewerMediaImpl::destroyMediaSource | vcam | creative-candidate | unknown; conditional evidence present | Edits LLPrismLens |
| 1838–1853 (n=16) | LLViewerMediaImpl::destroyMediaSource | vcam | creative-candidate | unknown; conditional evidence present | Edits ALMediaFile, LLPrismLens |
| 1858–1872 (n=15) | LLViewerMediaImpl::newSourceFromMediaType | unknown | unknown-review | unknown; conditional evidence present | Edits LLPluginClassMedia, LLViewerMediaImpl, LLPluginClassMediaOwner, LLMIMETypes |
| 1883–1890 (n=8) | LLViewerMediaImpl::newSourceFromMediaType | vcam | creative-candidate | unknown; conditional evidence present | Edits LLPrismLens, LLViewerMediaImpl |
| 1993–2000 (n=8) | LLViewerMediaImpl::newSourceFromMediaType | vcam | creative-candidate | unknown; conditional evidence present | Edits LLPrismLens, LLViewerMediaImpl |
| 2050–2060 (n=11) | LLViewerMediaImpl::initializePlugin | unknown | unknown-review | unknown; conditional evidence present | Edits LLPluginClassMedia |
| 2096–2110 (n=15) | LLViewerMediaImpl::loadURI | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 2117–2128 (n=12) | LLViewerMediaImpl::loadURI | vcam | creative-candidate | unknown; conditional evidence present | Edits LLPrismLens |
| 2184–2190 (n=7) | LLViewerMediaImpl::play | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 2205–2211 (n=7) | LLViewerMediaImpl::stop | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 2216–2222 (n=7) | LLViewerMediaImpl::pause | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 2230–2236 (n=7) | LLViewerMediaImpl::start | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 2297–2310 (n=14) | LLViewerMediaImpl::setPrismGateGain | vcam | creative-candidate | unknown; conditional evidence present | Edits LLViewerMediaImpl |
| 2325–2335 (n=11) | LLViewerMediaImpl::updateVolume | vcam | creative-candidate | unknown; conditional evidence present | Edits LLViewerMedia |
| 2772–2788 (n=17) | LLViewerMediaImpl::navigateInternal | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 3069–3326 (n=258) | LLViewerMediaImpl::completedMediaImage, LLViewerMediaImpl::cuePrivateFile, LLViewerMediaImpl::doMediaTexUpdate, LLViewerMediaImpl::finishMediaTexUpdate, LLViewerMediaImpl::mediaImageIdentity, LLViewerMediaImpl::mediaSlotCandidate, LLViewerMediaImpl::mediaSlotPermitted, LLViewerMediaImpl::preMediaTexUpdate, LLViewerMediaImpl::privateTransport, LLViewerMediaImpl::retirePrivateFile, LLViewerMediaImpl::update, MediaUpload, ~MediaUpload | vcam | creative-candidate | unknown; conditional evidence present | Comment evidence: The custom deleter below only destroys uploads on main. In particular, LLRefCount and LLImageGL's deferred-delete list are not worker-safe. Observing a source never overrides its ordinary visibility/update policy. A MIME mapping is provisional, never proof about a running plugin. |
| 3398–3407 (n=10) | LLViewerMediaImpl::update | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: A tracked CPU job may outlive disable and player replacement. It still owns the texture until cleanup; accelerated imports must not race it. |
| 3415–3428 (n=14) | LLViewerMediaImpl::update | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [WebIdle] a paused UI page does no texture work (the dirty flag is kept, so the first update after resume uploads the newest frame) On resume mWebRefreshPending forces one re-blit of the plugin's current shared texture even if no new frame is flagged dirty (viewer-side refresh). |
| 3433–3448 (n=16) | LLViewerMediaImpl::update | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: A global setting change can precede the current player's first tagged frame. Untagged players must continue consuming dirties through legacy. |
| 3485–3518 (n=34) | LLViewerMediaImpl::update | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: postTo posts its reply before destroying this callable. Drop the worker's copy first so only the main reply owns cleanup. |
| 3594–3600 (n=7) | LLViewerMediaImpl::doMediaTexUpdate | vcam | creative-candidate | unknown; conditional evidence present | Edits LLMediaFrame |
| 3791–3927 (n=137) | LLViewerMediaImpl::noteUIDrawn, LLViewerMediaImpl::updateMediaImageForSlots, LLViewerMediaImpl::updateWebPause | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: if (!mMediaSource) [SL:KB] - Patch: Render-TextureToggle (Catznip-4.0) [/SL:KB] llassert(!mTextureId.isNull()); *TODO: Consider enabling mipmaps (they have been disabled for a long time). Likely has a significant performance impact for tiled/high texture repeat media. Mip generat |
| 4037–4043 (n=7) | LLViewerMediaImpl::setDisabled | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 4050–4056 (n=7) | LLViewerMediaImpl::isForcedUnloaded | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 4528–4536 (n=9) | LLViewerMediaImpl::calculateInterest | vcam | creative-candidate | unknown; conditional evidence present | Edits LLPrismLens |
| 4676–4687 (n=12) | LLViewerMediaImpl::setPriority | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Ordinary admission made this decision; recovery requires a new Play. |
| 4821–4827 (n=7) | LLViewerMediaImpl::isAutoPlayable | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 4831–4838 (n=8) | LLViewerMediaImpl::shouldShowBasedOnClass | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Explicit private file playback has its own source/admission policy. |
| 4873–4881 (n=9) | LLViewerMediaImpl::isObscured | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: A private file has no prim or parcel classification. Global media policy and its explicit slot Play remain authoritative. |

## Slice 9: `indra/newview/llviewermedia.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 36–42 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 233–240 (n=8) | prismGateGain | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 281–323 (n=43) | hasLibVLCSource, isPrivateFile, privateBudgetLost, privateFileActive, privateFileAdmitted, privateFileEnded, privateFileFailed, privateFileRetaining, resolvedPlugin | vcam | creative-candidate | unknown; no local conditional evidence | Comment evidence: [WebIdle] re-evaluate the UI-web pause state; called from update(). |
| 350–356 (n=7) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLPluginClassMedia, LLPluginClassMediaOwner, LLStringUtil |
| 430–443 (n=14) | isWebPaused | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [WebIdle] UI web pages (LLMediaCtrl-owned impls only) that have not been drawn for a second are paused: no texture uploads, plugin priority HIDDEN (viewer side only; CEF itself is not told). LLMediaCtrl::draw() calls noteUIDrawn() every frame it is drawn, which resumes instantly. |
| 498–508 (n=11) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 524–547 (n=24) | unknown/non-function (private:) | vcam | creative-candidate | unknown; no local conditional evidence | Comment evidence: [WebIdle] see noteUIDrawn(). mUILastDrawTime is LLTimer::getElapsedSeconds() of the last LLMediaCtrl::draw() (initialised to impl creation time so a never-drawn, preloaded page gets a one second grace period). Viewer frame of the last draw: pausing also requires missed FRAMES, so |
| 560–566 (n=7) | unknown/non-function (private:) | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 595–601 (n=7) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLViewerMediaTexture |

## Slice 9: `indra/newview/skins/default/xui/en/floater_texture_ctrl.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 30–36 (n=7) | unknown/non-function (PICK: MATERIAL) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: xui-check: overlay preview_widget Multiple --> |
| 77–86 (n=10) | unknown/non-function (PICK: TEXTURE) | vcam | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 88–100 (n=13) | unknown/non-function (PICK: TEXTURE) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 273–322 (n=50) | unknown/non-function (PICK: TEXTURE) | vcam | creative-candidate | unknown; no local conditional evidence | Comment evidence: middle: VCam mode --> |

## Slice 10: `indra/newview/app_settings/shaders/class1/alchemy/colorCorrectF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 34–42 (n=9) | unknown/non-function (out vec4 frag_color;) | looks, sensor | creative-candidate | unknown; conditional evidence present | Edits AL_SENSOR_OPTICS, AL_SENSOR_GODRAYS |
| 58–64 (n=7) | unknown/non-function (uniform vec3  halation_tint;) | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 66–83 (n=18) | unknown/non-function (vec3 applyExposure(vec3 color);) | looks, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: Shared with tonemapUtilF. 8 == S-Log3 capture: a raw, ungraded camera-style transfer, so the color-grade stages below are bypassed for it. Declared unconditionally (a COLOR_GRADE-only variant has no TONEMAP define); when the pass has no tonemap the CPU leaves this at its default  |
| 95–102 (n=8) | unknown/non-function (vec3 applyChannelCurves(vec3 diff);) | looks | creative-candidate | unknown; no local conditional evidence | Comment evidence: Note: Graduated ND + Polarizer now run in a dedicated pre-pass (LLPipeline::applyOnLensFilters) ahead of bloom/flare, not here. |
| 116–291 (n=176) | cameraTransfer, defined, main | looks, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: White balance and lift/gamma/gain run in linear light, between exposure and tonemap, so chromatic adaptation and the three-way grade act on physically meaningful HDR values rather than rolled-off display ones. Bypassed for S-Log3 capture: grade neutrally, correct in post. Split t |
| 295–313 (n=19) | main | looks, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: White balance and lift/gamma/gain run in linear light, between exposure and tonemap, so chromatic adaptation and the three-way grade act on physically meaningful HDR values rather than rolled-off display ones. Split toning after tonemap so tints apply to rolled-off values. === DI |

## Slice 10: `indra/newview/app_settings/shaders/class1/avatar/avatarV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 24–37 (n=14) | unknown/non-function (declaration, XML, build list or global scope) | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 64–75 (n=12) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 79–81 (n=3) | unknown/non-function (void main()) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/avatar/eyeballV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 27–40 (n=14) | unknown/non-function (uniform mat3 normal_matrix;) | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 55–66 (n=12) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 68–70 (n=3) | unknown/non-function (void main()) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/SMAA.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1372–1380 (n=9) | SMAANeighborhoodBlendingPS | sensor | creative-candidate | unknown; conditional evidence present | Edits AL_SENSOR_HDR |
| 1383–1391 (n=9) | SMAANeighborhoodBlendingPS | sensor | creative-candidate | unknown; conditional evidence present | Edits AL_SENSOR_HDR |
| 1403–1417 (n=15) | SMAANeighborhoodBlendingPS | sensor | creative-candidate | unknown; conditional evidence present | Edits AL_SENSOR_HDR |
| 1423–1431 (n=9) | SMAANeighborhoodBlendingPS | sensor | creative-candidate | unknown; conditional evidence present | Edits AL_SENSOR_HDR |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/alphaV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 32–43 (n=12) | unknown/non-function (uniform mat4 texture_matrix0;) | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 73–81 (n=9) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 143–145 (n=3) | unknown/non-function (void main()) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/avatarAlphaMaskShadowF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 28–50 (n=23) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/avatarAlphaShadowF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 29–61 (n=33) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/avatarAlphaShadowV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–30 (n=6) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 33–44 (n=12) | main | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 56–64 (n=9) | main | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/avatarEyesV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 27–38 (n=12) | unknown/non-function (uniform mat3 normal_matrix;) | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 44–52 (n=9) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/avatarShadowF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 26–40 (n=15) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/avatarShadowV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 30–40 (n=11) | main | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 54–56 (n=3) | unknown/non-function (void main()) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/avatarV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 24–35 (n=12) | unknown/non-function (declaration, XML, build list or global scope) | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 55–63 (n=9) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 143–145 (n=3) | unknown/non-function (void main()) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/bumpV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 27–38 (n=12) | unknown/non-function (uniform mat4 modelview_matrix;) | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 52–60 (n=9) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/cofF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 43–98 (n=56) | lensSoftEdgeMask | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: [LensLooks] Soft-Edge "Defocus" blur type: widen the circle of confusion outside the sweet spot so DoF renders real bokeh there (doc/LENS_LOOKS_CONTRACT.md, stage S4).  dof_softedge.x == 0 (the GL default) is the bit-exact off path: the branch in main() is untaken and the origina |
| 124–144 (n=21) | main | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: [LensLooks] Defocus: outside the sweet spot, push the CoF toward the far-blur edge value (never shrinking an existing larger blur of either sign).  Off (dof_softedge.x == 0) leaves sc exactly as computed above. Far blur is NEGATIVE here (see the dof_softedge comment), which is th |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/diffuseNoColorV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 24–54 (n=31) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/diffuseV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 26–37 (n=12) | unknown/non-function (declaration, XML, build list or global scope) | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 54–62 (n=9) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/dofCombineF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 48–141 (n=94) | main, sensorDiskCount, sensorFocused, sensorReflect | sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: GLSL integer remainder is undefined for negative operands. A full-resolution focused receiver is opaque to background scatter. Only front-of-focus apertures contribute, with their own coverage and transmittance. Do not infer coverage from a mixed reduced depth cell. Integrate the |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/emissiveIndexedV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 37–63 (n=27) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 70–74 (n=5) | main | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/emissiveV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 26–40 (n=15) | unknown/non-function (declaration, XML, build list or global scope) | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 43–49 (n=7) | unknown/non-function (vec3 atmosAmbient();) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 53–61 (n=9) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 75–81 (n=7) | main | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/fullbrightShinyV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 28–34 (n=7) | unknown/non-function (uniform mat4 texture_matrix0;) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 38–46 (n=9) | unknown/non-function (uniform vec4 origin;) | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 58–66 (n=9) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/fullbrightV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 26–38 (n=13) | unknown/non-function (declaration, XML, build list or global scope) | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 54–62 (n=9) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/gbufferUtil.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 32–38 (n=7) | unknown/non-function (uniform sampler2D emissiveRect;) | sensor | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 44–50 (n=7) | getGBuffer | sensor | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/luminanceF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 37–55 (n=19) | unknown/non-function (uniform sampler2D emissiveRect;) | sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: [Night Mask B1] emissiveRect (bloomMip[0]) here is LAST frame's bloom, which generateBloomHDR() builds from the (possibly masked) scene AFTER this same luminance pass runs each frame. When Night Mask is active, that bloom was generated from the darkened scene, so folding it into  |
| 73–79 (n=7) | main | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/materialIndexedV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 31–37 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 42–50 (n=9) | unknown/non-function (uniform mat3 normal_matrix;) | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 73–81 (n=9) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/materialShadowIndexedF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 32–46 (n=15) | unknown/non-function (out vec4 frag_color;) | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 97–106 (n=10) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/materialShadowIndexedV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 35–40 (n=6) | unknown/non-function (mat4 getObjectSkinnedTransform();) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 43–55 (n=13) | main | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 60–65 (n=6) | main | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/materialV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 31–37 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 44–52 (n=9) | unknown/non-function (out vec3 vary_position;) | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 73–95 (n=23) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 146–162 (n=17) | main | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Rasterize with the identical beauty position above; motion uses the unjittered endpoints and the same history as the other velocity pools. |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/pbrShadowAlphaBlendF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 28–79 (n=52) | main | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: [ShadowDist P3] shadow-pass alpha policy (design v5 s2): this program is always the blended pass and has no minimum_alpha discard, so mode 1 applies the blended cutoff directly with no column dither. Mode 0 = stock branch verbatim. |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/pbrShadowAlphaMaskF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 28–40 (n=13) | unknown/non-function (out vec4 frag_color;) | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 44–53 (n=10) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/pbrShadowAlphaMaskIndexedF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 30–44 (n=15) | unknown/non-function (out vec4 frag_color;) | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 97–106 (n=10) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/pbrShadowAlphaMaskIndexedV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 36–41 (n=6) | unknown/non-function (mat4 getObjectSkinnedTransform();) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 47–59 (n=13) | main | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 65–70 (n=6) | main | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/pbrShadowAlphaMaskV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 35–55 (n=21) | main | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 62–67 (n=6) | main | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/pbralphaV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–31 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 53–61 (n=9) | unknown/non-function (uniform vec4[2] texture_emissive_transform;) | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 78–86 (n=9) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/pbrglowIndexedV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 35–56 (n=22) | unknown/non-function (uniform mat4 projection_matrix;) | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 60–84 (n=25) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 95–99 (n=5) | main | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/pbrglowV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 30–51 (n=22) | unknown/non-function (uniform mat4 projection_matrix;) | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 53–68 (n=16) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 71–86 (n=16) | main | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/pbropaqueIndexedV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 29–35 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 49–57 (n=9) | unknown/non-function (uniform vec4 gltf_mr_transform[2*GLTF_INDEXED_CHANNELS];) | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 77–85 (n=9) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/pbropaqueV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–31 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 46–54 (n=9) | unknown/non-function (uniform vec4[2] texture_metallic_roughness_transform;) | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 71–79 (n=9) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 147–149 (n=3) | unknown/non-function (void main()) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/postDeferredF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 47–58 (n=12) | dofSample | sensor | creative-candidate | unknown; conditional evidence present | Edits AL_SENSOR_HDR |
| 67–78 (n=12) | dofSampleNear | sensor | creative-candidate | unknown; conditional evidence present | Edits AL_SENSOR_HDR |
| 81–181 (n=101) | main, sensorCell, sensorDiskCount, sensorDof, sensorReflect | sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: Conservative area reduction onto the actual integer gather viewport. Each source cell scatters unit mass into a discrete circular aperture. Gathering those same coefficients preserves isolated-highlight flux; luminance never changes a source's total weight. GLSL integer remainder |
| 229–236 (n=8) | main | sensor | creative-candidate | unknown; conditional evidence present | Edits AL_SENSOR_HDR |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/shadowAlphaMaskF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 28–105 (n=78) | main | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: [ShadowDist P3] shadow-pass alpha policy (design v4 s3 / v5 s2). Mode 0 = stock (the legacy branches below run verbatim). Mode 1 = one LOW cutoff, no column dither: thin alpha-cut jewelry / hair whose mip-averaged alpha lands between the cutoff and 0.88 casts a CONTINUOUS shadow  |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/shadowAlphaMaskV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 32–52 (n=21) | main | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 59–64 (n=6) | main | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/shadowCubeV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 26–32 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 35–41 (n=7) | main | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/shadowF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 24–38 (n=15) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/shadowSkinnedV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 27–39 (n=13) | main | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/shadowV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 26–36 (n=11) | main | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/treeShadowF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 33–41 (n=9) | unknown/non-function (uniform float minimum_alpha;) | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 46–55 (n=10) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/treeShadowSkinnedV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 31–43 (n=13) | main | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/deferred/treeShadowV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 30–40 (n=11) | main | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class1/objects/simpleNoColorV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 27–42 (n=16) | unknown/non-function (uniform mat3 normal_matrix;) | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 55–65 (n=11) | main | looks | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/app_settings/shaders/class2/deferred/reflectionProbeF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 27–42 (n=16) | unknown/non-function (declaration, XML, build list or global scope) | sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: [EnvIntensity] viewer-side Sky/GI intensity (2^AlchemyEnvSkyGIEV); mirrors the class3 probe-irradiance multiplier so probes-off behaves consistently. [EnvIntensity v2] probe reflections EV factor (1.0 = stock, 1.0 in captures) and the luminance-normalised ambient tint (vec3(1) =  |
| 47–67 (n=21) | sampleReflectionProbes | sensor | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 79–105 (n=27) | sampleReflectionProbesLegacy | sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: [EnvIntensity v2] legacyenv stays sRGB-ENCODED here (applyLegacyEnv mixes encoded values), so the EV factor is applied in linear light with a bypass: the off path never round-trips the encoded value. |
| 108–124 (n=17) | applyLegacyEnv, applyLegacyEnvKeep | looks, sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: [AvatarLightResponse] keep scales ONLY the (encoded) reflection addend; the mixing weight is untouched and keep == 1.0 is bit-exact. The old name forwards 1.0. This probes-off path mixes in ENCODED (sRGB) space, so the reflection addend is scaled in encoded space too: monotonic i |

## Slice 10: `indra/newview/app_settings/shaders/class3/deferred/softenLightF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 31–37 (n=7) | unknown/non-function (out vec4 frag_color;) | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 51–58 (n=8) | unknown/non-function (uniform vec3 sun_dir;) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 103–174 (n=72) | areaDepthFill | looks, sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: [AvatarLightResponse] spec_keep variant (deferredUtil.glsl): PBR pixels carry Tame in normal.b (envIntensity) [EnvIntensity v2] deferredUtil.glsl: shadow lift (0 gain = untouched). [EnvIntensity v2] luminance-normalised ambient tint (vec3(1) = off); the SSAO ceiling is an absolut |
| 176–196 (n=21) | adjustIrradiance, main | sensor | creative-candidate | unknown; conditional evidence present | Comment evidence: This is the only uniform comparison paid by mode 0.  All diagnostic evaluation and the output override stay behind the resulting boolean. |
| 198–226 (n=29) | main | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Keep mode 0 free of a new texture read while allowing diagnostics to be enabled at runtime even if shadows and SSAO were off at shader load. |
| 233–244 (n=12) | main | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 271–295 (n=25) | main | looks, sensor | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 299–305 (n=7) | main | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 322–334 (n=13) | main | sensor | creative-candidate | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 336–342 (n=7) | main | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 345–354 (n=10) | main | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 373–384 (n=12) | main | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 386–394 (n=9) | main | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 397–422 (n=26) | defined, main | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 10: `indra/newview/llgltfmaterialpreviewmgr.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 33–39 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | sensor | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 391–412 (n=22) | fixup_shader_constants | sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: [EnvIntensity v2] Neutral direct / sampling terms for the thumbnail: the preview may render before the per-frame environment refresh and would otherwise inherit whatever (possibly capture-time) exposure-strip factors the SG_ANY cache holds. Force identity so no Sun/Moon EV, GI, p |
| 490–503 (n=14) | LLGLTFPreviewTexture::render | sensor | creative-candidate | unknown; no local conditional evidence | Comment evidence: [EnvIntensity v2] This setupHWLights() must see Local lights EV == 1 so the preview's own lights are never scaled by the exposure strip. Scoped to this call only: the clean-up setupHWLights() at the end of render() restores the normal (scaled) world lights. |

## Slice 10: `indra/newview/llheroprobemanager.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 86–100 (n=15) | LLHeroProbeManager::update | looks | creative-candidate | unknown; conditional evidence present | Comment evidence: [ShaderLoadTime] The reload is what makes mirrors work; wiping the whole binary cache first forced a second COLD compile of every program at login (~45 s+ in this fork with its 44 shared Actor FX variants). Keep the cache by default so the reload is mostly cache hits; RenderHeroP |

## Slice 10: `indra/newview/skins/default/xui/en/panel_status_bar.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 65–81 (n=17) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: xui-check: overlay <search_button> <clear_button> --> |
| 86–92 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 181–187 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | looks | creative-candidate | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/llappearance/llavatarappearance.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 169–175 (n=7) | unknown/non-function (protected:) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLAvatarSkeletonInfo |

## Slice 99: `indra/llaudio/llaudiodecodemgr.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 659–667 (n=9) | beginDecodingAndWritingAudio | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Decoded audio is written into the dedicated "sounds" cache subfolder (see LLAudioEngine::init, which creates it) rather than loose in the cache root. |

## Slice 99: `indra/llaudio/llaudioengine.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 123–135 (n=13) | LLAudioEngine::init | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Decoded sounds live in their own cache subfolder ("sounds") to keep the cache tidy instead of dumping loose .dsf files into the cache root. Create it up front so the first decode's write has somewhere to go (mkdir is a harmless no-op if it already exists). Still under LL_PATH_CAC |
| 1121–1127 (n=7) | LLAudioEngine::hasDecodedFile | unknown | unknown-review | unknown; no local conditional evidence | Edits gDirUtilp, LL_PATH_CACHE |
| 2117–2123 (n=7) | LLAudioData::load | unknown | unknown-review | unknown; no local conditional evidence | Edits gDirUtilp, LL_PATH_CACHE |

## Slice 99: `indra/llcharacter/lljoint.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1026–1028 (n=3) | unknown/non-function (void LLJoint::clampRotation(LLQuaternion old_rot, LLQuaternion new_rot)) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/llcharacter/lljoint.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 303–305 (n=3) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/llcharacter/lljointstate.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 112–127 (n=16) | isPresentationExcluded, setPresentationExcluded | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Precomputed by the owning LLMotion (see LLMotion::addJointState and rebuildJointSignature) whenever its presentation joint exclusion changes. The pose blender reads this flag instead of evaluating a predicate per joint per frame. Always false unless a presentation owner excludes  |

## Slice 99: `indra/llcharacter/llmotion.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 51–64 (n=14) | unknown/non-function (LLMotion::LLMotion( const LLUUID &id ) :) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 110–129 (n=20) | LLMotion::addJointState | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Honor any per-instance priority override here too, so the joint signature this builds agrees with what the pose blender will use. With no override this is the stock rule (USE_MOTION_PRIORITY -> motion base priority). Precompute the presentation exclusion once, here, so the pose b |
| 131–326 (n=196) | LLMotion::addJointState, LLMotion::claimPresentationControl, LLMotion::isFaceJoint, LLMotion::isPresentationControlledBy, LLMotion::rebuildJointSignature, LLMotion::releasePresentationControl, LLMotion::resetPresentationControl, LLMotion::setExternalSampleTime, LLMotion::setOwnerManagedLifetime, LLMotion::setPresentationJointExclusion, LLMotion::setPresentationWeight, LLMotion::setPriorityOverride, LLMotion::useNativePresentationClock, getNextJointState | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Evaluate the predicate once per joint state here (not per frame); LLPoseBlender::addMotion reads the cached flag. ----------------------------------------------------------------------------- setPriorityOverride() ------------------------------------------------------------------ |
| 360–375 (n=16) | LLMotion::deactivate | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: A client-side priority override does not survive a stop: clear it and restore the baked joint signature so a later replay of this instance starts from the asset's real priority unless the caller re-applies one. |
| 386–388 (n=3) | unknown/non-function (bool LLMotion::canDeprecate()) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/llcharacter/llmotion.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 59–70 (n=12) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 93–208 (n=116) | applyPresentationWeight, getEffectiveUpdateTime, getExternalSampleClockTime, getJointPriority, getPresentationBaseWeight, getPresentationClockRevision, getPresentationWeight, getPriorityOverride, hasOwnerManagedLifetime, hasPresentationControl, isPresentationJointExcluded, usesExternalSampleClock | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: ------------------------------------------------------------------------- Client-side, PER-INSTANCE priority override (machinima capture). -1 == none (baked priority, byte-identical to stock). When set, THIS motion instance blends at the overridden priority WITHOUT mutating the s |
| 267–275 (n=9) | supportsExternalSampling | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Only deterministic keyframe-backed motions opt into absolute sampling. |
| 303–324 (n=22) | unknown/non-function (protected:) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLUUID |
| 396–398 (n=3) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/llcharacter/llmotioncontroller.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 45–51 (n=7) | unknown/non-function (const U32 MAX_MOTION_INSTANCES = 32;) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLMotionController |
| 419–425 (n=7) | LLMotionController::startMotion | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 531–542 (n=12) | LLMotionController::updateIdleMotion | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 550–557 (n=8) | LLMotionController::updateIdleMotion | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 655–662 (n=8) | LLMotionController::updateMotionsByType | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 693–700 (n=8) | LLMotionController::updateMotionsByType | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 714–721 (n=8) | LLMotionController::updateMotionsByType | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 724–732 (n=9) | LLMotionController::updateMotionsByType | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 738–749 (n=12) | LLMotionController::updateMotionsByType | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 767–787 (n=21) | LLMotionController::updateMotionsByType | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 793–799 (n=7) | LLMotionController::updateMotionsByType | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 883–890 (n=8) | LLMotionController::updateMotions | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: sGlobalTimeFactor scales every controller uniformly (see setGlobalTimeFactor). |
| 992–1003 (n=12) | LLMotionController::activateMotionInstance | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/llcommon/llmemory.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 422–444 (n=23) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Raw system metrics. sAvailPhysicalMemInKB below is the effective allocation budget after physical, commit, and process-cap limits. |

## Slice 99: `indra/llcommon/llstring.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 672–699 (n=28) | unknown/non-function (using EmojiClusterList = std::vector<std::pair<size_t, size_t>>;) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Cost note for the cluster-aware helpers below: the single-argument forms rebuild the cluster list internally on every call (an O(N) scan of 'wstr'). Fine for one-off cursor moves; expensive in tight loops over the same text. Callers making multiple cluster-aware queries on the sa |

## Slice 99: `indra/llcorehttp/httpcommon.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 31–37 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 345–356 (n=12) | cleanup, releaseDetachedTransfer, retainDetachedTransfer | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/llcorehttp/httpcommon.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 496–504 (n=9) | unknown/non-function (namespace LLHttp) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Detached private downloads retain global curl/TLS state until process exit. |

## Slice 99: `indra/llfilesystem/lldir.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 367–373 (n=7) | LLDir::buildSLOSCacheDir | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/llfilesystem/lldir_mac.mm`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 92–98 (n=7) | LLDir_Mac::LLDir_Mac | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/llmath/llcamera.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 63–69 (n=7) | LLCamera::LLCamera | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 135–141 (n=7) | LLCamera::setNear | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/llmath/llcamera.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 45–53 (n=9) | unknown/non-function (constexpr F32 MAX_FAR_CLIP      = 512.0f;) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge G2.1] absolute clamp floor; values below MIN_NEAR_PLANE are only reachable via the BDMergeNearClipDistance override (costs depth precision) |

## Slice 99: `indra/llrender/CMakeLists.txt`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 31–37 (n=7) | unknown/non-function (set(llrender_SOURCE_FILES) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/llrender/llcubemap.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 95–101 (n=7) | LLCubeMap::initGL | unknown | unknown-review | unknown; no local conditional evidence | Edits LLImageGLMemory |
| 162–168 (n=7) | LLCubeMap::initGLData | unknown | unknown-review | unknown; no local conditional evidence | Edits LLImageGLMemory |
| 219–225 (n=7) | LLCubeMap::initEnvironmentMap | unknown | unknown-review | unknown; no local conditional evidence | Edits LLImageGLMemory |
| 241–247 (n=7) | LLCubeMap::generateMipMaps | unknown | unknown-review | unknown; no local conditional evidence | Edits LLImageGLMemory |

## Slice 99: `indra/llrender/llimagegl.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 70–145 (n=76) | get_estimated_mip_chain_bytes, get_estimated_texture_bytes, get_texture_allocation | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Generic GL_COMPRESSED_* formats let the driver choose the codec. Estimate their resident block-compressed footprint instead of charging the source channel depth as if it were uncompressed. |
| 148–207 (n=60) | LLImageGLMemory::alloc_tex_image, LLImageGLMemory::update_tex_image | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Sum the mip pyramid down to 1x1 the same way getMipBytes does, so non-power-of-two and non-square cases stay exact rather than relying on the 4/3 geometric-series approximation. it is a precondition that no existing allocation exists for this texture Remain symmetric in release b |
| 209–221 (n=13) | LLImageGLMemory::free_tex_image | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 249–266 (n=18) | LLImageGL::getMipmappedTextureBytesAllocatedEstimate, LLImageGL::getTextureBytesAllocatedEstimate | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: static static |
| 705–712 (n=8) | LLImageGL::LLImageGL | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 747–754 (n=8) | LLImageGL::init | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 946–956 (n=11) | LLImageGL::setImage | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Keep this upload's analysis private until its texture name is published. |
| 982–992 (n=11) | LLImageGL::setImage | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1006–1015 (n=10) | LLImageGL::setImage | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1023–1029 (n=7) | LLImageGL::setImage | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1064–1070 (n=7) | LLImageGL::setImage | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1167–1173 (n=7) | LLImageGL::setImage | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1208–1217 (n=10) | LLImageGL::setImage | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1223–1230 (n=8) | LLImageGL::setImage | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1240–1259 (n=20) | LLImageGL::setImage | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Direct updates already target the visible texture. |
| 1386–1393 (n=8) | LLImageGL::setSubImage | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1478–1485 (n=8) | LLImageGL::setSubImageFromFrameBuffer | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1788–1794 (n=7) | LLImageGL::createGLTexture | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1995–2005 (n=11) | LLImageGL::createGLTexture | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 2018–2024 (n=7) | LLImageGL::createGLTexture | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 2028–2035 (n=8) | LLImageGL::createGLTexture | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 2043–2049 (n=7) | unknown/non-function (bool LLImageGL::createGLTexture(S32 discard_level, const U8* data_in, bool data_) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLImageGL, LLGLuint |
| 2090–2096 (n=7) | LL::WorkQueue::postMaybe, LLImageGL::syncToMainThread | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 2098–2104 (n=7) | unknown/non-function (void LLImageGL::syncToMainThread(LLGLuint new_tex_name)) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLImageGL, LLGLuint |
| 2107–2114 (n=8) | LLImageGL::syncTexName | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 2253–2259 (n=7) | LLImageGL::destroyGLTexture | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 2428–2475 (n=48) | LLImageGL::getDefaultMinAlpha, LLImageGL::getMinAlpha, LLImageGL::isMinAlphaKnown, LLImageGL::setNeedsAlphaAndPickMask | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Deprecated luminance formats become RED/RG after alpha layout is calculated. |
| 2637–2658 (n=22) | LLImageGL::analyzeAlpha | unknown | unknown-review | unknown; conditional evidence present | Edits LLImageGL |
| 2697–2723 (n=27) | LLImageGL::analyzeAlpha | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Keep the fork's histogram unchanged, but include every odd edge in the minimum. |
| 2728–2734 (n=7) | LLImageGL::analyzeAlpha | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 2766–2776 (n=11) | LLImageGL::analyzeAlpha | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 3094–3101 (n=8) | unknown/non-function (void LLImageGL::checkActiveThread()) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLImageGL |
| 3137–3139 (n=3) | unknown/non-function (void LLImageGLThread::run()) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/llrender/llimagegl.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 40–46 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 56–64 (n=9) | unknown/non-function (namespace LLImageGLMemory) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Reclassify a shared-name texture after all surfaces or mip levels exist. Legacy cubemaps allocate six faces through one GL texture name. |
| 73–86 (n=14) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Get an estimate of how many bytes have been allocated in vram for textures. Allocations recorded via alloc_tex_image with has_mips=true include the full mip pyramid (driver-padded via dataFormatVRAMBytes); allocations from external setManualImage callers that don't pass has_mips= |
| 135–142 (n=8) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 189–195 (n=7) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 199–206 (n=8) | syncToMainThread | unknown | unknown-review | unknown; no local conditional evidence | Edits LLGLuint |
| 234–242 (n=9) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Lowest analysed alpha, 255 for formats without alpha, 0 when unknown. |
| 309–316 (n=8) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 396–406 (n=11) | setTexName, syncTexName | unknown | unknown-review | unknown; no local conditional evidence | Edits LLGLuint |

## Slice 99: `indra/llrender/llvertexbuffer.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 29–35 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 331–338 (n=8) | cleanIfDue | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Run any deferred expiry sweep (see RenderVBOPoolCleanAtFrameEnd). Default: nothing to do. |
| 536–553 (n=18) | clean, cleanIfDue | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: RenderVBOPoolCleanAtFrameEnd: only count the touch here; the expiry sweep runs from cleanIfDue() at the end of the frame, never in the middle of a rebuild. Same cadence as always (every 1k touches, 5 s expiry); only the call site differs. |
| 706–712 (n=7) | unknown/non-function (U32 LLVertexBuffer::sGLRenderBuffer = 0;) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLVertexBuffer |
| 925–931 (n=7) | LLVertexBuffer::drawRange | unknown | unknown-review | unknown; conditional evidence present | Edits LLDrawBudget |
| 938–944 (n=7) | LLVertexBuffer::drawRangeFast | unknown | unknown-review | unknown; conditional evidence present | Edits LLDrawBudget |
| 952–958 (n=7) | LLVertexBuffer::drawArrays | unknown | unknown-review | unknown; conditional evidence present | Edits LLDrawBudget |
| 990–1005 (n=16) | LLVertexBuffer::endOfFrameCleanPool | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: static |

## Slice 99: `indra/llrender/llvertexbuffer.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 120–130 (n=11) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: RenderVBOPoolCleanAtFrameEnd: call once per frame from a safe end-of-frame point. enabled = true defers the VBO pool's expiry sweep from alloc/free to this call; false restores the stock "sweep inside alloc/free" behaviour. |
| 335–341 (n=7) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/llui/llbutton.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 91–99 (n=9) | unknown/non-function (LLButton::Params::Params()) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 154–164 (n=11) | unknown/non-function (LLButton::LLButton(const LLButton::Params& p)) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 185–192 (n=8) | unknown/non-function (LLButton::LLButton(const LLButton::Params& p)) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 438–447 (n=10) | LLButton::onVisibilityChange | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 457–465 (n=9) | LLButton::handleUnicodeCharHere | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 477–485 (n=9) | LLButton::handleKeyHere | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 495–502 (n=8) | LLButton::handleMouseDown | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 587–594 (n=8) | LLButton::handleRightMouseDown | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 866–872 (n=7) | LLButton::draw | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 888–899 (n=12) | LLButton::draw | unknown | unknown-review | unknown; no local conditional evidence | Edits LLColor4 |
| 904–910 (n=7) | LLButton::draw | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1235–1245 (n=11) | LLButton::setImageColor | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Preserve the historical single-tint behaviour for callers that do not opt into a separate selected tint. |
| 1327–1338 (n=12) | LLButton::isFocusRingVisible | unknown | unknown-review | unknown; no local conditional evidence | Edits LLButton |

## Slice 99: `indra/llui/llbutton.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 97–105 (n=9) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 225–232 (n=8) | getLeftHPad, getRightHPad | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 237–245 (n=9) | setHoverImageColor, setPressedImageColor, setSelectedImageColor | unknown | unknown-review | unknown; no local conditional evidence | Edits LLUIColor |
| 250–260 (n=11) | setImageOverlayColor, setImageOverlaySelectedColor | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Overlay colour as a live LLUIColor (a colour-table reference follows theme changes). |
| 280–288 (n=9) | setKeyboardOnlyFocusBorder, showFocusRingFromKeyboard | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 365–373 (n=9) | unknown/non-function (protected:) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLUIColor |
| 406–413 (n=8) | unknown/non-function (protected:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/llui/llcombobox.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 73–86 (n=14) | unknown/non-function (LLComboBox::Params::Params()) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 94–107 (n=14) | unknown/non-function (LLComboBox::LLComboBox(const LLComboBox::Params& p)) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 687–695 (n=9) | LLComboBox::showList | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Most combos should track the closed control width. Image-rich pickers may request a wider popup without widening or rearranging their owning panel. |
| 964–979 (n=16) | LLComboBox::handleScrollWheel | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [AL] wheel_select opt-out: below this point a hovered wheel over a CLOSED combo changes the selection and commits. For combos where that is a semantic hazard rather than a harmless scrub (mode/preset jumps -- see floater_ultimate_diopter.xml), refuse the event so it can bubble to |

## Slice 99: `indra/llui/llcombobox.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 72–79 (n=8) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 89–101 (n=13) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [AL] Opt-out: a hovered wheel over a CLOSED combo normally changes the selection and commits (llcombobox.cpp:958-1000). Default true preserves that behavior everywhere; set false per-combo where a stray wheel click is a semantic hazard rather than a harmless scrub (e.g. mode/pres |
| 244–250 (n=7) | isListVisible | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 261–267 (n=7) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 271–277 (n=7) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/llui/llcontainerview.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 59–81 (n=23) | LLContainerView::getChildCullRectScreen, LLContainerView::postBuild | unknown | unknown-review | unknown; conditional evidence present | Edits LLScrollContainer, LLRect, LLContainerView |

## Slice 99: `indra/llui/llcontainerview.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 83–89 (n=7) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLRect |

## Slice 99: `indra/llui/llfolderview.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 43–49 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 164–170 (n=7) | unknown/non-function (LLFolderView::Params::Params()) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLRect |
| 325–340 (n=16) | LLFolderView::arrange | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: LLFolderViewFolder::arrange is recursive; keep one zone at its top-level caller rather than emitting a zone for every folder. |
| 353–374 (n=22) | LLFolderView::filter | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [PerfB1] dirtyFilter() bubbles -1 to the root; a current, unmodified root has no work. [FolderIdle] Shown (or force-arranged) views get the foreground slice; hidden views only reach here on update()'s throttled background-settle frames. |
| 654–660 (n=7) | LLFolderView::startDrag | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |
| 700–711 (n=12) | LLFolderView::draw | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 744–756 (n=13) | LLFolderView::draw | unknown | unknown-review | unknown; conditional evidence present | Edits LLUICachedControl, LLRect |
| 1635–1641 (n=7) | LLFolderView::handleDragAndDrop | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |
| 1765–1805 (n=41) | LLFolderView::update | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [FolderIdle] mForceArrange views are hidden on purpose, but their filter result decides whether they get shown again (Combination inventory view), so they keep filtering like visible views. [FolderIdle] Hidden views settle in the background at 1 ms every 8th frame (~0.13 ms/frame |
| 1836–1842 (n=7) | LLFolderView::update | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/llui/llfolderview.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 222–228 (n=7) | getChildCullRectScreenCached | unknown | unknown-review | unknown; no local conditional evidence | Edits LLRect |
| 272–278 (n=7) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLRect |

## Slice 99: `indra/llui/llfolderviewitem.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 686–692 (n=7) | LLFolderViewItem::handleMouseDown | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |
| 726–747 (n=22) | LLFolderViewItem::handleHover | unknown | unknown-review | unknown; conditional evidence present | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI, LL_PROFILE_ZONE_NAMED_CATEGORY_UI |
| 782–788 (n=7) | LLFolderViewItem::handleMouseUp | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |
| 1283–1294 (n=12) | LLFolderViewFolder::getChildCullRectScreen | unknown | unknown-review | unknown; no local conditional evidence | Edits LLRect, LLFolderViewFolder, LLFolderView |
| 1343–1348 (n=6) | LLFolderViewFolder::arrange | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |
| 2340–2346 (n=7) | LLFolderViewFolder::handleDragAndDrop | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |
| 2365–2371 (n=7) | LLFolderViewFolder::handleDragAndDropToThisFolder | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |
| 2734–2736 (n=3) | unknown/non-function (LLFolderViewItem* LLFolderViewFolder::getPreviousFromChild( LLFolderViewItem* it) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/llui/llfolderviewitem.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 372–378 (n=7) | unknown/non-function (protected:) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLRect |

## Slice 99: `indra/llui/lllineeditor.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 132–138 (n=7) | unknown/non-function (LLLineEditor::LLLineEditor(const LLLineEditor::Params& p)) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLFontGL |
| 1809–1822 (n=14) | LLLineEditor::handleKeyHere | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 2003–2010 (n=8) | LLLineEditor::drawBackground | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 2066–2083 (n=18) | LLLineEditor::draw | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Compact value fields can align at the right edge without changing the normal cursor/selection rendering or the behavior of existing editors. |

## Slice 99: `indra/llui/lllineeditor.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 263–279 (n=17) | getHAlign, setHAlign, setReturnCallback | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Optional explicit Return action; focus loss and programmatic commits continue to use the ordinary commit callback. |
| 297–307 (n=11) | getBgColor, getUseBgColor, setBgBorderColor, setBgColor, setUseBgColor | unknown | unknown-review | unknown; no local conditional evidence | Edits LLUIColor |
| 382–395 (n=14) | unknown/non-function (protected:) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLFontGL |
| 437–444 (n=8) | unknown/non-function (protected:) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLUIColor |

## Slice 99: `indra/llui/lllocalcliprect.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 41–58 (n=18) | getCurrentClipRect | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Top of the UI scissor stack in LLFontGL::sCurOrigin space (UI units, before UI scaling). Top-level views have no active clip. |

## Slice 99: `indra/llui/llmenugl.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 3990–4005 (n=16) | LLTearOffMenu::draw | unknown | unknown-review | unknown; conditional evidence present | Edits LLUI |

## Slice 99: `indra/llui/llmenugl.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 897–903 (n=7) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLFrameTimer |

## Slice 99: `indra/llui/llnotifications.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 64–70 (n=7) | unknown/non-function (LLNotificationForm::FormElementBase::FormElementBase()) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 188–194 (n=7) | LLNotificationForm::LLNotificationForm | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 201–207 (n=7) | LLNotificationForm::LLNotificationForm | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1895–1912 (n=18) | LLNotifications::clearSavedResponse | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: The remembered answer of a "Don't ask me again" notification (save_option="true"). Re-enabling the prompt must drop it, or disabling the prompt again later would replay a stale answer. |

## Slice 99: `indra/llui/llnotifications.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 175–181 (n=7) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 268–280 (n=13) | getIgnoreCheckboxLabel | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 989–996 (n=8) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Clears the remembered answer of a save_option="true" notification (no-op for other kinds). |

## Slice 99: `indra/llui/llpanel.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 200–206 (n=7) | LLPanel::draw | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |
| 304–313 (n=10) | LLPanel::handleKeyHere | unknown | unknown-review | unknown; conditional evidence present | Edits LLButton, gFocusMgr |

## Slice 99: `indra/llui/llscrollbar.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 126–135 (n=10) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLUIImage, LLUIColor |

## Slice 99: `indra/llui/llscrollcontainer.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 96–102 (n=7) | getScrolledView | unknown | unknown-review | unknown; no local conditional evidence | Edits LLView |
| 105–111 (n=7) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 133–140 (n=8) | unknown/non-function (protected:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 158–164 (n=7) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/llui/llscrolllistcell.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 105–111 (n=7) | unknown/non-function (const LLSD LLScrollListCell::getAltValue() const) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 118–124 (n=7) | LLScrollListIcon::getHeight | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 795–797 (n=3) | unknown/non-function (bool LLScrollListLineEditor::handleUnicodeCharHere(llwchar uni_char )) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/llui/llscrolllistcell.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 67–74 (n=8) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 92–98 (n=7) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/llui/llscrolllistctrl.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 2081–2091 (n=11) | LLScrollListCtrl::handleMouseUp | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/llui/llscrolllistctrl.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 297–305 (n=9) | setLastSelectedItem | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge F3] FS radar restores the multi-select anchor after a list rebuild (FSScrollListCtrl parity); mLastSelected is private. |
| 406–414 (n=9) | getMouseUpHitItem | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Valid only during the mouse-up commit, for actions requiring a row hit. |
| 482–494 (n=13) | setLineHeight | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge F3] FS radar list wants a fixed row height (FSScrollListCtrl parity); mLineHeight is private so expose a narrow setter. |

## Slice 99: `indra/llui/lltabcontainer.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1215–1221 (n=7) | unknown/non-function (void LLTabContainer::addPlaceholder(LLPanel* child, const std::string& label)) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLTabContainer, LLPanel |
| 1299–1306 (n=8) | LLTabContainer::removeTabPanel | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1825–1908 (n=84) | LLTabContainer::getTabContainedAtPoint, LLTabContainer::setTabButtonVisible, LLTabContainer::setTabPadding, LLTabContainer::setUseTabEllipses | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Ported from Firestorm (PanteraPolnocy, FIRE-35598); concept from Catznip. |

## Slice 99: `indra/llui/lltabcontainer.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 179–185 (n=7) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLPanel |
| 218–229 (n=12) | getUseTabEllipses | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Unlike setTabVisibility, this does not change the selected panel. |

## Slice 99: `indra/llui/lltextbase.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 252–258 (n=7) | unknown/non-function (LLTextBase::LLTextBase(const LLTextBase::Params &p)) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1055–1061 (n=7) | LLTextBase::insertStringNoUndo | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |
| 2416–2423 (n=8) | LLTextBase::setText | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |
| 2462–2532 (n=71) | AppendCursorScope, LLTextBase::appendTextImpl, ~AppendCursorScope | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [TextLinear] RAII scope for the once-per-append cursor/selection save+restore (audit A3). Previously appendAndHighlightTextImpl did this for every '\n'-separated chunk: deselect, setCursorPos(old_length), insert, restore -- two setCursorPos calls per line, each running a grapheme |
| 2783–2796 (n=14) | LLTextBase::appendAndHighlightTextImpl | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [TextLinear] When called inside appendTextImpl (the only in-tree path), AppendCursorScope has already saved selection+cursor and parked the cursor at the end; the per-chunk save/restore below is skipped. The fallback keeps the old behaviour for any direct caller. |
| 2799–2809 (n=11) | LLTextBase::appendAndHighlightTextImpl | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 2877–2887 (n=11) | LLTextBase::appendAndHighlightTextImpl | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 3557–3563 (n=7) | LLTextBase::setCursorPos | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |
| 3568–3591 (n=24) | LLTextBase::setCursorPos | unknown | unknown-review | unknown; conditional evidence present | Comment evidence:  [TextLinear] Fast path (audit A2): a document boundary (0 or >= length) can never be mid-cluster, and neither can a position whose own codepoint is ASCII -- every non-first codepoint of an emoji cluster (ZWJ, VS, keycap mark, skin tone, tag, joined base, second regional indicato |

## Slice 99: `indra/llui/lltextbase.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 498–504 (n=7) | getHAlign | unknown | unknown-review | unknown; no local conditional evidence | Edits LLFontGL |
| 726–732 (n=7) | unknown/non-function (protected:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 829–840 (n=12) | unknown/non-function (protected:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [TextLinear] appendTextImpl saves/restores selection+cursor once for the whole append; while true, appendAndHighlightTextImpl skips its own per-chunk save/restore (which cost two whole-document setCursorPos calls per line and made multi-line appends quadratic). |

## Slice 99: `indra/llui/lluicolortable.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 27–36 (n=10) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 83–93 (n=11) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Changes a skin colour's value in place, so existing LLUIColor references see it, without marking it user-modified: saveUserSettings() ignores it. Used by colour themes. Returns false when name is not a skin colour. |
| 98–109 (n=12) | setReloadCallback | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Called at the end of every loadFromSettings(), after the skin and user files are re-read. |
| 120–135 (n=16) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Skin values captured right after the skin files load. saveUserSettings() compares against these, so a theme value in mLoadedColors is never mistaken for a user change. Names reset to default: their user node still exists (nodes are never erased, LLUIColor points at them) but only |

## Slice 99: `indra/llui/llview.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 49–56 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 60–70 (n=11) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLView |
| 74–80 (n=7) | unknown/non-function (bool    LLView::sDebugRectsShowNames = true;) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLView |
| 84–90 (n=7) | unknown/non-function (S32     LLView::sLastLeftXML = S32_MIN;) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLView |
| 261–286 (n=26) | LLView::applyTransparencyType | unknown | unknown-review | unknown; conditional evidence present | Edits LLView, LLUICtrl |
| 354–360 (n=7) | LLView::addChild | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 665–675 (n=11) | LLView::setVisible | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 765–773 (n=9) | LLView::visibleAndContains | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 830–836 (n=7) | LLView::childrenHandleMouseEvent | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 856–862 (n=7) | LLView::childrenHandleToolTip | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 916–922 (n=7) | LLView::childrenHandleHover | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1287–1296 (n=10) | LLView::drawChildren | unknown | unknown-review | unknown; no local conditional evidence | Edits LLRect |
| 1305–1319 (n=15) | LLView::drawChildren | unknown | unknown-review | unknown; conditional evidence present | Edits LLRect |
| 1335–1344 (n=10) | LLView::drawChildren | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1623–1633 (n=11) | LLView::calcScreenRect | unknown | unknown-review | unknown; no local conditional evidence | Edits LLRect |
| 1723–1740 (n=18) | LLView::findChildView | unknown | unknown-review | unknown; conditional evidence present | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI, LL_PROFILE_ZONE_NAMED_ACTIVE, LLView |
| 1742–1752 (n=11) | LLView::findChildView | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1755–1774 (n=20) | LLView::findChildView | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1810–1821 (n=12) | LLView::localPointToScreen | unknown | unknown-review | unknown; no local conditional evidence | Edits LLView, LLRect |

## Slice 99: `indra/llui/llview.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 289–304 (n=16) | setVisibleDirect | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 545–555 (n=11) | getChildCullRectScreen | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Optional screen-space bound on children worth drawing. A parent that knows its children are clipped by a viewport can provide that viewport once per draw; LLRect::null leaves the general cull unchanged. |
| 600–607 (n=8) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 679–693 (n=15) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Per-frame UI draw counters, reset after LLViewerWindow draws the tree. Per-frame outer findChildView call and visited-node counters. |
| 707–715 (n=9) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Optional test/diagnostic observer. Empty in production; invoked only after a child has actually accepted a mouse/hover/tooltip event. |

## Slice 99: `indra/llui/llviewereventrecorder.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 124–130 (n=7) | LLViewerEventRecorder::logVisibilityChange | unknown | unknown-review | unknown; no local conditional evidence | Edits LLViewerEventRecorder |

## Slice 99: `indra/llui/llviewereventrecorder.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 55–61 (n=7) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/llui/llviewquery.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 107–128 (n=22) | LLViewQuery::filterChildren | unknown | unknown-review | unknown; no local conditional evidence | Edits LLView, LLViewQuery |

## Slice 99: `indra/llui/llviewquery.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 126–132 (n=7) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLView |

## Slice 99: `indra/llwindow/llwindow.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 155–162 (n=8) | setHideCursorWithUI | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Shape override only; must not change hide/show ownership or mouse input. |

## Slice 99: `indra/llwindow/llwindowwin32.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 38–45 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 79–85 (n=7) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 145–185 (n=41) | getFrameEnvGeneration, getFrameFocusGeneration, getFrameSwapInterval | unknown | mixed/perf-review | unknown; no local conditional evidence | Comment evidence: [FrameLimiter] Used by the frame rate cap (newview/alframelimiter.cpp). Main thread only. True when the viewer's VSync is on, the swap-interval extension is usable and the GL context is current on this thread. True when the window is visible, not minimised and is the foreground w |
| 285–306 (n=22) | unknown/non-function (protected:) | unknown | mixed/perf-review | unknown; no local conditional evidence | Comment evidence: [FrameLimiter] VSync as the viewer set it, the limiter's driver swap interval, and the cached refresh rate of the window's monitor. |

## Slice 99: `indra/llxml/llcontrol.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 222–235 (n=14) | LLControlVariable::setValue | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 267–273 (n=7) | LLControlVariable::setDefaultValue | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 276–282 (n=7) | LLControlVariable::setDefaultValue | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 300–311 (n=12) | LLControlVariable::resetToDefault | unknown | unknown-review | unknown; no local conditional evidence | Edits LLControlVariable |

## Slice 99: `indra/llxml/llcontrol.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 36–42 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 75–81 (n=7) | unknown/non-function (class LLControlVariable : public LLRefCount) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLControlVariable |
| 94–100 (n=7) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 115–129 (n=15) | getWriteSignal | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Accepted writes, including same-value writes and explicit resets. Unlike the change signal, this expresses intent to replace a runtime overlay. Allocate only for the few controls whose observers need this contract. |
| 144–151 (n=8) | notifyWrite | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/alfloaterblocked.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 411–419 (n=9) | ALPanelDerenderList::onSelectionRemove | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: The list holds objects and avatars; an id only matches entries of its own type. |
| 435–445 (n=11) | ALPanelDerenderList::refresh | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: sdColumns is reused across rows: without this an avatar row would show the previous row's location. |

## Slice 99: `indra/newview/alfloaterexploresounds.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 9–20 (n=12) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLContextMenu, LLUICtrl |
| 36–48 (n=13) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: right-click context menu on the sound list: reveal the selected sound's decoded cache file (cache/sounds/<asset>.dsf) in the OS file manager. |

## Slice 99: `indra/newview/aoengine.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 496–528 (n=33) | AOEngine::resolveAnimation | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Prefer the asset already active for this state. Otherwise use the selected slot exactly as-is: discovery must not advance/randomize the AO cycle or lazily rewrite inventory-backed state. |

## Slice 99: `indra/newview/aoengine.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 99–110 (n=12) | isEnabled | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Resolve the AO asset currently selected for a stock motion without cycling state, stopping animations, changing timers, or sending a simulator request. Null means the stock motion should be used. |

## Slice 99: `indra/newview/app_settings/settings_per_account.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–13 (n=13) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLSD |

## Slice 99: `indra/newview/app_settings/shaders/class1/avatar/objectSkinV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 26–37 (n=12) | unknown/non-function (in vec4 weight4;) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge A5.4-1b] Previous frame's palette (velocity pass only). Donor: Black Dragon objectSkinV.glsl. Both the uniform and the function below are eliminated at link time in every rigged shader that doesn't call getLastObjectSkinnedTransform(), so non-velocity shaders are unaffec |
| 76–121 (n=46) | getLastObjectSkinnedTransform | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge A5.4-1b] Same blend as getObjectSkinnedTransform, against the PREVIOUS frame's palette. Called only by the velocity shaders' HAS_SKIN path. |

## Slice 99: `indra/newview/app_settings/shaders/class1/deferred/impostorV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–31 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/app_settings/shaders/class1/deferred/materialF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 30–40 (n=11) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 53–55 (n=3) | unknown/non-function (void main()) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/app_settings/shaders/class1/deferred/pbralphaF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–35 (n=11) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/app_settings/shaders/class1/deferred/treeV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 26–32 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/app_settings/shaders/class1/environment/waterV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 26–32 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/app_settings/shaders/class1/interface/debugV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 24–30 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/app_settings/shaders/class1/interface/highlightNormV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–31 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/app_settings/shaders/class1/interface/highlightSpecV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–31 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/app_settings/shaders/class1/interface/highlightV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–31 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/app_settings/shaders/class1/interface/normaldebugV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 23–30 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/app_settings/shaders/class1/interface/pathfindingNoNormalV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 24–30 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/app_settings/shaders/class1/interface/pathfindingV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 24–30 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/app_settings/shaders/class1/interface/solidcolorV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 24–30 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/app_settings/shaders/class1/objects/bumpV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 26–32 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/app_settings/shaders/class1/objects/impostorV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–31 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/app_settings/shaders/class1/objects/previewPhysicsV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 26–32 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/app_settings/shaders/class1/objects/simpleNoAtmosV.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–31 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/app_settings/shaders/class2/deferred/sunLightF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 34–56 (n=23) | cascadeDebugColor | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 58–69 (n=12) | main | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/app_settings/shaders/class2/deferred/sunLightSSAOF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 30–36 (n=7) | unknown/non-function (out vec4 frag_color;) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 38–53 (n=16) | cascadeDebugColor | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 55–66 (n=12) | main | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/app_settings/shaders/class3/deferred/screenSpaceReflPostF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 41–47 (n=7) | unknown/non-function (uniform sampler2D diffuseRect;) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 57–67 (n=11) | main | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [SSRFlagFix] The G-buffer flag lives in the RAW attachment's .w; decodeNormal() (globalF.glsl) never writes .w, so getNorm(tc).w was undefined here and the PBR branch below was decided on garbage. |
| 71–77 (n=7) | main | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/app_settings/shaders/class3/deferred/waterHazeF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 36–43 (n=8) | unknown/non-function (vec4 getWaterFogView(vec3 pos);) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 72–88 (n=17) | main | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/app_settings/shaders/class3/environment/underWaterF.glsl`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 23–42 (n=20) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 97–108 (n=12) | main | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Same BRDF statement as the above-water shader: this renderer gives water no diffuse lobe, so zero diffuse at K=1 is a real answer. Declared here primarily so this program cannot write UNDEFINED values to attachments 1 and 2 while the water pool has the guard open (hazard H1). |

## Slice 99: `indra/newview/featuretable.txt`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 65–70 (n=6) | unknown/non-function (RenderDeferred				1	1) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFSAAType |
| 112–117 (n=6) | unknown/non-function (RenderTreeLODFactor			1	0) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFSAAType |
| 153–158 (n=6) | unknown/non-function (RenderTreeLODFactor			1	0.5) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFSAAType |
| 192–197 (n=6) | unknown/non-function (RenderTreeLODFactor			1	0.5) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFSAAType |
| 232–237 (n=6) | unknown/non-function (RenderTreeLODFactor			1	0.5) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFSAAType |
| 272–277 (n=6) | unknown/non-function (RenderTreeLODFactor			1	0.5) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFSAAType |
| 312–317 (n=6) | unknown/non-function (RenderTreeLODFactor			1	0.5) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFSAAType |
| 352–357 (n=6) | unknown/non-function (RenderVolumeLODFactor		1	2.0) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFSAAType |
| 411–416 (n=6) | unknown/non-function (RenderReflectionProbeCount  0   0) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFSAAType |
| 420–425 (n=6) | unknown/non-function (list AMD) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFSAAType |

## Slice 99: `indra/newview/featuretable_linux.txt`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 65–70 (n=6) | unknown/non-function (RenderDeferred				1	1) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFSAAType |
| 112–117 (n=6) | unknown/non-function (RenderTreeLODFactor			1	0) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFSAAType |
| 153–158 (n=6) | unknown/non-function (RenderTreeLODFactor			1	0.5) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFSAAType |
| 192–197 (n=6) | unknown/non-function (RenderTreeLODFactor			1	0.5) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFSAAType |
| 232–237 (n=6) | unknown/non-function (RenderTreeLODFactor			1	0.5) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFSAAType |
| 272–277 (n=6) | unknown/non-function (RenderTreeLODFactor			1	0.5) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFSAAType |
| 312–317 (n=6) | unknown/non-function (RenderTreeLODFactor			1	0.5) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFSAAType |
| 352–357 (n=6) | unknown/non-function (RenderVolumeLODFactor		1	2.0) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFSAAType |
| 411–416 (n=6) | unknown/non-function (RenderReflectionProbeCount  0   0) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFSAAType |
| 423–428 (n=6) | unknown/non-function (list NVIDIA) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFSAAType |

## Slice 99: `indra/newview/featuretable_mac.txt`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 62–67 (n=6) | unknown/non-function (RenderDeferred				1	1) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFSAAType |
| 110–115 (n=6) | unknown/non-function (RenderTreeLODFactor			1	0) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFSAAType |
| 151–156 (n=6) | unknown/non-function (RenderTreeLODFactor			1	0.5) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFSAAType |
| 192–197 (n=6) | unknown/non-function (RenderTreeLODFactor			1	0.5) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFSAAType |
| 232–237 (n=6) | unknown/non-function (RenderTreeLODFactor			1	0.5) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFSAAType |
| 272–277 (n=6) | unknown/non-function (RenderTreeLODFactor			1	0.5) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFSAAType |
| 312–317 (n=6) | unknown/non-function (RenderTreeLODFactor			1	0.5) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFSAAType |
| 352–357 (n=6) | unknown/non-function (RenderVolumeLODFactor		1	2.0) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderFSAAType |

## Slice 99: `indra/newview/fsmaniptranslatejoint.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 231–238 (n=8) | FSManipTranslateJoint::restoreGL | unknown | unknown-review | unknown; no local conditional evidence | Edits LLImageGL |

## Slice 99: `indra/newview/fsposeranimator.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 32–50 (n=19) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1308–1473 (n=166) | FSPoserAnimator::loadPoseFileOntoAvatar | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [FileSafety] The base name is joined onto user_settings/poses unescaped; refuse traversal. Optional Black Dragon import: BD saves bone rotations as Euler [roll, yaw, pitch] (VX, VZ, VY) while Firestorm expects [roll, pitch, yaw] (VX, VY, VZ). When enabled, swap Y/Z on each rotati |

## Slice 99: `indra/newview/fsposeranimator.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 32–56 (n=25) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: <summary> Describes how to load a pose file. </summary> |
| 722–736 (n=15) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: <summary> Loads a saved pose file onto an avatar that is already being posed. </summary> <param name="avatar">The avatar to load the pose onto.</param> <param name="poseFileBaseName">The pose filename without the .xml extension.</param> <param name="loadMethod">The subset of pose |

## Slice 99: `indra/newview/llagentwearables.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1478–1485 (n=8) | LLAgentWearables::userRemoveMultipleAttachments, erase | unknown | unknown-review | unknown; no local conditional evidence | Edits LLViewerObject |

## Slice 99: `indra/newview/llaisapi.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1061–1078 (n=18) | AISUpdate::AISUpdate | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1095–1104 (n=10) | AISUpdate::checkTimeout | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1825–1827 (n=3) | unknown/non-function (void AISUpdate::doUpdate()) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/llaisapi.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 109–115 (n=7) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 162–168 (n=7) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/lldrawable.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 182–189 (n=8) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Every extents write (setSpatialExtents either overload, shift, via any pointer type) lands here: the world-shadow near cache must see it. |
| 284–297 (n=14) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge A5.4-1a] Previous-frame world/render matrix for the velocity pass. Read as the "last" object matrix by every velocity draw of the drawable; the current matrix is queued and written once at velocity-pass exit ([VelHistory], see LLRenderPass::updateLastVelocityModelMatrix) |

## Slice 99: `indra/newview/lldrawpoolwater.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 109–124 (n=16) | LLDrawPoolWater::beginPostDeferredPass | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [PerfB1] The copy is only consumed by underwater haze or visible water faces. |
| 185–193 (n=9) | LLDrawPoolWater::renderPostDeferred | unknown | unknown-review | unknown; no local conditional evidence | Edits gPipeline, LLRenderTarget |
| 214–233 (n=20) | LLDrawPoolWater::renderPostDeferred | unknown | unknown-review | unknown; no local conditional evidence | Edits LLShaderMgr, gPipeline |

## Slice 99: `indra/newview/lldynamictexture.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 203–232 (n=30) | LLViewerDynamicTexture::updateAllInstances | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [DynTexIdle] The shared targets are bound + cleared lazily, only once an instance actually needs to render. Binding/clearing both every frame with nothing to draw cost ~3-5 ms/frame of CPU (driver stalls) in Tracy. needsRender() has side effects, so it is still called exactly onc |
| 250–283 (n=34) | LLViewerDynamicTexture::updateAllInstances | unknown | unknown-review | unknown; conditional evidence present | Edits LLPipeline, LLAvatarAppearanceDefines |

## Slice 99: `indra/newview/llfetchedgltfmaterial.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 61–68 (n=8) | unknown/non-function (LLFetchedGLTFMaterial& LLFetchedGLTFMaterial::operator=(const LLFetchedGLTFMater) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLFetchedGLTFMaterial, LLViewerTexture |
| 74–92 (n=19) | LLFetchedGLTFMaterial::bind | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: BLEND shadows install their own coverage threshold before binding. A -1 effective cutoff means "do not clobber that threshold" there. |

## Slice 99: `indra/newview/llfetchedgltfmaterial.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 52–63 (n=12) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: alpha_cutoff_override - effective DrawInfo cutoff; [-1,1] overrides shared material alpha state, while values below -1 retain authored behavior. During BLEND shadow rendering -1 preserves the shadow pass's coverage threshold instead of uploading a material cutoff. |
| 90–92 (n=3) | unknown/non-function (protected:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/llfilepicker.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 67–73 (n=7) | unknown/non-function (LLFilePicker LLFilePicker::sInstance;) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 234–242 (n=9) | setupLoadFilter | unknown | unknown-review | unknown; no local conditional evidence | Edits LLFilePicker |
| 647–655 (n=9) | LLFilePicker::setupFilter | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1139–1147 (n=9) | LLFilePicker::navOpenFilterProc | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/llfilepicker.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 80–87 (n=8) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/llfloaterabout.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 131–136 (n=6) | LLFloaterAbout::postBuild | unknown | unknown-review | unknown; no local conditional evidence | Edits LLViewerTextEditor |
| 200–205 (n=6) | LLFloaterAbout::postBuild | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Get the names of supporters, extracted from .../doc/supporters.txt by viewer_manifest.py at build time |

## Slice 99: `indra/newview/llfloaterbuy.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 92–102 (n=11) | LLFloaterBuy::show | unknown | unknown-review | unknown; conditional evidence present | Edits LLViewerObject |

## Slice 99: `indra/newview/llfloaterbuycontents.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 89–99 (n=11) | LLFloaterBuyContents::show | unknown | unknown-review | unknown; conditional evidence present | Edits LLViewerObject |

## Slice 99: `indra/newview/llfloatercamerapresets.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 22–30 (n=9) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge B2] Rename UI for camera preset list items — donor: Black Dragon "Unlimited Camera Presets" UX (commit 152762d400). Gated by BDMergeCameraPresets; hidden when the gate is off. |
| 95–101 (n=7) | LLCameraPresetFlatItem::LLCameraPresetFlatItem | unknown | unknown-review | unknown; no local conditional evidence | Edits LLCameraPresetFlatItem |
| 111–122 (n=12) | LLCameraPresetFlatItem::postBuild | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge B2] Rename is a BDMergeCameraPresets-gated addition; the button stays hidden entirely (not just on mouse-leave) when the gate is off, so toggling the setting off restores stock Alchemy's list item exactly. |
| 132–140 (n=9) | LLCameraPresetFlatItem::onMouseEnter | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge B2] Rename only makes sense for non-default (renameable) presets. |
| 143–149 (n=7) | LLCameraPresetFlatItem::onMouseLeave | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 169–207 (n=39) | LLCameraPresetFlatItem::onRenameBtnClick, LLCameraPresetFlatItem::onRenameConfirm | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge B2] static [BDMerge B2] |

## Slice 99: `indra/newview/llfloatercamerapresets.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 22–30 (n=9) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge B2] Rename UI for camera preset list items — donor: Black Dragon "Unlimited Camera Presets" UX (commit 152762d400). Gated by BDMergeCameraPresets; hidden when the gate is off. |
| 67–79 (n=13) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge B2] |

## Slice 99: `indra/newview/llfloaterpreferencesgraphicsadvanced.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 38–44 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 52–58 (n=7) | LLFloaterPreferenceGraphicsAdvanced::LLFloaterPreferenceGraphicsAdvanced | unknown | unknown-review | unknown; no local conditional evidence | Edits LLFloaterPreferenceGraphicsAdvanced |
| 111–137 (n=27) | LLFloaterPreferenceGraphicsAdvanced::draw, LLFloaterPreferenceGraphicsAdvanced::postBuild | unknown | mixed/perf-review | unknown; conditional evidence present | Comment evidence: LLSliderCtrl writes an LLSD real by default. Normalize the persisted value back to the declared U32 setting type when this floater opens. [UIPerf] 4 Hz is plenty for a live status readout. |
| 171–182 (n=12) | LLFloaterPreferenceGraphicsAdvanced::refresh | unknown | unknown-review | unknown; conditional evidence present | Edits LLSliderCtrl, gGLManager |
| 202–293 (n=92) | LLFloaterPreferenceGraphicsAdvanced::onVRAMCapChanged, LLFloaterPreferenceGraphicsAdvanced::refreshVRAMStatus | unknown | mixed/perf-review | unknown; conditional evidence present | Comment evidence: [UIPerf] Resolve once; the widgets live for the floater's lifetime. Zero is the Auto sentinel. Keep every Custom value enforceable by the governor's documented 768 MB minimum instead of exposing 256/512 MB slider stops that would silently clamp elsewhere. Write explicitly as U32. |

## Slice 99: `indra/newview/llfloaterpreferencesgraphicsadvanced.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 38–44 (n=7) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: virtual*/ void draw(); |
| 52–59 (n=8) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 69–80 (n=12) | unknown/non-function (protected:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [UIPerf] VRAM status readout: widgets cached once (were looked up by name every frame) and the readout refreshed at 4 Hz instead of every frame. |

## Slice 99: `indra/newview/llfloaterreporter.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 321–331 (n=11) | LLFloaterReporter::getObjectInfo | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/llfloatersnapshot.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 24–53 (n=30) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge B3] Freeze World: while BDMergeFreezeWorld is enabled the stock freeze-frame checkbox is swapped for a Freeze World checkbox (bound to the runtime-only UseFreezeWorld toggle) that freezes the whole scene while the camera stays free - without the removed laggy fullscreen  |
| 267–278 (n=12) | LLFloaterSnapshotBase::ImplBase::updateLayout | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge B3] unless Freeze World is holding the scene: its own toggle owns FreezeTime for the duration of the freeze |
| 347–361 (n=15) | LLFloaterSnapshot::Impl::updateControls | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge C6] high-res override: save-to-disk custom resolution may exceed the stock cap when unlocked (donor: Black Dragon MAX_SNAPSHOT_IMAGE_SIZE raise, gated at runtime here). Other destinations (inventory/profile/postcard) keep the stock cap. |
| 775–805 (n=31) | LLFloaterSnapshot::Impl::updateResolution | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge A1.2] Resolution-aware autoscale: while BDMergeSnapshotAutoscale is enabled, re-derive the output/window height ratio every time the requested snapshot resolution changes and stash it in BDMergeSnapshotAutoscaleMultiplier (non-persisted, mirrors donor's RenderSnapshotMul |
| 1006–1015 (n=10) | LLFloaterSnapshotBase::~LLFloaterSnapshotBase | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge B3] fully release Freeze World (thaws avatars via the UseFreezeWorld listener) so no pause handles outlive the floater |
| 1045–1066 (n=22) | LLFloaterSnapshot::postBuild | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge C6] detachable big preview: PREVIEW button shares the refresh button's row; when the gate is on, shrink REFRESH (stock 167px wide) and reveal it (donor: Black Dragon fab3226ac5). Gate off: button stays hidden and overlapped, stock layout untouched. |
| 1075–1098 (n=24) | LLFloaterSnapshot::postBuild | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge B3] Freeze World (donor: Black Dragon b4cfc2cb83): when the gate is on, show the Freeze World checkbox in place of the stock freeze-frame checkbox (BD replaced freeze-frame with Freeze World and dropped the laggy fullscreen preview; both occupy the same slot here). The c |
| 1223–1272 (n=50) | LLFloaterSnapshot::onClose, LLFloaterSnapshot::onOpen | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge C6] remembered snapshot destination: restore the panel saved at last close (donor: Black Dragon RememberSnapshotMode, adapted to a persisted panel name so it also survives sessions) virtual [BDMerge C6] remember the destination panel for the next open/session and take th |
| 1278–1323 (n=46) | LLFloaterSnapshot::attachPreview, LLFloaterSnapshot::isPreviewVisible, LLFloaterSnapshot::onClickBigPreview | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge C6] detachable big preview (donor: Black Dragon fab3226ac5, double-toggle crash fix f0db8914d0 folded in: never die() the shared preview on hide) Toggle the preview [/BDMerge C6] |
| 1332–1347 (n=16) | LLFloaterSnapshotBase::onClose | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge B3] closing the snapshot floater always releases Freeze World (the listener thaws the avatars); like BD, the freeze lives only while its UI is reachable so the scene can never get stuck frozen [BDMerge A1.2] reset the autoscale multiplier so a high-res selection can't ke |

## Slice 99: `indra/newview/llfloatersnapshot.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 24–34 (n=11) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge C6] Snapshot conveniences: detachable big preview floater hookup and remembered snapshot destination panel, gated by BDMergeSnapshotExtras. donor: Black Dragon (NiranV Dean) fab3226ac5/f0db8914d0/16143bb98b. See doc/BD_MERGE_PATCHLOG.md. |
| 40–46 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLFloaterBigPreview |
| 158–164 (n=7) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: virtual*/ void onClose(bool app_quitting); // [BDMerge C6] |
| 166–176 (n=11) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge C6] detachable big preview (donor: Black Dragon fab3226ac5) |
| 183–192 (n=10) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge C6] detachable big preview floater (donor: Black Dragon) |

## Slice 99: `indra/newview/llfolderviewmodelinventory.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 38–44 (n=7) | LLFolderViewModelInventory::startDrag | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |

## Slice 99: `indra/newview/llhudeffectlookat.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 24–36 (n=13) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge B11] setLookAt() gains an opt-in gate (BDMergeHideLookAt) that stops this client's lookAt target from being broadcast (and locally animated) at all. Donor: Black Dragon (Kyler Eastridge) 82477c484d / 5785bfdb70 / aae33e4ef2 / 2c91aed07b. Note: Alchemy's pre-existing "Ena |
| 413–439 (n=27) | LLHUDEffectLookAt::setLookAt | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge B11] When enabled, stop sending (and locally animating) our own lookAt target entirely -- other clients no longer receive this client's focus target. Off by default: stock broadcast behavior is unchanged. Donor: Black Dragon 82477c484d5f605c44074c277230ea8d1a993fa2 (feat |

## Slice 99: `indra/newview/llhudeffecttrail.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 39–45 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 265–279 (n=15) | LLHUDEffectSpiral::render | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge HideUI] Selection/edit/script beams are particle effects, so hiding the 2D interface never touched them. Treat a hidden interface exactly like ShowSelectionBeam=off: kill the part source. Effects are re-sent continuously while active (edits, scripted effects), so beams r |

## Slice 99: `indra/newview/llhudicon.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–31 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 82–88 (n=7) | LLHUDIcon::render | unknown | unknown-review | unknown; conditional evidence present | Edits ALDerenderList |
| 194–200 (n=7) | LLHUDIcon::lineSegmentIntersect | unknown | unknown-review | unknown; conditional evidence present | Edits ALDerenderList |

## Slice 99: `indra/newview/llhudnametag.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 26–32 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 116–122 (n=7) | LLHUDNameTag::lineSegmentIntersect | unknown | unknown-review | unknown; conditional evidence present | Edits ALDerenderList |
| 233–239 (n=7) | LLHUDNameTag::renderText | unknown | unknown-review | unknown; conditional evidence present | Edits ALDerenderList |
| 559–565 (n=7) | LLHUDNameTag::updateVisibility | unknown | unknown-review | unknown; conditional evidence present | Edits ALDerenderList |

## Slice 99: `indra/newview/llhudtext.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 27–33 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 114–120 (n=7) | LLHUDText::renderText | unknown | unknown-review | unknown; conditional evidence present | Edits ALDerenderList |
| 354–360 (n=7) | LLHUDText::updateVisibility | unknown | unknown-review | unknown; conditional evidence present | Edits ALDerenderList |

## Slice 99: `indra/newview/llimview.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1040–1048 (n=9) | LLIMModel::LLIMSession::addMessage | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge ChatCap] Absolute index = position since session start = live count (mMsgs.size()) plus however many were already trimmed (mMsgTrimOffset). |
| 1050–1073 (n=24) | LLIMModel::LLIMSession::addMessage | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge ChatCap] Bound the in-RAM per-session backlog so a long, busy session (heavy group / nearby chat) doesn't accumulate an unbounded LLSD list that costs memory and slows shutdown teardown. The full transcript is already persisted to disk per message (logToFile), and re-ope |
| 1311–1317 (n=7) | LLIMModel::LLIMSession::addMessagesFromServerHistory | unknown | unknown-review | unknown; no local conditional evidence | Edits LLSD |
| 1335–1341 (n=7) | LLIMModel::LLIMSession::addMessagesFromServerHistory | unknown | unknown-review | unknown; no local conditional evidence | Edits LLSD |
| 1371–1377 (n=7) | LLIMModel::LLIMSession::loadHistory | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1646–1662 (n=17) | LLIMModel::getMessagesSilently | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge ChatCap] "index" is absolute (position since session start), so the count of messages at or after start_index is total-ever (live size + trimmed) minus start_index. Clamp to what is still physically in mMsgs: if the caller is further behind than the cap, the oldest wante |

## Slice 99: `indra/newview/llimview.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 141–156 (n=16) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge ChatCap] Number of oldest messages dropped from mMsgs by the in-RAM cap (BDMergeMaxSessionChatMessages). Every message's "index" is its absolute position since session start; when we trim the oldest off the back of mMsgs we bump this instead of re-indexing, so the invari |

## Slice 99: `indra/newview/llinspectobject.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 216–226 (n=11) | LLInspectObject::onOpen | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/llinventorybridge.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 59–65 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 96–103 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 116–254 (n=139) | category_items_pass, clear_inventory_drag_validation_cache | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Match collectDescendents(EXCLUDE_TRASH): the Trash category is part of the result, but its contents are not. Test the category itself before deciding whether to descend into it. Match the legacy test exactly: getType(), not getActualType(). Don't return yet: the legacy path rejec |
| 1422–1428 (n=7) | LLInvFVBridge::startDrag | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |
| 2672–2689 (n=18) | LLFolderBridge::getLabelSuffix | unknown | unknown-review | unknown; conditional evidence present | Edits LLInventoryModel, gInventory, LL_PROFILE_ZONE_NAMED_CATEGORY_UI |
| 2899–2910 (n=12) | LLFolderBridge::dragCategoryIntoFolder | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |
| 2938–2983 (n=46) | LLFolderBridge::dragCategoryIntoFolder | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 3119–3129 (n=11) | LLFolderBridge::dragCategoryIntoFolder | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: The drop path needs this materialized list below to deactivate gestures moved to Trash. Collect it once and reuse it. |
| 3138–3144 (n=7) | LLFolderBridge::dragCategoryIntoFolder | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 3159–3175 (n=17) | LLFolderBridge::dragCategoryIntoFolder | unknown | unknown-review | unknown; conditional evidence present | Edits LL_PROFILE_ZONE_NAMED_CATEGORY_UI |
| 3180–3186 (n=7) | LLFolderBridge::dragCategoryIntoFolder | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 3229–3235 (n=7) | LLFolderBridge::dragCategoryIntoFolder | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_NAMED_CATEGORY_UI |
| 3497–3513 (n=17) | LLFolderBridge::dragCategoryIntoFolder | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 5291–5297 (n=7) | LLFolderBridge::dragOrDrop | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |
| 6489–6545 (n=57) | check_category | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: One DFS. The former implementation collected every subtree and then recursively repeated those same collections for each child folder. Empty folder should be checked as any other folder view item. If we are filtering by date the folder should not pass because it doesn't have its  |
| 7635–7646 (n=12) | LLObjectBridge::openItem | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [DoubleClickAdd] Restores 366a7f2325d (lost in the 663df365ca6 history merge): DoubleClickAttachmentAdd adds instead of replacing; Ctrl inverts. |
| 8019–8033 (n=15) | LLWearableBridge::openItem | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [DoubleClickAdd] DoubleClickWearableAdd adds clothing (alphas, tattoos, layers) instead of replacing that type; Ctrl inverts. Body parts always replace. |

## Slice 99: `indra/newview/llinventorybridge.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 41–49 (n=9) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Clears validation derived during the current inventory drag gesture. |

## Slice 99: `indra/newview/llinventoryfunctions.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1482–1560 (n=79) | can_move_folder_to_marketplace | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Traverse each hierarchy directly instead of materializing complete category/item arrays. Counts are still resolved before selecting an error, preserving the legacy folder/item/stock tooltip priority. Clear those counts or they will be counted twice because we're already inside th |
| 1586–1625 (n=40) | can_move_folder_to_marketplace | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Now check that each item in the folder can be moved in the marketplace Preserve collectDescendents()'s depth-first category-then-item order while stopping on the first invalid item and without retaining the whole subtree in arrays. |
| 4153–4155 (n=3) | unknown/non-function (void LLInventoryAction::updateMarketplaceFolders()) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/llinventorygallery.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 358–368 (n=11) | LLInventoryGallery::draw | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/llinventorymodel.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 448–454 (n=7) | unknown/non-function (LLInventoryModel::LLInventoryModel()) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 2297–2303 (n=7) | LLInventoryModel::notifyObservers | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 5246–5248 (n=3) | unknown/non-function (void LLInventoryModel::FetchItemHttpHandler::processFailure(const char * const r) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/llinventorymodel.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 593–602 (n=10) | getRevision | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Monotonically increases for every observer-visible inventory change. Consumers may use this to invalidate derived inventory data without rebuilding it on every UI query. |
| 611–617 (n=7) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 751–753 (n=3) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/llinventorymodelbackgroundfetch.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 563–575 (n=13) | LLInventoryModelBackgroundFetch::incrFetchFolderCount | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Clamped on its own count, and named apart in the warning. This is the one isFolderFetchProcessingComplete reads, and it treats anything at or below zero as nothing outstanding, so a folder response arriving without a request behind it must not be allowed to push it under. |
| 748–766 (n=19) | LLInventoryModelBackgroundFetch::bulkFetchViaAis | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: The wide profile applies only while a recursive sweep is genuinely underway: it must have been started (login kicks it off) and not yet latched complete. Without the started test a plain on-demand folder fetch would also run wide, which is not what the settings promise. Floor the |
| 769–778 (n=10) | LLInventoryModelBackgroundFetch::bulkFetchViaAis | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Post-wearables the stock budget is 6 ms per idle tick; before that it stays 1 s either way. The preload budget is clamped to 1-100 ms so a mistyped setting cannot stall login frames. |
| 897–909 (n=13) | LLInventoryModelBackgroundFetch::bulkFetchViaAis | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Same activity test as the pool/idle-budget path above. |

## Slice 99: `indra/newview/llinventorypanel.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 418–428 (n=11) | LLInventoryPanel::draw | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1626–1632 (n=7) | LLInventoryPanel::onSelectionChange | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |

## Slice 99: `indra/newview/llkeyconflict.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 303–310 (n=8) | LLKeyConflictHandler::loadFromSettings | unknown | unknown-review | unknown; conditional evidence present | Edits LLSimpleXUIParser, LLViewerInput |
| 579–588 (n=10) | LLKeyConflictHandler::saveToSettings | unknown | unknown-review | unknown; conditional evidence present | Edits gSavedSettings |

## Slice 99: `indra/newview/lllocalmesh.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1554–1560 (n=7) | LLLocalMeshMgr::spawnLinkset | unknown | unknown-review | unknown; no local conditional evidence | Edits LLViewerObject |

## Slice 99: `indra/newview/llmediactrl.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 844–855 (n=12) | LLMediaCtrl::draw | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [WebIdle] this page is on screen: keep it (or wake it) out of the paused state |

## Slice 99: `indra/newview/llmeshrepository.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 29–36 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | unknown | mixed/perf-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 2031–2078 (n=48) | LLMeshRepoThread::fetchMeshLOD | unknown | mixed/perf-review | unknown; conditional evidence present | Comment evidence: [BDMerge G5.1-2b] decoded mesh pool short-circuit: a hit skips the disk read AND the inflate + LLSD parse + face build in lodReceived. Serves a deep copy; replicates lodReceived's post-unpack delivery. |
| 2430–2439 (n=10) | LLMeshRepoThread::lodReceived | unknown | mixed/perf-review | unknown; no local conditional evidence | Comment evidence: [BDMerge G5.1-2b] write-through into the decoded mesh pool, pre-rigging (hits re-run rigging against current avatar state) |
| 4554–4562 (n=9) | LLMeshRepository::notifyLoadedMeshes | unknown | mixed/perf-review | unknown; no local conditional evidence | Comment evidence: [BDMerge G5.1-2b] main-thread settings/budget refresh for the mesh pool |

## Slice 99: `indra/newview/llpaneleditwater.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 33–40 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 56–75 (n=20) | enableCustomWaterLevelControls | unknown | unknown-review | unknown; no local conditional evidence | Edits LLView |
| 119–134 (n=16) | LLPanelSettingsWaterMainTab::postBuild, setCommitCallback | unknown | unknown-review | unknown; no local conditional evidence | Edits LLUICtrl, LLSD, gSavedSettings, RenderWaterLevel, LLViewerRegion, gAgentCamera, RenderWaterLevelOverride |
| 156–171 (n=16) | LLPanelSettingsWaterMainTab::refresh | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: The custom level is a client-side setting, independent of the asset. |

## Slice 99: `indra/newview/llpanellogin.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 77–84 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 99–121 (n=23) | erase, normalized_login_name | unknown | unknown-review | unknown; conditional evidence present | Edits LLStringUtil |
| 725–787 (n=63) | LLPanelLogin::selectSavedUser | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: static setFields() is the ordinary combo-selection path. It keeps the authenticator inside the viewer credential and exposes only whether the password field received its fixed asterisk filler. |
| 1501–1507 (n=7) | LLPanelLogin::draw | unknown | unknown-review | unknown; no local conditional evidence | Edits LL_PROFILE_ZONE_SCOPED_CATEGORY_UI |

## Slice 99: `indra/newview/llpanellogin.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 61–70 (n=10) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Select a remembered login exactly as the username combo does. This intentionally never exposes the credential or authenticator to callers. |

## Slice 99: `indra/newview/llpanelmaininventory.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 35–42 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 65–71 (n=7) | unknown/non-function (class LLPanelMainInventory : public LLPanel, LLInventoryObserver) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 80–88 (n=9) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 196–202 (n=7) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 239–246 (n=8) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/llpanelsnapshotlocal.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 24–34 (n=11) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge C6] high-res override: reveal the "Allow high resolutions" checkbox while BDMergeSnapshotExtras is enabled. donor: Black Dragon (NiranV Dean) SnapshotResolutionUnlock checkbox in panel_snapshot_local. See doc/BD_MERGE_PATCHLOG.md. |
| 90–102 (n=13) | LLPanelSnapshotLocal::postBuild | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge C6] high-res override checkbox, hidden unless the gate is on (donor: Black Dragon "Allow High Resolutions" in panel_snapshot_local) |

## Slice 99: `indra/newview/llphysicsmotion.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 105–111 (n=7) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 191–197 (n=7) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLDriverParam |
| 231–243 (n=13) | LLPhysicsMotion::initialize | unknown | unknown-review | unknown; conditional evidence present | Edits LLDriverParam, LL_WARNS, LL_ENDL |
| 672–691 (n=20) | LLPhysicsMotion::onUpdate | unknown | unknown-review | unknown; conditional evidence present | Edits LLDriverParam, LLViewerVisualParam |

## Slice 99: `indra/newview/llpresetsmanager.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 24–34 (n=11) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge B2] Camera presets: auto-local-copy-on-edit + rename, layered on this file's existing LL preset storage — donor: Black Dragon "Unlimited Camera Presets" (llagentcamera.cpp / llfloaterpreference.cpp), commit 152762d400 (2018-12-03), moved in 4ff6498a7e (2020-07-06). Gated |
| 86–96 (n=11) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge B2] True in-place rename for camera presets (Alchemy/LL stock only offered save-as-new + delete-old). No-ops when BDMergeCameraPresets is off. Donor: Black Dragon camera preset UX, see llpresetsmanager.cpp for details. |

## Slice 99: `indra/newview/llpreviewanim.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 50–58 (n=9) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLUUID |

## Slice 99: `indra/newview/llscenemonitor.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 433–445 (n=13) | LLSceneMonitor::calcDiffAggregate | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: gGL, not raw glColorMask: a raw call bypasses LLRender's cache (so the next gGL.setColorMask with matching cached factors is skipped and the mask silently stays wrong) and also bypasses the indexed draw-buffer guard's reassert. This path is not currently inside the post-deferred  |
| 470–476 (n=7) | LLSceneMonitor::calcDiffAggregate | unknown | unknown-review | unknown; no local conditional evidence | Edits gGL |

## Slice 99: `indra/newview/llselectmgr.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 245–254 (n=10) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLVector3, LLQuaternion, LLMatrix4 |

## Slice 99: `indra/newview/llsnapshotlivepreview.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–35 (n=11) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge C6] gated high-res snapshot limit: the max-image-size clamps below use bdmerge_max_snapshot_image_size() (llviewerwindow.h) instead of the compile-time MAX_SNAPSHOT_IMAGE_SIZE. donor: Black Dragon (NiranV Dean) MAX_SNAPSHOT_IMAGE_SIZE raise. See doc/BD_MERGE_PATCHLOG.md. |
| 118–124 (n=7) | LLSnapshotLivePreview::LLSnapshotLivePreview | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 141–147 (n=7) | LLSnapshotLivePreview::setMaxImageSize | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/llsurface.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 247–255 (n=9) | LLSurface::initTextures | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: The region caches the effective visual height before surface creation, so a newly discovered neighbor never flashes at the 20 m default. |
| 1142–1148 (n=7) | unknown/non-function (void LLSurface::dirtySurfacePatch(LLSurfacePatch *patchp)) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLSurface |
| 1150–1156 (n=7) | LLSurface::setWaterHeight | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 1173–1175 (n=3) | unknown/non-function (F32 LLSurface::getWaterHeight() const) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/llsurface.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 118–124 (n=7) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/lltexturecache.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 37–49 (n=13) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 250–366 (n=117) | run, runReadBody, ~ALCacheIOJob | unknown | mixed/perf-review | unknown; conditional evidence present | Comment evidence: [BDMerge G5.4] Parallel cache body I/O.  LLTextureCache is a single LLWorkerThread; stock doRead/doWrite perform synchronous file I/O inline, serializing every cache read and write on one thread (measured: 781ms mean / 5s p95 read latency under load on NVMe — pure queueing). When |
| 392–398 (n=7) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits ALCacheIOJob |
| 570–632 (n=63) | LLTextureCacheRemoteWorker::doRead, getQueue | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge G5.4] pooled path: post the body read and poll for completion so this worker thread can keep dispatching other requests meanwhile. No body: deliver whatever the header stage produced (stock behavior) |
| 850–908 (n=59) | LLTextureCacheRemoteWorker::doWrite, getQueue | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge G5.4] pooled path: the job owns a copy of the body bytes, so worker teardown can never race the pool thread. |
| 1042–1056 (n=15) | LLTextureCache::~LLTextureCache | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge G5.4] drain and stop the body-I/O pool; jobs are self-contained so any still-running ones just complete and free |
| 1237–1253 (n=17) | LLTextureCache::initCache | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: [BDMerge G5.4] main thread: size and start the cache body-I/O pool |

## Slice 99: `indra/newview/lltexturefetch.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 33–40 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | unknown | mixed/perf-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 1136–1160 (n=25) | LLTextureFetchWorker::doWork | unknown | mixed/perf-review | unknown; conditional evidence present | Comment evidence: [BDMerge G5.1] decoded RAM pool short-circuit: a hit at the desired discard or finer skips the encoded cache read AND the J2K decode. The encoded disk cache was already written when this data first decoded, so persistence is not bypassed. |
| 1863–1873 (n=11) | LLTextureFetchWorker::doWork | unknown | mixed/perf-review | unknown; conditional evidence present | Comment evidence: [BDMerge G5.1] write-through into the decoded RAM pool |
| 2524–2531 (n=8) | LLTextureFetch::createRequest | unknown | mixed/perf-review | unknown; no local conditional evidence | Comment evidence: [BDMerge G5.1] main-thread settings/budget refresh for the decoded pool |
| 2816–2822 (n=7) | LLTextureFetch::getRequestFinished | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 2830–2836 (n=7) | LLTextureFetch::getRequestFinished | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 2847–2857 (n=11) | LLTextureFetch::getRequestFinished | unknown | mixed/perf-review | unknown; no local conditional evidence | Comment evidence: [BDMerge G5.1-S1] session-level attribution for the decoded-pool spike |

## Slice 99: `indra/newview/lltextureview.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 601–609 (n=9) | LLGLTexMemBar::draw | unknown | unknown-review | unknown; no local conditional evidence | Edits LLImageGL, LLVertexBuffer, LLRenderTarget, LLViewerTexture |
| 632–663 (n=32) | LLGLTexMemBar::draw | unknown | mixed/perf-review | unknown; no local conditional evidence | Comment evidence: required height (see getRequiredRect) covers all eight lines of text below. required height (see getRequiredRect) covers all nine lines of text below. |
| 671–683 (n=13) | LLGLTexMemBar::draw | unknown | unknown-review | unknown; no local conditional evidence | Edits LLViewerTexture |
| 791–798 (n=8) | LLGLTexMemBar::getRequiredRect | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: draw() renders eight lines of text at v_offset + line_height * (1..8). draw() renders nine lines of text at v_offset + line_height * (1..9). |
| 1177–1179 (n=3) | unknown/non-function (bool LLTextureView::handleKey(KEY key, MASK mask, bool called_from_parent)) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/lltoastpanel.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 198–205 (n=8) | LLCheckBoxToastPanel::setCheckBoxes | unknown | unknown-review | unknown; no local conditional evidence | Edits LLNotifications, LLCheckBoxToastPanel |

## Slice 99: `indra/newview/lltoolgrab.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 41–47 (n=7) | unknown/non-function (class LLPickInfo;) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLViewerObject, LLPickInfo, LLVector3 |
| 86–95 (n=10) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Functional self-test only: forget an in-progress grab WITHOUT sending anything (no ObjectDeGrab / ObjectGrabUpdate).  Used when its mouse capture was dropped. |

## Slice 99: `indra/newview/lltoolpie.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 828–839 (n=12) | LLToolPie::handleHover | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: Functional self-test (in-world): no hover pick, hover object, hover parcel or media hover (each can message the simulator).  Belt and braces with LLViewerWindow::updateUI. |
| 987–1016 (n=30) | needs_tooltip | unknown | unknown-review | unknown; conditional evidence present | Comment evidence: <SK:Nexii> Restore the interactivity test SL-11532 removed so ShowAllObjectHoverTip is honored again </SK:Nexii> |
| 1430–1436 (n=7) | LLToolPie::handleToolTip | unknown | unknown-review | unknown; conditional evidence present | Edits gViewerInput |

## Slice 99: `indra/newview/llvelopack.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 914–920 (n=7) | on_first_run | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/llviewercontrol.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 43–50 (n=8) | unknown/non-function (bool toggle_show_favorites_panel(const LLSD& newvalue);) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Monotonic entry counter, including attempts rejected by the shader manager. |

## Slice 99: `indra/newview/llviewerinventory.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 739–745 (n=7) | LLViewerInventoryCategory::setFetching | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/llviewerjoint.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 30–36 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 95–102 (n=8) | LLViewerJoint::render | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Repeated transparent draws reuse the palette and never split bakes. |

## Slice 99: `indra/newview/llvieweroctree.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 361–374 (n=14) | LLViewerOctreeEntryData::setSpatialExtents | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 392–398 (n=7) | LLViewerOctreeEntryData::shift | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 776–783 (n=8) | LLViewerOctreeGroup::setVisible | unknown | unknown-review | unknown; conditional evidence present | Edits LLViewerCamera |

## Slice 99: `indra/newview/llvieweroctree.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 149–157 (n=9) | onSpatialExtentsChanged | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Called after EVERY write of the entry's extents (both setSpatialExtents overloads and shift), whatever the static type of the caller: LLDrawable overrides it to invalidate the world-shadow bounds cache. Keep it O(1). |

## Slice 99: `indra/newview/llviewerregion.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 148–156 (n=9) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 373–381 (n=9) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [ProbeOnDemand] probe_id: the object's UUID when known; a changed / replaced / new cache entry then counts as an authoritative update for the probe recorder. |
| 587–597 (n=11) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Preserve the simulator value so disabling the session override restores it exactly. All existing consumers continue to read getWaterHeight(). |

## Slice 99: `indra/newview/llviewertextureanim.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 40–46 (n=7) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 60–66 (n=7) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/llviewertexturelist.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 28–34 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 132–140 (n=9) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 236–242 (n=7) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits ALVRAMRecovery, LLTextureKey |

## Slice 99: `indra/newview/llviewerwindow.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 151–162 (n=12) | unknown/non-function (struct MainPanel : public LLPanel) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge C6] runtime max snapshot side length: 12288 when the high-res override (BDMergeSnapshotExtras + BDMergeSnapshotResolutionUnlock) is on, otherwise stock MAX_SNAPSHOT_IMAGE_SIZE. donor: Black Dragon b4cfc2cb83-era constant raise (12228 in llviewerwindow.h), made a gated ru |
| 193–213 (n=21) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge HideUI] True when in-world INDICATOR overlays (selection outlines + manipulators, selection/edit particle beams, beacon lines, face highlights) should draw. False when the interface is hidden by EITHER mechanism: the HideUIControls / mouselook path (mUIVisible) or the Ad |
| 516–522 (n=7) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 601–610 (n=10) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: Active Frame format rectangle inside a width x height view (any units, same origin); false when the format is Off / Native. Defined in llviewerwindow.cpp. |

## Slice 99: `indra/newview/llvopartgroup.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 25–31 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 334–342 (n=9) | LLVOPartGroup::updateGeometry | unknown | unknown-review | unknown; conditional evidence present | Edits ALDerenderList |
| 412–420 (n=9) | LLVOPartGroup::lineSegmentIntersect | unknown | unknown-review | unknown; conditional evidence present | Edits ALDerenderList |

## Slice 99: `indra/newview/llvovolume.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 55–63 (n=9) | unknown/non-function (enum LLVolumeInterfaceType) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge Batch4] Machinima High-LOD: raised object-LOD-factor ceiling used only while BDMergeMachinimaHighLOD is on (stock stays clamped to MAX_LOD_FACTOR). |
| 147–153 (n=7) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLVector3 |
| 397–407 (n=11) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [ProbeOnDemand] MS: true when a mesh / sculpt (re)build is a pure LOD / refinement of an asset this volume already built (same sculpt id + type, ready). Marks the pending rebuild as a LOD arrival. Only meaningful while RenderProbeOnDemand is ON; never writes anything when recordi |
| 500–518 (n=19) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [ProbeOnDemand] Built-asset identity for the probe dirty recorder (MS / H4). Written ONLY while RenderProbeOnDemand is ON. |

## Slice 99: `indra/newview/llworld.h`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 35–41 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 154–160 (n=7) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 211–217 (n=7) | unknown/non-function (public:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 237–245 (n=9) | unknown/non-function (private:) | unknown | unknown-review | unknown; no local conditional evidence | Edits LLFrameTimer |

## Slice 99: `indra/newview/rlveffects.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 329–343 (n=15) | RlvSphereEffect::setShaderUniforms | unknown | unknown-review | unknown; conditional evidence present | Edits gPipeline |

## Slice 99: `indra/newview/rlvhandler.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 3297–3304 (n=8) | onCommand | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/rlvlocks.cpp`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 495–501 (n=7) | RlvAttachmentLockWatchdog::detach | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |
| 523–530 (n=8) | RlvAttachmentLockWatchdog::detach | unknown | unknown-review | unknown; conditional evidence present | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/skins/default/textures/alchemy/alchemy_128.png`

binary/mode-only/deletion without textual hunks; review file-level delta

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|

## Slice 99: `indra/newview/skins/default/textures/windows/startup_logo.png`

binary/mode-only/deletion without textual hunks; review file-level delta

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|

## Slice 99: `indra/newview/skins/default/xui/de/floater_adjust_environment.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–28 (n=28) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/skins/default/xui/de/strings.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 31–39 (n=9) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/skins/default/xui/en/floater_about.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 39–61 (n=23) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 79–108 (n=30) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 110–192 (n=83) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 195–202 (n=8) | unknown/non-function (Dummy Name replaced at run time) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 206–212 (n=7) | unknown/non-function (Dummy Name replaced at run time) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 214–227 (n=14) | unknown/non-function (with open source contributions from:) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/skins/default/xui/en/floater_preferences_view_advanced.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 315–321 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 328–334 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/skins/default/xui/en/floater_preview_animation.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–11 (n=11) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 66–101 (n=36) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 106–112 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 144–152 (n=9) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/skins/default/xui/en/floater_snapshot.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 84–113 (n=30) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge C6] detachable big preview toggle (donor: Black Dragon |
| 225–244 (n=20) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge B3] Freeze World (donor: Black Dragon b4cfc2cb83). |

## Slice 99: `indra/newview/skins/default/xui/en/menu_fs_poser_poses_btn.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 32–43 (n=12) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/skins/default/xui/en/panel_camera_preset_item.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 39–63 (n=25) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge B2] Rename button; hidden unless BDMergeCameraPresets is on |

## Slice 99: `indra/newview/skins/default/xui/en/panel_inbox_inventory.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 13–19 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/skins/default/xui/en/panel_preferences_interface.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 60–66 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 101–107 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 151–166 (n=16) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 292–298 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/skins/default/xui/en/panel_presets_camera_pulldown.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 378–384 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 392–398 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/skins/default/xui/en/panel_prim_media_controls.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 373–379 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/skins/default/xui/en/panel_settings_water.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 144–174 (n=31) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits RenderWaterLevelOverride, RenderWaterLevel |

## Slice 99: `indra/newview/skins/default/xui/en/panel_snapshot_local.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 197–217 (n=21) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Comment evidence: [BDMerge C6] high-res override toggle (donor: Black Dragon "Allow High |

## Slice 99: `indra/newview/skins/default/xui/es/floater_adjust_environment.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–28 (n=28) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/skins/default/xui/es/strings.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 22–30 (n=9) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/skins/default/xui/fr/floater_adjust_environment.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–28 (n=28) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/skins/default/xui/fr/strings.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 31–39 (n=9) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/skins/default/xui/it/floater_adjust_environment.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–28 (n=28) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/skins/default/xui/it/strings.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 28–36 (n=9) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/skins/default/xui/ja/floater_adjust_environment.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 9–16 (n=8) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 99–104 (n=6) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/skins/default/xui/ja/strings.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 37–45 (n=9) | unknown/non-function (that are returned from one component and may appear in many places) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/skins/default/xui/pl/floater_adjust_environment.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–7 (n=7) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |
| 85–90 (n=6) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/skins/default/xui/pt/floater_adjust_environment.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–28 (n=28) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/skins/default/xui/pt/strings.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 22–30 (n=9) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/skins/default/xui/ru/floater_adjust_environment.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–28 (n=28) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/skins/default/xui/ru/strings.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 31–39 (n=9) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/skins/default/xui/tr/floater_adjust_environment.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–28 (n=28) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/skins/default/xui/tr/strings.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 31–39 (n=9) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/skins/default/xui/zh/floater_adjust_environment.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 1–28 (n=28) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

## Slice 99: `indra/newview/skins/default/xui/zh/strings.xml`

HEAD coordinates; guards/features are candidate evidence

| HEAD range | Function(s) / context | Feature candidates | Scope | Guard status | Purpose evidence |
|---|---|---|---|---|---|
| 31–39 (n=9) | unknown/non-function (declaration, XML, build list or global scope) | unknown | unknown-review | unknown; no local conditional evidence | Edits declarations/data/build wiring; purpose unknown |

