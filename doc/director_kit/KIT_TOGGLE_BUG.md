# Director v2 kit toggle click bug

## Root cause

The six reported controls are `ALKitSwitch`/`LLCheckBoxCtrl`, generated as
`<kit_switch>` by `scripts/code_tools/director_kit_gen.py:48-49` and declared in
`panel_dpage_light_cine_light_rig.xml:118-119,191-192,223-224,269-270,511-512,549-550`.
They are not legacy checkboxes and there is no duplicate widget: the functional
trace resolves both events to their one child `CheckboxCtrl Button`.

`LLButton::handleMouseDown()` captures the mouse (`indra/llui/llbutton.cpp:498-530`).
`LLButton::handleMouseUp()` toggles and commits only inside its
`hasMouseCapture()` branch and only for a release in the button's current rect
(`indra/llui/llbutton.cpp:543-585`); its no-capture branch merely forwards to
children (`:587-590`). The failed rows report a handled up but zero commits: the
release no longer satisfied that capture/current-rect gate. The event can still
be hit-tested back to the same button, which explains the reported up handler
without a commit. The CSV's control rectangle is captured before dispatch, so
containment in that rectangle does not prove containment in the release-time
rect. This also rules out an owner refresh: no parent commit signal fired.

The page master and card enable switches (`cine_enabled`, `cine_catchlight`)
retained a valid release state and followed the normal `:546-585` path. The
failure therefore followed placement/input state, not setting binding or a
different widget class.

## Fix

`widgets/kit_switch.xml:5-7` now opts every `ALKitSwitch` into capture-loss
commit and original-press-rect release handling. If capture is displaced during
an armed click, `LLButton::onMouseCaptureLost()` toggles and commits
(`indra/llui/llbutton.cpp:1320-1334`). On an ordinary mouse-up,
`handleMouseUp()` resets the down timer before releasing capture (`:548-553`),
so the capture-loss callback is inert and the normal path commits exactly once.
If layout moved the captured button, release at the physical press affordance is
accepted against the screen rect saved on down. Both LLButton options default
false (`indra/llui/llbutton.cpp:113-115,185-186`), so stock buttons, v1
checkboxes, and other widgets retain the exact old predicate. The generator
already maps every v2 toggle to `kit_switch`; no generated-page special case is
needed.

## Drawing and adjacent failures

The switch has no intermediate visual state: `ALKitSwitch::draw()` reads
`isOn()` once and uses that same boolean for the track, knob colour, and knob x
position (`indra/newview/alkitswitch.cpp:40-57`). USER_BUGS #1 was the old focus
outline overlapping the track; the current whole-track outline is outside the
track (`:59-67`) and the knob remains value-derived.

The three disabled Actor-only rows are a harness-precondition result over a
real product gate. Object-targeted rigs disable all Actor-only controls, and
Actor-only-all disables each per-light control without changing its stored
value (`indra/newview/alpanelcinelightrig.cpp:2408-2448`). The run left the
selected rig in that gated state; this is not a click defect.

The `cine_easy_bg` and `cine_rig_rim_tron_color` failures occur before input:
`settleFunctionalReveal()` retries the harness reveal and fails after six
frames when the control remains hidden
(`indra/newview/aluiperfselftest.cpp:4020-4042`). Both controls have complete
product wiring (`alpanelcinelightrig.cpp:2213-2228` and the direct
`CineRigRimTronColorSource` binding in `panel_dpage_light_rim.xml:188-189`).
They are reveal/selector harness failures, not product combo failures; no
product change is warranted from this run.
