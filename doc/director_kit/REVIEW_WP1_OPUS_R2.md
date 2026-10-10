# Adversarial review WP1 R2 (HEAD 1af3670dc8b)

Reviewer: Opus, independent context, 2026-10-10.
Scope: the R1 findings re-verified against HEAD, plus new defects introduced by the fixes and the reformat.

**VERDICT: FIX-NEEDED.** 2 BLOCKER, 6 MAJOR, 11 MINOR. All are new, or were missed in R1. No R1 BLOCKER or MAJOR regressed into its original form.

**Evidence base.**
- `zs_check.ps1 -Base 0e17c540240 -IncludeTests` passes 24/24 TUs (`cl /Zs /W3 /WX`, real Release flags).
- The maths was **executed**. I compiled a standalone harness (scratchpad, not the repo) that includes `alkitmath.h` only. It replays TUT test<10> and sweeps every inverse:
  - 681,012 moved-pointer drags across Top, 2.5D and Side, both side directions, r0 ∈ {0.6, 2.3, 6} m, OffsetZ ∈ {−1, 0, 0.4, 1.5}, orbit ∈ {−30, 0, 12};
  - 125k direct `pDisp → baseFromDisplayPitch` round trips.

  The results are cited below.

---

## R1 findings: verification

| R1 | Status | Evidence at HEAD |
|---|---|---|
| B1 static-init Params crash | **FIXED** | `alkitpage.h:14` gives `ALKitScroller::Params : Block<Params, LLScrollContainer::Params>`. Every registered kit tag now has a distinct Params type (`alkitcard.h:9,14`, `alkitrow.h`, `alkitswitch.h:9`, `alkitpage.h:23-29`, `alkitlazypage.h`, `alkitshell.h`, `allightrigmap.h`). `ParamDefaults` recurses through base blocks, so `scroll_container.xml` defaults still apply. |
| B2 compile | **FIXED** | 24/24 /Zs pass (W3/WX), including llui and the test TU. |
| B3 switch pink box / hit area | **FIXED** | `ALKitSwitch::draw` no longer draws the base or children (`alkitswitch.cpp:44-73`). Images are untouched, so the LL_WARNS spam is gone. The 34x18 button rect and hidden label are set in the constructor (`:18-25`) as well as in reshape. |
| B4 first edge lost | **FIXED** | `setControlName` reseeds `mLastState = isOn()` silently after binding (`alkitswitch.cpp:97-99`). `initFromParams` and the explicit post-create binds both pass through it. |
| B5 typed selector values | **FIXED** | `typedOptionValue` converts every option to the `sel_setting` control type (`alkitcard.cpp:35-47,123-129`). Segment, combo and reveal writes now carry typed LLSD. |
| B6 option hit-testing | **FIXED** | `layoutBody` gives each option the card's full rect (`alkitcard.cpp:503-507`). See m5 for a side effect. |
| B7 card → page relayout | **FIXED** | `ALKitCard::requestLayout` forwards to `ALKitPage::requestLayout` (`alkitcard.cpp:673-690`). |
| B8 legacy host 1x1 | **FIXED** | The host is created with `getLocalRect()` and FOLLOWS_ALL before reparenting (`alkitpage.cpp:377-386`). |
| B9 metres vs pixels | **FIXED (units)** | `DragStart::canvas_radius` is used by both unprojections (`alkitmath.h:175-176,205,210`), and `radius` is r0 for the pitch inverse. The pitch inverse itself is wrong: see **NB1**. |
| B10 grid/doc ordering | **PARTLY FIXED** | `layoutRows` now sizes, then reshapes, then places (`alflowgrid.cpp` layoutRows). The doc is reshaped before the grid origin is set, and only the doc is floored. The grid anchor is wrong: see **NB2**. |
| M1 kit compass | FIXED (spec decision recorded) | `alcompassdial.cpp` `getDialGeometry`: centred for compass, `(6s,h/2)` for arc. See m6 (letters). |
| M2 effYaw | FIXED | `alkitmath.h:137-141`; tests 6 and 17 now assert 55 and 121. |
| M3 fallback camera-relative | FIXED | `allightrigmap.cpp:230,316,519` subtract φc. |
| M4 grab frame | FIXED | grab = pointer − `projectLight(t0,p0)` (`allightrigmap.cpp:527-530`). depth0 now comes from the active view. |
| M5 legend colours | FIXED | `role_colors[]` (`allightrigmap.cpp:298,335,475`). |
| M6 tests prove nothing | **PARTLY FIXED** | test<10> now drives moved pointers with separate R_px and r0, and test<11>/<16> assert intermediates. But test<10> **fails** (NM1), and the sweep shows the pitch inverse is wrong where the tests do not look (NB1). |
| M7 vec2 | FIXED | `ALKitVecBinding::commit` preserves extra components, and `isDefault` compares N components (`alkitbinding.cpp:166-219`). Ranges are applied only when provided (`alkitrow.cpp:100-101`). |
| M8 value column | FIXED for display | The flip and dial-commit hooks are in, and dial decimals default to 0. The mechanism broke the history hook: see NM2/NM5. |
| M9 mode | FIXED | `syncEffectiveMode()` runs at postBuild, on flip and after the adapter, by name in `mModeDefs` (`alkitpage.cpp:271-286,485-521`). |
| M10 registry reach | FIXED | `ALKitBindingRegistry::add` finds the page under the owner and calls `rebindRows()` (`alkitbinding.cpp:244-273`). |
| M11 gesture page key | FIXED | `beginGesture(MOUSE, pageKey, label)` (`alkitcard.cpp:267,290`, `alkitpage.cpp:771`). |
| M12 chips | FIXED | Effective visible-card count; empty-section chips are hidden; `scrollToView` converts into doc coordinates after a relayout (`alkitpage.cpp:528-576,671-688`). |
| M13 adoptPage | FIXED (see m8) | Type-based `ALKitPage` discovery; no fallback to `host = content` (`alscrollfit.cpp` `adoptPage`). |
| M14 context menu parent | FIXED, but see **NM6** | Parented to `gMenuHolder` (`alkitcard.cpp:295`). |
| M15 v1 compass behaviour | **FIXED** | Legacy wrap is (−180,180] again (`setValue` legacy branch). The 0.5 px dead zone, tab stop and keys are now kit-only, and Home is a no-op when unbound. No v1 `compass_dial` carries `control_name`, so the new `onCommit` rounding is inert for v1. Wheel = guard + the same 5° step. |
| R1 minors 1-18 | Fixed or acceptable | The exceptions are listed under MINOR below (m1, m2, m4). |

---

## BLOCKER

**NB1. `baseFromDisplayPitch` returns the clamp bound for about 1 in 6 pitches, so 2.5D and Side drags jump.**
- Location: `alkitmath.h:64-85`. This was already present in R1, and I missed it there.
- Candidates 3 and 4 are candidates 1 and 2 minus 360°. `angularDistance` wraps, so they **tie** with 1 and 2. Float noise lets the −360 alias win, and the final `clamp(best, −85−orbit, 85−orbit)` then pins the pitch to `−85−orbit`.
- Harness results:
  - direct round trip: 12,318 / 31,245 fail;
  - every (r0, h) bucket, **including h = 0**, fails about 17%, with a worst error of 169.8°;
  - moved-pointer drags: pitch is wrong in 56,340 / 213,528 Side and 63,766 / 221,964 2.5D cases, i.e. 26-29%;
  - example: r=0.6, h=−1, orbit=−30, base −54.77 → returns −55.0.
- Top view is immune only because the `target_pitch == p0` shortcut skips the inverse.
- Fix:
  - Compute the two principal solutions `q = T + asin(rhs)` and `q = T + π − asin(rhs)`.
  - `wrap180` each one.
  - Keep only `cos(q) > 0` (the valid branch for q within ±85).
  - Pick the one nearest `q0`, then clamp `q − orbit`.
  - Add a TUT sweep like the harness, not hand-picked values.

**NB2. Short pages render bottom-aligned.**
- `ALKitPage::relayoutNow` (`alkitpage.cpp:643-654`) floors the **doc** to the viewport, as R1 asked. It then places the grid at `y = PAGE_PAD_B`, which is the **bottom** of the doc.
- When the content is shorter than the viewport, every card sits at the bottom of the page with an empty band above it. At 1160x720 the Rim page and most short pages are affected.
- Fix: `mGrid->setOrigin(PAGE_PAD_LR, docH - PAGE_PAD_T - gridH)`, i.e. top-anchored.

## MAJOR

**NM1. TUT `alkitmath` test<10> fails as committed.**
- Replayed in the harness: Side view, both side directions, `t0 = −41 → target 32`. Expected yaw 32 (and −114 with sigma −1); got −32 and −50.
- The target lies on the **opposite** depth side. `dragSideAbsolute` deliberately keeps the light's side (spec §6.6, depth penalty at `alkitmath.h:216-219`), so the test contradicts the implementation.
- `ctest` / RUN_TESTS goes red. Integration tests run under ctest (`LLAddBuildTest.cmake` `add_test`), not at link, but the post-build test gate counts them.
- Fix: choose Side targets on the same depth side as `t0` (e.g. t0 = −41 → −70), and add an explicit "keeps side" assertion for a cross-side pointer.

**NM2. The row history hook records nothing for toggles or widget-bound rows.**
- The hook (`alkitrow.cpp:131-136`) records `recordWidget(c, mLastValue, now)`. But `refreshValue()` (`:376-383`) and `bindingChanged()` (`:385-396`) now **overwrite `mLastValue`**, and they run **before** the hook:
  - **Toggles:** `ALKitSwitch::onCommit` fires `noteState` (flip, so `refreshValue` sets `mLastValue = new`) before `LLCheckBoxCtrl::onCommit` emits the commit signal. The hook then sees old == new and drops the entry.
  - **`ALKitWidgetBinding` rows:** `resolveBinding()` (`:129`) connects the binding's commit slot before the hook (`:131`). `bindingChanged` therefore runs first and overwrites the baseline, so every `bind=code` row with `default_value` is never recorded. That includes the Rig Easy feather (R5 minor 2).
  - **Text rows:** the 10 Hz poll (`:166-186`) adopts each keystroke as the baseline before Enter commits.
- Fix: keep `mLastValue` as the history baseline only. Write it in the hook, and on a non-commit external change only while the control is not focused. Give the display a separate `mShownValue`.

**NM3. The dyn-visibility probe no longer sees v1 `setVisible` on a control.**
- `isEffectivelyVisible()` (`alkitrow.cpp:352-356`) dropped the `mControl->getVisible()` term. `mCodeHidden` is only seeded once from XML (`:67`) and changed by `setControlVisible`.
- Unchanged v1 C++ that calls `setVisible` (clip labels, shadow hint, gobo preview, enable tag: the probe's whole purpose, §3.4 and WP4) no longer collapses rows.
- Worse, a control that starts hidden in XML and is later shown by v1 code keeps its row hidden forever.
- Fix: restore `(mControl->getVisible() || link/vec)` in `isEffectivelyVisible`, as §3.3 specifies.

**NM4. The page bar overlaps at the default and narrow sizes.**
- `ALKitPage::reshape` puts `<P>_master_box` at `w − SEARCH_W − 270` (`alkitpage.cpp:618`), independent of the left cluster.
- With `preset_mode=widgets` (Rig) the left cluster ends at about x=378: combo 160, save, delete, reset, undo, redo.
- At 1160 the page width is 760, so the master box starts at 320. At 850 it is 794, so the box starts at 354. Both overlap Undo/Redo, so the §6.4 absolute-zero audit will fail on Light › Rig.
- Fix: place the master box after the left cluster (`bar_x + gap`) and right-align only search. Collapse per §4.6 if they collide.

**NM5. Every row runs its own 10 Hz `doPeriodically` timer.**
- Location: `alkitrow.cpp:166-186`. That is one `LLEventTimer` per row: about 150 on Rig and about 1,800 once all 35 pages are built. Each timer does a handle lookup, a `dynamic_cast`, an `isWidgetOnScreen` parent walk and an LLSD compare, forever, including on hidden pages.
- This conflicts with the CLAUDE.md "zero cost when hidden" and on-demand refresh rules. It is also the mechanism that corrupts the history baseline (NM2).
- Fix: one page-level `ALLiveReadoutClock` (or a shell clock) that iterates only the built, visible page's value rows, with its display state separate from the history baseline.

**NM6. The reset menu is a raw pointer with a shutdown double-delete.**
- `ALKitCard::~ALKitCard` (`alkitcard.cpp:93-101`) does `mResetMenu->getParent()->removeChild(...)` and `delete mResetMenu`.
- At quit, `cleanup_menus()` deletes `gMenuHolder` and every child, including this menu (`llviewermenu.cpp` `cleanup_menus`), **before** `delete mRootView` destroys any Director floater still alive (`llviewerwindow.cpp` `shutdownViews`). The card destructor then touches freed memory.
- Fix: store `LLHandle<LLView>` and in the destructor `if (LLView* m = mResetMenuHandle.get()) m->die();` (the viewer's context-menu pattern).

## MINOR

1. **Who chip.** `setVisible(true)` + `showList()` on a combo whose `who_picker` row is hidden still shows nothing (`alkitpage.cpp:336-341`). Inert until the WP2 popover; note it in the WP2 brief.
2. **Legacy summary.** It is placed at the host's bottom edge with default follows-top, so the host shrink pushes it off-host, where it is invisible (`alkitpage.cpp:387-395`). §3.5 puts it in the bar.
3. **Card draw.** It looks up `LLUI::getUIImage("Kit_RoundRect6"/"Kit_RoundRect4")` by name every frame (`alkitcard.cpp:716-717`). Cache the `LLUIImagePtr` in the constructor (rule 1 / ui_draw_lint).
4. **Option overlays block clicks.** Full-card option panels are mouse-opaque, so they block hits on any XML sibling declared before them. Set `mouse_opaque=false` on options.
5. **Kit compass letters.** They sit at `radius+6 = 26` from the centre of a 48 px cell (`alcompassdial.cpp` draw `lr`), so they overflow the cell by about 2 px plus half a glyph. That contradicts the new spec text "keeps the ring and N/E/S/W labels inside the control cell". Use `lr = radius + 4s` with a smaller radius, or have the generator emit `show_cardinals=false`.
6. **Launcher button name.** Renamed to `kc_<id>_launch` (`alkitcard.cpp:348`), but spec §3.4 still says `kc_<id>_open`. Record the rename in the spec (it resolves R1 m16).
7. **`adoptPage` prefix.** It derives P from the tab name (`alscrollfit.cpp` `adoptPage`). If the generator ever makes the tab name and page key diverge, kit focus and wheel install are skipped silently. Use `kit_page->findChild<ALKitScroller>()` by type.
8. **Side yaw near t = 0.** The Side inverse is ill-conditioned at t ≈ 0 because `acos` is near 1: 173/213k sweep cases are off by up to about 0.1°. Acceptable; document it, or use `atan2` with the depth sign.
9. **Per-tick copies.** `tickShared` now copies `Setup`/`Transforms`/`Globals` per tick (`alcinelightrig.cpp:2672-2675`). `Globals` holds two `std::string`s, so a long label means a small allocation per tick per rig. Negligible; a revisit is optional.
10. **Map tooltip text.** The side-chip tooltip is "Change the side view." (`allightrigmap.cpp:591-592`). §3.14 specifies the longer explanatory text.
11. **`computeShellLayout`.** It subtracts the full 16 px scrollbar from **each** card (`alkitmath.h:238`), which is conservative by 8 px in two-column layouts. The ≥120 track claim still holds.

---

## Checklist

- **B1 crash:** fixed and audited.
- **Switch state flow:** fixed; one new interaction with the row history (NM2).
- **Selector typing:** fixed.
- **Relayout:** card → page is fixed. Grid anchoring is wrong (NB2).
- **Map maths:** units are fixed and grab, fallback, colours and effYaw are fixed. The pitch inverse is broken (NB1), and the TUT is red (NM1).
- **Tests:** they now move pointers, but they miss NB1, and test<10> fails.
- **v1 compass:** unchanged apart from the wheel guard (verified line by line, plus a check of the v1 XML users).
