# Director kit WP4: Opus adversarial review, round 1

Scope: the WP4 part of `af5acdd6529` (`alkitrow.*`, `alkitpage.*`, `alkitcard.*`, `alkitlazypage.*`, `alpanelcinelightrig.*`, `gate_policy.json`, `WP4_STATUS.md`, `WP4_TRACES.md`). I also read the code these depend on: `alkitswitch.cpp`, `alkithistory.cpp`, `alkitpresets.cpp`, `alkitbinding.cpp`, `allightrigmap.cpp`, `alpanelrigrim.cpp`, `alrigrimcontroller.cpp` and `alcinelightrigmanager.cpp`. I checked against spec sections 3.3, 3.4, 3.5, 3.11, 3.18, 5.1, 5.2 and WP4. The WP5 self-test files are reviewed separately.

## Verdict: **FIX-NEEDED**: 1 BLOCKER, 3 MAJOR, 6 MINOR

---

## 1. Trace walk (`WP4_TRACES.md`)

| # | Trace | Result | Evidence |
|---|---|---|---|
| 1 | Drag Key in map → dial value | **TRUE** | `allightrigmap.cpp:487-502` opens one MOUSE gesture on the page key and writes `DirectorV2SelectedLight`. Selector cards resync from the signal: `alkitcard.cpp:403-407` → `syncSelector` `:441-455`. The Fixture mirror is at `alkitrow.cpp:173-182`: it sets the index plus `onCommit()` under `ScopedSuspend` and `mSyncingMirror`, then `onFixtureRoleCommit` runs (`alpanelcinelightrig.cpp:1208-1215`). The combo is still empty at row postBuild; the idle one-shot at `alkitrow.cpp:184-195` fixes that. `applyDrag` (`allightrigmap.cpp:568-577`) writes Yaw/Pitch. The dials use `control_name="CineLightRigKeyYaw/Pitch"` (page XML 204/207). |
| 2 | Toggle Easy → cards | **FALSE** | See B1. The USER-origin `applyEasyMode` does not own the transition. Its own `setBOOL` re-enters a PASSIVE adapter call through the mode switch's flip signal, and that call performs the transition first. For an Easy-native rig, `normalizeSelectedForEasy(true)` is then skipped. For a non-native rig, the preference is written back passively in the middle of the transition. |
| 3 | Rim preset → one undo step → Ctrl+Z | **TRUE** | `alkitpresets.cpp:313-318` calls `beginGesture(MOUSE,"Apply preset")` and then `provider->apply`. `applyRigRimPreset` (`alrigrimcontroller.cpp:63-92`) writes 23 settings, and all of them are in the Rim page's watch list. Each one is recorded through `captureSetting` (`alkithistory.cpp:155-170`: pointer = open MOUSE gesture) and coalesced by `addEntry` `:179-185`. `undo` `:313-323` → `replay` `:246-281` under suspension. No context is registered for the Rim page, so the context check is a no-op. |
| 4 | Lights card reset → active option only | **TRUE** | `alkitcard.cpp:262-270`. `rowBranchVisible` (`:30-34`) rejects rows whose `ALKitOption` ancestor was hidden by `syncSelector` (`:448`). Enable switches are not rows. Who and preset rows are excluded. "Reset all" (`:284-294`) is the separate path. |
| 5 | Page reset asks first → one undo step | **TRUE** | `alkitpage.cpp:812-827`: a `GenericAlertYesCancel` notification with a handle-guarded callback. Cancel writes nothing. Yes opens one MOUSE gesture, and every row reset (setting- or widget-recorded) coalesces into it. (The modal alert instead of the spec's popover is m6.) |
| 6 | Rail selects Mara → slot 2 → undo not polluted | **TRUE** (but see M3) | `llfloaterdirector.cpp:396-398` → `ALKitShell::setCastSelection` → `alkitpage.cpp:394-401` → `cine_rig_slot` adapter (`alpanelcinelightrig.cpp:935-944`, Subject B → 2) → combo `setValue` + `onCommit` → `onAnchorSelected` `:1423-1438` (`ScopedSuspend` around `setSelectedSlot`, then `barrier`). The `cine_anchor` widget entry that the row records first is discarded by the barrier. Nothing fragmentary survives. |

Dynamic visibility and header presets:
- The card probe (`alkitcard.cpp:756-769`) is O(rows) over member bools and detects the clip, shadow-hint, gobo and enable-tag edges.
- `card_preset` controls are reparented, not recreated (`alkitcard.cpp:166-175`), so the callbacks the panel installs later survive.
- Both claims are **TRUE**.

---

## 2. Findings

### B1 (BLOCKER): passive mode reconciliation writes `CineLightRigEasyMode`. This clobbers the persisted Easy preference, stops the standalone v1 floater from entering Easy, and skips explicit normalization. The write is reachable from `draw()` through a signal, which is why the linter does not see it.

**Mechanism:**
- `ALKitPage`'s mode-switch flip callback (`alkitpage.cpp:323-333`) now calls the panel adapter with `PASSIVE`.
- The adapter is `applyEasyMode` (`alpanelcinelightrig.cpp:2100-2109`). It writes the setting whenever the cached value differs from `desired`, before any re-entrancy guard; the `mSyncingEasyControls` check is only in `syncEasyModeState` (`:2113`).
- The callback then calls `mModeSwitch->setValue(effective)`. `ALKitSwitch::setValue` → `noteState` → `mFlipSignal` (`alkitswitch.cpp`), so it re-enters itself.
- `mModeSwitch` is the same object as the panel's `mEasyModeToggle`: the page creates `cine_easy_mode` and the panel `getChild`s it at `:669`.
- So every `syncEasyControls()` → `setValueIfChanged(mEasyModeToggle, mEasyModeActive)` (`:2289`) fires the flip → adapter(PASSIVE) → `gSavedSettings.setBOOL("CineLightRigEasyMode", effective)`.

**Concrete failures (the preference is ON and the look is not Easy-native):**

1. **Page build clobbers the preference.**
   - `ALPanelCineLightRig::postBuild` → `syncEasyModeState(..., RESTORE, true)` (`:959-960`) → `syncEasyControls` → toggle to false → flip → adapter → `setBOOL(false)`.
   - Merely opening the Director Rig page persists Easy = OFF, and the standalone v1 floater also drops to Advanced.
   - The same thing happens on any draw-time demotion: scene load, preset, or a slot switch to a non-native look. The path is `draw` → `syncEasyModeForSelected` → `syncEasyModeState` → `syncEasyControls` → flip → write.
   - This reverses the v1 contract: "Record the user's preference even if this particular look cannot enter Easy, so other (Easy-native) instances still open in Easy".
2. **The v1 floater cannot enter Easy while the v2 Rig page exists.**
   - The v1 `cine_easy_mode` checkbox is `control_name`-bound (`panel_cine_light_rig.xml`), so a click writes `true`.
   - The v2 switch flips, its passive adapter computes effective = false (not native), then calls `setValue(false)`. The nested flip writes `setBOOL(false)` (the cache is already `true`; `LLControlCache` uses slot group 0, which fires first).
   - The v1 checkbox unticks before its commit callback runs. `onEasyModeCommit` then reads `false` and does nothing.
   - Pre-WP4 this entered Easy and folded Key EV. This is a v1 regression whenever DirectorUIV2 is on.
3. **v2 USER entry is preempted.**
   - In `setModeIndex(i, true)` → `applyEasyMode(true, USER)`, the `setBOOL(true)` synchronously runs the flip → adapter(PASSIVE) path before the USER `syncEasyModeState`.
   - Native rig: PASSIVE sets `mEasyModeActive = true` without normalizing. The USER sync then early-returns at `:2120`, so `normalizeSelectedForEasy(true)` never runs: Key is not forced on and near-bucket Rim/BG EVs are not snapped. The v1 toggle still does both, so the two front ends diverge.
   - Non-native rig where `normalizeSelectedForEasy` refuses (|Master + Key| > 16): the nested passive write leaves the preference `false`. Yet history recorded `CineLightRigEasyMode` false→true (the outer write was not suspended; the page now watches the mode setting, `alkitpage.cpp:199`). That leaves a Step whose undo is a no-op and whose redo is flapped straight back.
4. **The status claim is false.** `WP4_STATUS.md` says "setting-signal/draw/reveal reconciliation never writes it". Spec 3.5 says the panel "must not write the setting back from passive synchronization". `ui_draw_lint` reports 0 only because the write is behind `ALKitSwitch::mFlipSignal` → `std::function` adapter, which a static scan cannot follow. The fix is **not** genuinely event-driven.

**Fix (all four parts):**
- (a) At the top of `applyEasyMode`: `if(mSyncingEasyControls||mApplyingEasyMode)return mEasyModeActive;`. Write the setting **only** for `USER` and `SEARCH`. `PASSIVE`/`RESTORE` delegate to `syncEasyModeState` and never write.
- (b) For USER, set a member `mApplyingEasyMode = true` around the `setBOOL`. Make `mEasyModeConnection` and the adapter return early while it is set, then run `syncEasyModeState(desired, USER)` so `normalizeSelectedForEasy(true)` always runs on explicit entry. Keep the written preference even when normalization refuses (v1 semantics).
- (c) The page flip callback must not call the writing adapter or `mModeSwitch->setValue` re-entrantly. Use a read-only reconcile instead: adapter origin `PASSIVE`, which after (a) cannot write, plus `syncEffectiveMode()`. Or add an `mInModeFlip` guard.
- (d) Make `setModeIndex(i, false)` pass `SEARCH`, not `PASSIVE`, so only search reveal may write.
- Add self-test assertions:
  - building the Rig page with pref = 1 on a non-native look leaves `CineLightRigEasyMode == 1`;
  - with the v2 page built, the v1 checkbox enters Easy on a non-native look, and Key EV is folded into Master;
  - v2 USER entry on a native rig with Key off turns Key on.

### M1 (MAJOR): v1 passive Easy entry changed (`normalizeSelectedForEasy(false)` removed)

- Old `syncEasyModeForSelected` (HEAD~1) called `normalizeSelectedForEasy(false)` whenever the passive path activated Easy. That folds a ≤0.01 Key EV into Master, zeroes Key EV and forces `CineLightRigKeyOn = true`.
- New `syncEasyModeState` (`:2127`) normalizes only when `explicit_entry`. In v1 (DirectorUIV2 OFF), first show, slot switch or scene load into an Easy-native look with Key off now stays Key-off in Easy.
- That is not v1-identical (spec 3.5 asks for a refactor, not a behaviour change). The likely reason: these were the draw-path writes that `ui_draw_lint` flagged, and they were removed rather than justified.
- Fix: `if(mEasyModeActive&&!normalizeSelectedForEasy(explicit_entry))mEasyModeActive=false;`. It is edge-only (inside the changed-state branch), so record it as an edge-only reconcile exemption with a citation. Alternatively, move the passive reconcile to the setting/slot signals.
- Separately, the new passive path also calls `syncEasyControls()` immediately on the edge. Before, that happened on the 10 Hz `updateDerivedStatus`. This is benign, but note it in the parity record.

### M2 (MAJOR): search reveal of a card shown in both modes switches Advanced → Easy (and writes the preference)

- `ALKitPage::revealCard` (`alkitpage.cpp:787-794`) calls `setModeIndex` for the **first** mode that lists the card.
- `light_map` and `lights` are in both Easy and Advanced (page XML lines 5-6), so a hit on a Lights/Map row ("Key yaw", "Pitch", "Shaft", the map) while in Advanced calls `setModeIndex(Easy, false)`.
- That goes adapter → `applyEasyMode(true)` → writes `CineLightRigEasyMode = 1` and, on a native rig, swaps the user into Easy. `revealChip` (`:856-868`) inherits the same problem.
- This breaks the WP4 item "mode proxy driving cine_easy_mode passively (search)", and spec 3.5 "does not later switch the user back" assumes the switch is necessary.
- Fix: only switch when the card is not visible in the current mode: `if(listedInMode(c,mModeIndex)||!listedInAnyMode(c))` skip `setModeIndex`.

### M3 (MAJOR): a rail click on the already-selected subject wipes the whole Director undo stack

- Every rail commit (`llfloaterdirector.cpp:396-398`, `:861-864`) re-commits `cine_anchor` (`alkitpage.cpp:400`). `onAnchorSelected` (`alpanelcinelightrig.cpp:1432-1438`) calls `history.barrier()` unconditionally.
- `setSelectedSlot` is a no-op for the same slot (`alcinelightrigmanager.cpp:700`), and `barrier` clears **all** pages' undo/redo (`alkithistory.cpp:129-138`).
- So clicking yourself or the current subject in the rail, which is routine navigation, destroys undo for Atmosphere, Rim and every other page.
- Fix:
  - in the cast callback, skip when `mWhoCombo->getValue()==value`;
  - in `onAnchorSelected`, `if(value==(S32)manager.selectedSlot()){syncObjectTargetControls(true);return;}` before the suspend/barrier.
- This keeps trace 6 TRUE without over-clearing.

### MINOR

- **m1. The `cine_rig_slot` history context is dead code.**
  - `alpanelcinelightrig.cpp:945-948` registers context key `"cine_rig_slot"`, but `ALKitHistory::currentContext` is looked up by the Step's **page key** (`alkithistory.cpp:82, 240-243, 249`). The page's `undo_context` param is parsed (`alkitpage.cpp:115`) and never used.
  - The effective context comes from the WP2 page-key registration (`:924-928`), so behaviour is right, but `WP4_STATUS` overstates it.
  - Fix: either have `ALKitPage` pass `mUndoContext` to history (map page → context key), or delete the dead registration.
- **m2. `WP4_STATUS.md` is inaccurate.**
  - "never writes it" on passive reconciliation is false (B1).
  - "Kept ... slot history barrier" hides M3.
  - Correct both after the fix.
- **m3. Per-option More state.** `restoreOpenState` now `std::fill`s every option's More bit from the single saved `moreOpen()` of the active option (`alkitcard.cpp:719`). Per-option state (spec 3.4: "For a selector it is tracked per option") is not persisted and is homogenised on restore. Fix: persist `more` per option value.
- **m4. `ALRigRimController::tickFrame` bypasses the row helper.** It uses `setVisibleIfChanged(mEnableTag, ...)` (`alrigrimcontroller.cpp:182`). Spec 3.3 says extracted code uses `ALKitRow::setControlVisible`. The probe still catches it, so this is cosmetic and costs one frame of latency.
- **m5. Kit header in a v1 header.** `alpanelcinelightrig.h` now includes `alkitbinding.h` only for `ALKitModeAdapters::EOrigin`, which couples a kit header into a v1 header. Acceptable, but an `S32`/own enum in the panel would keep v1 headers kit-free.
- **m6. Modal alert, not popover.** Page reset uses a modal `GenericAlertYesCancel`; spec 3.5 calls for a confirm popover anchored to `<P>_reset_page`. The behaviour is correct; the UI form differs.
- **m7 (STYLE). Kit files violate repo style.** They use crammed one-line bodies/headers, `if(`/`for(` without spaces and multi-statement lines, which do not match the LL/Alchemy brace-per-line style in the rest of `newview` (counts are no-space `if(`/`for(` lines; `inline-body` = `{...;...}` on one line):
  - **Headers:**
    - `alkitrow.h` (line 16: 785 chars)
    - `alkitcard.h` (line 16: 1285 chars)
    - `alkitpage.h` (1109 chars)
    - `alkitlazypage.h`
    - `alkitbinding.h`
    - `alkitpresets.h`
    - `alkitshell.h`
    - `allightrigmap.h`
    - `alkithelp.h`
    - `alkitswitch.h` (1 inline)
    - `alkitmath.h` (8 inline bodies)
  - **Sources:**
    - `alkitcard.cpp` (137 no-space `if(`/`for(`; crammed lambdas ~264-300, 392-397, 403-407, 684-689)
    - `alkitpage.cpp` (130; e.g. 394-406, 492-495)
    - `alkitshell.cpp` (146)
    - `alkitrow.cpp` (77; 143-195)
    - `allightrigmap.cpp` (77; e.g. 197-200)
    - `alkithistory.cpp` (45)
    - `alkitbinding.cpp` (31; 345-361)
    - `alkitpresets.cpp` (29 + 5 inline)
    - `alkithelp.cpp` (23 + 5 inline)
    - `alkitpopover.cpp` (10)
    - `alrigrimcontroller.cpp` (15)
    - `alpanelrigrim.cpp` (7)
    - `alkitlazypage.cpp` (5)
  - The WP4 additions to `alpanelcinelightrig.cpp` (`:921-962`, `:2100-2135`) use the crammed style inside a file that is otherwise spaced/braced.
  - Already style-clean: `alkitbar.cpp`, `alkitsearch.cpp`, `alkitstyle.*`, `alkitswitch.cpp`, `alcompassdial.*`.
  - Fix: run a mechanical reformat pass (clang-format with an LL-style config, or a Codex pass) restricted to these files. Do it in its own commit before the build so the review diffs stay readable.

---

## 3. v1 safety (DirectorUIV2 OFF)

- **Per-frame calls are unchanged.**
  - `draw()` still calls `syncEasyModeForSelected()`, `applyManualLock()`, the 10 Hz block and `mRim.tickFrame()`.
  - `syncEasyModeForSelected` still reads the cached control, `selectedSlot()` and `selectedIsEasyNative()`, and early-outs on no change.
  - On an edge it now also calls `syncEasyControls()`, using diff-guarded setters.
- **The shell block (`:921-962`) is inert with no shell.** It holds the adapters, contexts, `mEasyModeConnection` and the RESTORE sync.
- **v1 explicit toggle (`onEasyModeCommit` → `applyEasyMode(USER)`) is equivalent to the old path.**
  - The checkbox is control-bound, so the cache already matches and nothing is written twice.
  - Normalize(true) runs, and a refusal keeps the preference.
- **Behaviour diffs:**
  - M1 (passive normalize dropped);
  - B1 #2, which affects v1 only while a v2 Rig page exists.
- **Rim ownership move:** complete and v1-identical.
  - `cine_rimq_status` and `cine_rimq_enable_tag` are found by `ALRigRimController::attach` (`alrigrimcontroller.cpp:111-112`) on both the v1 panel (`mRim.attach(this)` `:690`; ticks `:2697/:2760/:2769`) and `ALPanelRigRim` (`alpanelrigrim.cpp` postBuild/draw/onVisibilityChange).
  - The confirm notification and the hide-on-accept behaviour are unchanged.
  - Removing the two `pending_cpp_targets` is justified.

## 4. Link loop (map ↔ selector ↔ fixture ↔ rim)

- **No recursion.**
  - Map/segment/combo/`revealRow` write `DirectorV2SelectedLight`.
  - Cards only resync display (`syncSelector`, no writes).
  - The map only `select()`s.
  - The Fixture mirror sets the index and `onCommit` under `mSyncingMirror` + `ScopedSuspend`.
  - Its commit slot returns while syncing, and on a user commit it writes the setting only if it differs; the observer then early-returns on an equal index.
- **History:** selection is deliberately not recorded. Mirror rows skip the history hook (`alkitrow.cpp:143`), and `DirectorV2SelectedLight` is not watched. Inside a drag gesture, the fixture repopulation runs suspended, so only Yaw/Pitch land in the Step.

## 5. Draw cost and lifetime

- **No new per-frame work.**
  - The mirror is signal-only.
  - The Easy reconcile is edge-only (but see B1 for the hidden write on that edge).
  - `ALKitModeAdapters::get` (map sweep + `llformat`) runs only on events.
- **Lifetimes are sound:**
  - `resetPage` and the row idle one-shot use `LLHandle`s;
  - `mMirrorConnection`, `mMirrorCommitConnection` and `mEasyModeConnection` are `scoped_connection` members, disconnected before children are deleted;
  - the `cine_rig_slot` context lambda captures nothing panel-owned.

---

## R1 verification (commit `9777bfe2589`)

Verdict: **FIX-NEEDED**, with 0 BLOCKER, 1 MAJOR (new) and 2 MINOR (new). Every R1 finding except m7 is fixed (m7, the style pass, is deferred to clang-format).

### R1 findings

| ID | Status | Evidence |
|---|---|---|
| B1 | **FIXED** | (1) `applyEasyMode` (`alpanelcinelightrig.cpp:2103`) returns early under `mSyncingEasyControls`/`mApplyingEasyMode`, writes only for `USER`, and brackets that `setBOOL` with `mApplyingEasyMode`. (2) `syncEasyModeState` is guarded by the same flags. (3) The page flip callback is guarded by `mUpdatingModeSwitch` (`alkitpage.cpp:359`), and `setModeIndex` holds that guard across the adapter call and its `setValue`. Re-walked cases: page build/RESTORE on a non-native look keeps the preference; v1 checkbox with the v2 page built enters Easy; v2 USER entry runs `normalizeSelectedForEasy(true)`; a refusal keeps the preference. The only `CineLightRigEasyMode` writers left are `applyEasyMode(USER)`, the v1 `control_name` checkbox, history replay and the self-test restore. |
| M1 | **FIXED** | `:2136` `normalizeSelectedForEasy(explicit_entry)` runs on every activating edge, so the passive fold and Key-on are back. |
| M2 | **FIXED as specified** (but see N1) | `revealCard` (`alkitpage.cpp:831`) skips the mode change when the card is listed in the current mode. |
| M3 | **FIXED** | `onAnchorSelected` returns before the suspend and barrier when the slot is unchanged. |
| m1 | FIXED | Dead registration removed; the page-key context still applies. |
| m2 | FIXED | `WP4_STATUS.md` rewritten. However, trace 2 step 4 is still inaccurate (N1). |
| m3 | FIXED | `moreState()` stores and restores per option value; the legacy bool form falls back to fill. |
| m4 | FIXED | `ALKitRow::setControlVisible` falls back to `setVisibleIfChanged` outside a row, so v1 is unchanged. |
| m5 | FIXED | Private `EasyModeOrigin`; the kit include is gone from the v1 header. |
| m6 | FIXED | Anchored `ALKitPopover` parented to the shell. The handle-guarded confirm calls `performPageReset`; Cancel only hides. |

### Contract check

| # | Clause | Result |
|---|---|---|
| (1) | v1 byte-identical | Behaviourally identical. Not byte-identical: see N2. |
| (2) | Writers limited | PASS |
| (3) | v2 user entry normalizes | PASS |
| (4) | Passive Advanced display keeps the preference | PASS |
| (5) | `revealCard` flips only when needed, passively | Flips only when the card is absent and writes nothing, but **does not stick**: see N1 |
| (6) | No no-op steps | `addEntry` rejects old==new and `finishGesture` drops empty Steps, so no no-op Steps. But unsolicited Steps are possible: see N2. |

### Trace re-walk

- **Trace 2 (toggle Easy → cards), user path: TRUE.** Shell segment → `setModeIndex(i,true)` (guard on) → `applyEasyMode(USER)` → the guarded `setBOOL` is recorded once in the user gesture, and the panel's signal handler and flip callback both no-op → `syncEasyModeState(USER)` (no early return) → `normalize(true)` → `syncEasyControls` → `setValue` → `syncEffectiveMode` → 4 or 15 cards.
- **Trace 2 step 4 (search reveal): FALSE.** See N1.
- **Spot checks:**
  - Trace 1: TRUE, code unchanged.
  - Trace 3: TRUE. The `addEntry` guard does not affect coalescing.
  - Trace 4: TRUE.
  - Trace 5: TRUE via the popover. One MOUSE gesture opens on the Reset click.
  - Trace 6: TRUE, and a same-slot rail click no longer clears undo.

### New findings

**N1 (MAJOR): a search reveal into the other mode is reverted on the next frame, so the revealed card disappears.**

How the reveal itself works:
- `revealCard` → `setModeIndex(i,false)` → `adapter(on,PASSIVE)`.
- This now writes nothing (correct). `syncEasyModeState(on,PASSIVE)` sets `mEasyModeActive=on` and the page shows the other card set.

What happens next:
- On the very next `ALPanelCineLightRig::draw()` (`:2742`), `syncEasyModeForSelected()` (`:2751`) re-derives the mode from the unchanged preference: `effective = pref && native`.
- It sees `effective != mEasyModeActive` and flips back. `syncEasyControls` → switch `setValue` → flip callback → `syncEffectiveMode`.

Examples:
- With the preference on Easy and an Easy-native look, searching an Advanced-only control (Fixture, Cookie, Flare, Night mask, Setups) shows Advanced for one frame, then Easy again. The hit card is hidden and the flash and scroll are lost.
- In the other direction (preference Advanced, search Brightness/Drama/Warmth/Easy probe), the reveal never sticks.
- So the WP4 requirement "mode proxy driving `cine_easy_mode` … passively (search)" and contract (5) are not met.
- `WP4_TRACES` step 4 ("changes display passively") describes one frame only.

Fix: give the panel a session-only display override for search, separate from the preference:
- Pass a distinct `SEARCH` origin. `setModeIndex(i,false)` from `revealCard` passes `ALKitModeAdapters::SEARCH`, and the adapter maps it to a new `EasyModeOrigin::SEARCH` (it currently collapses to `PASSIVE`).
- On `SEARCH`, store `mDisplayOverride = on` with the current slot.
- `syncEasyModeState` uses the override instead of the preference while it is set:
  - an Easy override still requires `selectedIsEasyNative()` and runs only `normalize(false)`;
  - if the look is not native, refuse and leave the display unchanged.
- Clear the override on a USER mode click, a preference signal change, or a slot change. It is never written to settings and never recorded.
- Add a WP5 self-test case: search an Advanced-only row from Easy; after 3 frames the card is visible and `CineLightRigEasyMode` is unchanged.

**N2 (MINOR): two small issues remain in the Easy sync path.**

- (a) **Unsuspended passive writes.** The draw-time passive edge now runs `normalizeSelectedForEasy(false)` again (M1), writing MasterEV/KeyEV/KeyOn from `draw()`. These writes are not under `ALKitHistory::ScopedSuspend`. When a Director gesture is open, or keyboard/pointer focus in the shell is recent, `captureSetting` records them as a user Step. For example, after a slot switch into an Easy-native look with Key off, `KeyOn` false→true becomes an undoable Step the user never made. Fix: wrap the passive (non-USER) branch's `normalizeSelectedForEasy(false)` in `ALKitHistory::ScopedSuspend`. The guard is a static counter, so it is inert in v1.
- (b) **v1 not byte-identical.** On a passive edge, v1 now calls `syncEasyControls()` immediately, inside `mSyncingEasyControls`. Before WP4 it deferred to the 10 Hz `updateDerivedStatus`. The visible effect is only ≤100 ms of earlier toggle/gating refresh; there is no state difference. Either accept it and record it in the parity notes, or skip the immediate call when there is no shell.

### Draw cost and lifetime

- No new per-frame work: the guards are member bools, and the popover exists only on demand.
- The popover is owned by the shell and reaches the page through an `LLHandle`, so it is safe after page teardown.
- `mUpdatingModeSwitch`/`mApplyingEasyMode` are always reset on the same path (no early returns between set and clear).

---

## R2 verification (commit `83bfe27837e`)

Verdict: **FIX-NEEDED**, with 1 finding: N3, a one-line fix. N1 and N2(a) are fixed. N2(b) was fixed by gating the immediate refresh on the shell, and that gate over-reaches into the v1 user path.

### N1: search display override

**FIXED.** The override holds and is session-only.

- **Reveal path.** `revealCard` → `setModeIndex(i,false,true)` (under `ScopedSuspend`) → adapter `SEARCH` → `applyEasyMode(SEARCH)`. Only `USER` writes, so this writes nothing. Then `syncEasyModeState(SEARCH)` either:
  - refuses an Easy target on a non-native look and returns the current mode, or
  - stores `mEasyModeOverride`.
- **Persistence.** The next `draw` → `syncEasyModeForSelected` → `syncEasyModeState(pref,PASSIVE)` derives `displayed` from the override, so `effective` is unchanged and nothing is reverted.
- **Session-only.** `mEasyModeOverride` is a member that is never written to settings or history.
- **Cleared on:**
  - a user mode click: `applyEasyMode(USER)`, first line;
  - a preference change: `mEasyModeConnection` clears the override before syncing (undo/redo of the preference also goes through it);
  - a slot change: `selected != mEasyModeSlot` in `syncEasyModeState`.
- **Signal ordering checked.** On a preference change, the page flip callback fires before `mEasyModeConnection` because it was connected earlier, so it briefly evaluates the stale override. `mEasyModeConnection` then clears the override and re-syncs: `syncEasyControls` → guarded flip → `syncEffectiveMode`. The final display follows the new preference. Correct.

### N2(a): passive writes under suspension

**FIXED.** All non-USER `normalizeSelectedForEasy(false)` calls (PASSIVE, RESTORE, SEARCH) run under `ALKitHistory::ScopedSuspend`. This is a static counter, so it is inert in v1.

### N2(b): v1 refresh timing

`syncEasyModeState` now ends with `if(ALKitShell::find(this))syncEasyControls();` (`alpanelcinelightrig.cpp:2158`).

- **v1 passive edges:** restored to the pre-WP4 behaviour (the deferred 10 Hz `updateDerivedStatus` refresh). Correct.

### N3 (MAJOR: contract clause 1 breach; low user impact): the shell gate also drops the v1 explicit-click refresh

- Before WP4, `onEasyModeCommit` always ended with `syncEasyControls()`.
- Now the v1 USER path (`onEasyModeCommit` → `applyEasyMode(USER)` → `syncEasyModeState(USER)`) has no shell, so the immediate refresh is skipped.
- In the standalone floater, after an Easy click:
  - the Easy/Advanced gating (`mEasyGatedControls`, `mAdvancedDrivenControls`) and the Easy control values lag up to 100 ms, until the next `mPanelRefreshClock` tick (`:2773`/`:2787`);
  - when normalization refuses (|Master+Key| > 16), the checkbox stays ticked during that window.
- In R1 this path was v1-identical; this commit regressed it.
- `WP4_STATUS` says "v1 retains its deferred 10 Hz Easy-control refresh". That is true for passive edges only.
- **Fix:** `if(explicit_entry||mHasShell)syncEasyControls();` where `mHasShell` is a bool cached in `postBuild` from `ALKitShell::find(this)`. Caching also removes the parent walk from the edge path.

### Regression sweep

**Contract clauses:**

| # | Clause | Result |
|---|---|---|
| (1) | v1 byte-identical | Passive: PASS. Explicit click: FAIL (N3). |
| (2) | Writers limited | PASS. The writers are still `applyEasyMode(USER)`, the v1 `control_name` checkbox, history replay and the self-test restore. `SEARCH`/`PASSIVE`/`RESTORE` never write. |
| (3) | v2 user entry normalizes | PASS. `explicit_entry` → `normalize(true)`. `syncEasyControls` runs because the shell exists. |
| (4) | Passive Advanced display keeps the preference | PASS |
| (5) | `revealCard` flips only when needed, passively | PASS (holds across draw). An Easy target on a non-native look refuses and the display stays. |
| (6) | No no-op steps | PASS. The `addEntry` guard is unchanged, and the SEARCH path and passive normalize are suspended. |

**Traces:**
- 1, 3, 4, 5 and 6: code unchanged by this commit, so still TRUE.
- 2, user path (v2): TRUE.
- 2, step 4 (search reveal): now TRUE.

**Draw cost:**
- The new work runs only on edges: the override compare and `ALKitShell::find`, which runs after the no-change early return.
- No per-frame regression.

---

## R3 verification (commit `1ec18bb45a6`)

**CLEAN.** N3 is fixed and contract clause 1 holds: the v1 explicit click always calls `syncEasyControls()`, as it did before WP4, and v1 passive edges still use the deferred 10 Hz refresh (alpanelcinelightrig.cpp:2158). No other code changed. WP4 has 0 BLOCKER, 0 MAJOR, and m7 (the style pass) is deferred to clang-format.
