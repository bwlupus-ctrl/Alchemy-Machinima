# Director v2 kit toggle click bug

## Proven root cause

The affected controls are `ALKitSwitch`/`LLCheckBoxCtrl` instances whose input
lands on the single child `CheckboxCtrl Button`. `LLButton::handleMouseDown()`
first captures the mouse and then takes keyboard focus for a tab-stop button
(`indra/llui/llbutton.cpp:503-507`), before its normal mouse-down callbacks run
(`:520-524`). Focus propagation calls the callbacks registered on that button
and its control ancestors (`indra/llui/llfocusmgr.cpp:258-267`).

Two focus callbacks run on that path. `ALKitRow` restyles the editor and updates
the contextual help (`indra/newview/alkitrow.cpp:182-189`), but
`ALKitHelp::show()` only calls `setText()` on already-sized children
(`indra/newview/alkithelp.cpp:167-180`); it does not reshape the help panel,
cards, page, or scroll document. Card body/More state is likewise not changed by
focus, and `ALKitPage::scrollToView()` is deferred and only used by explicit
reveal/navigation actions (`indra/newview/alkitpage.cpp:1128-1147`).

The synchronous mover is the focus-reveal callback installed by
`ALScrollFocus`: focus immediately converts the control rect to document space
and calls `LLScrollContainer::scrollToShowRect()`
(`indra/newview/alscrollfocus.h:107-122`). That function clamps a new scrollbar
position and synchronously calls `updateScroll()`
(`indra/llui/llscrollcontainer.cpp:802-837`), which changes the scroll
document's origin while the button is still pressed. The 15:21 functional run
proves the result: `cine_mirror` moved 76 px, `cine_bounce` 138 px,
`cine_manual` 16 px, `cine_random_cycle` 10 px, and `cine_gizmo` moved 14 px in
the opposite direction between down and up. `cine_key_shaft`, `cine_bg_shaft`,
`cine_enabled`, and `cine_catchlight` were already inside the focus-reveal
constraint, so clamping retained the current scroll position and they did not
move. The difference is viewport placement, not help length, card state, or
setting binding.

`LLButton::handleMouseUp()` intentionally commits only when the release remains
inside the button's current rect (`indra/llui/llbutton.cpp:540-581`). Once the
document moved, the unchanged screen coordinate was outside that new rect, so
the captured mouse-up was handled without toggling or committing.

## Root fix

`ALScrollFit::FocusTracker` now identifies an `ALKitScroller` and installs its
focus reveal with pointer reveal disabled, while legacy and stock scrollers keep
the previous behavior (`indra/newview/alscrollfit.cpp:344-353`). The shared
focus callback detects the mouse capture already established by the pressed kit
control and skips only that synchronous reveal; keyboard/Tab focus still scrolls
the control into view (`indra/newview/alscrollfocus.h:87-122`). Thus a pointer
press cannot move the v2 document before release, with no draw-time work.

The unsafe symptom masks are removed. `kit_switch.xml` no longer opts into
`commit_on_capture_lost`, so capture loss from Alt-Tab, Escape/floater close,
modal opening, or capture stealing cannot toggle a latched switch. The
`commit_on_original_rect` parameter, stored press rect, and alternate mouse-up
predicate have also been removed from `LLButton`; `llbutton.cpp/.h` are restored
to their `a33602c4107` behavior.

## Regression oracle

`functionalPointerClick()` now records the target screen rect immediately
before the down event and again immediately before mouse-up
(`indra/newview/aluiperfselftest.cpp:3037-3076`). Every pointer click requires
the rects to match; a mismatch fails with `target moved during click (layout
shift)` (`:3128-3133`). This detects the bug even if a future input workaround
would otherwise make the setting commit.

The unrelated Actor-only gated controls and hidden selector/reveal fixtures from
the same run remain harness/precondition results, not toggle input defects.
