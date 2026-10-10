# Adversarial review R1: DIRECTOR_KIT_P0_SPEC.md + VCAM_MONITORS_DESIGN.md

Reviewer: Opus (independent context), 2026-10-10. Branch `feature/director-kit-p0` @ 241029a65e2.
Method: every concrete claim sampled against the tree (about 40 claims, listed at the end). Line numbers
are from this checkout. Note: `aluiperfselftest.cpp` was changing while I read it (the same statements
moved by about 64 lines), so another writer is active in the tree.

**VERDICT: NO-GO.** The kit spec cannot deliver the ALL-AT-ONCE decision as written. Its core toggle
widget is built on a hook that never fires. The VCam undo design blacks out every in-world display and
resets the Gate. The VCam 0.3 ms budget leaves out the cost that actually dominates. The two specs also
define incompatible VCam pages and two competing Ctrl+Z owners.

Counts: **6 BLOCKER, 16 MAJOR, 14 MINOR.**

---

## BLOCKER

### B1. ALL-AT-ONCE is unspecified. The spec stops at Light › Rig and Light › Rim.
- **Doc:** KIT §0 "Not in P0: converting any other page"; §4.3; §6.4 budget table; §6.2 (`--groups light_group`); §10 U1 ("P0 is identical either way"); the USER DECISIONS appendix (lines 1697-1702).
- **Evidence:**
  - The appendix makes v2 shippable only when all 36 pages are full kit pages with 0 absolute findings at 4 sizes. The body still says "the user has not chosen" (line 13) and stops at Rig and Rim.
  - The data snapshot `scripts/code_tools/director_kit/director_mockup_data.json` (36 pages) shows that the remaining pages are mostly code-driven:
    | Page | Rows without a setting |
    |---|---|
    | path_tab | 58/62 |
    | props_tab | 33/33 |
    | ghosts_tab | 122/125 |
    | dance_tab | 82/82 |
    | gaze_tab | 109/123 |
    | actor_style_tab | 18/18 |
    | practicals_tab | 16/16 |
  - Those pages need logic-panel owners like ALPanelCineLightRig. The only mechanism the spec gives is one hand-written `p0_pages` entry per page plus a bespoke C++ extraction (ALRigRimController).
  - `vcam_tab` has 3 data rows (the old summary card, not the monitors mockup). `capture_settings_tab` has 0 rows. `tron_tab` (132 rows) is classified as a **launcher**, which is not a "full kit page".
  - Widgets the mockup needs but the spec excludes: the curve editor, slot grid, anim picker, VCam monitors, the path editor canvas and `ALPanelAnimPreview`.
  - The CLAUDE.md rule 6 named exceptions (`ghost_studio_panel`, `path_editor`, floater_actor_mover) must each be "a separate, in-world-checked change".
- **Why it breaks:** Codex will deliver P0, and the remaining work for the user's chosen rollout has no spec, gates or WPs. It is a full re-planning, not "later phases back-to-back".
- **Fix (add to the spec before WP1):**
  - (a) A per-page conversion table for all 34 remaining pages: v1_sources (including embedded `filename=` panels), logic class (existing or new), who adapter, undo context, preset mode, card/option enables, and the custom widgets needed.
  - (b) A rule for shared embedded panels that other floaters also use (Gaze, Ghost Studio, Path, Weather, Exposure, Actor Style, Practicals). The global off-limits list freezes their XML, so state whether v2 duplicates them as kit XML with the same class, as Rig does, and how parity is kept.
  - (c) Kit kinds or custom-row contracts for the excluded widgets.
  - (d) Gates: `director_parity.py --groups all`; §6.4 absolute 0 findings on all pages at all 4 sizes (drop the "0 new vs v1" row for the final gate).
  - (e) Tron as a full kit page, not a launcher.
  - (f) Data completeness: VCam, Capture settings, help and keywords for every row.
  - (g) Delete or rewrite U1 and the line 13 rollout text.

### B2. ALKitSwitch caches state in `setValue()`, which never runs for setting changes or clicks.
- **Doc:** KIT §3.2, §3.4 (dimming reads `mEnable->isOn()`), §3.5 (banner, `setModeIndex`), §4.4.
- **Evidence:**
  - `LLCheckBoxCtrl::setControlName` forwards the binding to the inner button (`llcheckboxctrl.cpp:280-283`). The checkbox's own `getControlVariable()` is null.
  - A setting change goes through `controlListener` to `ctrl->setValue` on the **button** (`lluictrl.cpp:639-647`), not on the checkbox.
  - A user click goes through `LLButton::onCommit` to `toggleState` and `setToggleState` (the button's own `setControlValue` and `setValue`), then the click callback calls `LLCheckBoxCtrl::onCommit` (`llbutton.cpp:331-337, 1050-1055`; `llcheckboxctrl.cpp:143`). `ALKitSwitch::setValue` never runs.
  - `LLCheckBoxCtrl::onCommit` calls `setControlValue(getValue())` on the checkbox, which is a no-op because there is no control var (`llcheckboxctrl.cpp:159-166`; `lluictrl.cpp:510-517`). So:
    - §3.5's passive `sw->setControlValue(v)` writes nothing;
    - the explicit `sw->setValue(v); sw->onCommit()` writes the bound setting only because `onEasyModeCommit` happens to write it itself (`alpanelcinelightrig.cpp:2226`).
  - A new tag gets its widget defaults from `widgets/<tag>.xml`. There is no `widgets/kit_switch.xml`. `is_toggle="true"` and the 13×13 button rect come from `widgets/check_box.xml`, so a `kit_switch` button is non-toggle and 0×0. Clicks do nothing.
- **Why it breaks:** `mOn` and the flip signal go stale on every preset apply, undo, slot switch, scene load and click. The master banner, card dimming, Easy/Advanced card sets, row "On/Off" text and amber all show the wrong state. This is the kit's core toggle.
- **Fix:**
  - `isOn()` reads the inner button's toggle state (a member bool, allocation-free).
  - Flips are detected from (a) an `onCommit` override and (b) a connection to the button's `LLControlVariable` signal, or `getChild<LLButton>` once, plus a commit-signal hook on the button.
  - Ship `skins/default/xui/en/widgets/kit_switch.xml` (copy the `check_box.xml` defaults).
  - The passive path writes `gSavedSettings`/`control->set()` directly.
  - Spike S2 must add: "click toggles + writes the setting; external `set()` updates `isOn()`."

### B3. A passive (search-reveal) mode switch leaves the Easy controls dead.
- **Doc:** KIT §3.5 `setModeIndex` (passive = setting write without commit); §3.8 ("never changes the mode setting"; contradicts §3.5 and §3.11's "ALKitPage passive mode switches").
- **Evidence:**
  - Easy state is per instance: `mEasyModeActive` is set only in `onEasyModeCommit` (`alpanelcinelightrig.cpp:2217-2237`).
  - Every Easy commit returns early when `!mEasyModeActive` (`:2241`).
  - Writing `CineLightRigEasyMode` without `onCommit` (even after the B2 fix) shows the Easy cards while the panel stays in Advanced. Every Easy control then silently does nothing.
- **Fix:**
  - Passive reveal must call the owner's commit under `ALKitHistory::ScopedSuspend`, or the page must call a registered "mode adapter" on the logic panel.
  - Also make §3.8 and §3.5 agree on whether search may switch modes at all.

### B4. VCam undo through `applySceneData()` is destructive.
- **Doc:** VCAM §5.4, §7.7, WP1.
- **Evidence:** the commit block of `PrismLensRegistry::applySceneData` (`llprismlens.cpp:5503-5579`):
  - releases **every** capture output and the scratch buffers;
  - default-constructs every instance and display;
  - allocates **new generations** for every handle;
  - resets `mGate` and re-punches the on-air arm;
  - resets the runtime history.
- **Why it breaks:**
  - Every undo or redo of a single "Bind Camera 2 to face" blacks out all in-world Prism faces until each capture re-renders under the global budget.
  - It interrupts a live Gate program.
  - It reallocates up to 8 × 1024² RGBA16F outputs, which is a hitch.
  - "Restore selection only if handles still exist" never matches, because the generations changed.
  - A slider gesture snapshotted this way makes the problem routine.
- **Fix:** undo must be inverse typed commands on stable handles: bind/unbind, reassign, `setCamera`/`setDisplay`/`setRate`/`setLabel` with the before value, and add/remove with identity-preserving re-add (a new registry API that reuses UUID and generation). Otherwise `applySceneData` needs a diff mode that preserves identities, outputs and Gate runtime. Forbid full-snapshot undo.

### B5. The "≤ 0.3 ms at 1 fps × 8" budget counts GPU time only, so frame time is neither bounded nor measured.
- **Doc:** VCAM §4.5, §7.1, WP4.
- **Evidence:**
  - A preview-only capture runs the full `renderAuxiliaryView` path: a CPU cull, stateSort and draw submission (`llprismlens.cpp:7209-7265`). It also runs the Prism spot-shadow generation (`pipeline.h:760-763`) and deferred lighting with SSAO and shadows (`pipeline.cpp:1752`).
  - On a CPU-bound SL viewer this is milliseconds of render-thread time per attempt, and the budget leaves it out.
  - Scratch packs are an **exact-size** pool of 8 with LRU eviction and a full G-buffer allocation on a miss (`pipeline.cpp:1728-1826`).
  - Preview sizes ("fit aspect inside 256×144", so a different size per aspect) plus the adaptive-scaled visible sizes can exceed 8 distinct sizes. Every miss then allocates a deferred pack (a driver stall) that GPU timestamps around the render do not see.
- **Why it breaks:** the core promise is frame-time cost. The design can report PASS while frames spike by several ms every ~7 frames.
- **Fix:**
  - The budget debits `max(CPU render-thread wall time, GPU time)` per preview attempt, including scratch acquisition. Report p95 frame-time delta as well as the average.
  - Quantize every preview to ONE scratch bucket (for example, always 256×144 with UV letterbox), so previews add at most one pool entry and never evict a visible capture's pack.
  - Define a preview quality profile: no spot or sun shadows, no SSAO, no volumetrics, no probe update, and reduced draw distance.
  - The self-test prints the CPU and GPU components separately and fails on either.

### B6. The two specs define incompatible VCam pages and two competing undo systems.
- **Doc:** VCAM §6.1 (replace `floater_director.xml:1342-1353` with a filename embed plus an own `ALScrollFit` document), §5.4 (`ALPrismUndoHistory`, "claims Ctrl-Z/Ctrl-Shift-Z while focused"); KIT global off-limits (`floater_director.xml` byte-identical), §3.11-3.12 (Director-wide ALKitHistory, Ctrl+Z/Ctrl+Y, "Ctrl+Shift+Z untouched"), §4.3 ("VCam uses v1 vcam_tab"), and the appendix (every page a full kit page).
- **Evidence:**
  - VCam WP6 edits the frozen v1 file and builds a non-kit page with no `kit_row`/`kit_card`, no search index, help, amber or chips.
  - The Prism globals on that page (`PrismLensZoom`, `PrismProtectedMainFPS`, and others; `settings.xml:29474-29521`) are settings, so ALKitHistory's watch would journal the same change that ALPrismUndoHistory journals. Undo would then run twice.
- **Fix:**
  - Rewrite VCam §6 as a kit page: a logic class `panel_director_vcam` as the page root (as Rig does), monitors as a `custom` (or new `monitors`) row kind, and editors as kit cards bound through ALKitBinding/ALKitWidgetBinding.
  - Make ALKitHistory the only Ctrl+Z owner. VCam registers typed "custom entries" (B4) with it, and its settings are excluded from the page watch set.
  - Leave v1 `vcam_tab` untouched.

---

## MAJOR

### M1. Preview demand steals cadence from in-world displays.
- **Doc:** VCAM §4.2 ("included in the existing max-min water filling").
- **Evidence:** in `updateCadenceEntitlements` (`llprismlens.cpp:3096-3122`) low demands are satisfied first. Six 1 Hz previews therefore take 6 Hz from the shared capacity that visible faces split.
- In manual every-frame mode (`:3185-3197`), automatic producers break on the first due slot with no entitlement check. Automatic preview-only captures would take frame tokens round-robin.
- This contradicts "in-world displays continue at their own picture rates".
- **Fix:** use two tiers. Water-fill visible demand first, and give previews only the leftover. In every-frame mode, preview-only captures always use their own preview deadline.

### M2. `SOURCE_VIEWER` cargo resolves to the stale toolbar drag item.
- **Doc:** VCAM §5.1.
- **Evidence:**
  - `locateInventory()` for `SOURCE_VIEWER` returns `gToolBarView->getDragItem()` (`lltooldraganddrop.cpp:3371-3374`).
  - `LLToolBarView::getDragItem()` keeps returning the last toolbar `mDragItem` after any earlier toolbar drag (`lltoolbarview.cpp:958-966`; `resetDragTool` does not clear it).
  - The cargo branch is tested **before** the UUID branch (`lltooldraganddrop.cpp:536-552, 617-635`).
- **Why it breaks:** VCam UI drop targets (face rows) receive an `LLInventoryObject*` where they expect an `LLUUID*`.
- **Fix:** add `ESource::SOURCE_VCAM`. `locateInventory` returns NULL for it, and `is_uuid_dragged = (SOURCE_PEOPLE || SOURCE_VCAM)`. Fix the existing top-view drop pass, which passes `drop=false` (`:590`), for this source.

### M3. The keyboard table rests on false premises.
- **Doc:** KIT §3.12, §3.8.
- **Evidence:**
  - `LLLineEditor` has no undo: no `undo`/`canUndo` in `lllineeditor.cpp`.
  - With `hasAccelerators()` true, Ctrl+Z in the search field, `cine_seed` or any spinner's text bubbles up (`llview.cpp:1049-1053`) to `LLFloaterDirector::handleKeyHere`. That runs a Director-wide setting undo while the user thinks they are undoing text, so the "(they consume it)" claim is false.
  - `/` is a key below 0x80. With any keyboard focus, `LLViewerWindow::handleKey` returns early to the character path (`llviewerwindow.cpp:3409-3420`), and the key arrives through `handleUnicodeChar` (`:3684-3693`; `llview.cpp:1103-1129`), never `handleKeyHere`.
- **Fix:**
  - Ctrl+Z/Y in v2 is ignored when focus is an `LLLineEditor`, `LLTextEditor` or `LLSpinCtrl` (check `gFocusMgr.getKeyboardFocus()` with `dynamic_cast`).
  - Implement `/` in `LLFloaterDirector::handleUnicodeCharHere`, consumed only when the focus is not a text-entry widget.
  - Add both to spike S8.

### M4. Bulk writers corrupt the undo history.
- **Doc:** KIT §3.11, §9.4.
- **Evidence:**
  - Scene load: `ALCineLightRigManager::applySceneData` (`llfloaterdirector.cpp:1664`), including `gSavedSettings` writes (`alcinelightrig.cpp:4284-4297`).
  - `resetAll` calls `resetAllToSelf()`, which rewrites every slot blob (`alpanelcinelightrig.cpp:2014`; `alcinelightrigmanager.cpp:710-735`).
  - Cue playback and Lightbox preset applies are other bulk writers.
  - Only `onAnchorSelected` is suspended. Steps recorded before a scene load keep pre-load `oldv`, so undo after the load partially reverts the new scene. Undo of "Reset all" restores only the selected slot's settings; the other slots' blobs are lost.
- **Fix:** add `ALKitHistory::barrier(reason)`, which clears or seals the stack. Call it from scene apply, `resetAllToSelf`, `setSelectedSlot` from any caller, and preset or cue applies outside the Director. Wrap these in ScopedSuspend. Steps carry a scene generation in their context.

### M5. kit_lint rule 5 fails the P0 gate on real settings.
- **Doc:** KIT §6.1 rule 5 ("missing from `settings.xml`").
- **Evidence:** of the data rows with a setting, 161 live only in `settings_alchemy.xml`. That includes 18 Rig flare rows (`RenderLensFlareMaster`, `RenderCineLensFlareStrength`, …) and 11 Bloom & Glow rows, both of which are P0 pages.
- **Fix:**
  - Lint reads every file listed in `settings_files.xml` (Global plus PerAccount).
  - Bindings resolve through `LLUI::findControl` (both groups).
  - `vec2` maps to a Vector3 setting with z unused (`RenderVignetteCenter` is Vector3). There is no Vector2 control type (`llcontrol.h:51-66`).

### M6. Light map: the rig-off mirror rule contradicts itself, and object targets never get a facing.
- **Doc:** KIT §3.14, Display transform and Engine accessor.
- **Evidence:**
  - "use F = 0 and draw 'Mirror shown unmirrored'" contradicts itself. `eff = wrap180(2·0 − (base+orbit))` is mirrored about east, not unmirrored, and drags still apply `−Δ`.
  - Facing is assigned only on the `!object_targeted` path (`alcinelightrig.cpp:2632-2647`), so an object-targeted live rig always reports "rig off".
- **Fix:**
  - When the rig is live, draw from `selected().lastFrame().mProj[i].mOffX/Y/Z` (`alcinelightrigmodel.h:213-231`). That is the real world offset with mirror, effective radius and OffsetZ already applied, so F is not needed for display.
  - When the rig is off, use `eff = base+orbit` with a `+Δ` drag, and say so.
  - Define F for object targets (the object rotation), or exclude mirror there.

### M7. The Side-view drag does not invert the Side projection.
- **Doc:** KIT §3.14 (Displayed elevation `p_vis`; Side drag; TUT "dragSideYaw … inverse at a fixed pitch").
- **Evidence:**
  - The projection uses `p_vis = atan2(r·sin(eff_pitch)+h, r·cos(eff_pitch))`, where eff includes OrbitPitch.
  - The drag divides by `cos p_new` (base pitch + Δ) and adds Δp to the base, so the light jumps at drag start whenever orbit pitch or OffsetZ is non-zero.
  - Also, `p_vis` assumes radius r while the engine uses `effective_radius` (`alcinelightrigmodel.cpp:1615-1621`).
  - In 2.5D, `+x` adds `+Δt`, which moves back-layer lights (cos t < 0) left on screen, so the drag is inverted for them.
- **Fix:** define one `p_disp(base, orbit, h, r)` and invert exactly that function (solve `base_pitch` from the target `p_disp`). For 2.5D, use `Δt = sign(cos t)·dx·k`. Test with non-zero orbit pitch and OffsetZ.

### M8. Arc-dial geometry for the user's U3 decision (horizon = 0) is unspecified.
- **Doc:** KIT §3.13; appendix U3.
- **Evidence:**
  - The default is `arc_zero="up"`, and the geometry places the centre 14s from the bottom with a half arc of −90…90 above the baseline.
  - With `arc_zero="right"` the needle at −90 points down, and `r = 20s > 14s` draws below the widget.
  - The range labels, baseline and pointer inverse are only defined for "up".
- **Fix:** make `right` the default (the binding decision). Specify its geometry: centre at the left-middle, a right half arc, a horizontal 0 tick, up and down labels, and the inverse mapping. Add TUT cases.

### M9. "0 findings at 4 sizes" audits only the default-visible state.
- **Doc:** KIT §6.4, §6.1 warnings.
- **Evidence:**
  - The runtime audit visits each page once (`auditGeometry` per page).
  - With chips in filter mode, Easy vs Advanced, selector options, collapsed "More options", popovers and code-hidden rows, most rows are never laid out during the audit.
  - Labels use `use_ellipses=false` at LABEL_W 100, yet >16 characters is only a lint **warning**, so a truncation slips past every gate.
- **Fix:**
  - The audit iterates each page's modes × chips × selector options with More open, using the kit API.
  - kit_lint measures every label with the real font metrics (an IBMPlex/SansSerif width table) and makes label width > LABEL_W an **error** (or defines two-line labels with a row-height rule).

### M10. The transport bar cannot reach 0 findings at 850×560.
- **Doc:** KIT §4.6.
- **Evidence:**
  - From the listed widths, the visible children in icon mode sum to 778 px, plus 15 gaps × 8 + 2 × 10 padding, which is 918 px against an 850 px bar.
  - `collapse_order` removes `take_timecode` and `btn_scene_delete`, leaving `status_strip` about 94 px. Its v1 text truncates.
  - While a scene is being named, `scene_name_editor` (130) becomes visible and is not in `collapse_order`, so the bar overflows by about 44 px.
- **Fix:** add a width budget table at 850 and 1160. `scene_name_editor` replaces `scene_combo` (one slot). `status_strip` becomes a custom ellipsizing draw (exempt, as the help strip is). Extend `collapse_order` (for example `btn_reset_marks`, `rec_indicator` text).

### M11. WP1 cannot be completed or reviewed on its own.
- **Doc:** KIT §7 WP1 vs WP2.
- **Evidence:** WP1's ALKitPage and ALKitRow use ALKitHistory (undo/redo, ScopedSuspend, `recordWidget`), `ALKitSearch::Hits`, `ALKitShell::help()` and ALKitPresetStrip. All of those are WP2 files.
- **Fix:** WP1 ships the interface headers (`alkithistory.h`, `alkitsearch.h` Hits, `alkithelp.h`, the shell accessor) with stub implementations, or move ALKitPage's bar and search code to WP2.

### M12. Lifetime: the Director is destroyed on close, and several callbacks dangle.
- **Doc:** KIT §3.5 `requestLayout` (`doOnIdleOneTime`), §3.8 (`doOnIdleRepeating` tokenizer, `LLEventTimer` debounce), §3.11 (10 Hz gesture timer, static `noteKey`), §3.3 (`ALKitShell::help()` static), §3.18 (who adapters and contexts registered by a panel that exists twice: the standalone floater and the Director page).
- **Evidence:** the `floater_director.xml` root has no `reuse_instance`, so the floater is destroyed on close.
- **Fix:**
  - Every deferred callback captures an `LLHandle` and checks it.
  - Timers are shell-owned members.
  - There is no static shell pointer (look it up through the floater instance).
  - Who adapters are stateless functions of manager state. Registration is idempotent and never captures a panel `this`.

### M13. The parity gate checks names but not classes, and dynamic names escape it.
- **Doc:** KIT §6.2 check 3, §4.4 upgrades, D2 ("exact name and class").
- **Evidence:**
  - `alpanelcinelightrig.cpp:1069-1074` builds `cine_{role}_{profile,gobo,gel,flicker}` and calls `getChild<LLComboBox>` on them. They are missing from `cpp_name_patterns.json`, and check 3 only scans literals.
  - A presentation upgrade (slider to dial, spinner to slider) of a dynamically referenced widget passes parity, and `getChild<T>` then returns a dummy.
- **Fix:**
  - Check 3 extracts `getChild<T>`/`findChild<T>` with T and with `prefix + "suffix"` concatenations (or uses an instrumented runtime log from the self-test), and verifies that the v2 class is compatible with T.
  - The generator refuses any upgrade whose name matches a getChild suffix pattern.

### M14. The VCam atlas pass has no GL-state contract and no defined HDR-to-display transform.
- **Doc:** VCAM §4.3, §7.5.
- **Evidence:**
  - The spec does not say where in the frame the cell update runs, or what it saves and restores (FBO stack, `gGLViewport`, scissor, blend, depth, colour mask, bound shader, texture unit 0). CLAUDE.md rule 1 requires proof against the beauty pass and the feature-off path.
  - The retained output is RGBA16F HDR (`pipeline.cpp:1852`) and the cell is "display-referred RGBA8", but no tonemap or exposure is specified, so "exact" previews have no defined look.
- **Fix:**
  - Run it inside `renderAuxiliaryView` after the aux scope, under `ScopedPrismRenderState`, plus explicit `LLRenderTarget` bind/flush and `gGL` state guards.
  - Specify the transform (for example, the main view's current exposure plus tonemap type, uniforms from the same frame) and the UI-side encoding.
  - WP3 acceptance covers both.

### M15. The menu item exposes v2 before all pages pass.
- **Doc:** KIT §3.16 Menu; appendix ("user receives the new Director only when EVERY page…").
- **Fix:** P0 adds no user-visible menu item. `DirectorUIV2` is set only by `ui_selftest.ps1 --set`, or by a debug-settings-only path, until the final gate passes.

### M16. `ALKitSearch` index field count disagrees with itself, and the generator and loader contract is ambiguous.
- **Doc:** KIT §3.8.
- **Evidence:** the spec says "13 TAB-separated fields" but lists 15 (group_key … help).
- **Why it breaks:** WP2's loader and WP3's generator will be written to different counts.
- **Fix:** state 15, add a version-2 header line with the field count, and make the loader reject a mismatch.

---

## MINOR

1. **The audit size name changes the default run.**
   - KIT §6.4.1. `auditSizeName()` returns `"minimum"`, not `"min"` (`aluiperfselftest.cpp` `auditSizeName`).
   - Returning the token changes the default-run CSV keys and directories and breaks `-Compare` against old baselines.
   - Map `min` to "minimum".
2. **New dial geometry must not change existing dials.**
   - KIT §3.13. The existing dial radius is `min(w,h)/2 − 12` with the letters outside (`alcompassdial.cpp:71-78`), and the new compass geometry is `r = 20s`.
   - Gate the new geometry on a new param so the Actor Mover and Ghost Studio dials stay pixel-identical, as WP1 acceptance demands.
   - `wheel_adjust` is not an ALCompassDial param; add it to the table.
3. **Compass dial over a 0..360 setting.** KIT §5.1 `cine_catchlight_angle` (spinner 0..360, `panel_cine_light_rig.xml:244`). The compass wrap must keep values in [min, max), not [−180, 180], or amber and default compares break at 360 vs 0.
4. **Legend side letters change with the camera.**
   - KIT §3.14 Legend. The side letters (R/L) depend on the camera, so the text must be rebuilt on a 10 Hz `ALLiveReadoutClock` with diff guards, not in `draw()`.
   - The signature `yawSideText(t, base)` conflicts with the TUT `yawSideText(45)`.
5. **Collapsed rows must actually be hidden.**
   - KIT §3.4. The visibility probe covers the first 64 rows only; state the cap, or use a vector of bits.
   - Rows not effectively visible must be `setVisible(false)`. UIFit `childOverflow` skips only invisible children (`lluifitdiag.cpp:54-61`).
6. **Per-reshape name lookup in v2.**
   - KIT §3.15/§5.1. In v2 `cine_light_rig_flow_grid` is absent, so `ALPanelCineLightRig::reshape` runs a recursive `findChildView` on **every** reshape (`alpanelcinelightrig.cpp:2828-2831`), including resize drags.
   - Add a looked-up flag.
7. **Scrollbar width oscillation.** KIT §3.7. The scrollbar appearing changes the doc width, which relayouts, which changes the height. Reserve the scrollbar width permanently in ALKitDoc.
8. **Fixture role link semantics.**
   - KIT §5.1 `mirror_setting`. Specify that `cine_fixture_role` is set with `setValue` + `onCommit` inside ScopedSuspend.
   - Without the commit, `onFixtureRoleCommit` (`alpanelcinelightrig.cpp:925-926`) never repopulates kelvin and gels for the new role.
9. **`setPanelVisible` has two writers.** VCAM §4.1. `setPanelVisible(bool)` is written by both the inline page and the manager, so the last writer wins. Use a refcount or owner token.
10. **Visible-display state is stale before `prepare()`.** VCAM §4.2. Demand is published "before `prepare()`" but conditioned on "no prepared visible display". `mAnyDisplayVisible` is reset inside `prepare` (`llprismlens.cpp:2781`). Say that prepare decides between visible and preview-only.
11. **Scene load bypasses the permission rule.** VCAM §5.2. `applySceneData` restores bindings to faces that now fail the new `permModify()` rule. Decide the policy (reject at load, or grandfather), and apply the same validator.
12. **Retained outputs are never released.** VCAM §5.2 says it "releases the old capture output", but outputs are lifetime-fixed with no release path (`pipeline.cpp:1849-1851`). Reword it as "drops publication / demand".
13. **Timing query nesting.** VCAM §4.5. Use `GL_TIMESTAMP` (`glQueryCounter`), not `GL_TIME_ELAPSED`, which cannot nest with `LLGLSLShader` profiling (`llglslshader.cpp:263-288`) or Tracy GPU zones.
14. **Future cargo-value collision.** VCAM §5.1. `DAD_VCAM_FEED = 22` is safe today (`llui.h:56-82`; only `llinventorypanel.h:493` sizes by `DAD_COUNT`), but it will collide with any upstream `DAD_* = 22` in the planned re-port. Add a comment and a static_assert near the dictionary.

---

## Claims verified OK

1. `llviewerwindow.h:312` `getLeftMouseDown()`.
2. `llviewerwindow.cpp:3436-3539`: Ctrl/Alt menus before focus unless `keyboardFocusHasAccelerators()` (`llfocusmgr.cpp:362-375` walks the parent chain).
3. `gl_arc_2d` at `llrender2dutils.cpp:848`.
4. `alcinelightrig.cpp:2646` `transforms.mFacingAzimuthDeg = facing;`.
5. `alcinelightrigmodel.cpp:1057-1062` mirror formula `2F − (base+orbit)`; `1612-1621` offsets.
6. Yaw is world azimuth (the only facing use is mirror; `transforms.mYawDeg = orbit_yaw`, `:1502`).
7. `llviewerfloaterreg.cpp:776` director registration; `llappviewer.cpp:1025` after `initConfiguration` (`:885`).
8. All map, rig and rim settings exist with the stated types (`CineLightRig{Key,Fill,Rim,Bg}{Yaw,Pitch,On,OffsetZ}`, Mirror, OrbitYaw/Pitch, Radius, EasyMode, `CineRigRim*`); `DirectorLastTabName` exists; the new `DirectorV2*`/`Kit*` settings do not exist yet.
9. `LLSliderWheel::guarded`/`keepAlive`/`noteScroll` (`llsliderwheel.h`).
10. ALScrollFit API: `wrapTabPanel(LLPanel*, StretcherMins*)`, `fitPage`, `fillPage`, `setUnwrappedPages`, `FocusTracker::install(sc, doc, parent)`, `mMins` shared_ptr.
11. `ALFlowGrid` is `final` with `fit_cards`.
12. aluirefresh.h: `setTextIfChanged`…`setVisibleIfChanged`, `ALRevisionGate`, `ALLiveReadoutClock`, `isWidgetOnScreen`.
13. `LLViewDrawContext(F32 alpha)` multiplies with the stack (`llview.h:70-85`).
14. `IBMPlexMono` is declared in `fonts.xml`.
15. `GenericAlertYesCancel` exists; `ConfirmRestart` exists and can be reused.
16. `setSelectedSlot` writes settings synchronously (`alcinelightrigmanager.cpp:698-708`).
17. `RigFrame::mProj[i]` has `mSR`/`mSG`/`mSB`/`mOn` (`alcinelightrigmodel.h:213-236`).
18. `RIG_RIM_PRESETS[20]` and static `applyRigRimPresetByName` (`alpanelcinelightrig.cpp:420, 1890`).
19. `CineLightRig.ResetControl` is exactly `resetToDefault(true)` over a comma list (`:488-513`), so the kit reset is equivalent.
20. Children are built and post-built before the parent's `postBuild` (`lluictrlfactory.h:255-261`), so D3 holds.
21. LLFloaterDirector refreshers, `selectLeafTab`, `fillPage`, the `addLauncherCard` `getChild`, and the draw order (`llfloaterdirector.cpp:809-905`).
22. aluiperfselftest: `mScrollBegin`/`End`, `collectTabs`/`collectScrollers`, `applyChain`, `TAB_SELECT`, `mFindCalls`, the `mAuditSizeIndex < 2` loop; the audit runs after the TAB phases.
23. `ui_selftest.ps1` has `-Layout`, `-Compare` and `-Floaters`.
24. Map maths:
    - `right = (−sin φc, cos φc)`, so `d·right = sin t` (positive t is screen-right);
    - Top drag `atan2(dx, −dy)` inverts Top;
    - Side `s`, depth and `fr` sign conventions are consistent;
    - the hysteresis table (1141/1145/1150) is consistent with BP + 8.
25. Shell widths: 1160 gives main 760 and 2 columns (350 ≥ 340); 850 gives main 794 and a 150 px track (≥ 120).
26. `llprismlens.h:30-31` 8/16 limits; `RegistrySnapshot` (`:498-509`); `CaptureDefinition` has no label; `GateArmedCamera::mLabel`/`isOtsPairArmLabel` (`:261-272`).
27. `pipeline.cpp:1828-1856` lifetime-fixed 1024² RGBA16F output.
28. One `chooseRenderSlot` per frame (`llprismlens.cpp:7221`); visible-only prepare (`:2876-2886`); scheduler (`:3041-3249`); `markProduced` (`:3252`); `setCameraSettings` (`:3515`).
29. `onRenderTargetsReleased` is called from `pipeline.cpp:2489`.
30. DAD enum ends at 21 with `DAD_COUNT` 22.
31. Drag-and-drop dispatch:
    - dictionary at `lltooldraganddrop.cpp:211-236`;
    - UUID cargo is forwarded only for `SOURCE_PEOPLE` (`:521`);
    - the people top-view drop pass passes `drop=false` (`:590`; the spec is right to call it out);
    - `beginDrag` and `dad3d*` signatures match.
32. Director-local drop handlers guard their cargo type before casting (`alpaneldirectoranimswitcher.cpp:597`; `alpaneldanceball.cpp`).
33. `gPrismLensProgram` is a single fragment file (`llviewershadermgr.cpp:1875-1879`); `mShaderFiles` supports multiple files.
34. `LLFloaterPrismManager` 0.25 s draw poll (`SNAPSHOT_POLL_SECONDS`, `draw` at `:715`); `ALVCamBonePov::status`.
35. `floater_director.xml:1342` `vcam_tab`; 33 v1 leaves plus rim, bloom_glow and tron give the 36 data pages; the 32-legacy classification arithmetic is right.
