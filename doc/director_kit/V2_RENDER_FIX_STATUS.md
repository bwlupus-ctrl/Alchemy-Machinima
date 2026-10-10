# Director v2 render fix status — Round 3

Implemented on `feature/director-kit-p0` without generating, building, or committing.

## Part A blocker ledger

- **S5 fixed:** every page chrome band is constructed at the page's real width, and controls moved out of `kit_preset_host` have fixed left/top follows before the first reshape. The preset combo can no longer inherit a 1 px band's follow-right delta.
- **S6 fixed:** `btn_cast_chip` is no longer in the transport collapse order. At folded-rail widths it remains the route to `kit_cast_popover`; `CAST_UNREACHABLE` fails unless either the in-rail roster or the chip is actually visible.
- **Late layout fixed:** mode changes immediately repack only visible chips, and the CAST header ends before the four tools begin.
- **Blank legacy detection fixed:** required page chrome must be visible as well as contained, and the first captured legacy-body PNG must have real luminance/colour variance (`LEGACY_BODY_BLANK_PNG`). This rejects the original blank Look > Image rendering even when its host rect and child tree look structurally valid.

- **B1 fixed:** `ALKitLazyPage` refuses construction until `LLFloaterDirector::postBuildV2` has installed the page-built binder, scroll-fit controller, and called `ALKitShell::setReady()`. The selected first page is then built through `ensureBuilt`, adopted, registered, emitted, bound, and host-fitted. Legacy roots take the same path and are created at the final host size.
- **B2 fixed:** page bar, chip band, who band, banner, and master box are born at their token heights rather than `1 px`; their first follower reshape no longer displaces children into the preceding band.
- **B3 fixed:** the generator names `kit_preset_host`; its combo and measured Save/Delete controls are reparented into the page bar.
- **B4 fixed:** lazy attachment checks run only for `ALKitLazyPage` audit targets, not group panels or ordinary nested tabs.
- **B5 fixed:** live audit adds `TAB_OVERFLOW`, `TAB_LABEL_CLIPPED`, geometric `LAZY_PAGE_NO_CARD`, registered-page attachment, `LEGACY_BODY_COLLAPSED`, `LEGACY_BODY_UNBOUND`, and `PAGE_CHROME`. The PowerShell live gate treats each as fatal.
- **Majors fixed:** full help appears at the 1160 px mockup width and folds below it, so the 1150–1159 band retains a 120 px search field and two 340 px cards; audit sizes cover 1149/1150/1158 and 979/980/988. Transport collapses UI/reset/delete/status before timecode and enforces a 120 px readable status width (otherwise the tooltip retains the full text).
- **Related review minors:** rail labels are left-aligned and the active dot is painted at the trailing edge; tab offset resets when overflow disappears and displayed pip labels are measured; hidden chips no longer reserve gaps and counts reflect visible cards; roster tools share the CAST header and the loose group combo is hidden.

## Rendering fixes

- Lazy page hosts now fill both the generated logic wrapper and its inner `ALKitPage` at build and reshape; `adoptPage` reinforces the same contract.
- Kit scroll documents use the vertical-scroll viewport width, preventing horizontal scrollbars and the stray full-width bar.
- Page selection seeds the help pane/strip from the page title and summary.
- The rail is 176/56 with left-aligned icons, reserved label space, active amber label/dot, and 980 px folding; help switches panel/strip at 1150 px.
- Capture remains the eighth approved group with its Time and Scene pages in the v2 data generator, generated rail/tree/index, generated page files, and the v2 C++ group table. Only the empty `capture_settings_tab` page is dropped, leaving 35 page contracts.
- The roster has fixed header/tools/group/hint regions, an elastic contained cast list, compact icon tools, and a live `CAST n` header.
- `ALKitBar` restores XUI declaration order and only collapses direct children. The transport now has one explicit REC item and compact ACTION/arm/play/stop/timecode/scene/Load/Save/.../marks/paths/help/status/Cast/UI/? slots.
- Page tabs use measured compact widths, remain one row, and expose arrows only for real overflow; Easy/Advanced remains reserved at the right.

## Live failure gates

`-DirectorV2` records fatal findings for the Round 1 conditions above plus floater/roster containment, rail collision, the exact eight groups, transport order/containment/overlap/text/flex, and kit-scroller containment/height/horizontal overflow. The driver fails on findings, missing PNGs, or missing per-page/per-size style probes.

Round 3 adds fatal `CAST_UNREACHABLE`, framebuffer-derived style/token, text-colour, combo-caret and selected-state checks. Probe JSON is emitted only after the RGB capture exists; the PowerShell gate parses every probe and fails any nonzero mismatch count.

## Verification handoff

- The Capture correction restores the 35-page / 1,839-row generator assertions and 31 serialized legacy-page parity fixtures.
- Regeneration and all gates are left to the orchestrator as requested.
- Not run by request: Python/generator, build, `/Zs`, viewer/live screenshot execution, commit.
