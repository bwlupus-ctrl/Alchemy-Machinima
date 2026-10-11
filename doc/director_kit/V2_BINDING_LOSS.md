# Director v2 “binding loss”: not a product bug

## Corrected diagnosis

The 40 rows previously classified as Director v2 setting-binding loss are a functional-harness artifact. The harness changed raw `LLSlider` and `LLColorSwatchCtrl` widget values programmatically and then called `onCommit()`. That is not a real user input path, and `onCommit()` emits the commit signal without retroactively writing the widget's bound control.

Shipping input paths already persist the setting through the widget. Slider thumb drag, track click, arrow keys, and typed slider input reach `setValue(..., from_event=false)` / `setControlValue()` (`indra/llui/llslider.cpp:142-148,233-363`; `indra/llui/llsliderctrl.cpp:442,458,565`). The color picker writes the swatch's control in `LLColorSwatchCtrl::onColorChanged()` (`indra/newview/llcolorswatch.cpp:327`). Real users therefore do not lose these values.

The attempted product bridge was reverted. It duplicated a contract the widgets already satisfy on real input and caused a real regression: committing one control could disturb unrelated selection state, including `scope_radio` / `ActorMoverSync` deselection.

## Harness correction

- Raw `LLSlider` controls now use the same pointer thumb-drag helper as `LLSliderCtrl`, followed only by target-local `handleKeyHere()` nudges to land exactly on the increment grid.
- Color swatches still open the real picker, then model its picker-change callback with `set(color, true, false)` and `onCommit()`. Results remain labelled `programmatic_picker_commit` and require the bound-setting oracle.
- `am_test_combo` is a self-resetting command, not durable widget state. Its listener intentionally returns `CineLightRigActorMaskTest` to Idle (`alactormaskdiagnostics.cpp:423-430`); the login harness skips it with a specific reason because an effect oracle requires an active actor-only light and rendered diagnostic captures.
- The only remaining exercise-time `setValue()` + `onCommit()` is the generic programmatic fallback. It is explicitly recorded as `programmatic_commit_path = "generic fallback"`; current generated RUN rows are handled by concrete pointer, keyboard, picker, spinner, dial, combo, radio, toggle, or text paths before that fallback. Pre-state and restore writes are not exercises.

## Reclassified rows

All 40 former category-D rows are HARNESS: Focus (6), Filters (15), Image (1), Bloom & Glow (4), Atmosphere (2), Shadows (7), and Tron (4), plus Actor Mask (1). S-Log EV/toe and Tron grade tint are the same programmatic-path artifact and are HARNESS as well. `switcher_selection_order` remains a separate harness pre-state issue.
