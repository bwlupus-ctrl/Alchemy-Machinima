# Director functional self-test: harness v2 design

Status: DESIGN (no code changed). Repo `I:\alchemy-machinima`, branch `feature/director-kit-p0`, HEAD `b768cd51543`.
Input run: `doc/director_kit/run_functional_20261011_093636/` (PASS 164 / FAIL 2897 / SKIP 500).
Every claim below cites `file:line` in `indra/llui`, `indra/newview` or `scripts/`.

## 0. What the first run actually says

The headline numbers hide the real result. Of the 164 PASS rows, 161 are `EXPECT_MISSING`
("expected missing control"), 2 are the `rig_shutdown_restore` state rows and 1 is the v2
`keyboard_tab_focus` row. **Zero interactive controls passed on either UI version.** v1 is known
to work, so the harness, not the product, is broken, and it is broken by one dominant defect
plus five smaller ones:

| Fail reason (count) | Root cause | Kind | Section |
|---|---:|---|---|
| `not reachable: covered` (1080) | `root->childFromPoint(x,y,true)` returns the first *visible* child containing the point regardless of `mouse_opaque` or event handling (`llview.cpp:922-952`). `main_view.xml` declares several full-window, `mouse_opaque="false"` holders *after* (= in front of, `llview.cpp:355` push_front) the `Floater View`: `toolbar_view_holder` (:95-103), `snapshot_floater_view_holder` (:107-113), `popup_holder` (:125-131), `hint_holder` (:140-146), `Menu Holder` (:156-161), `tooltip view` (:162-168). `childFromPoint` stops at the first of these and returns it as the leaf (`llview.cpp:946`); the real dispatcher `childrenHandleMouseEvent` falls through them because they neither handle nor `blockMouseEvent` (`llview.cpp:817-834`, `:1784-1787`). Every visible, enabled control therefore reads as "covered". | harness | §1 |
| `launcher did not open its target floater/panel` (294) | Same defect: the launcher button is "covered" in `openFunctionalRedirect` (`aluiperfselftest.cpp:1860`) and the reachability reason is discarded (`:2436`). | harness | §1, §4 |
| `light_rim gate Turn on is not user-clickable` (2), v1 `weather_enabled` covered (1) | Same defect on the gate button / `cine_enabled`; reason discarded at `aluiperfselftest.cpp:2370-2374`. | harness | §7 |
| `no observable effect` (371) | All 371 have `observed="control not found"` (`aluiperfselftest.cpp:2452-2454`): the manifest page model puts controls on v1 pages that do not exist in v1 (`light_rim_tab`, `tron_tab`, `bloom_glow_tab` are absent from `floater_director.xml:314-1756`) or that live in `floater_lightbox_settings.xml` / `floater_ultimate_diopter.xml` behind a launcher card that v1 also has (`floater_director.xml:1450,1461,1706,1717`) but which is only tagged `v2_launcher_redirect` for v2 (`director_functional_manifest.py:540-541`). The reason string is wrong: this is a lookup failure, not an effect failure. | manifest/page model | §4, §5 |
| `page did not open; absence is unproven` (12) | Same page-model gap (v1 `bloom_glow_tab`, `tron_tab`). | manifest | §4 |
| `not reachable: hidden` (776) | Four reveal gaps: (a) kit reveal does not call `ALKitPage::revealCard` for rows inside cards (`aluiperfselftest.cpp:2193-2194`) so the section *chip* filter keeps the card hidden (`alkitpage.cpp:879,896-899`); (b) kit reveal is deferred to the idle queue (`alkitcard.cpp:822-844`, `alkitpage.cpp:1110-1119`, `doOnIdleOneTime` -> `gIdleCallbacks`, `llcallbacklist.cpp:145-148`) but the harness tests `isInVisibleChain()` in the same tick; idle callbacks only run at the start of the *next* tick (`llstartup.cpp:457-458`); (c) `revealFunctionalAncestors` only handles `LLTabContainer` (`aluiperfselftest.cpp:1777-1782`), not `LLAccordionCtrlTab` (`panel_path_editor.xml:177 expanded="false"`); (d) owner-code selector groups: `ALPanelCineCamParams::updateModePanel` hides 48 of 49 mode panels (`alpanelcinecamparams.cpp:520-533`, 235 rows per version) and `ALPanelWeatherSettings::updateControlGroup` hides 3 of 4 groups (`alpanelweathersettings.cpp:171-189`, 36 per version). | harness | §2 |
| `not reachable: disabled-by-parent` (361) | Mixed: owner refresh gates on world state (legit DEFER, e.g. `alpanelpatheditor.cpp:766-783`, `alpanelghoststudio.cpp:1396-1464`) and setting-only gates the manifest misses (e.g. `CineLightRigCatchlight` for `cine_catchlight_actor_only`; the R4 regex only knows Key/Fill/Rim/Bg, `functional_report.py:259-265`). | mixed | §3 |

Design goals, restated against this evidence:

* (a) **Green on a correct build**: fix the six causes above; the budget in §8 makes "green" mean
  "0 FAIL *and* a minimum PASS count per interactive kind", so a degenerate run like this one can
  never read as healthy again.
* (b) **Still FAILs every 10/10 class on old code**: the old-code assertions of R2-R5
  (`SELFTEST_R2.md`) are kept; they get *stronger* because the click now travels the real path
  (`LLButton::handleMouseDown/Up`, `llbutton.cpp:496-565`), so a switch that only reacts to
  programmatic `setValue` cannot pass.
* (c) **Diagnosable**: every FAIL row carries a JSON `observed` with the handler path, the
  visibility/enabled chain break point, rects, before/after values and the consumer probe (§6).

## 1. Reachability and click are one operation

### 1.1 Replace the hit test with the real dispatcher

Delete the `childFromPoint` probe (`aluiperfselftest.cpp:2160-2171`) and the separate
`mFunctionalRoot->handleMouseDown/Up` call (`:2205-2213`). Inject the click where the OS event
enters:

```
LLCoordGL gl(ll_round(ui_x * scale.mV[VX]), ll_round(ui_y * scale.mV[VY]));   // undo :1136-1137
bool toolmgr = false;
gViewerWindow->handleAnyMouseClick(gViewerWindow->getWindow(), gl, MASK_NONE, CLICK_LEFT, true,  toolmgr);
gViewerWindow->handleAnyMouseClick(gViewerWindow->getWindow(), gl, MASK_NONE, CLICK_LEFT, false, toolmgr);
```

* `LLViewerWindow::handleAnyMouseClick` is public (`llviewerwindow.h:162,208`) and is exactly
  what `LLViewerWindow::handleMouseDown` reaches through `gViewerInput.handleMouse`
  (`llviewerwindow.cpp:1315-1330`, `llviewerinput.cpp:1800-1803`). It honours, in order: the
  display scale (`:1136-1137`), the UI-visible feature mask (`:1149`), the mouse captor
  (`:1217-1247`, `llfocusmgr.cpp:377-402`), `mRootView->pointInView` (`:1250`), then root dispatch
  (`:1259`) via `LLMouseHandler::handleAnyMouseClick` (`llmousehandler.cpp:32-57`) ->
  `LLView::handleMouseDown` -> `childrenHandleMouseEvent` (`llview.cpp:810-837`), which applies
  `visibleEnabledAndContains` (`:772-776`) and `blockMouseEvent` (`:1784-1787`). Popups (combo
  lists) are reached through `popup_holder` -> `LLPopupView::handleMouseDown`
  (`llpopupview.cpp:139-147`), floaters are brought to front by `LLFloater::handleMouseDown`
  (`llfloater.cpp:1872`), and the tool manager fallback (`:1291-1301`) is harmless at login
  because `gDisconnected` is false but no tool consumes UI clicks.
* Use `gViewerInput.handleMouse(...)` instead if the double-click bookkeeping
  (`llviewerinput.cpp:1805-1845`) is wanted; it adds nothing for a single click, so prefer the
  window call and keep `mMouseDownTimer` untouched.
* Set `LLViewerWindow`'s current mouse point first (`handleMouseMove` / hover) so widgets that
  consult `gViewerWindow->getCurrentMouse()` or receive `handleHover` with capture (sliders,
  `llslider.cpp:310-340`) see a consistent position.

### 1.2 Which view handled it

Two independent witnesses, both recorded into `observed`:

1. **Handler chain.** `LLView::sDebugMouseHandling` is a plain public static (`llview.h:708-709`);
   when true, every handling level prepends its name into `sMouseHandlerMessage`
   (`llview.cpp:779-785`, called from `:831`, `:856`, `:915`). `handleAnyMouseClick` clears it
   (`llviewerwindow.cpp:1189`). Set the flag around the injected down and up, then compare the
   resulting `/a/b/.../leaf` string with `control->getPathname()` (`llview.cpp:480-483`): the leaf
   path must have the control's path as a prefix (control itself or a descendant). The string is
   name-based, so also add the exact check in 2.
2. **Recorder hook (10 lines in llui).** Add
   `static std::function<void(LLView*)> LLView::sMouseHandlerRecorder;` invoked next to
   `viewp->logMouseEvent()` in `childrenHandleMouseEvent` (`llview.cpp:831`) and in
   `childrenHandleToolTip/Hover` for symmetry. The harness installs a lambda that stores the
   *first* recorded view (the deepest, since the deepest level returns first) and asserts
   `hit == control || hit->hasAncestor(control)`. This is exact (pointer identity), survives
   duplicate names, and costs nothing when the function is empty.
3. **Capture witness.** After the down event, `gFocusMgr.getMouseCapture()` (`llfocusmgr.h:90`)
   must be the control or a descendant for buttons (`llbutton.cpp:503`), sliders
   (`llslider.cpp` down sets capture), spinner arrows and kit switches (`ALKitSwitch` is an
   `LLCheckBoxCtrl`; its `mButton` captures). After the up event capture must be `NULL`
   (`llbutton.cpp:550`, `llslider.cpp:294`); the existing sticky-press assertion
   (`aluiperfselftest.cpp:2686-2689`) keeps that.

"Reachable" is therefore defined as: *the down event's deepest handler is the control or one of
its descendants*. There is no longer a separate geometric probe to disagree with the dispatcher.

### 1.3 Where to click, per widget kind

The click point is computed from the **child that must handle it**, not from the control's rect
centre (the centre of a wide `ALKitSwitch` is empty: its button is pinned to
`LLRect(0, SWITCH_H, SWITCH_W, 0)` and the label is hidden, `alkitswitch.cpp:16-21,34-37`).

| Kind | Target child / point | Handler expected | Notes |
|---|---|---|---|
| check_box / kit_switch / radio item | `LLCheckBoxCtrl::mButton` centre. The stock button covers label+box (`llcheckboxctrl.cpp:130-136`), so a *second* probe on the label centre is a cheap extra assertion for legacy check boxes; for `ALKitSwitch` only the button rect is valid. Radio: the specific `LLRadioCtrl` child (`llradiogroup.cpp:473-493`). | the button | toggle oracle: `isOn()` == control value == setting (kept from R3) |
| button | own rect centre | the button | `LLButton::handleMouseUp` requires capture + `pointInView` (`llbutton.cpp:543-565`) |
| spinner | the upper of the two `LLButton` children (`llspinctrl.cpp:103-120`) for `+increment`, the lower for `-increment`; the `LLLineEditor` child for typed values (click, select-all, type, Return via `gViewerWindow->handleKey/handleUnicodeChar`). | the arrow button / the editor | Replace the manifest's exact `test_value` oracle with: value moved by exactly one `increment` in the clicked direction and the bound setting equals the displayed value (epsilon rule from R2). Typed path still verifies the exact `test_value`. |
| slider (`LLSliderCtrl`) | the `LLSlider` child: compute `x` on the track for `test_value` from the slider's rect and thumb width; a click off the thumb jumps there (`llslider.cpp:327-340` sets `mMouseOffset=0` and the hover path commits). Fallback when geometry is private: click the track at 25 %/75 %, then assert monotone movement and `setting == slider value`. | the `LLSlider` | also send one `handleHover` with capture so the drag path is exercised |
| combo | 1) click the combo's button (`llcombobox.cpp:110,795`) -> `showList` registers a popup (`:675-768`, `LLUI::addPopup` at `:764`); 2) assert `gFocusMgr`/popup list visible; 3) click the wanted `LLScrollListCtrl` item rect through the same window dispatch (routed by `LLPopupView::handleMouseEvent`, `llpopupview.cpp:97-136`). | step 1: the combo button; step 3: the list (descendant of the combo, `:142-143`) | `onItemSelected` commits and hides the list |
| color swatch | click centre; assert a picker floater opened (floater-set diff already exists, `aluiperfselftest.cpp:2655-2670`); then commit through the picker's own controls or, as today, programmatic `setValue+onCommit` tagged `programmatic` in `observed`. | the swatch | |
| line editor / text | click, `ctrl+a`, type, Return | the editor | |
| tab / accordion header / card header | click the header button | the header | used by the reveal layer (§2), not as test rows |

A **sentinel row** runs first in every process: click the Director floater's first tab button
and a kit search box, assert the handler path. If the injection channel itself is broken
(scale, UI feature mask, a modal), the whole run FAILs loudly on one row instead of 1080.

### 1.4 Preconditions the dispatcher imposes (and the harness must satisfy)

* No mouse captor left over from the previous row (`gFocusMgr.getMouseCapture()==NULL`), no
  top control / popup open (`gFocusMgr.getTopCtrl()`, `llfocusmgr.h:119-121`): close popups by
  clicking elsewhere or `LLUI::clearPopups`; the current per-row floater diff (`:2104-2111`)
  already closes stray floaters.
* The Director floater must be frontmost among floaters at the point: `gFloaterView` dispatch
  picks the first containing floater in child order; call
  `floater->setFrontmost(true)`/`bringToFront` before the row and assert
  `gFloaterView->getFrontmost()==floater` in `observed` (see §2.4 for the on-screen rule).
* `gPipeline.hasRenderDebugFeatureMask(RENDER_DEBUG_FEATURE_UI)` must be true
  (`llviewerwindow.cpp:1149`); assert once at start.

## 2. Reveal: make the control visible and on screen, then prove it

### 2.1 Reveal is a state machine, not a function call

Today `revealFunctionalControl` runs synchronously and `functionalReachable` tests
`isInVisibleChain()` in the same tick (`aluiperfselftest.cpp:2178-2203`, `:2133-2159`). Kit
widgets apply visibility in deferred layout passes: `ALKitCard::revealRow` only sets
`mBodyOpen/mActiveOption/mMoreOpen` and calls `requestLayout()` (`alkitcard.cpp:846-872`);
`requestLayout` queues `doOnIdleOneTime` (`:822-844`), which forwards to `ALKitPage::requestLayout`
(`alkitpage.cpp:1110-1119`); rows are shown/hidden in `layoutBody` (`alkitcard.cpp:622-690,
766-793`). `ALKitCard::draw` re-evaluates row visibility only when the card is on screen
(`:952-965`). The idle queue is drained at the top of the next tick, *before* the harness runs
(`llstartup.cpp:457` then `:458`). Hence the v2 `cine_light_rig_tab` 118 and `light_rim_tab` 36
"hidden" rows.

v2 reveal therefore becomes a per-row phase sequence driven by `functionalStep`:

```
REVEAL_APPLY   -> run every reveal action (below) that applies; record each in observed.reveal[]
REVEAL_SETTLE  -> return false (yield frame) until: idle queue drained (count ticks >= 1),
                  page/card report no pending layout (expose ALKitPage/ALKitCard::layoutPending()),
                  page draw counter advanced since REVEAL_APPLY, and the control's
                  isInVisibleChain()==true; bounded to 6 frames, else FAIL "reveal did not settle"
                  with the visibility-chain break point (§6)
REVEAL_VERIFY  -> on-screen check (2.4), then §1 reachability+click
```

### 2.2 Reveal actions, generic and ordered from the control upward

Walk `control -> parent -> ... -> floater` once and collect every container that gates
visibility; apply in root-to-leaf order:

| Ancestor type | Action | Code |
|---|---|---|
| `LLTabContainer` child panel | `selectTabPanel(panel)` | `lltabcontainer.cpp:1511`; already done at `aluiperfselftest.cpp:1777-1782` |
| `LLAccordionCtrlTab` | `setDisplayChildren(true)` / `changeOpenClose(true)` | `llaccordionctrltab.cpp:402,451`; **missing today** (path_tab 26x2) |
| `ALKitLazyPage` | `ensureBuilt()` then wait for `fitContent` (same tick) | `alkitlazypage.cpp:28-73` |
| `ALKitPage` | `revealCard(card)` (mode + section chip) **and** `card->revealRow(row)` (body, option, More) | `alkitpage.cpp:1212-1234`, `alkitcard.cpp:846-872`; today only one of the two runs (`aluiperfselftest.cpp:2193-2194`) |
| `ALKitPage` search filter | clear `setSearchText("")` if non-empty | `alkitpage.cpp:1239` |
| panel hidden by a sibling *selector* | **selector sweep** (2.3) | |
| `LLScrollContainer` | `scrollToShowRect(document_rect)` after layout settled, re-run after every layout change | `llscrollcontainer.cpp:794`; today runs before layout (`aluiperfselftest.cpp:2145-2154`) |
| `LLFloater` | open, un-minimize, `setFrontmost`, fit on screen (2.4) | `llfloater.cpp:1853-1878` |

Important: `ALKitPage::setModeIndex` is **not side-effect free**: with a mode switch it writes the
bound setting (`alkitpage.cpp:826 writeBoundValue`) or runs the mode adapter (`:813-821`). The
current reveal loop cycles every mode for any hidden control (`aluiperfselftest.cpp:2195-2201`)
and can flip page masters (e.g. Easy/Advanced on the rig page). v2 must call `revealCard`, which
selects only the mode that lists the card (`alkitpage.cpp:1220-1227`), snapshot the mode-bound
setting before and restore it with the row.

### 2.3 Selector sweep for owner-code auto-hide groups

Legacy panels hide siblings from C++: `ALPanelCineCamParams::updateModePanel` shows one of 49
per-mode panels keyed by `CinematicCamMode` (`alpanelcinecamparams.cpp:520-533`; 235 rows per
version) and `ALPanelWeatherSettings::updateControlGroup` shows one of four groups keyed by the
`weather_control_group` combo (`alpanelweathersettings.cpp:132-134,171-189`; 36 per version).
Neither is discoverable from XUI attributes.

Rule: when the nearest hidden ancestor `H` has visible siblings and the page contains a visible
combo/radio/tab/mode control `S` whose commit precedes the hide (either declared in the manifest
as `reveal.selector` or found by the sweep), iterate `S`'s items through the real click path
(§1.3 combo recipe), settle one frame each, stop when `H` becomes visible. Bound: items <= 64,
total frames <= 2x items. Record `{selector, item}` in `observed.reveal`. The manifest generator
should pre-compute the mapping where it is static:

* `panel_cinecam_params.xml`: parse `modeTable()`-style entries (`alpanelcinecamparams.cpp:282,
  523-531`) or simply map each mode panel name to the `CinematicCamMode` value; emit
  `reveal: [{"selector":"CinematicCamMode","value":N}]`.
* `panel_weather_settings.xml:34-38` comments name the groups; emit
  `{"selector":"weather_control_group","value":"rain"|"surface"|"occlusion"|"lightning"}`.

Restore the selector's pre-row value with the row (it is a setting for `CinematicCamMode`, a
widget value for `weather_control_group`).

### 2.4 Floater size, position, front

The login-screen window is the viewer window; the Director floater may be larger than the
viewport or partly off screen. Before the first row of each page: `floater->setShape` to at most
`gFloaterView->getRect()` (keep the original rect for `abortForLogin`, `:1317-1322`),
`gFloaterView->adjustToFitScreen(floater, false)`, `floater->setFrontmost(true)`,
`setMinimized(false)`. Per row, after scrolling: the control's screen rect intersected with the
scroll container's *viewport* rect and the floater rect must contain the click point; otherwise
scroll again, and if still outside report `FAIL not on screen` with the three rects in `observed`.
This is the only geometric check kept, and it is a *pre*-condition, not the reachability verdict.

### 2.5 Legitimately hidden vs should-be-visible

After the full reveal sequence settles, a still-hidden control is classified by the owner of the
nearest hidden ancestor (`observed.visible_break`):

1. Hidden by a kit mode/section/body/More/option, a tab, an accordion or a selector group ->
   always **FAIL (harness)**: these are user-reachable states.
2. Hidden by owner code gated on a *setting* (e.g. `weather_deferred_note visible="false"`
   shown only when deferred is off, `panel_weather_settings.xml:10-14`,
   `alpanelweathersettings.cpp:219`) -> establish the setting as a prerequisite (§3) and retry
   once; still hidden -> FAIL.
3. Hidden by owner code gated on *world state* (readouts such as `cine_shadow_hint`,
   `cine_shadow_fixit` "shown only when it matters", `panel_dpage_light_cine_light_rig.xml:541-546`)
   -> **DEFER_INWORLD** only if the manifest row carries `visible_when` naming the predicate
   (generator extracts the `setVisible(...)` expression next to the widget name, same technique
   as `functional_report.py:333-345`). No predicate recorded -> FAIL. Hidden must never be silent.

Applied to the two cases asked about:

* **v2 catchlight rows** (`cine_catchlight_ev/size/angle/actor_only`, card `kc_rig_catchlight`,
  `panel_dpage_light_cine_light_rig.xml:562-575`). The card is listed only in the *Advanced* mode
  (`:6`) and in a section chip; `enable_setting="CineLightRigCatchlight"` does **not** hide rows,
  it only dims them in `ALKitCard::draw` (`alkitcard.cpp:974-978`). They are hidden because (a)
  the row reveal never calls `revealCard`, so the chip filter (`alkitpage.cpp:879,896-899`,
  `filtered` is true whenever the chip mode is `filter`, or `auto` with >= 6 visible cards, which this 15-card Advanced mode satisfies) keeps the card invisible, and (b) the visibility check
  runs before the deferred layout. Class 1 -> harness bug, not a DEFER. Expected after fix: RUN
  and PASS at login (settings-only consumer `catchlight_enabled`).
* **v1 `path_tab` `node_*` spinners** (`panel_path_editor.xml:181-199`). Hidden because the
  accordion tab `node_camera_tab` is `expanded="false"` (`:177`) and the reveal does not open
  accordions -> class 1, harness. Once open they will be *disabled*:
  `ALPanelPathEditor::refreshInspector` enables them only when `path && listSelectedNode()>=0`
  (`alpanelpatheditor.cpp:766-777,783`), i.e. an actor path with a selected node -> §3 class
  "world-gated" -> `DEFER_INWORLD: enable_when=ALPanelPathEditor::refreshInspector(have node)`.
  `node_cam_transition_combo` and `path_camera_enable_check` (setting-bound) are not in that
  gate and must RUN.

## 3. Disabled-by-parent

### 3.1 Find the break point, then its owner

Runtime: walk up from the control until `getEnabled()` is false (`LLView::isInEnabledChain`,
`llview.cpp:429`); record that view's path as `observed.enabled_break`. Then look for the enabling
condition in this order:

1. `LLUICtrl::getEnabledControlVariable()` on the break view or any ancestor (already walked,
   `aluiperfselftest.cpp:1890-1900`) -> set it, settle one frame.
2. Manifest `enabled_control` prerequisites (`functional_report.py:230-273`): XUI
   `enabled_control/disabled_control/enable_setting/master_setting` attributes, the
   `owner_rules` families (`:171-178`), and the per-light `CineLightRig(Key|Fill|Rim|Bg)On` gate
   (`:259-265`). Extend the per-light regex with `Catchlight` -> `CineLightRigCatchlight` (fixes
   v1 `cine_catchlight_actor_only`) and `Bounce` -> `CineLightRigBounceEnabled`.
3. **Static owner-refresh parse** (new in the generator): for each `setEnabledIfChanged(mX, expr)`
   / `mX->setEnabled(expr)` in the owner `.cpp`, resolve `mX` to its widget name through the
   `mX = getChild<T>("name")` assignment (`alpanelpatheditor.cpp:99`), tokenize `expr`:
   * setting tokens (`getBOOL("S")`, `LLCachedControl` names, `gSavedSettings.getS32`) -> add
     `{setting, value}` prerequisites (value chosen so the boolean sub-expression is true; for
     `!x` use false);
   * world tokens (`isAgentAvatarValid`, `gAgentAvatarp`, `getSelection`, `mActor`, `path`,
     `entity`, `have`, `inst`, `selected`, `crowd_active`) -> mark the row
     `enable_when="<Class::method>: <expr>"` and `DEFER_INWORLD` at login.
   This replaces the 800/1600-character text window heuristic (`functional_report.py:333-345`),
   which is why 24 switcher gates are "unresolved" today.
4. **Runtime bounded probe** (fallback when 1-3 produce nothing): collect Boolean switches/check
   boxes in the same card/panel as the break view (nearest first, max 3), toggle each through the
   real click path, settle a frame, re-test `isInEnabledChain()`; undo every toggle with the row.
   Record `observed.enable_probe=[{widget, result}]`.

### 3.2 What stays a legitimate DEFER at the login screen

Only rows whose enable expression contains a world token and no satisfiable setting path, with
the expression recorded. From the run: ghosts (`alpanelghoststudio.cpp:1396-1464`, `have`/
`entity`/`overlay`), props (`panel_prop_mover.xml`, object selection), path node inspector
(`alpanelpatheditor.cpp:766-783`), switcher `custom_angle` spinners gated on
`switcher_custom_enabled` (setting-like, probably *satisfiable*: check `panel_director_switcher.xml:174-276`
and its owner before deferring), actor_style / avatar light response (`isAgentAvatarValid`),
`gaze_tab` 61 rows (owner `ALPanelLensGaze`, pattern `setEnabledIfChanged(mSlotYou,
isAgentAvatarValid())`, `alpanellensgaze.cpp:1125`; the other 60 need the parse in 3.1.3 to
split setting- from avatar-gated). A DEFER row must still pass **reveal** (visible, on screen) so
that the UI is proven to exist; only the click is skipped.

## 4. Launchers and the page model

### 4.1 One host table instead of two hard-coded lists

Today the host of a control is implied by `page` and patched by two literal maps
(`functionalRedirectFloater` `aluiperfselftest.cpp:1823-1831`, `functionalLauncherName`
`:1833-1847`, and `launchers` in `functional_report.py:216-222`). The generator should emit a
per-(version, page) **host record** and every row inherits it:

```
"host": {"floater":"director", "page_view":"cine_light_rig_tab", "via":"tab"}                 // v1 direct
"host": {"floater":"lightbox", "tab":"rotoink_settings", "via":"launcher",
         "launcher":{"floater":"director","page_view":"stylize_tab","widget":"card_rotoink_open"}} // v1 and v2
"host": {"floater":"director", "page_view":"cine_light_rig_tab", "alias_of":"light_rim_tab"}  // v1 rim rows
```

Sources, all static: `floater_director.xml` tab names (`:314-1756`; there is no v1
`light_rim_tab`, `tron_tab` or `bloom_glow_tab`), the launcher cards and their targets in
`LLFloaterDirector` (`llfloaterdirector.cpp:1304-1394`: `ALFloaterLightBox::showTab("rotoink_settings"|
"tron_settings"|"cinehaze_settings"|"shadow_settings"|"colortone_settings"|"sellight_settings"|
"actor_mask_settings")`, `LLFloaterReg::showInstance("ultimate_diopter"|"cine_light_cues")`), the
floater registry names (`llviewerfloaterreg.cpp:769,772,781,891`), and the kit XUI
(`panel_dpage_look_stylize.xml:9` etc. carry the same launcher names). v1 rows for lightbox-hosted
controls get the launcher host too (the v1 pages have the same cards,
`floater_director.xml:1450,1461,1706,1717`), which converts the 371 "control not found" rows and
the 12 "page did not open" rows into testable rows or honest `EXPECT_MISSING`.

### 4.2 Verifying that a launcher opened its target

A launcher row (and the implicit launcher step of a hosted row) passes only when all hold, each
recorded separately in `observed.launcher`:

1. **Click accepted**: the launcher button is the handler (§1) and `LLButton::handleMouseUp`
   ran (capture released).
2. **Floater opened**: diff the visible floater set before/after (`functionalVisibleFloaters`,
   `aluiperfselftest.cpp:1489-1494`) and require exactly one new floater whose
   `getInstanceName()` equals `host.floater`. Do not rely on `LLFloaterReg::findInstance(name)`
   with a default key: `findInstance` matches by key (`llfloaterreg.cpp:125-146`), so a target
   shown with a non-empty key is invisible to the default-key lookup used at `:1861`.
3. **Target page selected**: for a tab target, the floater's `LLTabContainer` current panel name
   equals `host.tab` (`LLTabContainer::getCurrentPanel`); for a lazy kit page,
   `ALKitLazyPage::ensureBuilt()` returned non-null and the page drew once; for a popup menu
   target, `LLMenuGL::sMenuContainer` has a visible menu whose name matches; for a *tab switch*
   inside the same floater, the before/after current-panel names differ and the after equals the
   target.
4. **Control found in the target**, then the normal reveal/settle/click pipeline runs *inside the
   target*, with the target floater brought to front (it now competes with the Director floater
   for the point; `gFloaterView` order decides, `llfloater.cpp:1872 bringToFront`).
5. **Restore**: close the target if it was not open before (already done, `:2119-2121`).

Each failing step gets its own reason: `launcher: not clickable (<handler path>)`, `launcher:
no new floater (visible={...})`, `launcher: wrong floater <name>`, `launcher: tab <x> not current
(<y>)`, `launcher: control <name> missing in <floater>/<tab>`. The present single string
(`:2436`) is retired.

## 5. "No observable effect": categories and a per-kind effect oracle

### 5.1 Fix the label first

All 371 rows were `control not found` reported under the reason `no observable effect`
(`aluiperfselftest.cpp:2452-2454`). v2 reports lookup failures as `control not found in
<host>` (and FAILs them, since EXPECT_MISSING is the only legitimate absence). After §4 the
remaining true "no effect" rows are the ones that reach `finishFunctionalEntry`
(`:2581-2762`) and show `observable==false`. Those split into four categories, told apart by
the probes recorded in `observed`:

| Category | Signature in `observed` | Verdict |
|---|---|---|
| A. harness picked the wrong oracle | setting changed OR widget value changed OR commit signal fired, but the chosen probe (owner visible-state hash `functionalVisibleState` `:1419-1428`, consumer) did not move | harness gap: fix the row's oracle (5.2) |
| B. commit fired, owner slot ran, nothing externally visible by design (e.g. a "copy" or "refresh" button at login) | commit witness true, owner witness true, no state delta | PASS with `effect=owner_ran`; the manifest must mark `effect_kind=side_effect_only` |
| C. click handled but no commit | handler is the control, capture released, commit witness false | **real dead control** (the 10/10 dead-switch class) |
| D. commit fired but no owner slot / handler name unresolved (`UNWIRED`) | commit witness true, owner witness false, no delta | real unwired control (R2 "unbound page" class) |

### 5.2 Per-kind effect oracle

Three witnesses are captured for every row, in addition to the kind-specific one:

* **commit witness**: a scoped connection on the control's commit signal
  (`LLUICtrl::setCommitCallback` returns a `boost::signals2::connection`; connect, count, disconnect
  in the row's restore) so "did the widget commit" is observed, not inferred. This does not add a
  slot that could create an effect (R2 rule) because it only counts.
* **owner witness**: the registry/binder name resolution already performed (`:2558-2565`), plus
  for kit rows the `ALKitRow` history connection exists (`alkitrow.cpp:167-169`).
* **external delta**: setting delta (type-aware equality, `:1402-1417`), consumer delta
  (`functionalConsumerValue`, `:1608`), owner visible-state hash *excluding the control and its
  row label* (`:1419-1428` already excludes the control; also exclude `mLabel`/readouts the row
  itself updates, else a value readout can masquerade as an effect).

| Kind | Primary oracle | Secondary |
|---|---|---|
| toggle / kit_switch | bound setting flipped to `!before`; `isOn()==value==setting` (R3) | consumer if declared |
| slider / spinner / dial | setting == widget value after one pointer step; delta sign matches direction; epsilon from `increment` | consumer |
| combo / radio | setting or widget value == selected item value; for `bind="code"` rows the owner hash changes outside the combo | selector rows: `selected_light_values` consumer (kept) |
| button (setting reset) | target setting == default (`reset_defaults`, `:2639-2651`) | |
| button (launcher) | §4.2 | |
| button (action) | owner witness + commit witness; external delta optional only when `effect_kind=side_effect_only` is set by the generator from the handler body (the R5 hazard parser already resolves handler bodies, `functional_report.py:71-132`) | |
| color | picker floater opened; after programmatic commit the setting equals the colour | |
| text / line editor | setting or owner value == typed text after Return | |
| preset strip / provider | full-group settings snapshot differs and the published bind vector matches (R3/R5 rim rule kept) | |

The harness's own writes (prerequisites, reveal selector changes, reset pre-state) are excluded
from "effect" by taking the baselines *after* them (R4 item 9 already does this, `:2557-2572`);
the reveal layer in §2 must snapshot and exclude the selector/mode settings it touched.

## 6. Diagnostics

### 6.1 `observed` becomes structured JSON on every row (PASS included, truncated)

```
{"host":{"floater":"director","page":"cine_light_rig_tab","frontmost":true},
 "reveal":[{"tab":"light_tabs/cine_light_rig_tab"},{"card":"kc_rig_catchlight","mode":"Advanced","chip":"lights"},
           {"row":"cine_catchlight_ev"},{"scroll":"page_scroll","doc_y":812},{"settle_frames":2}],
 "rect":{"ctrl":[x0,y0,x1,y1],"target":[...],"viewport":[...],"floater":[...],"point":[x,y]},
 "visible_break":null, "enabled_break":null,
 "click":{"down_handler":"/main_view/.../cine_catchlight_ev/SpinCtrl Up","captor":"SpinCtrl Up",
          "up_handler":"...","capture_after":null,"commits":1},
 "setting":{"name":"CineLightRigCatchlightEV","before":0.0,"after":0.25},
 "widget":{"before":0.0,"after":0.25},
 "consumer":{"name":"catchlight_enabled","before":...,"after":...},
 "owner":{"handler":"ALPanelCineLightRig::bindLightRigPage","state_changed":false},
 "frames":{"reveal":2,"click":1}}
```

`visible_break` / `enabled_break` are the path of the first ancestor failing `getVisible()` /
`getEnabled()` plus its class name and, where known, the owner that set it (kit: card/page and
the mode/chip/body flag; legacy: `enabled_control` name or the parsed `enable_when`).
`functional_report.py` renders a per-reason table and a per-page "first break point" histogram,
so the next iteration knows exactly which reveal/enable rule to add.

### 6.2 Screenshot-per-failure

Reuse the audit capture path (`captureAfterRender`, `aluiperfselftest.cpp:1345-1394`: glReadPixels
of the floater's raw rect after render). Add a phase `FAIL_SHOT`: when a row fails and the
per-(page, reason) quota (default 2) is not exhausted, request a capture of the *host floater*
(Director or the launcher target), write
`LL_PATH_LOGS/functional_shots/<ver>/<page>/<control>__<reason>.png`, and record the control/target
rects in `observed.rect` so `functional_report.py` can draw the rectangles (PIL) into the copied
PNGs in the run directory. `ui_selftest.ps1` copies the folder next to `functional.csv`. Opt-in
flag `-Shots` (default on for `-Pages` runs, off for full runs) keeps full runs at ~1 frame/row.

### 6.3 Log line per row

One `LL_INFOS("UIFunctionalSelfTest")` line per row with the compact observed JSON, and the
`sDebugMouseHandling` messages are left on only during the injected click so `Alchemy.log`
also shows `Left Mouse down /path...` lines (`llviewerwindow.cpp:1278-1286`).

## 7. The gate rows: harness or product?

### 7.1 Rim: `light_rim gate Turn on is not user-clickable` (v1 and v2)

Flow (`prepareFunctionalRimGate`, `aluiperfselftest.cpp:2302-2392`): disable all five rig slots
via `applySceneData`, force `CineRigRimEnabled=true`, `CineRigRimMasterGain=1`, tick the manager,
yield a frame, then (v2) require the banner visible and click `light_rim_gate_on`; (v1) click
`cine_enabled` on the rig tab; then require `CineLightRigEnabled && ALCineRigRim::isEnabled()`.

* v2: the banner stage *passed* (the FAIL is the later message), so `ALKitPage::updateGateStatus`
  correctly showed the banner for `enabledMask()==0` (`alkitpage.cpp:962-969`,
  `alcinelightrigmanager.cpp:793-804`). The gate button then failed `functionalReachable` ->
  `childFromPoint` -> one of the transparent full-window holders (§0) -> "covered"; the reason is
  discarded (`:2370-2374`). Harness false FAIL.
* v1: the same `cine_enabled` control fails its own row with `not reachable: covered`
  (`panel_cine_light_rig.xml:35-37`), so the gate path fails identically. Harness false FAIL.
* Product check of what would have run: `light_rim_gate_on` commits
  `gSavedSettings.setBOOL("CineLightRigEnabled", true)` (`alkitpage.cpp:560-563`);
  `ALCineLightRigManager::tick` copies the selected slot's settings into its blob
  (`alcinelightrigmanager.cpp:1102 captureSelected`) so `enabledMask()!=0`;
  `ALCineRigRim::isEnabled()` is `CineRigRimEnabled && MasterGain>0 && enabledMask()!=0`
  (`alcinerigrim.cpp:110-120`), both forced true by the harness. No product bug is visible in
  this path. One hygiene note, not a functional bug: the gate button writes the setting directly
  rather than through the rig page's master switch / kit history (compare the master banner's
  `writeBoundValue`, `alkitpage.cpp:526-535`), so Undo will not record the "Turn on".

### 7.2 Weather (v2): `weather gate precondition failed: banner/predicate disagree`

The single message covers four conditions (`:2346-2355`): `predicate_off`, `banner!=null`,
`banner->getVisible()`, `turn_on!=null`. Facts: the weather kit page is `legacy="true"` with no
master setting and no modes (`panel_dpage_world_weather.xml:3-9`), so neither `setModeIndex`
nor a master switch can rewrite `AlchemyWeatherEnabled`; the gate banner/button are created for
`world_group/weather_tab` (`alkitpage.cpp:543-544,558-569`) and the names match the rim page's
convention that already worked (`prefix()`, `:276`). The row is the 51st of 52 on its page, so
the page was built long before. Visibility is applied in `ALKitPage::draw` ->
`updateGateStatus` (`:1397-1398`, `:962-985`) using a `static LLCachedControl<bool>` (`:972`).

Candidates, in order of likelihood, none provable from the CSV because `observed` is empty:

1. The one-frame yield did not include an `ALKitPage::draw` of the weather page (page switched
   from `light_rim_tab` in the same tick as the force; a draw is skipped when the rect is still
   invalid or culled, `llview.cpp:1302-1310`), so the banner was still hidden at the check.
2. The `force` write did not change the value (`setValue(false,false)` on an already-false
   control fires nothing) while the banner had been hidden by an earlier row's prerequisite
   (`AlchemyWeatherEnabled=true` from the `owner_rules` family, `functional_report.py:175`) and
   its restore; the draw that would re-show it had not happened yet. Same fix as 1.
3. `predicate_off` false because a later restore from the *previous* row ran in between. Less
   likely: `restoreFunctionalAction` runs before `beginFunctionalEntry` in the same tick.

Verdict: harness race, no evidence of a product defect. v1 `weather_enabled` is "covered" (§0).

### 7.3 Required changes to the gate rows

* Split the precondition into four recorded sub-checks with their own reason strings and wait
  for the page's draw counter to advance after the force (the §2.1 settle rule), bounded to 6
  frames.
* Keep the forced-off / clicked-on sequence (R5 item 6) but route the "Turn on" click through §1
  and record the handler path; `rim_render_active` and `weather_rain_intensity` remain the
  consumer oracles.
* Add a product assertion that is currently missing: after the gate button click, the banner
  must *hide* within one frame (`updateGateStatus` returns changed -> `reshape`,
  `alkitpage.cpp:1397-1398`), and the page's master/undo state must reflect the write.

## 8. Iteration plan and pass/fail budget

### 8.1 Cheap runs

* `ui_selftest.ps1 -Functional -Version v1|v2|both -Pages a,b -Rows <regex> -Shots`
  -> request string `functional|<manifest>|pages=a,b|rows=<re>|shots=1`, parsed where
  `functional|` is parsed today (`aluiperfselftest.cpp:1242-1249`) and applied in
  `loadFunctionalManifest` (`:1977-1978`). The report (`functional_report.py`) accepts the same
  filters so a one-page CSV diffs against the last full run.
* Cost model: 3 s settle (`:2838`) + viewer start + 1-3 frames per row -> a 50-row page is well
  under a minute; a `-Both` full run stays within the 5-minute timeout (`ui_selftest.ps1:37`)
  only if the settle rule is bounded (6 frames) and screenshots are quota-limited.
* `-DryReveal`: run reveal+reachability only (no exercise, no restore churn) to iterate on §2/§3
  rules at ~1 frame/row.

### 8.2 Order of attack (expected false-FAIL burn-down)

1. §1 dispatcher + handler witnesses + sentinel row. Removes `covered` (1080), the 294 launcher
   rows' first failure, and the 3 gate rows. Verify on `operator_tab` (67/67 covered, direct
   legacy panel) and `cine_light_rig_tab` v1 (142).
2. §4 host table / page aliases. Converts 371 + 12 lookup failures. Verify on v1 `light_rim_tab`
   (41 rows, alias) and v2 `stylize_tab` (78 rows, lightbox launcher).
3. §2 reveal state machine: settle, `revealCard`+`revealRow`, accordions, selector sweep
   (`CinematicCamMode`, `weather_control_group`). Verify on `shot_tab` (235 hidden), v2
   `cine_light_rig_tab` (118), `weather_tab` (36), `path_tab` (26).
4. §3 enable classification. Verify on `gaze_tab` (61), `ghosts_tab` (37), v1 rig (21).
5. §5 oracle categories A-D on whatever remains; every remaining FAIL must be category C or D
   (real defects) or be fixed in the manifest.

### 8.3 Budget policy (what "green" means)

* `FAIL == 0` and `KNOWN_GAP == 0` on RUN rows, per version.
* **Minimum PASS floor per interactive kind and version**: toggle, slider, spinner, combo,
  button each >= 1 PASS that reached `finishFunctionalEntry` with a real click (`click.commits
  >= 1`), and total interactive PASS >= 60 % of RUN rows. This single rule would have failed the
  10/11 run (0 interactive PASS) immediately.
* `DEFER_INWORLD` and `SKIP` rows must carry `reason` **and** either `enable_when`/`visible_when`
  or a hazard tag; their counts are frozen in `doc/director_kit/functional/defer_budget.json`
  per (version, page). Any increase fails preflight; decreases are accepted and rewrite the
  budget on `-UpdateBudget`.
* Sentinel row FAIL aborts the run (exit 2, "injection channel broken") rather than producing
  thousands of derived FAILs.
* `EXPECT_MISSING` rows are reported in their own column and never counted toward the PASS floor.

## 9. Old-code regression coverage (goal b) after v2

| 10/10 class | v2 assertion that still fails on the old code |
|---|---|
| Selector wrote S32 via `setLLSD` | segment click through the real dispatcher must set `DirectorV2SelectedLight` and move `selected_light_values` (unchanged from R3/R5) |
| Rim switch/preset without rig | `rim_render_active` / `rim_bind_gain` consumers (unchanged); gate "Turn on" now really clicked |
| Switch visually stuck | `isOn()==value==setting` after a *dispatched* click that went through `LLButton::handleMouseDown/Up` (`llbutton.cpp:496-565`); capture released; no focus ring. A switch that only listens to `setValue` cannot pass |
| Empty spinner value | `functionalSpinnerTextFits` (`aluiperfselftest.cpp:1904-1912`) kept, now also after one arrow-button click |
| Unbound page / dead control | category C/D in §5.1: commit witness and owner witness are observed directly |

## 10. Code reference index

`llview.cpp` 355 (addChild push_front), 765-776 (visibleAndContains), 779-785 (logMouseEvent),
810-837 (childrenHandleMouseEvent), 922-952 (childFromPoint), 1170-1175, 1241-1264,
1281-1310 (draw culling), 1777-1787 (pointInView/blockMouseEvent), 411/429 (visible/enabled
chain), 480 (getPathname); `llview.h` 648-668 (sDrilldown), 708-709 (debug statics);
`llviewerwindow.cpp` 1129-1313 (handleAnyMouseClick), 1315-1353; `llviewerwindow.h` 162, 208,
217, 489; `llviewerinput.cpp` 1800-1845; `llmousehandler.cpp` 32-57; `llfocusmgr.cpp` 330-345,
377-402, 430-450; `llfocusmgr.h` 90-121; `llbutton.cpp` 496-565; `llslider.cpp` 284-340;
`llcheckboxctrl.cpp` 128-152; `llspinctrl.cpp` 103-120; `llcombobox.cpp` 110, 136-147,
675-768, 795; `llpopupview.cpp` 97-157; `llfloater.cpp` 1853-1878; `llfloaterreg.cpp` 125-146,
276-293; `lltabcontainer.cpp` 545, 1511; `llaccordionctrltab.cpp` 402, 451;
`llscrollcontainer.cpp` 115-151, 794; `llcallbacklist.cpp` 145-148; `llstartup.cpp` 457-458;
`main_view.xml` 55-168; `alkitpage.cpp` 276, 516-571, 800-949, 962-985, 1060-1119, 1212-1234,
1335-1362, 1364-1400; `alkitcard.cpp` 263-273, 545-560, 622-690, 766-793, 822-844, 846-872,
909-982; `alkitswitch.cpp` 12-38, 70-73; `alkitrow.cpp` 103-169; `alkitlazypage.cpp` 28-81;
`alkitshell.cpp` 732-766; `alflowgrid.cpp` 60-179; `alpanelcinecamparams.cpp` 520-533;
`alpanelweathersettings.cpp` 132-189, 219; `alpanelpatheditor.cpp` 99, 766-783;
`alpanelghoststudio.cpp` 1396-1464; `alpanellensgaze.cpp` 1125; `alcinerigrim.cpp` 110-133;
`alcinelightrigmanager.cpp` 793-804, 1091-1113; `llfloaterdirector.cpp` 1304-1394;
`llviewerfloaterreg.cpp` 769, 772, 781, 891; `floater_director.xml` 314-1756;
`panel_cine_light_rig.xml` 1-60, 225-252, 935-968; `panel_dpage_light_cine_light_rig.xml`
4-8, 538-575; `panel_dpage_world_weather.xml` 3-9; `panel_dpage_light_rim.xml` 4-7;
`panel_weather_settings.xml` 10-38; `panel_path_editor.xml` 177-199; `panel_cinecam_params.xml`
1-60; `aluiperfselftest.cpp` 1242-1249, 1345-1394, 1419-1428, 1777-1867, 1870-1902,
1947-1960, 1962-1989, 2133-2213, 2216-2280, 2302-2392, 2395-2578, 2581-2762, 2834-2879;
`scripts/perf/functional_report.py` 155-373; `scripts/code_tools/director_functional_manifest.py`
500-624; `scripts/perf/ui_selftest.ps1` 1-60, 148-242.
