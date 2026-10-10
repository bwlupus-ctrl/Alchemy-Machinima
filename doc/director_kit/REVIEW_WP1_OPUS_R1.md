# Adversarial review WP1 R1 (Director kit, uncommitted on 0e17c540240)

Reviewer: Opus, independent context, 2026-10-10.
Scope: the WP1 file set only (alkit*, allightrigmap*, alkitmath.h, tests/alkitmath_test.cpp, kit_switch.xml, kit/*.png, kit_png_gen.py, diffs of alcompassdial.*, alflowgrid.*, alscrollfit.*, alcinelightrig.*, CMakeLists.txt, colors.xml, textures.xml). The concurrent water-height job was ignored.

**VERDICT: FIX-NEEDED.** 10 BLOCKER, 15 MAJOR, 18 MINOR.

**Line numbers.** Most new .cpp files put a whole function body on one physical line. Each `file:N` points at the line that holds the named function or snippet.

**Compiler evidence (no build, no repo writes).** I ran `cl /Zs` (syntax and semantic check only, so no objects were written) on every WP1 translation unit. It used the real Release flags extracted from `alchemy-bin.vcxproj`, `llui.vcxproj` and the alcinelightrigmodel TEST vcxproj: `/W3 /WX`, C++20 and the PCH header force-included. Then I applied only the compile fixes listed in B2 to a scratch copy and re-ran. Every TU was then clean. So the B2 list is the complete set of compile errors at this level. Link was not run, but every symbol declared in the WP1 headers has a definition.

---

## BLOCKER

**B1. The viewer crashes at startup, even with DirectorUIV2 off.**
- Evidence: `alkitpage.cpp:22` registers `ALKitScroller` as `kit_scroller`. `alkitpage.h:11` gives it no Params block of its own, so its `T::Params` is `LLScrollContainer::Params`, which `scroll_container` already registered.
- `LLChildRegistry::Register` calls `LLUICtrlFactory::registerWidget` during static initialization (`lluictrlfactory.h`). `registerWidget` (`lluictrlfactory.cpp:256+`) prints "Duplicate entry for T::Params" and then deliberately dereferences null (`char* foo = 0; *foo = 1;`).
- Every launch dies before `main`.
- Fix: declare `struct Params : public LLInitParam::Block<Params, LLScrollContainer::Params> {};` in ALKitScroller. Build it with `ALKitScroller::Params` in `ALKitPage::postBuild`.

**B2. Eight compile-error sites, all confirmed by `cl` (W3/WX).**
1. `alkitrow.h:16`: the inline `reset()` calls `mBinding->reset()` on an incomplete `ALKitBinding` (C2027/C2232). This breaks alkitrow.cpp and alkitcard.cpp. Fix: `#include "alkitbinding.h"` in alkitrow.h, or move `reset()` out of line.
2. `alkitcard.cpp:70`: `child_list_reverse_iter_t it = getChildList()->rbegin()` on a const list (C2440). Fix: use `child_list_const_reverse_iter_t`.
3. `alkitpage.cpp:77`: `isWidgetOnScreen` is undeclared (C3861). Fix: `#include "aluirefresh.h"`.
4. `alkitpage.cpp:104`: `mScroller->updateScroll()` is private in LLScrollContainer (C2248, `llscrollcontainer.h:141`). Fix: delete the call (the container recomputes on reshape/draw).
5. `alkitrow.cpp:71`: `mValueText->setText(cond ? "On" : "Off")` (C2664; LLStringExplicit is explicit). Fix: `setText(LLStringExplicit(...))`.
6. `allightrigmap.cpp:36`: `setToolTip("Side view ...")` (C2664). Fix: wrap it in LLStringExplicit, and see m11.
7. `allightrigmap.cpp:91`: `S32 x,y` passed to the `float` parameters of `dragTopAbsolute/dragSideAbsolute/drag25DAbsolute` (C4244, an error under /WX). Fix: `(F32)x, (F32)y`.
8. `indra/llui/alcompassdial.cpp:268,280-285,296-297`: CRT `llround()` returns `long long`, which is passed to the `S32` parameters of `gl_triangle_2d/gl_line_2d/renderUTF8` (C4244 into C2220 in llui). Fix: use `ll_round(F32)` (it returns S32) or `(S32)llround(...)`, as the legacy compass block already does.

Also the TUT test: every `ensure_approximately_equals(msg, a, b, .001f)` in `tests/alkitmath_test.cpp` passes a float to the `U32 frac_bits` parameter (`lltut.h:100`). That is C4244 into C2220. The test target is built, so this breaks the build too. It is also semantically wrong: `frac_bits = 0` means a tolerance of about 1 unit, not 0.001. Fix: use `ensure_approximately_equals_range(msg, a, b, tol)` (`lltut.h:115`).

**B3. Every ALKitSwitch draws a pink box, and many are only 13x13 clickable.**
- `alkitswitch.cpp:13` (`reshape`) nulls the button images. `LLButton::draw` with a null image draws a pink outline over the full 34x18 rect (`llbutton.cpp:895-901`) every frame. The pill does not cover the corners or the x=34 edge.
- `setImageUnselected(nullptr)` also logs `LL_WARNS "Setting default button image ... to NULL"` on every reshape (llbutton.cpp). Every page relayout therefore logs one warning per switch.
- `mImagePressedSelected` is not cleared, so the stock pressed image flashes on click.
- The hit-rect expansion and the label hiding happen only in `reshape()`. Switches created in code are only `setOrigin`'d and never reshaped: the card enable and option enables (`alkitcard.cpp:28` postBuild) and the page master (`alkitpage.cpp:64` postBuild). Those switches draw the stock 13x13 checkbox under the pill, and only that 13x13 square is clickable.
- This breaks the "entire 34x18 switch is clickable" promise of §3.2.
- Fix:
  - Do not call `LLCheckBoxCtrl::draw()`; draw only the pill.
  - Leave the button images alone, or set a transparent image.
  - Apply the 34x18 button rect and hide the label in the constructor (`initFromParams`) as well as in `reshape`.

**B4. ALKitSwitch loses the first edge whenever the bound setting starts true.**
- The constructor seeds `mLastState = isOn()` (`alkitswitch.cpp:12`) before `initFromParams` binds the control.
- `LLUICtrl::setControlVariable` then seeds the inner button through `LLButton::setValue(control->getValue())` (`lluictrl.cpp:535`). That bypasses `noteState`.
- The same happens for the explicit `setControlName` calls after create (page master and mode at `alkitpage.cpp:64`, card and option enables at `alkitcard.cpp:28`).
- With the setting true, `mLastState == false` while `isOn() == true`. The next OFF, whether from a click, an external `set()` or `writeBoundValue`, fires no flip.
- What misses the first transition:
  - the Easy/Advanced card swap;
  - the master-banner relayout;
  - state text and amber.
- This is exactly what spike S2 was meant to prove.
- Fix: at the end of `ALKitSwitch::setControlName`, silently re-seed `mLastState = isOn(); mHaveState = true;`. Listeners seed from `isOn()` when they connect.

**B5. Selector cards never show an option (string versus integer LLSD).**
- `ALKitOption::value` is parsed from XUI as an LLSD **string** (`llxuiparser.cpp:1262`, `LLSD(getSanitizedValue())`).
- `syncSelector` (`alkitcard.cpp:53`) compares it with `gSavedSettings.getLLSD(sel_setting)`, which is an Integer for the S32 `DirectorV2SelectedLight` and the Rim/Shot selectors. `llsd_equals` returns false on any type mismatch (`llsdutil.cpp`). So every option is hidden and the Lights and Rim card bodies are empty.
- The segment, combo and reveal writes (`alkitcard.cpp:46-47`, `revealRow`) store the string `"2"` into the S32 control. That changes the type of the saved setting.
- Fix: in postBuild, convert each option value to the `sel_setting` control type (S32 → `asInteger()`, F32 → `asReal()`, Boolean, String). Compare with that type, not with raw LLSD `==`.

**B6. Rows inside a `kit_option` cannot receive the mouse.**
- `ALKitCard::layoutBody` (`alkitcard.cpp:55`) positions option rows in the option panel's coordinates but never sizes or positions the option panel. Kit tags carry no geometry, so the option panel keeps its XML/default rect (0x0).
- LLView child hit-testing requires the point to be inside each ancestor's rect, so clicks, hovers and focus never reach those rows.
- The rows still draw, because LLPanel does not clip. The Lights and Rim selectors look fine but are dead.
- Fix: in `layoutBody`, `setShape(getLocalRect())` on the active option (and on hidden options too) before laying out its rows.

**B7. A card that changes height never relayouts the page.**
- `ALKitCard::requestLayout` (`alkitcard.cpp:68`) only reshapes the card. LLPanel keeps `mBottom`, so a taller card grows **upward** over the card above it. The grid is never repacked and the doc height never changes.
- Triggered by:
  - More/Fewer options;
  - Show/Hide;
  - a selector option change;
  - the dyn-visibility probe;
  - `ALKitRow::setControlVisible`;
  - `revealRow`.
- §3.4 requires the probe's `requestLayout()` to coalesce "once per page and frame".
- Fix: the card's `requestLayout` forwards to the enclosing `ALKitPage::requestLayout()`. The page relayout already re-sizes every card through `heightForWidth`.

**B8. The legacy host is built 1x1, so every legacy page renders empty.**
- `alkitpage.cpp:78` creates `<P>_legacy_host` with `makePanel` (`LLRect(0,1,1,0)`, line 34). It then moves the v1 children, which keep their kit_page-relative rects, into it, and calls `reshape(w, top)`.
- `LLView::reshape` (`llview.cpp`) applies the delta (w−1, top−1) through the follows flags, so every top-following v1 child shifts up by about the page height and leaves the host.
- In the P0 artifact, 31 of 35 pages are legacy.
- Fix: create the host with `getLocalRect()` (the v1 leaf size) and FOLLOWS_ALL. Then reparent the children, then reshape to the content area, as §3.5 item 6 states.

**B9. Map drag in 2.5D (the default view) and in Side snaps on the first pixel.**
- `DragStart` has one `radius` field.
  - `beginDrag` sets it to the world radius r0 in metres (`mLightRadius/2.2` or `CineLightRigRadius`, `allightrigmap.cpp:88`).
  - `unproject25D` (`alkitmath.h:174`) and `dragSideAbsolute` (`alkitmath.h:204`) divide **pixel** offsets by it.
- With R_px about 100 and r0 about 2, |u| is about 50. The 2.5D inverse normalizes that to the silhouette rim, and the Side inverse clamps `asin` to ±90° (then ±85°).
- Zero motion is exact only through the `x==x0 && y==y0` shortcut. Any real drag is wrong.
- The tests hide this because they use one number for both quantities (tests 10, 11, 16 and the silhouette case).
- Fix:
  - Add a `canvas_radius` field used by the projection and unprojection.
  - Keep `radius` = r0 for `pDisp`/`baseFromDisplayPitch`.
  - Set both in `beginDrag`.
  - Add a TUT case with R_px = 120 and r0 = 2.3, using a pointer that differs from the start.

**B10. The row-pack layout and doc sizing misplace cards on every height change, and the scrollbar is permanent.**
- `ALFlowGrid::layoutRows` places cards (`setOrigin(..., -y-h)`) and only **then** calls `LLPanel::reshape(panel_width, final_height)` (`alflowgrid.cpp:272`). That reshape shifts every follows-top child by Δheight (cards and headings default to left|top, `llview.cpp:115`). The loop then adds `final_height` again, so every card ends up offset by Δh. The masonry path (`:183`) reshapes first for exactly this reason.
- `ALKitPage::relayoutNow` (`alkitpage.cpp:104`) sets the grid origin and then reshapes the doc, which shifts the grid by Δdoc.
- `layoutRows` floors the grid to the viewport (`alflowgrid.cpp:271`), and the doc is the grid plus `PAGE_PAD_T + PAGE_PAD_B`. The doc is therefore always at least 30 px taller than the viewport: a permanent scrollbar and dead scroll on short pages. That fails the audit and the look.
- Fix:
  - In `layoutRows`, run a sizing pass first, then reshape the grid, then place the children (as masonry does).
  - In `relayoutNow`, reshape the doc before setting the grid origin.
  - Floor the **doc** to the viewport, not the grid.

---

## MAJOR

**M1. Kit compass geometry is the arc geometry.**
- `alcompassdial.cpp:122-127` (`getDialGeometry`, `kit_geometry=true`) uses the arc centre `(6s, h/2)` for **compass** dials too.
- A full ring centred at x=6 with r=20 spans x from −14 to 26. The N/E/S/W letters fall outside the widget, over the label column. This affects every generated kit yaw dial (the Rig Key/Fill/Back/BG yaws).
- Spec §3.13 defines `(6s,h/2)` only for the half arc. Treat this as "stop and ask". My recommendation: kit compass centred at `(w/2,h/2)` with `r = min(20s, h/2-3s)`.
- Test 15 currently asserts the broken centre (`kit.cx == 6`) generically.

**M2. `effYaw` does not match `computeLive`.**
- `alkitmath.h:138` returns `wrap180((2F - base) + orbit)`.
- The model returns `wrap180(2F - (base + orbit))` (`alcinelightrigmodel.cpp:1057-1059`). The two differ by 2·orbit whenever Mirror is on.
- Tests 6 and 17 assert the wrong values: 65 and 131, where the model gives 55 and 121. §6.6 requires checking `effYaw` "against the computeLive formula".
- Fix the formula and both tests.

**M3. The rig-off fallback map is not camera-relative (U4).**
- Fallback `t = wrap180(base + orbit)` appears in `draw` (`allightrigmap.cpp:65`), in `beginDrag`'s `t0` (`:88`) and in the legend (`refreshLiveReadouts`, `:50`).
- None of them subtract φc, so with the rig off the map is world-relative under the "CAM/CAM L/CAM R" labels, and the R/L legend sides are wrong.
- Fix: `t = wrap180(base + orbit - φc)` in all three places. The sigma=+1 inverse is unaffected.

**M4. The grab offset is measured from the eased disc, not the target-frame disc.**
- `beginDrag` (`allightrigmap.cpp:88`) sets `grab = pointer - mScreenX[role]`, which is the **eased** lastFrame position. But `t0/p0` come from `mapTargetFrame()`, and the grabbed light is redrawn at the target.
- During a transition, the first pointer move therefore writes the remaining easing delta. This is the 0.9 s transition case on the P0 diagnostic list.
- Fix: `grab = pointer - projectLight(view, side, t0, p0, R_px, cx, cy)`.

**M5. Legend chips show the wrong colours.**
- `draw` sorts `lights[]` by depth (`allightrigmap.cpp:69`). The legend loop (`:73`) then uses `lights[i].color` for **role** i.
- Dot colours and the selected-chip border follow depth order. An OFF light's grey dot can land on an ON role's chip.
- Fix: keep a per-role colour array, or index by `l.i`.

**M6. The TUT tests do not test the spec claims.**
- The zero-motion, out-and-back and fixed-pointer tests (10, 11, 12 home, 16) are satisfied by the `x==x0 && y==y0` early return, or by repeating a pure call. Test 16's 1,000-sample loop discards every result (`(void)`).
- No test evaluates `drag25DAbsolute`/`dragSideAbsolute` at a pointer other than the start against a known (t,p).
- The 2.5D back branch is never exercised.
- All tolerances are 0 fraction bits (B2).
- §6.6 items 7-9 are unverified, which is how B9 got through.
- Required:
  - round-trip tests (choose base → pDisp/project → drag pointer → result equals base) for both 2.5D branches, both Side directions, Mirror ±, and separate R_px and r0;
  - out-and-back paths whose intermediate samples are asserted;
  - an `effYaw` case that reproduces `computeLive`.

**M7. `ALKitVecBinding` breaks vec2 and its defaults.**
- `commit()` (`alkitbinding.cpp:27`) writes a 2-element array into a Vector3 control. That zeroes z, but §6.1 says "vec2 preserves z unchanged".
- `isDefault` falls to exact `==` on size-mismatched arrays, so amber is permanently on.
- The spinners default to `min_value = max_value = 0` (`alkitrow.cpp:28`, from `min_val/max_val` defaults of 0). Any edit clamps to 0 unless the generator always emits a range.
- Fix:
  - Read the control, replace the first N components, and write back 3.
  - Compare only N components.
  - Do not set min/max when the range is not provided.

**M8. The value column is not driven by the control.**
- The toggle `<w>_state` and dial `<w>_value` text is updated only in `bindingChanged` (`alkitrow.cpp:71`).
  - Code-bound toggles and dials with no binding never get text.
  - The row never subscribes to `ALKitSwitch::setFlipCallback`; §3.3 says "updated on flip".
  - `WidgetBinding` rows update only on commit, not on C++ `setValue`.
- The format `"%.*f°"` with the default `decimals=3` gives "45.000°"; the spec shows "45°".
- Fix:
  - Toggle: connect the flip signal.
  - Dial: connect the dial commit.
  - Update `<w>_value` from `setValue`, or from a 10 Hz diff-guarded readout.
  - Use the dial `decimal_digits`, or 0.

**M9. Mode (Easy/Advanced) state is unreliable.**
- (a) There is no initial sync. `mModeIndex` starts at 0 whatever `cine_easy_mode` is (`alkitpage.cpp:64`).
- (b) The flip callback (`:74`) derives the index from the `modes="A|B"` token order, but `mModeIndex` indexes the `<kit_page.mode>` order.
- (c) `setModeIndex` (`:91`) stores the **requested** index even when the adapter returns a different effective value. A refusal that produces no switch edge leaves the page on the wrong card set. This contradicts §3.5 and the R2 M-R2 effective-mode rule.
- Fix: compute the mode only from the effective `isOn()` by name lookup in `mModeDefs`. Call it at postBuild, on flip, and after the adapter returns.

**M10. The binding registry (reset path 3) cannot be reached.**
- Rows resolve their binding once, in their own `postBuild` (`alkitrow.cpp:42`), keyed by the `ALKitPage*` pointer.
- Under D3 the owner logic panel's `postBuild` runs **after** the kit_page's, and the page pointer does not exist before the build. No owner can ever register in time.
- Fix: rows resolve bindings lazily. Use `ALKitPage::rebindRows()` after `pageBuiltSignal`/owner registration, or make `ALKitBindingRegistry::add` push into the live rows of that page.

**M11. Reset gestures pass a label where the API expects a page key.**
- `alkitcard.cpp:39` calls `beginGesture(MOUSE, "Reset " + mTitle)`; `alkitpage.cpp:112` calls `beginGesture(MOUSE, "Reset page")`.
- The second parameter is the page key (§3.11 "the setting is watched for the gesture's page"). Once WP2 implements the watch filter, card and page resets will record nothing and cannot be undone.
- Fix: pass `page->pageKey()`, and carry the label separately.

**M12. `chip_mode=auto` and chip hiding are wrong.**
- `applyModeAndChip` (`alkitpage.cpp:92-94`) filters only when the current `<mode>` lists 6 or more cards. A page with no modes, which is most pages, never filters even with 20 cards.
- §3.5 says "count of cards visible in the current mode ≥ 6".
- Chips of sections with no card in the current mode are not hidden.
- The chip and reveal scroll use grid coordinates (off by PAGE_PAD) taken before the deferred layout.

**M13. `adoptPage` guesses names from the content root.**
- `alscrollfit.cpp:463-482` looks for `content->getName()+"_scroll"` and `"<name>_legacy_host"`. The kit prefix P comes from the page or tab name (§3.5), and the page-file root is a logic panel (D3) whose name need not equal P.
- On a miss, a kit page falls into the legacy branch with `host = content` (`:482`). `wrapTabPanel` then wraps the whole kit page in a second scroller.
- Fix: derive P from `tab_page->getName()`, or find the `ALKitPage` and branch on `legacy()`. Never fall back to `host = content`.

**M14. The selector "Reset all" context menu is a child of the card.**
- `alkitcard.cpp:40` builds the LLContextMenu and `addChild`s it to the card. `LLMenuGL::showPopup` positions relative to `menu->getParent()`.
- The card's custom draw then draws it dimmed and clipped by the scroller, and it is not in `gMenuHolder` for outside-click dismissal.
- Fix: create it parented to `gMenuHolder` (existing pattern) and delete it in the card destructor.

**M15. ALCompassDial v1 behaviour changes beyond the wheel guard.** This violates the WP1 acceptance criterion "behave the same, apart from the wheel guard" (`alcompassdial.cpp:29-36, 100-109, 141, 90, 216-232`).
- Wrap (−180,180] becomes [−180,180), so dragging to south now commits −180 instead of 180.
- The dead zone grows from 0.5 px to 3 px.
- `setTabStop(true)` on every legacy dial changes tab order in Actor Mover and Ghost Studio.
- Arrow, PgUp/PgDn and Home now act on focused **unbound** v1 dials. Home jumps them to `min`, i.e. −180.
- Fix:
  - Gate the dead zone, tab stop and keys on `kit_geometry` (or an explicit opt-in).
  - Keep legacy wrapping when `min/max` are not provided.
  - Make Home a no-op when unbound.

---

## MINOR

1. `alcinelightrig.cpp:2672-2675` runs `computeLive + render` for **every rig, every tick**, even with V2 off and no map. `++mMapTargetRevision` every tick makes the revision useless as a change signal. The spec asks for an accessor that "allocates and mutates nothing". Fix: compute on demand from cached inputs, or bump the revision only on change.
2. Spec names and structure missing (§3.5):
   - `<P>_chips` and `<P>_who` containers, the who hint text;
   - `<P>_master_box` (label plus state text);
   - the banner's KitBannerBg fill;
   - the legacy summary text.
   The who chip calls `showList()` on a hidden combo (`alkitpage.cpp:77`), which shows nothing. A cast-selection failure overwrites the chip label, and the 2 Hz timer reverts it.
3. Card header (§3.4): the title is not bold and not ellipsized, and the badge sits at a fixed x, so long titles collide. The `Kit_RoundRect*` 9-slices are never used; card corners are square.
4. `textures.xml:1052-1053`: `scale.top="6" scale.bottom="10"` is inverted (LL y is up; compare `Caret_Top_Icon`). It should be top=10, bottom=6. This is latent, because nothing draws the textures yet.
5. Map, Top view: `gl_circle_2d` for the floor ring runs with the stale GL colour from the background `gl_rect_2d`, so the ring is invisible. Set `gGL.color4fv(track)`.
6. Map: the side-chip tooltip is set on the whole widget, so it is wrong in Top and 2.5D.
   - `'['`/`']'` cannot reach `handleKeyHere`: printable keys go through the Unicode path (§3.12). Implement `handleUnicodeCharHere`.
   - The OFF slash length is 2.04·rr, where the spec says 1.44·rr.
   - The near half of the globe shell is not drawn.
7. `alkitmath.h:116`: `frontFactor` returns 1 for Top. The spec uses `fr = smoothstep(cos t)` for size and alpha in Top too; only the layer is forced.
8. Dyn-visibility probe (`alkitcard.cpp:70`): `isEffectivelyVisible` (`alkitrow.cpp:69`) calls `card()` (a dynamic_cast parent walk) twice per row per frame. The spec says "reads only member bools". Cache the card pointer.
9. `ALKitPage::draw` (`alkitpage.cpp:113`) walks parents with `ALKitShell::find` every frame, and `updateMaster` does raw `setVisible`/`setDimmed` from draw. Use `setVisibleIfChanged`; this is a lint finding. `relayoutNow` reads `gSavedSettings.getS32("UIScrollbarSize")` by name; use LLCachedControl.
10. `ALKitModeAdapters` (`alkitbinding.cpp:40`) is keyed by `"%p"` with no liveness check or pruning, so entries go stale on address reuse.
11. `ALKitRow` `bind` defaults to `"code"` (`alkitrow.cpp`, Params). A setting-bound row without an explicit `bind` gets a widget-history hook as well as the setting watch. Default to empty and require `bind`.
12. `computeShellLayout` (`alkitmath.h:217`): `track` ignores the card padding (20) and the scrollbar. `packRows` is not the code ALFlowGrid runs, and `dialGeometry` duplicates the llui dial maths. The tests check copies, not the shipped code.
13. Top drag recomputes pitch through `baseFromDisplayPitch` (float drift of about 1e-5). Return `base_pitch0` exactly when `pTarget == p0`. `dragSideAbsolute` lacks the spec's depth tie-break.
14. `ALKitWidgetBinding` amber updates only on commit. Row `mLastValue` is not refreshed on reveal (§3.3).
15. `ALKitBinding::isDefault` on `ALKitSettingsListBinding`'s map uses exact deep `==`, so float noise produces false amber. Compare per entry with the row precision.
16. A launcher card that is also `advanced` creates two `kc_<id>_open` buttons (spec naming collision; flag it to the spec owner).
17. `ALFlowGrid` dropped `final` without a need (§3.15 says remove it only if required). `ALKitLazyPage` retries a failed `createFromFile` on every visibility change, which spams the log.
18. `WP1_STATUS.md` reports "16 done, 0 partial". That overstates the state, given the items above. SPIKES S2/S9 deferred the runtime proof, and B3/B4 are exactly the cases those spikes were meant to catch.

---

## Checklist results

- **1 Compile:** FAIL. See B1 (runtime registration crash) and B2 (8 sites plus the test). All overrides now match the base virtuals:
  - `LLCheckBoxCtrl` reshape, setValue, setControlName and onCommit;
  - `LLView` onMouseEnter/onMouseLeave and onVisibilityChange;
  - `LLScrollContainer::handleKeyHere`.

  The Params and Multiple blocks, the programmatic Params and the context-menu Params compile once B2 is fixed. The CMake entries match the files on disk. `alkitmath` is registered like `alxinputmath`, and its includes are limited to linden_common.h, lltut.h and alkitmath.h.
- **2 v1 safety:** FAIL.
  - B1 crashes v1.
  - M15 changes the dial behaviour.
  - Masonry is byte-for-byte unchanged apart from the early branch (OK).
  - The ALScrollFit v1 change is only the SelfSizedDocument early return (OK).
  - The CineLightRig object-front facing matches the avatar path (OK), but it adds per-tick cost (m1).
- **3 Draw cost:** mostly OK. Tracy zones are present in Switch, Row, Card, Page, Doc, SectionHeading, Map and Dial. The 10 Hz map callback and the 2 Hz who callback are handle-guarded; they keep polling while hidden, which is cheap. See m8 and m9.
- **4 Maths:** FAIL.
  - B9 (unit conflation), M2 (`effYaw`), M3 (fallback not camera-relative), M4 (grab frame), M6 (tests prove nothing).
  - Verified correct: `pDisp`, the `baseFromDisplayPitch` branch algebra (`sin(q−T) = −h·cosT/r`), `projectLight` for 2.5D/Top/Side, the 2.5D inverse rotation, the Side cosine inverse, `finishDrag`'s sigma, and the object azimuth from `getRenderRotation`.
- **5 Lifetime:** OK.
  - Every idle and periodic callback checks an LLHandle, and setting and flip connections are scoped.
  - One unscoped connection: the row's history-hook lambda on `mControl`'s commit signal is never disconnected. That matters once card-preset controls are reparented to the card.
  - Static adapter maps: see m10.
- **6 Spec conformance:** tags and Params match §3. Deviations: B5, B6, M8, M9, M12, m2, m3, m11.
