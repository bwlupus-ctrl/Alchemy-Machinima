# Wiring audit: Performance > Animate kit conversion

Static audit of the Animate page after it became a generated kit page (10/15, legacy conversion batch 1). No build, no commit, no viewer run: every row below is checked against the generated XML and the C++ source, not against a live session. Sources: `scripts/code_tools/director_kit/director_mockup_data.json` (page `animate_tab`, now 26 rows in 4 cards), `kit_pages.json` entry `performance_group/animate_tab`, generated `panel_dpage_performance_animate.xml`, hosted source nodes in `panel_director_animate_src.xml`, preview pane `panel_anim_preview_pane.xml`. Owners: `LLFloaterDirector::bindAnimatePage` (`llfloaterdirector.cpp:2436`) and `ALPanelAnimPreview` (`alpanelanimpreview.{h,cpp}`).

## Result

- The page is no longer legacy: classification `kit (P0)`, pin 13 -> 12, `performance_group/animate_tab` removed from `KNOWN_LEGACY_PAGES`.
- 26 generated rows = 24 mockup rows + 2 added (`animate_header`, `loco_text`, the v1 status text boxes the mockup lacked). 4 cards (Selected member, Play by UUID, Locomotion, Anim preview), 2 sections.
- Every widget name `bindAnimatePage` looks up resolves to a generated element of the same control class (table below). The v1 Director (`floater_director.xml`) and the Animation Explorer keep their own untouched XML.
- Two v1 names have no row and are in `parity_allow`: `preview_section_lbl` (static heading, now the card title) and `preview_hint` (static text inside the shared panel, now the `animation_preview` row help). `animate_scroll`, `animate_scroll_content` and `anim_preview_panel` are v1 containers; the preview panel row is now named `animation_preview`.
- C++ lookups changed: `llfloaterdirector.cpp:2455` (`"anim_preview_panel"` -> `"animation_preview"`), `:2484` new `setMaxTextLength(36)` and `:2489` `bindActionControls(root)`. `ALPanelAnimPreview::postBuild` no longer requires its three buttons (it uses `findChild`), accepts the kit pane (`animation_preview_frame`, panel is its own preview rect), and gained `bindActionControls()` and `previewRect()`.
- Behavior deltas worth an in-world check (no viewer run here): the preview pane is now a kit row (blit and mouse handling use `previewRect()`); the empty-state hint is an Advanced row (mockup `a: 1`) rather than an overlay on the list; `anim_list` has a dynamic-width UUID column instead of fixed 242 px.

## Controls

Defaults and ranges are the generated XML values. "Handler" is the function bound in `bindAnimatePage` unless stated. Read-back is `LLFloaterDirector::refreshAnimateTab` (`:6555`), which runs from `draw` only while `anim_list` is in the visible chain and writes only on change.

| # | widget | kit kind / class | setting or handler | type / range / combo data values | read-back | status |
|---|---|---|---|---|---|---|
| 1 | animate_header | readout `text` (LLTextBox), authored text "No cast member selected" | none; written by refresh (`setTextIfChanged`, `:6584`) | text | refresh writes the cast member name, "(away)" suffix | OK |
| 2 | anim_list | list `scroll_list` (LLScrollListCtrl), columns `anim_id` (dynamic), `prio` 30, `playing` 28 | none; rows built by refresh; right click -> `onAnimRightClick` (`:6276`) | column indexes read by name at bind (`getColumn("prio")`, `("playing")`); both columns present | `selectedAnimId()` (`:6259`) reads the selected row UUID | OK |
| 3 | btn_anim_copy | button | `onAnimCopyUUID` (`:6297`): clipboard copy of the selected row UUID | n/a | enabled by refresh when a row is selected | OK |
| 4 | btn_anim_setloco | button | `onAnimSetLoco` (`:6353`) -> `applySelectedLocomotion(selectedAnimId())` | n/a | enabled when a row and a member exist | OK |
| 5 | btn_anim_play | button | `onAnimPlayLocal(true)` (`:6379`) | n/a | enabled when a row and the avatar exist | OK |
| 6 | btn_anim_stop | button | `onAnimPlayLocal(false)` | n/a | same as play | OK |
| 7 | anim_hint | readout `text` (LLTextBox), basic (was Advanced; the v1 guidance is visible by default) | none; refresh sets text and visibility only when it changes | text | `:6684-6687` | OK (hidden by refresh when there is nothing to say; row label stays) |
| 8 | paste_lbl | readout `text`, authored static text | none; no runtime writer | text | n/a | OK |
| 9 | paste_uuid_editor | text `line_editor` (LLLineEditor) | read by `pasteAnimId()` (`:6265`), trimmed and validated as a UUID | free text; max length 36 set in bind (was a v1 XML attribute) | refresh caches the text and derives button enablement | OK |
| 10 | btn_paste_play | button | `onPastePlayLocal(true)` (`:6395`) | n/a | enabled when UUID valid and avatar exists | OK |
| 11 | btn_paste_stop | button | `onPastePlayLocal(false)` | n/a | same | OK |
| 12 | btn_paste_setloco | button | `onPasteSetLoco` (`:6410`) | n/a | enabled when UUID valid and member exists | OK |
| 13 | play_local_priority_combo | combo, setting `DirectorAnimPlayLocalPriority` | kit setting binding; consumer `apply_play_local_priority` (`:6362`) reads it with `LLCachedControl<S32>` | S32, default -1. Data values -1 Baked, 0 Low, 1 Medium, 2 High, 3 Higher, 4 Highest (row override `iv`; `inherit_selectable` keeps Baked selectable) | setting watch | OK |
| 14 | locomotion_role_combo | combo (LLComboBox), no setting | `onLocomotionRoleChanged` (`:6431`); `selectedLocomotionRole()` (`:6309`) reads `getValue().asInteger()` | data values 1 Idle, 2 Walk forward, 3 Run forward, 9 Fly, 10 Hover = `LOCO_IDLE`, `LOCO_WALK_FORWARD`, `LOCO_RUN_FORWARD`, `LOCO_FLY`, `LOCO_HOVER` (`lldirectorcast.h:203-212`); bind sets the initial value to `LOCO_WALK_FORWARD` | `refreshLocomotionRoleEditor` (`:6456`) | OK (the mockup items had indexes 0-4, fixed by `iv`) |
| 15 | locomotion_speed | number `spinner` (LLSpinCtrl) | `onLocomotionMetadataCommit` (`:6437`) clamps 0.05..20 and writes the role entry | 0.05..20, step 0.05, 2 dp; matches the C++ clamp | refresh sets it from the member's role entry | OK |
| 16 | locomotion_phase | number `spinner` | same handler, clamps -1..1 | -1..1, step 0.05, 2 dp; matches the clamp (-1 = automatic) | refresh | OK |
| 17 | locomotion_allow_ao | toggle `kit_switch` (LLCheckBoxCtrl subclass) | same handler, `get()` -> `mAllowAO`; refresh uses `set()` | Boolean | refresh | OK |
| 18 | loco_text | readout `text` (LLTextBox), authored "Select a cast member" | none; refresh writes the role status line (`:6549-6551`) | text | refresh | OK |
| 19 | btn_clear_loco | button | `onClickClearLoco` (`:6415`) | n/a | enabled when the role has an entry | OK (functional manifest SKIP: destructive) |
| 20 | btn_open_animexp | button | `LLFloaterReg::showInstance("animation_explorer")` (bind, `:2513`) | n/a | n/a | OK |
| 21 | locomotion_editor_label | readout `text`, basic (was Advanced), authored static text | none | text | n/a | OK |
| 22 | animation_preview | custom row hosting `panel` class `panel_anim_preview`, file `panel_anim_preview_pane.xml` (ALPanelAnimPreview) | looked up as `ALPanelAnimPreview` by name; `previewAnim(id, source)` called from refresh (`:6767`) | row height 186; the panel is its own preview rect (`animation_preview_frame` child) | the panel blits its dummy in `draw()` via `previewRect()`; mouse orbit/zoom gated to that rect | OK (no generic functional exercise: `NO_EXERCISE`, same class as other custom hosts) |
| 23 | capture_all_check | toggle `kit_switch`, setting `AnimExplorerCaptureAll` | kit setting binding; consumer `llviewermessage.cpp:4351` (`LLCachedControl<bool>`) | Boolean, default 0 | setting watch | OK |
| 24 | btn_anim_ex_stop | button | `ALPanelAnimPreview::onStop`, attached by `bindActionControls(root)` | n/a | enabled by `refreshActionButtons()` in the panel's `draw` | OK |
| 25 | btn_anim_ex_revoke | button | `onStopAndRevoke`, same attach | n/a | enabled when the source object is known | OK |
| 26 | btn_anim_ex_blacklist | button | `onBlacklist`, same attach | n/a | enabled when the source object is known | OK |

## Checks run

- `director_kit_gen.py --check` and `--selftest`, `director_page_sources.py --check`, `kit_lint.py --all`, `xui_layout_check.py --selftest` and `--changed HEAD`, `director_functional_manifest.py` plus its self-tests, `director_parity.py --selftest`, `ui_draw_lint.py --changed HEAD` (0 new), `zs_check.ps1` for `alpanelanimpreview.cpp` and `llfloaterdirector.cpp`, `prebuild_guard.ps1 -ChecksOnly`.
- Not run (rules): build, viewer, in-world self-test. The preview pane, the Explorer-floater panel and the v1 Director tab still need an in-world look.
