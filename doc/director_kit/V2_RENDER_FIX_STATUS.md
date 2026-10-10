# Director v2 render fix status

Implemented on `feature/director-kit-p0` without building or committing.

## Rendering fixes

- Lazy page hosts now fill both the generated logic wrapper and its inner `ALKitPage` at build and reshape; `adoptPage` reinforces the same contract.
- Kit scroll documents use the vertical-scroll viewport width, preventing horizontal scrollbars and the stray full-width bar.
- Page selection seeds the help pane/strip from the page title and summary.
- The rail is 176/56 with left-aligned icons, reserved label space, active amber label/dot, and 980 px folding; help switches panel/strip at 1100 px.
- Capture remains the eighth approved group with its Time and Scene pages in the v2 data generator, generated rail/tree/index, generated page files, and the v2 C++ group table. Only the empty `capture_settings_tab` page is dropped, leaving 35 page contracts.
- The roster has fixed header/tools/group/hint regions, an elastic contained cast list, compact icon tools, and a live `CAST n` header.
- `ALKitBar` restores XUI declaration order and only collapses direct children. The transport now has one explicit REC item and compact ACTION/arm/play/stop/timecode/scene/Load/Save/.../marks/paths/help/status/Cast/UI/? slots.
- Page tabs use measured compact widths, remain one row, and expose arrows only for real overflow; Easy/Advanced remains reserved at the right.

## Live failure gates

`-DirectorV2` now records fatal layout findings for lazy-page attachment/visibility/host size/cards/help, floater and roster containment, rail icon/label collision, the exact eight approved rail groups (including Capture), transport order/containment/overlap/text at 850/900/1160 in idle and record states, and kit-scroller containment/height/horizontal overflow. The PowerShell driver fails on any such finding and on any missing PNG for a page/size pair.

## Verification handoff

- The Capture correction restores the 35-page / 1,839-row generator assertions and 31 serialized legacy-page parity fixtures.
- Regeneration and gates are left to the orchestrator as requested.
- Not run by request: build, `/Zs`, viewer/live screenshot execution, commit.
