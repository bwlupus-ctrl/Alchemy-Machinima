# Adversarial review WP2 R1 (Director kit, commit 3661c636e42)

Reviewer: Opus, independent context, 2026-10-10.
Scope: `git show 3661c636e42`, which covers alkitshell/bar/popover/search/help/presets/history, ALRigRimController, ALPanelRigRim, the alkitpage/card/row deltas, llfloaterdirector, alpanelcinelightrig, llviewerwindow, llviewerfloaterreg, settings.xml and CMakeLists. The concurrent WP3 generator job (3f9fdbeba09) was ignored, except that I read the generated XUI to check names that the C++ looks up.

**VERDICT: FIX-NEEDED.** 1 BLOCKER, 8 MAJOR, 20 MINOR.

## What holds (verified, no action needed)

**v1 postBuild order is unchanged.**
- Codex did not split v1 into binders. It returns early into `postBuildV2()` (`llfloaterdirector.cpp:296-301`) and leaves the v1 body statement-for-statement intact.
- v1 gains only three things, and none of them changes behaviour:
  - `if(mShell)` in the cast commit (:393);
  - a `findChild("kit_ui_ms")` that returns null (:718);
  - a one-off `findChild<ALKitShell>` traversal.
- In v1, the `onClose`, `draw`, `handleKeyHere`, `handleMouseDown`, `handleRightMouseDown` and `handleScrollWheel` additions are all gated on `mIsV2`/`mShell`.
- `hasAccelerators()` returns `mIsV2`, so it is false in v1. `LLView::hasAccelerators` defaults to false and LLFloater does not override it.
- `handleUnicodeCharHere` in v1 is a pure pass-through to the base.

**v1 refreshers are null-safe.**
- `refreshGroupControls` has a chrome/Move split guard (:2989, :3031).
- `refreshSubjects` (:4142) and `refreshPrismDependents` (:4200) gained guards.
- Move, Path, ActorStyle, Animate and Gaze were already null-safe.
- `addLauncherCard` now uses `findChild` and skips missing cards. All of its cards exist in v1, so v1 behaviour is the same.

**ALPanelCineLightRig v1 is identical.**
- The same `getChild` set remains, minus `cine_rig_rim_preset`, `cine_rimq_status` and `cine_rimq_enable_tag`. Those three are now `findChild` calls in `ALRigRimController::attach`.
- The per-frame calls are the same:
  - `mRim.tickFrame()` sits exactly where the status block was;
  - `mRim.tickRefresh()` sits where the Tron block was;
  - `onReveal()` replaces the gate reset and `forceDue`.
- The preset table is byte-identical, and so are the apply function, the combo population ("Choose preset..." then "%d  %s") and the snap-back.
- `applyRigRimPresetByName` remains public static and forwards to the controller.
- `setControlVisible` falls back to plain `setVisible` diffing outside a kit row.

**Engine slot writes are suspended.** `onAnchorSelected` wraps `setSelectedSlot` in a suspend and then calls `barrier("rig slot changed")`.

**The `noteKey()` hook is placed and priced correctly.** It sits in `llviewerwindow.cpp:3334`: after the `LLSetKeyBindDialog` early-out, before keyboard-focus dispatch and the menus. It costs one static `LLTimer::getTotalSeconds()` store per key-down, with v2 on or off. That is negligible.

**Text entry is excluded from Director undo, per R1 M3.**
- Ctrl+Z/Y walks the focus ancestors for `LLLineEditor`, `LLTextEditor` and `LLSpinCtrl` (:1757-1767).
- If focus is in text entry, nothing is consumed: the key falls through to `LLFloater`, then back to the viewer menus and the editor's own undo.
- `/` is handled in `handleUnicodeCharHere` (:1690) with the same text-entry exclusion.

**Deferred work and timers are lifetime-safe.**
- Every deferred callback, button lambda, idle batch, debounce timer, notification callback and mode/context adapter captures an `LLHandle`.
- The debounce `LLEventTimer` is deleted in `~ALKitShell` through its tracker weak pointer, which uses a no-op deleter, so there is no double free.
- `GestureTimer` is `unique_ptr`-owned and returns false, so `updateClass` never deletes it.
- Menus use handles.

**Draw paths stay light.**
- `LLFloaterDirector::draw` (v2): `llformat` runs only on the 2 Hz clock, through `setTextIfChanged`. This is allowed by section 3.7.
- `ALKitShell::draw` and `ALPanelRigRim::draw` do no lookups, no allocation, and each opens a named Tracy zone.

**The `menu_viewer.xml` item was correctly omitted.** Section 3.16 ("P0 adds no `menu_viewer.xml` item") and ledger R1 M15 forbid it. The orchestrator note that says the spec asks for one is wrong.

**APIs and notifications exist.** No invented APIs were found: `LLFile::mkdir/isdir`, `LLTextBase::Params::wrap`, `LLEventTimer::run_after`, `LLTabContainer::getPanelByIndex/getCurrentPanelIndex/selectTabPanel`, `translateIntoRect` and `LLSD::with`. The `GenericAlertYesCancel`, `SystemMessageTip` and `EnableGBufferAvatarTagConfirm` notifications exist.

---

## BLOCKER

**B1. Building a page rewrites the user's Easy/Advanced setting from stale page state, and bypasses the mode adapter.**
- `ALKitPage::saveState` persists `page["mode"]` (`alkitpage.cpp:830`).
- `restoreState()` runs at the end of `ALKitPage::postBuild` (:501) and calls `setModeIndex(saved,false)` (:846). That path ends in `mModeSwitch->writeBoundValue(on)`, a real write to `mode_setting` (`CineLightRigEasyMode`).
- The kit_page's postBuild runs before its parent `ALPanelCineLightRig::postBuild`, where the `cine_rig_easy` adapter is registered (`alpanelcinelightrig.cpp:927`). So `ALKitModeAdapters::get` returns null, and the write skips `applyEasyMode`/`onEasyModeCommit`. That is exactly the R1-B3/R2-M-R2 failure class.
- The write is also outside `ScopedSuspend`. `watchSettings` already ran at :498, so if the build was triggered by a key (Alt/Shift+PgUp within 0.5 s, with focus in the Director), the flip is recorded as an undo step.
- In practice: toggle Easy in the standalone Cine Light Rig floater, reopen v2, and the Director reverts it.
- Section 3.5 defines page state as chip, card open/More and who pin. The mode belongs to the setting.
- Fix: remove `page["mode"]` from `saveState` and the mode branch from `restoreState`. Mode is always derived from `mModeSwitch->isOn()` through `syncEffectiveMode()`.

## MAJOR

**M1. The legend popover can never open.**
- In `floater_director_v2.xml:142`, `legend_panel` is a `kit_popover anchor="btn_help"`.
- `ALKitPopover::postBuild` (`alkitpopover.cpp:36`) connects `btn_help` to `show()`.
- `bindChrome` (`llfloaterdirector.cpp:835`) then connects a second slot to `onToggleLegend()`. Because the panel is now an ALKitPopover, that slot hides a visible popover (:1490).
- Slots run in connection order, so each click is show() followed immediately by hide().
- Fix: in v2, do not connect `mHelpBtn`. Make the popover's own anchor slot a toggle (`getVisible()? hide() : show()`). Generally, the auto-wired anchor slot must be the only opener.

**M2. Each ALKitLazyPage adopts into the shell's private ScrollFit controller, not the floater's.**
- `ALKitShell::scrollFit()` returns `mScrollFit` (`alkitshell.h:57`).
- `setScrollFit()` stores `mExternalScrollFit`, which nothing ever reads (`alkitshell.h:58`).
- `ALKitLazyPage::ensureBuilt` calls `shell->scrollFit().adoptPage(...)` (`alkitlazypage.cpp:30`). So `wrapTabPanel` records stretcher minimums in the shell's `mMins`, while `LLFloaterDirector::mScrollFit` refits on tab commit and `onOpen` (`llfloaterdirector.cpp:1379`) with a different `mMins`.
- Legacy hosts are wrapped by one controller and fitted by another. Section 3.17.1 requires one controller.
- Fix: `ALScrollFit::Controller& scrollFit(){ return mExternalScrollFit ? *mExternalScrollFit : mScrollFit; }`, and delete the private one if nothing else uses it.

**M3. Dance and Exposure are never filled in v2.**
- `bindCameraPage` looks for `dance_tab_logic` and `exposure_tab_logic` (`llfloaterdirector.cpp:1203-1212`). No generated page has either name; the dance root is `kit_page "dance_tab_content"`. So `mDancePage` and `mExposurePage` stay null, and every `fillPage` call (:1209, :1217, :1392-1393) does nothing.
- Section 3.17.1 says: `fillPage(view->getParent(), view, 30)`, where the parent is the legacy host.
- Fix: `if (LLView* v = root->findChild<LLView>("director_dance_ball_panel")) { mDancePanelView = v; mDancePage = dynamic_cast<LLPanel*>(v->getParent()); ALScrollFit::fillPage(mDancePage, v, 30); }`. Do the same for `director_exposure_panel`.

**M4. Every wheel tick becomes its own undo step.**
- `handleScrollWheel` calls `beginGesture(WHEEL, …)` (:1691) on every tick.
- `beginGesture` always calls `finishGesture()` first (`alkithistory.cpp:60-62`), so wheeling a slider 20 clicks pushes 20 steps and eats a tenth of `MAX_STEPS`.
- Section 3.11 defines a wheel gesture as "a wheel event plus 500 ms", with the gesture closing after settle. One wheel run is one Step.
- Fix: in `beginGesture`, if `mOpen && mOrigin==o && o==WHEEL && mPage==p`, only extend `mCloseAt` and return. Optionally do the same for KEY (see m3).

**M5. Combo-box picks are not undoable unless they happen within 350 ms.**
- A dropdown list is the focus manager's top control. `LLViewerWindow::handleAnyMouseClick` routes the item click to it before the view hierarchy, so `LLFloaterDirector::handleMouseDown` never sees it.
- The gesture opened by the combo-button click has settled 350 ms after release, so the commit's setting write fails both admission rules in `captureSetting` (`alkithistory.cpp:133-139`): there is no pointer gesture and no recent key.
- This affects every combo row (Tron mode, selector combos above 6 options, Rim/Rig enums). Strictly, it is a gap in the spec rule, but WP2 owns the recorder.
- Fix: add `ALKitHistory::notePointer()` beside `noteKey()`. Call it at the top of `LLViewerWindow::handleAnyMouseClick` (a static store, so the cost is the same). Admit when keyboard focus is inside the owner and `now - max(key, pointer) ≤ 0.5 s`.

**M6. Director scene load and Rig reset-all are not suspended or barriered.**
- Section 3.11 ("Who suspends") names the scene apply and `resetAllToSelf`.
- `LLFloaterDirector` scene load calls `ALCineLightRigManager::applySceneData` with no `ScopedSuspend` or barrier (`llfloaterdirector.cpp:2431`).
- `ALPanelCineLightRig::resetAllCallback` (`alpanelcinelightrig.cpp:1873`, `manager.resetAllToSelf()` at :1898) has none either.
- Inside an open gesture, both get recorded as a partial setting-only Step. Their model state (anchors, object target, shaft/hero flags) is not captured, so Ctrl+Z leaves a half-restored rig.
- Both files are in WP2 scope.
- Fix:
  - Wrap the whole scene load body in `ALKitHistory::ScopedSuspend`, then `if(mShell) mShell->history().barrier("scene loaded")`.
  - In `resetAllCallback`, wrap the reset loop and `resetAllToSelf` in a suspend, then call `ALKitHistory::globalBarrier("rig reset")`.

**M7. The Who flow does not meet section 3.5 or the WP2 acceptance.**
- WP1's who-chip slot (`alkitpage.cpp:377-381`: `mWhoCombo->setVisible(true); showList()`) is still connected.
- `ALKitShell::pageBuilt` (`alkitshell.cpp:522-556`) adds a second slot that reparents the source combo into a 40 px popover and shows it. After a pick, WP1's commit hides the combo (:386), leaving an empty open popover.
- The row still owns layout of the reparented control.
- There is no "↺ Follow rail (Name)" entry and no pin. The cast-selection slot (`alkitpage.cpp:392-404`) always overrides an explicit pick.
- `who_pin` is never written. WP2_STATUS says it cannot be persisted "because WP3 emits no who-pin control". That is false: `DirectorV2PageState` is a WP2 setting, and `saveState` already writes it.
- Fix:
  - Do not reparent the combo. Disconnect or replace WP1's chip slot.
  - Build the popover as a list: "Follow rail (Name)", then one button per `mWhoCombo` item. A pick runs `setValue` and `onCommit`, then stores `pin` in page state. "Follow rail" clears the pin.
  - The cast-selection slot returns early when the page is pinned.

**M8. Popover anchor ownership is fragile beyond M1.**
- `ALKitPopover::postBuild` appends a `show()` slot to any XML anchor's commit signal (`alkitpopover.cpp:36`), whether or not C++ also wires that control.
- `btn_arm` and `btn_hotkeys` are fine today, but every future anchor that C++ already handles will double-fire, as M1 does.
- Section 3.7 says "anchor (widget name, resolved once in shell postBuild)". It says nothing about stealing the commit signal.
- Fix: resolve the anchor only for positioning. Let the owner (shell or floater) call `toggle()`. Alternatively, add an explicit `open_on_click="true"` param and use it only where nothing else wires the control.

## MINOR

- **m1. The gesture timer never stops.** `GestureTimer` keeps ticking at 10 Hz after the first gesture until a barrier: `tick()` always returns false and only `barrier()` resets `mTimer` (`alkithistory.cpp:31`). Section 3.11 says the timer exists only while a gesture is open. Fix: when `tickGesture()` closes, `mTimer.release()` and return true (`updateClass` deletes it), or reset it on the next idle.
- **m2. Undo labels and custom-entry order.**
  - Step labels are setting names, because `captureSetting` passes `name`. Section 3.11 wants the row label ("Rig › Lights › EV"). Resolve it through `findRowBySetting` on the gesture page.
  - Custom entries replay forward on undo. Reverse them on undo.
- **m3. Keyboard gestures split.** A KEY gesture never extends `mCloseAt`, so holding an arrow on a slider for 2 s makes about 6 steps.
- **m4. Gesture page attribution.**
  - `beginGesture` uses `currentPageKey()`, not `pageKeyAt(x,y)` (section 3.17.4).
  - No `"chrome"` watch set is ever registered, so the chrome branch in `captureSetting` is dead.
- **m5. Search query loop allocates.** `ALKitSearch::query` copies and lower-cases `entry.label` for every entry and every query token (`alkitsearch.cpp`, label-prefix block). Precompute `label_lower` in `tokenizeBatch`.
  - Also missing: the `KitSearch query "<q>" <ms> ms <hits>` self-test log.
  - Enter ignores the Up/Down highlight and always jumps to the top hit. Use `mSearchSelection` when the user has moved it.
- **m6. Page lookup per mouse event.** `currentKitPage()` builds a string, runs `findChild` and does a DFS `dynamic_cast` walk. It is called on every Director mouse-down and wheel tick via `currentPageKey()` (:1679-1691). Cache a handle to the current kit page, updated on tab commit and `pageBuilt`.
- **m7. ALKitSnapshotStore robustness.**
  - `names()` re-reads the file on every `list()`/`refresh()`.
  - `save()` ignores a failed `read()` and overwrites a corrupt or half-edited store with only the new preset, which is silent data loss.
  - The write is non-atomic. Write `.tmp`, then rename.
  - `load()` applies any key found in the file. Restrict it to `mSettings`.
- **m8. The Rim provider's setting list is hard-coded and has drifted from the page.**
  - `alpanelrigrim.cpp:rimSettings()` lacks `CineRigRimActorsOnlyAlpha`, which is on the page.
  - It includes `CineRigRimWrapClamp`, which is not a page row, so its change on a user-preset load is not undoable.
  - Derive the list from the Rim page rows minus the 4 exclusions, or at least fix it and add an assert.
- **m9. Controller duplication and dead code.**
  - `ALRigRimController::presetNames()` duplicates the table's names. Build it from `RIG_RIM_PRESETS` with a `static_assert` on the count.
  - `onPresetSelected()` is dead.
- **m10. Preset strip names.** Section 3.5 requires `<P>_preset`, `<P>_preset_save` and `<P>_preset_delete`. Codex creates `<P>_presets` with generic `kit_preset_combo/save/delete` children, so the names repeat across pages.
- **m11. Help text.**
  - The kicker and title show raw keys (`light_group/cine_light_rig_tab`). `showPage` uses the key as the title, and `showCard` leaves the kicker empty. Spec: "Group › Page" labels.
  - The strip is a plain `LLTextBox` without ellipses. The spec calls for a custom-drawn, ellipsized line.
- **m12. Legacy reveal does not scroll.** The `navigateTo` legacy path only draws a highlight. Add `scrollToShowRect` on the nearest `LLScrollContainer`.
- **m13. Rect bump.** It can shrink a dimension that is already at or above the default, and it is not clamped to screen size; `translateIntoRect` only moves. Use `llmax(cur, default)` and clamp to `gFloaterView`.
- **m14. Half-bound pages.** `bindActorStylePage` and `bindAnimatePage` assign members before validating, so a partial page leaves `mActorStyleTarget`/`mAnimList` non-null with unwired, null siblings. Also, `mAnimateHeader`, `mAnimHint` and `mLocoText` are not validated. Make each binder all-or-nothing (null on failure).
- **m15. Gaze selection.** A Gaze panel bound after `onOpen` gets no initial selection, because `mLastGazePush` was already set. Push the selection in `bindCameraPage`.
- **m16. v1 was not refactored into binders (section 3.17.1).**
  - `bindChrome` is a second copy of the v1 chrome block, which is a drift risk (it is safer for v1 today).
  - The unwrapped-page list at :81 is hand-maintained. Derive it from the shell's tab tree.
  - Add a comment tying the two copies together.
- **m17. Resize cost.** `layoutChrome` and `updatePips` run on every reshape (each drag-resize step). They call `getTokens` and `gSavedSettings.getBOOL` by name, plus an un-diffed `setLabel` on about 45 buttons. Diff the labels, and recompute pips only on the watch signal.
- **m18. Settings read once or not at all.** `DirectorV2CastRosterOpen` is never read or written. `KitShowUIMs` is read only at build.
- **m19. Esc order.** The "Esc clears, then blurs" step is missing. A second Esc in an empty search field can fall through to CUT while the transport is live.
- **m20. Bar visibility churn.** `ALKitBar::layoutChildren` re-shows and then re-hides every collapsible child on every reshape. Compute first, then apply diffed visibility. Separately, the two text-entry walks in `handleKeyHere` and `handleUnicodeCharHere` should become the one `isTextEntryFocus()` helper the spec requires.

## Note for WP4 (not counted)

The `cine_rig_easy` adapter calls `mEasyModeToggle->setValue(); onEasyModeCommit()` for every origin. The guarded `applyEasyMode(desired, origin)` refactor from sections 3.5 and 3.18 is not done. It must land before WP4's "passive (search) mode drive" trace.
