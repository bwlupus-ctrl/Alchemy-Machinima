# Director Kit: Phase 0 build spec

Branch `feature/director-kit-p0`. Written 2026-10-09 (Opus) for Codex, which implements. Opus then reviews and Claude does one fresh build.

**Status:** build spec. Every name, tag, parameter and file below is binding unless a spike (section 8) says otherwise. When this spec and the code disagree, stop and ask; do not guess.

**Sources:**
- the plan: `C:\Users\xianw\cxs\director_aaa_plan.md` (Option C+);
- the gap audit: `C:\Users\xianw\cxs\mockup_gap_audit.md` (Top 10);
- the approved mockup data: `director_mockup_data.json`, snapshotted into the repo by WP0 at `scripts/code_tools/director_kit/director_mockup_data.json`;
- the approved mockup: `Director.dc.html` and `Kit.dc.html`. The metrics are copied into section 3.1, so Codex never needs the HTML.

**Rollout:** **ALL AT ONCE.** P0 is a non-shippable foundation. After WP5, the mandatory WP6-WP12 work in `DIRECTOR_KIT_ROLLOUT_SPEC.md` converts all 36 pages back-to-back. `DirectorUIV2` remains default-off and debug/self-test-only until that document's final ship gate passes. The normal user never sees a half-converted Director.
- v1 (`floater_director.xml` and everything it embeds) stays byte-identical, and its behaviour is unchanged.
- v2 becomes user-visible only when all 36 pages are kit pages and the absolute-zero four-size audit passes every default and non-default state.

---

## 0. Scope

**In P0:**
1. The kit:
   - widgets: ALKitStyle, ALKitSwitch, ALKitRow, ALKitCard, ALKitOption, ALKitPage, ALKitLazyPage, ALKitShell, ALKitBar, ALKitPopover;
   - services: ALKitSearch, ALKitHelp, ALKitPresetProvider (plus SnapshotStore and the `rig_rim` adapter), ALKitBinding with ALKitSettingWatch (amber), and ALKitHistory (undo).
2. The ALCompassDial arc mode and the new ALLightRigMap widget.
3. The generator `director_kit_gen.py`, which writes `floater_director_v2.xml`, 36 page files and the search index.
4. Two pages fully converted: **Light › Rig** and **Light › Rim**. ALRigRimController is extracted, and ALPanelRigRim is new.
5. Every other page in the non-shippable P0 test artifact:
   - a v1 page becomes a **legacy embed**;
   - a new page becomes **pure kit** if every row is pure, otherwise a **launcher page** (section 4.3).
6. LLFloaterDirector becomes shell-agnostic: per-page binders, lazy late binding, cast-selection signal, keyboard.
7. Tooling and gates:
   - `kit_lint.py`, `director_parity.py`, and the XUI checker learning the kit tags;
   - the self-test `-DirectorV2` mode: 4 audit sizes, lazy rescan and budget evaluation;
   - TUT tests for the pure maths.

**Deferred only to the mandatory all-at-once continuation (WP6-WP12), not omitted from the product:**
- converting every other page according to `DIRECTOR_KIT_ROLLOUT_SPEC.md`;
- VCam monitor tiles, the curve editor, the slot grid and the animation picker;
- the Quick panel;
- new rail PNG icons (existing `Command_*` icons are used);
- changing any render code.

**Global off-limits (every WP):**
- `floater_director.xml`, `panel_cine_light_rig.xml`, `floater_cine_light_rig.xml`;
- every v1 panel XML that a legacy page embeds;
- `alcinelightrigmodel.*`;
- anything under `indra/llrender` or the shaders;
- the pipeline;
- `settings.xml` entries other than the new ones in section 3.16.

Codex never builds. It only edits files inside `I:\alchemy-machinima`.

P0 cannot satisfy the release definition by itself. Its intermediate legacy/launcher pages exist solely for automated foundation testing and are forbidden in a shipped v2.

---

## 1. CLAUDE.md rules this code must satisfy (the reviewer checks each one)

1. **No work in `draw()`.** `draw()` may only:
   - compare revisions, timers or bools;
   - call `setTextIfChanged`, `setValueIfChanged`, `setEnabledIfChanged`, `setVisibleIfChanged` or `setToolTipIfChanged`;
   - render (`gl_*`, font render).

   It never does a `getChild`/`findChild`, a `gSavedSettings.getXxx("…")`, `setText`, list rebuilds or string building from UUIDs. Settings are read with `LLCachedControl` statics. Every kit `draw()` opens with `LL_PROFILE_ZONE_SCOPED_CATEGORY_UI`.
2. **Layout.**
   - Kit tags carry no hand coordinates (section 6.1 lint).
   - Kit page documents size themselves; ALScrollFit skips them (section 3.15).
   - Legacy pages are adopted by ALScrollFit (wrap and fit).
3. **Ownership.** Shared panels claim on reveal, focus or user action, never in `draw()`. ALRigRimController and ALPanelRigRim follow the existing patterns.
4. **Hidden pages.** Every page refresher returns early while its page is not in the visible chain.
5. **Wheel.**
   - Kit sliders keep the global wheel rule.
   - ALCompassDial and ALLightRigMap consult `LLSliderWheel::guarded(this)`. When guarded they call `LLSliderWheel::keepAlive()` and return false.
   - The kit scroller never moves content outside `LLScrollbar::scrollByWheel`.
6. **Gates.**
   - `xui_layout_check.py` and `ui_draw_lint.py` report 0 new findings.
   - The runtime audit reports 0 findings (section 6.4).
7. **Process.** Codex implements. Opus reviews adversarially, looping until there are 0 must-fix findings. Then there is ONE fresh build. Generated files are committed alongside their generator.

---

## 2. Architecture

```
LLFloaterDirector (class unchanged; registered once as "director")
 └ floater_director_v2.xml (picked when DirectorUIV2 is on, at registration)
   └ kit_shell "director_shell"                              ALKitShell
      ├ [code] kit_rail: group buttons + "kit_cast_roster" panel (v1 cast widgets)
      ├ [code] kit_tabstrip: page tab buttons + mode proxy segments
      ├ tab_container "director_tabs" hide_tabs=true (one panel per group)
      │   └ panel "<group>" → tab_container "<g>_tabs" hide_tabs=true
      │        └ kit_lazy_page "<page>" (empty until first reveal)
      │             └ built from panel_dpage_<g>_<slug>.xml:
      │                kit page:   [panel class=<logic>] → kit_page → bar, chips, who, banner, scroller → doc → grid → kit_card…
      │                legacy page: kit_page legacy=true → bar + "<page>_legacy_host" → verbatim v1 leaf children
      ├ [code] kit_help (224 px panel) / kit_help_strip (30 px)
      ├ kit_bar "kit_transport" (v1 transport widget names)
      └ kit_popover × n (arm, legend_panel, hotkeys, cast, search results)
```

The key decisions:

**D1. Every page is lazy.**
- `kit_lazy_page` builds its file on first reveal, from any of: `ALKitShell::selectPage`, `ensurePageBuilt`, or as a safety net in `onVisibilityChange(true)`. The safety net covers the self-test's direct `selectTab`.
- After the build it emits `pageBuiltSignal(page_name, content)`.
- Code must use `findChild` before that signal fires, never `getChild`.

**D2. C++ contracts are kept by name and class.** Every v1 widget that C++ touches keeps its exact `name` and its exact class in v2.
- A kit row hosts the v1 XML node as its **control child**, copied verbatim minus geometry (section 4.5).
- `check_box` is the only tag that gets rewritten, to `kit_switch`. ALKitSwitch is a subclass of LLCheckBoxCtrl, so `getChild<LLCheckBoxCtrl>` keeps working.

**D3. Logic panels own kit pages from the outside.**
- The Rig page file root is `<panel class="panel_cine_light_rig">` with the `kit_page` inside it. The kit_page and all rows are fully built before `ALPanelCineLightRig::postBuild` runs, so its `getChild` calls succeed.
- The Rim page root is `<panel class="panel_rig_rim">` (new ALPanelRigRim).

**D4. The page bar is page-owned.**
- Presets, page reset, Undo, Redo, master switch and a search field live inside each page's subtree.
- The shell owns one search model; each page's search field is a proxy of it.
- The mode segments in the tab strip are shell-owned proxies of the page's hidden mode switch.

**D5. v1 is untouched.**
- `LLFloaterDirector::postBuild` is refactored into binders (section 3.17). v1 runs all of them in the original order, so v1 behaviour is unchanged.
- `mIsV2 = findChild<ALKitShell>("director_shell") != nullptr`. Every v2-only path is gated on it.

---

## 3. Components

All new files go in `indra/newview/` unless noted, and are added to `indra/newview/CMakeLists.txt`. Every registered tag uses `static LLDefaultChildRegistry::Register<T> r("<tag>");`.

### 3.1 ALKitStyle: `alkitstyle.h/.cpp`

A namespace of `constexpr` metrics plus colour accessors. Colours are defined **once** in `skins/default/colors.xml` as `Kit*` names, and C++ reads them through `LLUIColorTable` via `ALKitStyle::color(EColor)`, cached as `LLUIColor`.

**Metrics** (pixels at UI scale 1; all from Kit.dc.html and Director.dc.html):

| Name | Value | Name | Value |
|---|---|---|---|
| ROW_H | 28 | LABEL_W | 100 |
| VALUE_W | 52 | RESET_W | 20 |
| COL_GAP | 6 | ROW_INSET | 4 (hover band extends 4 px each side) |
| DIAL_SIZE | 48 | DIAL_ROW_H | 56 |
| DIAL_CELL_H | 80 (2-up cell: 4 + label 20 + 2 + 48 + 6) | NUMBER_FIELD_W | 96 (80 field + 16 arrows) |
| SWITCH_W×H | 34×18, knob 14, inset 2 | RESET_GLYPH | 11; card reset button 24 (glyph 13) |
| CARD_HDR_H | 36 | CARD_PAD_L/R/T/B | 12 / 8 / 6 / 8 |
| CARD_RADIUS | 6 | CARD_MIN_W | 340 |
| GRID_GAP | 12 (columns and rows) | PAGE_PAD_T/LR/B | 14 / 16 / 16 |
| SUB_FIRST_H | 26 | SUB_H | 34 (6 margin + 1 divider + 8 + text + 3) |
| MORE_H | 26 | SELECTOR_BAND_H | 34 |
| TRACK_MIN | 120 (slider track) | |
| RAIL_W / RAIL_ICON_W | 176 / 56 | RAIL_ITEM_H | 40 (padding 8, label 13 px) |
| ROSTER_HDR_H | 24 | ROSTER_TOOL | 22×22 |
| HELP_W | 224 (padding 16) | HELP_STRIP_H | 30 |
| TABSTRIP_H | 40 (tab 32 high, 12 px side padding, 2 px underline) | CHIPS_H | 38 (chip height 24, radius 12) |
| WHO_H | 34 | BAR_H | 44 |
| BANNER_H | 32 | TRANSPORT_H | 40 |
| SEARCH_W | 170 (field 24 high) | PRESET_COMBO_W | 160 |
| CARD_PRESET_W | 120 (22 high) | MAP_CANVAS_H | clamp(round(0.62 × body_w), 180, 240) |
| MAP_LEGEND_H | 40 | FLOATER_DEFAULT | 1160×720 |
| FLOATER_MIN | 850×560 | BP_FULL | 1142 |
| BP_RAIL | 916 | BP_HYST | 8 (applies only when widening) |
| FLASH_SEC | 1.5 | DIM_ALPHA | 0.45 |

**Colours** (`colors.xml`, hex sRGB, alpha 1 unless shown):

| Name | Hex | Name | Hex |
|---|---|---|---|
| KitWindowBg | #111316 | KitRailBg | #15171B |
| KitSubBand | #181B1F | KitCardBg | #1B1E23 |
| KitCardBorder | #2A2F36 | KitDivider | #262A31 |
| KitDivider2 | #22262C | KitRowHover | #22262D |
| KitRailHover | #1E2228 | KitControlBg | #262A31 |
| KitInputBg | #121418 | KitInputBorder | #343A42 |
| KitTrack | #363C44 | KitSwitchOff | #3A4049 |
| KitKnobOff | #C9CED6 | KitKnobOn | #141413 |
| KitAccent | #E8A33D | KitAccentHi | #F3BE6A |
| KitRecord | #E5484D | KitRecordText | #F2A7A9 |
| KitText | #E6E8EB | KitTextStrong | #F1F2F4 |
| KitLabel | #C9CED6 | KitDim | #9BA3AE |
| KitMuted | #8A929D | KitFaint | #5C646F |
| KitDialRing | #4A515B | KitDialFace | #15171B |
| KitArcTrack | #2F343B | KitFlash | #E8A33D @ 0.28 |
| KitBannerBg | #E8A33D @ 0.08 | KitMapKey | #E8A33D |
| KitMapFill | #7FA8E0 | KitMapBack | #F1F2F4 |
| KitMapBg | #A98BF5 | KitOffGrey | #8A929D |

**Fonts:**
- Labels, titles and help use the user's UI font: `getFontSansSerifSmall()` for labels, and `SansSerif` bold for card titles.
- Numbers use `LLFontGL::getFont(LLFontDescriptor("IBMPlexMono", "Small", 0))`, falling back to `getFontMonospace()`. Spike S6 confirms the size names.

**Textures:** `scripts/code_tools/director_kit/kit_png_gen.py` writes the white alpha masks below to `skins/default/textures/kit/`, using a self-contained zlib/struct PNG writer with no PIL. They are registered in `textures.xml` as 9-slices and tinted at draw time:
- `Kit_RoundRect4.png` and `Kit_RoundRect6.png` (16×16, scale 6/6/6/6);
- `Kit_Pill9.png` (18×18);
- `Kit_Circle.png` (16×16).

The output is deterministic, and the script is idempotent.

### 3.2 ALKitSwitch: `alkitswitch.h/.cpp`, tag `kit_switch`

- `class ALKitSwitch : public LLCheckBoxCtrl`. Its Params block is `Block<Params, LLCheckBoxCtrl::Params>`, so every v1 `check_box` attribute parses.
- Add `skins/default/xui/en/widgets/kit_switch.xml` as a copy of the structural defaults in `widgets/check_box.xml`, including the 13x13 `check_button` rect and `is_toggle="true"`. A new tag does not inherit that file automatically; the required defaults are visible at `widgets/check_box.xml:11-24`.
- In `reshape`, expand the inner button's hit rect to the full 34x18 pill and suppress only its stock image drawing; keep `is_toggle` and its click callback. The entire visible switch, not merely the original 13x13 square, is clickable.
- Draw: an SWITCH_W×H pill.
  - **On:** KitAccent track, KitKnobOn knob at x=18.
  - **Off:** KitSwitchOff track, KitKnobOff knob at x=2.
  - **Disabled:** 0.4 alpha.
  - **Focused:** a 1 px KitAccent focus ring.
  - The LLCheckBoxCtrl label is not drawn.
- `bool isOn() const` reads `mButton->getToggleState()` directly. `mButton` is the checkbox's actual value store (`llcheckboxctrl.h:144-147`), so the read is allocation-free and cannot go stale.
- Override `setControlName`. Disconnect and replace the previous scoped connection first (S9 may rebind a generated control), call the base implementation, then connect to `mButton->getControlVariable()->getSignal()`. The callback consumes the signal's `newvalue` and calls `noteState(newvalue.asBoolean())`; it does not assume callback ordering by re-reading the button. `LLCheckBoxCtrl::setControlName` binds the inner button, not the wrapper (`llcheckboxctrl.cpp:280-283`), and `LLUICtrl::controlListener` sends external setting changes to the bound control's `setValue` (`lluictrl.cpp:639-647`). A rebind leaves exactly one live connection and the old control can no longer flip this switch.
- Override `onCommit()`. First call `noteState(mButton->getToggleState())`, then `LLCheckBoxCtrl::onCommit()`. A user click toggles the inner `LLButton` before its click callback (`llbutton.cpp:311-337`, `1050-1055`), so this observes the post-click value. `noteState` stores only the last notified bool and fires `mFlipSignal` only on a real edge; the setting signal and commit path may both arrive without double-firing.
- Override `setValue(const LLSD&)`: call `LLCheckBoxCtrl::setValue(value)`, then call `noteState(value.asBoolean())`. This is mandatory because existing C++ writes kit-bound controls without committing: group switches at `alpanelcinelightrig.cpp:1630-1634`, shaft/hero switches at `:2583-2584`, and the per-instance Easy presentation at `:2390`; `setValueIfChanged` itself calls `setValue` and never commits (`aluirefresh.h:291-302`). `noteState` is edge-only, so a control-variable signal, this override, and `onCommit()` may all observe one change without double-firing. Space and Enter use the same commit path as a click.
- `writeBoundValue(bool v)` is the passive write path: obtain the inner button's `LLControlVariable` and call `control->set(LLSD(v))`; if there is no bound control, set the inner toggle and call `noteState`. Never call `LLCheckBoxCtrl::setControlValue`, because the wrapper has no control variable and that method returns false (`lluictrl.cpp:510-517`; `llcheckboxctrl.cpp:159-166`).

### 3.3 ALKitRow: `alkitrow.h/.cpp`, tag `kit_row`

`class ALKitRow : public LLPanel`.

**Params** (all Optional unless marked Mandatory):

| Param | Type | Meaning |
|---|---|---|
| `kind` (Mandatory) | string | one of the kinds below |
| `widget` | string | name of the control child (or of the kit-created control for `link`/`vec2`/`vec3`) |
| `label` | string | row label (also used for search and help) |
| `label_name` | string | name of the created label textbox; default `<widget>_label` |
| `help`, `keywords`, `unit`, `sub` | string | metadata. `sub` = sub-heading key (combined cards) |
| `setting` | string | control variable this row represents (metadata, plus amber, reset and undo when `bind="setting"`) |
| `bind` | `setting` \| `code` | `setting` = the control child carries `control_name`; `code` = C++ drives the value |
| `reset_settings` | string | comma list of settings restored by the row reset (from the v1 `<w>_reset` button parameter) |
| `default_value` | string | LLSD notation default for `bind="code"` rows with a data `d` (enables amber for an ALKitWidgetBinding) |
| `advanced` | bool | hidden until the card's "More options" opens |
| `card_preset` | bool | the control is shown in the card header (first sub) or on its sub-heading |
| `who_picker` | bool | the row is never shown; the page "who" chip proxies its combo |
| `hidden` | bool | always hidden (parity only) |
| `added` | bool | draw a 5 px accent dot before the label ("new in this layout") |
| `link_to` | string | `group/page` for `link` rows |
| `action` | string | kit-native button action: `floater:<name>[:<key>]` or `lightbox:<tab>` |
| `row_height` | S32 | explicit height for list, viewport and custom rows (and word-wrapped readouts) |
| `min_val`, `max_val`, `decimals` | F32, F32, S32 | metadata for the help "Range" line and amber precision |
| `mirror_setting` | string | combo rows only: keep the control's index equal to this S32 setting (section 5.1 fixture link) |

**Children.** Exactly one control child, except for `link`, `vec2` and `vec3`, where the kit creates it. The control child is any registered widget, declared in XML with **no** `left/top/width/height/bottom/follows/layout/label_width/text_width` (kit_lint enforces this).

**Created in `postBuild()`** (`postBuild` runs after the XML children exist):
- `<label_name>`: an LLTextBox, `use_ellipses=false`, KitLabel colour, label font.
- `<widget>_reset`: an LLButton, image overlay reset glyph, only when a reset path exists (see "Reset path" below).
- the value-column view (if any): `<widget>_unit` (number), `<widget>_state` (toggle), or `<widget>_value` (dial and arc), each an LLTextBox in the mono font.

**Kinds.** The kind decides the layout. Column order: label | control | value | reset.

| kind | control child tag (P0) | control column | value column | height |
|---|---|---|---|---|
| slider | `slider` (LLSliderCtrl; generator forces `show_text="true" text_width="52"`, no label) | spans control+value (the ctrl's text sits in the 52 px value slot) | (inside the ctrl) | 28 |
| number | `spinner` (LLSpinCtrl, `label_width="0"`) | NUMBER_FIELD_W, left-aligned | `<w>_unit` (unit, KitMuted) | 28 |
| toggle | `kit_switch` | switch at the left of the control column | `<w>_state` "On"/"Off" (updated on flip) | 28 |
| combo | `combo_box` | spans control+value, height 24 | — | 28 |
| color | `color_swatch` (no label) | swatch 40×18 | — | 28 |
| readout | `text` (mono) | spans control+value | — | 28, or `row_height` when `word_wrap` |
| button | `button` | left-aligned, width = min(text + 20, span) | — | 28 |
| text | `line_editor` | spans control+value, height 22 | — | 28 |
| list | `scroll_list` | spans control+value | — | `row_height` (default 120) |
| dial / arc | `compass_dial` (`mode="compass"` or `"arc"`, `show_readout="false"`) | 48×48 at the control column left | `<w>_value` e.g. "45°" | 56 (a 2-up group cell is 80, see 3.4) |
| vec2 / vec3 | none: the kit creates `spinner`s `<w>_x/_y[/_z]` bound by ALKitVecBinding | spans control+value, equal split with axis letters | — | 28 |
| link | none: the kit creates `button` `<w>` (borderless, KitAccent text "Label ›") | spans | — | 28 |
| viewport | `view_border` or `panel` (hosted) | spans | — | `row_height` (default 180) + 6 |
| gobo | `icon` (64×64) | at the control column left | — | 72 |
| custom | any single child | spans control+value | — | max(28, `row_height`) |

**Layout.** `S32 heightForWidth(S32 w) const` and `void layoutRow(S32 w)`:
- label at x = ROW_INSET, width LABEL_W, vertically centred;
- control at x = ROW_INSET + LABEL_W + COL_GAP;
- control width = w − 2·ROW_INSET − LABEL_W − VALUE_W − RESET_W − 3·COL_GAP (this "track" is ≥ TRACK_MIN at CARD_MIN_W, see section 3.7);
- value at the next column;
- reset at the right.

Kinds that "span" take control + COL_GAP + value. Nothing is read from XML coordinates.

**Visibility.**
- `isEffectivelyVisible()` = `!hidden && !who_picker && filterVisible && (!advanced || card->moreOpen()) && control->getVisible()` (`link`/`vec` rows ignore the last term).
- C++ may hide a control (`setVisibleIfChanged`). The card notices through its visibility probe (section 3.4) and the row collapses.
- `static bool setControlVisible(LLView* control, bool visible)`:
  - if `control` is inside an ALKitRow and is that row's control, it sets the row's `mCodeHidden` and asks for a relayout;
  - otherwise it calls `setVisibleIfChanged(control, visible)`.

  It returns true if anything changed. New or extracted code (ALRigRimController) uses this; unchanged v1 code relies on the probe.

**Reset path**, in this order:
1. `bind="setting"`: an ALKitSettingBinding on `setting`.
2. `reset_settings` is non-empty: an ALKitSettingsListBinding. Amber = any non-default; reset = `resetToDefault(true)` on each.
3. A binding registered for `widget` (section 3.10).
4. `bind="code"` with `default_value`: an ALKitWidgetBinding.

If none applies, there is no reset button (the column stays empty).

**Amber.** The reset glyph is KitAccent when the binding is non-default, otherwise KitFaint. It is updated **only on a flip**, driven by ALKitSettingWatch or binding signals. Never per frame.

**Hover and focus.**
- `onMouseEnter` sets `mHovered` and calls `ALKitShell::help().showRow(this)`. `onMouseLeave` clears it.
- Focus entering the control (`onFocusReceived` via the control's focus-received signal) does the same.
- The hover band (KitRowHover, radius 4) is drawn only while `mHovered`.

**Flash.** `flash()` starts an LLFrameTimer. `draw()` paints KitFlash with alpha fading to 0 over FLASH_SEC, only while the timer runs.

**History hook.** For `bind="code"` value rows (slider, number, toggle, combo, text), the row connects to its control's commit signal **in its own postBuild**, so it runs before the owner panel's slot. On commit it calls `history.recordWidget(control, mLastValue, control->getValue(), label)`, then updates `mLastValue`. `mLastValue` is seeded at bind time and refreshed on reveal.

**Metadata accessor.** `const ALKitRowMeta& meta() const` returns kind, widget, label, help, keywords, setting, unit, min, max, decimals, advanced, card id, option index and page key. Help and search read it.

### 3.4 ALKitCard: `alkitcard.h/.cpp`, tag `kit_card`; ALKitOption: tag `kit_option`

`class ALKitCard : public LLPanel, public ALFlowGridItem`.

**Params:**
- `id` (Mandatory), `title`, `kind` = `rows|combined|selector|link|launcher|rigmap`, `advanced` (bool), `badge` (""|"NEW"), `help`;
- `enable_widget`, `enable_setting`, `enable_tooltip`;
- `sel_label`, `sel_setting`;
- `link_to`, `link_label`;
- `target`, `target_tab`, `button_label`;
- `Multiple<Summary> summary` (`<kit_card.summary setting label kind decimals unit/>`).

The panel name is `kc_<id>` (set by the generator).

**Header** (CARD_HDR_H; a bottom divider when the body is shown). From left to right:
1. title (13 px bold, ellipsized custom draw);
2. badge ("NEW" in accent, or "Advanced", "Link" or "Window" in grey);
3. "Show/Hide" toggle button (`kc_<id>_open`, only when `advanced`);
4. card presets (controls of `card_preset` rows that belong to the first sub, reparented into the header, CARD_PRESET_W each, tooltip "Card preset · <label>");
5. the enable switch;
6. card reset (`kc_<id>_reset`).

The enable switch is ALKitSwitch `enable_widget` (with `control_name=enable_setting` when given). For selector cards it is the **current option's** switch.

**Body** (padding as in section 3.1):
- **rows / combined:** rows in XML order. A sub-heading row is inserted whenever `sub` changes (combined cards). `card_preset` rows whose `sub` is not the first sub are placed right-aligned on their sub-heading.
- **Grouping:** consecutive dial/arc rows form one "aim group" of 2-up cells (each cell: label + reset on the top line, dial + value below). A single dial stays a normal 56 px row.
- **More options:** if the visible advanced count n > 0, a "More options (n)" / "Fewer options" toggle (`kc_<id>_more`) sits at the bottom. For a selector it is tracked per option.
- **selector:** under the header, a SELECTOR_BAND_H band (KitSubBand) holds `sel_label` (KitMuted 11 px) plus segment buttons `kc_<id>_seg_<i>` (≤ 6 options) or a combo (> 6). Then the active ALKitOption's rows. The active index comes from `sel_setting` (S32) when given, otherwise from page state.
- **link:** for each summary, a label and the current value (formatted from the setting, updated on its signal). Then "Lives on <Group › Page>", then a button `kc_<id>_go` "Open ›" that calls `shell->navigateTo(link_to)`.
- **launcher:** help text plus a button `kc_<id>_open` (label `button_label`). Its action is `target == "lightbox"` → `ALFloaterLightBox::showTab(target_tab)`, otherwise `LLFloaterReg::showInstance(target)`.
- **rigmap:** the single `light_rig_map` child (section 3.14). Its height is `map->heightForWidth(body_w)`. The view segments (Top / Side / 2.5D, `kc_<id>_view_<i>`) are in the header.

**Enable and dimming.** When the switch is off, the body (not the header) is drawn inside `LLViewDrawContext(DIM_ALPHA)`. It stays editable. `draw()` reads `mEnable->isOn()`.

**Visibility probe** (an explicit CLAUDE.md rule 1 exemption; the reviewer must confirm it):
- `draw()` (only when `isWidgetOnScreen(this)`) compares a pre-sized allocation-free vector of visibility bits for **all** rows; there is no 64-row cap.
- If any bit differs, it stores the bits and calls `requestLayout()`. That queues `doOnIdleOneTime` once per page and frame through a page `LLHandle`; a destroyed floater makes the callback a no-op.
- A row that is not effectively visible is also `setVisible(false)` during layout, and is restored when it becomes effective. Omitting it only from the height sum is insufficient because `childOverflow` skips only invisible children (`indra/llui/lluifitdiag.cpp:45-61`).

It reads only member bools (O(rows)), with no lookups, strings or allocations. It is how v1 C++ that toggles widget visibility (clip labels, shadow hint, gobo preview) collapses rows.

**Interface.**
- `S32 heightForWidth(S32 w) const override` sums the header, band and visible rows (and groups), plus padding.
- `bool spansAllColumns() const override` returns false, except for section headings in anchor mode (section 3.5).

**Reset.**
- The card reset applies, as **one undo step**, `reset()` to every resettable visible-or-advanced row of the card. For a selector this means **the active option only**.
- It **excludes** card and option enable switches, `who_picker` rows and `card_preset` rows.
- Right-click on the card reset of a selector card offers "Reset all <sel_label>s".

**Amber.** The card reset glyph is accent if any row in scope is amber. A per-card counter is kept, incremented and decremented on row flips.

**ALKitOption** (LLPanel):
- Params: `label`, `enable_widget`, `enable_setting`.
- It holds rows. Its enable switch is created as a child of the card header (`card->hostOptionSwitch(this, sw)`). All options' switches exist, so C++ `findChild` works, but only the active one is visible.

### 3.5 ALKitPage: `alkitpage.h/.cpp`, tag `kit_page`

`class ALKitPage : public LLPanel`.

**Params:**
- `page` (Mandatory, "group/page" key) and `summary`;
- `legacy` (bool);
- `master_widget`, `master_setting`, `master_label`, `master_tooltip`;
- `modes` ("A|B"), `mode_setting`, `mode_widget`, `mode_on` (the option name meaning "switch on"), `mode_tooltip`, `mode_adapter`;
- `who_label`, `who_picker`, `who_adapter`;
- `preset_mode` (`none|widgets|provider`), `preset_provider`;
- `undo_context`;
- `watch_settings` (comma list; legacy pages);
- `chip_mode` (`auto|filter|anchor`, default auto);
- `Multiple<Section> section` (`<kit_page.section id label cards="a,b"/>`);
- `Multiple<Mode> mode` (`<kit_page.mode name cards="a,b"/>`).

The kit page knows its own logic class only through its parent: a logic panel is its parent, or there is none.

**Structure, created in `postBuild()`** (top to bottom; all names prefixed `<P>_` where P = the page name without "_tab"):
1. `<P>_chips` (CHIPS_H): only if sections > 1. Pill buttons `<P>_chip_<i>` show label + count (mono 11) and a hit dot during search.
2. `<P>_who` (WHO_H): only if `who_picker`. Caption (uppercase 10.5) + chip button `<P>_who_chip` "<name> ▾" + hint text. A popover lists "↺ Follow rail (Name)", then the picker's items.
3. `<P>_bar` (BAR_H), left to right:
   - `preset_mode=widgets`: the `<kit_preset_host>` child's three widgets (combo PRESET_COMBO_W, save, delete);
   - `preset_mode=provider`: an ALKitPresetStrip (`<P>_preset`, `<P>_preset_save` "Save as…", `<P>_preset_delete`);
   - `preset_mode=none`: nothing;
   - then: page reset `<P>_reset_page` (opens the confirm popover), `<P>_undo`, `<P>_redo`;
   - then: the master box `<P>_master_box` (label + ALKitSwitch `master_widget` bound to `master_setting` + state text), only if a master exists;
   - then a spacer, then `<P>_search` (SEARCH_W line editor, placeholder "Find a control…");
   - **legacy pages:** the summary text (ellipsized custom draw) replaces the preset area.
4. `<P>_banner` (BANNER_H, KitBannerBg): shown while the master is off. It reads "<master_label> is off. Controls stay editable." with a `<P>_master_on` "Turn on" button.
5. **kit pages:** `<P>_scroll` (ALKitScroller, a subclass of LLScrollContainer) → `<P>_doc` (ALKitDoc: LLPanel + ALScrollFit::SelfSizedDocument) → `<P>_grid` (ALFlowGrid `pack="rows"`, `max_columns=2`, `card_width=340`, gap 12, `pad=0`). The doc pads PAGE_PAD. **The grid must never be named `cine_light_rig_flow_grid`.**
6. **legacy pages:** `<P>_legacy_host` (LLPanel, follows all) receives every XML child of the kit_page, with rects unchanged. The kit_page's own XML `width`/`height` equal the v1 leaf size, so `topleft` coordinates resolve as in v1. The host is then reshaped to the content area and follows-flags propagate.

**Reparenting** happens in `postBuild()`, after every child has run its own postBuild:
- `kit_card` children go into `<P>_grid`;
- the `kit_preset_host` widgets go into the bar;
- legacy children go into the host.

The page dims `<P>_doc` with DIM_ALPHA while the master is off.

**Modes.**
- A mode switch exists when `modes` is set: ALKitSwitch `mode_widget` with `control_name=mode_setting`, **invisible**, a child of the bar.
- The visible control is the shell's mode proxy (section 3.7). The page subscribes to the switch's flip signal.
- Current mode = `mode_on` if `isOn()`, otherwise the other option. Cards not listed in the current mode's `cards` are hidden. A card listed in no mode is shown in all modes.
- `setModeIndex(i, bool explicit_user)`:
  - resolve the registered `ALKitModeAdapter` and call its single `apply(v, origin)` function under `ALKitHistory::ScopedSuspend` for passive/search/restore origins;
  - the adapter writes the inner switch's bound `LLControlVariable` through `ALKitSwitch::writeBoundValue`, updates the owner model, and synchronizes enabled/visible controls before returning;
  - explicit user clicks use the same adapter with `USER` origin and remain undoable. Search reveal may switch modes, but does not create history and does not later switch the user back.

For Rig, refactor `ALPanelCineLightRig::onEasyModeCommit()` into `applyEasyMode(bool desired, EModeOrigin origin)`. The existing callback only reads the toggle and delegates. Setting-signal changes (preset, scene, undo, slot restore), the shell proxy, search reveal, and initial synchronization all delegate to this same function. It is solely responsible for the setting, `mEasyModeSlot`, `mEasyModeActive`, `normalizeSelectedForEasy`, and `syncEasyControls`. This is required because Easy commits return early when `mEasyModeActive` is false (`alpanelcinelightrig.cpp:2217-2243`). The current per-frame `syncEasyModeForSelected()` at `alpanelcinelightrig.cpp:2853` becomes a revision/signal fallback, not a second state transition implementation.

The adapter is page-instance scoped: `ALKitPage` registers it with the page's own `LLHandle<ALPanelCineLightRig>` logic root. It never searches `LIVE_PANELS` (both standalone and Director instances register there at `alpanelcinelightrig.cpp:823-827`). `applyEasyMode` runs under the existing `mSyncingEasyControls` re-entrancy guard and returns/publishes the **effective** `mEasyModeActive`, which can differ from `desired` when `normalizeSelectedForEasy` refuses (`alpanelcinelightrig.cpp:2230-2235`). The page uses that published effective value for its Easy/Advanced card set, master banner and dimming. A switch flip caused by `setValue`, a bound-control signal, click, search reveal, or undo schedules one edge-only reconciliation from the adapter's effective value; it never blindly treats the requested setting as the page mode. The standalone panel observes the shared setting signal and updates its own instance independently, under the same guard, and must not write the setting back from passive synchronization.

**Chips.**
- `chip_mode=auto` resolves to `filter` when the count of cards visible in the current mode is ≥ 6, otherwise `anchor`.
- **filter:** only the selected section's cards are shown.
- **anchor:** all sections are shown, each preceded by a full-width heading item (`spansAllColumns()=true`, 28 px, the section label), and a chip click scrolls to its heading.
- Sections with no card in the current mode hide their chip.
- The selected chip is persisted in `DirectorV2PageState[page].chip`.

**Who.**
- The picker row (`who_picker=true`) is never shown. Its control (a combo) is the source of truth.
- The chip shows `<who_label>: <combo->getSelectedItemLabel()>`. The label is refreshed on combo commit and on the 2 Hz page clock, diff-guarded.
- Popover pick:
  - an item → `combo->setValue(item); combo->onCommit()`, and record `pin`;
  - "Follow rail" → clear `pin`.
- On `shell->castSelectionSignal(ids)` (user commits in the rail list), for every **built** page with a who, not pinned:
  - `adapter = ALKitWhoAdapters::get(who_adapter)`;
  - if `adapter && !ids.empty()`, `v = adapter(ids.front())`; if `v` is defined, set the combo and commit as above;
  - otherwise show the hint "<Name> isn't selectable here".

  The pin is persisted in `DirectorV2PageState[page].who_pin` (combo value as a string).

**Registry and API:**
```cpp
ALKitRow*  findRow(const std::string& widget) const;      // map built in postBuild
ALKitRow*  findRowBySetting(const std::string& setting) const;
ALKitCard* findCard(const std::string& id) const;
const std::string& pageKey() const;
void requestLayout();                                     // coalesced via doOnIdleOneTime
void relayoutNow();                                       // width-driven: grid->reshape(width)
void applySearch(const ALKitSearch::Hits& hits);          // hides non-matching rows/cards (ignores chips and modes)
void clearSearch();
bool revealRow(ALKitRow* row, bool flash);                // passive mode switch, chip, option, More options, scroll, flash
bool revealCard(ALKitCard* card, bool flash);
void setSearchText(const std::string& q);                 // proxy field sync from the shell
void resetPage();                                         // one undo step; excludes master, mode, who, presets
```

**Draw.** The page `draw()` only:
- updates the Undo/Redo enabled state via an `ALRevisionGate` on `history.revision()`;
- shows or hides the banner by `masterSwitch->isOn()`;
- updates the who chip label on its `ALLiveReadoutClock` (2 Hz) while on screen.

### 3.6 ALKitLazyPage: `alkitlazypage.h/.cpp`, tag `kit_lazy_page`

`class ALKitLazyPage : public LLPanel`.

**Params:** `page_file` (Mandatory), `master_setting` (for the rail and tab on-pip).

**`LLPanel* ensureBuilt()`:**
1. Returns the content if already built.
2. `LL_PROFILE_ZONE_NAMED_CATEGORY_UI("Kit page build")`; start an LLTimer.
3. `content = LLUICtrlFactory::getInstance()->createFromFile<LLPanel>(page_file, this, LLDefaultChildRegistry::instance())`. The root is `panel` or `kit_page`.
4. Reshape the content to `getLocalRect()` (follows all).
5. Call `shell->scrollFit().adoptPage(this, content)` (section 3.15).
6. Log `Kit page <name> built in <ms> ms` (`LL_INFOS("DirectorKit")`).
7. Emit `shell->pageBuiltSignal(getName(), content)`.

`onVisibilityChange(true)` calls `ensureBuilt()` **before** `LLPanel::onVisibilityChange` (the safety net for the self-test's `selectTab`).

### 3.7 ALKitShell: `alkitshell.h/.cpp`, tag `kit_shell`

`class ALKitShell : public LLPanel`.

**Params:**
- `index_file` (default `director_kit_index.xml`);
- `Multiple<RailItem> rail_item`, where RailItem = `group label icon master_settings` (comma list for the on-pip).

**Owns:**
- the rail (`kit_rail`: buttons `kit_rail_<group>`, active = KitControlBg + accent dot, hover KitRailHover);
- the roster container (the XML panel `kit_cast_roster`);
- the tab strip (`kit_tabstrip`: buttons `kit_tab_<page>`, current = 600 weight + 2 px accent underline; on-pip when the page master is off, as a hollow 6 px ring);
- the mode proxy (`kit_mode_seg_<i>`);
- help (`kit_help` and `kit_help_strip`);
- ALKitHistory, ALKitSearch, the search results popover (`kit_search_results`) and the cast popover (`kit_cast_popover`).

**Lifetime.** The shell owns the search debounce/tokenizer, history gesture timer, layout coalescer, and every scoped signal connection as members; destruction cancels them. Every queued idle callback captures `LLHandle<ALKitShell>` or `LLHandle<ALKitPage>` and checks it before access. There is no static current-shell/help pointer: callers resolve the containing `ALKitShell` from their live view/floater. Who/mode adapters are stateless manager functions registered idempotently and never capture a panel `this`. These rules are mandatory because the Director has no `reuse_instance` and is destroyed on close.

**Constraint layout.** `reshape()` derives everything from W×H. It is the only place shell coordinates are computed.
- `railLabels = W ≥ BP_RAIL` and `helpPanel = W ≥ BP_FULL`. **Narrow → wide** needs `W ≥ BP + BP_HYST`; **wide → narrow** happens at `W < BP`. The current state is kept in members.
- Transport: bottom, TRANSPORT_H, full width.
- Rail: left, RAIL_W or RAIL_ICON_W, from the top down to the transport.
- Help: right, HELP_W, when `helpPanel`.
- The main column lies between them:
  - tab strip at the top (TABSTRIP_H);
  - help strip at the bottom (HELP_STRIP_H) when `!helpPanel`;
  - `director_tabs` fills the rest.

**Expected widths** (these must hold; verify in the TUT test with a pure `computeShellLayout(W,H)`):

| W | Rail | Help | Main | Page content C | Columns | Track |
|---|---|---|---|---|---|---|
| 1160 | 176 | panel 224 | 760 | 728 − scrollbar | 2 | ≥ 120 |
| 1141 | 176 | strip | 965 | ~919 | 2 | ≥ 120 |
| 915 | 56 | strip | 859 | ~813 | 2 | ≥ 120 |
| 850 | 56 | strip | 794 | ~748 | 2 | ≥ 120 |

Columns = 2 when (C − GRID_GAP)/2 ≥ CARD_MIN_W, otherwise 1. The page tab strip never wraps. When the tabs overflow, it scrolls horizontally on the wheel (via `LLSliderWheel::noteScroll(this)`) and shows ‹ › arrows.

**Roster.**
- In label mode the rail is the groups (8 × RAIL_ITEM_H), then `kit_cast_roster`: a header "CAST n" + tool buttons `btn_add_you`, `btn_add_animesh`, `btn_focus` and `btn_remove` (22×22, existing icons), then `cast_list` (fills; min 120), `group_assign_combo` and `cast_hint`.
- In icon mode, or when the roster would be < 120 px, `kit_cast_roster` is **reparented** into `kit_cast_popover`, opened by the transport chip `btn_cast_chip` ("Cast n"). In label mode it goes back into the rail.
- A reparent happens only on a mode change. Widget pointers stay valid.

**Signals:**
```cpp
using page_built_signal_t = boost::signals2::signal<void(const std::string&, LLPanel*)>;
using cast_signal_t       = boost::signals2::signal<void(const uuid_vec_t&)>;
boost::signals2::connection setPageBuiltCallback(const page_built_signal_t::slot_type&);
boost::signals2::connection setCastSelectionCallback(const cast_signal_t::slot_type&);
void setCastSelection(const uuid_vec_t& ids);   // called by LLFloaterDirector on a user cast-list commit
```

**Navigation:**
```cpp
bool selectPage(const std::string& page_name);           // group + page, lazily built, records DirectorLastTabName via the floater's tab commit
LLPanel* ensurePageBuilt(const std::string& page_name);
ALKitPage* currentKitPage() const;
struct ALKitTarget { std::string page_key, card_id, widget, setting; S32 option = -1; };
bool navigateTo(const ALKitTarget& t);                   // used by search, link rows/cards and the help "Go"
```
`navigateTo`:
1. Clears the search.
2. Selects the page, building it.
3. Kit page: `revealRow` / `revealCard`.
4. Legacy page: `findChild<LLView>(widget)`. If found, scroll the nearest LLScrollContainer ancestor with `scrollToShowRect`, then draw a 1.5 s accent outline overlay (`ALKitShell::draw` paints one rect while the timer runs).

**Popovers:** `ALKitPopover` (tag `kit_popover`, an LLPanel).
- Params: `anchor` (widget name, resolved once in shell postBuild), `side` (`above|below`).
- `show()` positions it against the anchor in shell coordinates, clamped inside the shell.
- It hides on Esc, on another popover opening, or on a mouse-down outside it (`ALKitShell::handleMouseDown`).
- It is XML-hidden by default, so the audit treats it as an alternate.

**Help** (ALKitHelp, `alkithelp.h/.cpp`):
- The panel shows a kicker (Group › Page, uppercase 11), a title (15/600), help text (12, word-wrap, scrolls, max 260 px), meta lines, and a footer "Right-click a cast member for subject and mark actions."
- Meta lines: Default, Range (min–max unit), Unit, Setting, Widget, "In More options" when advanced. For dials: "Range a°–b°".
- Card hover shows the card help (the map card shows "Drag a light to aim it · arrows on the focused map").
- The strip is a custom-drawn single line "<title> — <text>", ellipsized (an intentional truncation in a custom view, not an LLTextBox).
- It updates only on `showRow`, `showCard` or `showPage` events. Zero per-frame cost.

**Transport** is the XML `kit_bar "kit_transport"`; contents in section 4.6. ALKitBar (tag `kit_bar`, `alkitbar.h/.cpp`):
- Children are laid left to right with an 8 px gap and 10 px side padding, vertically centred, using their declared `width`/`height` (the only kit container whose children may declare size).
- Param `flex` names one child that absorbs the leftover width; children after it are right-aligned.
- Param `collapse_order` is a comma list of child names hidden (via `setVisible`) in that order while the sum of widths exceeds the bar.

**UI ms chip** (`kit_ui_ms`, only when `KitShowUIMs`):
- LLFloaterDirector measures its own `draw()` with an LLTimer into an EMA (α 0.1).
- The chip text "UI x.x ms" is set at 2 Hz through `setTextIfChanged`.

### 3.8 ALKitSearch: `alkitsearch.h/.cpp`

**Index file.** `skins/default/xui/en/director_kit_index.xml`, written by the generator. This is the source of the index, a deliberate change from the plan's per-page XML parse, because the data rows are the truth. The format is:
```
<?xml version="1.0" encoding="utf-8"?>
<director_kit_index version="2" fields="15"><![CDATA[
<one entry per line, exactly 15 TAB-separated fields, \t \n \\ escaped>
]]></director_kit_index>
```
Fields, in order: `group_key, page_key, page_label, group_label, section_id, card_id, card_title, option_index, widget, label, kind, setting, flags(a=advanced,l=link,x=legacy), keywords, help`.

The loader requires version 2 and `fields="15"`, validates exactly 15 decoded fields on every line, and rejects the whole index with a clear log message on mismatch; generator and loader share one field-count constant.

**Loading** is read as text (no XML DOM) via `gDirUtilp->findSkinnedFilename(LLDir::XUI, file)`.
- The bytes are read once on the first idle after open.
- Lines are tokenized 400 per idle tick by a shell-owned repeating callback carrying the shell handle; closing the floater cancels it.
- Log `KitSearch index <n> entries <ms> ms`.

**Tokens.** For each field (label, card title, keywords, help, setting, widget): lower-case, split on non-alphanumerics, and split camelCase (`CineLightRigKeyYaw` → cine light rig key yaw). Tokens are stored as interned `std::vector<U32>` ids per field.

**Query.**
1. Split the query the same way.
2. An entry matches when **every** query token is a prefix of at least one entry token in an eligible field.
3. Query tokens of length ≤ 2 are eligible only for label, card title and keyword tokens (word-prefix), never help.

**Score** = the sum over query tokens of the best field score:
- label starts with the token (whole label): 100;
- label word prefix: 80;
- card title: 60;
- keywords: 50;
- help: 30;
- setting: 20.

Tie-break: current page, then group order, then data order.

**Duplicates.** The rollout's `kit_pages.json.homes` table names the sole writable home for every setting that appears on multiple pages. Search collapses entries with the same non-empty `setting` into the writable home; only generated `link` rows and explicitly read-only occurrences contribute an "also on <pages>" list. A second writable occurrence is a generator/lint error. P0's two-page data follows the same rule even before the full homes table lands.

**UI.**
- The proxy field fires `setQuery(text)`, debounced 120 ms by a shell-owned `LLEventTimer` one-shot. Never in `draw()`.
- The current kit page calls `applySearch(hits on this page)`; chips show hit dots. Filtering ignores the currently selected chip. Navigating to a hit may passively select its mode, selector option, and More state through `revealRow`; the mode path is the registered adapter above, so the owner's controls remain live.
- `kit_search_results` lists up to 12 other-page hits as "Group › Page › Card · Label", with a "More" tag for advanced. The header reads "N on this page · M elsewhere", and the footer "Showing the best 12 of K. Add a word to narrow it down."
- Keys in the field:
  - **Enter** jumps to the top hit (`navigateTo`) and is consumed, so ACTION does not fire;
  - **Up/Down** move the highlight in the results;
  - **Esc** clears, then blurs.
- **Budget:** `query()` ≤ 5 ms on the full index. Log `KitSearch query "<q>" <ms> ms <hits>` when the self-test is active.

### 3.9 Presets: `alkitpresets.h/.cpp`

```cpp
class ALKitPresetProvider {
public:
    struct Entry { std::string id; std::string label; bool builtin; };
    virtual ~ALKitPresetProvider() = default;
    virtual const std::string& key() const = 0;
    virtual void list(std::vector<Entry>& out) const = 0;
    virtual bool apply(const std::string& id) = 0;            // the caller wraps it in one history gesture
    virtual bool canSaveUser() const { return false; }
    virtual bool saveUser(const std::string& name) { return false; }
    virtual bool canDelete(const std::string& id) const { return false; }
    virtual bool deleteUser(const std::string& id) { return false; }
    virtual U64  revision() const = 0;                         // the list changed
    virtual const std::vector<std::string>& trackedSettings() const = 0; // for "(modified)"
};
class ALKitPresetRegistry : public LLSingleton<ALKitPresetRegistry> {
    void add(std::unique_ptr<ALKitPresetProvider>); ALKitPresetProvider* get(const std::string& key);
};
class ALKitSnapshotStore {   // user presets: user_settings/director_presets/<key>.xml (LLSD map name -> {setting: value})
public:
    explicit ALKitSnapshotStore(std::string key, std::vector<std::string> settings);
    std::vector<std::string> names() const; bool save(const std::string& name); bool load(const std::string& name);
    bool remove(const std::string& name); U64 revision() const;
};
```

**`rig_rim` provider (P0).**
- Builtins come from `ALRigRimController::presetNames()` and `apply` calls `ALRigRimController::applyPresetByName`.
- User presets use ALKitSnapshotStore over the Rim page's row settings **minus** `CineRigRimEnabled`, `CineRigRimDebugRimOnly`, `CineRigRimIncludeProbes` and `CineRigRimDebugView`.
- Ids: `b:<name>` and `u:<name>`.

**ALKitPresetStrip** (page bar, provider mode):
- The combo shows "Custom" first, then builtins (separator "Built-in"), then "— Saved —" and user entries.
- After `apply`, the strip remembers `(id, values of trackedSettings)`. A tracked setting changing outside an apply shows "<Name> (modified)" plus a "Revert to <Name>" item. Detection uses the ALKitSettingWatch signal, not polling.
- "Save as…" opens an inline name editor (LLLineEditor in the bar, Enter saves). Delete is enabled only for `u:` entries and asks for confirmation through the existing `GenericAlertYesCancel` notification (Codex verifies the name).

**Rig page** (`preset_mode=widgets`): the existing `cine_setup_combo`, `cine_setup_save` and `cine_setup_delete`, with ALPanelCineLightRig logic unchanged.

**Card-header presets:** the existing combos (`cine_flare_preset`, `cine_easy_flare_preset`, `cine_night_mask_preset`, `cine_easy_night_mask_preset`, `cine_fixture_preset`), reparented into headers. Their C++ is unchanged.

### 3.10 Bindings and amber: `alkitbinding.h/.cpp`

```cpp
class ALKitBinding {
public:
    virtual ~ALKitBinding() = default;
    virtual LLSD get() const = 0;
    virtual LLSD getDefault() const = 0;
    virtual void set(const LLSD& v) = 0;            // used by reset and undo
    virtual bool hasDefault() const { return true; }
    virtual bool isDefault(S32 decimals) const;     // display-precision compare (below)
    virtual boost::signals2::connection connectChanged(std::function<void()> cb) = 0;
    virtual std::string describe() const = 0;       // help meta "Setting"/"Model"
};
class ALKitSettingBinding;       // one LLControlVariable
class ALKitSettingsListBinding;  // N settings: amber if any non-default; set() unsupported (reset = reset each)
class ALKitWidgetBinding;        // get = ctrl->getValue(); set = ctrl->setValue(v); ctrl->onCommit(); default from row default_value
class ALKitVecBinding;           // Vector2/Vector3 setting <-> component spinners
class ALKitBindingRegistry : public LLSingleton<ALKitBindingRegistry> {
    // Owner code registers bindings for code rows (later phases: actor-model rows). Keyed by (page root, widget).
    void add(LLView* page_root, const std::string& widget, std::function<std::unique_ptr<ALKitBinding>()> factory);
    std::unique_ptr<ALKitBinding> make(LLView* page_root, const std::string& widget) const;
};
```

**Display-precision compare.**
- F32: `llround(v·10^d) == llround(def·10^d)`, where d = row decimals, else the control's `decimal_digits`, else 3.
- S32, U32 and bool: exact.
- Colour and vector: per component at 3 decimals.
- String: exact.

**ALKitSettingWatch** (singleton):
- `connection watch(const std::string& setting, std::function<void(const LLSD& newv, const LLSD& oldv)>)`.
- One `LLControlVariable::getSignal()` connection per setting, fanned out to listeners. Listeners hold scoped connections and are refcounted.
- Rows, cards, the preset strip and ALKitHistory all use it. **Amber is never computed per frame.**
- `debugRecomputeAll()` returns the ms taken to recompute amber for every live row; the self-test uses it to measure the "storm" budget (≤ 2 ms).

### 3.11 ALKitHistory: `alkithistory.h/.cpp`

One instance per ALKitShell (Director-wide undo).

```cpp
class ALKitHistory {
public:
    enum EOrigin { MOUSE, WHEEL, KEY };
    struct Entry { bool widget; std::string key; LLHandle<LLUICtrl> ctrl; LLSD oldv, newv; };
    struct CustomEntry { std::string key; std::function<bool(bool redo, std::string& reason)> replay; };
    struct Step  { std::string label, context, page; U64 scene_generation; std::vector<Entry> entries; std::vector<CustomEntry> custom; };
    static constexpr size_t MAX_STEPS = 200;

    void beginGesture(EOrigin o, const std::string& page_key);  // from LLFloaterDirector mouse/wheel overrides
    static void noteKey();                                      // one line at the top of LLViewerWindow::handleKey (static timestamp)
    void watchSettings(const std::string& page_key, const std::vector<std::string>& names);
    void recordWidget(LLUICtrl* ctrl, const LLSD& oldv, const LLSD& newv, const std::string& label);
    void recordCustom(const std::string& key, const std::string& label,
                      std::function<bool(bool redo, std::string& reason)> replay);
    void barrier(const std::string& reason);                   // close gesture, clear undo/redo, increment scene generation
    static void globalBarrier(const std::string& reason);      // resolve the live Director at call time; safe no-op if absent
    bool canUndo() const; bool canRedo() const; std::string undoLabel() const; std::string redoLabel() const;
    void undo(); void redo();
    U64  revision() const;
    void registerContext(const std::string& key, std::function<std::string()> fn);
    struct ScopedSuspend { ScopedSuspend(); ~ScopedSuspend(); };  // static depth counter; usable from any code
};
```

**When a change is recorded.** A setting change from ALKitSettingWatch is recorded only when **all** of these hold:
- the history is not replaying, and `ScopedSuspend` depth is 0;
- **either** a pointer gesture is open (a mouse down in the Director, plus 350 ms settle after the button is released, or a wheel event plus 500 ms), **or** the keyboard focus is inside the Director floater and `now − noteKey time ≤ 0.5 s`;
- the setting is watched for the gesture's page (the page under the mouse at gesture start, or the page holding keyboard focus) or for `chrome`.

**Steps.**
- One gesture is one Step. Entries for the same key coalesce: keep the first `oldv` and the latest `newv`. No-op entries are dropped. Empty steps are not pushed.
- A push clears redo. The stack is capped at MAX_STEPS.
- Label: the first entry's row label ("Rig › Lights › EV"), else the setting name.
- Context: `registerContext` fn for the page's `undo_context` (Rig: `"slot:<n>"`).

**Gesture closing.** An LLEventTimer at 10 Hz exists only while a gesture is open. The gesture closes when `!gViewerWindow->getLeftMouseDown()` (`llviewerwindow.h:312`) and the settle time has passed.

**Undo and redo.**
- Refused with a toast ("Switch back to <context> to undo this") when the Step context differs from the current one.
- Replay sets `mReplaying` and runs:
  1. widget entries in reverse, each `setValue(oldv)` then `onCommit()`;
  2. setting entries in reverse, each `control->set(oldv)`.

  Exact setting values therefore win.
- Redo is symmetric, using `newv`.
- Typed custom entries replay stable, fine-grained model operations. They are the only history form for Prism registry mutations and other non-setting models; a custom entry's watched settings are excluded from ordinary setting capture. A replay failure leaves the step in place and shows its reason—there is no partial fall-through to a snapshot restore.
- A Step is valid only in the scene generation in which it was recorded. `barrier(reason)` closes/discards an open gesture, clears undo and redo, increments the generation, and logs the reason.

**Who suspends:**
- `ALPanelCineLightRig::onAnchorSelected` (a slot switch rewrites the CineLightRig settings).
- ALKitPage passive mode switches.
- `navigateTo`.
- Director scene apply, `ALCineLightRigManager::resetAllToSelf`, every `setSelectedSlot` caller, and preset/cue apply paths outside the active Director gesture use `ScopedSuspend` and then a barrier. Scene apply currently calls the rig manager at `llfloaterdirector.cpp:1664`; slot selection and reset synchronously rewrite settings at `alcinelightrigmanager.cpp:698-735`; those writes must never be left as fragmentary undo entries.
- An in-Director preset/reset that is deliberately represented as one complete Step uses `ScopedSuspend` around the bulk writer and records the complete typed/setting entry set itself; it does not also call a barrier. External Lightbox presets and cue playback call the barrier because the Director did not initiate a complete step.

External writers use `ALKitHistory::globalBarrier(reason)`. It resolves `LLFloaterReg::findTypedInstance<LLFloaterDirector>("director")` at call time, obtains that floater's live shell/history if present, and calls `barrier`; it stores no static shell pointer or handle. `ALTron::applyPreset`, `ALRotoInk::applyPreset`, and each Lightbox preset apply call it once after their suspended bulk write (the Tron call path begins at `alfloaterlightbox.cpp:832`). Cue playback calls it at playback **start** and **stop**, not per cue tick. The cue player owns `std::unique_ptr<ALKitHistory::ScopedSuspend> mHistorySuspend`: create it before the start barrier, retain it across ticks, reset it before the stop barrier, and reset it on every abort/destructor/logout path. Continuous cue writes therefore cannot create or repeatedly erase history. Tests cover Director absent, closed, reopened, and playback abort.

Codex must verify that `ALCineLightRigManager::setSelectedSlot` writes its settings synchronously. If it defers, use `ScopedSuspend` plus a `suspendFrames(2)` latch.

**Keys:** Ctrl+Z and Ctrl+Y (section 3.12). `ALKitHistory` is the only Director key-history owner; page models, including VCam, register custom entries here and never claim Ctrl+Z independently.

### 3.12 Keyboard (v2 only; Director focus only)

These facts are **proven** by `llviewerwindow.cpp:3436-3539`:
- Ctrl and Alt keys reach the menu accelerators **before** the focused view, unless `gFocusMgr.keyboardFocusHasAccelerators()` is true.
- Plain keys go to the focused view chain first.

v2 therefore overrides `LLFloaterDirector::hasAccelerators()` to return `mIsV2`. Ctrl/Alt keys then go first to the focused Director chain, and anything unhandled falls through to the viewer menus as before. Do not assume every text control consumes undo: `LLLineEditor::handleKeyHere` only delegates selection/special keys (`lllineeditor.cpp:1809-1831`), and an unhandled key bubbles to its parent (`llview.cpp:1049-1053`).

`LLFloaterDirector::handleKeyHere` (v2 branch; the v1 branch is unchanged):

| Key | Action |
|---|---|
| Esc | First close a popover, else clear search, else CUT while running or counting down, else stock |
| Enter | ACTION via the existing default button. Text fields, spinners, combos and the search field consume Enter first. |
| `/` | Implemented in `LLFloaterDirector::handleUnicodeCharHere`, not `handleKeyHere`; focus the current page's search only when focus is not a text-entry widget |
| Ctrl+Z / Ctrl+Y | `history.undo()` / `redo()` only when focus is not an `LLLineEditor`, `LLTextEditor`, `LLSpinCtrl`, or a spin control's descendant editor |
| PgUp / PgDn | Previous / next chip. ALKitScroller passes PgUp/PgDn up by returning false; keyboard page scrolling is dropped, the scrollbar and wheel remain. |
| Shift+PgUp / PgDn | Previous / next page in the group |
| Alt+PgUp / PgDn | Previous / next group |
| Home | Reset the focused control's row (when the control did not consume Home) |
| Arrows | Handled by the controls (sliders, spinners, dials, map) |

**Not taken:**
- Ctrl+F and Ctrl+1–9 and 0 stay with the viewer.
- Ctrl+Shift+Z is untouched.
- F2–F8 and the bare and Alt digits stay with ALDirectorHotkeys, which is unchanged.

`/` must use the Unicode path: with keyboard focus, printable keys below 0x80 return to character handling before `handleKeyHere` (`llviewerwindow.cpp:3409-3420`) and then bubble through `handleUnicodeChar` (`llviewerwindow.cpp:3684-3693`; `llview.cpp:1103-1129`). Both the key and Unicode handlers use one `isTextEntryFocus()` helper that walks focus ancestors; `/` typed in search, a line editor, a text editor, combo editor, or spinner editor remains text. Ctrl+Shift+Z remains untouched. VCam installs no second handler.

The hotkeys popover (`btn_hotkeys`) lists exactly the mockup table:
- Enter: ACTION;
- Esc: close, clear or CUT;
- F8 / F5;
- `/`;
- Ctrl+Z/Y;
- PgUp/PgDn;
- Shift+PgUp/PgDn;
- Alt+PgUp/PgDn;
- arrows;
- Home;
- Space/Enter;
- 1–9 and Alt+1–9.

### 3.13 ALCompassDial arc mode (`indra/llui/alcompassdial.h/.cpp`)

**New Params.** Existing users are unaffected by the defaults.

| Param | Default | Meaning |
|---|---|---|
| `mode` | `"compass"` | `compass` or `arc` |
| `min_val` / `max_val` | compass −180/180; arc −90/90 | value range |
| `increment` | 1 | keyboard step (Shift ×10; PgUp/PgDn ±15) |
| `wheel_step` | 5 | wheel step |
| `wheel_adjust` | true | participate in the global slider-wheel rule; false for scrubbers |
| `decimal_digits` | 1 | value rounding on commit |
| `kit_geometry` | false | use the new kit geometry; false preserves existing dials pixel-for-pixel |
| `arc_zero` | `"right"` | 0 = horizon, positive = up, negative = down |
| `show_cardinals` | true for compass | N/E/S/W letters |
| `show_range_labels` | true for arc | min and max under the baseline ends |
| `face_color`, `range_color`, `track_color`, `focus_color` | Kit colours | |

**Value.**
- Compass, span ≥ 360: wraps into `[min,max)`; otherwise it clamps to `[min,max]`.
- Arc: clamps.
- `setValue` clamps and wraps. `getValue` returns degrees.

**Arc angle mapping:** horizon mapping is binding. Details are in the binding-correction block below.

**Geometry:** legacy compass geometry remains the default. Kit compass and horizon-arc geometry are in the binding-correction block below.

**Input.**
- Mouse down captures and sets the value from the point, with a 3 px dead zone at the hub. Compass retains its existing inverse; arc uses the horizon inverse below.
- Hover while captured sets and commits. Mouse up releases and commits. These are existing semantics.
- Values outside ±180 keep their winding and move the short way.
- Wheel: if disabled, or `wheel_adjust` is false, or `LLSliderWheel::guarded(this)` → `LLSliderWheel::keepAlive()` when guarded, then return false. Otherwise ±`wheel_step` and commit.
- Keys (new `handleKeyHere`): Left/Down −step, Right/Up +step, Shift ×10, PgUp/PgDn ±15, Home = control default (`getControlVariable()->getDefault()` if bound). Commit after each. Return true only for these keys.
- `setTabStop(true)`.

**Binding correction (supersedes the `arc_zero`, arc mapping, arc geometry, and arc pointer bullets above).** The binding decision is horizon = 0. Add Params `wheel_adjust` (bool, default true) and `kit_geometry` (bool, default false), and change `arc_zero` default to `"right"`. Generator-created kit dials set `kit_geometry="true"`; existing Actor Mover/Ghost Studio dials leave it false and keep the current `radius=min(w,h)/2-12` geometry and outside letters exactly (`alcompassdial.cpp:71-78`).

For an arc, map a true tilt value to `a=clamp(v,-90,+90)` measured counter-clockwise from the positive-x horizon: 0 points right, +90 up, -90 down. A non-tilt `[min,max]` maps linearly to `[-90,+90]`. Centre is `(6s,h/2)` and `r=min(20s,h/2-3s,w-9s)`, `s=min(w,h)/48`, so the right half-circle stays inside the widget. Draw the half arc from -90 through 0 to +90, a horizontal 0 baseline/tick to the right, the maximum label/tick above, and the minimum below; no label may extend below the widget. Needle endpoint is `(cx+r*cos(a),cy+r*sin(a))`. Pointer input uses the exact inverse `atan2(y-cy,x-cx)`, clamps to the right half, and then applies the inverse range map. This replaces the earlier up-zero centre/baseline text.

Compass wrapping uses `[min_val,max_val)`, not a hard-coded signed range. In particular `cine_catchlight_angle` remains in 0..360, so 360/0 normalization does not create a false amber/default difference. The wheel returns false when `wheel_adjust=false`; otherwise it follows the global guard described above.

**Binding (new).** `onCommit()` override: `if (getControlVariable()) setControlValue(LLSD(round(v, decimal_digits)));` then `LLUICtrl::onCommit()`. Existing dials have no control_name, so they are unchanged.

**Draw.** Pure rendering; no other work.

### 3.14 ALLightRigMap: `allightrigmap.h/.cpp`, tag `light_rig_map`

`class ALLightRigMap : public LLUICtrl`.

**Params:**
- `selector_card` (default `kc_lights`), the Lights selector card to keep in sync;
- `view_setting` (default `DirectorV2LightMapView`, S32: 0 Top, 1 Side, 2 2.5D; default 2);
- `side_from_setting` (default `DirectorV2LightMapSideFrom`, S32: 0 = Side view seen from camera-left, 1 = from camera-right; default 0).

**Light identity.** Role i ∈ {0 Key, 1 Fill, 2 Back, 3 BG} uses these settings (prefix `CineLightRig` + `Key|Fill|Rim|Bg`):
- `…Yaw`, `…Pitch`, `…On`, `…OffsetZ`.

Rig-wide: `CineLightRigMirror`, `CineLightRigOrbitYaw`, `CineLightRigOrbitPitch`, `CineLightRigRadius`.

All are read with `static LLCachedControl` arrays.

**Frames.** When the selected rig is live and no gesture is active, the display source is `selected().lastFrame().mProj[i].mOffX/Y/Z`. Those are the eased positions: `lastFrame` is built from `mCurrentLive`, and `mCurrentLive` approaches the settings target over the transition (`alcinelightrig.cpp:1937-1958`, `2954-2961`). They already contain orbit, subject-scaled effective radius and OffsetZ (`alcinelightrigmodel.h:213-235`; `alcinelightrigmodel.cpp:1585-1621`). They are display-only and are never a drag baseline.

WP1 adds a read-only `mapTargetFrame()`/revision accessor on the selected rig. It computes the four **uneased settings targets** with the same sanitized settings, resolved subject scale and resolved live avatar-or-object facing as `computeLive`/`computeFrame`, but allocates and mutates nothing. While dragging, only the grabbed light is drawn from that target frame plus the absolute gesture result; the other lights remain live. This makes the grabbed position agree with the values the dials target even while `mCurrentLive` catches up.

Object-target Mirror remains enabled. `tickShared` default-constructs `Transforms` every tick (`alcinelightrig.cpp:2453`), resolves `root = object->getRootEdit()` in the object branch (`:2511`), but currently leaves `mFacingAzimuthDeg` at 0. WP1 makes the approximately six-line engine correction there: compute `fwd = LLVector3(1.f, 0.f, 0.f) * root->getRenderRotation()`, derive `facing = atan2(fwd.mV[VY], fwd.mV[VX]) * RAD_TO_DEG`, and publish the finite value through `transforms.mFacingAzimuthDeg`, using the same forward-axis/finite-guard rule as the avatar path at `:2637-2641`. `computeLive`, `computeFrame`, and `mapTargetFrame()` then mirror around the object's live front rather than world east. The Mirror row is never disabled for an object; a rotated-prop in-world check proves the map and emitted lights rotate together.

When the rig is not producing a usable live offset, the fallback is deliberately **unmirrored**: `eff_yaw=wrap180(base+orbitYaw)`, `eff_pitch=clamp(base_pitch+orbitPitch,±85)`, `r=CineLightRigRadius`, and `z=r*sin(eff_pitch)+OffsetZ`. Draw “Unmirrored preview · rig off”. Fallback drag uses `sigma=+1` in the absolute mouse-down formula regardless of the stored Mirror setting. This is internally consistent; it does not use `F=0` with the mirror formula.

**Display transform.**
- Camera bearing φc = `atan2(−at.y, −at.x)` in degrees, from `LLViewerCamera::getInstance()->getAtAxis()`, read in `draw()`. If `|at.xy| < 0.05`, keep the previous φc (member).
- For a live offset `(ox,oy,oz)`, world yaw is `atan2(oy,ox)`, horizontal radius is `hypot(ox,oy)`, and display yaw is `t=wrap180(world_yaw−φc)`. For fallback, use the unmirrored values above. `t=0` points at the camera and positive t is camera-right.

**Displayed elevation:** `p_disp=atan2(oz,hypot(ox,oy))` for an ordinary live offset. Fallback and the dragged target use `p_disp(base,orbit,h,r)=atan2(r·sin(clamp(base+orbit,±85))+h, r·cos(clamp(base+orbit,±85)))`. For a live rig, the target frame supplies subject-scaled `r`: `EmitterState::mLightRadius` is `effective_radius * 2.2` (`alcinelightrigmodel.cpp:1703`), so `r=mLightRadius/2.2`. Assert that this is consistent within float tolerance with `sqrt(ox²+oy²+(oz-h)²)` for the same target frame (`alcinelightrigmodel.cpp:1615-1621`). The exact same captured `r` is used to draw and invert the gesture. Rig-off fallback uses plain `CineLightRigRadius` for both. The canvas footer shows the actual live `hypot(ox,oy,oz)` distance when available, otherwise the fallback radius.

**Colour:**
- When `selected().lastFrame().mProj[i]` has a non-zero offset and `mOn`: (mSR, mSG, mSB) normalised so the max channel = 1. That is the live gel, profile and fixture colour.
- Otherwise the role colour (KitMapKey, KitMapFill, KitMapBack, KitMapBg).
- **Off** (`…On` false): KitOffGrey, whatever the frame says.

**Projection.** The canvas has height MAP_CANVAS_H and width w; (cx, cy) = (w/2, H/2). R = min(0.42·H, 0.30·w). With t, p in radians:
- X = R cos p sin t; Y = R sin p; Z = R cos p cos t.
- **2.5D** (e = 24°) and **Top** (e = 90°):
  - sx = cx + X; sy = cy − Z·sin e + Y·cos e (LL y is up);
  - depth = Z·cos e + Y·sin e.
- **Side** (user 10/10: the old Side view did not say WHICH side you look from; Key at +45 and Fill at −45 landed on the same spot). s = +1 when seen from camera-left (camera on the right edge), −1 when seen from camera-right (camera on the left edge): sx = cx + s·Z; sy = cy + Y; depth (toward the viewer) = −s·X.
- Chest point: (cx, cy + 0.33R) in 2.5D and Side; (cx, cy) in Top.

**Which-side cues (all views; mockup v16 is the reference):**
- Labels are camera-relative, never compass: Top and 2.5D show "CAM L" (left edge), "CAM R" (right edge), "BACK" (far edge; in 2.5D beside the back tick, never behind the avatar) and "CAM" + marker at t = 0. No N/E/S/W.
- The avatar shows where it faces: Top = a nose wedge toward CAM; 2.5D = two eye dots on the head plus a floor wedge toward CAM; Side = a **profile** silhouette (head, nose, eye with pupil, hair at the back of the head, near arm lighter, feet pointing toward CAM) plus a short dashed amber gaze line toward CAM, mirrored when seen from camera-right.
- Side view has a **"From cam L ⇄" chip** at the top-left of the canvas (`kc_light_map_side_from`): a 22 px mini top view (globe circle, subject with nose toward a CAM triangle at the bottom, an eye glyph on the viewing side with a dotted sight line to the subject), the text "From cam L" / "From cam R", and a swap glyph. Click toggles `side_from_setting`. Tooltip: "Side view from camera-left (the subject's right side). Solid = lights on this side, hollow = far side behind the avatar. Click to look from the other side."
- The key under the map reads "near / far / off" in Side and "in front / behind / off" in Top and 2.5D.

**Layering (front, behind, off).**
- fr = smoothstep(−0.2, 0.2, cos t) in Top and 2.5D; in **Side** fr = smoothstep(−0.2, 0.2, −s·sin t) (the hidden axis there is left/right, so solid = on the viewer's side, hollow = far side behind the avatar). dn = clamp(depth/R, −1, 1); underK = pitch < 0 ? min(1, −p°/40) : 0.
- front layer = Top view || fr ≥ 0.5.
- Draw order:
  1. the floor ring (an ellipse with radii (R, R·sin e) in 2.5D, a circle in Top, a line in Side) and the meridian, in KitArcTrack;
  2. back-layer lights, depth-ascending;
  3. the subject silhouette (a capsule in KitDialRing; in Top a 6 px circle) and the near half of the globe shell;
  4. front-layer lights, depth-ascending;
  5. the camera marker "CAM" (Top and 2.5D: the bottom edge at the screen projection of t = 0; Side: the right edge from camera-left, the left edge from camera-right, with "BACK" on the opposite edge).
- Light radius: rr = on ? 4.6 + 2.6fr + 0.8dn : 6.2 + 0.6dn.
- **On, front:** solid disc (alpha (0.5 + 0.5fr)(1 − 0.35·underK)); glow disc rr + 6 with alpha 0.26·fr²·(1 − 0.5·underK); solid beam line to the chest point at alpha 0.2 + 0.45fr.
- **On, behind:** hollow ring 1.5 px with a fill of alpha 0.35 + 0.65fr; beam dashed 3/3.
- **Under** (underK > 0.05): the outline is dashed 2/1.6.
- **Off:** a grey disc (KitControlBg fill, KitOffGrey ring) with a 45° slash of length 1.44·rr, the tag "OFF", no beam, no glow. **Off never reads as "behind".**
- **Selected:** a ring at rr + 4, alpha 0.6, in the light colour.
- Label: the name 11 px to the right if sx ≥ cx, otherwise to the left.

**Legend** (MAP_LEGEND_H; 4 chips, each `kc_light_map_legend_<i>`): a dot (colour, or the slashed grey when off), the name, and "yaw° / pitch°" showing the **base** values (matching the dials). The yaw carries its camera-relative side: "R45°", "L135°", plain "0°" and "180°" on axis. Since R/L changes with the camera, the four cached strings are rebuilt only on the shared 10 Hz `ALLiveReadoutClock` while visible and applied with diff guards—never constructed in `draw()`. The helper is `yawSideText(t,base)` consistently in implementation and tests. A click selects; the selected chip has its border in the light colour.

**Input.**
- Mouse down on a light disc (hit radius rr + 4, topmost first) selects role i. It sets the Lights selector option, writing `DirectorV2SelectedLight`, which also moves the Rim per-light selector and the fixture role (section 5.1). Then it captures one immutable `DragStart`: pointer `(x0,y0)`; setting `baseYaw0`, `basePitch0`, `orbitYaw0`, `orbitPitch0`, `offsetZ0`; view, side and Mirror state; camera bearing; target-frame `t0,p0`; and `r0` derived above. It also captures the pointer-to-disc-centre grab offset. No hover reads a live/eased position or uses the previous hover's written value.
- Let `sigma = -1` for any mirrored live target (avatar or object) and `+1` for unmirrored or rig-off cases. Every hover computes `baseYawNew=wrap180(baseYaw0 + sigma·wrap180(tTarget−t0))` and `basePitchNew=baseFromDisplayPitch(pTarget,orbitPitch0,offsetZ0,r0,basePitch0)`. Thus every write is a pure function of `DragStart` plus the current pointer; it is never `setting += delta`.
- **Top-view drag:** `a0=atan2(x0−cx,−(y0−cy))`, `a=atan2(x−cx,−(y−cy))`, `tTarget=wrap180(t0+wrap180(a−a0))`, and `pTarget=p0`. This preserves the initial grab even when mouse-down is off-centre and makes zero movement an exact no-op.
- **2.5D drag (exact projection inverse):** subtract the captured grab offset, set `u=(x−grabX−cx)/R`, `v=(y−grabY−cy)/R`, and radially clamp `(u,v)` to the unit disc. With elevation `e=24°`, choose `d=±sqrt(max(0,1−u²−v²))` on the branch continuous with captured `d0=cos(p0)cos(t0)cos(e)+sin(p0)sin(e)`; at the silhouette choose the candidate minimizing wrapped angular distance to `(t0,p0)`. Invert the view rotation: `X=u`, `Y=v cos(e)+d sin(e)`, `Z=−v sin(e)+d cos(e)`, then `pTarget=asin(clamp(Y,−1,1))`, `tTarget=atan2(X,Z)`. Feed those targets to the absolute formulas above. This exactly inverts `projectLight`, rather than integrating a per-event `dx·k`, and preserves the front/back branch.
- **Exact elevation inverse:** define `p_disp(base,orbit,h,r)` exactly as above and `baseFromDisplayPitch(target,orbit,h,r,current)`. Solve `sin(q−target)=−h·cos(target)/r` for `q=base+orbit`, choose the valid branch closest to captured `q0=basePitch0+orbitPitch0`, then clamp the resulting base so q remains within ±85°. Both target drawing and this inverse use captured `r0`, including subject scaling.
- **Side drag:** subtract the grab offset and set `pTarget=asin(clamp((y−grabY−cy)/R,−1,1))`. If `abs(cos(pTarget)) < 1e−4`, yaw is visually unobservable at the pole, so keep `tTarget=t0`; otherwise solve `cos(tTarget)=clamp((x−grabX−cx)/(s·R·cos(pTarget)),−1,1)`. Choose the `±acos` branch continuous with captured `t0` (tie-break by captured depth), then use the absolute base formulas above. Using the exact cosine, rather than an artificial denominator floor, is required for zero-motion round-trip. The hint reads "Drag: ↔ toward/away from camera, ↕ height".
- During capture, camera motion, transition progress, setting signals caused by the gesture, and 10 Hz display refreshes do not replace `DragStart`. A view/side change or external slot/scene/preset write cancels the gesture before applying that change.
- Writes go through `gSavedSettings.setF32(<cached name>)` inside the input handler, so the dials update through their control bindings. One history gesture covers the drag.
- **Keyboard** (when the map is focused): Left/Right yaw ∓/±1 (Shift 10); Up/Down pitch ±1 (Shift 10); `[` / `]` previous / next light; Home resets the selected light's Yaw and Pitch to their defaults.
- The wheel is not used (returns false, so the page scrolls).
- The view comes from `view_setting`, and the Top / Side / 2.5D header segments write it.

**Cost.** Draw is ≤ 0.05 ms (4 lights, ~60 GL primitives, 8 strings). Per draw it reads LLCachedControls, lastFrame and the camera at-axis, and does nothing else.

**Pure maths** goes in `alkitmath.h` (header-only, no UI includes) for the TUT test:
- `wrap180`;
- `arcAngle(v,min,max)` and its inverse `arcValue(a,min,max)`;
- `pDisp(base,orbit,h,r)` and `baseFromDisplayPitch(target,orbit,h,r,current)`;
- `projectLight(view,sideFrom,t,p,R,cx,cy,&x,&y,&depth)`;
- `frontFactor(view,sideFrom,t)`;
- `yawSideText(t,base)` ("R45°" / "L135°" / "0°" / "180°");
- `effYaw(base,orbit,mirror,F)`;
- `dragTopAbsolute(start,pointer)`, `unproject25D(start,pointer)`, and `dragSideAbsolute(start,pointer)`;
- `computeShellLayout(W,H,prev_state)`;
- `packRows(widths, heights, columns)`.

### 3.15 Modified llui and newview helpers

**ALFlowGrid** (`alflowgrid.h/.cpp`):
- New param `pack` = `"masonry"` (default, unchanged behaviour) or `"rows"`.
- New interface `class ALFlowGridItem { public: virtual ~ALFlowGridItem() = default; virtual S32 heightForWidth(S32 w) const = 0; virtual bool spansAllColumns() const { return false; } };`.
- `pack="rows"` places items row-major: left, right, then the next row. Row height = max(item heights); items are top-aligned. A spanning item takes a full row.
- An item's height = `heightForWidth(column_w)` if it is an ALFlowGridItem, otherwise its rect height.
- Hidden items are skipped.
- Masonry ignores the new interface unless `fit_cards`, exactly as today. Remove `final` only if it is required.

**ALScrollFit** (`alscrollfit.h/.cpp`):
1. `class SelfSizedDocument { public: virtual ~SelfSizedDocument() = default; };`. `fitContainer()` returns immediately when `dynamic_cast<SelfSizedDocument*>(scrolledView(sc))`.
2. `void Controller::adoptPage(LLPanel* tab_page, LLPanel* content)`:
   - Kit content (it contains an ALKitScroller, found by a `findChild` of the page's scroller): install focus-reveal on that scroller (`mFocus.install(sc, doc, doc)`), then `enableSliderWheel(content)`. No wrap, no fit.
   - Legacy content (an ALKitPage with `legacy=true`):
     - `host = <P>_legacy_host`;
     - if `host` has no direct scroller and its name is not in `mUnwrapped`, call `wrapTabPanel(host, mMins.get())`;
     - install focus-reveal on its direct scrollers;
     - `fitPage(tab_page, mMins.get())`;
     - `enableSliderWheel(content)`.

   `fitPage(lazy_page)` must reach the host's scrollers. It already recurses through panels, but **verify** this with a TUT-free code read and state it in the review hand-off.

**ALKitScroller** (`alkitpage.cpp`): an LLScrollContainer subclass with `handleKeyHere` returning false for PgUp/PgDn (section 3.12). Everything else is inherited.

`ALKitDoc` always subtracts/reserves the scroller's scrollbar width, whether or not the bar is currently visible. A content-height transition therefore cannot change document width and oscillate between one/two columns.

`ALPanelCineLightRig::reshape` caches a one-shot “v2 flow grid lookup attempted” flag. When v2 intentionally has no `cine_light_rig_flow_grid`, it performs `findChildView` once and never recursively searches on later resize events; the current fallback lookup is at `alpanelcinelightrig.cpp:2811-2831`.

### 3.16 Settings, menu, colours, textures

**`app_settings/settings.xml`** (each with a Comment):

| Name | Type | Default | Persist | Purpose |
|---|---|---|---|---|
| DirectorUIV2 | Boolean | 0 | 1 | Use the new Director layout (restart) |
| DirectorV2RectBumped | Boolean | 0 | 1 | One-time bump of a saved rect below 1160×720 |
| DirectorV2CastRosterOpen | Boolean | 1 | 1 | Rail roster expanded |
| DirectorV2PageState | LLSD | {} | 1 | Per-page chip, card open/more state, who pin |
| DirectorV2SelectedLight | S32 | 0 | 1 | Linked light selection (Rig Lights, Rim per-light, map, fixture role) |
| DirectorV2LightMapView | S32 | 2 | 1 | Light map view (0 Top, 1 Side, 2 2.5D) |
| DirectorV2LightMapSideFrom | S32 | 0 | 1 | Light map Side view seen from (0 camera-left, 1 camera-right) |
| KitShowUIMs | Boolean | 0 | 1 | Show the Director UI ms chip |
| KitSearchGlobal | Boolean | 1 | 1 | Search covers all pages (off = current page only) |
| UILayoutAuditSizes | String | "" | 0 | Self-test audit sizes ("WxH" or default/min tokens, comma list) |

- **Rollout exposure.** P0 adds no `menu_viewer.xml` item and no normal-user toggle. Until the all-at-once final gate, only `ui_selftest.ps1 --set DirectorUIV2 TRUE` and Debug Settings may enable v2. The menu/default rollout change belongs exclusively to `DIRECTOR_KIT_ROLLOUT_SPEC.md` WP12 after its ship gate.
- **Registration.** `llviewerfloaterreg.cpp:776` becomes `gSavedSettings.getBOOL("DirectorUIV2") ? "floater_director_v2.xml" : "floater_director.xml"`. It is called from `LLAppViewer::init` (`llappviewer.cpp:1025`), after the settings load. Codex confirms that `initConfiguration` precedes it.

### 3.17 LLFloaterDirector (`llfloaterdirector.h/.cpp`): shell-agnostic

1. **Binder refactor.** Split `postBuild()` into `bindChrome(LLView* root)` and `bindPage(const std::string& page, LLView* root)`, using a static table `{page_name, member-function}`.
   - A widget's page is the v1 leaf panel that contains it in `floater_director.xml`. Codex derives this with a one-off script and pastes the mapping into a comment.
   - Chrome is anything not inside `director_tabs`: transport, scenes, legend, status, cast column, `group_assign_combo` and the context menus.
   - Every lookup inside `bindPage` uses `findChild` and tolerates absence. A `getChild` stays only in `bindChrome`.
   - **v1 path:** `bindChrome(this)`, then `bindPage(name, this)` for every page in the **original statement order**, then the existing `installForTabs` and `fillPage` lines. The behaviour must be identical; the reviewer diffs v1 call order statement by statement.
   - **v2 path:**
     - `bindChrome(this)`;
     - `mShell->setPageBuiltCallback(bindPage)`;
     - `mScrollFit.setUnwrappedPages({all group names, all page names})`;
     - `mScrollFit.installForTabs(mTabContainer, this)`, which wraps nothing because every page is unwrapped;
     - give the shell the controller (`mShell->setScrollFit(&mScrollFit)`).
   - Dance and Exposure in v2: `fillPage(view->getParent(), view, 30)`, where the parent is the legacy host.
2. **Refreshers.** Every page refresher (`refreshMoveTab`, `refreshPathTab`, `refreshActorStyleTab`, `refreshAnimateTab`, the Move part of `refreshGroupControls`, `refreshGazeSelection`, `refreshSubjects`, `refreshPrismDependents`, `refreshLauncherCards`) returns early while its page is unbound (null members or a `mBound[page]` flag).
   - Split `refreshGroupControls` into its chrome part (the assign combo) and its Move part.
   - `addLauncherCard` becomes `findChild`-based and skips missing cards.
3. **Cast selection.** The `mCastList` commit (already wired) also calls `if (mShell) mShell->setCastSelection(selectedCastIds());`.
4. **v2 overrides:** `hasAccelerators()`, the `handleKeyHere` table (section 3.12), and `handleMouseDown`, `handleRightMouseDown` and `handleScrollWheel`. The last three call `history.beginGesture(..., pageKeyAt(x,y))`, then the base.
5. **`onOpen` (v2):**
   - If `!DirectorV2RectBumped` and the rect is smaller than FLOATER_DEFAULT, `setShape` to 1160×720 clamped to the screen, and set the flag.
   - Restore via `selectLeafTab`, which builds that page lazily.
   - The search index starts loading on idle.
6. **`draw()` (v2):** an LLTimer EMA for `kit_ui_ms` when `KitShowUIMs`.

### 3.18 ALRigRimController and ALPanelRigRim

**`alrigrimcontroller.h/.cpp`.** It moves out of `alpanelcinelightrig.cpp`, unchanged in logic:
- `RigRimPresetRow`, `RIG_RIM_PRESETS[20]`, `applyRigRimPreset`;
- the members `mRigRimPreset`, `mRigRimQualityStatus`, `mRigRimEnableTag`, `mRigRimStatusGate`, `mRigRimStatusClock` and `mTronTintWidgets[7]`;
- `onRigRimPresetSelected`, the preset-combo population block, the enable-tag callback, the status block from `draw()`, and the Tron row visibility block.

```cpp
class ALRigRimController {
public:
    void attach(LLView* root);   // findChild for every rim widget; null = inert; wires callbacks (handle-safe)
    void onReveal();             // gate reset + clock forceDue (was in ALPanelCineLightRig::onVisibilityChange)
    void tickFrame();            // was in ALPanelCineLightRig::draw (status clock + gate)
    void tickRefresh();          // the Tron visibility block, called at the SAME place/cadence as before
    static bool applyPresetByName(const std::string& name);
    static const std::vector<std::string>& presetNames();
};
```

**ALPanelCineLightRig:**
- owns `ALRigRimController mRim;`;
- postBuild: `mRim.attach(this)`, which replaces the three rim `getChild` calls and the rim combo population;
- draw: `mRim.tickFrame()`;
- the Tron block calls `mRim.tickRefresh()`;
- onVisibilityChange: `mRim.onReveal()`;
- `applyRigRimPresetByName` forwards to `ALRigRimController::applyPresetByName`, keeping its existing callers (ALTron and the Lightbox).

The Tron visibility uses `ALKitRow::setControlVisible`, which falls back to `setVisibleIfChanged` outside the kit, so v1 is identical.

**Also in ALPanelCineLightRig postBuild:**
- register the history context `"cine_rig_slot"` → `"slot:" + std::to_string(manager.selectedSlot())`;
- register the page-scoped mode adapter `"cine_rig_easy"` with this page's logic-root handle; it calls guarded `applyEasyMode(desired, origin)` for click, setting signal, search reveal, undo, preset, scene, slot restore, and initial sync, then publishes the returned effective mode to the owning page;
- register the who adapter `"cine_rig_slot"`: a cast UUID maps to the anchor combo value:
  - null or `gAgentID` → 0;
  - Subject A–D → 1–4;
  - anything else → undefined;
- wrap `onAnchorSelected` in `ALKitHistory::ScopedSuspend`, then call `history.barrier("rig slot changed")` after the synchronous slot write.

These are inert in v1 because no shell exists.

**ALPanelRigRim** (`alpanelrigrim.h/.cpp`, `LLPanelInjector` "panel_rig_rim"):
- postBuild: `mRim.attach(this)`;
- `draw()`: a zone; return if `!isWidgetOnScreen(this)`; then `mRim.tickFrame()`, plus `tickRefresh()` on a 10 Hz `ALLiveReadoutClock`;
- onVisibilityChange: `onReveal`.

It registers the `rig_rim` preset provider once (`ALKitPresetRegistry`, idempotent).

---

## 4. The generator: `scripts/code_tools/director_kit_gen.py`

**Usage:** `python director_kit_gen.py [--data PATH] [--config PATH] [--out-dir PATH] [--check]`.
- Defaults: data `scripts/code_tools/director_kit/director_mockup_data.json`, config `scripts/code_tools/director_kit/kit_pages.json`, and out `indra/newview/skins/default/xui/en/`.
- `--check` regenerates in memory, exits 1 if any output differs, and prints the paths.

**Deterministic and idempotent:**
- stable ordering from the data;
- attributes sorted in a fixed order: name, then kit attributes alphabetically, then control attributes in v1 order;
- LF endings, UTF-8, 4-space indent;
- XML-escaped;
- a header comment `<!-- GENERATED by scripts/code_tools/director_kit_gen.py from director_mockup_data.json sha256=<16 hex> + kit_pages.json sha256=<16 hex>. Do not edit. -->`.

No timestamps.

### 4.1 Outputs

| File | Content |
|---|---|
| `floater_director_v2.xml` | the floater, `kit_shell`, rail items, `kit_cast_roster`, `director_tabs` with the group panels, page tab containers and `kit_lazy_page`s, `kit_transport`, popovers |
| `panel_dpage_<g>_<slug>.xml` ×36 | one per data page. `<g>` = group name without `_group`. `<slug>` = page name without `_tab` and without a leading `<g>_` (`cine_light_rig_tab` → `cine_light_rig`; `light_rim_tab` → `rim`). |
| `director_kit_index.xml` | the search index (section 3.8) |
| `scripts/code_tools/director_kit/gen_report.txt` | per page: classification, rows, kit rows, hosted v1 nodes, presentation upgrades, warnings |

### 4.2 Data → XUI mapping

**Page.**

| Data | XUI |
|---|---|
| `group.name` | `<panel name>` in `director_tabs`; its tab container is `<g>_tabs` (the v1 names) |
| `group.label`, `group.icon` | `<kit_shell.rail_item group label icon>`. Icon map: cast→Command_People_Icon, perf→Command_Poser_Icon, camera→Command_View_Icon, lens→Command_Search_Icon, light→Command_PersonalLighting_Icon, look→Command_Lightbox_Icon, world→Command_Environments_Icon, capture→Command_Snapshot_Icon |
| `page.name`, `page.label` | `<kit_lazy_page name label page_file>` |
| `page.sum` | `kit_page summary` |
| `page.master {w,l,s}` | kit pages: `master_widget/label/setting`. Every page with `s`: `kit_lazy_page master_setting` and the rail item `master_settings` (for the on-pips) |
| `page.who {label, pickers[0]}` | `who_label`, `who_picker`; `who_adapter` from the config |
| `page.modes` | `modes="<options joined by |>" mode_setting mode_widget mode_on(config) mode_adapter(config)`; one `<kit_page.mode name cards>` per option (card ids) |
| `page.sections[] {id,label,cards[idx]}` | `<kit_page.section id label cards="<ids>">` |
| `page.pre` / `presets` / `preset_user` | from the config: `preset_mode` (+ `kit_preset_host` or `preset_provider`) |

**Card.**

| Data | XUI |
|---|---|
| `id`, `t`, `k`, `a`, `new`, `help` | `kit_card id title kind advanced badge="NEW" help`; `name="kc_<id>"` |
| `en` + `ew` / `es` | `enable_widget` / `enable_setting`. When `ew` is missing, from config `card_enables` |
| `opts[] {label, en, ew, es, rows}` | `<kit_option label enable_widget enable_setting>`; enables from `ew` or config `option_enables` |
| `sel_label`, `sel_setting` | `sel_label`; `sel_setting` = data, or config `selector_settings[id]` |
| `to`, `summary[]` | `link_to`; `<kit_card.summary setting label kind decimals unit>` |
| `target`, `target_tab`, `target_widget`, `button_label` | launcher params |
| `card_presets` | none (derived from `cp` rows) |

**Row** (common).

| Data | XUI |
|---|---|
| `l`, `h`, `kw`, `unit`, `sub`, `a`, `added` | `label help keywords unit sub advanced added` |
| `w` | `widget` (and the control child's `name`) |
| `s` | `setting` |
| `mn`/`mx`/`dc` | `min_val max_val decimals` (metadata). For kit-native controls they also go on the control child (with `st` → `increment`) |
| `cp` | `card_preset="true"` |
| `who_picker` | `who_picker="true"` |
| `to` | `link_to` |
| `target`/`target_tab` (button rows) | `action="lightbox:<tab>"` or `"floater:<target>"` |
| `bind:"code"` | `bind="code"` |
| `d` (with `bind="code"`) | `default_value` |
| `range_assumed`, `sample`, `src` | report-only |

### 4.3 Page classification

1. **P0 page** (listed in config `p0_pages`): **kit** page built from data, with v1 hosting (section 4.5).
2. **The page exists in v1** (its name is a leaf panel in `floater_director.xml`): **legacy embed**, always in P0.
   - The generator still evaluates the "pure-kit" predicate below and reports it, for planning, but does not act on it.
   - The future rule (P1+) is: convert only if the v1 subtree has no `class=` panel, no `filename=` embed, and every named interactive v1 widget carries `control_name`.
3. **New page** (not in v1, not P0):
   - **pure kit** if every row is pure;
   - otherwise a **launcher page**: one launcher card built from config `launcher_targets[page]`. With no entry it is a generator error.

A row is **pure** when one of these holds:
- (`k` ∈ {slider, number, toggle, combo, color, dial, arc, vec2, vec3} and `s` present and no `bind:"code"`);
- or `k == "link"`;
- or (`k == "button"` and `target` present).

**Result for today's data** (the generator must reproduce it):

| Page | Classification |
|---|---|
| light_group/cine_light_rig_tab | kit (P0) |
| light_group/light_rim_tab | kit (P0) |
| look_group/bloom_glow_tab | pure kit (new) |
| look_group/tron_tab | launcher in the non-shippable P0 artifact only; WP10 replaces it with the full inline kit page |
| the other 32 pages | legacy |

This table describes only the non-shippable P0 foundation artifact. WP6 replaces this classifier/config with the 36-page matrix in `DIRECTOR_KIT_ROLLOUT_SPEC.md`; the final generator rejects every legacy or launcher page. In particular Tron becomes a full inline kit page, VCam uses its monitor design rather than the three-row v1 summary, and Capture/Gaze Cues/Light Cues/Diopter are data-complete.

**Legacy page file:**
```xml
<kit_page name="<page>_content" page="<g>_group/<page>" legacy="true" summary="…"
          watch_settings="<sorted unique s of that page's data rows>" width="<v1 leaf w>" height="<v1 leaf h>">
  <!-- verbatim children of the v1 leaf <panel name="<page>"> from floater_director.xml, in order -->
</kit_page>
```
The verbatim copy keeps every attribute and comment of those children. The v1 leaf panel element itself is not copied; its width and height move onto the kit_page. In the non-shippable P0 artifact only, VCam (`component`) uses v1 `vcam_tab`; rollout WP9 replaces it with the kit monitor page without editing v1 XML.

### 4.4 Kind mapping: data `k` + v1 tag → kit kind + control tag

**v1-hosted rows** (P0 pages, the widget found in a v1 source; section 4.5): **the v1 tag is kept**, except for the allowed presentation upgrades.

| v1 tag | default kit kind | Allowed upgrade (only if the name is **not** C++-referenced) |
|---|---|---|
| slider | slider | → dial or arc when data `k` ∈ {dial, arc}: the tag becomes `compass_dial mode=…`, keeping `control_name`, `min_val`, `max_val`, `increment` and `decimal_digits` |
| spinner | number | → slider when data `k == slider`; → dial or arc when data `k` ∈ {dial, arc} |
| check_box | toggle (the tag is **always** rewritten to `kit_switch`) | — |
| combo_box | combo | — |
| line_editor | text | — |
| text | readout | — |
| button | button | — |
| color_swatch | color | — |
| icon | gobo | — |
| scroll_list | list | — |
| view_border | viewport | — |
| anything else | custom | — |

**Kit-native rows** (no v1 node). Data `k` → tag: slider→`slider`, number→`spinner`, toggle→`kit_switch`, combo→`combo_box` (+ `<combo_box.item label value=index>` from `it`), color→`color_swatch`, readout→`text`, button→`button`, text→`line_editor`, list→`scroll_list`, dial→`compass_dial mode=compass`, arc→`compass_dial mode=arc`; vec2, vec3 and link have no child. Kit-native viewport, gobo or custom is a generator error in P0.

**Data custom rows** (`k=custom`) map by `ty`, as audit fix 1 asks:

| ty | kit kind | bind |
|---|---|---|
| check | toggle | code |
| slider | slider | code |
| spinner | number | code |
| combo | combo | code |
| button | button | — |
| line_editor | text | code |
| list | list | — |
| anything else | `custom` hosting the named widget | — |

**C++-referenced names.** This is the set of string literals in `indra/newview/*.cpp|*.h` that equal a widget name, plus the expansions in `scripts/code_tools/director_kit/cpp_name_patterns.json`. Codex creates that file. Its P0 content:
```json
{"patterns": [
  {"template": "cine_{role}_{part}", "role": ["key","fill","rim","bg"], "part": ["clip","shaft","hero","cone_width","actor_only","actor_only_reset"]},
  {"template": "cine_{role}_{part}", "role": ["key","fill","rim","bg"], "part": ["profile","gobo","gel","flicker"]},
  {"template": "card_{id}_{part}", "id": ["gaze_cues","diopter","practicals","actor_mask","light_cues","image","rotoink","tron","haze","shadows_probes","snapshot"], "part": ["open","summary"]}
]}
```

The extractor also recognizes `prefix + "suffix"`, indexed arrays, and simple loop expansions, records the requested template type `T` from every `getChild<T>`/`findChild<T>`, and refuses an upgrade unless the generated control is compatible with `T`. The real dynamic combo lookups at `alpanelcinelightrig.cpp:1064-1074` are a mandatory fixture. The runtime self-test emits resolved name/class pairs to catch patterns static extraction cannot prove.

### 4.5 v1 hosting rules (P0 pages)

- The v1 sources come from config (`v1_sources`). The generator parses them with ElementTree and indexes nodes by `name`. A duplicate name in the sources is a generator error.
- **Strip from the copied node:** `left`, `top`, `bottom`, `width`, `height`, `follows`, `layout`, `label_width`, `text_width`, `tab_group`. Also `label` for slider, spinner and check_box: the kit label replaces it, and it is kept for buttons, whose label is the caption.
- **Keep everything else verbatim:** `control_name`, ranges, `increment`, `decimal_digits`, `can_edit_text`, `show_text`, `allow_text_entry`, `max_chars`, `word_wrap`, `text_color`, `font`, `visible`, `enabled`, `tool_tip`, `value`, `initial_value`, and all child item nodes (`combo_box.item` keeps `name`, `label` and `value`).
- Slider rows force `show_text="true"` and `text_width="52"`.
- A word-wrapped text keeps its v1 height as the row's `row_height` = v1 height + 6.
- **Reset:** if a v1 node named `<w>_reset` exists, `reset_settings` = its `commit_callback.parameter`. Otherwise `reset_settings` = `s` when the row binds a setting. The kit creates `<w>_reset`; the v1 reset button node itself is never copied.
- **bind:** `setting` if the hosted node has `control_name`; `code` otherwise. When data `s` disagrees with the v1 `control_name`, report it and **trust v1**. Example: `cine_night_mask_darkness` has data `s` = CineLightRigNightMaskDarkness but no v1 `control_name`. It stays `code`, because a percent slider over a 0..1 setting would corrupt the setting.
- **Label names:** `<w>_label`, or config `label_names[w]`.
- **Card and option enables:** a hosted v1 enable checkbox (for example `cine_catchlight`) is **not** emitted as a row. The card's `enable_widget` makes the kit create the ALKitSwitch with the same name. The copied `tool_tip` goes to `enable_tooltip`.
- **Master and mode widgets:** the same. The kit_page creates `cine_enabled`, `cine_easy_mode`, `cine_rig_rim_enable` and `cine_rimq_wrap_enable` (as card enables) from params.
- **Preset host widgets** (`preset_mode=widgets`): copy the three v1 nodes (stripped) into `<kit_preset_host>`.

### 4.6 `floater_director_v2.xml` chrome

```xml
<floater name="floater_director" title="Director" width="1160" height="720" min_width="850" min_height="560"
         can_resize="true" save_rect="true" single_instance="true" legacy_header_height="18" layout="topleft"
         bg_opaque_color="KitWindowBg" bg_alpha_color="KitWindowBg">
  <kit_shell name="director_shell" follows="all" left="0" top="18" width="1160" height="702" index_file="director_kit_index.xml">
    <kit_shell.rail_item group="cast_group" label="Cast" icon="Command_People_Icon" master_settings=""/>
    … (8 items)
    <panel name="kit_cast_roster" background_visible="false">
      <!-- copied from v1 (stripped): cast_list (with its <scroll_list.columns>), cast_hint, group_assign_combo,
           btn_add_you, btn_add_animesh, btn_focus, btn_remove -->
    </panel>
    <tab_container name="director_tabs" hide_tabs="true" tab_position="left" follows="all">
      <panel name="cast_group" label="Cast"><tab_container name="cast_tabs" hide_tabs="true" tab_position="top" follows="all">
        <kit_lazy_page name="move_tab" label="Move" page_file="panel_dpage_cast_move.xml"/> …
      </tab_container></panel> …
    </tab_container>
    <kit_bar name="kit_transport" flex="status_strip" collapse_order="kit_ui_ms,take_timecode,btn_scene_delete,btn_reset_marks,rec_indicator_label">
      <!-- in order; widths in px: btn_action 112, btn_cut 112 (v1, XML-hidden), btn_arm 24, btn_take_play 28,
           btn_take_stop 28, take_timecode 90, scene_combo 130, scene_name_editor 130 (v1 XML-hidden),
           btn_scene_save 50, btn_scene_delete 56, btn_set_marks 28 (icon), btn_reset_marks 28 (icon),
           rec_indicator 50, show_paths_global_check 28 (kit_switch styled as icon toggle), btn_help 28 (icon),
           status_strip (flex), btn_cast_chip 70 (visible only in icon-rail mode), kit_ui_ms 70, btn_hotkeys 28 -->
    </kit_bar>
    <kit_popover name="kit_arm_popover" anchor="btn_arm" side="above" visible="false">  <!-- 260 wide -->
      <!-- kit rows (toggle) for arm_moves_check, arm_camera_check, arm_play_check, arm_capture_check and
           (number) delay_spinner, v1 nodes hosted -->
    </kit_popover>
    <kit_popover name="legend_panel" anchor="btn_help" side="above" visible="false"> legend_body, btn_legend_close (v1) </kit_popover>
    <kit_popover name="kit_hotkeys_popover" anchor="btn_hotkeys" side="above" visible="false"> <text name="kit_hotkeys_text" word_wrap="true" …/> </kit_popover>
  </kit_shell>
</floater>
```

**Transport and chrome notes:**
- `scene_combo` and `scene_name_editor` occupy one 130 px slot and are mutually exclusive; starting rename never adds width.
- `status_strip` is a custom single-line ellipsizing draw, like the help strip, not an undersized non-ellipsizing `LLTextBox`.
- Preserve the v1 C++-referenced `rec_indicator` name and class. Rehost that same control in a 50 px `kit_rec_cluster` containing its unchanged 22 px indicator plus a new, presentation-only 28 px `rec_indicator_label`. The bar collapses only `rec_indicator_label`; it never renames, replaces or changes the class of `rec_indicator`. At minimum width the cluster therefore measures 22 px and preserves the live record state without its label, while parity continues to find the original widget.
- Width proof (includes 10 px side padding and 8 px inter-child gaps):

  | Width/state | Fixed visible children | Gaps + padding | `status_strip` remainder |
  |---|---:|---:|---:|
  | 1160, rail labels, normal scene combo | 708 px | 132 px | 320 px |
  | 850, icon rail, rename editor, after collapse | 576 px | 116 px | 158 px |

  The 850 row includes ACTION, Arm, Play/Stop, the one scene slot, Save, Set Marks, compact REC, Show Paths, Help, Cast, Hotkeys, and status; Timecode, Delete, Reset Marks, UI-ms, and REC text are collapsed. CUT replaces ACTION in the same slot. The audit exercises idle/ACTION, CUT, recording, and rename states.
- `btn_take_play` and `btn_take_stop` call `LLFlycamRecorder::instance().togglePlayback()` (the same as F7) and the recorder's stop function. Codex finds the exact stop API.
- `take_timecode` shows the playback position at 2 Hz if the recorder exposes one. Otherwise it is generated `visible="false"` and noted in the report.
- Icon buttons use existing textures: marks `Flag`; reset to marks `Refresh_Off`; legend `Inv_Notecard` or the closest existing; hotkeys a text "?". Codex picks existing names from `textures.xml` and lists them in the report.
- The ACTION pill: idle is a KitAccent fill with dark text; CUT is a KitRecord fill with white text (`image_color` and `label_color` params).
- Every `*_check`/`*_combo`/button name is identical to v1. `LLFloaterDirector::bindChrome` does not change except for `findChild` tolerance on `btn_take_*`.

### 4.7 Config: `scripts/code_tools/director_kit/kit_pages.json` (P0; Codex writes it exactly)

```json
{
 "version": 1,
 "p0_pages": {
  "light_group/cine_light_rig_tab": {
   "v1_sources": ["panel_cine_light_rig.xml"],
   "root_class": "panel_cine_light_rig", "root_name": "cine_light_rig_tab_logic",
   "undo_context": "cine_rig_slot", "who_adapter": "cine_rig_slot", "mode_adapter": "cine_rig_easy", "mode_on": "Easy",
   "preset": {"mode": "widgets", "combo": "cine_setup_combo", "save": "cine_setup_save", "delete": "cine_setup_delete"},
   "selector_settings": {"lights": "DirectorV2SelectedLight"},
   "card_enables": {"easy_probe": "cine_easy_probe_enable", "light_group": "cine_group_enable",
                    "rig_catchlight": "cine_catchlight", "live_probe": "cine_live_probe_enable",
                    "night_mask": "cine_night_mask_enable"},
   "option_enables": {"lights": {"Key": "cine_key_on", "Fill": "cine_fill_on", "Back": "cine_rim_on", "BG": "cine_bg_on"}},
   "row_overrides": {"CineGobo01": {"w": "cine_gobo_preview", "k": "gobo"},
                     "cine_shadow_fixit": {"a": 0},
                     "cine_fixture_role": {"mirror_setting": "DirectorV2SelectedLight"}},
   "add_rows": [
    {"card": "light_group", "row": {"k": "readout", "w": "cine_group_status", "l": "Status", "h": "Which subjects this rig is lighting right now."}},
    {"card": "rig_aim", "after": "cine_auto_shadow_slots", "row": {"k": "readout", "w": "cine_shadow_hint", "l": "Shadows", "h": "Why a rig light has no shadow and what will fix it. Shown only when it matters."}},
    {"card": "lights", "option": "Key",  "first": true, "row": {"k": "readout", "w": "cine_key_clip",  "l": "Clipping", "h": "This light is past the headroom. Lower its EV or raise Headroom."}},
    {"card": "lights", "option": "Fill", "first": true, "row": {"k": "readout", "w": "cine_fill_clip", "l": "Clipping", "h": "This light is past the headroom. Lower its EV or raise Headroom."}},
    {"card": "lights", "option": "Back", "first": true, "row": {"k": "readout", "w": "cine_rim_clip",  "l": "Clipping", "h": "This light is past the headroom. Lower its EV or raise Headroom."}},
    {"card": "lights", "option": "BG",   "first": true, "row": {"k": "readout", "w": "cine_bg_clip",   "l": "Clipping", "h": "This light is past the headroom. Lower its EV or raise Headroom."}},
    {"card": "live_probe", "after": "cine_live_probe_live_status", "row": {"k": "readout", "w": "cine_live_probe_status", "l": "Details", "h": "What the live probe is doing and what it costs."}},
    {"card": "night_mask", "after": "cine_night_mask_target", "row": {"k": "readout", "w": "night_mask_status", "l": "Status", "h": "Who the night mask is centred on and whether it is active."}}
   ],
   "label_names": {}
  },
  "light_group/light_rim_tab": {
   "v1_sources": ["panel_cine_light_rig.xml"],
   "root_class": "panel_rig_rim", "root_name": "light_rim_tab_logic",
   "preset": {"mode": "provider", "provider": "rig_rim"},
   "selector_settings": {"per_light_rim": "DirectorV2SelectedLight"},
   "card_enables": {"rim_wrap": "cine_rimq_wrap_enable"},
   "add_rows": [
    {"card": "rim_quality", "after": "cine_rimq_enable_tag", "row": {"k": "readout", "w": "cine_rimq_status", "l": "Status", "h": "Whether rim quality features are running, and what is missing."}},
    {"card": "rim_quality", "row": {"k": "toggle", "w": "cine_rimq_actors_alpha", "l": "Alpha actors", "a": 1, "hidden": 1, "h": "Reserved for a later version."}}
   ],
   "label_names": {"cine_rig_rim_tron_mode": "cine_rig_rim_tron_label",
                   "cine_rig_rim_tron_mix": "cine_rig_rim_tron_mix_label",
                   "cine_rig_rim_tron_color": "cine_rig_rim_tron_color_label"}
  }
 },
 "launcher_targets": {"look_group/tron_tab": {"target": "lightbox", "target_tab": "tron_settings", "button_label": "Open Tron in Lightbox ›", "help": "Tron is edited in the Lightbox until its page is converted."}},
 "parity_allow": {"cine_rig_rim_preset": "replaced by the kit preset strip (provider rig_rim)",
                  "cine_light_rig_flow_grid": "v1 layout container",
                  "cine_*_card": "v1 card containers (glob)",
                  "cine_*_label": "v1 decoration labels not referenced by C++ (glob; the referenced ones must exist)"}
}
```

If a `row_overrides` key or an `after` row is not found in the data, it is a generator error (no silent skip).

---

## 5. P0 pages: Light › Rig and Light › Rim

### 5.1 Light › Rig (`panel_dpage_light_cine_light_rig.xml`)

**Root:** `<panel class="panel_cine_light_rig" name="cine_light_rig_tab_logic" follows="all">`, containing `<kit_page name="cine_light_rig_kit" page="light_group/cine_light_rig_tab" …>`.

**Page:**
- master `cine_enabled` (CineLightRigEnabled, "Light rig");
- modes Easy|Advanced on `CineLightRigEasyMode` via `cine_easy_mode`, `mode_on="Easy"`;
- who "Lit subject" via `cine_anchor`, adapter `cine_rig_slot`;
- preset widgets `cine_setup_combo`, `cine_setup_save` and `cine_setup_delete`;
- undo context `cine_rig_slot`;
- 17 cards, 5 sections (Easy, Setup, Lights, Gear & flare, Probe & night).

**Modes** (from data):
- **Easy:** easy, easy_probe, light_map, lights. That is 4 cards, so the chips run in anchor mode.
- **Advanced:** the 15 data cards, so the chips run in filter mode.

**Cards, from data in data order, with these additions:**

| Card | Kit treatment |
|---|---|
| easy (combined, Easy look) | Subs "Easy look", "Beam and flare", "Easy night". `cine_easy_flare_preset` sits on the "Beam and flare" sub-heading and `cine_easy_night_mask_preset` on "Easy night". `cine_easy_cone_width` stays a slider (C++-referenced). The Easy rows are `bind=code` with the v1 `reset_settings`. |
| easy_probe | enable `cine_easy_probe_enable`, 5 rows |
| rig_status | `cine_anchor` is a who_picker (hidden). Rows Gizmo, Target object, Clear, Target. |
| light_group (Who's lit) | enable `cine_group_enable` (code). Five `kit_switch` code rows. Plus the `cine_group_status` readout. |
| rig_main | the `cine_radius` label is `cine_radius_label`. Reset all is a button. |
| rig_setups, rig_effects | as data. `cine_seed` stays a line_editor (text kind). |
| light_map (rigmap, NEW) | `<light_rig_map name="cine_light_map" selector_card="kc_lights" view_setting="DirectorV2LightMapView"/>` |
| lights (selector, `sel_setting` DirectorV2SelectedLight) | Options Key, Fill, Back, BG with enables `cine_{key,fill,rim,bg}_on`. The first row is the clip readout. Yaw → `compass_dial mode=compass`; Pitch → `compass_dial mode=arc`; they form a 2-up aim group. Shaft and Hero are code toggles; Cone is a code slider. |
| rig_aim | Mirror, the aim nudge buttons, Shadow policy, Auto shadows, the `cine_shadow_hint` readout, and `cine_shadow_fixit` (not advanced; C++ shows and hides it) |
| rig_bounce | as data |
| rig_catchlight | enable `cine_catchlight`. Angle → dial (0..360, compass, no wrap problems). |
| fixture | `cine_fixture_preset` in the header (cp). `cine_fixture_role` has `mirror_setting=DirectorV2SelectedLight`: the role follows the Lights selection, and choosing a role moves the selection. The adapter performs `cine_fixture_role->setValue(index); cine_fixture_role->onCommit()` inside `ALKitHistory::ScopedSuspend`, because `onFixtureRoleCommit` repopulates Kelvin/gels (`alpanelcinelightrig.cpp:925-926`); setting the value alone is forbidden. |
| cookie | `cine_gobo_preview` gobo row; Library combo; `cine_gobo_softness` readout; Cue stack button; `cine_cookie` text (advanced) |
| flare (combined) | `cine_flare_preset` in the header; subs Lens flare, Starburst, Flare optics, Flare colour |
| live_probe | enable `cine_live_probe_enable`; `cine_live_probe_status` added |
| night_mask | enable `cine_night_mask_enable`; `cine_night_mask_preset` in the header; `night_mask_status` added; `cine_night_mask_darkness` is code (percent) |

**Wiring.** ALPanelCineLightRig is unchanged except for the rim extraction and the section 3.18 registrations.
- Every name in its `getChild` list must exist on this page. The parity gate checks the names below, with the rim names moved to Rim:
  - `cine_anchor`, `cine_target_object`, `cine_clear_object`, `cine_object_target_status`;
  - `cine_group_enable`, `cine_group_status`, `cine_group_{self,a,b,c,d}`;
  - `cine_setup_combo`, `cine_setup_save`, `cine_setup_delete`;
  - `cine_flare_preset`, `cine_easy_flare_preset`, `cine_fx_combo`, `cine_seed`, `cine_fill_ev`;
  - `cine_easy_*` (brightness, drama, rim, bg, warmth, cone_width, cone_feather, mode, probe_enable, probe_gizmo, probe_radius, night_mask_enable, night_mask_preset, night_mask_darkness, shaft_length);
  - `cine_fixture_*` (role, mode, preset, kelvin, source_size, gel_0..2);
  - `cine_gobo_library`, `cine_gobo_preview`, `cine_gobo_softness`;
  - `cine_live_probe_status`, `cine_live_probe_live_status`, `cine_live_probe_refresh_now`, `cine_live_probe_manual_faces`, `cine_live_probe_manual_frames`, plus their `_reset`;
  - `cine_night_mask_preset`, `cine_night_mask_darkness`, `night_mask_status`;
  - `cine_master_ev`, `cine_{key,fill,rim,bg}_ev`, `cine_ratio_lock`, `cine_ratio`, plus their `_reset`;
  - `cine_shadow_hint`, `cine_shadow_fixit`, `cine_radius_label`, `cine_fx_stop`;
  - `cine_aim_{yaw,pitch}_{minus,plus}`, `cine_aim_reset`, `cine_reset_all`, `cine_seed_randomize`, `cine_open_cues`, `cine_manual`;
  - `cine_{role}_{clip,shaft,hero,cone_width,actor_only}` plus `_actor_only_reset`;
  - `cine_bounce_lights_set`, `cine_catchlight_actor_only`, `cine_actor_only_all`, plus `_reset`;
  - `cine_easy_*_reset` (8).
- The Easy and Advanced gating in C++ (`mEasyGatedControls`, `mAdvancedDrivenControls`) keeps working through `setEnabled` on the same widgets.

### 5.2 Light › Rim (`panel_dpage_light_rim.xml`)

**Root:** `<panel class="panel_rig_rim" name="light_rim_tab_logic" follows="all">`, containing `<kit_page name="rim_kit" page="light_group/light_rim_tab" master_widget="cine_rig_rim_enable" master_setting="CineRigRimEnabled" master_label="Rim light" preset_mode="provider" preset_provider="rig_rim">`.

**Cards:** rig_rim, rim_quality, per_light_rim, rim_edge, rim_wrap, rim_tron and rim_diagnostics (advanced, collapsible). Sections: Rim, Style, Advanced (7 cards in filter mode).
- `per_light_rim` is a selector on DirectorV2SelectedLight, with options Key, Fill, Back and BG and no enables. Its rows are numbers (as v1).
- `rim_wrap` has enable `cine_rimq_wrap_enable`.
- `rim_quality` gains the `cine_rimq_status` readout and the hidden `cine_rimq_actors_alpha` row.
- `rim_tron` uses label names `cine_rig_rim_tron_label`, `…_mix_label` and `…_color_label`. Reset `cine_rig_rim_tron_mix_reset` is automatic.

**Wiring.** ALPanelRigRim with ALRigRimController finds `cine_rimq_status`, `cine_rimq_enable_tag` and the 7 Tron names. `cine_rig_rim_preset` is absent: the preset strip replaces it (parity allowlist), and the controller is inert for that member.

---

## 6. Tooling and gates

### 6.1 `scripts/code_tools/kit_lint.py`

**Usage:** `python kit_lint.py [--all] [--json]`. It runs over `floater_director_v2.xml` and `panel_dpage_*.xml`. Exit 1 on any error.

**Errors:**
1. A kit tag (`kit_*`, `light_rig_map`) has `left`, `top`, `bottom`, `width`, `height`, `follows` or `layout`. Exceptions: the floater root, the `kit_shell` root, a legacy `kit_page` (width and height only), and direct children of `kit_bar` (width and height only).
2. A `kit_row` control child has any of the stripped attributes from section 4.5, or `kit_row` has the wrong number of control children for its kind.
3. The kind ↔ control tag pair is not in the section 3.3 table.
4. A widget name is not unique across the whole v2 tree (floater plus all page files). Duplicates already present inside v1 leaf pages are listed in `scripts/code_tools/director_kit/kit_lint_allow.json`; Codex generates that file once from v1, and it may only shrink.
5. A `setting`, `enable_setting`, `master_setting`, `mode_setting`, `sel_setting` or `control_name` is missing from the viewer's complete settings graph, or its type does not fit the kind. The lint loads every Global/PerAccount base in `settings_files.xml` and recursively follows each file's `Include` array; `settings.xml:6327-6330` includes `settings_alchemy.xml`, where the flare/Bloom settings live. Runtime bindings resolve through the same setting-group lookup as `LLView::findControl` (`llview.cpp:2405-2427`), not `gSavedSettings` alone:
   - slider and number: F32, S32 or U32;
   - toggle: Boolean;
   - combo: S32, U32 or String;
   - color: Color3 or Color4;
   - text: String;
   - vec2/vec3: Vector3 or Vector3D. There is no Vector2 control type (`llcontrol.h:51-66`); vec2 preserves z unchanged (for example `RenderVignetteCenter`, `settings_alchemy.xml:3435-3447`).
6. A section or mode references an unknown card id; a card is in no section.
7. A generated file's header sha does not match the current inputs (stale generation).
8. `bottom=` anywhere.

**Additional errors:**
- a label whose measured configured-font width exceeds `LABEL_W`, unless it declares the two-line label form and measured row height. Length-based warnings are not a substitute;

**Warnings:**
- help text matching `Lightbox|Phototools|\btab\b|type \d|Phase \d|scaffold|deferred|bucket|allocation` (audit §14);
- an empty help.

### 6.2 `scripts/code_tools/director_parity.py`

**Usage:** `python director_parity.py --groups light_group [--json]`.

**A** = the v1 names and control_names under each `light_group` leaf of `floater_director.xml`, with `filename=` embeds expanded recursively.
- Names count only for these tags: slider, spinner, check_box, combo_box, button, line_editor, color_swatch, icon, radio_group, scroll_list, text (named only), compass_dial.
- Names ending in `_reset` count only if they are C++-referenced (section 4.4).

**B** = the same over `floater_director_v2.xml` plus the light page files (embeds expanded), **plus the names the kit creates at runtime**: the generator writes them into `gen_report.txt` as `KITNAMES <page>: …` (`<w>_label`, `<w>_reset`, `<w>_value`, `<w>_unit`, `<w>_state`, enable, master and mode switches).

**Checks:**
1. Every name in A is in B, or matches a `parity_allow` entry (glob) with its reason.
2. Every control_name in A is bound by some control_name in B.
3. **getChild check:** every literal and expanded prefix/suffix/index pattern passed to `getChild<T>`/`findChild<T>` in the page owners exists in B **and the v2 class is compatible with T**. The extractor covers the dynamic role combos at `alpanelcinelightrig.cpp:1064-1074`; a runtime self-test log of resolved name/requested-type/actual-type closes patterns static extraction cannot prove. The generator refuses a presentation upgrade matching any extracted literal or suffix pattern.

It prints PASS or FAIL with the lists, and exits 1 on FAIL. Intermediate P0 gate: PASS for `light_group`. All-at-once gate: `director_parity.py --groups all` over every recursively expanded v1 source and all 36 kit pages.

### 6.3 `xui_layout_check.py` learns the kit

- Skip `director_kit_index.xml`.
- For `kit_page` files:
  - **Kit pages:** check only the hosted control children for disallowed geometry (as kit_lint rule 2); everything else is runtime-laid-out.
  - **Legacy pages:** check them like the v1 leaf. The root size is the kit_page `width`/`height`. The checker must report **exactly the same findings** as it does for that leaf inside `floater_director.xml` today: a regression-fixture pair in `--selftest`.
- `floater_director_v2.xml`: skip the inside of `kit_shell` except the copied v1 widgets, which get the attribute checks.
- Add `--selftest` fixtures: a kit row with a stray `width=` (must fail), and a clean kit page (must pass).

### 6.4 Runtime layout audit and self-test (`aluiperfselftest.cpp`, `scripts/perf/ui_selftest.ps1`)

1. **Audit sizes.**
   - The new setting `UILayoutAuditSizes` holds comma tokens: `default` (the original rect), `min`, or `WxH`. When empty it is `default,min`, unchanged behaviour.
   - The loop over `mAuditSizeIndex < 2` becomes a loop over the parsed vector. `auditSizeName()` maps the input token `min` to the existing output name `minimum` (`aluiperfselftest.cpp:1320-1323`) so default-run CSV keys/directories and `-Compare` baselines do not change; explicit `WxH` tokens keep their token.
   - A `WxH` below the floater minimum is skipped with an INFO log.
2. **Lazy rescan.**
   - In `TAB_SELECT`, after `applyChain`, if the target panel is an ALKitLazyPage that was unbuilt when tabs were collected:
     - call `collectScrollers(panel, chain, path)` and append the result, fixing `mScrollBegin`/`mScrollEnd`;
     - call `collectTabs(panel, chain, path)` and **insert** any nested TabTargets right after the current index.
   - Record the select action ms as that page's **first reveal**.
3. **State enumeration before geometry.** For a kit page the audit iterates every mode, chip/filter or anchor target, selector option, advanced-card open/closed, More open/closed, master state, registered custom-widget state, validation/error state, popover, and inline rename state. It applies each through the kit API, forces layout, audits, and records the state key in CSV/screenshot names. VCam adds 0/1/4/5/8 captures and every raw/display selection. A single default-visible visit is not coverage.
4. **Kit measures** (when the target is the director and an ALKitShell exists), added as CSV rows:
   - `kit_search`: run the fixed queries `key yaw`, `rim`, `nd`, `ev`, `exposure`, `actors only`, `froxel`, `zzz`, and record the max query ms;
   - `kit_amber_storm`: `ALKitSettingWatch::debugRecomputeAll()` ms;
   - `kit_index`: the index tokenize total ms;
   - `kit_build` for every page: its build ms (from ALKitLazyPage).
5. **`ui_selftest.ps1 -DirectorV2`:**
   - sets `--set DirectorUIV2 TRUE --set UILayoutAuditSizes "1160x720,min,1141x720,915x720"`, `Floaters=director`, and implies `-Layout`;
   - after the run, evaluates and prints `DIRECTOR V2 BUDGET: PASS|FAIL` with each line:

| Metric | Budget (hard) | Target |
|---|---|---|
| director open action ms | ≤ 120 | — |
| worst steady page mean | ≤ 0.5 ms | 0.4 |
| any kit page first reveal | ≤ 50 ms | 30 |
| kit_search max | ≤ 5 ms | 2 |
| kit_amber_storm | ≤ 2 ms | — |
| layout findings, shell + Light › Rig + Light › Rim, all states at all 4 sizes | 0 | — |
| layout findings, legacy pages, vs a v1 baseline run at default and min (`-Compare <v1 csv>`) | 0 new | — |

The legacy comparison is intermediate-only. The WP12 final run defined in `DIRECTOR_KIT_ROLLOUT_SPEC.md` replaces it with absolute zero findings for all 36 pages and every enumerated state at all four sizes.

6. **Help and amber per-frame cost.** The self-test's existing `mFindCalls` per frame on the Rig page must equal 0 while hovering rows. Add a `hover_sweep` row for the Director Rig page, reusing the inventory hover-sweep code path.

### 6.5 `ui_draw_lint.py`

New kit files must report 0 new findings (`--changed <base>`). Any suppression needs a reason; the ALKitCard visibility probe needs none (bool reads only).

### 6.6 TUT tests: `indra/newview/tests/alkitmath_test.cpp` (registered in `newview/CMakeLists.txt`)

Cases:
- `wrap180`;
- `arcAngle`/`arcValue` round-trip for (-85,85), (-180,180), and (0,360), including horizon 0/right, +up, -down, and in-bounds endpoints;
- compass wrapping preserves `[min,max)` for both -180..180 and 0..360; `kit_geometry=false` reproduces legacy radius/letter positions;
- `projectLight` (t = 0 in 2.5D lands below the centre and is front; t = 180 is behind; Side from camera-left puts the camera on the right and t = −45 (camera-left) nearer than t = +45; Side from camera-right mirrors both);
- `frontFactor` in Side: t = +45 and t = −45 never share a layer; `yawSideText(45)` = "R45°", `(−135)` = "L135°", `(0)` = "0°";
- `pDisp`/`baseFromDisplayPitch` round-trip with non-zero OrbitPitch and OffsetZ at plain and subject-scaled radii, with the same captured radius supplied to display and inverse;
- for Top, 2.5D and Side, pointer equal to mouse-down produces bit-identical base settings with Mirror off/on for avatars and rotated object targets, rig live/lagging and rig off; pressing without moving never writes a setting;
- `dragSideAbsolute` keeps the light's side and exactly inverts Side projection with non-zero orbit/OffsetZ; `unproject25D` round-trips both front and back branches and preserves its branch at the silhouette;
- feed 1,000 synthetic hover events at one fixed final pointer while a fake live display continues easing for 0.9 s: every event produces the same value as the first, the final target stays within 0.01° of the one-shot result, and there is zero accumulated drift in Top, 2.5D and Side;
- feed 1,000 out-and-back pointer samples ending at `(x0,y0)`: all three views return to the captured base values within 0.01°; repeat at scaled/unscaled radii, non-zero orbit/OffsetZ, both Side directions and both 2.5D depth branches. Include a Top path whose pointer travel completes a full turn: `wrap180(a−a0)` may jump by 360° after 180° because yaw is periodic, but the final value must still return within 0.01°;
- `frontFactor` monotone;
- `effYaw` with and without mirror (against the `computeLive` formula);
- `dragTopAbsolute` Mirror/unmirrored sign and wrap-boundary behavior;
- `computeShellLayout`: the four widths of section 3.7 (rail, help mode, columns, track ≥ 120), including hysteresis (1141 after 1160 gives a strip; 1145 after 850 gives a strip; 1150 after 850 gives the panel);
- `packRows` with a hidden and a spanning item.

### 6.7 Screenshot review (the Opus reviewer, after the build)

1. Run `ui_selftest.ps1 -DirectorV2`. The audit captures `logs/ui_audit/director/audit-<size>/light_group/{cine_light_rig_tab,light_rim_tab}*.png`.
2. Claude renders the approved mockup (Director.dc.html) at 1160×720 on Light › Rig (Easy and Advanced) and Light › Rim, and saves the screenshots to `doc/director_kit/p0_review/mockup_*.png`.
3. Opus checks the following, side by side, and writes `doc/director_kit/p0_review/REVIEW.md` with pass/fail per item:
   - row 28, label 100, value 52, reset 20, gap 6;
   - card header 36, 2 columns at gap 12, padding;
   - colours (the section 3.1 table);
   - switch shape, dial and arc geometry, map layering (front solid with glow, behind hollow and dashed, off grey and slashed);
   - at the 48×48 minimum, compare the intentionally left-weighted arc (x=6..26) and its separate value-column text against the mockup; unused right-half dial space is not treated as a centring defect;
   - chips, who row, bar order (preset · Save as · delete · reset · undo · redo · master · search);
   - transport order;
   - the help panel and strip;
   - no truncated labels.

**Orchestrator-only gate** (outside the repo): add these to `C:\Users\xianw\cxs\prebuild_guard.ps1`:
- `director_kit_gen.py --check`;
- `kit_lint.py --all`;
- `director_parity.py --groups light_group`;
- `xui_layout_check.py --selftest`.

---

## 7. Work packages

The order is WP0 → WP1 → WP2 → WP3 → WP4 → WP5, run sequentially in one tree.
- WP3 depends only on the tag contract (sections 3 and 4), so it may run before WP2 if useful.
- Opus reviews each WP's diff. Fixes loop until there are 0 must-fix.
- There is **one fresh build after WP5**, then the gates in section 6.

### WP0: data snapshot (orchestrator, not Codex)

1. Patch `scripts/code_tools/director_mockup_data.py`:
   - delete the `_strip_enable_keys(groups)` call;
   - in `apply_ia` merges, copy `ew`/`es` into each option (`opts.append({... "ew": c.get("ew"), "es": c.get("es")})`, omitting None);
   - in `_conv_inline_card`, set `o["ew"] = en` whenever it sets `en`.
2. Run it with `--out C:\Users\xianw\cxs\director_mockup\director_mockup_data.json`, then copy the result to `scripts/code_tools/director_kit/director_mockup_data.json`.
3. Verify with a script that removing the keys `ew`, `es` and option `ew`/`es` from the new JSON reproduces the old JSON exactly. The mockup ignores unknown keys, so the approved look is untouched.
4. Commit. The PY patch goes through the WP3 Opus review.

**Acceptance:**
- the equality check passes;
- `lights.opts[*].ew` = `cine_{key,fill,rim,bg}_on`;
- `rig_catchlight.ew` = `cine_catchlight`;
- `rim_wrap.ew` = `cine_rimq_wrap_enable`.

If they differ, the config tables in section 4.7 win and the generator reports the mismatch.

### WP1: kit core widgets, arc dial, light map, maths

**Files (new):**
- `alkitstyle.*`, `alkitswitch.*`, `alkitrow.*`, `alkitcard.*` (ALKitCard plus ALKitOption);
- `alkitpage.*` (ALKitPage, ALKitScroller, ALKitDoc, ALKitPresetHost, the page bar, chips, who row);
- `alkitlazypage.*`, `alkitbinding.*` (bindings, registry, ALKitSettingWatch, ALKitWhoAdapters);
- interface headers `alkithistory.h`, `alkitsearch.h` (including `Hits`), `alkithelp.h`, `alkitpresets.h`, and the shell accessor contract, plus inert stub `.cpp` implementations sufficient for ALKitPage/Row to link. WP2 replaces the stubs without changing these interfaces;
- `allightrigmap.*`, `alkitmath.h`;
- `scripts/code_tools/director_kit/kit_png_gen.py` and its PNGs;
- `skins/default/xui/en/widgets/kit_switch.xml` (structural defaults copied from `check_box.xml`);
- the `colors.xml` Kit* entries and `textures.xml` entries.

**Files (modified):**
- `indra/llui/alcompassdial.*` (section 3.13);
- `alflowgrid.*` (`pack=rows`, ALFlowGridItem);
- `alscrollfit.*` (SelfSizedDocument, adoptPage);
- `alcinelightrig.h/.cpp` (read-only `mapTargetFrame()`/revision accessor plus the object branch's approximately six-line live-facing publication from `root->getRenderRotation()`; no other engine behavior change);
- `indra/newview/CMakeLists.txt`;
- `newview/tests` (`alkitmath_test.cpp`).

**Spikes first:** S1, S2, S3, S6 and S9. Results go into `doc/director_kit/SPIKES.md` with the decision taken, using the fallback in section 8.

**Acceptance (the review checklist):**
- every class, tag and param is exactly as in section 3;
- no `draw()` violations; Tracy zones;
- existing dial users compile unchanged and behave the same, apart from the wheel guard;
- `ALFlowGrid` masonry behaviour is byte-for-byte unchanged in code paths;
- the `fitContainer` early-return is the only change on the v1 path;
- the TUT test compiles in isolation (it includes only `alkitmath.h` and `linden_common.h`).

**Do not touch:**
- llfloaterdirector, alpanelcinelightrig, any XML except `colors.xml` and `textures.xml`;
- the self-test.

### WP2: shell, search, presets, undo, help, transport, floater integration, keyboard

**Files (new):**
- `alkitshell.*` (shell, rail, roster, tab strip, mode proxy, layout, popovers via `alkitpopover.*`, ALKitBar via `alkitbar.*`);
- the real implementations of the WP1 interface stubs: `alkitsearch.cpp`, `alkithelp.cpp`, `alkitpresets.cpp` (registry, SnapshotStore, preset strip), and `alkithistory.cpp`;
- `alrigrimcontroller.*`, `alpanelrigrim.*`.

**Files (modified):**
- `llfloaterdirector.h/.cpp` (section 3.17, all items);
- `alpanelcinelightrig.h/.cpp` (section 3.18 extraction and registrations only);
- `llviewerfloaterreg.cpp` (one line);
- `llviewerwindow.cpp` (one line: `ALKitHistory::noteKey();` at the top of `handleKey`, after the existing early-out guards, before menus);
- `settings.xml` (section 3.16 rows);
- `CMakeLists.txt`.

**Acceptance:**
- v1 postBuild call order is identical (the reviewer diffs it);
- every v1 refresher is null-safe;
- ALPanelCineLightRig v1 behaviour is identical: same `getChild` set minus the 3 rim names, now `findChild` in the controller; the same per-frame calls;
- `applyRigRimPresetByName` is still public and static;
- `hasAccelerators` returns false in v1;
- the history recording rules match section 3.11;
- search query complexity is O(entries × tokens) with interned ids.

**Do not touch:**
- the v1 XML;
- `alcinelightrig*` engine logic (WP1's accessor and object-facing correction are the complete allowed engine delta);
- ALDirectorHotkeys.

### WP3: generator, v2 floater, page files, index, config

**Files (new):**
- `scripts/code_tools/director_kit_gen.py`;
- `scripts/code_tools/director_kit/kit_pages.json` (section 4.7), `cpp_name_patterns.json`, `kit_lint_allow.json`;
- generated: `floater_director_v2.xml`, 36 `panel_dpage_*.xml`, `director_kit_index.xml`, `gen_report.txt`.

**Files (modified):** `scripts/code_tools/director_mockup_data.py` (the WP0 patch, if not already done).

**Acceptance:**
- running twice gives byte-identical output, and `--check` exits 0;
- the classification table in section 4.3 is reproduced;
- every v1 leaf panel's children are copied verbatim (a test inside the generator: re-serialise both and compare canonical forms);
- `gen_report.txt` lists every presentation upgrade, every data/v1 `control_name` disagreement (with v1 trusted) and every `KITNAMES` line.

**Do not touch:** any C++.

### WP4: Rig and Rim pages end-to-end

This is the integration pass over WP1 to WP3 for the two pages. It covers:
- the light map ↔ selector ↔ fixture-role ↔ Rim selector link through DirectorV2SelectedLight (including `mirror_setting` in ALKitRow);
- the who adapter;
- the history context;
- preset providers (`rig_rim`) registered;
- card-header presets reparented;
- the dyn-visibility probe working for clip, shadow hint, gobo preview and enable tag;
- the mode proxy driving `cine_easy_mode` explicitly (user) and passively (search).

**Files:** the kit C++, generator config and regenerated files as needed. **No new architecture**; changes outside the two pages' needs are out of scope.

**Acceptance:**
- `director_parity.py --groups light_group` passes;
- `kit_lint --all` reports 0 errors;
- a written trace (in the hand-off) for each of these flows, with the code paths:
  - drag Key in the map → dial value;
  - toggle Easy → cards;
  - Rim preset apply → one undo step → Ctrl+Z restores;
  - card reset on the Lights selector resets the active option only;
  - page reset asks first and is one undo step;
  - rail selects Mara (Subject B) → anchor slot 2 → undo is not polluted.

### WP5: gates and self-test hooks

**Files:**
- `aluiperfselftest.cpp/.h` (section 6.4);
- `scripts/perf/ui_selftest.ps1` (`-DirectorV2`, budget evaluation);
- `scripts/code_tools/kit_lint.py`, `director_parity.py`;
- `xui_layout_check.py` (section 6.3, plus selftest fixtures);
- `ui_draw_lint` baseline untouched (it must not grow).

**Acceptance:**
- `xui_layout_check.py --selftest` and `ui_draw_lint.py --selftest` pass;
- `kit_lint`, `parity` and `gen --check` pass;
- the default-mode self-test behaviour is unchanged (no `UILayoutAuditSizes`, no DirectorV2): reviewer diff;
- the budget evaluation prints every line in section 6.4.

**After WP5:** review and automated foundation build may run, but v2 is not shippable and remains debug/self-test-only. Proceed directly through `DIRECTOR_KIT_ROLLOUT_SPEC.md` WP6-WP12. The single release-candidate fresh build and full 36-page in-world checklist occur only after WP12. The P0-only diagnostic checklist is:
- Rig Easy/Advanced;
- map drag in Top, 2.5D and both Side directions with Mirror on/off, zero-motion mouse-down, a 0.9 s transition, subject scaling, and an object target rotated away from world east (Mirror enabled and object-front-relative);
- preset apply, undo and redo;
- Rim presets;
- narrow sizes;
- a legacy page spot check (Move, Gaze, Atmosphere).

---

## 8. Spikes (at the start of WP1; record outcomes in `doc/director_kit/SPIKES.md`)

| # | Question | Pass | Fallback |
|---|---|---|---|
| S1 | Can an LLSliderCtrl with `text_width=52` and no label put its value exactly in the 52 px value column? Can a unit suffix render without breaking edit parsing? | Value right-aligned in mono; editing shows the bare number | Slider rows show the unit in the help meta only (labels stay unchanged). No LLSliderCtrl changes in P0. |
| S2 | ALKitSwitch subclass parse and flow: do all v1 `check_box` attributes parse with `widgets/kit_switch.xml`; does `getChild<LLCheckBoxCtrl>` succeed; do click, external `LLControlVariable::set()`, direct `setValue()`, `setValueIfChanged()`, undo and passive `writeBoundValue` each update `isOn()`/state text/amber/dimming and fire exactly one edge; does the Rig Easy adapter publish the effective card set after refusal? | All checks | Fix the wrapper/inner-button hooks and adapter publication; draw dimming as a KitWindowBg overlay only if alpha inheritance fails |
| S3 | Lazy `createFromFile<LLPanel>` into a live tab panel: do `postBuild` order, parent chain during postBuild, ALScrollFit `adoptPage`, the self-test `selectTab` safety net, and `onVisibilityChange` re-entrancy all work? What is the Rig build time? | Rig built ≤ 50 ms; no re-entrancy warnings | Build the selector's inactive options on first selection. Not allowed if a C++ `getChild` would miss them, so instead prebuild the Rig on the idle after open. |
| S4 | Help hover and focus: zero `findChild` per frame while hovering? Do focus-received signals fire for sliders, spinners and combos? | 0 finds; signals fire | Focus help on click only |
| S5 | Search index tokenize time and query time on the real index | ≤ 30 ms total in chunks; ≤ 5 ms per query | Drop help tokens for 1–2 letter queries (already the rule); cap results earlier |
| S6 | `IBMPlexMono` lookup by descriptor at sizes Small and Medium; UI scale 1.25 metrics | Font resolves; rows stay 28 × scale | `getFontMonospace()` |
| S7 | Run the v1 runtime audit at 1160×720 and 850×560 and classify the findings | A baseline CSV saved for the legacy "0 new" gate | — |
| S8 | Accelerator precedence: Ctrl+Z/Y invokes kit history in non-text controls; Ctrl+Z/Y in `LLLineEditor`, `LLTextEditor`, and spinner editors never reaches kit history; `/` reaches `handleUnicodeCharHere` and focuses search only outside text entry; Ctrl+F remains viewer search; Enter in search does not fire ACTION; 1–9 punches are suppressed while search is focused | All | Undo/Redo by buttons only; document it |
| S9 | Amber precision and storm: a slot switch fires about 150 signals. Total handler time? After `setControlName` rebind, is the old scoped connection disconnected and exactly one new connection live? | ≤ 2 ms; no stale amber; old control cannot fire | Coalesce amber flips into one idle pass |
| S10 | UI ms source: is a floater-draw LLTimer EMA stable enough vs Tracy? | ±0.05 ms of the Tracy zone mean | Hide the chip (it is opt-in anyway) |

---

## 9. Risks (with the mitigation built in)

1. **Lazy pages vs `getChild`.** Any `getChild` on an unbuilt page creates dummy widgets. Mitigation: binders use `findChild` and run on `pageBuiltSignal`; the getChild-name gate (section 6.2).
2. **Two Directors diverge.** v1 is frozen except for crash fixes. The binder refactor is the single shared risk: the reviewer diffs v1 call order.
3. **Legacy pages at 850×560** are an intermediate P0 risk only. The final delivery converts them all, audits their non-default states, and requires absolute zero; `DIRECTOR_KIT_ROLLOUT_SPEC.md` is binding.
4. **History attribution.** Background writers, scene loads, slot changes, reset-all, and external presets/cues use `ScopedSuspend` plus a generation barrier. Complete Director-owned bulk actions explicitly record one Step. Context and generation checks prevent cross-slot/cross-scene replay.
5. **The dyn-visibility probe** is a bool loop in `draw()`. It must stay allocation-free (Opus checks it).
6. **Mirror display.** Idle display reads already-resolved live offsets, but drag uses an uneased target frame and a mouse-down settings snapshot. WP1 publishes object-front azimuth from the already-resolved root rotation, so avatar and object Mirror use the same live-offset path; the rotated-prop test detects any regression to world-east. Rig-off fallback alone is unmirrored and every drag is absolute from mouse-down.
7. **Card-header preset reparenting** moves C++-owned combos. Pointers stay valid, but `getChild` from ALPanelCineLightRig after postBuild would miss them. Codex verifies that ALPanelCineLightRig never looks them up after postBuild.
8. **Rect bump.** A v1 saved width of 1020 bumps once to 1160.
9. **Search index staleness.** `gen --check` is a prebuild gate.
10. **Codex runtime wedge** (memory). The orchestrator drives each WP with the watchdog and `--prompt-file`.

---

## 10. Resolved rollout decisions

There are no open rollout choices in this specification. The binding decisions below govern P0 and the mandatory continuation in `DIRECTOR_KIT_ROLLOUT_SPEC.md`: all-at-once delivery, absolute-zero final audit, horizon-zero arcs, camera-relative Light map, and chips on every converted page. Intermediate legacy baselines are test scaffolding only.


## USER DECISIONS (2026-10-10, binding; supersede the recommendations above)
- **Rollout: ALL AT ONCE.** The user receives the new Director only when EVERY page is a full kit page matching the mockup. P0 (WP1-WP5) is the foundation; the remaining 32 legacy-embedded pages MUST be converted to kit pages (later phases, executed back-to-back) before v2 is offered to the user. Intermediate builds are allowed for automated gates only (self-test/audit); DirectorUIV2 stays off by default until all pages pass.
- **U2 audit gate:** with all-at-once, the final gate is an ABSOLUTE 0 layout findings on every page at all 4 sizes (the "0 new vs v1" rule applies only to intermediate builds).
- **U3 tilt arc:** HORIZON = 0 (needle sideways at 0; up positive, down negative, like a camera tilt gauge).
- **U4 Light map:** CAMERA-RELATIVE (camera at the bottom). No north-up toggle.
- **U5 chips:** every page gets section chips when converted (all pages, per the earlier decision).

## Review R1 resolution

- B1 -> `DIRECTOR_KIT_ROLLOUT_SPEC.md` sections 1-8 (36-page matrix, adapters/kinds, WP6-WP12, final ship gate) and this spec sections 0, 4.3, 6.2, 6.4, 7, 10.
- B2 -> sections 3.2, 7 WP1, and spike S2 (`LLCheckBoxCtrl` inner-button binding, widget defaults, click/external/direct-`setValue`/passive paths).
- B3 -> sections 3.5, 3.8, 3.18, and spike S8 (one page-scoped mode adapter, guarded `applyEasyMode`, and effective-mode publication for every source).
- B4 -> `VCAM_MONITORS_DESIGN.md` section 5.4 and VC1 (fine-grained identity-preserving typed undo; `applySceneData()` forbidden).
- B5 -> `VCAM_MONITORS_DESIGN.md` sections 4.2, 4.5, 7.1, and VC2/VC4 (CPU+GPU/scratch charge, fixed bucket, quality profile, admission and p95 gates).
- B6 -> section 3.11, `DIRECTOR_KIT_ROLLOUT_SPEC.md` Camera/VCam row, and `VCAM_MONITORS_DESIGN.md` sections 1, 5.4, 6.1 (kit page, frozen v1 XML, sole `ALKitHistory`).
- M1 -> `VCAM_MONITORS_DESIGN.md` section 4.2 and VC2 (visible-first two-tier cadence and Every Frame preview deadline).
- M2 -> `VCAM_MONITORS_DESIGN.md` section 5.1 and VC5 (`SOURCE_VCAM`, no toolbar inventory lookup, real drop flag).
- M3 -> sections 3.12 and spike S8 (text-entry focus guard and Unicode `/` path).
- M4 -> section 3.11 (bulk `ScopedSuspend`, reachable `globalBarrier`, playback lifetime token, and scene-generation contexts).
- M5 -> sections 6.1 and 5.1 (`settings_files.xml` plus recursive includes, setting groups, Vector3-backed vec2).
- M6 -> section 3.14 Frames/Display transform (live idle offsets, uneased drag targets, object-front-relative Mirror, consistent unmirrored rig-off fallback).
- M7 -> section 3.14 Input/Pure maths and section 6.6 (absolute-from-mouse-down inverses, shared effective radius, exact 2.5D/Side branch tests).
- M8 -> section 3.13 and section 6.6 (binding horizon-zero geometry and inverse).
- M9 -> section 6.4 and `DIRECTOR_KIT_ROLLOUT_SPEC.md` section 6 (all non-default audit states and measured label errors).
- M10 -> section 4.6 (850/1160 width budgets, mutually exclusive scene slot, ellipsizing status, extended collapse).
- M11 -> section 7 WP1/WP2 (interface headers and linkable stubs precede ALKitPage dependencies).
- M12 -> sections 3.4, 3.7, and 3.8 (handle-guarded deferred work, shell-owned timers, no static shell/panel captures).
- M13 -> sections 4.4 and 6.2 (dynamic name expansion and requested/actual class compatibility).
- M14 -> `VCAM_MONITORS_DESIGN.md` section 4.3 and VC3 (complete GL-state contract and previous-presented-frame exposure plus current tonemap/sRGB transform).
- M15 -> sections 0 and 3.16 plus `DIRECTOR_KIT_ROLLOUT_SPEC.md` sections 1 and 8 (no normal menu exposure before final gate).
- M16 -> section 3.8 (version-2 index header and exactly 15 validated fields).
- MIN1 -> section 6.4 (`min` maps to existing `minimum`).
- MIN2 -> sections 3.13, 7 WP1, and 6.6 (`kit_geometry` opt-in and `wheel_adjust`; legacy dials unchanged).
- MIN3 -> sections 3.13 and 6.6 (compass wrap honors 0..360 range).
- MIN4 -> section 3.14 Legend/Pure maths (10 Hz diff-guarded camera-relative side labels and one helper signature).
- MIN5 -> section 3.4 Visibility probe (all-row bit vector and actual row visibility).
- MIN6 -> section 3.15 (one-shot missing-flow-grid lookup in v2 reshape).
- MIN7 -> section 3.15 (`ALKitDoc` permanently reserves scrollbar width).
- MIN8 -> section 5.1 fixture row (`setValue` plus `onCommit` under suspension).
- MIN9 -> `VCAM_MONITORS_DESIGN.md` section 4.1 (visibility owner tokens/refcount).
- MIN10 -> `VCAM_MONITORS_DESIGN.md` section 4.2 (`prepare()` decides visible versus preview-only).
- MIN11 -> `VCAM_MONITORS_DESIGN.md` section 5.2 (scene restore uses the common validator, quarantines offline faces, and skips permission-only failures without rejecting existing scenes).
- MIN12 -> `VCAM_MONITORS_DESIGN.md` section 5.2 (drop publication demand; retained output stays lifetime-fixed).
- MIN13 -> `VCAM_MONITORS_DESIGN.md` section 4.5 (`GL_TIMESTAMP`, never nested `GL_TIME_ELAPSED`).
- MIN14 -> `VCAM_MONITORS_DESIGN.md` section 5.1 (reserved cargo comment and `static_assert`).

## Review R2 resolution

Findings below are the R2 items that land in this P0 spec or `VCAM_MONITORS_DESIGN.md`; rollout-only N2, N3, M-R7, M-R10 and MINOR 6 belong to the separately owned rollout document.

- N1 -> section 3.14 Frames/Displayed elevation/Input/Pure maths and section 6.6 (uneased target display while grabbed; immutable settings/pointer snapshot; exact absolute Top, 2.5D and Side inverses; shared subject-scaled radius; zero-motion and 1,000-event no-drift tests).
- M-R1 -> sections 3.2, 3.5 and spike S2 (`setValue` observes direct C++ writes; edge-only flip reconciliation updates state text, amber, dimming and effective mode cards).
- M-R2 -> sections 3.5 and 3.18 (page-owned Rig handle, `mSyncingEasyControls` guard, effective-mode publication, independent passive standalone synchronization).
- M-R3 -> `VCAM_MONITORS_DESIGN.md` section 5.4 and VC1 (`RemovedCaptureRecord` stores/rearms exact Gate state and subscribed displays atomically).
- M-R4 -> `VCAM_MONITORS_DESIGN.md` section 4.5 and VC2/VC4 (scoped enumerated preview flags/sites plus dedicated pinned 192x108 target outside the LRU).
- M-R5 -> `VCAM_MONITORS_DESIGN.md` section 4.3 and VC3 (previous-presented-frame exposure, shared color-correction permutation helper, explicit bloom exclusion).
- M-R6 -> section 3.8 (only configured writable homes collapse; other occurrences are links/read-only; duplicate writable homes fail generation).
- M-R8 -> `VCAM_MONITORS_DESIGN.md` section 5.2 and VC1 (permission-only binding skips with report; structural failure remains atomic; Director continues a warning-success scene load).
- M-R9 -> section 3.11 (`globalBarrier` resolves the Director at call time; presets barrier once; cue playback suspends and barriers only at start/stop).
- M-R11 -> `VCAM_MONITORS_DESIGN.md` section 8 and VC1-VC7, plus this ledger's VCam cross-references (VC prefix and rollout WP9 ordering).
- M-R12 -> section 3.14 Frames, section 6.6 and risk 6 (WP1 publishes object front from `getRenderRotation()`; Mirror stays enabled and is tested on a rotated prop).
- MINOR 1 -> section 4.6 width proof (1160 row: 132 px gaps/padding, 320 px status).
- MINOR 2 -> section 4.6 transport notes (`rec_indicator` preserved; only new presentation suffix collapses).
- MINOR 3 -> `VCAM_MONITORS_DESIGN.md` VC6 (tab-name migration, last-tab and v1/v2 rect/selection carry-over tests).
- MINOR 4 -> section 3.2 and spike S9 (`setControlName` replaces the old scoped connection).
- MINOR 5 -> section 6.7 screenshot review (48x48 left-weighted arc compared explicitly).
