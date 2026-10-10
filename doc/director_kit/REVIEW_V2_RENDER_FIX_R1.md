# Director v2 render fix: adversarial review R1

Scope: `51cab9ead6d`, plus the follow-up `1d83c1b0aa1`, which only marks .webp as binary.

Evidence used:
- The three reference images.
- The user's broken-run log `%APPDATA%\AlchemyMachinima\logs\Alchemy.log`, 10/10 4:20 PM.
- The runtime code paths, traced by hand. Nothing was built or run.

## Verdict: FIX-NEEDED

There are 5 blockers:
- Two are in the runtime: construction-time page builds, and the page-chrome bands.
- One is in the generator and runtime pair: the preset host is never found.
- Two are in the live gate: false positives that keep it permanently red, and checks that would have passed against the broken build.

The non-legacy Rig body is now sized correctly. Four symptoms are gone at runtime: rail overlap, roster spill, transport order/REC, and tab overflow. The stray bar is also gone, but by accident.

## Runtime rects, Light > Rig, traced from the code

| Size | Shell | Rail / help | `director_tabs` → lazy → kit_page | Chrome (chips 38, who 34, bar 44, banner 32 when the rig is off) | Scroller viewport | Grid / columns |
|---|---|---|---|---|---|---|
| 1160×720 | 1160×702 | 176 / 224 pane | 760×622 → ≈756×620 | top=620 → 582 → 548 → 504 → 472 | 756×472 (504 when on), positive | doc 741, grid 709 → 2 cols × 348 (≥340) |
| 850×560 | 850×542 | 56 / 30-px strip | 794×432 → ≈790×430 | 430 → 392 → 358 → 314 → 282 | 790×282, positive | grid 743 → 2 cols × 365. **No help pane** (strip only). |
| 1141×720 (an audited size) | 1141×702 | 176 / 224 | 741 → ≈737 | as at 1160 | positive | grid 690 → **1 column** (needs 692) |

The chain that produces these rects:
- `ALKitShell::layoutChrome` (`alkitshell.cpp:337-350`) sets the shell layout.
- `LLTabContainer::addTabPanel` gives each tab panel `FOLLOWS_ALL` (`lltabcontainer.cpp:1031`).
- The new `ALKitLazyPage::reshape` → `fitContent` (`alkitlazypage.cpp:54-73`) sizes the content.
- Then `ALKitPage::reshape` (`alkitpage.cpp:725-796`) and `relayoutNow` (`:798-814`) lay out the page.

The help pane is present at ≥1100 wide only.

## BLOCKER

**B1. Pages are built during XUI construction, at a ~9 px host. The legacy-page blank (Look > Image) is neither explained nor fixed.**

- **Evidence.** In `Alchemy.log`, lines 1226-1253 show the first page of all 8 groups being built *before* `Director v2 chrome build` (`postBuildV2`). Those pages are move, animate, shot, focus, cine_light_rig, image, weather and temporal, and they include both screenshot pages.
- **Mechanism.**
  - Each inner `tab_container` runs `postBuild` → `selectFirstTab` → `ALKitLazyPage::onVisibilityChange` → `ensureBuilt` (`alkitlazypage.cpp:75-79`).
  - `director_tabs` has no XUI height, so it gets `MIN_WIDGET_HEIGHT` 10 (`llview.cpp` `applyXUILayout`). That makes the host lazy page about 1156×9.
  - `ALKitShell::postBuild` has not run yet, so the `pageBuilt` signal has no slots (`alkitshell.cpp:65`, `llfloaterdirector.cpp:753`).
- **Consequence (a): pages are never registered or bound.**
  - These 7 non-initial pages never reach `pageBuilt`: no `mBuiltPages` entry, no search keystroke hook, no who popover.
  - They also never reach `LLFloaterDirector::bindPage`, so `bindAnimatePage`, `bindCameraPage` (shot) and `bindLauncherPage` (image, …) never run (`llfloaterdirector.cpp:945-985`).
  - Later selection only calls `ensureBuilt()`, which returns the cached content and emits nothing.
- **Consequence (b): legacy pages are not repaired by this fix.**
  - Legacy pages (31 of 35, including Image, whose root is `<kit_page height="525" legacy="true">`, not the "unsized wrapper" the diagnosis claims) are squeezed from 525 → 1 px → N px through `FOLLOWS_ALL` deltas on the v1 content in `mLegacyHost`.
  - `fitContent` only resizes `mContent`, which is exactly what the old `ensureBuilt` already did for a root `kit_page`. The legacy branch of `adoptPage` is unchanged (`alscrollfit.cpp:492-505`).
  - So nothing in this commit changes the Image path that rendered blank.
- **Fix.**
  - Never build from `onVisibilityChange` until the shell is ready: add an `ALKitShell::mReady` flag, set it at the end of `postBuildV2` after the signals are connected, and before that point have `ensureBuilt` return null.
  - Build the initial page explicitly once the shell is ready, at its final host size.
  - Verify in the log that exactly one `Kit page … built` line appears before `chrome build`, with any other builds on user selection.
  - Then confirm Look > Image visually with the per-page PNG before claiming symptom 1 fixed.

**B2. Every page's chrome bands put their contents outside the band on first sizing.**

This is pre-existing, but the old collapse masked it, and fixing the sizing now exposes it on all 35 pages.

- `makePanel` creates the bar, who row, banner and master box at `LLRect(0,1,1,0)` (`alkitpage.cpp:46-54`).
- `postBuild` then places children at y=10 inside these 1-px panels, using the default `FOLLOWS_LEFT|FOLLOWS_TOP`:
  - reset (`:277`), undo/redo (`:338-342`), and the preset items (`:252-253`);
  - who hint (`:435`);
  - banner text and the Turn on button (`:412`, `:428`);
  - master label and state (`:351`, `:355`).
- The first band `reshape` in `ALKitPage::reshape` (`:746`, `:754`, `:765`, `:782`) shifts those children by delta-height (`llview.cpp:1517-1521`):
  - bar items +43, which puts them at bar-local 53..77, inside the who row and on top of the who chip (`:748`);
  - who hint +33, into the chips row;
  - banner contents +31, into the page bar;
  - master label and state +23, out of their 24-px box.
- Only the chips, the who chip, the master box, the master switch and search are re-originated on each reshape.
- **Fix:** create each band at its final height (`LLRect(0, BAR_H, 1, 0)`, and so on), and lay out every band child in `reshape()`, not in `postBuild`. Alternatively, give band children `FOLLOWS_LEFT|FOLLOWS_BOTTOM`.

**B3. The preset host is never found, so there is no preset combo / Save / Delete in the page bar.**

- `director_kit_gen.py:586` emits `<kit_preset_host>` with no `name`.
- There is no `widgets/kit_preset_host.xml`, so the name defaults from `widgets/panel.xml` to `"panel"`.
- Therefore `findChild<ALKitPresetHost>("kit_preset_host")` (`alkitpage.cpp:243`) returns null.
- The host stays as an unmanaged visible child of the page body at its XUI default rect. Its `cine_setup_combo` inherits `follows="right|top"` from `widgets/combo_box.xml`.
- The page bar lacks the mockup's first three items.
- **Fix:** emit `name="kit_preset_host"` (or look the host up by type). Also stop forcing items 2..n to 28 px: "Save" and "Delete" get clipped (`:252`). Use a measured label width, or icon buttons.

**B4. The live gate is permanently red because of false positives.**

- `collectTabs` puts the 8 group panels (`cast_group` and the rest) into `mTabs`.
- For those entries, `auditDirectorLiveLayout` sees `dynamic_cast<ALKitLazyPage*>(group panel) == nullptr`. It emits `LAZY_PAGE_UNATTACHED` (`aluiperfselftest.cpp:2014-2021`) at every one of the 5 sizes, giving at least 40 findings.
- Non-lazy nested tabs that are inserted at `TAB_SELECT` (`:2966-2970`) do the same.
- `ui_selftest.ps1:478-493` fails on any of these, so the gate can never pass and will be waived.
- **Fix:** run the lazy-page block only when `mTabs[i].mPanel` is an `ALKitLazyPage`.

**B5. Some checks would have passed against the broken build, and one symptom has no check.**

Each check below was re-run by hand against `32b7fb162ae`:

| Check | Old build | Problem |
|---|---|---|
| Tab overflow (symptom 6) | — | **No check exists.** Nothing asserts that `kit_tab_prev` / `kit_tab_next` are hidden for the approved groups at 850/900/1160, or that the visible tabs fit inside the strip. Symptom 6 would regress silently. |
| `LAZY_PAGE_NO_CARD` | passes | Uses `visibleWithin`, which tests only visibility flags and positive size. The old cards were visible inside a 1-px scroller, so they passed. |
| `LAZY_PAGE_UNSIZED` on legacy pages | passes | `mContent` *is* the page, and `ensureBuilt` always resized it. The blank Image page passes. |
| `KIT_SCROLLER_*` on legacy pages | not run | Only `ALKitScroller` is checked, so the v1 scroll containers inside `mLegacyHost` are skipped. A blank legacy body triggers no live finding at all. |
| Page-chrome presence (symptom 6 in the diagnosis: chips, who, page bar, presets, search) | — | No check. B2 and B3 would pass the live gate; only the generic overlap audit might catch them. |

Fix:
- Add `TAB_OVERFLOW` and `TAB_LABEL_CLIPPED` checks. Measure the label as displayed (`getCurrentLabel`), including the "○" pip.
- Make the card check geometric: at least one card rect, in floater coordinates, must intersect the scroller's visible rect by ≥40 px.
- Add a legacy-body check: the legacy host height must be ≥ page height minus the chrome, with at least one visible leaf intersecting it.
- Add a `PAGE_CHROME` check: preset combo, reset, undo, redo, master switch and search each visible, contained in the bar rect and not overlapping; the who chip inside the who band.

## MAJOR

- **M1. The diagnosis of the stray bar is wrong.**
  - The full-width grey bar is the roster's `group_assign_combo`, not a page scrollbar:
    - its XUI default width is the shell's 1160;
    - `widgets/combo_box.xml` gives it `follows="right|top"`, so when the roster is reshaped from 1160 to 176 it moves −984 px. The screenshot offset is about −985 px.
    - The crop shows a combo's ▾ button at its right end.
    - It sits at the same pixels on both pages.
  - `layoutRoster` (`alkitshell.cpp:488-492`) removes it incidentally. `KIT_HORIZONTAL_SCROLL` guards a cause that did not exist; `DIRECTOR_SPILL` and `ROSTER_SPILL` are the checks that matter.
  - Correct `V2_RENDER_DIAG.md` item 5. Set explicit `follows` on every re-homed chrome control in the generator, so no widget-template default leaks through.
- **M2. Breakpoints vs. card width.**
  - Lowering `BP_FULL` to 1100 (`alkitstyle.h:20`) creates a 1-column band from 1100 to about 1145, which includes the audited 1141×720. In the same band (1100 to about 1115), search is hidden (`alkitpage.cpp:760-775`, `search_w < 120`).
  - Separately, `UILayoutAuditSizes` still lists 1141 and 915 (`ui_selftest.ps1:77`). Those are the old 1142/916 breakpoints, so the new 1100/980 edges are never audited.
  - **Fix:** derive help visibility from the width the 2-column grid needs (lazy ≥ 2·340 + 12 + 2·16 + 15), or set `BP_FULL` ≥ 1150. Audit 1099/1100/1108 and 979/980/988.
- **M3. The transport at 850 is still crammed.**
  - Required width = 20 + 858 + 17·8 = 1014. Collapsing `kit_ui_ms` gives 936, and collapsing `take_timecode` gives 838.
  - So at 850 and at 900 the **timecode is hidden**, and at 850 `status_strip` is **12 px** wide.
  - The diagnosis asked for a "readable minimum flex width". It is not implemented.
  - **Fix:** collapse `kit_ui_ms`, `btn_reset_marks`, `btn_scene_delete`, and the Cast chip when the roster is in the rail, *before* the timecode. Give the scene combo a minimum of 110. Hide `status_strip` below 120 px. Add a `TRANSPORT_FLEX_MIN` check.

## MINOR

- **Rail labels.** They stay `HCENTER` in the area left over (`LLButton` default), but the mockup is left-aligned: set `font_halign` to LEFT. The active "•" is appended to the label text instead of being drawn at the right edge. The `RAIL_ICON_LABEL_OVERLAP` model uses a 4-px label space; the real `imgoverlay_label_space` is 1 (`llbutton.cpp:87`).
- **Tabs.** Widths are measured from `tab.label`, which ignores the "  ○" pip (`alkitshell.cpp:412` vs `:308`). `mTabOffset` is clamped to `visible_tabs-1` and never reset when there is no overflow or when the group changes. Tabs can therefore disappear with no arrows (`:419`).
- **Chips.** The labels show the section total ("Lights 5"), not the count visible in the current mode. Hidden chips still take up a 106-px slot, which leaves a gap between "Easy" and "Lights" (`alkitpage.cpp:571`, `:735-740`).
- **Who row.** The who chip (16..376) overlaps the hint at x=370 (`:435`, `:748`).
- **Roster.** The tools sit on their own row instead of on the `CAST n` header row. `group_assign_combo` is not in the mockup. There is a 40-px blank slot between the last rail item and the roster (`alkitshell.cpp:352-368`). The roster is never audited in popover mode (850/900/915).
- **Transport check.** It covers idle and record states only, not the save state (where the name editor is visible). There is no duplicate-name check: duplicate REC was only caught indirectly.
- **`RAIL_GROUPS`.** It is a static equality check that also passed on the old build; that is harmless, but it is not a render check. Separately, `railGroups()` returns a `const` value.

## Item checks

- **4. Capture.** OK.
  - The data still holds `temporal_tab` (Time) and `scene_options_tab` (Scene). The data diff against `32b7fb162ae` is formatting only.
  - `capture_group` is in the rail, the tree, `bindChrome` and the approved list.
  - The only rule left is the `capture_settings_tab` ban in `check_kit_pages.py:160,383`.
- **5. v1 with `DirectorUIV2` off.** Unchanged.
  - `floater_director.xml` is untouched.
  - `bindChrome` and `btn_scene_load` are v2-only (`postBuildV2`).
  - `mCastRosterHeader` is null in v1, so the guard in `refreshCastList` is a no-op.
  - The kit, scroll-fit adopt and self-test changes are all gated to v2.

## Symptom outcome at runtime

| Symptom | Gone? | Note |
|---|---|---|
| Empty page bodies | **Partly** | Rig scroller and grid are fine (rects above). Legacy pages (31/35, including Image) are unfixed or unexplained (B1). Page chrome is scrambled (B2, B3). |
| Rail icons over labels | Yes | Icon at x=12; label in the remaining area. |
| Roster outside floater | Yes | `layoutRoster`. |
| Transport crammed / reversed / duplicate REC | Order and REC: yes. Cramming: no (M3). | |
| Stray horizontal bar | Yes, by accident | M1. |
| Tab overflow | Yes, at 850–1160 | Light needs about 449 px of the 572/606 available. There is no check (B5). |

## Mockup structure checklist (Light > Rig, 1160×720)

| Element | Status |
|---|---|
| Title breadcrumb "Director › Light › Rig" | missing (title is "Director"; the path is only in the help kicker) |
| Rail: 8 groups, icon + label | present; label centred, not left (minor) |
| Active rail item: amber label + right dot | wrong (dot appended to the text) |
| Cast roster: "CAST n" header + tool icons on the same row | wrong (tools on a second row; extra group combo) |
| Cast list rows (status, name, state, group tag) + hint | present (5-column list; differs in detail) |
| Tab strip, one row, no arrows | present |
| Easy / Advanced at the right | present |
| Chips "Easy 2", "Lights 2" | wrong (totals; gap from the hidden Setup chip) |
| Who row: label, "Acting on" chip, hint | wrong (hint pushed into the chips row, B2) |
| Page bar: preset combo, Save as…, delete | **missing** (B3) |
| Page bar: reset, undo, redo | **wrong** (drawn 9-33 px above the bar, over the who chip, B2) |
| Page bar: master switch "Light rig ● On" | wrong (label and state shifted up 23 px, B2) |
| Page bar: search | present (hidden from 1100 to about 1115, M2) |
| 2-column cards: Light map + Lights | present at 1160 and 850; 1 column from 1100 to about 1145 (M2) |
| Light map: NEW badge, reset, Top/Side/2.5D, legend | present (XUI) |
| Lights: Key/Fill/Back/BG option tabs; EV/Colour/Yaw/Pitch/Gel/Gobo/Shaft/More | present |
| Lights card header on/off switch | missing |
| Mockup opens on map + lights | differs (opens on the Easy section; scroll-chip mode) |
| Help pane "About this control" | present ≥1100 (page summary); strip below that |
| Transport: ACTION first, single REC, … order | present |
| Transport: timecode always visible | wrong at ≤935 (M3) |
| Banner when master is off | wrong (text and button shifted 31 px, B2) |
