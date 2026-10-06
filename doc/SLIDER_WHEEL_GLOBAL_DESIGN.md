# Viewer-wide slider mouse wheel: design (DESIGN ONLY, no code yet)

Status: DESIGN v7 (2026-10-05), DESIGN ONLY. No source edits, builds or commits. v1-v6 were NOT
APPROVED by Codex rounds 1-6. v7 fixes the R6 pseudocode-consistency items (below); the older tables
are kept for history.
User decisions (2026-10-05): ACCEPTED the any-enclosing-scroller guard (§9.1) and the default nice125
step for UNSET sliders, so Local offset = 25 mm/notch (§9.2). Next: Codex round 7.

## v7 changes (Codex R6 resolution)

R6 closed: R5 #1 (hardened `LLSliderCtrl::setValue`) and R5 #3 (ancestor-walk liveness).

| # | Finding | v7 resolution | Where |
|---|---------|---------------|-------|
| 1 | §4.2b still used `R()` (ctrl only) after callbacks, so a child deleted during `setValue`, validation or `onCommit` could be followed by `self->getValueF32()` or another signal | The ctrl-only `R()` macro is REMOVED. After every foreign call where execution continues, the pseudocode uses `RS()`, which re-resolves the ctrl AND the child | §4.2b |
| 2 | `updateText()` is not ctrl-only: it calls `getValueF32()`, which is `mSlider->getValueF32()` (llsliderctrl.h:100) | Both commit paths do a fresh `RS()` immediately before `updateText()`. Any `self->getValueF32()` (the editor signal argument, the reject-path compare) is also preceded by a fresh `RS()` | §4.2b |
| 3 | Replace EVERY post-callback `R()` with `RS()` | Done: after `setValue`, validate, `onCommit`, the editor signal and the reject-path `setValue`, on both `onEditorCommit` and `onSliderCommit`. The hardened `LLSliderCtrl::setValue` already re-resolves both | §4.2b |
| 4 | `aliveChild` must reject a REPLACED child | Becomes a member, `LLSlider* LLSliderCtrl::liveSlider(const LLHandle<LLSlider>& sh) const`: returns `s = alive(sh)` only if `s->getParent() == this` AND `mSlider == s`. A deleted, `die()`d, detached, reparented or replaced child is rejected | §4.2b, §8 |

## v6 changes (Codex R5 resolution)

R5 closed: §4.3/§5a (preview-only notches, a single final commit, stock typed/keyboard/drag).

| # | Finding | v6 resolution | Where |
|---|---------|---------------|-------|
| 1 | `LLSliderCtrl::setValue` is internally unsafe: the child `mSlider->setValue()` can run a control listener that destroys the ctrl or the slider, and :228-229 / :238-239 then touch stale `mSlider`/`mValue`/`updateText` | BOTH overloads (llsliderctrl.cpp:224-230 and :235-240) take a ctrl handle and a child handle before the child call. Afterwards they re-resolve both, and only then touch `mSlider`, `mValue` or `updateText`, through the re-resolved pointers, never `this` | §4.2b |
| 2 | "A live ctrl implies a live slider" is invalid: a callback can remove or destroy just the child | Dropped. The slider is re-resolved INDEPENDENTLY after every callback, through `aliveChild(sh, ctrl)` = alive + not dead (with ancestors) + still parented to that ctrl. This happens before `cancelWheelSettle()` and before ANY child access. If the child fails, the path returns (an abnormal state; nothing else is safe to assume). New child-only delete/`die()`/`removeChild` tests | §4.2b, §8 |
| 3 | `alive(ctrl)` checked only the object, so `die()` on its floater gave no early return | `alive()` now walks the parent chain and rejects the object if it or ANY ancestor `isDead()`. This is memory-safe: `die()` defers deletion (LLMortician), and a direct delete kills the handle first, because LLView's dtor deletes its children. Cost: one parent walk (UI depth, ~10-20) per callback point, and it runs ONLY on the §4.2b callback paths (commit/validate/editor/settle/close-flush, and `setValue` only when it actually writes a control), never per frame or per draw | §4.2b |

Request: "Wheel on ALL sliders viewer-wide", with the Lightbox/Director scroll-through guard, so a
panel scroll never nudges a slider. Trigger: stock Set Hover Height (floater_edit_hover_height.xml,
increment 0.001, decimal_digits 3, range -3..3 set in llfloaterhoverheight.cpp from MIN/MAX_HOVER_Z)
where the wheel does nothing.

## v5 changes (Codex R4 resolution)

R4 closed: the close-flush self handle / serial / nested-close design, and hosted self-close, host
ancestry and closeAllFloaters coverage.

| # | Sev | Finding | v5 resolution | Where |
|---|-----|---------|---------------|-------|
| 1 | P1 | Lifetime hardening was incomplete: the validate signals (llsliderctrl.cpp:378 in `onEditorCommit`, :414 in `onSliderCommit`) are also arbitrary callbacks, but v4 re-resolved only after `onCommit` and the editor signal | New §4.2b rule, which applies to EVERY call that can run foreign code, not a list of sites. (a) Take the handle BEFORE the first such call. (b) After each one, re-resolve through `alive(h)` = `h.get()` AND `!isDead()` (views are `LLMortician`s, llview.h:101), and return before any further access if it fails. Foreign-code calls on these paths: the control-variable write inside `LLSlider::setValue` (`setControlValue`, llslider.cpp:130, which runs control listeners), `mValidateSignal` (:378, :414), `onCommit()` (:387, :421), `mEditorCommitSignal` (:389), the reject-path `setValue(saved_val)` (:395, :427; control write), and `mMouseUpSignal` (settle). Tests add delete and `die()` from a validate callback on both paths, plus from a control listener | §4.1, §4.2b, §8 |
| 2 | (align) | Client Avatar Height: its code's manual per-notch `onFinalCommit` is being REMOVED, so the stock hover slider keeps today's behaviour | Stated as a rule: for sliders with a mouse-up finaliser, wheel commits are PREVIEW-ONLY (the normal commit signal per notch), and the settle or close flush is the SOLE final commit for wheel changes. Keyboard arrows and typed-text finalisation stay exactly stock. No caller may finalise per notch. New test | §4.3, §5a, §8 |

## v4 changes (Codex R3 resolution, history)

R3 closed: destruction-only cancel, ancestry propagation, the logout latch at both sites, kill-switch
stock behaviour, disabled-slider keep-alive, direct hide/minimise timer behaviour, tear-off ancestry,
non-floater panels.

| # | Sev | Finding | v4 resolution | Where |
|---|-----|---------|---------------|-------|
| 1 | P0 | Close-flush re-entrancy: `settleForClose()` runs arbitrary callbacks that may re-enter `closeFloater()` or delete the floater (`LLFloaterReg::destroyInstance` -> `delete`, llfloaterreg.cpp:236-241). The outer call then continues into `onClose`/signals/visibility (llfloater.cpp:842-869) with a stale `this` | Close-in-progress guard + self handle + lifecycle serial. The outer close returns IMMEDIATELY after the flush, touching no member, if the floater was deleted, a nested close or a reopen superseded it, or it is dead. A nested close during the flush skips the flush step and runs the full close itself. `settleForClose` re-checks root liveness and serial before EVERY fire, and on a supersede it re-inserts the unfired entries for the normal tick | §4.2a, §8 |
| 2 | P1 | Typed-commit safety claim was false: after `onCommit()` (llsliderctrl.cpp:387) and `mEditorCommitSignal` (:389), `self->updateText()` (:399) still dereferences `self` | `onEditorCommit`, and `onSliderCommit` (same shape, :421/:431, reached by every wheel commit), take `LLHandle<LLSliderCtrl>` on entry and re-resolve it after EACH potentially destructive callback, returning before the next access if it is dead. The settle cancel still runs before any callback. This also hardens the pre-existing stock deref that drags and typed text already have | §4.2 |
| 3 | P1 | "A multi-floater close closes each hosted floater" was false: a hosted floater's own close detaches it from the host before callbacks (llfloater.cpp:781-784), and `LLMultiFloater::onClose` (llmultifloater.cpp:70-77) does not close tabs | Corrected. A hosted floater closing ITSELF is detached, then flushes with itself as root (its sliders are still its descendants). A HOST closing flushes with the host as root; hosted tabs are its descendants, so host-root ancestry covers them, and the tabs are only hidden with the host, not closed. `LLMultiFloater::closeAllFloaters` (:92-115) calls each tab's own `closeFloater`, so each tab flushes itself | §4.2 |

## v3 changes (Codex R2 resolution, history)

R2 closed: tri-state propagation, the post-callback comparison, vertical OFF, resolved accessors, the
non-scrollbar audit + rule, enabled-slider H-wheel swallow, the LLHandle-only registry, CMake.

| # | Sev | Finding | v3 resolution | Where |
|---|-----|---------|---------------|-------|
| 1 | P0 | v2 flushed from `LLSlider::onVisibilityChange(false)`. On direct destruction, `LLFloater::~LLFloater` (llfloater.cpp:575) calls `setVisible(false)` (:602) AFTER the derived callback owner is destroyed; a live slider handle does not prove a bound derived `this` is alive | The visibility flush is REMOVED. The flush happens in `LLFloater::closeFloater()` immediately BEFORE `onClose()` (llfloater.cpp:841), while the whole floater, including the derived class, is alive. Every destruction path only CANCELS | §4.2 |
| 2 | P0 | Visibility propagation is unreliable: `LLPanelEnvironmentInfo::onVisibilityChange` never calls its base (llpanelenvironment.cpp:240) | Nothing depends on visibility notifications. The close root searches the settle REGISTRY for live sliders that have the closing floater as an ancestor (`hasAncestor`), so no notification has to reach the slider | §4.2 |
| 3 | P0 | `LLApp::isExiting()` is false during graceful logout: `closeAllChildren(true)` runs while only `mQuitRequested` is about to be set (llappviewer.cpp:4441/4451, 5686) | Quit = cancel WITHOUT firing, three layers. (a) `closeFloater(app_quitting=true)` cancels its descendants instead of flushing. (b) New `LLSliderWheel::cancelAllForQuit()` is called in llappviewer.cpp immediately before BOTH `closeAllChildren(true)` sites; it drops every entry and latches a quitting flag, so nothing schedules or fires afterwards. That also covers sliders outside floaters (Quick Settings pulldown). (c) The tick drops everything if `LLFloater::isQuitRequested()` (sQuitting, set at llfloater.cpp:770-773) or `LLApp::isExiting()`. So no settle can send Quick Settings hover (alpanelquicksettings.cpp:134) or call region `updateParcel` (llpanelenvironment.cpp:761) during logout | §4.2 |
| 4 | P1 | Typed-commit invalidation came after the callbacks | `onEditorCommit` invalidates as the FIRST statement of its `if (success)` block, BEFORE `self->onCommit()` (llsliderctrl.cpp:387) and `mEditorCommitSignal` (:389). (v3's "nothing touches the slider after the callbacks" was wrong; corrected in v4 #2) | §4.2 |
| 5 | P1 | "Kill switch restores exact stock" was false: UNSET vertical and H-wheel paths still went through the new guard | One predicate, `participates()` = ON, or UNSET with the switch on, gates ALL new behaviour: guard, new step, settle, and the disabled-slider container hook. With the switch off, UNSET sliders run today's code verbatim | §2.2 |
| 6 | P1 | Missed Lightbox/Director delta: WheelGuard hit-tests with `childFromPoint` (visibility only, llview.cpp:889-898), so it SEES disabled sliders (V-wheel re-arms, H-wheel swallowed, alscrollfit.cpp:406-423), while dispatch skips disabled views (llview.cpp:784) | Preserved, not just documented. When its children did not handle the event, `LLScrollContainer` (the only container type WheelGuard covered) runs WheelGuard's own hit test (`LLSliderWheel::sliderAt`). If it is guarded and over a participating slider (necessarily disabled, since an enabled one would have handled the event): on a V-wheel it calls keepAlive and then scrolls as normal; on an H-wheel it calls keepAlive and swallows. The nested-scroller row (accepted by the user) is again the ONLY delta | §1.3, §1.4 |

## v2 changes (Codex R1 resolution, history)

| # | Sev | Finding | v2 resolution | Where |
|---|-----|---------|---------------|-------|
| 1 | P0 | LLSliderCtrl copies its default `Optional<bool>` into the child, marking it provided (llsliderctrl.cpp:148-151) | Copy only when `p.wheel_adjust.isProvided()`; otherwise the child stays UNSET | §2.1 |
| 2 | P0 | Settle lifecycle underspecified (the mouse-up handler runs on every mouse-up, llslider.cpp:262) | One pending generation per slider; precise invalidation points; no scheduling while captured; a captured tick defers (amended in v3) | §4.2 |
| 3 | P0 | Settle only after a final ACCEPTED change (validation revert llsliderctrl.cpp:425-428; clamp suppresses the commit, llslider.cpp:211-220) | One helper compares the POST-callback value with the snapshot | §4.1 |
| 4 | P1 | OFF undefined for vertical sliders (llslider.cpp:347) | Explicit `false` disables both orientations and the H-wheel. Nothing sets `false` today | §2.2 |
| 5 | P1 | `getWheelAdjust()` raw but used for dispatch (llsliderctrl.cpp:246) | Resolved `wheelEnabled()`; `setWheelAdjust`/`enableSliderWheel` = explicit ON/OFF | §2.3 |
| 6 | P1 | Non-scrollbar scrollers (llpanelemojicomplete.cpp:257, llchiclet.cpp:1031) | Audited: none can contain a slider; standing rule for future ones | §1.2 |
| 7 | P1 | Guarded H-wheel must stay SWALLOWED (alscrollfit.cpp:414-423) | Swallowed + keep-alive | §1.3, §1.4 |
| 8 | P1 | No raw pointers in the timer | Registry holds `LLHandle<LLSlider>` + generation only | §4.2 |
| 9 | P2 | Build | CMakeLists.txt:73 / :192 | §7 |

## 0. What exists today (verified)

- `LLSlider::handleScrollWheel` (llslider.cpp:343-404): vertical sliders ALWAYS take the wheel;
  horizontal ones only with `mWheelAdjust` (XML `wheel_adjust`, default false; runtime
  `setWheelAdjust()`). Step = increment, Ctrl/Alt = x10. With `wheel_fine_div > 0`, Shift = increment/div
  and the value snaps to the fine grid. Commit: legacy path = `setValueAndCommit` (llslider.cpp:211,
  commits only if the clamped/snapped value changed); fine path = `setValue(...,precision_override)` then
  `onCommit()` if changed (369-375). `handleScrollHWheel` (406-415) steps every horizontal slider
  unconditionally.
- `LLSliderCtrl` ctor (llsliderctrl.cpp:148-151) copies `wheel_adjust` into the child whenever the
  child's own value is unprovided, which marks the child provided (the ctrl default `false`,
  llsliderctrl.h:81). `wheel_fine_div` is copied the same way, which is harmless because 0 means off.
- `LLSliderCtrl::handleScrollWheel` (244-251) forwards the whole ctrl rect only for
  `getWheelAdjust() && getWheelFineDiv() > 0`. `onSliderCommit` (403-432) validates and on reject calls
  `setValue(saved_val)`. `onEditorCommit` (358-400): on success, `onCommit()` (387) then
  `mEditorCommitSignal` (389).
- Mouse: `handleMouseDown` (284-320) fires the mouse-down signal. Ctrl+click = `setValueAndCommit(initial)`
  with NO capture; otherwise it takes capture (314). `handleMouseUp` (262-282) fires `mMouseUpSignal`
  only when captured. The wheel goes to the mouse captor first (llviewerwindow.cpp:3687), so a wheel
  during a drag reaches the captured slider.
- `ALScrollFit::WheelGuard` (alscrollfit.cpp:367-433), per floater, nearest enclosing
  `LLScrollContainer`, hit test `childFromPoint(x,y,true)` (findWheelTargets :36-57; it checks visibility
  only, so disabled sliders are found). V-wheel guarded: masks `wheel_adjust` for one dispatch, the
  container scrolls, and the guard re-arms even when the container is parked. H-wheel guarded:
  SWALLOWED + keep-alive. Used only by ALFloaterLightBox (281-293) and LLFloaterDirector (739-749);
  `enableSliderWheel` (354-365) sets their sliders ON.
- Dispatch: captor / top ctrl / `mRootView` (`childrenHandleMouseEvent` skips invisible AND disabled
  views, llview.cpp:784) -> tool -> camera zoom. `LLFloater::handleScrollWheel` always returns true.
- Scrollbar scrolling goes through `LLScrollbar::scrollByWheel` (llscrollbar.cpp:413-448; `changeLine`
  true = moved): LLScrollContainer (llscrollcontainer.cpp:265-317), LLAccordionCtrl (:543),
  LLAccordionCtrlTab (:1160), LLScrollListCtrl (:1856), LLTextBase (internal LLScrollContainer),
  LLHexEditor, LLPanelEmojiComplete's scrollbar branch.
- Floater lifecycle: `closeFloater(app_quitting)` (llfloater.cpp:~765-871) sets `sQuitting` when
  quitting (770-773), closes dependents, then `onClose` (842) / close signal, then hides and maybe
  destroys. `~LLFloater` (575) calls `setVisible(false)` (602) after derived destructors have run.
  Logout: `closeAllChildren(true)` at llappviewer.cpp:4441 and 5686, before `mQuitRequested` (4451).
- `LLSpinCtrl::handleScrollWheel` (llspinctrl.cpp:481) always steps.
- XML: 222 `wheel_adjust="true"` (diopter 149, rig 59, adjust_environment 12 with `wheel_fine_div`,
  director 1, lightbox 1); 0 `wheel_adjust="false"`. Vertical sliders: media volume, camera zoom,
  floater_test_slider. None is inside a scroll container.

## 1. Global guard in llui

### 1.1 Mechanism
- New `indra/llui/llsliderwheel.h/.cpp`, namespace `LLSliderWheel`. Guard state: one static
  `LLHandle<LLView>` (stamped owner) + `F64` stamp time. API: `noteScroll(LLView* owner)`, `keepAlive()`,
  `bool guarded(const LLView* v)` = the stamp is < 0.5 s old (WHEEL_GUARD_SEC), the stamped owner is
  alive through its handle, and the owner is `v` or an ancestor of `v`. A dead handle never guards.
- `LLScrollbar::scrollByWheel`: `noteScroll(getParent())` when `changeLine()` returns true. A sub-notch
  "handled, no line" return (llscrollbar.cpp:429-446) does not stamp.
- Order: within one event the slider (deeper) is visited before the container scrolls and stamps, so
  only earlier events arm the guard, as with WheelGuard.

### 1.2 Non-scrollbar scrollers (R1 #6, closed)
Audited every `handleScrollWheel` override. Only three move content without `scrollByWheel`, and none
can contain an LLSlider:
- `LLPanelEmojiComplete` (llpanelemojicomplete.cpp:257-274): its only child view is `mScrollbar` (:76).
- `LLChicletPanel` (llchiclet.cpp:1031-1042): chiclets + scroll buttons; 0 slider references.
- `LLMenuGL::scrollItems` (llmenugl.cpp:3217): only `LLMenuItemGL` children; 0 slider references.
Standing rule for CLAUDE.md: any scroller that moves content without `LLScrollbar::scrollByWheel` must
call `LLSliderWheel::noteScroll(this)` on a real move. The Codex review checks new `handleScrollWheel`
overrides.

### 1.3 Behaviour under the guard (only for `participates()` sliders, §2.2)
- Slider, V-wheel, guarded: `keepAlive(); return false;` The container scrolls (and stamps) or is parked.
- Slider, H-wheel, guarded: `keepAlive(); return true;` = SWALLOWED, as WheelGuard does
  (alscrollfit.cpp:419-423). Stock never H-scrolled a container from over a slider either, because the
  slider always stepped.
- The enable/participation check comes first. An OFF or non-participating slider runs its stock path
  and never reads or extends the guard.
- Disabled-slider hook (R2 #6): dispatch never reaches a disabled slider, but WheelGuard saw it. In
  `LLScrollContainer::handleScrollWheel` (after `LLUICtrl::handleScrollWheel` returned false, before
  scrolling) and in `LLScrollContainer::handleScrollHWheel` (same place):
  `LLSlider* s = LLSliderWheel::sliderAt(this, x, y)`. This is WheelGuard's findWheelTargets verbatim:
  `childFromPoint(x, y, true)`, walking up to `this`, taking an LLSlider or an LLSliderCtrl's inner
  slider. If `s && s->participates() && LLSliderWheel::guarded(this)`: on a V-wheel, `keepAlive()` and
  then scroll as normal (which may also stamp); on an H-wheel, `keepAlive(); return true;` (swallow).
  Enabled sliders never get here, because they already handled or declined the event themselves with
  the same outcome. Only LLScrollContainer gets the hook, because WheelGuard covered only that type;
  accordion/list scrollers keep the plain stamp rule (they are new guard territory, §6.4).

### 1.4 Lightbox/Director equivalence (sliders explicit ON)
| Case | WheelGuard today | v3 |
|---|---|---|
| V-wheel over enabled slider, guarded (same nearest container) | slider masked, container scrolls, re-arm | slider declines + keepAlive, container scrolls/parked: same |
| H-wheel over enabled slider, guarded | swallowed, keep-alive | slider swallows, keep-alive: same |
| V-wheel over DISABLED slider, guarded | dispatch skips it, container scrolls; re-arm even when parked | container hook: keepAlive, then scroll: same |
| H-wheel over DISABLED slider, guarded | swallowed, keep-alive | container hook: keepAlive, swallow: same |
| Wheel over slider, unguarded | slider steps (legacy step); disabled = skipped | same (explicit ON = legacy step, §3) |
| Wheel not over a slider | stock; arms when the container moved | stock; the scrollbar stamps when it moved: same |
| Slider in an INNER scroller while the OUTER one moves | not guarded (nearest differs): slider steps mid-gesture | guarded (any ancestor): keeps scrolling. ONLY delta, ACCEPTED by the user (§9.1) |

Retire `WheelGuard`, `Controller::onScrollWheel/onScrollHWheel/wheel()` and the four overrides in
ALFloaterLightBox / LLFloaterDirector (+ header decls). KEEP `enableSliderWheel` (explicit ON = legacy
step). alscrollfit.h doc header: drop the wheel-override lines.

## 2. Default on, with opt-outs

### 2.1 Tri-state and propagation (closed)
- `LLSlider::mWheelAdjust` -> `enum EWheelAdjust { WHEEL_UNSET, WHEEL_ON, WHEEL_OFF }`, set in the ctor
  from `p.wheel_adjust.isProvided() ? (p.wheel_adjust ? ON : OFF) : UNSET`.
- `LLSliderCtrl` ctor (148-151): `if (p.wheel_adjust.isProvided() && !slider_p.wheel_adjust.isProvided())
  slider_p.wheel_adjust = p.wheel_adjust;`. No widget default XML sets `wheel_adjust` (0 hits).
- Test: an LLSliderCtrl without `wheel_adjust` has a child that reports UNSET.

### 2.2 Resolution and the kill switch (R2 #5)
- Setting `UISliderWheelAdjust` (settings.xml, Boolean, Persist, default 1) via `LLCachedControl<bool>` on
  `LLUI::getInstance()->mSettingGroups["config"]` (pattern llsliderctrl.cpp:376).
- `bool participates() const` = `ON || (UNSET && UISliderWheelAdjust)`. It gates EVERYTHING new: the
  guard, the new step rule, the settle, and the §1.3 container hook.
- `handleScrollWheel` / `handleScrollHWheel`:
  - OFF: decline both orientations and the H-wheel (return base). Nothing sets `false` today, so no
    shipped slider changes. Rationale: an opt-out exists because a wheel step is wrong for that control
    (the scrubber), whatever its orientation.
  - `participates()`: §1.3 guard -> sub-notch pass-through -> §3 step -> §4.1 accept -> §4.2 settle.
  - UNSET with the switch off: today's code VERBATIM. Vertical: legacy step via `setValueAndCommit`.
    Horizontal: no V-wheel (base). H-wheel: stock increment step. No guard, no settle, no hook. That
    is exact stock.
- `wheelEnabled()` (V-wheel, used for LLSliderCtrl dispatch) = OFF -> false; `participates()` -> true;
  UNSET with the switch off -> `mOrientation == VERTICAL`. `hwheelEnabled()` = `!OFF`.
- Explicit ON sliders (the 222 + Lightbox/Director) ignore the switch.

### 2.3 Accessors (closed)
- Remove `getWheelAdjust()`. Add `wheelEnabled()`, `hwheelEnabled()`, `participates()` and
  `getWheelAdjustState()` (raw, tests only). `LLSliderCtrl::handleScrollWheel` uses
  `mSlider->wheelEnabled() && mSlider->getWheelFineDiv() > 0.f`. `setWheelAdjust(bool)` -> explicit ON/OFF.

### 2.4 Never-wheel list and notes
- `media_play_slider` (bare `<slider_bar>`, panel_prim_media_controls.xml:367-381, mouse_down/up =
  MediaCtrl.MouseDown/Up): `wheel_adjust="false"` (REQUIRED).
- Media volume and camera zoom are vertical, with legacy step unchanged. World-map zoom slider: fine.
- Disabled sliders are skipped by dispatch (hover until the region feature arrives; the stock hover
  slider for non-Me targets, §5a).
- Sub-notch events (`delta.mClicks == 0`): return false so the container accumulates residue.

## 3. Step rule (only for `participates()` sliders; otherwise stock)
- `wheel_fine_div > 0`: the existing path verbatim.
- Explicit ON and all vertical sliders: legacy step (increment; Ctrl/Alt x10).
- Horizontal UNSET (switch on): `plainStep(min, max, inc)` = `nice125(range / (100 * inc)) * inc`,
  where `nice125(x)` = the largest of {1,2,5}x10^k that is <= x, with a minimum of 1. Shift = `inc`;
  Ctrl/Alt = `10 * step`; Shift wins. Hover: 0.05/notch, Shift 0.001, Ctrl 0.5. 0..1/0.01 -> 0.01. Local
  offset (-2..2 / 0.005): 0.025, Shift 0.005, Ctrl 0.25 (user-accepted). The step is always a multiple
  of inc, so there is no re-snap.
- H-wheel: increment, no modifiers (as today).
- Clamp by `setValue`; `delta.mClicks` multiplies the step.

## 4. Commit semantics and wheel settle

### 4.1 Accepted-change helper (closed)
All participating wheel paths (fine, legacy, UNSET, H-wheel) end in:
```
bool LLSlider::applyWheelValue(F32 requested, bool precision_override)
{
    LLHandle<LLSlider> h = getDerivedHandle<LLSlider>();  // before ANY foreign code (§4.2b)
    const F32 before = getValueF32();
    setValue(requested, false, precision_override);   // clamp + snap; control write runs listeners
    LLSlider* s = LLSliderWheel::alive(h);
    if (!s) return false;
    if (s->getValueF32() == before) return false;     // clamped: no commit (as today)
    s->onCommit();                                    // ctrl validate (may revert) + commit signal
    s = LLSliderWheel::alive(h);
    return s && s->getValueF32() != before;           // POST-callback compare
}
```
Only `true` may schedule a settle, and the caller (the wheel handler) schedules through
`alive(h)` as well, never through `this`.

### 4.2 Wheel settle lifecycle (R2 #1-#4)
Purpose: the four families in §4.3 do their final/expensive commit on the slider mouse-up, which the
wheel never produces. The settle synthesises exactly one mouse-up signal per wheel gesture.

Per slider: `U32 mWheelSettleGen` (bumped by every invalidation) and `bool mWheelSettlePending`.
Registry (`LLSliderWheel`): entries `{LLHandle<LLSlider>, U32 gen, F64 deadline}` only, at most one per
slider; a static `sQuitting` latch; one shared `LLEventTimer` (0.05 s period) that exists only while
the registry is non-empty.

Schedule, after `applyWheelValue` returns true:
- skip if `!mMouseUpSignal`, if `hasMouseCapture()` (the drag's real mouse-up will report), or if
  `LLSliderWheel::sQuitting`;
- otherwise `mWheelSettlePending = true` and upsert the entry with `{getDerivedHandle<LLSlider>(),
  mWheelSettleGen, now + 0.3 s}`. A later notch moves the deadline of the same entry.

Invalidate = `++mWheelSettleGen; mWheelSettlePending = false;` (cancel, never fire):
- `handleMouseDown`, capture branch (llslider.cpp:314): a drag starts and its mouse-up will report.
  The Ctrl+click branch (no capture, no mouse-up) leaves a pending settle alone, so the settle reports
  the reset value once.
- `handleMouseUp`, captured branch, BEFORE `(*mMouseUpSignal)` (266-271). The uncaptured branch does not
  invalidate.
- `LLSliderCtrl::onEditorCommit`: the FIRST statement inside `if (success)`, BEFORE `self->onCommit()`
  (llsliderctrl.cpp:387) and `mEditorCommitSignal` (389): `mSlider->cancelWheelSettle()`. The typed
  value is the final commit (the hover floater wires the editor commit to its final handler).
  Lifetime: see §4.2b. It covers every callback on this path, including the validate signal at :378.
- `~LLSlider`: bump. The dead handle also drops the entry. Destruction ONLY cancels, on every path
  (direct delete, `die()`/mortician, parent destruction), and nothing fires from a destructor or from
  visibility changes. There is NO `onVisibilityChange` override.

Flush at close (R2 #1, #2): `LLSliderWheel::settleForClose(LLHandle<LLFloater> root, U32 serial, bool
app_quitting)`, called from the §4.2a block in `LLFloater::closeFloater()` immediately BEFORE
`onClose(app_quitting)` (llfloater.cpp:841-842). At that
point canClose() has passed and dependents are closed, but onClose has not run, nothing is hidden and
nothing is destroyed, so the full derived floater and every child are alive.
1. Collect `{handle, gen}` for the entries whose slider is alive and `s == root || s->hasAncestor(root)`,
   and erase them from the registry. No visibility propagation is involved, so a panel that does not
   call its base `onVisibilityChange` (llpanelenvironment.cpp:240) cannot block this.
2. If `app_quitting || sQuitting || LLFloater::isQuitRequested() || LLApp::isExiting()`: cancel each
   (bump, no fire).
3. Otherwise, before EACH fire: if the root handle is dead or the root's `mLifecycleSerial` differs
   from the serial passed in (superseded, §4.2a), stop firing. Re-insert the remaining entries whose
   slider is still alive into the registry with their gen (the normal tick re-validates them, so
   nothing is lost) and return. Otherwise re-resolve the slider handle and call
   `s->fireWheelSettle(gen)`, which re-checks gen/pending, clears pending and calls
   `(*mMouseUpSignal)(this, getValueF32())`.
Host coverage (R3 P1-2): a hosted floater that closes ITSELF is detached from its host first
(llfloater.cpp:781-784) and then flushes with itself as root; its sliders are still its descendants. A
HOST (LLMultiFloater) that closes flushes with the host as root, which covers every hosted tab through
ancestry. `LLMultiFloater::onClose` (llmultifloater.cpp:70-77) does NOT close the tabs; they are only
hidden with the host and keep no entries. `LLMultiFloater::closeAllFloaters` (:92-115) calls each tab's
own `closeFloater`, so each tab flushes itself. A floater only hidden (`setVisible(false)` without close) or minimised keeps
its entries, and the tick fires them while the floater is alive. If it is later destroyed, the dtor
cancels. Region env: the flush runs `commitDayLenOffsetChanges(true)` and clears the dirty flags, so the
panel's own hide-commit (llpanelenvironment.cpp:248) finds nothing dirty and does not send twice.

### 4.2a Close-flush re-entrancy (R3 P0)
New `LLFloater` members: `bool mCloseFlushActive = false` and `U32 mLifecycleSerial = 0`. `openFloater`
bumps `mLifecycleSerial`. In `closeFloater`, inside the `canClose()` branch, immediately before
`onClose(app_quitting)` (llfloater.cpp:841):
```
if (!mCloseFlushActive)                         // nested close: the outer call owns the flush
{
    LLHandle<LLFloater> self = getHandle();
    const U32 serial = ++mLifecycleSerial;
    mCloseFlushActive = true;
    LLSliderWheel::settleForClose(self, serial, app_quitting);   // may run arbitrary callbacks
    LLFloater* f = self.get();
    if (!f) return;                             // deleted (destroyInstance -> delete): touch nothing
    f->mCloseFlushActive = false;
    if (f->mLifecycleSerial != serial || f->isDead()) return;   // nested close / reopen / die(): superseded
}
else
{
    ++mLifecycleSerial;                         // nested close supersedes the outer one
}
// ... unchanged: onClose, close signal, hide/destroy
```
- A nested `closeFloater` from a flush callback skips the flush (the registry entries were already
  taken by the outer call), bumps the serial and runs the full close itself: one `onClose`, one close
  signal, one hide/destroy. The outer call sees the changed serial and returns without touching
  anything else.
- A reopen from a callback (`openFloater` bumps the serial) makes the outer close return, so the
  floater stays open as the callback asked. The unfired entries return to the tick.
- A delete from a callback (`LLFloaterReg::destroyInstance`, llfloaterreg.cpp:236-241) kills the handle;
  `settleForClose` stops before its next fire and the outer close returns before any member access.
  `die()` (deferred delete) is caught by `isDead()`.
- `mCloseFlushActive` is cleared only through a live handle. If the floater died, the flag dies with it.
- The `app_quitting` path fires no callbacks (cancel only), so it cannot re-enter.
The tick has no such hazard: it works on a swapped-out copy and re-resolves each handle before use.

Quit (R2 #3): `LLSliderWheel::cancelAllForQuit()` = set `sQuitting`, bump-and-drop every live entry
(cancel through the slider's method), and stop the timer. Call it in llappviewer.cpp immediately before
BOTH `gFloaterView->closeAllChildren(true)` calls (:4441 and :5686). Sliders outside floaters (Quick
Settings pulldown) are covered because the cancel goes through the registry, not the view tree. The
`closeFloater(true)` path and the tick check are a second and third layer.

Tick:
1. If `sQuitting || LLFloater::isQuitRequested() || LLApp::isExiting()`: cancel all, stop.
2. Swap the registry out into a local copy. For each entry: `s = handle.get()`; dead -> drop;
   `s->mWheelSettleGen != gen || !s->mWheelSettlePending` -> drop; `s->hasMouseCapture()` -> re-insert
   with deadline = now + 0.3 s (defer); `now < deadline` -> re-insert; otherwise fire through
   `s->fireWheelSettle(gen)` (the handle is re-resolved immediately before each fire). Re-inserting
   merges with any entry a callback added (one per slider, newest gen wins).
No raw `LLSlider*`, signal, floater or callback owner is stored or captured. A pointer exists only for
the duration of one `handle.get()` use.

### 4.2b Callback lifetime rule (R4 P1, R5 1-3)
Rule: on every slider path this design adds or touches, any call that can run foreign code is a
potential destroy point.
- `template<class T> T* LLSliderWheel::alive(const LLHandle<T>& h)`: `p = h.get()`; null -> nullptr; then
  `for (LLView* v = p; v; v = v->getParent()) if (v->isDead()) return nullptr;`, otherwise `p`. The
  check covers the object itself, its floater and any ancestor that was `die()`d (LLView is an
  LLMortician, llview.h:101, so `die()` leaves a valid but dead object and the walk is memory-safe; a
  direct delete deletes the children too, so the handle is already null). Cost: one parent walk per
  callback point, only on these paths, never per frame.
- `LLSlider* LLSliderCtrl::liveSlider(const LLHandle<LLSlider>& sh) const` (v7, a member so it can read
  `mSlider`): `s = LLSliderWheel::alive(sh)`; returns `s` only if `s && s->getParent() == this && mSlider
  == s`. It rejects a child that was deleted, `die()`d (itself or an ancestor), removed, reparented or
  REPLACED (a new `mSlider`) by a callback.
(a) Take the ctrl handle `h` AND the child handle `sh` BEFORE the first foreign call. (b) After EACH
foreign call where execution continues, re-resolve BOTH with `RS()`:
`if (!(self = LLSliderWheel::alive(h))) return; if (!(s = self->liveSlider(sh))) return;`. There is no
ctrl-only re-resolve. Every ctrl method that reads the child counts as a child access, including
`getValueF32()` (= `mSlider->getValueF32()`, llsliderctrl.h:100) and `updateText()` (which calls
`getValueF32()`), so it is always preceded by a fresh `RS()`. A live ctrl does NOT imply a live slider.
Foreign-code calls on these paths:
- the control write inside `LLSlider::setValue(F32, from_event=false, ...)` (`setControlValue`,
  llslider.cpp:130) runs control-variable listeners. Inside `setValue` itself, ONLY in the branch that
  actually writes (`!from_event && value changed`, so a no-op set or control sync costs nothing),
  take the handle before `setControlValue` and return if `alive()` fails, before
  `LLF32UICtrl::setValue`/`updateThumbRect` (:133-134);
- `LLSliderCtrl::setValue`, both overloads (llsliderctrl.cpp:224-230, :235-240), around the child call:
  ```
  void LLSliderCtrl::setValue(F32 v, bool from_event, bool precision_override)
  {
      LLHandle<LLSliderCtrl> h = getDerivedHandle<LLSliderCtrl>();
      LLHandle<LLSlider> sh = mSlider->getDerivedHandle<LLSlider>();
      mSlider->setValue(v, from_event, precision_override);   // may run control listeners
      LLSliderCtrl* self = LLSliderWheel::alive(h);           if (!self) return;
      LLSlider* s = self->liveSlider(sh);                     if (!s) return;   // = RS()
      self->mValue = s->getValueF32();
      self->updateText();
  }
  // LLSD overload: identical, with mSlider->setValue(value)
  ```
  Callers that touch the ctrl after `setValue` returns are stock and outside this rule; every caller on
  the §4.2b paths re-resolves after it (below);
- `mValidateSignal`: `onEditorCommit` :378, `onSliderCommit` :414;
- `onCommit()` (commit signal + control write): `onEditorCommit` :387, `onSliderCommit` :421,
  `applyWheelValue`;
- `mEditorCommitSignal`: :389;
- the reject paths' `setValue(saved_val)` (:395, :427), which writes the control;
- `mMouseUpSignal` in `fireWheelSettle` (nothing after it), real mouse-up (stock, no access after it).
`onEditorCommit` v7 shape (`RS()` = `if (!(self = LLSliderWheel::alive(h))) return; if (!(s =
self->liveSlider(sh))) return;`):
```
LLSliderCtrl* self = dynamic_cast<LLSliderCtrl*>(ctrl->getParent());
if (!self) return;
LLHandle<LLSliderCtrl> h  = self->getDerivedHandle<LLSliderCtrl>();       // before ANY foreign call
LLHandle<LLSlider>     sh = self->mSlider->getDerivedHandle<LLSlider>();
LLSlider* s = self->mSlider;
... parse text (no foreign code) ...
const F32 saved_val = self->mValue;
self->setValue(val, false, override);                  RS();   // control write
bool ok = !self->mValidateSignal || (*self->mValidateSignal)(self, val);   RS();   // validate :378
if (ok) {
    s->cancelWheelSettle();                            // s fresh from the RS() above, before onCommit
    self->onCommit();                                  RS();   // commit signal + control write
    if (self->mEditorCommitSignal)
    {
        const F32 v = self->getValueF32();             // child read: s fresh
        (*self->mEditorCommitSignal)(self, v);         RS();
    }
} else {
    if (self->getValueF32() != saved_val)              // child read: s fresh from the validate RS()
    {
        self->setValue(saved_val);                     RS();   // reject-path control write
    }
    self->reportInvalidData();                         // sound only, no foreign code
}
RS();                                                  // fresh, immediately before updateText
self->updateText();                                    // reads getValueF32() -> mSlider
```
`onSliderCommit` v7 shape, with the same handles on entry:
```
const F32 saved_val = self->mValue;
const F32 new_val = s->getValueF32();                  // before any callback
self->mValue = new_val;
bool ok = !self->mValidateSignal || (*self->mValidateSignal)(self, new_val);   RS();   // validate :414
if (ok) { self->onCommit();                            RS(); }                  // :421
else    { if (self->mValue != saved_val) { self->setValue(saved_val); RS(); }  // :427
          self->reportInvalidData(); }
RS();
self->updateText();                                    // :431
```
Every child access, direct or through a ctrl method that reads `mSlider`, is preceded by a fresh `RS()`
with no foreign call in between. `self->mSlider` is never dereferenced after a callback except through
the re-resolved `s`, which `liveSlider` proves IS the current `mSlider`. The `RS()` immediately before
`updateText()` is redundant after a branch that ended with `RS()`; it is kept so both branches converge
on one proven state. `applyWheelValue` (§4.1) re-resolves its own slider handle with `alive()` (ancestor
walk), so a floater `die()`d by a wheel commit stops the settle schedule. `fireWheelSettle` and
`settleForClose` resolve sliders through `alive()` too. Behaviour for live, attached controls is
identical to stock: only the early returns are new.

### 4.3 Mouse-up families (Codex audit)
Rule (v5): for every slider with a mouse-up finaliser, wheel commits are PREVIEW-ONLY. Each notch runs
only the normal commit signal (the same as one drag pixel), and the settle (§4.2) or the close flush
(§4.2/4.2a) is the SOLE final commit for a wheel gesture. No consumer may finalise per notch. Keyboard
arrows (`handleKeyHere`, which does not finalise today) and typed-text finalisation (editor commit ->
`setSliderEditorCommitCallback`, e.g. the hover floater's `onFinalCommit`) stay exactly stock.
| Family | Slider | Mouse-up does | Per-notch commit does |
|---|---|---|---|
| Hover Height floater | `HoverHeightSlider` (ctrl) | `onFinalCommit` -> `AvatarHoverOffsetZ` (listener sends) (llfloaterhoverheight.cpp:64/115-120) | `setHoverOffset(z,false)` local |
| Quick Settings | `hover_slider_bar` (bare, UNSET) | `setHoverOffset(..., true)` = server update (alpanelquicksettings.cpp:68/126-134) | local preview |
| Avatar Light Response | 4 ctrls (alfloateravatarlightresponse.cpp:111-114) | `onSliderMouseUp` | live local |
| Region environment | day length / offset (llpanelenvironment.cpp:189/194) | `commitDayLenOffsetChanges(true)` -> `updateParcel()` (:745, :761) | dirty flag + apparent time |
The multi-slider altitudes (:199) are an LLMultiSlider and are untouched. No other per-notch network
sender exists. Lightbox/Director sliders have no mouse-up signal, so they never schedule and keep their
local commits and legacy step. (Pre-existing, out of scope: the Quick Settings `hover_spinner` writes
`AvatarHoverOffsetZ` per spinner step.)

## 5. Spinners
Unchanged. A follow-up could add a one-line `guarded(this)` check in `LLSpinCtrl::handleScrollWheel`.

## 5a. Client Avatar Height (doc/CLIENT_AVATAR_HEIGHT_DESIGN.md, in progress)
- `ClientHeightSlider` ("Local offset (only you see)", -2..+2 m, inc 0.005) is a horizontal UNSET
  slider: 25 mm/notch, Shift 5 mm, Ctrl 250 mm (user-accepted). That doc's §5 "5 mm per notch" must be
  updated in its next round. The stock slider in the same floater steps 50 mm (Shift 1 mm).
- Its commit is a cheap local store write with no mouse-up wiring, so it schedules no settle. If it gains a mouse-up
  persist later, the settle and the close flush cover it.
- The stock slider is enabled only for Me, and disabled sliders get no dispatch. The floater has no
  scroll document, so neither the guard nor the container hook applies. A pending Me settle fires
  after a target switch with the Me value (correct). Closing the floater within 0.3 s flushes it
  before `onClose` (the floater is fully alive). Logging out within 0.3 s cancels it (no send).
- Answer to that doc's open question: each notch commits through the normal path, and `onFinalCommit`
  runs exactly once per gesture (settle or close flush), never during logout.
- Alignment (v5): the client-height implementation's manual per-notch `onFinalCommit` call is being
  REMOVED on that side. The stock `HoverHeightSlider` therefore keeps today's wiring unchanged: commit =
  `onSliderMoved` local preview, mouse-up/editor = `onFinalCommit`. For wheel changes the settle/close
  flush is its sole final commit (§4.3 rule), so `AvatarHoverOffsetZ` is written once per gesture, not
  per notch.

## 6. Risks
1. The first notch over a slider in a freshly opened panel adjusts it (guard arms only after a real
   scroll), as Lightbox does today. Accepted.
2. A wheel commit's `updateText` overwrites mid-edit typed text, as a drag does.
3. Camera zoom: unchanged under floaters/pulldowns. List the HUD panels with sliders in-world.
4. Accordion/list-scrolled floaters gain the guard (plain stamp rule, no disabled-slider hook).
5. The synthesised mouse-up passes (ctrl, value) like a real one; no consumer reads the mouse position.
6. Kill switch off = exact stock for UNSET (no guard, step, settle or hook); explicit ON keeps going.
7. Accepted loss: a floater DESTROYED without `closeFloater` within 0.3 s of a notch cancels the
   persist. The value was applied locally; only the final mouse-up commit is lost, the same as a drag
   interrupted by destruction today.
8. A mouse-up callback bound to an owner OUTSIDE the slider's ancestry has the same lifetime risk as a
   real mouse-up today; none of the four families does that.
9. Pre-existing, unchanged: arrow keys and Ctrl+click (with no pending settle) never fire mouse-up.

## 7. Implementation sketch
- indra/llui/llsliderwheel.h/.cpp (new): guard (`noteScroll`, `keepAlive`, `guarded`, `sliderAt`),
  pure `plainStep`/`nice125` (header-inline), settle registry (`scheduleSettle`, `settleForClose`,
  `cancelAllForQuit`, timer, `sQuitting`).
- indra/llui/CMakeLists.txt: `llsliderwheel.cpp` after `llsliderctrl.cpp` (:73); `llsliderwheel.h` after
  `llslider.h` (:192).
- indra/llui/llscrollbar.cpp `scrollByWheel`: `noteScroll(getParent())` on a real line change.
- indra/llui/llscrollcontainer.cpp `handleScrollWheel/HWheel`: the §1.3 disabled-slider hook.
- indra/llui/llslider.h/.cpp: enum, `participates/wheelEnabled/hwheelEnabled/getWheelAdjustState`,
  `setWheelAdjust` -> ON/OFF, remove `getWheelAdjust`, `applyWheelValue`, settle gen/pending,
  `fireWheelSettle`, `cancelWheelSettle`, invalidation in mouse-down (capture), mouse-up (captured,
  before the signal) and the dtor; the wheel handlers per §2.2. No `onVisibilityChange` override.
- indra/llui/llsliderctrl.cpp: ctor propagation; `wheelEnabled()` dispatch; `cancelWheelSettle()` first
  thing in `onEditorCommit`'s success block (before :387); handle taken before validation and
  `alive()` re-resolve after every callback in `onEditorCommit` and `onSliderCommit` (§4.2b).
- indra/llui/llfloater.h/.cpp: `mCloseFlushActive`, `mLifecycleSerial` (bumped in `openFloater`); the
  §4.2a block in `closeFloater` immediately before `onClose(app_quitting)` (:841-842).
- indra/newview/llappviewer.cpp: `LLSliderWheel::cancelAllForQuit()` immediately before both
  `closeAllChildren(true)` (:4441, :5686).
- settings.xml `UISliderWheelAdjust`; panel_prim_media_controls.xml `wheel_adjust="false"`.
- alscrollfit.h/.cpp, alfloaterlightbox.h/.cpp, llfloaterdirector.h/.cpp: remove WheelGuard and the
  overrides; keep `enableSliderWheel`.
- CLAUDE.md: the non-scrollbar-scroller `noteScroll` rule (§1.2).
- Lifetime: `LLSliderWheel::alive()` (ancestor walk) and `LLSliderCtrl::liveSlider()` (v7; replaces
  v6's `aliveChild`); the `RS()` discipline of §4.2b; `LLSlider::setValue` early
  return after `setControlValue` (write branch only); hardened `LLSliderCtrl::setValue` (both
  overloads); the §4.2b shapes in `onEditorCommit` / `onSliderCommit` / `applyWheelValue`.
- Process: Codex round 7 on this doc; after implementation, the Codex review loop, then checkpoint
  commit, then ONE build.

## 8. Tests
- Unit (pure, injected clock): `nice125` (0.5->1, 1, 1.9->1, 2, 4.9->2, 5, 9->5, 60->50, 1440->1000);
  `plainStep` (hover 0.05, 0..1/0.01 -> 0.01, -720..720/1 -> 10, local offset 0.025, always k*inc); guard
  (arm, 0.5 s expiry, keep-alive, owner == v, non-ancestor, dead handle); resolution/participation table
  (OFF/ON/UNSET x H/V x switch on/off -> guard/step/settle applied or stock verbatim); settle state
  machine (one entry per slider, deadline push, capturing mouse-down / captured mouse-up / editor commit
  before callbacks / dtor cancel, uncaptured mouse-up keeps, captured tick defers, fire once,
  swap-out re-entrancy, `settleForClose` fires descendants only, `settleForClose(app_quitting=true)`
  cancels, `cancelAllForQuit` blocks later schedules and ticks).
- Close re-entrancy (R3 P0): a test floater with two pending wheel settles whose mouse-up callback does
  each of the following. Run each in its own test, under ASan / the debug heap:
  (a) `LLFloaterReg::destroyInstance` on its own floater: no use-after-free; the second settle does not
      fire; `onClose` and the close signal do not run from the outer call.
  (b) `openFloater` (reopen): the floater stays open and visible; `onClose` does not run; the second
      settle fires later from the tick, exactly once.
  (c) nested `closeFloater`: `onClose` and the close signal run exactly ONCE in total; the outer call
      returns after the flush; the second settle does not fire from the flush (it is re-inserted, then
      dropped or fired by the tick per its gen; never twice).
  (d) `die()`: the outer call returns (`isDead`); no double destroy.
  (e) a callback that closes a DIFFERENT floater: the outer close completes normally.
- Callback lifetime (R3 P1-1, R4 P1), under ASan / the debug heap. For EACH foreign-code point on
  both `onEditorCommit` and `onSliderCommit` (validate signal :378/:414, `onCommit`, editor signal,
  reject-path `setValue`, and a control-variable listener reached from `setValue`), run two variants:
  the callback (i) deletes the LLSliderCtrl and (ii) calls `die()` on it. Expect no access after the
  callback, an early return (no `updateText`, no further signal), and no settle firing afterwards.
  Same for `applyWheelValue` driven by a wheel notch.
- v6 additions, same harness:
  - Child-only (R5 2): at each callback point the callback (iii) deletes ONLY `mSlider`, (iv) calls
    `die()` on ONLY `mSlider`, (v) `removeChild`s `mSlider` without deleting it, (vi) REPLACES it: the
    old child is detached and a new LLSlider is installed as `mSlider` (v7). Expect an early return
    before `cancelWheelSettle`, `getValueF32()` (editor-signal argument, reject compare), a later
    signal or `updateText()`, with the ctrl still alive and drawable and no settle. (vi) must be rejected
    by the `mSlider == s` check even though the old child may still be alive. Cover each callback point
    on both paths: control listener during `setValue`, validate, `onCommit`, the editor signal, and the
    reject-path `setValue`.
  - Ancestor (R5 3): the callback calls `die()` on the owning FLOATER (not the ctrl). Expect an early
    return at the next re-resolve and no further signal or settle.
  - `LLSliderCtrl::setValue` (R5 1), both overloads, ctrl bound to a test control whose listener (a)
    deletes the ctrl, (b) deletes only the child, (c) `die()`s the floater: no access after the child
    call (ASan); `mValue`/text are left untouched.
  - Cost: a counter on `alive()` shows 0 calls during idle frames and drag-free redraws, and a non-zero
    count only on commit/validate/settle/close and control-writing `setValue` calls.
  - Code-review additions (named in-world / debug-build checks; the llui unit harness cannot host
    LLView / LLFloater objects, it has no LLUI init, fonts or UI registry, so these are NOT unit tests):
    - `SW-ANCESTRY`: a slider with a pending settle sits under a die()d INTERMEDIATE panel; neither the
      tick nor the close flush fires it (both resolve with `alive()`), and its generation is cancelled.
    - `SW-REPLACE`: a commit/validate/control callback removes, reparents, or replaces the ctrl's inner
      slider WITHOUT deleting it: the original gets its settle cancelled (RS()/`setValue` failure path and
      `applyWheelValue`'s attached-parent / `liveSlider` check), and no settle is scheduled for it.
    - `SW-QUIT-FLUSH`: with two pending settles under one floater, the first mouse-up callback calls
      `cancelAllForQuit()` (and a variant that starts the exit): the second settle never fires (quit is
      re-checked before EVERY fire and the remaining local entries are cancelled), in the close flush and
      in the tick.
    - plus the delete/die/replace callback matrix and close re-entrancy (a)-(e) above, run in the same
      debug build.
- Preview-only finalisation (v5): Hover Height floater with an `onFinalCommit` counter. Five quick
  notches -> counter +1 (settle), 0 during the notches, and `onSliderMoved` x5; close within 0.3 s ->
  +1 total (flush), never 2; typed value -> +1 (editor path, stock); arrow keys -> +0 (stock); drag ->
  +1 at release (stock).
- Hosts (R3 P1-2): a pending settle in a hosted tab fires once when (a) the tab closes itself, (b) the
  host closes (flushed through host-root ancestry; tab not closed), (c) `closeAllFloaters` runs.
- Propagation: an LLSliderCtrl without `wheel_adjust` has an UNSET child; with true/false, ON/OFF.
- In-world: hover 0.05/notch, Shift 0.001, Ctrl 0.5; AvatarHoverOffsetZ written ONCE ~0.3 s after the
  last notch, persists after relog; wheel during drag -> one write at release; wheel then drag -> one
  write; wheel then close within 0.3 s -> one write before close; wheel then LOGOUT within 0.3 s -> no
  hover send and no `updateParcel` (log check); typed value after a wheel -> one write; wheel at range
  end -> no write; Quick Settings / region env / Avatar Light Response once per gesture; scrubber
  ignores V/H wheel; kill switch off -> hover slider dead, H-wheel stock, UNSET sliders in scrollers
  unguarded (stock), Lightbox still wheels; Local offset 25 mm/notch; stock slider disabled for
  non-Me targets.
- Lightbox/Director: V scroll through enabled AND disabled sliders keeps scrolling, including spinning
  parked at the end over a disabled slider; an H-wheel right after a scroll is swallowed over enabled and
  disabled sliders; fine/Shift on adjust_environment; Twist step unchanged.
- Regression floaters: Ultimate Diopter, Cine Light Rig, Preferences (Graphics, Graphics Advanced,
  Sound), Appearance shape/skin (accordion), Build tools, Environment editors, Region environment,
  Avatar Light Response, Quick Settings, Volume/Presets pulldowns, Camera floater, media volume, World
  map zoom, FS Poser, Particle editor, Ghost Studio, Prism Manager, Weather, Lens Gaze.

## 9. User decisions (2026-10-05)
1. ACCEPTED: the guard covers any enclosing scroller (the §1.4 nested row is the only Lightbox/Director
   delta).
2. ACCEPTED: UNSET sliders use the default nice125 step; Local offset = 25 mm/notch (Shift 5 mm).
