# WP5 adversarial review: Opus R1

**Commit:** `b39e54735ee` "Director kit WP5 (Codex, pending Opus review)" on `feature/director-kit-p0`.
**Scope:** `indra/newview/aluiperfselftest.cpp`, `scripts/perf/ui_selftest.ps1`, `doc/director_kit/WP5_STATUS.md`. The `.h` file is unchanged.
**Checks run:** `zs_check.ps1` on `aluiperfselftest.cpp` PASS. The ps1 parses with 0 errors under Windows PowerShell 5.1.26100. `director_parity.py --groups light_group` PASS with 0 findings. The viewer was not launched.
**Line numbers** refer to the HEAD file.

## Verdict: FIX-NEEDED

- 1 BLOCKER
- 4 MAJOR
- 10 MINOR

The default run is functionally identical to before this commit (proof in the "Default-run identity" section below). The `-DirectorV2` path leaks a persisted setting. Its budget gate also cannot produce a trustworthy PASS or FAIL.

---

## Default-run identity (no `-DirectorV2`, empty `UILayoutAuditSizes`, `DirectorUIV2` = FALSE)

Each new branch was traced. With `mDirectorV2 == false`, every new path is short-circuited or produces the same result as before.

**Audit sizes**
- `parseAuditSizes("")` returns `{DEFAULT "default", MINIMUM}`.
- `auditSizeName()` maps MINIMUM to `"minimum"` (1710-1715), so the directory and CSV names are unchanged.
- The old minimum-skip went straight to `AUDIT_RESTORE`. `skipAuditSize()` now does `++index` (2 is not < 2) and then `AUDIT_RESTORE`, which is the same.
- `++mAuditSizeIndex < mAuditSizes.size()` equals the old `< 2`.

**Page loop**
- At 2890, `isDirectorV2() && ++mKitAuditStateIndex…` short-circuits before the increment.
- The `else if (++mAuditPageIndex <= mTabs.size())` branch has the same increment and compare as before.

**Path keys**
- `auditStatePath()` returns `auditPagePath()` when not V2.
- Finding keys, `mAuditedPages`, `tab_path` and screenshot names (`requestCapture` else-branch) are byte-identical.

**Gated helpers and phases**
- These are no-ops when not V2: `collectKitMetrics`, the `kit_build` rows (2341, 2585), the hover-sweep branch (2590), `saveDirectorAuditSettings`, `restoreDirectorAuditSettings` (empty map), the 600-frame settle caps, and the `ui_layout_audit_states.csv` writer (3037).
- `TAB_SELECT` now copies `TabTarget` instead of referencing it (an allocation in the self-test only; no behaviour change). `lazy_unbuilt` is false.
- Inserting `DIRECTOR_HOVER_SWEEP` keeps every range guard (2227/2232/2239) semantically intact.

**CSV schemas**
- The perf CSV header and columns are unchanged, and no new rows appear in default runs.
- The layout CSV is unchanged.

**ps1**
- With `$DirectorV2` false, these all evaluate as before:
  - the argv;
  - the `layoutComplete` check (`-not $DirectorV2` is true);
  - the default screenshot regex branch (same match and `$Matches`);
  - `$layoutFlagged`;
  - the exit order (3, 5, 1, 4; the new 6 is unreachable).
- `-AB`, `-Inventory` (ON1/OFF/ON2, drift, exit 5, the `ready_wait_frames` column, 600-frame caps), `-Compare`, the crash-tolerant loop, `-WorkingDirectory` and dump symbolization are untouched.

**Residual exceptions:** one is unconditional and harmless. `applyKitAuditState()` always does `findFirstView<ALKitPage>(auditPageRoot())->relayoutNow()` (1706-1707). With v1 floaters there is no `ALKitPage`, so this is a no-op tree walk. The identity also depends on `DirectorUIV2` being false; see m1.

---

## BLOCKER

### B1. `DirectorUIV2=TRUE` leaks into the user's saved settings when a `-DirectorV2` run is aborted by a login

**Evidence**
- `ui_selftest.ps1:74-75` passes `--set DirectorUIV2 TRUE`.
- `DirectorUIV2` is `Persist=1` (`app_settings/settings.xml:8739`).
- `abortForLogin()` (1130-1151) restores only the seven Director audit settings, then sets `sSuppressSettingsSave = false` (1149).
- At the user's next quit, `llappviewer.cpp:2071` saves `gSavedSettings`, so `DirectorUIV2=TRUE` is written to disk.
- From then on, every launch registers `floater_director_v2.xml` (`llviewerfloaterreg.cpp:776`).

**Impact:** this breaks the binding USER DECISION ("DirectorUIV2 stays off by default until all pages pass") and the WP5 requirement to restore every changed setting on `abortForLogin`. The FINISH path is safe only because saving stays suppressed.

**Fix**
1. In the Runner constructor, record that the self-test forced v2: `mForcedV2 = DirectorUIV2 && !audit_sizes.empty()`. `UILayoutAuditSizes` is Persist=0 and only `-DirectorV2` sets it. A dedicated Persist=0 marker setting is cleaner.
2. In `abortForLogin()`, before clearing `sSuppressSettingsSave`, write `DirectorUIV2` back to `control->getDefault()` (false).
3. Do the same in FINISH as defence in depth.
4. Log one line saying v2 stays active until restart.

---

## MAJOR

### M1. Kit audit states are not isolated, and some "covered" states audit content that is never visible

**Evidence: `applyDirectorAuditDefaults()` (1534-1550)**
- It resets the seven settings, then calls `page->restoreState()`.
- `ALKitPage::restoreState()` (`alkitpage.cpp:873`) **returns immediately when `DirectorV2PageState` has no entry for the page**, which is the case after resetting it to its default empty map.
- The page's in-memory chip index and each card's body/More flags (`ALKitCard::restoreOpenState`) are therefore never reset.
- Consequences:
  - After `card-X-closed`, card X stays closed for the next state.
  - After `card-X-more`, X keeps More open for every later card, selector, master and map state.
  - The chip index from CHIP states also carries over.
- So the states are cumulative, not "every state starts from the same default snapshot" (rollout §6). Findings depend on enumeration order.

**Evidence: SELECTOR states (1682-1694)**
- These call `card->revealRow(row)` but never `page->revealCard(card)`.
- After the reset, the Rig page is in Easy mode (the `CineLightRigEasyMode` default is TRUE). In Advanced mode with 6 or more cards, chip filtering applies (`alkitpage.cpp:566`).
- So `kc_lights` and `kc_per_light_rim` can be hidden while their selector states are audited and recorded as covered.

**Evidence: map and master states**
- The `map-*` states (SETTING) do not reveal `kc_light_map` either.
- In Easy mode or when chip-filtered, the four map views are recorded without the map being visible.

**Fix**
1. In `prepareKitAuditStates()`, capture each card's initial body/More flags, the page's chip index and the page's mode once per page.
2. In `applyDirectorAuditDefaults()`, reapply them explicitly: `card->restoreOpenState(initial…)` for every card, set the chip, and call `setModeIndex(default,false)`.
3. For SELECTOR and map states, call `page->revealCard(owning card,false)` first, the same way as CARD.

### M2. `kit_build` measures the wrong thing and also includes legacy pages, so the "first reveal ≤ 50 ms" line can falsely FAIL

**Evidence**
- At `FIRST_FRAME` (2341-2348), the initially visible page gets `kit_build = mActionMs`. That is the **whole `showInstance` floater open** (the same number as the `open` row, which is budgeted at ≤ 120 ms).
- Every lazy page also gets a row (2580-2588), including the 31 legacy-embed pages.
- The ps1 then takes the maximum over all of them (`ui_selftest.ps1:411-414`) for the line "any kit page first reveal ≤ 50".
- Spec 6.4.4 says `kit_build` is "its build ms (from ALKitLazyPage)". `ALKitLazyPage` only logs this value (`alkitlazypage.cpp:33`) and does not expose it.

**Fix**
1. Expose `F64 ALKitLazyPage::buildMs() const`; it is a one-line accessor set at line 33. Coordinate with WP4, which is editing kit files. Alternatively, parse the `Kit page <name> built in <ms> ms` log line in the ps1.
2. Emit `kit_build` from that value for every built page, including the one built during open.
3. Tag each row as kit or legacy (for example `tab_path` plus a `legacy` marker, or a separate `kit_build_legacy` measure), so the P0 budget covers kit pages only (Rig and Rim). Legacy reveal time remains reported.

### M3. The "legacy pages vs v1 baseline = 0 new" gate can never pass

**Evidence**
- `relativePath()` makes widget paths relative to the audit page root.
- In v1 the root is the tab panel. For example, `move_tab` contains `move_scroll` directly (`floater_director.xml:323/331`).
- In v2 the root is the lazy page, which contains the generated `<kit_page name="move_tab_content" legacy="true">`. That page moves its XML children into `<pre>_legacy_host` (`alkitpage.cpp:410-416`).
- As a result, every v2 `widget_a`/`widget_b` has a prefix that v1 paths lack: `move_tab_content/…_legacy_host/move_scroll/…`.
- The key at `ui_selftest.ps1:478/483` includes those paths and the numeric `detail` (for example `label+padding=%d available=%d`, which changes with width). No v2 legacy finding can match.
- The v1 audit has 171 findings, so this line will report hundreds of "new" findings and FAIL permanently.
- The size mapping also assumes the v1 baseline was named `default`/`minimum`. A spike-S7 baseline taken with explicit `1160x720`/`850x560` tokens would never match.

**Fix**
1. In C++, when V2 is active and the page is a legacy `ALKitPage`, use its legacy host as `auditPageRoot()`. The relative paths then equal v1's.
2. Alternatively, in the ps1, strip the leading `^[^/]+_content/[^/]+_legacy_host/` from both widget columns.
3. Drop digits from `detail` in the key, or compare on (finding, widget_a, widget_b) only.
4. Accept both the `default`/`minimum` and `1160x720`/`850x560` baseline naming schemes.

### M4. The coverage gate checks only a subset of states and counts states that were not actually applied

**Evidence: the required patterns**
- The patterns at `ui_selftest.ps1:438-444` require only the following:
  - `default@default`;
  - `mode-Advanced`;
  - the four selectors;
  - `card-*-expanded` and `card-*-closed`.
- These state kinds are not required: chip, `card-*-more`, `master-on`/`master-off`, the four `map-*` views, and `mode-Easy`.
- If enumeration silently yields nothing for those kinds, the kit-layout line still PASSes. Examples: a chip renamed so it no longer contains `_chip_`, or no `ALKitLazyPage` master. Spec 6.4.3 says a single default-visible visit is not coverage.

**Evidence: the coverage records**
- `auditGeometry()` records coverage (1783-1789) even when `applyKitAuditState()` did nothing:
  - a dead handle;
  - a `dynamic_cast` to null;
  - a mode the adapter refused (WP4's PASSIVE adapter refuses Easy for non-Easy-native looks);
  - a hidden card (M1).

**Fix**
1. Have `applyKitAuditState()` return whether the state was applied, and whether its target card is visible after `relayoutNow()`.
2. Write `enumerated`/`applied` columns into `ui_layout_audit_states.csv`, together with the per-page enumerated count at each size.
3. In the ps1:
   - require every enumerated state to be applied at all four sizes;
   - require at least one state of each kind (mode-Easy, mode-Advanced, chip, card expanded/closed/More, selector ×4, master on/off, map ×4 on Rig) for Rig and Rim.

---

## MINOR

### m1. V2 mode is keyed off the persisted `DirectorUIV2` instead of the test mode (1068)

**Problem:** a profile where `DirectorUIV2` was enabled through Debug Settings makes an ordinary `-Layout` run take every V2 path:
- `@state` suffixes in `tab_path`, which breaks `-Compare`;
- the `director/audit-*` screenshot layout, which the default ps1 parser skips;
- `kit_*` rows;
- the hover sweep.

**Fix**
1. Gate on the explicit marker from B1 (`mForcedV2`), or have the ps1 pass `--set DirectorUIV2 FALSE` in non-V2 runs.
2. Also gate the unconditional `relayoutNow()` walk at 1706-1707.

### m2. Exit codes contradict the budget

In `-DirectorV2` runs, the generic flags `open>100ms` (`ps1:200-203`) and `tab>16ms` (204) still return **exit 1**, even when `DIRECTOR V2 BUDGET: PASS`. The budget allows open ≤ 120 ms and first reveal ≤ 50 ms, and a lazy first reveal is always more than 16 ms. The orchestrator will read every V2 run as a failure.

**Fix:** in V2 mode, skip the generic flags for director rows that the budget covers, or document that exit 6 and the budget are authoritative and exit 1 is advisory for the director.

### m3. The steady and hover lines use absolute whole-UI numbers

**Problem**
- `ui_mean_ms` is the whole root-view draw, including the login panel (`llviewerdisplay.cpp:205-216`).
- `find_calls_mean` is every `findChild` call across the frame (`llviewerwindow.cpp:3216`).
- The lines "worst steady page mean ≤ 0.5" and "hover find calls = 0" therefore include the login-screen baseline.

**Fix**
1. Subtract the same pass's `baseline` row, which matches the existing runner's semantics.
2. Include the page-0 `steady` row in "worst steady page".
3. Print the spec Target column (0.4 / 30 / 2) next to each line.

### m4. The hover sweep leaves the last synthetic hover position live

**Problem**
- After the sweep (2612-2645), the last hover position stays live, so the help panel or strip and row hover state can differ during the later audits. Layout findings and screenshots become non-deterministic.
- `mActionMs` also accumulates once per `step()`, not once per collected frame.

**Fix**
1. After the sweep, call `floater->handleHover(-1,-1,MASK_NONE)` (or clear help) and let it settle.
2. Time only the step that lands a sample.

### m5. Some state applications can write undo history

The SELECTOR, CHIP and CARD branches run outside `ALKitHistory::ScopedSuspend`:
- `revealRow` writes `DirectorV2SelectedLight` (`alkitcard.cpp:701`);
- the chip commit calls `saveState()`.

**Fix:** put a single `ScopedSuspend` around the whole `applyKitAuditState()` body.

### m6. The settings snapshot is a fixed 7-name list (1500-1516)

**Problem**
- Against HEAD, the `cine_rig_easy` adapter calls `onEasyModeCommit()`, which runs `normalizeSelectedForEasy(true)`. That writes `CineLightRigMasterEV`, `KeyEV`, `KeyOn` and the Rim/BG EV buckets (`alpanelcinelightrig.cpp:2039`), none of which are restored.
- WP4's in-flight rewrite (PASSIVE origin means no normalize) removes this path. Re-verify once WP4 lands.

**Fix (robust):** snapshot all `gSavedSettings` values once, with `applyToAll` into a map. On close and abort, restore only the values that changed.

### m7. Perf rows are measured in the user's live state

The perf rows (tab/steady/hover) are measured before any defaults are applied, with whatever Easy/Advanced and card state the profile has. The budget numbers therefore vary by profile.

**Fix:** apply the audit defaults once, right after `prepareFloaterActions()`, in V2 mode.

### m8. No runtime caps for V2 runs

**Problem**
- `-DirectorV2` inherits `TimeoutMin` = 25.
- There are about 100 Rig/Rim states × 4 sizes × up to 6 captures, plus 35 pages × 4 sizes, and no per-page or suite caps (rollout §6: 256 states, 2 min per page, 72 min per suite).
- A timeout kills the viewer with exit 2 and prints no budget.
- The 600-frame settle caps (2804, 2851) are effectively unreachable, because 5 frames plus 0.3 s always happens first.

**Fix**
1. Default `TimeoutMin` to 75 for `-DirectorV2`.
2. Cap enumerated states per page at 256 with a WARN.

### m9. Lazy rescan has two small path and restore bugs

**Problem**
- The lazy rescan passes the decorated `current.mPath` (which may carry a `" [markers]"` suffix) as the `collectTabs` prefix; the original code uses `raw_path`.
- Nested tab containers that are discovered later are not added to `mInitialTabs`, so their selection is not restored.

**Fix:** keep the raw path in `TabTarget` and append the nested containers to `mInitialTabs`.

### m10. New pre-login exposure

**Problem**
- The master states flip `CineLightRigEnabled` and `CineRigRimEnabled` at `STATE_LOGIN_WAIT` for the first time.
- The map states switch `ALLightRigMap` to Top/Side. Pre-login it reads only `LLViewerCamera` (`allightrigmap.cpp:221,268`), which is safe.
- No null environment getter is reached on these paths. Renderers read the rig enables only during world render.

**Note:** this is new, unexercised pre-login surface. The crash-tolerant runner will catch a crash, but on a director-only run that ends in exit 2 ("No measurable targets left") with no budget output. Call it out in the post-build checklist.

---

## Question 6: runtime requested/actual type instrumentation "partial pending WP6"

**Acceptable for P0; not a gap.**
- P0 §6.2 check 3 needs a runtime type log only for patterns that static extraction cannot prove.
- `director_parity.py` fails on any unexpanded dynamic lookup (`director_parity.py:560-562`).
- `--groups light_group` PASSes with 0 findings and 61 `getChild` requests, so P0 has no unproven pattern.
- The full `(page, widget, requested, actual, owner, adapter)` runtime stream belongs to rollout §6 (WP6), which is also where the M13 ledger points.

**Doc fix:** in `WP5_STATUS.md`, cite this evidence (parity PASS, 0 unresolved dynamic lookups) instead of the bare "partial".

## Verified clean

- `cl /Zs` passes.
- PowerShell 5.1 parses the script.
- Lifetimes and handles:
  - all stored views are held as `LLHandle`;
  - the `TabTarget&` reference is used before `mTabs.insert`;
  - captures use handles.
- No code was added to any normal-viewer per-frame path. Everything new sits behind `sActive`.
- `tokenizeBatch` returns false when it finishes, so the `while` loop terminates.
- The `-DirectorV2` argv matches 6.4.5.
- The screenshot layout matches 6.7.1.
- The budget prints all seven 6.4 lines, plus the 6.4.6 hover line.
